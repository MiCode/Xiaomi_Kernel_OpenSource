/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/qpnp/pin.h>

#define Q_REG_ADDR(q_spec, reg_index)	\
		((q_spec)->offset + reg_index)

#define Q_REG_STATUS1			0x8
#define Q_REG_STATUS1_VAL_MASK		0x1
#define Q_REG_STATUS1_GPIO_EN_REV0_MASK	0x2
#define Q_REG_STATUS1_GPIO_EN_MASK	0x80
#define Q_REG_STATUS1_MPP_EN_MASK	0x80

#define Q_NUM_CTL_REGS			0xD

/* revision registers base address offsets */
#define Q_REG_DIG_MINOR_REV		0x0
#define Q_REG_DIG_MAJOR_REV		0x1
#define Q_REG_ANA_MINOR_REV		0x2

/* type registers base address offsets */
#define Q_REG_TYPE			0x4
#define Q_REG_SUBTYPE			0x5

/* gpio peripheral type and subtype values */
#define Q_GPIO_TYPE			0x10
#define Q_GPIO_SUBTYPE_GPIO_4CH		0x1
#define Q_GPIO_SUBTYPE_GPIOC_4CH	0x5
#define Q_GPIO_SUBTYPE_GPIO_8CH		0x9
#define Q_GPIO_SUBTYPE_GPIOC_8CH	0xD
#define Q_GPIO_SUBTYPE_GPIO_LV		0x10
#define Q_GPIO_SUBTYPE_GPIO_MV		0x11

/* mpp peripheral type and subtype values */
#define Q_MPP_TYPE				0x11
#define Q_MPP_SUBTYPE_4CH_NO_ANA_OUT		0x3
#define Q_MPP_SUBTYPE_ULT_4CH_NO_ANA_OUT	0x4
#define Q_MPP_SUBTYPE_4CH_NO_SINK		0x5
#define Q_MPP_SUBTYPE_ULT_4CH_NO_SINK		0x6
#define Q_MPP_SUBTYPE_4CH_FULL_FUNC		0x7
#define Q_MPP_SUBTYPE_8CH_FULL_FUNC		0xF

/* control register base address offsets */
#define Q_REG_MODE_CTL			0x40
#define Q_REG_DIG_VIN_CTL		0x41
#define Q_REG_DIG_PULL_CTL		0x42
#define Q_REG_DIG_IN_CTL		0x43
#define Q_REG_DIG_OUT_SRC_CTL		0x44
#define Q_REG_DIG_OUT_CTL		0x45
#define Q_REG_EN_CTL			0x46
#define Q_REG_AOUT_CTL			0x48
#define Q_REG_AIN_CTL			0x4A
#define Q_REG_APASS_SEL_CTL		0x4A
#define Q_REG_SINK_CTL			0x4C

/* control register regs array indices */
#define Q_REG_I_MODE_CTL		0
#define Q_REG_I_DIG_VIN_CTL		1
#define Q_REG_I_DIG_PULL_CTL		2
#define Q_REG_I_DIG_IN_CTL		3
#define Q_REG_I_DIG_OUT_SRC_CTL		4
#define Q_REG_I_DIG_OUT_CTL		5
#define Q_REG_I_EN_CTL			6
#define Q_REG_I_AOUT_CTL		8
#define Q_REG_I_APASS_SEL_CTL		10
#define Q_REG_I_AIN_CTL			10
#define Q_REG_I_SINK_CTL		12

/* control reg: mode */
#define Q_REG_OUT_INVERT_SHIFT		0
#define Q_REG_OUT_INVERT_MASK		0x1
#define Q_REG_SRC_SEL_SHIFT		1
#define Q_REG_SRC_SEL_MASK		0xE
#define Q_REG_MODE_SEL_SHIFT		4
#define Q_REG_MODE_SEL_MASK		0x70
#define Q_REG_LV_MV_MODE_SEL_SHIFT	0
#define Q_REG_LV_MV_MODE_SEL_MASK	0x3

/* control reg: dig_out_src (GPIO LV/MV only) */
#define Q_REG_DIG_OUT_SRC_SRC_SEL_SHIFT 0
#define Q_REG_DIG_OUT_SRC_SRC_SEL_MASK	0xF
#define Q_REG_DIG_OUT_SRC_INVERT_SHIFT	7
#define Q_REG_DIG_OUT_SRC_INVERT_MASK	0x80

/* control reg: dig_vin */
#define Q_REG_VIN_SHIFT			0
#define Q_REG_VIN_MASK			0x7

/* control reg: dig_pull */
#define Q_REG_PULL_SHIFT		0
#define Q_REG_PULL_MASK			0x7

/* control reg: dig_out */
#define Q_REG_OUT_STRENGTH_SHIFT	0
#define Q_REG_OUT_STRENGTH_MASK		0x3
#define Q_REG_OUT_TYPE_SHIFT		4
#define Q_REG_OUT_TYPE_MASK		0x30

/* control reg: dig_in_ctl */
#define Q_REG_DTEST_SEL_SHIFT			0
#define Q_REG_DTEST_SEL_MASK			0xF
#define Q_REG_LV_MV_DTEST_SEL_CFG_SHIFT		0
#define Q_REG_LV_MV_DTEST_SEL_CFG_MASK		0x7
#define Q_REG_LV_MV_DTEST_SEL_EN_SHIFT		7
#define Q_REG_LV_MV_DTEST_SEL_EN_MASK		0x80

/* control reg: en */
#define Q_REG_MASTER_EN_SHIFT		7
#define Q_REG_MASTER_EN_MASK		0x80

/* control reg: ana_out */
#define Q_REG_AOUT_REF_SHIFT		0
#define Q_REG_AOUT_REF_MASK		0x7

/* control reg: ana_in */
#define Q_REG_AIN_ROUTE_SHIFT		0
#define Q_REG_AIN_ROUTE_MASK		0x7

/* control reg: sink */
#define Q_REG_CS_OUT_SHIFT		0
#define Q_REG_CS_OUT_MASK		0x7

/* control ref: apass_sel */
#define Q_REG_APASS_SEL_SHIFT		0
#define Q_REG_APASS_SEL_MASK		0x3

enum qpnp_pin_param_type {
	Q_PIN_CFG_MODE,
	Q_PIN_CFG_OUTPUT_TYPE,
	Q_PIN_CFG_INVERT,
	Q_PIN_CFG_PULL,
	Q_PIN_CFG_VIN_SEL,
	Q_PIN_CFG_OUT_STRENGTH,
	Q_PIN_CFG_SRC_SEL,
	Q_PIN_CFG_MASTER_EN,
	Q_PIN_CFG_AOUT_REF,
	Q_PIN_CFG_AIN_ROUTE,
	Q_PIN_CFG_CS_OUT,
	Q_PIN_CFG_APASS_SEL,
	Q_PIN_CFG_DTEST_SEL,
	Q_PIN_CFG_INVALID,
};

#define Q_NUM_PARAMS			Q_PIN_CFG_INVALID

/* param error checking */
#define QPNP_PIN_GPIO_MODE_INVALID		3
#define QPNP_PIN_GPIO_LV_MV_MODE_INVALID	4
#define QPNP_PIN_MPP_MODE_INVALID		7
#define QPNP_PIN_INVERT_INVALID			2
#define QPNP_PIN_OUT_BUF_INVALID		3
#define QPNP_PIN_GPIO_LV_MV_OUT_BUF_INVALID	4
#define QPNP_PIN_VIN_4CH_INVALID		5
#define QPNP_PIN_VIN_8CH_INVALID		8
#define QPNP_PIN_GPIO_LV_VIN_INVALID		1
#define QPNP_PIN_GPIO_MV_VIN_INVALID		2
#define QPNP_PIN_GPIO_PULL_INVALID		6
#define QPNP_PIN_MPP_PULL_INVALID		4
#define QPNP_PIN_OUT_STRENGTH_INVALID		4
#define QPNP_PIN_SRC_INVALID			8
#define QPNP_PIN_GPIO_LV_MV_SRC_INVALID		16
#define QPNP_PIN_MASTER_INVALID			2
#define QPNP_PIN_AOUT_REF_INVALID		8
#define QPNP_PIN_AIN_ROUTE_INVALID		8
#define QPNP_PIN_CS_OUT_INVALID			8
#define QPNP_PIN_APASS_SEL_INVALID		4
#define QPNP_PIN_DTEST_SEL_INVALID		4

struct qpnp_pin_spec {
	uint8_t slave;			/* 0-15 */
	uint16_t offset;		/* 0-255 */
	uint32_t gpio_chip_idx;		/* offset from gpio_chip base */
	uint32_t pmic_pin;		/* PMIC pin number */
	int irq;			/* logical IRQ number */
	u8 regs[Q_NUM_CTL_REGS];	/* Control regs */
	u8 num_ctl_regs;		/* usable number on this pin */
	u8 type;			/* peripheral type */
	u8 subtype;			/* peripheral subtype */
	u8 dig_major_rev;
	struct device_node *node;
	enum qpnp_pin_param_type params[Q_NUM_PARAMS];
	struct qpnp_pin_chip *q_chip;
};

