// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"HL6111R: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/debugfs.h>
#include "hl6111r.h"

static const struct regmap_config chip_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

struct hl6111r {
	struct device		*dev;
	struct regmap		*regmap;
	struct power_supply	*psy;
	struct dentry		*dfs_root;
};

struct vout_range {
	int min_mv;
	int step_mv;
};

struct cc_tuple {
	int raw;
	int val_ma;
};

struct cc_range {
	struct cc_tuple min;
	struct cc_tuple max;
	int step_ma;
};

/* Utility functions */

static int hl6111r_read(struct hl6111r *chip, u8 addr, u8 *val)
{
	int rc = 0;
	unsigned int value = 0;

	rc = regmap_read(chip->regmap, addr, &value);
	if (rc < 0)
		return rc;

	*val = (u8)value;

	pr_debug("read 0x%02x: 0x%02x\n", addr, *val);
	return rc;
}

static int hl6111r_write(struct hl6111r *chip, u8 addr, u8 val)
{
	int rc;

	rc = regmap_write(chip->regmap, addr, val);
	if (rc < 0)
		return rc;

	pr_debug("write 0x%02x: 0x%02x\n", addr, val);
	return rc;
}

static int hl6111r_masked_write(struct hl6111r *chip, u8 addr, u8 mask, u8 val)
{
	pr_debug("mask %02x write 0x%02x: 0x%02x\n", mask, addr, (val & mask));
	return regmap_update_bits(chip->regmap, addr, mask, val);
}

static int is_dc_online(bool *online)
{
	int rc;
	struct power_supply *dc_psy;
	union power_supply_propval pval;

	dc_psy = power_supply_get_by_name("dc");
	if (!dc_psy) {
		pr_err_ratelimited("DC psy unavailable\n");
		return -ENODEV;
	}

	rc = power_supply_get_property(dc_psy, POWER_SUPPLY_PROP_ONLINE,
			&pval);
	pr_debug("%s\n", (pval.intval ? "yes" : "no"));
	if (rc < 0)
		return rc;

	*online = pval.intval;

	return 0;
}

/* Callbacks for gettable properties */

static int hl6111r_get_online(struct hl6111r *chip, int *val)
{
	int rc;
	u8 stat;

	rc = hl6111r_read(chip, LATCHED_STATUS_REG, &stat);
	if (rc < 0)
		return rc;

	*val = stat & OUT_EN_L_BIT;

	return rc;
}

static int hl6111r_get_voltage_now(struct hl6111r *chip, int *val)
{
	int rc;
	u8 raw = 0;

	rc = hl6111r_read(chip, VOUT_NOW_REG, &raw);
	if (rc < 0)
		return rc;

	*val = raw * VOUT_STEP_UV;

	pr_debug("raw = 0x%02x, scaled = %d mV\n", raw, (*val / 1000));

	return rc;
}

static int hl6111r_get_current_now(struct hl6111r *chip, int *val)
{
	int rc;
	u8 raw = 0;

	rc = hl6111r_read(chip, IOUT_NOW_REG, &raw);
	if (rc < 0)
		return rc;

	*val = raw * IOUT_NOW_STEP_UA;

	pr_debug("raw = 0x%02x, scaled = %d mA\n", raw, (*val / 1000));
	return rc;
}

static int hl6111r_get_voltage_avg(struct hl6111r *chip, int *val)
{
	int rc;
	u8 raw = 0;

	rc = hl6111r_read(chip, VOUT_AVG_REG, &raw);
	if (rc < 0)
		return rc;

	*val = raw * VOUT_STEP_UV;

	pr_debug("raw = 0x%02x, scaled = %d mV\n", raw, (*val / 1000));
	return rc;
}

static int hl6111r_get_current_avg(struct hl6111r *chip, int *val)
{
	int rc;
	u8 raw = 0;

	rc = hl6111r_read(chip, IOUT_AVG_REG, &raw);
	if (rc < 0)
		return rc;

	*val = raw * IOUT_AVG_STEP_UA;

	pr_debug("raw = 0x%02x, scaled = %d mA\n", raw, (*val / 1000));
	return rc;
}

