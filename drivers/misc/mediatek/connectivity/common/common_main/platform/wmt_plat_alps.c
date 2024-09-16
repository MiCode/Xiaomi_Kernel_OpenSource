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
#define DFT_TAG         "[WMT-PLAT]"

#include <linux/version.h>

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/delay.h>

/* ALPS header files */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
#ifndef CONFIG_RTC_DRV_MT6397
#include <mtk_rtc.h>
#else
#include <linux/mfd/mt6397/rtc_misc.h>
#endif
#endif

#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
#include <mtk_6306_gpio.h>
#endif
/* ALPS and COMBO header files */
#include <mtk_wcn_cmb_stub.h>
/* MTK_WCN_COMBO header files */
#include "wmt_plat.h"
#include "wmt_dev.h"
#include "wmt_lib.h"
#include "mtk_wcn_cmb_hw.h"
#include "mtk_wcn_consys_hw.h"
#include "stp_dbg.h"
#include "osal.h"
#include "wmt_gpio.h"
#include "wmt_detect.h"
#include <connectivity_build_in_adapter.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>

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
#if CFG_WMT_PS_SUPPORT
static VOID wmt_plat_bgf_eirq_cb(VOID);
#endif
static INT32 wmt_plat_ldo_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_pmu_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_rtc_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_rst_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_bgf_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_wifi_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_all_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_uart_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_pcm_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_i2s_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_sdio_pin_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_gps_sync_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_gps_lna_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_uart_rx_ctrl(ENUM_PIN_STATE state);
#if CFG_WMT_LTE_COEX_HANDLING
static INT32 wmt_plat_tdm_req_ctrl(ENUM_PIN_STATE state);
#endif
static INT32 wmt_plat_dump_pin_conf(VOID);


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

INT32 gWmtMergeIfSupport;
UINT32 gCoClockFlag;
BGF_IRQ_BALANCE g_bgf_irq_lock;
INT32 wmtPlatLogLvl = WMT_PLAT_LOG_INFO;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

static ENUM_STP_TX_IF_TYPE gCommIfType = STP_MAX_IF_TX;
static OSAL_SLEEPABLE_LOCK gOsSLock;
static OSAL_WAKE_LOCK wmt_wake_lock;

irq_cb wmt_plat_bgf_irq_cb;
device_audio_if_cb wmt_plat_audio_if_cb;
func_ctrl_cb wmt_plat_func_ctrl_cb;
thermal_query_ctrl_cb wmt_plat_thermal_query_ctrl_cb;
trigger_assert_cb wmt_plat_trigger_assert_cb;
deep_idle_ctrl_cb wmt_plat_deep_idle_ctrl_cb;

static const fp_set_pin gfp_set_pin_table[] = {
	[PIN_LDO] = wmt_plat_ldo_ctrl,
	[PIN_PMU] = wmt_plat_pmu_ctrl,
	[PIN_RTC] = wmt_plat_rtc_ctrl,
	[PIN_RST] = wmt_plat_rst_ctrl,
	[PIN_BGF_EINT] = wmt_plat_bgf_eint_ctrl,
	[PIN_WIFI_EINT] = wmt_plat_wifi_eint_ctrl,
	[PIN_ALL_EINT] = wmt_plat_all_eint_ctrl,
	[PIN_UART_GRP] = wmt_plat_uart_ctrl,
	[PIN_PCM_GRP] = wmt_plat_pcm_ctrl,
	[PIN_I2S_GRP] = wmt_plat_i2s_ctrl,
	[PIN_SDIO_GRP] = wmt_plat_sdio_pin_ctrl,
	[PIN_GPS_SYNC] = wmt_plat_gps_sync_ctrl,
	[PIN_GPS_LNA] = wmt_plat_gps_lna_ctrl,
	[PIN_UART_RX] = wmt_plat_uart_rx_ctrl,
#if CFG_WMT_LTE_COEX_HANDLING
	[PIN_TDM_REQ] = wmt_plat_tdm_req_ctrl,
#endif
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*!
 * \brief audio control callback function for CMB_STUB on ALPS
 *
 * A platform function required for dynamic binding with CMB_STUB on ALPS.
 *
 * \param state desired audio interface state to use
 * \param flag audio interface control options
 *
 * \retval 0 operation success
 * \retval -1 invalid parameters
 * \retval < 0 error for operation fail
 */
INT32 wmt_plat_audio_ctrl(enum CMB_STUB_AIF_X state, enum CMB_STUB_AIF_CTRL ctrl)
{
	INT32 iRet = 0;
	UINT32 mergeIfSupport = 0;

	/* input sanity check */
	if ((state >= CMB_STUB_AIF_MAX) || (ctrl >= CMB_STUB_AIF_CTRL_MAX))
		return -1;

	iRet = 0;

	/* set host side first */
	switch (state) {
	case CMB_STUB_AIF_0:
		/* BT_PCM_OFF & FM line in/out */
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_DEINIT);
		break;

	case CMB_STUB_AIF_1:
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_INIT);
		break;

	case CMB_STUB_AIF_2:
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_INIT);
		break;

	case CMB_STUB_AIF_3:
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_INIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_INIT);
		break;

	default:
		/* FIXME: move to cust folder? */
		WMT_ERR_FUNC("invalid state [%d]\n", state);
		return -1;
	}

	if (wmt_plat_merge_if_flag_get() != 0) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		WMT_DBG_FUNC("[MT6628]<Merge IF> no need to ctrl combo chip side GPIO\n");
#else
		mergeIfSupport = 1;
#endif
	} else
		mergeIfSupport = 1;

	if (mergeIfSupport != 0) {
		if (ctrl == CMB_STUB_AIF_CTRL_EN) {
			WMT_INFO_FUNC("call chip aif setting\n");
			/* need to control chip side GPIO */
			if (wmt_plat_audio_if_cb != NULL)
				iRet += (*wmt_plat_audio_if_cb)(state, MTK_WCN_BOOL_FALSE);
			else {
				WMT_WARN_FUNC("wmt_plat_audio_if_cb is not registered\n");
				iRet -= 1;
			}


		} else
			WMT_INFO_FUNC("skip chip aif setting\n");
	}

	return iRet;
}

static VOID wmt_plat_func_ctrl(UINT32 type, UINT32 on)
{
	if (wmt_plat_func_ctrl_cb)
		(*wmt_plat_func_ctrl_cb)(on, type);
}

static long wmt_plat_thermal_ctrl(VOID)
{
	long temp = 0;

	if (wmt_plat_thermal_query_ctrl_cb)
		temp = (*wmt_plat_thermal_query_ctrl_cb)();

	return temp;
}

