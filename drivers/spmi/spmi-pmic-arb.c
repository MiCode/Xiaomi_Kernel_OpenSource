/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/seq_file.h>
#include <linux/syscore_ops.h>
#include <mach/qpnp-int.h>
#include "spmi-dbgfs.h"

#define SPMI_PMIC_ARB_NAME		"spmi_pmic_arb"

/* PMIC Arbiter configuration registers */
#define PMIC_ARB_VERSION		0x0000
#define PMIC_ARB_INT_EN			0x0004
#define PMIC_ARB_PROTOCOL_IRQ_STATUS	(0x700 + 0x820)
#define PMIC_ARB_GENI_CTRL		0x0024
#define PMIC_ARB_GENI_STATUS	0x0028
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

/* Mapping Table */
#define SPMI_MAPPING_TABLE_REG(N)	(0x0B00 + (4 * (N)))
#define SPMI_MAPPING_BIT_INDEX(X)	(((X) >> 18) & 0xF)
#define SPMI_MAPPING_BIT_IS_0_FLAG(X)	(((X) >> 17) & 0x1)
#define SPMI_MAPPING_BIT_IS_0_RESULT(X)	(((X) >> 9) & 0xFF)
#define SPMI_MAPPING_BIT_IS_1_FLAG(X)	(((X) >> 8) & 0x1)
#define SPMI_MAPPING_BIT_IS_1_RESULT(X)	(((X) >> 0) & 0xFF)

#define SPMI_MAPPING_TABLE_LEN		255
#define SPMI_MAPPING_TABLE_TREE_DEPTH	16	/* Maximum of 16-bits */

/* Ownership Table */
#define SPMI_OWNERSHIP_TABLE_REG(N)	(0x0700 + (4 * (N)))
#define SPMI_OWNERSHIP_PERIPH2OWNER(X)	((X) & 0x7)

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
#define PMIC_ARB_MAX_TRANS_BYTES	(8)

#define PMIC_ARB_APID_MASK		0xFF
#define PMIC_ARB_PPID_MASK		0xFFF

/* interrupt enable bit */
#define SPMI_PIC_ACC_ENABLE_BIT		BIT(0)

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
	void __iomem		*cnfg;
	int			pic_irq;
	bool			allow_wakeup;
	spinlock_t		lock;
	u8			owner;
	u8			channel;
	u8			min_apid;
	u8			max_apid;
	u16			periph_id_map[PMIC_ARB_MAX_PERIPHS];
	u32			mapping_table[SPMI_MAPPING_TABLE_LEN];
	u32			prev_prtcl_irq_stat;
};

static struct spmi_pmic_arb_dev *the_pmic_arb;

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

static void pmic_arb_save_stat_before_txn(struct spmi_pmic_arb_dev *dev)
{
	dev->prev_prtcl_irq_stat =
		readl_relaxed(dev->cnfg + PMIC_ARB_PROTOCOL_IRQ_STATUS);
}

static int pmic_arb_diagnosis(struct spmi_pmic_arb_dev *dev, u32 status)
{
	if (status & PMIC_ARB_STATUS_DENIED) {
		dev_err(dev->dev,
		    "wait_for_done: transaction denied by SPMI master (0x%x)\n",
		    status);
		return -EPERM;
	}

	if (status & PMIC_ARB_STATUS_FAILURE) {
		dev_err(dev->dev,
		    "wait_for_done: transaction failed (0x%x)\n", status);
		return -EIO;
	}

	if (status & PMIC_ARB_STATUS_DROPPED) {
		dev_err(dev->dev,
		    "wait_for_done: transaction dropped pmic-arb busy (0x%x)\n",
		    status);
		return -EAGAIN;
	}

	return 0;
}

static int pmic_arb_wait_for_done(struct spmi_pmic_arb_dev *dev)
{
	u32 status = 0;
	u32 timeout = PMIC_ARB_TIMEOUT_US;
	u32 offset = PMIC_ARB_STATUS(dev->channel);

	while (timeout--) {
		status = pmic_arb_read(dev, offset);

		if (status & PMIC_ARB_STATUS_DONE)
			return pmic_arb_diagnosis(dev, status);

		udelay(1);
	}

	dev_err(dev->dev, "wait_for_done:: timeout, status 0x%x\n", status);
	return -ETIMEDOUT;
}

