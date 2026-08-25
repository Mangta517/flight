#include "application_flight.h"

Gyro_struct last_gyro={0};
Euler_struct euler_angle = {0};
Accel_Gyro_struct accel_gyro_data = {0};

float yaw_sum;
extern KalmanFilter_Struct kfs[3];
extern volatile remote_data remote_data_receive;
extern Flight_State flight_state;
extern int32_t fix_height_pressure;
extern int32_t spl006_pressure;
extern int32_t spl006_temperature;
extern int32_t spl006_compensated_pressure;
#define APP_FLIGHT_MOTOR_MIN_SPEED 0
/* 放开限速: 0~1000 = 0~100% 占空比(TIM2周期1000)。原为400(40%)。
 * 注意: 动力会大幅增强，务必先卸桨测试! */
#define APP_FLIGHT_MOTOR_MAX_SPEED 1000

/* ============================================================
 * 前(TOP)电机转速补偿(手动微调)
 * 作用: 给两个前电机直接叠加固定转速，抵消"油门增大时机头前冲"
 * 正值 → 前(TOP)电机额外加速(机头抬升)  负值 → 前电机额外减速
 * 单位: 补偿量为绝对速度偏移(CCR)，与满量程解耦——放开限速后
 *       此偏移数值不变、物理补偿效果不变。
 *       按新满量程(0~1000)折算: 1% ≈ 10，当前 8 ≈ 0.8%。
 * 调整: 卸桨后解锁、小油门悬停观察，按需增减。NORMAL/FIX_HEIGHT 均生效。
 * 注意: 若前冲根因是校准面不水平/摇杆中位偏移/IMU装向错误，此参数只能掩盖症状。
 * 实验(2026): 排查"P=0仍前漂"时全部清零做对照，确认是否与此微调有关。
 * ============================================================ */
#define MOTOR_TOP_TRIM  0   // 前电机补偿 0(CCR)，清零对照

/* ============================================================
 * 左前(LEFT_TOP)电机额外转速补偿
 * 作用: 在 MOTOR_TOP_TRIM 基础上，单独给左前电机再叠加固定转速，
 *       用于补偿单侧动力/重心偏差(如机头往右偏时加大左前)。
 * 实验(2026): 与 TOP_TRIM 一并清零做对照。
 * 调整: 补偿量为绝对偏移，1% ≈ 10(按新满量程1000折算)。
 * ============================================================ */
#define MOTOR_LEFT_TOP_TRIM  0   // 左前电机额外补偿 0(CCR)，清零对照

/* ============================================================
 * 右前(RIGHT_TOP)电机额外转速补偿
 * 作用: 在 MOTOR_TOP_TRIM 基础上，单独给右前电机再叠加固定转速，
 *       用于补偿单侧动力/重心偏差(如机头往左偏时加大右前)。
 * 注意(2026): 原值12导致右前总补偿24、比左前(14)多10，左侧升力偏小，
 *       飞机向左漂。先改2与左前对称，现与TOP_TRIM一并清零做对照。
 *       若清零后出现明显漂移，说明原值在补偿真实重心/动力偏差，再按需调回。
 * 调整: 补偿量为绝对偏移，1% ≈ 10(按新满量程1000折算)。
 * ============================================================ */
#define MOTOR_RIGHT_TOP_TRIM  0   // 右前电机额外补偿 0(CCR)，清零对照

/* ============================================================
 * 摇杆死区
 * 作用: 消除摇杆中位微小偏移(如 PIT 中位读数 505~520)导致的持续
 *       俯仰/横滚/偏航指令——这种偏移会让飞机匀速往一个方向飘。
 * 量程 0~1000，死区 15 ≈ 1.5%，不影响正常操控手感。
 * ============================================================ */
#define STICK_DEADZONE  15

/* 摇杆原始值(0~1000, 中位500) → 角度目标值(°)，带死区 */
static float app_stick_to_angle(int16_t raw)
{
    int16_t cmd = raw - 500;
    if (cmd > STICK_DEADZONE)
    {
        cmd -= STICK_DEADZONE;
    }
    else if (cmd < -STICK_DEADZONE)
    {
        cmd += STICK_DEADZONE;
    }
    else
    {
        cmd = 0;
    }
    return cmd / 50.0f;
}

extern TaskHandle_t flight_remote_control_handle;

static int16_t app_flight_limit_motor_speed(float speed)
{
    if (speed > APP_FLIGHT_MOTOR_MAX_SPEED)
    {
        return APP_FLIGHT_MOTOR_MAX_SPEED;
    }
    else if (speed < APP_FLIGHT_MOTOR_MIN_SPEED)
    {
        return APP_FLIGHT_MOTOR_MIN_SPEED;
    }

    return (int16_t)speed;
}

