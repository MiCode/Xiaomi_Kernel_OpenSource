// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 */
#define pr_fmt(fmt)    "mtk_iommu: " fmt

#include <linux/bitfield.h>
#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/dma-direct.h>
#include <linux/dma-iommu.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>  /* only for debug func. */
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/soc/mediatek/infracfg.h>
#include <asm/barrier.h>
#include <soc/mediatek/smi.h>
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
#include <../misc/mediatek/iommu/iommu_debug.h>
#endif
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
#include <../misc/mediatek/iommu/iommu_secure.h>
#endif

#include "mtk_iommu.h"

#define REG_MMU_PT_BASE_ADDR			0x000
#define MMU_PT_ADDR_MASK			GENMASK(31, 7)

#define REG_MMU_INVALIDATE			0x020
#define F_ALL_INVLD				0x2
#define F_MMU_INV_RANGE				0x1

#define REG_MMU_INVLD_START_A			0x024
#define REG_MMU_INVLD_END_A			0x028

#define REG_MMU_INV_SEL_GEN2			0x02c
#define REG_MMU_INV_SEL_GEN1			0x038
#define F_INVLD_EN0				BIT(0)
#define F_INVLD_EN1				BIT(1)

#define REG_MMU_MISC_CTRL			0x048
#define F_MMU_IN_ORDER_WR_EN_MASK		(BIT(1) | BIT(17))
#define F_MMU_STANDARD_AXI_MODE_MASK		(BIT(3) | BIT(19))

#define REG_MMU_DCM_DIS				0x050
#define REG_MMU_WR_LEN_CTRL			0x054
#define F_MMU_WR_THROT_DIS_MASK			(BIT(5) | BIT(21))

#define REG_MMU_TBW_ID				0xa0

#define REG_MMU_CTRL_REG			0x110
#define F_MMU_SYNC_INVLDT_EN			BIT(3)
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
#define F_TLB_INVALID_DONE_EN			BIT(4)
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
#define F_MMU_INVAL_VA_31_12_MASK		GENMASK(31, 12)
#define F_MMU_INVAL_VA_34_32_MASK		GENMASK(11, 9)
#define F_MMU_INVAL_PA_34_32_MASK		GENMASK(8, 6)
#define F_MMU_FAULT_VA_WRITE_BIT		BIT(1)
#define F_MMU_FAULT_VA_LAYER_BIT		BIT(0)

#define REG_MMU0_INVLD_PA			0x140
#define REG_MMU1_FAULT_VA			0x144
#define REG_MMU1_INVLD_PA			0x148
#define REG_MMU0_INT_ID				0x150
#define REG_MMU1_INT_ID				0x154
#define F_MMU_INT_ID_COMM_ID(a)			(((a) >> 9) & 0xf)
#define F_MMU_INT_ID_SUB_COMM_ID(a)		(((a) >> 7) & 0x3)
#define F_MMU_INT_ID_LARB_ID(a)			(((a) >> 7) & 0x7)
#define F_MMU_INT_ID_PORT_ID(a)			(((a) >> 2) & 0x1f)

#define MTK_PROTECT_PA_ALIGN			256

#define HAS_4GB_MODE			BIT(0)
/* HW will use the EMI clock if there isn't the "bclk". */
#define HAS_BCLK			BIT(1)
#define HAS_VLD_PA_RNG			BIT(2)
#define RESET_AXI			BIT(3)
#define OUT_ORDER_WR_EN			BIT(4)
#define HAS_SUB_COMM			BIT(5)
#define WR_THROT_EN			BIT(6)
#define HAS_LEGACY_IVRP_PADDR		BIT(7)
#define IOVA_34_EN			BIT(8)
#define NOT_STD_AXI_MODE		BIT(9)
#define SET_TBW_ID			BIT(10)
#define LINK_WITH_APU			BIT(11)
#define TLB_SYNC_EN			BIT(12)
#define IOMMU_BK_EN			BIT(13)
#define SKIP_CFG_PORT			BIT(14)
/* For IOMMU EP/bring up phase: CLK AO */
#define IOMMU_CLK_AO_EN			BIT(15)
/* For IOMMU EP/bring up phase: smi not ready */
#define IOMMU_EN_PRE			BIT(16)
/* Debug: Skip register IRQ */
#define IOMMU_NO_IRQ			BIT(17)
#define GET_DOM_ID_LEGACY		BIT(18)

#define MTK_IOMMU_HAS_FLAG(pdata, _x) \
		((((pdata)->flags) & (_x)) == (_x))

#define MTK_IOMMU_ISR_COUNT_MAX			5
#define MTK_IOMMU_ISR_DISABLE_TIME		10

struct mtk_iommu_domain {
	struct io_pgtable_cfg		cfg;
	struct io_pgtable_ops		*iop;

	struct mtk_iommu_data		*data;
	struct iommu_domain		domain;
};

static const struct iommu_ops mtk_iommu_ops;

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data);

#define MTK_IOMMU_TLB_ADDR(iova) ({					\
	dma_addr_t _addr = iova;					\
	((lower_32_bits(_addr) & GENMASK(31, 12)) | upper_32_bits(_addr));\
})

/*
 * In M4U 4GB mode, the physical address is remapped as below:
 *
 * CPU Physical address:
 * ====================
 *
 * 0      1G       2G     3G       4G     5G
 * |---A---|---B---|---C---|---D---|---E---|
 * +--I/O--+------------Memory-------------+
 *
 * IOMMU output physical address:
 *  =============================
 *
 *                                 4G      5G     6G      7G      8G
 *                                 |---E---|---B---|---C---|---D---|
 *                                 +------------Memory-------------+
 *
 * The Region 'A'(I/O) can NOT be mapped by M4U; For Region 'B'/'C'/'D', the
 * bit32 of the CPU physical address always is needed to set, and for Region
 * 'E', the CPU physical address keep as is.
 * Additionally, The iommu consumers always use the CPU phyiscal address.
 */
#define MTK_IOMMU_4GB_MODE_REMAP_BASE	 0x140000000UL

static LIST_HEAD(m4ulist);	/* List all the M4U HWs */

#define for_each_m4u(data)	list_for_each_entry(data, &m4ulist, list)

struct mtk_iommu_iova_region {
	dma_addr_t		iova_base;
	unsigned long long	size;
};

static const struct mtk_iommu_iova_region single_domain[] = {
	{.iova_base = 0,		.size = SZ_4G},
};

static const struct mtk_iommu_iova_region single_domain_ext[] = {
	{.iova_base = 0, .size = SZ_4G * 4},
};

static const struct mtk_iommu_iova_region mt8192_multi_dom[] = {
	{ .iova_base = 0x0,		.size = SZ_4G},		/* disp: 0 ~ 4G */
	#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
	{ .iova_base = SZ_4G,		.size = SZ_4G},		/* vdec: 4G ~ 8G */
	{ .iova_base = SZ_4G * 2,	.size = SZ_4G},		/* CAM/MDP: 8G ~ 12G */
	{ .iova_base = 0x240000000ULL,	.size = 0x4000000},	/* CCU0 */
	{ .iova_base = 0x244000000ULL,	.size = 0x4000000},	/* CCU1 */
	#endif
};

static const struct mtk_iommu_iova_region mt6873_multi_dom[] = {
	{ .iova_base = 0x0, .size = SZ_4G},	      /* disp : 0 ~ 4G */
	{ .iova_base = SZ_4G, .size = SZ_4G},     /* vdec : 4G ~ 8G */
	{ .iova_base = SZ_4G * 2, .size = SZ_4G}, /* CAM/MDP: 8G ~ 12G */
	{ .iova_base = 0x240000000ULL, .size = 0x4000000}, /* CCU0 */
	{ .iova_base = 0x244000000ULL, .size = 0x4000000}, /* CCU1 */
	{ .iova_base = SZ_4G * 3, .size = SZ_4G}, /* APU DATA */
	{ .iova_base = 0x304000000ULL, .size = 0x4000000}, /* APU VLM */
	{ .iova_base = 0x310000000ULL, .size = 0x10000000}, /* APU VPU */
	{ .iova_base = 0x370000000ULL, .size = 0x12600000}, /* APU REG */
};

/*
 * 0,NORMAL: 0x2000_0000~0x3FFF_FFFF & 0x1_0800_0000~0x1_0FFF_FFFF & 0x1_7000_0000~0x3_FFFF_FFFF
 * 1,APU_SECURE:     0x1000~0x1FFF_FFFF
 * 2,APU_CODE:       0x4000_0000~0xFFFF_FFFF
 * 3,CCU0:           0x1_0000_0000~0x1_03FF_FFFF
 * 4,CCU1:           0x1_0400_0000~0x1_07FF_FFFF
 * 5,VDO_UP_512MB_1: 0x1_1000_0000~0x1_2FFF_FFFF
 * 6,VDO_UP_512MB_2: 0x1_3000_0000~0x1_4FFF_FFFF
 * 7,VDO_UP_256MB_1: 0x1_5000_0000~0x1_5FFF_FFFF
 * 8,VDO_UP_256MB_1: 0x1_6000_0000~0x1_6FFF_FFFF
 */
