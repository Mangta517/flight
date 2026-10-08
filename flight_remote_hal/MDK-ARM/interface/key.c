#include "key.h"

key_type key_get(void)
{
    if (HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin) == GPIO_PIN_RESET)
    {
        //电弧抖动延时消抖
        vTaskDelay(5);
        //再次确认按键状态
        if (HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin) == GPIO_PIN_RESET)
        {
            //按键按下时间很长，为了不多次判断，等待按键释放再返回
            while (HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin) == GPIO_PIN_RESET)
            {
                vTaskDelay(1);
            }
            return KEY_UP;
        }
    }
    else if (HAL_GPIO_ReadPin(KEY_DOWN_GPIO_Port, KEY_DOWN_Pin) == GPIO_PIN_RESET)
    {
        vTaskDelay(5);
        if (HAL_GPIO_ReadPin(KEY_DOWN_GPIO_Port, KEY_DOWN_Pin) == GPIO_PIN_RESET)
        {
            while (HAL_GPIO_ReadPin(KEY_DOWN_GPIO_Port, KEY_DOWN_Pin) == GPIO_PIN_RESET)
            {
                vTaskDelay(1);
            }
            return KEY_DOWN;
        }
    }
    else if (HAL_GPIO_ReadPin(KEY_LEFT_GPIO_Port, KEY_LEFT_Pin) == GPIO_PIN_RESET)
    {
        vTaskDelay(5);
        if (HAL_GPIO_ReadPin(KEY_LEFT_GPIO_Port, KEY_LEFT_Pin) == GPIO_PIN_RESET)
        {
            while (HAL_GPIO_ReadPin(KEY_LEFT_GPIO_Port, KEY_LEFT_Pin) == GPIO_PIN_RESET)
            {
                vTaskDelay(1);
            }
            return KEY_LEFT;
        }
    }
    else if (HAL_GPIO_ReadPin(KEY_RIGHT_GPIO_Port, KEY_RIGHT_Pin) == GPIO_PIN_RESET)
    {
        vTaskDelay(5);
        if (HAL_GPIO_ReadPin(KEY_RIGHT_GPIO_Port, KEY_RIGHT_Pin) == GPIO_PIN_RESET)
        {
            while (HAL_GPIO_ReadPin(KEY_RIGHT_GPIO_Port, KEY_RIGHT_Pin) == GPIO_PIN_RESET)
            {
                vTaskDelay(1);
            }
            return KEY_RIGHT;
        }
    }
    else if (HAL_GPIO_ReadPin(KEY_LEFT_TOP_GPIO_Port, KEY_LEFT_TOP_Pin) == GPIO_PIN_RESET)
    {
        vTaskDelay(5);
        if (HAL_GPIO_ReadPin(KEY_LEFT_TOP_GPIO_Port, KEY_LEFT_TOP_Pin) == GPIO_PIN_RESET)
        {
            while (HAL_GPIO_ReadPin(KEY_LEFT_TOP_GPIO_Port, KEY_LEFT_TOP_Pin) == GPIO_PIN_RESET)
            {
                vTaskDelay(1);
            }
            return KEY_LEFT_TOP;
        }
    }
    else if (HAL_GPIO_ReadPin(KEY_RIGHT_TOP_GPIO_Port, KEY_RIGHT_TOP_Pin) == GPIO_PIN_RESET)
    {
        //此时开始计时，长按为超过一秒
        TickType_t start_time = xTaskGetTickCount();
        vTaskDelay(5);
        if (HAL_GPIO_ReadPin(KEY_RIGHT_TOP_GPIO_Port, KEY_RIGHT_TOP_Pin) == GPIO_PIN_RESET)
        {
            while (HAL_GPIO_ReadPin(KEY_RIGHT_TOP_GPIO_Port, KEY_RIGHT_TOP_Pin) == GPIO_PIN_RESET)
            {
                vTaskDelay(1);
                //释放CPU防止卡死
            }
            //检查是否为长按
            TickType_t end_time = xTaskGetTickCount();
            if ((end_time - start_time) >= pdMS_TO_TICKS(1000))
            {
                return KEY_RIGHT_TOP_LONG;
            }

            return KEY_RIGHT_TOP;
        }
    }
    return KEY_NONE;
}
