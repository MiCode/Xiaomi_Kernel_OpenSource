/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/types.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/qpnp/gpio.h>

#include <mach/qpnp.h>

#define Q_REG_ADDR(q_spec, reg_index)	\
		((q_spec)->offset + reg_index)

#define Q_REG_STATUS1			0x8
#define Q_NUM_CTL_REGS			7

/* type registers base address offsets */
#define Q_REG_TYPE			0x10
#define Q_REG_SUBTYPE			0x11

/* gpio peripheral type and subtype values */
#define Q_GPIO_TYPE			0x10
#define Q_GPIO_SUBTYPE_GPIO_4CH		0x1
#define Q_GPIO_SUBTYPE_GPIOC_4CH	0x5
#define Q_GPIO_SUBTYPE_GPIO_8CH		0x9
#define Q_GPIO_SUBTYPE_GPIOC_8CH	0xD

/* control register base address offsets */
#define Q_REG_MODE_CTL			0x40
#define Q_REG_DIG_PULL_CTL		0x42
#define Q_REG_DIG_IN_CTL		0x43
#define Q_REG_DIG_VIN_CTL		0x44
#define Q_REG_DIG_OUT_CTL		0x45
#define Q_REG_EN_CTL			0x46

/* control register regs array indices */
#define Q_REG_I_MODE_CTL		0
#define Q_REG_I_DIG_PULL_CTL		2
#define Q_REG_I_DIG_IN_CTL		3
#define Q_REG_I_DIG_VIN_CTL		4
#define Q_REG_I_DIG_OUT_CTL		5
#define Q_REG_I_EN_CTL			6

/* control reg: mode */
#define Q_REG_OUT_INVERT_SHIFT		0
#define Q_REG_OUT_INVERT_MASK		0x1
#define Q_REG_SRC_SEL_SHIFT		1
#define Q_REG_SRC_SEL_MASK		0xE
#define Q_REG_MODE_SEL_SHIFT		4
#define Q_REG_MODE_SEL_MASK		0x70

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

/* control reg: en */
#define Q_REG_MASTER_EN_SHIFT		7
#define Q_REG_MASTER_EN_MASK		0x80

enum qpnp_gpio_param_type {
	Q_GPIO_CFG_DIRECTION,
	Q_GPIO_CFG_OUTPUT_TYPE,
	Q_GPIO_CFG_INVERT,
	Q_GPIO_CFG_PULL,
	Q_GPIO_CFG_VIN_SEL,
	Q_GPIO_CFG_OUT_STRENGTH,
	Q_GPIO_CFG_SRC_SELECT,
	Q_GPIO_CFG_MASTER_EN,
	Q_GPIO_CFG_INVALID,
};

#define Q_NUM_PARAMS			Q_GPIO_CFG_INVALID

/* param error checking */
#define QPNP_GPIO_DIR_INVALID		3
#define QPNP_GPIO_INVERT_INVALID	2
#define QPNP_GPIO_OUT_BUF_INVALID	3
#define QPNP_GPIO_VIN_INVALID		8
#define QPNP_GPIO_PULL_INVALID		6
#define QPNP_GPIO_OUT_STRENGTH_INVALID	4
#define QPNP_GPIO_SRC_INVALID		8
#define QPNP_GPIO_MASTER_INVALID	2

struct qpnp_gpio_spec {
	uint8_t slave;			/* 0-15 */
	uint16_t offset;		/* 0-255 */
	uint32_t gpio_chip_idx;		/* offset from gpio_chip base */
	uint32_t pmic_gpio;		/* PMIC gpio number */
	int irq;			/* logical IRQ number */
	u8 regs[Q_NUM_CTL_REGS];	/* Control regs */
	u8 type;			/* peripheral type */
	u8 subtype;			/* peripheral subtype */
	struct device_node *node;
	enum qpnp_gpio_param_type params[Q_NUM_PARAMS];
	struct qpnp_gpio_chip *q_chip;
};

struct qpnp_gpio_chip {
	struct gpio_chip	gpio_chip;
	struct spmi_device	*spmi;
	struct qpnp_gpio_spec	**pmic_gpios;
	struct qpnp_gpio_spec	**chip_gpios;
	uint32_t		pmic_gpio_lowest;
	uint32_t		pmic_gpio_highest;
	struct device_node	*int_ctrl;
	struct list_head	chip_list;
	struct dentry		*dfs_dir;
};

static LIST_HEAD(qpnp_gpio_chips);
static DEFINE_MUTEX(qpnp_gpio_chips_lock);

static inline void qpnp_pmic_gpio_set_spec(struct qpnp_gpio_chip *q_chip,
					      uint32_t pmic_gpio,
					      struct qpnp_gpio_spec *spec)
{
	q_chip->pmic_gpios[pmic_gpio - q_chip->pmic_gpio_lowest] = spec;
}

static inline struct qpnp_gpio_spec *qpnp_pmic_gpio_get_spec(
						struct qpnp_gpio_chip *q_chip,
						uint32_t pmic_gpio)
{
	if (pmic_gpio < q_chip->pmic_gpio_lowest ||
	    pmic_gpio > q_chip->pmic_gpio_highest)
		return NULL;

	return q_chip->pmic_gpios[pmic_gpio - q_chip->pmic_gpio_lowest];
}