static const struct mtk_iommu_iova_region mt6983_multi_dom[] = {
	{ .iova_base = SZ_4K, .size = (SZ_4G * 4 - SZ_4K)}, /* 0, NORMAL:512MB+128MB+10.25GB */
	{ .iova_base = SZ_4K, .size = (SZ_512M - SZ_4K)}, /* 1,APU_SECURE:512M */
	{ .iova_base = SZ_1G, .size = 0xc0000000}, /* 2,APU_CODE:3GB */
	{ .iova_base = 0x100000000ULL, .size = 0x4000000}, /* 3,CCU0:64MB */
	{ .iova_base = 0x104000000ULL, .size = 0x4000000}, /* 4,CCU1:64MB */
	{ .iova_base = 0x110000000ULL, .size = SZ_512M}, /* 5,VDO_UP_512MB_1 */
	{ .iova_base = 0x130000000ULL, .size = SZ_512M}, /* 6,VDO_UP_512MB_2 */
	{ .iova_base = 0x150000000ULL, .size = SZ_256M}, /* 7,VDO_UP_256MB_1 */
	{ .iova_base = 0x160000000ULL, .size = SZ_256M}, /* 8,VDO_UP_256MB_1 */
};

static const struct mtk_iommu_iova_region mt6983_multi_dom_test[] = {
	{ .iova_base = SZ_4K, .size = (SZ_4G * 2 - SZ_4K)}, /* 0,NORMAL: 0x1000~0x1_FFFF_FFFF */
	{ .iova_base = SZ_4K, .size = (SZ_512M - SZ_4K)}, /* 1,APU_SECURE:0x1000~0x1FFF_FFFF */
	{ .iova_base = SZ_1G, .size = 0xc0000000}, /* 2,APU_CODE:0x4000_0000~0xFFFF_FFFF */
	{ .iova_base = 0x100000000ULL, .size = SZ_1G}, /* 3,CCU0:1_0000_0000~0x1_3FFF_FFFF */
	{ .iova_base = 0x140000000ULL, .size = SZ_1G}, /* 4,CCU1:1_4000_0000~0x1_7FFF_FFFF */
	{ .iova_base = 0x180000000ULL, .size = SZ_1G}, /* 5,VDO_UP_512MB_1:1_8000_0000~0x1_BFFF_FFFF */
	{ .iova_base = 0x1C0000000ULL, .size = SZ_512M}, /* 6,VDO_UP_512MB_2: 1_C000_0000~0x1_DFFF_FFFF*/
	{ .iova_base = 0x1E0000000ULL, .size = SZ_256M}, /* 7,VDO_UP_256MB_1: 1_E000_0000~0x1_EFFF_FFFF */
	{ .iova_base = 0x1F0000000ULL, .size = (SZ_128M + 0x7400000)}, /* 8,VDO_UP_256MB_1: 1_F000_0000~0x1_FF3F_FFFF */
};

/*
 * There may be 1 or 2 M4U HWs, But we always expect they are in the same domain
 * for the performance.
 *
 * Here always return the mtk_iommu_data of the first probed M4U where the
 * iommu domain information is recorded.
 */
static struct mtk_iommu_data *mtk_iommu_get_m4u_data(void)
{
	struct mtk_iommu_data *data;

	for_each_m4u(data)
		return data;

	return NULL;
}

static struct mtk_iommu_domain *to_mtk_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct mtk_iommu_domain, domain);
}

static inline void mtk_iommu_isr_setup(unsigned long enable)
{
	struct mtk_iommu_data *data;
	u32 regval;
	int i;

	pr_info("%s, enable:%d\n", __func__, enable);
	for_each_m4u(data) {
		for (i = IOMMU_BK0; i < IOMMU_BK_NUM; i++) {
			void __iomem *base = NULL;

			if (i == IOMMU_BK0) {
				base = data->base;
			} else {
				if (!MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_BK_EN))
					break;
				base = data->bk_base[i];
			}

			if (!base || IS_ERR(base)) {
				pr_info("%s, invalid base addr, dev:%s, (%d,%d,%d)\n",
					__func__, dev_name(data->dev),
					data->plat_data->iommu_type,
					data->plat_data->iommu_id, i);
				break;
			}

			if (enable) {
				regval = F_L2_MULIT_HIT_EN |
						F_TABLE_WALK_FAULT_INT_EN |
						F_PREETCH_FIFO_OVERFLOW_INT_EN |
						F_MISS_FIFO_OVERFLOW_INT_EN |
						F_PREFETCH_FIFO_ERR_INT_EN |
						F_MISS_FIFO_ERR_INT_EN;
				writel_relaxed(regval, base + REG_MMU_INT_CONTROL0);

				regval = F_INT_TRANSLATION_FAULT |
						F_INT_MAIN_MULTI_HIT_FAULT |
						F_INT_INVALID_PA_FAULT |
						F_INT_ENTRY_REPLACEMENT_FAULT |
						F_INT_TLB_MISS_FAULT |
						F_INT_MISS_TRANSACTION_FIFO_FAULT |
						F_INT_PRETETCH_TRANSATION_FIFO_FAULT;
				writel_relaxed(regval, base + REG_MMU_INT_MAIN_CONTROL);
			} else {
				writel_relaxed(0, base + REG_MMU_INT_CONTROL0);
				writel_relaxed(0, base + REG_MMU_INT_MAIN_CONTROL);
			}
		}
	}
}

static void mtk_iommu_isr_restart(struct timer_list *t)
{
	mtk_iommu_isr_setup(1);
}

static int mtk_iommu_isr_pause_timer_init(struct mtk_iommu_data *data)
{
	timer_setup(&data->iommu_isr_pause_timer, mtk_iommu_isr_restart, 0);
	return 0;
}

static int mtk_iommu_isr_pause(struct mtk_iommu_data *data, int delay)
{
	if (!timer_pending(&data->iommu_isr_pause_timer)) {
		/* disable all intr */
		mtk_iommu_isr_setup(0);
		/* delay seconds */
		data->iommu_isr_pause_timer.expires = jiffies + delay * HZ;
		add_timer(&data->iommu_isr_pause_timer);
	}
	return 0;
}

static void mtk_iommu_isr_record(struct mtk_iommu_data *data)
{
	static int isr_cnt;
	static unsigned long first_jiffies;

	/* we allow one irq in 1s, or we will disable them after isr_cnt s. */
	if (!isr_cnt || time_after(jiffies, first_jiffies + isr_cnt * HZ)) {
		isr_cnt = 1;
		first_jiffies = jiffies;
	} else {
		isr_cnt++;
		if (isr_cnt >= MTK_IOMMU_ISR_COUNT_MAX) {
			/* irq too many! disable irq for a while, to avoid HWT timeout*/
			mtk_iommu_isr_pause(data, MTK_IOMMU_ISR_DISABLE_TIME);
			isr_cnt = 0;
		}
	}
}

static void mtk_iommu_tlb_flush_all(struct mtk_iommu_data *data)
{
	for_each_m4u(data) {
		if (pm_runtime_get_if_in_use(data->dev) <= 0 && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN))
			continue;

		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + data->plat_data->inv_sel_reg);
		writel_relaxed(F_ALL_INVLD, data->base + REG_MMU_INVALIDATE);
		wmb(); /* Make sure the tlb flush all done */

		if (!MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN))
			pm_runtime_put(data->dev);
	}
}

static void mtk_iommu_tlb_flush_range_sync(unsigned long iova, size_t size,
					   size_t granule,
					   struct mtk_iommu_data *data)
{
	bool has_pm = !!data->dev->pm_domain;
	unsigned long flags;
	int ret;
	u32 tmp;

	for_each_m4u(data) {
		if (has_pm) {
			if (pm_runtime_get_if_in_use(data->dev) <= 0 && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN))
				continue;
		}