struct qpnp_pin_chip {
	struct gpio_chip	gpio_chip;
	struct platform_device	*pdev;
	struct regmap		*regmap;
	struct qpnp_pin_spec	**pmic_pins;
	struct qpnp_pin_spec	**chip_gpios;
	uint32_t		pmic_pin_lowest;
	uint32_t		pmic_pin_highest;
	struct device_node	*int_ctrl;
	struct list_head	chip_list;
	struct dentry		*dfs_dir;
	bool			chip_registered;
};

static LIST_HEAD(qpnp_pin_chips);
static DEFINE_MUTEX(qpnp_pin_chips_lock);

static inline void qpnp_pmic_pin_set_spec(struct qpnp_pin_chip *q_chip,
					      uint32_t pmic_pin,
					      struct qpnp_pin_spec *spec)
{
	q_chip->pmic_pins[pmic_pin - q_chip->pmic_pin_lowest] = spec;
}

static inline struct qpnp_pin_spec *qpnp_pmic_pin_get_spec(
						struct qpnp_pin_chip *q_chip,
						uint32_t pmic_pin)
{
	if (pmic_pin < q_chip->pmic_pin_lowest ||
	    pmic_pin > q_chip->pmic_pin_highest)
		return NULL;

	return q_chip->pmic_pins[pmic_pin - q_chip->pmic_pin_lowest];
}

static inline struct qpnp_pin_spec *qpnp_chip_gpio_get_spec(
						struct qpnp_pin_chip *q_chip,
						uint32_t chip_gpio)
{
	if (chip_gpio >= q_chip->gpio_chip.ngpio)
		return NULL;

	return q_chip->chip_gpios[chip_gpio];
}

static inline void qpnp_chip_gpio_set_spec(struct qpnp_pin_chip *q_chip,
					      uint32_t chip_gpio,
					      struct qpnp_pin_spec *spec)
{
	q_chip->chip_gpios[chip_gpio] = spec;
}

static bool is_gpio_lv_mv(struct qpnp_pin_spec *q_spec)
{
	if ((q_spec->type == Q_GPIO_TYPE) &&
		(q_spec->subtype == Q_GPIO_SUBTYPE_GPIO_LV ||
		q_spec->subtype == Q_GPIO_SUBTYPE_GPIO_MV))
		return true;

	return false;
}

/*
 * Determines whether a specified param's configuration is correct.
 * This check is two tier. First a check is done whether the hardware
 * supports this param and value requested. The second check validates
 * that the configuration is correct, given the fact that the hardware
 * supports it.
 *
 * Returns
 *	-ENXIO is the hardware does not support this param.
 *	-EINVAL if the the hardware does support this param, but the
 *	requested value is outside the supported range.
 */
static int qpnp_pin_check_config(enum qpnp_pin_param_type idx,
				 struct qpnp_pin_spec *q_spec, uint32_t val)
{
	u8 subtype = q_spec->subtype;

	switch (idx) {
	case Q_PIN_CFG_MODE:
		if (q_spec->type == Q_GPIO_TYPE) {
			if (is_gpio_lv_mv(q_spec)) {
				if (val >= QPNP_PIN_GPIO_LV_MV_MODE_INVALID)
					return -EINVAL;
			} else if (val >= QPNP_PIN_GPIO_MODE_INVALID) {
					return -EINVAL;
			}
		} else if (q_spec->type == Q_MPP_TYPE) {
			if (val >= QPNP_PIN_MPP_MODE_INVALID)
				return -EINVAL;
			if ((subtype == Q_MPP_SUBTYPE_ULT_4CH_NO_ANA_OUT ||
			     subtype == Q_MPP_SUBTYPE_ULT_4CH_NO_SINK) &&
			     (val == QPNP_PIN_MODE_BIDIR))
				return -ENXIO;
		}
		break;
	case Q_PIN_CFG_OUTPUT_TYPE:
		if (q_spec->type != Q_GPIO_TYPE)
			return -ENXIO;
		if ((val == QPNP_PIN_OUT_BUF_OPEN_DRAIN_NMOS ||
		    val == QPNP_PIN_OUT_BUF_OPEN_DRAIN_PMOS) &&
		    (subtype == Q_GPIO_SUBTYPE_GPIOC_4CH ||
		    (subtype == Q_GPIO_SUBTYPE_GPIOC_8CH)))
			return -EINVAL;
		else if (is_gpio_lv_mv(q_spec) &&
			val >= QPNP_PIN_GPIO_LV_MV_OUT_BUF_INVALID)
			return -EINVAL;
		else if (val >= QPNP_PIN_OUT_BUF_INVALID)
			return -EINVAL;
		break;
	case Q_PIN_CFG_INVERT:
		if (val >= QPNP_PIN_INVERT_INVALID)
			return -EINVAL;
		break;
	case Q_PIN_CFG_PULL:
		if (q_spec->type == Q_GPIO_TYPE &&
		    val >= QPNP_PIN_GPIO_PULL_INVALID)
			return -EINVAL;
		if (q_spec->type == Q_MPP_TYPE) {
			if (val >= QPNP_PIN_MPP_PULL_INVALID)
				return -EINVAL;
			if (subtype == Q_MPP_SUBTYPE_ULT_4CH_NO_ANA_OUT ||
			    subtype == Q_MPP_SUBTYPE_ULT_4CH_NO_SINK)
				return -ENXIO;
		}
		break;
	case Q_PIN_CFG_VIN_SEL:
		if (is_gpio_lv_mv(q_spec)) {
			if (subtype == Q_GPIO_SUBTYPE_GPIO_LV) {
				if (val >= QPNP_PIN_GPIO_LV_VIN_INVALID)
					return -EINVAL;
			} else {
				if (val >= QPNP_PIN_GPIO_MV_VIN_INVALID)
					return -EINVAL;
			}
		} else if (val >= QPNP_PIN_VIN_8CH_INVALID) {
			return -EINVAL;
		} else if (val >= QPNP_PIN_VIN_4CH_INVALID) {
			if (q_spec->type == Q_GPIO_TYPE &&
			   (subtype == Q_GPIO_SUBTYPE_GPIO_4CH ||
			    subtype == Q_GPIO_SUBTYPE_GPIOC_4CH))
				return -EINVAL;
			if (q_spec->type == Q_MPP_TYPE &&
			   (subtype == Q_MPP_SUBTYPE_4CH_NO_ANA_OUT ||
			    subtype == Q_MPP_SUBTYPE_4CH_NO_SINK ||
			    subtype == Q_MPP_SUBTYPE_4CH_FULL_FUNC ||
			    subtype == Q_MPP_SUBTYPE_ULT_4CH_NO_ANA_OUT ||
			    subtype == Q_MPP_SUBTYPE_ULT_4CH_NO_SINK))
				return -EINVAL;
		}
		break;
	case Q_PIN_CFG_OUT_STRENGTH:
		if (q_spec->type != Q_GPIO_TYPE)
			return -ENXIO;
		if (val >= QPNP_PIN_OUT_STRENGTH_INVALID ||
		    val == 0)
			return -EINVAL;
		break;
	case Q_PIN_CFG_SRC_SEL:
		if (q_spec->type == Q_MPP_TYPE &&
		    (val == QPNP_PIN_SEL_FUNC_1 ||
		     val == QPNP_PIN_SEL_FUNC_2))
			return -EINVAL;
		if (is_gpio_lv_mv(q_spec)) {
			if (val >= QPNP_PIN_GPIO_LV_MV_SRC_INVALID)
				return -EINVAL;
		} else if (val >= QPNP_PIN_SRC_INVALID) {
			return -EINVAL;
		}
		break;
	case Q_PIN_CFG_MASTER_EN:
		if (val >= QPNP_PIN_MASTER_INVALID)
			return -EINVAL;
		break;
	case Q_PIN_CFG_AOUT_REF:
		if (q_spec->type != Q_MPP_TYPE)
			return -ENXIO;
		if (subtype == Q_MPP_SUBTYPE_4CH_NO_ANA_OUT ||
		    subtype == Q_MPP_SUBTYPE_ULT_4CH_NO_ANA_OUT)
			return -ENXIO;
		if (val >= QPNP_PIN_AOUT_REF_INVALID)
			return -EINVAL;
		break;
	case Q_PIN_CFG_AIN_ROUTE:
		if (q_spec->type != Q_MPP_TYPE)
			return -ENXIO;
		if (val >= QPNP_PIN_AIN_ROUTE_INVALID)
			return -EINVAL;
		break;
	case Q_PIN_CFG_CS_OUT:
		if (q_spec->type != Q_MPP_TYPE)
			return -ENXIO;
		if (subtype == Q_MPP_SUBTYPE_4CH_NO_SINK ||
		    subtype == Q_MPP_SUBTYPE_ULT_4CH_NO_SINK)
			return -ENXIO;
		if (val >= QPNP_PIN_CS_OUT_INVALID)
			return -EINVAL;
		break;
	case Q_PIN_CFG_APASS_SEL:
		if (!is_gpio_lv_mv(q_spec))
			return -ENXIO;
		if (val >= QPNP_PIN_APASS_SEL_INVALID)
			return -EINVAL;
		break;
	case Q_PIN_CFG_DTEST_SEL:
		if (val > QPNP_PIN_DTEST_SEL_INVALID)
			return -EINVAL;
		break;
	default:
		pr_err("invalid param type %u specified\n", idx);
		return -EINVAL;
	}
	return 0;
}

