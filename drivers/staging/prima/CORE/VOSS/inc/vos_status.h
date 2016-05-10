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

#if !defined( __VOS_STATUS_H )
#define __VOS_STATUS_H

/**=========================================================================
  
  \file  vos_Status.h
  
  \brief virtual Operating System Services (vOSS) Status codes
               
   Basic status codes/definitions used by vOSS 
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

typedef enum
{
   /// Request succeeded!
   VOS_STATUS_SUCCESS,
   
   /// Request failed because system resources (other than memory) to 
   /// fulfill request are not available. 
   VOS_STATUS_E_RESOURCES,
  
   /// Request failed because not enough memory is available to 
   /// fulfill the request.
   VOS_STATUS_E_NOMEM,  
   
   /// Request could not be fulfilled at this time.  Try again later.
   VOS_STATUS_E_AGAIN,
   
   /// Request failed because there of an invalid request.  This is 
   /// typically the result of invalid parameters on the request.
   VOS_STATUS_E_INVAL,
   
   /// Request failed because handling the request would cause a 
   /// system fault.  This error is typically returned when an 
   /// invalid pointer to memory is detected.
   VOS_STATUS_E_FAULT,

   /// Request refused becayse a request is already in progress and 
   /// another cannot be handled currently.
   VOS_STATUS_E_ALREADY,
    
   /// Request failed because the message (type) is bad, invalid, or
   /// not properly formatted.
   VOS_STATUS_E_BADMSG,

   /// Request failed because device or resource is busy. 
   VOS_STATUS_E_BUSY,

   /// Request did not complete because it was canceled.
   VOS_STATUS_E_CANCELED,

   /// Request did not complete because it was aborted.
   VOS_STATUS_E_ABORTED,
   
   /// Request failed because the request is valid, though not supported
   /// by the entity processing the request.
   VOS_STATUS_E_NOSUPPORT,
   
   /// Operation is not permitted.
   VOS_STATUS_E_PERM,
   
   /// Request failed because of an empty condition
   VOS_STATUS_E_EMPTY,
  
   /// Existance failure.  Operation could not be completed because
   /// something exists or does not exist.  
   VOS_STATUS_E_EXISTS,
   
   /// Operation timed out
   VOS_STATUS_E_TIMEOUT,
   
   /// Request failed for some unknown reason.  Note don't use this
   /// status unless nothing else applies
   VOS_STATUS_E_FAILURE   

} VOS_STATUS;

/// Macro to determine if a VOS_STATUS type is success.  All callers
/// wanting to interpret VOS_STATUS should use this macro to check 
/// for success to protect against the VOS_STATUS definitions 
/// changing.
///
/// Use like this...
///
///  if ( VOS_STATUS_SUCCESS( vosStatus ) ) ...
///
#define VOS_IS_STATUS_SUCCESS( status ) ( VOS_STATUS_SUCCESS == ( status ) )



#endif // if !defined __VOS_STATUS_H
