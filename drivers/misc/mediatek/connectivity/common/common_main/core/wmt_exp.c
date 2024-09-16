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
#define DFT_TAG         "[WMT-EXP]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "osal_typedef.h"
#include "wmt_step.h"

#include <wmt_exp.h>
#include <wmt_lib.h>
#include <wmt_detect.h>
#include <psm_core.h>
#include <hif_sdio.h>
#include <stp_dbg.h>
#include <stp_core.h>


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
wmt_wlan_probe_cb mtk_wcn_wlan_probe;
wmt_wlan_remove_cb mtk_wcn_wlan_remove;
wmt_wlan_bus_cnt_get_cb mtk_wcn_wlan_bus_tx_cnt;
wmt_wlan_bus_cnt_clr_cb mtk_wcn_wlan_bus_tx_cnt_clr;
wmt_wlan_emi_mpu_set_protection_cb mtk_wcn_wlan_emi_mpu_set_protection;
wmt_wlan_is_wifi_drv_own_cb mtk_wcn_wlan_is_wifi_drv_own;

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
OSAL_BIT_OP_VAR gBtWifiGpsState;
OSAL_BIT_OP_VAR gGpsFmState;
UINT32 gWifiProbed;
INT32 gWmtDbgLvl = WMT_LOG_INFO;
MTK_WCN_BOOL g_pwr_off_flag = MTK_WCN_BOOL_TRUE;

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

static MTK_WCN_BOOL mtk_wcn_wmt_pwr_on(VOID);
static MTK_WCN_BOOL mtk_wcn_wmt_func_ctrl(ENUM_WMTDRV_TYPE_T type, ENUM_WMT_OPID_T opId);
static MTK_WCN_BOOL mtk_wmt_gps_suspend_ctrl_by_type(MTK_WCN_BOOL gps_l1, MTK_WCN_BOOL gps_l5, MTK_WCN_BOOL suspend);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static MTK_WCN_BOOL mtk_wcn_wmt_pwr_on(VOID)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}
	pSignal = &pOp->signal;

	pOp->op.opId = WMT_OPID_PWR_ON;
	pSignal->timeoutValue = MAX_FUNC_ON_TIME;
	pOp->op.au4OpData[0] = WMTDRV_TYPE_WMT;

	wmt_lib_host_awake_get();
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed,OPID(%d) type(%zu) abort\n", pOp->op.opId, pOp->op.au4OpData[0]);
		wmt_lib_put_op_to_free_queue(pOp);
		wmt_lib_host_awake_put();
		return MTK_WCN_BOOL_FALSE;
	}

	bRet = wmt_lib_put_act_op(pOp);

	ENABLE_PSM_MONITOR();
	wmt_lib_host_awake_put();

	if (bRet == MTK_WCN_BOOL_FALSE)
		WMT_WARN_FUNC("OPID(%d) type(%zu) fail\n", pOp->op.opId, pOp->op.au4OpData[0]);

	return bRet;
}

static MTK_WCN_BOOL mtk_wcn_wmt_func_ctrl(ENUM_WMTDRV_TYPE_T type, ENUM_WMT_OPID_T opId)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;
	MTK_WCN_BOOL bOffload;
	MTK_WCN_BOOL bExplicitPwrOn;

	bOffload = (type == WMTDRV_TYPE_WIFI);
	bExplicitPwrOn = (bOffload && opId == WMT_OPID_FUNC_ON &&
				wmt_lib_get_drv_status(WMTDRV_TYPE_WMT) != DRV_STS_FUNC_ON);

	/* WIFI on no need to disable psm and prevent WIFI on blocked by psm lock. */
	/* So we power on connsys separately from function on flow. */
	if (bExplicitPwrOn)
		mtk_wcn_wmt_pwr_on();

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pSignal = &pOp->signal;

	pOp->op.opId = opId;
	pOp->op.au4OpData[0] = type;
	if (type == WMTDRV_TYPE_WIFI)
		pSignal->timeoutValue = 4000;
	else
		pSignal->timeoutValue = (pOp->op.opId == WMT_OPID_FUNC_ON) ? MAX_FUNC_ON_TIME : MAX_FUNC_OFF_TIME;

	WMT_INFO_FUNC("wmt-exp: OPID(%d) type(%zu) start\n", pOp->op.opId, pOp->op.au4OpData[0]);
	WMT_STEP_FUNC_CTRL_DO_ACTIONS_FUNC(type, opId);

	/*do not check return value, we will do this either way */
	wmt_lib_host_awake_get();
	/* wake up chip first */
	if (!bOffload) {
		if (DISABLE_PSM_MONITOR()) {
			WMT_ERR_FUNC("wake up failed,OPID(%d) type(%zu) abort\n", pOp->op.opId, pOp->op.au4OpData[0]);
			wmt_lib_put_op_to_free_queue(pOp);
			wmt_lib_host_awake_put();
			return MTK_WCN_BOOL_FALSE;
		}
	}

	bRet = wmt_lib_put_act_op(pOp);
	if (!bOffload)
		ENABLE_PSM_MONITOR();
	wmt_lib_host_awake_put();

	if (bRet == MTK_WCN_BOOL_FALSE)
		WMT_WARN_FUNC("OPID(%d) type(%zu) fail\n", pOp->op.opId, pOp->op.au4OpData[0]);
	else
		WMT_INFO_FUNC("OPID(%d) type(%zu) ok\n", pOp->op.opId, pOp->op.au4OpData[0]);

	return bRet;
}