static INT32 wmt_plat_assert_ctrl(VOID)
{
	INT32 ret = 0;

	if (wmt_plat_trigger_assert_cb)
		ret = (*wmt_plat_trigger_assert_cb)(WMTDRV_TYPE_WMT, 45);

	return ret;
}

static INT32 wmt_plat_deep_idle_ctrl(UINT32 dpilde_ctrl)
{
	INT32 iRet = -1;

	if (wmt_plat_deep_idle_ctrl_cb)
		iRet = (*wmt_plat_deep_idle_ctrl_cb)(dpilde_ctrl);

	return iRet;
}

static VOID wmt_plat_clock_fail_dump(VOID)
{
	mtk_wcn_consys_clock_fail_dump();
}

#if CFG_WMT_PS_SUPPORT
static VOID wmt_plat_bgf_eirq_cb(VOID)
{
/* #error "need to disable EINT here" */
	/* wmt_lib_ps_irq_cb(); */
	if (wmt_plat_bgf_irq_cb != NULL)
		(*(wmt_plat_bgf_irq_cb))();
	else
		WMT_PLAT_PR_WARN("WMT-PLAT: wmt_plat_bgf_irq_cb not registered\n");
}
#endif

irqreturn_t wmt_plat_bgf_irq_isr(INT32 irq, PVOID arg)
{
#if CFG_WMT_PS_SUPPORT
	wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
	wmt_plat_bgf_eirq_cb();
#else
	WMT_PLAT_PR_INFO("skip irq handing because psm is disable");
#endif

	return IRQ_HANDLED;
}

VOID wmt_plat_irq_cb_reg(irq_cb bgf_irq_cb)
{
	wmt_plat_bgf_irq_cb = bgf_irq_cb;
}

VOID wmt_plat_aif_cb_reg(device_audio_if_cb aif_ctrl_cb)
{
	wmt_plat_audio_if_cb = aif_ctrl_cb;
}

VOID wmt_plat_func_ctrl_cb_reg(func_ctrl_cb subsys_func_ctrl)
{
	wmt_plat_func_ctrl_cb = subsys_func_ctrl;
}

VOID wmt_plat_thermal_ctrl_cb_reg(thermal_query_ctrl_cb thermal_query_ctrl)
{
	wmt_plat_thermal_query_ctrl_cb = thermal_query_ctrl;
}

VOID wmt_plat_trigger_assert_cb_reg(trigger_assert_cb trigger_assert)
{
	wmt_plat_trigger_assert_cb = trigger_assert;
}

VOID wmt_plat_deep_idle_ctrl_cb_reg(deep_idle_ctrl_cb deep_idle_ctrl)
{
	wmt_plat_deep_idle_ctrl_cb = deep_idle_ctrl;
}

UINT32 wmt_plat_soc_co_clock_flag_get(VOID)
{
	return gCoClockFlag;
}

static UINT32 wmt_plat_soc_co_clock_flag_set(UINT32 flag)
{
	gCoClockFlag = flag;
	return 0;
}

INT32 wmt_plat_init(P_PWR_SEQ_TIME pPwrSeqTime, UINT32 co_clock_type)
{
	struct _CMB_STUB_CB_ stub_cb;
	INT32 iret = -1;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		iret = mtk_wcn_consys_co_clock_type();
		if ((co_clock_type == 0) && (iret >= 0))
			co_clock_type = iret;
		wmt_plat_soc_co_clock_flag_set(co_clock_type);
	}

	stub_cb.aif_ctrl_cb = wmt_plat_audio_ctrl;
	stub_cb.func_ctrl_cb = wmt_plat_func_ctrl;
	stub_cb.thermal_query_cb = wmt_plat_thermal_ctrl;
	stub_cb.trigger_assert_cb = wmt_plat_assert_ctrl;
	stub_cb.deep_idle_ctrl_cb = wmt_plat_deep_idle_ctrl;
	stub_cb.wmt_do_reset_cb = NULL;
	stub_cb.clock_fail_dump_cb = wmt_plat_clock_fail_dump;
	stub_cb.size = sizeof(stub_cb);

	/* register to cmb_stub */
	iret = mtk_wcn_cmb_stub_reg(&stub_cb);

	/*init wmt function ctrl wakelock if wake lock is supported by host platform */
	osal_strcpy(wmt_wake_lock.name, "wmtFuncCtrl");
	wmt_wake_lock.init_flag = 0;
	osal_wake_lock_init(&wmt_wake_lock);
	osal_sleepable_lock_init(&gOsSLock);

	/* init hw */
	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
		iret += mtk_wcn_consys_hw_init();
	else
		iret += mtk_wcn_cmb_hw_init(pPwrSeqTime);

	spin_lock_init(&g_bgf_irq_lock.lock);

	mtk_wcn_consys_detect_adie_chipid(co_clock_type);

	WMT_DBG_FUNC("WMT-PLAT: ALPS platform init (%d)\n", iret);

	return 0;
}

INT32 wmt_plat_deinit(VOID)
{
	INT32 iret = 0;

	/* 1. de-init hw */
	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
		iret += mtk_wcn_consys_hw_deinit();
	else
		iret = mtk_wcn_cmb_hw_deinit();
	/* 2. unreg to cmb_stub */
	iret += mtk_wcn_cmb_stub_unreg();
	/*3. wmt wakelock deinit */
	osal_wake_lock_deinit(&wmt_wake_lock);
	osal_sleepable_lock_deinit(&gOsSLock);
	WMT_DBG_FUNC("destroy wmt_wake_lock\n");
	WMT_DBG_FUNC("WMT-PLAT: ALPS platform init (%d)\n", iret);

	return 0;
}

INT32 wmt_plat_sdio_ctrl(WMT_SDIO_SLOT_NUM sdioPortType, ENUM_FUNC_STATE on)
{
	return board_sdio_ctrl(sdioPortType, (on == FUNC_OFF) ? 0 : 1);
}

INT32 wmt_plat_irq_ctrl(ENUM_FUNC_STATE state)
{
	return -1;
}

