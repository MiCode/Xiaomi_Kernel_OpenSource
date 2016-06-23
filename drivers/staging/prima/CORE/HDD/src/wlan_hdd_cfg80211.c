/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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




/**========================================================================

  \file  wlan_hdd_cfg80211.c

  \brief WLAN Host Device Driver implementation

  ========================================================================*/

/**=========================================================================

  EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who            what, where, why
  --------    ---            --------------------------------------------------------
 21/12/09     Ashwani        Created module.

 07/06/10     Kumar Deepak   Implemented cfg80211 callbacks for ANDROID
              Ganesh K
  ==========================================================================*/


#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <wlan_hdd_includes.h>
#include <net/arp.h>
#include <net/cfg80211.h>
#include <linux/wireless.h>
#include <wlan_hdd_wowl.h>
#include <aniGlobal.h>
#include "ccmApi.h"
#include "sirParams.h"
#include "dot11f.h"
#include "wlan_hdd_assoc.h"
#include "wlan_hdd_wext.h"
#include "sme_Api.h"
#include "wlan_hdd_p2p.h"
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_hostapd.h"
#include "sapInternal.h"
#include "wlan_hdd_softap_tx_rx.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_assoc.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_trace.h"
#include "vos_types.h"
#include "vos_trace.h"
#ifdef WLAN_BTAMP_FEATURE
#include "bap_hdd_misc.h"
#endif
#include <qc_sap_ioctl.h>
#ifdef FEATURE_WLAN_TDLS
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_wmm.h"
#include "wlan_qct_wda.h"
#endif
#include "wlan_nv.h"
#include "wlan_hdd_dev_pwr.h"

#define g_mode_rates_size (12)
#define a_mode_rates_size (8)
#define FREQ_BASE_80211G          (2407)
#define FREQ_BAND_DIFF_80211G     (5)
#define MAX_SCAN_SSID 9
#define MAX_PENDING_LOG 5
#define GET_IE_LEN_IN_BSS_DESC(lenInBss) ( lenInBss + sizeof(lenInBss) - \
        ((uintptr_t)OFFSET_OF( tSirBssDescription, ieFields)))

#define HDD2GHZCHAN(freq, chan, flag)   {     \
    .band =  IEEE80211_BAND_2GHZ, \
    .center_freq = (freq), \
    .hw_value = (chan),\
    .flags = (flag), \
    .max_antenna_gain = 0 ,\
    .max_power = 30, \
}

#define HDD5GHZCHAN(freq, chan, flag)   {     \
    .band =  IEEE80211_BAND_5GHZ, \
    .center_freq = (freq), \
    .hw_value = (chan),\
    .flags = (flag), \
    .max_antenna_gain = 0 ,\
    .max_power = 30, \
}

#define HDD_G_MODE_RATETAB(rate, rate_id, flag)\
{\
    .bitrate = rate, \
    .hw_value = rate_id, \
    .flags = flag, \
}

#ifndef WLAN_FEATURE_TDLS_DEBUG
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_INFO
#else
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_ERROR
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
#define WLAN_AKM_SUITE_FT_8021X         0x000FAC03
#define WLAN_AKM_SUITE_FT_PSK           0x000FAC04
#endif

#define HDD_CHANNEL_14 14
#define WLAN_HDD_MAX_FEATURE_SET   8

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
/*
 * Used to allocate the size of 4096 for the link layer stats.
 * The size of 4096 is considered assuming that all data per
 * respective event fit with in the limit.Please take a call
 * on the limit based on the data requirements on link layer
 * statistics.
 */
#define LL_STATS_EVENT_BUF_SIZE 4096
#endif
#ifdef WLAN_FEATURE_EXTSCAN
/*
 * Used to allocate the size of 4096 for the EXTScan NL data.
 * The size of 4096 is considered assuming that all data per
 * respective event fit with in the limit.Please take a call
 * on the limit based on the data requirements.
 */

#define EXTSCAN_EVENT_BUF_SIZE 4096
#define EXTSCAN_MAX_CACHED_RESULTS_PER_IND 32
#endif

/*EXT TDLS*/
/*
 * Used to allocate the size of 4096 for the TDLS.
 * The size of 4096 is considered assuming that all data per
 * respective event fit with in the limit.Please take a call
 * on the limit based on the data requirements on link layer
 * statistics.
 */
#define EXTTDLS_EVENT_BUF_SIZE 4096

static const u32 hdd_cipher_suites[] =
{
    WLAN_CIPHER_SUITE_WEP40,
    WLAN_CIPHER_SUITE_WEP104,
    WLAN_CIPHER_SUITE_TKIP,
#ifdef FEATURE_WLAN_ESE
#define WLAN_CIPHER_SUITE_KRK 0x004096ff /* use for KRK */
    WLAN_CIPHER_SUITE_KRK,
    WLAN_CIPHER_SUITE_CCMP,
#else
    WLAN_CIPHER_SUITE_CCMP,
#endif
#ifdef FEATURE_WLAN_WAPI
    WLAN_CIPHER_SUITE_SMS4,
#endif
#ifdef WLAN_FEATURE_11W
    WLAN_CIPHER_SUITE_AES_CMAC,
#endif
};

static inline int is_broadcast_ether_addr(const u8 *addr)
{
    return ((addr[0] == 0xff) && (addr[1] == 0xff) && (addr[2] == 0xff) &&
            (addr[3] == 0xff) && (addr[4] == 0xff) && (addr[5] == 0xff));
}

static struct ieee80211_channel hdd_channels_2_4_GHZ[] =
{
    HDD2GHZCHAN(2412, 1, 0) ,
    HDD2GHZCHAN(2417, 2, 0) ,
    HDD2GHZCHAN(2422, 3, 0) ,
    HDD2GHZCHAN(2427, 4, 0) ,
    HDD2GHZCHAN(2432, 5, 0) ,
    HDD2GHZCHAN(2437, 6, 0) ,
    HDD2GHZCHAN(2442, 7, 0) ,
    HDD2GHZCHAN(2447, 8, 0) ,
    HDD2GHZCHAN(2452, 9, 0) ,
    HDD2GHZCHAN(2457, 10, 0) ,
    HDD2GHZCHAN(2462, 11, 0) ,
    HDD2GHZCHAN(2467, 12, 0) ,
    HDD2GHZCHAN(2472, 13, 0) ,
    HDD2GHZCHAN(2484, 14, 0) ,
};

static struct ieee80211_channel hdd_social_channels_2_4_GHZ[] =
{
    HDD2GHZCHAN(2412, 1, 0) ,
    HDD2GHZCHAN(2437, 6, 0) ,
    HDD2GHZCHAN(2462, 11, 0) ,
};

static struct ieee80211_channel hdd_channels_5_GHZ[] =
{
    HDD5GHZCHAN(4920, 240, 0) ,
    HDD5GHZCHAN(4940, 244, 0) ,
    HDD5GHZCHAN(4960, 248, 0) ,
    HDD5GHZCHAN(4980, 252, 0) ,
    HDD5GHZCHAN(5040, 208, 0) ,
    HDD5GHZCHAN(5060, 212, 0) ,
    HDD5GHZCHAN(5080, 216, 0) ,
    HDD5GHZCHAN(5180, 36, 0) ,
    HDD5GHZCHAN(5200, 40, 0) ,
    HDD5GHZCHAN(5220, 44, 0) ,
    HDD5GHZCHAN(5240, 48, 0) ,
    HDD5GHZCHAN(5260, 52, 0) ,
    HDD5GHZCHAN(5280, 56, 0) ,
    HDD5GHZCHAN(5300, 60, 0) ,
    HDD5GHZCHAN(5320, 64, 0) ,
    HDD5GHZCHAN(5500,100, 0) ,
    HDD5GHZCHAN(5520,104, 0) ,
    HDD5GHZCHAN(5540,108, 0) ,
    HDD5GHZCHAN(5560,112, 0) ,
    HDD5GHZCHAN(5580,116, 0) ,
    HDD5GHZCHAN(5600,120, 0) ,
    HDD5GHZCHAN(5620,124, 0) ,
    HDD5GHZCHAN(5640,128, 0) ,
    HDD5GHZCHAN(5660,132, 0) ,
    HDD5GHZCHAN(5680,136, 0) ,
    HDD5GHZCHAN(5700,140, 0) ,
#ifdef FEATURE_WLAN_CH144
    HDD5GHZCHAN(5720,144, 0) ,
#endif /* FEATURE_WLAN_CH144 */
    HDD5GHZCHAN(5745,149, 0) ,
    HDD5GHZCHAN(5765,153, 0) ,
    HDD5GHZCHAN(5785,157, 0) ,
    HDD5GHZCHAN(5805,161, 0) ,
    HDD5GHZCHAN(5825,165, 0) ,
};

static struct ieee80211_rate g_mode_rates[] =
{
    HDD_G_MODE_RATETAB(10, 0x1, 0),
    HDD_G_MODE_RATETAB(20, 0x2, 0),
    HDD_G_MODE_RATETAB(55, 0x4, 0),
    HDD_G_MODE_RATETAB(110, 0x8, 0),
    HDD_G_MODE_RATETAB(60, 0x10, 0),
    HDD_G_MODE_RATETAB(90, 0x20, 0),
    HDD_G_MODE_RATETAB(120, 0x40, 0),
    HDD_G_MODE_RATETAB(180, 0x80, 0),
    HDD_G_MODE_RATETAB(240, 0x100, 0),
    HDD_G_MODE_RATETAB(360, 0x200, 0),
    HDD_G_MODE_RATETAB(480, 0x400, 0),
    HDD_G_MODE_RATETAB(540, 0x800, 0),
};

static struct ieee80211_rate a_mode_rates[] =
{
    HDD_G_MODE_RATETAB(60, 0x10, 0),
    HDD_G_MODE_RATETAB(90, 0x20, 0),
    HDD_G_MODE_RATETAB(120, 0x40, 0),
    HDD_G_MODE_RATETAB(180, 0x80, 0),
    HDD_G_MODE_RATETAB(240, 0x100, 0),
    HDD_G_MODE_RATETAB(360, 0x200, 0),
    HDD_G_MODE_RATETAB(480, 0x400, 0),
    HDD_G_MODE_RATETAB(540, 0x800, 0),
};

static struct ieee80211_supported_band wlan_hdd_band_2_4_GHZ =
{
    .channels = hdd_channels_2_4_GHZ,
    .n_channels = ARRAY_SIZE(hdd_channels_2_4_GHZ),
    .band       = IEEE80211_BAND_2GHZ,
    .bitrates = g_mode_rates,
    .n_bitrates = g_mode_rates_size,
    .ht_cap.ht_supported   = 1,
    .ht_cap.cap            =  IEEE80211_HT_CAP_SGI_20
                            | IEEE80211_HT_CAP_GRN_FLD
                            | IEEE80211_HT_CAP_DSSSCCK40
                            | IEEE80211_HT_CAP_LSIG_TXOP_PROT,
    .ht_cap.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,
    .ht_cap.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_16,
    .ht_cap.mcs.rx_mask    = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    .ht_cap.mcs.rx_highest = cpu_to_le16( 72 ),
    .ht_cap.mcs.tx_params  = IEEE80211_HT_MCS_TX_DEFINED,
};

static struct ieee80211_supported_band wlan_hdd_band_p2p_2_4_GHZ =
{
    .channels = hdd_social_channels_2_4_GHZ,
    .n_channels = ARRAY_SIZE(hdd_social_channels_2_4_GHZ),
    .band       = IEEE80211_BAND_2GHZ,
    .bitrates = g_mode_rates,
    .n_bitrates = g_mode_rates_size,
    .ht_cap.ht_supported   = 1,
    .ht_cap.cap            =  IEEE80211_HT_CAP_SGI_20
                            | IEEE80211_HT_CAP_GRN_FLD
                            | IEEE80211_HT_CAP_DSSSCCK40
                            | IEEE80211_HT_CAP_LSIG_TXOP_PROT,
    .ht_cap.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,
    .ht_cap.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_16,
    .ht_cap.mcs.rx_mask    = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    .ht_cap.mcs.rx_highest = cpu_to_le16( 72 ),
    .ht_cap.mcs.tx_params  = IEEE80211_HT_MCS_TX_DEFINED,
};

static struct ieee80211_supported_band wlan_hdd_band_5_GHZ =
{
    .channels = hdd_channels_5_GHZ,
    .n_channels = ARRAY_SIZE(hdd_channels_5_GHZ),
    .band     = IEEE80211_BAND_5GHZ,
    .bitrates = a_mode_rates,
    .n_bitrates = a_mode_rates_size,
    .ht_cap.ht_supported   = 1,
    .ht_cap.cap            =  IEEE80211_HT_CAP_SGI_20
                            | IEEE80211_HT_CAP_GRN_FLD
                            | IEEE80211_HT_CAP_DSSSCCK40
                            | IEEE80211_HT_CAP_LSIG_TXOP_PROT
                            | IEEE80211_HT_CAP_SGI_40
                            | IEEE80211_HT_CAP_SUP_WIDTH_20_40,
    .ht_cap.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,
    .ht_cap.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_16,
    .ht_cap.mcs.rx_mask    = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    .ht_cap.mcs.rx_highest = cpu_to_le16( 72 ),
    .ht_cap.mcs.tx_params  = IEEE80211_HT_MCS_TX_DEFINED,
};

/* This structure contain information what kind of frame are expected in
     TX/RX direction for each kind of interface */
static const struct ieee80211_txrx_stypes
wlan_hdd_txrx_stypes[NUM_NL80211_IFTYPES] = {
    [NL80211_IFTYPE_STATION] = {
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ACTION) |
            BIT(SIR_MAC_MGMT_PROBE_REQ),
    },
    [NL80211_IFTYPE_AP] = {
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
            BIT(SIR_MAC_MGMT_REASSOC_REQ) |
            BIT(SIR_MAC_MGMT_PROBE_REQ) |
            BIT(SIR_MAC_MGMT_DISASSOC) |
            BIT(SIR_MAC_MGMT_AUTH) |
            BIT(SIR_MAC_MGMT_DEAUTH) |
            BIT(SIR_MAC_MGMT_ACTION),
    },
    [NL80211_IFTYPE_ADHOC] = {
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
            BIT(SIR_MAC_MGMT_REASSOC_REQ) |
            BIT(SIR_MAC_MGMT_PROBE_REQ) |
            BIT(SIR_MAC_MGMT_DISASSOC) |
            BIT(SIR_MAC_MGMT_AUTH) |
            BIT(SIR_MAC_MGMT_DEAUTH) |
            BIT(SIR_MAC_MGMT_ACTION),
    },
    [NL80211_IFTYPE_P2P_CLIENT] = {
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ACTION) |
            BIT(SIR_MAC_MGMT_PROBE_REQ),
    },
    [NL80211_IFTYPE_P2P_GO] = {
        /* This is also same as for SoftAP */
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
            BIT(SIR_MAC_MGMT_REASSOC_REQ) |
            BIT(SIR_MAC_MGMT_PROBE_REQ) |
            BIT(SIR_MAC_MGMT_DISASSOC) |
            BIT(SIR_MAC_MGMT_AUTH) |
            BIT(SIR_MAC_MGMT_DEAUTH) |
            BIT(SIR_MAC_MGMT_ACTION),
    },
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
static const struct ieee80211_iface_limit
wlan_hdd_iface_limit[] = {
    {
        /* max = 3 ; Our driver create two interfaces during driver init
         * wlan0 and p2p0 interfaces. p2p0 is considered as station
         * interface until a group is formed. In JB architecture, once the
         * group is formed, interface type of p2p0 is changed to P2P GO or
         * Client.
         * When supplicant remove the group, it first issue a set interface
         * cmd to change the mode back to Station. In JB this works fine as
         * we advertize two station type interface during driver init.
         * Some vendors create separate interface for P2P GO/Client,
         * after group formation(Third one). But while group remove
         * supplicant first tries to change the mode(3rd interface) to STATION
         * But as we advertized only two sta type interfaces nl80211 was
         * returning error for the third one which was leading to failure in
         * delete interface. Ideally while removing the group, supplicant
         * should not try to change the 3rd interface mode to Station type.
         * Till we get a fix in wpa_supplicant, we advertize max STA
         * interface type to 3
         */
        .max = 3,
        .types = BIT(NL80211_IFTYPE_STATION),
    },
    {
        .max = 1,
        .types = BIT(NL80211_IFTYPE_ADHOC) | BIT(NL80211_IFTYPE_AP),
    },
    {
        .max = 1,
        .types = BIT(NL80211_IFTYPE_P2P_GO) |
                 BIT(NL80211_IFTYPE_P2P_CLIENT),
    },
};

/* By default, only single channel concurrency is allowed */
static struct ieee80211_iface_combination
wlan_hdd_iface_combination = {
        .limits = wlan_hdd_iface_limit,
        .num_different_channels = 1,
        /*
         * max = WLAN_MAX_INTERFACES ; JellyBean architecture creates wlan0
         * and p2p0 interfaces during driver init
         * Some vendors create separate interface for P2P operations.
         * wlan0: STA interface
         * p2p0: P2P Device interface, action frames goes
         * through this interface.
         * p2p-xx: P2P interface, After GO negotiation this interface is
         * created for p2p operations(GO/CLIENT interface).
         */
        .max_interfaces = WLAN_MAX_INTERFACES,
        .n_limits = ARRAY_SIZE(wlan_hdd_iface_limit),
        .beacon_int_infra_match = false,
};
#endif

static struct cfg80211_ops wlan_hdd_cfg80211_ops;

/* Data rate 100KBPS based on IE Index */
struct index_data_rate_type
{
   v_U8_t   beacon_rate_index;
   v_U16_t  supported_rate[4];
};

/* 11B, 11G Rate table include Basic rate and Extended rate
   The IDX field is the rate index
   The HI field is the rate when RSSI is strong or being ignored
    (in this case we report actual rate)
   The MID field is the rate when RSSI is moderate
    (in this case we cap 11b rates at 5.5 and 11g rates at 24)
   The LO field is the rate when RSSI is low
    (in this case we don't report rates, actual current rate used)
 */
static const struct
{
   v_U8_t   beacon_rate_index;
   v_U16_t  supported_rate[4];
} supported_data_rate[] =
{
/* IDX     HI  HM  LM LO (RSSI-based index */
   {2,   { 10,  10, 10, 0}},
   {4,   { 20,  20, 10, 0}},
   {11,  { 55,  20, 10, 0}},
   {12,  { 60,  55, 20, 0}},
   {18,  { 90,  55, 20, 0}},
   {22,  {110,  55, 20, 0}},
   {24,  {120,  90, 60, 0}},
   {36,  {180, 120, 60, 0}},
   {44,  {220, 180, 60, 0}},
   {48,  {240, 180, 90, 0}},
   {66,  {330, 180, 90, 0}},
   {72,  {360, 240, 90, 0}},
   {96,  {480, 240, 120, 0}},
   {108, {540, 240, 120, 0}}
};

/* MCS Based rate table */
static struct index_data_rate_type supported_mcs_rate[] =
{
/* MCS  L20   L40   S20  S40 */
   {0,  {65,  135,  72,  150}},
   {1,  {130, 270,  144, 300}},
   {2,  {195, 405,  217, 450}},
   {3,  {260, 540,  289, 600}},
   {4,  {390, 810,  433, 900}},
   {5,  {520, 1080, 578, 1200}},
   {6,  {585, 1215, 650, 1350}},
   {7,  {650, 1350, 722, 1500}}
};

#ifdef WLAN_FEATURE_11AC

#define DATA_RATE_11AC_MCS_MASK    0x03

struct index_vht_data_rate_type
{
   v_U8_t   beacon_rate_index;
   v_U16_t  supported_VHT80_rate[2];
   v_U16_t  supported_VHT40_rate[2];
   v_U16_t  supported_VHT20_rate[2];
};

typedef enum
{
   DATA_RATE_11AC_MAX_MCS_7,
   DATA_RATE_11AC_MAX_MCS_8,
   DATA_RATE_11AC_MAX_MCS_9,
   DATA_RATE_11AC_MAX_MCS_NA
} eDataRate11ACMaxMcs;

/* SSID broadcast  type */
typedef enum eSSIDBcastType
{
  eBCAST_UNKNOWN      = 0,
  eBCAST_NORMAL       = 1,
  eBCAST_HIDDEN       = 2,
} tSSIDBcastType;

/* MCS Based VHT rate table */
static struct index_vht_data_rate_type supported_vht_mcs_rate[] =
{
/* MCS  L80    S80     L40   S40    L20   S40*/
   {0,  {293,  325},  {135,  150},  {65,   72}},
   {1,  {585,  650},  {270,  300},  {130,  144}},
   {2,  {878,  975},  {405,  450},  {195,  217}},
   {3,  {1170, 1300}, {540,  600},  {260,  289}},
   {4,  {1755, 1950}, {810,  900},  {390,  433}},
   {5,  {2340, 2600}, {1080, 1200}, {520,  578}},
   {6,  {2633, 2925}, {1215, 1350}, {585,  650}},
   {7,  {2925, 3250}, {1350, 1500}, {650,  722}},
   {8,  {3510, 3900}, {1620, 1800}, {780,  867}},
   {9,  {3900, 4333}, {1800, 2000}, {780,  867}}
};
#endif /* WLAN_FEATURE_11AC */

/*array index points to MCS and array value points respective rssi*/
static int rssiMcsTbl[][10] =
{
/*MCS 0   1     2   3    4    5    6    7    8    9*/
   {-82, -79, -77, -74, -70, -66, -65, -64, -59, -57}, //20
   {-79, -76, -74, -71, -67, -63, -62, -61, -56, -54}, //40
   {-76, -73, -71, -68, -64, -60, -59, -58, -53, -51}  //80
};

extern struct net_device_ops net_ops_struct;
#ifdef FEATURE_WLAN_SCAN_PNO
static eHalStatus wlan_hdd_is_pno_allowed(hdd_adapter_t *pAdapter);
#endif

#ifdef WLAN_NL80211_TESTMODE
enum wlan_hdd_tm_attr
{
    WLAN_HDD_TM_ATTR_INVALID = 0,
    WLAN_HDD_TM_ATTR_CMD     = 1,
    WLAN_HDD_TM_ATTR_DATA    = 2,
    WLAN_HDD_TM_ATTR_TYPE    = 3,
    /* keep last */
    WLAN_HDD_TM_ATTR_AFTER_LAST,
    WLAN_HDD_TM_ATTR_MAX       = WLAN_HDD_TM_ATTR_AFTER_LAST - 1,
};

enum wlan_hdd_tm_cmd
{
    WLAN_HDD_TM_CMD_WLAN_HB    = 1,
};

#define WLAN_HDD_TM_DATA_MAX_LEN    5000

static const struct nla_policy wlan_hdd_tm_policy[WLAN_HDD_TM_ATTR_MAX + 1] =
{
    [WLAN_HDD_TM_ATTR_CMD]        = { .type = NLA_U32 },
    [WLAN_HDD_TM_ATTR_DATA]       = { .type = NLA_BINARY,
                                    .len = WLAN_HDD_TM_DATA_MAX_LEN },
};
#endif /* WLAN_NL80211_TESTMODE */

#ifdef FEATURE_WLAN_CH_AVOID
/*
 * FUNCTION: wlan_hdd_send_avoid_freq_event
 * This is called when wlan driver needs to send vendor specific
 * avoid frequency range event to userspace
 */
int wlan_hdd_send_avoid_freq_event(hdd_context_t *pHddCtx,
                                   tHddAvoidFreqList *pAvoidFreqList)
{
    struct sk_buff *vendor_event;

    ENTER();

    if (!pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is null", __func__);
        return -1;
    }

    if (!pAvoidFreqList)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: pAvoidFreqList is null", __func__);
        return -1;
    }

    vendor_event = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                       sizeof(tHddAvoidFreqList),
                       QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_INDEX,
                       GFP_KERNEL);
    if (!vendor_event)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: cfg80211_vendor_event_alloc failed", __func__);
        return -1;
    }

    memcpy(skb_put(vendor_event, sizeof(tHddAvoidFreqList)),
                   (void *)pAvoidFreqList, sizeof(tHddAvoidFreqList));

    cfg80211_vendor_event(vendor_event, GFP_KERNEL);

    EXIT();
    return 0;
}
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

static v_BOOL_t put_wifi_rate_stat( tpSirWifiRateStat stats,
                                struct sk_buff *vendor_event)
{
    if (nla_put_u8(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_PREAMBLE,
                stats->rate.preamble)  ||
        nla_put_u8(vendor_event,
            QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_NSS,
            stats->rate.nss)       ||
        nla_put_u8(vendor_event,
            QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BW,
            stats->rate.bw)        ||
        nla_put_u8(vendor_event,
            QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MCS_INDEX,
            stats->rate.rateMcsIdx) ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BIT_RATE,
            stats->rate.bitrate )   ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_TX_MPDU,
            stats->txMpdu )    ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RX_MPDU,
                stats->rxMpdu )     ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MPDU_LOST,
                stats->mpduLost )  ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES,
                stats->retries)     ||
        nla_put_u32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_SHORT,
                stats->retriesShort )   ||
        nla_put_u32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_LONG,
                stats->retriesLong))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("QCA_WLAN_VENDOR_ATTR put fail"));
        return FALSE;
    }
    return TRUE;
}

static v_BOOL_t put_wifi_peer_info( tpSirWifiPeerInfo stats,
                               struct sk_buff *vendor_event)
{
    u32 i = 0;
    struct nlattr *rateInfo;
    if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_TYPE,
                                     stats->type) ||
        nla_put(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS,
                VOS_MAC_ADDR_SIZE, &stats->peerMacAddress[0]) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_CAPABILITIES,
                    stats->capabilities) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES,
                    stats->numRate))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("QCA_WLAN_VENDOR_ATTR put fail"));
        goto error;
    }

    rateInfo = nla_nest_start(vendor_event,
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO);
    for (i = 0; i < stats->numRate; i++)
    {
        struct nlattr *rates;
        tpSirWifiRateStat pRateStats = (tpSirWifiRateStat )((uint8 *)
                                            stats->rateStats +
                                       (i * sizeof(tSirWifiRateStat)));
        rates = nla_nest_start(vendor_event, i);

        if (FALSE == put_wifi_rate_stat(pRateStats, vendor_event))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("QCA_WLAN_VENDOR_ATTR put fail"));
            return FALSE;
        }
        nla_nest_end(vendor_event, rates);
    }
    nla_nest_end(vendor_event, rateInfo);

    return TRUE;
error:
    return FALSE;
}

static v_BOOL_t put_wifi_wmm_ac_stat( tpSirWifiWmmAcStat stats,
                                  struct sk_buff *vendor_event)
{
    if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_AC,
                    stats->ac ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MPDU,
                    stats->txMpdu ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MPDU,
                    stats->rxMpdu ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MCAST,
                    stats->txMcast ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MCAST,
                    stats->rxMcast ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_AMPDU,
                    stats->rxAmpdu ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_AMPDU,
                    stats->txAmpdu ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_MPDU_LOST,
                    stats->mpduLost )||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES,
                    stats->retries ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_SHORT,
                    stats->retriesShort ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_LONG,
                    stats->retriesLong ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN,
                    stats->contentionTimeMin ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX,
                    stats->contentionTimeMax ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG,
                    stats->contentionTimeAvg ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES,
                    stats->contentionNumSamples ))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("QCA_WLAN_VENDOR_ATTR put fail") );
        return FALSE;
    }
    return TRUE;
}

static v_BOOL_t put_wifi_interface_info(tpSirWifiInterfaceInfo stats,
                                    struct sk_buff *vendor_event)
{
    if (nla_put_s32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MODE, stats->mode ) ||
            nla_put(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR,
                    VOS_MAC_ADDR_SIZE, stats->macAddr) ||
            nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_STATE,
                        stats->state ) ||
            nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_ROAMING,
                        stats->roaming ) ||
            nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_CAPABILITIES,
                        stats->capabilities ) ||
            nla_put(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID,
                    strlen(stats->ssid), stats->ssid) ||
            nla_put(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID,
                    WNI_CFG_BSSID_LEN, stats->bssid) ||
            nla_put(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR,
                    WNI_CFG_COUNTRY_CODE_LEN, stats->apCountryStr) ||
            nla_put(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR,
                    WNI_CFG_COUNTRY_CODE_LEN, stats->countryStr)
      )
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("QCA_WLAN_VENDOR_ATTR put fail") );
        return FALSE;
    }
    return TRUE;
}

static v_BOOL_t put_wifi_iface_stats(hdd_adapter_t *pAdapter,
                            tpSirWifiIfaceStat pWifiIfaceStat,
                                 struct sk_buff *vendor_event)
{
    int i = 0;
    struct nlattr *wmmInfo;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    WLANTL_InterfaceStatsType *pWifiIfaceStatTL = NULL;
    tSirWifiWmmAcStat accessclassStats;

    if (FALSE == put_wifi_interface_info(
                                &pWifiIfaceStat->info,
                                vendor_event))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("QCA_WLAN_VENDOR_ATTR put fail") );
        return FALSE;

    }
    pWifiIfaceStatTL = (WLANTL_InterfaceStatsType *)
                             vos_mem_malloc(sizeof(WLANTL_InterfaceStatsType));
    if (NULL == pWifiIfaceStatTL)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("vos_mem_malloc failed"));
        return FALSE;
    }

    accessclassStats = pWifiIfaceStat->AccessclassStats[WIFI_AC_BK];
    pWifiIfaceStat->AccessclassStats[WIFI_AC_BK] =
        pWifiIfaceStat->AccessclassStats[WIFI_AC_BE];
    pWifiIfaceStat->AccessclassStats[WIFI_AC_BE] = accessclassStats;

    accessclassStats.ac = pWifiIfaceStat->AccessclassStats[WIFI_AC_BK].ac;
    pWifiIfaceStat->AccessclassStats[WIFI_AC_BK].ac =
        pWifiIfaceStat->AccessclassStats[WIFI_AC_BE].ac;
    pWifiIfaceStat->AccessclassStats[WIFI_AC_BE].ac = accessclassStats.ac;

    if ( pWifiIfaceStat->info.state == WIFI_ASSOCIATED)
    {
        if (VOS_STATUS_SUCCESS ==
         WLANTL_CollectInterfaceStats((WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                    pHddStaCtx->conn_info.staId[0], pWifiIfaceStatTL))
        {
            /* mgmtRx, MgmtActionRx, rxMcast, rxMpdu, rxAmpdu, rssiData are
             * obtained from TL structure
             */

            pWifiIfaceStat->mgmtRx = pWifiIfaceStat->beaconRx +
                pWifiIfaceStatTL->mgmtRx;
            pWifiIfaceStat->rssiData = pWifiIfaceStatTL->rssiData;

            pWifiIfaceStat->AccessclassStats[WIFI_AC_VO].rxMcast
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_VO].rxMcast;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_VI].rxMcast
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_VI].rxMcast;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_BE].rxMcast
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_BE].rxMcast;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_BK].rxMcast
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_BK].rxMcast;

            pWifiIfaceStat->AccessclassStats[WIFI_AC_VO].rxMpdu
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_VO].rxMpdu;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_VI].rxMpdu
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_VI].rxMpdu;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_BE].rxMpdu
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_BE].rxMpdu;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_BK].rxMpdu
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_BK].rxMpdu;

            pWifiIfaceStat->AccessclassStats[WIFI_AC_VO].rxAmpdu
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_VO].rxAmpdu;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_VI].rxAmpdu
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_VI].rxAmpdu;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_BE].rxAmpdu
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_BE].rxAmpdu;
            pWifiIfaceStat->AccessclassStats[WIFI_AC_BK].rxAmpdu
                = pWifiIfaceStatTL->accessCategoryStats[WLANTL_AC_BK].rxAmpdu;
        }
        else
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("Error in getting stats from TL"));
        }

        pWifiIfaceStat->AccessclassStats[WIFI_AC_VO].txMcast =
            pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_VO];
        pWifiIfaceStat->AccessclassStats[WIFI_AC_VI].txMcast =
            pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_VI];
        pWifiIfaceStat->AccessclassStats[WIFI_AC_BE].txMcast =
            pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_BE];
        pWifiIfaceStat->AccessclassStats[WIFI_AC_BK].txMcast =
            pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_BK];
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Interface not Associated"));
    }



    if (nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX,
                    pWifiIfaceStat->beaconRx) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX,
                    pWifiIfaceStat->mgmtRx) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX,
                    pWifiIfaceStat->mgmtActionRx) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX,
                    pWifiIfaceStat->mgmtActionTx) ||
        nla_put_s32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT,
                    pWifiIfaceStat->rssiMgmt) ||
        nla_put_s32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA,
                    pWifiIfaceStat->rssiData) ||
        nla_put_s32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK,
                    pWifiIfaceStat->rssiAck))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("QCA_WLAN_VENDOR_ATTR put fail"));
        vos_mem_free(pWifiIfaceStatTL);
        return FALSE;
    }

    wmmInfo = nla_nest_start(vendor_event,
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO);
    for (i = 0; i < WIFI_AC_MAX; i++)
    {
        struct nlattr *wmmStats;
        wmmStats = nla_nest_start(vendor_event, i);
        if (FALSE == put_wifi_wmm_ac_stat(
                                &pWifiIfaceStat->AccessclassStats[i],
                                vendor_event))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    FL("QCA_WLAN_VENDOR_ATTR put Fail"));
            vos_mem_free(pWifiIfaceStatTL);
            return FALSE;
        }

        nla_nest_end(vendor_event, wmmStats);
    }
    nla_nest_end(vendor_event, wmmInfo);
    vos_mem_free(pWifiIfaceStatTL);
    return TRUE;
}

static tSirWifiInterfaceMode
    hdd_map_device_to_ll_iface_mode ( int deviceMode )
{
    switch (deviceMode)
    {
    case  WLAN_HDD_INFRA_STATION:
        return WIFI_INTERFACE_STA;
    case  WLAN_HDD_SOFTAP:
        return WIFI_INTERFACE_SOFTAP;
    case  WLAN_HDD_P2P_CLIENT:
        return WIFI_INTERFACE_P2P_CLIENT;
    case  WLAN_HDD_P2P_GO:
        return WIFI_INTERFACE_P2P_GO;
    case  WLAN_HDD_IBSS:
        return WIFI_INTERFACE_IBSS;
    default:
        return WIFI_INTERFACE_UNKNOWN;
    }
}

static v_BOOL_t hdd_get_interface_info(hdd_adapter_t *pAdapter,
                           tpSirWifiInterfaceInfo pInfo)
{
    v_U8_t *staMac = NULL;
    hdd_station_ctx_t *pHddStaCtx;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    pInfo->mode = hdd_map_device_to_ll_iface_mode(pAdapter->device_mode);

    vos_mem_copy(pInfo->macAddr,
        pAdapter->macAddressCurrent.bytes, sizeof(v_MACADDR_t));

    if (((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
            (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) ||
            (WLAN_HDD_P2P_DEVICE == pAdapter->device_mode)))
    {
        pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
        if (eConnectionState_NotConnected == pHddStaCtx->conn_info.connState)
        {
            pInfo->state = WIFI_DISCONNECTED;
        }
        if (eConnectionState_Connecting == pHddStaCtx->conn_info.connState)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Session ID %d, Connection is in progress", __func__,
                    pAdapter->sessionId);
            pInfo->state = WIFI_ASSOCIATING;
        }
        if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
            (VOS_FALSE == pHddStaCtx->conn_info.uIsAuthenticated))
        {
            staMac = (v_U8_t *) &(pAdapter->macAddressCurrent.bytes[0]);
            hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: client " MAC_ADDRESS_STR
                " is in the middle of WPS/EAPOL exchange.", __func__,
                MAC_ADDR_ARRAY(staMac));
            pInfo->state = WIFI_AUTHENTICATING;
        }
        if (eConnectionState_Associated == pHddStaCtx->conn_info.connState)
        {
            pInfo->state = WIFI_ASSOCIATED;
            vos_mem_copy(pInfo->bssid,
                    &pHddStaCtx->conn_info.bssId, WNI_CFG_BSSID_LEN);
            vos_mem_copy(pInfo->ssid,
                    pHddStaCtx->conn_info.SSID.SSID.ssId,
                    pHddStaCtx->conn_info.SSID.SSID.length);
            //NULL Terminate the string.
            pInfo->ssid[pHddStaCtx->conn_info.SSID.SSID.length] = 0;
        }
    }
    vos_mem_copy(pInfo->countryStr,
        pMac->scan.countryCodeCurrent, WNI_CFG_COUNTRY_CODE_LEN);

    vos_mem_copy(pInfo->apCountryStr,
        pMac->scan.countryCodeCurrent, WNI_CFG_COUNTRY_CODE_LEN);

    return TRUE;
}

/*
 * hdd_link_layer_process_peer_stats () - This function is called after
 * receiving Link Layer Peer statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 */
static v_VOID_t hdd_link_layer_process_peer_stats(hdd_adapter_t *pAdapter,
                                                   v_VOID_t *pData)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tpSirWifiRateStat   pWifiRateStat;
    tpSirWifiPeerStat   pWifiPeerStat;
    tpSirWifiPeerInfo   pWifiPeerInfo;
    struct nlattr *peerInfo;
    struct sk_buff *vendor_event;
    int status, i;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid") );
        return;
    }

    pWifiPeerStat = (tpSirWifiPeerStat) pData;

    hddLog(VOS_TRACE_LEVEL_INFO,
            "LL_STATS_PEER_ALL : numPeers %u",
            pWifiPeerStat->numPeers);
    {
        for (i = 0; i < pWifiPeerStat->numPeers; i++)
        {
            pWifiPeerInfo = (tpSirWifiPeerInfo)
                ((uint8 *)pWifiPeerStat->peerInfo +
                ( i * sizeof(tSirWifiPeerInfo)));

            if (WLAN_HDD_INFRA_STATION == pAdapter->device_mode) {
                    pWifiPeerInfo->type = WIFI_PEER_AP;
            }
            if (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) {
                    pWifiPeerInfo->type = WIFI_PEER_P2P_GO;
            }

            hddLog(VOS_TRACE_LEVEL_INFO,
                    " %d) LL_STATS Channel Stats "
                    " Peer Type %u "
                    " peerMacAddress  %pM "
                    " capabilities 0x%x "
                    " numRate %u ",
                    i,
                    pWifiPeerInfo->type,
                    pWifiPeerInfo->peerMacAddress,
                    pWifiPeerInfo->capabilities,
                    pWifiPeerInfo->numRate);
            {
                int j;
                for (j = 0; j < pWifiPeerInfo->numRate; j++)
                {
                    pWifiRateStat = (tpSirWifiRateStat)
                        ((tANI_U8 *) pWifiPeerInfo->rateStats +
                         ( j * sizeof(tSirWifiRateStat)));

                    hddLog(VOS_TRACE_LEVEL_INFO,
                            "   peer Rate Stats "
                            "   preamble  %u "
                            "   nss %u "
                            "   bw %u "
                            "   rateMcsIdx  %u "
                            "   reserved %u "
                            "   bitrate %u "
                            "   txMpdu %u "
                            "   rxMpdu %u "
                            "   mpduLost %u "
                            "   retries %u "
                            "   retriesShort %u "
                            "   retriesLong %u",
                            pWifiRateStat->rate.preamble,
                            pWifiRateStat->rate.nss,
                            pWifiRateStat->rate.bw,
                            pWifiRateStat->rate.rateMcsIdx,
                            pWifiRateStat->rate.reserved,
                            pWifiRateStat->rate.bitrate,
                            pWifiRateStat->txMpdu,
                            pWifiRateStat->rxMpdu,
                            pWifiRateStat->mpduLost,
                            pWifiRateStat->retries,
                            pWifiRateStat->retriesShort,
                            pWifiRateStat->retriesLong);
                }
            }
        }
    }

    /*
     * Allocate a size of 4096 for the peer stats comprising
     * each of size = sizeof (tSirWifiPeerInfo) + numRate *
     * sizeof (tSirWifiRateStat).Each field is put with an
     * NL attribute.The size of 4096 is considered assuming
     * that number of rates shall not exceed beyond 50 with
     * the sizeof (tSirWifiRateStat) being 32.
     */
    vendor_event = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
            LL_STATS_EVENT_BUF_SIZE + NLMSG_HDRLEN,
            QCA_NL80211_VENDOR_SUBCMD_LL_PEER_INFO_STATS_INDEX,
            GFP_KERNEL);
    if (!vendor_event)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: cfg80211_vendor_event_alloc failed",
                __func__);
        return;
    }
    if (nla_put_u32(vendor_event,
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS,
            pWifiPeerStat->numPeers))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: QCA_WLAN_VENDOR_ATTR put fail", __func__);
        kfree_skb(vendor_event);
        return;
    }

    peerInfo = nla_nest_start(vendor_event,
            QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO);

    pWifiPeerInfo = (tpSirWifiPeerInfo)  ((uint8 *)
                pWifiPeerStat->peerInfo);

    for (i = 1; i <= pWifiPeerStat->numPeers; i++)
    {
        struct nlattr *peers = nla_nest_start(vendor_event, i);
        int numRate = pWifiPeerInfo->numRate;

        if (FALSE == put_wifi_peer_info(
                                     pWifiPeerInfo, vendor_event))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: put_wifi_peer_info put fail", __func__);
            kfree_skb(vendor_event);
            return;
        }

        pWifiPeerInfo = (tpSirWifiPeerInfo)  ((uint8 *)
                pWifiPeerStat->peerInfo +
                (i * sizeof(tSirWifiPeerInfo)) +
                (numRate * sizeof (tSirWifiRateStat)));
        nla_nest_end(vendor_event, peers);
    }
    nla_nest_end(vendor_event, peerInfo);
    cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}

/*
 * hdd_link_layer_process_iface_stats () - This function is called after
 * receiving Link Layer Interface statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 */
static v_VOID_t hdd_link_layer_process_iface_stats(hdd_adapter_t *pAdapter,
                                                   v_VOID_t *pData)
{
    tpSirWifiIfaceStat  pWifiIfaceStat;
    struct sk_buff *vendor_event;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    int status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid") );
        return;
    }
    /*
     * Allocate a size of 4096 for the interface stats comprising
     * sizeof (tpSirWifiIfaceStat).The size of 4096 is considered
     * assuming that all these fit with in the limit.Please take
     * a call on the limit based on the data requirements on
     * interface statistics.
     */
    vendor_event = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
           LL_STATS_EVENT_BUF_SIZE + NLMSG_HDRLEN,
           QCA_NL80211_VENDOR_SUBCMD_LL_IFACE_STATS_INDEX,
           GFP_KERNEL);
    if (!vendor_event)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("cfg80211_vendor_event_alloc failed") );
        return;
    }

    pWifiIfaceStat = (tpSirWifiIfaceStat) pData;


    if (FALSE == hdd_get_interface_info( pAdapter,
                                        &pWifiIfaceStat->info))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("hdd_get_interface_info get fail") );
        kfree_skb(vendor_event);
        return;
    }

    if (FALSE == put_wifi_iface_stats( pAdapter, pWifiIfaceStat,
                                       vendor_event))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("put_wifi_iface_stats fail") );
        kfree_skb(vendor_event);
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           "WMI_LINK_STATS_IFACE Data");

    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_IFACE: "
           " Mode %u "
           " MAC %pM "
           " State %u "
           " Roaming %u "
           " capabilities 0x%x "
           " SSID %s "
           " BSSID %pM",
           pWifiIfaceStat->info.mode,
           pWifiIfaceStat->info.macAddr,
           pWifiIfaceStat->info.state,
           pWifiIfaceStat->info.roaming,
           pWifiIfaceStat->info.capabilities,
           pWifiIfaceStat->info.ssid,
           pWifiIfaceStat->info.bssid);

    hddLog(VOS_TRACE_LEVEL_INFO,
           " AP country str: %c%c%c",
           pWifiIfaceStat->info.apCountryStr[0],
           pWifiIfaceStat->info.apCountryStr[1],
           pWifiIfaceStat->info.apCountryStr[2]);


    hddLog(VOS_TRACE_LEVEL_INFO,
           " Country Str Association: %c%c%c",
           pWifiIfaceStat->info.countryStr[0],
           pWifiIfaceStat->info.countryStr[1],
           pWifiIfaceStat->info.countryStr[2]);

    hddLog(VOS_TRACE_LEVEL_INFO,
           " beaconRx %u "
           " mgmtRx %u "
           " mgmtActionRx  %u "
           " mgmtActionTx %u "
           " rssiMgmt %d "
           " rssiData %d "
           " rssiAck  %d",
           pWifiIfaceStat->beaconRx,
           pWifiIfaceStat->mgmtRx,
           pWifiIfaceStat->mgmtActionRx,
           pWifiIfaceStat->mgmtActionTx,
           pWifiIfaceStat->rssiMgmt,
           pWifiIfaceStat->rssiData,
           pWifiIfaceStat->rssiAck );


    {
        int i;
        for (i = 0 ; i < WIFI_AC_MAX; i ++)
        {
            hddLog(VOS_TRACE_LEVEL_INFO,

                   " %d) LL_STATS IFACE: "
                   " ac:  %u  txMpdu: %u "
                   " rxMpdu: %u txMcast: %u "
                   " rxMcast: %u  rxAmpdu: %u "
                   " txAmpdu:  %u  mpduLost: %u "
                   " retries: %u  retriesShort: %u "
                   " retriesLong: %u  contentionTimeMin: %u "
                   " contentionTimeMax: %u  contentionTimeAvg: %u "
                   " contentionNumSamples: %u",
                   i,
                   pWifiIfaceStat->AccessclassStats[i].ac,
                   pWifiIfaceStat->AccessclassStats[i].txMpdu,
                   pWifiIfaceStat->AccessclassStats[i].rxMpdu,
                   pWifiIfaceStat->AccessclassStats[i].txMcast,
                   pWifiIfaceStat->AccessclassStats[i].rxMcast,
                   pWifiIfaceStat->AccessclassStats[i].rxAmpdu,
                   pWifiIfaceStat->AccessclassStats[i].txAmpdu,
                   pWifiIfaceStat->AccessclassStats[i].mpduLost,
                   pWifiIfaceStat->AccessclassStats[i].retries,
                   pWifiIfaceStat->
                       AccessclassStats[i].retriesShort,
                   pWifiIfaceStat->AccessclassStats[i].retriesLong,
                   pWifiIfaceStat->
                       AccessclassStats[i].contentionTimeMin,
                   pWifiIfaceStat->
                       AccessclassStats[i].contentionTimeMax,
                   pWifiIfaceStat->
                       AccessclassStats[i].contentionTimeAvg,
                   pWifiIfaceStat->
                       AccessclassStats[i].contentionNumSamples);

        }
    }

    cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}

/*
 * hdd_link_layer_process_radio_stats () - This function is called after
 * receiving Link Layer Radio statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 */
static v_VOID_t hdd_link_layer_process_radio_stats(hdd_adapter_t *pAdapter,
                                                   v_VOID_t *pData)
{
    int status, i;
    tpSirWifiRadioStat  pWifiRadioStat;
    tpSirWifiChannelStats pWifiChannelStats;
    struct sk_buff *vendor_event;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    struct nlattr *chList;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid") );
        return;
    }
    pWifiRadioStat = (tpSirWifiRadioStat) pData;

    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_RADIO"
           " radio is %d onTime is %u "
           " txTime is %u  rxTime is %u "
           " onTimeScan is %u  onTimeNbd is %u "
           " onTimeEXTScan is %u onTimeRoamScan is %u "
           " onTimePnoScan is %u  onTimeHs20 is %u "
           " numChannels is %u",
           pWifiRadioStat->radio, pWifiRadioStat->onTime,
           pWifiRadioStat->txTime, pWifiRadioStat->rxTime,
           pWifiRadioStat->onTimeScan, pWifiRadioStat->onTimeNbd,
           pWifiRadioStat->onTimeEXTScan,
           pWifiRadioStat->onTimeRoamScan,
           pWifiRadioStat->onTimePnoScan,
           pWifiRadioStat->onTimeHs20,
           pWifiRadioStat->numChannels);
    /*
     * Allocate a size of 4096 for the Radio stats comprising
     * sizeof (tSirWifiRadioStat) + numChannels * sizeof
     * (tSirWifiChannelStats).Each channel data is put with an
     * NL attribute.The size of 4096 is considered assuming that
     * number of channels shall not exceed beyond  60 with the
     * sizeof (tSirWifiChannelStats) being 24 bytes.
     */

    vendor_event = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
           LL_STATS_EVENT_BUF_SIZE + NLMSG_HDRLEN ,
           QCA_NL80211_VENDOR_SUBCMD_LL_RADIO_STATS_INDEX,
           GFP_KERNEL);

    if (!vendor_event)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("cfg80211_vendor_event_alloc failed") );
        return;
    }

    if (nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID,
             pWifiRadioStat->radio)      ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME,
             pWifiRadioStat->onTime)     ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME,
             pWifiRadioStat->txTime)     ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_RX_TIME,
             pWifiRadioStat->rxTime)     ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_SCAN,
             pWifiRadioStat->onTimeScan) ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_NBD,
             pWifiRadioStat->onTimeNbd)  ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_EXTSCAN,
             pWifiRadioStat->onTimeEXTScan)||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN,
             pWifiRadioStat->onTimeRoamScan) ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN,
             pWifiRadioStat->onTimePnoScan) ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_HS20,
             pWifiRadioStat->onTimeHs20)    ||
        nla_put_u32(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS,
             pWifiRadioStat->numChannels))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("QCA_WLAN_VENDOR_ATTR put fail"));
        kfree_skb(vendor_event);
        return ;
    }

    chList = nla_nest_start(vendor_event,
             QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO);
    for (i = 0; i < pWifiRadioStat->numChannels; i++)
    {
        struct nlattr *chInfo;

        pWifiChannelStats = (tpSirWifiChannelStats) ((uint8*)
                pWifiRadioStat->channels +
                (i * sizeof(tSirWifiChannelStats)));

        hddLog(VOS_TRACE_LEVEL_INFO,
               " %d) Channel Info"
               "  width is %u "
               "  CenterFreq %u "
               "  CenterFreq0 %u "
               "  CenterFreq1 %u "
               "  onTime %u "
               "  ccaBusyTime %u",
               i,
               pWifiChannelStats->channel.width,
               pWifiChannelStats->channel.centerFreq,
               pWifiChannelStats->channel.centerFreq0,
               pWifiChannelStats->channel.centerFreq1,
               pWifiChannelStats->onTime,
               pWifiChannelStats->ccaBusyTime);


        chInfo = nla_nest_start(vendor_event, i);

        if (nla_put_u32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_WIDTH,
                pWifiChannelStats->channel.width) ||
            nla_put_u32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ,
                pWifiChannelStats->channel.centerFreq) ||
            nla_put_u32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0,
                pWifiChannelStats->channel.centerFreq0)  ||
            nla_put_u32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1,
                pWifiChannelStats->channel.centerFreq1)    ||
            nla_put_u32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ON_TIME,
                pWifiChannelStats->onTime)  ||
            nla_put_u32(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME,
                pWifiChannelStats->ccaBusyTime))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   FL("cfg80211_vendor_event_alloc failed") );
            kfree_skb(vendor_event);
            return ;
        }
        nla_nest_end(vendor_event, chInfo);
    }
    nla_nest_end(vendor_event, chList);

    cfg80211_vendor_event(vendor_event, GFP_KERNEL);
    return;
}

/*
 * hdd_link_layer_stats_ind_callback () - This function is called after
 * receiving Link Layer indications from FW.This callback converts the firmware
 * data to the NL data and send the same to the kernel/upper layers.
 */
static void hdd_link_layer_stats_ind_callback ( void *pCtx,
                                                int indType,
                                                void *pRsp, u8  *macAddr)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)pCtx;
    hdd_adapter_t *pAdapter = NULL;
    tpSirLLStatsResults linkLayerStatsResults = (tpSirLLStatsResults)pRsp;
    int status;

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return;
    }



    pAdapter = hdd_get_adapter_by_macaddr(pHddCtx, macAddr);
    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL(" MAC address %pM does not exist with host"),
                macAddr);
        return;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s: Interface: %s LLStats indType: %d", __func__,
            pAdapter->dev->name, indType);

    switch (indType)
    {
    case SIR_HAL_LL_STATS_RESULTS_RSP:
        {
            hddLog(VOS_TRACE_LEVEL_INFO,
                    FL("RESPONSE SIR_HAL_LL_STATS_RESULTS_RSP") );
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "LL_STATS RESULTS RESPONSE paramID = 0x%x",
                    linkLayerStatsResults->paramId);
            hddLog(VOS_TRACE_LEVEL_INFO,
               "LL_STATS RESULTS RESPONSE ifaceId = %u MAC: %pM",
               linkLayerStatsResults->ifaceId, macAddr);
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "LL_STATS RESULTS RESPONSE respId = %u",
                    linkLayerStatsResults->respId);
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "LL_STATS RESULTS RESPONSE moreResultToFollow = %u",
                    linkLayerStatsResults->moreResultToFollow);
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "LL_STATS RESULTS RESPONSE result = %p",
                    linkLayerStatsResults->result);
            if ( linkLayerStatsResults->paramId & WMI_LINK_STATS_RADIO )
            {
                hdd_link_layer_process_radio_stats(pAdapter,
                                (v_VOID_t *)linkLayerStatsResults->result);
            }
            else if ( linkLayerStatsResults->paramId & WMI_LINK_STATS_IFACE )
            {
                hdd_link_layer_process_iface_stats(pAdapter,
                                (v_VOID_t *)linkLayerStatsResults->result);
            }
            else if ( linkLayerStatsResults->paramId &
                    WMI_LINK_STATS_ALL_PEER )
            {
                hdd_link_layer_process_peer_stats(pAdapter,
                                (v_VOID_t *)linkLayerStatsResults->result);
            } /* WMI_LINK_STATS_ALL_PEER */
            else
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        FL("INVALID LL_STATS_NOTIFY RESPONSE ***********"));
            }

            break;
        }
        default:
            hddLog(VOS_TRACE_LEVEL_ERROR, "invalid event type %d", indType);
            break;
    }
    return;
}

const struct
nla_policy
qca_wlan_vendor_ll_set_policy[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD] =
    { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING] =
    { .type = NLA_U32 },
};

static int wlan_hdd_cfg80211_ll_stats_set(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        void *data,
        int data_len)
{
    int status;
    struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX + 1];
    tSirLLStatsSetReq linkLayerStatsSetReq;
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    hdd_station_ctx_t *pHddStaCtx;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }

    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD adapter is Null"));
        return -ENODEV;
    }
    /* check the LLStats Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableLLStats) ||
         (TRUE != sme_IsFeatureSupportedByFW(LINK_LAYER_STATS_MEAS)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("Link Layer Statistics not supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX,
           (struct nlattr *)data,
           data_len, qca_wlan_vendor_ll_set_policy))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL() );
        return -EINVAL;
    }
    if (!tb_vendor
            [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("MPDU size Not present"));
        return -EINVAL;
    }
    if (!tb_vendor[
         QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL(" Stats Gathering Not Present"));
        return -EINVAL;
    }
    // Shall take the request Id if the Upper layers pass. 1 For now.
    linkLayerStatsSetReq.reqId = 1;

    linkLayerStatsSetReq.mpduSizeThreshold =
        nla_get_u32(
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD]);

    linkLayerStatsSetReq.aggressiveStatisticsGathering =
        nla_get_u32(
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING]);

    vos_mem_copy(linkLayerStatsSetReq.macAddr,
               pAdapter->macAddressCurrent.bytes, sizeof(v_MACADDR_t));


    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_SET reqId = %d", linkLayerStatsSetReq.reqId);
    hddLog(VOS_TRACE_LEVEL_INFO,
            "LL_STATS_SET MAC = %pM", linkLayerStatsSetReq.macAddr);
    hddLog(VOS_TRACE_LEVEL_INFO,
            "LL_STATS_SET mpduSizeThreshold = %d",
            linkLayerStatsSetReq.mpduSizeThreshold);
    hddLog(VOS_TRACE_LEVEL_INFO,
            "LL_STATS_SET aggressive Statistics Gathering  = %d",
            linkLayerStatsSetReq.aggressiveStatisticsGathering);

    if (eHAL_STATUS_SUCCESS != sme_SetLinkLayerStatsIndCB(
                               pHddCtx->hHal,
                               hdd_link_layer_stats_ind_callback))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s:"
           "sme_SetLinkLayerStatsIndCB Failed", __func__);
        return -EINVAL;

    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    if (VOS_STATUS_SUCCESS !=
        WLANTL_ClearInterfaceStats((WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
        pHddStaCtx->conn_info.staId[0], WIFI_STATS_IFACE))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s:"
                "WLANTL_ClearInterfaceStats Failed", __func__);
        return -EINVAL;
    }

    pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_VO] = 0;
    pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_VI] = 0;
    pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_BE] = 0;
    pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_BK] = 0;

    if (eHAL_STATUS_SUCCESS != sme_LLStatsSetReq( pHddCtx->hHal,
                                            &linkLayerStatsSetReq))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s:"
           "sme_LLStatsSetReq Failed", __func__);
        return -EINVAL;
    }

    pAdapter->isLinkLayerStatsSet = 1;

    return 0;
}

const struct
nla_policy
qca_wlan_vendor_ll_get_policy[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX +1] =
{
    /* Unsigned 32bit value provided by the caller issuing the GET stats
     * command. When reporting
     * the stats results, the driver uses the same value to indicate
     * which GET request the results
     * correspond to.
     */
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID] = { .type = NLA_U32 },

    /* Unsigned 32bit value . bit mask to identify what statistics are
         requested for retrieval */
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK] = { .type = NLA_U32 },
};

static int wlan_hdd_cfg80211_ll_stats_get(struct wiphy *wiphy,
                                          struct wireless_dev *wdev,
                                          void *data,
                                          int data_len)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX + 1];
    tSirLLStatsGetReq linkLayerStatsGetReq;
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL ;
    }

    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
               "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }
    /* check the LLStats Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableLLStats) ||
         (TRUE != sme_IsFeatureSupportedByFW(LINK_LAYER_STATS_MEAS)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("Link Layer Statistics not supported by Firmware"));
        return -EINVAL;
    }


    if (!pAdapter->isLinkLayerStatsSet)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
               "%s: isLinkLayerStatsSet : %d",
               __func__, pAdapter->isLinkLayerStatsSet);
        return -EINVAL;
    }

    if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX,
            (struct nlattr *)data,
            data_len, qca_wlan_vendor_ll_get_policy))
    {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL() );
       return -EINVAL;
    }

    if (!tb_vendor
            [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID])
    {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("Request Id Not present"));
       return -EINVAL;
    }

    if (!tb_vendor
            [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK])
    {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("Req Mask Not present"));
       return -EINVAL;
    }


    linkLayerStatsGetReq.reqId =
        nla_get_u32( tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID]);
    linkLayerStatsGetReq.paramIdMask =
        nla_get_u32( tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK]);

    vos_mem_copy(linkLayerStatsGetReq.macAddr,
               pAdapter->macAddressCurrent.bytes, sizeof(v_MACADDR_t));

    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_GET reqId = %d", linkLayerStatsGetReq.reqId);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_GET MAC = %pM", linkLayerStatsGetReq.macAddr);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_GET paramIdMask = %d",
           linkLayerStatsGetReq.paramIdMask);

    if (eHAL_STATUS_SUCCESS  != sme_LLStatsGetReq( pHddCtx->hHal,
                                                &linkLayerStatsGetReq))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s:"
               "sme_LLStatsGetReq Failed", __func__);
        return -EINVAL;
    }
    return 0;
}

const struct
nla_policy
qca_wlan_vendor_ll_clr_policy[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ] = {.type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP] = {.type = NLA_U8 },
};

static int wlan_hdd_cfg80211_ll_stats_clear(struct wiphy *wiphy,
                                            struct wireless_dev *wdev,
                                            void *data,
                                            int data_len)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX + 1];
    tSirLLStatsClearReq linkLayerStatsClearReq;
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    u32 statsClearReqMask;
    u8 stopReq;
    int status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }

    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
                   "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }
    /* check the LLStats Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableLLStats) ||
         (TRUE != sme_IsFeatureSupportedByFW(LINK_LAYER_STATS_MEAS)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("Enable LLStats Capability"));
        return -EINVAL;
    }

    if (!pAdapter->isLinkLayerStatsSet)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
                   "%s: isLinkLayerStatsSet : %d",
                   __func__, pAdapter->isLinkLayerStatsSet);
        return -EINVAL;
    }

    if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX,
            (struct nlattr *)data,
            data_len, qca_wlan_vendor_ll_clr_policy))
    {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL() );
       return -EINVAL;
    }

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK] ||

        !tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ])
    {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("Error in LL_STATS CLR CONFIG PARA") );
       return -EINVAL;

    }


    statsClearReqMask = linkLayerStatsClearReq.statsClearReqMask =
        nla_get_u32(
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK]);

    stopReq = linkLayerStatsClearReq.stopReq =
        nla_get_u8(
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ]);

    // Shall take the request Id if the Upper layers pass. 1 For now.
    linkLayerStatsClearReq.reqId = 1;

    vos_mem_copy(linkLayerStatsClearReq.macAddr,
               pAdapter->macAddressCurrent.bytes, sizeof(v_MACADDR_t));

    hddLog(VOS_TRACE_LEVEL_INFO,
            "LL_STATS_CLEAR reqId = %d", linkLayerStatsClearReq.reqId);
    hddLog(VOS_TRACE_LEVEL_INFO,
            "LL_STATS_CLEAR MAC = %pM", linkLayerStatsClearReq.macAddr);
    hddLog(VOS_TRACE_LEVEL_INFO,
            "LL_STATS_CLEAR statsClearReqMask = 0x%X",
            linkLayerStatsClearReq.statsClearReqMask);
    hddLog(VOS_TRACE_LEVEL_INFO,
            "LL_STATS_CLEAR stopReq  = %d",
            linkLayerStatsClearReq.stopReq);

    if (eHAL_STATUS_SUCCESS == sme_LLStatsClearReq(pHddCtx->hHal,
                                                     &linkLayerStatsClearReq))
    {
        struct sk_buff *temp_skbuff;
        hdd_station_ctx_t *pHddStaCtx;

        pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
        if (VOS_STATUS_SUCCESS !=
           WLANTL_ClearInterfaceStats((WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
           pHddStaCtx->conn_info.staId[0], statsClearReqMask))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s:"
                    "WLANTL_ClearInterfaceStats Failed", __func__);
            return -EINVAL;
        }
        if ((statsClearReqMask & WIFI_STATS_IFACE_AC) ||
                (statsClearReqMask & WIFI_STATS_IFACE)) {
            pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_VO] = 0;
            pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_VI] = 0;
            pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_BE] = 0;
            pAdapter->hdd_stats.hddTxRxStats.txMcast[WLANTL_AC_BK] = 0;
        }

        temp_skbuff = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
                                2 * sizeof(u32) +
                            NLMSG_HDRLEN);

        if (temp_skbuff != NULL)
        {

            if (nla_put_u32(temp_skbuff,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK,
                    statsClearReqMask) ||
                 nla_put_u32(temp_skbuff,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP,
                    stopReq))
            {
                 hddLog(VOS_TRACE_LEVEL_ERROR, FL("LL_STATS_CLR put fail"));
                 kfree_skb(temp_skbuff);
                 return -EINVAL;
            }
            /* If the ask is to stop the stats collection as part of clear
             * (stopReq = 1) , ensure that no further requests of get
             * go to the firmware by having isLinkLayerStatsSet set to 0.
             * However it the stopReq as part of the clear request is 0 ,
             * the request to get the statistics are honoured as in this
             * case the firmware is just asked to clear the statistics.
             */
            if (linkLayerStatsClearReq.stopReq == 1)
                pAdapter->isLinkLayerStatsSet = 0;
            return cfg80211_vendor_cmd_reply(temp_skbuff);
        }
        return -ENOMEM;
    }
    return -EINVAL;
}
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef WLAN_FEATURE_EXTSCAN
static const struct nla_policy
wlan_hdd_extscan_config_policy
            [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1] =
{
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID] =
                                                        { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_WIFI_BAND] =
                                                        { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME] =
                                                            { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CLASS] = { .type = NLA_U8 },

    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_INDEX] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BAND] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_PERIOD] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_REPORT_EVENTS] =
                                                            { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS] =
                                                            { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_BASE_PERIOD] =
                                                            { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_MAX_AP_PER_SCAN] =
                                                            { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD] =
                                                            { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS] =
                                                            { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_FLUSH] =
                                                             { .type = NLA_U8 },

    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_MAX] =
                                                            { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID] =
                                                        { .type = NLA_UNSPEC },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW] =
                                                            { .type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH] =
                                                            { .type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_CHANNEL] =
                                                            { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_NUM_AP] =
                                                            { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE] =
                                                         { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE]
                                                         = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING] =
                                                           { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP] = { .type =
                                                                     NLA_U32 },
};

static void wlan_hdd_cfg80211_extscan_get_capabilities_ind(void *ctx, void *pMsg)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    tpSirEXTScanCapabilitiesEvent pData =
                    (tpSirEXTScanCapabilitiesEvent) pMsg;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                        EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                        QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES_INDEX,
                        GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "Scan cache size (%u)", pData->scanCacheSize);
    hddLog(VOS_TRACE_LEVEL_INFO, "Scan buckets (%u)", pData->scanBuckets);
    hddLog(VOS_TRACE_LEVEL_INFO, "Max AP per scan (%u)", pData->maxApPerScan);
    hddLog(VOS_TRACE_LEVEL_INFO, "maxRssiSampleSize (%u)",
                                        pData->maxRssiSampleSize);
    hddLog(VOS_TRACE_LEVEL_INFO, "maxScanReportingThreshold (%u)",
                                        pData->maxScanReportingThreshold);
    hddLog(VOS_TRACE_LEVEL_INFO, "maxHotlistAPs (%u)", pData->maxHotlistAPs);
    hddLog(VOS_TRACE_LEVEL_INFO, "maxSignificantWifiChangeAPs (%u)",
                                        pData->maxSignificantWifiChangeAPs);
    hddLog(VOS_TRACE_LEVEL_INFO, "maxBsidHistoryEntries (%u)",
                                        pData->maxBsidHistoryEntries);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
           pData->requestId) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS, pData->status) ||
        nla_put_u32(skb,
           QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_CACHE_SIZE,
           pData->scanCacheSize) ||
        nla_put_u32(skb,
           QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_BUCKETS,
           pData->scanBuckets) ||
        nla_put_u32(skb,
           QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_AP_CACHE_PER_SCAN,
           pData->maxApPerScan) ||
        nla_put_u32(skb,
           QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_RSSI_SAMPLE_SIZE,
           pData->maxRssiSampleSize) ||
        nla_put_u32(skb,
           QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_REPORTING_THRESHOLD,
           pData->maxScanReportingThreshold) ||
        nla_put_u32(skb,
           QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_HOTLIST_APS,
           pData->maxHotlistAPs) ||
        nla_put_u32(skb,
           QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SIGNIFICANT_WIFI_CHANGE_APS,
           pData->maxSignificantWifiChangeAPs) ||
        nla_put_u32(skb,
           QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_BSSID_HISTORY_ENTRIES,
           pData->maxBsidHistoryEntries)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}


static void wlan_hdd_cfg80211_extscan_start_rsp(void *ctx, void *pMsg)
{
    tpSirEXTScanStartRspParams pData = (tpSirEXTScanStartRspParams) pMsg;
    hdd_context_t *pHddCtx         = (hdd_context_t *)ctx;
    struct sk_buff *skb            = NULL;
    tpAniSirGlobal pMac = PMAC_STRUCT( pHddCtx->hHal );


    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                                  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                                  QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START_INDEX,
                                  GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "Status (%u)", pData->status);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                         pData->requestId) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS, pData->status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    /*
     * Store the Request ID for comparing with the requestID obtained
     * in other requests.HDD shall return a failure is the extscan_stop
     * request is issued with a different requestId as that of the
     * extscan_start request. Also, This requestId shall be used while
     * indicating the full scan results to the upper layers.
     * The requestId is stored with the assumption that the firmware
     * shall return the ext scan start request's requestId in ext scan
     * start response.
     */
    if (pData->status == 0)
        pMac->sme.extScanStartReqId = pData->requestId;


    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}


static void wlan_hdd_cfg80211_extscan_stop_rsp(void *ctx, void *pMsg)
{
    tpSirEXTScanStopRspParams pData = (tpSirEXTScanStopRspParams) pMsg;
    hdd_context_t *pHddCtx        = (hdd_context_t *)ctx;
    struct sk_buff *skb           = NULL;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                                    EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                                    QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP_INDEX,
                                    GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                         pData->requestId) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS, pData->status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}


static void wlan_hdd_cfg80211_extscan_set_bss_hotlist_rsp(void *ctx,
                                                        void *pMsg)
{
    hdd_context_t *pHddCtx    = (hdd_context_t *)ctx;
    struct sk_buff *skb       = NULL;
    tpSirEXTScanSetBssidHotListRspParams pData =
                    (tpSirEXTScanSetBssidHotListRspParams) pMsg;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }
    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                      EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                      QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST_INDEX,
                      GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "Status (%u)", pData->status);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                         pData->requestId) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS, pData->status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}

static void wlan_hdd_cfg80211_extscan_reset_bss_hotlist_rsp(void *ctx,
                                                          void *pMsg)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    tpSirEXTScanResetBssidHotlistRspParams pData =
                    (tpSirEXTScanResetBssidHotlistRspParams) pMsg;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                      EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                      QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST_INDEX,
                      GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                         pData->requestId) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS, pData->status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}


static void wlan_hdd_cfg80211_extscan_set_signf_wifi_change_rsp(void *ctx,
                                                              void *pMsg)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    tpSirEXTScanSetSignificantChangeRspParams pData =
                    (tpSirEXTScanSetSignificantChangeRspParams) pMsg;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE_INDEX,
                GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "Status (%u)", pData->status);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                         pData->requestId) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS, pData->status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}


static void wlan_hdd_cfg80211_extscan_reset_signf_wifi_change_rsp(void *ctx,
                                                                void *pMsg)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    tpSirEXTScanResetSignificantChangeRspParams pData =
                    (tpSirEXTScanResetSignificantChangeRspParams) pMsg;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
              EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
              QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE_INDEX,
              GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "Status (%u)", pData->status);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                         pData->requestId) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS, pData->status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}

static void wlan_hdd_cfg80211_extscan_cached_results_ind(void *ctx,
                                                       void *pMsg)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)ctx;
    struct sk_buff *skb    = NULL;
    tANI_U32 i = 0, j, resultsPerEvent;
    tANI_S32 totalResults;
    tpSirWifiScanResultEvent pData = (tpSirWifiScanResultEvent) pMsg;
    tpSirWifiScanResult pSirWifiScanResult;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }
    totalResults = pData->numOfAps;
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "Num results (%u)", pData->numOfAps);
    hddLog(VOS_TRACE_LEVEL_INFO, "More Data (%u)", pData->moreData);

    do{
        resultsPerEvent = ((totalResults >= EXTSCAN_MAX_CACHED_RESULTS_PER_IND) ?
                EXTSCAN_MAX_CACHED_RESULTS_PER_IND : totalResults);
        totalResults -= EXTSCAN_MAX_CACHED_RESULTS_PER_IND;

        skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS_INDEX,
                GFP_KERNEL);

        if (!skb) {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    FL("cfg80211_vendor_event_alloc failed"));
            return;
        }

        hddLog(VOS_TRACE_LEVEL_INFO, "resultsPerEvent (%u)", resultsPerEvent);

        if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                    pData->requestId) ||
                nla_put_u32(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
                    resultsPerEvent)) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
            goto fail;
        }
        if (nla_put_u8(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
                    pData->moreData ? 1 : (totalResults > 0 ? 1 : 0 )))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
            goto fail;
        }

        if (resultsPerEvent) {
            struct nlattr *aps;

            aps = nla_nest_start(skb,
                            QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
            if (!aps)
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                goto fail;
            }

            for (j = 0; j < resultsPerEvent; j++, i++) {
                struct nlattr *ap;
                pSirWifiScanResult =  (tpSirWifiScanResult) ((tANI_U8 *)
                        pData->ap + ( i* sizeof(tSirWifiScanResult)));

                hddLog(VOS_TRACE_LEVEL_INFO, "[index=%u] Timestamp(%llu) "
                        "Ssid (%s)"
                        "Bssid: %pM "
                        "Channel (%u)"
                        "Rssi (%d)"
                        "RTT (%u)"
                        "RTT_SD (%u)",
                        i,
                        pSirWifiScanResult->ts,
                        pSirWifiScanResult->ssid,
                        pSirWifiScanResult->bssid,
                        pSirWifiScanResult->channel,
                        pSirWifiScanResult->rssi,
                        pSirWifiScanResult->rtt,
                        pSirWifiScanResult->rtt_sd);

                ap = nla_nest_start(skb, j + 1);
                if (!ap)
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                    goto fail;
                }

                if (nla_put_u64(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
                    pSirWifiScanResult->ts) )
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                    goto fail;
                }
                if (nla_put(skb,
                         QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID,
                         sizeof(pSirWifiScanResult->ssid),
                        pSirWifiScanResult->ssid) )
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                    goto fail;
                }
                if (nla_put(skb,
                        QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID,
                        sizeof(pSirWifiScanResult->bssid),
                        pSirWifiScanResult->bssid) )
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                    goto fail;
                }
                if (nla_put_u32(skb,
                       QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL,
                        pSirWifiScanResult->channel) )
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                    goto fail;
                }
                if (nla_put_s32(skb,
                        QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI,
                        pSirWifiScanResult->rssi) )
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                    goto fail;
                }
                if (nla_put_u32(skb,
                          QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT,
                          pSirWifiScanResult->rtt) )
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                    goto fail;
                }
                if (nla_put_u32(skb,
                        QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD,
                        pSirWifiScanResult->rtt_sd))
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
                    goto fail;
                }

                nla_nest_end(skb, ap);
            }
            nla_nest_end(skb, aps);

        }
        cfg80211_vendor_event(skb, GFP_KERNEL);
    } while (totalResults > 0);

    return;
fail:
    kfree_skb(skb);
    return;
}

static void wlan_hdd_cfg80211_extscan_hotlist_match_ind(void *ctx,
                                                      void *pMsg)
{
    tpSirWifiScanResultEvent pData = (tpSirWifiScanResultEvent) pMsg;
    hdd_context_t *pHddCtx         = (hdd_context_t *)ctx;
    struct sk_buff *skb            = NULL;
    tANI_U32 i;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                      EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                      QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_FOUND_INDEX,
                      GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "Num results (%u)", pData->numOfAps);
    hddLog(VOS_TRACE_LEVEL_INFO, "More Data (%u)", pData->moreData);

    for (i = 0; i < pData->numOfAps; i++) {
        hddLog(VOS_TRACE_LEVEL_INFO, "[index=%u] Timestamp(0x%lld) "
                              "Ssid (%s) "
                              "Bssid (" MAC_ADDRESS_STR ") "
                              "Channel (%u) "
                              "Rssi (%d) "
                              "RTT (%u) "
                              "RTT_SD (%u) ",
                              i,
                              pData->ap[i].ts,
                              pData->ap[i].ssid,
                              MAC_ADDR_ARRAY(pData->ap[i].bssid),
                              pData->ap[i].channel,
                              pData->ap[i].rssi,
                              pData->ap[i].rtt,
                              pData->ap[i].rtt_sd);
    }

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                      pData->requestId) ||
        nla_put_u32(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
                      pData->numOfAps)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
        goto fail;
    }
    if (pData->numOfAps) {
        struct nlattr *aps;

        aps = nla_nest_start(skb,
                   QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
        if (!aps)
            goto fail;

        for (i = 0; i < pData->numOfAps; i++) {
            struct nlattr *ap;

            ap = nla_nest_start(skb, i + 1);
            if (!ap)
                goto fail;

            if (nla_put_u64(skb,
                   QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
                    pData->ap[i].ts) ||
                nla_put(skb,
                     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID,
                     sizeof(pData->ap[i].ssid),
                     pData->ap[i].ssid) ||
                nla_put(skb,
                     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID,
                     sizeof(pData->ap[i].bssid),
                     pData->ap[i].bssid) ||
                nla_put_u32(skb,
                     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL,
                     pData->ap[i].channel) ||
                nla_put_s32(skb,
                     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI,
                     pData->ap[i].rssi) ||
                nla_put_u32(skb,
                     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT,
                     pData->ap[i].rtt) ||
                nla_put_u32(skb,
                     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD,
                     pData->ap[i].rtt_sd))
                goto fail;

              nla_nest_end(skb, ap);
        }
        nla_nest_end(skb, aps);

        if (nla_put_u8(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
                      pData->moreData))
            goto fail;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

fail:
    kfree_skb(skb);
    return;

}
static void wlan_hdd_cfg80211_extscan_signif_wifi_change_results_ind(void *ctx,
                                                                   void *pMsg)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    tANI_U32 i, j;
    tpSirWifiSignificantChangeEvent pData =
                                     (tpSirWifiSignificantChangeEvent) pMsg;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }
    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                    EXTSCAN_EVENT_BUF_SIZE,
                    QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SIGNIFICANT_CHANGE_INDEX,
                    GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "total List Size %u ", pData->numSigRssiBss);
    hddLog(VOS_TRACE_LEVEL_INFO, " CUrrent List size  (%u)",
            pData->numSigRssiBss);
    hddLog(VOS_TRACE_LEVEL_INFO, "moreData (%u)", pData->moreData);

    for (i = 0; i < pData->numSigRssiBss; i++) {
        hddLog(VOS_TRACE_LEVEL_INFO , "Rssi List [%d] BSSID: (%pM) Channel %u "
                " num RSSI %u ",
                i, pData->sigRssiResult[i].bssid,
                pData->sigRssiResult[i].channel,
                pData->sigRssiResult[i].numRssi);

        for (j = 0; j < pData->sigRssiResult[i].numRssi; j++){

            hddLog(VOS_TRACE_LEVEL_INFO,
                    " [%d]",
                    pData->sigRssiResult[i].rssi[j]);

        }
    }


    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                pData->requestId) ||
            nla_put_u32(skb,
                QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
                pData->numSigRssiBss)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("put fail"));
        goto fail;
    }

    if (pData->numSigRssiBss) {
        struct nlattr *aps;
        aps = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
        if (!aps)
            goto fail;
        for (i = 0; i < pData->numSigRssiBss; i++) {
            struct nlattr *ap;

            ap = nla_nest_start(skb, i);
            if (!ap)
                goto fail;
            if (nla_put(skb,
                 QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_BSSID,
                 sizeof(tSirMacAddr), pData->sigRssiResult[i].bssid) ||
                nla_put_u32(skb,
                 QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_CHANNEL,
                 pData->sigRssiResult[i].channel) ||
                nla_put_u32(skb,
                 QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_NUM_RSSI,
                 pData->sigRssiResult[i].numRssi) ||
                nla_put(skb,
                 QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_RSSI_LIST,
                 sizeof(s32) * pData->sigRssiResult[i].numRssi,
                 pData->sigRssiResult[i].rssi))
                goto fail;
            nla_nest_end(skb, ap);
        }
        nla_nest_end(skb, aps);
        if (nla_put_u8(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
                    pData->moreData))
            goto fail;
    }
    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;
fail:
    kfree_skb(skb);
    return;
}

static void wlan_hdd_cfg80211_extscan_full_scan_result_event(void *ctx,
                                                           void *pMsg)
{
    struct sk_buff *skb;
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    tpSirWifiFullScanResultEvent pData =
        (tpSirWifiFullScanResultEvent) (pMsg);

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                    "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
            EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
            QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_FULL_SCAN_RESULT_INDEX,
            GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("cfg80211_vendor_event_alloc failed"));
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%u)"), pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("More Data (%u)"), pData->moreData);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("AP Info: Timestamp(0x%llX) "
                "Ssid (%s)"
                "Bssid (" MAC_ADDRESS_STR ")"
                "Channel (%u)"
                "Rssi (%d)"
                "RTT (%u)"
                "RTT_SD (%u)"),
            pData->ap.ts,
            pData->ap.ssid,
            MAC_ADDR_ARRAY(pData->ap.bssid),
            pData->ap.channel,
            pData->ap.rssi,
            pData->ap.rtt,
            pData->ap.rtt_sd);
    hddLog(VOS_TRACE_LEVEL_INFO, "IE Length (%u)", pData->ieLength);
    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                pData->requestId) ||
        nla_put_u64(skb,
            QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
            pData->ap.ts) ||
        nla_put(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID,
            sizeof(pData->ap.ssid),
            pData->ap.ssid) ||
        nla_put(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID,
            WNI_CFG_BSSID_LEN,
            pData->ap.bssid) ||
        nla_put_u32(skb,
            QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL,
            pData->ap.channel) ||
        nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI,
            pData->ap.rssi) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT,
            pData->ap.rtt) ||
        nla_put_u32(skb,
            QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD,
            pData->ap.rtt_sd) ||
        nla_put_u16(skb,
                QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BEACON_PERIOD,
            pData->ap.beaconPeriod) ||
        nla_put_u16(skb,
                QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CAPABILITY,
            pData->ap.capability) ||
        nla_put_u32(skb,
                QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_LENGTH,
            pData->ieLength))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }
    if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_DATA,
                pData->ieLength,
                pData->ie))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}

static void wlan_hdd_cfg80211_extscan_scan_res_available_event(void *ctx,
                                                             void *pMsg)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    tpSirEXTScanResultsAvailableIndParams pData =
                    (tpSirEXTScanResultsAvailableIndParams) pMsg;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_RESULTS_AVAILABLE_INDEX,
                GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Req Id (%u)", pData->requestId);
    hddLog(VOS_TRACE_LEVEL_INFO, "Num results (%u)",
                                  pData->numResultsAvailable);
    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                    pData->requestId) ||
        nla_put_u32(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
                    pData->numResultsAvailable)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}

static void wlan_hdd_cfg80211_extscan_scan_progress_event(void *ctx, void *pMsg)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    tpSirEXTScanProgressIndParams pData =
                                   (tpSirEXTScanProgressIndParams) pMsg;

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "
                                         "or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                            EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                            QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_EVENT_INDEX,
                            GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Scan event type (%u)",
            pData->extScanEventType);
    hddLog(VOS_TRACE_LEVEL_INFO, "Scan event status (%u)",
            pData->status);

    if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_EVENT_TYPE,
                   pData->extScanEventType) ||
        nla_put_u32(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                    pData->requestId) ||
        nla_put_u32(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_EVENT_STATUS,
                    pData->status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}

void wlan_hdd_cfg80211_extscan_callback(void *ctx, const tANI_U16 evType,
                                      void *pMsg)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)ctx;

    if (wlan_hdd_validate_context(pHddCtx)) {
        hddLog(VOS_TRACE_LEVEL_INFO, FL("HDD context is not valid"));
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Rcvd Event (%d)"), evType);


    switch(evType) {
    case SIR_HAL_EXTSCAN_START_RSP:
            wlan_hdd_cfg80211_extscan_start_rsp(ctx, pMsg);
        break;

    case SIR_HAL_EXTSCAN_STOP_RSP:
            wlan_hdd_cfg80211_extscan_stop_rsp(ctx, pMsg);
        break;
    case SIR_HAL_EXTSCAN_GET_CACHED_RESULTS_RSP:
        /* There is no need to send this response to upper layer
           Just log the message */
        hddLog(VOS_TRACE_LEVEL_INFO,
                FL("Rcvd SIR_HAL_EXTSCAN_CACHED_RESULTS_RSP"));
        break;
    case SIR_HAL_EXTSCAN_SET_BSS_HOTLIST_RSP:
        wlan_hdd_cfg80211_extscan_set_bss_hotlist_rsp(ctx, pMsg);
        break;

    case SIR_HAL_EXTSCAN_RESET_BSS_HOTLIST_RSP:
        wlan_hdd_cfg80211_extscan_reset_bss_hotlist_rsp(ctx, pMsg);
        break;

    case SIR_HAL_EXTSCAN_SET_SIGNF_RSSI_CHANGE_RSP:
        wlan_hdd_cfg80211_extscan_set_signf_wifi_change_rsp(ctx, pMsg);
        break;

    case SIR_HAL_EXTSCAN_RESET_SIGNF_RSSI_CHANGE_RSP:
        wlan_hdd_cfg80211_extscan_reset_signf_wifi_change_rsp(ctx, pMsg);
        break;
    case SIR_HAL_EXTSCAN_GET_CAPABILITIES_RSP:
        wlan_hdd_cfg80211_extscan_get_capabilities_ind(ctx, pMsg);
        break;
    case SIR_HAL_EXTSCAN_PROGRESS_IND:
        wlan_hdd_cfg80211_extscan_scan_progress_event(ctx, pMsg);
        break;
    case SIR_HAL_EXTSCAN_SCAN_AVAILABLE_IND:
        wlan_hdd_cfg80211_extscan_scan_res_available_event(ctx, pMsg);
        break;
    case SIR_HAL_EXTSCAN_SCAN_RESULT_IND:
        wlan_hdd_cfg80211_extscan_cached_results_ind(ctx, pMsg);
        break;
    case SIR_HAL_EXTSCAN_HOTLIST_MATCH_IND:
        wlan_hdd_cfg80211_extscan_hotlist_match_ind(ctx, pMsg);
        break;
    case SIR_HAL_EXTSCAN_SIGNF_WIFI_CHANGE_IND:
        wlan_hdd_cfg80211_extscan_signif_wifi_change_results_ind(ctx, pMsg);
        break;
    case SIR_HAL_EXTSCAN_FULL_SCAN_RESULT_IND:
        wlan_hdd_cfg80211_extscan_full_scan_result_event(ctx, pMsg);
        break;
    default:
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("invalid event type %d "), evType);
        break;
    }
}

static int wlan_hdd_cfg80211_extscan_get_capabilities(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    tSirGetEXTScanCapabilitiesReqParams reqMsg;
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr
        *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    eHalStatus status;

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering"));
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, dataLen,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }


    reqMsg.requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), reqMsg.requestId);

    reqMsg.sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id (%d)"), reqMsg.sessionId);

    status = sme_EXTScanGetCapabilities(pHddCtx->hHal, &reqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("sme_EXTScanGetCapabilities failed(err=%d)"), status);
        return -EINVAL;
    }

    return 0;
}


static int wlan_hdd_cfg80211_extscan_get_cached_results(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    tSirEXTScanGetCachedResultsReqParams reqMsg;
    struct net_device *dev                      = wdev->netdev;
    hdd_adapter_t *pAdapter                     = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                      = wiphy_priv(wiphy);
    struct nlattr
            *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    eHalStatus status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, dataLen,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }
    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }

    reqMsg.requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), reqMsg.requestId);

    reqMsg.sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id (%d)"), reqMsg.sessionId);

    /* Parse and fetch flush parameter */
    if (!tb
      [QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_FLUSH])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr flush failed"));
        goto failed;
    }
    reqMsg.flush = nla_get_u8(
    tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_FLUSH]);

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Flush (%d)"), reqMsg.flush);

    status = sme_getCachedResults(pHddCtx->hHal, &reqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("sme_getCachedResults failed(err=%d)"), status);
        return -EINVAL;
    }
    return 0;

failed:
    return -EINVAL;
}

static int wlan_hdd_cfg80211_extscan_set_bssid_hotlist(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    tpSirEXTScanSetBssidHotListReqParams pReqMsg = NULL;
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr
            *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr
            *tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *apTh;
    eHalStatus status;
    tANI_U8 i = 0;
    int rem;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, dataLen,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }
    pReqMsg = (tpSirEXTScanSetBssidHotListReqParams)
                                     vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("vos_mem_malloc failed"));
        return -ENOMEM;
    }


    pReqMsg->requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), pReqMsg->requestId);

    /* Parse and fetch number of APs */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_NUM_AP]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr number of AP failed"));
        goto fail;
    }

    pReqMsg->sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id (%d)"), pReqMsg->sessionId);

    pReqMsg->numAp = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_NUM_AP]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Number of AP (%d)"), pReqMsg->numAp);

    nla_for_each_nested(apTh,
                tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM], rem) {
        if(nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                nla_data(apTh), nla_len(apTh),
                NULL)) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla_parse failed"));
            goto fail;
        }

        /* Parse and fetch MAC address */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac address failed"));
            goto fail;
        }
        memcpy(pReqMsg->ap[i].bssid, nla_data(
                tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID]),
                sizeof(tSirMacAddr));
        hddLog(VOS_TRACE_LEVEL_INFO, FL("BSSID: %pM "), pReqMsg->ap[i].bssid);

        /* Parse and fetch low RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr low RSSI failed"));
            goto fail;
        }
        pReqMsg->ap[i].low = nla_get_s32(
             tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW]);
        hddLog(VOS_TRACE_LEVEL_INFO, FL("RSSI low (%d)"), pReqMsg->ap[i].low);

        /* Parse and fetch high RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr high RSSI failed"));
            goto fail;
        }
        pReqMsg->ap[i].high = nla_get_s32(
            tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH]);
        hddLog(VOS_TRACE_LEVEL_INFO, FL("RSSI High (%d)"),
                                         pReqMsg->ap[i].high);

        /* Parse and fetch channel */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_CHANNEL]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr channel failed"));
            goto fail;
        }
        pReqMsg->ap[i].channel = nla_get_u32(
              tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_CHANNEL]);
        hddLog(VOS_TRACE_LEVEL_INFO,
                FL("Channel (%u)"), pReqMsg->ap[i].channel);
        i++;
    }
    status = sme_SetBssHotlist(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("sme_SetBssHotlist failed(err=%d)"), status);
        vos_mem_free(pReqMsg);
        return -EINVAL;
    }

    vos_mem_free(pReqMsg);

    return 0;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

static int wlan_hdd_cfg80211_extscan_set_significant_change(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    tpSirEXTScanSetSignificantChangeReqParams pReqMsg = NULL;
    struct net_device *dev                  = wdev->netdev;
    hdd_adapter_t *pAdapter                 = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                  = wiphy_priv(wiphy);
    struct nlattr
        *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr
        *tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *apTh;
    eHalStatus status;
    int i = 0;
    int rem;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, dataLen,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }
    pReqMsg = (tpSirEXTScanSetSignificantChangeReqParams)
                                     vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("vos_mem_malloc failed"));
       return -ENOMEM;
    }



    pReqMsg->requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), pReqMsg->requestId);

    /* Parse and fetch RSSI sample size */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr RSSI sample size failed"));
        goto fail;
    }
    pReqMsg->rssiSampleSize = nla_get_u32(
    tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE]);
    hddLog(VOS_TRACE_LEVEL_INFO,
               FL("RSSI sample size (%u)"), pReqMsg->rssiSampleSize);

    /* Parse and fetch lost AP sample size */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr lost AP sample size failed"));
        goto fail;
    }
    pReqMsg->lostApSampleSize = nla_get_u32(
    tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE]);
    hddLog(VOS_TRACE_LEVEL_INFO,
               FL("Lost AP sample size (%u)"), pReqMsg->lostApSampleSize);
    /* Parse and fetch minimum Breaching */
    if (!tb
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr minBreaching failed"));
        goto fail;
    }
    pReqMsg->minBreaching = nla_get_u32(
      tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL(" Breaching (%d)"), pReqMsg->minBreaching);

    /* Parse and fetch number of APs */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr number of AP failed"));
        goto fail;
    }
    pReqMsg->numAp = nla_get_u32(
            tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Number of AP (%d)"), pReqMsg->numAp);

    pReqMsg->sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id (%d)"), pReqMsg->sessionId);

    nla_for_each_nested(apTh,
                tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM], rem) {
        if(nla_parse(tb2,
                QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                nla_data(apTh), nla_len(apTh),
                NULL)) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla_parse failed"));
            goto fail;
        }

        /* Parse and fetch MAC address */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac address failed"));
            goto fail;
        }
        memcpy(pReqMsg->ap[i].bssid, nla_data(
                tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID]),
                sizeof(tSirMacAddr));

        /* Parse and fetch low RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr low RSSI failed"));
            goto fail;
        }
        pReqMsg->ap[i].low = nla_get_s32(
             tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW]);
        hddLog(VOS_TRACE_LEVEL_INFO, FL("RSSI low (%d)"), pReqMsg->ap[i].low);

        /* Parse and fetch high RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr high RSSI failed"));
            goto fail;
        }
        pReqMsg->ap[i].high = nla_get_s32(
            tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH]);
        hddLog(VOS_TRACE_LEVEL_INFO,
                                    FL("RSSI High (%d)"), pReqMsg->ap[i].high);

        /* Parse and fetch channel */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_CHANNEL]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr channel failed"));
            goto fail;
        }
        pReqMsg->ap[i].channel = nla_get_u32(
              tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_CHANNEL]);
        hddLog(VOS_TRACE_LEVEL_INFO,
                           FL("Channel (%u)"), pReqMsg->ap[i].channel);
        i++;
    }

    status = sme_SetSignificantChange(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("sme_SetSignificantChange failed(err=%d)"), status);
        vos_mem_free(pReqMsg);
        return -EINVAL;
    }
    vos_mem_free(pReqMsg);
    hddLog(VOS_TRACE_LEVEL_ERROR, FL(" Exiting"));
    return 0;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

static int wlan_hdd_cfg80211_extscan_get_valid_channels(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    hdd_context_t *pHddCtx                               = wiphy_priv(wiphy);
    tANI_U32 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
    tANI_U8 numChannels                                  = 0;
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    tANI_U32 requestId;
    tWifiBand wifiBand;
    eHalStatus status;
    struct sk_buff *replySkb;
    tANI_U8 i;

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering"));
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                  data, dataLen,
                  wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }
    requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), requestId);

    /* Parse and fetch wifi band */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_WIFI_BAND])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr wifi band failed"));
        return -EINVAL;
    }
    wifiBand = nla_get_u32(
     tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_WIFI_BAND]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Wifi band (%d)"), wifiBand);

    status = sme_GetValidChannelsByBand((tHalHandle)(pHddCtx->hHal),
                                        wifiBand, ChannelList,
                                        &numChannels);
    if (eHAL_STATUS_SUCCESS != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
           FL("sme_GetValidChannelsByBand failed (err=%d)"), status);
        return -EINVAL;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Number of channels (%d)"), numChannels);
    for (i = 0; i < numChannels; i++)
        hddLog(VOS_TRACE_LEVEL_INFO, "Channel: %u ", ChannelList[i]);

    replySkb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
                                                    sizeof(u32) * numChannels +
                                                    NLMSG_HDRLEN);

    if (!replySkb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("valid channels: buffer alloc fail"));
        return -EINVAL;
    }
    if (nla_put_u32(replySkb,
                QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_CHANNELS,
                numChannels) ||
            nla_put(replySkb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CHANNELS,
                sizeof(u32) * numChannels, ChannelList)) {

        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        kfree_skb(replySkb);
        return -EINVAL;
    }

    return cfg80211_vendor_cmd_reply(replySkb);
}

static int wlan_hdd_cfg80211_extscan_start(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    tpSirEXTScanStartReqParams pReqMsg = NULL;
    struct net_device *dev                  = wdev->netdev;
    hdd_adapter_t *pAdapter                 = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                  = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *channel[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *buckets;
    struct nlattr *channels;
    int rem1;
    int rem2;
    eHalStatus status;
    tANI_U32 j = 0, index = 0;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, dataLen,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }

    pReqMsg = (tpSirEXTScanStartReqParams)
                                     vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("vos_mem_malloc failed"));
       return -ENOMEM;
    }

    pReqMsg->requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), pReqMsg->requestId);

    pReqMsg->sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id (%d)"), pReqMsg->sessionId);

    /* Parse and fetch base period */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_BASE_PERIOD]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr base period failed"));
        goto fail;
    }
    pReqMsg->basePeriod = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_BASE_PERIOD]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Base Period (%d)"),
                                         pReqMsg->basePeriod);

    /* Parse and fetch max AP per scan */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_MAX_AP_PER_SCAN]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr max_ap_per_scan failed"));
        goto fail;
    }
    pReqMsg->maxAPperScan = nla_get_u32(
             tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_MAX_AP_PER_SCAN]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Max AP per Scan (%d)"),
                                         pReqMsg->maxAPperScan);

    /* Parse and fetch report threshold */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr report_threshold failed"));
        goto fail;
    }
    pReqMsg->reportThreshold = nla_get_u8(
            tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Report Threshold (%d)"),
                                         pReqMsg->reportThreshold);

    /* Parse and fetch number of buckets */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr number of buckets failed"));
        goto fail;
    }
    pReqMsg->numBuckets = nla_get_u8(
                 tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS]);
    if (pReqMsg->numBuckets > WLAN_EXTSCAN_MAX_BUCKETS) {
        hddLog(VOS_TRACE_LEVEL_WARN, FL("Exceeded MAX number of buckets "
          "Setting numBuckets to %u"), WLAN_EXTSCAN_MAX_BUCKETS);
        pReqMsg->numBuckets = WLAN_EXTSCAN_MAX_BUCKETS;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Number of Buckets (%d)"),
                                         pReqMsg->numBuckets);
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr bucket spec failed"));
        goto fail;
    }

    nla_for_each_nested(buckets,
                tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC], rem1) {
        if(nla_parse(bucket,
            QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
            nla_data(buckets), nla_len(buckets), NULL)) { //policy
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla_parse failed"));
            goto fail;
        }

        /* Parse and fetch bucket spec */
        if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_INDEX]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr bucket index failed"));
            goto fail;
        }

        pReqMsg->buckets[index].bucket = nla_get_u8(
                bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_INDEX]);

        hddLog(VOS_TRACE_LEVEL_INFO, FL("Bucket spec Index (%d)"),
                pReqMsg->buckets[index].bucket);

        /* Parse and fetch wifi band */
        if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BAND]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr wifi band failed"));
            goto fail;
        }
        pReqMsg->buckets[index].band = nla_get_u8(
                         bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BAND]);
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Wifi band (%d)"),
                           pReqMsg->buckets[index].band);

        /* Parse and fetch period */
        if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_PERIOD]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr period failed"));
            goto fail;
        }
        pReqMsg->buckets[index].period = nla_get_u32(
                       bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_PERIOD]);
        hddLog(VOS_TRACE_LEVEL_INFO, FL("period (%d)"),
                           pReqMsg->buckets[index].period);

        /* Parse and fetch report events */
        if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_REPORT_EVENTS]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr report events failed"));
            goto fail;
        }
        pReqMsg->buckets[index].reportEvents = nla_get_u8(
                bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_REPORT_EVENTS]);
        hddLog(VOS_TRACE_LEVEL_INFO, FL("report events (%d)"),
                           pReqMsg->buckets[index].reportEvents);

        /* Parse and fetch number of channels */
        if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS])
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr num channels failed"));
            goto fail;
        }
        pReqMsg->buckets[index].numChannels = nla_get_u32(
            bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS]);
        hddLog(VOS_TRACE_LEVEL_INFO, FL("num channels (%d)"),
                           pReqMsg->buckets[index].numChannels);

        if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC]) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr channel spec failed"));
            goto fail;
        }

        j = 0;
        nla_for_each_nested(channels,
                      bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC], rem2) {
            if(nla_parse(channel,
                QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                nla_data(channels), nla_len(channels),
                NULL)) { //wlan_hdd_extscan_config_policy here
                hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla_parse failed"));
                goto fail;
            }

            /* Parse and fetch channel */
            if (!channel[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL]) {
                hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr channel failed"));
                goto fail;
            }
            pReqMsg->buckets[index].channels[j].channel = nla_get_u32(
                   channel[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL]);
            hddLog(VOS_TRACE_LEVEL_INFO, FL("channel (%u)"),
                         pReqMsg->buckets[index].channels[j].channel);

            /* Parse and fetch dwell time */
            if (!channel[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME]) {
                hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr dwelltime failed"));
                goto fail;
            }
            pReqMsg->buckets[index].channels[j].dwellTimeMs = nla_get_u32(
                 channel[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME]);
            hddLog(VOS_TRACE_LEVEL_INFO, FL("Dwell time (%u ms)"),
                     pReqMsg->buckets[index].channels[j].dwellTimeMs);

            /* Parse and fetch channel spec passive */
            if (!channel[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE]) {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                                     FL("attr channel spec passive failed"));
                goto fail;
            }
            pReqMsg->buckets[index].channels[j].passive = nla_get_u8(
                   channel[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE]);
            hddLog(VOS_TRACE_LEVEL_INFO, FL("Chnl spec passive (%u)"),
                     pReqMsg->buckets[index].channels[j].passive);
            j++;
        }
        index++;
    }
    status = sme_EXTScanStart(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                     FL("sme_EXTScanStart failed(err=%d)"), status);
        vos_mem_free(pReqMsg);
        return -EINVAL;
    }

    vos_mem_free(pReqMsg);
    return 0;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

static int wlan_hdd_cfg80211_extscan_stop(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    tSirEXTScanStopReqParams reqMsg;
    struct net_device *dev                  = wdev->netdev;
    hdd_adapter_t *pAdapter                 = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                  = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    eHalStatus status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, dataLen,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }

    reqMsg.requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), reqMsg.requestId);

    reqMsg.sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id (%d)"), reqMsg.sessionId);

    status = sme_EXTScanStop(pHddCtx->hHal, &reqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                    FL("sme_EXTScanStop failed(err=%d)"), status);
        return -EINVAL;
    }

    return 0;
}

static int wlan_hdd_cfg80211_extscan_reset_bssid_hotlist(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    tSirEXTScanResetBssidHotlistReqParams reqMsg;
    struct net_device *dev                       = wdev->netdev;
    hdd_adapter_t *pAdapter                      = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                       = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    eHalStatus status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, dataLen,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }

    reqMsg.requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), reqMsg.requestId);

    reqMsg.sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id (%d)"), reqMsg.sessionId);

    status = sme_ResetBssHotlist(pHddCtx->hHal, &reqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                          FL("sme_ResetBssHotlist failed(err=%d)"), status);
        return -EINVAL;
    }

    return 0;
}

static int wlan_hdd_cfg80211_extscan_reset_significant_change(
                                        struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        void *data, int dataLen)
{
    tSirEXTScanResetSignificantChangeReqParams reqMsg;
    struct net_device *dev                       = wdev->netdev;
    hdd_adapter_t *pAdapter                      = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                       = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    eHalStatus status;

    hddLog(VOS_TRACE_LEVEL_INFO, FL(" Entering"));
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    /* check the EXTScan Capability */
    if ( (TRUE != pHddCtx->cfg_ini->fEnableEXTScan) ||
         (TRUE != sme_IsFeatureSupportedByFW(EXTENDED_SCAN)))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("EXTScan not enabled/supported by Firmware"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, dataLen,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        return -EINVAL;
    }


    reqMsg.requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id (%d)"), reqMsg.requestId);

    reqMsg.sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id (%d)"), reqMsg.sessionId);

    status = sme_ResetSignificantChange(pHddCtx->hHal, &reqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                   FL("sme_ResetSignificantChange failed(err=%d)"), status);
        return -EINVAL;
    }

    hddLog(VOS_TRACE_LEVEL_ERROR, FL(" Exiting"));
    return 0;
}

#endif /* WLAN_FEATURE_EXTSCAN */

/*EXT TDLS*/
static const struct nla_policy
wlan_hdd_tdls_config_enable_policy[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR] = {.type = NLA_UNSPEC },
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS] =
                                                       {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS] = {.type = NLA_S32 },

};

static const struct nla_policy
wlan_hdd_tdls_config_disable_policy[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR] = {.type = NLA_UNSPEC },

};

static const struct nla_policy
wlan_hdd_tdls_config_state_change_policy[
                    QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAC_ADDR] = {.type = NLA_UNSPEC },
    [QCA_WLAN_VENDOR_ATTR_TDLS_NEW_STATE] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_STATE_REASON] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_STATE_CHANNEL] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_STATE_GLOBAL_OPERATING_CLASS] =
                                                {.type = NLA_S32 },

};

static const struct nla_policy
wlan_hdd_tdls_config_get_status_policy[
                     QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR] = {.type = NLA_UNSPEC },
    [QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_STATE] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_REASON] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_CHANNEL] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_GLOBAL_OPERATING_CLASS]
                                                   = {.type = NLA_S32 },

};

static const struct nla_policy
wlan_hdd_mac_config[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_MAX+1] =
{
    [QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI] = {.type = NLA_UNSPEC },
};

static int wlan_hdd_cfg80211_set_spoofed_mac_oui(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        void *data,
        int data_len)
{

    hdd_context_t *pHddCtx      = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_MAX + 1];

    if (0 != wlan_hdd_validate_context(pHddCtx)){
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("hdd Ctx invalid while spoof macAddr"));
        return -EINVAL;
    }
    if (FALSE == pHddCtx->cfg_ini->enableMacSpoofing) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("MAC_SPOOFED_SCAN disabled in ini"));
        return -ENOTSUPP;
     }
    if (TRUE != sme_IsFeatureSupportedByFW(MAC_SPOOFED_SCAN)){
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("MAC_SPOOFED_SCAN not supported by FW"));
        return -ENOTSUPP;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_MAX,
                data, data_len, wlan_hdd_mac_config)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch mac address */
    if (!tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac addr failed"));
        return -EINVAL;
    }

    memcpy(pHddCtx->spoofMacAddr.randomMacAddr.bytes, nla_data(
                tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI]),
            VOS_MAC_ADDR_LAST_3_BYTES);

    vos_trace_hex_dump( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,nla_data(
            tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI]),
            VOS_MAC_ADDR_FIRST_3_BYTES);

    if (VOS_STATUS_SUCCESS != vos_randomize_n_bytes(
        (void *)(&pHddCtx->spoofMacAddr.randomMacAddr.bytes[3]),
        VOS_MAC_ADDR_LAST_3_BYTES)) {
        hddLog(LOGE, FL("Failed to generate random Mac Addr"));
    }
    vos_trace_hex_dump( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
        pHddCtx->spoofMacAddr.randomMacAddr.bytes,
            VOS_MAC_ADDR_SIZE);

    if (eHAL_STATUS_SUCCESS != sme_SpoofMacAddrReq(pHddCtx->hHal,
                &pHddCtx->spoofMacAddr.randomMacAddr)) {
        hddLog(LOGE, FL("Failed to send Spoof Mac Addr to FW"));
    }

    return 0;
}

static int wlan_hdd_cfg80211_exttdls_get_status(struct wiphy *wiphy,
                                                struct wireless_dev *wdev,
                                                void *data,
                                                int data_len)
{
    u8 peer[6]                                 = {0};
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAX + 1];
    eHalStatus ret;
    tANI_S32 state;
    tANI_S32 reason;
    tANI_S32 global_operating_class = 0;
    tANI_S32 channel = 0;
    struct sk_buff *skb         = NULL;

    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {

        return -EINVAL;
    }
    if (pHddCtx->cfg_ini->fTDLSExternalControl == FALSE) {

        return -ENOTSUPP;
    }
    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAX,
                    data, data_len,
                    wlan_hdd_tdls_config_get_status_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch mac address */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac addr failed"));
        return -EINVAL;
    }

    memcpy(peer, nla_data(
           tb[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR]),
           sizeof(peer));
    hddLog(VOS_TRACE_LEVEL_INFO, FL(MAC_ADDRESS_STR),MAC_ADDR_ARRAY(peer));

    ret = wlan_hdd_tdls_get_status(pAdapter, peer, &state, &reason);

    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("get status Failed"));
        return -EINVAL;
    }
    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
                                              4 * sizeof(s32) +
                                              NLMSG_HDRLEN);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
        return -EINVAL;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Reason (%d) Status (%d) class (%d) channel (%d) peer" MAC_ADDRESS_STR),
                                 reason,
                                 state,
                                 global_operating_class,
                                 channel,
                                 MAC_ADDR_ARRAY(peer));
    if (nla_put_s32(skb,
                    QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_STATE,
                    state) ||
        nla_put_s32(skb,
                    QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_REASON,
                    reason) ||
        nla_put_s32(skb,
                    QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_GLOBAL_OPERATING_CLASS,
                    global_operating_class) ||
        nla_put_s32(skb,
                    QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_CHANNEL,
                    channel)) {

        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
    kfree_skb(skb);
    return -EINVAL;
}

static int wlan_hdd_cfg80211_exttdls_callback(tANI_U8* mac,
                                              tANI_S32 state,
                                              tANI_S32 reason,
                                              void *ctx)
{
    hdd_adapter_t* pAdapter       = (hdd_adapter_t*)ctx;
    hdd_context_t *pHddCtx        = WLAN_HDD_GET_CTX(pAdapter);
    struct sk_buff *skb           = NULL;
    tANI_S32 global_operating_class = 0;
    tANI_S32 channel = 0;

    if (wlan_hdd_validate_context(pHddCtx)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "));
        return -EINVAL;
    }

    if (pHddCtx->cfg_ini->fTDLSExternalControl == FALSE) {

        return -ENOTSUPP;
    }
    skb = cfg80211_vendor_event_alloc(
                            pHddCtx->wiphy,
                            EXTTDLS_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                            QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE_CHANGE_INDEX,
                            GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return -EINVAL;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Reason: (%d) Status: (%d) Class: (%d) Channel: (%d)",
           reason,
           state,
           global_operating_class,
           channel);
    hddLog(VOS_TRACE_LEVEL_WARN, "tdls peer " MAC_ADDRESS_STR,
                                          MAC_ADDR_ARRAY(mac));

    if (nla_put(skb,
                QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAC_ADDR,
                VOS_MAC_ADDR_SIZE, mac) ||
        nla_put_s32(skb,
                    QCA_WLAN_VENDOR_ATTR_TDLS_NEW_STATE,
                    state) ||
        nla_put_s32(skb,
                    QCA_WLAN_VENDOR_ATTR_TDLS_STATE_REASON,
                    reason) ||
        nla_put_s32(skb,
                    QCA_WLAN_VENDOR_ATTR_TDLS_STATE_CHANNEL,
                    channel) ||
        nla_put_s32(skb,
                    QCA_WLAN_VENDOR_ATTR_TDLS_STATE_GLOBAL_OPERATING_CLASS,
                    global_operating_class)
        ) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return (0);

nla_put_failure:
    kfree_skb(skb);
    return -EINVAL;
}

static int wlan_hdd_cfg80211_exttdls_enable(struct wiphy *wiphy,
                                            struct wireless_dev *wdev,
                                            void *data,
                                            int data_len)
{
    u8 peer[6]                                 = {0};
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX + 1];
    eHalStatus status;
    tdls_req_params_t   pReqMsg = {0};

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    if (pHddCtx->cfg_ini->fTDLSExternalControl == FALSE) {

        return -ENOTSUPP;
    }
    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX,
                    data, data_len,
                    wlan_hdd_tdls_config_enable_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch mac address */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac addr failed"));
        return -EINVAL;
    }

    memcpy(peer, nla_data(
                tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR]),
                sizeof(peer));
    hddLog(VOS_TRACE_LEVEL_INFO, FL(MAC_ADDRESS_STR),MAC_ADDR_ARRAY(peer));

    /* Parse and fetch channel */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr channel failed"));
        return -EINVAL;
    }
    pReqMsg.channel = nla_get_s32(
         tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Channel Num (%d)"), pReqMsg.channel);

    /* Parse and fetch global operating class */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr operating class failed"));
        return -EINVAL;
    }
    pReqMsg.global_operating_class = nla_get_s32(
        tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Operating class (%d)"),
                                     pReqMsg.global_operating_class);

    /* Parse and fetch latency ms */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr latency failed"));
        return -EINVAL;
    }
    pReqMsg.max_latency_ms = nla_get_s32(
        tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Latency (%d)"),
                                     pReqMsg.max_latency_ms);

    /* Parse and fetch required bandwidth kbps */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr bandwidth failed"));
        return -EINVAL;
    }

    pReqMsg.min_bandwidth_kbps = nla_get_s32(
        tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Bandwidth (%d)"),
                                     pReqMsg.min_bandwidth_kbps);

    return (wlan_hdd_tdls_extctrl_config_peer(pAdapter,
                                 peer,
                                 wlan_hdd_cfg80211_exttdls_callback));
}

static int wlan_hdd_cfg80211_exttdls_disable(struct wiphy *wiphy,
                                             struct wireless_dev *wdev,
                                             void *data,
                                             int data_len)
{
    u8 peer[6]                                 = {0};
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAX + 1];
    eHalStatus status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    if (pHddCtx->cfg_ini->fTDLSExternalControl == FALSE) {

        return -ENOTSUPP;
    }
    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAX,
                    data, data_len,
                    wlan_hdd_tdls_config_disable_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }
    /* Parse and fetch mac address */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac addr failed"));
        return -EINVAL;
    }

    memcpy(peer, nla_data(
                tb[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR]),
                sizeof(peer));
    hddLog(VOS_TRACE_LEVEL_INFO, FL(MAC_ADDRESS_STR),MAC_ADDR_ARRAY(peer));

    return (wlan_hdd_tdls_extctrl_deconfig_peer(pAdapter, peer));
}

static int
wlan_hdd_cfg80211_get_supported_features(struct wiphy *wiphy,
                                         struct wireless_dev *wdev,
                                         void *data, int data_len)
{
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx      = wiphy_priv(wiphy);
    struct sk_buff *skb         = NULL;
    tANI_U32       fset         = 0;

    if (wiphy->interface_modes & BIT(NL80211_IFTYPE_STATION)) {
        hddLog(LOG1, FL("Infra Station mode is supported by driver"));
        fset |= WIFI_FEATURE_INFRA;
    }

    if (TRUE == hdd_is_5g_supported(pHddCtx)) {
        hddLog(LOG1, FL("INFRA_5G is supported by firmware"));
        fset |= WIFI_FEATURE_INFRA_5G;
    }

#ifdef WLAN_FEATURE_P2P
    if ((wiphy->interface_modes & BIT(NL80211_IFTYPE_P2P_CLIENT)) &&
        (wiphy->interface_modes & BIT(NL80211_IFTYPE_P2P_GO))) {
        hddLog(LOG1, FL("WiFi-Direct is supported by driver"));
        fset |= WIFI_FEATURE_P2P;
    }
#endif

    /* Soft-AP is supported currently by default */
    fset |= WIFI_FEATURE_SOFT_AP;

#ifdef WLAN_FEATURE_EXTSCAN
    if ((TRUE == pHddCtx->cfg_ini->fEnableEXTScan) &&
            sme_IsFeatureSupportedByFW(EXTENDED_SCAN)) {
        hddLog(LOG1, FL("EXTScan is supported by firmware"));
        fset |= WIFI_FEATURE_EXTSCAN;
    }
#endif

#ifdef WLAN_FEATURE_NAN
    if (sme_IsFeatureSupportedByFW(NAN)) {
        hddLog(LOG1, FL("NAN is supported by firmware"));
        fset |= WIFI_FEATURE_NAN;
    }
#endif

    /* D2D RTT is not supported currently by default */
    if (sme_IsFeatureSupportedByFW(RTT)) {
        hddLog(LOG1, FL("RTT is supported by firmware"));
        fset |= WIFI_FEATURE_D2AP_RTT;
    }

#ifdef FEATURE_WLAN_BATCH_SCAN
    if (fset & WIFI_FEATURE_EXTSCAN) {
        hddLog(LOG1, FL("Batch scan is supported as extscan is supported"));
        fset &= ~WIFI_FEATURE_BATCH_SCAN;
    } else if (sme_IsFeatureSupportedByFW(BATCH_SCAN)) {
        hddLog(LOG1, FL("Batch scan is supported by firmware"));
        fset |= WIFI_FEATURE_BATCH_SCAN;
    }
#endif

#ifdef FEATURE_WLAN_SCAN_PNO
    if (pHddCtx->cfg_ini->configPNOScanSupport &&
        (eHAL_STATUS_SUCCESS == wlan_hdd_is_pno_allowed(pAdapter))) {
        hddLog(LOG1, FL("PNO is supported by firmware"));
        fset |= WIFI_FEATURE_PNO;
    }
#endif

    /* STA+STA is supported currently by default */
    fset |= WIFI_FEATURE_ADDITIONAL_STA;

#ifdef FEATURE_WLAN_TDLS
    if ((TRUE == pHddCtx->cfg_ini->fEnableTDLSSupport) &&
        sme_IsFeatureSupportedByFW(TDLS)) {
        hddLog(LOG1, FL("TDLS is supported by firmware"));
        fset |= WIFI_FEATURE_TDLS;
    }

    /* TDLS_OFFCHANNEL is not supported currently by default */
#endif

#ifdef WLAN_AP_STA_CONCURRENCY
    /* AP+STA concurrency is supported currently by default */
    fset |= WIFI_FEATURE_AP_STA;
#endif

    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(fset) +
                                              NLMSG_HDRLEN);

    if (!skb) {
        hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
        return -EINVAL;
    }
    hddLog(LOG1, FL("Supported Features : 0x%x"), fset);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_FEATURE_SET, fset)) {
        hddLog(LOGE, FL("nla put fail"));
        goto nla_put_failure;
    }

    return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
    kfree_skb(skb);
    return -EINVAL;
}

static int
wlan_hdd_cfg80211_get_concurrency_matrix(struct wiphy *wiphy,
                                         struct wireless_dev *wdev,
                                         void *data, int data_len)
{
    uint32_t feature_set_matrix[WLAN_HDD_MAX_FEATURE_SET] = {0};
    uint8_t i, feature_sets, max_feature_sets;
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX + 1];
    struct sk_buff *reply_skb;

    ENTER();

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX,
                  data, data_len, NULL)) {
        hddLog(LOGE, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch max feature set */
    if (!tb[QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_CONFIG_PARAM_SET_SIZE_MAX]) {
        hddLog(LOGE, FL("Attr max feature set size failed"));
        return -EINVAL;
    }
    max_feature_sets = nla_get_u32(
     tb[QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_CONFIG_PARAM_SET_SIZE_MAX]);
    hddLog(LOG1, FL("Max feature set size (%d)"), max_feature_sets);

    /* Fill feature combination matrix */
    feature_sets = 0;
    if (feature_sets >= WLAN_HDD_MAX_FEATURE_SET) goto max_buffer_err;
    feature_set_matrix[feature_sets++] = WIFI_FEATURE_INFRA |
                                         WIFI_FEATURE_P2P;

    if (feature_sets >= WLAN_HDD_MAX_FEATURE_SET) goto max_buffer_err;
    feature_set_matrix[feature_sets++] = WIFI_FEATURE_INFRA |
                                         WIFI_FEATURE_SOFT_AP;

    if (feature_sets >= WLAN_HDD_MAX_FEATURE_SET) goto max_buffer_err;
    feature_set_matrix[feature_sets++] = WIFI_FEATURE_P2P |
                                         WIFI_FEATURE_SOFT_AP;

    if (feature_sets >= WLAN_HDD_MAX_FEATURE_SET) goto max_buffer_err;
    feature_set_matrix[feature_sets++] = WIFI_FEATURE_INFRA |
                                         WIFI_FEATURE_SOFT_AP |
                                         WIFI_FEATURE_P2P;

    /* Add more feature combinations here */

    feature_sets = VOS_MIN(feature_sets, max_feature_sets);
    hddLog(LOG1, FL("Number of feature sets (%d)"), feature_sets);
    hddLog(LOG1, "Feature set matrix");
    for (i = 0; i < feature_sets; i++)
        hddLog(LOG1, "[%d] 0x%02X", i, feature_set_matrix[i]);

    reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
                                                    sizeof(u32) * feature_sets +
                                                    NLMSG_HDRLEN);

    if (reply_skb) {
        if (nla_put_u32(reply_skb,
                        QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET_SIZE,
                        feature_sets) ||
            nla_put(reply_skb,
                    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET,
                    sizeof(u32) * feature_sets, feature_set_matrix)) {
            hddLog(LOGE, FL("nla put fail"));
            kfree_skb(reply_skb);
            return -EINVAL;
        }

        return cfg80211_vendor_cmd_reply(reply_skb);
    }
    hddLog(LOGE, FL("Feature set matrix: buffer alloc fail"));
    return -ENOMEM;

max_buffer_err:
    hddLog(LOGE, FL("Feature set max buffer size reached. feature_sets(%d) max(%d)"),
           feature_sets, WLAN_HDD_MAX_FEATURE_SET);
    return -EINVAL;
}

static const struct nla_policy
wlan_hdd_set_no_dfs_flag_config_policy[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX
 +1] =
{
    [QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG] = {.type = NLA_U32 },
};

static int wlan_hdd_cfg80211_disable_dfs_channels(struct wiphy *wiphy,
                                            struct wireless_dev *wdev,
                                            void *data,
                                            int data_len)
{
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX + 1];
    eHalStatus status;
    u32 dfsFlag = 0;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX,
                    data, data_len,
                    wlan_hdd_set_no_dfs_flag_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch required bandwidth kbps */
    if (!tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr dfs flag failed"));
        return -EINVAL;
    }

    dfsFlag = nla_get_u32(
        tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL(" DFS flag (%d)"),
                                     dfsFlag);

    pHddCtx->disable_dfs_flag = dfsFlag;

    sme_disable_dfs_channel(hHal, dfsFlag);
    sme_FilterScanResults(hHal, pAdapter->sessionId);
    return 0;
}

const struct
nla_policy qca_wlan_vendor_attr[QCA_WLAN_VENDOR_ATTR_MAX+1] =
{
    [QCA_WLAN_VENDOR_ATTR_ROAMING_POLICY] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_MAC_ADDR]       = { .type = NLA_UNSPEC },
};

static int wlan_hdd_cfg80211_firmware_roaming(struct wiphy *wiphy,
            struct wireless_dev *wdev, void *data, int data_len)
{

    u8 bssid[6]                                 = {0};
    hdd_context_t *pHddCtx                      = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MAX + 1];
    eHalStatus status = eHAL_STATUS_SUCCESS;
    v_U32_t isFwrRoamEnabled = FALSE;
    int ret;

    if (NULL == pHddCtx) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }

    ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_MAX,
                    data, data_len,
                    qca_wlan_vendor_attr);
    if (ret){
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch Enable flag */
    if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_POLICY]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr enable failed"));
        return -EINVAL;
    }

    isFwrRoamEnabled = nla_get_u32(
         tb[QCA_WLAN_VENDOR_ATTR_ROAMING_POLICY]);

    hddLog(VOS_TRACE_LEVEL_INFO, FL("isFwrRoamEnabled (%d)"), isFwrRoamEnabled);

    /* Parse and fetch bssid */
    if (!tb[QCA_WLAN_VENDOR_ATTR_MAC_ADDR]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr bss id failed"));
        return -EINVAL;
    }

    memcpy(bssid, nla_data(
                tb[QCA_WLAN_VENDOR_ATTR_MAC_ADDR]),
                sizeof(bssid));
    hddLog(VOS_TRACE_LEVEL_INFO, FL(MAC_ADDRESS_STR),MAC_ADDR_ARRAY(bssid));

    //Update roaming
    status = sme_ConfigFwrRoaming((tHalHandle)(pHddCtx->hHal), isFwrRoamEnabled);
    return status;
}

const struct wiphy_vendor_command hdd_wiphy_vendor_commands[] =
{
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ROAMING,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_firmware_roaming
    },
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV |
            WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_ll_stats_clear
    },

    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV |
            WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_ll_stats_set
    },

    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV |
            WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_ll_stats_get
    },
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
#ifdef WLAN_FEATURE_EXTSCAN
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_start
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_stop
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_VALID_CHANNELS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_hdd_cfg80211_extscan_get_valid_channels
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_get_capabilities
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_get_cached_results
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_set_bssid_hotlist
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_reset_bssid_hotlist
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_set_significant_change
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_reset_significant_change
    },
#endif /* WLAN_FEATURE_EXTSCAN */
/*EXT TDLS*/
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_ENABLE,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_exttdls_enable
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_DISABLE,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_exttdls_disable
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_hdd_cfg80211_exttdls_get_status
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_SUPPORTED_FEATURES,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_hdd_cfg80211_get_supported_features
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NO_DFS_FLAG,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_hdd_cfg80211_disable_dfs_channels
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MAC_OUI,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_hdd_cfg80211_set_spoofed_mac_oui
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_CONCURRENCY_MATRIX,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_hdd_cfg80211_get_concurrency_matrix
    },
};

/* vendor specific events */
static const
struct nl80211_vendor_cmd_info wlan_hdd_cfg80211_vendor_events[] =
{
#ifdef FEATURE_WLAN_CH_AVOID
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY
    },
#endif /* FEATURE_WLAN_CH_AVOID Index = 0*/
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
    {
        /* Index = 1*/
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET
    },
    {
        /* Index = 2*/
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET
    },
    {
        /* Index = 3*/
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR
    },
    {
        /* Index = 4*/
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_RADIO_RESULTS
    },
    {
        /* Index = 5*/
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_IFACE_RESULTS
    },
    {
        /* Index = 6*/
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_PEERS_RESULTS
    },
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
#ifdef WLAN_FEATURE_EXTSCAN
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_RESULTS_AVAILABLE
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_FULL_SCAN_RESULT
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_EVENT
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_FOUND
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SIGNIFICANT_CHANGE
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE
    },
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE
    },
#endif /* WLAN_FEATURE_EXTSCAN */
/*EXT TDLS*/
    {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE
    },
};

/*
 * FUNCTION: wlan_hdd_cfg80211_wiphy_alloc
 * This function is called by hdd_wlan_startup()
 * during initialization.
 * This function is used to allocate wiphy structure.
 */
struct wiphy *wlan_hdd_cfg80211_wiphy_alloc(int priv_size)
{
    struct wiphy *wiphy;
    ENTER();
    /*
     *   Create wiphy device
     */
    wiphy = wiphy_new(&wlan_hdd_cfg80211_ops, priv_size);

    if (!wiphy)
    {
        /* Print error and jump into err label and free the memory */
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: wiphy init failed", __func__);
        return NULL;
    }


    return wiphy;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_update_band
 * This function is called from the supplicant through a
 * private ioctl to change the band value
 */
int wlan_hdd_cfg80211_update_band(struct wiphy *wiphy, eCsrBand eBand)
{
    int i, j;
    eNVChannelEnabledType channelEnabledState;

    ENTER();

    for (i = 0; i < IEEE80211_NUM_BANDS; i++)
    {

        if (NULL == wiphy->bands[i])
        {
           hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy->bands[i] is NULL, i = %d",
                  __func__, i);
           continue;
        }

        for (j = 0; j < wiphy->bands[i]->n_channels; j++)
        {
            struct ieee80211_supported_band *band = wiphy->bands[i];

            channelEnabledState = vos_nv_getChannelEnabledState(
                                  band->channels[j].hw_value);

            if (IEEE80211_BAND_2GHZ == i && eCSR_BAND_5G == eBand) // 5G only
            {
                band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
                continue;
            }
            else if (IEEE80211_BAND_5GHZ == i && eCSR_BAND_24 == eBand) // 2G only
            {
                band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
                continue;
            }

            if (NV_CHANNEL_DISABLE == channelEnabledState ||
                NV_CHANNEL_INVALID == channelEnabledState)
            {
                band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
            }
            else if (NV_CHANNEL_DFS == channelEnabledState)
            {
                band->channels[j].flags &= ~IEEE80211_CHAN_DISABLED;
                band->channels[j].flags |= IEEE80211_CHAN_RADAR;
            }
            else
            {
                band->channels[j].flags &= ~(IEEE80211_CHAN_DISABLED
                                             |IEEE80211_CHAN_RADAR);
            }
        }
    }
    return 0;
}
/*
 * FUNCTION: wlan_hdd_cfg80211_init
 * This function is called by hdd_wlan_startup()
 * during initialization.
 * This function is used to initialize and register wiphy structure.
 */
int wlan_hdd_cfg80211_init(struct device *dev,
                               struct wiphy *wiphy,
                               hdd_config_t *pCfg
                               )
{
    int i, j;
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);

    ENTER();

    /* Now bind the underlying wlan device with wiphy */
    set_wiphy_dev(wiphy, dev);

    wiphy->mgmt_stypes = wlan_hdd_txrx_stypes;

#ifndef CONFIG_ENABLE_LINUX_REG
    /* the flag for the other case would be initialzed in
       vos_init_wiphy_from_nv_bin */
    wiphy->flags |= WIPHY_FLAG_STRICT_REGULATORY;
#endif

    /* This will disable updating of NL channels from passive to
     * active if a beacon is received on passive channel. */
    wiphy->flags |=   WIPHY_FLAG_DISABLE_BEACON_HINTS;



#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
    wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME
                 |  WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD
                 |  WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL
                    | WIPHY_FLAG_OFFCHAN_TX;
    wiphy->country_ie_pref = NL80211_COUNTRY_IE_IGNORE_CORE;
#endif

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
    if (pCfg->isFastTransitionEnabled
#ifdef FEATURE_WLAN_LFR
       || pCfg->isFastRoamIniFeatureEnabled
#endif
#ifdef FEATURE_WLAN_ESE
       || pCfg->isEseIniFeatureEnabled
#endif
    )
    {
        wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;
    }
#endif
#ifdef FEATURE_WLAN_TDLS
    wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS
                 |  WIPHY_FLAG_TDLS_EXTERNAL_SETUP;
#endif
#ifdef FEATURE_WLAN_SCAN_PNO
    if (pCfg->configPNOScanSupport)
    {
        wiphy->flags |= WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
        wiphy->max_sched_scan_ssids = SIR_PNO_MAX_SUPP_NETWORKS;
        wiphy->max_match_sets       = SIR_PNO_MAX_SUPP_NETWORKS;
        wiphy->max_sched_scan_ie_len = SIR_MAC_MAX_IE_LENGTH;
    }
#endif/*FEATURE_WLAN_SCAN_PNO*/

#ifdef CONFIG_ENABLE_LINUX_REG
    /* even with WIPHY_FLAG_CUSTOM_REGULATORY,
       driver can still register regulatory callback and
       it will get regulatory settings in wiphy->band[], but
       driver need to determine what to do with both
       regulatory settings */

    wiphy->reg_notifier = wlan_hdd_linux_reg_notifier;
#else
    wiphy->reg_notifier = wlan_hdd_crda_reg_notifier;
#endif

    wiphy->max_scan_ssids = MAX_SCAN_SSID;

    wiphy->max_scan_ie_len = SIR_MAC_MAX_IE_LENGTH;

    wiphy->max_acl_mac_addrs = MAX_ACL_MAC_ADDRESS;

    /* Supports STATION & AD-HOC modes right now */
    wiphy->interface_modes =   BIT(NL80211_IFTYPE_STATION)
                             | BIT(NL80211_IFTYPE_ADHOC)
                             | BIT(NL80211_IFTYPE_P2P_CLIENT)
                             | BIT(NL80211_IFTYPE_P2P_GO)
                             | BIT(NL80211_IFTYPE_AP);

    if( pCfg->advertiseConcurrentOperation )
    {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
        if( pCfg->enableMCC )
        {
            /* Currently, supports up to two channels */
            wlan_hdd_iface_combination.num_different_channels = 2;

            if( !pCfg->allowMCCGODiffBI )
                wlan_hdd_iface_combination.beacon_int_infra_match = true;

        }
        wiphy->iface_combinations = &wlan_hdd_iface_combination;
        wiphy->n_iface_combinations = 1;
#endif
    }

    /* Before registering we need to update the ht capabilitied based
     * on ini values*/
    if( !pCfg->ShortGI20MhzEnable )
    {
        wlan_hdd_band_2_4_GHZ.ht_cap.cap     &= ~IEEE80211_HT_CAP_SGI_20;
        wlan_hdd_band_5_GHZ.ht_cap.cap       &= ~IEEE80211_HT_CAP_SGI_20;
        wlan_hdd_band_p2p_2_4_GHZ.ht_cap.cap &= ~IEEE80211_HT_CAP_SGI_20;
    }

    if( !pCfg->ShortGI40MhzEnable )
    {
        wlan_hdd_band_5_GHZ.ht_cap.cap       &= ~IEEE80211_HT_CAP_SGI_40;
    }

    if( !pCfg->nChannelBondingMode5GHz )
    {
        wlan_hdd_band_5_GHZ.ht_cap.cap &= ~IEEE80211_HT_CAP_SUP_WIDTH_20_40;
    }

   wiphy->bands[IEEE80211_BAND_2GHZ] = &wlan_hdd_band_2_4_GHZ;
   if (true == hdd_is_5g_supported(pHddCtx))
   {
       wiphy->bands[IEEE80211_BAND_5GHZ] = &wlan_hdd_band_5_GHZ;
   }

   for (i = 0; i < IEEE80211_NUM_BANDS; i++)
   {

       if (NULL == wiphy->bands[i])
       {
          hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy->bands[i] is NULL, i = %d",
                 __func__, i);
          continue;
       }

       for (j = 0; j < wiphy->bands[i]->n_channels; j++)
       {
           struct ieee80211_supported_band *band = wiphy->bands[i];

           if (IEEE80211_BAND_2GHZ == i && eCSR_BAND_5G == pCfg->nBandCapability) // 5G only
           {
               // Enable social channels for P2P
               if (WLAN_HDD_IS_SOCIAL_CHANNEL(band->channels[j].center_freq))
                   band->channels[j].flags &= ~IEEE80211_CHAN_DISABLED;
               else
                   band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
               continue;
           }
           else if (IEEE80211_BAND_5GHZ == i && eCSR_BAND_24 == pCfg->nBandCapability) // 2G only
           {
               band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
               continue;
           }
       }
    }
    /*Initialise the supported cipher suite details*/
    wiphy->cipher_suites = hdd_cipher_suites;
    wiphy->n_cipher_suites = ARRAY_SIZE(hdd_cipher_suites);

    /*signal strength in mBm (100*dBm) */
    wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
    wiphy->max_remain_on_channel_duration = 5000;
#endif

    wiphy->n_vendor_commands = ARRAY_SIZE(hdd_wiphy_vendor_commands);
    wiphy->vendor_commands = hdd_wiphy_vendor_commands;
    wiphy->vendor_events = wlan_hdd_cfg80211_vendor_events;
    wiphy->n_vendor_events = ARRAY_SIZE(wlan_hdd_cfg80211_vendor_events);

    EXIT();
    return 0;
}

/* In this function we are registering wiphy. */
int wlan_hdd_cfg80211_register(struct wiphy *wiphy)
{
    ENTER();
 /* Register our wiphy dev with cfg80211 */
    if (0 > wiphy_register(wiphy))
    {
        /* print error */
        hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy register failed", __func__);
        return -EIO;
    }

    EXIT();
    return 0;
}

/* In this function we are updating channel list when,
   regulatory domain is FCC and country code is US.
   Here In FCC standard 5GHz UNII-1 Bands are indoor only.
   As per FCC smart phone is not a indoor device.
   GO should not opeate on indoor channels */
void wlan_hdd_cfg80211_update_reg_info(struct wiphy *wiphy)
{
    int j;
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    tANI_U8 defaultCountryCode[3] = SME_INVALID_COUNTRY_CODE;
    //Default counrtycode from NV at the time of wiphy initialization.
    if (eHAL_STATUS_SUCCESS != sme_GetDefaultCountryCodeFrmNv(pHddCtx->hHal,
                                  &defaultCountryCode[0]))
    {
       hddLog(LOGE, FL("Failed to get default country code from NV"));
    }
    if ((defaultCountryCode[0]== 'U') && (defaultCountryCode[1]=='S'))
    {
       if (NULL == wiphy->bands[IEEE80211_BAND_5GHZ])
       {
          hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy->bands[IEEE80211_BAND_5GHZ] is NULL",__func__ );
          return;
       }
       for (j = 0; j < wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels; j++)
       {
          struct ieee80211_supported_band *band = wiphy->bands[IEEE80211_BAND_5GHZ];
          // Mark UNII -1 band channel as passive
          if (WLAN_HDD_CHANNEL_IN_UNII_1_BAND(band->channels[j].center_freq))
             band->channels[j].flags |= IEEE80211_CHAN_PASSIVE_SCAN;
       }
    }
}

/* This function registers for all frame which supplicant is interested in */
void wlan_hdd_cfg80211_register_frames(hdd_adapter_t* pAdapter)
{
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    /* Register for all P2P action, public action etc frames */
    v_U16_t type = (SIR_MAC_MGMT_FRAME << 2) | ( SIR_MAC_MGMT_ACTION << 4);

    ENTER();

   /* Right now we are registering these frame when driver is getting
      initialized. Once we will move to 2.6.37 kernel, in which we have
      frame register ops, we will move this code as a part of that */
    /* GAS Initial Request */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)GAS_INITIAL_REQ, GAS_INITIAL_REQ_SIZE );

    /* GAS Initial Response */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)GAS_INITIAL_RSP, GAS_INITIAL_RSP_SIZE );

    /* GAS Comeback Request */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)GAS_COMEBACK_REQ, GAS_COMEBACK_REQ_SIZE );

    /* GAS Comeback Response */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)GAS_COMEBACK_RSP, GAS_COMEBACK_RSP_SIZE );

    /* P2P Public Action */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)P2P_PUBLIC_ACTION_FRAME,
                                  P2P_PUBLIC_ACTION_FRAME_SIZE );

    /* P2P Action */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)P2P_ACTION_FRAME,
                                  P2P_ACTION_FRAME_SIZE );

    /* WNM BSS Transition Request frame */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)WNM_BSS_ACTION_FRAME,
                                  WNM_BSS_ACTION_FRAME_SIZE );

    /* WNM-Notification */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)WNM_NOTIFICATION_FRAME,
                                  WNM_NOTIFICATION_FRAME_SIZE );
}

void wlan_hdd_cfg80211_deregister_frames(hdd_adapter_t* pAdapter)
{
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    /* Register for all P2P action, public action etc frames */
    v_U16_t type = (SIR_MAC_MGMT_FRAME << 2) | ( SIR_MAC_MGMT_ACTION << 4);

    ENTER();

   /* Right now we are registering these frame when driver is getting
      initialized. Once we will move to 2.6.37 kernel, in which we have
      frame register ops, we will move this code as a part of that */
    /* GAS Initial Request */

    sme_DeregisterMgmtFrame(hHal, pAdapter->sessionId, type,
                          (v_U8_t*)GAS_INITIAL_REQ, GAS_INITIAL_REQ_SIZE );

    /* GAS Initial Response */
    sme_DeregisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)GAS_INITIAL_RSP, GAS_INITIAL_RSP_SIZE );

    /* GAS Comeback Request */
    sme_DeregisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)GAS_COMEBACK_REQ, GAS_COMEBACK_REQ_SIZE );

    /* GAS Comeback Response */
    sme_DeregisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)GAS_COMEBACK_RSP, GAS_COMEBACK_RSP_SIZE );

    /* P2P Public Action */
    sme_DeregisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)P2P_PUBLIC_ACTION_FRAME,
                                  P2P_PUBLIC_ACTION_FRAME_SIZE );

    /* P2P Action */
    sme_DeregisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)P2P_ACTION_FRAME,
                                  P2P_ACTION_FRAME_SIZE );
    /* WNM-Notification */
    sme_DeregisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)WNM_NOTIFICATION_FRAME,
                                  WNM_NOTIFICATION_FRAME_SIZE );
}

#ifdef FEATURE_WLAN_WAPI
void wlan_hdd_cfg80211_set_key_wapi(hdd_adapter_t* pAdapter, u8 key_index,
                                     const u8 *mac_addr, u8 *key , int key_Len)
{
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    tCsrRoamSetKey  setKey;
    v_BOOL_t isConnected = TRUE;
    int status = 0;
    v_U32_t roamId= 0xFF;
    tANI_U8 *pKeyPtr = NULL;
    int n = 0;

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s(%d)",
           __func__, hdd_device_modetoString(pAdapter->device_mode),
                                             pAdapter->device_mode);

    vos_mem_zero(&setKey, sizeof(tCsrRoamSetKey));
    setKey.keyId = key_index; // Store Key ID
    setKey.encType  = eCSR_ENCRYPT_TYPE_WPI; // SET WAPI Encryption
    setKey.keyDirection = eSIR_TX_RX;  // Key Directionn both TX and RX
    setKey.paeRole = 0 ; // the PAE role
    if (!mac_addr || is_broadcast_ether_addr(mac_addr))
    {
        vos_set_macaddr_broadcast( (v_MACADDR_t *)setKey.peerMac );
    }
    else
    {
        isConnected = hdd_connIsConnected(pHddStaCtx);
        vos_mem_copy(setKey.peerMac,&pHddStaCtx->conn_info.bssId,WNI_CFG_BSSID_LEN);
    }
    setKey.keyLength = key_Len;
    pKeyPtr = setKey.Key;
    memcpy( pKeyPtr, key, key_Len);

    hddLog(VOS_TRACE_LEVEL_INFO,"%s: WAPI KEY LENGTH:0x%04x",
                                            __func__, key_Len);
    for (n = 0 ; n < key_Len; n++)
        hddLog(VOS_TRACE_LEVEL_INFO, "%s WAPI KEY Data[%d]:%02x ",
                                           __func__,n,setKey.Key[n]);

    pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;
    if ( isConnected )
    {
        status= sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                             pAdapter->sessionId, &setKey, &roamId );
    }
    if ( status != 0 )
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "[%4d] sme_RoamSetKey returned ERROR status= %d",
                                                __LINE__, status );
        pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
    }
    /* Need to clear any trace of key value in the memory.
     * Thus zero out the memory even though it is local
     * variable.
     */
    vos_mem_zero(&setKey, sizeof(setKey));
}
#endif /* FEATURE_WLAN_WAPI*/

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
int wlan_hdd_cfg80211_alloc_new_beacon(hdd_adapter_t *pAdapter,
                                       beacon_data_t **ppBeacon,
                                       struct beacon_parameters *params)
#else
int wlan_hdd_cfg80211_alloc_new_beacon(hdd_adapter_t *pAdapter,
                                       beacon_data_t **ppBeacon,
                                       struct cfg80211_beacon_data *params,
                                       int dtim_period)
#endif
{
    int size;
    beacon_data_t *beacon = NULL;
    beacon_data_t *old = NULL;
    int head_len,tail_len;

    ENTER();
    if (params->head && !params->head_len)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("head_len is  NULL"));
        return -EINVAL;
    }

    old = pAdapter->sessionCtx.ap.beacon;

    if (!params->head && !old)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("session(%d) old and new heads points to NULL"),
                     pAdapter->sessionId);
        return -EINVAL;
    }

    if (params->tail && !params->tail_len)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            FL("tail_len is zero but tail is not NULL"));
        return -EINVAL;
    }

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,38))
    /* Kernel 3.0 is not updating dtim_period for set beacon */
    if (!params->dtim_period)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("dtim period is 0"));
        return -EINVAL;
    }
#endif

    if(params->head)
        head_len = params->head_len;
    else
        head_len = old->head_len;

    if(params->tail || !old)
        tail_len = params->tail_len;
    else
        tail_len = old->tail_len;

    size = sizeof(beacon_data_t) + head_len + tail_len;

    beacon = kzalloc(size, GFP_KERNEL);

    if( beacon == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("Mem allocation for beacon failed"));
        return -ENOMEM;
    }

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
    if(params->dtim_period || !old )
        beacon->dtim_period = params->dtim_period;
    else
        beacon->dtim_period = old->dtim_period;
#else
    if(dtim_period || !old )
        beacon->dtim_period = dtim_period;
    else
        beacon->dtim_period = old->dtim_period;
#endif

    beacon->head = ((u8 *) beacon) + sizeof(beacon_data_t);
    beacon->tail = beacon->head + head_len;
    beacon->head_len = head_len;
    beacon->tail_len = tail_len;

    if(params->head) {
        memcpy (beacon->head,params->head,beacon->head_len);
    }
    else {
        if(old)
            memcpy (beacon->head,old->head,beacon->head_len);
    }

    if(params->tail) {
        memcpy (beacon->tail,params->tail,beacon->tail_len);
    }
    else {
       if(old)
           memcpy (beacon->tail,old->tail,beacon->tail_len);
    }

    *ppBeacon = beacon;

    kfree(old);

    return 0;

}

v_U8_t* wlan_hdd_cfg80211_get_ie_ptr(v_U8_t *pIes, int length, v_U8_t eid)
{
    int left = length;
    v_U8_t *ptr = pIes;
    v_U8_t elem_id,elem_len;

    while(left >= 2)
    {
        elem_id  =  ptr[0];
        elem_len =  ptr[1];
        left -= 2;
        if(elem_len > left)
        {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                    FL("****Invalid IEs eid = %d elem_len=%d left=%d*****"),
                                                    eid,elem_len,left);
            return NULL;
        }
        if (elem_id == eid)
        {
            return ptr;
        }

        left -= elem_len;
        ptr += (elem_len + 2);
    }
    return NULL;
}

/* Check if rate is 11g rate or not */
static int wlan_hdd_rate_is_11g(u8 rate)
{
    static const u8 gRateArray[8] = {12, 18, 24, 36, 48, 72, 96, 108}; /* actual rate * 2 */
    u8 i;
    for (i = 0; i < 8; i++)
    {
        if(rate == gRateArray[i])
            return TRUE;
    }
    return FALSE;
}

/* Check for 11g rate and set proper 11g only mode */
static void wlan_hdd_check_11gmode(u8 *pIe, u8* require_ht,
                     u8* pCheckRatesfor11g, eSapPhyMode* pSapHw_mode)
{
    u8 i, num_rates = pIe[0];

    pIe += 1;
    for ( i = 0; i < num_rates; i++)
    {
        if( *pCheckRatesfor11g && (TRUE == wlan_hdd_rate_is_11g(pIe[i] & RATE_MASK)))
        {
            /* If rate set have 11g rate than change the mode to 11G */
            *pSapHw_mode = eSAP_DOT11_MODE_11g;
            if (pIe[i] & BASIC_RATE_MASK)
            {
                /* If we have 11g rate as  basic rate, it means mode
                   is 11g only mode.
                 */
               *pSapHw_mode = eSAP_DOT11_MODE_11g_ONLY;
               *pCheckRatesfor11g = FALSE;
            }
        }
        else if((BASIC_RATE_MASK | WLAN_BSS_MEMBERSHIP_SELECTOR_HT_PHY) == pIe[i])
        {
            *require_ht = TRUE;
        }
    }
    return;
}

static void wlan_hdd_set_sapHwmode(hdd_adapter_t *pHostapdAdapter)
{
    tsap_Config_t *pConfig = &pHostapdAdapter->sessionCtx.ap.sapConfig;
    beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;
    struct ieee80211_mgmt *pMgmt_frame = (struct ieee80211_mgmt*)pBeacon->head;
    u8 checkRatesfor11g = TRUE;
    u8 require_ht = FALSE;
    u8 *pIe=NULL;

    pConfig->SapHw_mode= eSAP_DOT11_MODE_11b;

    pIe = wlan_hdd_cfg80211_get_ie_ptr(&pMgmt_frame->u.beacon.variable[0],
                                       pBeacon->head_len, WLAN_EID_SUPP_RATES);
    if (pIe != NULL)
    {
        pIe += 1;
        wlan_hdd_check_11gmode(pIe, &require_ht, &checkRatesfor11g,
                               &pConfig->SapHw_mode);
    }

    pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
                                WLAN_EID_EXT_SUPP_RATES);
    if (pIe != NULL)
    {

        pIe += 1;
        wlan_hdd_check_11gmode(pIe, &require_ht, &checkRatesfor11g,
                               &pConfig->SapHw_mode);
    }

    if( pConfig->channel > 14 )
    {
        pConfig->SapHw_mode= eSAP_DOT11_MODE_11a;
    }

    pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
                                       WLAN_EID_HT_CAPABILITY);

    if(pIe)
    {
        pConfig->SapHw_mode= eSAP_DOT11_MODE_11n;
        if(require_ht)
            pConfig->SapHw_mode= eSAP_DOT11_MODE_11n_ONLY;
    }
}

static int wlan_hdd_add_ie(hdd_adapter_t* pHostapdAdapter, v_U8_t *genie,
                              v_U8_t *total_ielen, v_U8_t *oui, v_U8_t oui_size)
{
    v_U16_t ielen = 0;
    v_U8_t *pIe = NULL;
    beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;

    pIe = wlan_hdd_get_vendor_oui_ie_ptr(oui, oui_size,
                                          pBeacon->tail, pBeacon->tail_len);

    if (pIe)
    {
        ielen = pIe[1] + 2;
        if ((*total_ielen + ielen) <= MAX_GENIE_LEN)
        {
            vos_mem_copy(&genie[*total_ielen], pIe, ielen);
        }
        else
        {
            hddLog( VOS_TRACE_LEVEL_ERROR, "**Ie Length is too big***");
            return -EINVAL;
        }
        *total_ielen += ielen;
    }
    return 0;
}

static void wlan_hdd_add_hostapd_conf_vsie(hdd_adapter_t* pHostapdAdapter,
                                           v_U8_t *genie, v_U8_t *total_ielen)
{
    beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;
    int left = pBeacon->tail_len;
    v_U8_t *ptr = pBeacon->tail;
    v_U8_t elem_id, elem_len;
    v_U16_t ielen = 0;

    if ( NULL == ptr || 0 == left )
        return;

    while (left >= 2)
    {
        elem_id  = ptr[0];
        elem_len = ptr[1];
        left -= 2;
        if (elem_len > left)
        {
            hddLog( VOS_TRACE_LEVEL_ERROR,
                    "****Invalid IEs eid = %d elem_len=%d left=%d*****",
                    elem_id, elem_len, left);
            return;
        }
        if (IE_EID_VENDOR == elem_id)
        {
            /* skipping the VSIE's which we don't want to include or
             * it will be included by existing code
             */
            if ((memcmp( &ptr[2], WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE) != 0 ) &&
#ifdef WLAN_FEATURE_WFD
               (memcmp( &ptr[2], WFD_OUI_TYPE, WFD_OUI_TYPE_SIZE) != 0) &&
#endif
               (memcmp( &ptr[2], WHITELIST_OUI_TYPE, WPA_OUI_TYPE_SIZE) != 0) &&
               (memcmp( &ptr[2], BLACKLIST_OUI_TYPE, WPA_OUI_TYPE_SIZE) != 0) &&
               (memcmp( &ptr[2], "\x00\x50\xf2\x02", WPA_OUI_TYPE_SIZE) != 0) &&
               (memcmp( &ptr[2], WPA_OUI_TYPE, WPA_OUI_TYPE_SIZE) != 0) &&
               (memcmp( &ptr[2], P2P_OUI_TYPE, P2P_OUI_TYPE_SIZE) != 0))
            {
               ielen = ptr[1] + 2;
               if ((*total_ielen + ielen) <= MAX_GENIE_LEN)
               {
                   vos_mem_copy(&genie[*total_ielen], ptr, ielen);
                   *total_ielen += ielen;
               }
               else
               {
                   hddLog( VOS_TRACE_LEVEL_ERROR,
                           "IE Length is too big "
                           "IEs eid=%d elem_len=%d total_ie_lent=%d",
                           elem_id, elem_len, *total_ielen);
               }
            }
        }

        left -= elem_len;
        ptr += (elem_len + 2);
    }
    return;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static int wlan_hdd_cfg80211_update_apies(hdd_adapter_t* pHostapdAdapter,
                            struct beacon_parameters *params)
#else
static int wlan_hdd_cfg80211_update_apies(hdd_adapter_t* pHostapdAdapter,
                                     struct cfg80211_beacon_data *params)
#endif
{
    v_U8_t *genie;
    v_U8_t total_ielen = 0;
    v_U8_t addIE[1] = {0};
    int ret = 0;

    genie = vos_mem_malloc(MAX_GENIE_LEN);

    if(genie == NULL) {

        return -ENOMEM;
    }

    if (0 != wlan_hdd_add_ie(pHostapdAdapter, genie,
                              &total_ielen, WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE))
    {
         hddLog(LOGE,
               FL("Adding WPS IE failed"));
         ret = -EINVAL;
         goto done;
    }

#ifdef WLAN_FEATURE_WFD
    if (0 != wlan_hdd_add_ie(pHostapdAdapter, genie,
                              &total_ielen, WFD_OUI_TYPE, WFD_OUI_TYPE_SIZE))
    {
         hddLog(LOGE,
               FL("Adding WFD IE failed"));
         ret = -EINVAL;
         goto done;
    }
#endif

    if (0 != wlan_hdd_add_ie(pHostapdAdapter, genie,
                              &total_ielen, P2P_OUI_TYPE, P2P_OUI_TYPE_SIZE))
    {
         hddLog(LOGE,
               FL("Adding P2P IE failed"));
         ret = -EINVAL;
         goto done;
    }

    if (WLAN_HDD_SOFTAP == pHostapdAdapter->device_mode)
    {
        wlan_hdd_add_hostapd_conf_vsie(pHostapdAdapter, genie, &total_ielen);
    }

    if (ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
       WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA, genie, total_ielen, NULL,
               eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
        hddLog(LOGE,
               "Could not pass on WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA to CCM");
        ret = -EINVAL;
        goto done;
    }

    if (ccmCfgSetInt((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
          WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG, 1,NULL,
          test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags) ?
                   eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE)
          ==eHAL_STATUS_FAILURE)
    {
        hddLog(LOGE,
            "Could not pass on WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG to CCM");
        ret = -EINVAL;
        goto done;
    }

    // Added for ProResp IE
    if ( (params->proberesp_ies != NULL) && (params->proberesp_ies_len != 0) )
    {
        u16 rem_probe_resp_ie_len = params->proberesp_ies_len;
        u8 probe_rsp_ie_len[3] = {0};
        u8 counter = 0;
        /* Check Probe Resp Length if it is greater then 255 then Store
           Probe Resp IEs into WNI_CFG_PROBE_RSP_ADDNIE_DATA1 &
           WNI_CFG_PROBE_RSP_ADDNIE_DATA2 CFG Variable As We are not able
           Store More then 255 bytes into One Variable.
        */
        while ((rem_probe_resp_ie_len > 0) && (counter < 3))
        {
            if (rem_probe_resp_ie_len > MAX_CFG_STRING_LEN)
            {
                probe_rsp_ie_len[counter++] = MAX_CFG_STRING_LEN;
                rem_probe_resp_ie_len -= MAX_CFG_STRING_LEN;
            }
            else
            {
                probe_rsp_ie_len[counter++] = rem_probe_resp_ie_len;
                rem_probe_resp_ie_len = 0;
            }
        }

        rem_probe_resp_ie_len = 0;

        if (probe_rsp_ie_len[0] > 0)
        {
            if (ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA1,
                            (tANI_U8*)&params->proberesp_ies[rem_probe_resp_ie_len],
                            probe_rsp_ie_len[0], NULL,
                            eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
            {
                 hddLog(LOGE,
                       "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA1 to CCM");
                 ret = -EINVAL;
                 goto done;
            }
            rem_probe_resp_ie_len += probe_rsp_ie_len[0];
        }

        if (probe_rsp_ie_len[1] > 0)
        {
            if (ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA2,
                            (tANI_U8*)&params->proberesp_ies[rem_probe_resp_ie_len],
                            probe_rsp_ie_len[1], NULL,
                            eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
            {
                 hddLog(LOGE,
                       "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA2 to CCM");
                 ret = -EINVAL;
                 goto done;
            }
            rem_probe_resp_ie_len += probe_rsp_ie_len[1];
        }

        if (probe_rsp_ie_len[2] > 0)
        {
            if (ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA3,
                            (tANI_U8*)&params->proberesp_ies[rem_probe_resp_ie_len],
                            probe_rsp_ie_len[2], NULL,
                            eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
            {
                 hddLog(LOGE,
                       "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA3 to CCM");
                 ret = -EINVAL;
                 goto done;
            }
            rem_probe_resp_ie_len += probe_rsp_ie_len[2];
        }

        if (probe_rsp_ie_len[1] == 0 )
        {
            if ( eHAL_STATUS_FAILURE == ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA2, (tANI_U8*)addIE, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
            {
                hddLog(LOGE,
                   "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA2 to CCM");
            }
        }

        if (probe_rsp_ie_len[2] == 0 )
        {
            if ( eHAL_STATUS_FAILURE == ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA3, (tANI_U8*)addIE, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
            {
                hddLog(LOGE,
                   "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA3 to CCM");
            }
        }

        if (ccmCfgSetInt((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
             WNI_CFG_PROBE_RSP_ADDNIE_FLAG, 1,NULL,
             test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags) ?
                      eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE)
             == eHAL_STATUS_FAILURE)
        {
           hddLog(LOGE,
             "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_FLAG to CCM");
           ret = -EINVAL;
           goto done;
        }
    }
    else
    {
        // Reset WNI_CFG_PROBE_RSP Flags
        wlan_hdd_reset_prob_rspies(pHostapdAdapter);

        hddLog(VOS_TRACE_LEVEL_INFO,
               "%s: No Probe Response IE received in set beacon",
               __func__);
    }

    // Added for AssocResp IE
    if ( (params->assocresp_ies != NULL) && (params->assocresp_ies_len != 0) )
    {
       if (ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
               WNI_CFG_ASSOC_RSP_ADDNIE_DATA, (tANI_U8*)params->assocresp_ies,
               params->assocresp_ies_len, NULL,
               eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
       {
            hddLog(LOGE,
                  "Could not pass on WNI_CFG_ASSOC_RSP_ADDNIE_DATA to CCM");
            ret = -EINVAL;
            goto done;
       }

       if (ccmCfgSetInt((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
          WNI_CFG_ASSOC_RSP_ADDNIE_FLAG, 1, NULL,
          test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags) ?
                   eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE)
          == eHAL_STATUS_FAILURE)
       {
          hddLog(LOGE,
            "Could not pass on WNI_CFG_ASSOC_RSP_ADDNIE_FLAG to CCM");
          ret = -EINVAL;
          goto done;
       }
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_INFO,
               "%s: No Assoc Response IE received in set beacon",
               __func__);

        if ( eHAL_STATUS_FAILURE == ccmCfgSetInt((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_ASSOC_RSP_ADDNIE_FLAG, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
        {
            hddLog(LOGE,
               "Could not pass on WNI_CFG_ASSOC_RSP_ADDNIE_FLAG to CCM");
        }
    }

done:
    vos_mem_free(genie);
    return ret;
}

/*
 * FUNCTION: wlan_hdd_validate_operation_channel
 * called by wlan_hdd_cfg80211_start_bss() and
 * wlan_hdd_cfg80211_set_channel()
 * This function validates whether given channel is part of valid
 * channel list.
 */
VOS_STATUS wlan_hdd_validate_operation_channel(hdd_adapter_t *pAdapter,int channel)
{

    v_U32_t num_ch = 0;
    u8 valid_ch[WNI_CFG_VALID_CHANNEL_LIST_LEN];
    u32 indx = 0;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    v_U8_t fValidChannel = FALSE, count = 0;
    hdd_config_t *hdd_pConfig_ini= (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;

    num_ch = WNI_CFG_VALID_CHANNEL_LIST_LEN;

    if ( hdd_pConfig_ini->sapAllowAllChannel)
    {
         /* Validate the channel */
        for (count = RF_CHAN_1 ; count <= RF_CHAN_165 ; count++)
        {
            if ( channel == rfChannels[count].channelNum )
            {
                fValidChannel = TRUE;
                break;
            }
        }
        if (fValidChannel != TRUE)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Invalid Channel [%d]", __func__, channel);
            return VOS_STATUS_E_FAILURE;
        }
    }
    else
    {
        if (0 != ccmCfgGetStr(hHal, WNI_CFG_VALID_CHANNEL_LIST,
            valid_ch, &num_ch))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: failed to get valid channel list", __func__);
                return VOS_STATUS_E_FAILURE;
            }
        for (indx = 0; indx < num_ch; indx++)
        {
            if (channel == valid_ch[indx])
            {
                break;
            }
         }

         if (indx >= num_ch)
         {
             if (WLAN_HDD_P2P_GO == pAdapter->device_mode)
             {
                 eCsrBand band;
                 unsigned int freq;

                 sme_GetFreqBand(hHal, &band);

                 if (eCSR_BAND_5G == band)
                 {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,38))
                     if (channel <= ARRAY_SIZE(hdd_channels_2_4_GHZ))
                     {
                         freq = ieee80211_channel_to_frequency(channel,
                                                          IEEE80211_BAND_2GHZ);
                     }
                     else
                     {
                         freq = ieee80211_channel_to_frequency(channel,
                                                          IEEE80211_BAND_5GHZ);
                     }
#else
                     freq = ieee80211_channel_to_frequency(channel);
#endif
                     if(WLAN_HDD_IS_SOCIAL_CHANNEL(freq))
                         return VOS_STATUS_SUCCESS;
                 }
             }

             hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Invalid Channel [%d]", __func__, channel);
             return VOS_STATUS_E_FAILURE;
         }
    }

    return VOS_STATUS_SUCCESS;

}

/**
 * FUNCTION: wlan_hdd_cfg80211_set_channel
 * This function is used to set the channel number
 */
static int wlan_hdd_cfg80211_set_channel( struct wiphy *wiphy, struct net_device *dev,
                                   struct ieee80211_channel *chan,
                                   enum nl80211_channel_type channel_type
                                 )
{
    hdd_adapter_t *pAdapter = NULL;
    v_U32_t num_ch = 0;
    int channel = 0;
    int freq = chan->center_freq; /* freq is in MHZ */
    hdd_context_t *pHddCtx;
    int status;

    ENTER();


    if( NULL == dev )
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Called with dev = NULL.", __func__);
        return -ENODEV;
    }
    pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_CHANNEL, pAdapter->sessionId,
                     channel_type ));
    hddLog(VOS_TRACE_LEVEL_INFO,
                "%s: device_mode = %s (%d)  freq = %d", __func__,
                hdd_device_modetoString(pAdapter->device_mode),
                pAdapter->device_mode, chan->center_freq);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    /*
     * Do freq to chan conversion
     * TODO: for 11a
     */

    channel = ieee80211_frequency_to_channel(freq);

    /* Check freq range */
    if ((WNI_CFG_CURRENT_CHANNEL_STAMIN > channel) ||
            (WNI_CFG_CURRENT_CHANNEL_STAMAX < channel))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Channel [%d] is outside valid range from %d to %d",
                __func__, channel, WNI_CFG_CURRENT_CHANNEL_STAMIN,
                WNI_CFG_CURRENT_CHANNEL_STAMAX);
        return -EINVAL;
    }

    num_ch = WNI_CFG_VALID_CHANNEL_LIST_LEN;

    if ((WLAN_HDD_SOFTAP != pAdapter->device_mode) &&
       (WLAN_HDD_P2P_GO != pAdapter->device_mode))
    {
        if(VOS_STATUS_SUCCESS != wlan_hdd_validate_operation_channel(pAdapter,channel))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Invalid Channel [%d]", __func__, channel);
            return -EINVAL;
        }
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: set channel to [%d] for device mode =%d",
                          __func__, channel,pAdapter->device_mode);
    }
    if( (pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
     || (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)
      )
    {
        hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
        tCsrRoamProfile * pRoamProfile = &pWextState->roamProfile;
        hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

        if (eConnectionState_IbssConnected == pHddStaCtx->conn_info.connState)
        {
           /* Link is up then return cant set channel*/
            hddLog( VOS_TRACE_LEVEL_ERROR,
                   "%s: IBSS Associated, can't set the channel", __func__);
            return -EINVAL;
        }

        num_ch = pRoamProfile->ChannelInfo.numOfChannels = 1;
        pHddStaCtx->conn_info.operationChannel = channel;
        pRoamProfile->ChannelInfo.ChannelList =
            &pHddStaCtx->conn_info.operationChannel;
    }
    else if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
        ||   (pAdapter->device_mode == WLAN_HDD_P2P_GO)
            )
    {
        if (WLAN_HDD_P2P_GO == pAdapter->device_mode)
        {
            if(VOS_STATUS_SUCCESS !=
                       wlan_hdd_validate_operation_channel(pAdapter,channel))
            {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid Channel [%d]", __func__, channel);
               return -EINVAL;
            }
            (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->sapConfig.channel = channel;
        }
        else if ( WLAN_HDD_SOFTAP == pAdapter->device_mode )
        {
            hdd_config_t *cfg_param = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;

            /* If auto channel selection is configured as enable/ 1 then ignore
            channel set by supplicant
            */
            if ( cfg_param->apAutoChannelSelection )
            {
                (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->sapConfig.channel =
                                                          AUTO_CHANNEL_SELECT;
                hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                     "%s: set channel to auto channel (0) for device mode =%s (%d)",
                     __func__, hdd_device_modetoString(pAdapter->device_mode),
                                                     pAdapter->device_mode);
            }
            else
            {
                if(VOS_STATUS_SUCCESS !=
                         wlan_hdd_validate_operation_channel(pAdapter,channel))
                {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                          "%s: Invalid Channel [%d]", __func__, channel);
                   return -EINVAL;
                }
                (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->sapConfig.channel = channel;
            }
        }
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
               "%s: Invalid device mode failed to set valid channel", __func__);
        return -EINVAL;
    }
    EXIT();
    return status;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static int wlan_hdd_cfg80211_start_bss(hdd_adapter_t *pHostapdAdapter,
                            struct beacon_parameters *params)
#else
static int wlan_hdd_cfg80211_start_bss(hdd_adapter_t *pHostapdAdapter,
                                       struct cfg80211_beacon_data *params,
                                       const u8 *ssid, size_t ssid_len,
                                       enum nl80211_hidden_ssid hidden_ssid)
#endif
{
    tsap_Config_t *pConfig;
    beacon_data_t *pBeacon = NULL;
    struct ieee80211_mgmt *pMgmt_frame;
    v_U8_t *pIe=NULL;
    v_U16_t capab_info;
    eCsrAuthType RSNAuthType;
    eCsrEncryptionType RSNEncryptType;
    eCsrEncryptionType mcRSNEncryptType;
    int status = VOS_STATUS_SUCCESS;
    tpWLAN_SAPEventCB pSapEventCallback;
    hdd_hostapd_state_t *pHostapdState;
    v_U8_t wpaRsnIEdata[(SIR_MAC_MAX_IE_LENGTH * 2)+4];  //Max ie length 255 * 2(WPA+RSN) + 2 bytes (vendor specific ID) * 2
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    struct qc_mac_acl_entry *acl_entry = NULL;
    hdd_config_t *iniConfig;
    v_SINT_t i;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    tSmeConfigParams *psmeConfig;
    v_BOOL_t MFPCapable = VOS_FALSE;
    v_BOOL_t MFPRequired = VOS_FALSE;
    eHddDot11Mode sapDot11Mode =
            (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->sapDot11Mode;

    ENTER();

    iniConfig = pHddCtx->cfg_ini;

    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);

    pConfig = &pHostapdAdapter->sessionCtx.ap.sapConfig;

    pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;

    pMgmt_frame = (struct ieee80211_mgmt*)pBeacon->head;

    pConfig->beacon_int =  pMgmt_frame->u.beacon.beacon_int;

    //channel is already set in the set_channel Call back
    //pConfig->channel = pCommitConfig->channel;

    /*Protection parameter to enable or disable*/
    pConfig->protEnabled =
           (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apProtEnabled;

    pConfig->dtim_period = pBeacon->dtim_period;

    hddLog(VOS_TRACE_LEVEL_INFO_HIGH,"****pConfig->dtim_period=%d***",
                                      pConfig->dtim_period);

    if (pHostapdAdapter->device_mode == WLAN_HDD_SOFTAP)
    {
        pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
                                       WLAN_EID_COUNTRY);
        if(memcmp(pHddCtx->cfg_ini->apCntryCode, CFG_AP_COUNTRY_CODE_DEFAULT, 3) != 0)
        {
           tANI_BOOLEAN restartNeeded;
           pConfig->ieee80211d = 1;
           vos_mem_copy(pConfig->countryCode, pHddCtx->cfg_ini->apCntryCode, 3);
           sme_setRegInfo(hHal, pConfig->countryCode);
           sme_ResetCountryCodeInformation(hHal, &restartNeeded);
        }
        else if(pIe)
        {
            tANI_BOOLEAN restartNeeded;
            pConfig->ieee80211d = 1;
            vos_mem_copy(pConfig->countryCode, &pIe[2], 3);
            sme_setRegInfo(hHal, pConfig->countryCode);
            sme_ResetCountryCodeInformation(hHal, &restartNeeded);
        }
        else
        {
            pConfig->ieee80211d = 0;
        }
        /*
        * If auto channel is configured i.e. channel is 0,
        * so skip channel validation.
        */
        if( AUTO_CHANNEL_SELECT != pConfig->channel )
        {
            if(VOS_STATUS_SUCCESS != wlan_hdd_validate_operation_channel(pHostapdAdapter,pConfig->channel))
            {
                 hddLog(VOS_TRACE_LEVEL_ERROR,
                         "%s: Invalid Channel [%d]", __func__, pConfig->channel);
                 return -EINVAL;
            }
        }
        else
        {
             if(1 != pHddCtx->is_dynamic_channel_range_set)
             {
                  hdd_config_t *hdd_pConfig= (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini;
                  WLANSAP_SetChannelRange(hHal, hdd_pConfig->apStartChannelNum,
                       hdd_pConfig->apEndChannelNum,hdd_pConfig->apOperatingBand);
             }
                   pHddCtx->is_dynamic_channel_range_set = 0;
        }
    }
    else
    {
        pConfig->ieee80211d = 0;
    }
    pConfig->authType = eSAP_AUTO_SWITCH;

    capab_info = pMgmt_frame->u.beacon.capab_info;

    pConfig->privacy = (pMgmt_frame->u.beacon.capab_info &
                        WLAN_CAPABILITY_PRIVACY) ? VOS_TRUE : VOS_FALSE;

    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy = pConfig->privacy;

    /*Set wps station to configured*/
    pIe = wlan_hdd_get_wps_ie_ptr(pBeacon->tail, pBeacon->tail_len);

    if(pIe)
    {
        if(pIe[1] < (2 + WPS_OUI_TYPE_SIZE))
        {
            hddLog( VOS_TRACE_LEVEL_ERROR, "**Wps Ie Length is too small***");
            return -EINVAL;
        }
        else if(memcmp(&pIe[2], WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE) == 0)
        {
             hddLog( VOS_TRACE_LEVEL_INFO, "** WPS IE(len %d) ***", (pIe[1]+2));
             /* Check 15 bit of WPS IE as it contain information for wps state
              * WPS state
              */
              if(SAP_WPS_ENABLED_UNCONFIGURED == pIe[15])
              {
                  pConfig->wps_state = SAP_WPS_ENABLED_UNCONFIGURED;
              } else if(SAP_WPS_ENABLED_CONFIGURED == pIe[15])
              {
                  pConfig->wps_state = SAP_WPS_ENABLED_CONFIGURED;
              }
        }
    }
    else
    {
        pConfig->wps_state = SAP_WPS_DISABLED;
    }
    pConfig->fwdWPSPBCProbeReq  = 1; // Forward WPS PBC probe request frame up

    pConfig->RSNEncryptType = eCSR_ENCRYPT_TYPE_NONE;
    pConfig->mcRSNEncryptType = eCSR_ENCRYPT_TYPE_NONE;
    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->ucEncryptType =
        eCSR_ENCRYPT_TYPE_NONE;

    pConfig->RSNWPAReqIELength = 0;
    pConfig->pRSNWPAReqIE = NULL;
    pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
                                       WLAN_EID_RSN);
    if(pIe && pIe[1])
    {
        pConfig->RSNWPAReqIELength = pIe[1] + 2;
        memcpy(&wpaRsnIEdata[0], pIe, pConfig->RSNWPAReqIELength);
        pConfig->pRSNWPAReqIE = &wpaRsnIEdata[0];
        /* The actual processing may eventually be more extensive than
         * this. Right now, just consume any PMKIDs that are  sent in
         * by the app.
         * */
        status = hdd_softap_unpackIE(
                        vos_get_context( VOS_MODULE_ID_SME, pVosContext),
                        &RSNEncryptType,
                        &mcRSNEncryptType,
                        &RSNAuthType,
                        &MFPCapable,
                        &MFPRequired,
                        pConfig->pRSNWPAReqIE[1]+2,
                        pConfig->pRSNWPAReqIE );

        if( VOS_STATUS_SUCCESS == status )
        {
            /* Now copy over all the security attributes you have
             * parsed out
             * */
            pConfig->RSNEncryptType = RSNEncryptType; // Use the cipher type in the RSN IE
            pConfig->mcRSNEncryptType = mcRSNEncryptType;
            (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->ucEncryptType
                                                              = RSNEncryptType;
            hddLog( LOG1, FL("CSR AuthType = %d, "
                        "EncryptionType = %d mcEncryptionType = %d"),
                        RSNAuthType, RSNEncryptType, mcRSNEncryptType);
        }
    }

    pIe = wlan_hdd_get_vendor_oui_ie_ptr(WPA_OUI_TYPE, WPA_OUI_TYPE_SIZE,
                                         pBeacon->tail, pBeacon->tail_len);

    if(pIe && pIe[1] && (pIe[0] == DOT11F_EID_WPA))
    {
        if (pConfig->pRSNWPAReqIE)
        {
            /*Mixed mode WPA/WPA2*/
            memcpy((&wpaRsnIEdata[0] + pConfig->RSNWPAReqIELength), pIe, pIe[1] + 2);
            pConfig->RSNWPAReqIELength += pIe[1] + 2;
        }
        else
        {
            pConfig->RSNWPAReqIELength = pIe[1] + 2;
            memcpy(&wpaRsnIEdata[0], pIe, pConfig->RSNWPAReqIELength);
            pConfig->pRSNWPAReqIE = &wpaRsnIEdata[0];
            status = hdd_softap_unpackIE(
                          vos_get_context( VOS_MODULE_ID_SME, pVosContext),
                          &RSNEncryptType,
                          &mcRSNEncryptType,
                          &RSNAuthType,
                          &MFPCapable,
                          &MFPRequired,
                          pConfig->pRSNWPAReqIE[1]+2,
                          pConfig->pRSNWPAReqIE );

            if( VOS_STATUS_SUCCESS == status )
            {
                /* Now copy over all the security attributes you have
                 * parsed out
                 * */
                pConfig->RSNEncryptType = RSNEncryptType; // Use the cipher type in the RSN IE
                pConfig->mcRSNEncryptType = mcRSNEncryptType;
                (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->ucEncryptType
                                                              = RSNEncryptType;
                hddLog( LOG1, FL("CSR AuthType = %d, "
                                "EncryptionType = %d mcEncryptionType = %d"),
                                RSNAuthType, RSNEncryptType, mcRSNEncryptType);
            }
        }
    }

    if (pConfig->RSNWPAReqIELength > sizeof wpaRsnIEdata) {
        hddLog( VOS_TRACE_LEVEL_ERROR, "**RSNWPAReqIELength is too large***");
        return -EINVAL;
    }

    pConfig->SSIDinfo.ssidHidden = VOS_FALSE;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
    if (params->ssid != NULL)
    {
        memcpy(pConfig->SSIDinfo.ssid.ssId, params->ssid, params->ssid_len);
        pConfig->SSIDinfo.ssid.length = params->ssid_len;
        if (params->hidden_ssid != NL80211_HIDDEN_SSID_NOT_IN_USE)
            pConfig->SSIDinfo.ssidHidden = VOS_TRUE;
    }
#else
    if (ssid != NULL)
    {
        memcpy(pConfig->SSIDinfo.ssid.ssId, ssid, ssid_len);
        pConfig->SSIDinfo.ssid.length = ssid_len;
        if (hidden_ssid != NL80211_HIDDEN_SSID_NOT_IN_USE)
            pConfig->SSIDinfo.ssidHidden = VOS_TRUE;
    }
#endif

    vos_mem_copy(pConfig->self_macaddr.bytes,
               pHostapdAdapter->macAddressCurrent.bytes, sizeof(v_MACADDR_t));

    /* default value */
    pConfig->SapMacaddr_acl = eSAP_ACCEPT_UNLESS_DENIED;
    pConfig->num_accept_mac = 0;
    pConfig->num_deny_mac = 0;

    pIe = wlan_hdd_get_vendor_oui_ie_ptr(BLACKLIST_OUI_TYPE, WPA_OUI_TYPE_SIZE,
                                         pBeacon->tail, pBeacon->tail_len);

    /* pIe for black list is following form:
            type    : 1 byte
            length  : 1 byte
            OUI     : 4 bytes
            acl type : 1 byte
            no of mac addr in black list: 1 byte
            list of mac_acl_entries: variable, 6 bytes per mac address + sizeof(int) for vlan id
    */
    if ((pIe != NULL) && (pIe[1] != 0))
    {
        pConfig->SapMacaddr_acl = pIe[6];
        pConfig->num_deny_mac   = pIe[7];
        hddLog(VOS_TRACE_LEVEL_INFO,"acl type = %d no deny mac = %d",
                                     pIe[6], pIe[7]);
        if (pConfig->num_deny_mac > MAX_ACL_MAC_ADDRESS)
            pConfig->num_deny_mac = MAX_ACL_MAC_ADDRESS;
        acl_entry = (struct qc_mac_acl_entry *)(pIe + 8);
        for (i = 0; i < pConfig->num_deny_mac; i++)
        {
            vos_mem_copy(&pConfig->deny_mac[i], acl_entry->addr, sizeof(qcmacaddr));
            acl_entry++;
        }
    }
    pIe = wlan_hdd_get_vendor_oui_ie_ptr(WHITELIST_OUI_TYPE, WPA_OUI_TYPE_SIZE,
                                         pBeacon->tail, pBeacon->tail_len);

    /* pIe for white list is following form:
            type    : 1 byte
            length  : 1 byte
            OUI     : 4 bytes
            acl type : 1 byte
            no of mac addr in white list: 1 byte
            list of mac_acl_entries: variable, 6 bytes per mac address + sizeof(int) for vlan id
    */
    if ((pIe != NULL) && (pIe[1] != 0))
    {
        pConfig->SapMacaddr_acl = pIe[6];
        pConfig->num_accept_mac   = pIe[7];
        hddLog(VOS_TRACE_LEVEL_INFO,"acl type = %d no accept mac = %d",
                                      pIe[6], pIe[7]);
        if (pConfig->num_accept_mac > MAX_ACL_MAC_ADDRESS)
            pConfig->num_accept_mac = MAX_ACL_MAC_ADDRESS;
        acl_entry = (struct qc_mac_acl_entry *)(pIe + 8);
        for (i = 0; i < pConfig->num_accept_mac; i++)
        {
            vos_mem_copy(&pConfig->accept_mac[i], acl_entry->addr, sizeof(qcmacaddr));
            acl_entry++;
        }
    }

    wlan_hdd_set_sapHwmode(pHostapdAdapter);

#ifdef WLAN_FEATURE_11AC
    /* Overwrite the hostapd setting for HW mode only for 11ac.
     * This is valid only if mode is set to 11n in hostapd, either AUTO or
     * 11ac in .ini and 11ac is supported by both host and firmware.
     * Otherwise, leave whatever is set in hostapd (a OR b OR g OR n mode)
     */
    if( ((pConfig->SapHw_mode == eSAP_DOT11_MODE_11n) ||
         (pConfig->SapHw_mode == eSAP_DOT11_MODE_11n_ONLY)) &&
        (( sapDot11Mode == eHDD_DOT11_MODE_AUTO ) ||
         ( sapDot11Mode == eHDD_DOT11_MODE_11ac ) ||
         ( sapDot11Mode == eHDD_DOT11_MODE_11ac_ONLY ) ) &&
         (sme_IsFeatureSupportedByDriver(DOT11AC)) &&
          (sme_IsFeatureSupportedByFW(DOT11AC)) )
    {
        v_U32_t operatingBand = 0;
        pConfig->SapHw_mode = eSAP_DOT11_MODE_11ac;
        ccmCfgGetInt(hHal, WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND, &operatingBand);

        /* If ACS disable and selected channel <= 14
         *  OR
         *  ACS enabled and ACS operating band is choosen as 2.4
         *  AND
         *  VHT in 2.4G Disabled
         *  THEN
         *  Fallback to 11N mode
         */
        if (((AUTO_CHANNEL_SELECT != pConfig->channel && pConfig->channel <= SIR_11B_CHANNEL_END)
                    || (AUTO_CHANNEL_SELECT == pConfig->channel &&
            operatingBand == eSAP_RF_SUBBAND_2_4_GHZ)) &&
                iniConfig->enableVhtFor24GHzBand == FALSE)
        {
            hddLog(LOGW, FL("Setting hwmode to 11n, operatingBand = %d, Channel = %d"),
                    operatingBand, pConfig->channel);
            pConfig->SapHw_mode = eSAP_DOT11_MODE_11n;
        }
    }
#endif

    if ( AUTO_CHANNEL_SELECT != pConfig->channel )
    {
        sme_SelectCBMode(hHal,
            sapConvertSapPhyModeToCsrPhyMode(pConfig->SapHw_mode),
            pConfig->channel);
    }
    // ht_capab is not what the name conveys,this is used for protection bitmap
    pConfig->ht_capab =
                 (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apProtection;

    if ( 0 != wlan_hdd_cfg80211_update_apies(pHostapdAdapter, params) )
    {
        hddLog(LOGE, FL("SAP Not able to set AP IEs"));
        return -EINVAL;
    }

    //Uapsd Enabled Bit
    pConfig->UapsdEnable =
          (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apUapsdEnabled;
    //Enable OBSS protection
    pConfig->obssProtEnabled =
           (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apOBSSProtEnabled;

#ifdef WLAN_FEATURE_11W
    pConfig->mfpCapable = MFPCapable;
    pConfig->mfpRequired = MFPRequired;
    hddLog(LOGW, FL("Soft AP MFP capable %d, MFP required %d\n"),
           pConfig->mfpCapable, pConfig->mfpRequired);
#endif

    hddLog(LOGW, FL("SOftAP macaddress : "MAC_ADDRESS_STR),
                 MAC_ADDR_ARRAY(pHostapdAdapter->macAddressCurrent.bytes));
    hddLog(LOGW,FL("ssid =%s, beaconint=%d, channel=%d"),
                   pConfig->SSIDinfo.ssid.ssId, (int)pConfig->beacon_int,
                    (int)pConfig->channel);
    hddLog(LOGW,FL("hw_mode=%x, privacy=%d, authType=%d"),
                   pConfig->SapHw_mode, pConfig->privacy,
                   pConfig->authType);
    hddLog(LOGW,FL("RSN/WPALen=%d, Uapsd = %d"),
                   (int)pConfig->RSNWPAReqIELength, pConfig->UapsdEnable);
    hddLog(LOGW,FL("ProtEnabled = %d, OBSSProtEnabled = %d"),
                    pConfig->protEnabled, pConfig->obssProtEnabled);

    if(test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags))
    {
        //Bss already started. just return.
        //TODO Probably it should update some beacon params.
        hddLog( LOGE, "Bss Already started...Ignore the request");
        EXIT();
        return 0;
    }

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Reached max concurrent connections"));
        return -EINVAL;
    }

    pConfig->persona = pHostapdAdapter->device_mode;

    psmeConfig = (tSmeConfigParams*) vos_mem_malloc(sizeof(tSmeConfigParams));
    if ( NULL != psmeConfig)
    {
        sme_GetConfigParam(hHal, psmeConfig);
        pConfig->scanBandPreference = psmeConfig->csrConfig.scanBandPreference;
        vos_mem_free(psmeConfig);
    }
    pConfig->acsBandSwitchThreshold = iniConfig->acsBandSwitchThreshold;

    pSapEventCallback = hdd_hostapd_SAPEventCB;
    if(WLANSAP_StartBss(pVosContext, pSapEventCallback, pConfig,
                 (v_PVOID_t)pHostapdAdapter->dev) != VOS_STATUS_SUCCESS)
    {
        hddLog(LOGE,FL("SAP Start Bss fail"));
        return -EINVAL;
    }

    hddLog(LOG1,
           FL("Waiting for Scan to complete(auto mode) and BSS to start"));

    status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);

    if (!VOS_IS_STATUS_SUCCESS(status))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 ("ERROR: HDD vos wait for single_event failed!!"));
        smeGetCommandQStatus(hHal);
        VOS_ASSERT(0);
    }

    set_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
    /* Initialize WMM configuation */
    hdd_wmm_init(pHostapdAdapter);
    wlan_hdd_incr_active_session(pHddCtx, pHostapdAdapter->device_mode);

#ifdef WLAN_FEATURE_P2P_DEBUG
    if (pHostapdAdapter->device_mode == WLAN_HDD_P2P_GO)
    {
         if(globalP2PConnectionStatus == P2P_GO_NEG_COMPLETED)
         {
             globalP2PConnectionStatus = P2P_GO_COMPLETED_STATE;
             hddLog(LOGE,"[P2P State] From Go nego completed to "
                         "Non-autonomous Group started");
         }
         else if(globalP2PConnectionStatus == P2P_NOT_ACTIVE)
         {
             globalP2PConnectionStatus = P2P_GO_COMPLETED_STATE;
             hddLog(LOGE,"[P2P State] From Inactive to "
                         "Autonomous Group started");
         }
    }
#endif

    pHostapdState->bCommit = TRUE;
    EXIT();

   return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static int wlan_hdd_cfg80211_add_beacon(struct wiphy *wiphy,
                                        struct net_device *dev,
                                        struct beacon_parameters *params)
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t  *pHddCtx;
    int status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_ADD_BEACON,
                     pAdapter->sessionId, params->interval));
    hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "device mode=%s (%d)",
           hdd_device_modetoString(pAdapter->device_mode),
           pAdapter->device_mode);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Reached max concurrent connections"));
        return -EINVAL;
    }

    if ( (pAdapter->device_mode == WLAN_HDD_SOFTAP)
      || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {
        beacon_data_t *old,*new;

        old = pAdapter->sessionCtx.ap.beacon;

        if (old)
        {
            hddLog(VOS_TRACE_LEVEL_WARN,
                   FL("already beacon info added to session(%d)"),
                       pAdapter->sessionId);
            return -EALREADY;
        }

        status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter,&new,params);

        if(status != VOS_STATUS_SUCCESS)
        {
             hddLog(VOS_TRACE_LEVEL_FATAL,
                   "%s:Error!!! Allocating the new beacon",__func__);
             return -EINVAL;
        }

        pAdapter->sessionCtx.ap.beacon = new;

        status = wlan_hdd_cfg80211_start_bss(pAdapter, params);
    }

    EXIT();
    return status;
}

static int wlan_hdd_cfg80211_set_beacon(struct wiphy *wiphy,
                                        struct net_device *dev,
                                        struct beacon_parameters *params)
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t  *pHddCtx;
    int status;

    ENTER();
    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_BEACON,
                     pAdapter->sessionId, pHddStaCtx->conn_info.authType));
    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s (%d)",
           __func__, hdd_device_modetoString(pAdapter->device_mode),
                                             pAdapter->device_mode);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
     || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {
        beacon_data_t *old,*new;

        old = pAdapter->sessionCtx.ap.beacon;

        if (!old)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("session(%d) old and new heads points to NULL"),
                       pAdapter->sessionId);
            return -ENOENT;
        }

        status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter,&new,params);

        if(status != VOS_STATUS_SUCCESS) {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   "%s: Error!!! Allocating the new beacon",__func__);
            return -EINVAL;
       }

       pAdapter->sessionCtx.ap.beacon = new;

       status = wlan_hdd_cfg80211_start_bss(pAdapter, params);
    }

    EXIT();
    return status;
}

#endif //(LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static int wlan_hdd_cfg80211_del_beacon(struct wiphy *wiphy,
                                        struct net_device *dev)
#else
static int wlan_hdd_cfg80211_stop_ap (struct wiphy *wiphy,
                                      struct net_device *dev)
#endif
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t  *pHddCtx    = NULL;
    hdd_scaninfo_t *pScanInfo  = NULL;
    hdd_adapter_t  *staAdapter = NULL;
    VOS_STATUS status;
    long ret;

    ENTER();

    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: HDD adapter context is Null", __func__);
        return -ENODEV;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_STOP_AP,
                     pAdapter->sessionId, pAdapter->device_mode));
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    staAdapter = hdd_get_adapter(pAdapter->pHddCtx, WLAN_HDD_INFRA_STATION);
    if (NULL == staAdapter)
    {
        staAdapter = hdd_get_adapter(pAdapter->pHddCtx, WLAN_HDD_P2P_CLIENT);
        if (NULL == staAdapter)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                      "%s: HDD adapter context for STA/P2P-CLI is Null",
                      __func__);
        }
    }

    pScanInfo =  &pHddCtx->scan_info;

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s (%d)",
           __func__, hdd_device_modetoString(pAdapter->device_mode),
                                             pAdapter->device_mode);

    ret = wlan_hdd_scan_abort(pAdapter);

    if (ret < 0)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("Timeout occurred while waiting for abortscan %ld"), ret);

        if (pHddCtx->isLogpInProgress)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: LOGP in Progress. Ignore!!!", __func__);

            VOS_ASSERT(pScanInfo->mScanPending);
            return -EAGAIN;
        }
        VOS_ASSERT(pScanInfo->mScanPending);
    }

    /* Delete all associated STAs before stopping AP/P2P GO */
    hdd_del_all_sta(pAdapter);
    hdd_hostapd_stop(dev);

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
     || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {
        beacon_data_t *old;

        old = pAdapter->sessionCtx.ap.beacon;

        if (!old)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("session(%d) beacon data points to NULL"),
                      pAdapter->sessionId);
            return -ENOENT;
        }

        hdd_cleanup_actionframe(pHddCtx, pAdapter);

        mutex_lock(&pHddCtx->sap_lock);
        if(test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags))
        {
            if ( VOS_STATUS_SUCCESS == (status = WLANSAP_StopBss(pHddCtx->pvosContext) ) )
            {
                hdd_hostapd_state_t *pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

                status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);

                if (!VOS_IS_STATUS_SUCCESS(status))
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             ("ERROR: HDD vos wait for single_event failed!!"));
                    VOS_ASSERT(0);
                 }
             }
            clear_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags);
            /* BSS stopped, clear the active sessions for this device mode */
            wlan_hdd_decr_active_session(pHddCtx, pAdapter->device_mode);
        }
        mutex_unlock(&pHddCtx->sap_lock);

        if(status != VOS_STATUS_SUCCESS)
        {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                    "%s:Error!!! Stopping the BSS",__func__);
            return -EINVAL;
        }

        if (ccmCfgSetInt(pHddCtx->hHal,
            WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG, 0,NULL, eANI_BOOLEAN_FALSE)
                                                    ==eHAL_STATUS_FAILURE)
        {
            hddLog(LOGE,
               "Could not pass on WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG to CCM");
        }

        if ( eHAL_STATUS_FAILURE == ccmCfgSetInt(pHddCtx->hHal,
                            WNI_CFG_ASSOC_RSP_ADDNIE_FLAG, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
        {
            hddLog(LOGE,
               "Could not pass on WNI_CFG_ASSOC_RSP_ADDNIE_FLAG to CCM");
        }

        // Reset WNI_CFG_PROBE_RSP Flags
        wlan_hdd_reset_prob_rspies(pAdapter);

        pAdapter->sessionCtx.ap.beacon = NULL;
        kfree(old);
#ifdef WLAN_FEATURE_P2P_DEBUG
        if((pAdapter->device_mode == WLAN_HDD_P2P_GO) &&
           (globalP2PConnectionStatus == P2P_GO_COMPLETED_STATE))
        {
            hddLog(LOGE,"[P2P State] From GO completed to Inactive state "
                        "GO got removed");
            globalP2PConnectionStatus = P2P_NOT_ACTIVE;
        }
#endif
    }
    EXIT();
    return status;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,3,0))

static int wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
                                      struct net_device *dev,
                                      struct cfg80211_ap_settings *params)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    int            status;

    ENTER();

    if (NULL == dev)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Device is Null", __func__);
        return -ENODEV;
    }

    pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_START_AP, pAdapter->sessionId,
                     params-> beacon_interval));
    if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD adapter magic is invalid", __func__);
        return -ENODEV;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: device mode = %s (%d)",
           __func__, hdd_device_modetoString(pAdapter->device_mode),
                                             pAdapter->device_mode);

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
      || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {
        beacon_data_t  *old, *new;

        old = pAdapter->sessionCtx.ap.beacon;

        if (old)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                   FL("already beacon info added to session(%d)"),
                       pAdapter->sessionId);
            return -EALREADY;
        }

        status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter, &new, &params->beacon, params->dtim_period);

        if (status != 0)
        {
             hddLog(VOS_TRACE_LEVEL_FATAL,
                   "%s:Error!!! Allocating the new beacon", __func__);
             return -EINVAL;
        }
        pAdapter->sessionCtx.ap.beacon = new;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
        wlan_hdd_cfg80211_set_channel(wiphy, dev,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
        params->channel, params->channel_type);
#else
        params->chandef.chan, cfg80211_get_chandef_type(&(params->chandef)));
#endif
#endif
        status = wlan_hdd_cfg80211_start_bss(pAdapter, &params->beacon, params->ssid,
                                             params->ssid_len, params->hidden_ssid);
    }

    EXIT();
    return status;
}


static int wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
                                        struct net_device *dev,
                                        struct cfg80211_beacon_data *params)
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    int status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_CHANGE_BEACON,
                     pAdapter->sessionId, pAdapter->device_mode));
    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %d",
                                __func__, pAdapter->device_mode);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
     || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {
        beacon_data_t *old,*new;

        old = pAdapter->sessionCtx.ap.beacon;

        if (!old)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("session(%d) beacon data points to NULL"),
                      pAdapter->sessionId);
            return -ENOENT;
        }

        status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter, &new, params, 0);

        if(status != VOS_STATUS_SUCCESS) {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   "%s: Error!!! Allocating the new beacon",__func__);
            return -EINVAL;
       }

       pAdapter->sessionCtx.ap.beacon = new;

       status = wlan_hdd_cfg80211_start_bss(pAdapter, params, NULL, 0, 0);
    }

    EXIT();
    return status;
}

#endif //(LINUX_VERSION_CODE > KERNEL_VERSION(3,3,0))


static int __wlan_hdd_cfg80211_change_bss (struct wiphy *wiphy,
                                      struct net_device *dev,
                                      struct bss_parameters *params)
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_CHANGE_BSS,
                     pAdapter->sessionId, params->ap_isolate));
    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s (%d)",
           __func__, hdd_device_modetoString(pAdapter->device_mode),
                                             pAdapter->device_mode);

    if((pAdapter->device_mode == WLAN_HDD_SOFTAP)
     ||  (pAdapter->device_mode == WLAN_HDD_P2P_GO)
      )
    {
        /* ap_isolate == -1 means that in change bss, upper layer doesn't
         * want to update this parameter */
        if (-1 != params->ap_isolate)
        {
            pAdapter->sessionCtx.ap.apDisableIntraBssFwd = !!params->ap_isolate;
        }
    }

    EXIT();
    return 0;
}

static int wlan_hdd_cfg80211_change_bss (struct wiphy *wiphy,
                                      struct net_device *dev,
                                      struct bss_parameters *params)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_change_bss(wiphy, dev, params);
    vos_ssr_unprotect(__func__);

    return ret;
}
/* FUNCTION: wlan_hdd_change_country_code_cd
*  to wait for contry code completion
*/
void* wlan_hdd_change_country_code_cb(void *pAdapter)
{
    hdd_adapter_t *call_back_pAdapter = pAdapter;
    complete(&call_back_pAdapter->change_country_code);
    return NULL;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_change_iface
 * This function is used to set the interface type (INFRASTRUCTURE/ADHOC)
 */
int __wlan_hdd_cfg80211_change_iface( struct wiphy *wiphy,
                                    struct net_device *ndev,
                                    enum nl80211_iftype type,
                                    u32 *flags,
                                    struct vif_params *params
                                  )
{
    struct wireless_dev *wdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    hdd_context_t *pHddCtx;
    hdd_adapter_t  *pP2pAdapter = NULL;
    tCsrRoamProfile *pRoamProfile = NULL;
    eCsrRoamBssType LastBSSType;
    hdd_config_t *pConfig = NULL;
    eMib_dot11DesiredBssType connectedBssType;
    VOS_STATUS status;
    long ret;

    ENTER();

   if (!pAdapter)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Adapter context is null", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (!pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is null", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_CHANGE_IFACE,
                     pAdapter->sessionId, type));
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s (%d)",
            __func__, hdd_device_modetoString(pAdapter->device_mode),
                                              pAdapter->device_mode);

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Reached max concurrent connections"));
        return -EINVAL;
    }
    pConfig = pHddCtx->cfg_ini;
    wdev = ndev->ieee80211_ptr;

#ifdef WLAN_BTAMP_FEATURE
    if((NL80211_IFTYPE_P2P_CLIENT == type)||
       (NL80211_IFTYPE_ADHOC == type)||
       (NL80211_IFTYPE_AP == type)||
       (NL80211_IFTYPE_P2P_GO == type))
    {
        pHddCtx->isAmpAllowed = VOS_FALSE;
        // stop AMP traffic
        status = WLANBAP_StopAmp();
        if(VOS_STATUS_SUCCESS != status )
        {
            pHddCtx->isAmpAllowed = VOS_TRUE;
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   "%s: Failed to stop AMP", __func__);
            return -EINVAL;
        }
    }
#endif //WLAN_BTAMP_FEATURE
    /* Reset the current device mode bit mask*/
    wlan_hdd_clear_concurrency_mode(pHddCtx, pAdapter->device_mode);

    if( (pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
      || (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)
      || (pAdapter->device_mode == WLAN_HDD_P2P_DEVICE)
      )
    {
        hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
        if (!pWextState)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: pWextState is null", __func__);
            return VOS_STATUS_E_FAILURE;
        }
        pRoamProfile = &pWextState->roamProfile;
        LastBSSType = pRoamProfile->BSSType;

        switch (type)
        {
            case NL80211_IFTYPE_STATION:
            case NL80211_IFTYPE_P2P_CLIENT:
                hddLog(VOS_TRACE_LEVEL_INFO,
                   "%s: setting interface Type to INFRASTRUCTURE", __func__);
                pRoamProfile->BSSType = eCSR_BSS_TYPE_INFRASTRUCTURE;
#ifdef WLAN_FEATURE_11AC
                if(pConfig->dot11Mode == eHDD_DOT11_MODE_AUTO)
                {
                    pConfig->dot11Mode = eHDD_DOT11_MODE_11ac;
                }
#endif
                pRoamProfile->phyMode =
                hdd_cfg_xlate_to_csr_phy_mode(pConfig->dot11Mode);
                wdev->iftype = type;
                //Check for sub-string p2p to confirm its a p2p interface
                if (NULL != strstr(ndev->name,"p2p"))
                {
                    pAdapter->device_mode = (type == NL80211_IFTYPE_STATION) ?
                                WLAN_HDD_P2P_DEVICE : WLAN_HDD_P2P_CLIENT;
                }
                else
                {
                    pAdapter->device_mode = (type == NL80211_IFTYPE_STATION) ?
                                WLAN_HDD_INFRA_STATION: WLAN_HDD_P2P_CLIENT;
                }
#ifdef FEATURE_WLAN_TDLS
                /* The open adapter for the p2p shall skip initializations in
                 * tdls_init if the device mode is WLAN_HDD_P2P_DEVICE, for
                 * TDLS is supported only on WLAN_HDD_P2P_CLIENT. Hence invoke
                 * tdls_init when the change_iface sets the device mode to
                 * WLAN_HDD_P2P_CLIENT.
                 */

                if ( pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)
                {
                    if (0 != wlan_hdd_sta_tdls_init (pAdapter))
                    {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                            "%s: tdls initialization failed", __func__);
                        return -EINVAL;
                    }
                }
#endif

                break;
            case NL80211_IFTYPE_ADHOC:
                hddLog(VOS_TRACE_LEVEL_INFO,
                  "%s: setting interface Type to ADHOC", __func__);
                pRoamProfile->BSSType = eCSR_BSS_TYPE_START_IBSS;
                pRoamProfile->phyMode =
                    hdd_cfg_xlate_to_csr_phy_mode(pConfig->dot11Mode);
                pAdapter->device_mode = WLAN_HDD_IBSS;
                wdev->iftype = type;
                break;

            case NL80211_IFTYPE_AP:
            case NL80211_IFTYPE_P2P_GO:
            {
                hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                      "%s: setting interface Type to %s", __func__,
                      (type == NL80211_IFTYPE_AP) ? "SoftAP" : "P2pGo");

                //Cancel any remain on channel for GO mode
                if (NL80211_IFTYPE_P2P_GO == type)
                {
                    wlan_hdd_cancel_existing_remain_on_channel(pAdapter);
                }
                if (NL80211_IFTYPE_AP == type)
                {
                     /* As Loading WLAN Driver one interface being created for p2p device
                      * address. This will take one HW STA and the max number of clients
                      * that can connect to softAP will be reduced by one. so while changing
                      * the interface type to NL80211_IFTYPE_AP (SoftAP) remove p2p0
                      * interface as it is not required in SoftAP mode.
                      */

                    // Get P2P Adapter
                    pP2pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_DEVICE);

                    if (pP2pAdapter)
                    {
                        hdd_stop_adapter(pHddCtx, pP2pAdapter, VOS_TRUE);
                        hdd_deinit_adapter(pHddCtx, pP2pAdapter);
                        hdd_close_adapter(pHddCtx, pP2pAdapter, VOS_TRUE);
                    }
                }
                //Disable IMPS & BMPS for SAP/GO
                if(VOS_STATUS_E_FAILURE ==
                       hdd_disable_bmps_imps(pHddCtx, WLAN_HDD_P2P_GO))
                {
                    //Fail to Exit BMPS
                    VOS_ASSERT(0);
                }

                hdd_stop_adapter( pHddCtx, pAdapter, VOS_TRUE );

#ifdef FEATURE_WLAN_TDLS

                /* A Mutex Lock is introduced while changing the mode to
                 * protect the concurrent access for the Adapters by TDLS
                 * module.
                 */
                mutex_lock(&pHddCtx->tdls_lock);
#endif
                //De-init the adapter.
                hdd_deinit_adapter( pHddCtx, pAdapter );
                memset(&pAdapter->sessionCtx, 0, sizeof(pAdapter->sessionCtx));
                pAdapter->device_mode = (type == NL80211_IFTYPE_AP) ?
                                   WLAN_HDD_SOFTAP : WLAN_HDD_P2P_GO;
#ifdef FEATURE_WLAN_TDLS
                mutex_unlock(&pHddCtx->tdls_lock);
#endif
                if ((WLAN_HDD_SOFTAP == pAdapter->device_mode) &&
                    (pConfig->apRandomBssidEnabled))
                {
                    /* To meet Android requirements create a randomized
                       MAC address of the form 02:1A:11:Fx:xx:xx */
                    get_random_bytes(&ndev->dev_addr[3], 3);
                    ndev->dev_addr[0] = 0x02;
                    ndev->dev_addr[1] = 0x1A;
                    ndev->dev_addr[2] = 0x11;
                    ndev->dev_addr[3] |= 0xF0;
                    memcpy(pAdapter->macAddressCurrent.bytes, ndev->dev_addr,
                           VOS_MAC_ADDR_SIZE);
                    pr_info("wlan: Generated HotSpot BSSID " MAC_ADDRESS_STR"\n",
                            MAC_ADDR_ARRAY(ndev->dev_addr));
                }

                hdd_set_ap_ops( pAdapter->dev );

                /* This is for only SAP mode where users can
                 * control country through ini.
                 * P2P GO follows station country code
                 * acquired during the STA scanning. */
                if((NL80211_IFTYPE_AP == type) &&
                   (memcmp(pConfig->apCntryCode, CFG_AP_COUNTRY_CODE_DEFAULT, 3) != 0))
                {
                    int status = 0;
                    VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_INFO,
                         "%s: setting country code from INI ", __func__);
                    init_completion(&pAdapter->change_country_code);
                    status = (int)sme_ChangeCountryCode(pHddCtx->hHal,
                                     (void *)(tSmeChangeCountryCallback)
                                      wlan_hdd_change_country_code_cb,
                                      pConfig->apCntryCode, pAdapter,
                                      pHddCtx->pvosContext,
                                      eSIR_FALSE,
                                      eSIR_TRUE);
                    if (eHAL_STATUS_SUCCESS == status)
                    {
                        /* Wait for completion */
                        ret = wait_for_completion_interruptible_timeout(
                                       &pAdapter->change_country_code,
                                       msecs_to_jiffies(WLAN_WAIT_TIME_COUNTRY));
                        if (ret <= 0)
                        {
                            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             FL("SME Timed out while setting country code %ld"),
                                 ret);

                            if (pHddCtx->isLogpInProgress)
                            {
                                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                          "%s: LOGP in Progress. Ignore!!!", __func__);
                                return -EAGAIN;
                            }
                        }
                    }
                    else
                    {
                         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          "%s: SME Change Country code failed",__func__);
                         return -EINVAL;
                    }
                }
                status = hdd_init_ap_mode(pAdapter);
                if(status != VOS_STATUS_SUCCESS)
                {
                    hddLog(VOS_TRACE_LEVEL_FATAL,
                           "%s: Error initializing the ap mode", __func__);
                    return -EINVAL;
                }
                hdd_set_conparam(1);

                /*interface type changed update in wiphy structure*/
                if(wdev)
                {
                    wdev->iftype = type;
                    pHddCtx->change_iface = type;
                }
                else
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                            "%s: ERROR !!!! Wireless dev is NULL", __func__);
                    return -EINVAL;
                }
                goto done;
            }

            default:
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Unsupported interface Type",
                        __func__);
                return -EOPNOTSUPP;
        }
    }
    else if ( (pAdapter->device_mode == WLAN_HDD_SOFTAP)
           || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
            )
    {
       switch(type)
       {
           case NL80211_IFTYPE_STATION:
           case NL80211_IFTYPE_P2P_CLIENT:
           case NL80211_IFTYPE_ADHOC:

                hdd_stop_adapter( pHddCtx, pAdapter, VOS_TRUE );
#ifdef FEATURE_WLAN_TDLS

                /* A Mutex Lock is introduced while changing the mode to
                 * protect the concurrent access for the Adapters by TDLS
                 * module.
                 */
                mutex_lock(&pHddCtx->tdls_lock);
#endif
                hdd_deinit_adapter( pHddCtx, pAdapter );
                wdev->iftype = type;
                //Check for sub-string p2p to confirm its a p2p interface
                if (NULL != strstr(ndev->name,"p2p"))
                {
                    pAdapter->device_mode = (type == NL80211_IFTYPE_STATION) ?
                                  WLAN_HDD_P2P_DEVICE : WLAN_HDD_P2P_CLIENT;
                }
                else
                {
                    pAdapter->device_mode = (type == NL80211_IFTYPE_STATION) ?
                                  WLAN_HDD_INFRA_STATION: WLAN_HDD_P2P_CLIENT;
                }
                hdd_set_conparam(0);
                pHddCtx->change_iface = type;
                memset(&pAdapter->sessionCtx, 0, sizeof(pAdapter->sessionCtx));
                hdd_set_station_ops( pAdapter->dev );
#ifdef FEATURE_WLAN_TDLS
                mutex_unlock(&pHddCtx->tdls_lock);
#endif
                status = hdd_init_station_mode( pAdapter );
                if( VOS_STATUS_SUCCESS != status )
                    return -EOPNOTSUPP;
                /* In case of JB, for P2P-GO, only change interface will be called,
                 * This is the right place to enable back bmps_imps()
                 */
                if (pHddCtx->hdd_wlan_suspended)
                {
                    hdd_set_pwrparams(pHddCtx);
                }
                hdd_enable_bmps_imps(pHddCtx);
                goto done;
            case NL80211_IFTYPE_AP:
            case NL80211_IFTYPE_P2P_GO:
                wdev->iftype = type;
                pAdapter->device_mode = (type == NL80211_IFTYPE_AP) ?
                                        WLAN_HDD_SOFTAP : WLAN_HDD_P2P_GO;
               goto done;
           default:
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Unsupported interface Type",
                        __func__);
                return -EOPNOTSUPP;

       }

    }
    else
    {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: unsupported device mode(%s (%d))",
           __func__, hdd_device_modetoString(pAdapter->device_mode),
                                             pAdapter->device_mode);
      return -EOPNOTSUPP;
    }


    if(pRoamProfile)
    {
        if ( LastBSSType != pRoamProfile->BSSType )
        {
            /*interface type changed update in wiphy structure*/
            wdev->iftype = type;

            /*the BSS mode changed, We need to issue disconnect
              if connected or in IBSS disconnect state*/
            if ( hdd_connGetConnectedBssType(
                 WLAN_HDD_GET_STATION_CTX_PTR(pAdapter), &connectedBssType ) ||
                ( eCSR_BSS_TYPE_START_IBSS == LastBSSType ) )
            {
                /*need to issue a disconnect to CSR.*/
                INIT_COMPLETION(pAdapter->disconnect_comp_var);
                if( eHAL_STATUS_SUCCESS ==
                        sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                pAdapter->sessionId,
                                eCSR_DISCONNECT_REASON_UNSPECIFIED ) )
                {
                    ret = wait_for_completion_interruptible_timeout(
                                   &pAdapter->disconnect_comp_var,
                                   msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
                    if (ret <= 0)
                    {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                          FL("wait on disconnect_comp_var failed %ld"), ret);
                    }
                }
            }
        }
    }

done:
    /*set bitmask based on updated value*/
    wlan_hdd_set_concurrency_mode(pHddCtx, pAdapter->device_mode);

   /* Only STA mode support TM now
    * all other mode, TM feature should be disabled */
    if ( (pHddCtx->cfg_ini->thermalMitigationEnable) &&
         (~VOS_STA & pHddCtx->concurrency_mode) )
    {
        hddDevTmLevelChangedHandler(pHddCtx->parent_dev, 0);
    }

#ifdef WLAN_BTAMP_FEATURE
    if((NL80211_IFTYPE_STATION == type) && (pHddCtx->concurrency_mode <= 1) &&
       (pHddCtx->no_of_open_sessions[WLAN_HDD_INFRA_STATION] <=1))
    {
        //we are ok to do AMP
        pHddCtx->isAmpAllowed = VOS_TRUE;
    }
#endif //WLAN_BTAMP_FEATURE
    EXIT();
    return 0;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_change_iface
 * wrapper function to protect the actual implementation from SSR.
 */
int wlan_hdd_cfg80211_change_iface( struct wiphy *wiphy,
                                    struct net_device *ndev,
                                    enum nl80211_iftype type,
                                    u32 *flags,
                                    struct vif_params *params
                                  )
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_change_iface(wiphy, ndev, type, flags, params);
    vos_ssr_unprotect(__func__);

    return ret;
}

#ifdef FEATURE_WLAN_TDLS
static int wlan_hdd_tdls_add_station(struct wiphy *wiphy,
          struct net_device *dev, u8 *mac, bool update, tCsrStaParams *StaParams)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    VOS_STATUS status;
    hddTdlsPeer_t *pTdlsPeer;
    long ret;
    tANI_U16 numCurrTdlsPeers;

    ENTER();

    if (NULL == pHddCtx || NULL == pHddCtx->cfg_ini)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid arguments");
        return -EINVAL;
    }

    if ((eTDLS_SUPPORT_NOT_ENABLED == pHddCtx->tdls_mode) ||
        (eTDLS_SUPPORT_DISABLED == pHddCtx->tdls_mode))
    {
         VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: TDLS mode is disabled OR not enabled in FW."
                    MAC_ADDRESS_STR " Request declined.",
                    __func__, MAC_ADDR_ARRAY(mac));
        return -ENOTSUPP;
    }

    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s:LOGP in Progress. Ignore!!!", __func__);
        wlan_hdd_tdls_set_link_status(pAdapter,
                                      mac,
                                      eTDLS_LINK_IDLE,
                                      eTDLS_LINK_UNSPECIFIED);
        return -EBUSY;
    }

    pTdlsPeer = wlan_hdd_tdls_get_peer(pAdapter, mac);

    if ( NULL == pTdlsPeer ) {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
               "%s: " MAC_ADDRESS_STR " (update %d) not exist. return invalid",
               __func__, MAC_ADDR_ARRAY(mac), update);
        return -EINVAL;
    }

    /* in add station, we accept existing valid staId if there is */
    if ((0 == update) &&
        ((pTdlsPeer->link_status >= eTDLS_LINK_CONNECTING) ||
         (TDLS_STA_INDEX_VALID(pTdlsPeer->staId))))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                   "%s: " MAC_ADDRESS_STR
                   " link_status %d. staId %d. add station ignored.",
                   __func__, MAC_ADDR_ARRAY(mac), pTdlsPeer->link_status, pTdlsPeer->staId);
        return 0;
    }
    /* in change station, we accept only when staId is valid */
    if ((1 == update) &&
        ((pTdlsPeer->link_status > eTDLS_LINK_CONNECTING) ||
         (!TDLS_STA_INDEX_VALID(pTdlsPeer->staId))))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                   "%s: " MAC_ADDRESS_STR
                   " link status %d. staId %d. change station %s.",
                   __func__, MAC_ADDR_ARRAY(mac), pTdlsPeer->link_status, pTdlsPeer->staId,
                   (TDLS_STA_INDEX_VALID(pTdlsPeer->staId)) ? "ignored" : "declined");
        return (TDLS_STA_INDEX_VALID(pTdlsPeer->staId)) ? 0 : -EPERM;
    }

    /* when others are on-going, we want to change link_status to idle */
    if (NULL != wlan_hdd_tdls_is_progress(pHddCtx, mac, TRUE))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: " MAC_ADDRESS_STR
                   " TDLS setup is ongoing. Request declined.",
                   __func__, MAC_ADDR_ARRAY(mac));
        goto error;
    }

    /* first to check if we reached to maximum supported TDLS peer.
       TODO: for now, return -EPERM looks working fine,
       but need to check if any other errno fit into this category.*/
    numCurrTdlsPeers = wlan_hdd_tdlsConnectedPeers(pAdapter);
    if (HDD_MAX_NUM_TDLS_STA <= numCurrTdlsPeers)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: " MAC_ADDRESS_STR
                   " TDLS Max peer already connected. Request declined."
                   " Num of peers (%d), Max allowed (%d).",
                   __func__, MAC_ADDR_ARRAY(mac), numCurrTdlsPeers,
                   HDD_MAX_NUM_TDLS_STA);
        goto error;
    }
    else
    {
        hddTdlsPeer_t *pTdlsPeer;
        pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
        if (pTdlsPeer && TDLS_IS_CONNECTED(pTdlsPeer))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: " MAC_ADDRESS_STR " already connected. Request declined.",
                       __func__, MAC_ADDR_ARRAY(mac));
            return -EPERM;
        }
    }
    if (0 == update)
        wlan_hdd_tdls_set_link_status(pAdapter,
                                      mac,
                                      eTDLS_LINK_CONNECTING,
                                      eTDLS_LINK_SUCCESS);

    /* debug code */
    if (NULL != StaParams)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  "%s: TDLS Peer Parameters.", __func__);
        if(StaParams->htcap_present)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      "ht_capa->cap_info: %0x", StaParams->HTCap.capInfo);
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      "ht_capa->extended_capabilities: %0x",
                      StaParams->HTCap.extendedHtCapInfo);
        }
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  "params->capability: %0x",StaParams->capability);
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  "params->ext_capab_len: %0x",StaParams->extn_capability[0]);
        if(StaParams->vhtcap_present)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      "rxMcsMap %x rxHighest %x txMcsMap %x txHighest %x",
                      StaParams->VHTCap.suppMcs.rxMcsMap, StaParams->VHTCap.suppMcs.rxHighest,
                      StaParams->VHTCap.suppMcs.txMcsMap, StaParams->VHTCap.suppMcs.txHighest);
        }
        {
            int i = 0;
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "Supported rates:");
            for (i = 0; i < sizeof(StaParams->supported_rates); i++)
               VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                          "[%d]: %x ", i, StaParams->supported_rates[i]);
        }
    }  /* end debug code */
    else if ((1 == update) && (NULL == StaParams))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s : update is true, but staParams is NULL. Error!", __func__);
        return -EPERM;
    }

    INIT_COMPLETION(pAdapter->tdls_add_station_comp);

    if (!update)
    {
        status = sme_AddTdlsPeerSta(WLAN_HDD_GET_HAL_CTX(pAdapter),
                pAdapter->sessionId, mac);
    }
    else
    {
        status = sme_ChangeTdlsPeerSta(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                       pAdapter->sessionId, mac, StaParams);
    }

    ret = wait_for_completion_interruptible_timeout(&pAdapter->tdls_add_station_comp,
           msecs_to_jiffies(WAIT_TIME_TDLS_ADD_STA));

    if (ret <= 0)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: timeout waiting for tdls add station indication %ld",
                __func__, ret);
        return -EPERM;
    }

    if ( eHAL_STATUS_SUCCESS != pAdapter->tdlsAddStaStatus)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: Add Station is unsucessful", __func__);
        return -EPERM;
    }

    return 0;

error:
    wlan_hdd_tdls_set_link_status(pAdapter,
                                  mac,
                                  eTDLS_LINK_IDLE,
                                  eTDLS_LINK_UNSPECIFIED);
    return -EPERM;

}
#endif

static int wlan_hdd_change_station(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         u8 *mac,
                                         struct station_parameters *params)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_context_t *pHddCtx;
    hdd_station_ctx_t *pHddStaCtx;
    v_MACADDR_t STAMacAddress;
#ifdef FEATURE_WLAN_TDLS
    tCsrStaParams StaParams = {0};
    tANI_U8 isBufSta = 0;
    tANI_U8 isOffChannelSupported = 0;
#endif
    ENTER();

    if ((NULL == pAdapter))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "invalid adapter ");
        return -EINVAL;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CHANGE_STATION,
                      pAdapter->sessionId, params->listen_interval));
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    if ((NULL == pHddCtx) || (NULL == pHddStaCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "invalid HDD state or HDD station context");
        return -EINVAL;
    }

    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EAGAIN;
    }

    vos_mem_copy(STAMacAddress.bytes, mac, sizeof(v_MACADDR_t));

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
      || (pAdapter->device_mode == WLAN_HDD_P2P_GO))
    {
        if (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED))
        {
            status = hdd_softap_change_STA_state( pAdapter, &STAMacAddress,
                                                  WLANTL_STA_AUTHENTICATED);

            if (status != VOS_STATUS_SUCCESS)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         "%s: Not able to change TL state to AUTHENTICATED", __func__);
                return -EINVAL;
            }
        }
    }
    else if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
          || (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)) {
#ifdef FEATURE_WLAN_TDLS
        if (params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
            StaParams.capability = params->capability;
            StaParams.uapsd_queues = params->uapsd_queues;
            StaParams.max_sp = params->max_sp;

            /* Convert (first channel , number of channels) tuple to
             * the total list of channels. This goes with the assumption
             * that if the first channel is < 14, then the next channels
             * are an incremental of 1 else an incremental of 4 till the number
             * of channels.
             */
            if (0 != params->supported_channels_len) {
                int i = 0,j = 0,k = 0, no_of_channels = 0 ;
                for ( i = 0 ; i < params->supported_channels_len ; i+=2)
                {
                    int wifi_chan_index;
                    StaParams.supported_channels[j] = params->supported_channels[i];
                    wifi_chan_index =
                        ((StaParams.supported_channels[j] <= HDD_CHANNEL_14 ) ? 1 : 4 );
                    no_of_channels = params->supported_channels[i+1];
                    for(k=1; k <= no_of_channels; k++)
                    {
                        StaParams.supported_channels[j+1] =
                              StaParams.supported_channels[j] + wifi_chan_index;
                        j+=1;
                    }
                }
                StaParams.supported_channels_len = j;
            }
            vos_mem_copy(StaParams.supported_oper_classes,
                         params->supported_oper_classes,
                         params->supported_oper_classes_len);
            StaParams.supported_oper_classes_len  =
                                             params->supported_oper_classes_len;

            if (0 != params->ext_capab_len)
                vos_mem_copy(StaParams.extn_capability, params->ext_capab,
                             sizeof(StaParams.extn_capability));

            if (NULL != params->ht_capa)
            {
                StaParams.htcap_present = 1;
                vos_mem_copy(&StaParams.HTCap, params->ht_capa, sizeof(tSirHTCap));
            }

            StaParams.supported_rates_len = params->supported_rates_len;

            /* Note : The Maximum sizeof supported_rates sent by the Supplicant is 32.
             * The supported_rates array , for all the structures propogating till Add Sta
             * to the firmware has to be modified , if the supplicant (ieee80211) is
             * modified to send more rates.
             */

            /* To avoid Data Currption , set to max length to SIR_MAC_MAX_SUPP_RATES
             */
            if (StaParams.supported_rates_len > SIR_MAC_MAX_SUPP_RATES)
                StaParams.supported_rates_len = SIR_MAC_MAX_SUPP_RATES;

            if (0 != StaParams.supported_rates_len) {
                int i = 0;
                vos_mem_copy(StaParams.supported_rates, params->supported_rates,
                             StaParams.supported_rates_len);
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                           "Supported Rates with Length %d", StaParams.supported_rates_len);
                for (i=0; i < StaParams.supported_rates_len; i++)
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                               "[%d]: %0x", i, StaParams.supported_rates[i]);
            }

            if (NULL != params->vht_capa)
            {
                StaParams.vhtcap_present = 1;
                vos_mem_copy(&StaParams.VHTCap, params->vht_capa, sizeof(tSirVHTCap));
            }

            if (0 != params->ext_capab_len ) {
                /*Define A Macro : TODO Sunil*/
                if ((1<<4) & StaParams.extn_capability[3]) {
                    isBufSta = 1;
                }
                /* TDLS Channel Switching Support */
                if ((1<<6) & StaParams.extn_capability[3]) {
                    isOffChannelSupported = 1;
                }
            }
            status = wlan_hdd_tdls_set_peer_caps( pAdapter, mac,
                                                  &StaParams, isBufSta,
                                                  isOffChannelSupported);

            if (VOS_STATUS_SUCCESS != status) {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          "%s: wlan_hdd_tdls_set_peer_caps failed!", __func__);
                return -EINVAL;
            }
            status = wlan_hdd_tdls_add_station(wiphy, dev, mac, 1, &StaParams);

            if (VOS_STATUS_SUCCESS != status) {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          "%s: sme_ChangeTdlsPeerSta failed!", __func__);
                return -EINVAL;
            }
        }
#endif
    }
    EXIT();
    return status;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_add_key
 * This function is used to initialize the key information
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int __wlan_hdd_cfg80211_add_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index, bool pairwise,
                                      const u8 *mac_addr,
                                      struct key_params *params
                                      )
#else
static int __wlan_hdd_cfg80211_add_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index, const u8 *mac_addr,
                                      struct key_params *params
                                      )
#endif
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    tCsrRoamSetKey  setKey;
    u8 groupmacaddr[WNI_CFG_BSSID_LEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int status;
    v_U32_t roamId= 0xFF;
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
    hdd_hostapd_state_t *pHostapdState;
    VOS_STATUS vos_status;
    eHalStatus halStatus;
    hdd_context_t *pHddCtx;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_ADD_KEY,
                      pAdapter->sessionId, params->key_len));
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s (%d)",
            __func__, hdd_device_modetoString(pAdapter->device_mode),
                                              pAdapter->device_mode);

    if (CSR_MAX_NUM_KEY <= key_index)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid key index %d", __func__,
                key_index);

        return -EINVAL;
    }

    if (CSR_MAX_KEY_LEN < params->key_len)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid key length %d", __func__,
                params->key_len);

        return -EINVAL;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           "%s: called with key index = %d & key length %d",
           __func__, key_index, params->key_len);

    /*extract key idx, key len and key*/
    vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
    setKey.keyId = key_index;
    setKey.keyLength = params->key_len;
    vos_mem_copy(&setKey.Key[0],params->key, params->key_len);

    switch (params->cipher)
    {
        case WLAN_CIPHER_SUITE_WEP40:
            setKey.encType = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
            break;

        case WLAN_CIPHER_SUITE_WEP104:
            setKey.encType = eCSR_ENCRYPT_TYPE_WEP104_STATICKEY;
            break;

        case WLAN_CIPHER_SUITE_TKIP:
            {
                u8 *pKey = &setKey.Key[0];
                setKey.encType = eCSR_ENCRYPT_TYPE_TKIP;

                vos_mem_zero(pKey, CSR_MAX_KEY_LEN);

                /*Supplicant sends the 32bytes key in this order

                  |--------------|----------|----------|
                  |   Tk1        |TX-MIC    |  RX Mic  |
                  |--------------|----------|----------|
                  <---16bytes---><--8bytes--><--8bytes-->

                */
                /*Sme expects the 32 bytes key to be in the below order

                  |--------------|----------|----------|
                  |   Tk1        |RX-MIC    |  TX Mic  |
                  |--------------|----------|----------|
                  <---16bytes---><--8bytes--><--8bytes-->
                  */
                /* Copy the Temporal Key 1 (TK1) */
                vos_mem_copy(pKey, params->key, 16);

                /*Copy the rx mic first*/
                vos_mem_copy(&pKey[16], &params->key[24], 8);

                /*Copy the tx mic */
                vos_mem_copy(&pKey[24], &params->key[16], 8);


                break;
            }

        case WLAN_CIPHER_SUITE_CCMP:
            setKey.encType = eCSR_ENCRYPT_TYPE_AES;
            break;

#ifdef FEATURE_WLAN_WAPI
        case WLAN_CIPHER_SUITE_SMS4:
            {
                vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
                wlan_hdd_cfg80211_set_key_wapi(pAdapter, key_index, mac_addr,
                        params->key, params->key_len);
                return 0;
            }
#endif

#ifdef FEATURE_WLAN_ESE
        case WLAN_CIPHER_SUITE_KRK:
            setKey.encType = eCSR_ENCRYPT_TYPE_KRK;
            break;
#endif

#ifdef WLAN_FEATURE_11W
        case WLAN_CIPHER_SUITE_AES_CMAC:
            setKey.encType = eCSR_ENCRYPT_TYPE_AES_CMAC;
            break;
#endif

        default:
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: unsupported cipher type %u",
                    __func__, params->cipher);
            status = -EOPNOTSUPP;
            goto end;
    }

    hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: encryption type %d",
            __func__, setKey.encType);

    if (
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
                (!pairwise)
#else
                (!mac_addr || is_broadcast_ether_addr(mac_addr))
#endif
       )
    {
        /* set group key*/
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s- %d: setting Broadcast key",
                __func__, __LINE__);
        setKey.keyDirection = eSIR_RX_ONLY;
        vos_mem_copy(setKey.peerMac,groupmacaddr,WNI_CFG_BSSID_LEN);
    }
    else
    {
        /* set pairwise key*/
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s- %d: setting pairwise key",
                __func__, __LINE__);
        setKey.keyDirection = eSIR_TX_RX;
        vos_mem_copy(setKey.peerMac, mac_addr,WNI_CFG_BSSID_LEN);
    }
    if ((WLAN_HDD_IBSS == pAdapter->device_mode) && !pairwise)
    {
        setKey.keyDirection = eSIR_TX_RX;
        /*Set the group key*/
        status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
            pAdapter->sessionId, &setKey, &roamId );

        if ( 0 != status )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: sme_RoamSetKey failed, returned %d", __func__, status);
            status = -EINVAL;
            goto end;
        }
        /*Save the keys here and call sme_RoamSetKey for setting
          the PTK after peer joins the IBSS network*/
        vos_mem_copy(&pAdapter->sessionCtx.station.ibss_enc_key,
                                    &setKey, sizeof(tCsrRoamSetKey));
        goto end;
    }
    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
           (pAdapter->device_mode == WLAN_HDD_P2P_GO))
    {
        pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
        if( pHostapdState->bssState == BSS_START )
        {
            hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
            vos_status = wlan_hdd_check_ula_done(pAdapter);

            if ( vos_status != VOS_STATUS_SUCCESS )
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        "[%4d] wlan_hdd_check_ula_done returned ERROR status= %d",
                        __LINE__, vos_status );

                pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;

                status = -EINVAL;
                goto end;
            }

            status = WLANSAP_SetKeySta( pVosContext, &setKey);

            if ( status != eHAL_STATUS_SUCCESS )
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        "[%4d] WLANSAP_SetKeySta returned ERROR status= %d",
                        __LINE__, status );
                status = -EINVAL;
                goto end;
            }
        }

        /* Saving WEP keys */
        else if( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY  == setKey.encType ||
                eCSR_ENCRYPT_TYPE_WEP104_STATICKEY  == setKey.encType  )
        {
            //Save the wep key in ap context. Issue setkey after the BSS is started.
            hdd_ap_ctx_t *pAPCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
            vos_mem_copy(&pAPCtx->wepKey[key_index], &setKey, sizeof(tCsrRoamSetKey));
        }
        else
        {
            //Save the key in ap context. Issue setkey after the BSS is started.
            hdd_ap_ctx_t *pAPCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
            vos_mem_copy(&pAPCtx->groupKey, &setKey, sizeof(tCsrRoamSetKey));
        }
    }
    else if ( (pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
              (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) )
    {
        hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
        hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
        if (!pairwise)
#else
        if (!mac_addr || is_broadcast_ether_addr(mac_addr))
#endif
        {
            /* set group key*/
            if (pHddStaCtx->roam_info.deferKeyComplete)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                           "%s- %d: Perform Set key Complete",
                           __func__, __LINE__);
                hdd_PerformRoamSetKeyComplete(pAdapter);
            }
        }

        pWextState->roamProfile.Keys.KeyLength[key_index] = (u8)params->key_len;

        pWextState->roamProfile.Keys.defaultIndex = key_index;


        vos_mem_copy(&pWextState->roamProfile.Keys.KeyMaterial[key_index][0],
                params->key, params->key_len);


        pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;

        hddLog(VOS_TRACE_LEVEL_INFO_MED,
                "%s: set key for peerMac %2x:%2x:%2x:%2x:%2x:%2x, direction %d",
                __func__, setKey.peerMac[0], setKey.peerMac[1],
                setKey.peerMac[2], setKey.peerMac[3],
                setKey.peerMac[4], setKey.peerMac[5],
                setKey.keyDirection);

        vos_status = wlan_hdd_check_ula_done(pAdapter);

        if ( vos_status != VOS_STATUS_SUCCESS )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "[%4d] wlan_hdd_check_ula_done returned ERROR status= %d",
                    __LINE__, vos_status );

            pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;

            status = -EINVAL;
            goto end;

        }

#ifdef WLAN_FEATURE_VOWIFI_11R
        /* The supplicant may attempt to set the PTK once pre-authentication
           is done. Save the key in the UMAC and include it in the ADD BSS
           request */
        halStatus = sme_FTUpdateKey( WLAN_HDD_GET_HAL_CTX(pAdapter), &setKey);
        if ( halStatus == eHAL_STATUS_FT_PREAUTH_KEY_SUCCESS )
        {
           hddLog(VOS_TRACE_LEVEL_INFO_MED,
                  "%s: Update PreAuth Key success", __func__);
           status = 0;
           goto end;
        }
        else if ( halStatus == eHAL_STATUS_FT_PREAUTH_KEY_FAILED )
        {
           hddLog(VOS_TRACE_LEVEL_ERROR,
                  "%s: Update PreAuth Key failed", __func__);
           status = -EINVAL;
           goto end;
        }
#endif /* WLAN_FEATURE_VOWIFI_11R */

        /* issue set key request to SME*/
        status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                pAdapter->sessionId, &setKey, &roamId );

        if ( 0 != status )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: sme_RoamSetKey failed, returned %d", __func__, status);
            pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
            status = -EINVAL;
            goto end;
        }


        /* in case of IBSS as there was no information available about WEP keys during
         * IBSS join, group key intialized with NULL key, so re-initialize group key
         * with correct value*/
        if ( (eCSR_BSS_TYPE_START_IBSS == pWextState->roamProfile.BSSType) &&
            !(  ( IW_AUTH_KEY_MGMT_802_1X
                    == (pWextState->authKeyMgmt & IW_AUTH_KEY_MGMT_802_1X))
                    && (eCSR_AUTH_TYPE_OPEN_SYSTEM == pHddStaCtx->conn_info.authType)
                 )
                &&
                (  (WLAN_CIPHER_SUITE_WEP40 == params->cipher)
                   || (WLAN_CIPHER_SUITE_WEP104 == params->cipher)
                )
           )
        {
            setKey.keyDirection = eSIR_RX_ONLY;
            vos_mem_copy(setKey.peerMac,groupmacaddr,WNI_CFG_BSSID_LEN);

            hddLog(VOS_TRACE_LEVEL_INFO_MED,
                    "%s: set key peerMac %2x:%2x:%2x:%2x:%2x:%2x, direction %d",
                    __func__, setKey.peerMac[0], setKey.peerMac[1],
                    setKey.peerMac[2], setKey.peerMac[3],
                    setKey.peerMac[4], setKey.peerMac[5],
                    setKey.keyDirection);

            status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                    pAdapter->sessionId, &setKey, &roamId );

            if ( 0 != status )
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: sme_RoamSetKey failed for group key (IBSS), returned %d",
                        __func__, status);
                pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
                status = -EINVAL;
                goto end;
            }
        }
    }

end:
    /* Need to clear any trace of key value in the memory.
     * Thus zero out the memory even though it is local
     * variable.
     */
    vos_mem_zero(&setKey, sizeof(setKey));

    return status;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int wlan_hdd_cfg80211_add_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index, bool pairwise,
                                      const u8 *mac_addr,
                                      struct key_params *params
                                      )
#else
static int wlan_hdd_cfg80211_add_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index, const u8 *mac_addr,
                                      struct key_params *params
                                      )
#endif
{
    int ret;
    vos_ssr_protect(__func__);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
    ret = __wlan_hdd_cfg80211_add_key(wiphy, ndev, key_index, pairwise,
                                      mac_addr, params);
#else
    ret = __wlan_hdd_cfg80211_add_key(wiphy, ndev, key_index, mac_addr,
                                       params);
#endif
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_get_key
 * This function is used to get the key information
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int __wlan_hdd_cfg80211_get_key(
                        struct wiphy *wiphy,
                        struct net_device *ndev,
                        u8 key_index, bool pairwise,
                        const u8 *mac_addr, void *cookie,
                        void (*callback)(void *cookie, struct key_params*)
                        )
#else
static int __wlan_hdd_cfg80211_get_key(
                        struct wiphy *wiphy,
                        struct net_device *ndev,
                        u8 key_index, const u8 *mac_addr, void *cookie,
                        void (*callback)(void *cookie, struct key_params*)
                        )
#endif
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    hdd_wext_state_t *pWextState= WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrRoamProfile *pRoamProfile = &(pWextState->roamProfile);
    struct key_params params;

    ENTER();

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s (%d)",
            __func__, hdd_device_modetoString(pAdapter->device_mode),
                                              pAdapter->device_mode);

    memset(&params, 0, sizeof(params));

    if (CSR_MAX_NUM_KEY <= key_index)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("invalid key index %d"), key_index);
        return -EINVAL;
    }

    switch(pRoamProfile->EncryptionType.encryptionType[0])
    {
        case eCSR_ENCRYPT_TYPE_NONE:
            params.cipher = IW_AUTH_CIPHER_NONE;
            break;

        case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
        case eCSR_ENCRYPT_TYPE_WEP40:
            params.cipher = WLAN_CIPHER_SUITE_WEP40;
            break;

        case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
        case eCSR_ENCRYPT_TYPE_WEP104:
            params.cipher = WLAN_CIPHER_SUITE_WEP104;
            break;

        case eCSR_ENCRYPT_TYPE_TKIP:
            params.cipher = WLAN_CIPHER_SUITE_TKIP;
            break;

        case eCSR_ENCRYPT_TYPE_AES:
            params.cipher = WLAN_CIPHER_SUITE_AES_CMAC;
            break;

        default:
            params.cipher = IW_AUTH_CIPHER_NONE;
            break;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_GET_KEY,
                     pAdapter->sessionId, params.cipher));

    params.key_len = pRoamProfile->Keys.KeyLength[key_index];
    params.seq_len = 0;
    params.seq = NULL;
    params.key = &pRoamProfile->Keys.KeyMaterial[key_index][0];
    callback(cookie, &params);
    return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int wlan_hdd_cfg80211_get_key(
                        struct wiphy *wiphy,
                        struct net_device *ndev,
                        u8 key_index, bool pairwise,
                        const u8 *mac_addr, void *cookie,
                        void (*callback)(void *cookie, struct key_params*)
                        )
#else
static int wlan_hdd_cfg80211_get_key(
                        struct wiphy *wiphy,
                        struct net_device *ndev,
                        u8 key_index, const u8 *mac_addr, void *cookie,
                        void (*callback)(void *cookie, struct key_params*)
                        )
#endif
{
    int ret;

    vos_ssr_protect(__func__);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
    ret = __wlan_hdd_cfg80211_get_key(wiphy, ndev, key_index, pairwise,
                                    mac_addr, cookie, callback);
#else
    ret = __wlan_hdd_cfg80211_get_key(wiphy, ndev, key_index, mac_addr,
                                    callback);
#endif
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_del_key
 * This function is used to delete the key information
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int wlan_hdd_cfg80211_del_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index,
                                      bool pairwise,
                                      const u8 *mac_addr
                                    )
#else
static int wlan_hdd_cfg80211_del_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index,
                                      const u8 *mac_addr
                                    )
#endif
{
    int status = 0;

    //This code needs to be revisited. There is sme_removeKey API, we should
    //plan to use that. After the change to use correct index in setkey,
    //it is observed that this is invalidating peer
    //key index whenever re-key is done. This is affecting data link.
    //It should be ok to ignore del_key.
#if 0
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
    u8 groupmacaddr[WNI_CFG_BSSID_LEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    tCsrRoamSetKey  setKey;
    v_U32_t roamId= 0xFF;

    ENTER();

    hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: device_mode = %d\n",
                                     __func__,pAdapter->device_mode);

    if (CSR_MAX_NUM_KEY <= key_index)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid key index %d", __func__,
                key_index);

        return -EINVAL;
    }

    vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
    setKey.keyId = key_index;

    if (mac_addr)
        vos_mem_copy(setKey.peerMac, mac_addr,WNI_CFG_BSSID_LEN);
    else
        vos_mem_copy(setKey.peerMac, groupmacaddr, WNI_CFG_BSSID_LEN);

    setKey.encType = eCSR_ENCRYPT_TYPE_NONE;

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
      || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {

        hdd_hostapd_state_t *pHostapdState =
                                  WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
        if( pHostapdState->bssState == BSS_START)
        {
            status = WLANSAP_SetKeySta( pVosContext, &setKey);

            if ( status != eHAL_STATUS_SUCCESS )
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "[%4d] WLANSAP_SetKeySta returned ERROR status= %d",
                     __LINE__, status );
            }
        }
    }
    else if ( (pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
           || (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)
            )
    {
        hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

        pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;

        hddLog(VOS_TRACE_LEVEL_INFO_MED,
                "%s: delete key for peerMac %2x:%2x:%2x:%2x:%2x:%2x",
                __func__, setKey.peerMac[0], setKey.peerMac[1],
                setKey.peerMac[2], setKey.peerMac[3],
                setKey.peerMac[4], setKey.peerMac[5]);
        if(pAdapter->sessionCtx.station.conn_info.connState ==
                                       eConnectionState_Associated)
        {
            status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                   pAdapter->sessionId, &setKey, &roamId );

            if ( 0 != status )
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: sme_RoamSetKey failure, returned %d",
                                                     __func__, status);
                pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
                return -EINVAL;
            }
        }
    }
#endif
    EXIT();
    return status;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_default_key
 * This function is used to set the default tx key index
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int __wlan_hdd_cfg80211_set_default_key( struct wiphy *wiphy,
                                              struct net_device *ndev,
                                              u8 key_index,
                                              bool unicast, bool multicast)
#else
static int __wlan_hdd_cfg80211_set_default_key( struct wiphy *wiphy,
                                              struct net_device *ndev,
                                              u8 key_index)
#endif
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    int status;
    hdd_wext_state_t *pWextState;
    hdd_station_ctx_t *pHddStaCtx;
    hdd_context_t *pHddCtx;

    ENTER();

    if ((NULL == pAdapter))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
          "invalid adapter");
       return -EINVAL;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_DEFAULT_KEY,
                     pAdapter->sessionId, key_index));

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    if ((NULL == pWextState) || (NULL == pHddStaCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "invalid Wext state or HDD context");
        return -EINVAL;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %d key_index = %d",
                                         __func__,pAdapter->device_mode, key_index);

    if (CSR_MAX_NUM_KEY <= key_index)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid key index %d", __func__,
                key_index);

        return -EINVAL;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
     || (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)
       )
    {
        if ( (eCSR_ENCRYPT_TYPE_TKIP !=
                pHddStaCtx->conn_info.ucEncryptionType) &&
             (eCSR_ENCRYPT_TYPE_AES !=
                pHddStaCtx->conn_info.ucEncryptionType)
           )
        {
            /* if default key index is not same as previous one,
             * then update the default key index */

            tCsrRoamSetKey  setKey;
            v_U32_t roamId= 0xFF;
            tCsrKeys *Keys = &pWextState->roamProfile.Keys;

            hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: default tx key index %d",
                    __func__, key_index);

            Keys->defaultIndex = (u8)key_index;
            vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
            setKey.keyId = key_index;
            setKey.keyLength = Keys->KeyLength[key_index];

            vos_mem_copy(&setKey.Key[0],
                    &Keys->KeyMaterial[key_index][0],
                    Keys->KeyLength[key_index]);

            setKey.keyDirection = eSIR_TX_RX;

            vos_mem_copy(setKey.peerMac,
                    &pHddStaCtx->conn_info.bssId[0],
                    WNI_CFG_BSSID_LEN);

            if (Keys->KeyLength[key_index] == CSR_WEP40_KEY_LEN &&
               pWextState->roamProfile.EncryptionType.encryptionType[0] ==
               eCSR_ENCRYPT_TYPE_WEP104)
            {
                /*In the case of dynamic wep supplicant hardcodes DWEP type to eCSR_ENCRYPT_TYPE_WEP104
                 even though ap is configured for WEP-40 encryption. In this canse the key length
                 is 5 but the encryption type is 104 hence checking the key langht(5) and encryption
                 type(104) and switching encryption type to 40*/
                pWextState->roamProfile.EncryptionType.encryptionType[0] =
                   eCSR_ENCRYPT_TYPE_WEP40;
                pWextState->roamProfile.mcEncryptionType.encryptionType[0] =
                   eCSR_ENCRYPT_TYPE_WEP40;
            }

            setKey.encType =
                pWextState->roamProfile.EncryptionType.encryptionType[0];

            /* issue set key request */
            status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                   pAdapter->sessionId, &setKey, &roamId );

            if ( 0 != status )
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: sme_RoamSetKey failed, returned %d", __func__,
                        status);
                return -EINVAL;
            }
        }
    }

    /* In SoftAp mode setting key direction for default mode */
    else if ( WLAN_HDD_SOFTAP == pAdapter->device_mode )
    {
        if ( (eCSR_ENCRYPT_TYPE_TKIP !=
                pWextState->roamProfile.EncryptionType.encryptionType[0]) &&
             (eCSR_ENCRYPT_TYPE_AES !=
                pWextState->roamProfile.EncryptionType.encryptionType[0])
           )
        {
            /*  Saving key direction for default key index to TX default */
            hdd_ap_ctx_t *pAPCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
            pAPCtx->wepKey[key_index].keyDirection = eSIR_TX_DEFAULT;
        }
    }

    return status;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int wlan_hdd_cfg80211_set_default_key( struct wiphy *wiphy,
                                              struct net_device *ndev,
                                              u8 key_index,
                                              bool unicast, bool multicast)
#else
static int wlan_hdd_cfg80211_set_default_key( struct wiphy *wiphy,
                                              struct net_device *ndev,
                                              u8 key_index)
#endif
{
    int ret;
    vos_ssr_protect(__func__);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
    ret = __wlan_hdd_cfg80211_set_default_key(wiphy, ndev, key_index, unicast,
                                              multicast);
#else
    ret = __wlan_hdd_cfg80211_set_default_key(wiphy, ndev, key_index);
#endif
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_inform_bss
 * This function is used to inform the BSS details to nl80211 interface.
 */
static struct cfg80211_bss* wlan_hdd_cfg80211_inform_bss(
                    hdd_adapter_t *pAdapter, tCsrRoamConnectedProfile *roamProfile)
{
    struct net_device *dev = pAdapter->dev;
    struct wireless_dev *wdev = dev->ieee80211_ptr;
    struct wiphy *wiphy = wdev->wiphy;
    tSirBssDescription *pBssDesc = roamProfile->pBssDesc;
    int chan_no;
    int ie_length;
    const char *ie;
    unsigned int freq;
    struct ieee80211_channel *chan;
    int rssi = 0;
    struct cfg80211_bss *bss = NULL;

    ENTER();

    if( NULL == pBssDesc )
    {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: pBssDesc is NULL", __func__);
        return bss;
    }

    chan_no = pBssDesc->channelId;
    ie_length = GET_IE_LEN_IN_BSS_DESC( pBssDesc->length );
    ie =  ((ie_length != 0) ? (const char *)&pBssDesc->ieFields: NULL);

    if( NULL == ie )
    {
       hddLog(VOS_TRACE_LEVEL_FATAL, "%s: IE of BSS descriptor is NULL", __func__);
       return bss;
    }

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,38))
    if (chan_no <= ARRAY_SIZE(hdd_channels_2_4_GHZ))
    {
        freq = ieee80211_channel_to_frequency(chan_no, IEEE80211_BAND_2GHZ);
    }
    else
    {
        freq = ieee80211_channel_to_frequency(chan_no, IEEE80211_BAND_5GHZ);
    }
#else
    freq = ieee80211_channel_to_frequency(chan_no);
#endif

    chan = __ieee80211_get_channel(wiphy, freq);

    if (!chan) {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s chan pointer is NULL", __func__);
       return NULL;
    }

    rssi = (VOS_MIN ((pBssDesc->rssi + pBssDesc->sinr), 0))*100;

    return cfg80211_inform_bss(wiphy, chan, pBssDesc->bssId,
                le64_to_cpu(*(__le64 *)pBssDesc->timeStamp),
                pBssDesc->capabilityInfo,
                pBssDesc->beaconInterval, ie, ie_length,
                rssi, GFP_KERNEL );
}



/*
 * FUNCTION: wlan_hdd_cfg80211_inform_bss_frame
 * This function is used to inform the BSS details to nl80211 interface.
 */
struct cfg80211_bss*
wlan_hdd_cfg80211_inform_bss_frame( hdd_adapter_t *pAdapter,
                                    tSirBssDescription *bss_desc
                                    )
{
    /*
      cfg80211_inform_bss() is not updating ie field of bss entry, if entry
      already exists in bss data base of cfg80211 for that particular BSS ID.
      Using cfg80211_inform_bss_frame to update the bss entry instead of
      cfg80211_inform_bss, But this call expects mgmt packet as input. As of
      now there is no possibility to get the mgmt(probe response) frame from PE,
      converting bss_desc to ieee80211_mgmt(probe response) and passing to
      cfg80211_inform_bss_frame.
     */
    struct net_device *dev = pAdapter->dev;
    struct wireless_dev *wdev = dev->ieee80211_ptr;
    struct wiphy *wiphy = wdev->wiphy;
    int chan_no = bss_desc->channelId;
#ifdef WLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
    qcom_ie_age *qie_age = NULL;
    int ie_length = GET_IE_LEN_IN_BSS_DESC( bss_desc->length ) + sizeof(qcom_ie_age);
#else
    int ie_length = GET_IE_LEN_IN_BSS_DESC( bss_desc->length );
#endif
    const char *ie =
        ((ie_length != 0) ? (const char *)&bss_desc->ieFields: NULL);
    unsigned int freq;
    struct ieee80211_channel *chan;
    struct ieee80211_mgmt *mgmt = NULL;
    struct cfg80211_bss *bss_status = NULL;
    size_t frame_len = sizeof (struct ieee80211_mgmt) + ie_length;
    int rssi = 0;
    hdd_context_t *pHddCtx;
    int status;
#ifdef WLAN_OPEN_SOURCE
    struct timespec ts;
#endif

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    /*bss_update is not allowed during wlan driver loading or unloading*/
    if (WLAN_HDD_IS_LOAD_UNLOAD_IN_PROGRESS(pHddCtx))
    {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s:Loading_unloading in Progress. Ignore!!!",__func__);
         return NULL;
    }


    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return NULL;
    }

    mgmt = kzalloc((sizeof (struct ieee80211_mgmt) + ie_length), GFP_KERNEL);
    if (!mgmt)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: memory allocation failed ", __func__);
        return NULL;
    }

    memcpy(mgmt->bssid, bss_desc->bssId, ETH_ALEN);

#ifdef WLAN_OPEN_SOURCE
    /* Android does not want the timestamp from the frame.
       Instead it wants a monotonic increasing value */
    get_monotonic_boottime(&ts);
    mgmt->u.probe_resp.timestamp =
         ((u64)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
#else
    /* keep old behavior for non-open source (for now) */
    memcpy(&mgmt->u.probe_resp.timestamp, bss_desc->timeStamp,
            sizeof (bss_desc->timeStamp));

#endif

    mgmt->u.probe_resp.beacon_int = bss_desc->beaconInterval;
    mgmt->u.probe_resp.capab_info = bss_desc->capabilityInfo;

#ifdef WLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
    /* GPS Requirement: need age ie per entry. Using vendor specific. */
    /* Assuming this is the last IE, copy at the end */
    ie_length           -=sizeof(qcom_ie_age);
    qie_age =  (qcom_ie_age *)(mgmt->u.probe_resp.variable + ie_length);
    qie_age->element_id = QCOM_VENDOR_IE_ID;
    qie_age->len        = QCOM_VENDOR_IE_AGE_LEN;
    qie_age->oui_1      = QCOM_OUI1;
    qie_age->oui_2      = QCOM_OUI2;
    qie_age->oui_3      = QCOM_OUI3;
    qie_age->type       = QCOM_VENDOR_IE_AGE_TYPE;
    qie_age->age        = vos_timer_get_system_ticks() - bss_desc->nReceivedTime;
#endif

    memcpy(mgmt->u.probe_resp.variable, ie, ie_length);
    if (bss_desc->fProbeRsp)
    {
         mgmt->frame_control |=
                   (u16)(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP);
    }
    else
    {
         mgmt->frame_control |=
                   (u16)(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
    }

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,38))
    if (chan_no <= ARRAY_SIZE(hdd_channels_2_4_GHZ) &&
        (wiphy->bands[IEEE80211_BAND_2GHZ] != NULL))
    {
        freq = ieee80211_channel_to_frequency(chan_no, IEEE80211_BAND_2GHZ);
    }
    else if ((chan_no > ARRAY_SIZE(hdd_channels_2_4_GHZ)) &&
        (wiphy->bands[IEEE80211_BAND_5GHZ] != NULL))

    {
        freq = ieee80211_channel_to_frequency(chan_no, IEEE80211_BAND_5GHZ);
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s Invalid chan_no:%d",
                                                 __func__, chan_no);
        kfree(mgmt);
        return NULL;
    }
#else
    freq = ieee80211_channel_to_frequency(chan_no);
#endif
    chan = __ieee80211_get_channel(wiphy, freq);
    /*when the band is changed on the fly using the GUI, three things are done
     * 1. scan abort 2.flush scan results from cache 3.update the band with the new band user specified(refer to the hdd_setBand_helper function)
     * as part of the scan abort, message willbe queued to PE and we proceed with flushing and changinh the band.
     * pe will stop the scanning further and report back the results what ever it had till now by calling the call back function.
     * if the time between update band and scandone call back is sufficent enough the band change reflects in SME, SME validates the channels
     * and discards the channels correponding to previous band and calls back with zero bss results.
     * but if the time between band update and scan done callback is very small then band change will not reflect in SME and SME reports to HDD
     * all the channels correponding to previous band.this is due to race condition.but those channels are invalid to the new band and so
     * this function __ieee80211_get_channel will return NULL.Each time we report scan result with this pointer null warning kernel trace is printed.
     * if the scan results contain large number of APs continuosly kernel warning trace is printed and it will lead to apps watch dog bark.
     * So drop the bss and continue to next bss.
     */
    if(chan == NULL)
    {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s chan pointer is NULL", __func__);
       kfree(mgmt);
       return NULL;
    }
    /*To keep the rssi icon of the connected AP in the scan window
    *and the rssi icon of the wireless networks in sync
    * */
    if (( eConnectionState_Associated ==
             pAdapter->sessionCtx.station.conn_info.connState ) &&
             ( VOS_TRUE == vos_mem_compare(bss_desc->bssId,
                             pAdapter->sessionCtx.station.conn_info.bssId,
                             WNI_CFG_BSSID_LEN)))
    {
       /* supplicant takes the signal strength in terms of mBm(100*dBm) */
       rssi = (pAdapter->rssi * 100);
    }
    else
    {
       rssi = (VOS_MIN ((bss_desc->rssi + bss_desc->sinr), 0))*100;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: BSSID:" MAC_ADDRESS_STR " Channel:%d"
          "RSSI:%d", __func__, MAC_ADDR_ARRAY(mgmt->bssid),
                               chan->center_freq, (int)(rssi/100));

    bss_status = cfg80211_inform_bss_frame(wiphy, chan, mgmt,
            frame_len, rssi, GFP_KERNEL);
    kfree(mgmt);
    return bss_status;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_update_bss_db
 * This function is used to update the BSS data base of CFG8011
 */
struct cfg80211_bss* wlan_hdd_cfg80211_update_bss_db( hdd_adapter_t *pAdapter,
                                      tCsrRoamInfo *pRoamInfo
                                      )
{
    tCsrRoamConnectedProfile roamProfile;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    struct cfg80211_bss *bss = NULL;

    ENTER();

    memset(&roamProfile, 0, sizeof(tCsrRoamConnectedProfile));
    sme_RoamGetConnectProfile(hHal, pAdapter->sessionId, &roamProfile);

    if (NULL != roamProfile.pBssDesc)
    {
        bss = wlan_hdd_cfg80211_inform_bss_frame(pAdapter,
                roamProfile.pBssDesc);

        if (NULL == bss)
        {
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: cfg80211_inform_bss return NULL",
                    __func__);
        }

        sme_RoamFreeConnectProfile(hHal, &roamProfile);
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s:  roamProfile.pBssDesc is NULL",
                __func__);
    }
    return bss;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_update_bss
 */
static int wlan_hdd_cfg80211_update_bss( struct wiphy *wiphy,
                                         hdd_adapter_t *pAdapter
                                        )
{
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    tCsrScanResultInfo *pScanResult;
    eHalStatus status = 0;
    tScanResultHandle pResult;
    struct cfg80211_bss *bss_status = NULL;
    hdd_context_t *pHddCtx;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_UPDATE_BSS,
                       NO_SESSION, pAdapter->sessionId));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:LOGP in Progress. Ignore!!!",__func__);
        return -EAGAIN;
    }


    /*bss_update is not allowed during wlan driver loading or unloading*/
    if (WLAN_HDD_IS_LOAD_UNLOAD_IN_PROGRESS(pHddCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s:Loading_unloading in Progress. Ignore!!!",__func__);
        return VOS_STATUS_E_PERM;
    }


    /*
     * start getting scan results and populate cgf80211 BSS database
     */
    status = sme_ScanGetResult(hHal, pAdapter->sessionId, NULL, &pResult);

    /* no scan results */
    if (NULL == pResult)
    {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: No scan result Status %d",
                                                      __func__, status);
        return status;
    }

    pScanResult = sme_ScanResultGetFirst(hHal, pResult);

    while (pScanResult)
    {
        /*
         * cfg80211_inform_bss() is not updating ie field of bss entry, if
         * entry already exists in bss data base of cfg80211 for that
         * particular BSS ID.  Using cfg80211_inform_bss_frame to update the
         * bss entry instead of cfg80211_inform_bss, But this call expects
         * mgmt packet as input. As of now there is no possibility to get
         * the mgmt(probe response) frame from PE, converting bss_desc to
         * ieee80211_mgmt(probe response) and passing to c
         * fg80211_inform_bss_frame.
         * */

        bss_status = wlan_hdd_cfg80211_inform_bss_frame(pAdapter,
                &pScanResult->BssDescriptor);


        if (NULL == bss_status)
        {
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: NULL returned by cfg80211_inform_bss", __func__);
        }
        else
        {
            cfg80211_put_bss(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
                             wiphy,
#endif
                             bss_status);
        }

        pScanResult = sme_ScanResultGetNext(hHal, pResult);
    }

    sme_ScanResultPurge(hHal, pResult);

    return 0;
}

void
hddPrintMacAddr(tCsrBssid macAddr, tANI_U8 logLevel)
{
    VOS_TRACE(VOS_MODULE_ID_HDD, logLevel,
              MAC_ADDRESS_STR, MAC_ADDR_ARRAY(macAddr));
} /****** end hddPrintMacAddr() ******/

void
hddPrintPmkId(tANI_U8 *pmkId, tANI_U8 logLevel)
{
    VOS_TRACE(VOS_MODULE_ID_HDD, logLevel,
              "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
              pmkId[0], pmkId[1], pmkId[2], pmkId[3], pmkId[4],
              pmkId[5], pmkId[6], pmkId[7], pmkId[8], pmkId[9], pmkId[10],
              pmkId[11], pmkId[12], pmkId[13], pmkId[14], pmkId[15]);
} /****** end hddPrintPmkId() ******/

//hddPrintMacAddr(tCsrBssid macAddr, tANI_U8 logLevel);
//hddPrintMacAddr(macAddr, VOS_TRACE_LEVEL_FATAL);

//void sirDumpBuf(tpAniSirGlobal pMac, tANI_U8 modId, tANI_U32 level, tANI_U8 *buf, tANI_U32 size);
//sirDumpBuf(pMac, VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, pmkid, 16);

#define dump_bssid(bssid) \
    { \
        hddLog(VOS_TRACE_LEVEL_INFO, "BSSID (MAC) address:\t"); \
        hddPrintMacAddr(bssid, VOS_TRACE_LEVEL_INFO);\
    }

#define dump_pmkid(pMac, pmkid) \
    { \
        hddLog(VOS_TRACE_LEVEL_INFO, "PMKSA-ID:\t"); \
        hddPrintPmkId(pmkid, VOS_TRACE_LEVEL_INFO);\
    }

#if defined(FEATURE_WLAN_LFR) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
/*
 * FUNCTION: wlan_hdd_cfg80211_pmksa_candidate_notify
 * This function is used to notify the supplicant of a new PMKSA candidate.
 */
int wlan_hdd_cfg80211_pmksa_candidate_notify(
                    hdd_adapter_t *pAdapter, tCsrRoamInfo *pRoamInfo,
                    int index, bool preauth )
{
#ifdef FEATURE_WLAN_OKC
    struct net_device *dev = pAdapter->dev;
    hdd_context_t *pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;

    ENTER();
    hddLog(VOS_TRACE_LEVEL_INFO, "%s is going to notify supplicant of:", __func__);

    if( NULL == pRoamInfo )
    {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: pRoamInfo is NULL", __func__);
        return -EINVAL;
    }

    if (eANI_BOOLEAN_TRUE == hdd_is_okc_mode_enabled(pHddCtx))
    {
        dump_bssid(pRoamInfo->bssid);
        cfg80211_pmksa_candidate_notify(dev, index,
                                    pRoamInfo->bssid, preauth, GFP_KERNEL);
    }
#endif  /* FEATURE_WLAN_OKC */
    return 0;
}
#endif //FEATURE_WLAN_LFR

#ifdef FEATURE_WLAN_LFR_METRICS
/*
 * FUNCTION: wlan_hdd_cfg80211_roam_metrics_preauth
 * 802.11r/LFR metrics reporting function to report preauth initiation
 *
 */
#define MAX_LFR_METRICS_EVENT_LENGTH 100
VOS_STATUS wlan_hdd_cfg80211_roam_metrics_preauth(hdd_adapter_t *pAdapter,
                                                  tCsrRoamInfo *pRoamInfo)
{
    unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
    union iwreq_data wrqu;

    ENTER();

    if (NULL == pAdapter)
    {
        hddLog(LOGE, "%s: pAdapter is NULL!", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* create the event */
    memset(&wrqu, 0, sizeof(wrqu));
    memset(metrics_notification, 0, sizeof(metrics_notification));

    wrqu.data.pointer = metrics_notification;
    wrqu.data.length = scnprintf(metrics_notification,
        sizeof(metrics_notification), "QCOM: LFR_PREAUTH_INIT "
        MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pRoamInfo->bssid));

    wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, metrics_notification);

    EXIT();

    return VOS_STATUS_SUCCESS;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_roam_metrics_preauth_status
 * 802.11r/LFR metrics reporting function to report preauth completion
 * or failure
 */
VOS_STATUS wlan_hdd_cfg80211_roam_metrics_preauth_status(
    hdd_adapter_t *pAdapter, tCsrRoamInfo *pRoamInfo, bool preauth_status)
{
    unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
    union iwreq_data wrqu;

    ENTER();

    if (NULL == pAdapter)
    {
        hddLog(LOGE, "%s: pAdapter is NULL!", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* create the event */
    memset(&wrqu, 0, sizeof(wrqu));
    memset(metrics_notification, 0, sizeof(metrics_notification));

    scnprintf(metrics_notification, sizeof(metrics_notification),
        "QCOM: LFR_PREAUTH_STATUS "MAC_ADDRESS_STR,
        MAC_ADDR_ARRAY(pRoamInfo->bssid));

    if (1 == preauth_status)
        strncat(metrics_notification, " TRUE", 5);
    else
        strncat(metrics_notification, " FALSE", 6);

    wrqu.data.pointer = metrics_notification;
    wrqu.data.length = strlen(metrics_notification);

    wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, metrics_notification);

    EXIT();

    return VOS_STATUS_SUCCESS;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_roam_metrics_handover
 * 802.11r/LFR metrics reporting function to report handover initiation
 *
 */
VOS_STATUS wlan_hdd_cfg80211_roam_metrics_handover(hdd_adapter_t * pAdapter,
                                                   tCsrRoamInfo *pRoamInfo)
{
    unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
    union iwreq_data wrqu;

    ENTER();

    if (NULL == pAdapter)
    {
        hddLog(LOGE, "%s: pAdapter is NULL!", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* create the event */
    memset(&wrqu, 0, sizeof(wrqu));
    memset(metrics_notification, 0, sizeof(metrics_notification));

    wrqu.data.pointer = metrics_notification;
    wrqu.data.length = scnprintf(metrics_notification,
        sizeof(metrics_notification), "QCOM: LFR_PREAUTH_HANDOVER "
        MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pRoamInfo->bssid));

    wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, metrics_notification);

    EXIT();

    return VOS_STATUS_SUCCESS;
}
#endif

/*
 * FUNCTION: hdd_cfg80211_scan_done_callback
 * scanning callback function, called after finishing scan
 *
 */
static eHalStatus hdd_cfg80211_scan_done_callback(tHalHandle halHandle,
        void *pContext, tANI_U32 scanId, eCsrScanStatus status)
{
    struct net_device *dev = (struct net_device *) pContext;
    //struct wireless_dev *wdev = dev->ieee80211_ptr;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_scaninfo_t *pScanInfo = &pHddCtx->scan_info;
    struct cfg80211_scan_request *req = NULL;
    int ret = 0;
    bool aborted = false;
    long waitRet = 0;
    tANI_U8 i;

    ENTER();

    hddLog(VOS_TRACE_LEVEL_INFO,
            "%s called with halHandle = %p, pContext = %p,"
            "scanID = %d, returned status = %d",
            __func__, halHandle, pContext, (int) scanId, (int) status);

    pScanInfo->mScanPendingCounter = 0;

    //Block on scan req completion variable. Can't wait forever though.
    waitRet = wait_for_completion_interruptible_timeout(
                         &pScanInfo->scan_req_completion_event,
                         msecs_to_jiffies(WLAN_WAIT_TIME_SCAN_REQ));
    if (waitRet <= 0)
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s wait on scan_req_completion_event failed %ld",__func__, waitRet);
       VOS_ASSERT(pScanInfo->mScanPending);
       goto allow_suspend;
    }

    if (pScanInfo->mScanPending != VOS_TRUE)
    {
        VOS_ASSERT(pScanInfo->mScanPending);
        goto allow_suspend;
    }

    /* Check the scanId */
    if (pScanInfo->scanId != scanId)
    {
        hddLog(VOS_TRACE_LEVEL_INFO,
                "%s called with mismatched scanId pScanInfo->scanId = %d "
                "scanId = %d", __func__, (int) pScanInfo->scanId,
                (int) scanId);
    }

    ret = wlan_hdd_cfg80211_update_bss((WLAN_HDD_GET_CTX(pAdapter))->wiphy,
                                        pAdapter);

    if (0 > ret)
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: NO SCAN result", __func__);


    /* If any client wait scan result through WEXT
     * send scan done event to client */
    if (pHddCtx->scan_info.waitScanResult)
    {
        /* The other scan request waiting for current scan finish
         * Send event to notify current scan finished */
        if(WEXT_SCAN_PENDING_DELAY == pHddCtx->scan_info.scan_pending_option)
        {
            vos_event_set(&pHddCtx->scan_info.scan_finished_event);
        }
        /* Send notify to WEXT client */
        else if(WEXT_SCAN_PENDING_PIGGYBACK == pHddCtx->scan_info.scan_pending_option)
        {
            struct net_device *dev = pAdapter->dev;
            union iwreq_data wrqu;
            int we_event;
            char *msg;

            memset(&wrqu, '\0', sizeof(wrqu));
            we_event = SIOCGIWSCAN;
            msg = NULL;
            wireless_send_event(dev, we_event, &wrqu, msg);
        }
    }
    pHddCtx->scan_info.waitScanResult = FALSE;

    /* Get the Scan Req */
    req = pAdapter->request;

    if (!req)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "request is became NULL");
        pScanInfo->mScanPending = VOS_FALSE;
        goto allow_suspend;
    }

    pAdapter->request = NULL;
    /* Scan is no longer pending */
    pScanInfo->mScanPending = VOS_FALSE;

    /* last_scan_timestamp is used to decide if new scan
     * is needed or not on station interface. If last station
     *  scan time and new station scan time is less then
     * last_scan_timestamp ; driver will return cached scan.
     */
    if (req->no_cck == FALSE && status == eCSR_SCAN_SUCCESS) // no_cck will be set during p2p find
    {
        pScanInfo->last_scan_timestamp = vos_timer_get_system_time();

        if ( req->n_channels )
        {
            for (i = 0; i < req->n_channels ; i++ )
            {
                pHddCtx->scan_info.last_scan_channelList[i] = req->channels[i]->hw_value;
            }
            /* store no of channel scanned */
            pHddCtx->scan_info.last_scan_numChannels= req->n_channels;
        }

    }

    /*
     * cfg80211_scan_done informing NL80211 about completion
     * of scanning
     */
    if (status == eCSR_SCAN_ABORT || status == eCSR_SCAN_FAILURE)
    {
         aborted = true;
    }
    cfg80211_scan_done(req, aborted);
    complete(&pScanInfo->abortscan_event_var);

allow_suspend:
    /* release the wake lock at the end of the scan*/
    hdd_allow_suspend();

    /* Acquire wakelock to handle the case where APP's tries to suspend
     * immediatly after the driver gets connect request(i.e after scan)
     * from supplicant, this result in app's is suspending and not able
     * to process the connect request to AP */
    hdd_prevent_suspend_timeout(1000);

#ifdef FEATURE_WLAN_TDLS
    if (!(eTDLS_SUPPORT_NOT_ENABLED == pHddCtx->tdls_mode))
    {
        wlan_hdd_tdls_scan_done_callback(pAdapter);
    }
#endif

    EXIT();
    return 0;
}

/*
 * FUNCTION: hdd_isConnectionInProgress
 * Go through each adapter and check if Connection is in progress
 *
 */
v_BOOL_t hdd_isConnectionInProgress( hdd_context_t *pHddCtx, v_BOOL_t isRoC )
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    hdd_station_ctx_t *pHddStaCtx = NULL;
    hdd_adapter_t *pAdapter = NULL;
    VOS_STATUS status = 0;
    v_U8_t staId = 0;
    v_U8_t *staMac = NULL;

    if (TRUE == pHddCtx->btCoexModeSet)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
           FL("BTCoex Mode operation in progress"));
        return VOS_TRUE;
    }

    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;

        if( pAdapter )
        {
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: Adapter with device mode %s (%d) exists",
                    __func__, hdd_device_modetoString(pAdapter->device_mode),
                                                       pAdapter->device_mode);
            if ((((!isRoC) && (WLAN_HDD_INFRA_STATION == pAdapter->device_mode)) ||
                 (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) ||
                 (WLAN_HDD_P2P_DEVICE == pAdapter->device_mode)) &&
                 (eConnectionState_Connecting ==
                (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       "%s: %p(%d) Connection is in progress", __func__,
                       WLAN_HDD_GET_STATION_CTX_PTR(pAdapter), pAdapter->sessionId);
                return VOS_TRUE;
            }
            if (((!isRoC) && (WLAN_HDD_INFRA_STATION == pAdapter->device_mode)) ||
                     (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) ||
                     (WLAN_HDD_P2P_DEVICE == pAdapter->device_mode))
            {
                pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
                if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
                    (VOS_FALSE == pHddStaCtx->conn_info.uIsAuthenticated))
                {
                    staMac = (v_U8_t *) &(pAdapter->macAddressCurrent.bytes[0]);
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                           "%s: client " MAC_ADDRESS_STR
                           " is in the middle of WPS/EAPOL exchange.", __func__,
                            MAC_ADDR_ARRAY(staMac));
                    return VOS_TRUE;
                }
            }
            else if ((WLAN_HDD_SOFTAP == pAdapter->device_mode) ||
                    (WLAN_HDD_P2P_GO == pAdapter->device_mode))
            {
                for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++)
                {
                    if ((pAdapter->aStaInfo[staId].isUsed) &&
                            (WLANTL_STA_CONNECTED == pAdapter->aStaInfo[staId].tlSTAState))
                    {
                        staMac = (v_U8_t *) &(pAdapter->aStaInfo[staId].macAddrSTA.bytes[0]);

                        hddLog(VOS_TRACE_LEVEL_ERROR,
                               "%s: client " MAC_ADDRESS_STR " of SoftAP/P2P-GO is in the "
                               "middle of WPS/EAPOL exchange.", __func__,
                                MAC_ADDR_ARRAY(staMac));
                        return VOS_TRUE;
                    }
                }
            }
        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }
    return VOS_FALSE;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_scan
 * this scan respond to scan trigger and update cfg80211 scan database
 * later, scan dump command can be used to recieve scan results
 */
int __wlan_hdd_cfg80211_scan( struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                            struct net_device *dev,
#endif
                            struct cfg80211_scan_request *request)
{
    hdd_adapter_t *pAdapter = NULL;
    hdd_context_t *pHddCtx = NULL;
    hdd_wext_state_t *pwextBuf = NULL;
    hdd_config_t *cfg_param = NULL;
    tCsrScanRequest scanRequest;
    tANI_U8 *channelList = NULL, i;
    v_U32_t scanId = 0;
    int status;
    hdd_scaninfo_t *pScanInfo = NULL;
    v_U8_t* pP2pIe = NULL;
    int ret = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
    struct net_device *dev = NULL;
    if (NULL == request)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: scan req param null", __func__);
        return -EINVAL;
    }
    dev = request->wdev->netdev;
#endif

    pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
    pwextBuf = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

    ENTER();


    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s (%d)",
           __func__, hdd_device_modetoString(pAdapter->device_mode),
                                             pAdapter->device_mode);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if (NULL == pwextBuf)
    {
        hddLog (VOS_TRACE_LEVEL_ERROR, "%s ERROR: invalid WEXT state\n",
                __func__);
        return -EIO;
    }
    cfg_param = pHddCtx->cfg_ini;
    pScanInfo = &pHddCtx->scan_info;

#ifdef WLAN_BTAMP_FEATURE
    //Scan not supported when AMP traffic is on.
    if (VOS_TRUE == WLANBAP_AmpSessionOn())
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: No scanning when AMP is on", __func__);
        return -EOPNOTSUPP;
    }
#endif
    //Scan on any other interface is not supported.
    if (pAdapter->device_mode == WLAN_HDD_SOFTAP)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: Not scanning on device_mode = %s (%d)",
              __func__, hdd_device_modetoString(pAdapter->device_mode),
                                                pAdapter->device_mode);
        return -EOPNOTSUPP;
    }

    if (TRUE == pScanInfo->mScanPending)
    {
        if ( MAX_PENDING_LOG > pScanInfo->mScanPendingCounter++ )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: mScanPending is TRUE", __func__);
        }
        return -EBUSY;
    }

    //Don't Allow Scan and return busy if Remain On
    //Channel and action frame is pending
    //Otherwise Cancel Remain On Channel and allow Scan
    //If no action frame pending
    if (0 != wlan_hdd_check_remain_on_channel(pAdapter))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Remain On Channel Pending", __func__);
        return -EBUSY;
    }
#ifdef FEATURE_WLAN_TDLS
    /* if tdls disagree scan right now, return immediately.
       tdls will schedule the scan when scan is allowed. (return SUCCESS)
       or will reject the scan if any TDLS is in progress. (return -EBUSY)
    */
    status = wlan_hdd_tdls_scan_callback (pAdapter,
                                          wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                          dev,
#endif
                                          request);
    if(status <= 0)
    {
        if(!status)
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: TDLS in progress."
                  "scan rejected  %d", __func__, status);
        else
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: TDLS teardown is ongoing %d",
                                          __func__, status);

        return status;
    }
#endif

    if (mutex_lock_interruptible(&pHddCtx->tmInfo.tmOperationLock))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                  "%s: Acquire lock fail", __func__);
        return -EAGAIN;
    }
    if (TRUE == pHddCtx->tmInfo.tmAction.enterImps)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: MAX TM Level Scan not allowed", __func__);
        mutex_unlock(&pHddCtx->tmInfo.tmOperationLock);
        return -EBUSY;
    }
    mutex_unlock(&pHddCtx->tmInfo.tmOperationLock);

    /* Check if scan is allowed at this point of time.
     */
    if (hdd_isConnectionInProgress(pHddCtx, VOS_FALSE))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Scan not allowed", __func__);
        return -EBUSY;
    }

    vos_mem_zero( &scanRequest, sizeof(scanRequest));

    hddLog(VOS_TRACE_LEVEL_INFO, "scan request for ssid = %d",
           (int)request->n_ssids);


    /* Even though supplicant doesn't provide any SSIDs, n_ssids is set to 1.
     * Becasue of this, driver is assuming that this is not wildcard scan and so
     * is not aging out the scan results.
     */
    if (request->ssids && '\0' == request->ssids->ssid[0])
    {
        request->n_ssids = 0;
    }

    if ((request->ssids) && (0 < request->n_ssids))
    {
        tCsrSSIDInfo *SsidInfo;
        int j;
        scanRequest.SSIDs.numOfSSIDs = request->n_ssids;
        /* Allocate num_ssid tCsrSSIDInfo structure */
        SsidInfo = scanRequest.SSIDs.SSIDList =
                  ( tCsrSSIDInfo *)vos_mem_malloc(
                          request->n_ssids*sizeof(tCsrSSIDInfo));

        if(NULL == scanRequest.SSIDs.SSIDList)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                      "%s: memory alloc failed SSIDInfo buffer", __func__);
            return -ENOMEM;
        }

        /* copy all the ssid's and their length */
        for(j = 0; j < request->n_ssids; j++, SsidInfo++)
        {
            /* get the ssid length */
            SsidInfo->SSID.length = request->ssids[j].ssid_len;
            vos_mem_copy(SsidInfo->SSID.ssId, &request->ssids[j].ssid[0],
                         SsidInfo->SSID.length);
            SsidInfo->SSID.ssId[SsidInfo->SSID.length] = '\0';
            hddLog(VOS_TRACE_LEVEL_INFO, "SSID number %d:  %s",
                                                   j, SsidInfo->SSID.ssId);
        }
        /* set the scan type to active */
        scanRequest.scanType = eSIR_ACTIVE_SCAN;
    }
    else if(WLAN_HDD_P2P_GO == pAdapter->device_mode)
    {
        MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                    TRACE_CODE_HDD_CFG80211_SCAN,
                    pAdapter->sessionId, 0));
        /* set the scan type to active */
        scanRequest.scanType = eSIR_ACTIVE_SCAN;
    }
    else
    {
        /*Set the scan type to default type, in this case it is ACTIVE*/
        scanRequest.scanType = pScanInfo->scan_mode;
    }
    scanRequest.minChnTime = cfg_param->nActiveMinChnTime;
    scanRequest.maxChnTime = cfg_param->nActiveMaxChnTime;

    /* set BSSType to default type */
    scanRequest.BSSType = eCSR_BSS_TYPE_ANY;

    /*TODO: scan the requested channels only*/

    /*Right now scanning all the channels */
    if (MAX_CHANNEL < request->n_channels)
    {
        hddLog(VOS_TRACE_LEVEL_WARN,
           "No of Scan Channels exceeded limit: %d", request->n_channels);
        request->n_channels = MAX_CHANNEL;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
                           "No of Scan Channels: %d", request->n_channels);


    if( request->n_channels )
    {
        char chList [(request->n_channels*5)+1];
        int len;
        channelList = vos_mem_malloc( request->n_channels );
        if( NULL == channelList )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                           "%s: memory alloc failed channelList", __func__);
            status = -ENOMEM;
            goto free_mem;
        }

        for( i = 0, len = 0; i < request->n_channels ; i++ )
        {
            channelList[i] = request->channels[i]->hw_value;
            len += snprintf(chList+len, 5, "%d ", channelList[i]);
        }

        hddLog(VOS_TRACE_LEVEL_INFO,
                           "Channel-List:  %s ", chList);
    }

    scanRequest.ChannelInfo.numOfChannels = request->n_channels;
    scanRequest.ChannelInfo.ChannelList = channelList;

    /* set requestType to full scan */
    scanRequest.requestType = eCSR_SCAN_REQUEST_FULL_SCAN;

    /* if there is back to back scan happening in driver with in
     * nDeferScanTimeInterval interval driver should defer new scan request
     * and should provide last cached scan results instead of new channel list.
     * This rule is not applicable if scan is p2p scan.
     * This condition will work only in case when last request no of channels
     * and channels are exactly same as new request.
     * This should be done only in connected state
     */

    if ((VOS_STATUS_SUCCESS == hdd_is_any_session_connected(pHddCtx)))
    {
        if ( pScanInfo->last_scan_timestamp !=0 &&
             ((vos_timer_get_system_time() - pScanInfo->last_scan_timestamp ) < pHddCtx->cfg_ini->nDeferScanTimeInterval))
        {
            if ( request->no_cck == FALSE && scanRequest.ChannelInfo.numOfChannels != 1 &&
               (pScanInfo->last_scan_numChannels == scanRequest.ChannelInfo.numOfChannels) &&
                vos_mem_compare(pScanInfo->last_scan_channelList,
                           channelList, pScanInfo->last_scan_numChannels))
            {
                hddLog(VOS_TRACE_LEVEL_WARN,
                     " New and old station scan time differ is less then %u",
                pHddCtx->cfg_ini->nDeferScanTimeInterval);

                ret = wlan_hdd_cfg80211_update_bss((WLAN_HDD_GET_CTX(pAdapter))->wiphy,
                                        pAdapter);

                hddLog(VOS_TRACE_LEVEL_WARN,
                    "Return old cached scan as all channels"
                    "and no of channles are same");
                if (0 > ret)
                    hddLog(VOS_TRACE_LEVEL_INFO, "%s: NO SCAN result", __func__);

                cfg80211_scan_done(request, eCSR_SCAN_SUCCESS);
                return eHAL_STATUS_SUCCESS ;
            }
        }
    }

    /* Flush the scan results(only p2p beacons) for STA scan and P2P
     * search (Flush on both full  scan and social scan but not on single
     * channel scan).P2P  search happens on 3 social channels (1, 6, 11)
     */

    /* Supplicant does single channel scan after 8-way handshake
     * and in that case driver shoudnt flush scan results. If
     * driver flushes the scan results here and unfortunately if
     * the AP doesnt respond to our probe req then association
     * fails which is not desired
     */

    if( request->n_channels != WLAN_HDD_P2P_SINGLE_CHANNEL_SCAN )
    {
        hddLog(VOS_TRACE_LEVEL_DEBUG, "Flushing P2P Results");
        sme_ScanFlushP2PResult( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                            pAdapter->sessionId );
    }

    if( request->ie_len )
    {
        /* save this for future association (join requires this) */
        /*TODO: Array needs to be converted to dynamic allocation,
         * as multiple ie.s can be sent in cfg80211_scan_request structure
         * CR 597966
         */
        memset( &pScanInfo->scanAddIE, 0, sizeof(pScanInfo->scanAddIE) );
        memcpy( pScanInfo->scanAddIE.addIEdata, request->ie, request->ie_len);
        pScanInfo->scanAddIE.length = request->ie_len;

        if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
            (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) ||
            (WLAN_HDD_P2P_DEVICE == pAdapter->device_mode))
        {
            if ( request->ie_len <= SIR_MAC_MAX_IE_LENGTH)
            {
                pwextBuf->roamProfile.nAddIEScanLength = request->ie_len;
                memcpy( pwextBuf->roamProfile.addIEScan,
                                 request->ie, request->ie_len);
            }
            else
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "Scan Ie length is invalid:"
                         "%zu", request->ie_len);
            }

        }
        scanRequest.uIEFieldLen = pScanInfo->scanAddIE.length;
        scanRequest.pIEField = pScanInfo->scanAddIE.addIEdata;

        pP2pIe = wlan_hdd_get_p2p_ie_ptr((v_U8_t*)request->ie,
                                                   request->ie_len);
        if (pP2pIe != NULL)
        {
#ifdef WLAN_FEATURE_P2P_DEBUG
            if (((globalP2PConnectionStatus == P2P_GO_NEG_COMPLETED) ||
                (globalP2PConnectionStatus == P2P_GO_NEG_PROCESS)) &&
                (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
            {
                globalP2PConnectionStatus = P2P_CLIENT_CONNECTING_STATE_1;
                hddLog(VOS_TRACE_LEVEL_ERROR,"[P2P State] Changing state from "
                                "Go nego completed to Connection is started");
                hddLog(VOS_TRACE_LEVEL_ERROR,"[P2P]P2P Scanning is started "
                               "for 8way Handshake");
            }
            else if((globalP2PConnectionStatus == P2P_CLIENT_DISCONNECTED_STATE) &&
                    (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
            {
                globalP2PConnectionStatus = P2P_CLIENT_CONNECTING_STATE_2;
                hddLog(VOS_TRACE_LEVEL_ERROR,"[P2P State] Changing state from "
                                "Disconnected state to Connection is started");
                hddLog(VOS_TRACE_LEVEL_ERROR,"[P2P]P2P Scanning is started "
                                                    "for 4way Handshake");
            }
#endif

            /* no_cck will be set during p2p find to disable 11b rates */
            if(TRUE == request->no_cck)
            {
                hddLog(VOS_TRACE_LEVEL_INFO,
                       "%s: This is a P2P Search", __func__);
                scanRequest.p2pSearch = 1;

                if( request->n_channels == WLAN_HDD_P2P_SOCIAL_CHANNELS )
                {
                     /* set requestType to P2P Discovery */
                     scanRequest.requestType = eCSR_SCAN_P2P_DISCOVERY;
                }

                /*
                   Skip Dfs Channel in case of P2P Search
                   if it is set in ini file
                */
                if(cfg_param->skipDfsChnlInP2pSearch)
                {
                   scanRequest.skipDfsChnlInP2pSearch = 1;
                }
                else
                {
                   scanRequest.skipDfsChnlInP2pSearch = 0;
                }

            }
        }
    }

    INIT_COMPLETION(pScanInfo->scan_req_completion_event);

    /* acquire the wakelock to avoid the apps suspend during the scan. To
     * address the following issues.
     * 1) Disconnected scenario: we are not allowing the suspend as WLAN is not in
     * BMPS/IMPS this result in android trying to suspend aggressively and backing off
     * for long time, this result in apps running at full power for long time.
     * 2) Connected scenario: If we allow the suspend during the scan, RIVA will
     * be stuck in full power because of resume BMPS
     */
    hdd_prevent_suspend();

    hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
           "requestType %d, scanType %d, minChnTime %d, maxChnTime %d,"
           "p2pSearch %d, skipDfsChnlInP2pSearch %d",
           scanRequest.requestType, scanRequest.scanType,
           scanRequest.minChnTime, scanRequest.maxChnTime,
           scanRequest.p2pSearch, scanRequest.skipDfsChnlInP2pSearch);

    status = sme_ScanRequest( WLAN_HDD_GET_HAL_CTX(pAdapter),
                              pAdapter->sessionId, &scanRequest, &scanId,
                              &hdd_cfg80211_scan_done_callback, dev );

    if (eHAL_STATUS_SUCCESS != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: sme_ScanRequest returned error %d", __func__, status);
        complete(&pScanInfo->scan_req_completion_event);
        if(eHAL_STATUS_RESOURCES == status)
        {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: HO is in progress."
                                 "So defer the scan by informing busy",__func__);
                status = -EBUSY;
        } else {
                status = -EIO;
        }
        hdd_allow_suspend();
        goto free_mem;
    }

    pScanInfo->mScanPending = TRUE;
    pScanInfo->sessionId = pAdapter->sessionId;
    pAdapter->request = request;
    pScanInfo->scanId = scanId;

    complete(&pScanInfo->scan_req_completion_event);

free_mem:
    if( scanRequest.SSIDs.SSIDList )
    {
        vos_mem_free(scanRequest.SSIDs.SSIDList);
    }

    if( channelList )
      vos_mem_free( channelList );

    EXIT();

    return status;
}

int wlan_hdd_cfg80211_scan( struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                            struct net_device *dev,
#endif
                            struct cfg80211_scan_request *request)
{
    int ret;

    vos_ssr_protect(__func__);
    ret =  __wlan_hdd_cfg80211_scan(wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                   dev,
#endif
                                   request);
    vos_ssr_unprotect(__func__);

    return ret;
}

void hdd_select_cbmode( hdd_adapter_t *pAdapter,v_U8_t operationChannel)
{
    v_U8_t iniDot11Mode =
               (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->dot11Mode;
    eHddDot11Mode   hddDot11Mode = iniDot11Mode;

    hddLog(LOG1, FL("Channel Bonding Mode Selected is %u"),
           iniDot11Mode);
    switch ( iniDot11Mode )
    {
       case eHDD_DOT11_MODE_AUTO:
       case eHDD_DOT11_MODE_11ac:
       case eHDD_DOT11_MODE_11ac_ONLY:
#ifdef WLAN_FEATURE_11AC
          if ( sme_IsFeatureSupportedByDriver(DOT11AC) &&
               sme_IsFeatureSupportedByFW(DOT11AC) )
              hddDot11Mode = eHDD_DOT11_MODE_11ac;
          else
              hddDot11Mode = eHDD_DOT11_MODE_11n;
#else
          hddDot11Mode = eHDD_DOT11_MODE_11n;
#endif
          break;
       case eHDD_DOT11_MODE_11n:
       case eHDD_DOT11_MODE_11n_ONLY:
          hddDot11Mode = eHDD_DOT11_MODE_11n;
          break;
       default:
          hddDot11Mode = iniDot11Mode;
          break;
    }
     /* This call decides required channel bonding mode */
    sme_SelectCBMode((WLAN_HDD_GET_CTX(pAdapter)->hHal),
                     hdd_cfg_xlate_to_csr_phy_mode(hddDot11Mode),
                     operationChannel);
}

/*
 * FUNCTION: wlan_hdd_cfg80211_connect_start
 * This function is used to start the association process
 */
int wlan_hdd_cfg80211_connect_start( hdd_adapter_t  *pAdapter,
        const u8 *ssid, size_t ssid_len, const u8 *bssid, u8 operatingChannel)
{
    int status = 0;
    hdd_wext_state_t *pWextState;
    hdd_context_t *pHddCtx;
    v_U32_t roamId;
    tCsrRoamProfile *pRoamProfile;
    eCsrAuthType RSNAuthType;

    ENTER();

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    status = wlan_hdd_validate_context(pHddCtx);
    if (status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is not valid!", __func__);
        return status;
    }

    if (SIR_MAC_MAX_SSID_LENGTH < ssid_len)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: wrong SSID len", __func__);
        return -EINVAL;
    }

    pRoamProfile = &pWextState->roamProfile;

    if (pRoamProfile)
    {
        hdd_station_ctx_t *pHddStaCtx;
        pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

        if (HDD_WMM_USER_MODE_NO_QOS ==
                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->WmmMode)
        {
            /*QoS not enabled in cfg file*/
            pRoamProfile->uapsd_mask = 0;
        }
        else
        {
            /*QoS enabled, update uapsd mask from cfg file*/
            pRoamProfile->uapsd_mask =
                     (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->UapsdMask;
        }

        pRoamProfile->SSIDs.numOfSSIDs = 1;
        pRoamProfile->SSIDs.SSIDList->SSID.length = ssid_len;
        vos_mem_zero(pRoamProfile->SSIDs.SSIDList->SSID.ssId,
                sizeof(pRoamProfile->SSIDs.SSIDList->SSID.ssId));
        vos_mem_copy((void *)(pRoamProfile->SSIDs.SSIDList->SSID.ssId),
                ssid, ssid_len);

        if (bssid)
        {
            pRoamProfile->BSSIDs.numOfBSSIDs = 1;
            vos_mem_copy((void *)(pRoamProfile->BSSIDs.bssid), bssid,
                    WNI_CFG_BSSID_LEN);
            /* Save BSSID in seperate variable as well, as RoamProfile
               BSSID is getting zeroed out in the association process. And in
               case of join failure we should send valid BSSID to supplicant
             */
            vos_mem_copy((void *)(pWextState->req_bssId), bssid,
                    WNI_CFG_BSSID_LEN);
        }
        else
        {
            vos_mem_zero((void *)(pRoamProfile->BSSIDs.bssid),WNI_CFG_BSSID_LEN);
        }

        hddLog(LOG1, FL("Connect to SSID: %s opertating Channel: %u"),
               pRoamProfile->SSIDs.SSIDList->SSID.ssId, operatingChannel);
        if ((IW_AUTH_WPA_VERSION_WPA == pWextState->wpaVersion) ||
                (IW_AUTH_WPA_VERSION_WPA2 == pWextState->wpaVersion))
        {
            /*set gen ie*/
            hdd_SetGENIEToCsr(pAdapter, &RSNAuthType);
            /*set auth*/
            hdd_set_csr_auth_type(pAdapter, RSNAuthType);
        }
#ifdef FEATURE_WLAN_WAPI
        if (pAdapter->wapi_info.nWapiMode)
        {
            hddLog(LOG1, "%s: Setting WAPI AUTH Type and Encryption Mode values", __func__);
            switch (pAdapter->wapi_info.wapiAuthMode)
            {
                case WAPI_AUTH_MODE_PSK:
                {
                    hddLog(LOG1, "%s: WAPI AUTH TYPE: PSK: %d", __func__,
                                                   pAdapter->wapi_info.wapiAuthMode);
                    pRoamProfile->AuthType.authType[0] = eCSR_AUTH_TYPE_WAPI_WAI_PSK;
                    break;
                }
                case WAPI_AUTH_MODE_CERT:
                {
                    hddLog(LOG1, "%s: WAPI AUTH TYPE: CERT: %d", __func__,
                                                    pAdapter->wapi_info.wapiAuthMode);
                    pRoamProfile->AuthType.authType[0] = eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE;
                    break;
                }
            } // End of switch
            if ( pAdapter->wapi_info.wapiAuthMode == WAPI_AUTH_MODE_PSK ||
                pAdapter->wapi_info.wapiAuthMode == WAPI_AUTH_MODE_CERT)
            {
                hddLog(LOG1, "%s: WAPI PAIRWISE/GROUP ENCRYPTION: WPI", __func__);
                pRoamProfile->AuthType.numEntries = 1;
                pRoamProfile->EncryptionType.numEntries = 1;
                pRoamProfile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_WPI;
                pRoamProfile->mcEncryptionType.numEntries = 1;
                pRoamProfile->mcEncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_WPI;
            }
        }
#endif /* FEATURE_WLAN_WAPI */
#ifdef WLAN_FEATURE_GTK_OFFLOAD
        /* Initializing gtkOffloadReqParams */
        if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
            (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
        {
            memset(&pHddStaCtx->gtkOffloadReqParams, 0,
                  sizeof (tSirGtkOffloadParams));
            pHddStaCtx->gtkOffloadReqParams.ulFlags = GTK_OFFLOAD_DISABLE;
        }
#endif
        pRoamProfile->csrPersona = pAdapter->device_mode;

        if( operatingChannel )
        {
           pRoamProfile->ChannelInfo.ChannelList = &operatingChannel;
           pRoamProfile->ChannelInfo.numOfChannels = 1;
        }
        else
        {
            pRoamProfile->ChannelInfo.ChannelList = NULL;
            pRoamProfile->ChannelInfo.numOfChannels = 0;
        }
        if ( (WLAN_HDD_IBSS == pAdapter->device_mode) && operatingChannel)
        {
            hdd_select_cbmode(pAdapter,operatingChannel);
        }

        /* change conn_state to connecting before sme_RoamConnect(), because sme_RoamConnect()
         * has a direct path to call hdd_smeRoamCallback(), which will change the conn_state
         * If direct path, conn_state will be accordingly changed to NotConnected or Associated
         * by either hdd_AssociationCompletionHandler() or hdd_DisConnectHandler() in sme_RoamCallback()
         * if sme_RomConnect is to be queued, Connecting state will remain until it is completed.
         */
        if (WLAN_HDD_INFRA_STATION == pAdapter->device_mode ||
            WLAN_HDD_P2P_CLIENT == pAdapter->device_mode)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: Set HDD connState to eConnectionState_Connecting",
                   __func__);
            hdd_connSetConnectionState(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter),
                                                 eConnectionState_Connecting);
        }
        status = sme_RoamConnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                            pAdapter->sessionId, pRoamProfile, &roamId);

        if ((eHAL_STATUS_SUCCESS != status) &&
            (WLAN_HDD_INFRA_STATION == pAdapter->device_mode ||
             WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))

        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: sme_RoamConnect (session %d) failed with "
                                      "status %d. -> NotConnected", __func__, pAdapter->sessionId, status);
            /* change back to NotAssociated */
            hdd_connSetConnectionState(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter),
                                             eConnectionState_NotConnected);
        }

        pRoamProfile->ChannelInfo.ChannelList = NULL;
        pRoamProfile->ChannelInfo.numOfChannels = 0;

    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: No valid Roam profile", __func__);
        return -EINVAL;
    }
    EXIT();
    return status;
}

/*
 * FUNCTION: wlan_hdd_set_cfg80211_auth_type
 * This function is used to set the authentication type (OPEN/SHARED).
 *
 */
static int wlan_hdd_cfg80211_set_auth_type(hdd_adapter_t *pAdapter,
        enum nl80211_auth_type auth_type)
{
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    ENTER();

    /*set authentication type*/
    switch (auth_type)
    {
        case NL80211_AUTHTYPE_AUTOMATIC:
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: set authentication type to AUTOSWITCH", __func__);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_AUTOSWITCH;
            break;

        case NL80211_AUTHTYPE_OPEN_SYSTEM:
#ifdef WLAN_FEATURE_VOWIFI_11R
        case NL80211_AUTHTYPE_FT:
#endif /* WLAN_FEATURE_VOWIFI_11R */
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: set authentication type to OPEN", __func__);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_OPEN_SYSTEM;
            break;

        case NL80211_AUTHTYPE_SHARED_KEY:
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: set authentication type to SHARED", __func__);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_SHARED_KEY;
            break;
#ifdef FEATURE_WLAN_ESE
        case NL80211_AUTHTYPE_NETWORK_EAP:
            hddLog(VOS_TRACE_LEVEL_INFO,
                            "%s: set authentication type to CCKM WPA", __func__);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_CCKM_WPA;//eCSR_AUTH_TYPE_CCKM_RSN needs to be handled as well if required.
            break;
#endif


        default:
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Unsupported authentication type %d", __func__,
                    auth_type);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_UNKNOWN;
            return -EINVAL;
    }

    pWextState->roamProfile.AuthType.authType[0] =
                                        pHddStaCtx->conn_info.authType;
    return 0;
}

/*
 * FUNCTION: wlan_hdd_set_akm_suite
 * This function is used to set the key mgmt type(PSK/8021x).
 *
 */
static int wlan_hdd_set_akm_suite( hdd_adapter_t *pAdapter,
                                   u32 key_mgmt
                                   )
{
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    ENTER();
    /* Should be in ieee802_11_defs.h */
#define WLAN_AKM_SUITE_8021X_SHA256 0x000FAC05
#define WLAN_AKM_SUITE_PSK_SHA256   0x000FAC06
    /*set key mgmt type*/
    switch(key_mgmt)
    {
        case WLAN_AKM_SUITE_PSK:
        case WLAN_AKM_SUITE_PSK_SHA256:
#ifdef WLAN_FEATURE_VOWIFI_11R
        case WLAN_AKM_SUITE_FT_PSK:
#endif
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting key mgmt type to PSK",
                    __func__);
            pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_PSK;
            break;

        case WLAN_AKM_SUITE_8021X:
        case WLAN_AKM_SUITE_8021X_SHA256:
#ifdef WLAN_FEATURE_VOWIFI_11R
        case WLAN_AKM_SUITE_FT_8021X:
#endif
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting key mgmt type to 8021x",
                    __func__);
            pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_802_1X;
            break;
#ifdef FEATURE_WLAN_ESE
#define WLAN_AKM_SUITE_CCKM         0x00409600 /* Should be in ieee802_11_defs.h */
#define IW_AUTH_KEY_MGMT_CCKM       8  /* Should be in linux/wireless.h */
        case WLAN_AKM_SUITE_CCKM:
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting key mgmt type to CCKM",
                            __func__);
            pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_CCKM;
            break;
#endif
#ifndef WLAN_AKM_SUITE_OSEN
#define WLAN_AKM_SUITE_OSEN         0x506f9a01 /* Should be in ieee802_11_defs.h */
        case WLAN_AKM_SUITE_OSEN:
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting key mgmt type to OSEN",
                            __func__);
            pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_802_1X;
            break;
#endif

        default:
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Unsupported key mgmt type %d",
                    __func__, key_mgmt);
            return -EINVAL;

    }
    return 0;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_set_cipher
 * This function is used to set the encryption type
 * (NONE/WEP40/WEP104/TKIP/CCMP).
 */
static int wlan_hdd_cfg80211_set_cipher( hdd_adapter_t *pAdapter,
                                u32 cipher,
                                bool ucast
                                )
{
    eCsrEncryptionType encryptionType = eCSR_ENCRYPT_TYPE_NONE;
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    ENTER();

    if (!cipher)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: received cipher %d - considering none",
                __func__, cipher);
        encryptionType = eCSR_ENCRYPT_TYPE_NONE;
    }
    else
    {

        /*set encryption method*/
        switch (cipher)
        {
            case IW_AUTH_CIPHER_NONE:
                encryptionType = eCSR_ENCRYPT_TYPE_NONE;
                break;

            case WLAN_CIPHER_SUITE_WEP40:
                encryptionType = eCSR_ENCRYPT_TYPE_WEP40;
                break;

            case WLAN_CIPHER_SUITE_WEP104:
                encryptionType = eCSR_ENCRYPT_TYPE_WEP104;
                break;

            case WLAN_CIPHER_SUITE_TKIP:
                encryptionType = eCSR_ENCRYPT_TYPE_TKIP;
                break;

            case WLAN_CIPHER_SUITE_CCMP:
                encryptionType = eCSR_ENCRYPT_TYPE_AES;
                break;
#ifdef FEATURE_WLAN_WAPI
        case WLAN_CIPHER_SUITE_SMS4:
            encryptionType = eCSR_ENCRYPT_TYPE_WPI;
            break;
#endif

#ifdef FEATURE_WLAN_ESE
        case WLAN_CIPHER_SUITE_KRK:
            encryptionType = eCSR_ENCRYPT_TYPE_KRK;
            break;
#endif
            default:
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Unsupported cipher type %d",
                        __func__, cipher);
                return -EOPNOTSUPP;
        }
    }

    if (ucast)
    {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting unicast cipher type to %d",
                __func__, encryptionType);
        pHddStaCtx->conn_info.ucEncryptionType            = encryptionType;
        pWextState->roamProfile.EncryptionType.numEntries = 1;
        pWextState->roamProfile.EncryptionType.encryptionType[0] =
                                          encryptionType;
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting mcast cipher type to %d",
                __func__, encryptionType);
        pHddStaCtx->conn_info.mcEncryptionType                       = encryptionType;
        pWextState->roamProfile.mcEncryptionType.numEntries        = 1;
        pWextState->roamProfile.mcEncryptionType.encryptionType[0] = encryptionType;
    }

    return 0;
}


/*
 * FUNCTION: wlan_hdd_cfg80211_set_ie
 * This function is used to parse WPA/RSN IE's.
 */
int wlan_hdd_cfg80211_set_ie( hdd_adapter_t *pAdapter,
                              u8 *ie,
                              size_t ie_len
                              )
{
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    u8 *genie = ie;
    v_U16_t remLen = ie_len;
#ifdef FEATURE_WLAN_WAPI
    v_U32_t akmsuite[MAX_NUM_AKM_SUITES];
    u16 *tmp;
    v_U16_t akmsuiteCount;
    int *akmlist;
#endif
    ENTER();

    /* clear previous assocAddIE */
    pWextState->assocAddIE.length = 0;
    pWextState->roamProfile.bWPSAssociation = VOS_FALSE;
    pWextState->roamProfile.bOSENAssociation = VOS_FALSE;

    while (remLen >= 2)
    {
        v_U16_t eLen = 0;
        v_U8_t elementId;
        elementId = *genie++;
        eLen  = *genie++;
        remLen -= 2;

        hddLog(VOS_TRACE_LEVEL_INFO, "%s: IE[0x%X], LEN[%d]",
            __func__, elementId, eLen);

        switch ( elementId )
        {
            case DOT11F_EID_WPA:
                if (4 > eLen) /* should have at least OUI which is 4 bytes so extra 2 bytes not needed */
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                              "%s: Invalid WPA IE", __func__);
                    return -EINVAL;
                }
                else if (0 == memcmp(&genie[0], "\x00\x50\xf2\x04", 4))
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set WPS IE(len %d)",
                            __func__, eLen + 2);

                    if( SIR_MAC_MAX_IE_LENGTH < (pWextState->assocAddIE.length + eLen) )
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE. "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       return -ENOMEM;
                    }
                    // WSC IE is saved to Additional IE ; it should be accumulated to handle WPS IE + P2P IE
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.bWPSAssociation = VOS_TRUE;
                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
                else if (0 == memcmp(&genie[0], "\x00\x50\xf2", 3))
                {
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set WPA IE (len %d)",__func__, eLen + 2);
                    memset( pWextState->WPARSNIE, 0, MAX_WPA_RSN_IE_LEN );
                    memcpy( pWextState->WPARSNIE, genie - 2, (eLen + 2) /*ie_len*/);
                    pWextState->roamProfile.pWPAReqIE = pWextState->WPARSNIE;
                    pWextState->roamProfile.nWPAReqIELength = eLen + 2;//ie_len;
                }
                else if ( (0 == memcmp(&genie[0], P2P_OUI_TYPE,
                                                         P2P_OUI_TYPE_SIZE)))
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set P2P IE(len %d)",
                            __func__, eLen + 2);

                    if( SIR_MAC_MAX_IE_LENGTH < (pWextState->assocAddIE.length + eLen) )
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       return -ENOMEM;
                    }
                    // P2P IE is saved to Additional IE ; it should be accumulated to handle WPS IE + P2P IE
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
#ifdef WLAN_FEATURE_WFD
                else if ( (0 == memcmp(&genie[0], WFD_OUI_TYPE,
                                                         WFD_OUI_TYPE_SIZE))
                        /*Consider WFD IE, only for P2P Client */
                         && (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) )
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set WFD IE(len %d)",
                            __func__, eLen + 2);

                    if( SIR_MAC_MAX_IE_LENGTH < (pWextState->assocAddIE.length + eLen) )
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       return -ENOMEM;
                    }
                    // WFD IE is saved to Additional IE ; it should be accumulated to handle
                    // WPS IE + P2P IE + WFD IE
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
#endif
                /* Appending HS 2.0 Indication Element in Assiciation Request */
                else if ( (0 == memcmp(&genie[0], HS20_OUI_TYPE,
                                       HS20_OUI_TYPE_SIZE)) )
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set HS20 IE(len %d)",
                            __func__, eLen + 2);

                    if( SIR_MAC_MAX_IE_LENGTH < (pWextState->assocAddIE.length + eLen) )
                    {
                        hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                               "Need bigger buffer space");
                        VOS_ASSERT(0);
                        return -ENOMEM;
                    }
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
                 /* Appending OSEN Information  Element in Assiciation Request */
                else if ( (0 == memcmp(&genie[0], OSEN_OUI_TYPE,
                                       OSEN_OUI_TYPE_SIZE)) )
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set OSEN IE(len %d)",
                            __func__, eLen + 2);

                    if( SIR_MAC_MAX_IE_LENGTH < (pWextState->assocAddIE.length + eLen) )
                    {
                        hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                               "Need bigger buffer space");
                        VOS_ASSERT(0);
                        return -ENOMEM;
                    }
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.bOSENAssociation = VOS_TRUE;
                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }

                break;
                if (WLAN_HDD_IBSS == pAdapter->device_mode) {

                   /* populating as ADDIE in beacon frames */
                   if (ccmCfgSetStr(WLAN_HDD_GET_HAL_CTX(pAdapter),
                       WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA, genie - 2, eLen + 2,
                       NULL, eANI_BOOLEAN_FALSE)== eHAL_STATUS_SUCCESS)
                   {
                       if (ccmCfgSetInt(WLAN_HDD_GET_HAL_CTX(pAdapter),
                           WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG, 1,NULL,
                           eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
                       {
                           hddLog(LOGE,
                                  "Coldn't pass "
                                  "WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG to CCM");
                       }
                   }/* ccmCfgSetStr(,WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA, , )*/
                   else
                       hddLog(LOGE,
                              "Could not pass on "
                              "WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA to CCM");

                   /* IBSS mode doesn't contain params->proberesp_ies still
                    beaconIE's need to be populated in probe response frames */
                   if ( (NULL != (genie - 2)) && (0 != eLen + 2) )
                   {
                      u16 rem_probe_resp_ie_len = eLen + 2;
                      u8 probe_rsp_ie_len[3] = {0};
                      u8 counter = 0;

                      /* Check Probe Resp Length if it is greater then 255 then
                         Store Probe Rsp IEs into WNI_CFG_PROBE_RSP_ADDNIE_DATA1
                         & WNI_CFG_PROBE_RSP_ADDNIE_DATA2 CFG Variable As We are
                         not able Store More then 255 bytes into One Variable */

                      while ((rem_probe_resp_ie_len > 0) && (counter < 3))
                      {
                         if (rem_probe_resp_ie_len > MAX_CFG_STRING_LEN)
                         {
                            probe_rsp_ie_len[counter++] = MAX_CFG_STRING_LEN;
                            rem_probe_resp_ie_len -= MAX_CFG_STRING_LEN;
                         }
                         else
                         {
                            probe_rsp_ie_len[counter++] = rem_probe_resp_ie_len;
                            rem_probe_resp_ie_len = 0;
                         }
                      }

                      rem_probe_resp_ie_len = 0;

                      if (probe_rsp_ie_len[0] > 0)
                      {
                         if (ccmCfgSetStr(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                          WNI_CFG_PROBE_RSP_ADDNIE_DATA1,
                                          (tANI_U8*)(genie - 2),
                                          probe_rsp_ie_len[0], NULL,
                                          eANI_BOOLEAN_FALSE)
                                          == eHAL_STATUS_FAILURE)
                         {
                            hddLog(LOGE,
                                   "Could not pass"
                                   "on WNI_CFG_PROBE_RSP_ADDNIE_DATA1 to CCM");
                         }
                         rem_probe_resp_ie_len += probe_rsp_ie_len[0];
                      }

                      if (probe_rsp_ie_len[1] > 0)
                      {
                         if (ccmCfgSetStr(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                          WNI_CFG_PROBE_RSP_ADDNIE_DATA2,
                                          (tANI_U8*)(genie - (2 + rem_probe_resp_ie_len)),
                                          probe_rsp_ie_len[1], NULL,
                                          eANI_BOOLEAN_FALSE)
                                          == eHAL_STATUS_FAILURE)
                         {
                             hddLog(LOGE,
                                    "Could not pass"
                                    "on WNI_CFG_PROBE_RSP_ADDNIE_DATA2 to CCM");
                         }
                         rem_probe_resp_ie_len += probe_rsp_ie_len[1];
                      }

                      if (probe_rsp_ie_len[2] > 0)
                      {
                         if (ccmCfgSetStr(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                          WNI_CFG_PROBE_RSP_ADDNIE_DATA3,
                                          (tANI_U8*)(genie - (2 + rem_probe_resp_ie_len)),
                                          probe_rsp_ie_len[2], NULL,
                                          eANI_BOOLEAN_FALSE)
                                          == eHAL_STATUS_FAILURE)
                          {
                            hddLog(LOGE,
                                   "Could not pass"
                                   "on WNI_CFG_PROBE_RSP_ADDNIE_DATA3 to CCM");
                          }
                          rem_probe_resp_ie_len += probe_rsp_ie_len[2];
                       }

                       if (ccmCfgSetInt(WLAN_HDD_GET_HAL_CTX(pAdapter),
                           WNI_CFG_PROBE_RSP_ADDNIE_FLAG, 1,NULL,
                           eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
                       {
                          hddLog(LOGE,
                                "Could not pass"
                                "on WNI_CFG_PROBE_RSP_ADDNIE_FLAG to CCM");
                       }
                   }
                   else
                   {
                      // Reset WNI_CFG_PROBE_RSP Flags
                      wlan_hdd_reset_prob_rspies(pAdapter);

                      hddLog(VOS_TRACE_LEVEL_INFO,
                            "%s: No Probe Response IE received in set beacon",
                            __func__);
                   }
                } /* end of if (WLAN_HDD_IBSS == pAdapter->device_mode) */
                break;
            case DOT11F_EID_RSN:
                hddLog (VOS_TRACE_LEVEL_INFO, "%s Set RSN IE(len %d)",__func__, eLen + 2);
                memset( pWextState->WPARSNIE, 0, MAX_WPA_RSN_IE_LEN );
                memcpy( pWextState->WPARSNIE, genie - 2, (eLen + 2)/*ie_len*/);
                pWextState->roamProfile.pRSNReqIE = pWextState->WPARSNIE;
                pWextState->roamProfile.nRSNReqIELength = eLen + 2; //ie_len;
                break;
                /* Appending Extended Capabilities with Interworking bit set in Assoc Req */
            case DOT11F_EID_EXTCAP:
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set Extended CAPS IE(len %d)",
                            __func__, eLen + 2);

                    if( SIR_MAC_MAX_IE_LENGTH < (pWextState->assocAddIE.length + eLen) )
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       return -ENOMEM;
                    }
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                    break;
                }
#ifdef FEATURE_WLAN_WAPI
            case WLAN_EID_WAPI:
                pAdapter->wapi_info.nWapiMode = 1;   //Setting WAPI Mode to ON=1
                hddLog(VOS_TRACE_LEVEL_INFO, "WAPI MODE IS %u",
                                          pAdapter->wapi_info.nWapiMode);
                tmp = (u16 *)ie;
                tmp = tmp + 2; // Skip element Id and Len, Version
                akmsuiteCount = WPA_GET_LE16(tmp);
                tmp = tmp + 1;
                akmlist = (int *)(tmp);
                if(akmsuiteCount <= MAX_NUM_AKM_SUITES)
                {
                    memcpy(akmsuite, akmlist, (4*akmsuiteCount));
                }
                else
                {
                    hddLog(VOS_TRACE_LEVEL_FATAL, "Invalid akmSuite count");
                    VOS_ASSERT(0);
                    return -EINVAL;
                }

                if (WAPI_PSK_AKM_SUITE == akmsuite[0])
                {
                    hddLog(VOS_TRACE_LEVEL_INFO, "%s: WAPI AUTH MODE SET TO PSK",
                                                            __func__);
                    pAdapter->wapi_info.wapiAuthMode = WAPI_AUTH_MODE_PSK;
                }
                if (WAPI_CERT_AKM_SUITE == akmsuite[0])
                {
                    hddLog(VOS_TRACE_LEVEL_INFO, "%s: WAPI AUTH MODE SET TO CERTIFICATE",
                                                             __func__);
                    pAdapter->wapi_info.wapiAuthMode = WAPI_AUTH_MODE_CERT;
                }
                break;
#endif
            default:
                hddLog (VOS_TRACE_LEVEL_ERROR,
                        "%s Set UNKNOWN IE %X", __func__, elementId);
                /* when Unknown IE is received we should break and continue
                 * to the next IE in the buffer instead we were returning
                 * so changing this to break */
                break;
        }
        genie += eLen;
        remLen -= eLen;
    }
    EXIT();
    return 0;
}

/*
 * FUNCTION: hdd_isWPAIEPresent
 * Parse the received IE to find the WPA IE
 *
 */
static bool hdd_isWPAIEPresent(u8 *ie, u8 ie_len)
{
    v_U8_t eLen = 0;
    v_U16_t remLen = ie_len;
    v_U8_t elementId = 0;

    while (remLen >= 2)
    {
        elementId = *ie++;
        eLen  = *ie++;
        remLen -= 2;
        if (eLen > remLen)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: IE length is wrong %d", __func__, eLen);
            return FALSE;
        }
        if ((elementId == DOT11F_EID_WPA) && (remLen > 5))
        {
          /* OUI - 0x00 0X50 0XF2
             WPA Information Element - 0x01
             WPA version - 0x01*/
            if (0 == memcmp(&ie[0], "\x00\x50\xf2\x01\x01", 5))
               return TRUE;
        }
        ie += eLen;
        remLen -= eLen;
    }
    return FALSE;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_set_privacy
 * This function is used to initialize the security
 * parameters during connect operation.
 */
int wlan_hdd_cfg80211_set_privacy( hdd_adapter_t *pAdapter,
                                   struct cfg80211_connect_params *req
                                   )
{
    int status = 0;
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    ENTER();

    /*set wpa version*/
    pWextState->wpaVersion = IW_AUTH_WPA_VERSION_DISABLED;

    if (req->crypto.wpa_versions)
    {
        if (NL80211_WPA_VERSION_1 == req->crypto.wpa_versions)
        {
            pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA;
        }
        else if (NL80211_WPA_VERSION_2 == req->crypto.wpa_versions)
        {
            pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA2;
        }
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: set wpa version to %d", __func__,
            pWextState->wpaVersion);

    /*set authentication type*/
    status = wlan_hdd_cfg80211_set_auth_type(pAdapter, req->auth_type);

    if (0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: failed to set authentication type ", __func__);
        return status;
    }

    /*set key mgmt type*/
    if (req->crypto.n_akm_suites)
    {
        status = wlan_hdd_set_akm_suite(pAdapter, req->crypto.akm_suites[0]);
        if (0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to set akm suite",
                    __func__);
            return status;
        }
    }

    /*set pairwise cipher type*/
    if (req->crypto.n_ciphers_pairwise)
    {
        status = wlan_hdd_cfg80211_set_cipher(pAdapter,
                                      req->crypto.ciphers_pairwise[0], true);
        if (0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: failed to set unicast cipher type", __func__);
            return status;
        }
    }
    else
    {
        /*Reset previous cipher suite to none*/
        status = wlan_hdd_cfg80211_set_cipher(pAdapter, 0, true);
        if (0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: failed to set unicast cipher type", __func__);
            return status;
        }
    }

    /*set group cipher type*/
    status = wlan_hdd_cfg80211_set_cipher(pAdapter, req->crypto.cipher_group,
                                                                       false);

    if (0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to set mcast cipher type",
                __func__);
        return status;
    }

#ifdef WLAN_FEATURE_11W
    pWextState->roamProfile.MFPEnabled = (req->mfp == NL80211_MFP_REQUIRED);
#endif

    /*parse WPA/RSN IE, and set the correspoing fileds in Roam profile*/
    if (req->ie_len)
    {
        status = wlan_hdd_cfg80211_set_ie(pAdapter, req->ie, req->ie_len);
        if ( 0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to parse the WPA/RSN IE",
                    __func__);
            return status;
        }
    }

    /*incase of WEP set default key information*/
    if (req->key && req->key_len)
    {
        if ( (WLAN_CIPHER_SUITE_WEP40 == req->crypto.ciphers_pairwise[0])
                || (WLAN_CIPHER_SUITE_WEP104 == req->crypto.ciphers_pairwise[0])
          )
        {
            if ( IW_AUTH_KEY_MGMT_802_1X
                    == (pWextState->authKeyMgmt & IW_AUTH_KEY_MGMT_802_1X  ))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Dynamic WEP not supported",
                        __func__);
                return -EOPNOTSUPP;
            }
            else
            {
                u8 key_len = req->key_len;
                u8 key_idx = req->key_idx;

                if ((eCSR_SECURITY_WEP_KEYSIZE_MAX_BYTES >= key_len)
                        && (CSR_MAX_NUM_KEY > key_idx)
                  )
                {
                    hddLog(VOS_TRACE_LEVEL_INFO,
                     "%s: setting default wep key, key_idx = %hu key_len %hu",
                            __func__, key_idx, key_len);
                    vos_mem_copy(
                       &pWextState->roamProfile.Keys.KeyMaterial[key_idx][0],
                                  req->key, key_len);
                    pWextState->roamProfile.Keys.KeyLength[key_idx] =
                                                               (u8)key_len;
                    pWextState->roamProfile.Keys.defaultIndex = (u8)key_idx;
                }
            }
        }
    }

    return status;
}

/*
 * FUNCTION: wlan_hdd_try_disconnect
 * This function is used to disconnect from previous
 * connection
 */
static int wlan_hdd_try_disconnect( hdd_adapter_t *pAdapter )
{
    long ret = 0;
    hdd_station_ctx_t *pHddStaCtx;
    eMib_dot11DesiredBssType connectedBssType;

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    hdd_connGetConnectedBssType(pHddStaCtx,&connectedBssType );

    if((eMib_dot11DesiredBssType_independent == connectedBssType) ||
      (eConnectionState_Associated == pHddStaCtx->conn_info.connState) ||
      (eConnectionState_IbssConnected == pHddStaCtx->conn_info.connState))
    {
        /* Issue disconnect to CSR */
        INIT_COMPLETION(pAdapter->disconnect_comp_var);
        if( eHAL_STATUS_SUCCESS ==
              sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                        pAdapter->sessionId,
                        eCSR_DISCONNECT_REASON_UNSPECIFIED ) )
        {
            ret = wait_for_completion_interruptible_timeout(
                         &pAdapter->disconnect_comp_var,
                         msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
            if (0 >=  ret)
            {
                hddLog(LOGE, FL("Failed to receive disconnect event"));
                return -EALREADY;
            }
        }
    }
    else if(eConnectionState_Disconnecting == pHddStaCtx->conn_info.connState)
    {
        ret = wait_for_completion_interruptible_timeout(
                     &pAdapter->disconnect_comp_var,
                     msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
        if (0 >= ret)
        {
            hddLog(LOGE, FL("Failed to receive disconnect event"));
            return -EALREADY;
        }
    }

    return 0;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_connect
 * This function is used to start the association process
 */
static int __wlan_hdd_cfg80211_connect( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      struct cfg80211_connect_params *req
                                      )
{
    int status;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    VOS_STATUS exitbmpsStatus = VOS_STATUS_E_INVAL;
    hdd_context_t *pHddCtx = NULL;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_CONNECT,
                      pAdapter->sessionId, pAdapter->device_mode));
    hddLog(VOS_TRACE_LEVEL_INFO,
           "%s: device_mode = %s (%d)", __func__,
           hdd_device_modetoString(pAdapter->device_mode),
                                   pAdapter->device_mode);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (!pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is null", __func__);
        return -EINVAL;
    }

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Reached max concurrent connections"));
        return -ECONNREFUSED;
    }

#ifdef WLAN_BTAMP_FEATURE
    //Infra connect not supported when AMP traffic is on.
    if( VOS_TRUE == WLANBAP_AmpSessionOn() )
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: No connection when AMP is on", __func__);
        return -ECONNREFUSED;
    }
#endif

    //If Device Mode is Station Concurrent Sessions Exit BMps
    //P2P Mode will be taken care in Open/close adapter
    if((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) &&
        (vos_concurrent_open_sessions_running())) {
        exitbmpsStatus = hdd_disable_bmps_imps(pHddCtx,
                                               WLAN_HDD_INFRA_STATION);
    }

    /*Try disconnecting if already in connected state*/
    status = wlan_hdd_try_disconnect(pAdapter);
    if ( 0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Failed to disconnect the existing"
                " connection"));
        return -EALREADY;
    }

    /*initialise security parameters*/
    status = wlan_hdd_cfg80211_set_privacy(pAdapter, req);

    if ( 0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to set security params",
                __func__);
        return status;
    }
    if ( req->channel )
    {
        status = wlan_hdd_cfg80211_connect_start(pAdapter, req->ssid,
                                                  req->ssid_len, req->bssid,
                                                  req->channel->hw_value);
    }
    else
    {
        status = wlan_hdd_cfg80211_connect_start(pAdapter, req->ssid,
                                                  req->ssid_len, req->bssid, 0);
    }

    if (0 > status)
    {
        //ReEnable BMPS if disabled
        if((VOS_STATUS_SUCCESS == exitbmpsStatus) &&
            (NULL != pHddCtx))
        {
            if (pHddCtx->hdd_wlan_suspended)
            {
                hdd_set_pwrparams(pHddCtx);
            }
           //ReEnable Bmps and Imps back
           hdd_enable_bmps_imps(pHddCtx);
        }
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("connect failed"));
        return status;
    }
    pHddCtx->isAmpAllowed = VOS_FALSE;
    EXIT();
    return status;
}

static int wlan_hdd_cfg80211_connect( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      struct cfg80211_connect_params *req)
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_connect(wiphy, ndev, req);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: wlan_hdd_disconnect
 * This function is used to issue a disconnect request to SME
 */
int wlan_hdd_disconnect( hdd_adapter_t *pAdapter, u16 reason )
{
    int status, result = 0;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    long ret;

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    pHddCtx->isAmpAllowed = VOS_TRUE;

    /* Need to apply spin lock before decreasing active sessions
     * as there can be chance for double decrement if context switch
     * Calls  hdd_DisConnectHandler.
     */

    spin_lock_bh(&pAdapter->lock_for_active_session);
    if (eConnectionState_Associated ==  pHddStaCtx->conn_info.connState)
    {
        wlan_hdd_decr_active_session(pHddCtx, pAdapter->device_mode);
    }
    hdd_connSetConnectionState( pHddStaCtx, eConnectionState_Disconnecting );
    spin_unlock_bh(&pAdapter->lock_for_active_session);

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  FL( "Set HDD connState to eConnectionState_Disconnecting" ));

    INIT_COMPLETION(pAdapter->disconnect_comp_var);

    /*
     * stop tx queues before deleting STA/BSS context from the firmware.
     * tx has to be disabled because the firmware can get busy dropping
     * the tx frames after BSS/STA has been deleted and will not send
     * back a response resulting in WDI timeout
     */
    netif_tx_disable(pAdapter->dev);
    netif_carrier_off(pAdapter->dev);

    /*issue disconnect*/
    status = sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                 pAdapter->sessionId, reason);
     if(eHAL_STATUS_CMD_NOT_QUEUED == status)
     {
        hddLog(VOS_TRACE_LEVEL_INFO,
             FL("status = %d, already disconnected"),
                     (int)status );

    }
    else if ( 0 != status )
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s csrRoamDisconnect failure, returned %d",
               __func__, (int)status );
        result = -EINVAL;
        goto disconnected;
    }
    ret = wait_for_completion_interruptible_timeout(
                &pAdapter->disconnect_comp_var,
                msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
    if (!ret && ( eHAL_STATUS_CMD_NOT_QUEUED != status ))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: Failed to disconnect, timed out", __func__);
        result = -ETIMEDOUT;
    }
    else if (ret == -ERESTARTSYS)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to disconnect, wait interrupted", __func__);
        result = -ERESTARTSYS;
    }
disconnected:
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              FL("Set HDD connState to eConnectionState_NotConnected"));
    pHddStaCtx->conn_info.connState = eConnectionState_NotConnected;

    return result;
}


/*
 * FUNCTION: __wlan_hdd_cfg80211_disconnect
 * This function is used to issue a disconnect request to SME
 */
static int __wlan_hdd_cfg80211_disconnect( struct wiphy *wiphy,
                                         struct net_device *dev,
                                         u16 reason
                                         )
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    tCsrRoamProfile  *pRoamProfile =
                    &(WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter))->roamProfile;
    int status;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
#ifdef FEATURE_WLAN_TDLS
    tANI_U8 staIdx;
#endif

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_DISCONNECT,
                     pAdapter->sessionId, reason));
    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s(%d)",
           __func__, hdd_device_modetoString(pAdapter->device_mode),
                                             pAdapter->device_mode);

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: Disconnect called with reason code %d",
            __func__, reason);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if (NULL != pRoamProfile)
    {
        /*issue disconnect request to SME, if station is in connected state*/
        if ((pHddStaCtx->conn_info.connState == eConnectionState_Associated) ||
            (pHddStaCtx->conn_info.connState == eConnectionState_Connecting))
        {
            eCsrRoamDisconnectReason reasonCode =
                                       eCSR_DISCONNECT_REASON_UNSPECIFIED;
            hdd_scaninfo_t *pScanInfo;
            switch(reason)
            {
                case WLAN_REASON_MIC_FAILURE:
                    reasonCode = eCSR_DISCONNECT_REASON_MIC_ERROR;
                    break;

                case WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY:
                case WLAN_REASON_DISASSOC_AP_BUSY:
                case WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA:
                    reasonCode = eCSR_DISCONNECT_REASON_DISASSOC;
                    break;

                case WLAN_REASON_PREV_AUTH_NOT_VALID:
                case WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA:
                case WLAN_REASON_DEAUTH_LEAVING:
                    reasonCode = eCSR_DISCONNECT_REASON_DEAUTH;
                    break;

                default:
                    reasonCode = eCSR_DISCONNECT_REASON_UNSPECIFIED;
                    break;
            }
            pScanInfo =  &pHddCtx->scan_info;
            if (pScanInfo->mScanPending)
            {
                hddLog(VOS_TRACE_LEVEL_INFO, "Disconnect is in progress, "
                              "Aborting Scan");
                hdd_abort_mac_scan(pHddCtx, pScanInfo->sessionId,
                                   eCSR_SCAN_ABORT_DEFAULT);
            }

#ifdef FEATURE_WLAN_TDLS
            /* First clean up the tdls peers if any */
            for (staIdx = 0 ; staIdx < HDD_MAX_NUM_TDLS_STA; staIdx++)
            {
                if ((pHddCtx->tdlsConnInfo[staIdx].sessionId == pAdapter->sessionId) &&
                    (pHddCtx->tdlsConnInfo[staIdx].staId))
                {
                    uint8 *mac;
                    mac = pHddCtx->tdlsConnInfo[staIdx].peerMac.bytes;
                    VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                            "%s: call sme_DeleteTdlsPeerSta staId %d sessionId %d " MAC_ADDRESS_STR,
                            __func__, pHddCtx->tdlsConnInfo[staIdx].staId, pAdapter->sessionId,
                            MAC_ADDR_ARRAY(mac));
                    sme_DeleteTdlsPeerSta(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                          pAdapter->sessionId,
                                          mac);
                }
            }
#endif
           hddLog(LOG1, FL("Disconnecting with reasoncode:%u"), reasonCode);
            status = wlan_hdd_disconnect(pAdapter, reasonCode);
            if ( 0 != status )
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s wlan_hdd_disconnect failure, returned %d",
                        __func__, (int)status );
                return -EINVAL;
            }
        }
        else
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: unexpected cfg disconnect API"
                   "called while in %d state", __func__,
                    pHddStaCtx->conn_info.connState);
        }
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: No valid roam profile", __func__);
    }

    return status;
}

static int wlan_hdd_cfg80211_disconnect( struct wiphy *wiphy,
                                         struct net_device *dev,
                                         u16 reason
                                         )
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_disconnect(wiphy, dev, reason);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_set_privacy_ibss
 * This function is used to initialize the security
 * settings in IBSS mode.
 */
static int wlan_hdd_cfg80211_set_privacy_ibss(
                                         hdd_adapter_t *pAdapter,
                                         struct cfg80211_ibss_params *params
                                         )
{
    int status = 0;
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    eCsrEncryptionType encryptionType = eCSR_ENCRYPT_TYPE_NONE;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    ENTER();

    pWextState->wpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
    vos_mem_zero(&pHddStaCtx->ibss_enc_key, sizeof(tCsrRoamSetKey));

    if (params->ie_len && ( NULL != params->ie) )
    {
        if (wlan_hdd_cfg80211_get_ie_ptr (params->ie,
                            params->ie_len, WLAN_EID_RSN ))
        {
            pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA2;
            encryptionType = eCSR_ENCRYPT_TYPE_AES;
        }
        else if ( hdd_isWPAIEPresent (params->ie, params->ie_len ))
        {
            tDot11fIEWPA dot11WPAIE;
            tHalHandle halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);
            u8 *ie;

            memset(&dot11WPAIE, 0, sizeof(dot11WPAIE));
            ie = wlan_hdd_cfg80211_get_ie_ptr (params->ie,
                                     params->ie_len, DOT11F_EID_WPA);
            if ( NULL != ie )
            {
                pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA;
                // Unpack the WPA IE
                //Skip past the EID byte and length byte - and four byte WiFi OUI
                dot11fUnpackIeWPA((tpAniSirGlobal) halHandle,
                                &ie[2+4],
                                ie[1] - 4,
                                &dot11WPAIE);
                /*Extract the multicast cipher, the encType for unicast
                               cipher for wpa-none is none*/
                encryptionType =
                  hdd_TranslateWPAToCsrEncryptionType(dot11WPAIE.multicast_cipher);
            }
        }

        status = wlan_hdd_cfg80211_set_ie(pAdapter, params->ie, params->ie_len);

        if (0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to parse WPA/RSN IE",
                    __func__);
            return status;
        }
    }

    pWextState->roamProfile.AuthType.authType[0] =
                                pHddStaCtx->conn_info.authType =
                                eCSR_AUTH_TYPE_OPEN_SYSTEM;

    if (params->privacy)
    {
        /* Security enabled IBSS, At this time there is no information available
         * about the security paramters, so initialise the encryption type to
         * eCSR_ENCRYPT_TYPE_WEP40_STATICKEY.
         * The correct security parameters will be updated later in
         * wlan_hdd_cfg80211_add_key */
        /* Hal expects encryption type to be set inorder
         *enable privacy bit in beacons */

        encryptionType = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
    }
    VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                          "encryptionType=%d", encryptionType);
    pHddStaCtx->conn_info.ucEncryptionType                   = encryptionType;
    pWextState->roamProfile.EncryptionType.numEntries        = 1;
    pWextState->roamProfile.EncryptionType.encryptionType[0] = encryptionType;
    return status;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_join_ibss
 * This function is used to create/join an IBSS
 */
static int __wlan_hdd_cfg80211_join_ibss( struct wiphy *wiphy,
                                        struct net_device *dev,
                                        struct cfg80211_ibss_params *params
                                       )
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrRoamProfile          *pRoamProfile;
    int status;
    bool alloc_bssid = VOS_FALSE;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_JOIN_IBSS,
                     pAdapter->sessionId, pAdapter->device_mode));
    hddLog(VOS_TRACE_LEVEL_INFO,
           "%s: device_mode = %s (%d)", __func__,
           hdd_device_modetoString(pAdapter->device_mode),
                                   pAdapter->device_mode);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if (NULL == pWextState)
    {
        hddLog (VOS_TRACE_LEVEL_ERROR, "%s ERROR: Data Storage Corruption",
                __func__);
        return -EIO;
    }

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Reached max concurrent connections"));
        return -ECONNREFUSED;
    }

    /*Try disconnecting if already in connected state*/
    status = wlan_hdd_try_disconnect(pAdapter);
    if ( 0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Failed to disconnect the existing"
                " IBSS connection"));
        return -EALREADY;
    }

    pRoamProfile = &pWextState->roamProfile;

    if ( eCSR_BSS_TYPE_START_IBSS != pRoamProfile->BSSType )
    {
        hddLog (VOS_TRACE_LEVEL_ERROR,
                "%s Interface type is not set to IBSS", __func__);
        return -EINVAL;
    }

    /* BSSID is provided by upper layers hence no need to AUTO generate */
    if (NULL != params->bssid) {
       if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IBSS_AUTO_BSSID, 0,
                        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE) {
           hddLog (VOS_TRACE_LEVEL_ERROR,
                  "%s:ccmCfgStInt faild for WNI_CFG_IBSS_AUTO_BSSID", __func__);
           return -EIO;
       }
    }
    else if(pHddCtx->cfg_ini->isCoalesingInIBSSAllowed == 0)
    {
        if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IBSS_AUTO_BSSID, 0,
                         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
        {
            hddLog (VOS_TRACE_LEVEL_ERROR,
                    "%s:ccmCfgStInt faild for WNI_CFG_IBSS_AUTO_BSSID", __func__);
            return -EIO;
        }
        params->bssid = vos_mem_malloc(sizeof(VOS_MAC_ADDR_SIZE));
        if (!params->bssid)
        {
            hddLog (VOS_TRACE_LEVEL_ERROR,
                    "%s:Failed memory allocation", __func__);
            return -EIO;
        }
        vos_mem_copy((v_U8_t *)params->bssid,
                     (v_U8_t *)&pHddCtx->cfg_ini->IbssBssid.bytes[0],
                     VOS_MAC_ADDR_SIZE);
        alloc_bssid = VOS_TRUE;
    }

    /* Set Channel */
    if (NULL !=
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
                params->chandef.chan)
#else
                params->channel)
#endif
    {
        u8 channelNum;
        v_U32_t numChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;
        v_U8_t validChan[WNI_CFG_VALID_CHANNEL_LIST_LEN];
        tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
        int indx;

        /* Get channel number */
        channelNum =
               ieee80211_frequency_to_channel(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
                                            params->chandef.chan->center_freq);
#else
                                            params->channel->center_freq);
#endif

        if (0 != ccmCfgGetStr(hHal, WNI_CFG_VALID_CHANNEL_LIST,
                    validChan, &numChans))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: No valid channel list",
                    __func__);
            return -EOPNOTSUPP;
        }

        for (indx = 0; indx < numChans; indx++)
        {
            if (channelNum == validChan[indx])
            {
                break;
            }
        }
        if (indx >= numChans)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Not valid Channel %d",
                    __func__, channelNum);
            return -EINVAL;
        }
        /* Set the Operational Channel */
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: set channel %d", __func__,
                channelNum);
        pRoamProfile->ChannelInfo.numOfChannels = 1;
        pHddStaCtx->conn_info.operationChannel = channelNum;
        pRoamProfile->ChannelInfo.ChannelList =
            &pHddStaCtx->conn_info.operationChannel;
    }

    /* Initialize security parameters */
    status = wlan_hdd_cfg80211_set_privacy_ibss(pAdapter, params);
    if (status < 0)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to set security parameters",
                __func__);
        return status;
    }

    /* Issue connect start */
    status = wlan_hdd_cfg80211_connect_start(pAdapter, params->ssid,
            params->ssid_len, params->bssid,
            pHddStaCtx->conn_info.operationChannel);

    if (0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: connect failed", __func__);
        return status;
    }

    if (NULL != params->bssid &&
        pHddCtx->cfg_ini->isCoalesingInIBSSAllowed == 0 &&
        alloc_bssid == VOS_TRUE)
    {
        vos_mem_free(params->bssid);
    }
    return 0;
}

static int wlan_hdd_cfg80211_join_ibss( struct wiphy *wiphy,
                                        struct net_device *dev,
                                        struct cfg80211_ibss_params *params
                                       )
{
    int ret = 0;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_join_ibss(wiphy, dev, params);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_leave_ibss
 * This function is used to leave an IBSS
 */
static int __wlan_hdd_cfg80211_leave_ibss( struct wiphy *wiphy,
                                         struct net_device *dev
                                         )
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrRoamProfile *pRoamProfile;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    int status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_LEAVE_IBSS,
                      pAdapter->sessionId, eCSR_DISCONNECT_REASON_IBSS_LEAVE));
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %s (%d)", __func__,
                       hdd_device_modetoString(pAdapter->device_mode),
                                               pAdapter->device_mode);
    if (NULL == pWextState)
    {
        hddLog (VOS_TRACE_LEVEL_ERROR, "%s ERROR: Data Storage Corruption",
                __func__);
        return -EIO;
    }

    pRoamProfile = &pWextState->roamProfile;

    /* Issue disconnect only if interface type is set to IBSS */
    if (eCSR_BSS_TYPE_START_IBSS != pRoamProfile->BSSType)
    {
        hddLog (VOS_TRACE_LEVEL_ERROR, "%s: BSS Type is not set to IBSS",
                __func__);
        return -EINVAL;
    }

    /* Issue Disconnect request */
    INIT_COMPLETION(pAdapter->disconnect_comp_var);
    sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId,
                                  eCSR_DISCONNECT_REASON_IBSS_LEAVE);

    return 0;
}

static int wlan_hdd_cfg80211_leave_ibss( struct wiphy *wiphy,
                                         struct net_device *dev
                                         )
{
    int ret = 0;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_leave_ibss(wiphy, dev);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_wiphy_params
 * This function is used to set the phy parameters
 * (RTS Threshold/FRAG Threshold/Retry Count etc ...)
 */
static int __wlan_hdd_cfg80211_set_wiphy_params(struct wiphy *wiphy,
        u32 changed)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    tHalHandle hHal = pHddCtx->hHal;
    int status;

    ENTER();
     MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                      TRACE_CODE_HDD_CFG80211_SET_WIPHY_PARAMS,
                       NO_SESSION, wiphy->rts_threshold));
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if (changed & WIPHY_PARAM_RTS_THRESHOLD)
    {
        u16 rts_threshold = (wiphy->rts_threshold == -1) ?
                               WNI_CFG_RTS_THRESHOLD_STAMAX :
                               wiphy->rts_threshold;

        if ((WNI_CFG_RTS_THRESHOLD_STAMIN > rts_threshold) ||
                (WNI_CFG_RTS_THRESHOLD_STAMAX < rts_threshold))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Invalid RTS Threshold value %hu",
                    __func__, rts_threshold);
            return -EINVAL;
        }

        if (0 != ccmCfgSetInt(hHal, WNI_CFG_RTS_THRESHOLD,
                    rts_threshold, ccmCfgSetCallback,
                    eANI_BOOLEAN_TRUE))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: ccmCfgSetInt failed for rts_threshold value %hu",
                    __func__, rts_threshold);
            return -EIO;
        }

        hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set rts threshold %hu", __func__,
                rts_threshold);
    }

    if (changed & WIPHY_PARAM_FRAG_THRESHOLD)
    {
        u16 frag_threshold = (wiphy->frag_threshold == -1) ?
                                WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX :
                                wiphy->frag_threshold;

        if ((WNI_CFG_FRAGMENTATION_THRESHOLD_STAMIN > frag_threshold)||
                (WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX < frag_threshold) )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Invalid frag_threshold value %hu", __func__,
                    frag_threshold);
            return -EINVAL;
        }

        if (0 != ccmCfgSetInt(hHal, WNI_CFG_FRAGMENTATION_THRESHOLD,
                    frag_threshold, ccmCfgSetCallback,
                    eANI_BOOLEAN_TRUE))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: ccmCfgSetInt failed for frag_threshold value %hu",
                    __func__, frag_threshold);
            return -EIO;
        }

        hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set frag threshold %hu", __func__,
                frag_threshold);
    }

    if ((changed & WIPHY_PARAM_RETRY_SHORT)
            || (changed & WIPHY_PARAM_RETRY_LONG))
    {
        u8 retry_value = (changed & WIPHY_PARAM_RETRY_SHORT) ?
                         wiphy->retry_short :
                         wiphy->retry_long;

        if ((WNI_CFG_LONG_RETRY_LIMIT_STAMIN > retry_value) ||
                (WNI_CFG_LONG_RETRY_LIMIT_STAMAX < retry_value))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid Retry count %hu",
                    __func__, retry_value);
            return -EINVAL;
        }

        if (changed & WIPHY_PARAM_RETRY_SHORT)
        {
            if (0 != ccmCfgSetInt(hHal, WNI_CFG_LONG_RETRY_LIMIT,
                        retry_value, ccmCfgSetCallback,
                        eANI_BOOLEAN_TRUE))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: ccmCfgSetInt failed for long retry count %hu",
                        __func__, retry_value);
                return -EIO;
            }
            hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set long retry count %hu",
                    __func__, retry_value);
        }
        else if (changed & WIPHY_PARAM_RETRY_SHORT)
        {
            if (0 != ccmCfgSetInt(hHal, WNI_CFG_SHORT_RETRY_LIMIT,
                        retry_value, ccmCfgSetCallback,
                        eANI_BOOLEAN_TRUE))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: ccmCfgSetInt failed for short retry count %hu",
                        __func__, retry_value);
                return -EIO;
            }
            hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set short retry count %hu",
                    __func__, retry_value);
        }
    }

    return 0;
}

static int wlan_hdd_cfg80211_set_wiphy_params(struct wiphy *wiphy,
                                                                u32 changed)
{
     int ret;

     vos_ssr_protect(__func__);
     ret = __wlan_hdd_cfg80211_set_wiphy_params(wiphy, changed);
     vos_ssr_unprotect(__func__);

     return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_txpower
 * This function is used to set the txpower
 */
static int __wlan_hdd_cfg80211_set_txpower(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
        struct wireless_dev *wdev,
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,35)
        enum tx_power_setting type,
#else
        enum nl80211_tx_power_setting type,
#endif
        int dbm)
{
    hdd_context_t *pHddCtx = (hdd_context_t*) wiphy_priv(wiphy);
    tHalHandle hHal = NULL;
    tSirMacAddr bssid = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    tSirMacAddr selfMac = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int status;

    ENTER();
    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                      TRACE_CODE_HDD_CFG80211_SET_TXPOWER,
                      NO_SESSION, type ));
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hHal = pHddCtx->hHal;

    if (0 != ccmCfgSetInt(hHal, WNI_CFG_CURRENT_TX_POWER_LEVEL,
                dbm, ccmCfgSetCallback,
                eANI_BOOLEAN_TRUE))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: ccmCfgSetInt failed for tx power %hu", __func__, dbm);
        return -EIO;
    }

    hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set tx power level %d dbm", __func__,
            dbm);

    switch(type)
    {
    case NL80211_TX_POWER_AUTOMATIC: /*automatically determine transmit power*/
       /* Fall through */
    case NL80211_TX_POWER_LIMITED: /*limit TX power by the mBm parameter*/
       if( sme_SetMaxTxPower(hHal, bssid, selfMac, dbm) != eHAL_STATUS_SUCCESS )
       {
          hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting maximum tx power failed",
                 __func__);
          return -EIO;
       }
       break;
    case NL80211_TX_POWER_FIXED: /*fix TX power to the mBm parameter*/
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s: NL80211_TX_POWER_FIXED not supported",
              __func__);
       return -EOPNOTSUPP;
       break;
    default:
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid power setting type %d",
              __func__, type);
       return -EIO;
    }

    return 0;
}

static int wlan_hdd_cfg80211_set_txpower(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
        struct wireless_dev *wdev,
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,35)
        enum tx_power_setting type,
#else
        enum nl80211_tx_power_setting type,
#endif
        int dbm)
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_set_txpower(wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
                                        wdev,
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,35)
                                        type,
#else
                                        type,
#endif
                                        dbm);
   vos_ssr_unprotect(__func__);

   return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_get_txpower
 * This function is used to read the txpower
 */
static int wlan_hdd_cfg80211_get_txpower(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
                                         struct wireless_dev *wdev,
#endif
                                         int *dbm)
{

    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx = (hdd_context_t*) wiphy_priv(wiphy);
    int status;

    ENTER();

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        *dbm = 0;
        return status;
    }

    pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Not in station context " ,__func__);
        return -ENOENT;
    }

    wlan_hdd_get_classAstats(pAdapter);
    *dbm = pAdapter->hdd_stats.ClassA_stat.max_pwr;

    EXIT();
    return 0;
}

static int __wlan_hdd_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
                                   u8* mac, struct station_info *sinfo)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    int ssidlen = pHddStaCtx->conn_info.SSID.SSID.length;
    tANI_U32 rate_flags;

    hdd_context_t *pHddCtx = (hdd_context_t*) wiphy_priv(wiphy);
    hdd_config_t  *pCfg    = pHddCtx->cfg_ini;

    tANI_U8  OperationalRates[CSR_DOT11_SUPPORTED_RATES_MAX];
    tANI_U32 ORLeng = CSR_DOT11_SUPPORTED_RATES_MAX;
    tANI_U8  ExtendedRates[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
    tANI_U32 ERLeng = CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX;
    tANI_U8  MCSRates[SIZE_OF_BASIC_MCS_SET];
    tANI_U32 MCSLeng = SIZE_OF_BASIC_MCS_SET;
    tANI_U16 maxRate = 0;
    tANI_U16 myRate;
    tANI_U16 currentRate = 0;
    tANI_U8  maxSpeedMCS = 0;
    tANI_U8  maxMCSIdx = 0;
    tANI_U8  rateFlag = 1;
    tANI_U8  i, j, rssidx, mode=0;
    tANI_U16 temp;
    int status;

#ifdef WLAN_FEATURE_11AC
    tANI_U32 vht_mcs_map;
    eDataRate11ACMaxMcs vhtMaxMcs;
#endif /* WLAN_FEATURE_11AC */

    ENTER();

    if ((eConnectionState_Associated != pHddStaCtx->conn_info.connState) ||
            (0 == ssidlen))
    {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: Not associated or"
                    " Invalid ssidlen, %d", __func__, ssidlen);
        /*To keep GUI happy*/
        return 0;
    }

    if (VOS_TRUE == pHddStaCtx->hdd_ReassocScenario)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  "%s: Roaming in progress, so unable to proceed this request", __func__);
        return 0;
    }

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }


    wlan_hdd_get_rssi(pAdapter, &sinfo->signal);
    sinfo->filled |= STATION_INFO_SIGNAL;

    wlan_hdd_get_station_stats(pAdapter);
    rate_flags = pAdapter->hdd_stats.ClassA_stat.tx_rate_flags;

    /*overwrite rate_flags if MAX link-speed need to be reported*/
    if ((eHDD_LINK_SPEED_REPORT_MAX == pCfg->reportMaxLinkSpeed) ||
        (eHDD_LINK_SPEED_REPORT_MAX_SCALED == pCfg->reportMaxLinkSpeed &&
         sinfo->signal >= pCfg->linkSpeedRssiLow))
    {
        rate_flags = pAdapter->maxRateFlags;
    }

    //convert to the UI units of 100kbps
    myRate = pAdapter->hdd_stats.ClassA_stat.tx_rate * 5;

#ifdef LINKSPEED_DEBUG_ENABLED
    pr_info("RSSI %d, RLMS %u, rate %d, rssi high %d, rssi mid %d, rssi low %d, rate_flags 0x%x, MCS %d\n",
            sinfo->signal,
            pCfg->reportMaxLinkSpeed,
            myRate,
            (int) pCfg->linkSpeedRssiHigh,
            (int) pCfg->linkSpeedRssiMid,
            (int) pCfg->linkSpeedRssiLow,
            (int) rate_flags,
            (int) pAdapter->hdd_stats.ClassA_stat.mcs_index);
#endif //LINKSPEED_DEBUG_ENABLED

    if (eHDD_LINK_SPEED_REPORT_ACTUAL != pCfg->reportMaxLinkSpeed)
    {
        // we do not want to necessarily report the current speed
        if (eHDD_LINK_SPEED_REPORT_MAX == pCfg->reportMaxLinkSpeed)
        {
            // report the max possible speed
            rssidx = 0;
        }
        else if (eHDD_LINK_SPEED_REPORT_MAX_SCALED == pCfg->reportMaxLinkSpeed)
        {
            // report the max possible speed with RSSI scaling
            if (sinfo->signal >= pCfg->linkSpeedRssiHigh)
            {
                // report the max possible speed
                rssidx = 0;
            }
            else if (sinfo->signal >= pCfg->linkSpeedRssiMid)
            {
                // report middle speed
                rssidx = 1;
            }
            else if (sinfo->signal >= pCfg->linkSpeedRssiLow)
            {
                // report middle speed
                rssidx = 2;
            }
            else
            {
                // report actual speed
                rssidx = 3;
            }
        }
        else
        {
            // unknown, treat as eHDD_LINK_SPEED_REPORT_MAX
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid value for reportMaxLinkSpeed: %u",
                    __func__, pCfg->reportMaxLinkSpeed);
            rssidx = 0;
        }

        maxRate = 0;

        /* Get Basic Rate Set */
        if (0 != ccmCfgGetStr(WLAN_HDD_GET_HAL_CTX(pAdapter), WNI_CFG_OPERATIONAL_RATE_SET,
                             OperationalRates, &ORLeng))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: ccm api returned failure", __func__);
            /*To keep GUI happy*/
            return 0;
        }

        for (i = 0; i < ORLeng; i++)
        {
            for (j = 0; j < (sizeof(supported_data_rate) / sizeof(supported_data_rate[0])); j ++)
            {
                /* Validate Rate Set */
                if (supported_data_rate[j].beacon_rate_index == (OperationalRates[i] & 0x7F))
                {
                    currentRate = supported_data_rate[j].supported_rate[rssidx];
                    break;
                }
            }
            /* Update MAX rate */
            maxRate = (currentRate > maxRate)?currentRate:maxRate;
        }

        /* Get Extended Rate Set */
        if (0 != ccmCfgGetStr(WLAN_HDD_GET_HAL_CTX(pAdapter), WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
                             ExtendedRates, &ERLeng))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: ccm api returned failure", __func__);
            /*To keep GUI happy*/
            return 0;
        }

        for (i = 0; i < ERLeng; i++)
        {
            for (j = 0; j < (sizeof(supported_data_rate) / sizeof(supported_data_rate[0])); j ++)
            {
                if (supported_data_rate[j].beacon_rate_index == (ExtendedRates[i] & 0x7F))
                {
                    currentRate = supported_data_rate[j].supported_rate[rssidx];
                    break;
                }
            }
            /* Update MAX rate */
            maxRate = (currentRate > maxRate)?currentRate:maxRate;
        }

        /* Get MCS Rate Set --
           Only if we are always reporting max speed  (or)
           if we have good rssi */
        if ((3 != rssidx) && !(rate_flags & eHAL_TX_RATE_LEGACY))
        {
            if (rate_flags & eHAL_TX_RATE_VHT80)
                mode = 2;
            else if (rate_flags & (eHAL_TX_RATE_VHT40 | eHAL_TX_RATE_HT40))
                mode = 1;
            else
                mode = 0;

            if (0 != ccmCfgGetStr(WLAN_HDD_GET_HAL_CTX(pAdapter), WNI_CFG_CURRENT_MCS_SET,
                                 MCSRates, &MCSLeng))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: ccm api returned failure", __func__);
                /*To keep GUI happy*/
                return 0;
            }
            rateFlag = 0;
#ifdef WLAN_FEATURE_11AC
            /* VHT80 rate has seperate rate table */
            if (rate_flags & (eHAL_TX_RATE_VHT20|eHAL_TX_RATE_VHT40|eHAL_TX_RATE_VHT80))
            {
                ccmCfgGetInt(WLAN_HDD_GET_HAL_CTX(pAdapter), WNI_CFG_VHT_TX_MCS_MAP, &vht_mcs_map);
                vhtMaxMcs = (eDataRate11ACMaxMcs)(vht_mcs_map & DATA_RATE_11AC_MCS_MASK );
                if (rate_flags & eHAL_TX_RATE_SGI)
                {
                    rateFlag |= 1;
                }
                if (DATA_RATE_11AC_MAX_MCS_7 == vhtMaxMcs)
                {
                    maxMCSIdx = 7;
                }
                else if (DATA_RATE_11AC_MAX_MCS_8 == vhtMaxMcs)
                {
                    maxMCSIdx = 8;
                }
                else if (DATA_RATE_11AC_MAX_MCS_9 == vhtMaxMcs)
                {
                    //VHT20 is supporting 0~8
                    if (rate_flags & eHAL_TX_RATE_VHT20)
                        maxMCSIdx = 8;
                    else
                        maxMCSIdx = 9;
                }

                if (0 != rssidx)/*check for scaled */
                {
                    //get middle rate MCS index if rssi=1/2
                    for (i=0; i <= maxMCSIdx; i++)
                    {
                        if (sinfo->signal <= rssiMcsTbl[mode][i])
                        {
                            maxMCSIdx = i;
                            break;
                        }
                    }
                }

                if (rate_flags & eHAL_TX_RATE_VHT80)
                {
                    currentRate = supported_vht_mcs_rate[pAdapter->hdd_stats.ClassA_stat.mcs_index].supported_VHT80_rate[rateFlag];
                    maxRate = supported_vht_mcs_rate[maxMCSIdx].supported_VHT80_rate[rateFlag];
                }
                else if (rate_flags & eHAL_TX_RATE_VHT40)
                {
                    currentRate = supported_vht_mcs_rate[pAdapter->hdd_stats.ClassA_stat.mcs_index].supported_VHT40_rate[rateFlag];
                    maxRate = supported_vht_mcs_rate[maxMCSIdx].supported_VHT40_rate[rateFlag];
                }
                else if (rate_flags & eHAL_TX_RATE_VHT20)
                {
                    currentRate = supported_vht_mcs_rate[pAdapter->hdd_stats.ClassA_stat.mcs_index].supported_VHT20_rate[rateFlag];
                    maxRate = supported_vht_mcs_rate[maxMCSIdx].supported_VHT20_rate[rateFlag];
                }

                maxSpeedMCS = 1;
                if (currentRate > maxRate)
                {
                    maxRate = currentRate;
                }

            }
            else
#endif /* WLAN_FEATURE_11AC */
            {
                if (rate_flags & eHAL_TX_RATE_HT40)
                {
                    rateFlag |= 1;
                }
                if (rate_flags & eHAL_TX_RATE_SGI)
                {
                    rateFlag |= 2;
                }

                temp = sizeof(supported_mcs_rate) / sizeof(supported_mcs_rate[0]);
                if (rssidx == 1 || rssidx == 2)
                {
                    //get middle rate MCS index if rssi=1/2
                    for (i=0; i <= 7; i++)
                    {
                        if (sinfo->signal <= rssiMcsTbl[mode][i])
                        {
                            temp = i+1;
                            break;
                         }
                     }
                }

                for (i = 0; i < MCSLeng; i++)
                {
                    for (j = 0; j < temp; j++)
                    {
                        if (supported_mcs_rate[j].beacon_rate_index == MCSRates[i])
                        {
                            currentRate = supported_mcs_rate[j].supported_rate[rateFlag];
                            break;
                        }
                    }
                    if ((j < temp) && (currentRate > maxRate))
                    {
                        maxRate     = currentRate;
                        maxSpeedMCS = 1;
                        maxMCSIdx   = supported_mcs_rate[j].beacon_rate_index;
                    }
                }
            }
        }

        else if (!(rate_flags & eHAL_TX_RATE_LEGACY))
        {
            maxRate = myRate;
            maxSpeedMCS = 1;
            maxMCSIdx = pAdapter->hdd_stats.ClassA_stat.mcs_index;
        }
        // make sure we report a value at least as big as our current rate
        if ((maxRate < myRate) || (0 == maxRate))
        {
           maxRate = myRate;
           if (rate_flags & eHAL_TX_RATE_LEGACY)
           {
              maxSpeedMCS = 0;
           }
           else
           {
              maxSpeedMCS = 1;
              maxMCSIdx = pAdapter->hdd_stats.ClassA_stat.mcs_index;
           }
        }

        if (rate_flags & eHAL_TX_RATE_LEGACY)
        {
            sinfo->txrate.legacy  = maxRate;
#ifdef LINKSPEED_DEBUG_ENABLED
            pr_info("Reporting legacy rate %d\n", sinfo->txrate.legacy);
#endif //LINKSPEED_DEBUG_ENABLED
        }
        else
        {
            sinfo->txrate.mcs    = maxMCSIdx;
#ifdef WLAN_FEATURE_11AC
            sinfo->txrate.nss = 1;
            if (rate_flags & eHAL_TX_RATE_VHT80)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
                sinfo->txrate.flags |= RATE_INFO_FLAGS_80_MHZ_WIDTH;
            }
            else if (rate_flags & eHAL_TX_RATE_VHT40)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
                sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
            }
            else if (rate_flags & eHAL_TX_RATE_VHT20)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
            }
#endif /* WLAN_FEATURE_11AC */
            if (rate_flags & (eHAL_TX_RATE_HT20 | eHAL_TX_RATE_HT40))
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
                if (rate_flags & eHAL_TX_RATE_HT40)
                {
                    sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
                }
            }
            if (rate_flags & eHAL_TX_RATE_SGI)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
            }

#ifdef LINKSPEED_DEBUG_ENABLED
            pr_info("Reporting MCS rate %d flags %x\n",
                    sinfo->txrate.mcs,
                    sinfo->txrate.flags );
#endif //LINKSPEED_DEBUG_ENABLED
        }
    }
    else
    {
        // report current rate instead of max rate

        if (rate_flags & eHAL_TX_RATE_LEGACY)
        {
            //provide to the UI in units of 100kbps
            sinfo->txrate.legacy = myRate;
#ifdef LINKSPEED_DEBUG_ENABLED
            pr_info("Reporting actual legacy rate %d\n", sinfo->txrate.legacy);
#endif //LINKSPEED_DEBUG_ENABLED
        }
        else
        {
            //must be MCS
            sinfo->txrate.mcs = pAdapter->hdd_stats.ClassA_stat.mcs_index;
#ifdef WLAN_FEATURE_11AC
            sinfo->txrate.nss = 1;
            if (rate_flags & eHAL_TX_RATE_VHT80)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
            }
            else
#endif /* WLAN_FEATURE_11AC */
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
            }
            if (rate_flags & eHAL_TX_RATE_SGI)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
            }
            if (rate_flags & eHAL_TX_RATE_HT40)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
            }
#ifdef WLAN_FEATURE_11AC
            else if (rate_flags & eHAL_TX_RATE_VHT80)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_80_MHZ_WIDTH;
            }
#endif /* WLAN_FEATURE_11AC */
#ifdef LINKSPEED_DEBUG_ENABLED
            pr_info("Reporting actual MCS rate %d flags %x\n",
                    sinfo->txrate.mcs,
                    sinfo->txrate.flags );
#endif //LINKSPEED_DEBUG_ENABLED
        }
    }
    sinfo->filled |= STATION_INFO_TX_BITRATE;

    sinfo->tx_packets =
       pAdapter->hdd_stats.summary_stat.tx_frm_cnt[0] +
       pAdapter->hdd_stats.summary_stat.tx_frm_cnt[1] +
       pAdapter->hdd_stats.summary_stat.tx_frm_cnt[2] +
       pAdapter->hdd_stats.summary_stat.tx_frm_cnt[3];

    sinfo->tx_retries =
       pAdapter->hdd_stats.summary_stat.retry_cnt[0] +
       pAdapter->hdd_stats.summary_stat.retry_cnt[1] +
       pAdapter->hdd_stats.summary_stat.retry_cnt[2] +
       pAdapter->hdd_stats.summary_stat.retry_cnt[3];

    sinfo->tx_failed =
       pAdapter->hdd_stats.summary_stat.fail_cnt[0] +
       pAdapter->hdd_stats.summary_stat.fail_cnt[1] +
       pAdapter->hdd_stats.summary_stat.fail_cnt[2] +
       pAdapter->hdd_stats.summary_stat.fail_cnt[3];

    sinfo->filled |=
       STATION_INFO_TX_PACKETS |
       STATION_INFO_TX_RETRIES |
       STATION_INFO_TX_FAILED;

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_GET_STA,
                      pAdapter->sessionId, maxRate));
       EXIT();
       return 0;
}

static int wlan_hdd_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
                                   u8* mac, struct station_info *sinfo)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_get_station(wiphy, dev, mac, sinfo);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __wlan_hdd_cfg80211_set_power_mgmt(struct wiphy *wiphy,
                     struct net_device *dev, bool mode, int timeout)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    VOS_STATUS vos_status;
    int status;

    ENTER();

    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Adapter is NULL", __func__);
        return -ENODEV;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_POWER_MGMT,
                     pAdapter->sessionId, timeout));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if ((DRIVER_POWER_MODE_AUTO == !mode) &&
        (TRUE == pHddCtx->hdd_wlan_suspended) &&
        (pHddCtx->cfg_ini->fhostArpOffload) &&
        (eConnectionState_Associated ==
             (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
    {

        hddLog(VOS_TRACE_LEVEL_INFO,
               "offload: in cfg80211_set_power_mgmt, calling arp offload");
        vos_status = hdd_conf_arp_offload(pAdapter, TRUE);
        if (!VOS_IS_STATUS_SUCCESS(vos_status))
        {
            hddLog(VOS_TRACE_LEVEL_INFO,
                   "%s:Failed to enable ARPOFFLOAD Feature %d",
                   __func__, vos_status);
        }
    }

    /**The get power cmd from the supplicant gets updated by the nl only
     *on successful execution of the function call
     *we are oppositely mapped w.r.t mode in the driver
     **/
    vos_status =  wlan_hdd_enter_bmps(pAdapter, !mode);

    EXIT();
    if (VOS_STATUS_E_FAILURE == vos_status)
    {
         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: failed to enter bmps mode", __func__);
        return -EINVAL;
    }
    return 0;
}

static int wlan_hdd_cfg80211_set_power_mgmt(struct wiphy *wiphy,
                     struct net_device *dev, bool mode, int timeout)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_set_power_mgmt(wiphy, dev, mode, timeout);
    vos_ssr_unprotect(__func__);

    return ret;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int wlan_hdd_set_default_mgmt_key(struct wiphy *wiphy,
                         struct net_device *netdev,
                         u8 key_index)
{
    ENTER();
    return 0;
}
#endif //LINUX_VERSION_CODE

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
static int wlan_hdd_set_txq_params(struct wiphy *wiphy,
                   struct net_device *dev,
                   struct ieee80211_txq_params *params)
{
    ENTER();
    return 0;
}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
static int wlan_hdd_set_txq_params(struct wiphy *wiphy,
                   struct ieee80211_txq_params *params)
{
    ENTER();
    return 0;
}
#endif //LINUX_VERSION_CODE

static int __wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
                                       struct net_device *dev,
                                       struct tagCsrDelStaParams *pDelStaParams)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    VOS_STATUS vos_status;
    int status;
    v_U8_t staId;

    ENTER();

    if ( NULL == pAdapter )
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid Adapter" ,__func__);
        return -EINVAL;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_DEL_STA,
                     pAdapter->sessionId, pAdapter->device_mode));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if ( (WLAN_HDD_SOFTAP == pAdapter->device_mode)
       || (WLAN_HDD_P2P_GO == pAdapter->device_mode)
       )
    {
        if (vos_is_macaddr_broadcast((v_MACADDR_t *)pDelStaParams->peerMacAddr))
        {
            v_U16_t i;
            for(i = 0; i < WLAN_MAX_STA_COUNT; i++)
            {
                if ((pAdapter->aStaInfo[i].isUsed) &&
                    (!pAdapter->aStaInfo[i].isDeauthInProgress))
                {
                    vos_mem_copy(pDelStaParams->peerMacAddr,
                                 pAdapter->aStaInfo[i].macAddrSTA.bytes,
                                 ETHER_ADDR_LEN);

                    hddLog(VOS_TRACE_LEVEL_INFO,
                           "%s: Delete STA with MAC::"
                            MAC_ADDRESS_STR,
                            __func__,
                            MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));
                    vos_status = hdd_softap_sta_deauth(pAdapter, pDelStaParams);
                    if (VOS_IS_STATUS_SUCCESS(vos_status))
                        pAdapter->aStaInfo[i].isDeauthInProgress = TRUE;
                }
            }
        }
        else
        {

            vos_status = hdd_softap_GetStaId(pAdapter,
                            (v_MACADDR_t *)pDelStaParams->peerMacAddr, &staId);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
            {
                hddLog(VOS_TRACE_LEVEL_INFO,
                       "%s: Skip this DEL STA as this is not used::"
                       MAC_ADDRESS_STR,
                       __func__, MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));
                return -ENOENT;
            }

            if( pAdapter->aStaInfo[staId].isDeauthInProgress == TRUE)
            {
                hddLog(VOS_TRACE_LEVEL_INFO,
                       "%s: Skip this DEL STA as deauth is in progress::"
                       MAC_ADDRESS_STR,
                       __func__, MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));
                return -ENOENT;
            }

            pAdapter->aStaInfo[staId].isDeauthInProgress = TRUE;

            hddLog(VOS_TRACE_LEVEL_INFO,
                                "%s: Delete STA with MAC::"
                                MAC_ADDRESS_STR,
                                __func__,
                                MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));

            vos_status = hdd_softap_sta_deauth(pAdapter, pDelStaParams);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
            {
                pAdapter->aStaInfo[staId].isDeauthInProgress = FALSE;
                hddLog(VOS_TRACE_LEVEL_INFO,
                                "%s: STA removal failed for ::"
                                MAC_ADDRESS_STR,
                                __func__,
                                MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));
                return -ENOENT;
            }

        }
    }

    EXIT();

    return 0;
}

#ifdef CFG80211_DEL_STA_V2
static int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         struct station_del_parameters *param)
#else
static int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
                                         struct net_device *dev, u8 *mac)
#endif
{
    int ret;
    struct tagCsrDelStaParams delStaParams;

    vos_ssr_protect(__func__);

#ifdef CFG80211_DEL_STA_V2
    if (NULL == param) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid argumet passed", __func__);
        return -EINVAL;
    }

    WLANSAP_PopulateDelStaParams(param->mac, param->reason_code,
                                 param->subtype, &delStaParams);

#else
    WLANSAP_PopulateDelStaParams(mac, eCsrForcedDeauthSta,
                                 (SIR_MAC_MGMT_DEAUTH >> 4), &delStaParams);
#endif
    ret = __wlan_hdd_cfg80211_del_station(wiphy, dev, &delStaParams);

    vos_ssr_unprotect(__func__);

    return ret;
}

static int __wlan_hdd_cfg80211_add_station(struct wiphy *wiphy,
          struct net_device *dev, u8 *mac, struct station_parameters *params)
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    int status = -EPERM;
#ifdef FEATURE_WLAN_TDLS
    u32 mask, set;
    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_ADD_STA,
                     pAdapter->sessionId, params->listen_interval));
    mask = params->sta_flags_mask;

    set = params->sta_flags_set;

#ifdef WLAN_FEATURE_TDLS_DEBUG
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: mask 0x%x set 0x%x " MAC_ADDRESS_STR,
               __func__, mask, set, MAC_ADDR_ARRAY(mac));
#endif

    if (mask & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
        if (set & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
            status = wlan_hdd_tdls_add_station(wiphy, dev, mac, 0, NULL);
        }
    }
#endif
    return status;
}

static int wlan_hdd_cfg80211_add_station(struct wiphy *wiphy,
          struct net_device *dev, u8 *mac, struct station_parameters *params)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_add_station(wiphy, dev, mac, params);
    vos_ssr_unprotect(__func__);

    return ret;
}
#ifdef FEATURE_WLAN_LFR

static int __wlan_hdd_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *dev,
            struct cfg80211_pmksa *pmksa)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle halHandle;
    eHalStatus result;
    int status;
    hdd_context_t *pHddCtx;
    tPmkidCacheInfo pmk_id;

    // Validate pAdapter
    if ( NULL == pAdapter )
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid Adapter" ,__func__);
        return -EINVAL;
    }

    if (!pmksa) {
        hddLog(LOGE, FL("pmksa is NULL"));
        return -EINVAL;
    }

    if (!pmksa->bssid || !pmksa->pmkid) {
       hddLog(LOGE, FL("pmksa->bssid(%p) or pmksa->pmkid(%p) is NULL"),
              pmksa->bssid, pmksa->pmkid);
       return -EINVAL;
    }

    hddLog(VOS_TRACE_LEVEL_DEBUG, "%s: set PMKSA for " MAC_ADDRESS_STR,
           __func__, MAC_ADDR_ARRAY(pmksa->bssid));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    // Retrieve halHandle
    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

    vos_mem_copy(pmk_id.BSSID, pmksa->bssid, ETHER_ADDR_LEN);
    vos_mem_copy(pmk_id.PMKID, pmksa->pmkid, CSR_RSN_PMKID_SIZE);

    /* Add to the PMKSA ID Cache in CSR */
    result = sme_RoamSetPMKIDCache(halHandle,pAdapter->sessionId,
                                   &pmk_id, 1, FALSE);

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_PMKSA,
                     pAdapter->sessionId, result));

    return HAL_STATUS_SUCCESS(result) ? 0 : -EINVAL;
}

static int wlan_hdd_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *dev,
            struct cfg80211_pmksa *pmksa)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __wlan_hdd_cfg80211_set_pmksa(wiphy, dev, pmksa);
   vos_ssr_unprotect(__func__);

   return ret;
}


static int __wlan_hdd_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *dev,
             struct cfg80211_pmksa *pmksa)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle halHandle;
    hdd_context_t *pHddCtx;
    int status = 0;

    /* Validate pAdapter */
    if (NULL == pAdapter)
    {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid Adapter" ,__func__);
       return -EINVAL;
    }

    if (!pmksa) {
        hddLog(LOGE, FL("pmksa is NULL"));
        return -EINVAL;
    }

    if (!pmksa->bssid) {
       hddLog(LOGE, FL("pmksa->bssid is NULL"));
       return -EINVAL;
    }

    hddLog(VOS_TRACE_LEVEL_DEBUG, "%s: deleting PMKSA for " MAC_ADDRESS_STR,
           __func__, MAC_ADDR_ARRAY(pmksa->bssid));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    /*Retrieve halHandle*/
    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

    /* Delete the PMKID CSR cache */
    if (eHAL_STATUS_SUCCESS !=
        sme_RoamDelPMKIDfromCache(halHandle,
                                  pAdapter->sessionId, pmksa->bssid, FALSE)) {
        hddLog(LOGE, FL("Failed to delete PMKSA for "MAC_ADDRESS_STR),
                     MAC_ADDR_ARRAY(pmksa->bssid));
        status = -EINVAL;
    }

   return status;
}


static int wlan_hdd_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *dev,
             struct cfg80211_pmksa *pmksa)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_del_pmksa(wiphy, dev, pmksa);
    vos_ssr_unprotect(__func__);

    return ret;

}

static int __wlan_hdd_cfg80211_flush_pmksa(struct wiphy *wiphy, struct net_device *dev)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle halHandle;
    hdd_context_t *pHddCtx;
    int status = 0;

    hddLog(VOS_TRACE_LEVEL_DEBUG, "%s: flushing PMKSA ",__func__);

    /* Validate pAdapter */
    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Invalid Adapter" ,__func__);
       return -EINVAL;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: HDD context is not valid", __func__);
       return status;
    }

    /*Retrieve halHandle*/
    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

    /* Flush the PMKID cache in CSR */
    if (eHAL_STATUS_SUCCESS !=
        sme_RoamDelPMKIDfromCache(halHandle, pAdapter->sessionId, NULL, TRUE)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Cannot flush PMKIDCache"));
        status = -EINVAL;
    }

    return status;
}

static int wlan_hdd_cfg80211_flush_pmksa(struct wiphy *wiphy, struct net_device *dev)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_flush_pmksa(wiphy, dev);
    vos_ssr_unprotect(__func__);

    return ret;
}
#endif

#if defined(WLAN_FEATURE_VOWIFI_11R) && defined(KERNEL_SUPPORT_11R_CFG80211)
static int __wlan_hdd_cfg80211_update_ft_ies(struct wiphy *wiphy,
                                             struct net_device *dev,
                                             struct cfg80211_update_ft_ies_params *ftie)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx;

    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Adapter is NULL", __func__);
        return -ENODEV;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_UPDATE_FT_IES,
                     pAdapter->sessionId, pHddStaCtx->conn_info.connState));
    // Added for debug on reception of Re-assoc Req.
    if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
    {
        hddLog(LOGE, FL("Called with Ie of length = %zu when not associated"),
               ftie->ie_len);
        hddLog(LOGE, FL("Should be Re-assoc Req IEs"));
    }

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    hddLog(LOGE, FL("%s called with Ie of length = %zu"), __func__,
           ftie->ie_len);
#endif

    // Pass the received FT IEs to SME
    sme_SetFTIEs( WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId,
                  (const u8 *)ftie->ie,
                  ftie->ie_len);
    return 0;
}

static int wlan_hdd_cfg80211_update_ft_ies(struct wiphy *wiphy,
                                           struct net_device *dev,
                                           struct cfg80211_update_ft_ies_params *ftie)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_update_ft_ies(wiphy, dev, ftie);
    vos_ssr_unprotect(__func__);

    return ret;
}
#endif

#ifdef FEATURE_WLAN_SCAN_PNO

void hdd_cfg80211_sched_scan_done_callback(void *callbackContext,
                              tSirPrefNetworkFoundInd *pPrefNetworkFoundInd)
{
    int ret;
    hdd_adapter_t* pAdapter = (hdd_adapter_t*)callbackContext;
    hdd_context_t *pHddCtx;

    ENTER();

    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD adapter is Null", __func__);
        return ;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (NULL == pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HDD context is Null!!!", __func__);
        return ;
    }

    spin_lock(&pHddCtx->schedScan_lock);
    if (TRUE == pHddCtx->isWiphySuspended)
    {
        pHddCtx->isSchedScanUpdatePending = TRUE;
        spin_unlock(&pHddCtx->schedScan_lock);
        hddLog(VOS_TRACE_LEVEL_INFO,
               "%s: Update cfg80211 scan database after it resume", __func__);
        return ;
    }
    spin_unlock(&pHddCtx->schedScan_lock);

    ret = wlan_hdd_cfg80211_update_bss(pHddCtx->wiphy, pAdapter);

    if (0 > ret)
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: NO SCAN result", __func__);

    cfg80211_sched_scan_results(pHddCtx->wiphy);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s: cfg80211 scan result database updated", __func__);
}

/*
 * FUNCTION: wlan_hdd_is_pno_allowed
 * Disallow pno if any session is active
 */
static eHalStatus wlan_hdd_is_pno_allowed(hdd_adapter_t *pAdapter)
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pTempAdapter = NULL;
   hdd_station_ctx_t *pStaCtx;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   int status = 0;
   status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);

   /* The current firmware design does not allow PNO during any
    * active sessions. Hence, determine the active sessions
    * and return a failure.
    */
   while ((NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status))
   {
        pTempAdapter = pAdapterNode->pAdapter;
        pStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pTempAdapter);

        if (((WLAN_HDD_INFRA_STATION == pTempAdapter->device_mode)
          && (eConnectionState_NotConnected != pStaCtx->conn_info.connState))
          || (WLAN_HDD_P2P_CLIENT == pTempAdapter->device_mode)
          || (WLAN_HDD_P2P_GO == pTempAdapter->device_mode)
          || (WLAN_HDD_SOFTAP == pTempAdapter->device_mode)
          || (WLAN_HDD_TM_LEVEL_4 == pHddCtx->tmInfo.currentTmLevel)
          )
        {
            return eHAL_STATUS_FAILURE;
        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
   }
   return eHAL_STATUS_SUCCESS;
}

void hdd_cfg80211_sched_scan_start_status_cb(void *callbackContext, VOS_STATUS status)
{
    hdd_adapter_t *pAdapter = callbackContext;
    hdd_context_t *pHddCtx;

    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("Invalid adapter or adapter has invalid magic"));
        return;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (0 != wlan_hdd_validate_context(pHddCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("HDD context is not valid"));
        return;
    }

    if (VOS_STATUS_SUCCESS != status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              FL("PNO enable response status = %d"), status);
        pHddCtx->isPnoEnable = FALSE;
    }

    pAdapter->pno_req_status = (status == VOS_STATUS_SUCCESS) ? 0 : -EBUSY;
    complete(&pAdapter->pno_comp_var);
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_sched_scan_start
 * Function to enable PNO
 */
static int __wlan_hdd_cfg80211_sched_scan_start(struct wiphy *wiphy,
          struct net_device *dev, struct cfg80211_sched_scan_request *request)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tpSirPNOScanReq pPnoRequest = NULL;
    hdd_context_t *pHddCtx;
    tHalHandle hHal;
    v_U32_t i, indx, num_ch, tempInterval, j;
    u8 valid_ch[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
    u8 channels_allowed[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
    v_U32_t num_channels_allowed = WNI_CFG_VALID_CHANNEL_LIST_LEN;
    eHalStatus status = eHAL_STATUS_FAILURE;
    int ret = 0;
    hdd_config_t *pConfig = NULL;
    v_U32_t num_ignore_dfs_ch = 0;

    if (NULL == pAdapter)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);

    if (0 != ret)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return -EINVAL;
    }

    pConfig = pHddCtx->cfg_ini;
    hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    if (NULL == hHal)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HAL context  is Null!!!", __func__);
        return -EINVAL;
    }
    sme_ScanFlushResult(hHal, pAdapter->sessionId);
    ret = wlan_hdd_scan_abort(pAdapter);
    if (ret < 0)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: aborting the existing scan is unsuccessfull", __func__);
        return -EBUSY;
    }

    if (eHAL_STATUS_SUCCESS != wlan_hdd_is_pno_allowed(pAdapter))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  FL("Cannot handle sched_scan"));
        return -EBUSY;
    }

    if (TRUE == pHddCtx->isPnoEnable)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  FL("already PNO is enabled"));
       return -EBUSY;
    }

    if (VOS_STATUS_SUCCESS != wlan_hdd_cancel_remain_on_channel(pHddCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: abort ROC failed ", __func__);
        return -EBUSY;
    }

    pHddCtx->isPnoEnable = TRUE;

    pPnoRequest = (tpSirPNOScanReq) vos_mem_malloc(sizeof (tSirPNOScanReq));
    if (NULL == pPnoRequest)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: vos_mem_malloc failed", __func__);
        pHddCtx->isPnoEnable = FALSE;
        return -ENOMEM;
    }

    memset(pPnoRequest, 0, sizeof (tSirPNOScanReq));
    pPnoRequest->enable = 1; /*Enable PNO */
    pPnoRequest->ucNetworksCount = request->n_match_sets;

    if (( !pPnoRequest->ucNetworksCount ) ||
        ( pPnoRequest->ucNetworksCount > SIR_PNO_MAX_SUPP_NETWORKS ))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Network input is not correct %d Max Network supported is %d",
                   __func__, pPnoRequest->ucNetworksCount,
                   SIR_PNO_MAX_SUPP_NETWORKS);
        ret = -EINVAL;
        goto error;
    }

    if ( SIR_PNO_MAX_NETW_CHANNELS_EX < request->n_channels )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Incorrect number of channels %d",
                            __func__, request->n_channels);
        ret = -EINVAL;
        goto error;
    }

    /* Framework provides one set of channels(all)
     * common for all saved profile */
    if (0 != ccmCfgGetStr(hHal, WNI_CFG_VALID_CHANNEL_LIST,
            channels_allowed, &num_channels_allowed))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: failed to get valid channel list", __func__);
        ret = -EINVAL;
        goto error;
    }
    /* Checking each channel against allowed channel list */
    num_ch = 0;
    if (request->n_channels)
    {
        char chList [(request->n_channels*5)+1];
        int len;
        for (i = 0, len = 0; i < request->n_channels; i++)
        {
            for (indx = 0; indx < num_channels_allowed; indx++)
            {
                if (request->channels[i]->hw_value == channels_allowed[indx])
                {
                    if ((!pConfig->enableDFSPnoChnlScan) &&
                      (NV_CHANNEL_DFS == vos_nv_getChannelEnabledState(channels_allowed[indx])))
                    {
                        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                        "%s : Dropping DFS channel : %d",
                        __func__,channels_allowed[indx]);
                        num_ignore_dfs_ch++;
                        break;
                    }

                    valid_ch[num_ch++] = request->channels[i]->hw_value;
                    len += snprintf(chList+len, 5, "%d ",
                                         request->channels[i]->hw_value);
                    break ;
                 }
            }
        }
        hddLog(VOS_TRACE_LEVEL_INFO,"Channel-List:  %s ", chList);

        /*If all channels are DFS and dropped, then ignore the PNO request*/
        if (num_ignore_dfs_ch == request->n_channels)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "%s : All requested channels are DFS channels", __func__);
            ret = -EINVAL;
            goto error;
        }
     }
    /* Filling per profile  params */
    for (i = 0; i < pPnoRequest->ucNetworksCount; i++)
    {
        pPnoRequest->aNetworks[i].ssId.length =
               request->match_sets[i].ssid.ssid_len;

        if (( 0 == pPnoRequest->aNetworks[i].ssId.length ) ||
            ( pPnoRequest->aNetworks[i].ssId.length > 32 ) )
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: SSID Len %d is not correct for network %d",
                      __func__, pPnoRequest->aNetworks[i].ssId.length, i);
            ret = -EINVAL;
            goto error;
        }

        memcpy(pPnoRequest->aNetworks[i].ssId.ssId,
               request->match_sets[i].ssid.ssid,
               request->match_sets[i].ssid.ssid_len);
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  "%s: SSID of network %d is %s ",  __func__,
                   i, pPnoRequest->aNetworks[i].ssId.ssId);
        pPnoRequest->aNetworks[i].authentication = 0; /*eAUTH_TYPE_ANY*/
        pPnoRequest->aNetworks[i].encryption     = 0; /*eED_ANY*/
        pPnoRequest->aNetworks[i].bcastNetwType  = 0; /*eBCAST_UNKNOWN*/

        /*Copying list of valid channel into request */
        memcpy(pPnoRequest->aNetworks[i].aChannels, valid_ch, num_ch);
        pPnoRequest->aNetworks[i].ucChannelCount = num_ch;

        pPnoRequest->aNetworks[i].rssiThreshold = 0; //Default value
    }

    for (i = 0; i < request->n_ssids; i++)
    {
        j = 0;
        while (j < pPnoRequest->ucNetworksCount)
        {
            if ((pPnoRequest->aNetworks[j].ssId.length ==
                 request->ssids[i].ssid_len) &&
                 (0 == memcmp(pPnoRequest->aNetworks[j].ssId.ssId,
                            request->ssids[i].ssid,
                            pPnoRequest->aNetworks[j].ssId.length)))
            {
                pPnoRequest->aNetworks[j].bcastNetwType = eBCAST_HIDDEN;
                break;
            }
            j++;
        }
    }
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "Number of hidden networks being Configured = %d",
              request->n_ssids);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "request->ie_len = %zu", request->ie_len);
    if ((0 < request->ie_len) && (NULL != request->ie))
    {
        pPnoRequest->us24GProbeTemplateLen = request->ie_len;
        memcpy(&pPnoRequest->p24GProbeTemplate, request->ie,
                pPnoRequest->us24GProbeTemplateLen);

        pPnoRequest->us5GProbeTemplateLen = request->ie_len;
        memcpy(&pPnoRequest->p5GProbeTemplate, request->ie,
                pPnoRequest->us5GProbeTemplateLen);
    }

    /* Driver gets only one time interval which is hardcoded in
     * supplicant for 10000ms. Taking power consumption into account 6 timers
     * will be used, Timervalue is increased exponentially i.e 10,20,40,
     * 80,160,320 secs. And number of scan cycle for each timer
     * is configurable through INI param gPNOScanTimerRepeatValue.
     * If it is set to 0 only one timer will be used and PNO scan cycle
     * will be repeated after each interval specified by supplicant
     * till PNO is disabled.
     */
    if (0 == pHddCtx->cfg_ini->configPNOScanTimerRepeatValue)
        pPnoRequest->scanTimers.ucScanTimersCount = HDD_PNO_SCAN_TIMERS_SET_ONE;
    else
        pPnoRequest->scanTimers.ucScanTimersCount =
                                               HDD_PNO_SCAN_TIMERS_SET_MULTIPLE;

    tempInterval = (request->interval)/1000;
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "Base scan interval = %d PNOScanTimerRepeatValue = %d",
              tempInterval, pHddCtx->cfg_ini->configPNOScanTimerRepeatValue);
    for ( i = 0; i < pPnoRequest->scanTimers.ucScanTimersCount; i++)
    {
        pPnoRequest->scanTimers.aTimerValues[i].uTimerRepeat =
                                 pHddCtx->cfg_ini->configPNOScanTimerRepeatValue;
        pPnoRequest->scanTimers.aTimerValues[i].uTimerValue = tempInterval;
        tempInterval *= 2;
    }
    //Repeat last timer until pno disabled.
    pPnoRequest->scanTimers.aTimerValues[i-1].uTimerRepeat = 0;

    pPnoRequest->modePNO = SIR_PNO_MODE_IMMEDIATE;

    INIT_COMPLETION(pAdapter->pno_comp_var);
    pPnoRequest->statusCallback = hdd_cfg80211_sched_scan_start_status_cb;
    pPnoRequest->callbackContext = pAdapter;
    pAdapter->pno_req_status = 0;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "SessionId %d, enable %d, modePNO %d, ucScanTimersCount %d",
              pAdapter->sessionId, pPnoRequest->enable, pPnoRequest->modePNO,
                                pPnoRequest->scanTimers.ucScanTimersCount);

    status = sme_SetPreferredNetworkList(WLAN_HDD_GET_HAL_CTX(pAdapter),
                              pPnoRequest, pAdapter->sessionId,
                              hdd_cfg80211_sched_scan_done_callback, pAdapter);
    if (eHAL_STATUS_SUCCESS != status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to enable PNO", __func__);
        ret = -EINVAL;
        goto error;
    }

    ret = wait_for_completion_timeout(
                 &pAdapter->pno_comp_var,
                  msecs_to_jiffies(WLAN_WAIT_TIME_PNO));
    if (0 >= ret)
    {
        // Did not receive the response for PNO enable in time.
        // Assuming the PNO enable was success.
        // Returning error from here, because we timeout, results
        // in side effect of Wifi (Wifi Setting) not to work.
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("Timed out waiting for PNO to be Enabled"));
        ret = 0;
        goto error;
    }

    vos_mem_free(pPnoRequest);
    ret = pAdapter->pno_req_status;
    return ret;

error:
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              FL("PNO scanRequest offloaded ret = %d"), ret);
    vos_mem_free(pPnoRequest);
    pHddCtx->isPnoEnable = FALSE;
    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_sched_scan_start
 * NL interface to enable PNO
 */
static int wlan_hdd_cfg80211_sched_scan_start(struct wiphy *wiphy,
          struct net_device *dev, struct cfg80211_sched_scan_request *request)
{
     int ret;

     vos_ssr_protect(__func__);
     ret = __wlan_hdd_cfg80211_sched_scan_start(wiphy, dev, request);
     vos_ssr_unprotect(__func__);

     return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_sched_scan_stop
 * Function to disable PNO
 */
static int __wlan_hdd_cfg80211_sched_scan_stop(struct wiphy *wiphy,
          struct net_device *dev)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    tHalHandle hHal;
    tpSirPNOScanReq pPnoRequest = NULL;
    int ret = 0;

    ENTER();

    if (NULL == pAdapter)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if (NULL == pHddCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HDD context is Null", __func__);
        return -ENODEV;
    }

    /* The return 0 is intentional when isLogpInProgress and
     * isLoadUnloadInProgress. We did observe a crash due to a return of
     * failure in sched_scan_stop , especially for a case where the unload
     * of the happens at the same time. The function __cfg80211_stop_sched_scan
     * was clearing rdev->sched_scan_req only when the sched_scan_stop returns
     * success. If it returns a failure , then its next invocation due to the
     * clean up of the second interface will have the dev pointer corresponding
     * to the first one leading to a crash.
     */
    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: LOGP in Progress. Ignore!!!", __func__);
        return ret;
    }

    if (WLAN_HDD_IS_LOAD_UNLOAD_IN_PROGRESS(pHddCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Unloading/Loading in Progress. Ignore!!!", __func__);
        return ret;
    }

    hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    if (NULL == hHal)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HAL context  is Null!!!", __func__);
        return -EINVAL;
    }

    pPnoRequest = (tpSirPNOScanReq) vos_mem_malloc(sizeof (tSirPNOScanReq));
    if (NULL == pPnoRequest)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: vos_mem_malloc failed", __func__);
        return -ENOMEM;
    }

    memset(pPnoRequest, 0, sizeof (tSirPNOScanReq));
    pPnoRequest->enable = 0; /* Disable PNO */
    pPnoRequest->ucNetworksCount = 0;

    status = sme_SetPreferredNetworkList(hHal, pPnoRequest,
                                pAdapter->sessionId,
                                NULL, pAdapter);
    if (eHAL_STATUS_SUCCESS != status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "Failed to disabled PNO");
        ret = -EINVAL;
        goto error;
    }
    pHddCtx->isPnoEnable = FALSE;

error:
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   FL("PNO scan disabled ret = %d"), ret);
    vos_mem_free(pPnoRequest);

    EXIT();
    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_sched_scan_stop
 * NL interface to disable PNO
 */
static int wlan_hdd_cfg80211_sched_scan_stop(struct wiphy *wiphy,
          struct net_device *dev)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_sched_scan_stop(wiphy, dev);
    vos_ssr_unprotect(__func__);

    return ret;
}
#endif /*FEATURE_WLAN_SCAN_PNO*/


#ifdef FEATURE_WLAN_TDLS
#if TDLS_MGMT_VERSION2
static int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
                     u8 *peer, u8 action_code,  u8 dialog_token,
                      u16 status_code, u32 peer_capability, const u8 *buf, size_t len)
#else
static int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
                     u8 *peer, u8 action_code,  u8 dialog_token,
                     u16 status_code, const u8 *buf, size_t len)
#endif
{

    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    u8 peerMac[6];
    VOS_STATUS status;
    int max_sta_failed = 0;
    int responder;
    long rc;
#if !(TDLS_MGMT_VERSION2)
    u32 peer_capability = 0;
#endif
    tANI_U16 numCurrTdlsPeers;

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_TDLS_MGMT,
                     pAdapter->sessionId, action_code));
    if (NULL == pHddCtx || NULL == pHddCtx->cfg_ini)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid arguments");
        return -EINVAL;
    }

    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s:LOGP in Progress. Ignore!!!", __func__);
        wlan_hdd_tdls_set_link_status(pAdapter,
                                      peer,
                                      eTDLS_LINK_IDLE,
                                      eTDLS_LINK_UNSPECIFIED);
        return -EBUSY;
    }

    if (eTDLS_SUPPORT_NOT_ENABLED == pHddCtx->tdls_mode)
    {
         VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: TDLS mode is disabled OR not enabled in FW."
                    MAC_ADDRESS_STR " action %d declined.",
                    __func__, MAC_ADDR_ARRAY(peer), action_code);
        return -ENOTSUPP;
    }

    /* other than teardown frame, other mgmt frames are not sent if disabled */
    if (SIR_MAC_TDLS_TEARDOWN != action_code)
    {
       /* if tdls_mode is disabled to respond to peer's request */
        if (eTDLS_SUPPORT_DISABLED == pHddCtx->tdls_mode)
        {
             VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                        "%s: " MAC_ADDRESS_STR
                        " TDLS mode is disabled. action %d declined.",
                        __func__, MAC_ADDR_ARRAY(peer), action_code);

        return -ENOTSUPP;
        }

        if (vos_max_concurrent_connections_reached())
        {
            hddLog(VOS_TRACE_LEVEL_INFO, FL("Reached max concurrent connections"));
            return -EINVAL;
        }
    }

    if (WLAN_IS_TDLS_SETUP_ACTION(action_code))
    {
        if (NULL != wlan_hdd_tdls_is_progress(pHddCtx, peer, TRUE))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: " MAC_ADDRESS_STR
                       " TDLS setup is ongoing. action %d declined.",
                       __func__, MAC_ADDR_ARRAY(peer), action_code);
            return -EPERM;
        }
    }

    if (SIR_MAC_TDLS_SETUP_REQ == action_code ||
        SIR_MAC_TDLS_SETUP_RSP == action_code )
    {
        numCurrTdlsPeers = wlan_hdd_tdlsConnectedPeers(pAdapter);
        if (HDD_MAX_NUM_TDLS_STA <= numCurrTdlsPeers)
        {
            /* supplicant still sends tdls_mgmt(SETUP_REQ) even after
               we return error code at 'add_station()'. Hence we have this
               check again in addtion to add_station().
               Anyway, there is no hard to double-check. */
            if (SIR_MAC_TDLS_SETUP_REQ == action_code)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: " MAC_ADDRESS_STR
                           " TDLS Max peer already connected. action (%d) declined. Num of peers (%d), Max allowed (%d).",
                           __func__, MAC_ADDR_ARRAY(peer), action_code,
                           numCurrTdlsPeers, HDD_MAX_NUM_TDLS_STA);
                return -EINVAL;
            }
            else
            {
                /* maximum reached. tweak to send error code to peer and return
                   error code to supplicant */
                status_code = eSIR_MAC_UNSPEC_FAILURE_STATUS;
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: " MAC_ADDRESS_STR
                           " TDLS Max peer already connected, send response status (%d). Num of peers (%d), Max allowed (%d).",
                           __func__, MAC_ADDR_ARRAY(peer), status_code,
                           numCurrTdlsPeers, HDD_MAX_NUM_TDLS_STA);
                max_sta_failed = -EPERM;
                /* fall through to send setup resp with failure status
                code */
            }
        }
        else
        {
            hddTdlsPeer_t *pTdlsPeer;
            pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peer, TRUE);
            if (pTdlsPeer && TDLS_IS_CONNECTED(pTdlsPeer))
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        "%s:" MAC_ADDRESS_STR " already connected. action %d declined.",
                        __func__, MAC_ADDR_ARRAY(peer), action_code);
                return -EPERM;
            }
        }
    }
    vos_mem_copy(peerMac, peer, 6);

#ifdef WLAN_FEATURE_TDLS_DEBUG
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: " MAC_ADDRESS_STR " action %d, dialog_token %d status %d, len = %zu",
               "tdls_mgmt", MAC_ADDR_ARRAY(peer),
               action_code, dialog_token, status_code, len);
#endif

    /*Except teardown responder will not be used so just make 0*/
    responder = 0;
    if (SIR_MAC_TDLS_TEARDOWN == action_code)
    {

       hddTdlsPeer_t *pTdlsPeer;
       pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peerMac, TRUE);

       if(pTdlsPeer && TDLS_IS_CONNECTED(pTdlsPeer))
            responder = pTdlsPeer->is_responder;
       else
       {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: " MAC_ADDRESS_STR " peer doesn't exist or not connected %d dialog_token %d status %d, len = %zu",
                    __func__, MAC_ADDR_ARRAY(peer), (NULL == pTdlsPeer) ? -1 : pTdlsPeer->link_status,
                     dialog_token, status_code, len);
           return -EPERM;
       }
    }

    /* For explicit trigger of DIS_REQ come out of BMPS for
       successfully receiving DIS_RSP from peer. */
    if ((SIR_MAC_TDLS_SETUP_RSP == action_code) ||
        (SIR_MAC_TDLS_DIS_RSP == action_code) ||
        (SIR_MAC_TDLS_DIS_REQ == action_code))
    {
        if (TRUE == sme_IsPmcBmps(WLAN_HDD_GET_HAL_CTX(pAdapter)))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%s: Sending frame action_code %u.Disable BMPS", __func__, action_code);
            hdd_disable_bmps_imps(pHddCtx, WLAN_HDD_INFRA_STATION);
        }
        if (SIR_MAC_TDLS_DIS_REQ != action_code)
            wlan_hdd_tdls_set_cap(pAdapter, peerMac, eTDLS_CAP_SUPPORTED);
    }

    /* make sure doesn't call send_mgmt() while it is pending */
    if (TDLS_CTX_MAGIC == pAdapter->mgmtTxCompletionStatus)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "%s: " MAC_ADDRESS_STR " action %d couldn't sent, as one is pending. return EBUSY",
            __func__, MAC_ADDR_ARRAY(peer), action_code);
        return -EBUSY;
    }

    pAdapter->mgmtTxCompletionStatus = TDLS_CTX_MAGIC;
    INIT_COMPLETION(pAdapter->tdls_mgmt_comp);

    status = sme_SendTdlsMgmtFrame(WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId,
              peerMac, action_code, dialog_token, status_code, peer_capability, (tANI_U8 *)buf, len, responder);

    if (VOS_STATUS_SUCCESS != status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: sme_SendTdlsMgmtFrame failed!", __func__);
        pAdapter->mgmtTxCompletionStatus = FALSE;
        wlan_hdd_tdls_check_bmps(pAdapter);
        return -EINVAL;
    }

    rc = wait_for_completion_interruptible_timeout(&pAdapter->tdls_mgmt_comp,
                                                        msecs_to_jiffies(WAIT_TIME_TDLS_MGMT));

    if ((rc <= 0) || (TRUE != pAdapter->mgmtTxCompletionStatus))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Mgmt Tx Completion failed status %ld TxCompletion %u",
                  __func__, rc, pAdapter->mgmtTxCompletionStatus);
        pAdapter->mgmtTxCompletionStatus = FALSE;

        if (pHddCtx->isLogpInProgress)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: LOGP in Progress. Ignore!!!", __func__);
            return -EAGAIN;
        }

        wlan_hdd_tdls_check_bmps(pAdapter);
        return -EINVAL;
    }

    if (max_sta_failed)
    {
      wlan_hdd_tdls_check_bmps(pAdapter);
      return max_sta_failed;
    }

    if (SIR_MAC_TDLS_SETUP_RSP == action_code)
    {
        wlan_hdd_tdls_set_responder(pAdapter, peerMac, TRUE);
    }
    else if (SIR_MAC_TDLS_SETUP_CNF == action_code)
    {
        wlan_hdd_tdls_set_responder(pAdapter, peerMac, FALSE);
    }

    return 0;
}


int wlan_hdd_tdls_extctrl_config_peer(hdd_adapter_t *pAdapter,
                                      u8 *peer,
                                      cfg80211_exttdls_callback callback)
{

    hddTdlsPeer_t *pTdlsPeer;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              " %s : NL80211_TDLS_SETUP for " MAC_ADDRESS_STR,
              __func__, MAC_ADDR_ARRAY(peer));

    if ( (FALSE == pHddCtx->cfg_ini->fTDLSExternalControl) ||
         (FALSE == pHddCtx->cfg_ini->fEnableTDLSImplicitTrigger) ) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              " %s TDLS External control and Implicit Trigger not enabled ",
              __func__);
        return -ENOTSUPP;
    }

    /* To cater the requirement of establishing the TDLS link
     * irrespective of the data traffic , get an entry of TDLS peer.
     */
    pTdlsPeer = wlan_hdd_tdls_get_peer(pAdapter, peer);
    if (pTdlsPeer == NULL) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: peer " MAC_ADDRESS_STR " not existing",
                  __func__, MAC_ADDR_ARRAY(peer));
        return -EINVAL;
    }

    if ( 0 != wlan_hdd_tdls_set_force_peer(pAdapter, peer, TRUE) ) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              " %s TDLS Add Force Peer Failed",
              __func__);
        return -EINVAL;
    }
    /*EXT TDLS*/

    if ( 0 != wlan_hdd_set_callback(pTdlsPeer, callback) ) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              " %s TDLS set callback Failed",
              __func__);
        return -EINVAL;
    }

    return(0);

}

int wlan_hdd_tdls_extctrl_deconfig_peer(hdd_adapter_t *pAdapter, u8 *peer)
{

    hddTdlsPeer_t *pTdlsPeer;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              " %s : NL80211_TDLS_TEARDOWN for " MAC_ADDRESS_STR,
              __func__, MAC_ADDR_ARRAY(peer));

    if ( (FALSE == pHddCtx->cfg_ini->fTDLSExternalControl) ||
         (FALSE == pHddCtx->cfg_ini->fEnableTDLSImplicitTrigger) ) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              " %s TDLS External control and Implicit Trigger not enabled ",
              __func__);
        return -ENOTSUPP;
    }


    pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peer, TRUE);

    if ( NULL == pTdlsPeer ) {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: " MAC_ADDRESS_STR
               " peer not exsting",
               __func__, MAC_ADDR_ARRAY(peer));
        return -EINVAL;
    }
    else {
        wlan_hdd_tdls_indicate_teardown(pAdapter, pTdlsPeer,
                           eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON);
    }

    if ( 0 != wlan_hdd_tdls_set_force_peer(pAdapter, peer, FALSE) )
        return -EINVAL;

    /*EXT TDLS*/

    if ( 0 != wlan_hdd_set_callback(pTdlsPeer, NULL )) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              " %s TDLS set callback Failed",
              __func__);
        return -EINVAL;
    }
    return(0);

}
static int __wlan_hdd_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
                 u8 *peer, enum nl80211_tdls_operation oper)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    int status;
    hddTdlsPeer_t *pTdlsPeer;

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_TDLS_OPER,
                     pAdapter->sessionId, oper));
    if ( NULL == peer )
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid arguments", __func__);
        return -EINVAL;
    }

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }


    if( FALSE == pHddCtx->cfg_ini->fEnableTDLSSupport ||
        FALSE == sme_IsFeatureSupportedByFW(TDLS))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "TDLS Disabled in INI OR not enabled in FW. "
                "Cannot process TDLS commands");
        return -ENOTSUPP;
    }

    switch (oper) {
        case NL80211_TDLS_ENABLE_LINK:
            {
                VOS_STATUS status;
                long ret;
                tCsrTdlsLinkEstablishParams tdlsLinkEstablishParams;

                pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peer, TRUE);

                if ( NULL == pTdlsPeer ) {
                    hddLog(VOS_TRACE_LEVEL_ERROR, "%s: " MAC_ADDRESS_STR
                           " (oper %d) not exsting. ignored",
                           __func__, MAC_ADDR_ARRAY(peer), (int)oper);
                    return -EINVAL;
                }

                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: " MAC_ADDRESS_STR " link_status %d (%s) ", "tdls_oper",
                           MAC_ADDR_ARRAY(peer), pTdlsPeer->link_status,
                           "NL80211_TDLS_ENABLE_LINK");

                if (!TDLS_STA_INDEX_VALID(pTdlsPeer->staId))
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid Staion Index %u "
                           MAC_ADDRESS_STR " failed",
                           __func__, pTdlsPeer->staId, MAC_ADDR_ARRAY(peer));
                    return -EINVAL;
                }

                if (eTDLS_LINK_CONNECTED != pTdlsPeer->link_status)
                {
                    if (IS_ADVANCE_TDLS_ENABLE) {

                        if (0 != wlan_hdd_tdls_get_link_establish_params(
                                   pAdapter, peer,&tdlsLinkEstablishParams)) {
                            return -EINVAL;
                        }
                        INIT_COMPLETION(pAdapter->tdls_link_establish_req_comp);

                        sme_SendTdlsLinkEstablishParams(WLAN_HDD_GET_HAL_CTX(pAdapter),
                            pAdapter->sessionId, peer, &tdlsLinkEstablishParams);
                        /* Send TDLS peer UAPSD capabilities to the firmware and
                         * register with the TL on after the response for this operation
                         * is received .
                         */
                        ret = wait_for_completion_interruptible_timeout(
                                &pAdapter->tdls_link_establish_req_comp,
                                msecs_to_jiffies(WAIT_TIME_TDLS_LINK_ESTABLISH_REQ));
                        if (ret <= 0)
                        {
                            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                      "%s: Link Establish Request Faled Status %ld",
                                      __func__, ret);
                            return -EINVAL;
                        }
                    }
                    wlan_hdd_tdls_set_peer_link_status(pTdlsPeer,
                                                       eTDLS_LINK_CONNECTED,
                                                       eTDLS_LINK_SUCCESS);
                    /* Mark TDLS client Authenticated .*/
                    status = WLANTL_ChangeSTAState( pHddCtx->pvosContext,
                                                    pTdlsPeer->staId,
                                                    WLANTL_STA_AUTHENTICATED);
                    if (VOS_STATUS_SUCCESS == status)
                    {
                        if (pTdlsPeer->is_responder == 0)
                        {
                            v_U8_t staId = (v_U8_t)pTdlsPeer->staId;

                            wlan_hdd_tdls_timer_restart(pAdapter,
                                                        &pTdlsPeer->initiatorWaitTimeoutTimer,
                                                       WAIT_TIME_TDLS_INITIATOR);
                            /* suspend initiator TX until it receives direct packet from the
                            reponder or WAIT_TIME_TDLS_INITIATOR timer expires */
                            WLANTL_SuspendDataTx( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                                   &staId, NULL);
                        }
                        wlan_hdd_tdls_increment_peer_count(pAdapter);
                    }
                    wlan_hdd_tdls_check_bmps(pAdapter);

                    /* Update TL about the UAPSD masks , to route the packets to firmware */
                    if ((TRUE == pHddCtx->cfg_ini->fEnableTDLSBufferSta)
                        || pHddCtx->cfg_ini->fTDLSUapsdMask )
                    {
                        int ac;
                        uint8 ucAc[4] = { WLANTL_AC_VO,
                                          WLANTL_AC_VI,
                                          WLANTL_AC_BK,
                                          WLANTL_AC_BE };
                        uint8 tlTid[4] = { 7, 5, 2, 3 } ;
                        for(ac=0; ac < 4; ac++)
                        {
                            status = WLANTL_EnableUAPSDForAC( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                                               pTdlsPeer->staId, ucAc[ac],
                                                               tlTid[ac], tlTid[ac], 0, 0,
                                                               WLANTL_BI_DIR );
                        }
                    }
                }

            }
            break;
        case NL80211_TDLS_DISABLE_LINK:
            {
                pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peer, TRUE);

                if ( NULL == pTdlsPeer ) {
                    hddLog(VOS_TRACE_LEVEL_ERROR, "%s: " MAC_ADDRESS_STR
                           " (oper %d) not exsting. ignored",
                           __func__, MAC_ADDR_ARRAY(peer), (int)oper);
                    return -EINVAL;
                }

                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: " MAC_ADDRESS_STR " link_status %d (%s) ", "tdls_oper",
                           MAC_ADDR_ARRAY(peer), pTdlsPeer->link_status,
                           "NL80211_TDLS_DISABLE_LINK");

                if(TDLS_STA_INDEX_VALID(pTdlsPeer->staId))
                {
                    long status;


                    wlan_hdd_tdls_set_peer_link_status(pTdlsPeer,
                              eTDLS_LINK_TEARING,
                              (pTdlsPeer->link_status == eTDLS_LINK_TEARING)?
                              eTDLS_LINK_UNSPECIFIED:
                              eTDLS_LINK_DROPPED_BY_REMOTE);
                    INIT_COMPLETION(pAdapter->tdls_del_station_comp);

                    sme_DeleteTdlsPeerSta( WLAN_HDD_GET_HAL_CTX(pAdapter),
                            pAdapter->sessionId, peer );

                    status = wait_for_completion_interruptible_timeout(&pAdapter->tdls_del_station_comp,
                              msecs_to_jiffies(WAIT_TIME_TDLS_DEL_STA));
                    wlan_hdd_tdls_set_peer_link_status(pTdlsPeer,
                                                       eTDLS_LINK_IDLE,
                                                       eTDLS_LINK_UNSPECIFIED);
                    if (status <= 0)
                    {
                        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                  "%s: Del station failed status %ld",
                                  __func__, status);
                        return -EPERM;
                    }
                }
                else
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                              "%s: TDLS Peer Station doesn't exist.", __func__);
                }
            }
            break;
        case NL80211_TDLS_TEARDOWN:
            {
                status = wlan_hdd_tdls_extctrl_deconfig_peer(pAdapter, peer);

                if (0 != status)
                {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: Error in TDLS Teardown", __func__);
                    return status;
                }
                break;
            }
        case NL80211_TDLS_SETUP:
            {
                status = wlan_hdd_tdls_extctrl_config_peer(pAdapter,
                                                           peer,
                                                           NULL);

                if (0 != status)
                {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: Error in TDLS Setup", __func__);
                    return status;
                }
                break;
            }
        case NL80211_TDLS_DISCOVERY_REQ:
            /* We don't support in-driver setup/teardown/discovery */
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                 "%s: We don't support in-driver setup/teardown/discovery "
                 ,__func__);
            return -ENOTSUPP;
        default:
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: unsupported event",__func__);
            return -ENOTSUPP;
    }
    return 0;
}

static int wlan_hdd_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
                 u8 *peer, enum nl80211_tdls_operation oper)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_tdls_oper(wiphy, dev, peer, oper);
    vos_ssr_unprotect(__func__);

    return ret;
}

int wlan_hdd_cfg80211_send_tdls_discover_req(struct wiphy *wiphy,
                            struct net_device *dev, u8 *peer)
{
    hddLog(VOS_TRACE_LEVEL_INFO,
           "tdls send discover req: "MAC_ADDRESS_STR,
           MAC_ADDR_ARRAY(peer));

#if TDLS_MGMT_VERSION2
    return wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer,
                            WLAN_TDLS_DISCOVERY_REQUEST, 1, 0, 0, NULL, 0);
#else
    return wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer,
                            WLAN_TDLS_DISCOVERY_REQUEST, 1, 0, NULL, 0);
#endif
}
#endif

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/*
 * FUNCTION: wlan_hdd_cfg80211_update_replayCounterCallback
 * Callback rountine called upon receiving response for
 * get offload info
 */
void wlan_hdd_cfg80211_update_replayCounterCallback(void *callbackContext,
                            tpSirGtkOffloadGetInfoRspParams pGtkOffloadGetInfoRsp)
{

    hdd_adapter_t *pAdapter = (hdd_adapter_t *)callbackContext;
    tANI_U8 tempReplayCounter[8];
    hdd_station_ctx_t *pHddStaCtx;

    ENTER();

    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD adapter is Null", __func__);
        return ;
    }

    if (NULL == pGtkOffloadGetInfoRsp)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: pGtkOffloadGetInfoRsp is Null", __func__);
        return ;
    }

    if (VOS_STATUS_SUCCESS != pGtkOffloadGetInfoRsp->ulStatus)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: wlan Failed to get replay counter value",
                __func__);
        return ;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    /* Update replay counter */
    pHddStaCtx->gtkOffloadReqParams.ullKeyReplayCounter =
                                   pGtkOffloadGetInfoRsp->ullKeyReplayCounter;

    {
        /* changing from little to big endian since supplicant
         * works on big endian format
         */
        int i;
        tANI_U8 *p = (tANI_U8 *)&pGtkOffloadGetInfoRsp->ullKeyReplayCounter;

        for (i = 0; i < 8; i++)
        {
            tempReplayCounter[7-i] = (tANI_U8)p[i];
        }
    }

    /* Update replay counter to NL */
    cfg80211_gtk_rekey_notify(pAdapter->dev, pGtkOffloadGetInfoRsp->bssId,
          tempReplayCounter, GFP_KERNEL);
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_rekey_data
 * This function is used to offload GTK rekeying job to the firmware.
 */
int __wlan_hdd_cfg80211_set_rekey_data(struct wiphy *wiphy, struct net_device *dev,
                                     struct cfg80211_gtk_rekey_data *data)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    hdd_station_ctx_t *pHddStaCtx;
    tHalHandle hHal;
    int result;
    tSirGtkOffloadParams hddGtkOffloadReqParams;
    eHalStatus status = eHAL_STATUS_FAILURE;

    ENTER();


    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_REKEY_DATA,
                     pAdapter->sessionId, pAdapter->device_mode));

    result = wlan_hdd_validate_context(pHddCtx);

    if (0 != result)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return result;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    if (NULL == hHal)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HAL context  is Null!!!", __func__);
        return -EAGAIN;
    }

    pHddStaCtx->gtkOffloadReqParams.ulFlags = GTK_OFFLOAD_ENABLE;
    memcpy(pHddStaCtx->gtkOffloadReqParams.aKCK, data->kck, NL80211_KCK_LEN);
    memcpy(pHddStaCtx->gtkOffloadReqParams.aKEK, data->kek, NL80211_KEK_LEN);
    memcpy(pHddStaCtx->gtkOffloadReqParams.bssId, &pHddStaCtx->conn_info.bssId,
          WNI_CFG_BSSID_LEN);
    {
        /* changing from big to little endian since driver
         * works on little endian format
         */
        tANI_U8 *p =
              (tANI_U8 *)&pHddStaCtx->gtkOffloadReqParams.ullKeyReplayCounter;
        int i;

        for (i = 0; i < 8; i++)
        {
            p[7-i] = data->replay_ctr[i];
        }
    }

    if (TRUE == pHddCtx->hdd_wlan_suspended)
    {
        /* if wlan is suspended, enable GTK offload directly from here */
        memcpy(&hddGtkOffloadReqParams, &pHddStaCtx->gtkOffloadReqParams,
              sizeof (tSirGtkOffloadParams));
        status = sme_SetGTKOffload(hHal, &hddGtkOffloadReqParams,
                       pAdapter->sessionId);

        if (eHAL_STATUS_SUCCESS != status)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: sme_SetGTKOffload failed, returned %d",
                    __func__, status);

           /* Need to clear any trace of key value in the memory.
            * Thus zero out the memory even though it is local
            * variable.
            */
            vos_mem_zero(&hddGtkOffloadReqParams,
                          sizeof(hddGtkOffloadReqParams));
            return status;
        }
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: sme_SetGTKOffload successfull", __func__);
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: wlan not suspended GTKOffload request is stored",
                __func__);
    }

    /* Need to clear any trace of key value in the memory.
     * Thus zero out the memory even though it is local
     * variable.
     */
    vos_mem_zero(&hddGtkOffloadReqParams,
                  sizeof(hddGtkOffloadReqParams));

    return eHAL_STATUS_SUCCESS;
}

int wlan_hdd_cfg80211_set_rekey_data(struct wiphy *wiphy, struct net_device *dev,
                                     struct cfg80211_gtk_rekey_data *data)
{
    int ret;

    vos_ssr_protect(__func__);
    ret =  __wlan_hdd_cfg80211_set_rekey_data(wiphy, dev, data);
    vos_ssr_unprotect(__func__);

    return ret;
}
#endif /*WLAN_FEATURE_GTK_OFFLOAD*/
/*
 * FUNCTION: wlan_hdd_cfg80211_set_mac_acl
 * This function is used to set access control policy
 */
static int wlan_hdd_cfg80211_set_mac_acl(struct wiphy *wiphy,
                struct net_device *dev, const struct cfg80211_acl_data *params)
{
    int i;
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_hostapd_state_t *pHostapdState;
    tsap_Config_t *pConfig;
    v_CONTEXT_t pVosContext = NULL;
    hdd_context_t *pHddCtx;
    int status;

    ENTER();

    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }

    if (NULL == params)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: params is Null", __func__);
        return -EINVAL;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    pVosContext = pHddCtx->pvosContext;
    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

    if (NULL == pHostapdState)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: pHostapdState is Null", __func__);
        return -EINVAL;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"acl policy: = %d"
             "no acl entries = %d", params->acl_policy, params->n_acl_entries);

    if (WLAN_HDD_SOFTAP == pAdapter->device_mode)
    {
        pConfig = &pAdapter->sessionCtx.ap.sapConfig;

        /* default value */
        pConfig->num_accept_mac = 0;
        pConfig->num_deny_mac = 0;

        /**
         * access control policy
         * @NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED: Deny stations which are
         *   listed in hostapd.deny file.
         * @NL80211_ACL_POLICY_DENY_UNLESS_LISTED: Allow stations which are
         *   listed in hostapd.accept file.
         */
        if (NL80211_ACL_POLICY_DENY_UNLESS_LISTED == params->acl_policy)
        {
            pConfig->SapMacaddr_acl = eSAP_DENY_UNLESS_ACCEPTED;
        }
        else if (NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED == params->acl_policy)
        {
            pConfig->SapMacaddr_acl = eSAP_ACCEPT_UNLESS_DENIED;
        }
        else
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s:Acl Policy : %d is not supported",
                                            __func__, params->acl_policy);
            return -ENOTSUPP;
        }

        if (eSAP_DENY_UNLESS_ACCEPTED == pConfig->SapMacaddr_acl)
        {
            pConfig->num_accept_mac = params->n_acl_entries;
            for (i = 0; i < params->n_acl_entries; i++)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "** Add ACL MAC entry %i in WhiletList :"
                                MAC_ADDRESS_STR, i,
                                MAC_ADDR_ARRAY(params->mac_addrs[i].addr));

                vos_mem_copy(&pConfig->accept_mac[i], params->mac_addrs[i].addr,
                                                             sizeof(qcmacaddr));
            }
        }
        else if (eSAP_ACCEPT_UNLESS_DENIED == pConfig->SapMacaddr_acl)
        {
            pConfig->num_deny_mac = params->n_acl_entries;
            for (i = 0; i < params->n_acl_entries; i++)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "** Add ACL MAC entry %i in BlackList :"
                                MAC_ADDRESS_STR, i,
                                MAC_ADDR_ARRAY(params->mac_addrs[i].addr));

                vos_mem_copy(&pConfig->deny_mac[i], params->mac_addrs[i].addr,
                                                           sizeof(qcmacaddr));
            }
        }

        if (VOS_STATUS_SUCCESS != WLANSAP_SetMacACL(pVosContext, pConfig))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: SAP Set Mac Acl fail", __func__);
            return -EINVAL;
        }
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid device_mode = %s (%d)",
                    __func__, hdd_device_modetoString(pAdapter->device_mode),
                                                      pAdapter->device_mode);
        return -EINVAL;
    }

    return 0;
}

#ifdef WLAN_NL80211_TESTMODE
#ifdef FEATURE_WLAN_LPHB
void wlan_hdd_cfg80211_lphb_ind_handler
(
   void *pAdapter,
   void *indCont
)
{
   tSirLPHBInd     *lphbInd;
   struct sk_buff  *skb;
   hdd_context_t  *pHddCtxt;

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "LPHB indication arrived");

   if (pAdapter == NULL)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: pAdapter is NULL\n",__func__);
       return;
   }

   if (NULL == indCont)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "LPHB IND, invalid argument");
      return;
   }

   pHddCtxt  = (hdd_context_t *)pAdapter;
   lphbInd = (tSirLPHBInd *)indCont;
   skb = cfg80211_testmode_alloc_event_skb(
                  pHddCtxt->wiphy,
                  sizeof(tSirLPHBInd),
                  GFP_ATOMIC);
   if (!skb)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "LPHB timeout, NL buffer alloc fail");
      return;
   }

   if(nla_put_u32(skb, WLAN_HDD_TM_ATTR_CMD, WLAN_HDD_TM_CMD_WLAN_HB))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "WLAN_HDD_TM_ATTR_CMD put fail");
      goto nla_put_failure;
   }
   if(nla_put_u32(skb, WLAN_HDD_TM_ATTR_TYPE, lphbInd->protocolType))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "WLAN_HDD_TM_ATTR_TYPE put fail");
      goto nla_put_failure;
   }
   if(nla_put(skb, WLAN_HDD_TM_ATTR_DATA,
           sizeof(tSirLPHBInd), lphbInd))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "WLAN_HDD_TM_ATTR_DATA put fail");
      goto nla_put_failure;
   }
   cfg80211_testmode_event(skb, GFP_ATOMIC);
   return;

nla_put_failure:
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "NLA Put fail");
   kfree_skb(skb);

   return;
}
#endif /* FEATURE_WLAN_LPHB */

static int __wlan_hdd_cfg80211_testmode(struct wiphy *wiphy, void *data, int len)
{
    struct nlattr *tb[WLAN_HDD_TM_ATTR_MAX + 1];
    int err = 0;
#ifdef FEATURE_WLAN_LPHB
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    eHalStatus smeStatus;
#endif /* FEATURE_WLAN_LPHB */

    err = nla_parse(tb, WLAN_HDD_TM_ATTR_MAX, data, len, wlan_hdd_tm_policy);
    if (err)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s Testmode INV ATTR", __func__);
        return err;
    }

    if (!tb[WLAN_HDD_TM_ATTR_CMD])
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s Testmode INV CMD", __func__);
        return -EINVAL;
    }

    switch (nla_get_u32(tb[WLAN_HDD_TM_ATTR_CMD]))
    {
#ifdef FEATURE_WLAN_LPHB
        /* Low Power Heartbeat configuration request */
        case WLAN_HDD_TM_CMD_WLAN_HB:
        {
            int   buf_len;
            void *buf;
            tSirLPHBReq *hb_params = NULL;
            tSirLPHBReq *hb_params_temp = NULL;

            if (!tb[WLAN_HDD_TM_ATTR_DATA])
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          "%s Testmode INV DATA", __func__);
                return -EINVAL;
            }

            buf = nla_data(tb[WLAN_HDD_TM_ATTR_DATA]);
            buf_len = nla_len(tb[WLAN_HDD_TM_ATTR_DATA]);

            hb_params_temp =(tSirLPHBReq *)buf;
            if ((hb_params_temp->cmd == LPHB_SET_TCP_PARAMS_INDID) &&
                (hb_params_temp->params.lphbTcpParamReq.timePeriodSec == 0))
                return -EINVAL;

            hb_params = (tSirLPHBReq *)vos_mem_malloc(sizeof(tSirLPHBReq));
            if (NULL == hb_params)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          "%s Request Buffer Alloc Fail", __func__);
                return -EINVAL;
            }

            vos_mem_copy(hb_params, buf, buf_len);
            smeStatus = sme_LPHBConfigReq((tHalHandle)(pHddCtx->hHal),
                               hb_params,
                               wlan_hdd_cfg80211_lphb_ind_handler);
            if (eHAL_STATUS_SUCCESS != smeStatus)
            {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "LPHB Config Fail, disable");
               vos_mem_free(hb_params);
            }
            return 0;
         }
#endif /* FEATURE_WLAN_LPHB */
        default:
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: unsupported event",__func__);
            return -EOPNOTSUPP;
    }

   return err;
}

static int wlan_hdd_cfg80211_testmode(struct wiphy *wiphy, void *data, int len)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __wlan_hdd_cfg80211_testmode(wiphy, data, len);
   vos_ssr_unprotect(__func__);

   return ret;
}
#endif /* CONFIG_NL80211_TESTMODE */

static int __wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         int idx, struct survey_info *survey)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    hdd_station_ctx_t *pHddStaCtx;
    tHalHandle halHandle;
    v_U32_t channel = 0, freq = 0; /* Initialization Required */
    v_S7_t snr,rssi;
    int status, i, j, filled = 0;

    ENTER();

    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }

    if (NULL == wiphy)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: wiphy is Null", __func__);
        return -ENODEV;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    if (0 == pHddCtx->cfg_ini->fEnableSNRMonitoring ||
        0 != pAdapter->survey_idx ||
        eConnectionState_Associated != pHddStaCtx->conn_info.connState)
    {
        /* The survey dump ops when implemented completely is expected to
         * return a survey of all channels and the ops is called by the
         * kernel with incremental values of the argument 'idx' till it
         * returns -ENONET. But we can only support the survey for the
         * operating channel for now. survey_idx is used to track
         * that the ops is called only once and then return -ENONET for
         * the next iteration
         */
        pAdapter->survey_idx = 0;
        return -ENONET;
    }

    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

    wlan_hdd_get_snr(pAdapter, &snr);
    wlan_hdd_get_rssi(pAdapter, &rssi);

    sme_GetOperationChannel(halHandle, &channel, pAdapter->sessionId);
    hdd_wlan_get_freq(channel, &freq);


    for (i = 0; i < IEEE80211_NUM_BANDS; i++)
    {
        if (NULL == wiphy->bands[i])
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                     "%s: wiphy->bands[i] is NULL, i = %d", __func__, i);
           continue;
        }

        for (j = 0; j < wiphy->bands[i]->n_channels; j++)
        {
            struct ieee80211_supported_band *band = wiphy->bands[i];

            if (band->channels[j].center_freq == (v_U16_t)freq)
            {
                survey->channel = &band->channels[j];
                /* The Rx BDs contain SNR values in dB for the received frames
                 * while the supplicant expects noise. So we calculate and
                 * return the value of noise (dBm)
                 *  SNR (dB) = RSSI (dBm) - NOISE (dBm)
                 */
                survey->noise = rssi - snr;
                survey->filled = SURVEY_INFO_NOISE_DBM;
                filled = 1;
            }
        }
     }

     if (filled)
        pAdapter->survey_idx = 1;
     else
     {
        pAdapter->survey_idx = 0;
        return -ENONET;
     }

     return 0;
}

static int wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         int idx, struct survey_info *survey)
{
     int ret;

     vos_ssr_protect(__func__);
     ret = __wlan_hdd_cfg80211_dump_survey(wiphy, dev, idx, survey);
     vos_ssr_unprotect(__func__);

     return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_resume_wlan
 * this is called when cfg80211 driver resume
 * driver updates  latest sched_scan scan result(if any) to cfg80211 database
 */
int __wlan_hdd_cfg80211_resume_wlan(struct wiphy *wiphy)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    hdd_adapter_t *pAdapter;
    hdd_adapter_list_node_t *pAdapterNode, *pNext;
    VOS_STATUS status = VOS_STATUS_SUCCESS;

    ENTER();

    if ( NULL == pHddCtx )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s: HddCtx validation failed", __func__);
        return 0;
    }

    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: LOGP in Progress. Ignore!!!", __func__);
        return 0;
    }

    if (WLAN_HDD_IS_LOAD_UNLOAD_IN_PROGRESS(pHddCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Unloading/Loading in Progress. Ignore!!!", __func__);
        return 0;
    }

    spin_lock(&pHddCtx->schedScan_lock);
    pHddCtx->isWiphySuspended = FALSE;
    if (TRUE != pHddCtx->isSchedScanUpdatePending)
    {
        spin_unlock(&pHddCtx->schedScan_lock);
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Return resume is not due to PNO indication", __func__);
        return 0;
    }
    // Reset flag to avoid updatating cfg80211 data old results again
    pHddCtx->isSchedScanUpdatePending = FALSE;
    spin_unlock(&pHddCtx->schedScan_lock);


    status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;
        if ( (NULL != pAdapter) &&
             (WLAN_HDD_INFRA_STATION == pAdapter->device_mode) )
        {
            if (0 != wlan_hdd_cfg80211_update_bss(pHddCtx->wiphy, pAdapter))
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      "%s: NO SCAN result", __func__);
            }
            else
            {
                /* Acquire wakelock to handle the case where APP's tries to
                 * suspend immediately after updating the scan results. Whis
                 * results in app's is in suspended state and not able to
                 * process the connect request to AP
                 */
                hdd_prevent_suspend_timeout(2000);
                cfg80211_sched_scan_results(pHddCtx->wiphy);
            }

            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "%s : cfg80211 scan result database updated", __func__);

            return 0;

        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          "%s: Failed to find Adapter", __func__);
    return 0;
}

int wlan_hdd_cfg80211_resume_wlan(struct wiphy *wiphy)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_resume_wlan(wiphy);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_suspend_wlan
 * this is called when cfg80211 driver suspends
 */
int __wlan_hdd_cfg80211_suspend_wlan(struct wiphy *wiphy,
                                   struct cfg80211_wowlan *wow)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);

    ENTER();
    if (NULL == pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s: HddCtx validation failed", __func__);
        return 0;
    }

    pHddCtx->isWiphySuspended = TRUE;

    EXIT();

    return 0;
}

int wlan_hdd_cfg80211_suspend_wlan(struct wiphy *wiphy,
                                   struct cfg80211_wowlan *wow)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_suspend_wlan(wiphy, wow);
    vos_ssr_unprotect(__func__);

    return ret;
}
/* cfg80211_ops */
static struct cfg80211_ops wlan_hdd_cfg80211_ops =
{
    .add_virtual_intf = wlan_hdd_add_virtual_intf,
    .del_virtual_intf = wlan_hdd_del_virtual_intf,
    .change_virtual_intf = wlan_hdd_cfg80211_change_iface,
    .change_station = wlan_hdd_change_station,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
    .add_beacon = wlan_hdd_cfg80211_add_beacon,
    .del_beacon = wlan_hdd_cfg80211_del_beacon,
    .set_beacon = wlan_hdd_cfg80211_set_beacon,
#else
    .start_ap = wlan_hdd_cfg80211_start_ap,
    .change_beacon = wlan_hdd_cfg80211_change_beacon,
    .stop_ap = wlan_hdd_cfg80211_stop_ap,
#endif
    .change_bss = wlan_hdd_cfg80211_change_bss,
    .add_key = wlan_hdd_cfg80211_add_key,
    .get_key = wlan_hdd_cfg80211_get_key,
    .del_key = wlan_hdd_cfg80211_del_key,
    .set_default_key = wlan_hdd_cfg80211_set_default_key,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
    .set_channel = wlan_hdd_cfg80211_set_channel,
#endif
    .scan = wlan_hdd_cfg80211_scan,
    .connect = wlan_hdd_cfg80211_connect,
    .disconnect = wlan_hdd_cfg80211_disconnect,
    .join_ibss  = wlan_hdd_cfg80211_join_ibss,
    .leave_ibss = wlan_hdd_cfg80211_leave_ibss,
    .set_wiphy_params = wlan_hdd_cfg80211_set_wiphy_params,
    .set_tx_power = wlan_hdd_cfg80211_set_txpower,
    .get_tx_power = wlan_hdd_cfg80211_get_txpower,
    .remain_on_channel = wlan_hdd_cfg80211_remain_on_channel,
    .cancel_remain_on_channel =  wlan_hdd_cfg80211_cancel_remain_on_channel,
    .mgmt_tx =  wlan_hdd_mgmt_tx,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
     .mgmt_tx_cancel_wait = wlan_hdd_cfg80211_mgmt_tx_cancel_wait,
     .set_default_mgmt_key = wlan_hdd_set_default_mgmt_key,
     .set_txq_params = wlan_hdd_set_txq_params,
#endif
     .get_station = wlan_hdd_cfg80211_get_station,
     .set_power_mgmt = wlan_hdd_cfg80211_set_power_mgmt,
     .del_station  = wlan_hdd_cfg80211_del_station,
     .add_station  = wlan_hdd_cfg80211_add_station,
#ifdef FEATURE_WLAN_LFR
     .set_pmksa = wlan_hdd_cfg80211_set_pmksa,
     .del_pmksa = wlan_hdd_cfg80211_del_pmksa,
     .flush_pmksa = wlan_hdd_cfg80211_flush_pmksa,
#endif
#if defined(WLAN_FEATURE_VOWIFI_11R) && defined(KERNEL_SUPPORT_11R_CFG80211)
     .update_ft_ies = wlan_hdd_cfg80211_update_ft_ies,
#endif
#ifdef FEATURE_WLAN_TDLS
     .tdls_mgmt = wlan_hdd_cfg80211_tdls_mgmt,
     .tdls_oper = wlan_hdd_cfg80211_tdls_oper,
#endif
#ifdef WLAN_FEATURE_GTK_OFFLOAD
     .set_rekey_data = wlan_hdd_cfg80211_set_rekey_data,
#endif /* WLAN_FEATURE_GTK_OFFLOAD */
#ifdef FEATURE_WLAN_SCAN_PNO
     .sched_scan_start = wlan_hdd_cfg80211_sched_scan_start,
     .sched_scan_stop = wlan_hdd_cfg80211_sched_scan_stop,
#endif /*FEATURE_WLAN_SCAN_PNO */
     .resume = wlan_hdd_cfg80211_resume_wlan,
     .suspend = wlan_hdd_cfg80211_suspend_wlan,
     .set_mac_acl = wlan_hdd_cfg80211_set_mac_acl,
#ifdef WLAN_NL80211_TESTMODE
     .testmode_cmd = wlan_hdd_cfg80211_testmode,
#endif
     .dump_survey = wlan_hdd_cfg80211_dump_survey,
};