/**
 * pa_read_data: reads pmic-arb's register and copy 1..4 bytes to buf
 * @bc byte count -1. range: 0..3
 * @reg register's address
 * @buf output parameter, length must be bc+1
 */
static void pa_read_data(struct spmi_pmic_arb_dev *dev, u8 *buf, u32 reg, u8 bc)
{
	u32 data = pmic_arb_read(dev, reg);
	memcpy(buf, &data, (bc & 3) + 1);
}

/**
 * pa_write_data: write 1..4 bytes from buf to pmic-arb's register
 * @bc byte-count -1. range: 0..3
 * @reg register's address
 * @buf buffer to write. length must be bc+1
 */
static void
pa_write_data(struct spmi_pmic_arb_dev *dev, u8 *buf, u32 reg, u8 bc)
{
	u32 data = 0;
	memcpy(&data, buf, (bc & 3) + 1);
	pmic_arb_write(dev, reg, data);
}

static void pmic_arb_dbg_err_dump(struct spmi_pmic_arb_dev *pmic_arb, int ret,
		const char *msg, u8 opc, u8 sid, u16 addr, u8 bc, u8 *buf)
{
	u32 irq_stat  = readl_relaxed(pmic_arb->cnfg
				+ PMIC_ARB_PROTOCOL_IRQ_STATUS);
	u32 geni_stat = readl_relaxed(pmic_arb->cnfg + PMIC_ARB_GENI_STATUS);
	u32 geni_ctrl = readl_relaxed(pmic_arb->cnfg + PMIC_ARB_GENI_CTRL);

	bc += 1; /* actual byte count */

	if (buf)
		dev_err(pmic_arb->dev,
		"error:%d on data %s  opcode:0x%x sid:%d addr:0x%x bc:%d buf:%*phC\n",
			ret, msg, opc, sid, addr, bc, bc, buf);
	else
		dev_err(pmic_arb->dev,
		"error:%d on non-data-cmd opcode:0x%x sid:%d\n",
			ret, opc, sid);
	dev_err(pmic_arb->dev,
		"PROTOCOL_IRQ_STATUS before:0x%x after:0x%x GENI_STATUS:0x%x GENI_CTRL:0x%x\n",
		irq_stat, pmic_arb->prev_prtcl_irq_stat, geni_stat, geni_ctrl);
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

	opc -= SPMI_CMD_RESET - PMIC_ARB_OP_RESET;

	cmd = (opc << 27) | ((sid & 0xf) << 20);

	spin_lock_irqsave(&pmic_arb->lock, flags);
	pmic_arb_save_stat_before_txn(pmic_arb);
	pmic_arb_write(pmic_arb, PMIC_ARB_CMD(pmic_arb->channel), cmd);
	rc = pmic_arb_wait_for_done(pmic_arb);
	spin_unlock_irqrestore(&pmic_arb->lock, flags);

	if (rc)
		pmic_arb_dbg_err_dump(pmic_arb, rc, "cmd", opc, sid, 0, 0, 0);
	return rc;
}

static int pmic_arb_read_cmd(struct spmi_controller *ctrl,
				u8 opc, u8 sid, u16 addr, u8 bc, u8 *buf)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	unsigned long flags;
	u32 cmd;
	int rc;

	if (bc >= PMIC_ARB_MAX_TRANS_BYTES) {
		dev_err(pmic_arb->dev
		, "pmic-arb supports 1..%d bytes per trans, but:%d requested"
					, PMIC_ARB_MAX_TRANS_BYTES, bc+1);
		return  -EINVAL;
	}
	dev_dbg(pmic_arb->dev, "client-rd op:0x%x sid:%d addr:0x%x bc:%d\n",
							opc, sid, addr, bc + 1);

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
	pmic_arb_save_stat_before_txn(pmic_arb);
	pmic_arb_write(pmic_arb, PMIC_ARB_CMD(pmic_arb->channel), cmd);
	rc = pmic_arb_wait_for_done(pmic_arb);
	if (rc)
		goto done;

	/* Read from FIFO, note 'bc' is actually number of bytes minus 1 */
	pa_read_data(pmic_arb, buf, PMIC_ARB_RDATA0(pmic_arb->channel)
							, min_t(u8, bc, 3));

	if (bc > 3)
		pa_read_data(pmic_arb, buf + 4,
				PMIC_ARB_RDATA1(pmic_arb->channel), bc - 4);

