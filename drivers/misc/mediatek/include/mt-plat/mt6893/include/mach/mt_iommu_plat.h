/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT_IOMMU_PLAT_H__
#define __MT_IOMMU_PLAT_H__

#include "clk-mt6885-pg.h"

#define MMU0_SET_ORDER   7
#define MMU1_SET_ORDER   7
#define MMU_SET_ORDER(mmu)      ((mmu == 0) ? MMU0_SET_ORDER : MMU1_SET_ORDER)
#define MTK_IOMMU_SET_NR(mmu)    (1<<MMU_SET_ORDER(mmu))
#define MMU_SET_LSB_OFFSET	       15
#define MMU_SET_MSB_OFFSET(mmu)	 (MMU_SET_LSB_OFFSET+MMU_SET_ORDER(mmu)-1)
#define MMU_PFH_VA_TO_SET(mmu, va)     \
	F_MSK_SHIFT(va, MMU_SET_MSB_OFFSET(mmu), MMU_SET_LSB_OFFSET)
#define IOMMU_DESIGN_OF_BANK

#ifdef CONFIG_FPGA_EARLY_PORTING
static unsigned int g_tag_count[MTK_IOMMU_M4U_COUNT] = {64};
#else
static unsigned int g_tag_count[MTK_IOMMU_M4U_COUNT] = {64, 64, 64, 64};
#endif

char *smi_clk_name[MTK_IOMMU_LARB_NR] = {
	"iommu_disp_larb0", "iommu_disp_larb1", "iommu_mdp_larb2",
	"iommu_mdp_larb3", "iommu_mdp_larb4", "iommu_disp_larb5",
	"iommu_null_larb6", "iommu_disp_larb7", "iommu_mdp_larb8",
	"iommu_mdp_larb9", "iommu_null_larb10", "iommu_disp_larb11",
	"iommu_null_larb12", "iommu_mdp_larb13", "iommu_disp_larb14",
	"iommu_null_larb15", "iommu_mdp_larb16", "iommu_disp_larb17",
	"iommu_mdp_larb18", "iommu_disp_larb19", "iommu_disp_larb20",
	"iommu_null_larb21", "iommu_null_larb22", "iommu_null_larb23"
};

enum subsys_id iommu_mtcmos_subsys[MTK_IOMMU_M4U_COUNT] = {
	SYS_DIS, SYS_MDP, SYS_VPU, SYS_VPU
};

unsigned int port_size_not_aligned[] = {
	M4U_PORT_L21_APU_FAKE_CODE
};
const char *smi_larb_id = "mediatek,larb-id";

#ifdef CONFIG_FPGA_EARLY_PORTING
char *iommu_secure_compatible[MTK_IOMMU_M4U_COUNT] = {
	"mediatek,sec_m4u",
};
#else
char *iommu_secure_compatible[MTK_IOMMU_M4U_COUNT] = {
	"mediatek,sec_m4u0", "mediatek,sec_m4u1",
	"mediatek,sec_m4u2", "mediatek,sec_m4u3",
};
#ifdef IOMMU_DESIGN_OF_BANK
char *iommu_bank_compatible[MTK_IOMMU_M4U_COUNT][MTK_IOMMU_BANK_NODE_COUNT] = {
	{"mediatek,bank1_m4u0", "mediatek,bank2_m4u0", "mediatek,bank3_m4u0"},
	{"mediatek,bank1_m4u1", "mediatek,bank2_m4u1", "mediatek,bank3_m4u1"},
	{"mediatek,bank1_m4u2", "mediatek,bank2_m4u2", "mediatek,bank3_m4u2"},
	{"mediatek,bank1_m4u3", "mediatek,bank2_m4u3", "mediatek,bank3_m4u3"},
};
#endif
#endif

/* ================ register control ================ */
#define F_VAL(val, msb, lsb)	(((val)&((1<<(msb-lsb+1))-1))<<lsb)
#define F_VAL_L(val, msb, lsb) (((val)&((1L<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)	F_VAL(0xffffffff, msb, lsb)
#define F_MSK_L(msb, lsb)	F_VAL_L(0xffffffffffffffff, msb, lsb)
#define F_BIT_SET(bit)		(1<<(bit))
#define F_BIT_VAL(val, bit)	((!!(val))<<(bit))
#define F_MSK_SHIFT(regval, msb, lsb) (((regval)&F_MSK(msb, lsb))>>lsb)
#define F_MSK_SHIFT64(regval, msb, lsb) \
	((((unsigned long)regval)&F_MSK(msb, lsb))>>lsb)

#ifdef IOMMU_DESIGN_OF_BANK
/* m4u atf debug parameter */
#define IOMMU_ATF_INDEX_MASK     F_MSK(3, 0)
#define IOMMU_ATF_BANK_MASK      F_MSK(7, 4)
#define IOMMU_ATF_CMD_MASK       F_MSK(16, 8)
#define IOMMU_ATF_SET_COMMAND(m4u, bank, cmd) \
	(m4u | (bank << 4) | (cmd << 8))

