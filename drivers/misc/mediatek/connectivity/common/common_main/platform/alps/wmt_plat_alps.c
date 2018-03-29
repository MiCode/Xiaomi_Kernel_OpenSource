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


#ifdef CONFIG_PM_WAKELOCKS
/* #include <linux/pm_wakeup.h> */
#else
#include <linux/wakelock.h>
#endif
#define CFG_WMT_WAKELOCK_SUPPORT 1

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-PLAT]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/* ALPS header files */
#include <mtk_rtc.h>
#ifdef CONFIG_MTK_MT6306_SUPPORT
#include <mach/dcl_sim_gpio.h>
#endif

/* ALPS and COMBO header files */
#include <mtk_wcn_cmb_stub.h>
/* MTK_WCN_COMBO header files */
#include "wmt_plat.h"
#include "wmt_dev.h"
#include "wmt_lib.h"
#include "mtk_wcn_cmb_hw.h"
#include "osal.h"
#include "wmt_gpio.h"

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>

static UINT32 bgf_irq;

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
/* static VOID wmt_plat_bgf_eirq_cb(VOID); */

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

static INT32 wmt_plat_dump_pin_conf(VOID);


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
/* UINT32 gWmtDbgLvl = WMT_LOG_INFO; */
INT32 gWmtMergeIfSupport = 0;
static ENUM_STP_TX_IF_TYPE gCommIfType = STP_MAX_IF_TX;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
#if CFG_WMT_WAKELOCK_SUPPORT
static OSAL_SLEEPABLE_LOCK gOsSLock;
static struct wake_lock wmtWakeLock;
#endif

irq_cb wmt_plat_bgf_irq_cb = NULL;
device_audio_if_cb wmt_plat_audio_if_cb = NULL;

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
INT32 wmt_plat_audio_ctrl(CMB_STUB_AIF_X state, CMB_STUB_AIF_CTRL ctrl)
{
	INT32 iRet = 0;
	UINT32 pinShare = 0;
	UINT32 mergeIfSupport = 0;

	/* input sanity check */
	if ((CMB_STUB_AIF_MAX <= state)
	    || (CMB_STUB_AIF_CTRL_MAX <= ctrl)) {
		return -1;
	}

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

	if (0 != wmt_plat_merge_if_flag_get()) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		WMT_DBG_FUNC("[MT6628]<Merge IF> no need to ctrl combo chip side GPIO\n");
#else
		mergeIfSupport = 1;
#endif
	} else {
		mergeIfSupport = 1;
	}

	if (0 != mergeIfSupport) {
		if (CMB_STUB_AIF_CTRL_EN == ctrl) {
			WMT_INFO_FUNC("call chip aif setting\n");
			/* need to control chip side GPIO */
			if (NULL != wmt_plat_audio_if_cb) {
				iRet +=
				    (*wmt_plat_audio_if_cb) (state,
							     (pinShare) ? MTK_WCN_BOOL_TRUE :
							     MTK_WCN_BOOL_FALSE);
			} else {
				WMT_WARN_FUNC("wmt_plat_audio_if_cb is not registered\n");
				iRet -= 1;
			}


		} else {
			WMT_INFO_FUNC("skip chip aif setting\n");
		}
	}
	return iRet;

}

irqreturn_t wmt_plat_bgf_eirq_cb (int irq, void *data)
{
	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num) {
#if CFG_WMT_PS_SUPPORT
		/* #error "need to disable EINT here" */
		/* wmt_lib_ps_irq_cb(); */
		if (NULL != wmt_plat_bgf_irq_cb)
			(*(wmt_plat_bgf_irq_cb)) ();
		else
			WMT_WARN_FUNC("WMT-PLAT: wmt_plat_bgf_irq_cb not registered\n");
#endif
	}

	return IRQ_HANDLED;
}

VOID wmt_lib_plat_irq_cb_reg(irq_cb bgf_irq_cb)
{
	wmt_plat_bgf_irq_cb = bgf_irq_cb;
}

VOID wmt_lib_plat_aif_cb_reg(device_audio_if_cb aif_ctrl_cb)
{
	wmt_plat_audio_if_cb = aif_ctrl_cb;
}

