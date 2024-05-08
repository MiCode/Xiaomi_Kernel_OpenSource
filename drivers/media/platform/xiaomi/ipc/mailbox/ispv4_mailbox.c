// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 *
 */
#define pr_fmt(fmt) "ispv4 mbox: " fmt
//#define DEBUG

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/bitfield.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include "ispv4_regops.h"
#include <linux/delay.h>
#include "../../bus/pcie/ispv4_boot.h"
#include "../../bus/pcie/ispv4_notify.h"

// #define ISPV4_MBOX_FAKE_TEST
#define HIGH_PERF_MASK (BIT(0)|BIT(10))

#define MBOX_BASE_ADDR 0x02150000
#define REG_SEND_DATA 0x0
#define REG_MBOX_INFO 0x4
#define REG_RECV_DATA 0x8
#define REG_MBOX_CLEAR 0xc
#define REG_MBOX_INT_RAW_STATUS 0x10
#define REG_MBOX_INT_STATUS 0x14
#define REG_MBOX_INT_MASK 0x18
#define REG_MBOX_INT_CLEAR 0x1c
#define REG_MBOX_INT_FORCE 0x20
#define REG_CPU2AP_DATA 0x2c
#define REG_CPU2AP_CNT 0x28
#define REG_CPU2AP_INT_RAW_STATUS 0x34
#define REG_CPU2AP_INT_CLEAR 0x40

//GPIO INTR
#define ISPV4_AP_INTC_ADDR_OFFSET (0x0D400000 + 0x0002C000)
#define AP_INTC_G0R0_INT_MASK_OFFSET 0
#define AP_INTC_G0R0_INT_STATUS_OFFSET 4
#define MBOX_INTC_AP2CPU_MASK 0xfffff7ff
#define MBOX_INTC_CPU2AP_MASK 0xfffffbff
#define MBOX_INTC_GPIO_SWITCH (0x20 * 1)

#define REG_UNDERFLOW_MASK GENMASK(2, 2)
#define REG_OVERFLOW_MASK GENMASK(1, 1)
#define REG_INTFLAG_MASK GENMASK(0, 0)

#define REG_RCOUNT_MASK GENMASK(6, 0)

// 16 * 128Bit
#define MBOX_MAX_DEPTH 16
// 8Bit channel id
#define CHANNEL_MASK GENMASK(7, 0)
#define CHANNEL_SHIFT 24
// max channel num.
#define CHANNEL_NUM 16

extern struct dentry *ispv4_debugfs;
static irqreturn_t xm_ispv4_mailbox_rx_irq(int irq, void *data);
static irqreturn_t xm_ispv4_mailbox_rx_thirq(int irq, void *data);
static irqreturn_t xm_ispv4_mailbox_rx_isr(int irq, void *data);

enum XM_MB_TYPE {
	XM_MB_PCI,
	XM_MB_SPI,
	XM_MB_PCI_INTC,
};

struct xm_ispv4_mailbox {
	struct mbox_controller controller;
	void __iomem *reg_base;
	void __iomem *intc_base;
	spinlock_t lock;
	int irq;
	unsigned long irqf;
	struct device *dev;
	u32 n_chans;
	atomic_t open_cnt;
	enum XM_MB_TYPE type;
	struct work_struct send_work;
	atomic_t send_finish;
	void *send_work_data;
	void *send_work_chan;
	u32 rdata[4];
	u32 high_perf_mask;
	bool pci_err;
	bool ioirq_disable;

	int (*reg_read)(struct xm_ispv4_mailbox *, u32 addr, u32 *data);
	int (*reg_write)(struct xm_ispv4_mailbox *, u32 addr, u32 data);
};

static void reset_pcie_err(struct xm_ispv4_mailbox *mb)
{
	if (mb->ioirq_disable == false) {
		disable_irq_nosync(mb->irq);
		mb->ioirq_disable = true;
		pr_err("disable_irq %s.\n", __FUNCTION__);
	}
	//_pci_reset();
	pr_err("_pci_reset for link err. %s.\n", __FUNCTION__);
	ispv4_notifier_call_chain(1, NULL);
}