enum IOMMU_ATF_CMD {
	IOMMU_ATF_DUMP_SECURE_REG,
	IOMMU_ATF_SECURITY_DEBUG_ENABLE,
	IOMMU_ATF_SECURITY_DEBUG_DISABLE,
	IOMMU_ATF_BANK_ENABLE_TF,
	IOMMU_ATF_BANK_DISABLE_TF,
	IOMMU_ATF_BANK_DUMP_INFO,
	IOMMU_ATF_BANK_DOMAIN_CONFIG,
	IOMMU_ATF_SECURITY_BACKUP,
	IOMMU_ATF_SECURITY_RESTORE,
	IOMMU_ATF_DUMP_SECURE_PORT_CONFIG,
	IOMMU_ATF_CMD_COUNT
};

char *iommu_atf_cmd_name[IOMMU_ATF_CMD_COUNT] = {
	"IOMMU_ATF_DUMP_SECURE_REG",
	"IOMMU_ATF_SECURITY_DEBUG_ENABLE",
	"IOMMU_ATF_SECURITY_DEBUG_DISABLE",
	"IOMMU_ATF_BANK_ENABLE_TF",
	"IOMMU_ATF_BANK_DISABLE_TF",
	"IOMMU_ATF_BANK_DUMP_INFO",
	"IOMMU_ATF_BANK_DOMAIN_CONFIG",
	"IOMMU_ATF_SECURITY_BACKUP",
	"IOMMU_ATF_SECURITY_RESTORE",
	"IOMMU_ATF_DUMP_SECURE_PORT_CONFIG",
};
#endif
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

static inline unsigned int iommu_get_field_by_mask(
		void __iomem *M4UBase, unsigned int reg,
						   unsigned int mask)
{
	return readl_relaxed(M4UBase + reg) & mask;
}

/* regiters of BANK0 */
#define REG_MMU_PT_BASE_ADDR	(0x0)
#define F_MMU_PT_BASE_ADDR_MSK		F_MSK(31, 7)
#define F_MMU_PT_BASE_ADDR_BIT32	F_MSK(2, 0)

#define REG_MMU_STA	(0x8)
#define F_MMU_STA_INT_MASK	F_MSK(18, 0)

#define REG_MMU_PROG_EN	(0x10)
#define F_MMU_PROG_EN_TLB1	F_BIT_SET(1)
#define F_MMU_PROG_EN_TLB0	F_BIT_SET(0)

#define REG_MMU_PROG_VA	(0x14)
#define F_MMU_PROG_VA_TBL_ID	F_MSK(1, 0)
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
#define F_MMU_PROG_VA_BIT32	F_MSK(6, 5)
#endif
#define F_MMU_PROG_VA_SECURE	F_BIT_SET(7)
#define F_MMU_PROG_VA_SIZE16X	F_BIT_SET(8)
#define F_MMU_PROG_VA_LAYER	F_BIT_SET(9)
#define F_MMU_PROG_VA_LOCK	F_BIT_SET(11)
#define F_MMU_PROG_VA_BIT31_12	F_MSK(31, 12)

#define REG_MMU_PROG_DSC	(0x18)

#define REG_MMU_INVLDT	(0x20)
#define F_MMU_INVLDT_RNG	F_BIT_SET(0)
#define F_MMU_INVLDT_ALL	F_BIT_SET(1)
#define F_MMU_INVLDT_VICT_ALL	F_BIT_SET(2)

#define REG_MMU_INVLD_START_A	(0x24)
#define REG_MMU_INVLD_END_A	(0x28)
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
#define F_MMU_INVLD_BIT32	F_MSK(1, 0)
#endif
#define F_MMU_INVLD_BIT31_12	F_MSK(31, 12)

#define REG_INVLID_SEL	(0x2c)
#define F_MMU_INV_EN_L2	 F_BIT_SET(1)
#define F_MMU_INV_EN_L1	 F_BIT_SET(0)

#define REG_MMU_DUMMY	0x44
#define F_REG_MMU_IDLE_ENABLE	F_BIT_SET(0)

#define REG_MMU_MISC_CTRL	(0x48)
#define F_MMU_MISC_CTRL_COHERENCE_EN(MMU)	(F_BIT_SET(0) << (MMU * 16))
#define F_MMU_MISC_CTRL_IN_ORDER_WR_EN(MMU)	(F_BIT_SET(1) << (MMU * 16))
#define F_MMU_MISC_CTRL_STD_AXI_MODE(MMU)	(F_BIT_SET(3) << (MMU * 16))
#define F_MMU_MISC_CTRL_BLOCKING_MODE(MMU)	(F_BIT_SET(4) << (MMU * 16))
#define F_MMU_MISC_CTRL_HALF_ENTRY_MODE(MMU)	(F_BIT_SET(5) << (MMU * 16))

#define REG_MMU_PRIORITY	(0x4c)
#define F_MMU_PRIORITY_WALK_ULTRA	F_BIT_SET(0)
#define F_MMU_PRIORITY_AUTO_PF_ULTRA	F_BIT_SET(1)
#define F_MMU_PRIORITY_RS_FULL_ULTRA	F_BIT_SET(2)

#define REG_MMU_DCM_DIS	(0x50)

#define REG_MMU_WR_LEN_CTRL	  (0x54)
#define F_MMU_WR_LEN_CTRL_THROT_DIS(MMU)    (F_BIT_SET(5) << (MMU * 0x10))
#define F_MMU_WR_LEN_CTRL_LEN(MMU)    (F_MSK(4, 0) << (MMU * 0x10))