static INT32 wmt_plat_dump_pin_conf(VOID)
{
	WMT_DBG_FUNC("[WMT-PLAT]=>dump wmt pin configuration start<=\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("LDO(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("LDO(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("PMU(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PMU(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("PMUV28(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PMUV28(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("RST(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("RST(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("BGF_EINT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("BGF_EINT(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("BGF_EINT_NUM(%d)\n",
				gpio_to_irq(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num));
	} else
		WMT_DBG_FUNC("BGF_EINT_NUM(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WIFI_EINT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("WIFI_EINT(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WIFI_EINT_NUM(%d)\n",
				gpio_to_irq(gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num));
	} else
		WMT_DBG_FUNC("WIFI_EINT_NUM(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("UART_RX(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("UART_RX(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("UART_TX(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("UART_TX(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("DAICLK(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("DAICLK(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("PCMOUT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PCMOUT(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("PCMIN(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PCMIN(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("PCMSYNC(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PCMSYNC(not defined)\n");
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("I2S_CK(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("I2S_CK(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("I2S_WS(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("I2S_WS(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("I2S_DAT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("I2S_DAT(not defined)\n");

#else				/* FM_ANALOG_INPUT || FM_ANALOG_OUTPUT */
	WMT_DBG_FUNC("FM digital mode is not set, no need for I2S GPIOs\n");
#endif

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("GPS_SYNC(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("GPS_SYNC(not defined)\n");

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_num != DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("GPS_LNA(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("GPS_LNA(not defined)\n");

	WMT_DBG_FUNC("[WMT-PLAT]=>dump wmt pin configuration emds<=\n");

	return 0;
}

INT32 wmt_plat_pwr_ctrl(ENUM_FUNC_STATE state)
{
	INT32 ret = -1;

	switch (state) {
	case FUNC_ON:
		/* TODO:[ChangeFeature][George] always output this or by request throuth /proc or sysfs? */
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
			ret = mtk_wcn_consys_hw_pwr_on(gCoClockFlag);
		else {
			wmt_plat_dump_pin_conf();
			ret = mtk_wcn_cmb_hw_pwr_on();
		}
		break;

	case FUNC_OFF:
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
			ret = mtk_wcn_consys_hw_pwr_off(gCoClockFlag);
		else
			ret = mtk_wcn_cmb_hw_pwr_off();
		break;

	case FUNC_RST:
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
			ret = mtk_wcn_consys_hw_rst(gCoClockFlag);
		else
			ret = mtk_wcn_cmb_hw_rst();
		break;
	case FUNC_STAT:
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
			ret = mtk_wcn_consys_hw_state_show();
		else
			ret = mtk_wcn_cmb_hw_state_show();
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) in pwr_ctrl\n", state);
		break;
	}

	return ret;
}

INT32 wmt_plat_ps_ctrl(ENUM_FUNC_STATE state)
{
	return -1;
}

INT32 wmt_plat_eirq_ctrl(ENUM_PIN_ID id, ENUM_PIN_STATE state)
{
	INT32 iret;
	static UINT32 bgf_irq_num = -1;
	static UINT32 bgf_irq_flag;

	/* TODO: [ChangeFeature][GeorgeKuo]: use another function to handle this, as done in gpio_ctrls */

	if ((state != PIN_STA_INIT) && (state != PIN_STA_DEINIT) && (state != PIN_STA_EINT_EN)
			&& (state != PIN_STA_EINT_DIS)) {
		WMT_WARN_FUNC("WMT-PLAT:invalid PIN_STATE(%d) in eirq_ctrl for PIN(%d)\n", state, id);
		return -1;
	}

	iret = -2;
	switch (id) {
	case PIN_BGF_EINT:
		if (state == PIN_STA_INIT) {
			if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
#ifdef CONFIG_OF
				iret = mtk_wcn_consys_read_irq_info_from_dts(&bgf_irq_num, &bgf_irq_flag);
				if (iret)
					return iret;
#else
				bgf_irq_num = MT_CONN2AP_BTIF_WAKEUP_IRQ_ID;
				bgf_irq_flag = IRQF_TRIGGER_LOW;
#endif
				iret = request_irq(bgf_irq_num, wmt_plat_bgf_irq_isr, bgf_irq_flag,
						   "BTIF_WAKEUP_IRQ", NULL);
				if (iret) {
					WMT_PLAT_PR_ERR("request_irq fail,irq_no(%d),iret(%d)\n",
							  bgf_irq_num, iret);
					return iret;
				} else {
					iret = enable_irq_wake(bgf_irq_num);
					if (iret)
						WMT_PLAT_PR_ERR("enable irq wake fail,irq_no(%d),iret(%d)\n",
							bgf_irq_num, iret);
					iret = 0;
				}
			} else {
				struct device_node *node;
				INT32 ret = -EINVAL;

				node = of_find_compatible_node(NULL, NULL, "mediatek,connectivity-combo");
				if (node) {
					/*BGF-eint name maybe wrong*/
					bgf_irq_num = irq_of_parse_and_map(node, 1);
					ret = request_irq(bgf_irq_num, wmt_plat_bgf_irq_isr,
							  IRQF_TRIGGER_LOW, "BGF-eint", NULL);
					if (ret)
						WMT_ERR_FUNC("BGF EINT IRQ LINE NOT AVAILABLE!!\n");
					else
						WMT_INFO_FUNC("BGF EINT request_irq success!!\n");
				} else
					WMT_ERR_FUNC("[%s] can't find BGF eint compatible node\n",
						     __func__);
			}
			g_bgf_irq_lock.counter = 1;
		} else if (state == PIN_STA_EINT_EN) {
			spin_lock_irqsave(&g_bgf_irq_lock.lock, g_bgf_irq_lock.flags);
			if (g_bgf_irq_lock.counter) {
				WMT_PLAT_PR_DBG("BGF INT has been enabled,counter(%d)\n",
						  g_bgf_irq_lock.counter);
			} else {
				enable_irq(bgf_irq_num);
				g_bgf_irq_lock.counter++;
				WMT_DBG_FUNC("WMT-PLAT:BGFInt (en)\n");
			}
			spin_unlock_irqrestore(&g_bgf_irq_lock.lock, g_bgf_irq_lock.flags);
		} else if (state == PIN_STA_EINT_DIS) {
			spin_lock_irqsave(&g_bgf_irq_lock.lock, g_bgf_irq_lock.flags);
			if (!g_bgf_irq_lock.counter) {
				WMT_PLAT_PR_DBG("BGF INT has been disabled,counter(%d)\n",
						  g_bgf_irq_lock.counter);
			} else {
				disable_irq_nosync(bgf_irq_num);
				g_bgf_irq_lock.counter--;
				WMT_DBG_FUNC("WMT-PLAT:BGFInt (dis)\n");
			}
			spin_unlock_irqrestore(&g_bgf_irq_lock.lock, g_bgf_irq_lock.flags);
		} else {
			free_irq(bgf_irq_num, NULL);
			WMT_DBG_FUNC("WMT-PLAT:BGFInt (free)\n");
			/* de-init: nothing to do in ALPS, such as un-registration... */
		}

		iret = 0;
		break;
	case PIN_ALL_EINT:
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num != DEFAULT_PIN_ID) {
			if (state == PIN_STA_INIT) {
				disable_irq_nosync(gpio_to_irq(gpio_ctrl_info.
							gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num));
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (INIT but not used yet)\n");
			} else if (state == PIN_STA_EINT_EN) {
				enable_irq(gpio_to_irq(gpio_ctrl_info.
							gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num));
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (EN but not used yet)\n");
			} else if (state == PIN_STA_EINT_DIS) {
				disable_irq_nosync(gpio_to_irq(gpio_ctrl_info.
							gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num));
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (DIS but not used yet)\n");
			} else {
				disable_irq_nosync(gpio_to_irq(gpio_ctrl_info.
							gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num));
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (DEINIT but not used yet)\n");
				/* de-init: nothing to do in ALPS, such as un-registration... */
			}
		} else
			WMT_DBG_FUNC("WMT-PLAT:ALL EINT not defined\n");

		iret = 0;
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:unsupported EIRQ(PIN_ID:%d) in eirq_ctrl\n", id);
		iret = -1;
		break;
	}

	return iret;
}

INT32 wmt_plat_gpio_ctrl(ENUM_PIN_ID id, ENUM_PIN_STATE state)
{
	INT32 iret = -1;

	if ((id >= 0) && (id < PIN_ID_MAX) && (state < PIN_STA_MAX)) {
		/* TODO: [FixMe][GeorgeKuo] do sanity check to const function table when init and skip checking here */
		if (gfp_set_pin_table[id])
			iret = (*(gfp_set_pin_table[id]))(state);	/* .handler */
		else {
			WMT_WARN_FUNC("WMT-PLAT: null fp for gpio_ctrl(%d)\n", id);
			iret = -2;
		}
	}

	return iret;
}

static INT32 wmt_plat_ldo_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WMT-PLAT:LDO is not used\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_state[GPIO_PULL_DIS]);
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:LDO init (out 0)\n");
		break;
	case PIN_STA_OUT_H:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num, 1);
		WMT_DBG_FUNC("WMT-PLAT:LDO (out 1)\n");
		break;
	case PIN_STA_OUT_L:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:LDO (out 0)\n");
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		WMT_DBG_FUNC("WMT-PLAT:LDO deinit (in pd)\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on LDO\n", state);
		break;
	}

	return 0;
}

static INT32 wmt_plat_pmu_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_ERR_FUNC("WMT-PLAT:PMU not define\n");
		return -1;
	}

	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_state[GPIO_PULL_DIS]);
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:PMU init (out %d)\n",
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num));
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num != DEFAULT_PIN_ID) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_state[GPIO_PULL_DIS]);
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num,
					0);
		}
		WMT_DBG_FUNC("WMT-PLAT:PMU init (out 0)\n");
		break;

	case PIN_STA_OUT_H:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num, 1);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 1): %d\n",
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num));
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num != DEFAULT_PIN_ID)
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num,
					1);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 1)\n");
		break;

	case PIN_STA_OUT_L:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 0): %d\n",
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num));
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num != DEFAULT_PIN_ID)
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num,
					0);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 0)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num != DEFAULT_PIN_ID)
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		WMT_DBG_FUNC("WMT-PLAT:PMU deinit (in pd)\n");
		break;
	case PIN_STA_SHOW:
		WMT_INFO_FUNC("WMT-PLAT:PMU PIN_STA_SHOW start\n");
		WMT_INFO_FUNC("WMT-PLAT:PMU out(%d)\n",
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num));
		WMT_INFO_FUNC("WMT-PLAT:PMU PIN_STA_SHOW end\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on PMU\n", state);
		break;
	}

	return 0;
}