#define Q_CHK_INVALID(idx, q_spec, val) \
	(qpnp_pin_check_config(idx, q_spec, val) == -EINVAL)

static int qpnp_pin_check_constraints(struct qpnp_pin_spec *q_spec,
				      struct qpnp_pin_cfg *param)
{
	int pin = q_spec->pmic_pin;
	const char *name;

	name = (q_spec->type == Q_GPIO_TYPE) ? "gpio" : "mpp";

	if (Q_CHK_INVALID(Q_PIN_CFG_MODE, q_spec, param->mode))
		pr_err("invalid direction value %d for %s %d\n",
						param->mode, name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_INVERT, q_spec, param->invert))
		pr_err("invalid invert polarity value %d for %s %d\n",
						param->invert,  name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_SRC_SEL, q_spec, param->src_sel))
		pr_err("invalid source select value %d for %s %d\n",
						param->src_sel, name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_OUT_STRENGTH,
						q_spec, param->out_strength))
		pr_err("invalid out strength value %d for %s %d\n",
					param->out_strength,  name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_OUTPUT_TYPE,
						 q_spec, param->output_type))
		pr_err("invalid out type value %d for %s %d\n",
					param->output_type,  name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_VIN_SEL, q_spec, param->vin_sel))
		pr_err("invalid vin select %d value for %s %d\n",
						param->vin_sel, name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_PULL, q_spec, param->pull))
		pr_err("invalid pull value %d for pin %s %d\n",
						param->pull,  name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_MASTER_EN, q_spec, param->master_en))
		pr_err("invalid master_en value %d for %s %d\n",
						param->master_en, name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_AOUT_REF, q_spec, param->aout_ref))
		pr_err("invalid aout_reg value %d for %s %d\n",
						param->aout_ref, name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_AIN_ROUTE, q_spec, param->ain_route))
		pr_err("invalid ain_route value %d for %s %d\n",
						param->ain_route, name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_CS_OUT, q_spec, param->cs_out))
		pr_err("invalid cs_out value %d for %s %d\n",
						param->cs_out, name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_APASS_SEL, q_spec, param->apass_sel))
		pr_err("invalid apass_sel value %d for %s %d\n",
						param->apass_sel, name, pin);
	else if (Q_CHK_INVALID(Q_PIN_CFG_DTEST_SEL, q_spec, param->dtest_sel))
		pr_err("invalid dtest_sel value %d for %s %d\n",
					param->dtest_sel, name, pin);
	else
		return 0;

	return -EINVAL;
}

static inline u8 q_reg_get(u8 *reg, int shift, int mask)
{
	return (*reg & mask) >> shift;
}

static inline void q_reg_set(u8 *reg, int shift, int mask, int value)
{
	*reg |= (value << shift) & mask;
}

static inline void q_reg_clr_set(u8 *reg, int shift, int mask, int value)
{
	*reg &= ~mask;
	*reg |= (value << shift) & mask;
}

/*
 * Calculate the minimum number of registers that must be read / written
 * in order to satisfy the full feature set of the given pin.
 */
static int qpnp_pin_ctl_regs_init(struct qpnp_pin_spec *q_spec)
{
	if (q_spec->type == Q_GPIO_TYPE) {
		if (is_gpio_lv_mv(q_spec))
			q_spec->num_ctl_regs = 11;
		else
			q_spec->num_ctl_regs = 7;
	} else if (q_spec->type == Q_MPP_TYPE) {
		switch (q_spec->subtype) {
		case Q_MPP_SUBTYPE_4CH_NO_SINK:
		case Q_MPP_SUBTYPE_ULT_4CH_NO_SINK:
			q_spec->num_ctl_regs = 12;
			break;
		case Q_MPP_SUBTYPE_4CH_NO_ANA_OUT:
		case Q_MPP_SUBTYPE_ULT_4CH_NO_ANA_OUT:
		case Q_MPP_SUBTYPE_4CH_FULL_FUNC:
		case Q_MPP_SUBTYPE_8CH_FULL_FUNC:
			q_spec->num_ctl_regs = 13;
			break;
		default:
			pr_err("Invalid MPP subtype 0x%x\n", q_spec->subtype);
			return -EINVAL;
		}
	} else {
		pr_err("Invalid type 0x%x\n", q_spec->type);
		return -EINVAL;
	}
	return 0;
}

static int qpnp_pin_read_regs(struct qpnp_pin_chip *q_chip,
			      struct qpnp_pin_spec *q_spec)
{
	int bytes_left = q_spec->num_ctl_regs;
	int rc;
	char *buf_p = &q_spec->regs[0];
	u16 reg_addr = Q_REG_ADDR(q_spec, Q_REG_MODE_CTL);

	while (bytes_left > 0) {
		rc = regmap_bulk_read(q_chip->regmap, reg_addr, buf_p,
				      bytes_left < 8 ? bytes_left : 8);
		if (rc)
			return rc;
		bytes_left -= 8;
		buf_p += 8;
		reg_addr += 8;
	}
	return 0;
}

static int qpnp_pin_write_regs(struct qpnp_pin_chip *q_chip,
			       struct qpnp_pin_spec *q_spec)
{
	int bytes_left = q_spec->num_ctl_regs;
	int rc;
	char *buf_p = &q_spec->regs[0];
	u16 reg_addr = Q_REG_ADDR(q_spec, Q_REG_MODE_CTL);

	while (bytes_left > 0) {
		rc = regmap_bulk_write(q_chip->regmap, reg_addr, buf_p,
				       bytes_left < 8 ? bytes_left : 8);
		if (rc)
			return rc;
		bytes_left -= 8;
		buf_p += 8;
		reg_addr += 8;
	}
	return 0;
}

static int qpnp_pin_cache_regs(struct qpnp_pin_chip *q_chip,
			       struct qpnp_pin_spec *q_spec)
{
	int rc;
	struct device *dev = &q_chip->pdev->dev;

	rc = qpnp_pin_read_regs(q_chip, q_spec);
	if (rc)
		dev_err(dev, "%s: unable to read control regs\n", __func__);

	return rc;
}

#define Q_HAVE_HW_SP(idx, q_spec, val) \
	(qpnp_pin_check_config(idx, q_spec, val) == 0)

static int _qpnp_pin_config(struct qpnp_pin_chip *q_chip,
			    struct qpnp_pin_spec *q_spec,
			    struct qpnp_pin_cfg *param)
{
	struct device *dev = &q_chip->pdev->dev;
	int rc;
	u8 shift, mask, *reg;

	rc = qpnp_pin_check_constraints(q_spec, param);
	if (rc)
		goto gpio_cfg;

	/* set mode */
	if (Q_HAVE_HW_SP(Q_PIN_CFG_MODE, q_spec, param->mode)) {
		if (is_gpio_lv_mv(q_spec)) {
			shift = Q_REG_LV_MV_MODE_SEL_SHIFT;
			mask = Q_REG_LV_MV_MODE_SEL_MASK;
		} else {
			shift = Q_REG_MODE_SEL_SHIFT;
			mask = Q_REG_MODE_SEL_MASK;
		}
		q_reg_clr_set(&q_spec->regs[Q_REG_I_MODE_CTL],
			shift, mask, param->mode);
	}

	/* output specific configuration */
	if (Q_HAVE_HW_SP(Q_PIN_CFG_INVERT, q_spec, param->invert)) {
		if (is_gpio_lv_mv(q_spec)) {
			shift = Q_REG_DIG_OUT_SRC_INVERT_SHIFT;
			mask = Q_REG_DIG_OUT_SRC_INVERT_MASK;
			reg = &q_spec->regs[Q_REG_I_DIG_OUT_SRC_CTL];
		} else {
			shift = Q_REG_OUT_INVERT_SHIFT;
			mask = Q_REG_OUT_INVERT_MASK;
			reg = &q_spec->regs[Q_REG_I_MODE_CTL];
		}
		q_reg_clr_set(reg, shift, mask, param->invert);
	}

