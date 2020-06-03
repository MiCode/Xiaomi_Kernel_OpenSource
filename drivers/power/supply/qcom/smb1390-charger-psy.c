// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "SMB1390: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/iio/consumer.h>

#define MISC_CSIR_LSB_REG		0x9F1
#define MISC_CSIR_MSB_REG		0x9F2
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

#define CORE_FTRIM_CTRL_REG		0x1031
#define TEMP_ALERT_LVL_MASK		GENMASK(6, 5)
#define TEMP_ALERT_LVL_SHIFT		5
#define TEMP_BUFFER_OUTPUT_BIT		BIT(7)

#define CORE_FTRIM_LVL_REG		0x1033
#define CFG_WIN_HI_MASK			GENMASK(3, 2)
#define WIN_OV_LVL_1000MV		0x08

#define CORE_FTRIM_MISC_REG		0x1034
#define TR_WIN_1P5X_BIT			BIT(0)
#define TR_IREV_BIT			BIT(1)
#define WINDOW_DETECTION_DELTA_X1P0	0
#define WINDOW_DETECTION_DELTA_X1P5	1

#define CORE_FTRIM_DIS_REG		0x1035
#define TR_DIS_ILIM_DET_BIT		BIT(4)

#define CORE_ATEST1_SEL_REG		0x10E2
#define ATEST1_OUTPUT_ENABLE_BIT	BIT(7)
#define ATEST1_SEL_MASK			GENMASK(6, 0)
#define ISNS_INT_VAL			0x09

#define CP_VOTER		"CP_VOTER"
#define USER_VOTER		"USER_VOTER"
#define ILIM_VOTER		"ILIM_VOTER"
#define TAPER_END_VOTER		"TAPER_END_VOTER"
#define FCC_VOTER		"FCC_VOTER"
#define ICL_VOTER		"ICL_VOTER"
#define WIRELESS_VOTER		"WIRELESS_VOTER"
#define SRC_VOTER		"SRC_VOTER"
#define SWITCHER_TOGGLE_VOTER	"SWITCHER_TOGGLE_VOTER"
#define SOC_LEVEL_VOTER		"SOC_LEVEL_VOTER"
#define HW_DISABLE_VOTER	"HW_DISABLE_VOTER"
#define CC_MODE_VOTER		"CC_MODE_VOTER"
#define MAIN_DISABLE_VOTER	"MAIN_DISABLE_VOTER"
#define TAPER_MAIN_ICL_LIMIT_VOTER	"TAPER_MAIN_ICL_LIMIT_VOTER"

#define CP_MASTER		0
#define CP_SLAVE		1
#define THERMAL_SUSPEND_DECIDEGC	1400
#define MAX_ILIM_UA			3200000
#define MAX_ILIM_DUAL_CP_UA		6400000
#define CC_MODE_TAPER_DELTA_UA		200000
#define DEFAULT_TAPER_DELTA_UA		100000
#define CC_MODE_TAPER_MAIN_ICL_UA	500000

#define smb1390_dbg(chip, reason, fmt, ...)				\
	do {								\
		if (chip->debug_mask & (reason))			\
			pr_info("SMB1390: %s: " fmt, __func__,		\
				##__VA_ARGS__);				\
		else							\
			pr_debug("SMB1390: %s: " fmt, __func__,		\
				##__VA_ARGS__);				\
	} while (0)

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

enum isns_mode {
	ISNS_MODE_OFF = 0,
	ISNS_MODE_ACTIVE,
	ISNS_MODE_STANDBY,
};

enum {
	SWITCHER_EN = 0,
	SMB_PIN_EN,
};

enum print_reason {
	PR_INTERRUPT		= BIT(0),
	PR_REGISTER		= BIT(1),
	PR_INFO			= BIT(2),
	PR_EXT_DEPENDENCY	= BIT(3),
	PR_MISC			= BIT(4),
};

struct smb1390_iio {
	struct iio_channel	*die_temp_chan;
};

struct smb1390 {
	struct device		*dev;
	struct regmap		*regmap;
	struct notifier_block	nb;
	struct wakeup_source	*cp_ws;
	struct dentry		*dfs_root;
	struct pmic_revid_data	*pmic_rev_id;

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
	struct votable		*fv_votable;
	struct votable		*cp_awake_votable;
	struct votable		*slave_disable_votable;
	struct votable		*usb_icl_votable;
	struct votable		*fcc_main_votable;

	/* power supplies */
	struct power_supply	*cps_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*dc_psy;
	struct power_supply	*cp_master_psy;

	int			irqs[NUM_IRQS];
	bool			status_change_running;
	bool			taper_work_running;
	bool			smb_init_done;
	struct smb1390_iio	iio;
	int			irq_status;
	int			taper_entry_fv;
	bool			switcher_enabled;
	int			cp_status1;
	int			cp_status2;
	int			cp_enable;
	int			cp_isns_master;
	int			cp_isns_slave;
	int			cp_ilim;
	int			die_temp;
	bool			suspended;
	bool			disabled;
	u32			debug_mask;
	u32			min_ilim_ua;
	u32			max_temp_alarm_degc;
	u32			max_cutoff_soc;
	u32			pl_output_mode;
	u32			pl_input_mode;
	u32			cp_role;
	enum isns_mode		current_capability;
	bool			batt_soc_validated;
	int			cp_slave_thr_taper_ua;
	int			cc_mode_taper_main_icl_ua;
};

struct smb_cfg {
	u16	address;
	u8	mask;
	u8	val;
};

/* SMB1390 rev2/3 for dual charge */
static const struct smb_cfg smb1390_dual[] = {
	{0x1031, 0xff, 0x7A},
	{0x1032, 0xff, 0x07},
	{0x1035, 0xff, 0x63},
	{0x1036, 0xff, 0x80},
	{0x103A, 0xff, 0x44},
};

/* SMB1390 rev3, CSIR2500, for triple charge */
static const struct smb_cfg smb1390_csir2500_triple[] = {
	{0x1030, 0x80, 0x80},
	{0x1031, 0xff, 0x72},
	{0x1032, 0xff, 0x03},
	{0x1033, 0x04, 0x04},
	{0x1034, 0x80, 0x00},
	{0x1035, 0xff, 0xE3},
	{0x1036, 0xff, 0xA0},
	{0x1037, 0xff, 0x80},
	{0x1039, 0xff, 0x30},
	{0x103A, 0xff, 0x40},
	{0x103B, 0xff, 0x20},
	{0x103E, 0xff, 0x00},
};

/* SMB1390 rev3, CSIR 2515 or 2519 for triple charge */
static const struct smb_cfg smb1390_triple[] = {
	{0x1031, 0xff, 0x72},
	{0x1032, 0xff, 0x03},
	{0x1035, 0xff, 0xE3},
	{0x1036, 0xff, 0xA0},
	{0x103A, 0xff, 0x40},
	{0x1037, 0x04, 0x00},
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

	smb1390_dbg(chip, PR_REGISTER, "Writing 0x%02x to 0x%04x with mask 0x%02x\n",
			val, reg, mask);
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
			smb1390_dbg(chip, PR_EXT_DEPENDENCY, "Couldn't find battery psy\n");
			return false;
		}
	}

	if (!chip->usb_psy) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy) {
			smb1390_dbg(chip, PR_EXT_DEPENDENCY, "Couldn't find usb psy\n");
			return false;
		}
	}

	if (!chip->dc_psy) {
		chip->dc_psy = power_supply_get_by_name("dc");
		if (!chip->dc_psy) {
			smb1390_dbg(chip, PR_EXT_DEPENDENCY, "Couldn't find dc psy\n");
			return false;
		}
	}

	if (!chip->fcc_votable) {
		chip->fcc_votable = find_votable("FCC");
		if (!chip->fcc_votable) {
			smb1390_dbg(chip, PR_EXT_DEPENDENCY, "Couldn't find FCC votable\n");
			return false;
		}
	}

	if (!chip->fv_votable) {
		chip->fv_votable = find_votable("FV");
		if (!chip->fv_votable) {
			smb1390_dbg(chip, PR_EXT_DEPENDENCY, "Couldn't find FV votable\n");
			return false;
		}
	}

	if (!chip->usb_icl_votable) {
		chip->usb_icl_votable = find_votable("USB_ICL");
		if (!chip->usb_icl_votable) {
			smb1390_dbg(chip, PR_EXT_DEPENDENCY, "Couldn't find ICL votable\n");
			return false;
		}
	}

	if (!chip->disable_votable) {
		smb1390_dbg(chip, PR_MISC, "Couldn't find CP DISABLE votable\n");
		return false;
	}

	return true;
}

