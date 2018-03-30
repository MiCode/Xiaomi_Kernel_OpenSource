/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/bitmap.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spmi.h>

/* PMIC Arbiter configuration registers */
#define VPMIC_ARB_VERSION		0x0000

/* Virtual PMIC Arbiter registers offset*/
#define VPMIC_ARB_CMD			0x00
#define VPMIC_ARB_STATUS		0x04
#define VPMIC_ARB_DATA0			0x08
#define VPMIC_ARB_DATA1			0x10

/* Mapping Table */
#define PMIC_ARB_MAX_PPID		BIT(12) /* PPID is 12bit */
#define PMIC_ARB_CHAN_VALID		BIT(15)

/* Channel Status fields */
enum pmic_arb_chnl_status {
	PMIC_ARB_STATUS_DONE	= BIT(0),
	PMIC_ARB_STATUS_FAILURE	= BIT(1),
	PMIC_ARB_STATUS_DENIED	= BIT(2),
	PMIC_ARB_STATUS_DROPPED	= BIT(3),
};

/* Command Opcodes */
enum pmic_arb_cmd_op_code {
	PMIC_ARB_OP_EXT_WRITEL = 0,
	PMIC_ARB_OP_EXT_READL = 1,
	PMIC_ARB_OP_EXT_WRITE = 2,
	PMIC_ARB_OP_RESET = 3,
	PMIC_ARB_OP_SLEEP = 4,
	PMIC_ARB_OP_SHUTDOWN = 5,
	PMIC_ARB_OP_WAKEUP = 6,
	PMIC_ARB_OP_AUTHENTICATE = 7,
	PMIC_ARB_OP_MSTR_READ = 8,
	PMIC_ARB_OP_MSTR_WRITE = 9,
	PMIC_ARB_OP_EXT_READ = 13,
	PMIC_ARB_OP_WRITE = 14,
	PMIC_ARB_OP_READ = 15,
	PMIC_ARB_OP_ZERO_WRITE = 16,
};

/*
 * PMIC arbiter version 5 uses different register offsets for read/write vs
 * observer channels.
 */
enum pmic_arb_channel {
	PMIC_ARB_CHANNEL_RW,
	PMIC_ARB_CHANNEL_OBS,
};

/* Maximum number of support PMIC peripherals */
#define PMIC_ARB_MAX_PERIPHS		512
#define PMIC_ARB_TIMEOUT_US		100
#define PMIC_ARB_MAX_TRANS_BYTES	(8)

#define PMIC_ARB_APID_MASK		0xFF
#define PMIC_ARB_PPID_MASK		0xFFF

/* interrupt enable bit */
#define SPMI_PIC_ACC_ENABLE_BIT		BIT(0)

#define HWIRQ(slave_id, periph_id, irq_id, apid) \
	((((slave_id) & 0xF)   << 28) | \
	(((periph_id) & 0xFF)  << 20) | \
	(((irq_id)    & 0x7)   << 16) | \
	(((apid)      & 0x1FF) << 0))

#define HWIRQ_SID(hwirq)  (((hwirq) >> 28) & 0xF)
#define HWIRQ_PER(hwirq)  (((hwirq) >> 20) & 0xFF)
#define HWIRQ_IRQ(hwirq)  (((hwirq) >> 16) & 0x7)
#define HWIRQ_APID(hwirq) (((hwirq) >> 0)  & 0x1FF)

struct vspmi_backend_driver_ver_ops;

struct apid_data {
	u16		ppid;
	u8		write_owner;
	u8		irq_owner;
};

/**
 * vspmi_pmic_arb - Virtual SPMI PMIC Arbiter object
 *
 * @wr_base:		on v1 "core", on v2 "chnls"    register base off DT.
 * @intr:		address of the SPMI interrupt control registers.
 * @acc_status:		address of SPMI ACC interrupt status registers.
 * @lock:		lock to synchronize accesses.
 * @irq:		PMIC ARB interrupt.
 * @min_apid:		minimum APID (used for bounding IRQ search)
 * @max_apid:		maximum APID
 * @max_periph:		maximum number of PMIC peripherals supported by HW.
 * @mapping_table:	in-memory copy of PPID -> APID mapping table.
 * @domain:		irq domain object for PMIC IRQ domain
 * @spmic:		SPMI controller object
 * @ver_ops:		backend version dependent operations.
 * @ppid_to_apid	in-memory copy of PPID -> channel (APID) mapping table.
 */
