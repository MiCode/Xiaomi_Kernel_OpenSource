// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cputype.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/workqueue.h>
#include <linux/arm-smccc.h>
#include <mt-plat/aee.h>
#include "sda.h"

#define MCU_BP_IRQ_TRIGGER_THRESHOLD	(2)
#define INFRA_BP_IRQ_TRIGGER_THRESHOLD	(2)

union bus_parity_err {
	struct _mst {
		unsigned int parity_data;
		unsigned int rid;
		unsigned int rdata[4];
		bool is_err;
	} mst;
	struct _slv {
		unsigned int parity_data;
		unsigned int wid;
		unsigned int wdata[4];
		unsigned int arid;
		unsigned int araddr[2];
		unsigned int awid;
		unsigned int awaddr[2];
		bool is_err;
	} slv;
};

struct bus_parity_elem {
	const char *name;
	void __iomem *base;
	unsigned int type;
	unsigned int data_len;
	union bus_parity_err bpr;
};

struct bus_parity {
	struct bus_parity_elem *bpm;
	unsigned int nr_bpm;
	unsigned int nr_err;
	unsigned long long ts;
	struct work_struct wk;
	void __iomem *parity_sta;
	unsigned int irq;
	char *dump;
};

#define BPR_LOG(fmt, ...) \
	do { \
		pr_notice(fmt, __VA_ARGS__); \
		aee_sram_printk(fmt, __VA_ARGS__); \
	} while (0)

static struct bus_parity mcu_bp, infra_bp;
static DEFINE_SPINLOCK(mcu_bp_isr_lock);
static DEFINE_SPINLOCK(infra_bp_isr_lock);

static ssize_t bus_status_show(struct device_driver *driver, char *buf)
{
	int n;

	n = 0;

	if (mcu_bp.nr_err || infra_bp.nr_err) {
		n += snprintf(buf + n, PAGE_SIZE - n, "True\n");

		n += snprintf(buf + n, PAGE_SIZE - n,
			"MCU Bus Parity: %u times (1st timestamp: %llu ns)\n",
			mcu_bp.nr_err, mcu_bp.ts);

		n += snprintf(buf + n, PAGE_SIZE - n,
			"Infra Bus Parity: %u times (1st timestamp: %llu ns)\n",
			infra_bp.nr_err, infra_bp.ts);

		return n;
	} else
		return snprintf(buf, PAGE_SIZE, "False\n");
}

static DRIVER_ATTR_RO(bus_status);

static void mcu_bp_irq_work(struct work_struct *w)
{
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	int i;

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		bpm = &mcu_bp.bpm[i];
		bpr = &mcu_bp.bpm[i].bpr;

		if (!bpm->type && (bpr->mst.is_err == true))
			bpr->mst.is_err = false;
		else if (bpm->type && (bpr->slv.is_err == true))
			bpr->slv.is_err = false;
		else
			continue;
	}

	aee_kernel_exception("MCU Bus Parity", mcu_bp.dump);

	if (mcu_bp.nr_err < MCU_BP_IRQ_TRIGGER_THRESHOLD)
		enable_irq(mcu_bp.irq);
	else
		BPR_LOG("%s disable irq %d due to trigger over than %d times.\n",
			__func__, mcu_bp.irq, MCU_BP_IRQ_TRIGGER_THRESHOLD);
}

static void infra_bp_irq_work(struct work_struct *w)
{
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	int i;

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		bpm = &infra_bp.bpm[i];
		bpr = &infra_bp.bpm[i].bpr;

		if (!bpm->type && (bpr->mst.is_err == true))
			bpr->mst.is_err = false;
		else if (bpm->type && (bpr->slv.is_err == true))
			bpr->slv.is_err = false;
		else
			continue;
	}

	aee_kernel_exception("Infra Bus Parity", infra_bp.dump);

	if (infra_bp.nr_err < INFRA_BP_IRQ_TRIGGER_THRESHOLD)
		enable_irq(infra_bp.irq);
	else
		BPR_LOG("%s disable irq %d due to trigger over than %d times.\n",
			__func__, infra_bp.irq, INFRA_BP_IRQ_TRIGGER_THRESHOLD);
}

