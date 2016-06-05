/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

  \file  wlan_hdd_ftm.c

  \brief This file contains the WLAN factory test mode implementation


  ========================================================================*/

/**=========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  04/20/11    Leo/Henri      Convergence for Prima and Volans. Single image
                             for FTM and mission mode
  04/5/09     Shailender     Created module.

  ==========================================================================*/
#include <vos_mq.h>
#include "vos_sched.h"
#include <vos_api.h>
#include "sirTypes.h"
#include "halTypes.h"
#include "sirApi.h"
#include "sirMacProtDef.h"
#include "sme_Api.h"
#include "macInitApi.h"
#include "wlan_qct_sys.h"
#include "wlan_qct_tl.h"
#include "wlan_hdd_misc.h"
#include "i_vos_packet.h"
#include "vos_nvitem.h"
#include "wlan_hdd_main.h"
#include "qwlan_version.h"

#include "wlan_nv.h"
#include "wlan_qct_wda.h"
#include "cfgApi.h"
#include "pttMsgApi.h"
#include "wlan_qct_pal_device.h"
#include "linux/wcnss_wlan.h"

#define RXMODE_DISABLE_ALL 0
#define RXMODE_ENABLE_ALL  1
#define RXMODE_ENABLE_11GN 2
#define RXMODE_ENABLE_11B  3

#define FTM_CHAIN_SEL_NO_RX_TX      0
#define FTM_CHAIN_SEL_R0_ON         1
#define FTM_CHAIN_SEL_T0_ON         2
#define FTM_CHAIN_SEL_R0_T0_ON      3
#define FTM_CHAIN_SEL_ANTENNA_0     7
#define FTM_CHAIN_SEL_ANTENNA_1     8
#define FTM_CHAIN_SEL_MAX           8

#define WCNSS_TXFIR_OFFSET          0x00018000

#define QWLAN_PHYDBG_TXPKT_CNT_CNT_MASK     0xFFFF

#ifndef QWLAN_AGC_CHANNEL_FREQ_REG_OFFSET
#define QWLAN_AGC_CHANNEL_FREQ_REG_OFFSET   0x00013c34
#define QWLAN_AGC_CHANNEL_FREQ_FREQ_MASK    0x1FFF
#endif /* QWLAN_AGC_CHANNEL_FREQ_REG_OFFSET */


/* To set 4MAC addresses from given first MAC address,
 * Last byte value within given MAC address must less than 0xFF - 3 */
#define QWLAN_MAX_MAC_LAST_BYTE_VALUE       0xFC
#define NV_EMBEDDED_VERSION                 0x80

#define QWLAN_TXFIR_CFG_DPD_BYPASS_MASK     0x8

typedef struct {
   tANI_U32 tableSize;                      /* Whole NV Table Size */
   tANI_U32 chunkSize;                      /* Current Chunk Size < 2K */
   eNvTable nvTable;
   tANI_U8  tableData;                     /* Filled by host driver */
} pttGetNvTable;

typedef struct {
   tANI_U32 tableSize;                      /* Whole NV Table Size */
   tANI_U32 chunkSize;                      /* Current Chunk Size < 2K */
   eNvTable nvTable;
   tANI_U8  tableData;
} pttSetNvTable;


extern const sHalNv nvDefaults;
static int wlan_ftm_register_wext(hdd_adapter_t *pAdapter);
static int wlan_ftm_stop(hdd_context_t *pHddCtx);
VOS_STATUS wlan_write_to_efs (v_U8_t *pData, v_U16_t data_len);

static rateStr2rateIndex_t rateName_rateIndex_tbl[] =
{
   { HAL_PHY_RATE_11B_LONG_1_MBPS,       "11B_LONG_1_MBPS"},
   { HAL_PHY_RATE_11B_LONG_2_MBPS,       "11B_LONG_2_MBPS"},
   { HAL_PHY_RATE_11B_LONG_5_5_MBPS,     "11B_LONG_5_5_MBPS"},
   { HAL_PHY_RATE_11B_LONG_11_MBPS,      "11B_LONG_11_MBPS"},
   { HAL_PHY_RATE_11B_SHORT_2_MBPS,      "11B_SHORT_2_MBPS"},
   { HAL_PHY_RATE_11B_SHORT_5_5_MBPS,    "11B_SHORT_5_5_MBPS"},
   { HAL_PHY_RATE_11B_SHORT_11_MBPS,     "11B_SHORT_11_MBPS"},
   //Spica_Virgo 11A 20MHz Rates
   { HAL_PHY_RATE_11A_6_MBPS,            "11A_6_MBPS"},
   { HAL_PHY_RATE_11A_9_MBPS,            "11A_9_MBPS"},
   { HAL_PHY_RATE_11A_12_MBPS,           "11A_12_MBPS"},
   { HAL_PHY_RATE_11A_18_MBPS,           "11A_18_MBPS"},
   { HAL_PHY_RATE_11A_24_MBPS,           "11A_24_MBPS"},
   { HAL_PHY_RATE_11A_36_MBPS,           "11A_36_MBPS"},
   { HAL_PHY_RATE_11A_48_MBPS,           "11A_48_MBPS"},
   { HAL_PHY_RATE_11A_54_MBPS,           "11A_54_MBPS"},

//MCS Index #0-15 (20MHz)
   { HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,   "MCS_6_5_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_13_MBPS,    "MCS_13_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,  "MCS_19_5_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_26_MBPS,    "MCS_26_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_39_MBPS,    "MCS_39_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_52_MBPS,    "MCS_52_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,  "MCS_58_5_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_65_MBPS,    "MCS_65_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,   "MCS_SG_7_2_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,  "MCS_SG_14_4_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,  "MCS_SG_21_7_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,  "MCS_SG_28_9_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,  "MCS_SG_43_3_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,  "MCS_SG_57_8_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,    "MCS_SG_65_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,  "MCS_SG_72_2_MBPS"},

//MCS Index #8-15 (40MHz)

   { HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS, "MCS_CB_13_5_MBPS" },
   { HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,   "MCS_CB_27_MBPS" },
   { HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS, "MCS_CB_40_5_MBPS" },
   { HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS, "MCS_CB_54_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS, "MCS_CB_81_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS, "MCS_CB_108_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS, "MCS_CB_121_5_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,   "MCS_CB_135_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,  "MCS_CB_15_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,  "MCS_CB_30_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,  "MCS_CB_45_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,  "MCS_CB_60_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,  "MCS_CB_90_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS, "MCS_CB_120_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS, "MCS_CB_135_MBPS"},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS, "MCS_CB_150_MBPS"},

#ifdef WLAN_FEATURE_11AC
    /*11AC rate 20MHZ Normal GI*/
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_6_5_MBPS, "MCS_VHT20_NGI_6_5_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_13_MBPS,  "MCS_VHT20_NGI_13_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_19_5_MBPS,"MCS_VHT20_NGI_19_5_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_26_MBPS,  "MCS_VHT20_NGI_26_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_39_MBPS,  "MCS_VHT20_NGI_39_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_52_MBPS,  "MCS_VHT20_NGI_52_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_58_5_MBPS,"MCS_VHT20_NGI_58_5_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_65_MBPS,  "MCS_VHT20_NGI_65_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_78_MBPS,  "MCS_VHT20_NGI_78_MBPS"},
#ifdef WCN_PRONTO
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,"MCS_VHT20_NGI_86_5_MBPS"},
#endif

    /*11AC rate 20MHZ Short GI*/
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_7_2_MBPS, "MCS_VHT20_SGI_7_2_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_14_4_MBPS,"MCS_VHT20_SGI_14_4_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_21_6_MBPS,"MCS_VHT20_SGI_21_6_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_28_8_MBPS,"MCS_VHT20_SGI_28_8_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_43_3_MBPS,"MCS_VHT20_SGI_43_3_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_57_7_MBPS,"MCS_VHT20_SGI_57_7_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_65_MBPS,  "MCS_VHT20_SGI_65_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_72_2_MBPS,"MCS_VHT20_SGI_72_2_MBPS"},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_86_6_MBPS,"MCS_VHT20_SGI_86_6_MBPS"},
#ifdef WCN_PRONTO
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,"MCS_VHT20_SGI_96_1_MBPS"},
#endif

    /*11AC rates 40MHZ normal GI*/
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
       "MCS_VHT40_NGI_CB_13_5_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
       "MCS_VHT40_NGI_CB_27_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
       "MCS_VHT40_NGI_CB_40_5_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
       "MCS_VHT40_NGI_CB_54_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
       "MCS_VHT40_NGI_CB_81_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
       "MCS_VHT40_NGI_CB_108_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
       "MCS_VHT40_NGI_CB_121_5_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
       "MCS_VHT40_NGI_CB_135_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
       "MCS_VHT40_NGI_CB_162_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
       "MCS_VHT40_NGI_CB_180_MBPS"},

    /*11AC rates 40MHZ short GI*/
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
       "MCS_VHT40_SGI_CB_15_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
       "MCS_VHT40_SGI_CB_30_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
       "MCS_VHT40_SGI_CB_45_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
       "MCS_VHT40_SGI_CB_60_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
       "MCS_VHT40_SGI_CB_90_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
       "MCS_VHT40_SGI_CB_120_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
       "MCS_VHT40_SGI_CB_135_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
       "MCS_VHT40_SGI_CB_150_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
       "MCS_VHT40_SGI_CB_180_MBPS"},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,
       "MCS_VHT40_SGI_CB_200_MBPS"},

    /*11AC rates 80 MHZ normal GI*/
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
       "MCS_VHT80_NGI_CB_29_3_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
       "MCS_VHT80_NGI_CB_58_5_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
       "MCS_VHT80_NGI_CB_87_8_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
       "MCS_VHT80_NGI_CB_117_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
       "MCS_VHT80_NGI_CB_175_5_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
       "MCS_VHT80_NGI_CB_234_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
       "MCS_VHT80_NGI_CB_263_3_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
       "MCS_VHT80_NGI_CB_292_5_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
       "MCS_VHT80_NGI_CB_351_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
       "MCS_VHT80_NGI_CB_390_MBPS"},

    /*11AC rates 80 MHZ short GI*/
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
       "MCS_VHT80_SGI_CB_32_5_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
       "MCS_VHT80_SGI_CB_65_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
       "MCS_VHT80_SGI_CB_97_5_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
       "MCS_VHT80_SGI_CB_130_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
       "MCS_VHT80_SGI_CB_195_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
       "MCS_VHT80_SGI_CB_260_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
       "MCS_VHT80_SGI_CB_292_5_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
       "MCS_VHT80_SGI_CB_325_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
       "MCS_VHT80_SGI_CB_390_MBPS"},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
       "MCS_VHT80_SGI_CB_433_3_MBPS"},
#endif
};

static rateIndex2Preamble_t rate_index_2_preamble_table[] =
{

   { HAL_PHY_RATE_11B_LONG_1_MBPS,       PHYDBG_PREAMBLE_LONGB},
   { HAL_PHY_RATE_11B_LONG_2_MBPS,       PHYDBG_PREAMBLE_LONGB},
   { HAL_PHY_RATE_11B_LONG_5_5_MBPS,     PHYDBG_PREAMBLE_LONGB},
   { HAL_PHY_RATE_11B_LONG_11_MBPS,      PHYDBG_PREAMBLE_LONGB},
   { HAL_PHY_RATE_11B_SHORT_2_MBPS,      PHYDBG_PREAMBLE_SHORTB},
   { HAL_PHY_RATE_11B_SHORT_5_5_MBPS,    PHYDBG_PREAMBLE_SHORTB},
   { HAL_PHY_RATE_11B_SHORT_11_MBPS,     PHYDBG_PREAMBLE_SHORTB},


   //Spica_Virgo 11A 20MHz Rates
   { HAL_PHY_RATE_11A_6_MBPS,           PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_9_MBPS,           PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_12_MBPS,          PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_18_MBPS,          PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_24_MBPS,          PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_36_MBPS,          PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_48_MBPS,          PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_54_MBPS,          PHYDBG_PREAMBLE_OFDM},

   // 11A 20MHz Rates
   { HAL_PHY_RATE_11A_DUP_6_MBPS,       PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_DUP_9_MBPS,       PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_DUP_12_MBPS,      PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_DUP_18_MBPS,      PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_DUP_24_MBPS,      PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_DUP_36_MBPS,      PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_DUP_48_MBPS,      PHYDBG_PREAMBLE_OFDM},
   { HAL_PHY_RATE_11A_DUP_54_MBPS,      PHYDBG_PREAMBLE_OFDM},

   //MCS Index #0-15 (20MHz)
   { HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,   PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_13_MBPS,    PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_26_MBPS,    PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_39_MBPS,    PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_52_MBPS,    PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_65_MBPS,    PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS, PHYDBG_PREAMBLE_NOT_SUPPORTED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,PHYDBG_PREAMBLE_NOT_SUPPORTED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,PHYDBG_PREAMBLE_NOT_SUPPORTED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,PHYDBG_PREAMBLE_NOT_SUPPORTED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,PHYDBG_PREAMBLE_NOT_SUPPORTED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,PHYDBG_PREAMBLE_NOT_SUPPORTED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS, PHYDBG_PREAMBLE_NOT_SUPPORTED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS, PHYDBG_PREAMBLE_MIXED},

   //MCS index (40MHz)
   { HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS, PHYDBG_PREAMBLE_MIXED},

#ifdef WLAN_FEATURE_11AC
    /*11AC rate 20MHZ Normal GI*/
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_6_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_13_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_19_5_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_26_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_39_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_52_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_58_5_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_65_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_78_MBPS,  PHYDBG_PREAMBLE_MIXED},
#ifdef WCN_PRONTO
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,PHYDBG_PREAMBLE_MIXED},
#endif
    /*11AC rate 20MHZ Short GI*/
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_7_2_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_14_4_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_21_6_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_28_8_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_43_3_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_57_7_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_65_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_72_2_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_86_6_MBPS,PHYDBG_PREAMBLE_MIXED},
#ifdef WCN_PRONTO
   { HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,PHYDBG_PREAMBLE_MIXED},
#endif

    /*11AC rates 40MHZ normal GI*/
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,   PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,   PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,   PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,  PHYDBG_PREAMBLE_MIXED},

    /*11AC rates 40MHZ short GI*/
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,PHYDBG_PREAMBLE_MIXED},

    /*11AC rates 80 MHZ normal GI*/
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,  PHYDBG_PREAMBLE_MIXED},

    /*11AC rates 80 MHZ short GI*/
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,   PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS, PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,  PHYDBG_PREAMBLE_MIXED},
   { HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,PHYDBG_PREAMBLE_MIXED},
#endif
};

static unsigned int valid_channel[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                       11, 12, 13, 14, 36, 40, 44, 48,
                                       52, 56, 60, 64, 100, 104, 108,
                                       112, 116, 120, 124, 128, 132,
                                       136, 140, 149, 153, 157, 161,
                                       165, 208, 212, 216, 240, 244,
                                       248, 252, 0 };
static unsigned int valid_channel_cb40[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 38,
                                             42, 46, 50, 54, 58, 62, 102, 106,
                                             110, 114, 118, 122, 126, 130, 134,
                                             138, 151, 155, 159, 163, 210, 214,
                                             242, 246, 250, 0 };
static unsigned int valid_channel_cb80[] = { 7, 42, 46, 50, 54, 58, 106, 110,
                                             114, 118, 122, 126, 130, 134, 155,
                                             159, 246, 0 };

typedef struct
{
    tANI_BOOLEAN frameGenEnabled;
    tANI_BOOLEAN wfRfGenEnabled;
    tANI_BOOLEAN wfmEnabled;
    sPttFrameGenParams frameParams;
    v_U16_t powerCtlMode;
    v_U16_t rxmode;
    v_U16_t chainSelect;
    ePhyChanBondState cbmode;

} FTM_STATUS ;
static FTM_STATUS ftm_status;

//tpAniSirGlobal pMac;
static tPttMsgbuffer *pMsgBuf;

