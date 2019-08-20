/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <linux/mutex.h>

#include <linux/virtio.h>
#include <linux/virtio_spmi.h>
#include <linux/scatterlist.h>

/* Mapping Table */
#define PMIC_ARB_MAX_PPID		BIT(12) /* PPID is 12bit */
#define PMIC_ARB_APID_VALID		BIT(15)
#define PMIC_ARB_CHAN_IS_IRQ_OWNER(reg)	((reg) & BIT(24))
#define INVALID_EE				0xFF

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

/* Maximum number of support PMIC peripherals */
#define PMIC_ARB_MAX_PERIPHS		512
#define PMIC_ARB_MAX_TRANS_BYTES	(8)

#define PMIC_ARB_APID_MASK		0xFF
#define PMIC_ARB_PPID_MASK		0xFFF

/* interrupt enable bit */
#define SPMI_PIC_ACC_ENABLE_BIT		BIT(0)

#define spec_to_hwirq(slave_id, periph_id, irq_id, apid) \
	((((slave_id) & 0xF)   << 28) | \
	(((periph_id) & 0xFF)  << 20) | \
	(((irq_id)    & 0x7)   << 16) | \
	(((apid)      & 0x1FF) << 0))

#define hwirq_to_sid(hwirq)  (((hwirq) >> 28) & 0xF)
#define hwirq_to_per(hwirq)  (((hwirq) >> 20) & 0xFF)
#define hwirq_to_irq(hwirq)  (((hwirq) >> 16) & 0x7)
#define hwirq_to_apid(hwirq) (((hwirq) >> 0)  & 0x1FF)

struct pmic_arb_ver_ops;

struct apid_data {
	u16		ppid;
	u8		write_ee;
	u8		irq_ee;
};

struct virtio_spmi {
	struct virtio_device	*vdev;
	struct virtqueue		*vq;
	spinlock_t              lock;
	struct virtio_spmi_config config;
	struct spmi_pmic_arb *pa;
};
static struct virtio_spmi *g_vspmi;

/**
 * spmi_pmic_arb - SPMI PMIC Arbiter object
 *
 * @irq:		PMIC ARB interrupt.
 * @ee:			the current Execution Environment
 * @min_apid:		minimum APID (used for bounding IRQ search)
 * @max_apid:		maximum APID
 * @domain:		irq domain object for PMIC IRQ domain
 * @spmic:		SPMI controller object
 * @ver_ops:		version dependent operations.
 * @ppid_to_apid	in-memory copy of PPID -> APID mapping table.
 */
struct spmi_pmic_arb {
	int			irq;
	u8			ee;
	u16			min_apid;
	u16			max_apid;
	struct irq_domain	*domain;
	struct spmi_controller	*spmic;
	const struct pmic_arb_ver_ops *ver_ops;
	u16			*ppid_to_apid;
	struct apid_data	apid_data[PMIC_ARB_MAX_PERIPHS];
};

/**
 * pmic_arb_ver: version dependent functionality.
 *
 * @ver_str:		version string.
 * @ppid_to_apid:	finds the apid for a given ppid.
 * @fmt_cmd:		formats a GENI/SPMI command.
 */
struct pmic_arb_ver_ops {
	const char *ver_str;
	int (*ppid_to_apid)(struct spmi_pmic_arb *pa, u16 ppid);
	u32 (*fmt_cmd)(u8 opc, u8 sid, u16 addr, u8 bc);
};

static int
vspmi_pmic_arb_xfer(struct spmi_pmic_arb *pa, struct virtio_spmi_msg *req)
{
	struct scatterlist sg[1];
	struct virtio_spmi_msg *rsp;
	unsigned int vqlen;
	int rc = 0;
	unsigned long flags;

	sg_init_one(sg, req, virtio32_to_cpu(g_vspmi->vdev, req->len));

	spin_lock_irqsave(&g_vspmi->lock, flags);
	rc = virtqueue_add_outbuf(g_vspmi->vq, sg, 1, req, GFP_ATOMIC);
	if (rc) {
		dev_err(&g_vspmi->vdev->dev, "fail to add output buffer\n");
		goto out;
	}

	virtqueue_kick(g_vspmi->vq);

	do {
		rsp = virtqueue_get_buf(g_vspmi->vq, &vqlen);
	} while (!rsp);
	rc = virtio32_to_cpu(g_vspmi->vdev, rsp->res);

out:
	spin_unlock_irqrestore(&g_vspmi->lock, flags);
	return rc;
}

