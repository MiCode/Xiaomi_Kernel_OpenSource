/*
 * Copyright (c) 2013, Sony Mobile Communications AB.
 * Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/log2.h>
#include <linux/irq.h>
#include "../core.h"
#include "../pinconf.h"
#include "pinctrl-msm.h"
#include "../pinctrl-utils.h"

#define MAX_NR_GPIO 300
#define PS_HOLD_OFFSET 0x820

/**
 * struct msm_pinctrl - state for a pinctrl-msm device
 * @dev:            device handle.
 * @pctrl:          pinctrl handle.
 * @chip:           gpiochip handle.
 * @restart_nb:     restart notifier block.
 * @irq:            parent irq for the TLMM irq_chip.
 * @lock:           Spinlock to protect register resources as well
 *                  as msm_pinctrl data structures.
 * @enabled_irqs:   Bitmap of currently enabled irqs.
 * @dual_edge_irqs: Bitmap of irqs that need sw emulated dual edge
 *                  detection.
 * @soc;            Reference to soc_data of platform specific data.
 * @regs:           Base address for the TLMM register map.
 */
struct msm_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pctrl;
	struct gpio_chip chip;
	struct notifier_block restart_nb;
	int irq;

	raw_spinlock_t lock;

	DECLARE_BITMAP(dual_edge_irqs, MAX_NR_GPIO);
	DECLARE_BITMAP(enabled_irqs, MAX_NR_GPIO);

	const struct msm_pinctrl_soc_data *soc;
	void __iomem *regs;
};

static int msm_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	return pctrl->soc->ngroups;
}

static const char *msm_get_group_name(struct pinctrl_dev *pctldev,
				      unsigned group)
{
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	return pctrl->soc->groups[group].name;
}

static int msm_get_group_pins(struct pinctrl_dev *pctldev,
			      unsigned group,
			      const unsigned **pins,
			      unsigned *num_pins)
{
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	*pins = pctrl->soc->groups[group].pins;
	*num_pins = pctrl->soc->groups[group].npins;
	return 0;
}

static const struct pinctrl_ops msm_pinctrl_ops = {
	.get_groups_count	= msm_get_groups_count,
	.get_group_name		= msm_get_group_name,
	.get_group_pins		= msm_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_group,
	.dt_free_map		= pinctrl_utils_free_map,
};

static int msm_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	return pctrl->soc->nfunctions;
}

static const char *msm_get_function_name(struct pinctrl_dev *pctldev,
					 unsigned function)
{
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	return pctrl->soc->functions[function].name;
}

static int msm_get_function_groups(struct pinctrl_dev *pctldev,
				   unsigned function,
				   const char * const **groups,
				   unsigned * const num_groups)
{
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pctrl->soc->functions[function].groups;
	*num_groups = pctrl->soc->functions[function].ngroups;
	return 0;
}

static int msm_pinmux_set_mux(struct pinctrl_dev *pctldev,
			      unsigned function,
			      unsigned group)
{
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	const struct msm_pingroup *g;
	unsigned long flags;
	u32 val, mask;
	int i;

	g = &pctrl->soc->groups[group];
	mask = GENMASK(g->mux_bit + order_base_2(g->nfuncs) - 1, g->mux_bit);

	for (i = 0; i < g->nfuncs; i++) {
		if (g->funcs[i] == function)
			break;
	}

	if (WARN_ON(i == g->nfuncs))
		return -EINVAL;

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	val = readl(pctrl->regs + g->ctl_reg);
	val &= ~mask;
	val |= i << g->mux_bit;
	/* Check if egpio present and enable that feature */
	if (val & BIT(g->egpio_present))
		val |= BIT(g->egpio_enable);

	writel(val, pctrl->regs + g->ctl_reg);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);

	return 0;
}

static const struct pinmux_ops msm_pinmux_ops = {
	.get_functions_count	= msm_get_functions_count,
	.get_function_name	= msm_get_function_name,
	.get_function_groups	= msm_get_function_groups,
	.set_mux		= msm_pinmux_set_mux,
};

static int msm_config_reg(struct msm_pinctrl *pctrl,
			  const struct msm_pingroup *g,
			  unsigned param,
			  unsigned *mask,
			  unsigned *bit)
{
	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_BUS_HOLD:
	case PIN_CONFIG_BIAS_PULL_UP:
		*bit = g->pull_bit;
		*mask = 3;
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		*bit = g->drv_bit;
		*mask = 7;
		break;
	case PIN_CONFIG_OUTPUT:
	case PIN_CONFIG_INPUT_ENABLE:
		*bit = g->oe_bit;
		*mask = 1;
		break;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

#define MSM_NO_PULL		0
#define MSM_PULL_DOWN		1
#define MSM_KEEPER		2
#define MSM_PULL_UP_NO_KEEPER	2
#define MSM_PULL_UP		3

static unsigned msm_regval_to_drive(u32 val)
{
	return (val + 1) * 2;
}

static int msm_config_group_get(struct pinctrl_dev *pctldev,
				unsigned int group,
				unsigned long *config)
{
	const struct msm_pingroup *g;
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	unsigned param = pinconf_to_config_param(*config);
	unsigned mask;
	unsigned arg;
	unsigned bit;
	int ret;
	u32 val;

	g = &pctrl->soc->groups[group];

	ret = msm_config_reg(pctrl, g, param, &mask, &bit);
	if (ret < 0)
		return ret;

	val = readl(pctrl->regs + g->ctl_reg);
	arg = (val >> bit) & mask;

	/* Convert register value to pinconf value */
	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		arg = arg == MSM_NO_PULL;
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		arg = arg == MSM_PULL_DOWN;
		break;
	case PIN_CONFIG_BIAS_BUS_HOLD:
		if (pctrl->soc->pull_no_keeper)
			return -ENOTSUPP;

		arg = arg == MSM_KEEPER;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		if (pctrl->soc->pull_no_keeper)
			arg = arg == MSM_PULL_UP_NO_KEEPER;
		else
			arg = arg == MSM_PULL_UP;
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		arg = msm_regval_to_drive(arg);
		break;
	case PIN_CONFIG_OUTPUT:
		/* Pin is not output */
		if (!arg)
			return -EINVAL;

		val = readl(pctrl->regs + g->io_reg);
		arg = !!(val & BIT(g->in_bit));
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		/* Pin is output */
		if (arg)
			return -EINVAL;
		arg = 1;
		break;
	default:
		return -ENOTSUPP;
	}

	*config = pinconf_to_config_packed(param, arg);

	return 0;
}

