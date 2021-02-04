/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/bootmem.h>
#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/list.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/barrier.h>
#include <soc/mediatek/smi.h>
#include <linux/dma-debug.h>
#ifndef CONFIG_ARM64
#include <asm/dma-iommu.h>
#endif

#include "io-pgtable.h"
#include "mtk_iommu.h"

#include "mach/mt_iommu.h"
#include "mach/pseudo_m4u.h"

#include "mtk_iommu_ext.h"

#define PREALLOC_DMA_DEBUG_ENTRIES 4096

#define TO_BE_IMPL

#define F_VAL(val, msb, lsb)	(((val)&((1<<(msb-lsb+1))-1))<<lsb)
#define F_VAL_L(val, msb, lsb) (((val)&((1L<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)	F_VAL(0xffffffff, msb, lsb)
#define F_MSK_L(msb, lsb)	F_VAL_L(0xffffffffffffffff, msb, lsb)
#define F_BIT_SET(bit)		(1<<(bit))
#define F_BIT_VAL(val, bit)	((!!(val))<<(bit))
#define F_MSK_SHIFT(regval, msb, lsb) (((regval)&F_MSK(msb, lsb))>>lsb)

#define REG_MMU_PT_BASE_ADDR			0x000

#define REG_MMU_INVALIDATE			0x020
#define F_ALL_INVLD				0x2
#define F_MMU_INV_RANGE			0x1

#define REG_MMU_INVLD_START_A			0x024
#define REG_MMU_INVLD_END_A			0x028

#define REG_MMU_INV_SEL			0x038
#define F_INVLD_EN0				BIT(0)
#define F_INVLD_EN1				BIT(1)

#define REG_MMU_DUMMY				0x44
#define F_REG_MMU_IDLE_ENABLE			F_BIT_SET(0)

#define REG_MMU_STANDARD_AXI_MODE		0x048
#define REG_MMU_DCM_DIS			0x050

#define REG_MMU_WR_LEN				0x54
#define F_MMU_WR_THROT_DIS(sel)		F_VAL(sel, 11, 10)
#define F_MMU_MMU1_WRITE_LEN			F_MSK(9, 5)
#define F_MMU_MMU0_WRITE_LEN			F_MSK(4, 0)

#define REG_MMU_MMU_COHERENCE_EN		0x80
#define REG_MMU_IN_ORDER_WR_EN		0x84
#define REG_MMU_MMU_TABLE_WALK_DIS		0x88
#define REG_MMU_MMU_MTLB_RESERVE_MODE_DIS	0x8c

#define REG_MMU_CTRL_REG			0x110
#define F_MMU_CTRL_HIT_AT_PFQ_EN(en)		F_BIT_VAL(en, 11)
#define F_MMU_CTRL_HIT_AT_PFQ_L2_EN(en)	F_BIT_VAL(en, 10)
#define F_MMU_CTRL_HANG_PREVENTION(en)	F_BIT_VAL(en,  9)
#define F_MMU_CTRL_INVALID_FIFO_EN(en)	F_BIT_VAL(en,  8)
#define F_MMU_CTRL_LAYER2_PFH_DIS(dis)	F_BIT_VAL(dis, 7)
#define F_MMU_CTRL_INT_HANG_EN(en)		F_BIT_VAL(en,  6)
#define F_MMU_CTRL_TF_PROTECT_SEL(en)	F_VAL(en, 5, 4)
#define F_MMU_CTRL_MONITOR_CLR(clr)		F_BIT_VAL(clr, 2)
#define F_MMU_CTRL_MONITOR_EN(en)		F_BIT_VAL(en,  1)
#define F_MMU_CTRL_PFH_DIS(dis)		F_BIT_VAL(dis, 0)

#define REG_MMU_IVRP_PADDR			0x114
#define F_MMU_IVRP_PA_SET(PA, EXT)  \
	((((unsigned long long)PA) & F_MSK(31, 7)) | \
	 ((((unsigned long long)PA) >> 32) & F_MSK(1, 0)))

#define REG_MMU_INT_CONTROL0			0x120
#define F_L2_MULIT_HIT_EN			BIT(0)
#define F_TABLE_WALK_FAULT_INT_EN		BIT(1)
#define F_PREETCH_FIFO_OVERFLOW_INT_EN	BIT(2)
#define F_MISS_FIFO_OVERFLOW_INT_EN		BIT(3)
#define F_L2_INVALIDATION_DONE_INT_EN	BIT(4)
#define F_PREFETCH_FIFO_ERR_INT_EN		BIT(5)
#define F_MISS_FIFO_ERR_INT_EN		BIT(6)
#define F_INT_CLR_BIT				BIT(12)

