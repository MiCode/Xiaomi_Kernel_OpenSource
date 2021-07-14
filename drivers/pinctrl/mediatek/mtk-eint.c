// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2014-2018 MediaTek Inc.

/*
 * Library for MediaTek External Interrupt Support
 *
 * Author: Maoguang Meng <maoguang.meng@mediatek.com>
 *	   Sean Wang <sean.wang@mediatek.com>
 *	   Po-Kai Chi <pk.chi@mediatek.com>
 *
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#include "mtk-eint.h"

#define MTK_EINT_EDGE_SENSITIVE           0
#define MTK_EINT_LEVEL_SENSITIVE          1
#define MTK_EINT_DBNC_SET_DBNC_BITS	  4
#define MTK_EINT_DBNC_RST_BIT		  (0x1 << 1)
#define MTK_EINT_DBNC_SET_EN		  (0x1 << 0)

#define MTK_EINT_NO_OFFSET		  0

static struct mtk_eint *global_eintc;

static const struct mtk_eint_regs mtk_generic_eint_regs = {
	.stat      = 0x000,
	.ack       = 0x040,
	.mask      = 0x080,
	.mask_set  = 0x0c0,
	.mask_clr  = 0x100,
	.sens      = 0x140,
	.sens_set  = 0x180,
	.sens_clr  = 0x1c0,
	.soft      = 0x200,
	.soft_set  = 0x240,
	.soft_clr  = 0x280,
	.pol       = 0x300,
	.pol_set   = 0x340,
	.pol_clr   = 0x380,
	.dom_en    = 0x400,
	.dbnc_ctrl = 0x500,
	.dbnc_set  = 0x600,
	.dbnc_clr  = 0x700,
	.raw_stat  = 0xa00,
};

/*
 * Return the iomem of specific register offset and decode the coordinate
 * (instance, index) from global eint number.
 * If return NULL, then it must be either out-of-range or do-not-support.
 */
static void __iomem *mtk_eint_get_offset(struct mtk_eint *eint,
					 unsigned int eint_num,
					 unsigned int offset,
					 unsigned int *instance,
					 unsigned int *index)
{
	void __iomem *reg;

	if (eint_num >= eint->total_pin_number ||
	    !eint->pins[eint_num].enabled) {
		WARN_ON(1);
		return NULL;
	}

	*instance = eint->pins[eint_num].instance;
	*index = eint->pins[eint_num].index;
	reg = eint->instances[*instance].base + offset + (*index / 32 * 4);

	return reg;
}

/*
 * Generate helper function to access property register of a dedicate pin.
 */
#define DEFINE_EINT_GET_FUNCTION(_NAME, _OFFSET) \
static unsigned int mtk_eint_get_##_NAME(struct mtk_eint *eint, \
				   unsigned int eint_num) \
{ \
	unsigned int instance, index; \
	void __iomem *reg = mtk_eint_get_offset(eint, eint_num, \
						_OFFSET, \
						&instance, &index); \
	unsigned int bit = BIT(index & 0x1f);\
\
	if (!reg) { \
		dev_err(eint->dev, "%s invalid eint_num %d\n", \
			__func__, eint_num); \
		return 0;\
	} \
\
	return !!(readl(reg) & bit); \
}

DEFINE_EINT_GET_FUNCTION(stat, eint->comp->regs->stat);
DEFINE_EINT_GET_FUNCTION(mask, eint->comp->regs->mask);
DEFINE_EINT_GET_FUNCTION(sens, eint->comp->regs->sens);
DEFINE_EINT_GET_FUNCTION(pol, eint->comp->regs->pol);
DEFINE_EINT_GET_FUNCTION(soft, eint->comp->regs->soft);
DEFINE_EINT_GET_FUNCTION(raw_stat, eint->comp->regs->raw_stat);

