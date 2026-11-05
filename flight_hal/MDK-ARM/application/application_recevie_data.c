#include "application_recevie_data.h"
#include "application_flight.h"

static TickType_t last_valid_tick;
static uint8_t frame_seen;
static Thr_State thr_state = FREE;
static TickType_t start_max_time;
static TickType_t start_min_time;
extern int32_t spl006_pressure;
extern int32_t spl006_temperature;
extern int32_t spl006_compensated_pressure;
extern int32_t fix_height_pressure;
extern Euler_struct euler_angle;

void application_remote_snapshot(remote_snapshot_struct *snapshot)
{
    taskENTER_CRITICAL();
    snapshot->control = remote_data_receive;
    snapshot->last_valid_tick = last_valid_tick;
    snapshot->seen = frame_seen;
    remote_data_receive.fix_height = 0;
    taskEXIT_CRITICAL();
}

uint8_t application_recevie_data(void)
{
    uint8_t rx_buff[TX_PLOAD_WIDTH];
    uint8_t flight[TX_PLOAD_WIDTH] = {0};
    remote_data received;
    uint32_t sum = 0;
    uint32_t sum_receive;
    if (Int_nRF24L01P_RxPacket(rx_buff) != 0)
    {
        return 1;
    }
    if (rx_buff[0] != FRAME_HEADER1 || rx_buff[1] != FRAME_HEADER2 || rx_buff[2] != FRAME_HEADER3)
    {
        return 1;
    }
    for (uint8_t i = 0; i < TX_PLOAD_WIDTH - 4; i++)
    {
        sum += rx_buff[i];
    }
    sum_receive = ((uint32_t)rx_buff[TX_PLOAD_WIDTH - 4] << 24)
        | ((uint32_t)rx_buff[TX_PLOAD_WIDTH - 3] << 16)
        | ((uint32_t)rx_buff[TX_PLOAD_WIDTH - 2] << 8) | rx_buff[TX_PLOAD_WIDTH - 1];
    received.thr = (rx_buff[3] << 8) | rx_buff[4];
    received.yaw = (rx_buff[5] << 8) | rx_buff[6];
    received.pit = (rx_buff[7] << 8) | rx_buff[8];
    received.roll = (rx_buff[9] << 8) | rx_buff[10];
    if (sum != sum_receive || received.thr < 0 || received.thr > 1000
        || received.yaw < 0 || received.yaw > 1000 || received.pit < 0 || received.pit > 1000
        || received.roll < 0 || received.roll > 1000 || rx_buff[11] > 1 || rx_buff[12] > 1)
    {
        return 1;
    }

    /* 先发布及记录有效帧时刻，遥测发送失败不能拖延失联判定。 */
    taskENTER_CRITICAL();
    received.shutdown = remote_data_receive.shutdown;
    if (rx_buff[11] == 1)
    {
        received.shutdown = !received.shutdown;
    }
    received.fix_height = remote_data_receive.fix_height | rx_buff[12];
    remote_data_receive = received;
    last_valid_tick = xTaskGetTickCount();
    frame_seen = 1;
    taskEXIT_CRITICAL();

    if (flight_state == FAIL)
    {
        flight[0] = 1;
    }
    int16_t p = (int16_t)(euler_angle.pitch * 100.0f);
    int16_t r = (int16_t)(euler_angle.roll * 100.0f);
    int16_t y = (int16_t)(euler_angle.yaw * 100.0f);
    flight[1] = (p >> 8) & 0xFF;
    flight[2] = p & 0xFF;
    flight[3] = (r >> 8) & 0xFF;
    flight[4] = r & 0xFF;
    flight[5] = (y >> 8) & 0xFF;
    flight[6] = y & 0xFF;
    Int_nRF24L01P_TX_Mode();
    /* 每轮只尝试一次；硬件自动重发仍由原 SETUP_RETR 配置控制。 */
    Int_nRF24L01P_TxPacket(flight);
    Int_nRF24L01P_RX_Mode();
    return 0;
}

void application_process_connect_state(const remote_snapshot_struct *snapshot)
{
    if (snapshot->seen && (TickType_t)(xTaskGetTickCount() - snapshot->last_valid_tick) < pdMS_TO_TICKS(REMOTE_TIMEOUT_MS))
    {
        remote_state = REMOTE_CONNECTED;
    }
    else
    {
        remote_state = REMOTE_DISCONNECTED;
        thr_state = FREE;
        if (flight_state == NORMAL || flight_state == FIX_HEIGHT)
        {
            flight_state = FAIL;
        }
    }
}

static uint8_t application_process_unlock(const remote_data *control)
{
    TickType_t now = xTaskGetTickCount();
    switch (thr_state)
    {
        case FREE:
            if (control->thr >= 900)
            {
                thr_state = MAX;
                start_max_time = now;
            }
            break;
        case MAX:
            if (control->thr < 900)
            {
                thr_state = FREE;
                if ((TickType_t)(now - start_max_time) >= pdMS_TO_TICKS(1000))
                {
                    thr_state = LEAVE_MAX;
                }
            }
            break;
        case LEAVE_MAX:
            if (control->thr < 100)
            {
                thr_state = MIN;
                start_min_time = now;
            }
            break;
        case MIN:
            if (control->thr >= 100)
            {
                thr_state = FREE;
            }
            else if ((TickType_t)(now - start_min_time) >= pdMS_TO_TICKS(1000))
            {
                thr_state = FREE;
                return 0;
            }
            break;
        default:
            thr_state = FREE;
            break;
    }
    return 1;
}

void application_process_flight_state(const remote_snapshot_struct *snapshot, uint8_t can_arm)
{
    application_process_connect_state(snapshot);
    if (snapshot->control.shutdown)
    {
        flight_state = IDLE;
        thr_state = FREE;
        app_flight_pid_reset();
        return;
    }
    if (flight_state == FAIL || remote_state != REMOTE_CONNECTED)
    {
        thr_state = FREE;
        return;
    }
    if (flight_state == IDLE)
    {
        if (!can_arm)
        {
            thr_state = FREE;
        }
        else if (application_process_unlock(&snapshot->control) == 0)
        {
            app_flight_pid_reset();
            flight_state = NORMAL;
        }
    }
    else if (snapshot->control.fix_height)
    {
        if (flight_state == NORMAL)
        {
            fix_height_pressure = spl_process_pressure(spl006_pressure, spl006_temperature, &spl006_compensated_pressure);
            flight_state = FIX_HEIGHT;
        }
        else if (flight_state == FIX_HEIGHT)
        {
            flight_state = NORMAL;
        }
    }
}
