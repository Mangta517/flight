#ifndef __KEY_H__
#define __KEY_H__

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
typedef enum
{
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_LEFT_TOP,
    KEY_RIGHT_TOP,
    KEY_LEFT_TOP_LONG,
    KEY_RIGHT_TOP_LONG,
} key_type;

key_type key_get(void);

#endif
