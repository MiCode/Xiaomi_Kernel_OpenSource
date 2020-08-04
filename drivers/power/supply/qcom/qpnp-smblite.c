// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/log2.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/iio/consumer.h>
#include <linux/pmic-voter.h>
#include <linux/usb/typec.h>
#include "smblite-reg.h"
#include "smblite-lib.h"
#include "schgm-flashlite.h"

static struct power_supply_desc usb_psy_desc;

static struct smb_params smblite_params = {
	.fcc			= {
		.name   = "fast charge current",
		.reg    = CHGR_FAST_CHARGE_CURRENT_CFG_REG,
		.min_u  = 0,
		.max_u  = 2000000,
		.step_u = 100000,
	},
	.fv			= {
		.name   = "float voltage",
		.reg    = CHGR_FLOAT_VOLTAGE_CFG_REG,
		.min_u  = 3600000,
		.max_u  = 4600000,
		.step_u = 20000,
	},
	.usb_icl		= {
		.name   = "usb input current limit",
		.reg    = USBIN_CURRENT_LIMIT_CFG_REG,
		.min_u  = 0,
		.max_u  = 2000000,
		.step_u = 100000,
	},
	.icl_max_stat		= {
		.name   = "dcdc icl max status",
		.reg    = ICL_MAX_STATUS_REG,
		.min_u  = 0,
		.max_u  = 2000000,
		.step_u = 100000,
	},
	.icl_stat		= {
		.name   = "input current limit status",
		.reg    = ICL_STATUS_REG,
		.min_u  = 0,
		.max_u  = 2000000,
		.step_u = 100000,
	},
	.aicl_5v_threshold		= {
		.name   = "AICL 5V threshold",
		.reg    = USBIN_LV_AICL_THRESHOLD_REG,
		.min_u  = 4200,
		.max_u  = 4800,
		.step_u = 200,
	},
};

struct smb_dt_props {
	int			usb_icl_ua;
	struct device_node	*revid_dev_node;
	int			chg_inhibit_thr_mv;
	bool			no_battery;
	int			auto_recharge_soc;
	int			auto_recharge_vbat_mv;
	int			wd_bark_time;
	int			batt_profile_fcc_ua;
	int			batt_profile_fv_uv;
	int			term_current_src;
	int			term_current_thresh_hi_ma;
	int			term_current_thresh_lo_ma;
	int			disable_suspend_on_collapse;
};

struct smblite {
	struct smb_charger	chg;
	struct dentry		*dfs_root;
	struct smb_dt_props	dt;
};

static int __debug_mask;

static ssize_t weak_chg_icl_ua_show(struct device *dev, struct device_attribute
				    *attr, char *buf)
{
	struct smblite *chip = dev_get_drvdata(dev);
	struct smb_charger *chg = &chip->chg;

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->weak_chg_icl_ua);
}

static ssize_t weak_chg_icl_ua_store(struct device *dev, struct device_attribute
				 *attr, const char *buf, size_t count)
{
	int val;
	struct smblite *chip = dev_get_drvdata(dev);
	struct smb_charger *chg = &chip->chg;

	if (kstrtos32(buf, 0, &val))
		return -EINVAL;

	chg->weak_chg_icl_ua = val;

	return count;
}
static DEVICE_ATTR_RW(weak_chg_icl_ua);

static struct attribute *smblite_attrs[] = {
	&dev_attr_weak_chg_icl_ua.attr,
	NULL,
};
ATTRIBUTE_GROUPS(smblite);

static int smblite_chg_config_init(struct smblite *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct pmic_revid_data *pmic_rev_id;
	struct device_node *revid_dev_node, *node = chg->dev->of_node;
	u8 val;
	int rc = 0;

	revid_dev_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property\n");
		return -EINVAL;
	}

	pmic_rev_id = get_revid_data(revid_dev_node);
	if (IS_ERR_OR_NULL(pmic_rev_id)) {
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		rc =  -EPROBE_DEFER;
		goto out;
	}

	switch (pmic_rev_id->pmic_subtype) {
	case PM2250_SUBTYPE:
		chg->wa_flags |= WEAK_ADAPTER_WA;
		if (pmic_rev_id->rev4 < 2)
			chg->wa_flags |= FLASH_DIE_TEMP_DERATE_WA;
		break;
	default:
		pr_err("Unsupported PMIC subtype=%d\n",
				pmic_rev_id->pmic_subtype);
		break;
	}

	rc = smblite_lib_read(chg, DCDC_LDO_CFG_REG, &val);
	if (rc < 0) {
		pr_err("Couldn't read LDO config reg rc=%d\n", rc);
		return rc;
	}
	chg->ldo_mode = !!(val & LDO_MODE_BIT);

	chip->chg.chg_param.smb_version = 0;
	chg->param = smblite_params;
	chg->name = "PM2250_charger";

out:
	of_node_put(revid_dev_node);
	return rc;
}

#define MICRO_1P5A			1500000
#define MICRO_P1A			100000
#define MICRO_1PA			1000000
#define MICRO_3PA			3000000
#define OTG_DEFAULT_DEGLITCH_TIME_MS	50
#define DEFAULT_WD_BARK_TIME		16
#define DEFAULT_FCC_STEP_SIZE_UA	100000
#define DEFAULT_FCC_STEP_UPDATE_DELAY_MS	1000
static int smblite_parse_dt_misc(struct smblite *chip, struct device_node *node)
{
	int rc = 0, byte_len;
	struct smb_charger *chg = &chip->chg;

	chg->typec_legacy_use_rp_icl = of_property_read_bool(node,
				"qcom,typec-legacy-rp-icl");

	rc = of_property_read_u32(node, "qcom,wd-bark-time-secs",
					&chip->dt.wd_bark_time);
	if (rc < 0 || chip->dt.wd_bark_time < MIN_WD_BARK_TIME)
		chip->dt.wd_bark_time = DEFAULT_WD_BARK_TIME;


	chip->dt.no_battery = of_property_read_bool(node,
						"qcom,batteryless-platform");

	if (of_find_property(node, "qcom,thermal-mitigation", &byte_len)) {
		chg->thermal_mitigation = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->thermal_mitigation == NULL)
			return -ENOMEM;

		chg->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"qcom,thermal-mitigation",
				chg->thermal_mitigation,
				chg->thermal_levels);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	chip->dt.auto_recharge_soc = -EINVAL;
	rc = of_property_read_u32(node, "qcom,auto-recharge-soc",
				&chip->dt.auto_recharge_soc);
	if ((rc < 0) || (chip->dt.auto_recharge_soc < 0 ||
			chip->dt.auto_recharge_soc > 100)) {
		pr_err("qcom,auto-recharge-soc is incorrect\n");
		return -EINVAL;
	}
	chg->auto_recharge_soc = chip->dt.auto_recharge_soc;

	chg->suspend_input_on_debug_batt = of_property_read_bool(node,
					"qcom,suspend-input-on-debug-batt");

	chg->fake_chg_status_on_debug_batt = of_property_read_bool(node,
					"qcom,fake-chg-status-on-debug-batt");

	chg->fcc_stepper_enable = of_property_read_bool(node,
					"qcom,fcc-stepping-enable");

	chip->dt.disable_suspend_on_collapse = of_property_read_bool(node,
					"qcom,disable-suspend-on-collapse");

	of_property_read_u32(node, "qcom,fcc-step-delay-ms",
					&chg->chg_param.fcc_step_delay_ms);
	if (chg->chg_param.fcc_step_delay_ms <= 0)
		chg->chg_param.fcc_step_delay_ms =
					DEFAULT_FCC_STEP_UPDATE_DELAY_MS;

	of_property_read_u32(node, "qcom,fcc-step-size-ua",
					&chg->chg_param.fcc_step_size_ua);
	if (chg->chg_param.fcc_step_size_ua <= 0)
		chg->chg_param.fcc_step_size_ua = DEFAULT_FCC_STEP_SIZE_UA;

	return 0;
}