INT32 mtk_wcn_wmt_psm_ctrl(MTK_WCN_BOOL flag)
{
	return -EFAULT;
}
EXPORT_SYMBOL(mtk_wcn_wmt_psm_ctrl);

MTK_WCN_BOOL mtk_wcn_wmt_func_off(ENUM_WMTDRV_TYPE_T type)
{
	MTK_WCN_BOOL ret;

	if (type == WMTDRV_TYPE_BT) {
		osal_printtimeofday("############ BT OFF ====>");
	}

	ret = mtk_wcn_wmt_func_ctrl(type, WMT_OPID_FUNC_OFF);

	if (type == WMTDRV_TYPE_BT)
		osal_printtimeofday("############ BT OFF <====");

	return ret;
}
EXPORT_SYMBOL(mtk_wcn_wmt_func_off);

MTK_WCN_BOOL mtk_wcn_wmt_func_on(ENUM_WMTDRV_TYPE_T type)
{
	MTK_WCN_BOOL ret;

	if (type == WMTDRV_TYPE_BT)
		osal_printtimeofday("############ BT ON ====>");

	ret = mtk_wcn_wmt_func_ctrl(type, WMT_OPID_FUNC_ON);

	if (type == WMTDRV_TYPE_BT)
		osal_printtimeofday(" ############BT ON <====");

	return ret;
}
EXPORT_SYMBOL(mtk_wcn_wmt_func_on);

/*
*return value:
*enable/disable thermal sensor function: true(1)/false(0)
*read thermal sensor function:thermal value
*/
VOID mtk_wcn_wmt_func_ctrl_for_plat(UINT32 on, ENUM_WMTDRV_TYPE_T type)
{
	MTK_WCN_BOOL ret;

	if (on)
		ret = mtk_wcn_wmt_func_on(type);
	else
		ret = mtk_wcn_wmt_func_off(type);

	WMT_INFO_FUNC("on=%d type=%d ret=%d\n", on, type, ret);
}

INT8 mtk_wcn_wmt_therm_ctrl(ENUM_WMTTHERM_TYPE_T eType)
{
	P_OSAL_OP pOp;
	P_WMT_OP pOpData;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	/*parameter validation check */
	if (eType > WMTTHERM_MAX || eType < WMTTHERM_ENABLE) {
		WMT_ERR_FUNC("invalid thermal control command (%d)\n", eType);
		return MTK_WCN_BOOL_FALSE;
	}

	/*check if chip support thermal control function or not */
	bRet = wmt_lib_is_therm_ctrl_support(eType);
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_DBG_FUNC("thermal ctrl function not supported\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pSignal = &pOp->signal;
	pOpData = &pOp->op;
	pOpData->opId = WMT_OPID_THERM_CTRL;
	/*parameter fill */
	pOpData->au4OpData[0] = eType;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;

	WMT_DBG_FUNC("OPID(%d) type(%zu) start\n", pOp->op.opId, pOp->op.au4OpData[0]);
	WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_BEFORE_READ_THERMAL);

	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed,OPID(%d) type(%zu) abort!\n", pOp->op.opId, pOp->op.au4OpData[0]);
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("OPID(%d) type(%zu) fail\n\n", pOpData->opId, pOpData->au4OpData[0]);
		/*0xFF means read error occurs */
		/*will return to function driver */
		pOpData->au4OpData[1] = (eType == WMTTHERM_READ) ? 0xFF : MTK_WCN_BOOL_FALSE;
	} else
		WMT_DBG_FUNC("OPID(%d) type(%zu) return(%zu) ok\n\n",
				pOpData->opId, pOpData->au4OpData[0], pOpData->au4OpData[1]);
	/*return value will be put to lxop->op.au4OpData[1] */
	WMT_DBG_FUNC("therm ctrl type(%d), iRet(0x%08zx)\n", eType, pOpData->au4OpData[1]);

	return (INT8) pOpData->au4OpData[1];
}
EXPORT_SYMBOL(mtk_wcn_wmt_therm_ctrl);

