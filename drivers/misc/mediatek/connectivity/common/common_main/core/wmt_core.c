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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-CORE]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <connectivity_build_in_adapter.h>
#include "osal_typedef.h"
#include "connsys_debug_utility.h"
#include "wmt_lib.h"
#include "wmt_core.h"
#include "wmt_ctrl.h"
#include "wmt_ic.h"
#include "wmt_conf.h"

#include "wmt_func.h"
#include "stp_core.h"
#include "psm_core.h"
#include "wmt_exp.h"
#include "wmt_detect.h"
#include "wmt_plat.h"
#include "wmt_dev.h"

P_WMT_FUNC_OPS gpWmtFuncOps[WMTDRV_TYPE_MAX] = {
#if CFG_FUNC_BT_SUPPORT
	[WMTDRV_TYPE_BT] = &wmt_func_bt_ops,
#else
	[WMTDRV_TYPE_BT] = NULL,
#endif

#if CFG_FUNC_FM_SUPPORT
	[WMTDRV_TYPE_FM] = &wmt_func_fm_ops,
#else
	[WMTDRV_TYPE_FM] = NULL,
#endif

#if CFG_FUNC_GPS_SUPPORT
	[WMTDRV_TYPE_GPS] = &wmt_func_gps_ops,
#else
	[WMTDRV_TYPE_GPS] = NULL,
#endif

#if CFG_FUNC_GPSL5_SUPPORT
	[WMTDRV_TYPE_GPSL5] = &wmt_func_gpsl5_ops,
#else
	[WMTDRV_TYPE_GPSL5] = NULL,
#endif

#if CFG_FUNC_WIFI_SUPPORT
	[WMTDRV_TYPE_WIFI] = &wmt_func_wifi_ops,
#else
	[WMTDRV_TYPE_WIFI] = NULL,
#endif

#if CFG_FUNC_ANT_SUPPORT
	[WMTDRV_TYPE_ANT] = &wmt_func_ant_ops,
#else
	[WMTDRV_TYPE_ANT] = NULL,
#endif


};

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* TODO:[FixMe][GeorgeKuo]: is it an MT6620 only or general general setting?
 * move to wmt_ic_6620 temporarily.
 */
/* #define CFG_WMT_BT_PORT2 (1) *//* BT Port 2 Feature. */
#define CFG_CHECK_WMT_RESULT (1)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

static WMT_CTX gMtkWmtCtx;
static UINT8 gLpbkBuf[WMT_LPBK_BUF_LEN] = { 0 };
#ifdef CONFIG_MTK_COMBO_ANT
static UINT8 gAntBuf[1024] = { 0 };
#endif
#if CFG_WMT_LTE_COEX_HANDLING
static UINT32 g_open_wmt_lte_flag;
#endif
static UINT8 gFlashBuf[1024] = { 0 };
#if CFG_WMT_LTE_COEX_HANDLING
static UINT8 msg_local_buffer[WMT_IDC_MSG_BUFFER] = { 0 };
#endif
/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static INT32 opfunc_hif_conf(P_WMT_OP pWmtOp);
static INT32 opfunc_pwr_on(P_WMT_OP pWmtOp);
static INT32 opfunc_pwr_off(P_WMT_OP pWmtOp);
static INT32 opfunc_func_on(P_WMT_OP pWmtOp);
static INT32 opfunc_func_off(P_WMT_OP pWmtOp);
static INT32 opfunc_reg_rw(P_WMT_OP pWmtOp);
static INT32 opfunc_exit(P_WMT_OP pWmtOp);
static INT32 opfunc_pwr_sv(P_WMT_OP pWmtOp);
static INT32 opfunc_dsns(P_WMT_OP pWmtOp);
static INT32 opfunc_lpbk(P_WMT_OP pWmtOp);
static INT32 opfunc_cmd_test(P_WMT_OP pWmtOp);
static INT32 opfunc_hw_rst(P_WMT_OP pWmtOp);
static INT32 opfunc_sw_rst(P_WMT_OP pWmtOp);
static INT32 opfunc_stp_rst(P_WMT_OP pWmtOp);
static INT32 opfunc_efuse_rw(P_WMT_OP pWmtOp);
static INT32 opfunc_therm_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_gpio_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_sdio_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_pin_state(P_WMT_OP pWmtOp);
static INT32 opfunc_bgw_ds(P_WMT_OP pWmtOp);
static INT32 opfunc_set_mcu_clk(P_WMT_OP pWmtOp);
static INT32 opfunc_adie_lpbk_test(P_WMT_OP pWmtOp);
static INT32 wmt_core_gen2_set_mcu_clk(UINT32 kind);
static INT32 wmt_core_gen3_set_mcu_clk(UINT32 kind);
static INT32 wmt_core_set_mcu_clk(UINT32 kind);
static VOID wmt_core_dump_func_state(PINT8 pSource);
static INT32 wmt_core_stp_init(VOID);
static INT32 wmt_core_trigger_assert(VOID);
static INT32 wmt_core_stp_deinit(VOID);
static INT32 wmt_core_hw_check(VOID);
#ifdef CONFIG_MTK_COMBO_ANT
static INT32 opfunc_ant_ram_down(P_WMT_OP pWmtOp);
static INT32 opfunc_ant_ram_stat_get(P_WMT_OP pWmtOp);
#endif
#if CFG_WMT_LTE_COEX_HANDLING
static INT32 opfunc_idc_msg_handling(P_WMT_OP pWmtOp);
#endif
static INT32 opfunc_trigger_stp_assert(P_WMT_OP pWmtOp);
static INT32 opfunc_flash_patch_down(P_WMT_OP pWmtOp);
static INT32 opfunc_flash_patch_ver_get(P_WMT_OP pWmtOp);
static INT32 opfunc_utc_time_sync(P_WMT_OP pWmtOp);
static INT32 opfunc_fw_log_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_wlan_probe(P_WMT_OP pWmtOp);
static INT32 opfunc_wlan_remove(P_WMT_OP pWmtOp);
static INT32 opfunc_try_pwr_off(P_WMT_OP pWmtOp);
static INT32 opfunc_gps_mcu_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_blank_status_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_met_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_gps_suspend(P_WMT_OP pWmtOp);
static INT32 opfunc_get_consys_state(P_WMT_OP pWmtOp);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

static const UINT8 WMT_SLEEP_CMD[] = { 0x01, 0x03, 0x01, 0x00, 0x01 };
static const UINT8 WMT_SLEEP_EVT[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x01 };

static const UINT8 WMT_HOST_AWAKE_CMD[] = { 0x01, 0x03, 0x01, 0x00, 0x02 };
static const UINT8 WMT_HOST_AWAKE_EVT[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x02 };

static const UINT8 WMT_WAKEUP_CMD[] = { 0xFF };
static const UINT8 WMT_WAKEUP_EVT[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x03 };

static UINT8 WMT_THERM_CMD[] = { 0x01, 0x11, 0x01, 0x00,
	0x00			/*thermal sensor operation */
};
static UINT8 WMT_THERM_CTRL_EVT[] = { 0x02, 0x11, 0x01, 0x00, 0x00 };
static UINT8 WMT_THERM_READ_EVT[] = { 0x02, 0x11, 0x02, 0x00, 0x00, 0x00 };

static UINT8 WMT_EFUSE_CMD[] = { 0x01, 0x0D, 0x08, 0x00,
	0x01,			/*[4]operation, 0:init, 1:write 2:read */
	0x01,			/*[5]Number of register setting */
	0xAA, 0xAA,		/*[6-7]Address */
	0xBB, 0xBB, 0xBB, 0xBB	/*[8-11] Value */
};

static UINT8 WMT_EFUSE_EVT[] = { 0x02, 0x0D, 0x08, 0x00,
	0xAA,			/*[4]operation, 0:init, 1:write 2:read */
	0xBB,			/*[5]Number of register setting */
	0xCC, 0xCC,		/*[6-7]Address */
	0xDD, 0xDD, 0xDD, 0xDD	/*[8-11] Value */
};

static UINT8 WMT_DSNS_CMD[] = { 0x01, 0x0E, 0x02, 0x00, 0x01,
	0x00			/*desnse type */
};
static UINT8 WMT_DSNS_EVT[] = { 0x02, 0x0E, 0x01, 0x00, 0x00 };

/* TODO:[NewFeature][GeorgeKuo] Update register group in ONE CMD/EVT */
static UINT8 WMT_SET_REG_CMD[] = { 0x01, 0x08, 0x10, 0x00	/*length */
	    , 0x00		/*op: w(1) & r(2) */
	    , 0x01		/*type: reg */
	    , 0x00		/*res */
	    , 0x01		/*1 register */
	    , 0x00, 0x00, 0x00, 0x00	/* addr */
	    , 0x00, 0x00, 0x00, 0x00	/* value */
	    , 0xFF, 0xFF, 0xFF, 0xFF	/*mask */
};

static UINT8 WMT_SET_REG_WR_EVT[] = { 0x02, 0x08, 0x04, 0x00	/*length */
	    , 0x00		/*S: 0 */
	    , 0x00		/*type: reg */
	    , 0x00		/*rev */
	    , 0x01		/*1 register */
	    /* , 0x00, 0x00, 0x00, 0x00 *//* addr */
	    /* , 0x00, 0x00, 0x00, 0x00 *//* value */
};

static UINT8 WMT_SET_REG_RD_EVT[] = { 0x02, 0x08, 0x04, 0x00	/*length */
	    , 0x00		/*S: 0 */
	    , 0x00		/*type: reg */
	    , 0x00		/*rev */
	    , 0x01		/*1 register */
	    , 0x00, 0x00, 0x00, 0x00	/* addr */
	    , 0x00, 0x00, 0x00, 0x00	/* value */
};

#ifdef CONFIG_MTK_COMBO_ANT
static UINT8 WMT_ANT_RAM_STA_GET_CMD[] = { 0x01, 0x06, 0x02, 0x00, 0x05, 0x02
};
static UINT8 WMT_ANT_RAM_STA_GET_EVT[] = { 0x02, 0x06, 0x03, 0x00	/*length */
	    , 0x05, 0x02, 0x00	/*S: result */
};
static UINT8 WMT_ANT_RAM_DWN_CMD[] = { 0x01, 0x15, 0x00, 0x00, 0x01
};
static UINT8 WMT_ANT_RAM_DWN_EVT[] = { 0x02, 0x15, 0x01, 0x00	/*length */
	, 0x00
};
#endif

static UINT8 WMT_FLASH_PATCH_VER_GET_CMD[] = { 0x01, 0x01, 0x05, 0x00	/*length*/
	, 0x06, 0x00, 0x00, 0x00, 0x00 /*flash patch type*/
};

static UINT8 WMT_FLASH_PATCH_VER_GET_EVT[] = { 0x02, 0x01, 0x09, 0x00	/*length */
	, 0x06, 0x00, 0x00, 0x00, 0x00	/*flash patch type*/
	, 0x00, 0x00, 0x00, 0x00	/*flash patch version*/
};

static UINT8 WMT_FLASH_PATCH_DWN_CMD[] = { 0x01, 0x01, 0x0d, 0x00, 0x05
};

static UINT8 WMT_FLASH_PATCH_DWN_EVT[] = { 0x02, 0x01, 0x01, 0x00	/*length */
	, 0x00
};

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
static UINT8 WMT_UTC_SYNC_CMD[] = { 0x01, 0xF0, 0x09, 0x00, 0x02
	, 0x00, 0x00, 0x00, 0x00 /*UTC time second unit*/
	, 0x00, 0x00, 0x00, 0x00 /*UTC time microsecond unit*/
};
static UINT8 WMT_UTC_SYNC_EVT[] = { 0x02, 0xF0, 0x02, 0x00, 0x02, 0x00
};

static UINT8 WMT_BLANK_STATUS_CMD[] = { 0x01, 0xF0, 0x02, 0x00, 0x03, 0x00 };
static UINT8 WMT_BLANK_STATUS_EVT[] = { 0x02, 0xF0, 0x02, 0x00, 0x03, 0x00 };
#endif

static UINT8 WMT_FW_LOG_CTRL_CMD[] = { 0x01, 0xF0, 0x04, 0x00, 0x01
	, 0x00 /* subsys type */
	, 0x00 /* on/off */
	, 0x00 /* level (subsys-specific) */
};
static UINT8 WMT_FW_LOG_CTRL_EVT[] = { 0x02, 0xF0, 0x02, 0x00, 0x01, 0x00 };

/* GeorgeKuo: Use designated initializers described in
 * http://gcc.gnu.org/onlinedocs/gcc-4.0.4/gcc/Designated-Inits.html
 */

static const WMT_OPID_FUNC wmt_core_opfunc[] = {
	[WMT_OPID_HIF_CONF] = opfunc_hif_conf,
	[WMT_OPID_PWR_ON] = opfunc_pwr_on,
	[WMT_OPID_PWR_OFF] = opfunc_pwr_off,
	[WMT_OPID_FUNC_ON] = opfunc_func_on,
	[WMT_OPID_FUNC_OFF] = opfunc_func_off,
	[WMT_OPID_REG_RW] = opfunc_reg_rw,	/* TODO:[ChangeFeature][George] is this OP obsoleted? */
	[WMT_OPID_EXIT] = opfunc_exit,
	[WMT_OPID_PWR_SV] = opfunc_pwr_sv,
	[WMT_OPID_DSNS] = opfunc_dsns,
	[WMT_OPID_LPBK] = opfunc_lpbk,
	[WMT_OPID_CMD_TEST] = opfunc_cmd_test,
	[WMT_OPID_HW_RST] = opfunc_hw_rst,
	[WMT_OPID_SW_RST] = opfunc_sw_rst,
	[WMT_OPID_STP_RST] = opfunc_stp_rst,
	[WMT_OPID_THERM_CTRL] = opfunc_therm_ctrl,
	[WMT_OPID_EFUSE_RW] = opfunc_efuse_rw,
	[WMT_OPID_GPIO_CTRL] = opfunc_gpio_ctrl,
	[WMT_OPID_SDIO_CTRL] = opfunc_sdio_ctrl,
	[WMT_OPID_GPIO_STATE] = opfunc_pin_state,
	[WMT_OPID_BGW_DS] = opfunc_bgw_ds,
	[WMT_OPID_SET_MCU_CLK] = opfunc_set_mcu_clk,
	[WMT_OPID_ADIE_LPBK_TEST] = opfunc_adie_lpbk_test,
#ifdef CONFIG_MTK_COMBO_ANT
	[WMT_OPID_ANT_RAM_DOWN] = opfunc_ant_ram_down,
	[WMT_OPID_ANT_RAM_STA_GET] = opfunc_ant_ram_stat_get,
#endif
#if CFG_WMT_LTE_COEX_HANDLING
	[WMT_OPID_IDC_MSG_HANDLING] = opfunc_idc_msg_handling,
#endif
	[WMT_OPID_TRIGGER_STP_ASSERT] = opfunc_trigger_stp_assert,
	[WMT_OPID_FLASH_PATCH_DOWN] = opfunc_flash_patch_down,
	[WMT_OPID_FLASH_PATCH_VER_GET] = opfunc_flash_patch_ver_get,
	[WMT_OPID_UTC_TIME_SYNC] = opfunc_utc_time_sync,
	[WMT_OPID_FW_LOG_CTRL] = opfunc_fw_log_ctrl,
	[WMT_OPID_WLAN_PROBE] = opfunc_wlan_probe,
	[WMT_OPID_WLAN_REMOVE] = opfunc_wlan_remove,
	[WMT_OPID_GPS_MCU_CTRL] = opfunc_gps_mcu_ctrl,
	[WMT_OPID_TRY_PWR_OFF] = opfunc_try_pwr_off,
	[WMT_OPID_BLANK_STATUS_CTRL] = opfunc_blank_status_ctrl,
	[WMT_OPID_MET_CTRL] = opfunc_met_ctrl,
	[WMT_OPID_GPS_SUSPEND] = opfunc_gps_suspend,
	[WMT_OPID_GET_CONSYS_STATE] = opfunc_get_consys_state,
};

atomic_t g_wifi_on_off_ready;

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
INT32 wmt_core_init(VOID)
{
	INT32 i = 0;

	osal_memset(&gMtkWmtCtx, 0, osal_sizeof(gMtkWmtCtx));
	/* gMtkWmtCtx.p_ops is cleared to NULL */

	/* default FUNC_OFF state */
	for (i = 0; i < WMTDRV_TYPE_MAX; ++i) {
		/* WinMo is default to DRV_STS_UNREG; */
		gMtkWmtCtx.eDrvStatus[i] = DRV_STS_POWER_OFF;
	}

	atomic_set(&g_wifi_on_off_ready, 0);

	return 0;
}

INT32 wmt_core_deinit(VOID)
{
	/* return to init state */
	osal_memset(&gMtkWmtCtx, 0, osal_sizeof(gMtkWmtCtx));
	/* gMtkWmtCtx.p_ops is cleared to NULL */
	return 0;
}

/* TODO: [ChangeFeature][George] Is wmt_ctrl a good interface? maybe not...... */
/* parameters shall be copied in/from ctrl buffer, which is also a size-wasting buffer. */
INT32
wmt_core_tx(const PUINT8 pData, const UINT32 size, PUINT32 writtenSize, const MTK_WCN_BOOL bRawFlag)
{
	INT32 iRet = 0;
	INT32 retry_times = 0;
	INT32 max_retry_times = 0;
	INT32 retry_delay_ms = 0;
	ENUM_WMT_CHIP_TYPE chip_type;

	chip_type = wmt_detect_get_chip_type();
	iRet = wmt_ctrl_tx_ex(pData, size, writtenSize, bRawFlag);
	if (*writtenSize == 0 && (chip_type == WMT_CHIP_TYPE_SOC)) {
		retry_times = 0;
		max_retry_times = 3;
		retry_delay_ms = 360;
		WMT_WARN_FUNC("WMT-CORE: wmt_ctrl_tx_ex failed and written ret:%d, maybe no winspace in STP layer\n",
			      *writtenSize);
		while ((*writtenSize == 0) && (retry_times < max_retry_times)) {
			WMT_ERR_FUNC("WMT-CORE: retrying, wait for %d ms\n", retry_delay_ms);
			osal_sleep_ms(retry_delay_ms);

			iRet = wmt_ctrl_tx_ex(pData, size, writtenSize, bRawFlag);
			retry_times++;
		}
	}
	return iRet;
}

INT32 wmt_core_rx(PUINT8 pBuf, UINT32 bufLen, PUINT32 readSize)
{
	INT32 iRet;
	WMT_CTRL_DATA ctrlData;

	ctrlData.ctrlId = WMT_CTRL_RX;
	ctrlData.au4CtrlData[0] = (SIZE_T) pBuf;
	ctrlData.au4CtrlData[1] = bufLen;
	ctrlData.au4CtrlData[2] = (SIZE_T) readSize;

	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/* ERROR */
		WMT_ERR_FUNC("WMT-CORE: wmt_core_ctrl failed: WMT_CTRL_RX, iRet:%d\n", iRet);
		osal_assert(0);
	}
	return iRet;
}

INT32 wmt_core_rx_flush(UINT32 type)
{
	INT32 iRet;
	WMT_CTRL_DATA ctrlData;

	ctrlData.ctrlId = WMT_CTRL_RX_FLUSH;
	ctrlData.au4CtrlData[0] = (UINT32) type;

	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/* ERROR */
		WMT_ERR_FUNC("WMT-CORE: wmt_core_ctrl failed: WMT_CTRL_RX_FLUSH, iRet:%d\n", iRet);
		osal_assert(0);
	}
	return iRet;
}