static unsigned int mtk_eint_can_en_debounce(struct mtk_eint *eint,
					     unsigned int eint_num)
{
	unsigned int sens;
	unsigned int instance, index;
	void __iomem *reg = mtk_eint_get_offset(eint, eint_num,
						eint->comp->regs->sens,
						&instance, &index);
	unsigned int bit = BIT(index & 0x1f);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return 0;
	}

	if (readl(reg) & bit)
		sens = MTK_EINT_LEVEL_SENSITIVE;
	else
		sens = MTK_EINT_EDGE_SENSITIVE;

	if (eint->pins[eint_num].debounce &&
	    sens != MTK_EINT_EDGE_SENSITIVE)
		return 1;
	else
		return 0;
}

static int mtk_eint_flip_edge(struct mtk_eint *eint, int eint_num)
{
	int start_level, curr_level;
	unsigned int reg_offset;
	unsigned int instance, index, mask, port;
	void __iomem *reg;

	reg = mtk_eint_get_offset(eint, eint_num, MTK_EINT_NO_OFFSET,
				  &instance, &index);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return 0;
	}

	mask = BIT(index & 0x1f);
	port = index >> 5;
	reg = eint->instances[instance].base + port * 4;

	curr_level = eint->gpio_xlate->get_gpio_state(eint->pctl, eint_num);

	do {
		start_level = curr_level;
		if (start_level)
			reg_offset = eint->comp->regs->pol_clr;
		else
			reg_offset = eint->comp->regs->pol_set;

		writel(mask, reg + reg_offset);

		curr_level = eint->gpio_xlate->get_gpio_state(eint->pctl,
							      eint_num);
	} while (start_level != curr_level);

	return start_level;
}

static void mtk_eint_mask(struct irq_data *d)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	unsigned int instance, index;
	void __iomem *reg = mtk_eint_get_offset(eint, d->hwirq,
						eint->comp->regs->mask_set,
						&instance, &index);
	u32 mask = BIT(index & 0x1f);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, d->hwirq);
		return;
	}

	eint->instances[instance].cur_mask[index >> 5] &= ~mask;

	writel(mask, reg);
}

static void mtk_eint_unmask(struct irq_data *d)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	unsigned int instance, index;
	void __iomem *reg = mtk_eint_get_offset(eint, d->hwirq,
						eint->comp->regs->mask_clr,
						&instance, &index);
	u32 mask = BIT(index & 0x1f);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, d->hwirq);
		return;
	}

	eint->instances[instance].cur_mask[index >> 5] |= mask;

	writel(mask, reg);

	if (eint->pins[d->hwirq].dual_edge)
		mtk_eint_flip_edge(eint, d->hwirq);
}

/*
 * We need to do extra effort to clear edge-triggered EINT
 * which located in eint_c due to hw design limitation.
 */
void mt6983_eint_ack(struct irq_data *d)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	unsigned int instance, index;
	void __iomem *sens_reg,
		     *ack_reg = mtk_eint_get_offset(eint, d->hwirq,
						eint->comp->regs->sens,
						&instance, &index);
	unsigned int bit = BIT(index & 0x1f);

	if (!ack_reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, d->hwirq);
		return;
	}

	if (instance == 4) {
		sens_reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->sens_clr,
					  &instance, &index);
		writel(bit, sens_reg);
		sens_reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->sens_set,
					  &instance, &index);
		writel(bit, sens_reg);
	} else
		writel(bit, ack_reg);
}

static void mtk_eint_ack(struct irq_data *d)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	unsigned int instance, index;
	void __iomem *reg;
	unsigned int bit;

	if (eint->comp->ops.ack)
		eint->comp->ops.ack(d);
	else {
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->ack,
					  &instance, &index);
		bit = BIT(index & 0x1f);
		if (!reg) {
			dev_err(eint->dev, "%s invalid eint_num %d\n",
				__func__, d->hwirq);
			return;
		}

		writel(bit, reg);
	}
}

