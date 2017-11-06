/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "#%d: " fmt, __LINE__

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
#include <linux/irqchip/qpnp-int.h>
#include "spmi-dbgfs.h"

#define SPMI_PMIC_ARB_NAME		"spmi_pmic_arb"

/* PMIC Arbiter configuration registers */
#define PMIC_ARB_VERSION		0x0000
#define PMIC_ARB_INT_EN			0x0004

enum {
	PMIC_ARB_GENI_CTRL,
	PMIC_ARB_GENI_STATUS,
	PMIC_ARB_PROTOCOL_IRQ_STATUS,
};

u32 pmic_arb_regs_v1[] = {
	[PMIC_ARB_GENI_CTRL]	= 0x0024,
	[PMIC_ARB_GENI_STATUS]	= 0x0028,
	[PMIC_ARB_PROTOCOL_IRQ_STATUS] = (0x700 + 0x820),
};

u32 pmic_arb_regs_v2[] = {
	[PMIC_ARB_GENI_CTRL]	= 0x0028,
	[PMIC_ARB_GENI_STATUS]	= 0x002C,
	[PMIC_ARB_PROTOCOL_IRQ_STATUS] = (0x700 + 0x900),
};

/* Offset per chnnel-register type */
#define PMIC_ARB_CMD		(0x00)
#define PMIC_ARB_CONFIG		(0x04)
#define PMIC_ARB_STATUS		(0x08)
#define PMIC_ARB_WDATA0		(0x10)
#define PMIC_ARB_WDATA1		(0x14)
#define PMIC_ARB_RDATA0		(0x18)
#define PMIC_ARB_RDATA1		(0x1C)

/* PMIC Arbiter configuration registers values */
#define PMIC_ARB_V2_MIN			(0x20010000)
#define PMIC_ARB_CORE_REGISTERS_OBS	(0x800000)

/* Mapping Table */
#define SPMI_MAPPING_TABLE_REG(N)	(0x0B00 + (4 * (N)))
#define SPMI_MAPPING_BIT_INDEX(X)	(((X) >> 18) & 0xF)
#define SPMI_MAPPING_BIT_IS_0_FLAG(X)	(((X) >> 17) & 0x1)
#define SPMI_MAPPING_BIT_IS_0_RESULT(X)	(((X) >> 9) & 0xFF)
#define SPMI_MAPPING_BIT_IS_1_FLAG(X)	(((X) >> 8) & 0x1)
#define SPMI_MAPPING_BIT_IS_1_RESULT(X)	(((X) >> 0) & 0xFF)

#define SPMI_MAPPING_TABLE_LEN		256
#define SPMI_MAPPING_TABLE_TREE_DEPTH	16	/* Maximum of 16-bits */

/* Ownership Table */
#define SPMI_OWNERSHIP_TABLE_REG(N)	(0x0700 + (4 * (N)))
#define SPMI_OWNERSHIP_PERIPH2OWNER(X)	((X) & 0x7)

/* PPID, SID, PID */
#define PMIC_ARB_PERIPH_ID(spmi_addr)		(((spmi_addr) >> 8) & 0xFF)
#define PMIC_ARB_ADDR_IN_PERIPH(spmi_addr)	((spmi_addr) & 0xFF)
#define PMIC_ARB_REG_CHNL(chnl_num)		(0x800 + 0x4 * (chnl_num))
#define PMIC_ARB_TO_PPID(sid, pid)	((pid & 0xFF) | ((sid & 0xF) << 8))
#define PMIC_ARB_PPID_CNT			(1 << 12)

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
#define PMIC_ARB_PERIPHS_CHNL_DEFAULT	128
#define PMIC_ARB_PERIPHS_INTR_DEFAULT	256
#define PMIC_ARB_PERIPH_ID_VALID	(1 << 15)
#define PMIC_ARB_TIMEOUT_US		100
#define PMIC_ARB_MAX_TRANS_BYTES	(8)

#define PMIC_ARB_APID_MASK		0xFF
#define PMIC_ARB_PPID_MASK		0xFFF

/* interrupt enable bit */
#define SPMI_PIC_ACC_ENABLE_BIT		BIT(0)

/*
 * spmi_pmic_arb_dbg: information used for debugging
 *
 * @base_phy   physical address of the core      register space
 * @rdbase_phy physical address of the observer  register space
 * @wrbase_phy physical address of the channels  register space
 * @intr_phy   physical address of the interrupt register space
 */
struct spmi_pmic_arb_dbg {
	phys_addr_t		base_phy;
	phys_addr_t		rdbase_phy;
	phys_addr_t		wrbase_phy;
	phys_addr_t		intr_phy;
};

struct spmi_pmic_arb_dev;