static int msm_config_group_set(struct pinctrl_dev *pctldev,
				unsigned group,
				unsigned long *configs,
				unsigned num_configs)
{
	const struct msm_pingroup *g;
	struct msm_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;
	unsigned param;
	unsigned mask;
	unsigned arg;
	unsigned bit;
	int ret;
	u32 val;
	int i;

	g = &pctrl->soc->groups[group];

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		ret = msm_config_reg(pctrl, g, param, &mask, &bit);
		if (ret < 0)
			return ret;

		/* Convert pinconf values to register values */
		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
			arg = MSM_NO_PULL;
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			arg = MSM_PULL_DOWN;
			break;
		case PIN_CONFIG_BIAS_BUS_HOLD:
			if (pctrl->soc->pull_no_keeper)
				return -ENOTSUPP;

			arg = MSM_KEEPER;
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			if (pctrl->soc->pull_no_keeper)
				arg = MSM_PULL_UP_NO_KEEPER;
			else
				arg = MSM_PULL_UP;
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			/* Check for invalid values */
			if (arg > 16 || arg < 2 || (arg % 2) != 0)
				arg = -1;
			else
				arg = (arg / 2) - 1;
			break;
		case PIN_CONFIG_OUTPUT:
			/* set output value */
			raw_spin_lock_irqsave(&pctrl->lock, flags);
			val = readl(pctrl->regs + g->io_reg);
			if (arg)
				val |= BIT(g->out_bit);
			else
				val &= ~BIT(g->out_bit);
			writel(val, pctrl->regs + g->io_reg);
			raw_spin_unlock_irqrestore(&pctrl->lock, flags);

			/* enable output */
			arg = 1;
			break;
		case PIN_CONFIG_INPUT_ENABLE:
			/* disable output */
			arg = 0;
			break;
		default:
			dev_err(pctrl->dev, "Unsupported config parameter: %x\n",
				param);
			return -EINVAL;
		}

		/* Range-check user-supplied value */
		if (arg & ~mask) {
			dev_err(pctrl->dev, "config %x: %x is invalid\n", param, arg);
			return -EINVAL;
		}

		raw_spin_lock_irqsave(&pctrl->lock, flags);
		val = readl(pctrl->regs + g->ctl_reg);
		val &= ~(mask << bit);
		val |= arg << bit;
		writel(val, pctrl->regs + g->ctl_reg);
		raw_spin_unlock_irqrestore(&pctrl->lock, flags);
	}

	return 0;
}

static const struct pinconf_ops msm_pinconf_ops = {
	.is_generic		= true,
	.pin_config_group_get	= msm_config_group_get,
	.pin_config_group_set	= msm_config_group_set,
};

static struct pinctrl_desc msm_pinctrl_desc = {
	.pctlops = &msm_pinctrl_ops,
	.pmxops = &msm_pinmux_ops,
	.confops = &msm_pinconf_ops,
	.owner = THIS_MODULE,
};

static int msm_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	const struct msm_pingroup *g;
	struct msm_pinctrl *pctrl = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	g = &pctrl->soc->groups[offset];

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	val = readl(pctrl->regs + g->ctl_reg);
	val &= ~BIT(g->oe_bit);
	writel(val, pctrl->regs + g->ctl_reg);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);

	return 0;
}

static int msm_gpio_direction_output(struct gpio_chip *chip, unsigned offset, int value)
{
	const struct msm_pingroup *g;
	struct msm_pinctrl *pctrl = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	g = &pctrl->soc->groups[offset];

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	val = readl(pctrl->regs + g->io_reg);
	if (value)
		val |= BIT(g->out_bit);
	else
		val &= ~BIT(g->out_bit);
	writel(val, pctrl->regs + g->io_reg);

	val = readl(pctrl->regs + g->ctl_reg);
	val |= BIT(g->oe_bit);
	writel(val, pctrl->regs + g->ctl_reg);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);

	return 0;
}

static int msm_gpio_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	struct msm_pinctrl *pctrl = gpiochip_get_data(chip);
	const struct msm_pingroup *g;
	u32 val;

	g = &pctrl->soc->groups[offset];

	val = readl(pctrl->regs + g->ctl_reg);

	/* 0 = output, 1 = input */
	return val & BIT(g->oe_bit) ? 0 : 1;
}