static INT32 wmt_plat_rtc_ctrl(ENUM_PIN_STATE state)
{
	switch (state) {
	case PIN_STA_INIT:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
		rtc_gpio_enable_32k(RTC_GPIO_USER_GPS);
		WMT_DBG_FUNC("WMT-PLAT:RTC init\n");
#endif
		break;
	case PIN_STA_SHOW:
		WMT_INFO_FUNC("WMT-PLAT:RTC PIN_STA_SHOW start\n");
		/* WMT_INFO_FUNC("WMT-PLAT:RTC Status(%d)\n", rtc_gpio_32k_status()); */
		WMT_INFO_FUNC("WMT-PLAT:RTC PIN_STA_SHOW end\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on RTC\n", state);
		break;
	}

	return 0;
}

static INT32 wmt_plat_rst_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_ERR_FUNC("WMT-PLAT:RST not define\n");
		return -1;
	}

	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_state[GPIO_PULL_DIS]);
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:RST init (out 0)\n");
		break;

	case PIN_STA_OUT_H:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num, 1);
		WMT_DBG_FUNC("WMT-PLAT:RST (out 1): %d\n",
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num));
		break;

	case PIN_STA_OUT_L:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:RST (out 0): %d\n",
			gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num));
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		WMT_DBG_FUNC("WMT-PLAT:RST deinit (in pd)\n");
		break;
	case PIN_STA_SHOW:
		WMT_INFO_FUNC("WMT-PLAT:RST PIN_STA_SHOW start\n");
		WMT_INFO_FUNC("WMT-PLAT:RST out(%d)\n",
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num));
		WMT_INFO_FUNC("WMT-PLAT:RST PIN_STA_SHOW end\n");
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on RST\n", state);
		break;
	}

	return 0;
}

static INT32 wmt_plat_bgf_eint_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_INFO_FUNC("WMT-PLAT:BGF EINT not defined\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio input low, pull down enable */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt init(in pd)\n");
		break;
	case PIN_STA_MUX:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_state[GPIO_IN_PULLUP]);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt mux (eint)\n");
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt deinit(in pd)\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on BGF EINT\n", state);
		break;
	}

	return 0;
}


static INT32 wmt_plat_wifi_eint_ctrl(ENUM_PIN_STATE state)
{
#if 0				/*def GPIO_WIFI_EINT_PIN */
	switch (state) {
	case PIN_STA_INIT:
		mt_set_gpio_pull_enable(GPIO_WIFI_EINT_PIN, GPIO_PULL_DISABLE);
		mt_set_gpio_dir(GPIO_WIFI_EINT_PIN, GPIO_DIR_OUT);
		mt_set_gpio_mode(GPIO_WIFI_EINT_PIN, GPIO_MODE_GPIO);
		mt_set_gpio_out(GPIO_WIFI_EINT_PIN, GPIO_OUT_ONE);
		break;
	case PIN_STA_MUX:
		mt_set_gpio_mode(GPIO_WIFI_EINT_PIN, GPIO_WIFI_EINT_PIN_M_GPIO);
		mt_set_gpio_pull_enable(GPIO_WIFI_EINT_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_WIFI_EINT_PIN, GPIO_PULL_UP);
		mt_set_gpio_mode(GPIO_WIFI_EINT_PIN, GPIO_WIFI_EINT_PIN_M_EINT);

		break;
	case PIN_STA_EINT_EN:
		mt_eint_unmask(CUST_EINT_WIFI_NUM);
		break;
	case PIN_STA_EINT_DIS:
		mt_eint_mask(CUST_EINT_WIFI_NUM);
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_set_gpio_mode(GPIO_WIFI_EINT_PIN, GPIO_COMBO_BGF_EINT_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_WIFI_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_WIFI_EINT_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_WIFI_EINT_PIN, GPIO_PULL_ENABLE);
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on WIFI EINT\n", state);
		break;
	}
#else
	WMT_INFO_FUNC("WMT-PLAT:WIFI EINT is controlled by MSDC driver\n");
#endif
	return 0;
}