		spin_lock_irqsave(&data->tlb_lock, flags);
		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + data->plat_data->inv_sel_reg);

		writel_relaxed(MTK_IOMMU_TLB_ADDR(iova),
			       data->base + REG_MMU_INVLD_START_A);
		writel_relaxed(MTK_IOMMU_TLB_ADDR(iova + size - 1),
			       data->base + REG_MMU_INVLD_END_A);
		writel_relaxed(F_MMU_INV_RANGE,
			       data->base + REG_MMU_INVALIDATE);

		/* tlb sync */
		ret = readl_poll_timeout_atomic(data->base + REG_MMU_CPE_DONE,
						tmp, tmp != 0, 10, 1000);
		if (ret) {
			dev_warn(data->dev,
				 "Partial TLB flush timed out, falling back to full flush\n");
			pr_info("[iommu_debug] dump info 0x0:0x%x, 0x24:0x%x, 0x28:0x%x, 0x120:0x%x, 0x124:0x%x, iova:0x%lx\n",
				readl_relaxed(data->base + REG_MMU_INVLD_START_A),
				readl_relaxed(data->base + REG_MMU_INVLD_END_A),
				readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR),
				readl_relaxed(data->base + REG_MMU_INT_CONTROL0),
				readl_relaxed(data->base + REG_MMU_INT_MAIN_CONTROL),
				iova);
			mtk_iommu_tlb_flush_all(data);
		}
		/* Clear the CPE status */
		writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
		spin_unlock_irqrestore(&data->tlb_lock, flags);

		if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN))
			pm_runtime_put(data->dev);
	}
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova);
static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = dev_id;
	void __iomem *base = data->base; /* bank0 base */
	struct device *dev = data->dev; /* bank0 dev */
	u32 int_state, regval, va34_32, pa34_32, table_base;
	u64 fault_iova, fault_pa;
	bool layer, write;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	int i;
	int id = data->plat_data->iommu_id;
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	u64 tf_iova_tmp;
	phys_addr_t fault_pgpa;
	#define TF_IOVA_DUMP_NUM	5
#else
	struct mtk_iommu_domain *dom = data->m4u_dom;
	unsigned int fault_larb, fault_port, sub_comm = 0;
#endif

	pr_warn("%s start, type:%d, id:%d\n", __func__,
		data->plat_data->iommu_type, data->plat_data->iommu_id);
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	for (i = IOMMU_BK1; i < IOMMU_BK_NUM; i++) {
		if (data->bk_irq[i] == irq) {
			base = data->bk_base[i];
			dev = data->bk_dev[i];
			pr_info("%s, type:%d, id:%d, bank:%d\n",
				__func__, data->plat_data->iommu_type,
				data->plat_data->iommu_id, i);
			if (i == IOMMU_BK4) {
				int ret;
				u32 fault_iova_32, fault_pa_32;

				ret = mtk_iommu_secure_bk_tf_dump(type, id, &fault_iova_32, &fault_pa_32, &regval);
				fault_iova = (u64)fault_iova_32;
				fault_pa = (u64)fault_pa_32;
				mtk_iommu_tlb_flush_all(data);
				if (ret) {
					dev_warn(dev, "%s secure bank fail, type:%d, id:%d\n",
						 __func__, type, id);
					return IRQ_HANDLED;
				}
				layer = fault_iova & F_MMU_FAULT_VA_LAYER_BIT;
				write = fault_iova & F_MMU_FAULT_VA_WRITE_BIT;
				report_custom_iommu_fault(fault_iova, fault_pa, regval, type, id);
				dev_warn(dev, "fault iova=0x%llx pa=0x%llx layer=%d %s\n",
					 fault_iova, fault_pa, layer, write ? "write" : "read");
				return IRQ_HANDLED;
			}
		}
	}
#endif
	table_base = readl_relaxed(base + REG_MMU_PT_BASE_ADDR);
	/* Read error info from registers */
	int_state = readl_relaxed(base + REG_MMU_FAULT_ST1);
	if (int_state & F_REG_MMU0_FAULT_MASK) {
		regval = readl_relaxed(base + REG_MMU0_INT_ID);
		fault_iova = readl_relaxed(base + REG_MMU0_FAULT_VA);
		fault_pa = readl_relaxed(base + REG_MMU0_INVLD_PA);
	} else {
		regval = readl_relaxed(base + REG_MMU1_INT_ID);
		fault_iova = readl_relaxed(base + REG_MMU1_FAULT_VA);
		fault_pa = readl_relaxed(base + REG_MMU1_INVLD_PA);
	}
	layer = fault_iova & F_MMU_FAULT_VA_LAYER_BIT;
	write = fault_iova & F_MMU_FAULT_VA_WRITE_BIT;

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOVA_34_EN)) {
		va34_32 = FIELD_GET(F_MMU_INVAL_VA_34_32_MASK, fault_iova);
		fault_iova = fault_iova & F_MMU_INVAL_VA_31_12_MASK;
		fault_iova |= (u64)va34_32 << 32;
	}
		pa34_32 = FIELD_GET(F_MMU_INVAL_PA_34_32_MASK, fault_iova);
		fault_pa |= (u64)pa34_32 << 32;

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
		for (i = 0, tf_iova_tmp = fault_iova; i < TF_IOVA_DUMP_NUM; i++) {
			if (i > 0)
				tf_iova_tmp -= SZ_4K;
			fault_pgpa = mtk_iommu_iova_to_phys(&data->m4u_dom->domain, tf_iova_tmp);
			pr_warn("[iommu_debug] error, index:%d, falut_iova:0x%lx, fault_pa(pg):%pa\n",
				i, tf_iova_tmp, &fault_pgpa);
			if (!fault_pgpa && i > 0)
				break;
		}
		if (fault_iova) /* skip dump when fault iova = 0 */
			mtk_iova_map_dump(fault_iova);
		report_custom_iommu_fault(fault_iova, fault_pa, regval, type, id);
		dev_warn(dev, "base:0x%x fault type=0x%x iova=0x%llx pa=0x%llx layer=%d %s\n",
			table_base, int_state, fault_iova, fault_pa, layer, write ? "write" : "read");
#else
	fault_port = F_MMU_INT_ID_PORT_ID(regval);
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_SUB_COMM)) {
		fault_larb = F_MMU_INT_ID_COMM_ID(regval);
		sub_comm = F_MMU_INT_ID_SUB_COMM_ID(regval);
	} else {
		fault_larb = F_MMU_INT_ID_LARB_ID(regval);
	}
	fault_larb = data->plat_data->larbid_remap[fault_larb][sub_comm];

	if (report_iommu_fault(&dom->domain, data->dev, fault_iova,
			       write ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ)) {
		dev_err_ratelimited(
			dev,
			"fault type=0x%x iova=0x%llx pa=0x%llx larb=%d port=%d layer=%d %s\n",
			int_state, fault_iova, fault_pa, fault_larb, fault_port,
			layer, write ? "write" : "read");
	}
#endif

	/* Interrupt clear */
	regval = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	regval |= F_INT_CLR_BIT;
	writel_relaxed(regval, base + REG_MMU_INT_CONTROL0);

	mtk_iommu_tlb_flush_all(data);

	mtk_iommu_isr_record(data);

	return IRQ_HANDLED;
}

static int mtk_iommu_get_domain_id(struct device *dev,
				   const struct mtk_iommu_plat_data *plat_data)
{
	const struct mtk_iommu_iova_region *rgn = plat_data->iova_region;
	const struct bus_dma_region *dma_rgn = dev->dma_range_map;
	struct mtk_iommu_data *data = dev_iommu_priv_get(dev);
	int i, candidate = -1;
	dma_addr_t dma_end;

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, GET_DOM_ID_LEGACY)) {
		int domid;
		struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

		domid = MTK_M4U_TO_DOM(fwspec->ids[0]);
		if (domid >= plat_data->iova_region_nr) {
			dev_err(dev, "iommu domain id(%d/%d) is error.\n", domid,
				plat_data->iova_region_nr);
			return -EINVAL;
		}

		return domid;
	}

	if (!dma_rgn || plat_data->iova_region_nr == 1)
		return 0;

	dma_end = dma_rgn->dma_start + dma_rgn->size - 1;
	for (i = 0; i < plat_data->iova_region_nr; i++, rgn++) {
		/* Best fit. */
		if (dma_rgn->dma_start == rgn->iova_base &&
		    dma_end == rgn->iova_base + rgn->size - 1)
			return i;
		/* ok if it is inside this region. */
		if (dma_rgn->dma_start >= rgn->iova_base &&
		    dma_end < rgn->iova_base + rgn->size)
			candidate = i;
	}

	if (candidate >= 0)
		return candidate;
	dev_err(dev, "Can NOT find the iommu domain id(%pad 0x%llx).\n",
		&dma_rgn->dma_start, dma_rgn->size);
	return -EINVAL;
}

static void mtk_iommu_config(struct mtk_iommu_data *data, struct device *dev,
			     bool enable, unsigned int domid)
{
	struct mtk_smi_larb_iommu    *larb_mmu;
	unsigned int                 larbid, portid;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	const struct mtk_iommu_iova_region *region;
	int i;

	if (data->plat_data->iommu_type != MM_IOMMU ||
	    MTK_IOMMU_HAS_FLAG(data->plat_data, SKIP_CFG_PORT))
		return;

	for (i = 0; i < fwspec->num_ids; ++i) {
		larbid = MTK_M4U_TO_LARB(fwspec->ids[i]);
		portid = MTK_M4U_TO_PORT(fwspec->ids[i]);

		larb_mmu = &data->larb_imu[larbid];

		region = data->plat_data->iova_region + domid;
		larb_mmu->bank[portid] = upper_32_bits(region->iova_base);

		dev_dbg(dev, "%s iommu for larb(%s) port %d dom %d bank %d.\n",
			enable ? "enable" : "disable", dev_name(larb_mmu->dev),
			portid, domid, larb_mmu->bank[portid]);

		if (enable)
			larb_mmu->mmu |= MTK_SMI_MMU_EN(portid);
		else
			larb_mmu->mmu &= ~MTK_SMI_MMU_EN(portid);
	}
}