/*
 * spmi_pmic_arb_ver: version dependent callbacks.
 *
 * @chnl_ofst calc offset per channel. Note that v1 channel is one per EE, and
 *   v2 channels are one per PMIC peripheral.
 *   err is only considered on v2 HW.
 * @fmt_cmd format formats a GENI/SPMI command.
 * @owner_acc_status calc offset to PMIC_ARB_SPMI_PIC_OWNERm_ACC_STATUSn on v1,
 *   and SPMI_PIC_OWNERm_ACC_STATUSn on v2.
 * @acc_enable calc offset to PMIC_ARB_SPMI_PIC_ACC_ENABLEn on v1,
 *   and SPMI_PIC_ACC_ENABLEn on v2.
 * @irq_status calc offset to PMIC_ARB_SPMI_PIC_IRQ_STATUSn on v1,
 *   and SPMI_PIC_IRQ_STATUSn on v2.
 * @irq_clear calc offset to PMIC_ARB_SPMI_PIC_IRQ_CLEARn on v,
 *   and SPMI_PIC_IRQ_CLEARn on v2.
 */
struct spmi_pmic_arb_ver {
	int (*non_data_cmd)(struct spmi_pmic_arb_dev *dev, u8 opc, u8 sid);
	/* following functions are about phripheral rd/wr */
	phys_addr_t	(*chnl_ofst)(struct spmi_pmic_arb_dev *dev, u8 sid,
			u16 addr, int *err);
	u32		(*fmt_cmd)(u8 opc, u8 sid, u16 addr, u8 bc);
	/* following functions calc offsets to peripheral PIC registers */
	phys_addr_t	(*owner_acc_status)(u8 m, u8 n);
	phys_addr_t	(*acc_enable)(u8 n);
	phys_addr_t	(*irq_status)(u8 n);
	phys_addr_t	(*irq_clear)(u8 n);
	u32 *regs;
};

/*
 * @base base address of the PMIC Arbiter core registers.
 * @rdbase, @wrbase base address of the PMIC Arbiter read core registers.
 *     For HW-v1 these are equal to base.
 *     For HW-v2, the value is the same in eeraly probing, in order to read
 *     PMIC_ARB_CORE registers, then chnls, and obsrvr are set to
 *     PMIC_ARB_CORE_REGISTERS and PMIC_ARB_CORE_REGISTERS_OBS respectivly.
 * @intr base address of the SPMI interrupt control registers
 * @ppid_2_chnl_tbl lookup table f(SID, Periph-ID) -> channel num
 *      entry is only valid if corresponding bit is set in valid_ppid_bitmap.
 * @valid_ppid_bitmap bit is set only for valid ppids.
 * @fmt_cmd formats a command to be set into PMIC_ARBq_CHNLn_CMD
 * @chnl_ofst calculates offset of the base of a channel reg space
 * @ee execution environment id
 * @irq_acc0_init_val initial value of the interrupt accumulator at probe time.
 *      Use for an HW workaround. On handling interrupts, the first accumulator
 *      register will be compared against this value, and bits which are set at
 *      boot will be ignored.
 * @reserved_chnl entry of ppid_2_chnl_tbl that this driver should never touch.
 *      value is positive channel number or negative to mark it unused.
 */
struct spmi_pmic_arb_dev {
	struct spmi_controller	controller;
	struct device		*dev;
	struct device		*slave;
	void __iomem		*base;
	void __iomem		*rdbase;
	void __iomem		*wrbase;
	void __iomem		*intr;
	void __iomem		*cnfg;
	struct spmi_pmic_arb_dbg dbg;
	int			pic_irq;
	bool			allow_wakeup;
	spinlock_t		lock;
	u8			ee;
	u8			channel;
	u16			max_peripherals;
	u16			min_intr_apid;
	u16			max_intr_apid;
	u16			max_periph_intrs;
	u16			periph_id_map[PMIC_ARB_MAX_PERIPHS];
	u32			mapping_table[SPMI_MAPPING_TABLE_LEN];
	const struct spmi_pmic_arb_ver *ver;
	u8			*ppid_2_chnl_tbl;
	int			reserved_chnl;
	unsigned long		*valid_ppid_bitmap;
	u32			prev_prtcl_irq_stat;
	u32			irq_acc0_init_val;
};

static struct spmi_pmic_arb_dev *the_pmic_arb;

static phys_addr_t pmic_arb_chnl_ofst_v1(struct spmi_pmic_arb_dev *dev,
					 u8 sid, u16 addr, int *err)
{
	if (err)
		*err = 0;
	return 0x800 + 0x80 * (dev->channel);
}

static phys_addr_t pmic_arb_chnl_ofst_v2(struct spmi_pmic_arb_dev *dev,
					 u8 sid, u16 addr, int *err)
{
	u8   pid      = (addr >> 8) & 0xFF;
	u16  ppid     = PMIC_ARB_TO_PPID(sid, pid);
	bool is_valid = dev->valid_ppid_bitmap[BIT_WORD(ppid)] & BIT_MASK(ppid);
	char chnl     = dev->ppid_2_chnl_tbl[ppid];

	if (err)
		*err = 0;

	if (!is_valid) {
		dev_err(dev->dev,
		"error access to unmapped pmic peripheral sid:0x%x addr:0x%x\n",
			sid, addr);
		if (err)
			*err = -ENXIO;
	}
	if (chnl == dev->reserved_chnl) {
		dev_err(dev->dev,
		"error access to reserved channel sid:0x%x addr:0x%x chan:%d\n",
			sid, addr, chnl);
		if (err)
			*err = -EACCES;
	}

	return 0x1000 * (dev->ee) + 0x8000 * (chnl);
}

