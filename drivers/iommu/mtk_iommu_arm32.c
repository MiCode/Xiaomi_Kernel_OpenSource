// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: junyun <junyun.zhang@mediatek.com>
 * Change for the Arm32 flow base on drivers/iommu/mtk_iommu.c
 */
#define dev_fmt(fmt)    "mtk_iommu_32: " fmt

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
#include <linux/arm-smccc.h>
#include <asm/barrier.h>
#include <soc/mediatek/smi.h>
#ifndef CONFIG_ARM64
#include <asm/dma-iommu.h>
#endif
#if IS_ENABLED(CONFIG_MTK_SMI)
#include <../misc/mediatek/smi/mtk-smi-dbg.h>
#endif
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
#include <../misc/mediatek/iommu/iommu_debug.h>
#endif
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
#include <../misc/mediatek/iommu/iommu_secure.h>
#endif

#include "mtk_iommu.h"

#define REG_MMU_PT_BASE_ADDR			0x000
#define MMU_PT_ADDR_MASK			GENMASK(31, 7)

#define REG_MMU_STA				0x008

#define REG_MMU_INVALIDATE			0x020
#define F_ALL_INVLD				0x2
#define F_MMU_INV_RANGE				0x1

#define REG_MMU_INVLD_START_A			0x024
#define REG_MMU_INVLD_END_A			0x028

#define REG_MMU_INV_SEL_GEN2			0x02c
#define REG_MMU_INV_SEL_GEN1			0x038
#define F_INVLD_EN0				BIT(0)
#define F_INVLD_EN1				BIT(1)

#define REG_MMU_DUMMY				0x044

#define REG_MMU_MISC_CTRL			0x048
#define F_MMU_IN_ORDER_WR_EN_MASK		(BIT(1) | BIT(17))
#define F_MMU_STANDARD_AXI_MODE_MASK		(BIT(3) | BIT(19))

#define REG_MMU_DCM_DIS				0x050
#define REG_MMU_WR_LEN_CTRL			0x054
#define F_MMU_WR_THROT_DIS_MASK			(BIT(5) | BIT(21))

#define REG_MMU_DBG(index)			(0x060 + index * 4)

#define REG_MMU_TBW_ID				0xa0

#define REG_MMU_CTRL_REG			0x110
#define F_MMU_MON_EN				BIT(1)
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
#define F_INT_MAIN_MAU_INT_EN(MAUCNT)	\
	(F_REG_MMU_MAU_INT_MASK(0, MAUCNT) | F_REG_MMU_MAU_INT_MASK(1, MAUCNT))
#define F_INT_MMU_MAU_INT_EN(MMU, MAU, MAUCNT)	BIT(14+(MMU)*(MAUCNT)+(MAU))

#define REG_MMU_CPE_DONE			0x12C

#define REG_MMU_FAULT_ST0			0x130
#define F_INT_MHIT_ERR				BIT(0)
#define F_INT_TBW_FAULT				BIT(1)
#define F_INT_PFQ_FIFO_FULL			BIT(2)
#define F_INT_MQ_FIFO_FULL			BIT(3)
#define F_INT_INVLDT_DONE			BIT(4)
#define F_INT_PFQ_OUT_FIFO_ERR			BIT(5)
#define F_INT_PFQ_IN_FIFO_ERR			BIT(6)
#define F_INT_MQ_OUT_FIFO_ERR			BIT(7)
#define F_INT_MQ_IN_FIFO_ERR			BIT(8)
#define F_INT_CDB_SLICE_ERR			BIT(9)
#define F_REG_MMU_FAULT_ST0_MASK		GENMASK(9, 0)

#define REG_MMU_FAULT_ST1			0x134
#define F_INT_MMU_TF_ERR(MMU)			BIT(0+(MMU)*7)
#define F_INT_MMU_MHIT_ERR(MMU)			BIT(1+(MMU)*7)
#define F_INT_MMU_INV_PA_ERR(MMU)		BIT(2+(MMU)*7)
#define F_INT_MMU_ENTR_REP_ERR(MMU)		BIT(3+(MMU)*7)
#define F_INT_MMU_TLBM_ERR(MMU)			BIT(4+(MMU)*7)
#define F_INT_MMU_MQ_OVF_ERR(MMU)		BIT(5+(MMU)*7)
#define F_INT_MMU_PFQ_OVF_ERR(MMU)		BIT(6+(MMU)*7)
#define F_INT_MMU_MAU_INT_STA(MMU, MAU, MAUCNT)	BIT(14+(MMU)*(MAUCNT)+(MAU))

#define F_REG_MMU0_INV_PA_MASK			BIT(0)
#define F_REG_MMU0_TF_MASK			BIT(2)
#define F_REG_MMU0_MAU_INT_MASK			GENMASK(10, 7)
#define F_REG_MMU0_FAULT_MASK			GENMASK(6, 0)
#define F_REG_MMU1_FAULT_MASK			GENMASK(13, 7)
#define F_REG_MMU_MAU_INT_MASK(MMU, MAUCNT)	\
	GENMASK((14+(MAUCNT)*((MMU)+1)-1), (14+(MAUCNT)*(MMU)))

#define REG_MMU_TBWALK_FAULT_VA			0x138
#define F_L2_TBWALK_FAULT_VA_31_12_MASK		GENMASK(31, 12)
#define F_L2_TBWALK_FAULT_VA_33_32_MASK		GENMASK(10, 9)
#define F_L2_TBWALK_FAULT_PAGE_LAYER_BIT	BIT(0)

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

#define REG_MMU_RS_VA(MMU, RS)			(0x380 + MMU * 0x300 + RS * 0x10)

/* MAU set register */
#define REG_MMU_MAU_SA(MMU, MAU)		(0x900+(MMU)*0x100+(MAU)*0x24)
#define REG_MMU_MAU_SA_EXT(MMU, MAU)		(0x904+(MMU)*0x100+(MAU)*0x24)
#define REG_MMU_MAU_EA(MMU, MAU)		(0x908+(MMU)*0x100+(MAU)*0x24)
#define REG_MMU_MAU_EA_EXT(MMU, MAU)		(0x90C+(MMU)*0x100+(MAU)*0x24)
#define REG_MMU_MAU_LARB_EN(MMU, MAU)		(0x910+(MMU)*0x100+(MAU)*0x24)
#define REG_MMU_MAU_PORT_EN(MMU, MAU)		(0x914+(MMU)*0x100+(MAU)*0x24)
#define REG_MMU_MAU_ASRT_ID(MMU, MAU)		(0x918+(MMU)*0x100+(MAU)*0x24)
#define REG_MMU_MAU_AA(MMU, MAU)		(0x91C+(MMU)*0x100+(MAU)*0x24)
#define REG_MMU_MAU_AA_EXT(MMU, MAU)		(0x920+(MMU)*0x100+(MAU)*0x24)

#define REG_MMU_MAU_CLR(MMU, CNT)		(0x900+(MMU)*0x100+(CNT)*0x24)
#define REG_MMU_MAU_IO(MMU, CNT)		(0x904+(MMU)*0x100+(CNT)*0x24)
#define REG_MMU_MAU_RW(MMU, CNT)		(0x908+(MMU)*0x100+(CNT)*0x24)
#define REG_MMU_MAU_VA(MMU, CNT)		(0x90C+(MMU)*0x100+(CNT)*0x24)
#define REG_MMU_MAU_ASRT_STA(MMU, CNT)		(0x910+(MMU)*0x100+(CNT)*0x24)
#define REG_MMU_MAU_C4K(MMU, CNT)		(0x914+(MMU)*0x100+(CNT)*0x24)
#define REG_MMU_MAU_ASSERT_INFO(MMU, CNT)	(0x918+(MMU)*0x100+(CNT)*0x24)

#define F_MMU_MAU_BIT_VAL(VAL, MAU)		((!!(VAL))<<(MAU))
#define F_MMU_MAU_ASRT_ID_VAL			GENMASK(7, 0)

/* iommu hw register define */
#define MTK_IOMMU_DEBUG_REG_NR			(7)
#define MTK_IOMMU_MMU_COUNT			(2)
#define MTK_IOMMU_RS_COUNT			(16)

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
#define IOMMU_SEC_BK_EN			BIT(13)
#define SKIP_CFG_PORT			BIT(14)
/* For IOMMU EP/bring up phase: CLK AO */
#define IOMMU_CLK_AO_EN			BIT(15)
/* For IOMMU EP/bring up phase: smi not ready */
#define IOMMU_EN_PRE			BIT(16)
/* Debug: Skip register IRQ */
#define IOMMU_NO_IRQ			BIT(17)
#define GET_DOM_ID_LEGACY		BIT(18)
#define HAS_SMI_SUB_COMM		BIT(19)
#define SAME_SUBSYS			BIT(20)
#define IOMMU_MAU_EN			BIT(21)
#define PM_OPS_SKIP			BIT(22)
#define SHARE_PGTABLE			BIT(23)
#define IOMMU_NO_SMCCC			BIT(24)

#define POWER_ON_STA		1
#define POWER_OFF_STA		0

#define MTK_IOMMU_HAS_FLAG(pdata, _x) \
		((((pdata)->flags) & (_x)) == (_x))

#define MTK_IOMMU_ISR_COUNT_MAX			5
#define MTK_IOMMU_ISR_DISABLE_TIME		10
#define MTK_IOMMU_TF_IOVA_DUMP_NUM		5

#define MMU_MAU_REG_BACKUP_SIZE		(100 * sizeof(unsigned int))

/* hyp-pmm fastcalls */
#define HYP_PMM_SHARE_IOVA			(0XBB00FFA2)
#define HYP_PMM_UNSHARE_IOVA			(0XBB00FFA3)
#define HYP_PMM_GET_HYPMMU_TYPE2_EN		(0XBB00FFA4)
#define HYP_PMM_REG_HYPMMU_SHARE_REGION		(0XBB00FFA5)
#define HYP_PMM_IOVA_TO_PHYS			(0XBB00FFA6)
#define HYP_PMM_HYPMMU_TYPE2_INV		(0XBB00FFA7)

struct mtk_iommu_domain {
	int				tab_id;
	struct io_pgtable_cfg		cfg;
	struct io_pgtable_ops		*iop;

	struct mtk_iommu_data		*data;
	struct iommu_domain		domain;
};

static const struct iommu_ops mtk_iommu_ops;

static bool pd_sta[MM_IOMMU_NUM];
static spinlock_t tlb_locks[MM_IOMMU_NUM];
static struct notifier_block mtk_pd_notifiers[MM_IOMMU_NUM];
static bool hypmmu_type2_en;
static struct mutex init_mutexs[PGTBALE_NUM];
static struct mutex group_mutexs[MTK_IOMMU_GROUP_MAX];
static atomic_t init_once_flag = ATOMIC_INIT(0);

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data);

#ifndef	CONFIG_ARM64
/*
 * the code is to use of_phandle_iterator_args, but because the CONFIG_MTK_IOMMU = m
 * leading of_phandle_iterator_args build error. (CONFIG_MTK_IOMMU = y lead other build error)
 * So, the temp solution is porting the of_phandle_iterator_args as of_phandle_iterator_args_2
 *
 * TO_DO:
 * next action is aim to use a new function to replance the of_phandle_iterator_args
 *
 */
static int of_phandle_iterator_args_2(struct of_phandle_iterator *it,
			     uint32_t *args,
			     int size)
{
	int i, count;

	count = it->cur_count;

	if (WARN_ON(size < count))
		count = size;
	for (i = 0; i < count; i++)
		args[i] = be32_to_cpup(it->cur++);
	return count;
}
#endif

#define MTK_IOMMU_TLB_ADDR(iova) ({					\
	dma_addr_t _addr = iova;					\
	((lower_32_bits(_addr) & GENMASK(31, 12)) | upper_32_bits(_addr));\
})

#define MTK_IOMMU_ID_FLAG(type, id)		BIT((2*(type)) + (id))

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
static LIST_HEAD(mm_iommu_list);	/* List apu iommu HWs */
static LIST_HEAD(apu_iommu_list);	/* List mm iommu HWs */
static bool share_pgtable;

#define for_each_m4u(data, head)  list_for_each_entry(data, head, list)

enum iova_type {
	NORMAL,
	PROTECTED,
	SECURE,
	IOVA_TYPE
};

struct mtk_iommu_iova_region {
	dma_addr_t		iova_base;
	unsigned long long	size;
	enum iova_type		type;
};

static struct mtk_iommu_iova_region single_domain[] __maybe_unused = {
	{.iova_base = 0,		.size = SZ_4G},
};

static struct mtk_iommu_iova_region single_domain_ext[] __maybe_unused = {
	{.iova_base = 0, .size = SZ_4G * 4},
};

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova);
#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
uint64_t mtee_iova_to_phys(unsigned long iova, u32 tab_id, u32 *sr_info,
				u64 *pa, u32 *type, u32 *lvl)
{
	struct arm_smccc_res smc_res;
	uint32_t ns_res, prot_res;

	arm_smccc_smc(HYP_PMM_IOVA_TO_PHYS, lower_32_bits(iova),
		      upper_32_bits(iova), tab_id, 0, 0, 0, 0, &smc_res);

	/*
	 * ns_table: a0[31:0], prot_table: a0[63:32]
	 * smc_res.a0: include PA + SR_INFO + TYPE
	 * smc_res.a0[2:0] = PA[34:32]
	 * smc_res.a0[7:3] = SR_INFO
	 * smc_res.a0[9:8] = TYPE
	 * smc_res.a0[11:10] = lvl
	 */
	ns_res = smc_res.a0 & 0xffffffff;
	prot_res = (smc_res.a0 >> 32) & 0xffffffff;

	sr_info[NS_TAB] = FIELD_GET(GENMASK(7, 3), ns_res);
	pa[NS_TAB] = (FIELD_GET(GENMASK(2, 0), ns_res) << 32) | (ns_res & (~(PAGE_SIZE - 1)));
	type[NS_TAB] = FIELD_GET(GENMASK(9, 8), ns_res);
	lvl[NS_TAB] = FIELD_GET(GENMASK(11, 10), ns_res);

	sr_info[PROT_TAB] = FIELD_GET(GENMASK(7, 3), prot_res);
	pa[PROT_TAB] = (FIELD_GET(GENMASK(2, 0), prot_res) << 32) | (prot_res & (~(PAGE_SIZE - 1)));
	type[PROT_TAB] = FIELD_GET(GENMASK(9, 8), prot_res);
	lvl[PROT_TAB] = FIELD_GET(GENMASK(11, 10), prot_res);

	return pa[NS_TAB];
}
EXPORT_SYMBOL_GPL(mtee_iova_to_phys);
#endif

