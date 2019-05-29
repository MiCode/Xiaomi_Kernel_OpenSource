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
#include <linux/dma-debug.h>
#ifndef CONFIG_ARM64
#include <asm/dma-iommu.h>
#endif

#include "io-pgtable.h"
#include "mtk_iommu.h"

#define PREALLOC_DMA_DEBUG_ENTRIES	4096

#define TO_BE_IMPL

#define REG_MMU_PT_BASE_ADDR		0x000
#define MMU_PT_ADDR_MASK		GENMASK(31, 7)

#define REG_MMU_INVALIDATE		0x020
#define F_ALL_INVLD			BIT(1)
#define F_MMU_INV_RANGE			BIT(0)

#define REG_MMU_INVLD_START_A		0x024
#define REG_MMU_INVLD_END_A		0x028

#define REG_MMU_INV_SEL			0x02c
#define F_INVLD_EN0			BIT(0)
#define F_INVLD_EN1			BIT(1)


#define REG_MMU_MISC_CRTL		0x048
#define REG_MMU0_STANDARD_AXI_MODE	BIT(3)
#define REG_MMU1_STANDARD_AXI_MODE	BIT(19)
#define REG_MMU_MMU0_COHERENCE_EN	BIT(0)
#define REG_MMU_MMU1_COHERENCE_EN	BIT(16)
#define REG_MMU0_IN_ORDER_WR_EN		BIT(1)
#define REG_MMU1_IN_ORDER_WR_EN		BIT(17)
#define F_MMU_MMU1_HALF_ENTRY_MODE_L	BIT(21)
#define F_MMU_MMU0_HALF_ENTRY_MODE_L	BIT(5)
#define F_MMU_MMU1_BLOCKING_MODE_L	BIT(20)
#define F_MMU_MMU0_BLOCKING_MODE_L	BIT(4)

#define REG_MMU_STANDARD_AXI_MODE	(BIT(3) | BIT(19))
#define REG_MMU_COHERENCE_EN		(BIT(0) | BIT(16))
#define REG_MMU_IN_ORDER_WR_EN		(BIT(1) | BIT(17))
#define F_MMU_HALF_ENTRY_MODE_L		(BIT(5) | BIT(21))
#define F_MMU_BLOCKING_MODE_L		(BIT(4) | BIT(20))


#define REG_MMU_DCM_DIS			0x050

#define REG_MMU_WR_LEN			0x054
#define F_MMU_WR_THROT_DIS		(BIT(5) |  BIT(21))

#define REG_MMU_CTRL_REG		0x110
#define F_MMU_CTRL_INT_HANG_EN		0
#define F_MMU_CTRL_TF_PROTECT_SEL	BIT(5)
#define F_MMU_CTRL_MONITOR_CLR		0
#define F_MMU_CTRL_MONITOR_EN		0
#define F_MMU_CTRL_PFH_DIS		0
#define F_INT_CLR_BIT			BIT(12)

#define REG_MMU_IVRP_PADDR		0x114

#define REG_MMU_INT_CONTROL0		0x120

#define REG_MMU_INT_MAIN_CONTROL	0x124
#define F_INT_MMU0_MAIN_MSK		GENMASK(6, 0)
#define F_INT_MMU1_MAIN_MSK		GENMASK(13, 7)
#define F_INT_MMU0_MAU_MSK		BIT(14)
#define F_INT_MMU1_MAU_MSK		BIT(15)

#define REG_MMU_CPE_DONE		0x12c

#define REG_MMU_L2_FAULT_ST		0x130

#define REG_MMU_FAULT_ST1		0x134

#define REG_MMU0_FAULT_VA		0x13c
#define F_MMU_FAULT_VA_MSK		GENMASK(31, 12)
#define F_MMU_FAULT_VA_WRITE_BIT	BIT(1)
#define F_MMU_FAULT_VA_LAYER_BIT	BIT(0)

#define REG_MMU1_FAULT_VA		0x144
#define REG_MMU0_INVLD_PA		0x140
#define REG_MMU1_INVLD_PA		0x148
#define REG_MMU0_INT_ID			0x150
#define REG_MMU1_INT_ID			0x154

#define MTK_PROTECT_PA_ALIGN		256

/* bit[9:7] indicate larbid */
#define F_MMU0_INT_ID_LARB_ID(a)	(((a) >> 7) & 0xf)
/*
 * bit[6:2] indicate portid, bit[1:0] indicate master id, every port
 * have four types of command, master id indicate the m4u port's command
 * type, iommu do not care about this.
 */
#define F_MMU0_INT_ID_PORT_ID(a)	(((a) >> 2) & 0x1f)

#define F_MMU_IVRP_PA_SET(PA, EXT)  \
	((((unsigned long long)PA) & GENMASK(31, 7)) | \
	 ((((unsigned long long)PA) >> 32) & GENMASK(1, 0)))

