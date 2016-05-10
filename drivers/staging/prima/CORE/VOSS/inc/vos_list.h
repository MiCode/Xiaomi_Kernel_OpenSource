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

#if !defined( __VOS_LIST_H )
#define __VOS_LIST_H

/**=========================================================================
  
  \file  vos_list.h
  
  \brief virtual Operating System Services (vOSS) List APIs
               
   Definitions for vOSS Linked Lists API
   
   Lists are implemented as a doubly linked list. An item in a list can 
   be of any type as long as the datatype contains a field of type 
   vos_link_t.

   In general, a list is a doubly linked list of items with a pointer 
   to the front of the list and a pointer to the end of the list.  The
   list items contain a forward and back link.
            
     List                            Nodes
   =============          ===========================
   +-------+
   | Front |------------->+---------+     +---------+
   +-------+              | Next    |---->| Next    |---->NULL
   | Back  |-+            +---------+     +---------+
   +-------+ |   NULL<----| Prev    |<----| Prev    |
             |            +---------+     +---------+
             |            |User Data|     |User Data|
             |            +---------+     +---------+
             |                                 ^
             |                                 |
             +---------------------------------+

   This linked list API is implemented with appropriate locking
   mechanisms to assure operations on the list are thread safe.
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>
#include <i_vos_list.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
  
/**---------------------------------------------------------------------------
  
  \brief vos_list_init() - initialize a vOS Linked List  
    
  The \a vos_list_init() function initializes the specified linked list
  'object'.  Upon successful initialization, the state of the list 
  becomes initialized and available for use through the other vos_list_xxx
  APIs.

  A list must be initialized by calling vos_list_init() before it 
  may be used in any other lock functions. 
  
  Attempting to initialize an already initialized list results in 
  a failure.
 
  \param pList - pointer to the opaque list object to initialize
  
  \return VOS_STATUS_SUCCESS - list was successfully initialized and 
          is ready to be used.
  
          VOS_STATUS_E_RESOURCES - System resources (other than memory) 
          are unavailable to initilize the list

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize 
          the list

          VOS_STATUS_E_BUSY - The implementation has detected an attempt 
          to reinitialize the object referenced by list, a previously 
          initialized, but not yet destroyed, list.

          VOS_STATUS_E_FAULT  - pList is an invalid pointer.     
          
  \sa vos_list_destroy()
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_list_init( vos_list_t *pList );


/**-------------------------------------------------------------------------
  
  \brief vos_list_destroy() - Destroy a vOSS List

  The \a vos_list_destroy() function shall destroy the list object 
  referenced by pList.  After a successful return from \a vos_list_destroy()
  the list object becomes, in effect, uninitialized.
   
  A destroyed lock object can be reinitialized using vos_list_init(); 
  the results of otherwise referencing the object after it has been destroyed 
  are undefined.  Calls to vOSS list functions to manipulate the list such
  will fail if the list or has not been initialized or is destroyed.  
  Therefore, don't use the list after it has been destroyed until it has 
  been re-initialized.
  
  \param pLlist - pointer to the list object to be destroyed.
  
  \return VOS_STATUS_SUCCESS - list was successfully destroyed.
  
          VOS_STATUS_E_BUSY - The implementation has detected an attempt 
          to destroy the object referenced by pList that is still has
          nodes.  The list must be empty before it can be destroyed. 

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.
          
          VOS_STATUS_E_FAULT  - pList is an invalid pointer.     
  \sa
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_list_destroy( vos_list_t *pList );

/*--------------------------------------------------------------------------
  
  \brief vos_list_lock() - Lock a vOSS List

  The \a vos_list_lock() function shall lock the list object to prevent
  other tasks from operating on this list.  The list remains locked until
  a call to vos_list_unlock() is made.
  
  Each list function already operate within a critical section.  
  However it is sometimes necessary to lock a list over a series of list 
  function calls.  
  
  For example, when one needs to search a node on a list and insert another 
  node after it, one would want to lock this list for the entire process 
  in case another task attempts to manipulate this list.

  \param pLlist - pointer to the list object to be locked.
  
  \return VOS_STATUS_SUCCESS - list was successfully locked.
  
          VOS_STATUS_E_FAULT  - pList is an invalid pointer.     

  \sa vos_list_unlock()
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_list_lock( vos_list_t *pList );

/*--------------------------------------------------------------------------
  
  \brief vos_list_unlock() - Unlock a vOSS List

  The \a vos_list_unlock() function shall unlock the list object to allow 
  other tasks to use this list.  This function is only called when 
  the vos_list_lock() is previously called in the current task. 
  
  \param pLlist - pointer to the list object to be unlocked.
  
  \return VOS_STATUS_SUCCESS - list was successfully unlocked.
  
          VOS_STATUS_E_FAULT  - pList is an invalid pointer.     

  \sa vos_list_lock()
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_list_unlock( vos_list_t *pList );


/**---------------------------------------------------------------------------
  
  \brief vos_list_insert_front() - insert node at front of a linked list 

  The vos_list_insert_front() API will insert a node at the front of
  a properly initialized vOS List object. 
  
  \param pList - Pointer to list object where the node will be inserted
  
  \param pNode - Pointer to the list node to be inserted into the list.
  
  \return VOS_STATUS_SUCCESS - list node was successfully inserted onto 
          the front of the list.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.
          
          VOS_STATUS_E_FAULT  - pList is an invalid pointer or pNode is an
          invalid pointer.
    
  \sa
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_list_insert_front( vos_list_t *pList, vos_list_node_t *pNode );


/**---------------------------------------------------------------------------
  
  \brief vos_list_insert_back() - insert node at back of a linked list 

  The vos_list_insert_back() API will insert a node at the back of
  a properly initialized vOS List object. 
  
  \param pList - Pointer to list object where the node will be inserted
  
  \param pNode - Pointer to the list node to be inserted into the list.
  
  \return VOS_STATUS_SUCCESS - list node was successfully inserted onto 
          the back of the list.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.
          
          VOS_STATUS_E_FAULT  - pList is an invalid pointer or pNode is an
          invalid pointer.
    
  \sa
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_list_insert_back( vos_list_t *pList, vos_list_node_t *pNode );

/**---------------------------------------------------------------------------
  
  \brief vos_list_insert_back_size() - insert node at back of a linked list and
  return the size AFTER the enqueue. This size is determined in a race free 
  manner i.e. while the list is locked for the enqueue operation

  The vos_list_insert_back_size() API will insert a node at the back of
  a properly initialized vOS List object. 
  
  \param pList - Pointer to list object where the node will be inserted
  
  \param pNode - Pointer to the list node to be inserted into the list.

  \param pSize - Pointer to a size variable, where the size of the 
                 list will be returned.
 
  \return VOS_STATUS_SUCCESS - list node was successfully inserted onto 
          the back of the list.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.
          
          VOS_STATUS_E_FAULT  - pList is an invalid pointer or pNode is an
          invalid pointer.
    
  \sa
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_list_insert_back_size( vos_list_t *pList, vos_list_node_t *pNode, v_SIZE_t *pSize );


/**---------------------------------------------------------------------------
  
  \brief vos_list_remove_front() - remove node at front of a linked list 

  The vos_list_remove_front() API will remove a node at the front of
  a properly initialized vOS List object. 
  
  \param pList - Pointer to list object where the node will be removed
  
  \param ppNode - Pointer to a pointer to the list node to be removed 
  from the list.
  
  \return VOS_STATUS_SUCCESS - list node was successfully removed from 
          the front of the list.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_EMPTY - The specified is empty so nodes cannot be 
          removed.
         
          VOS_STATUS_E_FAULT  - pList is an invalid pointer or ppNode is an
          invalid pointer.
    
  \sa vos_list_remove_back()
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_list_remove_front( vos_list_t *pList, vos_list_node_t **ppNode );


/**---------------------------------------------------------------------------
  
  \brief vos_list_remove_back() - remove node at back of a linked list 

  The vos_list_remove_back() API will remove a node at the back of
  a properly initialized vOS List object. 
  
  \param pList - Pointer to list object where the node will be removed
  
  \param ppNode - Pointer to a pointer to the list node to be removed 
  from the list.
  
  \return VOS_STATUS_SUCCESS - list node was successfully removed from 
          the back of the list.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.

          VOS_STATUS_E_EMPTY - The specified is empty so nodes cannot be 
          removed.
          
          VOS_STATUS_E_FAULT  - pList is an invalid pointer or ppNode is an
          invalid pointer.
    
  \sa vos_list_remove_back()
  
  --------------------------------------------------------------------------*/  
