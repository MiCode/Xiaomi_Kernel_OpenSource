/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MT_IOMMU_PLAT_H__
#define __MT_IOMMU_PLAT_H__

/* ================ register control ================ */
#define F_VAL(val, msb, lsb)	(((val)&((1<<(msb-lsb+1))-1))<<lsb)
#define F_VAL_L(val, msb, lsb) (((val)&((1L<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)	F_VAL(0xffffffff, msb, lsb)
#define F_MSK_L(msb, lsb)	F_VAL_L(0xffffffffffffffff, msb, lsb)
#define F_BIT_SET(bit)		(1<<(bit))
#define F_BIT_VAL(val, bit)	((!!(val))<<(bit))
#define F_MSK_SHIFT(regval, msb, lsb) (((regval)&F_MSK(msb, lsb))>>lsb)

inline void iommu_set_field_by_mask(void __iomem *M4UBase,
					   unsigned int reg,
					   unsigned long mask,
					   unsigned int val)
{
	unsigned int regval;

	regval = readl_relaxed(M4UBase + reg);
	regval = (regval & (~mask)) | val;
	writel_relaxed(regval, M4UBase + reg);
}

/* ================ register defination ================ */
#define REG_MMU_PT_BASE_ADDR	(0x0)
#define REG_MMU_PT_BASE_SEC_ADDR	(0x4)
#define F_MMU_PT_VA_MSK	0xffffff80
#define F_MMU_PT_VA_MSK_SEC	0xffffff80
#define F_PGD_REG_BIT32     F_BIT_SET(0)
#define F_PGD_REG_BIT33     F_BIT_SET(1)
#define REG_MMU_INVALIDATE	(0x20)
#define F_ALL_INVLD	(0x2)
#define F_MMU_INV_RANGE	(0x1)

#define REG_MMU_INVLD_START_A	(0x24)
#define REG_MMU_INVLD_END_A	(0x28)

#define REG_MMU_INV_SEL	(0x2c)
#define F_INVLD_EN0	BIT(0)
#define F_INVLD_EN1	BIT(1)

#define REG_INVLID_SEL	(0x38)
#define F_MMU_INV_EN_L2	 (1<<1)
#define F_MMU_INV_EN_L1	 (1<<0)

#define REG_MMU_DUMMY	0x44
#define F_REG_MMU_IDLE_ENABLE	F_BIT_SET(0)

#define REG_MMU_MISC_CRTL	(0x48)
#define REG_MMU0_STANDARD_AXI_MODE  F_BIT_SET(0)
#define REG_MMU1_STANDARD_AXI_MODE  F_BIT_SET(1)

#define REG_MMU_DCM_DIS	(0x50)

#define REG_MMU_WR_LEN	  (0x54)
#define F_MMU_MMU0_WR_THROT_DIS    F_BIT_SET(10)
#define F_MMU_MMU1_WR_THROT_DIS    F_BIT_SET(11)
#define F_MMU_MMU1_WRITE_LEN    F_MSK(9, 5)
#define F_MMU_MMU0_WRITE_LEN    F_MSK(4, 0)

#define REG_MMU_COHERENCE_EN    (0x80)
#define F_MMU_MMU0_COHERENCE_EN    F_BIT_SET(0)
#define F_MMU_MMU1_COHERENCE_EN    F_BIT_SET(1)
#define REG_MMU_IN_ORDER_WR_EN    (0x84)
#define F_MMU_MMU0_IN_ORDER_WR_EN    F_BIT_SET(0)
#define F_MMU_MMU1_IN_ORDER_WR_EN    F_BIT_SET(1)


#define REG_MMU_CTRL_REG	  (0x110)
#define F_MMU_CTRL_VICT_TLB_EN(en)	F_BIT_VAL(en, 10)
#define F_MMU_CTRL_HANG_PREVENTION(en)	F_BIT_VAL(en,  9)
#define F_MMU_CTRL_INVALID_FIFO_EN(en)	F_BIT_VAL(en,  8)
#define F_MMU_CTRL_LAYER2_PFH_DIS(dis)	F_BIT_VAL(dis, 7)
#define F_MMU_CTRL_INT_HANG_EN(en)		F_BIT_VAL(en,  6)
#define F_MMU_CTRL_TF_PROTECT_SEL(en)	F_VAL(en, 5, 4)
#define F_MMU_CTRL_MONITOR_CLR(clr)		F_BIT_VAL(clr, 2)
#define F_MMU_CTRL_MONITOR_EN(en)		F_BIT_VAL(en,  1)
#define F_MMU_CTRL_PFH_DIS(dis)		F_BIT_VAL(dis, 0)

#define REG_MMU_IVRP_PADDR	  (0x114)
#define F_MMU_IVRP_PA_SET(PA, EXT) (\
	(((unsigned long long)PA) & F_MSK(31, 7)) | \
	 ((((unsigned long long)PA) >> 32) & F_MSK(1, 0)))
#define F_RP_PA_REG_BIT32     F_BIT_SET(0)
#define F_RP_PA_REG_BIT33     F_BIT_SET(1)

