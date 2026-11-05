// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"AMOLED: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

/* Register definitions */
#define PERIPH_REVISION4			0x03
#define IBB_PERIPH_TYPE				0x20
#define AB_PERIPH_TYPE				0x24
#define OLEDB_PERIPH_TYPE			0x2C

#define PERIPH_SUBTYPE				0x05

/* AB */
#define PMD802X_AB_PULL_DOWN_CTL(chip)		(chip->ab_base + 0x47)
#define PM8350B_AB_LDO_PD_CTL(chip)		(chip->ab_base + 0x78)

/* PM8350B_AB_LDO_PD_CTL */
#define PM8350B_AB_PULLDN_EN_BIT		BIT(7)

/* PMD802X_AB_PULL_DOWN_CTL */
#define PMD802X_AB_PULLDN_EN_BIT		BIT(1)
#define PMD802X_AB_PULLDN_STRENGTH_BIT		BIT(0)
#define PMD802X_AB_PULLDN_STRENGTH_STRONG	1

/* IBB */
#define IBB_PD_CTL(chip)			(chip->ibb_base + 0x47)

/* IBB_PD_CTL */
#define ENABLE_PD_BIT				BIT(7)

#define IBB_DUAL_PHASE_CTL(chip)		(chip->ibb_base + 0x70)

/* IBB_DUAL_PHASE_CTL */
#define IBB_DUAL_PHASE_CTL_MASK			GENMASK(2, 0)
#define AUTO_DUAL_PHASE_BIT			BIT(2)
#define FORCE_DUAL_PHASE_BIT			BIT(1)
#define FORCE_SINGLE_PHASE_BIT			BIT(0)

/* IBB SPUR FSM/SQM CTL */
#define IBB_SPUR_CTL(chip)			(chip->ibb_base + 0xB6)
#define SPUR_FSM_EN				BIT(7)
#define SPUR_SQM_EN				BIT(6)

#define IBB_SPUR_FREQ_CTL(chip)			(chip->ibb_base + 0xB7)
#define FREQ_RES_SEL				BIT(0)

#define IBB_SPUR_FREQ_THRESH_HIGH(i)		(chip->ibb_base + 0xB8 + i*2)
#define IBB_SPUR_FREQ_THRESH_LOW(i)		(chip->ibb_base + 0xB9 + i*2)

#define MAX_SPUR_FREQ_BANDS			3
#define MAX_SPUR_FREQ_KHZ			248
#define AMOLED_SDAM_OFFSET			0xB8
#define SQM_TIMER_LOWER_LIMIT_MS		100
#define SQM_TIMER_UPPER_LIMIT_MS		10000

enum {
	SPUR_MITIGATION_DISABLED,
	SPUR_MITIGATION_ENABLED_WITHOUT_SQM,
	SPUR_MITIGATION_ENABLED_WITH_SQM,
};

struct amoled_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	struct device_node	*node;
	unsigned int		mode;
	bool			enabled;
};

struct oledb_regulator {
	struct amoled_regulator	vreg;

	/* DT params */
	bool			swire_control;
};

struct ab_regulator {
	struct amoled_regulator	vreg;
	u8			subtype;

	/* DT params */
	bool			swire_control;
	bool			pd_control;
};

struct ibb_regulator {
	struct amoled_regulator	vreg;
	u8			subtype;
	u8			rev4;

	/* DT params */
	bool			swire_control;
	bool			pd_control;
	bool			single_phase;

	/* ibb_spur_mitigation params */
	u32			spur_mitigation_level;
	u32			spur_sqm_timer_ms;
	u32			spur_freq_thresh_high[MAX_SPUR_FREQ_BANDS];
	u32			spur_freq_thresh_low[MAX_SPUR_FREQ_BANDS];
	bool			spur_freq_res_sel;
};

struct amoled_chip {
	struct device		*dev;
	struct regmap		*regmap;
	struct oledb_regulator	oledb;
	struct ab_regulator	ab;
	struct ibb_regulator	ibb;
	struct nvmem_cell	*nvmem_cell;
	/* DT params */
	u32			oledb_base;
	u32			ab_base;
	u32			ibb_base;
};

