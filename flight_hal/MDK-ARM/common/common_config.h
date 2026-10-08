#ifndef __COMMON_CONFIG_H__
#define __COMMON_CONFIG_H__

#include "main.h"

//表示连接状态
typedef enum
{
    REMOTE_CONNECTED = 0,
    REMOTE_DISCONNECTED,

} Remote_State;

typedef enum
{
    IDLE = 0,
    NORMAL,
    FIX_HEIGHT,
    SHUTDOWN,
    FAIL,
} Flight_State;

typedef enum
{
    FREE = 0,
    MAX,
    LEAVE_MAX,
    MIN,
    UNLOCK,

} Thr_State;

//16位ADC
typedef struct
{
    int16_t Gyro_x; //向左飞为正，向右飞为负，表示横滚角
    int16_t Gyro_y; //向后飞为正，向前飞为负，表示俯仰角
    int16_t Gyro_z; //逆时针为正，顺时针为负，表示偏航角
} Gyro_struct;

typedef struct
{
    int16_t Accel_x; //向前为负
    int16_t Accel_y; //向右为正
    int16_t Accel_z; //朝上飞加速度为正
} Accel_struct;

typedef struct
{
    Gyro_struct Gyro;
    Accel_struct Accel;
} Accel_Gyro_struct;

typedef struct
{
    float yaw;
    float pitch;
    float roll;
} Euler_struct;

#endif
