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

#if !defined( __VOS_LOCK_H )
#define __VOS_LOCK_H

/**=========================================================================
  
  \file  vos_lock.h
  
  \brief virtual Operating System Servies (vOS) Locks
               
   Definitions for vOSS Locks
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_status.h"
#include "i_vos_lock.h"

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  
  \brief vos_lock_init() - initialize a vOSS lock
  
  The \a vos_lock_init() function initializes the specified lock. Upon 
  successful initialization, the state of the lock becomes initialized 
  and unlocked.

  A lock must be initialized by calling vos_lock_init() before it 
  may be used in any other lock functions. 
  
  Attempting to initialize an already initialized lock results in 
  a failure.
 
  \param lock - pointer to the opaque lock object to initialize
  
  \return VOS_STATUS_SUCCESS - lock was successfully initialized and 
          is ready to be used.
  
          VOS_STATUS_E_RESOURCES - System resources (other than memory) 
          are unavailable to initilize the lock

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize 
          the lock

          VOS_STATUS_E_BUSY - The implementation has detected an attempt 
          to reinitialize the object referenced by lock, a previously 
          initialized, but not yet destroyed, lock.

          VOS_STATUS_E_FAULT  - lock is an invalid pointer.     
  \sa
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_lock_init( vos_lock_t *lock );

/*--------------------------------------------------------------------------
  
  \brief vos_lock_acquire() - acquire a lock

  A lock object is acquired by calling \a vos_lock_acquire().  If the lock 
  is already locked, the calling thread shall block until the lock becomes 
  available. This operation shall return with the lock object referenced by 
  lock in the locked state with the calling thread as its owner. 
  
  \param lock - the lock object to acquire
  
  \return VOS_STATUS_SUCCESS - the lock was successfully acquired by 
          the calling thread.
  
          VOS_STATUS_E_INVAL - The value specified by lock does not refer 
          to an initialized lock object.
          
          VOS_STATUS_E_FAULT - lock is an invalid pointer.     
          
  \sa
  
  ------------------------------------------------------------------------*/
VOS_STATUS vos_lock_acquire( vos_lock_t * lock );


/*--------------------------------------------------------------------------
  
  \brief vos_lock_release() - release a lock

  The \a vos_lock_release() function shall release the lock object 
  referenced by 'lock'.  

  If a thread attempts to release a lock that it unlocked or is not
  initialized, an error is returned. 

  \param lock - the lock to release
  
  \return VOS_STATUS_SUCCESS - the lock was successfully released
  
          VOS_STATUS_E_INVAL - The value specified by lock does not refer 
          to an initialized lock object.
                   
          VOS_STATUS_E_FAULT - The value specified by lock does not refer 
          to an initialized lock object.
                   
          VOS_STATUS_E_PERM - Operation is not permitted.  The calling 
          thread does not own the lock. 
    
  \sa
  
  ------------------------------------------------------------------------*/
VOS_STATUS vos_lock_release( vos_lock_t *lock );


/*--------------------------------------------------------------------------
  
  \brief vos_lock_destroy() - Destroy a vOSS Lock

  The \a vos_lock_destroy() function shall destroy the lock object 
  referenced by lock.  After a successful return from \a vos_lock_destroy()
  the lock object becomes, in effect, uninitialized.
   
  A destroyed lock object can be reinitialized using vos_lock_init(); 
  the results of otherwise referencing the object after it has been destroyed 
  are undefined.  Calls to vOSS lock functions to manipulate the lock such
  as vos_lock_acquire() will fail if the lock is destroyed.  Therefore, 
  don't use the lock after it has been destroyed until it has 
  been re-initialized.
  
  \param lock - the lock object to be destroyed.
  
  \return VOS_STATUS_SUCCESS - lock was successfully destroyed.
  
          VOS_STATUS_E_BUSY - The implementation has detected an attempt 
          to destroy the object referenced by lock while it is locked 
          or still referenced. 

          VOS_STATUS_E_INVAL - The value specified by lock is invalid.
          
          VOS_STATUS_E_FAULT  - lock is an invalid pointer.     
  \sa
  
  ------------------------------------------------------------------------*/
VOS_STATUS vos_lock_destroy( vos_lock_t *lock );

/*--------------------------------------------------------------------------
  
  \brief vos_spin_lock_init() - initializes a vOSS spin lock
  
  The vos_spin_lock_init() function initializes the specified spin lock. Upon 
  successful initialization, the state of the lock becomes initialized 
  and unlocked.

  A lock must be initialized by calling vos_spin_lock_init() before it 
  may be used in any other lock functions. 
  
  Attempting to initialize an already initialized lock results in 
  a failure.
 
  \param pLock - pointer to the opaque lock object to initialize
  
  \return VOS_STATUS_SUCCESS - spin lock was successfully initialized and 
          is ready to be used.
  --------------------------------------------------------------------------*/
VOS_STATUS vos_spin_lock_init(vos_spin_lock_t *pLock);

/*--------------------------------------------------------------------------
  
  \brief vos_spin_lock_acquire() - acquires a spin lock

  A lock object is acquired by calling \a vos_spin_lock_acquire().  If the lock 
  is already locked, the calling thread shall spin until the lock becomes 
  available. This operation shall return with the lock object referenced by 
  lock in the locked state with the calling thread as its owner. 
  
  \param pLock - the lock object to acquire
  
  \return VOS_STATUS_SUCCESS - the lock was successfully acquired by 
          the calling thread.
      
  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_spin_lock_acquire(vos_spin_lock_t *pLock);

/*--------------------------------------------------------------------------
  
  \brief vos_spin_lock_release() - releases a lock

  The \a vos_lock_release() function shall release the spin lock object 
  referenced by 'lock'.  

  If a thread attempts to release a lock that it unlocked or is not
  initialized, an error is returned. 

  \param pLock - the lock to release
  
  \return VOS_STATUS_SUCCESS - the lock was successfully released
  
  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_spin_lock_release(vos_spin_lock_t *pLock);

/*--------------------------------------------------------------------------
  
  \brief vos_spin_lock_destroy() - releases resource of a lock

  \param pLock - pointer to a lock to release
  
  \return VOS_STATUS_SUCCESS - the lock was successfully released
  
  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_spin_lock_destroy(vos_spin_lock_t *pLock);


#endif // __VOSS_LOCK_H