static struct virtio_spmi_msg *vspmi_init_msg(u32 len, u32 type, u32 u)
{
	struct virtio_spmi_msg *req = NULL;

	req = kzalloc(len, GFP_ATOMIC);
	if (req) {
		req->len = cpu_to_virtio32(g_vspmi->vdev, len);
		req->type = cpu_to_virtio32(g_vspmi->vdev, type);
		req->u.cnt = req->u.cmd = cpu_to_virtio32(g_vspmi->vdev, u);
	} else {
		dev_err(&g_vspmi->vdev->dev, "no atomic mem\n");
	}

	dev_dbg(&g_vspmi->vdev->dev, "len:%u type:%u u:%x\n", len, type, u);
	return req;
}

static void
vspmi_fill_one(struct virtio_spmi_msg *req, u32 ppid, u32 val)
{
	u32 idx = virtio32_to_cpu(g_vspmi->vdev, req->u.cnt);
	u32 type = virtio32_to_cpu(g_vspmi->vdev, req->type);

	req->payload[idx].irqd.ppid =
		cpu_to_virtio32(g_vspmi->vdev, ppid);
	if ((type == VIO_IRQ_CLEAR) || (type == VIO_ACC_ENABLE_WR))
		req->payload[idx].irqd.val =
			cpu_to_virtio32(g_vspmi->vdev, val);

	dev_dbg(&g_vspmi->vdev->dev, "spmi: cnt:%u ppid:%u val:%u\n", idx,
		virtio32_to_cpu(g_vspmi->vdev, req->payload[idx].irqd.ppid),
		virtio32_to_cpu(g_vspmi->vdev, req->payload[idx].irqd.val));

	req->u.cnt = cpu_to_virtio32(g_vspmi->vdev, (idx + 1));
}

static int pmic_arb_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			     u16 addr, u8 *buf, size_t len)
{
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	struct virtio_spmi_msg *req;
	u8 bc = len - 1;
	u32 data, cmd;
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

	req = vspmi_init_msg(MSG_SZ(1), VIO_SPMI_BUS_READ, cmd);
	if (!req)
		return -ENOMEM;

	rc = vspmi_pmic_arb_xfer(pa, req);
	if (rc)
		goto out;

	data = virtio32_to_cpu(g_vspmi->vdev,
			req->payload[0].cmdd.data[0]);
	memcpy(buf, &data, (bc & 3) + 1);

	if (bc > 3) {
		data = virtio32_to_cpu(g_vspmi->vdev,
				req->payload[0].cmdd.data[1]);
		memcpy((buf + 4), &data, ((bc - 4) & 3) + 1);
	}

out:
	kfree(req);
	return rc;
}

static int pmic_arb_write_cmd(struct spmi_controller *ctrl, u8 opc,
			    u8 sid, u16 addr, const u8 *buf, size_t len)
{
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	struct virtio_spmi_msg *req;
	u8 bc = len - 1;
	u32 data, cmd;
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

	req = vspmi_init_msg(MSG_SZ(1), VIO_SPMI_BUS_WRITE, cmd);
	if (!req)
		return -ENOMEM;

	memcpy(&data, buf, (bc & 3) + 1);
	req->payload[0].cmdd.data[0] = cpu_to_virtio32(g_vspmi->vdev, data);
	if (bc > 3) {
		memcpy(&data, (buf + 4), ((bc - 4) & 3) + 1);
		req->payload[0].cmdd.data[1] =
			cpu_to_virtio32(g_vspmi->vdev, data);
	}

	rc = vspmi_pmic_arb_xfer(pa, req);

	kfree(req);
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
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 sid = hwirq_to_sid(d->hwirq);
	u8 per = hwirq_to_per(d->hwirq);

	if (pmic_arb_write_cmd(pa->spmic, SPMI_CMD_EXT_WRITEL, sid,
			       (per << 8) + reg, buf, len))
		dev_err_ratelimited(&pa->spmic->dev,
				"failed irqchip transaction on %x\n", d->irq);
}