#define REG_MMU_HW_DEBUG	(0x58)
#define F_MMU_HW_DEBUG_PFQ_BD_EN	F_BIT_SET(0)
#define F_MMU_HW_DEBUG_L2_SCAN_ALL_INVLDT	F_BIT_SET(1)

#define REG_MMU_DBG(index)	(0x60 + index * 4)

#define REG_MMU_COH_THROT_CTRL	(0x90)
#define F_MMU_COH_THROT_CTRL_REN0	F_BIT_SET(0)
#define F_MMU_COH_THROT_CTRL_RLMT0	F_MSK(3, 2)
#define F_MMU_COH_THROT_CTRL_WEN0	F_BIT_SET(4)
#define F_MMU_COH_THROT_CTRL_WLMT0	F_MSK(7, 6)
#define F_MMU_COH_THROT_CTRL_REN1	F_BIT_SET(8)
#define F_MMU_COH_THROT_CTRL_RLMT1	F_MSK(11, 10)
#define F_MMU_COH_THROT_CTRL_WEN1	F_BIT_SET(12)
#define F_MMU_COH_THROT_CTRL_WLMT1	F_MSK(15, 14)

#define REG_MMU_TBW_ID	(0xA0)
#define F_MMU_TBW_ID_MISS	F_MSK(15, 0)
#define F_MMU_TBW_ID_PFTH	F_MSK(31, 16)

#define REG_MMU_READ_ENTRY	(0x100)
#define F_READ_ENTRY_EN		 F_BIT_SET(31)
#define F_READ_ENTRY_VICT_TLB_SEL F_BIT_SET(13)
#define F_READ_ENTRY_MMx_MAIN(id)       F_BIT_SET(27+id)
#define F_READ_ENTRY_PFH		F_BIT_SET(26)
#define F_READ_ENTRY_MAIN_IDX(mmu, idx)     \
	F_VAL(idx, 19 + mmu * 6, 14 + mmu * 6)
#define F_READ_ENTRY_PFH_IDX(idx)       F_VAL(idx, 11, 5)
#define F_READ_ENTRY_PFH_PAGE_IDX(idx)    F_VAL(idx, 4, 2)
#define F_READ_ENTRY_PFH_WAY(way)       F_VAL(way, 1, 0)

#define REG_MMU_DES_RDATA	(0x104)
#define REG_MMU_PFH_TAG_RDATA			(0x108)
#define F_PFH_TAG_VA_GET(mmu, tag)    \
	(F_MSK_SHIFT64(tag, 14, 3)<<(MMU_SET_MSB_OFFSET(mmu)+1))
#define F_VIC_TAG_VA_GET_L0(mmu, tag)    \
	(F_MSK_SHIFT(tag, 23, 17)<<(MMU_SET_MSB_OFFSET(mmu)+2))
#define F_VIC_TAG_VA_GET_L1(mmu, tag)    \
	(F_MSK_SHIFT(tag, 23, 17)<<(MMU_SET_MSB_OFFSET(mmu)-6))
#define F_MMU_PFH_TAG_VA_LAYER0_MSK(mmu)  \
	((mmu == 0)?F_MSK(31, 28):F_MSK(31, 28))
#define F_PFH_PT_BANK_BIT	 F_MSK(15, 16)
#define F_PFH_TAG_LAYER_BIT	 F_BIT_SET(2)
#define F_PFH_TAG_SEC_BIT	   F_BIT_SET(1)
#define F_PFH_TAG_AUTO_PFH	  F_BIT_SET(0)


#define REG_MMU_VICT_VLD (0x10c)
#define F_MMU_VICT_VLD_BIT(way)      F_BIT_SET((way)&0xf)


#define REG_MMU_CTRL_REG	  (0x110)
#define F_MMU_CTRL_4GB_MODE(mode)	F_BIT_VAL(mode, 17)
#define F_MMU_CTRL_RESEND_L2_EN(en)	F_BIT_VAL(en, 16)
#define F_MMU_CTRL_RESEND_PFQ_EN(en)	F_BIT_VAL(en, 15)
#define F_MMU_CTRL_L2TLB_EN(en)		F_BIT_VAL(en, 14)
#define F_MMU_CTRL_MTLB_EN(en)		F_BIT_VAL(en, 13)
#define F_MMU_CTRL_VICT_TLB_EN(en)	F_BIT_VAL(en, 12)
#define F_MMU_CTRL_VICT_TLB_EN(en)	F_BIT_VAL(en, 12)
#define F_MMU_CTRL_HANG_PREVENTION(en)	F_BIT_VAL(en,  9)
#define F_MMU_CTRL_LAYER2_PFH_DIS(dis)	F_BIT_VAL(dis, 7)
#define F_MMU_CTRL_INT_FREEZE_EN(en)	F_BIT_VAL(en, 6)
#define F_MMU_CTRL_MONITOR_CLR(clr)	F_BIT_VAL(clr, 2)
#define F_MMU_CTRL_MONITOR_EN(en)	F_BIT_VAL(en,  1)
#define F_MMU_CTRL_PFH_DIS(dis)		F_BIT_VAL(dis, 0)

#define REG_MMU_TFRP_PADDR	  (0x114)
#define F_RP_PA_REG_BIT31_7     F_MSK(31, 7)
#define F_RP_PA_REG_BIT32    F_MSK(2, 0)
#define F_MMU_TFRP_PA_SET(PA, EXT) (\
	(((unsigned long long)PA) & F_RP_PA_REG_BIT31_7) | \
	 ((((unsigned long long)PA) >> 32) & F_RP_PA_REG_BIT32))

