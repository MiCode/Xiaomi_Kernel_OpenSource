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

/**
 *
 *  @file:         wlan_qct_dev_defs.h
 *
 *  @brief:        This file contains the hardware related definitions.
 *
 */

#ifndef __WLAN_QCT_DEV_DEFS_H
#define __WLAN_QCT_DEV_DEFS_H


/* --------------------------------------------------------------------
 * HW definitions for WLAN Chip
 * --------------------------------------------------------------------
 */

#ifdef WCN_PRONTO

#ifdef WLAN_SOFTAP_VSTA_FEATURE
//supports both V1 and V2
#define HAL_NUM_ASSOC_STA           32 // HAL_NUM_STA - No of GP STAs - 2 (1 self Sta + 1 Bcast Sta)
#define HAL_NUM_STA                 41
#define HAL_NUM_HW_STA              16

#define HAL_NUM_GPSTA               4
#define HAL_NUM_UMA_DESC_ENTRIES    HAL_NUM_HW_STA // or HAL_NUM_STA

#define HAL_NUM_BSSID               2
#define HAL_NUM_STA_WITHOUT_VSTA    12
#define HAL_NUM_STA_INCLUDING_VSTA  32

#define HAL_NUM_VSTA                (HAL_NUM_STA - HAL_NUM_HW_STA)
#define QWLANFW_MAX_NUM_VSTA        (HAL_NUM_VSTA)
#define QWLANFW_VSTA_INVALID_IDX    (HAL_NUM_STA+1)
#define QWLAN_VSTA_MIN_IDX          (HAL_NUM_HW_STA)
#define QWLANFW_NUM_GPSTA           (HAL_NUM_GPSTA)

// For Pronto
#define HAL_NUM_STA_WITHOUT_VSTA_PRONTO_V1 9
#define HAL_NUM_STA_WITHOUT_VSTA_PRONTO_V2 (HAL_NUM_STA_WITHOUT_VSTA)

#define IS_VSTA_VALID_IDX(__x) \
                          ((__x) != QWLANFW_VSTA_INVALID_IDX)

#define IS_VSTA_IDX(__x) \
                   (((__x) >= QWLAN_VSTA_MIN_IDX) && ((__x) < HAL_NUM_STA))

#define GET_VSTA_INDEX_FOR_STA_INDEX(__idx)    ((__idx) - QWLAN_VSTA_MIN_IDX)

// is the STA a General Purpose STA?
#define IS_GPSTA_IDX(__x) \
    (((__x) >= (HAL_NUM_HW_STA-HAL_NUM_GPSTA)) && \
     ((__x) < HAL_NUM_HW_STA))

// is the STA a HW STA (excluding GP STAs)
#define IS_HWSTA_IDX(__x) \
    ((__x) < (HAL_NUM_HW_STA-HAL_NUM_GPSTA))

#define HAL_NUM_STA_INCLUDING_VSTA  32

#elif WCN_PRONTO_V1

/* In Pronto 1.0 TPE descriptor size is increased to 1K per station
 * but not the cMEM allocated for hardware descriptors. Due to this
 * memory limitation the number of stations are limited to 9 and BSS
 * to 2 respectively. 
 *
 * In Pronto 2.0, TPE descriptor size is reverted
 * back to 512 bytes and hence more stations and BSSs can be supported
 * from Pronto 2.0
 *
 * In Pronto 1.0, 9 HW stations are supported including BCAST STA(staId 0)
 * and SELF STA(staId 1). So total ASSOC stations which can connect to
 * Pronto 1.0 Softap = 9 - 1(self sta) - 1(Bcast sta) = 7 stations
 */
#define HAL_NUM_HW_STA              9
#define HAL_NUM_STA                 (HAL_NUM_HW_STA)
#define HAL_NUM_BSSID               2
#define HAL_NUM_UMA_DESC_ENTRIES    9
#define HAL_NUM_ASSOC_STA           7


#else /* WCN_PRONTO_V1 */

