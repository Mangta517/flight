/* Link the real application/receiver/PID; mock only their dependencies. */
#include "application_flight.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
TIM_HandleTypeDef htim2;
motor_struct motor_left_top = {&htim2, 2, 0};
motor_struct motor_right_top = {&htim2, 1, 0};
motor_struct motor_right_down = {&htim2, 0, 0};
motor_struct motor_left_down = {&htim2, 3, 0};
Remote_State remote_state = REMOTE_DISCONNECTED;
Flight_State flight_state = IDLE;
volatile remote_data remote_data_receive = {0, 500, 500, 500, 0, 0};
int32_t fix_height_pressure, spl006_pressure, spl006_temperature, spl006_compensated_pressure;
KalmanFilter_Struct kfs[3];
extern pid_struct pitch_pid;
static TickType_t tick = 10;
static uint8_t packet[TX_PLOAD_WIDTH];
static int calibration_calls, starts, imu_calls, attitude_calls, writes, depth, trim_calls;
static uint8_t init_error, read_error, calibration_error, inject_frame;
static uint32_t calibration_delay, tx_delay, read_delay;
static int16_t written[4];
void test_enter_critical(void)
{
    assert(depth == 0);
    depth++;
}
void test_exit_critical(void)
{
    assert(depth == 1);
    depth--;
}
TickType_t xTaskGetTickCount(void)
{
    return tick;
}
uint8_t my_mpu6050_init(void)
{
    return init_error;
}
uint8_t my_mpu6050_get_data(Accel_Gyro_struct *data)
{
    imu_calls++;
    tick += read_delay;
    if (read_error)
    {
        return 1;
    }
    *data = (Accel_Gyro_struct){0};
    data->Accel.Accel_z = 16384;
    return 0;
}
uint8_t my_mpu6050_zero_offset(void)
{
    assert(flight_state == IDLE);
    assert(motor_left_top.speed == 0 && motor_right_top.speed == 0);
    assert(motor_left_down.speed == 0 && motor_right_down.speed == 0);
    calibration_calls++;
    tick += calibration_delay;
    return calibration_error;
}
void my_mpu6050_gyro_trim(int16_t x, int16_t y, int16_t z)
{
    assert(flight_state == IDLE);
    assert(motor_left_top.speed == 0 && motor_right_top.speed == 0);
    assert(motor_left_down.speed == 0 && motor_right_down.speed == 0);
    trim_calls++;
}
void motor_start(motor_struct *motor)
{
    assert(motor->htim == &htim2);
    starts++;
}
void timer_motor_speed(motor_struct *motor)
{
    written[motor->channel] = motor->speed;
    writes++;
}
int16_t Common_Filter_LowPass(int16_t value, int16_t old)
{
    return value;
}
double Common_Filter_KalmanFilter(KalmanFilter_Struct *filter, double value)
{
    return value;
}
static void receive(int16_t throttle);
void Common_IMU_GetEulerAngle(Accel_Gyro_struct *data, Euler_struct *angle, float dt)
{
    assert(dt > 0 && dt <= 0.02f);
    attitude_calls++;
    *angle = (Euler_struct){0};
    if (inject_frame)
    {
        inject_frame = 0;
        receive(900);
    }
}
int spl_process_pressure(int32_t pressure, int32_t temperature, int32_t *result)
{
    *result = pressure;
    return pressure;
}
void spl_update_data(void)
{
}
uint8_t Int_nRF24L01P_RxPacket(uint8_t *data)
{
    for (unsigned i = 0; i < sizeof(packet); i++)
    {
        data[i] = packet[i];
    }
    return 0;
}
void Int_nRF24L01P_TX_Mode(void)
{
}
void Int_nRF24L01P_RX_Mode(void)
{
}
uint8_t Int_nRF24L01P_TxPacket(uint8_t *data)
{
    tick += tx_delay;
    return 1;
}
static void checksum(void)
{
    uint32_t sum = 0;
    for (unsigned i = 0; i < TX_PLOAD_WIDTH - 4; i++)
    {
        sum += packet[i];
    }
    for (unsigned i = 0; i < 4; i++)
    {
        packet[TX_PLOAD_WIDTH - 4 + i] = sum >> (24 - 8 * i);
    }
}
static void frame(int16_t throttle)
{
    packet[0] = 'h';
    packet[1] = 'y';
    packet[2] = 'f';
    int16_t controls[] = {throttle, 500, 500, 500};
    for (unsigned i = 0; i < 4; i++)
    {
        packet[3 + 2 * i] = controls[i] >> 8;
        packet[4 + 2 * i] = controls[i] & 255;
    }
    packet[11] = 0;
    packet[12] = 0;
    checksum();
}
static void receive(int16_t throttle)
{
    frame(throttle);
    assert(application_recevie_data() == 0);
}
static void prepare(void)
{
    assert(app_flight_init() == 0 && starts == 4);
    receive(0);
    app_flight_cycle();
    assert(calibration_calls == 1 && flight_state == IDLE);
}
static void set_motors(int16_t speed)
{
    motor_left_top.speed = speed;
    motor_right_top.speed = speed;
    motor_left_down.speed = speed;
    motor_right_down.speed = speed;
}
static void assert_stopped(void)
{
    for (unsigned i = 0; i < 4; i++)
    {
        assert(written[i] == 0);
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *test = argv[1];
    if (strcmp(test, "init_failure") == 0)
    {
        init_error = 1;
        assert(app_flight_init() == 1);
        receive(1000);
        app_flight_cycle();
        assert(calibration_calls == 0 && imu_calls == 0);
        assert_stopped();
    }
    else if (strcmp(test, "calibration") == 0)
    {
        calibration_delay = 600;
        prepare();
        assert(remote_state == REMOTE_DISCONNECTED);
        assert_stopped();
        receive(500);
        app_flight_cycle();
        flight_state = NORMAL;
        set_motors(500);
        tick += 80;
        app_flight_cycle();
        receive(500);
        app_flight_cycle();
        assert(calibration_calls == 1 && flight_state == FAIL);
    }
    else if (strcmp(test, "calibration_failure") == 0)
    {
        calibration_error = 1;
        prepare();
        for (int i = 0; i < 2500; i++)
        {
            tick++;
            receive(1000);
            app_flight_cycle();
        }
        receive(0);
        app_flight_cycle();
        for (int i = 0; i < 1100; i++)
        {
            tick++;
            receive(0);
            app_flight_cycle();
        }
        assert(flight_state == IDLE);
        assert_stopped();
    }
    else if (strcmp(test, "idle_trim") == 0)
    {
        prepare();
        for (int i = 0; i < 347; i++)
        {
            tick += 2;
            receive(0);
            app_flight_cycle();
        }
        assert(trim_calls == 0);
        flight_state = NORMAL;
        set_motors(500);
        frame(500);
        packet[11] = 1;
        checksum();
        assert(application_recevie_data() == 0);
        app_flight_cycle();
        assert(trim_calls == 0);
        assert_stopped();
        for (int i = 0; i < 348; i++)
        {
            tick += 2;
            receive(0);
            app_flight_cycle();
        }
        assert(trim_calls == 0);
        app_flight_cycle();
        assert(trim_calls == 1);
    }
    else if (strcmp(test, "ramp") == 0 || strcmp(test, "wrap") == 0)
    {
        if (strcmp(test, "wrap") == 0)
        {
            tick = UINT32_MAX - 50;
        }
        prepare();
        receive(500);
        flight_state = NORMAL;
        set_motors(500);
        tick += 80;
        app_flight_cycle();
        assert(flight_state == FAIL && motor_left_top.speed == 500);
        for (int i = 0; i < 500; i++)
        {
            tick += 2;
            receive(0);
            app_flight_cycle();
            assert(flight_state == FAIL);
        }
        assert(motor_left_top.speed == 417 && calibration_calls == 1);
        for (int i = 0; i < 3000 && flight_state == FAIL; i++)
        {
            tick += 2;
            receive(1000);
            app_flight_cycle();
        }
        assert(flight_state == IDLE);
        assert_stopped();
        app_flight_cycle();
        assert(flight_state == IDLE);
        for (int i = 0; i < 1100; i++)
        {
            tick++;
            receive(1000);
            app_flight_cycle();
        }
        receive(0);
        app_flight_cycle();
        tick++;
        receive(0);
        app_flight_cycle();
        for (int i = 0; i < 1100; i++)
        {
            tick++;
            receive(0);
            app_flight_cycle();
        }
        assert(flight_state == NORMAL);
        assert_stopped();
    }
    else if (strcmp(test, "freshness") == 0)
    {
        prepare();
        flight_state = NORMAL;
        set_motors(500);
        tx_delay = 80;
        receive(500);
        app_flight_cycle();
        assert(flight_state == FAIL);
    }
    else if (strcmp(test, "read_failure") == 0)
    {
        prepare();
        flight_state = NORMAL;
        set_motors(500);
        int before = attitude_calls;
        read_error = 1;
        app_flight_cycle();
        assert(attitude_calls == before && flight_state == IDLE);
        assert_stopped();
        read_error = 0;
        receive(1000);
        app_flight_cycle();
        assert(flight_state == IDLE && attitude_calls == before);
    }
    else if (strcmp(test, "read_deadline") == 0)
    {
        prepare();
        flight_state = NORMAL;
        set_motors(500);
        read_delay = 80;
        app_flight_cycle();
        assert(flight_state == FAIL && motor_left_top.speed == 500);
    }
    else if (strcmp(test, "snapshot") == 0)
    {
        prepare();
        receive(500);
        flight_state = NORMAL;
        inject_frame = 1;
        app_flight_cycle();
        assert(pitch_pid.desire == PITCH_TRIM_DEG);
        assert(motor_left_top.speed < 600 && remote_data_receive.thr == 900);
    }
    else if (strcmp(test, "validation") == 0)
    {
        prepare();
        remote_snapshot_struct before, after;
        application_remote_snapshot(&before);
        tick += 5;
        frame(1001);
        assert(application_recevie_data() == 1);
        frame(100);
        packet[0] = 'x';
        checksum();
        assert(application_recevie_data() == 1);
        frame(100);
        packet[TX_PLOAD_WIDTH - 4] = 0x80;
        assert(application_recevie_data() == 1);
        application_remote_snapshot(&after);
        assert(after.last_valid_tick == before.last_valid_tick && after.control.thr == 0);
        frame(100);
        packet[12] = 1;
        checksum();
        assert(application_recevie_data() == 0);
        receive(200);
        application_remote_snapshot(&after);
        assert(after.control.fix_height == 1);
        application_remote_snapshot(&after);
        assert(after.control.fix_height == 0);
    }
    else
    {
        assert(0);
    }
    assert(depth == 0 && writes % 4 == 0);
    printf("PASS flight %s\n", test);
    return 0;
}