static void qpnpint_spmi_read(struct irq_data *d, u8 reg, void *buf, size_t len)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 sid = hwirq_to_sid(d->hwirq);
	u8 per = hwirq_to_per(d->hwirq);

	if (pmic_arb_read_cmd(pa->spmic, SPMI_CMD_EXT_READL, sid,
			      (per << 8) + reg, buf, len))
		dev_err_ratelimited(&pa->spmic->dev,
				"failed irqchip transaction on %x\n", d->irq);
}

static void cleanup_irq(struct spmi_pmic_arb *pa, u16 apid, int id)
{
	u16 ppid = pa->apid_data[apid].ppid;
	u8 sid = ppid >> 8;
	u8 per = ppid & 0xFF;
	u8 irq_mask = BIT(id);

	struct virtio_spmi_msg *req;

	req = vspmi_init_msg(MSG_SZ(1), VIO_IRQ_CLEAR, 0);
	if (req) {
		dev_err_ratelimited(&pa->spmic->dev,
			"%s apid=%d sid=0x%x per=0x%x irq=%d\n",
			__func__, apid, sid, per, id);

		vspmi_fill_one(req, ppid, irq_mask);
		vspmi_pmic_arb_xfer(pa, req);
		kfree(req);
	}
}

static void periph_interrupt(struct spmi_pmic_arb *pa, u16 apid)
{
	unsigned int irq;
	u32 status, id;
	u8 sid = (pa->apid_data[apid].ppid >> 8) & 0xF;
	u8 per = pa->apid_data[apid].ppid & 0xFF;

	struct virtio_spmi_msg *req;

	req = vspmi_init_msg(MSG_SZ(1), VIO_IRQ_STATUS, 0);
	if (!req)
		return;

	vspmi_fill_one(req, pa->apid_data[apid].ppid, 0);
	if (vspmi_pmic_arb_xfer(pa, req)) {
		kfree(req);
		return;
	}

	status = virtio32_to_cpu(g_vspmi->vdev,
			req->payload[0].irqd.val);
	kfree(req);

	while (status) {
		id = ffs(status) - 1;
		status &= ~BIT(id);
		irq = irq_find_mapping(pa->domain,
					spec_to_hwirq(sid, per, id, apid));
		if (irq == 0) {
			cleanup_irq(pa, apid, id);
			continue;
		}
		generic_handle_irq(irq);
	}
}

static void pmic_arb_chained_irq(struct irq_desc *desc)
{
	struct spmi_pmic_arb *pa = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);

	u32 enable;
	int i;
	u16 ppid;

	struct virtio_spmi_msg *req;
	u32 *irq_status;

	irq_status = kzalloc((pa->max_apid + 1) * sizeof(u32), GFP_ATOMIC);
	if (!irq_status)
		return;

	chained_irq_enter(chip, desc);

	/* get all irq_status */
	req = vspmi_init_msg(MSG_SZ(pa->max_apid + 1), VIO_IRQ_STATUS, 0);
	if (req) {
		for (i = pa->min_apid; i <= pa->max_apid; i++) {
			ppid = pa->apid_data[i].ppid;
			vspmi_fill_one(req, ppid, 0);
		}
	} else
		goto out;

	if (vspmi_pmic_arb_xfer(pa, req)) {
		kfree(req);
		goto out;
	}

	for (i = pa->min_apid; i <= pa->max_apid; i++)
		irq_status[i] = virtio32_to_cpu(g_vspmi->vdev,
				req->payload[i].irqd.val);
	kfree(req);

	/* ACC_STATUS is empty but IRQ fired check IRQ_STATUS */
	for (i = pa->min_apid; i <= pa->max_apid; i++) {
		ppid = pa->apid_data[i].ppid;

		if (irq_status[i]) {
			req = vspmi_init_msg(MSG_SZ(1), VIO_ACC_ENABLE_RD, 0);
			if (!req)
				goto out;

			vspmi_fill_one(req, ppid, 0);
			if (vspmi_pmic_arb_xfer(pa, req))
				goto out;

			enable = virtio32_to_cpu(g_vspmi->vdev,
					req->payload[0].irqd.val);
			kfree(req);

			if (enable & SPMI_PIC_ACC_ENABLE_BIT) {
				dev_dbg(&pa->spmic->dev,
					"Dispatching IRQ for apid=%d status=%x\n",
					i, irq_status[i]);
				periph_interrupt(pa, i);
			}
		}
	}