INT32 wmt_core_func_ctrl_cmd(ENUM_WMTDRV_TYPE_T type, MTK_WCN_BOOL fgEn)
{
	INT32 iRet = 0;
	UINT32 u4WmtCmdPduLen;
	UINT32 u4WmtEventPduLen;
	UINT32 u4ReadSize;
	UINT32 u4WrittenSize;
	WMT_PKT rWmtPktCmd;
	WMT_PKT rWmtPktEvent;
	MTK_WCN_BOOL fgFail;

	/* TODO:[ChangeFeature][George] remove WMT_PKT. replace it with hardcoded arrays. */
	/* Using this struct relies on compiler's implementation and pack() settings */
	osal_memset(&rWmtPktCmd, 0, osal_sizeof(rWmtPktCmd));
	osal_memset(&rWmtPktEvent, 0, osal_sizeof(rWmtPktEvent));

	rWmtPktCmd.eType = (UINT8) WMT_PKT_TYPE_CMD;
	rWmtPktCmd.eOpCode = (UINT8) OPCODE_FUNC_CTRL;

	/* Flag field: driver type */
	rWmtPktCmd.aucParam[0] = (UINT8) type;
	/* Parameter field: ON/OFF */
	rWmtPktCmd.aucParam[1] = (fgEn == WMT_FUNC_CTRL_ON) ? 1 : 0;
	rWmtPktCmd.u2SduLen = WMT_FLAG_LEN + WMT_FUNC_CTRL_PARAM_LEN;	/* (2) */

	/* WMT Header + WMT SDU */
	u4WmtCmdPduLen = WMT_HDR_LEN + rWmtPktCmd.u2SduLen;	/* (6) */
	u4WmtEventPduLen = WMT_HDR_LEN + WMT_STS_LEN;	/* (5) */

	do {
		fgFail = MTK_WCN_BOOL_TRUE;
/* iRet = (*kal_stp_tx)((PUINT8)&rWmtPktCmd, u4WmtCmdPduLen, &u4WrittenSize); */
		iRet =
		    wmt_core_tx((PUINT8) &rWmtPktCmd, u4WmtCmdPduLen, &u4WrittenSize,
				MTK_WCN_BOOL_FALSE);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: wmt_func_ctrl_cmd kal_stp_tx failed\n");
			break;
		}

		iRet = wmt_core_rx((PUINT8) &rWmtPktEvent, u4WmtEventPduLen, &u4ReadSize);
		if (iRet) {
			WMT_ERR_FUNC
				("WMT firwmare no rx event, trigger f/w assert. sub-driver type:%d, state(%d)\n",
				type, fgEn);
			wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 32);
			break;
		}

		/* Error Checking */
		if (rWmtPktEvent.eType != WMT_PKT_TYPE_EVENT) {
			WMT_ERR_FUNC
			    ("WMT-CORE: wmt_func_ctrl_cmd WMT_PKT_TYPE_EVENT != rWmtPktEvent.eType %d\n",
			     rWmtPktEvent.eType);
			break;
		}

		if (rWmtPktCmd.eOpCode != rWmtPktEvent.eOpCode) {
			WMT_ERR_FUNC
			    ("WMT-CORE: wmt_func_ctrl_cmd rWmtPktCmd.eOpCode(0x%x) != rWmtPktEvent.eType(0x%x)\n",
			     rWmtPktCmd.eOpCode, rWmtPktEvent.eOpCode);
			break;
		}

		if (u4WmtEventPduLen != (rWmtPktEvent.u2SduLen + WMT_HDR_LEN)) {
			WMT_ERR_FUNC
			    ("WMT-CORE: wmt_func_ctrl_cmd u4WmtEventPduLen(0x%x) != rWmtPktEvent.u2SduLen(0x%x)+4\n",
			     u4WmtEventPduLen, rWmtPktEvent.u2SduLen);
			break;
		}
		/* Status field of event check */
		if (rWmtPktEvent.aucParam[0] != 0) {
			WMT_ERR_FUNC("WMT-CORE: wmt_func_ctrl_cmd, 0 != status(%d)\n",
				     rWmtPktEvent.aucParam[0]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;
	} while (0);

	if (fgFail == MTK_WCN_BOOL_FALSE) {
		/* WMT_INFO_FUNC("WMT-CORE: wmt_func_ctrl_cmd OK!\n"); */
		return 0;
	}
	WMT_ERR_FUNC("WMT-CORE: wmt_func_ctrl_cmd 0x%x FAIL\n", rWmtPktCmd.aucParam[0]);
	return -2;
}

INT32 wmt_core_opid_handler(P_WMT_OP pWmtOp)
{
	UINT32 opId;
	INT32 ret;

	opId = pWmtOp->opId;

	if (wmt_core_opfunc[opId]) {
		ret = (*(wmt_core_opfunc[opId])) (pWmtOp);	/*wmtCoreOpidHandlerPack[].opHandler */
		return ret;
	}
	WMT_ERR_FUNC("WMT-CORE: null handler (%d)\n", pWmtOp->opId);
	return -2;
}

INT32 wmt_core_opid(P_WMT_OP pWmtOp)
{

	/*sanity check */
	if (pWmtOp == NULL) {
		WMT_ERR_FUNC("null pWmtOP\n");
		/*print some message with error info */
		return -1;
	}

	if (pWmtOp->opId >= WMT_OPID_MAX) {
		WMT_ERR_FUNC("WMT-CORE: invalid OPID(%d)\n", pWmtOp->opId);
		return -2;
	}
	/* TODO: [FixMe][GeorgeKuo] do sanity check to const function table when init and skip checking here */
	return wmt_core_opid_handler(pWmtOp);
}

INT32 wmt_core_ctrl(ENUM_WMT_CTRL_T ctrId, PULONG pPa1, PULONG pPa2)
{
	INT32 iRet = -1;
	WMT_CTRL_DATA ctrlData;
	SIZE_T val1 = (pPa1) ? *pPa1 : 0;
	SIZE_T val2 = (pPa2) ? *pPa2 : 0;

	ctrlData.ctrlId = (SIZE_T) ctrId;
	ctrlData.au4CtrlData[0] = val1;
	ctrlData.au4CtrlData[1] = val2;

	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/* ERROR */
		WMT_ERR_FUNC
		    ("WMT-CORE: wmt_core_ctrl failed: id(%d), type(%zu), value(%zu) iRet:(%d)\n",
		     ctrId, val1, val2, iRet);
		osal_assert(0);
	} else {
		if (pPa1)
			*pPa1 = ctrlData.au4CtrlData[0];
		if (pPa2)
			*pPa2 = ctrlData.au4CtrlData[1];
	}
	return iRet;
}


VOID wmt_core_dump_data(PUINT8 pData, PUINT8 pTitle, UINT32 len)
{
	PUINT8 ptr = pData;
	INT32 k = 0;

	WMT_INFO_FUNC("%s len=%d\n", pTitle, len);
	for (k = 0; k < len; k++) {
		if (k % 16 == 0)
			WMT_INFO_FUNC("\n");
		WMT_INFO_FUNC("0x%02x ", *ptr);
		ptr++;
	}
	WMT_INFO_FUNC("--end\n");
}

/*!
 * \brief An WMT-CORE function to support read, write, and read after write to
 * an internal register.
 *
 * Detailed description.
 *
 * \param isWrite 1 for write, 0 for read
 * \param offset of register to be written or read
 * \param pVal a pointer to the 32-bit value to be writtern or read
 * \param mask a 32-bit mask to be applied for the read or write operation
 *
 * \retval 0 operation success
 * \retval -1 invalid parameters
 * \retval -2 tx cmd fail
 * \retval -3 rx event fail
 * \retval -4 read check error
 */
INT32 wmt_core_reg_rw_raw(UINT32 isWrite, UINT32 offset, PUINT32 pVal, UINT32 mask)
{
	INT32 iRet;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	WMT_SET_REG_CMD[4] = (isWrite) ? 0x1 : 0x2;	/* w:1, r:2 */
	osal_memcpy(&WMT_SET_REG_CMD[8], &offset, 4);	/* offset */
	osal_memcpy(&WMT_SET_REG_CMD[12], pVal, 4);	/* [2] is var addr */
	osal_memcpy(&WMT_SET_REG_CMD[16], &mask, 4);	/* mask */

	/* send command */
	iRet = wmt_core_tx(WMT_SET_REG_CMD, sizeof(WMT_SET_REG_CMD), &u4Res, MTK_WCN_BOOL_FALSE);
	if ((iRet) || (u4Res != sizeof(WMT_SET_REG_CMD))) {
		WMT_ERR_FUNC("Tx REG_CMD fail!(%d) len (%d, %zu)\n", iRet, u4Res,
			     sizeof(WMT_SET_REG_CMD));
		return -2;
	}

	/* receive event */
	evtLen = (isWrite) ? sizeof(WMT_SET_REG_WR_EVT) : sizeof(WMT_SET_REG_RD_EVT);
	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if ((iRet) || (u4Res != evtLen)) {
		WMT_ERR_FUNC("Rx REG_EVT fail!(%d) len(%d, %d)\n", iRet, u4Res, evtLen);
		if (isWrite)
			WMT_INFO_FUNC("buf:[%2X,%2X,%2X,%2X,%2X] evt:[%2X,%2X,%2X,%2X,%2X]\n",
					evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
					WMT_SET_REG_WR_EVT[0], WMT_SET_REG_WR_EVT[1],
					WMT_SET_REG_WR_EVT[2], WMT_SET_REG_WR_EVT[3],
					WMT_SET_REG_WR_EVT[4]);
		else
			WMT_INFO_FUNC("buf:[%2X,%2X,%2X,%2X,%2X] evt:[%2X,%2X,%2X,%2X,%2X]\n",
					evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
					WMT_SET_REG_RD_EVT[0], WMT_SET_REG_RD_EVT[1],
					WMT_SET_REG_RD_EVT[2], WMT_SET_REG_RD_EVT[3],
					WMT_SET_REG_RD_EVT[4]);
		mtk_wcn_stp_dbg_dump_package();
		wmt_core_trigger_assert();
		return -3;
	}

	if (!isWrite) {
		UINT32 rxEvtAddr;
		UINT32 txCmdAddr;

		osal_memcpy(&txCmdAddr, &WMT_SET_REG_CMD[8], 4);
		osal_memcpy(&rxEvtAddr, &evtBuf[8], 4);

		/* check read result */
		if (txCmdAddr != rxEvtAddr) {
			WMT_ERR_FUNC("Check read addr fail (0x%08x, 0x%08x)\n", rxEvtAddr,
				     txCmdAddr);
			return -4;
		}
		WMT_DBG_FUNC("Check read addr(0x%08x) ok\n", rxEvtAddr);
		osal_memcpy(pVal, &evtBuf[12], 4);
	}

	/* no error here just return 0 */
	return 0;
}

INT32 wmt_core_init_script_retry(struct init_script *script, INT32 count, INT32 retry, INT32 dump_err_log)
{
	UINT8 evtBuf[256];
	UINT32 u4Res;
	INT32 i = 0;
	INT32 iRet;
	INT32 err = 0;

	do {
		err = 0;
		for (i = 0; i < count; i++) {
			WMT_DBG_FUNC("WMT-CORE: init_script operation %s start\n", script[i].str);
			/* CMD */
			/* iRet = (*kal_stp_tx)(script[i].cmd, script[i].cmdSz, &u4Res); */
			iRet = wmt_core_tx(script[i].cmd, script[i].cmdSz, &u4Res, MTK_WCN_BOOL_FALSE);
			if (iRet || (u4Res != script[i].cmdSz)) {
				WMT_ERR_FUNC("WMT-CORE: write (%s) iRet(%d) cmd len err(%d, %d)\n",
					     script[i].str, iRet, u4Res, script[i].cmdSz);

				err = -1;
				break;
			}
			/* EVENT BUF */

			osal_memset(evtBuf, 0, sizeof(evtBuf));
			iRet = wmt_core_rx(evtBuf, script[i].evtSz, &u4Res);
			if (iRet || (u4Res != script[i].evtSz)) {
				WMT_ERR_FUNC("WMT-CORE: read (%s) iRet(%d) evt len err(rx:%d, exp:%d)\n",
					     script[i].str, iRet, u4Res, script[i].evtSz);
				if (dump_err_log == 1)
					mtk_wcn_stp_dbg_dump_package();

				err = -1;
				break;
			}
			/* RESULT */
			if (evtBuf[1] != 0x14) { /*workaround RF calibration data EVT, do not care this EVT*/
				if (osal_memcmp(evtBuf, script[i].evt, script[i].evtSz) != 0) {
					WMT_ERR_FUNC("WMT-CORE:compare %s result error\n", script[i].str);
					WMT_ERR_FUNC
						("WMT-CORE:rx(%d):[%02X,%02X,%02X,%02X,%02X]\n",
						 u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4]);
					WMT_ERR_FUNC
						("WMT-CORE:exp(%d):[%02X,%02X,%02X,%02X,%02X]\n",
						 script[i].evtSz, script[i].evt[0], script[i].evt[1], script[i].evt[2],
						 script[i].evt[3], script[i].evt[4]);
					if (dump_err_log == 1)
						mtk_wcn_stp_dbg_dump_package();

					err = -1;
					break;
				}
			}
			WMT_DBG_FUNC("init_script operation %s ok\n", script[i].str);
		}
		retry--;
	} while (retry >= 0 && err < 0);

	return (i == count) ? 0 : -1;
}


INT32 wmt_core_init_script(struct init_script *script, INT32 count)
{
	return wmt_core_init_script_retry(script, count, 0, 1);
}

static INT32 wmt_core_trigger_assert(VOID)
{
	INT32 ret = 0;
	UINT32 u4Res;
	UINT32 tstCmdSz = 0;
	UINT32 tstEvtSz = 0;
	UINT8 tstCmd[64];
	UINT8 tstEvt[64];
	UINT8 WMT_ASSERT_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x08 };
	UINT8 WMT_ASSERT_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };

	WMT_INFO_FUNC("Send Assert command !\n");
	tstCmdSz = osal_sizeof(WMT_ASSERT_CMD);
	tstEvtSz = osal_sizeof(WMT_ASSERT_EVT);
	osal_memcpy(tstCmd, WMT_ASSERT_CMD, tstCmdSz);
	osal_memcpy(tstEvt, WMT_ASSERT_EVT, tstEvtSz);

	ret = wmt_core_tx((PUINT8) tstCmd, tstCmdSz, &u4Res, MTK_WCN_BOOL_FALSE);
	if (ret || (u4Res != tstCmdSz)) {
		WMT_ERR_FUNC("WMT-CORE: wmt_cmd_test iRet(%d) cmd len err(%d, %d)\n", ret, u4Res,
			     tstCmdSz);
		ret = -1;
	}
	return ret;
}

static INT32 wmt_core_stp_init(VOID)
{
	INT32 iRet = -1;
	ULONG ctrlPa1;
	ULONG ctrlPa2;
	UINT8 co_clock_type;
	P_WMT_CTX pctx = &gMtkWmtCtx;
	P_WMT_GEN_CONF pWmtGenConf = NULL;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO)
		wmt_conf_read_file();
	gDevWmt.rWmtGenConf.co_clock_flag = wmt_lib_co_clock_flag_get();

	pWmtGenConf = wmt_conf_get_cfg();
	if (pWmtGenConf == NULL)
		WMT_ERR_FUNC("WMT-CORE: wmt_conf_get_cfg return NULL!!\n");
	if (!(pctx->wmtInfoBit & WMT_OP_HIF_BIT)) {
		WMT_ERR_FUNC("WMT-CORE: no hif info!\n");
		osal_assert(0);
		return -1;
	}


	/* 4 <0> turn on SDIO2 for common SDIO */
	if (pctx->wmtHifConf.hifType == WMT_HIF_SDIO) {
		ctrlPa1 = WMT_SDIO_SLOT_SDIO2;
		ctrlPa2 = 1;	/* turn on SDIO2 slot */
		iRet = wmt_core_ctrl(WMT_CTRL_SDIO_HW, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: turn on SLOT_SDIO2 fail (%d)\n", iRet);
			osal_assert(0);

			return -2;
		}
		pctx->eDrvStatus[WMTDRV_TYPE_SDIO2] = DRV_STS_FUNC_ON;

		ctrlPa1 = WMT_SDIO_FUNC_STP;
		ctrlPa2 = 1;	/* turn on STP driver */
		iRet = wmt_core_ctrl(WMT_CTRL_SDIO_FUNC, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: turn on SDIO_FUNC_STP func fail (%d)\n", iRet);

			/* check all sub-func and do power off */
			return -3;
		}
	}
	/* 4 <1> open stp */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_OPEN, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt open stp\n");
		return -4;
	}

	if (pctx->wmtHifConf.hifType == WMT_HIF_UART) {
		ctrlPa1 = WMT_DEFAULT_BAUD_RATE;
		ctrlPa2 = 0;
		iRet = wmt_core_ctrl(WMT_CTRL_HOST_BAUDRATE_SET, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: change host baudrate(%d) fails\n",
				     pctx->wmtHifConf.au4HifConf[0]);
			return -5;
		}
	}
	/* WMT_DBG_FUNC("WMT-CORE: change host baudrate(%d) ok\n", gMtkWmtCtx.wmtHifConf.au4HifConf[0]); */

	/* 4 <1.5> disable and un-ready stp */
	ctrlPa1 = WMT_STP_CONF_EN;
	ctrlPa2 = 0;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	ctrlPa1 = WMT_STP_CONF_RDY;
	ctrlPa2 = 0;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);

	/* 4 <2> set mode and enable */
	if (pctx->wmtHifConf.hifType == WMT_HIF_UART) {
		ctrlPa1 = WMT_STP_CONF_MODE;
		ctrlPa2 = MTKSTP_UART_MAND_MODE;
		iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	} else if (pctx->wmtHifConf.hifType == WMT_HIF_SDIO) {

		ctrlPa1 = WMT_STP_CONF_MODE;
		ctrlPa2 = MTKSTP_SDIO_MODE;
		iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
		if (iRet)
			WMT_ERR_FUNC(" confif SDIO_MODE fail!!!!\n");
	}
	if (pctx->wmtHifConf.hifType == WMT_HIF_BTIF) {
		ctrlPa1 = WMT_STP_CONF_MODE;
		ctrlPa2 = MTKSTP_BTIF_MAND_MODE;
		iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	}
	ctrlPa1 = WMT_STP_CONF_EN;
	ctrlPa2 = 1;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: stp_init <1><2> fail:%d\n", iRet);
		return -7;
	}
	/* TODO: [ChangeFeature][GeorgeKuo] can we apply raise UART baud rate firstly for ALL supported chips??? */

#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
	WMT_DBG_FUNC("disable deep sleep featrue before the first command to firmware\n");
	wmt_lib_deep_sleep_flag_set(MTK_WCN_BOOL_FALSE);
#endif

	iRet = wmt_core_hw_check();
	if (iRet) {
		WMT_ERR_FUNC("hw_check fail:%d\n", iRet);
		return -8;
	}
	/* mtkWmtCtx.p_ic_ops is identified and checked ok */
	if ((pctx->p_ic_ops->co_clock_ctrl != NULL) && (pWmtGenConf != NULL)) {
		co_clock_type = (pWmtGenConf->co_clock_flag & 0x0f);
		(*(pctx->p_ic_ops->co_clock_ctrl)) (co_clock_type == 0 ? WMT_CO_CLOCK_DIS : WMT_CO_CLOCK_EN);
	} else {
		WMT_WARN_FUNC("pctx->p_ic_ops->co_clock_ctrl(%p), pWmtGenConf(%p)\n", pctx->p_ic_ops->co_clock_ctrl,
			      pWmtGenConf);
	}
	osal_assert(pctx->p_ic_ops->sw_init != NULL);
	if (pctx->p_ic_ops->sw_init != NULL) {
		iRet = (*(pctx->p_ic_ops->sw_init)) (&pctx->wmtHifConf);
	} else {
		WMT_ERR_FUNC("gMtkWmtCtx.p_ic_ops->sw_init is NULL\n");
		return -9;
	}
	if (iRet) {
		WMT_ERR_FUNC("gMtkWmtCtx.p_ic_ops->sw_init fail:%d\n", iRet);
		return -10;
	}

	/* send UTC time sync command after connsys power on or chip reset */
	opfunc_utc_time_sync(NULL);

	/* 4 <10> set stp ready */
	ctrlPa1 = WMT_STP_CONF_RDY;
	ctrlPa2 = 1;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	return iRet;
}

static INT32 wmt_core_stp_deinit(VOID)
{
	INT32 iRet;
	ULONG ctrlPa1;
	ULONG ctrlPa2;

	WMT_DBG_FUNC(" start\n");

	if (gMtkWmtCtx.p_ic_ops == NULL) {
		WMT_WARN_FUNC("gMtkWmtCtx.p_ic_ops is NULL\n");
		goto deinit_ic_ops_done;
	}
	if (gMtkWmtCtx.p_ic_ops->sw_deinit != NULL) {
		iRet = (*(gMtkWmtCtx.p_ic_ops->sw_deinit)) (&gMtkWmtCtx.wmtHifConf);
		/* unbind WMT-IC */
		gMtkWmtCtx.p_ic_ops = NULL;
	} else {
		WMT_ERR_FUNC("gMtkWmtCtx.p_ic_ops->sw_init is NULL\n");
	}

deinit_ic_ops_done:

	/* 4 <1> un-ready, disable, and close stp. */
	ctrlPa1 = WMT_STP_CONF_RDY;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	ctrlPa1 = WMT_STP_CONF_EN;
	ctrlPa2 = 0;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CLOSE, &ctrlPa1, &ctrlPa2);

	/* 4 <1.1> turn off SDIO2 for common SDIO */
	if (gMtkWmtCtx.wmtHifConf.hifType == WMT_HIF_SDIO) {
		ctrlPa1 = WMT_SDIO_FUNC_STP;
		ctrlPa2 = 0;	/* turn off STP driver */
		iRet = wmt_core_ctrl(WMT_CTRL_SDIO_FUNC, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_WARN_FUNC("turn off SDIO_FUNC_STP fail (%d)\n", iRet);
			/* Anyway, continue turning SDIO HW off */
		} else {
			WMT_DBG_FUNC("turn off SDIO_FUNC_STP ok\n");
		}

		ctrlPa1 = WMT_SDIO_SLOT_SDIO2;
		ctrlPa2 = 0;	/* turn off SDIO2 slot */
		iRet = wmt_core_ctrl(WMT_CTRL_SDIO_HW, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_WARN_FUNC("turn off SDIO2 HW fail (%d)\n", iRet);
			/* Anyway, continue turning STP SDIO to POWER OFF state */
		} else
			WMT_DBG_FUNC("turn off SDIO2 HW ok\n");
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO2] = DRV_STS_POWER_OFF;
	}

	if (iRet)
		WMT_WARN_FUNC("end with fail:%d\n", iRet);

	return iRet;
}