static INT32 wmt_plat_all_eint_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WMT-PLAT:ALL EINT not defined\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		WMT_DBG_FUNC("WMT-PLAT:ALLInt init(in pd)\n");
		break;
	case PIN_STA_MUX:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_state[GPIO_IN_PULLUP]);
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on ALL EINT\n", state);
		break;
	}

	return 0;
}

static INT32 wmt_plat_uart_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WMT-PLAT:UART TX not defined\n");
		return 0;
	}

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WMT-PLAT:UART RX not defined\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_MUX:
	case PIN_STA_INIT:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_state[GPIO_PULL_DIS]);
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_state[GPIO_PULL_DIS]);
		WMT_DBG_FUNC("WMT-PLAT:UART init (mode_01, uart)\n");
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num, 0);
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:UART deinit (out 0)\n");
		break;
	case PIN_STA_IN_PU:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_state[GPIO_IN_PULLUP]);
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on UART Group\n", state);
		break;
	}

	return 0;
}

static INT32 wmt_plat_pcm_ctrl(ENUM_PIN_STATE state)
{
	UINT32 normalPCMFlag = 0;

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_INFO_FUNC("WMT-PLAT:PCM DAICLK not defined\n");
		return 0;
	}

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_INFO_FUNC("WMT-PLAT:PCM DAIPCMOUT not defined\n");
		return 0;
	}

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_INFO_FUNC("WMT-PLAT:PCM DAIPCMIN not defined\n");
		return 0;
	}

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_INFO_FUNC("WMT-PLAT:PCM DAISYNC not defined\n");
		return 0;
	}
	/*check if combo chip support merge if or not */
	if (wmt_plat_merge_if_flag_get() != 0) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		/* Hardware support Merge IF function */
		WMT_DBG_FUNC("WMT-PLAT:<Merge IF>set to Merge PCM function\n");
		/*merge PCM function define */
		switch (state) {
		case PIN_STA_MUX:
		case PIN_STA_INIT:
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_state[GPIO_PULL_DIS]);
			WMT_DBG_FUNC("WMT-PLAT:<Merge IF>PCM init (pcm)\n");
			break;

		case PIN_STA_IN_L:
		case PIN_STA_DEINIT:
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_state[GPIO_PULL_DIS]);
			WMT_DBG_FUNC("WMT-PLAT:<Merge IF>PCM deinit (out 0)\n");
			break;

		default:
			WMT_WARN_FUNC
			    ("WMT-PLAT:<Merge IF>Warnning, invalid state(%d) on PCM Group\n",
			     state);
			break;
		}

#else
		/* Hardware does not support Merge IF function */
		normalPCMFlag = 1;
		WMT_DBG_FUNC("WMT-PLAT:set to normal PCM function\n");
#endif

	} else {
		normalPCMFlag = 1;
	}

	if (normalPCMFlag != 0) {
		/*normal PCM function define */
		switch (state) {
		case PIN_STA_MUX:
		case PIN_STA_INIT:
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_state[GPIO_PULL_DIS]);
			WMT_DBG_FUNC("WMT-PLAT:MT6589 PCM init (pcm)\n");
			break;

		case PIN_STA_IN_L:
		case PIN_STA_DEINIT:
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_state[GPIO_PULL_DIS]);
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
					gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_state[GPIO_PULL_DIS]);
			WMT_DBG_FUNC("WMT-PLAT:MT6589 PCM deinit (out 0)\n");
			break;

		default:
			WMT_WARN_FUNC("WMT-PLAT:MT6589 Warnning, invalid state(%d) on PCM Group\n",
				      state);
			break;
		}
	}

	return 0;
}

static INT32 wmt_plat_cmb_i2s_ctrl(ENUM_PIN_STATE state)
{
	UINT32 normalI2SFlag = 0;

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WMT-PLAT:I2S CK not defined\n");
		return 0;
	}

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WMT-PLAT:I2S WS not defined\n");
		return 0;
	}

	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WMT-PLAT:DAT CK not defined\n");
		return 0;
	}
	/*check if combo chip support merge if or not */
	if (wmt_plat_merge_if_flag_get() != 0) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		/* Hardware support Merge IF function */
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num != DEFAULT_PIN_ID) {
			switch (state) {
			case PIN_STA_INIT:
			case PIN_STA_MUX:
				pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_state[GPIO_PULL_DIS]);
				pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_state[GPIO_PULL_DIS]);
				pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_state[GPIO_PULL_DIS]);
				pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_PCM_DAIPMCOUT_PIN].
							gpio_state[GPIO_PULL_DIS]);
				WMT_DBG_FUNC("WMT-PLAT:<Merge IF>I2S init (I2S0 system)\n");
				break;
			case PIN_STA_IN_L:
			case PIN_STA_DEINIT:
				gpio_direction_output(gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num, 0);
				gpio_direction_output(gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_num, 0);
				gpio_direction_output(gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num, 0);
				WMT_DBG_FUNC("WMT-PLAT:<Merge IF>I2S deinit (out 0)\n");
				break;
			default:
				WMT_WARN_FUNC("WMT-PLAT:<Merge IF>Warnning, invalid state(%d) on I2S Group\n",
						state);
				break;
			}
		} else
			WMT_ERR_FUNC("[MT662x]<Merge IF>Error:FM digital mode set, no I2S GPIOs defined\n");
#else
		WMT_INFO_FUNC("[MT662x]<Merge IF>warnning:FM digital mode is not set\n");
		WMT_INFO_FUNC("no I2S GPIO settings should be modified by combo driver\n");
#endif
#else
		/* Hardware does support Merge IF function */
		normalI2SFlag = 1;