out:
	chained_irq_exit(chip, desc);
	kfree(irq_status);
}

static void qpnpint_irq_ack(struct irq_data *d)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 irq = hwirq_to_irq(d->hwirq);
	u16 apid = hwirq_to_apid(d->hwirq);
	u16 ppid = pa->apid_data[apid].ppid;
	u8 data;

	struct virtio_spmi_msg *req;

	req = vspmi_init_msg(MSG_SZ(1), VIO_IRQ_CLEAR, 0);
	if (req) {
		vspmi_fill_one(req, ppid, BIT(irq));
		vspmi_pmic_arb_xfer(pa, req);
		kfree(req);
	} else
		return;

	data = BIT(irq);
	qpnpint_spmi_write(d, QPNPINT_REG_LATCHED_CLR, &data, 1);
}

static void qpnpint_irq_mask(struct irq_data *d)
{
	u8 irq = hwirq_to_irq(d->hwirq);
	u8 data = BIT(irq);

	qpnpint_spmi_write(d, QPNPINT_REG_EN_CLR, &data, 1);
}

static void qpnpint_irq_unmask(struct irq_data *d)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 irq = hwirq_to_irq(d->hwirq);
	u16 apid = hwirq_to_apid(d->hwirq);
	u16 ppid = pa->apid_data[apid].ppid;
	u8 buf[2];

	struct virtio_spmi_msg *req;

	req = vspmi_init_msg(MSG_SZ(1), VIO_ACC_ENABLE_WR, 0);
	if (req) {
		vspmi_fill_one(req, ppid, SPMI_PIC_ACC_ENABLE_BIT);
		vspmi_pmic_arb_xfer(pa, req);
		kfree(req);
	} else
		return;

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
	irq_flow_handler_t flow_handler;
	u8 irq = hwirq_to_irq(d->hwirq);

	qpnpint_spmi_read(d, QPNPINT_REG_SET_TYPE, &type, sizeof(type));

	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		type.type |= BIT(irq);
		if (flow_type & IRQF_TRIGGER_RISING)
			type.polarity_high |= BIT(irq);
		else
			type.polarity_high &= ~BIT(irq);
		if (flow_type & IRQF_TRIGGER_FALLING)
			type.polarity_low  |= BIT(irq);
		else
			type.polarity_low  &= ~BIT(irq);

		flow_handler = handle_edge_irq;
	} else {
		if ((flow_type & (IRQF_TRIGGER_HIGH)) &&
		    (flow_type & (IRQF_TRIGGER_LOW)))
			return -EINVAL;

		type.type &= ~BIT(irq); /* level trig */
		if (flow_type & IRQF_TRIGGER_HIGH) {
			type.polarity_high |= BIT(irq);
			type.polarity_low  &= ~BIT(irq);
		} else {
			type.polarity_low  |= BIT(irq);
			type.polarity_high &= ~BIT(irq);
		}

		flow_handler = handle_level_irq;
	}

	qpnpint_spmi_write(d, QPNPINT_REG_SET_TYPE, &type, sizeof(type));
	irq_set_handler_locked(d, flow_handler);

	return 0;
}

static int qpnpint_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);

	return irq_set_irq_wake(pa->irq, on);
}

static int qpnpint_get_irqchip_state(struct irq_data *d,
				     enum irqchip_irq_state which,
				     bool *state)
{
	u8 irq = hwirq_to_irq(d->hwirq);
	u8 status = 0;

	if (which != IRQCHIP_STATE_LINE_LEVEL)
		return -EINVAL;

	qpnpint_spmi_read(d, QPNPINT_REG_RT_STS, &status, 1);
	*state = !!(status & BIT(irq));

	return 0;
}

static int qpnpint_irq_request_resources(struct irq_data *d)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u16 periph = hwirq_to_per(d->hwirq);
	u16 apid = hwirq_to_apid(d->hwirq);
	u16 sid = hwirq_to_sid(d->hwirq);
	u16 irq = hwirq_to_irq(d->hwirq);

	if (pa->apid_data[apid].irq_ee != pa->ee) {
		dev_err(&pa->spmic->dev, "failed to xlate sid = %#x, periph = %#x, irq = %u: ee=%u but owner=%u\n",
			sid, periph, irq, pa->ee,
			pa->apid_data[apid].irq_ee);
		return -ENODEV;
	}

	return 0;
}