INT32 wmt_plat_init(P_PWR_SEQ_TIME pPwrSeqTime)
{
	/*PWR_SEQ_TIME pwr_seq_time; */
	INT32 iret = -1;
	/* init cmb_hw */
	iret += mtk_wcn_cmb_hw_init(pPwrSeqTime);

	/*init wmt function ctrl wakelock if wake lock is supported by host platform */
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	wake_lock_init(&wmtWakeLock, WAKE_LOCK_SUSPEND, "wmtFuncCtrl");
	osal_sleepable_lock_init(&gOsSLock);
#endif

	WMT_DBG_FUNC("WMT-PLAT: ALPS platform init (%d)\n", iret);

	return 0;
}


INT32 wmt_plat_deinit(VOID)
{
	INT32 iret;

	/* 1. de-init cmb_hw */
	iret = mtk_wcn_cmb_hw_deinit();
	/* 2. unreg to cmb_stub */
	iret += mtk_wcn_cmb_stub_unreg();
	/*3. wmt wakelock deinit */
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	wake_lock_destroy(&wmtWakeLock);
	osal_sleepable_lock_deinit(&gOsSLock);
	WMT_DBG_FUNC("destroy wmtWakeLock\n");
#endif
	WMT_DBG_FUNC("WMT-PLAT: ALPS platform init (%d)\n", iret);

	return 0;
}

INT32 wmt_plat_sdio_ctrl(WMT_SDIO_SLOT_NUM sdioPortType, ENUM_FUNC_STATE on)
{
	return board_sdio_ctrl(sdioPortType, (FUNC_OFF == on) ? 0 : 1);
}

INT32 wmt_plat_irq_ctrl(ENUM_FUNC_STATE state)
{
	return -1;
}