static int smblite_parse_dt_adc_channels(struct smb_charger *chg)
{
	int rc = 0;

	rc = smblite_lib_get_iio_channel(chg, "usb_in_voltage",
					&chg->iio.usbin_v_chan);
	if (rc < 0)
		return rc;

	rc = smblite_lib_get_iio_channel(chg, "chg_temp",
					&chg->iio.temp_chan);
	if (rc < 0)
		return rc;

	return 0;
}

static int smblite_parse_dt_currents(struct smblite *chip,
					struct device_node *node)
{
	int rc = 0;

	rc = of_property_read_u32(node,
			"qcom,fcc-max-ua", &chip->dt.batt_profile_fcc_ua);
	if (rc < 0)
		chip->dt.batt_profile_fcc_ua = -EINVAL;

	rc = of_property_read_u32(node,
				"qcom,usb-icl-ua", &chip->dt.usb_icl_ua);
	if (rc < 0)
		chip->dt.usb_icl_ua = -EINVAL;

	rc = of_property_read_u32(node, "qcom,chg-term-src",
			&chip->dt.term_current_src);
	if (rc < 0)
		chip->dt.term_current_src = ITERM_SRC_UNSPECIFIED;

	if (chip->dt.term_current_src == ITERM_SRC_ADC)
		rc = of_property_read_u32(node, "qcom,chg-term-base-current-ma",
				&chip->dt.term_current_thresh_lo_ma);

	rc = of_property_read_u32(node, "qcom,chg-term-current-ma",
			&chip->dt.term_current_thresh_hi_ma);

	return 0;
}

static int smblite_parse_dt_voltages(struct smblite *chip,
					struct device_node *node)
{
	int rc = 0, delta_mv = 4200;

	rc = of_property_read_u32(node,
				"qcom,fv-max-uv", &chip->dt.batt_profile_fv_uv);
	if (rc < 0)
		chip->dt.batt_profile_fv_uv = -EINVAL;
	else
		delta_mv = chip->dt.batt_profile_fv_uv - 50;

	chip->dt.chg_inhibit_thr_mv = -EINVAL;
	rc = of_property_read_u32(node, "qcom,chg-inhibit-threshold-mv",
				&chip->dt.chg_inhibit_thr_mv);
	if (!rc && (chip->dt.chg_inhibit_thr_mv <= 0 ||
			chip->dt.chg_inhibit_thr_mv > delta_mv)) {
		pr_err("qcom,chg-inhibit-threshold-mv is incorrect\n");
		return -EINVAL;
	}

	chip->dt.auto_recharge_vbat_mv = -EINVAL;
	rc = of_property_read_u32(node, "qcom,auto-recharge-vbat-mv",
				&chip->dt.auto_recharge_vbat_mv);
	if (!rc && (chip->dt.auto_recharge_vbat_mv <= 0 ||
			chip->dt.auto_recharge_vbat_mv > delta_mv)) {
		pr_err("qcom,auto-recharge-vbat-mv is incorrect\n");
		return -EINVAL;
	}

	return 0;
}

static int smblite_parse_dt(struct smblite *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	int rc = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = smblite_parse_dt_voltages(chip, node);
	if (rc < 0)
		return rc;

	rc = smblite_parse_dt_currents(chip, node);
	if (rc < 0)
		return rc;

	rc = smblite_parse_dt_adc_channels(chg);
	if (rc < 0)
		return rc;

	rc = smblite_parse_dt_misc(chip, node);
	if (rc < 0)
		return rc;

	return 0;
}