/* Local arbiter ID */
#define MTK_M4U_TO_LARB(id)		(((id) >> 5) & 0xf)
/* PortID within the local arbiter */
#define MTK_M4U_TO_PORT(id)		((id) & 0x1f)

#define F_MMU_INT_ID_COMM_ID(a)		(((a) >> 9) & 0x7)
#define F_MMU_INT_ID_SUB_COMM_ID(a)	(((a) >> 7) & 0x3)


/* IO virtual address start page frame number */
#define IOVA_START_PFN			(1)
#define IOVA_PFN(addr)			((addr) >> PAGE_SHIFT)
#define DMA_32BIT_PFN			IOVA_PFN(DMA_BIT_MASK(32))

/*
 * reserved IOVA Domain for IOMMU users of HW limitation.
 */
#define MTK_IOVA_DOMAIN_COUNT	(3)
#define MTK_IOVA_REMOVE_CNT	(2)
enum mtk_reserve_region_type {
	// no reserved region
	IOVA_REGION_UNDEFINE,
	// users cannot touch the IOVA in the reserved region
	IOVA_REGION_REMOVE,
	// users can only touch the IOVA in the reserved region
	IOVA_REGION_STAY,
};
/*
 * struct mtk_iova_domain_data:	domain configuration
 * @min_iova:	start address of IOVAD
 * @max_iova:	end address of IOVAD
 * @resv_start: the start address of reserved region
 * @resv_size:	the size of reserved region
 * @resv_type:	the type of reserve region
 * @port_mask:	the user list of IOVAD
 * One user can only belongs to one IOVAD, the port mask is in unit of SMI larb.
 */
struct mtk_iova_domain_data {
	unsigned long min_iova;
	unsigned long max_iova;
	unsigned long resv_start[MTK_IOVA_REMOVE_CNT];
	unsigned long resv_size[MTK_IOVA_REMOVE_CNT];
	unsigned int resv_type;
	unsigned int port_mask[MTK_LARB_NR_MAX];
};

const struct mtk_iova_domain_data mtk_domain_array[MTK_IOVA_DOMAIN_COUNT] = {
	/* normal */
	{
	 .min_iova = 0x0,
	 .max_iova = DMA_BIT_MASK(32),
	 .resv_start = {0x40000000, 0x7da00000},
	 .resv_size = {0x8000000, 0x4C00000},
	 .resv_type = IOVA_REGION_REMOVE,
	 .port_mask = {0x1ff, 0x3fff, 0xfff, 0x7ffff, 0x0, 0x3ffffff,
		       0x7, 0xf, 0x3ff, 0x9fffff, 0x7fffffff, 0x1f,
		       0x0, 0x2}
	},
	/* ccu domain */
	{
	 .min_iova = 0x40000000,
	 .max_iova = 0x48000000 - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x3, 0x0}
	},
	/* vpu domain */
	{
	 .min_iova = 0x7da00000,
	 .max_iova = 0x7fc00000 - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x1}
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
	struct list_head	m4u_dom_v2;
	spinlock_t		domain_lock; /* lock for page table */
	unsigned int		domain_count;
	unsigned int		init_domain_id;
};

struct mtk_iommu_domain *to_mtk_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct mtk_iommu_domain, domain);
}

static struct iommu_ops mtk_iommu_ops;

static LIST_HEAD(m4ulist);

static unsigned int		total_iommu_cnt;
static unsigned int		init_data_id;
static struct mtk_iommu_pgtable *m4u_pgtable;
static bool			single_pt;

static struct mtk_iommu_data *mtk_iommu_get_m4u_data(int id)
{
	struct mtk_iommu_data *data;
	unsigned int i = 0;

	list_for_each_entry(data, &m4ulist, list) {
		if (data && data->plat_data->iommu_id == id &&
		    data->base && !IS_ERR(data->base))
			return data;
		if (++i >= total_iommu_cnt)
			return NULL;
	}

	pr_notice("%s, %d, failed to get data of %d\n", __func__, __LINE__, id);
	return NULL;
}

static struct mtk_iommu_pgtable *mtk_iommu_get_pgtable(
			struct mtk_iommu_data *data, unsigned int data_id)
{
	if (single_pt)
		return m4u_pgtable;

	if (data)
		return data->pgtable;

	data = mtk_iommu_get_m4u_data(data_id);
	if (data)
		return data->pgtable;

	return NULL;
}

int mtk_iommu_set_pgtable(
			struct mtk_iommu_data *data,
			unsigned int data_id,
			struct mtk_iommu_pgtable *value)
{
	if (single_pt)
		m4u_pgtable = value;