static struct irq_chip pmic_arb_irqchip = {
	.name		= "pmic_arb",
	.irq_ack	= qpnpint_irq_ack,
	.irq_mask	= qpnpint_irq_mask,
	.irq_unmask	= qpnpint_irq_unmask,
	.irq_set_type	= qpnpint_irq_set_type,
	.irq_set_wake	= qpnpint_irq_set_wake,
	.irq_get_irqchip_state	= qpnpint_get_irqchip_state,
	.irq_request_resources = qpnpint_irq_request_resources,
	.flags		= IRQCHIP_MASK_ON_SUSPEND,
};

static void qpnpint_irq_domain_activate(struct irq_domain *domain,
					struct irq_data *d)
{
	u8 irq = hwirq_to_irq(d->hwirq);
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
	struct spmi_pmic_arb *pa = d->host_data;
	u16 apid, ppid;
	int rc;

	dev_dbg(&pa->spmic->dev, "intspec[0] 0x%1x intspec[1] 0x%02x intspec[2] 0x%02x\n",
		intspec[0], intspec[1], intspec[2]);

	if (irq_domain_get_of_node(d) != controller)
		return -EINVAL;
	if (intsize != 4)
		return -EINVAL;
	if (intspec[0] > 0xF || intspec[1] > 0xFF || intspec[2] > 0x7)
		return -EINVAL;

	ppid = intspec[0] << 8 | intspec[1];
	rc = pa->ver_ops->ppid_to_apid(pa, ppid);
	if (rc < 0) {
		dev_err(&pa->spmic->dev, "failed to xlate sid = %#x, periph = %#x, irq = %u rc = %d\n",
		intspec[0], intspec[1], intspec[2], rc);
		return rc;
	}

	apid = rc;
	/* Keep track of {max,min}_apid for bounding search during interrupt */
	if (apid > pa->max_apid)
		pa->max_apid = apid;
	if (apid < pa->min_apid)
		pa->min_apid = apid;

	*out_hwirq = spec_to_hwirq(intspec[0], intspec[1], intspec[2], apid);
	*out_type  = intspec[3] & IRQ_TYPE_SENSE_MASK;

	dev_dbg(&pa->spmic->dev, "out_hwirq = %lu\n", *out_hwirq);

	return 0;
}

static int qpnpint_irq_domain_map(struct irq_domain *d,
				  unsigned int virq,
				  irq_hw_number_t hwirq)
{
	struct spmi_pmic_arb *pa = d->host_data;

	dev_dbg(&pa->spmic->dev, "virq = %u, hwirq = %lu\n", virq, hwirq);

	irq_set_chip_and_handler(virq, &pmic_arb_irqchip, handle_level_irq);
	irq_set_chip_data(virq, d->host_data);
	irq_set_noprobe(virq);
	return 0;
}

static int pmic_arb_read_apid_map_v5(struct spmi_pmic_arb *pa)
{
	struct apid_data *apidd = pa->apid_data;
	u16 apid, ppid;
	u16 i;

	for (i = 0; i < VM_MAX_PERIPHS; i++) {
		ppid = g_vspmi->config.ppid_allowed[i];
		if (!ppid)
			break;

		apid = i;
		pa->ppid_to_apid[ppid] = apid | PMIC_ARB_APID_VALID;
		pa->apid_data[apid].ppid = ppid;
		pa->apid_data[apid].irq_ee = 0;
		pa->apid_data[apid].write_ee = 0;
	}

	/* Dump the mapping table for debug purposes. */
	dev_dbg(&pa->spmic->dev, "PPID APID Write-EE IRQ-EE\n");
	for (ppid = 0; ppid < PMIC_ARB_MAX_PPID; ppid++) {
		apid = pa->ppid_to_apid[ppid];
		if (apid & PMIC_ARB_APID_VALID) {
			apid &= ~PMIC_ARB_APID_VALID;
			apidd = &pa->apid_data[apid];
			dev_dbg(&pa->spmic->dev, "%#03X %3u %2u %2u\n",
			      ppid, apid, apidd->write_ee, apidd->irq_ee);
		}
	}

	return 0;
}

