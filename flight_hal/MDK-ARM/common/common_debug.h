#ifndef COMMON_DEBUG_H
#define COMMON_DEBUG_H

#include "usart.h"
#include "stdio.h"
#include "stdarg.h"
//日志打印需要很多时间，在无人机飞行时可以注释掉
//设计一个日志打印开关
#define DEBUG_PRINTF_ON 1

#if DEBUG_PRINTF_ON
//打印文件名，不打印路径
#define __FILE___NAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
//#define __FILE___NAME (strrchr(__FILE_NAME_, '\') ? strrchr(__FILE__, '\') + 1 : __FILE__)
//使用宏定义，在打印日志之前先打印文件名和行号
#define debug_printf(format,...) printf("[%s:%d]"format,__FILE__,__LINE__,##__VA_ARGS__)
#else

#define debug_printf(format,...)

#endif
#endif // COMMON_DEBUG_H