/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property smblite_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_SDP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_TYPEC_POWER_ROLE,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_HW_CURRENT_MAX,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_CONNECTOR_TYPE,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int smblite_usb_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smblite *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	val->intval = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblite_lib_get_prop_usb_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = smblite_lib_get_usb_online(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = smblite_lib_get_prop_usb_voltage_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_SDP_CURRENT_MAX:
		rc = smblite_lib_get_charge_current(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = chg->real_charger_type;
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		rc = smblite_lib_get_usb_prop_typec_mode(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		rc = smblite_lib_get_prop_typec_power_role(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		rc = smblite_lib_get_prop_typec_cc_orientation(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		rc = smblite_lib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_HW_CURRENT_MAX:
		rc = smblite_lib_get_hw_current_max(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TYPE:
		val->intval = chg->connector_type;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		rc = smblite_lib_get_prop_scope(chg, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = get_effective_result_locked(chg->usb_icl_votable);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	default:
		pr_err("get prop %d is not supported in usb\n", psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

void smblite_update_usb_desc(struct smb_charger *chg)
{
	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB:
		usb_psy_desc.type = chg->real_charger_type;
		break;
	default:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	}
}

#define MIN_THERMAL_VOTE_UA	500000
static int smblite_usb_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smblite *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_SDP_CURRENT_MAX:
		rc = smblite_lib_set_prop_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		rc = smblite_lib_set_prop_usb_type(chg, val);
		break;
	default:
		pr_err("set prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int smblite_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc = 0;

	switch (psp) {
	default:
		rc = 0;
		break;
	}

	return rc;
}

static struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = smblite_usb_props,
	.num_properties = ARRAY_SIZE(smblite_usb_props),
	.get_property = smblite_usb_get_prop,
	.set_property = smblite_usb_set_prop,
	.property_is_writeable = smblite_usb_prop_is_writeable,
};

static int smblite_init_usb_psy(struct smblite *chip)
{
	struct power_supply_config usb_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_cfg.drv_data = chip;
	usb_cfg.of_node = chg->dev->of_node;
	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}

	return 0;
}

/*****************************
 * USB MAIN PSY REGISTRATION *
 *****************************/

static enum power_supply_property smblite_usb_main_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED,
	POWER_SUPPLY_PROP_FCC_DELTA,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_FLASH_TRIGGER,
	POWER_SUPPLY_PROP_FLASH_ACTIVE,
};

static int smblite_usb_main_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smblite *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblite_lib_get_charge_param(chg, &chg->param.fv,
							&val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblite_lib_get_charge_param(chg, &chg->param.fcc,
							&val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_MAIN;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		rc = smblite_lib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED:
		rc = smblite_lib_get_prop_input_voltage_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_FCC_DELTA:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblite_lib_get_icl_current(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_FLASH_TRIGGER:
		rc = schgm_flashlite_get_vreg_ok(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_FLASH_ACTIVE:
		val->intval = chg->flash_active;
		break;
	default:
		pr_debug("get prop %d is not supported in usb-main\n", psp);
		rc = -EINVAL;
		break;
	}
	if (rc < 0)
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);

	return rc;
}

static int smblite_usb_main_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smblite *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0, icl_ua;
	union power_supply_propval pval = {0, };

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblite_lib_set_charge_param(chg, &chg->param.fv,
						val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblite_lib_set_charge_param(chg, &chg->param.fcc,
						val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblite_lib_set_icl_current(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_FLASH_ACTIVE:
		if (chg->flash_active != val->intval) {
			chg->flash_active = val->intval;

			rc = smblite_lib_get_prop_usb_present(chg, &pval);
			if (rc < 0)
				pr_err("Failed to get USB present status rc=%d\n",
						rc);
			if (!pval.intval) {
				/* vote 100ma when usb is not present*/
				vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER,
							true, USBIN_100UA);
			} else if (chg->flash_active) {
				icl_ua = get_effective_result_locked(
						chg->usb_icl_votable);
				if (icl_ua >= USBIN_400UA) {
					vote(chg->usb_icl_votable,
						FLASH_ACTIVE_VOTER,
						true, icl_ua - USBIN_300UA);
				}
			} else {
				vote(chg->usb_icl_votable, FLASH_ACTIVE_VOTER,
							false, 0);
			}
			pr_debug("flash_active=%d usb_present=%d icl=%d\n",
				chg->flash_active, pval.intval,
				get_effective_result_locked(
				chg->usb_icl_votable));

		}
		break;
	default:
		pr_err("set prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int smblite_usb_main_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	int rc = 0;

	switch (psp) {
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc usb_main_psy_desc = {
	.name		= "main",
	.type		= POWER_SUPPLY_TYPE_MAIN,
	.properties	= smblite_usb_main_props,
	.num_properties	= ARRAY_SIZE(smblite_usb_main_props),
	.get_property	= smblite_usb_main_get_prop,
	.set_property	= smblite_usb_main_set_prop,
	.property_is_writeable = smblite_usb_main_prop_is_writeable,
};

static int smblite_init_usb_main_psy(struct smblite *chip)
{
	struct power_supply_config usb_main_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_main_cfg.drv_data = chip;
	usb_main_cfg.of_node = chg->dev->of_node;
	chg->usb_main_psy = devm_power_supply_register(chg->dev,
						  &usb_main_psy_desc,
						  &usb_main_cfg);
	if (IS_ERR(chg->usb_main_psy)) {
		pr_err("Couldn't register USB main power supply\n");
		return PTR_ERR(chg->usb_main_psy);
	}

	return 0;
}

/*************************
 * BATT PSY REGISTRATION *
 *************************/
static enum power_supply_property smblite_batt_props[] = {
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGER_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_SW_JEITA_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_DONE,
	POWER_SUPPLY_PROP_SET_SHIP_MODE,
	POWER_SUPPLY_PROP_DIE_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_RECHARGE_SOC,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_FORCE_RECHARGE,
	POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
};

#define DEBUG_ACCESSORY_TEMP_DECIDEGC	250
static int smblite_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb_charger *chg = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smblite_lib_get_prop_batt_status(chg, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = smblite_lib_get_prop_batt_health(chg, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblite_lib_get_prop_batt_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smblite_lib_get_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = smblite_lib_get_prop_batt_charge_type(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smblite_lib_get_prop_batt_capacity(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = smblite_lib_get_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		rc = smblite_lib_get_prop_system_temp_level_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP:
		rc = smblite_lib_get_prop_charger_temp(chg, val);
		break;
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		val->intval = chg->step_chg_enabled;
		break;
	case POWER_SUPPLY_PROP_SW_JEITA_ENABLED:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = smblite_lib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = get_client_vote(chg->fv_votable,
					      BATT_PROFILE_VOTER);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = smblite_lib_get_batt_current_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_client_vote(chg->fcc_votable,
					      BATT_PROFILE_VOTER);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = get_effective_result(chg->fcc_votable);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		rc = smblite_lib_get_prop_batt_iterm(chg, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chg->typec_mode == POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY)
			val->intval = DEBUG_ACCESSORY_TEMP_DECIDEGC;
		else
			rc = smblite_lib_get_prop_from_bms(chg,
						POWER_SUPPLY_PROP_TEMP, val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		rc = smblite_lib_get_prop_batt_charge_done(chg, val);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as device is active */
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		rc = smblite_lib_get_die_health(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = smblite_lib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CHARGE_COUNTER, val);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = smblite_lib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CYCLE_COUNT, val);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		val->intval = chg->auto_recharge_soc;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = smblite_lib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CHARGE_FULL, val);
		break;
	case POWER_SUPPLY_PROP_FORCE_RECHARGE:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE:
		val->intval = chg->fcc_stepper_enable;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		rc = smblite_lib_get_prop_input_current_limited(chg, val);
		break;
	default:
		pr_err("batt power supply prop %d not supported\n", psp);
		return -EINVAL;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int smblite_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct smb_charger *chg = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smblite_lib_set_prop_batt_status(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smblite_lib_set_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = smblite_lib_set_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smblite_lib_set_prop_batt_capacity(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		chg->batt_profile_fv_uv = val->intval;
		vote(chg->fv_votable, BATT_PROFILE_VOTER, true, val->intval);
		break;
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		chg->step_chg_enabled = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		chg->batt_profile_fcc_ua = val->intval;
		vote(chg->fcc_votable, BATT_PROFILE_VOTER, true, val->intval);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as the device is active */
		if (!val->intval)
			break;
		if (chg->pl.psy)
			power_supply_set_property(chg->pl.psy,
				POWER_SUPPLY_PROP_SET_SHIP_MODE, val);
		rc = smblite_lib_set_prop_ship_mode(chg, val);
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		power_supply_changed(chg->batt_psy);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		rc = smblite_lib_set_prop_rechg_soc_thresh(chg, val);
		break;
	case POWER_SUPPLY_PROP_FORCE_RECHARGE:
		/* toggle charging to force recharge */
		vote(chg->chg_disable_votable, FORCE_RECHARGE_VOTER,
					true, 0);
		/* charge disable delay */
		msleep(50);
		vote(chg->chg_disable_votable, FORCE_RECHARGE_VOTER,
					false, 0);
		break;
	case POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE:
		chg->fcc_stepper_enable = val->intval;
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static int smblite_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_DIE_HEALTH:
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = smblite_batt_props,
	.num_properties = ARRAY_SIZE(smblite_batt_props),
	.get_property = smblite_batt_get_prop,
	.set_property = smblite_batt_set_prop,
	.property_is_writeable = smblite_batt_prop_is_writeable,
};

static int smblite_init_batt_psy(struct smblite *chip)
{
	struct power_supply_config batt_cfg = {};
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;
	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);
	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}

	return rc;
}

/***************************
 * HARDWARE INITIALIZATION *
 ***************************/
static int smblite_configure_typec(struct smb_charger *chg)
{
	union power_supply_propval pval = {0, };
	int rc;
	u8 val = 0;

	rc = smblite_lib_read(chg, LEGACY_CABLE_STATUS_REG, &val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't read Legacy status rc=%d\n", rc);
		return rc;
	}

	/*
	 * Across reboot, standard typeC cables get detected as legacy cables
	 * due to VBUS attachment prior to CC attach/dettach, reset legacy cable
	 * detection by disabling/enabling typeC mode.
	 */
	if (val & TYPEC_LEGACY_CABLE_STATUS_BIT) {
		pval.intval = POWER_SUPPLY_TYPEC_PR_NONE;
		smblite_lib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable TYPEC rc=%d\n", rc);
			return rc;
		}

		/* delay before enabling typeC */
		msleep(50);

		pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		smblite_lib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable TYPEC rc=%d\n", rc);
			return rc;
		}
	}

	/* Use simple write to clear interrupts */
	rc = smblite_lib_write(chg, TYPE_C_INTERRUPT_EN_CFG_1_REG, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	rc = smblite_lib_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	rc = smblite_lib_masked_write(chg, TYPE_C_MODE_CFG_REG,
					EN_TRY_SNK_BIT | EN_SNK_ONLY_BIT,
					EN_TRY_SNK_BIT);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure TYPE_C_MODE_CFG_REG rc=%d\n",
				rc);
		return rc;
	}

	rc = smblite_lib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG,
				SEL_SRC_UPPER_REF_BIT, SEL_SRC_UPPER_REF_BIT);
	if (rc < 0)
		dev_err(chg->dev,
			"Couldn't configure CC threshold voltage rc=%d\n", rc);

	return rc;
}

#define RAW_ITERM(iterm_ma, max_range)				\
		div_s64((int64_t)iterm_ma * ADC_CHG_ITERM_MASK, max_range)
static int smblite_configure_iterm_thresholds_adc(struct smblite *chip)
{
	u8 *buf;
	int rc = 0;
	s16 raw_hi_thresh, raw_lo_thresh, max_limit_ma;
	struct smb_charger *chg = &chip->chg;

	max_limit_ma = ITERM_LIMITS_MA;

	if (chip->dt.term_current_thresh_hi_ma < (-1 * max_limit_ma)
		|| chip->dt.term_current_thresh_hi_ma > max_limit_ma
		|| chip->dt.term_current_thresh_lo_ma < (-1 * max_limit_ma)
		|| chip->dt.term_current_thresh_lo_ma > max_limit_ma) {
		dev_err(chg->dev, "ITERM threshold out of range rc=%d\n", rc);
		return -EINVAL;
	}

	/*
	 * Conversion:
	 *	raw (A) = (term_current * ADC_CHG_ITERM_MASK) / max_limit_ma
	 * Note: raw needs to be converted to big-endian format.
	 */

	if (chip->dt.term_current_thresh_hi_ma) {
		raw_hi_thresh = RAW_ITERM(chip->dt.term_current_thresh_hi_ma,
					max_limit_ma);
		raw_hi_thresh = sign_extend32(raw_hi_thresh, 15);
		buf = (u8 *)&raw_hi_thresh;
		raw_hi_thresh = buf[1] | (buf[0] << 8);

		rc = smblite_lib_batch_write(chg, CHGR_ADC_ITERM_UP_THD_MSB_REG,
				(u8 *)&raw_hi_thresh, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ITERM threshold HIGH rc=%d\n",
					rc);
			return rc;
		}
	}

	if (chip->dt.term_current_thresh_lo_ma) {
		raw_lo_thresh = RAW_ITERM(chip->dt.term_current_thresh_lo_ma,
					max_limit_ma);
		raw_lo_thresh = sign_extend32(raw_lo_thresh, 15);
		buf = (u8 *)&raw_lo_thresh;
		raw_lo_thresh = buf[1] | (buf[0] << 8);

		rc = smblite_lib_batch_write(chg, CHGR_ADC_ITERM_LO_THD_MSB_REG,
				(u8 *)&raw_lo_thresh, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ITERM threshold LOW rc=%d\n",
					rc);
			return rc;
		}
	}

	return rc;
}

static int smblite_configure_iterm_thresholds(struct smblite *chip)
{
	int rc = 0;

	switch (chip->dt.term_current_src) {
	case ITERM_SRC_ADC:
		rc = smblite_configure_iterm_thresholds_adc(chip);
		break;
	default:
		break;
	}

	return rc;
}

static int smblite_configure_recharging(struct smblite *chip)
{
	int rc = 0;
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval pval;

	/* Configure VBATT-based or automatic recharging */
	rc = smblite_lib_masked_write(chg, CHGR_RECHG_CFG_REG, RECHG_MASK,
				(chip->dt.auto_recharge_vbat_mv > 0) ?
				VBAT_BASED_RECHG_BIT : 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure VBAT-rechg CHG_CFG2_REG rc=%d\n",
			rc);
		return rc;
	}

	/* program the auto-recharge VBAT threshold */
	if (chip->dt.auto_recharge_vbat_mv  > 0) {
		u32 temp = VBAT_TO_VRAW_ADC(chip->dt.auto_recharge_vbat_mv);

		temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
		rc = smblite_lib_batch_write(chg,
			CHGR_ADC_RECHARGE_THRESHOLD_MSB_REG, (u8 *)&temp, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ADC_RECHARGE_THRESHOLD REG rc=%d\n",
				rc);
			return rc;
		}
		/* Program the sample count for VBAT based recharge to 3 */
		rc = smblite_lib_masked_write(chg, CHGR_RECHG_CFG_REG,
					NO_OF_SAMPLE_FOR_RCHG, 3);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHGR_NO_SAMPLE_FOR_TERM_RCHG_CFG rc=%d\n",
				rc);
			return rc;
		}
	}

	rc = smblite_lib_masked_write(chg, CHGR_RECHG_CFG_REG, RECHG_MASK,
				(chip->dt.auto_recharge_soc != -EINVAL) ?
				SOC_BASED_RECHG_BIT : VBAT_BASED_RECHG_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure SOC-rechg CHG_CFG2_REG rc=%d\n",
			rc);
		return rc;
	}

	/* program the auto-recharge threshold */
	if (chip->dt.auto_recharge_soc != -EINVAL) {
		pval.intval = chip->dt.auto_recharge_soc;
		rc = smblite_lib_set_prop_rechg_soc_thresh(chg, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHG_RCHG_SOC_REG rc=%d\n",
					rc);
			return rc;
		}
	}

	return 0;
}