	if (data) {
		data->pgtable = value;
	} else {
		data = mtk_iommu_get_m4u_data(data_id);
		if (data)
			data->pgtable = value;
		else
			return -1;
	}

	return 0;
}

static unsigned int __mtk_iommu_get_domain_id(
				unsigned int larbid, unsigned int portid)
{
	unsigned int domain_id = MTK_IOVA_DOMAIN_COUNT;
	int i;

	if (larbid >= MTK_LARB_NR_MAX)
		return MTK_IOVA_DOMAIN_COUNT;

	for (i = 0; i < MTK_IOVA_DOMAIN_COUNT; i++) {
		if (mtk_domain_array[i].port_mask[larbid] &
		    (1 << portid)) {
			domain_id = i;
			break;
		}
	}

	return domain_id;
}

static unsigned int mtk_iommu_get_domain_id(
					struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	unsigned int larbid, portid;

	larbid = MTK_M4U_TO_LARB(fwspec->ids[0]);
	portid = MTK_M4U_TO_PORT(fwspec->ids[0]);

	return __mtk_iommu_get_domain_id(larbid, portid);
}

static struct iommu_domain *__mtk_iommu_get_domain(
				struct mtk_iommu_data *data,
				unsigned int larbid, unsigned int portid)
{
	unsigned int domain_id;
	struct mtk_iommu_domain *dom;

	domain_id = __mtk_iommu_get_domain_id(
				larbid, portid);
	if (domain_id == MTK_IOVA_DOMAIN_COUNT)
		return NULL;

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
	if (domain_id == MTK_IOVA_DOMAIN_COUNT)
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

static void mtk_iommu_tlb_flush_all(void *cookie)
{
	struct mtk_iommu_data *data, *data_tmp = cookie;

	list_for_each_entry(data, &m4ulist, list) {
		if (!single_pt)
			data = data_tmp;

		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
		       data->base + REG_MMU_INV_SEL);
		writel_relaxed(F_ALL_INVLD,
			data->base + REG_MMU_INVALIDATE);
		wmb(); /* Make sure the tlb flush all done */

		if (!single_pt)
			break;
	}
}

static void mtk_iommu_tlb_add_flush_nosync(unsigned long iova,
					   size_t size,
					   size_t granule, bool leaf,
					   void *cookie)
{
	struct mtk_iommu_data *data, *data_tmp = cookie;

	list_for_each_entry(data, &m4ulist, list) {
		if (!single_pt)
			data = data_tmp;

		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
		       data->base + REG_MMU_INV_SEL);

		writel_relaxed(iova, data->base + REG_MMU_INVLD_START_A);
		writel_relaxed(iova + size - 1,
			       data->base + REG_MMU_INVLD_END_A);
		writel_relaxed(F_MMU_INV_RANGE,
			       data->base + REG_MMU_INVALIDATE);
		data->tlb_flush_active = true;

		if (!single_pt)
			break;
	}
}

static void mtk_iommu_tlb_sync(void *cookie)
{
	struct mtk_iommu_data *data, *data_tmp = cookie;
	int ret;
	u32 tmp;

	list_for_each_entry(data, &m4ulist, list) {
		if (!single_pt)
			data = data_tmp;

		/* Avoid timing out if there's nothing to wait for */
		if (!data->tlb_flush_active)
			return;

		ret = readl_poll_timeout_atomic(data->base +
						REG_MMU_CPE_DONE,
						tmp, tmp != 0,
						10, 100000);
		if (ret) {
			dev_warn(data->dev,
				 "Partial TLB flush time out\n");
			mtk_iommu_tlb_flush_all(cookie);
		}
		/* Clear the CPE status */
		writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
		data->tlb_flush_active = false;

		if (!single_pt)
			break;
	}
}

static void mtk_iommu_iotlb_sync(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_sync(dom->data);
}

static const struct iommu_gather_ops mtk_iommu_gather_ops = {
	.tlb_flush_all = mtk_iommu_tlb_flush_all,
	.tlb_add_flush = mtk_iommu_tlb_add_flush_nosync,
	.tlb_sync = mtk_iommu_tlb_sync,
};

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova);

