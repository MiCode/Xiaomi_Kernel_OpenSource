/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-qpnp-flash.h>
#include <linux/leds-qpnp-flash-v2.h>
#include "leds.h"

#define	FLASH_LED_REG_LED_STATUS1(base)		(base + 0x08)
#define	FLASH_LED_REG_LED_STATUS2(base)		(base + 0x09)
#define	FLASH_LED_REG_INT_RT_STS(base)		(base + 0x10)
#define	FLASH_LED_REG_SAFETY_TMR(base)		(base + 0x40)
#define	FLASH_LED_REG_TGR_CURRENT(base)		(base + 0x43)
#define	FLASH_LED_REG_MOD_CTRL(base)		(base + 0x46)
#define	FLASH_LED_REG_IRES(base)		(base + 0x47)
#define	FLASH_LED_REG_STROBE_CFG(base)		(base + 0x48)
#define	FLASH_LED_REG_STROBE_CTRL(base)		(base + 0x49)
#define	FLASH_LED_EN_LED_CTRL(base)		(base + 0x4C)
#define	FLASH_LED_REG_HDRM_PRGM(base)		(base + 0x4D)
#define	FLASH_LED_REG_HDRM_AUTO_MODE_CTRL(base)	(base + 0x50)
#define	FLASH_LED_REG_WARMUP_DELAY(base)	(base + 0x51)
#define	FLASH_LED_REG_ISC_DELAY(base)		(base + 0x52)
#define	FLASH_LED_REG_THERMAL_THRSH1(base)	(base + 0x56)
#define	FLASH_LED_REG_THERMAL_THRSH2(base)	(base + 0x57)
#define	FLASH_LED_REG_THERMAL_THRSH3(base)	(base + 0x58)
#define	FLASH_LED_REG_VPH_DROOP_THRESHOLD(base)	(base + 0x61)
#define	FLASH_LED_REG_VPH_DROOP_DEBOUNCE(base)	(base + 0x62)
#define	FLASH_LED_REG_ILED_GRT_THRSH(base)	(base + 0x67)
#define	FLASH_LED_REG_MITIGATION_SEL(base)	(base + 0x6E)
#define	FLASH_LED_REG_MITIGATION_SW(base)	(base + 0x6F)
#define	FLASH_LED_REG_LMH_LEVEL(base)		(base + 0x70)
#define	FLASH_LED_REG_CURRENT_DERATE_EN(base)	(base + 0x76)

#define	FLASH_LED_HDRM_MODE_PRGM_MASK		GENMASK(7, 0)
#define	FLASH_LED_HDRM_VOL_MASK			GENMASK(7, 4)
#define	FLASH_LED_CURRENT_MASK			GENMASK(6, 0)
#define	FLASH_LED_ENABLE_MASK			GENMASK(2, 0)
#define	FLASH_LED_SAFETY_TMR_MASK		GENMASK(7, 0)
#define	FLASH_LED_INT_RT_STS_MASK		GENMASK(7, 0)
#define	FLASH_LED_ISC_WARMUP_DELAY_MASK		GENMASK(1, 0)
#define	FLASH_LED_CURRENT_DERATE_EN_MASK	GENMASK(2, 0)
#define	FLASH_LED_VPH_DROOP_DEBOUNCE_MASK	GENMASK(1, 0)
#define	FLASH_LED_CHGR_MITIGATION_SEL_MASK	GENMASK(5, 4)
#define	FLASH_LED_LMH_MITIGATION_SEL_MASK	GENMASK(1, 0)
#define	FLASH_LED_ILED_GRT_THRSH_MASK		GENMASK(5, 0)
#define	FLASH_LED_LMH_LEVEL_MASK		GENMASK(1, 0)
#define	FLASH_LED_VPH_DROOP_HYSTERESIS_MASK	GENMASK(5, 4)
#define	FLASH_LED_VPH_DROOP_THRESHOLD_MASK	GENMASK(2, 0)
#define	FLASH_LED_THERMAL_THRSH_MASK		GENMASK(2, 0)
#define	FLASH_LED_THERMAL_OTST_MASK		GENMASK(2, 0)
#define	FLASH_LED_MOD_CTRL_MASK			BIT(7)
#define	FLASH_LED_HW_SW_STROBE_SEL_MASK		BIT(2)
#define	FLASH_LED_VPH_DROOP_FAULT_MASK		BIT(4)
#define	FLASH_LED_LMH_MITIGATION_EN_MASK	BIT(0)
#define	FLASH_LED_CHGR_MITIGATION_EN_MASK	BIT(4)

#define	VPH_DROOP_DEBOUNCE_US_TO_VAL(val_us)	(val_us / 8)
#define	VPH_DROOP_HYST_MV_TO_VAL(val_mv)	(val_mv / 25)
#define	VPH_DROOP_THRESH_MV_TO_VAL(val_mv)	((val_mv / 100) - 25)
#define	VPH_DROOP_THRESH_VAL_TO_UV(val)		((val + 25) * 100000)
#define	MITIGATION_THRSH_MA_TO_VAL(val_ma)	(val_ma / 100)
#define	CURRENT_MA_TO_REG_VAL(curr_ma, ires_ua)	((curr_ma * 1000) / ires_ua - 1)

#define	FLASH_LED_ISC_WARMUP_DELAY_SHIFT	6
#define	FLASH_LED_WARMUP_DELAY_DEFAULT		2
#define	FLASH_LED_ISC_DELAY_DEFAULT		3
#define	FLASH_LED_VPH_DROOP_DEBOUNCE_DEFAULT	2
#define	FLASH_LED_VPH_DROOP_HYST_DEFAULT	2
#define	FLASH_LED_VPH_DROOP_THRESH_DEFAULT	5
#define	FLASH_LED_VPH_DROOP_DEBOUNCE_MAX	3
#define	FLASH_LED_VPH_DROOP_HYST_MAX		3
#define	FLASH_LED_VPH_DROOP_THRESH_MAX		7
#define	FLASH_LED_THERMAL_THRSH_MIN		3
#define	FLASH_LED_THERMAL_OTST_LEVELS		3
#define	FLASH_LED_VLED_MAX_DEFAULT_UV		3500000
#define	FLASH_LED_IBATT_OCP_THRESH_DEFAULT_UA	4500000
#define	FLASH_LED_RPARA_DEFAULT_UOHM		0
#define	FLASH_LED_SAFETY_TMR_VAL_OFFSET		1
#define	FLASH_LED_SAFETY_TMR_VAL_DIVISOR	10
#define	FLASH_LED_SAFETY_TMR_ENABLE		BIT(7)
#define	FLASH_LED_LMH_LEVEL_DEFAULT		0
#define	FLASH_LED_LMH_MITIGATION_ENABLE		1
#define	FLASH_LED_LMH_MITIGATION_DISABLE	0
#define	FLASH_LED_CHGR_MITIGATION_ENABLE	BIT(4)
#define	FLASH_LED_CHGR_MITIGATION_DISABLE	0
#define	FLASH_LED_MITIGATION_SEL_DEFAULT	2
#define	FLASH_LED_MITIGATION_SEL_MAX		2
#define	FLASH_LED_CHGR_MITIGATION_SEL_SHIFT	4
#define	FLASH_LED_MITIGATION_THRSH_DEFAULT	0xA
#define	FLASH_LED_MITIGATION_THRSH_MAX		0x1F
#define	FLASH_LED_LMH_OCV_THRESH_DEFAULT_UV	3700000
#define	FLASH_LED_LMH_RBATT_THRESH_DEFAULT_UOHM	400000
#define	FLASH_LED_IRES_BASE			3
#define	FLASH_LED_IRES_DIVISOR			2500
#define	FLASH_LED_IRES_MIN_UA			5000
#define	FLASH_LED_IRES_DEFAULT_UA		12500
#define	FLASH_LED_IRES_DEFAULT_VAL		0x00
#define	FLASH_LED_HDRM_VOL_SHIFT		4
#define	FLASH_LED_HDRM_VOL_DEFAULT_MV		0x80
#define	FLASH_LED_HDRM_VOL_HI_LO_WIN_DEFAULT_MV	0x04
#define	FLASH_LED_HDRM_VOL_BASE_MV		125
#define	FLASH_LED_HDRM_VOL_STEP_MV		25
#define	FLASH_LED_STROBE_CFG_DEFAULT		0x00
#define	FLASH_LED_HW_STROBE_OPTION_1		0x00
#define	FLASH_LED_HW_STROBE_OPTION_2		0x01
#define	FLASH_LED_HW_STROBE_OPTION_3		0x02
#define	FLASH_LED_ENABLE			BIT(0)
#define	FLASH_LED_MOD_ENABLE			BIT(7)
#define	FLASH_LED_DISABLE			0x00
#define	FLASH_LED_SAFETY_TMR_DISABLED		0x13
#define	FLASH_LED_MIN_CURRENT_MA		25
#define	FLASH_LED_MAX_TOTAL_CURRENT_MA		3750

