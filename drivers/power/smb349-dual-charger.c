/* Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "SMB349-dual %s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mutex.h>
#include "smb349-charger.h"

#define LOW_CHARGE_DELAY_MS		2000
#define PERIODIC_DELAY_MS		120000
#define LOW_CHARGE_CURRENT_MA		200
#define SMB_RECHARGE_THRESHOLD_MV	50
#define DELAY_IRQ_LEVEL_MS		20

enum wake_reason {
	PM_SMB349_IRQ_HANDLING = BIT(0),
	PM_SMB349_DC_INSERTED = BIT(1),
};

struct smb349_dual_charger {
	struct i2c_client	*client;
	struct device		*dev;

	enum wake_reason	wake_reasons;
	bool			recharge_disabled;
	int			recharge_mv;
	bool			iterm_disabled;
	int			iterm_ma;
	int			vfloat_mv;
	int			chg_stat_gpio;
	int			chg_present;
	const char		*ext_psy_name;

	bool			charging_disabled;
	int			fastchg_current_max_ma;

	struct delayed_work	irq_handler_dwork;
	struct delayed_work	periodic_charge_handler_dwork;

	struct power_supply	*ext_psy;
	struct power_supply	cradle_psy;

	struct mutex		irq_complete;
	struct mutex		pm_lock;

	struct dentry		*debug_root;
	u32			peek_poke_address;
};

struct smb_irq_info {
	const char *name;
	int (*smb_irq)(struct smb349_dual_charger *chip, u8 rt_stat);
	int high;
	int low;
};

struct irq_handler_info {
	u8			stat_reg;
	u8			val;
	u8			prev_val;
	struct smb_irq_info	irq_info[4];
};

struct chg_current_map {
	int	chg_current_ma;
	u8	value;
};

static int smb349_read_reg(struct smb349_dual_charger *chip, int reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int smb349_write_reg(struct smb349_dual_charger *chip, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int smb349_masked_write(struct smb349_dual_charger *chip, int reg,
							u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = smb349_read_reg(chip, reg, &temp);
	if (rc) {
		dev_err(chip->dev,
			"smb349_read_reg Failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = smb349_write_reg(chip, reg, temp);
	if (rc) {
		dev_err(chip->dev,
			"smb349_write Failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	return 0;
}

static int smb349_enable_volatile_writes(struct smb349_dual_charger *chip)
{
	int rc;

	rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_VOLATILE_W_PERM_BIT,
						CMD_A_VOLATILE_W_PERM_BIT);
	if (rc)
		dev_err(chip->dev, "Couldn't write VOLATILE_W_PERM_BIT rc=%d\n",
				rc);

	return rc;
}

static int smb349_fastchg_current_set(struct smb349_dual_charger *chip)
{
	u8 temp;

	if ((chip->fastchg_current_max_ma < SMB349_FAST_CHG_MIN_MA) ||
		(chip->fastchg_current_max_ma >  SMB349_FAST_CHG_MAX_MA)) {
		dev_dbg(chip->dev, "bad fastchg current mA=%d asked to set\n",
					chip->fastchg_current_max_ma);
		return -EINVAL;
	}

	temp = (chip->fastchg_current_max_ma - SMB349_FAST_CHG_MIN_MA)
			/ SMB349_FAST_CHG_STEP_MA;

	temp = temp << SMB349_FAST_CHG_SHIFT;
	dev_dbg(chip->dev, "fastchg limit=%d setting %02x\n",
			chip->fastchg_current_max_ma, temp);

	return smb349_masked_write(chip, CHG_CURRENT_CTRL_REG,
				SMB_FAST_CHG_CURRENT_MASK, temp);
}

static int smb349_float_voltage_set(struct smb349_dual_charger *chip,
								int vfloat_mv)
{
	u8 temp;

	if ((vfloat_mv < MIN_FLOAT_MV) || (vfloat_mv > MAX_FLOAT_MV)) {
		dev_err(chip->dev, "bad float voltage mv =%d asked to set\n",
					vfloat_mv);
		return -EINVAL;
	}

	temp = (vfloat_mv - MIN_FLOAT_MV) / VFLOAT_STEP_MV;

	return smb349_masked_write(chip, VFLOAT_REG, VFLOAT_MASK, temp);
}

static int smb349_hw_init(struct smb349_dual_charger *chip)
{
	int rc;
	u8 reg = 0;

	rc = smb349_enable_volatile_writes(chip);
	if (rc) {
		dev_err(chip->dev, "Couldn't configure volatile writes rc=%d\n",
				rc);
		return rc;
	}

	/* overwrite OTP defaults */
	rc = smb349_masked_write(chip, CHG_CURRENT_CTRL_REG, 0xFF, 0x5F);
	if (rc) {
		dev_err(chip->dev, "Couldn't set current control rc=%d\n",
				rc);
		return rc;
	}
	rc = smb349_masked_write(chip, THERM_A_CTRL_REG, 0xFF, 0xCF);
	if (rc) {
		dev_err(chip->dev, "Couldn't set thermal control rc=%d\n",
				rc);
		return rc;
	}
	rc = smb349_masked_write(chip, OTG_TLIM_THERM_CTRL_REG, 0xFF, 0x5B);
	if (rc) {
		dev_err(chip->dev, "Couldn't set otg thermal limits rc=%d\n",
				rc);
		return rc;
	}
	rc = smb349_masked_write(chip, HARD_SOFT_TLIM_CTRL_REG, 0xFF, 0x23);
	if (rc) {
		dev_err(chip->dev, "Couldn't set thermal limits rc=%d\n",
				rc);
		return rc;
	}

	/* set the fast charge current limit */
	rc = smb349_fastchg_current_set(chip);
	if (rc) {
		dev_err(chip->dev, "Couldn't set fastchg current rc=%d\n", rc);
		return rc;
	}

	/* set the float voltage */
	if (chip->vfloat_mv != -EINVAL) {
		rc = smb349_float_voltage_set(chip, chip->vfloat_mv);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set float voltage rc = %d\n", rc);
			return rc;
		}
	}

	/* set iterm */
	if (chip->iterm_ma != -EINVAL) {
		if (chip->iterm_disabled) {
			dev_err(chip->dev, "Error: Both iterm_disabled and iterm_ma set\n");
			return -EINVAL;
		} else {
			if (chip->iterm_ma <= 100)
				reg = CHG_ITERM_100MA;
			else if (chip->iterm_ma <= 200)
				reg = CHG_ITERM_200MA;
			else if (chip->iterm_ma <= 300)
				reg = CHG_ITERM_300MA;
			else if (chip->iterm_ma <= 400)
				reg = CHG_ITERM_400MA;
			else if (chip->iterm_ma <= 500)
				reg = CHG_ITERM_500MA;
			else if (chip->iterm_ma <= 600)
				reg = CHG_ITERM_600MA;
			else
				reg = CHG_ITERM_700MA;

			rc = smb349_masked_write(chip, CHG_OTH_CURRENT_CTRL_REG,
							CHG_ITERM_MASK, reg);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't set iterm rc = %d\n", rc);
				return rc;
			}

			rc = smb349_masked_write(chip, CHG_CTRL_REG,
						CHG_CTRL_CURR_TERM_END_MASK, 0);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't enable iterm rc = %d\n", rc);
				return rc;
			}
		}
	} else  if (chip->iterm_disabled) {
		rc = smb349_masked_write(chip, CHG_CTRL_REG,
					CHG_CTRL_CURR_TERM_END_MASK,
					CHG_CTRL_CURR_TERM_END_MASK);
		if (rc) {
			dev_err(chip->dev, "Couldn't set iterm rc = %d\n",
								rc);
			return rc;
		}
	}

	/* set recharge-threshold */
	if (chip->recharge_mv != -EINVAL) {
		if (chip->recharge_disabled) {
			dev_err(chip->dev, "Error: Both recharge_disabled and recharge_mv set\n");
			return -EINVAL;
		} else {
			reg = 0;
			if (chip->recharge_mv > SMB_RECHARGE_THRESHOLD_MV)
				reg = CHG_CTRL_RECHG_100MV_BIT;

			rc = smb349_masked_write(chip, CHG_CTRL_REG,
					CHG_CTRL_RECHG_50_100_MASK |
					CHG_CTRL_AUTO_RECHARGE_MASK, reg);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't set rechg-cfg rc = %d\n", rc);
				return rc;
			}
		}
	} else if (chip->recharge_disabled) {
		rc = smb349_masked_write(chip, CHG_CTRL_REG,
				CHG_CTRL_AUTO_RECHARGE_MASK,
				CHG_CTRL_AUTO_RECHARGE_MASK);
		if (rc) {
			dev_err(chip->dev,
				"Couldn't disable auto-rechg rc = %d\n", rc);
			return rc;
		}
	}

	/* enable/disable charging */
	rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_CHG_ENABLE_BIT,
			chip->charging_disabled ? 0 : CMD_A_CHG_ENABLE_BIT);
	if (rc) {
		dev_err(chip->dev, "Unable to %s charging. rc=%d\n",
			chip->charging_disabled ? "disable" : "enable", rc);
	}

	return rc;
}

