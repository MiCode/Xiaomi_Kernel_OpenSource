/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spmi.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "spmi-sim.h"

#define SPMI_SIM_PERM_NOT_READ	SPMI_SIM_PERM_W
#define SPMI_SIM_PERM_NOT_WRITE	SPMI_SIM_PERM_R

#define HWIRQ(slave_id, periph_id, irq_id) \
	((((slave_id) & 0xF)   << 12) | \
	(((periph_id) & 0xFF)  << 4) | \
	(((irq_id)    & 0x7)   << 0))

#define HWIRQ_SID(hwirq)  (((hwirq) >> 12) & 0xF)
#define HWIRQ_PER(hwirq)  (((hwirq) >> 4) & 0xFF)
#define HWIRQ_IRQ(hwirq)  (((hwirq) >> 0) & 0x7)

/* Common PMIC interrupt register offsets */
enum qpnpint_regs {
	QPNPINT_REG_RT_STS		= 0x10,
	QPNPINT_REG_SET_TYPE		= 0x11,
	QPNPINT_REG_POLARITY_HIGH	= 0x12,
	QPNPINT_REG_POLARITY_LOW	= 0x13,
	QPNPINT_REG_LATCHED_CLR		= 0x14,
	QPNPINT_REG_EN_SET		= 0x15,
	QPNPINT_REG_EN_CLR		= 0x16,
	QPNPINT_REG_LATCHED_STS		= 0x18,
};

/**
 * struct spmi_sim_register - simulated SPMI register state and configuration
 *
 * @value:		Current register value
 * @not_readable:	Flag indicating that the register is not readable
 * @not_writeable:	Flag indicating that the register is not writeable
 * @initialized:	Flag indicating that some entity has initialized the
 *			register value.  This is used primarily to allow device
 *			tree default values to override generic hardware default
 *			register values specified by a PMIC simulator driver.
 * @ops:		Pointer to register operators
 */
struct spmi_sim_register {
	u8			value;
	bool			not_readable;
	bool			not_writeable;
	bool			initialized;
	struct spmi_sim_ops	*ops;
};

/**
 * struct sim_range - range of SPMI registers to simulate
 *
 * @start:		First 20-bit SPMI register in the range
 * @end:		Last 20-bit SPMI register in the range
 * @reg:		Array of register structs for each register in the range
 */
struct sim_range {
	u32				start;
	u32				end;
	struct spmi_sim_register	*reg;
};

/**
 * struct spmi_sim - SIM simulator primary state and configuration structure
 *
 * @lock:		Spinlock used to ensure mutual exclusion of SPMI
 *			transactions
 * @domain:		IRQ domain for the simulated SPMI interrupts
 * @ctrl:		Pointer to the SPMI controller struct
 * @range:		Array of simulated register ranges
 * @range_count:	Number of elements in 'range' array
 * @of_node:		Device tree node pointer for the SPMI simulator device
 * @debugfs:		SPMI simulator debugfs base directory
 * @debug_hwirq:	hwirq value used by debugfs operations
 * @irq_lock:		Spinlock used when triggering IRQs
 */
struct spmi_sim {
	spinlock_t		lock;
	struct irq_domain	*domain;
	struct spmi_controller	*ctrl;
	struct sim_range	*range;
	int			range_count;
	struct device_node	*of_node;
	struct dentry		*debugfs;
	u32			debug_hwirq;
	spinlock_t		irq_lock;
};

/* Non-data command */
static int spmi_sim_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	return -EOPNOTSUPP;
}

static struct spmi_sim_register *spmi_sim_find_reg(struct spmi_sim *sim,
						u32 addr)
{
	int i;

	for (i = 0; i < sim->range_count; i++) {
		if (addr >= sim->range[i].start &&
		    addr <= sim->range[i].end)
			return &sim->range[i].reg[addr - sim->range[i].start];
	}

	dev_err(&sim->ctrl->dev, "SPMI address 0x%05X does not exist\n", addr);

	return ERR_PTR(-ENODEV);
}

static int spmi_sim_read_reg(struct spmi_sim *sim, u32 addr, u8 *val)
{
	struct spmi_sim_register *reg;
	int rc;

	reg = spmi_sim_find_reg(sim, addr);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	if (reg->ops && reg->ops->pre_read) {
		rc = reg->ops->pre_read(sim, addr);
		if (rc) {
			dev_err(&sim->ctrl->dev, "pre_read(0x%05X) failed, rc=%d\n",
				addr, rc);
			return rc;
		}
	}

	*val = reg->not_readable ? 0 : reg->value;

	if (reg->ops && reg->ops->post_read) {
		rc = reg->ops->post_read(sim, addr, val);
		if (rc) {
			dev_err(&sim->ctrl->dev, "post_read(0x%05X) failed, rc=%d\n",
				addr, rc);
			return rc;
		}
	}

	return 0;
}