static int xm_mbox_spir(struct xm_ispv4_mailbox *mb, u32 addr, u32 *data)
{
	int ret;
	if (data == NULL) {
		return -ENOPARAM;
	}
	ret = ispv4_regops_dread(MBOX_BASE_ADDR + addr, data);
	if (ret != 0) {
		*data = 0xffffffff;
		pr_err("spi read 0x%lx=0x%lx\n", addr, *data);
	}
	pr_debug("spi read 0x%lx=0x%lx\n", addr, *data);
	return ret;
}

static int xm_mbox_spiw(struct xm_ispv4_mailbox *mb, u32 addr, u32 data)
{
	pr_debug("spi write 0x%lx=0x%lx\n", addr, data);
	return ispv4_regops_write(MBOX_BASE_ADDR + addr, data);
}

static int xm_mbox_pcir(struct xm_ispv4_mailbox *mb, u32 addr, u32 *data)
{
	if (data == NULL) {
		return -ENOPARAM;
	}
	*data = readl_relaxed(mb->reg_base + addr);
	pr_debug("pcie read 0x%lx=0x%lx\n", addr, *data);
	if (*data == 0xffffffff) {
		mb->pci_err = true;
		pr_err("pcie read 0x%lx=0x%lx\n", addr, *data);
	}
	return 0;
}

uint32_t global_addr = 0;

int xm_mbox_pcir_debug(void *mb, u64 *data)
{
	uint32_t d;
	xm_mbox_pcir(mb, global_addr, &d);
	*data = d;
	return 0;
}

