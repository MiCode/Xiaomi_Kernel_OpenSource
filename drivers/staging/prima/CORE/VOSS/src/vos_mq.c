/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

/**=========================================================================
  
  \file  vos_mq.c
  
  \brief virtual Operating System Services (vOSS) message queue APIs
               
   Message Queue Definitions and API
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_mq.h>
#include "vos_sched.h"
#include <vos_api.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
  
/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
  
/*---------------------------------------------------------------------------
  
  \brief vos_mq_init() - Initialize the vOSS Scheduler  
    
  The \a vos_mq_init() function initializes the Message queue.
      
  \param  pMq - pointer to the message queue
  
  \return VOS_STATUS_SUCCESS - Message queue was successfully initialized and 
          is ready to be used.
          
          VOS_STATUS_E_RESOURCES - Invalid parameter passed to the message
          queue initialize function.
          
  \sa vos_mq_init()
  
---------------------------------------------------------------------------*/
__inline VOS_STATUS vos_mq_init(pVosMqType pMq)
{

  /* Some quick sanity check*/
  if (pMq == NULL) {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: NULL pointer passed",__func__);
     return VOS_STATUS_E_FAILURE;
  }

  /* 
  ** Now initialize the lock
  */
  spin_lock_init(&pMq->mqLock);

  /*
  ** Now initialize the List data structure
  */
  INIT_LIST_HEAD(&pMq->mqList);

  return VOS_STATUS_SUCCESS;
   
} /* vos_mq_init()*/

/*---------------------------------------------------------------------------
  
  \brief vos_mq_deinit() - DeInitialize the vOSS Scheduler  
    
  The \a vos_mq_init() function de-initializes the Message queue.
      
  \param  pMq - pointer to the message queue
  
  \return None
          
  \sa vos_mq_deinit()
  
---------------------------------------------------------------------------*/
__inline void vos_mq_deinit(pVosMqType pMq)
{
  /* 
  ** Some quick sanity check
  */
  if (pMq == NULL) {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: NULL pointer passed",__func__);
     return ;
  }

  /* we don't have to do anything with the embedded list or spinlock */

}/* vos_mq_deinit() */


/*---------------------------------------------------------------------------
  
  \brief vos_mq_put() - Add a message to the message queue 
    
  The \a vos_mq_put() function add a message to the Message queue.
      
  \param  pMq - pointer to the message queue
  
  \param  pMsgWrapper - Msg Wrapper containing the message
  
  \return None
          
  \sa vos_mq_put()
  
---------------------------------------------------------------------------*/
__inline void vos_mq_put(pVosMqType pMq, pVosMsgWrapper pMsgWrapper)
{
  unsigned long flags;

  /* 
  ** Some quick sanity check
  */
  if ((pMq == NULL) || (pMsgWrapper == NULL)) {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: NULL pointer passed",__func__);
     return ;
  }

  spin_lock_irqsave(&pMq->mqLock, flags);

  list_add_tail(&pMsgWrapper->msgNode, &pMq->mqList);

  spin_unlock_irqrestore(&pMq->mqLock, flags);

} /* vos_mq_put() */


/*---------------------------------------------------------------------------
  
  \brief vos_mq_get() - Get a message with its wrapper from a message queue 
    
  The \a vos_mq_get() function retrieve a message with its wrapper from 
      the Message queue.
      
  \param  pMq - pointer to the message queue
  
  \return pointer to the Message Wrapper
          
  \sa vos_mq_get()
  
---------------------------------------------------------------------------*/
__inline pVosMsgWrapper vos_mq_get(pVosMqType pMq)
{
  pVosMsgWrapper pMsgWrapper = NULL;

  /* 
  ** Some quick sanity check
  */
  struct list_head * listptr;
  unsigned long flags;
  
  if (pMq == NULL) {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: NULL pointer passed",__func__);
     return NULL;
  }
 
  spin_lock_irqsave(&pMq->mqLock, flags);

  if( list_empty(&pMq->mqList) )
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
             "%s: VOS Message Queue is empty",__func__);
  }
  else
  {
    listptr = pMq->mqList.next;
    pMsgWrapper = (pVosMsgWrapper)list_entry(listptr, VosMsgWrapper, msgNode);
    list_del(pMq->mqList.next);
  }

  spin_unlock_irqrestore(&pMq->mqLock, flags);

  return pMsgWrapper;

} /* vos_mq_get() */


/*---------------------------------------------------------------------------
  
  \brief vos_is_mq_empty() - Return if the MQ is empty
  
  The \a vos_is_mq_empty() returns true if the queue is empty
      
  \param  pMq - pointer to the message queue
  
  \return pointer to the Message Wrapper
          
  \sa vos_mq_get()
  
---------------------------------------------------------------------------*/
__inline v_BOOL_t vos_is_mq_empty(pVosMqType pMq)
{
  v_BOOL_t  state = VOS_FALSE;
  unsigned long flags;

  /* 
  ** Some quick sanity check
  */
  if (pMq == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: NULL pointer passed",__func__);
     return VOS_STATUS_E_FAILURE;
  }

  spin_lock_irqsave(&pMq->mqLock, flags);
  state = list_empty(&pMq->mqList)?VOS_TRUE:VOS_FALSE;
  spin_unlock_irqrestore(&pMq->mqLock, flags);

  return state;

} /* vos_mq_get() */