#define IOUT_MIN_100_MA		100
#define IOUT_MAX_2200_MA	2200
#define IOUT_NO_LIMIT_RAW	0x1F
#define IOUT_NO_LIMIT_VAL	0
static const struct cc_range hl6111r_cc_range[] = {
	{
		.min = {0, IOUT_MIN_100_MA},
		.max = {0x12, 1000},
		.step_ma = 50,
	},
	{
		.min = {0x13, 1100},
		.max = {0x1E, IOUT_MAX_2200_MA},
		.step_ma = 100,
	},
	{
		/* IOUT_NO_LIMIT */
		.min = {IOUT_NO_LIMIT_RAW, IOUT_NO_LIMIT_VAL},
		.max = {IOUT_NO_LIMIT_RAW, IOUT_NO_LIMIT_VAL},
		.step_ma = INT_MAX,
	}
};

static int hl6111r_get_cc_current(struct hl6111r *chip, int *val)
{
	int rc, scaled_ma = 0, range = 0, step = 0;
	u8 raw = 0;
	const struct cc_range *r;

	rc = hl6111r_read(chip, IOUT_LIM_SEL_REG, &raw);
	if (rc < 0)
		return rc;

	raw >>= IOUT_LIM_SHIFT;

	if (raw == IOUT_NO_LIMIT_RAW) {
		/* IOUT_NO_LIMIT */
		*val = IOUT_NO_LIMIT_VAL;
		return 0;
	}

	range = raw / hl6111r_cc_range[1].min.raw;
	step = raw % hl6111r_cc_range[1].min.raw;

	if (range >= ARRAY_SIZE(hl6111r_cc_range))
		range = ARRAY_SIZE(hl6111r_cc_range) - 1;

	r = &hl6111r_cc_range[range];

	/* Determine constant current output from range */
	scaled_ma = r->min.val_ma + (step * r->step_ma);

	/* Return value in uA */
	*val = scaled_ma * 1000;

	pr_debug("raw = 0x%02x, scaled = %d mA\n", raw, scaled_ma);
	return rc;
}

static int hl6111r_get_temp(struct hl6111r *chip, int *val)
{
	int rc;
	u8 raw = 0;

	rc = hl6111r_read(chip, DIE_TEMP_REG, &raw);
	if (rc < 0)
		return rc;

	*val = 10 * DIE_TEMP_SCALED_DEG_C(raw);

	pr_debug("raw = 0x%02x, scaled = %d deg C\n", raw, (*val / 10));
	return rc;
}

static const struct vout_range hl6111r_vout_range[] = {
	/* {Range's min value (mV), Range's step size (mV) */
	{4940, 20},
	{7410, 30},
	{9880, 40},
	{3952, 16}
};

static int hl6111r_get_vout_target(struct hl6111r *chip, int *val)
{
	int rc, vout_target_mv = 0;
	u8 raw = 0, range, vout_target_raw;
	bool dc_online = false;
	const struct vout_range *r;

	*val = 0;

	rc = is_dc_online(&dc_online);
	if (rc < 0)
		return rc;
	if (!dc_online)
		return 0;

	/* Read range selector register to determine range */
	rc = hl6111r_read(chip, VOUT_RANGE_SEL_REG, &raw);
	if (rc < 0)
		return rc;

	range = (raw & VOUT_RANGE_SEL_MASK) >> VOUT_RANGE_SEL_SHIFT;
	r = &hl6111r_vout_range[range];

	/* Use range information to calculate voltage */
	rc = hl6111r_read(chip, VOUT_TARGET_REG, &vout_target_raw);
	if (rc < 0)
		return rc;

	vout_target_mv = r->min_mv + (r->step_mv * vout_target_raw);

	*val = (vout_target_mv * 1000);

	return rc;
}

