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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/of_spmi.h>
#include <linux/module.h>
#include <mach/qpnp-int.h>

#define SPMI_PMIC_ARB_NAME		"spmi_pmic_arb"

/* PMIC Arbiter configuration registers */
#define PMIC_ARB_VERSION		0x0000
#define PMIC_ARB_INT_EN			0x0004

/* PMIC Arbiter channel registers */
#define PMIC_ARB_CMD(N)			(0x0800 + (0x80 * (N)))
#define PMIC_ARB_CONFIG(N)		(0x0804 + (0x80 * (N)))
#define PMIC_ARB_STATUS(N)		(0x0808 + (0x80 * (N)))
#define PMIC_ARB_WDATA0(N)		(0x0810 + (0x80 * (N)))
#define PMIC_ARB_WDATA1(N)		(0x0814 + (0x80 * (N)))
#define PMIC_ARB_RDATA0(N)		(0x0818 + (0x80 * (N)))
#define PMIC_ARB_RDATA1(N)		(0x081C + (0x80 * (N)))

/* Interrupt Controller */
#define SPMI_PIC_OWNER_ACC_STATUS(M, N)	(0x0000 + ((32 * (M)) + (4 * (N))))
#define SPMI_PIC_ACC_ENABLE(N)		(0x0200 + (4 * (N)))
#define SPMI_PIC_IRQ_STATUS(N)		(0x0600 + (4 * (N)))
#define SPMI_PIC_IRQ_CLEAR(N)		(0x0A00 + (4 * (N)))

/* Channel Status fields */
enum pmic_arb_chnl_status {
	PMIC_ARB_STATUS_DONE	= (1 << 0),
	PMIC_ARB_STATUS_FAILURE	= (1 << 1),
	PMIC_ARB_STATUS_DENIED	= (1 << 2),
	PMIC_ARB_STATUS_DROPPED	= (1 << 3),
};

/* Command register fields */
#define PMIC_ARB_CMD_MAX_BYTE_COUNT	8

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
#define PMIC_ARB_MAX_PERIPHS		256
#define PMIC_ARB_PERIPH_ID_VALID	(1 << 15)
#define PMIC_ARB_TIMEOUT_US		100

#define PMIC_ARB_APID_MASK				0xFF
#define PMIC_ARB_PPID_MASK				0xFFF
/* extract PPID and APID from interrupt map in .dts config file format */
#define PMIC_ARB_DEV_TRE_2_PPID(MAP_COMPRS_VAL)		\
			((MAP_COMPRS_VAL) >> (20))
#define PMIC_ARB_DEV_TRE_2_APID(MAP_COMPRS_VAL)		\
			((MAP_COMPRS_VAL) &  PMIC_ARB_APID_MASK)

/**
 * base - base address of the PMIC Arbiter core registers.
 * intr - base address of the SPMI interrupt control registers
 */
struct spmi_pmic_arb_dev {
	struct spmi_controller	controller;
	struct device		*dev;
	struct device		*slave;
	void __iomem		*base;
	void __iomem		*intr;
	int			pic_irq;
	spinlock_t		lock;
	u8			owner;
	u8			channel;
	u8			min_apid;
	u8			max_apid;
	u16			periph_id_map[PMIC_ARB_MAX_PERIPHS];
};

static u32 pmic_arb_read(struct spmi_pmic_arb_dev *dev, u32 offset)
{
	u32 val = readl_relaxed(dev->base + offset);
	pr_debug("address 0x%p, val 0x%x\n", dev->base + offset, val);
	return val;
}

static void pmic_arb_write(struct spmi_pmic_arb_dev *dev, u32 offset, u32 val)
{
	pr_debug("address 0x%p, val 0x%x\n", dev->base + offset, val);
	writel_relaxed(val, dev->base + offset);
}