#define HAL_NUM_HW_STA              14
#define HAL_NUM_STA                 (HAL_NUM_HW_STA)
#define HAL_NUM_BSSID               4
#define HAL_NUM_UMA_DESC_ENTRIES    14
#define HAL_NUM_ASSOC_STA           12


#endif /* WCN_PRONTO_V1 and WLAN_SOFTAP_VSTA_FEATURE*/
#else  /* WCN_PRONTO */

/*
 * Riva supports 16 stations in hardware
 *
 * Riva without Virtual STA feature can only support 12 stations:
 *    1 Broadcast STA (hard)
 *    1 "Self" STA (hard)
 *    10 Soft AP Stations (hard)
 *
 * Riva with Virtual STA feature supports 38 stations:
 *    1 Broadcast STA (hard)
 *    1 "Self" STA (hard)
 *    4 General Purpose Stations to support Virtual STAs (hard)
 *   32 Soft AP Stations (10 hard/22 virtual)
 *
 * To support concurrency with Vsta, number of stations are increased to 41 (from 38).
 *    1 for the second interface.
 *    1 for reserving an infra peer STA index (hard) for the other interface.
 *    1 for P2P device role.
 */
#ifdef WLAN_SOFTAP_VSTA_FEATURE
#define HAL_NUM_ASSOC_STA           32
#define HAL_NUM_STA                 41
#define HAL_NUM_HW_STA              16
#define HAL_NUM_GPSTA               4
#define HAL_NUM_VSTA                (HAL_NUM_STA - HAL_NUM_HW_STA)

#define QWLANFW_MAX_NUM_VSTA        HAL_NUM_VSTA
#define QWLANFW_VSTA_INVALID_IDX    (HAL_NUM_STA+1)
#define QWLAN_VSTA_MIN_IDX          HAL_NUM_HW_STA
#define QWLANFW_NUM_GPSTA           HAL_NUM_GPSTA


#define IS_VSTA_VALID_IDX(__x) \
                          ((__x) != QWLANFW_VSTA_INVALID_IDX)

#define IS_VSTA_IDX(__x) \
                   (((__x) >= QWLAN_VSTA_MIN_IDX) && ((__x) < HAL_NUM_STA))

#define GET_VSTA_INDEX_FOR_STA_INDEX(__idx)    ((__idx) - QWLAN_VSTA_MIN_IDX)

// is the STA a General Purpose STA?
#define IS_GPSTA_IDX(__x) \
    (((__x) >= (HAL_NUM_HW_STA-HAL_NUM_GPSTA)) && \
     ((__x) < HAL_NUM_HW_STA))

// is the STA a HW STA (excluding GP STAs)
#define IS_HWSTA_IDX(__x) \
    ((__x) < (HAL_NUM_HW_STA-HAL_NUM_GPSTA))

#define HAL_NUM_STA_INCLUDING_VSTA  32
#define HAL_NUM_STA_WITHOUT_VSTA    12

#else
#define HAL_NUM_STA                 12
#define HAL_NUM_ASSOC_STA           10
#define HAL_NUM_HW_STA              12
#endif

#define HAL_NUM_BSSID               2
#define HAL_NUM_UMA_DESC_ENTRIES    HAL_NUM_HW_STA

#endif /* WCN_PRONTO */

#ifdef FEATURE_WLAN_TDLS
#define CXM_TDLS_MAX_NUM_STA            32
#endif

#define HAL_INVALID_BSSIDX          HAL_NUM_BSSID

#define MAX_NUM_OF_BACKOFFS         8
#define HAL_MAX_ASSOC_ID            HAL_NUM_STA

#define WLANHAL_TX_BD_HEADER_SIZE   40  //FIXME_PRIMA - Revisit
#define WLANHAL_RX_BD_HEADER_SIZE   76  