static u32 pmic_arb_fmt_cmd_v1(u8 opc, u8 sid, u16 addr, u8 bc)
{
	return (opc << 27) | ((sid & 0xF) << 20) | (addr << 4) | (bc & 0x7);
}

static u32 pmic_arb_fmt_cmd_v2(u8 opc, u8 sid, u16 addr, u8 bc)
{
	return (opc << 27) | (PMIC_ARB_ADDR_IN_PERIPH(addr) << 4) | (bc & 0x7);
}

static phys_addr_t pmic_arb_owner_acc_status_v1(u8 m, u8 n)
{
	return 0x20 * (m) + 0x4 * (n);
}

static phys_addr_t pmic_arb_owner_acc_status_v2(u8 m, u8 n)
{
	return 0x100000 + 0x1000 * (m) + 0x4 * (n);
}

static phys_addr_t pmic_arb_acc_enable_v1(u8 n)
{
	return 0x200 + 0x4 * (n);
}

static phys_addr_t pmic_arb_acc_enable_v2(u8 n)
{
	return 0x1000 * (n);
}

static phys_addr_t pmic_arb_irq_status_v1(u8 n)
{
	return 0x600 + 0x4 * (n);
}

static phys_addr_t pmic_arb_irq_status_v2(u8 n)
{
	return 0x4 + 0x1000 * (n);
}

static phys_addr_t pmic_arb_irq_clear_v1(u8 n)
{
	return 0xA00 + 0x4 * (n);
}

static phys_addr_t pmic_arb_irq_clear_v2(u8 n)
{
	return 0x8 + 0x1000 * (n);
}

static void dbg_io(struct spmi_pmic_arb_dev *dev, const char *name,
			void *virt, phys_addr_t phys, u32 offset, u32 val)
{
	dev_dbg(dev->dev,
	    "%-10s phy-base:0x%lx phy:0x%lx virt:0x%p ofst:0x%03x val:0x%x\n",
	    name, (ulong) phys, (ulong) (phys + offset), (virt + offset),
	    offset,  val);
}

static u32 pmic_arb_read(struct spmi_pmic_arb_dev *dev, u32 offset)
{
	u32 val = readl_relaxed(dev->rdbase + offset);

	dbg_io(dev, "spmi-rx", dev->rdbase, dev->dbg.rdbase_phy, offset, val);
	return val;
}

static void pmic_arb_write(struct spmi_pmic_arb_dev *dev, u32 offset, u32 val)
{
	writel_relaxed(val, dev->wrbase + offset);
	dbg_io(dev, "spmi-tx", dev->wrbase, dev->dbg.wrbase_phy, offset, val);
}

static void pmic_arb_set_rd_cmd(struct spmi_pmic_arb_dev *dev, u32 offset,
									u32 val)
{
	dbg_io(dev, "set-rd-cmd", dev->rdbase, dev->dbg.rdbase_phy, offset,
									val);
	writel_relaxed(val, dev->rdbase + offset);
}

static void dbg_pic_io(struct spmi_pmic_arb_dev *dev, const char *name,
			void *virt, phys_addr_t phys, u32 offset, u32 val,
			u8 sid, u16 pid, u8 apid, const char *desc)
{
	dev_dbg(dev->dev,
	"%-10s phy-base:0x%lx phy:0x%lx virt:0x%p ofst:0x%03x val:0x%x sid:%d pid:0x%x apid:0x%x %s\n",
	name, (ulong) phys, (ulong) (phys + offset), (virt + offset), offset,
	val, sid, pid, apid, desc ? desc : "");
}

static void spmi_pic_acc_en_wr(struct spmi_pmic_arb_dev *dev, u32 val,
				u8 sid, u16 pid, u8 apid, const char *desc)
{
	phys_addr_t ofst = dev->ver->acc_enable(apid);
	dbg_pic_io(dev, "acc-en-wr", dev->intr, dev->dbg.intr_phy, ofst, val,
							sid, pid, apid, desc);
	writel_relaxed(val, dev->intr + ofst);
}

static u32 spmi_pic_acc_en_rd(struct spmi_pmic_arb_dev *dev,
				u8 sid, u16 pid, u8 apid, const char *desc)
{
	phys_addr_t ofst = dev->ver->acc_enable(apid);
	u32 val = readl_relaxed(dev->intr + ofst);
	dbg_pic_io(dev, "acc-en-rd", dev->intr, dev->dbg.intr_phy, ofst, val,
							sid, pid, apid, desc);

	return val;
}

static void pmic_arb_save_stat_before_txn(struct spmi_pmic_arb_dev *dev)
{
	dev->prev_prtcl_irq_stat =
		readl_relaxed(dev->cnfg +
			dev->ver->regs[PMIC_ARB_PROTOCOL_IRQ_STATUS]);
}