	if (Q_HAVE_HW_SP(Q_PIN_CFG_SRC_SEL, q_spec, param->src_sel)) {
		if (is_gpio_lv_mv(q_spec)) {
			shift = Q_REG_DIG_OUT_SRC_SRC_SEL_SHIFT;
			mask = Q_REG_DIG_OUT_SRC_SRC_SEL_MASK;
			reg = &q_spec->regs[Q_REG_I_DIG_OUT_SRC_CTL];
		} else {
			shift = Q_REG_SRC_SEL_SHIFT;
			mask = Q_REG_SRC_SEL_MASK;
			reg = &q_spec->regs[Q_REG_I_MODE_CTL];
		}
		q_reg_clr_set(reg, shift, mask, param->src_sel);
	}

	if (Q_HAVE_HW_SP(Q_PIN_CFG_OUT_STRENGTH, q_spec, param->out_strength))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_OUT_CTL],
			  Q_REG_OUT_STRENGTH_SHIFT, Q_REG_OUT_STRENGTH_MASK,
			  param->out_strength);
	if (Q_HAVE_HW_SP(Q_PIN_CFG_OUTPUT_TYPE, q_spec, param->output_type))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_OUT_CTL],
			  Q_REG_OUT_TYPE_SHIFT, Q_REG_OUT_TYPE_MASK,
			  param->output_type);

	/* input config */
	if (Q_HAVE_HW_SP(Q_PIN_CFG_DTEST_SEL, q_spec, param->dtest_sel)
			&& param->dtest_sel) {
		if (is_gpio_lv_mv(q_spec)) {
			q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_IN_CTL],
					Q_REG_LV_MV_DTEST_SEL_CFG_SHIFT,
					Q_REG_LV_MV_DTEST_SEL_CFG_MASK,
					param->dtest_sel - 1);
			q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_IN_CTL],
					Q_REG_LV_MV_DTEST_SEL_EN_SHIFT,
					Q_REG_LV_MV_DTEST_SEL_EN_MASK, 0x1);
		} else {
			q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_IN_CTL],
					Q_REG_DTEST_SEL_SHIFT,
					Q_REG_DTEST_SEL_MASK,
					BIT(param->dtest_sel - 1));
		}
	}

	/* config applicable for both input / output */
	if (Q_HAVE_HW_SP(Q_PIN_CFG_VIN_SEL, q_spec, param->vin_sel))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_VIN_CTL],
			  Q_REG_VIN_SHIFT, Q_REG_VIN_MASK,
			  param->vin_sel);
	if (Q_HAVE_HW_SP(Q_PIN_CFG_PULL, q_spec, param->pull))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_PULL_CTL],
			  Q_REG_PULL_SHIFT, Q_REG_PULL_MASK,
			  param->pull);
	if (Q_HAVE_HW_SP(Q_PIN_CFG_MASTER_EN, q_spec, param->master_en))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_EN_CTL],
			  Q_REG_MASTER_EN_SHIFT, Q_REG_MASTER_EN_MASK,
			  param->master_en);

	/* mpp specific config */
	if (Q_HAVE_HW_SP(Q_PIN_CFG_AOUT_REF, q_spec, param->aout_ref))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_AOUT_CTL],
			  Q_REG_AOUT_REF_SHIFT, Q_REG_AOUT_REF_MASK,
			  param->aout_ref);
	if (Q_HAVE_HW_SP(Q_PIN_CFG_AIN_ROUTE, q_spec, param->ain_route))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_AIN_CTL],
			  Q_REG_AIN_ROUTE_SHIFT, Q_REG_AIN_ROUTE_MASK,
			  param->ain_route);
	if (Q_HAVE_HW_SP(Q_PIN_CFG_CS_OUT, q_spec, param->cs_out))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_SINK_CTL],
			  Q_REG_CS_OUT_SHIFT, Q_REG_CS_OUT_MASK,
			  param->cs_out);
	if (Q_HAVE_HW_SP(Q_PIN_CFG_APASS_SEL, q_spec, param->apass_sel))
		q_reg_clr_set(&q_spec->regs[Q_REG_I_APASS_SEL_CTL],
			  Q_REG_APASS_SEL_SHIFT, Q_REG_APASS_SEL_MASK,
			  param->apass_sel);

	rc = qpnp_pin_write_regs(q_chip, q_spec);
	if (rc) {
		dev_err(&q_chip->pdev->dev,
			"%s: unable to write master enable\n",
								__func__);
		goto gpio_cfg;
	}

	return 0;

gpio_cfg:
	dev_err(dev, "%s: unable to set default config for pmic pin %d\n",
						__func__, q_spec->pmic_pin);

	return rc;
}

int qpnp_pin_config(int gpio, struct qpnp_pin_cfg *param)
{
	int rc, chip_offset;
	struct qpnp_pin_chip *q_chip;
	struct qpnp_pin_spec *q_spec = NULL;
	struct gpio_chip *gpio_chip;

	if (param == NULL)
		return -EINVAL;

	mutex_lock(&qpnp_pin_chips_lock);
	list_for_each_entry(q_chip, &qpnp_pin_chips, chip_list) {
		gpio_chip = &q_chip->gpio_chip;
		if (gpio >= gpio_chip->base
				&& gpio < gpio_chip->base + gpio_chip->ngpio) {
			chip_offset = gpio - gpio_chip->base;
			q_spec = qpnp_chip_gpio_get_spec(q_chip, chip_offset);
			if (WARN_ON(!q_spec)) {
				mutex_unlock(&qpnp_pin_chips_lock);
				return -ENODEV;
			}
			break;
		}
	}
	mutex_unlock(&qpnp_pin_chips_lock);

	if (!q_spec)
		return -ENODEV;

	rc = _qpnp_pin_config(q_chip, q_spec, param);

	return rc;
}
EXPORT_SYMBOL(qpnp_pin_config);

#define Q_MAX_CHIP_NAME 128
int qpnp_pin_map(const char *name, uint32_t pmic_pin)
{
	struct qpnp_pin_chip *q_chip;
	struct qpnp_pin_spec *q_spec = NULL;

	mutex_lock(&qpnp_pin_chips_lock);
	list_for_each_entry(q_chip, &qpnp_pin_chips, chip_list) {
		if (strncmp(q_chip->gpio_chip.label, name,
							Q_MAX_CHIP_NAME) != 0)
			continue;
		if (q_chip->pmic_pin_lowest <= pmic_pin &&
		    q_chip->pmic_pin_highest >= pmic_pin) {
			q_spec = qpnp_pmic_pin_get_spec(q_chip, pmic_pin);
			mutex_unlock(&qpnp_pin_chips_lock);
			if (WARN_ON(!q_spec))
				return -ENODEV;
			return q_chip->gpio_chip.base + q_spec->gpio_chip_idx;
		}
	}
	mutex_unlock(&qpnp_pin_chips_lock);
	return -EINVAL;
}
EXPORT_SYMBOL(qpnp_pin_map);

static int qpnp_pin_to_irq(struct gpio_chip *gpio_chip, unsigned offset)
{
	struct qpnp_pin_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_pin_spec *q_spec;
	struct of_phandle_args oirq;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (!q_spec)
		return -EINVAL;

	/* if we have mapped this pin previously return the virq */
	if (q_spec->irq)
		return q_spec->irq;

	/* call into irq_domain to get irq mapping */
	oirq.np = q_chip->int_ctrl;
	oirq.args[0] = to_spmi_device(q_chip->pdev->dev.parent)->usid;
	oirq.args[1] = (q_spec->offset >> 8) & 0xFF;
	oirq.args[2] = 0;
	oirq.args[3] = IRQ_TYPE_NONE;
	oirq.args_count = 4;

	q_spec->irq = irq_create_of_mapping(&oirq);
	if (!q_spec->irq) {
		dev_err(&q_chip->pdev->dev, "%s: invalid irq for gpio %u\n",
						__func__, q_spec->pmic_pin);
		WARN_ON(1);
		return -EINVAL;
	}

	return q_spec->irq;
}

