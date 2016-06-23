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

/**=============================================================================

  vos_list.c

  \brief

  Description...


               Copyright 2008 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.

  ==============================================================================**/
/* $HEADER$ */

/**-----------------------------------------------------------------------------
  Include files
  ----------------------------------------------------------------------------*/
#include <vos_list.h>
#include <vos_trace.h>

/**-----------------------------------------------------------------------------
  Preprocessor definitions and constants
  ----------------------------------------------------------------------------*/
#define VOS_LIST_COOKIE 0xabadfeed


/**-----------------------------------------------------------------------------
  Type declarations
  ----------------------------------------------------------------------------*/

/**-----------------------------------------------------------------------------
  Function declarations and documenation
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_list_init( vos_list_t *pList )
{
   if ( pList == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie == VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: already initialized list", __func__);
      return VOS_STATUS_E_BUSY;
   }

   mutex_init(&pList->lock);

   INIT_LIST_HEAD( &pList->anchor );

   pList->count = 0;
   pList->cookie = VOS_LIST_COOKIE;

   return( VOS_STATUS_SUCCESS );
}


VOS_STATUS vos_list_destroy( vos_list_t *pList )
{
   int rc;
   if (pList == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->count !=0 )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list length not equal to zero", __func__);
      mutex_unlock(&pList->lock);
      return VOS_STATUS_E_BUSY;
   }

   // clear the cookie. This indicates the list is destroyed.
   pList->cookie = 0;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}


VOS_STATUS vos_list_insert_front( vos_list_t *pList, vos_list_node_t *pNode )
{
   int rc;

   if ( ( pList == NULL) || ( pNode == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   list_add( pNode, &pList->anchor );

   pList->count++;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_list_insert_back( vos_list_t *pList, vos_list_node_t *pNode )
{
   int rc;

   if ( ( pList == NULL) || ( pNode == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   list_add_tail( pNode, &pList->anchor );

   pList->count++;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}


VOS_STATUS vos_list_insert_back_size( vos_list_t *pList, vos_list_node_t *pNode, v_SIZE_t *pSize )
{
   int rc;
   if ( ( pList == NULL) || ( pNode == NULL) || (pSize == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   list_add_tail( pNode, &pList->anchor );

   pList->count++;
   *pSize = pList->count;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}


VOS_STATUS vos_list_remove_front( vos_list_t *pList, vos_list_node_t **ppNode )
{
   struct list_head * listptr;
   int rc;

   // the assumption here is that pList is the head of the list (dummy
   // node) and points to first and last element in circular linked list
   if ( ( pList == NULL ) || ( ppNode == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty( &pList->anchor ) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
      return VOS_STATUS_E_EMPTY;
   }

   listptr = pList->anchor.next;

   *ppNode = listptr;

   list_del(pList->anchor.next);

   pList->count--;
   mutex_unlock(&pList->lock);
   return VOS_STATUS_SUCCESS;
}



VOS_STATUS vos_list_remove_back( vos_list_t *pList, vos_list_node_t **ppNode )
{
   struct list_head * listptr;
   int rc;

   // the assumption here is that pList is the head of the list (dummy node) and points to first and
   // last element in circular linked list
   if ( ( pList == NULL ) || ( ppNode == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty( &pList->anchor ) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
      return VOS_STATUS_E_EMPTY;
   }

   listptr = pList->anchor.prev;

   *ppNode = listptr;

   list_del(pList->anchor.prev);

   pList->count--;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_list_size( vos_list_t *pList, v_SIZE_t *pSize )
{
   int rc;
   if ( ( pList ==NULL) || ( pSize == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: list not initialized", __func__);
       return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   *pSize = pList->count;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------

  \brief vos_list_peek_front() - peek at the node at front of a linked list

  The vos_list_peek_front() API will return a pointer to the node at the
  front of a properly initialized vOS List object.  The node will *not* be
  removed from the list.

  \param pList - Pointer to list object of the list to be 'peeked'

  \param ppNode - Pointer to a pointer to the list node that exists at
  the front of the list.

  \return VOS_STATUS_SUCCESS - list node at the front of the list was
          successfully returned.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_EMPTY - The specified is empty so nodes cannot be
          removed.

          VOS_STATUS_E_FAULT  - pList or or ppNode is an invalid pointer.

  \sa vos_list_remove_back()

  --------------------------------------------------------------------------*/