#endif
	} else
		normalI2SFlag = 1;

	if (normalI2SFlag != 0) {
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num != DEFAULT_PIN_ID) {
			switch (state) {
			case PIN_STA_INIT:
			case PIN_STA_MUX:
				pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_state[GPIO_PULL_DIS]);
				pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_state[GPIO_PULL_DIS]);
				pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_state[GPIO_PULL_DIS]);
				WMT_DBG_FUNC("WMT-PLAT:<I2S IF>I2S init (I2S0 system)\n");
				break;
			case PIN_STA_IN_L:
			case PIN_STA_DEINIT:
				gpio_direction_output(gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num, 0);
				gpio_direction_output(gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_num, 0);
				gpio_direction_output(gpio_ctrl_info.
						gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num, 0);
				WMT_DBG_FUNC("WMT-PLAT:<I2S IF>I2S deinit (out 0)\n");
				break;
			default:
				WMT_WARN_FUNC("WMT-PLAT:<I2S IF>Warnning, invalid state(%d) on I2S Group\n",
					      state);
				break;
			}
		} else
			WMT_ERR_FUNC("[MT662x]<I2S IF>Error:FM digital mode set, but no I2S GPIOs defined\n");
#else
		WMT_INFO_FUNC("[MT662x]<I2S IF>warnning:FM digital mode is not set\n");
		WMT_INFO_FUNC("no I2S GPIO settings should be modified by combo driver\n");
#endif
	}

	return 0;
}

static INT32 wmt_plat_soc_i2s_ctrl(ENUM_PIN_STATE state)
{
	WMT_PLAT_PR_WARN("host i2s pin not defined!!!\n");

	return 0;
}

static INT32 wmt_plat_i2s_ctrl(ENUM_PIN_STATE state)
{
	INT32 ret = -1;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
		ret = wmt_plat_soc_i2s_ctrl(state);
	else
		ret = wmt_plat_cmb_i2s_ctrl(state);

	return ret;
}

static INT32 wmt_plat_sdio_pin_ctrl(ENUM_PIN_STATE state)
{
	return 0;
}

static INT32 wmt_plat_cmb_gps_sync_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_INFO_FUNC("WMT-PLAT:GPS SYNC not defined\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_DEINIT:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_state[GPIO_PULL_DIS]);
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_num, 0);
		break;
	case PIN_STA_MUX:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_state[GPIO_PULL_DIS]);
		break;
	default:
		break;
	}

	return 0;
}

static INT32 wmt_plat_soc_gps_sync_ctrl(ENUM_PIN_STATE state)
{
	WMT_PLAT_PR_WARN("host gps sync pin not defined!!!\n");

	return 0;
}

static INT32 wmt_plat_gps_sync_ctrl(ENUM_PIN_STATE state)
{
	INT32 ret = -1;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
		ret = wmt_plat_soc_gps_sync_ctrl(state);
	else
		ret = wmt_plat_cmb_gps_sync_ctrl(state);

	return ret;
}

static INT32 wmt_plat_soc_gps_lna_ctrl(ENUM_PIN_STATE state)
{
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_DEINIT:
		KERNEL_mt6306_set_gpio_dir(MT6306_GPIO_01, MT6306_GPIO_DIR_OUT);
		KERNEL_mt6306_set_gpio_out(MT6306_GPIO_01, MT6306_GPIO_OUT_LOW);
		WMT_PLAT_PR_DBG("set gps lna to init\n");
		break;
	case PIN_STA_OUT_H:
		KERNEL_mt6306_set_gpio_out(MT6306_GPIO_01, MT6306_GPIO_OUT_HIGH);
		WMT_PLAT_PR_DBG("set gps lna to oh\n");
		break;
	case PIN_STA_OUT_L:
		KERNEL_mt6306_set_gpio_out(MT6306_GPIO_01, MT6306_GPIO_OUT_LOW);
		WMT_PLAT_PR_DBG("set gps lna to ol\n");
		break;
	default:
		WMT_PLAT_PR_WARN("%d mode not defined for  gps lna pin !!!\n", state);
		break;
	}
#else
	struct pinctrl_state *gps_lna_init = NULL;
	struct pinctrl_state *gps_lna_oh = NULL;
	struct pinctrl_state *gps_lna_ol = NULL;
	struct pinctrl *consys_pinctrl = NULL;

	WMT_PLAT_PR_DBG("ENTER++\n");
	consys_pinctrl = mtk_wcn_consys_get_pinctrl();
	if (!consys_pinctrl) {
		WMT_PLAT_PR_ERR("get consys pinctrl fail\n");
		return 0;
	}

	gps_lna_init = pinctrl_lookup_state(consys_pinctrl, "gps_lna_state_init");
	if (IS_ERR(gps_lna_init)) {
		WMT_PLAT_PR_ERR("Cannot find gps lna pin init state!\n");
		return 0;
	}

	gps_lna_oh = pinctrl_lookup_state(consys_pinctrl, "gps_lna_state_oh");
	if (IS_ERR(gps_lna_oh)) {
		WMT_PLAT_PR_ERR("Cannot find gps lna pin oh state!\n");
		return 0;
	}

	gps_lna_ol = pinctrl_lookup_state(consys_pinctrl, "gps_lna_state_ol");
	if (IS_ERR(gps_lna_ol)) {
		WMT_PLAT_PR_ERR("Cannot find gps lna pin ol state!\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_DEINIT:
		pinctrl_select_state(consys_pinctrl, gps_lna_init);
		WMT_PLAT_PR_DBG("set gps lna to init\n");
		break;
	case PIN_STA_OUT_H:
		pinctrl_select_state(consys_pinctrl, gps_lna_oh);
		WMT_PLAT_PR_DBG("set gps lna to oh\n");
		break;
	case PIN_STA_OUT_L:
		pinctrl_select_state(consys_pinctrl, gps_lna_ol);
		WMT_PLAT_PR_DBG("set gps lna to ol\n");
		break;
	default:
		WMT_PLAT_PR_WARN("%d mode not defined for  gps lna pin !!!\n", state);
		break;
	}
#endif

	return 0;
}

static INT32 wmt_plat_cmb_gps_lna_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_num != DEFAULT_PIN_ID) {
		switch (state) {
		case PIN_STA_INIT:
		case PIN_STA_DEINIT:
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_state[GPIO_PULL_DIS]);
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_num, 0);
			break;
		case PIN_STA_OUT_H:
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_num, 1);
			break;
		case PIN_STA_OUT_L:
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_num, 0);
			break;
		default:
			WMT_WARN_FUNC("%d mode not defined for  gps lna pin !!!\n", state);
			break;
		}
	} else {
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
		WMT_WARN_FUNC("/******************************************************************/\n");
		WMT_WARN_FUNC("use MT6306 GPIO7 for  gps lna pin.\n this HARD CODE may hurt other\n");
		WMT_WARN_FUNC("system module, if GPIO7 of MT6306 is not defined as GPS_LNA function\n");
		WMT_WARN_FUNC("/******************************************************************/\n");

		switch (state) {
		case PIN_STA_INIT:
		case PIN_STA_DEINIT:
			/* KERNEL_mt6306_set_gpio_dir(GPIO7, GPIO_DIR_OUT); */
			/* KERNEL_mt6306_set_gpio_out(GPIO7, GPIO_OUT_ZERO); */
			break;
		case PIN_STA_OUT_H:
			/* KERNEL_mt6306_set_gpio_out(GPIO7, GPIO_OUT_ONE); */
			break;
		case PIN_STA_OUT_L:
			/* KERNEL_mt6306_set_gpio_out(GPIO7, GPIO_OUT_ZERO); */
			break;
		default:
			WMT_WARN_FUNC("%d mode not defined for  gps lna pin !!!\n", state);
			break;
		}
#else
		WMT_WARN_FUNC("host gps lna pin not defined!!!\n");
		WMT_WARN_FUNC("if you donot use eighter AP or MT6306's pin as GPS_LNA\n");
		WMT_WARN_FUNC("please customize your own GPS_LNA related code here\n");
#endif
	}

	return 0;
}

