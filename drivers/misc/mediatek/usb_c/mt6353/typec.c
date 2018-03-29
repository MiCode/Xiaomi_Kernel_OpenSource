/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/notifier.h>
#include <typec.h>

#include <mt-plat/upmu_common.h>
#include <mt-plat/mt_gpio.h>
/******************/
#include <mt-plat/battery_meter.h>
#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif
#include <mt-plat/mt_chip.h>
/******************/

#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include <mt-plat/mt_boot_common.h>
#endif

#include "typec_mt6353.h"
#include "typec-ioctl.h"

struct typec_hba *g_hba;
/**************************************************************************/

#if USE_AUXADC
inline void typec_auxadc_set_event(struct typec_hba *hba)
{
	hba->auxadc_event = 1;
}

inline void typec_auxadc_clear_event(struct typec_hba *hba)
{
	hba->auxadc_event = 0;
}

inline uint16_t typec_auxadc_get_value(struct typec_hba *hba)
{
}

void typec_auxadc_set_thresholds(struct typec_hba *hba, uint16_t min, uint16_t max)
{
}

void typec_auxadc_register(struct typec_hba *hba)
{
	/*register auxadc service for TYPEC controller*/

	/* config auxadc
	 *   set sample period to AUXADC_INTERVAL_MS
	 *   set debounce time to AUXADC_DEBOUNCE_TIME
	 *   set minimum value to SNK_VRPUSB_AUXADC_MIN_VAL
	 *   set maximum value to SNK_VRPUSB_AUXADC_MAX_VAL
	 */
	typec_auxadc_set_thresholds(hba, SNK_VRPUSB_AUXADC_MIN_VAL, SNK_VRPUSB_AUXADC_MAX_VAL);

	/*enable interrupts so that call back function (typec_auxadc_set_event) is invoked when
	 *    a. result is smaller than the minimum value
	 *    b. result is greater than the maximum value
	 */
}

void typec_auxadc_unregister(struct typec_hba *hba)
{
	/* unregister auxadc service */
}
#endif
/**************************************************************************/

#if 0
BLOCKING_NOTIFIER_HEAD(type_notifier_list);
void typec_notifier_register(struct notifier_block *n)
{
	blocking_notifier_chain_register(&type_notifier_list, n);
}
EXPORT_SYMBOL(typec_notifier_register);

void typec_notifier_unregister(struct notifier_block *n)
{
	blocking_notifier_chain_unregister(&type_notifier_list, n);
}
EXPORT_SYMBOL(typec_notifier_unregister);

void typec_notifier_call_chain(void)
{
	int type;
	int event;

	blocking_notifier_call_chain(&type_notifier_list, type, event);
}
#else
static struct usbtypc g_typec;
static bool g_host_connected;
static bool g_device_connected;
static uint16_t g_cc_is0;
static void typec_platform_hanlder_work(struct work_struct *work)
{
#if !CC_STANDALONE_COMPLIANCE
	if (g_cc_is0 & TYPE_C_CC_ENT_UNATTACH_SNK_INTR) {
		if (g_typec.device_driver && g_device_connected == true)
			g_typec.device_driver->disable(g_typec.device_driver->priv_data);
			g_device_connected = false;
	}
	if (g_cc_is0 & TYPE_C_CC_ENT_ATTACH_SNK_INTR) {
		if (g_typec.device_driver && g_device_connected == false)
			g_typec.device_driver->enable(g_typec.device_driver->priv_data);
			g_device_connected = true;
	}
	if (g_cc_is0 & TYPE_C_CC_ENT_UNATTACH_SRC_INTR) {
		if (g_typec.host_driver && g_host_connected == true)
			g_typec.host_driver->disable(g_typec.host_driver->priv_data);
			g_host_connected = false;
	}
	if (g_cc_is0 & TYPE_C_CC_ENT_ATTACH_SRC_INTR) {
		if (g_typec.host_driver && g_host_connected == false)
			g_typec.host_driver->enable(g_typec.host_driver->priv_data);
			g_host_connected = true;
	}
#endif
}

void typec_platform_hanlder(struct typec_hba *hba, uint16_t cc_is0)
{
	g_cc_is0 = cc_is0;
	schedule_work(&hba->platform_hanlder_work);
}