static int pmic_arb_ppid_to_apid_v5(struct spmi_pmic_arb *pa, u16 ppid)
{
	if (!(pa->ppid_to_apid[ppid] & PMIC_ARB_APID_VALID))
		return -ENODEV;

	return pa->ppid_to_apid[ppid] & ~PMIC_ARB_APID_VALID;
}

static u32 pmic_arb_fmt_cmd_v1(u8 opc, u8 sid, u16 addr, u8 bc)
{
	return (opc << 27) | ((sid & 0xf) << 20) | (addr << 4) | (bc & 0x7);
}

static const struct pmic_arb_ver_ops pmic_arb_v5 = {
	.ver_str		= "v5",
	.ppid_to_apid	= pmic_arb_ppid_to_apid_v5,
	.fmt_cmd		= pmic_arb_fmt_cmd_v1,
};

static const struct irq_domain_ops pmic_arb_irq_domain_ops = {
	.map	= qpnpint_irq_domain_map,
	.xlate	= qpnpint_irq_domain_dt_translate,
	.activate	= qpnpint_irq_domain_activate,
};

static int spmi_pmic_arb_probe(struct platform_device *pdev)
{
	struct spmi_pmic_arb *pa;
	struct spmi_controller *ctrl;
	int err;
	u32 ee;

	if (!g_vspmi)
		return -EINVAL;

	ctrl = spmi_controller_alloc(&pdev->dev, sizeof(*pa));
	if (!ctrl)
		return -ENOMEM;

	pa = spmi_controller_get_drvdata(ctrl);
	pa->spmic = ctrl;
	g_vspmi->pa = pa;

	pa->ver_ops = &pmic_arb_v5;
	dev_info(&ctrl->dev, "Virtio PMIC arbiter\n");

	pa->irq = platform_get_irq_byname(pdev, "periph_irq");
	if (pa->irq < 0) {
		err = pa->irq;
		goto err_put_ctrl;
	}

	err = of_property_read_u32(pdev->dev.of_node, "qcom,ee", &ee);
	if (err) {
		dev_err(&pdev->dev, "EE unspecified.\n");
		goto err_put_ctrl;
	}
	pa->ee = ee;

	pa->ppid_to_apid = devm_kcalloc(&ctrl->dev, PMIC_ARB_MAX_PPID,
					      sizeof(*pa->ppid_to_apid),
					      GFP_KERNEL);
	if (!pa->ppid_to_apid) {
		err = -ENOMEM;
		goto err_put_ctrl;
	}

	/* Initialize max_apid/min_apid to the opposite bounds, during
	 * the irq domain translation, we are sure to update these
	 */
	pa->max_apid = 0;
	pa->min_apid = PMIC_ARB_MAX_PERIPHS - 1;

	platform_set_drvdata(pdev, ctrl);

	ctrl->read_cmd = pmic_arb_read_cmd;
	ctrl->write_cmd = pmic_arb_write_cmd;

	err = pmic_arb_read_apid_map_v5(pa);
	if (err) {
		dev_err(&pdev->dev, "could not read APID->PPID mapping table, rc= %d\n",
			err);
		goto err_put_ctrl;
	}

	dev_dbg(&pdev->dev, "adding irq domain\n");
	pa->domain = irq_domain_add_tree(pdev->dev.of_node,
					 &pmic_arb_irq_domain_ops, pa);
	if (!pa->domain) {
		dev_err(&pdev->dev, "unable to create irq_domain\n");
		err = -ENOMEM;
		goto err_put_ctrl;
	}

	irq_set_chained_handler_and_data(pa->irq, pmic_arb_chained_irq, pa);
	err = spmi_controller_add(ctrl);
	if (err)
		goto err_domain_remove;

	return 0;

err_domain_remove:
	irq_set_chained_handler_and_data(pa->irq, NULL, NULL);
	irq_domain_remove(pa->domain);
err_put_ctrl:
	spmi_controller_put(ctrl);
	return err;
}