struct vspmi_pmic_arb {
	void __iomem		*wr_base;
	void __iomem		*core;
	void __iomem		*intr;
	void __iomem		*acc_status;
	resource_size_t		core_size;
	raw_spinlock_t		lock;
	u8			channel;
	int			irq;
	u16			min_apid;
	u16			max_apid;
	u16			max_periph;
	u32			*mapping_table;
	DECLARE_BITMAP(mapping_table_valid, PMIC_ARB_MAX_PERIPHS);
	struct irq_domain	*domain;
	struct spmi_controller	*spmic;
	const struct vspmi_backend_driver_ver_ops *ver_ops;
	u16			*ppid_to_apid;
	u16			last_apid;
	struct apid_data	apid_data[PMIC_ARB_MAX_PERIPHS];
};
static struct vspmi_pmic_arb *the_pa;

/**
 * pmic_arb_ver: version dependent functionality.
 *
 * @ver_str:		version string.
 * @ppid_to_apid:	finds the apid for a given ppid.
 * @fmt_cmd:		formats a GENI/SPMI command.
 * @acc_enable:		offset of SPMI_PIC_ACC_ENABLEn.
 * @irq_status:		offset of SPMI_PIC_IRQ_STATUSn.
 * @irq_clear:		offset of SPMI_PIC_IRQ_CLEARn.
 * @channel_map_offset:	offset of PMIC_ARB_REG_CHNLn
 */
struct vspmi_backend_driver_ver_ops {
	const char *ver_str;
	int (*ppid_to_apid)(struct vspmi_pmic_arb *pa, u8 sid, u16 addr,
			u16 *apid);
	u32 (*fmt_cmd)(u8 opc, u8 sid, u16 addr, u8 bc);
	u32 (*acc_enable)(u16 n);
	u32 (*irq_status)(u16 n);
	u32 (*irq_clear)(u16 n);
	u32 (*channel_map_offset)(u16 n);
};

/**
 * vspmi_pa_read_data: reads vspmi backend's register and copy 1..4 bytes to buf
 * @bc:		byte count -1. range: 0..3
 * @reg:	register's address
 * @buf:	output parameter, length must be bc + 1
 */
static void
vspmi_pa_read_data(struct vspmi_pmic_arb *pa, u8 *buf, u32 reg, u8 bc)
{
	u32 data = __raw_readl(pa->wr_base + reg);

	memcpy(buf, &data, (bc & 3) + 1);
}

/**
 * vspmi_pa_write_data: write 1..4 bytes from buf to vspmi backend's register
 * @bc:		byte-count -1. range: 0..3.
 * @reg:	register's address.
 * @buf:	buffer to write. length must be bc + 1.
 */
static void
vspmi_pa_write_data(struct vspmi_pmic_arb *pa, const u8 *buf, u32 reg, u8 bc)
{
	u32 data = 0;

	memcpy(&data, buf, (bc & 3) + 1);
	writel_relaxed(data, pa->wr_base + reg);
}

static int vspmi_pmic_arb_wait_for_done(struct spmi_controller *ctrl,
				  void __iomem *base, u8 sid, u16 addr,
				  enum pmic_arb_channel ch_type)
{
	u32 status = 0;
	u32 timeout = PMIC_ARB_TIMEOUT_US;
	u32 offset;

	offset = VPMIC_ARB_STATUS;

