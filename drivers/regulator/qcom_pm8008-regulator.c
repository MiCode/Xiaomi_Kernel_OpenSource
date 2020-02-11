// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt) "PM8008: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/string.h>

#define pm8008_err(reg, message, ...) \
	pr_err("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)
#define pm8008_debug(reg, message, ...) \
	pr_debug("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)

#define STARTUP_DELAY_USEC		20
#define VSET_STEP_SIZE_MV		1
#define VSET_STEP_MV			8
#define VSET_STEP_UV			(VSET_STEP_MV * 1000)

#define MISC_BASE			0x900

#define MISC_CHIP_ENABLE_REG		(MISC_BASE + 0x50)
#define CHIP_ENABLE_BIT			BIT(0)

#define MISC_SHUTDOWN_CTRL_REG		(MISC_BASE + 0x59)
#define IGNORE_LDO_OCP_SHUTDOWN		BIT(3)

#define LDO_ENABLE_REG(base)		(base + 0x46)
#define ENABLE_BIT			BIT(7)

#define LDO_STATUS1_REG(base)		(base + 0x08)
#define VREG_OCP_BIT			BIT(5)
#define VREG_READY_BIT			BIT(7)
#define MODE_STATE_MASK			GENMASK(1, 0)
#define MODE_STATE_NPM			3
#define MODE_STATE_LPM			2
#define MODE_STATE_BYPASS		0

#define LDO_VSET_LB_REG(base)		(base + 0x40)

#define LDO_VSET_VALID_LB_REG(base)	(base + 0x42)

#define LDO_MODE_CTL1_REG(base)		(base + 0x45)
#define MODE_PRIMARY_MASK		GENMASK(2, 0)
#define LDO_MODE_NPM			7
#define LDO_MODE_LPM			4
#define FORCED_BYPASS			2

#define LDO_OCP_CTL1_REG(base)		(base + 0x88)
#define VREG_OCP_STATUS_CLR		BIT(1)
#define LDO_OCP_BROADCAST_EN_BIT	BIT(2)

#define LDO_STEPPER_CTL_REG(base)	(base + 0x3b)
#define STEP_RATE_MASK			GENMASK(1, 0)

#define LDO_PD_CTL_REG(base)		(base + 0xA0)
#define STRONG_PD_EN_BIT		BIT(7)

#define MAX_REG_NAME			20
#define PM8008_MAX_LDO			7

struct pm8008_chip {
	struct device		*dev;
	struct regmap		*regmap;
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
	int			ocp_irq;
};

struct regulator_data {
	char		*name;
	char		*supply_name;
	int		hpm_min_load_ua;
	int		min_dropout_uv;
};

struct pm8008_regulator {
	struct device		*dev;
	struct regmap		*regmap;
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	struct regulator	*parent_supply;
	struct regulator	*en_supply;
	struct device_node	*of_node;
	struct notifier_block	nb;
	u16			base;
	int			hpm_min_load_ua;
	int			min_dropout_uv;
	int			step_rate;
	bool			enable_ocp_broadcast;
};

static struct regulator_data reg_data[] = {
			/* name,        parent,  min load, headroom */
			{"pm8008_l1", "vdd_l1_l2", 10000, 225000},
			{"pm8008_l2", "vdd_l1_l2", 10000, 225000},
			{"pm8008_l3", "vdd_l3_l4", 10000, 200000},
			{"pm8008_l4", "vdd_l3_l4", 10000, 200000},
			{"pm8008_l5", "vdd_l5", 10000, 300000},
			{"pm8008_l6", "vdd_l6", 10000, 300000},
			{"pm8008_l7", "vdd_l7", 10000, 300000},
};

/* common functions */
static int pm8008_read(struct regmap *regmap,  u16 reg, u8 *val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		pr_err("failed to read 0x%04x\n", reg);

	return rc;
}

static int pm8008_write(struct regmap *regmap, u16 reg, u8 *val, int count)
{
	int rc;

	pr_debug("Writing 0x%02x to 0x%04x\n", val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		pr_err("failed to write 0x%04x\n", reg);

	return rc;
}

static int pm8008_masked_write(struct regmap *regmap, u16 reg, u8 mask,
				u8 val)
{
	int rc;

	pr_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		pr_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n",
				val, reg, mask);

	return rc;
}

/* PM8008 LDO Regulator callbacks */
static int pm8008_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);
	u8 vset_raw[2];
	int rc;

	rc = pm8008_read(pm8008_reg->regmap,
			LDO_VSET_VALID_LB_REG(pm8008_reg->base),
			vset_raw, 2);
	if (rc < 0) {
		pm8008_err(pm8008_reg,
			"failed to read regulator voltage rc=%d\n", rc);
		return rc;
	}

	pm8008_debug(pm8008_reg, "VSET read [%x][%x]\n",
			vset_raw[1], vset_raw[0]);
	return (vset_raw[1] << 8 | vset_raw[0]) * 1000;
}

