// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/iio/consumer.h>
#include <linux/pmic-voter.h>
#include <linux/of_batterydata.h>
#include <linux/ktime.h>
#include "smblite-lib.h"
#include "smblite-reg.h"
#include "step-chg-jeita.h"
#include "storm-watch.h"
#include "schgm-flashlite.h"

#define smblite_lib_err(chg, fmt, ...)		\
	pr_err("%s: %s: " fmt, chg->name,	\
		__func__, ##__VA_ARGS__)	\

#define smblite_lib_dbg(chg, reason, fmt, ...)			\
	do {							\
		if (*chg->debug_mask & (reason))		\
			pr_info("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
		else						\
			pr_debug("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
	} while (0)

#define typec_rp_med_high(chg, typec_mode)			\
	((typec_mode == POWER_SUPPLY_TYPEC_SOURCE_MEDIUM	\
	|| typec_mode == POWER_SUPPLY_TYPEC_SOURCE_HIGH)	\
	&& (!chg->typec_legacy || chg->typec_legacy_use_rp_icl))

static void update_sw_icl_max(struct smb_charger *chg,
				enum power_supply_type type);
static int smblite_lib_get_prop_typec_mode(struct smb_charger *chg);

int smblite_lib_read(struct smb_charger *chg, u16 addr, u8 *val)
{
	unsigned int value;
	int rc = 0;

	rc = regmap_read(chg->regmap, addr, &value);
	if (rc >= 0)
		*val = (u8)value;

	return rc;
}

int smblite_lib_batch_read(struct smb_charger *chg, u16 addr, u8 *val,
			int count)
{
	return regmap_bulk_read(chg->regmap, addr, val, count);
}

int smblite_lib_write(struct smb_charger *chg, u16 addr, u8 val)
{
	return regmap_write(chg->regmap, addr, val);
}

int smblite_lib_batch_write(struct smb_charger *chg, u16 addr, u8 *val,
			int count)
{
	return regmap_bulk_write(chg->regmap, addr, val, count);
}

int smblite_lib_masked_write(struct smb_charger *chg, u16 addr, u8 mask, u8 val)
{
	return regmap_update_bits(chg->regmap, addr, mask, val);
}

int smblite_lib_get_iio_channel(struct smb_charger *chg, const char *propname,
					struct iio_channel **chan)
{
	int rc = 0;

	rc = of_property_match_string(chg->dev->of_node,
					"io-channel-names", propname);
	if (rc < 0)
		return 0;

	*chan = iio_channel_get(chg->dev, propname);
	if (IS_ERR(*chan)) {
		rc = PTR_ERR(*chan);
		if (rc != -EPROBE_DEFER)
			smblite_lib_err(chg, "%s channel unavailable, %d\n",
							propname, rc);
		*chan = NULL;
	}

	return rc;
}

#define DIV_FACTOR_MICRO_V_I	1
#define DIV_FACTOR_MILI_V_I	1000
#define DIV_FACTOR_DECIDEGC	100
int smblite_lib_read_iio_channel(struct smb_charger *chg,
				struct iio_channel *chan, int div, int *data)
{
	int rc = 0;
	*data = -ENODATA;

	if (chan) {
		rc = iio_read_channel_processed(chan, data);
		if (rc < 0) {
			smblite_lib_err(chg, "Error in reading IIO channel data, rc=%d\n",
					rc);
			return rc;
		}

		if (div != 0)
			*data /= div;
	}

	return rc;
}

int smblite_lib_icl_override(struct smb_charger *chg,
				enum icl_override_mode  mode)
{
	int rc;
	u8 usb51_mode, icl_override;

	switch (mode) {
	case SW_OVERRIDE_USB51_MODE:
		usb51_mode = 0;
		icl_override = ICL_OVERRIDE_BIT;
		break;
	case SW_OVERRIDE_HC_MODE:
		usb51_mode = USBIN_MODE_CHG_BIT;
		icl_override = ICL_OVERRIDE_BIT;
		break;
	case HW_AUTO_MODE:
	default:
		usb51_mode = USBIN_MODE_CHG_BIT;
		icl_override = 0;
		break;
	}

	rc = smblite_lib_masked_write(chg, USBIN_ICL_OPTIONS_REG,
				USBIN_MODE_CHG_BIT, usb51_mode);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't set USBIN_ICL_OPTIONS rc=%d\n",
					rc);
		return rc;
	}

	rc = smblite_lib_masked_write(chg, CMD_ICL_OVERRIDE_REG,
				ICL_OVERRIDE_BIT, icl_override);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't override ICL rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static void smblite_lib_notify_extcon_props(struct smb_charger *chg, int id)
{
	union extcon_property_value val;
	union power_supply_propval prop_val;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC) {
		smblite_lib_get_prop_typec_cc_orientation(chg, &prop_val);
		val.intval = ((prop_val.intval == 2) ? 1 : 0);
		extcon_set_property(chg->extcon, id,
				EXTCON_PROP_USB_TYPEC_POLARITY, val);
		val.intval = true;
		extcon_set_property(chg->extcon, id,
				EXTCON_PROP_USB_SS, val);
	} else if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		val.intval = false;
		extcon_set_property(chg->extcon, id,
				EXTCON_PROP_USB_SS, val);
	}
}

static void smblite_lib_notify_device_mode(struct smb_charger *chg, bool enable)
{
	if (enable)
		smblite_lib_notify_extcon_props(chg, EXTCON_USB);

	extcon_set_state_sync(chg->extcon, EXTCON_USB, enable);
}

#define VBOOST_5P00V	0x03
static void smblite_lib_notify_usb_host(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	/* LDO mode doesn't support OTG */
	if (chg->ldo_mode)
		return;

	if (enable) {
		smblite_lib_dbg(chg, PR_OTG, "enabling VBUS in OTG mode\n");
		rc = smblite_lib_masked_write(chg, DCDC_CMD_OTG_REG,
					OTG_EN_BIT, OTG_EN_BIT);
		if (rc < 0) {
			smblite_lib_err(chg,
				"Couldn't enable VBUS in OTG mode rc=%d\n", rc);
			return;
		}
		rc = smblite_lib_masked_write(chg, DCDC_BST_VREG_SEL,
					VBOOST_MASK, VBOOST_5P00V);
		if (rc < 0) {
			smblite_lib_err(chg,
				"Couldn't write BST_VREG_SEL rc=%d\n", rc);
			return;
		}
		smblite_lib_notify_extcon_props(chg, EXTCON_USB_HOST);
	} else {
		smblite_lib_dbg(chg, PR_OTG, "disabling VBUS in OTG mode\n");
		rc = smblite_lib_masked_write(chg, DCDC_CMD_OTG_REG,
					OTG_EN_BIT, 0);
		if (rc < 0) {
			smblite_lib_err(chg,
				"Couldn't disable VBUS in OTG mode rc=%d\n",
				rc);
			return;
		}
	}

	extcon_set_state_sync(chg->extcon, EXTCON_USB_HOST, enable);
	chg->otg_present = enable;
}

/********************
 * REGISTER GETTERS *
 ********************/

int smblite_lib_get_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int *val_u)
{
	int rc = 0;
	u8 val_raw;

	rc = smblite_lib_read(chg, param->reg, &val_raw);
	if (rc < 0) {
		smblite_lib_err(chg, "%s: Couldn't read from 0x%04x rc=%d\n",
			param->name, param->reg, rc);
		return rc;
	}

	if (param->get_proc)
		*val_u = param->get_proc(param, val_raw);
	else
		*val_u = val_raw * param->step_u + param->min_u;
	smblite_lib_dbg(chg, PR_REGISTER, "%s = %d (0x%02x)\n",
		   param->name, *val_u, val_raw);

	return rc;
}

int smblite_lib_get_usb_suspend(struct smb_charger *chg, int *suspend)
{
	int rc = 0;
	u8 temp;

	rc = smblite_lib_read(chg, USBIN_INPUT_SUSPEND_REG, &temp);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read USBIN_CMD_IL rc=%d\n", rc);
		return rc;
	}
	*suspend = temp & USBIN_SUSPEND_BIT;

	return rc;
}

#define INPUT_NOT_PRESENT	0
#define INPUT_PRESENT_USB	BIT(1)
static int smblite_lib_is_input_present(struct smb_charger *chg,
				   int *present)
{
	int rc;
	union power_supply_propval pval = {0, };

	*present = INPUT_NOT_PRESENT;

	rc = smblite_lib_get_prop_usb_present(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get usb presence status rc=%d\n", rc);
		return rc;
	}
	*present |= pval.intval ? INPUT_PRESENT_USB : INPUT_NOT_PRESENT;

	return 0;
}

/********************
 * REGISTER SETTERS *
 ********************/
int smblite_lib_set_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int val_u)
{
	int rc = 0;
	u8 val_raw;

	if (param->set_proc) {
		rc = param->set_proc(param, val_u, &val_raw);
		if (rc < 0)
			return -EINVAL;
	} else {
		if (val_u > param->max_u || val_u < param->min_u)
			smblite_lib_dbg(chg, PR_MISC,
				"%s: %d is out of range [%d, %d]\n",
				param->name, val_u, param->min_u, param->max_u);

		if (val_u > param->max_u)
			val_u = param->max_u;
		if (val_u < param->min_u)
			val_u = param->min_u;

		val_raw = (val_u - param->min_u) / param->step_u;
	}

	rc = smblite_lib_write(chg, param->reg, val_raw);
	if (rc < 0) {
		smblite_lib_err(chg, "%s: Couldn't write 0x%02x to 0x%04x rc=%d\n",
			param->name, val_raw, param->reg, rc);
		return rc;
	}

	smblite_lib_dbg(chg, PR_REGISTER, "%s = %d (0x%02x)\n",
		   param->name, val_u, val_raw);

	return rc;
}

int smblite_lib_set_usb_suspend(struct smb_charger *chg, bool suspend)
{
	int rc = 0;

	if (suspend)
		vote(chg->icl_irq_disable_votable, USB_SUSPEND_VOTER,
				true, 0);

	rc = smblite_lib_masked_write(chg, USBIN_INPUT_SUSPEND_REG,
				USBIN_SUSPEND_BIT,
				suspend ? USBIN_SUSPEND_BIT : 0);
	if (rc < 0)
		smblite_lib_err(chg, "Couldn't write %s to USBIN_SUSPEND_BIT rc=%d\n",
			suspend ? "suspend" : "resume", rc);

	if (!suspend)
		vote(chg->icl_irq_disable_votable, USB_SUSPEND_VOTER,
				false, 0);

	return rc;
}

/********************
 * HELPER FUNCTIONS *
 ********************/
int smblite_lib_get_prop_from_bms(struct smb_charger *chg,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int rc;

	if (!chg->bms_psy)
		return -EINVAL;

	rc = power_supply_get_property(chg->bms_psy, psp, val);

	return rc;
}

static void smblite_lib_update_usb_type(struct smb_charger *chg,
					enum power_supply_type type)
{
	chg->real_charger_type = type;
	smblite_update_usb_desc(chg);
}

static int smblite_lib_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct smb_charger *chg = container_of(nb, struct smb_charger, nb);

	if (!strcmp(psy->desc->name, "bms")) {
		if (!chg->bms_psy)
			chg->bms_psy = psy;
		if (ev == PSY_EVENT_PROP_CHANGED)
			schedule_work(&chg->bms_update_work);
	}

	if (chg->jeita_configured == JEITA_CFG_NONE)
		schedule_work(&chg->jeita_update_work);

	return NOTIFY_OK;
}

static int smblite_lib_register_notifier(struct smb_charger *chg)
{
	int rc;

	chg->nb.notifier_call = smblite_lib_notifier_call;
	rc = power_supply_reg_notifier(&chg->nb);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't register psy notifier rc = %d\n",
					rc);
		return rc;
	}

	return 0;
}