	while (timeout--) {
		status = readl_relaxed(base + offset);

		if (status & PMIC_ARB_STATUS_DONE) {
			if (status & PMIC_ARB_STATUS_DENIED) {
				dev_err(&ctrl->dev,
					"%s: transaction denied (0x%x)\n",
					__func__, status);
				return -EPERM;
			}

			if (status & PMIC_ARB_STATUS_FAILURE) {
				dev_err(&ctrl->dev,
					"%s: transaction failed (0x%x)\n",
					__func__, status);
				return -EIO;
			}

			if (status & PMIC_ARB_STATUS_DROPPED) {
				dev_err(&ctrl->dev,
					"%s: transaction dropped (0x%x)\n",
					__func__, status);
				return -EIO;
			}

			return 0;
		}
		udelay(1);
	}

	dev_err(&ctrl->dev,
		"%s: timeout, status 0x%x\n",
		__func__, status);
	return -ETIMEDOUT;
}

static int vspmi_pmic_arb_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			     u16 addr, u8 *buf, size_t len)
{
	struct vspmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	unsigned long flags;
	u8 bc = len - 1;
	u32 cmd;
	int rc;

	if (bc >= PMIC_ARB_MAX_TRANS_BYTES) {
		dev_err(&ctrl->dev,
			"pmic-arb supports 1..%d bytes per trans, but:%zu requested",
			PMIC_ARB_MAX_TRANS_BYTES, len);
		return  -EINVAL;
	}

	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7F)
		opc = PMIC_ARB_OP_READ;
	else if (opc >= 0x20 && opc <= 0x2F)
		opc = PMIC_ARB_OP_EXT_READ;
	else if (opc >= 0x38 && opc <= 0x3F)
		opc = PMIC_ARB_OP_EXT_READL;
	else
		return -EINVAL;

	cmd = pa->ver_ops->fmt_cmd(opc, sid, addr, bc);

	raw_spin_lock_irqsave(&pa->lock, flags);
	writel_relaxed(cmd, pa->wr_base + VPMIC_ARB_CMD);
	rc = vspmi_pmic_arb_wait_for_done(ctrl, pa->wr_base, sid, addr,
				    PMIC_ARB_CHANNEL_OBS);
	if (rc)
		goto done;

	vspmi_pa_read_data(pa, buf, VPMIC_ARB_DATA0, min_t(u8, bc, 3));

	if (bc > 3)
		vspmi_pa_read_data(pa, buf + 4, VPMIC_ARB_DATA1, bc - 4);

done:
	raw_spin_unlock_irqrestore(&pa->lock, flags);
	return rc;
}

static int vspmi_pmic_arb_write_cmd(struct spmi_controller *ctrl, u8 opc,
			    u8 sid, u16 addr, const u8 *buf, size_t len)
{
	struct vspmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	unsigned long flags;
	u8 bc = len - 1;
	u32 cmd;
	int rc;

	if (bc >= PMIC_ARB_MAX_TRANS_BYTES) {
		dev_err(&ctrl->dev,
			"pmic-arb supports 1..%d bytes per trans, but:%zu requested",
			PMIC_ARB_MAX_TRANS_BYTES, len);
		return  -EINVAL;
	}

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIC_ARB_OP_WRITE;
	else if (opc <= 0x0F)
		opc = PMIC_ARB_OP_EXT_WRITE;
	else if (opc >= 0x30 && opc <= 0x37)
		opc = PMIC_ARB_OP_EXT_WRITEL;
	else if (opc >= 0x80)
		opc = PMIC_ARB_OP_ZERO_WRITE;
	else
		return -EINVAL;

	cmd = pa->ver_ops->fmt_cmd(opc, sid, addr, bc);

	/* Write data to FIFOs */
	raw_spin_lock_irqsave(&pa->lock, flags);
	vspmi_pa_write_data(pa, buf, VPMIC_ARB_DATA0, min_t(u8, bc, 3));
	if (bc > 3)
		vspmi_pa_write_data(pa, buf + 4, VPMIC_ARB_DATA1, bc - 4);

	/* Start the transaction */
	writel_relaxed(cmd, pa->wr_base + VPMIC_ARB_CMD);
	rc = vspmi_pmic_arb_wait_for_done(ctrl, pa->wr_base, sid, addr,
				    PMIC_ARB_CHANNEL_RW);
	raw_spin_unlock_irqrestore(&pa->lock, flags);

	return rc;
}

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

