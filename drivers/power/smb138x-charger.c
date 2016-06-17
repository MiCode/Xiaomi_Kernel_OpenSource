/* Copyright (c) 2016 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "SMB138X: %s: " fmt, __func__

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include "pmic-voter.h"
#include "smb138x-charger.h"

#define SMB_DEFAULT_FCC_UA 1000000
#define SMB_DEFAULT_ICL_UA 1500000

#define smb_dbg(chg, reason, fmt, ...)			\
	do {							\
		if (*chg->debug_mask & (reason))		\
			pr_info(fmt, ##__VA_ARGS__);	\
		else						\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

enum smb_print_reason {
	PR_INTERRUPT	= BIT(0),
	PR_REGISTER	= BIT(1),
	PR_MISC		= BIT(2),
};

enum smb_voters {
	DEFAULT_VOTER = 0,
	USER_VOTER,
	USB_PSY_VOTER,
	NUM_VOTERS,
};

enum smb_mode {
	PARALLEL_MASTER = 0,
	PARALLEL_SLAVE,
	NUM_MODES,
};

struct smb_regulator {
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
};

struct smb_irq_data {
	void		*parent_data;
	const char	*name;
};

struct smb_chg_param {
	const char	*name;
	u16		reg;
	int		min_u;
	int		max_u;
	int		step_u;
};

struct smb_params {
	struct smb_chg_param fcc;
	struct smb_chg_param fv;
	struct smb_chg_param usb_icl;
	struct smb_chg_param dc_icl;
};

struct smb_charger {
	struct device		*dev;
	struct regmap		*regmap;
	struct smb_params	param;
	int			*debug_mask;
	enum smb_mode		mode;

	/* locks */
	struct mutex		write_lock;

	/* power supplies */
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;

	/* regulators */
	struct smb_regulator	*vbus_vreg;

	/* votables */
	struct votable		*usb_suspend_votable;
	struct votable		*dc_suspend_votable;
	struct votable		*fcc_votable;
	struct votable		*usb_icl_votable;
	struct votable		*dc_icl_votable;
};

static struct smb_params v1_params = {
	.fcc		= {
		.name	= "fast charge current",
		.reg	= FAST_CHARGE_CURRENT_CFG_REG,
		.min_u	= 0,
		.max_u	= 4500000,
		.step_u	= 25000,
	},
	.fv		= {
		.name	= "float voltage",
		.reg	= FLOAT_VOLTAGE_CFG_REG,
		.min_u	= 2500000,
		.max_u	= 5000000,
		.step_u	= 10000,
	},
	.usb_icl	= {
		.name	= "usb input current limit",
		.reg	= USBIN_CURRENT_LIMIT_CFG_REG,
		.min_u	= 0,
		.max_u	= 6000000,
		.step_u	= 25000,
	},
	.dc_icl		= {
		.name	= "dc input current limit",
		.reg	= DCIN_CURRENT_LIMIT_CFG_REG,
		.min_u	= 0,
		.max_u	= 6000000,
		.step_u	= 25000,
	},
};

struct smb_dt_props {
	bool	suspend_input;
	int	fcc_ua;
	int	usb_icl_ua;
	int	dc_icl_ua;
	bool	micro_usb;
};

struct smb138x {
	struct smb_charger	chg;
	struct smb_dt_props	dt;
	struct notifier_block	nb;
	struct delayed_work	usb_init_work;
};

static int smb138x_debug_mask;
module_param_named(
	debug_mask, smb138x_debug_mask, int, S_IRUSR | S_IWUSR
);

static bool is_secure(struct smb_charger *chg, int addr)
{
	/* assume everything above 0xC0 is secure */
	return (bool)(addr >= 0xC0);
}

static int smb138x_read(struct smb_charger *chg, u16 addr, u8 *val)
{
	unsigned int temp;
	int rc = 0;

	rc = regmap_read(chg->regmap, addr, &temp);
	if (rc >= 0)
		*val = (u8)temp;

	return rc;
}