static void smblite_lib_uusb_removal(struct smb_charger *chg)
{
	int rc;
	struct smb_irq_data *data;
	struct storm_watch *wdata;

	cancel_delayed_work_sync(&chg->pl_enable_work);

	if (chg->wa_flags & BOOST_BACK_WA) {
		data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
		if (data) {
			wdata = &data->storm_data;
			update_storm_count(wdata, WEAK_CHG_STORM_COUNT);
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					false, 0);
		}
	}
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);

	/* reset both usbin current and voltage votes */
	vote(chg->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, false, 0);
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
			is_flashlite_active(chg) ? USBIN_200UA : USBIN_100UA);
	vote(chg->usb_icl_votable, FLASH_ACTIVE_VOTER, false, 0);

	/* Remove SW thermal regulation votes */
	vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER, false, 0);
	vote(chg->awake_votable, SW_THERM_REGULATION_VOTER, false, 0);

	/* clear USB ICL vote for USB_PSY_VOTER */
	rc = vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	if (rc < 0)
		smblite_lib_err(chg, "Couldn't un-vote for USB ICL rc=%d\n",
					rc);
}

void smblite_lib_suspend_on_debug_battery(struct smb_charger *chg)
{
	int rc;
	union power_supply_propval val;

	rc = smblite_lib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_DEBUG_BATTERY, &val);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get debug battery prop rc=%d\n",
					rc);
		return;
	}
	if (chg->suspend_input_on_debug_batt) {
		vote(chg->usb_icl_votable, DEBUG_BOARD_VOTER, val.intval, 0);
		if (val.intval) {
			pr_info("Input suspended: Fake battery\n");
			schgm_flashlite_config_usbin_collapse(chg, false);
		}
	} else {
		vote(chg->chg_disable_votable, DEBUG_BOARD_VOTER,
					val.intval, 0);
	}
}

static int set_sdp_current(struct smb_charger *chg, int icl_ua)
{
	int rc;
	u8 icl_options;
	enum icl_override_mode icl_override = SW_OVERRIDE_USB51_MODE;

	/* power source is SDP */
	switch (icl_ua) {
	case USBIN_100UA:
	case USBIN_150UA:
		/* USB 2.0 100mA */
		icl_options = 0;
		break;
	case USBIN_500UA:
		icl_options = USB51_MODE_BIT;
		break;
	default:
		/* Use ICL configuration register for HC_MODE*/
		icl_override = SW_OVERRIDE_HC_MODE;
		icl_options = USBIN_MODE_CHG_BIT;
		break;
	}

	rc = smblite_lib_set_charge_param(chg, &chg->param.usb_icl,
						icl_ua);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't set HC ICL rc=%d\n", rc);
		return rc;
	}

	rc = smblite_lib_masked_write(chg,
			USBIN_ICL_OPTIONS_REG, USB51_MODE_BIT, icl_options);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't set ICL options rc=%d\n", rc);
		return rc;
	}

	rc = smblite_lib_icl_override(chg, icl_override);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't set ICL override rc=%d\n", rc);
		return rc;
	}

	return rc;
}

int smblite_lib_set_icl_current(struct smb_charger *chg, int icl_ua)
{
	int rc = 0;
	enum icl_override_mode icl_override = HW_AUTO_MODE;
	/* suspend if 25mA or less is requested */
	bool suspend = (icl_ua <= USBIN_25UA);

	schgm_flashlite_torch_priority(chg, suspend ? TORCH_BOOST_MODE :
							TORCH_BUCK_MODE);
	/* Do not configure ICL from SW for DAM */
	if (smblite_lib_get_prop_typec_mode(chg) ==
			    POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY)
		return 0;

	if (suspend)
		return smblite_lib_set_usb_suspend(chg, true);

	/* No client updated it's vote */
	if (icl_ua == INT_MAX)
		goto set_mode;

	/* configure current */
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB
		&& (chg->typec_legacy
		|| chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
		|| chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)) {
		rc = set_sdp_current(chg, icl_ua);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't set SDP ICL rc=%d\n",
						rc);
			goto out;
		}

		goto unsuspend;
	} else {
		/*
		 * Try USB 2.0/3,0 option first on USB path when maximum input
		 * current limit is 500mA or below for better accuracy; in case
		 * of error, proceed to use USB high-current mode.
		 */
		if (icl_ua <= USBIN_500UA) {
			rc = set_sdp_current(chg, icl_ua);
			if (rc >= 0)
				goto unsuspend;
		}

		rc = smblite_lib_set_charge_param(chg, &chg->param.usb_icl,
						icl_ua);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't set HC ICL rc=%d\n", rc);
			goto out;
		}
		icl_override = SW_OVERRIDE_HC_MODE;
	}

set_mode:
	rc = smblite_lib_icl_override(chg, icl_override);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't set ICL override rc=%d\n", rc);
		goto out;
	}

unsuspend:
	/* unsuspend after configuring current and override */
	rc = smblite_lib_set_usb_suspend(chg, false);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't resume input rc=%d\n", rc);
		goto out;
	}

	/* Re-run AICL */
	if (icl_override != SW_OVERRIDE_HC_MODE)
		rc = smblite_lib_run_aicl(chg, RERUN_AICL);
out:
	return rc;
}

int smblite_lib_get_icl_current(struct smb_charger *chg, int *icl_ua)
{
	int rc;

	rc = smblite_lib_get_charge_param(chg, &chg->param.icl_max_stat,
						icl_ua);
	if (rc < 0)
		smblite_lib_err(chg, "Couldn't get HC ICL rc=%d\n", rc);

	return rc;
}

/*********************
 * VOTABLE CALLBACKS *
 *********************/
static int smblite_lib_awake_vote_callback(struct votable *votable, void *data,
			int awake, const char *client)
{
	struct smb_charger *chg = data;

	if (awake)
		pm_stay_awake(chg->dev);
	else
		pm_relax(chg->dev);

	return 0;
}

static int smblite_lib_chg_disable_vote_callback(struct votable *votable,
				void *data, int chg_disable, const char *client)
{
	struct smb_charger *chg = data;
	int rc;

	rc = smblite_lib_masked_write(chg, CHARGING_ENABLE_CMD_REG,
				 CHARGING_ENABLE_CMD_BIT,
				 chg_disable ? 0 : CHARGING_ENABLE_CMD_BIT);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't %s charging rc=%d\n",
			chg_disable ? "disable" : "enable", rc);
		return rc;
	}

	return 0;
}

static int smblite_lib_icl_irq_disable_vote_callback(struct votable *votable,
				void *data, int disable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq)
		return 0;

	if (chg->irq_info[USBIN_ICL_CHANGE_IRQ].enabled) {
		if (disable)
			disable_irq_nosync(
				chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq);
	} else {
		if (!disable)
			enable_irq(chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq);
	}

	chg->irq_info[USBIN_ICL_CHANGE_IRQ].enabled = !disable;

	return 0;
}

static int smblite_lib_temp_change_irq_disable_vote_callback(
		struct votable *votable, void *data,
		int disable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[TEMP_CHANGE_IRQ].irq)
		return 0;

	if (chg->irq_info[TEMP_CHANGE_IRQ].enabled && disable) {
		if (chg->irq_info[TEMP_CHANGE_IRQ].wake)
			disable_irq_wake(chg->irq_info[TEMP_CHANGE_IRQ].irq);
		disable_irq_nosync(chg->irq_info[TEMP_CHANGE_IRQ].irq);
	} else if (!chg->irq_info[TEMP_CHANGE_IRQ].enabled && !disable) {
		enable_irq(chg->irq_info[TEMP_CHANGE_IRQ].irq);
		if (chg->irq_info[TEMP_CHANGE_IRQ].wake)
			enable_irq_wake(chg->irq_info[TEMP_CHANGE_IRQ].irq);
	}

	chg->irq_info[TEMP_CHANGE_IRQ].enabled = !disable;

	return 0;
}

/********************
 * BATT PSY GETTERS *
 ********************/

int smblite_lib_get_prop_input_suspend(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	val->intval
		= (get_client_vote(chg->usb_icl_votable, USER_VOTER) == 0);
	return 0;
}

int smblite_lib_get_prop_batt_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblite_lib_read(chg, BATIF_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read BATIF_INT_RT_STS rc=%d\n",
					rc);
		return rc;
	}

	val->intval = !(stat & BAT_THERM_OR_ID_MISSING_RT_STS_BIT);

	return rc;
}

int smblite_lib_get_prop_batt_capacity(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	int rc = -EINVAL;

	if (chg->fake_capacity >= 0) {
		val->intval = chg->fake_capacity;
		return 0;
	}

	rc = smblite_lib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_CAPACITY,
						val);

	return rc;
}

static bool is_charging_paused(struct smb_charger *chg)
{
	int rc;
	u8 val;

	rc = smblite_lib_read(chg, CHARGING_ENABLE_CMD_REG, &val);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read CHARGING_PAUSE_CMD rc=%d\n",
					rc);
		return false;
	}

	return val & CHARGING_PAUSE_CMD_BIT;
}

#define CUTOFF_COUNT		3
int smblite_lib_get_prop_batt_status(struct smb_charger *chg,
				union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	bool usb_online;
	u8 stat;
	int rc, input_present = 0;

	if (chg->fake_chg_status_on_debug_batt) {
		rc = smblite_lib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_DEBUG_BATTERY, &pval);
		if (rc < 0) {
			pr_err_ratelimited("Couldn't get debug battery prop rc=%d\n",
					rc);
		} else if (pval.intval == 1) {
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
			return 0;
		}
	}

	/*
	 * If SOC = 0 and we are discharging with input connected, report
	 * the battery status as DISCHARGING.
	 */
	smblite_lib_is_input_present(chg, &input_present);
	rc = smblite_lib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (!rc && pval.intval == 0 && input_present) {
		rc = smblite_lib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (!rc && pval.intval > 0) {
			if (chg->cutoff_count > CUTOFF_COUNT) {
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
				return 0;
			}
			chg->cutoff_count++;
		} else {
			chg->cutoff_count = 0;
		}
	} else {
		chg->cutoff_count = 0;
	}

	rc = smblite_lib_get_prop_usb_online(chg, &pval);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get usb online property rc=%d\n",
			rc);
		return rc;
	}
	usb_online = (bool)pval.intval;

	rc = smblite_lib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}
	stat = stat & BATTERY_CHARGER_STATUS_MASK;

	if (!usb_online) {
		switch (stat) {
		case TERMINATE_CHARGE:
		case INHIBIT_CHARGE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		}
		return rc;
	}

	switch (stat) {
	case TRICKLE_CHARGE:
	case PRE_CHARGE:
	case FULLON_CHARGE:
	case TAPER_CHARGE:
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case TERMINATE_CHARGE:
	case INHIBIT_CHARGE:
		val->intval = POWER_SUPPLY_STATUS_FULL;
		break;
	case DISABLE_CHARGE:
	case PAUSE_CHARGE:
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	default:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	if (is_charging_paused(chg)) {
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		return 0;
	}

	if (val->intval != POWER_SUPPLY_STATUS_CHARGING)
		return 0;

	if (!usb_online
		&& chg->fake_batt_status == POWER_SUPPLY_STATUS_FULL) {
		val->intval = POWER_SUPPLY_STATUS_FULL;
		return 0;
	}

	return 0;
}

int smblite_lib_get_prop_batt_charge_type(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblite_lib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}

	switch (stat & BATTERY_CHARGER_STATUS_MASK) {
	case TRICKLE_CHARGE:
	case PRE_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case FULLON_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case TAPER_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TAPER;
		break;
	default:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return rc;
}

int smblite_lib_get_prop_batt_health(struct smb_charger *chg,
				union power_supply_propval *val)
{
	union power_supply_propval pval;
	int rc;
	int effective_fv_uv;
	u8 stat;