int xm_mbox_pciw_debug(void *mb, u64 val)
{
	global_addr = val;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(dbg_ispv4_pcir,xm_mbox_pcir_debug, xm_mbox_pciw_debug, "%llu/n");

static int xm_mbox_pciw(struct xm_ispv4_mailbox *mb, u32 addr, u32 data)
{
	writel_relaxed(data, mb->reg_base + addr);
	pr_debug("pcie write 0x%lx=0x%lx\n", addr, data);
	return 0;
}

static inline bool xm_mb_is_full(struct xm_ispv4_mailbox *mb)
{
	u32 info, c = 0;
	int ret;

	ret = mb->reg_read(mb, REG_MBOX_INFO, &info);
	if (ret != 0) {
		return true;
	}

	c = FIELD_GET(REG_RCOUNT_MASK, info);
	/* Make sure there are more than 4DW space to write */
	return c <= (MBOX_MAX_DEPTH - 1) * 4 ? false : true;
}

static bool xm_mailbox_tx_done(struct mbox_chan *chan)
{
	struct xm_ispv4_mailbox *mb =
		container_of(chan->mbox, struct xm_ispv4_mailbox, controller);
	bool mb_full_flag = false;
	if (mb->pci_err)
		return false;
	mb_full_flag = xm_mb_is_full(mb);
	if (mb->pci_err) {
		reset_pcie_err(mb);
		return false;
	}
	return !mb_full_flag;
}

static int xm_mailbox_send_data(struct mbox_chan *chan, void *data)
{
	int ret, i, chan_id, regops_rc;
	u32 check = 0, check_reg;
	unsigned long flags;
	bool check_full = false;
	bool mb_full_flag = false;

	struct xm_ispv4_mailbox *mb =
		container_of(chan->mbox, struct xm_ispv4_mailbox, controller);

	chan_id = (u64)chan->con_priv;
	if (chan_id >= CHANNEL_NUM) {
		return -1;
	}

	pr_info("chan [%d] into %s\n", chan_id, __func__);

	if (mb->pci_err) {
		pr_err("chan [%d] stop send mb data for pci_err\n", chan_id);
		reset_pcie_err(mb);
		return 0;
	}

	if (FIELD_GET(CHANNEL_MASK, *(u32 *)data) != 0) {
		pr_warn("set channel id bits [%d:0x%x]\n", chan_id, *(u32 *)data);
		dump_stack();
	}
	*(u32 *)data &= ~FIELD_PREP(CHANNEL_MASK, FIELD_MAX(CHANNEL_MASK));
	*(u32 *)data |= FIELD_PREP(CHANNEL_MASK, chan_id);

	if (mb->type == XM_MB_PCI || mb->type == XM_MB_PCI_INTC) {
		spin_lock_irqsave(&mb->lock, flags);
		check_full = true;
	}
	// Need not check full in SPI type.
	mb_full_flag = xm_mb_is_full(mb);
	if (mb->pci_err) {
		pr_err("chan [%d] stop send mb data for pci_err\n", chan_id);
		ret = 0;
		goto err;
	}

	if (check_full && mb_full_flag) {
		ret = -1;
	} else {
		for (i = 0; i < 4; i++) {
			regops_rc = mb->reg_write(mb, REG_SEND_DATA,
						  *((u32 *)data + i));
			if (regops_rc != 0) {
				ret = -1;
				goto err;
			}
		}
		regops_rc =
			mb->reg_read(mb, REG_MBOX_INT_RAW_STATUS, &check_reg);
		if (regops_rc != 0) {
			ret = -1;
			goto err;
		}

		check = FIELD_GET(REG_OVERFLOW_MASK, check_reg);
		ret = 0;
	}
	if (mb->type == XM_MB_PCI || mb->type == XM_MB_PCI_INTC)
		spin_unlock_irqrestore(&mb->lock, flags);

	pr_info("chan [%d] done %s\n", chan_id, __func__);

	if (check != 0)
		pr_err("overflow or underflow\n");

	return 0;

err:
	if (mb->type == XM_MB_PCI || mb->type == XM_MB_PCI_INTC)
		spin_unlock_irqrestore(&mb->lock, flags);
	return ret;
}

static void xm_mbox_send_work(struct work_struct *work)
{
	struct xm_ispv4_mailbox *mb =
		container_of(work, struct xm_ispv4_mailbox, send_work);
	xm_mailbox_send_data(mb->send_work_chan, mb->send_work_data);

	while (unlikely(xm_mb_is_full(mb)))
		udelay(100);

	pr_debug("unlock [send finish]. %d", __LINE__);
	atomic_set(&mb->send_finish, 0);
}

static int xm_mailbox_send_data_defer(struct mbox_chan *chan, void *data)
{
	int retry = 30;
	struct xm_ispv4_mailbox *mb =
		container_of(chan->mbox, struct xm_ispv4_mailbox, controller);

	pr_debug("Into xm_mailbox_send_data_defer.");

	if (atomic_cmpxchg(&mb->send_finish, 0, 1))
		return -1;

	pr_debug("lock [send finish]. %d", __LINE__);

	mb->send_work_chan = chan;
	mb->send_work_data = data;

	while (retry--) {
		if (queue_work(system_highpri_wq, &mb->send_work))
			break;
	}

	if (unlikely(retry <= 0)) {
		pr_err("more retry...");
		pr_debug("unlock [send finish]. %d", __LINE__);
		atomic_set(&mb->send_finish, 0);
		return -1;
	}

	return 0;
}

static bool xm_mailbox_defer_tx_done(struct mbox_chan *chan)
{
	struct xm_ispv4_mailbox *mb =
		container_of(chan->mbox, struct xm_ispv4_mailbox, controller);
	return !atomic_read(&mb->send_finish);
}

static int _config_intc(struct xm_ispv4_mailbox *mb)
{
	int regops_rc = 0;
	if (mb->type == XM_MB_SPI || mb->type == XM_MB_PCI_INTC) {
		u32 intc_mask;
		regops_rc =
			ispv4_regops_read(ISPV4_AP_INTC_ADDR_OFFSET +
						  AP_INTC_G0R0_INT_MASK_OFFSET +
						  MBOX_INTC_GPIO_SWITCH,
					  &intc_mask);
		if (regops_rc != 0) {
			return regops_rc;
		}
		intc_mask |= ~MBOX_INTC_AP2CPU_MASK;
		intc_mask &= MBOX_INTC_CPU2AP_MASK;
		regops_rc = ispv4_regops_write(
			ISPV4_AP_INTC_ADDR_OFFSET +
				AP_INTC_G0R0_INT_MASK_OFFSET +
				MBOX_INTC_GPIO_SWITCH,
			intc_mask);
		if (regops_rc != 0) {
			return regops_rc;
		}
	}
	return 0;
}

static int xm_mailbox_startup(struct mbox_chan *chan)
{
	u32 opencnt;
	int regops_rc;
	int ret = 0;
	irqreturn_t (*isr_f)(int, void *);
	irqreturn_t (*irqth_f)(int, void *);
	struct xm_ispv4_mailbox *mb =
		container_of(chan->mbox, struct xm_ispv4_mailbox, controller);

	opencnt = atomic_fetch_inc_relaxed(&mb->open_cnt);
	if (opencnt == 0) {
		regops_rc = mb->reg_write(mb, REG_MBOX_CLEAR, 1);
		if (regops_rc != 0) {
			return -1;
		}
		// Mask under/over flow intr.
		regops_rc = mb->reg_write(mb, REG_MBOX_INT_MASK, 6);
		if (regops_rc != 0) {
			return regops_rc;
		}
		regops_rc = _config_intc(mb);
		if (regops_rc != 0)
			return regops_rc;

#ifndef ISPV4_MBOX_FAKE_TEST
		if (mb->type == XM_MB_PCI_INTC || mb->type == XM_MB_PCI) {
			mb->irqf |= IRQF_NO_THREAD;
			isr_f = xm_ispv4_mailbox_rx_isr;
			irqth_f = xm_ispv4_mailbox_rx_thirq;
		} else {
			isr_f = NULL;
			irqth_f = xm_ispv4_mailbox_rx_irq;
		}
		ret = request_threaded_irq(mb->irq, isr_f, irqth_f, mb->irqf,
					   dev_name(mb->dev), mb);
		if (ret) {
			dev_err(mb->dev, "failed to request irq %d\n", ret);
			ret = -EINVAL;
		}
#endif
		pr_info("ispv4: mbox startup!\n");
	}

	return ret;
}

static void xm_mailbox_shutdown(struct mbox_chan *chan)
{
	u32 opencnt;
	struct xm_ispv4_mailbox *mb =
		container_of(chan->mbox, struct xm_ispv4_mailbox, controller);
	/* Make sure no message in dealing */
	synchronize_irq(mb->irq);
	opencnt = atomic_dec_if_positive(&mb->open_cnt);
	if (opencnt == 0) {
#ifndef ISPV4_MBOX_FAKE_TEST
		free_irq(mb->irq, mb);
#endif
		if (mb->ioirq_disable) {
			enable_irq(mb->irq);
			mb->ioirq_disable = false;
			pr_info("ispv4: mbox re-enable_irq!\n");
		}
		pr_info("ispv4: mbox shutdown!\n");
		mb->pci_err = false;
	}
}

static const struct mbox_chan_ops xm_mailbox_ops = {
	.send_data = xm_mailbox_send_data,
	.shutdown = xm_mailbox_shutdown,
	.startup = xm_mailbox_startup,
	.last_tx_done = xm_mailbox_tx_done,
};

static const struct mbox_chan_ops xm_mailbox_spi_ops = {
	.send_data = xm_mailbox_send_data_defer,
	.shutdown = xm_mailbox_shutdown,
	.startup = xm_mailbox_startup,
	.last_tx_done = xm_mailbox_defer_tx_done,
};

static bool mask_irq = false;
static struct dentry *mbox_debugfs;

static int check_realirq(struct xm_ispv4_mailbox *mb, bool *f)
{
	int ret = 0;
	u32 intc_status = 0;
	switch (mb->type) {
	case XM_MB_PCI_INTC:
		if (mb->intc_base != NULL) {
			intc_status = readl_relaxed(
				mb->intc_base + AP_INTC_G0R0_INT_STATUS_OFFSET +
				MBOX_INTC_GPIO_SWITCH);
		}
		break;
	case XM_MB_SPI:
		ret = ispv4_regops_read(ISPV4_AP_INTC_ADDR_OFFSET +
						AP_INTC_G0R0_INT_STATUS_OFFSET +
						MBOX_INTC_GPIO_SWITCH,
					&intc_status);
		if (ret != 0)
			return ret;

		break;
	case XM_MB_PCI:
		*f = true;
		return 0;
	default:
		return -EFAULT;
	}
	pr_info("read status = 0x%x\n", intc_status);
	*f = ((intc_status & ~MBOX_INTC_CPU2AP_MASK) != 0);
	return 0;
}

__maybe_unused static irqreturn_t xm_ispv4_mailbox_rx_irq(int irq, void *data)
{
	u32 rdata[4], cid, check, check_reg;
	int regops_rc;
	int i;
	struct xm_ispv4_mailbox *mb = data;
	u32 cnt;
	bool pci_err = true;
	bool real_irq;
	ktime_t times, timed;

	pr_debug("Into %s.\n", __FUNCTION__);

	times = ktime_get();
	regops_rc = check_realirq(mb, &real_irq);
	if (regops_rc != 0) {
		pr_err("check realirq error %d\n", regops_rc);
		return IRQ_NONE;
	}

	if (!real_irq) {
		pr_debug("Not my irq\n");
		return IRQ_NONE;
	}

	regops_rc = mb->reg_read(mb, REG_CPU2AP_CNT, &cnt);
	if (regops_rc != 0 || cnt == 0) {
		goto early_ret;
	}

	if (unlikely(mask_irq)) {
		return IRQ_HANDLED;
	}

	// REG_CPU2AP_DATA only use in this irq.
	// Need not lock to protect.
	for (i = 0; i < 4; i++) {
		regops_rc = mb->reg_read(mb, REG_CPU2AP_DATA, &rdata[i]);
		if (regops_rc != 0) {
			goto early_ret;
		}
		if (rdata[i] != 0xffffffff)
			pci_err = false;
	}

	/* if rdata is all 0xffffffff, assume pci error */
	if(pci_err) {
		pr_err("xm_ispv4_mailbox_rx_irq err\n");
		return IRQ_NONE;
	}

	regops_rc = mb->reg_read(mb, REG_CPU2AP_INT_RAW_STATUS, &check_reg);
	if (regops_rc != 0) {
		goto early_ret;
	}

	check = FIELD_GET(REG_UNDERFLOW_MASK, check_reg);
	if(check != 0) {
		pr_err("met underflow!!!\n");
		return IRQ_NONE;
	}

	cid = FIELD_GET(CHANNEL_MASK, rdata[0]);
	if (WARN_ON(cid >= 16)) {
		pr_err("recv unknowned id!\n");
		return IRQ_NONE;
	}

	rdata[0] &= ~CHANNEL_MASK;

	if (mb->controller.chans[cid].cl)
		mbox_chan_received_data(&mb->controller.chans[cid], rdata);

	pr_debug("recv data finish [chan: %d].\n", cid);

	timed = ktime_get();
	timed = ktime_sub(timed, times);
	pr_info("[chan: %d] us=%d", cid, ktime_to_us(timed));
early_ret:
	// Clear interrupt.
	mb->reg_write(mb, REG_CPU2AP_INT_CLEAR, 1);
	return IRQ_HANDLED;
}

//void ispv4_pci_read_configsp(void *p);
void ispv4_pci_debugbar(void *addr);

__maybe_unused static irqreturn_t xm_ispv4_mailbox_rx_isr(int irq, void *data)
{
	u32 cid, check, check_reg;
	int i;
	struct xm_ispv4_mailbox *mb = data;
	u32 cnt;
	bool pci_err = true;
	bool real_irq;
	//ktime_t timez1, timez2, times, timed;
	ktime_t times, timed;

	pr_info("Into %s.\n", __FUNCTION__);
	//timez1 = ktime_get();
	//ispv4_pci_read_configsp(NULL);
	//timez2 = ktime_get();
	//timez2 = ktime_sub(timez2, timez1);
	//pr_err("read config sp us=%d", ktime_to_us(timez2));
	times = ktime_get();
	// PCIe will not return err code.
	(void)check_realirq(mb, &real_irq);
	if (!real_irq) {
		pr_debug("Not my irq\n");
		return IRQ_NONE;
	}

	mb->reg_read(mb, REG_CPU2AP_CNT, &cnt);
	if (cnt < 4) {
		goto early_ret;
	}

	if (unlikely(mask_irq)) {
		return IRQ_HANDLED;
	}

	// REG_CPU2AP_DATA only use in this irq.
	// Need not lock to protect.
	for (i = 0; i < 4; i++) {
		(void)mb->reg_read(mb, REG_CPU2AP_DATA, &mb->rdata[i]);
		if (mb->rdata[i] != 0xffffffff)
			pci_err = false;
	}

	/* if rdata is all 0xffffffff, assume pci error */
	if (pci_err) {
		pr_err("xm_ispv4_mailbox_rx_irq err\n");
		mb->pci_err = true;
		return IRQ_WAKE_THREAD;
	}

	(void)mb->reg_read(mb, REG_CPU2AP_INT_RAW_STATUS, &check_reg);
	check = FIELD_GET(REG_UNDERFLOW_MASK, check_reg);
	if (check != 0) {
		pr_err("met underflow!!!\n");
		goto early_ret;
	}

	cid = FIELD_GET(CHANNEL_MASK, mb->rdata[0]);
	if (WARN_ON(cid >= 16)) {
		pr_err("recv unknowned id!\n");
		goto early_ret;
	}

	if ((mb->high_perf_mask & BIT(cid)) != 0) {
		mb->rdata[0] &= ~CHANNEL_MASK;
		if (mb->controller.chans[cid].cl)
			mbox_chan_received_data(&mb->controller.chans[cid],
						mb->rdata);
	} else {
		// Clear interrupt.
		mb->reg_write(mb, REG_CPU2AP_INT_CLEAR, 1);
		return IRQ_WAKE_THREAD;
	}

	pr_debug("recv data finish(isr) [chan: %d].\n", cid);

	timed = ktime_get();
	timed = ktime_sub(timed, times);
	pr_info("recv data finish(isr) [chan: %d] us=%d in %s", cid, ktime_to_us(timed), __func__);
early_ret:
	// Clear interrupt.
	mb->reg_write(mb, REG_CPU2AP_INT_CLEAR, 1);
	return IRQ_HANDLED;
}

__maybe_unused static irqreturn_t xm_ispv4_mailbox_rx_thirq(int irq, void *data)
{
#define SPIDUMP(addr, name)                                                    \
	do {                                                                   \
		ispv4_regops_read(addr, &read_val);                            \
		pr_err("pci_err_dump reg " #name " =  0x%08x", read_val);      \
	} while (0)

#define SPIDUMPADDR(addr) SPIDUMP(addr, addr)

#define SPIDUMPADDRRG(addrs, addre) do {\
	int addr = addrs;\
	for (; addr <= addre; addr += 4) \
	SPIDUMPADDR(addr);\
} while(0)\

	u32 cid;
	struct xm_ispv4_mailbox *mb = data;
	ktime_t times, timed;

	pr_info("Into %s.\n", __FUNCTION__);

	if (mb->pci_err) {
		reset_pcie_err(mb);
		return IRQ_HANDLED;
	}

	if (mb->pci_err) {

		u32 read_val = 0xdeadbeef;

		SPIDUMP(REG_CPU2AP_CNT + MBOX_BASE_ADDR, mbox_count);
		SPIDUMP(0, iram_0_addr);

		pr_err("pci changing iram 0");
		ispv4_pci_debugbar(NULL);

		SPIDUMP(0, iram_0_addr);
		SPIDUMP(0x80000000, ddr_0_addr);

		SPIDUMP(0xCC00020, pci_state_machine);

		SPIDUMPADDRRG(0xCC000A8, 0xCC000AC);
		SPIDUMPADDRRG(0xCC001B0, 0xCC001BC);
		SPIDUMPADDRRG(0xD462000, 0xD462154);

		return IRQ_NONE;
	}

	times = ktime_get();

	cid = FIELD_GET(CHANNEL_MASK, mb->rdata[0]);
	if (WARN_ON(cid >= 16)) {
		pr_err("recv unknowned id!\n");
		return IRQ_NONE;
	}

	mb->rdata[0] &= ~CHANNEL_MASK;
	if (mb->controller.chans[cid].cl)
		mbox_chan_received_data(&mb->controller.chans[cid], mb->rdata);

	pr_debug("recv data finish(thread) [chan: %d].\n", cid);

	timed = ktime_get();
	timed = ktime_sub(timed, times);
	pr_info("recv data finish(thread) [chan: %d] us=%d in %s", cid, ktime_to_us(timed), __func__);

	return IRQ_HANDLED;
}

static int xm_ispv4_mailbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = NULL;
	struct xm_ispv4_mailbox *xm_mb;
	struct resource *res;
	struct resource *res_intc;
	unsigned long i, irq_flag = IRQF_ONESHOT;
	int ret;
	struct resource *irq_rsc;

	np = of_find_node_by_name(NULL, "xm_ispv4_mbox");
	if (!np) {
		dev_err(dev, "No DT found\n");
		return -ENODEV;
	}
	dev->of_node = np;

	xm_mb = devm_kzalloc(dev, sizeof(*xm_mb), GFP_KERNEL);
	if (!xm_mb) {
		ret = -ENOMEM;
		goto alloc_mb_err;
	}

	xm_mb->dev = dev;

	// mutex_init(&xm_mb->lock);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		return -EINVAL;
	}
	if (res->name == NULL) {
		xm_mb->type = XM_MB_PCI;
	} else {
		xm_mb->type = !strcmp(res->name, "spi") ? XM_MB_SPI : XM_MB_PCI;
	}
	res_intc = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res_intc != NULL) {
		xm_mb->intc_base = devm_ioremap(dev, res_intc->start,
						resource_size(res_intc));
		pr_info("found intc regspace");
	}