static int smb138x_masked_write(struct smb_charger *chg,
				u16 addr, u8 mask, u8 val)
{
	int rc = 0;

	mutex_lock(&chg->write_lock);
	if (is_secure(chg, addr)) {
		rc = regmap_write(chg->regmap, (addr & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0)
			goto unlock;
	}

	rc = regmap_update_bits(chg->regmap, addr, mask, val);

unlock:
	mutex_unlock(&chg->write_lock);
	return rc;
}

static int smb138x_write(struct smb_charger *chg, u16 addr, u8 val)
{
	int rc = 0;

	mutex_lock(&chg->write_lock);
	if (is_secure(chg, addr)) {
		rc = regmap_write(chg->regmap, (addr & ~(0xFF)) | 0xD0, 0xA5);
		if (rc < 0)
			goto unlock;
	}

	rc = regmap_write(chg->regmap, addr, val);

unlock:
	mutex_unlock(&chg->write_lock);
	return rc;
}

struct apsd_result {
	const char * const name;
	const u8 bit;
	const enum power_supply_type pst;
};

static const struct apsd_result const smb138x_apsd_results[] = {
	{"UNKNOWN", 0, POWER_SUPPLY_TYPE_UNKNOWN},
	{"SDP", SDP_CHARGER_BIT, POWER_SUPPLY_TYPE_USB},
	{"CDP", CDP_CHARGER_BIT, POWER_SUPPLY_TYPE_USB_CDP},
	{"DCP", DCP_CHARGER_BIT, POWER_SUPPLY_TYPE_USB_DCP},
	{"OCP", OCP_CHARGER_BIT, POWER_SUPPLY_TYPE_USB_DCP},
	{"FLOAT", FLOAT_CHARGER_BIT, POWER_SUPPLY_TYPE_USB_DCP},
	{"HVDCP2", DCP_CHARGER_BIT | QC_2P0_BIT, POWER_SUPPLY_TYPE_USB_HVDCP},
	{"HVDCP3", DCP_CHARGER_BIT | QC_3P0_BIT, POWER_SUPPLY_TYPE_USB_HVDCP_3},
};

static const struct apsd_result *smb138x_get_apsd_result(
						struct smb_charger *chg)
{
	int rc, i;
	u8 stat;

	rc = smb138x_read(chg, APSD_STATUS_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read APSD_STATUS rc=%d\n", rc);
		return &smb138x_apsd_results[0];
	}
	smb_dbg(chg, PR_REGISTER, "APSD_STATUS = 0x%02x\n", stat);

	if (!(stat & APSD_DTC_STATUS_DONE_BIT))
		return &smb138x_apsd_results[0];

	rc = smb138x_read(chg, APSD_RESULT_STATUS_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read APSD_RESULT_STATUS rc=%d\n",
			rc);
		return &smb138x_apsd_results[0];
	}
	stat &= APSD_RESULT_STATUS_MASK;

	for (i = 0; i < ARRAY_SIZE(smb138x_apsd_results); i++) {
		if (smb138x_apsd_results[i].bit == stat)
			return &smb138x_apsd_results[i];
	}

	pr_err("Couldn't find an APSD result for 0x%02x\n", stat);
	return &smb138x_apsd_results[0];
}

static int smb138x_enable_charging(struct smb_charger *chg, bool enable)
{
	int rc;

	rc = smb138x_masked_write(chg, CHARGING_ENABLE_CMD_REG,
				 CHARGING_ENABLE_CMD_BIT,
				 enable ? CHARGING_ENABLE_CMD_BIT : 0);
	if (rc < 0) {
		pr_err("Couldn't %s charging rc=%d\n",
			enable ? "enable" : "disable", rc);
		return rc;
	}

	return rc;
}

static int smb138x_set_charge_param(struct smb_charger *chg,
				struct smb_chg_param *param, int val_u)
{
	int rc = 0;
	u8 val_raw;

	if (val_u > param->max_u || val_u < param->min_u) {
		pr_err("%s: %d is out of range [%d, %d]\n",
			param->name, val_u, param->min_u, param->max_u);
		return -EINVAL;
	}

	val_raw = (val_u - param->min_u) / param->step_u;
	rc = smb138x_write(chg, param->reg, val_raw);
	if (rc < 0) {
		pr_err("%s: Couldn't write 0x%02x to 0x%04x rc=%d\n",
			param->name, val_raw, param->reg, rc);
		return rc;
	}

	smb_dbg(chg, PR_REGISTER, "%s = %d (0x%02x)\n",
		param->name, val_u, val_raw);

	return rc;
}

static int smb138x_set_usb_suspend(struct smb_charger *chg, bool suspend)
{
	int rc;

	rc = smb138x_masked_write(chg, USBIN_CMD_IL_REG, USBIN_SUSPEND_BIT,
				  suspend ? USBIN_SUSPEND_BIT : 0);
	if (rc < 0)
		pr_err("Couldn't write %s to USBIN_SUSPEND_BIT rc=%d\n",
			suspend ? "suspend" : "resume", rc);

	return rc;
}

static int smb138x_set_dc_suspend(struct smb_charger *chg, bool suspend)
{
	int rc;

	rc = smb138x_masked_write(chg, DCIN_CMD_IL_REG, DCIN_SUSPEND_BIT,
				  suspend ? DCIN_SUSPEND_BIT : 0);
	if (rc < 0)
		pr_err("Couldn't write %s to DCIN_SUSPEND_BIT rc=%d\n",
			suspend ? "suspend" : "resume", rc);

	return rc;
}

static int smb138x_usb_suspend_vote_callback(struct device *dev,
		int suspend, int client, int last_suspend, int last_client)
{
	struct smb_charger *chg = dev_get_drvdata(dev);
	int rc;

	rc = smb138x_set_usb_suspend(chg, suspend);
	if (rc < 0)
		pr_err("Couldn't %s USB rc=%d\n",
		       suspend ? "suspend" : "resume", rc);

	return rc;
}

