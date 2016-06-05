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

#if !defined( __VOS_MQ_H )
#define __VOS_MQ_H

/**=========================================================================

  \file  vos_mq.h

  \brief virtual Operating System Services (vOSS) message queue APIs

   Message Queue Definitions and API
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
  
/// vos Message Type.  
/// This represnets a message that can be posted to another module through
/// the voss Message Queues.  
///
/// \note This is mapped directly to the tSirMsgQ for backward
///       compatibility with the legacy MAC code.  

typedef struct vos_msg_s
{
    v_U16_t type;
    /*
     * This field can be used as sequence number/dialog token for matching
     * requests and responses.
     */
    v_U16_t reserved;
    /**
     * Based on the type either a bodyptr pointer into
     * memory or bodyval as a 32 bit data is used.
     * bodyptr: is always a freeable pointer, one should always
     * make sure that bodyptr is always freeable.
     * 
     * Messages should use either bodyptr or bodyval; not both !!!.
     */
    void *bodyptr;

    v_U32_t bodyval;

    /*
     * Some messages provide a callback function.  The function signature
     * must be agreed upon between the two entities exchanging the message
     */
    void *callback;

} vos_msg_t;


/*-------------------------------------------------------------------------
  Function declarations and documenation
  ------------------------------------------------------------------------*/

/// Message Queue IDs
typedef enum
{
  /// Message Queue ID for messages bound for SME
  VOS_MQ_ID_SME = VOS_MODULE_ID_SME,

  /// Message Queue ID for messages bound for PE
  VOS_MQ_ID_PE = VOS_MODULE_ID_PE,

  /// Message Queue ID for messages bound for WDA
  VOS_MQ_ID_WDA = VOS_MODULE_ID_WDA,

  /// Message Queue ID for messages bound for TL
  VOS_MQ_ID_TL = VOS_MODULE_ID_TL,

  /// Message Queue ID for messages bound for the SYS module
  VOS_MQ_ID_SYS = VOS_MODULE_ID_SYS,

  /// Message Queue ID for messages bound for WDI
  VOS_MQ_ID_WDI = VOS_MODULE_ID_WDI,

} VOS_MQ_ID;


/**---------------------------------------------------------------------------
  
  \brief vos_mq_post_message() - post a message to a message queue

  This API allows messages to be posted to a specific message queue.  Messages
  can be posted to the following message queues:
  
  <ul>
    <li> SME
    <li> PE
    <li> HAL
    <li> TL
  </ul> 
  
  \param msgQueueId - identifies the message queue upon which the message
         will be posted.
         
  \param message - a pointer to a message buffer.  Memory for this message 
         buffer is allocated by the caller and free'd by the vOSS after the
         message is posted to the message queue.  If the consumer of the 
         message needs anything in this message, it needs to copy the contents
         before returning from the message queue handler.
  
  \return VOS_STATUS_SUCCESS - the message has been successfully posted
          to the message queue.
          
          VOS_STATUS_E_INVAL - The value specified by msgQueueId does not 
          refer to a valid Message Queue Id.
          
          VOS_STATUS_E_FAULT  - message is an invalid pointer.     
          
          VOS_STATUS_E_FAILURE - the message queue handler has reported
          an unknown failure.

  \sa
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_mq_post_message( VOS_MQ_ID msgQueueId, vos_msg_t *message );

/**--------------------------------------------------------------------------
  \brief vos_mq_post_message_high_pri() - posts a high priority message to
           a message queue

  This API allows messages to be posted to the head of a specific message
  queue. Messages  can be posted to the following message queues:

  <ul>
    <li> SME
    <li> PE
    <li> HAL
    <li> TL
  </ul>

  \param msgQueueId - identifies the message queue upon which the message
         will be posted.

  \param message - a pointer to a message buffer.  Memory for this message
         buffer is allocated by the caller and free'd by the vOSS after the
         message is posted to the message queue.  If the consumer of the
         message needs anything in this message, it needs to copy the contents
         before returning from the message queue handler.

  \return VOS_STATUS_SUCCESS - the message has been successfully posted
          to the message queue.

          VOS_STATUS_E_INVAL - The value specified by msgQueueId does not
          refer to a valid Message Queue Id.

          VOS_STATUS_E_FAULT  - message is an invalid pointer.

          VOS_STATUS_E_FAILURE - the message queue handler has reported
          an unknown failure.
  --------------------------------------------------------------------------*/

VOS_STATUS vos_mq_post_message_high_pri(VOS_MQ_ID msgQueueId, vos_msg_t *message);


/**---------------------------------------------------------------------------
  
  \brief vos_tx_mq_serialize() - serialize a message to the Tx execution flow

  This API allows messages to be posted to a specific message queue in the 
  Tx excution flow.  Messages for the Tx execution flow can be posted only 
  to the following queue.
  
  <ul>
    <li> TL
    <li> WDI/SSC
  </ul>
  
  \param msgQueueId - identifies the message queue upon which the message
         will be posted.
         
  \param message - a pointer to a message buffer.  Body memory for this message 
         buffer is allocated by the caller and free'd by the vOSS after the
         message is dispacthed to the appropriate component.  If the consumer 
         of the message needs to keep anything in the body, it needs to copy 
         the contents before returning from the message handler.
  
  \return VOS_STATUS_SUCCESS - the message has been successfully posted
          to the message queue.
          
          VOS_STATUS_E_INVAL - The value specified by msgQueueId does not 
          refer to a valid Message Queue Id.
          
          VOS_STATUS_E_FAULT  - message is an invalid pointer.     
          
          VOS_STATUS_E_FAILURE - the message queue handler has reported
          an unknown failure.

  \sa
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_tx_mq_serialize( VOS_MQ_ID msgQueueId, vos_msg_t *message );

/**---------------------------------------------------------------------------

  \brief vos_rx_mq_serialize() - serialize a message to the Rx execution flow

  This API allows messages to be posted to a specific message queue in the
  Tx excution flow.  Messages for the Rx execution flow can be posted only
  to the following queue.

  <ul>
    <li> WDI
  </ul>

  \param msgQueueId - identifies the message queue upon which the message
         will be posted.

  \param message - a pointer to a message buffer.  Body memory for this message
         buffer is allocated by the caller and free'd by the vOSS after the
         message is dispacthed to the appropriate component.  If the consumer
         of the message needs to keep anything in the body, it needs to copy
         the contents before returning from the message handler.

  \return VOS_STATUS_SUCCESS - the message has been successfully posted
          to the message queue.

          VOS_STATUS_E_INVAL - The value specified by msgQueueId does not
          refer to a valid Message Queue Id.

          VOS_STATUS_E_FAULT  - message is an invalid pointer.

          VOS_STATUS_E_FAILURE - the message queue handler has reported
          an unknown failure.

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS vos_rx_mq_serialize( VOS_MQ_ID msgQueueId, vos_msg_t *message );


#endif // if !defined __VOS_MQ_H