enum reg_type {
	OLEDB,
	AB,
	IBB,
};

enum ab_subtype {
	PM8350B_AB = 0x06,
	PMD802X_AB = 0x07,
};

enum ibb_subtype {
	PM8150A_IBB = 0x03,
	PM8350B_IBB = 0x04,
	PMD802X_IBB = 0x05,
};

enum ibb_rev4 {
	IBB_ANA_MAJOR_V1 = 0x01,
	IBB_ANA_MAJOR_V2 = 0x02,
};

static inline bool is_spur_mitigation_supported(struct ibb_regulator *ibb)
{
	switch (ibb->subtype) {
	case PMD802X_IBB:
		return true;
	case PM8350B_IBB:
		if (ibb->rev4 >= IBB_ANA_MAJOR_V2)
			return true;
		fallthrough;
	default:
		return false;
	}
}

static inline bool is_phase_ctrl_supported(struct ibb_regulator *ibb)
{
	if (ibb->subtype == PM8350B_IBB)
		return true;

	return false;
}

static int amoled_read(struct amoled_chip *chip,
			u16 addr, u8 *value, u8 count)
{
	int rc = 0;

	rc = regmap_bulk_read(chip->regmap, addr, value, count);
	if (rc < 0)
		pr_err("Failed to read from addr=0x%02x rc=%d\n", addr, rc);

	return rc;
}

static int amoled_write(struct amoled_chip *chip,
			u16 addr, u8 *value, u8 count)
{
	int rc;

	rc = regmap_bulk_write(chip->regmap, addr, value, count);
	if (rc < 0)
		pr_err("Failed to write to addr=0x%02x rc=%d\n", addr, rc);

	return rc;
}

static int amoled_masked_write(struct amoled_chip *chip,
				u16 addr, u8 mask, u8 value)
{
	int rc = 0;

	rc = regmap_update_bits(chip->regmap, addr, mask, value);
	if (rc < 0)
		pr_err("Failed to write addr=0x%02x value=0x%02x rc=%d\n",
			addr, value, rc);

	return rc;
}

/* AB regulator */

static int amoled_ab_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	return chip->ab.vreg.enabled;
}

static int amoled_ab_regulator_enable(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	chip->ab.vreg.enabled = true;
	return 0;
}

static int amoled_ab_regulator_disable(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	chip->ab.vreg.enabled = false;
	return 0;
}

/* IBB regulator */

static int amoled_ibb_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	return chip->ibb.vreg.enabled;
}

static int amoled_ibb_regulator_enable(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	chip->ibb.vreg.enabled = true;
	return 0;
}

static int amoled_ibb_regulator_disable(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	chip->ibb.vreg.enabled = false;
	return 0;
}

/* common to AB and IBB */

static int amoled_ab_ibb_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->ab.swire_control || chip->ibb.swire_control)
		return 0;

	return 0;
}

static int amoled_ab_ibb_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->ab.swire_control || chip->ibb.swire_control)
		return 0;

	return 0;
}

static int amoled_ab_pd_control(struct amoled_chip *chip, bool en)
{
	u8 val, mask;
	u16 addr;

	if (!chip->ab.pd_control)
		return 0;

	if (chip->ab.subtype == PM8350B_AB) {
		addr = PM8350B_AB_LDO_PD_CTL(chip);
		val = en ? PM8350B_AB_PULLDN_EN_BIT : 0;
		mask = PM8350B_AB_PULLDN_EN_BIT;
	} else if (chip->ab.subtype == PMD802X_AB) {
		addr = PMD802X_AB_PULL_DOWN_CTL(chip);
		val = en ? PMD802X_AB_PULLDN_EN_BIT |
			PMD802X_AB_PULLDN_STRENGTH_STRONG : 0;
		mask = PMD802X_AB_PULLDN_EN_BIT |
			PMD802X_AB_PULLDN_STRENGTH_BIT;
	} else {
		return -EINVAL;
	}

	return amoled_masked_write(chip, addr, mask, val);
}