static inline struct qpnp_gpio_spec *qpnp_chip_gpio_get_spec(
						struct qpnp_gpio_chip *q_chip,
						uint32_t chip_gpio)
{
	if (chip_gpio > q_chip->gpio_chip.ngpio)
		return NULL;

	return q_chip->chip_gpios[chip_gpio];
}

static inline void qpnp_chip_gpio_set_spec(struct qpnp_gpio_chip *q_chip,
					      uint32_t chip_gpio,
					      struct qpnp_gpio_spec *spec)
{
	q_chip->chip_gpios[chip_gpio] = spec;
}

static int qpnp_gpio_check_config(struct qpnp_gpio_spec *q_spec,
				  struct qpnp_gpio_cfg *param)
{
	int gpio = q_spec->pmic_gpio;

	if (param->direction >= QPNP_GPIO_DIR_INVALID)
		pr_err("invalid direction for gpio %d\n", gpio);
	else if (param->invert >= QPNP_GPIO_INVERT_INVALID)
		pr_err("invalid invert polarity for gpio %d\n", gpio);
	else if (param->src_select >= QPNP_GPIO_SRC_INVALID)
		pr_err("invalid source select for gpio %d\n", gpio);
	else if (param->out_strength >= QPNP_GPIO_OUT_STRENGTH_INVALID ||
		 param->out_strength == 0)
		pr_err("invalid out strength for gpio %d\n", gpio);
	else if (param->output_type >= QPNP_GPIO_OUT_BUF_INVALID)
		pr_err("invalid out type for gpio %d\n", gpio);
	else if ((param->output_type == QPNP_GPIO_OUT_BUF_OPEN_DRAIN_NMOS ||
		 param->output_type == QPNP_GPIO_OUT_BUF_OPEN_DRAIN_PMOS) &&
		 (q_spec->subtype == Q_GPIO_SUBTYPE_GPIOC_4CH ||
		 (q_spec->subtype == Q_GPIO_SUBTYPE_GPIOC_8CH)))
		pr_err("invalid out type for gpio %d\n"
		       "gpioc does not support open-drain\n", gpio);
	else if (param->vin_sel >= QPNP_GPIO_VIN_INVALID)
		pr_err("invalid vin select value for gpio %d\n", gpio);
	else if (param->pull >= QPNP_GPIO_PULL_INVALID)
		pr_err("invalid pull value for gpio %d\n", gpio);
	else if (param->master_en >= QPNP_GPIO_MASTER_INVALID)
		pr_err("invalid master_en value for gpio %d\n", gpio);
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

static int qpnp_gpio_cache_regs(struct qpnp_gpio_chip *q_chip,
				struct qpnp_gpio_spec *q_spec)
{
	int rc;
	struct device *dev = &q_chip->spmi->dev;

	rc = spmi_ext_register_readl(q_chip->spmi->ctrl, q_spec->slave,
				     Q_REG_ADDR(q_spec, Q_REG_MODE_CTL),
				     &q_spec->regs[Q_REG_I_MODE_CTL],
				     Q_NUM_CTL_REGS);
	if (rc)
		dev_err(dev, "%s: unable to read control regs\n", __func__);

	return rc;
}

static int _qpnp_gpio_config(struct qpnp_gpio_chip *q_chip,
			     struct qpnp_gpio_spec *q_spec,
			     struct qpnp_gpio_cfg *param)
{
	struct device *dev = &q_chip->spmi->dev;
	int rc;

	rc = qpnp_gpio_check_config(q_spec, param);
	if (rc)
		goto gpio_cfg;

	/* set direction */
	q_reg_clr_set(&q_spec->regs[Q_REG_I_MODE_CTL],
			  Q_REG_MODE_SEL_SHIFT, Q_REG_MODE_SEL_MASK,
			  param->direction);

	/* output specific configuration */
	q_reg_clr_set(&q_spec->regs[Q_REG_I_MODE_CTL],
			  Q_REG_OUT_INVERT_SHIFT, Q_REG_OUT_INVERT_MASK,
			  param->invert);
	q_reg_clr_set(&q_spec->regs[Q_REG_I_MODE_CTL],
			  Q_REG_SRC_SEL_SHIFT, Q_REG_SRC_SEL_MASK,
			  param->src_select);
	q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_OUT_CTL],
			  Q_REG_OUT_STRENGTH_SHIFT, Q_REG_OUT_STRENGTH_MASK,
			  param->out_strength);
	q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_OUT_CTL],
			  Q_REG_OUT_TYPE_SHIFT, Q_REG_OUT_TYPE_MASK,
			  param->output_type);

	/* config applicable for both input / output */
	q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_VIN_CTL],
			  Q_REG_VIN_SHIFT, Q_REG_VIN_MASK,
			  param->vin_sel);
	q_reg_clr_set(&q_spec->regs[Q_REG_I_DIG_PULL_CTL],
			  Q_REG_PULL_SHIFT, Q_REG_PULL_MASK,
			  param->pull);
	q_reg_clr_set(&q_spec->regs[Q_REG_I_EN_CTL],
			  Q_REG_MASTER_EN_SHIFT, Q_REG_MASTER_EN_MASK,
			  param->master_en);

	rc = spmi_ext_register_writel(q_chip->spmi->ctrl, q_spec->slave,
			      Q_REG_ADDR(q_spec, Q_REG_MODE_CTL),
			      &q_spec->regs[Q_REG_I_MODE_CTL], Q_NUM_CTL_REGS);
	if (rc) {
		dev_err(&q_chip->spmi->dev, "%s: unable to write master"
						" enable\n", __func__);
		goto gpio_cfg;
	}

	return 0;