static int mtk_iommu_domain_finalise(struct mtk_iommu_domain *dom,
				     struct mtk_iommu_data *data,
				     unsigned int domid)
{
	const struct mtk_iommu_iova_region *region;
	struct mtk_iommu_data *data_temp;

	for_each_m4u(data_temp) {
		/* Use the exist domain as there is only one pgtable here. */
		if (data_temp->m4u_dom) {
			dom->iop = data_temp->m4u_dom->iop;
			dom->cfg = data_temp->m4u_dom->cfg;
			dom->domain.pgsize_bitmap = data_temp->m4u_dom->cfg.pgsize_bitmap;
			goto update_iova_region;
		}
	}

	dom->cfg = (struct io_pgtable_cfg) {
		.quirks = IO_PGTABLE_QUIRK_ARM_NS |
			IO_PGTABLE_QUIRK_NO_PERMS |
			IO_PGTABLE_QUIRK_ARM_MTK_EXT,
		.pgsize_bitmap = mtk_iommu_ops.pgsize_bitmap,
		.ias = MTK_IOMMU_HAS_FLAG(data->plat_data, IOVA_34_EN) ? 34 : 32,
		.iommu_dev = data->dev,
	};

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_4GB_MODE))
		dom->cfg.oas = data->enable_4GB ? 33 : 32;
	else
		dom->cfg.oas = 35;

	dom->iop = alloc_io_pgtable_ops(ARM_V7S, &dom->cfg, data);
	if (!dom->iop) {
		dev_err(data->dev, "Failed to alloc io pgtable\n");
		return -EINVAL;
	}

	/* Update our support page sizes bitmap */
	dom->domain.pgsize_bitmap = dom->cfg.pgsize_bitmap;

update_iova_region:
	/* Update the iova region for this domain */
	region = data->plat_data->iova_region + domid;
	dom->domain.geometry.aperture_start = region->iova_base;
	dom->domain.geometry.aperture_end = region->iova_base + region->size - 1;
	dom->domain.geometry.force_aperture = true;

	pr_info("%s, dom:%u, start:%pa, end:0x%pa\n",
		__func__, domid,
		&dom->domain.geometry.aperture_start,
		&dom->domain.geometry.aperture_end);
	return 0;
}

static struct iommu_domain *mtk_iommu_domain_alloc(unsigned type)
{
	struct mtk_iommu_domain *dom;

	pr_info("%s start\n", __func__);
	if (type != IOMMU_DOMAIN_DMA)
		return NULL;

	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	if (iommu_get_dma_cookie(&dom->domain)) {
		kfree(dom);
		return NULL;
	}

	pr_info("%s done\n", __func__);
	return &dom->domain;
}

static void mtk_iommu_domain_free(struct iommu_domain *domain)
{
	iommu_put_dma_cookie(domain);
	kfree(to_mtk_domain(domain));
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_priv_get(dev);
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct device *m4udev = data->dev;
	int ret, domid;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	domid = mtk_iommu_get_domain_id(dev, data->plat_data);
	if (domid < 0)
		return domid;

	if (!dom->data) {
		if (mtk_iommu_domain_finalise(dom, data, domid))
			return -ENODEV;
		dom->data = data;
	}

	if (!data->m4u_dom) { /* Initialize the M4U HW */
		ret = pm_runtime_resume_and_get(m4udev);
		if (ret < 0)
			return ret;
		/*
		 * Because m4u_dom is used by mtk_iommu_isr, we must set it before
		 * enable all banks irq to avoid m4u_dom is NULL.
		 * ex: apu_iommu irq test.
		 */
		data->m4u_dom = dom;
		ret = mtk_iommu_hw_init(data);
		if (ret) {
			dev_err(data->dev, "HW init fail %d in attach\n",
				ret);
			pm_runtime_put(m4udev);
			return ret;
		}
		writel(dom->cfg.arm_v7s_cfg.ttbr & MMU_PT_ADDR_MASK,
		       data->base + REG_MMU_PT_BASE_ADDR);
		pr_info("%s, iommu_dev:%s(%d,%d), user_dev:%s, pgtable:0x%lx -- 0x%x -- 0x%x\n",
			__func__, dev_name(data->dev), data->plat_data->iommu_type,
			data->plat_data->iommu_id, dev_name(dev), (unsigned long)dom->cfg.arm_v7s_cfg.ttbr,
			dom->cfg.arm_v7s_cfg.ttbr, readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR));
		pm_runtime_put(m4udev);
	}

	mtk_iommu_config(data, dev, true, domid);

	pr_info("%s done, dev:%s, domid:%d\n", __func__, dev_name(dev), domid);
	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_priv_get(dev);

	mtk_iommu_config(data, dev, false, 0);
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot, gfp_t gfp)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	int ret;

	/* The "4GB mode" M4U physically can not use the lower remap of Dram. */
	if (dom->data->enable_4GB)
		paddr |= BIT_ULL(32);

	/* Synchronize with the tlb_lock */
	ret = dom->iop->map(dom->iop, iova, paddr, size, prot, gfp);
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	if (!ret)
		mtk_iova_map(iova, size);
#endif
	return ret;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size,
			      struct iommu_iotlb_gather *gather)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	unsigned long end = iova + size - 1;
	size_t ret;

	if (gather->start > iova)
		gather->start = iova;
	if (gather->end < end)
		gather->end = end;
	ret = dom->iop->unmap(dom->iop, iova, size, gather);
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	if (ret)
		mtk_iova_unmap(iova, size);
#endif
	return ret;
}

static void mtk_iommu_flush_iotlb_all(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_flush_all(dom->data);
}

static void mtk_iommu_iotlb_sync(struct iommu_domain *domain,
				 struct iommu_iotlb_gather *gather)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	size_t length = gather->end - gather->start + 1;

	mtk_iommu_tlb_flush_range_sync(gather->start, length, gather->pgsize,
				       dom->data);
}

static void mtk_iommu_sync_map(struct iommu_domain *domain, unsigned long iova,
			       size_t size)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_flush_range_sync(iova, size, size, dom->data);
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	phys_addr_t pa;

	pa = dom->iop->iova_to_phys(dom->iop, iova);
	if (dom->data->enable_4GB && pa >= MTK_IOMMU_4GB_MODE_REMAP_BASE)
		pa &= ~BIT_ULL(32);

	return pa;
}

static struct iommu_device *mtk_iommu_probe_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return ERR_PTR(-ENODEV); /* Not a iommu client device */

	data = dev_iommu_priv_get(dev);

	return &data->iommu;
}

static void mtk_iommu_release_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return;

	iommu_fwspec_free(dev);
}

static struct iommu_group *mtk_iommu_device_group(struct device *dev)
{
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data();
	struct iommu_group *group;
	int domid;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	if (!data)
		return ERR_PTR(-ENODEV);

	domid = mtk_iommu_get_domain_id(dev, data->plat_data);
	if (domid < 0)
		return ERR_PTR(domid);

	group = data->m4u_group[domid];
	if (!group) {
		group = iommu_group_alloc();
		if (!IS_ERR(group))
			data->m4u_group[domid] = group;
	} else {
		iommu_group_ref_get(group);
	}
	pr_info("%s done, dom:%d, dev:%s\n", __func__, domid, dev_name(dev));
	return group;
}

static int mtk_iommu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct platform_device *m4updev;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!dev_iommu_priv_get(dev)) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		if (WARN_ON(!m4updev))
			return -EINVAL;

		dev_iommu_priv_set(dev, platform_get_drvdata(m4updev));
	}

	return iommu_fwspec_add_ids(dev, args->args, 1);
}

static void mtk_iommu_get_resv_regions(struct device *dev,
				       struct list_head *head)
{
	struct mtk_iommu_data *data = dev_iommu_priv_get(dev);
	unsigned int domid = mtk_iommu_get_domain_id(dev, data->plat_data), i;
	const struct mtk_iommu_iova_region *resv, *curdom;
	struct iommu_resv_region *region;
	int prot = IOMMU_WRITE | IOMMU_READ;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	if ((int)domid < 0)
		return;
	curdom = data->plat_data->iova_region + domid;
	for (i = 0; i < data->plat_data->iova_region_nr; i++) {
		resv = data->plat_data->iova_region + i;

		/* Only reserve when the region is inside the current domain */
		if (resv->iova_base <= curdom->iova_base ||
		    resv->iova_base + resv->size >= curdom->iova_base + curdom->size)
			continue;

		region = iommu_alloc_resv_region(resv->iova_base, resv->size,
						 prot, IOMMU_RESV_RESERVED);
		if (!region)
			return;

		list_add_tail(&region->list, head);
	}
	pr_info("%s start, dev:%s, dom:%u\n", __func__, dev_name(dev), domid);
}