static int qpnp_pin_get(struct gpio_chip *gpio_chip, unsigned offset)
{
	int rc, ret_val;
	struct qpnp_pin_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_pin_spec *q_spec = NULL;
	u8 buf[1], en_mask;
	u8 shift, mask, reg;
	int val;

	if (WARN_ON(!q_chip))
		return -ENODEV;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (WARN_ON(!q_spec))
		return -ENODEV;

	if (is_gpio_lv_mv(q_spec)) {
		mask = Q_REG_LV_MV_MODE_SEL_MASK;
		shift = Q_REG_LV_MV_MODE_SEL_SHIFT;
	} else {
		mask = Q_REG_MODE_SEL_MASK;
		shift = Q_REG_MODE_SEL_SHIFT;
	}

	/* gpio val is from RT status iff input is enabled */
	if (q_reg_get(&q_spec->regs[Q_REG_I_MODE_CTL], shift, mask)
					== QPNP_PIN_MODE_DIG_IN) {
		rc = regmap_read(q_chip->regmap,
				 Q_REG_ADDR(q_spec, Q_REG_STATUS1), &val);
		buf[0] = (u8)val;

		if (q_spec->type == Q_GPIO_TYPE && q_spec->dig_major_rev == 0)
			en_mask = Q_REG_STATUS1_GPIO_EN_REV0_MASK;
		else if (q_spec->type == Q_GPIO_TYPE &&
			 q_spec->dig_major_rev > 0)
			en_mask = Q_REG_STATUS1_GPIO_EN_MASK;
		else /* MPP */
			en_mask = Q_REG_STATUS1_MPP_EN_MASK;

		if (!(buf[0] & en_mask))
			return -EPERM;

		return buf[0] & Q_REG_STATUS1_VAL_MASK;
	} else {
		if (is_gpio_lv_mv(q_spec)) {
			shift = Q_REG_DIG_OUT_SRC_INVERT_SHIFT;
			mask = Q_REG_DIG_OUT_SRC_INVERT_MASK;
			reg = q_spec->regs[Q_REG_I_DIG_OUT_SRC_CTL];
		} else {
			shift = Q_REG_OUT_INVERT_SHIFT;
			mask = Q_REG_OUT_INVERT_MASK;
			reg = q_spec->regs[Q_REG_I_MODE_CTL];
		}

		ret_val = (reg & mask) >> shift;
		return ret_val;
	}

	return 0;
}

static int __qpnp_pin_set(struct qpnp_pin_chip *q_chip,
			   struct qpnp_pin_spec *q_spec, int value)
{
	int rc;
	u8 shift, mask, *reg;
	u16 address;

	if (!q_chip || !q_spec)
		return -EINVAL;

	if (is_gpio_lv_mv(q_spec)) {
		shift = Q_REG_DIG_OUT_SRC_INVERT_SHIFT;
		mask = Q_REG_DIG_OUT_SRC_INVERT_MASK;
		reg = &q_spec->regs[Q_REG_I_DIG_OUT_SRC_CTL];
		address = Q_REG_ADDR(q_spec, Q_REG_DIG_OUT_SRC_CTL);
	} else {
		shift = Q_REG_OUT_INVERT_SHIFT;
		mask = Q_REG_OUT_INVERT_MASK;
		reg = &q_spec->regs[Q_REG_I_MODE_CTL];
		address = Q_REG_ADDR(q_spec, Q_REG_MODE_CTL);
	}

	q_reg_clr_set(reg, shift, mask, !!value);

	rc = regmap_write(q_chip->regmap, address, *reg);
	if (rc)
		dev_err(&q_chip->pdev->dev, "%s: spmi write failed\n",
								__func__);
	return rc;
}


static void qpnp_pin_set(struct gpio_chip *gpio_chip,
		unsigned offset, int value)
{
	struct qpnp_pin_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_pin_spec *q_spec;

	if (WARN_ON(!q_chip))
		return;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (WARN_ON(!q_spec))
		return;

	__qpnp_pin_set(q_chip, q_spec, value);
}

static int qpnp_pin_set_mode(struct qpnp_pin_chip *q_chip,
				   struct qpnp_pin_spec *q_spec, int mode)
{
	int rc;
	u8 shift, mask;

	if (!q_chip || !q_spec)
		return -EINVAL;

	if (qpnp_pin_check_config(Q_PIN_CFG_MODE, q_spec, mode)) {
		pr_err("invalid mode specification %d\n", mode);
		return -EINVAL;
	}

	if (is_gpio_lv_mv(q_spec)) {
		shift = Q_REG_LV_MV_MODE_SEL_SHIFT;
		mask = Q_REG_LV_MV_MODE_SEL_MASK;
	} else {
		shift = Q_REG_MODE_SEL_SHIFT;
		mask = Q_REG_MODE_SEL_MASK;
	}

	q_reg_clr_set(&q_spec->regs[Q_REG_I_MODE_CTL], shift, mask, mode);

	rc = regmap_write(q_chip->regmap, Q_REG_ADDR(q_spec, Q_REG_MODE_CTL),
			  *&q_spec->regs[Q_REG_I_MODE_CTL]);
	return rc;
}

static int qpnp_pin_direction_input(struct gpio_chip *gpio_chip,
		unsigned offset)
{
	struct qpnp_pin_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_pin_spec *q_spec;

	if (WARN_ON(!q_chip))
		return -ENODEV;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (WARN_ON(!q_spec))
		return -ENODEV;

	return qpnp_pin_set_mode(q_chip, q_spec, QPNP_PIN_MODE_DIG_IN);
}

static int qpnp_pin_direction_output(struct gpio_chip *gpio_chip,
		unsigned offset,
		int val)
{
	int rc;
	struct qpnp_pin_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_pin_spec *q_spec;

	if (WARN_ON(!q_chip))
		return -ENODEV;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (WARN_ON(!q_spec))
		return -ENODEV;

	rc = __qpnp_pin_set(q_chip, q_spec, val);
	if (rc)
		return rc;

	rc = qpnp_pin_set_mode(q_chip, q_spec, QPNP_PIN_MODE_DIG_OUT);

	return rc;
}

static int qpnp_pin_of_gpio_xlate(struct gpio_chip *gpio_chip,
				   const struct of_phandle_args *gpio_spec,
				   u32 *flags)
{
	struct qpnp_pin_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_pin_spec *q_spec;

	if (WARN_ON(gpio_chip->of_gpio_n_cells < 2)) {
		pr_err("of_gpio_n_cells < 2\n");
		return -EINVAL;
	}

	q_spec = qpnp_pmic_pin_get_spec(q_chip, gpio_spec->args[0]);
	if (!q_spec) {
		pr_err("no such PMIC gpio %u in device topology\n",
							gpio_spec->args[0]);
		return -EINVAL;
	}

	if (flags)
		*flags = gpio_spec->args[1];

	return q_spec->gpio_chip_idx;
}

static int qpnp_pin_apply_config(struct qpnp_pin_chip *q_chip,
				  struct qpnp_pin_spec *q_spec)
{
	struct qpnp_pin_cfg param;
	struct device_node *node = q_spec->node;
	int rc;
	u8 shift, mask, *reg;

	if (is_gpio_lv_mv(q_spec)) {
		shift = Q_REG_LV_MV_MODE_SEL_SHIFT;
		mask = Q_REG_LV_MV_MODE_SEL_MASK;
	} else {
		shift = Q_REG_MODE_SEL_SHIFT;
		mask = Q_REG_MODE_SEL_MASK;
	}
	param.mode	   = q_reg_get(&q_spec->regs[Q_REG_I_MODE_CTL],
							shift, mask);

	param.output_type  = q_reg_get(&q_spec->regs[Q_REG_I_DIG_OUT_CTL],
				       Q_REG_OUT_TYPE_SHIFT,
				       Q_REG_OUT_TYPE_MASK);

	if (is_gpio_lv_mv(q_spec)) {
		shift = Q_REG_DIG_OUT_SRC_INVERT_SHIFT;
		mask = Q_REG_DIG_OUT_SRC_INVERT_MASK;
		reg = &q_spec->regs[Q_REG_I_DIG_OUT_SRC_CTL];
	} else {
		shift = Q_REG_OUT_INVERT_SHIFT;
		mask = Q_REG_OUT_INVERT_MASK;
		reg = &q_spec->regs[Q_REG_I_MODE_CTL];
	}
	param.invert	   = q_reg_get(reg, shift, mask);

	param.pull	   = q_reg_get(&q_spec->regs[Q_REG_I_DIG_PULL_CTL],
				       Q_REG_PULL_SHIFT, Q_REG_PULL_MASK);
	param.vin_sel	   = q_reg_get(&q_spec->regs[Q_REG_I_DIG_VIN_CTL],
				       Q_REG_VIN_SHIFT, Q_REG_VIN_MASK);
	param.out_strength = q_reg_get(&q_spec->regs[Q_REG_I_DIG_OUT_CTL],
				       Q_REG_OUT_STRENGTH_SHIFT,
				       Q_REG_OUT_STRENGTH_MASK);

	if (is_gpio_lv_mv(q_spec)) {
		shift = Q_REG_DIG_OUT_SRC_SRC_SEL_SHIFT;
		mask = Q_REG_DIG_OUT_SRC_SRC_SEL_MASK;
		reg = &q_spec->regs[Q_REG_I_DIG_OUT_SRC_CTL];
	} else {
		shift = Q_REG_SRC_SEL_SHIFT;
		mask = Q_REG_SRC_SEL_MASK;
		reg = &q_spec->regs[Q_REG_I_MODE_CTL];
	}
	param.src_sel   = q_reg_get(reg, shift, mask);

