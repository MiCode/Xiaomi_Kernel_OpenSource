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

#if !defined( __VOS_THREADS_H )
#define __VOS_THREADS_H

/**=========================================================================
  
  \file  vos_threads.h
  
  \brief virtual Operating System Services (vOSS) Threading APIs
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
  
  \brief vos_sleep() - sleep

  The \a vos_sleep() function suspends the execution of the current thread
  until the specified time out interval elapses.
     
  \param msInterval - the number of milliseconds to suspend the current thread.
  A value of 0 may or may not cause the current thread to yield.
  
  \return Nothing.
    
  \sa
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_sleep( v_U32_t msInterval );

/*----------------------------------------------------------------------------
  
  \brief vos_sleep_us() - sleep

  The \a vos_sleep_us() function suspends the execution of the current thread
  until the specified time out interval elapses.
     
  \param usInterval - the number of microseconds to suspend the current thread.
  A value of 0 may or may not cause the current thread to yield.
  
  \return Nothing.
    
  \sa
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_sleep_us( v_U32_t usInterval );


/*----------------------------------------------------------------------------
  
  \brief vos_busy_wait() - busy wait

  The \a vos_busy_wait() function places the current thread in busy wait
  until the specified time out interval elapses. If the interval is greater
  than 50us on WM, the behaviour is undefined.
     
  \param usInterval - the number of microseconds to busy wait. 
  
  \return Nothing.
    
  \sa
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_busy_wait( v_U32_t usInterval );

#endif // __VOS_THREADS_H
