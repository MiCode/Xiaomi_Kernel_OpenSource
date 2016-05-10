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

/**=============================================================================
  wlan_hdd_dp_utils.c
  
  \brief      Utility functions for data path module
  
  Description...
               Copyright 2008 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.
  
  ==============================================================================**/
/* $HEADER$ */
  
/**-----------------------------------------------------------------------------
  Include files
  ----------------------------------------------------------------------------*/
#include <wlan_hdd_dp_utils.h>

/**-----------------------------------------------------------------------------
  Preprocessor definitions and constants
 ----------------------------------------------------------------------------*/
  
/**-----------------------------------------------------------------------------
  Type declarations
 ----------------------------------------------------------------------------*/
  
/**-----------------------------------------------------------------------------
  Function declarations and documenation
 ----------------------------------------------------------------------------*/


VOS_STATUS hdd_list_insert_front( hdd_list_t *pList, hdd_list_node_t *pNode )
{
   list_add( pNode, &pList->anchor );
   pList->count++;
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_insert_back( hdd_list_t *pList, hdd_list_node_t *pNode )
{
   list_add_tail( pNode, &pList->anchor );
   pList->count++;
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_insert_back_size( hdd_list_t *pList, hdd_list_node_t *pNode, v_SIZE_t *pSize )
{
   list_add_tail( pNode, &pList->anchor );
   pList->count++;
   *pSize = pList->count;
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_remove_front( hdd_list_t *pList, hdd_list_node_t **ppNode )
{
   struct list_head * listptr;

   if ( list_empty( &pList->anchor ) )
   {
      return VOS_STATUS_E_EMPTY;
   }
         
   listptr = pList->anchor.next;
   *ppNode = listptr;
   list_del(pList->anchor.next);
   pList->count--;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_remove_back( hdd_list_t *pList, hdd_list_node_t **ppNode )
{
   struct list_head * listptr;

   if ( list_empty( &pList->anchor ) )
   {
      return VOS_STATUS_E_EMPTY;
   }

   listptr = pList->anchor.prev;
   *ppNode = listptr;
   list_del(pList->anchor.prev);
   pList->count--;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_remove_node( hdd_list_t *pList,
                                 hdd_list_node_t *pNodeToRemove )
{
   hdd_list_node_t *tmp;
   int found = 0;

   if ( list_empty( &pList->anchor ) )
   {
      return VOS_STATUS_E_EMPTY;
   }

    // verify that pNodeToRemove is indeed part of list pList
   list_for_each(tmp, &pList->anchor) 
   {
     if (tmp == pNodeToRemove)
     {
        found = 1;
        break;
     }
   }
   if (found == 0)
       return VOS_STATUS_E_INVAL;

   list_del(pNodeToRemove); 
   pList->count--;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_peek_front( hdd_list_t *pList,
                                hdd_list_node_t **ppNode )
{
   struct list_head * listptr;
   if ( list_empty( &pList->anchor ) )
   {
      return VOS_STATUS_E_EMPTY;
   }

   listptr = pList->anchor.next;
   *ppNode = listptr;
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_peek_next( hdd_list_t *pList, hdd_list_node_t *pNode,
                               hdd_list_node_t **ppNode )
{
   struct list_head * listptr;
   int found = 0;
   hdd_list_node_t *tmp;
      
   if ( ( pList == NULL) || ( pNode == NULL) || (ppNode == NULL))
   {
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty(&pList->anchor) )
   {
       return VOS_STATUS_E_EMPTY;
   }

   // verify that pNode is indeed part of list pList
   list_for_each(tmp, &pList->anchor) 
   {
     if (tmp == pNode)
     {
        found = 1;
        break;
     }
   }

   if (found == 0)
   {
      return VOS_STATUS_E_INVAL;
   }

   listptr = pNode->next;
   if (listptr == &pList->anchor)
   {
       return VOS_STATUS_E_EMPTY;
   }

   *ppNode =  listptr;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_string_to_hex( char *pSrcMac, int length, char *pDescMac )
{
   int i;
   int k;
   char temp[3] = {0};
   int rv;

   //18 is MAC Address length plus the colons
   if ( !pSrcMac && (length > 18 || length < 18) )
   {
      return VOS_STATUS_E_FAILURE;
   }
   i = k = 0;
   while ( i < length )
   {
       memcpy(temp, pSrcMac+i, 2);
       rv = kstrtou8(temp, 16, &pDescMac[k++]);
       if (rv < 0)
           return VOS_STATUS_E_FAILURE;
       i += 3;
   }

   return VOS_STATUS_SUCCESS;
}

