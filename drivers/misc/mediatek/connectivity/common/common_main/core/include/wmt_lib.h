/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/



#ifndef _WMT_LIB_H_
#define _WMT_LIB_H_


#include "wmt_core.h"
#include "wmt_exp.h"
#include <mtk_wcn_cmb_stub.h>
#include "stp_wmt.h"
#include "wmt_plat.h"
#include "wmt_idc.h"
#include "osal.h"
#include "mtk_wcn_consys_hw.h"



/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#define USE_NEW_PROC_FS_FLAG 1


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define WMT_OP_BUF_SIZE (16)
#if 0				/* moved to wmt_exp.h */
#define WMT_LOG_LOUD    4
#define WMT_LOG_DBG     3
#define WMT_LOG_INFO    2
#define WMT_LOG_WARN    1
#define WMT_LOG_ERR     0
#endif
typedef enum _ENUM_WMTRSTRET_TYPE_T {
	WMTRSTRET_SUCCESS = 0x0,
	WMTRSTRET_FAIL = 0x1,
	WMTRSTRET_ONGOING = 0x2,
	WMTRSTRET_RETRY = 0x3,
	WMTRSTRET_MAX
} ENUM_WMTRSTRET_TYPE_T, *P_ENUM_WMTRSTRET_TYPE_T;

#define MAX_ANT_RAM_CODE_DOWN_TIME 3000
/*
*3(retry times) * 180 (STP retry time out)
*+ 10 (firmware process time) +
*10 (transmit time) +
*10 (uart process -> WMT response pool) +
*230 (others)
*/
#define WMT_LIB_RX_TIMEOUT 2000	/*800-->cover v1.2phone BT function on time (~830ms) */
/* Since GPS timeout is 6 seconds */
#define WMT_LIB_RX_EXTEND_TIMEOUT 4000
/*
*open wifi during wifi power on procedure
*(because wlan is insert to system after mtk_hif_sdio module,
*so wifi card is not registered to hif module
*when mtk_wcn_wmt_func_on is called by wifi through rfkill)
*/
#define MAX_WIFI_ON_TIME 5500

#define WMT_PWRON_RTY_DFT 2
#define MAX_RETRY_TIME_DUE_TO_RX_TIMEOUT (WMT_PWRON_RTY_DFT * WMT_LIB_RX_TIMEOUT)
#define MAX_EACH_FUNC_ON_WHEN_CHIP_POWER_ON_ALREADY WMT_LIB_RX_TIMEOUT	/*each WMT command */
#define MAX_FUNC_ON_TIME (MAX_WIFI_ON_TIME + MAX_RETRY_TIME_DUE_TO_RX_TIMEOUT +\
	MAX_EACH_FUNC_ON_WHEN_CHIP_POWER_ON_ALREADY * 3 + MAX_ANT_RAM_CODE_DOWN_TIME)

/*1000->WMT_LIB_RX_TIMEOUT + 1000, logical judgement */
#define MAX_EACH_FUNC_OFF (WMT_LIB_RX_TIMEOUT + 1000)
#define MAX_FUNC_OFF_TIME (MAX_EACH_FUNC_OFF * 4)

/*1000->WMT_LIB_RX_TIMEOUT + 1000, logical judgement */
#define MAX_EACH_WMT_CMD (WMT_LIB_RX_TIMEOUT + 1000)

/* For WMT OP who will have trouble if timeout really happens */
#define MAX_WMT_OP_TIMEOUT (30000)

#define MAX_GPIO_CTRL_TIME (2000)	/* [FixMe][GeorgeKuo] a temp value */

#define UTC_SYNC_TIME (60 * 60 * 1000)

#define WMT_IDC_MSG_BUFFER 2048
#define WMT_IDC_MSG_MAX_SIZE (WMT_IDC_MSG_BUFFER - 7) /* Subtract STP payload cmd size */

#define MAX_PATCH_NUM (10)