static VOID wmt_core_dump_func_state(PINT8 pSource)
{
	WMT_INFO_FUNC
	    ("[%s]status(b:%d f:%d g:%d gl5:%d w:%d lpbk:%d coredump:%d wmt:%d ant:%d sd1:%d sd2:%d stp:%d)\n",
	     (pSource == NULL ? (PINT8) "CORE" : pSource), gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT],
	     gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_FM], gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPS],
	     gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPSL5],
	     gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI], gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK],
	     gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP], gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT],
	     gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_ANT], gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO1],
	     gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO2], gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_STP]
	    );
	return;

}

ENUM_DRV_STS wmt_core_get_drv_status(ENUM_WMTDRV_TYPE_T type)
{
	if ((type < WMTDRV_TYPE_BT) || (type >= WMTDRV_TYPE_MAX))
		return DRV_STS_POWER_OFF;
	return gMtkWmtCtx.eDrvStatus[type];
}

MTK_WCN_BOOL wmt_core_patch_check(UINT32 u4PatchVer, UINT32 u4HwVer)
{
	if (MAJORNUM(u4HwVer) != MAJORNUM(u4PatchVer)) {
		/*major no. does not match */
		WMT_ERR_FUNC("WMT-CORE: chip version(0x%x) does not match patch version(0x%x)\n",
			     u4HwVer, u4PatchVer);
		return MTK_WCN_BOOL_FALSE;
	}
	return MTK_WCN_BOOL_TRUE;
}

static INT32 wmt_core_hw_check(VOID)
{
	UINT32 chipid;
	P_WMT_IC_OPS p_ops;
	INT32 iret;

	/* 1. get chip id */
	chipid = 0;
	WMT_LOUD_FUNC("before read hwcode (chip id)\n");
	iret = wmt_core_reg_rw_raw(0, GEN_HCR, &chipid, GEN_HCR_MASK);	/* read 0x80000008 */
	if (iret) {
#if defined(KERNEL_clk_buf_show_status_info)
		KERNEL_clk_buf_show_status_info();  /* dump clock buffer */
#endif
		WMT_ERR_FUNC("get hwcode (chip id) fail (%d)\n", iret);
		return -2;
	}
	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		if (wmt_lib_get_icinfo(WMTCHIN_IPVER))
			chipid = wmt_plat_get_soc_chipid();
	}
	WMT_INFO_FUNC("get hwcode (chip id) (0x%x)\n", chipid);

	/* TODO:[ChangeFeature][George]: use a better way to select a correct ops table based on chip id */
	switch (chipid) {
#if CFG_CORE_MT6620_SUPPORT
	case 0x6620:
		p_ops = &wmt_ic_ops_mt6620;
		break;
#endif
#if CFG_CORE_MT6628_SUPPORT
	case 0x6628:
		p_ops = &wmt_ic_ops_mt6628;
		break;
#endif

#if CFG_CORE_MT6630_SUPPORT
	case 0x6630:
		p_ops = &wmt_ic_ops_mt6630;
		break;
#endif

#if CFG_CORE_MT6632_SUPPORT
	case 0x6632:
		p_ops = &wmt_ic_ops_mt6632;
		break;
#endif
#if CFG_CORE_SOC_SUPPORT
	case 0x0690:
	case 0x6572:
	case 0x6582:
	case 0x6592:
	case 0x8127:
	case 0x6571:
	case 0x6752:
	case 0x0279:
	case 0x0326:
	case 0x0321:
	case 0x0335:
	case 0x0337:
	case 0x8163:
	case 0x6580:
	case 0x0551:
	case 0x8167:
	case 0x0507:
	case 0x0688:
	case 0x0699:
	case 0x0633:
	case 0x0713:
	case 0x0788:
	case 0x6765:
	case 0x6761:
	case 0x6779:
	case 0x6768:
	case 0x6785:
	case 0x6833:
	case 0x6853:
	case 0x6873:
	case 0x8168:
		p_ops = &wmt_ic_ops_soc;
		break;
#endif

	default:
		p_ops = (P_WMT_IC_OPS) NULL;
#if CFG_CORE_SOC_SUPPORT
		if (chipid - 0x600 == 0x7f90) {
			p_ops = &wmt_ic_ops_soc;
			chipid -= 0xf6d;
		}
#endif
		break;
	}

	if (p_ops == NULL) {
		WMT_ERR_FUNC("unsupported chip id (hw_code): 0x%x\n", chipid);
		return -3;
	} else if (wmt_core_ic_ops_check(p_ops) == MTK_WCN_BOOL_FALSE) {
		WMT_ERR_FUNC
		    ("chip id(0x%x) with null operation fp: init(0x%p), deinit(0x%p), pin_ctrl(0x%p), ver_chk(0x%p)\n",
		     chipid, p_ops->sw_init, p_ops->sw_deinit, p_ops->ic_pin_ctrl,
		     p_ops->ic_ver_check);
		return -4;
	}
	WMT_DBG_FUNC("chip id(0x%x) fp: init(0x%p), deinit(0x%p), pin_ctrl(0x%p), ver_chk(0x%p)\n",
		     chipid, p_ops->sw_init, p_ops->sw_deinit, p_ops->ic_pin_ctrl,
		     p_ops->ic_ver_check);
	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		wmt_ic_ops_soc.icId = chipid;
		wmt_ic_ops_soc.options = mtk_wcn_consys_get_options();
		WMT_INFO_FUNC("options = %llx", wmt_ic_ops_soc.options);
	}
	iret = p_ops->ic_ver_check();
	if (iret) {
		WMT_ERR_FUNC("chip id(0x%x) ver_check error:%d\n", chipid, iret);
		return -5;
	}

	WMT_DBG_FUNC("chip id(0x%x) ver_check ok\n", chipid);
	gMtkWmtCtx.p_ic_ops = p_ops;
	return 0;
}

static INT32 opfunc_hif_conf(P_WMT_OP pWmtOp)
{
	if (!(pWmtOp->u4InfoBit & WMT_OP_HIF_BIT)) {
		WMT_ERR_FUNC("WMT-CORE: no HIF_BIT in WMT_OP!\n");
		return -1;
	}

	if (gMtkWmtCtx.wmtInfoBit & WMT_OP_HIF_BIT) {
		WMT_ERR_FUNC("WMT-CORE: WMT HIF already exist. Just return\n");
		return 0;
	} else {
		gMtkWmtCtx.wmtInfoBit |= WMT_OP_HIF_BIT;
		WMT_ERR_FUNC("WMT-CORE: WMT HIF info added\n");
	}

	osal_memcpy(&gMtkWmtCtx.wmtHifConf,
		    &pWmtOp->au4OpData[0], osal_sizeof(gMtkWmtCtx.wmtHifConf));
	return 0;

}

static INT32 opfunc_pwr_on(P_WMT_OP pWmtOp)
{

	INT32 iRet;
	ULONG ctrlPa1;
	ULONG ctrlPa2;
	INT32 retry = WMT_PWRON_RTY_DFT;

	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] != DRV_STS_POWER_OFF) {
		WMT_ERR_FUNC("WMT-CORE: already powered on, WMT DRV_STS_[0x%x]\n",
			     gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]);
		osal_assert(0);
		return -1;
	}

pwr_on_rty:
	/* power on control */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_HW_PWR_ON, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: WMT_CTRL_HW_PWR_ON fail iRet(%d)\n", iRet);
		if (retry-- == 0) {
			WMT_INFO_FUNC("WMT-CORE: retry (%d)\n", retry);
			goto pwr_on_rty;
		}
		return -2;
	}
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_ON;

	/* init stp */
	iRet = wmt_core_stp_init();
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt_core_stp_init fail (%d)\n", iRet);
		osal_assert(0);

		/* deinit stp */
		iRet = wmt_core_stp_deinit();
		if (iRet)
			WMT_ERR_FUNC("WMT-CORE: wmt_core_stp_deinit() failed.\n");
		iRet = opfunc_pwr_off(pWmtOp);
		if (iRet)
			WMT_ERR_FUNC("WMT-CORE: opfunc_pwr_off fail during pwr_on retry\n");

		if (retry-- > 0) {
			WMT_INFO_FUNC("WMT-CORE: retry (%d)\n", retry);
			goto pwr_on_rty;
		}
		return -3;
	}

	WMT_DBG_FUNC("WMT-CORE: WMT [FUNC_ON]\n");
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_FUNC_ON;

	/* update blank status when ConnSys power on */
	wmt_blank_status_ctrl(wmt_dev_get_blank_state());

	mtk_wcn_consys_sleep_info_restore();

	/* What to do when state is changed from POWER_OFF to POWER_ON?
	 * 1. STP driver does s/w reset
	 * 2. UART does 0xFF wake up
	 * 3. SDIO does re-init command(changed to trigger by host)
	 */
	return iRet;

}

static INT32 opfunc_pwr_off(P_WMT_OP pWmtOp)
{

	INT32 iRet;
	ULONG ctrlPa1;
	ULONG ctrlPa2;

	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] == DRV_STS_POWER_OFF) {
		WMT_WARN_FUNC("WMT-CORE: WMT already off, WMT DRV_STS_[0x%x]\n",
			      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]);
		osal_assert(0);
		return -1;
	}
	if (g_pwr_off_flag == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("CONNSYS power off be disabled, maybe need trigger core dump!\n");
		osal_assert(0);
		return -2;
	}
	/* wmt and stp are initialized successfully */
	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] == DRV_STS_FUNC_ON) {
		iRet = wmt_core_stp_deinit();
		if (iRet) {
			WMT_WARN_FUNC("wmt_core_stp_deinit fail (%d)\n", iRet);
			/*should let run to power down chip */
		}
	}

	if (wmt_lib_power_lock_aquire() == 0) {
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_OFF;
		wmt_lib_power_lock_release();
	} else {
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_OFF;
		WMT_INFO_FUNC("wmt_lib_power_lock_aquire failed\n");
	}

	/* power off control */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_HW_PWR_OFF, &ctrlPa1, &ctrlPa2);
	if (iRet)
		WMT_WARN_FUNC("HW_PWR_OFF fail (%d)\n", iRet);
	else
		WMT_DBG_FUNC("HW_PWR_OFF ok\n");

	return iRet;

}

static INT32 opfunc_func_on(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	UINT32 drvType = pWmtOp->au4OpData[0];

	/* Check abnormal type */
	if (drvType >= WMTDRV_TYPE_MAX) {
		WMT_ERR_FUNC("abnormal Fun(%d)\n", drvType);
		osal_assert(0);
		return -1;
	}

	/* Check abnormal state */
	if ((gMtkWmtCtx.eDrvStatus[drvType] < DRV_STS_POWER_OFF)
	    || (gMtkWmtCtx.eDrvStatus[drvType] >= DRV_STS_MAX)) {
		WMT_ERR_FUNC("func(%d) status[0x%x] abnormal\n",
			     drvType, gMtkWmtCtx.eDrvStatus[drvType]);
		osal_assert(0);
		return -2;
	}

	if (WMTDRV_TYPE_GPSL5 == drvType)
		mtk_wcn_stp_set_support_gpsl5(1);

	/* check if func already on */
	if (gMtkWmtCtx.eDrvStatus[drvType] == DRV_STS_FUNC_ON) {
		WMT_WARN_FUNC("func(%d) already on\n", drvType);
		return 0;
	}
	/*enable power off flag, if flag=0, power off connsys will not be executed */
	mtk_wcn_set_connsys_power_off_flag(MTK_WCN_BOOL_TRUE);
	/* check if chip power on is needed */
	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] != DRV_STS_FUNC_ON) {
		iRet = opfunc_pwr_on(pWmtOp);
		if (iRet) {
			WMT_ERR_FUNC("func(%d) pwr_on fail(%d)\n", drvType, iRet);
			osal_assert(0);

			/* check all sub-func and do power off */
			return -3;
		}
	}

	if (WMTDRV_TYPE_WMT > drvType || WMTDRV_TYPE_ANT == drvType || WMTDRV_TYPE_GPSL5 == drvType) {
		if (gpWmtFuncOps[drvType] && gpWmtFuncOps[drvType]->func_on) {

			/* special handling for Wi-Fi */
			if (drvType == WMTDRV_TYPE_WIFI) {
				P_OSAL_OP pOp = wmt_lib_get_current_op(&gDevWmt);
				atomic_set(&g_wifi_on_off_ready, 1);

				pOp->op.opId = WMT_OPID_WLAN_PROBE;
				if (wmt_lib_put_worker_op(pOp) == MTK_WCN_BOOL_FALSE) {
					WMT_WARN_FUNC("put to activeWorker queue fail\n");
					atomic_set(&g_wifi_on_off_ready, 0);
					return -4;
				}
				return 0;
			}

			iRet = (*(gpWmtFuncOps[drvType]->func_on)) (gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());
			if (iRet != 0)
				gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;
			else
				gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_FUNC_ON;
		} else {
			WMT_WARN_FUNC("WMT-CORE: ops for type(%d) not found\n", drvType);
			iRet = -5;
		}
	} else {
		if (drvType == WMTDRV_TYPE_LPBK)
			gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_FUNC_ON;
		else if (drvType == WMTDRV_TYPE_COREDUMP)
			gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_FUNC_ON;
		iRet = 0;
	}

	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE:type(0x%x) function on failed, ret(%d)\n", drvType, iRet);
		opfunc_try_pwr_off(pWmtOp);
		return iRet;
	}

	/* send UTC time sync command after function on */
	opfunc_utc_time_sync(NULL);
	wmt_core_dump_func_state("AF FUNC ON");

	return 0;
}

static INT32 opfunc_func_off(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	UINT32 drvType = pWmtOp->au4OpData[0];

	/* Check abnormal type */
	if (drvType >= WMTDRV_TYPE_MAX) {
		WMT_ERR_FUNC("WMT-CORE: abnormal Fun(%d) in wmt_func_off\n", drvType);
		osal_assert(0);
		return -1;
	}

	/* Check abnormal state */
	if (gMtkWmtCtx.eDrvStatus[drvType] >= DRV_STS_MAX) {
		WMT_ERR_FUNC("WMT-CORE: Fun(%d) DRV_STS_[0x%x] abnormal in wmt_func_off\n",
			     drvType, gMtkWmtCtx.eDrvStatus[drvType]);
		osal_assert(0);
		return -2;
	}

	if (gMtkWmtCtx.eDrvStatus[drvType] != DRV_STS_FUNC_ON) {
		WMT_WARN_FUNC
		    ("WMT-CORE: Fun(%d) DRV_STS_[0x%x] already non-FUN_ON in wmt_func_off\n",
		     drvType, gMtkWmtCtx.eDrvStatus[drvType]);
		/* needs to check 4 subsystem's state? */
		return 0;
	} else if (WMTDRV_TYPE_WMT > drvType || WMTDRV_TYPE_ANT == drvType || WMTDRV_TYPE_GPSL5 == drvType) {
		if (gpWmtFuncOps[drvType] && gpWmtFuncOps[drvType]->func_off) {
			/* special handling for Wi-Fi */
			if (drvType == WMTDRV_TYPE_WIFI) {
				P_OSAL_OP pOp = wmt_lib_get_current_op(&gDevWmt);
				atomic_set(&g_wifi_on_off_ready, 1);

				pOp->op.opId = WMT_OPID_WLAN_REMOVE;
				if (wmt_lib_put_worker_op(pOp) == MTK_WCN_BOOL_FALSE) {
					WMT_WARN_FUNC("put to activeWorker queue fail\n");
					atomic_set(&g_wifi_on_off_ready, 0);
					return -4;
				}
				return 0;
			}
			iRet = (*(gpWmtFuncOps[drvType]->func_off)) (gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());
		} else {
			WMT_WARN_FUNC("WMT-CORE: ops for type(%d) not found\n", drvType);
			iRet = -3;
		}
	} else {
		if (drvType == WMTDRV_TYPE_LPBK)
			gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;
		else if (drvType == WMTDRV_TYPE_COREDUMP)
			gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;
		iRet = 0;
	}

	if (drvType != WMTDRV_TYPE_WMT)
		gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;

	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: type(0x%x) function off failed, ret(%d)\n", drvType, iRet);
		osal_assert(0);
		/* no matter subsystem function control fail or not, chip should be powered off
		 * when no subsystem is active
		 * return iRet;
		 */
	}

	/* check all sub-func and do power off */
	opfunc_try_pwr_off(pWmtOp);
	wmt_core_dump_func_state("AF FUNC OFF");
	return iRet;
}

static INT32 opfunc_gps_suspend_by_type(ENUM_WMTDRV_TYPE_T type, P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	P_WMT_GEN_CONF pWmtGenConf = NULL;
	MTK_WCN_BOOL suspend = (pWmtOp->au4OpData[0] != 0);
	UINT32 suspend_flag = WMT_GPS_SUSPEND;

	if (WMTDRV_TYPE_GPS != type && WMTDRV_TYPE_GPSL5 != type)
		return 0;

	if (WMTDRV_TYPE_GPSL5 == type)
		suspend_flag = WMT_GPSL5_SUSPEND;

	pWmtGenConf = wmt_conf_get_cfg();

	if (gMtkWmtCtx.eDrvStatus[type] != DRV_STS_FUNC_ON) {
		WMT_WARN_FUNC("WMT-CORE: GPS(%d) driver non-FUN_ON in opfunc_gps_suspend\n", type);
		return 0;
	}

	if (MTK_WCN_BOOL_TRUE == suspend) {
		if (osal_test_bit(suspend_flag, &gGpsFmState)) {
			WMT_WARN_FUNC("WMT-CORE: GPS(%d) already suspend\n", type);
			return 0;
		}
	} else {
		if (!osal_test_bit(suspend_flag, &gGpsFmState)) {
			WMT_WARN_FUNC("WMT-CORE: GPS(%d) already resume on\n", type);
			return 0;
		}
	}

	if (MTK_WCN_BOOL_TRUE == suspend) {
		if (gpWmtFuncOps[type] && gpWmtFuncOps[type]->func_off) {
			if (pWmtGenConf != NULL)
				pWmtGenConf->wmt_gps_suspend_ctrl = 1;
			iRet = (*(gpWmtFuncOps[type]->func_off)) (gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());
			if (pWmtGenConf != NULL)
				pWmtGenConf->wmt_gps_suspend_ctrl = 0;
		} else {
			WMT_WARN_FUNC("WMT-CORE: GPS(%d) suspend ops not found\n", type);
			iRet = -3;
		}
	} else {
		/*enable power off flag, if flag=0, power off connsys will not be executed */
		mtk_wcn_set_connsys_power_off_flag(MTK_WCN_BOOL_TRUE);
		/* check if chip power on is needed */
		if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] != DRV_STS_FUNC_ON) {
			iRet = opfunc_pwr_on(pWmtOp);
			if (iRet) {
				WMT_ERR_FUNC("WMT-CORE: func(%d) hw resume fail(%d)\n", type, iRet);
				osal_assert(0);

				/* check all sub-func and do power off */
				return -5;
			}
		}

		if (gpWmtFuncOps[type] && gpWmtFuncOps[type]->func_on) {
			if (pWmtGenConf != NULL)
				pWmtGenConf->wmt_gps_suspend_ctrl = 1;
			iRet = (*(gpWmtFuncOps[type]->func_on)) (gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());
			if (pWmtGenConf != NULL)
				pWmtGenConf->wmt_gps_suspend_ctrl = 0;
		} else {
			WMT_WARN_FUNC("WMT-CORE: GPS(%d) resume ops not found\n", type);
			iRet = -7;
		}
	}

	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: gps(%d) %s function failed, ret(%d)\n",
			type, ((pWmtOp->au4OpData[0] != 0) ? "suspend" : "resume"), iRet);
		osal_assert(0);
	}

	if (MTK_WCN_BOOL_FALSE == suspend)
		opfunc_utc_time_sync(NULL);

	return iRet;
}