static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = dev_id;
	struct iommu_domain *domain;
	u32 int_state, regval, fault_iova, fault_pa;
	unsigned int fault_larb, fault_port, sub_comm = 0;
	bool layer, write;
	int slave_id = 0;
	phys_addr_t pa;
	unsigned int m4uid = data->plat_data->iommu_id;

	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr%d\n",
			__func__, __LINE__, data->base);
		return 0;
	}
	/* Read error info from registers */
	regval = readl_relaxed(data->base + REG_MMU_L2_FAULT_ST);
	int_state = readl_relaxed(data->base + REG_MMU_FAULT_ST1);

	if (int_state & (F_INT_MMU0_MAIN_MSK | F_INT_MMU0_MAU_MSK))
		slave_id = 0;
	else if (int_state & (F_INT_MMU1_MAIN_MSK | F_INT_MMU1_MAU_MSK))
		slave_id = 1;
	else {
		pr_info("m4u interrupt error: status = 0x%x\n", int_state);
		regval = readl_relaxed(data->base + REG_MMU_INT_CONTROL0);
		regval |= F_INT_CLR_BIT;
		writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);
		return 0;
	}

	pr_notice("iommu L2 int sta=0x%x, main sta=0x%x\n", regval, int_state);
	if (slave_id == 0) {
		fault_iova = readl_relaxed(data->base + REG_MMU0_FAULT_VA);
		fault_pa = readl_relaxed(data->base + REG_MMU0_INVLD_PA);
		regval = readl_relaxed(data->base + REG_MMU0_INT_ID);
	} else if (slave_id == 1) {
		fault_iova = readl_relaxed(data->base + REG_MMU1_FAULT_VA);
		fault_pa = readl_relaxed(data->base + REG_MMU1_INVLD_PA);
		regval = readl_relaxed(data->base + REG_MMU1_INT_ID);
	}
	layer = fault_iova & F_MMU_FAULT_VA_LAYER_BIT;
	write = fault_iova & F_MMU_FAULT_VA_WRITE_BIT;
	fault_iova &= F_MMU_FAULT_VA_MSK;

	/* for vpu iommu, its fault id is irregular, so be treated specially */
	if (data->plat_data->m4u_plat == M4U_MT6779 && m4uid) {
		fault_port = 1;
		fault_larb = 13;
		goto out;
	}

	fault_port = F_MMU0_INT_ID_PORT_ID(regval);
	fault_larb = F_MMU_INT_ID_COMM_ID(regval);
	sub_comm = F_MMU_INT_ID_SUB_COMM_ID(regval);
	if (fault_larb == 5)
		fault_larb = sub_comm ? 8 : 7;
	else
		fault_larb = data->plat_data->larbid_remap[m4uid][fault_larb];

out:
	domain = __mtk_iommu_get_domain(data,
				fault_larb, fault_port);
	pa = mtk_iommu_iova_to_phys(domain, fault_iova);
	pr_notice("iova=%x,pa=%x\n", fault_iova, (unsigned int)pa);

	if (report_iommu_fault(domain, data->dev, fault_iova,
			      write ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ)) {
		dev_err_ratelimited(
			data->dev,
			"iommu fault id:%u type=0x%x iova=0x%x pa=0x%x larb=%d port=%d layer=%d regval=0x%x %s\n",
			data->plat_data->iommu_id, int_state, fault_iova,
			fault_pa, fault_larb, fault_port,
			layer, regval, write ? "write" : "read");
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
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
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

int __mtk_iommu_get_pgtable_base_addr(
		struct mtk_iommu_pgtable *pgtable,
		unsigned int *pgd_pa)
{
	if (!pgtable) {
		pgtable = mtk_iommu_get_pgtable(NULL, 0);
	}

	if (!pgtable) {
		pr_notice("%s, %d, cannot find pgtable\n",
			__func__, __LINE__);
		return -1;
	}
	*pgd_pa = pgtable->cfg.arm_v7s_cfg.ttbr[0] & MMU_PT_ADDR_MASK;

	return 0;
}


/* For secure driver */
int mtk_iommu_get_pgtable_base_addr(unsigned int *pgd_pa)
{
	return __mtk_iommu_get_pgtable_base_addr(NULL, pgd_pa);
}

static int mtk_iommu_create_pgtable(struct mtk_iommu_data *data)
{
	struct mtk_iommu_pgtable *pgtable =
				mtk_iommu_get_pgtable(data, init_data_id);

	if (pgtable)
		return 0;

	pgtable = kzalloc(sizeof(*pgtable), GFP_KERNEL);
	if (!pgtable)
		return -ENOMEM;

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

	if (data->dram_is_4gb)
		pgtable->cfg.quirks |= IO_PGTABLE_QUIRK_ARM_MTK_4GB;

	pgtable->iop = alloc_io_pgtable_ops(ARM_V7S, &pgtable->cfg, data);
	if (!pgtable->iop) {
		dev_err(data->dev, "Failed to alloc io pgtable\n");
		return -EINVAL;
	}

	if (mtk_iommu_set_pgtable(data, init_data_id, pgtable))
		return -EFAULT;
	else
		pr_notice("%s, %d, create pgtable done\n",
			    __func__, __LINE__);

	return 0;
}

static int mtk_iommu_attach_pgtable(struct mtk_iommu_data *data,
			struct device *dev)
{
	struct mtk_iommu_pgtable *pgtable =
				mtk_iommu_get_pgtable(data, init_data_id);
	unsigned int regval = 0, ret;
	unsigned int pgd_pa_reg = 0;

	// create pgtable
	if (!pgtable) {
		ret = mtk_iommu_create_pgtable(data);
		if (ret) {
			pr_notice("%s, %d, failed to create pgtable, err %d\n",
				    __func__, __LINE__, ret);
			return ret;
		}
		pgtable = mtk_iommu_get_pgtable(data, init_data_id);
	}

	// binding to pgtable
	data->pgtable = pgtable;

	// update HW settings
	if (__mtk_iommu_get_pgtable_base_addr(pgtable, &pgd_pa_reg))
		return -EFAULT;
	writel(pgd_pa_reg, data->base + REG_MMU_PT_BASE_ADDR);
	regval = readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR);
	pr_notice("%s, %d, m4u%d pgtable base addr=0x%x, quiks=0x%lx\n",
		__func__, __LINE__, data->plat_data->iommu_id,
		regval, pgtable->cfg.quirks);

	return 0;
}

