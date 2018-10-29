/* Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "SMB1390: %s: " fmt, __func__

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/iio/consumer.h>

#define CORE_STATUS1_REG		0x1006
#define WIN_OV_BIT			BIT(0)
#define WIN_UV_BIT			BIT(1)
#define EN_PIN_OUT_BIT			BIT(2)
#define LCM_AUTO_BIT			BIT(3)
#define LCM_PIN_BIT			BIT(4)
#define ILIM_BIT			BIT(5)
#define TEMP_ALARM_BIT			BIT(6)
#define VPH_OV_SOFT_BIT			BIT(7)

#define CORE_STATUS2_REG		0x1007
#define SWITCHER_HOLD_OFF_BIT		BIT(0)
#define VPH_OV_HARD_BIT			BIT(1)
#define TSD_BIT				BIT(2)
#define IREV_BIT			BIT(3)
#define IOC_BIT				BIT(4)
#define VIN_UV_BIT			BIT(5)
#define VIN_OV_BIT			BIT(6)
#define EN_PIN_OUT2_BIT			BIT(7)

#define CORE_STATUS3_REG		0x1008
#define EN_SL_BIT			BIT(0)
#define IIN_REF_SS_DONE_BIT		BIT(1)
#define FLYCAP_SS_DONE_BIT		BIT(2)
#define SL_DETECTED_BIT			BIT(3)

#define CORE_INT_RT_STS_REG		0x1010
#define SWITCHER_OFF_WINDOW_STS_BIT	BIT(0)
#define SWITCHER_OFF_FAULT_STS_BIT	BIT(1)
#define TSD_STS_BIT			BIT(2)
#define IREV_STS_BIT			BIT(3)
#define VPH_OV_HARD_STS_BIT		BIT(4)
#define VPH_OV_SOFT_STS_BIT		BIT(5)
#define ILIM_STS_BIT			BIT(6)
#define TEMP_ALARM_STS_BIT		BIT(7)

#define CORE_CONTROL1_REG		0x1020
#define CMD_EN_SWITCHER_BIT		BIT(0)
#define CMD_EN_SL_BIT			BIT(1)

#define CORE_FTRIM_ILIM_REG		0x1030
#define CFG_ILIM_MASK			GENMASK(4, 0)

#define CORE_FTRIM_LVL_REG		0x1033
#define CFG_WIN_HI_MASK			GENMASK(3, 2)
#define WIN_OV_LVL_1000MV		0x08

#define CORE_FTRIM_MISC_REG		0x1034
#define TR_WIN_1P5X_BIT			BIT(0)
#define WINDOW_DETECTION_DELTA_X1P0	0
#define WINDOW_DETECTION_DELTA_X1P5	1

#define CORE_ATEST1_SEL_REG		0x10E2
#define ATEST1_OUTPUT_ENABLE_BIT	BIT(7)
#define ATEST1_SEL_MASK			GENMASK(6, 0)
#define ISNS_INT_VAL			0x09

#define CP_VOTER		"CP_VOTER"
#define USER_VOTER		"USER_VOTER"
#define ILIM_VOTER		"ILIM_VOTER"
#define FCC_VOTER		"FCC_VOTER"
#define ICL_VOTER		"ICL_VOTER"
#define WIRELESS_VOTER		"WIRELESS_VOTER"
#define SRC_VOTER		"SRC_VOTER"
#define SWITCHER_TOGGLE_VOTER	"SWITCHER_TOGGLE_VOTER"

enum {
	SWITCHER_OFF_WINDOW_IRQ = 0,
	SWITCHER_OFF_FAULT_IRQ,
	TSD_IRQ,
	IREV_IRQ,
	VPH_OV_HARD_IRQ,
	VPH_OV_SOFT_IRQ,
	ILIM_IRQ,
	TEMP_ALARM_IRQ,
	NUM_IRQS,
};

enum {
	SWITCHER_EN = 0,
	SMB_PIN_EN,
};

struct smb1390_iio {
	struct iio_channel	*die_temp_chan;
};

struct smb1390 {
	struct device		*dev;
	struct regmap		*regmap;
	struct notifier_block	nb;
	struct wakeup_source	*cp_ws;

	/* work structs */
	struct work_struct	status_change_work;
	struct work_struct	taper_work;

	/* mutexes */
	spinlock_t		status_change_lock;
	struct mutex		die_chan_lock;

	/* votables */
	struct votable		*disable_votable;
	struct votable		*ilim_votable;
	struct votable		*fcc_votable;
	struct votable		*cp_awake_votable;

	/* power supplies */
	struct power_supply	*usb_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*dc_psy;
	struct power_supply	*cp_master_psy;

	int			irqs[NUM_IRQS];
	bool			status_change_running;
	bool			taper_work_running;
	struct smb1390_iio	iio;
	int			irq_status;
};