static INT32 opfunc_gps_suspend(P_WMT_OP pWmtOp)
{
	if (pWmtOp->au4OpData[1] == 1)
		opfunc_gps_suspend_by_type(WMTDRV_TYPE_GPS, pWmtOp);

	if (pWmtOp->au4OpData[2] == 1)
		opfunc_gps_suspend_by_type(WMTDRV_TYPE_GPSL5, pWmtOp);

	return 0;
}

/* TODO:[ChangeFeature][George] is this OP obsoleted? */
static INT32 opfunc_reg_rw(P_WMT_OP pWmtOp)
{
	INT32 iret;

	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] != DRV_STS_FUNC_ON) {
		WMT_ERR_FUNC("reg_rw when WMT is powered off\n");
		return -1;
	}
	iret = wmt_core_reg_rw_raw(pWmtOp->au4OpData[0],
				   pWmtOp->au4OpData[1],
				   (PUINT32) pWmtOp->au4OpData[2], pWmtOp->au4OpData[3]);

	return iret;
}

static INT32 opfunc_exit(P_WMT_OP pWmtOp)
{
	/* TODO: [FixMe][George] is ok to leave this function empty??? */
	WMT_WARN_FUNC("EMPTY FUNCTION\n");
	return 0;
}

static INT32 opfunc_pwr_sv(P_WMT_OP pWmtOp)
{
	INT32 ret = -1;
	UINT32 u4_result = 0;
	UINT32 evt_len;
	UINT8 evt_buf[16] = { 0 };
	ULONG ctrlPa1 = 0;
	ULONG ctrlPa2 = 0;

	typedef INT32(*STP_PSM_CB) (const MTKSTP_PSM_ACTION_T);
	STP_PSM_CB psm_cb = NULL;

	if (pWmtOp->au4OpData[0] == SLEEP) {
		WMT_DBG_FUNC("**** Send sleep command\n");
		/* mtk_wcn_stp_set_psm_state(ACT_INACT); */
		/* (*kal_stp_flush_rx)(WMT_TASK_INDX); */
		ret =
		    wmt_core_tx((const PUINT8)(&WMT_SLEEP_CMD[0]), sizeof(WMT_SLEEP_CMD),
				&u4_result, 0);
		if (ret || (u4_result != sizeof(WMT_SLEEP_CMD))) {
			WMT_ERR_FUNC("wmt_core: SLEEP_CMD ret(%d) cmd len err(%d, %zu) ", ret,
				     u4_result, sizeof(WMT_SLEEP_CMD));
			goto pwr_sv_done;
		}

		evt_len = sizeof(WMT_SLEEP_EVT);
		ret = wmt_core_rx(evt_buf, evt_len, &u4_result);
		if (ret || (u4_result != evt_len)) {
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC
				("wmt_core: read SLEEP_EVT fail(%d) len(%d, %d), host trigger firmware assert\n",
				 ret, u4_result, evt_len);

			wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 33);
			goto pwr_sv_done;
		}

		if (osal_memcmp(evt_buf, (const PVOID)WMT_SLEEP_EVT, sizeof(WMT_SLEEP_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_SLEEP_EVT error\n");
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("rx(%d):[%2X,%2X,%2X,%2X,%2X,%2X] exp(%zu):[%2X,%2X,%2X,%2X,%2X,%2X]\n",
					u4_result, evt_buf[0], evt_buf[1], evt_buf[2], evt_buf[3], evt_buf[4],
					evt_buf[5], sizeof(WMT_SLEEP_EVT), WMT_SLEEP_EVT[0], WMT_SLEEP_EVT[1],
					WMT_SLEEP_EVT[2], WMT_SLEEP_EVT[3], WMT_SLEEP_EVT[4],
					WMT_SLEEP_EVT[5]);
			mtk_wcn_stp_dbg_dump_package();
			goto pwr_sv_done;
		}
		WMT_DBG_FUNC("Send sleep command OK!\n");
	} else if (pWmtOp->au4OpData[0] == WAKEUP) {
		if (mtk_wcn_stp_is_btif_fullset_mode()) {
			WMT_DBG_FUNC("wakeup connsys by btif");
			ret = wmt_core_ctrl(WMT_CTRL_SOC_WAKEUP_CONSYS, &ctrlPa1, &ctrlPa2);
			if (ret) {
				WMT_ERR_FUNC("wmt-core:WAKEUP_CONSYS by BTIF fail(%d)", ret);
				goto pwr_sv_done;
			}
		} else if (mtk_wcn_stp_is_sdio_mode()) {
			WMT_DBG_FUNC("**** Send wakeup command\n");
			ret =
				wmt_core_tx((const PUINT8)WMT_WAKEUP_CMD, sizeof(WMT_WAKEUP_CMD), &u4_result, 1);
			if (ret || (u4_result != sizeof(WMT_WAKEUP_CMD))) {
				wmt_core_rx_flush(WMT_TASK_INDX);
				WMT_ERR_FUNC("wmt_core: WAKEUP_CMD ret(%d) cmd len err(%d, %zu)\n",
						ret, u4_result, sizeof(WMT_WAKEUP_CMD));
				goto pwr_sv_done;
			}
		}
		evt_len = sizeof(WMT_WAKEUP_EVT);
		ret = wmt_core_rx(evt_buf, evt_len, &u4_result);
		if (ret || (u4_result != evt_len)) {
			WMT_ERR_FUNC
				("wmt_core: read WAKEUP_EVT fail(%d) len(%d, %d), host grigger firmaware assert\n",
					ret, u4_result, evt_len);

			wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 34);
			goto pwr_sv_done;
		}

		if (osal_memcmp(evt_buf, (const PVOID)WMT_WAKEUP_EVT, sizeof(WMT_WAKEUP_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_WAKEUP_EVT error\n");
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("rx(%d):[%2X,%2X,%2X,%2X,%2X,%2X] exp(%zu):[%2X,%2X,%2X,%2X,%2X,%2X]\n",
					u4_result, evt_buf[0], evt_buf[1], evt_buf[2], evt_buf[3], evt_buf[4],
					evt_buf[5], sizeof(WMT_WAKEUP_EVT), WMT_WAKEUP_EVT[0],
					WMT_WAKEUP_EVT[1], WMT_WAKEUP_EVT[2], WMT_WAKEUP_EVT[3],
					WMT_WAKEUP_EVT[4], WMT_WAKEUP_EVT[5]);
			mtk_wcn_stp_dbg_dump_package();
			goto pwr_sv_done;
		}
		WMT_DBG_FUNC("Send wakeup command OK!\n");
	} else if (pWmtOp->au4OpData[0] == HOST_AWAKE) {

		WMT_DBG_FUNC("**** Send host awake command\n");

		psm_cb = (STP_PSM_CB) pWmtOp->au4OpData[1];
		/* (*kal_stp_flush_rx)(WMT_TASK_INDX); */
		ret =
		    wmt_core_tx((const PUINT8)WMT_HOST_AWAKE_CMD, sizeof(WMT_HOST_AWAKE_CMD),
				&u4_result, 0);
		if (ret || (u4_result != sizeof(WMT_HOST_AWAKE_CMD))) {
			WMT_ERR_FUNC("wmt_core: HOST_AWAKE_CMD ret(%d) cmd len err(%d, %zu) ", ret,
				     u4_result, sizeof(WMT_HOST_AWAKE_CMD));
			goto pwr_sv_done;
		}

		evt_len = sizeof(WMT_HOST_AWAKE_EVT);
		ret = wmt_core_rx(evt_buf, evt_len, &u4_result);
		if (ret || (u4_result != evt_len)) {
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC
				("wmt_core:read HOST_AWAKE_EVT fail(%d) len(%d, %d), host trigger f/w assert\n",
				 ret, u4_result, evt_len);

			wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 35);
			goto pwr_sv_done;
		}

		if (osal_memcmp
		    (evt_buf, (const PVOID)WMT_HOST_AWAKE_EVT, sizeof(WMT_HOST_AWAKE_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_HOST_AWAKE_EVT error\n");
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("rx(%d):[%2X,%2X,%2X,%2X,%2X,%2X] exp(%zu):[%2X,%2X,%2X,%2X,%2X,%2X]\n",
					u4_result, evt_buf[0], evt_buf[1], evt_buf[2], evt_buf[3], evt_buf[4],
					evt_buf[5], sizeof(WMT_HOST_AWAKE_EVT), WMT_HOST_AWAKE_EVT[0],
					WMT_HOST_AWAKE_EVT[1], WMT_HOST_AWAKE_EVT[2], WMT_HOST_AWAKE_EVT[3],
					WMT_HOST_AWAKE_EVT[4], WMT_HOST_AWAKE_EVT[5]);
			mtk_wcn_stp_dbg_dump_package();
			/* goto pwr_sv_done; */
		} else {
			WMT_DBG_FUNC("Send host awake command OK!\n");
		}
	}
pwr_sv_done:

	if (pWmtOp->au4OpData[0] < STP_PSM_MAX_ACTION) {
		psm_cb = (STP_PSM_CB) pWmtOp->au4OpData[1];
		WMT_DBG_FUNC("Do STP-CB! %zu %p\n", pWmtOp->au4OpData[0],
			     (PVOID) pWmtOp->au4OpData[1]);
		if (psm_cb != NULL) {
			psm_cb(pWmtOp->au4OpData[0]);
		} else {
			WMT_ERR_FUNC
			    ("fatal error !!!, psm_cb = %p, god, someone must have corrupted our memory.\n",
			     psm_cb);
		}
	}

	return ret;
}

static INT32 opfunc_dsns(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	WMT_DSNS_CMD[4] = pWmtOp->au4OpData[0];
	WMT_DSNS_CMD[5] = pWmtOp->au4OpData[1];

	/* send command */
	/* iRet = (*kal_stp_tx)(WMT_DSNS_CMD, osal_sizeof(WMT_DSNS_CMD), &u4Res); */
	iRet =
	    wmt_core_tx((PUINT8) WMT_DSNS_CMD, osal_sizeof(WMT_DSNS_CMD), &u4Res,
			MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != osal_sizeof(WMT_DSNS_CMD))) {
		WMT_ERR_FUNC("WMT-CORE: DSNS_CMD iRet(%d) cmd len err(%d, %zu)\n", iRet, u4Res,
			     osal_sizeof(WMT_DSNS_CMD));
		return iRet;
	}

	evtLen = osal_sizeof(WMT_DSNS_EVT);

	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || (u4Res != evtLen)) {
		WMT_ERR_FUNC("WMT-CORE: read DSNS_EVT fail(%d) len(%d, %d)\n", iRet, u4Res, evtLen);
		mtk_wcn_stp_dbg_dump_package();
		return iRet;
	}

	if (osal_memcmp(evtBuf, WMT_DSNS_EVT, osal_sizeof(WMT_DSNS_EVT)) != 0) {
		WMT_ERR_FUNC("WMT-CORE: compare WMT_DSNS_EVT error\n");
		WMT_ERR_FUNC
		    ("WMT-CORE: rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%zu):[%02X,%02X,%02X,%02X,%02X]\n",
		     u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
		     osal_sizeof(WMT_DSNS_EVT), WMT_DSNS_EVT[0], WMT_DSNS_EVT[1], WMT_DSNS_EVT[2],
		     WMT_DSNS_EVT[3], WMT_DSNS_EVT[4]);
	} else {
		WMT_INFO_FUNC("Send WMT_DSNS_CMD command OK!\n");
	}

	return iRet;
}

#if CFG_CORE_INTERNAL_TXRX
INT32 wmt_core_lpbk_do_stp_init(void)
{
	INT32 iRet = 0;
	ULONG ctrlPa1 = 0;
	ULONG ctrlPa2 = 0;

	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_OPEN, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt open stp\n");
		return -1;
	}

	ctrlPa1 = WMT_STP_CONF_MODE;
	ctrlPa2 = MTKSTP_BTIF_MAND_MODE;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);

	ctrlPa1 = WMT_STP_CONF_EN;
	ctrlPa2 = 1;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: stp_init <1><2> fail:%d\n", iRet);
		return -2;
	}
}

INT32 wmt_core_lpbk_do_stp_deinit(void)
{
	INT32 iRet = 0;
	ULONG ctrlPa1 = 0;
	ULONG ctrlPa2 = 0;

	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_CLOSE, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt open stp\n");
		return -1;
	}

	return 0;
}
#endif


static INT32 opfunc_lpbk(P_WMT_OP pWmtOp)
{

	INT32 iRet;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	UINT32 buf_length = 0;
	PUINT32 pbuffer = NULL;
	UINT16 len_in_cmd;
	/* UINT32 offset; */
	UINT8 WMT_TEST_LPBK_CMD[] = { 0x1, 0x2, 0x0, 0x0, 0x7 };
	UINT8 WMT_TEST_LPBK_EVT[] = { 0x2, 0x2, 0x0, 0x0, 0x0 };
	/* UINT8 lpbk_buf[1024 + 5] = {0}; */
	MTK_WCN_BOOL fgFail;

	buf_length = pWmtOp->au4OpData[0];	/* packet length */
	pbuffer = (PUINT32) pWmtOp->au4OpData[1];	/* packet buffer pointer */
	WMT_DBG_FUNC("WMT-CORE: -->wmt_do_lpbk\n");
	/*check if WMTDRV_TYPE_LPBK function is already on */
	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK] != DRV_STS_FUNC_ON
	    || buf_length + osal_sizeof(WMT_TEST_LPBK_CMD) > osal_sizeof(gLpbkBuf)) {
		WMT_ERR_FUNC("WMT-CORE: abnormal LPBK in wmt_do_lpbk\n");
		osal_assert(0);
		return -2;
	}
	/*package loopback for STP */

	/* init buffer */
	osal_memset(gLpbkBuf, 0, osal_sizeof(gLpbkBuf));

	len_in_cmd = buf_length + 1;	/* add flag field */

	osal_memcpy(&WMT_TEST_LPBK_CMD[2], &len_in_cmd, 2);
	osal_memcpy(&WMT_TEST_LPBK_EVT[2], &len_in_cmd, 2);

	/* wmt cmd */
	osal_memcpy(gLpbkBuf, WMT_TEST_LPBK_CMD, osal_sizeof(WMT_TEST_LPBK_CMD));
	osal_memcpy(gLpbkBuf + osal_sizeof(WMT_TEST_LPBK_CMD), pbuffer, buf_length);

	do {
		fgFail = MTK_WCN_BOOL_TRUE;
		/*send packet through STP */
		/* iRet = (*kal_stp_tx)((PUINT8)gLpbkBuf, osal_sizeof(WMT_TEST_LPBK_CMD) +
		 * buf_length, &u4WrittenSize);
		 */
		iRet =
		    wmt_core_tx((PUINT8) gLpbkBuf, (osal_sizeof(WMT_TEST_LPBK_CMD) + buf_length),
				&u4WrittenSize, MTK_WCN_BOOL_FALSE);
		if (iRet) {
			WMT_ERR_FUNC("opfunc_lpbk wmt_core_tx failed\n");
			break;
		}
		/*receive firmware response from STP */
		iRet =
		    wmt_core_rx((PUINT8) gLpbkBuf, (osal_sizeof(WMT_TEST_LPBK_EVT) + buf_length),
				&u4ReadSize);
		if (iRet) {
			WMT_ERR_FUNC("opfunc_lpbk wmt_core_rx failed\n");
			break;
		}
		/*check if loopback response ok or not */
		if (u4ReadSize != (osal_sizeof(WMT_TEST_LPBK_CMD) + buf_length)) {
			WMT_ERR_FUNC("lpbk event read size wrong(%d, %zu)\n", u4ReadSize,
				     (osal_sizeof(WMT_TEST_LPBK_CMD) + buf_length));
			break;
		}
		if (osal_memcmp(WMT_TEST_LPBK_EVT, gLpbkBuf, osal_sizeof(WMT_TEST_LPBK_EVT))) {
			WMT_ERR_FUNC
			    ("WMT-CORE WMT_TEST_LPBK_EVT error! read len %d [%02x,%02x,%02x,%02x,%02x]\n",
			     (INT32) u4ReadSize, gLpbkBuf[0], gLpbkBuf[1], gLpbkBuf[2], gLpbkBuf[3],
			     gLpbkBuf[4]
			    );
			break;
		}
		pWmtOp->au4OpData[0] = u4ReadSize - osal_sizeof(WMT_TEST_LPBK_EVT);
		osal_memcpy((PVOID) pWmtOp->au4OpData[1],
			    gLpbkBuf + osal_sizeof(WMT_TEST_LPBK_CMD), buf_length);
		fgFail = MTK_WCN_BOOL_FALSE;
	} while (0);
	/*return result */
	/* WMT_DBG_FUNC("WMT-CORE: <--wmt_do_lpbk, fgFail = %d\n", fgFail); */
	if (fgFail == MTK_WCN_BOOL_TRUE) {
		WMT_ERR_FUNC("LPBK fail and trigger assert\n");
		wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 37);
	}
	return fgFail;

}

static INT32 opfunc_cmd_test(P_WMT_OP pWmtOp)
{

	INT32 iRet = 0;
	UINT32 cmdNo = 0;
	UINT32 cmdNoPa = 0;

	UINT8 tstCmd[64];
	UINT8 tstEvt[64];
	UINT8 tstEvtTmp[64];
	UINT32 u4Res;
	UINT32 tstCmdSz = 0;
	UINT32 tstEvtSz = 0;

	PUINT8 pRes = NULL;
	UINT32 resBufRoom = 0;
	/*test command list */
	/*1 */
	UINT8 WMT_ASSERT_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x08 };
	UINT8 WMT_ASSERT_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };
	UINT8 WMT_NOACK_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x0A };
	UINT8 WMT_NOACK_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };
	UINT8 WMT_WARNRST_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x0B };
	UINT8 WMT_WARNRST_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };
	UINT8 WMT_FWLOGTST_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x0C };
	UINT8 WMT_FWLOGTST_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };

	UINT8 WMT_EXCEPTION_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x09 };
	UINT8 WMT_EXCEPTION_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };
	/*2 */
	UINT8 WMT_COEXDBG_CMD[] = { 0x01, 0x10, 0x02, 0x00,
		0x08,
		0xAA		/*Debugging Parameter */
	};
	UINT8 WMT_COEXDBG_1_EVT[] = { 0x02, 0x10, 0x05, 0x00,
		0x00,
		0xAA, 0xAA, 0xAA, 0xAA	/*event content */
	};
	UINT8 WMT_COEXDBG_2_EVT[] = { 0x02, 0x10, 0x07, 0x00,
		0x00,
		0xAA, 0xAA, 0xAA, 0xAA, 0xBB, 0xBB	/*event content */
	};
	UINT8 WMT_COEXDBG_3_EVT[] = { 0x02, 0x10, 0x0B, 0x00,
		0x00,
		0xAA, 0xAA, 0xAA, 0xAA, 0xBB, 0xBB, 0xBB, 0xBB	/*event content */
	};
	/*test command list -end */

	cmdNo = pWmtOp->au4OpData[0];

	WMT_INFO_FUNC("Send Test command %d!\n", cmdNo);
	if (cmdNo == 0) {
		/*dead command */
		WMT_INFO_FUNC("Send Assert command !\n");
		tstCmdSz = osal_sizeof(WMT_ASSERT_CMD);
		tstEvtSz = osal_sizeof(WMT_ASSERT_EVT);
		osal_memcpy(tstCmd, WMT_ASSERT_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_ASSERT_EVT, tstEvtSz);
	} else if (cmdNo == 1) {
		/*dead command */
		WMT_INFO_FUNC("Send Exception command !\n");
		tstCmdSz = osal_sizeof(WMT_EXCEPTION_CMD);
		tstEvtSz = osal_sizeof(WMT_EXCEPTION_EVT);
		osal_memcpy(tstCmd, WMT_EXCEPTION_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_EXCEPTION_EVT, tstEvtSz);
	} else if (cmdNo == 2) {
		cmdNoPa = pWmtOp->au4OpData[1];
		pRes = (PUINT8) pWmtOp->au4OpData[2];
		resBufRoom = pWmtOp->au4OpData[3];
		if (cmdNoPa <= 0xf) {
			WMT_INFO_FUNC("Send Coexistence Debug command [0x%x]!\n", cmdNoPa);
			tstCmdSz = osal_sizeof(WMT_COEXDBG_CMD);
			osal_memcpy(tstCmd, WMT_COEXDBG_CMD, tstCmdSz);
			if (tstCmdSz > 5)
				tstCmd[5] = cmdNoPa;

			/*setup the expected event length */
			if (cmdNoPa <= 0x4) {
				tstEvtSz = osal_sizeof(WMT_COEXDBG_1_EVT);
				osal_memcpy(tstEvt, WMT_COEXDBG_1_EVT, tstEvtSz);
			} else if (cmdNoPa == 0x5) {
				tstEvtSz = osal_sizeof(WMT_COEXDBG_2_EVT);
				osal_memcpy(tstEvt, WMT_COEXDBG_2_EVT, tstEvtSz);
			} else if (cmdNoPa >= 0x6 && cmdNoPa <= 0xf) {
				tstEvtSz = osal_sizeof(WMT_COEXDBG_3_EVT);
				osal_memcpy(tstEvt, WMT_COEXDBG_3_EVT, tstEvtSz);
			} else {

			}
		} else {
			WMT_ERR_FUNC("cmdNoPa is wrong\n");
			return iRet;
		}
	} else if (cmdNo == 3) {
		/*dead command */
		WMT_INFO_FUNC("Send No Ack command !\n");
		tstCmdSz = osal_sizeof(WMT_NOACK_CMD);
		tstEvtSz = osal_sizeof(WMT_NOACK_EVT);
		osal_memcpy(tstCmd, WMT_NOACK_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_NOACK_EVT, tstEvtSz);
	} else if (cmdNo == 4) {
		/*dead command */
		WMT_INFO_FUNC("Send Warm reset command !\n");
		tstCmdSz = osal_sizeof(WMT_WARNRST_CMD);
		tstEvtSz = osal_sizeof(WMT_WARNRST_EVT);
		osal_memcpy(tstCmd, WMT_WARNRST_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_WARNRST_EVT, tstEvtSz);
	} else if (cmdNo == 5) {
		/*dead command */
		WMT_INFO_FUNC("Send f/w log test command !\n");
		tstCmdSz = osal_sizeof(WMT_FWLOGTST_CMD);
		tstEvtSz = osal_sizeof(WMT_FWLOGTST_EVT);
		osal_memcpy(tstCmd, WMT_FWLOGTST_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_FWLOGTST_EVT, tstEvtSz);
	}

	/* send command */
	/* iRet = (*kal_stp_tx)(tstCmd, tstCmdSz, &u4Res); */
	iRet = wmt_core_tx((PUINT8) tstCmd, tstCmdSz, &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != tstCmdSz)) {
		WMT_ERR_FUNC("WMT-CORE: wmt_cmd_test iRet(%d) cmd len err(%d, %d)\n", iRet, u4Res,
			     tstCmdSz);
		return -1;
	}

	if ((cmdNo == 0) || (cmdNo == 1) || cmdNo == 3) {
		WMT_INFO_FUNC("WMT-CORE: not to rx event for assert command\n");
		return 0;
	}

	iRet = wmt_core_rx(tstEvtTmp, tstEvtSz, &u4Res);

	/*Event Post Handling */
	if (cmdNo == 2) {
		WMT_INFO_FUNC("#=========================================================#\n");
		WMT_INFO_FUNC("coext debugging id = %d", cmdNoPa);
		if (tstEvtSz > 5)
			wmt_core_dump_data(&tstEvtTmp[5], "coex debugging ", tstEvtSz - 5);
		else
			WMT_ERR_FUNC("error coex debugging event\n");
		/*put response to buffer for shell to read */
		if (pRes != NULL && resBufRoom > 0) {
			pWmtOp->au4OpData[3] =
			    resBufRoom < tstEvtSz - 5 ? resBufRoom : tstEvtSz - 5;
			osal_memcpy(pRes, &tstEvtTmp[5], pWmtOp->au4OpData[3]);
		} else
			pWmtOp->au4OpData[3] = 0;
		WMT_INFO_FUNC("#=========================================================#\n");
	}

	return iRet;

}