done:
	spin_unlock_irqrestore(&pmic_arb->lock, flags);
	if (rc)
		pmic_arb_dbg_err_dump(pmic_arb, rc, "read", opc, sid, addr, bc,
									buf);
	return rc;
}

static int pmic_arb_write_cmd(struct spmi_controller *ctrl,
				u8 opc, u8 sid, u16 addr, u8 bc, u8 *buf)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	unsigned long flags;
	u32 cmd;
	int rc;

	if (bc >= PMIC_ARB_MAX_TRANS_BYTES) {
		dev_err(pmic_arb->dev
		, "pmic-arb supports 1..%d bytes per trans, but:%d requested"
					, PMIC_ARB_MAX_TRANS_BYTES, bc+1);
		return  -EINVAL;
	}
	dev_dbg(pmic_arb->dev, "client-wr op:0x%x sid:%d addr:0x%x bc:%d\n",
							opc, sid, addr, bc + 1);

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
	pmic_arb_save_stat_before_txn(pmic_arb);
	pa_write_data(pmic_arb, buf, PMIC_ARB_WDATA0(pmic_arb->channel)
							, min_t(u8, bc, 3));
	if (bc > 3)
		pa_write_data(pmic_arb, buf + 4,
				PMIC_ARB_WDATA1(pmic_arb->channel), bc - 4);

	/* Start the transaction */
	pmic_arb_write(pmic_arb, PMIC_ARB_CMD(pmic_arb->channel), cmd);
	rc = pmic_arb_wait_for_done(pmic_arb);
	spin_unlock_irqrestore(&pmic_arb->lock, flags);

	if (rc)
		pmic_arb_dbg_err_dump(pmic_arb, rc, "write", opc, sid, addr, bc,
									buf);

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

static u32 search_mapping_table(struct spmi_pmic_arb_dev *pmic_arb, u16 ppid)
{
	u32 *mapping_table = pmic_arb->mapping_table;
	u32 apid = PMIC_ARB_MAX_PERIPHS;
	int index = 0;
	u32 data;
	int i;

	for (i = 0; i < SPMI_MAPPING_TABLE_TREE_DEPTH; ++i) {
		data = mapping_table[index];

		if (ppid & (1 << SPMI_MAPPING_BIT_INDEX(data))) {
			if (SPMI_MAPPING_BIT_IS_1_FLAG(data)) {
				index = SPMI_MAPPING_BIT_IS_1_RESULT(data);
			} else {
				apid = SPMI_MAPPING_BIT_IS_1_RESULT(data);
				break;
			}
		} else {
			if (SPMI_MAPPING_BIT_IS_0_FLAG(data)) {
				index = SPMI_MAPPING_BIT_IS_0_RESULT(data);
			} else {
				apid = SPMI_MAPPING_BIT_IS_0_RESULT(data);
				break;
			}
		}
	}

	return apid;
}