	rc = smblite_lib_read(chg, CHARGER_VBAT_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read CHARGER_VBAT_STATUS_REG rc=%d\n",
			rc);
		return rc;
	}
	smblite_lib_dbg(chg, PR_REGISTER, "CHARGER_VBAT_STATUS_REG = 0x%02x\n",
		   stat);

	if (stat & BAT_OV_BIT) {
		rc = smblite_lib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (!rc) {
			/*
			 * If Vbatt is within 40mV above Vfloat, then don't
			 * treat it as overvoltage.
			 */
			effective_fv_uv = get_effective_result(chg->fv_votable);
			if (pval.intval >= effective_fv_uv + 40000) {
				val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				smblite_lib_err(chg, "battery over-voltage vbat_fg = %duV, fv = %duV\n",
						pval.intval, effective_fv_uv);
				goto done;
			}
		}
	}

	rc = smblite_lib_read(chg, BATTERY_TEMP_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
			rc);
		return rc;
	}
	if (stat & BAT_TEMP_STATUS_TOO_COLD_BIT)
		val->intval = POWER_SUPPLY_HEALTH_COLD;
	else if (stat & BAT_TEMP_STATUS_TOO_HOT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (stat & BAT_TEMP_STATUS_COLD_SOFT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_COOL;
	else if (stat & BAT_TEMP_STATUS_HOT_SOFT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_WARM;
	else
		val->intval = POWER_SUPPLY_HEALTH_GOOD;

done:
	return rc;
}

int smblite_lib_get_prop_system_temp_level(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->system_temp_level;
	return 0;
}

int smblite_lib_get_prop_system_temp_level_max(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->thermal_levels;
	return 0;
}

int smblite_lib_get_prop_input_current_limited(struct smb_charger *chg,
				union power_supply_propval *val)
{
	u8 stat;
	int rc;

	rc = smblite_lib_read(chg, AICL_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read AICL_STATUS rc=%d\n", rc);
		return rc;
	}

	val->intval = (stat & SOFT_ILIMIT_BIT);

	return 0;
}

int smblite_lib_get_prop_batt_iterm(struct smb_charger *chg,
		union power_supply_propval *val)
{
	int rc, temp;
	u8 stat, buf[2];

	/* Currently, only ADC comparator-based termination is supported
	 * and validate, hence read only the threshold corresponding to ADC
	 * source. Proceed only if CHGR_ITERM_USE_ANALOG_BIT is 0.
	 */
	rc = smblite_lib_read(chg, CHGR_TERM_CFG_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read CHGR_TERM_CFG_REG rc=%d\n",
				rc);
		return rc;
	}

	if (stat & CHGR_ITERM_USE_ANALOG_BIT) {
		val->intval = -EINVAL;
		return 0;
	}

	rc = smblite_lib_batch_read(chg, CHGR_ADC_ITERM_UP_THD_MSB_REG, buf, 2);

	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read CHGR_ADC_ITERM_UP_THD_MSB_REG rc=%d\n",
				rc);
		return rc;
	}

	temp = buf[1] | (buf[0] << 8);
	temp = sign_extend32(temp, 15);

	temp = DIV_ROUND_CLOSEST(temp * ITERM_LIMITS_MA,
					ADC_CHG_ITERM_MASK);

	val->intval = temp;

	return rc;
}

int smblite_lib_get_prop_batt_charge_done(struct smb_charger *chg,
					union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblite_lib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}

	stat = stat & BATTERY_CHARGER_STATUS_MASK;
	val->intval = (stat == TERMINATE_CHARGE);
	return 0;
}

int smblite_lib_get_batt_current_now(struct smb_charger *chg,
					union power_supply_propval *val)
{
	int rc;

	rc = smblite_lib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_CURRENT_NOW, val);
	if (!rc)
		val->intval *= (-1);

	return rc;
}

/***********************
 * BATTERY PSY SETTERS *
 ***********************/

int smblite_lib_set_prop_input_suspend(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	int rc;

	/* vote 0mA when suspended */
	rc = vote(chg->usb_icl_votable, USER_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't vote to %s USB rc=%d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	power_supply_changed(chg->batt_psy);
	return rc;
}

int smblite_lib_set_prop_batt_capacity(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	chg->fake_capacity = val->intval;

	power_supply_changed(chg->batt_psy);

	return 0;
}

int smblite_lib_set_prop_batt_status(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	/* Faking battery full */
	if (val->intval == POWER_SUPPLY_STATUS_FULL)
		chg->fake_batt_status = val->intval;
	else
		chg->fake_batt_status = -EINVAL;

	power_supply_changed(chg->batt_psy);

	return 0;
}

int smblite_lib_set_prop_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	if (val->intval < 0)
		return -EINVAL;

	if (chg->thermal_levels <= 0)
		return -EINVAL;

	if (val->intval > chg->thermal_levels)
		return -EINVAL;

	chg->system_temp_level = val->intval;

	if (chg->system_temp_level == chg->thermal_levels)
		return vote(chg->chg_disable_votable,
			THERMAL_DAEMON_VOTER, true, 0);

	vote(chg->chg_disable_votable, THERMAL_DAEMON_VOTER, false, 0);
	if (chg->system_temp_level == 0)
		return vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);

	vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, true,
			chg->thermal_mitigation[chg->system_temp_level]);
	return 0;
}

int smblite_lib_set_prop_rechg_soc_thresh(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc;
	u8 new_thr = DIV_ROUND_CLOSEST(val->intval * 255, 100);

	rc = smblite_lib_write(chg, CHARGE_RCHG_SOC_THRESHOLD_CFG_REG,
			new_thr);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't write to RCHG_SOC_THRESHOLD_CFG_REG rc=%d\n",
				rc);
		return rc;
	}

	chg->auto_recharge_soc = val->intval;

	return rc;
}

int smblite_lib_run_aicl(struct smb_charger *chg, int type)
{
	int rc;
	u8 stat;

	rc = smblite_lib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
								rc);
		return rc;
	}

	/* USB is suspended so skip re-running AICL */
	if (stat & USBIN_SUSPEND_STS_BIT)
		return rc;

	smblite_lib_dbg(chg, PR_MISC, "re-running AICL\n");

	stat = (type == RERUN_AICL) ? RERUN_AICL_BIT : RESTART_AICL_BIT;
	rc = smblite_lib_masked_write(chg, AICL_CMD_REG, stat, stat);
	if (rc < 0)
		smblite_lib_err(chg, "Couldn't write to AICL_CMD_REG rc=%d\n",
				rc);
	return 0;
}

/*******************
 * USB PSY GETTERS *
 *******************/

int smblite_lib_get_prop_usb_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblite_lib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read USBIN_RT_STS rc=%d\n", rc);
		return rc;
	}

	val->intval = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);
	return 0;
}

int smblite_lib_get_prop_usb_online(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	int rc = 0;
	u8 stat;

	if (get_client_vote_locked(chg->usb_icl_votable, USER_VOTER) == 0) {
		val->intval = false;
		return rc;
	}

	rc = smblite_lib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
			rc);
		return rc;
	}
	smblite_lib_dbg(chg, PR_REGISTER, "POWER_PATH_STATUS = 0x%02x\n",
		   stat);

	val->intval = (stat & USE_USBIN_BIT) &&
		      (stat & VALID_INPUT_POWER_SOURCE_STS_BIT);
	return rc;
}

int smblite_lib_get_usb_online(struct smb_charger *chg,
			union power_supply_propval *val)
{
	int rc;

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		val->intval = 0;
		return 0;
	}

	rc = smblite_lib_get_prop_usb_online(chg, val);
	if (!val->intval)
		goto exit;

exit:
	return rc;
}

static int smblite_lib_read_usbin_voltage_chan(struct smb_charger *chg,
				     union power_supply_propval *val)
{
	int rc;

	if (!chg->iio.usbin_v_chan)
		return -ENODATA;

	rc = iio_read_channel_processed(chg->iio.usbin_v_chan, &val->intval);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read USBIN channel rc=%d\n", rc);
		return rc;
	}

	return 0;
}

int smblite_lib_get_prop_usb_voltage_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	int rc = 0;
	u8 reg;

	rc = smblite_lib_get_prop_usb_present(chg, &pval);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get usb presence status rc=%d\n",
					rc);
		goto out;
	}

	/*
	 * Skip reading voltage only if USB is not present and we are not in
	 * OTG mode.
	 */
	if (!pval.intval) {
		rc = smblite_lib_read(chg, DCDC_CMD_OTG_REG, &reg);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't read CMD_OTG rc=%d", rc);
			goto out;
		}

		if (!(reg & OTG_EN_BIT))
			goto out;
	}

	rc = smblite_lib_read_usbin_voltage_chan(chg, val);
	if (rc < 0)
		smblite_lib_err(chg, "Couldn't to read USBIN over vadc, rc=%d\n",
					rc);

out:
	return rc;
}

int smblite_lib_get_prop_charger_temp(struct smb_charger *chg,
				 union power_supply_propval *val)
{
	int temp, rc;
	int input_present;

	rc = smblite_lib_is_input_present(chg, &input_present);
	if (rc < 0)
		return rc;

	if (input_present == INPUT_NOT_PRESENT)
		return -ENODATA;

	if (chg->iio.temp_chan) {
		rc = iio_read_channel_processed(chg->iio.temp_chan,
				&temp);
		if (rc < 0) {
			pr_err("Error in reading temp channel, rc=%d\n", rc);
			return rc;
		}
		val->intval = temp / 100;
	} else {
		return -ENODATA;
	}

	return rc;
}

int smblite_lib_get_prop_typec_cc_orientation(struct smb_charger *chg,
					 union power_supply_propval *val)
{
	int rc = 0;
	u8 stat;

	val->intval = 0;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	rc = smblite_lib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n",
					rc);
		return rc;
	}
	smblite_lib_dbg(chg, PR_REGISTER, "TYPE_C_STATUS_4 = 0x%02x\n", stat);

	if (stat & CC_ATTACHED_BIT)
		val->intval = (bool)(stat & CC_ORIENTATION_BIT) + 1;

	return rc;
}

static const char * const smblite_lib_typec_mode_name[] = {
	[POWER_SUPPLY_TYPEC_NONE]		  = "NONE",
	[POWER_SUPPLY_TYPEC_SOURCE_DEFAULT]	  = "SOURCE_DEFAULT",
	[POWER_SUPPLY_TYPEC_SOURCE_MEDIUM]	  = "SOURCE_MEDIUM",
	[POWER_SUPPLY_TYPEC_SOURCE_HIGH]	  = "SOURCE_HIGH",
	[POWER_SUPPLY_TYPEC_NON_COMPLIANT]	  = "NON_COMPLIANT",
	[POWER_SUPPLY_TYPEC_SINK]		  = "SINK",
	[POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE]   = "SINK_POWERED_CABLE",
	[POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY] = "SINK_DEBUG_ACCESSORY",
	[POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER]   = "SINK_AUDIO_ADAPTER",
	[POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY]   = "POWERED_CABLE_ONLY",
};

static int smblite_lib_get_prop_ufp_mode(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	rc = smblite_lib_read(chg, TYPE_C_SNK_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read TYPE_C_STATUS_1 rc=%d\n",
					rc);
		return POWER_SUPPLY_TYPEC_NONE;
	}
	smblite_lib_dbg(chg, PR_REGISTER, "TYPE_C_STATUS_1 = 0x%02x\n", stat);

	switch (stat & DETECTED_SRC_TYPE_MASK) {
	case SNK_RP_STD_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	case SNK_RP_1P5_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case SNK_RP_3P0_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	case SNK_RP_SHORT_BIT:
		return POWER_SUPPLY_TYPEC_NON_COMPLIANT;
	case SNK_DAM_500MA_BIT:
	case SNK_DAM_1500MA_BIT:
	case SNK_DAM_3000MA_BIT:
		return POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int smblite_lib_get_prop_dfp_mode(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	rc = smblite_lib_read(chg, TYPE_C_SRC_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read TYPE_C_SRC_STATUS_REG rc=%d\n",
				rc);
		return POWER_SUPPLY_TYPEC_NONE;
	}
	smblite_lib_dbg(chg, PR_REGISTER, "TYPE_C_SRC_STATUS_REG = 0x%02x\n",
				stat);

	switch (stat & DETECTED_SNK_TYPE_MASK) {
	case AUDIO_ACCESS_RA_RA_BIT:
		return POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER;
	case SRC_DEBUG_ACCESS_BIT:
		return POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY;
	case SRC_RD_OPEN_BIT:
		return POWER_SUPPLY_TYPEC_SINK;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int smblite_lib_get_prop_typec_mode(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return POWER_SUPPLY_TYPEC_NONE;

	rc = smblite_lib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read TYPE_C_MISC_STATUS_REG rc=%d\n",
				rc);
		return 0;
	}
	smblite_lib_dbg(chg, PR_REGISTER, "TYPE_C_MISC_STATUS_REG = 0x%02x\n",
				stat);

	if (stat & SNK_SRC_MODE_BIT)
		return smblite_lib_get_prop_dfp_mode(chg);
	else
		return smblite_lib_get_prop_ufp_mode(chg);
}