static int pm8008_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = pm8008_read(pm8008_reg->regmap,
			LDO_ENABLE_REG(pm8008_reg->base), &reg, 1);
	if (rc < 0) {
		pm8008_err(pm8008_reg, "failed to read enable reg rc=%d\n", rc);
		return rc;
	}

	return !!(reg & ENABLE_BIT);
}

static int pm8008_regulator_enable(struct regulator_dev *rdev)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);
	int rc, rc2, current_uv, delay_us, delay_ms, retry_count = 10;
	u8 reg;

	current_uv = pm8008_regulator_get_voltage(rdev);
	if (current_uv < 0) {
		pm8008_err(pm8008_reg, "failed to get current voltage rc=%d\n",
			current_uv);
		return current_uv;
	}

	rc = regulator_enable(pm8008_reg->en_supply);
	if (rc < 0) {
		pm8008_err(pm8008_reg,
			"failed to enable en_supply rc=%d\n", rc);
		return rc;
	}

	if (pm8008_reg->parent_supply) {
		rc = regulator_set_voltage(pm8008_reg->parent_supply,
					current_uv + pm8008_reg->min_dropout_uv,
					INT_MAX);
		if (rc < 0) {
			pm8008_err(pm8008_reg, "failed to request parent supply voltage rc=%d\n",
				rc);
			goto remove_en;
		}

		rc = regulator_enable(pm8008_reg->parent_supply);
		if (rc < 0) {
			pm8008_err(pm8008_reg,
				"failed to enable parent rc=%d\n", rc);
			regulator_set_voltage(pm8008_reg->parent_supply, 0,
						INT_MAX);
			goto remove_en;
		}
	}

	rc = pm8008_masked_write(pm8008_reg->regmap,
				LDO_ENABLE_REG(pm8008_reg->base),
				ENABLE_BIT, ENABLE_BIT);
	if (rc < 0) {
		pm8008_err(pm8008_reg,
			"failed to enable regulator rc=%d\n", rc);
		goto remove_vote;
	}

	/*
	 * Wait for the VREG_READY status bit to be set using a timeout delay
	 * calculated from the current commanded voltage.
	 */
	delay_us = STARTUP_DELAY_USEC
			+ DIV_ROUND_UP(current_uv, pm8008_reg->step_rate);
	delay_ms = DIV_ROUND_UP(delay_us, 1000);

	/* Retry 10 times for VREG_READY before bailing out */
	while (retry_count--) {
		if (delay_ms > 20)
			msleep(delay_ms);
		else
			usleep_range(delay_us, delay_us + 100);

		rc = pm8008_read(pm8008_reg->regmap,
				LDO_STATUS1_REG(pm8008_reg->base), &reg, 1);
		if (rc < 0) {
			pm8008_err(pm8008_reg,
				"failed to read regulator status rc=%d\n", rc);
			goto disable_ldo;
		}
		if (reg & VREG_READY_BIT) {
			pm8008_debug(pm8008_reg, "regulator enabled\n");
			return 0;
		}
	}

	pm8008_err(pm8008_reg, "failed to enable regulator, VREG_READY not set\n");
	rc = -ETIME;

disable_ldo:
	pm8008_masked_write(pm8008_reg->regmap,
			LDO_ENABLE_REG(pm8008_reg->base), ENABLE_BIT, 0);

remove_vote:
	if (pm8008_reg->parent_supply) {
		rc2 = regulator_disable(pm8008_reg->parent_supply);
		if (rc2 < 0)
			pm8008_err(pm8008_reg, "failed to disable parent supply rc=%d\n",
				rc2);
		rc2 = regulator_set_voltage(pm8008_reg->parent_supply, 0,
						INT_MAX);
		if (rc2 < 0)
			pm8008_err(pm8008_reg, "failed to remove voltage vote for parent supply rc=%d\n",
				rc2);
	}