int register_typec_switch_callback(struct typec_switch_data *new_driver)
{
	if (new_driver->type == DEVICE_TYPE) {
		g_typec.device_driver = new_driver;
		g_typec.device_driver->on = 0;
		if (g_device_connected)
			g_typec.device_driver->enable(g_typec.device_driver->priv_data);
		return 0;
	}

	if (new_driver->type == HOST_TYPE) {
		g_typec.host_driver = new_driver;
		g_typec.host_driver->on = 0;
		if (g_host_connected)
			g_typec.host_driver->enable(g_typec.host_driver->priv_data);
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(register_typec_switch_callback);

int unregister_typec_switch_callback(struct typec_switch_data *new_driver)
{
	if ((new_driver->type == DEVICE_TYPE) && (g_typec.device_driver == new_driver))
		g_typec.device_driver = NULL;

	if ((new_driver->type == HOST_TYPE) && (g_typec.host_driver == new_driver))
		g_typec.host_driver = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_typec_switch_callback);
#endif
/**************************************************************************/

void typec_vbus_present(struct typec_hba *hba, uint8_t enable)
{
	typec_writew_msk(hba, TYPE_C_SW_VBUS_PRESENT,
		((enable) ? TYPE_C_SW_VBUS_PRESENT : 0), TYPE_C_CC_SW_CTRL);

	if (hba->dbg_lvl >= TYPEC_DBG_LVL_2 && (hba->vbus_present != enable))
		dev_err(hba->dev, "VBUS_PRESENT %s", ((enable) ? "ON" : "OFF"));

	hba->vbus_present = (enable ? 1 : 0);
	typec_sw_probe(hba, DBG_VBUS_PRESENT, (enable<<DBG_VBUS_PRESENT_OFST));
}

void typec_vbus_det_enable(struct typec_hba *hba, uint8_t enable)
{
	if (hba->dbg_lvl >= TYPEC_DBG_LVL_2 && (hba->vbus_det_en != enable))
		dev_err(hba->dev, "VBUS_DET %s", ((enable) ? "ON" : "OFF"));

	hba->vbus_det_en = (enable ? 1 : 0);
	typec_sw_probe(hba, DBG_VBUS_DET_EN, (enable<<DBG_VBUS_DET_EN_OFST));
}

#if FPGA_PLATFORM
int typec_is_vbus_present(struct typec_hba *hba, enum enum_vbus_lvl lvl)
{
	uint16_t tmp;

	tmp = typec_readw(hba, TYPE_C_FPGA_STATUS);
	#if DETECT_VSAFE_0V
	if (((lvl == TYPEC_VSAFE_5V) && !(tmp & TYPE_C_FPGA_VBUS_VSAFE_5V_MON_N))
		|| ((lvl == TYPEC_VSAFE_0V) && !(tmp & TYPE_C_FPGA_VBUS_VSAFE_0V_MON_N)))
		return 1;
	#else
	if (((lvl == TYPEC_VSAFE_5V) && !(tmp & TYPE_C_FPGA_VBUS_VSAFE_5V_MON_N))
		|| ((lvl == TYPEC_VSAFE_0V) && (tmp & TYPE_C_FPGA_VBUS_VSAFE_5V_MON_N)))
		return 1;
	#endif
	else
		return 0;
}

void typec_drive_vbus(struct typec_hba *hba, uint8_t on)
{
	typec_writew_msk(hba, TYPE_C_FPGA_VBSU_PWR_EN,
		(on ? TYPE_C_FPGA_VBSU_PWR_EN : 0), TYPE_C_FPGA_CTRL);

	if ((hba->dbg_lvl >= TYPEC_DBG_LVL_2) && (hba->vbus_en != on))
		dev_err(hba->dev, "VBUS %s", (on ? "ON" : "OFF"));

	hba->vbus_en = (on ? 1 : 0);
	typec_sw_probe(hba, DBG_VBUS_EN, (on<<DBG_VBUS_EN_OFST));
}

#if SUPPORT_PD
void typec_drive_vconn(struct typec_hba *hba, uint8_t on)
{
	typec_writew_msk(hba, TYPE_C_SW_DA_DRIVE_VCONN_EN,
		(on ? TYPE_C_SW_DA_DRIVE_VCONN_EN : 0), TYPE_C_CC_SW_CTRL);

	if ((hba->dbg_lvl >= TYPEC_DBG_LVL_2) && (hba->vconn_en != on))
		dev_err(hba->dev, "VCONN %s", (on ? "ON" : "OFF"));

	hba->vconn_en = (on ? 1 : 0);
	typec_sw_probe(hba, DBG_VCONN_EN, (on<<DBG_VCONN_EN_OFST));
}
#endif
#else
int typec_is_vbus_present(struct typec_hba *hba, enum enum_vbus_lvl lvl)
{
	if (((lvl == TYPEC_VSAFE_5V) && (upmu_get_rgs_chrdet()))
		|| ((lvl == TYPEC_VSAFE_0V) && !(upmu_get_rgs_chrdet())))
		return 1;
	else
		return 0;
}

#if CC_STANDALONE_COMPLIANCE
static uint8_t enable_boost;
#endif
static uint8_t drive_vbus_action;
static void typec_drive_vbus_work(struct work_struct *work)
{
	struct typec_hba *hba = container_of(work, struct typec_hba, drive_vbus_work);

#if CC_STANDALONE_COMPLIANCE
	#if defined(CONFIG_MTK_BQ25896_SUPPORT)
	#ifdef CONFIG_MTK_LEGACY
		mt_set_gpio_mode(GPIO_OTG_DRVVBUS_PIN, GPIO_OTG_DRVVBUS_PIN_M_GPIO);
		mt_set_gpio_out(GPIO_OTG_DRVVBUS_PIN, GPIO_OUT_ONE);
	#else
		mt_set_gpio_mode(0x80000043, 1);
		mt_set_gpio_out(0x80000043, GPIO_OUT_ONE);
	#endif
		if (enable_boost != drive_vbus_action) {
			enable_boost = drive_vbus_action;
			bq25890_otg_en((drive_vbus_action ? 1 : 0));
			bq25890_set_boost_ilim(0x03);	/* 1.3A */
		}
	#elif defined(CONFIG_MTK_OTG_PMIC_BOOST_5V)
		mtk_enable_pmic_otg_mode();
	#endif
#endif
	if ((hba->dbg_lvl >= TYPEC_DBG_LVL_2) && (hba->vbus_en != drive_vbus_action))
		dev_err(hba->dev, "VBUS %s", (drive_vbus_action ? "ON" : "OFF"));

	hba->vbus_en = (drive_vbus_action ? 1 : 0);
	typec_sw_probe(hba, DBG_VBUS_EN, (drive_vbus_action<<DBG_VBUS_EN_OFST));
}

void typec_drive_vbus(struct typec_hba *hba, uint8_t on)
{
	drive_vbus_action = on;
	schedule_work(&hba->drive_vbus_work);
}

#if SUPPORT_PD
void typec_drive_vconn(struct typec_hba *hba, uint8_t on)
{
}
#endif
#endif

/**************************************************************************/

static void typec_set_default_param(struct typec_hba *hba)
{
	uint32_t val;


	/* timing parameters */
	val = ZERO_INDEXED_DIV_AND_RND_UP(REF_CLK_DIVIDEND * CC_VOL_PERIODIC_MEAS_VAL, REF_CLK_DIVIDER);
	typec_writew(hba, val, TYPE_C_CC_VOL_PERIODIC_MEAS_VAL);
	val = ZERO_INDEXED_DIV_AND_RND_UP(REF_CLK_DIVIDEND * DRP_SRC_CNT_VAL, REF_CLK_DIVIDER);
	typec_writew(hba, val, TYPE_C_DRP_SRC_CNT_VAL_0);
	val = ZERO_INDEXED_DIV_AND_RND_UP(REF_CLK_DIVIDEND * DRP_SNK_CNT_VAL, REF_CLK_DIVIDER);
	typec_writew(hba, val, TYPE_C_DRP_SNK_CNT_VAL_0);
	val = ZERO_INDEXED_DIV_AND_RND_UP(REF_CLK_DIVIDEND * DRP_TRY_CNT_VAL, REF_CLK_DIVIDER);
	typec_writew(hba, val, TYPE_C_DRP_TRY_CNT_VAL_0);
	val = ZERO_INDEXED_DIV_AND_RND_UP(REF_CLK_DIVIDEND * DRP_TRY_WAIT_CNT_VAL, REF_CLK_DIVIDER);
	typec_writew(hba, val & 0xffff, TYPE_C_DRP_TRY_WAIT_CNT_VAL_0);
	typec_writew(hba, (val>>16), TYPE_C_DRP_TRY_WAIT_CNT_VAL_1);


	/* SRC reference voltages */
	typec_writew(hba, (SRC_VRD_DEFAULT_DAC_VAL<<REG_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL_OFST |
		SRC_VOPEN_DEFAULT_DAC_VAL<<REG_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL_OFST),
		TYPE_C_CC_SRC_DEFAULT_DAC_VAL);
	typec_writew(hba, (SRC_VRD_15_DAC_VAL<<REG_TYPE_C_CC_SRC_VRD_15_DAC_VAL_OFST |
		SRC_VOPEN_15_DAC_VAL<<REG_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL_OFST), TYPE_C_CC_SRC_15_DAC_VAL);
	typec_writew(hba, (SRC_VRD_30_DAC_VAL<<REG_TYPE_C_CC_SRC_VRD_30_DAC_VAL_OFST |
		SRC_VOPEN_30_DAC_VAL<<REG_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL_OFST), TYPE_C_CC_SRC_30_DAC_VAL);


	/* SNK reference voltages */
	typec_writew(hba, (SNK_VRP15_DAC_VAL<<REG_TYPE_C_CC_SNK_VRP15_DAC_VAL_OFST |
		SNK_VRP30_DAC_VAL<<REG_TYPE_C_CC_SNK_VRP30_DAC_VAL_OFST), TYPE_C_CC_SNK_DAC_VAL_0);
	typec_writew(hba, SNK_VRPUSB_DAC_VAL<<REG_TYPE_C_CC_SNK_VRPUSB_DAC_VAL_OFST, TYPE_C_CC_SNK_DAC_VAL_1);


	/* mode configuration */
	/* AUXADC or DAC (preferred) */
	#if USE_AUXADC
	typec_set(hba, REG_TYPE_C_ADC_EN, TYPE_C_CTRL);
	#else
	typec_clear(hba, REG_TYPE_C_ADC_EN, TYPE_C_CTRL);
	#endif
	/* termination decided by controller */
	typec_clear(hba, TYPEC_TERM_CC, TYPE_C_CC_SW_FORCE_MODE_ENABLE);
	/* enable/disable accessory mode */
	#if ENABLE_ACC
	typec_set(hba, TYPEC_ACC_EN, TYPE_C_CTRL);
	#else
	typec_clear(hba, TYPEC_ACC_EN, TYPE_C_CTRL);
	#endif


	#if DBG_PROBE
	/* debug probe setting */
	typec_writew(hba, 0x0000, REG_TYPE_C_DBG_MOD_SEL); /* typec debug signal */
	typec_writew(hba, 0x2423, TYPE_C_DEBUG_PORT_SELECT_0);
	typec_writew(hba, 0x2625, TYPE_C_DEBUG_PORT_SELECT_1);
	#endif
}

/**************************************************************************/

int typec_enable(struct typec_hba *hba, int enable)
{
	if (enable) {
		/*#if SUPPORT_PD*/
		typec_vbus_det_enable(hba, 1);
		/*#endif*/

		switch (hba->support_role) {
		case TYPEC_ROLE_SINK:
			typec_set(hba, W1_TYPE_C_SW_ENT_UNATCH_SNK_CMD, TYPE_C_CC_SW_CTRL);
			break;
		case TYPEC_ROLE_SOURCE:
		case TYPEC_ROLE_DRP:
			typec_set(hba, W1_TYPE_C_SW_ENT_UNATCH_SRC_CMD, TYPE_C_CC_SW_CTRL);
			break;
		default:
			return 1;
		}
	} else {
		/*#if SUPPORT_PD*/
		typec_vbus_det_enable(hba, 0);
		/*#endif*/

		typec_set(hba, W1_TYPE_C_SW_ENT_DISABLE_CMD, TYPE_C_CC_SW_CTRL);
		typec_clear(hba, TYPEC_TERM_CC, TYPE_C_CC_SW_FORCE_MODE_ENABLE);
		typec_clear(hba, (TYPEC_TERM_CC1 | TYPEC_TERM_CC2), TYPE_C_CC_SW_FORCE_MODE_VAL_0);
	}


	return 0;
}

void typec_select_rp(struct typec_hba *hba, enum enum_typec_rp rp_val)
{
	typec_writew_msk(hba, TYPE_C_PHY_RG_CC_RP_SEL, rp_val, TYPE_C_PHY_RG_0);
}

void typec_force_term(struct typec_hba *hba, enum enum_typec_term cc1_term, enum enum_typec_term cc2_term)
{
	uint32_t val;

	val = 0;
	val |= (cc1_term == TYPEC_TERM_RP) ? REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC1_EN :
		(cc1_term == TYPEC_TERM_RD) ? REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC1_EN :
		(cc1_term == TYPEC_TERM_RA) ? REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC1_EN : 0;
	val |= (cc2_term == TYPEC_TERM_RP) ? REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC2_EN :
		(cc2_term == TYPEC_TERM_RD) ? REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC2_EN :
		(cc2_term == TYPEC_TERM_RA) ? REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC2_EN : 0;

	typec_set(hba, TYPEC_TERM_CC, TYPE_C_CC_SW_FORCE_MODE_ENABLE);
	typec_writew_msk(hba, (TYPEC_TERM_CC1 | TYPEC_TERM_CC2), val, TYPE_C_CC_SW_FORCE_MODE_VAL_0);
}

int typec_set_mode(struct typec_hba *hba, enum enum_typec_role role, int param1, int param2)
{
	enum enum_typec_rp rp_val = param1;
	enum enum_try_mode try_mode = param2;


	/*go back to disable state*/
	typec_enable(hba, 0);

	switch (role) {
	case TYPEC_ROLE_SINK_W_ACTIVE_CABLE:
		if (param1 == 1)
			typec_force_term(hba, TYPEC_TERM_RD, TYPEC_TERM_RA);
		else if (param1 == 2)
			typec_force_term(hba, TYPEC_TERM_RA, TYPEC_TERM_RD);
		else
			goto err_handle;
		break;

	case TYPEC_ROLE_SINK:
		typec_writew_msk(hba, REG_TYPE_C_PORT_SUPPORT_ROLE, TYPEC_ROLE_SINK, TYPE_C_CTRL);
		break;

	case TYPEC_ROLE_SOURCE:
		typec_writew_msk(hba, REG_TYPE_C_PORT_SUPPORT_ROLE, role, TYPE_C_CTRL);
		typec_select_rp(hba, rp_val);
		break;

	case TYPEC_ROLE_DRP:
		typec_writew_msk(hba, REG_TYPE_C_PORT_SUPPORT_ROLE, role, TYPE_C_CTRL);
		typec_select_rp(hba, rp_val);

		switch (try_mode) {
		case TYPEC_TRY_DISABLE:
			typec_clear(hba, TYPEC_TRY, TYPE_C_CTRL);
			break;
		case TYPEC_TRY_ENABLE:
			typec_set(hba, TYPEC_TRY, TYPE_C_CTRL);
			break;
		default:
			goto err_handle;
		}
		break;

	case TYPEC_ROLE_ACCESSORY_AUDIO:
		typec_force_term(hba, TYPEC_TERM_RA, TYPEC_TERM_RA);
		break;

	case TYPEC_ROLE_ACCESSORY_DEBUG:
		typec_force_term(hba, TYPEC_TERM_RD, TYPEC_TERM_RD);
		break;

	case TYPEC_ROLE_OPEN:
		typec_force_term(hba, TYPEC_TERM_NA, TYPEC_TERM_NA);
		break;

	default:
		goto err_handle;
	}

	hba->support_role = role;
	hba->rp_val = rp_val;

	return 0;


err_handle:
	return 1;
}

static void typec_show_routed_cc(struct typec_hba *hba)
{
	dev_err(hba->dev, "CC%d\n", (((typec_readw(hba, TYPE_C_CC_STATUS) & RO_TYPE_C_ROUTED_CC) == 0) ? 1 : 2));
}

/**************************************************************************/

static void typec_wait_vbus_on_try_wait_snk(struct work_struct *work)
{
	struct typec_hba *hba = container_of(work, struct typec_hba, wait_vbus_on_try_wait_snk);

	while ((typec_readw(hba, TYPE_C_CC_STATUS) & RO_TYPE_C_CC_ST) == TYPEC_STATE_TRY_WAIT_SNK) {
		if (hba->vbus_det_en && typec_is_vbus_present(hba, TYPEC_VSAFE_5V)) {
			typec_vbus_present(hba, 1);
			dev_err(hba->dev, "Vbus ON in TryWait.SNK state\n");

			return;
		}
		msleep(POLLING_INTERVAL_MS);
	}

}

static void typec_wait_vbus_on_attach_wait_snk(struct work_struct *work)
{
	struct typec_hba *hba = container_of(work, struct typec_hba, wait_vbus_on_attach_wait_snk);

	while ((typec_readw(hba, TYPE_C_CC_STATUS) & RO_TYPE_C_CC_ST) == TYPEC_STATE_ATTACH_WAIT_SNK) {
		if (hba->vbus_det_en && typec_is_vbus_present(hba, TYPEC_VSAFE_5V)) {
			typec_vbus_present(hba, 1);
			dev_err(hba->dev, "Vbus ON in AttachWait.SNK state\n");

			return;
		}
		msleep(POLLING_INTERVAL_MS);
	}
}

static void typec_wait_vbus_off_attached_snk(struct work_struct *work)
{
	struct typec_hba *hba = container_of(work, struct typec_hba, wait_vbus_off_attached_snk);

	while ((typec_readw(hba, TYPE_C_CC_STATUS) & RO_TYPE_C_CC_ST) == TYPEC_STATE_ATTACHED_SNK) {
		if (hba->vbus_det_en && !typec_is_vbus_present(hba, TYPEC_VSAFE_5V)) {
			typec_vbus_present(hba, 0);
			dev_err(hba->dev, "Vbus OFF in Attachd.SNK state\n");

			return;
		}
		msleep(POLLING_INTERVAL_MS);
	}
}

static void typec_wait_vbus_off_then_drive_attached_src(struct work_struct *work)
{
	struct typec_hba *hba = container_of(work, struct typec_hba, wait_vbus_off_then_drive_attached_src);

	while ((typec_readw(hba, TYPE_C_CC_STATUS) & RO_TYPE_C_CC_ST) == TYPEC_STATE_ATTACHED_SRC) {
		if (hba->vbus_det_en && typec_is_vbus_present(hba, TYPEC_VSAFE_0V)) {
			typec_drive_vbus(hba, 1);
			return;
		}

		msleep(POLLING_INTERVAL_MS);
	}
}

#if USE_AUXADC
static int typec_auxadc_voltage_mon_attached_snk(struct work_struct *work)
{
	uint16_t tmp;
	struct typec_hba *hba = container_of(work, struct typec_hba, auxadc_voltage_mon_attached_snk);

	/*register AUXADC service*/
	typec_auxadc_register(hba);

	/*polling for AUXADC event*/
	while ((typec_readw(hba, TYPE_C_CC_STATUS) & RO_TYPE_C_CC_ST) == TYPEC_STATE_ATTACHED_SNK) {
		if (hba->auxadc_event) {
			/*get AUXADC value*/
			tmp = typec_auxadc_get_value(hba);

			/*SNK_VRP30_AUXADC_MIN_VAL < tmp <= SNK_VRP30_AUXADC_MAX_VAL*/
			if (tmp > SNK_VRP30_AUXADC_MIN_VAL) {
				typec_set(hba, W1_TYPE_C_SW_ADC_RESULT_MET_VRD_30_CMD, TYPE_C_CC_SW_CTRL);
				typec_auxadc_set_thresholds(hba, SNK_VRP30_AUXADC_MIN_VAL, SNK_VRP30_AUXADC_MAX_VAL);
			}
			/*SNK_VRP15_AUXADC_MIN_VAL < tmp <= SNK_VRP15_AUXADC_MAX_VAL*/
			else if (tmp > SNK_VRP15_AUXADC_MIN_VAL) {
				typec_set(hba, W1_TYPE_C_SW_ADC_RESULT_MET_VRD_15_CMD, TYPE_C_CC_SW_CTRL);
				typec_auxadc_set_thresholds(hba, SNK_VRP15_AUXADC_MIN_VAL, SNK_VRP15_AUXADC_MAX_VAL);
			}
			/*SNK_VRPUSB_AUXADC_MIN_VAL <= tmp <= SNK_VRPUSB_AUXADC_MAX_VAL*/
			else {
				typec_set(hba, W1_TYPE_C_SW_ADC_RESULT_MET_VRD_DEFAULT_CMD, TYPE_C_CC_SW_CTRL);
				typec_auxadc_set_thresholds(hba, SNK_VRPUSB_AUXADC_MIN_VAL, SNK_VRPUSB_AUXADC_MAX_VAL);
			}

			/*clear event*/
			typec_auxadc_clear_event(hba);
		}

		msleep(AUXADC_EVENT_INTERVAL_MS);
	}


	/*unregister AUXADC service*/
	typec_auxadc_unregister(hba);

	return 0;
}
#endif

/**************************************************************************/

void typec_int_enable(struct typec_hba *hba, uint16_t msk0, uint16_t msk2)
{
	typec_set(hba, msk0, TYPE_C_INTR_EN_0);
	typec_set(hba, msk2, TYPE_C_INTR_EN_2);
}

void typec_int_disable(struct typec_hba *hba, uint16_t msk0, uint16_t msk2)
{
	typec_clear(hba, msk0, TYPE_C_INTR_EN_0);
	typec_clear(hba, msk2, TYPE_C_INTR_EN_2);
}

struct bit_mapping is0_mapping[] = {

	{TYPE_C_CC_ENT_ATTACH_SRC_INTR, "CC_ENT_ATTACH_SRC"},
	{TYPE_C_CC_ENT_ATTACH_SNK_INTR, "CC_ENT_ATTACH_SNK"},
	#if ENABLE_ACC
	{TYPE_C_CC_ENT_AUDIO_ACC_INTR, "CC_ENT_AUDIO_ACC"},
	{TYPE_C_CC_ENT_DBG_ACC_INTR, "CC_ENT_DBG_ACC"},
	#endif
	{TYPE_C_CC_ENT_TRY_SRC_INTR, "CC_ENT_TRY_SRC"},
	{TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR, "CC_ENT_TRY_WAIT_SNK"},
};
struct bit_mapping is0_mapping_dbg[] = {

	{TYPE_C_CC_ENT_DISABLE_INTR, "CC_ENT_DISABLE"},
	{TYPE_C_CC_ENT_UNATTACH_SRC_INTR, "CC_ENT_UNATTACH_SRC"},
	{TYPE_C_CC_ENT_UNATTACH_SNK_INTR, "CC_ENT_UNATTACH_SNK"},
	{TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR, "CC_ENT_ATTACH_WAIT_SRC"},
	{TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR, "CC_ENT_ATTACH_WAIT_SNK"},
	#if ENABLE_ACC
	{TYPE_C_CC_ENT_UNATTACH_ACC_INTR, "CC_ENT_UNATTACH_ACC"},
	{TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR, "CC_ENT_ATTACH_WAIT_ACC"},
	#endif
};
struct bit_mapping is2_mapping[] = {

	{TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR, "CC_ENT_SNK_PWR_DEFAULT"},
	{TYPE_C_CC_ENT_SNK_PWR_15_INTR, "CC_ENT_SNK_PWR_15"},
	{TYPE_C_CC_ENT_SNK_PWR_30_INTR, "CC_ENT_SNK_PWR_30"},
};
struct bit_mapping is2_mapping_dbg[] = {

	{TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR, "CC_ENT_SNK_PWR_REDETECT"},
	{TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR, "CC_ENT_SNK_PWR_IDLE"},
};
static void typec_dump_intr(struct typec_hba *hba, uint16_t is0, uint16_t is2)
{
	int i;

	if (hba->dbg_lvl >= TYPEC_DBG_LVL_1) {
		for (i = 0; i < sizeof(is0_mapping)/sizeof(struct bit_mapping); i++) {
			if (is0 & is0_mapping[i].mask)
				dev_err(hba->dev, "%s\n", is0_mapping[i].name);
		}

		for (i = 0; i < sizeof(is2_mapping)/sizeof(struct bit_mapping); i++) {
			if (is2 & is2_mapping[i].mask)
				dev_err(hba->dev, "%s\n", is2_mapping[i].name);
		}
	}

	if (hba->dbg_lvl >= TYPEC_DBG_LVL_3) {
		for (i = 0; i < sizeof(is0_mapping_dbg)/sizeof(struct bit_mapping); i++) {
			if (is0 & is0_mapping_dbg[i].mask)
				dev_err(hba->dev, "%s\n", is0_mapping_dbg[i].name);
		}

		for (i = 0; i < sizeof(is2_mapping_dbg)/sizeof(struct bit_mapping); i++) {
			if (is2 & is2_mapping_dbg[i].mask)
				dev_err(hba->dev, "%s\n", is2_mapping_dbg[i].name);
		}
	}
}

/**
 * typec_intr - Interrupt service routine
 * @hba: per adapter instance
 * @is0: interrupt status 0
 * @is2: interrupt status 2
 */
static void typec_intr(struct typec_hba *hba, uint16_t cc_is0, uint16_t cc_is2)
{
	#if ENABLE_ACC
	uint16_t toggle = TYPE_C_INTR_DRP_TOGGLE | TYPE_C_INTR_ACC_TOGGLE;
	#else
	uint16_t toggle = TYPE_C_INTR_DRP_TOGGLE;
	#endif


	/*dump interrupt information*/
	typec_dump_intr(hba, cc_is0, cc_is2);

	/*process usb device/host function*/
	typec_platform_hanlder(hba, cc_is0);

	/*serve interrupts according to power role*/
	/*TODO: move to main loop*/
	if (cc_is0 & (TYPE_C_CC_ENT_DISABLE_INTR | toggle)) {
		/*ignore SNK<->SRC & SNK<->ACC*/
#if 0
		/* this case may lost interrupt.
		 * Example,
		 * In ATTACH_SRC case, user plugout cable and HW issue UNATTACH_SNK interrupt first.
		 * We lost UNATTACH_SRC if you disable all.
		 */
		typec_int_disable(hba, toggle, 0);
#else
		/* disable toggle interrupt which issued already. */
		typec_int_disable(hba, toggle & cc_is0, 0);
#endif

		typec_vbus_present(hba, 0);
		typec_drive_vbus(hba, 0);
	}

	if (cc_is0 & TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR)
		schedule_work(&hba->wait_vbus_on_attach_wait_snk);

	#if ENABLE_ACC
	if (cc_is0 & ((TYPE_C_CC_ENT_ATTACH_SNK_INTR | TYPE_C_CC_ENT_ATTACH_SRC_INTR) |
			(TYPE_C_CC_ENT_AUDIO_ACC_INTR | TYPE_C_CC_ENT_DBG_ACC_INTR))) {
	#else
	if (cc_is0 & (TYPE_C_CC_ENT_ATTACH_SNK_INTR | TYPE_C_CC_ENT_ATTACH_SRC_INTR)) {
	#endif
		#if ENABLE_ACC
		/*SNK<->ACC toggle happens ONLY for sink*/
		if (hba->support_role == TYPEC_ROLE_SINK)
			typec_int_enable(hba, TYPE_C_INTR_ACC_TOGGLE, 0);
		else
		#endif
			typec_int_enable(hba, TYPE_C_INTR_DRP_TOGGLE, 0);
	}

	if (cc_is0 & TYPE_C_CC_ENT_ATTACH_SNK_INTR) {
		typec_show_routed_cc(hba);
		schedule_work(&hba->wait_vbus_off_attached_snk);
		#if USE_AUXADC
		schedule_work(&hba->auxadc_voltage_mon_attached_snk);
		#endif
	}

	if (cc_is0 & TYPE_C_CC_ENT_ATTACH_SRC_INTR) {
		typec_show_routed_cc(hba);
		schedule_work(&hba->wait_vbus_off_then_drive_attached_src);
	}

	/*transition from Attached.SRC to TryWait.SNK*/
	if (cc_is0 & TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR) {
		typec_drive_vbus(hba, 0);
		schedule_work(&hba->wait_vbus_on_try_wait_snk);
	}
}

/**************************************************************************/

/**
 * typec_top_intr - main interrupt service routine
 * @irq: irq number
 * @__hba: pointer to adapter instance
 *
 * Returns IRQ_HANDLED - If interrupt is valid
 *		IRQ_NONE - If invalid interrupt
 */
static irqreturn_t typec_top_intr(int irq, void *__hba)
{
	uint16_t cc_is0, cc_is2;
	#if SUPPORT_PD
	uint16_t pd_is0, pd_is1;
	#endif
	uint16_t handled;
	irqreturn_t retval = IRQ_NONE;
	/*struct typec_hba *hba = __hba;*/
	struct typec_hba *hba = g_hba;


	spin_lock(&hba->typec_lock);

	/* TYPEC */
	typec_sw_probe(hba, DBG_INTR_STATE, (DBG_INTR_CC<<DBG_INTR_STATE_OFST));

	cc_is0 = typec_readw(hba, TYPE_C_INTR_0);
	cc_is2 = typec_readw(hba, TYPE_C_INTR_2);
	typec_writew(hba, cc_is0, TYPE_C_INTR_0);
	typec_writew(hba, cc_is2, TYPE_C_INTR_2);

	if (cc_is0 | cc_is2)
		typec_intr(hba, cc_is0, cc_is2);
	handled = (cc_is0 | cc_is2);


	#if SUPPORT_PD
	/* PD */
	typec_sw_probe(hba, DBG_INTR_STATE, (DBG_INTR_PD<<DBG_INTR_STATE_OFST));

	pd_is0 = typec_readw(hba, PD_INTR_0);
	pd_is1 = typec_readw(hba, PD_INTR_1);
	#if PD_SW_WORKAROUND1_2
	if (pd_is0 & PD_RX_RCV_MSG_INTR)
		typec_clear(hba, REG_PD_RX_RCV_MSG_INTR_EN, PD_INTR_EN_0);
	typec_writew(hba, (pd_is0 & ~PD_RX_RCV_MSG_INTR), PD_INTR_0);
	#else
	typec_writew(hba, pd_is0, PD_INTR_0);
	#endif
	typec_writew(hba, pd_is1, PD_INTR_1);

	cc_is0 &= PD_INTR_IS0_LISTEN;
	if (pd_is0 | pd_is1 | cc_is0)
		pd_intr(hba, pd_is0, pd_is1, cc_is0, 0);
	handled |= (pd_is0 | pd_is1);
	#endif


	/*check if at least 1 interrupt has been served this time*/
	typec_sw_probe(hba, DBG_INTR_STATE, (DBG_INTR_NONE<<DBG_INTR_STATE_OFST));
	if (handled)
		retval = IRQ_HANDLED;

	spin_unlock(&hba->typec_lock);


	return retval;
}

/**************************************************************************/

/**
 * typec_init - Driver initialization routine
 * @dev: pointer to device handle
 * @hba_handle: driver private handle
 * @mmio_base: base register address
 * @irq: Interrupt line of device
 * @id: device id
 * Returns 0 on success, non-zero value on failure
 */

void typec_hanlder(void)
{
	typec_top_intr(0, 0);
}
int typec_init(struct device *dev, struct typec_hba **hba_handle,
		void __iomem *mmio_base, unsigned int irq, int id)
{
	int err;
	struct typec_hba *hba;

	hba = kzalloc(sizeof(struct typec_hba), GFP_KERNEL);

	/*check arguments*/
	if (!dev) {
		dev_err(dev,
		"Invalid memory reference for dev is NULL\n");
		err = -ENODEV;
		goto out_error;
	}

#if FPGA_PLATFORM
	if (!mmio_base) {
		dev_err(dev,
		"Invalid memory reference for mmio_base is NULL\n");
		err = -ENODEV;
		goto out_error;
	}
#endif

	/*initialize controller data*/
	hba->dev = dev;
	hba->mmio_base = mmio_base;
	hba->irq = irq;
	hba->id = id;
	g_hba = hba;

	mutex_init(&hba->ioctl_lock);
	spin_lock_init(&hba->typec_lock);

	INIT_WORK(&hba->wait_vbus_on_attach_wait_snk, typec_wait_vbus_on_attach_wait_snk);
	INIT_WORK(&hba->wait_vbus_on_try_wait_snk, typec_wait_vbus_on_try_wait_snk);
	INIT_WORK(&hba->wait_vbus_off_attached_snk, typec_wait_vbus_off_attached_snk);
	INIT_WORK(&hba->wait_vbus_off_then_drive_attached_src, typec_wait_vbus_off_then_drive_attached_src);
#if !(FPGA_PLATFORM)
	INIT_WORK(&hba->drive_vbus_work, typec_drive_vbus_work);
	INIT_WORK(&hba->platform_hanlder_work, typec_platform_hanlder_work);
#endif
	#if USE_AUXADC
	INIT_WORK(&hba->auxadc_voltage_mon_attached_snk, typec_auxadc_voltage_mon_attached_snk);
	#endif


	/*register IRQ*/
#if FPGA_PLATFORM
	dev_dbg(dev, "IRQ registration\n");
	err = devm_request_irq(dev, irq, typec_top_intr, IRQF_SHARED | IRQF_TRIGGER_LOW, TYPEC, hba);
	if (err) {
		dev_err(hba->dev, "request irq failed\n");
		goto out_error;
	}
#endif

#if TYPEC_CLI
	/*create character device for CLI*/
	err = typec_cdev_init(dev, hba, id);
#endif

	/*initialize TYPEC*/
	typec_set_default_param(hba);
	typec_int_enable(hba, TYPE_C_INTR_EN_0_MSK, TYPE_C_INTR_EN_2_MSK);

	hba->support_role = TYPEC_ROLE_DRP;
	hba->rp_val = TYPEC_RP_DFT;
	hba->dbg_lvl = TYPEC_DBG_LVL_3;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT || get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT)
		hba->support_role = TYPEC_ROLE_SINK;
#endif

	#if SUPPORT_PD
	/*initialize PD*/
	pd_init(hba);
	#endif


	/*initialization completes*/
	*hba_handle = hba;

	typec_set_mode(hba, hba->support_role, hba->rp_val, 0);
	typec_enable(hba, 1);

	return 0;


out_error:
	kfree(hba);

	return err;
}
EXPORT_SYMBOL_GPL(typec_init);

/**
 * typec_remove - de-allocate data structure memory
 * @hba - per adapter instance
 */
void typec_remove(struct typec_hba *hba)
{
#if TYPEC_CLI
	typec_cdev_remove(hba);
#endif
	kfree(hba);

	/* disable interrupts */
	typec_int_disable(hba, TYPE_C_INTR_EN_0_MSK, TYPE_C_INTR_EN_2_MSK);
}
EXPORT_SYMBOL_GPL(typec_remove);

/**************************************************************************/

/**
 * typec_suspend - suspend power management function
 * @hba: per adapter instance
 * @state: power state
 *
 * Returns -ENOSYS
 */
int typec_suspend(struct typec_hba *hba, pm_message_t state)
{
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(typec_suspend);

/**
 * typec_resume - resume power management function
 * @hba: per adapter instance
 *
 * Returns -ENOSYS
 */
int typec_resume(struct typec_hba *hba)
{
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(typec_resume);

int typec_runtime_suspend(struct typec_hba *hba)
{
	return 0;
}
EXPORT_SYMBOL(typec_runtime_suspend);

int typec_runtime_resume(struct typec_hba *hba)
{
	return 0;
}
EXPORT_SYMBOL(typec_runtime_resume);

int typec_runtime_idle(struct typec_hba *hba)
{
	return 0;
}
EXPORT_SYMBOL(typec_runtime_idle);

/**************************************************************************/

/**
 * typec_exit - Driver init routine
 */
static int __init typec_module_init(void)
{
	int err;
#if TYPEC_CLI
	err = typec_cdev_module_init();
	if (err)
		return err;
#endif
	err = typec_pltfrm_init();
	if (err)
		goto err_handle;

	return 0;


err_handle:
#if TYPEC_CLI
	typec_cdev_module_exit();
#endif

	return err;
}
module_init(typec_module_init);

/**
 * typec_exit - Driver exit clean-up routine
 */
static void __exit typec_module_exit(void)
{
	typec_pltfrm_exit();
#if TYPEC_CLI
	typec_cdev_module_exit();
#endif
}
module_exit(typec_module_exit);

/**************************************************************************/
