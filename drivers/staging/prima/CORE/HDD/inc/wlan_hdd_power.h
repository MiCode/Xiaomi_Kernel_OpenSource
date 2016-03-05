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
* Copyright (c) 2012 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/

#ifndef __WLAN_HDD_POWER_H
#define __WLAN_HDD_POWER_H

/**===========================================================================

  \file  wlan_hdd_power.h

  \brief Linux HDD Power
         Copyright 2008 (c) Qualcomm Technologies, Inc.
         All Rights Reserved.
         Qualcomm Technologies Confidential and Proprietary.

  ==========================================================================*/


/*--------------------------------------------------------------------------
 * Include Files
 *------------------------------------------------------------------------*/
#include "wlan_hdd_main.h"

/*---------------------------------------------------------------------------
 *   Preprocessor definitions and constants
 *-------------------------------------------------------------------------*/
 //gEnableSuspend = 1 in INI file implies suspend to standby
 #define WLAN_MAP_SUSPEND_TO_STANDBY     1

 //gEnableSuspend = 2 in INI file implies suspend to deep sleep
 #define WLAN_MAP_SUSPEND_TO_DEEP_SLEEP  2

 //gEnableSuspend = 3 in INI file implies suspend to set MCAST/BCAST filter 
 #define WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER  3

 //gEnableDriverStop = 1 implies map driver stop to standby
 #define WLAN_MAP_DRIVER_STOP_TO_STANDBY     1

 //gEnableDriverStop = 2 implies map sriver stop to deep sleep
 #define WLAN_MAP_DRIVER_STOP_TO_DEEP_SLEEP  2

 //Maximum time (ms) to wait for standby to complete
 #define WLAN_WAIT_TIME_STANDBY          3000

 //Maximum time (ms) to wait for full pwr to complete
 #define WLAN_WAIT_TIME_FULL_PWR         3000


/*---------------------------------------------------------------------------
 *   Type declarations
 *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------
 * Function declarations and documentation
 * ------------------------------------------------------------------------*/
 eHalStatus hdd_exit_standby(hdd_context_t *pHddCtx);
 VOS_STATUS hdd_exit_deep_sleep(hdd_context_t *pHddCtx, 
                                hdd_adapter_t* pAdapter);
 VOS_STATUS hdd_enter_standby(hdd_context_t *pHddCtx);
 VOS_STATUS hdd_enter_deep_sleep(hdd_context_t *pHddCtx, 
                                hdd_adapter_t* pAdapter);
#ifdef CONFIG_HAS_EARLYSUSPEND
 VOS_STATUS hdd_wlan_reset(void);
 VOS_STATUS hdd_wlan_reset_initialization(void) ;
#endif
 /* SSR shutdown & re-init functions */
 VOS_STATUS hdd_wlan_shutdown(void);
 VOS_STATUS hdd_wlan_re_init(void);

void hdd_conf_mcastbcast_filter(hdd_context_t* pHddCtx, v_BOOL_t setfilter);
VOS_STATUS hdd_conf_arp_offload(hdd_adapter_t* pAdapter, int fenable);
/*
 * Function: hdd_conf_hostoffload
 *           Central function to configure the supported offloads,
 *           either enable or disable them.
 */
void hdd_conf_hostoffload(hdd_adapter_t * pAdapter, v_BOOL_t fenable);
#ifdef WLAN_FEATURE_GTK_OFFLOAD
void hdd_conf_gtk_offload(hdd_adapter_t *pAdapter, v_BOOL_t fenable);
#endif
#ifdef WLAN_NS_OFFLOAD
void hdd_conf_ns_offload(hdd_adapter_t *pAdapter, int fenable);
#endif
#endif // if !defined __WLAN_QCT_DRIVER_H