static INT32 opfunc_hw_rst(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	ULONG ctrlPa1 = 0;
	ULONG ctrlPa2 = 0;

	wmt_core_dump_func_state("BE HW RST");
    /*-->Reset WMT  data structure*/
	/*gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT] = DRV_STS_POWER_OFF;*/
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_FM] = DRV_STS_POWER_OFF;
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPS] = DRV_STS_POWER_OFF;
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPSL5] = DRV_STS_POWER_OFF;
	/* gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI] = DRV_STS_POWER_OFF; */
	/*gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK] = DRV_STS_POWER_OFF;*/
	/* gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO1]= DRV_STS_POWER_OFF; */
	/* gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO2]= DRV_STS_POWER_OFF; */
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_STP] = DRV_STS_POWER_OFF;
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_ANT] = DRV_STS_POWER_OFF;
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP] = DRV_STS_POWER_OFF;
	/*enable power off flag, if flag=0, power off connsys will not be executed */
	mtk_wcn_set_connsys_power_off_flag(MTK_WCN_BOOL_TRUE);

	/* if wmt is poweroff, we need poweron chip first */
	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] == DRV_STS_POWER_OFF) {
		WMT_WARN_FUNC("WMT-CORE: WMT is off, need re-poweron\n");
		/* power on control */
		ctrlPa1 = 0;
		ctrlPa2 = 0;
		iRet = wmt_core_ctrl(WMT_CTRL_HW_PWR_ON, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: WMT_CTRL_HW_PWR_ON fail iRet(%d)\n", iRet);
			return -1;
		}
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_ON;
	}

	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT] == DRV_STS_FUNC_ON) {
		if (mtk_wcn_stp_is_btif_fullset_mode()) {
			ctrlPa1 = BT_PALDO;
			ctrlPa2 = PALDO_OFF;
			iRet = wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
			if (iRet)
				WMT_ERR_FUNC("WMT-CORE: wmt_ctrl_soc_paldo_ctrl failed(%d)(%lu)(%lu)\n",
						iRet, ctrlPa1, ctrlPa2);
		}
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT] = DRV_STS_POWER_OFF;
	}

	iRet = wmt_lib_wlan_lock_aquire();
	if (iRet) {
		WMT_ERR_FUNC("--->lock wlan_lock failed, iRet=%d\n", iRet);
		return iRet;
	}

	/*--> reset SDIO function/slot additionally if wifi ON*/
	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI] == DRV_STS_FUNC_ON) {
		if (mtk_wcn_stp_is_sdio_mode()) {
			ctrlPa1 = WMT_SDIO_FUNC_WIFI;
			ctrlPa2 = 0;	/* turn off Wi-Fi driver */
			iRet = wmt_core_ctrl(WMT_CTRL_SDIO_FUNC, &ctrlPa1, &ctrlPa2);
			if (iRet) {
				WMT_ERR_FUNC("WMT-CORE: turn off SDIO_WIFI func fail (%d)\n", iRet);
				/* check all sub-func and do power off */
			} else
				WMT_INFO_FUNC("wmt core: turn off SDIO WIFI func ok!!\n");
			/* Anyway, turn off Wi-Fi Function */
		} else if (mtk_wcn_stp_is_btif_fullset_mode()) {
			if (gpWmtFuncOps[WMTDRV_TYPE_WIFI] != NULL &&
				gpWmtFuncOps[WMTDRV_TYPE_WIFI]->func_off != NULL) {
				iRet = gpWmtFuncOps[WMTDRV_TYPE_WIFI]->func_off(gMtkWmtCtx.p_ic_ops,
							wmt_conf_get_cfg());
				if (iRet) {
					WMT_ERR_FUNC("WMT-CORE: turn off WIFI func fail (%d)\n", iRet);

				/* check all sub-func and do power off */
				} else {
					WMT_INFO_FUNC("wmt core: turn off  WIFI func ok!!\n");
				}
			}
		}
		/* Anyway, turn off Wi-Fi Function */
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI] = DRV_STS_POWER_OFF;

		if (gMtkWmtCtx.wmtHifConf.hifType == WMT_HIF_UART) {
			ctrlPa1 = WMT_SDIO_SLOT_SDIO1;
			ctrlPa2 = 0;	/* turn off SDIO1 slot */
			iRet = wmt_core_ctrl(WMT_CTRL_SDIO_HW, &ctrlPa1, &ctrlPa2);
			if (iRet) {
				WMT_ERR_FUNC("WMT-CORE: turn off SLOT_SDIO1 fail (%d)\n", iRet);
				osal_assert(0);

				/* check all sub-func and do power off */
			} else
				WMT_INFO_FUNC("WMT-CORE: turn off SLOT_SDIO1 successfully (%d)\n", iRet);
			gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO1] = DRV_STS_POWER_OFF;
		}
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI] = DRV_STS_POWER_OFF;
	}
	wmt_lib_wlan_lock_release();

	if (gMtkWmtCtx.wmtHifConf.hifType == WMT_HIF_SDIO) {
		ctrlPa1 = WMT_SDIO_FUNC_STP;
		ctrlPa2 = 0;	/* turn off STP driver */
		iRet = wmt_core_ctrl(WMT_CTRL_SDIO_FUNC, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: turn off SDIO_FUNC_STP func fail (%d)\n", iRet);

			/* check all sub-func and do power off */
			/* goto stp_deinit_done; */
		} else
			WMT_INFO_FUNC("WMT-CORE: turn off SDIO_FUNC_STP func successfully (%d)\n", iRet);

		ctrlPa1 = WMT_SDIO_SLOT_SDIO2;
		ctrlPa2 = 0;	/* turn off SDIO2 slot */
		iRet = wmt_core_ctrl(WMT_CTRL_SDIO_HW, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: turn off SLOT_SDIO2 fail (%d)\n", iRet);
			osal_assert(0);

			/* check all sub-func and do power off */
			/* goto stp_deinit_done; */
		} else
			WMT_INFO_FUNC("WMT-CORE: turn off SLOT_SDIO2 successfully (%d)\n", iRet);
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO2] = DRV_STS_POWER_OFF;
	}
#if 0
	/*<4>Power off Combo chip */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_HW_RST, &ctrlPa1, &ctrlPa2);
	if (iRet)
		WMT_ERR_FUNC("WMT-CORE: [HW RST] WMT_CTRL_POWER_OFF fail (%d)", iRet);
	else
		WMT_INFO_FUNC("WMT-CORE: [HW RST] WMT_CTRL_POWER_OFF ok (%d)", iRet);
#endif
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_OFF;

    /*-->PesetCombo chip*/
	iRet = wmt_core_ctrl(WMT_CTRL_HW_RST, &ctrlPa1, &ctrlPa2);
	if (iRet)
		WMT_ERR_FUNC("WMT-CORE: -->[HW RST] fail iRet(%d)\n", iRet);
	else
		WMT_INFO_FUNC("WMT-CORE: -->[HW RST] ok\n");

	/* 4  close stp */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_CLOSE, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt close stp failed\n");
		return -1;
	}

	wmt_core_dump_func_state("AF HW RST");
	return iRet;
}

static INT32 opfunc_sw_rst(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;

	iRet = wmt_core_stp_init();
	if (iRet == 0) {
		WMT_INFO_FUNC("WMT-CORE: SW Rst succeed\n");
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_FUNC_ON;
	} else {
		WMT_ERR_FUNC("WMT-CORE: SW Rst failed\n");
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_OFF;
	}

	return iRet;
}

static INT32 opfunc_stp_rst(P_WMT_OP pWmtOp)
{

	return 0;
}

static INT32 opfunc_therm_ctrl(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	WMT_THERM_CMD[4] = pWmtOp->au4OpData[0];	/*CMD type, refer to ENUM_WMTTHERM_TYPE_T */

	/* send command */
	/* iRet = (*kal_stp_tx)(WMT_THERM_CMD, osal_sizeof(WMT_THERM_CMD), &u4Res); */
	iRet =
	    wmt_core_tx((PUINT8) WMT_THERM_CMD, osal_sizeof(WMT_THERM_CMD), &u4Res,
			MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != osal_sizeof(WMT_THERM_CMD))) {
		WMT_ERR_FUNC("WMT-CORE: THERM_CTRL_CMD iRet(%d) cmd len err(%d, %zu)\n", iRet,
			     u4Res, osal_sizeof(WMT_THERM_CMD));
		return iRet;
	}

	evtLen = 16;

	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || ((u4Res != osal_sizeof(WMT_THERM_CTRL_EVT)) && (u4Res != osal_sizeof(WMT_THERM_READ_EVT)))) {
		WMT_ERR_FUNC("WMT-CORE: read THERM_CTRL_EVT/THERM_READ_EVENT fail(%d) len(%d)\n", iRet, u4Res);
		wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 36);
		return iRet;
	}
	if (u4Res == osal_sizeof(WMT_THERM_CTRL_EVT)) {
		if (osal_memcmp(evtBuf, WMT_THERM_CTRL_EVT, osal_sizeof(WMT_THERM_CTRL_EVT)) != 0) {
			WMT_ERR_FUNC("WMT-CORE: compare WMT_THERM_CTRL_EVT error\n");
			WMT_ERR_FUNC
			    ("WMT-CORE: rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%zu):[%02X,%02X,%02X,%02X,%02X]\n",
			     u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
			     osal_sizeof(WMT_THERM_CTRL_EVT), WMT_THERM_CTRL_EVT[0],
			     WMT_THERM_CTRL_EVT[1], WMT_THERM_CTRL_EVT[2], WMT_THERM_CTRL_EVT[3],
			     WMT_THERM_CTRL_EVT[4]);
			pWmtOp->au4OpData[1] = MTK_WCN_BOOL_FALSE;	/*will return to function driver */
			mtk_wcn_stp_dbg_dump_package();
		} else {
			WMT_DBG_FUNC("Send WMT_THERM_CTRL_CMD command OK!\n");
			pWmtOp->au4OpData[1] = MTK_WCN_BOOL_TRUE;	/*will return to function driver */
		}
	} else {
		/*no need to judge the real thermal value */
		if (osal_memcmp(evtBuf, WMT_THERM_READ_EVT, osal_sizeof(WMT_THERM_READ_EVT) - 1) !=
		    0) {
			WMT_ERR_FUNC("WMT-CORE: compare WMT_THERM_READ_EVT error\n");
			WMT_ERR_FUNC
			    ("WMT-CORE: rx(%d):[%02X,%02X,%02X,%02X,%02X,%02X] exp(%zu):[%02X,%02X,%02X,%02X]\n",
			     u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
			     evtBuf[5], osal_sizeof(WMT_THERM_READ_EVT), WMT_THERM_READ_EVT[0],
			     WMT_THERM_READ_EVT[1], WMT_THERM_READ_EVT[2], WMT_THERM_READ_EVT[3]);
			pWmtOp->au4OpData[1] = 0xFF;	/*will return to function driver */
			mtk_wcn_stp_dbg_dump_package();
		} else {
			WMT_DBG_FUNC("Send WMT_THERM_READ_CMD command OK!\n");
			pWmtOp->au4OpData[1] = evtBuf[5];	/*will return to function driver */
		}
	}

	return iRet;

}

static INT32 opfunc_efuse_rw(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] != DRV_STS_FUNC_ON) {
		WMT_ERR_FUNC("WMT-CORE: wmt_efuse_rw fail: chip is powered off\n");
		return -1;
	}

	WMT_EFUSE_CMD[4] = (pWmtOp->au4OpData[0]) ? 0x1 : 0x2;	/* w:2, r:1 */
	osal_memcpy(&WMT_EFUSE_CMD[6], (PUINT8) &pWmtOp->au4OpData[1], 2);	/* address */
	osal_memcpy(&WMT_EFUSE_CMD[8], (PUINT32) pWmtOp->au4OpData[2], 4);	/* value */

	wmt_core_dump_data(&WMT_EFUSE_CMD[0], "efuse_cmd", osal_sizeof(WMT_EFUSE_CMD));

	/* send command */
	/* iRet = (*kal_stp_tx)(WMT_EFUSE_CMD, osal_sizeof(WMT_EFUSE_CMD), &u4Res); */
	iRet =
	    wmt_core_tx((PUINT8) WMT_EFUSE_CMD, osal_sizeof(WMT_EFUSE_CMD), &u4Res,
			MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != osal_sizeof(WMT_EFUSE_CMD))) {
		WMT_ERR_FUNC("WMT-CORE: EFUSE_CMD iRet(%d) cmd len err(%d, %zu)\n", iRet, u4Res,
			     osal_sizeof(WMT_EFUSE_CMD));
		return iRet;
	}

	evtLen = osal_sizeof(WMT_EFUSE_EVT);
	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || (u4Res != evtLen)) {
		WMT_ERR_FUNC("WMT-CORE: read REG_EVB fail(%d) len(%d, %d)\n", iRet, u4Res, evtLen);
		WMT_INFO_FUNC("buf:[%2X,%2X,%2X,%2X,%2X] evt:[%2X,%2X,%2X,%2X,%2X]\n",
				evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
				WMT_EFUSE_EVT[0], WMT_EFUSE_EVT[1], WMT_EFUSE_EVT[2],
				WMT_EFUSE_EVT[3], WMT_EFUSE_EVT[4]);
	}
	wmt_core_dump_data(&evtBuf[0], "efuse_evt", osal_sizeof(evtBuf));

	return iRet;
}

static INT32 opfunc_gpio_ctrl(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	WMT_IC_PIN_ID id;
	WMT_IC_PIN_STATE stat;
	UINT32 flag;

	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] != DRV_STS_FUNC_ON) {
		WMT_ERR_FUNC("WMT-CORE: wmt_gpio_ctrl fail: chip is powered off\n");
		return -1;
	}

	if (!gMtkWmtCtx.p_ic_ops->ic_pin_ctrl) {
		WMT_ERR_FUNC("WMT-CORE: error, gMtkWmtCtx.p_ic_ops->ic_pin_ctrl(NULL)\n");
		return -1;
	}

	id = pWmtOp->au4OpData[0];
	stat = pWmtOp->au4OpData[1];
	flag = pWmtOp->au4OpData[2];

	WMT_INFO_FUNC("ic pin id:%d, stat:%d, flag:0x%x\n", id, stat, flag);

	iRet = (*(gMtkWmtCtx.p_ic_ops->ic_pin_ctrl)) (id, stat, flag);

	return iRet;
}

/* turn on/off sdio function */
INT32 opfunc_sdio_ctrl(P_WMT_OP pWmtOp)
{
	ULONG ctrlPa1 = 0;
	ULONG ctrlPa2 = 0;
	UINT32 iRet = 0;

	ctrlPa1 =
	    WMT_HIF_SDIO ==
	    gMtkWmtCtx.wmtHifConf.hifType ? WMT_SDIO_SLOT_SDIO2 : WMT_SDIO_SLOT_SDIO1;
	ctrlPa2 = pWmtOp->au4OpData[0];	/* turn off/on SDIO slot */
	iRet = wmt_core_ctrl(WMT_CTRL_SDIO_HW, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_WARN_FUNC("SDIO hw ctrl fail ret(%d)\n", iRet);
		/* Anyway, continue turning STP SDIO to POWER OFF/ON state */
		gMtkWmtCtx.eDrvStatus[ctrlPa1] = DRV_STS_POWER_OFF;
	} else {
		WMT_INFO_FUNC("SDIO hw ctrl succeed\n");
		gMtkWmtCtx.eDrvStatus[ctrlPa1] =
		    ctrlPa2 == 0 ? DRV_STS_POWER_OFF : DRV_STS_POWER_ON;
	}

	return 0;

}

INT32 opfunc_trigger_stp_assert(P_WMT_OP pWmtOp)
{
	if (wmt_core_trigger_stp_assert() == MTK_WCN_BOOL_TRUE) {
		WMT_INFO_FUNC("trigger STP assert succeed\n");
		return 0;
	}
	WMT_WARN_FUNC("trigger STP assert failed\n");
	return -1;
}

MTK_WCN_BOOL wmt_core_is_quick_ps_support(VOID)
{
	P_WMT_CTX pctx = &gMtkWmtCtx;

	if ((pctx->p_ic_ops != NULL) && (pctx->p_ic_ops->is_quick_sleep != NULL))
		return (*(pctx->p_ic_ops->is_quick_sleep))();
	return MTK_WCN_BOOL_FALSE;
}

MTK_WCN_BOOL wmt_core_get_aee_dump_flag(VOID)
{
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_WMT_CTX pctx = &gMtkWmtCtx;

	if ((pctx->p_ic_ops != NULL) && (pctx->p_ic_ops->is_aee_dump_support != NULL))
		bRet = (*(pctx->p_ic_ops->is_aee_dump_support))();
	else
		bRet = MTK_WCN_BOOL_FALSE;

	return bRet;
}

