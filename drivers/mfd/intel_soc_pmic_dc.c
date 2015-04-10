/*
 * Dollar Cove  --  Device access for Intel PMIC for CR
 *
 * Copyright (c) 2014, Intel Corporation.
 *
 * Author: Yang Bin <bin.yang@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/acpi.h>
#include <linux/version.h>
#include <linux/power/dc_xpwr_battery.h>
#include <linux/power/dc_xpwr_charger.h>
#include <linux/regulator/intel_dollar_cove_pmic.h>

#include <asm/intel_em_config.h>
#include <linux/extcon/extcon-dc-pwrsrc.h>
#include <linux/power/battery_id.h>

#include "intel_soc_pmic_core.h"

#define XPOWER_DEFAULT_TEMP_MAX 45

#define GPIO_USB_MUX_INDEX	1
#define GPIO_OTG_VBUS_INDEX	2

enum {
	VBUS_FALLING_IRQ = 2,
	VBUS_RISING_IRQ,
	VBUS_OV_IRQ,
	VBUS_FALLING_ALT_IRQ,
	VBUS_RISING_ALT_IRQ,
	VBUS_OV_ALT_IRQ,

	CHARGE_DONE_IRQ = 10,
	CHARGE_CHARGING_IRQ,
	BAT_SAFE_QUIT_IRQ,
	BAT_SAFE_ENTER_IRQ,
	BAT_ABSENT_IRQ,
	BAT_APPEND_IRQ,

	QWBTU_IRQ = 16,
	WBTU_IRQ,
	QWBTO_IRQ,
	WBTO_IRQ,
	QCBTU_IRQ,
	CBTU_IRQ,
	QCBTO_IRQ,
	CBTO_IRQ,

	WL2_IRQ = 24,
	WL1_IRQ,
	GPADC_IRQ,
	OT_IRQ = 31,

	GPIO0_IRQ = 32,
	GPIO1_IRQ,
	POKO_IRQ,
	POKL_IRQ,
	POKS_IRQ,
	POKN_IRQ,
	POKP_IRQ,
	EVENT_IRQ,

	MV_CHNG_IRQ = 40,
	BC_USB_CHNG_IRQ,
};

static struct resource power_button_resources[] = {
	{
		.start	= POKN_IRQ,
		.end	= POKN_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= POKP_IRQ,
		.end	= POKP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};


static struct resource gpio_resources[] = {
	{
		.start	= GPIO0_IRQ,
		.end	= GPIO1_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource adc_resources[] = {
	{
		.start = GPADC_IRQ,
		.end   = GPADC_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource pwrsrc_resources[] = {
	{
		.start = VBUS_FALLING_IRQ,
		.end   = VBUS_FALLING_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = VBUS_RISING_IRQ,
		.end   = VBUS_RISING_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = MV_CHNG_IRQ,
		.end   = MV_CHNG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = BC_USB_CHNG_IRQ,
		.end   = BC_USB_CHNG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource charger_resources[] = {
	{
		.start = VBUS_OV_IRQ,
		.end   = VBUS_OV_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CHARGE_DONE_IRQ,
		.end   = CHARGE_DONE_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CHARGE_CHARGING_IRQ,
		.end   = CHARGE_CHARGING_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = BAT_SAFE_QUIT_IRQ,
		.end   = BAT_SAFE_QUIT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = BAT_SAFE_ENTER_IRQ,
		.end   = BAT_SAFE_ENTER_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = QCBTU_IRQ,
		.end   = QCBTU_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CBTU_IRQ,
		.end   = CBTU_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = QCBTO_IRQ,
		.end   = QCBTO_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CBTO_IRQ,
		.end   = CBTO_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource battery_resources[] = {
	{
		.start = QWBTU_IRQ,
		.end   = QWBTU_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = WBTU_IRQ,
		.end   = WBTU_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = QWBTO_IRQ,
		.end   = QWBTO_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = WBTO_IRQ,
		.end   = WBTO_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = WL2_IRQ,
		.end   = WL2_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = WL1_IRQ,
		.end   = WL1_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell dollar_cove_dev[] = {
	{
		.name = "dollar_cove_adc",
		.id = 0,
		.num_resources = ARRAY_SIZE(adc_resources),
		.resources = adc_resources,
	},
	{
		.name = "dollar_cove_gpio",
		.id = 0,
		.num_resources = ARRAY_SIZE(gpio_resources),
		.resources = gpio_resources,
	},
	{
		.name = "dollar_cove_power_button",
		.id = 0,
		.num_resources = ARRAY_SIZE(power_button_resources),
		.resources = power_button_resources,
	},
	{
		.name = "dollar_cove_pwrsrc",
		.id = 0,
		.num_resources = ARRAY_SIZE(pwrsrc_resources),
		.resources = pwrsrc_resources,
	},
	{
		.name = "dollar_cove_charger",
		.id = 0,
		.num_resources = ARRAY_SIZE(charger_resources),
		.resources = charger_resources,
	},
	{
		.name = "dollar_cove_battery",
		.id = 0,
		.num_resources = ARRAY_SIZE(battery_resources),
		.resources = battery_resources,
	},
	{
		.name = "dollar_cove_region",
	},
	{
		.name = "dcovex_regulator",
		.id = DCOVEX_ID_LDO2 + 1,
		.num_resources = 0,
		.resources = NULL,
	},
	{
		.name = "dcovex_regulator",
		.id = DCOVEX_ID_LDO3 + 1,
		.num_resources = 0,
		.resources = NULL,
	},
	{NULL, },
};

#define DOLLAR_COVE_IRQREGMAP(irq) \
	[irq] = { \
		{(0x40 + (irq / 8)), (irq % 8), 1, INTEL_PMIC_REG_INV},\
		{(0x48 + (irq / 8)), (irq % 8), 1, INTEL_PMIC_REG_W1C},\
		{(0x48 + (irq / 8)), (irq % 8), 1, INTEL_PMIC_REG_W1C},\
	}

struct intel_pmic_irqregmap dollar_cove_irqregmap[] = {
	DOLLAR_COVE_IRQREGMAP(VBUS_FALLING_IRQ),
	DOLLAR_COVE_IRQREGMAP(VBUS_RISING_IRQ),
	DOLLAR_COVE_IRQREGMAP(VBUS_OV_IRQ),
	DOLLAR_COVE_IRQREGMAP(VBUS_FALLING_ALT_IRQ),
	DOLLAR_COVE_IRQREGMAP(VBUS_RISING_ALT_IRQ),
	DOLLAR_COVE_IRQREGMAP(VBUS_OV_ALT_IRQ),
	DOLLAR_COVE_IRQREGMAP(CHARGE_DONE_IRQ),
	DOLLAR_COVE_IRQREGMAP(CHARGE_CHARGING_IRQ),
	DOLLAR_COVE_IRQREGMAP(BAT_SAFE_QUIT_IRQ),
	DOLLAR_COVE_IRQREGMAP(BAT_SAFE_ENTER_IRQ),
	DOLLAR_COVE_IRQREGMAP(BAT_ABSENT_IRQ),
	DOLLAR_COVE_IRQREGMAP(BAT_APPEND_IRQ),
	DOLLAR_COVE_IRQREGMAP(QWBTU_IRQ),
	DOLLAR_COVE_IRQREGMAP(WBTU_IRQ),
	DOLLAR_COVE_IRQREGMAP(QWBTO_IRQ),
	DOLLAR_COVE_IRQREGMAP(WBTO_IRQ),
	DOLLAR_COVE_IRQREGMAP(QCBTU_IRQ),
	DOLLAR_COVE_IRQREGMAP(CBTU_IRQ),
	DOLLAR_COVE_IRQREGMAP(QCBTO_IRQ),
	DOLLAR_COVE_IRQREGMAP(CBTO_IRQ),
	DOLLAR_COVE_IRQREGMAP(WL2_IRQ),
	DOLLAR_COVE_IRQREGMAP(WL1_IRQ),
	DOLLAR_COVE_IRQREGMAP(GPADC_IRQ),
	DOLLAR_COVE_IRQREGMAP(OT_IRQ),
	DOLLAR_COVE_IRQREGMAP(GPIO0_IRQ),
	DOLLAR_COVE_IRQREGMAP(GPIO1_IRQ),
	DOLLAR_COVE_IRQREGMAP(POKO_IRQ),
	DOLLAR_COVE_IRQREGMAP(POKL_IRQ),
	DOLLAR_COVE_IRQREGMAP(POKS_IRQ),
	DOLLAR_COVE_IRQREGMAP(POKN_IRQ),
	DOLLAR_COVE_IRQREGMAP(POKP_IRQ),
	DOLLAR_COVE_IRQREGMAP(EVENT_IRQ),
	DOLLAR_COVE_IRQREGMAP(MV_CHNG_IRQ),
	DOLLAR_COVE_IRQREGMAP(BC_USB_CHNG_IRQ),
};


static struct ps_batt_chg_prof ps_batt_chrg_prof;
static struct ps_pse_mod_prof *pse_mod_prof;
#ifdef CONFIG_POWER_SUPPLY_CHARGER

#define DC_CHRG_CHRG_CUR_NOLIMIT	1800
#define DC_CHRG_CHRG_CUR_MEDIUM		1400
#define DC_CHRG_CHRG_CUR_LOW		1000

static struct power_supply_throttle dc_chrg_throttle_states[] = {
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = DC_CHRG_CHRG_CUR_NOLIMIT,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = DC_CHRG_CHRG_CUR_MEDIUM,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = DC_CHRG_CHRG_CUR_LOW,
	},
	{
		.throttle_action = PSY_THROTTLE_DISABLE_CHARGING,
	},
};

static char *dc_chrg_supplied_to[] = {
	"dollar_cove_battery"
};

static void *platform_get_batt_charge_profile(void)
{
	int ret;

	ret = get_batt_prop(&ps_batt_chrg_prof);
	pse_mod_prof = (struct ps_pse_mod_prof *)
					ps_batt_chrg_prof.batt_prof;
	if ((ret < 0 || ps_batt_chrg_prof.chrg_prof_type == CHRG_PROF_NONE)
		&& pse_mod_prof)
			snprintf(pse_mod_prof->batt_id, (BATTID_STR_LEN + 1),
				"%s", BATTID_UNKNOWN);

	return &ps_batt_chrg_prof;
}

static void platform_init_chrg_params(struct dollarcove_chrg_pdata *pdata)
{
	struct ps_pse_mod_prof *prof;
	int i;
	pdata->throttle_states = dc_chrg_throttle_states;
	pdata->supplied_to = dc_chrg_supplied_to;
	pdata->num_throttle_states = ARRAY_SIZE(dc_chrg_throttle_states);
	pdata->num_supplicants = ARRAY_SIZE(dc_chrg_supplied_to);
	pdata->supported_cables = POWER_SUPPLY_CHARGER_TYPE_USB;
	pdata->chg_profile = (struct ps_batt_chg_prof *)
			platform_get_batt_charge_profile();
	/* Initialize the default parameters */
	pdata->def_cc = 500;
	pdata->def_cv = 4350;
	pdata->def_ilim = 900;
	pdata->def_iterm = 300;
	pdata->def_max_temp = 55;
	pdata->def_min_temp = 0;

	/* Initialize with default values if no charge profile */
	if (!pdata->chg_profile) {
		pdata->max_cc = 2000;
		pdata->max_cv = 4350;
		return;
	}
	prof = (struct ps_pse_mod_prof *)pdata->chg_profile->batt_prof;
	/* Read max parameters from charge profile */
	for (i = 0; i < prof->temp_mon_ranges; i++) {
		if (prof->temp_mon_range[i].full_chrg_vol > pdata->max_cv)
			pdata->max_cv = prof->temp_mon_range[i].full_chrg_vol;
		if (prof->temp_mon_range[i].full_chrg_cur > pdata->max_cc)
			pdata->max_cc = prof->temp_mon_range[i].full_chrg_cur;
	}
	if (pdata->def_cc > pdata->max_cc)
		pdata->def_cc = pdata->max_cc;
	if (pdata->def_cv > pdata->max_cv)
		pdata->def_cv = pdata->max_cv;
}