static int amoled_ibb_pd_control(struct amoled_chip *chip, bool en)
{
	u8 val = en ? ENABLE_PD_BIT : 0;

	if (!chip->ibb.pd_control)
		return 0;

	return amoled_masked_write(chip, IBB_PD_CTL(chip), ENABLE_PD_BIT,
					val);
}

static int amoled_ab_ibb_regulator_set_mode(struct regulator_dev *rdev,
						unsigned int mode)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);
	int rc = 0;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_STANDBY &&
		mode != REGULATOR_MODE_IDLE) {
		pr_err("Unsupported mode %u\n", mode);
		return -EINVAL;
	}

	if (mode == chip->ab.vreg.mode || mode == chip->ibb.vreg.mode)
		return 0;

	pr_debug("mode: %d\n", mode);

	if (mode == REGULATOR_MODE_NORMAL || mode == REGULATOR_MODE_STANDBY) {
		rc = amoled_ibb_pd_control(chip, true);
		if (rc < 0)
			goto error;

		rc = amoled_ab_pd_control(chip, true);
		if (rc < 0)
			goto error;
	} else if (mode == REGULATOR_MODE_IDLE) {
		rc = amoled_ibb_pd_control(chip, false);
		if (rc < 0)
			goto error;

		rc = amoled_ab_pd_control(chip, false);
		if (rc < 0)
			goto error;
	}

	chip->ab.vreg.mode = chip->ibb.vreg.mode = mode;
error:
	if (rc < 0)
		pr_err("Failed to configure for mode %d\n", mode);
	return rc;
}

static unsigned int amoled_ab_ibb_regulator_get_mode(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	return chip->ibb.vreg.mode;
}

#define SINGLE_PHASE_ILIMIT_UA	30000

static int amoled_ibb_regulator_set_load(struct regulator_dev *rdev,
				int load_uA)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);
	u8 ibb_phase;

	if (!is_phase_ctrl_supported(&chip->ibb))
		return 0;

	/* For IBB single phase, it's configured only once. */
	if (chip->ibb.single_phase)
		return 0;

	if (load_uA < 0)
		return -EINVAL;
	else if (load_uA <= SINGLE_PHASE_ILIMIT_UA)
		ibb_phase = AUTO_DUAL_PHASE_BIT;
	else
		ibb_phase = FORCE_DUAL_PHASE_BIT;

	return amoled_masked_write(chip, IBB_DUAL_PHASE_CTL(chip),
			IBB_DUAL_PHASE_CTL_MASK, ibb_phase);
}

static const struct regulator_ops amoled_ab_ops = {
	.enable		= amoled_ab_regulator_enable,
	.disable	= amoled_ab_regulator_disable,
	.is_enabled	= amoled_ab_regulator_is_enabled,
	.set_voltage	= amoled_ab_ibb_regulator_set_voltage,
	.get_voltage	= amoled_ab_ibb_regulator_get_voltage,
	.set_mode	= amoled_ab_ibb_regulator_set_mode,
	.get_mode	= amoled_ab_ibb_regulator_get_mode,
};

static const struct regulator_ops amoled_ibb_ops = {
	.enable		= amoled_ibb_regulator_enable,
	.disable	= amoled_ibb_regulator_disable,
	.is_enabled	= amoled_ibb_regulator_is_enabled,
	.set_voltage	= amoled_ab_ibb_regulator_set_voltage,
	.get_voltage	= amoled_ab_ibb_regulator_get_voltage,
	.set_mode	= amoled_ab_ibb_regulator_set_mode,
	.get_mode	= amoled_ab_ibb_regulator_get_mode,
	.set_load	= amoled_ibb_regulator_set_load,
};

/* OLEDB regulator */

static int amoled_oledb_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	return chip->oledb.vreg.enabled;
}

static int amoled_oledb_regulator_enable(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.enabled = true;
	return 0;
}

static int amoled_oledb_regulator_disable(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.enabled = false;
	return 0;
}

static int amoled_oledb_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->oledb.swire_control)
		return 0;

	return 0;
}