struct spmi_pmic_arb_qpnpint_type {
	u8 type; /* 1 -> edge */
	u8 polarity_high;
	u8 polarity_low;
} __packed;

/* Simplified accessor functions for irqchip callbacks */
static void qpnpint_spmi_write(struct irq_data *d, u8 reg, void *buf,
			       size_t len)
{
	struct vspmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 sid = HWIRQ_SID(d->hwirq);
	u8 per = HWIRQ_PER(d->hwirq);

	if (vspmi_pmic_arb_write_cmd(pa->spmic, SPMI_CMD_EXT_WRITEL, sid,
			       (per << 8) + reg, buf, len))
		dev_err_ratelimited(&pa->spmic->dev,
				"failed irqchip transaction on %x\n",
				    d->irq);
}

static void qpnpint_spmi_read(struct irq_data *d, u8 reg, void *buf, size_t len)
{
	struct vspmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 sid = HWIRQ_SID(d->hwirq);
	u8 per = HWIRQ_PER(d->hwirq);

	if (vspmi_pmic_arb_read_cmd(pa->spmic, SPMI_CMD_EXT_READL, sid,
			      (per << 8) + reg, buf, len))
		dev_err_ratelimited(&pa->spmic->dev,
				"failed irqchip transaction on %x\n",
				    d->irq);
}

static void cleanup_irq(struct vspmi_pmic_arb *pa, u16 apid, int id)
{
	u16 ppid = pa->apid_data[apid].ppid;
	u8 sid = ppid >> 8;
	u8 per = ppid & 0xFF;
	u8 irq_mask = BIT(id);

	dev_err_ratelimited(&pa->spmic->dev,
		"cleanup_irq apid=%d sid=0x%x per=0x%x irq=%d\n",
		apid, sid, per, id);
	writel_relaxed(irq_mask, pa->intr + pa->ver_ops->irq_clear(apid));
}

static void periph_interrupt(struct vspmi_pmic_arb *pa, u16 apid, bool show)
{
	unsigned int irq;
	u32 status;
	int id;
	u8 sid = (pa->apid_data[apid].ppid >> 8) & 0xF;
	u8 per = pa->apid_data[apid].ppid & 0xFF;

	status = readl_relaxed(pa->intr + pa->ver_ops->irq_status(apid));
	while (status) {
		id = ffs(status) - 1;
		status &= ~BIT(id);
		irq = irq_find_mapping(pa->domain, HWIRQ(sid, per, id, apid));
		if (irq == 0) {
			cleanup_irq(pa, apid, id);
			continue;
		}
		if (show) {
			struct irq_desc *desc;
			const char *name = "null";

			desc = irq_to_desc(irq);
			if (desc == NULL)
				name = "stray irq";
			else if (desc->action && desc->action->name)
				name = desc->action->name;

			pr_warn("spmi_show_resume_irq: %d triggered [0x%01x, 0x%02x, 0x%01x] %s\n",
				irq, sid, per, id, name);
		} else {
			generic_handle_irq(irq);
		}
	}
}

static void __pmic_arb_chained_irq(struct vspmi_pmic_arb *pa, bool show)
{
	u32 enable;
	int i;
	/* status based dispatch */
	bool acc_valid = false;
	u32 irq_status = 0;

	/* ACC_STATUS is empty but IRQ fired check IRQ_STATUS */
	if (!acc_valid) {
		for (i = pa->min_apid; i <= pa->max_apid; i++) {
			irq_status = readl_relaxed(pa->intr +
						pa->ver_ops->irq_status(i));
			if (irq_status) {
				enable = readl_relaxed(pa->intr +
						pa->ver_ops->acc_enable(i));
				if (enable & SPMI_PIC_ACC_ENABLE_BIT) {
					dev_dbg(&pa->spmic->dev,
						"Dispatching IRQ for apid=%d status=%x\n",
						i, irq_status);
					periph_interrupt(pa, i, show);
				}
			}
		}
	}
}

static void pmic_arb_chained_irq(struct irq_desc *desc)
{
	struct vspmi_pmic_arb *pa = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);

	chained_irq_enter(chip, desc);
	__pmic_arb_chained_irq(pa, false);
	chained_irq_exit(chip, desc);
}