#define WMT_FIRMWARE_VERSION_LENGTH       (14)
#define WMT_FIRMWARE_MAX_FILE_NAME_LENGTH (50)

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/* AIF FLAG definition */
/* bit(0): share pin or not */
#define WMT_LIB_AIF_FLAG_MASK (0x1UL)
#define WMT_LIB_AIF_FLAG_SHARE (0x1UL << 0)
#define WMT_LIB_AIF_FLAG_SEPARATE (0x0UL << 0)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/* bit field offset definition */
typedef enum {
	WMT_STAT_PWR = 0,	/* is powered on */
	WMT_STAT_STP_REG = 1,	/* is STP driver registered: */
	WMT_STAT_STP_OPEN = 2,	/* is STP opened: default FALSE */
	WMT_STAT_STP_EN = 3,	/* is STP enabled: default FALSE */
	WMT_STAT_STP_RDY = 4,	/* is STP ready for client: default FALSE */
	WMT_STAT_RX = 5,	/* is rx data available */
	WMT_STAT_CMD = 6,	/* is cmd string to be read */
	WMT_STAT_SDIO1_ON = 7,	/* is SDIO1 on */
	WMT_STAT_SDIO2_ON = 8,	/* is SDIO2 on */
	WMT_STAT_SDIO_WIFI_ON = 9,	/* is Wi-Fi SDIO function on */
	WMT_STAT_SDIO_STP_ON = 10,	/* is STP SDIO function on */
	WMT_STAT_RST_ON = 11,
	WMT_STAT_MAX
} WMT_STAT;

typedef enum _ENUM_WMTRSTSRC_TYPE_T {
	WMTRSTSRC_RESET_BT = 0x0,
	WMTRSTSRC_RESET_FM = 0x1,
	WMTRSTSRC_RESET_GPS = 0x2,
	WMTRSTSRC_RESET_WIFI = 0x3,
	WMTRSTSRC_RESET_STP = 0x4,
	WMTRSTSRC_RESET_TEST = 0x5,
	WMTRSTSRC_RESET_MAX
} ENUM_WMTRSTSRC_TYPE_T, *P_ENUM_WMTRSTSRC_TYPE_T;

enum wmt_fw_log_type {
	WMT_FWLOG_MCU = 0,
	WMT_FWLOG_MAX
};

typedef struct {
	PF_WMT_CB fDrvRst[WMTDRV_TYPE_MAX];
} WMT_FDRV_CB, *P_WMT_FDRV_CB;


typedef struct {
	UINT32 dowloadSeq;
	UINT8 addRess[4];
	UINT8 patchName[256];
} WMT_PATCH_INFO, *P_WMT_PATCH_INFO;

struct wmt_rom_patch_info {
	UINT32 type;
	UINT8 addRess[4];
	UINT8 patchName[256];
};

struct wmt_vendor_patch {
	union {
		INT32 id;
		INT32 type;
	};
	UINT8 file_name[WMT_FIRMWARE_MAX_FILE_NAME_LENGTH + 1];
	UINT8 version[WMT_FIRMWARE_VERSION_LENGTH + 1];
};

struct vendor_patch_table {
	UINT32  capacity;
	UINT32  num;
	INT8    status;
	INT8    need_update;
	PUINT8 *active_version;
	struct wmt_vendor_patch *patch;
};

enum wmt_patch_type {
	WMT_PATCH_TYPE_ROM = 0,
	WMT_PATCH_TYPE_RAM,
	WMT_PATCH_TYPE_WIFI
};

enum wmt_cp_status {
	WMT_CP_INIT = 0,
	WMT_CP_READY_TO_CHECK,
	WMT_CP_CHECK_DONE
};

#define WMT_LIB_DMP_SLOT 2
struct consys_state_dmp_req {
	struct consys_state_dmp_op consys_ops[WMT_LIB_DMP_SLOT];
	atomic_t version;
};