	param.master_en    = q_reg_get(&q_spec->regs[Q_REG_I_EN_CTL],
				       Q_REG_MASTER_EN_SHIFT,
				       Q_REG_MASTER_EN_MASK);
	param.aout_ref    = q_reg_get(&q_spec->regs[Q_REG_I_AOUT_CTL],
				       Q_REG_AOUT_REF_SHIFT,
				       Q_REG_AOUT_REF_MASK);
	param.ain_route    = q_reg_get(&q_spec->regs[Q_REG_I_AIN_CTL],
				       Q_REG_AIN_ROUTE_SHIFT,
				       Q_REG_AIN_ROUTE_MASK);
	param.cs_out    = q_reg_get(&q_spec->regs[Q_REG_I_SINK_CTL],
				       Q_REG_CS_OUT_SHIFT,
				       Q_REG_CS_OUT_MASK);
	param.apass_sel    = q_reg_get(&q_spec->regs[Q_REG_I_APASS_SEL_CTL],
				       Q_REG_APASS_SEL_SHIFT,
				       Q_REG_APASS_SEL_MASK);
	if (is_gpio_lv_mv(q_spec)) {
		param.dtest_sel = q_reg_get(&q_spec->regs[Q_REG_I_DIG_IN_CTL],
				Q_REG_LV_MV_DTEST_SEL_CFG_SHIFT,
				Q_REG_LV_MV_DTEST_SEL_CFG_MASK);
	} else {
		 param.dtest_sel = q_reg_get(&q_spec->regs[Q_REG_I_DIG_IN_CTL],
				Q_REG_DTEST_SEL_SHIFT,
				Q_REG_DTEST_SEL_MASK);
	}

	of_property_read_u32(node, "qcom,mode",
		&param.mode);
	of_property_read_u32(node, "qcom,output-type",
		&param.output_type);
	of_property_read_u32(node, "qcom,invert",
		&param.invert);
	of_property_read_u32(node, "qcom,pull",
		&param.pull);
	of_property_read_u32(node, "qcom,vin-sel",
		&param.vin_sel);
	of_property_read_u32(node, "qcom,out-strength",
		&param.out_strength);
	of_property_read_u32(node, "qcom,src-sel",
		&param.src_sel);
	of_property_read_u32(node, "qcom,master-en",
		&param.master_en);
	of_property_read_u32(node, "qcom,aout-ref",
		&param.aout_ref);
	of_property_read_u32(node, "qcom,ain-route",
		&param.ain_route);
	of_property_read_u32(node, "qcom,cs-out",
		&param.cs_out);
	of_property_read_u32(node, "qcom,apass-sel",
		&param.apass_sel);
	of_property_read_u32(node, "qcom,dtest-sel",
		&param.dtest_sel);

	rc = _qpnp_pin_config(q_chip, q_spec, &param);

	return rc;
}

static int qpnp_pin_free_chip(struct qpnp_pin_chip *q_chip)
{
	int i, rc = 0;

	if (q_chip->chip_gpios)
		for (i = 0; i < q_chip->gpio_chip.ngpio; i++)
			kfree(q_chip->chip_gpios[i]);

	mutex_lock(&qpnp_pin_chips_lock);
	list_del(&q_chip->chip_list);
	mutex_unlock(&qpnp_pin_chips_lock);
	if (q_chip->chip_registered)
		gpiochip_remove(&q_chip->gpio_chip);

	kfree(q_chip->chip_gpios);
	kfree(q_chip->pmic_pins);
	kfree(q_chip);
	return rc;
}

#ifdef CONFIG_GPIO_QPNP_PIN_DEBUG
struct qpnp_pin_reg {
	uint32_t addr;
	uint32_t idx;
	uint32_t shift;
	uint32_t mask;
};

static struct dentry *driver_dfs_dir;