static INT32 wmt_plat_gps_lna_ctrl(ENUM_PIN_STATE state)
{
	INT32 ret = -1;

	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
		ret = wmt_plat_soc_gps_lna_ctrl(state);
	else
		ret = wmt_plat_cmb_gps_lna_ctrl(state);

	return ret;
}

static INT32 wmt_plat_uart_rx_ctrl(ENUM_PIN_STATE state)
{
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num == DEFAULT_PIN_ID) {
		WMT_DBG_FUNC("WMT-PLAT:UART RX not defined\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_MUX:
	case PIN_STA_INIT:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_state[GPIO_PULL_DIS]);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx init\n");
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_state[GPIO_PULL_DIS]);
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx deinit (out 0)\n");
		break;
	case PIN_STA_IN_NP:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_state[GPIO_IN_PULL_DIS]);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx input pull none\n");
		break;
	case PIN_STA_IN_H:
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_state[GPIO_IN_PULLUP]);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx input pull high\n");
		break;
	case PIN_STA_OUT_H:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num, 1);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx output high\n");
		break;
	case PIN_STA_OUT_L:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx deinit (out 0)\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on UART Rx\n", state);
		break;
	}

	return 0;
}

#if CFG_WMT_LTE_COEX_HANDLING
static INT32 wmt_plat_tdm_req_ctrl(ENUM_PIN_STATE state)
{
	return 0;
}
#endif

INT32 wmt_plat_wake_lock_ctrl(ENUM_WL_OP opId)
{
	static INT32 counter;
	INT32 ret = 0;

	ret = osal_lock_sleepable_lock(&gOsSLock);
	if (ret) {
		WMT_ERR_FUNC("--->lock gOsSLock failed, ret=%d\n", ret);
		return ret;
	}

	if (opId == WL_OP_GET)
		++counter;
	else if (opId == WL_OP_PUT)
		--counter;

	osal_unlock_sleepable_lock(&gOsSLock);
	if (opId == WL_OP_GET && counter == 1) {
		osal_wake_lock(&wmt_wake_lock);
		WMT_DBG_FUNC("WMT-PLAT: after wake_lock(%d), counter(%d)\n",
				osal_wake_lock_count(&wmt_wake_lock), counter);

	} else if (opId == WL_OP_PUT && counter == 0) {
		osal_wake_unlock(&wmt_wake_lock);
		WMT_DBG_FUNC("WMT-PLAT: after wake_unlock(%d), counter(%d)\n",
				osal_wake_lock_count(&wmt_wake_lock), counter);
	} else {
		WMT_WARN_FUNC("WMT-PLAT: wakelock status(%d), counter(%d)\n",
				osal_wake_lock_count(&wmt_wake_lock), counter);
	}

	return 0;
}


INT32 wmt_plat_merge_if_flag_ctrl(UINT32 enable)
{
	if (enable) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		gWmtMergeIfSupport = 1;
#else
		gWmtMergeIfSupport = 0;
		WMT_WARN_FUNC("neither MT6589, MTK_MERGE_INTERFACE_SUPPORT nor MT6628 is not set to 1\n");
		WMT_WARN_FUNC("so set gWmtMergeIfSupport to %d\n", gWmtMergeIfSupport);
#endif
	} else
		gWmtMergeIfSupport = 0;

	WMT_INFO_FUNC("set gWmtMergeIfSupport to %d\n", gWmtMergeIfSupport);

	return gWmtMergeIfSupport;
}

INT32 wmt_plat_merge_if_flag_get(VOID)
{
	return gWmtMergeIfSupport;
}

INT32 wmt_plat_set_comm_if_type(ENUM_STP_TX_IF_TYPE type)
{
	gCommIfType = type;

	return 0;
}

ENUM_STP_TX_IF_TYPE wmt_plat_get_comm_if_type(VOID)
{
	return gCommIfType;
}

INT32 wmt_plat_soc_paldo_ctrl(ENUM_PALDO_TYPE ePt, ENUM_PALDO_OP ePo)
{
	INT32 iRet = 0;

	switch (ePt) {
	case BT_PALDO:
		iRet = mtk_wcn_consys_hw_bt_paldo_ctrl(ePo);
		break;
	case WIFI_PALDO:
		iRet = mtk_wcn_consys_hw_wifi_paldo_ctrl(ePo);
		break;
	case FM_PALDO:
	case GPS_PALDO:
		iRet = mtk_wcn_consys_hw_vcn28_ctrl(ePo);
		break;
	case EFUSE_PALDO:
		iRet = mtk_wcn_consys_hw_efuse_paldo_ctrl(ePo, wmt_plat_soc_co_clock_flag_get());
		break;
	default:
		WMT_PLAT_PR_WARN("WMT-PLAT:Warnning, invalid type(%d) in palod_ctrl\n", ePt);
		break;
	}

	return iRet;
}

#if CONSYS_WMT_REG_SUSPEND_CB_ENABLE
UINT32 wmt_plat_soc_osc_en_ctrl(UINT32 en)
{
	return mtk_wcn_consys_hw_osc_en_ctrl(en);
}
#endif

UINT8 *wmt_plat_get_emi_virt_add(UINT32 offset)
{
	return mtk_wcn_consys_emi_virt_addr_get(offset);
}

P_CONSYS_EMI_ADDR_INFO wmt_plat_get_emi_phy_add(VOID)
{
	return mtk_wcn_consys_soc_get_emi_phy_add();
}

#if CONSYS_ENALBE_SET_JTAG
UINT32 wmt_plat_jtag_flag_ctrl(UINT32 en)
{
	return mtk_wcn_consys_jtag_flag_ctrl(en);
}
#endif