static int amoled_oledb_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->oledb.swire_control)
		return 0;

	return 0;
}

static int amoled_oledb_regulator_set_mode(struct regulator_dev *rdev,
						unsigned int mode)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.mode = mode;
	return 0;
}

static unsigned int amoled_oledb_regulator_get_mode(struct regulator_dev *rdev)
{
	struct amoled_chip *chip  = rdev_get_drvdata(rdev);

	return chip->oledb.vreg.mode;
}

static const struct regulator_ops amoled_oledb_ops = {
	.enable		= amoled_oledb_regulator_enable,
	.disable	= amoled_oledb_regulator_disable,
	.is_enabled	= amoled_oledb_regulator_is_enabled,
	.set_voltage	= amoled_oledb_regulator_set_voltage,
	.get_voltage	= amoled_oledb_regulator_get_voltage,
	.set_mode	= amoled_oledb_regulator_set_mode,
	.get_mode	= amoled_oledb_regulator_get_mode,
};

static int amoled_regulator_register(struct amoled_chip *chip,
					enum reg_type type)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};
	struct regulator_desc *rdesc;
	struct regulator_dev *rdev;
	struct device_node *node;

	if (type == OLEDB) {
		node		= chip->oledb.vreg.node;
		rdesc		= &chip->oledb.vreg.rdesc;
		rdesc->ops	= &amoled_oledb_ops;
		rdev		= chip->oledb.vreg.rdev;
	} else if (type == AB) {
		node		= chip->ab.vreg.node;
		rdesc		= &chip->ab.vreg.rdesc;
		rdesc->ops	= &amoled_ab_ops;
		rdev		= chip->ab.vreg.rdev;
	} else if (type == IBB) {
		node		= chip->ibb.vreg.node;
		rdesc		= &chip->ibb.vreg.rdesc;
		rdesc->ops	= &amoled_ibb_ops;
		rdev		= chip->ibb.vreg.rdev;
	} else {
		pr_err("Invalid regulator type %d\n", type);
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(chip->dev, node, rdesc);
	if (!init_data) {
		pr_err("Failed to get regulator_init_data for type %d\n", type);
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		rdesc->owner	= THIS_MODULE;
		rdesc->type	= REGULATOR_VOLTAGE;
		rdesc->name	= init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = node;

		if (of_get_property(chip->dev->of_node, "parent-supply",
				NULL))
			init_data->supply_regulator = "parent";

		init_data->constraints.valid_ops_mask
				|= REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS
				| REGULATOR_CHANGE_MODE;
		init_data->constraints.valid_modes_mask
				|= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE
					| REGULATOR_MODE_STANDBY;

		rdev = devm_regulator_register(chip->dev, rdesc, &cfg);
		if (IS_ERR(rdev)) {
			rc = PTR_ERR(rdev);
			rdev = NULL;
			pr_err("Failed to register amoled regulator for type %d rc = %d\n",
				type, rc);
			return rc;
		}

		rc = devm_regulator_debug_register(chip->dev, rdev);
		if (rc) {
			pr_err("failed to register debug regulator rc=%d\n",
				rc);
			rc = 0;
		}

		if (type == OLEDB)
			chip->oledb.vreg.mode = REGULATOR_MODE_NORMAL;
		else if (type == IBB)
			chip->ibb.vreg.mode = REGULATOR_MODE_NORMAL;
		else
			chip->ab.vreg.mode = REGULATOR_MODE_NORMAL;
	} else {
		pr_err("regulator name missing for type %d\n", type);
		return -EINVAL;
	}

	return rc;
}