/* PPID to APID */
static uint32_t map_peripheral_id(struct spmi_pmic_arb_dev *pmic_arb, u16 ppid)
{
	u32 apid = search_mapping_table(pmic_arb, ppid);
	u32 old_ppid;
	u32 owner;

	/* If the apid was found, add it to the lookup table */
	if (apid < PMIC_ARB_MAX_PERIPHS) {
		old_ppid = get_peripheral_id(pmic_arb, apid);

		owner = SPMI_OWNERSHIP_PERIPH2OWNER(
				readl_relaxed(pmic_arb->cnfg +
					SPMI_OWNERSHIP_TABLE_REG(apid)));

		/* Check ownership */
		if (owner != pmic_arb->owner) {
			dev_err(pmic_arb->dev, "PPID 0x%x incorrect owner %d\n",
				ppid, owner);
			return PMIC_ARB_MAX_PERIPHS;
		}

		/* Check if already mapped */
		if (pmic_arb->periph_id_map[apid] & PMIC_ARB_PERIPH_ID_VALID) {
			if (ppid != old_ppid) {
				dev_err(pmic_arb->dev,
					"PPID 0x%x: APID 0x%x already mapped\n",
					ppid, apid);
				return PMIC_ARB_MAX_PERIPHS;
			}
			return apid;
		}

		pmic_arb->periph_id_map[apid] = ppid | PMIC_ARB_PERIPH_ID_VALID;

		if (apid > pmic_arb->max_apid)
			pmic_arb->max_apid = apid;

		if (apid < pmic_arb->min_apid)
			pmic_arb->min_apid = apid;

		return apid;
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
	if (!(status & SPMI_PIC_ACC_ENABLE_BIT)) {
		status = status | SPMI_PIC_ACC_ENABLE_BIT;
		writel_relaxed(status,
				pmic_arb->intr + SPMI_PIC_ACC_ENABLE(apid));
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
	if (status & SPMI_PIC_ACC_ENABLE_BIT) {
		/* clear the enable bit and write */
		status = status & ~SPMI_PIC_ACC_ENABLE_BIT;
		writel_relaxed(status,
				pmic_arb->intr + SPMI_PIC_ACC_ENABLE(apid));
		/* Interrupt needs to be disabled before returning to caller */
		wmb();
	}
	spin_unlock_irqrestore(&pmic_arb->lock, flags);
	return 0;
}

static irqreturn_t
periph_interrupt(struct spmi_pmic_arb_dev *pmic_arb, u8 apid, bool show)
{
	u16 ppid = get_peripheral_id(pmic_arb, apid);
	void __iomem *intr = pmic_arb->intr;
	u8 sid = (ppid >> 8) & 0x0F;
	u8 pid = ppid & 0xFF;
	u32 status;
	int i;

	if (!is_apid_valid(pmic_arb, apid)) {
		dev_err(pmic_arb->dev,
		"periph_interrupt(apid:0x%x sid:0x%x pid:0x%x) unknown peripheral\n",
			apid, sid, pid);
		/* return IRQ_NONE; */
	}

	status = readl_relaxed(intr + SPMI_PIC_ACC_ENABLE(apid));
	if (!(status & SPMI_PIC_ACC_ENABLE_BIT)) {
		/*
		 * All interrupts from this peripheral are disabled
		 * don't bother calling the qpnpint handler
		 */
		return IRQ_HANDLED;
	}

	/* Read the peripheral specific interrupt bits */
	status = readl_relaxed(intr + SPMI_PIC_IRQ_STATUS(apid));

	if (!show) {
		/* Clear the peripheral interrupts */
		writel_relaxed(status, intr + SPMI_PIC_IRQ_CLEAR(apid));
		/* Irq needs to be cleared/acknowledged before exiting ISR */
		mb();
	}

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
			if (show)
				qpnpint_show_irq(&pmic_arb->controller,
								&irq_spec);
			else
				qpnpint_handle_irq(&pmic_arb->controller,
								&irq_spec);
		}
	}
	return IRQ_HANDLED;
}

/* Peripheral interrupt handler */
static irqreturn_t
__pmic_arb_periph_irq(int irq, void *dev_id, bool show)
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

				ret |= periph_interrupt(pmic_arb, id, show);
			}
		}
	}

	return ret;
}

static irqreturn_t pmic_arb_periph_irq(int irq, void *dev_id)
{
	return __pmic_arb_periph_irq(irq, dev_id, false);
}

static void spmi_pmic_arb_resume(void)
{
	if (qpnpint_show_resume_irq())
		__pmic_arb_periph_irq(the_pmic_arb->pic_irq,
						the_pmic_arb, true);
}

static struct syscore_ops spmi_pmic_arb_syscore_ops = {
	.resume = spmi_pmic_arb_resume,
};

/* Callback to register an APID for specific slave/peripheral */
static int pmic_arb_intr_priv_data(struct spmi_controller *ctrl,
				struct qpnp_irq_spec *spec, uint32_t *data)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	u16 ppid = ((spec->slave & 0x0F) << 8) | (spec->per & 0xFF);
	*data = map_peripheral_id(pmic_arb, ppid);
	return 0;
}