static const struct iommu_ops mtk_iommu_ops = {
	.domain_alloc	= mtk_iommu_domain_alloc,
	.domain_free	= mtk_iommu_domain_free,
	.attach_dev	= mtk_iommu_attach_device,
	.detach_dev	= mtk_iommu_detach_device,
	.map		= mtk_iommu_map,
	.unmap		= mtk_iommu_unmap,
	.flush_iotlb_all = mtk_iommu_flush_iotlb_all,
	.iotlb_sync	= mtk_iommu_iotlb_sync,
	.iotlb_sync_map	= mtk_iommu_sync_map,
	.iova_to_phys	= mtk_iommu_iova_to_phys,
	.probe_device	= mtk_iommu_probe_device,
	.release_device	= mtk_iommu_release_device,
	.device_group	= mtk_iommu_device_group,
	.of_xlate	= mtk_iommu_of_xlate,
	.get_resv_regions = mtk_iommu_get_resv_regions,
	.put_resv_regions = generic_iommu_put_resv_regions,
	.pgsize_bitmap	= SZ_4K | SZ_64K | SZ_1M | SZ_16M,
};

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data)
{
	u32 regval;
	int i, ret;

	ret = clk_prepare_enable(data->bclk);
	if (ret) {
		dev_err(data->dev, "Failed to enable iommu bclk(%d)\n", ret);
		return ret;
	}

	if (data->plat_data->m4u_plat == M4U_MT8173) {
		regval = F_MMU_PREFETCH_RT_REPLACE_MOD |
			 F_MMU_TF_PROT_TO_PROGRAM_ADDR_MT8173;
	} else {
		regval = readl_relaxed(data->base + REG_MMU_CTRL_REG);
		if (MTK_IOMMU_HAS_FLAG(data->plat_data, TLB_SYNC_EN))
			regval |= F_MMU_SYNC_INVLDT_EN;
		else
			regval |= F_MMU_TF_PROT_TO_PROGRAM_ADDR;
	}
	writel_relaxed(regval, data->base + REG_MMU_CTRL_REG);

	if (data->enable_4GB &&
	    MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_VLD_PA_RNG)) {
		/*
		 * If 4GB mode is enabled, the validate PA range is from
		 * 0x1_0000_0000 to 0x1_ffff_ffff. here record bit[32:30].
		 */
		regval = F_MMU_VLD_PA_RNG(7, 4);
		writel_relaxed(regval, data->base + REG_MMU_VLD_PA_RNG);
	}
	writel_relaxed(0, data->base + REG_MMU_DCM_DIS);
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, WR_THROT_EN)) {
		/* write command throttling mode */
		regval = readl_relaxed(data->base + REG_MMU_WR_LEN_CTRL);
		regval &= ~F_MMU_WR_THROT_DIS_MASK;
		writel_relaxed(regval, data->base + REG_MMU_WR_LEN_CTRL);
	}

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, RESET_AXI)) {
		/* The register is called STANDARD_AXI_MODE in this case */
		regval = 0;
	} else {
		regval = readl_relaxed(data->base + REG_MMU_MISC_CTRL);
		if (MTK_IOMMU_HAS_FLAG(data->plat_data, NOT_STD_AXI_MODE))
			regval &= ~F_MMU_STANDARD_AXI_MODE_MASK;
		if (MTK_IOMMU_HAS_FLAG(data->plat_data, OUT_ORDER_WR_EN))
			regval &= ~F_MMU_IN_ORDER_WR_EN_MASK;
	}
	writel_relaxed(regval, data->base + REG_MMU_MISC_CTRL);

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, SET_TBW_ID))
		writel_relaxed(data->plat_data->tbw_reg_val, data->base + REG_MMU_TBW_ID);

	for (i = IOMMU_BK0; i < IOMMU_BK_NUM; i++) {
		void __iomem *base = NULL;
		struct device *dev;
		unsigned int irq;

		if (i == IOMMU_BK0) {
			base = data->base;
			dev = data->dev;
			irq = data->irq;
		} else {
			if (!MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_BK_EN))
				break;
			base = data->bk_base[i];
			dev = data->bk_dev[i];
			irq = data->bk_irq[i];
		}
		/*
		 * If iommu don't find bank1~4 node in dtsi(not support bank1~4),
		 * base will be NULL. And bank4's base always is NULL.
		 *
		 */
		if (!base) {
			pr_info("%s, base is NULL, (%d,%d,%d), irq:%d\n",
				__func__,
				data->plat_data->iommu_type,
				data->plat_data->iommu_id, i, irq);
			goto register_irq;
		}
		regval = F_L2_MULIT_HIT_EN | F_TABLE_WALK_FAULT_INT_EN |
			F_PREETCH_FIFO_OVERFLOW_INT_EN |
			F_MISS_FIFO_OVERFLOW_INT_EN | F_PREFETCH_FIFO_ERR_INT_EN |
			F_MISS_FIFO_ERR_INT_EN;
		writel_relaxed(regval, base + REG_MMU_INT_CONTROL0);

		regval = F_INT_TRANSLATION_FAULT | F_INT_MAIN_MULTI_HIT_FAULT |
			F_INT_INVALID_PA_FAULT | F_INT_ENTRY_REPLACEMENT_FAULT |
			F_INT_TLB_MISS_FAULT | F_INT_MISS_TRANSACTION_FIFO_FAULT |
			F_INT_PRETETCH_TRANSATION_FIFO_FAULT;
		writel_relaxed(regval, base + REG_MMU_INT_MAIN_CONTROL);

		if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_LEGACY_IVRP_PADDR))
			regval = (data->protect_base >> 1) | (data->enable_4GB << 31);
		else
			regval = lower_32_bits(data->protect_base) |
				upper_32_bits(data->protect_base);
		writel_relaxed(regval, base + REG_MMU_IVRP_PADDR);

		pr_info("%s, (%d,%d,%d), irq:%d, dump reg: 0x48:0x%x, 0x50:0x%x, 0x54:0x%x, 0xa0:0x%x, 0x110:0x%x, 0x114:0x%x, 0x120:0x%x, 0x124:0x%x\n",
			__func__,
			data->plat_data->iommu_type,
			data->plat_data->iommu_id,
			i, irq,
			readl_relaxed(base + REG_MMU_MISC_CTRL),
			readl_relaxed(base + REG_MMU_DCM_DIS),
			readl_relaxed(base + REG_MMU_WR_LEN_CTRL),
			readl_relaxed(base + REG_MMU_TBW_ID),
			readl_relaxed(base + REG_MMU_CTRL_REG),
			readl_relaxed(base + REG_MMU_IVRP_PADDR),
			readl_relaxed(base + REG_MMU_INT_CONTROL0),
			readl_relaxed(base + REG_MMU_INT_MAIN_CONTROL));
register_irq:
		/*
		 * If iommu don't find bank1~4 node in dtsi(not support bank1~4),
		 * irq will be Zero.
		 *
		 */
		if (!irq) {
			pr_info("%s, (%d,%d,%d), irq:%d\n",
				__func__,
				data->plat_data->iommu_type,
				data->plat_data->iommu_id, i, irq);
			continue;
		}
		if (devm_request_irq(dev, irq, mtk_iommu_isr, 0, dev_name(dev), (void *)data)) {
			if (i != IOMMU_BK4)
				writel_relaxed(0, data->base + REG_MMU_PT_BASE_ADDR);
			clk_disable_unprepare(data->bclk);
			dev_err(dev, "Failed @ IRQ-%d Request\n", irq);
			return -ENODEV;
		}
		pr_info("%s, register irq done %d\n", __func__, irq);
	}

	pr_info("%s, done\n", __func__);
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
	struct device_node	*larbnode, *smicomm_node;
	struct platform_device	*plarbdev;
	struct device_link	*link;
	struct resource         *res;
	resource_size_t		ioaddr;
	struct component_match  *match = NULL;
	struct regmap		*infracfg;
	void                    *protect;
	int                     i, larb_nr, ret;
	u32			val;
	char                    *p;

	pr_info("%s start dev:%s\n", __func__, dev_name(dev));
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

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_4GB_MODE)) {
		switch (data->plat_data->m4u_plat) {
		case M4U_MT2712:
			p = "mediatek,mt2712-infracfg";
			break;
		case M4U_MT8173:
			p = "mediatek,mt8173-infracfg";
			break;
		default:
			p = NULL;
		}

		infracfg = syscon_regmap_lookup_by_compatible(p);

		if (IS_ERR(infracfg))
			return PTR_ERR(infracfg);

		ret = regmap_read(infracfg, REG_INFRA_MISC, &val);
		if (ret)
			return ret;
		data->enable_4GB = !!(val & F_DDR_4GB_SUPPORT_EN);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);
	ioaddr = res->start;

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_NO_IRQ))
		goto skip_irq;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		pr_info("%s, %s(%d,%d) can not irq!!\n", __func__, dev_name(dev),
			data->plat_data->iommu_type, data->plat_data->iommu_id);
		return data->irq;
	}