static void _ftm_status_init(void)
{
    tANI_U8 addr1[ANI_MAC_ADDR_SIZE] = { 0x00, 0x11, 0x11, 0x11, 0x11, 0x11 };   //dest
    tANI_U8 addr2[ANI_MAC_ADDR_SIZE] = { 0x00, 0x22, 0x22, 0x22, 0x22, 0x22 };   //sour
    tANI_U8 addr3[ANI_MAC_ADDR_SIZE] = { 0x00, 0x33, 0x33, 0x33, 0x33, 0x33 };   //bssId

    ftm_status.wfmEnabled = eANI_BOOLEAN_FALSE;
    ftm_status.frameGenEnabled = eANI_BOOLEAN_FALSE;
    ftm_status.wfRfGenEnabled = eANI_BOOLEAN_FALSE;
    ftm_status.frameParams.numTestPackets = 0;   //Continuous
    ftm_status.frameParams.interFrameSpace = 200;
    ftm_status.frameParams.rate = HAL_PHY_RATE_11A_6_MBPS;
    ftm_status.frameParams.payloadContents = TEST_PAYLOAD_RANDOM;
    ftm_status.frameParams.payloadLength = 1000;
    ftm_status.frameParams.payloadFillByte = 0xA5;
    ftm_status.frameParams.pktAutoSeqNum = eANI_BOOLEAN_FALSE;
    ftm_status.frameParams.tx_mode = 0;
    ftm_status.frameParams.crc = 0;
    ftm_status.frameParams.preamble = PHYDBG_PREAMBLE_OFDM;
    memcpy(&ftm_status.frameParams.addr1[0], addr1, ANI_MAC_ADDR_SIZE);
    memcpy(&ftm_status.frameParams.addr2[0], addr2, ANI_MAC_ADDR_SIZE);
    memcpy(&ftm_status.frameParams.addr3[0], addr3, ANI_MAC_ADDR_SIZE);
    ftm_status.powerCtlMode= 2 ; //CLPC mode
    ftm_status.rxmode = RXMODE_ENABLE_ALL; /* macStart() enables all receive pkt types */
    ftm_status.chainSelect = FTM_CHAIN_SEL_R0_T0_ON;
    ftm_status.cbmode = 0 ; //none channel bonding

    return;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_postmsg() -

   The function used for sending the command to the halphy.

  \param  - cmd_ptr - Pointer command buffer.

  \param  - cmd_len - Command length.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static v_U32_t wlan_ftm_postmsg(v_U8_t *cmd_ptr, v_U16_t cmd_len)
{
    vos_msg_t   *ftmReqMsg;
    vos_msg_t    ftmMsg;
    ENTER();

    ftmReqMsg = (vos_msg_t *) cmd_ptr;

    ftmMsg.type = WDA_FTM_CMD_REQ;
    ftmMsg.reserved = 0;
    ftmMsg.bodyptr = (v_U8_t*)cmd_ptr;
    ftmMsg.bodyval = 0;

    /* Use Vos messaging mechanism to send the command to halPhy */
    if (VOS_STATUS_SUCCESS != vos_mq_post_message(
        VOS_MODULE_ID_WDA,
                                    (vos_msg_t *)&ftmMsg)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,"%s: : Failed to post Msg to HAL",__func__);

        return VOS_STATUS_E_FAILURE;
    }

    EXIT();
    return VOS_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------

  \brief wlan_ftm_vos_open() - Open the vOSS Module

  The \a wlan_ftm_vos_open() function opens the vOSS Scheduler
  Upon successful initialization:

     - All VOS submodules should have been initialized

     - The VOS scheduler should have opened

     - All the WLAN SW components should have been opened. This include
       MAC.


  \param  devHandle: pointer to the OS specific device handle.


  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initialize the scheduler


          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/

  \sa wlan_ftm_vos_open()

---------------------------------------------------------------------------*/
static VOS_STATUS wlan_ftm_vos_open( v_CONTEXT_t pVosContext, v_PVOID_t devHandle )
{
   VOS_STATUS vStatus      = VOS_STATUS_SUCCESS;
   int iter                = 0;
   tSirRetStatus sirStatus = eSIR_SUCCESS;
   tMacOpenParameters macOpenParms;
   pVosContextType gpVosContext = (pVosContextType)pVosContext;

   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: Opening VOSS", __func__);

   if (NULL == gpVosContext)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "%s: Trying to open VOSS without a PreOpen",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   /* Initialize the probe event */
   if (vos_event_init(&gpVosContext->ProbeEvent) != VOS_STATUS_SUCCESS)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "%s: Unable to init probeEvent",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   if(vos_event_init(&(gpVosContext->wdaCompleteEvent)) != VOS_STATUS_SUCCESS )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Unable to init wdaCompleteEvent",__func__);
      VOS_ASSERT(0);

      goto err_probe_event;
   }

   if(vos_event_init(&(gpVosContext->fwLogsComplete)) != VOS_STATUS_SUCCESS )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Unable to init fwLogsComplete",__func__);
      VOS_ASSERT(0);

      goto err_wda_complete_event;
   }

   /* Initialize the free message queue */
   vStatus = vos_mq_init(&gpVosContext->freeVosMq);
   if (! VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to initialize VOS free message queue %d",
                 __func__, vStatus);
      VOS_ASSERT(0);
      goto err_fw_logs_complete_event;
   }

   for (iter = 0; iter < VOS_CORE_MAX_MESSAGES; iter++)
   {
      (gpVosContext->aMsgWrappers[iter]).pVosMsg =
         &(gpVosContext->aMsgBuffers[iter]);
      INIT_LIST_HEAD(&gpVosContext->aMsgWrappers[iter].msgNode);
      vos_mq_put(&gpVosContext->freeVosMq, &(gpVosContext->aMsgWrappers[iter]));
   }

   /* Now Open the VOS Scheduler */
   vStatus= vos_sched_open(gpVosContext, &gpVosContext->vosSched,
                           sizeof(VosSchedContext));

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to open VOS Scheduler %d", __func__, vStatus);
      VOS_ASSERT(0);
      goto err_msg_queue;
   }

   /* Open the SYS module */
   vStatus = sysOpen(gpVosContext);

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to open SYS module %d", __func__, vStatus);
      VOS_ASSERT(0);
      goto err_sched_close;
   }

   /*Open the WDA module */
   vos_mem_set(&macOpenParms, sizeof(macOpenParms), 0);
   macOpenParms.driverType = eDRIVER_TYPE_MFG;
   vStatus = WDA_open(gpVosContext, devHandle, &macOpenParms);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to open WDA module %d", __func__, vStatus);
      VOS_ASSERT(0);
      goto err_sys_close;
   }

   /* initialize the NV module */
   vStatus = vos_nv_open();
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     // NV module cannot be initialized, however the driver is allowed
     // to proceed
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to initialize the NV module %d", __func__, vStatus);
     goto err_wda_close;
   }

   vStatus = vos_nv_get_dictionary_data();

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s : failed to get dictionary data for NV %d",
                 __func__, vStatus);
      goto err_wda_close;
   }

   /* If we arrive here, both threads dispacthing messages correctly */

   /* Now proceed to open the MAC */

   /* UMA is supported in hardware for performing the
      frame translation 802.11 <-> 802.3 */
   macOpenParms.frameTransRequired = 1;
   sirStatus = macOpen(&(gpVosContext->pMACContext), gpVosContext->pHDDContext,
                         &macOpenParms);

   if (eSIR_SUCCESS != sirStatus)
   {
     /* Critical Error ...  Cannot proceed further */
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to open MAC %d", __func__, sirStatus);
     VOS_ASSERT(0);
     goto err_nv_close;
   }

   /* Now proceed to open the SME */
   vStatus = sme_Open(gpVosContext->pMACContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to open SME %d", __func__, vStatus);
      goto err_mac_close;
   }
   return VOS_STATUS_SUCCESS;


   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS successfully Opened",__func__);

   return VOS_STATUS_SUCCESS;
err_mac_close:
   macClose(gpVosContext->pMACContext);

err_nv_close:
   vos_nv_close();

err_wda_close:
   WDA_close(gpVosContext);

err_sys_close:
   sysClose(gpVosContext);

err_sched_close:
   vos_sched_close(gpVosContext);
err_msg_queue:
   vos_mq_deinit(&gpVosContext->freeVosMq);

err_fw_logs_complete_event:
   vos_event_destroy(&gpVosContext->fwLogsComplete);

err_wda_complete_event:
   vos_event_destroy(&gpVosContext->wdaCompleteEvent);

err_probe_event:
   vos_event_destroy(&gpVosContext->ProbeEvent);

   return VOS_STATUS_E_FAILURE;

} /* wlan_ftm_vos_open() */

/*---------------------------------------------------------------------------

  \brief wlan_ftm_vos_close() - Close the vOSS Module

  The \a wlan_ftm_vos_close() function closes the vOSS Module

  \param vosContext  context of vos

  \return VOS_STATUS_SUCCESS - successfully closed

  \sa wlan_ftm_vos_close()

---------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_vos_close( v_CONTEXT_t vosContext )
{
  VOS_STATUS vosStatus;
  pVosContextType gpVosContext = (pVosContextType)vosContext;

  vosStatus = sme_Close(((pVosContextType)vosContext)->pMACContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close SME %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = macClose( ((pVosContextType)vosContext)->pMACContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close MAC %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  ((pVosContextType)vosContext)->pMACContext = NULL;

  vosStatus = vos_nv_close();
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close NV %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }


  vosStatus = sysClose( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close SYS %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  if ( TRUE == WDA_needShutdown(vosContext))
  {
     vosStatus = WDA_shutdown(vosContext, VOS_TRUE);
     if (!VOS_IS_STATUS_SUCCESS(vosStatus))
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                            "%s: Failed to shutdown WDA %d", __func__, vosStatus);
        VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));
     }

  }
  else
  {
     vosStatus = WDA_close(vosContext);
     if (!VOS_IS_STATUS_SUCCESS(vosStatus))
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to close WDA %d", __func__, vosStatus);
        VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));
     }
  }

  vos_mq_deinit(&((pVosContextType)vosContext)->freeVosMq);

  vosStatus = vos_event_destroy(&gpVosContext->ProbeEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to destroy ProbeEvent %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = vos_event_destroy(&gpVosContext->wdaCompleteEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to destroy wdaCompleteEvent %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = vos_event_destroy(&gpVosContext->fwLogsComplete);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to destroy fwLogsComplete %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_set_txifs() -

   This function is used for

  \param  - pAdapter - Pointer HDD Context.
          - ifs

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/



static VOS_STATUS wlan_ftm_priv_set_txifs(hdd_adapter_t *pAdapter,v_U32_t ifs)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm. ",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* do not allow to change setting when tx pktgen is enabled */
    if (ftm_status.frameGenEnabled)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:cannot set txifs when pktgen is enabled.",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (ifs > 100000) //max = (MSK_24 / ONE_MICROSECOND)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                    "%s:ifs value is invalid %x", __func__, ifs);
        return VOS_STATUS_E_FAILURE;
    }

    ftm_status.frameParams.interFrameSpace = ifs;

    return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_set_txpktcnt() -

   This function is used for

  \param  - pAdapter - Pointer HDD Context.
          - ifs

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_set_txpktcnt(hdd_adapter_t *pAdapter,v_U32_t cnt)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm. ",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* do not allow to change setting when tx pktgen is enabled */
    if (ftm_status.frameGenEnabled)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:cannot set txpktcnt when pktgen is enabled.",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (cnt > QWLAN_PHYDBG_TXPKT_CNT_CNT_MASK) //0xFFFF
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s:pktcnt value is invalid %08x", __func__, cnt);
        return VOS_STATUS_E_FAILURE;
    }

    ftm_status.frameParams.numTestPackets = cnt;

    return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wlan_ftm_priv_set_txpktlen(hdd_adapter_t *pAdapter,v_U32_t len)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm. ",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* do not allow to change setting when tx pktgen is enabled */
    if (ftm_status.frameGenEnabled)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:cannot set txpktcnt when pktgen is enabled.",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (len > 4095) //4096
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:payload len is invalid %08x", __func__, len);
        return VOS_STATUS_E_FAILURE;
    }

    ftm_status.frameParams.payloadLength = (tANI_U16)len;

    return VOS_STATUS_SUCCESS;
}


static VOS_STATUS wlan_ftm_priv_start_stop_tx_pktgen(hdd_adapter_t *pAdapter,v_U16_t startStop);
/**---------------------------------------------------------------------------
<FTM_Command>set_tx_wf_gain
<argument> is <n>
Designates the number of amplitude gain (31 to 255).
Description
This command can be set only when Tx CW generation is stopped.
--------------------------------------------------------------------------*/
static VOS_STATUS wlan_ftm_priv_set_wfgain(hdd_adapter_t *pAdapter,v_S15_t dGain,v_U16_t rfGain)
{
    uPttMsgs *pMsgBody;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    printk(KERN_EMERG "dGain: %02x rfGain: %02x", dGain,rfGain);
    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm.", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (ftm_status.wfRfGenEnabled) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:cannot set gain when cwgen is enabled.", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (dGain > 24 || dGain <-39) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:digital gain value is invalid", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (rfGain > 31) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:rf gain value is invalid", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (pMsgBuf == NULL) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    wlan_ftm_priv_start_stop_tx_pktgen(pAdapter,0);
    wlan_ftm_priv_start_stop_tx_pktgen(pAdapter,0);

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_SET_TX_WAVEFORM_GAIN_PRIMA_V1;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttSetTxWaveformGain_PRIMA_V1) + PTT_HEADER_LENGTH;
    pMsgBody = &pMsgBuf->msgBody;
    pMsgBody->SetTxWaveformGain_PRIMA_V1.txChain = PHY_TX_CHAIN_0;
    pMsgBody->SetTxWaveformGain_PRIMA_V1.gain = (rfGain << 16 | (dGain & 0xffff));
    if (wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength) !=
           VOS_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed",__func__);
        return VOS_STATUS_E_FAILURE;
    }
    return VOS_STATUS_SUCCESS;
}


/**---------------------------------------------------------------------------
  <FTM_Command> wlan_ftm_priv_cw_rf_gen
  <argument> is < 1 | 0 >
  1 : Start Tx CW rf generation
  0 : Stop Tx CW rf generation
  Description
    This command starts/stops Tx CW rf generation.
--------------------------------------------------------------------------*/
static VOS_STATUS wlan_ftm_priv_cw_rf_gen(hdd_adapter_t *pAdapter,v_U16_t startStop)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    printk(KERN_EMERG "startStop: %02x ", startStop);

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (startStop != 1 && startStop != 0)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Tx value is invalid ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if ((ftm_status.wfRfGenEnabled && startStop == 1) ||
        (!ftm_status.wfRfGenEnabled && startStop == 0))
    {
        return VOS_STATUS_SUCCESS;
    }

    if (pMsgBuf == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }
    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    if (startStop == 1) {
        tANI_U16 numSamples = 1;
        tANI_BOOLEAN clk80 = TRUE;
        v_BYTE_t msgT[4] = {0xff,0x00,0x00,0x00};

        init_completion(&pHddCtx->ftm.ftm_comp_var);
        pMsgBuf->msgId = PTT_MSG_SET_WAVEFORM;
        pMsgBuf->msgBodyLength = sizeof(tMsgPttSetWaveformRF) + PTT_HEADER_LENGTH;
        pMsgBody = &pMsgBuf->msgBody;

        memcpy((v_BYTE_t*)pMsgBody->SetWaveformRF.waveform,msgT,4);
        pMsgBody->SetWaveformRF.numSamples = numSamples;
        pMsgBody->SetWaveformRF.clk80 = clk80;
        status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);
        if (status != VOS_STATUS_SUCCESS) {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                       "%s:posting PTT_MSG_CONFIG_TX_PACKET_GEN failed",
                       __func__);
            status = VOS_STATUS_E_FAILURE;
            goto done;
        }
        wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                  msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
        if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS) {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                       "%s: PTT_MSG_CONFIG_TX_PACKET_GEN failed", __func__);
            status = VOS_STATUS_E_FAILURE;
            goto done;
        }
    } else {
        init_completion(&pHddCtx->ftm.ftm_comp_var);
        pMsgBuf->msgId = PTT_MSG_STOP_WAVEFORM;
        pMsgBuf->msgBodyLength = PTT_HEADER_LENGTH;
        status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);
        if(status != VOS_STATUS_SUCCESS) {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                       "%s:wlan_ftm_postmsg failed", __func__);
            status = VOS_STATUS_E_FAILURE;
            goto done;
        }

        wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                  msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
        if(pMsgBuf->msgResponse != PTT_STATUS_SUCCESS) {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                       "%s:Ptt response status failed", __func__);
            status = VOS_STATUS_E_FAILURE;
        }
    }
done:
    if (status == VOS_STATUS_SUCCESS) {
        if (startStop == 1)
            ftm_status.wfRfGenEnabled = eANI_BOOLEAN_TRUE;
        else
            ftm_status.wfRfGenEnabled = eANI_BOOLEAN_FALSE;
    }
    return status;
}


static VOS_STATUS wlan_ftm_priv_enable_chain(hdd_adapter_t *pAdapter,v_U16_t chainSelect)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    v_U16_t chainSelect_save = chainSelect;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    long ret;

    if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm. ",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    if (chainSelect > FTM_CHAIN_SEL_MAX)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Invalid chain %08x", __func__, chainSelect);
        return VOS_STATUS_E_FAILURE;
    }

    /* do not allow to change setting when tx pktgen is enabled */
    if (ftm_status.frameGenEnabled)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:cannot select chain when pktgen is enabled.",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    switch (chainSelect)
    {
        case FTM_CHAIN_SEL_NO_RX_TX:
            chainSelect = PHY_CHAIN_SEL_NO_RX_TX;
            break;

        case FTM_CHAIN_SEL_R0_ON:
            chainSelect = PHY_CHAIN_SEL_R0_ON;
            break;

        case FTM_CHAIN_SEL_T0_ON:
            chainSelect = PHY_CHAIN_SEL_T0_ON;
            break;

        case FTM_CHAIN_SEL_ANTENNA_0:
            chainSelect = PHY_CHAIN_SEL_ANT_0;
            break;

        case FTM_CHAIN_SEL_ANTENNA_1:
            chainSelect = PHY_CHAIN_SEL_ANT_1;
            break;
    }

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_ENABLE_CHAINS;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttEnableChains) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;
    pMsgBody->EnableChains.chainSelect = chainSelect;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ftm_status.chainSelect = chainSelect_save;
done:

    return status;
}

/**---------------------------------------------------------------------------
  --------------------------------------------------------------------------*/
static VOS_STATUS wlan_ftm_priv_get_status(hdd_adapter_t *pAdapter,char *buf)
{
    int ii;
    int lenBuf = WE_FTM_MAX_STR_LEN;
    int lenRes = 0;
    char *chain[] = {
        "None",
        "R0,R1",
        "R0",
        "R1",
        "T0",
        "R0,R1,T0"
    };
    char *rx[] = {
        "disable",
        "11b/g/n",
        "11g/n",
        "11b"
    };
    char *tx[] = {
        "stopped",
        "started",
    };
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm. ",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    lenRes = snprintf(buf, lenBuf, "\n chainSelect: %s\n rxmode: %s\n "
                                   "txpktgen: %s\n  txifs: %d\n  txrate: ",
                      chain[ftm_status.chainSelect], rx[ftm_status.rxmode],
                      tx[ftm_status.frameGenEnabled],
                      ftm_status.frameParams.interFrameSpace);
    if ((lenRes < 0) || (lenRes >= lenBuf))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("failed to copy data into buf %d"), lenRes);
       return VOS_STATUS_E_FAILURE;
    }

    buf += lenRes;
    lenBuf -= lenRes;

    for (ii = 0; ii < SIZE_OF_TABLE(rateName_rateIndex_tbl); ii++)
    {
        if (rateName_rateIndex_tbl[ii].rate_index == ftm_status.frameParams.rate)
            break;
    }

    if (ii < SIZE_OF_TABLE(rateName_rateIndex_tbl))
    {
        lenRes = strlcpy(buf, rateName_rateIndex_tbl[ii].rate_str, lenBuf);
    }
    else
    {
        lenRes = strlcpy(buf, "invalid", lenBuf);
    }
    if ((lenRes < 0) || (lenRes >= lenBuf))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         FL("failed to copy data into buf %d"), lenRes);
       return VOS_STATUS_E_FAILURE;
    }

    buf += lenRes;
    lenBuf -= lenRes;

    lenRes = snprintf(buf, lenBuf, "\n  power ctl mode: %d\n  txpktcnt: %d\n  "
                                   "txpktlen: %d\n", ftm_status.powerCtlMode,
                      ftm_status.frameParams.numTestPackets,
                      ftm_status.frameParams.payloadLength);

    if ((lenRes < 0) || (lenRes >= lenBuf))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         FL("failed to copy data into buf %d"), lenRes);
       return VOS_STATUS_E_FAILURE;
    }

    return VOS_STATUS_SUCCESS;
}


