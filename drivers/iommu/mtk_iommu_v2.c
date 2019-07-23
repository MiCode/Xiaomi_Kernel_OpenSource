// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include "mtk_iommu.h"
#ifdef CONFIG_MTK_IOMMU_MISC_DBG
#include "m4u_debug.h"
#endif

#define REG_MMU_PT_BASE_ADDR			0x000
#define MMU_PT_ADDR_MASK			GENMASK(31, 7)

#define REG_MMU_INVALIDATE			0x020
#define F_ALL_INVLD				0x2
#define F_MMU_INV_RANGE				0x1

#define REG_MMU_INVLD_START_A			0x024
#define REG_MMU_INVLD_END_A			0x028

#define REG_MMU_INV_SEL				0x038
#define REG_MMU_INV_SEL_MT6779			0x02c
#define F_INVLD_EN0				BIT(0)
#define F_INVLD_EN1				BIT(1)

#define REG_MMU_STANDARD_AXI_MODE		0x048

#define REG_MMU_MISC_CRTL_MT6779		0x048
#define REG_MMU_STANDARD_AXI_MODE_MT6779	(BIT(3) | BIT(19))
#define REG_MMU_COHERENCE_EN			(BIT(0) | BIT(16))
#define REG_MMU_IN_ORDER_WR_EN			(BIT(1) | BIT(17))
#define F_MMU_HALF_ENTRY_MODE_L			(BIT(5) | BIT(21))
#define F_MMU_BLOCKING_MODE_L			(BIT(4) | BIT(20))

#define REG_MMU_DCM_DIS				0x050

#define REG_MMU_WR_LEN				0x054
#define F_MMU_WR_THROT_DIS			(BIT(5) |  BIT(21))

#define REG_MMU_CTRL_REG			0x110
#define F_MMU_TF_PROT_TO_PROGRAM_ADDR		(2 << 4)
#define F_MMU_PREFETCH_RT_REPLACE_MOD		BIT(4)
#define F_MMU_TF_PROT_TO_PROGRAM_ADDR_MT8173	(2 << 5)

#define REG_MMU_IVRP_PADDR			0x114

#define REG_MMU_VLD_PA_RNG			0x118
#define F_MMU_VLD_PA_RNG(EA, SA)		(((EA) << 8) | (SA))

#define REG_MMU_INT_CONTROL0			0x120
#define F_L2_MULIT_HIT_EN			BIT(0)
#define F_TABLE_WALK_FAULT_INT_EN		BIT(1)
#define F_PREETCH_FIFO_OVERFLOW_INT_EN		BIT(2)
#define F_MISS_FIFO_OVERFLOW_INT_EN		BIT(3)
#define F_PREFETCH_FIFO_ERR_INT_EN		BIT(5)
#define F_MISS_FIFO_ERR_INT_EN			BIT(6)
#define F_INT_CLR_BIT				BIT(12)

#define REG_MMU_INT_MAIN_CONTROL		0x124
						/* mmu0 | mmu1 */
#define F_INT_TRANSLATION_FAULT			(BIT(0) | BIT(7))
#define F_INT_MAIN_MULTI_HIT_FAULT		(BIT(1) | BIT(8))
#define F_INT_INVALID_PA_FAULT			(BIT(2) | BIT(9))
#define F_INT_ENTRY_REPLACEMENT_FAULT		(BIT(3) | BIT(10))
#define F_INT_TLB_MISS_FAULT			(BIT(4) | BIT(11))
#define F_INT_MISS_TRANSACTION_FIFO_FAULT	(BIT(5) | BIT(12))
#define F_INT_PRETETCH_TRANSATION_FIFO_FAULT	(BIT(6) | BIT(13))

#define REG_MMU_CPE_DONE			0x12C

#define REG_MMU_FAULT_ST1			0x134
#define F_REG_MMU0_FAULT_MASK			GENMASK(6, 0)
#define F_REG_MMU1_FAULT_MASK			GENMASK(13, 7)

#define REG_MMU0_FAULT_VA			0x13c
#define F_MMU_FAULT_VA_WRITE_BIT		BIT(1)
#define F_MMU_FAULT_VA_LAYER_BIT		BIT(0)

#define REG_MMU0_INVLD_PA			0x140
#define REG_MMU1_FAULT_VA			0x144
#define REG_MMU1_INVLD_PA			0x148
#define REG_MMU0_INT_ID				0x150
#define REG_MMU1_INT_ID				0x154
#define F_MMU_INT_ID_COMM_ID(a)			(((a) >> 9) & 0x7)
#define F_MMU_INT_ID_SUB_COMM_ID(a)		(((a) >> 7) & 0x3)
#define F_MMU_INT_ID_LARB_ID(a)			(((a) >> 7) & 0x7)
#define F_MMU_INT_ID_PORT_ID(a)			(((a) >> 2) & 0x1f)
#define F_MMU_INT_ID_COMM_APU_ID(a)		((a) & 0x3)
#define F_MMU_INT_ID_SUB_APU_ID(a)		(((a) >> 2) & 0x3)

#define MTK_PROTECT_PA_ALIGN			256

/*
 * Get the local arbiter ID and the portid within the larb arbiter
 * from mtk_m4u_id which is defined by MTK_M4U_ID.
 */