skip_irq:
	/*
	 * Note: we must be find iommu bank from bank1;
	 * And if iommu upstream, we need to merged with bank0.
	 */
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_BK_EN)) {
		int bk_nr = of_count_phandle_with_args(dev->of_node,
					     "mediatek,iommu_banks", NULL);

		if (bk_nr >= IOMMU_BK_NUM || bk_nr < 0) {
			pr_info("%s, get bank nr fail, %d\n", __func__, bk_nr);
			goto out;
		}
		pr_info("%s, get bank nr:%d\n", __func__, bk_nr);

		for (i = IOMMU_BK0; i < bk_nr; i++) {
			u32 bk_id;
			resource_size_t	bk_pa;
			struct resource *bk_res[IOMMU_BK_NUM];
			struct device_node *bk_node;
			struct platform_device *bk_dev;

			bk_node = of_parse_phandle(dev->of_node, "mediatek,iommu_banks", i);
			if (!bk_node) {
				dev_warn(dev, "Find iommu_bank:%d node fail\n", i);
				continue;
			}

			ret = of_property_read_u32(bk_node, "mediatek,bank-id", &bk_id);
			if (ret) {
				dev_warn(dev, "Get mediatek,bank-id fail\n");
				continue;
			}

			bk_dev = of_find_device_by_node(bk_node);
			if (!bk_dev) {
				of_node_put(bk_node);
				dev_warn(dev, "Find iommu_bank:%d dev fail\n", bk_id);
				continue;
			}
			if (!MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_NO_IRQ)) {
				data->bk_irq[bk_id] = platform_get_irq(bk_dev, 0);
				if (data->bk_irq[bk_id] < 0) {
					dev_err(dev, "Get iommu_bank:%d irq fail\n", bk_id);
					return data->bk_irq[bk_id];
				}
			}
			data->bk_dev[bk_id] = &bk_dev->dev;
			bk_res[bk_id] = platform_get_resource(bk_dev, IORESOURCE_MEM, 0);
			bk_pa = bk_res[bk_id]->start;
			dev_info(data->bk_dev[bk_id], "Get iommu bank:%u(%s) irq:%d ,pa:%pa, success\n",
				 bk_id, dev_name(data->bk_dev[bk_id]), data->bk_irq[bk_id], &bk_pa);

			/* Note: bank4(secure bank) base don't need to get base */
			if (bk_id == IOMMU_BK4)
				continue;
			data->bk_base[bk_id] = devm_ioremap_resource(data->bk_dev[bk_id], bk_res[bk_id]);
			if (IS_ERR(data->bk_base[bk_id])) {
				dev_err(dev, "get iommu_bank:%d base fail\n", bk_id);
				return PTR_ERR(data->bk_base[bk_id]);
			}
		}
	}
out:
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_BCLK)) {
		data->bclk = devm_clk_get(dev, "bclk");
		if (IS_ERR(data->bclk))
			return PTR_ERR(data->bclk);
	}

	/* Only for mt6873 and mt6893 */
	if (data->plat_data->iommu_type == APU_IOMMU &&
	    MTK_IOMMU_HAS_FLAG(data->plat_data, LINK_WITH_APU)) {
		struct device_node *apunode;
		struct platform_device *apudev;
		struct device_link *link;

		apunode = of_parse_phandle(dev->of_node, "mediatek,apu_power", 0);
		if (!apunode) {
			dev_warn(dev, "Can't find apu power node!\n");
			return -EINVAL;
		}
		apudev = of_find_device_by_node(apunode);
		if (!apudev) {
			of_node_put(apunode);
			dev_warn(dev, "Find apudev fail!\n");
			return -EPROBE_DEFER;
		}
		link = device_link_add(&apudev->dev, dev, DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		if (!link)
			dev_err(dev, "Unable link %s.\n", apudev->name);

		goto skip_smi;
	}

	/* PERI_IOMMU + APU_IOMMU */
	if (data->plat_data->iommu_type != MM_IOMMU ||
	    MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_EN_PRE)) {
		dev_info(dev, "skip smi\n");
		goto skip_smi;
	}
	larb_nr = of_count_phandle_with_args(dev->of_node,
					     "mediatek,larbs", NULL);
	if (larb_nr < 0) {
		dev_err(dev, "%s, can't fine mediatek,larbs !\n", __func__);
		return larb_nr;
	}

	for (i = 0; i < larb_nr; i++) {
		u32 id;

		larbnode = of_parse_phandle(dev->of_node, "mediatek,larbs", i);
		if (!larbnode)
			return -EINVAL;

		if (!of_device_is_available(larbnode)) {
			of_node_put(larbnode);
			continue;
		}

		ret = of_property_read_u32(larbnode, "mediatek,larb-id", &id);
		if (ret)/* The id is consecutive if there is no this property */
			id = i;

		plarbdev = of_find_device_by_node(larbnode);
		if (!plarbdev) {
			of_node_put(larbnode);
			return -EPROBE_DEFER;
		}
		data->larb_imu[id].dev = &plarbdev->dev;

		component_match_add_release(dev, &match, release_of,
					    compare_of, larbnode);
	}

	/* Get smi-common dev from the last larb. */
	smicomm_node = of_parse_phandle(larbnode, "mediatek,smi", 0);
	if (!smicomm_node)
		return -EINVAL;

	plarbdev = of_find_device_by_node(smicomm_node);
	of_node_put(smicomm_node);
	data->smicomm_dev = &plarbdev->dev;

skip_smi:
	pm_runtime_enable(dev);

	if (data->smicomm_dev) {
		link = device_link_add(data->smicomm_dev, dev,
				DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		if (!link) {
			dev_err(dev, "Unable to link %s.\n",
				dev_name(data->smicomm_dev));
			ret = -EINVAL;
			goto out_runtime_disable;
		}
	}

	platform_set_drvdata(pdev, data);

	ret = iommu_device_sysfs_add(&data->iommu, dev, NULL,
				     "mtk-iommu.%pa", &ioaddr);
	if (ret)
		goto out_link_remove;

	iommu_device_set_ops(&data->iommu, &mtk_iommu_ops);
	iommu_device_set_fwnode(&data->iommu, &pdev->dev.of_node->fwnode);

	ret = iommu_device_register(&data->iommu);
	if (ret)
		goto out_sysfs_remove;

	spin_lock_init(&data->tlb_lock);
	list_add_tail(&data->list, &m4ulist);

	if (!iommu_present(&platform_bus_type)) {
		ret = bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);
		if (ret)
			goto out_list_del;
	}

	if (data->plat_data->iommu_type == MM_IOMMU &&
	    !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_EN_PRE)) {
		ret = component_master_add_with_match(dev, &mtk_iommu_com_ops,
						      match);
		if (ret)
			goto out_bus_set_null;
	}

	mtk_iommu_isr_pause_timer_init(data);

	pr_info("%s done dev:%s\n", __func__, dev_name(dev));

	return ret;

out_bus_set_null:
	bus_set_iommu(&platform_bus_type, NULL);
out_list_del:
	list_del(&data->list);
	iommu_device_unregister(&data->iommu);
out_sysfs_remove:
	iommu_device_sysfs_remove(&data->iommu);
out_link_remove:
	device_link_remove(data->smicomm_dev, dev);
out_runtime_disable:
	pm_runtime_disable(dev);
	return ret;
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct mtk_iommu_data *data = platform_get_drvdata(pdev);

	iommu_device_sysfs_remove(&data->iommu);
	iommu_device_unregister(&data->iommu);

	if (iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, NULL);

	clk_disable_unprepare(data->bclk);
	device_link_remove(data->smicomm_dev, &pdev->dev);
	pm_runtime_disable(&pdev->dev);
	if (!MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_NO_IRQ))
		devm_free_irq(&pdev->dev, data->irq, data);
	component_master_del(&pdev->dev, &mtk_iommu_com_ops);
	return 0;
}

static int __maybe_unused mtk_iommu_runtime_suspend(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;

	reg->wr_len_ctrl = readl_relaxed(base + REG_MMU_WR_LEN_CTRL);
	reg->misc_ctrl = readl_relaxed(base + REG_MMU_MISC_CTRL);
	reg->dcm_dis = readl_relaxed(base + REG_MMU_DCM_DIS);
	reg->ctrl_reg = readl_relaxed(base + REG_MMU_CTRL_REG);
	reg->int_control0 = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	reg->int_main_control = readl_relaxed(base + REG_MMU_INT_MAIN_CONTROL);
	reg->ivrp_paddr = readl_relaxed(base + REG_MMU_IVRP_PADDR);
	reg->vld_pa_rng = readl_relaxed(base + REG_MMU_VLD_PA_RNG);
	reg->tbw_id = readl_relaxed(base + REG_MMU_TBW_ID);
	clk_disable_unprepare(data->bclk);
	return 0;
}

