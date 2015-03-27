/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/string.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/spm-regulator.h>
#include <soc/qcom/spm.h>

#define SPM_REGULATOR_DRIVER_NAME "qcom,spm-regulator"

struct voltage_range {
	int min_uV;
	int set_point_min_uV;
	int max_uV;
	int step_uV;
};

enum qpnp_regulator_uniq_type {
	QPNP_TYPE_FTS2,
	QPNP_TYPE_FTS2p5,
	QPNP_TYPE_ULT_HF,
};

enum qpnp_regulator_type {
	QPNP_FTS2_TYPE		= 0x1C,
	QPNP_FTS2p5_TYPE	= 0x1C,
	QPNP_ULT_HF_TYPE	= 0x22,
};

enum qpnp_regulator_subtype {
	QPNP_FTS2_SUBTYPE	= 0x08,
	QPNP_FTS2p5_SUBTYPE	= 0x09,
	QPNP_ULT_HF_SUBTYPE	= 0x0D,
};

static const struct voltage_range fts2_range0 = {0, 350000, 1275000,  5000};
static const struct voltage_range fts2_range1 = {0, 700000, 2040000, 10000};
static const struct voltage_range fts2p5_range0
					 = { 80000, 350000, 1355000,  5000};
static const struct voltage_range fts2p5_range1
					 = {160000, 700000, 2200000, 10000};
static const struct voltage_range ult_hf_range0 = {375000, 375000, 1562500,
								12500};
static const struct voltage_range ult_hf_range1 = {750000, 750000, 1525000,
								25000};

#define QPNP_SMPS_REG_TYPE		0x04
#define QPNP_SMPS_REG_SUBTYPE		0x05
#define QPNP_FTS2_REG_VOLTAGE_RANGE	0x40
#define QPNP_SMPS_REG_VOLTAGE_SETPOINT	0x41
#define QPNP_SMPS_REG_MODE		0x45
#define QPNP_SMPS_REG_STEP_CTRL		0x61

#define QPNP_SMPS_MODE_PWM		0x80
#define QPNP_FTS2_MODE_AUTO		0x40

#define QPNP_FTS2_STEP_CTRL_STEP_MASK	0x18
#define QPNP_FTS2_STEP_CTRL_STEP_SHIFT	3
#define QPNP_SMPS_STEP_CTRL_DELAY_MASK	0x07
#define QPNP_SMPS_STEP_CTRL_DELAY_SHIFT	0

/* Clock rate in kHz of the FTS2 regulator reference clock. */
#define QPNP_FTS2_CLOCK_RATE		19200

/* Time to delay in us to ensure that a mode change has completed. */
#define QPNP_FTS2_MODE_CHANGE_DELAY	50

/* Minimum time in us that it takes to complete a single SPMI write. */
#define QPNP_SPMI_WRITE_MIN_DELAY	8

/* Minimum voltage stepper delay for each step. */
#define QPNP_FTS2_STEP_DELAY		8
#define QPNP_ULT_HF_STEP_DELAY		20

/*
 * The ratio QPNP_FTS2_STEP_MARGIN_NUM/QPNP_FTS2_STEP_MARGIN_DEN is use to
 * adjust the step rate in order to account for oscillator variance.
 */
#define QPNP_FTS2_STEP_MARGIN_NUM	4
#define QPNP_FTS2_STEP_MARGIN_DEN	5

/* VSET value to decide the range of ULT SMPS */
#define ULT_SMPS_RANGE_SPLIT 0x60

struct spm_vreg {
	struct regulator_desc		rdesc;
	struct regulator_dev		*rdev;
	struct spmi_device		*spmi_dev;
	const struct voltage_range	*range;
	int				uV;
	int				last_set_uV;
	unsigned			vlevel;
	unsigned			last_set_vlevel;
	bool				online;
	u16				spmi_base_addr;
	u8				init_mode;
	int				step_rate;
	enum qpnp_regulator_uniq_type	regulator_type;
	u32				cpu_num;
	bool				bypass_spm;
};

static int qpnp_fts2_set_mode(struct spm_vreg *vreg, u8 mode)
{
	int rc;

	rc = spmi_ext_register_writel(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_SMPS_REG_MODE, &mode, 1);
	if (rc)
		dev_err(&vreg->spmi_dev->dev, "%s: could not write to mode register, rc=%d\n",
			__func__, rc);

	return rc;
}