ENUM_WMTHWVER_TYPE_T mtk_wcn_wmt_hwver_get(VOID)
{
	/* TODO: [ChangeFeature][GeorgeKuo] Reconsider usage of this type */
	/* TODO: how do we extend for new chip and newer revision? */
	/* TODO: This way is hard to extend */
	return wmt_lib_get_icinfo(WMTCHIN_MAPPINGHWVER);
}
EXPORT_SYMBOL(mtk_wcn_wmt_hwver_get);

UINT32 mtk_wcn_wmt_ic_info_get(ENUM_WMT_CHIPINFO_TYPE_T type)
{
	return wmt_lib_get_icinfo(type);
}
EXPORT_SYMBOL(mtk_wcn_wmt_ic_info_get);

MTK_WCN_BOOL mtk_wcn_wmt_dsns_ctrl(ENUM_WMTDSNS_TYPE_T eType)
{
	P_OSAL_OP pOp;
	P_WMT_OP pOpData;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	if (eType >= WMTDSNS_MAX) {
		WMT_ERR_FUNC("invalid desense control command (%d)\n", eType);
		return MTK_WCN_BOOL_FALSE;
	}

	/*check if chip support thermal control function or not */
	bRet = wmt_lib_is_dsns_ctrl_support();
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_ERR_FUNC("thermal ctrl function not supported\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pSignal = &pOp->signal;
	pOpData = &pOp->op;
	pOpData->opId = WMT_OPID_DSNS;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;
	/*parameter fill */
	if ((eType >= WMTDSNS_FM_DISABLE) && (eType <= WMTDSNS_FM_GPS_ENABLE)) {
		pOpData->au4OpData[0] = WMTDRV_TYPE_FM;
		pOpData->au4OpData[1] = eType;
	}

	WMT_INFO_FUNC("OPID(%d) type(%zu) start\n", pOp->op.opId, pOp->op.au4OpData[0]);

	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed,OPID(%d) type(%zu) abort!\n", pOp->op.opId, pOp->op.au4OpData[0]);
		wmt_lib_put_op_to_free_queue(pOp);
		return MTK_WCN_BOOL_FALSE;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	if (bRet == MTK_WCN_BOOL_FALSE)
		WMT_WARN_FUNC("OPID(%d) type(%zu) fail\n\n", pOpData->opId, pOpData->au4OpData[0]);
	else
		WMT_INFO_FUNC("OPID(%d) type(%zu) ok\n\n", pOpData->opId, pOpData->au4OpData[0]);

	return bRet;
}
EXPORT_SYMBOL(mtk_wcn_wmt_dsns_ctrl);

INT32 mtk_wcn_wmt_msgcb_reg(ENUM_WMTDRV_TYPE_T eType, PF_WMT_CB pCb)
{
	return (INT32) wmt_lib_msgcb_reg(eType, pCb);
}
EXPORT_SYMBOL(mtk_wcn_wmt_msgcb_reg);

INT32 mtk_wcn_wmt_msgcb_unreg(ENUM_WMTDRV_TYPE_T eType)
{
	return (INT32) wmt_lib_msgcb_unreg(eType);
}
EXPORT_SYMBOL(mtk_wcn_wmt_msgcb_unreg);

INT32 mtk_wcn_stp_wmt_sdio_op_reg(PF_WMT_SDIO_PSOP own_cb)
{
	wmt_lib_ps_set_sdio_psop(own_cb);
	return 0;
}
EXPORT_SYMBOL(mtk_wcn_stp_wmt_sdio_op_reg);

#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
INT32 mtk_wcn_wmt_sdio_deep_sleep_flag_cb_reg(PF_WMT_SDIO_DEEP_SLEEP flag_cb)
{
	wmt_lib_sdio_deep_sleep_flag_set_cb_reg(flag_cb);
	return 0;
}
EXPORT_SYMBOL(mtk_wcn_wmt_sdio_deep_sleep_flag_cb_reg);
#endif

INT32 mtk_wcn_wmt_sdio_rw_cb_reg(PF_WMT_SDIO_DEBUG reg_rw_cb)
{
	wmt_lib_sdio_reg_rw_cb(reg_rw_cb);
	return 0;
}

INT32 mtk_wcn_stp_wmt_sdio_host_awake(VOID)
{
	wmt_lib_ps_irq_cb();
	return 0;
}
EXPORT_SYMBOL(mtk_wcn_stp_wmt_sdio_host_awake);

MTK_WCN_BOOL mtk_wcn_wmt_assert_timeout(ENUM_WMTDRV_TYPE_T type, UINT32 reason, INT32 timeout)
{
	MTK_WCN_BOOL bRet;

	bRet = wmt_lib_trigger_assert(type, reason);

	return bRet == 0 ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE;
}
EXPORT_SYMBOL(mtk_wcn_wmt_assert_timeout);

MTK_WCN_BOOL mtk_wcn_wmt_assert(ENUM_WMTDRV_TYPE_T type, UINT32 reason)
{
	return mtk_wcn_wmt_assert_timeout(type, reason, MAX_EACH_WMT_CMD);
}
EXPORT_SYMBOL(mtk_wcn_wmt_assert);

MTK_WCN_BOOL mtk_wcn_wmt_assert_keyword(ENUM_WMTDRV_TYPE_T type, PUINT8 keyword)
{
	MTK_WCN_BOOL bRet;

	bRet = wmt_lib_trigger_assert_keyword(type, 0, keyword);

	return bRet == 0 ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE;
}
EXPORT_SYMBOL(mtk_wcn_wmt_assert_keyword);

/*
*	ctrlId: get flash patch version opId or flash patch download opId
*	pBuf: pointer to flash patch
*	length: total length of flash patch
*	type: flash patch type
*	version: flash patch version
*	checksum: flash patch checksum
*/
ENUM_WMT_FLASH_PATCH_STATUS mtk_wcn_wmt_flash_patch_ctrl(ENUM_WMT_FLASH_PATCH_CTRL ctrlId,
		PUINT8 pBuf, UINT32 length, ENUM_WMT_FLASH_PATCH_SEQ seq, ENUM_WMT_FLASH_PATCH_TYPE type,
		PUINT32 version, UINT32 checksum)
{
	ENUM_WMT_FLASH_PATCH_STATUS eRet = 0;
	P_OSAL_OP pOp = NULL;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_OSAL_SIGNAL pSignal;

	/*1. parameter validation check */
	/*for WMT_FLASH_PATCH_VERSION_GET, ignore pBuf and length */
	/*for WMT_ANT_RAM_DOWNLOAD,
	 *  pBuf must not be NULL, kernel space memory pointer
	 *  length must be large than 0
	 */
	switch (ctrlId) {
	case WMT_FLASH_PATCH_VERSION_GET:
		break;
	case WMT_FLASH_PATCH_DOWNLOAD:
		if ((pBuf == NULL) || (length <= 0) || (length > 1000)) {
			WMT_ERR_FUNC("error parameter detected, ctrlId:%d, pBuf:%p,length(0x%x).\n",
					ctrlId, pBuf, length);
			eRet = WMT_FLASH_PATCH_PARA_ERR;
			goto exit;
		} else
			break;
	default:
		WMT_ERR_FUNC("error ctrlId:%d detected.\n", ctrlId);
		eRet = WMT_FLASH_PATCH_PARA_ERR;
		goto exit;
	}

	/*get WMT opId */
	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		eRet = WMT_FLASH_PATCH_OP_ERR;
		goto exit;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue =
	    (ctrlId == WMT_FLASH_PATCH_DOWNLOAD) ? MAX_FUNC_ON_TIME : MAX_EACH_WMT_CMD;

	pOp->op.opId = (ctrlId == WMT_FLASH_PATCH_DOWNLOAD) ?
		WMT_OPID_FLASH_PATCH_DOWN : WMT_OPID_FLASH_PATCH_VER_GET;
	pOp->op.au4OpData[0] = (size_t) pBuf;
	pOp->op.au4OpData[1] = length;
	pOp->op.au4OpData[2] = seq;
	pOp->op.au4OpData[3] = type;
	pOp->op.au4OpData[4] = *version;
	pOp->op.au4OpData[5] = checksum;

	/*disable PSM monitor */
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		eRet = WMT_FLASH_PATCH_OP_ERR;
		goto exit;
	}
	/*wakeup wmtd thread */
	bRet = wmt_lib_put_act_op(pOp);

	/*enable PSM monitor */
	ENABLE_PSM_MONITOR();

	WMT_INFO_FUNC("CMD_TEST, opid (%d), ret(%d),retVal(%zu) result(%s)\n",
		      pOp->op.opId,
		      bRet,
		      pOp->op.au4OpData[6], MTK_WCN_BOOL_FALSE == bRet ? "failed" : "succeed");

	/*check return value and return result */
	if (bRet == MTK_WCN_BOOL_FALSE) {
		eRet = WMT_FLASH_PATCH_OP_ERR;
	} else {
		switch (ctrlId) {
		case WMT_FLASH_PATCH_VERSION_GET:
			if (pOp->op.au4OpData[6] == 0) {
				*version = pOp->op.au4OpData[4];
				eRet = WMT_FLASH_PATCH_VERSION_GET_OK;
			} else
				eRet = WMT_FLASH_PATCH_VERSION_GET_FAIL;
			break;
		case WMT_FLASH_PATCH_DOWNLOAD:
			eRet = (pOp->op.au4OpData[6] == 0) ?
				WMT_FLASH_PATCH_DOWNLOAD_OK : WMT_FLASH_PATCH_DOWNLOAD_FAIL;
			break;
		default:
			WMT_ERR_FUNC("error ctrlId:%d detected.\n", ctrlId);
			eRet = WMT_FLASH_PATCH_PARA_ERR;
			break;
		}
	}

exit:
	return eRet;
}
EXPORT_SYMBOL(mtk_wcn_wmt_flash_patch_ctrl);