/* notifier call chain for flash-led irqs */
static ATOMIC_NOTIFIER_HEAD(irq_notifier_list);

enum flash_led_type {
	FLASH_LED_TYPE_FLASH,
	FLASH_LED_TYPE_TORCH,
};

enum {
	LED1 = 0,
	LED2,
	LED3,
};

/*
 * Configurations for each individual LED
 */
struct flash_node_data {
	struct platform_device		*pdev;
	struct led_classdev		cdev;
	struct pinctrl			*pinctrl;
	struct pinctrl_state		*gpio_state_active;
	struct pinctrl_state		*gpio_state_suspend;
	struct pinctrl_state		*hw_strobe_state_active;
	struct pinctrl_state		*hw_strobe_state_suspend;
	int				hw_strobe_gpio;
	int				ires_ua;
	int				max_current;
	int				current_ma;
	u8				duration;
	u8				id;
	u8				type;
	u8				ires;
	u8				hdrm_val;
	u8				current_reg_val;
	u8				trigger;
	bool				led_on;
};

struct flash_regulator_data {
	struct regulator		*vreg;
	const char			*reg_name;
	u32				max_volt_uv;
};

struct flash_switch_data {
	struct platform_device		*pdev;
	struct led_classdev		cdev;
	struct flash_regulator_data	*reg_data;
	int				led_mask;
	int				num_regulators;
	bool				regulator_on;
	bool				enabled;
};

/*
 * Flash LED configuration read from device tree
 */
struct flash_led_platform_data {
	int	*thermal_derate_current;
	int	all_ramp_up_done_irq;
	int	all_ramp_down_done_irq;
	int	led_fault_irq;
	int	ibatt_ocp_threshold_ua;
	int	vled_max_uv;
	int	rpara_uohm;
	int	lmh_rbatt_threshold_uohm;
	int	lmh_ocv_threshold_uv;
	u8	isc_delay;
	u8	warmup_delay;
	u8	current_derate_en_cfg;
	u8	vph_droop_threshold;
	u8	vph_droop_hysteresis;
	u8	vph_droop_debounce;
	u8	lmh_mitigation_sel;
	u8	chgr_mitigation_sel;
	u8	lmh_level;
	u8	iled_thrsh_val;
	u8	hw_strobe_option;
	bool	hdrm_auto_mode_en;
	bool	thermal_derate_en;
};

/*
 * Flash LED data structure containing flash LED attributes
 */
struct qpnp_flash_led {
	struct flash_led_platform_data	*pdata;
	struct platform_device		*pdev;
	struct regmap			*regmap;
	struct flash_node_data		*fnode;
	struct flash_switch_data	*snode;
	struct power_supply		*bms_psy;
	struct notifier_block		nb;
	spinlock_t			lock;
	int				num_fnodes;
	int				num_snodes;
	int				enable;
	u16				base;
	bool				trigger_lmh;
	bool				trigger_chgr;
};

static int
qpnp_flash_led_read(struct qpnp_flash_led *led, u16 addr, u8 *data)
{
	int rc;
	uint val;

	rc = regmap_read(led->regmap, addr, &val);
	if (rc < 0)
		dev_err(&led->pdev->dev, "Unable to read from 0x%04X rc = %d\n",
			addr, rc);
	else
		dev_dbg(&led->pdev->dev, "Read 0x%02X from addr 0x%04X\n",
			val, addr);

	*data = (u8)val;
	return rc;
}

static int
qpnp_flash_led_masked_read(struct qpnp_flash_led *led, u16 addr, u8 mask,
								u8 *val)
{
	int rc;

	rc = qpnp_flash_led_read(led, addr, val);
	if (rc < 0)
		return rc;

	*val &= mask;
	return rc;
}

static int
qpnp_flash_led_masked_write(struct qpnp_flash_led *led, u16 addr, u8 mask,
								u8 val)
{
	int rc;

	rc = regmap_update_bits(led->regmap, addr, mask, val);
	if (rc < 0)
		dev_err(&led->pdev->dev, "Unable to update bits from 0x%04X, rc = %d\n",
			addr, rc);
	else
		dev_dbg(&led->pdev->dev, "Wrote 0x%02X to addr 0x%04X\n",
			val, addr);

	return rc;
}

static enum
led_brightness qpnp_flash_led_brightness_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int qpnp_flash_led_init_settings(struct qpnp_flash_led *led)
{
	int rc, i, addr_offset;
	u8 val = 0;

	for (i = 0; i < led->num_fnodes; i++) {
		addr_offset = led->fnode[i].id;
		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_HDRM_PRGM(led->base + addr_offset),
			FLASH_LED_HDRM_MODE_PRGM_MASK,
			led->fnode[i].hdrm_val);
		if (rc < 0)
			return rc;

		val |= 0x1 << led->fnode[i].id;
	}

	rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_HDRM_AUTO_MODE_CTRL(led->base),
				FLASH_LED_HDRM_MODE_PRGM_MASK, val);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_ISC_DELAY(led->base),
			FLASH_LED_ISC_WARMUP_DELAY_MASK,
			led->pdata->isc_delay);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_WARMUP_DELAY(led->base),
			FLASH_LED_ISC_WARMUP_DELAY_MASK,
			led->pdata->warmup_delay);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_CURRENT_DERATE_EN(led->base),
			FLASH_LED_CURRENT_DERATE_EN_MASK,
			led->pdata->current_derate_en_cfg);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_VPH_DROOP_DEBOUNCE(led->base),
			FLASH_LED_VPH_DROOP_DEBOUNCE_MASK,
			led->pdata->vph_droop_debounce);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_VPH_DROOP_THRESHOLD(led->base),
			FLASH_LED_VPH_DROOP_THRESHOLD_MASK,
			led->pdata->vph_droop_threshold);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_VPH_DROOP_THRESHOLD(led->base),
			FLASH_LED_VPH_DROOP_HYSTERESIS_MASK,
			led->pdata->vph_droop_hysteresis);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_MITIGATION_SEL(led->base),
			FLASH_LED_LMH_MITIGATION_SEL_MASK,
			led->pdata->lmh_mitigation_sel);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_MITIGATION_SEL(led->base),
			FLASH_LED_CHGR_MITIGATION_SEL_MASK,
			led->pdata->chgr_mitigation_sel);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_LMH_LEVEL(led->base),
			FLASH_LED_LMH_LEVEL_MASK,
			led->pdata->lmh_level);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_ILED_GRT_THRSH(led->base),
			FLASH_LED_ILED_GRT_THRSH_MASK,
			led->pdata->iled_thrsh_val);
	if (rc < 0)
		return rc;

	return 0;
}

static int qpnp_flash_led_hw_strobe_enable(struct flash_node_data *fnode,
						int hw_strobe_option, bool on)
{
	int rc = 0;

	/*
	 * If the LED controlled by this fnode is not GPIO controlled
	 * for the given strobe_option, return.
	 */
	if (hw_strobe_option == FLASH_LED_HW_STROBE_OPTION_1)
		return 0;
	else if (hw_strobe_option == FLASH_LED_HW_STROBE_OPTION_2
						&& fnode->id != LED3)
		return 0;
	else if (hw_strobe_option == FLASH_LED_HW_STROBE_OPTION_3
						&& fnode->id == LED1)
		return 0;

	if (gpio_is_valid(fnode->hw_strobe_gpio)) {
		gpio_set_value(fnode->hw_strobe_gpio, on ? 1 : 0);
	} else if (fnode->hw_strobe_state_active &&
					fnode->hw_strobe_state_suspend) {
		rc = pinctrl_select_state(fnode->pinctrl,
			on ? fnode->hw_strobe_state_active :
			fnode->hw_strobe_state_suspend);
		if (rc < 0) {
			dev_err(&fnode->pdev->dev,
				"failed to change hw strobe pin state\n");
			return rc;
		}
	}

	return rc;
}

static int qpnp_flash_led_regulator_enable(struct qpnp_flash_led *led,
				struct flash_switch_data *snode, bool on)
{
	int i, rc = 0;

	if (snode->regulator_on == on)
		return 0;