#define MTK_M4U_ID(larb, port)		(((larb) << 5) | (port))
#define MTK_M4U_TO_LARB(id)		(((id) >> 5) & 0xf)
#define MTK_M4U_TO_PORT(id)		((id) & 0x1f)

struct mtk_iommu_resv_iova_region {
	unsigned int dom_id;
	dma_addr_t iova_base;
	size_t iova_size;
	enum iommu_resv_type type;
};

static const struct iommu_ops mtk_iommu_ops;

/*
 * In M4U 4GB mode, the physical address is remapped as below:
 *  CPU PA         ->   M4U HW PA
 *  0x4000_0000         0x1_4000_0000 (Add bit32)
 *  0x8000_0000         0x1_8000_0000 ...
 *  0xc000_0000         0x1_c000_0000 ...
 *  0x1_0000_0000       0x1_0000_0000 (No change)
 *
 * Thus, We always add BIT32 in the iommu_map and disable BIT32 if PA is >=
 * 0x1_4000_0000 in the iova_to_phys.
 */
#define MTK_IOMMU_4GB_MODE_PA_140000000     0x140000000UL

static LIST_HEAD(m4ulist);	/* List all the M4U HWs */

#define for_each_m4u(data)	list_for_each_entry(data, &m4ulist, list)

/*
 * There may be 1 or 2 M4U HWs, But we always expect they are in the same domain
 * for the performance.
 *
 * Here always return the mtk_iommu_data of the first probed M4U where the
 * iommu domain information is recorded.
 */

/*
 * reserved IOVA Domain for IOMMU users of HW limitation.
 */

/*
 * struct mtk_domain_data:	domain configuration
 * @min_iova:	Start address of iova
 * @max_iova:	End address of iova
 * @port_mask:	User can specify mtk_iommu_domain by smi larb and port.
 *		Different mtk_iommu_domain have different iova space,
 *		port_mask is made up of larb_id and port_id.
 *		The format of larb and port can refer to mtxxxx-larb-port.h.
 *		bit[4:0] = port_id  bit[11:5] = larb_id.
 * Note: one user can only belong to one IOVAD,
 * the port mask is in unit of SMI larb.
 */
#define MTK_MAX_PORT_NUM	5

struct mtk_domain_data {
	unsigned long min_iova;
	unsigned long max_iova;
	unsigned int port_mask[MTK_MAX_PORT_NUM];
};

const struct mtk_domain_data single_dom = {
	.min_iova = 0x0,
	.max_iova = DMA_BIT_MASK(32)
};

/*
 * related file: mt6779-larb-port.h
 */
const struct mtk_domain_data mt6779_multi_dom[] = {
	/* normal  domain */
	{
	 .min_iova = 0x0,
	 .max_iova = DMA_BIT_MASK(32),
	},
	/* ccu domain */
	{
	 .min_iova = 0x40000000,
	 .max_iova = 0x48000000 - 1,
	 .port_mask = {MTK_M4U_ID(9, 21), MTK_M4U_ID(9, 22),
		       MTK_M4U_ID(12, 0), MTK_M4U_ID(12, 1)}
	},
	/* vpu domain */
	{
	 .min_iova = 0x7da00000,
	 .max_iova = 0x7fc00000 - 1,
	 .port_mask = {MTK_M4U_ID(13, 0)}
	}
};

struct mtk_iommu_domain {
	unsigned int			id;
	struct iommu_domain		domain;
	struct iommu_group		*group;
	struct mtk_iommu_pgtable	*pgtable;
	struct mtk_iommu_data		*data;
	struct list_head		list;
};

struct mtk_iommu_pgtable {
	spinlock_t		pgtlock; /* lock for page table */
	struct io_pgtable_cfg	cfg;
	struct io_pgtable_ops	*iop;
	const struct mtk_domain_data	*dom_region;
	struct list_head	m4u_dom_v2;
	spinlock_t		domain_lock; /* lock for page table */
	unsigned int		domain_count;
	unsigned int		init_domain_id;
};

static struct mtk_iommu_pgtable *share_pgtable;

static struct mtk_iommu_pgtable *mtk_iommu_get_pgtable(void)
{
	return share_pgtable;
}

static unsigned int __mtk_iommu_get_domain_id(
		struct mtk_iommu_data *data,
		unsigned int portid)
{
	unsigned int dom_id = 0;
	const struct mtk_domain_data *mtk_dom_array = data->plat_data->dom_data;
	int i, j;

	for (i = 0; i < data->plat_data->dom_cnt; i++) {
		for (j = 0; j < MTK_MAX_PORT_NUM; j++) {
			if (portid == mtk_dom_array[i].port_mask[j])
				return i;
		}
	}

	return dom_id;
}

static unsigned int mtk_iommu_get_domain_id(
					struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	int portid = fwspec->ids[0];

	return __mtk_iommu_get_domain_id(data, portid);
}

static struct iommu_domain *__mtk_iommu_get_domain(
				struct mtk_iommu_data *data,
				unsigned int larbid, unsigned int portid)
{
	unsigned int domain_id = 0; /* single dom id */
	unsigned int port_mask = MTK_M4U_ID(larbid, portid);
	struct mtk_iommu_domain *dom;

	domain_id = __mtk_iommu_get_domain_id(data, port_mask);

	list_for_each_entry(dom, &data->pgtable->m4u_dom_v2, list) {
		if (dom->id == domain_id)
			return &dom->domain;
	}
	return NULL;
}