#define REG_MMU_INT_CONTROL0	  (0x120)
#define F_INT_L2_MULTI_HIT_FAULT		   F_BIT_SET(0)
#define F_INT_L2_TABLE_WALK_FAULT		  F_BIT_SET(1)
#define F_INT_L2_PFH_DMA_FIFO_OVERFLOW	     F_BIT_SET(2)
#define F_INT_L2_MISS_DMA_FIFO_OVERFLOW	    F_BIT_SET(3)
#define F_INT_L2_INVALID_DONE		       F_BIT_SET(4)
#define F_INT_L2_PFH_FIFO_ERROR		    F_BIT_SET(5)
#define F_INT_L2_MISS_FIFO_ERR		     F_BIT_SET(6)
#define F_INT_CTL0_L2_CDB_SLICE_ERR	  BIT(7)
#define F_INT_CTL0_INT_CLR	  BIT(12)

#define REG_MMU_INT_MAIN_CONTROL	  (0x124)
#define F_INT_TRANSLATION_FAULT(MMU)                 F_BIT_SET(0+(MMU)*7)
#define F_INT_MAIN_MULTI_HIT_FAULT(MMU)              F_BIT_SET(1+(MMU)*7)
#define F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(MMU)    F_BIT_SET(2+(MMU)*7)
#define F_INT_ENTRY_REPLACEMENT_FAULT(MMU)           F_BIT_SET(3+(MMU)*7)
#define F_INT_TLB_MISS_FAULT(MMU)                    F_BIT_SET(4+(MMU)*7)
#define F_INT_MISS_FIFO_ERR(MMU)                     F_BIT_SET(5+(MMU)*7)
#define F_INT_PFH_FIFO_ERR(MMU)                      F_BIT_SET(6+(MMU)*7)
#define F_INT_MAIN_MAU_INT_EN(MMU)     F_BIT_SET(14 + MMU)
#define F_INT_MMU_MAIN_MSK(MMU)	  F_MSK((6+(MMU)*7), ((MMU)*7))

#define REG_MMU_CPE_DONE	  (0x12C)
#define F_MMU_CPE_DONE_RNG_INVLADT	BIT(0)

#define REG_MMU_L2_FAULT_ST	  (0x130)
#define F_MMU_L2_FAULT_MHIT	BIT(0)
#define F_MMU_L2_FAULT_TBW	BIT(1)
#define F_MMU_L2_FAULT_PFQ_FIFO_FULL	BIT(2)
#define F_MMU_L2_FAULT_MQ_FIFO_FULL	BIT(3)
#define F_MMU_L2_FAULT_INVLDT_DONE	BIT(4)
#define F_INT_L2_PFH_OUT_FIFO_ERROR		   F_BIT_SET(5)
#define F_INT_L2_PFH_IN_FIFO_ERROR		    F_BIT_SET(6)
#define F_INT_L2_MISS_OUT_FIFO_ERROR	     F_BIT_SET(7)
#define F_INT_L2_MISS_IN_FIFO_ERR		     F_BIT_SET(8)
#define F_MMU_L2_FAULT_L2_CDB_SLICE_ERR	BIT(9)

#define REG_MMU_FAULT_ST1	  (0x134)
//same to REG_MMU_INT_MAIN_CONTROL

#define REG_MMU_TBWALK_FAULT_VA	(0x138)
#define F_MMU_TBWALK_FAULT_LAYER	BIT(0)
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
#define F_MMU_TBWALK_FAULT_BIT32	F_MSK(10, 9)
#endif
#define F_MMU_TBWALK_FAULT_BIT31_12	F_MSK(31, 12)

#define REG_MMU_FAULT_STATUS(MMU)	  (0x13c+((MMU)<<3))
#define F_MMU_FAULT_VA_BIT31_12	F_MSK(31, 12)
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
#define F_MMU_FAULT_VA_BIT32   F_MSK(11, 9)
#endif
#define F_MMU_FAULT_PA_BIT32   F_MSK(8, 6)
#define F_MMU_FAULT_INVPA	F_BIT_SET(5)
#define F_MMU_FAULT_TF		F_BIT_SET(4)
#define F_MMU_FAULT_VA_WRITE_BIT	  F_BIT_SET(1)
#define F_MMU_FAULT_VA_LAYER_BIT	  F_BIT_SET(0)

#define REG_MMU_INVLD_PA(MMU)	  (0x140+((MMU)<<3))
#define REG_MMU_INT_ID(MMU)	  (0x150+((MMU)<<2))
#define F_MMU_INT_L2_SRC	  F_MSK(30, 28)
#define F_MMU_INT_L1_SRC	  F_MSK(26, 24)
#define F_MMU_INT_TF_VAL(regval)	  (regval & F_MSK(11, 2))

#define REG_MMU_PERF_MON_PTLB	(0x180)
#define F_MMU_PERF_MON_PTLB_ID	F_MSK(15, 0)
#define F_MMU_PERF_MON_PTLB_ALL	F_BIT_SET(16)
#define F_MMU_PERF_MON_PTLB_ID_SEL	F_BIT_SET(17)
#define F_MMU_PERF_MON_PTLB_AXI_SEL	F_MSK(19, 18)

