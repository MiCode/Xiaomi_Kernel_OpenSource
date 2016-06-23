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

#ifndef _QC_SAP_IOCTL_H_
#define _QC_SAP_IOCTL_H_

/*
 * QCSAP ioctls.
 */

/*
 * Max size of optional information elements.  We artificially
 * constrain this; it's limited only by the max frame size (and
 * the max parameter size of the wireless extensions).
 */
#define QCSAP_MAX_OPT_IE        256
#define QCSAP_MAX_WSC_IE        256
#define QCSAP_MAX_GET_STA_INFO  512

typedef struct sSSID
{
    u_int8_t       length;
    u_int8_t       ssId[32];
} tSSID;

typedef struct sSSIDInfo
{
   tSSID     ssid;   
   u_int8_t  ssidHidden;
}tSSIDInfo;

typedef enum {
    eQC_DOT11_MODE_ALL = 0,
    eQC_DOT11_MODE_ABG = 0x0001,    //11a/b/g only, no HT, no proprietary
    eQC_DOT11_MODE_11A = 0x0002,
    eQC_DOT11_MODE_11B = 0x0004,
    eQC_DOT11_MODE_11G = 0x0008,
    eQC_DOT11_MODE_11N = 0x0010,
    eQC_DOT11_MODE_11G_ONLY = 0x0020,
    eQC_DOT11_MODE_11N_ONLY = 0x0040,
    eQC_DOT11_MODE_11B_ONLY = 0x0080,
    eQC_DOT11_MODE_11A_ONLY = 0x0100,
    //This is for WIFI test. It is same as eWNIAPI_MAC_PROTOCOL_ALL except when it starts IBSS in 11B of 2.4GHz
    //It is for CSR internal use
    eQC_DOT11_MODE_AUTO = 0x0200,

} tQcPhyMode;

#define QCSAP_ADDR_LEN  6

typedef u_int8_t qcmacaddr[QCSAP_ADDR_LEN];

struct qc_mac_acl_entry {
    qcmacaddr addr;
    int vlan_id;
};

typedef enum {
    eQC_AUTH_TYPE_OPEN_SYSTEM,
    eQC_AUTH_TYPE_SHARED_KEY,
    eQC_AUTH_TYPE_AUTO_SWITCH
} eQcAuthType; 

typedef enum {
    eQC_WPS_BEACON_IE,
    eQC_WPS_PROBE_RSP_IE,
    eQC_WPS_ASSOC_RSP_IE
} eQCWPSType;


/*
 * Retrieve the WPA/RSN information element for an associated station.
 */
struct sQcSapreq_wpaie {
    u_int8_t    wpa_ie[QCSAP_MAX_OPT_IE];
    u_int8_t    wpa_macaddr[QCSAP_ADDR_LEN];
};

/*
 * Retrieve the WSC information element for an associated station.
 */
struct sQcSapreq_wscie {
    u_int8_t    wsc_macaddr[QCSAP_ADDR_LEN];
    u_int8_t    wsc_ie[QCSAP_MAX_WSC_IE];
};


/*
 * Retrieve the WPS PBC Probe Request IEs.
 */
typedef struct sQcSapreq_WPSPBCProbeReqIES {
    u_int8_t    macaddr[QCSAP_ADDR_LEN];
    u_int16_t   probeReqIELen;
    u_int8_t    probeReqIE[512]; 
} sQcSapreq_WPSPBCProbeReqIES_t ;

/*
 * Channel List Info
 */

typedef struct
{
    v_U8_t            num_channels;    
    v_U8_t            channels[WNI_CFG_VALID_CHANNEL_LIST_LEN];
}tChannelListInfo, *tpChannelListInfo;


#ifdef __linux__
/*
 * Wireless Extensions API, private ioctl interfaces.
 *
 * NB: Even-numbered ioctl numbers have set semantics and are privileged!
 *     (regardless of the incorrect comment in wireless.h!)
 */

#define QCSAP_IOCTL_SETPARAM          (SIOCIWFIRSTPRIV+0)
#define QCSAP_IOCTL_GETPARAM          (SIOCIWFIRSTPRIV+1)
#define QCSAP_IOCTL_COMMIT            (SIOCIWFIRSTPRIV+2)

#define QCSAP_IOCTL_GET_STAWPAIE      (SIOCIWFIRSTPRIV+4)
#define QCSAP_IOCTL_SETWPAIE          (SIOCIWFIRSTPRIV+5)
#define QCSAP_IOCTL_STOPBSS           (SIOCIWFIRSTPRIV+6)
#define QCSAP_IOCTL_VERSION           (SIOCIWFIRSTPRIV+7)
#define QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES       (SIOCIWFIRSTPRIV+8)
#define QCSAP_IOCTL_GET_CHANNEL       (SIOCIWFIRSTPRIV+9)
#define QCSAP_IOCTL_ASSOC_STA_MACADDR (SIOCIWFIRSTPRIV+10)
#define QCSAP_IOCTL_DISASSOC_STA      (SIOCIWFIRSTPRIV+11)
#define QCSAP_IOCTL_GET_STATS         (SIOCIWFIRSTPRIV+13)
#define QCSAP_IOCTL_CLR_STATS         (SIOCIWFIRSTPRIV+14)

#define QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE (SIOCIWFIRSTPRIV+15)
#define WE_SET_WLAN_DBG 1
#define QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE (SIOCIWFIRSTPRIV+16)
#define WE_LOG_DUMP_CMD 1
#define QCSAP_IOCTL_SET_CHANNEL_RANGE (SIOCIWFIRSTPRIV+17)

#define WE_P2P_NOA_CMD  2
//IOCTL to configure MCC params
#define WE_MCC_CONFIG_CREDENTIAL 3
#define WE_MCC_CONFIG_PARAMS  4

#define QCSAP_IOCTL_MODIFY_ACL          (SIOCIWFIRSTPRIV+18)
#define QCSAP_IOCTL_GET_CHANNEL_LIST    (SIOCIWFIRSTPRIV+19)
#define QCSAP_IOCTL_SET_TX_POWER        (SIOCIWFIRSTPRIV+20) 
#define QCSAP_IOCTL_GET_STA_INFO        (SIOCIWFIRSTPRIV+21)
#define QCSAP_IOCTL_SET_MAX_TX_POWER    (SIOCIWFIRSTPRIV+22)
#define QCSAP_IOCTL_DATAPATH_SNAP_SHOT  (SIOCIWFIRSTPRIV+23)
#define QCSAP_IOCTL_SET_TRAFFIC_MONITOR (SIOCIWFIRSTPRIV+24)
#define QCSAP_IOCTL_AP_STATS            (SIOCIWFIRSTPRIV+25) // get routines should be odd numbered

#define MAX_VAR_ARGS         7
#define QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED (SIOCIWFIRSTPRIV + 31)

enum { 
    QCSAP_PARAM_MAX_ASSOC = 1,
    QCSAP_PARAM_GET_WLAN_DBG = 4,
    QCSAP_PARAM_CLR_ACL = 6,
    QCSAP_PARAM_ACL_MODE = 7,
    QCSAP_PARAM_HIDE_SSID = 8,
    QCSAP_PARAM_AUTO_CHANNEL = 9,
    QCSAP_PARAM_SET_MC_RATE = 10,
    QCSAP_PARAM_SET_AUTO_CHANNEL = 11,
};

int iw_softap_get_channel_list(struct net_device *dev, 
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra);

#endif /* __linux__ */

#endif /*_QC_SAP_IOCTL_H_*/