static struct mtk_iommu_domain *__mtk_iommu_get_mtk_domain(
					struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	struct mtk_iommu_domain *dom;
	unsigned int domain_id;

	domain_id = mtk_iommu_get_domain_id(dev);
	if (domain_id == data->plat_data->dom_cnt)
		return NULL;

	list_for_each_entry(dom, &data->pgtable->m4u_dom_v2, list) {
		if (dom->id == domain_id)
			return dom;
	}
	return NULL;
}

static struct iommu_group *mtk_iommu_get_group(
					struct device *dev)
{
	struct mtk_iommu_domain *dom;

	dom = __mtk_iommu_get_mtk_domain(dev);
	if (dom)
		return dom->group;

	return NULL;
}

static struct mtk_iommu_domain *to_mtk_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct mtk_iommu_domain, domain);
}

static void mtk_iommu_tlb_flush_all(void *cookie)
{
	struct mtk_iommu_data *data = cookie;

	for_each_m4u(data) {
		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + data->plat_data->inv_sel_reg);
		writel_relaxed(F_ALL_INVLD, data->base + REG_MMU_INVALIDATE);
		wmb(); /* Make sure the tlb flush all done */
	}
}

static void mtk_iommu_tlb_add_flush_nosync(unsigned long iova, size_t size,
					   size_t granule, bool leaf,
					   void *cookie)
{
	struct mtk_iommu_data *data = cookie;
	int ret;
	u32 tmp;

	for_each_m4u(data) {
		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + data->plat_data->inv_sel_reg);

		writel_relaxed(iova, data->base + REG_MMU_INVLD_START_A);
		writel_relaxed(iova + size - 1,
			       data->base + REG_MMU_INVLD_END_A);
		writel(F_MMU_INV_RANGE,
			       data->base + REG_MMU_INVALIDATE);

		ret = readl_poll_timeout_atomic(data->base + REG_MMU_CPE_DONE,
						tmp, tmp != 0, 10, 3000);
		if (ret) {
			dev_warn(data->dev,
				 "Partial TLB flush timed out, falling back to full flush\n");
			mtk_iommu_tlb_flush_all(cookie);
		}
		writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
	}
}

static void mtk_iommu_tlb_sync(void *cookie)
{

}

static const struct iommu_gather_ops mtk_iommu_gather_ops = {
	.tlb_flush_all = mtk_iommu_tlb_flush_all,
	.tlb_add_flush = mtk_iommu_tlb_add_flush_nosync,
	.tlb_sync = mtk_iommu_tlb_sync,
};

static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = dev_id;
	struct iommu_domain *domain;
	u32 int_state, regval, fault_iova, fault_pa;
	unsigned int fault_larb, fault_port, sub_comm = 0;
	bool layer, write, is_vpu = false;

	/* Read error info from registers */
	int_state = readl_relaxed(data->base + REG_MMU_FAULT_ST1);
	if (int_state & F_REG_MMU0_FAULT_MASK) {
		regval = readl_relaxed(data->base + REG_MMU0_INT_ID);
		fault_iova = readl_relaxed(data->base + REG_MMU0_FAULT_VA);
		fault_pa = readl_relaxed(data->base + REG_MMU0_INVLD_PA);
	} else {
		regval = readl_relaxed(data->base + REG_MMU1_INT_ID);
		fault_iova = readl_relaxed(data->base + REG_MMU1_FAULT_VA);
		fault_pa = readl_relaxed(data->base + REG_MMU1_INVLD_PA);
	}
	layer = fault_iova & F_MMU_FAULT_VA_LAYER_BIT;
	write = fault_iova & F_MMU_FAULT_VA_WRITE_BIT;
	fault_port = F_MMU_INT_ID_PORT_ID(regval);
	if (data->plat_data->has_sub_comm[data->m4u_id]) {
		/* m4u1 is VPU in mt6779.*/
		if (data->m4u_id && data->plat_data->m4u_plat == M4U_MT6779) {
			fault_larb = F_MMU_INT_ID_COMM_APU_ID(regval);
			sub_comm = F_MMU_INT_ID_SUB_APU_ID(regval);
			fault_port = 0; /* for mt6779 APU ID is irregular */
			is_vpu = true;
		} else {
			fault_larb = F_MMU_INT_ID_COMM_ID(regval);
			sub_comm = F_MMU_INT_ID_SUB_COMM_ID(regval);
		}
	} else {
		fault_larb = F_MMU_INT_ID_LARB_ID(regval);
	}

	fault_larb = data->plat_data->larbid_remap[data->m4u_id][fault_larb];

	domain = __mtk_iommu_get_domain(data, fault_larb, fault_port);
#ifdef CONFIG_MTK_IOMMU_MISC_DBG
	report_custom_iommu_fault(fault_iova, fault_pa,
					regval, is_vpu);
#endif
	if (report_iommu_fault(domain, data->dev, fault_iova,
			       write ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ)) {
		dev_err_ratelimited(
			data->dev,
			"fault type=0x%x iova=0x%x pa=0x%x larb=%d sub_comm=%d port=%d regval=0x%x layer=%d %s\n",
			int_state, fault_iova, fault_pa, fault_larb,
			sub_comm, fault_port, regval,
			layer, write ? "write" : "read");
	}

	/* Interrupt clear */
	regval = readl_relaxed(data->base + REG_MMU_INT_CONTROL0);
	regval |= F_INT_CLR_BIT;
	writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);

	mtk_iommu_tlb_flush_all(data);

	return IRQ_HANDLED;
}