static void platform_set_battery_data(struct dollarcove_fg_pdata *pdata,
	struct ps_batt_chg_prof *chg_prof)
{
	struct ps_pse_mod_prof *prof = (struct ps_pse_mod_prof *)
		chg_prof->batt_prof;

	/*
	 * Get the data from ACPI Table OEM0 if it is available
	 * Also make sure we are not setting value of 0 as acpi table may
	 * sometimes incorrectly set them to 0.
	 */
	if (chg_prof->chrg_prof_type == PSE_MOD_CHRG_PROF) {
		pdata->design_cap = prof->capacity ? prof->capacity : 4045;
		pdata->design_max_volt = prof->voltage_max ? prof->voltage_max : 4350;
		pdata->design_min_volt = prof->low_batt_mV ? prof->low_batt_mV : 3400;
	} else {
		pdata->design_cap = 4045;
		pdata->design_max_volt = 4350;
		pdata->design_min_volt = 3400;
	}
}

#else

static void platform_init_chrg_params(struct dollarcove_chrg_pdata *pdata)
{
}

static void platform_set_battery_data(struct dollarcove_fg_pdata *pdata,
	struct ps_batt_chg_prof *chg_prof)
{
	pdata->design_cap = 4045;
	pdata->design_max_volt = 4350;
	pdata->design_min_volt = 3400;
}