static int msm_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	const struct msm_pingroup *g;
	struct msm_pinctrl *pctrl = gpiochip_get_data(chip);
	u32 val;

	g = &pctrl->soc->groups[offset];

	val = readl(pctrl->regs + g->io_reg);
	return !!(val & BIT(g->in_bit));
}

static void msm_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	const struct msm_pingroup *g;
	struct msm_pinctrl *pctrl = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	g = &pctrl->soc->groups[offset];

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	val = readl(pctrl->regs + g->io_reg);
	if (value)
		val |= BIT(g->out_bit);
	else
		val &= ~BIT(g->out_bit);
	writel(val, pctrl->regs + g->io_reg);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);
}

#ifdef CONFIG_DEBUG_FS
#include <linux/seq_file.h>

static void msm_gpio_dbg_show_one(struct seq_file *s,
				  struct pinctrl_dev *pctldev,
				  struct gpio_chip *chip,
				  unsigned offset,
				  unsigned gpio)
{
	const struct msm_pingroup *g;
	struct msm_pinctrl *pctrl = gpiochip_get_data(chip);
	unsigned func;
	int is_out;
	int drive;
	int pull;
	u32 ctl_reg;

	static const char * const pulls[] = {
		"no pull",
		"pull down",
		"keeper",
		"pull up"
	};

	g = &pctrl->soc->groups[offset];
	ctl_reg = readl(pctrl->regs + g->ctl_reg);

	is_out = !!(ctl_reg & BIT(g->oe_bit));
	func = (ctl_reg >> g->mux_bit) & 7;
	drive = (ctl_reg >> g->drv_bit) & 7;
	pull = (ctl_reg >> g->pull_bit) & 3;

	seq_printf(s, " %-8s: %-3s %d", g->name, is_out ? "out" : "in", func);
	seq_printf(s, " %dmA", msm_regval_to_drive(drive));
	seq_printf(s, " %s", pulls[pull]);
}

static void msm_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	unsigned gpio = chip->base;
	unsigned i;

	for (i = 0; i < chip->ngpio; i++, gpio++) {
		msm_gpio_dbg_show_one(s, NULL, chip, i, gpio);
		seq_puts(s, "\n");
	}
}

#else
#define msm_gpio_dbg_show NULL
#endif

static const struct gpio_chip msm_gpio_template = {
	.direction_input  = msm_gpio_direction_input,
	.direction_output = msm_gpio_direction_output,
	.get_direction    = msm_gpio_get_direction,
	.get              = msm_gpio_get,
	.set              = msm_gpio_set,
	.request          = gpiochip_generic_request,
	.free             = gpiochip_generic_free,
	.dbg_show         = msm_gpio_dbg_show,
};

/* For dual-edge interrupts in software, since some hardware has no
 * such support:
 *
 * At appropriate moments, this function may be called to flip the polarity
 * settings of both-edge irq lines to try and catch the next edge.
 *
 * The attempt is considered successful if:
 * - the status bit goes high, indicating that an edge was caught, or
 * - the input value of the gpio doesn't change during the attempt.
 * If the value changes twice during the process, that would cause the first
 * test to fail but would force the second, as two opposite
 * transitions would cause a detection no matter the polarity setting.
 *
 * The do-loop tries to sledge-hammer closed the timing hole between
 * the initial value-read and the polarity-write - if the line value changes
 * during that window, an interrupt is lost, the new polarity setting is
 * incorrect, and the first success test will fail, causing a retry.
 *
 * Algorithm comes from Google's msmgpio driver.
 */
static void msm_gpio_update_dual_edge_pos(struct msm_pinctrl *pctrl,
					  const struct msm_pingroup *g,
					  struct irq_data *d)
{
	int loop_limit = 100;
	unsigned val, val2, intstat;
	unsigned pol;

	do {
		val = readl(pctrl->regs + g->io_reg) & BIT(g->in_bit);

		pol = readl(pctrl->regs + g->intr_cfg_reg);
		pol ^= BIT(g->intr_polarity_bit);
		writel(pol, pctrl->regs + g->intr_cfg_reg);

		val2 = readl(pctrl->regs + g->io_reg) & BIT(g->in_bit);
		intstat = readl(pctrl->regs + g->intr_status_reg);
		if (intstat || (val == val2))
			return;
	} while (loop_limit-- > 0);
	dev_err(pctrl->dev, "dual-edge irq failed to stabilize, %#08x != %#08x\n",
		val, val2);
}

static void msm_gpio_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	const struct msm_pingroup *g;
	unsigned long flags;
	u32 val;

	g = &pctrl->soc->groups[d->hwirq];

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	val = readl(pctrl->regs + g->intr_cfg_reg);
	val &= ~BIT(g->intr_enable_bit);
	writel(val, pctrl->regs + g->intr_cfg_reg);

	clear_bit(d->hwirq, pctrl->enabled_irqs);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);

	if (d->parent_data)
		irq_chip_mask_parent(d);
}

static void msm_gpio_irq_enable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	const struct msm_pingroup *g;
	unsigned long flags;
	u32 val;

	g = &pctrl->soc->groups[d->hwirq];

	raw_spin_lock_irqsave(&pctrl->lock, flags);
	/* clear the interrupt status bit before unmask to avoid
	 * any erraneous interrupts that would have got latched
	 * when the intterupt is not in use.
	 */
	val = readl(pctrl->regs + g->intr_status_reg);
	val &= ~BIT(g->intr_status_bit);
	writel(val, pctrl->regs + g->intr_status_reg);

	val = readl(pctrl->regs + g->intr_cfg_reg);
	val |= BIT(g->intr_enable_bit);
	writel(val, pctrl->regs + g->intr_cfg_reg);

	set_bit(d->hwirq, pctrl->enabled_irqs);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);

	if (d->parent_data)
		irq_chip_enable_parent(d);
}