static void mtk_eint_soft_set(struct mtk_eint *eint,
				      unsigned int eint_num)
{
	unsigned int instance, index;
	void __iomem *reg = mtk_eint_get_offset(eint, eint_num,
						eint->comp->regs->soft_set,
						&instance, &index);
	unsigned int bit = BIT(index & 0x1f);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return;
	}

	writel(bit, reg);
}

static void mtk_eint_soft_clr(struct mtk_eint *eint,
				      unsigned int eint_num)
{
	unsigned int instance, index;
	void __iomem *reg = mtk_eint_get_offset(eint, eint_num,
						eint->comp->regs->soft_clr,
						&instance, &index);
	unsigned int bit = BIT(index & 0x1f);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return;
	}

	writel(bit, reg);
}

static int mtk_eint_set_type(struct irq_data *d, unsigned int type)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	u32 mask;
	unsigned int instance, index;
	void __iomem *reg;

	if (((type & IRQ_TYPE_EDGE_BOTH) && (type & IRQ_TYPE_LEVEL_MASK)) ||
	    ((type & IRQ_TYPE_LEVEL_MASK) == IRQ_TYPE_LEVEL_MASK)) {
		dev_err(eint->dev,
			"Can't configure IRQ%d (EINT%lu) for type 0x%X\n",
			d->irq, d->hwirq, type);
		return -EINVAL;
	}

	if ((type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH)
		eint->pins[d->hwirq].dual_edge = 1;
	else
		eint->pins[d->hwirq].dual_edge = 0;

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING))
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->pol_clr,
					  &instance, &index);
	else
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->pol_set,
					  &instance, &index);

	mask = BIT(index & 0x1f);
	writel(mask, reg);

	if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING))
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->sens_clr,
					  &instance, &index);
	else
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->sens_set,
					  &instance, &index);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, d->hwirq);
		return 0;
	}

	mask = BIT(index & 0x1f);
	writel(mask, reg);

	if (eint->pins[d->hwirq].dual_edge)
		mtk_eint_flip_edge(eint, d->hwirq);

	return 0;
}

static int mtk_eint_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	unsigned int instance, index, shift, port;
	void __iomem *reg = mtk_eint_get_offset(eint, d->hwirq,
						MTK_EINT_NO_OFFSET,
						&instance, &index);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, d->hwirq);
		return 0;
	}

	shift = index & 0x1f;
	port = index >> 5;

	if (on)
		eint->instances[instance].wake_mask[port] |= BIT(shift);
	else
		eint->instances[instance].wake_mask[port] &= ~BIT(shift);

	return 0;
}

static int mtk_eint_irq_request_resources(struct irq_data *d)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	struct gpio_chip *gpio_c;
	unsigned int gpio_n;
	int err;

	err = eint->gpio_xlate->get_gpio_n(eint->pctl, d->hwirq,
					   &gpio_n, &gpio_c);
	if (err < 0) {
		dev_err(eint->dev, "Can not find pin\n");
		goto err_out;
	}

	err = gpiochip_lock_as_irq(gpio_c, gpio_n);
	if (err < 0) {
		dev_err(eint->dev, "unable to lock HW IRQ %lu for IRQ\n",
			irqd_to_hwirq(d));
		goto err_out;
	}

	err = eint->gpio_xlate->set_gpio_as_eint(eint->pctl, d->hwirq);
	if (err < 0) {
		dev_err(eint->dev, "Can not eint mode\n");
		goto err_out;
	}

	return 0;
err_out:
	return err;
}

static void mtk_eint_irq_release_resources(struct irq_data *d)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	struct gpio_chip *gpio_c;
	unsigned int gpio_n;

	eint->gpio_xlate->get_gpio_n(eint->pctl, d->hwirq, &gpio_n,
				     &gpio_c);

	gpiochip_unlock_as_irq(gpio_c, gpio_n);
}