struct smb_irq {
	const char		*name;
	const irq_handler_t	handler;
	const bool		wake;
};

static const struct smb_irq smb_irqs[];

static int smb1390_read(struct smb1390 *chip, int reg, int *val)
{
	int rc;

	rc = regmap_read(chip->regmap, reg, val);
	if (rc < 0)
		pr_err("Couldn't read 0x%04x\n", reg);

	return rc;
}

static int smb1390_masked_write(struct smb1390 *chip, int reg, int mask,
				int val)
{
	int rc;

	pr_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(chip->regmap, reg, mask, val);
	if (rc < 0)
		pr_err("Couldn't write 0x%02x to 0x%04x with mask 0x%02x\n",
		       val, reg, mask);

	return rc;
}

static bool is_psy_voter_available(struct smb1390 *chip)
{
	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			pr_debug("Couldn't find battery psy\n");
			return false;
		}
	}

	if (!chip->usb_psy) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy) {
			pr_debug("Couldn't find usb psy\n");
			return false;
		}
	}

	if (!chip->dc_psy) {
		chip->dc_psy = power_supply_get_by_name("dc");
		if (!chip->dc_psy) {
			pr_debug("Couldn't find dc psy\n");
			return false;
		}
	}

	if (!chip->fcc_votable) {
		chip->fcc_votable = find_votable("FCC");
		if (!chip->fcc_votable) {
			pr_debug("Couldn't find FCC votable\n");
			return false;
		}
	}

	return true;
}

static void cp_toggle_switcher(struct smb1390 *chip)
{
	vote(chip->disable_votable, SWITCHER_TOGGLE_VOTER, true, 0);

	/* Delay for toggling switcher */
	usleep_range(20, 30);

	vote(chip->disable_votable, SWITCHER_TOGGLE_VOTER, false, 0);
}