inline int smblite_lib_get_usb_prop_typec_mode(struct smb_charger *chg,
				union power_supply_propval *val)
{
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		val->intval = POWER_SUPPLY_TYPEC_NONE;
	else
		val->intval = chg->typec_mode;

	return 0;
}

int smblite_lib_get_prop_typec_power_role(struct smb_charger *chg,
				     union power_supply_propval *val)
{
	int rc = 0;
	u8 ctrl;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		return 0;
	}

	rc = smblite_lib_read(chg, TYPE_C_MODE_CFG_REG, &ctrl);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read TYPE_C_MODE_CFG_REG rc=%d\n",
			rc);
		return rc;
	}
	smblite_lib_dbg(chg, PR_REGISTER, "TYPE_C_MODE_CFG_REG = 0x%02x\n",
		   ctrl);

	if (ctrl & TYPEC_DISABLE_CMD_BIT) {
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		return rc;
	}

	switch (ctrl & (EN_SRC_ONLY_BIT | EN_SNK_ONLY_BIT)) {
	case 0:
		val->intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		break;
	case EN_SRC_ONLY_BIT:
		val->intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
		break;
	case EN_SNK_ONLY_BIT:
		val->intval = POWER_SUPPLY_TYPEC_PR_SINK;
		break;
	default:
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		smblite_lib_err(chg, "unsupported power role 0x%02lx\n",
			ctrl & (EN_SRC_ONLY_BIT | EN_SNK_ONLY_BIT));
		return -EINVAL;
	}

	chg->power_role = val->intval;
	return rc;
}

static inline bool typec_in_src_mode(struct smb_charger *chg)
{
	return (chg->typec_mode > POWER_SUPPLY_TYPEC_NONE &&
		chg->typec_mode < POWER_SUPPLY_TYPEC_SOURCE_DEFAULT);
}

int smblite_lib_get_prop_input_current_settled(struct smb_charger *chg,
					  union power_supply_propval *val)
{
	return smblite_lib_get_charge_param(chg, &chg->param.icl_stat,
						&val->intval);
}

int smblite_lib_get_prop_input_voltage_settled(struct smb_charger *chg,
						union power_supply_propval *val)
{
	/* TODO: do we need to read the real VBUS */
	val->intval = 5000000;
	return 0;
}

int smblite_lib_get_prop_die_health(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	int input_present;

	rc = smblite_lib_is_input_present(chg, &input_present);
	if (rc < 0)
		return rc;

	if (input_present == INPUT_NOT_PRESENT)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	rc = smblite_lib_read(chg, DIE_TEMP_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read DIE_TEMP_STATUS_REG, rc=%d\n",
				rc);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	if (stat & DIE_TEMP_RST_BIT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	if (stat & DIE_TEMP_UB_BIT)
		return POWER_SUPPLY_HEALTH_HOT;

	if (stat & DIE_TEMP_LB_BIT)
		return POWER_SUPPLY_HEALTH_WARM;

	return POWER_SUPPLY_HEALTH_COOL;
}

int smblite_lib_get_die_health(struct smb_charger *chg,
			union power_supply_propval *val)
{
	val->intval = smblite_lib_get_prop_die_health(chg);
	return 0;
}

int smblite_lib_get_prop_scope(struct smb_charger *chg,
			union power_supply_propval *val)
{
	int rc;
	union power_supply_propval pval;

	val->intval = POWER_SUPPLY_SCOPE_UNKNOWN;
	rc = smblite_lib_get_prop_usb_present(chg, &pval);
	if (rc < 0)
		return rc;

	val->intval = pval.intval ? POWER_SUPPLY_SCOPE_DEVICE
		: chg->otg_present ? POWER_SUPPLY_SCOPE_SYSTEM
		: POWER_SUPPLY_SCOPE_UNKNOWN;

	return 0;
}

static int get_rp_based_dcp_current(struct smb_charger *chg, int typec_mode)
{
	int rp_ua;

	switch (typec_mode) {
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		rp_ua = TYPEC_HIGH_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
	/* fall through */
	default:
		rp_ua = DCP_CURRENT_UA;
	}

	return rp_ua;
}

/*******************
 * USB PSY SETTERS *
 * *****************/

int smblite_lib_set_prop_usb_type(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	smblite_lib_dbg(chg, PR_MISC,
		"Charger type request form USB driver type=%d\n", val->intval);
	/* update real charger type */
	smblite_lib_update_usb_type(chg, val->intval);

	/* For SDP rely on USB enumeration based reported the current */
	if ((chg->real_charger_type == POWER_SUPPLY_TYPE_USB)
		|| (chg->real_charger_type == POWER_SUPPLY_TYPE_UNKNOWN))
		return 0;

	vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	update_sw_icl_max(chg, chg->real_charger_type);

	power_supply_changed(chg->usb_psy);
	return 0;
}

int smblite_lib_set_prop_current_max(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;

	smblite_lib_dbg(chg, PR_MISC,
		"Current request from USB driver current=%dmA\n", val->intval);
	/* ignore current request from USB for charger other than SDP */
	if (chg->real_charger_type != POWER_SUPPLY_TYPE_USB)
		return 0;

	if (is_flashlite_active(chg) && (val->intval >= USBIN_400UA)) {
		/* For Uusb based SDP port */
		vote(chg->usb_icl_votable, FLASH_ACTIVE_VOTER, true,
				val->intval - USBIN_300UA);
		smblite_lib_dbg(chg, PR_MISC, "flash_active = 1, ICL set to  %d\n",
						val->intval - USBIN_300UA);
	}

	rc = vote(chg->usb_icl_votable, USB_PSY_VOTER, true, val->intval);
	if (rc < 0) {
		pr_err("Couldn't vote ICL USB_PSY_VOTER rc=%d\n", rc);
		return rc;
	}

	rc = vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
	if (rc < 0) {
		pr_err("Couldn't remove SW_ICL_MAX vote rc=%d\n", rc);
		return rc;
	}

	/* Update TypeC Rp based current */
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC) {
		update_sw_icl_max(chg, chg->real_charger_type);
	}

	return 0;
}

int smblite_lib_set_prop_typec_power_role(struct smb_charger *chg,
				     const union power_supply_propval *val)
{
	int rc = 0;
	u8 power_role;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return -EINVAL;

	smblite_lib_dbg(chg, PR_MISC, "power role change: %d --> %d!",
			chg->power_role, val->intval);

	if (chg->power_role == val->intval) {
		smblite_lib_dbg(chg, PR_MISC, "power role already in %d, ignore!",
				chg->power_role);
		return 0;
	}

	switch (val->intval) {
	case POWER_SUPPLY_TYPEC_PR_NONE:
		power_role = TYPEC_DISABLE_CMD_BIT;
		break;
	case POWER_SUPPLY_TYPEC_PR_DUAL:
		power_role = 0;
		break;
	case POWER_SUPPLY_TYPEC_PR_SINK:
		power_role = EN_SNK_ONLY_BIT;
		break;
	case POWER_SUPPLY_TYPEC_PR_SOURCE:
		power_role = EN_SRC_ONLY_BIT;
		break;
	default:
		smblite_lib_err(chg, "power role %d not supported\n",
					val->intval);
		return -EINVAL;
	}

	rc = smblite_lib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				TYPEC_POWER_ROLE_CMD_MASK | TYPEC_TRY_MODE_MASK,
				power_role);
	if (rc < 0) {
		smblite_lib_err(chg,
			"Couldn't write 0x%02x to TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
			power_role, rc);
		return rc;
	}

	chg->power_role = val->intval;
	return rc;
}

int smblite_lib_set_prop_ship_mode(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc;

	smblite_lib_dbg(chg, PR_MISC, "Set ship mode: %d!!\n", !!val->intval);

	rc = smblite_lib_masked_write(chg, SHIP_MODE_REG, SHIP_MODE_EN_BIT,
			!!val->intval ? SHIP_MODE_EN_BIT : 0);
	if (rc < 0)
		dev_err(chg->dev, "Couldn't %s ship mode, rc=%d\n",
				!!val->intval ? "enable" : "disable", rc);

	return rc;
}

#define JEITA_SOFT			0
#define JEITA_HARD			1
static int smblite_lib_update_jeita(struct smb_charger *chg, u32 *thresholds,
								int type)
{
	int rc;
	u16 temp, base_low, base_high;


	base_low = (type == JEITA_SOFT) ? CHGR_JEITA_COOL_THRESHOLD_REG
					: CHGR_JEITA_COLD_THRESHOLD_REG;
	base_high = (type == JEITA_SOFT) ? CHGR_JEITA_WARM_THRESHOLD_REG
					: CHGR_JEITA_HOT_THRESHOLD_REG;
	temp = thresholds[1] & 0xFFFF;
	temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
	rc = smblite_lib_batch_write(chg, base_high, (u8 *)&temp, 2);
	if (rc < 0) {
		smblite_lib_err(chg,
			"Couldn't configure Jeita %s hot threshold rc=%d\n",
			(type == JEITA_SOFT) ? "Soft" : "Hard", rc);
		return rc;
	}

	temp = thresholds[0] & 0xFFFF;
	temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
	rc = smblite_lib_batch_write(chg, base_low, (u8 *)&temp, 2);
	if (rc < 0) {
		smblite_lib_err(chg,
			"Couldn't configure Jeita %s cold threshold rc=%d\n",
			(type == JEITA_SOFT) ? "Soft" : "Hard", rc);
		return rc;
	}

	smblite_lib_dbg(chg, PR_MISC, "%s Jeita threshold configured\n",
				(type == JEITA_SOFT) ? "Soft" : "Hard");

	return 0;
}

static int smblite_lib_charge_inhibit_en(struct smb_charger *chg, bool enable)
{
	int rc;

	rc = smblite_lib_masked_write(chg, CHGR_INHIBIT_REG,
					CHGR_INHIBIT_BIT,
					enable ? CHGR_INHIBIT_BIT : 0);
	return rc;
}