void HEXDUMP(char *s0, char *s1, int len)
{
    int i = 0, j = 0;
    printk(KERN_EMERG "%s\n :", s0);

    if (len > 8)
    {
        for (j = 0; j < len/8; j++)
        {
            printk(KERN_EMERG "%02x %02x %02x %02x %02x %02x %02x %02x",
                    s1[j*8], s1[j*8+1], s1[j*8+2], s1[j*8+3], s1[j*8+4],
                    s1[j*8+5],s1[j*8+6],s1[j*8+7] );
        }
        len = len - j*8;
    }
    for (i = 0; i< len; i++) {
        printk(KERN_EMERG "%02x ", s1[j*8+i]);
    }
    printk("\n");
}

/*---------------------------------------------------------------------------

  \brief vos_ftm_preStart() -

  The \a vos_ftm_preStart() function to download CFG.
  including:
      - ccmStart

      - WDA: triggers the CFG download


  \param  pVosContext: The VOS context


  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initialize the scheduler


          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/

  \sa vos_start

---------------------------------------------------------------------------*/
VOS_STATUS vos_ftm_preStart( v_CONTEXT_t vosContext )
{
   VOS_STATUS vStatus          = VOS_STATUS_SUCCESS;
   pVosContextType pVosContext = (pVosContextType)vosContext;

   VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO,
             "vos prestart");

   if (NULL == pVosContext->pWDAContext)
   {
      VOS_ASSERT(0);
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
            "%s: WDA NULL context", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   /* call macPreStart */
   vStatus = macPreStart(pVosContext->pMACContext);
   if ( !VOS_IS_STATUS_SUCCESS(vStatus) )
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
             "Failed at macPreStart ");
      return VOS_STATUS_E_FAILURE;
   }

   /* call ccmStart */
   ccmStart(pVosContext->pMACContext);

   /* Reset wda wait event */
   vos_event_reset(&pVosContext->wdaCompleteEvent);


   /*call WDA pre start*/
   vStatus = WDA_preStart(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
             "Failed to WDA prestart ");
      macStop(pVosContext->pMACContext, HAL_STOP_TYPE_SYS_DEEP_SLEEP);
      ccmStop(pVosContext->pMACContext);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   /* Need to update time out of complete */
   vStatus = vos_wait_single_event( &pVosContext->wdaCompleteEvent, 1000);
   if ( vStatus != VOS_STATUS_SUCCESS )
   {
      if ( vStatus == VOS_STATUS_E_TIMEOUT )
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: Timeout occurred before WDA complete",__func__);
      }
      else
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: WDA_preStart reporting  other error",__func__);
      }
      VOS_ASSERT( 0 );
      return VOS_STATUS_E_FAILURE;
   }

   return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_open() -

   The function hdd_wlan_startup calls this function to initialize the FTM specific modules.

  \param  - pAdapter - Pointer HDD Context.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

int wlan_hdd_ftm_open(hdd_context_t *pHddCtx)
{
    VOS_STATUS vStatus       = VOS_STATUS_SUCCESS;
    pVosContextType pVosContext= NULL;
    hdd_adapter_t *pAdapter;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: Opening VOSS", __func__);

    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

    if (NULL == pVosContext)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: Trying to open VOSS without a PreOpen", __func__);
        VOS_ASSERT(0);
        goto err_vos_status_failure;
    }

   // Open VOSS
   vStatus = wlan_ftm_vos_open( pVosContext, pHddCtx->parent_dev);

   if ( !VOS_IS_STATUS_SUCCESS( vStatus ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_open failed", __func__);
      goto err_vos_status_failure;
   }

    /*
     For Integrated SOC, only needed to start WDA, whihc happens in wlan_hdd_ftm_start()
    */
    /* Save the hal context in Adapter */
    pHddCtx->hHal = (tHalHandle)vos_get_context(VOS_MODULE_ID_SME, pVosContext );

    if ( NULL == pHddCtx->hHal )
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: HAL context is null", __func__);
       goto err_ftm_vos_close;
    }

    pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_FTM, "wlan%d",
                wlan_hdd_get_intf_addr(pHddCtx), FALSE);
    if( NULL == pAdapter )
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: hdd_open_adapter failed", __func__);
       goto err_adapter_open_failure;
    }

    if( wlan_ftm_register_wext(pAdapter)!= 0 )
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: hdd_register_wext failed", __func__);
       goto err_adapter_close;
    }

       //Initialize the nlink service
    if(nl_srv_init() != 0)
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: nl_srv_init failed", __func__);
       goto err_ftm_register_wext_close;
    }

#ifdef WLAN_KD_READY_NOTIFIER
   pHddCtx->kd_nl_init = 1;
#endif /* WLAN_KD_READY_NOTIFIER */

#ifdef PTT_SOCK_SVC_ENABLE
    //Initialize the PTT service
    if(ptt_sock_activate_svc(pHddCtx) != 0)
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: ptt_sock_activate_svc failed", __func__);
       goto err_nl_srv_init;
    }
#endif

   pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
   pHddCtx->ftm.targetNVTableSize    = 0;
   pHddCtx->ftm.targetNVTablePointer = NULL;
   pHddCtx->ftm.processedNVTableSize = 0;
   pHddCtx->ftm.tempNVTableBuffer    = (v_U8_t *)vos_mem_malloc(MAX_NV_TABLE_SIZE);
   if(NULL == pHddCtx->ftm.tempNVTableBuffer)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: NV Table Buffer Alloc Fail", __func__);
      VOS_ASSERT(0);
      goto err_nl_srv_init;
   }
   vos_mem_zero((v_VOID_t *)pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);

    _ftm_status_init();
    /* Initialize the ftm vos event */
    if (vos_event_init(&pHddCtx->ftm.ftm_vos_event) != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: Unable to init probeEvent", __func__);
        VOS_ASSERT(0);
        vos_mem_free(pHddCtx->ftm.tempNVTableBuffer);
        goto err_nl_srv_init;
    }

    pHddCtx->ftm.ftm_state = WLAN_FTM_INITIALIZED;
    init_completion(&pHddCtx->ftm.startCmpVar);

    return VOS_STATUS_SUCCESS;

err_nl_srv_init:
#ifdef WLAN_KD_READY_NOTIFIER
nl_srv_exit(pHddCtx->ptt_pid);
#else
nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */
err_ftm_register_wext_close:
hdd_UnregisterWext(pAdapter->dev);

err_adapter_close:
err_adapter_open_failure:
hdd_close_all_adapters( pHddCtx );

err_ftm_vos_close:
    wlan_ftm_vos_close(pVosContext);
err_vos_status_failure:

    return VOS_STATUS_E_FAILURE;
}



int wlan_hdd_ftm_close(hdd_context_t *pHddCtx)
{
    VOS_STATUS vosStatus;
    v_CONTEXT_t vosContext = pHddCtx->pvosContext;

    hdd_adapter_t *pAdapter = hdd_get_adapter(pHddCtx,WLAN_HDD_FTM);
    ENTER();
    if(pAdapter == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:pAdapter is NULL",__func__);
        return VOS_STATUS_E_NOMEM;
    }

    /*release the wlan_hdd_process_ftm_cmd(), if waiting for any response.*/
    if (pHddCtx->ftm.IsCmdPending == TRUE)
    {
        if (vos_event_set(&pHddCtx->ftm.ftm_vos_event)!= VOS_STATUS_SUCCESS)
        {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                                      "%s: vos_event_set failed", __func__);
        }
    }
    if(WLAN_FTM_STARTED == pHddCtx->ftm.ftm_state)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: Ftm has been started. stopping ftm", __func__);
        wlan_ftm_stop(pHddCtx);
    }

#ifdef WLAN_KD_READY_NOTIFIER
    nl_srv_exit(pHddCtx->ptt_pid);
#else
    nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */
    //TODO----------
    //Deregister the device with the kernel
    hdd_UnregisterWext(pAdapter->dev);

    hdd_close_all_adapters( pHddCtx );
#if 0
    if(test_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags))
    {
        unregister_netdev(pAdapter->dev);
        clear_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags);
    }
#endif
    //-----------------

    vosStatus = vos_sched_close( vosContext );
    if (!VOS_IS_STATUS_SUCCESS(vosStatus))       {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: Failed to close VOSS Scheduler",__func__);
       VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
    }

    //Close VOSS
    wlan_ftm_vos_close(vosContext);


    vosStatus = vos_event_destroy(&pHddCtx->ftm.ftm_vos_event);
    if (!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to destroy ftm_vos Event",__func__);
        VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
    }
    vos_mem_free(pHddCtx->ftm.tempNVTableBuffer);

    return 0;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_send_response() -

   The function sends the response to the ptt socket application running in user space.

  \param  - pAdapter - Pointer HDD Context.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_send_response(hdd_context_t *pHddCtx){

   if( ptt_sock_send_msg_to_app(&pHddCtx->ftm.wnl->wmsg, 0,
                   ANI_NL_MSG_PUMAC, pHddCtx->ftm.wnl->nlh.nlmsg_pid, 0) < 0) {

       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("Ptt Socket error sending message to the app!!"));
       return VOS_STATUS_E_FAILURE;
   }
   return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_start() -

   This function gets called when the FTM start commands received from the ptt socket application and
   it starts the following modules.
   1) SAL Start.
   2) BAL Start.
   3) MAC Start to download the firmware.


  \param  - pAdapter - Pointer HDD Context.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static int wlan_hdd_ftm_start(hdd_context_t *pHddCtx)
{
    VOS_STATUS vStatus          = VOS_STATUS_SUCCESS;
    tSirRetStatus sirStatus      = eSIR_SUCCESS;
    pVosContextType pVosContext = (pVosContextType)(pHddCtx->pvosContext);
    tHalMacStartParameters halStartParams;

    if (WLAN_FTM_STARTED == pHddCtx->ftm.ftm_state)
    {
       return VOS_STATUS_SUCCESS;
    }

    pHddCtx->ftm.ftm_state = WLAN_FTM_STARTING;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s: Starting Libra SW", __func__);

    /* We support only one instance for now ...*/
    if (pVosContext == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
           "%s: mismatch in context",__func__);
        goto err_status_failure;
    }


    if (pVosContext->pMACContext == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: MAC NULL context",__func__);
        goto err_status_failure;
    }

    /*
      Prima needs to start the WDA correctly instead of BAL and SAL
    */

    /* Vos preStart is calling */
    if ( !VOS_IS_STATUS_SUCCESS(vos_ftm_preStart(pHddCtx->pvosContext) ) )
    {
       hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_preStart failed",__func__);
       goto err_status_failure;
    }


    vStatus = WDA_NVDownload_Start(pVosContext);

    if ( vStatus != VOS_STATUS_SUCCESS )
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to start NV Download",__func__);
       return VOS_STATUS_E_FAILURE;
    }

    vStatus = vos_wait_single_event(&(pVosContext->wdaCompleteEvent), 1000 * 30);

    if ( vStatus != VOS_STATUS_SUCCESS )
    {
       if ( vStatus == VOS_STATUS_E_TIMEOUT )
       {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Timeout occurred before WDA_NVDownload_Start complete",__func__);
       }
       else
       {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "%s: WDA_NVDownload_Start reporting  other error",__func__);
       }
       VOS_ASSERT(0);
       WDA_setNeedShutdown(pHddCtx->pvosContext);
       goto err_status_failure;
    }

    vStatus = WDA_start(pVosContext);
    if (vStatus != VOS_STATUS_SUCCESS)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to start WDA",__func__);
       goto err_status_failure;
    }


    /* Start the MAC */
    vos_mem_zero((v_PVOID_t)&halStartParams, sizeof(tHalMacStartParameters));


    halStartParams.driverType = eDRIVER_TYPE_MFG;

    /* Start the MAC */
    sirStatus = macStart(pVosContext->pMACContext,(v_PVOID_t)&halStartParams);


    if (eSIR_SUCCESS != sirStatus)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s: Failed to start MAC", __func__);

        goto err_wda_stop;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s: MAC correctly started",__func__);


    pHddCtx->ftm.ftm_state = WLAN_FTM_STARTED;

    return VOS_STATUS_SUCCESS;

err_wda_stop:
   vos_event_reset(&(pVosContext->wdaCompleteEvent));
   WDA_stop(pVosContext, HAL_STOP_TYPE_RF_KILL);
   vStatus = vos_wait_single_event(&(pVosContext->wdaCompleteEvent), 1000);
   if(vStatus != VOS_STATUS_SUCCESS)
   {
      if(vStatus == VOS_STATUS_E_TIMEOUT)
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "%s: Timeout occurred before WDA_stop complete",__func__);

      }
      else
      {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: WDA_stop reporting  other error",__func__);
      }
      VOS_ASSERT(0);
   }

err_status_failure:

    pHddCtx->ftm.ftm_state = WLAN_FTM_INITIALIZED;
    return VOS_STATUS_E_FAILURE;

}


static int wlan_ftm_stop(hdd_context_t *pHddCtx)
{
   VOS_STATUS vosStatus;

   if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm. ",__func__);
       return VOS_STATUS_E_FAILURE;
   }

   //if(pHddCtx->ftm.cmd_iwpriv == TRUE)
   {
       /*  STOP MAC only */
       v_VOID_t *hHal;
       hHal = vos_get_context( VOS_MODULE_ID_SME, pHddCtx->pvosContext );
       if (NULL == hHal)
       {
           VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                      "%s: NULL hHal", __func__);
       }
       else
       {
           vosStatus = macStop(hHal, HAL_STOP_TYPE_SYS_DEEP_SLEEP );
           if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           {
               VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                          "%s: Failed to stop SYS", __func__);
               VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
           }
       }


       WDA_stop(pHddCtx->pvosContext, HAL_STOP_TYPE_RF_KILL);

    }
   return WLAN_FTM_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_get_nv_table() -
            Get Specific NV table
            NV V2 dedicated API

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             1, Process Host command success

  --------------------------------------------------------------------------*/
int wlan_hdd_ftm_get_nv_table
(
   hdd_context_t  *pHddCtx,
   tPttMsgbuffer  *ftmCmd
)
{
   VOS_STATUS          nvStatus = VOS_STATUS_SUCCESS;
   pttGetNvTable      *nvTable = (pttGetNvTable *)&ftmCmd->msgBody.GetNvTable;
   v_SIZE_t            nvSize;
   sHalNvV2           *nvContents = NULL;
   eNvVersionType      nvVersion;

   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "Not valid driver context");
      return -EINVAL;
   }

   nvVersion = vos_nv_getNvVersion();
   if (E_NV_V2 != nvVersion)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s : Not valid NV Version %d", __func__, nvVersion);
      return -EINVAL;
   }

   /* Test first chunk of NV table */
   if ((NV_MAX_TABLE == pHddCtx->ftm.processingNVTable) ||
      (0 == pHddCtx->ftm.processedNVTableSize))
   {
      nvStatus = vos_nv_getNVBuffer((void **)&nvContents, &nvSize);
      if ((VOS_STATUS_SUCCESS != nvStatus) || (NULL == nvContents))
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                FL("Fail to get cached NV value Status %d"), nvStatus);
         return -EIO;
      }

      switch (nvTable->nvTable)
      {
         case NV_TABLE_RATE_POWER_SETTINGS:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.pwrOptimum);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.pwrOptimum;
            break;

         case NV_TABLE_REGULATORY_DOMAINS:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.regDomains);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.regDomains;
            break;

         case NV_TABLE_DEFAULT_COUNTRY:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.defaultCountryTable);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.defaultCountryTable;
            break;

         case NV_TABLE_TPC_POWER_TABLE:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.plutCharacterized);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.plutCharacterized[0];
            break;

         case NV_TABLE_TPC_PDADC_OFFSETS:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.plutPdadcOffset);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.plutPdadcOffset[0];
            break;

         case NV_TABLE_VIRTUAL_RATE:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.pwrOptimum_virtualRate);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.pwrOptimum_virtualRate[0];
            break;

         case NV_TABLE_RSSI_CHANNEL_OFFSETS:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.rssiChanOffsets);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.rssiChanOffsets[0];
            break;

         case NV_TABLE_HW_CAL_VALUES:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.hwCalValues);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.hwCalValues;
            break;

         case NV_TABLE_FW_CONFIG:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.fwConfig);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.fwConfig;
            break;

         case NV_TABLE_ANTENNA_PATH_LOSS:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.antennaPathLoss);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.antennaPathLoss[0];
            break;

         case NV_TABLE_PACKET_TYPE_POWER_LIMITS:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.pktTypePwrLimits);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.pktTypePwrLimits[0][0];
            break;

         default:
            VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                       "Not Valid NV Table %d", nvTable->nvTable);
            return -EIO;
            break;
      }

      if (pHddCtx->ftm.targetNVTableSize != nvTable->tableSize)
      {
         /* Invalid table size, discard and initialize data */
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                    "Invalid Table Size %d for Table %d"
                    " expected size %d", nvTable->tableSize, nvTable->nvTable,
                    pHddCtx->ftm.targetNVTableSize);
         pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
         pHddCtx->ftm.targetNVTableSize    = 0;
         pHddCtx->ftm.processedNVTableSize = 0;
         vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
         return -EINVAL;
      }

      /* Set Current Processing NV table type */
      pHddCtx->ftm.processingNVTable = nvTable->nvTable;
      /* Copy target NV table value into temp context buffer */
      vos_mem_copy(pHddCtx->ftm.tempNVTableBuffer,
                   pHddCtx->ftm.targetNVTablePointer,
                   pHddCtx->ftm.targetNVTableSize);

   }

   if (pHddCtx->ftm.processingNVTable != nvTable->nvTable)
   {
      /* Invalid table type */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "Invalid NV Table, now Processing %d, not %d",
                  pHddCtx->ftm.processingNVTable, nvTable->nvTable);
      pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
      pHddCtx->ftm.targetNVTableSize    = 0;
      pHddCtx->ftm.processedNVTableSize = 0;
      vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);

      return -EINVAL;
   }

   /* Copy next chunk of NV table value into response buffer */
   vos_mem_copy(&nvTable->tableData,
                pHddCtx->ftm.tempNVTableBuffer + pHddCtx->ftm.processedNVTableSize,
                nvTable->chunkSize);
   /* Update processed pointer to prepare next chunk copy */
   pHddCtx->ftm.processedNVTableSize += nvTable->chunkSize;

   if (pHddCtx->ftm.targetNVTableSize == pHddCtx->ftm.processedNVTableSize)
   {
      /* Finished to process last chunk of data, initialize buffer */
      pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
      pHddCtx->ftm.targetNVTableSize    = 0;
      pHddCtx->ftm.processedNVTableSize = 0;
      vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
   }

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_set_nv_table() -
            Set Specific NV table as given
            NV V2 dedicated API

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             1, Process Host command success

  --------------------------------------------------------------------------*/