#define REG_MMU_INT_MAIN_CONTROL		0x124
#define F_INT_TRANSLATION_FAULT		BIT(0)
#define F_INT_MAIN_MULTI_HIT_FAULT		BIT(1)
#define F_INT_INVALID_PA_FAULT		BIT(2)
#define F_INT_ENTRY_REPLACEMENT_FAULT	BIT(3)
#define F_INT_TLB_MISS_FAULT			BIT(4)
#define F_INT_MISS_TRANSACTION_FIFO_FAULT	BIT(5)
#define F_INT_PRETETCH_TRANSATION_FIFO_FAULT	BIT(6)

#define F_INT_MMU0_MAIN_MSK			F_MSK(6, 0)
#define F_INT_MMU1_MAIN_MSK			F_MSK(13, 7)
#define F_INT_MMU0_MAU_MSK			F_MSK(17, 14)
#define F_INT_MMU1_MAU_MSK			F_MSK(21, 18)

#define REG_MMU_CPE_DONE			0x12C

#define REG_MMU_L2_FAULT_ST			0x130
#define REG_MMU_FAULT_ST1			0x134

#define REG_MMU_FAULT_VA(mmu)			(0x13c+((mmu)<<3))
#define F_MMU_FAULT_VA_MSK			F_MSK(31, 12)
#define F_MMU_FAULT_VA_WRITE_BIT		F_BIT_SET(1)
#define F_MMU_FAULT_VA_LAYER_BIT		F_BIT_SET(0)

#define REG_MMU_INVLD_PA(mmu)			(0x140+((mmu)<<3))
#define REG_MMU_INT_ID(mmu)			(0x150+((mmu)<<2))
#define F_MMU0_INT_ID_TF_MSK			(~0x3)	/* only for MM iommu.*/

/* IO virtual address start page frame number */
#define IOVA_START_PFN		(1)
#define IOVA_PFN(addr)		((addr) >> PAGE_SHIFT)
#define DMA_32BIT_PFN		IOVA_PFN(DMA_BIT_MASK(32))

inline void m4uHw_set_field_by_mask(void __iomem *M4UBase, unsigned int reg,
					   unsigned long mask, unsigned int val)
{
	unsigned int regval;

	regval = readl_relaxed(M4UBase + reg);
	regval = (regval & (~mask)) | val;
	writel_relaxed(regval, M4UBase + reg);
}



/* bit[9:7] indicate larbid */
#define F_MMU0_INT_ID_LARB_ID(a)		(((a) >> 7) & 0x7)
/*
 * bit[6:2] indicate portid, bit[1:0] indicate master id, every port
 * have four types of command, master id indicate the m4u port's command
 * type, iommu do not care about this.
 */
#define F_MMU0_INT_ID_PORT_ID(a)		(((a) >> 2) & 0x1f)

#define MTK_PROTECT_PA_ALIGN			128

struct mtk_iommu_domain {
	spinlock_t			pgtlock; /* lock for page table */

	struct io_pgtable_cfg		cfg;
	struct io_pgtable_ops		*iop;

	struct iommu_domain		domain;
};

static struct iommu_ops mtk_iommu_ops;
static const struct of_device_id mtk_iommu_of_ids[];

static LIST_HEAD(m4ulist);

static struct mtk_iommu_domain *to_mtk_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct mtk_iommu_domain, domain);
}

static void mtk_iommu_tlb_flush_all(void *cookie)
{
	struct mtk_iommu_data *data, *temp;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + REG_MMU_INV_SEL);
		writel_relaxed(F_ALL_INVLD, data->base + REG_MMU_INVALIDATE);
		wmb(); /* Make sure the tlb flush all done */
	}
}

static void mtk_iommu_tlb_add_flush_nosync(unsigned long iova,
					   size_t size,
					   size_t granule, bool leaf,
					   void *cookie)
{
	struct mtk_iommu_data *data, *temp;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {

		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + REG_MMU_INV_SEL);

		writel_relaxed(iova, data->base + REG_MMU_INVLD_START_A);
		writel_relaxed(iova + size - 1,
			       data->base + REG_MMU_INVLD_END_A);
		writel_relaxed(F_MMU_INV_RANGE,
			       data->base + REG_MMU_INVALIDATE);
	}
}

