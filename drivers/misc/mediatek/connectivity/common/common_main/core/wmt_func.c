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
#define DFT_TAG         "[WMT-FUNC]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "osal_typedef.h"

#include "wmt_func.h"
#include "wmt_lib.h"
#include "wmt_core.h"
#include "wmt_exp.h"
#include "wmt_detect.h"


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/



/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
#if CFG_FUNC_BT_SUPPORT

static INT32 wmt_func_bt_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);
static INT32 wmt_func_bt_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);

WMT_FUNC_OPS wmt_func_bt_ops = {
	/* BT subsystem function on/off */
	.func_on = wmt_func_bt_on,
	.func_off = wmt_func_bt_off
};
#endif

#if CFG_FUNC_FM_SUPPORT

static INT32 wmt_func_fm_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);
static INT32 wmt_func_fm_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);

WMT_FUNC_OPS wmt_func_fm_ops = {
	/* FM subsystem function on/off */
	.func_on = wmt_func_fm_on,
	.func_off = wmt_func_fm_off
};
#endif

#if CFG_FUNC_GPS_SUPPORT

static INT32 wmt_func_gps_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);
static INT32 wmt_func_gps_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);

WMT_FUNC_OPS wmt_func_gps_ops = {
	/* GPS subsystem function on/off */
	.func_on = wmt_func_gps_on,
	.func_off = wmt_func_gps_off
};

#endif

#if CFG_FUNC_GPSL5_SUPPORT

static INT32 wmt_func_gpsl5_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);
static INT32 wmt_func_gpsl5_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);

WMT_FUNC_OPS wmt_func_gpsl5_ops = {
	/* GPS subsystem function on/off */
	.func_on = wmt_func_gpsl5_on,
	.func_off = wmt_func_gpsl5_off
};

#endif

#if CFG_FUNC_WIFI_SUPPORT
static INT32 wmt_func_wifi_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);
static INT32 wmt_func_wifi_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);

WMT_FUNC_OPS wmt_func_wifi_ops = {
	/* Wi-Fi subsystem function on/off */
	.func_on = wmt_func_wifi_on,
	.func_off = wmt_func_wifi_off
};
#endif


#if CFG_FUNC_ANT_SUPPORT

static INT32 wmt_func_ant_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);
static INT32 wmt_func_ant_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf);

WMT_FUNC_OPS wmt_func_ant_ops = {
	/* BT subsystem function on/off */
	.func_on = wmt_func_ant_on,
	.func_off = wmt_func_ant_off
};
#endif

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
#if CFG_FUNC_GPS_SUPPORT
CMB_PIN_CTRL_REG eediPinOhRegs[] = {
	{
	 /* pull down ctrl register */
	 .regAddr = 0x80050020,
	 .regValue = ~(0x1L << 5),
	 .regMask = 0x00000020L,
	 },
	{
	 /* pull up ctrl register */
	 .regAddr = 0x80050000,
	 .regValue = 0x1L << 5,
	 .regMask = 0x00000020L,
	 },
	{
	 /* iomode ctrl register */
	 .regAddr = 0x80050110,
	 .regValue = 0x1L << 0,
	 .regMask = 0x00000007L,
	 },
	{
	 /* output high/low ctrl register */
	 .regAddr = 0x80050040,
	 .regValue = 0x1L << 5,
	 .regMask = 0x00000020L,
	 }

};

CMB_PIN_CTRL_REG eediPinOlRegs[] = {
	{
	 .regAddr = 0x80050020,
	 .regValue = 0x1L << 5,
	 .regMask = 0x00000020L,
	 },
	{
	 .regAddr = 0x80050000,
	 .regValue = ~(0x1L << 5),
	 .regMask = 0x00000020L,
	 },
	{
	 .regAddr = 0x80050110,
	 .regValue = 0x1L << 0,
	 .regMask = 0x00000007L,
	 },
	{
	 .regAddr = 0x80050040,
	 .regValue = ~(0x1L << 5),
	 .regMask = 0x00000020L,
	 }
};