int wlan_hdd_ftm_set_nv_table
(
   hdd_context_t  *pHddCtx,
   tPttMsgbuffer  *ftmCmd
)
{
   VOS_STATUS          nvStatus = VOS_STATUS_SUCCESS;
   pttSetNvTable      *nvTable = (pttSetNvTable *)&ftmCmd->msgBody.SetNvTable;
   v_SIZE_t            nvSize;
   sHalNvV2           *nvContents = NULL;
   eNvVersionType      nvVersion;

   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "Not valid driver context");
      return -EINVAL;
   }

   nvVersion = vos_nv_getNvVersion();
   if (E_NV_V2 != nvVersion)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s : Not valid NV Version %d", __func__, nvVersion);
      return -EINVAL;
   }

   /* Test first chunk of NV table */
   if ((NV_MAX_TABLE == pHddCtx->ftm.processingNVTable) ||
       (0 == pHddCtx->ftm.processedNVTableSize))
   {
      nvStatus = vos_nv_getNVBuffer((void **)&nvContents, &nvSize);
      if ((VOS_STATUS_SUCCESS != nvStatus) || (NULL == nvContents))
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                FL("Fail to get cached NV value Status %d"), nvStatus);
         return -EINVAL;
      }

      switch (nvTable->nvTable)
      {
         case NV_TABLE_RATE_POWER_SETTINGS:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.pwrOptimum);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.pwrOptimum;
            break;

         case NV_TABLE_REGULATORY_DOMAINS:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.regDomains);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.regDomains;
            break;

         case NV_TABLE_DEFAULT_COUNTRY:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.defaultCountryTable);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.defaultCountryTable;
            break;

         case NV_TABLE_TPC_POWER_TABLE:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.plutCharacterized);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.plutCharacterized[0];
            break;

         case NV_TABLE_TPC_PDADC_OFFSETS:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.plutPdadcOffset);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.plutPdadcOffset[0];
            break;

         case NV_TABLE_VIRTUAL_RATE:
            pHddCtx->ftm.targetNVTableSize = sizeof(nvContents->tables.pwrOptimum_virtualRate);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.pwrOptimum_virtualRate[0];
            break;

         case NV_TABLE_RSSI_CHANNEL_OFFSETS:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.rssiChanOffsets);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.rssiChanOffsets[0];
            break;

         case NV_TABLE_HW_CAL_VALUES:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.hwCalValues);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.hwCalValues;
            break;

         case NV_TABLE_FW_CONFIG:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.fwConfig);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.fwConfig;
            break;

         case NV_TABLE_ANTENNA_PATH_LOSS:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.antennaPathLoss);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.antennaPathLoss[0];
            break;

         case NV_TABLE_PACKET_TYPE_POWER_LIMITS:
            pHddCtx->ftm.targetNVTableSize    = sizeof(nvContents->tables.pktTypePwrLimits);
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)&nvContents->tables.pktTypePwrLimits[0][0];
            break;

         default:
            VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                       "Not Valid NV Table %d", nvTable->nvTable);
            return -EIO;
            break;
      }

      /* Set Current Processing NV table type */
      pHddCtx->ftm.processingNVTable = nvTable->nvTable;
      if (pHddCtx->ftm.targetNVTableSize != nvTable->tableSize)
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                    "Invalid Table Size %d", nvTable->tableSize);
         pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
         pHddCtx->ftm.targetNVTableSize    = 0;
         pHddCtx->ftm.processedNVTableSize = 0;
         vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
         return -EINVAL;
      }
   }

   if (pHddCtx->ftm.processingNVTable != nvTable->nvTable)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "Invalid NV Table, now Processing %d, not %d",
                  pHddCtx->ftm.processingNVTable, nvTable->nvTable);
      pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
      pHddCtx->ftm.targetNVTableSize    = 0;
      pHddCtx->ftm.processedNVTableSize = 0;
      vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
      return -EINVAL;
   }
   vos_mem_copy(pHddCtx->ftm.tempNVTableBuffer + pHddCtx->ftm.processedNVTableSize,
                &nvTable->tableData,
                nvTable->chunkSize);

   pHddCtx->ftm.processedNVTableSize += nvTable->chunkSize;
   if (pHddCtx->ftm.targetNVTableSize == pHddCtx->ftm.processedNVTableSize)
   {
      vos_mem_copy(pHddCtx->ftm.targetNVTablePointer,
                   pHddCtx->ftm.tempNVTableBuffer,
                   pHddCtx->ftm.targetNVTableSize);
      pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
      pHddCtx->ftm.targetNVTableSize    = 0;
      pHddCtx->ftm.processedNVTableSize = 0;
      vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
   }

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_blank_nv() -
            Set all NV table value as default
            NV V2 dedicated API

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             0, Process Host command success

  --------------------------------------------------------------------------*/
int wlan_hdd_ftm_blank_nv_table
(
   tPttMsgbuffer  *ftmCmd
)
{
   VOS_STATUS          nvStatus = VOS_STATUS_SUCCESS;
   v_SIZE_t            nvSize;
   v_SIZE_t            itemSize;
   sHalNvV2           *nvContents = NULL;
   eNvVersionType      nvVersion;

   nvStatus = vos_nv_getNVBuffer((void **)&nvContents, &nvSize);
   if((VOS_STATUS_SUCCESS != nvStatus) || (NULL == nvContents))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                FL("Fail to get cached NV value Status %d"), nvStatus);
      return -EIO;
   }

   nvVersion = vos_nv_getNvVersion();
   if (E_NV_V2 != nvVersion)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s : Not valid NV Version %d", __func__, nvVersion);
      return -EINVAL;
   }

   itemSize = sizeof(nvContents->tables.pwrOptimum);
   memcpy(&nvContents->tables.pwrOptimum,
          &nvDefaultsV2.tables.pwrOptimum,
          itemSize);

   itemSize = sizeof(nvContents->tables.regDomains);
   memcpy(&nvContents->tables.regDomains,
          &nvDefaultsV2.tables.regDomains,
          itemSize);

   itemSize = sizeof(nvContents->tables.defaultCountryTable);
   memcpy(&nvContents->tables.defaultCountryTable,
          &nvDefaultsV2.tables.defaultCountryTable,
          itemSize);

   itemSize = sizeof(nvContents->tables.plutCharacterized);
   memcpy(&nvContents->tables.plutCharacterized[0],
          &nvDefaultsV2.tables.plutCharacterized[0],
          itemSize);

   itemSize = sizeof(nvContents->tables.plutPdadcOffset);
   memcpy(&nvContents->tables.plutPdadcOffset[0],
          &nvDefaultsV2.tables.plutPdadcOffset[0],
          itemSize);

   itemSize = sizeof(nvContents->tables.pwrOptimum_virtualRate);
   memcpy(&nvContents->tables.pwrOptimum_virtualRate[0],
          &nvDefaultsV2.tables.pwrOptimum_virtualRate[0],
          itemSize);

   itemSize = sizeof(nvContents->tables.rssiChanOffsets);
   memcpy(&nvContents->tables.rssiChanOffsets[0],
          &nvDefaultsV2.tables.rssiChanOffsets[0],
          itemSize);

   itemSize = sizeof(nvContents->tables.hwCalValues);
   memcpy(&nvContents->tables.hwCalValues,
          &nvDefaultsV2.tables.hwCalValues,
          itemSize);

   itemSize = sizeof(nvContents->tables.antennaPathLoss);
   memcpy(&nvContents->tables.antennaPathLoss[0],
          &nvDefaultsV2.tables.antennaPathLoss[0],
          itemSize);

   itemSize = sizeof(nvContents->tables.pktTypePwrLimits);
   memcpy(&nvContents->tables.pktTypePwrLimits[0][0],
          &nvDefaultsV2.tables.pktTypePwrLimits[0][0],
          itemSize);

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_delete_nv_table() -
            Delete Specific NV table
            NV V2 dedicated API

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             1, Process Host command success

  --------------------------------------------------------------------------*/
int wlan_hdd_ftm_delete_nv_table
(
   tPttMsgbuffer  *ftmCmd
)
{
   VOS_STATUS          nvStatus = VOS_STATUS_SUCCESS;
   tMsgPttDelNvTable  *nvTable = (tMsgPttDelNvTable *)&ftmCmd->msgBody.DelNvTable;
   v_SIZE_t            nvSize;
   v_SIZE_t            itemSize;
   sHalNvV2           *nvContents = NULL;
   eNvVersionType      nvVersion;

   nvStatus = vos_nv_getNVBuffer((void **)&nvContents, &nvSize);
   if ((VOS_STATUS_SUCCESS != nvStatus) || (NULL == nvContents))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                FL("Fail to get cached NV value Status %d"), nvStatus);
      return -EIO;
   }

   nvVersion = vos_nv_getNvVersion();
   if (E_NV_V2 != nvVersion)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s : Not valid NV Version %d", __func__, nvVersion);
      return -EINVAL;
   }

   switch (nvTable->nvTable)
   {
      case NV_TABLE_RATE_POWER_SETTINGS:
         itemSize = sizeof(nvContents->tables.pwrOptimum);
         memcpy(&nvContents->tables.pwrOptimum,
                &nvDefaultsV2.tables.pwrOptimum,
                itemSize);
         break;

      case NV_TABLE_REGULATORY_DOMAINS:
         itemSize = sizeof(nvContents->tables.regDomains);
         memcpy(&nvContents->tables.regDomains,
                &nvDefaultsV2.tables.regDomains,
                itemSize);
         break;

      case NV_TABLE_DEFAULT_COUNTRY:
         itemSize = sizeof(nvContents->tables.defaultCountryTable);
         memcpy(&nvContents->tables.defaultCountryTable,
                &nvDefaultsV2.tables.defaultCountryTable,
                itemSize);
         break;

      case NV_TABLE_TPC_POWER_TABLE:
         itemSize = sizeof(nvContents->tables.plutCharacterized);
         memcpy(&nvContents->tables.plutCharacterized[0],
                &nvDefaultsV2.tables.plutCharacterized[0],
                itemSize);
         break;

      case NV_TABLE_TPC_PDADC_OFFSETS:
         itemSize = sizeof(nvContents->tables.plutPdadcOffset);
         memcpy(&nvContents->tables.plutPdadcOffset[0],
                &nvDefaultsV2.tables.plutPdadcOffset[0],
                itemSize);
         break;

      case NV_TABLE_VIRTUAL_RATE:
         itemSize = sizeof(nvContents->tables.pwrOptimum_virtualRate);
         memcpy(&nvContents->tables.pwrOptimum_virtualRate[0],
                &nvDefaultsV2.tables.pwrOptimum_virtualRate[0],
                itemSize);
         break;

      case NV_TABLE_RSSI_CHANNEL_OFFSETS:
         itemSize = sizeof(nvContents->tables.rssiChanOffsets);
         memcpy(&nvContents->tables.rssiChanOffsets[0],
                &nvDefaultsV2.tables.rssiChanOffsets[0],
                itemSize);
         break;

      case NV_TABLE_HW_CAL_VALUES:
         itemSize = sizeof(nvContents->tables.hwCalValues);
         memcpy(&nvContents->tables.hwCalValues,
                &nvDefaultsV2.tables.hwCalValues,
                itemSize);
         break;

      case NV_TABLE_FW_CONFIG:
         itemSize = sizeof(nvContents->tables.fwConfig);
         memcpy(&nvContents->tables.fwConfig,
                &nvDefaultsV2.tables.fwConfig,
                itemSize);
         break;

      case NV_TABLE_ANTENNA_PATH_LOSS:
         itemSize = sizeof(nvContents->tables.antennaPathLoss);
         memcpy(&nvContents->tables.antennaPathLoss[0],
                &nvDefaultsV2.tables.antennaPathLoss[0],
                itemSize);
         break;

      case NV_TABLE_PACKET_TYPE_POWER_LIMITS:
         itemSize = sizeof(nvContents->tables.pktTypePwrLimits);
         memcpy(&nvContents->tables.pktTypePwrLimits[0][0],
                &nvDefaultsV2.tables.pktTypePwrLimits[0][0],
                itemSize);
         break;

      default:
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "Not Valid NV Table %d", nvTable->nvTable);
         return -EIO;
         break;
   }

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_get_nv_field() -
            Get Specific NV field

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             1, Process Host command success

  --------------------------------------------------------------------------*/
int wlan_hdd_ftm_get_nv_field
(
   tPttMsgbuffer  *ftmCmd
)
{
   sNvFields           nvFieldDataBuffer;
   tMsgPttGetNvField  *nvField = (tMsgPttGetNvField *)&ftmCmd->msgBody.GetNvField;
   VOS_STATUS          nvStatus = VOS_STATUS_SUCCESS;
   sHalNv             *nvContents = NULL;
   v_SIZE_t            nvSize;

   nvStatus = vos_nv_getNVBuffer((void **)&nvContents, &nvSize);
   if ((VOS_STATUS_SUCCESS != nvStatus) || (NULL == nvContents))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                FL("Fail to get cached NV value Status %d"), nvStatus);
      return -EIO;
   }
   memcpy(&nvFieldDataBuffer, &nvContents->fields, sizeof(sNvFields));

   switch (nvField->nvField)
   {
      case NV_COMMON_PRODUCT_ID:
         memcpy((void *)&nvField->fieldData,
             &nvFieldDataBuffer.productId,
             sizeof(nvFieldDataBuffer.productId));
         break;

      case NV_COMMON_PRODUCT_BANDS:
         memcpy((void *)&nvField->fieldData,
             &nvFieldDataBuffer.productBands,
             sizeof(nvFieldDataBuffer.productBands));
         break;

      case NV_COMMON_NUM_OF_TX_CHAINS:
         memcpy((void *)&nvField->fieldData,
             &nvFieldDataBuffer.numOfTxChains,
             sizeof(nvFieldDataBuffer.numOfTxChains));
         break;

      case NV_COMMON_NUM_OF_RX_CHAINS:
         memcpy((void *)&nvField->fieldData,
             &nvFieldDataBuffer.numOfRxChains,
             sizeof(nvFieldDataBuffer.numOfRxChains));
         break;

      case NV_COMMON_MAC_ADDR:
         memcpy((void *)&nvField->fieldData,
             &nvFieldDataBuffer.macAddr[0],
             NV_FIELD_MAC_ADDR_SIZE);
         break;

      case NV_COMMON_MFG_SERIAL_NUMBER:
         memcpy((void *)&nvField->fieldData,
             &nvFieldDataBuffer.mfgSN[0],
             NV_FIELD_MFG_SN_SIZE);
         break;

      case NV_COMMON_WLAN_NV_REV_ID:
         memcpy((void *)&nvField->fieldData,
             &nvFieldDataBuffer.wlanNvRevId,
             sizeof(nvFieldDataBuffer.wlanNvRevId));
         break;

      case NV_COMMON_COUPLER_TYPE:
         memcpy((void *)&nvField->fieldData,
                &nvFieldDataBuffer.couplerType,
                sizeof(nvFieldDataBuffer.couplerType));
         break;

      case NV_COMMON_NV_VERSION:
         {
            VOS_STATUS nvEmbededStatus = VOS_STATUS_SUCCESS;
            v_U8_t nvVersion = nvFieldDataBuffer.nvVersion;

            nvEmbededStatus = vos_nv_isEmbeddedNV();

            if ( nvEmbededStatus == VOS_STATUS_SUCCESS )
            {
                // High bit is set to indicate embedded NV..
                nvVersion = nvVersion | NV_EMBEDDED_VERSION;
            }

            memcpy((void *)&nvField->fieldData,
                   &nvVersion,
                   sizeof(nvFieldDataBuffer.nvVersion));
         }
         break;

      default:
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "Not Valid NV field %d", nvField->nvField);
         return -EIO;
         break;
   }

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_set_nv_field() -
            Set Specific NV field

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             1, Process Host command success

  --------------------------------------------------------------------------*/
int wlan_hdd_ftm_set_nv_field
(
   tPttMsgbuffer  *ftmCmd
)
{
   tMsgPttSetNvField *nvField = (tMsgPttSetNvField *)&ftmCmd->msgBody.SetNvField;
   VOS_STATUS         nvStatus = VOS_STATUS_SUCCESS;
   v_SIZE_t           nvSize;
   sHalNv            *nvContents = NULL;
   v_U8_t             macLoop;
   v_U8_t            *pNVMac;
   v_U8_t             lastByteMAC;


   nvStatus = vos_nv_getNVBuffer((void **)&nvContents, &nvSize);
   if((VOS_STATUS_SUCCESS != nvStatus) || (NULL == nvContents))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                FL("Fail to get cached NV value Status %d"), nvStatus);
      return -EIO;
   }

   switch (nvField->nvField)
   {
      case NV_COMMON_PRODUCT_ID:
         memcpy(&nvContents->fields.productId,
                &nvField->fieldData,
                sizeof(nvContents->fields.productId));
         break;

      case NV_COMMON_PRODUCT_BANDS:
         memcpy(&nvContents->fields.productBands,
                &nvField->fieldData,
                sizeof(nvContents->fields.productBands));
         break;

      case NV_COMMON_NUM_OF_TX_CHAINS:
         memcpy(&nvContents->fields.numOfTxChains,
                &nvField->fieldData,
                sizeof(nvContents->fields.numOfTxChains));
         break;

      case NV_COMMON_NUM_OF_RX_CHAINS:
         memcpy(&nvContents->fields.numOfRxChains,
                &nvField->fieldData,
                sizeof(nvContents->fields.numOfRxChains));
         break;

      case NV_COMMON_MAC_ADDR:
         /* If Last byte is larger than 252 (0xFC), return Error,
          * Since 3MACs should be derived from first MAC */
         if(QWLAN_MAX_MAC_LAST_BYTE_VALUE <
            nvField->fieldData.macAddr.macAddr1[VOS_MAC_ADDRESS_LEN - 1])
         {
            VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                       "Last Byte of the seed MAC is too large 0x%x",
                        nvField->fieldData.macAddr.macAddr1[VOS_MAC_ADDRESS_LEN - 1]);
            return -EILSEQ;
         }

         pNVMac = (v_U8_t *)nvContents->fields.macAddr;
         lastByteMAC = nvField->fieldData.macAddr.macAddr1[VOS_MAC_ADDRESS_LEN - 1];
         for(macLoop = 0; macLoop < VOS_MAX_CONCURRENCY_PERSONA; macLoop++)
         {
            nvField->fieldData.macAddr.macAddr1[VOS_MAC_ADDRESS_LEN - 1] =
                                               lastByteMAC + macLoop;
            vos_mem_copy(pNVMac + (macLoop * NV_FIELD_MAC_ADDR_SIZE),
                         &nvField->fieldData.macAddr.macAddr1[0],
                         NV_FIELD_MAC_ADDR_SIZE);
         }
         break;

      case NV_COMMON_MFG_SERIAL_NUMBER:
         memcpy(&nvContents->fields.mfgSN[0],
                &nvField->fieldData,
             NV_FIELD_MFG_SN_SIZE);
         break;

     case NV_COMMON_WLAN_NV_REV_ID:
        memcpy(&nvContents->fields.wlanNvRevId,
               &nvField->fieldData,
               sizeof(nvContents->fields.wlanNvRevId));
        break;

      case NV_COMMON_COUPLER_TYPE:
         memcpy(&nvContents->fields.couplerType,
                &nvField->fieldData,
                sizeof(nvContents->fields.couplerType));
         break;

      case NV_COMMON_NV_VERSION:
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "Cannot modify NV version field %d", nvField->nvField);
         return -EIO;
         break;

      default:
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "Not Valid NV field %d", nvField->nvField);
         return -EIO;
         break;
   }

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_store_nv_table() -
            Store Cached NV information into Flash Memory, file
            NV V2 dedicated API

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             0, Process Host command success

  --------------------------------------------------------------------------*/