/* OS independent wrapper for WMT_OP */
typedef struct _DEV_WMT_ {

	OSAL_SLEEPABLE_LOCK psm_lock;
	OSAL_SLEEPABLE_LOCK idc_lock;
	OSAL_SLEEPABLE_LOCK wlan_lock;
	OSAL_SLEEPABLE_LOCK assert_lock;
	OSAL_SLEEPABLE_LOCK mpu_lock;
	OSAL_SLEEPABLE_LOCK power_lock;
	/* WMTd thread information */
	/* struct task_struct *pWmtd;   *//* main thread (wmtd) handle */
	OSAL_THREAD thread;
	/* wait_queue_head_t rWmtdWq;  *//*WMTd command wait queue */
	OSAL_EVENT rWmtdWq;	/* rename */
	OSAL_THREAD worker_thread;
	OSAL_EVENT rWmtdWorkerWq;
	/* ULONG state; *//* bit field of WMT_STAT */
	OSAL_BIT_OP_VAR state;

	/* STP context information */
	/* wait_queue_head_t rWmtRxWq;  *//* STP Rx wait queue */
	OSAL_EVENT rWmtRxWq;	/* rename */
	/* WMT_STP_FUNC rStpFunc; *//* STP functions */
	WMT_FDRV_CB rFdrvCb;

	/* WMT Configurations */
	WMT_HIF_CONF rWmtHifConf;
	WMT_GEN_CONF rWmtGenConf;

	/* Patch information */
	UINT8 cPatchName[NAME_MAX + 1];
	UINT8 cFullPatchName[NAME_MAX + 1];
	UINT32 patchNum;

	const osal_firmware *pPatch;

	UINT8 cWmtcfgName[NAME_MAX + 1];

	const osal_firmware *pWmtCfg;

	const osal_firmware *pNvram;

	/* Current used UART port description */
	INT8 cUartName[NAME_MAX + 1];

	OSAL_OP_Q rFreeOpQ;	/* free op queue */
	OSAL_OP_Q rActiveOpQ;	/* active op queue */
	OSAL_OP_Q rWorkerOpQ;
	OSAL_OP arQue[WMT_OP_BUF_SIZE];	/* real op instances */
	P_OSAL_OP pCurOP;	/* current op */
	P_OSAL_OP pWorkerOP;	/* current op on worker thread */

	/* cmd str buffer */
	UINT8 cCmd[NAME_MAX + 1];
	INT32 cmdResult;
/* struct completion cmd_comp; */
	/* wait_queue_head_t cmd_wq; *//* read command queues */
	OSAL_SIGNAL cmdResp;
	OSAL_EVENT cmdReq;

	/* WMT loopback Thread Information */
/* WMT_CMB_VER combo_ver; */
	/* P_WMT_CMB_CHIP_INFO_S pChipInfo; */
	UINT32 chip_id;
	UINT32 hw_ver;
	UINT32 fw_ver;
	UINT32 ip_ver;

	UINT32 ext_ldo_flag;
	P_WMT_PATCH_INFO pWmtPatchInfo;
	struct wmt_rom_patch_info *pWmtRomPatchInfo[WMTDRV_TYPE_ANT];
	/* MET thread information */
	OSAL_THREAD met_thread;
	INT32 met_log_ctrl;
	/* Timer to sync UTC time with connsys */
	OSAL_TIMER utc_sync_timer;
	struct work_struct utcSyncWorker;
	/* Timer for wmt_worker_thread */
	OSAL_TIMER worker_timer;
	struct work_struct wmtd_worker_thread_work;
	struct osal_op_history wmtd_op_history;
	struct osal_op_history worker_op_history;
	UINT8 msg_local_buffer[WMT_IDC_MSG_BUFFER];
	struct vendor_patch_table patch_table;

	struct consys_state_dmp_req state_dmp_req;

} DEV_WMT, *P_DEV_WMT;


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