CMB_PIN_CTRL_REG eedoPinOhRegs[] = {
	{
	 .regAddr = 0x80050020,
	 .regValue = ~(0x1L << 7),
	 .regMask = 0x00000080L,
	 },
	{
	 .regAddr = 0x80050000,
	 .regValue = 0x1L << 7,
	 .regMask = 0x00000080L,
	 },
	{
	 .regAddr = 0x80050110,
	 .regValue = 0x1L << 12,
	 .regMask = 0x00007000L,
	 },
	{
	 .regAddr = 0x80050040,
	 .regValue = 0x1L << 7,
	 .regMask = 0x00000080L,
	 }
};


CMB_PIN_CTRL_REG eedoPinOlRegs[] = {
	{
	 .regAddr = 0x80050020,
	 .regValue = 0x1L << 7,
	 .regMask = 0x00000080L,
	 },
	{
	 .regAddr = 0x80050000,
	 .regValue = ~(0x1L << 7),
	 .regMask = 0x00000080L,
	 },
	{
	 .regAddr = 0x80050110,
	 .regValue = 0x1L << 12,
	 .regMask = 0x00007000L,
	 },
	{
	 .regAddr = 0x80050040,
	 .regValue = ~(0x1L << 7),
	 .regMask = 0x00000080L,
	 }

};

CMB_PIN_CTRL_REG gsyncPinOnRegs[] = {
	{
	 .regAddr = 0x80050110,
	 .regValue = 0x3L << 20,
	 .regMask = 0x7L << 20,
	 }

};

CMB_PIN_CTRL_REG gsyncPinOffRegs[] = {
	{
	 .regAddr = 0x80050110,
	 .regValue = 0x0L << 20,
	 .regMask = 0x7L << 20,
	 }
};

/* templete usage for GPIO control */
CMB_PIN_CTRL gCmbPinCtrl[3] = {
	{
	 .pinId = CMB_PIN_EEDI_ID,
	 .regNum = 4,
	 .pFuncOnArray = eediPinOhRegs,
	 .pFuncOffArray = eediPinOlRegs,
	 },
	{
	 .pinId = CMB_PIN_EEDO_ID,
	 .regNum = 4,
	 .pFuncOnArray = eedoPinOhRegs,
	 .pFuncOffArray = eedoPinOlRegs,
	 },
	{
	 .pinId = CMB_PIN_GSYNC_ID,
	 .regNum = 1,
	 .pFuncOnArray = gsyncPinOnRegs,
	 .pFuncOffArray = gsyncPinOffRegs,
	 }
};
#endif




/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#if CFG_FUNC_BT_SUPPORT

INT32 _osal_inline_ wmt_func_bt_ctrl(ENUM_FUNC_STATE funcState)
{
	/*only need to send turn BT subsystem wmt command */
	return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_BT,
				      (FUNC_ON ==
				       funcState) ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE);
}

INT32 wmt_func_bt_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	INT32 iRet = -1;
	ULONG ctrlPa1;
	ULONG ctrlPa2;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO)
		return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_BT, MTK_WCN_BOOL_TRUE);

	ctrlPa1 = BT_PALDO;
	ctrlPa2 = PALDO_ON;
	iRet = wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("wmt-func: wmt_ctrl_soc_paldo_ctrl failed(%d)(%lu)(%lu)\n",
				iRet, ctrlPa1, ctrlPa2);
		return -1;
	}
	iRet = wmt_core_func_ctrl_cmd(WMTDRV_TYPE_BT, MTK_WCN_BOOL_TRUE);
	if (iRet) {
		WMT_ERR_FUNC("wmt-func: wmt_core_func_ctrl_cmd(bt_on) failed(%d)\n", iRet);
		ctrlPa1 = BT_PALDO;
		ctrlPa2 = PALDO_OFF;
		wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);

		return -2;
	}
	osal_set_bit(WMT_BT_ON, &gBtWifiGpsState);
	return 0;
}

