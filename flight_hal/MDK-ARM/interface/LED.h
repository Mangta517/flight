#ifndef __LED_H__
#define __LED_H__

#include "main.h"


typedef struct 
{
   GPIO_TypeDef *GPIOx;
   uint16_t GPIO_Pin;
   
}led_struct;

void led_on(led_struct *led);
void led_off(led_struct *led);
void led_toggle(led_struct *led);
#endif