static int _spm_regulator_set_voltage(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);
	bool spm_failed = false;
	int rc = 0;
	u8 reg;

	if (vreg->vlevel == vreg->last_set_vlevel)
		return 0;

	if ((vreg->regulator_type == QPNP_TYPE_FTS2)
	    && !(vreg->init_mode & QPNP_SMPS_MODE_PWM)
	    && vreg->uV > vreg->last_set_uV) {
		/* Switch to PWM mode so that voltage ramping is fast. */
		rc = qpnp_fts2_set_mode(vreg, QPNP_SMPS_MODE_PWM);
		if (rc)
			return rc;
	}

	if (likely(!vreg->bypass_spm)) {
		/* Set voltage control register via SPM. */
		rc = msm_spm_set_vdd(vreg->cpu_num, vreg->vlevel);
		if (rc) {
			pr_debug("%s: msm_spm_set_vdd failed, rc=%d; falling back on SPMI write\n",
				vreg->rdesc.name, rc);
			spm_failed = true;
		}
	}

	if (unlikely(vreg->bypass_spm || spm_failed)) {
		/* Set voltage control register via SPMI. */
		reg = vreg->vlevel;
		rc = spmi_ext_register_writel(vreg->spmi_dev->ctrl,
			vreg->spmi_dev->sid,
			vreg->spmi_base_addr + QPNP_SMPS_REG_VOLTAGE_SETPOINT,
			&reg, 1);
		if (rc) {
			pr_err("%s: spmi_ext_register_writel failed, rc=%d\n",
				vreg->rdesc.name, rc);
			return rc;
		}
	}

	if (vreg->uV > vreg->last_set_uV) {
		/* Wait for voltage stepping to complete. */
		udelay(DIV_ROUND_UP(vreg->uV - vreg->last_set_uV,
					vreg->step_rate));
	}

	if ((vreg->regulator_type == QPNP_TYPE_FTS2)
	    && !(vreg->init_mode & QPNP_SMPS_MODE_PWM)
	    && vreg->uV > vreg->last_set_uV) {
		/* Wait for mode transition to complete. */
		udelay(QPNP_FTS2_MODE_CHANGE_DELAY - QPNP_SPMI_WRITE_MIN_DELAY);
		/* Switch to AUTO mode so that power consumption is lowered. */
		rc = qpnp_fts2_set_mode(vreg, QPNP_FTS2_MODE_AUTO);
		if (rc)
			return rc;
	}

	vreg->last_set_uV = vreg->uV;
	vreg->last_set_vlevel = vreg->vlevel;

	return rc;
}

static int spm_regulator_set_voltage(struct regulator_dev *rdev, int min_uV,
					int max_uV, unsigned *selector)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);
	const struct voltage_range *range = vreg->range;
	int uV = min_uV;
	unsigned vlevel;

	if (uV < range->set_point_min_uV && max_uV >= range->set_point_min_uV)
		uV = range->set_point_min_uV;

	if (uV < range->set_point_min_uV || uV > range->max_uV) {
		pr_err("%s: request v=[%d, %d] is outside possible v=[%d, %d]\n",
			vreg->rdesc.name, min_uV, max_uV,
			range->set_point_min_uV, range->max_uV);
		return -EINVAL;
	}

	vlevel = DIV_ROUND_UP(uV - range->min_uV, range->step_uV);
	uV = vlevel * range->step_uV + range->min_uV;

	if (uV > max_uV) {
		pr_err("%s: request v=[%d, %d] cannot be met by any set point\n",
			vreg->rdesc.name, min_uV, max_uV);
		return -EINVAL;
	}

	*selector = vlevel -
		(vreg->range->set_point_min_uV - vreg->range->min_uV)
			/ vreg->range->step_uV;

	/* Fix VSET for ULT HF Buck */
	if ((vreg->regulator_type == QPNP_TYPE_ULT_HF) &&
					(range == &ult_hf_range1)) {

		vlevel &= 0x1F;
		vlevel |= ULT_SMPS_RANGE_SPLIT;
	}

	vreg->vlevel = vlevel;
	vreg->uV = uV;

	if (!vreg->online)
		return 0;

	return _spm_regulator_set_voltage(rdev);
}

static int spm_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->uV;
}

static int spm_regulator_list_voltage(struct regulator_dev *rdev,
					unsigned selector)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);

	if (selector >= vreg->rdesc.n_voltages)
		return 0;

	return selector * vreg->range->step_uV + vreg->range->set_point_min_uV;
}