static void mtk_iommu_tlb_sync(void *cookie)
{
	struct mtk_iommu_data *data, *temp;
	int ret;
	u32 tmp;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		ret = readl_poll_timeout_atomic(data->base + REG_MMU_CPE_DONE,
						tmp, tmp != 0, 10, 100000);
		if (ret) {
			dev_warn(data->dev, "Partial TLB flush timed out, falling back to full flush\n");
		mtk_iommu_tlb_flush_all(cookie);
	}
		/* Clear the CPE status */
		writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
	}
}

static const struct iommu_gather_ops mtk_iommu_gather_ops = {
	.tlb_flush_all = mtk_iommu_tlb_flush_all,
	.tlb_add_flush = mtk_iommu_tlb_add_flush_nosync,
	.tlb_sync = mtk_iommu_tlb_sync,
};

static struct timer_list iommu_isr_pause_timer;

static inline void mtk_iommu_intr_modify_all(unsigned long enable)
{
	struct mtk_iommu_data *data, *temp;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		if (enable) {
			writel_relaxed(0x6f,
				       data->base + REG_MMU_INT_CONTROL0);
			writel_relaxed(0xffffffff,
				       data->base + REG_MMU_INT_MAIN_CONTROL);
		} else {
			writel_relaxed(0,
				       data->base + REG_MMU_INT_CONTROL0);
			writel_relaxed(0,
				       data->base + REG_MMU_INT_MAIN_CONTROL);
		}
	}
}

static void mtk_iommu_isr_restart(unsigned long unused)
{
	mtk_iommu_intr_modify_all(1);
	mtk_iommu_debug_reset();
}

static int mtk_iommu_isr_pause_timer_init(void)
{
	init_timer(&iommu_isr_pause_timer);
	iommu_isr_pause_timer.function = mtk_iommu_isr_restart;
	return 0;
}

static int mtk_iommu_isr_pause(int delay)
{
	mtk_iommu_intr_modify_all(0); /* disable all intr */
	/* delay seconds */
	iommu_isr_pause_timer.expires = jiffies + delay * HZ;
	add_timer(&iommu_isr_pause_timer);
	return 0;
}

static void mtk_iommu_isr_record(void)
{
	static int isr_cnt;
	static unsigned long first_jiffies;

	/* we allow one irq in 1s, or we will disable them after 5s. */
	if (!isr_cnt || time_after(jiffies, first_jiffies + isr_cnt * HZ)) {
		isr_cnt = 1;
		first_jiffies = jiffies;
	} else {
		isr_cnt++;
		if (isr_cnt >= 5) {
			/* 5 irqs come in 5s, too many ! */
			/* disable irq for a while, to avoid HWT timeout */
			mtk_iommu_isr_pause(10);
			isr_cnt = 0;
		}
	}
}

static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = dev_id;
	struct mtk_iommu_domain *dom = data->m4u_dom;
	u32 int_state, regval, fault_iova, fault_pa;
	unsigned int fault_larb, fault_port;
	bool layer, write;
	int slave_id = 0;

	/* Read error info from registers */
	regval = readl_relaxed(data->base + REG_MMU_L2_FAULT_ST);
	int_state = readl_relaxed(data->base + REG_MMU_FAULT_ST1);

	if (int_state & (F_INT_MMU0_MAIN_MSK | F_INT_MMU0_MAU_MSK))
		slave_id = 0;
	else if (int_state & (F_INT_MMU1_MAIN_MSK | F_INT_MMU1_MAU_MSK))
		slave_id = 1;
	else {
		pr_info("m4u interrupt error: status = 0x%x\n", int_state);
		m4uHw_set_field_by_mask(data->base, REG_MMU_INT_CONTROL0,
				       F_INT_CLR_BIT, F_INT_CLR_BIT);
		return 0;
	}

	pr_info("iommu L2 int sta=0x%x, main sta=0x%x\n", regval, int_state);
	fault_iova = readl_relaxed(data->base + REG_MMU_FAULT_VA(slave_id));
	layer = fault_iova & F_MMU_FAULT_VA_LAYER_BIT;
	write = fault_iova & F_MMU_FAULT_VA_WRITE_BIT;
	fault_iova &= F_MMU_FAULT_VA_MSK;
	fault_pa = readl_relaxed(data->base + REG_MMU_INVLD_PA(slave_id));
	regval = readl_relaxed(data->base + REG_MMU_INT_ID(slave_id));
	fault_larb = F_MMU0_INT_ID_LARB_ID(regval);
	fault_port = F_MMU0_INT_ID_PORT_ID(regval);

	if (enable_custom_tf_report()) {
		report_custom_iommu_fault(data->base,
					  int_state,
					  fault_iova,
					  fault_pa,
					  regval);
	}

	if (report_iommu_fault(&dom->domain, data->dev, fault_iova,
			      write ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ)) {
		dev_err_ratelimited(
			data->dev,
			"iommu fault type=0x%x iova=0x%x pa=0x%x larb=%d port=%d layer=%d %s\n",
			int_state, fault_iova, fault_pa, fault_larb, fault_port,
			layer, write ? "write" : "read");
	}

	/* Interrupt clear */
	regval = readl_relaxed(data->base + REG_MMU_INT_CONTROL0);
	regval |= F_INT_CLR_BIT;
	writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);

	mtk_iommu_tlb_flush_all(data);
	mtk_iommu_isr_record();

	return IRQ_HANDLED;
}