extern DEV_WMT gDevWmt;
extern INT32 wmt_lib_init(VOID);
extern INT32 wmt_lib_deinit(VOID);
extern INT32 wmt_lib_tx(PUINT8 data, UINT32 size, PUINT32 writtenSize);
extern INT32 wmt_lib_tx_raw(PUINT8 data, UINT32 size, PUINT32 writtenSize);
extern INT32 wmt_lib_rx(PUINT8 buff, UINT32 buffLen, PUINT32 readSize);
extern VOID wmt_lib_flush_rx(VOID);
extern UINT32 wmt_lib_co_clock_flag_get(VOID);
extern INT32 wmt_lib_sdio_reg_rw(INT32 func_num, INT32 direction, UINT32 offset, UINT32 value);


#if WMT_PLAT_ALPS
extern PINT8 wmt_uart_port_desc;	/* defined in mtk_wcn_cmb_stub_alps.cpp */
#endif

#if CFG_WMT_PS_SUPPORT
extern INT32 wmt_lib_ps_set_idle_time(UINT32 psIdleTime);
extern INT32 wmt_lib_ps_init(VOID);
extern INT32 wmt_lib_ps_deinit(VOID);
extern INT32 wmt_lib_ps_enable(VOID);
extern INT32 wmt_lib_ps_ctrl(UINT32 state);

extern INT32 wmt_lib_ps_disable(VOID);
extern VOID wmt_lib_ps_irq_cb(VOID);
#endif
extern VOID wmt_lib_ps_set_sdio_psop(PF_WMT_SDIO_PSOP own_cb);
#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
extern VOID wmt_lib_sdio_deep_sleep_flag_set_cb_reg(PF_WMT_SDIO_DEEP_SLEEP flag_cb);
#endif
extern VOID wmt_lib_sdio_reg_rw_cb(PF_WMT_SDIO_DEBUG reg_rw_cb);
extern INT32 wmt_lib_register_thermal_ctrl_cb(thermal_query_ctrl_cb thermal_ctrl);
extern INT32 wmt_lib_register_trigger_assert_cb(trigger_assert_cb trigger_assert);

/* LXOP functions: */
extern P_OSAL_OP wmt_lib_get_free_op(VOID);
extern INT32 wmt_lib_put_op_to_free_queue(P_OSAL_OP pOp);
extern MTK_WCN_BOOL wmt_lib_put_act_op(P_OSAL_OP pOp);
extern MTK_WCN_BOOL wmt_lib_put_worker_op(P_OSAL_OP pOp);

/* extern ENUM_WMTHWVER_TYPE_T wmt_lib_get_hwver (VOID); */
extern UINT32 wmt_lib_get_icinfo(ENUM_WMT_CHIPINFO_TYPE_T type);

extern MTK_WCN_BOOL wmt_lib_is_therm_ctrl_support(ENUM_WMTTHERM_TYPE_T eType);
extern MTK_WCN_BOOL wmt_lib_is_dsns_ctrl_support(VOID);
extern INT32 wmt_lib_trigger_cmd_signal(INT32 result);
extern PUINT8 wmt_lib_get_cmd(VOID);
extern P_OSAL_EVENT wmt_lib_get_cmd_event(VOID);
extern INT32 wmt_lib_set_patch_name(PUINT8 cPatchName);
extern INT32 wmt_lib_set_uart_name(PINT8 cUartName);
extern INT32 wmt_lib_set_hif(ULONG hifconf);
extern P_WMT_HIF_CONF wmt_lib_get_hif(VOID);
extern MTK_WCN_BOOL wmt_lib_get_cmd_status(VOID);

/* GeorgeKuo: replace set_chip_gpio() with more specific ones */
#if 0				/* moved to wmt_exp.h */
extern INT32 wmt_lib_set_aif(enum CMB_STUB_AIF_X aif, MTK_WCN_BOOL share);	/* set AUDIO interface options */
#endif
extern INT32 wmt_lib_host_awake_get(VOID);
extern INT32 wmt_lib_host_awake_put(VOID);
extern UINT32 wmt_lib_dbg_level_set(UINT32 level);
#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
extern INT32 wmt_lib_deep_sleep_ctrl(INT32 value);
extern MTK_WCN_BOOL wmt_lib_deep_sleep_flag_set(MTK_WCN_BOOL flag);
#endif
extern INT32 wmt_lib_msgcb_reg(ENUM_WMTDRV_TYPE_T eType, PF_WMT_CB pCb);

