/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <asm/mach/irq.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include "pinctrl-msm.h"

/* config translations */
#define drv_str_to_rval(drv)	((drv >> 1) - 1)
#define rval_to_drv_str(val)	((val + 1) << 1)
#define dir_to_inout_val(dir)	(dir << 1)
#define inout_val_to_dir(val)	(val >> 1)
#define rval_to_pull(val)	((val > 2) ? 1 : val)
#define TLMMV3_NO_PULL		0
#define TLMMV3_PULL_DOWN	1
#define TLMMV3_PULL_UP		3
/* GP PIN TYPE REG MASKS */
#define TLMMV3_GP_DRV_SHFT		6
#define TLMMV3_GP_DRV_MASK		0x7
#define TLMMV3_GP_PULL_SHFT		0
#define TLMMV3_GP_PULL_MASK		0x3
#define TLMMV3_GP_DIR_SHFT		9
#define TLMMV3_GP_DIR_MASK		1
#define TLMMV3_GP_FUNC_SHFT		2
#define TLMMV3_GP_FUNC_MASK		0xF
#define GPIO_OUT_BIT			1
#define GPIO_IN_BIT			0
#define GPIO_OE_BIT			9
/* SDC1 PIN TYPE REG MASKS */
#define TLMMV3_SDC1_CLK_DRV_SHFT	6
#define TLMMV3_SDC1_CLK_DRV_MASK	0x7
#define TLMMV3_SDC1_DATA_DRV_SHFT	0
#define TLMMV3_SDC1_DATA_DRV_MASK	0x7
#define TLMMV3_SDC1_CMD_DRV_SHFT	3
#define TLMMV3_SDC1_CMD_DRV_MASK	0x7
#define TLMMV3_SDC1_CLK_PULL_SHFT	13
#define TLMMV3_SDC1_CLK_PULL_MASK	0x3
#define TLMMV3_SDC1_DATA_PULL_SHFT	9
#define TLMMV3_SDC1_DATA_PULL_MASK	0x3
#define TLMMV3_SDC1_CMD_PULL_SHFT	11
#define TLMMV3_SDC1_CMD_PULL_MASK	0x3
/* SDC2 PIN TYPE REG MASKS */
#define TLMMV3_SDC2_CLK_DRV_SHFT	6
#define TLMMV3_SDC2_CLK_DRV_MASK	0x7
#define TLMMV3_SDC2_DATA_DRV_SHFT	0
#define TLMMV3_SDC2_DATA_DRV_MASK	0x7
#define TLMMV3_SDC2_CMD_DRV_SHFT	3
#define TLMMV3_SDC2_CMD_DRV_MASK	0x7
#define TLMMV3_SDC2_CLK_PULL_SHFT	14
#define TLMMV3_SDC2_CLK_PULL_MASK	0x3
#define TLMMV3_SDC2_DATA_PULL_SHFT	9
#define TLMMV3_SDC2_DATA_PULL_MASK	0x3
#define TLMMV3_SDC2_CMD_PULL_SHFT	11
#define TLMMV3_SDC2_CMD_PULL_MASK	0x3
/* TLMM V3 IRQ REG fields */
#define INTR_ENABLE_BIT		0
#define	INTR_POL_CTL_BIT	1
#define	INTR_DECT_CTL_BIT	2
#define	INTR_RAW_STATUS_EN_BIT	4
#define	INTR_TARGET_PROC_BIT	5
#define	INTR_DIR_CONN_EN_BIT	8
#define INTR_STATUS_BIT		0
#define DC_POLARITY_BIT		8

/* Target processors for TLMM pin based interrupts */
#define INTR_TARGET_PROC_APPS(core_id)	((core_id) << INTR_TARGET_PROC_BIT)
#define TLMMV3_APPS_ID_DEFAULT	4
#define INTR_TARGET_PROC_NONE	(7 << INTR_TARGET_PROC_BIT)
/* Interrupt flag bits */
#define DC_POLARITY_HI		BIT(DC_POLARITY_BIT)
#define INTR_POL_CTL_HI		BIT(INTR_POL_CTL_BIT)
#define INTR_DECT_CTL_LEVEL	(0 << INTR_DECT_CTL_BIT)
#define INTR_DECT_CTL_POS_EDGE	(1 << INTR_DECT_CTL_BIT)
#define INTR_DECT_CTL_NEG_EDGE	(2 << INTR_DECT_CTL_BIT)
#define INTR_DECT_CTL_DUAL_EDGE	(3 << INTR_DECT_CTL_BIT)
#define INTR_DECT_CTL_MASK	(3 << INTR_DECT_CTL_BIT)