/* Callbacks for settable properties */

#define HL6111R_MIN_VOLTAGE_UV	4940000
#define HL6111R_MAX_VOLTAGE_UV	20080000
static int hl6111r_set_vout_target(struct hl6111r *chip, const int val)
{
	int rc, vout_target_uv;
	u8 vout_target_raw;
	const struct vout_range *r;

	vout_target_uv = val;

	if (val < HL6111R_MIN_VOLTAGE_UV || val > HL6111R_MAX_VOLTAGE_UV)
		return -EINVAL;

	/*
	 * Next, write to range selector register to set the range.
	 * Select only range 0 for now.
	 *	Range 0: V_out = [4.94 V, 10.04V] in steps of 20mV
	 */
	rc = hl6111r_write(chip, VOUT_RANGE_SEL_REG, 0);
	if (rc < 0)
		return rc;

	r = &hl6111r_vout_range[0];

	vout_target_raw = ((vout_target_uv / 1000) - r->min_mv) / r->step_mv;

	pr_debug("set = %d, raw = 0x%02x\n", vout_target_uv, vout_target_raw);
	rc = hl6111r_write(chip, VOUT_TARGET_REG, vout_target_raw);

	return rc;
}

static int hl6111r_set_cc_current(struct hl6111r *chip, const int val)
{
	u8 raw;
	int rc, tmp_ma, range = 0;
	const struct cc_range *r;
	const int max_cc_ranges = ARRAY_SIZE(hl6111r_cc_range) - 1;

	/* Minimum settable cc current = 100 mA */
	tmp_ma = max(IOUT_MIN_100_MA, (val / 1000));

	/*
	 * Special case:
	 *	if tmp_ma is 2200, range will be incorrectly set to 2 according
	 *	to the range calculation. Correct range to 1 in this case.
	 */
	if (tmp_ma == IOUT_MAX_2200_MA)
		range = 1;
	else
		/* Limit max range to 2 */
		range = min(max_cc_ranges,
				(tmp_ma / hl6111r_cc_range[1].min.val_ma));

	r = &hl6111r_cc_range[range];

	if (range == (ARRAY_SIZE(hl6111r_cc_range) - 1)) {
		/* IOUT_NO_LIMIT */
		raw = IOUT_NO_LIMIT_RAW;
	} else {
		raw = r->min.raw +
			(((tmp_ma - r->min.val_ma) * (r->max.raw - r->min.raw))
				/ (r->max.val_ma - r->min.val_ma));
	}

	pr_debug("cc_current = %d mA, unmasked raw = 0x%02x\n", tmp_ma, raw);

	rc = hl6111r_masked_write(chip, IOUT_LIM_SEL_REG, IOUT_LIM_SEL_MASK,
			(raw << IOUT_LIM_SHIFT));

	return rc;
}

static enum power_supply_property hl6111r_psy_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
};

