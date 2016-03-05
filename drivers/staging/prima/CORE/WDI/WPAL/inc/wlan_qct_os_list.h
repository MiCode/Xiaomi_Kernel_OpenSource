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

#if !defined( __WLAN_QCT_OS_LIST_H )
#define __WLAN_QCT_OS_LIST_H

/**=========================================================================
  
  \file  wlan_qct_pal_list.h
  
  \brief define linked list PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform dependent. It is with VOSS support.
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
//Include vos_list.h here. For non-VOSS PAL, it needs to provide its own definition.
#include "vos_list.h"

typedef vos_list_t wpt_list;
typedef vos_list_node_t wpt_list_node;

#define WPAL_LIST_STATUS_BASIC_CHECK(status) ( VOS_IS_STATUS_SUCCESS(status) ? \
            eWLAN_PAL_STATUS_SUCCESS : eWLAN_PAL_STATUS_E_FAILURE )

#define WPAL_LIST_IS_VOS_STATUS_BUSY(status) (VOS_STATUS_E_BUSY == (status))
#define WPAL_LIST_STATUS_BUSY_CHECK(status) ( VOS_IS_STATUS_SUCCESS(status) ? \
            eWLAN_PAL_STATUS_SUCCESS : (WPAL_LIST_IS_VOS_STATUS_BUSY(status) ? \
            eWLAN_PAL_STATUS_E_BUSY : eWLAN_PAL_STATUS_E_FAILURE) )

/**---------------------------------------------------------------------------
  
  \brief wpal_list_init() - initialize a wpt_list Linked List  
    
  The \a wpal_list_init() function initializes the specified linked list
  'object'.  Upon successful initialization, the state of the list 
  becomes initialized and available for use through the other wpt_list_xxx
  APIs.

  A list must be initialized by calling wpal_list_init() before it 
  may be used in any other lock functions. 
  
  Attempting to initialize an already initialized list results in 
  a failure.
 
  \param pList - pointer to the opaque list object to initialize
  
  \return eWLAN_PAL_STATUS_SUCCESS - list was successfully initialized and 
          is ready to be used.
  
          eWLAN_PAL_STATUS_E_RESOURCES - System resources (other than memory) 
          are unavailable to initilize the list

          eWLAN_PAL_STATUS_E_NOMEM - insufficient memory exists to initialize 
          the list

          eWLAN_PAL_STATUS_E_BUSY - The implementation has detected an attempt 
          to reinitialize the object referenced by list, a previously 
          initialized, but not yet destroyed, list.

          eWLAN_PAL_STATUS_E_FAULT  - pList is an invalid pointer.     
          
  \sa 
  
  --------------------------------------------------------------------------*/
#define wpal_list_init(pList) \
         WPAL_LIST_STATUS_BASIC_CHECK( vos_list_init( (vos_list_t *)(pList) ) )


/**-------------------------------------------------------------------------
  
  \brief wpal_list_destroy() - Destroy a wpt_list List

  The \a wpal_list_destroy() function shall destroy the list object 
  referenced by pList.  After a successful return from \a wpal_list_destroy()
  the list object becomes, in effect, uninitialized.
   
  A destroyed lock object can be reinitialized using wpal_list_init(); 
  the results of otherwise referencing the object after it has been destroyed 
  are undefined.  Calls to wpt_list functions to manipulate the list such
  will fail if the list or has not been initialized or is destroyed.  
  Therefore, don't use the list after it has been destroyed until it has 
  been re-initialized.
  
  \param pLlist - pointer to the list object to be destroyed.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list was successfully destroyed.
  
          eWLAN_PAL_STATUS_E_BUSY - The implementation has detected an attempt 
          to destroy the object referenced by pList that is still has
          nodes.  The list must be empty before it can be destroyed. 

          eWLAN_PAL_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.
          
          eWLAN_PAL_STATUS_E_FAULT  - pList is an invalid pointer.     
  \sa
  
  ----------------------------------------------------------------------------*/
