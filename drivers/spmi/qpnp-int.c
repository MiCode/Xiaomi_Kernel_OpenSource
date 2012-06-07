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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/spmi.h>
#include <linux/radix-tree.h>
#include <linux/slab.h>
#include <linux/printk.h>

#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <mach/qpnp-int.h>

#define QPNPINT_MAX_BUSSES 1

/* 16 slave_ids, 256 per_ids per slave, and 8 ints per per_id */
#define QPNPINT_NR_IRQS (16 * 256 * 8)

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

struct q_perip_data {
	uint8_t type;	    /* bitmap */
	uint8_t pol_high;   /* bitmap */
	uint8_t pol_low;    /* bitmap */
	uint8_t int_en;     /* bitmap */
	uint8_t use_count;
};

struct q_irq_data {
	uint32_t priv_d; /* data to optimize arbiter interactions */
	struct q_chip_data *chip_d;
	struct q_perip_data *per_d;
	uint8_t mask_shift;
	uint8_t spmi_slave;
	uint16_t spmi_offset;
};

struct q_chip_data {
	int bus_nr;
	struct irq_domain domain;
	struct qpnp_local_int cb;
	struct spmi_controller *spmi_ctrl;
	struct radix_tree_root per_tree;
};

static struct q_chip_data chip_data[QPNPINT_MAX_BUSSES] __read_mostly;

/**
 * qpnpint_encode_hwirq - translate between qpnp_irq_spec and
 *			  hwirq representation.
 *
 * slave_offset = (addr->slave * 256 * 8);
 * perip_offset = slave_offset + (addr->perip * 8);
 * return perip_offset + addr->irq;
 */
static inline int qpnpint_encode_hwirq(struct qpnp_irq_spec *spec)
{
	uint32_t hwirq;

	if (spec->slave > 15 || spec->irq > 7)
		return -EINVAL;

	hwirq = (spec->slave << 11);
	hwirq |= (spec->per << 3);
	hwirq |= spec->irq;

	return hwirq;
}
/**
 * qpnpint_decode_hwirq - translate between hwirq and
 *			  qpnp_irq_spec representation.
 */
static inline int qpnpint_decode_hwirq(unsigned long hwirq,
					struct qpnp_irq_spec *spec)
{
	if (hwirq > 65535)
		return -EINVAL;

	spec->slave = (hwirq >> 11) & 0xF;
	spec->per = (hwirq >> 3) & 0xFF;
	spec->irq = hwirq & 0x7;
	return 0;
}

static int qpnpint_spmi_write(struct q_irq_data *irq_d, uint8_t reg,
			      void *buf, uint32_t len)
{
	struct q_chip_data *chip_d = irq_d->chip_d;
	int rc;

	if (!chip_d->spmi_ctrl)
		return -ENODEV;

	rc = spmi_ext_register_writel(chip_d->spmi_ctrl, irq_d->spmi_slave,
				      irq_d->spmi_offset + reg, buf, len);
	return rc;
}

static void qpnpint_irq_mask(struct irq_data *d)
{
	struct q_irq_data *irq_d = irq_data_get_irq_chip_data(d);
	struct q_chip_data *chip_d = irq_d->chip_d;
	struct q_perip_data *per_d = irq_d->per_d;
	struct qpnp_irq_spec q_spec;
	int rc;

	pr_debug("hwirq %lu irq: %d\n", d->hwirq, d->irq);

	if (chip_d->cb.mask) {
		rc = qpnpint_decode_hwirq(d->hwirq, &q_spec);
		if (rc)
			pr_err("%s: decode failed on hwirq %lu\n",
						 __func__, d->hwirq);
		else
			chip_d->cb.mask(chip_d->spmi_ctrl, &q_spec,
								irq_d->priv_d);
	}

	per_d->int_en &= ~irq_d->mask_shift;

	rc = qpnpint_spmi_write(irq_d, QPNPINT_REG_EN_CLR,
					(u8 *)&irq_d->mask_shift, 1);
	if (rc)
		pr_err("%s: spmi failure on irq %d\n",
						 __func__, d->irq);
}