static int mtee_share_iova(uint64_t iova_start, uint32_t size)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(HYP_PMM_SHARE_IOVA, lower_32_bits(iova_start),
		      upper_32_bits(iova_start), size, 0, 0, 0, 0, &smc_res);

	if (smc_res.a0)
		return -EINVAL;

	return 0;
}

static int mtee_unshare_iova(uint64_t iova_start, uint32_t size)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(HYP_PMM_UNSHARE_IOVA, lower_32_bits(iova_start),
		      upper_32_bits(iova_start), size, 0, 0, 0, 0, &smc_res);

	if (smc_res.a0)
		return -EINVAL;

	return 0;
}

static bool mtee_hypmmu_type2_enabled(void)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(HYP_PMM_GET_HYPMMU_TYPE2_EN, 0, 0, 0, 0, 0, 0, 0, &smc_res);
	pr_info("%s, hyp-mmu ret:%lu\n", __func__, smc_res.a0);
	if (smc_res.a0 == 1) {
		hypmmu_type2_en = true;
		return true;
	}
	return false;
}

static int mtee_hypmmu_type2_inv(unsigned long iova, size_t size, int tab_id)
{
	struct arm_smccc_res smc_res;
	u32 sa, ea;

	sa = MTK_IOMMU_TLB_ADDR(iova);
	ea = MTK_IOMMU_TLB_ADDR(iova + size - 1);

	arm_smccc_smc(HYP_PMM_HYPMMU_TYPE2_INV, sa, ea, tab_id, 0, 0, 0, 0, &smc_res);
	if (smc_res.a0) {
		pr_err("%s err ret:%lu", __func__, smc_res.a0);
		return -EINVAL;
	}

	return 0;
}

static int mtee_hypmmu_reg_share_region(u32 base_page_no, u32 size_in_pages, u32 tab_id)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(HYP_PMM_REG_HYPMMU_SHARE_REGION, base_page_no, size_in_pages,
		tab_id, 0, 0, 0, 0, &smc_res);

	if (smc_res.a0) {
		pr_err("%s err ret:%lu", __func__, smc_res.a0);
		WARN_ON_ONCE(smc_res.a0 != 0);
		return -EINVAL;
	}

	return 0;
}

static int iova_is_secure(struct mtk_iommu_data *data, unsigned long iova, size_t size)
{
	int i;
	const struct mtk_iommu_iova_region *region;
	unsigned long iova_end = iova + size - 1;

	if (iova > iova_end)
		return 0;

	for (i = 0; i < data->plat_data->iova_region_nr; i++) {
		region = &data->plat_data->iova_region[i];
		if (iova >= region->iova_base && iova_end < (region->iova_base + region->size) &&
		    region->type == PROTECTED)
			return 1;
	}

	return 0;
}

static int iova_secure_map(struct mtk_iommu_data *data, unsigned long iova,
				size_t size, bool mapped)
{
	int ret;

	ret = iova_is_secure(data, iova, size);
	if (!ret)
		return 0;

	if (mapped)
		ret = mtee_share_iova(iova, size);
	else
		ret = mtee_unshare_iova(iova, size);
	if (ret) {
		pr_err("%s err, share_iova failed, iova:0x%lx ~ 0x%lx, mapped:%d\n",
		       __func__, iova, (iova + size - 1), mapped);
		return -EINVAL;
	}

	pr_info("%s done, iova:0x%lx ~ 0x%lx, mapped:%d\n",
		__func__, iova, (iova + size - 1), mapped);
	return 0;
}

static int iova_secure_inv(unsigned long iova, size_t size, int tab_id)
{
	int ret;

	ret = mtee_hypmmu_type2_inv(iova, size, tab_id);
	if (ret) {
		pr_err("%s err, type2_inv failed, iova:0x%lx ~ 0x%lx\n",
		       __func__, iova, (iova + size - 1));
		return -EINVAL;
	}

	return 0;
}

/* If 2 M4U share a domain(use the same hwlist), Put the corresponding info in first data.*/
static struct mtk_iommu_data *mtk_iommu_get_first_data(struct list_head *hwlist)
{
	return list_first_entry(hwlist, struct mtk_iommu_data, list);
}

static struct mtk_iommu_domain *to_mtk_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct mtk_iommu_domain, domain);
}

/**
 * mtk_iommu_power_get - Get iommu power status,
 * conditionally call pm_runtime_get_if_in_use.
 * @data: iommu data of target device
 * @see #mtk_iommu_power_put
 * @return true if power on
 *
 * Notice: This function must be called pairs with mtk_iommu_power_put.
 */
static __maybe_unused bool mtk_iommu_power_get(struct mtk_iommu_data *data)
{
	bool has_pm = !!data->dev->pm_domain;

	if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN)) {
		if ((data->plat_data->iommu_type == MM_IOMMU &&
			pd_sta[data->plat_data->iommu_id] == POWER_OFF_STA) ||
			(data->plat_data->iommu_type != MM_IOMMU &&
			pm_runtime_get_if_in_use(data->dev) <= 0)) {
			return false;
		}
	}

	return true;
}

/**
 * mtk_iommu_power_put - Put iommu power status,
 * conditionally call pm_runtime_put.
 * @data: iommu data of target device
 * @see #mtk_iommu_power_get
 *
 * Notice: This function must be called pairs with mtk_iommu_power_get.
 */
static __maybe_unused void mtk_iommu_power_put(struct mtk_iommu_data *data)
{
	bool has_pm = !!data->dev->pm_domain;

	if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN) &&
		data->plat_data->iommu_type != MM_IOMMU)
		pm_runtime_put(data->dev);
}

static void mtk_iommu_bk0_intr_en(const struct mtk_iommu_data *data,
				unsigned long enable)
{
	u32 regval;
	void __iomem *base = data->base;

	if (enable) {
		regval = F_L2_MULIT_HIT_EN | F_TABLE_WALK_FAULT_INT_EN |
			F_PREFETCH_FIFO_ERR_INT_EN | F_MISS_FIFO_ERR_INT_EN;
		writel_relaxed(regval, base + REG_MMU_INT_CONTROL0);

		regval = F_INT_TRANSLATION_FAULT | F_INT_MAIN_MULTI_HIT_FAULT |
			F_INT_INVALID_PA_FAULT | F_INT_ENTRY_REPLACEMENT_FAULT |
			F_INT_TLB_MISS_FAULT | F_INT_MISS_TRANSACTION_FIFO_FAULT;

		if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_MAU_EN))
			regval |= F_INT_MAIN_MAU_INT_EN(data->plat_data->mau_count);

		writel_relaxed(regval, base + REG_MMU_INT_MAIN_CONTROL);
	} else {
		writel_relaxed(0, base + REG_MMU_INT_CONTROL0);
		writel_relaxed(0, base + REG_MMU_INT_MAIN_CONTROL);
	}
}

static inline void mtk_iommu_isr_setup(struct mtk_iommu_data *data, unsigned long enable)
{
	bool has_pm = !!data->dev->pm_domain;

	pr_info("%s, iommu:(%d,%d), enable:%lu\n", __func__,
		data->plat_data->iommu_type, data->plat_data->iommu_id, enable);
	if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN)) {
		if ((data->plat_data->iommu_type == MM_IOMMU &&
			pd_sta[data->plat_data->iommu_id] == POWER_OFF_STA) ||
			(data->plat_data->iommu_type != MM_IOMMU &&
			pm_runtime_get_if_in_use(data->dev) <= 0)) {
			pr_info("%s, power off:%s\n", __func__, dev_name(data->dev));
			return;
		}
	}
	mtk_iommu_bk0_intr_en(data, enable);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_SEC_BK_EN))
		mtk_iommu_sec_bk_irq_en_by_atf(data->plat_data->iommu_type,
				data->plat_data->iommu_id, enable);
#endif

	if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN) &&
		data->plat_data->iommu_type != MM_IOMMU)
		pm_runtime_put(data->dev);
}

static void mtk_iommu_isr_restart(struct timer_list *t)
{
	struct mtk_iommu_data *data = from_timer(data, t, iommu_isr_pause_timer);

	mtk_iommu_isr_setup(data, 1);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	mtk_iommu_debug_reset();
#endif
}

static int mtk_iommu_isr_pause_timer_init(struct mtk_iommu_data *data)
{
	timer_setup(&data->iommu_isr_pause_timer, mtk_iommu_isr_restart, 0);
	return 0;
}

static int mtk_iommu_isr_pause(struct mtk_iommu_data *data, int delay)
{
	/* disable all intr */
	mtk_iommu_isr_setup(data, 0);
	/* delay seconds */
	data->iommu_isr_pause_timer.expires = jiffies + delay * HZ;
	if (!timer_pending(&data->iommu_isr_pause_timer))
		add_timer(&data->iommu_isr_pause_timer);
	return 0;
}

static void mtk_iommu_isr_record(struct mtk_iommu_data *data)
{
	/* we allow one irq in 1s, or we will disable them after isr_cnt s. */
	if (!data->isr_cnt || time_after(jiffies, data->first_jiffies + data->isr_cnt * HZ)) {
		data->isr_cnt = 1;
		data->first_jiffies = jiffies;
	} else {
		data->isr_cnt++;
		if (data->isr_cnt >= MTK_IOMMU_ISR_COUNT_MAX) {
			/* irq too many! disable irq for a while, to avoid HWT timeout*/
			mtk_iommu_isr_pause(data, MTK_IOMMU_ISR_DISABLE_TIME);
			data->isr_cnt = 0;
		}
	}
}

static void mtk_dump_reg_for_hang_issue(struct mtk_iommu_data *data);

static void mtk_iommu_tlb_flush_check(struct mtk_iommu_data *data, bool range)
{
	u32 tlb_en = readl_relaxed(data->base + REG_MMU_INVALIDATE);
	u32 cpe_done = readl_relaxed(data->base + REG_MMU_CPE_DONE);

	pr_info("%s in, range:%d, tlb_en:0x%x, cpe_done:%d\n", __func__, range, tlb_en, cpe_done);
	/* check tlb inv enable bit */
	if (range && (tlb_en & F_MMU_INV_RANGE)) {
		pr_warn("%s, TLB flush Range timed out, need to extend time!!(%d, %d)\n", __func__,
			data->plat_data->iommu_type, data->plat_data->iommu_id);
		mtk_dump_reg_for_hang_issue(data);
		mtk_smi_dbg_hang_detect("iommu");
		pr_warn("%s, dump: 0x20:0x%x, 0x12c:0x%x\n",
			__func__, readl_relaxed(data->base + REG_MMU_INVALIDATE),
			readl_relaxed(data->base + REG_MMU_CPE_DONE));
		return;
	} else if (!range && (tlb_en & F_ALL_INVLD)) {
		pr_warn("%s, TLB flush All timed out, need to extend time!!(%d, %d)\n", __func__,
			data->plat_data->iommu_type, data->plat_data->iommu_id);
		mtk_dump_reg_for_hang_issue(data);
		mtk_smi_dbg_hang_detect("iommu");
		pr_warn("%s, dump: 0x20:0x%x, 0x12c:0x%x\n",
			__func__, readl_relaxed(data->base + REG_MMU_INVALIDATE),
			readl_relaxed(data->base + REG_MMU_CPE_DONE));
		return;
	}
	/* check tlb inv done bit */
	if (!cpe_done) {
		pr_warn("%s, subsys power status is Off!!\n", __func__);
		return;
	}
	pr_info("%s, TLB flush already done\n", __func__);
}

/* Notice!!: Before use it, must be ensure mtcmos is on */
static void mtk_iommu_tlb_flush_all(struct mtk_iommu_data *data)
{
	unsigned long flags;
	int iommu_ids = 0;
	struct list_head *head = data->hw_list;

	for_each_m4u(data, head) {
		bool has_pm = !!data->dev->pm_domain;

		spin_lock_irqsave(&data->tlb_lock, flags);
		if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN)) {
			if ((data->plat_data->iommu_type == MM_IOMMU &&
				pd_sta[data->plat_data->iommu_id] == POWER_OFF_STA) ||
				(data->plat_data->iommu_type != MM_IOMMU &&
				pm_runtime_get_if_in_use(data->dev) <= 0)) {
				spin_unlock_irqrestore(&data->tlb_lock, flags);
				continue;
			}
		}

		iommu_ids |= MTK_IOMMU_ID_FLAG(data->plat_data->iommu_type,
						data->plat_data->iommu_id);

		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + data->plat_data->inv_sel_reg);

		writel_relaxed(F_ALL_INVLD, data->base + REG_MMU_INVALIDATE);
		wmb(); /* Make sure the tlb flush all done */

		if (MTK_IOMMU_HAS_FLAG(data->plat_data, TLB_SYNC_EN)) {
			int ret;
			u32 tmp;
			u32 ctrl_reg = readl_relaxed(data->base + REG_MMU_CTRL_REG);
			u32 sync_en = ctrl_reg & F_MMU_SYNC_INVLDT_EN;

			if (!sync_en) {
				pr_info("skip flush all polling, 0x%x, (%d, %d)\n", ctrl_reg,
					data->plat_data->iommu_type, data->plat_data->iommu_id);
				goto skip_polling;
			}
			/* tlb sync */
			ret = readl_poll_timeout_atomic(data->base + REG_MMU_CPE_DONE,
							tmp, tmp != 0, 10, 1000);
			if (ret) {
				pr_warn("TLB flush All timed out, (%d, %d)\n",
					data->plat_data->iommu_type, data->plat_data->iommu_id);
				mtk_iommu_tlb_flush_check(data, false);
			}
			/* Clear the CPE status */
			writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
		}