static int amoled_hw_init(struct amoled_chip *chip)
{
	int rc;
	u8 val;

	rc = amoled_regulator_register(chip, OLEDB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register OLEDB regulator rc=%d\n",
			rc);
		return rc;
	}

	rc = amoled_regulator_register(chip, AB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register AB regulator rc=%d\n",
			rc);
		return rc;
	}

	rc = amoled_regulator_register(chip, IBB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register IBB regulator rc=%d\n",
			rc);
		return rc;
	}

	if (is_phase_ctrl_supported(&chip->ibb) && chip->ibb.single_phase) {
		val = FORCE_SINGLE_PHASE_BIT;

		rc = amoled_masked_write(chip, IBB_DUAL_PHASE_CTL(chip),
			IBB_DUAL_PHASE_CTL_MASK, val);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int amoled_ibb_spur_parse_dt(struct amoled_chip *chip, struct device_node *node)
{
	int freq_array_len, rc, i;
	u32 spur_thres[2*MAX_SPUR_FREQ_BANDS];

	rc = of_property_read_u32(node,
				"qcom,ibb-spur-mitigation-level",
				&chip->ibb.spur_mitigation_level);
	if (rc < 0 || (chip->ibb.spur_mitigation_level == SPUR_MITIGATION_DISABLED)) {
		dev_dbg(chip->dev, "ibb spur mitigation DISABLED!");
		return rc;
	}

	if (chip->ibb.spur_mitigation_level == SPUR_MITIGATION_ENABLED_WITH_SQM) {

		of_property_read_u32(node, "qcom,ibb-spur-sqm-timer-ms",
					&chip->ibb.spur_sqm_timer_ms);

		chip->nvmem_cell = devm_nvmem_cell_get(chip->dev,
				"ibb_spur_sqm_timer");
		if (IS_ERR(chip->nvmem_cell)) {
			rc = PTR_ERR(chip->nvmem_cell);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev, "Failed to get nvmem-cells, rc=%d\n", rc);
			return rc;
		}
	}

	/*
	 * Read the step size - 1khz or 2khz.
	 *
	 * NOTE: Even if this is not defined, step size may still be
	 * set to 2khz indirectly, if any freq1/2/3 thresh limit
	 * is in range: 248khz < f < 496khz.
	 */
	chip->ibb.spur_freq_res_sel = of_property_read_bool(node,
					"qcom,ibb-spur-2khz-step-size");
	freq_array_len = of_property_count_elems_of_size(node,
				"qcom,ibb-spur-freq-thresholds", sizeof(u32));

	if (freq_array_len !=  2*MAX_SPUR_FREQ_BANDS) {
		dev_err(chip->dev, "invalid ibb spur freq threshold array size = %d\n",
			freq_array_len);
		chip->ibb.spur_mitigation_level = SPUR_MITIGATION_DISABLED;

		return -EINVAL;
	}

	rc = of_property_read_u32_array(node,
		"qcom,ibb-spur-freq-thresholds", spur_thres, freq_array_len);
	if (rc < 0) {
		dev_err(chip->dev, "failed to read thresholds = %d\n", rc);
		return rc;
	}

	for (i = 0; i < MAX_SPUR_FREQ_BANDS; i++) {
		chip->ibb.spur_freq_thresh_low[i] = spur_thres[2*i];
		chip->ibb.spur_freq_thresh_high[i] = spur_thres[(2*i)+1];
	}

	return rc;
}

static int amoled_parse_dt(struct amoled_chip *chip)
{
	struct device_node *temp, *node = chip->dev->of_node;
	const __be32 *prop_addr;
	int rc = 0;
	u32 base;
	u8 val[3];

	for_each_available_child_of_node(node, temp) {
		prop_addr = of_get_address(temp, 0, NULL, NULL);
		if (!prop_addr) {
			pr_err("Couldn't get reg address\n");
			return -EINVAL;
		}

		base = be32_to_cpu(*prop_addr);
		rc = amoled_read(chip, base + PERIPH_REVISION4, val, 3);
		if (rc < 0) {
			pr_err("Couldn't read PERIPH_REVISION4 for base %x\n", base);
			return rc;
		}

		switch (val[1]) {
		case OLEDB_PERIPH_TYPE:
			chip->oledb_base = base;
			chip->oledb.vreg.node = temp;
			chip->oledb.swire_control = of_property_read_bool(temp,
							"qcom,swire-control");
			break;
		case AB_PERIPH_TYPE:
			chip->ab_base = base;
			chip->ab.subtype = val[2];
			chip->ab.vreg.node = temp;
			chip->ab.swire_control = of_property_read_bool(temp,
							"qcom,swire-control");
			chip->ab.pd_control = of_property_read_bool(temp,
							"qcom,aod-pd-control");
			break;
		case IBB_PERIPH_TYPE:
			chip->ibb_base = base;
			chip->ibb.subtype = val[2];
			chip->ibb.rev4 = val[0];

			chip->ibb.vreg.node = temp;
			chip->ibb.swire_control = of_property_read_bool(temp,
							"qcom,swire-control");
			chip->ibb.pd_control = of_property_read_bool(temp,
							"qcom,aod-pd-control");
			chip->ibb.single_phase = of_property_read_bool(temp,
							"qcom,ibb-single-phase");

			if (is_spur_mitigation_supported(&chip->ibb)) {
				rc = amoled_ibb_spur_parse_dt(chip, temp);
				if (rc < 0)
					pr_err("Failed to parse ibb_spur_parse_dt\n");
			}

			break;
		default:
			pr_err("Unknown peripheral type 0x%x\n", val[0]);
			return -EINVAL;
		}
	}

	return 0;
}