#if !(DELETE_HIF_SDIO_CHRDEV)
extern INT32 mtk_wcn_wmt_chipid_query(VOID)
{
	return mtk_wcn_hif_sdio_query_chipid(0);
}
EXPORT_SYMBOL(mtk_wcn_wmt_chipid_query);
#endif

INT8 mtk_wcn_wmt_co_clock_flag_get(void)
{
	return wmt_lib_co_clock_get();
}
EXPORT_SYMBOL(mtk_wcn_wmt_co_clock_flag_get);

INT32 mtk_wcn_wmt_system_state_reset(void)
{
	osal_memset(&gBtWifiGpsState, 0, osal_sizeof(gBtWifiGpsState));
	osal_memset(&gGpsFmState, 0, osal_sizeof(gGpsFmState));

	return 0;
}

INT32 mtk_wcn_wmt_wlan_reg(P_MTK_WCN_WMT_WLAN_CB_INFO pWmtWlanCbInfo)
{
	INT32 iRet = -1;

	if (!pWmtWlanCbInfo) {
		WMT_ERR_FUNC("wlan cb info in null!\n");
		return -1;
	}

	WMT_INFO_FUNC("wmt wlan cb register\n");
	mtk_wcn_wlan_probe = pWmtWlanCbInfo->wlan_probe_cb;
	mtk_wcn_wlan_remove = pWmtWlanCbInfo->wlan_remove_cb;
	mtk_wcn_wlan_bus_tx_cnt = pWmtWlanCbInfo->wlan_bus_cnt_get_cb;
	mtk_wcn_wlan_bus_tx_cnt_clr = pWmtWlanCbInfo->wlan_bus_cnt_clr_cb;
	mtk_wcn_wlan_emi_mpu_set_protection = pWmtWlanCbInfo->wlan_emi_mpu_set_protection_cb;
	mtk_wcn_wlan_is_wifi_drv_own = pWmtWlanCbInfo->wlan_is_wifi_drv_own_cb;

	if (gWifiProbed) {
		WMT_INFO_FUNC("wlan has been done power on,call probe directly\n");
		iRet = (*mtk_wcn_wlan_probe) ();
		if (!iRet) {
			WMT_INFO_FUNC("call wlan probe OK when do wlan register to wmt\n");
			gWifiProbed = 0;
		} else {
			WMT_ERR_FUNC("call wlan probe fail(%d) when do wlan register to wmt\n", iRet);
			return -2;
		}
	}
	return 0;
}
EXPORT_SYMBOL(mtk_wcn_wmt_wlan_reg);