static void mtk_iommu_config(struct mtk_iommu_data *data,
			     struct device *dev, bool enable)
{
	struct mtk_iommu_client_priv *head, *cur, *next;
	struct mtk_smi_larb_iommu    *larb_mmu;
	unsigned int                 larbid, portid;

	head = dev->archdata.iommu;
	list_for_each_entry_safe(cur, next, &head->client, client) {
		larbid = MTK_M4U_TO_LARB(cur->mtk_m4u_id);
		portid = MTK_M4U_TO_PORT(cur->mtk_m4u_id);
		larb_mmu = &data->smi_imu.larb_imu[larbid];

		dev_dbg(dev, "%s iommu port: %d\n",
			enable ? "enable" : "disable", portid);

		if (enable)
			larb_mmu->mmu |= MTK_SMI_MMU_EN(portid);
		else
			larb_mmu->mmu &= ~MTK_SMI_MMU_EN(portid);
	}
}

static int mtk_iommu_domain_finalise(struct mtk_iommu_data *data)
{
	struct mtk_iommu_domain *dom = data->m4u_dom;

	spin_lock_init(&dom->pgtlock);

	dom->cfg = (struct io_pgtable_cfg) {
		.quirks = IO_PGTABLE_QUIRK_ARM_NS |
			IO_PGTABLE_QUIRK_NO_PERMS |
			IO_PGTABLE_QUIRK_TLBI_ON_MAP,
		.pgsize_bitmap = mtk_iommu_ops.pgsize_bitmap,
		.ias = 32,
		.oas = 32,
		.tlb = &mtk_iommu_gather_ops,
		.iommu_dev = data->dev,
	};

	if (data->enable_4GB)
		dom->cfg.quirks |= IO_PGTABLE_QUIRK_ARM_MTK_4GB;

	dom->iop = alloc_io_pgtable_ops(ARM_V7S, &dom->cfg, data);
	if (!dom->iop) {
		dev_err(data->dev, "Failed to alloc io pgtable\n");
		return -EINVAL;
	}

	/* Update our support page sizes bitmap */
	dom->domain.pgsize_bitmap = dom->cfg.pgsize_bitmap;

	writel(data->m4u_dom->cfg.arm_v7s_cfg.ttbr[0],
	       data->base + REG_MMU_PT_BASE_ADDR);
	return 0;
}

#ifdef CONFIG_ARM64
static struct iommu_domain *mtk_iommu_domain_alloc(unsigned int type)
{
	struct mtk_iommu_domain *dom;

	if (type != IOMMU_DOMAIN_DMA)
		return NULL;

	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	if (iommu_get_dma_cookie(&dom->domain)) {
		kfree(dom);
		return NULL;
	}

	dom->domain.geometry.aperture_start = 0;
	dom->domain.geometry.aperture_end = DMA_BIT_MASK(32);
	dom->domain.geometry.force_aperture = true;

	return &dom->domain;
}
#else
static struct iommu_domain *mtk_iommu_domain_alloc(unsigned int type)
{
	struct mtk_iommu_domain *dom;

	if (type != IOMMU_DOMAIN_UNMANAGED)
		return NULL;

	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	return &dom->domain;
}
#endif