static void mtk_iommu_config(struct mtk_iommu_data *data,
			     struct device *dev, bool enable)
{
	struct mtk_smi_larb_iommu    *larb_mmu;
	unsigned int                 larbid, portid;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct device_link *link;
	int i;

	for (i = 0; i < fwspec->num_ids; ++i) {
		larbid = MTK_M4U_TO_LARB(fwspec->ids[i]);
		portid = MTK_M4U_TO_PORT(fwspec->ids[i]);
		larb_mmu = &data->smi_imu.larb_imu[larbid];

		dev_dbg(dev, "%s iommu port: %d\n",
			enable ? "enable" : "disable", portid);

		if (enable) {
			larb_mmu->mmu |= MTK_SMI_MMU_EN(portid);
			/* Link the consumer with the larb device(supplier) */
			link = device_link_add(dev, larb_mmu->dev,
					       DL_FLAG_PM_RUNTIME |
					       DL_FLAG_AUTOREMOVE_CONSUMER);
			if (!link) {
				dev_err(dev, "Unable to link %s\n",
					dev_name(larb_mmu->dev));
				return;
			}
		} else {
			larb_mmu->mmu &= ~MTK_SMI_MMU_EN(portid);
		}
	}
}

static struct iommu_group *mtk_iommu_create_domain(
			struct mtk_iommu_data *data, struct device *dev)
{
	struct mtk_iommu_pgtable *pgtable = mtk_iommu_get_pgtable();
	struct mtk_iommu_domain *dom;
	struct iommu_group *group;
	unsigned long flags;

	group = mtk_iommu_get_group(dev);

	if (group) {
		iommu_group_ref_get(group);
		return group;
	}

	/* init mtk_iommu_domain */
	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return ERR_PTR(-ENOMEM);

	/* init iommu_group */
	group = iommu_group_alloc();
	if (IS_ERR(group)) {
		dev_notice(dev, "Failed to allocate M4U IOMMU group\n");
		goto free_dom;
	}
	dom->group = group;
	dom->id = mtk_iommu_get_domain_id(dev);
	dom->data = data;
	dom->pgtable = pgtable;
	spin_lock_irqsave(&pgtable->domain_lock, flags);
	if (pgtable->domain_count >= data->plat_data->dom_cnt) {
		spin_unlock_irqrestore(&pgtable->domain_lock, flags);
		pr_notice("%s, %d, too many domain, count=%d\n",
			__func__, __LINE__, pgtable->domain_count);
		return NULL;
	}
	pgtable->init_domain_id = dom->id;
	pgtable->domain_count++;
	spin_unlock_irqrestore(&pgtable->domain_lock, flags);
	list_add_tail(&dom->list, &pgtable->m4u_dom_v2);

	pr_notice("%s: dev:%s, dom_id:%u, dom_cnt:%u\n",
		    __func__, dev_name(dev),
		    dom->id, pgtable->domain_count);

	return group;

free_dom:
	kfree(dom);
	return NULL;
}

static struct mtk_iommu_pgtable *mtk_iommu_create_pgtable(
			struct mtk_iommu_data *data)
{
	struct mtk_iommu_pgtable *pgtable;

	pgtable = kzalloc(sizeof(*pgtable), GFP_KERNEL);
	if (!pgtable)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&pgtable->pgtlock);
	spin_lock_init(&pgtable->domain_lock);
	pgtable->domain_count = 0;
	INIT_LIST_HEAD(&pgtable->m4u_dom_v2);

	pgtable->cfg = (struct io_pgtable_cfg) {
		.quirks = IO_PGTABLE_QUIRK_ARM_NS |
			IO_PGTABLE_QUIRK_NO_PERMS |
			IO_PGTABLE_QUIRK_TLBI_ON_MAP,
		.pgsize_bitmap = mtk_iommu_ops.pgsize_bitmap,
		.ias = 32,
		.oas = 32,
		.tlb = &mtk_iommu_gather_ops,
		.iommu_dev = data->dev,
	};

	if (data->dram_is_4gb) /* need it ? */
		pgtable->cfg.quirks |= IO_PGTABLE_QUIRK_ARM_MTK_4GB;

	pgtable->iop = alloc_io_pgtable_ops(ARM_V7S, &pgtable->cfg, data);
	if (!pgtable->iop) {
		dev_err(data->dev, "Failed to alloc io pgtable\n");
		return ERR_PTR(-EINVAL);
	}

	pgtable->dom_region = data->plat_data->dom_data;

	pr_notice("%s create pgtable done\n", __func__);

	return pgtable;
}

static int mtk_iommu_attach_pgtable(struct mtk_iommu_data *data,
			struct device *dev)
{
	struct mtk_iommu_pgtable *pgtable = mtk_iommu_get_pgtable();

	/* create share pgtable */
	if (!pgtable) {
		pgtable = mtk_iommu_create_pgtable(data);
		if (IS_ERR(pgtable)) {
			dev_err(data->dev, "Failed to create pgtable\n");
			return -ENOMEM;
		}

		share_pgtable = pgtable;
	}

	/* binding to pgtable */
	data->pgtable = pgtable;