static enum power_supply_property smb349_cradle_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int smb349_get_prop_cradle_status(struct smb349_dual_charger *chip)
{
	int rc;
	u8 reg = 0;

	if (chip->chg_present && !chip->charging_disabled) {
		rc = smb349_read_reg(chip, STATUS_C_REG, &reg);
		if (rc) {
			dev_dbg(chip->dev, "Couldn't read STAT_C rc = %d\n",
									rc);
			return POWER_SUPPLY_STATUS_UNKNOWN;
		}

		dev_dbg(chip->dev, "%s: STATUS_C_REG=%x\n", __func__, reg);
		if ((reg & STATUS_C_CHARGING_MASK) &&
				!(reg & STATUS_C_CHG_ERR_STATUS_BIT))
			return POWER_SUPPLY_STATUS_CHARGING;
	}

	return POWER_SUPPLY_STATUS_DISCHARGING;
}

static int smb349_get_prop_cradle_present(struct smb349_dual_charger *chip)
{
	return chip->chg_present && !chip->charging_disabled;
}

static int smb349_get_prop_charge_type(struct smb349_dual_charger *chip)
{
	int rc;
	u8 reg = 0;

	if (chip->chg_present && !chip->charging_disabled) {
		rc = smb349_read_reg(chip, STATUS_C_REG, &reg);
		if (rc) {
			dev_err(chip->dev, "Couldn't read STAT_C rc = %d\n",
									rc);
			return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}

		dev_dbg(chip->dev, "%s: STATUS_C_REG=%x\n", __func__, reg);

		reg &= STATUS_C_CHARGING_MASK;

		if ((reg == STATUS_C_FAST_CHARGING)
			|| (reg == STATUS_C_TAPER_CHARGING))
			return POWER_SUPPLY_CHARGE_TYPE_FAST;
		else if (reg == STATUS_C_PRE_CHARGING)
			return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	}

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int smb349_get_charging_status(struct smb349_dual_charger *chip)
{
	int rc;
	u8 reg = 0;

	if (chip->chg_present && !chip->charging_disabled) {
		rc = smb349_read_reg(chip, STATUS_C_REG, &reg);
		if (rc) {
			dev_err(chip->dev, "Couldn't read STAT_C rc = %d\n",
									rc);
			return 0;
		}

		return (reg & STATUS_C_CHG_ENABLE_STATUS_BIT) ? 1 : 0;
	}

	return 0;
}

static int smb349_charging(struct smb349_dual_charger *chip, int enable)
{
	int rc = 0;

	dev_dbg(chip->dev, "%s: charging enable = %d\n", __func__, enable);

	if (chip->chg_present) {
		rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_CHG_ENABLE_BIT,
					enable ? CMD_A_CHG_ENABLE_BIT : 0);
		if (rc)
			dev_err(chip->dev, "Couldn't enable = %d rc = %d\n",
					enable, rc);
		dev_dbg(chip->dev, "cradle psy changed\n");
		power_supply_changed(&chip->cradle_psy);
	}

	chip->charging_disabled = !enable;
	return rc;
}

