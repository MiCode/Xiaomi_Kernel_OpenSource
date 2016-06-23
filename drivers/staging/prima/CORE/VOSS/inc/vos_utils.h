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


#if !defined( __VOS_UTILS_H )
#define __VOS_UTILS_H
 
/**=========================================================================
  
  \file  vos_utils.h
  
  \brief virtual Operating System Services (vOSS) utility APIs
               
   Various utility functions
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>
//#include <Wincrypt.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#define VOS_DIGEST_SHA1_SIZE    20
#define VOS_DIGEST_MD5_SIZE     16
#define VOS_BAND_2GHZ          1
#define VOS_BAND_5GHZ          2

#define VOS_24_GHZ_CHANNEL_14  14
/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

VOS_STATUS vos_crypto_init( v_U32_t *phCryptProv );

VOS_STATUS vos_crypto_deinit( v_U32_t hCryptProv );



/**
 * vos_rand_get_bytes

 * FUNCTION:
 * Returns cryptographically secure pseudo-random bytes.
 *
 *
 * @param pbBuf - the caller allocated location where the bytes should be copied
 * @param numBytes the number of bytes that should be generated and
 * copied
 * 
 * @return VOS_STATUS_SUCCSS if the operation succeeds
*/
VOS_STATUS vos_rand_get_bytes( v_U32_t handle, v_U8_t *pbBuf, v_U32_t numBytes );


/**
 * vos_sha1_hmac_str
 *
 * FUNCTION:
 * Generate the HMAC-SHA1 of a string given a key.
 *
 * LOGIC:
 * Standard HMAC processing from RFC 2104. The code is provided in the
 * appendix of the RFC.
 *
 * ASSUMPTIONS:
 * The RFC is correct.
 *
 * @param text text to be hashed
 * @param textLen length of text
 * @param key key to use for HMAC
 * @param keyLen length of key
 * @param digest holds resultant SHA1 HMAC (20B)
 *
 * @return VOS_STATUS_SUCCSS if the operation succeeds
 *
 */
VOS_STATUS vos_sha1_hmac_str(v_U32_t cryptHandle, /* Handle */
           v_U8_t *text, /* pointer to data stream */
           v_U32_t textLen, /* length of data stream */
           v_U8_t *key, /* pointer to authentication key */
           v_U32_t keyLen, /* length of authentication key */
           v_U8_t digest[VOS_DIGEST_SHA1_SIZE]);/* caller digest to be filled in */

/**
 * vos_md5_hmac_str
 *
 * FUNCTION:
 * Generate the HMAC-MD5 of a string given a key.
 *
 * LOGIC:
 * Standard HMAC processing from RFC 2104. The code is provided in the
 * appendix of the RFC.
 *
 * ASSUMPTIONS:
 * The RFC is correct.
 *
 * @param text text to be hashed
 * @param textLen length of text
 * @param key key to use for HMAC
 * @param keyLen length of key
 * @param digest holds resultant MD5 HMAC (16B)
 *
 * @return VOS_STATUS_SUCCSS if the operation succeeds
 *
 */
VOS_STATUS vos_md5_hmac_str(v_U32_t cryptHandle, /* Handle */
           v_U8_t *text, /* pointer to data stream */
           v_U32_t textLen, /* length of data stream */
           v_U8_t *key, /* pointer to authentication key */
           v_U32_t keyLen, /* length of authentication key */
           v_U8_t digest[VOS_DIGEST_MD5_SIZE]);/* caller digest to be filled in */



VOS_STATUS vos_encrypt_AES(v_U32_t cryptHandle, /* Handle */
                           v_U8_t *pText, /* pointer to data stream */
                           v_U8_t *Encrypted,
                           v_U8_t *pKey); /* pointer to authentication key */


VOS_STATUS vos_decrypt_AES(v_U32_t cryptHandle, /* Handle */
                           v_U8_t *pText, /* pointer to data stream */
                           v_U8_t *pDecrypted,
                           v_U8_t *pKey); /* pointer to authentication key */

v_U8_t vos_chan_to_band(v_U32_t chan);

#ifdef DEBUG_ROAM_DELAY
#define ROAM_DELAY_TABLE_SIZE   30

enum e_roaming_event
{
    e_HDD_DISABLE_TX_QUEUE = 0,
    e_SME_PREAUTH_REASSOC_START,
    e_SME_PREAUTH_CALLBACK_HIT,
    e_SME_ISSUE_REASSOC_REQ,
    e_LIM_SEND_REASSOC_REQ,
    e_HDD_SEND_REASSOC_RSP,
    e_SME_DISASSOC_ISSUE,
    e_SME_DISASSOC_COMPLETE,
    e_LIM_ADD_BS_REQ,
    e_LIM_ADD_BS_RSP,
    e_HDD_ENABLE_TX_QUEUE,
    e_HDD_SET_PTK_REQ,
    e_HDD_SET_GTK_REQ,
    e_HDD_SET_PTK_RSP,
    e_HDD_SET_GTK_RSP,
    e_HDD_FIRST_XMIT_TIME,
    e_DXE_FIRST_XMIT_TIME,
    e_SME_VO_ADDTS_REQ,
    e_SME_VO_ADDTS_RSP,
    e_SME_VI_ADDTS_REQ,
    e_SME_VI_ADDTS_RSP,
    e_CACHE_ROAM_DELAY_DATA,
    e_CACHE_ROAM_PEER_MAC,
    e_TL_FIRST_XMIT_TIME,
    e_HDD_RX_PKT_CBK_TIME,
    e_DXE_RX_PKT_TIME,

