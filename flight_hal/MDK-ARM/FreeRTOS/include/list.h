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

/*
 * 这是调度器使用的 list 实现。虽然它主要针对调度器的需求进行了定制，
 * 但也可以供应用程序代码使用。
 *
 * list_ts 只能存储指向 list_item_ts 的指针。每个 ListItem_t 包含一个
 * 数值（xItemValue）。大多数情况下，list 按 item value 升序排序。
 *
 * list 在创建时已经包含一个 list item。该 item 的值是
 * 可存储的最大可能值，因此它始终位于 list 的末尾，并充当标记。
 * list 成员 pxHead 始终指向此标记——即使它位于 list 的尾部。
 * 这是因为尾部包含一个回绕指针，指向 list 的真正头部。
 *
 * 除了其值之外，每个 list item 还包含一个指向 list 中下一个
 * item 的指针（pxNext）、一个指向其所在 list 的指针（pxContainer）
 * 以及一个指向包含它的对象的指针。后两个指针是为了提高 list 操作
 * 的效率而加入的。包含 list item 的对象与 list item 本身之间
 * 实际上存在一个双向链接。
 *
 *
 * \page ListIntroduction List Implementation
 * \ingroup FreeRTOSIntro
 */


#ifndef LIST_H
#define LIST_H

#ifndef INC_FREERTOS_H
    #error "FreeRTOS.h must be included before list.h"
#endif

/*
 * list 结构体成员会在中断中被修改，因此按理应该声明为 volatile。
 * 然而，它们只是在功能上以原子方式被修改（在临界区或调度器挂起的情况下），
 * 并且要么通过引用传递给函数，要么通过 volatile 变量进行索引。
 * 因此，在迄今为止测试过的所有用例中，可以省略 volatile 限定符，
 * 以提供适度的性能提升，而不会对功能行为产生不利影响。
 * 已检查了 IAR、ARM 和 GCC 编译器在各自编译器选项设置为最大优化时
 * 生成的汇编指令，并确认其符合预期。话虽如此，随着编译器技术的发展，
 * 特别是如果使用了激进的跨模块优化（一种尚未经过充分测试的用例），
 * 则可能需要 volatile 限定符来实现正确的优化。预计如果编译器因为
 * list 结构体成员上没有 volatile 限定符，并在激进的跨模块优化下
 * 认为代码不必要而删除了关键代码，将导致调度器完全且明显的失败。
 * 如果遇到这种情况，可以通过在 FreeRTOSConfig.h 中简单地将
 * configLIST_VOLATILE 定义为 volatile（如本注释块底部的示例所示），
 * 在 list 结构体的相关位置插入 volatile 限定符。
 * 如果未定义 configLIST_VOLATILE，则下面的预处理指令
 * 将简单地将 configLIST_VOLATILE 定义为空。
 *
 * 要使用 volatile list 结构体成员，请在 FreeRTOSConfig.h 中添加
 * 以下行（不带引号）：
 * "#define configLIST_VOLATILE volatile"
 */
#ifndef configLIST_VOLATILE
    #define configLIST_VOLATILE
#endif /* configSUPPORT_CROSS_MODULE_OPTIMISATION */

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

/* 可用于在 list 结构体中放置已知值，然后在应用程序执行期间
 * 检查已知值是否未被破坏的宏。这些宏可以捕获 list 数据结构
 * 在内存中被覆盖的情况。它们不会捕获由于错误配置或不正确使用
 * FreeRTOS 所导致的数据错误。*/
#if ( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 0 )
    /* 将宏定义为空操作。 */
    #define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE
    #define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE
    #define listFIRST_LIST_INTEGRITY_CHECK_VALUE
    #define listSECOND_LIST_INTEGRITY_CHECK_VALUE
    #define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
    #define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
    #define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )
    #define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )
    #define listTEST_LIST_ITEM_INTEGRITY( pxItem )
    #define listTEST_LIST_INTEGRITY( pxList )