static int smblite_init_connector_type(struct smb_charger *chg)
{
	int rc, type = 0;
	u8 val = 0;

	rc = smblite_lib_read(chg, TYPEC_U_USB_CFG_REG, &val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't read U_USB config rc=%d\n",
				rc);
		return rc;
	}

	type = !!(val & EN_MICRO_USB_MODE_BIT);

	pr_debug("Connector type=%s\n", type ? "Micro USB" : "TypeC");

	if (type) {
		chg->connector_type = POWER_SUPPLY_CONNECTOR_MICRO_USB;
	} else {
		chg->connector_type = POWER_SUPPLY_CONNECTOR_TYPEC;
		rc = smblite_configure_typec(chg);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't configure TypeC/micro-USB mode rc=%d\n",
				rc);
			return rc;
		}
	}

	return 0;
}

static int smblite_init_otg(struct smblite *chip)
{
	struct smb_charger *chg = &chip->chg;

	chg->usb_id_gpio = chg->usb_id_irq = -EINVAL;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC)
		return 0;

	if (of_find_property(chg->dev->of_node, "qcom,usb-id-gpio", NULL))
		chg->usb_id_gpio = of_get_named_gpio(chg->dev->of_node,
					"qcom,usb-id-gpio", 0);

	chg->usb_id_irq = of_irq_get_byname(chg->dev->of_node,
						"usb_id_irq");
	if (chg->usb_id_irq < 0 || chg->usb_id_gpio < 0)
		pr_err("OTG irq (%d) / gpio (%d) not defined\n",
				chg->usb_id_irq, chg->usb_id_gpio);

	return 0;
}