/*
 * From NOVA Mac Arch document
 *  Encryp. mode    The encryption mode
 *  000: Encryption functionality is not enabled
 *  001: Encryption is set to WEP
 *  010: Encryption is set to WEP 104
 *  011: Encryption is set to TKIP
 *  100: Encryption is set to AES
 *  101 - 111: Reserved for future
 */

#define HAL_ENC_POLICY_NULL        0
#define HAL_ENC_POLICY_WEP40       1
#define HAL_ENC_POLICY_WEP104      2
#define HAL_ENC_POLICY_TKIP        3
#define HAL_ENC_POLICY_AES_CCM     4

/* --------------------------------------------------------------------- */
/* BMU */
/* --------------------------------------------------------------------- */

/*
 * BMU WQ assignment, as per Prima Programmer's Guide - FIXME_PRIMA: Revisit
 *
 */

typedef enum sBmuWqId {

    /* ====== In use WQs ====== */

    /* BMU */
    BMUWQ_BMU_IDLE_BD = 0,
    BMUWQ_BMU_IDLE_PDU = 1,

    /* RxP */
    BMUWQ_RXP_UNKNWON_ADDR = 2,  /* currently unhandled by HAL */

    /* DPU RX */
    BMUWQ_DPU_RX = 3,

    /* DPU TX */
    BMUWQ_DPU_TX = 6,

    /* Firmware */
    BMUWQ_FW_TRANSMIT = 12,  /* DPU Tx->FW Tx */
    BMUWQ_FW_RECV = 7,       /* DPU Rx->FW Rx */

    BMUWQ_FW_RPE_RECV = 16,   /* RXP/RPE Rx->FW Rx */
    FW_SCO_WQ = BMUWQ_FW_RPE_RECV,

    /* DPU Error */
    BMUWQ_DPU_ERROR_WQ = 8,

    /* DXE RX */
    BMUWQ_DXE_RX = 11,

    BMUWQ_DXE_RX_HI = 4,

    /* ADU/UMA */
    BMUWQ_ADU_UMA_TX = 23,
    BMUWQ_ADU_UMA_RX = 24,

    /* BMU BTQM */
    BMUWQ_BTQM = 25,

    /* Special WQ for BMU to dropping all frames coming to this WQ ID */
    BMUWQ_SINK = 255,

#ifdef WCN_PRONTO
    BMUWQ_BMU_CMEM_IDLE_BD = 27,
    /* Total BMU WQ count in Pronto */
    BMUWQ_NUM = 28,
    
    //WQs 17 through 22 are enabled in Pronto. So, set not supported mask to 0.
    BMUWQ_NOT_SUPPORTED_MASK = 0x0,
#else
    /* Total BMU WQ count in Prima */
    BMUWQ_NUM = 27,

    //Prima has excluded support for WQs 17 through 22.
    BMUWQ_NOT_SUPPORTED_MASK = 0x7e0000,
#endif //WCN_PRONTO


    /* Aliases */
    BMUWQ_BTQM_TX_MGMT = BMUWQ_BTQM,
    BMUWQ_BTQM_TX_DATA = BMUWQ_BTQM,
    BMUWQ_BMU_WQ2 = BMUWQ_RXP_UNKNWON_ADDR,
    BMUWQ_FW_DPU_TX = 5,

    //WQ where all the frames with addr1/addr2/addr3 with value 254/255 go to. 
    BMUWQ_FW_RECV_EXCEPTION = 14, //using BMUWQ_FW_MESSAGE WQ for this purpose.

    //WQ where all frames with unknown Addr2 filter exception cases frames will pushed if FW wants host to 
    //send deauth to the sender. 
    BMUWQ_HOST_RX_UNKNOWN_ADDR2_FRAMES = 15, //using BMUWQ_FW_DXECH2_0 for this purpose.

    /* ====== Unused/Reserved WQ ====== */

    /* ADU/UMA Error WQ */
    BMUWQ_ADU_UMA_TX_ERROR_WQ = 13, /* Not in use by HAL */
    BMUWQ_ADU_UMA_RX_ERROR_WQ = 10, /* Not in use by HAL */

    /* DPU Error WQ2 */
    BMUWQ_DPU_ERROR_WQ2 = 9, /* Not in use by HAL */

    /* FW WQs */
    //This WQ is being used for RXP to push in frames in exception cases ( addr1/add2/addr3 254/255)
    //BMUWQ_FW_MESG = 14,      /* DxE Tx->FW, Not in use by FW */
    //BMUWQ_FW_DXECH2_0 = 15,  /* BD/PDU<->MEM conversion using DxE CH2.  Not in use by FW */
    BMUWQ_FW_DXECH2_1 = 16,  /* BD/PDU<->MEM conversion using DxE CH2.  Not in use by FW */

    /* NDPA Addr3 workaround */
    BMUWQ_RXP_DEFAULT_PUSH_WQ = 17,
/*  These WQs are not supported in Volans
    BMUWQ_BMU_WQ17 = 17,
    BMUWQ_BMU_WQ18 = 18,
    BMUWQ_BMU_WQ19 = 19,
    BMUWQ_BMU_WQ20 = 20,
    BMUWQ_BMU_WQ21 = 21,
    BMUWQ_BMU_WQ22 = 22
*/
} tBmuWqId;