#define REG_MMU_PERF_MON_MTLB(MMU)	(0x190 + mmu * 0x4)
#define F_MMU_PERF_MON_MTLB_ID	F_MSK(15, 0)
#define F_MMU_PERF_MON_MTLB_ALL	F_BIT_SET(16)
#define F_MMU_PERF_MON_MTLB_ID_SEL	F_BIT_SET(17)

#define REG_MMU_PF_L1_MSCNT	(0x1A0)
#define REG_MMU_PF_L2_MSCNT	(0x1A4)
#define REG_MMU_PF_L1_CNT	(0x1A8)
#define REG_MMU_PF_L2_CNT	(0x1AC)
#define REG_MMU_ACC_CNT(MMU)	(0x1C0 + MMU * 16)
#define REG_MMU_MAIN_L1_MSCNT(MMU)	(0x1C4 + MMU * 16)
#define REG_MMU_MAIN_L2_MSCNT(MMU)	(0x1C8 + MMU * 16)
#define REG_MMU_RS_PERF_CNT(MMU)	(0x1CC + MMU * 16)
#define REG_MMU_LOOKUP_CNT(MMU)	(0x1D0 + MMU * 0x20)
#define REG_MMU_PFH_VLD(VLD, WAY)	(0x200 + VLD * 0x4 + WAY * 0x10)
#define F_MMU_PFH_VLD_BIT(set, way)      F_BIT_SET((set)&0x1f)	/* set%32 */

#define REG_MMU_RS_VA(MMU, RS)	(0x380 + MMU * 0x300 + RS * 0x10)
#define F_MMU_RS_VA_BIT32	F_MSK(2, 0)
#define F_MMU_RS_VA_TBL_ID	F_MSK(5, 4)
#define F_MMU_RS_VA_BIT31_12	F_MSK(31, 12)

#define REG_MMU_RS_PA(MMU, RS)	(0x384 + MMU * 0x300 + RS * 0x10)
#define REG_MMU_RS_2ND_BASE(MMU, RS)	(0x388 + MMU * 0x300 + RS * 0x10)
#define F_MMU_RS_2ND_BASE_MSK	F_MSK(24, 0)

#define REG_MMU_RS_STA(MMU, RS)	(0x38C + MMU * 0x300 + RS * 0x10)
#define F_MMU_RS_STA_IS_LTAIL	BIT(0)
#define F_MMU_RS_STA_IS_LHEAD	BIT(1)
#define F_MMU_RS_STA_IS_PTAIL	BIT(2)
#define F_MMU_RS_STA_IS_PHEAD	BIT(3)
#define F_MMU_RS_STA_PA_RDY	BIT(4)
#define F_MMU_RS_STA_RDY2REQ	BIT(5)
#define F_MMU_RS_STA_WRT_CMD	BIT(6)
#define F_MMU_RS_STA_VLD	BIT(7)
#define F_MMU_RS_STA_TO		BIT(8)
#define F_MMU_RS_STA_ID		F_MSK(31, 16)

#define REG_MMU_MAIN_TAG(MMU, TAG)	(0x500 + MMU * 0x300 + TAG * 0x4)
#define F_MAIN_TLB_VA_MSK	   F_MSK(31, 12)
#define F_MAIN_TLB_VA_BIT32		F_MSK(1, 0)
#define F_MAIN_TLB_LOCK_BIT	 (1<<11)
#define F_MAIN_TLB_VALID_BIT	(1<<10)
#define F_MAIN_TLB_LAYER_BIT	F_BIT_SET(9)
#define F_MAIN_TLB_16X_BIT	  F_BIT_SET(8)
#define F_MAIN_TLB_SEC_BIT	  F_BIT_SET(7)
#define F_MAIN_TLB_INV_DES_BIT      (1<<6)
#define F_MAIN_TLB_TABLE_ID_BIT(regval) F_MSK_SHIFT(regval, 5, 4)

#define REG_MMU_MAU_SA(MMU, MAU)	(0x900 + MMU * 0x100 + MAU * 0)
#define REG_MMU_MAU_SA_EXT(MMU, MAU)	(0x904 + MMU * 0x100 + MAU * 0)
#define REG_MMU_MAU_EA(MMU, MAU)	(0x908 + MMU * 0x100 + MAU * 0)
#define REG_MMU_MAU_EA_EXT(MMU, MAU)	(0x90C + MMU * 0x100 + MAU * 0)
#define REG_MMU_MAU_LARB_EN(MMU)	(0x910 + MMU * 0x100)
#define REG_MMU_MAU_PORT_EN(MMU, MAU)	(0x914 + MMU * 0x100 + MAU * 0)
#define REG_MMU_MAU_ASRT_ID(MMU, MAU)	(0x918 + MMU * 0x100 + MAU * 0)
#define F_MMU_MAU_ASRT_ID_LARB(regval)    F_MSK_SHIFT(regval, 9, 5)
#define F_MMU_MAU_ASRT_ID_PORT(regval)    F_MSK_SHIFT(regval, 4, 0)
#define F_MMU_MAU_ASRT_ID_VAL    F_MSK(9, 0)