#ifdef FORCE_NOT_USE_HW_MSI
	if (xm_mb->type == XM_MB_PCI)
		xm_mb->type = XM_MB_PCI_INTC;
#endif
	if (xm_mb->type == XM_MB_SPI) {
		pr_info("probe mailbox using SPI\n");
		irq_flag |= IRQF_TRIGGER_HIGH | IRQF_SHARED;
		INIT_WORK(&xm_mb->send_work, xm_mbox_send_work);
		xm_mb->reg_read = xm_mbox_spir;
		xm_mb->reg_write = xm_mbox_spiw;
	} else {
		pr_info("probe mailbox using PCI\n");
		if (xm_mb->type == XM_MB_PCI_INTC)
			irq_flag |= IRQF_TRIGGER_HIGH | IRQF_SHARED;
		spin_lock_init(&xm_mb->lock);
		xm_mb->reg_read = xm_mbox_pcir;
		xm_mb->reg_write = xm_mbox_pciw;
	}
#ifndef ISPV4_MBOX_FAKE_TEST
	if (xm_mb->type == XM_MB_PCI || xm_mb->type == XM_MB_PCI_INTC) {
		xm_mb->reg_base =
			devm_ioremap(dev, res->start, resource_size(res));
		if (IS_ERR(xm_mb->reg_base)) {
			dev_err(dev, "no REG base address specified\n");
			ret = PTR_ERR(xm_mb->reg_base);
			goto err;
		}
		dev_info(dev, "mailbox map addr 0x%x=0x%x\n", res->start,
			 xm_mb->reg_base);
	}
	irq_rsc = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	xm_mb->irq = irq_rsc->start;
	if (xm_mb->irq < 0) {
		dev_err(dev, "no IRQ specified\n");
		ret = -EINVAL;
		goto err;
	} else {
		dev_info(dev, "using irq %d\n", xm_mb->irq);
	}
