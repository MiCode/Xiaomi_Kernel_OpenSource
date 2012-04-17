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

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
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
#define Q_NUM_CTL_REGS			5

/* control register base address offsets */
#define Q_REG_IO_CTL1			0x42
#define Q_REG_INPUT_CTL1		0x43
#define Q_REG_OUTPUT_CTL1		0x44
#define Q_REG_OUTPUT_CTL2		0x45
#define Q_REG_EN_CTL1			0x46

/* control register regs array indices */
#define Q_REG_I_IO_CTL1			0
#define Q_REG_I_INPUT_CTL1		1
#define Q_REG_I_OUTPUT_CTL1		2
#define Q_REG_I_OUTPUT_CTL2		3
#define Q_REG_I_EN_CTL1			4

/* control register configuration */
#define Q_REG_VIN_SHIFT			0
#define Q_REG_VIN_MASK			0x7
#define Q_REG_PULL_SHIFT		4
#define Q_REG_PULL_MASK			0x70
#define Q_REG_INPUT_EN_SHIFT		7
#define Q_REG_INPUT_EN_MASK		0x80
#define Q_REG_OUT_STRENGTH_SHIFT	0
#define Q_REG_OUT_STRENGTH_MASK		0x3
#define Q_REG_OUT_TYPE_SHIFT		6
#define Q_REG_OUT_TYPE_MASK		0x40
#define Q_REG_OUT_INVERT_SHIFT		0
#define Q_REG_OUT_INVERT_MASK		0x1
#define Q_REG_SRC_SEL_SHIFT		1
#define Q_REG_SRC_SEL_MASK		0xE
#define Q_REG_OUTPUT_EN_SHIFT		7
#define Q_REG_OUTPUT_EN_MASK		0x80
#define Q_REG_MASTER_EN_SHIFT		7
#define Q_REG_MASTER_EN_MASK		0x80


struct qpnp_gpio_spec {
	uint8_t slave;			/* 0-15 */
	uint16_t offset;		/* 0-255 */
	uint32_t gpio_chip_idx;		/* offset from gpio_chip base */
	int irq;			/* logical IRQ number */
	u8 regs[Q_NUM_CTL_REGS];	/* Control regs */
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
	if (!q_spec) {
		pr_err("gpio %d not handled by any pmic\n", gpio);
		return -EINVAL;
	}

	q_spec->regs[Q_REG_I_IO_CTL1] = (param->vin_sel <<
					Q_REG_VIN_SHIFT) & Q_REG_VIN_MASK;
	q_spec->regs[Q_REG_I_IO_CTL1] |= (param->pull <<
					Q_REG_PULL_SHIFT) & Q_REG_PULL_MASK;
	q_spec->regs[Q_REG_I_INPUT_CTL1] = ((param->direction &
			QPNP_GPIO_DIR_IN) ? ((1 << Q_REG_INPUT_EN_SHIFT)) : 0);

	if (param->direction & QPNP_GPIO_DIR_OUT) {
		q_spec->regs[Q_REG_I_OUTPUT_CTL1] = (param->out_strength
			 << Q_REG_OUT_STRENGTH_SHIFT) & Q_REG_OUT_STRENGTH_MASK;
		q_spec->regs[Q_REG_I_OUTPUT_CTL1] |= (param->output_type
			 << Q_REG_OUT_TYPE_SHIFT) & Q_REG_OUT_TYPE_MASK;
	} else {
		q_spec->regs[Q_REG_I_OUTPUT_CTL1] = 0;
	}

	if (param->direction & QPNP_GPIO_DIR_OUT) {
		q_spec->regs[Q_REG_I_OUTPUT_CTL2] = (param->inv_int_pol
			    << Q_REG_OUT_INVERT_SHIFT) & Q_REG_OUT_INVERT_MASK;
		q_spec->regs[Q_REG_I_OUTPUT_CTL2] |= (param->src_select
			    << Q_REG_SRC_SEL_SHIFT) & Q_REG_SRC_SEL_MASK;
		q_spec->regs[Q_REG_I_OUTPUT_CTL2] |= (1 <<
			      Q_REG_OUTPUT_EN_SHIFT) & Q_REG_OUTPUT_EN_MASK;
	} else {
		q_spec->regs[Q_REG_I_OUTPUT_CTL2] = 0;
	}

	q_spec->regs[Q_REG_I_EN_CTL1] = (param->master_en <<
				Q_REG_MASTER_EN_SHIFT) & Q_REG_MASTER_EN_MASK;