skip_polling:
		if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN) &&
			data->plat_data->iommu_type != MM_IOMMU)
			pm_runtime_put(data->dev);
		spin_unlock_irqrestore(&data->tlb_lock, flags);
	}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	mtk_iommu_tlb_sync_trace(0x0, 0x1, iommu_ids);
#endif
}

static void mtk_iommu_tlb_flush_range_sync(unsigned long iova, size_t size,
					   size_t granule,
					   struct mtk_iommu_data *data)
{
	unsigned long flags;
	int iommu_ids = 0;
	int ret;
	u32 tmp;
	bool need_sync_all = false;
	struct mtk_iommu_data *orig_data = data;
	struct list_head *head = data->hw_list;

	for_each_m4u(data, head) {
		bool has_pm = !!data->dev->pm_domain;

		spin_lock_irqsave(&data->tlb_lock, flags);
		if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN)) {
			if ((data->plat_data->iommu_type == MM_IOMMU &&
				pd_sta[data->plat_data->iommu_id] == POWER_OFF_STA) ||
				(data->plat_data->iommu_type != MM_IOMMU &&
				pm_runtime_get_if_in_use(data->dev) <= 0)) {
				spin_unlock_irqrestore(&data->tlb_lock, flags);
				continue;
			}
		}

		iommu_ids |= MTK_IOMMU_ID_FLAG(data->plat_data->iommu_type,
						data->plat_data->iommu_id);

		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + data->plat_data->inv_sel_reg);

		writel_relaxed(MTK_IOMMU_TLB_ADDR(iova),
			       data->base + REG_MMU_INVLD_START_A);
		writel_relaxed(MTK_IOMMU_TLB_ADDR(iova + size - 1),
			       data->base + REG_MMU_INVLD_END_A);
		writel_relaxed(F_MMU_INV_RANGE, data->base + REG_MMU_INVALIDATE);

		/* tlb sync */
		ret = readl_poll_timeout_atomic(data->base + REG_MMU_CPE_DONE,
						tmp, tmp != 0, 10, 1000);
		if (ret) {
			pr_warn("Partial TLB flush timed out, (%d, %d), iova:0x%llx,0x%zx\n",
				data->plat_data->iommu_type, data->plat_data->iommu_id,
				iova, size);
			if (MTK_IOMMU_HAS_FLAG(data->plat_data, TLB_SYNC_EN))
				mtk_iommu_tlb_flush_check(data, true);
			else
				need_sync_all = true;
		}
		/* Clear the CPE status */
		writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
		spin_unlock_irqrestore(&data->tlb_lock, flags);
		if (has_pm && !MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_CLK_AO_EN) &&
			data->plat_data->iommu_type != MM_IOMMU)
			pm_runtime_put(data->dev);
	}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	mtk_iommu_tlb_sync_trace(iova, size, iommu_ids);
#endif
	if (need_sync_all)
		mtk_iommu_tlb_flush_all(orig_data);
}

static void mtk_iommu_dump_tf_iova(struct mtk_iommu_data *data,
		enum iommu_bank bank, u64 fault_iova)
{
	u64 tf_iova_tmp = { 0 };
#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
	// for 32bit legacy 4G chip not use gz hypervisor
	// so discompile those variable to avoid build error
	u64 hw_pa[TAB_TYPE_NUM] = { 0 };
	u32 pg_type[TAB_TYPE_NUM] = { 0 };
	u32 lvl[TAB_TYPE_NUM] = { 0 };
	u32 sr_info[TAB_TYPE_NUM] = { 0 };
#endif
	phys_addr_t fake_pa;
	int i;

	for (i = 0, tf_iova_tmp = fault_iova; i < MTK_IOMMU_TF_IOVA_DUMP_NUM; i++) {
		if (i > 0)
			tf_iova_tmp -= SZ_4K;
		fake_pa = mtk_iommu_iova_to_phys(&data->m4u_dom->domain, tf_iova_tmp);

		// for 32bit legacy 4G chip not use gz hypervisor
#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
		if (hypmmu_type2_en == true)
			hw_pa[NS_TAB] = mtee_iova_to_phys(tf_iova_tmp,
						data->plat_data->tab_id,
						sr_info, hw_pa, pg_type, lvl);
		pr_err("error, type2_en:%d, index:%d, lvl:%u, pg_type:0x%x, falut_iova:0x%lx, fault_pa:0x%llx ~ 0x%llx\n",
			   hypmmu_type2_en, i, lvl[NS_TAB], pg_type[NS_TAB], tf_iova_tmp,
			   (u64)fake_pa, hw_pa[NS_TAB]);
#endif
		if (!fake_pa && i > 0)
			break;
	}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	/* skip dump when fault iova = 0 */
	if (fault_iova)
		mtk_iova_map_dump(fault_iova, data->plat_data->tab_id);
#endif
}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
static irqreturn_t mtk_iommu_dump_sec_bank(struct mtk_iommu_data *data,
		uint32_t bank)
{
	u32 regval;
	int ret;
	int id = data->plat_data->iommu_id;
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	struct device *dev = data->bk_dev[bank];
	u32 va34_32, pa34_32, fault_iova_32 = 0, fault_pa_32 = 0;
	u64 fault_iova, fault_pa;
	bool layer, write;

	if (!MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_SEC_BK_EN)) {
		pr_warn("%s error, bank support is disabled, type:%d, id:%d, bank:%u\n",
			__func__, type, id, bank);
		return IRQ_HANDLED;
	}

	ret = mtk_iommu_secure_bk_tf_dump(type, id, bank, &fault_iova_32,
			&fault_pa_32, &regval);
	if (ret) {
		pr_warn("%s call to TF-A fail, type:%d, id:%d, bank:%u\n",
			__func__, type, id, bank);
		return IRQ_HANDLED;
	}

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOVA_34_EN)) {
		va34_32 = FIELD_GET(F_MMU_INVAL_VA_34_32_MASK, fault_iova_32);
		fault_iova =  (u64)(fault_iova_32 & F_MMU_INVAL_VA_31_12_MASK);
		fault_iova |= (u64)va34_32 << 32;
	} else {
		fault_iova = (u64)fault_iova_32;
	}

	pa34_32 = FIELD_GET(F_MMU_INVAL_PA_34_32_MASK, fault_iova_32);
	fault_pa = (u64)fault_pa_32;
	fault_pa |= (u64)pa34_32 << 32;

	layer = fault_iova & F_MMU_FAULT_VA_LAYER_BIT;
	write = fault_iova & F_MMU_FAULT_VA_WRITE_BIT;
	pr_info("%s fault iova=0x%llx pa=0x%llx layer=%d %s\n",
		dev_name(dev), fault_iova, fault_pa, layer,
		write ? "write" : "read");
	mtk_iommu_dump_tf_iova(data, bank, fault_iova);
	report_custom_iommu_fault(fault_iova, fault_pa, regval, type, id);

	mtk_iommu_tlb_flush_all(data);

	mtk_iommu_isr_record(data);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_iommu_isr_sec(int irq, struct mtk_iommu_data *data)
{
	uint32_t bk;

	for (bk = IOMMU_BK1; bk < IOMMU_BK_NUM; bk++) {
		if (data->bk_irq[bk] == irq) {
			pr_info("%s start, type:%d, id:%d, bank:%u\n",
				__func__, data->plat_data->iommu_type,
				data->plat_data->iommu_id, bk);
			return mtk_iommu_dump_sec_bank(data, bk);
		}
	}

	return IRQ_NONE;
}
#endif

static void peri_iommu_read_data(void __iomem *base, enum peri_iommu iommu_id)
{
	u32 int_state0, int_state1, fault_id, va34_32, pa34_32, regval;
	u64 fault_iova, fault_pa;
	bool layer, write;
	char *port;

	pr_info("%s start, iommu_id:%d, test_data:0x%x\n", __func__, iommu_id,
		readl_relaxed(base + REG_MMU_TBW_ID));

	int_state0 = readl_relaxed(base + REG_MMU_FAULT_ST0);
	int_state1 = readl_relaxed(base + REG_MMU_FAULT_ST1);
	if (!int_state0 && !int_state1) {
		pr_info("%s, peri_iommu_%d no error\n", __func__, iommu_id);
		return;
	}

	if (int_state0 & F_INT_TBW_FAULT) {
		regval = readl_relaxed(base + REG_MMU_TBWALK_FAULT_VA);
		pr_err("%s err, peri_iommu_%d table walk fault, reg:0x%x\n",
		       __func__, iommu_id, regval);
		return;
	}

	if (int_state1 & F_REG_MMU0_INV_PA_MASK)
		pr_info("%s err, peri_iommu_%d invalid pa\n", __func__, iommu_id);

	if (int_state1 & F_REG_MMU0_TF_MASK)
		pr_info("%s err, peri_iommu_%d translation fault\n", __func__, iommu_id);

	fault_id = readl_relaxed(base + REG_MMU0_INT_ID);
	fault_iova = readl_relaxed(base + REG_MMU0_FAULT_VA);
	fault_pa = readl_relaxed(base + REG_MMU0_INVLD_PA);
	layer = fault_iova & F_MMU_FAULT_VA_LAYER_BIT;
	write = fault_iova & F_MMU_FAULT_VA_WRITE_BIT;

	va34_32 = FIELD_GET(F_MMU_INVAL_VA_34_32_MASK, fault_iova);
	fault_iova = fault_iova & F_MMU_INVAL_VA_31_12_MASK;
	fault_iova |= (u64)va34_32 << 32;

	pa34_32 = FIELD_GET(F_MMU_INVAL_PA_34_32_MASK, fault_iova);
	fault_pa |= (u64)pa34_32 << 32;

	pr_info("%s on-going, iommu_id:%d, fault_id:0x%x\n", __func__, iommu_id, fault_id);
	port = peri_tf_analyse(iommu_id, fault_id);
	if (!port) {
		pr_err("%s err, peri_tf_analyse is not support\n", __func__);
		return;
	}
	pr_info("%s done, peri_iommu:%d, port:%s, iova:0x%lx, pa:0x%lx, layer:%d, write:%d\n",
	       __func__, iommu_id, port, fault_iova, fault_pa, layer, write);

	if (int_state1 & F_REG_MMU0_MAU_INT_MASK)
		pr_info("%s err, peri_iommu_%d MAU monitor\n", __func__, iommu_id);

	regval = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	regval |= F_INT_CLR_BIT;
	writel_relaxed(regval, base + REG_MMU_INT_CONTROL0);
}

void mtk_peri_iommu_isr(struct mtk_iommu_data *data, u32 bus_id)
{
	struct list_head *head = data->hw_list;
	enum peri_iommu id = get_peri_iommu_id(bus_id);

	if (id >= PERI_IOMMU_NUM) {
		pr_info("%s, it is not iommu port:%d\n", __func__,  id);
		return;
	}
	for_each_m4u(data, head) {
		if (data->plat_data->iommu_type == PERI_IOMMU &&
		    data->plat_data->iommu_id == id) {
			peri_iommu_read_data(data->base, id);
			break;
		}
	}

	pr_info("%s done type:%d, id:%d\n", __func__,
		data->plat_data->iommu_type, data->plat_data->iommu_id);
	mtk_iommu_tlb_flush_all(data);
	mtk_iommu_isr_record(data);
}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
static void mtk_iommu_mau_init(struct mtk_iommu_data *data);

static int mtk_iommu_mau_dump_status(struct mtk_iommu_data *data,
				     int slave, int mau, bool aee_dump);
#endif

static void mtk_iommu_isr_other(struct mtk_iommu_data *data,
				u32 int_state0, u32 int_state1)
{
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	int id = data->plat_data->iommu_id;
	void __iomem *base = data->base;
	struct device *dev = data->dev;
	unsigned int mau_count;
	u32 va_33_32;
	u64 fault_iova;
	unsigned int layer;
	int slave_id, mau, i;

	if (!int_state0 && !int_state1) {
		pr_info("%s, iommu:(%d,%d) no error\n", __func__, type, id);
		return;
	}

	/* MMU Interrupt Status0 For L2 Related Interrupt Status */
	if (int_state0 & F_REG_MMU_FAULT_ST0_MASK)
		pr_notice("%s, iommu:(%d,%d)_0 int_state0:0x%x happens\n",
			  __func__, type, id, int_state0);

	/* L2 table walk fault */
	if (int_state0 & F_INT_TBW_FAULT) {
		fault_iova = readl_relaxed(base + REG_MMU_TBWALK_FAULT_VA);
		layer = fault_iova & F_L2_TBWALK_FAULT_PAGE_LAYER_BIT;

		if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOVA_34_EN)) {
			va_33_32 = FIELD_GET(F_L2_TBWALK_FAULT_VA_33_32_MASK, fault_iova);
			fault_iova = fault_iova & F_L2_TBWALK_FAULT_VA_31_12_MASK;
			fault_iova |= (u64)va_33_32 << 32;
		}

		dev_warn(dev, "L2 table walk fault: iova=0x%lx, layer=%d\n",
			 fault_iova, layer);
	}

	/* MMU Interrupt Status1 for MTLB Related Interrupt Status */
	if ((int_state1 & F_INT_TRANSLATION_FAULT) == 0) {
		if ((int_state1 & F_REG_MMU0_FAULT_MASK) ||
		    (int_state1 & F_REG_MMU1_FAULT_MASK)) {
			slave_id = (int_state1 & F_REG_MMU0_FAULT_MASK) ? 0 : 1;
			pr_notice("%s, iommu:(%d,%d)_%d int_state1:0x%x happens\n",
				  __func__, type, id, slave_id, int_state1);
		}
	}

	/* MMU Interrupt Status1 for MAU Related Interrupt Status */
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_MAU_EN)) {
		mau_count = data->plat_data->mau_count;

		for (i = 0, slave_id = 0; i < MTK_IOMMU_MMU_COUNT; i++) {
			if (int_state1 & F_REG_MMU_MAU_INT_MASK(i, mau_count)) {
				slave_id = i;
				break;
			}
		}

		if (int_state1 & F_REG_MMU_MAU_INT_MASK(slave_id, mau_count)) {
			pr_notice("%s, iommu:(%d,%d)_%d int_state1:0x%x mau int happens\n",
				  __func__, type, id, slave_id, int_state1);
			for (mau = 0; mau < mau_count; mau++) {
				if (int_state1 & F_INT_MMU_MAU_INT_STA(slave_id, mau, mau_count)) {
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
					mtk_iommu_mau_dump_status(data, slave_id, mau, false);
#endif
				}
			}
		}
	}
}

