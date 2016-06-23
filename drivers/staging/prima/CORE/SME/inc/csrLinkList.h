/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  
    \file csrLinkList.h
  
    Exports and types for the Common link list interfaces.
 
   ========================================================================== */
#ifndef CSR_LINK_LIST_H__
#define CSR_LINK_LIST_H__

#include "vos_lock.h"


#define LL_ACCESS_LOCK          eANI_BOOLEAN_TRUE
#define LL_ACCESS_NOLOCK        eANI_BOOLEAN_FALSE

typedef struct tagListElem
{
    struct tagListElem *last;
    struct tagListElem *next;
}tListElem;

typedef enum
{
    LIST_FLAG_CLOSE = 0,
    LIST_FLAG_OPEN = 0xa1b2c4d7,
}tListFlag;

//This is a circular double link list
typedef struct tagDblLinkList
{
  tListElem ListHead;
  vos_lock_t Lock;
  tANI_U32  Count;
  tHddHandle hHdd;
  tListFlag Flag;

  /*command debugging */
  tANI_U32  cmdTimeoutDuration;  /* command timeout duration */
  vos_timer_t *cmdTimeoutTimer;  /*command timeout Timer */
}tDblLinkList;

//To get the address of an object of (type) base on the (address) of one of its (field)
#define GET_BASE_ADDR(address, type, field) ((type *)( \
                                                  (tANI_U8 *)(address) - \
                                                  (tANI_U8 *)(&((type *)0)->field)))
                                     
//To get the offset of (field) inside structure (type)                                                  
#define GET_FIELD_OFFSET(type, field)  ((uintptr_t)(&(((type *)0)->field)))

#define GET_ROUND_UP( _Field, _Boundary ) (((_Field) + ((_Boundary) - 1))  & ~((_Boundary) - 1))
#define BITS_ON(  _Field, _Bitmask ) ( (_Field) |=  (_Bitmask) )
#define BITS_OFF( _Field, _Bitmask ) ( (_Field) &= ~(_Bitmask) )

#define CSR_MAX(a, b)  ((a) > (b) ? (a) : (b))
#define CSR_MIN(a, b)  ((a) < (b) ? (a) : (b))

                                                  
#define csrIsListEmpty(pHead) ((pHead)->next == (pHead))

tANI_U32 csrLLCount( tDblLinkList *pList );

eHalStatus csrLLOpen( tHddHandle hHdd, tDblLinkList  *pList );
void csrLLClose( tDblLinkList *pList );

void csrLLLock( tDblLinkList *pList );
void csrLLUnlock( tDblLinkList *pList );

tANI_BOOLEAN csrLLIsListEmpty( tDblLinkList *pList, tANI_BOOLEAN fInterlocked );

void csrLLInsertHead( tDblLinkList *pList, tListElem *pEntry, tANI_BOOLEAN fInterlocked );
void csrLLInsertTail( tDblLinkList *pList, tListElem *pEntry, tANI_BOOLEAN fInterlocked );
//This function put pNewEntry before pEntry. Caller should have found pEntry
void csrLLInsertEntry( tDblLinkList *pList, tListElem *pEntry, tListElem *pNewEntry, tANI_BOOLEAN fInterlocked );

tListElem *csrLLPeekHead( tDblLinkList *pList, tANI_BOOLEAN fInterlocked );
tListElem *csrLLPeekTail( tDblLinkList *pList, tANI_BOOLEAN fInterlocked );

tListElem *csrLLRemoveHead( tDblLinkList *pList, tANI_BOOLEAN fInterlocked );
tListElem *csrLLRemoveTail( tDblLinkList *pList, tANI_BOOLEAN fInterlocked );
tANI_BOOLEAN  csrLLRemoveEntry( tDblLinkList *pList, tListElem *pEntryToRemove, tANI_BOOLEAN fInterlocked );
void csrLLPurge( tDblLinkList *pList, tANI_BOOLEAN fInterlocked );

//csrLLNext return NULL if reaching the end or list is empty
tListElem *csrLLNext( tDblLinkList *pList, tListElem *pEntry, tANI_BOOLEAN fInterlocked );

tListElem *csrLLPrevious( tDblLinkList *pList, tListElem *pEntry, tANI_BOOLEAN fInterlocked );

tANI_BOOLEAN csrLLFindEntry( tDblLinkList *pList, tListElem *pEntryToFind );


#endif