extern INT32 wmt_lib_msgcb_unreg(ENUM_WMTDRV_TYPE_T eType);
ENUM_WMTRSTRET_TYPE_T wmt_lib_cmb_rst(ENUM_WMTRSTSRC_TYPE_T src);
MTK_WCN_BOOL wmt_lib_sw_rst(INT32 baudRst);
MTK_WCN_BOOL wmt_lib_hw_rst(VOID);
INT32 wmt_lib_reg_rw(UINT32 isWrite, UINT32 offset, PUINT32 pvalue, UINT32 mask);
INT32 wmt_lib_efuse_rw(UINT32 isWrite, UINT32 offset, PUINT32 pvalue, UINT32 mask);
INT32 wmt_lib_sdio_ctrl(UINT32 on);
INT32 wmt_lib_met_ctrl(INT32 met_ctrl, INT32 log_ctrl);
INT32 wmt_lib_gps_mcu_ctrl(PUINT8 p_tx_data_buf, UINT32 tx_data_len, PUINT8 p_rx_data_buf,
			   UINT32 rx_data_buf_len, PUINT32 p_rx_data_len);
VOID wmt_lib_set_ext_ldo(UINT32 flag);
UINT32 wmt_lib_get_ext_ldo(VOID);
INT32 wmt_lib_try_pwr_off(VOID);
P_WMT_PATCH_INFO wmt_lib_get_patch_info(VOID);
INT32 wmt_lib_met_cmd(UINT32 value);

INT32 wmt_lib_blank_status_ctrl(UINT32 on_off_flag);
VOID wmt_lib_set_blank_status(UINT32 on_off_flag);
extern UINT32 wmt_lib_get_blank_status(VOID);

extern INT32 DISABLE_PSM_MONITOR(VOID);
extern VOID ENABLE_PSM_MONITOR(VOID);
extern INT32 wmt_lib_notify_stp_sleep(VOID);
extern VOID wmt_lib_psm_lock_release(VOID);
extern INT32 wmt_lib_psm_lock_aquire(VOID);
extern INT32 wmt_lib_psm_lock_trylock(VOID);
extern VOID wmt_lib_idc_lock_release(VOID);
extern INT32 wmt_lib_idc_lock_aquire(VOID);
extern VOID wmt_lib_wlan_lock_release(VOID);
extern INT32 wmt_lib_wlan_lock_trylock(VOID);
extern INT32 wmt_lib_wlan_lock_aquire(VOID);
extern VOID wmt_lib_assert_lock_release(VOID);
extern INT32 wmt_lib_assert_lock_trylock(VOID);
extern INT32 wmt_lib_assert_lock_aquire(VOID);
extern VOID wmt_lib_mpu_lock_release(VOID);
extern INT32 wmt_lib_mpu_lock_aquire(VOID);
extern VOID wmt_lib_power_lock_release(VOID);
extern INT32 wmt_lib_power_lock_trylock(VOID);
extern INT32 wmt_lib_power_lock_aquire(VOID);
extern INT32 wmt_lib_set_stp_wmt_last_close(UINT32 value);

extern VOID wmt_lib_set_patch_num(UINT32 num);
extern VOID wmt_lib_set_patch_info(P_WMT_PATCH_INFO pPatchinfo);
extern VOID wmt_lib_set_rom_patch_info(struct wmt_rom_patch_info *PatchInfo, ENUM_WMTDRV_TYPE_T type);
extern MTK_WCN_BOOL wmt_lib_stp_is_btif_fullset_mode(VOID);