	if (on == false) {
		i = snode->num_regulators;
		goto out;
	}

	for (i = 0; i < snode->num_regulators; i++) {
		rc = regulator_enable(snode->reg_data[i].vreg);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"regulator enable failed, rc=%d\n", rc);
			goto out;
		}
	}
	snode->regulator_on = true;

	return rc;

out:
	while (i--)
		regulator_disable(snode->reg_data[i].vreg);

	snode->regulator_on = false;
	return rc;
}

static int get_property_from_fg(struct qpnp_flash_led *led,
		enum power_supply_property prop, int *val)
{
	int rc;
	union power_supply_propval pval = {0, };

	if (!led->bms_psy) {
		dev_err(&led->pdev->dev, "no bms psy found\n");
		return -EINVAL;
	}

	rc = power_supply_get_property(led->bms_psy, prop, &pval);
	if (rc) {
		dev_err(&led->pdev->dev,
			"bms psy doesn't support reading prop %d rc = %d\n",
			prop, rc);
		return rc;
	}

	*val = pval.intval;
	return rc;
}

#define VOLTAGE_HDRM_DEFAULT_MV	350
static int qpnp_flash_led_get_voltage_headroom(struct qpnp_flash_led *led)
{
	int i, voltage_hdrm_mv = 0, voltage_hdrm_max = 0;

	for (i = 0; i < led->num_fnodes; i++) {
		if (led->fnode[i].led_on) {
			if (led->fnode[i].id < 2) {
				if (led->fnode[i].current_ma < 750)
					voltage_hdrm_mv = 125;
				else if (led->fnode[i].current_ma < 1000)
					voltage_hdrm_mv = 175;
				else if (led->fnode[i].current_ma < 1250)
					voltage_hdrm_mv = 250;
				else
					voltage_hdrm_mv = 350;
			} else {
				if (led->fnode[i].current_ma < 375)
					voltage_hdrm_mv = 125;
				else if (led->fnode[i].current_ma < 500)
					voltage_hdrm_mv = 175;
				else if (led->fnode[i].current_ma < 625)
					voltage_hdrm_mv = 250;
				else
					voltage_hdrm_mv = 350;
			}

			voltage_hdrm_max = max(voltage_hdrm_max,
						voltage_hdrm_mv);
		}
	}

	if (!voltage_hdrm_max)
		return VOLTAGE_HDRM_DEFAULT_MV;

	return voltage_hdrm_max;
}

#define UCONV			1000000LL
#define MCONV			1000LL
#define FLASH_VDIP_MARGIN	50000
#define BOB_EFFICIENCY		900LL
#define VIN_FLASH_MIN_UV	3300000LL
static int qpnp_flash_led_calc_max_current(struct qpnp_flash_led *led)
{
	int ocv_uv, rbatt_uohm, ibat_now, voltage_hdrm_mv, rc;
	int64_t ibat_flash_ua, avail_flash_ua, avail_flash_power_fw;
	int64_t ibat_safe_ua, vin_flash_uv, vph_flash_uv, vph_flash_vdip;

	/* RESISTANCE = esr_uohm + rslow_uohm */
	rc = get_property_from_fg(led, POWER_SUPPLY_PROP_RESISTANCE,
			&rbatt_uohm);
	if (rc < 0) {
		dev_err(&led->pdev->dev, "bms psy does not support resistance, rc=%d\n",
				rc);
		return rc;
	}

	/* If no battery is connected, return max possible flash current */
	if (!rbatt_uohm)
		return FLASH_LED_MAX_TOTAL_CURRENT_MA;

	rc = get_property_from_fg(led, POWER_SUPPLY_PROP_VOLTAGE_OCV, &ocv_uv);
	if (rc < 0) {
		dev_err(&led->pdev->dev, "bms psy does not support OCV, rc=%d\n",
				rc);
		return rc;
	}

	rc = get_property_from_fg(led, POWER_SUPPLY_PROP_CURRENT_NOW,
			&ibat_now);
	if (rc < 0) {
		dev_err(&led->pdev->dev, "bms psy does not support current, rc=%d\n",
				rc);
		return rc;
	}

	rbatt_uohm += led->pdata->rpara_uohm;
	voltage_hdrm_mv = qpnp_flash_led_get_voltage_headroom(led);
	vph_flash_vdip =
		VPH_DROOP_THRESH_VAL_TO_UV(led->pdata->vph_droop_threshold)
							+ FLASH_VDIP_MARGIN;

	/* Check if LMH_MITIGATION needs to be triggered */
	if (!led->trigger_lmh && (ocv_uv < led->pdata->lmh_ocv_threshold_uv ||
			rbatt_uohm > led->pdata->lmh_rbatt_threshold_uohm)) {
		led->trigger_lmh = true;
		rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_MITIGATION_SW(led->base),
				FLASH_LED_LMH_MITIGATION_EN_MASK,
				FLASH_LED_LMH_MITIGATION_ENABLE);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "trigger lmh mitigation failed, rc=%d\n",
				rc);
			return rc;
		}

		/* Wait for LMH mitigation to take effect */
		udelay(100);

		return qpnp_flash_led_calc_max_current(led);
	}

	/*
	 * Calculate the maximum current that can pulled out of the battery
	 * before the battery voltage dips below a safe threshold.
	 */
	ibat_safe_ua = div_s64((ocv_uv - vph_flash_vdip) * UCONV,
				rbatt_uohm);

	if (ibat_safe_ua <= led->pdata->ibatt_ocp_threshold_ua) {
		/*
		 * If the calculated current is below the OCP threshold, then
		 * use it as the possible flash current.
		 */
		ibat_flash_ua = ibat_safe_ua - ibat_now;
		vph_flash_uv = vph_flash_vdip;
	} else {
		/*
		 * If the calculated current is above the OCP threshold, then
		 * use the ocp threshold instead.
		 *
		 * Any higher current will be tripping the battery OCP.
		 */
		ibat_flash_ua = led->pdata->ibatt_ocp_threshold_ua - ibat_now;
		vph_flash_uv = ocv_uv - div64_s64((int64_t)rbatt_uohm
				* led->pdata->ibatt_ocp_threshold_ua, UCONV);
	}
	/* Calculate the input voltage of the flash module. */
	vin_flash_uv = max((led->pdata->vled_max_uv +
				(voltage_hdrm_mv * MCONV)), VIN_FLASH_MIN_UV);
	/* Calculate the available power for the flash module. */
	avail_flash_power_fw = BOB_EFFICIENCY * vph_flash_uv * ibat_flash_ua;
	/*
	 * Calculate the available amount of current the flash module can draw
	 * before collapsing the battery. (available power/ flash input voltage)
	 */
	avail_flash_ua = div64_s64(avail_flash_power_fw, vin_flash_uv * MCONV);
	dev_dbg(&led->pdev->dev, "avail_iflash=%lld, ocv=%d, ibat=%d, rbatt=%d, trigger_lmh=%d\n",
			avail_flash_ua, ocv_uv, ibat_now, rbatt_uohm,
			led->trigger_lmh);
	return min(FLASH_LED_MAX_TOTAL_CURRENT_MA,
			(int)(div64_s64(avail_flash_ua, MCONV)));
}