INT32 mtk_wcn_wmt_wlan_unreg(void)
{
	WMT_INFO_FUNC("wmt wlan cb unregister\n");
	mtk_wcn_wlan_probe = NULL;
	mtk_wcn_wlan_remove = NULL;
	mtk_wcn_wlan_bus_tx_cnt = NULL;
	mtk_wcn_wlan_bus_tx_cnt_clr = NULL;
	mtk_wcn_wlan_emi_mpu_set_protection = NULL;
	mtk_wcn_wlan_is_wifi_drv_own = NULL;

	return 0;
}
EXPORT_SYMBOL(mtk_wcn_wmt_wlan_unreg);

MTK_WCN_BOOL mtk_wcn_set_connsys_power_off_flag(MTK_WCN_BOOL value)
{
	g_pwr_off_flag = value;
	if (g_pwr_off_flag)
		WMT_DBG_FUNC("enable connsys power off flag\n");
	else
		WMT_INFO_FUNC("disable connsys power off, maybe need trigger coredump!\n");
	return g_pwr_off_flag;
}
EXPORT_SYMBOL(mtk_wcn_set_connsys_power_off_flag);

#ifdef CONFIG_MTK_COMBO_ANT
/*
*	ctrlId: get ram code status opId or ram code download opId
*	pBuf: pointer to ANT ram code
*	length: total length of ANT ram code
*/
ENUM_WMT_ANT_RAM_STATUS mtk_wcn_wmt_ant_ram_ctrl(ENUM_WMT_ANT_RAM_CTRL ctrlId, PUINT8 pBuf,
						 UINT32 length, ENUM_WMT_ANT_RAM_SEQ seq)
{
	ENUM_WMT_ANT_RAM_STATUS eRet = 0;
	P_OSAL_OP pOp = NULL;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_OSAL_SIGNAL pSignal;

	/*1. parameter validation check */
	/*for WMT_ANT_RAM_GET_STATUS, ignore pBuf and length */
	/*for WMT_ANT_RAM_DOWNLOAD,
	 *  pBuf must not be NULL, kernel space memory pointer
	 *  length must be large than 0
	 */
	if ((ctrlId < WMT_ANT_RAM_GET_STATUS) || (ctrlId >= WMT_ANT_RAM_CTRL_MAX)) {
		WMT_ERR_FUNC("error ctrlId:%d detected.\n", ctrlId);
		eRet = WMT_ANT_RAM_PARA_ERR;
		return eRet;
	}

	if ((ctrlId == WMT_ANT_RAM_DOWNLOAD) && ((pBuf == NULL) || (length <= 0) ||
	     (length > 1000) || (seq >= WMT_ANT_RAM_SEQ_MAX) || (seq < WMT_ANT_RAM_START_PKT))) {
		eRet = WMT_ANT_RAM_PARA_ERR;
		WMT_ERR_FUNC
			("error parameter detected, ctrlId:%d, pBuf:%p,length(0x%x),seq(%d) .\n",
			ctrlId, pBuf, length, seq);
		return eRet;
	}
	/*get WMT opId */
	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue =
	    (ctrlId == WMT_ANT_RAM_DOWNLOAD) ? MAX_FUNC_ON_TIME : MAX_EACH_WMT_CMD;

	pOp->op.opId =
	    (ctrlId == WMT_ANT_RAM_DOWNLOAD) ? WMT_OPID_ANT_RAM_DOWN : WMT_OPID_ANT_RAM_STA_GET;
	pOp->op.au4OpData[0] = (size_t) pBuf;
	pOp->op.au4OpData[1] = length;
	pOp->op.au4OpData[2] = seq;


	/*disable PSM monitor */
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return MTK_WCN_BOOL_FALSE;
	}
	/*wakeup wmtd thread */
	bRet = wmt_lib_put_act_op(pOp);

	/*enable PSM monitor */
	ENABLE_PSM_MONITOR();

	WMT_INFO_FUNC("CMD_TEST, opid (%d), ret(%d),retVal(%zu) result(%s)\n",
		      pOp->op.opId,
		      bRet,
		      pOp->op.au4OpData[2], MTK_WCN_BOOL_FALSE == bRet ? "failed" : "succeed");

	/*check return value and return result */
	if (bRet == MTK_WCN_BOOL_FALSE) {
		eRet = WMT_ANT_RAM_OP_ERR;
	} else {
		eRet = (ctrlId == WMT_ANT_RAM_DOWNLOAD) ?
		    WMT_ANT_RAM_DOWN_OK :
		    ((pOp->op.au4OpData[2] == 1) ? WMT_ANT_RAM_EXIST : WMT_ANT_RAM_NOT_EXIST);
	}

	return eRet;

}
EXPORT_SYMBOL(mtk_wcn_wmt_ant_ram_ctrl);
#endif
MTK_WCN_BOOL mtk_wcn_wmt_do_reset(ENUM_WMTDRV_TYPE_T type)
{
	INT32 iRet = -1;
	UINT8 *drv_name[] = {
		[0] = "DRV_TYPE_BT",
		[1] = "DRV_TYPE_FM",
		[2] = "DRV_TYPE_GPS",
		[3] = "DRV_TYPE_WIFI",
		[4] = "DRV_TYPE_WMT",
		[5] = "DRV_TYPE_ANT",
		[11] = "DRV_TYPE_GPSL5",
	};

	if ((type < WMTDRV_TYPE_BT) || (type > WMTDRV_TYPE_ANT)) {
		WMT_INFO_FUNC("Wrong driver type: %d, do not trigger reset.\n", type);
		return MTK_WCN_BOOL_FALSE;
	}

	WMT_INFO_FUNC("Subsystem trigger whole chip reset, reset source: %s\n", drv_name[type]);
	if (mtk_wcn_stp_get_wmt_trg_assert() == 0)
		iRet = wmt_lib_trigger_reset();
	else {
		WMT_INFO_FUNC("assert has been triggered that no chip reset is required\n");
		iRet = 0;
	}

	return iRet == 0 ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE;
}
EXPORT_SYMBOL(mtk_wcn_wmt_do_reset);