VOS_STATUS vos_list_peek_front( vos_list_t *pList, vos_list_node_t **ppNode )
{
   struct list_head * listptr;
   int rc;

   if ( ( pList == NULL) || ( ppNode == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: list not initialized", __func__);
       return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty(&pList->anchor) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
      return VOS_STATUS_E_EMPTY;
   }
   listptr = pList->anchor.next;
   *ppNode = listptr;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

  \brief vos_list_peek_back() - peek at the node at back of a linked list

  The vos_list_peek_back() API will return a pointer to the node at the
  back of a properly initialized vOS List object.  The node will *not* be
  removed from the list.

  \param pList - Pointer to list object of the list to be 'peeked'

  \param ppNode - Pointer to a pointer to the list node that exists at
  the back of the list.

  \return VOS_STATUS_SUCCESS - list node at the back of the list was
          successfully returned.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_EMPTY - The specified is empty so nodes cannot be
          removed.

          VOS_STATUS_E_FAULT  - pList or or ppNode is an invalid pointer.

  \sa vos_list_peek_back(), vos_list_remove_back(), vos_list_peek_front(),
      vos_list_remove_front()

  --------------------------------------------------------------------------*/

VOS_STATUS vos_list_peek_back( vos_list_t *pList, vos_list_node_t **ppNode )
{
   struct list_head * listptr;
   int rc;

   if ( ( pList == NULL) || ( ppNode == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty(&pList->anchor) )
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                 "%s: list empty", __func__);
       mutex_unlock(&pList->lock);
       return VOS_STATUS_E_EMPTY;
   }
   listptr = pList->anchor.prev;
   *ppNode = listptr;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

  \brief vos_list_peek_next() - peek at the node after the specified node

  The vos_list_peek_next() API will return a pointer to the node following the
  specified node on a properly initialized vOS List object.  The node will
  *not* be removed from the list.

  \param pList - Pointer to list object of the list to be 'peeked'

  \param pNode - Pointer to the node that is being 'peeked'

  \param ppNode - Pointer to a pointer to the list node that follows the
         pNode node on the list.

  \return VOS_STATUS_SUCCESS - list node following pNode on the properly
          initialized list is successfully returned.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_EMPTY - There is no 'next' node (the input node is
          at the back of the list).

          VOS_STATUS_E_FAULT  - pList, pNode or ppNode is an invalid pointer.

  \sa vos_list_remove_back()

  --------------------------------------------------------------------------*/

VOS_STATUS vos_list_peek_next( vos_list_t *pList, vos_list_node_t *pNode,
                               vos_list_node_t **ppNode )
{
   struct list_head * listptr;
   int rc, found = 0;
   vos_list_node_t *tmp;

   if ( ( pList == NULL) || ( pNode == NULL) || (ppNode == NULL))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty(&pList->anchor) )
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                 "%s: list empty", __func__);
       mutex_unlock(&pList->lock);
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
      return VOS_STATUS_E_INVAL;

   listptr = pNode->next;
   if (listptr == &pList->anchor)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
      return VOS_STATUS_E_EMPTY;
   }

   *ppNode =  listptr;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

  \brief vos_list_peek_prev() - peek at the node before the specified node

  The vos_list_peek_prev() API will return a pointer to the node before the
  specified node on a properly initialized vOS List object.  The node will
  *not* be removed from the list.

  \param pList - Pointer to list object of the list to be 'peeked'

  \param pNode - Pointer to the node that is being 'peeked'

  \param ppNode - Pointer to a pointer to the list node before the
  pNode node on the list.

  \return VOS_STATUS_SUCCESS - list node before pNode on the properly
          initialized list is successfully returned.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_EMPTY - There is no 'previous' node (the input node is
          at the front of the list).

          VOS_STATUS_E_FAULT  - pList, pNode or ppNode is an invalid pointer.

  \sa vos_list_remove_back()

  --------------------------------------------------------------------------*/