static void mcu_bp_dump(void)
{
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	int n, i, j;

	if (!mcu_bp.dump)
		return;

	n = 0;

	n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n, "mcu_bp err:\n");
	if (n > PAGE_SIZE)
		return;

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		bpm = &mcu_bp.bpm[i];
		bpr = &mcu_bp.bpm[i].bpr;

		if (!bpm->type && (bpr->mst.is_err == true)) {
			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
				"%s,pd:0x%x,rid:0x%x,rdata:0x%x",
				bpm->name, bpr->mst.parity_data,
				bpr->mst.rid, bpr->mst.rdata[0]);

			if (n > PAGE_SIZE)
				return;

			for (j = 1; j < bpm->data_len; j++) {
				n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
					"/0x%x", bpr->mst.rdata[j]);

				if (n > PAGE_SIZE)
					return;
			}

			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n, "\n");

			if (n > PAGE_SIZE)
				return;
		} else if (bpm->type && (bpr->slv.is_err == true)) {
			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
				"%s,pd:0x%x,awid:0x%x,awaddr:0x%x/0x%x,",
				bpm->name, bpr->slv.parity_data, bpr->slv.awid,
				bpr->slv.awaddr[0], bpr->slv.awaddr[1]);

			if (n > PAGE_SIZE)
				return;

			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
				"arid:0x%x,araddr:0x%x/0x%x,",
				bpr->slv.arid, bpr->slv.araddr[0],
				bpr->slv.araddr[1]);

			if (n > PAGE_SIZE)
				return;

			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
				"wid:0x%x,wdata:0x%x",
				bpr->slv.wid, bpr->slv.wdata[0]);

			if (n > PAGE_SIZE)
				return;

			for (j = 1; j < bpm->data_len; j++) {
				n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
					"/0x%x", bpr->slv.wdata[j]);

				if (n > PAGE_SIZE)
					return;
			}

			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n, "\n");

			if (n > PAGE_SIZE)
				return;
		} else
			continue;
	}
}

static void infra_bp_dump(void)
{
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	int n, i;

	if (!infra_bp.dump)
		return;

	n = 0;

	n += snprintf(infra_bp.dump + n, PAGE_SIZE - n, "infra_bp err:\n");
	if (n > PAGE_SIZE)
		return;

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		bpm = &infra_bp.bpm[i];
		bpr = &infra_bp.bpm[i].bpr;

		if (!bpm->type && (bpr->mst.is_err == true)) {
			n += snprintf(infra_bp.dump + n, PAGE_SIZE - n,
				"%s,pd:0x%x,rid:0x%x\n",
				bpm->name, bpr->mst.parity_data,
				bpr->mst.rid);

			if (n > PAGE_SIZE)
				return;
		} else if (bpm->type && (bpr->slv.is_err == true)) {
			n += snprintf(infra_bp.dump + n, PAGE_SIZE - n,
				"%s,pd:0x%x,awid:0x%x,awaddr:0x%x/0x%x,",
				bpm->name, bpr->slv.parity_data, bpr->slv.awid,
				bpr->slv.awaddr[0], bpr->slv.awaddr[1]);

			if (n > PAGE_SIZE)
				return;

			n += snprintf(infra_bp.dump + n, PAGE_SIZE - n,
				"arid:0x%x,araddr:0x%x/0x%x,wid:0x%x\n",
				bpr->slv.arid, bpr->slv.araddr[0],
				bpr->slv.araddr[1], bpr->slv.wid);

			if (n > PAGE_SIZE)
				return;
		} else
			continue;
	}
}