static struct irq_chip mtk_eint_irq_chip = {
	.name = "mtk-eint",
	.irq_disable = mtk_eint_mask,
	.irq_mask = mtk_eint_mask,
	.irq_unmask = mtk_eint_unmask,
	.irq_ack = mtk_eint_ack,
	.irq_set_type = mtk_eint_set_type,
	.irq_set_wake = mtk_eint_irq_set_wake,
	.irq_request_resources = mtk_eint_irq_request_resources,
	.irq_release_resources = mtk_eint_irq_release_resources,
};

/*
 * Configure all EINT pins as domain 0, which only belongs to AP.
 */
static unsigned int mtk_eint_hw_init(struct mtk_eint *eint)
{
	void __iomem *reg;
	unsigned int i, j;

	for (i = 0; i < eint->instance_number; i++) {
		reg = eint->instances[i].base + eint->comp->regs->dom_en;
		for (j = 0; j < eint->instances[i].number; j += 32) {
			writel(0xffffffff, reg);
			reg += 4;
		}
	}

	return 0;
}

static inline void
mtk_eint_debounce_process(struct mtk_eint *eint, int eint_num)
{
	unsigned int rst, ctrl_offset;
	unsigned int bit, dbnc;
	unsigned int instance, index;
	void __iomem *reg;

	reg = mtk_eint_get_offset(eint, eint_num, MTK_EINT_NO_OFFSET,
				  &instance, &index);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return;
	}

	ctrl_offset = (index / 4) * 4 + eint->comp->regs->dbnc_ctrl;
	dbnc = readl(eint->instances[instance].base + ctrl_offset);
	bit = MTK_EINT_DBNC_SET_EN << ((index % 4) * 8);

	if ((bit & dbnc) > 0) {
		ctrl_offset = (index / 4) * 4 + eint->comp->regs->dbnc_set;
		rst = MTK_EINT_DBNC_RST_BIT << ((index % 4) * 8);
		writel(rst, eint->instances[instance].base + ctrl_offset);
	}
}

static void mtk_eint_irq_handler(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct mtk_eint *eint = irq_desc_get_handler_data(desc);
	unsigned int status, i, j;
	int shift, port, eint_num, virq;
	unsigned int dual_edge, start_level, curr_level;
	struct mtk_eint_instance eint_instance;
	void __iomem *addr;

	chained_irq_enter(chip, desc);

	for (i = 0; i < eint->instance_number; i++) {
		eint_instance = eint->instances[i];

		/* Iterate all pins by port */
		for (j = 0; j < eint_instance.number; j += 32) {
			port = j >> 5;
			status = readl(eint_instance.base + port * 4 +
				       eint->comp->regs->stat);
			while (status) {
				shift = __ffs(status);
				status &= ~BIT(shift);

				eint_num = eint->instances[i].pin_list[shift + j];
				virq = irq_find_mapping(eint->domain, eint_num);

				/*
				 * If we get an interrupt on pin that was only required
				 * for wake (but no real interrupt requested), mask the
				 * interrupt (as would mtk_eint_resume do anyway later
				 * in the resume sequence).
				 */
				if (eint->instances[i].wake_mask[port] & BIT(shift) &&
				    !(eint->instances[i].cur_mask[port] & BIT(shift))) {
					addr = eint_instance.base + port * 4 +
						eint->comp->regs->mask_set;
					writel_relaxed(BIT(shift), addr);
				}

				dual_edge = eint->pins[eint_num].dual_edge;
				if (dual_edge) {
					/*
					 * Clear soft-irq in case we raised it last
					 * time.
					 */
					mtk_eint_soft_clr(eint, eint_num);

					start_level =
					eint->gpio_xlate->get_gpio_state(eint->pctl,
									 eint_num);
				}

				generic_handle_irq(virq);

				if (dual_edge) {
					curr_level = mtk_eint_flip_edge(eint, eint_num);

					/*
					 * If level changed, we might lost one edge
					 * interrupt, raised it through soft-irq.
					 */
					if (start_level != curr_level)
						mtk_eint_soft_set(eint, eint_num);
				}

				if (eint->pins[eint_num].debounce)
					mtk_eint_debounce_process(eint, eint_num);

			}
		}
	}
	chained_irq_exit(chip, desc);
}

