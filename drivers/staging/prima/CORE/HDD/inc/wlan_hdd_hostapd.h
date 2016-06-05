/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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

#if !defined( WLAN_HDD_HOSTAPD_H )
#define WLAN_HDD_HOSTAPD_H

/**===========================================================================

  \file  WLAN_HDD_HOSTAPD_H.h

  \brief Linux HDD HOSTAPD include file
  ==========================================================================*/

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <vos_list.h>
#include <vos_types.h>

#include <wlan_qct_tl.h>
#include <wlan_hdd_main.h>

/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/

/* max length of command string in hostapd ioctl */
#define HOSTAPD_IOCTL_COMMAND_STRLEN_MAX   8192

hdd_adapter_t* hdd_wlan_create_ap_dev( hdd_context_t *pHddCtx, tSirMacAddr macAddr, tANI_U8 *name);

VOS_STATUS hdd_register_hostapd(hdd_adapter_t *pAdapter, tANI_U8 rtnl_held);

VOS_STATUS hdd_unregister_hostapd(hdd_adapter_t *pAdapter, tANI_U8 rtnl_held);

eCsrAuthType 
hdd_TranslateRSNToCsrAuthType( u_int8_t auth_suite[4]);

eCsrEncryptionType 
hdd_TranslateRSNToCsrEncryptionType(u_int8_t cipher_suite[4]);

eCsrEncryptionType 
hdd_TranslateRSNToCsrEncryptionType(u_int8_t cipher_suite[4]);

eCsrAuthType 
hdd_TranslateWPAToCsrAuthType(u_int8_t auth_suite[4]);

eCsrEncryptionType 
hdd_TranslateWPAToCsrEncryptionType(u_int8_t cipher_suite[4]);

VOS_STATUS hdd_softap_sta_deauth(hdd_adapter_t*, struct tagCsrDelStaParams*);
void hdd_softap_sta_disassoc(hdd_adapter_t*,v_U8_t*);
void hdd_softap_tkip_mic_fail_counter_measure(hdd_adapter_t*,v_BOOL_t);
int hdd_softap_unpackIE( tHalHandle halHandle,
                eCsrEncryptionType *pEncryptType,
                eCsrEncryptionType *mcEncryptType,
                eCsrAuthType *pAuthType,
                v_BOOL_t *pMFPCapable,
                v_BOOL_t *pMFPRequired,
                u_int16_t gen_ie_len,
                u_int8_t *gen_ie );

VOS_STATUS hdd_hostapd_SAPEventCB( tpSap_Event pSapEvent, v_PVOID_t usrDataForCallback);
VOS_STATUS hdd_init_ap_mode( hdd_adapter_t *pAdapter );
void hdd_set_ap_ops( struct net_device *pWlanHostapdDev );
int hdd_hostapd_stop (struct net_device *dev);
void hdd_restart_softap (hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter);
#ifdef FEATURE_WLAN_CH_AVOID
void hdd_hostapd_ch_avoid_cb(void *pAdapter, void *indParam);
#endif /* FEATURE_WLAN_CH_AVOID */
int hdd_del_all_sta(hdd_adapter_t *pAdapter);
#endif    // end #if !defined( WLAN_HDD_HOSTAPD_H )