#ifdef CONFIG_ARM64
static struct iommu_domain *mtk_iommu_domain_alloc(unsigned int type)
{
	struct mtk_iommu_domain *dom;
	struct mtk_iommu_pgtable *pgtable =
				mtk_iommu_get_pgtable(NULL, init_data_id);
	unsigned int id = pgtable->init_domain_id;
	// allocated at device_group for IOVA  space management by iovad
	unsigned int domain_type = IOMMU_DOMAIN_DMA;

	if (type != domain_type) {
		pr_notice("%s, %d, err type%d\n",
			__func__, __LINE__, type);
		return NULL;
	}

	list_for_each_entry(dom, &pgtable->m4u_dom_v2, list) {
		if (dom->id == id)
			return &dom->domain;
	}

	return NULL;
}
#else
static struct iommu_domain *mtk_iommu_domain_alloc(unsigned int type)
{
	struct mtk_iommu_domain *dom;

	if (type != IOMMU_DOMAIN_UNMANAGED) {
		pr_notice("%s, %d, err type%d\n",
			__func__, __LINE__, type);
		return NULL;
	}

	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	return &dom->domain;
}
#endif

static void mtk_iommu_domain_free(struct iommu_domain *domain)
{
	unsigned long flags;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;

	pr_notice("%s, %d, domain_count=%d, free the %d domain\n",
		    __func__, __LINE__, pgtable->domain_count, dom->id);

#ifdef CONFIG_ARM64
	iommu_put_dma_cookie(domain);
#endif
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
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;

	if (!data)
		return -ENODEV;

	mtk_iommu_config(data, dev, true);

	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;

	if (!data)
		return;

	mtk_iommu_config(data, dev, false);
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	int ret;

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

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	phys_addr_t pa;

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	pa = pgtable->iop->iova_to_phys(pgtable->iop, iova);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	return pa;
}