static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = dev_id;
	struct mtk_iommu_domain *dom = data->m4u_dom;
	void __iomem *base = data->base; /* bank0 base */
	struct device *dev = data->dev; /* bank0 dev */
	u32 int_state0, int_state1, regval, va34_32, pa34_32, table_base;
	u64 fault_iova, fault_pa;
	bool layer, write;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	int ret;
#endif
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	int id = data->plat_data->iommu_id;
	enum mtk_iommu_type type = data->plat_data->iommu_type;
#else
	unsigned int fault_larb, fault_port, sub_comm = 0;
#endif

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_SEC_BK_EN))
		if (mtk_iommu_isr_sec(irq, data) == IRQ_HANDLED)
			return IRQ_HANDLED;

	/* switch to secure debug */
	ret = ao_secure_dbg_switch_by_atf(type, id, 1);
	if (ret)
		pr_notice("%s, iommu:(%d,%d) failed to enable secure debug:%d\n",
			  __func__, type, id, ret);
#endif

	table_base = readl_relaxed(base + REG_MMU_PT_BASE_ADDR);
	/* Read error info from registers */
	int_state0 = readl_relaxed(base + REG_MMU_FAULT_ST0);
	int_state1 = readl_relaxed(base + REG_MMU_FAULT_ST1);

	if (int_state1 & F_INT_TRANSLATION_FAULT) {
		if (int_state1 & F_INT_MMU_TF_ERR(0)) {
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

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
		pr_info("%s, iommu:(%d,%d) reg_raw_data: int_status:0x%x,0x%x, int_id:0x%x, int_va:0x%llx, int_pa:0x%llx\n",
			__func__, type, id, int_state0, int_state1, regval, fault_iova, fault_pa);
#endif
		if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOVA_34_EN)) {
			va34_32 = FIELD_GET(F_MMU_INVAL_VA_34_32_MASK, fault_iova);
			fault_iova = fault_iova & F_MMU_INVAL_VA_31_12_MASK;
			fault_iova |= (u64)va34_32 << 32;
		}
		pa34_32 = FIELD_GET(F_MMU_INVAL_PA_34_32_MASK, fault_iova);
		fault_pa |= (u64)pa34_32 << 32;

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
		pr_info("%s base:0x%x fault type=0x%x iova=0x%llx pa=0x%llx layer=%d %s, pgtable=0x%x, tfrp_pa=0x%llx, 0x114=0x%x\n",
			dev_name(dev), table_base, int_state1, fault_iova, fault_pa,
			layer, (write ? "write" : "read"), dom->cfg.arm_v7s_cfg.ttbr,
			data->protect_base, readl_relaxed(base + REG_MMU_IVRP_PADDR));
		mtk_iommu_dump_tf_iova(data, IOMMU_BK0, fault_iova);
		report_custom_iommu_fault(fault_iova, fault_pa, regval, type, id);
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
				int_state1, fault_iova, fault_pa, fault_larb, fault_port,
				layer, write ? "write" : "read");
		}
#endif
	}

	mtk_iommu_isr_other(data, int_state0, int_state1);

	/* Interrupt clear */
	regval = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	regval |= F_INT_CLR_BIT;
	writel_relaxed(regval, base + REG_MMU_INT_CONTROL0);

	mtk_iommu_tlb_flush_all(data);

	mtk_iommu_isr_record(data);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	ret = ao_secure_dbg_switch_by_atf(type, id, 0);
	if (ret)
		pr_notice("%s, iommu:(%d,%d) failed to disable secure debug:%d\n",
			  __func__, type, id, ret);

#endif
	return IRQ_HANDLED;
}

int dev_is_normal_region(struct device *dev)
{
	struct mtk_iommu_data *data;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	int domid;

	if (!fwspec) {
		pr_err("%s err, dev(%s) is not iommu-dev\n", __func__, dev_name(dev));
		return 0;
	}

	data = dev_iommu_priv_get(dev);
	domid = MTK_M4U_TO_DOM(fwspec->ids[0]);

	pr_info("%s, domid:%d -- %u\n", __func__, domid, data->plat_data->normal_dom);
	return domid == data->plat_data->normal_dom;
}
EXPORT_SYMBOL_GPL(dev_is_normal_region);

static int mtk_iommu_get_domain_id(struct device *dev,
				   const struct mtk_iommu_plat_data *plat_data)
{
	const struct mtk_iommu_iova_region *rgn = plat_data->iova_region;
	const struct bus_dma_region *dma_rgn = dev->dma_range_map;
	struct mtk_iommu_data *data = dev_iommu_priv_get(dev);
	int i, candidate = -1;
	dma_addr_t dma_end;

	if (!data) {
		pr_info("%s fail get iommu data\n", __func__);
		return -EINVAL;
	}

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

	if (!fwspec) {
		pr_info("%s fail get iommu fwspec\n", __func__);
		return;
	}

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

static void register_share_region(const struct mtk_iommu_plat_data *plat_data)
{
	int i;
	const struct mtk_iommu_iova_region *region;
	u32 base_page_no;
	u32 size_in_pages;

	for (i = 0; i < plat_data->iova_region_nr; i++) {
		region = &plat_data->iova_region[i];
		if (region->type == PROTECTED) {
			base_page_no = region->iova_base >> PAGE_SHIFT;
			size_in_pages = region->size >> PAGE_SHIFT;
			pr_info("%s: tab_id:%d, reg base=0x%x size=0x%x\n", __func__,
				plat_data->tab_id, base_page_no, size_in_pages);
			mtee_hypmmu_reg_share_region(base_page_no, size_in_pages,
						plat_data->tab_id);
		}
	}
}

static int mtk_iommu_domain_finalise(struct mtk_iommu_domain *dom,
				     struct mtk_iommu_data *data,
				     unsigned int domid)
{
	struct list_head *head = data->hw_list;
	const struct mtk_iommu_iova_region *region;
	struct mtk_iommu_data *data_temp;

	for_each_m4u(data_temp, head) {
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
#ifdef CONFIG_ARM64
		.ias = MTK_IOMMU_HAS_FLAG(data->plat_data, IOVA_34_EN) ? 34 : 32,
#else
		.ias = 32,
		.oas = 32,
#endif
		.iommu_dev = data->dev,
	};

#ifdef CONFIG_ARM64
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_4GB_MODE))
		dom->cfg.oas = data->enable_4GB ? 33 : 32;
	else
		dom->cfg.oas = 35;
#endif

	if (!MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_NO_SMCCC) &&
	    mtee_hypmmu_type2_enabled()) {
		pr_info("hyp-mmu type2 enabled. turn on coherent_walk\n");
		dom->cfg.coherent_walk = true;
		register_share_region(data->plat_data);
	}

	pr_info("%s, create table data:(%d,%d), dom = %x\n", __func__,
		data->plat_data->iommu_type, data->plat_data->iommu_id, dom);

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

	pr_info("%s, dom:%u, start:%pa, end:%pa, table:0x%x\n",
		__func__, domid,
		&dom->domain.geometry.aperture_start,
		&dom->domain.geometry.aperture_end,
		dom->cfg.arm_v7s_cfg.ttbr);
	return 0;
}

static struct iommu_domain *mtk_iommu_domain_alloc(unsigned int type)
{
	struct mtk_iommu_domain *dom = NULL;
#ifndef CONFIG_ARM64
	static struct iommu_domain *tmp_dom;

	/* For ARM32, arm_iommu_create_mapping() will re-perform
	 * iommu_domain_alloc() with IOMMU_DOMAIN_UNMANAGED at
	 * mtk_iommu_attach_device() -> arm_iommu_create_mapping().
	 * So iommu_domain will be allocated once time at
	 * iommu_group_alloc_default_domain() when IOMMU_DOMAIN_DMA type is
	 * passed in, and to return tmp_dom for arm_iommu_create_mapping().
	 */
	if (type == IOMMU_DOMAIN_UNMANAGED) {
		pr_info("domain alloc returns the dom = %x, tmp_dom = %x", dom, tmp_dom);
		return tmp_dom;
	}

#endif
	if (type != IOMMU_DOMAIN_DMA) {
		pr_err("[%s] domain type is wrong");
		return NULL;
	}

	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom) {
		WARN_ON(1);
		return NULL;
	}

#ifdef CONFIG_ARM64
	if (iommu_get_dma_cookie(&dom->domain)) {
		pr_err("[%s] get_dma_cookie error ", __func__);
		kfree(dom);
		return NULL;
	}

#else
	tmp_dom = &dom->domain;
#endif
	return &dom->domain;
}

static void mtk_iommu_domain_free(struct iommu_domain *domain)
{
	iommu_put_dma_cookie(domain);
	kfree(to_mtk_domain(domain));
}

static int mtk_iommu_set_dev_dma(struct device *dev)
{
	int ret = 0;

	if (!dev)
		return -EINVAL;

	if (!dev->dma_parms) {
		dev->dma_parms = devm_kzalloc(dev,
					      sizeof(*dev->dma_parms),
					      GFP_KERNEL);
		if (!dev->dma_parms) {
			WARN_ON(1);
			return -ENOMEM;
		}
	}

	ret = dma_set_max_seg_size(dev,
				   (unsigned int)DMA_BIT_MASK(34));
	if (ret) {
		dev_info(dev, "Failed to set DMA segment size\n");
		return ret;
	}

	return 0;
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_priv_get(dev), *frstdata;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
#ifndef CONFIG_ARM64
	struct device *m4udev = data->dev;
	int tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
	int domid, ret = 0;

	struct dma_iommu_mapping *mtk_mapping;
	const struct mtk_iommu_iova_region *region;

	frstdata = mtk_iommu_get_first_data(data->hw_list);
#else
	struct device *m4udev;
	int tab_id, domid, ret = 0;

	frstdata = mtk_iommu_get_first_data(data->hw_list);
	if (!data || !dom || !fwspec) {
		pr_info("%s fail get iommu %pa.%pa.%pa\n", __func__,
			data, dom, fwspec);
		return -ENODEV;
	}

	m4udev = data->dev;
	tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
#endif
	domid = mtk_iommu_get_domain_id(dev, data->plat_data);
	if (domid < 0) {
		pr_warn("[%s][name: %s ] seems get wrong domid = %d\n",
		__func__, __LINE__, dev_name(dev), domid);
		return domid;
	}
#ifndef CONFIG_ARM64
	/* To avoid calling __iommu_attach_device() in arm_iommu_attach_device()
	 * repeatedly.
	 */
	if (dev->archdata.dma_ops_setup) {
		pr_warn("[%s][name: %s ] dma_ops_setup = NULL\n", __func__, dev_name(dev));
		return 0;
	}
	mtk_mapping = frstdata->mapping[domid];
	if (!mtk_mapping) {
		region = frstdata->plat_data->iova_region + domid;
		mtk_mapping = arm_iommu_create_mapping(&platform_bus_type,
						       region->iova_base,
						       region->size);
		if (IS_ERR_OR_NULL(mtk_mapping)) {
			dev_err(dev,
				"iommu init arm mapping for rgn %d fail %d\n",
				domid, PTR_ERR(mtk_mapping));
			return -EFAULT;
		}
		frstdata->mapping[domid] = mtk_mapping;
	}
	dev->archdata.dma_ops_setup = true;
	/*
	 * ret = arm_iommu_attach_device(dev, mtk_mapping);
	 * if (ret) {
	 * dev_err(dev, "arm attach device fail %d\n", ret);
	 * dev->archdata.dma_ops_setup = false;
	 * }
	 */

	/* Initialize the max segment size as 4G for device to support
	 * continuous IOVA mapping at ARM32.
	 */
	if (!dev->dma_parms) {
		dev->dma_parms = kzalloc(sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}
	dma_set_max_seg_size(dev, (unsigned int)DMA_BIT_MASK(31));
#endif

	mutex_lock(&init_mutexs[tab_id]);

	if (!dom->data) {
		if (mtk_iommu_domain_finalise(dom, data, domid)) {
			ret = -ENODEV;
			pr_err("[%s][dev %s]iommu_domain_finalise err\n", __func__, dev_name(dev));
			goto out_unlock;
		}
		dom->data = data;
		dom->tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		pr_info("%s, set mtk_iommu_domain, data:(%d,%d), tab_id:%d and dom->data = %x\n",
			__func__, data->plat_data->iommu_type,
			data->plat_data->iommu_id, dom->tab_id, dom->data);
	}

	pr_info("%s, set mtk_iommu_domain, data:(%d,%d), tab_id:%d and dom->data = %x, dom->iop = %x\n",
			__func__, data->plat_data->iommu_type,
			data->plat_data->iommu_id, dom->tab_id, dom->data, dom->iop);

	if (!data->m4u_dom) { /* Initialize the M4U HW */
		ret = pm_runtime_resume_and_get(m4udev);
		if (ret < 0) {
			dev_err(data->dev,
				"%s, PM fail:%d, dom:%d, iommu_dev:(%d,%d), user_dev:%s\n",
				__func__, ret, domid, data->plat_data->iommu_type,
				data->plat_data->iommu_id, dev_name(dev));
			goto out_unlock;
		}
		/*
		 * Because m4u_dom is used by mtk_iommu_isr, we must set it before
		 * enable all banks irq to avoid m4u_dom is NULL.
		 * ex: apu_iommu irq test.
		 */
		data->m4u_dom = dom;
		ret = mtk_iommu_hw_init(data);
		if (ret) {
			dev_err(data->dev, "HW init fail %d in attach\n", ret);
			pm_runtime_put(m4udev);
			goto out_unlock;
		}
		writel(dom->cfg.arm_v7s_cfg.ttbr & MMU_PT_ADDR_MASK,
		       data->base + REG_MMU_PT_BASE_ADDR);
		pr_info("%s, iommu_dev:%s(%d,%d), user_dev:%s, pgtable:0x%lx -- 0x%x -- 0x%x, tab_id:%d\n",
			__func__, dev_name(data->dev), data->plat_data->iommu_type,
			data->plat_data->iommu_id, dev_name(dev),
			(unsigned long)dom->cfg.arm_v7s_cfg.ttbr,
			dom->cfg.arm_v7s_cfg.ttbr,
			readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR),
			dom->tab_id);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
		if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_MAU_EN))
			mtk_iommu_mau_init(data);
