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

#if !defined( __WLAN_HDD_DP_UTILS_H )
#define __WLAN_HDD_DP_UTILS_H

/**=============================================================================
  wlan_hdd_dp_utils.h

  \brief      Utility functions for data path module

  ==============================================================================**/
/* $HEADER$ */

/**-----------------------------------------------------------------------------
  Include files
  ----------------------------------------------------------------------------*/
#include <linux/list.h>
#include <vos_types.h>
#include <linux/kernel.h>
#include <i_vos_types.h>
#include <vos_status.h>
#include <linux/spinlock.h>
#include <vos_trace.h>
#include <vos_list.h>

/**-----------------------------------------------------------------------------
  Preprocessor definitions and constants
 ----------------------------------------------------------------------------*/

/**-----------------------------------------------------------------------------
  Type declarations
 ----------------------------------------------------------------------------*/

typedef struct list_head hdd_list_node_t;

typedef struct hdd_list_s
{
   hdd_list_node_t anchor;
   v_SIZE_t count;
   v_SIZE_t max_size;
   spinlock_t lock; 
} hdd_list_t;

typedef struct
{
   hdd_list_node_t anchor;
   struct sk_buff *skb;
   int userPriority;
} skb_list_node_t;

//FIXME Need a helper function to cleanup skbs in a queue. Required for cleanup/shutdown
  
/**-----------------------------------------------------------------------------
  Function declarations and documenation
 ----------------------------------------------------------------------------*/
VOS_INLINE_FN v_VOID_t hdd_list_init( hdd_list_t *pList, v_SIZE_t max_size)
{
   INIT_LIST_HEAD( &pList->anchor );
   pList->count = 0;
   pList->max_size = max_size;
   spin_lock_init(&pList->lock);
}

VOS_INLINE_FN v_VOID_t hdd_list_destroy( hdd_list_t *pList )
{
   if ( pList->count !=0 )
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "%s: list length not equal to zero",__func__);
   }
}

VOS_INLINE_FN v_VOID_t hdd_list_size( hdd_list_t *pList, v_SIZE_t *pSize )
{
   *pSize = pList->count;
}

VOS_STATUS hdd_list_insert_front( hdd_list_t *pList, hdd_list_node_t *pNode );

VOS_STATUS hdd_list_insert_back( hdd_list_t *pList, hdd_list_node_t *pNode );

VOS_STATUS hdd_list_insert_back_size( hdd_list_t *pList, hdd_list_node_t *pNode, v_SIZE_t *pSize );

VOS_STATUS hdd_list_remove_front( hdd_list_t *pList, hdd_list_node_t **ppNode );

VOS_STATUS hdd_list_remove_back( hdd_list_t *pList, hdd_list_node_t **ppNode );

VOS_STATUS hdd_list_remove_node( hdd_list_t *pList, hdd_list_node_t *pNodeToRemove );
VOS_STATUS hdd_list_peek_front( hdd_list_t *pList, hdd_list_node_t **ppNode );
VOS_STATUS hdd_list_peek_next( hdd_list_t *pList, hdd_list_node_t *pNode,
                               hdd_list_node_t **ppNode );
VOS_STATUS hdd_string_to_hex( char *pSrcMac, int length, char *pDescMac );
#endif //__WLAN_HDD_DP_UTILS_H