remove_en:
	rc2 = regulator_disable(pm8008_reg->en_supply);
	if (rc2 < 0)
		pm8008_err(pm8008_reg, "failed to disable en_supply rc=%d\n",
			rc2);

	return rc;
}

static int pm8008_regulator_disable(struct regulator_dev *rdev)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8008_masked_write(pm8008_reg->regmap,
				LDO_ENABLE_REG(pm8008_reg->base),
				ENABLE_BIT, 0);
	if (rc < 0) {
		pm8008_err(pm8008_reg,
			"failed to disable regulator rc=%d\n", rc);
		return rc;
	}

	/* remove voltage vote from parent regulator */
	if (pm8008_reg->parent_supply) {
		rc = regulator_disable(pm8008_reg->parent_supply);
		if (rc < 0) {
			pm8008_err(pm8008_reg, "failed to disable parent rc=%d\n",
				rc);
			return rc;
		}
		rc = regulator_set_voltage(pm8008_reg->parent_supply,
					0, INT_MAX);
		if (rc < 0) {
			pm8008_err(pm8008_reg, "failed to remove parent voltage rc=%d\n",
				rc);
			return rc;
		}
	}

	/* remove vote from chip enable regulator */
	rc = regulator_disable(pm8008_reg->en_supply);
	if (rc < 0) {
		pm8008_err(pm8008_reg, "failed to disable en_supply rc=%d\n",
			rc);
		return rc;
	}

	pm8008_debug(pm8008_reg, "regulator disabled\n");
	return 0;
}

static int pm8008_write_voltage(struct pm8008_regulator *pm8008_reg, int min_uv,
				int max_uv)
{
	int rc = 0, mv;
	u8 vset_raw[2];

	mv = DIV_ROUND_UP(min_uv, 1000);
	if (mv * 1000 > max_uv) {
		pm8008_err(pm8008_reg,
			"requested voltage above maximum limit\n");
		return -EINVAL;
	}

	/*
	 * Each LSB of regulator is 1mV and the voltage setpoint
	 * should be multiple of 8mV(step).
	 */
	mv = DIV_ROUND_UP(DIV_ROUND_UP(mv, VSET_STEP_MV) * VSET_STEP_MV,
				VSET_STEP_SIZE_MV);

	vset_raw[0] = mv & 0xff;
	vset_raw[1] = (mv & 0xff00) >> 8;
	rc = pm8008_write(pm8008_reg->regmap, LDO_VSET_LB_REG(pm8008_reg->base),
			vset_raw, 2);
	if (rc < 0) {
		pm8008_err(pm8008_reg, "failed to write voltage rc=%d\n", rc);
		return rc;
	}

	pm8008_debug(pm8008_reg, "VSET=[%x][%x]\n", vset_raw[1], vset_raw[0]);
	return 0;
}

static int pm8008_regulator_set_voltage_time(struct regulator_dev *rdev,
				int old_uV, int new_uv)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);

	return DIV_ROUND_UP(abs(new_uv - old_uV), pm8008_reg->step_rate);
}

static int pm8008_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uv, int max_uv, unsigned int *selector)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);
	int rc = 0, current_uv = 0, rounded_uv = 0, enabled = 0;

	if (pm8008_reg->parent_supply) {
		enabled = pm8008_regulator_is_enabled(rdev);
		if (enabled < 0) {
			return enabled;
		} else if (enabled) {
			current_uv = pm8008_regulator_get_voltage(rdev);
			if (current_uv < 0)
				return current_uv;
			rounded_uv = roundup(min_uv, VSET_STEP_UV);
		}
	}

	/*
	 * Set the parent_supply voltage before changing the LDO voltage when
	 * the LDO voltage is being increased.
	 */
	if (pm8008_reg->parent_supply && enabled && rounded_uv >= current_uv) {
		/* Request parent voltage with headroom */
		rc = regulator_set_voltage(pm8008_reg->parent_supply,
					rounded_uv + pm8008_reg->min_dropout_uv,
					INT_MAX);
		if (rc < 0) {
			pm8008_err(pm8008_reg, "failed to request parent supply voltage rc=%d\n",
				rc);
			return rc;
		}
	}

	rc = pm8008_write_voltage(pm8008_reg, min_uv, max_uv);
	if (rc < 0)
		return rc;

	/*
	 * Set the parent_supply voltage after changing the LDO voltage when
	 * the LDO voltage is being reduced.
	 */
	if (pm8008_reg->parent_supply && enabled && rounded_uv < current_uv) {
		/*
		 * Ensure sufficient time for the LDO voltage to slew down
		 * before reducing the parent supply voltage.  The regulator
		 * framework will add the same delay after this function returns
		 * in all cases (i.e. enabled/disabled and increasing/decreasing
		 * voltage).
		 */
		udelay(pm8008_regulator_set_voltage_time(rdev, rounded_uv,
							current_uv));

		/* Request parent voltage with headroom */
		rc = regulator_set_voltage(pm8008_reg->parent_supply,
					rounded_uv + pm8008_reg->min_dropout_uv,
					INT_MAX);
		if (rc < 0) {
			pm8008_err(pm8008_reg, "failed to request parent supply voltage rc=%d\n",
				rc);
			return rc;
		}
	}

	pm8008_debug(pm8008_reg, "voltage set to %d\n", min_uv);
	return rc;
}