#endif

static void dc_xpwr_chrg_pdata(void)
{
	static struct dollarcove_chrg_pdata pdata;
	struct gpio_desc *gpio_desc;


	gpio_desc = devm_gpiod_get_index(dollar_cove_pmic.dev,
			"pmic_res", GPIO_OTG_VBUS_INDEX);
	if (IS_ERR(gpio_desc))
		pdata.otg_gpio = -1;
	else {
		pdata.otg_gpio = desc_to_gpio(gpio_desc);
		gpiod_put(gpio_desc);
	}

	platform_init_chrg_params(&pdata);
	intel_soc_pmic_set_pdata("dollar_cove_charger",
				(void *)&pdata, sizeof(pdata), 0);
}

static int fg_bat_curve[] = {
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x2,
	0x2, 0x3, 0x5, 0x9, 0xf, 0x18, 0x24, 0x29,
    0x2e, 0x32, 0x35, 0x3b, 0x40, 0x45, 0x49, 0x4c,
    0x50, 0x53, 0x55, 0x57, 0x5a, 0x5d, 0x61, 0x64,
};

#ifdef CONFIG_ACPI
#define FGCONFIG_ACPI_TABLE_NAME	"BCFG"
#define XPWR_FGCONFIG_NAME		"XPOWER-0"