	/* update HW settings */
	writel(pgtable->cfg.arm_v7s_cfg.ttbr[0] & MMU_PT_ADDR_MASK,
		       data->base + REG_MMU_PT_BASE_ADDR);

	pr_notice("m4u%u attach_pgtable done!\n", data->m4u_id);

	return 0;
}

static struct iommu_domain *mtk_iommu_domain_alloc(unsigned int type)
{
	struct mtk_iommu_domain *dom_tmp, *dom = NULL;
	struct mtk_iommu_pgtable *pgtable = mtk_iommu_get_pgtable();
	unsigned int id = pgtable->init_domain_id;
	/* allocated at device_group for IOVA  space management by iovad */
	unsigned int domain_type = IOMMU_DOMAIN_DMA;

	if (type != domain_type) {
		pr_notice("%s, %d, err type%d\n",
			__func__, __LINE__, type);
		return NULL;
	}

	list_for_each_entry(dom_tmp, &pgtable->m4u_dom_v2, list) {
		if (dom_tmp->id == id) {
			dom = dom_tmp;
			break;
		}
	}

	if (!dom)
		return NULL;

	if (iommu_get_dma_cookie(&dom->domain))
		goto free_dom;

	dom->domain.geometry.aperture_start =
				pgtable->dom_region[dom->id].min_iova;
	dom->domain.geometry.aperture_end =
				pgtable->dom_region[dom->id].max_iova;
	dom->domain.geometry.force_aperture = true;

	/* Update our support page sizes bitmap */
	dom->domain.pgsize_bitmap = pgtable->cfg.pgsize_bitmap;

	pr_notice("%s, allocated the %u start:%pa, end:%pa\n",
		    __func__,
		    dom->id,
		    &dom->domain.geometry.aperture_start,
		    &dom->domain.geometry.aperture_end);

	return &dom->domain;

free_dom:
	kfree(dom);
	return NULL;
}

static void mtk_iommu_domain_free(struct iommu_domain *domain)
{
	unsigned long flags;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;

	pr_notice("%s, %d, domain_count=%d, free the %d domain\n",
		    __func__, __LINE__, pgtable->domain_count, dom->id);

	iommu_put_dma_cookie(domain);

	kfree(dom);

	spin_lock_irqsave(&pgtable->domain_lock, flags);
	pgtable->domain_count--;
	if (pgtable->domain_count > 0) {
		spin_unlock_irqrestore(&pgtable->domain_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&pgtable->domain_lock, flags);
	free_io_pgtable_ops(pgtable->iop);
	kfree(pgtable);
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;

	if (!data)
		return -ENODEV;

	mtk_iommu_config(data, dev, true);
	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;

	if (!data)
		return;

	mtk_iommu_config(data, dev, false);
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_data *data = dom->data;
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	int ret;

	/* The "4GB mode" M4U physically can not use the lower remap of Dram. */
	if (data->plat_data->has_4gb_mode && data->dram_is_4gb)
		paddr |= BIT_ULL(32);

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	ret = pgtable->iop->map(pgtable->iop, iova, paddr, size, prot);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	return ret;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	size_t unmapsz;

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	unmapsz = pgtable->iop->unmap(pgtable->iop, iova, size);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	return unmapsz;
}

static void mtk_iommu_iotlb_sync(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_sync(dom->data);
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	struct mtk_iommu_data *data = dom->data;
	unsigned long flags;
	phys_addr_t pa;

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	pa = pgtable->iop->iova_to_phys(pgtable->iop, iova);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	if (data->plat_data->has_4gb_mode && data->dram_is_4gb &&
	    pa >= MTK_IOMMU_4GB_MODE_PA_140000000)
		pa &= ~BIT_ULL(32);

	return pa;
}

static int mtk_iommu_add_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;
	struct iommu_group *group;

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return -ENODEV; /* Not a iommu client device */

	data = fwspec->iommu_priv;
	iommu_device_link(&data->iommu, dev);

	group = iommu_group_get_for_dev(dev);
	if (IS_ERR(group))
		return PTR_ERR(group);

	iommu_group_put(group);
	return 0;
}

static void mtk_iommu_remove_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return;

	data = fwspec->iommu_priv;
	iommu_device_unlink(&data->iommu, dev);

	iommu_group_remove_device(dev);
	iommu_fwspec_free(dev);
}

static struct iommu_group *mtk_iommu_device_group(struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	struct mtk_iommu_pgtable *pgtable;
	int ret = 0;

	if (!data)
		return ERR_PTR(-ENODEV);

	pgtable = data->pgtable;
	if (!pgtable) {
		ret = mtk_iommu_attach_pgtable(data, dev);
		if (ret) {
			dev_err(data->dev, "Failed to device_group\n");
			return NULL;
		}
	}

	pr_notice("%s, init data:%u\n", __func__, data->m4u_id);
	return mtk_iommu_create_domain(data, dev);
}

static int mtk_iommu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct platform_device *m4updev;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!fwspec->iommu_priv) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		if (WARN_ON(!m4updev))
			return -EINVAL;

		fwspec->iommu_priv = platform_get_drvdata(m4updev);
	}

	return iommu_fwspec_add_ids(dev, args->args, 1);
}