static int pm8008_regulator_set_mode(struct regulator_dev *rdev,
				unsigned int mode)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 val = LDO_MODE_LPM;

	if (mode == REGULATOR_MODE_NORMAL)
		val = LDO_MODE_NPM;
	else if (mode == REGULATOR_MODE_IDLE)
		val = LDO_MODE_LPM;

	rc = pm8008_masked_write(pm8008_reg->regmap,
				LDO_MODE_CTL1_REG(pm8008_reg->base),
				MODE_PRIMARY_MASK, val);
	if (!rc)
		pm8008_debug(pm8008_reg, "mode set to %d\n", val);

	return rc;
}

static unsigned int pm8008_regulator_get_mode(struct regulator_dev *rdev)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = pm8008_read(pm8008_reg->regmap,
			LDO_STATUS1_REG(pm8008_reg->base), &reg, 1);
	if (rc < 0) {
		pm8008_err(pm8008_reg, "failed to get mode rc=%d\n", rc);
		return rc;
	}

	return ((reg & MODE_STATE_MASK) == MODE_STATE_NPM)
			? REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE;
}

static int pm8008_regulator_set_load(struct regulator_dev *rdev, int load_uA)
{
	struct pm8008_regulator *pm8008_reg = rdev_get_drvdata(rdev);
	int mode;

	if (load_uA >= pm8008_reg->hpm_min_load_ua)
		mode = REGULATOR_MODE_NORMAL;
	else
		mode = REGULATOR_MODE_IDLE;

	return pm8008_regulator_set_mode(rdev, mode);
}

static struct regulator_ops pm8008_regulator_ops = {
	.enable			= pm8008_regulator_enable,
	.disable		= pm8008_regulator_disable,
	.is_enabled		= pm8008_regulator_is_enabled,
	.set_voltage		= pm8008_regulator_set_voltage,
	.get_voltage		= pm8008_regulator_get_voltage,
	.set_mode		= pm8008_regulator_set_mode,
	.get_mode		= pm8008_regulator_get_mode,
	.set_load		= pm8008_regulator_set_load,
	.set_voltage_time	= pm8008_regulator_set_voltage_time,
};

static int pm8008_ldo_cb(struct notifier_block *nb, ulong event, void *data)
{
	struct pm8008_regulator *pm8008_reg = container_of(nb,
						struct pm8008_regulator, nb);
	u8 val;
	int rc;

	if (event != REGULATOR_EVENT_OVER_CURRENT)
		return NOTIFY_OK;

	rc = pm8008_read(pm8008_reg->regmap,
			 LDO_STATUS1_REG(pm8008_reg->base), &val, 1);
	if (rc < 0) {
		pm8008_err(pm8008_reg,
			"failed to read regulator status rc=%d\n", rc);
		goto error;
	}

	if (!(val & VREG_OCP_BIT))
		return NOTIFY_OK;

	pr_err("OCP triggered on %s\n", pm8008_reg->rdesc.name);
	/*
	 * Toggle the OCP_STATUS_CLR bit to re-arm the OCP status for
	 * the next OCP event
	 */
	rc = pm8008_masked_write(pm8008_reg->regmap,
				 LDO_OCP_CTL1_REG(pm8008_reg->base),
				 VREG_OCP_STATUS_CLR, VREG_OCP_STATUS_CLR);
	if (rc < 0) {
		pm8008_err(pm8008_reg, "failed to write OCP_STATUS_CLR rc=%d\n",
			   rc);
		goto error;
	}

	rc = pm8008_masked_write(pm8008_reg->regmap,
				 LDO_OCP_CTL1_REG(pm8008_reg->base),
				 VREG_OCP_STATUS_CLR, 0);
	if (rc < 0) {
		pm8008_err(pm8008_reg, "failed to write OCP_STATUS_CLR rc=%d\n",
			   rc);
		goto error;
	}

	/* Notify the consumers about the OCP event */
	mutex_lock(&pm8008_reg->rdev->mutex);
	regulator_notifier_call_chain(pm8008_reg->rdev,
				REGULATOR_EVENT_OVER_CURRENT, NULL);
	mutex_unlock(&pm8008_reg->rdev->mutex);

error:
	return NOTIFY_OK;
}