//俯仰角PID
pid_struct pitch_pid={.kp=2.5,.ki=0,.kd=0.3,.integral_limit=5000};
//Y轴角速度，对应俯仰角内环PID
pid_struct gyro_y_pid={.kp=1.5,.ki=0,.kd=0.18,.integral_limit=10000};
//横滚角PID
pid_struct roll_pid={.kp=2.5,.ki=0,.kd=0.3,.integral_limit=5000};
//X轴角速度，对应横滚角内环PID
pid_struct gyro_x_pid={.kp=1.5,.ki=0,.kd=0.18,.integral_limit=10000};
//偏航角PID
pid_struct yaw_pid={.kp=0.5,.ki=0,.kd=0.5,.integral_limit=5000};
//Z轴角速度，对应偏航角内环PID
pid_struct gyro_z_pid={.kp=0.5,.ki=0,.kd=0,.integral_limit=10000};

//定高PID
pid_struct height_pid={.kp=0.3,.ki=0,.kd=0.5,.integral_limit=5000};

void app_flight_init()
{
    //mpu6050初始化
my_mpu6050_init();
//电机初始化
motor_start(&motor_left_top);
motor_start(&motor_right_top);
motor_start(&motor_right_down);
motor_start(&motor_left_down);
}


void app_flight_get_euler_angle()
{
//通过MPU6050获取数据
my_mpu6050_get_data(&accel_gyro_data);



//对角速度进行低通滤波，会对后续的数据实时性有影响
//对准确性要求没那么高，但计算一定要迅速
//output=加权系数*last_output+(1-加权系数)*当前测量值
accel_gyro_data.Gyro.Gyro_x=Common_Filter_LowPass(accel_gyro_data.Gyro.Gyro_x,last_gyro.Gyro_x);
accel_gyro_data.Gyro.Gyro_y=Common_Filter_LowPass(accel_gyro_data.Gyro.Gyro_y,last_gyro.Gyro_y);
accel_gyro_data.Gyro.Gyro_z=Common_Filter_LowPass(accel_gyro_data.Gyro.Gyro_z,last_gyro.Gyro_z);
last_gyro.Gyro_x=accel_gyro_data.Gyro.Gyro_x;
last_gyro.Gyro_y=accel_gyro_data.Gyro.Gyro_y;
last_gyro.Gyro_z=accel_gyro_data.Gyro.Gyro_z;

//卡尔曼滤波
accel_gyro_data.Accel.Accel_x=Common_Filter_KalmanFilter(&kfs[0],accel_gyro_data.Accel.Accel_x);
accel_gyro_data.Accel.Accel_y=Common_Filter_KalmanFilter(&kfs[1],accel_gyro_data.Accel.Accel_y);
accel_gyro_data.Accel.Accel_z=Common_Filter_KalmanFilter(&kfs[2],accel_gyro_data.Accel.Accel_z);


//打印角速度
//debug_printf(":%d,%d,%d\n", accel_gyro_data.Gyro.Gyro_x, accel_gyro_data.Gyro.Gyro_y, accel_gyro_data.Gyro.Gyro_z);
//打印加速度
//对毛刺比较明显的数据进行卡尔曼滤波
//debug_printf(":%d,%d,%d\n",accel_gyro_data.Accel.Accel_x,accel_gyro_data.Accel.Accel_y,accel_gyro_data.Accel.Accel_z);

//使用互补解算来计算欧拉角==》优先使用加速度解算==》俯仰角和横滚角可以使用
//偏航角只能通过角速度积分

/* euler_angle.pitch=atan2(accel_gyro_data.Accel.Accel_x,accel_gyro_data.Accel.Accel_z)/3.14159*180;
euler_angle.roll=atan2(accel_gyro_data.Accel.Accel_y,accel_gyro_data.Accel.Accel_z)/3.14159*180;

//偏航角使用角速度积分
//16位ADC65535转化为角度，量程为+-2000°
yaw_sum+=(accel_gyro_data.Gyro.Gyro_z*2000.0/32767.0)*0.002;
euler_angle.yaw=yaw_sum;
 */
//使用移植的四元数解算
Common_IMU_GetEulerAngle(&accel_gyro_data,&euler_angle,0.002);

//debug_printf(":%.2f,%.2f,%.2f\n",euler_angle.pitch,euler_angle.roll,euler_angle.yaw);
}