static void mtk_iommu_domain_free(struct iommu_domain *domain)
{
#ifdef CONFIG_ARM64
	iommu_put_dma_cookie(domain);
#endif
	kfree(to_mtk_domain(domain));
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_client_priv *priv = dev->archdata.iommu;
	struct mtk_iommu_data *data;
	int ret;

	if (!priv)
		return -ENODEV;

	data = dev_get_drvdata(priv->m4udev);
	if (!data->m4u_dom) {
		data->m4u_dom = dom;
		ret = mtk_iommu_domain_finalise(data);
		if (ret) {
			data->m4u_dom = NULL;
			return ret;
		}
	} else if (data->m4u_dom != dom) {
		/* All the client devices should be in the same m4u domain */
		dev_err(dev, "try to attach into the error iommu domain\n");
		return -EPERM;
	}

	mtk_iommu_config(data, dev, true);
	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct mtk_iommu_client_priv *priv = dev->archdata.iommu;
	struct mtk_iommu_data *data;

	if (!priv)
		return;

	data = dev_get_drvdata(priv->m4udev);
	mtk_iommu_config(data, dev, false);
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&dom->pgtlock, flags);
	ret = dom->iop->map(dom->iop, iova, paddr, size, prot);
	spin_unlock_irqrestore(&dom->pgtlock, flags);

	return ret;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	unsigned long flags;
	size_t unmapsz;

	spin_lock_irqsave(&dom->pgtlock, flags);
	unmapsz = dom->iop->unmap(dom->iop, iova, size);
	spin_unlock_irqrestore(&dom->pgtlock, flags);

	return unmapsz;
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	unsigned long flags;
	phys_addr_t pa;

	spin_lock_irqsave(&dom->pgtlock, flags);
	pa = dom->iop->iova_to_phys(dom->iop, iova);
	spin_unlock_irqrestore(&dom->pgtlock, flags);

	return pa;
}

#ifdef CONFIG_ARM64
static int mtk_iommu_add_device(struct device *dev)
{
	struct iommu_group *group;

	if (!dev->archdata.iommu) /* Not a iommu client device */
		return -ENODEV;

	group = iommu_group_get_for_dev(dev);
	if (IS_ERR(group))
		return PTR_ERR(group);

	iommu_group_put(group);
	return 0;
}
#else
/*
 * MTK generation one iommu HW only support one iommu domain, and all the client
 * sharing the same iova address space.
 */
static int mtk_iommu_create_mapping(struct device *dev,
				    struct of_phandle_args *args)
{
	struct mtk_iommu_client_priv *head, *priv, *next;
	struct platform_device *m4updev;
	struct dma_iommu_mapping *mtk_mapping;
	struct device *m4udev;
	int ret;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!dev->archdata.iommu) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		if (WARN_ON(!m4updev))
			return -EINVAL;

		head = kzalloc(sizeof(*head), GFP_KERNEL);
		if (!head)
			return -ENOMEM;

		dev->archdata.iommu = head;
		INIT_LIST_HEAD(&head->client);
		head->m4udev = &m4updev->dev;
	} else {
		head = dev->archdata.iommu;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_free_mem;
	}
	priv->mtk_m4u_id = args->args[0];
	list_add_tail(&priv->client, &head->client);

	m4udev = head->m4udev;
	mtk_mapping = m4udev->archdata.iommu;
	if (!mtk_mapping) {
		/* MTK iommu support 4GB iova address space. */
		mtk_mapping = arm_iommu_create_mapping(&platform_bus_type,
						0, 1ULL << 32);
		if (IS_ERR(mtk_mapping)) {
			ret = PTR_ERR(mtk_mapping);
			goto err_free_mem;
		}
		m4udev->archdata.iommu = mtk_mapping;
	}

	ret = arm_iommu_attach_device(dev, mtk_mapping);
	if (ret)
		goto err_release_mapping;

	return 0;

err_release_mapping:
	arm_iommu_release_mapping(mtk_mapping);
	m4udev->archdata.iommu = NULL;
err_free_mem:
	list_for_each_entry_safe(priv, next, &head->client, client)
		kfree(priv);
	kfree(head);
	dev->archdata.iommu = NULL;
	return ret;
}

static int mtk_iommu_add_device(struct device *dev)
{
	struct iommu_group *group;
	struct of_phandle_args iommu_spec;
	int idx = 0;

	while (!of_parse_phandle_with_args(dev->of_node, "iommus",
					   "#iommu-cells", idx,
					   &iommu_spec)) {
		mtk_iommu_create_mapping(dev, &iommu_spec);
		of_node_put(iommu_spec.np);
		idx++;
	}

	if (!idx)
		return -ENODEV;

	if (!dev->archdata.iommu) /* Not a iommu client device */
		return -ENODEV;

	group = iommu_group_get_for_dev(dev);
	if (IS_ERR(group))
		return PTR_ERR(group);

	iommu_group_put(group);

	return 0;
}