static int pmic_arb_mapping_data_show(struct seq_file *file, void *unused)
{
	struct spmi_pmic_arb_dev *pmic_arb = file->private;
	int first = pmic_arb->min_apid;
	int last = pmic_arb->max_apid;
	int i;

	for (i = first; i <= last; ++i) {
		if (!is_apid_valid(pmic_arb, i))
			continue;

		seq_printf(file, "APID 0x%.2x = PPID 0x%.3x. Enabled:%d\n",
			i, get_peripheral_id(pmic_arb, i),
			readl_relaxed(pmic_arb->intr + SPMI_PIC_ACC_ENABLE(i)));
	}

	return 0;
}

static int pmic_arb_mapping_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_arb_mapping_data_show, inode->i_private);
}

static const struct file_operations pmic_arb_dfs_fops = {
	.open		= pmic_arb_mapping_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

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
	int i;

	pr_debug("SPMI PMIC Arbiter\n");

	pmic_arb = devm_kzalloc(&pdev->dev,
				sizeof(struct spmi_pmic_arb_dev), GFP_KERNEL);
	if (!pmic_arb) {
		dev_err(&pdev->dev, "can not allocate pmic_arb data\n");
		return -ENOMEM;
	}

	mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "core");
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

	mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "intr");
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

	mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cnfg");
	if (!mem_res) {
		dev_err(&pdev->dev, "missing mem resource (configuration)\n");
		return -ENODEV;
	}

	pmic_arb->cnfg = devm_ioremap(&pdev->dev,
					mem_res->start, resource_size(mem_res));
	if (!pmic_arb->cnfg) {
		dev_err(&pdev->dev, "ioremap of 'cnfg' failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(pmic_arb->mapping_table); ++i)
		pmic_arb->mapping_table[i] = readl_relaxed(
				pmic_arb->cnfg + SPMI_MAPPING_TABLE_REG(i));

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

	ret = spmi_pmic_arb_get_property(pdev, "qcom,pmic-arb-ee", &prop);
	if (ret)
		return -ENODEV;
	pmic_arb->owner = (u8)prop;

	ret = spmi_pmic_arb_get_property(pdev, "qcom,pmic-arb-channel", &prop);
	if (ret)
		return -ENODEV;
	pmic_arb->channel = (u8)prop;

	pmic_arb->allow_wakeup = !of_property_read_bool(pdev->dev.of_node,
					"qcom,not-wakeup");
	if (pmic_arb->allow_wakeup) {
		ret = irq_set_irq_wake(pmic_arb->pic_irq, 1);
		if (unlikely(ret)) {
			pr_err("Unable to set wakeup irq, err=%d\n", ret);
			return -ENODEV;
		}
	}

	pmic_arb->max_apid = 0;
	pmic_arb->min_apid = PMIC_ARB_MAX_PERIPHS - 1;

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

	/* Add debugfs file for mapping data */
	if (spmi_dfs_create_file(&pmic_arb->controller, "mapping",
					pmic_arb, &pmic_arb_dfs_fops) == NULL)
		dev_err(&pdev->dev, "error creating 'mapping' debugfs file\n");

	pr_debug("PMIC Arb Version 0x%x\n",
			pmic_arb_read(pmic_arb, PMIC_ARB_VERSION));

	the_pmic_arb = pmic_arb;
	register_syscore_ops(&spmi_pmic_arb_syscore_ops);

	return 0;

err_reg_controller:
	spmi_del_controller(&pmic_arb->controller);
err_add_controller:
	platform_set_drvdata(pdev, NULL);
	if (pmic_arb->allow_wakeup)
		irq_set_irq_wake(pmic_arb->pic_irq, 0);
	return ret;
}

static int __devexit spmi_pmic_arb_remove(struct platform_device *pdev)
{
	struct spmi_pmic_arb_dev *pmic_arb = platform_get_drvdata(pdev);
	int ret;

	ret = qpnpint_unregister_controller(pmic_arb->controller.dev.of_node);
	if (ret)
		dev_err(&pdev->dev, "Unable to unregister controller %d\n",
					pmic_arb->controller.nr);

	if (pmic_arb->allow_wakeup)
		irq_set_irq_wake(pmic_arb->pic_irq, 0);
	platform_set_drvdata(pdev, NULL);
	spmi_del_controller(&pmic_arb->controller);
	return ret;
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