#endif
		pm_runtime_put(m4udev);
	}

	mtk_iommu_config(data, dev, true, domid);
	mtk_iommu_set_dev_dma(dev);

out_unlock:
	mutex_unlock(&init_mutexs[tab_id]);
	return ret;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_priv_get(dev);

#ifndef CONFIG_ARM64
	/* To avoid calling __iommu_detach_device() in arm_iommu_detach_device()
	 * repeatedly.
	 */
	/*
	 * if (!dev->archdata.dma_ops_setup)
	 * return;
	 * dev->archdata.dma_ops_setup = false;
	 * arm_iommu_detach_device(dev);
	 */
	kfree(dev->dma_parms);
	dev->dma_parms = NULL;
#else
	if (!data) {
		pr_info("%s fail get iommu data\n", __func__);
		return;
	}
#endif
	mtk_iommu_config(data, dev, false, 0);
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot, gfp_t gfp)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	int ret;
	int retry_count = 0;

	/* The "4GB mode" M4U physically can not use the lower remap of Dram. */
	if (dom->data->enable_4GB)
		paddr |= BIT_ULL(32);

	/* Synchronize with the tlb_lock */
	ret = dom->iop->map(dom->iop, iova, paddr, size, prot, gfp);

	/*
	 * Retry if atomic alloc memory fail, most wait 4ms at atomic or 64ms
	 * at normal.
	 */
	while (ret == -ENOMEM && (gfp & GFP_ATOMIC) != 0 && retry_count < 8) {
		pr_info("%s, retry map alloc memory %d\n", __func__,
			retry_count + 1);
		if (irqs_disabled() || in_interrupt()) {
			ret = dom->iop->map(dom->iop, iova, paddr, size, prot,
					    gfp);
		} else {
			/* if not in atomic ctx, wait memory reclaim. */
			gfp_t ignore_atomic = (gfp & ~GFP_ATOMIC) | GFP_KERNEL;

			ret = dom->iop->map(dom->iop, iova, paddr, size, prot,
					    ignore_atomic);
		}
		if (ret == -ENOMEM) {
			retry_count++;
			if (irqs_disabled() || in_interrupt())
				udelay(500);
			else
				usleep_range(8000, 10*1000);
		}
	}

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
	int ret;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	size_t length = gather->end - gather->start + 1;

	if (gather->start > gather->end) {
		pr_err("%s fail, iova range : 0x%lx ~ 0x%lx\n",
		       __func__, gather->start, gather->end);
		return;
	}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	if (gather->start > 0 && gather->start != ULONG_MAX)
		mtk_iova_unmap(dom->tab_id, gather->start, length);
#endif

	if (!hypmmu_type2_en) {
		ret = iova_secure_map(dom->data, gather->start, length, false); /* clean bank1 */
		if (ret)
			pr_warn("%s failed\n", __func__);
	} else {
		ret = iova_secure_inv(gather->start, length, dom->tab_id);
		if (ret)
			pr_warn("%s failed\n", __func__);
	}

	mtk_iommu_tlb_flush_range_sync(gather->start, length, gather->pgsize,
				       dom->data);
}

static void mtk_iommu_sync_map(struct iommu_domain *domain, unsigned long iova,
			       size_t size)
{
	int ret;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	if (iova > (iova + size)) {
		pr_err("%s fail, iova range : 0x%lx ~ 0x%lx\n",
		       __func__, iova, iova + size);
		return;
	}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	if (iova > 0 && iova != ULONG_MAX)
		mtk_iova_map(dom->tab_id, iova, size);
#endif

	if (!hypmmu_type2_en) {
		ret = iova_secure_map(dom->data, iova, size, true);
		if (ret)
			pr_warn("%s failed\n", __func__);
	} else {
		ret = iova_secure_inv(iova, size, dom->tab_id);
		if (ret)
			pr_warn("%s failed\n", __func__);
	}

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

#ifdef	CONFIG_ARM64
static struct iommu_device *mtk_iommu_probe_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return ERR_PTR(-ENODEV); /* Not a iommu client device */

	data = dev_iommu_priv_get(dev);
	return &data->iommu;
}

#else

/*
 * MTK generation one iommu HW only support one iommu domain, and all the client
 * sharing the same iova address space.
 */
static int mtk_iommu_create_mapping(struct device *dev,
				    struct of_phandle_args *args)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;
	struct platform_device *m4updev;
	struct dma_iommu_mapping *mtk_mapping;
	int ret, domid = 0;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!fwspec) {
		ret = iommu_fwspec_init(dev, &args->np->fwnode, &mtk_iommu_ops);
		if (ret)
			return ret;
		fwspec = dev_iommu_fwspec_get(dev);
	} else if (dev_iommu_fwspec_get(dev)->ops != &mtk_iommu_ops) {
		pr_err("[%s][name: %s ] dev`s ops doesn`t use mtk_iommu_ops = %x\n", __func__,
		__LINE__, dev_name(dev));
		return -EINVAL;
	}

	if (!dev_iommu_priv_get(dev)) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		if (WARN_ON(!m4updev))
			return -EINVAL;

		dev_iommu_priv_set(dev, platform_get_drvdata(m4updev));
	}

	ret = iommu_fwspec_add_ids(dev, args->args, 1);
	if (ret)
		return ret;

	data = dev_iommu_priv_get(dev);
	domid = mtk_iommu_get_domain_id(dev, data->plat_data);
	mtk_mapping = data->mapping[domid];
	if (!mtk_mapping) {
		/* MTK iommu support 4GB iova address space. */
		mtk_mapping = arm_iommu_create_mapping(&platform_bus_type,
						0, 1ULL << 32);
		if (IS_ERR(mtk_mapping))
			return PTR_ERR(mtk_mapping);

		data->mapping[domid] = mtk_mapping;
	}

	return 0;
}

static struct iommu_device *mtk_iommu_probe_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct of_phandle_args iommu_spec;
	struct of_phandle_iterator it;
	struct mtk_iommu_data *data;
	int err;
	int mapping_ret;

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return ERR_PTR(-ENODEV); /* Not a iommu client device */

	of_for_each_phandle(&it, err, dev->of_node, "iommus",
			"#iommu-cells", -1) {
		int count = of_phandle_iterator_args_2(&it, iommu_spec.args,
					MAX_PHANDLE_ARGS);
		iommu_spec.np = of_node_get(it.node);
		iommu_spec.args_count = count;
		mapping_ret = mtk_iommu_create_mapping(dev, &iommu_spec);

		if (mapping_ret != 0) {
			pr_warn("[%s] fail to mtk_iommu_create_mapping ", __func__);
			WARN_ON(1);
		}
		/* dev->iommu_fwspec might have changed */
		fwspec = dev_iommu_fwspec_get(dev);

		of_node_put(iommu_spec.np);
	}

	data = dev_iommu_priv_get(dev);

	return &data->iommu;
}
#endif

static void mtk_iommu_release_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return;

	iommu_fwspec_free(dev);

#ifndef CONFIG_ARM64
	if (to_dma_iommu_mapping(dev))
		arm_iommu_detach_device(dev);
#endif
}

static struct iommu_group *mtk_iommu_device_group(struct device *dev)
{
	struct iommu_group *group;
#ifdef CONFIG_ARM64
	struct mtk_iommu_data *c_data, *data;
	struct list_head *hw_list;
	int domid;

	c_data = dev_iommu_priv_get(dev);
	if (!c_data) {
		pr_info("%s fail get iommu data\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	hw_list = c_data->hw_list;
	data = mtk_iommu_get_first_data(hw_list);
	if (!data)
		return ERR_PTR(-ENODEV);

	domid = mtk_iommu_get_domain_id(dev, data->plat_data);
	if (domid < 0)
		return ERR_PTR(domid);

	mutex_lock(&group_mutexs[domid]);
	group = data->m4u_group[domid];
	if (!group) {
		pr_info("%s create group, data:(%d,%d)-->(%d,%d), dev:%s, domid:%d\n", __func__,
			c_data->plat_data->iommu_type, c_data->plat_data->iommu_id,
			data->plat_data->iommu_type, data->plat_data->iommu_id,
			dev_name(dev), domid);
		group = iommu_group_alloc();
		if (!IS_ERR(group))
			data->m4u_group[domid] = group;
	} else {
		iommu_group_ref_get(group);
	}
	mutex_unlock(&group_mutexs[domid]);

#else
	/* For ARM32, iommu_attach_device() in arm_iommu_attach_device() needs
	 * a proprietary iommu_group for current device.
	 */
	group = iommu_group_alloc();
#endif
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
	const struct mtk_iommu_iova_region *resv, *curdom;
	struct iommu_resv_region *region;
	int prot = IOMMU_WRITE | IOMMU_READ;
	unsigned int domid, i;

	if (!data) {
		pr_info("%s fail get iommu data\n", __func__);
		return;
	}

	domid = mtk_iommu_get_domain_id(dev, data->plat_data);
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
}

#ifndef CONFIG_ARM64

static struct dma_iommu_mapping *
mtk_iommu_get_device_mapping_by_region(struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_priv_get(dev), *frstdata;
	struct list_head *hw_list = data->hw_list;
	int region_id;

	region_id = mtk_iommu_get_domain_id(dev, data->plat_data);
	if (region_id < 0) {
		pr_warn("%s error, region_id is %d < 0\n", __func__, region_id);
		WARN_ON(1);
		return NULL;
	}

	frstdata = mtk_iommu_get_first_data(hw_list);

	return frstdata->mapping[region_id];
}

static void mtk_iommu_probe_finalize(struct device *dev)
{
	struct dma_iommu_mapping *mtk_mapping =
		mtk_iommu_get_device_mapping_by_region(dev);
	int ret = -EFAULT;

	if (mtk_mapping)
		ret = arm_iommu_attach_device(dev, mtk_mapping);

	if (ret) {
		dev_err(dev, "arm attach device fail %d\n", ret);
		dev->archdata.dma_ops_setup = false;
		to_dma_iommu_mapping(dev) = NULL;
	}
}

static int extend_iommu_mapping(struct dma_iommu_mapping *mapping)
{
	int next_bitmap;

	if (mapping->nr_bitmaps >= mapping->extensions)
		return -EINVAL;
	next_bitmap = mapping->nr_bitmaps;
	mapping->bitmaps[next_bitmap] = kzalloc(mapping->bitmap_size,
						GFP_ATOMIC);
	if (!mapping->bitmaps[next_bitmap])
		return -ENOMEM;
	mapping->nr_bitmaps++;
	return 0;
}
/* For reserve iova regions */
static inline int __reserve_iova(struct dma_iommu_mapping *mapping,
				 dma_addr_t iova, size_t size)
{
	unsigned long count, start;
	unsigned long flags;
	int i, sbitmap, ebitmap;

	if (!mapping || iova < mapping->base)
		return -EINVAL;
	start = (iova - mapping->base) >> PAGE_SHIFT;
	count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	sbitmap = start / mapping->bits;
	ebitmap = (start + count) / mapping->bits;
	start = start % mapping->bits;
	if (ebitmap > mapping->extensions)
		return -EINVAL;
	spin_lock_irqsave(&mapping->lock, flags);
	for (i = mapping->nr_bitmaps; i <= ebitmap; i++) {
		if (extend_iommu_mapping(mapping)) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return -ENOMEM;
		}
	}
	for (i = sbitmap; count && i < mapping->nr_bitmaps; i++) {
		int bits = count;

		if (bits + start > mapping->bits)
			bits = mapping->bits - start;
		bitmap_set(mapping->bitmaps[i], start, bits);
		start = 0;
		count -= bits;
	}
	spin_unlock_irqrestore(&mapping->lock, flags);
	return 0;
}
static int arm_iommu_reserve(struct dma_iommu_mapping *mapping, dma_addr_t addr,
			     size_t size)
{
	return __reserve_iova(mapping, addr, size);
}
static void mtk_iommu_apply_resv_region(struct device *dev,
					struct iommu_domain *domain,
					struct iommu_resv_region *region)
{
	struct dma_iommu_mapping *mtk_mapping =
		mtk_iommu_get_device_mapping_by_region(dev);
	int ret = -EFAULT;

