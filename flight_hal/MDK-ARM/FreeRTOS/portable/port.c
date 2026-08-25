/*
 * FreeRTOS Kernel V10.5.1
 * 版权所有 (C) 2021 Amazon.com, Inc. 或其关联公司。保留所有权利。
 *
 * SPDX-License-Identifier: MIT
 *
 * 特此免费授予任何获得本软件及相关文档文件（以下简称"软件"）副本的人士
 * 不受限制地使用本软件的权利，包括但不限于使用、复制、修改、合并、出版、
 * 分发、再许可及/或销售本软件副本的权利，并允许获得本软件提供的人士
 * 在符合以下条件的情况下这样做：
 *
 * 上述版权声明和本许可声明应包含在
 * 本软件的所有副本或重要部分中。
 *
 * 本软件按"原样"提供，不作任何明示或暗示的保证，
 * 包括但不限于适销性、特定用途的适用性和非侵权的保证。
 * 在任何情况下，作者或版权持有人均不对任何索赔、损害赔偿或其他责任
 * 承担责任，无论是因合同、侵权或其他原因引起的，
 * 与本软件或本软件的使用或其他交易有关。
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*-----------------------------------------------------------
* ARM CM3 移植层中 portable.h 定义的函数的实现。
*----------------------------------------------------------*/

/* 调度器头文件。 */
#include "FreeRTOS.h"
#include "task.h"

#ifndef configKERNEL_INTERRUPT_PRIORITY
    #define configKERNEL_INTERRUPT_PRIORITY    255
#endif

#if configMAX_SYSCALL_INTERRUPT_PRIORITY == 0
    #error configMAX_SYSCALL_INTERRUPT_PRIORITY 不能设为 0。参见 http: /*www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
#endif

/* 仅用于向后兼容的旧版宏。此宏曾用于替换配置用于生成滴答
 * 中断的时钟的函数（prvSetupTimerInterrupt()），但现在该函数被声明为弱函数，
 * 因此应用程序编写者可以通过简单地定义一个同名函数
 * （vApplicationSetupTickInterrupt()）来覆盖它。 */
#ifndef configOVERRIDE_DEFAULT_TICK_CONFIGURATION
    #define configOVERRIDE_DEFAULT_TICK_CONFIGURATION    0
#endif

/* 操作内核所需的常量。首先是寄存器…… */
#define portNVIC_SYSTICK_CTRL_REG             ( *( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG             ( *( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG    ( *( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SHPR3_REG                    ( *( ( volatile uint32_t * ) 0xe000ed20 ) )
/* ……然后是寄存器中的位。 */
#define portNVIC_SYSTICK_CLK_BIT              ( 1UL << 2UL )
#define portNVIC_SYSTICK_INT_BIT              ( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT           ( 1UL << 0UL )
#define portNVIC_SYSTICK_COUNT_FLAG_BIT       ( 1UL << 16UL )
#define portNVIC_PENDSVCLEAR_BIT              ( 1UL << 27UL )
#define portNVIC_PEND_SYSTICK_SET_BIT         ( 1UL << 26UL )
#define portNVIC_PEND_SYSTICK_CLEAR_BIT       ( 1UL << 25UL )

#define portNVIC_PENDSV_PRI                   ( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )
#define portNVIC_SYSTICK_PRI                  ( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )

/* 检查中断优先级有效性所需的常量。 */
#define portFIRST_USER_INTERRUPT_NUMBER       ( 16 )
#define portNVIC_IP_REGISTERS_OFFSET_16       ( 0xE000E3F0 )
#define portAIRCR_REG                         ( *( ( volatile uint32_t * ) 0xE000ED0C ) )
#define portMAX_8_BIT_VALUE                   ( ( uint8_t ) 0xff )
#define portTOP_BIT_OF_BYTE                   ( ( uint8_t ) 0x80 )
#define portMAX_PRIGROUP_BITS                 ( ( uint8_t ) 7 )
#define portPRIORITY_GROUP_MASK               ( 0x07UL << 8UL )
#define portPRIGROUP_SHIFT                    ( 8UL )

/* 屏蔽 ICSR 寄存器中除 VECTACTIVE 位之外的所有位。 */
#define portVECTACTIVE_MASK                   ( 0xFFUL )

/* 设置初始栈所需的常量。 */
#define portINITIAL_XPSR                      ( 0x01000000 )

