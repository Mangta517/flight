#include "common_pid.h"


void com_pid_calc(pid_struct *pid)
{
    pid->err = pid->desire - pid->measure;
    pid->integral += pid->err;

    // 积分限幅，防止windup
    if (pid->integral_limit > 0) {
        if (pid->integral > pid->integral_limit)
            pid->integral = pid->integral_limit;
        else if (pid->integral < -pid->integral_limit)
            pid->integral = -pid->integral_limit;
    }

    if (pid->last_err == 0) {
        pid->last_err = pid->err;
    }
    float der = (pid->err - pid->last_err);

    // 使用实际控制周期 dt(由调用方测量)，未设置时退回默认 PID_Period
    float dt = pid->dt;
    if (dt <= 0.0f) dt = PID_Period;

    pid->output = pid->kp * pid->err
                + ((pid->ki * pid->integral) * dt)
                + ((pid->kd * der) / dt);

    pid->last_err = pid->err;
}

void com_pid_calc_chain(pid_struct *out_pid, pid_struct *in_pid)
{
//先计算外环
com_pid_calc(out_pid);

//把外环的输出作为内环的目标值
in_pid->desire=out_pid->output;

//计算内环
com_pid_calc(in_pid);
}

void com_pid_reset(pid_struct *pid)
{
    pid->integral = 0;
    pid->last_err = 0;
    pid->output = 0;
    pid->err = 0;
}

int16_t com_limit(int16_t speed, int16_t min_speed, int16_t max_speed)
{
    if (speed > max_speed)
    {

       return max_speed;
    }

    else if (speed < min_speed)

    {

        return min_speed;
    }

    return speed;
}