	if (mtk_mapping)
		ret = arm_iommu_reserve(mtk_mapping,
					(dma_addr_t)(region->start),
					(size_t)region->length);

	if (ret)
		dev_err(dev, "arm dma rsv iova 0x%lx(0x%x) fail %d\n",
			(unsigned long)region->start,
			(unsigned int)region->length, ret);
}
static int mtk_iommu_def_domain_type(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return IOMMU_DOMAIN_BLOCKED;
	return IOMMU_DOMAIN_DMA;
}
#endif	/* !CONFIG_ARM64 */


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
#ifndef CONFIG_ARM64
	.probe_finalize	= mtk_iommu_probe_finalize,
	.apply_resv_region = mtk_iommu_apply_resv_region,
	.def_domain_type = mtk_iommu_def_domain_type,
#endif
	.pgsize_bitmap	= SZ_4K | SZ_64K | SZ_1M | SZ_16M,
};

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data)
{
	u32 regval;
	int i;

	if (data->plat_data->m4u_plat == M4U_MT8173) {
		regval = F_MMU_PREFETCH_RT_REPLACE_MOD |
			 F_MMU_TF_PROT_TO_PROGRAM_ADDR_MT8173;
	} else {
		regval = readl_relaxed(data->base + REG_MMU_CTRL_REG);
		if (MTK_IOMMU_HAS_FLAG(data->plat_data, TLB_SYNC_EN))
			regval |= F_MMU_SYNC_INVLDT_EN;
		else
			regval |= F_MMU_TF_PROT_TO_PROGRAM_ADDR;
		regval |= F_MMU_MON_EN;
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

	mtk_iommu_bk0_intr_en(data, 1);

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_LEGACY_IVRP_PADDR))
		regval = (data->protect_base >> 1) | (data->enable_4GB << 31);
	else
		regval = lower_32_bits(data->protect_base) |
			upper_32_bits(data->protect_base);
	writel_relaxed(regval, data->base + REG_MMU_IVRP_PADDR);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_SEC_BK_EN))
		mtk_iommu_sec_bk_init_by_atf(data->plat_data->iommu_type,
				data->plat_data->iommu_id);
#endif

	for (i = IOMMU_BK0; i < IOMMU_BK_NUM; i++) {
		struct device *dev;
		unsigned int irq;

		if (i == IOMMU_BK0) {
			dev = data->dev;
			irq = data->irq;
		} else {
			if (!MTK_IOMMU_HAS_FLAG(data->plat_data,
					IOMMU_SEC_BK_EN))
				break;
			dev = data->bk_dev[i];
			irq = data->bk_irq[i];
		}

		if (!irq) {
			pr_err("%s error, irq is 0(%d,%d,%d)\n", __func__,
				data->plat_data->iommu_type,
				data->plat_data->iommu_id, i);
			continue;
		}
		if (devm_request_irq(dev, irq, mtk_iommu_isr, 0, dev_name(dev), (void *)data)) {
			if (i == IOMMU_BK0)
				writel_relaxed(0, data->base + REG_MMU_PT_BASE_ADDR);
			dev_err(dev, "Failed @ IRQ-%d Request\n", irq);
			return -ENODEV;
		}
		pr_info("%s, register irq done %d\n", __func__, irq);
	}

	pr_info("%s done, (%d,%d), dump reg: 0x48:0x%x, 0x50:0x%x, 0x54:0x%x, 0xa0:0x%x, 0x110:0x%x, 0x114:0x%x, 0x120:0x%x, 0x124:0x%x\n",
		__func__,
		data->plat_data->iommu_type,
		data->plat_data->iommu_id,
		readl_relaxed(data->base + REG_MMU_MISC_CTRL),
		readl_relaxed(data->base + REG_MMU_DCM_DIS),
		readl_relaxed(data->base + REG_MMU_WR_LEN_CTRL),
		readl_relaxed(data->base + REG_MMU_TBW_ID),
		readl_relaxed(data->base + REG_MMU_CTRL_REG),
		readl_relaxed(data->base + REG_MMU_IVRP_PADDR),
		readl_relaxed(data->base + REG_MMU_INT_CONTROL0),
		readl_relaxed(data->base + REG_MMU_INT_MAIN_CONTROL));

	return 0;
}

static const struct component_master_ops mtk_iommu_com_ops = {
	.bind		= mtk_iommu_bind,
	.unbind		= mtk_iommu_unbind,
};

static int mtk_iommu_pd_callback(struct notifier_block *nb,
			unsigned long flags, void *data)
{
	unsigned long lock_flags;

	spin_lock_irqsave(&tlb_locks[nb->priority], lock_flags);

	if (flags == GENPD_NOTIFY_ON)
		pd_sta[nb->priority] = POWER_ON_STA;
	else if (flags == GENPD_NOTIFY_PRE_OFF)
		pd_sta[nb->priority] = POWER_OFF_STA;

	spin_unlock_irqrestore(&tlb_locks[nb->priority], lock_flags);

	return NOTIFY_OK;
}

static int mtk_iommu_dbg_hang_cb(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	if (!IS_ERR_OR_NULL(data) && strcmp((char *) data, "iommu") == 0)
		return NOTIFY_DONE;

	mtk_iommu_dbg_hang_detect(MM_IOMMU, DISP_IOMMU);
	mtk_iommu_dbg_hang_detect(MM_IOMMU, MDP_IOMMU);
	return NOTIFY_OK;
}

static int register_dbg_notifier;
static struct notifier_block mtk_iommu_dbg_hang_nb = {
		.notifier_call = mtk_iommu_dbg_hang_cb,
};

/********** mtk iommu MAU start **********/
static inline void iommu_set_field_by_mask(void __iomem *m4u_base,
					   unsigned int reg,
					   unsigned long mask,
					   unsigned int val)
{
	unsigned int regval;

	regval = readl_relaxed(m4u_base + reg);
	regval = (regval & (~mask)) | val;
	writel_relaxed(regval, m4u_base + reg);
}

static inline unsigned int iommu_get_field_by_mask(void __iomem *m4u_base,
						   unsigned int reg,
						   unsigned int mask)
{
	return readl_relaxed(m4u_base + reg) & mask;
}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
static int mtk_iommu_mau_start_monitor(struct mtk_iommu_data *data,
				       unsigned int slave, unsigned int mau,
				       const struct mau_config_info *mau_cfg)
{
	unsigned int mau_count = data->plat_data->mau_count;
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	int id = data->plat_data->iommu_id;
	unsigned long flags;
	void __iomem *base;
	int ret;

	if (slave >= MTK_IOMMU_MMU_COUNT || mau >= mau_count) {
		pr_notice("%s, invalid iommu:(%d,%d), slave:%d, mau:%d\n",
			  __func__, type, id, slave, mau);
		return -1;
	}

	spin_lock_irqsave(&data->tlb_lock, flags);
	if (!mtk_iommu_power_get(data)) {
		pr_notice("%s, iommu:(%d,%d) power off dev:%s\n",
			  __func__, type, id, dev_name(data->dev));
		spin_unlock_irqrestore(&data->tlb_lock, flags);
		return 0;
	}

	/* switch to secure debug */
	ret = ao_secure_dbg_switch_by_atf(type, id, 1);
	if (ret) {
		mtk_iommu_power_put(data);
		spin_unlock_irqrestore(&data->tlb_lock, flags);
		pr_notice("%s, iommu:(%d,%d) failed to enable secure debug:%d\n",
			  __func__, type, id, ret);
		return ret;
	}

	base = data->base;

	/*enable interrupt*/
	iommu_set_field_by_mask(base,
				REG_MMU_INT_MAIN_CONTROL,
				F_INT_MMU_MAU_INT_EN(slave, mau, mau_count),
				F_INT_MMU_MAU_INT_EN(slave, mau, mau_count));

	/*config start addr*/
	writel_relaxed(mau_cfg->start, base +
		       REG_MMU_MAU_SA(slave, mau));
	writel_relaxed(mau_cfg->start_bit32, base +
		       REG_MMU_MAU_SA_EXT(slave, mau));

	/*config end addr*/
	writel_relaxed(mau_cfg->end, base +
		       REG_MMU_MAU_EA(slave, mau));
	writel_relaxed(mau_cfg->end_bit32, base +
		       REG_MMU_MAU_EA_EXT(slave, mau));

	/*config larb id*/
	writel_relaxed(mau_cfg->larb_mask, base +
		       REG_MMU_MAU_LARB_EN(slave, mau));

	/*config port id*/
	writel_relaxed(mau_cfg->port_mask, base +
		       REG_MMU_MAU_PORT_EN(slave, mau));

	/*config input/output*/
	iommu_set_field_by_mask(base, REG_MMU_MAU_IO(slave, mau_count),
				F_MMU_MAU_BIT_VAL(1, mau),
				F_MMU_MAU_BIT_VAL(mau_cfg->io, mau));

	/*config read/write*/
	iommu_set_field_by_mask(base, REG_MMU_MAU_RW(slave, mau_count),
				F_MMU_MAU_BIT_VAL(1, mau),
				F_MMU_MAU_BIT_VAL(mau_cfg->wr, mau));

	/*config PA/VA*/
	iommu_set_field_by_mask(base, REG_MMU_MAU_VA(slave, mau_count),
				F_MMU_MAU_BIT_VAL(1, mau),
				F_MMU_MAU_BIT_VAL(mau_cfg->virt, mau));
	wmb(); /*make sure the MAU ops has been triggered*/

	pr_notice("%s, iommu:(%d,%d), slave:%d, mau:%d, mau_reg: start=0x%x(0x%x), end=0x%x(0x%x), wr:0x%x, virt:0x%x, io:0x%x, larb:0x%x, port:0x%x\n",
		  __func__, type, id, slave, mau,
		  readl_relaxed(base + REG_MMU_MAU_SA(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_SA_EXT(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_EA(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_EA_EXT(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_RW(slave, mau_count)),
		  readl_relaxed(base + REG_MMU_MAU_VA(slave, mau_count)),
		  readl_relaxed(base + REG_MMU_MAU_IO(slave, mau_count)),
		  readl_relaxed(base + REG_MMU_MAU_LARB_EN(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_PORT_EN(slave, mau)));

	ret = ao_secure_dbg_switch_by_atf(type, id, 0);
	if (ret)
		pr_notice("%s, iommu:(%d,%d) failed to disable secure debug:%d\n",
			  __func__, type, id, ret);

	mtk_iommu_power_put(data);
	spin_unlock_irqrestore(&data->tlb_lock, flags);

	return 0;
}

/* Notice: must fill cfg->slave/mau before call this func. */
static int mtk_iommu_mau_get_config(struct mtk_iommu_data *data,
				    struct mau_config_info *mau_cfg)
{
	unsigned int mau_count = data->plat_data->mau_count;
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	int id = data->plat_data->iommu_id;
	int slave = mau_cfg->slave;
	int mau = mau_cfg->mau;
	void __iomem *base;

	if (slave >= MTK_IOMMU_MMU_COUNT || mau >= mau_count) {
		pr_notice("%s, invalid iommu:(%d,%d), slave:%d, mau:%d\n",
			  __func__, type, id, slave, mau);
		return -1;
	}

	base = data->base;

	mau_cfg->start = readl_relaxed(base + REG_MMU_MAU_SA(slave, mau));
	mau_cfg->end = readl_relaxed(base + REG_MMU_MAU_EA(slave, mau));
	mau_cfg->start_bit32 = readl_relaxed(base + REG_MMU_MAU_SA_EXT(slave, mau));
	mau_cfg->end_bit32 = readl_relaxed(base + REG_MMU_MAU_EA_EXT(slave, mau));
	mau_cfg->port_mask = readl_relaxed(base + REG_MMU_MAU_PORT_EN(slave, mau));
	mau_cfg->larb_mask = readl_relaxed(base + REG_MMU_MAU_LARB_EN(slave, mau));

	mau_cfg->io = !!(iommu_get_field_by_mask(base,
						 REG_MMU_MAU_IO(slave, mau_count),
						 F_MMU_MAU_BIT_VAL(1, mau)));

	mau_cfg->wr = !!(iommu_get_field_by_mask(base,
						 REG_MMU_MAU_RW(slave, mau_count),
						 F_MMU_MAU_BIT_VAL(1, mau)));

	mau_cfg->virt = !!(iommu_get_field_by_mask(base,
						   REG_MMU_MAU_VA(slave, mau_count),
						   F_MMU_MAU_BIT_VAL(1, mau)));

	return 0;
}

static int mtk_iommu_mau_dump_status(struct mtk_iommu_data *data,
				     int slave, int mau, bool aee_dump)
{
	unsigned int mau_count = data->plat_data->mau_count;
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	int id = data->plat_data->iommu_id;
	void __iomem *base;
	unsigned long flags;
	unsigned int status;
	unsigned int falut_id, assert_id, assert_address, assert_b32;
	struct mau_config_info mau_cfg;
	char *port_name = NULL;
	int ret;

	if (slave >= MTK_IOMMU_MMU_COUNT || mau >= mau_count) {
		pr_notice("%s, invalid iommu:(%d,%d), slave:%d, mau:%d\n",
			  __func__, type, id, slave, mau);
		return -1;
	}

	spin_lock_irqsave(&data->tlb_lock, flags);
	if (!mtk_iommu_power_get(data)) {
		pr_notice("%s, iommu:(%d,%d), slave:%d, mau:%d, power off dev:%s\n",
			  __func__, type, id, slave, mau, dev_name(data->dev));
		spin_unlock_irqrestore(&data->tlb_lock, flags);
		return 0;
	}

	base = data->base;
	status = readl_relaxed(base + REG_MMU_MAU_ASRT_STA(slave, mau_count));

	if (status & (1 << mau)) {
		pr_notice("%s: iommu:(%d,%d), slave:%d, mau_assert:%d, status:0x%x\n",
			  __func__, type, id, slave, mau, status);

		assert_id = readl_relaxed(base + REG_MMU_MAU_ASRT_ID(slave, mau));
		assert_address = readl_relaxed(base + REG_MMU_MAU_AA(slave, mau));
		assert_b32 = readl_relaxed(base + REG_MMU_MAU_AA_EXT(slave, mau));

		falut_id = (assert_id & F_MMU_MAU_ASRT_ID_VAL) << 2;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
		port_name = mtk_iommu_get_port_name(type, id, falut_id);
		if (aee_dump)
			report_iommu_mau_fault(assert_id, falut_id, (port_name ?
					       port_name : "port_unknown"),
					       assert_address, assert_b32);
#endif
		pr_notice("%s: ASRT_ID=0x%x, FALUT_ID=0x%x(%s), AA=0x%x, AA_EXT=0x%x\n",
			  __func__, assert_id, falut_id,
			  (port_name ? port_name : "port_unknown"),
			  assert_address, assert_b32);

		/* Clear bits for MAU set */
		writel_relaxed((1 << mau), base + REG_MMU_MAU_CLR(slave, mau_count));
		writel_relaxed(0, base + REG_MMU_MAU_CLR(slave, mau_count));
		wmb(); /*make sure the MAU data is cleared*/

		mau_cfg.slave = slave;
		mau_cfg.mau = mau;
		ret = mtk_iommu_mau_get_config(data, &mau_cfg);
		if (!ret) {
			pr_info("%s, iommu:(%d,%d), slave:%d, mau:%d, mau_cfg: start=0x%x(0x%x), end=0x%x(0x%x), wr:0x%x, virt:0x%x, io:0x%x, larb:0x%x, port:0x%x\n",
				__func__, type, id, slave, mau,
				mau_cfg.start, mau_cfg.start_bit32,
				mau_cfg.end, mau_cfg.end_bit32,
				mau_cfg.wr, mau_cfg.virt, mau_cfg.io,
				mau_cfg.larb_mask, mau_cfg.port_mask);
		}
	} else
		pr_notice("%s: mau no assert in MAU set %d, status:0x%x\n",
			  __func__, mau, status);

	mtk_iommu_power_put(data);
	spin_unlock_irqrestore(&data->tlb_lock, flags);
	return 0;
}

static int mtk_iommu_mau_reg_backup(struct mtk_iommu_data *data)
{
	unsigned int mau_count = data->plat_data->mau_count;
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	int id = data->plat_data->iommu_id;
	unsigned int *p_reg, *p_reg_base;
	void __iomem *base = data->base;
	unsigned int real_size;
	int slave, mau, ret;

	if (!reg->mau) {
		pr_notice("%s, iommu:(%d,%d) no memory for backup\n",
			  __func__, type, id);
		return -1;
	}

	/* switch to secure debug */
	ret = ao_secure_dbg_switch_by_atf(type, id, 1);
	if (ret) {
		pr_notice("%s, iommu:(%d,%d) failed to enable secure debug:%d\n",
			  __func__, type, id, ret);
		return ret;
	}

	p_reg_base = reg->mau;
	p_reg = reg->mau;

	for (slave = 0; slave < MTK_IOMMU_MMU_COUNT; slave++) {
		for (mau = 0; mau < mau_count; mau++) {
			*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_SA(slave, mau));
			*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_SA_EXT(slave, mau));
			*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_EA(slave, mau));
			*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_EA_EXT(slave, mau));
			*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_LARB_EN(slave, mau));
			*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_PORT_EN(slave, mau));
		}
		*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_IO(slave, mau_count));
		*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_RW(slave, mau_count));
		*(p_reg++) = readl_relaxed(base + REG_MMU_MAU_VA(slave, mau_count));
	}

	/* disable secure debug */
	ret = ao_secure_dbg_switch_by_atf(type, id, 0);
	if (ret)
		pr_notice("%s, iommu:(%d,%d) failed to disable secure debug:%d\n",
			  __func__, type, id, ret);

	/* check register size (to prevent overflow) */
	real_size = (p_reg - p_reg_base) * sizeof(unsigned int);
	if (real_size > MMU_MAU_REG_BACKUP_SIZE)
		pr_info("%s error: overflow! %d>%d\n",
			__func__, real_size, (int)MMU_MAU_REG_BACKUP_SIZE);

	reg->mau_real_size = real_size;

	return 0;
}