/* SysTick 是一个 24 位计数器。 */
#define portMAX_24_BIT_NUMBER                 ( 0xffffffUL )

/* 一个微调因子，用于估算在无滴答空闲计算期间
 * SysTick 计数器停止时本应发生的 SysTick 计数次数。 */
#define portMISSED_COUNTS_FACTOR              ( 94UL )

/* 为了严格遵守 Cortex-M 规范，任务起始地址的
 * bit-0 应清零，因为它会在退出 ISR 时被加载到 PC 中。 */
#define portSTART_ADDRESS_MASK                ( ( StackType_t ) 0xfffffffeUL )

/* 允许用户覆盖默认的 SysTick 时钟频率。如果由用户定义，
 * 此符号必须等于配置寄存器中 CLK 位为 0 时的 SysTick 时钟频率。 */
#ifndef configSYSTICK_CLOCK_HZ
    #define configSYSTICK_CLOCK_HZ             ( configCPU_CLOCK_HZ )
    /* 确保 SysTick 以与内核相同的频率进行时钟驱动。 */
    #define portNVIC_SYSTICK_CLK_BIT_CONFIG    ( portNVIC_SYSTICK_CLK_BIT )
#else
    /* 选择不以与内核相同的频率来驱动 SysTick 时钟的选项。 */
    #define portNVIC_SYSTICK_CLK_BIT_CONFIG    ( 0 )
#endif

/*
 * 设置定时器以生成滴答中断。此文件中的实现是弱函数，
 * 以便应用程序编写者可以更改用于生成滴答中断的定时器。
 */
void vPortSetupTimerInterrupt( void );

/*
 * 异常处理函数。
 */
void xPortPendSVHandler( void );
void xPortSysTickHandler( void );
void vPortSVCHandler( void );

/*
 * 启动第一个任务是一个独立的函数，以便可以单独进行测试。
 */
static void prvStartFirstTask( void );

/*
 * 用于捕获试图从其实现函数返回的任务。
 */
static void prvTaskExitError( void );

/*-----------------------------------------------------------*/

/* 每个任务在临界嵌套变量中维护自己的中断状态。 */
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

/*
 * 构成一个滴答周期的 SysTick 增量次数。
 */
#if ( configUSE_TICKLESS_IDLE == 1 )
    static uint32_t ulTimerCountsForOneTick = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 可被抑制的最大滴答周期数受到
 * SysTick 定时器 24 位分辨率的限制。
 */
#if ( configUSE_TICKLESS_IDLE == 1 )
    static uint32_t xMaximumPossibleSuppressedTicks = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 补偿 SysTick 停止期间经过的 CPU 周期
 * （仅限低功耗功能。
 */
#if ( configUSE_TICKLESS_IDLE == 1 )
    static uint32_t ulStoppedTimerCompensation = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 由 portASSERT_IF_INTERRUPT_PRIORITY_INVALID() 宏使用，以确保
 * FreeRTOS API 函数不会从被分配了高于
 * configMAX_SYSCALL_INTERRUPT_PRIORITY 的优先级的中断中调用。
 */
#if ( configASSERT_DEFINED == 1 )
    static uint8_t ucMaxSysCallPriority = 0;
    static uint32_t ulMaxPRIGROUPValue = 0;
    static const volatile uint8_t * const pcInterruptPriorityRegisters = ( uint8_t * ) portNVIC_IP_REGISTERS_OFFSET_16;
#endif /* configASSERT_DEFINED */

/*-----------------------------------------------------------*/

/*
 * 有关描述请参见头文件。
 */
StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    /* 模拟上下文切换中断将会创建的栈帧。 */
    pxTopOfStack--;                                                      /* 添加偏移量以补偿 MCU 在中断进入/退出时使用栈的方式。 */
    *pxTopOfStack = portINITIAL_XPSR;                                    /* xPSR */
    pxTopOfStack--;
    *pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK; /* PC */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) prvTaskExitError;                    /* LR */

    pxTopOfStack -= 5;                                                   /* R12、R3、R2 和 R1。 */
    *pxTopOfStack = ( StackType_t ) pvParameters;                        /* R0 */
    pxTopOfStack -= 8;                                                   /* R11、R10、R9、R8、R7、R6、R5 和 R4。 */

    return pxTopOfStack;
}
/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
    /* 实现任务的函数绝不能退出或尝试返回其调用者，
     * 因为没有可返回的地方。如果任务想要退出，
     * 应改为调用 vTaskDelete( NULL )。
     *
     * 如果定义了 configASSERT()，则人为地强制触发 assert()，
     * 然后在此处停止，以便应用程序编写者可以捕获错误。 */
    configASSERT( uxCriticalNesting == ~0UL );
    portDISABLE_INTERRUPTS();

    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/