static int smblite_init_hw(struct smblite *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc;
	u8 val = 0, mask = 0;
	u32 temp;

	if (chip->dt.no_battery)
		chg->fake_capacity = 50;

	if (chip->dt.batt_profile_fcc_ua < 0)
		smblite_lib_get_charge_param(chg, &chg->param.fcc,
				&chg->batt_profile_fcc_ua);

	if (chip->dt.batt_profile_fv_uv < 0)
		smblite_lib_get_charge_param(chg, &chg->param.fv,
				&chg->batt_profile_fv_uv);

	smblite_lib_get_charge_param(chg, &chg->param.aicl_5v_threshold,
				&chg->default_aicl_5v_threshold_mv);
	chg->aicl_5v_threshold_mv = chg->default_aicl_5v_threshold_mv;

	rc = smblite_init_connector_type(chg);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure connector type rc=%d\n",
				rc);
		return rc;
	}

	rc = smblite_init_otg(chip);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't init otg rc=%d\n", rc);
		return rc;
	}

	rc = schgm_flashlite_init(chg);
	if (rc < 0) {
		pr_err("Couldn't configure flash rc=%d\n", rc);
		return rc;
	}

	rc = smblite_lib_icl_override(chg, HW_AUTO_MODE);
	if (rc < 0) {
		pr_err("Couldn't disable ICL override rc=%d\n", rc);
		return rc;
	}

	/* vote 0mA on usb_icl for non battery platforms */
	vote(chg->usb_icl_votable,
		DEFAULT_VOTER, chip->dt.no_battery, 0);
	vote(chg->fcc_votable, HW_LIMIT_VOTER,
		chip->dt.batt_profile_fcc_ua > 0, chip->dt.batt_profile_fcc_ua);
	vote(chg->fv_votable, HW_LIMIT_VOTER,
		chip->dt.batt_profile_fv_uv > 0, chip->dt.batt_profile_fv_uv);
	vote(chg->fcc_votable,
		BATT_PROFILE_VOTER, chg->batt_profile_fcc_ua > 0,
		chg->batt_profile_fcc_ua);
	vote(chg->fv_votable,
		BATT_PROFILE_VOTER, chg->batt_profile_fv_uv > 0,
		chg->batt_profile_fv_uv);

	/* Some h/w limit maximum supported ICL */
	vote(chg->usb_icl_votable, HW_LIMIT_VOTER,
			chip->dt.usb_icl_ua > 0, chip->dt.usb_icl_ua);

	/*
	 * AICL configuration: enable aicl and aicl rerun and based on DT
	 * configuration enable/disable ADB based AICL and Suspend on collapse.
	 */

	mask = USBIN_AICL_PERIODIC_RERUN_EN_BIT | USBIN_AICL_RERUN_TIME_MASK;
	val = USBIN_AICL_PERIODIC_RERUN_EN_BIT | AICL_RERUN_TIME_12S_VAL;
	rc = smblite_lib_masked_write(chg, MISC_AICL_RERUN_CFG_REG, mask, val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't config AICL rerun rc=%d\n", rc);
		return rc;
	}

	mask = USBIN_AICL_EN_BIT | USBIN_AICL_START_AT_MAX;
	val = USBIN_AICL_EN_BIT;
	rc = smblite_lib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG, mask,
				val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't config AICL rc=%d\n", rc);
		return rc;
	}

	if (!chip->dt.disable_suspend_on_collapse) {
		rc = smblite_lib_masked_write(chg, USBIN_INPUT_SUSPEND_REG,
				SUSPEND_ON_COLLAPSE_USBIN_BIT,
				SUSPEND_ON_COLLAPSE_USBIN_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't config AICL rc=%d\n", rc);
			return rc;
		}
	}

	/* enable the charging path */
	rc = vote(chg->chg_disable_votable, DEFAULT_VOTER, false, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable charging rc=%d\n", rc);
		return rc;
	}

	/* configure VBUS for software control */
	rc = smblite_lib_masked_write(chg, DCDC_OTG_CFG_REG,
				OTG_EN_SRC_CFG_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure VBUS for SW control rc=%d\n", rc);
		return rc;
	}

	val = (ilog2(chip->dt.wd_bark_time / 16) << BARK_WDOG_TIMEOUT_SHIFT)
						& BARK_WDOG_TIMEOUT_MASK;
	val |= BITE_WDOG_TIMEOUT_8S << BITE_WDOG_TIMEOUT_SHIFT;

	rc = smblite_lib_masked_write(chg, SNARL_BARK_BITE_WD_CFG_REG,
			BARK_WDOG_TIMEOUT_MASK | BITE_WDOG_TIMEOUT_MASK,
			val);
	if (rc < 0) {
		pr_err("Couldn't configue WD time config rc=%d\n", rc);
		return rc;
	}

	/* enable WD BARK and enable it on plugin */
	mask = WDOG_TIMER_EN_ON_PLUGIN_BIT | BARK_WDOG_INT_EN_BIT
		| BITE_WDOG_DISABLE_CHARGING_CFG_BIT | WDOG_TIMER_EN_BIT;
	val = WDOG_TIMER_EN_ON_PLUGIN_BIT | BARK_WDOG_INT_EN_BIT
		| BITE_WDOG_DISABLE_CHARGING_CFG_BIT;
	rc = smblite_lib_masked_write(chg, WD_CFG_REG, mask, val);
	if (rc < 0) {
		pr_err("Couldn't configue WD config rc=%d\n", rc);
		return rc;
	}

	/* set termination current threshold values */
	rc = smblite_configure_iterm_thresholds(chip);
	if (rc < 0) {
		pr_err("Couldn't configure ITERM thresholds rc=%d\n",
				rc);
		return rc;
	}

	if (chip->dt.chg_inhibit_thr_mv > 0) {
		temp = VBAT_TO_VRAW_ADC(chip->dt.chg_inhibit_thr_mv);
		temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
		rc = smblite_lib_batch_write(chg,
			CHGR_INHIBIT_THRESHOLD_CFG_REG, (u8 *)&temp, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ADC_RECHARGE_THRESHOLD REG rc=%d\n",
				rc);
			return rc;
		}
		rc = smblite_lib_masked_write(chg, CHGR_INHIBIT_REG,
					CHGR_INHIBIT_BIT, CHGR_INHIBIT_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable INHIBIT rc=%d\n",
				rc);
			return rc;
		}
	} else {
		rc = smblite_lib_masked_write(chg, CHGR_INHIBIT_REG,
					CHGR_INHIBIT_BIT, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable INHIBIT rc=%d\n",
				rc);
			return rc;
		}
	}

	mask = FAST_CHARGE_SAFETY_TIMER_EN_BIT | FAST_CHARGE_SAFETY_TIMER_MASK;
	val = FAST_CHARGE_SAFETY_TIMER_EN_BIT
					| FAST_CHARGE_SAFETY_TIMER_768_MIN;
	rc = smblite_lib_masked_write(chg,
			CHGR_FAST_CHARGE_SAFETY_TIMER_CFG_REG, mask, val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set CHGR_FAST_CHARGE_SAFETY_TIMER_CFG_REG rc=%d\n",
			rc);
		return rc;
	}

	rc = smblite_configure_recharging(chip);
	if (rc < 0)
		return rc;

	return rc;
}