static int smb1390_get_cp_en_status(struct smb1390 *chip, int id, bool *enable)
{
	int rc = 0, status;

	rc = smb1390_read(chip, CORE_STATUS2_REG, &status);
	if (rc < 0) {
		pr_err("Couldn't read CP_STATUS_2 register, rc=%d\n", rc);
		return rc;
	}

	switch (id) {
	case SWITCHER_EN:
		*enable = !!(status & EN_PIN_OUT2_BIT) &&
				!(status & SWITCHER_HOLD_OFF_BIT);
		break;
	case SMB_PIN_EN:
		*enable = !!(status & EN_PIN_OUT2_BIT);
		break;
	default:
		pr_debug("cp_en status %d is not supported", id);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static irqreturn_t default_irq_handler(int irq, void *data)
{
	struct smb1390 *chip = data;
	int i;

	for (i = 0; i < NUM_IRQS; ++i) {
		if (irq == chip->irqs[i]) {
			pr_debug("%s IRQ triggered\n", smb_irqs[i].name);
			chip->irq_status |= 1 << i;
		}
	}

	if (chip->cp_master_psy)
		power_supply_changed(chip->cp_master_psy);
	return IRQ_HANDLED;
}

static const struct smb_irq smb_irqs[] = {
	[SWITCHER_OFF_WINDOW_IRQ] = {
		.name		= "switcher-off-window",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[SWITCHER_OFF_FAULT_IRQ] = {
		.name		= "switcher-off-fault",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[TSD_IRQ] = {
		.name		= "tsd-fault",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[IREV_IRQ] = {
		.name		= "irev-fault",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[VPH_OV_HARD_IRQ] = {
		.name		= "vph-ov-hard",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[VPH_OV_SOFT_IRQ] = {
		.name		= "vph-ov-soft",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[ILIM_IRQ] = {
		.name		= "ilim",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[TEMP_ALARM_IRQ] = {
		.name		= "temp-alarm",
		.handler	= default_irq_handler,
		.wake		= true,
	},
};

static int smb1390_get_die_temp(struct smb1390 *chip,
			union power_supply_propval *val)
{
	int die_temp_deciC = 0;
	int rc = 0;
	bool enable;

	/*
	 * If SMB1390 chip is not enabled, adc channel read may render
	 * erroneous value. Return error to signify, adc read is not admissible
	 */
	rc = smb1390_get_cp_en_status(chip, SMB_PIN_EN, &enable);
	if (rc < 0) {
		pr_err("Couldn't get SMB_PIN enable status, rc=%d\n", rc);
		return rc;
	}

	if (!enable)
		return -ENODATA;

	mutex_lock(&chip->die_chan_lock);
	rc = iio_read_channel_processed(chip->iio.die_temp_chan,
			&die_temp_deciC);
	mutex_unlock(&chip->die_chan_lock);

	if (rc < 0)
		pr_err("Couldn't read die chan, rc = %d\n", rc);
	else
		val->intval = die_temp_deciC / 100;

	return rc;
}

static int smb1390_get_isns(struct smb1390 *chip,
			union power_supply_propval *val)
{
	int temp = 0;
	int rc;
	bool enable;

	/*
	 * If SMB1390 chip is not enabled, adc channel read may render
	 * erroneous value. Return error to signify, adc read is not admissible
	 */
	rc = smb1390_get_cp_en_status(chip, SMB_PIN_EN, &enable);
	if (rc < 0) {
		pr_err("Couldn't get SMB_PIN enable status, rc=%d\n", rc);
		return rc;
	}

	if (!enable)
		return -ENODATA;

	mutex_lock(&chip->die_chan_lock);
	rc = smb1390_masked_write(chip, CORE_ATEST1_SEL_REG,
				ATEST1_OUTPUT_ENABLE_BIT | ATEST1_SEL_MASK,
				ATEST1_OUTPUT_ENABLE_BIT | ISNS_INT_VAL);
	if (rc < 0) {
		pr_err("Couldn't set CORE_ATEST1_SEL_REG, rc = %d\n", rc);
		goto unlock;
	}

	rc = iio_read_channel_processed(chip->iio.die_temp_chan,
			&temp);
	if (rc < 0) {
		pr_err("Couldn't read die chan for isns, rc = %d\n", rc);
		goto unlock;
	}

	rc = smb1390_masked_write(chip, CORE_ATEST1_SEL_REG,
				ATEST1_OUTPUT_ENABLE_BIT | ATEST1_SEL_MASK, 0);
	if (rc < 0)
		pr_err("Couldn't set CORE_ATEST1_SEL_REG, rc = %d\n", rc);

unlock:
	mutex_unlock(&chip->die_chan_lock);

	/* ISNS = 2 * (1496 - 1390_therm_input * 0.00356) * 1000 uA */
	if (rc >= 0)
		val->intval = (1496 * 1000 - div_s64((s64)temp * 3560,
							1000)) * 2;

	return rc;
}

/* voter callbacks */
static int smb1390_disable_vote_cb(struct votable *votable, void *data,
				  int disable, const char *client)
{
	struct smb1390 *chip = data;
	int rc = 0;

	if (!is_psy_voter_available(chip))
		return -EAGAIN;

	if (disable) {
		rc = smb1390_masked_write(chip, CORE_CONTROL1_REG,
				   CMD_EN_SWITCHER_BIT, 0);
		if (rc < 0)
			return rc;

		vote(chip->cp_awake_votable, CP_VOTER, false, 0);
	} else {
		vote(chip->cp_awake_votable, CP_VOTER, true, 0);
		rc = smb1390_masked_write(chip, CORE_CONTROL1_REG,
				   CMD_EN_SWITCHER_BIT, CMD_EN_SWITCHER_BIT);
		if (rc < 0)
			return rc;
	}

	/* charging may have been disabled by ILIM; send uevent */
	if (chip->cp_master_psy)
		power_supply_changed(chip->cp_master_psy);
	return rc;
}

static int smb1390_ilim_vote_cb(struct votable *votable, void *data,
			      int ilim_uA, const char *client)
{
	struct smb1390 *chip = data;
	int rc = 0;

	if (!is_psy_voter_available(chip))
		return -EAGAIN;

	/* ILIM should always have at least one active vote */
	if (!client) {
		pr_err("Client missing\n");
		return -EINVAL;
	}

	/* ILIM less than 1A is not accurate; disable charging */
	if (ilim_uA < 1000000) {
		pr_debug("ILIM %duA is too low to allow charging\n", ilim_uA);
		vote(chip->disable_votable, ILIM_VOTER, true, 0);
	} else {
		pr_debug("setting ILIM to %duA\n", ilim_uA);
		rc = smb1390_masked_write(chip, CORE_FTRIM_ILIM_REG,
				CFG_ILIM_MASK,
				DIV_ROUND_CLOSEST(ilim_uA - 500000, 100000));
		if (rc < 0)
			pr_err("Failed to write ILIM Register, rc=%d\n", rc);
		if (rc >= 0)
			vote(chip->disable_votable, ILIM_VOTER, false, 0);
	}

	return rc;
}

static int smb1390_awake_vote_cb(struct votable *votable, void *data,
				int awake, const char *client)
{
	struct smb1390 *chip = data;

	if (awake)
		__pm_stay_awake(chip->cp_ws);
	else
		__pm_relax(chip->cp_ws);

	pr_debug("client: %s awake: %d\n", client, awake);
	return 0;
}

static int smb1390_notifier_cb(struct notifier_block *nb,
			       unsigned long event, void *data)
{
	struct smb1390 *chip = container_of(nb, struct smb1390, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, "battery") == 0
				|| strcmp(psy->desc->name, "usb") == 0
				|| strcmp(psy->desc->name, "main") == 0) {
		spin_lock_irqsave(&chip->status_change_lock, flags);
		if (!chip->status_change_running) {
			chip->status_change_running = true;
			pm_stay_awake(chip->dev);
			schedule_work(&chip->status_change_work);
		}
		spin_unlock_irqrestore(&chip->status_change_lock, flags);
	}

	return NOTIFY_OK;
}

static void smb1390_status_change_work(struct work_struct *work)
{
	struct smb1390 *chip = container_of(work, struct smb1390,
					    status_change_work);
	union power_supply_propval pval = {0, };
	int rc;

	if (!is_psy_voter_available(chip))
		goto out;

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_SMB_EN_MODE, &pval);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		goto out;
	}

	if (pval.intval == POWER_SUPPLY_CHARGER_SEC_CP) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_SMB_EN_REASON, &pval);
		if (rc < 0) {
			pr_err("Couldn't get cp reason rc=%d\n", rc);
			goto out;
		}

		vote(chip->disable_votable, SRC_VOTER, false, 0);

		/*
		 * ILIM is set based on the primary chargers AICL result. This
		 * ensures VBUS does not collapse due to the current drawn via
		 * MID.
		 */
		if (pval.intval == POWER_SUPPLY_CP_WIRELESS) {
			vote(chip->ilim_votable, ICL_VOTER, false, 0);
			rc = power_supply_get_property(chip->dc_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
			if (rc < 0)
				pr_err("Couldn't get dc icl rc=%d\n", rc);
			else
				vote(chip->ilim_votable, WIRELESS_VOTER, true,
								pval.intval);
		} else { /* QC3 or PPS */
			vote(chip->ilim_votable, WIRELESS_VOTER, false, 0);
			rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &pval);
			if (rc < 0)
				pr_err("Couldn't get usb icl rc=%d\n", rc);
			else
				vote(chip->ilim_votable, ICL_VOTER, true,
								pval.intval);
		}

		/* input current is always half the charge current */
		vote(chip->ilim_votable, FCC_VOTER, true,
				get_effective_result(chip->fcc_votable) / 2);

		/*
		 * all votes that would result in disabling the charge pump have
		 * been cast; ensure the charhe pump is still enabled before
		 * continuing.
		 */
		if (get_effective_result(chip->disable_votable))
			goto out;

		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get charge type rc=%d\n", rc);
		} else if (pval.intval ==
				POWER_SUPPLY_CHARGE_TYPE_TAPER) {
			/*
			 * mutual exclusion is already guaranteed by
			 * chip->status_change_running
			 */
			if (!chip->taper_work_running) {
				chip->taper_work_running = true;
				queue_work(system_long_wq,
					   &chip->taper_work);
			}
		}
	} else {
		vote(chip->disable_votable, SRC_VOTER, true, 0);
		vote(chip->fcc_votable, CP_VOTER, false, 0);
	}

out:
	pm_relax(chip->dev);
	chip->status_change_running = false;
}

static void smb1390_taper_work(struct work_struct *work)
{
	struct smb1390 *chip = container_of(work, struct smb1390, taper_work);
	union power_supply_propval pval = {0, };
	int rc, fcc_uA;

	if (!is_psy_voter_available(chip))
		goto out;

	do {
		fcc_uA = get_effective_result(chip->fcc_votable) - 100000;
		pr_debug("taper work reducing FCC to %duA\n", fcc_uA);
		vote(chip->fcc_votable, CP_VOTER, true, fcc_uA);

		rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get charge type rc=%d\n", rc);
			goto out;
		}

		msleep(500);
	} while (fcc_uA >= 2000000
		 && pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER);

out:
	pr_debug("taper work exit\n");
	chip->taper_work_running = false;
}

static enum power_supply_property smb1390_charge_pump_props[] = {
	POWER_SUPPLY_PROP_CP_STATUS1,
	POWER_SUPPLY_PROP_CP_STATUS2,
	POWER_SUPPLY_PROP_CP_ENABLE,
	POWER_SUPPLY_PROP_CP_SWITCHER_EN,
	POWER_SUPPLY_PROP_CP_DIE_TEMP,
	POWER_SUPPLY_PROP_CP_ISNS,
	POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER,
	POWER_SUPPLY_PROP_CP_IRQ_STATUS,
	POWER_SUPPLY_PROP_CP_ILIM,
};

static int smb1390_get_prop(struct power_supply *psy,
			enum power_supply_property prop,
			union power_supply_propval *val)
{
	struct smb1390 *chip = power_supply_get_drvdata(psy);
	int rc = 0, status;
	bool enable;

	switch (prop) {
	case POWER_SUPPLY_PROP_CP_STATUS1:
		rc = smb1390_read(chip, CORE_STATUS1_REG, &status);
		if (!rc)
			val->intval = status;
		break;
	case POWER_SUPPLY_PROP_CP_STATUS2:
		rc = smb1390_read(chip, CORE_STATUS2_REG, &status);
		if (!rc)
			val->intval = status;
		break;
	case POWER_SUPPLY_PROP_CP_ENABLE:
		rc = smb1390_get_cp_en_status(chip, SMB_PIN_EN, &enable);
		if (!rc)
			val->intval = enable &&
				!get_effective_result(chip->disable_votable);
		break;
	case POWER_SUPPLY_PROP_CP_SWITCHER_EN:
		rc = smb1390_get_cp_en_status(chip, SWITCHER_EN, &enable);
		if (!rc)
			val->intval = enable;
		break;
	case POWER_SUPPLY_PROP_CP_DIE_TEMP:
		rc = smb1390_get_die_temp(chip, val);
		break;
	case POWER_SUPPLY_PROP_CP_ISNS:
		rc = smb1390_get_isns(chip, val);
		break;
	case POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_IRQ_STATUS:
		/*
		 * irq_status variable stores provious IRQs that have been
		 * handled by kernel, but not addressed by user space daemon.
		 */
		val->intval = chip->irq_status;
		rc = smb1390_read(chip, CORE_INT_RT_STS_REG, &status);
		if (!rc)
			val->intval |= status;
		break;
	case POWER_SUPPLY_PROP_CP_ILIM:
		rc = smb1390_read(chip, CORE_FTRIM_ILIM_REG, &status);
		if (!rc)
			val->intval = ((status & CFG_ILIM_MASK) * 100000)
					+ 500000;
		break;
	default:
		pr_debug("charge pump power supply get prop %d not supported\n",
			prop);
		return -EINVAL;
	}

	return rc;
}

static int smb1390_set_prop(struct power_supply *psy,
			enum power_supply_property prop,
			const union power_supply_propval *val)
{
	struct smb1390 *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_CP_ENABLE:
		vote(chip->disable_votable, USER_VOTER, !val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER:
		if (val->intval)
			cp_toggle_switcher(chip);
		break;
	case POWER_SUPPLY_PROP_CP_IRQ_STATUS:
		chip->irq_status = val->intval;
		break;
	default:
		pr_debug("charge pump power supply set prop %d not supported\n",
			prop);
		return -EINVAL;
	}

	return rc;
}

static int smb1390_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CP_ENABLE:
	case POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER:
	case POWER_SUPPLY_PROP_CP_IRQ_STATUS:
		return 1;
	default:
		break;
	}

	return 0;
}