static int pmic_arb_wait_for_done(struct spmi_pmic_arb_dev *dev,
					void __iomem *base, u8 sid, u16 addr)
{
	u32 status = 0;
	u32 timeout = PMIC_ARB_TIMEOUT_US;
	int rc;
	int offset = dev->ver->chnl_ofst(dev, sid, addr, &rc) + PMIC_ARB_STATUS;
	static const char * const diag_msg_fmt =
			"wait_for_done: %s status:0x%x sid:%d addr:0x%x\n";

	if (rc < 0)
		return rc;

	while (timeout--) {
		status = readl_relaxed(base + offset);

		if (status & PMIC_ARB_STATUS_DONE) {
			if (status & PMIC_ARB_STATUS_DENIED) {
				dev_err(dev->dev, diag_msg_fmt,
					"transaction denied by SPMI master "
					"(peripheral not owned by apps)",
					status, sid, addr);
				return -EPERM;
			}

			if (status & PMIC_ARB_STATUS_FAILURE) {
				dev_err(dev->dev, diag_msg_fmt,
				   "failed (possible parity-error due to noisy"
				   "bus or access to nonexistent peripheral)",
				   status, sid, addr);
				return -EIO;
			}

			if (status & PMIC_ARB_STATUS_DROPPED) {
				dev_err(dev->dev, diag_msg_fmt,
					"transaction dropped pmic-arb busy",
					status, sid, addr);
				return -EBUSY;
			}

			return 0;
		};

		udelay(1);
	}

	dev_err(dev->dev, diag_msg_fmt, "timeout", status, sid, addr);
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
	u32 irq_stat  = readl_relaxed(pmic_arb->cnfg +
			pmic_arb->ver->regs[PMIC_ARB_PROTOCOL_IRQ_STATUS]);
	u32 geni_stat = readl_relaxed(pmic_arb->cnfg +
				pmic_arb->ver->regs[PMIC_ARB_GENI_STATUS]);
	u32 geni_ctrl = readl_relaxed(pmic_arb->cnfg +
				pmic_arb->ver->regs[PMIC_ARB_GENI_CTRL]);

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

static int
pmic_arb_non_data_cmd_v1(struct spmi_pmic_arb_dev *pmic_arb, u8 opc, u8 sid)
{
	unsigned long flags;
	u32 cmd;
	int rc;
	/* sid and addr are don't-care for pmic_arb_chnl_ofst_v1() HW-v1  */
	phys_addr_t chnl_ofst = pmic_arb_chnl_ofst_v1(pmic_arb, 0, 0, NULL);

	opc -= SPMI_CMD_RESET - PMIC_ARB_OP_RESET;

	cmd = (opc << 27) | ((sid & 0xf) << 20);

	spin_lock_irqsave(&pmic_arb->lock, flags);
	pmic_arb_save_stat_before_txn(pmic_arb);
	pmic_arb_write(pmic_arb, chnl_ofst + PMIC_ARB_CMD, cmd);
	/* sid and addr are don't-care for pmic_arb_wait_for_done() HW-v1 */
	rc = pmic_arb_wait_for_done(pmic_arb, pmic_arb->wrbase, 0, 0);
	spin_unlock_irqrestore(&pmic_arb->lock, flags);

	if (rc)
		pmic_arb_dbg_err_dump(pmic_arb, rc, "cmd", opc, sid, 0, 0, 0);
	return rc;
}

/*
 * currently unsupported by HW
 */
static int
pmic_arb_non_data_cmd_v2(struct spmi_pmic_arb_dev *pmic_arb, u8 opc, u8 sid)
{
	return -EOPNOTSUPP;
}

static int pmic_arb_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);

	pr_debug("op:0x%x sid:%d\n", opc, sid);

	/* Check for valid non-data command */
	if (opc < SPMI_CMD_RESET || opc > SPMI_CMD_WAKEUP)
		return -EINVAL;

	return pmic_arb->ver->non_data_cmd(pmic_arb, opc, sid);
}

static const struct spmi_pmic_arb_ver spmi_pmic_arb_v1 = {
	.non_data_cmd		= pmic_arb_non_data_cmd_v1,
	.chnl_ofst		= pmic_arb_chnl_ofst_v1,
	.fmt_cmd		= pmic_arb_fmt_cmd_v1,
	.owner_acc_status	= pmic_arb_owner_acc_status_v1,
	.acc_enable		= pmic_arb_acc_enable_v1,
	.irq_status		= pmic_arb_irq_status_v1,
	.irq_clear		= pmic_arb_irq_clear_v1,
	.regs			= pmic_arb_regs_v1,
};

static const struct spmi_pmic_arb_ver spmi_pmic_arb_v2 = {
	.non_data_cmd		= pmic_arb_non_data_cmd_v2,
	.chnl_ofst		= pmic_arb_chnl_ofst_v2,
	.fmt_cmd		= pmic_arb_fmt_cmd_v2,
	.owner_acc_status	= pmic_arb_owner_acc_status_v2,
	.acc_enable		= pmic_arb_acc_enable_v2,
	.irq_status		= pmic_arb_irq_status_v2,
	.irq_clear		= pmic_arb_irq_clear_v2,
	.regs			= pmic_arb_regs_v2,
};