void app_flight_pid_process()
{
    // 测量实际控制周期，替代硬编码 PID_Period(0.002)，
    // 消除 I2C/浮点耗时不固定导致的 D/I 项缩放偏差
    static TickType_t s_last_ctrl_tick = 0;
    TickType_t now = xTaskGetTickCount();
    float ctrl_dt = (s_last_ctrl_tick == 0) ? PID_Period
                                           : ((float)(now - s_last_ctrl_tick)) / 1000.0f;
    s_last_ctrl_tick = now;
    if (ctrl_dt < 0.001f) ctrl_dt = 0.001f;
    if (ctrl_dt > 0.05f)  ctrl_dt = 0.05f;

    pitch_pid.dt = ctrl_dt;
    roll_pid.dt  = ctrl_dt;
    yaw_pid.dt   = ctrl_dt;
    gyro_y_pid.dt = ctrl_dt;
    gyro_x_pid.dt = ctrl_dt;
    gyro_z_pid.dt = ctrl_dt;

remote_data remote_snapshot = remote_data_receive;
//俯仰角，需要给目标值和测量值赋值=》如果平稳飞行则外环角度位0，如果遥控则角度等于摇杆的值
//需要把摇杆1000的值进行转换（带死区，消除中位偏移导致的持续漂移）
//叠加 PITCH_TRIM_DEG 俯仰配平角，抵消桨盘面机械前倾造成的持续前漂
pitch_pid.desire=app_stick_to_angle(remote_snapshot.pit) + PITCH_TRIM_DEG;
//外环的测量值为当前的俯仰角
pitch_pid.measure=euler_angle.pitch;
//内环的测量值为当前的俯仰角角速度
gyro_y_pid.measure=(accel_gyro_data.Gyro.Gyro_y*2000.0/32767.0);
//进行串级PID计算
com_pid_calc_chain(&pitch_pid,&gyro_y_pid);

//横滚角的PID计算
//需要把摇杆1000的值进行转换,横滚角目标值（带死区）
//叠加 ROLL_TRIM_DEG 横滚配平角，抵消桨盘面机械侧倾造成的持续横漂
roll_pid.desire=app_stick_to_angle(remote_snapshot.roll) + ROLL_TRIM_DEG;
//外环的测量值为当前的横滚角
roll_pid.measure=euler_angle.roll;
//内环的测量值为当前的横滚角角速度
gyro_x_pid.measure=(accel_gyro_data.Gyro.Gyro_x*2000.0/32767.0);
//进行串级PID计算
com_pid_calc_chain(&roll_pid,&gyro_x_pid);

//偏航角的PID计算
//需要把摇杆1000的值进行转换,偏航角目标值（带死区）
yaw_pid.desire=app_stick_to_angle(remote_snapshot.yaw);
//外环的测量值为当前的偏航角
yaw_pid.measure=euler_angle.yaw;
//内环的测量值为当前的偏航角角速度
gyro_z_pid.measure=(accel_gyro_data.Gyro.Gyro_z*2000.0/32767.0);
//进行串级PID计算
com_pid_calc_chain(&yaw_pid,&gyro_z_pid);
}

void app_flight_pid_reset(void)
{
	com_pid_reset(&pitch_pid);
	com_pid_reset(&gyro_y_pid);
	com_pid_reset(&roll_pid);
	com_pid_reset(&gyro_x_pid);
	com_pid_reset(&yaw_pid);
	com_pid_reset(&gyro_z_pid);
	com_pid_reset(&height_pid);
}