#else
	(void)irq_rsc;
	xm_mb->reg_base = (void *)res->start;
#endif
	xm_mb->irqf = irq_flag;
	xm_mb->high_perf_mask = HIGH_PERF_MASK;
	xm_mb->controller.dev = dev;
	xm_mb->controller.txdone_poll = true;
	xm_mb->controller.txpoll_period = 1;
	xm_mb->controller.ops = &xm_mailbox_ops;
	xm_mb->controller.num_chans = CHANNEL_NUM;
	xm_mb->controller.chans = devm_kcalloc(
		dev, CHANNEL_NUM, sizeof(*xm_mb->controller.chans), GFP_KERNEL);
	if (!xm_mb->controller.chans) {
		dev_err(dev, "failed to alloc chand mem\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < CHANNEL_NUM; i++)
		xm_mb->controller.chans[i].con_priv = (void *)i;

	if (xm_mb->type == XM_MB_SPI) {
		xm_mb->controller.ops = &xm_mailbox_spi_ops;
		dev_info(dev, "use mbox spi ops!!\n");
	}

	ret = devm_mbox_controller_register(dev, &xm_mb->controller);
	if (ret) {
		dev_err(dev, "register mbox err %d\n", ret);
		goto err;
	}

	mbox_debugfs = debugfs_create_dir("ispv4_mbox", ispv4_debugfs);
	if (!IS_ERR_OR_NULL(mbox_debugfs)) {
		debugfs_create_bool("mask_irq", 0666, mbox_debugfs, &mask_irq);
	}

	debugfs_create_file("ispv4_mbox_reg", 0666, mbox_debugfs,
			    xm_mb, &dbg_ispv4_pcir);
	platform_set_drvdata(pdev, xm_mb);
	return 0;

err:
alloc_mb_err:
	of_node_put(np);
	dev->of_node = NULL;

	return ret;
}

static int xm_ispv4_mailbox_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct xm_ispv4_mailbox *mb;

	if (!IS_ERR_OR_NULL(mbox_debugfs)) {
		debugfs_remove(mbox_debugfs);
	}

	mb = platform_get_drvdata(pdev);
	if (mb->type == XM_MB_SPI)
		cancel_work_sync(&mb->send_work);

	if (dev->of_node == NULL)
		return 0;
	of_node_put(dev->of_node);
	dev->of_node = NULL;
	return 0;
}

static struct platform_driver xm_ispv4_mailbox_driver = {
	.driver = {
		.name = "xm-ispv4-mbox",
	},
	.probe		= xm_ispv4_mailbox_probe,
	.remove		= xm_ispv4_mailbox_remove,
};

module_platform_driver(xm_ispv4_mailbox_driver);

MODULE_AUTHOR("Chenhonglin <chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4 mailbox driver");
MODULE_LICENSE("GPL v2");