static int
smb349_cradle_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return 1;
	default:
		break;
	}

	return 0;
}

static int smb349_cradle_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct smb349_dual_charger *chip = container_of(psy,
				struct smb349_dual_charger, cradle_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smb349_charging(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smb349_cradle_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb349_dual_charger *chip = container_of(psy,
				struct smb349_dual_charger, cradle_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb349_get_prop_cradle_status(chip);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = smb349_get_prop_cradle_present(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb349_get_prop_cradle_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = smb349_get_charging_status(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb349_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "SMB349 Dual Charger";
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fast_chg(struct smb349_dual_charger *chip, u8 status)
{
	dev_dbg(chip->dev, "%s\n", __func__);
	return 0;
}

static int chg_term(struct smb349_dual_charger *chip, u8 status)
{
	dev_dbg(chip->dev, "%s\n", __func__);
	return 0;
}

static int chg_type_start(struct smb349_dual_charger *chip, u8 status)
{
	dev_dbg(chip->dev, "%s status=%d\n", __func__, status);
	return 0;
}

static int chg_type_taper(struct smb349_dual_charger *chip, u8 status)
{
	dev_dbg(chip->dev, "%s status=%d\n", __func__, status);
	return 0;
}

static int chg_type_recharge(struct smb349_dual_charger *chip, u8 status)
{
	dev_dbg(chip->dev, "%s status=%d\n", __func__, status);
	return 0;
}

static struct irq_handler_info handlers[] = {
	[0] = {
		.stat_reg	= IRQ_A_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{
				.name		= "cold_soft",
			},
			{
				.name		= "hot_soft",
			},
			{
				.name		= "cold_hard",
			},
			{
				.name		= "hot_hard",
			},
		},
	},
	[1] = {
		.stat_reg	= IRQ_B_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{
				.name		= "fast_chg",
				.smb_irq	= fast_chg,
			},
			{
				.name		= "vbat_low",
			},
			{
				.name		= "battery_missing",
			},
			{
				.name		= "battery_ov",
			},
		},
	},
	[2] = {
		.stat_reg	= IRQ_C_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{
				.name		= "chg_term",
				.smb_irq	= chg_term,
			},
			{
				.name		= "taper",
				.smb_irq	= chg_type_taper,
			},
			{
				.name		= "recharge",
				.smb_irq	= chg_type_recharge,
			},
			{
				.name		= "chg_hot",
			},
		},
	},
	[3] = {
		.stat_reg	= IRQ_D_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{
				.name		= "prechg_timeout",
			},
			{
				.name		= "safety_timeout",
			},
			{
				.name		= "aicl_complete",
				.smb_irq	= chg_type_start,
			},
			{
				.name		= "src_detect",
			},
		},
	},
	[4] = {
		.stat_reg	= IRQ_E_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{
				.name		= "dcin_uv",
			},
			{
				.name		= "dcin_ov",
			},
			{
				.name		= "afvc_active",
			},
			{
				.name		= "unknown",
			},
		},
	},
	[5] = {
		.stat_reg	= IRQ_F_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{
				.name		= "power_ok",
			},
			{
				.name		= "otg_det",
			},
			{
				.name		= "otg_batt_uv",
			},
			{
				.name		= "otg_oc",
			},
		},
	},
};