int wlan_hdd_ftm_store_nv_table
(
   tPttMsgbuffer  *ftmCmd
)
{
   VOS_STATUS           nvStatus = VOS_STATUS_SUCCESS;
   v_SIZE_t             nvSize;
   tMsgPttStoreNvTable *nvTable = (tMsgPttStoreNvTable *)&ftmCmd->msgBody.StoreNvTable;
   void                *tablePtr = NULL;
   unsigned int         tableSize = 0;
   VNV_TYPE             tableVNVType = VNV_FIELD_IMAGE;
   sHalNvV2            *nvContents = NULL;
   eNvVersionType       nvVersion;

   nvStatus = vos_nv_getNVBuffer((void **)&nvContents, &nvSize);
   if((VOS_STATUS_SUCCESS != nvStatus) || (NULL == nvContents))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                FL("Fail to get cached NV value Status %d"), nvStatus);
      return -EIO;
   }

   nvVersion = vos_nv_getNvVersion();
   if (E_NV_V2 != nvVersion)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s : Not valid NV Version %d", __func__, nvVersion);
      return -EINVAL;
   }

   /* Set Platform type as PRIMA */
   nvContents->fields.wlanNvRevId = 2;

   switch(nvTable->nvTable)
   {
      case NV_FIELDS_IMAGE:
         tablePtr     = (void *)&nvContents->fields;
         tableSize    = sizeof(nvContents->fields);
         tableVNVType = VNV_FIELD_IMAGE;
         break;

      case NV_TABLE_RATE_POWER_SETTINGS:
         tablePtr     = (void *)&nvContents->tables.pwrOptimum[0];
         tableSize    = sizeof(nvContents->tables.pwrOptimum);
         tableVNVType = VNV_RATE_TO_POWER_TABLE;
         break;

      case NV_TABLE_REGULATORY_DOMAINS:
         tablePtr     = (void *)&nvContents->tables.regDomains[0];
         tableSize    = sizeof(nvContents->tables.regDomains);
         tableVNVType = VNV_REGULARTORY_DOMAIN_TABLE;
         break;

      case NV_TABLE_DEFAULT_COUNTRY:
         tablePtr     = (void *)&nvContents->tables.defaultCountryTable;
         tableSize    = sizeof(nvContents->tables.defaultCountryTable);
         tableVNVType = VNV_DEFAULT_LOCATION;
         break;

      case NV_TABLE_TPC_POWER_TABLE:
         tablePtr     = (void *)&nvContents->tables.plutCharacterized[0];
         tableSize    = sizeof(nvContents->tables.plutCharacterized);
         tableVNVType = VNV_TPC_POWER_TABLE;
         break;

      case NV_TABLE_TPC_PDADC_OFFSETS:
         tablePtr     = (void *)&nvContents->tables.plutPdadcOffset[0];
         tableSize    = sizeof(nvContents->tables.plutPdadcOffset);
         tableVNVType = VNV_TPC_PDADC_OFFSETS;
         break;

      case NV_TABLE_VIRTUAL_RATE:
         tablePtr     = (void *)&nvContents->tables.pwrOptimum_virtualRate[0];
         tableSize    = sizeof(nvContents->tables.pwrOptimum_virtualRate);
         tableVNVType = VNV_TABLE_VIRTUAL_RATE;
         break;

      case NV_TABLE_RSSI_CHANNEL_OFFSETS:
         tablePtr     = (void *)&nvContents->tables.rssiChanOffsets[0];
         tableSize    = sizeof(nvContents->tables.rssiChanOffsets);
         tableVNVType = VNV_RSSI_CHANNEL_OFFSETS;
         break;

      case NV_TABLE_HW_CAL_VALUES:
         tablePtr     = (void *)&nvContents->tables.hwCalValues;
         tableSize    = sizeof(nvContents->tables.hwCalValues);
         tableVNVType = VNV_HW_CAL_VALUES;
         break;

      case NV_TABLE_FW_CONFIG:
         tablePtr     = (void *)&nvContents->tables.fwConfig;
         tableSize    = sizeof(nvContents->tables.fwConfig);
         tableVNVType = VNV_FW_CONFIG;
         break;

      case NV_TABLE_ANTENNA_PATH_LOSS:
         tablePtr     = (void *)&nvContents->tables.antennaPathLoss[0];
         tableSize    = sizeof(nvContents->tables.antennaPathLoss);
         tableVNVType = VNV_ANTENNA_PATH_LOSS;
         break;

      case NV_TABLE_PACKET_TYPE_POWER_LIMITS:
         tablePtr     = (void *)&nvContents->tables.pktTypePwrLimits[0][0];
         tableSize    = sizeof(nvContents->tables.pktTypePwrLimits);
         tableVNVType = VNV_PACKET_TYPE_POWER_LIMITS;
         break;

      default:
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "Not Supported Table Type %d", nvTable->nvTable);
         return -EIO;
         break;

   }

   nvStatus = vos_nv_write(tableVNVType,
                           tablePtr,
                           tableSize);
   if(VOS_STATUS_SUCCESS != nvStatus)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "Failed update nv item %d", nvStatus);
      return -EIO;
   }

   return 1;
}

/* --------------------------------------------------------------------------
  \brief wlan_hdd_ftm_get_nv_bin() -
            Get NV bin read from Flash Memory, file

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             0, Process Host command success
--------------------------------------------------------------------------*/

static int wlan_hdd_ftm_get_nv_bin
(
   v_U16_t msgId,
   hdd_context_t  *pHddCtx,
   tPttMsgbuffer  *ftmCmd
)
{
   VOS_STATUS          nvStatus = VOS_STATUS_SUCCESS;
   pttGetNvTable      *nvTable = (pttGetNvTable *)&ftmCmd->msgBody.GetNvBin;
   v_SIZE_t            nvSize;
   v_U8_t             *nvContents;
   v_U16_t offset = 0;
   eNvVersionType      nvVersion;

   nvVersion = vos_nv_getNvVersion();
   if (E_NV_V3 != nvVersion)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s : Not valid NV Version %d", __func__, nvVersion);
      return -EINVAL;
   }

   if ((NV_MAX_TABLE == pHddCtx->ftm.processingNVTable) ||
      (0 == pHddCtx->ftm.processedNVTableSize))
   {
      if ( msgId == PTT_MSG_GET_NV_BIN )
      {
         nvStatus = vos_nv_getNVEncodedBuffer((void **)&nvContents, &nvSize);
      }
      else
      {
         nvStatus = vos_nv_getNVDictionary((void **)&nvContents, &nvSize);
      }

      if ((VOS_STATUS_SUCCESS != nvStatus) || (NULL == nvContents))
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  FL("Fail to get cached NV value Status %d"), nvStatus);
         return -EIO;
      }

      switch (nvTable->nvTable)
      {
         case NV_BINARY_IMAGE:
            pHddCtx->ftm.targetNVTablePointer = (v_U8_t *)nvContents;
            break;
         default:
            VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                       "Not Valid NV Table %d", nvTable->nvTable);
            return -EIO;
            break;
      }

      /* Set Current Processing NV table type */
      pHddCtx->ftm.processingNVTable = nvTable->nvTable;
      if ( msgId == PTT_MSG_GET_NV_BIN )
      {
         pHddCtx->ftm.targetNVTableSize = nvSize + sizeof(v_U32_t);
         /* Validity Period */
         pHddCtx->ftm.tempNVTableBuffer[0] = 0xFF;
         pHddCtx->ftm.tempNVTableBuffer[1] = 0xFF;
         pHddCtx->ftm.tempNVTableBuffer[2] = 0xFF;
         pHddCtx->ftm.tempNVTableBuffer[3] = 0xFF;
         offset = sizeof(v_U32_t);
      }
      else
      {
         pHddCtx->ftm.targetNVTableSize = nvSize;
         offset = 0;
      }

      /* Copy target NV table value into temp context buffer */
      vos_mem_copy(&pHddCtx->ftm.tempNVTableBuffer[offset],
                   pHddCtx->ftm.targetNVTablePointer,
                   pHddCtx->ftm.targetNVTableSize);
   }


   if (pHddCtx->ftm.processingNVTable != nvTable->nvTable)
   {
      /* Invalid table type */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "Invalid NV Table, now Processing %d, not %d",
                  pHddCtx->ftm.processingNVTable, nvTable->nvTable);

      pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
      pHddCtx->ftm.targetNVTableSize    = 0;
      pHddCtx->ftm.processedNVTableSize = 0;
      vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);

      return -EINVAL;
   }

   nvTable->tableSize = pHddCtx->ftm.targetNVTableSize;

   /* Update processed pointer to prepare next chunk copy */
   if ( (nvTable->chunkSize + pHddCtx->ftm.processedNVTableSize) >
          pHddCtx->ftm.targetNVTableSize )
   {
      nvTable->chunkSize =
          (pHddCtx->ftm.targetNVTableSize - pHddCtx->ftm.processedNVTableSize);
   }

   /* Copy next chunk of NV table value into response buffer */
   vos_mem_copy(
        &nvTable->tableData,
        pHddCtx->ftm.tempNVTableBuffer + pHddCtx->ftm.processedNVTableSize,
        nvTable->chunkSize);

   pHddCtx->ftm.processedNVTableSize += nvTable->chunkSize;

   if (pHddCtx->ftm.targetNVTableSize == pHddCtx->ftm.processedNVTableSize)
   {
      /* Finished to process last chunk of data, initialize buffer */
      pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
      pHddCtx->ftm.targetNVTableSize    = 0;
      pHddCtx->ftm.processedNVTableSize = 0;
      vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
   }

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_set_nv_bin() -
            Set NV bin to Flash Memory, file

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             0, Process Host command success

+----------------------------------------------------------------------------*/

static int wlan_hdd_ftm_set_nv_bin
(
   hdd_context_t  *pHddCtx,
   tPttMsgbuffer  *ftmCmd
)
{
   VOS_STATUS          nvStatus = VOS_STATUS_SUCCESS;
   pttSetNvTable      *nvTable = (pttSetNvTable *)&ftmCmd->msgBody.SetNvBin;
   eNvVersionType      nvVersion;

   nvVersion = vos_nv_getNvVersion();
   if (E_NV_V3 != nvVersion)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s : Not valid NV Version %d", __func__, nvVersion);
      return -EINVAL;
   }


   /* Test first chunk of NV table */
   if ((NV_MAX_TABLE == pHddCtx->ftm.processingNVTable) ||
       (0 == pHddCtx->ftm.processedNVTableSize))
   {
      switch (nvTable->nvTable)
      {
         case NV_BINARY_IMAGE:
            pHddCtx->ftm.targetNVTableSize = nvTable->tableSize;
            break;
         default:
            VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                       "Not Valid NV Table %d", nvTable->nvTable);
            return -EIO;
            break;
      }

      /* Set Current Processing NV table type */
      pHddCtx->ftm.processingNVTable = nvTable->nvTable;
      pHddCtx->ftm.processedNVTableSize = 0;

      if (pHddCtx->ftm.targetNVTableSize != nvTable->tableSize)
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                    "Invalid Table Size %d", nvTable->tableSize);
         pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
         pHddCtx->ftm.targetNVTableSize    = 0;
         pHddCtx->ftm.processedNVTableSize = 0;
         vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
         return -EINVAL;
      }
   }

   if (pHddCtx->ftm.processingNVTable != nvTable->nvTable)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "Invalid NV Table, now Processing %d, not %d",
                  pHddCtx->ftm.processingNVTable, nvTable->nvTable);
      pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
      pHddCtx->ftm.targetNVTableSize    = 0;
      pHddCtx->ftm.processedNVTableSize = 0;
      vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
      return -EINVAL;
   }

   vos_mem_copy(
       pHddCtx->ftm.tempNVTableBuffer + pHddCtx->ftm.processedNVTableSize,
       &nvTable->tableData,
       nvTable->chunkSize);

   pHddCtx->ftm.processedNVTableSize += nvTable->chunkSize;

   if (pHddCtx->ftm.targetNVTableSize == pHddCtx->ftm.processedNVTableSize)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "Processing Done!! write encoded Buffer %d",
          pHddCtx->ftm.targetNVTableSize);

      nvStatus = wlan_write_to_efs ((v_U8_t*)pHddCtx->ftm.tempNVTableBuffer,
                   (v_U16_t)pHddCtx->ftm.targetNVTableSize);

      if ((VOS_STATUS_SUCCESS != nvStatus))
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                    "Fail to set NV Binary %d", nvStatus);
         return -EIO;
      }

      nvStatus = vos_nv_setNVEncodedBuffer(
            (v_U8_t*)pHddCtx->ftm.tempNVTableBuffer,
            (v_SIZE_t)pHddCtx->ftm.targetNVTableSize);

      if ((VOS_STATUS_SUCCESS != nvStatus))
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                    "Fail to set NV Binary %d", nvStatus);
         return -EIO;
      }

      pHddCtx->ftm.processingNVTable    = NV_MAX_TABLE;
      pHddCtx->ftm.targetNVTableSize    = 0;
      pHddCtx->ftm.processedNVTableSize = 0;
      vos_mem_zero(pHddCtx->ftm.tempNVTableBuffer, MAX_NV_TABLE_SIZE);
   }

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_temp_get_rel_num() -
            Get internal release number

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            -1, Process Host command fail, vail out
             0, Process Host command success

  --------------------------------------------------------------------------*/
int wlan_hdd_ftm_temp_get_rel_num
(
   tPttMsgbuffer  *ftmCmd
)
{
   tMsgPttGetBuildReleaseNumber *relNum = (tMsgPttGetBuildReleaseNumber *)&ftmCmd->msgBody.GetBuildReleaseNumber;

   relNum->relParams.drvMjr = QWLAN_VERSION_MAJOR;
   relNum->relParams.drvMnr = QWLAN_VERSION_MINOR;
   relNum->relParams.drvPtch = QWLAN_VERSION_PATCH;
   relNum->relParams.drvBld = QWLAN_VERSION_BUILD;
   relNum->relParams.pttMax = 10;
   relNum->relParams.pttMin = 1;

   return 1;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_process_ftm_host_cmd() -
            process any command should be handled within host.
            decide any command should be send to HAL or not

  \param  - ftmCmd - Pointer FTM Commad Buffer

  \return - int
            < 0, Process Host command fail, bail out
             0, Process Host command success, not need to send CMD to HAL
             1, Process Host command success, need to send CMD to HAL

  --------------------------------------------------------------------------*/
int wlan_hdd_process_ftm_host_cmd
(
   hdd_context_t *pHddCtx,
   void *ftmCmd
)
{
   tPttMsgbuffer *pFTMCmd = (tPttMsgbuffer *)ftmCmd;
   int            needToRouteHal = 1;
   int            hostState = 1;

   switch(pFTMCmd->msgId)
   {
      case PTT_MSG_GET_NV_TABLE:
         hostState = wlan_hdd_ftm_get_nv_table(pHddCtx, pFTMCmd);
         needToRouteHal = 0;
         break;

      case PTT_MSG_SET_NV_TABLE:
         hostState = wlan_hdd_ftm_set_nv_table(pHddCtx, pFTMCmd);
         /* Temp NV Operation will be isolated to host
         needToRouteHal = 1; */
         needToRouteHal = 0;
         break;

      case PTT_MSG_BLANK_NV:
         hostState = wlan_hdd_ftm_blank_nv_table(pFTMCmd);
         needToRouteHal = 1;
         break;

      case PTT_MSG_DEL_NV_TABLE:
         hostState = wlan_hdd_ftm_delete_nv_table(pFTMCmd);
         needToRouteHal = 1;
         break;

      case PTT_MSG_GET_NV_FIELD:
         hostState = wlan_hdd_ftm_get_nv_field(pFTMCmd);
         needToRouteHal = 0;
         break;

      case PTT_MSG_SET_NV_FIELD:
         hostState = wlan_hdd_ftm_set_nv_field(pFTMCmd);
         needToRouteHal = 0;
         break;

      case PTT_MSG_STORE_NV_TABLE:
         hostState = wlan_hdd_ftm_store_nv_table(pFTMCmd);
         needToRouteHal = 0;
         break;

      case PTT_MSG_GET_NV_BIN:
      case PTT_MSG_GET_DICTIONARY:
         hostState = wlan_hdd_ftm_get_nv_bin(pFTMCmd->msgId, pHddCtx, pFTMCmd);
         needToRouteHal = 0;
         break;

      case PTT_MSG_SET_NV_BIN:
         hostState = wlan_hdd_ftm_set_nv_bin(pHddCtx, pFTMCmd);
         needToRouteHal = 0;
         break;

      case PTT_MSG_DBG_READ_REGISTER:
         wpalReadRegister(pFTMCmd->msgBody.DbgReadRegister.regAddr,
                          &pFTMCmd->msgBody.DbgReadRegister.regValue);
         needToRouteHal = 0;
         break;

      case PTT_MSG_DBG_WRITE_REGISTER:
         wpalWriteRegister(pFTMCmd->msgBody.DbgWriteRegister.regAddr,
                           pFTMCmd->msgBody.DbgWriteRegister.regValue);
         needToRouteHal = 0;
         break;

      case PTT_MSG_DBG_READ_MEMORY:
         wpalReadDeviceMemory(pFTMCmd->msgBody.DbgReadMemory.memAddr,
                              (unsigned char *)pFTMCmd->msgBody.DbgReadMemory.pMemBuf,
                              pFTMCmd->msgBody.DbgReadMemory.nBytes);
         needToRouteHal = 0;
         break;

      case PTT_MSG_DBG_WRITE_MEMORY:
         wpalWriteDeviceMemory(pFTMCmd->msgBody.DbgWriteMemory.memAddr,
                               (unsigned char *)pFTMCmd->msgBody.DbgWriteMemory.pMemBuf,
                               pFTMCmd->msgBody.DbgWriteMemory.nBytes);
         needToRouteHal = 0;
         break;

      case PTT_MSG_GET_BUILD_RELEASE_NUMBER:
         wlan_hdd_ftm_temp_get_rel_num(pFTMCmd);
         needToRouteHal = 0;
         break;

      default:
         needToRouteHal = 1;
         break;
   }

   if( 0 > hostState)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "Host Command Handle Fail, Bailout");
      return hostState;
   }

   return needToRouteHal;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_process_ftm_cmd() -

   This function process the commands received from the ptt socket application.

  \param  - pAdapter - Pointer HDD Context.

  \param  - wnl - Pointer to the ANI netlink header.

  \return - none

  --------------------------------------------------------------------------*/