static int pmic_arb_wait_for_done(struct spmi_pmic_arb_dev *dev)
{
	u32 status = 0;
	u32 timeout = PMIC_ARB_TIMEOUT_US;
	u32 offset = PMIC_ARB_STATUS(dev->channel);

	while (timeout--) {
		status = pmic_arb_read(dev, offset);

		if (status & PMIC_ARB_STATUS_DONE) {
			if (status & PMIC_ARB_STATUS_DENIED) {
				dev_err(dev->dev,
					"%s: transaction denied (0x%x)\n",
					__func__, status);
				return -EPERM;
			}

			if (status & PMIC_ARB_STATUS_FAILURE) {
				dev_err(dev->dev,
					"%s: transaction failed (0x%x)\n",
					__func__, status);
				return -EIO;
			}

			if (status & PMIC_ARB_STATUS_DROPPED) {
				dev_err(dev->dev,
					"%s: transaction dropped (0x%x)\n",
					__func__, status);
				return -EIO;
			}

			return 0;
		}
		udelay(1);
	}

	dev_err(dev->dev, "%s: timeout, status 0x%x\n", __func__, status);
	return -ETIMEDOUT;
}

static void pa_read_data(struct spmi_pmic_arb_dev *dev, u8 *buf, u32 reg, u8 bc)
{
	u32 data = pmic_arb_read(dev, reg);

	switch (bc & 0x3) {
	case 3:
		*buf++ = data & 0xff;
		data >>= 8;
	case 2:
		*buf++ = data & 0xff;
		data >>= 8;
	case 1:
		*buf++ = data & 0xff;
		data >>= 8;
	case 0:
		*buf++ = data & 0xff;
	default:
		break;
	}
}

static void
pa_write_data(struct spmi_pmic_arb_dev *dev, u8 *buf, u32 reg, u8 bc)
{
	u32 data = 0;

	switch (bc & 0x3) {
	case 3:
		data = (buf[0]|buf[1]<<8|buf[2]<<16|buf[3]<<24);
		break;
	case 2:
		data = (buf[0]|buf[1]<<8|buf[2]<<16);
		break;
	case 1:
		data = (buf[0]|buf[1]<<8);
		break;
	case 0:
		data = (buf[0]);
		break;
	default:
		break;
	}

	pmic_arb_write(dev, reg, data);
}

/* Non-data command */
static int pmic_arb_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	unsigned long flags;
	u32 cmd;
	int rc;

	pr_debug("op:0x%x sid:%d\n", opc, sid);

	/* Check for valid non-data command */
	if (opc < SPMI_CMD_RESET || opc > SPMI_CMD_WAKEUP)
		return -EINVAL;

	cmd = ((opc | 0x40) << 27) | ((sid & 0xf) << 20);

	spin_lock_irqsave(&pmic_arb->lock, flags);
	pmic_arb_write(pmic_arb, PMIC_ARB_CMD(pmic_arb->channel), cmd);
	rc = pmic_arb_wait_for_done(pmic_arb);
	spin_unlock_irqrestore(&pmic_arb->lock, flags);

	return rc;
}

static int pmic_arb_read_cmd(struct spmi_controller *ctrl,
				u8 opc, u8 sid, u16 addr, u8 bc, u8 *buf)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	unsigned long flags;
	u32 cmd;
	int rc;

	pr_debug("op:0x%x sid:%d bc:%d addr:0x%x\n", opc, sid, bc, addr);

	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7F)
		opc = PMIC_ARB_OP_READ;
	else if (opc >= 0x20 && opc <= 0x2F)
		opc = PMIC_ARB_OP_EXT_READ;
	else if (opc >= 0x38 && opc <= 0x3F)
		opc = PMIC_ARB_OP_EXT_READL;
	else
		return -EINVAL;

	cmd = (opc << 27) | ((sid & 0xf) << 20) | (addr << 4) | (bc & 0x7);

	spin_lock_irqsave(&pmic_arb->lock, flags);
	pmic_arb_write(pmic_arb, PMIC_ARB_CMD(pmic_arb->channel), cmd);
	rc = pmic_arb_wait_for_done(pmic_arb);
	if (rc)
		goto done;

	/* Read from FIFO, note 'bc' is actually number of bytes minus 1 */
	pa_read_data(pmic_arb, buf, PMIC_ARB_RDATA0(pmic_arb->channel), bc);

	if (bc > 3)
		pa_read_data(pmic_arb, buf + 4,
				PMIC_ARB_RDATA1(pmic_arb->channel), bc);