static void smb349_pm_stay_awake(struct smb349_dual_charger *chip, int reason)
{
	int reasons;

	mutex_lock(&chip->pm_lock);
	reasons = chip->wake_reasons | reason;
	if (reasons != 0 && chip->wake_reasons == 0) {
		dev_dbg(chip->dev, "staying awake: 0x%02x (bit %d)\n",
			reasons, reason);
		pm_stay_awake(chip->dev);
	}
	chip->wake_reasons = reasons;
	mutex_unlock(&chip->pm_lock);
}

static void smb349_pm_relax(struct smb349_dual_charger *chip, int reason)
{
	int reasons;

	mutex_lock(&chip->pm_lock);
	reasons = chip->wake_reasons & (~reason);
	if (reasons == 0 && chip->wake_reasons != 0) {
		dev_dbg(chip->dev, "relaxing: 0x%02x (bit %d)\n",
			reasons, reason);
		pm_relax(chip->dev);
	}
	chip->wake_reasons = reasons;
	mutex_unlock(&chip->pm_lock);
}

static irqreturn_t smb349_chg_stat_handler(int irq, void *dev_id)
{
	struct smb349_dual_charger *chip = dev_id;

	smb349_pm_stay_awake(chip, PM_SMB349_IRQ_HANDLING);
	schedule_delayed_work(&chip->irq_handler_dwork,
		round_jiffies_relative(msecs_to_jiffies(DELAY_IRQ_LEVEL_MS)));

	return IRQ_HANDLED;
}