static int qpnp_pin_reg_attr(enum qpnp_pin_param_type type,
			     struct qpnp_pin_reg *cfg,
			struct qpnp_pin_spec *q_spec)
{
	switch (type) {
	case Q_PIN_CFG_MODE:
		if (is_gpio_lv_mv(q_spec)) {
			cfg->shift = Q_REG_LV_MV_MODE_SEL_SHIFT;
			cfg->mask = Q_REG_LV_MV_MODE_SEL_MASK;
		} else {
			cfg->shift = Q_REG_MODE_SEL_SHIFT;
			cfg->mask = Q_REG_MODE_SEL_MASK;
		}
		cfg->addr = Q_REG_MODE_CTL;
		cfg->idx = Q_REG_I_MODE_CTL;
		break;
	case Q_PIN_CFG_OUTPUT_TYPE:
		cfg->addr = Q_REG_DIG_OUT_CTL;
		cfg->idx = Q_REG_I_DIG_OUT_CTL;
		cfg->shift = Q_REG_OUT_TYPE_SHIFT;
		cfg->mask = Q_REG_OUT_TYPE_MASK;
		break;
	case Q_PIN_CFG_INVERT:
		if (is_gpio_lv_mv(q_spec)) {
			cfg->addr = Q_REG_DIG_OUT_SRC_CTL;
			cfg->idx = Q_REG_I_DIG_OUT_SRC_CTL;
			cfg->shift = Q_REG_DIG_OUT_SRC_INVERT_SHIFT;
			cfg->mask = Q_REG_DIG_OUT_SRC_INVERT_MASK;
		} else {
			cfg->addr = Q_REG_MODE_CTL;
			cfg->idx = Q_REG_I_MODE_CTL;
			cfg->shift = Q_REG_OUT_INVERT_SHIFT;
			cfg->mask = Q_REG_OUT_INVERT_MASK;
		}
		break;
	case Q_PIN_CFG_PULL:
		cfg->addr = Q_REG_DIG_PULL_CTL;
		cfg->idx = Q_REG_I_DIG_PULL_CTL;
		cfg->shift = Q_REG_PULL_SHIFT;
		cfg->mask = Q_REG_PULL_MASK;
		break;
	case Q_PIN_CFG_VIN_SEL:
		cfg->addr = Q_REG_DIG_VIN_CTL;
		cfg->idx = Q_REG_I_DIG_VIN_CTL;
		cfg->shift = Q_REG_VIN_SHIFT;
		cfg->mask = Q_REG_VIN_MASK;
		break;
	case Q_PIN_CFG_OUT_STRENGTH:
		cfg->addr = Q_REG_DIG_OUT_CTL;
		cfg->idx = Q_REG_I_DIG_OUT_CTL;
		cfg->shift = Q_REG_OUT_STRENGTH_SHIFT;
		cfg->mask = Q_REG_OUT_STRENGTH_MASK;
		break;
	case Q_PIN_CFG_SRC_SEL:
		if (is_gpio_lv_mv(q_spec)) {
			cfg->addr = Q_REG_DIG_OUT_SRC_CTL;
			cfg->idx = Q_REG_I_DIG_OUT_SRC_CTL;
			cfg->shift = Q_REG_DIG_OUT_SRC_SRC_SEL_SHIFT;
			cfg->mask = Q_REG_DIG_OUT_SRC_SRC_SEL_MASK;
		} else {
			cfg->addr = Q_REG_MODE_CTL;
			cfg->idx = Q_REG_I_MODE_CTL;
			cfg->shift = Q_REG_SRC_SEL_SHIFT;
			cfg->mask = Q_REG_SRC_SEL_MASK;
		}
		break;
	case Q_PIN_CFG_MASTER_EN:
		cfg->addr = Q_REG_EN_CTL;
		cfg->idx = Q_REG_I_EN_CTL;
		cfg->shift = Q_REG_MASTER_EN_SHIFT;
		cfg->mask = Q_REG_MASTER_EN_MASK;
		break;
	case Q_PIN_CFG_AOUT_REF:
		cfg->addr = Q_REG_AOUT_CTL;
		cfg->idx = Q_REG_I_AOUT_CTL;
		cfg->shift = Q_REG_AOUT_REF_SHIFT;
		cfg->mask = Q_REG_AOUT_REF_MASK;
		break;
	case Q_PIN_CFG_AIN_ROUTE:
		cfg->addr = Q_REG_AIN_CTL;
		cfg->idx = Q_REG_I_AIN_CTL;
		cfg->shift = Q_REG_AIN_ROUTE_SHIFT;
		cfg->mask = Q_REG_AIN_ROUTE_MASK;
		break;
	case Q_PIN_CFG_CS_OUT:
		cfg->addr = Q_REG_SINK_CTL;
		cfg->idx = Q_REG_I_SINK_CTL;
		cfg->shift = Q_REG_CS_OUT_SHIFT;
		cfg->mask = Q_REG_CS_OUT_MASK;
		break;
	case Q_PIN_CFG_APASS_SEL:
		cfg->addr = Q_REG_APASS_SEL_CTL;
		cfg->idx = Q_REG_I_APASS_SEL_CTL;
		cfg->shift = Q_REG_APASS_SEL_SHIFT;
		cfg->mask = Q_REG_APASS_SEL_MASK;
		break;
	case Q_PIN_CFG_DTEST_SEL:
		if (is_gpio_lv_mv(q_spec)) {
			cfg->shift = Q_REG_LV_MV_DTEST_SEL_CFG_SHIFT;
			cfg->mask = Q_REG_LV_MV_DTEST_SEL_CFG_MASK;
		} else {
			cfg->shift = Q_REG_DTEST_SEL_SHIFT;
			cfg->mask = Q_REG_DTEST_SEL_MASK;
		}
		cfg->addr = Q_REG_DIG_IN_CTL;
		cfg->idx = Q_REG_I_DIG_IN_CTL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qpnp_pin_debugfs_get(void *data, u64 *val)
{
	enum qpnp_pin_param_type *idx = data;
	struct qpnp_pin_spec *q_spec;
	struct qpnp_pin_reg cfg = {};
	int rc;

	q_spec = container_of(idx, struct qpnp_pin_spec, params[*idx]);

	rc = qpnp_pin_reg_attr(*idx, &cfg, q_spec);
	if (rc)
		return rc;

	*val = q_reg_get(&q_spec->regs[cfg.idx], cfg.shift, cfg.mask);
	return 0;
}

static int qpnp_pin_debugfs_set(void *data, u64 val)
{
	enum qpnp_pin_param_type *idx = data;
	struct qpnp_pin_spec *q_spec;
	struct qpnp_pin_chip *q_chip;
	struct qpnp_pin_reg cfg = {};
	int rc;

	q_spec = container_of(idx, struct qpnp_pin_spec, params[*idx]);
	q_chip = q_spec->q_chip;

	/*
	 * special handling for GPIO_LV/MV 'dtest-sel'
	 * if (dtest_sel == 0) then disable dtest-sel
	 * else enable and set dtest.
	 */
	if ((q_spec->subtype == Q_GPIO_SUBTYPE_GPIO_LV ||
		q_spec->subtype == Q_GPIO_SUBTYPE_GPIO_MV) &&
				*idx == Q_PIN_CFG_DTEST_SEL) {
		/* enable/disable DTEST */
		cfg.shift = Q_REG_LV_MV_DTEST_SEL_EN_SHIFT;
		cfg.mask = Q_REG_LV_MV_DTEST_SEL_EN_MASK;
		cfg.addr = Q_REG_DIG_IN_CTL;
		cfg.idx = Q_REG_I_DIG_IN_CTL;
		q_reg_clr_set(&q_spec->regs[cfg.idx],
				cfg.shift, cfg.mask, !!val);
	}

	rc = qpnp_pin_check_config(*idx, q_spec, val);
	if (rc)
		return rc;

	rc = qpnp_pin_reg_attr(*idx, &cfg, q_spec);
	if (rc)
		return rc;

	if (*idx == Q_PIN_CFG_DTEST_SEL && val)  {
		if (is_gpio_lv_mv(q_spec))
			val -= 1;
		else
			val = BIT(val - 1);
	}

	q_reg_clr_set(&q_spec->regs[cfg.idx], cfg.shift, cfg.mask, val);
	rc = regmap_write(q_chip->regmap, Q_REG_ADDR(q_spec, cfg.addr),
			  *&q_spec->regs[cfg.idx]);

	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(qpnp_pin_fops, qpnp_pin_debugfs_get,
			qpnp_pin_debugfs_set, "%llu\n");

#define DEBUGFS_BUF_SIZE 11 /* supports 2^32 in decimal */

struct qpnp_pin_debugfs_args {
	enum qpnp_pin_param_type type;
	const char *filename;
};

static struct qpnp_pin_debugfs_args dfs_args[] = {
	{ Q_PIN_CFG_MODE, "mode" },
	{ Q_PIN_CFG_OUTPUT_TYPE, "output_type" },
	{ Q_PIN_CFG_INVERT, "invert" },
	{ Q_PIN_CFG_PULL, "pull" },
	{ Q_PIN_CFG_VIN_SEL, "vin_sel" },
	{ Q_PIN_CFG_OUT_STRENGTH, "out_strength" },
	{ Q_PIN_CFG_SRC_SEL, "src_sel" },
	{ Q_PIN_CFG_MASTER_EN, "master_en" },
	{ Q_PIN_CFG_AOUT_REF, "aout_ref" },
	{ Q_PIN_CFG_AIN_ROUTE, "ain_route" },
	{ Q_PIN_CFG_CS_OUT, "cs_out" },
	{ Q_PIN_CFG_APASS_SEL, "apass_sel" },
	{ Q_PIN_CFG_DTEST_SEL, "dtest-sel" },
};

static int qpnp_pin_debugfs_create(struct qpnp_pin_chip *q_chip)
{
	struct platform_device *pdev = q_chip->pdev;
	struct device *dev = &pdev->dev;
	struct qpnp_pin_spec *q_spec;
	enum qpnp_pin_param_type *params;
	enum qpnp_pin_param_type type;
	char pmic_pin[DEBUGFS_BUF_SIZE];
	const char *filename;
	struct dentry *dfs, *dfs_io_dir;
	int i, j, rc;

	BUG_ON(Q_NUM_PARAMS != ARRAY_SIZE(dfs_args));

	q_chip->dfs_dir = debugfs_create_dir(q_chip->gpio_chip.label,
							driver_dfs_dir);
	if (q_chip->dfs_dir == NULL) {
		dev_err(dev, "%s: cannot register chip debugfs directory %s\n",
						__func__, dev->of_node->name);
		return -ENODEV;
	}

	for (i = 0; i < q_chip->gpio_chip.ngpio; i++) {
		q_spec = qpnp_chip_gpio_get_spec(q_chip, i);
		params = q_spec->params;
		snprintf(pmic_pin, DEBUGFS_BUF_SIZE, "%u", q_spec->pmic_pin);
		dfs_io_dir = debugfs_create_dir(pmic_pin, q_chip->dfs_dir);
		if (dfs_io_dir == NULL)
			goto dfs_err;

		for (j = 0; j < Q_NUM_PARAMS; j++) {
			type = dfs_args[j].type;
			filename = dfs_args[j].filename;

			/*
			 * Use a value of '0' to see if the pin has even basic
			 * support for a function. Do not create a file if
			 * it doesn't.
			 */
			rc = qpnp_pin_check_config(type, q_spec, 0);
			if (rc == -ENXIO)
				continue;

			params[type] = type;
			dfs = debugfs_create_file(
					filename,
					S_IRUGO | S_IWUSR,
					dfs_io_dir,
					&q_spec->params[type],
					&qpnp_pin_fops);
			if (dfs == NULL)
				goto dfs_err;
		}
	}
	return 0;
dfs_err:
	dev_err(dev, "%s: cannot register debugfs for pmic gpio %u on chip %s\n",
			__func__, q_spec->pmic_pin, dev->of_node->name);
	debugfs_remove_recursive(q_chip->dfs_dir);
	return -ENFILE;
}
#else
static int qpnp_pin_debugfs_create(struct qpnp_pin_chip *q_chip)
{
	return 0;
}
#endif

static int qpnp_pin_is_valid_pin(struct qpnp_pin_spec *q_spec)
{
	if (q_spec->type == Q_GPIO_TYPE)
		switch (q_spec->subtype) {
		case Q_GPIO_SUBTYPE_GPIO_4CH:
		case Q_GPIO_SUBTYPE_GPIOC_4CH:
		case Q_GPIO_SUBTYPE_GPIO_8CH:
		case Q_GPIO_SUBTYPE_GPIOC_8CH:
		case Q_GPIO_SUBTYPE_GPIO_LV:
		case Q_GPIO_SUBTYPE_GPIO_MV:
			return 1;
		}
	else if (q_spec->type == Q_MPP_TYPE)
		switch (q_spec->subtype) {
		case Q_MPP_SUBTYPE_4CH_NO_ANA_OUT:
		case Q_MPP_SUBTYPE_ULT_4CH_NO_ANA_OUT:
		case Q_MPP_SUBTYPE_4CH_NO_SINK:
		case Q_MPP_SUBTYPE_ULT_4CH_NO_SINK:
		case Q_MPP_SUBTYPE_4CH_FULL_FUNC:
		case Q_MPP_SUBTYPE_8CH_FULL_FUNC:
			return 1;
		}

	return 0;
}

static int qpnp_pin_probe(struct platform_device *pdev)
{
	struct qpnp_pin_chip *q_chip;
	struct qpnp_pin_spec *q_spec;
	unsigned int base;
	struct device_node *child;
	int i, rc;
	u32 lowest_gpio = UINT_MAX, highest_gpio = 0;
	u32 gpio;
	char version[Q_REG_SUBTYPE - Q_REG_DIG_MAJOR_REV + 1];
	const char *pin_dev_name;

	pin_dev_name = dev_name(&pdev->dev);
	if (!pin_dev_name) {
		dev_err(&pdev->dev,
			"%s: label binding undefined for node %s\n",
					__func__,
			pdev->dev.of_node->full_name);
		return -EINVAL;
	}

	q_chip = kzalloc(sizeof(*q_chip), GFP_KERNEL);
	if (!q_chip)
		return -ENOMEM;

	q_chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!q_chip->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}
	q_chip->pdev = pdev;
	dev_set_drvdata(&pdev->dev, q_chip);

	mutex_lock(&qpnp_pin_chips_lock);
	list_add(&q_chip->chip_list, &qpnp_pin_chips);
	mutex_unlock(&qpnp_pin_chips_lock);

	/* first scan through nodes to find the range required for allocation */
	i = 0;
	for_each_available_child_of_node(pdev->dev.of_node, child) {
		rc = of_property_read_u32(child, "qcom,pin-num", &gpio);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: unable to get qcom,pin-num property\n",
								__func__);
			goto err_probe;
		}

		if (gpio < lowest_gpio)
			lowest_gpio = gpio;
		if (gpio > highest_gpio)
			highest_gpio = gpio;
		i++;
	}
	q_chip->gpio_chip.ngpio = i;