static int smb1390_isns_mode_control(struct smb1390 *chip, enum isns_mode mode)
{
	int rc;
	u8 val;

	switch  (mode) {
	case ISNS_MODE_ACTIVE:
		val = ATEST1_OUTPUT_ENABLE_BIT | ISNS_INT_VAL;
		break;
	case ISNS_MODE_STANDBY:
		val = ATEST1_OUTPUT_ENABLE_BIT;
		break;
	case ISNS_MODE_OFF:
	default:
		val = 0;
		break;
	}

	rc = smb1390_masked_write(chip, CORE_ATEST1_SEL_REG,
				ATEST1_OUTPUT_ENABLE_BIT | ATEST1_SEL_MASK,
				val);
	if (rc < 0)
		pr_err("Couldn't set CORE_ATEST1_SEL_REG, rc = %d\n", rc);

	return rc;
}

static bool smb1390_is_adapter_cc_mode(struct smb1390 *chip)
{
	int rc;
	union power_supply_propval pval = {0, };

	if (!chip->usb_psy) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy)
			return false;
	}

	rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ADAPTER_CC_MODE,
				&pval);
	if (rc < 0) {
		pr_err("Couldn't get PPS CC mode status rc=%d\n", rc);
		return false;
	}

	return pval.intval;
}

static bool is_cps_available(struct smb1390 *chip)
{
	if (!chip->cps_psy)
		chip->cps_psy = power_supply_get_by_name("cp_slave");
	else
		return true;

	if (!chip->cps_psy)
		return false;

	return true;
}

