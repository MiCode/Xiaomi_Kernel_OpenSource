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

#include <wmt_exp.h>
#include <wmt_lib.h>
#include <psm_core.h>
#include <hif_sdio.h>


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
wmt_wlan_probe_cb mtk_wcn_wlan_probe = NULL;
wmt_wlan_remove_cb mtk_wcn_wlan_remove = NULL;
wmt_wlan_bus_cnt_get_cb mtk_wcn_wlan_bus_tx_cnt = NULL;
wmt_wlan_bus_cnt_clr_cb mtk_wcn_wlan_bus_tx_cnt_clr = NULL;

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
OSAL_BIT_OP_VAR gBtWifiGpsState;
OSAL_BIT_OP_VAR gGpsFmState;
UINT32 gWifiProbed = 0;
UINT32 gWmtDbgLvl = WMT_LOG_INFO;
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

static MTK_WCN_BOOL mtk_wcn_wmt_func_ctrl(ENUM_WMTDRV_TYPE_T type, ENUM_WMT_OPID_T opId);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static MTK_WCN_BOOL mtk_wcn_wmt_func_ctrl(ENUM_WMTDRV_TYPE_T type, ENUM_WMT_OPID_T opId)
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

	pOp->op.opId = opId;
#if MTK_WCN_CMB_FOR_SDIO_1V_AUTOK
	if (WMTDRV_TYPE_AUTOK == type)
		pOp->op.au4OpData[0] = WMTDRV_TYPE_WIFI;
	else
#endif
		pOp->op.au4OpData[0] = type;
	if (WMTDRV_TYPE_WIFI == type)
		pSignal->timeoutValue = 4000;
		/*donot block system server/Init/Netd from longer than 5s, in case of ANR happens */
	else
		pSignal->timeoutValue =
		    (WMT_OPID_FUNC_ON == pOp->op.opId) ? MAX_FUNC_ON_TIME : MAX_FUNC_OFF_TIME;

	WMT_INFO_FUNC("wmt-exp: OPID(%d) type(%zu) start\n", pOp->op.opId, pOp->op.au4OpData[0]);

	/*do not check return value, we will do this either way */
	wmt_lib_host_awake_get();
	/*wake up chip first */
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed,OPID(%d) type(%zu) abort\n", pOp->op.opId, pOp->op.au4OpData[0]);
		wmt_lib_put_op_to_free_queue(pOp);
		return MTK_WCN_BOOL_FALSE;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();
	wmt_lib_host_awake_put();

	if (MTK_WCN_BOOL_FALSE == bRet)
		WMT_WARN_FUNC("OPID(%d) type(%zu) fail\n", pOp->op.opId, pOp->op.au4OpData[0]);
	else
		WMT_INFO_FUNC("OPID(%d) type(%zu) ok\n", pOp->op.opId, pOp->op.au4OpData[0]);

	return bRet;
}

INT32 mtk_wcn_wmt_psm_ctrl(MTK_WCN_BOOL flag)
{
#if CFG_WMT_PS_SUPPORT
	if (flag == MTK_WCN_BOOL_FALSE) {
		wmt_lib_ps_ctrl(0);
		WMT_INFO_FUNC("disable PSM\n");
	} else {
		wmt_lib_ps_set_idle_time(5000);
		wmt_lib_ps_ctrl(1);
		WMT_INFO_FUNC("enable PSM, idle to sleep time = 5000 ms\n");
	}
#else
	WMT_INFO_FUNC("WMT PS not supported\n");
#endif

	return 0;
}
EXPORT_SYMBOL(mtk_wcn_wmt_psm_ctrl);