#define wpal_list_destroy(pList) \
    WPAL_LIST_STATUS_BUSY_CHECK( vos_list_destroy( (vos_list_t *)(pList) ) )

/**---------------------------------------------------------------------------
  
  \brief wpal_list_insert_front() - insert node at front of a linked list 

  The wpal_list_insert_front() API will insert a node at the front of
  a properly initialized wpt_list object. 
  
  \param pList - Pointer to list object where the node will be inserted
  
  \param pNode - Pointer to the list node to be inserted into the list.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node was successfully inserted onto 
          the front of the list.

          eWLAN_PAL_STATUS_E_FAILURE - Failure.
    
  \sa
  
  --------------------------------------------------------------------------*/
//wpt_status wpal_list_insert_front( wpt_list *pList, wpt_list_node *pNode );
#define wpal_list_insert_front(pList, pNode) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_insert_front( (vos_list_t *)(pList), (vos_list_node_t *)(pNode) ) )


/**---------------------------------------------------------------------------
  
  \brief wpal_list_insert_back() - insert node at back of a linked list 

  The wpal_list_insert_back() API will insert a node at the back of
  a properly initialized wpt_list object. 
  
  \param pList - Pointer to list object where the node will be inserted
  
  \param pNode - Pointer to the list node to be inserted into the list.
  
  \return eWLAN_PAL__STATUS_SUCCESS - list node was successfully inserted onto 
          the back of the list.

          eWLAN_PAL_STATUS_E_FAILURE - Failure.
    
  \sa
  
  --------------------------------------------------------------------------*/
//wpt_status wpal_list_insert_back( wpt_list *pList, wpt_list_node *pNode );
#define wpal_list_insert_back(pList, pNode) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_insert_back( (vos_list_t *)(pList), (vos_list_node_t *)(pNode) ) )


/**---------------------------------------------------------------------------
  
  \brief wpal_list_remove_front() - remove node at front of a linked list 

  The wpal_list_remove_front() API will remove a node at the front of
  a properly initialized wpt_ist object. 
  
  \param pList - Pointer to list object where the node will be removed
  
  \param ppNode - Pointer to a pointer to the list node to be removed 
  from the list.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node was successfully removed from 
          the front of the list.

          eWLAN_PAL_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          eWLAN_PAL_STATUS_E_EMPTY - The specified is empty so nodes cannot be 
          removed.
         
          eWLAN_PAL_STATUS_E_FAULT  - pList is an invalid pointer or ppNode is an
          invalid pointer.
    
  \sa 
  
  --------------------------------------------------------------------------*/
//wpt_status wpal_list_remove_front( wpt_list *pList, wpt_list_node **ppNode );
#define wpal_list_remove_front(pList, ppNode) \
  ((wpt_status)vos_list_remove_front( (vos_list_t *)(pList), (vos_list_node_t **)(ppNode) ))


/**---------------------------------------------------------------------------
  
  \brief wpal_list_remove_back() - remove node at back of a linked list 

  The wpal_list_remove_back() API will remove a node at the back of
  a properly initialized wpt_list object. 
  
  \param pList - Pointer to list object where the node will be removed
  
  \param ppNode - Pointer to a pointer to the list node to be removed 
  from the list.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node was successfully removed from 
          the back of the list.

          eWLAN_PAL_STATUS_E_FAILURE - Failure.
    
  \sa 
  
  --------------------------------------------------------------------------*/  
//wpt_status wpal_list_remove_back( wpt_list *pList, wpt_list_node **ppNode );
#define wpal_list_remove_back(pList, ppNode) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_remove_back( (vos_list_t *)(pList), (vos_list_node_t **)(ppNode) ) )