static int __maybe_unused mtk_iommu_runtime_resume(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	struct mtk_iommu_domain *m4u_dom = data->m4u_dom;
	void __iomem *base = data->base;
	int ret;

	/* Avoid first resume to affect the default value of registers below. */
	if (!m4u_dom)
		return 0;
	ret = clk_prepare_enable(data->bclk);
	if (ret) {
		dev_err(data->dev, "Failed to enable clk(%d) in resume\n", ret);
		return ret;
	}
	writel_relaxed(reg->tbw_id, base + REG_MMU_TBW_ID);
	writel_relaxed(reg->wr_len_ctrl, base + REG_MMU_WR_LEN_CTRL);
	writel_relaxed(reg->misc_ctrl, base + REG_MMU_MISC_CTRL);
	writel_relaxed(reg->dcm_dis, base + REG_MMU_DCM_DIS);
	writel_relaxed(reg->ctrl_reg, base + REG_MMU_CTRL_REG);
	writel_relaxed(reg->int_control0, base + REG_MMU_INT_CONTROL0);
	writel_relaxed(reg->int_main_control, base + REG_MMU_INT_MAIN_CONTROL);
	writel_relaxed(reg->ivrp_paddr, base + REG_MMU_IVRP_PADDR);
	writel_relaxed(reg->vld_pa_rng, base + REG_MMU_VLD_PA_RNG);
	writel(m4u_dom->cfg.arm_v7s_cfg.ttbr & MMU_PT_ADDR_MASK, base + REG_MMU_PT_BASE_ADDR);
	return 0;
}

static const struct dev_pm_ops mtk_iommu_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_iommu_runtime_suspend, mtk_iommu_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				     pm_runtime_force_resume)
};

static const struct mtk_iommu_plat_data mt2712_data = {
	.m4u_plat     = M4U_MT2712,
	.flags        = HAS_4GB_MODE | HAS_BCLK | HAS_VLD_PA_RNG |
			NOT_STD_AXI_MODE,
	.inv_sel_reg  = REG_MMU_INV_SEL_GEN1,
	.iova_region  = single_domain,
	.iova_region_nr = ARRAY_SIZE(single_domain),
	.iommu_id	= DISP_IOMMU,
	.iommu_type     = MM_IOMMU,
	.larbid_remap = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}},
};

static const struct mtk_iommu_plat_data mt6779_data = {
	.m4u_plat      = M4U_MT6779,
	.flags         = HAS_SUB_COMM | OUT_ORDER_WR_EN | WR_THROT_EN |
			 NOT_STD_AXI_MODE,
	.inv_sel_reg   = REG_MMU_INV_SEL_GEN2,
	.iova_region   = single_domain,
	.iova_region_nr = ARRAY_SIZE(single_domain),
	.iommu_id	= DISP_IOMMU,
	.iommu_type     = MM_IOMMU,
	.larbid_remap  = {{0}, {1}, {2}, {3}, {5}, {7, 8}, {10}, {9}},
};

static const struct mtk_iommu_plat_data mt6873_data = {
	.m4u_plat = M4U_MT6873,
	.flags         = HAS_SUB_COMM | OUT_ORDER_WR_EN | WR_THROT_EN |
			 HAS_BCLK | NOT_STD_AXI_MODE,
	.inv_sel_reg   = REG_MMU_INV_SEL_GEN2,
	.iommu_id	= DISP_IOMMU,
	.iommu_type     = MM_IOMMU,
	.iova_region    = mt6873_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6873_multi_dom),
	/* not use larbid_remap */
	.larbid_remap = {{0}, {1}, {4, 5}, {7}, {2}, {9, 11, 19, 20},
			 {0, 14, 16}, {0, 13, 18, 17}},
};

static const struct mtk_iommu_plat_data mt6873_data_apu = {
	.m4u_plat        = M4U_MT6873,
	.flags           = LINK_WITH_APU,
	.inv_sel_reg     = REG_MMU_INV_SEL_GEN2,
	.iommu_id	 = APU_IOMMU0,
	.iommu_type      = APU_IOMMU,
	.iova_region     = mt6873_multi_dom,
	/* not use larbid_remap */
	.larbid_remap    = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}},
	.iova_region_nr  = ARRAY_SIZE(mt6873_multi_dom),
};

static const struct mtk_iommu_plat_data mt6893_data_iommu0 = {
	.m4u_plat        = M4U_MT6893,
	.flags           = NOT_STD_AXI_MODE | HAS_SUB_COMM | OUT_ORDER_WR_EN | WR_THROT_EN | HAS_BCLK | IOVA_34_EN | GET_DOM_ID_LEGACY,
	/* not use larbid_remap */
	.larbid_remap    = {{0}, {1}, {4, 5}, {7}, {2}, {9, 11, 19, 20},
			    {0, 14, 16}, {0, 13, 18, 17}},
	.inv_sel_reg     = REG_MMU_INV_SEL_GEN2,
	.iommu_id	 = DISP_IOMMU,
	.iommu_type      = MM_IOMMU,
	.iova_region     = mt6873_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6873_multi_dom),
};

static const struct mtk_iommu_plat_data mt6893_data_iommu1 = {
	.m4u_plat        = M4U_MT6893,
	.flags           = NOT_STD_AXI_MODE | HAS_SUB_COMM | OUT_ORDER_WR_EN | WR_THROT_EN | HAS_BCLK | IOVA_34_EN | GET_DOM_ID_LEGACY,
	/* not use larbid_remap */
	.larbid_remap    = {{0}, {1}, {4, 5}, {7}, {2}, {9, 11, 19, 20},
			    {0, 14, 16}, {0, 13, 18, 17}},
	.inv_sel_reg     = REG_MMU_INV_SEL_GEN2,
	.iommu_id	 = MDP_IOMMU,
	.iommu_type      = MM_IOMMU,
	.iova_region     = mt6873_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6873_multi_dom),
};

static const struct mtk_iommu_plat_data mt6893_data_iommu2 = {
	.m4u_plat        = M4U_MT6893,
	.flags           = LINK_WITH_APU | IOVA_34_EN | GET_DOM_ID_LEGACY,
	.iommu_id	 = APU_IOMMU0,
	.iommu_type      = APU_IOMMU,
	.inv_sel_reg	 = REG_MMU_INV_SEL_GEN2,
	.iova_region	 = mt6873_multi_dom,
	/* not use larbid_remap */
	.larbid_remap    = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}},
	.iova_region_nr  = ARRAY_SIZE(mt6873_multi_dom),
};

static const struct mtk_iommu_plat_data mt6893_data_iommu3 = {
	.m4u_plat        = M4U_MT6893,
	.flags           = LINK_WITH_APU,
	.iommu_id	 = APU_IOMMU1,
	.iommu_type      = APU_IOMMU,
	.inv_sel_reg	 = REG_MMU_INV_SEL_GEN2,
	.iova_region	 = mt6873_multi_dom,
	/* not use larbid_remap */
	.larbid_remap    = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}},
	.iova_region_nr  = ARRAY_SIZE(mt6873_multi_dom),
};

static const struct mtk_iommu_plat_data mt6983_data_disp = {
	.m4u_plat	= M4U_MT6983,
	.flags          = HAS_SUB_COMM | OUT_ORDER_WR_EN | GET_DOM_ID_LEGACY |
			  NOT_STD_AXI_MODE | TLB_SYNC_EN | IOMMU_BK_EN | IOMMU_CLK_AO_EN |
			  IOMMU_EN_PRE | SKIP_CFG_PORT | IOVA_34_EN,// | HAS_BCLK,
	.inv_sel_reg    = REG_MMU_INV_SEL_GEN2,
	.iommu_id	= DISP_IOMMU,
	.iommu_type     = MM_IOMMU,
	.iova_region    = mt6983_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6983_multi_dom),
	/* not use larbid_remap */
	.larbid_remap	= {{0}, {0}, {21}, {0},         /*0 ~ 3*/
	                  {2}, {0}, {5}, {0},           /*4 ~ 7*/
	                  {7}, {0}, {9, 10, 11, 23},    /*8 ~ 10*/
	                  {0}, {13, 25, 27, 29}, {30},  /*11 ~ 13*/
	                  {6}, {0}},                    /*14 ~ 15*/
};