static void handle_stat_irqs(struct smb349_dual_charger *chip)
{
	int i, j;
	u8 triggered;
	u8 changed;
	u8 rt_stat, prev_rt_stat;
	int rc;
	int handler_count = 0;

	dev_dbg(chip->dev, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		rc = smb349_read_reg(chip, handlers[i].stat_reg,
						&handlers[i].val);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read %d rc = %d\n",
					handlers[i].stat_reg, rc);
			continue;
		}

		for (j = 0; j < ARRAY_SIZE(handlers[i].irq_info); j++) {
			triggered = handlers[i].val
			       & (IRQ_LATCHED_MASK << (j * BITS_PER_IRQ));
			rt_stat = handlers[i].val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			prev_rt_stat = handlers[i].prev_val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			changed = prev_rt_stat ^ rt_stat;

			if (triggered || changed)
				rt_stat ? handlers[i].irq_info[j].high++ :
						handlers[i].irq_info[j].low++;

			if ((triggered || changed)
				&& handlers[i].irq_info[j].smb_irq != NULL) {
				handler_count++;
				rc = handlers[i].irq_info[j].smb_irq(chip,
								rt_stat);
				if (rc < 0)
					dev_err(chip->dev,
						"Couldn't handle %d irq for reg 0x%02x rc = %d\n",
						j, handlers[i].stat_reg, rc);
			}
		}
		handlers[i].prev_val = handlers[i].val;
	}

	pr_debug("handler count = %d\n", handler_count);
	if (handler_count) {
		pr_debug("cradle psy changed\n");
		power_supply_changed(&chip->cradle_psy);
	}
}

static int smb_parse_dt(struct smb349_dual_charger *chip)
{
	int rc;
	enum of_gpio_flags gpio_flags;
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	chip->charging_disabled = of_property_read_bool(node,
					"qcom,charging-disabled");

	rc = of_property_read_string(node, "qcom,ext-psy-name",
						&chip->ext_psy_name);
	if (rc) {
		dev_err(chip->dev, "Invalid qcom,ext-psy-name, rc = %d\n", rc);
		return -EINVAL;
	}

	chip->chg_stat_gpio = of_get_named_gpio_flags(node,
				"qcom,chg-stat-gpio", 0, &gpio_flags);
	if (!gpio_is_valid(chip->chg_stat_gpio)) {
		dev_err(chip->dev, "Invalid qcom,chg-stat-gpio, rc = %d\n",
									rc);
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "qcom,fastchg-current-max-ma",
						&chip->fastchg_current_max_ma);
	if (rc)
		chip->fastchg_current_max_ma = SMB349_FAST_CHG_MAX_MA;

	chip->iterm_disabled = of_property_read_bool(node,
					"qcom,iterm-disabled");

	rc = of_property_read_u32(node, "qcom,iterm-ma", &chip->iterm_ma);
	if (rc < 0)
		chip->iterm_ma = -EINVAL;

	rc = of_property_read_u32(node, "qcom,float-voltage-mv",
						&chip->vfloat_mv);
	if (rc < 0)
		chip->vfloat_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,recharge-mv",
						&chip->recharge_mv);
	if (rc < 0)
		chip->recharge_mv = -EINVAL;

	chip->recharge_disabled = of_property_read_bool(node,
					"qcom,recharge-disabled");

	return 0;
}