#ifdef CONFIG_ARM64
static int mtk_iommu_add_device(struct device *dev)
{
	struct mtk_iommu_data *data;
	struct iommu_group *group;

	if (!dev->iommu_fwspec ||
	    dev->iommu_fwspec->ops != &mtk_iommu_ops) {
		return -ENODEV;
	}

	data = dev->iommu_fwspec->iommu_priv;
	iommu_device_link(&data->iommu, dev);

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
	struct mtk_iommu_data *data;
	struct platform_device *m4updev;
	struct dma_iommu_mapping *mtk_mapping;
	struct device *m4udev;
	int ret;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!dev->iommu_fwspec) {
		ret = iommu_fwspec_init(dev, &args->np->fwnode, &mtk_iommu_ops);
		if (ret) {
			pr_notice("%s, %d, fwspec init failed, ret=%d\n",
				__func__, __LINE__, ret);
			return ret;
		}
	} else if (dev->iommu_fwspec->ops != &mtk_iommu_ops) {
		return -EINVAL;
	}

	if (!dev->iommu_fwspec->iommu_priv) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		if (WARN_ON(!m4updev))
			return -EINVAL;

		dev->iommu_fwspec->iommu_priv = platform_get_drvdata(m4updev);
	}

	ret = iommu_fwspec_add_ids(dev, args->args, 1);
	if (ret)
		return ret;

	data = dev->iommu_fwspec->iommu_priv;
	m4udev = data->dev;
	mtk_mapping = m4udev->archdata.iommu;
	if (!mtk_mapping) {
		/* MTK iommu support 4GB iova address space. */
		mtk_mapping = arm_iommu_create_mapping(&platform_bus_type,
						0, 1ULL << 32);
		if (IS_ERR(mtk_mapping)) {
			pr_noitce("%s, %d, err mapping\n",
				__func__, __LINE__);
			return PTR_ERR(mtk_mapping);
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
	return ret;
}

static int mtk_iommu_add_device(struct device *dev)
{
	struct of_phandle_args iommu_spec;
	struct mtk_iommu_data *data;
	struct iommu_group *group;
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

	if (!dev->iommu_fwspec || dev->iommu_fwspec->ops != &mtk_iommu_ops)
		return -ENODEV; /* Not a iommu client device */

	data = dev->iommu_fwspec->iommu_priv;
	iommu_device_link(&data->iommu, dev);

	group = iommu_group_get_for_dev(dev);
	if (IS_ERR(group))
		return PTR_ERR(group);

	iommu_group_put(group);
	return 0;
}

#endif
static void mtk_iommu_remove_device(struct device *dev)
{
	struct mtk_iommu_data *data;

	if (!dev->iommu_fwspec || dev->iommu_fwspec->ops != &mtk_iommu_ops)
		return;

	data = dev->iommu_fwspec->iommu_priv;
	iommu_device_unlink(&data->iommu, dev);

	iommu_group_remove_device(dev);
	iommu_fwspec_free(dev);
}

static struct iommu_group *mtk_iommu_create_iova_space(
			struct mtk_iommu_data *data, struct device *dev)
{
	struct mtk_iommu_pgtable *pgtable =
				mtk_iommu_get_pgtable(data, init_data_id);
	struct mtk_iommu_domain *dom;
	struct iommu_group *group;
	unsigned long flags;

	group = mtk_iommu_get_group(dev);

	if (group) {
		iommu_group_ref_get(group);
		return group;
	}

	// init mtk_iommu_domain
	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	// init iommu_group
	group = iommu_group_alloc();
	if (IS_ERR(group)) {
		dev_notice(dev, "Failed to allocate M4U IOMMU group\n");
		goto free_dom;
	}
	dom->group = group;

	dom->id = mtk_iommu_get_domain_id(dev);

	spin_lock_irqsave(&pgtable->domain_lock, flags);
	if (pgtable->domain_count >= MTK_IOVA_DOMAIN_COUNT) {
		spin_unlock_irqrestore(&pgtable->domain_lock, flags);
		pr_notice("%s, %d, too many domain, count=%d\n",
			__func__, __LINE__, pgtable->domain_count);
		return NULL;
	}
	pgtable->init_domain_id = dom->id;
	pgtable->domain_count++;
	spin_unlock_irqrestore(&pgtable->domain_lock, flags);

	dom->domain.pgsize_bitmap = pgtable->cfg.pgsize_bitmap;
	dom->pgtable = pgtable;
	dom->data = data;
	list_add_tail(&dom->list, &pgtable->m4u_dom_v2);

	// init mtk_iommu_domain
	if (iommu_get_dma_cookie(&dom->domain))
		goto free_group;

	dom->domain.geometry.aperture_start =
				mtk_domain_array[dom->id].min_iova;
	dom->domain.geometry.aperture_end =
				mtk_domain_array[dom->id].max_iova;
	dom->domain.geometry.force_aperture = true;

	pr_notice("%s, %d, dev:%s allocated the %d group:%p, domain:%p start:0x%x, end:0x%x\n",
		    __func__, __LINE__, dev_name(dev),
		    dom->id, group, &dom->domain,
		    dom->domain.geometry.aperture_start,
		    dom->domain.geometry.aperture_end);

	return group;

#ifdef CONFIG_ARM64
free_group:
	kfree(group);
#endif
free_dom:
	kfree(dom);
	return NULL;
}

static struct iommu_group *mtk_iommu_device_group(struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	struct mtk_iommu_pgtable *pgtable;
	int ret = 0;

	if (!data)
		return NULL;

	init_data_id = data->plat_data->iommu_id;
	pgtable = data->pgtable;
	if (!pgtable) {
		ret = mtk_iommu_attach_pgtable(data, dev);
		if (ret) {
			data->pgtable = NULL;
			return NULL;
		}
	}

	pr_notice("%s, %d, init data:%d\n", __func__, __LINE__, init_data_id);
	return mtk_iommu_create_iova_space(data, dev);
}

#ifdef CONFIG_ARM64
static int mtk_iommu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct platform_device *m4updev;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!dev->iommu_fwspec->iommu_priv) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		of_node_put(args->np);
		if (WARN_ON(!m4updev))
			return -EINVAL;

		dev->iommu_fwspec->iommu_priv = platform_get_drvdata(m4updev);
	}

	return iommu_fwspec_add_ids(dev, args->args, 1);
}
#endif

static void mtk_iommu_get_resv_region(
					struct device *dev,
					struct list_head *list)
{
	struct iommu_resv_region *region;
	const struct mtk_iova_domain_data *dom_data;
	unsigned int id;
	int i;
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;

