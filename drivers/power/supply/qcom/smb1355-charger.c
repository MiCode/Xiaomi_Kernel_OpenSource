/* Copyright (c) 2016-2018 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "SMB1355: %s: " fmt, __func__

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/iio/consumer.h>
#include <linux/platform_device.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/pmic-voter.h>

/* SMB1355 registers, different than mentioned in smb-reg.h */

#define CHGR_BASE	0x1000
#define ANA2_BASE	0x1100
#define BATIF_BASE	0x1200
#define USBIN_BASE	0x1300
#define MISC_BASE	0x1600

#define BATTERY_STATUS_2_REG			(CHGR_BASE + 0x0B)
#define DISABLE_CHARGING_BIT			BIT(3)

#define BATTERY_STATUS_3_REG			(CHGR_BASE + 0x0C)
#define BATT_GT_PRE_TO_FAST_BIT			BIT(4)
#define ENABLE_CHARGING_BIT			BIT(3)

#define CHGR_CFG2_REG				(CHGR_BASE + 0x51)
#define CHG_EN_SRC_BIT				BIT(7)
#define CHG_EN_POLARITY_BIT			BIT(6)

#define CFG_REG					(CHGR_BASE + 0x53)
#define CHG_OPTION_PIN_TRIM_BIT			BIT(7)
#define BATN_SNS_CFG_BIT			BIT(4)
#define CFG_TAPER_DIS_AFVC_BIT			BIT(3)
#define BATFET_SHUTDOWN_CFG_BIT			BIT(2)
#define VDISCHG_EN_CFG_BIT			BIT(1)
#define VCHG_EN_CFG_BIT				BIT(0)

#define FAST_CHARGE_CURRENT_CFG_REG		(CHGR_BASE + 0x61)
#define FAST_CHARGE_CURRENT_SETTING_MASK	GENMASK(7, 0)

#define CHGR_BATTOV_CFG_REG			(CHGR_BASE + 0x70)
#define BATTOV_SETTING_MASK			GENMASK(7, 0)

#define CHGR_PRE_TO_FAST_THRESHOLD_CFG_REG	(CHGR_BASE + 0x74)
#define PRE_TO_FAST_CHARGE_THRESHOLD_MASK	GENMASK(2, 0)

#define ANA2_TR_SBQ_ICL_1X_REF_OFFSET_REG	(ANA2_BASE + 0xF5)
#define TR_SBQ_ICL_1X_REF_OFFSET		GENMASK(4, 0)

#define POWER_MODE_HICCUP_CFG			(BATIF_BASE + 0x72)
#define MAX_HICCUP_DUETO_BATDIS_MASK		GENMASK(5, 2)
#define HICCUP_TIMEOUT_CFG_MASK			GENMASK(1, 0)

#define BATIF_CFG_SMISC_BATID_REG		(BATIF_BASE + 0x73)
#define CFG_SMISC_RBIAS_EXT_CTRL_BIT		BIT(2)

#define SMB2CHGS_BATIF_ENG_SMISC_DIETEMP	(BATIF_BASE + 0xC0)
#define TDIE_COMPARATOR_THRESHOLD		GENMASK(5, 0)

#define BATIF_ENG_SCMISC_SPARE1_REG		(BATIF_BASE + 0xC2)
#define EXT_BIAS_PIN_BIT			BIT(2)
#define DIE_TEMP_COMP_HYST_BIT			BIT(1)

#define TEMP_COMP_STATUS_REG			(MISC_BASE + 0x07)
#define SKIN_TEMP_RST_HOT_BIT			BIT(6)
#define SKIN_TEMP_UB_HOT_BIT			BIT(5)
#define SKIN_TEMP_LB_HOT_BIT			BIT(4)
#define DIE_TEMP_TSD_HOT_BIT			BIT(3)
#define DIE_TEMP_RST_HOT_BIT			BIT(2)
#define DIE_TEMP_UB_HOT_BIT			BIT(1)
#define DIE_TEMP_LB_HOT_BIT			BIT(0)

#define MISC_RT_STS_REG				(MISC_BASE + 0x10)
#define HARD_ILIMIT_RT_STS_BIT			BIT(5)

#define BARK_BITE_WDOG_PET_REG			(MISC_BASE + 0x43)
#define BARK_BITE_WDOG_PET_BIT			BIT(0)

#define WD_CFG_REG				(MISC_BASE + 0x51)
#define WATCHDOG_TRIGGER_AFP_EN_BIT		BIT(7)
#define BARK_WDOG_INT_EN_BIT			BIT(6)
#define BITE_WDOG_INT_EN_BIT			BIT(5)
#define WDOG_IRQ_SFT_BIT			BIT(2)
#define WDOG_TIMER_EN_ON_PLUGIN_BIT		BIT(1)
#define WDOG_TIMER_EN_BIT			BIT(0)