gpio_cfg:
	dev_err(dev, "%s: unable to set default config for"
		     " pmic gpio %d\n", __func__, q_spec->pmic_gpio);

	return rc;
}

int qpnp_gpio_config(int gpio, struct qpnp_gpio_cfg *param)
{
	int rc, chip_offset;
	struct qpnp_gpio_chip *q_chip;
	struct qpnp_gpio_spec *q_spec = NULL;
	struct gpio_chip *gpio_chip;

	if (param == NULL)
		return -EINVAL;

	mutex_lock(&qpnp_gpio_chips_lock);
	list_for_each_entry(q_chip, &qpnp_gpio_chips, chip_list) {
		gpio_chip = &q_chip->gpio_chip;
		if (gpio >= gpio_chip->base
				&& gpio < gpio_chip->base + gpio_chip->ngpio) {
			chip_offset = gpio - gpio_chip->base;
			q_spec = qpnp_chip_gpio_get_spec(q_chip, chip_offset);
			if (WARN_ON(!q_spec)) {
				mutex_unlock(&qpnp_gpio_chips_lock);
				return -ENODEV;
			}
			break;
		}
	}
	mutex_unlock(&qpnp_gpio_chips_lock);

	rc = _qpnp_gpio_config(q_chip, q_spec, param);

	return rc;
}
EXPORT_SYMBOL(qpnp_gpio_config);

int qpnp_gpio_map_gpio(uint16_t slave_id, uint32_t pmic_gpio)
{
	struct qpnp_gpio_chip *q_chip;
	struct qpnp_gpio_spec *q_spec = NULL;

	mutex_lock(&qpnp_gpio_chips_lock);
	list_for_each_entry(q_chip, &qpnp_gpio_chips, chip_list) {
		if (q_chip->spmi->sid != slave_id)
			continue;
		if (q_chip->pmic_gpio_lowest <= pmic_gpio &&
		    q_chip->pmic_gpio_highest >= pmic_gpio) {
			q_spec = qpnp_pmic_gpio_get_spec(q_chip, pmic_gpio);
			mutex_unlock(&qpnp_gpio_chips_lock);
			if (WARN_ON(!q_spec))
				return -ENODEV;
			return q_chip->gpio_chip.base + q_spec->gpio_chip_idx;
		}
	}
	mutex_unlock(&qpnp_gpio_chips_lock);
	return -EINVAL;
}
EXPORT_SYMBOL(qpnp_gpio_map_gpio);

static int qpnp_gpio_to_irq(struct gpio_chip *gpio_chip, unsigned offset)
{
	struct qpnp_gpio_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_gpio_spec *q_spec;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (!q_spec)
		return -EINVAL;

	return q_spec->irq;
}

static int qpnp_gpio_get(struct gpio_chip *gpio_chip, unsigned offset)
{
	int rc, ret_val;
	struct qpnp_gpio_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_gpio_spec *q_spec = NULL;
	u8 buf[1];

	if (WARN_ON(!q_chip))
		return -ENODEV;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (WARN_ON(!q_spec))
		return -ENODEV;

	/* gpio val is from RT status iff input is enabled */
	if ((q_spec->regs[Q_REG_I_MODE_CTL] & Q_REG_MODE_SEL_MASK)
						== QPNP_GPIO_DIR_IN) {
		/* INT_RT_STS */
		rc = spmi_ext_register_readl(q_chip->spmi->ctrl, q_spec->slave,
				Q_REG_ADDR(q_spec, Q_REG_STATUS1),
				&buf[0], 1);
		return buf[0];

	} else {
		ret_val = (q_spec->regs[Q_REG_I_MODE_CTL] &
			       Q_REG_OUT_INVERT_MASK) >> Q_REG_OUT_INVERT_SHIFT;
		return ret_val;
	}

	return 0;
}

static int __qpnp_gpio_set(struct qpnp_gpio_chip *q_chip,
			   struct qpnp_gpio_spec *q_spec, int value)
{
	int rc;

	if (!q_chip || !q_spec)
		return -EINVAL;

	if (value)
		q_reg_clr_set(&q_spec->regs[Q_REG_I_MODE_CTL],
			  Q_REG_OUT_INVERT_SHIFT, Q_REG_OUT_INVERT_MASK, 1);
	else
		q_reg_clr_set(&q_spec->regs[Q_REG_I_MODE_CTL],
			  Q_REG_OUT_INVERT_SHIFT, Q_REG_OUT_INVERT_MASK, 0);