static void qpnpint_irq_mask_ack(struct irq_data *d)
{
	struct q_irq_data *irq_d = irq_data_get_irq_chip_data(d);
	struct q_chip_data *chip_d = irq_d->chip_d;
	struct q_perip_data *per_d = irq_d->per_d;
	struct qpnp_irq_spec q_spec;
	int rc;

	pr_debug("hwirq %lu irq: %d mask: 0x%x\n", d->hwirq, d->irq,
							irq_d->mask_shift);

	if (chip_d->cb.mask) {
		rc = qpnpint_decode_hwirq(d->hwirq, &q_spec);
		if (rc)
			pr_err("%s: decode failed on hwirq %lu\n",
						 __func__, d->hwirq);
		else
			chip_d->cb.mask(chip_d->spmi_ctrl, &q_spec,
								irq_d->priv_d);
	}

	per_d->int_en &= ~irq_d->mask_shift;

	rc = qpnpint_spmi_write(irq_d, QPNPINT_REG_EN_CLR,
							&irq_d->mask_shift, 1);
	if (rc)
		pr_err("%s: spmi failure on irq %d\n",
						 __func__, d->irq);

	rc = qpnpint_spmi_write(irq_d, QPNPINT_REG_LATCHED_CLR,
							&irq_d->mask_shift, 1);
	if (rc)
		pr_err("%s: spmi failure on irq %d\n",
						 __func__, d->irq);
}

static void qpnpint_irq_unmask(struct irq_data *d)
{
	struct q_irq_data *irq_d = irq_data_get_irq_chip_data(d);
	struct q_chip_data *chip_d = irq_d->chip_d;
	struct q_perip_data *per_d = irq_d->per_d;
	struct qpnp_irq_spec q_spec;
	int rc;

	pr_debug("hwirq %lu irq: %d\n", d->hwirq, d->irq);

	if (chip_d->cb.unmask) {
		rc = qpnpint_decode_hwirq(d->hwirq, &q_spec);
		if (rc)
			pr_err("%s: decode failed on hwirq %lu\n",
						 __func__, d->hwirq);
		else
			chip_d->cb.unmask(chip_d->spmi_ctrl, &q_spec,
								irq_d->priv_d);
	}

	per_d->int_en |= irq_d->mask_shift;
	rc = qpnpint_spmi_write(irq_d, QPNPINT_REG_EN_SET,
					&irq_d->mask_shift, 1);
	if (rc)
		pr_err("%s: spmi failure on irq %d\n",
						 __func__, d->irq);
}

static int qpnpint_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct q_irq_data *irq_d = irq_data_get_irq_chip_data(d);
	struct q_perip_data *per_d = irq_d->per_d;
	int rc;
	u8 buf[3];

	pr_debug("hwirq %lu irq: %d flow: 0x%x\n", d->hwirq,
							d->irq, flow_type);

	per_d->pol_high &= ~irq_d->mask_shift;
	per_d->pol_low &= ~irq_d->mask_shift;
	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		per_d->type |= irq_d->mask_shift; /* edge trig */
		if (flow_type & IRQF_TRIGGER_RISING)
			per_d->pol_high |= irq_d->mask_shift;
		if (flow_type & IRQF_TRIGGER_FALLING)
			per_d->pol_low |= irq_d->mask_shift;
	} else {
		if ((flow_type & IRQF_TRIGGER_HIGH) &&
		    (flow_type & IRQF_TRIGGER_LOW))
			return -EINVAL;
		per_d->type &= ~irq_d->mask_shift; /* level trig */
		if (flow_type & IRQF_TRIGGER_HIGH)
			per_d->pol_high |= irq_d->mask_shift;
		else
			per_d->pol_high &= ~irq_d->mask_shift;
	}

	buf[0] = per_d->type;
	buf[1] = per_d->pol_high;
	buf[2] = per_d->pol_low;

	rc = qpnpint_spmi_write(irq_d, QPNPINT_REG_SET_TYPE, &buf, 3);
	if (rc)
		pr_err("%s: spmi failure on irq %d\n",
						 __func__, d->irq);
	return rc;
}

static struct irq_chip qpnpint_chip = {
	.name		= "qpnp-int",
	.irq_mask	= qpnpint_irq_mask,
	.irq_mask_ack	= qpnpint_irq_mask_ack,
	.irq_unmask	= qpnpint_irq_unmask,
	.irq_set_type	= qpnpint_irq_set_type,
};