static int smb138x_dc_suspend_vote_callback(struct device *dev,
		int suspend, int client, int last_suspend, int last_client)
{
	struct smb_charger *chg = dev_get_drvdata(dev);
	int rc;

	rc = smb138x_set_dc_suspend(chg, suspend);
	if (rc < 0)
		pr_err("Couldn't %s DC rc=%d\n",
		       suspend ? "suspend" : "resume", rc);

	return rc;
}

static int smb138x_fcc_vote_callback(struct device *dev,
		int fcc_ua, int client, int last_fcc_ua, int last_client)
{
	struct smb_charger *chg = dev_get_drvdata(dev);
	int rc;

	rc = smb138x_set_charge_param(chg, &chg->param.fcc, fcc_ua);
	if (rc < 0)
		pr_err("Couldn't set fast charge current %d rc=%d\n",
		       fcc_ua, rc);

	return rc;
}

#define USBIN_100MA 100000
static int smb138x_usb_icl_vote_callback(struct device *dev,
		int icl_ua, int client, int last_icl_ua, int last_client)
{
	struct smb_charger *chg = dev_get_drvdata(dev);
	const struct apsd_result *apsd_result = smb138x_get_apsd_result(chg);
	int rc = 0;

	if (apsd_result->pst == POWER_SUPPLY_TYPE_USB)
		rc = smb138x_masked_write(chg, USBIN_ICL_OPTIONS_REG,
				USB51_MODE_BIT,
				(icl_ua > USBIN_100MA) ? USB51_MODE_BIT : 0);
	else
		rc = smb138x_set_charge_param(chg, &chg->param.usb_icl, icl_ua);

	if (rc < 0)
		pr_err("Couldn't set USB input current limit %d rc=%d\n",
		       icl_ua, rc);

	return rc;
}

static int smb138x_dc_icl_vote_callback(struct device *dev,
		int icl_ua, int client, int last_icl_ua, int last_client)
{
	struct smb_charger *chg = dev_get_drvdata(dev);
	int rc;

	rc = smb138x_set_charge_param(chg, &chg->param.dc_icl, icl_ua);
	if (rc < 0)
		pr_err("Couldn't set DC input current limit %d rc=%d\n",
		       icl_ua, rc);

	return rc;
}

static int smb138x_vbus_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;

	rc = regmap_write(chg->regmap, CMD_OTG_REG, OTG_EN_BIT);
	if (rc < 0)
		pr_err("Couldn't enable OTG regulator rc=%d\n", rc);

	return rc;
}

static int smb138x_vbus_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;

	rc = regmap_write(chg->regmap, CMD_OTG_REG, 0);
	if (rc < 0)
		pr_err("Couldn't disable OTG regulator rc=%d\n", rc);

	return rc;
}

static int smb138x_vbus_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;
	u8 cmd;

	rc = smb138x_read(chg, CMD_OTG_REG, &cmd);
	if (rc < 0) {
		pr_err("Couldn't read CMD_OTG rc=%d", rc);
		return rc;
	}

	return (cmd & OTG_EN_BIT) ? 1 : 0;
}

static int smb138x_get_prop_input_suspend(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	val->intval = get_client_vote(chg->usb_suspend_votable, USER_VOTER);
	return 0;
}

static int smb138x_get_prop_batt_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smb138x_read(chg, BATIF_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		pr_err("Couldn't read BATIF_INT_RT_STS rc=%d\n",
			rc);
		return rc;
	}

	val->intval = !(stat & (BAT_THERM_OR_ID_MISSING_RT_STS_BIT |
				BAT_TERMINAL_MISSING_RT_STS_BIT));

	return rc;
}

static int smb138x_get_prop_batt_capacity(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	val->intval = 50;
	return 0;
}

static int smb138x_get_prop_batt_status(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smb138x_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}
	smb_dbg(chg, PR_REGISTER, "BATTERY_CHARGER_STATUS_1 = 0x%02x\n", stat);

	if (stat & CC_SOFT_TERMINATE_BIT) {
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		return rc;
	}

	stat = stat & BATTERY_CHARGER_STATUS_MASK;
	if (stat >= COMPLETED_CHARGE)
		val->intval = POWER_SUPPLY_STATUS_FULL;
	else
		val->intval = POWER_SUPPLY_STATUS_CHARGING;

	return rc;
}

static int smb138x_get_prop_batt_charge_type(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smb138x_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}
	smb_dbg(chg, PR_REGISTER, "BATTERY_CHARGER_STATUS_1 = 0x%02x\n", stat);

	switch (stat & BATTERY_CHARGER_STATUS_MASK) {
	case TRICKLE_CHARGE:
	case PRE_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case FAST_CHARGE:
	case FULLON_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case TAPER_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TAPER;
		break;
	default:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	return rc;
}