static int
dc_xpwr_get_acpi_cdata(struct dollarcove_fg_pdata *pdata)
{
	struct dc_xpwr_acpi_fg_config *acpi_tbl = NULL;
	char *name = FGCONFIG_ACPI_TABLE_NAME;
	acpi_size tbl_size;
	acpi_status status;

	/* read the fg config table from acpi */
	status = acpi_get_table_with_size(name , 0,
			(struct acpi_table_header **)&acpi_tbl, &tbl_size);
	if (ACPI_FAILURE(status)) {
		pr_err("%s:%s table not found!!\n", __func__, name);
		return status;
	}
	pr_info("%s: %s table found, size=%d\n",
				__func__, name, (int)tbl_size);

	/* validate the table size */
	if (tbl_size <  sizeof(struct dc_xpwr_acpi_fg_config)) {
		pr_err("%s:%s table incomplete!!\n", __func__, name);
		pr_info("%s: table_size=%d, structure_size=%lu\n",
			__func__, (int)tbl_size,
			sizeof(struct dc_xpwr_acpi_fg_config));
		return -ENXIO;
	}

	if (strncmp(acpi_tbl->cdata.fg_name, XPWR_FGCONFIG_NAME,
		ACPI_FG_CONF_NAME_LEN)
		|| strncmp(acpi_tbl->cdata.battid, pdata->battid,
		BATTID_STR_LEN)) {
		pr_err("%s: battid and fg_name mismatch!!!\n", __func__);
		return -EINVAL;
	}

	memcpy(&pdata->cdata, &acpi_tbl->cdata,
			sizeof(struct dc_xpwr_fg_config_data));

	return 0;
}
#endif /* CONFIG_ACPI */

static void dc_xpwr_get_fg_config_data(struct dollarcove_fg_pdata *pdata)
{
	int scaled_capacity;
	int i;

#ifdef CONFIG_ACPI
	if (!dc_xpwr_get_acpi_cdata(pdata)) {
		pr_info("%s: Loading fg config from acpi table\n", __func__);
		pdata->fg_save_restore_enabled = 1;
		return;
	}
#endif /* CONFIG_ACPI */

	pr_info("%s: Loading default fg config.\n", __func__);
	/*
	 * Calculate cap1 and cap0.  The value of a LSB is 1.456mAh.
	 * Using 1.5 as math friendly and close enough.
	 */

	scaled_capacity = (pdata->design_cap >> 1) +
				(pdata->design_cap >> 3) +
				(pdata->design_cap >> 4);

	/*
	 * bit 7 of cap1 register is set to indicate battery maximum
	 * capacity is valid
	 */
	pdata->cdata.cap0 = scaled_capacity & 0xFF;
	pdata->cdata.cap1 = (scaled_capacity >> 8) | 0x80;

	pdata->cdata.rdc1 = 0xc0;
	pdata->cdata.rdc0 = 0x97;

	/* Donot update the entire fg  data on every boot*/
	pdata->cdata.fco = 0x00;
	/* copy curve data */
	for (i = 0; i < XPWR_BAT_CURVE_SIZE; i++)
		pdata->cdata.bat_curve[i] = fg_bat_curve[i];

	pdata->fg_save_restore_enabled = 0;
	return;
}