void wlan_hdd_process_ftm_cmd
(
    hdd_context_t *pHddCtx,
    tAniNlHdr *wnl
)
{
    wlan_hdd_ftm_request_t  *pRequestBuf = (wlan_hdd_ftm_request_t*)(((v_U8_t*)(&wnl->wmsg))+sizeof(tAniHdr)) ;
    v_U16_t   cmd_len;
    v_U8_t *pftm_data;
    pVosContextType pVosContext = (pVosContextType)(pHddCtx->pvosContext);
    int hostState;
    tPttMsgbuffer *tempRspBuffer = NULL;
    static int count;

    ENTER();

    //Delay to fix NV write failure on JB
    vos_busy_wait(10000); //10ms

    if (!pRequestBuf) {

        hddLog(VOS_TRACE_LEVEL_ERROR,"%s: request buffer is null",__func__);
        return ;
    }

    if (vos_is_load_unload_in_progress(VOS_MODULE_ID_HDD, NULL))
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
        "%s: Load/Unload in Progress. Ignoring FTM Command %d"
        , __func__, pRequestBuf->ftmpkt.ftm_cmd_type);
        return ;
    }

    /*Save the received request*/
    pHddCtx->ftm.pRequestBuf = pRequestBuf;

    pHddCtx->ftm.pResponseBuf = (wlan_hdd_ftm_response_t*)pRequestBuf;
     /*Save the received request netlink header used for sending the response*/
    pHddCtx->ftm.wnl = wnl;
    if (pRequestBuf->module_type != QUALCOMM_MODULE_TYPE) {

        hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Invalid Module Type =%d",__func__,pRequestBuf->module_type);

        pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
        wlan_ftm_send_response(pHddCtx);
        return ;
    }

    if (VOS_FTM_MODE != hdd_get_conparam())
    {
        count++;
        if (count == 1 || !(count % 10))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Driver not loaded in FTM"
                " mode, current mode: %d ",__func__, hdd_get_conparam());
        }
        pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
        wlan_ftm_send_response(pHddCtx);
        return ;
    }

    switch (pRequestBuf->ftmpkt.ftm_cmd_type)
    {
    case WLAN_FTM_START:
        if (pHddCtx->ftm.ftm_state == WLAN_FTM_STARTED) {

            hddLog(VOS_TRACE_LEVEL_ERROR,"%s: FTM has already started =%d",__func__,pRequestBuf->ftmpkt.ftm_cmd_type);
            pHddCtx->ftm.pResponseBuf->ftm_hdr.data_len -= 1;
            pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_SUCCESS;
            wlan_ftm_send_response(pHddCtx);
            return;
        }

        if (wlan_hdd_ftm_start(pVosContext->pHDDContext) != VOS_STATUS_SUCCESS)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: : Failed to start WLAN FTM"
                   ,__func__);
            pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
            wlan_ftm_send_response(pHddCtx);
            complete(&pHddCtx->ftm.startCmpVar);
            return;
        }
        /* Ptt application running on the host PC expects the length to be one byte less that what we have received*/
        pHddCtx->ftm.pResponseBuf->ftm_hdr.data_len -= 1;
        pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_SUCCESS;
        pHddCtx->ftm.pResponseBuf->ftmpkt.ftm_cmd_type = 0;

        wlan_ftm_send_response(pHddCtx);
        complete(&pHddCtx->ftm.startCmpVar);
        break;

    case WLAN_FTM_STOP:
        if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED) {

            hddLog(VOS_TRACE_LEVEL_ERROR,"%s:: FTM has not started",__func__);
            pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_SUCCESS;
            wlan_ftm_send_response(pHddCtx);
            return;
        }

        if (VOS_STATUS_SUCCESS != wlan_ftm_stop(pHddCtx)) {

            pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
            wlan_ftm_send_response(pHddCtx);
            return;
        }

        pHddCtx->ftm.ftm_state = WLAN_FTM_STOPPED;
        /* This would send back the Command Success Status */
        pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_SUCCESS;

        wlan_ftm_send_response(pHddCtx);

        break;

    case WLAN_FTM_CMD:
        /* if it is regular FTM command, pass it to HAL PHY */
        if(pHddCtx->ftm.IsCmdPending == TRUE) {
            hddLog(VOS_TRACE_LEVEL_ERROR,"%s:: FTM command pending for process",__func__);
            return;
        }
        if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED) {

            hddLog(VOS_TRACE_LEVEL_ERROR,"%s:: FTM has not started",__func__);

            pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
            wlan_ftm_send_response(pHddCtx);
            return;

        }
        vos_event_reset(&pHddCtx->ftm.ftm_vos_event);
        cmd_len = pRequestBuf->ftm_hdr.data_len;
        cmd_len -= (sizeof(wlan_hdd_ftm_request_t)- sizeof(pRequestBuf->ftmpkt.ftm_cmd_type));
        pftm_data = pRequestBuf->ftmpkt.pFtmCmd;

        hostState = wlan_hdd_process_ftm_host_cmd(pHddCtx, pftm_data);
        if (0 == hostState)
        {
           tempRspBuffer = (tPttMsgbuffer *)vos_mem_malloc(((tPttMsgbuffer *)pftm_data)->msgBodyLength);
           if (NULL == tempRspBuffer)
           {
              hddLog(VOS_TRACE_LEVEL_ERROR,
                     "%s:: temp Mem Alloc Fail",__func__);
              pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
              wlan_ftm_send_response(pHddCtx);
              return;
           }
           memcpy(tempRspBuffer, pftm_data, ((tPttMsgbuffer *)pftm_data)->msgBodyLength);
           tempRspBuffer->msgResponse = PTT_STATUS_SUCCESS;
           memcpy((unsigned char *)&pHddCtx->ftm.pResponseBuf->ftmpkt,
                  (unsigned char *) tempRspBuffer,
                  tempRspBuffer->msgBodyLength);
           pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_SUCCESS;
           wlan_ftm_send_response(pHddCtx);
           vos_mem_free(tempRspBuffer);
           return;
        }
        else if (0 > hostState)
        {
           hddLog(VOS_TRACE_LEVEL_ERROR, "*** Host Command Handle Fail ***");
           pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
           wlan_ftm_send_response(pHddCtx);
           return;
        }

        //HEXDUMP("Request:",(char*)pftm_data,cmd_len);


        /*Post the command to the HAL*/
        if (wlan_ftm_postmsg(pftm_data, cmd_len) != VOS_STATUS_SUCCESS) {

            hddLog(VOS_TRACE_LEVEL_ERROR,"%s:: FTM command failed",__func__);
            return;

        }
        /*After successful posting of message the command should be pending*/
        pHddCtx->ftm.IsCmdPending = TRUE;

        /*Wait here until you get the response from HAL*/
        if (vos_wait_single_event(&pHddCtx->ftm.ftm_vos_event, FTM_VOS_EVENT_WAIT_TIME)!= VOS_STATUS_SUCCESS)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,"%s: vos_wait_single_event failed",__func__);
            pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
            wlan_ftm_send_response(pHddCtx);
            pHddCtx->ftm.IsCmdPending = FALSE;
            return;
        }
        /*This check will handle the case where the completion is sent by
          wlan_hdd_process_ftm_cmd() and not by the HAL*/
        if (vos_is_load_unload_in_progress(VOS_MODULE_ID_HDD, NULL))
        {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
            "%s: Load/Unload in Progress. Ignoring FTM Command %d"
            , __func__, pRequestBuf->ftmpkt.ftm_cmd_type);

            pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_FAILURE;
            wlan_ftm_send_response(pHddCtx);
            pHddCtx->ftm.IsCmdPending = FALSE;
            return ;
        }

        cmd_len = be16_to_cpu(pHddCtx->ftm.wnl->wmsg.length);

        //HEXDUMP("Response to QXDM:", (char *)&pAdapter->ftm.wnl->wmsg, cmd_len);

        wlan_ftm_send_response(pHddCtx);
        pHddCtx->ftm.IsCmdPending = FALSE;
        break;

    default:

        hddLog(VOS_TRACE_LEVEL_ERROR,"%s:: Command not supported",__func__);
        return;
    }

    EXIT();
    return;
} /* wlan_adp_ftm_cmd() */

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_start_stop_ftm() -

   This function is used for start/stop the ftm driver.

  \param  - pAdapter - Pointer HDD Context.
              - start - 1/0 to start/stop ftm driver.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_start_stop_ftm(hdd_adapter_t *pAdapter,
                                               v_U16_t start)
{
    VOS_STATUS status;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (start)
    {
        pHddCtx->ftm.cmd_iwpriv = TRUE;
        status = wlan_hdd_ftm_start(pHddCtx);

        if (status != VOS_STATUS_SUCCESS)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                      "FTM Start Failed");
            return VOS_STATUS_E_FAILURE;
        }
        if (NULL == pMsgBuf)
        {
            pMsgBuf = (tPttMsgbuffer *)vos_mem_malloc(sizeof(tPttMsgbuffer));
            if (NULL == pMsgBuf)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                            "%s:pMsgBuf is NULL", __func__);
                return VOS_STATUS_E_FAILURE;
            }
        }
    }
    else
    {
        status = wlan_ftm_stop(pHddCtx);

        if (status != VOS_STATUS_SUCCESS)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                      "FTM Stop Failed");
            return VOS_STATUS_E_FAILURE;
        }
        pHddCtx->ftm.ftm_state = WLAN_FTM_STOPPED;
        if (pMsgBuf)
        {
            vos_mem_free((v_VOID_t * )pMsgBuf);
            pMsgBuf = NULL;
        }
    }
    return VOS_STATUS_SUCCESS;
}


static VOS_STATUS validate_channel(unsigned int channel,unsigned int cb)
{
    unsigned int *table = NULL;
    int index = 0;

    if (PHY_SINGLE_CHANNEL_CENTERED == cb)
        table = valid_channel;
    else if (cb >= PHY_DOUBLE_CHANNEL_LOW_PRIMARY &&
             cb <= PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)
        table = valid_channel_cb40;
    else if (cb >= PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED &&
             cb <= PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH)
        table = valid_channel_cb80;

    if (NULL == table)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s failed to find channel table %d",
                __func__, cb);
        return VOS_STATUS_E_FAILURE;
    }

    while (table[index] != 0)
    {
        if (table[index] == channel)
            return VOS_STATUS_SUCCESS;

        index++;
    }

    return VOS_STATUS_E_FAILURE;
}


static unsigned int get_primary_channel(unsigned int center_channel,unsigned int cb)
{
    unsigned int primary_channel = center_channel;

    switch (cb)
    {
        case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
            primary_channel -= 2;
            break;


        case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
                        primary_channel += 2;
                        break;

                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
                        primary_channel -= 6;
                        break;

                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
                        primary_channel += 6;
                        break;
        }

        return primary_channel;

}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_set_channel() -

   This function is used for setting the channel to the halphy ptt module.

  \param  - pAdapter - Pointer HDD Context.
              - channel   -  Channel Number 1-14.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_set_channel(hdd_adapter_t *pAdapter,v_U16_t channel)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    long ret;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    if (VOS_STATUS_SUCCESS != validate_channel(channel, ftm_status.cbmode))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Invalid Channel Number. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    vos_mem_set(pMsgBuf, sizeof(*pMsgBuf), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_SET_CHANNEL;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttSetChannel) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;

    pMsgBody->SetChannel.chId = get_primary_channel(channel, ftm_status.cbmode);

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Channel =%d",pMsgBody->SetChannel.chId);
    pMsgBody->SetChannel.cbState = ftm_status.cbmode ;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;

    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;

    }
done:

    return status;
}


/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_set_pwr_cntl_mode() -

   This function is used for setting the power control mode for tx.

  \param  - pAdapter - Pointer HDD Context.
          - pwr_mode   -  power control mode 0-2.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_set_pwr_cntl_mode(hdd_adapter_t *pAdapter,
                                                      v_U16_t pwr_mode)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    long ret;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    if (pwr_mode > 2)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:invalid control mode.valid mode is 0 , 1, 2.", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBody = &pMsgBuf->msgBody;
    pMsgBuf->msgId = PTT_MSG_CLOSE_TPC_LOOP_PRIMA_V1;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttCloseTpcLoop) + PTT_HEADER_LENGTH;

    pMsgBody->CloseTpcLoop.tpcClose = pwr_mode;
    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf, pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                 msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

    ftm_status.powerCtlMode= pwr_mode;

 done:
    return status;

}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_set_txpower() -

   This function is used for setting the txpower to the halphy ptt module.

  \param  - pAdapter - Pointer HDD Context.
              - txpower   -  txpower Number 1-18.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_set_txpower(hdd_adapter_t *pAdapter,
                                            v_U16_t txpower)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    long ret;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    /* do not allow to change setting when tx pktgen is enabled, although halphy does allow changing tx power
     * when tx pktgen is enabled
     */
    if (ftm_status.frameGenEnabled)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:cannot set txpower when pktgen is enabled.", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if(!(txpower >= 9 && txpower <= 24))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Invalid tx power. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBody = &pMsgBuf->msgBody;
    pMsgBuf->msgId = PTT_MSG_SET_TX_POWER;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttSetTxPower) + PTT_HEADER_LENGTH;

    pMsgBody->SetTxPower.dbmPwr = txpower*100;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                              msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

 done:

    return status;

}


static VOS_STATUS wlan_ftm_priv_enable_dpd(hdd_adapter_t *pAdapter,
                                              v_U16_t enable)
{
    tANI_U32 value = 0;
    tANI_U32 reg_addr;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    reg_addr = WCNSS_TXFIR_OFFSET;

    wpalReadRegister(reg_addr, &value);
    if (enable)
    {
        value &= (~QWLAN_TXFIR_CFG_DPD_BYPASS_MASK);
    }
    else
    {
        value |= QWLAN_TXFIR_CFG_DPD_BYPASS_MASK;
    }

    wpalWriteRegister(reg_addr, value);

    return VOS_STATUS_SUCCESS;
}


/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_set_txrate() -

   This function is used for setting the txrate to the halphy ptt module.
   It converts the user input string for txrate to the tx rate index.

  \param  - pAdapter - Pointer HDD Context.
              - txrate   -  Pointer to the tx rate string.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_set_txrate(hdd_adapter_t *pAdapter,char *txrate)
{
    int ii;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm.",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* do not allow to change setting when tx pktgen is enabled */
    if (ftm_status.frameGenEnabled)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:cannot set txrate when pktgen is enabled.",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    for(ii = 0; ii < SIZE_OF_TABLE(rateName_rateIndex_tbl); ii++)
    {
        if(!strcmp(rateName_rateIndex_tbl[ii].rate_str,txrate))
           break;
    }
    if(ii >= SIZE_OF_TABLE(rateName_rateIndex_tbl))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Invalid Rate String",__func__);
        return VOS_STATUS_E_FAILURE;
    }

    ftm_status.frameParams.rate = rateName_rateIndex_tbl[ii].rate_index;
    ftm_status.frameParams.preamble = rate_index_2_preamble_table[rateName_rateIndex_tbl[ii].rate_index].Preamble;

    return VOS_STATUS_SUCCESS;
}



static VOS_STATUS wlan_ftm_priv_set_power_index(hdd_adapter_t *pAdapter,
                                         ePowerTempIndexSource pwr_source)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (pwr_source > 3)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:invalid power index source. valid mode is 0 , 1, 2. ",
                   __func__);
        return VOS_STATUS_E_FAILURE;
    }

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);

    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBody = &pMsgBuf->msgBody;
    pMsgBuf->msgId = PTT_MSG_SET_PWR_INDEX_SOURCE;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttSetPwrIndexSource) + PTT_HEADER_LENGTH;

    pMsgBody->SetPwrIndexSource.indexSource = pwr_source;
    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                              msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

done:

   return status;
}