static int smblite_lib_soft_jeita_arb_wa(struct smb_charger *chg)
{
	union power_supply_propval pval;
	int rc = 0;
	bool soft_jeita;

	rc = smblite_lib_get_prop_batt_health(chg, &pval);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get battery health rc=%d\n", rc);
		return rc;
	}

	/* Do nothing on entering hard JEITA condition */
	if (pval.intval == POWER_SUPPLY_HEALTH_COLD ||
		pval.intval == POWER_SUPPLY_HEALTH_HOT)
		return 0;

	if (chg->jeita_soft_fcc[0] < 0 || chg->jeita_soft_fcc[1] < 0 ||
		chg->jeita_soft_fv[0] < 0 || chg->jeita_soft_fv[1] < 0)
		return 0;

	soft_jeita = (pval.intval == POWER_SUPPLY_HEALTH_COOL) ||
			(pval.intval == POWER_SUPPLY_HEALTH_WARM);

	/* Do nothing on entering soft JEITA from hard JEITA */
	if (chg->jeita_arb_flag && soft_jeita)
		return 0;

	/* Do nothing, initial to health condition */
	if (!chg->jeita_arb_flag && !soft_jeita)
		return 0;

	/* Entering soft JEITA from normal state */
	if (!chg->jeita_arb_flag && soft_jeita) {
		vote(chg->chg_disable_votable, JEITA_ARB_VOTER, true, 0);

		rc = smblite_lib_charge_inhibit_en(chg, true);
		if (rc < 0)
			smblite_lib_err(chg, "Couldn't enable charge inhibit rc=%d\n",
					rc);

		rc = smblite_lib_update_jeita(chg, chg->jeita_soft_hys_thlds,
					JEITA_SOFT);
		if (rc < 0)
			smblite_lib_err(chg,
				"Couldn't configure Jeita soft threshold rc=%d\n",
				rc);

		if (pval.intval == POWER_SUPPLY_HEALTH_COOL) {
			vote(chg->fcc_votable, JEITA_ARB_VOTER, true,
						chg->jeita_soft_fcc[0]);
			vote(chg->fv_votable, JEITA_ARB_VOTER, true,
						chg->jeita_soft_fv[0]);
		} else {
			vote(chg->fcc_votable, JEITA_ARB_VOTER, true,
						chg->jeita_soft_fcc[1]);
			vote(chg->fv_votable, JEITA_ARB_VOTER, true,
						chg->jeita_soft_fv[1]);
		}

		vote(chg->chg_disable_votable, JEITA_ARB_VOTER, false, 0);
		chg->jeita_arb_flag = true;
	} else if (chg->jeita_arb_flag && !soft_jeita) {
		/* Exit to health state from soft JEITA */

		vote(chg->chg_disable_votable, JEITA_ARB_VOTER, true, 0);

		rc = smblite_lib_charge_inhibit_en(chg, false);
		if (rc < 0)
			smblite_lib_err(chg, "Couldn't disable charge inhibit rc=%d\n",
					rc);

		rc = smblite_lib_update_jeita(chg, chg->jeita_soft_thlds,
							JEITA_SOFT);
		if (rc < 0)
			smblite_lib_err(chg, "Couldn't configure Jeita soft threshold rc=%d\n",
				rc);

		vote(chg->fcc_votable, JEITA_ARB_VOTER, false, 0);
		vote(chg->fv_votable, JEITA_ARB_VOTER, false, 0);
		vote(chg->chg_disable_votable, JEITA_ARB_VOTER, false, 0);
		chg->jeita_arb_flag = false;
	}

	smblite_lib_dbg(chg, PR_MISC, "JEITA ARB status %d, soft JEITA status %d\n",
			chg->jeita_arb_flag, soft_jeita);
	return rc;
}

/************************
 * USB MAIN PSY SETTERS *
 ************************/
int smblite_lib_get_hw_current_max(struct smb_charger *chg,
					int *total_current_ua)
{
	union power_supply_propval val = {0, };
	int rc = 0, typec_source_rd, current_ua;
	bool non_compliant;
	u8 stat;

	rc = smblite_lib_read(chg, LEGACY_CABLE_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read TYPE_C_STATUS_5 rc=%d\n",
					rc);
		return rc;
	}
	non_compliant = stat & TYPEC_NONCOMP_LEGACY_CABLE_STATUS_BIT;

	/* get settled ICL */
	rc = smblite_lib_get_prop_input_current_settled(chg, &val);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get settled ICL rc=%d\n", rc);
		return rc;
	}

	typec_source_rd = smblite_lib_get_prop_ufp_mode(chg);

	if ((chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		|| (non_compliant && !chg->typec_legacy_use_rp_icl)) {
		switch (chg->real_charger_type) {
		case POWER_SUPPLY_TYPE_USB_CDP:
			current_ua = CDP_CURRENT_UA;
			break;
		case POWER_SUPPLY_TYPE_USB_DCP:
			current_ua = DCP_CURRENT_UA;
			break;
		default:
			current_ua = 0;
			break;
		}

		*total_current_ua = max(current_ua, val.intval);
		return 0;
	}

	switch (typec_source_rd) {
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
		switch (chg->real_charger_type) {
		case POWER_SUPPLY_TYPE_USB_CDP:
			current_ua = CDP_CURRENT_UA;
			break;
		case POWER_SUPPLY_TYPE_USB_DCP:
			current_ua = DCP_CURRENT_UA;
			break;
		default:
			current_ua = 0;
			break;
		}
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
		current_ua = TYPEC_MEDIUM_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		current_ua = TYPEC_HIGH_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_NON_COMPLIANT:
	case POWER_SUPPLY_TYPEC_NONE:
	default:
		current_ua = 0;
		break;
	}

	*total_current_ua = max(current_ua, val.intval);
	return 0;
}

int smblite_lib_get_charge_current(struct smb_charger *chg,
					int *total_current_ua)
{
	if (chg->usb_icl_votable)
		*total_current_ua = get_effective_result(chg->usb_icl_votable);

	return 0;
}

/**********************
 * INTERRUPT HANDLERS *
 **********************/

irqreturn_t smblite_default_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	return IRQ_HANDLED;
}

irqreturn_t smblite_chg_state_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	u8 stat;
	int rc;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	rc = smblite_lib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
				rc);
		return IRQ_HANDLED;
	}

	stat = stat & BATTERY_CHARGER_STATUS_MASK;

	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

irqreturn_t smblite_batt_temp_changed_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	if (chg->jeita_configured != JEITA_CFG_COMPLETE)
		return IRQ_HANDLED;

	rc = smblite_lib_soft_jeita_arb_wa(chg);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't fix soft jeita arb rc=%d\n",
				rc);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

irqreturn_t smblite_batt_psy_changed_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

#define AICL_STEP_MV		200
#define MAX_AICL_THRESHOLD_MV	4800
irqreturn_t smblite_usbin_uv_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	struct storm_watch *wdata;
	int rc;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	if ((chg->wa_flags & WEAK_ADAPTER_WA)
			&& is_storming(&irq_data->storm_data)) {

		if (chg->aicl_max_reached) {
			smblite_lib_dbg(chg, PR_MISC,
					"USBIN_UV storm at max AICL threshold\n");
			return IRQ_HANDLED;
		}

		smblite_lib_dbg(chg, PR_MISC, "USBIN_UV storm at threshold %d\n",
				chg->aicl_5v_threshold_mv);

		/* suspend USBIN before updating AICL threshold */
		vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER, true, 0);

		/* delay for VASHDN deglitch */
		msleep(20);

		if (chg->aicl_5v_threshold_mv > MAX_AICL_THRESHOLD_MV) {
			/* reached max AICL threshold */
			chg->aicl_max_reached = true;
			goto unsuspend_input;
		}

		/* Increase AICL threshold by 200mV */
		rc = smblite_lib_set_charge_param(chg,
				&chg->param.aicl_5v_threshold,
				chg->aicl_5v_threshold_mv + AICL_STEP_MV);
		if (rc < 0)
			dev_err(chg->dev,
				"Error in setting AICL threshold rc=%d\n", rc);
		else
			chg->aicl_5v_threshold_mv += AICL_STEP_MV;

unsuspend_input:
		/* Force torch in boost mode to ensure it works with low ICL */
		schgm_flashlite_torch_priority(chg, TORCH_BOOST_MODE);

		if (chg->aicl_max_reached) {
			smblite_lib_dbg(chg, PR_MISC,
				"Reached max AICL threshold resctricting ICL to 100mA\n");
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					true, USBIN_100UA);
			smblite_lib_run_aicl(chg, RESTART_AICL);
		} else {
			smblite_lib_run_aicl(chg, RESTART_AICL);
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					false, 0);
		}

		wdata = &chg->irq_info[USBIN_UV_IRQ].irq_data->storm_data;
		reset_storm_count(wdata);
	}

	if (!chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data)
		return IRQ_HANDLED;

	wdata = &chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data->storm_data;
	reset_storm_count(wdata);

	return IRQ_HANDLED;
}

#define USB_WEAK_INPUT_UA	1400000
#define ICL_CHANGE_DELAY_MS	1000
irqreturn_t smblite_icl_change_irq_handler(int irq, void *data)
{
	int rc, settled_ua, delay = ICL_CHANGE_DELAY_MS;
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	if (chg->mode == PARALLEL_MASTER) {
		rc = smblite_lib_get_charge_param(chg, &chg->param.icl_stat,
					&settled_ua);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't get ICL status rc=%d\n",
						rc);
			return IRQ_HANDLED;
		}

		/* If AICL settled then schedule work now */
		if (settled_ua == get_effective_result(chg->usb_icl_votable))
			delay = 0;

		cancel_delayed_work_sync(&chg->icl_change_work);
		schedule_delayed_work(&chg->icl_change_work,
						msecs_to_jiffies(delay));
	}

	return IRQ_HANDLED;
}

static int smblite_lib_role_switch_failure(struct smb_charger *chg)
{
	int rc = 0;
	union power_supply_propval pval = {0, };

	rc = smblite_lib_get_prop_usb_present(chg, &pval);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get usb presence status rc=%d\n",
					rc);
		return rc;
	}

	/*
	 * When role switch fails notify the
	 * current charger state to usb driver.
	 */
	if (pval.intval) {
		smblite_lib_dbg(chg, PR_MISC, "Role reversal failed, notifying device mode to usb driver.\n");
		smblite_lib_notify_device_mode(chg, true);
	}

	return rc;
}

static int typec_partner_register(struct smb_charger *chg)
{
	int typec_mode, rc = 0;

	mutex_lock(&chg->typec_lock);

	if (!chg->typec_port || chg->pr_swap_in_progress)
		goto unlock;

	if (!chg->typec_partner) {
		if (chg->sink_src_mode == AUDIO_ACCESS_MODE)
			chg->typec_partner_desc.accessory =
					TYPEC_ACCESSORY_AUDIO;
		else
			chg->typec_partner_desc.accessory =
					TYPEC_ACCESSORY_NONE;

		chg->typec_partner = typec_register_partner(chg->typec_port,
				&chg->typec_partner_desc);
		if (IS_ERR(chg->typec_partner)) {
			rc = PTR_ERR(chg->typec_partner);
			pr_err("Couldn't to register typec_partner rc=%d\n",
								rc);
			goto unlock;
		}
	}

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		goto unlock;

	typec_mode = smblite_lib_get_prop_typec_mode(chg);

	if (typec_mode >= POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
			|| typec_mode == POWER_SUPPLY_TYPEC_NONE) {
		if (chg->typec_role_swap_failed) {
			rc = smblite_lib_role_switch_failure(chg);
			if (rc < 0)
				smblite_lib_err(chg, "Failed to role switch rc=%d\n",
								rc);
			chg->typec_role_swap_failed = false;
		}

		typec_set_data_role(chg->typec_port, TYPEC_DEVICE);
		typec_set_pwr_role(chg->typec_port, TYPEC_SINK);
	} else {
		typec_set_data_role(chg->typec_port, TYPEC_HOST);
		typec_set_pwr_role(chg->typec_port, TYPEC_SOURCE);
	}

unlock:
	mutex_unlock(&chg->typec_lock);
	return rc;
}

static void typec_partner_unregister(struct smb_charger *chg)
{
	mutex_lock(&chg->typec_lock);

	if (!chg->typec_port)
		goto unlock;

	if (chg->typec_partner && !chg->pr_swap_in_progress) {
		smblite_lib_dbg(chg, PR_MISC, "Un-registering typeC partner\n");
		typec_unregister_partner(chg->typec_partner);
		chg->typec_partner = NULL;
	}

unlock:
	mutex_unlock(&chg->typec_lock);
}

static void smblite_lib_micro_usb_plugin(struct smb_charger *chg,
					bool vbus_rising)
{
	int rc = 0;

	if (vbus_rising) {
		smblite_lib_notify_device_mode(chg, true);
		rc = typec_partner_register(chg);
		if (rc < 0)
			smblite_lib_err(chg, "Couldn't register partner rc =%d\n",
					rc);
	} else {
		smblite_lib_notify_device_mode(chg, false);
		smblite_lib_uusb_removal(chg);
		typec_partner_unregister(chg);
	}
}