#ifdef CONFIG_ARM64
/* reserve/dir-map iova region for arm64 evb */
static void mtk_iommu_get_resv_regions(struct device *dev,
				       struct list_head *head)
{
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;
	unsigned int i, total_cnt = data->plat_data->resv_cnt;
	unsigned int dom_id = mtk_iommu_get_domain_id(dev);
	const struct mtk_iommu_resv_iova_region *resv_data;
	struct iommu_resv_region *region;
	unsigned long base = 0;
	size_t size = 0;
	int prot = IOMMU_WRITE | IOMMU_READ;

	resv_data = data->plat_data->resv_region;

	for (i = 0; i < total_cnt; i++) {
		size = 0;
		if (resv_data[i].iova_size) {
			base = (unsigned long)resv_data[i].iova_base;
			size = resv_data[i].iova_size;
		}
		if (!size || resv_data[i].dom_id != dom_id)
			continue;

		region = iommu_alloc_resv_region(base, size, prot,
						 resv_data[i].type);
		if (!region)
			return;

		list_add_tail(&region->list, head);

		dev_dbg(data->dev, "%s iova 0x%x ~ 0x%x\n",
			(resv_data[i].type == IOMMU_RESV_DIRECT) ? "dm" : "rsv",
			(unsigned int)base, (unsigned int)(base + size - 1));
	}
}

static void mtk_iommu_put_resv_regions(struct device *dev,
				      struct list_head *head)
{
	struct iommu_resv_region *entry, *next;

	list_for_each_entry_safe(entry, next, head, list)
		kfree(entry);
}
#endif

static const struct iommu_ops mtk_iommu_ops = {
	.domain_alloc	= mtk_iommu_domain_alloc,
	.domain_free	= mtk_iommu_domain_free,
	.attach_dev	= mtk_iommu_attach_device,
	.detach_dev	= mtk_iommu_detach_device,
	.map		= mtk_iommu_map,
	.unmap		= mtk_iommu_unmap,
	.flush_iotlb_all = mtk_iommu_iotlb_sync,
	.iotlb_sync	= mtk_iommu_iotlb_sync,
	.iova_to_phys	= mtk_iommu_iova_to_phys,
	.add_device	= mtk_iommu_add_device,
	.remove_device	= mtk_iommu_remove_device,
	.device_group	= mtk_iommu_device_group,
	.of_xlate	= mtk_iommu_of_xlate,
#ifdef CONFIG_ARM64
	.get_resv_regions = mtk_iommu_get_resv_regions,
	.put_resv_regions = mtk_iommu_put_resv_regions,
#endif
	.pgsize_bitmap	= SZ_4K | SZ_64K | SZ_1M | SZ_16M,
};

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data)
{
	u32 regval;
	int ret;

	ret = clk_prepare_enable(data->bclk);
	if (ret) {
		dev_err(data->dev, "Failed to enable iommu bclk(%d)\n", ret);
		return ret;
	}

	regval = readl_relaxed(data->base + REG_MMU_CTRL_REG);
	if (data->plat_data->m4u_plat == M4U_MT8173)
		regval |= F_MMU_PREFETCH_RT_REPLACE_MOD |
			 F_MMU_TF_PROT_TO_PROGRAM_ADDR_MT8173;
	else
		regval |= F_MMU_TF_PROT_TO_PROGRAM_ADDR;
	writel_relaxed(regval, data->base + REG_MMU_CTRL_REG);

	regval = F_L2_MULIT_HIT_EN |
		F_TABLE_WALK_FAULT_INT_EN |
		F_PREETCH_FIFO_OVERFLOW_INT_EN |
		F_MISS_FIFO_OVERFLOW_INT_EN |
		F_PREFETCH_FIFO_ERR_INT_EN |
		F_MISS_FIFO_ERR_INT_EN;
	writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);

	regval = F_INT_TRANSLATION_FAULT |
		F_INT_MAIN_MULTI_HIT_FAULT |
		F_INT_INVALID_PA_FAULT |
		F_INT_ENTRY_REPLACEMENT_FAULT |
		F_INT_TLB_MISS_FAULT |
		F_INT_MISS_TRANSACTION_FIFO_FAULT |
		F_INT_PRETETCH_TRANSATION_FIFO_FAULT;
	writel_relaxed(regval, data->base + REG_MMU_INT_MAIN_CONTROL);

	if (data->plat_data->m4u_plat == M4U_MT8173)
		regval = (data->protect_base >> 1) | (data->dram_is_4gb << 31);
	else
		regval = lower_32_bits(data->protect_base) |
			 upper_32_bits(data->protect_base);
	writel_relaxed(regval, data->base + REG_MMU_IVRP_PADDR);

	if (data->dram_is_4gb && data->plat_data->has_vld_pa_rng) {
		/*
		 * If 4GB mode is enabled, the validate PA range is from
		 * 0x1_0000_0000 to 0x1_ffff_ffff. here record bit[32:30].
		 */
		regval = F_MMU_VLD_PA_RNG(7, 4);
		writel_relaxed(regval, data->base + REG_MMU_VLD_PA_RNG);
	}
	writel_relaxed(0, data->base + REG_MMU_DCM_DIS);

	if (data->plat_data->reset_axi)
		writel_relaxed(0, data->base + REG_MMU_STANDARD_AXI_MODE);

	if (data->plat_data->has_wr_len) {
		/* write command throttling mode */
		regval = readl_relaxed(data->base + REG_MMU_WR_LEN);
		regval &= ~F_MMU_WR_THROT_DIS;
		writel_relaxed(regval, data->base + REG_MMU_WR_LEN);
	}
	/* special settings for mmu0 (multimedia iommu) */
	if (data->plat_data->has_misc_ctrl[data->m4u_id]) {
		regval = readl_relaxed(data->base + REG_MMU_MISC_CRTL_MT6779);
		/* non-standard AXI mode */
		regval &= ~REG_MMU_STANDARD_AXI_MODE_MT6779;
		writel_relaxed(regval, data->base + REG_MMU_MISC_CRTL_MT6779);
	}

	if (devm_request_irq(data->dev, data->irq, mtk_iommu_isr, 0,
			     dev_name(data->dev), (void *)data)) {
		writel_relaxed(0, data->base + REG_MMU_PT_BASE_ADDR);
		clk_disable_unprepare(data->bclk);
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
	struct mtk_iommu_data   *data;
	struct device           *dev = &pdev->dev;
	struct resource         *res;
	resource_size_t		ioaddr;
	struct component_match  *match = NULL;
	void                    *protect;
	int                     i, larb_nr, ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = dev;
	data->plat_data = of_device_get_match_data(dev);

	/* Protect memory. HW will access here while translation fault.*/
	protect = devm_kzalloc(dev, MTK_PROTECT_PA_ALIGN * 2, GFP_KERNEL);
	if (!protect)
		return -ENOMEM;
	data->protect_base = ALIGN(virt_to_phys(protect), MTK_PROTECT_PA_ALIGN);

	/* Whether the current dram is 4GB. */
	data->dram_is_4gb = !!(max_pfn > (BIT_ULL(32) >> PAGE_SHIFT));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);
	ioaddr = res->start;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0)
		return data->irq;

	if (data->plat_data->has_bclk) {
		data->bclk = devm_clk_get(dev, "bclk");
		if (IS_ERR(data->bclk))
			return PTR_ERR(data->bclk);
	}

	larb_nr = of_count_phandle_with_args(dev->of_node,
					     "mediatek,larbs", NULL);
	if (larb_nr < 0)
		return larb_nr;

	for (i = 0; i < larb_nr; i++) {
		struct device_node *larbnode;
		struct platform_device *plarbdev;
		u32 id;

		larbnode = of_parse_phandle(dev->of_node, "mediatek,larbs", i);
		if (!larbnode)
			return -EINVAL;

		if (!of_device_is_available(larbnode))
			continue;

		ret = of_property_read_u32(larbnode, "mediatek,larb-id", &id);
		if (ret)/* The id is consecutive if there is no this property */
			id = i;

		plarbdev = of_find_device_by_node(larbnode);
		if (!plarbdev || !plarbdev->dev.driver)
			return -EPROBE_DEFER;
		data->smi_imu.larb_imu[id].dev = &plarbdev->dev;

		if (data->plat_data->m4u1_mask == (1 << id))
			data->m4u_id = 1;

		component_match_add_release(dev, &match, release_of,
					    compare_of, larbnode);
	}

	platform_set_drvdata(pdev, data);

	ret = mtk_iommu_hw_init(data);
	if (ret)
		return ret;

	ret = iommu_device_sysfs_add(&data->iommu, dev, NULL,
				     "mtk-iommu.%pa", &ioaddr);
	if (ret)
		return ret;

	iommu_device_set_ops(&data->iommu, &mtk_iommu_ops);
	iommu_device_set_fwnode(&data->iommu, &pdev->dev.of_node->fwnode);

	ret = iommu_device_register(&data->iommu);
	if (ret)
		return ret;

	list_add_tail(&data->list, &m4ulist);

	if (!iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);

	return component_master_add_with_match(dev, &mtk_iommu_com_ops, match);
}