	rc = spmi_ext_register_writel(q_chip->spmi->ctrl, q_spec->slave,
			      Q_REG_ADDR(q_spec, Q_REG_I_MODE_CTL),
			      &q_spec->regs[Q_REG_I_MODE_CTL], 1);
	if (rc)
		dev_err(&q_chip->spmi->dev, "%s: spmi write failed\n",
								__func__);
	return rc;
}


static void qpnp_gpio_set(struct gpio_chip *gpio_chip,
		unsigned offset, int value)
{
	struct qpnp_gpio_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_gpio_spec *q_spec;

	if (WARN_ON(!q_chip))
		return;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (WARN_ON(!q_spec))
		return;

	__qpnp_gpio_set(q_chip, q_spec, value);
}

static int qpnp_gpio_set_direction(struct qpnp_gpio_chip *q_chip,
				   struct qpnp_gpio_spec *q_spec, int direction)
{
	int rc;

	if (!q_chip || !q_spec)
		return -EINVAL;

	if (direction >= QPNP_GPIO_DIR_INVALID) {
		pr_err("invalid direction specification %d\n", direction);
		return -EINVAL;
	}

	q_reg_clr_set(&q_spec->regs[Q_REG_I_MODE_CTL],
			Q_REG_MODE_SEL_SHIFT,
			Q_REG_MODE_SEL_MASK,
			direction);

	rc = spmi_ext_register_writel(q_chip->spmi->ctrl, q_spec->slave,
			      Q_REG_ADDR(q_spec, Q_REG_I_MODE_CTL),
			      &q_spec->regs[Q_REG_I_MODE_CTL], 1);
	return rc;
}

static int qpnp_gpio_direction_input(struct gpio_chip *gpio_chip,
		unsigned offset)
{
	struct qpnp_gpio_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_gpio_spec *q_spec;

	if (WARN_ON(!q_chip))
		return -ENODEV;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (WARN_ON(!q_spec))
		return -ENODEV;

	return qpnp_gpio_set_direction(q_chip, q_spec, QPNP_GPIO_DIR_IN);
}

static int qpnp_gpio_direction_output(struct gpio_chip *gpio_chip,
		unsigned offset,
		int val)
{
	int rc;
	struct qpnp_gpio_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_gpio_spec *q_spec;

	if (WARN_ON(!q_chip))
		return -ENODEV;

	q_spec = qpnp_chip_gpio_get_spec(q_chip, offset);
	if (WARN_ON(!q_spec))
		return -ENODEV;

	rc = __qpnp_gpio_set(q_chip, q_spec, val);
	if (rc)
		return rc;

	rc = qpnp_gpio_set_direction(q_chip, q_spec, QPNP_GPIO_DIR_OUT);

	return rc;
}

static int qpnp_gpio_of_gpio_xlate(struct gpio_chip *gpio_chip,
				   struct device_node *np,
				   const void *gpio_spec, u32 *flags)
{
	struct qpnp_gpio_chip *q_chip = dev_get_drvdata(gpio_chip->dev);
	struct qpnp_gpio_spec *q_spec;
	const __be32 *gpio = gpio_spec;
	u32 n = be32_to_cpup(gpio);

	if (WARN_ON(gpio_chip->of_gpio_n_cells < 2)) {
		pr_err("of_gpio_n_cells < 2\n");
		return -EINVAL;
	}

	q_spec = qpnp_pmic_gpio_get_spec(q_chip, n);
	if (!q_spec) {
		pr_err("no such PMIC gpio %u in device topology\n", n);
		return -EINVAL;
	}

	if (flags)
		*flags = be32_to_cpu(gpio[1]);

	return q_spec->gpio_chip_idx;
}

static int qpnp_gpio_apply_config(struct qpnp_gpio_chip *q_chip,
				  struct qpnp_gpio_spec *q_spec)
{
	struct qpnp_gpio_cfg param;
	struct device_node *node = q_spec->node;
	int rc;

	param.direction    = q_reg_get(&q_spec->regs[Q_REG_I_MODE_CTL],
				       Q_REG_MODE_SEL_SHIFT,
				       Q_REG_MODE_SEL_MASK);
	param.output_type  = q_reg_get(&q_spec->regs[Q_REG_I_DIG_OUT_CTL],
				       Q_REG_OUT_TYPE_SHIFT,
				       Q_REG_OUT_TYPE_MASK);
	param.invert	   = q_reg_get(&q_spec->regs[Q_REG_I_MODE_CTL],
				       Q_REG_OUT_INVERT_MASK,
				       Q_REG_OUT_INVERT_MASK);
	param.pull	   = q_reg_get(&q_spec->regs[Q_REG_I_MODE_CTL],
				       Q_REG_PULL_SHIFT, Q_REG_PULL_MASK);
	param.vin_sel	   = q_reg_get(&q_spec->regs[Q_REG_I_DIG_VIN_CTL],
				       Q_REG_VIN_SHIFT, Q_REG_VIN_MASK);
	param.out_strength = q_reg_get(&q_spec->regs[Q_REG_I_DIG_OUT_CTL],
				       Q_REG_OUT_STRENGTH_SHIFT,
				       Q_REG_OUT_STRENGTH_MASK);
	param.src_select   = q_reg_get(&q_spec->regs[Q_REG_I_MODE_CTL],
				       Q_REG_SRC_SEL_SHIFT, Q_REG_SRC_SEL_MASK);
	param.master_en    = q_reg_get(&q_spec->regs[Q_REG_I_EN_CTL],
				       Q_REG_MASTER_EN_SHIFT,
				       Q_REG_MASTER_EN_MASK);