__asm void vPortSVCHandler( void )
{
/* *INDENT-OFF* */
    PRESERVE8

    ldr r3, = pxCurrentTCB   /* 加载 pxCurrentTCB 变量地址。 */
    ldr r1, [ r3 ]           /* 获取 pxCurrentTCB 指向的 TCB 地址。 */
    ldr r0, [ r1 ]           /* TCB 第一个成员 — 任务栈顶指针。 */
    ldmia r0 !, { r4 - r11 } /* 弹出异常未自动保存的寄存器 (r4-r11)。 */
    msr psp, r0              /* 恢复任务栈指针 PSP。 */
    isb
    mov r0, # 0
    msr basepri, r0
    orr r14, # 0xd
    bx r14
/* *INDENT-ON* */
}
/*-----------------------------------------------------------*/

__asm void prvStartFirstTask( void )
{
/* *INDENT-OFF* */
    PRESERVE8

    /* 从 VTOR (向量表偏移寄存器) 读取 MSP 初始值。 */
    ldr r0, =0xE000ED08
    ldr r0, [ r0 ]
    ldr r0, [ r0 ]

    /* 将 MSP 恢复至向量表指定的初始栈顶。 */
    msr msp, r0
    /* 全局使能中断 (IRQ + Fault)。 */
    cpsie i
    cpsie f
    dsb
    isb
    /* 触发 SVC 异常，SVC handler 将启动第一个任务。 */
    svc 0
    nop
    nop
/* *INDENT-ON* */
}
/*-----------------------------------------------------------*/

/*
 * 有关描述请参见头文件。
 */
BaseType_t xPortStartScheduler( void )
{
    #if ( configASSERT_DEFINED == 1 )
    {
        volatile uint32_t ulOriginalPriority;
        volatile uint8_t * const pucFirstUserPriorityRegister = ( uint8_t * ) ( portNVIC_IP_REGISTERS_OFFSET_16 + portFIRST_USER_INTERRUPT_NUMBER );
        volatile uint8_t ucMaxPriorityValue;

        /* 确定最大优先级，从此优先级开始可以安全地调用 ISR 安全的
         * FreeRTOS API 函数。ISR 安全的函数是指以 "FromISR" 结尾的
         * 函数。FreeRTOS 维护独立的线程和 ISR API 函数，
         * 以确保中断进入尽可能快速和简单。
         *
         * 保存即将被覆盖的中断优先级值。 */
        ulOriginalPriority = *pucFirstUserPriorityRegister;

        /* 确定可用的优先级位数。首先写入所有可能的位。 */
        *pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;

        /* 读回值以查看哪些位被保留了。 */
        ucMaxPriorityValue = *pucFirstUserPriorityRegister;

        /* 内核中断优先级应设置为最低优先级。 */
        configASSERT( ucMaxPriorityValue == ( configKERNEL_INTERRUPT_PRIORITY & ucMaxPriorityValue ) );

        /* 对最大系统调用优先级使用相同的掩码。 */
        ucMaxSysCallPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY & ucMaxPriorityValue;

        /* 根据读回的位数计算最大可接受的优先级分组值。 */
        ulMaxPRIGROUPValue = portMAX_PRIGROUP_BITS;

        while( ( ucMaxPriorityValue & portTOP_BIT_OF_BYTE ) == portTOP_BIT_OF_BYTE )
        {
            ulMaxPRIGROUPValue--;
            ucMaxPriorityValue <<= ( uint8_t ) 0x01;
        }

        #ifdef __NVIC_PRIO_BITS
        {
            /* 检查定义优先级位数的 CMSIS 配置是否与
             * 从硬件实际查询到的优先级位数匹配。 */
            configASSERT( ( portMAX_PRIGROUP_BITS - ulMaxPRIGROUPValue ) == __NVIC_PRIO_BITS );
        }
        #endif

        #ifdef configPRIO_BITS
        {
            /* 检查定义优先级位数的 FreeRTOS 配置是否与
             * 从硬件实际查询到的优先级位数匹配。 */
            configASSERT( ( portMAX_PRIGROUP_BITS - ulMaxPRIGROUPValue ) == configPRIO_BITS );
        }
        #endif

        /* 将优先级分组值移回其在 AIRCR 寄存器中的位置。 */
        ulMaxPRIGROUPValue <<= portPRIGROUP_SHIFT;
        ulMaxPRIGROUPValue &= portPRIORITY_GROUP_MASK;

        /* 将被覆盖的中断优先级寄存器恢复为原始值。 */
        *pucFirstUserPriorityRegister = ulOriginalPriority;
    }
    #endif /* configASSERT_DEFINED */

    /* 将 PendSV 和 SysTick 设为最低优先级的中断。 */
    portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;

    portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;

    /* 启动生成滴答 ISR 的定时器。此处中断已被禁用。 */
    vPortSetupTimerInterrupt();

    /* 初始化临界嵌套计数，为第一个任务做好准备。 */
    uxCriticalNesting = 0;

    /* 启动第一个任务。 */
    prvStartFirstTask();

    /* 不应执行到这里！ */
    return 0;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
    /* 在没有可返回内容的移植层中未实现。
     * 人为地强制触发 assert。 */
    configASSERT( uxCriticalNesting == 1000UL );
}
/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;

    /* 这不是进入临界函数的中断安全版本，
     * 因此如果从中断上下文中调用它，则触发 assert()。
     * 只有以 "FromISR" 结尾的 API 函数才能在中断中使用。
     * 只有在临界嵌套计数为 1 时才进行断言，
     * 以防止在 assert 函数也使用临界区时因递归调用而出错。 */
    if( uxCriticalNesting == 1 )
    {
        configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
    }
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting );
    uxCriticalNesting--;

    if( uxCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}