static int qpnp_flash_led_calc_thermal_current_lim(struct qpnp_flash_led *led)
{
	int thermal_current_lim = 0;
	int rc;
	u8 thermal_thrsh1, thermal_thrsh2, thermal_thrsh3, otst_status;

	/* Store THERMAL_THRSHx register values */
	rc = qpnp_flash_led_masked_read(led,
			FLASH_LED_REG_THERMAL_THRSH1(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			&thermal_thrsh1);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_read(led,
			FLASH_LED_REG_THERMAL_THRSH2(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			&thermal_thrsh2);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_read(led,
			FLASH_LED_REG_THERMAL_THRSH3(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			&thermal_thrsh3);
	if (rc < 0)
		return rc;

	/* Lower THERMAL_THRSHx thresholds to minimum */
	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_THERMAL_THRSH1(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			FLASH_LED_THERMAL_THRSH_MIN);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_THERMAL_THRSH2(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			FLASH_LED_THERMAL_THRSH_MIN);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_THERMAL_THRSH3(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			FLASH_LED_THERMAL_THRSH_MIN);
	if (rc < 0)
		return rc;

	/* Check THERMAL_OTST status */
	rc = qpnp_flash_led_read(led,
			FLASH_LED_REG_LED_STATUS2(led->base),
			&otst_status);
	if (rc < 0)
		return rc;

	/* Look up current limit based on THERMAL_OTST status */
	if (otst_status)
		thermal_current_lim =
			led->pdata->thermal_derate_current[otst_status >> 1];

	/* Restore THERMAL_THRESHx registers to original values */
	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_THERMAL_THRSH1(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			thermal_thrsh1);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_THERMAL_THRSH2(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			thermal_thrsh2);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_THERMAL_THRSH3(led->base),
			FLASH_LED_THERMAL_THRSH_MASK,
			thermal_thrsh3);
	if (rc < 0)
		return rc;

	return thermal_current_lim;
}

static int qpnp_flash_led_get_max_avail_current(struct qpnp_flash_led *led)
{
	int max_avail_current, thermal_current_lim = 0;

	led->trigger_lmh = false;
	max_avail_current = qpnp_flash_led_calc_max_current(led);
	if (led->pdata->thermal_derate_en)
		thermal_current_lim =
			qpnp_flash_led_calc_thermal_current_lim(led);

	if (thermal_current_lim)
		max_avail_current = min(max_avail_current, thermal_current_lim);

	return max_avail_current;
}

static void qpnp_flash_led_node_set(struct flash_node_data *fnode, int value)
{
	int prgm_current_ma = value;

	if (value <= 0)
		prgm_current_ma = 0;
	else if (value < FLASH_LED_MIN_CURRENT_MA)
		prgm_current_ma = FLASH_LED_MIN_CURRENT_MA;

	prgm_current_ma = min(prgm_current_ma, fnode->max_current);
	fnode->current_ma = prgm_current_ma;
	fnode->cdev.brightness = prgm_current_ma;
	fnode->current_reg_val = CURRENT_MA_TO_REG_VAL(prgm_current_ma,
					fnode->ires_ua);
	fnode->led_on = prgm_current_ma != 0;
}

static int qpnp_flash_led_switch_disable(struct flash_switch_data *snode)
{
	struct qpnp_flash_led *led = dev_get_drvdata(&snode->pdev->dev);
	int i, rc, addr_offset;

	rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_EN_LED_CTRL(led->base),
				snode->led_mask, FLASH_LED_DISABLE);
	if (rc < 0)
		return rc;

	if (led->trigger_lmh) {
		rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_MITIGATION_SW(led->base),
				FLASH_LED_LMH_MITIGATION_EN_MASK,
				FLASH_LED_LMH_MITIGATION_DISABLE);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "disable lmh mitigation failed, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (!led->trigger_chgr) {
		rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_MITIGATION_SW(led->base),
				FLASH_LED_CHGR_MITIGATION_EN_MASK,
				FLASH_LED_CHGR_MITIGATION_DISABLE);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "disable chgr mitigation failed, rc=%d\n",
				rc);
			return rc;
		}
	}

	led->enable--;
	if (led->enable == 0) {
		rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_MOD_CTRL(led->base),
				FLASH_LED_MOD_CTRL_MASK, FLASH_LED_DISABLE);
		if (rc < 0)
			return rc;
	}

	for (i = 0; i < led->num_fnodes; i++) {
		if (!led->fnode[i].led_on ||
				!(snode->led_mask & BIT(led->fnode[i].id)))
			continue;

		addr_offset = led->fnode[i].id;
		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_TGR_CURRENT(led->base + addr_offset),
			FLASH_LED_CURRENT_MASK, 0);
		if (rc < 0)
			return rc;

		led->fnode[i].led_on = false;

		if (led->fnode[i].pinctrl) {
			rc = pinctrl_select_state(led->fnode[i].pinctrl,
					led->fnode[i].gpio_state_suspend);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"failed to disable GPIO, rc=%d\n", rc);
				return rc;
			}
		}

		if (led->fnode[i].trigger & FLASH_LED_HW_SW_STROBE_SEL_MASK) {
			rc = qpnp_flash_led_hw_strobe_enable(&led->fnode[i],
					led->pdata->hw_strobe_option, false);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Unable to disable hw strobe, rc=%d\n",
					rc);
				return rc;
			}
		}
	}

	snode->enabled = false;
	return 0;
}

static int qpnp_flash_led_switch_set(struct flash_switch_data *snode, bool on)
{
	struct qpnp_flash_led *led = dev_get_drvdata(&snode->pdev->dev);
	int rc, i, addr_offset;
	u8 val;

	if (snode->enabled == on) {
		dev_warn(&led->pdev->dev, "Switch node is already %s!\n",
				on ? "enabled" : "disabled");
		return 0;
	}

	if (!on) {
		rc = qpnp_flash_led_switch_disable(snode);
		return rc;
	}

	/* Iterate over all leds for this switch node */
	val = 0;
	for (i = 0; i < led->num_fnodes; i++)
		if (snode->led_mask & BIT(led->fnode[i].id))
			val |= led->fnode[i].ires << (led->fnode[i].id * 2);

	rc = qpnp_flash_led_masked_write(led, FLASH_LED_REG_IRES(led->base),
						FLASH_LED_CURRENT_MASK, val);
	if (rc < 0)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
					FLASH_LED_REG_STROBE_CFG(led->base),
					FLASH_LED_ENABLE_MASK,
					led->pdata->hw_strobe_option);
	if (rc < 0)
		return rc;

	val = 0;
	for (i = 0; i < led->num_fnodes; i++) {
		if (!led->fnode[i].led_on ||
				!(snode->led_mask & BIT(led->fnode[i].id)))
			continue;

		addr_offset = led->fnode[i].id;
		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_STROBE_CTRL(led->base + addr_offset),
			FLASH_LED_ENABLE_MASK, led->fnode[i].trigger);
		if (rc < 0)
			return rc;

		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_TGR_CURRENT(led->base + addr_offset),
			FLASH_LED_CURRENT_MASK, led->fnode[i].current_reg_val);
		if (rc < 0)
			return rc;

		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_SAFETY_TMR(led->base + addr_offset),
			FLASH_LED_SAFETY_TMR_MASK, led->fnode[i].duration);
		if (rc < 0)
			return rc;

		val |= FLASH_LED_ENABLE << led->fnode[i].id;

		if (led->fnode[i].pinctrl) {
			rc = pinctrl_select_state(led->fnode[i].pinctrl,
					led->fnode[i].gpio_state_active);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
						"failed to enable GPIO\n");
				return rc;
			}
		}

		if (led->fnode[i].trigger & FLASH_LED_HW_SW_STROBE_SEL_MASK) {
			rc = qpnp_flash_led_hw_strobe_enable(&led->fnode[i],
					led->pdata->hw_strobe_option, true);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Unable to enable hw strobe\n");
				return rc;
			}
		}
	}

	if (led->enable == 0) {
		rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_MOD_CTRL(led->base),
				FLASH_LED_MOD_CTRL_MASK, FLASH_LED_MOD_ENABLE);
		if (rc < 0)
			return rc;
	}
	led->enable++;

	if (led->trigger_lmh) {
		rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_MITIGATION_SW(led->base),
				FLASH_LED_LMH_MITIGATION_EN_MASK,
				FLASH_LED_LMH_MITIGATION_ENABLE);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "trigger lmh mitigation failed, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (led->trigger_chgr) {
		rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_MITIGATION_SW(led->base),
				FLASH_LED_CHGR_MITIGATION_EN_MASK,
				FLASH_LED_CHGR_MITIGATION_ENABLE);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "trigger chgr mitigation failed, rc=%d\n",
				rc);
			return rc;
		}
	}

	rc = qpnp_flash_led_masked_write(led,
					FLASH_LED_EN_LED_CTRL(led->base),
					snode->led_mask, val);
	if (rc < 0)
		return rc;

	snode->enabled = true;
	return 0;
}

int qpnp_flash_led_prepare(struct led_trigger *trig, int options,
					int *max_current)
{
	struct led_classdev *led_cdev = trigger_to_lcdev(trig);
	struct flash_switch_data *snode;
	struct qpnp_flash_led *led;
	int rc;

	if (!led_cdev) {
		pr_err("Invalid led_trigger provided\n");
		return -EINVAL;
	}

	snode = container_of(led_cdev, struct flash_switch_data, cdev);
	led = dev_get_drvdata(&snode->pdev->dev);

	if (!(options & FLASH_LED_PREPARE_OPTIONS_MASK)) {
		dev_err(&led->pdev->dev, "Invalid options %d\n", options);
		return -EINVAL;
	}

	if (options & ENABLE_REGULATOR) {
		rc = qpnp_flash_led_regulator_enable(led, snode, true);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"enable regulator failed, rc=%d\n", rc);
			return rc;
		}
	}

	if (options & DISABLE_REGULATOR) {
		rc = qpnp_flash_led_regulator_enable(led, snode, false);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"disable regulator failed, rc=%d\n", rc);
			return rc;
		}
	}

	if (options & QUERY_MAX_CURRENT) {
		rc = qpnp_flash_led_get_max_avail_current(led);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"query max current failed, rc=%d\n", rc);
			return rc;
		}
		*max_current = rc;
	}

	led->trigger_chgr = false;
	if (options & PRE_FLASH)
		led->trigger_chgr = true;

	return 0;
}