static int spm_regulator_enable(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = _spm_regulator_set_voltage(rdev);

	if (!rc)
		vreg->online = true;

	return rc;
}

static int spm_regulator_disable(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);

	vreg->online = false;

	return 0;
}

static int spm_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->online;
}

static struct regulator_ops spm_regulator_ops = {
	.get_voltage	= spm_regulator_get_voltage,
	.set_voltage	= spm_regulator_set_voltage,
	.list_voltage	= spm_regulator_list_voltage,
	.enable		= spm_regulator_enable,
	.disable	= spm_regulator_disable,
	.is_enabled	= spm_regulator_is_enabled,
};

static int qpnp_smps_check_type(struct spm_vreg *vreg)
{
	int rc;
	u8 type[2];

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_SMPS_REG_TYPE, type, 2);
	if (rc) {
		dev_err(&vreg->spmi_dev->dev, "%s: could not read type register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (type[0] == QPNP_FTS2_TYPE && type[1] == QPNP_FTS2_SUBTYPE) {
		vreg->regulator_type = QPNP_TYPE_FTS2;
	} else if (type[0] == QPNP_FTS2p5_TYPE
					&& type[1] == QPNP_FTS2p5_SUBTYPE) {
		vreg->regulator_type = QPNP_TYPE_FTS2p5;
	} else if (type[0] == QPNP_ULT_HF_TYPE
					&& type[1] == QPNP_ULT_HF_SUBTYPE) {
		vreg->regulator_type = QPNP_TYPE_ULT_HF;
	} else {
		dev_err(&vreg->spmi_dev->dev, "%s: invalid type=0x%02X, subtype=0x%02X register pair\n",
			 __func__, type[0], type[1]);
		return -ENODEV;
	};

	return rc;
}

static int qpnp_fts_init_range(struct spm_vreg *vreg,
	const struct voltage_range *range0, const struct voltage_range *range1)
{
	int rc;
	u8 reg = 0;

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_FTS2_REG_VOLTAGE_RANGE, &reg, 1);
	if (rc) {
		dev_err(&vreg->spmi_dev->dev, "%s: could not read voltage range register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (reg == 0x00) {
		vreg->range = range0;
	} else if (reg == 0x01) {
		vreg->range = range1;
	} else {
		dev_err(&vreg->spmi_dev->dev, "%s: voltage range=%d is invalid\n",
			__func__, reg);
		rc = -EINVAL;
	}

	return rc;
}