static struct power_supply_desc charge_pump_psy_desc = {
	.name			= "charge_pump_master",
	.type			= POWER_SUPPLY_TYPE_CHARGE_PUMP,
	.properties		= smb1390_charge_pump_props,
	.num_properties		= ARRAY_SIZE(smb1390_charge_pump_props),
	.get_property		= smb1390_get_prop,
	.set_property		= smb1390_set_prop,
	.property_is_writeable	= smb1390_prop_is_writeable,
};

static int smb1390_init_charge_pump_psy(struct smb1390 *chip)
{
	struct power_supply_config charge_pump_cfg = {};

	charge_pump_cfg.drv_data = chip;
	charge_pump_cfg.of_node = chip->dev->of_node;

	chip->cp_master_psy = devm_power_supply_register(chip->dev,
							&charge_pump_psy_desc,
							&charge_pump_cfg);
	if (IS_ERR(chip->cp_master_psy)) {
		pr_err("Couldn't register charge pump power supply\n");
		return PTR_ERR(chip->cp_master_psy);
	}

	return 0;
}

static int smb1390_parse_dt(struct smb1390 *chip)
{
	int rc;

	rc = of_property_match_string(chip->dev->of_node, "io-channel-names",
			"cp_die_temp");
	if (rc >= 0) {
		chip->iio.die_temp_chan =
			iio_channel_get(chip->dev, "cp_die_temp");
		if (IS_ERR(chip->iio.die_temp_chan)) {
			rc = PTR_ERR(chip->iio.die_temp_chan);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev,
					"cp_die_temp channel unavailable %ld\n",
					rc);
			chip->iio.die_temp_chan = NULL;
			return rc;
		}
	}

	return rc;
}