#else /* if ( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 0 ) */
    /* 定义向 list 结构体中添加新成员的宏。 */
    #define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE     TickType_t xListItemIntegrityValue1;
    #define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE    TickType_t xListItemIntegrityValue2;
    #define listFIRST_LIST_INTEGRITY_CHECK_VALUE          TickType_t xListIntegrityValue1;
    #define listSECOND_LIST_INTEGRITY_CHECK_VALUE         TickType_t xListIntegrityValue2;

    /* 定义将新结构体成员设置为已知值的宏。 */
    #define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )     ( pxItem )->xListItemIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
    #define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )    ( pxItem )->xListItemIntegrityValue2 = pdINTEGRITY_CHECK_VALUE
    #define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )              ( pxList )->xListIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
    #define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )              ( pxList )->xListIntegrityValue2 = pdINTEGRITY_CHECK_VALUE

    /* 定义当某个结构体成员不包含其预期值时将触发断言的宏。 */
    #define listTEST_LIST_ITEM_INTEGRITY( pxItem )                      configASSERT( ( ( pxItem )->xListItemIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxItem )->xListItemIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
    #define listTEST_LIST_INTEGRITY( pxList )                           configASSERT( ( ( pxList )->xListIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxList )->xListIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
#endif /* configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES */


/*
 * list 可包含的唯一对象类型的定义。
 */