static void qpnp_flash_led_brightness_set(struct led_classdev *led_cdev,
						enum led_brightness value)
{
	struct flash_node_data *fnode = NULL;
	struct flash_switch_data *snode = NULL;
	struct qpnp_flash_led *led = NULL;
	int rc;

	if (!strncmp(led_cdev->name, "led:switch", strlen("led:switch"))) {
		snode = container_of(led_cdev, struct flash_switch_data, cdev);
		led = dev_get_drvdata(&snode->pdev->dev);
	} else if (!strncmp(led_cdev->name, "led:flash", strlen("led:flash")) ||
			!strncmp(led_cdev->name, "led:torch",
						strlen("led:torch"))) {
		fnode = container_of(led_cdev, struct flash_node_data, cdev);
		led = dev_get_drvdata(&fnode->pdev->dev);
	}

	if (!led) {
		pr_err("Failed to get flash driver data\n");
		return;
	}

	spin_lock(&led->lock);
	if (snode) {
		rc = qpnp_flash_led_switch_set(snode, value > 0);
		if (rc < 0)
			dev_err(&led->pdev->dev,
					"Failed to set flash LED switch\n");
	} else if (fnode) {
		qpnp_flash_led_node_set(fnode, value);
	}

	spin_unlock(&led->lock);
}

static int flash_led_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct qpnp_flash_led *led =
			container_of(nb, struct qpnp_flash_led, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (!strcmp(psy->desc->name, "bms")) {
		led->bms_psy = power_supply_get_by_name("bms");
		if (!led->bms_psy)
			dev_err(&led->pdev->dev, "Failed to get bms power_supply\n");
		else
			power_supply_unreg_notifier(&led->nb);
	}

	return NOTIFY_OK;
}

static int flash_led_psy_register_notifier(struct qpnp_flash_led *led)
{
	int rc;

	led->nb.notifier_call = flash_led_psy_notifier_call;
	rc = power_supply_reg_notifier(&led->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier, rc = %d\n", rc);
		return rc;
	}

	return 0;
}

/* irq handler */
static irqreturn_t qpnp_flash_led_irq_handler(int irq, void *_led)
{
	struct qpnp_flash_led *led = _led;
	enum flash_led_irq_type irq_type = INVALID_IRQ;
	int rc;
	u8 irq_status, led_status1, led_status2;

	dev_dbg(&led->pdev->dev, "irq received, irq=%d\n", irq);

	rc = qpnp_flash_led_read(led,
			FLASH_LED_REG_INT_RT_STS(led->base), &irq_status);
	if (rc < 0) {
		dev_err(&led->pdev->dev, "Failed to read interrupt status reg, rc=%d\n",
				rc);
		goto exit;
	}

	if (irq == led->pdata->all_ramp_up_done_irq)
		irq_type = ALL_RAMP_UP_DONE_IRQ;
	else if (irq == led->pdata->all_ramp_down_done_irq)
		irq_type = ALL_RAMP_DOWN_DONE_IRQ;
	else if (irq == led->pdata->led_fault_irq)
		irq_type = LED_FAULT_IRQ;

	if (irq_type == ALL_RAMP_UP_DONE_IRQ)
		atomic_notifier_call_chain(&irq_notifier_list,
						irq_type, NULL);

	if (irq_type == LED_FAULT_IRQ) {
		rc = qpnp_flash_led_read(led,
			FLASH_LED_REG_LED_STATUS1(led->base), &led_status1);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "Failed to read led_status1 reg, rc=%d\n",
					rc);
			goto exit;
		}

		rc = qpnp_flash_led_read(led,
			FLASH_LED_REG_LED_STATUS2(led->base), &led_status2);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "Failed to read led_status2 reg, rc=%d\n",
					rc);
			goto exit;
		}

		if (led_status1)
			dev_emerg(&led->pdev->dev, "led short/open fault detected! led_status1=%x\n",
					led_status1);

		if (led_status2 & FLASH_LED_VPH_DROOP_FAULT_MASK)
			dev_emerg(&led->pdev->dev, "led vph_droop fault detected!\n");
	}

	dev_dbg(&led->pdev->dev, "irq handled, irq_type=%x, irq_status=%x\n",
		irq_type, irq_status);

exit:
	return IRQ_HANDLED;
}

int qpnp_flash_led_register_irq_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&irq_notifier_list, nb);
}

int qpnp_flash_led_unregister_irq_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&irq_notifier_list, nb);
}

static int qpnp_flash_led_regulator_setup(struct qpnp_flash_led *led,
				struct flash_switch_data *snode, bool on)
{
	int i, rc = 0;

	if (on == false) {
		i = snode->num_regulators;
		goto out;
	}

	for (i = 0; i < snode->num_regulators; i++) {
		snode->reg_data[i].vreg = regulator_get(snode->cdev.dev,
						snode->reg_data[i].reg_name);
		if (IS_ERR(snode->reg_data[i].vreg)) {
			rc = PTR_ERR(snode->reg_data[i].vreg);
			dev_err(&led->pdev->dev,
					"Failed to get regulator, rc=%d\n", rc);
			goto out;
		}

		if (regulator_count_voltages(snode->reg_data[i].vreg) > 0) {
			rc = regulator_set_voltage(snode->reg_data[i].vreg,
					snode->reg_data[i].max_volt_uv,
					snode->reg_data[i].max_volt_uv);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"regulator set voltage failed, rc=%d\n",
					rc);
				regulator_put(snode->reg_data[i].vreg);
				goto out;
			}
		}
	}

	return rc;

out:
	while (i--) {
		if (regulator_count_voltages(snode->reg_data[i].vreg) > 0)
			regulator_set_voltage(snode->reg_data[i].vreg, 0,
					snode->reg_data[i].max_volt_uv);

		regulator_put(snode->reg_data[i].vreg);
	}

	return rc;
}

static int qpnp_flash_led_regulator_parse_dt(struct qpnp_flash_led *led,
					struct flash_switch_data *snode,
					struct device_node *node) {

	int i = 0, rc = 0, num_regs = 0;
	struct device_node *temp = NULL;
	const char *temp_string;
	u32 val;

	while ((temp = of_get_next_available_child(node, temp))) {
		if (of_find_property(temp, "regulator-name", NULL))
			num_regs++;
	}
	snode->num_regulators = num_regs;

	if (snode->num_regulators == 0)
		return 0;

	snode->reg_data = devm_kcalloc(&led->pdev->dev, snode->num_regulators,
					sizeof(*snode->reg_data),
					GFP_KERNEL);
	if (!snode->reg_data)
		return -ENOMEM;

	for_each_available_child_of_node(node, temp) {
		rc = of_property_read_string(temp, "regulator-name",
							&temp_string);
		if (!rc)
			snode->reg_data[i].reg_name = temp_string;
		else {
			dev_err(&led->pdev->dev,
				"Unable to read regulator name, rc=%d\n", rc);
			return rc;
		}

		rc = of_property_read_u32(temp, "max-voltage-uv", &val);
		if (!rc) {
			snode->reg_data[i].max_volt_uv = val;
		} else if (rc != -EINVAL) {
			dev_err(&led->pdev->dev,
				"Unable to read max voltage, rc=%d\n", rc);
			return rc;
		}

		i++;
	}

	return 0;
}

static int qpnp_flash_led_parse_each_led_dt(struct qpnp_flash_led *led,
			struct flash_node_data *fnode, struct device_node *node)
{
	const char *temp_string;
	int rc;
	u32 val;
	bool strobe_sel = 0, edge_trigger = 0, active_high = 0;

	fnode->pdev = led->pdev;
	fnode->cdev.brightness_set = qpnp_flash_led_brightness_set;
	fnode->cdev.brightness_get = qpnp_flash_led_brightness_get;

	rc = of_property_read_string(node, "qcom,led-name", &fnode->cdev.name);
	if (rc < 0) {
		dev_err(&led->pdev->dev, "Unable to read flash LED names\n");
		return rc;
	}