static INT32 opfunc_bgw_ds(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	UINT32 buf_len = 0;
	UINT8 *buffer = NULL;
	UINT8 evt_buffer[8] = { 0 };
	MTK_WCN_BOOL fgFail;

	UINT8 WMT_BGW_DESENSE_CMD[] = {
		0x01, 0x0e, 0x0f, 0x00,
		0x02, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00
	};
	UINT8 WMT_BGW_DESENSE_EVT[] = { 0x02, 0x0e, 0x01, 0x00, 0x00 };

	buf_len = pWmtOp->au4OpData[0];
	buffer = (PUINT8) pWmtOp->au4OpData[1];

	osal_memcpy(&WMT_BGW_DESENSE_CMD[5], buffer, buf_len);

	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		iRet =
		    wmt_core_tx(&WMT_BGW_DESENSE_CMD[0], osal_sizeof(WMT_BGW_DESENSE_CMD), &u4WrittenSize,
				MTK_WCN_BOOL_FALSE);
		if (iRet || (u4WrittenSize != osal_sizeof(WMT_BGW_DESENSE_CMD))) {
			WMT_ERR_FUNC("bgw desense tx CMD fail(%d),size(%d)\n", iRet, u4WrittenSize);
			break;
		}

		iRet = wmt_core_rx(evt_buffer, osal_sizeof(WMT_BGW_DESENSE_EVT), &u4ReadSize);
		if (iRet || (u4ReadSize != osal_sizeof(WMT_BGW_DESENSE_EVT))) {
			WMT_ERR_FUNC("bgw desense rx EVT fail(%d),size(%d)\n", iRet, u4ReadSize);
			break;
		}

		if (osal_memcmp(evt_buffer, WMT_BGW_DESENSE_EVT, osal_sizeof(WMT_BGW_DESENSE_EVT)) != 0) {
			WMT_ERR_FUNC
			    ("bgw desense WMT_BGW_DESENSE_EVT compare fail:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			     evt_buffer[0], evt_buffer[1], evt_buffer[2], evt_buffer[3], evt_buffer[4]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	return fgFail;
}

static INT32 wmt_core_gen2_set_mcu_clk(UINT32 kind)
{
	INT32 iRet = -1;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	UINT8 evt_buffer[12] = { 0 };
	MTK_WCN_BOOL fgFail;
	PUINT8 set_mcu_clk_str[] = {
		"Enable GEN2 MCU PLL",
		"SET GEN2 MCU CLK to 26M",
		"SET GEN2 MCU CLK to 37M",
		"SET GEN2 MCU CLK to 64M",
		"SET GEN2 MCU CLK to 69M",
		"SET GEN2 MCU CLK to 104M",
		"SET GEN2 MCU CLK to 118.857M",
		"SET GEN2 MCU CLK to 138.67M",
		"Disable GEN2 MCU PLL"
	};
	UINT8 WMT_SET_MCU_CLK_CMD[] = {
		0x01, 0x08, 0x10, 0x00,
		0x01, 0x01, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0xff, 0xff, 0xff, 0xff
	};
	UINT8 WMT_SET_MCU_CLK_EVT[] = { 0x02, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01 };

	UINT8 WMT_EN_MCU_CLK_CMD[] = { 0x34, 0x03, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00 };	/* enable pll clk */
	UINT8 WMT_26_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x00, 0x4d, 0x84, 0x00 };	/* set 26M */
	UINT8 WMT_37_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x1e, 0x4d, 0x84, 0x00 };	/* set 37.8M */
	UINT8 WMT_64_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x1d, 0x4d, 0x84, 0x00 };	/* set 64M */
	UINT8 WMT_69_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x1c, 0x4d, 0x84, 0x00 };	/* set 69M */
	UINT8 WMT_104_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x5b, 0x4d, 0x84, 0x00 };	/* set 104M */
	UINT8 WMT_108_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x5a, 0x4d, 0x84, 0x00 };	/* set 118.857M */
	UINT8 WMT_138_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x59, 0x4d, 0x84, 0x00 };	/* set 138.67M */
	UINT8 WMT_DIS_MCU_CLK_CMD[] = { 0x34, 0x03, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00 };	/* disable pll clk */

	WMT_INFO_FUNC("do %s\n", set_mcu_clk_str[kind]);

	switch (kind) {
	case 0:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_EN_MCU_CLK_CMD[0], osal_sizeof(WMT_EN_MCU_CLK_CMD));
		break;
	case 1:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_26_MCU_CLK_CMD[0], osal_sizeof(WMT_26_MCU_CLK_CMD));
		break;
	case 2:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_37_MCU_CLK_CMD[0], osal_sizeof(WMT_37_MCU_CLK_CMD));
		break;
	case 3:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_64_MCU_CLK_CMD[0], osal_sizeof(WMT_64_MCU_CLK_CMD));
		break;
	case 4:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_69_MCU_CLK_CMD[0], osal_sizeof(WMT_69_MCU_CLK_CMD));
		break;
	case 5:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_104_MCU_CLK_CMD[0], osal_sizeof(WMT_104_MCU_CLK_CMD));
		break;
	case 6:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_108_MCU_CLK_CMD[0], osal_sizeof(WMT_108_MCU_CLK_CMD));
		break;
	case 7:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_138_MCU_CLK_CMD[0], osal_sizeof(WMT_138_MCU_CLK_CMD));
		break;
	case 8:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_DIS_MCU_CLK_CMD[0], osal_sizeof(WMT_DIS_MCU_CLK_CMD));
		break;
	default:
		WMT_ERR_FUNC("unknown kind\n");
		break;
	}

	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		iRet =
		    wmt_core_tx(&WMT_SET_MCU_CLK_CMD[0], osal_sizeof(WMT_SET_MCU_CLK_CMD), &u4WrittenSize,
				MTK_WCN_BOOL_FALSE);
		if (iRet || (u4WrittenSize != osal_sizeof(WMT_SET_MCU_CLK_CMD))) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_CMD fail(%d),size(%d)\n", iRet, u4WrittenSize);
			break;
		}

		iRet = wmt_core_rx(evt_buffer, osal_sizeof(WMT_SET_MCU_CLK_EVT), &u4ReadSize);
		if (iRet || (u4ReadSize != osal_sizeof(WMT_SET_MCU_CLK_EVT))) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT fail(%d),size(%d)\n", iRet, u4ReadSize);
			mtk_wcn_stp_dbg_dump_package();
			break;
		}

		if (osal_memcmp(evt_buffer, WMT_SET_MCU_CLK_EVT, osal_sizeof(WMT_SET_MCU_CLK_EVT)) != 0) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT compare fail, [0-3]:0x%02x,0x%02x,0x%02x,0x%02x\n",
				     evt_buffer[0], evt_buffer[1], evt_buffer[2], evt_buffer[3]);
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT compare fail, [4-7]:0x%02x,0x%02x,0x%02x,0x%02x\n",
				     evt_buffer[4], evt_buffer[5], evt_buffer[6], evt_buffer[7]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	if (fgFail == MTK_WCN_BOOL_FALSE)
		WMT_INFO_FUNC("wmt-core:%s: ok!\n", set_mcu_clk_str[kind]);
	else
		WMT_INFO_FUNC("wmt-core:%s: fail!\n", set_mcu_clk_str[kind]);

	return fgFail;
}

static INT32 wmt_core_gen3_set_mcu_clk(UINT32 kind)
{
	INT32 iRet = -1;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	UINT8 evt_buffer[12] = { 0 };
	MTK_WCN_BOOL fgFail;
	PUINT8 set_mcu_clk_str[] = {
		"SET GEN3 MCU CLK to 26M",
		"SET GEN3 MCU CLK to 46M",
		"SET GEN3 MCU CLK to 97M",
		"SET GEN3 MCU CLK to 104M",
		"SET GEN3 MCU CLK to 184M",
		"SET GEN3 MCU CLK to 208M",
	};
	UINT8 set_mcu_clk_vel[] = {
		0x1a,	/* set 26M*/
		0x2e,	/* set 46M*/
		0x61,	/* set 97M*/
		0x68,	/* set 104M */
		0xb8,	/* set 184M */
		0xd0,	/* set 208M */
	};
	UINT8 WMT_SET_MCU_CLK_CMD[] = { 0x01, 0x0a, 0x04, 0x00, 0x09, 0x03, 0x00, 0x00 };
	UINT8 WMT_SET_MCU_CLK_EVT[] = { 0x02, 0x0a, 0x01, 0x00, 0x00 };


	if (kind < osal_sizeof(set_mcu_clk_vel)) {
		WMT_INFO_FUNC("do %s\n", set_mcu_clk_str[kind]);
		WMT_SET_MCU_CLK_CMD[6] = set_mcu_clk_vel[kind];
	} else {
		WMT_ERR_FUNC("unknown kind(%d)!\n", kind);
		return MTK_WCN_BOOL_TRUE;
	}

	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		iRet = wmt_core_tx(&WMT_SET_MCU_CLK_CMD[0], osal_sizeof(WMT_SET_MCU_CLK_CMD), &u4WrittenSize,
				MTK_WCN_BOOL_FALSE);
		if (iRet || (u4WrittenSize != osal_sizeof(WMT_SET_MCU_CLK_CMD))) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_CMD fail(%d),size(%d)\n", iRet, u4WrittenSize);
			break;
		}

		iRet = wmt_core_rx(evt_buffer, osal_sizeof(WMT_SET_MCU_CLK_EVT), &u4ReadSize);
		if (iRet || (u4ReadSize != osal_sizeof(WMT_SET_MCU_CLK_EVT))) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT fail(%d),size(%d)\n", iRet, u4ReadSize);
			mtk_wcn_stp_dbg_dump_package();
			break;
		}

		if (osal_memcmp(evt_buffer, WMT_SET_MCU_CLK_EVT, osal_sizeof(WMT_SET_MCU_CLK_EVT)) != 0) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT compare fail, [0-3]:0x%02x,0x%02x,0x%02x,0x%02x\n",
				     evt_buffer[0], evt_buffer[1], evt_buffer[2], evt_buffer[3]);
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT compare fail, [4-7]:0x%02x,0x%02x,0x%02x,0x%02x\n",
				     evt_buffer[4], evt_buffer[5], evt_buffer[6], evt_buffer[7]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	if (fgFail == MTK_WCN_BOOL_FALSE)
		WMT_INFO_FUNC("wmt-core:%s: ok!\n", set_mcu_clk_str[kind]);
	else
		WMT_INFO_FUNC("wmt-core:%s: fail!\n", set_mcu_clk_str[kind]);

	return fgFail;
}

static INT32 wmt_core_set_mcu_clk(UINT32 kind)
{
	INT32 iRet = -1;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	UINT8 evt_buffer[12] = { 0 };
	MTK_WCN_BOOL fgFail;

	UINT8 WMT_SET_MCU_CLK_CMD[] = { 0x01, 0x0a, 0x04, 0x00, 0x09, 0x01, 0x00, 0x00 };
	UINT8 WMT_SET_MCU_CLK_EVT[] = { 0x02, 0x0a, 0x01, 0x00, 0x00 };


	WMT_INFO_FUNC("clock frequency 0x%x\n", kind);
	WMT_SET_MCU_CLK_CMD[6] = (kind & 0xff);

	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		iRet = wmt_core_tx(&WMT_SET_MCU_CLK_CMD[0], osal_sizeof(WMT_SET_MCU_CLK_CMD), &u4WrittenSize,
				   MTK_WCN_BOOL_FALSE);
		if (iRet || (u4WrittenSize != osal_sizeof(WMT_SET_MCU_CLK_CMD))) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_CMD fail(%d),size(%d)\n", iRet, u4WrittenSize);
			break;
		}

		iRet = wmt_core_rx(evt_buffer, osal_sizeof(WMT_SET_MCU_CLK_EVT), &u4ReadSize);
		if (iRet || (u4ReadSize != osal_sizeof(WMT_SET_MCU_CLK_EVT))) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT fail(%d),size(%d)\n", iRet, u4ReadSize);
			mtk_wcn_stp_dbg_dump_package();
			break;
		}

		if (osal_memcmp(evt_buffer, WMT_SET_MCU_CLK_EVT, osal_sizeof(WMT_SET_MCU_CLK_EVT)) != 0) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT compare fail, [0-3]:0x%02x,0x%02x,0x%02x,0x%02x\n",
				     evt_buffer[0], evt_buffer[1], evt_buffer[2], evt_buffer[3]);
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT compare fail, [4-7]:0x%02x,0x%02x,0x%02x,0x%02x\n",
				     evt_buffer[4], evt_buffer[5], evt_buffer[6], evt_buffer[7]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	if (fgFail == MTK_WCN_BOOL_FALSE)
		WMT_INFO_FUNC("wmt-core:set mcu clock ok!\n");
	else
		WMT_INFO_FUNC("wmt-core:set mcu clock fail!\n");

	return fgFail;
}

static INT32 opfunc_set_mcu_clk(P_WMT_OP pWmtOp)
{
	UINT32 kind = 0;
	UINT32 version = 0;
	MTK_WCN_BOOL ret;

	kind = pWmtOp->au4OpData[0];
	version = pWmtOp->au4OpData[1];

	switch (version) {
	case 0:
		ret = wmt_core_gen2_set_mcu_clk(kind);
		break;
	case 1:
		ret = wmt_core_gen3_set_mcu_clk(kind);
		break;
	case 2:
		ret = wmt_core_set_mcu_clk(kind);
		break;
	default:
		WMT_ERR_FUNC("wmt-core: version(%d) is not support!\n", version);
		ret = MTK_WCN_BOOL_TRUE;
	}

	return ret;
}

static INT32 opfunc_adie_lpbk_test(P_WMT_OP pWmtOp)
{
	UINT8 *buffer = NULL;
	MTK_WCN_BOOL fgFail;
	UINT32 u4Res;
	UINT32 aDieChipid = 0;
	UINT8 soc_adie_chipid_cmd[] = { 0x01, 0x13, 0x04, 0x00, 0x02, 0x04, 0x24, 0x00 };
	UINT8 soc_adie_chipid_evt[] = { 0x02, 0x13, 0x09, 0x00, 0x00, 0x02, 0x04, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00 };
	UINT8 evtbuf[20];
	INT32 iRet = -1;

	buffer = (PUINT8) pWmtOp->au4OpData[1];

	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		/* read A die chipid by wmt cmd */
		iRet =
		    wmt_core_tx((PUINT8) &soc_adie_chipid_cmd[0], osal_sizeof(soc_adie_chipid_cmd), &u4Res,
				MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != osal_sizeof(soc_adie_chipid_cmd))) {
			WMT_ERR_FUNC("wmt_core:read A die chipid CMD fail(%d),size(%d)\n", iRet, u4Res);
			break;
		}
		osal_memset(evtbuf, 0, osal_sizeof(evtbuf));
		iRet = wmt_core_rx(evtbuf, osal_sizeof(soc_adie_chipid_evt), &u4Res);
		if (iRet || (u4Res != osal_sizeof(soc_adie_chipid_evt))) {
			WMT_ERR_FUNC("wmt_core:read A die chipid EVT fail(%d),size(%d)\n", iRet, u4Res);
			break;
		}
		osal_memcpy(&aDieChipid, &evtbuf[u4Res - 2], 2);
		osal_memcpy(buffer, &evtbuf[u4Res - 2], 2);
		pWmtOp->au4OpData[0] = 2;
		WMT_INFO_FUNC("get SOC A die chipid(0x%x)\n", aDieChipid);

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	return fgFail;
}

MTK_WCN_BOOL wmt_core_trigger_stp_assert(VOID)
{
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_WMT_CTX pctx = &gMtkWmtCtx;

	if (mtk_wcn_stp_coredump_flag_get() == 0) {
		WMT_INFO_FUNC("coredump is disabled, omit trigger STP assert\n");
		wmt_lib_trigger_reset();
		return MTK_WCN_BOOL_FALSE;
	}

	if ((pctx->p_ic_ops != NULL) && (pctx->p_ic_ops->trigger_stp_assert != NULL)) {
		WMT_INFO_FUNC("trigger stp assert function is supported by  0x%X\n",
			      pctx->p_ic_ops->icId);
		bRet = (*(pctx->p_ic_ops->trigger_stp_assert)) ();
	} else {
		if (pctx->p_ic_ops != NULL)
			WMT_INFO_FUNC("trigger stp assert function is not supported by  0x%X\n",
				      pctx->p_ic_ops->icId);
		bRet = MTK_WCN_BOOL_FALSE;
	}

	return bRet;
}
#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
MTK_WCN_BOOL wmt_core_deep_sleep_ctrl(INT32 value)
{
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_WMT_CTX pctx = &gMtkWmtCtx;

	if ((pctx->p_ic_ops != NULL) && (pctx->p_ic_ops->deep_sleep_ctrl != NULL)) {
		bRet = (*(pctx->p_ic_ops->deep_sleep_ctrl)) (value);
	} else {
		if (pctx->p_ic_ops != NULL)
			WMT_INFO_FUNC("deep sleep function is not supported by 0x%x\n",
				pctx->p_ic_ops->icId);
		bRet = MTK_WCN_BOOL_FALSE;
	}
	return bRet;
}
#endif
INT32 opfunc_pin_state(P_WMT_OP pWmtOp)
{
	ULONG ctrlPa1 = 0;
	ULONG ctrlPa2 = 0;
	INT32 iRet = 0;

	iRet = wmt_core_ctrl(WMT_CTRL_HW_STATE_DUMP, &ctrlPa1, &ctrlPa2);
	return iRet;
}