	of_property_read_u32(node, "qcom,direction",
		&param.direction);
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
	of_property_read_u32(node, "qcom,src-select",
		&param.src_select);
	rc = of_property_read_u32(node, "qcom,master-en",
		&param.master_en);

	rc = _qpnp_gpio_config(q_chip, q_spec, &param);

	return rc;
}

static int qpnp_gpio_free_chip(struct qpnp_gpio_chip *q_chip)
{
	struct spmi_device *spmi = q_chip->spmi;
	int rc, i;

	if (q_chip->chip_gpios)
		for (i = 0; i < spmi->num_dev_node; i++)
			kfree(q_chip->chip_gpios[i]);

	mutex_lock(&qpnp_gpio_chips_lock);
	list_del(&q_chip->chip_list);
	mutex_unlock(&qpnp_gpio_chips_lock);
	rc = gpiochip_remove(&q_chip->gpio_chip);
	if (rc)
		dev_err(&q_chip->spmi->dev, "%s: unable to remove gpio\n",
				__func__);
	kfree(q_chip->chip_gpios);
	kfree(q_chip->pmic_gpios);
	kfree(q_chip);
	return rc;
}

#ifdef CONFIG_GPIO_QPNP_DEBUG
struct qpnp_gpio_reg {
	uint32_t addr;
	uint32_t idx;
	uint32_t shift;
	uint32_t mask;
};

static struct dentry *driver_dfs_dir;