done:
	spin_unlock_irqrestore(&pmic_arb->lock, flags);
	return rc;
}

static int pmic_arb_write_cmd(struct spmi_controller *ctrl,
				u8 opc, u8 sid, u16 addr, u8 bc, u8 *buf)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	unsigned long flags;
	u32 cmd;
	int rc;

	pr_debug("op:0x%x sid:%d bc:%d addr:0x%x\n", opc, sid, bc, addr);

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIC_ARB_OP_WRITE;
	else if (opc >= 0x00 && opc <= 0x0F)
		opc = PMIC_ARB_OP_EXT_WRITE;
	else if (opc >= 0x30 && opc <= 0x37)
		opc = PMIC_ARB_OP_EXT_WRITEL;
	else if (opc >= 0x80 && opc <= 0xFF)
		opc = PMIC_ARB_OP_ZERO_WRITE;
	else
		return -EINVAL;

	cmd = (opc << 27) | ((sid & 0xf) << 20) | (addr << 4) | (bc & 0x7);

	/* Write data to FIFOs */
	spin_lock_irqsave(&pmic_arb->lock, flags);
	pa_write_data(pmic_arb, buf, PMIC_ARB_WDATA0(pmic_arb->channel), bc);

	if (bc > 3)
		pa_write_data(pmic_arb, buf + 4,
				PMIC_ARB_WDATA1(pmic_arb->channel), bc);

	/* Start the transaction */
	pmic_arb_write(pmic_arb, PMIC_ARB_CMD(pmic_arb->channel), cmd);
	rc = pmic_arb_wait_for_done(pmic_arb);
	spin_unlock_irqrestore(&pmic_arb->lock, flags);

	return rc;
}

/* APID to PPID */
static u16 get_peripheral_id(struct spmi_pmic_arb_dev *pmic_arb, u8 apid)
{
	return pmic_arb->periph_id_map[apid] & PMIC_ARB_PPID_MASK;
}

/* APID to PPID, returns valid flag */
static int is_apid_valid(struct spmi_pmic_arb_dev *pmic_arb, u8 apid)
{
	return pmic_arb->periph_id_map[apid] & PMIC_ARB_PERIPH_ID_VALID;
}

/* PPID to APID */
static uint32_t map_peripheral_id(struct spmi_pmic_arb_dev *pmic_arb, u16 ppid)
{
	int first = pmic_arb->min_apid;
	int last = pmic_arb->max_apid;
	int i;

	/* Search table for a matching PPID */
	for (i = first; i <= last; ++i) {
		if ((pmic_arb->periph_id_map[i] & PMIC_ARB_PPID_MASK) == ppid)
			return i;
	}

	dev_err(pmic_arb->dev, "Unknown ppid 0x%x\n", ppid);
	return PMIC_ARB_MAX_PERIPHS;
}