INT32 wmt_func_bt_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	INT32 iRet1 = -1;
	INT32 iRet2 = -1;
	ULONG ctrlPa1;
	ULONG ctrlPa2;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO)
		return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_BT, MTK_WCN_BOOL_FALSE);

	iRet1 = wmt_core_func_ctrl_cmd(WMTDRV_TYPE_BT, MTK_WCN_BOOL_FALSE);
	if (iRet1)
		WMT_ERR_FUNC("wmt-func: wmt_core_func_ctrl_cmd(bt_off) failed(%d)\n", iRet1);

	ctrlPa1 = BT_PALDO;
	ctrlPa2 = PALDO_OFF;
	iRet2 = wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
	if (iRet2)
		WMT_ERR_FUNC("wmt-func: wmt_ctrl_soc_paldo_ctrl(bt_off) failed(%d)\n", iRet2);

	if (iRet1 + iRet2)
		return -1;

	osal_clear_bit(WMT_BT_ON, &gBtWifiGpsState);
	return 0;
}

#endif
#if CFG_FUNC_ANT_SUPPORT

INT32 _osal_inline_ wmt_func_ant_ctrl(ENUM_FUNC_STATE funcState)
{
	/*only need to send turn BT subsystem wmt command */
	return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_ANT,
				      (FUNC_ON ==
				       funcState) ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE);
}

INT32 wmt_func_ant_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_ANT, MTK_WCN_BOOL_TRUE);
}

INT32 wmt_func_ant_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_ANT, MTK_WCN_BOOL_FALSE);
}

#endif

#if CFG_FUNC_GPS_SUPPORT

INT32 _osal_inline_ wmt_func_gps_ctrl(ENUM_FUNC_STATE funcState)
{
	/*send turn GPS subsystem wmt command */
	return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_GPS,
				      (FUNC_ON ==
				       funcState) ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE);
}