#ifdef CONFIG_MTK_COMBO_ANT
INT32 opfunc_ant_ram_down(P_WMT_OP pWmtOp)
{
	INT32 iRet = 0;
	size_t ctrlPa1 = pWmtOp->au4OpData[0];
	UINT32 ctrlPa2 = pWmtOp->au4OpData[1];
	PUINT8 pbuf = (PUINT8) ctrlPa1;
	UINT32 fragSeq = 0;
	UINT16 fragSize = 0;
	UINT16 wmtCmdLen;
	UINT16 wmtPktLen;
	UINT32 u4Res = 0;
	UINT8 antEvtBuf[osal_sizeof(WMT_ANT_RAM_DWN_EVT)];
#if 1
	UINT32 ctrlPa3 = pWmtOp->au4OpData[2];

	do {
		fragSize = ctrlPa2;
		fragSeq = ctrlPa3;
		gAntBuf[5] = fragSeq;


		wmtPktLen = fragSize + sizeof(WMT_ANT_RAM_DWN_CMD) + 1;

		/*WMT command length cal */
		wmtCmdLen = wmtPktLen - 4;
#if 0
		WMT_ANT_RAM_DWN_CMD[2] = wmtCmdLen & 0xFF;
		WMT_ANT_RAM_DWN_CMD[3] = (wmtCmdLen & 0xFF00) >> 16;
#else
		osal_memcpy(&WMT_ANT_RAM_DWN_CMD[2], &wmtCmdLen, 2);
#endif



		WMT_ANT_RAM_DWN_CMD[4] = 1;	/*RAM CODE download */

		osal_memcpy(gAntBuf, WMT_ANT_RAM_DWN_CMD, sizeof(WMT_ANT_RAM_DWN_CMD));

		/*copy ram code content to global buffer */
		osal_memcpy(&gAntBuf[osal_sizeof(WMT_ANT_RAM_DWN_CMD) + 1], pbuf, fragSize);

		iRet = wmt_core_tx(gAntBuf, wmtPktLen, &u4Res, MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != wmtPktLen)) {
			WMT_ERR_FUNC("wmt_core: write fragSeq(%d) size(%d, %d) fail(%d)\n", fragSeq,
				     wmtPktLen, u4Res, iRet);
			iRet = -4;
			break;
		}
		WMT_DBG_FUNC("wmt_core: write fragSeq(%d) size(%d, %d) ok\n",
			     fragSeq, wmtPktLen, u4Res);

		osal_memset(antEvtBuf, 0, sizeof(antEvtBuf));

		WMT_ANT_RAM_DWN_EVT[4] = 0;	/*download result; 0 */

		iRet = wmt_core_rx(antEvtBuf, sizeof(WMT_ANT_RAM_DWN_EVT), &u4Res);
		if (iRet || (u4Res != sizeof(WMT_ANT_RAM_DWN_EVT))) {
			WMT_ERR_FUNC("wmt_core: read WMT_ANT_RAM_DWN_EVT length(%zu, %d) fail(%d)\n",
				     sizeof(WMT_ANT_RAM_DWN_EVT), u4Res, iRet);
			iRet = -5;
			break;
		}
#if CFG_CHECK_WMT_RESULT
		if (osal_memcmp(antEvtBuf, WMT_ANT_RAM_DWN_EVT, sizeof(WMT_ANT_RAM_DWN_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_ANT_RAM_DWN_EVT result error\n");
			WMT_ERR_FUNC("rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%zu):[%02X,%02X,%02X,%02X,%02X]\n",
					u4Res, antEvtBuf[0], antEvtBuf[1], antEvtBuf[2], antEvtBuf[3],
					antEvtBuf[4], sizeof(WMT_ANT_RAM_DWN_EVT), WMT_ANT_RAM_DWN_EVT[0],
					WMT_ANT_RAM_DWN_EVT[1], WMT_ANT_RAM_DWN_EVT[2], WMT_ANT_RAM_DWN_EVT[3],
					WMT_ANT_RAM_DWN_EVT[4]);
			iRet = -6;
			break;
		}
#endif
		WMT_DBG_FUNC("wmt_core: read WMT_ANT_RAM_DWN_EVT length(%zu, %d) ok\n",
			     sizeof(WMT_ANT_RAM_DWN_EVT), u4Res);

	} while (0);
#else
	UINT32 patchSize = ctrlPa2;
	UINT32 patchSizePerFrag = 1000;
	UINT32 offset;
	UINT32 fragNum = 0;
	/*cal patch fragNum */
	fragNum = (patchSize + patchSizePerFrag - 1) / patchSizePerFrag;
	if (fragNum <= 2) {
		WMT_WARN_FUNC("ANT ramcode size(%d) too short\n", patchSize);
		return -1;
	}

	while (fragSeq < fragNum) {
		/*update fragNum */
		fragSeq++;

		if (fragSeq == 1) {
			fragSize = patchSizePerFrag;
			/*first package */
			gAntBuf[5] = 1;	/*RAM CODE start */
		} else if (fragNum == fragSeq) {
			/*last package */
			fragSize = patchSizePerFrag;
			gAntBuf[5] = 3;	/*RAM CODE end */
		} else {
			/*middle package */
			fragSize = patchSize - ((fragNum - 1) * patchSizePerFrag);
			gAntBuf[5] = 2;	/*RAM CODE confinue */
		}
		wmtPktLen = fragSize + sizeof(WMT_ANT_RAM_OP_CMD) + 1;

		/*WMT command length cal */
		wmtCmdLen = wmtPktLen - 4;

		WMT_ANT_RAM_OP_CMD[2] = wmtCmdLen & 0xFF;
		WMT_ANT_RAM_OP_CMD[3] = (wmtCmdLen & 0xFF00) >> 16;

		WMT_ANT_RAM_OP_CMD[4] = 1;	/*RAM CODE download */

		osal_memcpy(gAntBuf, WMT_ANT_RAM_OP_CMD, sizeof(WMT_ANT_RAM_OP_CMD));

		/*copy ram code content to global buffer */
		osal_memcpy(&gAntBuf[6], pbuf, fragSize);

		/*update offset */
		offset += fragSize;
		pbuf += offset;

		iRet = wmt_core_tx(gAntBuf, wmtPktLen, &u4Res, MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != wmtPktLen)) {
			WMT_ERR_FUNC("wmt_core: write fragSeq(%d) size(%d, %d) fail(%d)\n", fragSeq,
				     wmtPktLen, u4Res, iRet);
			iRet = -4;
			break;
		}
		WMT_DBG_FUNC("wmt_core: write fragSeq(%d) size(%d, %d) ok\n",
			     fragSeq, wmtPktLen, u4Res);

		osal_memset(antEvtBuf, 0, sizeof(antEvtBuf));

		WMT_SET_RAM_OP_EVT[4] = 0;	/*download result; 0 */

		iRet = wmt_core_rx(antEvtBuf, sizeof(WMT_SET_RAM_OP_EVT), &u4Res);
		if (iRet || (u4Res != sizeof(WMT_SET_RAM_OP_EVT))) {
			WMT_ERR_FUNC("wmt_core: read WMT_SET_RAM_OP_EVT length(%d, %d) fail(%d)\n",
				     sizeof(WMT_SET_RAM_OP_EVT), u4Res, iRet);
			iRet = -5;
			break;
		}
#if CFG_CHECK_WMT_RESULT
		if (osal_memcmp(antEvtBuf, WMT_SET_RAM_OP_EVT, sizeof(WMT_SET_RAM_OP_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_SET_RAM_OP_EVT result error\n");
			WMT_ERR_FUNC("rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%d):[%02X,%02X,%02X,%02X,%02X]\n",
			     u4Res, antEvtBuf[0], antEvtBuf[1], antEvtBuf[2], antEvtBuf[3],
			     antEvtBuf[4], sizeof(WMT_SET_RAM_OP_EVT), WMT_SET_RAM_OP_EVT[0],
			     WMT_SET_RAM_OP_EVT[1], WMT_SET_RAM_OP_EVT[2], WMT_SET_RAM_OP_EVT[3],
			     WMT_SET_RAM_OP_EVT[4]);
			iRet = -6;
			break;
		}
#endif
		WMT_DBG_FUNC("wmt_core: read WMT_SET_RAM_OP_EVT length(%d, %d) ok\n",
			     sizeof(WMT_SET_RAM_OP_EVT), u4Res);


	}
	if (fragSeq != fragNum)
		iRet = -7;
#endif
	return iRet;
}


INT32 opfunc_ant_ram_stat_get(P_WMT_OP pWmtOp)
{
	INT32 iRet = 0;
	UINT32 u4Res = 0;
	UINT32 wmtPktLen = osal_sizeof(WMT_ANT_RAM_STA_GET_CMD);
	UINT32 u4AntRamStatus = 0;
	UINT8 antEvtBuf[osal_sizeof(WMT_ANT_RAM_STA_GET_EVT)];


	iRet = wmt_core_tx(WMT_ANT_RAM_STA_GET_CMD, wmtPktLen, &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != wmtPktLen)) {
		WMT_ERR_FUNC
		    ("wmt_core: write wmt and ramcode status query command failed, (%d, %d), iRet(%d)\n",
		     wmtPktLen, u4Res, iRet);
		iRet = -4;
		return iRet;
	}


	iRet = wmt_core_rx(antEvtBuf, sizeof(WMT_ANT_RAM_STA_GET_EVT), &u4Res);
	if (iRet || (u4Res != sizeof(WMT_ANT_RAM_STA_GET_EVT))) {
		WMT_ERR_FUNC("wmt_core: read WMT_ANT_RAM_STA_GET_EVT length(%zu, %d) fail(%d)\n",
			     sizeof(WMT_ANT_RAM_STA_GET_EVT), u4Res, iRet);
		iRet = -5;
		return iRet;
	}
#if CFG_CHECK_WMT_RESULT
	if (osal_memcmp(antEvtBuf, WMT_ANT_RAM_STA_GET_EVT, sizeof(WMT_ANT_RAM_STA_GET_EVT) - 1) !=
	    0) {
		WMT_ERR_FUNC("wmt_core: compare WMT_ANT_RAM_STA_GET_EVT result error\n");
		WMT_ERR_FUNC("rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%zu):[%02X,%02X,%02X,%02X,%02X]\n",
				u4Res, antEvtBuf[0], antEvtBuf[1], antEvtBuf[2], antEvtBuf[3], antEvtBuf[4],
				sizeof(WMT_ANT_RAM_STA_GET_EVT), WMT_ANT_RAM_STA_GET_EVT[0],
				WMT_ANT_RAM_STA_GET_EVT[1], WMT_ANT_RAM_STA_GET_EVT[2],
				WMT_ANT_RAM_STA_GET_EVT[3], WMT_ANT_RAM_STA_GET_EVT[4]);
		iRet = -6;
		return iRet;
	}
#endif
	if (iRet == 0) {
		u4AntRamStatus = antEvtBuf[sizeof(WMT_ANT_RAM_STA_GET_EVT) - 1];
		pWmtOp->au4OpData[2] = u4AntRamStatus;
		WMT_INFO_FUNC("ANT ram code %s\n",
			      u4AntRamStatus == 1 ? "exist already" : "not exist");
	}
	return iRet;
}
#endif
VOID wmt_core_set_coredump_state(ENUM_DRV_STS state)
{
	WMT_INFO_FUNC("wmt-core: set coredump state(%d)\n", state);
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP] = state;
}

INT32 opfunc_flash_patch_down(P_WMT_OP pWmtOp)
{
	INT32 iRet = 0;
	PUINT8 u4pbuf = (PUINT8)pWmtOp->au4OpData[0];
	UINT16 u4PatchSize = pWmtOp->au4OpData[1];
	UINT32 u4PatchSeq = pWmtOp->au4OpData[2];
	UINT32 u4PatchType = pWmtOp->au4OpData[3];
	UINT32 u4PatchVersion = pWmtOp->au4OpData[4];
	UINT32 u4PatchChecksum = pWmtOp->au4OpData[5];
	UINT16 wmtCmdLen;
	UINT16 wmtPktLen = 0;
	UINT32 u4Res = 0;
	UINT8 evtBuf[osal_sizeof(WMT_FLASH_PATCH_DWN_EVT)];
	UINT32 i = 0;

	do {
		osal_memcpy(gFlashBuf, WMT_FLASH_PATCH_DWN_CMD, sizeof(WMT_FLASH_PATCH_DWN_CMD));

		switch (u4PatchSeq) {
		case WMT_FLASH_PATCH_HEAD_PKT:
			/*WMT command length cal */
			wmtPktLen = sizeof(WMT_FLASH_PATCH_DWN_CMD) + WMT_FLASH_PATCH_DWN_CMD[2] - 1;
			osal_memcpy(&gFlashBuf[5], &u4PatchType, sizeof(u4PatchType));
			osal_memcpy(&gFlashBuf[9], &u4PatchVersion, sizeof(u4PatchVersion));
			osal_memcpy(&gFlashBuf[13], &u4PatchChecksum, sizeof(u4PatchChecksum));
			break;
		case WMT_FLASH_PATCH_START_PKT:
		case WMT_FLASH_PATCH_CONTINUE_PKT:
		case WMT_FLASH_PATCH_END_PKT:
			gFlashBuf[4] = u4PatchSeq;
			/*WMT command length cal */
			wmtCmdLen = u4PatchSize + 1;
			wmtPktLen = u4PatchSize + sizeof(WMT_FLASH_PATCH_DWN_CMD);
			gFlashBuf[2] = wmtCmdLen & 0xFF;
			gFlashBuf[3] = (wmtCmdLen & 0xFF00) >> 8;
			/*copy ram code content to global buffer */
			osal_memcpy(&gFlashBuf[osal_sizeof(WMT_FLASH_PATCH_DWN_CMD)], u4pbuf, u4PatchSize);
			break;
		default:
			break;
		}

		iRet = wmt_core_tx(gFlashBuf, wmtPktLen, &u4Res, MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != wmtPktLen)) {
			WMT_ERR_FUNC("wmt_core: write PatchSeq(%d) size(%d, %d) fail(%d)\n", u4PatchSeq,
				     wmtPktLen, u4Res, iRet);
			iRet = -4;
			u4Res = -1;
			break;
		}
		WMT_DBG_FUNC("wmt_core: write PatchSeq(%d) size(%d, %d) ok\n",
			     u4PatchSeq, wmtPktLen, u4Res);

		osal_memset(evtBuf, 0, sizeof(evtBuf));

		/* flash patch download time longer than WMT command timeout */
		for (i = 0; i < 3; i++) {
			iRet = wmt_core_rx(evtBuf, sizeof(WMT_FLASH_PATCH_DWN_EVT), &u4Res);
			if (!iRet)
				break;
		}
		if (iRet || (u4Res != sizeof(WMT_FLASH_PATCH_DWN_EVT))) {
			WMT_ERR_FUNC("wmt_core: read WMT_FLASH_PATCH_DWN_EVT length(%zu, %d) fail(%d)\n",
				     sizeof(WMT_FLASH_PATCH_DWN_EVT), u4Res, iRet);
			wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 39);

			iRet = -5;
			u4Res = -2;
			break;
		}
#if CFG_CHECK_WMT_RESULT
		if (osal_memcmp(evtBuf, WMT_FLASH_PATCH_DWN_EVT, sizeof(WMT_FLASH_PATCH_DWN_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_FLASH_PATCH_DWN_EVT result error\n");
			WMT_ERR_FUNC("rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%zu):[%02X,%02X,%02X,%02X,%02X]\n",
					u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
					sizeof(WMT_FLASH_PATCH_DWN_EVT), WMT_FLASH_PATCH_DWN_EVT[0],
					WMT_FLASH_PATCH_DWN_EVT[1], WMT_FLASH_PATCH_DWN_EVT[2],
					WMT_FLASH_PATCH_DWN_EVT[3], WMT_FLASH_PATCH_DWN_EVT[4]);
			iRet = -6;
			u4Res = -3;
			break;
		}
#endif
		u4Res = 0;
		WMT_DBG_FUNC("wmt_core: read WMT_FLASH_PATCH_DWN_EVT length(%zu, %d) ok\n",
			     sizeof(WMT_FLASH_PATCH_DWN_EVT), u4Res);

	} while (0);

	pWmtOp->au4OpData[6] = u4Res;
	return iRet;
}

INT32 opfunc_flash_patch_ver_get(P_WMT_OP pWmtOp)
{
	INT32 iRet = 0;
	UINT32 u4Res = 0;
	UINT32 wmtPktLen = osal_sizeof(WMT_FLASH_PATCH_VER_GET_CMD);
	UINT32 u4PatchType = pWmtOp->au4OpData[3];
	UINT32 u4PatchVer = 0;
	UINT8 evtBuf[osal_sizeof(WMT_FLASH_PATCH_VER_GET_EVT)];

	do {
		osal_memcpy(&WMT_FLASH_PATCH_VER_GET_CMD[5], &u4PatchType, sizeof(u4PatchType));

		iRet = wmt_core_tx(WMT_FLASH_PATCH_VER_GET_CMD, wmtPktLen, &u4Res, MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != wmtPktLen)) {
			WMT_ERR_FUNC
				("wmt_core: write wmt and flash patch query command failed, (%d, %d), iRet(%d)\n",
				 wmtPktLen, u4Res, iRet);
			iRet = -4;
			u4Res = -1;
			break;
		}

		iRet = wmt_core_rx(evtBuf, sizeof(WMT_FLASH_PATCH_VER_GET_EVT), &u4Res);
		if (iRet || (u4Res != sizeof(WMT_FLASH_PATCH_VER_GET_EVT))) {
			WMT_ERR_FUNC("wmt_core: read WMT_FLASH_PATCH_VER_GET_EVT length(%zu, %d) fail(%d)\n",
					sizeof(WMT_FLASH_PATCH_VER_GET_EVT), u4Res, iRet);
			wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 38);

			iRet = -5;
			u4Res = -2;
			break;
		}
#if CFG_CHECK_WMT_RESULT
		if (osal_memcmp(evtBuf, WMT_FLASH_PATCH_VER_GET_EVT,
					sizeof(WMT_FLASH_PATCH_VER_GET_EVT) - 8) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_FLASH_PATCH_VER_GET_EVT result error\n");
			WMT_ERR_FUNC("rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%zu):[%02X,%02X,%02X,%02X,%02X]\n",
					u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
					sizeof(WMT_FLASH_PATCH_VER_GET_EVT), WMT_FLASH_PATCH_VER_GET_EVT[0],
					WMT_FLASH_PATCH_VER_GET_EVT[1], WMT_FLASH_PATCH_VER_GET_EVT[2],
					WMT_FLASH_PATCH_VER_GET_EVT[3], WMT_FLASH_PATCH_VER_GET_EVT[4]);
			iRet = -6;
			u4Res = -3;
			break;
		}
#endif
		if (iRet == 0) {
			osal_memcpy(&u4PatchVer, &evtBuf[9], sizeof(u4PatchVer));
			pWmtOp->au4OpData[4] = u4PatchVer;
			u4Res = 0;
			WMT_INFO_FUNC("flash patch type: %x, flash patch version %x\n",
					u4PatchType, u4PatchVer);
		}
	} while (0);

	pWmtOp->au4OpData[6] = u4Res;
	return iRet;
}

#if CFG_WMT_LTE_COEX_HANDLING
static INT32 opfunc_idc_msg_handling(P_WMT_OP pWmtOp)
{
	MTK_WCN_BOOL fgFail;
	UINT32 u4Res;
	UINT8 host_lte_btwf_coex_cmd[] = { 0x01, 0x10, 0x00, 0x00, 0x00 };
	UINT8 host_lte_btwf_coex_evt[] = { 0x02, 0x10, 0x01, 0x00, 0x00 };
	UINT8 *pTxBuf = NULL;
	UINT8 evtbuf[8] = { 0 };
	INT32 iRet = -1;
	UINT16 msg_len = 0;
	UINT32 total_len = 0;
	UINT32 index = 0;
	UINT32 evtLen;

	pTxBuf = (UINT8 *) pWmtOp->au4OpData[0];
	if (pTxBuf == NULL) {
		WMT_ERR_FUNC("idc msg buffer is NULL\n");
		return -1;
	}
	iRet = wmt_lib_idc_lock_aquire();
	if (iRet) {
		WMT_ERR_FUNC("--->lock idc_lock failed, ret=%d\n", iRet);
		return iRet;
	}
	osal_memcpy(&msg_len, &pTxBuf[0], osal_sizeof(msg_len));
	if (msg_len > WMT_IDC_MSG_MAX_SIZE) {
		wmt_lib_idc_lock_release();
		WMT_ERR_FUNC("abnormal idc msg len:%d\n", msg_len);
		return -2;
	}
	msg_len += 1;	/*flag byte */
	osal_memcpy(&host_lte_btwf_coex_cmd[2], &msg_len, 2);
	host_lte_btwf_coex_cmd[4] = (pWmtOp->au4OpData[1] & 0x00ff);
	osal_memcpy(&msg_local_buffer[0], &host_lte_btwf_coex_cmd[0],
			osal_sizeof(host_lte_btwf_coex_cmd));
	osal_memcpy(&msg_local_buffer[osal_sizeof(host_lte_btwf_coex_cmd)],
			&pTxBuf[osal_sizeof(msg_len)], msg_len - 1);
	wmt_lib_idc_lock_release();
	total_len = osal_sizeof(host_lte_btwf_coex_cmd) + msg_len - 1;
	WMT_DBG_FUNC("wmt_core:idc msg payload len form lte(%d),wmt msg total len(%d)\n",
			msg_len - 1, total_len);
	WMT_DBG_FUNC("wmt_core:idc msg payload:\n");
	for (index = 0; index < total_len; index++)
		WMT_DBG_FUNC("0x%02x ", msg_local_buffer[index]);
	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		/* read A die chipid by wmt cmd */
		iRet =
		    wmt_core_tx((PUINT8) &msg_local_buffer[0], total_len, &u4Res,
				MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != total_len)) {
			WMT_ERR_FUNC("wmt_core:send lte idc msg to connsys fail(%d),size(%d)\n",
				     iRet, u4Res);
			break;
		}
		osal_memset(evtbuf, 0, osal_sizeof(evtbuf));
		evtLen = osal_sizeof(host_lte_btwf_coex_evt);
		iRet = wmt_core_rx(evtbuf, evtLen, &u4Res);
		if (iRet || (u4Res != evtLen)) {
			WMT_ERR_FUNC("wmt_core:recv host_lte_btwf_coex_evt fail(%d) len(%d, %d)\n",
				iRet, u4Res, evtLen);
			wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 41);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	return fgFail;
}

/*TEST CODE*/
VOID wmt_core_set_flag_for_test(UINT32 enable)
{
	WMT_INFO_FUNC("%s wmt_lte_flag\n", enable ? "enable" : "disable");
	g_open_wmt_lte_flag = enable;
}

UINT32 wmt_core_get_flag_for_test(VOID)
{
	return g_open_wmt_lte_flag;
}

#endif

static INT32 opfunc_utc_time_sync(P_WMT_OP pWmtOp)
{
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	INT32 iRet;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };
	UINT32 tsec;
	UINT32 tusec;

	connsys_dedicated_log_get_utc_time(&tsec, &tusec);
	/* UTC time second unit */
	osal_memcpy(&WMT_UTC_SYNC_CMD[5], &tsec, 4);
	/* UTC time microsecond unit */
	osal_memcpy(&WMT_UTC_SYNC_CMD[9], &tusec, 4);

	/* send command */
	iRet = wmt_core_tx(WMT_UTC_SYNC_CMD, sizeof(WMT_UTC_SYNC_CMD),
		&u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet) {
		WMT_ERR_FUNC("Tx WMT_UTC_SYNC_CMD fail!(%d) len (%d, %zu)\n",
			iRet, u4Res, sizeof(WMT_UTC_SYNC_CMD));
		return -1;
	}

	/* receive event */
	evtLen = osal_sizeof(WMT_UTC_SYNC_EVT);
	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || (u4Res != evtLen)) {
		WMT_ERR_FUNC("WMT-CORE: read WMT_UTC_SYNC_EVT fail(%d) len(%d, %d)\n",
			iRet, u4Res, evtLen);
		osal_assert(0);
		return iRet;
	}

	if (osal_memcmp(evtBuf, WMT_UTC_SYNC_EVT,
		osal_sizeof(WMT_UTC_SYNC_EVT)) != 0) {
		WMT_ERR_FUNC("WMT-CORE: compare WMT_UTC_SYNC_EVT error\n");
		WMT_ERR_FUNC("WMT-CORE: rx(%d):[%02X,%02X,%02X,%02X,%02X,%02X]\n",
			u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4], evtBuf[5]);
		WMT_ERR_FUNC("WMT-CORE: exp(%zu):[%02X,%02X,%02X,%02X,%02X,%02X]\n",
			osal_sizeof(WMT_UTC_SYNC_EVT), WMT_UTC_SYNC_EVT[0],
			WMT_UTC_SYNC_EVT[1], WMT_UTC_SYNC_EVT[2], WMT_UTC_SYNC_EVT[3],
			WMT_UTC_SYNC_EVT[4], WMT_UTC_SYNC_EVT[5]);
	} else {
		WMT_INFO_FUNC("Send WMT_UTC_SYNC_CMD command OK!\n");
	}
	return 0;
