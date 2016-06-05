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

#ifndef WLAN_HDD_FTM_H
#define WLAN_HDD_FTM_H
#include "vos_status.h"
#include "vos_mq.h"
#include "vos_api.h"
#include "msg.h"
#include "halTypes.h"
#include "vos_types.h"
#include <wlan_ptt_sock_svc.h>

#define WLAN_FTM_SUCCESS   0
#define WLAN_FTM_FAILURE   1

#define WLAN_FTM_START              1
#define WLAN_FTM_STOP               2        
#define WLAN_FTM_CMD                3


#define WLAN_FTM_PHY_CMD         100
#define SIR_HAL_FTM_CMD          10
#define QUALCOMM_MODULE_TYPE     2
#define WLAN_FTM_COMMAND_TIME_OUT 10000
#define PHYDBG_PREAMBLE_NOT_SUPPORTED 0xFF
/* Private ioctls and their sub-ioctls */
#define WLAN_FTM_PRIV_SET_INT_GET_NONE    (SIOCIWFIRSTPRIV + 0)
#define WE_FTM_ON_OFF         1
#define WE_TX_PKT_GEN         2
#define WE_SET_TX_IFS         3
#define WE_SET_TX_PKT_CNT     4
#define WE_SET_TX_PKT_LEN     5
#define WE_SET_CHANNEL        6
#define WE_SET_TX_POWER       7
#define WE_CLEAR_RX_PKT_CNT   8
#define WE_RX                 9
#define WE_ENABLE_CHAIN      10
#define WE_SET_PWR_CNTL_MODE 11
#define WE_ENABLE_DPD        12
#define WE_SET_CB            13
#define WE_TX_CW_RF_GEN      14

/* Private ioctls and their sub-ioctls */
#define WLAN_FTM_PRIV_SET_NONE_GET_INT    (SIOCIWFIRSTPRIV + 1)
#define WE_GET_CHANNEL      1
#define WE_GET_TX_POWER     2
#define WE_GET_RX_PKT_CNT   3

/* Private ioctls and their sub-ioctls */
#define WLAN_FTM_PRIV_SET_INT_GET_INT     (SIOCIWFIRSTPRIV + 2)

/* Private ioctls and their sub-ioctls */
#define WLAN_FTM_PRIV_SET_CHAR_GET_NONE   (SIOCIWFIRSTPRIV + 3)
#define WE_SET_MAC_ADDRESS   1
#define WE_SET_TX_RATE       2

/* Private ioctls and their sub-ioctls */
#define WLAN_FTM_PRIV_SET_THREE_INT_GET_NONE   (SIOCIWFIRSTPRIV + 4)
#define WE_SET_WLAN_DBG      1

/* Private ioctls and their sub-ioctls */
#define WLAN_FTM_PRIV_GET_CHAR_SET_NONE   (SIOCIWFIRSTPRIV + 5)
#define WE_GET_MAC_ADDRESS   1
#define WE_GET_TX_RATE        2
#define WE_GET_FTM_VERSION   3
#define WE_GET_FTM_STATUS    4
#define WE_GET_RX_RSSI       5

/* Private ioctls and their sub-ioctls */
#define WLAN_FTM_PRIV_SET_NONE_GET_NONE   (SIOCIWFIRSTPRIV + 6)
#define WE_SET_NV_DEFAULTS    1

#define WLAN_FTM_PRIV_SET_VAR_INT_GET_NONE   (SIOCIWFIRSTPRIV + 7)
#define WE_SET_TX_WF_GAIN  1

#define WE_FTM_MAX_STR_LEN 1024
#define MAX_FTM_VAR_ARGS  7

#define MAX_NV_TABLE_SIZE  40000

typedef enum {
    WLAN_FTM_CMD_START = 1,
    WLAN_FTM_CMD_STOP,        
    WLAN_FTM_CMD_CMD
} wlan_hdd_ftm_cmds;
typedef struct ftm_hdr_s {
    v_U16_t cmd_id;
    v_U16_t data_len;
    v_U16_t respPktSize;
} ftm_hdr_t;