/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_start_stop_tx_pktgen() -

   This function is used for start/stop the tx packet generation.

  \param  - pAdapter - Pointer HDD Context.
              - startStop   -  Value( 1/0) start/stop the tx packet generation.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_start_stop_tx_pktgen(hdd_adapter_t *pAdapter,v_U16_t startStop)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    long ret;

    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    if (startStop != 1 && startStop != 0)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Tx value is invalid ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if ((ftm_status.frameGenEnabled && startStop == 1) ||
        (!ftm_status.frameGenEnabled && startStop == 0))
    {
        return VOS_STATUS_SUCCESS ;
    }
    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);

    if (startStop == 1)
    {
        init_completion(&pHddCtx->ftm.ftm_comp_var);
        pMsgBuf->msgId = PTT_MSG_CONFIG_TX_PACKET_GEN;
        pMsgBuf->msgBodyLength = sizeof(tMsgPttConfigTxPacketGen) + PTT_HEADER_LENGTH;
        pMsgBody = &pMsgBuf->msgBody;
        pMsgBody->ConfigTxPacketGen.frameParams = ftm_status.frameParams ;

        status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);
        if (status != VOS_STATUS_SUCCESS)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s:posting PTT_MSG_CONFIG_TX_PACKET_GEN failed", __func__);
            status = VOS_STATUS_E_FAILURE;
            goto done;
        }

        ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                   msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
        if (0 >= ret )
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("wait on ftm_comp_var failed %ld"), ret);
        }
        if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                       "%s: PTT_MSG_CONFIG_TX_PACKET_GEN failed", __func__);
            status = VOS_STATUS_E_FAILURE;
            goto done;
        }

        if (ftm_status.powerCtlMode == 2) //only for CLPC mode
        {
           status = wlan_ftm_priv_set_power_index(pAdapter, FIXED_POWER_DBM) != VOS_STATUS_SUCCESS; //power index source set to Fixed
           if(status != VOS_STATUS_SUCCESS)
           {
              goto done;
           }
        }
    }

    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_START_STOP_TX_PACKET_GEN;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttStartStopTxPacketGen) + PTT_HEADER_LENGTH;
    pMsgBody = &pMsgBuf->msgBody;
    pMsgBody->StartStopTxPacketGen.startStop = startStop;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);
    if(status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:wlan_ftm_postmsg failed",__func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

    ret  = wait_for_completion_interruptible_timeout(
                  &pHddCtx->ftm.ftm_comp_var,
                   msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }
    if(pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ptt response status failed",__func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

done:

    if (status == VOS_STATUS_SUCCESS)
    {
        if (startStop == 1)
        {
            ftm_status.frameGenEnabled = eANI_BOOLEAN_TRUE;
        }
        else
        {
            ftm_status.frameGenEnabled = eANI_BOOLEAN_FALSE;
        }
    }

    return status;
}



static VOS_STATUS wlan_ftm_priv_set_cb(hdd_adapter_t *pAdapter, v_U16_t cbmode)
{

    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (cbmode > PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:cb mode value is invalid ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    ftm_status.cbmode = cbmode;

    return VOS_STATUS_SUCCESS;

}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_rx_mode() -

   This function is used for start/stop the rx packet generation.

  \param  - pAdapter - Pointer HDD Context.
              - rxmode   -  0-disable RX.
                               -  1-rx ALL frames
                               -  2-rx 11 g/n frames
                               -  3-rx 11b frames

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_rx_mode(hdd_adapter_t *pAdapter,v_U16_t rxmode)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    long ret;

    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    if (rxmode > 3)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Rx mode value is invalid ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);

    pMsgBuf->msgId = PTT_MSG_SET_RX_DISABLE_MODE;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttSetRxDisableMode) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;

    switch(rxmode)
    {
        case RXMODE_DISABLE_ALL:
          pMsgBody->SetRxDisableMode.disabled.agPktsDisabled = VOS_TRUE;
          pMsgBody->SetRxDisableMode.disabled.bPktsDisabled  = VOS_TRUE;
          pMsgBody->SetRxDisableMode.disabled.slrPktsDisabled= VOS_TRUE;
          break;

        case RXMODE_ENABLE_ALL:
          pMsgBody->SetRxDisableMode.disabled.agPktsDisabled = VOS_FALSE;
          pMsgBody->SetRxDisableMode.disabled.bPktsDisabled  = VOS_FALSE;
          pMsgBody->SetRxDisableMode.disabled.slrPktsDisabled= VOS_FALSE;
          break;

        case RXMODE_ENABLE_11GN:
          pMsgBody->SetRxDisableMode.disabled.agPktsDisabled = VOS_FALSE;
          pMsgBody->SetRxDisableMode.disabled.bPktsDisabled  = VOS_TRUE;
          pMsgBody->SetRxDisableMode.disabled.slrPktsDisabled= VOS_TRUE;
          break;

        case RXMODE_ENABLE_11B:
          pMsgBody->SetRxDisableMode.disabled.agPktsDisabled = VOS_TRUE;
          pMsgBody->SetRxDisableMode.disabled.bPktsDisabled  = VOS_FALSE;
          pMsgBody->SetRxDisableMode.disabled.slrPktsDisabled= VOS_TRUE;
          break;

    }

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL(" wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ftm_status.rxmode = rxmode ;
done:

    return status;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_rx_pkt_clear() -

   This function sets the rx pkt count to zero.

  \param  - pAdapter - Pointer HDD Context.
              - rx_pkt_clear   -  rx_pkt_clear value.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_rx_pkt_clear(hdd_adapter_t *pAdapter,v_U16_t rx_pkt_clear)
{
    VOS_STATUS status;
    long ret;

    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    if (rx_pkt_clear != 1)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Invalid rx_pkt_clear value ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_RESET_RX_PACKET_STATISTICS;
    pMsgBuf->msgBodyLength = /*sizeof(tMsgPttResetRxPacketStatistics) + */PTT_HEADER_LENGTH;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                 msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
done:

    return status;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_get_channel() -

   This function gets the channel number from the halphy ptt module and
   returns the channel number to the application.

  \param  - pAdapter - Pointer HDD Context.
              - pChannel   -  Poniter to get the Channel number.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_get_channel(hdd_adapter_t *pAdapter,v_U16_t *pChannel)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    v_U16_t  freq;
    long ret;

    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    v_PVOID_t devHandle = pHddCtx->parent_dev;
    struct device *wcnss_device = (struct device *)devHandle;
    struct resource *wcnss_memory;
    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_DBG_READ_REGISTER;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttDbgReadRegister) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;
    wcnss_memory = wcnss_wlan_get_memory_map(wcnss_device);
    if (NULL == wcnss_memory)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: wcnss_memory is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }
    else
    {
        pMsgBody->DbgReadRegister.regAddr = wcnss_memory->start
                                          + QWLAN_AGC_CHANNEL_FREQ_REG_OFFSET;
    }
    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;

    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                 msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

    freq = ((v_U16_t)pMsgBody->DbgReadRegister.regValue & QWLAN_AGC_CHANNEL_FREQ_FREQ_MASK);

    *pChannel = vos_freq_to_chan(freq);
    (*pChannel) ? (status = VOS_STATUS_SUCCESS) : (status = VOS_STATUS_E_FAILURE);

     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Channel = %d  freq = %d",*pChannel, freq);
 done:

     return status;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_get_txpower() -

   This function gets the TX power from the halphy ptt module and
   returns the TX power to the application.

  \param  - pAdapter - Pointer HDD Context.
              - pTxPwr   -  Poniter to get the Tx power.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_get_txpower(hdd_adapter_t *pAdapter,v_U16_t *pTxPwr)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    long ret;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }
    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_GET_TX_POWER_REPORT;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttGetTxPowerReport) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                 msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: PTT_MSG_GET_TX_POWER_REPORT failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    *pTxPwr = pMsgBody->GetTxPowerReport.pwrTemplateIndex;

 done:

     return status;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_get_txrate() -

   This function gets the TX rate from the halphy ptt module and
   returns the TX rate to the application.

  \param  - pAdapter - Pointer HDD Context.
              - pTxRate   -  Poniter to get the Tx rate.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_get_txrate(hdd_adapter_t *pAdapter,char *pTxRate)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    v_U16_t ii;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    for(ii = 0; ii < SIZE_OF_TABLE(rateName_rateIndex_tbl); ii++) {
        if(rateName_rateIndex_tbl[ii].rate_index == ftm_status.frameParams.rate)
          break;
    }
    if(ii >= SIZE_OF_TABLE(rateName_rateIndex_tbl))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Invalid Rate Index",__func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    strlcpy(pTxRate,rateName_rateIndex_tbl[ii].rate_str, WE_FTM_MAX_STR_LEN);

done:
    return status;

}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_get_rx_pkt_count() -

   This function gets the rx pkt count from the halphy ptt module and
   returns the rx pkt count  to the application.

  \param  - pAdapter - Pointer HDD Context.
              - pRxPktCnt   -  Poniter to get the rx pkt count.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_get_rx_pkt_count(hdd_adapter_t *pAdapter,v_U16_t *pRxPktCnt)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    long ret;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_GET_RX_PKT_COUNTS;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttGetRxPktCounts) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                 msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    *pRxPktCnt = pMsgBody->GetRxPktCounts.counters.totalRxPackets;
done:

    return status;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_get_rx_rssi() -

   This function gets the rx rssi from the halphy ptt module and
   returns the rx rssi to the application.

  \param  - pAdapter - Pointer HDD Context.
              - buf   -  Poniter to get rssi of Rx chains

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_get_rx_rssi(hdd_adapter_t *pAdapter,char *buf)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    long ret;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (NULL == pMsgBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:pMsgBuf is NULL", __func__);
        return VOS_STATUS_E_NOMEM;
    }
    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_GET_RX_RSSI;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttGetRxRssi) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                    "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

   ret = snprintf(buf, WE_FTM_MAX_STR_LEN, " R0:%d, R1:%d",
                      pMsgBody->GetRxRssi.rssi.rx[0],
                  pMsgBody->GetRxRssi.rssi.rx[1]);

   if( ret < 0 || ret >= WE_FTM_MAX_STR_LEN )
   {
      status = VOS_STATUS_E_FAILURE;
   }

done:

    return status;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_get_mac_address() -

   This function gets the mac address from the halphy ptt module and
   returns the mac address  to the application.

  \param  - pAdapter - Pointer HDD Context.
              - buf   -  Poniter to get the mac address.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_get_mac_address(hdd_adapter_t *pAdapter,char *buf)
{
    v_BOOL_t itemIsValid = VOS_FALSE;
    v_U8_t macAddr[VOS_MAC_ADDRESS_LEN] = {0, 0x0a, 0xf5, 4,5, 6};
    int ret;

    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;

    if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm. ",__func__);
        return VOS_STATUS_E_FAILURE;
    }
    /*Check the NV FIELD is valid or not*/
    if (vos_nv_getValidity(VNV_FIELD_IMAGE, &itemIsValid) == VOS_STATUS_SUCCESS)
    {
       if (itemIsValid == VOS_TRUE)
       {
            vos_nv_readMacAddress(macAddr);

         ret = snprintf(buf, WE_FTM_MAX_STR_LEN,
                             "%02x:%02x:%02x:%02x:%02x:%02x",
                        MAC_ADDR_ARRAY(macAddr));
         if( ret < 0 || ret >= WE_FTM_MAX_STR_LEN )
         {
             return VOS_STATUS_E_FAILURE;
         }
       }
   }
   else
   {
         /*Return Hard coded mac address*/
      ret = snprintf(buf, WE_FTM_MAX_STR_LEN,
                            "%02x:%02x:%02x:%02x:%02x:%02x",
                     MAC_ADDR_ARRAY(macAddr));

      if( ret < 0 || ret >= WE_FTM_MAX_STR_LEN )
      {
          return VOS_STATUS_E_FAILURE;
      }
   }
    return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief wlan_ftm_priv_set_mac_address() -

   This function sets the mac address to the halphy ptt module and
   sends the netlink message to the ptt socket application which writes
   the macaddress to the qcom_wlan_nv.bin file

  \param  - pAdapter - Pointer HDD Context.
              - buf   -  Poniter to the macaddress.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_priv_set_mac_address(hdd_adapter_t *pAdapter,char *buf)
{
    uPttMsgs *pMsgBody;
    VOS_STATUS status;
    int macAddr[VOS_MAC_ADDRESS_LEN];
    v_U8_t *pMacAddress;
    v_U8_t  ii;
    hdd_context_t *pHddCtx = (hdd_context_t *)pAdapter->pHddCtx;
    long ret;

    if (pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ftm has not started. Please start the ftm. ", __func__);
        return VOS_STATUS_E_FAILURE;
    }
    vos_mem_set(pMsgBuf, sizeof(tPttMsgbuffer), 0);
    init_completion(&pHddCtx->ftm.ftm_comp_var);
    pMsgBuf->msgId = PTT_MSG_SET_NV_FIELD;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttSetNvField) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;
    pMsgBody->SetNvField.nvField = NV_COMMON_MAC_ADDR;

    /*We get the mac address in string format "XX:XX:XX:XX:XX:XX" convert to hex*/
    if (6 != sscanf(buf,"%02x:%02x:%02x:%02x:%02x:%02x",&macAddr[0],(int*)&macAddr[1],(int*)&macAddr[2],(int*)&macAddr[3],(int*)&macAddr[4],(int*)&macAddr[5]))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "Invalid MacAddress Input %s", buf);
       return VOS_STATUS_E_FAILURE;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
              "MacAddress = " MAC_ADDRESS_STR, MAC_ADDR_ARRAY(macAddr));


    pMacAddress = &pMsgBody->SetNvField.fieldData.macAddr.macAddr1[0];

    for(ii = 0; ii < VOS_MAC_ADDRESS_LEN; ii++)
       pMacAddress[ii] = (v_U8_t)macAddr[ii];


    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
              "pMacAddress = " MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pMacAddress));
    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed!!", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }
    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }

    if (pMsgBuf->msgResponse != PTT_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:Ptt response status failed", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "NV_COMMON_MAC_ADDR Success!!!");

    init_completion(&pHddCtx->ftm.ftm_comp_var);
    memset( pMsgBuf,0,sizeof(tPttMsgbuffer));

    pMsgBuf->msgId = PTT_MSG_STORE_NV_TABLE;
    pMsgBuf->msgBodyLength = sizeof(tMsgPttStoreNvTable) + PTT_HEADER_LENGTH;

    pMsgBody = &pMsgBuf->msgBody;

    pMsgBody->StoreNvTable.nvTable = NV_FIELDS_IMAGE;

    status = wlan_ftm_postmsg((v_U8_t*)pMsgBuf,pMsgBuf->msgBodyLength);

    if (status != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:wlan_ftm_postmsg failed!!!!", __func__);
        status = VOS_STATUS_E_FAILURE;
        goto done;
    }

    ret = wait_for_completion_interruptible_timeout(&pHddCtx->ftm.ftm_comp_var,
                                msecs_to_jiffies(WLAN_FTM_COMMAND_TIME_OUT));
    if (0 >= ret )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("wait on ftm_comp_var failed %ld"), ret);
    }
done:

    return VOS_STATUS_SUCCESS;
}

/* set param sub-ioctls */
static int __iw_ftm_setchar_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    int ret,sub_cmd;
    unsigned int length;
    char *param;
    VOS_STATUS status;
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    struct iw_point s_priv_data;

    ENTER();

    if (!capable(CAP_NET_ADMIN))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("permission check failed"));
        return -EPERM;
    }

    ret =0;
    /* helper function to get iwreq_data with compat handling. */
    if (hdd_priv_get_data(&s_priv_data, wrqu))
    {
       return -EINVAL;
    }

    /* make sure all params are correctly passed to function */
    if ((NULL == s_priv_data.pointer) || (0 == s_priv_data.length))
    {
       return -EINVAL;
    }

    pAdapter = (hdd_adapter_t *)netdev_priv(dev);
    if (NULL == pAdapter)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Adapter is NULL",__func__);
        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret)
    {
        return ret;
    }
    sub_cmd = s_priv_data.flags;
    length = s_priv_data.length;

    /* we cannot use iotctl_private_iw_point in kernel to allocate memory
     * to store data from userspace as IW_SETCHAR_GETNONE is defined as
     * odd number which assigns set_args to zero.we assisgn memory using
     * kzalloc here to hold userspace data
     */
    param = kzalloc(length + 1, GFP_KERNEL);
    if (!param)
        return -EINVAL;

    if (copy_from_user(param, s_priv_data.pointer, length))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s:Failed to get user data %s", __func__, param);

        ret = -EINVAL;
        goto OUT;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Received length %d, parameters: %s", __func__, length, param);

    switch(sub_cmd)
    {
       case WE_SET_MAC_ADDRESS:
       {

          VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "SET MAC ADDRESS");

          status  = wlan_ftm_priv_set_mac_address(pAdapter,param);

          if(status != VOS_STATUS_SUCCESS)
          {
             hddLog(VOS_TRACE_LEVEL_FATAL,
                    "wlan_ftm_priv_set_mac_address Failed =%d",status);

             ret = -EINVAL;
          }

          break;
       }
       case WE_SET_TX_RATE:
       {
            status  = wlan_ftm_priv_set_txrate(pAdapter,param);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,
                      "wlan_ftm_priv_set_txrate Failed =%d",status);

                ret = -EINVAL;
            }

            break;
       }
       default:
       {
           hddLog(LOGE, "%s: Invalid sub command %d",__func__, sub_cmd);
           ret = -EINVAL;
           break;
       }
    }

OUT:
    kfree(param);
    EXIT();
    return ret;
}

static int iw_ftm_setchar_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __iw_ftm_setchar_getnone(dev, info, wrqu, extra);
   vos_ssr_unprotect(__func__);

   return ret;
}

static int __iw_ftm_setint_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    int *value = (int *)extra;
    int sub_cmd = value[0];
    int set_value = value[1];
    int ret = 0; /* success */
    VOS_STATUS status;

    ENTER();

    pAdapter = (netdev_priv(dev));
    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Adapter is NULL",__func__);
        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret)
    {
        return ret;
    }
    switch(sub_cmd)
    {
        case WE_FTM_ON_OFF:
        {
            status  = wlan_ftm_priv_start_stop_ftm(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"%s Failed =%d",__func__, status);
               ret = -EINVAL;
            }

            break;
        }

        case WE_TX_PKT_GEN:
            status  = wlan_ftm_priv_start_stop_tx_pktgen(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_start_stop_tx_pktgen Failed =%d",status);
               ret = -EINVAL;
            }
            break;

        case WE_SET_TX_IFS:
            status  = wlan_ftm_priv_set_txifs(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_set_txifs Failed =%d",status);
               ret = -EINVAL;
            }
            break;

        case WE_SET_TX_PKT_CNT:
            status  = wlan_ftm_priv_set_txpktcnt(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_set_txpktcnt Failed =%d",status);
               ret = -EINVAL;
            }
            break;

        case WE_SET_TX_PKT_LEN:
            status  = wlan_ftm_priv_set_txpktlen(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_set_txpktlen Failed =%d",status);
               ret = -EINVAL;
            }
            break;

        case WE_TX_CW_RF_GEN:
            status = wlan_ftm_priv_cw_rf_gen(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
                hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_cw_gen Failed =%d\n",status);
                ret = -EINVAL;
            }
            break;

        case WE_SET_CHANNEL:
        {
            status  = wlan_ftm_priv_set_channel(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_set_channel Failed =%d",status);
               ret = -EINVAL;
            }
            break;
        }
        case WE_SET_TX_POWER:
        {
            status  = wlan_ftm_priv_set_txpower(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_set_txpower Failed =%d",status);
               ret = -EINVAL;
            }
            break;
        }
        case WE_CLEAR_RX_PKT_CNT:
        {
            status  = wlan_ftm_priv_rx_pkt_clear(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_rx_pkt_clear Failed =%d",status);
               ret = -EINVAL;
            }
            break;
        }
        case WE_RX:
        {
            status  = wlan_ftm_priv_rx_mode(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_rx_mode Failed =%d",status);
               ret = -EINVAL;
            }
            break;
        }
        case WE_ENABLE_CHAIN:
        {
            status  = wlan_ftm_priv_enable_chain(pAdapter,set_value);

            if(status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_enable_chain Failed =%d",status);
               ret = -EINVAL;
            }
            break;
        }

        case WE_SET_PWR_CNTL_MODE:
        {
            status  = wlan_ftm_priv_set_pwr_cntl_mode(pAdapter, set_value);
            if (status != VOS_STATUS_SUCCESS)
            {
               hddLog(VOS_TRACE_LEVEL_FATAL, "set_pwr_cntl_mode failed = %d",
                         status);
               ret = -EINVAL;
            }
            break;
        }

        case WE_ENABLE_DPD:
        {
           status  = wlan_ftm_priv_enable_dpd(pAdapter, set_value);
           if (status != VOS_STATUS_SUCCESS)
           {
              hddLog(VOS_TRACE_LEVEL_FATAL, "enable_dpd failed = %d", status);
              ret = -EINVAL;
           }
           break;
        }

        case WE_SET_CB:
        {
           status  = wlan_ftm_priv_set_cb(pAdapter, set_value);
           if (status != VOS_STATUS_SUCCESS)
           {
              hddLog(VOS_TRACE_LEVEL_FATAL, "set_cb failed = %d", status);
              ret = -EINVAL;
           }
           break;
        }

        default:
        {
            hddLog(LOGE, "Invalid IOCTL setvalue command %d value %d",
                sub_cmd, set_value);
            break;
        }
    }

    EXIT();
    return ret;
}