#endif
static void mtk_iommu_remove_device(struct device *dev)
{
	struct mtk_iommu_client_priv *head, *cur, *next;

	head = dev->archdata.iommu;
	if (!head)
		return;

	list_for_each_entry_safe(cur, next, &head->client, client) {
		list_del(&cur->client);
		kfree(cur);
	}
	kfree(head);
	dev->archdata.iommu = NULL;

	iommu_group_remove_device(dev);
}

static struct iommu_group *mtk_iommu_device_group(struct device *dev)
{
	struct mtk_iommu_data *data;
	struct mtk_iommu_client_priv *priv;

	priv = dev->archdata.iommu;
	if (!priv)
		return ERR_PTR(-ENODEV);

	/* All the client devices are in the same m4u iommu-group */
	data = dev_get_drvdata(priv->m4udev);

	/*
	 * We may run into a scenario that of_xlate have already set
	 * dev->archdata.iommu, but iommu driver has not been finished
	 * probe, at this time data is NULL, we will trigger
	 * bus scan all the device after all iommu's probe have been done.
	 * Just make the first enter is safe and will not crash the system.
	 * After all the iommu driver has finished probe, bus_set_iommu
	 * will trigger all the not-been added device
	 * add to iommu domain again.
	 */
	if (!data)
		return ERR_PTR(-ENODEV);

	if (!data->m4u_group) {
		data->m4u_group = iommu_group_alloc();
		if (IS_ERR(data->m4u_group))
			dev_err(dev, "Failed to allocate M4U IOMMU group\n");
	}
	return data->m4u_group;
}

#ifdef CONFIG_ARM64
static int mtk_iommu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct mtk_iommu_client_priv *head, *priv, *next;
	struct platform_device *m4updev;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!dev->archdata.iommu) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		of_node_put(args->np);
		if (WARN_ON(!m4updev))
			return -EINVAL;

		head = kzalloc(sizeof(*head), GFP_KERNEL);
		if (!head)
			return -ENOMEM;

		dev->archdata.iommu = head;
		INIT_LIST_HEAD(&head->client);
		head->m4udev = &m4updev->dev;
	} else {
		head = dev->archdata.iommu;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		goto err_free_mem;

	priv->mtk_m4u_id = args->args[0];
	list_add_tail(&priv->client, &head->client);

	return 0;

err_free_mem:
	list_for_each_entry_safe(priv, next, &head->client, client)
		kfree(priv);
	kfree(head);
	dev->archdata.iommu = NULL;
	return -ENOMEM;
}
#endif


#ifndef CONFIG_MTK_MEMCFG
#define CONFIG_MTK_MEMCFG
#endif

#ifdef CONFIG_MTK_MEMCFG
bool dm_region;
static void mtk_iommu_get_dm_region(struct device *dev, struct list_head *list)
{
	struct iommu_dm_region *region;

	/* for framebuffer region */
	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return;

	INIT_LIST_HEAD(&region->list);
	region->start = mtkfb_get_fb_base();
	region->length = mtkfb_get_fb_size();
	region->prot = IOMMU_READ | IOMMU_WRITE;
	list_add_tail(&region->list, list);

	dm_region = true;
}

static void mtk_iommu_put_dm_region(struct device *dev, struct list_head *list)
{
	struct  iommu_dm_region *region, *tmp;

	list_for_each_entry_safe(region, tmp, list, list)
		kfree(region);
}
#else
static void mtk_iommu_get_dm_region(struct device *dev, struct list_head *list)
{
	struct iommu_dm_region *region;

	/* for framebuffer region */
	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return;

	INIT_LIST_HEAD(&region->list);
	list_add_tail(&region->list, list);
}

static void mtk_iommu_put_dm_region(struct device *dev, struct list_head *list)
{
	struct  iommu_dm_region *region, *tmp;

	list_for_each_entry_safe(region, tmp, list, list)
		kfree(region);
}
#endif