static void msm_gpio_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	const struct msm_pingroup *g;
	unsigned long flags;
	u32 val;

	g = &pctrl->soc->groups[d->hwirq];

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	val = readl(pctrl->regs + g->intr_cfg_reg);
	val |= BIT(g->intr_enable_bit);
	writel(val, pctrl->regs + g->intr_cfg_reg);

	set_bit(d->hwirq, pctrl->enabled_irqs);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);

	if (d->parent_data)
		irq_chip_unmask_parent(d);
}

static void msm_gpio_irq_ack(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	const struct msm_pingroup *g;
	unsigned long flags;
	u32 val;

	g = &pctrl->soc->groups[d->hwirq];

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	val = readl(pctrl->regs + g->intr_status_reg);
	if (g->intr_ack_high)
		val |= BIT(g->intr_status_bit);
	else
		val &= ~BIT(g->intr_status_bit);
	writel(val, pctrl->regs + g->intr_status_reg);

	if (test_bit(d->hwirq, pctrl->dual_edge_irqs))
		msm_gpio_update_dual_edge_pos(pctrl, g, d);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);
}

static int msm_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	const struct msm_pingroup *g;
	unsigned long flags;
	u32 val;

	g = &pctrl->soc->groups[d->hwirq];

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	/*
	 * For hw without possibility of detecting both edges
	 */
	if (g->intr_detection_width == 1 && type == IRQ_TYPE_EDGE_BOTH)
		set_bit(d->hwirq, pctrl->dual_edge_irqs);
	else
		clear_bit(d->hwirq, pctrl->dual_edge_irqs);

	/* Route interrupts to application cpu */
	val = readl(pctrl->regs + g->intr_target_reg);
	val &= ~(7 << g->intr_target_bit);
	val |= g->intr_target_kpss_val << g->intr_target_bit;
	writel(val, pctrl->regs + g->intr_target_reg);

	/* Update configuration for gpio.
	 * RAW_STATUS_EN is left on for all gpio irqs. Due to the
	 * internal circuitry of TLMM, toggling the RAW_STATUS
	 * could cause the INTR_STATUS to be set for EDGE interrupts.
	 */
	val = readl(pctrl->regs + g->intr_cfg_reg);
	val |= BIT(g->intr_raw_status_bit);
	if (g->intr_detection_width == 2) {
		val &= ~(3 << g->intr_detection_bit);
		val &= ~(1 << g->intr_polarity_bit);
		switch (type) {
		case IRQ_TYPE_EDGE_RISING:
			val |= 1 << g->intr_detection_bit;
			val |= BIT(g->intr_polarity_bit);
			break;
		case IRQ_TYPE_EDGE_FALLING:
			val |= 2 << g->intr_detection_bit;
			val |= BIT(g->intr_polarity_bit);
			break;
		case IRQ_TYPE_EDGE_BOTH:
			val |= 3 << g->intr_detection_bit;
			val |= BIT(g->intr_polarity_bit);
			break;
		case IRQ_TYPE_LEVEL_LOW:
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			val |= BIT(g->intr_polarity_bit);
			break;
		}
	} else if (g->intr_detection_width == 1) {
		val &= ~(1 << g->intr_detection_bit);
		val &= ~(1 << g->intr_polarity_bit);
		switch (type) {
		case IRQ_TYPE_EDGE_RISING:
			val |= BIT(g->intr_detection_bit);
			val |= BIT(g->intr_polarity_bit);
			break;
		case IRQ_TYPE_EDGE_FALLING:
			val |= BIT(g->intr_detection_bit);
			break;
		case IRQ_TYPE_EDGE_BOTH:
			val |= BIT(g->intr_detection_bit);
			val |= BIT(g->intr_polarity_bit);
			break;
		case IRQ_TYPE_LEVEL_LOW:
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			val |= BIT(g->intr_polarity_bit);
			break;
		}
	} else {
		BUG();
	}
	writel(val, pctrl->regs + g->intr_cfg_reg);

	if (test_bit(d->hwirq, pctrl->dual_edge_irqs))
		msm_gpio_update_dual_edge_pos(pctrl, g, d);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);

	if (d->parent_data)
		irq_chip_set_type_parent(d, type);

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH))
		irq_set_handler_locked(d, handle_level_irq);
	else if (type & (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING))
		irq_set_handler_locked(d, handle_edge_irq);

	return 0;
}

static int msm_gpio_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	unsigned long flags;

	raw_spin_lock_irqsave(&pctrl->lock, flags);

	irq_set_irq_wake(pctrl->irq, on);

	raw_spin_unlock_irqrestore(&pctrl->lock, flags);

	if (d->parent_data)
		irq_chip_set_wake_parent(d, on);

	return 0;
}

static int msm_gpiochip_irq_reqres(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);

	if (!try_module_get(chip->owner))
		return -ENODEV;

	if (gpiochip_lock_as_irq(chip, d->hwirq)) {
		pr_err("unable to lock HW IRQ %lu for IRQ\n", d->hwirq);
		module_put(chip->owner);
		return -EINVAL;
	}
	return 0;
}