static bool is_2khz_step_needed(struct amoled_chip *chip)
{
	u8 i;

	/*
	 * If any of the freq1/2/3 band has valid thresh
	 * (i.e f_high >= f_low)
	 * and freq values is in range of 248khz < f < 496khz
	 * then use step_size = 2khz
	 */
	for (i = 0; i < MAX_SPUR_FREQ_BANDS; i++) {
		if ((chip->ibb.spur_freq_thresh_high[i] > MAX_SPUR_FREQ_KHZ) &&
		   (chip->ibb.spur_freq_thresh_high[i]  < MAX_SPUR_FREQ_KHZ * 2) &&
		   (chip->ibb.spur_freq_thresh_high[i] >=
			chip->ibb.spur_freq_thresh_low[i])) {
			return true;
		}
	}

	return false;
}

static int amoled_ibb_spur_set_thresh(struct amoled_chip *chip)
{
	int i = 0, rc = 0;
	u16 low, high, max, temp = 0;

	if (!chip->ibb.spur_freq_res_sel)
		chip->ibb.spur_freq_res_sel = is_2khz_step_needed(chip);

	rc = amoled_masked_write(chip,
			IBB_SPUR_FREQ_CTL(chip),
			FREQ_RES_SEL,
			(chip->ibb.spur_freq_res_sel ? FREQ_RES_SEL : 0));
	if (rc < 0) {
		dev_err(chip->dev, "failed to write IBB_SPUR_CTL register!\n");
		return rc;
	}

	/* Calculate max based on the step size */
	max = MAX_SPUR_FREQ_KHZ * (chip->ibb.spur_freq_res_sel ? 2 : 1);

	for (i = 0; i < MAX_SPUR_FREQ_BANDS; i++) {
		low = chip->ibb.spur_freq_thresh_low[i];
		high = chip->ibb.spur_freq_thresh_high[i];

		if (high < low || low > max || high > max) {
			dev_err(chip->dev, "ibb spur freq band%d threshold invalid!\n",
				(i+1));

			/* Set both thresholds to max to in effect disable it */
			chip->ibb.spur_freq_thresh_high[i] = max;
			chip->ibb.spur_freq_thresh_low[i] = max;

			low = max;
			high = max;
		}

		/*
		 *For High threshold, roundoff-to-ceiling for odd frequency
		 * with 2khz step
		 */
		temp = high / (chip->ibb.spur_freq_res_sel ? 2 : 1);
		temp += chip->ibb.spur_freq_res_sel ? (high % 2) : 0;

		rc = amoled_write(chip, IBB_SPUR_FREQ_THRESH_HIGH(i),
				(u8 *)&temp, 1);
		if (rc < 0) {
			dev_err(chip->dev, "failed to write IBB_SPUR_FREQ_HIGH register!\n");
			return rc;
		}
		/*
		 * For Low threshold, roundoff-to-floor for odd frequency
		 * with 2khz step
		 */
		temp = low / (chip->ibb.spur_freq_res_sel ? 2 : 1);
		rc = amoled_write(chip, IBB_SPUR_FREQ_THRESH_LOW(i),
				(u8 *)&temp, 1);
		if (rc < 0) {
			dev_err(chip->dev, "failed to write IBB_SPUR_FREQ_LOW register!\n");
			return rc;
		}
	}

	return 0;
}