#define TLMMV3_GP_INOUT_BIT		1
#define TLMMV3_GP_OUT			BIT(TLMMV3_GP_INOUT_BIT)
#define TLMMV3_GP_IN			0

#define gc_to_pintype(gc) \
		container_of(gc, struct msm_pintype_info, gc)
#define ic_to_pintype(ic) \
		((struct msm_pintype_info *)ic->pinfo)
#define pintype_get_gc(pinfo)	(&pinfo->gc)
#define pintype_get_ic(pinfo)	(pinfo->irq_chip)

/* SDC Pin type register offsets */
#define TLMMV3_SDC_OFFSET		0x2044
#define TLMMV3_SDC1_CFG(base)		(base)
#define TLMMV3_SDC2_CFG(base)		(TLMMV3_SDC1_CFG(base) + 0x4)

/* GP pin type register offsets */
#define TLMMV3_GP_CFG(base, pin)		(base + 0x1000 + 0x10 * (pin))
#define TLMMV3_GP_INOUT(base, pin)		(base + 0x1004 + 0x10 * (pin))
#define TLMMV3_GP_INTR_CFG(base, pin)		(base + 0x1008 + 0x10 * (pin))
#define TLMMV3_GP_INTR_STATUS(base, pin)	(base + 0x100c + 0x10 * (pin))

struct msm_sdc_regs {
	unsigned int offset;
	unsigned long pull_mask;
	unsigned long pull_shft;
	unsigned long drv_mask;
	unsigned long drv_shft;
};

static struct msm_sdc_regs sdc_regs[] = {
	/* SDC1 CLK */
	{
		.offset = 0,
		.pull_mask = TLMMV3_SDC1_CLK_PULL_MASK,
		.pull_shft = TLMMV3_SDC1_CLK_PULL_SHFT,
		.drv_mask = TLMMV3_SDC1_CLK_DRV_MASK,
		.drv_shft = TLMMV3_SDC1_CLK_DRV_SHFT,
	},
	/* SDC1 CMD */
	{
		.offset = 0,
		.pull_mask = TLMMV3_SDC1_CMD_PULL_MASK,
		.pull_shft = TLMMV3_SDC1_CMD_PULL_SHFT,
		.drv_mask = TLMMV3_SDC1_CMD_DRV_MASK,
		.drv_shft = TLMMV3_SDC1_CMD_DRV_SHFT,
	},
	/* SDC1 DATA */
	{
		.offset = 0,
		.pull_mask = TLMMV3_SDC1_DATA_PULL_MASK,
		.pull_shft = TLMMV3_SDC1_DATA_PULL_SHFT,
		.drv_mask = TLMMV3_SDC1_DATA_DRV_MASK,
		.drv_shft = TLMMV3_SDC1_DATA_DRV_SHFT,
	},
	/* SDC2 CLK */
	{
		.offset = 0x4,
		.pull_mask = TLMMV3_SDC2_CLK_PULL_MASK,
		.pull_shft = TLMMV3_SDC2_CLK_PULL_SHFT,
		.drv_mask = TLMMV3_SDC2_CLK_DRV_MASK,
		.drv_shft = TLMMV3_SDC2_CLK_DRV_SHFT,
	},
	/* SDC2 CMD */
	{
		.offset = 0x4,
		.pull_mask = TLMMV3_SDC2_CMD_PULL_MASK,
		.pull_shft = TLMMV3_SDC2_CMD_PULL_SHFT,
		.drv_mask = TLMMV3_SDC2_CMD_DRV_MASK,
		.drv_shft = TLMMV3_SDC2_CMD_DRV_SHFT,
	},
	/* SDC2 DATA */
	{
		.offset = 0x4,
		.pull_mask = TLMMV3_SDC2_DATA_PULL_MASK,
		.pull_shft = TLMMV3_SDC2_DATA_PULL_SHFT,
		.drv_mask = TLMMV3_SDC2_DATA_DRV_MASK,
		.drv_shft = TLMMV3_SDC2_DATA_DRV_SHFT,
	},
};