static void qpnpint_irq_ack(struct irq_data *d)
{
	struct vspmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u16 apid = HWIRQ_APID(d->hwirq);
	u8 data;

	writel_relaxed(BIT(irq), pa->intr + pa->ver_ops->irq_clear(apid));

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
	struct vspmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u16 apid = HWIRQ_APID(d->hwirq);
	u8 buf[2];

	writel_relaxed(SPMI_PIC_ACC_ENABLE_BIT,
		pa->intr + pa->ver_ops->acc_enable(apid));

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

static int qpnpint_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct spmi_pmic_arb_qpnpint_type type;
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
	.name		= "pmic_arb",
	.irq_ack	= qpnpint_irq_ack,
	.irq_mask	= qpnpint_irq_mask,
	.irq_unmask	= qpnpint_irq_unmask,
	.irq_set_type	= qpnpint_irq_set_type,
	.irq_get_irqchip_state	= qpnpint_get_irqchip_state,
	.flags		= IRQCHIP_MASK_ON_SUSPEND
			| IRQCHIP_SKIP_SET_WAKE,
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
	struct vspmi_pmic_arb *pa = d->host_data;
	int rc;
	u16 apid;

	dev_dbg(&pa->spmic->dev,
		"intspec[0] 0x%1x intspec[1] 0x%02x intspec[2] 0x%02x\n",
		intspec[0], intspec[1], intspec[2]);

	if (irq_domain_get_of_node(d) != controller)
		return -EINVAL;
	if (intsize != 4)
		return -EINVAL;
	if (intspec[0] > 0xF || intspec[1] > 0xFF || intspec[2] > 0x7)
		return -EINVAL;

	rc = pa->ver_ops->ppid_to_apid(pa, intspec[0],
			(intspec[1] << 8), &apid);
	if (rc < 0) {
		dev_err(&pa->spmic->dev,
		"failed to xlate sid = 0x%x, periph = 0x%x, irq = %u rc = %d\n",
		intspec[0], intspec[1], intspec[2], rc);
		return rc;
	}

	/* Keep track of {max,min}_apid for bounding search during interrupt */
	if (apid > pa->max_apid)
		pa->max_apid = apid;
	if (apid < pa->min_apid)
		pa->min_apid = apid;

	*out_hwirq = HWIRQ(intspec[0], intspec[1], intspec[2], apid);
	*out_type  = intspec[3] & IRQ_TYPE_SENSE_MASK;

	dev_dbg(&pa->spmic->dev, "out_hwirq = %lu\n", *out_hwirq);

	return 0;
}

static int qpnpint_irq_domain_map(struct irq_domain *d,
				  unsigned int virq,
				  irq_hw_number_t hwirq)
{
	struct vspmi_pmic_arb *pa = d->host_data;

	dev_dbg(&pa->spmic->dev, "virq = %u, hwirq = %lu\n", virq, hwirq);

	irq_set_chip_and_handler(virq, &pmic_arb_irqchip, handle_level_irq);
	irq_set_chip_data(virq, d->host_data);
	irq_set_noprobe(virq);
	return 0;
}

static u16 pmic_arb_find_apid(struct vspmi_pmic_arb *pa, u16 ppid)
{
	u32 regval, offset;
	u16 apid;
	u16 id;

	/*
	 * PMIC_ARB_REG_CHNL is a table in HW mapping channel to ppid.
	 * ppid_to_apid is an in-memory invert of that table.
	 */
	for (apid = pa->last_apid; apid < pa->max_periph; apid++) {
		offset = pa->ver_ops->channel_map_offset(apid);
		if (offset >= pa->core_size)
			break;

		regval = readl_relaxed(pa->core + offset);
		if (!regval) {
			/* If this regval is 0, it means that this apid is
			 * unused. Write the current ppid to this reg to
			 * use this apid to map to the given ppid.
			 */
			writel_relaxed(ppid, pa->core + offset);
			regval = ppid;
		}

		id = regval & PMIC_ARB_PPID_MASK;
		pa->ppid_to_apid[id] = apid | PMIC_ARB_CHAN_VALID;
		pa->apid_data[apid].ppid = id;
		if (id == ppid) {
			apid |= PMIC_ARB_CHAN_VALID;
			break;
		}
	}
	pa->last_apid = apid & ~PMIC_ARB_CHAN_VALID;

	return apid;
}