MTK_WCN_BOOL mtk_wcn_wmt_do_reset_only(ENUM_WMTDRV_TYPE_T type)
{
	INT32 iRet = -1;

	WMT_INFO_FUNC("Whole chip reset without trigger assert\n");
	if (mtk_wcn_stp_get_wmt_trg_assert() == 0) {
		chip_reset_only = 1;
		iRet = wmt_lib_trigger_reset();
	} else {
		WMT_INFO_FUNC("assert has been triggered already\n");
		iRet = 0;
	}

	return iRet == 0 ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE;
}
EXPORT_SYMBOL(mtk_wcn_wmt_do_reset_only);

VOID mtk_wcn_wmt_set_wifi_ver(UINT32 Value)
{
	wmt_lib_soc_set_wifiver(Value);
}
EXPORT_SYMBOL(mtk_wcn_wmt_set_wifi_ver);

INT32 mtk_wcn_wmt_wifi_fem_cfg_report(PVOID pvInfoBuf)
{
	INT32 iRet = -1;

	iRet = wmt_lib_wifi_fem_cfg_report(pvInfoBuf);
	return iRet;
}
EXPORT_SYMBOL(mtk_wcn_wmt_wifi_fem_cfg_report);

VOID mtk_wcn_wmt_dump_wmtd_backtrace(VOID)
{
	wmt_lib_dump_wmtd_backtrace();
}
EXPORT_SYMBOL(mtk_wcn_wmt_dump_wmtd_backtrace);