/*----------------------------------------------------------------------------
  
  \brief wpal_list_size() - return the size of of a linked list 

  The wpal_list_size() API will return the number of nodes on the 
  given wpt_list object. 
  
  \param pList - Pointer to list object where the node will be counted
  
  \param pSize - Pointer to a size variable, where the size of the 
                 list will be returned.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list size of the properly initialized 
          wpt_list object has been returned.

          eWLAN_PAL_STATUS_E_FAILURE - Failure
    
  \sa
  
  --------------------------------------------------------------------------*/  
//wpt_status wpal_list_size( wpt_list *pList, wpt_uint32 *pSize );
#define wpal_list_size(pList, pSize) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_size( (vos_list_t *)(pList), (v_SIZE_t *)(pSize) ) )

/**---------------------------------------------------------------------------
  
  \brief wpal_list_peek_front() - peek at the node at front of a linked list 

  The wpal_list_peek_front() API will return a pointer to the node at the 
  front of a properly initialized wpt_list object.  The node will *not* be
  removed from the list.
  
  \param pList - Pointer to list object of the list to be 'peeked' 
  
  \param ppNode - Pointer to a pointer to the list node that exists at
  the front of the list.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node at the front of the list was 
          successfully returned.

          eWLAN_PAL_STATUS_E_Failure.
    
  \sa 
  
  --------------------------------------------------------------------------*/  
//wpt_status wpal_list_peek_front( wpt_list *pList, wpt_list_node **ppNode );
#define wpal_list_peek_front(pList, ppNode) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_peek_front( (vos_list_t *)(pList), (vos_list_node_t **)(ppNode) ) )

/**---------------------------------------------------------------------------
  
  \brief wpal_list_peek_back() - peek at the node at back of a linked list 

  The wpal_list_peek_back() API will return a pointer to the node at the 
  back of a properly initialized wpt_list object.  The node will *not* be
  removed from the list.
  
  \param pList - Pointer to list object of the list to be 'peeked' 
  
  \param ppNode - Pointer to a pointer to the list node that exists at
  the back of the list.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node at the back of the list was 
          successfully returned.

          eWLAN_PAL_STATUS_E_FAILURE - Failure.
    
  \sa 
  
  --------------------------------------------------------------------------*/  
//wpt_status wpal_list_peek_back( wpal_list *pList, wpt_list_node **ppNode );
#define wpal_list_peek_back(pList, ppNode) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_peek_back( (vos_list_t *)(pList), (vos_list_node_t **)(ppNode) ) )

/**---------------------------------------------------------------------------
  
  \brief wpal_list_peek_next() - peek at the node after the specified node 

  The wpal_list_peek_next() API will return a pointer to the node following the
  specified node on a properly initialized wpt_list object.  The node will 
  *not* be removed from the list.
  
  \param pList - Pointer to list object of the list to be 'peeked' 
  
  \param pNode - Pointer to the node that is being 'peeked'
  
  \param ppNode - Pointer to a pointer to the list node that follows the
  pNode node on the list.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node following pNode on the properly 
          initialized list is successfully returned.

          eWLAN_PAL_STATUS_E_FAILURE - Failure.
    
  \sa 
  
  --------------------------------------------------------------------------*/  
//wpt_status wpal_list_peek_next( wpt_list *pList, wpt_list_node *pNode, 
//                               wpt_list_node **ppNode );
#define wpal_list_peek_next(pList, pNode, ppNode) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_peek_next( (vos_list_t *)(pList), (vos_list_node_t *)(pNode), (vos_list_node_t **)(ppNode) ) )

/**---------------------------------------------------------------------------
  
  \brief wpal_list_peek_prev() - peek at the node before the specified node 

  The wpal_list_peek_prev() API will return a pointer to the node before the
  specified node on a properly initialized wpt_list object.  The node will 
  *not* be removed from the list.
  
  \param pList - Pointer to list object of the list to be 'peeked' 
  
  \param pNode - Pointer to the node that is being 'peeked'
  
  \param ppNode - Pointer to a pointer to the list node before the
  pNode node on the list.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node before pNode on the properly 
          initialized list is successfully returned.

          eWLAN_PAL_STATUS_E_FAILURE - Failure.
    
  \sa 
  
  --------------------------------------------------------------------------*/                                 