int mtk_eint_do_suspend(struct mtk_eint *eint)
{
	unsigned int i, j, port;

	for (i = 0; i < eint->instance_number; i++) {
		struct mtk_eint_instance inst = eint->instances[i];

		for (j = 0; j < inst.number; j += 32) {
			port = j >> 5;
			writel_relaxed(~inst.wake_mask[port],
				       inst.base + port*4 + eint->comp->regs->mask_set);
			writel_relaxed(inst.wake_mask[port],
				       inst.base + port*4 + eint->comp->regs->mask_clr);
		}
	}
	dsb(sy);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_eint_do_suspend);

int mtk_eint_do_resume(struct mtk_eint *eint)
{
	unsigned int i, j, port;

	for (i = 0; i < eint->instance_number; i++) {
		struct mtk_eint_instance inst = eint->instances[i];

		for (j = 0; j < inst.number; j += 32) {
			port = j >> 5;
			writel_relaxed(~inst.cur_mask[port],
				       inst.base + port*4 + eint->comp->regs->mask_set);
			writel_relaxed(inst.cur_mask[port],
				       inst.base + port*4 + eint->comp->regs->mask_clr);
		}
	}
	dsb(sy);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_eint_do_resume);

int mtk_eint_set_debounce(struct mtk_eint *eint, unsigned long eint_num,
			  unsigned int debounce)
{
	int virq, eint_offset;
	unsigned int set_offset, bit, clr_bit, clr_offset, rst, i, unmask,
		     dbnc;
	static const unsigned int debounce_time[] = { 156, 313, 625, 1250,
		20000, 40000, 80000, 160000, 320000, 640000 };
	struct irq_data *d;
	unsigned int instance, index;
	void __iomem *reg;

	/*
	 * Due to different number of bit field, we only decode
	 * the coordinate here, instead of get the VA.
	 */
	reg = mtk_eint_get_offset(eint, eint_num, MTK_EINT_NO_OFFSET,
				  &instance, &index);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return 0;
	}

	virq = irq_find_mapping(eint->domain, eint_num);
	eint_offset = (index % 4) * 8;
	d = irq_get_irq_data(virq);

	reg = eint->instances[instance].base;
	set_offset = (index / 4) * 4 + eint->comp->regs->dbnc_set;
	clr_offset = (index / 4) * 4 + eint->comp->regs->dbnc_clr;

	if (!mtk_eint_can_en_debounce(eint, eint_num))
		return -EINVAL;

	/*
	 * Check eint number to avoid access out-of-range
	 */
	dbnc = ARRAY_SIZE(debounce_time) - 1;
	for (i = 0; i < ARRAY_SIZE(debounce_time); i++) {
		if (debounce <= debounce_time[i]) {
			dbnc = i;
			break;
		}
	}

	if (!mtk_eint_get_mask(eint, eint_num)) {
		mtk_eint_mask(d);
		unmask = 1;
	} else
		unmask = 0;

	clr_bit = 0xff << eint_offset;
	writel(clr_bit, reg + clr_offset);

	bit = ((dbnc << MTK_EINT_DBNC_SET_DBNC_BITS)
		| MTK_EINT_DBNC_SET_EN) << eint_offset;
	rst = MTK_EINT_DBNC_RST_BIT << eint_offset;
	writel(rst | bit, reg + set_offset);

	/*
	 * Delay should be (8T @ 32k) + de-bounce count-down time
	 * from dbc rst to work correctly.
	 */
	udelay(debounce_time[dbnc] + 250);
	if (unmask == 1)
		mtk_eint_unmask(d);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_eint_set_debounce);