static int msm_tlmm_v3_sdc_cfg(uint pin_no, unsigned long *config,
						void __iomem *reg_base,
						bool write)
{
	unsigned int val, id, data;
	u32 mask, shft;
	void __iomem *cfg_reg;

	cfg_reg = reg_base + sdc_regs[pin_no].offset;
	id = pinconf_to_config_param(*config);
	val = readl_relaxed(cfg_reg);
	/* Get mask and shft values for this config type */
	switch (id) {
	case PIN_CONFIG_BIAS_DISABLE:
		mask = sdc_regs[pin_no].pull_mask;
		shft = sdc_regs[pin_no].pull_shft;
		data = TLMMV3_NO_PULL;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		mask = sdc_regs[pin_no].pull_mask;
		shft = sdc_regs[pin_no].pull_shft;
		data = TLMMV3_PULL_DOWN;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		mask = sdc_regs[pin_no].pull_mask;
		shft = sdc_regs[pin_no].pull_shft;
		data = TLMMV3_PULL_UP;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		mask = sdc_regs[pin_no].drv_mask;
		shft = sdc_regs[pin_no].drv_shft;
		if (write) {
			data = pinconf_to_config_argument(*config);
			data = drv_str_to_rval(data);
		} else {
			val >>= shft;
			val &= mask;
			data = rval_to_drv_str(val);
		}
		break;
	default:
		return -EINVAL;
	};

	if (write) {
		val &= ~(mask << shft);
		val |= (data << shft);
		writel_relaxed(val, cfg_reg);
	} else
		*config = pinconf_to_config_packed(id, data);
	return 0;
}

static void msm_tlmm_v3_sdc_set_reg_base(void __iomem **ptype_base,
							void __iomem *tlmm_base)
{
	*ptype_base = tlmm_base + TLMMV3_SDC_OFFSET;
}

static int msm_tlmm_v3_gp_cfg(uint pin_no, unsigned long *config,
						void *reg_base, bool write)
{
	unsigned int val, id, data, inout_val;
	u32 mask = 0, shft = 0;
	void __iomem *inout_reg = NULL;
	void __iomem *cfg_reg = TLMMV3_GP_CFG(reg_base, pin_no);

	id = pinconf_to_config_param(*config);
	val = readl_relaxed(cfg_reg);
	/* Get mask and shft values for this config type */
	switch (id) {
	case PIN_CONFIG_BIAS_DISABLE:
		mask = TLMMV3_GP_PULL_MASK;
		shft = TLMMV3_GP_PULL_SHFT;
		data = TLMMV3_NO_PULL;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		mask = TLMMV3_GP_PULL_MASK;
		shft = TLMMV3_GP_PULL_SHFT;
		data = TLMMV3_PULL_DOWN;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		mask = TLMMV3_GP_PULL_MASK;
		shft = TLMMV3_GP_PULL_SHFT;
		data = TLMMV3_PULL_UP;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		mask = TLMMV3_GP_DRV_MASK;
		shft = TLMMV3_GP_DRV_SHFT;
		if (write) {
			data = pinconf_to_config_argument(*config);
			data = drv_str_to_rval(data);
		} else {
			val >>= shft;
			val &= mask;
			data = rval_to_drv_str(val);
		}
		break;
	case PIN_CONFIG_OUTPUT:
		mask = TLMMV3_GP_DIR_MASK;
		shft = TLMMV3_GP_DIR_SHFT;
		inout_reg = TLMMV3_GP_INOUT(reg_base, pin_no);
		if (write) {
			data = pinconf_to_config_argument(*config);
			inout_val = dir_to_inout_val(data);
			writel_relaxed(inout_val, inout_reg);
			data = (mask << shft);
		} else {
			inout_val = readl_relaxed(inout_reg);
			data = inout_val_to_dir(inout_val);
		}
		break;
	default:
		return -EINVAL;
	};

	if (write) {
		val &= ~(mask << shft);
		val |= (data << shft);
		writel_relaxed(val, cfg_reg);
	} else
		*config = pinconf_to_config_packed(id, data);
	return 0;
}

static void msm_tlmm_v3_gp_fn(uint pin_no, u32 func, void *reg_base,
								bool enable)
{
	unsigned int val;
	void __iomem *cfg_reg = TLMMV3_GP_CFG(reg_base, pin_no);
	val = readl_relaxed(cfg_reg);
	val &= ~(TLMMV3_GP_FUNC_MASK << TLMMV3_GP_FUNC_SHFT);
	if (enable)
		val |= (func << TLMMV3_GP_FUNC_SHFT);
	writel_relaxed(val, cfg_reg);
}