static int smblite_post_init(struct smblite *chip)
{
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval pval;
	int rc;

	/*
	 * In case the usb path is suspended, we would have missed disabling
	 * the icl change interrupt because the interrupt could have been
	 * not requested
	 */
	rerun_election(chg->usb_icl_votable);

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC) {
		/* configure power role for dual-role */
		pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		rc = smblite_lib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure DRP role rc=%d\n",
					rc);
			return rc;
		}
	}

	return 0;
}

/****************************
 * DETERMINE INITIAL STATUS *
 ****************************/

static int smblite_determine_initial_status(struct smblite *chip)
{
	struct smb_irq_data irq_data = {chip, "determine-initial-status"};
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval val;
	int rc;

	rc = smblite_lib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		return rc;
	}

	if (chg->bms_psy)
		smblite_lib_suspend_on_debug_battery(chg);

	smblite_usb_plugin_irq_handler(0, &irq_data);
	smblite_typec_attach_detach_irq_handler(0, &irq_data);
	smblite_typec_state_change_irq_handler(0, &irq_data);
	smblite_chg_state_change_irq_handler(0, &irq_data);
	smblite_icl_change_irq_handler(0, &irq_data);
	smblite_batt_temp_changed_irq_handler(0, &irq_data);
	smblite_wdog_bark_irq_handler(0, &irq_data);
	smblite_typec_or_rid_detection_change_irq_handler(0, &irq_data);

	if (chg->usb_id_gpio > 0 &&
		chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		smblite_usb_id_irq_handler(0, chg);

	return 0;
}

/**************************
 * INTERRUPT REGISTRATION *
 **************************/

