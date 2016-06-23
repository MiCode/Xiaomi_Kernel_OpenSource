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

#if !defined( __WLAN_QCT_OS_TIMER_H )
#define __WLAN_QCT_OS_TIMER_H

/**=========================================================================
  
  \file  wlan_qct_os_timer.h
  
  \brief define synchronization objects PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform dependent (Linux Android).
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "vos_timer.h"

typedef struct
{
   vos_timer_t timerObj;
} wpt_os_timer;


#endif // __WLAN_QCT_OS_TIMER_H