static INT32 wmt_plat_dump_pin_conf(VOID)
{
	WMT_DBG_FUNC("[WMT-PLAT]=>dump wmt pin configuration start<=\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num) {
		WMT_DBG_FUNC("LDO(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("LDO(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num) {
		WMT_DBG_FUNC("PMU(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PMU(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num) {
		WMT_DBG_FUNC("PMUV28(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PMUV28(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num) {
		WMT_DBG_FUNC("RST(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("RST(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num) {
		WMT_DBG_FUNC("BGF_EINT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("BGF_EINT(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num) {
		WMT_DBG_FUNC("BGF_EINT_NUM(%d)\n",
				gpio_to_irq(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num));
	} else
		WMT_DBG_FUNC("BGF_EINT_NUM(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num) {
		WMT_DBG_FUNC("WIFI_EINT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("WIFI_EINT(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num) {
		WMT_DBG_FUNC("WIFI_EINT_NUM(%d)\n",
				gpio_to_irq(gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num));
	} else
		WMT_DBG_FUNC("WIFI_EINT_NUM(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num) {
		WMT_DBG_FUNC("UART_RX(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("UART_RX(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_num) {
		WMT_DBG_FUNC("UART_TX(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("UART_TX(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_num) {
		WMT_DBG_FUNC("DAICLK(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("DAICLK(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_num) {
		WMT_DBG_FUNC("PCMOUT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PCMOUT(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_num) {
		WMT_DBG_FUNC("PCMIN(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PCMIN(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_num) {
		WMT_DBG_FUNC("PCMSYNC(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("PCMSYNC(not defined)\n");
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num) {
		WMT_DBG_FUNC("I2S_CK(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("I2S_CK(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_num) {
		WMT_DBG_FUNC("I2S_WS(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("I2S_WS(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num) {
		WMT_DBG_FUNC("I2S_DAT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("I2S_DAT(not defined)\n");

#else				/* FM_ANALOG_INPUT || FM_ANALOG_OUTPUT */
	WMT_DBG_FUNC("FM digital mode is not set, no need for I2S GPIOs\n");
#endif

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_num) {
		WMT_DBG_FUNC("GPS_SYNC(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_num);
	} else
		WMT_DBG_FUNC("GPS_SYNC(not defined)\n");

	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_num) {
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
		wmt_plat_dump_pin_conf();
		ret = mtk_wcn_cmb_hw_pwr_on();
		break;

	case FUNC_OFF:
		ret = mtk_wcn_cmb_hw_pwr_off();
		break;

	case FUNC_RST:
		ret = mtk_wcn_cmb_hw_rst();
		break;
	case FUNC_STAT:
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

	/* TODO: [ChangeFeature][GeorgeKuo]: use another function to handle this, as done in gpio_ctrls */

	if ((PIN_STA_INIT != state)
	    && (PIN_STA_DEINIT != state)
	    && (PIN_STA_EINT_EN != state)
	    && (PIN_STA_EINT_DIS != state)) {
		WMT_WARN_FUNC("WMT-PLAT:invalid PIN_STATE(%d) in eirq_ctrl for PIN(%d)\n", state,
			      id);
		return -1;
	}

	iret = -2;
	switch (id) {
	case PIN_BGF_EINT:
		if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num) {
			if (PIN_STA_INIT == state) {
				struct device_node *node;
				INT32 ret = -EINVAL;

				node = of_find_compatible_node(NULL, NULL, "mediatek,connectivity-combo");
				if (node) {
					/*BGF-eint name maybe wrong*/
					bgf_irq = irq_of_parse_and_map(node, 1);
					ret = request_irq(bgf_irq, wmt_plat_bgf_eirq_cb,
							IRQF_TRIGGER_LOW, "BGF-eint", NULL);
					if (ret)
						WMT_ERR_FUNC("BGF EINT IRQ LINE NOT AVAILABLE!!\n");
					else
						WMT_INFO_FUNC("BGF EINT request_irq success!!\n");
				} else
					WMT_ERR_FUNC("[%s] can't find BGF eint compatible node\n", __func__);
			} else if (PIN_STA_EINT_EN == state) {
				enable_irq(bgf_irq);
				WMT_DBG_FUNC("WMT-PLAT:BGFInt (en)\n");
			} else if (PIN_STA_EINT_DIS == state) {
				disable_irq_nosync(bgf_irq);
				WMT_DBG_FUNC("WMT-PLAT:BGFInt (dis)\n");
			} else {
				disable_irq_nosync(bgf_irq);
				WMT_DBG_FUNC("WMT-PLAT:BGFInt (dis)\n");
				/* de-init: nothing to do in ALPS, such as un-registration... */
			}
		} else
			WMT_INFO_FUNC("WMT-PLAT:BGF EINT not defined\n");

		iret = 0;
		break;
	case PIN_ALL_EINT:
		if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num) {
			if (PIN_STA_INIT == state) {
				disable_irq_nosync(gpio_to_irq(gpio_ctrl_info.
							gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num));
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (INIT but not used yet)\n");
			} else if (PIN_STA_EINT_EN == state) {
				enable_irq(gpio_to_irq(gpio_ctrl_info.
							gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num));
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (EN but not used yet)\n");
			} else if (PIN_STA_EINT_DIS == state) {
				disable_irq_nosync(gpio_to_irq(gpio_ctrl_info.
							gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num));
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (DIS but not used yet)\n");
			} else {
				disable_irq_nosync(gpio_to_irq(gpio_ctrl_info.
							gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num));
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (DEINIT but not used yet)\n");
				/* de-init: nothing to do in ALPS, such as un-registration... */
			}
		} else {
			WMT_INFO_FUNC("WMT-PLAT:ALL EINT not defined\n");
		}

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

	if ((PIN_ID_MAX > id)
	    && (PIN_STA_MAX > state)) {

		/* TODO: [FixMe][GeorgeKuo] do sanity check to const function table when init and skip checking here */
		if (gfp_set_pin_table[id]) {
			iret = (*(gfp_set_pin_table[id])) (state);	/* .handler */
		} else {
			WMT_WARN_FUNC("WMT-PLAT: null fp for gpio_ctrl(%d)\n", id);
			iret = -2;
		}
	}
	return iret;
}

INT32 wmt_plat_ldo_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num) {
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

INT32 wmt_plat_pmu_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num) {
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
		if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num) {
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
		if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num)
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num,
					1);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 1)\n");
		break;

	case PIN_STA_OUT_L:
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num, 0);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 0): %d\n",
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num));
		if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num)
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num,
					0);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 0)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info, gpio_ctrl_info.
				gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_state[GPIO_IN_PULLDOWN]);
		if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num)
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

INT32 wmt_plat_rtc_ctrl(ENUM_PIN_STATE state)
{
	switch (state) {
	case PIN_STA_INIT:
		rtc_gpio_enable_32k(RTC_GPIO_USER_GPS);
		WMT_DBG_FUNC("WMT-PLAT:RTC init\n");
		break;
	case PIN_STA_SHOW:
		WMT_INFO_FUNC("WMT-PLAT:RTC PIN_STA_SHOW start\n");
		/* TakMan: Temp. solution for building pass.
		 * Hongcheng Xia should check with vend_ownen.chen */
		/* WMT_INFO_FUNC("WMT-PLAT:RTC Status(%d)\n", rtc_gpio_32k_status()); */
		WMT_INFO_FUNC("WMT-PLAT:RTC PIN_STA_SHOW end\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on RTC\n", state);
		break;
	}
	return 0;
}


INT32 wmt_plat_rst_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num) {
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

INT32 wmt_plat_bgf_eint_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num) {
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


INT32 wmt_plat_wifi_eint_ctrl(ENUM_PIN_STATE state)
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


INT32 wmt_plat_all_eint_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_ALL_EINT_PIN].gpio_num) {
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

INT32 wmt_plat_uart_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_num) {
		WMT_DBG_FUNC("WMT-PLAT:UART TX not defined\n");
		return 0;
	}

	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num) {
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
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on UART Group\n", state);
		break;
	}

	return 0;
}


INT32 wmt_plat_pcm_ctrl(ENUM_PIN_STATE state)
{
	UINT32 normalPCMFlag = 0;

	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_num) {
		WMT_INFO_FUNC("WMT-PLAT:PCM DAICLK not defined\n");
		return 0;
	}

	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_num) {
		WMT_INFO_FUNC("WMT-PLAT:PCM DAIPCMOUT not defined\n");
		return 0;
	}

	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_num) {
		WMT_INFO_FUNC("WMT-PLAT:PCM DAIPCMIN not defined\n");
		return 0;
	}

	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_num) {
		WMT_INFO_FUNC("WMT-PLAT:PCM DAISYNC not defined\n");
		return 0;
	}
	/*check if combo chip support merge if or not */
	if (0 != wmt_plat_merge_if_flag_get()) {
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

	if (0 != normalPCMFlag) {
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


INT32 wmt_plat_i2s_ctrl(ENUM_PIN_STATE state)
{
	/* TODO: [NewFeature][GeorgeKuo]: GPIO_I2Sx is changed according to different project. */
	/* TODO: provide a translation table in board_custom.h
	 * for different ALPS project customization. */

	UINT32 normalI2SFlag = 0;

	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num) {
		WMT_DBG_FUNC("WMT-PLAT:I2S CK not defined\n");
		return 0;
	}

	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_WS_PIN].gpio_num) {
		WMT_DBG_FUNC("WMT-PLAT:I2S WS not defined\n");
		return 0;
	}

	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num) {
		WMT_DBG_FUNC("WMT-PLAT:DAT CK not defined\n");
		return 0;
	}
	/*check if combo chip support merge if or not */
	if (0 != wmt_plat_merge_if_flag_get()) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		/* Hardware support Merge IF function */
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
		if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num) {
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

	if (0 != normalI2SFlag) {
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
		if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_CK_PIN].gpio_num) {
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

INT32 wmt_plat_sdio_pin_ctrl(ENUM_PIN_STATE state)
{
#if 0
	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_MUX:
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
#if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
		/* TODO: [FixMe][GeorgeKuo]: below are used for MT6573 only!
		 * Find a better way to do ALPS customization for different platform. */
		/* WMT_INFO_FUNC( "[mt662x] pull up sd1 bus(gpio62~68)\n"); */
		mt_set_gpio_pull_enable(GPIO62, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO62, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO63, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO63, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO64, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO64, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO65, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO65, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO66, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO66, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO67, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO67, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO68, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO68, GPIO_PULL_UP);
		WMT_DBG_FUNC("WMT-PLAT:SDIO init (pu)\n");
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
#error "fix sdio2 gpio settings"
#endif
#else
#error "CONFIG_MTK_WCN_CMB_SDIO_SLOT undefined!!!"
#endif

		break;

	case PIN_STA_DEINIT:
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
#if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
		/* TODO: [FixMe][GeorgeKuo]: below are used for MT6573 only!
		 * Find a better way to do ALPS customization for different platform. */
		/* WMT_INFO_FUNC( "[mt662x] pull down sd1 bus(gpio62~68)\n"); */
		mt_set_gpio_pull_select(GPIO62, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO62, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO63, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO63, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO64, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO64, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO65, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO65, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO66, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO66, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO67, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO67, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO68, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO68, GPIO_PULL_ENABLE);
		WMT_DBG_FUNC("WMT-PLAT:SDIO deinit (pd)\n");
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
#error "fix sdio2 gpio settings"
#endif
#else
#error "CONFIG_MTK_WCN_CMB_SDIO_SLOT undefined!!!"
#endif
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on SDIO Group\n", state);
		break;
	}
#endif
	return 0;
}

static INT32 wmt_plat_gps_sync_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_SYNC_PIN].gpio_num) {
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


static INT32 wmt_plat_gps_lna_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_GPS_LNA_PIN].gpio_num) {
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
#ifdef CONFIG_MTK_MT6306_SUPPORT
		WMT_WARN_FUNC("/******************************************************************/\n");
		WMT_WARN_FUNC("use MT6306 GPIO7 for  gps lna pin.\n this HARD CODE may hurt other\n");
		WMT_WARN_FUNC("system module, if GPIO7 of MT6306 is not defined as GPS_LNA function\n");
		WMT_WARN_FUNC("/******************************************************************/\n");

		switch (state) {
		case PIN_STA_INIT:
		case PIN_STA_DEINIT:
			mt6306_set_gpio_dir(GPIO7, GPIO_DIR_OUT);
			mt6306_set_gpio_out(GPIO7, GPIO_OUT_ZERO);
			break;
		case PIN_STA_OUT_H:
			mt6306_set_gpio_out(GPIO7, GPIO_OUT_ONE);
			break;
		case PIN_STA_OUT_L:
			mt6306_set_gpio_out(GPIO7, GPIO_OUT_ZERO);
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


static INT32 wmt_plat_uart_rx_ctrl(ENUM_PIN_STATE state)
{
	if (DEFAULT_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_num) {
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


INT32 wmt_plat_wake_lock_ctrl(ENUM_WL_OP opId)
{
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	static INT32 counter;
	INT32 ret = 0;


	ret = osal_lock_sleepable_lock(&gOsSLock);
	if (ret) {
		WMT_ERR_FUNC("--->lock gOsSLock failed, ret=%d\n", ret);
		return ret;
	}

	if (WL_OP_GET == opId)
		++counter;
	else if (WL_OP_PUT == opId)
		--counter;

	osal_unlock_sleepable_lock(&gOsSLock);
	if (WL_OP_GET == opId && counter == 1) {
		wake_lock(&wmtWakeLock);
		WMT_DBG_FUNC("WMT-PLAT: after wake_lock(%d), counter(%d)\n",
			     wake_lock_active(&wmtWakeLock), counter);

	} else if (WL_OP_PUT == opId && counter == 0) {
		wake_unlock(&wmtWakeLock);
		WMT_DBG_FUNC("WMT-PLAT: after wake_unlock(%d), counter(%d)\n",
			     wake_lock_active(&wmtWakeLock), counter);
	} else {
		WMT_WARN_FUNC("WMT-PLAT: wakelock status(%d), counter(%d)\n",
			      wake_lock_active(&wmtWakeLock), counter);
	}
	return 0;
#else
	WMT_WARN_FUNC("WMT-PLAT: host awake function is not supported.");
	return 0;

#endif
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