INT32 wmt_func_gps_pre_ctrl(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf, ENUM_FUNC_STATE funcStatus)
{
	UINT32 i = 0;
	UINT32 iRet = 0;
	UINT32 regAddr = 0;
	UINT32 regValue = 0;
	UINT32 regMask = 0;
	UINT32 regNum = 0;
	P_CMB_PIN_CTRL_REG pReg;
	P_CMB_PIN_CTRL pCmbPinCtrl = &gCmbPinCtrl[CMB_PIN_GSYNC_ID];
	WMT_CTRL_DATA ctrlData;
	WMT_IC_PIN_ID wmtIcPinId = WMT_IC_PIN_MAX;
	/* sanity check */
	if (FUNC_ON != funcStatus && FUNC_OFF != funcStatus) {
		WMT_ERR_FUNC("invalid funcStatus(%d)\n", funcStatus);
		return -1;
	}
	/* turn on GPS sync function on both side */
	ctrlData.ctrlId = WMT_CTRL_GPS_SYNC_SET;
	ctrlData.au4CtrlData[0] = (funcStatus == FUNC_ON) ? 1 : 0;
	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/*we suppose this would never print */
		WMT_ERR_FUNC("ctrl GPS_SYNC_SET(%d) fail, ret(%d)\n", funcStatus, iRet);
		/* TODO:[FixMe][George] error handling? */
		return -2;
	}
	WMT_DBG_FUNC("ctrl GPS_SYNC_SET(%d) ok\n", funcStatus);

	if ((pOps->ic_pin_ctrl == NULL) || (pOps->ic_pin_ctrl(WMT_IC_PIN_GSYNC, FUNC_ON ==
					funcStatus ? WMT_IC_PIN_MUX : WMT_IC_PIN_GPIO, 1) < 0)) {
		/*WMT_IC_PIN_GSYNC */
		pCmbPinCtrl = &gCmbPinCtrl[CMB_PIN_GSYNC_ID];
		regNum = pCmbPinCtrl->regNum;
		for (i = 0; i < regNum; i++) {
			pReg =
			    FUNC_ON ==
			    funcStatus ? &pCmbPinCtrl->
			    pFuncOnArray[i] : &pCmbPinCtrl->pFuncOffArray[i];
			regAddr = pReg->regAddr;
			regValue = pReg->regValue;
			regMask = pReg->regMask;

			iRet = wmt_core_reg_rw_raw(1, regAddr, &regValue, regMask);
			if (iRet) {
				WMT_ERR_FUNC("set reg for GPS_SYNC function fail(%d)\n", iRet);
				/* TODO:[FixMe][Chaozhong] error handling? */
				return -2;
			}

		}
	} else {
		WMT_DBG_FUNC("set reg for GPS_SYNC function okay by chip ic_pin_ctrl\n");
	}
	WMT_DBG_FUNC("ctrl combo chip gps sync function succeed\n");
	/* turn on GPS lna ctrl function */
	if (pConf != NULL) {
		if (pConf->wmt_gps_lna_enable == 0) {

			WMT_INFO_FUNC("host pin used for gps lna\n");
			/* host LNA ctrl pin needed */
			ctrlData.ctrlId = WMT_CTRL_GPS_LNA_SET;
			ctrlData.au4CtrlData[0] = funcStatus == FUNC_ON ? 1 : 0;
			iRet = wmt_ctrl(&ctrlData);
			if (iRet) {
				/*we suppose this would never print */
				WMT_ERR_FUNC("ctrl host GPS_LNA output high fail, ret(%d)\n", iRet);
				/* TODO:[FixMe][Chaozhong] error handling? */
				return -3;
			}
			WMT_INFO_FUNC("GPS LNA pin number(%d)\n", mtk_wmt_get_gps_lna_pin_num());
			WMT_DBG_FUNC("ctrl host gps lna function succeed\n");
		} else {
			WMT_INFO_FUNC("combo chip pin(%s) used for gps lna\n",
				      pConf->wmt_gps_lna_pin == 0 ? "EEDI" : "EEDO");
			wmtIcPinId =
			    pConf->wmt_gps_lna_pin == 0 ? WMT_IC_PIN_EEDI : WMT_IC_PIN_EEDO;
			if ((pOps->ic_pin_ctrl == NULL) || (pOps->ic_pin_ctrl(wmtIcPinId, FUNC_ON ==
					funcStatus ? WMT_IC_PIN_GPIO_HIGH : WMT_IC_PIN_GPIO_LOW, 1) < 0)) {
				/*WMT_IC_PIN_GSYNC */
				if (pConf->wmt_gps_lna_pin == 0) {
					/* EEDI needed */
					pCmbPinCtrl = &gCmbPinCtrl[CMB_PIN_EEDI_ID];
				} else if (pConf->wmt_gps_lna_pin == 1) {
					/* EEDO needed */
					pCmbPinCtrl = &gCmbPinCtrl[CMB_PIN_EEDO_ID];
				}
				regNum = pCmbPinCtrl->regNum;
				for (i = 0; i < regNum; i++) {
					pReg =
					    funcStatus == FUNC_ON ? &pCmbPinCtrl->pFuncOnArray[i] :
					    &pCmbPinCtrl->pFuncOffArray[i];
					regAddr = pReg->regAddr;
					regValue = pReg->regValue;
					regMask = pReg->regMask;

					iRet = wmt_core_reg_rw_raw(1, regAddr, &regValue, regMask);
					if (iRet) {
						WMT_ERR_FUNC
						    ("set reg for GPS_LNA function fail(%d)\n",
						     iRet);
						/* TODO:[FixMe][Chaozhong] error handling? */
						return -3;
					}
				}
				WMT_INFO_FUNC("ctrl combo chip gps lna succeed\n");
			} else {
				WMT_INFO_FUNC
				    ("set reg for GPS_LNA function okay by chip ic_pin_ctrl\n");
			}
		}
	}
	return 0;

}

INT32 wmt_func_gps_pre_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	return wmt_func_gps_pre_ctrl(pOps, pConf, FUNC_ON);
}

INT32 wmt_func_gps_pre_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{

	return wmt_func_gps_pre_ctrl(pOps, pConf, FUNC_OFF);
}