unsigned int mtk_eint_get_debounce_en(struct mtk_eint *eint,
				      unsigned int eint_num)
{
	unsigned int instance, index, bit;
	void __iomem *reg;

	reg = mtk_eint_get_offset(eint, eint_num, MTK_EINT_NO_OFFSET,
				  &instance, &index);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return 0;
	}

	reg = eint->instances[instance].base +
		(index / 4) * 4 + eint->comp->regs->dbnc_ctrl;

	bit = MTK_EINT_DBNC_SET_EN << ((index % 4) * 8);

	return (readl(reg) & bit) ? 1 : 0;
}

unsigned int mtk_eint_get_debounce_value(struct mtk_eint *eint,
					   unsigned int eint_num)
{
	unsigned int instance, index, mask, offset;
	void __iomem *reg;

	reg = mtk_eint_get_offset(eint, eint_num, MTK_EINT_NO_OFFSET,
				  &instance, &index);

	if (!reg) {
		dev_err(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return 0;
	}

	reg = eint->instances[instance].base +
		(index / 4) * 4 + eint->comp->regs->dbnc_ctrl;

	offset = MTK_EINT_DBNC_SET_DBNC_BITS + ((index % 4) * 8);
	mask = 0xf << offset;

	return ((readl(reg) & mask) >> offset);
}

int mtk_eint_find_irq(struct mtk_eint *eint, unsigned long eint_n)
{
	int irq;

	irq = irq_find_mapping(eint->domain, eint_n);
	if (!irq)
		return -EINVAL;

	return irq;
}
EXPORT_SYMBOL_GPL(mtk_eint_find_irq);

/*
 * Dump the properties/states of the specific EINT pin.
 * @eint_num: the global EINT number.
 * @buf: the pointer of a string buffer.
 * @buf_size: the size of the buffer.
 *
 * If the return value < 0, it means that the @eint_num is invalid;
 * Otherwise, return 0;
 */
int dump_eint_pin_status(unsigned int eint_num, char *buf, unsigned int buf_size)
{
	unsigned int len = 0, enabled, stat, raw_stat, soft, mask, sens, pol,
		     deb_en, deb_val;

	if (eint_num < 0 || eint_num >= global_eintc->total_pin_number)
		return -ENODEV;

	enabled = global_eintc->pins[eint_num].enabled;
	stat = mtk_eint_get_stat(global_eintc, eint_num);
	raw_stat = mtk_eint_get_raw_stat(global_eintc, eint_num);
	soft = mtk_eint_get_soft(global_eintc, eint_num);
	mask = mtk_eint_get_mask(global_eintc, eint_num);
	sens = mtk_eint_get_sens(global_eintc, eint_num);
	pol = mtk_eint_get_pol(global_eintc, eint_num);

	len += snprintf(buf + len, buf_size - len,
			"%s=%u(%s)\n%s=%s_%s\n%s=%u\n%s=%u\n%s=%u\n%s=%u\n",
			"Pin", eint_num, enabled ? "enabled" : "disabled",
			"Type", (sens == 1) ? "level" : "edge",
			(pol == 1) ? "high" : "low",
			"Pending", stat,
			"Raw", raw_stat,
			"Soft", soft,
			"Mask", mask);

	if (mtk_eint_can_en_debounce(global_eintc, eint_num)) {
		deb_en	= mtk_eint_get_debounce_en(global_eintc, eint_num);
		deb_val = mtk_eint_get_debounce_value(global_eintc, eint_num);

		len += snprintf(buf + len, buf_size - len,
				"Support debounce, %s=%u, %s=%u\n",
				"enable", deb_en,
				"setting", deb_val);
	} else
		len += snprintf(buf + len, buf_size - len,
				"Not support debounce\n");

	return 0;
}
EXPORT_SYMBOL_GPL(dump_eint_pin_status);

static ssize_t eintc_status_show(struct device_driver *driver, char *buf)
{
	struct mtk_eint *eint = global_eintc;
	unsigned int i, j, len = 0,
		     instance_num = eint->instance_number;

	len += snprintf(buf + len, PAGE_SIZE - len, "=====EINTC Dump=====\n");

	for (i = 0; i < instance_num; i++) {
		struct mtk_eint_instance inst = eint->instances[i];

		len += snprintf(buf + len, PAGE_SIZE - len,
				"Instance %d name=%s with %u pins\n",
				i, inst.name, inst.number);

		for (j = 0; j < inst.number; j++)
			len += snprintf(buf + len, PAGE_SIZE - len,
					"%d ", inst.pin_list[j]);

		len += snprintf(buf + len, PAGE_SIZE - len,
				"\n", i, inst.pin_list[j]);
	}

	return strlen(buf);
}

static DRIVER_ATTR_RO(eintc_status);

static ssize_t eint_pin_status_show(struct device_driver *driver, char *buf)
{
	struct mtk_eint *eint = global_eintc;
	unsigned int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"=====EINT Pin Dump=====\n");

	dump_eint_pin_status(eint->dump_target_eint,
			     buf + len, PAGE_SIZE - len);

	return strlen(buf);

}