static int mtk_iommu_mau_reg_restore(struct mtk_iommu_data *data)
{
	unsigned int mau_count = data->plat_data->mau_count;
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	int id = data->plat_data->iommu_id;
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	unsigned int *p_reg, *p_reg_base;
	void __iomem *base = data->base;
	unsigned int real_size;
	int slave, mau, ret;

	if (!reg->mau) {
		pr_notice("%s, iommu:(%d,%d) no memory for restore\n",
			  __func__, type, id);
		return -1;
	}

	/* switch to secure debug */
	ret = ao_secure_dbg_switch_by_atf(type, id, 1);
	if (ret) {
		pr_notice("%s, iommu:(%d,%d) failed to enable secure debug:%d\n",
			  __func__, type, id, ret);
		return ret;
	}

	p_reg_base = reg->mau;
	p_reg = reg->mau;

	for (slave = 0; slave < MTK_IOMMU_MMU_COUNT; slave++) {
		for (mau = 0; mau < mau_count; mau++) {
			writel_relaxed(*(p_reg++), base + REG_MMU_MAU_SA(slave, mau));
			writel_relaxed(*(p_reg++), base + REG_MMU_MAU_SA_EXT(slave, mau));
			writel_relaxed(*(p_reg++), base + REG_MMU_MAU_EA(slave, mau));
			writel_relaxed(*(p_reg++), base + REG_MMU_MAU_EA_EXT(slave, mau));
			writel_relaxed(*(p_reg++), base + REG_MMU_MAU_LARB_EN(slave, mau));
			writel_relaxed(*(p_reg++), base + REG_MMU_MAU_PORT_EN(slave, mau));
		}
		writel_relaxed(*(p_reg++), base + REG_MMU_MAU_IO(slave, mau_count));
		writel_relaxed(*(p_reg++), base + REG_MMU_MAU_RW(slave, mau_count));
		writel_relaxed(*(p_reg++), base + REG_MMU_MAU_VA(slave, mau_count));
	}
	wmb(); /*make sure the MVA data is restored*/

	/* disable secure debug */
	ret = ao_secure_dbg_switch_by_atf(type, id, 0);
	if (ret)
		pr_notice("%s, iommu:(%d,%d) failed to disable secure debug:%d\n",
			  __func__, type, id, ret);

	/* check register size (to prevent overflow) */
	real_size = (p_reg - p_reg_base) * sizeof(unsigned int);
	if (real_size != reg->mau_real_size)
		pr_notice("%s error: %d!=%d\n", __func__,
			  real_size, reg->mau_real_size);

	return 0;
}

static void mtk_iommu_mau_init(struct mtk_iommu_data *data)
{
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	int id = data->plat_data->iommu_id;
	const struct mau_config_info *mau_config;
	unsigned int slave, mau;

	pr_info("%s, iommu dev:%s\n", __func__, dev_name(data->dev));

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	for (slave = 0; slave < MTK_IOMMU_MMU_COUNT; slave++) {
		for (mau = 0; mau < data->plat_data->mau_count; mau++) {
			mau_config = mtk_iommu_get_mau_config(type, id, slave, mau);
			if (mau_config != NULL)
				mtk_iommu_mau_start_monitor(data, slave, mau, mau_config);
		}
	}
#endif
}

static void mtk_iommu_update_region(struct mtk_iommu_data *data)
{
	enum mtk_iommu_type type = data->plat_data->iommu_type;
	int id = data->plat_data->iommu_id;
	struct mtk_iommu_iova_region *region = data->plat_data->iova_region;

	/* Reserve 0~1G for support legacy secure IOMMU on MTEE solution */
	if (is_iommu_sec_on_mtee() && type == MM_IOMMU && id == DISP_IOMMU &&
	    region[0].iova_base == 0  && region[0].size == SZ_4G) {
		region[0].iova_base = SZ_1G;
		region[0].size = 0xc0000000;
	}
}
#endif
/**********mtk iommu mau end**********/

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
	bool disp_power_on = 0;

	pr_info("%s start dev:%s\n", __func__, dev_name(dev));
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = dev;
	data->plat_data = of_device_get_match_data(dev);

	if (!atomic_cmpxchg(&init_once_flag, 0, 1)) {
		for (i = 0; i < PGTBALE_NUM; i++)
			mutex_init(&init_mutexs[i]);

		for (i = 0; i < MTK_IOMMU_GROUP_MAX; i++)
			mutex_init(&group_mutexs[i]);
	}

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

		if (!infracfg)
			return -EINVAL;

		if (IS_ERR(infracfg))
			return PTR_ERR(infracfg);

		ret = regmap_read(infracfg, REG_INFRA_MISC, &val);
		if (ret)
			return ret;
		data->enable_4GB = !!(val & F_DDR_4GB_SUPPORT_EN);
	}

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_MAU_EN)) {
		struct mtk_iommu_suspend_reg *reg = &data->reg;

		reg->mau = kmalloc(MMU_MAU_REG_BACKUP_SIZE, GFP_KERNEL | __GFP_ZERO);
		if (!reg->mau)
			return -ENOMEM;
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
		pr_err("%s, %s(%d,%d) can not irq!!\n", __func__, dev_name(dev),
			data->plat_data->iommu_type, data->plat_data->iommu_id);
		return data->irq;
	}
skip_irq:
	/*
	 * Note: we must be find iommu bank from bank1;
	 * And if iommu upstream, we need to merged with bank0.
	 */
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_SEC_BK_EN)) {
		int bk_nr = of_count_phandle_with_args(dev->of_node,
					     "mediatek,iommu_banks", NULL);

		if (bk_nr >= IOMMU_BK_NUM || bk_nr < 0) {
			pr_info("%s, get bank nr fail, %d\n", __func__, bk_nr);
			goto out;
		}
		pr_info("%s, get bank nr:%d\n", __func__, bk_nr);

		for (i = 0; i < bk_nr; i++) {
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
		}
	}
out:
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_BCLK)) {
		data->bclk = devm_clk_get(dev, "bclk");
		if (IS_ERR(data->bclk)) {
			dev_err(dev, "%s,get clk failed\n", __func__);
			return PTR_ERR(data->bclk);
		}
	}

	/* mt6873, mt6893, mt6983, mt6879 */
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
		dev_err(dev, "%s, can't find mediatek,larbs !\n", __func__);
		return larb_nr;
	}

	for (i = 0; i < larb_nr; i++) {
		u32 id;

		larbnode = of_parse_phandle(dev->of_node, "mediatek,larbs", i);
		if (!larbnode) {
			dev_err(dev, "%s, can't find larbnode:%d !\n", __func__, i);
			return -EINVAL;
		}

		if (!of_device_is_available(larbnode)) {
			of_node_put(larbnode);
			continue;
		}

		ret = of_property_read_u32(larbnode, "mediatek,larb-id", &id);
		if (ret)/* The id is consecutive if there is no this property */
			id = i;

		disp_power_on |= of_property_read_bool(larbnode, "init-power-on");

		plarbdev = of_find_device_by_node(larbnode);
		if (!plarbdev) {
			dev_err(dev, "%s, can't find larb dev:%d !\n", __func__, i);
			of_node_put(larbnode);
			return -EPROBE_DEFER;
		}
		data->larb_imu[id].dev = &plarbdev->dev;

		component_match_add_release(dev, &match, release_of,
					    compare_of, larbnode);
	}

	/* Get smi-common dev from the last larb. */
	smicomm_node = of_parse_phandle(larbnode, "mediatek,smi", 0);
	if (!smicomm_node) {
		dev_err(dev, "%s, can't find smicomm_node phase1\n", __func__);
		return -EINVAL;
	}

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, HAS_SMI_SUB_COMM)) {
		int string_nr, ret;
		const char *compat_name = NULL;

repeat:
		pr_info("%s, check smi commom node start, dev:%s\n", __func__, dev_name(dev));
		if (!smicomm_node) {
			dev_err(dev, "%s, can't find smicomm_node phase2\n", __func__);
			return -EINVAL;
		}
		string_nr = of_property_count_strings(smicomm_node, "compatible");
		if (string_nr < 0) {
			dev_err(dev, "%s err, find compatible fail, nr:%d\n", __func__, string_nr);
			return -EINVAL;
		}
		for (i = 0; i < string_nr; i++) {
			ret = of_property_read_string_index(smicomm_node, "compatible",
							i, &compat_name);
			if (ret) {
				dev_err(dev, "%s err, find compatible name fail, ret:%d\n",
				       __func__, ret);
				return -EINVAL;
			}
			pr_info("%s, compatible_name:%s, i:%d\n", __func__, compat_name, i);
			if (strstr(compat_name, "sub-common"))
				break; /* it is sub-common and goto find next node */
		}
		if (i < string_nr) {
			pr_info("%s, find next level node, dev:%s\n", __func__, dev_name(dev));
			smicomm_node = of_parse_phandle(smicomm_node, "mediatek,smi", 0);
			goto repeat;
		}
	}
	plarbdev = of_find_device_by_node(smicomm_node);
	of_node_put(smicomm_node);
	data->smicomm_dev = &plarbdev->dev;
	pr_info("%s, smi_common:%s, iommu_dev:%s\n", __func__,
		dev_name(&plarbdev->dev), dev_name(dev));

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

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, SHARE_PGTABLE)) {
		list_add_tail(&data->list, &m4ulist);
		data->hw_list = &m4ulist;
		share_pgtable = true;
	} else {
		list_add_tail(&data->list, data->plat_data->hw_list);
		data->hw_list = data->plat_data->hw_list;
	}

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

	/* register the notifier for power domain just for mm_iommu */
	if (data->plat_data->iommu_type == MM_IOMMU) {
		int r, iommu_id = data->plat_data->iommu_id;

		mtk_pd_notifiers[iommu_id].notifier_call = mtk_iommu_pd_callback;
		mtk_pd_notifiers[iommu_id].priority = iommu_id;

		/* defaut power on,if larbs has the node "init-power-on" */
		if (disp_power_on) {
			if (MTK_IOMMU_HAS_FLAG(data->plat_data, SAME_SUBSYS)) {
				pd_sta[DISP_IOMMU] = POWER_ON_STA;
				pd_sta[MDP_IOMMU] = POWER_ON_STA;
				pr_info("%s, config power on, (%d,%d)\n", __func__,
					data->plat_data->iommu_type, data->plat_data->iommu_id);
			} else {
				pd_sta[iommu_id] = POWER_ON_STA;
			}
		}

		r = dev_pm_genpd_add_notifier(dev, &mtk_pd_notifiers[iommu_id]);
		tlb_locks[iommu_id] = data->tlb_lock;
		pr_info("%s add_notifier dev:%s, disp_power_on:%d, iommu:%d\n",
			__func__, dev_name(dev), disp_power_on, iommu_id);
		if (r)
			pr_info("%s notifier err, dev:%s\n", __func__, dev_name(dev));
	}