#define REG_MMU_MAU_ADDR(MMU, MAU)	(0x91C + MMU * 0x100 + MAU * 0)
#define REG_MMU_MAU_ADDR_BIT32(MMU, MAU)	(0x920 + MMU * 0x100 + MAU * 0)
#define REG_MMU_MAU_CLR(MMU)	(0x924 + MMU * 0x100)
#define REG_MMU_MAU_IO(MMU)	(0x928 + MMU * 0x100)
#define F_MAU_BIT_VAL(val, mau)     F_BIT_VAL(val, mau)
#define REG_MMU_MAU_RW(MMU)	(0x92C + MMU * 0x100)
#define REG_MMU_MAU_VA(MMU)	(0x930 + MMU * 0x100)
#define REG_MMU_MAU_ASRT_STA(MMU)	(0x934 + MMU * 0x100)
#define REG_MMU_MAU_ASRT_C4K(MMU)	(0x938 + MMU * 0x100)

#define REG_MMU_PFH_DIST(MMU, DIST)	(0xB00 + MMU * 0x80 + DIST * 0x4)

// =================== IOVA Reservation ==========================
/*
 * reserved IOVA Domain for IOMMU users of HW limitation.
 */
#define MTK_IOVA_REMOVE_CNT (6)
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
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	unsigned int boundary;
#endif
	unsigned long min_iova;
	unsigned long max_iova;
	unsigned long resv_start[MTK_IOVA_REMOVE_CNT];
	unsigned long resv_size[MTK_IOVA_REMOVE_CNT];
	unsigned int resv_type;
	unsigned int port_mask[MTK_IOMMU_LARB_NR];
	int owner;
};

#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#define MTK_IOVA_DOMAIN_COUNT (10)	// Domain count
#define RESERVED_IOVA_ADDR_SECURE SZ_4K
#define RESERVED_IOVA_SIZE_SECURE (SZ_1G - SZ_4K)

#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
#define RESERVED_IOVA_ADDR_CCU_NODE (0x0240000000UL)
#define RESERVED_IOVA_SIZE_CCU_NODE (0x0004000000UL)
#define RESERVED_IOVA_ADDR_CCU_LARB (0x0244000000UL)
#define RESERVED_IOVA_SIZE_CCU_LARB (0x0004000000UL)
#define RESERVED_IOVA_ADDR_APU_CODE (0x0370000000UL)
#define RESERVED_IOVA_SIZE_APU_CODE (0x0012600000UL)
#define RESERVED_IOVA_ADDR_APU_DATA (0x0310000000UL)
#define RESERVED_IOVA_SIZE_APU_DATA (0x0010000000UL)
#define RESERVED_IOVA_ADDR_APU_VLM (0x0304000000UL)
#define RESERVED_IOVA_SIZE_APU_VLM (0x0004000000UL)
#define IOVA_ADDR_4GB    (1UL << 32)
#define IOVA_ADDR_8GB    (2UL << 32)
#define IOVA_ADDR_12GB   (3UL << 32)
#define IOVA_ADDR_16GB   (4UL << 32)
const struct mtk_iova_domain_data mtk_domain_array[MTK_IOVA_DOMAIN_COUNT] = {
#if (defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)) || \
	defined(CONFIG_MTK_GZ_SUPPORT_SDSP)
	{ // boundary(0~4GB) IOVA space for OVL
	 .boundary = 0,
	 .owner = -1,
	 .min_iova = SZ_1G,
	 .max_iova = IOVA_ADDR_4GB - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x7fff, 0x7fff, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x0, 0x0, 0x0} //20~23
	},
#else
	{ // boundary(0~4GB) IOVA space for display
	 .boundary = 0,
	 .owner = -1,
	 .min_iova = SZ_4K,
	 .max_iova = IOVA_ADDR_4GB - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x7fff, 0x7fff, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x0, 0x0, 0x0} //20~23
	},