static void cp_toggle_switcher(struct smb1390 *chip)
{
	int rc;

	/*
	 * Disable ILIM detection before toggling the switcher
	 * to prevent any ILIM interrupt storm while toggling
	 * the switcher.
	 */
	rc = regmap_update_bits(chip->regmap, CORE_FTRIM_DIS_REG,
			TR_DIS_ILIM_DET_BIT, TR_DIS_ILIM_DET_BIT);
	if (rc < 0)
		pr_err("Couldn't disable ILIM rc=%d\n", rc);

	vote(chip->disable_votable, SWITCHER_TOGGLE_VOTER, true, 0);

	/* Delay for toggling switcher */
	usleep_range(20, 30);

	vote(chip->disable_votable, SWITCHER_TOGGLE_VOTER, false, 0);

	rc = regmap_update_bits(chip->regmap, CORE_FTRIM_DIS_REG,
			TR_DIS_ILIM_DET_BIT, 0);
	if (rc < 0)
		pr_err("Couldn't enable ILIM rc=%d\n", rc);
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
		smb1390_dbg(chip, PR_MISC, "cp_en status %d is not supported\n",
				id);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int smb1390_set_ilim(struct smb1390 *chip, int ilim_ua)
{
	int rc;

	rc = smb1390_masked_write(chip, CORE_FTRIM_ILIM_REG,
			CFG_ILIM_MASK, ilim_ua);
	if (rc < 0)
		pr_err("Failed to write ILIM Register, rc=%d\n", rc);

	return rc;
}

static irqreturn_t default_irq_handler(int irq, void *data)
{
	struct smb1390 *chip = data;
	int i, rc;
	bool enable;

	for (i = 0; i < NUM_IRQS; ++i) {
		if (irq == chip->irqs[i]) {
			smb1390_dbg(chip, PR_INTERRUPT, "%s IRQ triggered\n",
				smb_irqs[i].name);
			chip->irq_status |= 1 << i;
		}
	}

	rc = smb1390_get_cp_en_status(chip, SWITCHER_EN, &enable);
	if (!rc) {
		if (chip->switcher_enabled != enable) {
			chip->switcher_enabled = enable;
			if (chip->fcc_votable)
				rerun_election(chip->fcc_votable);
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

static int smb1390_get_isns(int temp)
{
	/* ISNS = 2 * (1496 - 1390_therm_input * 0.00356) * 1000 uA */
	return ((1496 * 1000 - div_s64((s64)temp * 3560, 1000)) * 2);
}

static int smb1390_get_isns_master(struct smb1390 *chip,
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

	/*
	 * Since master and slave share temp_pin line
	 * which is re-used to measure isns, configure the
	 * master as follows:
	 * 1. Put slave in standby mode
	 * 2. Configure master to provide current reading
	 * 3. Read current value
	 * 4. Configure master back to report temperature
	 */
	mutex_lock(&chip->die_chan_lock);
	if (is_cps_available(chip)) {
		val->intval = ISNS_MODE_STANDBY;
		rc = power_supply_set_property(chip->cps_psy,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY, val);
		if (rc < 0) {
			pr_err("Couldn't change slave charging state rc=%d\n",
				rc);
			goto unlock;
		}
	}

	rc = smb1390_isns_mode_control(chip, ISNS_MODE_ACTIVE);
	if (rc < 0) {
		pr_err("Failed to set master in Active mode, rc=%d\n", rc);
		goto unlock;
	}

	rc = iio_read_channel_processed(chip->iio.die_temp_chan,
			&temp);
	if (rc < 0) {
		pr_err("Couldn't read die_temp chan for isns, rc = %d\n", rc);
		goto unlock;
	}

	rc = smb1390_isns_mode_control(chip, ISNS_MODE_OFF);
	if (rc < 0)
		pr_err("Couldn't set master to off mode, rc = %d\n", rc);

	if (is_cps_available(chip)) {
		val->intval = ISNS_MODE_OFF;
		rc = power_supply_set_property(chip->cps_psy,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY, val);
		if (rc < 0)
			pr_err("Couldn't change slave charging state rc=%d\n",
				rc);
	}

unlock:
	mutex_unlock(&chip->die_chan_lock);

	if (rc >= 0)
		val->intval = smb1390_get_isns(temp);

	return rc;
}

static int smb1390_get_isns_slave(struct smb1390 *chip,
			union power_supply_propval *val)
{
	int temp = 0;
	int rc;
	bool enable;

	if (!is_cps_available(chip)) {
		val->intval = 0;
		return 0;
	}
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

	/*
	 * Since master and slave share temp_pin line
	 * which is re-used to measure isns, configure the
	 * slave as follows:
	 * 1. Put slave in standby mode
	 * 2. Configure slave to in Active mode to provide current reading
	 * 3. Read current value
	 */
	mutex_lock(&chip->die_chan_lock);
	rc = smb1390_isns_mode_control(chip, ISNS_MODE_STANDBY);
	if (rc < 0)
		goto unlock;

	val->intval = ISNS_MODE_ACTIVE;
	rc = power_supply_set_property(chip->cps_psy,
			POWER_SUPPLY_PROP_CURRENT_CAPABILITY, val);
	if (rc < 0) {
		pr_err("Couldn't change slave charging state rc=%d\n",
			rc);
		goto unlock;
	}

	rc = iio_read_channel_processed(chip->iio.die_temp_chan,
			&temp);
	if (rc < 0) {
		pr_err("Couldn't read die chan for isns, rc = %d\n", rc);
		goto unlock;
	}

	val->intval = ISNS_MODE_OFF;
	rc = power_supply_set_property(chip->cps_psy,
			POWER_SUPPLY_PROP_CURRENT_CAPABILITY, val);
	if (rc < 0)
		pr_err("Couldn't change slave charging state rc=%d\n",
			rc);

	rc = smb1390_isns_mode_control(chip, ISNS_MODE_OFF);
	if (rc < 0)
		pr_err("Couldn't set CORE_ATEST1_SEL_REG, rc = %d\n", rc);


unlock:
	mutex_unlock(&chip->die_chan_lock);

	if (rc >= 0)
		val->intval = smb1390_get_isns(temp);

	return rc;
}

static int smb1390_get_cp_ilim(struct smb1390 *chip,
			       union power_supply_propval *val)
{
	int rc = 0, status;

	if (is_cps_available(chip)) {
		if (!chip->ilim_votable) {
			chip->ilim_votable = find_votable("CP_ILIM");
			if (!chip->ilim_votable)
				return -EINVAL;
		}

		val->intval = get_effective_result(chip->ilim_votable);
	} else {
		rc = smb1390_read(chip, CORE_FTRIM_ILIM_REG, &status);
		if (!rc)
			val->intval =
				((status & CFG_ILIM_MASK) * 100000)
					+ 500000;
	}

	return rc;
}

static int smb1390_is_batt_soc_valid(struct smb1390 *chip)
{
	int rc;
	union power_supply_propval pval = {0, };

	if (!chip->batt_psy)
		goto out;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		pr_err("Couldn't get CAPACITY rc=%d\n", rc);
		goto out;
	}

	if (pval.intval >= chip->max_cutoff_soc)
		return false;

out:
	return true;
}

static int smb1390_triple_init_hw(struct smb1390 *chip)
{
	int i, rc = 0;
	int csir_lsb = 0, csir_msb = 0;
	u16 csir = 0;

	smb1390_read(chip, MISC_CSIR_LSB_REG, &csir_lsb);
	smb1390_read(chip, MISC_CSIR_MSB_REG, &csir_msb);
	csir = ((csir_msb << 8) | csir_lsb);
	smb1390_dbg(chip, PR_INFO, "CSIR register = 0x%04x\n", csir);

	if (csir == 0x2500) {
		for (i = 0; i < ARRAY_SIZE(smb1390_csir2500_triple); i++) {
			rc = smb1390_masked_write(chip,
				smb1390_csir2500_triple[i].address,
				smb1390_csir2500_triple[i].mask,
				smb1390_csir2500_triple[i].val);
			if (rc < 0) {
				pr_err("Failed to configure SMB1390 for triple chg config for address 0x%04x rc=%d\n",
				       smb1390_csir2500_triple[i].address, rc);
				return rc;
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(smb1390_triple); i++) {
			rc = smb1390_masked_write(chip,
				smb1390_triple[i].address,
				smb1390_triple[i].mask,
				smb1390_triple[i].val);
			if (rc < 0) {
				pr_err("Failed to configure SMB1390 for triple chg config for address 0x%04x rc=%d\n",
				       smb1390_triple[i].address, rc);
				return rc;
			}
		}
	}

	smb1390_dbg(chip, PR_INFO, "Configured SMB1390 charge pump for triple chg config\n");
	chip->smb_init_done = true;
	return rc;
}

static int smb1390_dual_init_hw(struct smb1390 *chip)
{
	int rc = 0, i;

	for (i = 0; i < ARRAY_SIZE(smb1390_dual); i++) {
		rc = smb1390_masked_write(chip,
			smb1390_dual[i].address,
			smb1390_dual[i].mask,
			smb1390_dual[i].val);
		if (rc < 0) {
			pr_err("Failed to configure SMB1390 for dual chg config for address 0x%04x rc=%d\n",
			       smb1390_dual[i].address, rc);
			return rc;
		}
	}

	smb1390_dbg(chip, PR_INFO, "Configured SMB1390 charge pump for Dual chg config\n");
	return rc;
}

/* voter callbacks */
static int smb1390_disable_vote_cb(struct votable *votable, void *data,
				  int disable, const char *client)
{
	struct smb1390 *chip = data;
	int rc = 0;

	if (!is_psy_voter_available(chip) || chip->suspended)
		return -EAGAIN;

	if (is_cps_available(chip))
		vote(chip->slave_disable_votable, MAIN_DISABLE_VOTER,
					disable ? true : false, 0);

	rc = smb1390_masked_write(chip, CORE_CONTROL1_REG, CMD_EN_SWITCHER_BIT,
				  disable ? 0 : CMD_EN_SWITCHER_BIT);
	if (rc < 0) {
		pr_err("Couldn't write CORE_CONTROL1_REG, rc=%d\n", rc);
		return rc;
	}

	smb1390_dbg(chip, PR_INFO, "client: %s, master: %s\n",
			client, (disable ? "disabled" : "enabled"));

	/* charging may have been disabled by ILIM; send uevent */
	if (chip->cp_master_psy && (disable != chip->disabled))
		power_supply_changed(chip->cp_master_psy);

	chip->disabled = disable;
	return rc;
}

static int smb1390_slave_disable_vote_cb(struct votable *votable, void *data,
			      int disable, const char *client)
{
	struct smb1390 *chip = data;
	int rc = 0, ilim_ua = 0;

	rc = smb1390_masked_write(chip, CORE_CONTROL1_REG, CMD_EN_SL_BIT,
					disable ? 0 : CMD_EN_SL_BIT);
	if (rc < 0) {
		pr_err("Couldn't %s slave rc=%d\n",
				disable ? "disable" : "enable", rc);
		return rc;
	}

	smb1390_dbg(chip, PR_INFO, "client: %s, slave: %s\n",
			client, (disable ? "disabled" : "enabled"));

	/* Re-distribute ILIM to Master CP when Slave is disabled */
	if (disable && (chip->ilim_votable)) {
		ilim_ua = get_effective_result_locked(chip->ilim_votable);
		if (ilim_ua > MAX_ILIM_UA)
			ilim_ua = MAX_ILIM_UA;

		if (ilim_ua < 500000) {
			smb1390_dbg(chip, PR_INFO, "ILIM too low, not re-distributing, ilim=%duA\n",
								ilim_ua);
			return 0;
		}

		rc = smb1390_set_ilim(chip,
		      DIV_ROUND_CLOSEST(ilim_ua - 500000, 100000));
		if (rc < 0) {
			pr_err("Failed to set ILIM, rc=%d\n", rc);
			return rc;
		}

		smb1390_dbg(chip, PR_INFO, "Master ILIM set to %duA\n",
								ilim_ua);
	}

	return rc;
}

static int smb1390_ilim_vote_cb(struct votable *votable, void *data,
			      int ilim_uA, const char *client)
{
	struct smb1390 *chip = data;
	union power_supply_propval pval = {0, };
	int rc = 0;
	bool slave_enabled = false;

	if (!is_psy_voter_available(chip) || chip->suspended)
		return -EAGAIN;

	/* ILIM should always have at least one active vote */
	if (!client) {
		pr_err("Client missing\n");
		return -EINVAL;
	}

	ilim_uA = min(ilim_uA, (is_cps_available(chip) ?
				MAX_ILIM_DUAL_CP_UA : MAX_ILIM_UA));
	/* ILIM less than min_ilim_ua, disable charging */
	if (ilim_uA < chip->min_ilim_ua) {
		smb1390_dbg(chip, PR_INFO, "ILIM %duA is too low to allow charging\n",
			ilim_uA);
		vote(chip->disable_votable, ILIM_VOTER, true, 0);
	} else {
		/* Disable Slave CP if ILIM is < 2 * min ILIM */
		if (is_cps_available(chip)) {
			vote(chip->slave_disable_votable, ILIM_VOTER,
				(ilim_uA < (2 * chip->min_ilim_ua)), 0);

			if (get_effective_result(chip->slave_disable_votable)
									== 0)
				slave_enabled = true;
		}

		if (slave_enabled) {
			ilim_uA /= 2;
			pval.intval = DIV_ROUND_CLOSEST(ilim_uA - 500000,
					100000);
			rc = power_supply_set_property(chip->cps_psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
					&pval);
			if (rc < 0)
				pr_err("Couldn't change slave ilim  rc=%d\n",
					rc);
		}

		rc = smb1390_set_ilim(chip,
		      DIV_ROUND_CLOSEST(ilim_uA - 500000, 100000));
		if (rc < 0) {
			pr_err("Failed to set ILIM, rc=%d\n", rc);
			return rc;
		}

		smb1390_dbg(chip, PR_INFO, "ILIM set to %duA slave_enabled = %d\n",
						ilim_uA, slave_enabled);
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

	smb1390_dbg(chip, PR_INFO, "client: %s awake: %d\n", client, awake);

	return 0;
}

static int smb1390_notifier_cb(struct notifier_block *nb,
			       unsigned long event, void *data)
{
	struct smb1390 *chip = container_of(nb, struct smb1390, nb);
	struct power_supply *psy = data;
	int rc;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, "battery") == 0
				|| strcmp(psy->desc->name, "usb") == 0
				|| strcmp(psy->desc->name, "main") == 0
				|| strcmp(psy->desc->name, "cp_slave") == 0) {
		spin_lock_irqsave(&chip->status_change_lock, flags);

		if (!chip->status_change_running) {
			chip->status_change_running = true;
			pm_stay_awake(chip->dev);
			schedule_work(&chip->status_change_work);
		}
		spin_unlock_irqrestore(&chip->status_change_lock, flags);

		/*
		 * If not already configured for triple chg, configure master
		 * SMB1390 here for triple chg, if slave is detected.
		 */
		if (is_cps_available(chip) && !chip->smb_init_done) {
			smb1390_dbg(chip, PR_INFO, "SMB1390 slave has registered, configure for triple charging\n");
			rc = smb1390_triple_init_hw(chip);
			if (rc < 0)
				pr_err("Couldn't configure SMB1390 for triple-chg config rc=%d\n",
					rc);
		}
	}

	return NOTIFY_OK;
}

#define ILIM_NR			10
#define ILIM_DR			8
#define ILIM_FACTOR(ilim)	((ilim * ILIM_NR) / ILIM_DR)

static void smb1390_configure_ilim(struct smb1390 *chip, int mode)
{
	int rc;
	union power_supply_propval pval = {0, };

	/* PPS adapter reply on the current advertised by the adapter */
	if ((chip->pl_output_mode == POWER_SUPPLY_PL_OUTPUT_VPH)
			&& (mode == POWER_SUPPLY_CP_PPS)) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PD_CURRENT_MAX, &pval);
		if (rc < 0)
			pr_err("Couldn't get PD CURRENT MAX rc=%d\n", rc);
		else
			vote(chip->ilim_votable, ICL_VOTER,
					true, ILIM_FACTOR(pval.intval));
	}

	/* QC3.0/Wireless adapter rely on the settled AICL for USBMID_USBMID */
	if ((chip->pl_input_mode == POWER_SUPPLY_PL_USBMID_USBMID)
			&& (mode == POWER_SUPPLY_CP_HVDCP3)) {
		if (!chip->fcc_main_votable)
			chip->fcc_main_votable = find_votable("FCC_MAIN");

		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &pval);
		if (rc < 0) {
			pr_err("Couldn't get usb aicl rc=%d\n", rc);
		} else {
			vote(chip->ilim_votable, ICL_VOTER, true, pval.intval);
			/*
			 * Rerun FCC votable to ensure offset for ILIM
			 * compensation is recalculated based on new ILIM.
			 */
			if (chip->fcc_main_votable)
				rerun_election(chip->fcc_main_votable);
		}
	}
}