static int pm8008_register_ldo(struct pm8008_regulator *pm8008_reg,
						const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;
	struct device *dev = pm8008_reg->dev;
	struct device_node *reg_node = pm8008_reg->of_node;
	char buff[MAX_REG_NAME];
	int rc, i, init_voltage;
	u32 base = 0;
	u8 reg;

	/* get regulator data */
	for (i = 0; i < PM8008_MAX_LDO; i++)
		if (!strcmp(reg_data[i].name, name))
			break;

	if (i == PM8008_MAX_LDO) {
		pr_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32(reg_node, "reg", &base);
	if (rc < 0) {
		pr_err("%s: failed to get regulator base rc=%d\n", name, rc);
		return rc;
	}
	pm8008_reg->base = base;

	pm8008_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "qcom,min-dropout-voltage",
						&pm8008_reg->min_dropout_uv);

	pm8008_reg->hpm_min_load_ua = reg_data[i].hpm_min_load_ua;
	of_property_read_u32(reg_node, "qcom,hpm-min-load",
						&pm8008_reg->hpm_min_load_ua);
	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "qcom,init-voltage", &init_voltage);

	if (of_property_read_bool(reg_node, "qcom,strong-pd")) {
		rc = pm8008_masked_write(pm8008_reg->regmap,
				LDO_PD_CTL_REG(pm8008_reg->base),
				STRONG_PD_EN_BIT, STRONG_PD_EN_BIT);
		if (rc < 0) {
			pr_err("%s: failed to configure pull down rc=%d\n",
				name, rc);
			return rc;
		}
	}

	if (pm8008_reg->enable_ocp_broadcast) {
		rc = pm8008_masked_write(pm8008_reg->regmap,
				LDO_OCP_CTL1_REG(pm8008_reg->base),
				LDO_OCP_BROADCAST_EN_BIT,
				LDO_OCP_BROADCAST_EN_BIT);
		if (rc < 0) {
			pr_err("%s: failed to configure ocp broadcast rc=%d\n",
				name, rc);
			return rc;
		}
	}

	/* get slew rate */
	rc = pm8008_read(pm8008_reg->regmap,
			LDO_STEPPER_CTL_REG(pm8008_reg->base), &reg, 1);
	if (rc < 0) {
		pr_err("%s: failed to read step rate configuration rc=%d\n",
				name, rc);
		return rc;
	}
	pm8008_reg->step_rate = 38400 >> (reg & STEP_RATE_MASK);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		pm8008_reg->parent_supply = devm_regulator_get(dev,
						reg_data[i].supply_name);
		if (IS_ERR(pm8008_reg->parent_supply)) {
			rc = PTR_ERR(pm8008_reg->parent_supply);
			if (rc != -EPROBE_DEFER)
				pr_err("%s: failed to get parent regulator rc=%d\n",
					name, rc);
			return rc;
		}
	}

	/* pm8008_en should be present otherwise fail the regulator probe */
	pm8008_reg->en_supply = devm_regulator_get(dev, "pm8008_en");
	if (IS_ERR(pm8008_reg->en_supply)) {
		rc = PTR_ERR(pm8008_reg->en_supply);
		pr_err("%s: failed to get chip_en supply\n", name);
		return rc;
	}

	init_data = of_get_regulator_init_data(dev, reg_node,
						&pm8008_reg->rdesc);
	if (init_data == NULL) {
		pr_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}
	if (!init_data->constraints.name) {
		pr_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = pm8008_write_voltage(pm8008_reg, init_voltage,
					init_data->constraints.max_uV);
		if (rc < 0)
			pr_err("%s: failed to set initial voltage rc=%d\n",
					name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
						| REGULATOR_CHANGE_VOLTAGE
						| REGULATOR_CHANGE_MODE
						| REGULATOR_CHANGE_DRMS;
	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = pm8008_reg;
	reg_config.of_node = reg_node;

	pm8008_reg->rdesc.owner = THIS_MODULE;
	pm8008_reg->rdesc.type = REGULATOR_VOLTAGE;
	pm8008_reg->rdesc.ops = &pm8008_regulator_ops;
	pm8008_reg->rdesc.name = init_data->constraints.name;
	pm8008_reg->rdesc.n_voltages = 1;

	pm8008_reg->rdev = devm_regulator_register(dev, &pm8008_reg->rdesc,
						&reg_config);
	if (IS_ERR(pm8008_reg->rdev)) {
		rc = PTR_ERR(pm8008_reg->rdev);
		pr_err("%s: failed to register regulator rc=%d\n",
				pm8008_reg->rdesc.name, rc);
		return rc;
	}

	if (pm8008_reg->enable_ocp_broadcast) {
		pm8008_reg->nb.notifier_call = pm8008_ldo_cb;
		rc = devm_regulator_register_notifier(pm8008_reg->en_supply,
						 &pm8008_reg->nb);
		if (rc < 0) {
			pr_err("Failed to register a regulator notifier rc=%d\n",
				rc);
			return rc;
		}
	}

	pr_debug("%s regulator registered\n", name);

	return 0;
}

/* PMIC probe and helper function */
static int pm8008_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc = 0;
	const char *name;
	struct device_node *child;
	struct pm8008_regulator *pm8008_reg;
	bool ocp;

	ocp = of_property_read_bool(dev->of_node, "qcom,enable-ocp-broadcast");

	/* parse each subnode and register regulator for regulator child */
	for_each_available_child_of_node(dev->of_node, child) {
		pm8008_reg = devm_kzalloc(dev, sizeof(*pm8008_reg), GFP_KERNEL);
		if (!pm8008_reg)
			return -ENOMEM;

		pm8008_reg->regmap = regmap;
		pm8008_reg->of_node = child;
		pm8008_reg->dev = dev;
		pm8008_reg->enable_ocp_broadcast = ocp;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = pm8008_register_ldo(pm8008_reg, name);
		if (rc < 0) {
			pr_err("failed to register regulator %s rc=%d\n",
					name, rc);
			return rc;
		}
	}

	return 0;
}