static void msm_gpiochip_irq_relres(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);

	gpiochip_unlock_as_irq(chip, d->hwirq);
	module_put(chip->owner);
}

static struct irq_chip msm_gpio_irq_chip = {
	.name           = "msmgpio",
	.irq_enable     = msm_gpio_irq_enable,
	.irq_mask       = msm_gpio_irq_mask,
	.irq_unmask     = msm_gpio_irq_unmask,
	.irq_ack        = msm_gpio_irq_ack,
	.irq_set_type   = msm_gpio_irq_set_type,
	.irq_set_wake   = msm_gpio_irq_set_wake,
	.irq_request_resources    = msm_gpiochip_irq_reqres,
	.irq_release_resources	  = msm_gpiochip_irq_relres,
	.flags                    = IRQCHIP_MASK_ON_SUSPEND |
					IRQCHIP_SKIP_SET_WAKE,
};

static void msm_gpio_domain_set_info(struct irq_domain *d, unsigned int irq,
							irq_hw_number_t hwirq)
{
	struct gpio_chip *gc = d->host_data;

	irq_domain_set_info(d, irq, hwirq, gc->irqchip, d->host_data,
		gc->irq_handler, NULL, NULL);

	if (gc->can_sleep)
		irq_set_nested_thread(irq, 1);

	irq_set_noprobe(irq);
}

static int msm_gpio_domain_translate(struct irq_domain *d,
	struct irq_fwspec *fwspec, unsigned long *hwirq, unsigned int *type)
{
	if (is_of_node(fwspec->fwnode)) {
		if (fwspec->param_count < 2)
			return -EINVAL;
		if (hwirq)
			*hwirq = fwspec->param[0];
		if (type)
			*type = fwspec->param[1] & IRQ_TYPE_SENSE_MASK;
		return 0;
	}

	return -EINVAL;
}

static int msm_gpio_domain_alloc(struct irq_domain *domain, unsigned int virq,
					unsigned int nr_irqs, void *arg)
{
	int ret = 0;
	irq_hw_number_t hwirq;
	struct irq_fwspec *fwspec = arg, parent_fwspec;

	ret = msm_gpio_domain_translate(domain, fwspec, &hwirq, NULL);
	if (ret)
		return ret;

	msm_gpio_domain_set_info(domain, virq, hwirq);

	parent_fwspec = *fwspec;
	parent_fwspec.fwnode = domain->parent->fwnode;

	return irq_domain_alloc_irqs_parent(domain, virq, nr_irqs,
					&parent_fwspec);
}

static const struct irq_domain_ops msm_gpio_domain_ops = {
	.translate	= msm_gpio_domain_translate,
	.alloc		= msm_gpio_domain_alloc,
	.free		= irq_domain_free_irqs_top,
};

static bool is_gpio_dual_edge(struct irq_data *d, irq_hw_number_t *dir_conn_irq)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	int i;

	if (!parent_data)
		return false;

	for (i = 0; i < pctrl->soc->n_dir_conns; i++) {
		const struct msm_dir_conn *dir_conn = &pctrl->soc->dir_conn[i];

		if (dir_conn->gpio == d->hwirq && (dir_conn->hwirq + 32)
				!= parent_data->hwirq) {
			*dir_conn_irq = dir_conn->hwirq + 32;
			return true;
		}
	}
	return false;
}

static void msm_dirconn_irq_mask(struct irq_data *d)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);
	irq_hw_number_t dir_conn_irq = 0;

	if (!parent_data)
		return;

	if (is_gpio_dual_edge(d, &dir_conn_irq)) {
		struct irq_data *dir_conn_data =
			irq_get_irq_data(irq_find_mapping(parent_data->domain,
						dir_conn_irq));

		if (dir_conn_data && dir_conn_data->chip->irq_mask)
			dir_conn_data->chip->irq_mask(dir_conn_data);
	}
	if (parent_data->chip->irq_mask)
		parent_data->chip->irq_mask(parent_data);
}

static void msm_dirconn_irq_enable(struct irq_data *d)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);
	irq_hw_number_t dir_conn_irq = 0;

	if (!parent_data)
		return;

	if (is_gpio_dual_edge(d, &dir_conn_irq)) {
		struct irq_data *dir_conn_data =
			irq_get_irq_data(irq_find_mapping(parent_data->domain,
						dir_conn_irq));

		if (dir_conn_data &&
				dir_conn_data->chip->irq_set_irqchip_state)
			dir_conn_data->chip->irq_set_irqchip_state(
					dir_conn_data,
					IRQCHIP_STATE_PENDING, 0);

		if (dir_conn_data && dir_conn_data->chip->irq_unmask)
			dir_conn_data->chip->irq_unmask(dir_conn_data);
	}

	if (parent_data->chip->irq_set_irqchip_state)
		parent_data->chip->irq_set_irqchip_state(parent_data,
						IRQCHIP_STATE_PENDING, 0);

	if (parent_data->chip->irq_unmask)
		parent_data->chip->irq_unmask(parent_data);
}

static void msm_dirconn_irq_unmask(struct irq_data *d)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);
	irq_hw_number_t dir_conn_irq = 0;

	if (!parent_data)
		return;

	if (is_gpio_dual_edge(d, &dir_conn_irq)) {
		struct irq_data *dir_conn_data =
			irq_get_irq_data(irq_find_mapping(parent_data->domain,
						dir_conn_irq));

		if (dir_conn_data && dir_conn_data->chip->irq_unmask)
			dir_conn_data->chip->irq_unmask(dir_conn_data);
	}
	if (parent_data->chip->irq_unmask)
		parent_data->chip->irq_unmask(parent_data);
}