INT32 wmt_func_gps_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	INT32 iRet = 0;
	unsigned long ctrlPa1;
	unsigned long ctrlPa2;
	UINT8 co_clock_type = 0;

	if (!pConf)
		return -1;

	co_clock_type = (pConf->co_clock_flag & 0x0f);

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		if ((co_clock_type) && (pConf->wmt_gps_lna_enable == 0)) {	/* use SOC external LNA */
			if (!osal_test_bit(WMT_FM_ON, &gGpsFmState) && !osal_test_bit(WMT_GPSL5_ON, &gGpsFmState)) {
				ctrlPa1 = GPS_PALDO;
				ctrlPa2 = PALDO_ON;
				wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
			} else {
				WMT_INFO_FUNC("LDO VCN28 has been turn on by FM or GPSL5\n");
			}
		}
	}
	if (!osal_test_bit(WMT_GPSL5_ON, &gGpsFmState))
		iRet = wmt_func_gps_pre_on(pOps, pConf);
	else
		WMT_INFO_FUNC("gps has been pre on by GPSL5\n");
	if (iRet == 0) {
		if (!pConf || pConf->wmt_gps_suspend_ctrl == 0)
			iRet = wmt_func_gps_ctrl(FUNC_ON);
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
			if (!iRet) {
				osal_set_bit(WMT_GPS_ON, &gBtWifiGpsState);
				if ((co_clock_type) && (pConf->wmt_gps_lna_enable == 0)) /* use SOC external LNA */
					osal_set_bit(WMT_GPS_ON, &gGpsFmState);
				osal_clear_bit(WMT_GPS_SUSPEND, &gGpsFmState);
			}
		}
	}
	return iRet;
}

INT32 wmt_func_gps_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	INT32 iRet = 0;
	unsigned long ctrlPa1 = 0;
	unsigned long ctrlPa2 = 0;
	UINT8 co_clock_type = 0;

	if (!pConf)
		return -1;

	co_clock_type = (pConf->co_clock_flag & 0x0f);

	if (osal_test_bit(WMT_GPSL5_ON, &gGpsFmState))
		WMT_INFO_FUNC("GPSL5 is still on, do not pre off gps\n");
	else if (!osal_test_bit(WMT_GPS_SUSPEND, &gGpsFmState))
		iRet = wmt_func_gps_pre_off(pOps, pConf);
	if (iRet == 0) {
		if (!pConf || pConf->wmt_gps_suspend_ctrl == 0)
			iRet = wmt_func_gps_ctrl(FUNC_OFF);
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
			if (!iRet)
				osal_clear_bit(WMT_GPS_ON, &gBtWifiGpsState);
		}
	}
	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		if ((co_clock_type) && (pConf->wmt_gps_lna_enable == 0)) {	/* use SOC external LNA */
			if (osal_test_bit(WMT_FM_ON, &gGpsFmState) || osal_test_bit(WMT_GPSL5_ON, &gGpsFmState))
				WMT_INFO_FUNC("FM or GPSL5 is still on, do not turn off LDO VCN28\n");
			else if (osal_test_bit(WMT_GPS_SUSPEND, &gGpsFmState))
				WMT_INFO_FUNC("It's GPS suspend mode, LDO VCN28 has been turned off\n");
			else {
				ctrlPa1 = GPS_PALDO;
				ctrlPa2 = PALDO_OFF;
				wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
			}
			osal_clear_bit(WMT_GPS_ON, &gGpsFmState);
		}
		if (pConf->wmt_gps_suspend_ctrl == 1)
			osal_set_bit(WMT_GPS_SUSPEND, &gGpsFmState);
	}
	return iRet;

}
#endif

#if CFG_FUNC_GPSL5_SUPPORT
INT32 _osal_inline_ wmt_func_gpsl5_ctrl(ENUM_FUNC_STATE funcState)
{
	/*send turn GPS subsystem wmt command */
	return wmt_core_func_ctrl_cmd(6,
				      (FUNC_ON ==
				       funcState) ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE);
}