UINT32 mtk_wmt_get_gps_lna_pin_num(VOID)
{
	return wmt_lib_get_gps_lna_pin_num();
}
EXPORT_SYMBOL(mtk_wmt_get_gps_lna_pin_num);

VOID mtk_wmt_set_ext_ldo(UINT32 flag)
{
	wmt_lib_set_ext_ldo(flag);
}
EXPORT_SYMBOL(mtk_wmt_set_ext_ldo);

INT32 mtk_wmt_gps_mcu_ctrl(PUINT8 p_tx_data_buf, UINT32 tx_data_len, PUINT8 p_rx_data_buf,
			   UINT32 rx_data_buf_len, PUINT32 p_rx_data_len)
{
	return wmt_lib_gps_mcu_ctrl(p_tx_data_buf, tx_data_len, p_rx_data_buf, rx_data_buf_len,
				    p_rx_data_len);
}
EXPORT_SYMBOL(mtk_wmt_gps_mcu_ctrl);

VOID mtk_wcn_wmt_set_mcif_mpu_protection(MTK_WCN_BOOL enable)
{
	mtk_consys_set_mcif_mpu_protection(enable);
}
EXPORT_SYMBOL(mtk_wcn_wmt_set_mcif_mpu_protection);

static MTK_WCN_BOOL mtk_wmt_gps_suspend_ctrl_by_type(MTK_WCN_BOOL gps_l1, MTK_WCN_BOOL gps_l5, MTK_WCN_BOOL suspend)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pSignal = &pOp->signal;

	pOp->op.opId = WMT_OPID_GPS_SUSPEND;
	pOp->op.au4OpData[0] = (MTK_WCN_BOOL_FALSE == suspend ? 0 : 1);
	pOp->op.au4OpData[1] = (MTK_WCN_BOOL_FALSE == gps_l1 ? 0 : 1);
	pOp->op.au4OpData[2] = (MTK_WCN_BOOL_FALSE == gps_l5 ? 0 : 1);
	pSignal->timeoutValue = (MTK_WCN_BOOL_FALSE == suspend) ? MAX_FUNC_ON_TIME : MAX_FUNC_OFF_TIME;

	WMT_INFO_FUNC("wmt-exp: OPID(%d) type(%zu) start\n", pOp->op.opId, pOp->op.au4OpData[0]);

	/*do not check return value, we will do this either way */
	wmt_lib_host_awake_get();
	/* wake up chip first */
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed,OPID(%d) type(%zu) abort\n", pOp->op.opId, pOp->op.au4OpData[0]);
		wmt_lib_put_op_to_free_queue(pOp);
		wmt_lib_host_awake_put();
		return MTK_WCN_BOOL_FALSE;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();
	wmt_lib_host_awake_put();

	if (bRet == MTK_WCN_BOOL_FALSE)
		WMT_WARN_FUNC("OPID(%d) type(%zu) fail\n", pOp->op.opId, pOp->op.au4OpData[0]);
	else
		WMT_INFO_FUNC("OPID(%d) type(%zu) ok\n", pOp->op.opId, pOp->op.au4OpData[0]);

	return bRet;
}