static struct iommu_ops mtk_iommu_ops = {
	.domain_alloc	= mtk_iommu_domain_alloc,
	.domain_free	= mtk_iommu_domain_free,
	.attach_dev	= mtk_iommu_attach_device,
	.detach_dev	= mtk_iommu_detach_device,
	.map		= mtk_iommu_map,
	.unmap		= mtk_iommu_unmap,
	.map_sg		= default_iommu_map_sg,
	.iova_to_phys	= mtk_iommu_iova_to_phys,
	.add_device	= mtk_iommu_add_device,
	.remove_device	= mtk_iommu_remove_device,
	.device_group	= mtk_iommu_device_group,
#ifdef CONFIG_ARM64
	.of_xlate	= mtk_iommu_of_xlate,
#endif
	.pgsize_bitmap	= SZ_4K | SZ_64K | SZ_1M | SZ_16M,
	.get_dm_regions	= mtk_iommu_get_dm_region,
	.put_dm_regions	= mtk_iommu_put_dm_region,
};

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data)
{
	u32 regval;

	regval = readl_relaxed(data->base + REG_MMU_CTRL_REG);
	regval = regval | F_MMU_CTRL_PFH_DIS(0)
					 | F_MMU_CTRL_MONITOR_EN(1)
					 | F_MMU_CTRL_MONITOR_CLR(0)
					 | F_MMU_CTRL_TF_PROTECT_SEL(2)
					 | F_MMU_CTRL_INT_HANG_EN(0);

	writel_relaxed(regval, data->base + REG_MMU_CTRL_REG);

	writel_relaxed(0x3, data->base + REG_MMU_MMU_COHERENCE_EN);
	writel_relaxed(0, data->base + REG_MMU_MMU_TABLE_WALK_DIS);

	writel_relaxed(0x6f, data->base + REG_MMU_INT_CONTROL0);
	writel_relaxed(0xffffffff, data->base + REG_MMU_INT_MAIN_CONTROL);

	writel_relaxed(F_MMU_IVRP_PA_SET(data->protect_base, data->enable_4GB),
		       data->base + REG_MMU_IVRP_PADDR);

	writel_relaxed(0, data->base + REG_MMU_DCM_DIS);

#ifdef CONFIG_MTK_SMI_EXT
	writel_relaxed(0, data->base + REG_MMU_IN_ORDER_WR_EN);
#endif

	writel_relaxed(0, data->base + REG_MMU_STANDARD_AXI_MODE);

	m4uHw_set_field_by_mask(data->base, REG_MMU_WR_LEN,
				F_MMU_WR_THROT_DIS(3), F_MMU_WR_THROT_DIS(0));
	m4uHw_set_field_by_mask(data->base,
				REG_MMU_DUMMY, F_REG_MMU_IDLE_ENABLE, 0);

	if (devm_request_irq(data->dev, data->irq, mtk_iommu_isr, 0,
			     dev_name(data->dev), (void *)data)) {
		writel_relaxed(0, data->base + REG_MMU_PT_BASE_ADDR);
		dev_err(data->dev, "Failed @ IRQ-%d Request\n", data->irq);
		return -ENODEV;
	}

	return 0;
}

static const struct component_master_ops mtk_iommu_com_ops = {
	.bind		= mtk_iommu_bind,
	.unbind		= mtk_iommu_unbind,
};

static int mtk_iommu_probe(struct platform_device *pdev)
{
	const struct of_device_id        *of_id;
	struct mtk_iommu_data   *data;
	struct device           *dev = &pdev->dev;
	struct resource         *res;
	struct component_match  *match = NULL;
	struct of_phandle_args		larb_spec;
	struct of_phandle_iterator	it;
	void                    *protect;
	int                     larb_nr, ret, err;
	static int iommu_cnt;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = dev;

	of_id = of_match_node(mtk_iommu_of_ids, dev->of_node);
	data->match_data = (const struct mtk_iommu_match_data *)of_id->data;

	/* Protect memory. HW will access here while translation fault.*/
	protect = devm_kzalloc(dev, MTK_PROTECT_PA_ALIGN * 2, GFP_KERNEL);
	if (!protect)
		return -ENOMEM;
	data->protect_base = ALIGN(virt_to_phys(protect),
				   MTK_PROTECT_PA_ALIGN);

	/* Whether the current dram is over 4GB */
	data->enable_4GB = !!(max_pfn > (0xffffffffUL >> PAGE_SHIFT));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base)) {
		pr_info("mtk_iommu base is null\n");
		return PTR_ERR(data->base);
	}

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		pr_info("mtk_iommu irq error\n");
		return data->irq;
	}

	larb_nr = 0;
	of_for_each_phandle(&it, err, dev->of_node,
			"mediatek,larbs", NULL, 0) {
		struct platform_device *plarbdev;
		int count = of_phandle_iterator_args(&it, larb_spec.args,
					MAX_PHANDLE_ARGS);

		if (count)
			continue;

		larb_spec.np = of_node_get(it.node);
		if (!of_device_is_available(larb_spec.np))
			continue;

		plarbdev = of_find_device_by_node(larb_spec.np);
		of_node_put(larb_spec.np);
		if (!plarbdev) {
			plarbdev = of_platform_device_create(
						larb_spec.np, NULL,
						platform_bus_type.dev_root);
			if (!plarbdev) {
				pr_err("mtk_iommu device_create fail\n");
				return -EPROBE_DEFER;
			}
		}

		data->smi_imu.larb_imu[larb_nr].dev = &plarbdev->dev;
		component_match_add(dev, &match, compare_of, larb_spec.np);
		larb_nr++;
	}
	data->smi_imu.larb_nr = larb_nr;

	platform_set_drvdata(pdev, data);

	ret = mtk_iommu_hw_init(data);
	if (ret)
		return ret;

	list_add_tail(&data->list, &m4ulist);

	iommu_cnt++;
	/*
	 * trigger the bus to scan all the device to add them to iommu
	 * domain after all the iommu have finished probe.
	 */
	if (!iommu_present(&platform_bus_type) &&
	    iommu_cnt == data->match_data->iommu_cnt)
		bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);

	mtk_iommu_isr_pause_timer_init();
	mtk_iommu_debug_init();
	return component_master_add_with_match(dev, &mtk_iommu_com_ops, match);
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct mtk_iommu_data *data = platform_get_drvdata(pdev);

	if (iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, NULL);

	free_io_pgtable_ops(data->m4u_dom->iop);
	devm_free_irq(&pdev->dev, data->irq, data);
	component_master_del(&pdev->dev, &mtk_iommu_com_ops);
	return 0;
}