/* Enable interrupt at the PMIC Arbiter PIC */
static int pmic_arb_pic_enable(struct spmi_controller *ctrl,
				struct qpnp_irq_spec *spec, uint32_t data)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	u8 apid = data & PMIC_ARB_APID_MASK;
	unsigned long flags;
	u32 status;

	dev_dbg(pmic_arb->dev, "PIC enable, apid:0x%x, sid:0x%x, pid:0x%x\n",
				apid, spec->slave, spec->per);

	if (data < pmic_arb->min_apid || data > pmic_arb->max_apid) {
		dev_err(pmic_arb->dev, "int enable: invalid APID %d\n", data);
		return -EINVAL;
	}

	if (!is_apid_valid(pmic_arb, apid)) {
		dev_err(pmic_arb->dev, "int enable: int not supported\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&pmic_arb->lock, flags);
	status = readl_relaxed(pmic_arb->intr + SPMI_PIC_ACC_ENABLE(apid));
	if (!status) {
		writel_relaxed(0x1, pmic_arb->intr + SPMI_PIC_ACC_ENABLE(apid));
		/* Interrupt needs to be enabled before returning to caller */
		wmb();
	}
	spin_unlock_irqrestore(&pmic_arb->lock, flags);
	return 0;
}

/* Disable interrupt at the PMIC Arbiter PIC */
static int pmic_arb_pic_disable(struct spmi_controller *ctrl,
				struct qpnp_irq_spec *spec, uint32_t data)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	u8 apid = data & PMIC_ARB_APID_MASK;
	unsigned long flags;
	u32 status;

	dev_dbg(pmic_arb->dev, "PIC disable, apid:0x%x, sid:0x%x, pid:0x%x\n",
				apid, spec->slave, spec->per);

	if (data < pmic_arb->min_apid || data > pmic_arb->max_apid) {
		dev_err(pmic_arb->dev, "int disable: invalid APID %d\n", data);
		return -EINVAL;
	}

	if (!is_apid_valid(pmic_arb, apid)) {
		dev_err(pmic_arb->dev, "int disable: int not supported\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&pmic_arb->lock, flags);
	status = readl_relaxed(pmic_arb->intr + SPMI_PIC_ACC_ENABLE(apid));
	if (status) {
		writel_relaxed(0x0, pmic_arb->intr + SPMI_PIC_ACC_ENABLE(apid));
		/* Interrupt needs to be disabled before returning to caller */
		wmb();
	}
	spin_unlock_irqrestore(&pmic_arb->lock, flags);
	return 0;
}

static irqreturn_t
periph_interrupt(struct spmi_pmic_arb_dev *pmic_arb, u8 apid)
{
	u16 ppid = get_peripheral_id(pmic_arb, apid);
	void __iomem *base = pmic_arb->intr;
	u8 sid = (ppid >> 8) & 0x0F;
	u8 pid = ppid & 0xFF;
	u32 status;
	int i;

	if (!is_apid_valid(pmic_arb, apid)) {
		dev_err(pmic_arb->dev, "unknown peripheral id 0x%x\n", ppid);
		/* return IRQ_NONE; */
	}

	/* Read the peripheral specific interrupt bits */
	status = readl_relaxed(base + SPMI_PIC_IRQ_STATUS(apid));

	/* Clear the peripheral interrupts */
	writel_relaxed(status, base + SPMI_PIC_IRQ_CLEAR(apid));
	/* Interrupt needs to be cleared/acknowledged before exiting ISR */
	mb();

	dev_dbg(pmic_arb->dev,
		"interrupt, apid:0x%x, sid:0x%x, pid:0x%x, intr:0x%x\n",
						apid, sid, pid, status);

	/* Send interrupt notification */
	for (i = 0; status && i < 8; ++i, status >>= 1) {
		if (status & 0x1) {
			struct qpnp_irq_spec irq_spec = {
				.slave = sid,
				.per = pid,
				.irq = i,
			};
			qpnpint_handle_irq(&pmic_arb->controller, &irq_spec);
		}
	}
	return IRQ_HANDLED;
}

/* Peripheral interrupt handler */
static irqreturn_t pmic_arb_periph_irq(int irq, void *dev_id)
{
	struct spmi_pmic_arb_dev *pmic_arb = dev_id;
	void __iomem *intr = pmic_arb->intr;
	u8 ee = pmic_arb->owner;
	u32 ret = IRQ_NONE;
	u32 status;

	int first = pmic_arb->min_apid >> 5;
	int last = pmic_arb->max_apid >> 5;
	int i, j;

	dev_dbg(pmic_arb->dev, "Peripheral interrupt detected\n");

	/* Check the accumulated interrupt status */
	for (i = first; i <= last; ++i) {
		status = readl_relaxed(intr + SPMI_PIC_OWNER_ACC_STATUS(ee, i));

		for (j = 0; status && j < 32; ++j, status >>= 1) {
			if (status & 0x1) {
				u8 id = (i * 32) + j;
				ret |= periph_interrupt(pmic_arb, id);
			}
		}
	}

	return ret;
}

/* Callback to register an APID for specific slave/peripheral */
static int pmic_arb_intr_priv_data(struct spmi_controller *ctrl,
				struct qpnp_irq_spec *spec, uint32_t *data)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	u16 ppid = ((spec->slave & 0x0F) << 8) | (spec->per & 0xFF);
	*data = map_peripheral_id(pmic_arb, ppid);
	return 0;
}

static int __devinit
spmi_pmic_arb_get_property(struct platform_device *pdev, char *pname, u32 *prop)
{
	int ret = of_property_read_u32(pdev->dev.of_node, pname, prop);

	if (ret)
		dev_err(&pdev->dev, "missing property: %s\n", pname);
	else
		pr_debug("%s = 0x%x\n", pname, *prop);

	return ret;
}

static int __devinit spmi_pmic_arb_get_map_data(struct platform_device *pdev,
					struct spmi_pmic_arb_dev *pmic_arb)
{
	int i;
	int ret;
	int map_size;
	u32 *map_data;
	const int map_width = sizeof(*map_data);
	const struct device_node *of_node = pdev->dev.of_node;

	/* Get size of the mapping table (in bytes) */
	if (!of_get_property(of_node, "qcom,pmic-arb-ppid-map", &map_size)) {
		dev_err(&pdev->dev, "missing ppid mapping table\n");
		return -ENODEV;
	}

	/* Map size can't exceed the maximum number of peripherals */
	if (map_size == 0 || map_size > map_width * PMIC_ARB_MAX_PERIPHS) {
		dev_err(&pdev->dev, "map size of %d is not valid\n", map_size);
		return -ENODEV;
	}

	map_data = kzalloc(map_size, GFP_KERNEL);
	if (!map_data) {
		dev_err(&pdev->dev, "can not allocate map data\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32_array(of_node,
		"qcom,pmic-arb-ppid-map", map_data, map_size/sizeof(u32));
	if (ret) {
		dev_err(&pdev->dev, "invalid or missing property: ppid-map\n");
		goto err;
	};

	pmic_arb->max_apid = 0;
	pmic_arb->min_apid = PMIC_ARB_MAX_PERIPHS - 1;

	/* Build the mapping table from the data */
	for (i = 0; i < map_size/sizeof(u32);) {
		u32 map_compressed_val = map_data[i++];
		u32 ppid = PMIC_ARB_DEV_TRE_2_PPID(map_compressed_val) ;
		u32 apid = PMIC_ARB_DEV_TRE_2_APID(map_compressed_val) ;

		if (pmic_arb->periph_id_map[apid] & PMIC_ARB_PERIPH_ID_VALID)
			dev_warn(&pdev->dev, "duplicate APID 0x%x\n", apid);

		pmic_arb->periph_id_map[apid] = ppid | PMIC_ARB_PERIPH_ID_VALID;

		if (apid > pmic_arb->max_apid)
			pmic_arb->max_apid = apid;

		if (apid < pmic_arb->min_apid)
			pmic_arb->min_apid = apid;
	}

	pr_debug("%d value(s) mapped, min:%d, max:%d\n",
		map_size/map_width, pmic_arb->min_apid, pmic_arb->max_apid);

err:
	kfree(map_data);
	return ret;
}

static struct qpnp_local_int spmi_pmic_arb_intr_cb = {
	.mask = pmic_arb_pic_disable,
	.unmask = pmic_arb_pic_enable,
	.register_priv_data = pmic_arb_intr_priv_data,
};

static int __devinit spmi_pmic_arb_probe(struct platform_device *pdev)
{
	struct spmi_pmic_arb_dev *pmic_arb;
	struct resource *mem_res;
	u32 cell_index;
	u32 prop;
	int ret = 0;

	pr_debug("SPMI PMIC Arbiter\n");

	pmic_arb = devm_kzalloc(&pdev->dev,
				sizeof(struct spmi_pmic_arb_dev), GFP_KERNEL);
	if (!pmic_arb) {
		dev_err(&pdev->dev, "can not allocate pmic_arb data\n");
		return -ENOMEM;
	}

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "missing base memory resource\n");
		return -ENODEV;
	}

	pmic_arb->base = devm_ioremap(&pdev->dev,
					mem_res->start, resource_size(mem_res));
	if (!pmic_arb->base) {
		dev_err(&pdev->dev, "ioremap of 'base' failed\n");
		return -ENOMEM;
	}

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!mem_res) {
		dev_err(&pdev->dev, "missing mem resource (interrupts)\n");
		return -ENODEV;
	}

	pmic_arb->intr = devm_ioremap(&pdev->dev,
					mem_res->start, resource_size(mem_res));
	if (!pmic_arb->intr) {
		dev_err(&pdev->dev, "ioremap of 'intr' failed\n");
		return -ENOMEM;
	}

	pmic_arb->pic_irq = platform_get_irq(pdev, 0);
	if (!pmic_arb->pic_irq) {
		dev_err(&pdev->dev, "missing IRQ resource\n");
		return -ENODEV;
	}

	ret = devm_request_irq(&pdev->dev, pmic_arb->pic_irq,
		pmic_arb_periph_irq, IRQF_TRIGGER_HIGH, pdev->name, pmic_arb);
	if (ret) {
		dev_err(&pdev->dev, "request IRQ failed\n");
		return ret;
	}

	/* Get properties from the device tree */
	ret = spmi_pmic_arb_get_property(pdev, "cell-index", &cell_index);
	if (ret)
		return -ENODEV;

	ret = spmi_pmic_arb_get_map_data(pdev, pmic_arb);
	if (ret)
		return ret;

	ret = spmi_pmic_arb_get_property(pdev, "qcom,pmic-arb-ee", &prop);
	if (ret)
		return -ENODEV;
	pmic_arb->owner = (u8)prop;

	ret = spmi_pmic_arb_get_property(pdev, "qcom,pmic-arb-channel", &prop);
	if (ret)
		return -ENODEV;
	pmic_arb->channel = (u8)prop;

	pmic_arb->dev = &pdev->dev;
	platform_set_drvdata(pdev, pmic_arb);
	spmi_set_ctrldata(&pmic_arb->controller, pmic_arb);

	spin_lock_init(&pmic_arb->lock);

	pmic_arb->controller.nr = cell_index;
	pmic_arb->controller.dev.parent = pdev->dev.parent;
	pmic_arb->controller.dev.of_node = of_node_get(pdev->dev.of_node);

	/* Callbacks */
	pmic_arb->controller.cmd = pmic_arb_cmd;
	pmic_arb->controller.read_cmd = pmic_arb_read_cmd;
	pmic_arb->controller.write_cmd =  pmic_arb_write_cmd;

	ret = spmi_add_controller(&pmic_arb->controller);
	if (ret)
		goto err_add_controller;

	/* Register the interrupt enable/disable functions */
	ret = qpnpint_register_controller(pmic_arb->controller.dev.of_node,
					  &pmic_arb->controller,
					  &spmi_pmic_arb_intr_cb);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register controller %d\n",
					cell_index);
		goto err_reg_controller;
	}

	/* Register device(s) from the device tree */
	of_spmi_register_devices(&pmic_arb->controller);

	pr_debug("PMIC Arb Version 0x%x\n",
			pmic_arb_read(pmic_arb, PMIC_ARB_VERSION));

	return 0;