static void msm_tlmm_v3_gp_set_reg_base(void __iomem **ptype_base,
						void __iomem *tlmm_base)
{
	*ptype_base = tlmm_base;
}

/* GPIO CHIP */
static int msm_tlmm_v3_gp_get(struct gpio_chip *gc, unsigned offset)
{
	struct msm_pintype_info *pinfo = gc_to_pintype(gc);
	void __iomem *inout_reg = TLMMV3_GP_INOUT(pinfo->reg_base, offset);

	return readl_relaxed(inout_reg) & BIT(GPIO_IN_BIT);
}

static void msm_tlmm_v3_gp_set(struct gpio_chip *gc, unsigned offset, int val)
{
	struct msm_pintype_info *pinfo = gc_to_pintype(gc);
	void __iomem *inout_reg = TLMMV3_GP_INOUT(pinfo->reg_base, offset);

	writel_relaxed(val ? BIT(GPIO_OUT_BIT) : 0, inout_reg);
}

static int msm_tlmm_v3_gp_dir_in(struct gpio_chip *gc, unsigned offset)
{
	unsigned int val;
	struct msm_pintype_info *pinfo = gc_to_pintype(gc);
	void __iomem *cfg_reg = TLMMV3_GP_CFG(pinfo->reg_base, offset);

	val = readl_relaxed(cfg_reg);
	val &= ~BIT(GPIO_OE_BIT);
	writel_relaxed(val, cfg_reg);
	return 0;
}

static int msm_tlmm_v3_gp_dir_out(struct gpio_chip *gc, unsigned offset,
								int val)
{
	struct msm_pintype_info *pinfo = gc_to_pintype(gc);
	void __iomem *cfg_reg = TLMMV3_GP_CFG(pinfo->reg_base, offset);

	msm_tlmm_v3_gp_set(gc, offset, val);
	val = readl_relaxed(cfg_reg);
	val |= BIT(GPIO_OE_BIT);
	writel_relaxed(val, cfg_reg);
	return 0;
}

static int msm_tlmm_v3_gp_to_irq(struct gpio_chip *gc, unsigned offset)
{
	struct msm_pintype_info *pinfo = gc_to_pintype(gc);
	struct msm_tlmm_irq_chip *ic = pintype_get_ic(pinfo);
	return irq_create_mapping(ic->domain, offset);
}

/* Irq reg ops */
static void msm_tlmm_v3_set_intr_status(struct msm_tlmm_irq_chip *ic,
								unsigned pin)
{
	void __iomem *status_reg = TLMMV3_GP_INTR_STATUS(ic->chip_base, pin);
	writel_relaxed(0, status_reg);
}

static int msm_tlmm_v3_get_intr_status(struct msm_tlmm_irq_chip *ic,
								unsigned pin)
{
	void __iomem *status_reg = TLMMV3_GP_INTR_STATUS(ic->chip_base, pin);
	return readl_relaxed(status_reg) & BIT(INTR_STATUS_BIT);
}

static void msm_tlmm_v3_set_intr_cfg_enable(struct msm_tlmm_irq_chip *ic,
								unsigned pin,
								int enable)
{
	unsigned int val;
	void __iomem *cfg_reg = TLMMV3_GP_INTR_CFG(ic->chip_base, pin);

	val = readl_relaxed(cfg_reg);
	if (enable) {
		val &= ~BIT(INTR_DIR_CONN_EN_BIT);
		val |= BIT(INTR_ENABLE_BIT);
	} else
		val &= ~BIT(INTR_ENABLE_BIT);
	writel_relaxed(val, cfg_reg);
}

static int msm_tlmm_v3_get_intr_cfg_enable(struct msm_tlmm_irq_chip *ic,
								unsigned pin)
{
	void __iomem *cfg_reg = TLMMV3_GP_INTR_CFG(ic->chip_base, pin);
	return readl_relaxed(cfg_reg) & BIT(INTR_ENABLE_BIT);
}