static int amoled_ibb_spur_set_sqm_timer(struct amoled_chip *chip, u16 sqm_timer)
{
	return nvmem_cell_write(chip->nvmem_cell,
				&sqm_timer,
				sizeof(sqm_timer));
}

static int amoled_ibb_spur_init(struct amoled_chip *chip)
{
	int rc = 0;

	switch (chip->ibb.spur_mitigation_level) {

	case SPUR_MITIGATION_ENABLED_WITH_SQM:
		/*set SQM mode */
		rc = amoled_masked_write(chip, IBB_SPUR_CTL(chip),
					SPUR_SQM_EN,
					SPUR_SQM_EN);
		if (rc < 0) {
			dev_err(chip->dev, "failed to enable spur SQM mode!\n");
			return rc;
		}

		/*set SQM timer if defined */
		if (chip->ibb.spur_sqm_timer_ms > SQM_TIMER_LOWER_LIMIT_MS  &&
			chip->ibb.spur_sqm_timer_ms < SQM_TIMER_UPPER_LIMIT_MS) {

			rc = amoled_ibb_spur_set_sqm_timer(chip,
					(u16)chip->ibb.spur_sqm_timer_ms);
			if (rc < 0) {
				if (rc != -EPROBE_DEFER)
					dev_err(chip->dev,
						"failed to enable spur SQM timer\n");
				return rc;
			}

		}

		fallthrough;

	case SPUR_MITIGATION_ENABLED_WITHOUT_SQM:

		rc = amoled_ibb_spur_set_thresh(chip);

		if (rc < 0) {
			dev_err(chip->dev, "failed to set spur thresholds!\n");
			return rc;
		}

		rc = amoled_masked_write(chip, IBB_SPUR_CTL(chip),
				SPUR_FSM_EN,
				SPUR_FSM_EN);
		if (rc < 0) {
			dev_err(chip->dev, "failed to enable spur FSM!\n");
			return rc;
		}

		break;

	case SPUR_MITIGATION_DISABLED:
	default:
		/* disable ibb spur FSM */
		rc = amoled_masked_write(chip, IBB_SPUR_CTL(chip),
				SPUR_FSM_EN,
				0);
		if (rc < 0) {
			dev_err(chip->dev, "failed to disable spur FSM!\n");
			return rc;
		}
	}

	return 0;
}

static int amoled_regulator_probe(struct platform_device *pdev)
{
	int rc;
	struct device_node *node;
	struct amoled_chip *chip;

	node = pdev->dev.of_node;
	if (!node) {
		pr_err("No nodes defined\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Failed to get the regmap handle\n");
		rc = -EINVAL;
		goto error;
	}

	dev_set_drvdata(&pdev->dev, chip);

	rc = amoled_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to parse DT params rc=%d\n", rc);
		goto error;
	}

	rc = amoled_hw_init(chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to initialize HW rc=%d\n", rc);

	if (is_spur_mitigation_supported(&chip->ibb)) {
		rc = amoled_ibb_spur_init(chip);
		if (rc < 0)
			dev_err(chip->dev, "Failed to init ibb spur settings rc=%d\n",
					rc);
	}

error:
	return rc;
}


static const struct of_device_id amoled_match_table[] = {
	{ .compatible = "qcom,amoled-regulator", },
	{}
};
MODULE_DEVICE_TABLE(of, amoled_match_table);

static struct platform_driver amoled_regulator_driver = {
	.driver		= {
		.name		= "qcom-amoled-regulator",
		.of_match_table	= amoled_match_table,
	},
	.probe		= amoled_regulator_probe,
};
module_platform_driver(amoled_regulator_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. AMOLED regulator driver");
MODULE_LICENSE("GPL");