VOS_STATUS vos_list_peek_prev( vos_list_t *pList, vos_list_node_t *pNode,
                               vos_list_node_t **ppNode )
{
   struct list_head * listptr;
   int rc, found = 0;
   vos_list_node_t *tmp;

   if ( ( pList == NULL) || ( pNode == NULL) || (ppNode == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty(&pList->anchor) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
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
      return VOS_STATUS_E_INVAL;

   listptr = pNode->prev;

   if (listptr == &pList->anchor)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
      return VOS_STATUS_E_EMPTY;
   }

   *ppNode = listptr;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

  \brief vos_list_insert_before() - insert node at front of a specified
  list node

  The vos_list_insert_before() API will insert a node onto a properly
  initialized vOS List object in front of the specified list node.

  \param pList - Pointer to list object where the node will be inserted

  \param pNodeToInsert - Pointer to the list node to be inserted into the list.

  \param pNode - Pointer to the list node where pNodeToInsert will be inserted
  in front of.

  \return VOS_STATUS_SUCCESS - list node was successfully inserted onto
          the front of the list.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_FAULT  - pList, pNodeToInsert, or pNode are
          invalid pointer(s)

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS vos_list_insert_before( vos_list_t *pList, vos_list_node_t *pNodeToInsert,
                                   vos_list_node_t *pNode )
{
   int rc, found = 0;
   vos_list_node_t *tmp;

   if ( ( pList == NULL) || ( pNode == NULL) || (pNodeToInsert == NULL))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty(&pList->anchor) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
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
      return VOS_STATUS_E_INVAL;

   list_add(pNodeToInsert, pNode);
   pList->count++;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------

  \brief vos_list_insert_after() - insert node behind a specified list node

  The vos_list_insert_after() API will insert a node onto a properly
  initialized vOS List object after the specified list node.

  \param pList - Pointer to list object where the node will be inserted

  \param pNodeToInsert - Pointer to the list node to be inserted into the list.

  \param pNode - Pointer to the list node where pNodeToInsert will be inserted
  after.

  \return VOS_STATUS_SUCCESS - list node was successfully inserted onto
          the front of the list.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_FAULT  - pList, pNodeToInsert, or pNode are
          invalid pointer(s)

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS vos_list_insert_after( vos_list_t *pList, vos_list_node_t *pNodeToInsert,
                                  vos_list_node_t *pNode )
{
   int rc, found = 0;
   vos_list_node_t *tmp;

   if ( ( pList == NULL) || ( pNode == NULL) || (pNodeToInsert == NULL))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }


   if ( list_empty(&pList->anchor) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
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
      return VOS_STATUS_E_INVAL;

   list_add_tail(pNodeToInsert, pNode);
   pList->count++;
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

  \brief vos_list_remove_node() - remove specified node from vOS list list

  The vos_list_remove_node() API will remove a specified node from the
  properly initialized vOS List object.

  \param pList - Pointer to list object where the node will be removed

  \param ppNode - Pointer to the node to be removed from the list.

  \return VOS_STATUS_SUCCESS - list node was successfully removed from
          the list.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_EMPTY - The specified is empty so nodes cannot be
          removed.


          VOS_STATUS_E_FAULT  - pList or pNodeToRemove is not a valid pointer

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS vos_list_remove_node( vos_list_t *pList, vos_list_node_t *pNodeToRemove )
{
   int rc, found = 0;
   vos_list_node_t *tmp;

   if ( ( pList == NULL ) || ( pNodeToRemove == NULL) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: NULL pointer passed in", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( pList->cookie != VOS_LIST_COOKIE )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list not initialized", __func__);
      return VOS_STATUS_E_INVAL;
   }

   rc = mutex_lock_interruptible(&pList->lock);
   if (rc)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock list", __func__);
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty(&pList->anchor) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: list empty", __func__);
      mutex_unlock(&pList->lock);
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
   mutex_unlock(&pList->lock);

   return VOS_STATUS_SUCCESS;
}