static void msm_dirconn_irq_ack(struct irq_data *d)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);

	if (parent_data->chip->irq_ack)
		parent_data->chip->irq_ack(parent_data);
}

static void msm_dirconn_irq_eoi(struct irq_data *d)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);

	if (parent_data->chip->irq_eoi)
		parent_data->chip->irq_eoi(parent_data);
}

static int msm_dirconn_irq_set_affinity(struct irq_data *d,
		const struct cpumask *maskval, bool force)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);

	if (!parent_data)
		return 0;

	if (parent_data->chip->irq_set_affinity)
		return parent_data->chip->irq_set_affinity(parent_data,
				maskval, force);
	return 0;
}

static int msm_dirconn_irq_set_vcpu_affinity(struct irq_data *d,
		void *vcpu_info)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);

	if (parent_data->chip->irq_set_vcpu_affinity)
		return parent_data->chip->irq_set_vcpu_affinity(parent_data,
				vcpu_info);
	return 0;
}

static void msm_dirconn_cfg_reg(struct irq_data *d, u32 offset)
{
	u32 val = 0;
	const struct msm_pingroup *g;
	unsigned long flags;
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&pctrl->lock, flags);
	g = &pctrl->soc->groups[d->hwirq];

	val = (d->hwirq) & 0xFF;

	writel_relaxed(val, pctrl->regs + g->dir_conn_reg + (offset * 4));

	//write the dir_conn_en bit
	val = readl_relaxed(pctrl->regs + g->intr_cfg_reg);
	val |= BIT(g->dir_conn_en_bit);
	writel_relaxed(val, pctrl->regs + g->intr_cfg_reg);
	raw_spin_unlock_irqrestore(&pctrl->lock, flags);
}

static void msm_dirconn_uncfg_reg(struct irq_data *d, u32 offset)
{
	const struct msm_pingroup *g;
	unsigned long flags;
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&pctrl->lock, flags);
	g = &pctrl->soc->groups[d->hwirq];

	writel_relaxed(BIT(8), pctrl->regs + g->dir_conn_reg + (offset * 4));
	raw_spin_unlock_irqrestore(&pctrl->lock, flags);
}

static int select_dir_conn_mux(struct irq_data *d, irq_hw_number_t *irq)
{
	struct msm_dir_conn *dc = NULL;
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	int i;

	if (!parent_data)
		return -EINVAL;

	for (i = 0; i < pctrl->soc->n_dir_conns; i++) {
		struct msm_dir_conn *dir_conn =
			(struct msm_dir_conn *)&pctrl->soc->dir_conn[i];

		/* Check if there is already mux assigned for this gpio */
		if (dir_conn->gpio == d->hwirq && (dir_conn->hwirq + 32) !=
				parent_data->hwirq) {
			*irq = dir_conn->hwirq + 32;
			return pctrl->soc->dir_conn_irq_base - dir_conn->hwirq;
		}

		if (dir_conn->gpio)
			continue;

		/* Use the first unused direct connect available */
		dc = dir_conn;
		break;
	}

	if (dc) {
		*irq = dc->hwirq + 32;
		dc->gpio = (u32)d->hwirq;
		return pctrl->soc->dir_conn_irq_base - (u32)dc->hwirq;
	}

	pr_err("%s: No direct connects available for interrupt %lu\n",
				__func__, d->hwirq);
	return -EINVAL;
}

static void add_dirconn_tlmm(struct irq_data *d, irq_hw_number_t irq)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);
	struct irq_data *dir_conn_data = NULL;
	int offset = 0;
	unsigned int virt = 0;

	offset = select_dir_conn_mux(d, &irq);
	if (offset < 0 || !parent_data)
		return;

	virt = irq_find_mapping(parent_data->domain, irq);
	msm_dirconn_cfg_reg(d, offset);
	irq_set_handler_data(virt, d);
	desc = irq_to_desc(virt);
	if (!desc)
		return;

	dir_conn_data = &(desc->irq_data);

	if (dir_conn_data) {
		if (dir_conn_data->chip && dir_conn_data->chip->irq_set_type)
			dir_conn_data->chip->irq_set_type(dir_conn_data,
					IRQ_TYPE_EDGE_RISING);
		if (dir_conn_data->chip && dir_conn_data->chip->irq_unmask)
			dir_conn_data->chip->irq_unmask(dir_conn_data);
	}
}

static void remove_dirconn_tlmm(struct irq_data *d, irq_hw_number_t irq)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);
	struct irq_data *dir_conn_data = NULL;
	int offset = 0;
	unsigned int virt = 0;

	if (!parent_data)
		return;

	virt = irq_find_mapping(parent_data->domain, irq);
	msm_dirconn_uncfg_reg(d, offset);
	irq_set_handler_data(virt, NULL);
	desc = irq_to_desc(virt);
	if (!desc)
		return;

	dir_conn_data = &(desc->irq_data);

	if (dir_conn_data) {
		if (dir_conn_data->chip && dir_conn_data->chip->irq_mask)
			dir_conn_data->chip->irq_mask(dir_conn_data);
	}
}