	if (!data->plat_data->has_resv_region)
		return;

	id = mtk_iommu_get_domain_id(dev);
	if (WARN_ON(id >= MTK_IOVA_DOMAIN_COUNT))
		return;

	dom_data = &mtk_domain_array[id];
	switch (dom_data->resv_type) {
	case IOVA_REGION_REMOVE:
		for (i = 0; i < MTK_IOVA_REMOVE_CNT; i++) {
			if (!dom_data->resv_start[i] ||
				!dom_data->resv_size[i])
				continue;

			region = iommu_alloc_resv_region(
				dom_data->resv_start[i],
				dom_data->resv_size[i],
				0, IOMMU_RESV_RESERVED);
			if (!region) {
				pr_notice("Out of memory allocating dm-regions for %s\n",
					  dev_name(dev));
				return;
			}
			list_add_tail(&region->list, list);
		}
		break;
	default:
		break;
	}
}

static void mtk_iommu_put_resv_region(
					struct device *dev,
					struct list_head *list)
{
	struct  iommu_resv_region *region, *tmp;
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;

	if (!data->plat_data->has_resv_region)
		return;

	list_for_each_entry_safe(region, tmp, list, list)
		kfree(region);
}

static struct iommu_ops mtk_iommu_ops = {
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
	.pgsize_bitmap	= SZ_4K | SZ_64K | SZ_1M | SZ_16M,
	.get_resv_regions	= mtk_iommu_get_resv_region,
	.put_resv_regions	= mtk_iommu_put_resv_region,
};

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data)
{
	u32 regval;

	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr%d\n",
			__func__, __LINE__, data->base);
		return -1;
	}

	regval = readl_relaxed(data->base + REG_MMU_CTRL_REG);
	regval = regval | F_MMU_CTRL_PFH_DIS
		 | F_MMU_CTRL_MONITOR_EN
		 | F_MMU_CTRL_MONITOR_CLR
		 | F_MMU_CTRL_INT_HANG_EN;
	writel_relaxed(regval, data->base + REG_MMU_CTRL_REG);
	/* enable all interrupts */
	writel_relaxed(0x6f, data->base + REG_MMU_INT_CONTROL0);
	writel_relaxed(0xffffffff, data->base + REG_MMU_INT_MAIN_CONTROL);
	/* set translation fault proctection buffer address */
	writel_relaxed(F_MMU_IVRP_PA_SET(data->protect_base, data->dram_is_4gb),
		       data->base + REG_MMU_IVRP_PADDR);
	/* enable DCM */
	writel_relaxed(0, data->base + REG_MMU_DCM_DIS);
	/* write command throttling mode */
	regval = readl_relaxed(data->base + REG_MMU_WR_LEN);
	regval = regval & (~F_MMU_WR_THROT_DIS);
	writel_relaxed(regval, data->base + REG_MMU_WR_LEN);

	/* special settings for mmu0 (multimedia iommu) */
	if (data->plat_data->iommu_id == 0) {
		regval = readl_relaxed(data->base + REG_MMU_MISC_CRTL);
		/* non-standard AXI mode */
		regval = regval & (~REG_MMU_STANDARD_AXI_MODE);
		/* disable in-order-write */
		regval = regval & (~REG_MMU_IN_ORDER_WR_EN);
		writel_relaxed(regval, data->base + REG_MMU_MISC_CRTL);
	}

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
	struct mtk_iommu_data   *data;
	struct device           *dev = &pdev->dev;
	struct resource         *res;
	resource_size_t		ioaddr;
	struct component_match  *match = NULL;
	void                    *protect;
	int                     i, larb_nr, ret;

	pr_notice("%s start, %d,+\n",
		__func__, __LINE__);
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;
	data->plat_data = of_device_get_match_data(dev);

	single_pt = data->plat_data->single_pt;
	/* Protect memory. HW will access here while translation fault.*/
	protect = devm_kzalloc(dev, MTK_PROTECT_PA_ALIGN * 2, GFP_KERNEL);
	if (!protect)
		return -ENOMEM;
	data->protect_base = ALIGN(virt_to_phys(protect),
				   MTK_PROTECT_PA_ALIGN);

	/* Whether the current dram is over 4GB */
	data->dram_is_4gb = !!(max_pfn > (BIT_ULL(32) >> PAGE_SHIFT));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base)) {
		pr_notice("mtk_iommu base is null\n");
		return PTR_ERR(data->base);
	}
	pr_notice("%s, %d, base=0x%lx, protect_base=0x%lx\n",
		__func__, __LINE__, data->base, data->protect_base);
	ioaddr = res->start;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		pr_notice("mtk_iommu irq error\n");
		return data->irq;
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
	if (single_pt)
		pr_notice("%s, %d, add m4ulist, use share pgtable\n",
			__func__, __LINE__);
	else
		pr_notice("%s, %d, add m4ulist, use private pgtable\n",
			__func__, __LINE__);

	total_iommu_cnt++;
	/*
	 * trigger the bus to scan all the device to add them to iommu
	 * domain after all the iommu have finished probe.
	 */
	if (!iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);

	pr_notice("%s done, %d, iommu_id=%u\n",
		__func__, __LINE__, data->plat_data->iommu_id);

	return component_master_add_with_match(dev, &mtk_iommu_com_ops, match);
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct mtk_iommu_data *data = platform_get_drvdata(pdev);

	pr_notice("%s, %d\n",
		__func__, __LINE__);
	iommu_device_sysfs_remove(&data->iommu);
	iommu_device_unregister(&data->iommu);

	if (iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, NULL);

	devm_free_irq(&pdev->dev, data->irq, data);
	component_master_del(&pdev->dev, &mtk_iommu_com_ops);
	return 0;
}