INT32 wmt_func_gpsl5_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	INT32 iRet = 0;
	unsigned long ctrlPa1;
	unsigned long ctrlPa2;
	UINT8 co_clock_type = 0;

	if (!pConf) {
		WMT_INFO_FUNC("pConf == NULL\n");
		return -1;
	}

	co_clock_type = (pConf->co_clock_flag & 0x0f);

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		if ((co_clock_type) && (pConf->wmt_gps_lna_enable == 0)) {	/* use SOC external LNA */
			if (!osal_test_bit(WMT_FM_ON, &gGpsFmState) &&
					!osal_test_bit(WMT_GPS_ON, &gGpsFmState)) {
				ctrlPa1 = GPS_PALDO;
				ctrlPa2 = PALDO_ON;
				wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
			} else {
				WMT_INFO_FUNC("LDO VCN28 has been turn on by FM or GPSL1\n");
			}
		}
	}
	if (!osal_test_bit(WMT_GPS_ON, &gGpsFmState))
		iRet = wmt_func_gps_pre_on(pOps, pConf);
	else
		WMT_INFO_FUNC("gps has been pre on by GPSL1\n");
	if (iRet == 0) {
		if (pConf->wmt_gps_suspend_ctrl == 0)
			iRet = wmt_func_gpsl5_ctrl(FUNC_ON);
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
			if (!iRet) {
				osal_set_bit(WMT_GPSL5_ON, &gBtWifiGpsState);
				if ((co_clock_type) && (pConf->wmt_gps_lna_enable == 0)) /* use SOC external LNA */
					osal_set_bit(WMT_GPSL5_ON, &gGpsFmState);
				osal_clear_bit(WMT_GPSL5_SUSPEND, &gGpsFmState);
			}
		}
	}
	return iRet;
}

INT32 wmt_func_gpsl5_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	INT32 iRet = 0;
	unsigned long ctrlPa1 = 0;
	unsigned long ctrlPa2 = 0;
	UINT8 co_clock_type = 0;

	if (!pConf) {
		WMT_INFO_FUNC("pConf == NULL\n");
		return -1;
	}

	co_clock_type = (pConf->co_clock_flag & 0x0f);

	if (osal_test_bit(WMT_GPS_ON, &gGpsFmState))
		WMT_INFO_FUNC("GPSL1 is still on, do not pre off gps\n");
	else if (!osal_test_bit(WMT_GPSL5_SUSPEND, &gGpsFmState))
		iRet = wmt_func_gps_pre_off(pOps, pConf);
	if (iRet == 0) {
		if (pConf->wmt_gps_suspend_ctrl == 0)
			iRet = wmt_func_gpsl5_ctrl(FUNC_OFF);
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
			if (!iRet)
				osal_clear_bit(WMT_GPSL5_ON, &gBtWifiGpsState);
		}
	}
	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		if ((co_clock_type) && (pConf->wmt_gps_lna_enable == 0)) {	/* use SOC external LNA */
			if (osal_test_bit(WMT_FM_ON, &gGpsFmState) || osal_test_bit(WMT_GPS_ON, &gGpsFmState))
				WMT_INFO_FUNC("FM or GPSL1 is still on, do not turn off LDO VCN28\n");
			else if (osal_test_bit(WMT_GPSL5_SUSPEND, &gGpsFmState))
				WMT_INFO_FUNC("It's GPS suspend mode, LDO VCN28 has been turned off\n");
			else {
				ctrlPa1 = GPS_PALDO;
				ctrlPa2 = PALDO_OFF;
				wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
			}
			osal_clear_bit(WMT_GPSL5_ON, &gGpsFmState);
		}
		if (pConf->wmt_gps_suspend_ctrl == 1)
			osal_set_bit(WMT_GPSL5_SUSPEND, &gGpsFmState);
	}
	return iRet;

}
#endif

#if CFG_FUNC_FM_SUPPORT

INT32 _osal_inline_ wmt_func_fm_ctrl(ENUM_FUNC_STATE funcState)
{
	/*only need to send turn FM subsystem wmt command */
	return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_FM,
				      (FUNC_ON ==
				       funcState) ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE);
}


INT32 wmt_func_fm_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	ULONG ctrlPa1 = 0;
	ULONG ctrlPa2 = 0;
	INT32 iRet = -1;
	UINT8 co_clock_type = (pConf->co_clock_flag & 0x0f);

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		if (co_clock_type) {
			if (!osal_test_bit(WMT_GPS_ON, &gGpsFmState) && !osal_test_bit(WMT_GPSL5_ON, &gGpsFmState)) {
				ctrlPa1 = FM_PALDO;
				ctrlPa2 = PALDO_ON;
				wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
			} else {
				WMT_INFO_FUNC("LDO VCN28 has been turn on by GPS\n");
			}
		} else
			WMT_ERR_FUNC("wmt-func:  co_clock_type is not 1!\n");
		iRet = wmt_core_func_ctrl_cmd(WMTDRV_TYPE_FM, MTK_WCN_BOOL_TRUE);
		if (!iRet) {
			if (co_clock_type)
				osal_set_bit(WMT_FM_ON, &gGpsFmState);
		}
		return iRet;
	}
	return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_FM, MTK_WCN_BOOL_TRUE);
}