static int
pmic_arb_ppid_to_apid_v2(struct vspmi_pmic_arb *pa, u8 sid, u16 addr, u16 *apid)
{
	u16 ppid = (sid << 8) | (addr >> 8);
	u16 apid_valid;

	apid_valid = pa->ppid_to_apid[ppid];
	if (!(apid_valid & PMIC_ARB_CHAN_VALID))
		apid_valid = pmic_arb_find_apid(pa, ppid);
	if (!(apid_valid & PMIC_ARB_CHAN_VALID))
		return -ENODEV;

	*apid = (apid_valid & ~PMIC_ARB_CHAN_VALID);
	return 0;
}

static u32 vspmi_pmic_arb_fmt_cmd_v1(u8 opc, u8 sid, u16 addr, u8 bc)
{
	return (opc << 27) | ((sid & 0xf) << 20) | (addr << 4) |
		   ((bc & 0x7) + 1);
}

static u32 pmic_arb_acc_enable_v2(u16 n)
{
	return 0x1000 * n;
}

static u32 pmic_arb_irq_status_v2(u16 n)
{
	return 0x4 + 0x1000 * n;
}

static u32 pmic_arb_irq_clear_v2(u16 n)
{
	return 0x8 + 0x1000 * n;
}

static u32 pmic_arb_channel_map_offset_v2(u16 n)
{
	return 0x800 + 0x4 * n;
}

static const struct vspmi_backend_driver_ver_ops pmic_arb_v1 = {
	.ver_str		= "v1",
	.ppid_to_apid		= pmic_arb_ppid_to_apid_v2,
	.fmt_cmd		= vspmi_pmic_arb_fmt_cmd_v1,
	.acc_enable		= pmic_arb_acc_enable_v2,
	.irq_status		= pmic_arb_irq_status_v2,
	.irq_clear		= pmic_arb_irq_clear_v2,
	.channel_map_offset	= pmic_arb_channel_map_offset_v2,
};

static const struct irq_domain_ops pmic_arb_irq_domain_ops = {
	.map	= qpnpint_irq_domain_map,
	.xlate	= qpnpint_irq_domain_dt_translate,
	.activate	= qpnpint_irq_domain_activate,
};