static void msm_tlmm_v3_set_intr_cfg_type(struct msm_tlmm_irq_chip *ic,
							struct irq_data *d,
							unsigned int type)
{
	unsigned cfg;
	void __iomem *cfg_reg = TLMMV3_GP_INTR_CFG(ic->chip_base,
							(irqd_to_hwirq(d)));
	/*
	 * RAW_STATUS_EN is left on for all gpio irqs. Due to the
	 * internal circuitry of TLMM, toggling the RAW_STATUS
	 * could cause the INTR_STATUS to be set for EDGE interrupts.
	 */
	cfg = BIT(INTR_RAW_STATUS_EN_BIT) | INTR_TARGET_PROC_APPS(ic->apps_id);
	writel_relaxed(cfg, cfg_reg);
	cfg &= ~INTR_DECT_CTL_MASK;
	if (type == IRQ_TYPE_EDGE_RISING)
		cfg |= INTR_DECT_CTL_POS_EDGE;
	else if (type == IRQ_TYPE_EDGE_FALLING)
		cfg |= INTR_DECT_CTL_NEG_EDGE;
	else if (type == IRQ_TYPE_EDGE_BOTH)
		cfg |= INTR_DECT_CTL_DUAL_EDGE;
	else
		cfg |= INTR_DECT_CTL_LEVEL;

	if (type & IRQ_TYPE_LEVEL_LOW)
		cfg &= ~INTR_POL_CTL_HI;
	else
		cfg |= INTR_POL_CTL_HI;

	writel_relaxed(cfg, cfg_reg);
	/*
	 * Sometimes it might take a little while to update
	 * the interrupt status after the RAW_STATUS is enabled
	 * We clear the interrupt status before enabling the
	 * interrupt in the unmask call-back.
	 */
	udelay(5);
}

static irqreturn_t msm_tlmm_v3_gp_handle_irq(int irq,
						struct  msm_tlmm_irq_chip *ic)
{
	unsigned long i;
	unsigned int virq = 0;
	struct irq_desc *desc = irq_to_desc(irq);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct msm_pintype_info *pinfo = ic_to_pintype(ic);
	struct gpio_chip *gc = pintype_get_gc(pinfo);

	chained_irq_enter(chip, desc);
	for_each_set_bit(i, ic->enabled_irqs, ic->num_irqs)
	 {
		dev_dbg(ic->dev, "hwirq in bit mask %d\n", (unsigned int)i);
		if (msm_tlmm_v3_get_intr_status(ic, i)) {
			dev_dbg(ic->dev, "hwirw %d fired\n", (unsigned int)i);
			virq = msm_tlmm_v3_gp_to_irq(gc, i);
			if (!virq) {
				dev_dbg(ic->dev, "invalid virq\n");
				return IRQ_NONE;
			}
			generic_handle_irq(virq);
		}

	}
	chained_irq_exit(chip, desc);
	return IRQ_HANDLED;
}

static void msm_tlmm_v3_irq_ack(struct irq_data *d)
{
	struct msm_tlmm_irq_chip *ic = irq_data_get_irq_chip_data(d);

	msm_tlmm_v3_set_intr_status(ic, irqd_to_hwirq(d));
	mb();
}

static void msm_tlmm_v3_irq_mask(struct irq_data *d)
{
	unsigned long irq_flags;
	struct msm_tlmm_irq_chip *ic = irq_data_get_irq_chip_data(d);

	spin_lock_irqsave(&ic->irq_lock, irq_flags);
	msm_tlmm_v3_set_intr_cfg_enable(ic, irqd_to_hwirq(d), 0);
	__clear_bit(irqd_to_hwirq(d), ic->enabled_irqs);
	mb();
	spin_unlock_irqrestore(&ic->irq_lock, irq_flags);
	if (ic->irq_chip_extn->irq_mask)
		ic->irq_chip_extn->irq_mask(d);
}

static void msm_tlmm_v3_irq_unmask(struct irq_data *d)
{
	unsigned long irq_flags;
	struct msm_tlmm_irq_chip *ic = irq_data_get_irq_chip_data(d);

	spin_lock_irqsave(&ic->irq_lock, irq_flags);
	__set_bit(irqd_to_hwirq(d), ic->enabled_irqs);
	if (!msm_tlmm_v3_get_intr_cfg_enable(ic, irqd_to_hwirq(d))) {
		msm_tlmm_v3_set_intr_status(ic, irqd_to_hwirq(d));
		msm_tlmm_v3_set_intr_cfg_enable(ic, irqd_to_hwirq(d), 1);
		mb();
	}
	spin_unlock_irqrestore(&ic->irq_lock, irq_flags);
	if (ic->irq_chip_extn->irq_unmask)
		ic->irq_chip_extn->irq_unmask(d);
}

static void msm_tlmm_v3_irq_disable(struct irq_data *d)
{
	struct msm_tlmm_irq_chip *ic = irq_data_get_irq_chip_data(d);
	if (ic->irq_chip_extn->irq_disable)
		ic->irq_chip_extn->irq_disable(d);
}

