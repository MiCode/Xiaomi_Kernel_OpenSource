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

#if !defined( __WLAN_QCT_OS_STATUS_H )
#define __WLAN_QCT_OS_STATUS_H

/**=========================================================================
  
  \file  wlan_qct_os_status.h
  
  \brief define synchronization objects PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform dependent(Windows XP). 
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "vos_status.h"

/**
 * \brief Macros to derive PAL STATUS from the VOS STATUS
 */

#define WPAL_IS_VOS_STATUS_E_RESOURCES(status) ( VOS_STATUS_E_RESOURCES == (status)) 
#define WPAL_IS_VOS_STATUS_E_NOMEM(status) ( VOS_STATUS_E_NOMEM == (status)) 
#define WPAL_IS_VOS_STATUS_E_INVAL(status) ( VOS_STATUS_E_INVAL == (status)) 
#define WPAL_IS_VOS_STATUS_E_FAULT(status) ( VOS_STATUS_E_FAULT == (status)) 
#define WPAL_IS_VOS_STATUS_E_BUSY(status) ( VOS_STATUS_E_BUSY == (status)) 
#define WPAL_IS_VOS_STATUS_E_CANCELED(status) ( VOS_STATUS_E_CANCELED == (status)) 
#define WPAL_IS_VOS_STATUS_E_ABORTED(status) ( VOS_STATUS_E_ABORTED == (status)) 
#define WPAL_IS_VOS_STATUS_E_NOSUPPORT(status) ( VOS_STATUS_E_NOSUPPORT == (status)) 
#define WPAL_IS_VOS_STATUS_E_EMPTY(status) ( VOS_STATUS_E_EMPTY == (status)) 
#define WPAL_IS_VOS_STATUS_E_EXISTS(status) ( VOS_STATUS_E_EXISTS == (status)) 
#define WPAL_IS_VOS_STATUS_E_TIMEOUT(status) ( VOS_STATUS_E_TIMEOUT == (status)) 


#define WPAL_STATUS_E_TIMEOUT_CHECK(status) ( WPAL_IS_VOS_STATUS_E_TIMEOUT(status)? eWLAN_PAL_STATUS_E_TIMEOUT : eWLAN_PAL_STATUS_E_FAILURE )

#define WPAL_STATUS_E_EXISTS_CHECK(status) ( WPAL_IS_VOS_STATUS_E_EXISTS(status)? eWLAN_PAL_STATUS_E_EXISTS : WPAL_STATUS_E_TIMEOUT_CHECK(status) )

#define WPAL_STATUS_E_EMPTY_CHECK(status) ( WPAL_IS_VOS_STATUS_E_EMPTY(status)? eWLAN_PAL_STATUS_E_EMPTY : WPAL_STATUS_E_EXISTS_CHECK(status) )

#define WPAL_STATUS_E_NOSUPPORT_CHECK(status) ( WPAL_IS_VOS_STATUS_E_NOSUPPORT(status)? eWLAN_PAL_STATUS_E_NOSUPPORT : WPAL_STATUS_E_EMPTY_CHECK(status) )

#define WPAL_STATUS_E_ABORTED_CHECK(status) ( WPAL_IS_VOS_STATUS_E_ABORTED(status)? eWLAN_PAL_STATUS_E_ABORTED : WPAL_STATUS_E_NOSUPPORT_CHECK(status) )

#define WPAL_STATUS_E_CANCELED_CHECK(status) ( WPAL_IS_VOS_STATUS_E_CANCELED(status)? eWLAN_PAL_STATUS_E_CANCELED : WPAL_STATUS_E_ABORTED_CHECK(status) )

#define WPAL_STATUS_E_BUSY_CHECK(status) ( WPAL_IS_VOS_STATUS_E_BUSY(status)? eWLAN_PAL_STATUS_E_BUSY : WPAL_STATUS_E_CANCELED_CHECK(status) )

#define WPAL_STATUS_E_FAULT_CHECK(status) ( WPAL_IS_VOS_STATUS_E_FAULT(status)? eWLAN_PAL_STATUS_E_FAULT : WPAL_STATUS_E_BUSY_CHECK(status) )

#define WPAL_STATUS_E_INVAL_CHECK(status) ( WPAL_IS_VOS_STATUS_E_INVAL(status)? eWLAN_PAL_STATUS_E_INVAL : WPAL_STATUS_E_FAULT_CHECK(status) )

#define WPAL_STATUS_E_NOMEM_CHECK(status) ( WPAL_IS_VOS_STATUS_E_NOMEM(status)? eWLAN_PAL_STATUS_E_NOMEM : WPAL_STATUS_E_INVAL_CHECK(status) )

#define WPAL_STATUS_E_RESOURCES_CHECK(status) ( WPAL_IS_VOS_STATUS_E_RESOURCES(status)? eWLAN_PAL_STATUS_E_RESOURCES : WPAL_STATUS_E_NOMEM_CHECK(status) )

#define WPAL_VOS_TO_WPAL_STATUS(status) ( VOS_IS_STATUS_SUCCESS(status)? eWLAN_PAL_STATUS_SUCCESS : WPAL_STATUS_E_RESOURCES_CHECK(status) )

#endif // __WLAN_QCT_OS_STATUS_H