static int spmi_pmic_arb_remove(struct platform_device *pdev)
{
	struct spmi_controller *ctrl = platform_get_drvdata(pdev);
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);

	spmi_controller_remove(ctrl);
	irq_set_chained_handler_and_data(pa->irq, NULL, NULL);
	irq_domain_remove(pa->domain);
	spmi_controller_put(ctrl);
	return 0;
}

static const struct of_device_id spmi_pmic_arb_match_table[] = {
	{ .compatible = "qcom,viospmi-pmic-arb", },
	{},
};
MODULE_DEVICE_TABLE(of, spmi_pmic_arb_match_table);

static struct platform_driver spmi_pmic_arb_driver = {
	.probe		= spmi_pmic_arb_probe,
	.remove		= spmi_pmic_arb_remove,
	.driver		= {
		.name	= "viospmi_pmic_arb",
		.of_match_table = spmi_pmic_arb_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

static void virtio_spmi_isr(struct virtqueue *vq) { }

static int virtio_spmi_init_vqs(struct virtio_spmi *vspmi)
{
	struct virtqueue *vqs[1];
	vq_callback_t *cbs[] = { virtio_spmi_isr };
	static const char * const names[] = { "virtio_spmi_isr" };
	int rc;

	rc = virtio_find_vqs(vspmi->vdev, 1, vqs, cbs, names, NULL);
	if (rc)
		return rc;

	vspmi->vq = vqs[0];

	return 0;
}

static void virtio_spmi_del_vqs(struct virtio_spmi *vspmi)
{
	vspmi->vdev->config->del_vqs(vspmi->vdev);
}

static int virtio_spmi_probe(struct virtio_device *vdev)
{
	struct virtio_spmi *vspmi;
	int i;
	int ret = 0;
	u32 val;

	if (!virtio_has_feature(vdev, VIRTIO_F_VERSION_1))
		return -ENODEV;

	vspmi = devm_kzalloc(&vdev->dev, sizeof(*vspmi), GFP_KERNEL);
	if (!vspmi)
		return -ENOMEM;

	vdev->priv = vspmi;
	vspmi->vdev = vdev;
	spin_lock_init(&vspmi->lock);

	ret = virtio_spmi_init_vqs(vspmi);
	if (ret)
		goto err_init_vq;

	virtio_device_ready(vdev);

	memset(&vspmi->config, 0x0, sizeof(vspmi->config));

	for (i = 0; i < VM_MAX_PERIPHS; i += 2) {
		val = virtio_cread32(vdev,
			offsetof(struct virtio_spmi_config, ppid_allowed[i]));
		vspmi->config.ppid_allowed[i] = val & PMIC_ARB_PPID_MASK;
		vspmi->config.ppid_allowed[i + 1] =
					(val >> 16) & PMIC_ARB_PPID_MASK;
		if ((!vspmi->config.ppid_allowed[i]) ||
				!(vspmi->config.ppid_allowed[i + 1]))
			break;
	}

	g_vspmi = vspmi;

	return platform_driver_register(&spmi_pmic_arb_driver);

err_init_vq:
	virtio_spmi_del_vqs(vspmi);
	devm_kfree(&vdev->dev, vspmi);
	return ret;
}

static void virtio_spmi_remove(struct virtio_device *vdev)
{
	struct virtio_spmi *vspmi = vdev->priv;

	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
	devm_kfree(&vdev->dev, vspmi);
	g_vspmi = NULL;
}

static unsigned int features[] = {
	VIRTIO_SPMI_F_INT,
};

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_SPMI, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_spmi_driver = {
	.feature_table		= features,
	.feature_table_size	= ARRAY_SIZE(features),
	.driver.name		= KBUILD_MODNAME,
	.driver.owner		= THIS_MODULE,
	.id_table		= id_table,
	.probe		= virtio_spmi_probe,
	.remove		= virtio_spmi_remove,
};

static int __init virtio_spmi_init(void)
{
	return register_virtio_driver(&virtio_spmi_driver);
}

static void __exit virtio_spmi_exit(void)
{
	unregister_virtio_driver(&virtio_spmi_driver);
}

subsys_initcall(virtio_spmi_init);
module_exit(virtio_spmi_exit);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("virtio spmi_pmic_arb frontend driver");
MODULE_LICENSE("GPL v2");