static int msm_tlmm_v3_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	unsigned long irq_flags;
	unsigned int pin = irqd_to_hwirq(d);
	struct msm_tlmm_irq_chip *ic = irq_data_get_irq_chip_data(d);


	spin_lock_irqsave(&ic->irq_lock, irq_flags);

	if (flow_type & IRQ_TYPE_EDGE_BOTH) {
		__irq_set_handler_locked(d->irq, handle_edge_irq);
		if ((flow_type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH)
			__set_bit(pin, ic->dual_edge_irqs);
		else
			__clear_bit(pin, ic->dual_edge_irqs);
	} else {
		__irq_set_handler_locked(d->irq, handle_level_irq);
		__clear_bit(pin, ic->dual_edge_irqs);
	}

	msm_tlmm_v3_set_intr_cfg_type(ic, d, flow_type);

	mb();
	spin_unlock_irqrestore(&ic->irq_lock, irq_flags);

	if ((flow_type & IRQ_TYPE_EDGE_BOTH) != IRQ_TYPE_EDGE_BOTH) {
		if (ic->irq_chip_extn->irq_set_type)
			ic->irq_chip_extn->irq_set_type(d, flow_type);
	}

	return 0;
}

static int msm_tlmm_v3_irq_set_wake(struct irq_data *d, unsigned int on)
{
	unsigned int pin = irqd_to_hwirq(d);
	struct msm_tlmm_irq_chip *ic = irq_data_get_irq_chip_data(d);

	if (on) {
		if (bitmap_empty(ic->wake_irqs, ic->num_irqs))
			irq_set_irq_wake(ic->irq, 1);
		set_bit(pin, ic->wake_irqs);
	} else {
		clear_bit(pin, ic->wake_irqs);
		if (bitmap_empty(ic->wake_irqs, ic->num_irqs))
			irq_set_irq_wake(ic->irq, 0);
	}

	if (ic->irq_chip_extn->irq_set_wake)
		ic->irq_chip_extn->irq_set_wake(d, on);

	return 0;
}

static struct lock_class_key msm_tlmm_irq_lock_class;

static int msm_tlmm_v3_irq_map(struct irq_domain *h, unsigned int virq,
					irq_hw_number_t hw)
{
	struct msm_tlmm_irq_chip *ic = h->host_data;

	irq_set_lockdep_class(virq, &msm_tlmm_irq_lock_class);
	irq_set_chip_data(virq, ic);
	irq_set_chip_and_handler(virq, &ic->chip,
					handle_level_irq);
	set_irq_flags(virq, IRQF_VALID);
	return 0;
}

/*
 * irq domain callbacks for interrupt controller.
 */
static const struct irq_domain_ops msm_tlmm_v3_gp_irqd_ops = {
	.map	= msm_tlmm_v3_irq_map,
	.xlate	= irq_domain_xlate_twocell,
};

struct irq_chip mpm_tlmm_irq_extn = {
	.irq_eoi	= NULL,
	.irq_mask	= NULL,
	.irq_unmask	= NULL,
	.irq_retrigger	= NULL,
	.irq_set_type	= NULL,
	.irq_set_wake	= NULL,
	.irq_disable	= NULL,
};

static struct msm_tlmm_irq_chip msm_tlmm_v3_gp_irq = {
			.irq_chip_extn = &mpm_tlmm_irq_extn,
			.chip = {
				.name		= "msm_tlmm_v3_irq",
				.irq_mask	= msm_tlmm_v3_irq_mask,
				.irq_unmask	= msm_tlmm_v3_irq_unmask,
				.irq_ack	= msm_tlmm_v3_irq_ack,
				.irq_set_type	= msm_tlmm_v3_irq_set_type,
				.irq_set_wake	= msm_tlmm_v3_irq_set_wake,
				.irq_disable	= msm_tlmm_v3_irq_disable,
			},
			.apps_id = TLMMV3_APPS_ID_DEFAULT,
			.domain_ops = &msm_tlmm_v3_gp_irqd_ops,
			.handler = msm_tlmm_v3_gp_handle_irq,
};

/* Power management core operations */