static int qpnpint_init_irq_data(struct q_chip_data *chip_d,
				 struct q_irq_data *irq_d,
				 unsigned long hwirq)
{
	struct qpnp_irq_spec q_spec;
	int rc;

	irq_d->mask_shift = 1 << (hwirq & 0x7);
	rc = qpnpint_decode_hwirq(hwirq, &q_spec);
	if (rc < 0)
		return rc;
	irq_d->spmi_slave = q_spec.slave;
	irq_d->spmi_offset = q_spec.per << 8;
	irq_d->per_d->use_count++;
	irq_d->chip_d = chip_d;

	if (chip_d->cb.register_priv_data)
		rc = chip_d->cb.register_priv_data(chip_d->spmi_ctrl, &q_spec,
							&irq_d->priv_d);
	return rc;
}

static struct q_irq_data *qpnpint_alloc_irq_data(
					struct q_chip_data *chip_d,
					unsigned long hwirq)
{
	struct q_irq_data *irq_d;
	struct q_perip_data *per_d;

	irq_d = kzalloc(sizeof(struct q_irq_data), GFP_KERNEL);
	if (!irq_d)
		return ERR_PTR(-ENOMEM);

	/**
	 * The Peripheral Tree is keyed from the slave + per_id. We're
	 * ignoring the irq bits here since this peripheral structure
	 * should be common for all irqs on the same peripheral.
	 */
	per_d = radix_tree_lookup(&chip_d->per_tree, (hwirq & ~0x7));
	if (!per_d) {
		per_d = kzalloc(sizeof(struct q_perip_data), GFP_KERNEL);
		if (!per_d)
			return ERR_PTR(-ENOMEM);
		radix_tree_insert(&chip_d->per_tree,
				  (hwirq & ~0x7), per_d);
	}
	irq_d->per_d = per_d;

	return irq_d;
}

static int qpnpint_register_int(uint32_t busno, unsigned long hwirq)
{
	int irq, rc;
	struct irq_domain *domain;
	struct q_irq_data *irq_d;

	pr_debug("busno = %u hwirq = %lu\n", busno, hwirq);

	if (hwirq < 0 || hwirq >= 32768) {
		pr_err("%s: hwirq %lu out of qpnp interrupt bounds\n",
							__func__, hwirq);
		return -EINVAL;
	}

	if (busno < 0 || busno > QPNPINT_MAX_BUSSES) {
		pr_err("%s: invalid bus number %d\n", __func__, busno);
		return -EINVAL;
	}

	domain = &chip_data[busno].domain;
	irq = irq_domain_to_irq(domain, hwirq);

	rc = irq_alloc_desc_at(irq, numa_node_id());
	if (rc < 0) {
		if (rc != -EEXIST)
			pr_err("%s: failed to alloc irq at %d with "
					"rc %d\n", __func__, irq, rc);
		return rc;
	}
	irq_d = qpnpint_alloc_irq_data(&chip_data[busno], hwirq);
	if (IS_ERR(irq_d)) {
		pr_err("%s: failed to alloc irq data %d with "
					"rc %d\n", __func__, irq, rc);
		rc = PTR_ERR(irq_d);
		goto register_err_cleanup;
	}
	rc = qpnpint_init_irq_data(&chip_data[busno], irq_d, hwirq);
	if (rc) {
		pr_err("%s: failed to init irq data %d with "
					"rc %d\n", __func__, irq, rc);
		goto register_err_cleanup;
	}

	irq_domain_register_irq(domain, hwirq);

	irq_set_chip_and_handler(irq,
			&qpnpint_chip,
			handle_level_irq);
	irq_set_chip_data(irq, irq_d);
#ifdef CONFIG_ARM
	set_irq_flags(irq, IRQF_VALID);
#else
	irq_set_noprobe(irq);
#endif
	return 0;

register_err_cleanup:
	irq_free_desc(irq);
	if (!IS_ERR(irq_d)) {
		if (irq_d->per_d->use_count == 1)
			kfree(irq_d->per_d);
		else
			irq_d->per_d->use_count--;
		kfree(irq_d);
	}
	return rc;
}