static int spmi_sim_write_reg(struct spmi_sim *sim, u32 addr, u8 val)
{
	struct spmi_sim_register *reg;
	int rc;

	reg = spmi_sim_find_reg(sim, addr);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	if (reg->ops && reg->ops->pre_write) {
		rc = reg->ops->pre_write(sim, addr, &val);
		if (rc) {
			dev_err(&sim->ctrl->dev, "pre_write(0x%05X) failed, rc=%d\n",
				addr, rc);
			return rc;
		}
	}

	if (!reg->not_writeable)
		reg->value = val;

	if (reg->ops && reg->ops->post_write) {
		rc = reg->ops->post_write(sim, addr);
		if (rc) {
			dev_err(&sim->ctrl->dev, "post_write(0x%05X) failed, rc=%d\n",
				addr, rc);
			return rc;
		}
	}

	return 0;
}

#define DEBUG_PRINT_BUFFER_SIZE 64
static void fill_string(char *str, size_t str_len, const u8 *buf, int buf_len)
{
	int pos = 0;
	int i;

	str[0] = '\0';
	for (i = 0; i < buf_len; i++) {
		pos += scnprintf(str + pos, str_len - pos, "0x%02X", buf[i]);
		if (i < buf_len - 1)
			pos += scnprintf(str + pos, str_len - pos, ", ");
	}
}

static int spmi_sim_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			     u16 addr, u8 *buf, size_t len)
{
	struct spmi_sim *sim = spmi_controller_get_drvdata(ctrl);
	u32 base_addr = ((u32)sid << 16) | addr;
	char str[DEBUG_PRINT_BUFFER_SIZE];
	unsigned long flags;
	int rc = 0;
	int i;

	spin_lock_irqsave(&sim->lock, flags);

	for (i = 0; i < len; i++) {
		rc = spmi_sim_read_reg(sim, base_addr + i, &buf[i]);
		if (rc)
			break;
	}

	spin_unlock_irqrestore(&sim->lock, flags);

	fill_string(str, DEBUG_PRINT_BUFFER_SIZE, buf, len);
	dev_dbg(&sim->ctrl->dev, " read(0x%05X): %s\n", base_addr, str);

	return rc;
}

static int spmi_sim_write_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			     u16 addr, const u8 *buf, size_t len)
{
	struct spmi_sim *sim = spmi_controller_get_drvdata(ctrl);
	u32 base_addr = ((u32)sid << 16) | addr;
	char str[DEBUG_PRINT_BUFFER_SIZE];
	unsigned long flags;
	int rc = 0;
	int i;

	fill_string(str, DEBUG_PRINT_BUFFER_SIZE, buf, len);
	dev_dbg(&sim->ctrl->dev, "write(0x%05X): %s\n", base_addr, str);

	spin_lock_irqsave(&sim->lock, flags);

	for (i = 0; i < len; i++) {
		rc = spmi_sim_write_reg(sim, base_addr + i, buf[i]);
		if (rc)
			break;
	}

	spin_unlock_irqrestore(&sim->lock, flags);

	return rc;
}

/* Simplified accessor functions for irqchip callbacks */
static void qpnpint_spmi_write(struct irq_data *d, u8 reg, void *buf,
			       size_t len)
{
	struct spmi_sim *sim = irq_data_get_irq_chip_data(d);
	u8 sid = HWIRQ_SID(d->hwirq);
	u8 per = HWIRQ_PER(d->hwirq);

	if (spmi_sim_write_cmd(sim->ctrl, SPMI_CMD_EXT_WRITEL, sid,
			       (per << 8) + reg, buf, len))
		dev_err_ratelimited(&sim->ctrl->dev, "failed irqchip write transaction on %u\n",
				    d->irq);
}

static void qpnpint_spmi_read(struct irq_data *d, u8 reg, void *buf, size_t len)
{
	struct spmi_sim *sim = irq_data_get_irq_chip_data(d);
	u8 sid = HWIRQ_SID(d->hwirq);
	u8 per = HWIRQ_PER(d->hwirq);

	if (spmi_sim_read_cmd(sim->ctrl, SPMI_CMD_EXT_READL, sid,
			      (per << 8) + reg, buf, len))
		dev_err_ratelimited(&sim->ctrl->dev, "failed irqchip read transaction on %u\n",
				    d->irq);
}

/**
 * spmi_sim_trigger_irq() - trigger a simulated SPMI interrupt
 *
 * @sim:	Pointer to the SPMI simulator
 * @sid:	Global slave ID of the PMIC peripheral
 * @per:	PMIC peripheral ID; 20-bit SPMI address bits [15:8]
 * @irq:	IRQ within the peripheral to trigger (0 - 7)
 *
 * This function simulates a PMIC interrupt triggering in hardware.  The IRQ
 * will be triggered even if it is not enabled in the EN_SET register.
 *
 * Return: 0 on success, errno on failure
 */