static int msm_tlmm_v3_gp_irq_suspend(void)
{
	unsigned long irq_flags;
	unsigned long i;
	struct msm_tlmm_irq_chip *ic = &msm_tlmm_v3_gp_irq;
	int num_irqs = ic->num_irqs;

	spin_lock_irqsave(&ic->irq_lock, irq_flags);
	for_each_set_bit(i, ic->enabled_irqs, num_irqs)
		msm_tlmm_v3_set_intr_cfg_enable(ic, i, 0);

	for_each_set_bit(i, ic->enabled_irqs, num_irqs)
		msm_tlmm_v3_set_intr_cfg_enable(ic, i, 1);
	mb();
	spin_unlock_irqrestore(&ic->irq_lock, irq_flags);
	return 0;
}

static void msm_tlmm_v3_gp_irq_resume(void)
{
	unsigned long irq_flags;
	unsigned long i;
	struct msm_tlmm_irq_chip *ic = &msm_tlmm_v3_gp_irq;
	int num_irqs = ic->num_irqs;

	spin_lock_irqsave(&ic->irq_lock, irq_flags);
	for_each_set_bit(i, ic->wake_irqs, num_irqs)
		msm_tlmm_v3_set_intr_cfg_enable(ic, i, 0);

	for_each_set_bit(i, ic->enabled_irqs, num_irqs)
		msm_tlmm_v3_set_intr_cfg_enable(ic, i, 1);
	mb();
	spin_unlock_irqrestore(&ic->irq_lock, irq_flags);
}

static struct syscore_ops msm_tlmm_v3_irq_syscore_ops = {
	.suspend = msm_tlmm_v3_gp_irq_suspend,
	.resume = msm_tlmm_v3_gp_irq_resume,
};

#ifdef CONFIG_USE_PINCTRL_IRQ
int __init msm_tlmm_v3_of_irq_init(struct device_node *controller,
						struct device_node *parent)
{
	int ret, num_irqs, apps_id;
	struct msm_tlmm_irq_chip *ic = &msm_tlmm_v3_gp_irq;

	ret = of_property_read_u32(controller, "num_irqs", &num_irqs);
	if (ret) {
		WARN(1, "Cannot get numirqs from device tree\n");
		return ret;
	}
	ret = of_property_read_u32(controller, "apps_id", &apps_id);
	if (!ret) {
		pr_info("processor id specified, in device tree %d\n", apps_id);
		ic->apps_id = apps_id;
	}
	ic->num_irqs = num_irqs;
	ic->domain = irq_domain_add_linear(controller, ic->num_irqs,
						ic->domain_ops,
						ic);
	if (IS_ERR(ic->domain))
			return -ENOMEM;
	return 0;
}
#endif

static int msm_tlmm_v3_gp_irq_init(int irq, struct msm_pintype_info *pinfo,
						struct device *tlmm_dev)
{
	int num_irqs;
	struct msm_tlmm_irq_chip *ic = pinfo->irq_chip;

	num_irqs = ic->num_irqs;
	ic->enabled_irqs = devm_kzalloc(tlmm_dev, sizeof(unsigned long)
					* BITS_TO_LONGS(num_irqs), GFP_KERNEL);
	if (IS_ERR(ic->enabled_irqs)) {
		dev_err(tlmm_dev, "Unable to allocate enabled irqs bitmap\n");
		return PTR_ERR(ic->enabled_irqs);
	}
	ic->dual_edge_irqs = devm_kzalloc(tlmm_dev, sizeof(unsigned long)
					* BITS_TO_LONGS(num_irqs), GFP_KERNEL);
	if (IS_ERR(ic->dual_edge_irqs)) {
		dev_err(tlmm_dev, "Unable to allocate dual edge irqs bitmap\n");
		return PTR_ERR(ic->dual_edge_irqs);
	}
	ic->wake_irqs = devm_kzalloc(tlmm_dev, sizeof(unsigned long)
					* BITS_TO_LONGS(num_irqs), GFP_KERNEL);
	if (IS_ERR(ic->wake_irqs)) {
		dev_err(tlmm_dev, "Unable to allocate dual edge irqs bitmap\n");
		return PTR_ERR(ic->wake_irqs);
	}
	spin_lock_init(&ic->irq_lock);
	ic->chip_base = pinfo->reg_base;
	ic->irq = irq;
	ic->dev = tlmm_dev;
	ic->num_irqs = pinfo->num_pins;
	ic->pinfo = pinfo;
	register_syscore_ops(&msm_tlmm_v3_irq_syscore_ops);
	return 0;
}