#define REG_MMU_INT_CONTROL0	  (0x120)
#define F_L2_MULIT_HIT_EN	  BIT(0)
#define F_TABLE_WALK_FAULT_INT_EN	  BIT(1)
#define F_PREETCH_FIFO_OVERFLOW_INT_EN	  BIT(2)
#define F_MISS_FIFO_OVERFLOW_INT_EN	  BIT(3)
#define F_L2_INVALIDATION_DONE_INT_EN	  BIT(4)
#define F_PREFETCH_FIFO_ERR_INT_EN	  BIT(5)
#define F_MISS_FIFO_ERR_INT_EN	  BIT(6)
#define F_INT_CLR_BIT	  BIT(12)

#define REG_MMU_INT_MAIN_CONTROL	  (0x124)
#define F_INT_TRANSLATION_FAULT(MMU)	  BIT(0+(MMU)*7)
#define F_INT_MAIN_MULTI_HIT_FAULT(MMU)	  BIT(1+(MMU)*7)
#define F_INT_INVALID_PA_FAULT(MMU)	  BIT(2+(MMU)*7)
#define F_INT_ENTRY_REPLACEMENT_FAULT(MMU)	  BIT(3+(MMU)*7)
#define F_INT_TLB_MISS_FAULT(MMU)	  BIT(4+(MMU)*7)
#define F_INT_MISS_TRANSACTION_FIFO_FAULT(MMU)	  BIT(5+(MMU)*7)
#define F_INT_PRETETCH_TRANSATION_FIFO_FAULT(MMU)	  BIT(6+(MMU)*7)

#define F_INT_MMU0_MAIN_MSK	  F_MSK(6, 0)
#define F_INT_MMU1_MAIN_MSK	  F_MSK(13, 7)
#define F_INT_MMU0_MAU_MSK	  F_MSK(14, 14)
#define F_INT_MMU1_MAU_MSK	  F_MSK(15, 15)

#define REG_MMU_CPE_DONE	  (0x12C)

#define REG_MMU_L2_FAULT_ST	  (0x130)
#define REG_MMU_FAULT_ST1	  (0x134)

#define REG_MMU_FAULT_VA(mmu)	  (0x13c+((mmu)<<3))
#define F_MMU_FAULT_VA_MSK	  F_MSK(31, 12)
#define F_MMU_FAULT_CATCH_INVPA    F_BIT_SET(11)
#define F_MMU_FAULT_CATCH_TF    F_BIT_SET(10)
#define F_MMU_FAULT_PA_B33_32   F_MSK(9, 8)
#define F_MMU_FAULT_VA_WRITE_BIT	  F_BIT_SET(1)
#define F_MMU_FAULT_VA_LAYER_BIT	  F_BIT_SET(0)

#define REG_MMU_INVLD_PA(mmu)	  (0x140+((mmu)<<3))
#define REG_MMU_INT_ID(mmu)	  (0x150+((mmu)<<2))
#define F_MMU0_INT_ID_TF_MSK	  (~0x3) /* only for MM iommu.*/
#define F_MMU_INT_TF_MSK	  F_MSK(11, 0)

/* bit[9:7] indicate larbid */
#define F_MMU0_INT_ID_LARB_ID(a)	  (((a) >> 7) & 0xf)
/*
 * bit[6:2] indicate portid, bit[1:0] indicate master id, every port
 * have four types of command, master id indicate the m4u port's command
 * type, iommu do not care about this.
 */
#define F_MMU0_INT_ID_PORT_ID(a)	  (((a) >> 2) & 0x1f)

/*
 * reserved IOVA Domain for IOMMU users of HW limitation.
 */
#define MTK_IOVA_DOMAIN_COUNT (2)	// Domain count
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
	unsigned long resv_start;
	unsigned long resv_size;
	unsigned int resv_type;
	unsigned int port_mask[MTK_IOMMU_LARB_NR];
};

#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
const struct mtk_iova_domain_data mtk_domain_array[MTK_IOVA_DOMAIN_COUNT] = {
#if (defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	defined(CONFIG_MTK_CAM_SECURITY_SUPPORT))
	{
	 .min_iova = 0x48000000,
	 .max_iova = DMA_BIT_MASK(32),
	 .resv_start = 0x0,
	 .resv_size = 0x0,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0xff, 0x1ff, 0xfff, 0xe7fff, 0x7ff}
	},
#else
	{
	 .min_iova = 0x1000,
	 .max_iova = DMA_BIT_MASK(32),
	 .resv_start = 0x40000000,
	 .resv_size = 0x8000000,
	 .resv_type = IOVA_REGION_REMOVE,
	 .port_mask = {0xff, 0x1ff, 0xfff, 0xe7fff, 0x7ff}
	},
#endif
	{
	 .min_iova = 0x40000000,
	 .max_iova = 0x48000000 - 1,
	 .resv_start = 0x0,
	 .resv_size = 0x0,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x118000, 0x0}
	}
};

#define MTK_IOMMU_PAGE_TABLE_SHARE (1)

#endif