static void smb1390_release_channels(struct smb1390 *chip)
{
	if (!IS_ERR_OR_NULL(chip->iio.die_temp_chan))
		iio_channel_release(chip->iio.die_temp_chan);
}

static int smb1390_create_votables(struct smb1390 *chip)
{
	chip->cp_awake_votable = create_votable("CP_AWAKE", VOTE_SET_ANY,
			smb1390_awake_vote_cb, chip);
	if (IS_ERR(chip->cp_awake_votable))
		return PTR_ERR(chip->cp_awake_votable);

	chip->disable_votable = create_votable("CP_DISABLE",
			VOTE_SET_ANY, smb1390_disable_vote_cb, chip);
	if (IS_ERR(chip->disable_votable))
		return PTR_ERR(chip->disable_votable);

	chip->ilim_votable = create_votable("CP_ILIM",
			VOTE_MIN, smb1390_ilim_vote_cb, chip);
	if (IS_ERR(chip->ilim_votable))
		return PTR_ERR(chip->ilim_votable);

	/*
	 * charge pump is initially disabled; this indirectly votes to allow
	 * traditional parallel charging if present
	 */
	vote(chip->disable_votable, USER_VOTER, true, 0);

	return 0;
}

static void smb1390_destroy_votables(struct smb1390 *chip)
{
	destroy_votable(chip->disable_votable);
	destroy_votable(chip->ilim_votable);
	destroy_votable(chip->cp_awake_votable);
}