#endif
	{ // boundary(4GB~8GB) IOVA space for CODEC
	 .boundary = 1,
	 .owner = -1,
	 .min_iova = IOVA_ADDR_4GB,
	 .max_iova = IOVA_ADDR_8GB - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x7ff, 0xff, 0x0, 0x7ffffff, //4~7
				 0x7ffffff, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x0, 0x0, 0x0} //20~23
	},
	{ // boundary(8GB~12GB) IOVA space for CMA MDP
	 .boundary = 2,
	 .owner = -1,
	 .min_iova = IOVA_ADDR_8GB,
	 .max_iova = IOVA_ADDR_12GB - 1,
	 .resv_start = {RESERVED_IOVA_ADDR_CCU_NODE,
			RESERVED_IOVA_ADDR_CCU_LARB},
	 .resv_size = {RESERVED_IOVA_SIZE_CCU_NODE,
			RESERVED_IOVA_SIZE_CCU_LARB},
	 .resv_type = IOVA_REGION_REMOVE,
	 .port_mask = {0x0, 0x0, 0x3f, 0x3f, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x1fffffff, 0x0, 0x1fffffff, //8~11
				 0x0, 0xfff, 0x3f, 0x0, //12~15
				 0x1ffff, 0x1ffff, 0x1ffff, 0xf, //16~19
				 0x3f, 0x0, 0x0, 0x0} //20~23
	},
	{ //CCU_NODE IOVA space
	 .boundary = 2,
	 .owner = -1,
	 .min_iova = RESERVED_IOVA_ADDR_CCU_NODE,
	 .max_iova = RESERVED_IOVA_ADDR_CCU_NODE +
			RESERVED_IOVA_SIZE_CCU_NODE - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x0, 0x0, 0x1} //20~23
	},
	{ //CCU_LARB IOVA space
	 .boundary = 2,
	 .owner = -1,
	 .min_iova = RESERVED_IOVA_ADDR_CCU_LARB,
	 .max_iova = RESERVED_IOVA_ADDR_CCU_LARB +
			RESERVED_IOVA_SIZE_CCU_LARB - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x0, 0x1, 0x0} //20~23
	},
	{ // boundary(12GB~16GB) IOVA space for APU DATA
	 .boundary = 3,
	 .owner = M4U_PORT_L21_APU_FAKE_DATA,
	 .min_iova = IOVA_ADDR_12GB,
	 .max_iova = IOVA_ADDR_16GB - 1,
	 .resv_start = {RESERVED_IOVA_ADDR_APU_DATA,
			RESERVED_IOVA_ADDR_APU_CODE,
			RESERVED_IOVA_ADDR_APU_VLM},
	 .resv_size = {RESERVED_IOVA_SIZE_APU_DATA,
			RESERVED_IOVA_SIZE_APU_CODE,
			RESERVED_IOVA_SIZE_APU_VLM},
	 .resv_type = IOVA_REGION_REMOVE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x2, 0x0, 0x0} //20~23
	},
	{ //VPU CODE IOVA space
	 .boundary = 3,
	 .owner = M4U_PORT_L21_APU_FAKE_CODE,
	 .min_iova = RESERVED_IOVA_ADDR_APU_CODE,
	 .max_iova = RESERVED_IOVA_ADDR_APU_CODE +
			RESERVED_IOVA_SIZE_APU_CODE - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x1, 0x0, 0x0} //20~23
	},
	{ //VPU VLM IOVA space
	 .boundary = 3,
	 .owner = M4U_PORT_L21_APU_FAKE_VLM,
	 .min_iova = RESERVED_IOVA_ADDR_APU_VLM,
	 .max_iova = RESERVED_IOVA_ADDR_APU_VLM +
			RESERVED_IOVA_SIZE_APU_VLM - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x4, 0x0, 0x0} //20~23
	},
};

#else
#define RESERVED_IOVA_ADDR_CCU_NODE (0x40000000U)
#define RESERVED_IOVA_SIZE_CCU_NODE (0x04000000U)
#define RESERVED_IOVA_ADDR_CCU_LARB (0x44000000U)
#define RESERVED_IOVA_SIZE_CCU_LARB (0x04000000U)
#define RESERVED_IOVA_ADDR_APU_CODE (0x70000000U)
#define RESERVED_IOVA_SIZE_APU_CODE (0x12600000U)
#define RESERVED_IOVA_ADDR_APU_DATA (0x10000000U)
#define RESERVED_IOVA_SIZE_APU_DATA (0x10000000U)
#define RESERVED_IOVA_ADDR_APU_VLM  (0x48000000U)
#define RESERVED_IOVA_SIZE_APU_VLM  (0x04000000U)
const struct mtk_iova_domain_data mtk_domain_array[MTK_IOVA_DOMAIN_COUNT] = {
#if (defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)) || \
	defined(CONFIG_MTK_GZ_SUPPORT_SDSP)
	{ //REE IOVA space
	 .owner = -1,
	 .min_iova = RESERVED_IOVA_ADDR_APU_VLM +
			RESERVED_IOVA_SIZE_APU_VLM,
	 .max_iova = DMA_BIT_MASK(MTK_IOVA_ADDR_BITS),
	 .resv_start = {RESERVED_IOVA_ADDR_APU_CODE},
	 .resv_size = {RESERVED_IOVA_SIZE_APU_CODE},
	 .resv_type = IOVA_REGION_REMOVE,
	 .port_mask = {0x7fff, 0x7fff, 0x3f, 0x3f, //0~3
				 0x7ff, 0xff, 0x0, 0x7ffffff, //4~7
				 0x7ffffff, 0x1fffffff, 0x0, 0x1fffffff, //8~11
				 0x0, 0x9ff, 0xf, 0x0, //12~15
				 0x1ffff, 0x1ffff, 0x1ffff, 0xf, //16~19
				 0x3f, 0x2, 0x0, 0x0} //20~23
	},
#else
	{ //public IOVA space
	 .owner = -1,
	 .min_iova = SZ_4K,
	 .max_iova = DMA_BIT_MASK(MTK_IOVA_ADDR_BITS),
	 .resv_start = {RESERVED_IOVA_ADDR_CCU_NODE,
			RESERVED_IOVA_ADDR_CCU_LARB,
			RESERVED_IOVA_ADDR_APU_CODE,
			RESERVED_IOVA_ADDR_APU_VLM,
			RESERVED_IOVA_ADDR_APU_DATA},
	 .resv_size = {RESERVED_IOVA_SIZE_CCU_NODE,
			RESERVED_IOVA_SIZE_CCU_LARB,
			RESERVED_IOVA_SIZE_APU_CODE,
			RESERVED_IOVA_SIZE_APU_VLM,
			RESERVED_IOVA_SIZE_APU_DATA},
	 .resv_type = IOVA_REGION_REMOVE,
	 .port_mask = {0x7fff, 0x7fff, 0x3f, 0x3f, //0~3
				 0x7ff, 0xff, 0x0, 0x7ffffff, //4~7
				 0x7ffffff, 0x1fffffff, 0x0, 0x1fffffff, //8~11
				 0x0, 0xfff, 0x3f, 0x0, //12~15
				 0x1ffff, 0x1ffff, 0x1ffff, 0xf, //16~19
				 0x3f, 0x2, 0x0, 0x0} //20~23
	},
#endif
	{ //CCU_NODE IOVA space
	 .owner = -1,
	 .min_iova = RESERVED_IOVA_ADDR_CCU_NODE,
	 .max_iova = RESERVED_IOVA_ADDR_CCU_NODE +
			RESERVED_IOVA_SIZE_CCU_NODE - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x0, 0x0, 0x1} //20~23
	},
	{ //CCU_LARB IOVA space
	 .owner = -1,
	 .min_iova = RESERVED_IOVA_ADDR_CCU_LARB,
	 .max_iova = RESERVED_IOVA_ADDR_CCU_LARB +
			RESERVED_IOVA_SIZE_CCU_LARB - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x0, 0x1, 0x0} //20~23
	},
	{ //VPU CODE IOVA space
	 .owner = M4U_PORT_L21_APU_FAKE_CODE,
	 .min_iova = RESERVED_IOVA_ADDR_APU_CODE,
	 .max_iova = RESERVED_IOVA_ADDR_APU_CODE +
			RESERVED_IOVA_SIZE_APU_CODE - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x1, 0x0, 0x0} //20~23
	},
	{ //VPU VLM IOVA space
	 .owner = M4U_PORT_L21_APU_FAKE_VLM,
	 .min_iova = RESERVED_IOVA_ADDR_APU_VLM,
	 .max_iova = RESERVED_IOVA_ADDR_APU_VLM +
			RESERVED_IOVA_SIZE_APU_VLM - 1,
	 .resv_type = IOVA_REGION_UNDEFINE,
	 .port_mask = {0x0, 0x0, 0x0, 0x0, //0~3
				 0x0, 0x0, 0x0, 0x0, //4~7
				 0x0, 0x0, 0x0, 0x0, //8~11
				 0x0, 0x0, 0x0, 0x0, //12~15
				 0x0, 0x0, 0x0, 0x0, //16~19
				 0x0, 0x4, 0x0, 0x0} //20~23
	},
};
#endif