	rc = of_property_read_string(node, "label", &temp_string);
	if (!rc) {
		if (!strcmp(temp_string, "flash")) {
			fnode->type = FLASH_LED_TYPE_FLASH;
		} else if (!strcmp(temp_string, "torch")) {
			fnode->type = FLASH_LED_TYPE_TORCH;
		} else {
			dev_err(&led->pdev->dev, "Wrong flash LED type\n");
			return rc;
		}
	} else {
		dev_err(&led->pdev->dev, "Unable to read flash LED label\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,id", &val);
	if (!rc) {
		fnode->id = (u8)val;
	} else {
		dev_err(&led->pdev->dev, "Unable to read flash LED ID\n");
		return rc;
	}

	rc = of_property_read_string(node, "qcom,default-led-trigger",
						&fnode->cdev.default_trigger);
	if (rc < 0) {
		dev_err(&led->pdev->dev, "Unable to read trigger name\n");
		return rc;
	}

	fnode->ires_ua = FLASH_LED_IRES_DEFAULT_UA;
	fnode->ires = FLASH_LED_IRES_DEFAULT_VAL;
	rc = of_property_read_u32(node, "qcom,ires-ua", &val);
	if (!rc) {
		fnode->ires_ua = val;
		fnode->ires = FLASH_LED_IRES_BASE -
			(val - FLASH_LED_IRES_MIN_UA) / FLASH_LED_IRES_DIVISOR;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to read current resolution\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,max-current", &val);
	if (!rc) {
		if (val < FLASH_LED_MIN_CURRENT_MA)
			val = FLASH_LED_MIN_CURRENT_MA;
		fnode->max_current = val;
		fnode->cdev.max_brightness = val;
	} else {
		dev_err(&led->pdev->dev,
				"Unable to read max current, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,current-ma", &val);
	if (!rc) {
		if (val < FLASH_LED_MIN_CURRENT_MA ||
				val > fnode->max_current)
			dev_warn(&led->pdev->dev,
				 "Invalid operational current specified, capping it\n");
		if (val < FLASH_LED_MIN_CURRENT_MA)
			val = FLASH_LED_MIN_CURRENT_MA;
		if (val > fnode->max_current)
			val = fnode->max_current;
		fnode->current_ma = val;
		fnode->cdev.brightness = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev,
			"Unable to read operational current, rc=%d\n", rc);
		return rc;
	}

	fnode->duration = FLASH_LED_SAFETY_TMR_DISABLED;
	rc = of_property_read_u32(node, "qcom,duration-ms", &val);
	if (!rc) {
		fnode->duration = (u8)(((val -
					FLASH_LED_SAFETY_TMR_VAL_OFFSET) /
					FLASH_LED_SAFETY_TMR_VAL_DIVISOR) |
					FLASH_LED_SAFETY_TMR_ENABLE);
	} else if (rc == -EINVAL) {
		if (fnode->type == FLASH_LED_TYPE_FLASH) {
			dev_err(&led->pdev->dev,
				"Timer duration is required for flash LED\n");
			return rc;
		}
	} else {
		dev_err(&led->pdev->dev,
				"Unable to read timer duration\n");
		return rc;
	}

	fnode->hdrm_val = FLASH_LED_HDRM_VOL_DEFAULT_MV;
	rc = of_property_read_u32(node, "qcom,hdrm-voltage-mv", &val);
	if (!rc) {
		val = (val - FLASH_LED_HDRM_VOL_BASE_MV) /
						FLASH_LED_HDRM_VOL_STEP_MV;
		fnode->hdrm_val = (val << FLASH_LED_HDRM_VOL_SHIFT) &
							FLASH_LED_HDRM_VOL_MASK;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to read headroom voltage\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,hdrm-vol-hi-lo-win-mv", &val);
	if (!rc) {
		fnode->hdrm_val |= (val / FLASH_LED_HDRM_VOL_STEP_MV) &
						~FLASH_LED_HDRM_VOL_MASK;
	} else if (rc == -EINVAL) {
		fnode->hdrm_val |= FLASH_LED_HDRM_VOL_HI_LO_WIN_DEFAULT_MV;
	} else {
		dev_err(&led->pdev->dev,
				"Unable to read hdrm hi-lo window voltage\n");
		return rc;
	}

	strobe_sel = of_property_read_bool(node, "qcom,hw-strobe-sel");
	if (strobe_sel) {
		edge_trigger = of_property_read_bool(node,
						"qcom,hw-strobe-edge-trigger");
		active_high = !of_property_read_bool(node,
						"qcom,hw-strobe-active-low");
	}
	fnode->trigger = (strobe_sel << 2) | (edge_trigger << 1) | active_high;

	if (fnode->trigger & FLASH_LED_HW_SW_STROBE_SEL_MASK) {
		if (of_find_property(node, "qcom,hw-strobe-gpio", NULL)) {
			fnode->hw_strobe_gpio = of_get_named_gpio(node,
						"qcom,hw-strobe-gpio", 0);
			if (fnode->hw_strobe_gpio < 0) {
				dev_err(&led->pdev->dev,
					"Invalid gpio specified\n");
				return fnode->hw_strobe_gpio;
			}
			gpio_direction_output(fnode->hw_strobe_gpio, 0);
		} else {
			fnode->hw_strobe_gpio = -1;
			fnode->hw_strobe_state_active =
				pinctrl_lookup_state(fnode->pinctrl,
				"strobe_enable");
			if (IS_ERR_OR_NULL(fnode->hw_strobe_state_active)) {
				dev_err(&led->pdev->dev,
					"No active pin for hardware strobe, rc=%ld\n",
					PTR_ERR(fnode->hw_strobe_state_active));
				fnode->hw_strobe_state_active = NULL;
			}

			fnode->hw_strobe_state_suspend =
				pinctrl_lookup_state(fnode->pinctrl,
				"strobe_disable");
			if (IS_ERR_OR_NULL(fnode->hw_strobe_state_suspend)) {
				dev_err(&led->pdev->dev,
					"No suspend pin for hardware strobe, rc=%ld\n",
					PTR_ERR(fnode->hw_strobe_state_suspend)
					);
				fnode->hw_strobe_state_suspend = NULL;
			}
		}
	}

	rc = led_classdev_register(&led->pdev->dev, &fnode->cdev);
	if (rc < 0) {
		dev_err(&led->pdev->dev, "Unable to register led node %d\n",
								fnode->id);
		return rc;
	}

	fnode->cdev.dev->of_node = node;

	fnode->pinctrl = devm_pinctrl_get(fnode->cdev.dev);
	if (IS_ERR_OR_NULL(fnode->pinctrl)) {
		dev_warn(&led->pdev->dev, "No pinctrl defined\n");
		fnode->pinctrl = NULL;
	} else {
		fnode->gpio_state_active =
			pinctrl_lookup_state(fnode->pinctrl, "led_enable");
		if (IS_ERR_OR_NULL(fnode->gpio_state_active)) {
			dev_err(&led->pdev->dev,
					"Cannot lookup LED active state\n");
			devm_pinctrl_put(fnode->pinctrl);
			fnode->pinctrl = NULL;
			return PTR_ERR(fnode->gpio_state_active);
		}

		fnode->gpio_state_suspend =
			pinctrl_lookup_state(fnode->pinctrl, "led_disable");
		if (IS_ERR_OR_NULL(fnode->gpio_state_suspend)) {
			dev_err(&led->pdev->dev,
					"Cannot lookup LED disable state\n");
			devm_pinctrl_put(fnode->pinctrl);
			fnode->pinctrl = NULL;
			return PTR_ERR(fnode->gpio_state_suspend);
		}
	}

	return 0;
}

static int qpnp_flash_led_parse_and_register_switch(struct qpnp_flash_led *led,
						struct flash_switch_data *snode,
						struct device_node *node)
{
	int rc = 0;

	rc = of_property_read_string(node, "qcom,led-name", &snode->cdev.name);
	if (rc < 0) {
		dev_err(&led->pdev->dev,
				"Failed to read switch node name, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_string(node, "qcom,default-led-trigger",
					&snode->cdev.default_trigger);
	if (rc < 0) {
		dev_err(&led->pdev->dev,
				"Unable to read trigger name, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,led-mask", &snode->led_mask);
	if (rc < 0) {
		dev_err(&led->pdev->dev, "Unable to read led mask rc=%d\n", rc);
		return rc;
	}

	if (snode->led_mask < 1 || snode->led_mask > 7) {
		dev_err(&led->pdev->dev, "Invalid value for led-mask\n");
		return -EINVAL;
	}

	rc = qpnp_flash_led_regulator_parse_dt(led, snode, node);
	if (rc < 0) {
		dev_err(&led->pdev->dev,
			"Unable to parse regulator data, rc=%d\n", rc);
		return rc;
	}

	if (snode->num_regulators) {
		rc = qpnp_flash_led_regulator_setup(led, snode, true);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"Unable to setup regulator, rc=%d\n", rc);
			return rc;
		}
	}

	snode->pdev = led->pdev;
	snode->cdev.brightness_set = qpnp_flash_led_brightness_set;
	snode->cdev.brightness_get = qpnp_flash_led_brightness_get;
	rc = led_classdev_register(&led->pdev->dev, &snode->cdev);
	if (rc < 0) {
		dev_err(&led->pdev->dev,
					"Unable to register led switch node\n");
		return rc;
	}

	snode->cdev.dev->of_node = node;
	return 0;
}

static int qpnp_flash_led_parse_common_dt(struct qpnp_flash_led *led,
						struct device_node *node)
{
	int rc;
	u32 val;
	bool short_circuit_det, open_circuit_det, vph_droop_det;

	led->pdata->hdrm_auto_mode_en = of_property_read_bool(node,
							"qcom,hdrm-auto-mode");

	led->pdata->isc_delay = FLASH_LED_ISC_DELAY_DEFAULT;
	rc = of_property_read_u32(node, "qcom,isc-delay-us", &val);
	if (!rc) {
		led->pdata->isc_delay =
				val >> FLASH_LED_ISC_WARMUP_DELAY_SHIFT;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev,
				"Unable to read ISC delay, rc=%d\n", rc);
		return rc;
	}

	led->pdata->warmup_delay = FLASH_LED_WARMUP_DELAY_DEFAULT;
	rc = of_property_read_u32(node, "qcom,warmup-delay-us", &val);
	if (!rc) {
		led->pdata->warmup_delay =
				val >> FLASH_LED_ISC_WARMUP_DELAY_SHIFT;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev,
				"Unable to read WARMUP delay, rc=%d\n", rc);
		return rc;
	}

	short_circuit_det =
		of_property_read_bool(node, "qcom,short-circuit-det");
	open_circuit_det = of_property_read_bool(node, "qcom,open-circuit-det");
	vph_droop_det = of_property_read_bool(node, "qcom,vph-droop-det");
	led->pdata->current_derate_en_cfg = (vph_droop_det << 2) |
				(open_circuit_det << 1) | short_circuit_det;

	led->pdata->thermal_derate_en =
		of_property_read_bool(node, "qcom,thermal-derate-en");

	if (led->pdata->thermal_derate_en) {
		led->pdata->thermal_derate_current =
			devm_kcalloc(&led->pdev->dev,
					FLASH_LED_THERMAL_OTST_LEVELS,
					sizeof(int), GFP_KERNEL);
		if (!led->pdata->thermal_derate_current)
			return -ENOMEM;

		rc = of_property_read_u32_array(node,
					"qcom,thermal-derate-current",
					led->pdata->thermal_derate_current,
					FLASH_LED_THERMAL_OTST_LEVELS);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "Unable to read thermal current limits, rc=%d\n",
					rc);
			return rc;
		}
	}

	led->pdata->vph_droop_debounce = FLASH_LED_VPH_DROOP_DEBOUNCE_DEFAULT;
	rc = of_property_read_u32(node, "qcom,vph-droop-debounce-us", &val);
	if (!rc) {
		led->pdata->vph_droop_debounce =
			VPH_DROOP_DEBOUNCE_US_TO_VAL(val);
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev,
			"Unable to read VPH droop debounce, rc=%d\n", rc);
		return rc;
	}

	if (led->pdata->vph_droop_debounce > FLASH_LED_VPH_DROOP_DEBOUNCE_MAX) {
		dev_err(&led->pdev->dev,
				"Invalid VPH droop debounce specified");
		return -EINVAL;
	}

	led->pdata->vph_droop_threshold = FLASH_LED_VPH_DROOP_THRESH_DEFAULT;
	rc = of_property_read_u32(node, "qcom,vph-droop-threshold-mv", &val);
	if (!rc) {
		led->pdata->vph_droop_threshold =
			VPH_DROOP_THRESH_MV_TO_VAL(val);
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev,
			"Unable to read VPH droop threshold, rc=%d\n", rc);
		return rc;
	}

	if (led->pdata->vph_droop_threshold > FLASH_LED_VPH_DROOP_THRESH_MAX) {
		dev_err(&led->pdev->dev,
				"Invalid VPH droop threshold specified");
		return -EINVAL;
	}

	led->pdata->vph_droop_hysteresis =
			FLASH_LED_VPH_DROOP_HYST_DEFAULT;
	rc = of_property_read_u32(node, "qcom,vph-droop-hysteresis-mv", &val);
	if (!rc) {
		led->pdata->vph_droop_hysteresis =
			VPH_DROOP_HYST_MV_TO_VAL(val);
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev,
			"Unable to read VPH droop hysteresis, rc=%d\n", rc);
		return rc;
	}

	if (led->pdata->vph_droop_hysteresis > FLASH_LED_VPH_DROOP_HYST_MAX) {
		dev_err(&led->pdev->dev,
				"Invalid VPH droop hysteresis specified");
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "qcom,hw-strobe-option", &val);
	if (!rc) {
		led->pdata->hw_strobe_option = (u8)val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev,
			"Unable to parse hw strobe option, rc=%d\n", rc);
		return rc;
	}

	led->pdata->vled_max_uv = FLASH_LED_VLED_MAX_DEFAULT_UV;
	rc = of_property_read_u32(node, "qcom,vled-max-uv", &val);
	if (!rc) {
		led->pdata->vled_max_uv = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse vled_max voltage, rc=%d\n",
				rc);
		return rc;
	}

	led->pdata->ibatt_ocp_threshold_ua =
		FLASH_LED_IBATT_OCP_THRESH_DEFAULT_UA;
	rc = of_property_read_u32(node, "qcom,ibatt-ocp-threshold-ua", &val);
	if (!rc) {
		led->pdata->ibatt_ocp_threshold_ua = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse ibatt_ocp threshold, rc=%d\n",
				rc);
		return rc;
	}

	led->pdata->rpara_uohm = FLASH_LED_RPARA_DEFAULT_UOHM;
	rc = of_property_read_u32(node, "qcom,rparasitic-uohm", &val);
	if (!rc) {
		led->pdata->rpara_uohm = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse rparasitic, rc=%d\n",
				rc);
		return rc;
	}

	led->pdata->lmh_ocv_threshold_uv =
		FLASH_LED_LMH_OCV_THRESH_DEFAULT_UV;
	rc = of_property_read_u32(node, "qcom,lmh-ocv-threshold-uv", &val);
	if (!rc) {
		led->pdata->lmh_ocv_threshold_uv = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse lmh ocv threshold, rc=%d\n",
				rc);
		return rc;
	}

	led->pdata->lmh_rbatt_threshold_uohm =
		FLASH_LED_LMH_RBATT_THRESH_DEFAULT_UOHM;
	rc = of_property_read_u32(node, "qcom,lmh-rbatt-threshold-uohm", &val);
	if (!rc) {
		led->pdata->lmh_rbatt_threshold_uohm = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse lmh rbatt threshold, rc=%d\n",
				rc);
		return rc;
	}

	led->pdata->lmh_level = FLASH_LED_LMH_LEVEL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,lmh-level", &val);
	if (!rc) {
		led->pdata->lmh_level = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse lmh_level, rc=%d\n",
				rc);
		return rc;
	}

	led->pdata->lmh_mitigation_sel = FLASH_LED_MITIGATION_SEL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,lmh-mitigation-sel", &val);
	if (!rc) {
		led->pdata->lmh_mitigation_sel = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse lmh_mitigation_sel, rc=%d\n",
				rc);
		return rc;
	}

	if (led->pdata->lmh_mitigation_sel > FLASH_LED_MITIGATION_SEL_MAX) {
		dev_err(&led->pdev->dev, "Invalid lmh_mitigation_sel specified\n");
		return -EINVAL;
	}

	led->pdata->chgr_mitigation_sel = FLASH_LED_MITIGATION_SEL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,chgr-mitigation-sel", &val);
	if (!rc) {
		led->pdata->chgr_mitigation_sel = val;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse chgr_mitigation_sel, rc=%d\n",
				rc);
		return rc;
	}

	if (led->pdata->chgr_mitigation_sel > FLASH_LED_MITIGATION_SEL_MAX) {
		dev_err(&led->pdev->dev, "Invalid chgr_mitigation_sel specified\n");
		return -EINVAL;
	}

	led->pdata->chgr_mitigation_sel <<= FLASH_LED_CHGR_MITIGATION_SEL_SHIFT;

	led->pdata->iled_thrsh_val = FLASH_LED_MITIGATION_THRSH_DEFAULT;
	rc = of_property_read_u32(node, "qcom,iled-thrsh-ma", &val);
	if (!rc) {
		led->pdata->iled_thrsh_val = MITIGATION_THRSH_MA_TO_VAL(val);
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to parse iled_thrsh_val, rc=%d\n",
				rc);
		return rc;
	}

	if (led->pdata->iled_thrsh_val > FLASH_LED_MITIGATION_THRSH_MAX) {
		dev_err(&led->pdev->dev, "Invalid iled_thrsh_val specified\n");
		return -EINVAL;
	}

	led->pdata->all_ramp_up_done_irq =
		of_irq_get_byname(node, "all-ramp-up-done-irq");
	if (led->pdata->all_ramp_up_done_irq < 0)
		dev_dbg(&led->pdev->dev, "all-ramp-up-done-irq not used\n");

	led->pdata->all_ramp_down_done_irq =
		of_irq_get_byname(node, "all-ramp-down-done-irq");
	if (led->pdata->all_ramp_down_done_irq < 0)
		dev_dbg(&led->pdev->dev, "all-ramp-down-done-irq not used\n");

	led->pdata->led_fault_irq =
		of_irq_get_byname(node, "led-fault-irq");
	if (led->pdata->led_fault_irq < 0)
		dev_dbg(&led->pdev->dev, "led-fault-irq not used\n");

	return 0;
}