static int qpnp_ult_hf_init_range(struct spm_vreg *vreg)
{
	int rc;
	u8 reg = 0;

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_SMPS_REG_VOLTAGE_SETPOINT, &reg, 1);
	if (rc) {
		dev_err(&vreg->spmi_dev->dev, "%s: could not read voltage range register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	vreg->range = (reg < ULT_SMPS_RANGE_SPLIT) ? &ult_hf_range0 :
							&ult_hf_range1;
	return rc;
}

static int qpnp_smps_init_voltage(struct spm_vreg *vreg)
{
	int rc;
	u8 reg = 0;

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_SMPS_REG_VOLTAGE_SETPOINT, &reg, 1);
	if (rc) {
		dev_err(&vreg->spmi_dev->dev, "%s: could not read voltage setpoint register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	vreg->vlevel = reg;
	/*
	 * Calculate ULT HF buck VSET based on range:
	 * In case of range 0: VSET is a 7 bit value.
	 * In case of range 1: VSET is a 5 bit value
	 *
	 */
	if ((vreg->regulator_type == QPNP_TYPE_ULT_HF) &&
				(vreg->range == &ult_hf_range1))
		vreg->vlevel &= ~ULT_SMPS_RANGE_SPLIT;

	vreg->uV = vreg->vlevel * vreg->range->step_uV + vreg->range->min_uV;
	vreg->last_set_uV = vreg->uV;

	return rc;
}

static int qpnp_smps_init_mode(struct spm_vreg *vreg)
{
	const char *mode_name;
	int rc;

	rc = of_property_read_string(vreg->spmi_dev->dev.of_node, "qcom,mode",
					&mode_name);
	if (!rc) {
		if (strcmp("pwm", mode_name) == 0) {
			vreg->init_mode = QPNP_SMPS_MODE_PWM;
		} else if ((strcmp("auto", mode_name) == 0) &&
				(vreg->regulator_type == QPNP_TYPE_FTS2
				 || vreg->regulator_type == QPNP_TYPE_FTS2p5)) {
			vreg->init_mode = QPNP_FTS2_MODE_AUTO;
		} else {
			dev_err(&vreg->spmi_dev->dev, "%s: unknown regulator mode: %s\n",
				__func__, mode_name);
			return -EINVAL;
		}

		rc = spmi_ext_register_writel(vreg->spmi_dev->ctrl,
			vreg->spmi_dev->sid,
			vreg->spmi_base_addr + QPNP_SMPS_REG_MODE,
			&vreg->init_mode, 1);
		if (rc)
			dev_err(&vreg->spmi_dev->dev, "%s: could not write mode register, rc=%d\n",
				__func__, rc);
	} else {
		rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl,
			vreg->spmi_dev->sid,
			vreg->spmi_base_addr + QPNP_SMPS_REG_MODE,
			&vreg->init_mode, 1);
		if (rc)
			dev_err(&vreg->spmi_dev->dev, "%s: could not read mode register, rc=%d\n",
				__func__, rc);
	}

	return rc;
}

static int qpnp_smps_init_step_rate(struct spm_vreg *vreg)
{
	int rc;
	u8 reg = 0;
	int step = 0, delay;

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_SMPS_REG_STEP_CTRL, &reg, 1);
	if (rc) {
		dev_err(&vreg->spmi_dev->dev, "%s: could not read stepping control register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	/* ULT buck does not support steps */
	if (vreg->regulator_type != QPNP_TYPE_ULT_HF)
		step = (reg & QPNP_FTS2_STEP_CTRL_STEP_MASK)
			>> QPNP_FTS2_STEP_CTRL_STEP_SHIFT;

	delay = (reg & QPNP_SMPS_STEP_CTRL_DELAY_MASK)
		>> QPNP_SMPS_STEP_CTRL_DELAY_SHIFT;

	/* step_rate has units of uV/us. */
	vreg->step_rate = QPNP_FTS2_CLOCK_RATE * vreg->range->step_uV
				* (1 << step);

	if (vreg->regulator_type == QPNP_TYPE_ULT_HF)
		vreg->step_rate /= 1000 * (QPNP_ULT_HF_STEP_DELAY << delay);
	else
		vreg->step_rate /= 1000 * (QPNP_FTS2_STEP_DELAY << delay);

	vreg->step_rate = vreg->step_rate * QPNP_FTS2_STEP_MARGIN_NUM
				/ QPNP_FTS2_STEP_MARGIN_DEN;

	/* Ensure that the stepping rate is greater than 0. */
	vreg->step_rate = max(vreg->step_rate, 1);

	return rc;
}

static bool spm_regulator_using_range0(struct spm_vreg *vreg)
{
	return vreg->range == &fts2_range0 || vreg->range == &fts2p5_range0
		|| vreg->range == &ult_hf_range0;
}

static int spm_regulator_probe(struct spmi_device *spmi)
{
	struct regulator_config reg_config = {};
	struct device_node *node = spmi->dev.of_node;
	struct regulator_init_data *init_data;
	struct spm_vreg *vreg;
	struct resource *res;
	bool bypass_spm;
	int rc;

	if (!node) {
		dev_err(&spmi->dev, "%s: device node missing\n", __func__);
		return -ENODEV;
	}

	bypass_spm = of_property_read_bool(node, "qcom,bypass-spm");
	if (!bypass_spm) {
		rc = msm_spm_probe_done();
		if (rc) {
			if (rc != -EPROBE_DEFER)
				dev_err(&spmi->dev, "%s: spm unavailable, rc=%d\n",
					__func__, rc);
			return rc;
		}
	}

	vreg = devm_kzalloc(&spmi->dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg) {
		pr_err("allocation failed.\n");
		return -ENOMEM;
	}
	vreg->spmi_dev = spmi;
	vreg->bypass_spm = bypass_spm;

	res = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "%s: node is missing base address\n",
			__func__);
		return -EINVAL;
	}
	vreg->spmi_base_addr = res->start;

	rc = qpnp_smps_check_type(vreg);
	if (rc)
		return rc;

	/* Specify CPU 0 as default in order to handle shared regulator case. */
	vreg->cpu_num = 0;
	of_property_read_u32(vreg->spmi_dev->dev.of_node, "qcom,cpu-num",
						&vreg->cpu_num);

	/*
	 * The regulator must be initialized to range 0 or range 1 during
	 * PMIC power on sequence.  Once it is set, it cannot be changed
	 * dynamically.
	 */
	if (vreg->regulator_type == QPNP_TYPE_FTS2)
		rc = qpnp_fts_init_range(vreg, &fts2_range0, &fts2_range1);
	else if (vreg->regulator_type == QPNP_TYPE_FTS2p5)
		rc = qpnp_fts_init_range(vreg, &fts2p5_range0, &fts2p5_range1);
	else if (vreg->regulator_type == QPNP_TYPE_ULT_HF)
		rc = qpnp_ult_hf_init_range(vreg);
	if (rc)
		return rc;

	rc = qpnp_smps_init_voltage(vreg);
	if (rc)
		return rc;

	rc = qpnp_smps_init_mode(vreg);
	if (rc)
		return rc;

	rc = qpnp_smps_init_step_rate(vreg);
	if (rc)
		return rc;

	init_data = of_get_regulator_init_data(&spmi->dev, node);
	if (!init_data) {
		dev_err(&spmi->dev, "%s: unable to allocate memory\n",
				__func__);
		return -ENOMEM;
	}
	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
						| REGULATOR_CHANGE_VOLTAGE;

	if (!init_data->constraints.name) {
		dev_err(&spmi->dev, "%s: node is missing regulator name\n",
			__func__);
		return -EINVAL;
	}

	vreg->rdesc.name	= init_data->constraints.name;
	vreg->rdesc.type	= REGULATOR_VOLTAGE;
	vreg->rdesc.owner	= THIS_MODULE;
	vreg->rdesc.ops		= &spm_regulator_ops;
	vreg->rdesc.n_voltages
		= (vreg->range->max_uV - vreg->range->set_point_min_uV)
			/ vreg->range->step_uV + 1;

	reg_config.dev = &spmi->dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = vreg;
	reg_config.of_node = node;
	vreg->rdev = regulator_register(&vreg->rdesc, &reg_config);
	if (IS_ERR(vreg->rdev)) {
		rc = PTR_ERR(vreg->rdev);
		dev_err(&spmi->dev, "%s: regulator_register failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	dev_set_drvdata(&spmi->dev, vreg);

	pr_info("name=%s, range=%s, voltage=%d uV, mode=%s, step rate=%d uV/us\n",
		vreg->rdesc.name,
		spm_regulator_using_range0(vreg) ? "LV" : "MV",
		vreg->uV,
		vreg->init_mode & QPNP_SMPS_MODE_PWM ? "PWM" :
		    (vreg->init_mode & QPNP_FTS2_MODE_AUTO ? "AUTO" : "PFM"),
		vreg->step_rate);

	return rc;
}

static int spm_regulator_remove(struct spmi_device *spmi)
{
	struct spm_vreg *vreg = dev_get_drvdata(&spmi->dev);

	regulator_unregister(vreg->rdev);

	return 0;
}

static struct of_device_id spm_regulator_match_table[] = {
	{ .compatible = SPM_REGULATOR_DRIVER_NAME, },
	{}
};

static const struct spmi_device_id spm_regulator_id[] = {
	{ SPM_REGULATOR_DRIVER_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(spmi, spm_regulator_id);

static struct spmi_driver spm_regulator_driver = {
	.driver = {
		.name		= SPM_REGULATOR_DRIVER_NAME,
		.of_match_table = spm_regulator_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= spm_regulator_probe,
	.remove		= spm_regulator_remove,
	.id_table	= spm_regulator_id,
};

/**
 * spm_regulator_init() - register spmi driver for spm-regulator
 *
 * This initialization function should be called in systems in which driver
 * registration ordering must be controlled precisely.
 *
 * Returns 0 on success or errno on failure.
 */
int __init spm_regulator_init(void)
{
	static bool has_registered;

	if (has_registered)
		return 0;
	else
		has_registered = true;

	return spmi_driver_register(&spm_regulator_driver);
}
EXPORT_SYMBOL(spm_regulator_init);

static void __exit spm_regulator_exit(void)
{
	spmi_driver_unregister(&spm_regulator_driver);
}

arch_initcall(spm_regulator_init);
module_exit(spm_regulator_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SPM regulator driver");
MODULE_ALIAS("platform:spm-regulator");