    e_ROAM_EVENT_MAX
};

typedef enum
{
    eVOS_AUTH_TYPE_NONE,    //never used
    // MAC layer authentication types
    eVOS_AUTH_TYPE_OPEN_SYSTEM,
    eVOS_AUTH_TYPE_SHARED_KEY,
    eVOS_AUTH_TYPE_AUTOSWITCH,

    // Upper layer authentication types
    eVOS_AUTH_TYPE_WPA,
    eVOS_AUTH_TYPE_WPA_PSK,
    eVOS_AUTH_TYPE_WPA_NONE,

    eVOS_AUTH_TYPE_RSN,
    eVOS_AUTH_TYPE_RSN_PSK,
#if defined WLAN_FEATURE_VOWIFI_11R
    eVOS_AUTH_TYPE_FT_RSN,
    eVOS_AUTH_TYPE_FT_RSN_PSK,
#endif
#ifdef FEATURE_WLAN_WAPI
    eVOS_AUTH_TYPE_WAPI_WAI_CERTIFICATE,
    eVOS_AUTH_TYPE_WAPI_WAI_PSK,
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_CCX
    eVOS_AUTH_TYPE_CCKM_WPA,
    eVOS_AUTH_TYPE_CCKM_RSN,
#endif /* FEATURE_WLAN_CCX */
#ifdef WLAN_FEATURE_11W
    eVOS_AUTH_TYPE_RSN_PSK_SHA256,
#endif
    eVOS_NUM_OF_SUPPORT_AUTH_TYPE,
    eVOS_AUTH_TYPE_FAILED = 0xff,
    eVOS_AUTH_TYPE_UNKNOWN = eVOS_AUTH_TYPE_FAILED,

}eVosAuthType;


typedef struct sRoamDelayMetaInfo
{
    v_BOOL_t           log_tl;
    v_U8_t             hdd_monitor_tx;//monitor the tx @ hdd basically (Eapol , First Tx Data Frame )
    v_U8_t             hdd_monitor_rx;//monitor the rx @ hdd basically (Eapol )
    v_U8_t             dxe_monitor_tx;//monitor the tx @ dxe basically (Eapol , First Tx Data Frame )
    v_U8_t             dxe_monitor_rx;//monitor the rx @ dxe basically (Eapol )
    v_BOOL_t           log_dxe_tx_isr;
    v_U8_t             peer_mac_addr[6];
    v_ULONG_t          hdd_first_xmit_time;
    v_ULONG_t          preauth_reassoc_start_time;
    v_ULONG_t          preauth_cb_time;
    v_ULONG_t          disable_tx_queues_time;
    v_ULONG_t          lim_add_bss_req_time;
    v_ULONG_t          issue_reassoc_req_time;
    v_ULONG_t          hdd_sendassoc_rsp_time;
    v_ULONG_t          enable_tx_queues_reassoc_time;
    v_ULONG_t          set_ptk_roam_key_time;
    v_ULONG_t          set_gtk_roam_key_time;
    v_ULONG_t          complete_ptk_roam_key_time;
    v_ULONG_t          complete_gtk_roam_key_time;
    v_ULONG_t          dxe_first_tx_time;
    v_ULONG_t          send_reassoc_req_time;
    v_ULONG_t          disassoc_comp_time;
    v_ULONG_t          disassoc_issue_time;
    v_ULONG_t          lim_add_bss_rsp_time;
    v_ULONG_t          tl_fetch_pkt_time;
    v_ULONG_t          dxe_tx_isr_time;
    v_ULONG_t          hdd_eapol_m1;
    v_ULONG_t          hdd_eapol_m2;
    v_ULONG_t          hdd_eapol_m3;
    v_ULONG_t          hdd_eapol_m4;
    v_ULONG_t          dxe_eapol_m1;
    v_ULONG_t          dxe_eapol_m2;
    v_ULONG_t          dxe_eapol_m3;
    v_ULONG_t          dxe_eapol_m4;
    v_ULONG_t          hdd_first_pkt_len;
    v_U8_t             hdd_first_pkt_data[50];
    v_ULONG_t          dxe_first_pkt_len;
    v_U8_t             dxe_first_pkt_data[75];
    v_ULONG_t          hdd_addts_vo_req_time;
    v_ULONG_t          hdd_addts_vo_rsp_time;
    v_ULONG_t          hdd_addts_vi_req_time;
    v_ULONG_t          hdd_addts_vi_rsp_time;
    eVosAuthType       hdd_auth_type;

} tRoamDelayMetaInfo, *tpRoamDelayMetaInfo;

extern  tRoamDelayMetaInfo gRoamDelayMetaInfo;
extern  tRoamDelayMetaInfo gRoamDelayTable[ROAM_DELAY_TABLE_SIZE];
extern  v_BOOL_t           gRoamDelayCurrentIndex;

void    vos_reset_roam_timer_log(void);
void    vos_dump_roam_time_log_service(void);
void    vos_record_roam_event(enum e_roaming_event, void *pBuff, v_ULONG_t buff_len);
#endif //#ifdef DEBUG_ROAM_DELAY
#endif // #if !defined __VOSS_UTILS_H