#define PL_DELAY_MS	30000
void smblite_lib_usb_plugin_locked(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	bool vbus_rising;
	struct smb_irq_data *data;
	struct storm_watch *wdata;

	rc = smblite_lib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n",
					rc);
		return;
	}

	vbus_rising = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);

	if (vbus_rising) {
		/* Remove FCC_STEPPER 1.5A init vote to allow FCC ramp up */
		if (chg->fcc_stepper_enable)
			vote(chg->fcc_votable, FCC_STEPPER_VOTER, false, 0);

		/* Schedule work to enable parallel charger */
		vote(chg->awake_votable, PL_DELAY_VOTER, true, 0);
		schedule_delayed_work(&chg->pl_enable_work,
					msecs_to_jiffies(PL_DELAY_MS));
	} else {
		smblite_lib_update_usb_type(chg, POWER_SUPPLY_TYPE_UNKNOWN);
		/* Disable SW Thermal Regulation */
		if (chg->wa_flags & BOOST_BACK_WA) {
			data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
			if (data) {
				wdata = &data->storm_data;
				update_storm_count(wdata,
						WEAK_CHG_STORM_COUNT);
				vote(chg->usb_icl_votable, BOOST_BACK_VOTER,
						false, 0);
				vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
						false, 0);
			}
		}

		/* Force 1500mA FCC on removal if fcc stepper is enabled */
		if (chg->fcc_stepper_enable)
			vote(chg->fcc_votable, FCC_STEPPER_VOTER,
							true, 1500000);

		if (chg->wa_flags & WEAK_ADAPTER_WA) {
			chg->aicl_5v_threshold_mv =
					chg->default_aicl_5v_threshold_mv;

			smblite_lib_set_charge_param(chg,
					&chg->param.aicl_5v_threshold,
					chg->aicl_5v_threshold_mv);
			chg->aicl_max_reached = false;

			/*
			 * schgm_flash_torch_priority(chg, TORCH_BUCK_MODE);
			 */

			data = chg->irq_info[USBIN_UV_IRQ].irq_data;
			if (data) {
				wdata = &data->storm_data;
				reset_storm_count(wdata);
			}
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					false, 0);
		}
	}

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		smblite_lib_micro_usb_plugin(chg, vbus_rising);

	vote(chg->temp_change_irq_disable_votable, DEFAULT_VOTER,
						!vbus_rising, 0);
	power_supply_changed(chg->usb_psy);
	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: usbin-plugin %s\n",
					vbus_rising ? "attached" : "detached");
}

irqreturn_t smblite_usb_plugin_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblite_lib_usb_plugin_locked(chg);

	return IRQ_HANDLED;
}

static void update_sw_icl_max(struct smb_charger *chg,
				enum power_supply_type type)
{
	int typec_mode;
	int rp_ua, icl_ua;

	if (chg->typec_mode == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, 500000);
		return;
	}

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					USBIN_100UA);
		return;
	}

	/* TypeC rp med or high, use rp value */
	typec_mode = smblite_lib_get_prop_typec_mode(chg);
	if (typec_rp_med_high(chg, typec_mode)) {
		rp_ua = get_rp_based_dcp_current(chg, typec_mode);
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, rp_ua);
		vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
		return;
	}

	/* rp-std or legacy, USB BC 1.2 */
	switch (type) {
	case POWER_SUPPLY_TYPE_USB:
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		icl_ua = CDP_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		icl_ua = get_rp_based_dcp_current(chg, typec_mode);
		break;
	case POWER_SUPPLY_TYPE_UNKNOWN:
	default:
		icl_ua = USBIN_100UA;
		break;
	}

	if (is_flashlite_active(chg) && type != POWER_SUPPLY_TYPE_USB) {
		vote(chg->usb_icl_votable, FLASH_ACTIVE_VOTER, true,
				icl_ua - USBIN_300UA);
		smblite_lib_dbg(chg, PR_MISC, "flash_active = 1 ICL is set to %d\n",
					icl_ua - USBIN_300UA);
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, icl_ua);
	}

}

static void typec_sink_insertion(struct smb_charger *chg)
{
	smblite_lib_notify_usb_host(chg, true);
}

static void typec_src_insertion(struct smb_charger *chg)
{
	int rc = 0;
	u8 stat;

	smblite_lib_notify_device_mode(chg, true);
	if (chg->pr_swap_in_progress) {
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		return;
	}

	rc = smblite_lib_read(chg, LEGACY_CABLE_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read TYPE_C_STATE_MACHINE_STATUS_REG rc=%d\n",
					rc);
		return;
	}

	chg->typec_legacy = stat & TYPEC_LEGACY_CABLE_STATUS_BIT;

}

static void typec_ra_ra_insertion(struct smb_charger *chg)
{
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, 500000);
	vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
}

static const char * const dr_mode_text[] = {
	"ufp", "dfp", "none"
};

static int smblite_lib_force_dr_mode(struct smb_charger *chg, int mode)
{
	int rc = 0;

	switch (mode) {
	case TYPEC_PORT_SNK:
		rc = smblite_lib_masked_write(chg, TYPE_C_MODE_CFG_REG,
			TYPEC_POWER_ROLE_CMD_MASK, EN_SNK_ONLY_BIT);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't enable snk, rc=%d\n",
						rc);
			return rc;
		}
		break;
	case TYPEC_PORT_SRC:
		rc = smblite_lib_masked_write(chg, TYPE_C_MODE_CFG_REG,
			TYPEC_POWER_ROLE_CMD_MASK, EN_SRC_ONLY_BIT);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't enable src, rc=%d\n",
						rc);
			return rc;
		}
		break;
	case TYPEC_PORT_DRP:
		rc = smblite_lib_masked_write(chg, TYPE_C_MODE_CFG_REG,
			TYPEC_POWER_ROLE_CMD_MASK, 0);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't enable DRP, rc=%d\n",
						rc);
			return rc;
		}
		break;
	default:
		smblite_lib_err(chg, "Power role %d not supported\n", mode);
		return -EINVAL;
	}

	chg->dr_mode = mode;

	return rc;
}

int smblite_lib_typec_port_type_set(const struct typec_capability *cap,
					enum typec_port_type type)
{
	struct smb_charger *chg = container_of(cap,
					struct smb_charger, typec_caps);
	int rc = 0;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	mutex_lock(&chg->typec_lock);

	if ((chg->pr_swap_in_progress) || (type == TYPEC_PORT_DRP)) {
		smblite_lib_dbg(chg, PR_MISC, "Ignoring port type request type = %d swap_in_progress = %d\n",
				type, chg->pr_swap_in_progress);
		goto unlock;
	}

	chg->pr_swap_in_progress = true;

	rc = smblite_lib_force_dr_mode(chg, type);
	if (rc < 0) {
		chg->pr_swap_in_progress = false;
		smblite_lib_err(chg, "Couldn't to force mode, rc=%d\n", rc);
		goto unlock;
	}

	smblite_lib_dbg(chg, PR_MISC, "Requested role %s\n",
				type ? "SINK" : "SOURCE");

	/*
	 * As per the hardware requirements,
	 * schedule the work with required delay.
	 */
	if (!(delayed_work_pending(&chg->role_reversal_check))) {
		cancel_delayed_work_sync(&chg->role_reversal_check);
		schedule_delayed_work(&chg->role_reversal_check,
			msecs_to_jiffies(ROLE_REVERSAL_DELAY_MS));
		vote(chg->awake_votable, TYPEC_SWAP_VOTER, true, 0);
	}

unlock:
	mutex_unlock(&chg->typec_lock);
	return rc;
}

static void smblite_lib_typec_role_check_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
					role_reversal_check.work);
	int rc = 0;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		chg->pr_swap_in_progress = false;
		vote(chg->awake_votable, TYPEC_SWAP_VOTER, false, 0);
		return;
	}

	mutex_lock(&chg->typec_lock);

	switch (chg->dr_mode) {
	case TYPEC_PORT_SNK:
		if (chg->typec_mode < POWER_SUPPLY_TYPEC_SOURCE_DEFAULT) {
			smblite_lib_dbg(chg, PR_MISC, "Role reversal not latched to UFP in %d msecs. Resetting to DRP mode\n",
						ROLE_REVERSAL_DELAY_MS);
			rc = smblite_lib_force_dr_mode(chg, TYPEC_PORT_DRP);
			if (rc < 0)
				smblite_lib_err(chg, "Couldn't to set DRP mode, rc=%d\n",
						rc);
		} else {
			chg->power_role = POWER_SUPPLY_TYPEC_PR_SINK;
			typec_set_pwr_role(chg->typec_port, TYPEC_SINK);
			typec_set_data_role(chg->typec_port, TYPEC_DEVICE);
			smblite_lib_dbg(chg, PR_MISC, "Role changed successfully to SINK");
		}
		break;
	case TYPEC_PORT_SRC:
		if (chg->typec_mode >= POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
			|| chg->typec_mode == POWER_SUPPLY_TYPEC_NONE) {
			smblite_lib_dbg(chg, PR_MISC, "Role reversal not latched to DFP in %d msecs. Resetting to DRP mode\n",
						ROLE_REVERSAL_DELAY_MS);
			chg->pr_swap_in_progress = false;
			chg->typec_role_swap_failed = true;
			rc = smblite_lib_force_dr_mode(chg,
							TYPEC_PORT_DRP);
			if (rc < 0)
				smblite_lib_err(chg, "Couldn't to set DRP mode, rc=%d\n",
							rc);
		} else {
			chg->power_role = POWER_SUPPLY_TYPEC_PR_SOURCE;
			typec_set_pwr_role(chg->typec_port, TYPEC_SOURCE);
			typec_set_data_role(chg->typec_port, TYPEC_HOST);
			smblite_lib_dbg(chg, PR_MISC, "Role changed successfully to SOURCE");
		}
		break;
	default:
		pr_debug("Already in DRP mode\n");
		break;
	}

	chg->pr_swap_in_progress = false;
	vote(chg->awake_votable, TYPEC_SWAP_VOTER, false, 0);
	mutex_unlock(&chg->typec_lock);
}

static void typec_sink_removal(struct smb_charger *chg)
{
	if (chg->otg_present)
		smblite_lib_notify_usb_host(chg, false);
}

static void typec_src_removal(struct smb_charger *chg)
{
	struct smb_irq_data *data;
	struct storm_watch *wdata;

	if (chg->wa_flags & BOOST_BACK_WA) {
		data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
		if (data) {
			wdata = &data->storm_data;
			update_storm_count(wdata, WEAK_CHG_STORM_COUNT);
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					false, 0);
		}
	}

	cancel_delayed_work_sync(&chg->pl_enable_work);

	/* reset input current limit voters */
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
			is_flashlite_active(chg) ? USBIN_200UA : USBIN_100UA);
	vote(chg->usb_icl_votable, FLASH_ACTIVE_VOTER, false, 0);
	vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);

	/* reset parallel voters */
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, false, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);

	/* Remove SW thermal regulation votes */
	vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER, false, 0);
	vote(chg->awake_votable, SW_THERM_REGULATION_VOTER, false, 0);

	smblite_lib_notify_device_mode(chg, false);

	chg->typec_legacy = false;
}

static void typec_mode_unattached(struct smb_charger *chg)
{
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, USBIN_100UA);
}

static void smblite_lib_handle_rp_change(struct smb_charger *chg,
					int typec_mode)
{
	/*
	 * if type is not updated or charger current is not set
	 * for SDP ignore Rp change requests.
	 */
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_UNKNOWN
		|| (chg->real_charger_type == POWER_SUPPLY_TYPE_USB
			&& !is_client_vote_enabled(chg->usb_icl_votable,
							USB_PSY_VOTER)))
		return;

	update_sw_icl_max(chg, chg->real_charger_type);
	smblite_lib_dbg(chg, PR_MISC, "CC change old_mode=%d new_mode=%d\n",
						chg->typec_mode, typec_mode);
}

irqreturn_t smblite_typec_or_rid_detection_change_irq_handler(int irq,
								void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	if (chg->usb_psy)
		power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

irqreturn_t smblite_typec_state_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int typec_mode;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		smblite_lib_dbg(chg, PR_INTERRUPT,
				"Ignoring for micro USB\n");
		return IRQ_HANDLED;
	}

	typec_mode = smblite_lib_get_prop_typec_mode(chg);
	if (chg->sink_src_mode != UNATTACHED_MODE
			&& (typec_mode != chg->typec_mode))
		smblite_lib_handle_rp_change(chg, typec_mode);
	chg->typec_mode = typec_mode;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: cc-state-change; Type-C %s detected\n",
				smblite_lib_typec_mode_name[chg->typec_mode]);

	power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