static struct smb_irq_info smblite_irqs[] = {
	/* CHARGER IRQs */
	[CHGR_ERROR_IRQ] = {
		.name		= "chgr-error",
		.handler	= smblite_default_irq_handler,
	},
	[CHG_STATE_CHANGE_IRQ] = {
		.name		= "chg-state-change",
		.handler	= smblite_chg_state_change_irq_handler,
		.wake		= true,
	},
	[VPH_OV_IRQ] = {
		.name		= "vph-ov",
	},
	[BUCK_OC_IRQ] = {
		.name		= "buck-oc",
	},
	/* DCDC IRQs */
	[OTG_FAIL_IRQ] = {
		.name		= "otg-fail",
		.handler	= smblite_default_irq_handler,
	},
	[OTG_FAULT_IRQ] = {
		.name		= "otg-fault",
	},
	[SKIP_MODE_IRQ] = {
		.name		= "skip-mode",
	},
	[INPUT_CURRENT_LIMITING_IRQ] = {
		.name		= "input-current-limiting",
	},
	[SWITCHER_POWER_OK_IRQ] = {
		.name		= "switcher-power-ok",
		.handler	= smblite_switcher_power_ok_irq_handler,
	},
	/* BATTERY IRQs */
	[BAT_TEMP_IRQ] = {
		.name		= "bat-temp",
		.handler	= smblite_batt_temp_changed_irq_handler,
		.wake		= true,
	},
	[BAT_THERM_OR_ID_MISSING_IRQ] = {
		.name		= "bat-therm-or-id-missing",
		.handler	= smblite_batt_psy_changed_irq_handler,
	},
	[BAT_LOW_IRQ] = {
		.name		= "bat-low",
		.handler	= smblite_batt_psy_changed_irq_handler,
	},
	[BAT_OV_IRQ] = {
		.name		= "bat-ov",
		.handler	= smblite_batt_psy_changed_irq_handler,
	},
	[BSM_ACTIVE_IRQ] = {
		.name		= "bsm-active",
	},
	/* USB INPUT IRQs */
	[USBIN_PLUGIN_IRQ] = {
		.name		= "usbin-plugin",
		.handler	= smblite_usb_plugin_irq_handler,
		.wake           = true,
	},
	[USBIN_COLLAPSE_IRQ] = {
		.name		= "usbin-collapse",
	},
	[USBIN_UV_IRQ] = {
		.name		= "usbin-uv",
		.handler	= smblite_usbin_uv_irq_handler,
		.wake		= true,
		.storm_data	= {true, 3000, 5},
	},
	[USBIN_OV_IRQ] = {
		.name		= "usbin-ov",
		.handler	= smblite_usbin_ov_irq_handler,
	},
	[USBIN_GT_VT_IRQ] = {
		.name		= "usbin-gtvt",
	},
	[USBIN_ICL_CHANGE_IRQ] = {
		.name		= "usbin-icl-change",
		.handler	= smblite_icl_change_irq_handler,
		.wake           = true,
	},
	/* TYPEC IRQs */
	[TYPEC_OR_RID_DETECTION_CHANGE_IRQ] = {
		.name		= "typec-or-rid-detect-change",
		.handler	=
			smblite_typec_or_rid_detection_change_irq_handler,
		.wake           = true,
	},
	[TYPEC_VPD_DETECT_IRQ] = {
		.name		= "typec-vpd-detect",
	},
	[TYPEC_CC_STATE_CHANGE_IRQ] = {
		.name		= "typec-cc-state-change",
		.handler	= smblite_typec_state_change_irq_handler,
		.wake           = true,
	},
	[TYPEC_VBUS_CHANGE_IRQ] = {
		.name		= "typec-vbus-change",
	},
	[TYPEC_ATTACH_DETACH_IRQ] = {
		.name		= "typec-attach-detach",
		.handler	= smblite_typec_attach_detach_irq_handler,
		.wake		= true,
	},
	[TYPEC_LEGACY_CABLE_DETECT_IRQ] = {
		.name		= "typec-legacy-cable-detect",
		.handler	= smblite_default_irq_handler,
	},
	[TYPEC_TRY_SNK_SRC_DETECT_IRQ] = {
		.name		= "typec-try-snk-src-detect",
	},
	/* MISCELLANEOUS IRQs */
	[WDOG_SNARL_IRQ] = {
		.name		= "wdog-snarl",
	},
	[WDOG_BARK_IRQ] = {
		.name		= "wdog-bark",
		.handler	= smblite_wdog_bark_irq_handler,
		.wake		= true,
	},
	[AICL_FAIL_IRQ] = {
		.name		= "aicl-fail",
	},
	[AICL_DONE_IRQ] = {
		.name		= "aicl-done",
	},
	[IMP_TRIGGER_IRQ] = {
		.name		= "imp-trigger",
	},
	[ALL_CHNL_CONV_DONE_IRQ] = {
		.name		= "all-chnl-cond-done",
	},
	/*
	 * triggered when DIE temperature across either of the
	 * _REG_L, _REG_H, _RST, or _SHDN thresholds.
	 */
	[TEMP_CHANGE_IRQ] = {
		.name		= "temp-change",
		.handler	= smblite_temp_change_irq_handler,
		.wake		= true,
	},
	/* FLASH */
	[VREG_OK_IRQ] = {
		.name		= "vreg-ok",
	},
	[ILIM_S2_IRQ] = {
		.name		= "ilim2-s2",
		.handler	= schgm_flashlite_ilim2_irq_handler,
	},
	[ILIM_S1_IRQ] = {
		.name		= "ilim1-s1",
	},
	[FLASH_STATE_CHANGE_IRQ] = {
		.name		= "flash-state-change",
		.handler	= schgm_flashlite_state_change_irq_handler,
	},
	[TORCH_REQ_IRQ] = {
		.name		= "torch-req",
	},
	[FLASH_EN_IRQ] = {
		.name		= "flash-en",
	},
};

static int smblite_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smblite_irqs); i++) {
		if (strcmp(smblite_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int smblite_request_interrupt(struct smblite *chip,
				struct device_node *node, const char *irq_name)
{
	struct smb_charger *chg = &chip->chg;
	int rc, irq, irq_index;
	struct smb_irq_data *irq_data;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		pr_err("Couldn't get irq %s byname\n", irq_name);
		return irq;
	}

	irq_index = smblite_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	if (!smblite_irqs[irq_index].handler)
		return 0;

	irq_data = devm_kzalloc(chg->dev, sizeof(*irq_data), GFP_KERNEL);
	if (!irq_data)
		return -ENOMEM;

	irq_data->parent_data = chip;
	irq_data->name = irq_name;
	irq_data->storm_data = smblite_irqs[irq_index].storm_data;
	mutex_init(&irq_data->storm_data.storm_lock);

	smblite_irqs[irq_index].enabled = true;
	rc = devm_request_threaded_irq(chg->dev, irq, NULL,
					smblite_irqs[irq_index].handler,
					IRQF_ONESHOT, irq_name, irq_data);
	if (rc < 0) {
		pr_err("Couldn't request irq %d\n", irq);
		return rc;
	}

	smblite_irqs[irq_index].irq = irq;
	smblite_irqs[irq_index].irq_data = irq_data;
	if (smblite_irqs[irq_index].wake)
		enable_irq_wake(irq);

	return rc;
}

static int smblite_request_interrupts(struct smblite *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	struct device_node *child;
	int rc = 0;
	const char *name;
	struct property *prop;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
					    prop, name) {
			rc = smblite_request_interrupt(chip, child, name);
			if (rc < 0)
				return rc;
		}
	}

	/* register the USB-id irq */
	if (chg->usb_id_irq > 0 && chg->usb_id_gpio > 0) {
		rc = devm_request_threaded_irq(chg->dev,
				chg->usb_id_irq, NULL,
				smblite_usb_id_irq_handler,
				IRQF_ONESHOT
				| IRQF_TRIGGER_FALLING
				| IRQF_TRIGGER_RISING,
				"smblite_id_irq", chg);
		if (rc < 0) {
			pr_err("Failed to register id-irq rc=%d\n", rc);
			return rc;
		}
		enable_irq_wake(chg->usb_id_irq);
	}

	return rc;
}

static void smblite_disable_interrupts(struct smb_charger *chg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smblite_irqs); i++) {
		if (smblite_irqs[i].irq > 0) {
			if (smblite_irqs[i].wake)
				disable_irq_wake(smblite_irqs[i].irq);
			disable_irq(smblite_irqs[i].irq);
		}
	}

	if (chg->usb_id_irq > 0 && chg->usb_id_gpio > 0) {
		disable_irq_wake(chg->usb_id_irq);
		disable_irq(chg->usb_id_irq);
	}
}

#if defined(CONFIG_DEBUG_FS)

static int force_batt_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->batt_psy);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(force_batt_psy_update_ops, NULL,
			force_batt_psy_update_write, "0x%02llx\n");

static int force_usb_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->usb_psy);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(force_usb_psy_update_ops, NULL,
			force_usb_psy_update_write, "0x%02llx\n");