static int qpnp_flash_led_probe(struct platform_device *pdev)
{
	struct qpnp_flash_led *led;
	struct device_node *node, *temp;
	const char *temp_string;
	unsigned int base;
	int rc, i = 0;

	node = pdev->dev.of_node;
	if (!node) {
		dev_info(&pdev->dev, "No flash LED nodes defined\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "reg", &base);
	if (rc < 0) {
		dev_err(&pdev->dev, "Couldn't find reg in node %s, rc = %d\n",
							node->full_name, rc);
		return rc;
	}

	led = devm_kzalloc(&pdev->dev, sizeof(struct qpnp_flash_led),
								GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!led->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	led->base = base;
	led->pdev = pdev;
	led->pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct flash_led_platform_data), GFP_KERNEL);
	if (!led->pdata)
		return -ENOMEM;

	rc = qpnp_flash_led_parse_common_dt(led, node);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"Failed to parse common flash LED device tree\n");
		return rc;
	}

	for_each_available_child_of_node(node, temp) {
		rc = of_property_read_string(temp, "label", &temp_string);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Failed to parse label, rc=%d\n", rc);
			return rc;
		}

		if (!strcmp("switch", temp_string)) {
			led->num_snodes++;
		} else if (!strcmp("flash", temp_string) ||
				!strcmp("torch", temp_string)) {
			led->num_fnodes++;
		} else {
			dev_err(&pdev->dev,
					"Invalid label for led node\n");
			return -EINVAL;
		}
	}

	if (!led->num_fnodes) {
		dev_err(&pdev->dev, "No LED nodes defined\n");
		return -ECHILD;
	}

	led->fnode = devm_kcalloc(&pdev->dev, led->num_fnodes,
				sizeof(*led->fnode),
				GFP_KERNEL);
	if (!led->fnode)
		return -ENOMEM;

	led->snode = devm_kcalloc(&pdev->dev, led->num_snodes,
				sizeof(*led->snode),
				GFP_KERNEL);
	if (!led->snode)
		return -ENOMEM;

	temp = NULL;
	for (i = 0; i < led->num_fnodes; i++) {
		temp = of_get_next_available_child(node, temp);
		rc = qpnp_flash_led_parse_each_led_dt(led,
							&led->fnode[i], temp);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Unable to parse flash node %d rc=%d\n", i, rc);
			goto error_led_register;
		}
	}

	for (i = 0; i < led->num_snodes; i++) {
		temp = of_get_next_available_child(node, temp);
		rc = qpnp_flash_led_parse_and_register_switch(led,
							&led->snode[i], temp);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Unable to parse and register switch node, rc=%d\n",
				rc);
			goto error_switch_register;
		}
	}

	/* setup irqs */
	if (led->pdata->all_ramp_up_done_irq >= 0) {
		rc = devm_request_threaded_irq(&led->pdev->dev,
			led->pdata->all_ramp_up_done_irq,
			NULL, qpnp_flash_led_irq_handler,
			IRQF_ONESHOT,
			"qpnp_flash_led_all_ramp_up_done_irq", led);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Unable to request all_ramp_up_done(%d) IRQ(err:%d)\n",
				led->pdata->all_ramp_up_done_irq, rc);
			goto error_switch_register;
		}
	}

	if (led->pdata->all_ramp_down_done_irq >= 0) {
		rc = devm_request_threaded_irq(&led->pdev->dev,
			led->pdata->all_ramp_down_done_irq,
			NULL, qpnp_flash_led_irq_handler,
			IRQF_ONESHOT,
			"qpnp_flash_led_all_ramp_down_done_irq", led);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Unable to request all_ramp_down_done(%d) IRQ(err:%d)\n",
				led->pdata->all_ramp_down_done_irq, rc);
			goto error_switch_register;
		}
	}

	if (led->pdata->led_fault_irq >= 0) {
		rc = devm_request_threaded_irq(&led->pdev->dev,
			led->pdata->led_fault_irq,
			NULL, qpnp_flash_led_irq_handler,
			IRQF_ONESHOT,
			"qpnp_flash_led_fault_irq", led);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Unable to request led_fault(%d) IRQ(err:%d)\n",
				led->pdata->led_fault_irq, rc);
			goto error_switch_register;
		}
	}

	led->bms_psy = power_supply_get_by_name("bms");
	if (!led->bms_psy) {
		rc = flash_led_psy_register_notifier(led);
		if (rc < 0) {
			dev_err(&pdev->dev, "Couldn't register psy notifier, rc = %d\n",
					rc);
			goto error_switch_register;
		}
	}

	rc = qpnp_flash_led_init_settings(led);
	if (rc < 0) {
		dev_err(&pdev->dev,
				"Failed to initialize flash LED, rc=%d\n", rc);
		goto unreg_notifier;
	}

	spin_lock_init(&led->lock);

	dev_set_drvdata(&pdev->dev, led);

	return 0;