static int pmic_arb_read_cmd(struct spmi_controller *ctrl,
				u8 opc, u8 sid, u16 addr, u8 bc, u8 *buf)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	unsigned long flags;
	u32 cmd;
	int rc;
	int chnl_ofst = pmic_arb->ver->chnl_ofst(pmic_arb, sid, addr, &rc);

	if (rc < 0)
		return rc;

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

	cmd = pmic_arb->ver->fmt_cmd(opc, sid, addr, bc);

	spin_lock_irqsave(&pmic_arb->lock, flags);
	pmic_arb_save_stat_before_txn(pmic_arb);

	pmic_arb_set_rd_cmd(pmic_arb, chnl_ofst + PMIC_ARB_CMD, cmd);
	rc = pmic_arb_wait_for_done(pmic_arb, pmic_arb->rdbase, sid, addr);
	if (rc)
		goto done;

	/* Read from FIFO, note 'bc' is actually number of bytes minus 1 */
	pa_read_data(pmic_arb, buf, chnl_ofst + PMIC_ARB_RDATA0,
							min_t(u8, bc, 3));

	if (bc > 3)
		pa_read_data(pmic_arb, buf + 4,
				chnl_ofst + PMIC_ARB_RDATA1, bc - 4);

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
	int chnl_ofst = pmic_arb->ver->chnl_ofst(pmic_arb, sid, addr, &rc);

	if (rc < 0)
		return rc;

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

	cmd = pmic_arb->ver->fmt_cmd(opc, sid, addr, bc);

	/* Write data to FIFOs */
	spin_lock_irqsave(&pmic_arb->lock, flags);
	pmic_arb_save_stat_before_txn(pmic_arb);
	pa_write_data(pmic_arb, buf, chnl_ofst + PMIC_ARB_WDATA0,
							min_t(u8, bc, 3));

	if (bc > 3)
		pa_write_data(pmic_arb, buf + 4,
				chnl_ofst + PMIC_ARB_WDATA1, bc - 4);

	/* Start the transaction */
	pmic_arb_write(pmic_arb, chnl_ofst + PMIC_ARB_CMD, cmd);

	rc = pmic_arb_wait_for_done(pmic_arb, pmic_arb->wrbase, sid, addr);
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

static void dbg_dump_bad_irq_request(struct spmi_pmic_arb_dev *pmic_arb,
					u8 apid, u16 ppid, const char *msg)
{
	dev_err(pmic_arb->dev, "bad request: %s APID:0x%02x PPID:0x%03x\n",
							msg, apid, ppid);

	/* dump the stack to trace the caller */
	dump_stack();

	dev_info(pmic_arb->dev, "APID => PPID mapping table:\n");
	for (apid = pmic_arb->min_intr_apid;
		apid <= pmic_arb->max_intr_apid; ++apid)
		if (is_apid_valid(pmic_arb, apid))
			dev_info(pmic_arb->dev, "0x%02x => 0x%03x\n", apid,
					get_peripheral_id(pmic_arb, apid));
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
		if (owner != pmic_arb->ee) {
			dev_err(pmic_arb->dev, "PPID 0x%x incorrect owner %d\n",
				ppid, owner);
			return PMIC_ARB_MAX_PERIPHS;
		}

		/* Check if already mapped */
		if (pmic_arb->periph_id_map[apid] & PMIC_ARB_PERIPH_ID_VALID) {
			if (ppid != old_ppid) {
				dbg_dump_bad_irq_request(pmic_arb, apid, ppid,
						"map irq: apid already mapped");
				return PMIC_ARB_MAX_PERIPHS;
			}
			return apid;
		}

		pmic_arb->periph_id_map[apid] = ppid | PMIC_ARB_PERIPH_ID_VALID;

		if ((apid < pmic_arb->max_periph_intrs)
			&& (apid > pmic_arb->max_intr_apid))
			pmic_arb->max_intr_apid = apid;

		if (apid < pmic_arb->min_intr_apid)
			pmic_arb->min_intr_apid = apid;

		return apid;
	}

	dev_err(pmic_arb->dev, "Unknown ppid 0x%x\n", ppid);
	return PMIC_ARB_MAX_PERIPHS;
}

/*
 * pmic_arb_pic_enable: Enable interrupt at the PMIC Arbiter PIC
 *
 * This function is a callback of request_irq(a PMIC interrupt #).
 */