static void smb1390_status_change_work(struct work_struct *work)
{
	struct smb1390 *chip = container_of(work, struct smb1390,
					    status_change_work);
	union power_supply_propval pval = {0, };
	int rc, dc_current_max = 0;

	if (!is_psy_voter_available(chip))
		goto out;

	/*
	 * If batt soc is not valid upon bootup, but becomes
	 * valid due to the battery discharging later, remove
	 * vote from SOC_LEVEL_VOTER.
	 */
	if (smb1390_is_batt_soc_valid(chip))
		vote(chip->disable_votable, SOC_LEVEL_VOTER, false, 0);

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

		/*
		 * Slave SMB1390 is not required for the power-rating of QC3
		 */
		if (pval.intval != POWER_SUPPLY_CP_HVDCP3)
			vote(chip->slave_disable_votable, SRC_VOTER, false, 0);

		/* Check for SOC threshold only once before enabling CP */
		vote(chip->disable_votable, SRC_VOTER, false, 0);
		if (!chip->batt_soc_validated) {
			vote(chip->disable_votable, SOC_LEVEL_VOTER,
				smb1390_is_batt_soc_valid(chip) ?
				false : true, 0);
			chip->batt_soc_validated = true;
		}

		if (pval.intval == POWER_SUPPLY_CP_WIRELESS) {
			vote(chip->ilim_votable, ICL_VOTER, false, 0);
			rc = power_supply_get_property(chip->dc_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
			if (rc < 0) {
				pr_err("Couldn't get dc icl rc=%d\n", rc);
			} else {
				dc_current_max = pval.intval;

				rc = power_supply_get_property(chip->dc_psy,
						POWER_SUPPLY_PROP_AICL_DONE,
						&pval);
				if (rc < 0)
					pr_err("Couldn't get aicl done rc=%d\n",
							rc);
				else if (pval.intval)
					vote(chip->ilim_votable, WIRELESS_VOTER,
							true, dc_current_max);
			}
		} else {
			vote(chip->ilim_votable, WIRELESS_VOTER, false, 0);
			smb1390_configure_ilim(chip, pval.intval);
		}

		/*
		 * Remove SMB1390 Taper condition disable vote if float voltage
		 * increased in comparison to voltage at which it entered taper.
		 */
		if (chip->taper_entry_fv <
				get_effective_result(chip->fv_votable))
			vote(chip->disable_votable, TAPER_END_VOTER, false, 0);

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
		chip->batt_soc_validated = false;
		vote(chip->slave_disable_votable, SRC_VOTER, true, 0);
		vote(chip->disable_votable, SRC_VOTER, true, 0);
		vote(chip->disable_votable, TAPER_END_VOTER, false, 0);
		vote(chip->fcc_votable, CP_VOTER, false, 0);
		vote(chip->disable_votable, SOC_LEVEL_VOTER, true, 0);
		vote_override(chip->ilim_votable, CC_MODE_VOTER, false, 0);
		vote(chip->ilim_votable, WIRELESS_VOTER, false, 0);
		vote(chip->slave_disable_votable, TAPER_END_VOTER, false, 0);
		vote(chip->slave_disable_votable, MAIN_DISABLE_VOTER, true, 0);
		vote_override(chip->usb_icl_votable, TAPER_MAIN_ICL_LIMIT_VOTER,
								false, 0);
	}

out:
	pm_relax(chip->dev);
	chip->status_change_running = false;
}