err_reg_controller:
	spmi_del_controller(&pmic_arb->controller);
err_add_controller:
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int __devexit spmi_pmic_arb_remove(struct platform_device *pdev)
{
	struct spmi_pmic_arb_dev *pmic_arb = platform_get_drvdata(pdev);

	free_irq(pmic_arb->pic_irq, pmic_arb);
	platform_set_drvdata(pdev, NULL);
	spmi_del_controller(&pmic_arb->controller);
	return 0;
}

static struct of_device_id spmi_pmic_arb_match_table[] = {
	{	.compatible = "qcom,spmi-pmic-arb",
	},
	{}
};

static struct platform_driver spmi_pmic_arb_driver = {
	.probe		= spmi_pmic_arb_probe,
	.remove		= __exit_p(spmi_pmic_arb_remove),
	.driver		= {
		.name	= SPMI_PMIC_ARB_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = spmi_pmic_arb_match_table,
	},
};

static int __init spmi_pmic_arb_init(void)
{
	return platform_driver_register(&spmi_pmic_arb_driver);
}
postcore_initcall(spmi_pmic_arb_init);

static void __exit spmi_pmic_arb_exit(void)
{
	platform_driver_unregister(&spmi_pmic_arb_driver);
}
module_exit(spmi_pmic_arb_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:spmi_pmic_arb");