	if (highest_gpio < lowest_gpio) {
		dev_err(&pdev->dev,
			"%s: no device nodes specified in topology\n",
								__func__);
		rc = -EINVAL;
		goto err_probe;
	} else if (lowest_gpio == 0) {
		dev_err(&pdev->dev, "%s: 0 is not a valid PMIC GPIO\n",
								__func__);
		rc = -EINVAL;
		goto err_probe;
	}

	q_chip->pmic_pin_lowest = lowest_gpio;
	q_chip->pmic_pin_highest = highest_gpio;

	/* allocate gpio lookup tables */
	q_chip->pmic_pins = kzalloc(sizeof(struct qpnp_pin_spec *) *
					(highest_gpio - lowest_gpio + 1),
					GFP_KERNEL);
	q_chip->chip_gpios = kzalloc(sizeof(struct qpnp_pin_spec *) *
					q_chip->gpio_chip.ngpio,
					GFP_KERNEL);
	if (!q_chip->pmic_pins || !q_chip->chip_gpios) {
		dev_err(&pdev->dev, "%s: unable to allocate memory\n",
								__func__);
		rc = -ENOMEM;
		goto err_probe;
	}

	/* get interrupt controller device_node */
	q_chip->int_ctrl = of_irq_find_parent(pdev->dev.of_node);
	if (!q_chip->int_ctrl) {
		dev_err(&pdev->dev, "%s: Can't find interrupt parent\n",
								__func__);
		rc = -EINVAL;
		goto err_probe;
	}
	i = 0;
	/* now scan through again and populate the lookup table */
	for_each_available_child_of_node(pdev->dev.of_node, child) {
		rc = of_property_read_u32(child, "reg", &base);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Couldn't find reg in node = %s rc = %d\n",
				child->full_name, rc);
			goto err_probe;
		}

		rc = of_property_read_u32(child, "qcom,pin-num", &gpio);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: unable to get qcom,pin-num property\n",
								__func__);
			goto err_probe;
		}

		q_spec = kzalloc(sizeof(struct qpnp_pin_spec), GFP_KERNEL);
		if (!q_spec) {
			rc = -ENOMEM;
			goto err_probe;
		}

		q_spec->slave = to_spmi_device(pdev->dev.parent)->usid;
		q_spec->offset = base;
		q_spec->gpio_chip_idx = i;
		q_spec->pmic_pin = gpio;
		q_spec->node = child;
		q_spec->q_chip = q_chip;

		rc = regmap_bulk_read(q_chip->regmap,
				Q_REG_ADDR(q_spec, Q_REG_DIG_MAJOR_REV),
				&version[0],
				ARRAY_SIZE(version));
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to read type regs\n",
						__func__);
			goto err_probe;
		}
		q_spec->dig_major_rev = version[Q_REG_DIG_MAJOR_REV -
						Q_REG_DIG_MAJOR_REV];
		q_spec->type	= version[Q_REG_TYPE - Q_REG_DIG_MAJOR_REV];
		q_spec->subtype = version[Q_REG_SUBTYPE - Q_REG_DIG_MAJOR_REV];

		if (!qpnp_pin_is_valid_pin(q_spec)) {
			dev_err(&pdev->dev,
				"%s: invalid pin type (type=0x%x subtype=0x%x)\n",
				       __func__, q_spec->type, q_spec->subtype);
			goto err_probe;
		}

		rc = qpnp_pin_ctl_regs_init(q_spec);
		if (rc)
			goto err_probe;

		/* initialize lookup table params */
		qpnp_pmic_pin_set_spec(q_chip, gpio, q_spec);
		qpnp_chip_gpio_set_spec(q_chip, i, q_spec);
		i++;
	}

	q_chip->gpio_chip.base = -1;
	q_chip->gpio_chip.label = pin_dev_name;
	q_chip->gpio_chip.direction_input = qpnp_pin_direction_input;
	q_chip->gpio_chip.direction_output = qpnp_pin_direction_output;
	q_chip->gpio_chip.to_irq = qpnp_pin_to_irq;
	q_chip->gpio_chip.get = qpnp_pin_get;
	q_chip->gpio_chip.set = qpnp_pin_set;
	q_chip->gpio_chip.dev = &pdev->dev;
	q_chip->gpio_chip.of_xlate = qpnp_pin_of_gpio_xlate;
	q_chip->gpio_chip.of_gpio_n_cells = 2;
	q_chip->gpio_chip.can_sleep = 0;

	rc = gpiochip_add(&q_chip->gpio_chip);
	if (rc) {
		dev_err(&pdev->dev, "%s: Can't add gpio chip, rc = %d\n",
								__func__, rc);
		goto err_probe;
	}

	q_chip->chip_registered = true;
	/* now configure gpio config defaults if they exist */
	for (i = 0; i < q_chip->gpio_chip.ngpio; i++) {
		q_spec = qpnp_chip_gpio_get_spec(q_chip, i);
		if (WARN_ON(!q_spec)) {
			rc = -ENODEV;
			goto err_probe;
		}

		rc = qpnp_pin_cache_regs(q_chip, q_spec);
		if (rc)
			goto err_probe;

		rc = qpnp_pin_apply_config(q_chip, q_spec);
		if (rc)
			goto err_probe;
	}

	dev_dbg(&pdev->dev, "%s: gpio_chip registered between %d-%u\n",
			__func__, q_chip->gpio_chip.base,
			(q_chip->gpio_chip.base + q_chip->gpio_chip.ngpio) - 1);

	rc = qpnp_pin_debugfs_create(q_chip);
	if (rc) {
		dev_err(&pdev->dev, "%s: debugfs creation failed\n",
			__func__);
		goto err_probe;
	}

	return 0;

err_probe:
	qpnp_pin_free_chip(q_chip);
	return rc;
}

static int qpnp_pin_remove(struct platform_device *pdev)
{
	struct qpnp_pin_chip *q_chip = dev_get_drvdata(&pdev->dev);

	debugfs_remove_recursive(q_chip->dfs_dir);

	return qpnp_pin_free_chip(q_chip);
}

static struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,qpnp-pin",
	},
	{}
};

static const struct platform_device_id qpnp_pin_id[] = {
	{ "qcom,qpnp-pin", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spmi, qpnp_pin_id);

static struct platform_driver qpnp_pin_driver = {
	.driver		= {
		.name		= "qcom,qpnp-pin",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_pin_probe,
	.remove		= qpnp_pin_remove,
	.id_table	= qpnp_pin_id,
};

static int __init qpnp_pin_init(void)
{
#ifdef CONFIG_GPIO_QPNP_PIN_DEBUG
	driver_dfs_dir = debugfs_create_dir("qpnp_pin", NULL);
	if (driver_dfs_dir == NULL)
		pr_err("Cannot register top level debugfs directory\n");
#endif

	return platform_driver_register(&qpnp_pin_driver);
}

static void __exit qpnp_pin_exit(void)
{
#ifdef CONFIG_GPIO_QPNP_PIN_DEBUG
	debugfs_remove_recursive(driver_dfs_dir);
#endif
	platform_driver_unregister(&qpnp_pin_driver);
}

MODULE_DESCRIPTION("QPNP PMIC gpio driver");
MODULE_LICENSE("GPL v2");

subsys_initcall(qpnp_pin_init);
module_exit(qpnp_pin_exit);