static int hl6111r_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	int rc, *val = &pval->intval;
	struct hl6111r *chip = power_supply_get_drvdata(psy);
	bool dc_online = false;

	/* Check if DC PSY is online first */
	rc = is_dc_online(&dc_online);
	if (!dc_online || rc < 0) {
		*val = 0;
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		rc = hl6111r_get_online(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = hl6111r_get_voltage_now(chip, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = hl6111r_get_current_now(chip, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = hl6111r_get_temp(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		rc = hl6111r_get_voltage_avg(chip, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		rc = hl6111r_get_current_avg(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		*val = HL6111R_MAX_VOLTAGE_UV;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		rc = hl6111r_get_cc_current(chip, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		rc = hl6111r_get_vout_target(chip, val);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("property %d unavailable: %d\n", psp, rc);
		return -ENODATA;
	}

	return rc;
}

static int hl6111r_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return 1;
	default:
		break;
	}

	return 0;
}

static int hl6111r_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	int rc;
	const int *val = &pval->intval;
	struct hl6111r *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		rc = hl6111r_set_vout_target(chip, *val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		rc = hl6111r_set_cc_current(chip, *val);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static const struct power_supply_desc hl6111r_psy_desc = {
	.name = "wireless",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = hl6111r_psy_props,
	.num_properties = ARRAY_SIZE(hl6111r_psy_props),
	.get_property = hl6111r_get_prop,
	.set_property = hl6111r_set_prop,
	.property_is_writeable = hl6111r_prop_is_writeable,
};

static ssize_t irect_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int rc, irect_ua = 0;
	u8 raw = 0;
	bool dc_online = false;
	struct hl6111r *chip = dev_get_drvdata(dev);

	rc = is_dc_online(&dc_online);
	if (rc < 0 || !dc_online)
		goto exit;

	rc = hl6111r_read(chip, IRECT_REG, &raw);
	if (rc < 0)
		goto exit;

	irect_ua = IRECT_SCALED_UA(raw);

	pr_debug("raw = 0x%02x, scaled = %d mA\n", raw, (irect_ua / 1000));
exit:
	return scnprintf(buf, PAGE_SIZE, "%d\n", irect_ua);
}
static DEVICE_ATTR_RO(irect);

static ssize_t vrect_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int rc, vrect_uv = 0;
	u8 raw = 0;
	bool dc_online = false;
	struct hl6111r *chip = dev_get_drvdata(dev);

	rc = is_dc_online(&dc_online);
	if (rc < 0 || !dc_online)
		goto exit;

	rc = hl6111r_read(chip, VRECT_REG, &raw);
	if (rc < 0)
		goto exit;

	vrect_uv = VRECT_SCALED_UV(raw);

	pr_debug("raw = 0x%02x, scaled = %d mV\n",
			raw, (vrect_uv / 1000));
exit:
	return scnprintf(buf, PAGE_SIZE, "%d\n", vrect_uv);
}
static DEVICE_ATTR_RO(vrect);

static struct attribute *hl6111r_attrs[] = {
	&dev_attr_vrect.attr,
	&dev_attr_irect.attr,
	NULL,
};
ATTRIBUTE_GROUPS(hl6111r);

static int hl6111r_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int rc;
	struct hl6111r *chip;
	struct power_supply_config cfg = {0};

	chip = devm_kzalloc(&i2c->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;
	chip->dev = &i2c->dev;

	i2c_set_clientdata(i2c, chip);

	chip->regmap = devm_regmap_init_i2c(i2c, &chip_regmap);
	if (IS_ERR(chip->regmap)) {
		rc = PTR_ERR(chip->regmap);
		dev_err(&i2c->dev, "regmap init failed: %d\n", rc);
		goto cleanup;
	}

	/* Create PSY */
	cfg.drv_data = chip;
	cfg.of_node = chip->dev->of_node;

	chip->psy = devm_power_supply_register(chip->dev, &hl6111r_psy_desc,
			&cfg);

	if (IS_ERR(chip->psy)) {
		dev_err(&i2c->dev, "psy registration failed: %d\n",
				PTR_ERR(chip->psy));
		rc = PTR_ERR(chip->psy);
		goto cleanup;
	}

	/* Create device attributes */
	rc = sysfs_create_groups(&chip->dev->kobj, hl6111r_groups);
	if (rc < 0)
		goto cleanup;

	pr_info("probe successful\n");

	return 0;

cleanup:
	i2c_set_clientdata(i2c, NULL);
	return rc;
}

static int hl6111r_remove(struct i2c_client *i2c)
{
	i2c_set_clientdata(i2c, NULL);
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "halo,hl6111r", },
	{ }
};

static struct i2c_driver hl6111r_driver = {
	.driver = {
		.name = "hl6111r-driver",
		.of_match_table = match_table,
	},
	.probe =    hl6111r_probe,
	.remove =   hl6111r_remove,
};

module_i2c_driver(hl6111r_driver);

MODULE_DESCRIPTION("HL6111R driver");
MODULE_LICENSE("GPL v2");