/*-----------------------------------------------------------*/

__asm void xPortPendSVHandler( void )
{
    extern uxCriticalNesting;
    extern pxCurrentTCB;
    extern vTaskSwitchContext;

/* *INDENT-OFF* */
    PRESERVE8

    mrs r0, psp              /* 将当前任务栈指针 PSP 读入 r0。 */
    isb

    ldr r3, =pxCurrentTCB    /* r3 = &pxCurrentTCB (指针的地址)。 */
    ldr r2, [ r3 ]           /* r2 = pxCurrentTCB (当前 TCB 指针)。 */

    /*  等效伪代码:
     *  *(--r0) = R11; *(--r0) = R10; ... *(--r0) = R4;
     *  将 r4-r11 压入当前任务栈, 并更新 r0 为新的栈顶。
     */
    stmdb r0 !, { r4 - r11 } /* 将 r4-r11 压入任务栈。 */
    str r0, [ r2 ]           /* 将新栈顶存入 TCB 的第一个成员 pxTopOfStack。 */

    stmdb sp !, { r3, r14 }  /* 在主栈上保存 r3 和 r14 (LR)。 */
    mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
    msr basepri, r0          /* 设置 BASEPRI, 屏蔽临界区中断。 */
    dsb
    isb
    bl vTaskSwitchContext    /* 调用 C 函数选择下一个运行的任务。 */
    //程序进行到这，pxCurrentTCB 已经被更新为下一个任务的 TCB 了
    mov r0, #0
    msr basepri, r0          /* 清除 BASEPRI, 恢复中断。 */
    ldmia sp !, { r3, r14 }  /* 从主栈恢复 r3 和 r14。 */

    ldr r1, [ r3 ]           /* r1 = &pxCurrentTCB (新任务 TCB 地址)。 */
    ldr r0, [ r1 ]           /* r0 = TCB 第一个成员 = 新任务的栈顶。 */
    ldmia r0 !, { r4 - r11 } /* r0已经指向新任务的栈顶，从新任务栈弹出 r4-r11。 */
    msr psp, r0              /* 将新栈顶写入 PSP。 */
    isb
    bx r14                   /* 异常返回, 硬件自动出栈 r0-r3 等。 */
    nop
/* *INDENT-ON* */
}
/*-----------------------------------------------------------*/