	rc = spmi_ext_register_writel(q_chip->spmi->ctrl, q_spec->slave,
			      Q_REG_ADDR(q_spec, Q_REG_IO_CTL1),
			      &q_spec->regs[Q_REG_I_IO_CTL1], Q_NUM_CTL_REGS);
	if (rc)
		dev_err(&q_chip->spmi->dev, "%s: unable to write master"
						" enable\n", __func__);

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
	if (q_spec->regs[Q_REG_I_INPUT_CTL1] & Q_REG_INPUT_EN_MASK) {
		/* INT_RT_STS */
		rc = spmi_ext_register_readl(q_chip->spmi->ctrl, q_spec->slave,
				Q_REG_ADDR(q_spec, Q_REG_STATUS1),
				&buf[0], 1);
		return buf[0];

	} else {
		ret_val = (q_spec->regs[Q_REG_I_OUTPUT_CTL2] &
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

	q_spec->regs[Q_REG_I_OUTPUT_CTL2] &= ~(1 << Q_REG_OUT_INVERT_SHIFT);

	if (value)
		q_spec->regs[Q_REG_I_OUTPUT_CTL2] |=
					    (1 << Q_REG_OUT_INVERT_SHIFT);

	rc = spmi_ext_register_writel(q_chip->spmi->ctrl, q_spec->slave,
			      Q_REG_ADDR(q_spec, Q_REG_OUTPUT_CTL2),
			      &q_spec->regs[Q_REG_I_OUTPUT_CTL2], 1);
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

	if (direction & QPNP_GPIO_DIR_IN) {
		q_spec->regs[Q_REG_I_INPUT_CTL1] |=
					(1 << Q_REG_INPUT_EN_SHIFT);
		q_spec->regs[Q_REG_I_OUTPUT_CTL2] &=
					~(1 << Q_REG_OUTPUT_EN_SHIFT);
	} else {
		q_spec->regs[Q_REG_I_INPUT_CTL1] &=
					~(1 << Q_REG_INPUT_EN_SHIFT);
		q_spec->regs[Q_REG_I_OUTPUT_CTL2] |=
					(1 << Q_REG_OUTPUT_EN_SHIFT);
	}

	rc = spmi_ext_register_writel(q_chip->spmi->ctrl, q_spec->slave,
			      Q_REG_ADDR(q_spec, Q_REG_INPUT_CTL1),
			      &q_spec->regs[Q_REG_I_INPUT_CTL1], 3);
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
		pr_err("%s: of_gpio_n_cells < 2\n", __func__);
		return -EINVAL;
	}

	q_spec = qpnp_pmic_gpio_get_spec(q_chip, n);
	if (!q_spec) {
		pr_err("%s: no such PMIC gpio %u in device topology\n",
							__func__, n);
		return -EINVAL;
	}

	if (flags)
		*flags = be32_to_cpu(gpio[1]);

	return q_spec->gpio_chip_idx;
}

static int qpnp_gpio_config_default(struct spmi_device *spmi,
					const __be32 *prop, int gpio)
{
	struct qpnp_gpio_cfg param;
	int rc;

	dev_dbg(&spmi->dev, "%s: p[0]: 0x%x p[1]: 0x%x p[2]: 0x%x p[3]:"
		" 0x%x p[4]: 0x%x p[5]: 0x%x p[6]: 0x%x p[7]: 0x%x"
		" p[8]: 0x%x\n", __func__,
		be32_to_cpup(&prop[0]), be32_to_cpup(&prop[1]),
		be32_to_cpup(&prop[2]), be32_to_cpup(&prop[3]),
		be32_to_cpup(&prop[4]), be32_to_cpup(&prop[5]),
		be32_to_cpup(&prop[6]), be32_to_cpup(&prop[7]),
		be32_to_cpup(&prop[8]));

	param.direction    =	be32_to_cpup(&prop[0]);
	param.output_type  =	be32_to_cpup(&prop[1]);
	param.output_value =	be32_to_cpup(&prop[2]);
	param.pull	   =	be32_to_cpup(&prop[3]);
	param.vin_sel	   =	be32_to_cpup(&prop[4]);
	param.out_strength =	be32_to_cpup(&prop[5]);
	param.src_select   =	be32_to_cpup(&prop[6]);
	param.inv_int_pol  =	be32_to_cpup(&prop[7]);
	param.master_en    =	be32_to_cpup(&prop[8]);

	rc = qpnp_gpio_config(gpio, &param);
	if (rc)
		dev_err(&spmi->dev, "%s: unable to set default config for"
				" gpio %d\n", __func__, gpio);
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

static int qpnp_gpio_probe(struct spmi_device *spmi)
{
	struct qpnp_gpio_chip *q_chip;
	struct resource *res;
	struct qpnp_gpio_spec *q_spec;
	const __be32 *prop;
	int i, rc, ret, gpio, len;
	int lowest_gpio = INT_MAX, highest_gpio = INT_MIN;
	u32 intspec[3];

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
		prop = of_get_property(spmi->dev_node[i].of_node,
						"qcom,qpnp-gpio-num", &len);
		if (!prop) {
			dev_err(&spmi->dev, "%s: unable to get"
				" qcom,qpnp-gpio-num property\n", __func__);
			ret = -EINVAL;
			goto err_probe;
		} else if (len != sizeof(__be32)) {
			dev_err(&spmi->dev, "%s: Invalid qcom,qpnp-gpio-num"
				" property\n", __func__);
			ret = -EINVAL;
			goto err_probe;
		}

		gpio = be32_to_cpup(prop);
		if (gpio < lowest_gpio)
			lowest_gpio = gpio;
		if (gpio > highest_gpio)
			highest_gpio = gpio;
	}

	if (highest_gpio < lowest_gpio) {
		dev_err(&spmi->dev, "%s: no device nodes specified in"
					" topology\n", __func__);
		ret = -EINVAL;
		goto err_probe;
	} else if (lowest_gpio == 0) {
		dev_err(&spmi->dev, "%s: 0 is not a valid PMIC GPIO\n",
								__func__);
		ret = -EINVAL;
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
		ret = -ENOMEM;
		goto err_probe;
	}

	/* get interrupt controller device_node */
	q_chip->int_ctrl = of_irq_find_parent(spmi->dev.of_node);
	if (!q_chip->int_ctrl) {
		dev_err(&spmi->dev, "%s: Can't find interrupt parent\n",
								__func__);
		ret = -EINVAL;
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

		prop = of_get_property(spmi->dev_node[i].of_node,
				"qcom,qpnp-gpio-num", &len);
		if (!prop) {
			dev_err(&spmi->dev, "%s: unable to get"
				" qcom,qpnp-gpio-num property\n", __func__);
			ret = -EINVAL;
			goto err_probe;
		} else if (len != sizeof(__be32)) {
			dev_err(&spmi->dev, "%s: Invalid qcom,qpnp-gpio-num"
				" property\n", __func__);
			ret = -EINVAL;
			goto err_probe;
		}
		gpio = be32_to_cpup(prop);

		q_spec = kzalloc(sizeof(struct qpnp_gpio_spec),
							GFP_KERNEL);
		if (!q_spec) {
			dev_err(&spmi->dev, "%s: unable to allocate"
						" memory\n",
					__func__);
			ret = -ENOMEM;
			goto err_probe;
		}

		q_spec->slave = spmi->sid;
		q_spec->offset = res->start;
		q_spec->gpio_chip_idx = i;

		/* call into irq_domain to get irq mapping */
		intspec[0] = q_chip->spmi->sid;
		intspec[1] = (q_spec->offset >> 8) & 0xFF;
		intspec[2] = 0;
		q_spec->irq = irq_create_of_mapping(q_chip->int_ctrl,
							intspec, 3);
		if (!q_spec->irq) {
			dev_err(&spmi->dev, "%s: invalid irq for gpio"
					" %u\n", __func__, gpio);
			ret = -EINVAL;
			goto err_probe;
		}
		/* initialize lookup table entries */
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
		ret = rc;
		goto err_probe;
	}

	/* now configure gpio defaults if they exist */
	for (i = 0; i < spmi->num_dev_node; i++) {
		q_spec = qpnp_chip_gpio_get_spec(q_chip, i);
		if (WARN_ON(!q_spec))
			return -ENODEV;

		/* It's not an error to not config a default */
		prop = of_get_property(spmi->dev_node[i].of_node,
				"qcom,qpnp-gpio-cfg", &len);
		/* 9 data values constitute one tuple */
		if (prop && (len != (9 * sizeof(__be32)))) {
			dev_err(&spmi->dev, "%s: invalid format for"
				" qcom,qpnp-gpio-cfg property\n",
							__func__);
			ret = -EINVAL;
			goto err_probe;
		} else if (prop) {
			rc = qpnp_gpio_config_default(spmi, prop,
				     q_chip->gpio_chip.base + i);
			if (rc) {
				ret = rc;
				goto err_probe;
			}
		} else {
			/* initialize with hardware defaults */
			rc = spmi_ext_register_readl(
				q_chip->spmi->ctrl, q_spec->slave,
				Q_REG_ADDR(q_spec, Q_REG_IO_CTL1),
				&q_spec->regs[Q_REG_I_IO_CTL1],
				Q_NUM_CTL_REGS);
			q_spec->regs[Q_REG_I_EN_CTL1] |=
				(1 << Q_REG_MASTER_EN_SHIFT);
			rc = spmi_ext_register_writel(
				q_chip->spmi->ctrl, q_spec->slave,
				Q_REG_ADDR(q_spec, Q_REG_EN_CTL1),
				&q_spec->regs[Q_REG_EN_CTL1], 1);
			if (rc) {
				dev_err(&spmi->dev, "%s: spmi write"
						" failed\n", __func__);
				ret = rc;
				goto err_probe;
			}
		}
	}

	dev_dbg(&spmi->dev, "%s: gpio_chip registered between %d-%u\n",
			__func__, q_chip->gpio_chip.base,
			(q_chip->gpio_chip.base + q_chip->gpio_chip.ngpio) - 1);
	return 0;

err_probe:
	qpnp_gpio_free_chip(q_chip);
	return ret;
}

static int qpnp_gpio_remove(struct spmi_device *spmi)
{
	struct qpnp_gpio_chip *q_chip = dev_get_drvdata(&spmi->dev);

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
	return spmi_driver_register(&qpnp_gpio_driver);
}

static void __exit qpnp_gpio_exit(void)
{
}

MODULE_DESCRIPTION("QPNP PMIC gpio driver");
MODULE_LICENSE("GPL v2");

module_init(qpnp_gpio_init);
module_exit(qpnp_gpio_exit);