static void mtk_iommu_shutdown(struct platform_device *pdev)
{
	pr_notice("%s, %d\n",
		__func__, __LINE__);
	mtk_iommu_remove(pdev);
}

static int mtk_iommu_suspend(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;

	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr%d\n",
			__func__, __LINE__, data->base);
		return -1;
	}
	reg->standard_axi_mode = readl_relaxed(base +
					       REG_MMU_MISC_CRTL);
	reg->dcm_dis = readl_relaxed(base + REG_MMU_DCM_DIS);
	reg->ctrl_reg = readl_relaxed(base + REG_MMU_CTRL_REG);
	reg->int_control0 = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	reg->int_main_control = readl_relaxed(base + REG_MMU_INT_MAIN_CONTROL);
	reg->wr_len = readl_relaxed(base + REG_MMU_WR_LEN);
	return 0;
}

static int mtk_iommu_resume(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;
	unsigned int pgd_pa_reg = 0;

	if (!data->base  || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr%d\n",
			__func__, __LINE__, data->base);
		return -1;
	}
	if (__mtk_iommu_get_pgtable_base_addr(data->pgtable, &pgd_pa_reg))
		return -EFAULT;
	writel(pgd_pa_reg, base + REG_MMU_PT_BASE_ADDR);
	writel_relaxed(pgd_pa_reg, base + REG_MMU_PT_BASE_ADDR);
	writel_relaxed(reg->standard_axi_mode,
		       base + REG_MMU_MISC_CRTL);
	writel_relaxed(reg->dcm_dis, base + REG_MMU_DCM_DIS);
	writel_relaxed(reg->ctrl_reg, base + REG_MMU_CTRL_REG);
	writel_relaxed(reg->int_control0, base + REG_MMU_INT_CONTROL0);
	writel_relaxed(reg->int_main_control, base + REG_MMU_INT_MAIN_CONTROL);
	writel_relaxed(F_MMU_IVRP_PA_SET(data->protect_base, data->dram_is_4gb),
		       base + REG_MMU_IVRP_PADDR);
	writel_relaxed(reg->wr_len, base + REG_MMU_WR_LEN);
	return 0;
}

static const struct dev_pm_ops mtk_iommu_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_iommu_suspend, mtk_iommu_resume)
};

const struct mtk_iommu_plat_data mt6779_data_mm = {
	.m4u_plat = M4U_MT6779,
	.larbid_remap[0] = {0, 1, 2, 3, 5, 7, 10, 9},
	.single_pt = true,
	.has_resv_region = true,
	.iommu_id = 0,
};

const struct mtk_iommu_plat_data mt6779_data_vpu = {
	.m4u_plat = M4U_MT6779,
	.single_pt = true,
	.has_resv_region = true,
	.iommu_id = 1,
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,mt6779-m4u-mm", .data = &mt6779_data_mm},
	{ .compatible = "mediatek,mt6779-m4u-vpu", .data = &mt6779_data_vpu},
	{}
};

static struct platform_driver mtk_iommu_driver = {
	.probe	= mtk_iommu_probe,
	.remove	= mtk_iommu_remove,
	.shutdown = mtk_iommu_shutdown,
	.driver	= {
		.name = "mtk-iommu-v2",
		.of_match_table = of_match_ptr(mtk_iommu_of_ids),
		.pm = &mtk_iommu_pm_ops,
	}
};

static int __init mtk_iommu_init(void)
{
	int ret;

	pr_notice("%s, %d\n",
		__func__, __LINE__);

	ret = platform_driver_register(&mtk_iommu_driver);
	if (ret != 0) {
		pr_notice("Failed to register MTK IOMMU driver\n");
		return ret;
	}

	return ret;
}

subsys_initcall(mtk_iommu_init);