static irqreturn_t msm_tlmm_v3_handle_irq(int irq, void *data)
{
	int i, num_pintypes;
	struct msm_pintype_info *pintypes, *pintype;
	struct msm_tlmm_irq_chip *ic;
	struct msm_tlmm_desc *tlmm_desc = (struct msm_tlmm_desc *)data;
	irqreturn_t ret = IRQ_NONE;

	pintypes = tlmm_desc->pintypes;
	num_pintypes = tlmm_desc->num_pintypes;
	for (i = 0; i < num_pintypes; i++) {
		pintype = &pintypes[i];
		if (!pintype->irq_chip)
			continue;
		ic = pintype->irq_chip;
		if (!ic->node)
			continue;
		ret = ic->handler(irq, ic);
		if (ret != IRQ_HANDLED)
			break;
	}
	return ret;
}

static struct msm_pintype_info tlmm_v3_pininfo[] = {
	{
		.prg_cfg = msm_tlmm_v3_gp_cfg,
		.prg_func = msm_tlmm_v3_gp_fn,
		.set_reg_base = msm_tlmm_v3_gp_set_reg_base,
		.reg_base = NULL,
		.prop_name = "qcom,pin-type-gp",
		.name = "gp",
		.gc = {
				.label		  = "msm_tlmm_v3_gpio",
				.direction_input  = msm_tlmm_v3_gp_dir_in,
				.direction_output = msm_tlmm_v3_gp_dir_out,
				.get              = msm_tlmm_v3_gp_get,
				.set              = msm_tlmm_v3_gp_set,
				.to_irq           = msm_tlmm_v3_gp_to_irq,
		},
		.init_irq = msm_tlmm_v3_gp_irq_init,
		.irq_chip = &msm_tlmm_v3_gp_irq,
	},
	{
		.prg_cfg = msm_tlmm_v3_sdc_cfg,
		.set_reg_base = msm_tlmm_v3_sdc_set_reg_base,
		.reg_base = NULL,
		.prop_name = "qcom,pin-type-sdc",
		.name = "sdc",
	}
};

struct msm_tlmm_pintype tlmm_v3_pintypes = {
	.num_entries = ARRAY_SIZE(tlmm_v3_pininfo),
	.pintype_info = tlmm_v3_pininfo,
};

static const struct of_device_id msm_tlmm_v3_dt_match[] = {
	{ .compatible = "qcom,msm-tlmm-v3",
		.data = &tlmm_v3_pintypes, },
	{},
};
MODULE_DEVICE_TABLE(of, msm_tlmm_v3_dt_match);

static int msm_tlmm_v3_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct msm_tlmm_pintype *pinfo;
	struct msm_tlmm_desc *tlmm_desc;
	int irq, ret;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;

	match = of_match_node(msm_tlmm_v3_dt_match, node);
	if (IS_ERR(match))
		return PTR_ERR(match);
	pinfo = match->data;
	tlmm_desc = devm_kzalloc(&pdev->dev, sizeof(*tlmm_desc), GFP_KERNEL);
	if (!tlmm_desc) {
		dev_err(&pdev->dev, "Alloction failed for tlmm desc\n");
		return -ENOMEM;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot find IO resource\n");
		return -ENOENT;
	}
	tlmm_desc->base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (IS_ERR(tlmm_desc->base))
		return PTR_ERR(tlmm_desc->base);
	tlmm_desc->irq = -EINVAL;
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res) {
		irq = res->start;
		ret = devm_request_irq(&pdev->dev, irq, msm_tlmm_v3_handle_irq,
							IRQF_TRIGGER_HIGH,
							dev_name(&pdev->dev),
							tlmm_desc);
		if (ret) {
			dev_err(&pdev->dev, "register for irq failed\n");
			return ret;
		}
		tlmm_desc->irq = irq;
	}
	tlmm_desc->pintypes = pinfo->pintype_info;
	tlmm_desc->num_pintypes = pinfo->num_entries;
	return msm_pinctrl_probe(pdev, tlmm_desc);
}

static struct platform_driver msm_tlmm_v3_drv = {
	.probe		= msm_tlmm_v3_probe,
	.driver = {
		.name	= "msm-tlmmv3-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(msm_tlmm_v3_dt_match),
	},
};

static int __init msm_tlmm_v3_drv_register(void)
{
	return platform_driver_register(&msm_tlmm_v3_drv);
}
postcore_initcall(msm_tlmm_v3_drv_register);

static void __exit msm_tlmm_v3_drv_unregister(void)
{
	platform_driver_unregister(&msm_tlmm_v3_drv);
}
module_exit(msm_tlmm_v3_drv_unregister);

MODULE_LICENSE("GPLv2");