#define MISC_CUST_SDCDC_CLK_CFG_REG		(MISC_BASE + 0xA0)
#define SWITCHER_CLK_FREQ_MASK			GENMASK(3, 0)

#define SNARL_BARK_BITE_WD_CFG_REG		(MISC_BASE + 0x53)
#define BITE_WDOG_DISABLE_CHARGING_CFG_BIT	BIT(7)
#define SNARL_WDOG_TIMEOUT_MASK			GENMASK(6, 4)
#define BARK_WDOG_TIMEOUT_MASK			GENMASK(3, 2)
#define BITE_WDOG_TIMEOUT_MASK			GENMASK(1, 0)

#define MISC_THERMREG_SRC_CFG_REG		(MISC_BASE + 0x70)
#define BYP_THERM_CHG_CURR_ADJUST_BIT		BIT(2)
#define THERMREG_SKIN_CMP_SRC_EN_BIT		BIT(1)
#define THERMREG_DIE_CMP_SRC_EN_BIT		BIT(0)

#define MISC_CHGR_TRIM_OPTIONS_REG		(MISC_BASE + 0x55)
#define CMD_RBIAS_EN_BIT			BIT(2)

#define MISC_ENG_SDCDC_INPUT_CURRENT_CFG1_REG	(MISC_BASE + 0xC8)
#define PROLONG_ISENSE_MASK			GENMASK(7, 6)
#define PROLONG_ISENSEM_SHIFT			6
#define SAMPLE_HOLD_DELAY_MASK			GENMASK(5, 2)
#define SAMPLE_HOLD_DELAY_SHIFT			2
#define DISABLE_ILIMIT_BIT			BIT(0)

#define MISC_ENG_SDCDC_INPUT_CURRENT_CFG2_REG	(MISC_BASE + 0xC9)
#define INPUT_CURRENT_LIMIT_SOURCE_BIT		BIT(7)
#define TC_ISENSE_AMPLIFIER_MASK		GENMASK(6, 4)
#define TC_ISENSE_AMPLIFIER_SHIFT		4
#define HS_II_CORRECTION_MASK			GENMASK(3, 0)

#define MISC_ENG_SDCDC_RESERVE3_REG		(MISC_BASE + 0xCB)
#define VDDCAP_SHORT_DISABLE_TRISTATE_BIT	BIT(7)
#define PCL_SHUTDOWN_BUCK_BIT			BIT(6)
#define ISENSE_TC_CORRECTION_BIT		BIT(5)
#define II_SOURCE_BIT				BIT(4)
#define SCALE_SLOPE_COMP_MASK			GENMASK(3, 0)

#define USBIN_CURRENT_LIMIT_CFG_REG		(USBIN_BASE + 0x70)
#define USB_TR_SCPATH_ICL_1X_GAIN_REG		(USBIN_BASE + 0xF2)
#define TR_SCPATH_ICL_1X_GAIN_MASK		GENMASK(5, 0)

#define IS_USBIN(mode)				\
	((mode == POWER_SUPPLY_PL_USBIN_USBIN) \
	 || (mode == POWER_SUPPLY_PL_USBIN_USBIN_EXT))

struct smb_chg_param {
	const char	*name;
	u16		reg;
	int		min_u;
	int		max_u;
	int		step_u;
};

struct smb_params {
	struct smb_chg_param	fcc;
	struct smb_chg_param	ov;
	struct smb_chg_param	usb_icl;
};

static struct smb_params v1_params = {
	.fcc		= {
		.name	= "fast charge current",
		.reg	= FAST_CHARGE_CURRENT_CFG_REG,
		.min_u	= 0,
		.max_u	= 6000000,
		.step_u	= 25000,
	},
	.ov		= {
		.name	= "battery over voltage",
		.reg	= CHGR_BATTOV_CFG_REG,
		.min_u	= 2450000,
		.max_u	= 5000000,
		.step_u	= 10000,
	},
	.usb_icl	= {
		.name   = "usb input current limit",
		.reg    = USBIN_CURRENT_LIMIT_CFG_REG,
		.min_u  = 100000,
		.max_u  = 5000000,
		.step_u = 30000,
	},
};

struct smb_irq_info {
	const char		*name;
	const irq_handler_t	handler;
	const bool		wake;
	int			irq;
};

struct smb_iio {
	struct iio_channel	*temp_max_chan;
};

struct smb_dt_props {
	bool	disable_ctm;
	int	pl_mode;
	int	pl_batfet_mode;
};

struct smb1355 {
	struct device		*dev;
	char			*name;
	struct regmap		*regmap;

	struct smb_dt_props	dt;
	struct smb_params	param;
	struct smb_iio		iio;

	struct mutex		write_lock;

	struct power_supply	*parallel_psy;
	struct pmic_revid_data	*pmic_rev_id;