VOS_STATUS vos_list_remove_back( vos_list_t *pList, vos_list_node_t **ppNode );


/*----------------------------------------------------------------------------
  
  \brief vos_list_size() - return the size of of a linked list 

  The vos_list_size() API will return the number of nodes on the 
  given vOS List object. 
  
  \param pList - Pointer to list object where the node will be counted
  
  \param pSize - Pointer to a size variable, where the size of the 
                 list will be returned.
  
  \return VOS_STATUS_SUCCESS - list size of the properly initialized 
          vos list object has been returned.

          VOS_STATUS_E_INVAL - The value specified by pList is not a valid,
          initialized list object.
          
          VOS_STATUS_E_FAULT  - pList or pSize are not valid pointers
    
  \sa
  
  --------------------------------------------------------------------------*/  
VOS_STATUS vos_list_size( vos_list_t *pList, v_SIZE_t *pSize );

/**---------------------------------------------------------------------------
  
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
VOS_STATUS vos_list_peek_front( vos_list_t *pList, vos_list_node_t **ppNode );

/**---------------------------------------------------------------------------
  
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
VOS_STATUS vos_list_peek_back( vos_list_t *pList, vos_list_node_t **ppNode );

/**---------------------------------------------------------------------------
  
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

          VOS_STATUS_E_EMPTY - The specified is empty so nodes cannot be 
          removed.
          
          VOS_STATUS_E_FAULT  - pList, pNode or ppNode is an invalid pointer.
    
  \sa vos_list_remove_back()
  
  --------------------------------------------------------------------------*/  
VOS_STATUS vos_list_peek_next( vos_list_t *pList, vos_list_node_t *pNode, 
                               vos_list_node_t **ppNode );

/**---------------------------------------------------------------------------
  
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
          
          VOS_STATUS_E_EMPTY - The specified is empty so nodes cannot be 
          removed.
          
          VOS_STATUS_E_FAULT  - pList, pNode or ppNode is an invalid pointer.
    
  \sa vos_list_remove_back()
  
  --------------------------------------------------------------------------*/                                 
VOS_STATUS vos_list_peek_prev( vos_list_t *pList, vos_list_node_t *pNode, 
                               vos_list_node_t **ppNode );

/**---------------------------------------------------------------------------
  
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
                                   vos_list_node_t *pNode );

/**---------------------------------------------------------------------------
  
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
                                  vos_list_node_t *pNode );         


/**---------------------------------------------------------------------------
  
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
VOS_STATUS vos_list_remove_node( vos_list_t *pList, vos_list_node_t *pNodeToRemove );



#endif // __VOS_LIST_H