#if IS_ENABLED(CONFIG_MTK_SMI)
	if (data->plat_data->iommu_type == MM_IOMMU) {
		if (register_dbg_notifier != 1) {
			mtk_smi_dbg_register_notifier(&mtk_iommu_dbg_hang_nb);
			register_dbg_notifier = 1;
		}
	}
#endif

	mtk_iommu_isr_pause_timer_init(data);
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	mtk_iommu_update_region(data);
#endif
	pr_info("%s done dev:%s, head:%lx\n", __func__, dev_name(dev),
		(unsigned long)data->hw_list);
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
#ifndef CONFIG_ARM64
	struct mtk_iommu_data *frstdata =
				mtk_iommu_get_first_data(data->hw_list);
	struct dma_iommu_mapping *mtk_mapping;
	int i;
#endif

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
#ifndef CONFIG_ARM64
	for (i = 0; i < MTK_IOMMU_GROUP_MAX; i++) {
		mtk_mapping = frstdata->mapping[i];
		if (mtk_mapping)
			arm_iommu_release_mapping(mtk_mapping);
	}
#endif
	return 0;
}

static int __maybe_unused mtk_iommu_runtime_suspend(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;
	unsigned long flags;

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, PM_OPS_SKIP))
		return 0;

	/* skip read register when hw_init is not finish. */
	if (!data->m4u_dom) {
		clk_disable_unprepare(data->bclk);
		return 0;
	}

	spin_lock_irqsave(&data->tlb_lock, flags);
	if (!mtk_iommu_power_get(data)) {
		pr_notice("%s, iommu:(%d,%d) power off dev:%s\n",
			  __func__, data->plat_data->iommu_type, data->plat_data->iommu_id,
			  dev_name(data->dev));
		spin_unlock_irqrestore(&data->tlb_lock, flags);
		return 0;
	}

	reg->wr_len_ctrl = readl_relaxed(base + REG_MMU_WR_LEN_CTRL);
	reg->misc_ctrl = readl_relaxed(base + REG_MMU_MISC_CTRL);
	reg->dcm_dis = readl_relaxed(base + REG_MMU_DCM_DIS);
	reg->ctrl_reg = readl_relaxed(base + REG_MMU_CTRL_REG);
	reg->int_control0 = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	reg->int_main_control = readl_relaxed(base + REG_MMU_INT_MAIN_CONTROL);
	reg->ivrp_paddr = readl_relaxed(base + REG_MMU_IVRP_PADDR);
	reg->vld_pa_rng = readl_relaxed(base + REG_MMU_VLD_PA_RNG);
	reg->tbw_id = readl_relaxed(base + REG_MMU_TBW_ID);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_SEC_BK_EN))
		mtk_iommu_secure_bk_backup_by_atf(data->plat_data->iommu_type,
				data->plat_data->iommu_id);

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_MAU_EN))
		mtk_iommu_mau_reg_backup(data);
#endif

	mtk_iommu_power_put(data);
	spin_unlock_irqrestore(&data->tlb_lock, flags);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	mtk_iommu_pm_trace(dev, false);
#endif

	clk_disable_unprepare(data->bclk);
	return 0;
}

static int __maybe_unused mtk_iommu_runtime_resume(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	struct mtk_iommu_domain *m4u_dom = data->m4u_dom;
	void __iomem *base = data->base;
	unsigned long flags;
	int ret;

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, PM_OPS_SKIP)) {
		if (data->plat_data->iommu_type == APU_IOMMU)
			mtk_iommu_tlb_flush_all(data);
		return 0;
	}

	ret = clk_prepare_enable(data->bclk);
	if (ret) {
		dev_err(data->dev, "Failed to enable clk(%d) in resume\n", ret);
		return ret;
	}

	/*
	 * Uppon first resume, only enable the clk and return, since the values of the
	 * registers are not yet set.
	 */
	if (!m4u_dom)
		return 0;

	spin_lock_irqsave(&data->tlb_lock, flags);
	if (!mtk_iommu_power_get(data)) {
		pr_notice("%s, iommu:(%d,%d) power off dev:%s\n",
			  __func__, data->plat_data->iommu_type, data->plat_data->iommu_id,
			  dev_name(data->dev));
		spin_unlock_irqrestore(&data->tlb_lock, flags);
		return 0;
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

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_SEC_BK_EN))
		mtk_iommu_secure_bk_restore_by_atf(data->plat_data->iommu_type,
				data->plat_data->iommu_id);

	if (MTK_IOMMU_HAS_FLAG(data->plat_data, IOMMU_MAU_EN))
		mtk_iommu_mau_reg_restore(data);
#endif

	mtk_iommu_power_put(data);
	spin_unlock_irqrestore(&data->tlb_lock, flags);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	mtk_iommu_pm_trace(dev, true);
#endif

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
static int mtk_dump_reg(const struct mtk_iommu_data *data,
	unsigned int start, unsigned int length)
{
	int i = 0;
	void __iomem *base = data->base;

	for (i = 0; i < length; i += 4) {
		if (length - i == 1)
			pr_info("0x%x=0x%x\n",
				start + 4 * i,
				readl_relaxed(base + start + 4 * i));
		else if (length - i == 2)
			pr_info("0x%x=0x%x, 0x%x=0x%x\n",
				start + 4 * i,
				readl_relaxed(base + start + 4 * i),
				start + 4 * (i + 1),
				readl_relaxed(base + start + 4 * (i + 1)));
		else if (length - i == 3)
			pr_info("0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x\n",
				start + 4 * i,
				readl_relaxed(base + start + 4 * i),
				start + 4 * (i + 1),
				readl_relaxed(base + start + 4 * (i + 1)),
				start + 4 * (i + 2),
				readl_relaxed(base + start + 4 * (i + 2)));
		else if (length - i >= 4)
			pr_info("0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x\n",
				start + 4 * i,
				readl_relaxed(base + start + 4 * i),
				start + 4 * (i + 1),
				readl_relaxed(base + start + 4 * (i + 1)),
				start + 4 * (i + 2),
				readl_relaxed(base + start + 4 * (i + 2)),
				start + 4 * (i + 3),
				readl_relaxed(base + start + 4 * (i + 3)));
	}

	return 0;
}

static int mtk_dump_debug_reg_info(const struct mtk_iommu_data *data)
{
	pr_info("---- iommu(%d, %d) debug register ----\n",
		data->plat_data->iommu_type, data->plat_data->iommu_id);
	return mtk_dump_reg(data, REG_MMU_DBG(0), MTK_IOMMU_DEBUG_REG_NR);
}

static int mtk_dump_rs_sta_info(const struct mtk_iommu_data *data, int mmu)
{
	pr_info("---- iommu(%d, %d) mmu%d: RS status register ----\n",
		data->plat_data->iommu_type, data->plat_data->iommu_id, mmu);

	pr_info("--<0x0>iova/bank --<0x4>descriptor --<0x8>2nd-base --<0xc>status\n");
	return mtk_dump_reg(data,
			    REG_MMU_RS_VA(mmu, 0),
			    MTK_IOMMU_RS_COUNT * 4);
}
#endif

static void mtk_dump_reg_for_hang_issue(struct mtk_iommu_data *data)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	int cnt, ret, i, dump_count = 1;
#endif
	void __iomem *base = data->base;

	if (data->base == NULL) {
		pr_err("%s, base is NULL\n",  __func__);
		return;
	}

	base = data->base;
	pr_info("%s start, (%d, %d)\n", __func__, data->plat_data->iommu_type,
		data->plat_data->iommu_id);
	/* control register */
	pr_info("REG_MMU_PT_BASE_ADDR(0x000)	   = 0x%x\n",
		readl_relaxed(base + REG_MMU_PT_BASE_ADDR));
	pr_info("REG_MMU_IVRP_PADDR(0x114)	   = 0x%x\n",
		readl_relaxed(base + REG_MMU_IVRP_PADDR));
	pr_info("REG_MMU_DUMMY(0x044)	   = 0x%x\n",
		readl_relaxed(base + REG_MMU_DUMMY));
	pr_info("REG_MMU_MISC_CTRL(0x048)   = 0x%x\n",
		readl_relaxed(base + REG_MMU_MISC_CTRL));
	pr_info("REG_MMU_DCM_DIS(0x050)	 = 0x%x\n",
		readl_relaxed(base + REG_MMU_DCM_DIS));
	pr_info("REG_MMU_WR_LEN_CTRL(0x054) = 0x%x\n",
		readl_relaxed(base + REG_MMU_WR_LEN_CTRL));
	pr_info("REG_MMU_TBW_ID(0x0A0)	  = 0x%x\n",
		readl_relaxed(base + REG_MMU_TBW_ID));
	pr_info("REG_MMU_CTRL_REG(0x110)   = 0x%x\n",
		readl_relaxed(base + REG_MMU_CTRL_REG));

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	ret = ao_secure_dbg_switch_by_atf(data->plat_data->iommu_type,
			data->plat_data->iommu_id, 1);
	if (ret) {
		pr_err("%s, failed to enable secure debug\n", __func__);
		return;
	}

	for (cnt = 0; cnt < dump_count; cnt++) {
		pr_info("===== the %d time: REG_MMU_STA(0x008) = 0x%x =====\n",
			cnt, readl_relaxed(base + REG_MMU_STA));
		mtk_dump_debug_reg_info(data);
		for (i = 0; i < MTK_IOMMU_MMU_COUNT; i++)
			mtk_dump_rs_sta_info(data, i);
	}

	pr_info("===== dump hang reg end =====\n");

	ret = ao_secure_dbg_switch_by_atf(data->plat_data->iommu_type,
			data->plat_data->iommu_id, 0);
	if (ret)
		pr_err("%s, failed to disable secure debug\n", __func__);
#endif
	pr_info("%s done, (%d, %d)\n", __func__, data->plat_data->iommu_type,
		data->plat_data->iommu_id);
}

void mtk_iommu_dbg_hang_detect(enum mtk_iommu_type type, int id)
{
	struct list_head *hw_list;
	struct mtk_iommu_data *data;
	unsigned long flags;

	if (!share_pgtable) {
		switch (type) {
		case MM_IOMMU:
			hw_list = &mm_iommu_list;
			break;
		case APU_IOMMU:
			hw_list = &apu_iommu_list;
			break;
		default:
			pr_err("%s failed, type is invalid, %d\n",
				__func__, type);
			return;
		}
	} else {
		hw_list = &m4ulist;
	}

	for_each_m4u(data, hw_list) {
		if (data->plat_data->iommu_type == type && data->plat_data->iommu_id == id) {
			spin_lock_irqsave(&data->tlb_lock, flags);
			if (!mtk_iommu_power_get(data)) {
				pr_notice("%s, iommu:(%d,%d) power off dev:%s\n",
					  __func__, type, id, dev_name(data->dev));
				spin_unlock_irqrestore(&data->tlb_lock, flags);
				return;
			}

			mtk_dump_reg_for_hang_issue(data);

			mtk_iommu_power_put(data);
			spin_unlock_irqrestore(&data->tlb_lock, flags);
			return;
		}
	}

	pr_info("%s, (%d,%d) no dump\n", __func__, type, id);
}
EXPORT_SYMBOL_GPL(mtk_iommu_dbg_hang_detect);

static const struct dev_pm_ops mtk_iommu_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_iommu_runtime_suspend, mtk_iommu_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				     pm_runtime_force_resume)
};

static const struct mtk_iommu_plat_data mt6768_data = {
	.m4u_plat      = M4U_MT6768,
	.flags         = HAS_SUB_COMM | OUT_ORDER_WR_EN | WR_THROT_EN |
			 NOT_STD_AXI_MODE | SHARE_PGTABLE,
	.inv_sel_reg   = REG_MMU_INV_SEL_GEN1,
	.iova_region   = single_domain,
	.iova_region_nr = ARRAY_SIZE(single_domain),
	.iommu_id	= DISP_IOMMU,
	.iommu_type     = MM_IOMMU,
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,mt6768-m4u", .data = &mt6768_data},
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