static int msm_dirconn_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct irq_desc *desc = irq_data_to_desc(d);
	struct irq_data *parent_data = irq_get_irq_data(desc->parent_irq);
	irq_hw_number_t irq = 0;

	if (!parent_data)
		return 0;

	if (type == IRQ_TYPE_EDGE_BOTH) {
		add_dirconn_tlmm(d, irq);
	} else {
		if (is_gpio_dual_edge(d, &irq))
			remove_dirconn_tlmm(d, irq);
	}

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH))
		irq_set_handler_locked(d, handle_level_irq);
	else if (type & (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING))
		irq_set_handler_locked(d, handle_edge_irq);

	if (parent_data->chip->irq_set_type)
		return parent_data->chip->irq_set_type(parent_data, type);

	return 0;
}

static struct irq_chip msm_dirconn_irq_chip = {
	.name			= "msmgpio-dc",
	.irq_mask		= msm_dirconn_irq_mask,
	.irq_enable		= msm_dirconn_irq_enable,
	.irq_unmask		= msm_dirconn_irq_unmask,
	.irq_eoi		= msm_dirconn_irq_eoi,
	.irq_ack		= msm_dirconn_irq_ack,
	.irq_set_type		= msm_dirconn_irq_set_type,
	.irq_set_affinity	= msm_dirconn_irq_set_affinity,
	.irq_set_vcpu_affinity	= msm_dirconn_irq_set_vcpu_affinity,
	.flags			= IRQCHIP_SKIP_SET_WAKE
					| IRQCHIP_MASK_ON_SUSPEND
					| IRQCHIP_SET_TYPE_MASKED,
};

static void msm_gpio_irq_handler(struct irq_desc *desc)
{
	struct gpio_chip *gc = irq_desc_get_handler_data(desc);
	const struct msm_pingroup *g;
	struct msm_pinctrl *pctrl = gpiochip_get_data(gc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int irq_pin;
	int handled = 0;
	u32 val;
	int i;

	chained_irq_enter(chip, desc);

	/*
	 * Each pin has it's own IRQ status register, so use
	 * enabled_irq bitmap to limit the number of reads.
	 */
	for_each_set_bit(i, pctrl->enabled_irqs, pctrl->chip.ngpio) {
		g = &pctrl->soc->groups[i];
		val = readl(pctrl->regs + g->intr_status_reg);
		if (val & BIT(g->intr_status_bit)) {
			irq_pin = irq_find_mapping(gc->irqdomain, i);
			generic_handle_irq(irq_pin);
			handled++;
		}
	}

	/* No interrupts were flagged */
	if (handled == 0)
		handle_bad_irq(desc);

	chained_irq_exit(chip, desc);
}

static void msm_gpio_dirconn_handler(struct irq_desc *desc)
{
	struct irq_data *irqd = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);

	chained_irq_enter(chip, desc);
	generic_handle_irq(irqd->irq);
	chained_irq_exit(chip, desc);
}

static void msm_gpio_setup_dir_connects(struct msm_pinctrl *pctrl)
{
	struct device_node *parent_node;
	struct irq_domain *parent_domain;
	struct irq_fwspec fwspec;
	unsigned int i;

	parent_node = of_irq_find_parent(pctrl->dev->of_node);

	if (!parent_node)
		return;

	parent_domain = irq_find_host(parent_node);
	if (!parent_domain)
		return;

	fwspec.fwnode = parent_domain->fwnode;
	for (i = 0; i < pctrl->soc->n_dir_conns; i++) {
		const struct msm_dir_conn *dirconn = &pctrl->soc->dir_conn[i];
		unsigned int parent_irq;
		int irq;

		fwspec.param[0] = 0; /* SPI */
		fwspec.param[1] = dirconn->hwirq;
		fwspec.param[2] = IRQ_TYPE_NONE;
		fwspec.param_count = 3;
		parent_irq = irq_create_fwspec_mapping(&fwspec);

		if (dirconn->gpio != 0) {
			irq = irq_create_mapping(pctrl->chip.irqdomain,
					dirconn->gpio);

			irq_set_parent(irq, parent_irq);
			irq_set_chip(irq, &msm_dirconn_irq_chip);
			__irq_set_handler(parent_irq, msm_gpio_dirconn_handler,
				false, NULL);
			irq_set_handler_data(parent_irq, irq_get_irq_data(irq));
		} else {
			__irq_set_handler(parent_irq, msm_gpio_dirconn_handler,
				false, NULL);
		}
	}
}

static int msm_gpiochip_to_irq(struct gpio_chip *chip, unsigned int offset)
{
	struct irq_fwspec fwspec;

	fwspec.fwnode = of_node_to_fwnode(chip->of_node);
	fwspec.param[0] = offset;
	fwspec.param[1] = IRQ_TYPE_NONE;
	fwspec.param_count = 2;

	return irq_create_fwspec_mapping(&fwspec);
}