static void notify_external_charger(struct smb349_dual_charger *chip, int on)
{
	union power_supply_propval prop = {on,};

	chip->ext_psy->set_property(chip->ext_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &prop);
}

static inline void handle_dc_inout(struct smb349_dual_charger *chip,
								int present)
{
	if (chip->chg_present != present) {
		chip->chg_present = present;
		power_supply_changed(&chip->cradle_psy);

		if (present) {
			dev_dbg(chip->dev, "DC IN\n");
			smb349_hw_init(chip);
			smb349_pm_stay_awake(chip, PM_SMB349_DC_INSERTED);
			schedule_delayed_work(
				&chip->periodic_charge_handler_dwork,
				round_jiffies_relative(
					msecs_to_jiffies(PERIODIC_DELAY_MS)));
			notify_external_charger(chip, 0);
			dev_dbg(chip->dev, "Periodic work started\n");
		} else {
			dev_dbg(chip->dev, "DC OUT\n");
			cancel_delayed_work(
				&chip->periodic_charge_handler_dwork);
			smb349_pm_relax(chip, PM_SMB349_DC_INSERTED);
			notify_external_charger(chip, 1);
			dev_dbg(chip->dev, "Periodic work stopped\n");
		}
	}
}

static int determine_initial_state(struct smb349_dual_charger *chip)
{
	u8 level;

	if (gpio_is_valid(chip->chg_stat_gpio))
		level = gpio_get_value_cansleep(chip->chg_stat_gpio);
	else
		return -EINVAL;

	handle_dc_inout(chip, !level);

	return 0;
}

static void stat_irq_work(struct work_struct *work)
{
	struct smb349_dual_charger *chip = container_of(work,
				struct smb349_dual_charger,
				irq_handler_dwork.work);
	int level;

	mutex_lock(&chip->irq_complete);
	level = gpio_get_value_cansleep(chip->chg_stat_gpio);
	if (level < 0) {
		dev_err(chip->dev, "Couldn't read chg_valid gpio=%d\n",
						chip->chg_stat_gpio);
		mutex_unlock(&chip->irq_complete);
		return;
	}

	dev_dbg(chip->dev, "%s level=%d\n", __func__, level);

	if (chip->chg_present == !level)
		handle_stat_irqs(chip);
	else
		handle_dc_inout(chip, !level);
	smb349_pm_relax(chip, PM_SMB349_IRQ_HANDLING);
	mutex_unlock(&chip->irq_complete);
}

/*
 * There is a PMI Fuel Gauge requirement to lower
 * the Fast Charge current for 2 seconds each 2 minutes by at least 200mA
 */
static void periodic_charge_work(struct work_struct *work)
{
	int save_current, rc;
	struct smb349_dual_charger *chip = container_of(work,
	struct smb349_dual_charger,
	periodic_charge_handler_dwork.work);

	rc = smb349_enable_volatile_writes(chip);
	if (rc) {
		dev_dbg(chip->dev, "Couldn't configure volatile writes rc=%d\n",
			rc);
		goto resched;
	}

	save_current = chip->fastchg_current_max_ma;
	chip->fastchg_current_max_ma -= LOW_CHARGE_CURRENT_MA;

	/* lower the fast charge current limit to allow PMIC FG metering */
	rc = smb349_fastchg_current_set(chip);
	if (rc) {
		dev_dbg(chip->dev, "Couldn't set fastchg current rc=%d\n", rc);
		goto resched;
	}

	/* The required delay to ensure PMI FG detects the transient */
	msleep(LOW_CHARGE_DELAY_MS);

	chip->fastchg_current_max_ma = save_current;
	/* set the fast charge current limit */
	rc = smb349_fastchg_current_set(chip);
	if (rc) {
		dev_dbg(chip->dev, "Couldn't set fastchg current rc=%d\n", rc);
		goto resched;
	}

resched:
	schedule_delayed_work(&chip->periodic_charge_handler_dwork,
		round_jiffies_relative(msecs_to_jiffies(PERIODIC_DELAY_MS)));
	return;
}