static int smb1390_validate_slave_chg_taper(struct smb1390 *chip, int fcc_uA)
{
	int rc = 0;

	/*
	 * In Collapse mode, while in Taper, Disable the slave SMB1390
	 * when FCC drops below a specified threshold.
	 */
	if (fcc_uA < (chip->cp_slave_thr_taper_ua) && is_cps_available(chip)) {
		vote(chip->slave_disable_votable, TAPER_END_VOTER,
					true, 0);
		/*
		 * Set ILIM of master CP to Max value = 3.2A once slave is
		 * disabled to prevent ILIM irq storm.
		 */
		smb1390_dbg(chip, PR_INFO, "Set Master ILIM to MAX, post Slave disable in taper, fcc=%d\n",
									fcc_uA);
		vote_override(chip->ilim_votable, CC_MODE_VOTER,
				smb1390_is_adapter_cc_mode(chip),
				MAX_ILIM_DUAL_CP_UA);

		if (chip->usb_icl_votable)
			vote_override(chip->usb_icl_votable,
				      TAPER_MAIN_ICL_LIMIT_VOTER,
				      smb1390_is_adapter_cc_mode(chip),
				      chip->cc_mode_taper_main_icl_ua);
	}

	return rc;
}

static void smb1390_taper_work(struct work_struct *work)
{
	struct smb1390 *chip = container_of(work, struct smb1390, taper_work);
	union power_supply_propval pval = {0, };
	int rc, fcc_uA, delta_fcc_uA, main_fcc_ua = 0;

	if (!is_psy_voter_available(chip))
		goto out;

	if (!chip->fcc_main_votable)
		chip->fcc_main_votable = find_votable("FCC_MAIN");

	if (chip->fcc_main_votable)
		main_fcc_ua = get_effective_result(chip->fcc_main_votable);

	if (main_fcc_ua < 0)
		main_fcc_ua = 0;

	chip->taper_entry_fv = get_effective_result(chip->fv_votable);
	while (true) {
		rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get charge type rc=%d\n", rc);
			goto out;
		}

		if (get_effective_result(chip->fv_votable) >
						chip->taper_entry_fv) {
			smb1390_dbg(chip, PR_INFO, "Float voltage increased. Exiting taper\n");
			goto out;
		} else {
			chip->taper_entry_fv =
					get_effective_result(chip->fv_votable);
		}

		if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
			delta_fcc_uA =
				(smb1390_is_adapter_cc_mode(chip) ?
							CC_MODE_TAPER_DELTA_UA :
							DEFAULT_TAPER_DELTA_UA);
			fcc_uA = get_effective_result(chip->fcc_votable)
								- delta_fcc_uA;
			smb1390_dbg(chip, PR_INFO, "taper work reducing FCC to %duA\n",
				fcc_uA);
			vote(chip->fcc_votable, CP_VOTER, true, fcc_uA);
			rc = smb1390_validate_slave_chg_taper(chip, (fcc_uA -
							      main_fcc_ua));
			if (rc < 0) {
				pr_err("Couldn't Disable slave in Taper, rc=%d\n",
				       rc);
				goto out;
			}

			if ((fcc_uA - main_fcc_ua) < (chip->min_ilim_ua * 2)) {
				vote(chip->disable_votable, TAPER_END_VOTER,
								true, 0);
				/*
				 * When master CP is disabled, reset all votes
				 * on ICL to enable Main charger to pump
				 * charging current.
				 */
				if (chip->usb_icl_votable)
					vote_override(chip->usb_icl_votable,
						TAPER_MAIN_ICL_LIMIT_VOTER,
						false, 0);
				goto out;
			}
		} else {
			smb1390_dbg(chip, PR_INFO, "In fast charging. Wait for next taper\n");
		}

		msleep(500);
	}