MTK_WCN_BOOL mtk_wcn_wmt_func_off(ENUM_WMTDRV_TYPE_T type)
{
	MTK_WCN_BOOL ret;

	if (type == WMTDRV_TYPE_BT) {
		mtk_wcn_wmt_psm_ctrl(MTK_WCN_BOOL_TRUE);
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
return value:
enable/disable thermal sensor function: true(1)/false(0)
read thermal sensor function:thermal value

*/
VOID mtk_wcn_wmt_func_ctrl_for_plat(UINT32 on, ENUM_WMTDRV_TYPE_T type)
{
	if (on)
		mtk_wcn_wmt_func_on(type);
	else
		mtk_wcn_wmt_func_off(type);
}

INT8 mtk_wcn_wmt_therm_ctrl(ENUM_WMTTHERM_TYPE_T eType)
{
	P_OSAL_OP pOp;
	P_WMT_OP pOpData;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	/*parameter validation check */
	if (WMTTHERM_MAX < eType || WMTTHERM_ENABLE > eType) {
		WMT_ERR_FUNC("invalid thermal control command (%d)\n", eType);
		return MTK_WCN_BOOL_FALSE;
	}

	/*check if chip support thermal control function or not */
	bRet = wmt_lib_is_therm_ctrl_support();
	if (MTK_WCN_BOOL_FALSE == bRet) {
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

	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed,OPID(%d) type(%zu) abort!\n", pOp->op.opId, pOp->op.au4OpData[0]);
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	if (MTK_WCN_BOOL_FALSE == bRet) {
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

	if (WMTDSNS_MAX <= eType) {
		WMT_ERR_FUNC("invalid desense control command (%d)\n", eType);
		return MTK_WCN_BOOL_FALSE;
	}

	/*check if chip support thermal control function or not */
	bRet = wmt_lib_is_dsns_ctrl_support();
	if (MTK_WCN_BOOL_FALSE == bRet) {
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
	if ((WMTDSNS_FM_DISABLE <= eType) && (WMTDSNS_FM_GPS_ENABLE >= eType)) {
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

	if (MTK_WCN_BOOL_FALSE == bRet)
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

INT32 mtk_wcn_stp_wmt_sdio_host_awake(VOID)
{
	wmt_lib_ps_irq_cb();
	return 0;
}
EXPORT_SYMBOL(mtk_wcn_stp_wmt_sdio_host_awake);

MTK_WCN_BOOL mtk_wcn_wmt_assert_timeout(ENUM_WMTDRV_TYPE_T type, UINT32 reason, INT32 timeout)
{
	P_OSAL_OP pOp = NULL;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}
	wmt_lib_set_host_assert_info(type, reason, 1);

	pSignal = &pOp->signal;

	pOp->op.opId = WMT_OPID_TRIGGER_STP_ASSERT;
	pSignal->timeoutValue = timeout;
	/*this test command should be run with usb cable connected, so no host awake is needed */
	/* wmt_lib_host_awake_get(); */
	pOp->op.au4OpData[0] = 0;
	/*wake up chip first */
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return MTK_WCN_BOOL_FALSE;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	/* wmt_lib_host_awake_put(); */
	WMT_INFO_FUNC("STP_ASSERT, opid (%d), par(%zu, %zu), ret(%d), reason(%d), drv_type(%d), result(%s)\n",
		      pOp->op.opId,
		      pOp->op.au4OpData[0],
		      pOp->op.au4OpData[1],
		      bRet,
		      reason,
		      type, MTK_WCN_BOOL_FALSE == bRet ? "failed" : "succeed");
	/*If trigger stp assert succeed, just return; trigger WMT level assert if failed */
	if (MTK_WCN_BOOL_TRUE == bRet)
		return bRet;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}
	wmt_lib_set_host_assert_info(type, reason, 1);

	pSignal = &pOp->signal;

	pOp->op.opId = WMT_OPID_CMD_TEST;

	pSignal->timeoutValue = timeout;
	/*this test command should be run with usb cable connected, so no host awake is needed */
	/* wmt_lib_host_awake_get(); */
	pOp->op.au4OpData[0] = 0;

	/*wake up chip first */
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return MTK_WCN_BOOL_FALSE;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	/* wmt_lib_host_awake_put(); */
	WMT_INFO_FUNC("CMD_TEST, opid (%d), par(%zu, %zu), ret(%d), result(%s)\n",
		      pOp->op.opId,
		      pOp->op.au4OpData[0],
		      pOp->op.au4OpData[1],
		      bRet, MTK_WCN_BOOL_FALSE == bRet ? "failed" : "succeed");

	return bRet;
}
EXPORT_SYMBOL(mtk_wcn_wmt_assert_timeout);

MTK_WCN_BOOL mtk_wcn_wmt_assert(ENUM_WMTDRV_TYPE_T type, UINT32 reason)
{
	return mtk_wcn_wmt_assert_timeout(type, reason, MAX_EACH_WMT_CMD);
}
EXPORT_SYMBOL(mtk_wcn_wmt_assert);

/*
	ctrlId: get flash patch version opId or flash patch download opId
	pBuf: pointer to flash patch
	length: total length of flash patch
	type: flash patch type
	version: flash patch version
	checksum: flash patch checksum
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
	   pBuf must not be NULL, kernel space memory pointer
	   length must be large than 0 */
	switch (ctrlId) {
	case WMT_FLASH_PATCH_VERSION_GET:
		break;
	case WMT_FLASH_PATCH_DOWNLOAD:
		if ((NULL == pBuf) || (0 >= length) || (1000 < length)) {
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
	    (WMT_FLASH_PATCH_DOWNLOAD == ctrlId) ? MAX_FUNC_ON_TIME : MAX_EACH_WMT_CMD;

	pOp->op.opId = (WMT_FLASH_PATCH_DOWNLOAD == ctrlId) ?
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
	if (MTK_WCN_BOOL_FALSE == bRet) {
		eRet = WMT_FLASH_PATCH_OP_ERR;
	} else {
		switch (ctrlId) {
		case WMT_FLASH_PATCH_VERSION_GET:
			if (0 == pOp->op.au4OpData[6]) {
				*version = pOp->op.au4OpData[4];
				eRet = WMT_FLASH_PATCH_VERSION_GET_OK;
			} else
				eRet = WMT_FLASH_PATCH_VERSION_GET_FAIL;
			break;
		case WMT_FLASH_PATCH_DOWNLOAD:
			eRet = (0 == pOp->op.au4OpData[6]) ?
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
	ctrlId: get ram code status opId or ram code download opId
	pBuf: pointer to ANT ram code
	length: total length of ANT ram code
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
	   pBuf must not be NULL, kernel space memory pointer
	   length must be large than 0 */

	if ((WMT_ANT_RAM_GET_STATUS > ctrlId) || (WMT_ANT_RAM_CTRL_MAX <= ctrlId)) {
		WMT_ERR_FUNC("error ctrlId:%d detected.\n", ctrlId);
		eRet = WMT_ANT_RAM_PARA_ERR;
		return eRet;
	}

	if ((WMT_ANT_RAM_DOWNLOAD == ctrlId) &&
	    ((NULL == pBuf) ||
	     (0 >= length) ||
	     (1000 < length) || (seq >= WMT_ANT_RAM_SEQ_MAX) || (seq < WMT_ANT_RAM_START_PKT))) {
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
	    (WMT_ANT_RAM_DOWNLOAD == ctrlId) ? MAX_FUNC_ON_TIME : MAX_EACH_WMT_CMD;

	pOp->op.opId =
	    (WMT_ANT_RAM_DOWNLOAD == ctrlId) ? WMT_OPID_ANT_RAM_DOWN : WMT_OPID_ANT_RAM_STA_GET;
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
	if (MTK_WCN_BOOL_FALSE == bRet) {
		eRet = WMT_ANT_RAM_OP_ERR;
	} else {
		eRet = (WMT_ANT_RAM_DOWNLOAD == ctrlId) ?
		    WMT_ANT_RAM_DOWN_OK :
		    ((1 == pOp->op.au4OpData[2]) ? WMT_ANT_RAM_EXIST : WMT_ANT_RAM_NOT_EXIST);
	}

	return eRet;

}
EXPORT_SYMBOL(mtk_wcn_wmt_ant_ram_ctrl);
#endif
MTK_WCN_BOOL mtk_wcn_wmt_do_reset(ENUM_WMTDRV_TYPE_T type)
{
	INT32 iRet = -1;
	UINT8 *drv_name[] = {
		"DRV_TYPE_BT",
		"DRV_TYPE_FM",
		"DRV_TYPE_GPS",
		"DRV_TYPE_WIFI",
		"DRV_TYPE_WMT",
		"DRV_TYPE_ANT"
	};

	WMT_INFO_FUNC("Subsystem trigger whole chip reset, reset source: %s\n", drv_name[type]);
	iRet = wmt_lib_trigger_reset();
	return 0 == iRet ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE;
}
EXPORT_SYMBOL(mtk_wcn_wmt_do_reset);

VOID mtk_wcn_wmt_set_wifi_ver(UINT32 Value)
{
	wmt_lib_soc_set_wifiver(Value);
}
EXPORT_SYMBOL(mtk_wcn_wmt_set_wifi_ver);