static void mtk_iommu_shutdown(struct platform_device *pdev)
{
	struct mtk_iommu_data *data = platform_get_drvdata(pdev);

	iommu_device_sysfs_remove(&data->iommu);
	iommu_device_unregister(&data->iommu);

	if (iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, NULL);

	clk_disable_unprepare(data->bclk);
	devm_free_irq(&pdev->dev, data->irq, data);
	component_master_del(&pdev->dev, &mtk_iommu_com_ops);
}

static int __maybe_unused mtk_iommu_suspend(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;

	reg->wr_len = readl_relaxed(base + REG_MMU_WR_LEN);
	reg->standard_axi_mode = readl_relaxed(base +
					       REG_MMU_STANDARD_AXI_MODE);
	reg->dcm_dis = readl_relaxed(base + REG_MMU_DCM_DIS);
	reg->ctrl_reg = readl_relaxed(base + REG_MMU_CTRL_REG);
	reg->int_control0 = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	reg->int_main_control = readl_relaxed(base + REG_MMU_INT_MAIN_CONTROL);
	reg->ivrp_paddr = readl_relaxed(base + REG_MMU_IVRP_PADDR);
	reg->vld_pa_rng = readl_relaxed(base + REG_MMU_VLD_PA_RNG);
	clk_disable_unprepare(data->bclk);
	return 0;
}