static void dc_xpwr_fg_pdata(void)
{
	static struct dollarcove_fg_pdata pdata;
	int i, temp_mon_ranges;
	int scaled_capacity;

	if (ps_batt_chrg_prof.chrg_prof_type == CHRG_PROF_NONE
		|| (!pse_mod_prof))
		snprintf(pdata.battid, (BATTID_STR_LEN + 1),
			"%s", BATTID_UNKNOWN);
	else {
		memcpy(pdata.battid, pse_mod_prof->batt_id,
				strlen(pse_mod_prof->batt_id));
		pdata.battid[BATTID_STR_LEN] = '\0';
	}

	platform_set_battery_data(&pdata, &ps_batt_chrg_prof);
	pse_mod_prof = (struct ps_pse_mod_prof *)
					ps_batt_chrg_prof.batt_prof;
	temp_mon_ranges = min_t(u16, pse_mod_prof->temp_mon_ranges,
						BATT_TEMP_NR_RNG);
	for (i = 0; i < temp_mon_ranges; ++i) {
		if (pse_mod_prof->temp_mon_range[i].full_chrg_cur)
				break;
	}
	if (i < temp_mon_ranges)
		pdata.max_temp = pse_mod_prof->temp_mon_range[i].temp_up_lim;
	else
		pdata.max_temp = XPOWER_DEFAULT_TEMP_MAX;

	pdata.min_temp = pse_mod_prof->temp_low_lim;

	/* Load FG config data to pdata */
	dc_xpwr_get_fg_config_data(&pdata);

	intel_soc_pmic_set_pdata("dollar_cove_battery",
				(void *)&pdata, sizeof(pdata), 0);
}

static void dc_xpwr_pwrsrc_pdata(void)
{
	static struct dc_xpwr_pwrsrc_pdata pdata;

	/*
	 * set en_chrg_det to true if the
	 * D+/D- lines are connected to
	 * PMIC itself.
	 */
	pdata.en_chrg_det = true;

	/* Get the gpio based uab mux which will be used to switch usb D+/D-
	 * data line between SOC for data communication and PMIC for charger
	 * detection functionality.
	 */
	pdata.gpio_mux_cntl = devm_gpiod_get_index(dollar_cove_pmic.dev,
				"pmic_res", GPIO_USB_MUX_INDEX);
	if (IS_ERR(pdata.gpio_mux_cntl))
		pdata.gpio_mux_cntl = NULL;
	else
		gpiod_put(pdata.gpio_mux_cntl);

	intel_soc_pmic_set_pdata("dollar_cove_pwrsrc",
				 (void *)&pdata, sizeof(pdata), 0);
}

static struct pmic_gpio_data dollar_cove_gpio_data = {
	.type = DOLLAR_COVE,
	.num_gpio = 2,
	.num_vgpio = 0x5e,
};

static void dc_set_gpio_pdata(void)
{
	intel_soc_pmic_set_pdata("dollar_cove_gpio",
				(void *)&dollar_cove_gpio_data,
				sizeof(dollar_cove_gpio_data), 0);
}


static int dollar_cove_init(void)
{
	pr_info("Dollar Cove: IC_TYPE 0x%02X\n", intel_soc_pmic_readb(0x03));
	dc_xpwr_chrg_pdata();
	dc_xpwr_pwrsrc_pdata();
	dc_xpwr_fg_pdata();
	dc_set_gpio_pdata();

	return 0;
}

struct intel_soc_pmic dollar_cove_pmic = {
	.label		= "dollar cove",
	.irq_flags	= IRQF_TRIGGER_FALLING | IRQF_ONESHOT
				| IRQF_NO_SUSPEND,
	.init		= dollar_cove_init,
	.cell_dev       = dollar_cove_dev,
	.irq_regmap	= dollar_cove_irqregmap,
	.irq_num	= 48,
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yang Bin <bin.yang@intel.com");