static int pmic_arb_pic_enable(struct spmi_controller *ctrl,
				struct qpnp_irq_spec *spec, uint32_t data)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	u8 apid = data & PMIC_ARB_APID_MASK;
	unsigned long flags;
	u32 status;

	dev_dbg(pmic_arb->dev, "PIC enable, apid:0x%x, sid:0x%x, pid:0x%x\n",
				apid, spec->slave, spec->per);

	if ((apid < pmic_arb->min_intr_apid) ||
		(apid > pmic_arb->max_intr_apid) ||
		(!is_apid_valid(pmic_arb, apid))) {
		dbg_dump_bad_irq_request(pmic_arb, apid,
				PMIC_ARB_TO_PPID(spec->slave, spec->per),
				"enable irq: invalid apid");
		return -EINVAL;
	}

	spin_lock_irqsave(&pmic_arb->lock, flags);
	status = spmi_pic_acc_en_rd(pmic_arb, spec->slave, spec->per, apid,
								"pic-en");
	if (!(status & SPMI_PIC_ACC_ENABLE_BIT)) {
		status = status | SPMI_PIC_ACC_ENABLE_BIT;
		spmi_pic_acc_en_wr(pmic_arb, status, spec->slave, spec->per,
								apid, "pic-en");
		/* Interrupt needs to be enabled before returning to caller */
		wmb();
	}
	spin_unlock_irqrestore(&pmic_arb->lock, flags);
	return 0;
}

/*
 * pmic_arb_pic_disable: Disable interrupt at the PMIC Arbiter PIC
 *
 * This function is a callback of free_irq(a PMIC interrupt #).
 */
static int pmic_arb_pic_disable(struct spmi_controller *ctrl,
				struct qpnp_irq_spec *spec, uint32_t data)
{
	struct spmi_pmic_arb_dev *pmic_arb = spmi_get_ctrldata(ctrl);
	u8 apid = data & PMIC_ARB_APID_MASK;
	unsigned long flags;
	u32 status;

	dev_dbg(pmic_arb->dev, "PIC disable, apid:0x%x, sid:0x%x, pid:0x%x\n",
				apid, spec->slave, spec->per);

	if ((apid < pmic_arb->min_intr_apid) ||
		(apid > pmic_arb->max_intr_apid) ||
		(!is_apid_valid(pmic_arb, apid))) {
		dbg_dump_bad_irq_request(pmic_arb, apid,
				PMIC_ARB_TO_PPID(spec->slave, spec->per),
				"disable irq: invalid apid");
		return -EINVAL;
	}