static int pm8008_regulator_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct regmap *regmap;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}

	rc = pm8008_parse_regulator(regmap, &pdev->dev);
	if (rc < 0) {
		pr_err("failed to parse device tree rc=%d\n", rc);
		return rc;
	}

	return 0;
}

/* PM8008 chip enable regulator callbacks */
static int pm8008_enable_regulator_enable(struct regulator_dev *rdev)
{
	struct pm8008_chip *chip = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8008_masked_write(chip->regmap, MISC_CHIP_ENABLE_REG,
				CHIP_ENABLE_BIT, CHIP_ENABLE_BIT);
	if (rc  < 0) {
		pm8008_err(chip, "failed to enable chip rc=%d\n", rc);
		return rc;
	}

	pm8008_debug(chip, "regulator enabled\n");
	return 0;
}

static int pm8008_enable_regulator_disable(struct regulator_dev *rdev)
{
	struct pm8008_chip *chip = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8008_masked_write(chip->regmap, MISC_CHIP_ENABLE_REG,
				CHIP_ENABLE_BIT, 0);
	if (rc  < 0) {
		pm8008_err(chip, "failed to disable chip rc=%d\n", rc);
		return rc;
	}

	pm8008_debug(chip, "regulator disabled\n");
	return 0;
}

static int pm8008_enable_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct pm8008_chip *chip = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = pm8008_read(chip->regmap, MISC_CHIP_ENABLE_REG, &reg, 1);
	if (rc  < 0) {
		pm8008_err(chip, "failed to get chip state rc=%d\n", rc);
		return rc;
	}

	return !!(reg & CHIP_ENABLE_BIT);
}