static irqreturn_t mcu_bp_isr(int irq, void *dev_id)
{
	int i, j;
	unsigned int status;
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	struct arm_smccc_res res;

	disable_irq_nosync(irq);

	if (!mcu_bp.nr_err)
		mcu_bp.ts = local_clock();
	mcu_bp.nr_err++;

	status = readl(mcu_bp.parity_sta);
	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		if (status & (0x1<<i)) {
			bpm = &mcu_bp.bpm[i];
			bpr = &mcu_bp.bpm[i].bpr;

			if (!bpm->type) {
				bpr->mst.is_err = true;
				bpr->mst.parity_data = readl(bpm->base+0x4);
				bpr->mst.rid = readl(bpm->base+0x8);
				for (j = 0; j < bpm->data_len; j++)
					bpr->mst.rdata[j] =
						readl(bpm->base+0x10+(j<<2));
			} else {
				bpr->slv.is_err = true;
				bpr->slv.parity_data = readl(bpm->base+0x4);
				bpr->slv.awid = readl(bpm->base+0x8);
				bpr->slv.arid = readl(bpm->base+0xC);
				bpr->slv.awaddr[0] = readl(bpm->base+0x10);
				bpr->slv.awaddr[1] = readl(bpm->base+0x14);
				bpr->slv.araddr[0] = readl(bpm->base+0x18);
				bpr->slv.araddr[1] = readl(bpm->base+0x1C);
				bpr->slv.wid = readl(bpm->base+0x20);
				if (bpm->data_len == 2) {
					bpr->slv.wdata[0] =
						readl(bpm->base+0x28);
					bpr->slv.wdata[1] =
						readl(bpm->base+0x2C);
				} else {
					bpr->slv.wdata[0] =
						readl(bpm->base+0x30);
					bpr->slv.wdata[1] =
						readl(bpm->base+0x34);
					bpr->slv.wdata[2] =
						readl(bpm->base+0x38);
					bpr->slv.wdata[3] =
						readl(bpm->base+0x3C);
				}
			}
		} else
			continue;
	}

	schedule_work(&mcu_bp.wk);

	spin_lock(&mcu_bp_isr_lock);

	arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_BUS_PARITY, BP_MCU_CLR, status,
			0, 0, 0, 0, &res);

	spin_unlock(&mcu_bp_isr_lock);

	mcu_bp_dump();
	BPR_LOG("%s", mcu_bp.dump);

	return IRQ_HANDLED;
}

static irqreturn_t infra_bp_isr(int irq, void *dev_id)
{
	int i;
	unsigned int status;
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;

	disable_irq_nosync(irq);

	if (!infra_bp.nr_err)
		infra_bp.ts = local_clock();
	infra_bp.nr_err++;

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		status = readl(infra_bp.bpm[i].base);
		if (status & (0x1<<31)) {
			bpm = &infra_bp.bpm[i];
			bpr = &infra_bp.bpm[i].bpr;

			if (!bpm->type) {
				bpr->mst.is_err = true;
				bpr->mst.parity_data = (status << 1) >> 16;
				bpr->mst.rid = readl(bpm->base+0x4);
			} else {
				bpr->slv.is_err = true;
				bpr->slv.parity_data = (status << 1) >> 10;
				bpr->slv.awaddr[0] = readl(bpm->base+0x4);
				status = readl(bpm->base+0x8);
				bpr->slv.awaddr[1] = (status << 27) >> 27;
				bpr->slv.awid = (status << 14) >> 19;
				bpr->slv.wid = (status << 1) >> 19;
				bpr->slv.araddr[0] = readl(bpm->base+0xC);
				status = readl(bpm->base+0x10);
				bpr->slv.araddr[1] = (status << 27) >> 27;
				bpr->slv.arid = (status << 14) >> 19;
			}
		} else
			continue;
	}

	schedule_work(&infra_bp.wk);

	spin_lock(&infra_bp_isr_lock);

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		status = readl(infra_bp.bpm[i].base);
		writel((status|(0x1<<2)), infra_bp.bpm[i].base);
		dsb(sy);
		writel(status, infra_bp.bpm[i].base);
		dsb(sy);
	}

	spin_unlock(&infra_bp_isr_lock);

	infra_bp_dump();
	BPR_LOG("%s", infra_bp.dump);

	return IRQ_HANDLED;
}