int spmi_sim_trigger_irq(struct spmi_sim *sim, u8 sid, u8 per, u8 irq)
{
	unsigned int virq;
	u32 addr;
	int rc;
	unsigned long flags;

	virq = irq_find_mapping(sim->domain, HWIRQ(sid, per, irq));
	if (virq == 0) {
		dev_err(&sim->ctrl->dev, "could not find virq for sid=0x%X, per=0x%02X, irq=%u\n",
			sid, per, irq);
		return -EINVAL;
	}

	dev_dbg(&sim->ctrl->dev, "triggering irq %u (sid=0x%X, per=0x%02X, irq=%u)\n",
		virq, sid, per, irq);

	addr = (sid << 16) | (per << 8) | QPNPINT_REG_LATCHED_STS;
	rc = spmi_sim_masked_write(sim, addr, BIT(irq), BIT(irq));
	if (rc)
		return rc;

	spin_lock_irqsave(&sim->irq_lock, flags);
	generic_handle_irq(virq);
	spin_unlock_irqrestore(&sim->irq_lock, flags);

	return 0;
}
EXPORT_SYMBOL(spmi_sim_trigger_irq);


/**
 * spmi_sim_set_irq_rt_status() - set the real-time status of a PMIC IRQ signal
 *
 * @sim:	Pointer to the SPMI simulator
 * @sid:	Global slave ID of the PMIC peripheral
 * @per:	PMIC peripheral ID; 20-bit SPMI address bits [15:8]
 * @irq:	IRQ within the peripheral to trigger (0 - 7)
 * @rt_status:	Desired real-time status value (0 or 1)
 *
 * This function simulates a PMIC IRQ signal.  It will automatically invoke
 * spmi_sim_trigger_irq() if the IRQ is enabled and the rt_status transition
 * matches the IRQ type and polarity configuration registers.
 *
 * Return: 0 on success, errno on failure
 */
int spmi_sim_set_irq_rt_status(struct spmi_sim *sim, u8 sid, u8 per, u8 irq,
			u8 rt_status)
{
	u8 enable, type, pol_high, pol_low, mask, old_rt_status, new_rt_status;
	u32 addr;
	int rc;

	addr = (sid << 16) | (per << 8);
	mask = BIT(irq);

	rc = spmi_sim_read(sim, addr + QPNPINT_REG_RT_STS, &old_rt_status);
	if (rc)
		return rc;

	new_rt_status = (old_rt_status & ~mask) | (rt_status ? mask : 0);

	rc = spmi_sim_write(sim, addr + QPNPINT_REG_RT_STS, new_rt_status);
	if (rc)
		return rc;

	old_rt_status &= mask;
	new_rt_status &= mask;

	/* Check if the IRQ is enabled */
	rc = spmi_sim_read(sim, addr + QPNPINT_REG_EN_SET, &enable);
	if (rc)
		return rc;
	enable &= mask;
	if (!enable)
		return 0;

	rc = spmi_sim_read(sim, addr + QPNPINT_REG_SET_TYPE, &type);
	if (rc)
		return rc;
	type &= mask;

	rc = spmi_sim_read(sim, addr + QPNPINT_REG_POLARITY_HIGH, &pol_high);
	if (rc)
		return rc;
	pol_high &= mask;

	rc = spmi_sim_read(sim, addr + QPNPINT_REG_POLARITY_LOW, &pol_low);
	if (rc)
		return rc;
	pol_low &= mask;

	/* Check level and edge conditions */
	if ((!type && pol_high && new_rt_status) ||
	    (!type && pol_low && !new_rt_status) ||
	    (type && pol_high && !old_rt_status && new_rt_status) ||
	    (type && pol_low && old_rt_status && !new_rt_status)) {
		rc = spmi_sim_trigger_irq(sim, sid, per, irq);
		if (rc) {
			dev_err(&sim->ctrl->dev, "error triggering SPMI IRQ sid=%u, per=0x%02X, irq=%u, rc=%d\n",
				sid, per, irq, rc);
			return rc;
		}
	}

	return 0;
}
EXPORT_SYMBOL(spmi_sim_set_irq_rt_status);

static void qpnpint_irq_ack(struct irq_data *d)
{
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 data;

	data = BIT(irq);
	qpnpint_spmi_write(d, QPNPINT_REG_LATCHED_CLR, &data, 1);
}

static void qpnpint_irq_mask(struct irq_data *d)
{
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 data = BIT(irq);

	qpnpint_spmi_write(d, QPNPINT_REG_EN_CLR, &data, 1);
}

static void qpnpint_irq_unmask(struct irq_data *d)
{
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 buf[2];

	qpnpint_spmi_read(d, QPNPINT_REG_EN_SET, &buf[0], 1);
	if (!(buf[0] & BIT(irq))) {
		/*
		 * Since the interrupt is currently disabled, write to both the
		 * LATCHED_CLR and EN_SET registers so that a spurious interrupt
		 * cannot be triggered when the interrupt is enabled
		 */
		buf[0] = BIT(irq);
		buf[1] = BIT(irq);
		qpnpint_spmi_write(d, QPNPINT_REG_LATCHED_CLR, &buf, 2);
	}
}

struct spmi_sim_qpnpint_type {
	u8 type;		/* 1 == edge */
	u8 polarity_high;
	u8 polarity_low;
} __packed;