//wpt_status wpal_list_peek_prev( wpt_list *pList, wpt_list_node *pNode, 
//                               wpt_list_node **ppNode );
#define wpal_list_peek_prev(pList, pNode, ppNode) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_peek_prev( (vos_list_t *)(pList), (vos_list_node_t *)(pNode), (vos_list_node_t **)(ppNode) ) )

/**---------------------------------------------------------------------------
  
  \brief wpal_list_insert_before() - insert node at front of a specified
  list node 

  The wpal_list_insert_before() API will insert a node onto a properly 
  initialized wpt_list object in front of the specified list node.
  
  \param pList - Pointer to list object where the node will be inserted
  
  \param pNodeToInsert - Pointer to the list node to be inserted into the list.
  
  \param pNode - Pointer to the list node where pNodeToInsert will be inserted
  in front of.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node was successfully inserted onto 
          the front of the list.

          eWLAN_PAL_STATUS_FAILURE - Failure.
    
  \sa
  
  --------------------------------------------------------------------------*/
//wpt_status wpal_list_insert_before( wpt_list *pList, 
//                                    wpt_list_node *pNodeToInsert, 
//                                    wpt_list_node *pNode );
#define wpal_list_insert_before(pList, pNodeToInsert, pNode) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_insert_before( (vos_list_t *)(pList), \
                  (vos_list_node_t *)(pNodeToInsert), (vos_list_node_t *)(pNode) ) )

/**---------------------------------------------------------------------------
  
  \brief wpal_list_insert_after() - insert node behind a specified list node 

  The wpal_list_insert_after() API will insert a node onto a properly 
  initialized wpt_list object after the specified list node.
  
  \param pList - Pointer to list object where the node will be inserted
  
  \param pNodeToInsert - Pointer to the list node to be inserted into the list.
  
  \param pNode - Pointer to the list node where pNodeToInsert will be inserted
  after.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node was successfully inserted onto 
          the front of the list.

          eWLAN_PAL_STATUS_E_FAILURE - Failure
    
  \sa
  
  --------------------------------------------------------------------------*/                                   
//wpt_status wpal_list_insert_after( wpt_list *pList, 
//                                   wpt_list_node *pNodeToInsert, 
//                                   wpt_list_node *pNode );         
#define wpal_list_insert_after(pList, pNodeToInsert, pNode) \
    (WPAL_LIST_STATUS_BASIC_CHECK( vos_list_insert_after((vos_list_t *)(pList), \
                                                         (vos_list_node_t *)(pNodeToInsert), (vos_list_node_t *)(pNode) ))


/**---------------------------------------------------------------------------
  
  \brief wpal_list_remove_node() - remove specified node from wpt_list list 

  The wpal_list_remove_node() API will remove a specified node from the 
  properly initialized wpt_list object. 
  
  \param pList - Pointer to list object where the node will be removed
  
  \param ppNode - Pointer to the node to be removed from the list.
  
  \return eWLAN_PAL_STATUS_SUCCESS - list node was successfully removed from 
          the list.

          eWLAN_PAL_STATUS_E_FAILURE - Failure.
    
  \sa
  
  --------------------------------------------------------------------------*/  
//wpt_status wpal_list_remove_node( wpt_list *pList, wpt_list_node *pNodeToRemove );
#define wpal_list_remove_node(pList, pNodeToRemove) \
    WPAL_LIST_STATUS_BASIC_CHECK( vos_list_remove_node( (vos_list_t *)(pList), \
            (vos_list_node_t *)(pNodeToRemove) ) )


#endif // __WLAN_QCT_OS_LIST_H