void xPortSysTickHandler( void )
{
    /* SysTick 以最低中断优先级运行，因此当此中断执行时，
     * 所有中断必须处于非屏蔽状态。因此无需保存
     * 然后恢复中断屏蔽值，因为其值已经是已知的——
     * 因此使用稍快的 vPortRaiseBASEPRI() 函数
     * 来代替 portSET_INTERRUPT_MASK_FROM_ISR()。 */
    vPortRaiseBASEPRI();
    {
        /* 递增 RTOS 滴答计数。 */
        if( xTaskIncrementTick() != pdFALSE )
        {
            /* 需要进行上下文切换。上下文切换在
             * PendSV 中断中执行。触发 PendSV 中断。 */
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;/* 调用 pendsv 函数 */
        }
    }

    vPortClearBASEPRIFromISR();
}
/*-----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE == 1 )

    __weak void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
    {
        uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements, ulSysTickDecrementsLeft;
        TickType_t xModifiableIdleTime;

        /* 确保 SysTick 重载值不会使计数器溢出。 */
        if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
        {
            xExpectedIdleTime = xMaximumPossibleSuppressedTicks;
        }

        /* 进入临界区，但不使用 taskENTER_CRITICAL() 方法，
         * 因为那会屏蔽本应退出睡眠模式的中断。 */
        __disable_irq();
        __dsb( portSY_FULL_READ_WRITE );
        __isb( portSY_FULL_READ_WRITE );

        /* 如果有上下文切换正在挂起或有任务正在等待调度器
         * 解除挂起，则放弃低功耗进入。 */
        if( eTaskConfirmSleepModeStatus() == eAbortSleep )
        {
            /* 重新使能中断——参见上面 __disable_irq() 调用上方的注释。 */
            __enable_irq();
        }
        else
        {
            /* 暂时停止 SysTick。SysTick 停止的时间会
             * 尽可能地进行补偿，但使用无滴答模式
             * 将不可避免地导致内核维护的时间相对于
             * 日历时间产生微小的漂移。 */
            portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT );

            /* 使用 SysTick 当前值寄存器来确定在下一次
             * 滴答中断之前剩余的 SysTick 递减次数。如果
             * 当前值寄存器为零，则实际上剩余
             * ulTimerCountsForOneTick 次递减，而不是零，
             * 因为 SysTick 在从 1 递减到 0 时才请求中断。 */
            ulSysTickDecrementsLeft = portNVIC_SYSTICK_CURRENT_VALUE_REG;

            if( ulSysTickDecrementsLeft == 0 )
            {
                ulSysTickDecrementsLeft = ulTimerCountsForOneTick;
            }

            /* 计算等待 xExpectedIdleTime 个滴答周期所需的重载值。
             * 使用 -1 是因为此代码通常在第一个滴答周期的
             * 中途执行。但如果 SysTick IRQ 现在已挂起，
             * 则清除 IRQ，抑制第一个滴答，并修正重载值
             * 以反映第二个滴答周期已经开始。
             * 预期空闲时间始终至少为两个滴答。 */
            ulReloadValue = ulSysTickDecrementsLeft + ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );

            if( ( portNVIC_INT_CTRL_REG & portNVIC_PEND_SYSTICK_SET_BIT ) != 0 )
            {
                portNVIC_INT_CTRL_REG = portNVIC_PEND_SYSTICK_CLEAR_BIT;
                ulReloadValue -= ulTimerCountsForOneTick;
            }

            if( ulReloadValue > ulStoppedTimerCompensation )
            {
                ulReloadValue -= ulStoppedTimerCompensation;
            }

            /* 设置新的重载值。 */
            portNVIC_SYSTICK_LOAD_REG = ulReloadValue;

            /* 清除 SysTick 计数标志并将计数值设回零。 */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

            /* 重新启动 SysTick。 */
            portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

            /* 睡眠直到有事件发生。configPRE_SLEEP_PROCESSING() 可以
             * 将其参数设置为 0 来表示其实现中包含自己的
             * 等待中断或等待事件指令，因此不应再次执行 wfi。
             * 但是，原始的预期空闲时间变量必须保持不变，
             * 因此会复制一份。 */
            xModifiableIdleTime = xExpectedIdleTime;
            configPRE_SLEEP_PROCESSING( xModifiableIdleTime );

            if( xModifiableIdleTime > 0 )
            {
                __dsb( portSY_FULL_READ_WRITE );
                __wfi();
                __isb( portSY_FULL_READ_WRITE );
            }

            configPOST_SLEEP_PROCESSING( xExpectedIdleTime );

            /* 重新使能中断以允许将 MCU 从睡眠模式中唤醒的
             * 中断立即执行。参见上面 __disable_irq() 调用
             * 上方的注释。 */
            __enable_irq();
            __dsb( portSY_FULL_READ_WRITE );
            __isb( portSY_FULL_READ_WRITE );

            /* 再次禁用中断，因为时钟即将停止，
             * 而在时钟停止时执行的中断会增加
             * RTOS 维护的时间与日历时间之间的漂移。 */
            __disable_irq();
            __dsb( portSY_FULL_READ_WRITE );
            __isb( portSY_FULL_READ_WRITE );

            /* 禁用 SysTick 时钟，不读取
             * portNVIC_SYSTICK_CTRL_REG 寄存器，以确保
             * 如果 portNVIC_SYSTICK_COUNT_FLAG_BIT 被设置，则不会被清除。
             * 同样，SysTick 停止的时间会尽可能地进行补偿，
             * 但使用无滴答模式将不可避免地导致内核维护的时间
             * 相对于日历时间产生微小的漂移*/
            portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT );

            /* 判断 SysTick 是否已经计数到零。 */
            if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
            {
                uint32_t ulCalculatedLoadValue;

                /* 滴答中断结束了睡眠（或现在已挂起），并且
                 * 一个新的滴答周期已经开始。将 portNVIC_SYSTICK_LOAD_REG
                 * 重置为新滴答周期中剩余的值。 */
                ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL ) - ( ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG );

                /* 不要允许微小的值，或者由于睡眠后钩子函数
                 * 执行了耗时过长的操作或 SysTick 当前值寄存器
                 * 为零而某种程度上下溢出的值。 */
                if( ( ulCalculatedLoadValue <= ulStoppedTimerCompensation ) || ( ulCalculatedLoadValue > ulTimerCountsForOneTick ) )
                {
                    ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL );
                }

                portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;

                /* 由于挂起的滴答将在此函数退出后立即处理，
                 * 滴答所维护的滴答值将向前步进
                 * 比等待时间少一的值。 */
                ulCompleteTickPeriods = xExpectedIdleTime - 1UL;
            }
            else
            {
                /* 是滴答中断以外的事件结束了睡眠。 */

                /* 使用 SysTick 当前值寄存器来确定在
                 * 预期空闲时间本应结束时剩余的 SysTick 递减次数。 */
                ulSysTickDecrementsLeft = portNVIC_SYSTICK_CURRENT_VALUE_REG;
                #if ( portNVIC_SYSTICK_CLK_BIT_CONFIG != portNVIC_SYSTICK_CLK_BIT )
                {
                    /* 如果 SysTick 不使用内核时钟，则当前值寄存器
                     * 在此处可能仍然为零。在这种情况下，
                     * SysTick 没有从重载寄存器加载，
                     * 在预期空闲时间内剩余的是 ulReloadValue 次递减，
                     * 而不是零。 */
                    if( ulSysTickDecrementsLeft == 0 )
                    {
                        ulSysTickDecrementsLeft = ulReloadValue;
                    }
                }
                #endif /* portNVIC_SYSTICK_CLK_BIT_CONFIG */

                /* 计算睡眠持续了多长时间，四舍五入到完整的滴答周期
                 * （不是已经考虑了部分滴答的 ulReload 值）。 */
                ulCompletedSysTickDecrements = ( xExpectedIdleTime * ulTimerCountsForOneTick ) - ulSysTickDecrementsLeft;

                /* 处理器等待期间经过了多少个完整的滴答周期？ */
                ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulTimerCountsForOneTick;

                /* 重载值设置为单个滴答周期中剩余的比例部分。 */
                portNVIC_SYSTICK_LOAD_REG = ( ( ulCompleteTickPeriods + 1UL ) * ulTimerCountsForOneTick ) - ulCompletedSysTickDecrements;
            }

            /* 重新启动 SysTick，使其再次从 portNVIC_SYSTICK_LOAD_REG 运行，
             * 然后将 portNVIC_SYSTICK_LOAD_REG 设置回其标准值。如果
             * SysTick 不使用内核时钟，则临时将其配置为使用内核时钟。
             * 此配置强制 SysTick 立即从 portNVIC_SYSTICK_LOAD_REG 加载，
             * 而不是等待另一个时钟的下一个周期。
             * 然后 portNVIC_SYSTICK_LOAD_REG 准备好立即接收标准值。 */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
            portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
            #if ( portNVIC_SYSTICK_CLK_BIT_CONFIG == portNVIC_SYSTICK_CLK_BIT )
            {
                portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;
            }
            #else
            {
                /* 如上所述，临时使用内核时钟已完成其目的。
                 * 恢复使用其他时钟。 */
                portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT;

                if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
                {
                    /* 部分滴答周期已经结束。确保 SysTick 只计数一次。 */
                    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0;
                }

                portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;
                portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
            }
            #endif /* portNVIC_SYSTICK_CLK_BIT_CONFIG */

            /* 步进滴答以补偿已经过的时间周期。 */
            vTaskStepTick( ulCompleteTickPeriods );

            /* 在中断已使能的状态下退出。 */
            __enable_irq();
        }
    }