static int bus_parity_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	size_t size;
	int ret, i;

	dev_info(dev, "driver probed\n");

	mcu_bp.nr_err = 0;
	infra_bp.nr_err = 0;

	INIT_WORK(&mcu_bp.wk, mcu_bp_irq_work);
	INIT_WORK(&infra_bp.wk, infra_bp_irq_work);

	ret = of_property_count_strings(np, "mcu-names");
	if (ret < 0) {
		dev_notice(dev, "can't count mcu-names(%d)\n", ret);
		return ret;
	}
	mcu_bp.nr_bpm = ret;

	ret = of_property_count_strings(np, "infra-names");
	if (ret < 0) {
		dev_notice(dev, "can't count infra-names(%d)\n", ret);
		return ret;
	}
	infra_bp.nr_bpm = ret;

	dev_info(dev, "%s=%d, %s=%d\n", "nr_mcu_bpm", mcu_bp.nr_bpm,
			"nr_infra_bpm", infra_bp.nr_bpm);

	size = sizeof(struct bus_parity_elem) * mcu_bp.nr_bpm;
	mcu_bp.bpm = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!mcu_bp.bpm)
		return -ENOMEM;

	size = sizeof(struct bus_parity_elem) * infra_bp.nr_bpm;
	infra_bp.bpm = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!infra_bp.bpm)
		return -ENOMEM;

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		ret = of_property_read_string_index(np, "mcu-names", i,
				&mcu_bp.bpm[i].name);
		if (ret) {
			dev_notice(dev, "can't read mcu-names(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		ret = of_property_read_string_index(np, "infra-names", i,
				&infra_bp.bpm[i].name);
		if (ret) {
			dev_notice(dev, "can't read infra-names(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		mcu_bp.bpm[i].base = of_iomap(np, i);
		if (!mcu_bp.bpm[i].base) {
			dev_notice(dev, "can't map mcu_bp(%d)\n", i);
			return -ENXIO;
		}
	}

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		infra_bp.bpm[i].base = of_iomap(np, mcu_bp.nr_bpm + i);
		if (!infra_bp.bpm[i].base) {
			dev_notice(dev, "can't map infra_bp(%d)\n", i);
			return -ENXIO;
		}
	}

	mcu_bp.parity_sta = of_iomap(np, mcu_bp.nr_bpm + infra_bp.nr_bpm);
	if (!mcu_bp.parity_sta) {
		dev_notice(dev, "can't map mcu_bp status\n");
		return -ENXIO;
	}

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		ret = of_property_read_u32_index(np, "mcu-types", i,
				&mcu_bp.bpm[i].type);
		if (ret) {
			dev_notice(dev, "can't read mcu-types(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		ret = of_property_read_u32_index(np, "infra-types", i,
				&infra_bp.bpm[i].type);
		if (ret) {
			dev_notice(dev, "can't read infra-types(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		ret = of_property_read_u32_index(np, "mcu-data-len", i,
				&mcu_bp.bpm[i].data_len);
		if (ret) {
			dev_notice(dev, "can't read mcu-data-len(%d)\n", ret);
			return ret;
		}
	}

	mcu_bp.dump = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
	if (!mcu_bp.dump)
		return -ENOMEM;

	infra_bp.dump = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
	if (!infra_bp.dump)
		return -ENOMEM;

	mcu_bp.irq = irq_of_parse_and_map(np, 0);
	if (!mcu_bp.irq) {
		dev_notice(dev, "can't map mcu-bus-parity irq\n");
		return -EINVAL;
	}

	ret = devm_request_irq(dev, mcu_bp.irq, mcu_bp_isr, IRQF_ONESHOT |
			IRQF_TRIGGER_NONE, "mcu-bus-parity", NULL);
	if (ret) {
		dev_notice(dev, "can't request mcu-bus-parity irq(%d)\n", ret);
		return ret;
	}

	infra_bp.irq = irq_of_parse_and_map(np, 1);
	if (!infra_bp.irq) {
		dev_notice(dev, "can't map infra-bus-parity irq\n");
		return -EINVAL;
	}

	ret = devm_request_irq(dev, infra_bp.irq, infra_bp_isr, IRQF_ONESHOT |
			IRQF_TRIGGER_NONE,  "infra-bus-parity", NULL);
	if (ret) {
		dev_notice(dev, "can't request infra-bus-parity irq(%d)\n", ret);
		return ret;
	}
	return 0;
}

static int bus_parity_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");

	flush_work(&mcu_bp.wk);
	flush_work(&infra_bp.wk);

	return 0;
}

static const struct of_device_id bus_parity_of_ids[] = {
	{ .compatible = "mediatek,bus-parity", },
	{ .compatible = "mediatek,mt6885-bus-parity", },
	{ .compatible = "mediatek,mt6877-bus-parity", },
	{}
};

static struct platform_driver bus_parity_drv = {
	.driver = {
		.name = "bus_parity",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = bus_parity_of_ids,
	},
	.probe = bus_parity_probe,
	.remove = bus_parity_remove,
};

static int __init bus_parity_init(void)
{
	int ret;

	ret = platform_driver_register(&bus_parity_drv);
	if (ret)
		return ret;

	ret = driver_create_file(&bus_parity_drv.driver,
				 &driver_attr_bus_status);
	if (ret)
		return ret;

	return 0;
}

static __exit void bus_parity_exit(void)
{
	driver_remove_file(&bus_parity_drv.driver,
			 &driver_attr_bus_status);

	platform_driver_unregister(&bus_parity_drv);
}

module_init(bus_parity_init);
module_exit(bus_parity_exit);

MODULE_DESCRIPTION("MediaTek Bus Parity Driver");
MODULE_LICENSE("GPL v2");
