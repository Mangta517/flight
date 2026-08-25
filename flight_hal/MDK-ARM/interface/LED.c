#include "LED.h"

void led_on(led_struct *led)
{
    HAL_GPIO_WritePin(led->GPIOx, led->GPIO_Pin,GPIO_PIN_SET);
}

void led_off(led_struct *led)
{
    HAL_GPIO_WritePin(led->GPIOx, led->GPIO_Pin,GPIO_PIN_RESET);
}

void led_toggle(led_struct *led)
{
    HAL_GPIO_TogglePin(led->GPIOx, led->GPIO_Pin);
}