#else
	return -1;
#endif
}

static INT32 opfunc_fw_log_ctrl(P_WMT_OP pWmtOp)
{
	INT32 iRet;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[osal_sizeof(WMT_FW_LOG_CTRL_EVT)];

	/* fill command parameters  */
	WMT_FW_LOG_CTRL_CMD[5] = (UINT8)pWmtOp->au4OpData[0]; /* type */
	WMT_FW_LOG_CTRL_CMD[6] = (UINT8)pWmtOp->au4OpData[1]; /* on/off */
	WMT_FW_LOG_CTRL_CMD[7] = (UINT8)pWmtOp->au4OpData[2]; /* log level */

	/* send command */
	iRet = wmt_core_tx(WMT_FW_LOG_CTRL_CMD, sizeof(WMT_FW_LOG_CTRL_CMD), &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet) {
		WMT_ERR_FUNC("Tx WMT_FW_LOG_CTRL_CMD fail!(%d) len (%d, %zu)\n", iRet, u4Res,
				sizeof(WMT_FW_LOG_CTRL_CMD));
		return -1;
	}

	/* receive event */
	evtLen = osal_sizeof(WMT_FW_LOG_CTRL_EVT);
	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || (u4Res != evtLen)) {
		WMT_ERR_FUNC("WMT-CORE: read WMT_FW_LOG_CTRL_EVT fail(%d) len(%d, %d)\n", iRet, u4Res, evtLen);
		osal_assert(0);
		return iRet;
	}

	if (osal_memcmp(evtBuf, WMT_FW_LOG_CTRL_EVT, evtLen) != 0) {
		WMT_ERR_FUNC("WMT-CORE: compare WMT_FW_LOG_CTRL_EVT error\n");
		WMT_ERR_FUNC("WMT-CORE: tx(%zu):[%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x]\n",
				osal_sizeof(WMT_FW_LOG_CTRL_CMD), WMT_FW_LOG_CTRL_CMD[0], WMT_FW_LOG_CTRL_CMD[1],
				WMT_FW_LOG_CTRL_CMD[2], WMT_FW_LOG_CTRL_CMD[3], WMT_FW_LOG_CTRL_CMD[4],
				WMT_FW_LOG_CTRL_CMD[5], WMT_FW_LOG_CTRL_CMD[6], WMT_FW_LOG_CTRL_CMD[7]);
		WMT_ERR_FUNC("WMT-CORE: rx(%u):[%02x,%02x,%02x,%02x,%02x]\n",
				u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4]);
	} else {
		WMT_INFO_FUNC("Send WMT_FW_LOG_CTRL_EVT command OK!\n");
	}
	return 0;
}

static INT32 opfunc_wlan_probe(P_WMT_OP pWmtOp)
{
	ULONG ctrlPa1;
	ULONG ctrlPa2;
	INT32 iRet;
	UINT32 drvType = pWmtOp->au4OpData[0];


	iRet = wmt_lib_wlan_lock_aquire();
	atomic_set(&g_wifi_on_off_ready, 0);
	if (iRet) {
		WMT_ERR_FUNC("--->lock wlan_lock failed, iRet=%d\n", iRet);
		return iRet;
	}

	if (gMtkWmtCtx.eDrvStatus[drvType] == DRV_STS_FUNC_ON) {
		WMT_WARN_FUNC("func(%d) already on\n", drvType);
		iRet = 0;
		wmt_lib_wlan_lock_release();
		goto done;
	}

	if (gMtkWmtCtx.wmtHifConf.hifType == WMT_HIF_UART) {
		ctrlPa1 = WMT_SDIO_SLOT_SDIO1;
		ctrlPa2 = 1;	/* turn on SDIO1 slot */
		iRet = wmt_core_ctrl(WMT_CTRL_SDIO_HW, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("turn on SLOT_SDIO1 fail (%d)\n",
				     iRet);
			osal_assert(0);
		} else {
			gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO1] =
			    DRV_STS_FUNC_ON;
		}
	} else if (gMtkWmtCtx.wmtHifConf.hifType == WMT_HIF_SDIO) {
		if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO2] == DRV_STS_FUNC_ON) {
			WMT_DBG_FUNC("SLOT_SDIO2 ready for WIFI\n");
		} else {
			/* SDIO2 slot power shall be either turned on in STP init
			 * procedures already, or failed in STP init before. Here is
			 * the assert condition.
			 **/
			WMT_ERR_FUNC("turn on Wi-Fi SDIO2 but SDIO in FUNC_OFF state(0x%x)\n",
					gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO2]);
			osal_assert(0);
			iRet = -4;
			wmt_lib_wlan_lock_release();
			goto done;
		}
	} else {
		WMT_ERR_FUNC("not implemented yet hifType: 0x%x, unspecified wifi_hif\n",
				gMtkWmtCtx.wmtHifConf.hifType);
		/* TODO:  Wi-Fi/WMT uses other interfaces. NOT IMPLEMENTED YET! */
	}

	iRet = (*(gpWmtFuncOps[drvType]->func_on)) (gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());
	if (iRet != 0) {
		if (WMT_HIF_UART == gMtkWmtCtx.wmtHifConf.hifType) {
			/*need to power SDIO off when Power on Wi-Fi fail, in case of power leakage and
			 * right SDIO power status maintain
			 */
			ctrlPa1 = WMT_SDIO_SLOT_SDIO1;
			ctrlPa2 = 0;	/* turn off SDIO1 slot */
			wmt_core_ctrl(WMT_CTRL_SDIO_HW, &ctrlPa1, &ctrlPa2);
			/* does not need to check turn off result */
			gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO1] = DRV_STS_POWER_OFF;
		}
		gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;
	} else
		gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_FUNC_ON;

	/* wlan_lock must release before try_pwr_off */
	wmt_lib_wlan_lock_release();
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE:type(0x%x) function on failed, ret(%d)\n", drvType, iRet);
		/* FIX-ME:[Chaozhong Liang], Error handling? check subsystem state and do pwr off if necessary? */
		/* check all sub-func and do power off */
		wmt_lib_try_pwr_off();
	}

done:
	wmt_core_dump_func_state("AF FUNC ON");
	return iRet;
}

static INT32 opfunc_wlan_remove(P_WMT_OP pWmtOp)
{
	ULONG ctrlPa1;
	ULONG ctrlPa2;
	INT32 iRet;
	UINT32 drvType = pWmtOp->au4OpData[0];

	iRet = wmt_lib_wlan_lock_aquire();
	atomic_set(&g_wifi_on_off_ready, 0);
	if (iRet) {
		WMT_ERR_FUNC("--->lock wlan_lock failed, iRet=%d\n", iRet);
		return iRet;
	}

	if (gMtkWmtCtx.eDrvStatus[drvType] != DRV_STS_FUNC_ON) {
		WMT_WARN_FUNC("WMT-CORE: Fun(%d) DRV_STS_[0x%x] already non-FUN_ON in wmt_func_off\n",
			drvType, gMtkWmtCtx.eDrvStatus[drvType]);
		iRet = 0;
		wmt_lib_wlan_lock_release();
		goto done;
	}

	iRet = (*(gpWmtFuncOps[drvType]->func_off)) (gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());

	if (WMT_HIF_UART == gMtkWmtCtx.wmtHifConf.hifType) {
		UINT32 iRet = 0;

		ctrlPa1 = WMT_SDIO_SLOT_SDIO1;
		ctrlPa2 = 0;	/* turn off SDIO1 slot */
		iRet = wmt_core_ctrl(WMT_CTRL_SDIO_HW, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: turn on SLOT_SDIO1 fail (%d)\n",
				     iRet);
			osal_assert(0);
		}
		/* Anyway, turn SDIO1 state to POWER_OFF state */
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_SDIO1] = DRV_STS_POWER_OFF;
	}

	gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;

	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: type(0x%x) function off failed, ret(%d)\n", drvType, iRet);
		osal_assert(0);
		/* no matter subsystem function control fail or not, chip should be powered off
		 * when no subsystem is active
		 * return iRet;
		 */
	}

	/* wlan_lock must release before try_pwr_off */
	wmt_lib_wlan_lock_release();
	/* check all sub-func and do power off */
	wmt_lib_try_pwr_off();

done:
	wmt_core_dump_func_state("AF FUNC OFF");
	return iRet;
}

static INT32 opfunc_try_pwr_off(P_WMT_OP pWmtOp)
{
	INT32 iRet = 0;
	UINT32 drvType = pWmtOp->au4OpData[0];

	if (atomic_read(&g_wifi_on_off_ready) == 1) {
		WMT_INFO_FUNC("wlan on/off procedure will be started, do not power off now.\n");
		return iRet;
	}

	/* Why it can use try lock?
	 * Because only wmtd_worker_thread get wlan lock for wifi on/off in current design.
	 * It means it can decide whether to do Connsys power off after Wifi function on/off complete.
	 */
	if (wmt_lib_wlan_lock_trylock() == 0) {
		WMT_INFO_FUNC("Can't lock wlan mutex which might be held by wlan on/off procedure.\n");
		return iRet;
	}

	if ((gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT] == DRV_STS_POWER_OFF) &&
	    (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPS] == DRV_STS_POWER_OFF) &&
	    (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPSL5] == DRV_STS_POWER_OFF) &&
	    (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_FM] == DRV_STS_POWER_OFF) &&
	    (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI] == DRV_STS_POWER_OFF) &&
	    (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK] == DRV_STS_POWER_OFF) &&
	    (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_ANT] == DRV_STS_POWER_OFF) &&
	    (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP] == DRV_STS_POWER_OFF)) {
		WMT_INFO_FUNC("WMT-CORE:Fun(%d) [POWER_OFF] and power down chip\n", drvType);
		mtk_wcn_wmt_system_state_reset();
		iRet = opfunc_pwr_off(pWmtOp);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: wmt_pwr_off fail(%d) when turn off func(%d)\n",
				     iRet, drvType);
			osal_assert(0);
		}
	}
	wmt_lib_wlan_lock_release();
	return iRet;
}

static INT32 opfunc_gps_mcu_ctrl(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	INT32 fgFail = -1;
	PUINT8 p_tx_data_buf;
	UINT32 tx_data_len;
	PUINT8 p_rx_data_buf;
	UINT32 rx_data_buf_len;
	PUINT32 p_rx_data_len;
	UINT8 WMT_GPS_MCU_CTRL_CMD[] = {0x01, 0x32, 0x00, 0x00};
	PUINT8 p_tx_buf = NULL;
	PUINT8 p_rx_buf = NULL;

	p_tx_data_buf = (PUINT8)pWmtOp->au4OpData[0];
	tx_data_len = pWmtOp->au4OpData[1];
	p_rx_data_buf = (PUINT8)pWmtOp->au4OpData[2];
	rx_data_buf_len = pWmtOp->au4OpData[3];
	p_rx_data_len = (PINT32)(pWmtOp->au4OpData[4]);

	if ((!p_tx_data_buf) || (tx_data_len == 0) || (!p_rx_data_buf) || (rx_data_buf_len == 0)) {
		pWmtOp->au4OpData[5] = -1;
		WMT_ERR_FUNC("Parameter error!\n");
		return fgFail;
	}

	p_tx_buf = osal_malloc(tx_data_len + osal_sizeof(WMT_GPS_MCU_CTRL_CMD));
	if (!p_tx_buf) {
		pWmtOp->au4OpData[5] = -2;
		WMT_ERR_FUNC("p_tx_buf alloc fail!\n");
		return fgFail;
	}

	p_rx_buf = osal_malloc(rx_data_buf_len + osal_sizeof(WMT_GPS_MCU_CTRL_CMD));
	if (!p_rx_buf) {
		osal_free(p_tx_buf);
		pWmtOp->au4OpData[5] = -3;
		WMT_ERR_FUNC("p_rx_buf alloc fail!\n");
		return fgFail;
	}

	WMT_GPS_MCU_CTRL_CMD[2] = (tx_data_len & 0x000000ff);
	WMT_GPS_MCU_CTRL_CMD[3] = ((tx_data_len & 0x0000ff00) >> 8);
	osal_memcpy(p_tx_buf, WMT_GPS_MCU_CTRL_CMD, osal_sizeof(WMT_GPS_MCU_CTRL_CMD));
	osal_memcpy(p_tx_buf + osal_sizeof(WMT_GPS_MCU_CTRL_CMD), p_tx_data_buf, tx_data_len);

	do {

		iRet = wmt_core_tx(p_tx_buf, tx_data_len + osal_sizeof(WMT_GPS_MCU_CTRL_CMD), &u4WrittenSize,
				   MTK_WCN_BOOL_FALSE);
		if (iRet || (u4WrittenSize != (tx_data_len + osal_sizeof(WMT_GPS_MCU_CTRL_CMD)))) {
			WMT_ERR_FUNC("gps mcu ctrl tx CMD fail(%d),size(%d)\n", iRet, u4WrittenSize);
			break;
		}

		iRet = wmt_core_rx(p_rx_buf, rx_data_buf_len + osal_sizeof(WMT_GPS_MCU_CTRL_CMD),
				   &u4ReadSize);
		if (iRet || (p_rx_buf[1] != WMT_GPS_MCU_CTRL_CMD[1])) {
			WMT_ERR_FUNC("gps mcu ctrl rx EVT fail(%d),size(%d)\n", iRet, u4ReadSize);
			break;
		}
		*p_rx_data_len = (p_rx_buf[2] | (p_rx_buf[3] << 8));
		osal_memcpy(p_rx_data_buf, p_rx_buf + osal_sizeof(WMT_GPS_MCU_CTRL_CMD),
			    *p_rx_data_len > rx_data_buf_len ? rx_data_buf_len : *p_rx_data_len);

		fgFail = 0;
	} while (0);

	osal_free(p_tx_buf);
	osal_free(p_rx_buf);

	return fgFail;
}

P_WMT_GEN_CONF wmt_get_gen_conf_pointer(VOID)
{
	P_WMT_GEN_CONF pWmtGenConf = NULL;

	pWmtGenConf = wmt_conf_get_cfg();
	return pWmtGenConf;
}

VOID wmt_core_set_blank_status(UINT32 on_off_flag)
{
	gMtkWmtCtx.wmtBlankStatus = on_off_flag;
}

UINT32 wmt_core_get_blank_status(VOID)
{
	return gMtkWmtCtx.wmtBlankStatus;
}

INT32 wmt_blank_status_ctrl(UINT32 on_off_flag)
{
	INT32 iRet = 0;
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	WMT_BLANK_STATUS_CMD[5] = (on_off_flag) ? 0x1 : 0x0;

	/* send command */
	iRet = wmt_core_tx((PUINT8)WMT_BLANK_STATUS_CMD, osal_sizeof(WMT_BLANK_STATUS_CMD), &u4Res,
			   MTK_WCN_BOOL_FALSE);

	if (iRet || (u4Res != osal_sizeof(WMT_BLANK_STATUS_CMD))) {
		WMT_ERR_FUNC("WMT-CORE: WMT_BLANK_STATUS_CMD iRet(%d) cmd len err(%d, %zu)\n",
			     (iRet == 0 ? -1 : iRet), u4Res, osal_sizeof(WMT_BLANK_STATUS_CMD));
		return iRet;
	}

	evtLen = osal_sizeof(WMT_BLANK_STATUS_EVT);
	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || (u4Res != evtLen)) {
		WMT_ERR_FUNC("WMT-CORE: read WMT_BLANK_STATUS_EVT fail(%d) len(%d, %d)\n",
			     iRet, u4Res, evtLen);
		WMT_INFO_FUNC("buf:[%2X,%2X,%2X,%2X,%2X] evt:[%2X,%2X,%2X,%2X,%2X]\n",
				evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
				WMT_BLANK_STATUS_EVT[0], WMT_BLANK_STATUS_EVT[1],
				WMT_BLANK_STATUS_EVT[2], WMT_BLANK_STATUS_EVT[3],
				WMT_BLANK_STATUS_EVT[4]);
	}
	else
		wmt_lib_set_blank_status(WMT_BLANK_STATUS_CMD[5]);
#endif
	return iRet;
}

static INT32 opfunc_blank_status_ctrl(P_WMT_OP pWmtOp)
{
	return wmt_blank_status_ctrl(pWmtOp->au4OpData[0]);
}

static INT32 opfunc_met_ctrl(P_WMT_OP pWmtOp)
{
	INT32 iRet;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };
	UINT32 value;
	UINT8 WMT_MET_CTRL_CMD[] = { 0x01, 0x31, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00};
	UINT8 WMT_MET_CTRL_EVT[] = { 0x02, 0x31, 0x01, 0x00, 0x00 };

	value = pWmtOp->au4OpData[0];
	WMT_MET_CTRL_CMD[5] = (value & 0x000000ff);
	WMT_MET_CTRL_CMD[6] = (value & 0x0000ff00) >> 8;
	WMT_MET_CTRL_CMD[7] = (value & 0x00ff0000) >> 16;
	WMT_MET_CTRL_CMD[8] = (value & 0xff000000) >> 24;

	/* send command */
	iRet = wmt_core_tx(WMT_MET_CTRL_CMD, sizeof(WMT_MET_CTRL_CMD), &u4Res, MTK_WCN_BOOL_FALSE);
	if ((iRet) || (u4Res != sizeof(WMT_MET_CTRL_CMD))) {
		WMT_ERR_FUNC("Tx MET_CTRL_CMD fail!(%d) len (%d, %zu)\n", iRet, u4Res,
			     sizeof(WMT_MET_CTRL_CMD));
		return -2;
	}

	/* receive event */
	evtLen = sizeof(WMT_MET_CTRL_EVT);
	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if ((iRet) || (u4Res != evtLen)) {
		WMT_ERR_FUNC("Rx MET_CTRL_EVT fail!(%d) len(%d, %d)\n", iRet, u4Res, evtLen);
		mtk_wcn_stp_dbg_dump_package();
		wmt_core_trigger_assert();
		return -3;
	}

	return 0;
}

static INT32 opfunc_get_consys_state(P_WMT_OP pWmtOp)
{
	INT32 ret = 0, i;
	INT32 times = 0, slp_ms;
	P_CONSYS_STATE_DMP_OP dmp_op = (P_CONSYS_STATE_DMP_OP)pWmtOp->au4OpData[0];
	ULONG ver = (ULONG)pWmtOp->au4OpData[1];

	osal_lock_sleepable_lock(&dmp_op->lock);
	if (dmp_op->status == WMT_DUMP_STATE_NONE
		|| dmp_op->version != ver) {
		ret = -1;
		goto done;
	}

	/* WMT should be ON */
	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] != DRV_STS_FUNC_ON) {
		WMT_INFO_FUNC("WMT is not on");
		ret = -1;
		goto done;
	}

	if (mtk_wcn_consys_sleep_info_read_all_ctrl(&dmp_op->dmp_info.state) != 0)
		ret = -1;

	/* Consys register should be readable */
	if (mtk_consys_check_reg_readable() == 0) {
		WMT_INFO_FUNC("cr cannot readable");
		ret = -1;
		goto done;
	}

	times = dmp_op->times;
	slp_ms = dmp_op->cpu_sleep_ms;

	/* dmp cpu_pcr */
	for (i = 0; i < times; i++) {
		dmp_op->dmp_info.cpu_pcr[i] = wmt_plat_read_cpupcr();
		if (slp_ms > 0)
			osal_sleep_ms(slp_ms);
	}

	ret = mtk_consys_dump_osc_state(&dmp_op->dmp_info.state);
	if (ret != MTK_WCN_BOOL_TRUE)
		ret = -2;

	ret = mtk_wcn_consys_dump_gating_state(&dmp_op->dmp_info.state);
	if (ret != MTK_WCN_BOOL_TRUE)
		ret = -3;

done:
	osal_unlock_sleepable_lock(&dmp_op->lock);
	return 0;
}