static int qpnpint_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct spmi_sim_qpnpint_type type;
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 bit_mask_irq = BIT(irq);

	qpnpint_spmi_read(d, QPNPINT_REG_SET_TYPE, &type, sizeof(type));

	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		type.type |= bit_mask_irq;
		if (flow_type & IRQF_TRIGGER_RISING)
			type.polarity_high |= bit_mask_irq;
		if (flow_type & IRQF_TRIGGER_FALLING)
			type.polarity_low  |= bit_mask_irq;
	} else {
		if ((flow_type & (IRQF_TRIGGER_HIGH)) &&
		    (flow_type & (IRQF_TRIGGER_LOW)))
			return -EINVAL;

		type.type &= ~bit_mask_irq; /* level trig */
		if (flow_type & IRQF_TRIGGER_HIGH)
			type.polarity_high |= bit_mask_irq;
		else
			type.polarity_low  |= bit_mask_irq;
	}

	qpnpint_spmi_write(d, QPNPINT_REG_SET_TYPE, &type, sizeof(type));

	if (flow_type & IRQ_TYPE_EDGE_BOTH)
		irq_set_handler_locked(d, handle_edge_irq);
	else
		irq_set_handler_locked(d, handle_level_irq);

	return 0;
}

static int qpnpint_get_irqchip_state(struct irq_data *d,
				     enum irqchip_irq_state which,
				     bool *state)
{
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 status = 0;

	if (which != IRQCHIP_STATE_LINE_LEVEL)
		return -EINVAL;

	qpnpint_spmi_read(d, QPNPINT_REG_RT_STS, &status, 1);
	*state = !!(status & BIT(irq));

	return 0;
}

static struct irq_chip pmic_arb_irqchip = {
	.name			= "pmic_sim",
	.irq_ack		= qpnpint_irq_ack,
	.irq_mask		= qpnpint_irq_mask,
	.irq_unmask		= qpnpint_irq_unmask,
	.irq_set_type		= qpnpint_irq_set_type,
	.irq_get_irqchip_state	= qpnpint_get_irqchip_state,
	.flags			= IRQCHIP_MASK_ON_SUSPEND |
				  IRQCHIP_SKIP_SET_WAKE,
};

static void qpnpint_irq_domain_activate(struct irq_domain *domain,
					struct irq_data *d)
{
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 buf;

	buf = BIT(irq);
	qpnpint_spmi_write(d, QPNPINT_REG_EN_CLR, &buf, 1);
	qpnpint_spmi_write(d, QPNPINT_REG_LATCHED_CLR, &buf, 1);
}

static int qpnpint_irq_domain_dt_translate(struct irq_domain *d,
					   struct device_node *controller,
					   const u32 *intspec,
					   unsigned int intsize,
					   unsigned long *out_hwirq,
					   unsigned int *out_type)
{
	struct spmi_sim *sim = d->host_data;
	struct spmi_sim_register *reg;
	u32 addr;
	int rc;

	if (irq_domain_get_of_node(d) != controller)
		return -EINVAL;
	if (intsize != 4)
		return -EINVAL;
	if (intspec[0] > 0xF || intspec[1] > 0xFF || intspec[2] > 0x7)
		return -EINVAL;

	addr = (intspec[0] << 16) | (intspec[1] << 8);
	reg = spmi_sim_find_reg(sim, addr);
	if (IS_ERR(reg)) {
		rc = PTR_ERR(reg);
		dev_err(&sim->ctrl->dev, "failed to translate sid = 0x%X, periph = 0x%02X, irq = %u; rc = %d\n",
			intspec[0], intspec[1], intspec[2], rc);
		return rc;
	}

	*out_hwirq = HWIRQ(intspec[0], intspec[1], intspec[2]);
	*out_type  = intspec[3] & IRQ_TYPE_SENSE_MASK;

	return 0;
}