static int qpnp_gpio_reg_attr(enum qpnp_gpio_param_type type,
			     struct qpnp_gpio_reg *cfg)
{
	switch (type) {
	case Q_GPIO_CFG_DIRECTION:
		cfg->addr = Q_REG_MODE_CTL;
		cfg->idx = Q_REG_I_MODE_CTL;
		cfg->shift = Q_REG_MODE_SEL_SHIFT;
		cfg->mask = Q_REG_MODE_SEL_MASK;
		break;
	case Q_GPIO_CFG_OUTPUT_TYPE:
		cfg->addr = Q_REG_DIG_OUT_CTL;
		cfg->idx = Q_REG_I_DIG_OUT_CTL;
		cfg->shift = Q_REG_OUT_TYPE_SHIFT;
		cfg->mask = Q_REG_OUT_TYPE_MASK;
		break;
	case Q_GPIO_CFG_INVERT:
		cfg->addr = Q_REG_MODE_CTL;
		cfg->idx = Q_REG_I_MODE_CTL;
		cfg->shift = Q_REG_OUT_INVERT_SHIFT;
		cfg->mask = Q_REG_OUT_INVERT_MASK;
		break;
	case Q_GPIO_CFG_PULL:
		cfg->addr = Q_REG_DIG_PULL_CTL;
		cfg->idx = Q_REG_I_DIG_PULL_CTL;
		cfg->shift = Q_REG_PULL_SHIFT;
		cfg->mask = Q_REG_PULL_MASK;
		break;
	case Q_GPIO_CFG_VIN_SEL:
		cfg->addr = Q_REG_DIG_VIN_CTL;
		cfg->idx = Q_REG_I_DIG_VIN_CTL;
		cfg->shift = Q_REG_VIN_SHIFT;
		cfg->mask = Q_REG_VIN_MASK;
		break;
	case Q_GPIO_CFG_OUT_STRENGTH:
		cfg->addr = Q_REG_DIG_OUT_CTL;
		cfg->idx = Q_REG_I_DIG_OUT_CTL;
		cfg->shift = Q_REG_OUT_STRENGTH_SHIFT;
		cfg->mask = Q_REG_OUT_STRENGTH_MASK;
		break;
	case Q_GPIO_CFG_SRC_SELECT:
		cfg->addr = Q_REG_MODE_CTL;
		cfg->idx = Q_REG_I_MODE_CTL;
		cfg->shift = Q_REG_SRC_SEL_SHIFT;
		cfg->mask = Q_REG_SRC_SEL_MASK;
		break;
	case Q_GPIO_CFG_MASTER_EN:
		cfg->addr = Q_REG_EN_CTL;
		cfg->idx = Q_REG_I_EN_CTL;
		cfg->shift = Q_REG_MASTER_EN_SHIFT;
		cfg->mask = Q_REG_MASTER_EN_MASK;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qpnp_gpio_debugfs_get(void *data, u64 *val)
{
	enum qpnp_gpio_param_type *idx = data;
	struct qpnp_gpio_spec *q_spec;
	struct qpnp_gpio_reg cfg = {};
	int rc;

	rc = qpnp_gpio_reg_attr(*idx, &cfg);
	if (rc)
		return rc;
	q_spec = container_of(idx, struct qpnp_gpio_spec, params[*idx]);
	*val = q_reg_get(&q_spec->regs[cfg.idx], cfg.shift, cfg.mask);
	return 0;
}

static int qpnp_gpio_check_reg_val(enum qpnp_gpio_param_type idx,
				   struct qpnp_gpio_spec *q_spec,
				   uint32_t val)
{
	switch (idx) {
	case Q_GPIO_CFG_DIRECTION:
		if (val >= QPNP_GPIO_DIR_INVALID)
			return -EINVAL;
		break;
	case Q_GPIO_CFG_OUTPUT_TYPE:
		if ((val >= QPNP_GPIO_OUT_BUF_INVALID) ||
		   ((val == QPNP_GPIO_OUT_BUF_OPEN_DRAIN_NMOS ||
		   val == QPNP_GPIO_OUT_BUF_OPEN_DRAIN_PMOS) &&
		   (q_spec->subtype == Q_GPIO_SUBTYPE_GPIOC_4CH ||
		   (q_spec->subtype == Q_GPIO_SUBTYPE_GPIOC_8CH))))
			return -EINVAL;
		break;
	case Q_GPIO_CFG_INVERT:
		if (val >= QPNP_GPIO_INVERT_INVALID)
			return -EINVAL;
		break;
	case Q_GPIO_CFG_PULL:
		if (val >= QPNP_GPIO_PULL_INVALID)
			return -EINVAL;
		break;
	case Q_GPIO_CFG_VIN_SEL:
		if (val >= QPNP_GPIO_VIN_INVALID)
			return -EINVAL;
		break;
	case Q_GPIO_CFG_OUT_STRENGTH:
		if (val >= QPNP_GPIO_OUT_STRENGTH_INVALID ||
		    val == 0)
			return -EINVAL;
		break;
	case Q_GPIO_CFG_SRC_SELECT:
		if (val >= QPNP_GPIO_SRC_INVALID)
			return -EINVAL;
		break;
	case Q_GPIO_CFG_MASTER_EN:
		if (val >= QPNP_GPIO_MASTER_INVALID)
			return -EINVAL;
		break;
	default:
		pr_err("invalid param type %u specified\n", idx);
		return -EINVAL;
	}
	return 0;
}

static int qpnp_gpio_debugfs_set(void *data, u64 val)
{
	enum qpnp_gpio_param_type *idx = data;
	struct qpnp_gpio_spec *q_spec;
	struct qpnp_gpio_chip *q_chip;
	struct qpnp_gpio_reg cfg = {};
	int rc;

	q_spec = container_of(idx, struct qpnp_gpio_spec, params[*idx]);
	q_chip = q_spec->q_chip;

	rc = qpnp_gpio_check_reg_val(*idx, q_spec, val);
	if (rc)
		return rc;

	rc = qpnp_gpio_reg_attr(*idx, &cfg);
	if (rc)
		return rc;
	q_reg_clr_set(&q_spec->regs[cfg.idx], cfg.shift, cfg.mask, val);
	rc = spmi_ext_register_writel(q_chip->spmi->ctrl, q_spec->slave,
				      Q_REG_ADDR(q_spec, cfg.addr),
				      &q_spec->regs[cfg.idx], 1);

	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(qpnp_gpio_fops, qpnp_gpio_debugfs_get,
			qpnp_gpio_debugfs_set, "%llu\n");

#define DEBUGFS_BUF_SIZE 11 /* supports 2^32 in decimal */

struct qpnp_gpio_debugfs_args {
	enum qpnp_gpio_param_type type;
	const char *filename;
};

static struct qpnp_gpio_debugfs_args dfs_args[] = {
	{ Q_GPIO_CFG_DIRECTION, "direction" },
	{ Q_GPIO_CFG_OUTPUT_TYPE, "output_type" },
	{ Q_GPIO_CFG_INVERT, "invert" },
	{ Q_GPIO_CFG_PULL, "pull" },
	{ Q_GPIO_CFG_VIN_SEL, "vin_sel" },
	{ Q_GPIO_CFG_OUT_STRENGTH, "out_strength" },
	{ Q_GPIO_CFG_SRC_SELECT, "src_select" },
	{ Q_GPIO_CFG_MASTER_EN, "master_en" }
};

static int qpnp_gpio_debugfs_create(struct qpnp_gpio_chip *q_chip)
{
	struct spmi_device *spmi = q_chip->spmi;
	struct device *dev = &spmi->dev;
	struct qpnp_gpio_spec *q_spec;
	enum qpnp_gpio_param_type *params;
	enum qpnp_gpio_param_type type;
	char pmic_gpio[DEBUGFS_BUF_SIZE];
	const char *filename;
	struct dentry *dfs, *dfs_io_dir;
	int i, j;

	BUG_ON(Q_NUM_PARAMS != ARRAY_SIZE(dfs_args));

	q_chip->dfs_dir = debugfs_create_dir(dev->of_node->name,
							driver_dfs_dir);
	if (q_chip->dfs_dir == NULL) {
		dev_err(dev, "%s: cannot register chip debugfs directory %s\n",
						__func__, dev->of_node->name);
		return -ENODEV;
	}

	for (i = 0; i < spmi->num_dev_node; i++) {
		q_spec = qpnp_chip_gpio_get_spec(q_chip, i);
		params = q_spec->params;
		snprintf(pmic_gpio, DEBUGFS_BUF_SIZE, "%u", q_spec->pmic_gpio);
		dfs_io_dir = debugfs_create_dir(pmic_gpio,
							q_chip->dfs_dir);
		if (dfs_io_dir == NULL)
			goto dfs_err;

		for (j = 0; j < Q_NUM_PARAMS; j++) {
			type = dfs_args[j].type;
			filename = dfs_args[j].filename;

			params[type] = type;
			dfs = debugfs_create_file(
					filename,
					S_IRUGO | S_IWUSR,
					dfs_io_dir,
					&q_spec->params[type],
					&qpnp_gpio_fops);
			if (dfs == NULL)
				goto dfs_err;
		}
	}
	return 0;
dfs_err:
	dev_err(dev, "%s: cannot register debugfs for pmic gpio %u on"
				     " chip %s\n", __func__,
				     q_spec->pmic_gpio, dev->of_node->name);
	debugfs_remove_recursive(q_chip->dfs_dir);
	return -ENFILE;
}
#else
static int qpnp_gpio_debugfs_create(struct qpnp_gpio_chip *q_chip)
{
	return 0;
}
#endif

static int qpnp_gpio_probe(struct spmi_device *spmi)
{
	struct qpnp_gpio_chip *q_chip;
	struct resource *res;
	struct qpnp_gpio_spec *q_spec;
	int i, rc;
	int lowest_gpio = UINT_MAX, highest_gpio = 0;
	u32 intspec[3], gpio;
	char buf[2];

	q_chip = kzalloc(sizeof(*q_chip), GFP_KERNEL);
	if (!q_chip) {
		dev_err(&spmi->dev, "%s: Can't allocate gpio_chip\n",
								__func__);
		return -ENOMEM;
	}
	q_chip->spmi = spmi;
	dev_set_drvdata(&spmi->dev, q_chip);

	mutex_lock(&qpnp_gpio_chips_lock);
	list_add(&q_chip->chip_list, &qpnp_gpio_chips);
	mutex_unlock(&qpnp_gpio_chips_lock);

	/* first scan through nodes to find the range required for allocation */
	for (i = 0; i < spmi->num_dev_node; i++) {
		rc = of_property_read_u32(spmi->dev_node[i].of_node,
							"qcom,gpio-num", &gpio);
		if (rc) {
			dev_err(&spmi->dev, "%s: unable to get"
				" qcom,gpio-num property\n", __func__);
			goto err_probe;
		}

		if (gpio < lowest_gpio)
			lowest_gpio = gpio;
		if (gpio > highest_gpio)
			highest_gpio = gpio;
	}

	if (highest_gpio < lowest_gpio) {
		dev_err(&spmi->dev, "%s: no device nodes specified in"
					" topology\n", __func__);
		rc = -EINVAL;
		goto err_probe;
	} else if (lowest_gpio == 0) {
		dev_err(&spmi->dev, "%s: 0 is not a valid PMIC GPIO\n",
								__func__);
		rc = -EINVAL;
		goto err_probe;
	}

	q_chip->pmic_gpio_lowest = lowest_gpio;
	q_chip->pmic_gpio_highest = highest_gpio;

	/* allocate gpio lookup tables */
	q_chip->pmic_gpios = kzalloc(sizeof(struct qpnp_gpio_spec *) *
						highest_gpio - lowest_gpio + 1,
						GFP_KERNEL);
	q_chip->chip_gpios = kzalloc(sizeof(struct qpnp_gpio_spec *) *
						spmi->num_dev_node, GFP_KERNEL);
	if (!q_chip->pmic_gpios || !q_chip->chip_gpios) {
		dev_err(&spmi->dev, "%s: unable to allocate memory\n",
								__func__);
		rc = -ENOMEM;
		goto err_probe;
	}

	/* get interrupt controller device_node */
	q_chip->int_ctrl = of_irq_find_parent(spmi->dev.of_node);
	if (!q_chip->int_ctrl) {
		dev_err(&spmi->dev, "%s: Can't find interrupt parent\n",
								__func__);
		rc = -EINVAL;
		goto err_probe;
	}

	/* now scan through again and populate the lookup table */
	for (i = 0; i < spmi->num_dev_node; i++) {
		res = qpnp_get_resource(spmi, i, IORESOURCE_MEM, 0);
		if (!res) {
			dev_err(&spmi->dev, "%s: node %s is missing has no"
				" base address definition\n",
				__func__, spmi->dev_node[i].of_node->full_name);
		}

		rc = of_property_read_u32(spmi->dev_node[i].of_node,
							"qcom,gpio-num", &gpio);
		if (rc) {
			dev_err(&spmi->dev, "%s: unable to get"
				" qcom,gpio-num property\n", __func__);
			goto err_probe;
		}

		q_spec = kzalloc(sizeof(struct qpnp_gpio_spec),
							GFP_KERNEL);
		if (!q_spec) {
			dev_err(&spmi->dev, "%s: unable to allocate"
						" memory\n",
					__func__);
			rc = -ENOMEM;
			goto err_probe;
		}

		q_spec->slave = spmi->sid;
		q_spec->offset = res->start;
		q_spec->gpio_chip_idx = i;
		q_spec->pmic_gpio = gpio;
		q_spec->node = spmi->dev_node[i].of_node;
		q_spec->q_chip = q_chip;

		rc = spmi_ext_register_readl(spmi->ctrl, q_spec->slave,
				Q_REG_ADDR(q_spec, Q_REG_TYPE), &buf[0], 2);
		if (rc) {
			dev_err(&spmi->dev, "%s: unable to read type regs\n",
						__func__);
			goto err_probe;
		}
		q_spec->type	= buf[0];
		q_spec->subtype = buf[1];

		/* call into irq_domain to get irq mapping */
		intspec[0] = q_chip->spmi->sid;
		intspec[1] = (q_spec->offset >> 8) & 0xFF;
		intspec[2] = 0;
		q_spec->irq = irq_create_of_mapping(q_chip->int_ctrl,
							intspec, 3);
		if (!q_spec->irq) {
			dev_err(&spmi->dev, "%s: invalid irq for gpio"
					" %u\n", __func__, gpio);
			rc = -EINVAL;
			goto err_probe;
		}
		/* initialize lookup table params */
		qpnp_pmic_gpio_set_spec(q_chip, gpio, q_spec);
		qpnp_chip_gpio_set_spec(q_chip, i, q_spec);
	}

	q_chip->gpio_chip.base = -1;
	q_chip->gpio_chip.ngpio = spmi->num_dev_node;
	q_chip->gpio_chip.label = "qpnp-gpio";
	q_chip->gpio_chip.direction_input = qpnp_gpio_direction_input;
	q_chip->gpio_chip.direction_output = qpnp_gpio_direction_output;
	q_chip->gpio_chip.to_irq = qpnp_gpio_to_irq;
	q_chip->gpio_chip.get = qpnp_gpio_get;
	q_chip->gpio_chip.set = qpnp_gpio_set;
	q_chip->gpio_chip.dev = &spmi->dev;
	q_chip->gpio_chip.of_xlate = qpnp_gpio_of_gpio_xlate;
	q_chip->gpio_chip.of_gpio_n_cells = 2;
	q_chip->gpio_chip.can_sleep = 0;

	rc = gpiochip_add(&q_chip->gpio_chip);
	if (rc) {
		dev_err(&spmi->dev, "%s: Can't add gpio chip, rc = %d\n",
								__func__, rc);
		goto err_probe;
	}

	/* now configure gpio config defaults if they exist */
	for (i = 0; i < spmi->num_dev_node; i++) {
		q_spec = qpnp_chip_gpio_get_spec(q_chip, i);
		if (WARN_ON(!q_spec)) {
			rc = -ENODEV;
			goto err_probe;
		}

		rc = qpnp_gpio_cache_regs(q_chip, q_spec);
		if (rc)
			goto err_probe;

		rc = qpnp_gpio_apply_config(q_chip, q_spec);
		if (rc)
			goto err_probe;
	}

	dev_dbg(&spmi->dev, "%s: gpio_chip registered between %d-%u\n",
			__func__, q_chip->gpio_chip.base,
			(q_chip->gpio_chip.base + q_chip->gpio_chip.ngpio) - 1);

	rc = qpnp_gpio_debugfs_create(q_chip);
	if (rc) {
		dev_err(&spmi->dev, "%s: debugfs creation failed\n", __func__);
		goto err_probe;
	}

	return 0;

err_probe:
	qpnp_gpio_free_chip(q_chip);
	return rc;
}

static int qpnp_gpio_remove(struct spmi_device *spmi)
{
	struct qpnp_gpio_chip *q_chip = dev_get_drvdata(&spmi->dev);

	debugfs_remove_recursive(q_chip->dfs_dir);

	return qpnp_gpio_free_chip(q_chip);
}

static struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,qpnp-gpio",
	},
	{}
};

static const struct spmi_device_id qpnp_gpio_id[] = {
	{ "qcom,qpnp-gpio", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spmi, qpnp_gpio_id);

static struct spmi_driver qpnp_gpio_driver = {
	.driver		= {
		.name	= "qcom,qpnp-gpio",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_gpio_probe,
	.remove		= qpnp_gpio_remove,
	.id_table	= qpnp_gpio_id,
};

static int __init qpnp_gpio_init(void)
{
#ifdef CONFIG_GPIO_QPNP_DEBUG
	driver_dfs_dir = debugfs_create_dir("qpnp_gpio", NULL);
	if (driver_dfs_dir == NULL)
		pr_err("Cannot register top level debugfs directory\n");
#endif

	return spmi_driver_register(&qpnp_gpio_driver);
}

static void __exit qpnp_gpio_exit(void)
{
#ifdef CONFIG_GPIO_QPNP_DEBUG
	debugfs_remove_recursive(driver_dfs_dir);
#endif
	spmi_driver_unregister(&qpnp_gpio_driver);
}

MODULE_DESCRIPTION("QPNP PMIC gpio driver");
MODULE_LICENSE("GPL v2");

module_init(qpnp_gpio_init);
module_exit(qpnp_gpio_exit);