	spin_lock_irqsave(&pmic_arb->lock, flags);
	status = spmi_pic_acc_en_rd(pmic_arb, spec->slave, spec->per, apid,
								"pic-en");
	if (status & SPMI_PIC_ACC_ENABLE_BIT) {
		/* clear the enable bit and write */
		status = status & ~SPMI_PIC_ACC_ENABLE_BIT;
		spmi_pic_acc_en_wr(pmic_arb, status, spec->slave, spec->per,
								apid, "pic-en");
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

	status = spmi_pic_acc_en_rd(pmic_arb, sid, pid, apid, "isr");
	if (!(status & SPMI_PIC_ACC_ENABLE_BIT)) {
		/*
		 * All interrupts from this peripheral are disabled
		 * don't bother calling the qpnpint handler
		 */
		return IRQ_HANDLED;
	}

	/* Read the peripheral specific interrupt bits */
	status = readl_relaxed(intr + pmic_arb->ver->irq_status(apid));

	if (!show) {
		/* Clear the peripheral interrupts */
		writel_relaxed(status, intr + pmic_arb->ver->irq_clear(apid));
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
	u8 ee = pmic_arb->ee;
	u32 ret = IRQ_NONE;
	u32 status;

	int first = pmic_arb->min_intr_apid >> 5;
	int last = pmic_arb->max_intr_apid >> 5;
	int i, j;
	/* status based dispatch */
	bool acc_valid = false;
	u32 irq_status = 0;


	dev_dbg(pmic_arb->dev, "Peripheral interrupt detected\n");

	/* Check the accumulated interrupt status */
	for (i = first; i <= last; ++i) {
		status = readl_relaxed(pmic_arb->intr +
					pmic_arb->ver->owner_acc_status(ee, i));
		if (status)
			acc_valid = true;

		if ((i == 0) && (status & pmic_arb->irq_acc0_init_val)) {
			dev_dbg(pmic_arb->dev, "Ignoring IRQ acc[0] mask:0x%x\n",
					status & pmic_arb->irq_acc0_init_val);
			status &= ~pmic_arb->irq_acc0_init_val;
		}

		for (j = 0; status && j < 32; ++j, status >>= 1) {
			if (status & 0x1) {
				u8 id = (i * 32) + j;

				ret |= periph_interrupt(pmic_arb, id, show);
			}
		}
	}

	/* ACC_STATUS is empty but IRQ fired check IRQ_STATUS */
	if (!acc_valid) {
		for (i = pmic_arb->min_intr_apid; i <= pmic_arb->max_intr_apid;
				i++) {
			if (!is_apid_valid(pmic_arb, i))
				continue;
			irq_status = readl_relaxed(pmic_arb->intr +
					pmic_arb->ver->irq_status(i));
			if (irq_status) {
				dev_dbg(pmic_arb->dev,
					"Dispatching for IRQ_STATUS_REG:0x%lx IRQ_STATUS:0x%x\n",
					(ulong) pmic_arb->ver->irq_status(i),
					irq_status);
				ret |= periph_interrupt(pmic_arb, i, show);
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
	int first = pmic_arb->min_intr_apid;
	int last = pmic_arb->max_intr_apid;
	int i;

	for (i = first; i <= last; ++i) {
		if (!is_apid_valid(pmic_arb, i))
			continue;

		seq_printf(file, "APID 0x%.2x = PPID 0x%.3x. Enabled:%d\n",
			i, get_peripheral_id(pmic_arb, i),
			readl_relaxed(pmic_arb->intr +
						pmic_arb->ver->acc_enable(i)));
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

/* mask interrupts that are stack at boot time */
static void pmic_arb_handle_stuck_irqs(struct spmi_pmic_arb_dev *pmic_arb)
{
	int apid;

	/* we only saw the firt 32bit accumulator get currupted at boot */
	pmic_arb->irq_acc0_init_val = readl_relaxed(pmic_arb->intr +
			pmic_arb->ver->owner_acc_status(pmic_arb->ee, 0));

	if (!pmic_arb->irq_acc0_init_val)
		return;

	dev_err(pmic_arb->dev, "non-zero irq-accumulator[0]:0x%x\n",
					pmic_arb->irq_acc0_init_val);

	for (apid = 0; apid < 32 ; ++apid) {
		u32 mask = BIT(apid);
		if (pmic_arb->irq_acc0_init_val & mask) {
			u32 owner = SPMI_OWNERSHIP_PERIPH2OWNER(
					readl_relaxed(pmic_arb->cnfg +
					      SPMI_OWNERSHIP_TABLE_REG(apid)));
			/* don't mask interrupts that we own */
			if (owner == pmic_arb->ee)
				pmic_arb->irq_acc0_init_val &= ~mask;
		}
	}
}

static int
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

static int pmic_arb_chnl_tbl_create(struct spmi_pmic_arb_dev *pmic_arb)
{
	u16	chnl;
	u16	ppid;
	u32	reg;
	size_t	bitmap_sz = sizeof(*pmic_arb->valid_ppid_bitmap) *
			    DIV_ROUND_UP(PMIC_ARB_PPID_CNT, BITS_PER_LONG);

	pmic_arb->ppid_2_chnl_tbl = devm_kzalloc(pmic_arb->dev,
						 PMIC_ARB_PPID_CNT, GFP_KERNEL);
	if (!pmic_arb->ppid_2_chnl_tbl)
		return -ENOMEM;

	pmic_arb->valid_ppid_bitmap = devm_kzalloc(pmic_arb->dev, bitmap_sz,
						   GFP_KERNEL);
	if (!pmic_arb->valid_ppid_bitmap)
		return -ENOMEM;

	/*
	 * The PMIC_ARB_REG_CHNL registers are a table mapping channel number
	 * to SID + PID (PPID). We create an invert of that table here for
	 * optimization of mapping SID+PID to channel number.
	 */
	for (chnl = 0; chnl < pmic_arb->max_peripherals; ++chnl) {
		reg  = readl_relaxed(pmic_arb->base + PMIC_ARB_REG_CHNL(chnl));
		ppid = (reg >> 8) & 0xFFF;

		pmic_arb->ppid_2_chnl_tbl[ppid] = chnl;
		pmic_arb->valid_ppid_bitmap[BIT_WORD(ppid)] |= BIT_MASK(ppid);
	}
	return 0;
}

/*
 * pmic_arb_devm_ioremap: get resource and ioremap it
 *
 * @res_name name of resource
 * @virt input parameter, will be set with the resources mapped virtual adderss
 * @phys input parameter, if not null, will be set to the resources physical
 *    address. If null, no-op.
 */
static int pmic_arb_devm_ioremap(struct platform_device *pdev,
		const char *res_name, void __iomem **virt, phys_addr_t *phys)
{
	struct resource *mem_res =
	     platform_get_resource_byname(pdev, IORESOURCE_MEM, res_name);

	if (!mem_res) {
		dev_err(&pdev->dev, "error missing config of %s reg-space\n",
								res_name);
		return -ENODEV;
	}

	*virt = devm_ioremap(&pdev->dev, mem_res->start,
							resource_size(mem_res));

	dev_dbg(&pdev->dev,
		"%s ioremap(phy:0x%lx vir:0x%p len:0x%lx)\n", res_name,
		(ulong) mem_res->start, *virt, (ulong) resource_size(mem_res));

	if (!(*virt)) {
		dev_err(&pdev->dev,
			"error %s ioremap(phy:0x%lx len:0x%lx) failed\n",
			res_name, (ulong) mem_res->start,
			(ulong) resource_size(mem_res));
		return -ENOMEM;
	}

	if (phys)
		*phys = mem_res->start;

	return 0;
}

static int pmic_arb_version_specific_init(struct spmi_pmic_arb_dev *pmic_arb,
						struct platform_device *pdev)
{
	int ret;
	u32 version;

	version = readl_relaxed(pmic_arb->base + PMIC_ARB_VERSION);

	if (version < PMIC_ARB_V2_MIN) {
		dev_err(&pdev->dev, "PMIC Arb Version-1 0x%x\n", version);
		pmic_arb->rdbase	 = pmic_arb->base;
		pmic_arb->wrbase	 = pmic_arb->base;
		pmic_arb->dbg.rdbase_phy = pmic_arb->dbg.base_phy;
		pmic_arb->dbg.wrbase_phy = pmic_arb->dbg.base_phy;
		pmic_arb->ver		 = &spmi_pmic_arb_v1;
	} else {
		dev_err(&pdev->dev, "PMIC Arb Version-2 0x%x\n", version);
		ret = pmic_arb_chnl_tbl_create(pmic_arb);
		if (ret)
			return ret;

		ret = pmic_arb_devm_ioremap(pdev, "obsrvr", &pmic_arb->rdbase,
						&pmic_arb->dbg.rdbase_phy);
		if (ret)
			return ret;

		ret = pmic_arb_devm_ioremap(pdev, "chnls", &pmic_arb->wrbase,
						&pmic_arb->dbg.wrbase_phy);
		if (ret)
			return ret;

		pmic_arb->ver = &spmi_pmic_arb_v2;
	}
	return 0;
}

static int spmi_pmic_arb_probe(struct platform_device *pdev)
{
	struct spmi_pmic_arb_dev *pmic_arb;
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
	pmic_arb->dev = &pdev->dev;

	ret = pmic_arb_devm_ioremap(pdev, "core", &pmic_arb->base,
						&pmic_arb->dbg.base_phy);
	if (ret)
		return ret;

	ret = spmi_pmic_arb_get_property(pdev, "qcom,pmic-arb-max-peripherals",
					&prop);
	if (ret)
		prop = PMIC_ARB_PERIPHS_CHNL_DEFAULT;
	pmic_arb->max_peripherals = prop;

	ret = pmic_arb_version_specific_init(pmic_arb, pdev);
	if (ret)
		return ret;

	ret = pmic_arb_devm_ioremap(pdev, "intr", &pmic_arb->intr,
						&pmic_arb->dbg.intr_phy);
	if (ret)
		return ret;

	ret = pmic_arb_devm_ioremap(pdev, "cnfg", &pmic_arb->cnfg, NULL);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(pmic_arb->mapping_table); ++i)
		pmic_arb->mapping_table[i] = readl_relaxed(
				pmic_arb->cnfg + SPMI_MAPPING_TABLE_REG(i));

	pmic_arb->pic_irq = platform_get_irq(pdev, 0);
	if (!pmic_arb->pic_irq) {
		dev_err(&pdev->dev, "missing IRQ resource\n");
		return -ENODEV;
	}

	/* Get properties from the device tree */
	ret = spmi_pmic_arb_get_property(pdev, "cell-index", &cell_index);
	if (ret)
		return -ENODEV;

	ret = spmi_pmic_arb_get_property(pdev, "qcom,pmic-arb-ee", &prop);
	if (ret)
		return -ENODEV;
	pmic_arb->ee = (u8)prop;

	ret = spmi_pmic_arb_get_property(pdev, "qcom,pmic-arb-channel",
					&prop);
	if (ret)
		return -ENODEV;
	pmic_arb->channel = (u8)prop;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,reserved-channel",
				   &prop);
	pmic_arb->reserved_chnl = ret ? -1 : (u8)prop;

	pmic_arb->allow_wakeup = !of_property_read_bool(pdev->dev.of_node,
					"qcom,not-wakeup");
	if (pmic_arb->allow_wakeup) {
		ret = irq_set_irq_wake(pmic_arb->pic_irq, 1);
		if (unlikely(ret))
			pr_err("Unable to set wakeup irq, err=%d\n", ret);
	}

	ret = spmi_pmic_arb_get_property(pdev,
			"qcom,pmic-arb-max-periph-interrupts", &prop);
	if (ret)
		prop = PMIC_ARB_PERIPHS_INTR_DEFAULT;

	pmic_arb->max_periph_intrs = prop;
	pmic_arb->max_intr_apid = 0;
	pmic_arb->min_intr_apid = PMIC_ARB_MAX_PERIPHS - 1;

	platform_set_drvdata(pdev, pmic_arb);
	spmi_set_ctrldata(&pmic_arb->controller, pmic_arb);

	spin_lock_init(&pmic_arb->lock);

	pmic_arb->controller.nr = cell_index;
	pmic_arb->controller.dev.parent = pdev->dev.parent;
	pmic_arb->controller.dev.of_node = of_node_get(pdev->dev.of_node);

	pmic_arb_handle_stuck_irqs(pmic_arb);

	/* Callbacks */
	pmic_arb->controller.cmd = pmic_arb_cmd;
	pmic_arb->controller.read_cmd = pmic_arb_read_cmd;
	pmic_arb->controller.write_cmd =  pmic_arb_write_cmd;

	ret = devm_request_irq(&pdev->dev, pmic_arb->pic_irq,
		pmic_arb_periph_irq, IRQF_TRIGGER_HIGH, pdev->name, pmic_arb);
	if (ret) {
		dev_err(&pdev->dev, "request IRQ failed\n");
		return ret;
	}

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

static int spmi_pmic_arb_remove(struct platform_device *pdev)
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
	.remove		= spmi_pmic_arb_remove,
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