static int __maybe_unused mtk_iommu_resume(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_pgtable *pgtable = data->pgtable;
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;
	int ret;

	ret = clk_prepare_enable(data->bclk);
	if (ret) {
		dev_err(data->dev, "Failed to enable clk(%d) in resume\n", ret);
		return ret;
	}
	writel_relaxed(reg->wr_len, base + REG_MMU_WR_LEN);
	writel_relaxed(reg->standard_axi_mode,
		       base + REG_MMU_STANDARD_AXI_MODE);
	writel_relaxed(reg->dcm_dis, base + REG_MMU_DCM_DIS);
	writel_relaxed(reg->ctrl_reg, base + REG_MMU_CTRL_REG);
	writel_relaxed(reg->int_control0, base + REG_MMU_INT_CONTROL0);
	writel_relaxed(reg->int_main_control, base + REG_MMU_INT_MAIN_CONTROL);
	writel_relaxed(reg->ivrp_paddr, base + REG_MMU_IVRP_PADDR);
	writel_relaxed(reg->vld_pa_rng, base + REG_MMU_VLD_PA_RNG);
	if (pgtable)
		writel(pgtable->cfg.arm_v7s_cfg.ttbr[0] & MMU_PT_ADDR_MASK,
		       base + REG_MMU_PT_BASE_ADDR);
	return 0;
}

static const struct dev_pm_ops mtk_iommu_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_iommu_suspend, mtk_iommu_resume)
};

static const struct mtk_iommu_resv_iova_region mt2712_iommu_rsv_list[] = {
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	{
		.dom_id = 0,
		.iova_base = 0x4d000000UL,	   /* FastRVC */
		.iova_size = 0x8000000,
		.type = IOMMU_RESV_DIRECT,
	},
#endif
};

static const struct mtk_iommu_resv_iova_region mt6779_iommu_rsv_list[] = {
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	{
		.dom_id = 0,			/* Secure Region */
		.iova_base = 0x0,
		.iova_size = 0x13000000,
		.type = IOMMU_RESV_RESERVED,
	},
#endif
	{
		.dom_id = 0,
		.iova_base = 0x40000000,	/* CCU */
		.iova_size = 0x8000000,
		.type = IOMMU_RESV_RESERVED,
	},
	{
		.dom_id = 0,
		.iova_base = 0x7da00000,	/* VPU/MDLA */
		.iova_size = 0x2700000,
		.type = IOMMU_RESV_RESERVED,
	},
};

static const struct mtk_iommu_plat_data mt2712_data = {
	.m4u_plat     = M4U_MT2712,
	.has_4gb_mode = true,
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	.resv_cnt     = ARRAY_SIZE(mt2712_iommu_rsv_list),
	.resv_region  = mt2712_iommu_rsv_list,
#endif
	.has_bclk     = true,
	.has_vld_pa_rng   = true,
	.dom_cnt = 1,
	.dom_data = &single_dom,
	.larbid_remap[0] = {0, 1, 2, 3},
	.larbid_remap[1] = {4, 5, 7, 8, 9},
	.inv_sel_reg = REG_MMU_INV_SEL,
	.m4u1_mask =  BIT(4),
};

static const struct mtk_iommu_plat_data mt6779_data = {
	.m4u_plat = M4U_MT6779,
	.resv_cnt     = ARRAY_SIZE(mt6779_iommu_rsv_list),
	.resv_region  = mt6779_iommu_rsv_list,
	.dom_cnt = ARRAY_SIZE(mt6779_multi_dom),
	.dom_data = mt6779_multi_dom,
	.larbid_remap[0] = {0, 1, 2, 3, 5, 7, 10, 9},
	/* vp6a, vp6b, mdla/core2, mdla/edmc*/
	.larbid_remap[1] = {2, 0, 3, 1},
	.has_sub_comm = {true, true},
	.has_wr_len = true,
	.has_misc_ctrl = {true, false},
	.inv_sel_reg = REG_MMU_INV_SEL_MT6779,
	.m4u1_mask =  BIT(6),
};

static const struct mtk_iommu_plat_data mt8173_data = {
	.m4u_plat     = M4U_MT8173,
	.has_4gb_mode = true,
	.has_bclk     = true,
	.reset_axi    = true,
	.dom_cnt = 1,
	.dom_data = &single_dom,
	.larbid_remap[0] = {0, 1, 2, 3, 4, 5}, /* Linear mapping. */
	.inv_sel_reg = REG_MMU_INV_SEL,
};

static const struct mtk_iommu_plat_data mt8183_data = {
	.m4u_plat     = M4U_MT8183,
	.reset_axi    = true,
	.dom_cnt = 1,
	.dom_data = &single_dom,
	.larbid_remap[0] = {0, 4, 5, 6, 7, 2, 3, 1},
	.inv_sel_reg = REG_MMU_INV_SEL,
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,mt2712-m4u", .data = &mt2712_data},
	{ .compatible = "mediatek,mt6779-m4u", .data = &mt6779_data},
	{ .compatible = "mediatek,mt8173-m4u", .data = &mt8173_data},
	{ .compatible = "mediatek,mt8183-m4u", .data = &mt8183_data},
	{}
};

static struct platform_driver mtk_iommu_driver = {
	.probe	= mtk_iommu_probe,
	.shutdown = mtk_iommu_shutdown,
	.driver	= {
		.name = "mtk-iommu",
		.of_match_table = of_match_ptr(mtk_iommu_of_ids),
		.pm = &mtk_iommu_pm_ops,
	}
};
builtin_platform_driver(mtk_iommu_driver);