static struct regulator_ops pm8008_enable_reg_ops = {
	.enable = pm8008_enable_regulator_enable,
	.disable = pm8008_enable_regulator_disable,
	.is_enabled = pm8008_enable_regulator_is_enabled,
};

static int pm8008_init_enable_regulator(struct pm8008_chip *chip)
{
	struct regulator_config cfg = {};
	int rc = 0;

	cfg.dev = chip->dev;
	cfg.driver_data = chip;

	chip->rdesc.owner = THIS_MODULE;
	chip->rdesc.type = REGULATOR_VOLTAGE;
	chip->rdesc.ops = &pm8008_enable_reg_ops;
	chip->rdesc.of_match = "qcom,pm8008-chip-en";
	chip->rdesc.name = "qcom,pm8008-chip-en";

	chip->rdev = devm_regulator_register(chip->dev, &chip->rdesc, &cfg);
	if (IS_ERR(chip->rdev)) {
		rc = PTR_ERR(chip->rdev);
		chip->rdev = NULL;
		return rc;
	}

	return 0;
}

static irqreturn_t pm8008_ocp_irq(int irq, void *_chip)
{
	struct pm8008_chip *chip = _chip;

	mutex_lock(&chip->rdev->mutex);
	regulator_notifier_call_chain(chip->rdev, REGULATOR_EVENT_OVER_CURRENT,
				      NULL);
	mutex_unlock(&chip->rdev->mutex);

	return IRQ_HANDLED;
}

static int pm8008_chip_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct pm8008_chip *chip;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}
	chip->dev = &pdev->dev;

	/* Register chip enable regulator */
	rc = pm8008_init_enable_regulator(chip);
	if (rc < 0) {
		pr_err("Failed to register chip enable regulator rc=%d\n", rc);
		return rc;
	}

	chip->ocp_irq = of_irq_get_byname(chip->dev->of_node, "ocp");
	if (chip->ocp_irq < 0) {
		pr_debug("Failed to get pm8008-ocp-irq\n");
	} else {
		rc = devm_request_threaded_irq(chip->dev, chip->ocp_irq, NULL,
				pm8008_ocp_irq, IRQF_ONESHOT,
				"ocp", chip);
		if (rc < 0) {
			pr_err("Failed to request 'pm8008-ocp-irq' rc=%d\n",
				rc);
			return rc;
		}

		/* Ignore PMIC shutdown for LDO OCP event */
		rc = pm8008_masked_write(chip->regmap, MISC_SHUTDOWN_CTRL_REG,
			IGNORE_LDO_OCP_SHUTDOWN, IGNORE_LDO_OCP_SHUTDOWN);
		if (rc < 0) {
			pr_err("Failed to write MISC_SHUTDOWN register rc=%d\n",
				rc);
			return rc;
		}
	}

	pr_debug("PM8008 chip registered\n");
	return 0;
}

static int pm8008_chip_remove(struct platform_device *pdev)
{
	struct pm8008_chip *chip = platform_get_drvdata(pdev);
	int rc;

	rc = pm8008_masked_write(chip->regmap, MISC_CHIP_ENABLE_REG,
				CHIP_ENABLE_BIT, 0);
	if (rc  < 0)
		pr_err("failed to disable chip rc=%d\n", rc);

	return 0;
}

static const struct of_device_id pm8008_regulator_match_table[] = {
	{
		.compatible	= "qcom,pm8008-regulator",
	},
	{ },
};

static struct platform_driver pm8008_regulator_driver = {
	.driver	= {
		.name		= "qcom,pm8008-regulator",
		.owner		= THIS_MODULE,
		.of_match_table	= pm8008_regulator_match_table,
	},
	.probe		= pm8008_regulator_probe,
};
module_platform_driver(pm8008_regulator_driver);

static const struct of_device_id pm8008_chip_match_table[] = {
	{
		.compatible	= "qcom,pm8008-chip",
	},
	{ },
};

static struct platform_driver pm8008_chip_driver = {
	.driver	= {
		.name		= "qcom,pm8008-chip",
		.owner		= THIS_MODULE,
		.of_match_table	= pm8008_chip_match_table,
	},
	.probe		= pm8008_chip_probe,
	.remove		= pm8008_chip_remove,
};
module_platform_driver(pm8008_chip_driver);

MODULE_DESCRIPTION("QPNP PM8008 PMIC Regulator Driver");
MODULE_LICENSE("GPL v2");