static int msm_gpio_init(struct msm_pinctrl *pctrl)
{
	struct gpio_chip *chip;
	int ret;
	unsigned ngpio = pctrl->soc->ngpios;
	struct device_node *irq_parent = NULL;
	struct irq_domain *domain_parent;

	if (WARN_ON(ngpio > MAX_NR_GPIO))
		return -EINVAL;

	chip = &pctrl->chip;
	chip->base = 0;
	chip->ngpio = ngpio;
	chip->label = dev_name(pctrl->dev);
	chip->parent = pctrl->dev;
	chip->owner = THIS_MODULE;
	chip->of_node = pctrl->dev->of_node;

	ret = gpiochip_add_data(&pctrl->chip, pctrl);
	if (ret) {
		dev_err(pctrl->dev, "Failed register gpiochip\n");
		return ret;
	}

	ret = gpiochip_add_pin_range(&pctrl->chip, dev_name(pctrl->dev), 0, 0, chip->ngpio);
	if (ret) {
		dev_err(pctrl->dev, "Failed to add pin range\n");
		gpiochip_remove(&pctrl->chip);
		return ret;
	}

	irq_parent = of_irq_find_parent(chip->of_node);
	if (of_device_is_compatible(irq_parent, "qcom,mpm-gpio")) {
		chip->irqchip = &msm_gpio_irq_chip;
		chip->irq_handler = handle_fasteoi_irq;
		chip->irq_default_type = IRQ_TYPE_NONE;
		chip->to_irq = msm_gpiochip_to_irq;
		chip->lock_key = NULL;
		domain_parent = irq_find_host(irq_parent);
		if (!domain_parent) {
			pr_err("unable to find parent domain\n");
			gpiochip_remove(&pctrl->chip);
			return -ENXIO;
		}

		chip->irqdomain = irq_domain_add_hierarchy(domain_parent, 0,
							chip->ngpio,
							chip->of_node,
							&msm_gpio_domain_ops,
							chip);
		if (!chip->irqdomain) {
			dev_err(pctrl->dev, "Failed to add irqchip to gpiochip\n");
			chip->irqchip = NULL;
			gpiochip_remove(&pctrl->chip);
			return -ENXIO;
		}
	} else {
		ret = gpiochip_irqchip_add(chip,
					&msm_gpio_irq_chip,
					0,
					handle_fasteoi_irq,
					IRQ_TYPE_NONE);
		if (ret) {
			dev_err(pctrl->dev, "Failed to add irqchip to gpiochip\n");
			gpiochip_remove(&pctrl->chip);
			return ret;
		}
	}
	gpiochip_set_chained_irqchip(chip, &msm_gpio_irq_chip,
				pctrl->irq, msm_gpio_irq_handler);

	msm_gpio_setup_dir_connects(pctrl);
	return 0;
}

static int msm_ps_hold_restart(struct notifier_block *nb, unsigned long action,
			       void *data)
{
	struct msm_pinctrl *pctrl = container_of(nb, struct msm_pinctrl, restart_nb);

	writel(0, pctrl->regs + PS_HOLD_OFFSET);
	mdelay(1000);
	return NOTIFY_DONE;
}

static struct msm_pinctrl *poweroff_pctrl;

static void msm_ps_hold_poweroff(void)
{
	msm_ps_hold_restart(&poweroff_pctrl->restart_nb, 0, NULL);
}

static void msm_pinctrl_setup_pm_reset(struct msm_pinctrl *pctrl)
{
	int i;
	const struct msm_function *func = pctrl->soc->functions;

	for (i = 0; i < pctrl->soc->nfunctions; i++)
		if (!strcmp(func[i].name, "ps_hold")) {
			pctrl->restart_nb.notifier_call = msm_ps_hold_restart;
			pctrl->restart_nb.priority = 128;
			if (register_restart_handler(&pctrl->restart_nb))
				dev_err(pctrl->dev,
					"failed to setup restart handler.\n");
			poweroff_pctrl = pctrl;
			pm_power_off = msm_ps_hold_poweroff;
			break;
		}
}

int msm_pinctrl_probe(struct platform_device *pdev,
		      const struct msm_pinctrl_soc_data *soc_data)
{
	struct msm_pinctrl *pctrl;
	struct resource *res;
	int ret;

	pctrl = devm_kzalloc(&pdev->dev, sizeof(*pctrl), GFP_KERNEL);
	if (!pctrl) {
		dev_err(&pdev->dev, "Can't allocate msm_pinctrl\n");
		return -ENOMEM;
	}
	pctrl->dev = &pdev->dev;
	pctrl->soc = soc_data;
	pctrl->chip = msm_gpio_template;

	raw_spin_lock_init(&pctrl->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pctrl->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pctrl->regs))
		return PTR_ERR(pctrl->regs);

	msm_pinctrl_setup_pm_reset(pctrl);

	pctrl->irq = platform_get_irq(pdev, 0);
	if (pctrl->irq < 0) {
		dev_err(&pdev->dev, "No interrupt defined for msmgpio\n");
		return pctrl->irq;
	}

	msm_pinctrl_desc.name = dev_name(&pdev->dev);
	msm_pinctrl_desc.pins = pctrl->soc->pins;
	msm_pinctrl_desc.npins = pctrl->soc->npins;
	pctrl->pctrl = devm_pinctrl_register(&pdev->dev, &msm_pinctrl_desc,
					     pctrl);
	if (IS_ERR(pctrl->pctrl)) {
		dev_err(&pdev->dev, "Couldn't register pinctrl driver\n");
		return PTR_ERR(pctrl->pctrl);
	}

	ret = msm_gpio_init(pctrl);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pctrl);

	dev_dbg(&pdev->dev, "Probed Qualcomm pinctrl driver\n");

	return 0;
}
EXPORT_SYMBOL(msm_pinctrl_probe);

int msm_pinctrl_remove(struct platform_device *pdev)
{
	struct msm_pinctrl *pctrl = platform_get_drvdata(pdev);

	gpiochip_remove(&pctrl->chip);

	unregister_restart_handler(&pctrl->restart_nb);

	return 0;
}
EXPORT_SYMBOL(msm_pinctrl_remove);