static int vspmi_pmic_arb_probe(struct platform_device *pdev)
{
	struct vspmi_pmic_arb *pa;
	struct spmi_controller *ctrl;
	struct resource *res;
	u32 backend_ver;
	u32 channel;
	int err;

	ctrl = spmi_controller_alloc(&pdev->dev, sizeof(*pa));
	if (!ctrl)
		return -ENOMEM;

	pa = spmi_controller_get_drvdata(ctrl);
	pa->spmic = ctrl;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "core");
	if (!res) {
		dev_err(&pdev->dev, "vdev resource not specified\n");
		err = -EINVAL;
		goto err_put_ctrl;
	}

	pa->core = devm_ioremap_resource(&ctrl->dev, res);
	if (IS_ERR(pa->core)) {
		err = PTR_ERR(pa->core);
		goto err_put_ctrl;
	}
	pa->core_size = resource_size(res);

	backend_ver = VPMIC_ARB_VERSION;

	if (backend_ver == VPMIC_ARB_VERSION)
		pa->ver_ops = &pmic_arb_v1;

	/* the apid to ppid table starts at PMIC_ARB_REG_CHNL0 */
	pa->max_periph
	     = (pa->core_size - pa->ver_ops->channel_map_offset(0)) / 4;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "chnls");
	pa->wr_base = devm_ioremap_resource(&ctrl->dev, res);
	if (IS_ERR(pa->wr_base)) {
		err = PTR_ERR(pa->wr_base);
		goto err_put_ctrl;
	}

	pa->ppid_to_apid = devm_kcalloc(&ctrl->dev,
					PMIC_ARB_MAX_PPID,
					sizeof(*pa->ppid_to_apid),
					GFP_KERNEL);
	if (!pa->ppid_to_apid) {
		err = -ENOMEM;
		goto err_put_ctrl;
	}


	dev_info(&ctrl->dev, "PMIC arbiter version %s (0x%x)\n",
		 pa->ver_ops->ver_str, backend_ver);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "intr");
	pa->intr = devm_ioremap_resource(&ctrl->dev, res);
	if (IS_ERR(pa->intr)) {
		err = PTR_ERR(pa->intr);
		goto err_put_ctrl;
	}
	pa->acc_status = pa->intr;

	pa->irq = platform_get_irq_byname(pdev, "periph_irq");
	if (pa->irq < 0) {
		err = pa->irq;
		goto err_put_ctrl;
	}

	err = of_property_read_u32(pdev->dev.of_node, "qcom,channel", &channel);
	if (err) {
		dev_err(&pdev->dev, "channel unspecified.\n");
		goto err_put_ctrl;
	}

	if (channel > 5) {
		dev_err(&pdev->dev, "invalid channel (%u) specified.\n",
			channel);
		err = -EINVAL;
		goto err_put_ctrl;
	}

	pa->channel = channel;

	pa->mapping_table = devm_kcalloc(&ctrl->dev, PMIC_ARB_MAX_PERIPHS - 1,
					sizeof(*pa->mapping_table), GFP_KERNEL);
	if (!pa->mapping_table) {
		err = -ENOMEM;
		goto err_put_ctrl;
	}

	/* Initialize max_apid/min_apid to the opposite bounds, during
	 * the irq domain translation, we are sure to update these.
	 */
	pa->max_apid = 0;
	pa->min_apid = PMIC_ARB_MAX_PERIPHS - 1;

	platform_set_drvdata(pdev, ctrl);
	raw_spin_lock_init(&pa->lock);

	ctrl->read_cmd = vspmi_pmic_arb_read_cmd;
	ctrl->write_cmd = vspmi_pmic_arb_write_cmd;

	dev_dbg(&pdev->dev, "adding irq domain\n");
	pa->domain = irq_domain_add_tree(pdev->dev.of_node,
					 &pmic_arb_irq_domain_ops, pa);
	if (!pa->domain) {
		dev_err(&pdev->dev, "unable to create irq_domain\n");
		err = -ENOMEM;
		goto err_put_ctrl;
	}

	irq_set_chained_handler_and_data(pa->irq, pmic_arb_chained_irq, pa);
	enable_irq_wake(pa->irq);

	err = spmi_controller_add(ctrl);
	if (err)
		goto err_domain_remove;

	the_pa = pa;
	return 0;

err_domain_remove:
	irq_set_chained_handler_and_data(pa->irq, NULL, NULL);
	irq_domain_remove(pa->domain);
err_put_ctrl:
	spmi_controller_put(ctrl);
	return err;
}

static int vspmi_pmic_arb_remove(struct platform_device *pdev)
{
	struct spmi_controller *ctrl = platform_get_drvdata(pdev);

	spmi_controller_remove(ctrl);
	the_pa = NULL;
	spmi_controller_put(ctrl);
	return 0;
}

static const struct of_device_id vspmi_pmic_arb_match_table[] = {
	{ .compatible = "qcom,virtspmi-pmic-arb", },
	{},
};
MODULE_DEVICE_TABLE(of, vspmi_pmic_arb_match_table);

static struct platform_driver vspmi_pmic_arb_driver = {
	.probe		= vspmi_pmic_arb_probe,
	.remove		= vspmi_pmic_arb_remove,
	.driver		= {
		.name	= "virtspmi_pmic_arb",
		.of_match_table = vspmi_pmic_arb_match_table,
	},
};

static int __init vspmi_pmic_arb_init(void)
{
	return platform_driver_register(&vspmi_pmic_arb_driver);
}
arch_initcall(vspmi_pmic_arb_init);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:virtspmi_pmic_arb");
