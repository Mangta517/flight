#include "common_pid.h"

/* 成熟位置式PID(参考Betaflight/ArduPilot实现):
 *  - 积分项按 dt 归一:  integral += err*dt, 输出 ki*integral
 *  - D项对误差微分, 并做一阶低通滤波, 抑制500Hz下微分噪声放大
 *  - 首拍用 init 标志初始化, 避免 last_err=0 时 D 项冲击
 *  - output_limit>0 时对输出限幅, 防止串级内环目标过大
 */
void com_pid_calc(pid_struct *pid)
{
    float dt = pid->dt;
    if (dt <= 0.0f)
        dt = PID_Period; // 兜底周期

    pid->err = pid->desire - pid->measure;

    // 首拍清零积分历史并对齐 last_err，避免首拍微分冲击；本拍仍累加 err * dt
    if (!pid->init)
    {
        pid->last_err = pid->err;
        pid->integral = 0.0f;
        pid->d_lpf = 0.0f;
        pid->init = 1;
    }

    // 积分(按dt归一) + 积分限幅防windup
    pid->integral += pid->err * dt;
    if (pid->integral_limit > 0.0f)
    {
        if (pid->integral > pid->integral_limit)
            pid->integral = pid->integral_limit;
        else if (pid->integral < -pid->integral_limit)
            pid->integral = -pid->integral_limit;
    }

    // 微分项(误差变化率) + 一阶低通滤波
    float der = (pid->err - pid->last_err) / dt;
    if (pid->d_lpf_coef > 0.0f && pid->d_lpf_coef < 1.0f)
    {
        pid->d_lpf += pid->d_lpf_coef * (der - pid->d_lpf); // 一阶IIR
        der = pid->d_lpf;
    }
    else
    {
        pid->d_lpf = der; // 不滤波时直接透传
    }

    // 位置式PID合成
    pid->output = pid->kp * pid->err + pid->ki * pid->integral + pid->kd * der;

    // 输出限幅(保护内环目标/电机指令)
    if (pid->output_limit > 0.0f)
    {
        if (pid->output > pid->output_limit)
            pid->output = pid->output_limit;
        else if (pid->output < -pid->output_limit)
            pid->output = -pid->output_limit;
    }

    pid->last_err = pid->err;
}

void com_pid_calc_chain(pid_struct *out_pid, pid_struct *in_pid)
{
    //先计算外环
    com_pid_calc(out_pid);

    //把外环的输出作为内环的目标值
    in_pid->desire = out_pid->output;

    //计算内环
    com_pid_calc(in_pid);
}

void com_pid_reset(pid_struct *pid)
{
    pid->integral = 0;
    pid->last_err = 0;
    pid->output = 0;
    pid->err = 0;
    pid->d_lpf = 0;
    pid->init = 0; // 复位首拍标志, 下次计算重新初始化
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