static void smblite_create_debugfs(struct smblite *chip)
{
	struct dentry *file;

	chip->dfs_root = debugfs_create_dir("charger", NULL);
	if (IS_ERR_OR_NULL(chip->dfs_root)) {
		pr_err("Couldn't create charger debugfs rc=%ld\n",
			(long)chip->dfs_root);
		return;
	}

	file = debugfs_create_file("force_batt_psy_update", 0600,
			    chip->dfs_root, chip, &force_batt_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_batt_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_file("force_usb_psy_update", 0600,
			    chip->dfs_root, chip, &force_usb_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_usb_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_u32("debug_mask", 0600, chip->dfs_root,
			&__debug_mask);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create debug_mask file rc=%ld\n", (long)file);
}

#else

static void smblite_create_debugfs(struct smblite *chip)
{}

#endif

static int smblite_show_charger_status(struct smblite *chip)
{
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval val;
	int usb_present, batt_present, batt_health, batt_charge_type;
	int rc;

	rc = smblite_lib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		return rc;
	}
	usb_present = val.intval;

	rc = smblite_lib_get_prop_batt_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt present rc=%d\n", rc);
		return rc;
	}
	batt_present = val.intval;

	rc = smblite_lib_get_prop_batt_health(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt health rc=%d\n", rc);
		val.intval = POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	batt_health = val.intval;

	rc = smblite_lib_get_prop_batt_charge_type(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt charge type rc=%d\n", rc);
		return rc;
	}
	batt_charge_type = val.intval;

	pr_info("SMBLITE: Mode=%s Conn=%s USB Present=%d Battery present=%d health=%d charge=%d\n",
		chg->ldo_mode ? "LDO" : "SMBC",
		(chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC) ?
			"TYPEC" : "uUSB", usb_present, batt_present,
			batt_health, batt_charge_type);
	return rc;
}

/*********************************
 * TYPEC CLASS REGISTRATION *
 **********************************/

static int smblite_init_typec_class(struct smblite *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	mutex_init(&chg->typec_lock);
	chg->typec_caps.type = TYPEC_PORT_DRP;
	chg->typec_caps.data = TYPEC_PORT_DRD;
	chg->typec_partner_desc.usb_pd = false;
	chg->typec_partner_desc.accessory = TYPEC_ACCESSORY_NONE;
	chg->typec_caps.port_type_set = smblite_lib_typec_port_type_set;
	chg->typec_caps.revision = 0x0130;

	chg->typec_port = typec_register_port(chg->dev, &chg->typec_caps);
	if (IS_ERR(chg->typec_port)) {
		rc = PTR_ERR(chg->typec_port);
		pr_err("Couldn't to register typec_port rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int smblite_probe(struct platform_device *pdev)
{
	struct smblite *chip;
	struct smb_charger *chg;
	int rc = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chg = &chip->chg;
	chg->dev = &pdev->dev;
	chg->debug_mask = &__debug_mask;
	chg->weak_chg_icl_ua = 500000;
	chg->mode = PARALLEL_MASTER;
	chg->irq_info = smblite_irqs;
	chg->otg_present = false;
	chg->step_chg_enabled = true;

	chg->regmap = dev_get_regmap(chg->dev->parent, NULL);
	if (!chg->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}

	rc = smblite_chg_config_init(chip);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't setup chg_config rc=%d\n", rc);
		return rc;
	}

	rc = smblite_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	rc = smblite_lib_init(chg);
	if (rc < 0) {
		pr_err("Smblib_init failed rc=%d\n", rc);
		return rc;
	}

	/* set driver data before resources request it */
	platform_set_drvdata(pdev, chip);

	/* extcon registration */
	chg->extcon = devm_extcon_dev_allocate(chg->dev,
					smblite_lib_extcon_cable);
	if (IS_ERR(chg->extcon)) {
		rc = PTR_ERR(chg->extcon);
		dev_err(chg->dev, "Couldn't to allocate extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	rc = devm_extcon_dev_register(chg->dev, chg->extcon);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't to register extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	/* Support reporting polarity and speed via properties */
	rc = extcon_set_property_capability(chg->extcon,
			EXTCON_USB, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(chg->extcon,
			EXTCON_USB, EXTCON_PROP_USB_SS);
	rc |= extcon_set_property_capability(chg->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(chg->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't to configure extcon capabilities\n");
		goto cleanup;
	}

	rc = smblite_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto cleanup;
	}

	rc = smblite_init_usb_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smblite_init_usb_main_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb main psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smblite_init_batt_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smblite_init_typec_class(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize typec class rc=%d\n", rc);
		goto cleanup;
	}

	rc = smblite_determine_initial_status(chip);
	if (rc < 0) {
		pr_err("Couldn't determine initial status rc=%d\n",
			rc);
		goto cleanup;
	}

	rc = smblite_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		goto cleanup;
	}

	rc = smblite_post_init(chip);
	if (rc < 0) {
		pr_err("Couldn't in post init rc=%d\n", rc);
		goto disable_irq;
	}

	smblite_create_debugfs(chip);

	rc = sysfs_create_groups(&chg->dev->kobj, smblite_groups);
	if (rc < 0) {
		pr_err("Couldn't create sysfs files rc=%d\n", rc);
		goto disable_irq;
	}

	rc = smblite_show_charger_status(chip);
	if (rc < 0) {
		pr_err("Couldn't in getting charger status rc=%d\n", rc);
		goto disable_irq;
	}

	device_init_wakeup(chg->dev, true);

	return rc;

disable_irq:
	smblite_disable_interrupts(chg);
cleanup:
	smblite_lib_deinit(chg);
	platform_set_drvdata(pdev, NULL);

	return rc;
}

static int smblite_remove(struct platform_device *pdev)
{
	struct smblite *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;

	smblite_disable_interrupts(chg);
	smblite_lib_deinit(chg);
	sysfs_remove_groups(&chg->dev->kobj, smblite_groups);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void smblite_shutdown(struct platform_device *pdev)
{
	struct smblite *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;

	/* disable all interrupts */
	smblite_disable_interrupts(chg);

	/* configure power role for UFP */
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC)
		smblite_lib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				TYPEC_POWER_ROLE_CMD_MASK, EN_SNK_ONLY_BIT);
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,qpnp-smblite", },
	{ },
};

static struct platform_driver smblite_driver = {
	.driver		= {
		.name		= "qcom,qpnp-smblite",
		.of_match_table	= match_table,
	},
	.probe		= smblite_probe,
	.remove		= smblite_remove,
	.shutdown	= smblite_shutdown,
};
module_platform_driver(smblite_driver);

MODULE_DESCRIPTION("QPNP SMBLITE Charger Driver");
MODULE_LICENSE("GPL v2");