static int qpnpint_irq_domain_map(struct irq_domain *d,
				  unsigned int virq,
				  irq_hw_number_t hwirq)
{
	struct spmi_sim *sim = d->host_data;

	dev_dbg(&sim->ctrl->dev, "virq = %u, hwirq = %lu\n", virq, hwirq);

	irq_set_chip_and_handler(virq, &pmic_arb_irqchip, handle_level_irq);
	irq_set_chip_data(virq, d->host_data);
	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops pmic_arb_irq_domain_ops = {
	.map		= qpnpint_irq_domain_map,
	.xlate		= qpnpint_irq_domain_dt_translate,
	.activate	= qpnpint_irq_domain_activate,
};

/**
 * spmi_sim_read() - read a simulated SPMI register value
 *
 * @sim:	Pointer to the SPMI simulator
 * @addr:	20-bit SPMI register address
 * @val:	Filled with register value on success
 *
 * Return: 0 on success, errno on failure
 */
int spmi_sim_read(struct spmi_sim *sim, u32 addr, u8 *val)
{
	struct spmi_sim_register *reg;

	if (IS_ERR_OR_NULL(sim)) {
		pr_err("%s: invalid sim pointer\n", __func__);
		return -EINVAL;
	}

	reg = spmi_sim_find_reg(sim, addr);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	*val = reg->value;

	return 0;
}
EXPORT_SYMBOL(spmi_sim_read);

/**
 * spmi_sim_write() - write a simulated SPMI register value
 *
 * @sim:	Pointer to the SPMI simulator
 * @addr:	20-bit SPMI register address
 * @val:	Value to be written into the register
 *
 * Return: 0 on success, errno on failure
 */
int spmi_sim_write(struct spmi_sim *sim, u32 addr, u8 val)
{
	struct spmi_sim_register *reg;

	if (IS_ERR_OR_NULL(sim)) {
		pr_err("%s: invalid sim pointer\n", __func__);
		return -EINVAL;
	}

	reg = spmi_sim_find_reg(sim, addr);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	reg->value = val;

	return 0;
}
EXPORT_SYMBOL(spmi_sim_write);

/**
 * spmi_sim_write() - masked write to a simulated SPMI register value
 *
 * @sim:	Pointer to the SPMI simulator
 * @addr:	20-bit SPMI register address
 * @mask:	Mask of bits to modify with 'val'
 * @val:	Value to be written into the register
 *
 * Return: 0 on success, errno on failure
 */
int spmi_sim_masked_write(struct spmi_sim *sim, u32 addr, u8 mask, u8 val)
{
	struct spmi_sim_register *reg;

	if (IS_ERR_OR_NULL(sim)) {
		pr_err("%s: invalid sim pointer\n", __func__);
		return -EINVAL;
	}

	reg = spmi_sim_find_reg(sim, addr);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	reg->value &= ~mask;
	reg->value |= val & mask;

	return 0;
}
EXPORT_SYMBOL(spmi_sim_masked_write);

/**
 * spmi_sim_get() - get the handle for an SPMI simulator device
 *
 * @dev:	Device pointer of a PMIC simulator associated with an SPMI
 *		simulator
 *
 * The device node associated with 'dev' must specify a property named
 * "qcom,spmi-sim" which contains a phandle for the SPMI simulator device.
 *
 * Return: SPMI simulator handle on success, ERR_PTR on failure
 */
struct spmi_sim *spmi_sim_get(struct device *dev)
{
	struct spmi_sim *sim = ERR_PTR(-EPROBE_DEFER);
	struct platform_device *pdev;
	struct device_node *node;

	if (IS_ERR_OR_NULL(dev)) {
		pr_err("%s: invalid device pointer\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	node = of_parse_phandle(dev->of_node, "qcom,spmi-sim", 0);
	if (!node) {
		dev_err(dev, "qcom,spmi-sim property missing\n");
		return ERR_PTR(-EINVAL);
	}

	pdev = of_find_device_by_node(node);
	if (pdev)
		sim = platform_get_drvdata(pdev);
	of_node_put(node);

	return sim;
}
EXPORT_SYMBOL(spmi_sim_get);

/**
 * spmi_sim_init_register() - initialize simulated SPMI register default values
 *				and read/write permissions
 *
 * @sim:	Pointer to the SPMI simulator
 * @regs:	Array of register initializers
 * @count:	Number of elements in 'regs'
 * @base_addr:	20-bit SPMI address corresponding to PMIC local SID 0 register
 *		0x0000
 *
 * The base_addr parameter is provided so that PMIC addresses can be defined
 * statically without worrying about the global SID offset used for the PMIC on
 * a given board.
 *
 * Return: 0 on success, errno on failure
 */
int spmi_sim_init_register(struct spmi_sim *sim,
			const struct spmi_sim_register_init *regs,
			size_t count,
			u32 base_addr)
{
	struct spmi_sim_register *reg;
	u32 addr;
	int i;

	if (IS_ERR_OR_NULL(sim) || IS_ERR_OR_NULL(regs)) {
		pr_err("%s: invalid pointer(s)\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		addr = regs[i].addr + base_addr;
		reg = spmi_sim_find_reg(sim, addr);
		if (IS_ERR(reg))
			return PTR_ERR(reg);

		if (reg->initialized)
			dev_dbg(&sim->ctrl->dev, "SPMI simulator address 0x%05X already initialized\n",
				addr);
		else
			reg->value = regs[i].value;

		reg->not_readable
			= regs[i].permissions & SPMI_SIM_PERM_NOT_READ;
		reg->not_writeable
			= regs[i].permissions & SPMI_SIM_PERM_NOT_WRITE;
		reg->initialized = true;
	}

	return 0;
}
EXPORT_SYMBOL(spmi_sim_init_register);

/**
 * spmi_sim_register_ops() - register simulated SPMI register operators
 *
 * @sim:	Pointer to the SPMI simulator
 * @reg_ops:	Array of register operator initializers
 * @count:	Number of elements in 'reg_ops'
 * @base_addr:	20-bit SPMI address corresponding to PMIC local SID 0 register
 *		0x0000
 *
 * The base_addr parameter is provided so that PMIC addresses can be defined
 * statically without worrying about the global SID offset used for the PMIC on
 * a given board.
 *
 * Return: 0 on success, errno on failure
 */
int spmi_sim_register_ops(struct spmi_sim *sim,
			const struct spmi_sim_register_ops_init *reg_ops,
			size_t count,
			u32 base_addr)
{
	struct spmi_sim_register *reg;
	u32 addr;
	int i;

	if (IS_ERR_OR_NULL(sim) || IS_ERR_OR_NULL(reg_ops)) {
		pr_err("%s: invalid pointer(s)\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		addr = reg_ops[i].addr + base_addr;
		reg = spmi_sim_find_reg(sim, addr);
		if (IS_ERR(reg))
			return PTR_ERR(reg);

		if (reg->ops)
			dev_dbg(&sim->ctrl->dev, "SPMI simulator address 0x%05X ops already initialized\n",
				addr);
		else
			reg->ops = reg_ops[i].ops;
	}

	return 0;
}
EXPORT_SYMBOL(spmi_sim_register_ops);

/**
 * spmi_sim_unregister_ops() - unregister simulated SPMI register operators
 *
 * @sim:	Pointer to the SPMI simulator
 * @reg_ops:	Array of register operator initializers
 * @count:	Number of elements in 'reg_ops'
 * @base_addr:	20-bit SPMI address corresponding to PMIC local SID 0 register
 *		0x0000
 *
 * The base_addr parameter is provided so that PMIC addresses can be defined
 * statically without worrying about the global SID offset used for the PMIC on
 * a given board.
 *
 * Return: 0 on success, errno on failure
 */
int spmi_sim_unregister_ops(struct spmi_sim *sim,
			const struct spmi_sim_register_ops_init *reg_ops,
			size_t count,
			u32 base_addr)
{
	struct spmi_sim_register *reg;
	u32 addr;
	int i;

	if (IS_ERR_OR_NULL(sim) || IS_ERR_OR_NULL(reg_ops)) {
		pr_err("%s: invalid pointer(s)\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		addr = reg_ops[i].addr + base_addr;
		reg = spmi_sim_find_reg(sim, addr);
		if (IS_ERR(reg))
			return PTR_ERR(reg);

		reg->ops = NULL;
	}

	return 0;
}
EXPORT_SYMBOL(spmi_sim_unregister_ops);

static int spmi_sim_hwirq_get(void *data, u64 *val)
{
	struct spmi_sim *sim = data;

	*val = sim->debug_hwirq;

	return 0;
}

static int spmi_sim_hwirq_set(void *data, u64 val)
{
	struct spmi_sim *sim = data;

	sim->debug_hwirq = val;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(spmi_sim_hwirq_fops, spmi_sim_hwirq_get,
			spmi_sim_hwirq_set, "0x%05llX\n");

static int spmi_sim_irq_trigger_set(void *data, u64 val)
{
	struct spmi_sim *sim = data;
	struct spmi_sim_register *reg;
	u32 sid, per, irq, addr;
	int rc;

	irq = HWIRQ_IRQ(sim->debug_hwirq);
	per = HWIRQ_PER(sim->debug_hwirq);
	sid = HWIRQ_SID(sim->debug_hwirq);

	addr = (sid << 16) | (per << 8) | QPNPINT_REG_EN_SET;
	reg = spmi_sim_find_reg(sim, addr);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	if (!(reg->value & BIT(irq))) {
		dev_info(&sim->ctrl->dev, "SPMI IRQ not enabled: sid=%u, per=0x%02X, irq=%u\n",
			sid, per, irq);
		return 0;
	}

	rc = spmi_sim_trigger_irq(sim, sid, per, irq);
	if (rc) {
		dev_err(&sim->ctrl->dev, "error triggering SPMI IRQ 0x%04llX; sid=%u, per=0x%02X, irq=%u, rc=%d\n",
			val, sid, per, irq, rc);
		return rc;
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(spmi_sim_irq_trigger_fops, NULL,
			spmi_sim_irq_trigger_set, "%llx\n");

static int spmi_sim_rt_status_get(void *data, u64 *val)
{
	struct spmi_sim *sim = data;
	u32 sid, per, irq, addr;
	u8 reg_val;
	int rc;

	irq = HWIRQ_IRQ(sim->debug_hwirq);
	per = HWIRQ_PER(sim->debug_hwirq);
	sid = HWIRQ_SID(sim->debug_hwirq);

	addr = (sid << 16) | (per << 8) | QPNPINT_REG_RT_STS;
	rc = spmi_sim_read(sim, addr, &reg_val);
	if (rc)
		return rc;

	*val = !!(reg_val & BIT(irq));

	return 0;
}

static int spmi_sim_rt_status_set(void *data, u64 val)
{
	struct spmi_sim *sim = data;
	u32 sid, per, irq;
	int rc;

	irq = HWIRQ_IRQ(sim->debug_hwirq);
	per = HWIRQ_PER(sim->debug_hwirq);
	sid = HWIRQ_SID(sim->debug_hwirq);

	rc = spmi_sim_set_irq_rt_status(sim, sid, per, irq, !!val);
	if (rc)
		return rc;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(spmi_sim_rt_status_fops, spmi_sim_rt_status_get,
			spmi_sim_rt_status_set, "%llu\n");

static int spmi_sim_irq_latched_clr_pre_write(struct spmi_sim *sim,  u32 addr,
						u8 *val)
{
	u32 per_addr = addr - QPNPINT_REG_LATCHED_CLR;
	int rc;

	rc = spmi_sim_masked_write(sim, per_addr + QPNPINT_REG_LATCHED_STS,
				*val, 0);
	if (rc)
		return rc;

	*val = 0;

	return 0;
}

static struct spmi_sim_ops spmi_sim_irq_latched_clr_ops = {
	.pre_write = spmi_sim_irq_latched_clr_pre_write,
};

static int spmi_sim_irq_en_set_pre_write(struct spmi_sim *sim,  u32 addr,
						u8 *val)
{
	u8 reg_val;
	int rc;

	rc = spmi_sim_read(sim, addr, &reg_val);
	if (rc)
		return rc;

	*val |= reg_val;

	return 0;
}

static struct spmi_sim_ops spmi_sim_irq_en_set_ops = {
	.pre_write = spmi_sim_irq_en_set_pre_write,
};

static int spmi_sim_irq_en_clr_pre_write(struct spmi_sim *sim,  u32 addr,
						u8 *val)
{
	u32 per_addr = addr - QPNPINT_REG_EN_CLR;
	int rc;

	rc = spmi_sim_masked_write(sim, per_addr + QPNPINT_REG_EN_SET, *val, 0);
	if (rc)
		return rc;

	*val = 0;

	return 0;
}

static int spmi_sim_irq_en_clr_post_read(struct spmi_sim *sim,  u32 addr,
						u8 *val)
{
	u32 per_addr = addr - QPNPINT_REG_EN_CLR;
	int rc;

	rc = spmi_sim_read(sim, per_addr + QPNPINT_REG_EN_SET, val);
	if (rc)
		return rc;

	return 0;
}

static struct spmi_sim_ops spmi_sim_irq_en_clr_ops = {
	.post_read = spmi_sim_irq_en_clr_post_read,
	.pre_write = spmi_sim_irq_en_clr_pre_write,
};

static int spmi_sim_irq_register_config(struct spmi_sim *sim, u32 addr)
{
	const struct spmi_sim_register_ops_init reg_init[] = {
		{addr + QPNPINT_REG_LATCHED_CLR, &spmi_sim_irq_latched_clr_ops},
		{addr + QPNPINT_REG_EN_SET, &spmi_sim_irq_en_set_ops},
		{addr + QPNPINT_REG_EN_CLR, &spmi_sim_irq_en_clr_ops},
	};
	int rc;

	rc = spmi_sim_register_ops(sim, reg_init, ARRAY_SIZE(reg_init), 0);

	return rc;
}

static int spmi_sim_irq_register_init(struct spmi_sim *sim)
{
	u32 addr, addr_end;
	int i, rc;

	for (i = 0; i < sim->range_count; i++) {
		addr = round_up(sim->range[i].start, 0x100);
		addr_end = sim->range[i].end - QPNPINT_REG_EN_CLR;
		for (; addr <= addr_end; addr += 0x100) {
			rc = spmi_sim_irq_register_config(sim, addr);
			if (rc)
				return rc;
		}
	}

	return 0;
}

static int spmi_sim_debugfs_init(struct spmi_sim *sim)
{
	struct dentry *irq_dir;
	char buf[20];

	scnprintf(buf, sizeof(buf), "spmi-sim%u", sim->ctrl->nr);
	sim->debugfs = debugfs_create_dir(buf, NULL);
	irq_dir = debugfs_create_dir("irq", sim->debugfs);
	debugfs_create_file("hwirq", 0644, irq_dir, sim,
				&spmi_sim_hwirq_fops);
	debugfs_create_file("trigger", 0200, irq_dir, sim,
				&spmi_sim_irq_trigger_fops);
	debugfs_create_file("rt_status", 0644, irq_dir, sim,
				&spmi_sim_rt_status_fops);

	return 0;
}

static int spmi_sim_load_defaults(struct spmi_sim *sim)
{
	struct spmi_sim_register *reg;
	u32 addr, val;
	int i, rc;
	int len = 0;

	if (!of_find_property(sim->ctrl->dev.of_node, "qcom,reg-defaults",
				&len))
		return 0;

	if (len % (sizeof(u32) * 2)) {
		dev_err(&sim->ctrl->dev, "qcom,reg-defaults property size is invalid\n");
		return -EINVAL;
	}

	len /= sizeof(u32) * 2;

	for (i = 0; i < len; i++) {
		rc = of_property_read_u32_index(sim->ctrl->dev.of_node,
					"qcom,reg-defaults", i * 2, &addr);
		if (rc) {
			dev_err(&sim->ctrl->dev, "error reading qcom,reg-defaults, rc=%d\n",
				rc);
			return rc;
		}

		reg = spmi_sim_find_reg(sim, addr);
		if (IS_ERR(reg))
			return PTR_ERR(reg);

		rc = of_property_read_u32_index(sim->ctrl->dev.of_node,
					"qcom,reg-defaults", i * 2 + 1, &val);
		if (rc) {
			dev_err(&sim->ctrl->dev, "error reading qcom,reg-defaults, rc=%d\n",
				rc);
			return rc;
		}

		if (val > 0xFF) {
			dev_err(&sim->ctrl->dev, "qcom,reg-defaults register value 0x%02X is too large\n",
				val);
			return -EINVAL;
		}

		reg->value = val;
		reg->initialized = true;
	}

	return 0;
}

static int spmi_sim_validate_ranges(struct spmi_sim *sim)
{
	int i, j;
	struct sim_range *r1;
	struct sim_range *r2;

	for (i = 0; i < sim->range_count; i++) {
		for (j = i + 1; j < sim->range_count; j++) {
			r1 = &sim->range[i];
			r2 = &sim->range[j];
			if (r1->start <= r2->end && r2->start <= r1->end) {
				dev_err(&sim->ctrl->dev, "register ranges overlap: [0x%05X, 0x%05X] and [0x%05X, 0x%05X]\n",
					r1->start, r1->end, r2->start, r2->end);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static void spmi_sim_free_ranges(struct spmi_sim *sim)
{
	int i;

	for (i = 0; i < sim->range_count; i++)
		vfree(sim->range[i].reg);
}

static int spmi_sim_probe(struct platform_device *pdev)
{
	struct spmi_sim *sim;
	struct spmi_controller *ctrl;
	struct resource *res;
	resource_size_t len;
	int rc, i;

	ctrl = spmi_controller_alloc(&pdev->dev, sizeof(*sim));
	if (!ctrl)
		return -ENOMEM;

	sim = spmi_controller_get_drvdata(ctrl);
	sim->ctrl = ctrl;
	sim->of_node = pdev->dev.of_node;
	platform_set_drvdata(pdev, sim);
	spin_lock_init(&sim->lock);
	spin_lock_init(&sim->irq_lock);

	for (i = 0, sim->range_count = 0; i < pdev->num_resources; i++) {
		if (resource_type(&pdev->resource[i]) == IORESOURCE_MEM)
			sim->range_count++;
	}

	if (sim->range_count == 0) {
		dev_err(&pdev->dev, "emulated SPMI address ranges not specified\n");
		rc = -EINVAL;
		goto err_put_ctrl;
	}

	sim->range = devm_kcalloc(&pdev->dev, sim->range_count,
				sizeof(*sim->range), GFP_KERNEL);
	if (!sim->range) {
		rc = -ENOMEM;
		goto err_put_ctrl;
	}

	for (i = 0; i < sim->range_count; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(&pdev->dev, "could not read address\n");
			rc = -EINVAL;
			goto err_free_ranges;
		}
		len = resource_size(res);
		if (len == 0) {
			dev_err(&pdev->dev, "address range has size 0\n");
			rc = -EINVAL;
			goto err_free_ranges;
		}
		sim->range[i].start = res->start;
		sim->range[i].end = res->end;
		sim->range[i].reg = vzalloc(len * sizeof(*sim->range[i].reg));
		if (!sim->range[i].reg) {
			rc = -ENOMEM;
			goto err_free_ranges;
		}

	}

	rc = spmi_sim_validate_ranges(sim);
	if (rc)
		goto err_free_ranges;

	rc = spmi_sim_load_defaults(sim);
	if (rc)
		goto err_free_ranges;

	rc = spmi_sim_irq_register_init(sim);
	if (rc)
		goto err_free_ranges;

	ctrl->cmd = spmi_sim_cmd;
	ctrl->read_cmd = spmi_sim_read_cmd;
	ctrl->write_cmd = spmi_sim_write_cmd;

	sim->domain = irq_domain_add_tree(pdev->dev.of_node,
					 &pmic_arb_irq_domain_ops, sim);
	if (!sim->domain) {
		dev_err(&pdev->dev, "unable to create irq_domain\n");
		rc = -ENOMEM;
		goto err_free_ranges;
	}

	rc = spmi_sim_debugfs_init(sim);
	if (rc)
		goto err_domain_remove;

	rc = spmi_controller_add(ctrl);
	if (rc)
		goto err_debugfs_remove;

	dev_info(&ctrl->dev, "SPMI simulator bus registered\n");

	return 0;

err_debugfs_remove:
	debugfs_remove_recursive(sim->debugfs);
err_domain_remove:
	irq_domain_remove(sim->domain);
err_free_ranges:
	spmi_sim_free_ranges(sim);
err_put_ctrl:
	spmi_controller_put(ctrl);

	return rc;
}

static int spmi_sim_remove(struct platform_device *pdev)
{
	struct spmi_sim *sim = platform_get_drvdata(pdev);

	debugfs_remove_recursive(sim->debugfs);

	spmi_sim_free_ranges(sim);
	spmi_controller_remove(sim->ctrl);
	irq_domain_remove(sim->domain);
	spmi_controller_put(sim->ctrl);

	return 0;
}

static const struct of_device_id spmi_sim_match_table[] = {
	{ .compatible = "qcom,spmi-sim", },
	{},
};
MODULE_DEVICE_TABLE(of, spmi_sim_match_table);

static struct platform_driver spmi_sim_driver = {
	.probe		= spmi_sim_probe,
	.remove		= spmi_sim_remove,
	.driver		= {
		.name	= "spmi_sim",
		.of_match_table = spmi_sim_match_table,
	},
};

static int __init spmi_sim_init(void)
{
	return platform_driver_register(&spmi_sim_driver);
}
postcore_initcall(spmi_sim_init);

static void __exit spmi_sim_exit(void)
{
	platform_driver_unregister(&spmi_sim_driver);
}
module_exit(spmi_sim_exit);

MODULE_DESCRIPTION("SPMI Simulator Driver");
MODULE_LICENSE("GPL v2");