static int smb138x_get_prop_batt_health(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smb138x_read(chg, BATTERY_CHARGER_STATUS_2_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
			rc);
		return rc;
	}
	smb_dbg(chg, PR_REGISTER, "BATTERY_CHARGER_STATUS_2 = 0x%02x\n", stat);

	if (stat & CHARGER_ERROR_STATUS_BAT_OV_BIT) {
		pr_err("battery over-voltage\n");
		val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		goto done;
	}

	switch (stat & BAT_TEMP_STATUS_MASK) {
	case BAT_TEMP_STATUS_TOO_COLD_BIT:
		val->intval = POWER_SUPPLY_HEALTH_COLD;
		break;
	case BAT_TEMP_STATUS_TOO_HOT_BIT:
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	case BAT_TEMP_STATUS_COLD_SOFT_LIMIT_BIT:
		val->intval = POWER_SUPPLY_HEALTH_COOL;
		break;
	case BAT_TEMP_STATUS_HOT_SOFT_LIMIT_BIT:
		val->intval = POWER_SUPPLY_HEALTH_WARM;
		break;
	default:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	}

done:
	return rc;
}

static int smb138x_set_prop_input_suspend(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	int rc;

	rc = vote(chg->usb_suspend_votable, USER_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		pr_err("Couldn't vote to %s USB rc=%d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	rc = vote(chg->dc_suspend_votable, USER_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		pr_err("Couldn't vote to %s DC rc=%d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	return rc;
}

static irqreturn_t smb138x_handle_debug(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smb_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	return IRQ_HANDLED;
}

static irqreturn_t smb138x_handle_batt_psy_changed(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smb138x_handle_debug(irq, data);
	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t smb138x_handle_usb_plugin(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;
	u8 stat;
	bool attached;

	if (!chg->usb_psy)
		return IRQ_HANDLED;

	rc = smb138x_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		pr_err("Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	attached = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);

	power_supply_set_dp_dm(chg->usb_psy, attached ?
			       POWER_SUPPLY_DP_DM_DPF_DMF :
			       POWER_SUPPLY_DP_DM_DPR_DMR);
	power_supply_set_present(chg->usb_psy, attached);

	smb_dbg(chg, PR_INTERRUPT, "IRQ: %s %s\n",
		irq_data->name, attached ? "attached" : "detached");
	return IRQ_HANDLED;
}

static void smb138x_handle_slow_plugin_timeout(struct smb_charger *chg,
					      bool rising)
{
	smb_dbg(chg, PR_INTERRUPT, "IRQ: slow-plugin-timeout %s\n",
		rising ? "rising" : "falling");
}

static void smb138x_handle_sdp_enumeration_done(struct smb_charger *chg,
					       bool rising)
{
	smb_dbg(chg, PR_INTERRUPT, "IRQ: sdp-enumeration-done %s\n",
		rising ? "rising" : "falling");
}

static void smb138x_handle_adaptive_voltage_done(struct smb_charger *chg,
						bool rising)
{
	smb_dbg(chg, PR_INTERRUPT, "IRQ: adaptive-voltage-done %s\n",
		rising ? "rising" : "falling");
}

/* triggers when HVDCP 3.0 authentication has finished */
static void smb138x_handle_hvdcp_3p0_auth_done(struct smb_charger *chg,
					      bool rising)
{
	const struct apsd_result *apsd_result;

	if (!rising)
		return;

	apsd_result = smb138x_get_apsd_result(chg);
	power_supply_set_supply_type(chg->usb_psy, apsd_result->pst);
	smb_dbg(chg, PR_INTERRUPT, "IRQ: hvdcp-3p0-auth-done rising; %s detected\n",
		apsd_result->name);
}

/* triggers when HVDCP is detected */
static void smb138x_handle_hvdcp_detect_done(struct smb_charger *chg,
					    bool rising)
{
	if (!rising)
		return;

	smb_dbg(chg, PR_INTERRUPT, "IRQ: hvdcp-detect-done %s\n",
		rising ? "rising" : "falling");
}

static void smb138x_handle_apsd_done(struct smb_charger *chg, bool rising)
{
	const struct apsd_result *apsd_result;

	if (!rising)
		return;

	apsd_result = smb138x_get_apsd_result(chg);
	power_supply_set_supply_type(chg->usb_psy, apsd_result->pst);
	smb_dbg(chg, PR_INTERRUPT, "IRQ: apsd-done rising; %s detected\n",
		apsd_result->name);
}

static irqreturn_t smb138x_handle_usb_source_change(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc = 0;
	u8 stat;

	if (!chg->usb_psy)
		return IRQ_HANDLED;

	rc = smb138x_read(chg, APSD_STATUS_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read APSD_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	smb_dbg(chg, PR_REGISTER, "APSD_STATUS = 0x%02x\n", stat);

	smb138x_handle_apsd_done(chg,
		(bool)(stat & APSD_DTC_STATUS_DONE_BIT));

	smb138x_handle_hvdcp_detect_done(chg,
		(bool)(stat & QC_CHARGER_BIT));

	smb138x_handle_hvdcp_3p0_auth_done(chg,
		(bool)(stat & QC_AUTH_DONE_STATUS_BIT));

	smb138x_handle_adaptive_voltage_done(chg,
		(bool)(stat & VADP_CHANGE_DONE_AFTER_AUTH_BIT));

	smb138x_handle_sdp_enumeration_done(chg,
		(bool)(stat & ENUMERATION_DONE_BIT));

	smb138x_handle_slow_plugin_timeout(chg,
		(bool)(stat & SLOW_PLUGIN_TIMEOUT_BIT));

	power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

static irqreturn_t smb138x_handle_usb_typec_change(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	union power_supply_propval otg = {0, };
	int rc;
	u8 stat;

	if (!chg->usb_psy)
		return IRQ_HANDLED;

	rc = smb138x_read(chg, TYPE_C_STATUS_4_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read TYPE_C_STATUS_4 rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	smb_dbg(chg, PR_REGISTER, "TYPE_C_STATUS_4 = 0x%02x\n", stat);

	if (stat & TYPEC_VBUS_ERROR_STATUS_BIT) {
		dev_err(chg->dev, "IRQ: vbus-error rising\n");
		return IRQ_HANDLED;
	}

	if (stat & TYPEC_DEBOUNCE_DONE_STATUS_BIT)
		otg.intval = (bool)(stat & UFP_DFP_MODE_STATUS_BIT);

	rc = chg->usb_psy->set_property(chg->usb_psy, POWER_SUPPLY_PROP_USB_OTG,
					&otg);
	if (rc < 0)
		pr_err("Couldn't %s OTG rc=%d\n",
			otg.intval ? "enable" : "disable", rc);

	power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

static void smb138x_usb_init_work(struct work_struct *work)
{
	struct smb138x *chip = container_of(work, struct smb138x,
					    usb_init_work.work);
	struct smb_irq_data irq_data = {chip, "usb-init-work"};

	power_supply_unreg_notifier(&chip->nb);
	smb138x_handle_usb_plugin(0, &irq_data);
	smb138x_handle_usb_source_change(0, &irq_data);
	if (!chip->dt.micro_usb)
		smb138x_handle_usb_typec_change(0, &irq_data);
}

static enum power_supply_property smb138x_batt_props[] = {
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
};

static int smb138x_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb138x *chip = dev_get_drvdata(psy->dev->parent);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smb138x_get_prop_batt_status(chg, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = smb138x_get_prop_batt_health(chg, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smb138x_get_prop_batt_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smb138x_get_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = smb138x_get_prop_batt_charge_type(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smb138x_get_prop_batt_capacity(chg, val);
		break;
	default:
		pr_err("batt power supply prop %d not supported\n",
			psp);
		return -EINVAL;
	}

	return rc;
}

static int smb138x_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct smb138x *chip = dev_get_drvdata(psy->dev->parent);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smb138x_set_prop_input_suspend(chg, val);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int smb138x_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		return 1;
	default:
		break;
	}

	return 0;
}

static void smb138x_batt_external_power_changed(struct power_supply *psy)
{
	struct smb138x *chip = dev_get_drvdata(psy->dev->parent);
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval prop = {0, };
	int rc = 0;

	if (!chg->usb_psy)
		return;

	rc = chg->usb_psy->get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_TYPE, &prop);
	if (rc < 0) {
		pr_err("Couldn't get usb supply type rc=%d\n", rc);
		return;
	}

	if (prop.intval != POWER_SUPPLY_TYPE_USB)
		return;

	rc = chg->usb_psy->get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0) {
		pr_err("Couldn't get usb current max rc=%d\n", rc);
		return;
	}

	vote(chg->usb_icl_votable, USB_PSY_VOTER, true, prop.intval / 1000);
}

static int smb138x_init_batt_psy(struct smb138x *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	chg->batt_psy = devm_kzalloc(chg->dev, sizeof(*chg->batt_psy),
				     GFP_KERNEL);
	if (!chg->batt_psy)
		return -ENOMEM;

	chg->batt_psy->name = "battery";
	chg->batt_psy->type = POWER_SUPPLY_TYPE_BATTERY;
	chg->batt_psy->get_property = smb138x_batt_get_prop;
	chg->batt_psy->set_property = smb138x_batt_set_prop;
	chg->batt_psy->property_is_writeable = smb138x_batt_prop_is_writeable;
	chg->batt_psy->properties = smb138x_batt_props;
	chg->batt_psy->num_properties = ARRAY_SIZE(smb138x_batt_props);
	chg->batt_psy->external_power_changed =
					smb138x_batt_external_power_changed;

	rc = power_supply_register(chg->dev, chg->batt_psy);
	if (rc < 0)
		pr_err("Couldn't register battery power supply\n");

	return rc;
}

static struct regulator_ops smb138x_vbus_reg_ops = {
	.enable		= smb138x_vbus_regulator_enable,
	.disable	= smb138x_vbus_regulator_disable,
	.is_enabled	= smb138x_vbus_regulator_is_enabled,
};

static int smb138x_init_vbus_regulator(struct smb138x *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct regulator_config cfg = {};
	int rc = 0;

	chg->vbus_vreg = devm_kzalloc(chg->dev, sizeof(*chg->vbus_vreg),
				      GFP_KERNEL);
	if (!chg->vbus_vreg)
		return -ENOMEM;

	cfg.dev = chg->dev;
	cfg.driver_data = chip;

	chg->vbus_vreg->rdesc.owner = THIS_MODULE;
	chg->vbus_vreg->rdesc.type = REGULATOR_VOLTAGE;
	chg->vbus_vreg->rdesc.ops = &smb138x_vbus_reg_ops;
	chg->vbus_vreg->rdesc.of_match = "qcom,smb138x-vbus";
	chg->vbus_vreg->rdesc.name = "qcom,smb138x-vbus";

	chg->vbus_vreg->rdev = devm_regulator_register(chg->dev,
						&chg->vbus_vreg->rdesc, &cfg);
	if (IS_ERR(chg->vbus_vreg->rdev)) {
		rc = PTR_ERR(chg->vbus_vreg->rdev);
		chg->vbus_vreg->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't register VBUS regulator rc=%d\n", rc);
	}

	return rc;
}

static int smb138x_create_votables(struct smb138x *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	chg->usb_suspend_votable = create_votable(chg->dev,
					"USB_SUSPEND", VOTE_SET_ANY,
					NUM_VOTERS, 0,
					smb138x_usb_suspend_vote_callback);
	if (IS_ERR(chg->usb_suspend_votable)) {
		rc = PTR_ERR(chg->usb_suspend_votable);
		return rc;
	}

	chg->dc_suspend_votable = create_votable(chg->dev,
					"DC_SUSPEND", VOTE_SET_ANY,
					NUM_VOTERS, 0,
					smb138x_dc_suspend_vote_callback);
	if (IS_ERR(chg->dc_suspend_votable)) {
		rc = PTR_ERR(chg->dc_suspend_votable);
		return rc;
	}

	chg->fcc_votable = create_votable(chg->dev,
					"FCC", VOTE_MIN,
					NUM_VOTERS, SMB_DEFAULT_FCC_UA,
					smb138x_fcc_vote_callback);
	if (IS_ERR(chg->fcc_votable)) {
		rc = PTR_ERR(chg->fcc_votable);
		return rc;
	}

	chg->usb_icl_votable = create_votable(chg->dev,
					"USB_ICL", VOTE_MIN,
					NUM_VOTERS, SMB_DEFAULT_ICL_UA,
					smb138x_usb_icl_vote_callback);
	if (IS_ERR(chg->usb_icl_votable)) {
		rc = PTR_ERR(chg->usb_icl_votable);
		return rc;
	}

	chg->dc_icl_votable = create_votable(chg->dev,
					"DC_ICL", VOTE_MIN,
					NUM_VOTERS, SMB_DEFAULT_ICL_UA,
					smb138x_dc_icl_vote_callback);
	if (IS_ERR(chg->dc_icl_votable)) {
		rc = PTR_ERR(chg->dc_icl_votable);
		return rc;
	}

	return rc;
}

struct smb138x_irq_info {
	const char *name;
	const irq_handler_t handler;
};

static const struct smb138x_irq_info smb138x_irqs[] = {
/* CHARGER IRQs */
	{ "chg-error",			smb138x_handle_debug },
	{ "chg-state-change",		smb138x_handle_debug },
	{ "step-chg-state-change",	smb138x_handle_debug },
	{ "step-chg-soc-update-fail",	smb138x_handle_debug },
	{ "step-chg-soc-update-request", smb138x_handle_debug },
/* OTG IRQs */
	{ "otg-fail",			smb138x_handle_debug },
	{ "otg-overcurrent",		smb138x_handle_debug },
	{ "otg-oc-dis-sw-sts",		smb138x_handle_debug },
	{ "testmode-change-detect",	smb138x_handle_debug },
/* BATTERY IRQs */
	{ "bat-temp",			smb138x_handle_batt_psy_changed },
	{ "bat-ocp",			smb138x_handle_batt_psy_changed },
	{ "bat-ov",			smb138x_handle_batt_psy_changed },
	{ "bat-low",			smb138x_handle_batt_psy_changed },
	{ "bat-therm-or-id-missing",	smb138x_handle_batt_psy_changed },
	{ "bat-terminal-missing",	smb138x_handle_batt_psy_changed },
/* USB INPUT IRQs */
	{ "usbin-collapse",		smb138x_handle_debug },
	{ "usbin-lt-3p6v",		smb138x_handle_debug },
	{ "usbin-uv",			smb138x_handle_debug },
	{ "usbin-ov",			smb138x_handle_debug },
	{ "usbin-plugin",		smb138x_handle_usb_plugin },
	{ "usbin-src-change",		smb138x_handle_usb_source_change },
	{ "usbin-icl-change",		smb138x_handle_debug },
	{ "type-c-change",		smb138x_handle_usb_typec_change },
/* DC INPUT IRQs */
	{ "dcin-collapse",		smb138x_handle_debug },
	{ "dcin-lt-3p6v",		smb138x_handle_debug },
	{ "dcin-uv",			smb138x_handle_debug },
	{ "dcin-ov",			smb138x_handle_debug },
	{ "dcin-plugin",		smb138x_handle_debug },
	{ "div2-en-dg",			smb138x_handle_debug },
	{ "dcin-icl-change",		smb138x_handle_debug },
/* MISCELLANEOUS IRQs */
	{ "wdog-snarl",			smb138x_handle_debug },
	{ "wdog-bark",			smb138x_handle_debug },
	{ "aicl-fail",			smb138x_handle_debug },
	{ "aicl-done",			smb138x_handle_debug },
	{ "high-duty-cycle",		smb138x_handle_debug },
	{ "input-current-limiting",	smb138x_handle_debug },
	{ "temperature-change",		smb138x_handle_debug },
	{ "switcher-power-ok",		smb138x_handle_debug },
};

static int smb138x_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb138x_irqs); i++) {
		if (strcmp(smb138x_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int smb138x_request_interrupt(struct smb138x *chip,
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

	irq_index = smb138x_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	irq_data = devm_kzalloc(chg->dev, sizeof(*irq_data), GFP_KERNEL);
	if (!irq_data)
		return -ENOMEM;

	irq_data->parent_data = chip;
	irq_data->name = irq_name;

	rc = devm_request_threaded_irq(chg->dev, irq, NULL,
					smb138x_irqs[irq_index].handler,
					IRQF_ONESHOT, irq_name, irq_data);
	if (rc < 0) {
		pr_err("Couldn't request irq %d rc=%d\n", irq, rc);
		return rc;
	}

	return rc;
}

static int smb138x_request_interrupts(struct smb138x *chip)
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
			rc = smb138x_request_interrupt(chip, child, name);
			if (rc < 0) {
				pr_err("Coudn't request interrupt %s rc=%d\n",
				       name, rc);
				return rc;
			}
		}
	}

	return rc;
}

static int smb138x_notifier_call(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	struct smb138x *chip = container_of(nb, struct smb138x, nb);
	struct smb_charger *chg = &chip->chg;
	struct power_supply *psy = data;

	if (strcmp(psy->name, "usb") != 0)
		return NOTIFY_OK;

	chg->usb_psy = psy;
	schedule_delayed_work(&chip->usb_init_work, 100);
	return NOTIFY_OK;
}

static int smb138x_parse_dt(struct smb138x *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	int rc;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	chip->dt.suspend_input = of_property_read_bool(node,
				"qcom,suspend-input");

	chip->dt.micro_usb = of_property_read_bool(node,
				"qcom,micro-usb");

	rc = of_property_read_u32(node,
				"qcom,fcc-max-ua", &chip->dt.fcc_ua);
	if (rc < 0)
		chip->dt.fcc_ua = SMB_DEFAULT_FCC_UA;

	rc = of_property_read_u32(node,
				"qcom,usb-icl-ua", &chip->dt.usb_icl_ua);
	if (rc < 0)
		chip->dt.usb_icl_ua = SMB_DEFAULT_ICL_UA;

	rc = of_property_read_u32(node,
				"qcom,dc-icl-ua", &chip->dt.dc_icl_ua);
	if (rc < 0)
		chip->dt.dc_icl_ua = SMB_DEFAULT_ICL_UA;

	return 0;
}

static int smb138x_init_hw(struct smb138x *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc;

	/* enable missing battery detection */
	rc = smb138x_write(chg, BAT_MISS_SRC_CFG_REG,
			   BAT_MISS_BATID_SRC_EN_BIT |
			   BAT_MISS_THERM_SRC_EN_BIT);
	if (rc < 0) {
		pr_err("Couldn't enable missing battery detection rc=%d\n", rc);
		return rc;
	}

	/* votes must be cast before configuring software control */
	vote(chg->usb_suspend_votable,
		DEFAULT_VOTER, chip->dt.suspend_input, 0);
	vote(chg->dc_suspend_votable,
		DEFAULT_VOTER, chip->dt.suspend_input, 0);
	vote(chg->fcc_votable,
		DEFAULT_VOTER, true, chip->dt.fcc_ua);
	vote(chg->usb_icl_votable,
		DEFAULT_VOTER, true, chip->dt.usb_icl_ua);
	vote(chg->dc_icl_votable,
		DEFAULT_VOTER, true, chip->dt.dc_icl_ua);

	/* configure charge enable for software control; active high */
	rc = smb138x_masked_write(chg, CHGR_CFG2_REG,
				 CHG_EN_POLARITY_BIT | CHG_EN_SRC_BIT, 0);
	if (rc < 0) {
		pr_err("Couldn't configure charge enable source rc=%d\n", rc);
		return rc;
	}

	/* enable the charging path */
	rc = smb138x_enable_charging(chg, true);
	if (rc < 0) {
		pr_err("Couldn't enable charging rc=%d\n", rc);
		return rc;
	}

	/*
	 * trigger the usb-typec-change interrupt only when the CC state
	 * changes, or there was a VBUS error
	 */
	rc = smb138x_write(chg, TYPE_C_INTRPT_ENB_REG,
			   TYPEC_CCSTATE_CHANGE_INT_EN_BIT |
			   TYPEC_VBUS_ERROR_INT_EN_BIT);
	if (rc < 0) {
		pr_err("Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	/* configure VBUS for software control */
	rc = smb138x_masked_write(chg, OTG_CFG_REG, OTG_EN_SRC_CFG_BIT, 0);
	if (rc < 0) {
		pr_err("Couldn't configure VBUS for SW control rc=%d\n", rc);
		return rc;
	}

	/* configure power role for dual-role */
	rc = smb138x_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				  TYPEC_POWER_ROLE_CMD_MASK, 0);
	if (rc < 0) {
		pr_err("Couldn't configure power role for DRP rc=%d\n", rc);
		return rc;
	}

	/* enable source change interrupts */
	rc = smb138x_write(chg, USBIN_SOURCE_CHANGE_INTRPT_ENB_REG,
			   APSD_IRQ_EN_CFG_BIT |
			   HVDCP_IRQ_EN_CFG_BIT |
			   AUTH_IRQ_EN_CFG_BIT |
			   VADP_IRQ_EN_CFG_BIT |
			   ENUMERATION_IRQ_EN_CFG_BIT |
			   SLOW_IRQ_EN_CFG_BIT);
	if (rc < 0) {
		pr_err("Couldn't configure source change interrupts rc=%d\n",
		       rc);
		return rc;
	}

	/* configure USBIN options */
	rc = smb138x_write(chg, USBIN_OPTIONS_1_CFG_REG,
			   HVDCP_EN_BIT |
			   AUTO_SRC_DETECT_BIT |
			   INPUT_PRIORITY_BIT |
			   HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT |
			   HVDCP_AUTH_ALG_EN_CFG_BIT);
	if (rc < 0) {
		pr_err("Couldn't configure USBIN options rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int smb138x_master_probe(struct smb138x *chip)
{
	int rc = 0;

	rc = smb138x_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	rc = smb138x_create_votables(chip);
	if (rc < 0) {
		pr_err("Couldn't register votables rc=%d\n", rc);
		return rc;
	}

	rc = smb138x_init_vbus_regulator(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize vbus regulator rc=%d\n",
			rc);
		return rc;
	}

	rc = smb138x_init_batt_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		return rc;
	}

	rc = smb138x_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		return rc;
	}

	chip->nb.notifier_call = smb138x_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc=%d\n", rc);
		return rc;
	}

	rc = smb138x_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int smb138x_slave_probe(struct smb138x *chip)
{
	return 0;
}

static const struct of_device_id match_table[] = {
	{
		.compatible	= "qcom,smb138x-charger",
		.data		= (void *) PARALLEL_MASTER,
	},
	{
		.compatible	= "qcom,smb138x-parallel-slave",
		.data		= (void *) PARALLEL_SLAVE
	},
	{ },
};

static int smb138x_probe(struct platform_device *pdev)
{
	struct smb138x *chip;
	struct smb_charger *chg;
	const struct of_device_id *id;
	int rc = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chg = &chip->chg;
	chg->dev = &pdev->dev;
	chg->param = v1_params;
	chg->debug_mask = &smb138x_debug_mask;
	mutex_init(&chg->write_lock);
	INIT_DELAYED_WORK(&chip->usb_init_work, smb138x_usb_init_work);

	chg->regmap = dev_get_regmap(chg->dev->parent, NULL);
	if (!chg->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}

	id = of_match_device(of_match_ptr(match_table), chip->chg.dev);
	if (!id) {
		pr_err("Couldn't find a matching device\n");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, chip);
	device_init_wakeup(chg->dev, true);

	chg->mode = (enum smb_mode) id->data;
	switch (chg->mode) {
	case PARALLEL_MASTER:
		rc = smb138x_master_probe(chip);
		break;
	case PARALLEL_SLAVE:
		rc = smb138x_slave_probe(chip);
		break;
	default:
		pr_err("Couldn't find a matching mode %d\n", chg->mode);
		rc = -EINVAL;
		goto cleanup;
	}

	pr_info("SMB138X probed successfully mode=%d\n", chip->chg.mode);
	return rc;

cleanup:
	if (chg->batt_psy)
		power_supply_unregister(chg->batt_psy);
	device_init_wakeup(chg->dev, false);
	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int smb138x_remove(struct platform_device *pdev)
{
	struct smb138x *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;

	if (chg->batt_psy)
		power_supply_unregister(chg->batt_psy);
	device_init_wakeup(chg->dev, false);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver smb138x_driver = {
	.driver		= {
		.name		= "qcom,smb138x-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= smb138x_probe,
	.remove		= smb138x_remove,
};
module_platform_driver(smb138x_driver);

MODULE_DESCRIPTION("QPNP SMB138X Charger Driver");
MODULE_LICENSE("GPL v2");