void app_flight_motor_process()
{
    remote_data remote_snapshot = remote_data_receive;
    if (remote_snapshot.shutdown == 1)
    {
        motor_left_down.speed = 0;
        motor_right_down.speed = 0;
        motor_left_top.speed = 0;
        motor_right_top.speed = 0;
    }
    else
    {
    switch (flight_state)
    {
    case IDLE://加锁，让一开始上电电机速度为0
        motor_left_down.speed = 0;
        motor_right_down.speed = 0;
        motor_left_top.speed = 0;
        motor_right_top.speed = 0;
        break;
        case NORMAL:
        {
        int16_t thr = remote_snapshot.thr;
        float pitch_out = gyro_y_pid.output;
        float roll_out = gyro_x_pid.output;
        // 偏航符号修正: 实机桨/电机转向与混控假设相反，导致偏航正反馈(起飞后越转越快)。
        // 取反 yaw_out 使偏航通道恢复负反馈；若改后反而不稳，说明问题在电机通道映射。
        float yaw_out = -gyro_z_pid.output;

        // X型混控（布局见 application_freertos_task.c 注释）：
        // +pitch→前电机(TOP)加速 -pitch→后电机加速
        // +roll →右电机加速      -roll →左电机加速
        // +yaw  →对角配对(左上+右下 / 右上+左下)参与
        // MOTOR_TOP_TRIM / MOTOR_LEFT/RIGHT_TOP_TRIM: 前电机固定补偿(当前清零对照)
        motor_left_top.speed = app_flight_limit_motor_speed(thr+pitch_out-roll_out+yaw_out+MOTOR_TOP_TRIM+MOTOR_LEFT_TOP_TRIM);
        motor_right_top.speed = app_flight_limit_motor_speed(thr+pitch_out+roll_out-yaw_out+MOTOR_TOP_TRIM+MOTOR_RIGHT_TOP_TRIM);
        motor_left_down.speed = app_flight_limit_motor_speed(thr-pitch_out-roll_out-yaw_out);
        motor_right_down.speed = app_flight_limit_motor_speed(thr-pitch_out+roll_out+yaw_out);
        }
        break;
        case FIX_HEIGHT:
        //在定高状态下定高的PID作用
        {
        int16_t thr = remote_snapshot.thr;
        float pitch_out = gyro_y_pid.output;
        float roll_out = gyro_x_pid.output;
        // 偏航符号修正: 与NORMAL模式一致，取反yaw_out保持负反馈
        float yaw_out = -gyro_z_pid.output;

        // 定高混控同样叠加 MOTOR_TOP_TRIM 与左/右前额外补偿
        motor_left_top.speed = app_flight_limit_motor_speed(thr+pitch_out-roll_out+yaw_out+height_pid.output+MOTOR_TOP_TRIM+MOTOR_LEFT_TOP_TRIM);
        motor_right_top.speed = app_flight_limit_motor_speed(thr + pitch_out + roll_out - yaw_out + height_pid.output + MOTOR_TOP_TRIM + MOTOR_RIGHT_TOP_TRIM);
        motor_left_down.speed = app_flight_limit_motor_speed(thr - pitch_out - roll_out - yaw_out + height_pid.output);
        motor_right_down.speed = app_flight_limit_motor_speed(thr - pitch_out + roll_out + yaw_out + height_pid.output);
        }
        break;
        case FAIL:
            // 处理故障=》一直处理直到满足条件进行任务通知修改飞行状态=》IDLE
            // 2ms降低电机转速0.17(与原6ms降0.5的物理减速速率一致，约83/s)，直到电机转速为0
            {
                motor_left_down.speed = app_flight_limit_motor_speed(motor_left_down.speed - 0.17);
                motor_right_down.speed = app_flight_limit_motor_speed(motor_right_down.speed - 0.17);
                motor_left_top.speed = app_flight_limit_motor_speed(motor_left_top.speed - 0.17);
                motor_right_top.speed = app_flight_limit_motor_speed(motor_right_top.speed - 0.17);
                if (motor_left_down.speed == 0 && motor_right_down.speed == 0 && motor_left_top.speed == 0 && motor_right_top.speed == 0)
                {
                    // 通知飞行任务处理完成
                    xTaskNotifyGive(flight_remote_control_handle);
                }
            }
            break;

        default:
            break;
        }
    }
    // 设置安全机制，如果油门小于50=》电机速度为0
    if (remote_snapshot.thr < 50)
    {
        motor_left_down.speed=0;
        motor_right_down.speed=0;
        motor_left_top.speed=0;
        motor_right_top.speed=0;
    }

    //设置电机速度
    timer_motor_speed(&motor_left_down);
    timer_motor_speed(&motor_right_down);
    timer_motor_speed(&motor_left_top);
    timer_motor_speed(&motor_right_top);
}

void app_fixed_height_pid_process()
{
//测量周期约为6ms(每3个控制周期执行一次)，用真实周期修正PID
    static TickType_t s_last_h_tick = 0;
    TickType_t now = xTaskGetTickCount();
    float h_dt = (s_last_h_tick == 0) ? 0.006f : ((float)(now - s_last_h_tick)) / 1000.0f;
    s_last_h_tick = now;
    if (h_dt < 0.005f) h_dt = 0.005f;
    if (h_dt > 0.05f)  h_dt = 0.05f;
    height_pid.dt = h_dt;

//PID的目标值为定高时记录的气压值，测量值为当前的气压值(飞行任务已周期性缓存)
height_pid.desire=fix_height_pressure;
height_pid.measure=spl_process_pressure(spl006_pressure, spl006_temperature, &spl006_compensated_pressure);
//进行PID计算
com_pid_calc(&height_pid);
}