#define TYPEC_DETACH_DETECT_DELAY_MS 2000
irqreturn_t smblite_typec_attach_detach_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	u8 stat;
	bool attached = false;
	int rc;

	/* IRQ not expected to be executed for uUSB, return */
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return IRQ_HANDLED;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	rc = smblite_lib_read(chg, TYPE_C_STATE_MACHINE_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read TYPE_C_STATE_MACHINE_STATUS_REG rc=%d\n",
			rc);
		return IRQ_HANDLED;
	}

	attached = !!(stat & TYPEC_ATTACH_DETACH_STATE_BIT);

	if (attached) {
		rc = smblite_lib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't read TYPE_C_MISC_STATUS_REG rc=%d\n",
				rc);
			return IRQ_HANDLED;
		}

		if (smblite_lib_get_prop_dfp_mode(chg) ==
				POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
			chg->sink_src_mode = AUDIO_ACCESS_MODE;
			typec_ra_ra_insertion(chg);
		} else if (stat & SNK_SRC_MODE_BIT) {
			chg->sink_src_mode = SRC_MODE;
			typec_sink_insertion(chg);
		} else {
			chg->sink_src_mode = SINK_MODE;
			typec_src_insertion(chg);
		}

		rc = typec_partner_register(chg);
		if (rc < 0)
			smblite_lib_err(chg, "Couldn't to register partner rc =%d\n",
					rc);
	} else {
		switch (chg->sink_src_mode) {
		case SRC_MODE:
			typec_sink_removal(chg);
			break;
		case SINK_MODE:
		case AUDIO_ACCESS_MODE:
			typec_src_removal(chg);
			break;
		case UNATTACHED_MODE:
		default:
			typec_mode_unattached(chg);
			break;
		}

		if (!chg->pr_swap_in_progress)
			chg->sink_src_mode = UNATTACHED_MODE;

		/*
		 * Restore DRP mode on type-C cable disconnect if role
		 * swap is not in progress, to ensure forced sink or src
		 * mode configuration is reset properly.
		 */

		if (chg->typec_port && !chg->pr_swap_in_progress) {
			/*
			 * Schedule the work to differentiate actual removal
			 * of cable and detach interrupt during role swap,
			 * unregister the partner only during actual cable
			 * removal.
			 */
			cancel_delayed_work(&chg->pr_swap_detach_work);
			vote(chg->awake_votable, DETACH_DETECT_VOTER, true, 0);
			schedule_delayed_work(&chg->pr_swap_detach_work,
				msecs_to_jiffies(TYPEC_DETACH_DETECT_DELAY_MS));
			smblite_lib_force_dr_mode(chg, TYPEC_PORT_DRP);

			/*
			 * To handle cable removal during role
			 * swap failure.
			 */
			chg->typec_role_swap_failed = false;
		}
	}

	rc = smblite_lib_masked_write(chg, USB_CMD_PULLDOWN_REG,
			EN_PULLDOWN_USB_IN_BIT,
			attached ?  0 : EN_PULLDOWN_USB_IN_BIT);
	if (rc < 0)
		smblite_lib_err(chg, "Couldn't configure pulldown on USB_IN rc=%d\n",
				rc);

	power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

static void smblite_lib_bb_removal_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						bb_removal_work.work);

	vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
	vote(chg->awake_votable, BOOST_BACK_VOTER, false, 0);
}

#define BOOST_BACK_UNVOTE_DELAY_MS		750
#define BOOST_BACK_STORM_COUNT			3
#define WEAK_CHG_STORM_COUNT			8
irqreturn_t smblite_switcher_power_ok_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	struct storm_watch *wdata = &irq_data->storm_data;
	int rc, usb_icl;
	u8 stat;

	if (!(chg->wa_flags & BOOST_BACK_WA))
		return IRQ_HANDLED;

	rc = smblite_lib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
					rc);
		return IRQ_HANDLED;
	}

	/* skip suspending input if its already suspended by some other voter */
	usb_icl = get_effective_result(chg->usb_icl_votable);
	if ((stat & USE_USBIN_BIT) && usb_icl >= 0 && usb_icl <= USBIN_25UA)
		return IRQ_HANDLED;

	if (is_storming(&irq_data->storm_data)) {
		/* This could be a weak charger reduce ICL */
		if (!is_client_vote_enabled(chg->usb_icl_votable,
						WEAK_CHARGER_VOTER)) {
			smblite_lib_err(chg,
				"Weak charger detected: voting %dmA ICL\n",
				chg->weak_chg_icl_ua / 1000);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					true, chg->weak_chg_icl_ua);
			/*
			 * reset storm data and set the storm threshold
			 * to 3 for reverse boost detection.
			 */
			update_storm_count(wdata, BOOST_BACK_STORM_COUNT);
		} else {
			smblite_lib_err(chg,
				"Reverse boost detected: voting 0mA to suspend input\n");
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, true, 0);
			vote(chg->awake_votable, BOOST_BACK_VOTER, true, 0);
			/*
			 * Remove the boost-back vote after a delay, to avoid
			 * permanently suspending the input if the boost-back
			 * condition is unintentionally hit.
			 */
			schedule_delayed_work(&chg->bb_removal_work,
				msecs_to_jiffies(BOOST_BACK_UNVOTE_DELAY_MS));
		}
	}

	return IRQ_HANDLED;
}

irqreturn_t smblite_wdog_bark_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	rc = smblite_lib_write(chg, BARK_BITE_WDOG_PET_REG,
				BARK_BITE_WDOG_PET_BIT);
	if (rc < 0)
		smblite_lib_err(chg, "Couldn't pet the dog rc=%d\n", rc);

	power_supply_changed(chg->batt_psy);

	return IRQ_HANDLED;
}

/*
 * triggered when DIE temperature across
 * either of the _REG_L, _REG_H, _RST, or _SHDN thresholds
 */
#define THERM_REGULATION_DELAY_MS		1000
#define THERM_REGULATION_STEP_UA		100000
irqreturn_t smblite_temp_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	vote(chg->awake_votable, SW_THERM_REGULATION_VOTER, true, 0);
	cancel_delayed_work_sync(&chg->thermal_regulation_work);
	schedule_delayed_work(&chg->thermal_regulation_work,
				msecs_to_jiffies(THERM_REGULATION_DELAY_MS));

	return IRQ_HANDLED;
}

#define USB_OV_DBC_PERIOD_MS		1000
irqreturn_t smblite_usbin_ov_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	return IRQ_HANDLED;
}

irqreturn_t smblite_usb_id_irq_handler(int irq, void *data)
{
	struct smb_charger *chg = data;
	bool id_state;

	id_state = gpio_get_value(chg->usb_id_gpio);

	smblite_lib_dbg(chg, PR_INTERRUPT, "IRQ: %s, id_state=%d\n",
					"usb-id-irq", id_state);
	if (id_state) {
		/*otg cable removed */
		if (chg->otg_present) {
			if (chg->typec_port) {
				typec_set_data_role(chg->typec_port,
							TYPEC_DEVICE);
				typec_set_pwr_role(chg->typec_port, TYPEC_SINK);
				typec_partner_unregister(chg);
			}
		}
	} else if (chg->typec_port) {
		/*otg cable inserted */
		typec_partner_register(chg);
		typec_set_data_role(chg->typec_port, TYPEC_HOST);
		typec_set_pwr_role(chg->typec_port, TYPEC_SOURCE);
	}

	smblite_lib_notify_usb_host(chg, !id_state);

	return IRQ_HANDLED;
}

/***************
 * Work Queues *
 ***************/
static void smblite_lib_pr_swap_detach_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						pr_swap_detach_work.work);
	int rc;
	u8 stat;

	rc = smblite_lib_read(chg, TYPE_C_STATE_MACHINE_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read STATE_MACHINE_STS rc=%d\n",
								rc);
		goto out;
	}
	smblite_lib_dbg(chg, PR_REGISTER, "STATE_MACHINE_STS %#x\n", stat);

	if (!(stat & TYPEC_ATTACH_DETACH_STATE_BIT))
		typec_partner_unregister(chg);

out:
	vote(chg->awake_votable, DETACH_DETECT_VOTER, false, 0);
}

static void bms_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						bms_update_work);

	smblite_lib_suspend_on_debug_battery(chg);

	if (chg->batt_psy)
		power_supply_changed(chg->batt_psy);
}

static void smblite_lib_icl_change_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							icl_change_work.work);
	int rc, settled_ua;

	rc = smblite_lib_get_charge_param(chg, &chg->param.icl_stat,
					&settled_ua);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get ICL status rc=%d\n", rc);
		return;
	}

	power_supply_changed(chg->usb_main_psy);

	smblite_lib_dbg(chg, PR_INTERRUPT, "icl_settled=%d\n", settled_ua);
}

static void smblite_lib_pl_enable_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							pl_enable_work.work);

	smblite_lib_dbg(chg, PR_PARALLEL, "timer expired, enabling parallel\n");
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, false, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);
}

static void smblite_lib_thermal_regulation_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						thermal_regulation_work.work);
	int rc, icl_ua, input_present;
	u8 stat;


	if (!chg->usb_icl_votable)
		goto exit;

	rc = smblite_lib_is_input_present(chg, &input_present);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read input status rc=%d\n", rc);
		goto exit;
	}

	if (input_present == INPUT_NOT_PRESENT) {
		vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER, false, 0);
		goto exit;
	}

	rc = smblite_lib_read(chg, DIE_TEMP_STATUS_REG, &stat);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't read DIE_TEMP_STATUS_REG, rc=%d\n",
				rc);
		goto reschedule;
	}

	icl_ua = get_effective_result(chg->usb_icl_votable);

	if (stat & DIE_TEMP_RST_BIT) {
		vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER,
				true, USBIN_500UA);
		icl_ua = USBIN_500UA;
		goto exit;
	}

	if (stat & DIE_TEMP_UB_BIT) {
		/* Check if we reached minimum ICL limit */
		if (icl_ua < USBIN_500UA + THERM_REGULATION_STEP_UA)
			goto exit;

		/* Decrement ICL by one step */
		icl_ua -= THERM_REGULATION_STEP_UA;
		vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER,
				true, icl_ua);

		goto reschedule;
	}

	/* check if DIE_TEMP is below LB */
	if (!(stat & DIE_TEMP_MASK)) {

		/*
		 * Check if we need further increments:
		 * If thermal is still effective client then work can continue
		 * with increment otherwise if other voter has voted a lower
		 * ICL then remove vote and exit work.
		 */
		if (!strcmp(get_effective_client(chg->usb_icl_votable),
				SW_THERM_REGULATION_VOTER)) {
			icl_ua += THERM_REGULATION_STEP_UA;
			vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER,
					true, icl_ua);
			goto reschedule;
		}
	}

exit:
	smblite_lib_dbg(chg, PR_MISC,
			"exiting DIE_TEMP regulation work DIE_TEMP_STATUS=%x icl=%duA\n",
			stat, icl_ua);
	vote(chg->awake_votable, SW_THERM_REGULATION_VOTER, false, 0);
	return;

reschedule:
	smblite_lib_dbg(chg, PR_MISC,
			"rescheduling DIE_TEMP regulation work DIE_TEMP_STATUS=%x icl=%duA\n",
				stat, icl_ua);
	schedule_delayed_work(&chg->thermal_regulation_work,
				msecs_to_jiffies(THERM_REGULATION_DELAY_MS));
}