out:
	smb1390_dbg(chip, PR_INFO, "taper work exit\n");
	vote(chip->fcc_votable, CP_VOTER, false, 0);
	chip->taper_work_running = false;
}

static enum power_supply_property smb1390_charge_pump_props[] = {
	POWER_SUPPLY_PROP_CP_STATUS1,
	POWER_SUPPLY_PROP_CP_STATUS2,
	POWER_SUPPLY_PROP_CP_ENABLE,
	POWER_SUPPLY_PROP_CP_SWITCHER_EN,
	POWER_SUPPLY_PROP_CP_DIE_TEMP,
	POWER_SUPPLY_PROP_CP_ISNS,
	POWER_SUPPLY_PROP_CP_ISNS_SLAVE,
	POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER,
	POWER_SUPPLY_PROP_CP_IRQ_STATUS,
	POWER_SUPPLY_PROP_CP_ILIM,
	POWER_SUPPLY_PROP_CHIP_VERSION,
	POWER_SUPPLY_PROP_PARALLEL_OUTPUT_MODE,
	POWER_SUPPLY_PROP_MIN_ICL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
};

static int smb1390_get_prop_suspended(struct smb1390 *chip,
				enum power_supply_property prop,
				union power_supply_propval *val)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CP_STATUS1:
		val->intval = chip->cp_status1;
		break;
	case POWER_SUPPLY_PROP_CP_STATUS2:
		val->intval = chip->cp_status2;
		break;
	case POWER_SUPPLY_PROP_CP_ENABLE:
		val->intval = chip->cp_enable;
		break;
	case POWER_SUPPLY_PROP_CP_SWITCHER_EN:
		val->intval = chip->switcher_enabled;
		break;
	case POWER_SUPPLY_PROP_CP_DIE_TEMP:
		val->intval = chip->die_temp;
		break;
	case POWER_SUPPLY_PROP_CP_ISNS:
		val->intval = chip->cp_isns_master;
		break;
	case POWER_SUPPLY_PROP_CP_ISNS_SLAVE:
		val->intval = chip->cp_isns_slave;
		break;
	case POWER_SUPPLY_PROP_CP_IRQ_STATUS:
		val->intval = chip->irq_status;
		break;
	case POWER_SUPPLY_PROP_CP_ILIM:
		val->intval = chip->cp_ilim;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smb1390_get_prop(struct power_supply *psy,
			enum power_supply_property prop,
			union power_supply_propval *val)
{
	struct smb1390 *chip = power_supply_get_drvdata(psy);
	int rc = 0, status;
	bool enable;

	/*
	 * Return the cached values when the system is in suspend state
	 * instead of reading the registers to avoid read failures.
	 */
	if (chip->suspended) {
		rc = smb1390_get_prop_suspended(chip, prop, val);
		if (!rc)
			return rc;
		rc = 0;
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CP_STATUS1:
		rc = smb1390_read(chip, CORE_STATUS1_REG, &status);
		if (!rc)
			chip->cp_status1 = val->intval = status;
		break;
	case POWER_SUPPLY_PROP_CP_STATUS2:
		rc = smb1390_read(chip, CORE_STATUS2_REG, &status);
		if (!rc)
			chip->cp_status2 = val->intval = status;
		break;
	case POWER_SUPPLY_PROP_CP_ENABLE:
		rc = smb1390_get_cp_en_status(chip, SMB_PIN_EN,
					      &enable);
		if (!rc)
			chip->cp_enable = val->intval = enable &&
			!get_effective_result(chip->disable_votable);
		break;
	case POWER_SUPPLY_PROP_CP_SWITCHER_EN:
		rc = smb1390_get_cp_en_status(chip, SWITCHER_EN,
					      &enable);
		if (!rc)
			val->intval = enable;
		break;
	case POWER_SUPPLY_PROP_CP_DIE_TEMP:
		/*
		 * Add a filter to the die temp value read:
		 * If temp > THERMAL_SUSPEND_DECIDEGC then
		 *	- treat it as an error and report last valid
		 *	  cached temperature.
		 *	- return -ENODATA if the cached value is
		 *	  invalid.
		 */

		rc = smb1390_get_die_temp(chip, val);
		if (rc >= 0) {
			if (val->intval <= THERMAL_SUSPEND_DECIDEGC)
				chip->die_temp = val->intval;
			else if (chip->die_temp == -ENODATA)
				rc = -ENODATA;
			else
				val->intval = chip->die_temp;
		}
		break;
	case POWER_SUPPLY_PROP_CP_ISNS:
		rc = smb1390_get_isns_master(chip, val);
		if (!rc)
			chip->cp_isns_master = val->intval;
		break;
	case POWER_SUPPLY_PROP_CP_ISNS_SLAVE:
		rc = smb1390_get_isns_slave(chip, val);
		if (!rc)
			chip->cp_isns_slave = val->intval;
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
		rc = smb1390_get_cp_ilim(chip, val);
		if (!rc)
			chip->cp_ilim = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHIP_VERSION:
		val->intval = chip->pmic_rev_id->rev4;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_OUTPUT_MODE:
		val->intval = chip->pl_output_mode;
		break;
	case POWER_SUPPLY_PROP_MIN_ICL:
		val->intval = chip->min_ilim_ua;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = (chip->pmic_rev_id->rev4 > 2) ? "SMB1390_V3" :
								"SMB1390_V2";
		break;
	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		val->intval = chip->pl_input_mode;
		break;
	default:
		smb1390_dbg(chip, PR_MISC, "charge pump power supply get prop %d not supported\n",
			prop);
		rc = -EINVAL;
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
	case POWER_SUPPLY_PROP_CP_ILIM:
		if (chip->ilim_votable)
			vote_override(chip->ilim_votable, CC_MODE_VOTER,
					(val->intval > 0), val->intval);
		break;
	default:
		smb1390_dbg(chip, PR_MISC, "charge pump power supply set prop %d not supported\n",
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
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
	case POWER_SUPPLY_PROP_CP_ILIM:
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
					"cp_die_temp channel unavailable %d\n",
					rc);
			chip->iio.die_temp_chan = NULL;
			return rc;
		}
	} else {
		return rc;
	}

	chip->min_ilim_ua = 1000000; /* 1A */
	of_property_read_u32(chip->dev->of_node, "qcom,min-ilim-ua",
			&chip->min_ilim_ua);

	chip->max_temp_alarm_degc = 105;
	of_property_read_u32(chip->dev->of_node, "qcom,max-temp-alarm-degc",
			&chip->max_temp_alarm_degc);

	chip->max_cutoff_soc = 85; /* 85% */
	of_property_read_u32(chip->dev->of_node, "qcom,max-cutoff-soc",
			&chip->max_cutoff_soc);

	/* Default parallel output configuration is VPH connection */
	chip->pl_output_mode = POWER_SUPPLY_PL_OUTPUT_VPH;
	of_property_read_u32(chip->dev->of_node, "qcom,parallel-output-mode",
			&chip->pl_output_mode);

	/* Default parallel input configuration is USBMID connection */
	chip->pl_input_mode = POWER_SUPPLY_PL_USBMID_USBMID;
	of_property_read_u32(chip->dev->of_node, "qcom,parallel-input-mode",
			&chip->pl_input_mode);

	chip->cp_slave_thr_taper_ua = 3 * chip->min_ilim_ua;
	of_property_read_u32(chip->dev->of_node, "qcom,cp-slave-thr-taper-ua",
			      &chip->cp_slave_thr_taper_ua);

	chip->cc_mode_taper_main_icl_ua = CC_MODE_TAPER_MAIN_ICL_UA;
	of_property_read_u32(chip->dev->of_node,
			     "qcom,cc-mode-taper-main-icl-ua",
			     &chip->cc_mode_taper_main_icl_ua);

	return 0;
}