unreg_notifier:
	power_supply_unreg_notifier(&led->nb);
error_switch_register:
	while (i > 0)
		led_classdev_unregister(&led->snode[--i].cdev);
	i = led->num_fnodes;
error_led_register:
	while (i > 0)
		led_classdev_unregister(&led->fnode[--i].cdev);

	return rc;
}

static int qpnp_flash_led_remove(struct platform_device *pdev)
{
	struct qpnp_flash_led *led = dev_get_drvdata(&pdev->dev);
	int i;

	for (i = 0; i < led->num_snodes; i++) {
		if (led->snode[i].num_regulators) {
			if (led->snode[i].regulator_on)
				qpnp_flash_led_regulator_enable(led,
						&led->snode[i], false);
			qpnp_flash_led_regulator_setup(led,
					&led->snode[i], false);
		}
	}

	while (i > 0)
		led_classdev_unregister(&led->snode[--i].cdev);
	i = led->num_fnodes;
	while (i > 0)
		led_classdev_unregister(&led->fnode[--i].cdev);

	power_supply_unreg_notifier(&led->nb);
	return 0;
}

const struct of_device_id qpnp_flash_led_match_table[] = {
	{ .compatible = "qcom,qpnp-flash-led-v2",},
	{ },
};

static struct platform_driver qpnp_flash_led_driver = {
	.driver		= {
		.name = "qcom,qpnp-flash-led-v2",
		.of_match_table = qpnp_flash_led_match_table,
	},
	.probe		= qpnp_flash_led_probe,
	.remove		= qpnp_flash_led_remove,
};

static int __init qpnp_flash_led_init(void)
{
	return platform_driver_register(&qpnp_flash_led_driver);
}
late_initcall(qpnp_flash_led_init);

static void __exit qpnp_flash_led_exit(void)
{
	platform_driver_unregister(&qpnp_flash_led_driver);
}
module_exit(qpnp_flash_led_exit);

MODULE_DESCRIPTION("QPNP Flash LED driver v2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp-flash-v2");