static int smb1390_init_hw(struct smb1390 *chip)
{
	int rc;

	/*
	 * Improve ILIM accuracy:
	 *  - Configure window (Vin - 2Vout) OV level to 1000mV
	 *  - Configure VOUT tracking value to 1.0
	 */
	rc = smb1390_masked_write(chip, CORE_FTRIM_LVL_REG,
			CFG_WIN_HI_MASK, WIN_OV_LVL_1000MV);
	if (rc < 0)
		return rc;

	rc = smb1390_masked_write(chip, CORE_FTRIM_MISC_REG,
			TR_WIN_1P5X_BIT, WINDOW_DETECTION_DELTA_X1P0);
	if (rc < 0)
		return rc;


	return 0;
}

static int smb1390_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb_irqs); i++) {
		if (strcmp(smb_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int smb1390_request_interrupt(struct smb1390 *chip,
				struct device_node *node,
				const char *irq_name)
{
	int rc = 0, irq, irq_index;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		pr_err("Couldn't get irq %s byname\n", irq_name);
		return irq;
	}

	irq_index = smb1390_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	if (!smb_irqs[irq_index].handler)
		return 0;

	rc = devm_request_threaded_irq(chip->dev, irq, NULL,
				smb_irqs[irq_index].handler,
				IRQF_ONESHOT, irq_name, chip);
	if (rc < 0) {
		pr_err("Couldn't request irq %d rc=%d\n", irq, rc);
		return rc;
	}

	chip->irqs[irq_index] = irq;
	if (smb_irqs[irq_index].wake)
		enable_irq_wake(irq);

	return rc;
}

static int smb1390_request_interrupts(struct smb1390 *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *child;
	int rc = 0;
	const char *name;
	struct property *prop;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
					    prop, name) {
			rc = smb1390_request_interrupt(chip, child, name);
			if (rc < 0) {
				pr_err("Couldn't request interrupt %s rc=%d\n",
					name, rc);
				return rc;
			}
		}
	}

	return rc;
}