static int smb349_dual_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc, irq;
	struct smb349_dual_charger *chip;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Couldn't allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->chg_present = -EINVAL;

	INIT_DELAYED_WORK(&chip->irq_handler_dwork, stat_irq_work);
	INIT_DELAYED_WORK(&chip->periodic_charge_handler_dwork,
							periodic_charge_work);

	rc = smb_parse_dt(chip);
	if (rc) {
		dev_err(&client->dev, "Couldn't parse DT nodes rc=%d\n", rc);
		return rc;
	}

	i2c_set_clientdata(client, chip);

	chip->cradle_psy.name		= "cradle-charger";
	chip->cradle_psy.type		= POWER_SUPPLY_TYPE_MAINS;
	chip->cradle_psy.get_property	= smb349_cradle_get_property;
	chip->cradle_psy.set_property	= smb349_cradle_set_property;
	chip->cradle_psy.property_is_writeable =
					smb349_cradle_property_is_writeable;
	chip->cradle_psy.properties	= smb349_cradle_properties;
	chip->cradle_psy.num_properties	= ARRAY_SIZE(smb349_cradle_properties);

	chip->ext_psy = power_supply_get_by_name((char *)chip->ext_psy_name);
	if (!chip->ext_psy) {
		dev_err(chip->dev,
			"Waiting for '%s' psy to become available\n",
			(char *)chip->ext_psy_name);
		return -EPROBE_DEFER;
	}

	mutex_init(&chip->irq_complete);
	mutex_init(&chip->pm_lock);

	rc = power_supply_register(chip->dev, &chip->cradle_psy);
	if (rc < 0) {
		dev_err(&client->dev, "Couldn't register '%s' psy rc=%d\n",
				chip->cradle_psy.name, rc);
		return rc;
	}

	rc = gpio_request(chip->chg_stat_gpio, "smb349_chg_stat");
	if (rc) {
		dev_err(&client->dev,
			"gpio_request for %d failed rc=%d\n",
			chip->chg_stat_gpio, rc);
		goto fail_chg_stat_irq;
	}

	irq = gpio_to_irq(chip->chg_stat_gpio);
	if (irq < 0) {
		dev_err(&client->dev,
			"Invalid chg_stat irq = %d\n", irq);
		goto fail_chg_stat_irq;
	}
	rc = devm_request_threaded_irq(&client->dev, irq,
		NULL, smb349_chg_stat_handler,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
		"smb349_chg_stat_irq", chip);
	if (rc) {
		dev_err(&client->dev,
			"Failed request_irq irq=%d, gpio=%d rc=%d\n",
					irq, chip->chg_stat_gpio, rc);
		goto fail_chg_stat_irq;
	}

	determine_initial_state(chip);

	dev_info(chip->dev, "SMB349 successfully initialized in STAT driven mode. charger=%d\n",
			chip->chg_present);
	return 0;

fail_chg_stat_irq:
	if (gpio_is_valid(chip->chg_stat_gpio))
		gpio_free(chip->chg_stat_gpio);
	power_supply_unregister(&chip->cradle_psy);
	return rc;
}

static int smb349_dual_charger_remove(struct i2c_client *client)
{
	struct smb349_dual_charger *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->cradle_psy);
	if (gpio_is_valid(chip->chg_stat_gpio))
		gpio_free(chip->chg_stat_gpio);

	mutex_destroy(&chip->pm_lock);
	mutex_destroy(&chip->irq_complete);
	return 0;
}

static struct of_device_id smb349_match_table[] = {
	{ .compatible = "qcom,smb349-dual-charger",},
	{},
};

static const struct i2c_device_id smb349_dual_charger_id[] = {
	{"smb349-dual-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb349_dual_charger_id);

static struct i2c_driver smb349_dual_charger_driver = {
	.driver		= {
		.name		= "smb349-dual-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= smb349_match_table,
	},
	.probe		= smb349_dual_charger_probe,
	.remove		= smb349_dual_charger_remove,
	.id_table	= smb349_dual_charger_id,
};

module_i2c_driver(smb349_dual_charger_driver);

MODULE_DESCRIPTION("SMB349 Dual Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb349-dual-charger");