#endif /* #if configUSE_TICKLESS_IDLE */

/*-----------------------------------------------------------*/

/*
 * 设置 SysTick 定时器以所需频率生成滴答中断。
 */
#if ( configOVERRIDE_DEFAULT_TICK_CONFIGURATION == 0 )

    __weak void vPortSetupTimerInterrupt( void )
    {
        /* 计算配置滴答中断所需的常量。 */
        #if ( configUSE_TICKLESS_IDLE == 1 )
        {
            ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );
            xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;
            ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );
        }
        #endif /* configUSE_TICKLESS_IDLE */

        /* 停止并清除 SysTick。 */
        portNVIC_SYSTICK_CTRL_REG = 0UL;
        portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

        /* 配置 SysTick 以请求的速率中断。 */
        portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
        portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );
    }

#endif /* configOVERRIDE_DEFAULT_TICK_CONFIGURATION */
/*-----------------------------------------------------------*/

__asm uint32_t vPortGetIPSR( void )
{
/* *INDENT-OFF* */
    PRESERVE8

    mrs r0, ipsr      /* 读取 IPSR 寄存器 (当前中断编号)。 */
    bx r14            /* 返回, 结果在 r0 中。 */
/* *INDENT-ON* */
}
/*-----------------------------------------------------------*/

#if ( configASSERT_DEFINED == 1 )

    void vPortValidateInterruptPriority( void )
    {
        uint32_t ulCurrentInterrupt;
        uint8_t ucCurrentPriority;

        /* 获取当前正在执行的中断的编号。 */
        ulCurrentInterrupt = vPortGetIPSR();

        /* 中断编号是否为用户定义的中断？ */
        if( ulCurrentInterrupt >= portFIRST_USER_INTERRUPT_NUMBER )
        {
            /* 查找该中断的优先级。 */
            ucCurrentPriority = pcInterruptPriorityRegisters[ ulCurrentInterrupt ];

            /* 如果被分配了高于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的
             * 优先级的中断的服务例程（ISR）调用了 ISR 安全的
             * FreeRTOS API 函数，则以下断言将失败。
             * ISR 安全的 FreeRTOS API 函数只能从被分配了等于
             * 或低于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的优先级的
             * 中断中调用。
             *
             * 数值上较小的中断优先级数字表示逻辑上较高的
             * 中断优先级，因此中断的优先级必须设置为等于
             * 或数值上高于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的值。
             *
             * 使用 FreeRTOS API 的中断不能保持其默认优先级零，
             * 因为零是可能的最高优先级，这保证高于
             * configMAX_SYSCALL_INTERRUPT_PRIORITY，
             * 因此也保证是无效的。
             *
             * FreeRTOS 维护独立的线程和 ISR API 函数以确保
             * 中断进入尽可能快速和简单。
             *
             * 以下链接提供详细信息：
             * https://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
             * https://www.FreeRTOS.org/FAQHelp.html */
            configASSERT( ucCurrentPriority >= ucMaxSysCallPriority );
        }

        /* 优先级分组：中断控制器（NVIC）允许将定义每个中断优先级的位
         * 分割为定义中断的抢占优先级位和定义中断的子优先级的位。
         * 为简单起见，所有位都必须定义为抢占优先级位。
         * 如果不是这种情况（如果某些位代表子优先级），
         * 则以下断言将失败。
         *
         * 如果应用程序仅使用 CMSIS 库进行中断配置，
         * 则可以通过在启动调度器之前调用
         * NVIC_SetPriorityGrouping( 0 ); 在所有 Cortex-M 设备上
         * 实现正确的设置。但请注意，某些供应商特定的外设库
         * 假设非零的优先级分组设置，在这种情况下使用
         * 零值将导致不可预测的行为。 */
        configASSERT( ( portAIRCR_REG & portPRIORITY_GROUP_MASK ) <= ulMaxPRIGROUPValue );
    }

#endif /* configASSERT_DEFINED */