/* The request buffer of FTM which contains a byte of command and the request */
typedef struct wlan_hdd_ftm_payload_s {
    v_U16_t    ftm_cmd_type;
    v_U8_t    pFtmCmd[1];
}wlan_hdd_ftm_payload;
#define SIZE_OF_FTM_DIAG_HEADER_LEN 12
/* the FTM command/response structure */
typedef struct wlan_hdd_ftm_request_s
{
    v_U8_t    cmd_code;
    v_U8_t    sub_sys_id;
    v_U16_t   mode_id;
    ftm_hdr_t ftm_hdr; 
    v_U16_t   module_type;
    wlan_hdd_ftm_payload ftmpkt;
}wlan_hdd_ftm_request_t;

typedef struct wlan_hdd_ftm_response_s
{
    v_U8_t    cmd_code;
    v_U8_t    sub_sys_id;
    v_U16_t   mode_id;
    ftm_hdr_t ftm_hdr; 
    v_U16_t   ftm_err_code;
    wlan_hdd_ftm_payload ftmpkt;
}wlan_hdd_ftm_response_t;

typedef enum {
    WLAN_FTM_INITIALIZED,
    WLAN_FTM_STOPPED,
    WLAN_FTM_STARTED,
    WLAN_FTM_STARTING,
} wlan_hdd_ftm_state;
typedef struct wlan_hdd_ftm_status_s
{
    v_U8_t ftm_state;
    wlan_hdd_ftm_request_t    *pRequestBuf;
    wlan_hdd_ftm_response_t   *pResponseBuf;
    tAniNlHdr *wnl;
        /**vos event */
    vos_event_t  ftm_vos_event;
    
   /** completion variable for ftm command to complete*/
    struct completion ftm_comp_var;
    v_BOOL_t  IsCmdPending;
    v_BOOL_t  cmd_iwpriv;

    /** Large size of NV Table Handle **/
    eNvTable  processingNVTable;
    v_U32_t   targetNVTableSize;
    v_U8_t   *targetNVTablePointer;
    v_U32_t   processedNVTableSize;
    v_U8_t   *tempNVTableBuffer;
    struct completion startCmpVar;

} wlan_hdd_ftm_status_t;
typedef struct ftm_msg_s
{
    /* This field can be used as sequence 
        number/dialogue token for matching request/response */
    v_U16_t type;
    
    /* This guy carries the command buffer along with command id */
    void *cmd_ptr;
    v_U32_t bodyval;
} ftm_msg_t;
typedef struct ftm_rsp_msg_s
{
    v_U16_t   msgId;
    v_U16_t   msgBodyLength;
    v_U32_t   respStatus;
    v_U8_t   *msgResponse;
} ftm_rsp_msg_t;

typedef struct rateIndex2Preamble
{
    v_U16_t   rate_index;
    v_U16_t   Preamble;
} rateIndex2Preamble_t;
typedef struct freq_chan_s
{
    v_U16_t   freq;
    v_U16_t   chan;
} freq_chan_t;

typedef struct rateStr2rateIndex_s
{
    v_U16_t   rate_index;
    char      rate_str[30];
} rateStr2rateIndex_t;


#define FTM_SWAP16(A) ((((tANI_U16)(A) & 0xff00) >> 8) | \
                         (((tANI_U16)(A) & 0x00ff) << 8)   \
                      )
#define PTT_HEADER_LENGTH 8

#define FTM_VOS_EVENT_WAIT_TIME 10000

#define SIZE_OF_TABLE(a) (sizeof(a) / sizeof(a[0]))

int wlan_hdd_ftm_open(hdd_context_t *pHddCtx);
void wlan_hdd_process_ftm_cmd (hdd_context_t *pHddCtx,tAniNlHdr *wnl);
int wlan_hdd_ftm_close(hdd_context_t *pHddCtx);

#endif