static void smb1390_release_channels(struct smb1390 *chip)
{
	if (!IS_ERR_OR_NULL(chip->iio.die_temp_chan))
		iio_channel_release(chip->iio.die_temp_chan);
}

static int smb1390_create_votables(struct smb1390 *chip)
{
	chip->cp_awake_votable = create_votable("CP_AWAKE",
			VOTE_SET_ANY, smb1390_awake_vote_cb, chip);
	chip->disable_votable = create_votable("CP_DISABLE",
			VOTE_SET_ANY, smb1390_disable_vote_cb, chip);
	if (IS_ERR(chip->disable_votable))
		return PTR_ERR(chip->disable_votable);

	chip->ilim_votable = create_votable("CP_ILIM",
			VOTE_MIN, smb1390_ilim_vote_cb, chip);
	if (IS_ERR(chip->ilim_votable))
		return PTR_ERR(chip->ilim_votable);

	chip->slave_disable_votable = create_votable("CP_SLAVE_DISABLE",
			VOTE_SET_ANY, smb1390_slave_disable_vote_cb, chip);
	if (IS_ERR(chip->slave_disable_votable))
		return PTR_ERR(chip->slave_disable_votable);

	/* Keep slave SMB disabled */
	vote(chip->slave_disable_votable, SRC_VOTER, true, 0);
	/*
	 * charge pump is initially disabled; this indirectly votes to allow
	 * traditional parallel charging if present
	 */
	vote(chip->disable_votable, USER_VOTER, true, 0);
	/* keep charge pump disabled if SOC is above threshold */
	vote(chip->disable_votable, SOC_LEVEL_VOTER,
			smb1390_is_batt_soc_valid(chip) ? false : true, 0);

	/*
	 * In case SMB1390 probe happens after FCC value has been configured,
	 * update ilim vote to reflect FCC / 2 value, this is only applicable
	 * when SMB1390 is directly connected to VBAT.
	 */
	if ((chip->pl_output_mode != POWER_SUPPLY_PL_OUTPUT_VPH)
			&& chip->fcc_votable)
		vote(chip->ilim_votable, FCC_VOTER, true,
			get_effective_result(chip->fcc_votable) / 2);

	return 0;
}

static void smb1390_destroy_votables(struct smb1390 *chip)
{
	destroy_votable(chip->disable_votable);
	destroy_votable(chip->ilim_votable);
}

static int smb1390_init_hw(struct smb1390 *chip)
{
	int rc = 0, val;

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

	switch (chip->max_temp_alarm_degc) {
	case 115:
		val = 0x00;
		break;
	case 90:
		val = 0x02;
		break;
	case 80:
		val = 0x03;
		break;
	case 105:
	default:
		val = 0x01;
		break;
	}
	rc = smb1390_masked_write(chip, CORE_FTRIM_CTRL_REG,
			TEMP_ALERT_LVL_MASK, val << TEMP_ALERT_LVL_SHIFT);
	if (rc < 0) {
		pr_err("Failed to write CORE_FTRIM_CTRL_REG rc=%d\n", rc);
		return rc;
	}

	/* Configure IREV threshold to 200mA */
	rc = smb1390_masked_write(chip, CORE_FTRIM_MISC_REG, TR_IREV_BIT, 0);
	if (rc < 0) {
		pr_err("Couldn't configure IREV threshold rc=%d\n", rc);
		return rc;
	}
	/*
	 * If the slave charger has registered, configure Master SMB1390 for
	 * triple-chg config, else configure for dual. Later, if the slave
	 * charger registers, re-configure for triple chg config from the
	 * power-supply notifier.
	 */
	if (!chip->smb_init_done) {
		if (is_cps_available(chip))
			rc = smb1390_triple_init_hw(chip);
		else
			rc = smb1390_dual_init_hw(chip);
	}

	return rc;
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

#ifdef CONFIG_DEBUG_FS
static void smb1390_create_debugfs(struct smb1390 *chip)
{
	struct dentry *entry;

	chip->dfs_root = debugfs_create_dir("smb1390_charger_psy", NULL);
	if (IS_ERR_OR_NULL(chip->dfs_root)) {
		pr_err("Failed to create debugfs directory, rc=%ld\n",
					(long)chip->dfs_root);
		return;
	}

	entry = debugfs_create_u32("debug_mask", 0600, chip->dfs_root,
			&chip->debug_mask);
	if (IS_ERR_OR_NULL(entry)) {
		pr_err("Failed to create debug_mask, rc=%ld\n", (long)entry);
		debugfs_remove_recursive(chip->dfs_root);
	}
}
#else
static void smb1390_create_debugfs(struct smb1390 *chip)
{
}
#endif

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,smb1390-charger-psy",
	  .data = (void *)CP_MASTER
	},
	{ .compatible = "qcom,smb1390-slave",
	  .data = (void *)CP_SLAVE
	},
	{ },
};

static int smb1390_master_probe(struct smb1390 *chip)
{
	int rc;
	struct device_node *revid_dev_node;
	struct pmic_revid_data *pmic_rev_id;

	revid_dev_node = of_parse_phandle(chip->dev->of_node,
					  "qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property\n");
		return -EINVAL;
	}

	pmic_rev_id = get_revid_data(revid_dev_node);
	of_node_put(revid_dev_node);
	if (IS_ERR_OR_NULL(pmic_rev_id)) {
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	chip->pmic_rev_id = pmic_rev_id;
	spin_lock_init(&chip->status_change_lock);
	mutex_init(&chip->die_chan_lock);

	rc = smb1390_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	chip->cp_ws = wakeup_source_register(NULL, "qcom-chargepump");
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

	smb1390_dbg(chip, PR_INFO, "Detected revid=0x%02x\n",
			 chip->pmic_rev_id->rev4);
	if (chip->pmic_rev_id->rev4 <= 0x02 && chip->pl_output_mode !=
			POWER_SUPPLY_PL_OUTPUT_VPH) {
		pr_err("Incompatible SMB1390 HW detected, Disabling the charge pump\n");
		if (chip->disable_votable)
			vote(chip->disable_votable, HW_DISABLE_VOTER,
			     true, 0);
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

	smb1390_create_debugfs(chip);
	return 0;

out_notifier:
	power_supply_unreg_notifier(&chip->nb);
out_votables:
	smb1390_destroy_votables(chip);
out_work:
	cancel_work_sync(&chip->taper_work);
	cancel_work_sync(&chip->status_change_work);
	wakeup_source_unregister(chip->cp_ws);
	return rc;
}

static enum power_supply_property smb1390_cp_slave_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
};