static int smb1390_probe(struct platform_device *pdev)
{
	struct smb1390 *chip;
	int rc;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	spin_lock_init(&chip->status_change_lock);
	mutex_init(&chip->die_chan_lock);

	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		pr_err("Couldn't get regmap\n");
		return -EINVAL;
	}

	rc = smb1390_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	chip->cp_ws = wakeup_source_register("qcom-chargepump");
	if (!chip->cp_ws)
		return -ENOMEM;

	INIT_WORK(&chip->status_change_work, smb1390_status_change_work);
	INIT_WORK(&chip->taper_work, smb1390_taper_work);

	rc = smb1390_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't init hardware rc=%d\n", rc);
		goto out_work;
	}

	rc = smb1390_create_votables(chip);
	if (rc < 0) {
		pr_err("Couldn't create votables rc=%d\n", rc);
		goto out_work;
	}

	rc = smb1390_init_charge_pump_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize charge pump psy rc=%d\n", rc);
		goto out_votables;
	}

	chip->nb.notifier_call = smb1390_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc=%d\n", rc);
		goto out_votables;
	}

	rc = smb1390_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		goto out_notifier;
	}

	pr_info("smb1390 probed successfully");
	return 0;

out_notifier:
	power_supply_unreg_notifier(&chip->nb);
out_votables:
	smb1390_destroy_votables(chip);
out_work:
	cancel_work(&chip->taper_work);
	cancel_work(&chip->status_change_work);
	wakeup_source_unregister(chip->cp_ws);
	return rc;
}

static int smb1390_remove(struct platform_device *pdev)
{
	struct smb1390 *chip = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&chip->nb);

	/* explicitly disable charging */
	vote(chip->disable_votable, USER_VOTER, true, 0);
	cancel_work(&chip->taper_work);
	cancel_work(&chip->status_change_work);
	wakeup_source_unregister(chip->cp_ws);
	smb1390_destroy_votables(chip);
	smb1390_release_channels(chip);
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,smb1390-charger-psy", },
	{ },
};

static struct platform_driver smb1390_driver = {
	.driver	= {
		.name		= "qcom,smb1390-charger-psy",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe	= smb1390_probe,
	.remove	= smb1390_remove,
};
module_platform_driver(smb1390_driver);

MODULE_DESCRIPTION("SMB1390 Charge Pump Driver");
MODULE_LICENSE("GPL v2");