static ssize_t eint_pin_status_store(struct device_driver *driver,
				     const char *buf, size_t count)
{
	int eint_num, ret;

	ret = kstrtouint(buf, 10, &eint_num);

	if (ret || eint_num >= global_eintc->total_pin_number) {
		dev_err(global_eintc->dev,
			"%s invalid input: %s.\n", __func__, buf);
		goto err_out;
	}

	global_eintc->dump_target_eint = (unsigned int)eint_num;

err_out:
	return count;
}

static DRIVER_ATTR_RW(eint_pin_status);

static const struct mtk_eint_compatible default_compat = {
	.regs = &mtk_generic_eint_regs,
};

static const struct mtk_eint_compatible mt6983_compat = {
	.ops = {
		.ack = mt6983_eint_ack,
	},
	.regs = &mtk_generic_eint_regs,
};

static const struct of_device_id eint_compatible_ids[] = {
	{ .compatible = "mediatek,mt6983-pinctrl", .data = &mt6983_compat },
	{ }
};

int mtk_eint_do_init(struct mtk_eint *eint)
{
	int i, matrix_number = 0;
	struct device_node *node;
	unsigned int ret, size, offset;
	unsigned int id, inst, idx, support_deb;

	const phandle *ph;

#if defined(MTK_EINT_DEBUG)
	struct mtk_eint_pin pin;
#endif

	ph = of_get_property(eint->dev->of_node, "mediatek,eint", NULL);
	if (!ph) {
		dev_err(eint->dev, "Cannot find EINT phandle in PIO node.\n");
		return -ENODEV;
	}

	node = of_find_node_by_phandle(be32_to_cpup(ph));
	if (!node) {
		dev_err(eint->dev, "Cannot find EINT node by phandle.\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "mediatek,total-pin-number",
				   &eint->total_pin_number);
	if (ret) {
		dev_err(eint->dev,
		       "%s cannot read total-pin-number from device node.\n",
		       __func__);
		return -EINVAL;
	} else
		dev_info(eint->dev,
			 "%s eint total %u pins.\n", __func__, eint->total_pin_number);

	ret = of_property_read_u32(node, "mediatek,instance-num",
				   &eint->instance_number);
	if (ret)
		eint->instance_number = 1; // only 1 instance in legacy chip

	size = eint->instance_number * sizeof(struct mtk_eint_instance);
	eint->instances = devm_kzalloc(eint->dev, size, GFP_KERNEL);
	if (!eint->instances)
		return -ENOMEM;

	size = eint->total_pin_number * sizeof(struct mtk_eint_pin);
	eint->pins = devm_kzalloc(eint->dev, size, GFP_KERNEL);
	if (!eint->pins)
		return -ENOMEM;

	for (i = 0; i < eint->instance_number; i++) {
		ret = of_property_read_string_index(node, "reg-name", i,
						    &(eint->instances[i].name));
		if (ret) {
			dev_info(eint->dev,
				 "%s cannot read the name of instance %d.\n",
				 __func__, i);
		}

		eint->instances[i].base = of_iomap(node, i);
		if (!eint->instances[i].base)
			return -ENOMEM;
	}

	matrix_number = of_property_count_u32_elems(node, "mediatek,pins") / 4;
	if (matrix_number < 0) {
		matrix_number = eint->total_pin_number;
		dev_info(eint->dev, "%s eint in legacy mode, assign the matrix number to %u.\n",
			 __func__, matrix_number);
	} else
		dev_info(eint->dev, "%s eint in new mode, assign the matrix number to %u.\n",
			 __func__, matrix_number);

	for (i = 0; i < matrix_number; i++) {
		offset = i * 4;

		ret = of_property_read_u32_index(node, "mediatek,pins",
					   offset, &id);
		ret |= of_property_read_u32_index(node, "mediatek,pins",
					   offset+1, &inst);
		ret |= of_property_read_u32_index(node, "mediatek,pins",
					   offset+2, &idx);
		ret |= of_property_read_u32_index(node, "mediatek,pins",
					   offset+3, &support_deb);

		/* Legacy chip which no need to give coordinate list */
		if (ret) {
			id = i;
			inst = 0;
			idx = i;
			support_deb = (i < 32) ? 1 : 0;
		}

		eint->pins[id].enabled = true;
		eint->pins[id].instance = inst;
		eint->pins[id].index = idx;
		eint->pins[id].debounce = support_deb;

		eint->instances[inst].pin_list[idx] = id;
		eint->instances[inst].number++;

#if defined(MTK_EINT_DEBUG)
		pin = eint->pins[id];
		dev_info(eint->dev,
			 "EINT%u in (%u-%u, %u), deb = %u. %u",
			 id,
			 pin.instance,
			 eint->instances[inst].number,
			 pin.index,
			 pin.debounce,
			 eint->instances[pin.instance].pin_list[pin.index]);
#endif
	}

	for (i = 0; i < eint->instance_number; i++) {
		size = (eint->instances[i].number / 32 + 1) * sizeof(unsigned int);
		eint->instances[i].wake_mask =
			devm_kzalloc(eint->dev, size, GFP_KERNEL);
		eint->instances[i].cur_mask =
			devm_kzalloc(eint->dev, size, GFP_KERNEL);

		if (!eint->instances[i].wake_mask ||
		    !eint->instances[i].cur_mask)
			return -ENOMEM;
	}

	eint->comp = (struct mtk_eint_compatible *)
			of_device_get_match_data(eint->dev);

	if (!eint->comp)
		eint->comp = &default_compat;

	eint->irq = irq_of_parse_and_map(node, 0);
	if (!eint->irq) {
		dev_err(eint->dev,
			"%s IRQ parse fail.\n", __func__);
		return -EINVAL;
	}

	eint->domain = irq_domain_add_linear(eint->dev->of_node,
					     eint->total_pin_number,
					     &irq_domain_simple_ops, NULL);
	if (!eint->domain)
		return -ENOMEM;

	mtk_eint_hw_init(eint);
	for (i = 0; i < eint->total_pin_number; i++) {
		int virq = irq_create_mapping(eint->domain, i);

		irq_set_chip_and_handler(virq, &mtk_eint_irq_chip,
					 handle_level_irq);
		irq_set_chip_data(virq, eint);
	}

	irq_set_chained_handler_and_data(eint->irq, mtk_eint_irq_handler,
					 eint);

	ret = driver_create_file(eint->dev->driver,
				 &driver_attr_eintc_status);

	ret |= driver_create_file(eint->dev->driver,
				  &driver_attr_eint_pin_status);

	if (ret)
		dev_err(eint->dev, "%s create sysfs files failed.\n", __func__);

	global_eintc = eint;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_eint_do_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek EINT Driver");
