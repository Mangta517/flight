/*
 * FreeRTOS Kernel V10.5.1
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */


#include <stdlib.h>

/* 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 可阻止 task.h 重新定义
 * 所有 API 函数以使用 MPU 包装器。仅当从应用程序文件中
 * 包含 task.h 时才应这样做。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "list.h"

/* 抑制 Lint e9021、e961、e750 警告，作为 MISRA 例外处理：
 * MPU 移植需要在上述头文件中定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE，
 * 但在此文件中不需要，以生成正确的特权/非特权链接和布局。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750 !e9021. */

/*-----------------------------------------------------------
* 公共 LIST API — 详见 list.h 文档
*----------------------------------------------------------*/

void vListInitialise( List_t * const pxList )
{
    /* list 结构体包含一个用作末尾标记的 list item。
     * 初始化 list 时，将该标记作为唯一的 list 条目插入。 */
    pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd ); /*lint !e826 !e740 !e9087 使用 mini list 结构体作为 list 末尾以节省 RAM，经验证有效。 */

    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( &( pxList->xListEnd ) );

    /* 将 list 末尾标记的值设为列表中的最大可能值，
     * 以确保它始终保持在 list 的末尾。 */
    pxList->xListEnd.xItemValue = portMAX_DELAY;

    /* list 末尾的前向和后向指针初始指向自身，
     * 这样当 list 为空时可以据此判断。 */
    pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );     /*lint !e826 !e740 !e9087 使用 mini list 结构体作为 list 末尾以节省 RAM，经验证有效。 */
    pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd ); /*lint !e826 !e740 !e9087 使用 mini list 结构体作为 list 末尾以节省 RAM，经验证有效。 */

    /* 当 xListEnd 是完整的 ListItem_t 时，初始化其剩余字段。 */
    #if ( configUSE_MINI_LIST_ITEM == 0 )
    {
        pxList->xListEnd.pvOwner = NULL;
        pxList->xListEnd.pxContainer = NULL;
        listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( &( pxList->xListEnd ) );
    }
    #endif

    pxList->uxNumberOfItems = ( UBaseType_t ) 0U;

    /* 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设为 1，
     * 则向 list 中写入已知值作为完整性校验。 */
    listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList );
    listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList );
}
/*-----------------------------------------------------------*/

void vListInitialiseItem( ListItem_t * const pxItem )
{
    /* 确保 list item 未被记录为属于某个 list。 */
    pxItem->pxContainer = NULL;

    /* 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设为 1，
     * 则向 list item 写入已知值作为完整性校验。 */
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
    listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
}
/*-----------------------------------------------------------*/

void vListInsertEnd( List_t * const pxList,
                     ListItem_t * const pxNewListItem )
{
    ListItem_t * const pxIndex = pxList->pxIndex;

    /* 仅在同时定义了 configASSERT() 时有效，这些测试可捕获
     * list 数据结构在内存中被覆盖的情况。它们无法捕获
     * 因 FreeRTOS 配置错误或使用不当导致的数据错误。 */
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* 将新的 list item 插入 pxList，但不是进行排序，
     * 而是使新 item 成为通过 listGET_OWNER_OF_NEXT_ENTRY()
     * 最后一个被返回的 item。 */
    pxNewListItem->pxNext = pxIndex;
    pxNewListItem->pxPrevious = pxIndex->pxPrevious;

    /* 仅用于决策覆盖测试。 */
    mtCOVERAGE_TEST_DELAY();

    pxIndex->pxPrevious->pxNext = pxNewListItem;
    pxIndex->pxPrevious = pxNewListItem;

    /* 记录该 item 所在的 list。 */
    pxNewListItem->pxContainer = pxList;

    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

void vListInsert( List_t * const pxList,
                  ListItem_t * const pxNewListItem )
{
    ListItem_t * pxIterator;
    const TickType_t xValueOfInsertion = pxNewListItem->xItemValue;

    /* 仅在同时定义了 configASSERT() 时有效，这些测试可捕获
     * list 数据结构在内存中被覆盖的情况。它们无法捕获
     * 因 FreeRTOS 配置错误或使用不当导致的数据错误。 */
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* 将新的 list item 按 xItemValue 升序插入 list 中的正确位置。
     *
     * 如果 list 中已存在等值的 list item，新 item 应插在其后面。
     * 这确保存储在同一个就绪 list 中、拥有相同 xItemValue 的任务
     * 都能公平地分配到 CPU 时间。但如果 xItemValue 等于末尾标记的值，
     * 下面的遍历循环将无法终止。因此优先检查此条件，
     * 并在必要时对排序算法略作调整。 */
    if( xValueOfInsertion == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        /* *** 注意 ***********************************************************
        *  如果您的应用程序在此处崩溃，常见原因如下。此外，
        *  请参阅 https://www.FreeRTOS.org/FAQHelp.html 获取更多建议，
        *  并确保已定义 configASSERT()！
        *  https://www.FreeRTOS.org/a00110.html#configASSERT
        *
        *   1) 栈溢出 —
        *      参见 https://www.FreeRTOS.org/Stacks-and-stack-overflow-checking.html
        *   2) 中断优先级分配错误，尤其在 Cortex-M 设备上，
        *      数值大的优先级值表示实际中断优先级低，这可能违反直觉。
        *      参见 https://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
        *      以及 https://www.FreeRTOS.org/a00110.html 中
        *      configMAX_SYSCALL_INTERRUPT_PRIORITY 的定义。
        *   3) 在临界区内部或调度器挂起期间调用 API 函数，
        *      或从中断中调用了不以 "FromISR" 结尾的 API 函数。
        *   4) 在队列或信号量初始化之前，或在调度器启动之前
        *      就使用了它们（中断是否在 vTaskStartScheduler() 调用之前
        *      就已经在触发了？）。
        *   5) 如果 FreeRTOS 移植支持中断嵌套，请确保滴答中断的优先级
        *      等于或低于 configMAX_SYSCALL_INTERRUPT_PRIORITY。
        **********************************************************************/

        for( pxIterator = ( ListItem_t * ) &( pxList->xListEnd ); pxIterator->pxNext->xItemValue <= xValueOfInsertion; pxIterator = pxIterator->pxNext ) /*lint !e826 !e740 !e9087 The mini list structure is used as the list end to save RAM.  This is checked and valid. *//*lint !e440 The iterator moves to a different value, not xValueOfInsertion. */
        {
            /* 此处无需操作，仅遍历以找到正确的插入位置。 */
        }
    }

    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = pxNewListItem;

    /* 记录该 item 所在的 list，以便后续快速移除。 */
    pxNewListItem->pxContainer = pxList;

    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove )
{
/* list item 知道自己所在的 list。从 list item 中获取该 list。 */
    List_t * const pxList = pxItemToRemove->pxContainer;

    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

    /* 仅用于决策覆盖测试。 */
    mtCOVERAGE_TEST_DELAY();

    /* 确保索引始终指向一个有效的 item。 */
    if( pxList->pxIndex == pxItemToRemove )
    {
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    pxItemToRemove->pxContainer = NULL;
    ( pxList->uxNumberOfItems )--;

    return pxList->uxNumberOfItems;
}
/*-----------------------------------------------------------*/