static int iw_ftm_setint_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_ftm_setint_getnone(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}
/* get param sub-ioctls */
static int __iw_ftm_setnone_getint(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    int *value = (int *)extra;
    int ret = 0; /* success */
    VOS_STATUS status;

    ENTER();

    pAdapter = (netdev_priv(dev));
    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Adapter is NULL",__func__);
        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret)
    {
        return ret;
    }

    switch (value[0])
    {
        case WE_GET_CHANNEL:
        {
           status = wlan_ftm_priv_get_channel(pAdapter,(v_U16_t*)value);

           if(status != VOS_STATUS_SUCCESS)
           {
              hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_get_channel Failed =%d",status);
              ret = -EINVAL;
           }
           break;
        }
        case WE_GET_TX_POWER:
        {
           status = wlan_ftm_priv_get_txpower(pAdapter,(v_U16_t*)value);

           if(status != VOS_STATUS_SUCCESS)
           {
              hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_get_txpower Failed =%d",status);
              ret = -EINVAL;
           }
           break;
        }
        case WE_GET_RX_PKT_CNT:
        {
           status = wlan_ftm_priv_get_rx_pkt_count(pAdapter,(v_U16_t*)value);

           if(status != VOS_STATUS_SUCCESS)
           {
              hddLog(VOS_TRACE_LEVEL_FATAL,"wlan_ftm_priv_get_rx_pkt_count Failed =%d",status);
              ret = -EINVAL;
           }
           break;
        }
        default:
        {
            hddLog(LOGE, "Invalid IOCTL get_value command %d ",value[0]);
            break;
        }
    }

    EXIT();
    return ret;
}

static int iw_ftm_setnone_getint(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_ftm_setnone_getint(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __iw_ftm_get_char_setnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    int sub_cmd = wrqu->data.flags;
    VOS_STATUS status;
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    int ret = 0;

    ENTER();

    pAdapter = (netdev_priv(dev));
    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Adapter is NULL",__func__);
        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret)
    {
        return ret;
    }
    switch(sub_cmd)
    {
        case WE_GET_MAC_ADDRESS:
        {
            status = wlan_ftm_priv_get_mac_address(pAdapter, extra);

            if(status != VOS_STATUS_SUCCESS)
            {
                hddLog(VOS_TRACE_LEVEL_FATAL, "wlan_ftm_priv_get_mac_address failed =%d",status);
                return -EINVAL;
            }
            wrqu->data.length = strlen(extra)+1;
            break;
        }
        case WE_GET_TX_RATE:
        {
            status = wlan_ftm_priv_get_txrate(pAdapter, extra);

            if(status != VOS_STATUS_SUCCESS)
            {
                hddLog(VOS_TRACE_LEVEL_FATAL, "wlan_ftm_priv_get_txrate failed =%d",status);
                return -EINVAL;
            }

            wrqu->data.length = strlen(extra)+1;
            break;
        }
        case WE_GET_FTM_STATUS:
        {
            status = wlan_ftm_priv_get_status(pAdapter, extra);

            if(status != VOS_STATUS_SUCCESS)
            {
                hddLog(VOS_TRACE_LEVEL_FATAL, "wlan_ftm_priv_get_status failed =%d",status);
                return -EINVAL;
            }

            wrqu->data.length = strlen(extra)+1;
            break;
        }
        case WE_GET_RX_RSSI:
        {
            status = wlan_ftm_priv_get_rx_rssi(pAdapter, extra);

            if(status != VOS_STATUS_SUCCESS)
            {
                hddLog(VOS_TRACE_LEVEL_FATAL, "wlan_ftm_priv_get_rx_rssi failed =%d",status);
                return -EINVAL;
            }

            wrqu->data.length = strlen(extra)+1;
            break;
        }
        default:
        {
            hddLog(LOGE, "%s: Invalid IOCTL command %d", __func__, sub_cmd );
            break;
        }
    }

    EXIT();
    return 0;
}

static int iw_ftm_get_char_setnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_ftm_get_char_setnone(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);


    return ret;
}

VOS_STATUS wlan_write_to_efs (v_U8_t *pData, v_U16_t data_len)
{
#if defined(MSM_PLATFORM)
    tAniHdr *wmsg = NULL;
    v_U8_t *pBuf;
    hdd_context_t *pHddCtx = NULL;
    v_CONTEXT_t pVosContext= NULL;

    pBuf =  (v_U8_t*)vos_mem_malloc(sizeof(tAniHdr) + sizeof(v_U32_t)+ data_len);
    if(pBuf == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:pBuf is NULL",__func__);
        return VOS_STATUS_E_NOMEM;
    }
    wmsg = (tAniHdr*)pBuf;
    wmsg->type = PTT_MSG_FTM_CMDS_TYPE;
    wmsg->length = data_len + sizeof(tAniHdr)+ sizeof(v_U32_t);
    wmsg->length = FTM_SWAP16(wmsg->length);
    pBuf += sizeof(tAniHdr);

     /*Get the global context */
    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

     /*Get the Hdd Context */
    //pAdapter = ((VosContextType*)(pVosContext))->pHDDContext;
    pHddCtx = (hdd_context_t *)(((VosContextType*)(pVosContext))->pHDDContext);

    /* EfS command Code */
    *(v_U32_t*)pBuf = 0x000000EF;

    pBuf += sizeof(v_U32_t);

    memcpy(pBuf, pData,data_len);

   if(pHddCtx->ftm.cmd_iwpriv == TRUE) {
       if( ptt_sock_send_msg_to_app(wmsg, 0,
                      ANI_NL_MSG_PUMAC, pHddCtx->ptt_pid, 0) < 0) {

           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("Ptt Socket error sending message to the app!!"));
           vos_mem_free((v_VOID_t*)wmsg);
           return VOS_STATUS_E_FAILURE;
       }
   }
   else {
    if( ptt_sock_send_msg_to_app(wmsg, 0,
                    ANI_NL_MSG_PUMAC, pHddCtx->ftm.wnl->nlh.nlmsg_pid, 0) < 0) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("Ptt Socket error sending message to the app!!"));
        vos_mem_free((v_VOID_t*)wmsg);
        return VOS_STATUS_E_FAILURE;
    }
   }

    vos_mem_free((v_VOID_t*)wmsg);
#endif //FTM and ANDROID

    return VOS_STATUS_SUCCESS;
}

/*  action sub-ioctls */
static int __iw_ftm_setnone_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    int sub_cmd = wrqu->data.flags;
    int ret = 0; /* success */

    switch (sub_cmd)
    {
        case WE_SET_NV_DEFAULTS:
        {
            v_U8_t *pu8buf,*pTempBuf;
            v_U16_t size;
            size = sizeof(v_U32_t) + sizeof(sHalNv);
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH,"HAL NV Size =%d",size);
            pu8buf = vos_mem_malloc(size);
            if(pu8buf == NULL)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:pu8buf is NULL",__func__);
                return VOS_STATUS_E_NOMEM;
            }
            memset(pu8buf,0,size);
            pTempBuf = pu8buf;
            pTempBuf += sizeof(v_U32_t);
            memcpy(pTempBuf,&nvDefaults,sizeof(sHalNv));

            wlan_write_to_efs(pu8buf,size);
            vos_mem_free(pu8buf);
        }

        default:
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,"%s: unknown ioctl %d", __func__, sub_cmd);
            hddLog(LOGE, "Invalid IOCTL action command %d ", sub_cmd);
            break;
        }
    }

    return ret;
}

static int iw_ftm_setnone_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_ftm_setnone_getnone(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __iw_ftm_set_var_ints_getnone(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    int sub_cmd = wrqu->data.flags;
    int *value = (int*)wrqu->data.pointer;
    int ret = 0;

    ENTER();

    if(wrqu->data.length != 2)
    {
        hddLog(LOGE, "Invalid number of Arguments  %d ",  wrqu->data.length);
        return -EINVAL;
    }
    pAdapter = (netdev_priv(dev));
    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  "%s: Adapter is NULL",__func__);
        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret)
    {
        return ret;
    }
    switch (sub_cmd)
    {
        case WE_SET_TX_WF_GAIN:
        {
            v_S15_t dGain = 0;
            v_U16_t rfGain = 0;
            VOS_STATUS status;

            dGain = *(v_S15_t*) value++;
            rfGain = *(v_U16_t*) value;
            status = wlan_ftm_priv_set_wfgain(pAdapter,dGain,rfGain);

            if(status != VOS_STATUS_SUCCESS)
            {
                hddLog(VOS_TRACE_LEVEL_FATAL,
                        "wlan_ftm_priv_set_wfgain Failed =%d\n", status);
                return -EINVAL;
            }
        }
        break;

        default:
        {
            hddLog(LOGE, "Invalid IOCTL command %d ", sub_cmd );
            break;
        }
    }

    EXIT();
    return 0;
}


static int iw_ftm_set_var_ints_getnone(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
   int ret;

   if (!capable(CAP_NET_ADMIN))
   {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
           FL("permission check failed"));
        return -EPERM;
   }

   vos_ssr_protect(__func__);
   ret = __iw_ftm_set_var_ints_getnone(dev, info, wrqu, extra);
   vos_ssr_unprotect(__func__);

   return ret;
}

static const iw_handler we_ftm_private[] = {

   [WLAN_FTM_PRIV_SET_INT_GET_NONE      - SIOCIWFIRSTPRIV]   = iw_ftm_setint_getnone,  //set priv ioctl
   [WLAN_FTM_PRIV_SET_NONE_GET_INT      - SIOCIWFIRSTPRIV]   = iw_ftm_setnone_getint,  //get priv ioctl
   [WLAN_FTM_PRIV_SET_CHAR_GET_NONE     - SIOCIWFIRSTPRIV]   = iw_ftm_setchar_getnone, //get priv ioctl
   [WLAN_FTM_PRIV_GET_CHAR_SET_NONE     - SIOCIWFIRSTPRIV]   = iw_ftm_get_char_setnone,
   [WLAN_FTM_PRIV_SET_NONE_GET_NONE     - SIOCIWFIRSTPRIV]   = iw_ftm_setnone_getnone, //action priv ioctl
   [WLAN_FTM_PRIV_SET_VAR_INT_GET_NONE  - SIOCIWFIRSTPRIV]   = iw_ftm_set_var_ints_getnone,
};

/*Maximum command length can be only 15 */
static const struct iw_priv_args we_ftm_private_args[] = {

    /* handlers for main ioctl */
    {   WLAN_FTM_PRIV_SET_INT_GET_NONE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "" },

    {   WE_FTM_ON_OFF,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "ftm" },

    {   WE_TX_PKT_GEN,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "tx" },

    {   WE_SET_TX_IFS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set_txifs" },

    {   WE_SET_TX_PKT_CNT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set_txpktcnt" },

    {   WE_SET_TX_PKT_LEN,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set_txpktlen" },

    {   WE_SET_TX_WF_GAIN,
        IW_PRIV_TYPE_INT | MAX_FTM_VAR_ARGS,
        0,
        "set_tx_wf_gain" },

    {   WE_TX_CW_RF_GEN,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "tx_cw_rf_gen" },
    {   WE_SET_CHANNEL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set_channel" },

    {   WE_SET_TX_POWER,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set_txpower" },

    {   WE_CLEAR_RX_PKT_CNT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "clr_rxpktcnt" },

    {   WE_RX,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "rx" },

    {   WE_ENABLE_CHAIN,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "ena_chain" },

    {   WE_SET_PWR_CNTL_MODE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "pwr_cntl_mode" },

    {   WE_ENABLE_DPD,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "ena_dpd" },

    {   WE_SET_CB,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set_cb" },

    /* handlers for main ioctl */
    {   WLAN_FTM_PRIV_SET_NONE_GET_INT,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "" },

    {   WE_GET_CHANNEL,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_channel" },

    {   WE_GET_TX_POWER,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_txpower" },

    {   WE_GET_RX_PKT_CNT,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_rxpktcnt" },

    /* handlers for main ioctl */
    {   WLAN_FTM_PRIV_SET_CHAR_GET_NONE,
        IW_PRIV_TYPE_CHAR| 512,
        0,
        "" },

    {   WE_SET_MAC_ADDRESS,
        IW_PRIV_TYPE_CHAR| 512,
        0,
        "set_mac_address" },

    {   WE_SET_TX_RATE,
        IW_PRIV_TYPE_CHAR | 512,
        0,
        "set_txrate" },

    /* handlers for main ioctl */
    {   WLAN_FTM_PRIV_GET_CHAR_SET_NONE,
        0,
        IW_PRIV_TYPE_CHAR| WE_FTM_MAX_STR_LEN,
        "" },

    {   WE_GET_MAC_ADDRESS,
        0,
        IW_PRIV_TYPE_CHAR| WE_FTM_MAX_STR_LEN,
        "get_mac_address" },

    {   WE_GET_TX_RATE,
        0,
        IW_PRIV_TYPE_CHAR| WE_FTM_MAX_STR_LEN,
        "get_txrate" },

    {   WE_GET_FTM_STATUS,
        0,
        IW_PRIV_TYPE_CHAR| WE_FTM_MAX_STR_LEN,
        "get_status" },

    {   WE_GET_RX_RSSI,
        0,
        IW_PRIV_TYPE_CHAR| WE_FTM_MAX_STR_LEN,
        "get_rx_rssi" },

    {   WLAN_FTM_PRIV_SET_VAR_INT_GET_NONE,
        IW_PRIV_TYPE_INT | MAX_FTM_VAR_ARGS,
        0,
        "" },
    /* handlers for main ioctl */
    {   WLAN_FTM_PRIV_SET_NONE_GET_NONE,
        0,
        0,
        "" },

    /* handlers for sub-ioctl */
    {   WE_SET_NV_DEFAULTS,
        0,
        0,
        "set_nv_defaults" },

};

const struct iw_handler_def we_ftm_handler_def = {
   .num_standard     = 0,
   .num_private      = sizeof(we_ftm_private) / sizeof(we_ftm_private[0]),
   .num_private_args = sizeof(we_ftm_private_args) / sizeof(we_ftm_private_args[0]),

   .standard         = (iw_handler *)NULL,
   .private          = (iw_handler *)we_ftm_private,
   .private_args     = we_ftm_private_args,
   .get_wireless_stats = NULL,
};

static int wlan_ftm_register_wext(hdd_adapter_t *pAdapter)
{

    //hdd_wext_state_t *pwextBuf = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

    // Zero the memory.  This zeros the profile structure.
    //memset(pwextBuf, 0,sizeof(hdd_wext_state_t));

    pAdapter->dev->wireless_handlers = (struct iw_handler_def *)&we_ftm_handler_def;

    return 0;
}


VOS_STATUS WLANFTM_McProcessMsg (v_VOID_t *message)
{
    ftm_rsp_msg_t   *pFtmMsgRsp;

    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    hdd_context_t *pHddCtx;
    v_CONTEXT_t pVosContext= NULL;

    ENTER();

    pFtmMsgRsp = (ftm_rsp_msg_t *)message;

    if (!message )
    {
        VOS_TRACE( VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
                "WLAN FTM:Invalid parameter sent on WLANFTM_ProcessMainMessage");
        return VOS_STATUS_E_INVAL;
    }
    /*Get the global context */
    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

     /*Get the Hdd Context */
    pHddCtx = ((VosContextType*)(pVosContext))->pHDDContext;

    if (pHddCtx->ftm.cmd_iwpriv == TRUE) {

        memcpy((char*)pMsgBuf, (char*)message, pFtmMsgRsp->msgBodyLength);
        complete(&pHddCtx->ftm.ftm_comp_var);
    }
    else {
    /*Response length to Ptt App*/
    pHddCtx->ftm.wnl->wmsg.length = sizeof(tAniHdr)+ SIZE_OF_FTM_DIAG_HEADER_LEN + pFtmMsgRsp->msgBodyLength;

     /*Ptt App expects the response length in LE */
    pHddCtx->ftm.wnl->wmsg.length = FTM_SWAP16(pHddCtx->ftm.wnl->wmsg.length);

    /*Response expects the length to be in */
    pHddCtx->ftm.pResponseBuf->ftm_hdr.data_len = pHddCtx->ftm.pRequestBuf->ftm_hdr.data_len -
                                           sizeof(pHddCtx->ftm.pRequestBuf->ftm_hdr.data_len);

    /*Copy the message*/
    memcpy((char*)&pHddCtx->ftm.pResponseBuf->ftmpkt,(char*)message,pFtmMsgRsp->msgBodyLength);

    /*Update the error code*/
    pHddCtx->ftm.pResponseBuf->ftm_err_code = WLAN_FTM_SUCCESS;

    vos_status = vos_event_set(&pHddCtx->ftm.ftm_vos_event);

    if (!VOS_IS_STATUS_SUCCESS(vos_status))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: HDD vos_event_set failed!!"));
       return VOS_STATUS_E_FAILURE;
    }
    }
    EXIT();
    return VOS_STATUS_SUCCESS;

}