#define SOFT_JEITA_HYSTERESIS_OFFSET	0x200
static void jeita_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						jeita_update_work);
	struct device_node *node = chg->dev->of_node;
	struct device_node *batt_node, *pnode;
	union power_supply_propval val;
	int rc, tmp[2], max_fcc_ma, max_fv_uv;
	u32 jeita_hard_thresholds[2];
	u16 addr;
	u8 buff[2];

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		smblite_lib_err(chg, "Batterydata not available\n");
		goto out;
	}

	/* if BMS is not ready, defer the work */
	if (!chg->bms_psy)
		return;

	rc = smblite_lib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &val);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't to get batt-id rc=%d\n", rc);
		goto out;
	}

	/* if BMS hasn't read out the batt_id yet, defer the work */
	if (val.intval <= 0)
		return;

	pnode = of_batterydata_get_best_profile(batt_node,
					val.intval / 1000, NULL);
	if (IS_ERR(pnode)) {
		rc = PTR_ERR(pnode);
		smblite_lib_err(chg, "Couldn't to detect valid battery profile %d\n",
				rc);
		goto out;
	}

	rc = of_property_read_u32_array(pnode, "qcom,jeita-hard-thresholds",
				jeita_hard_thresholds, 2);
	if (!rc) {
		rc = smblite_lib_update_jeita(chg, jeita_hard_thresholds,
					JEITA_HARD);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't configure Hard Jeita rc=%d\n",
					rc);
			goto out;
		}
	}

	rc = of_property_read_u32_array(pnode, "qcom,jeita-soft-thresholds",
				chg->jeita_soft_thlds, 2);
	if (!rc) {
		rc = smblite_lib_update_jeita(chg, chg->jeita_soft_thlds,
					JEITA_SOFT);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't configure Soft Jeita rc=%d\n",
					rc);
			goto out;
		}

		rc = of_property_read_u32_array(pnode,
					"qcom,jeita-soft-hys-thresholds",
					chg->jeita_soft_hys_thlds, 2);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't get Soft Jeita hysteresis thresholds rc=%d\n",
					rc);
			goto out;
		}
	} else {
		/* Populate the jeita-soft-thresholds */
		addr = CHGR_JEITA_COOL_THRESHOLD_REG;
		rc = smblite_lib_batch_read(chg, addr, buff, 2);
		if (rc < 0) {
			pr_err("Couldn't to read 0x%4X, rc=%d\n", addr, rc);
			goto out;
		}
		chg->jeita_soft_thlds[0] = buff[1] | buff[0] << 8;

		rc = smblite_lib_batch_read(chg, addr + 2, buff, 2);
		if (rc < 0) {
			pr_err("Couldn't to read 0x%4X, rc=%d\n", addr + 2, rc);
			goto out;
		}
		chg->jeita_soft_thlds[1] = buff[1] | buff[0] << 8;

		/*
		 * Update the soft jeita hysteresis 2 DegC less for warm and
		 * 2 DegC more for cool than the soft jeita thresholds to avoid
		 * overwriting the registers with invalid values.
		 */
		chg->jeita_soft_hys_thlds[1] =
			chg->jeita_soft_thlds[0] - SOFT_JEITA_HYSTERESIS_OFFSET;
		chg->jeita_soft_hys_thlds[0] =
			chg->jeita_soft_thlds[1] + SOFT_JEITA_HYSTERESIS_OFFSET;
	}

	chg->jeita_soft_fcc[0] = chg->jeita_soft_fcc[1] = -EINVAL;
	chg->jeita_soft_fv[0] = chg->jeita_soft_fv[1] = -EINVAL;
	max_fcc_ma = max_fv_uv = -EINVAL;

	of_property_read_u32(pnode, "qcom,fastchg-current-ma", &max_fcc_ma);
	of_property_read_u32(pnode, "qcom,max-voltage-uv", &max_fv_uv);

	if (max_fcc_ma <= 0 || max_fv_uv <= 0) {
		smblite_lib_err(chg, "Incorrect fastchg-current-ma or max-voltage-uv\n");
		goto out;
	}

	rc = of_property_read_u32_array(pnode, "qcom,jeita-soft-fcc-ua",
					tmp, 2);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get fcc values for soft JEITA rc=%d\n",
				rc);
		goto out;
	}

	max_fcc_ma *= 1000;
	if (tmp[0] > max_fcc_ma || tmp[1] > max_fcc_ma) {
		smblite_lib_err(chg, "Incorrect FCC value [%d %d] max: %d\n",
					tmp[0], tmp[1], max_fcc_ma);
		goto out;
	}
	chg->jeita_soft_fcc[0] = tmp[0];
	chg->jeita_soft_fcc[1] = tmp[1];

	rc = of_property_read_u32_array(pnode, "qcom,jeita-soft-fv-uv", tmp,
					2);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't get fv values for soft JEITA rc=%d\n",
				rc);
		goto out;
	}

	if (tmp[0] > max_fv_uv || tmp[1] > max_fv_uv) {
		smblite_lib_err(chg, "Incorrect FV value [%d %d] max: %d\n",
					tmp[0], tmp[1], max_fv_uv);
		goto out;
	}
	chg->jeita_soft_fv[0] = tmp[0];
	chg->jeita_soft_fv[1] = tmp[1];

	rc = smblite_lib_soft_jeita_arb_wa(chg);
	if (rc < 0) {
		smblite_lib_err(chg, "Couldn't fix soft jeita arb rc=%d\n",
				rc);
		goto out;
	}

	chg->jeita_configured = JEITA_CFG_COMPLETE;
	return;

out:
	chg->jeita_configured = JEITA_CFG_FAILURE;
}

static int smblite_lib_create_votables(struct smb_charger *chg)
{
	int rc = 0;

	chg->fcc_votable = find_votable("FCC");
	if (chg->fcc_votable == NULL) {
		rc = -EINVAL;
		smblite_lib_err(chg, "Couldn't find FCC votable rc=%d\n", rc);
		return rc;
	}

	chg->fcc_main_votable = find_votable("FCC_MAIN");
	if (chg->fcc_main_votable == NULL) {
		rc = -EINVAL;
		smblite_lib_err(chg, "Couldn't find FCC Main votable rc=%d\n",
					rc);
		return rc;
	}

	chg->fv_votable = find_votable("FV");
	if (chg->fv_votable == NULL) {
		rc = -EINVAL;
		smblite_lib_err(chg, "Couldn't find FV votable rc=%d\n", rc);
		return rc;
	}

	chg->usb_icl_votable = find_votable("USB_ICL");
	if (chg->usb_icl_votable == NULL) {
		rc = -EINVAL;
		smblite_lib_err(chg, "Couldn't find USB_ICL votable rc=%d\n",
					rc);
		return rc;
	}

	chg->pl_disable_votable = find_votable("PL_DISABLE");
	if (chg->pl_disable_votable == NULL) {
		rc = -EINVAL;
		smblite_lib_err(chg, "Couldn't find votable PL_DISABLE rc=%d\n",
					rc);
		return rc;
	}

	chg->pl_enable_votable_indirect = find_votable("PL_ENABLE_INDIRECT");
	if (chg->pl_enable_votable_indirect == NULL) {
		rc = -EINVAL;
		smblite_lib_err(chg,
			"Couldn't find votable PL_ENABLE_INDIRECT rc=%d\n",
			rc);
		return rc;
	}

	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);

	chg->awake_votable = create_votable("AWAKE", VOTE_SET_ANY,
					smblite_lib_awake_vote_callback,
					chg);
	if (IS_ERR(chg->awake_votable)) {
		rc = PTR_ERR(chg->awake_votable);
		chg->awake_votable = NULL;
		return rc;
	}

	chg->chg_disable_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					smblite_lib_chg_disable_vote_callback,
					chg);
	if (IS_ERR(chg->chg_disable_votable)) {
		rc = PTR_ERR(chg->chg_disable_votable);
		chg->chg_disable_votable = NULL;
		return rc;
	}

	chg->icl_irq_disable_votable = create_votable("USB_ICL_IRQ_DISABLE",
				VOTE_SET_ANY,
				smblite_lib_icl_irq_disable_vote_callback,
				chg);
	if (IS_ERR(chg->icl_irq_disable_votable)) {
		rc = PTR_ERR(chg->icl_irq_disable_votable);
		chg->icl_irq_disable_votable = NULL;
		return rc;
	}

	chg->temp_change_irq_disable_votable = create_votable(
			"TEMP_CHANGE_IRQ_DISABLE", VOTE_SET_ANY,
			smblite_lib_temp_change_irq_disable_vote_callback, chg);
	if (IS_ERR(chg->temp_change_irq_disable_votable)) {
		rc = PTR_ERR(chg->temp_change_irq_disable_votable);
		chg->temp_change_irq_disable_votable = NULL;
		return rc;
	}

	return rc;
}

static void smblite_lib_destroy_votables(struct smb_charger *chg)
{
	if (chg->usb_icl_votable)
		destroy_votable(chg->usb_icl_votable);
	if (chg->awake_votable)
		destroy_votable(chg->awake_votable);
	if (chg->chg_disable_votable)
		destroy_votable(chg->chg_disable_votable);
}

static void smblite_lib_iio_deinit(struct smb_charger *chg)
{
	if (!IS_ERR_OR_NULL(chg->iio.usbin_v_chan))
		iio_channel_release(chg->iio.usbin_v_chan);
	if (!IS_ERR_OR_NULL(chg->iio.temp_chan))
		iio_channel_release(chg->iio.temp_chan);
}

int smblite_lib_init(struct smb_charger *chg)
{
	int rc = 0;

	INIT_WORK(&chg->bms_update_work, bms_update_work);
	INIT_WORK(&chg->jeita_update_work, jeita_update_work);
	INIT_DELAYED_WORK(&chg->icl_change_work, smblite_lib_icl_change_work);
	INIT_DELAYED_WORK(&chg->pl_enable_work, smblite_lib_pl_enable_work);
	INIT_DELAYED_WORK(&chg->bb_removal_work, smblite_lib_bb_removal_work);
	INIT_DELAYED_WORK(&chg->thermal_regulation_work,
					smblite_lib_thermal_regulation_work);
	INIT_DELAYED_WORK(&chg->role_reversal_check,
					smblite_lib_typec_role_check_work);
	INIT_DELAYED_WORK(&chg->pr_swap_detach_work,
					smblite_lib_pr_swap_detach_work);
	chg->fake_capacity = -EINVAL;
	chg->fake_batt_status = -EINVAL;
	chg->sink_src_mode = UNATTACHED_MODE;
	chg->jeita_configured = false;
	chg->dr_mode = TYPEC_PORT_DRP;

	switch (chg->mode) {
	case PARALLEL_MASTER:
		rc = qcom_batt_init(&chg->chg_param);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't init qcom_batt_init rc=%d\n",
				rc);
			return rc;
		}

		rc = qcom_step_chg_init(chg->dev, true, true, false);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't init qcom_step_chg_init rc=%d\n",
				rc);
			return rc;
		}

		rc = smblite_lib_create_votables(chg);
		if (rc < 0) {
			smblite_lib_err(chg, "Couldn't create votables rc=%d\n",
				rc);
			return rc;
		}

		chg->bms_psy = power_supply_get_by_name("bms");
		rc = smblite_lib_register_notifier(chg);
		if (rc < 0) {
			smblite_lib_err(chg,
				"Couldn't register notifier rc=%d\n", rc);
			return rc;
		}
		break;
	case PARALLEL_SLAVE:
		break;
	default:
		smblite_lib_err(chg, "Unsupported mode %d\n", chg->mode);
		return -EINVAL;
	}

	return rc;
}

int smblite_lib_deinit(struct smb_charger *chg)
{
	switch (chg->mode) {
	case PARALLEL_MASTER:
		cancel_work_sync(&chg->bms_update_work);
		cancel_work_sync(&chg->jeita_update_work);
		cancel_delayed_work_sync(&chg->icl_change_work);
		cancel_delayed_work_sync(&chg->pl_enable_work);
		cancel_delayed_work_sync(&chg->bb_removal_work);
		cancel_delayed_work_sync(&chg->thermal_regulation_work);
		cancel_delayed_work_sync(&chg->role_reversal_check);
		cancel_delayed_work_sync(&chg->pr_swap_detach_work);
		power_supply_unreg_notifier(&chg->nb);
		smblite_lib_destroy_votables(chg);
		qcom_step_chg_deinit();
		qcom_batt_deinit();
		break;
	case PARALLEL_SLAVE:
		break;
	default:
		smblite_lib_err(chg, "Unsupported mode %d\n", chg->mode);
		return -EINVAL;
	}

	smblite_lib_iio_deinit(chg);

	return 0;
}