struct xLIST;
struct xLIST_ITEM
{
    listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE           /*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
    configLIST_VOLATILE TickType_t xItemValue;          /*< 被列入 list 的值。大多数情况下，此值用于按升序对 list 进行排序。 */
    struct xLIST_ITEM * configLIST_VOLATILE pxNext;     /*< 指向 list 中下一个 ListItem_t 的指针。 */
    struct xLIST_ITEM * configLIST_VOLATILE pxPrevious; /*< 指向 list 中上一个 ListItem_t 的指针。 */
    void * pvOwner;                                     /*< 指向包含该 list item 的对象（通常是 TCB）的指针。因此，包含 list item 的对象与 list item 本身之间存在双向链接。 */
    struct xLIST * configLIST_VOLATILE pxContainer;     /*< 指向此 list item 所在 list 的指针（如果有的话）。 */
    listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE          /*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
};
typedef struct xLIST_ITEM ListItem_t;                   /* 出于某种原因，lint 要求将其作为两个独立的定义。 */

#if ( configUSE_MINI_LIST_ITEM == 1 )
    struct xMINI_LIST_ITEM
    {
        listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE /*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
        configLIST_VOLATILE TickType_t xItemValue;
        struct xLIST_ITEM * configLIST_VOLATILE pxNext;
        struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;
    };
    typedef struct xMINI_LIST_ITEM MiniListItem_t;
#else
    typedef struct xLIST_ITEM      MiniListItem_t;
#endif

/*
 * 调度器使用的队列类型的定义。
 */
typedef struct xLIST
{
    listFIRST_LIST_INTEGRITY_CHECK_VALUE      /*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
    volatile UBaseType_t uxNumberOfItems;
    ListItem_t * configLIST_VOLATILE pxIndex; /*< 用于遍历 list。指向由 listGET_OWNER_OF_NEXT_ENTRY () 调用返回的最后一个 item。 */
    MiniListItem_t xListEnd;                  /*< 包含最大可能 item value 的 list item，这意味着它始终位于 list 的末尾，因此用作标记。 */
    listSECOND_LIST_INTEGRITY_CHECK_VALUE     /*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
} List_t;

/*
 * 用于设置 list item 所有者的访问宏。list item 的所有者
 * 是包含该 list item 的对象（通常是 TCB）。
 *
 * \page listSET_LIST_ITEM_OWNER listSET_LIST_ITEM_OWNER
 * \ingroup LinkedList
 */
#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )    ( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )

/*
 * 用于获取 list item 所有者的访问宏。list item 的所有者
 * 是包含该 list item 的对象（通常是 TCB）。
 *
 * \page listGET_LIST_ITEM_OWNER listSET_LIST_ITEM_OWNER
 * \ingroup LinkedList
 */
#define listGET_LIST_ITEM_OWNER( pxListItem )             ( ( pxListItem )->pvOwner )

/*
 * 用于设置 list item 值的访问宏。大多数情况下，该值
 * 用于按升序对 list 进行排序。
 *
 * \page listSET_LIST_ITEM_VALUE listSET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )     ( ( pxListItem )->xItemValue = ( xValue ) )

/*
 * 用于获取 list item 值的访问宏。该值可以表示
 * 任意内容——例如任务的优先级，或者任务应该被解除阻塞的时间。
 *
 * \page listGET_LIST_ITEM_VALUE listGET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listGET_LIST_ITEM_VALUE( pxListItem )             ( ( pxListItem )->xItemValue )

/*
 * 用于获取给定 list 头部 list item 值的访问宏。
 *
 * \page listGET_LIST_ITEM_VALUE listGET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )        ( ( ( pxList )->xListEnd ).pxNext->xItemValue )

/*
 * 返回 list 头部的 list item。
 *
 * \page listGET_HEAD_ENTRY listGET_HEAD_ENTRY
 * \ingroup LinkedList
 */
#define listGET_HEAD_ENTRY( pxList )                      ( ( ( pxList )->xListEnd ).pxNext )

/*
 * 返回下一个 list item。
 *
 * \page listGET_NEXT listGET_NEXT
 * \ingroup LinkedList
 */
#define listGET_NEXT( pxListItem )                        ( ( pxListItem )->pxNext )

/*
 * 返回标记 list 末尾的 list item。
 *
 * \page listGET_END_MARKER listGET_END_MARKER
 * \ingroup LinkedList
 */
#define listGET_END_MARKER( pxList )                      ( ( ListItem_t const * ) ( &( ( pxList )->xListEnd ) ) )

/*
 * 用于判断 list 是否包含任何 item 的访问宏。
 * 仅当 list 为空时，该宏的值才为 true。
 *
 * \page listLIST_IS_EMPTY listLIST_IS_EMPTY
 * \ingroup LinkedList
 */
#define listLIST_IS_EMPTY( pxList )                       ( ( ( pxList )->uxNumberOfItems == ( UBaseType_t ) 0 ) ? pdTRUE : pdFALSE )

/*
 * 用于返回 list 中 item 数量的访问宏。
 */
#define listCURRENT_LIST_LENGTH( pxList )                 ( ( pxList )->uxNumberOfItems )

/*
 * 用于获取 list 中下一个条目的所有者的访问函数。
 *
 * list 成员 pxIndex 用于遍历 list。调用
 * listGET_OWNER_OF_NEXT_ENTRY 会将 pxIndex 递增到 list 中的下一个 item，
 * 并返回该条目的 pxOwner 参数。通过多次调用此函数，
 * 可以遍历 list 中包含的每一个 item。
 *
 * list item 的 pxOwner 参数是指向拥有该 list item 的对象的指针。
 * 在调度器中，这通常是一个任务控制块（TCB）。
 * pxOwner 参数实际上在 list item 与其所有者之间创建了双向链接。
 *
 * @param pxTCB pxTCB 被设置为下一个 list item 所有者的地址。
 * @param pxList 要从中返回下一个 item 所有者的 list。
 *
 * \page listGET_OWNER_OF_NEXT_ENTRY listGET_OWNER_OF_NEXT_ENTRY
 * \ingroup LinkedList
 */
#define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )                                           \
    {                                                                                          \
        List_t * const pxConstList = ( pxList );                                               \
        /* 将索引递增到下一个 item 并返回该 item，确保 */               \
        /* 不返回 list 末尾使用的标记。  */                         \
        ( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;                           \
        if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) ) \
        {                                                                                      \
            ( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;                       \
        }                                                                                      \
        ( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;                                         \
    }

/*
 * uxListRemove() 的版本，不返回值。作为 xTaskIncrementTick()
 * 的一种轻微优化，以内联方式提供。
 *
 * 从 list 中移除一个 item。list item 有一个指向其所在 list 的指针，
 * 因此只需要将 list item 传递给函数即可。
 *
 * @param uxListRemove 要移除的 item。该 item 将根据其 pxContainer 参数
 * 指向的 list 从该 list 中移除自身。
 *
 * @return 移除 list item 后 list 中剩余的 item 数量。
 *
 * \page listREMOVE_ITEM listREMOVE_ITEM
 * \ingroup LinkedList
 */
#define listREMOVE_ITEM( pxItemToRemove ) \
    {                                     \
        /* list item 知道自己所在的 list。从 list item 获取该 list。 */ \
        List_t * const pxList = ( pxItemToRemove )->pxContainer;                 \
                                                                                 \
        ( pxItemToRemove )->pxNext->pxPrevious = ( pxItemToRemove )->pxPrevious; \
        ( pxItemToRemove )->pxPrevious->pxNext = ( pxItemToRemove )->pxNext;     \
        /* 确保索引保持指向一个有效的 item。 */              \
        if( pxList->pxIndex == ( pxItemToRemove ) )                              \
        {                                                                        \
            pxList->pxIndex = ( pxItemToRemove )->pxPrevious;                    \
        }                                                                        \
                                                                                 \
        ( pxItemToRemove )->pxContainer = NULL;                                  \
        ( pxList->uxNumberOfItems )--;                                           \
    }

/*
 * vListInsertEnd() 的内联版本，为 xTaskIncrementTick() 提供轻微优化。
 *
 * 将一个 list item 插入到 list 中。该 item 将被插入到这样一个位置：
 * 使得它成为通过多次调用 listGET_OWNER_OF_NEXT_ENTRY 返回的 list 中的
 * 最后一个 item。
 *
 * list 成员 pxIndex 用于遍历 list。调用
 * listGET_OWNER_OF_NEXT_ENTRY 会将 pxIndex 递增到 list 中的下一个 item。
 * 使用 vListInsertEnd 将一个 item 放入 list 中，实际上是将该 item
 * 放置在 pxIndex 所指向的 list 位置。这意味着 list 中的每一个其他
 * item 都会在 pxIndex 参数再次指向正在插入的 item 之前，
 * 由 listGET_OWNER_OF_NEXT_ENTRY 返回。
 *
 * @param pxList 要将 item 插入到的 list。
 *
 * @param pxNewListItem 要插入到 list 中的 list item。
 *
 * \page listINSERT_END listINSERT_END
 * \ingroup LinkedList
 */
#define listINSERT_END( pxList, pxNewListItem )           \
    {                                                     \
        ListItem_t * const pxIndex = ( pxList )->pxIndex; \
                                                          \
        /* 仅在同时定义了 configASSERT() 时有效，这些测试可以捕获 \
         * list 数据结构在内存中被覆盖的情况。它们不会捕获 \
         * 由于错误配置或不正确使用 FreeRTOS 所导致的数据错误。 */ \
        listTEST_LIST_INTEGRITY( ( pxList ) );                                  \
        listTEST_LIST_ITEM_INTEGRITY( ( pxNewListItem ) );                      \
                                                                                \
        /* 将一个新的 list item 插入到 ( pxList ) 中，但不是对 list 进行排序， \
         * 而是使新的 list item 成为通过调用 \
         * listGET_OWNER_OF_NEXT_ENTRY() 最后一个被移除的 item。 */                 \
        ( pxNewListItem )->pxNext = pxIndex;                 \
        ( pxNewListItem )->pxPrevious = pxIndex->pxPrevious; \
                                                             \
        pxIndex->pxPrevious->pxNext = ( pxNewListItem );     \
        pxIndex->pxPrevious = ( pxNewListItem );             \
                                                             \
        /* 记住该 item 所在的 list。 */            \
        ( pxNewListItem )->pxContainer = ( pxList );         \
                                                             \
        ( ( pxList )->uxNumberOfItems )++;                   \
    }

/*
 * 用于获取 list 中第一个条目所有者的访问函数。
 * list 通常按 item value 升序排序。
 *
 * 此函数返回 list 中第一个 item 的 pxOwner 成员。
 * list item 的 pxOwner 参数是指向拥有该 list item 的对象的指针。
 * 在调度器中，这通常是一个任务控制块（TCB）。
 * pxOwner 参数实际上在 list item 与其所有者之间创建了双向链接。
 *
 * @param pxList 要从中返回头部 item 所有者的 list。
 *
 * \page listGET_OWNER_OF_HEAD_ENTRY listGET_OWNER_OF_HEAD_ENTRY
 * \ingroup LinkedList
 */
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )            ( ( &( ( pxList )->xListEnd ) )->pxNext->pvOwner )

/*
 * 检查一个 list item 是否在某个 list 中。list item 维护一个
 * "container" 指针，指向其所在的 list。这个宏所做的就是
 * 检查 container 和 list 是否匹配。
 *
 * @param pxList 我们想知道 list item 是否在其中的 list。
 * @param pxListItem 我们想知道是否在 list 中的 list item。
 * @return 如果 list item 在 list 中则返回 pdTRUE，否则返回 pdFALSE。
 */
#define listIS_CONTAINED_WITHIN( pxList, pxListItem )    ( ( ( pxListItem )->pxContainer == ( pxList ) ) ? ( pdTRUE ) : ( pdFALSE ) )

/*
 * 返回 list item 所包含在（被引用自）的 list。
 *
 * @param pxListItem 被查询的 list item。
 * @return 指向引用 pxListItem 的 List_t 对象的指针。
 */
#define listLIST_ITEM_CONTAINER( pxListItem )            ( ( pxListItem )->pxContainer )

/*
 * 这提供了一种粗略的方法来判断一个 list 是否已经被初始化，
 * 因为 vListInitialise() 函数会将 pxList->xListEnd.xItemValue
 * 设置为 portMAX_DELAY。
 */
#define listLIST_IS_INITIALISED( pxList )                ( ( pxList )->xListEnd.xItemValue == portMAX_DELAY )

/*
 * 在使用 list 之前必须调用！此函数初始化 list 结构体的所有成员，
 * 并将 xListEnd item 作为标记插入到 list 的末尾。
 *
 * @param pxList 指向正在被初始化的 list 的指针。
 *
 * \page vListInitialise vListInitialise
 * \ingroup LinkedList
 */
void vListInitialise( List_t * const pxList ) PRIVILEGED_FUNCTION;

/*
 * 在使用 list item 之前必须调用。此函数将 list container 设置为 null，
 * 以便该 item 不会认为它已经包含在某个 list 中。
 *
 * @param pxItem 指向正在被初始化的 list item 的指针。
 *
 * \page vListInitialiseItem vListInitialiseItem
 * \ingroup LinkedList
 */
void vListInitialiseItem( ListItem_t * const pxItem ) PRIVILEGED_FUNCTION;

/*
 * 将一个 list item 插入到 list 中。该 item 将被插入到 list 中
 * 由其 item value 确定的位置（按 item value 升序排列）。
 *
 * @param pxList 要将 item 插入到的 list。
 *
 * @param pxNewListItem 要放入 list 中的 item。
 *
 * \page vListInsert vListInsert
 * \ingroup LinkedList
 */
void vListInsert( List_t * const pxList,
                  ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * 将一个 list item 插入到 list 中。该 item 将被插入到这样一个位置：
 * 使得它成为通过多次调用 listGET_OWNER_OF_NEXT_ENTRY 返回的 list 中的
 * 最后一个 item。
 *
 * list 成员 pxIndex 用于遍历 list。调用
 * listGET_OWNER_OF_NEXT_ENTRY 会将 pxIndex 递增到 list 中的下一个 item。
 * 使用 vListInsertEnd 将一个 item 放入 list 中，实际上是将该 item
 * 放置在 pxIndex 所指向的 list 位置。这意味着 list 中的每一个其他
 * item 都会在 pxIndex 参数再次指向正在插入的 item 之前，
 * 由 listGET_OWNER_OF_NEXT_ENTRY 返回。
 *
 * @param pxList 要将 item 插入到的 list。
 *
 * @param pxNewListItem 要插入到 list 中的 list item。
 *
 * \page vListInsertEnd vListInsertEnd
 * \ingroup LinkedList
 */
void vListInsertEnd( List_t * const pxList,
                     ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * 从 list 中移除一个 item。list item 有一个指向其所在 list 的指针，
 * 因此只需要将 list item 传递给函数即可。
 *
 * @param uxListRemove 要移除的 item。该 item 将根据其 pxContainer 参数
 * 指向的 list 从该 list 中移除自身。
 *
 * @return 移除 list item 后 list 中剩余的 item 数量。
 *
 * \page uxListRemove uxListRemove
 * \ingroup LinkedList
 */
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove ) PRIVILEGED_FUNCTION;

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* ifndef LIST_H */