INT32 wmt_func_fm_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	ULONG ctrlPa1 = 0;
	ULONG ctrlPa2 = 0;
	INT32 iRet = -1;
	UINT8 co_clock_type = (pConf->co_clock_flag & 0x0f);

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		iRet = wmt_core_func_ctrl_cmd(WMTDRV_TYPE_FM, MTK_WCN_BOOL_FALSE);
		if (co_clock_type) {
			if (osal_test_bit(WMT_GPS_ON, &gGpsFmState) || osal_test_bit(WMT_GPSL5_ON, &gGpsFmState)) {
				WMT_INFO_FUNC("GPS is still on, do not turn off LDO VCN28\n");
			} else {
				ctrlPa1 = FM_PALDO;
				ctrlPa2 = PALDO_OFF;
				wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
			}
			osal_clear_bit(WMT_FM_ON, &gGpsFmState);
		}
		return iRet;
	}
	return wmt_core_func_ctrl_cmd(WMTDRV_TYPE_FM, MTK_WCN_BOOL_FALSE);
}

#endif

#if CFG_FUNC_WIFI_SUPPORT

INT32 wmt_func_wifi_ctrl(ENUM_FUNC_STATE funcState)
{
	INT32 iRet = 0;
	ULONG ctrlPa1 = WMT_SDIO_FUNC_WIFI;
	ULONG ctrlPa2 = (funcState == FUNC_ON) ? 1 : 0;	/* turn on Wi-Fi driver */

	iRet = wmt_core_ctrl(WMT_CTRL_SDIO_FUNC, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-FUNC: turn on WIFI function fail (%d)", iRet);
		return -1;
	}
	return 0;
}


INT32 wmt_func_wifi_on(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	INT32 iRet = 0;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO)
		return wmt_func_wifi_ctrl(FUNC_ON);

	if (mtk_wcn_wlan_probe != NULL) {
		WMT_WARN_FUNC("WMT-FUNC: wmt wlan func on before wlan probe\n");
		iRet = (*mtk_wcn_wlan_probe) ();
		if (iRet) {
			WMT_ERR_FUNC("WMT-FUNC: wmt call wlan probe fail(%d)\n", iRet);
			iRet = -1;
		} else {
			WMT_WARN_FUNC("WMT-FUNC: wmt call wlan probe ok\n");
		}
	} else {
		WMT_ERR_FUNC("WMT-FUNC: null pointer mtk_wcn_wlan_probe\n");
		gWifiProbed = 1;
		iRet = -2;
	}
	if (!iRet)
		osal_set_bit(WMT_WIFI_ON, &gBtWifiGpsState);
	return iRet;
}

INT32 wmt_func_wifi_off(P_WMT_IC_OPS pOps, P_WMT_GEN_CONF pConf)
{
	INT32 iRet = 0;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO)
		return wmt_func_wifi_ctrl(FUNC_OFF);

	if (mtk_wcn_wlan_remove != NULL) {
		WMT_WARN_FUNC("WMT-FUNC: wmt wlan func on before wlan remove\n");
		iRet = (*mtk_wcn_wlan_remove) ();
		if (iRet) {
			WMT_ERR_FUNC("WMT-FUNC: wmt call wlan remove fail(%d)\n", iRet);
			iRet = -1;
		} else {
			WMT_WARN_FUNC("WMT-FUNC: wmt call wlan remove ok\n");
		}
	} else {
		WMT_ERR_FUNC("WMT-FUNC: null pointer mtk_wcn_wlan_remove\n");
		iRet = -2;
	}
	if (!iRet)
		osal_clear_bit(WMT_WIFI_ON, &gBtWifiGpsState);
	return iRet;
}
#endif