extern INT32 wmt_lib_set_current_op(P_DEV_WMT pWmtDev, P_OSAL_OP pOp);
extern P_OSAL_OP wmt_lib_get_current_op(P_DEV_WMT pWmtDev);
extern INT32 wmt_lib_set_worker_op(P_DEV_WMT pWmtDev, P_OSAL_OP pOp);
extern P_OSAL_OP wmt_lib_get_worker_op(P_DEV_WMT pWmtDev);
extern PUINT8 wmt_lib_get_fwinfor_from_emi(UINT8 section, UINT32 offset, PUINT8 buff, UINT32 len);
extern INT32 wmt_lib_merge_if_flag_ctrl(UINT32 enable);
extern INT32 wmt_lib_merge_if_flag_get(UINT32 enable);

extern PUINT8 wmt_lib_get_cpupcr_xml_format(PUINT32 len);
extern PUINT8 wmt_lib_get_cpupcr_reg_info(PUINT32 len, PUINT32 consys_reg);
extern UINT32 wmt_lib_set_host_assert_info(UINT32 type, UINT32 reason, UINT32 en);
extern INT8 wmt_lib_co_clock_get(VOID);
extern UINT32 wmt_lib_soc_set_wifiver(UINT32 wifiver);
extern VOID wmt_lib_dump_wmtd_backtrace(VOID);

#if CFG_WMT_LTE_COEX_HANDLING
extern MTK_WCN_BOOL wmt_lib_handle_idc_msg(conn_md_ipc_ilm_t *idc_infor);
#endif
extern UINT32 wmt_lib_get_drv_status(UINT32 type);
extern INT32 wmt_lib_tm_temp_query(VOID);
extern INT32 wmt_lib_trigger_reset(VOID);
extern INT32 wmt_lib_trigger_assert(ENUM_WMTDRV_TYPE_T type, UINT32 reason);
extern INT32 wmt_lib_trigger_assert_keyword(ENUM_WMTDRV_TYPE_T type, UINT32 reason, PUINT8 keyword);
extern VOID wmt_lib_trigger_assert_keyword_delay(ENUM_WMTDRV_TYPE_T type, UINT32 reason, PUINT8 keyword);
extern INT32 wmt_lib_wifi_fem_cfg_report(PVOID pvInfoBuf);
#if CFG_WMT_PS_SUPPORT
extern UINT32 wmt_lib_quick_sleep_ctrl(UINT32 en);
#endif
extern UINT32 wmt_lib_fw_patch_update_rst_ctrl(UINT32 en);
#if CONSYS_ENALBE_SET_JTAG
extern UINT32 wmt_lib_jtag_flag_set(UINT32 en);
#endif

UINT32 wmt_lib_get_gps_lna_pin_num(VOID);
extern INT32 wmt_lib_fw_log_ctrl(enum wmt_fw_log_type type, UINT8 onoff, UINT8 level);
VOID wmt_lib_print_wmtd_op_history(VOID);
VOID wmt_lib_print_worker_op_history(VOID);
extern INT32 wmt_lib_get_vendor_patch_num(VOID);
extern INT32 wmt_lib_set_vendor_patch_version(struct wmt_vendor_patch *p);
extern INT32 wmt_lib_get_vendor_patch_version(struct wmt_vendor_patch *p);
extern INT32 wmt_lib_set_check_patch_status(INT32 status);
extern INT32 wmt_lib_get_check_patch_status(VOID);
extern INT32 wmt_lib_set_active_patch_version(struct wmt_vendor_patch *p);
extern INT32 wmt_lib_get_active_patch_version(struct wmt_vendor_patch *p);
extern INT32 wmt_lib_get_need_update_patch_version(VOID);
extern INT32 wmt_lib_set_need_update_patch_version(INT32 need);
extern VOID wmt_lib_set_bt_link_status(INT32 type, INT32 value);
VOID mtk_lib_set_mcif_mpu_protection(MTK_WCN_BOOL enable);
INT32 wmt_lib_dmp_consys_state(P_CONSYS_STATE_DMP_INFO dmp_info,
				UINT32 cpupcr_times, UINT32 slp_ms);

extern INT32 wmt_lib_reg_readable(VOID);
extern INT32 wmt_lib_reg_readable_by_addr(SIZE_T addr);
extern INT32 wmt_lib_utc_time_sync(VOID);
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif				/* _WMT_LIB_H_ */
