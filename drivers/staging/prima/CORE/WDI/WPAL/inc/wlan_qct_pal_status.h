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

#if !defined( __WLAN_QCT_PAL_STATUS_H )
#define __WLAN_QCT_PAL_STATUS_H

/**=========================================================================
  
  \file  wlan_qct_pal_status.h
  
  \brief define status PAL exports. wpt = (Wlan Pal Type)
               
   Definitions for platform independent. 
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

typedef enum
{
   /// Request succeeded!
   eWLAN_PAL_STATUS_SUCCESS,
   
   /// Request failed because system resources (other than memory) to 
   /// fulfill request are not available. 
   eWLAN_PAL_STATUS_E_RESOURCES,
  
   /// Request failed because not enough memory is available to 
   /// fulfill the request.
   eWLAN_PAL_STATUS_E_NOMEM,  
      
   /// Request failed because there of an invalid request.  This is 
   /// typically the result of invalid parameters on the request.
   eWLAN_PAL_STATUS_E_INVAL,
   
   /// Request failed because handling the request would cause a 
   /// system fault.  This error is typically returned when an 
   /// invalid pointer to memory is detected.
   eWLAN_PAL_STATUS_E_FAULT,

   /// Request failed because device or resource is busy. 
   eWLAN_PAL_STATUS_E_BUSY,

   /// Request did not complete because it was canceled.
   eWLAN_PAL_STATUS_E_CANCELED,

   /// Request did not complete because it was aborted.
   eWLAN_PAL_STATUS_E_ABORTED,
   
   /// Request failed because the request is valid, though not supported
   /// by the entity processing the request.
   eWLAN_PAL_STATUS_E_NOSUPPORT,
   
   /// Request failed because of an empty condition
   eWLAN_PAL_STATUS_E_EMPTY,
  
   /// Existance failure.  Operation could not be completed because
   /// something exists or does not exist.  
   eWLAN_PAL_STATUS_E_EXISTS,
   
   /// Operation timed out
   eWLAN_PAL_STATUS_E_TIMEOUT,
   
   /// Request failed for some unknown reason.  Note don't use this
   /// status unless nothing else applies
   eWLAN_PAL_STATUS_E_FAILURE,
} wpt_status;


#define WLAN_PAL_IS_STATUS_SUCCESS(status) ( eWLAN_PAL_STATUS_SUCCESS == (status) )

#endif // __WLAN_QCT_PAL_STATUS_H