#define MTK_IOMMU_PAGE_TABLE_SHARE (1)
#ifdef CONFIG_MTK_APUSYS_SUPPORT
#define MTK_APU_TFRP_SUPPORT
#endif
#define IOMMU_POWER_CLK_SUPPORT
#ifdef IOMMU_POWER_CLK_SUPPORT
#define MTK_IOMMU_LOW_POWER_SUPPORT
#endif

#define MTK_IOMMU_SIZE_NOT_ALIGNMENT
#define MTK_IOMMU_BANK_IRQ_SUPPORT

#if (defined(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	defined(CONFIG_MICROTRUST_TEE_SUPPORT)) && \
	defined(CONFIG_MTK_TEE_GP_SUPPORT)
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) && \
	!defined(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)
#define MTK_M4U_SECURE_IRQ_SUPPORT
#elif defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#define MTK_M4U_SECURE_IRQ_SUPPORT
#elif defined(CONFIG_MTK_GZ_SUPPORT_SDSP)
#define MTK_M4U_SECURE_IRQ_SUPPORT
#endif
#elif defined(MTK_IOMMU_BANK_IRQ_SUPPORT)
#define MTK_M4U_SECURE_IRQ_SUPPORT
#endif

#define IOMMU_SECURITY_DBG_SUPPORT

struct mau_config_info mt6885_mau_info[MTK_IOMMU_M4U_COUNT] = {
	{
		.start = 0x0,
		.end = SZ_4K - 1,
		.port_mask = 0xffffffff,
		.larb_mask = 0xffffffff,
		.wr = 0x1,
		.virt = 0x1,
		.io = 0x0,
		.start_bit32 = 0x0,
		.end_bit32 = 0x0,
	},
	{
		.start = 0x0,
		.end = SZ_4K - 1,
		.port_mask = 0xffffffff,
		.larb_mask = 0xffffffff,
		.wr = 0x1,
		.virt = 0x1,
		.io = 0x0,
		.start_bit32 = 0x0,
		.end_bit32 = 0x0,
	},
	{
		.start = 0x0,
		.end = SZ_4K - 1,
		.port_mask = 0xffffffff,
		.larb_mask = 0xffffffff,
		.wr = 0x1,
		.virt = 0x1,
		.io = 0x0,
		.start_bit32 = 0x0,
		.end_bit32 = 0x0,
	},
	{
		.start = 0x0,
		.end = SZ_4K - 1,
		.port_mask = 0xffffffff,
		.larb_mask = 0xffffffff,
		.wr = 0x1,
		.virt = 0x1,
		.io = 0x0,
		.start_bit32 = 0x0,
		.end_bit32 = 0x0,
	},
};

struct mau_config_info *get_mau_info(int m4u_id)
{
	if (m4u_id < MTK_IOMMU_M4U_COUNT)
		return &mt6885_mau_info[m4u_id];
	else
		return NULL;
}

#endif