#if CFG_WMT_DUMP_INT_STATUS
VOID wmt_plat_BGF_irq_dump_status(VOID)
{
	mt_irq_dump_status(269);/*tag3 wujun rainier is enabled */

	WMT_PLAT_PR_INFO("this function is null in MT6735\n");
}

MTK_WCN_BOOL wmt_plat_dump_BGF_irq_status(VOID)
{
	return MTK_WCN_BOOL_FALSE;/*tag4 wujun rainier is enabled */
}
#endif

UINT32 wmt_plat_read_cpupcr(VOID)
{
	return mtk_wcn_consys_read_cpupcr();
}
EXPORT_SYMBOL(wmt_plat_read_cpupcr);

UINT32 wmt_plat_read_dmaregs(UINT32 type)
{
	return 0;
#if 0
	switch (type) {
	case CONNSYS_CLK_GATE_STATUS:
		return CONSYS_REG_READ(CONNSYS_CLK_GATE_STATUS_REG);
	case CONSYS_EMI_STATUS:
		return CONSYS_REG_READ(CONSYS_EMI_STATUS_REG);
	case SYSRAM1:
		return CONSYS_REG_READ(SYSRAM1_REG);
	case SYSRAM2:
		return CONSYS_REG_READ(SYSRAM2_REG);
	case SYSRAM3:
		return CONSYS_REG_READ(SYSRAM3_REG);
	default:
		return 0;
	}
#endif
}

INT32 wmt_plat_set_host_dump_state(ENUM_HOST_DUMP_STATE state)
{
	PUINT8 p_virtual_addr = NULL;

	p_virtual_addr = wmt_plat_get_emi_virt_add(EXP_APMEM_CTRL_HOST_SYNC_STATE);
	if (!p_virtual_addr) {
		WMT_PLAT_PR_ERR("get virtual address fail\n");
		return -1;
	}

	CONSYS_REG_WRITE(p_virtual_addr, state);

	return 0;
}

UINT32 wmt_plat_force_trigger_assert(ENUM_FORCE_TRG_ASSERT_T type)
{
	PUINT8 p_virtual_addr = NULL;

	switch (type) {
	case STP_FORCE_TRG_ASSERT_EMI:

		WMT_PLAT_PR_INFO("[Force Assert] stp_trigger_firmware_assert_via_emi -->\n");
		p_virtual_addr = wmt_plat_get_emi_virt_add(EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1);
		if (!p_virtual_addr) {
			WMT_PLAT_PR_ERR("get virtual address fail\n");
			return -1;
		}

		CONSYS_REG_WRITE(p_virtual_addr, EXP_APMEM_HOST_OUTBAND_ASSERT_MAGIC_W1);
		WMT_PLAT_PR_INFO("[Force Assert] stp_trigger_firmware_assert_via_emi <--\n");
		break;
	case STP_FORCE_TRG_ASSERT_DEBUG_PIN:
		mtk_wcn_force_trigger_assert_debug_pin();
		break;
	default:
		WMT_PLAT_PR_ERR("unknown force trigger assert type\n");
		break;
	}

	return 0;
}

INT32 wmt_plat_update_host_sync_num(VOID)
{
	PUINT8 p_virtual_addr = NULL;
	UINT32 sync_num = 0;

	p_virtual_addr = wmt_plat_get_emi_virt_add(EXP_APMEM_CTRL_HOST_SYNC_NUM);
	if (!p_virtual_addr) {
		WMT_PLAT_PR_ERR("get virtual address fail\n");
		return -1;
	}

	sync_num = CONSYS_REG_READ(p_virtual_addr);
	CONSYS_REG_WRITE(p_virtual_addr, sync_num + 1);

	return 0;
}

INT32 wmt_plat_get_dump_info(UINT32 offset)
{
	PUINT8 p_virtual_addr = NULL;

	p_virtual_addr = wmt_plat_get_emi_virt_add(offset);
	if (!p_virtual_addr) {
		WMT_PLAT_PR_ERR("get virtual address fail\n");
		return -1;
	}
	WMT_PLAT_PR_DBG("connsys_reg_read (0x%x), (0x%p), (0x%x)\n", CONSYS_REG_READ(p_virtual_addr), p_virtual_addr,
			   offset);
	return CONSYS_REG_READ(p_virtual_addr);
}

INT32 wmt_plat_write_emi_l(UINT32 offset, UINT32 value)
{
	PUINT8 p_virtual_addr = NULL;

	p_virtual_addr = wmt_plat_get_emi_virt_add(offset);
	if (!p_virtual_addr) {
		WMT_PLAT_PR_ERR("get virtual address fail\n");
		return -1;
	}

	CONSYS_REG_WRITE(p_virtual_addr, value);
	return 0;
}

UINT32 wmt_plat_get_soc_chipid(VOID)
{
	UINT32 chipId = mtk_wcn_consys_soc_chipid();

	return chipId;
}
EXPORT_SYMBOL(wmt_plat_get_soc_chipid);

INT32 wmt_plat_get_adie_chipid(VOID)
{
	return mtk_wcn_consys_detect_adie_chipid(gCoClockFlag);
}

#if CFG_WMT_LTE_COEX_HANDLING
INT32 wmt_plat_get_tdm_antsel_index(VOID)
{
	WMT_PLAT_PR_INFO("not support LTE in this platform\n");
	return 0;
}
#endif

INT32 wmt_plat_set_dbg_mode(UINT32 flag)
{
	INT32 ret = -1;
	PUINT8 vir_addr = NULL;

	vir_addr = mtk_wcn_consys_emi_virt_addr_get(EXP_APMEM_CTRL_CHIP_FW_DBGLOG_MODE);
	if (!vir_addr) {
		WMT_PLAT_PR_ERR("get vir address fail\n");
		return ret;
	}
	if (flag) {
		CONSYS_REG_WRITE(vir_addr, 0x1);
		ret = 0;
	} else {
		CONSYS_REG_WRITE(vir_addr, 0x0);
		ret = 1;
	}
	WMT_PLAT_PR_INFO("fw dbg mode register value(0x%08x)\n", CONSYS_REG_READ(vir_addr));

	return ret;
}

INT32 wmt_plat_set_dynamic_dumpmem(PUINT32 str_buf)
{
	PUINT8 vir_addr = NULL;

	vir_addr = mtk_wcn_consys_emi_virt_addr_get(EXP_APMEM_CTRL_CHIP_DYNAMIC_DUMP);
	if (!vir_addr) {
		WMT_PLAT_PR_ERR("get vir address fail\n");
		return -1;
	}
	memcpy(vir_addr, str_buf, DYNAMIC_DUMP_GROUP_NUM*8);
	WMT_PLAT_PR_INFO("dynamic dump register value(0x%08x)\n", CONSYS_REG_READ(vir_addr));

	return 0;
}