static int smb1390_slave_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property prop)
{
	return 0;
}

static int smb1390_cp_slave_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb1390 *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		val->intval = 0;
		if (!chip->ilim_votable)
			chip->ilim_votable = find_votable("CP_ILIM");
		if (chip->ilim_votable)
			val->intval =
				get_effective_result_locked(chip->ilim_votable);
		break;
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		val->intval = (int)chip->current_capability;
		break;
	default:
		smb1390_dbg(chip, PR_MISC, "SMB 1390 slave power supply get prop %d not supported\n",
			psp);
		return -EINVAL;
	}

	return 0;
}

static int smb1390_cp_slave_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb1390 *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		rc = smb1390_set_ilim(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		chip->current_capability = (enum isns_mode)val->intval;
		rc = smb1390_isns_mode_control(chip, val->intval);
		break;
	default:
		smb1390_dbg(chip, PR_MISC, "SMB 1390 slave power supply set prop %d not supported\n",
			psp);
		return -EINVAL;
	}

	return 0;
};

static const struct power_supply_desc cps_psy_desc = {
	.name = "cp_slave",
	.type = POWER_SUPPLY_TYPE_PARALLEL,
	.properties = smb1390_cp_slave_props,
	.num_properties = ARRAY_SIZE(smb1390_cp_slave_props),
	.get_property = smb1390_cp_slave_get_prop,
	.set_property = smb1390_cp_slave_set_prop,
	.property_is_writeable = smb1390_slave_prop_is_writeable,
};

static int smb1390_init_cps_psy(struct smb1390 *chip)
{
	struct power_supply_config cps_cfg = {};

	cps_cfg.drv_data = chip;
	cps_cfg.of_node = chip->dev->of_node;
	chip->cps_psy = devm_power_supply_register(chip->dev,
						  &cps_psy_desc,
						  &cps_cfg);
	if (IS_ERR(chip->cps_psy)) {
		pr_err("Couldn't register CP slave power supply\n");
		return PTR_ERR(chip->cps_psy);
	}

	return 0;
}

static int smb1390_slave_probe(struct smb1390 *chip)
{
	int stat, rc;

	/* a "hello" read to test the presence of the slave PMIC */
	rc = smb1390_read(chip, CORE_STATUS1_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't find slave SMB1390\n");
		return -EINVAL;
	}

	rc = smb1390_triple_init_hw(chip);
	if (rc < 0)
		return rc;

	/* Configure Slave CP Temp buffer O/P to High Impedance */
	rc = smb1390_masked_write(chip, CORE_FTRIM_CTRL_REG,
				  TEMP_BUFFER_OUTPUT_BIT,
				  TEMP_BUFFER_OUTPUT_BIT);
	if (rc < 0) {
		pr_err("Couldn't configure Slave temp Buffer rc=%d\n", rc);
		return rc;
	}

	rc = smb1390_init_cps_psy(chip);
	if (rc < 0)
		pr_err("Couldn't initialize cps psy rc=%d\n", rc);

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
	chip->die_temp = -ENODATA;
	chip->disabled = true;

	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		pr_err("Couldn't get regmap\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, chip);
	chip->cp_role = (int)of_device_get_match_data(chip->dev);
	switch (chip->cp_role) {
	case CP_MASTER:
		rc = smb1390_master_probe(chip);
		break;
	case CP_SLAVE:
		rc = smb1390_slave_probe(chip);
		break;
	default:
		pr_err("Couldn't find a matching role %d\n", chip->cp_role);
		rc = -EINVAL;
		goto cleanup;
	}

	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't probe SMB1390 %s rc=%d\n",
			       chip->cp_role ? "Slave" : "Master", rc);
		goto cleanup;
	}

	pr_info("smb1390 %s probed successfully\n", chip->cp_role ? "Slave" :
		"Master");
	return 0;

cleanup:
	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int smb1390_remove(struct platform_device *pdev)
{
	struct smb1390 *chip = platform_get_drvdata(pdev);

	if (chip->cp_role !=  CP_MASTER) {
		platform_set_drvdata(pdev, NULL);
		return 0;
	}

	power_supply_unreg_notifier(&chip->nb);

	/* explicitly disable charging */
	vote(chip->disable_votable, USER_VOTER, true, 0);
	vote(chip->disable_votable, SOC_LEVEL_VOTER, true, 0);
	cancel_work_sync(&chip->taper_work);
	cancel_work_sync(&chip->status_change_work);
	wakeup_source_unregister(chip->cp_ws);
	smb1390_destroy_votables(chip);
	smb1390_release_channels(chip);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void smb1390_shutdown(struct platform_device *pdev)
{
	struct smb1390 *chip = platform_get_drvdata(pdev);
	int rc;

	power_supply_unreg_notifier(&chip->nb);
	/* Disable SMB1390 */
	smb1390_dbg(chip, PR_MISC, "Disabling SMB1390\n");
	rc = smb1390_masked_write(chip, CORE_CONTROL1_REG,
					CMD_EN_SWITCHER_BIT, 0);
	if (rc < 0)
		pr_err("Couldn't disable chip rc=%d\n", rc);
}

static int smb1390_suspend(struct device *dev)
{
	struct smb1390 *chip = dev_get_drvdata(dev);

	chip->suspended = true;
	return 0;
}

static int smb1390_resume(struct device *dev)
{
	struct smb1390 *chip = dev_get_drvdata(dev);

	chip->suspended = false;

	/* ILIM rerun is applicable for both master and slave */
	if (!chip->ilim_votable)
		chip->ilim_votable = find_votable("CP_ILIM");

	if (chip->ilim_votable)
		rerun_election(chip->ilim_votable);

	/* Run disable votable for master only */
	if (chip->cp_role == CP_MASTER)
		rerun_election(chip->disable_votable);

	return 0;
}

static const struct dev_pm_ops smb1390_pm_ops = {
	.suspend	= smb1390_suspend,
	.resume		= smb1390_resume,
};

static struct platform_driver smb1390_driver = {
	.driver	= {
		.name		= "qcom,smb1390-charger-psy",
		.pm		= &smb1390_pm_ops,
		.of_match_table	= match_table,
	},
	.probe	= smb1390_probe,
	.remove	= smb1390_remove,
	.shutdown	= smb1390_shutdown,
};
module_platform_driver(smb1390_driver);

MODULE_DESCRIPTION("SMB1390 Charge Pump Driver");
MODULE_LICENSE("GPL v2");