static int mtk_iommu_suspend(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;

	reg->standard_axi_mode = readl_relaxed(base +
					       REG_MMU_STANDARD_AXI_MODE);
	reg->dcm_dis = readl_relaxed(base + REG_MMU_DCM_DIS);
	reg->ctrl_reg = readl_relaxed(base + REG_MMU_CTRL_REG);
	reg->int_control0 = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	reg->int_main_control = readl_relaxed(base + REG_MMU_INT_MAIN_CONTROL);
	return 0;
}

static int mtk_iommu_resume(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;

	data->m4u_dom->cfg.arm_v7s_cfg.ttbr[0] &= ~(BIT(0) | BIT(1));
	writel_relaxed(data->m4u_dom->cfg.arm_v7s_cfg.ttbr[0],
		       base + REG_MMU_PT_BASE_ADDR);
	writel_relaxed(reg->standard_axi_mode,
		       base + REG_MMU_STANDARD_AXI_MODE);
	writel_relaxed(reg->dcm_dis, base + REG_MMU_DCM_DIS);
	writel_relaxed(reg->ctrl_reg, base + REG_MMU_CTRL_REG);
	writel_relaxed(reg->int_control0, base + REG_MMU_INT_CONTROL0);
	writel_relaxed(reg->int_main_control, base + REG_MMU_INT_MAIN_CONTROL);
	writel_relaxed(F_MMU_IVRP_PA_SET(data->protect_base, data->enable_4GB),
		       base + REG_MMU_IVRP_PADDR);
	return 0;
}

static const struct dev_pm_ops mtk_iommu_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_iommu_suspend, mtk_iommu_resume)
};


const struct mtk_iommu_match_data mt6xxx_v0_data = {
	.match_type = iommu_mt6xxx_v0,
	.iommu_cnt = 1,
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,iommu_v0", .data = (void *)&mt6xxx_v0_data},
	{}
};

static struct platform_driver mtk_iommu_driver = {
	.probe	= mtk_iommu_probe,
	.remove	= mtk_iommu_remove,
	.driver	= {
		.name = "mtk-iommu",
		.of_match_table = of_match_ptr(mtk_iommu_of_ids),
		.pm = &mtk_iommu_pm_ops,
	}
};

#ifdef CONFIG_ARM64
static int mtk_iommu_init_fn(struct device_node *np)
{
	static bool init_done;
	int ret;
	struct platform_device *pdev;

	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	if (!init_done) {
		pdev = of_platform_device_create(np, NULL,
						 platform_bus_type.dev_root);
		if (!pdev)
			return -ENOMEM;

		ret = platform_driver_register(&mtk_iommu_driver);
		if (ret) {
			pr_err("%s: Failed to register driver\n", __func__);
			return ret;
		}
		init_done = true;
	}

	of_iommu_set_ops(np, &mtk_iommu_ops);
	return 0;
}
#else
static int mtk_iommu_init_fn(struct device_node *np)
{
	int ret;

	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	ret = platform_driver_register(&mtk_iommu_driver);
	if (ret) {
		pr_err("%s: Failed to register driver\n", __func__);
		return ret;
	}

	return 0;
}
#endif

IOMMU_OF_DECLARE(mtk_iommu, "mediatek,iommu_v0", mtk_iommu_init_fn);