typedef enum
{
    BTQM_QID0 = 0,
    BTQM_QID1,
    BTQM_QID2,
    BTQM_QID3,
    BTQM_QID4,
    BTQM_QID5,
    BTQM_QID6,
    BTQM_QID7,
    BTQM_QID8,
    BTQM_QID9,
    BTQM_QID10,

    BTQM_QUEUE_TX_TID_0 = BTQM_QID0,
    BTQM_QUEUE_TX_TID_1,
    BTQM_QUEUE_TX_TID_2,
    BTQM_QUEUE_TX_TID_3,
    BTQM_QUEUE_TX_TID_4,
    BTQM_QUEUE_TX_TID_5,
    BTQM_QUEUE_TX_TID_6,
    BTQM_QUEUE_TX_TID_7,


    /* Queue Id <-> BO 
       */
    BTQM_QUEUE_TX_nQOS = BTQM_QID8,
    BTQM_QUEUE_SELF_STA_BCAST_MGMT = BTQM_QID10,    
    BTQM_QUEUE_SELF_STA_UCAST_MGMT = BTQM_QID9,
    BTQM_QUEUE_SELF_STA_UCAST_DATA = BTQM_QID9,
    BTQM_QUEUE_NULL_FRAME          = BTQM_QID9,      
    BTQM_QUEUE_SELF_STA_PROBE_RSP =  BTQM_QID9,
    BTQM_QUEUE_TX_AC_BE = BTQM_QUEUE_TX_TID_0,
    BTQM_QUEUE_TX_AC_BK = BTQM_QUEUE_TX_TID_2,
    BTQM_QUEUE_TX_AC_VI = BTQM_QUEUE_TX_TID_4,
    BTQM_QUEUE_TX_AC_VO = BTQM_QUEUE_TX_TID_6
}tBtqmQId;

#define STACFG_MAX_TC   8

/* --------------------------------------------------------------------- */
/* BD  type*/
/* --------------------------------------------------------------------- */
#define HWBD_TYPE_GENERIC                  0   /* generic BD format */
#define HWBD_TYPE_FRAG                     1   /* fragmentation BD format*/

/*---------------------------------------------------------------------- */
/* HW Tx power                                                           */
/*---------------------------------------------------------------------- */
#ifdef WLAN_HAL_PRIMA
   #define WLAN_SOC_PRIMA_MAX_TX_POWER 22
   #define WLAN_SOC_PRIMA_MIN_TX_POWER 6
#else
   /* add more platforms here */
   #define WLAN_SOC_PRIMA_MAX_TX_POWER 22
   #define WLAN_SOC_PRIMA_MIN_TX_POWER 6
#endif //#ifdef WCN_PRIMA

#endif /* __WLAN_QCT_DEV_DEFS_H */