static const struct mtk_iommu_plat_data mt6983_data_mdp = {
	.m4u_plat	= M4U_MT6983,
	.flags          = HAS_SUB_COMM | OUT_ORDER_WR_EN | GET_DOM_ID_LEGACY |
			  NOT_STD_AXI_MODE | TLB_SYNC_EN | IOMMU_BK_EN | IOMMU_CLK_AO_EN |
			  IOMMU_EN_PRE | SKIP_CFG_PORT | IOVA_34_EN,// | HAS_BCLK,
	.inv_sel_reg    = REG_MMU_INV_SEL_GEN2,
	.iommu_id	= MDP_IOMMU,
	.iommu_type     = MM_IOMMU,
	.iova_region    = mt6983_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6983_multi_dom),
	/* not use larbid_remap */
	.larbid_remap	= {{1}, {0}, {20}, {0},         /*0 ~ 3*/
	                  {3}, {0}, {4}, {0},           /*4 ~ 7*/
	                  {8}, {0}, {22, 12, 15, 18},   /*8 ~ 10*/
	                  {0}, {14, 26, 16, 17}, {28, 19, 0, 0}, /*11 ~ 13*/
	                  {0}, {0}},                    /*14 ~ 15*/
};

static const struct mtk_iommu_plat_data mt6983_data_apu0 = {
	.m4u_plat	= M4U_MT6983,
	.flags          = HAS_SUB_COMM | TLB_SYNC_EN | IOMMU_BK_EN | GET_DOM_ID_LEGACY | IOVA_34_EN,// | HAS_BCLK,
	.inv_sel_reg    = REG_MMU_INV_SEL_GEN2,
	.iommu_id	= APU_IOMMU0,
	.iommu_type     = APU_IOMMU,
	.iova_region    = mt6983_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6983_multi_dom),
	/* not use larbid_remap */
};

static const struct mtk_iommu_plat_data mt6983_data_apu1 = {
	.m4u_plat	= M4U_MT6983,
	.flags          = HAS_SUB_COMM | TLB_SYNC_EN | IOMMU_BK_EN | GET_DOM_ID_LEGACY | IOVA_34_EN,// | HAS_BCLK,
	.inv_sel_reg    = REG_MMU_INV_SEL_GEN2,
	.iommu_id	= APU_IOMMU1,
	.iommu_type     = APU_IOMMU,
	.iova_region    = mt6983_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6983_multi_dom),
	/* not use larbid_remap */
};

static const struct mtk_iommu_plat_data mt6983_data_peri_m4 = {
	.m4u_plat	= M4U_MT6983,
	.flags          = HAS_SUB_COMM | SET_TBW_ID | TLB_SYNC_EN |
			  GET_DOM_ID_LEGACY | IOMMU_BK_EN | IOMMU_NO_IRQ | IOVA_34_EN,// | HAS_BCLK,
	.inv_sel_reg    = REG_MMU_INV_SEL_GEN2,
	.tbw_reg_val	= 0x3ffc3ffd,
	.iommu_id	= PERI_IOMMU_M4,
	.iommu_type     = PERI_IOMMU,
	.iova_region    = mt6983_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6983_multi_dom),
	/* not use larbid_remap */
};

static const struct mtk_iommu_plat_data mt6983_data_peri_m6 = {
	.m4u_plat	= M4U_MT6983,
	.flags          = HAS_SUB_COMM | SET_TBW_ID | TLB_SYNC_EN |
			  GET_DOM_ID_LEGACY | IOMMU_BK_EN | IOMMU_NO_IRQ | IOVA_34_EN,// | HAS_BCLK,
	.inv_sel_reg    = REG_MMU_INV_SEL_GEN2,
	.tbw_reg_val	= 0x03fc03fd,
	.iommu_id	= PERI_IOMMU_M6,
	.iommu_type     = PERI_IOMMU,
	.iova_region    = mt6983_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6983_multi_dom),
	/* not use larbid_remap */
};

static const struct mtk_iommu_plat_data mt6983_data_peri_m7 = {
	.m4u_plat	= M4U_MT6983,
	.flags          = HAS_SUB_COMM | TLB_SYNC_EN | IOMMU_BK_EN |
			  GET_DOM_ID_LEGACY | IOMMU_NO_IRQ | IOVA_34_EN,// | HAS_BCLK,
	.inv_sel_reg    = REG_MMU_INV_SEL_GEN2,
	.iommu_id	= PERI_IOMMU_M7,
	.iommu_type     = PERI_IOMMU,
	.iova_region    = mt6983_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt6983_multi_dom),
	/* not use larbid_remap */
};

static const struct mtk_iommu_plat_data mt8167_data = {
	.m4u_plat     = M4U_MT8167,
	.flags        = RESET_AXI | HAS_LEGACY_IVRP_PADDR,
	.inv_sel_reg  = REG_MMU_INV_SEL_GEN1,
	.iommu_id     = DISP_IOMMU,
	.iommu_type   = MM_IOMMU,
	.iova_region  = single_domain,
	.iova_region_nr = ARRAY_SIZE(single_domain),
	.larbid_remap = {{0}, {1}, {2}}, /* Linear mapping. */
};

static const struct mtk_iommu_plat_data mt8173_data = {
	.m4u_plat     = M4U_MT8173,
	.flags	      = HAS_4GB_MODE | HAS_BCLK | RESET_AXI |
			HAS_LEGACY_IVRP_PADDR,
	.inv_sel_reg  = REG_MMU_INV_SEL_GEN1,
	.iova_region  = single_domain,
	.iova_region_nr = ARRAY_SIZE(single_domain),
	.iommu_id	= DISP_IOMMU,
	.iommu_type    = MM_IOMMU,
	.larbid_remap = {{0}, {1}, {2}, {3}, {4}, {5}}, /* Linear mapping. */
};

static const struct mtk_iommu_plat_data mt8183_data = {
	.m4u_plat     = M4U_MT8183,
	.flags        = RESET_AXI,
	.inv_sel_reg  = REG_MMU_INV_SEL_GEN1,
	.iova_region  = single_domain,
	.iova_region_nr = ARRAY_SIZE(single_domain),
	.iommu_id	= DISP_IOMMU,
	.iommu_type    = MM_IOMMU,
	.larbid_remap = {{0}, {4}, {5}, {6}, {7}, {2}, {3}, {1}},
};

static const struct mtk_iommu_plat_data mt8192_data = {
	.m4u_plat       = M4U_MT8192,
	.flags          = HAS_BCLK | HAS_SUB_COMM | OUT_ORDER_WR_EN |
			  WR_THROT_EN | IOVA_34_EN,
	.inv_sel_reg    = REG_MMU_INV_SEL_GEN2,
	.iova_region    = mt8192_multi_dom,
	.iova_region_nr = ARRAY_SIZE(mt8192_multi_dom),
	.iommu_id	= DISP_IOMMU,
	.iommu_type      = MM_IOMMU,
	.larbid_remap   = {{0}, {1}, {4, 5}, {7}, {2}, {9, 11, 19, 20},
			   {0, 14, 16}, {0, 13, 18, 17}},
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,mt2712-m4u", .data = &mt2712_data},
	{ .compatible = "mediatek,mt6779-m4u", .data = &mt6779_data},
	{ .compatible = "mediatek,mt6873-m4u", .data = &mt6873_data},
	{ .compatible = "mediatek,mt6873-apu-iommu", .data = &mt6873_data_apu},
	{ .compatible = "mediatek,mt6893-iommu0", .data = &mt6893_data_iommu0},
	{ .compatible = "mediatek,mt6893-iommu1", .data = &mt6893_data_iommu1},
	{ .compatible = "mediatek,mt6893-iommu2", .data = &mt6893_data_iommu2},
	{ .compatible = "mediatek,mt6893-iommu3", .data = &mt6893_data_iommu3},
	{ .compatible = "mediatek,mt6983-apu-iommu0", .data = &mt6983_data_apu0},
	{ .compatible = "mediatek,mt6983-apu-iommu1", .data = &mt6983_data_apu1},
	{ .compatible = "mediatek,mt6983-disp-iommu", .data = &mt6983_data_disp},
	{ .compatible = "mediatek,mt6983-mdp-iommu", .data = &mt6983_data_mdp},
	{ .compatible = "mediatek,mt6983-peri-iommu-m4", .data = &mt6983_data_peri_m4},
	{ .compatible = "mediatek,mt6983-peri-iommu-m6", .data = &mt6983_data_peri_m6},
	{ .compatible = "mediatek,mt6983-peri-iommu-m7", .data = &mt6983_data_peri_m7},
	{ .compatible = "mediatek,mt8167-m4u", .data = &mt8167_data},
	{ .compatible = "mediatek,mt8173-m4u", .data = &mt8173_data},
	{ .compatible = "mediatek,mt8183-m4u", .data = &mt8183_data},
	{ .compatible = "mediatek,mt8192-m4u", .data = &mt8192_data},
	{}
};

static struct platform_driver mtk_iommu_driver = {
	.probe	= mtk_iommu_probe,
	.remove	= mtk_iommu_remove,
	.driver	= {
		.name = "mtk-iommu",
		.of_match_table = mtk_iommu_of_ids,
		.pm = &mtk_iommu_pm_ops,
	}
};
module_platform_driver(mtk_iommu_driver);

MODULE_DESCRIPTION("IOMMU API for MediaTek M4U implementations");
MODULE_LICENSE("GPL v2");