static int qpnpint_irq_domain_dt_translate(struct irq_domain *d,
				       struct device_node *controller,
				       const u32 *intspec, unsigned int intsize,
				       unsigned long *out_hwirq,
				       unsigned int *out_type)
{
	struct qpnp_irq_spec addr;
	struct q_chip_data *chip_d = d->priv;
	int ret;

	pr_debug("%s: intspec[0] 0x%x intspec[1] 0x%x intspec[2] 0x%x\n",
				__func__, intspec[0], intspec[1], intspec[2]);

	if (d->of_node != controller)
		return -EINVAL;
	if (intsize != 3)
		return -EINVAL;

	addr.irq = intspec[2] & 0x7;
	addr.per = intspec[1] & 0xFF;
	addr.slave = intspec[0] & 0xF;

	ret = qpnpint_encode_hwirq(&addr);
	if (ret < 0) {
		pr_err("%s: invalid intspec\n", __func__);
		return ret;
	}
	*out_hwirq = ret;
	*out_type = IRQ_TYPE_NONE;

	/**
	 * Register the interrupt if it's not already registered.
	 * This implies that mapping a qpnp interrupt allocates
	 * resources.
	 */
	ret = qpnpint_register_int(chip_d->bus_nr, *out_hwirq);
	if (ret && ret != -EEXIST) {
		pr_err("%s: Cannot register hwirq %lu\n", __func__, *out_hwirq);
		return ret;
	}

	return 0;
}

const struct irq_domain_ops qpnpint_irq_domain_ops = {
	.dt_translate = qpnpint_irq_domain_dt_translate,
};

int qpnpint_register_controller(unsigned int busno,
				struct qpnp_local_int *li_cb)
{
	if (busno >= QPNPINT_MAX_BUSSES)
		return -EINVAL;
	chip_data[busno].cb = *li_cb;
	chip_data[busno].spmi_ctrl = spmi_busnum_to_ctrl(busno);
	if (!chip_data[busno].spmi_ctrl)
		return -ENOENT;

	return 0;
}
EXPORT_SYMBOL(qpnpint_register_controller);

int qpnpint_handle_irq(struct spmi_controller *spmi_ctrl,
		       struct qpnp_irq_spec *spec)
{
	struct irq_domain *domain;
	unsigned long hwirq, busno;
	int irq;

	pr_debug("spec slave = %u per = %u irq = %u\n",
					spec->slave, spec->per, spec->irq);

	if (!spec || !spmi_ctrl)
		return -EINVAL;

	busno = spmi_ctrl->nr;
	if (busno >= QPNPINT_MAX_BUSSES)
		return -EINVAL;

	hwirq = qpnpint_encode_hwirq(spec);
	if (hwirq < 0) {
		pr_err("%s: invalid irq spec passed\n", __func__);
		return -EINVAL;
	}

	domain = &chip_data[busno].domain;
	irq = irq_domain_to_irq(domain, hwirq);

	generic_handle_irq(irq);

	return 0;
}
EXPORT_SYMBOL(qpnpint_handle_irq);

/**
 * This assumes that there's a relationship between the order of the interrupt
 * controllers specified to of_irq_match() is the SPMI device topology. If
 * this ever turns out to be a bad assumption, then of_irq_init_cb_t should
 * be modified to pass a parameter to this function.
 */
static int qpnpint_cnt __initdata;

int __init qpnpint_of_init(struct device_node *node, struct device_node *parent)
{
	struct q_chip_data *chip_d = &chip_data[qpnpint_cnt];
	struct irq_domain *domain = &chip_d->domain;

	INIT_RADIX_TREE(&chip_d->per_tree, GFP_ATOMIC);

	domain->irq_base = irq_domain_find_free_range(0, QPNPINT_NR_IRQS);
	domain->nr_irq = QPNPINT_NR_IRQS;
	domain->of_node = of_node_get(node);
	domain->priv = chip_d;
	domain->ops = &qpnpint_irq_domain_ops;
	irq_domain_add(domain);

	pr_info("irq_base = %d\n", domain->irq_base);

	qpnpint_cnt++;

	return 0;
}
EXPORT_SYMBOL(qpnpint_of_init);