	int			c_health;
	int			die_temp_deciDegC;
	bool			exit_die_temp;
	struct delayed_work	die_temp_work;
	bool			disabled;
};

static bool is_secure(struct smb1355 *chip, int addr)
{
	/* assume everything above 0xA0 is secure */
	return (addr & 0xFF) >= 0xA0;
}

static int smb1355_read(struct smb1355 *chip, u16 addr, u8 *val)
{
	unsigned int temp;
	int rc;

	rc = regmap_read(chip->regmap, addr, &temp);
	if (rc >= 0)
		*val = (u8)temp;

	return rc;
}

static int smb1355_masked_write(struct smb1355 *chip, u16 addr, u8 mask, u8 val)
{
	int rc;

	mutex_lock(&chip->write_lock);
	if (is_secure(chip, addr)) {
		rc = regmap_write(chip->regmap, (addr & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0)
			goto unlock;
	}

	rc = regmap_update_bits(chip->regmap, addr, mask, val);

unlock:
	mutex_unlock(&chip->write_lock);
	return rc;
}

static int smb1355_write(struct smb1355 *chip, u16 addr, u8 val)
{
	int rc;

	mutex_lock(&chip->write_lock);

	if (is_secure(chip, addr)) {
		rc = regmap_write(chip->regmap, (addr & ~(0xFF)) | 0xD0, 0xA5);
		if (rc < 0)
			goto unlock;
	}

	rc = regmap_write(chip->regmap, addr, val);

unlock:
	mutex_unlock(&chip->write_lock);
	return rc;
}

static int smb1355_set_charge_param(struct smb1355 *chip,
			struct smb_chg_param *param, int val_u)
{
	int rc;
	u8 val_raw;

	if (val_u > param->max_u || val_u < param->min_u) {
		pr_err("%s: %d is out of range [%d, %d]\n",
			param->name, val_u, param->min_u, param->max_u);
		return -EINVAL;
	}

	val_raw = (val_u - param->min_u) / param->step_u;

	rc = smb1355_write(chip, param->reg, val_raw);
	if (rc < 0) {
		pr_err("%s: Couldn't write 0x%02x to 0x%04x rc=%d\n",
			param->name, val_raw, param->reg, rc);
		return rc;
	}

	return rc;
}

static int smb1355_get_charge_param(struct smb1355 *chip,
			struct smb_chg_param *param, int *val_u)
{
	int rc;
	u8 val_raw;

	rc = smb1355_read(chip, param->reg, &val_raw);
	if (rc < 0) {
		pr_err("%s: Couldn't read from 0x%04x rc=%d\n",
			param->name, param->reg, rc);
		return rc;
	}

	*val_u = val_raw * param->step_u + param->min_u;

	return rc;
}

#define UB_COMP_OFFSET_DEGC		34
#define DIE_TEMP_MEAS_PERIOD_MS		10000
static void die_temp_work(struct work_struct *work)
{
	struct smb1355 *chip = container_of(work, struct smb1355,
							die_temp_work.work);
	int rc, i;
	u8 temp_stat;

	for (i = 0; i < BIT(5); i++) {
		rc = smb1355_masked_write(chip,
				SMB2CHGS_BATIF_ENG_SMISC_DIETEMP,
				TDIE_COMPARATOR_THRESHOLD, i);
		if (rc < 0) {
			pr_err("Couldn't set temp comp threshold rc=%d\n", rc);
			continue;
		}

		if (chip->exit_die_temp)
			return;

		/* wait for the comparator output to deglitch */
		msleep(100);

		rc = smb1355_read(chip, TEMP_COMP_STATUS_REG, &temp_stat);
		if (rc < 0) {
			pr_err("Couldn't read temp comp status rc=%d\n", rc);
			continue;
		}

		if (!(temp_stat & DIE_TEMP_UB_HOT_BIT)) {
			/* found the temp */
			break;
		}
	}

	chip->die_temp_deciDegC = 10 * (i + UB_COMP_OFFSET_DEGC);

	schedule_delayed_work(&chip->die_temp_work,
			msecs_to_jiffies(DIE_TEMP_MEAS_PERIOD_MS));
}

static int smb1355_get_prop_input_current_limited(struct smb1355 *chip,
					union power_supply_propval *pval)
{
	int rc;
	u8 stat = 0;

	rc = smb1355_read(chip, MISC_RT_STS_REG, &stat);
	if (rc < 0)
		pr_err("Couldn't read SMB1355_BATTERY_STATUS_3 rc=%d\n", rc);

	pval->intval = !!(stat & HARD_ILIMIT_RT_STS_BIT);

	return 0;
}

static irqreturn_t smb1355_handle_chg_state_change(int irq, void *data)
{
	struct smb1355 *chip = data;

	if (chip->parallel_psy)
		power_supply_changed(chip->parallel_psy);

	return IRQ_HANDLED;
}

static irqreturn_t smb1355_handle_wdog_bark(int irq, void *data)
{
	struct smb1355 *chip = data;
	int rc;

	rc = smb1355_write(chip, BARK_BITE_WDOG_PET_REG,
					BARK_BITE_WDOG_PET_BIT);
	if (rc < 0)
		pr_err("Couldn't pet the dog rc=%d\n", rc);

	return IRQ_HANDLED;
}

static irqreturn_t smb1355_handle_temperature_change(int irq, void *data)
{
	struct smb1355 *chip = data;

	if (chip->parallel_psy)
		power_supply_changed(chip->parallel_psy);

	return IRQ_HANDLED;
}

static int smb1355_determine_initial_status(struct smb1355 *chip)
{
	smb1355_handle_temperature_change(0, chip);
	return 0;
}

static int smb1355_parse_dt(struct smb1355 *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	chip->dt.disable_ctm =
		of_property_read_bool(node, "qcom,disable-ctm");

	/*
	 * If parallel-mode property is not present default
	 * parallel configuration is USBMID-USBMID.
	 */
	rc = of_property_read_u32(node,
		"qcom,parallel-mode", &chip->dt.pl_mode);
	if (rc < 0)
		chip->dt.pl_mode = POWER_SUPPLY_PL_USBMID_USBMID;

	/*
	 * If stacked-batfet property is not present default
	 * configuration is NON-STACKED-BATFET.
	 */
	chip->dt.pl_batfet_mode = POWER_SUPPLY_PL_NON_STACKED_BATFET;
	if (of_property_read_bool(node, "qcom,stacked-batfet"))
		chip->dt.pl_batfet_mode = POWER_SUPPLY_PL_STACKED_BATFET;

	return 0;
}

/*****************************
 * PARALLEL PSY REGISTRATION *
 *****************************/

static enum power_supply_property smb1355_parallel_props[] = {
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_CHARGER_TEMP,
	POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
	POWER_SUPPLY_PROP_CONNECTOR_HEALTH,
	POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
	POWER_SUPPLY_PROP_MIN_ICL,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static int smb1355_get_prop_batt_charge_type(struct smb1355 *chip,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smb1355_read(chip, BATTERY_STATUS_3_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read SMB1355_BATTERY_STATUS_3 rc=%d\n", rc);
		return rc;
	}

	if (stat & ENABLE_CHARGING_BIT) {
		if (stat & BATT_GT_PRE_TO_FAST_BIT)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	} else {
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return rc;
}

static int smb1355_get_prop_connector_health(struct smb1355 *chip)
{
	u8 temp;
	int rc;

	rc = smb1355_read(chip, TEMP_COMP_STATUS_REG, &temp);
	if (rc < 0) {
		pr_err("Couldn't read comp stat reg rc = %d\n", rc);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	if (temp & SKIN_TEMP_RST_HOT_BIT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	if (temp & SKIN_TEMP_UB_HOT_BIT)
		return POWER_SUPPLY_HEALTH_HOT;

	if (temp & SKIN_TEMP_LB_HOT_BIT)
		return POWER_SUPPLY_HEALTH_WARM;

	return POWER_SUPPLY_HEALTH_COOL;
}


static int smb1355_get_prop_charger_temp_max(struct smb1355 *chip,
				union power_supply_propval *val)
{
	int rc;

	if (!chip->iio.temp_max_chan ||
		PTR_ERR(chip->iio.temp_max_chan) == -EPROBE_DEFER)
		chip->iio.temp_max_chan = devm_iio_channel_get(chip->dev,
							"charger_temp_max");
	if (IS_ERR(chip->iio.temp_max_chan))
		return PTR_ERR(chip->iio.temp_max_chan);

	rc = iio_read_channel_processed(chip->iio.temp_max_chan, &val->intval);
	val->intval /= 100;
	return rc;
}

#define MIN_PARALLEL_ICL_UA		250000
static int smb1355_parallel_get_prop(struct power_supply *psy,
				     enum power_supply_property prop,
				     union power_supply_propval *val)
{
	struct smb1355 *chip = power_supply_get_drvdata(psy);
	u8 stat;
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = smb1355_get_prop_batt_charge_type(chip, val);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		rc = smb1355_read(chip, BATTERY_STATUS_3_REG, &stat);
		if (rc >= 0)
			val->intval = (bool)(stat & ENABLE_CHARGING_BIT);
		break;
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		rc = smb1355_read(chip, BATTERY_STATUS_2_REG, &stat);
		if (rc >= 0)
			val->intval = !(stat & DISABLE_CHARGING_BIT);
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP:
		val->intval = chip->die_temp_deciDegC;
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		rc = smb1355_get_prop_charger_temp_max(chip, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		val->intval = chip->disabled;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smb1355_get_charge_param(chip, &chip->param.ov,
						&val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smb1355_get_charge_param(chip, &chip->param.fcc,
						&val->intval);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = chip->name;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		val->intval = chip->dt.pl_mode;
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		if (chip->c_health == -EINVAL)
			val->intval = smb1355_get_prop_connector_health(chip);
		else
			val->intval = chip->c_health;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE:
		val->intval = chip->dt.pl_batfet_mode;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		if (IS_USBIN(chip->dt.pl_mode))
			rc = smb1355_get_prop_input_current_limited(chip, val);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (IS_USBIN(chip->dt.pl_mode))
			rc = smb1355_get_charge_param(chip,
					&chip->param.usb_icl, &val->intval);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_MIN_ICL:
		val->intval = MIN_PARALLEL_ICL_UA;
		break;
	default:
		pr_err_ratelimited("parallel psy get prop %d not supported\n",
			prop);
		return -EINVAL;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", prop, rc);
		return -ENODATA;
	}

	return rc;
}

static int smb1355_set_parallel_charging(struct smb1355 *chip, bool disable)
{
	int rc;

	if (chip->disabled == disable)
		return 0;

	rc = smb1355_masked_write(chip, WD_CFG_REG, WDOG_TIMER_EN_BIT,
				 disable ? 0 : WDOG_TIMER_EN_BIT);
	if (rc < 0) {
		pr_err("Couldn't %s watchdog rc=%d\n",
		       disable ? "disable" : "enable", rc);
		disable = true;
	}

	/*
	 * Configure charge enable for high polarity and
	 * When disabling charging set it to cmd register control(cmd bit=0)
	 * When enabling charging set it to pin control
	 */
	rc = smb1355_masked_write(chip, CHGR_CFG2_REG,
			CHG_EN_POLARITY_BIT | CHG_EN_SRC_BIT,
			disable ? 0 : CHG_EN_SRC_BIT);
	if (rc < 0) {
		pr_err("Couldn't configure charge enable source rc=%d\n", rc);
		disable = true;
	}

	chip->die_temp_deciDegC = -EINVAL;
	if (disable) {
		chip->exit_die_temp = true;
		cancel_delayed_work_sync(&chip->die_temp_work);
	} else {
		/* start the work to measure temperature */
		chip->exit_die_temp = false;
		schedule_delayed_work(&chip->die_temp_work, 0);
	}

	chip->disabled = disable;

	return 0;
}

static int smb1355_set_current_max(struct smb1355 *chip, int curr)
{
	int rc = 0;

	if (!IS_USBIN(chip->dt.pl_mode))
		return 0;

	if ((curr / 1000) < 100) {
		/* disable parallel path (ICL < 100mA) */
		rc = smb1355_set_parallel_charging(chip, true);
	} else {
		rc = smb1355_set_parallel_charging(chip, false);
		if (rc < 0)
			return rc;

		rc = smb1355_set_charge_param(chip,
				&chip->param.usb_icl, curr);
	}

	return rc;
}

static int smb1355_parallel_set_prop(struct power_supply *psy,
				     enum power_supply_property prop,
				     const union power_supply_propval *val)
{
	struct smb1355 *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smb1355_set_parallel_charging(chip, (bool)val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smb1355_set_current_max(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smb1355_set_charge_param(chip, &chip->param.ov,
						val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smb1355_set_charge_param(chip, &chip->param.fcc,
						val->intval);
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		chip->c_health = val->intval;
		power_supply_changed(chip->parallel_psy);
		break;
	default:
		pr_debug("parallel power supply set prop %d not supported\n",
			prop);
		return -EINVAL;
	}

	return rc;
}

static int smb1355_parallel_prop_is_writeable(struct power_supply *psy,
					      enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		return 1;
	default:
		break;
	}

	return 0;
}

static struct power_supply_desc parallel_psy_desc = {
	.name			= "parallel",
	.type			= POWER_SUPPLY_TYPE_PARALLEL,
	.properties		= smb1355_parallel_props,
	.num_properties		= ARRAY_SIZE(smb1355_parallel_props),
	.get_property		= smb1355_parallel_get_prop,
	.set_property		= smb1355_parallel_set_prop,
	.property_is_writeable	= smb1355_parallel_prop_is_writeable,
};

static int smb1355_init_parallel_psy(struct smb1355 *chip)
{
	struct power_supply_config parallel_cfg = {};

	parallel_cfg.drv_data = chip;
	parallel_cfg.of_node = chip->dev->of_node;

	/* change to smb1355's property list */
	parallel_psy_desc.properties = smb1355_parallel_props;
	parallel_psy_desc.num_properties = ARRAY_SIZE(smb1355_parallel_props);
	chip->parallel_psy = devm_power_supply_register(chip->dev,
						   &parallel_psy_desc,
						   &parallel_cfg);
	if (IS_ERR(chip->parallel_psy)) {
		pr_err("Couldn't register parallel power supply\n");
		return PTR_ERR(chip->parallel_psy);
	}

	return 0;
}

/***************************
 * HARDWARE INITIALIZATION *
 ***************************/

static int smb1355_tskin_sensor_config(struct smb1355 *chip)
{
	int rc;

	if (chip->dt.disable_ctm) {
		/*
		 * the TSKIN sensor with external resistor needs a bias,
		 * disable it here.
		 */
		rc = smb1355_masked_write(chip, BATIF_ENG_SCMISC_SPARE1_REG,
					 EXT_BIAS_PIN_BIT, 0);
		if (rc < 0) {
			pr_err("Couldn't enable ext bias pin path rc=%d\n",
				rc);
			return rc;
		}

		rc = smb1355_masked_write(chip, BATIF_CFG_SMISC_BATID_REG,
					CFG_SMISC_RBIAS_EXT_CTRL_BIT, 0);
		if (rc < 0) {
			pr_err("Couldn't set  BATIF_CFG_SMISC_BATID rc=%d\n",
				rc);
			return rc;
		}

		rc = smb1355_masked_write(chip, MISC_CHGR_TRIM_OPTIONS_REG,
					CMD_RBIAS_EN_BIT, 0);
		if (rc < 0) {
			pr_err("Couldn't set MISC_CHGR_TRIM_OPTIONS rc=%d\n",
				rc);
			return rc;
		}

		/* disable skin temperature comparator source */
		rc = smb1355_masked_write(chip, MISC_THERMREG_SRC_CFG_REG,
			THERMREG_SKIN_CMP_SRC_EN_BIT, 0);
		if (rc < 0) {
			pr_err("Couldn't set Skin temp comparator src rc=%d\n",
				rc);
			return rc;
		}
	} else {
		/*
		 * the TSKIN sensor with external resistor needs a bias,
		 * enable it here.
		 */
		rc = smb1355_masked_write(chip, BATIF_ENG_SCMISC_SPARE1_REG,
					 EXT_BIAS_PIN_BIT, EXT_BIAS_PIN_BIT);
		if (rc < 0) {
			pr_err("Couldn't enable ext bias pin path rc=%d\n",
				rc);
			return rc;
		}

		rc = smb1355_masked_write(chip, BATIF_CFG_SMISC_BATID_REG,
					CFG_SMISC_RBIAS_EXT_CTRL_BIT,
					CFG_SMISC_RBIAS_EXT_CTRL_BIT);
		if (rc < 0) {
			pr_err("Couldn't set  BATIF_CFG_SMISC_BATID rc=%d\n",
				rc);
			return rc;
		}

		rc = smb1355_masked_write(chip, MISC_CHGR_TRIM_OPTIONS_REG,
					CMD_RBIAS_EN_BIT,
					CMD_RBIAS_EN_BIT);
		if (rc < 0) {
			pr_err("Couldn't set MISC_CHGR_TRIM_OPTIONS rc=%d\n",
				rc);
			return rc;
		}

		/* Enable skin temperature comparator source */
		rc = smb1355_masked_write(chip, MISC_THERMREG_SRC_CFG_REG,
			THERMREG_SKIN_CMP_SRC_EN_BIT,
			THERMREG_SKIN_CMP_SRC_EN_BIT);
		if (rc < 0) {
			pr_err("Couldn't set Skin temp comparator src rc=%d\n",
				rc);
			return rc;
		}
	}

	return rc;
}

static int smb1355_init_hw(struct smb1355 *chip)
{
	int rc;

	/* enable watchdog bark and bite interrupts, and disable the watchdog */
	rc = smb1355_masked_write(chip, WD_CFG_REG, WDOG_TIMER_EN_BIT
			| WDOG_TIMER_EN_ON_PLUGIN_BIT | BITE_WDOG_INT_EN_BIT
			| BARK_WDOG_INT_EN_BIT,
			BITE_WDOG_INT_EN_BIT | BARK_WDOG_INT_EN_BIT);
	if (rc < 0) {
		pr_err("Couldn't configure the watchdog rc=%d\n", rc);
		return rc;
	}

	/* disable charging when watchdog bites */
	rc = smb1355_masked_write(chip, SNARL_BARK_BITE_WD_CFG_REG,
				 BITE_WDOG_DISABLE_CHARGING_CFG_BIT,
				 BITE_WDOG_DISABLE_CHARGING_CFG_BIT);
	if (rc < 0) {
		pr_err("Couldn't configure the watchdog bite rc=%d\n", rc);
		return rc;
	}

	/* disable parallel charging path */
	rc = smb1355_set_parallel_charging(chip, true);
	if (rc < 0) {
		pr_err("Couldn't disable parallel path rc=%d\n", rc);
		return rc;
	}

	/* initialize FCC to 0 */
	rc = smb1355_set_charge_param(chip, &chip->param.fcc, 0);
	if (rc < 0) {
		pr_err("Couldn't set 0 FCC rc=%d\n", rc);
		return rc;
	}

	/* HICCUP setting, unlimited retry with 250ms interval */
	rc = smb1355_masked_write(chip, POWER_MODE_HICCUP_CFG,
			HICCUP_TIMEOUT_CFG_MASK | MAX_HICCUP_DUETO_BATDIS_MASK,
			0);
	if (rc < 0) {
		pr_err("Couldn't set HICCUP interval rc=%d\n",
			rc);
		return rc;
	}

	/* enable parallel current sensing */
	rc = smb1355_masked_write(chip, CFG_REG,
				 VCHG_EN_CFG_BIT, VCHG_EN_CFG_BIT);
	if (rc < 0) {
		pr_err("Couldn't enable parallel current sensing rc=%d\n",
			rc);
		return rc;
	}

	/* set Pre-to-Fast Charging Threshold 2.6V */
	rc = smb1355_masked_write(chip, CHGR_PRE_TO_FAST_THRESHOLD_CFG_REG,
				 PRE_TO_FAST_CHARGE_THRESHOLD_MASK, 0);
	if (rc < 0) {
		pr_err("Couldn't set PRE_TO_FAST_CHARGE_THRESHOLD rc=%d\n",
			rc);
		return rc;
	}

	/*
	 * Enable thermal Die temperature comparator source and disable hw
	 * mitigation for skin/die
	 */
	rc = smb1355_masked_write(chip, MISC_THERMREG_SRC_CFG_REG,
		THERMREG_DIE_CMP_SRC_EN_BIT | BYP_THERM_CHG_CURR_ADJUST_BIT,
		THERMREG_DIE_CMP_SRC_EN_BIT | BYP_THERM_CHG_CURR_ADJUST_BIT);
	if (rc < 0) {
		pr_err("Couldn't set Skin temperature comparator src rc=%d\n",
			rc);
		return rc;
	}

	/*
	 * Disable hysterisis for die temperature. This is so that sw can run
	 * stepping scheme quickly
	 */
	rc = smb1355_masked_write(chip, BATIF_ENG_SCMISC_SPARE1_REG,
				DIE_TEMP_COMP_HYST_BIT, 0);
	if (rc < 0) {
		pr_err("Couldn't disable hyst. for die rc=%d\n", rc);
		return rc;
	}

	rc = smb1355_tskin_sensor_config(chip);
	if (rc < 0) {
		pr_err("Couldn't configure tskin regs rc=%d\n", rc);
		return rc;
	}

	/* USBIN-USBIN configuration */
	if (IS_USBIN(chip->dt.pl_mode)) {
		/* set swicther clock frequency to 700kHz */
		rc = smb1355_masked_write(chip, MISC_CUST_SDCDC_CLK_CFG_REG,
				SWITCHER_CLK_FREQ_MASK, 0x03);
		if (rc < 0) {
			pr_err("Couldn't set MISC_CUST_SDCDC_CLK_CFG rc=%d\n",
				rc);
			return rc;
		}

		/*
		 * configure compensation for input current limit (ICL) loop
		 * accuracy, scale slope compensation using 30k resistor.
		 */
		rc = smb1355_masked_write(chip, MISC_ENG_SDCDC_RESERVE3_REG,
				II_SOURCE_BIT | SCALE_SLOPE_COMP_MASK,
				II_SOURCE_BIT);
		if (rc < 0) {
			pr_err("Couldn't set MISC_ENG_SDCDC_RESERVE3_REG rc=%d\n",
				rc);
			return rc;
		}

		/* configuration to improve ICL accuracy */
		rc = smb1355_masked_write(chip,
				MISC_ENG_SDCDC_INPUT_CURRENT_CFG1_REG,
				PROLONG_ISENSE_MASK | SAMPLE_HOLD_DELAY_MASK,
				((uint8_t)0x0C << SAMPLE_HOLD_DELAY_SHIFT));
		if (rc < 0) {
			pr_err("Couldn't set MISC_ENG_SDCDC_INPUT_CURRENT_CFG1_REG rc=%d\n",
				rc);
			return rc;
		}

		rc = smb1355_masked_write(chip,
				MISC_ENG_SDCDC_INPUT_CURRENT_CFG2_REG,
				INPUT_CURRENT_LIMIT_SOURCE_BIT
				| HS_II_CORRECTION_MASK,
			       INPUT_CURRENT_LIMIT_SOURCE_BIT | 0xC);

		if (rc < 0) {
			pr_err("Couldn't set MISC_ENG_SDCDC_INPUT_CURRENT_CFG2_REG rc=%d\n",
				rc);
			return rc;
		}

		/* configure DAC offset */
		rc = smb1355_masked_write(chip,
				ANA2_TR_SBQ_ICL_1X_REF_OFFSET_REG,
				TR_SBQ_ICL_1X_REF_OFFSET, 0x00);
		if (rc < 0) {
			pr_err("Couldn't set ANA2_TR_SBQ_ICL_1X_REF_OFFSET_REG rc=%d\n",
				rc);
			return rc;
		}

		/* configure DAC gain */
		rc = smb1355_masked_write(chip, USB_TR_SCPATH_ICL_1X_GAIN_REG,
				TR_SCPATH_ICL_1X_GAIN_MASK, 0x22);
		if (rc < 0) {
			pr_err("Couldn't set USB_TR_SCPATH_ICL_1X_GAIN_REG rc=%d\n",
				rc);
			return rc;
		}
	}

	return 0;
}

/**************************
 * INTERRUPT REGISTRATION *
 **************************/
static struct smb_irq_info smb1355_irqs[] = {
	[0] = {
		.name		= "wdog-bark",
		.handler	= smb1355_handle_wdog_bark,
		.wake		= true,
	},
	[1] = {
		.name		= "chg-state-change",
		.handler	= smb1355_handle_chg_state_change,
		.wake		= true,
	},
	[2] = {
		.name		= "temperature-change",
		.handler	= smb1355_handle_temperature_change,
	},
};

static int smb1355_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb1355_irqs); i++) {
		if (strcmp(smb1355_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int smb1355_request_interrupt(struct smb1355 *chip,
				struct device_node *node,
				const char *irq_name)
{
	int rc = 0, irq, irq_index;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		pr_err("Couldn't get irq %s byname\n", irq_name);
		return irq;
	}

	irq_index = smb1355_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	if (!smb1355_irqs[irq_index].handler)
		return 0;

	rc = devm_request_threaded_irq(chip->dev, irq, NULL,
				smb1355_irqs[irq_index].handler,
				IRQF_ONESHOT, irq_name, chip);
	if (rc < 0) {
		pr_err("Couldn't request irq %d rc=%d\n", irq, rc);
		return rc;
	}

	if (smb1355_irqs[irq_index].wake)
		enable_irq_wake(irq);

	return rc;
}

static int smb1355_request_interrupts(struct smb1355 *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *child;
	int rc = 0;
	const char *name;
	struct property *prop;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
					prop, name) {
			rc = smb1355_request_interrupt(chip, child, name);
			if (rc < 0) {
				pr_err("Couldn't request interrupt %s rc=%d\n",
					name, rc);
				return rc;
			}
		}
	}

	return rc;
}

/*********
 * PROBE *
 *********/
static const struct of_device_id match_table[] = {
	{
		.compatible	= "qcom,smb1355",
	},
	{ },
};

static int smb1355_probe(struct platform_device *pdev)
{
	struct smb1355 *chip;
	const struct of_device_id *id;
	int rc = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->param = v1_params;
	chip->c_health = -EINVAL;
	chip->name = "smb1355";
	mutex_init(&chip->write_lock);
	INIT_DELAYED_WORK(&chip->die_temp_work, die_temp_work);
	chip->disabled = true;
	chip->die_temp_deciDegC = -EINVAL;

	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}

	id = of_match_device(of_match_ptr(match_table), chip->dev);
	if (!id) {
		pr_err("Couldn't find a matching device\n");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, chip);

	rc = smb1355_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb1355_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb1355_init_parallel_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize parallel psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb1355_determine_initial_status(chip);
	if (rc < 0) {
		pr_err("Couldn't determine initial status rc=%d\n",
			rc);
		goto cleanup;
	}

	rc = smb1355_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		goto cleanup;
	}

	pr_info("%s probed successfully pl_mode=%s batfet_mode=%s\n",
		chip->name,
		IS_USBIN(chip->dt.pl_mode) ? "USBIN-USBIN" : "USBMID-USBMID",
		(chip->dt.pl_batfet_mode == POWER_SUPPLY_PL_STACKED_BATFET)
			? "STACKED_BATFET" : "NON-STACKED_BATFET");
	return rc;

cleanup:
	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int smb1355_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void smb1355_shutdown(struct platform_device *pdev)
{
	struct smb1355 *chip = platform_get_drvdata(pdev);
	int rc;

	/* disable parallel charging path */
	rc = smb1355_set_parallel_charging(chip, true);
	if (rc < 0)
		pr_err("Couldn't disable parallel path rc=%d\n", rc);
}

static struct platform_driver smb1355_driver = {
	.driver	= {
		.name		= "qcom,smb1355-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= smb1355_probe,
	.remove		= smb1355_remove,
	.shutdown	= smb1355_shutdown,
};
module_platform_driver(smb1355_driver);

MODULE_DESCRIPTION("QPNP SMB1355 Charger Driver");
MODULE_LICENSE("GPL v2");