MTK_WCN_BOOL mtk_wmt_gps_suspend_ctrl(MTK_WCN_BOOL suspend)
{
	return mtk_wmt_gps_suspend_ctrl_by_type(MTK_WCN_BOOL_TRUE, MTK_WCN_BOOL_TRUE, suspend);
}
EXPORT_SYMBOL(mtk_wmt_gps_suspend_ctrl);

MTK_WCN_BOOL mtk_wmt_gps_l1_suspend_ctrl(MTK_WCN_BOOL suspend)
{
	return mtk_wmt_gps_suspend_ctrl_by_type(MTK_WCN_BOOL_TRUE, MTK_WCN_BOOL_FALSE, suspend);
}
EXPORT_SYMBOL(mtk_wmt_gps_l1_suspend_ctrl);

MTK_WCN_BOOL mtk_wmt_gps_l5_suspend_ctrl(MTK_WCN_BOOL suspend)
{
	return mtk_wmt_gps_suspend_ctrl_by_type(MTK_WCN_BOOL_FALSE, MTK_WCN_BOOL_TRUE, suspend);
}
EXPORT_SYMBOL(mtk_wmt_gps_l5_suspend_ctrl);

INT32 mtk_wcn_wmt_mpu_lock_aquire(VOID)
{
	return wmt_lib_mpu_lock_aquire();
}
EXPORT_SYMBOL(mtk_wcn_wmt_mpu_lock_aquire);

VOID mtk_wcn_wmt_mpu_lock_release(VOID)
{
	wmt_lib_mpu_lock_release();
}
EXPORT_SYMBOL(mtk_wcn_wmt_mpu_lock_release);

