// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MT6758_M4U_REG_H__
#define _MT6758_M4U_REG_H__

/* #include "mach/mt_reg_base.h" */

/* ================================================= */
/* common macro definitions */
#define F_VAL(val, msb, lsb) (((val)&((1<<(msb-lsb+1))-1))<<lsb)
#define F_VAL_L(val, msb, lsb) (((val)&((1L<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)     F_VAL(0xffffffff, msb, lsb)
#define F_MSK_L(msb, lsb)     F_VAL_L(0xffffffffffffffffL, msb, lsb)
#define F_BIT_SET(bit)	  (1<<(bit))
#define F_BIT_VAL(val, bit)  ((!!(val))<<(bit))
#define F_MSK_SHIFT(regval, msb, lsb) (((regval)&F_MSK(msb, lsb))>>lsb)


/* ===================================================== */
/* M4U register definition */
/* ===================================================== */

#define REG_MMUg_PT_BASE	   (0x0)
#define F_MMUg_PT_VA_MSK	0xffff0000
#define REG_MMUg_PT_BASE_SEC	 (0x4)
#define F_MMUg_PT_VA_MSK_SEC	0xffff0000


#define REG_MMU_PROG_EN		 0x10
#define F_MMU0_PROG_EN	       1

#define REG_MMU_PROG_VA		 0x14
#define F_PROG_VA_LOCK_BIT	     (1<<11)
#define F_PROG_VA_LAYER_BIT	    F_BIT_SET(9)
#define F_PROG_VA_SIZE16X_BIT	  F_BIT_SET(8)
#define F_PROG_VA_SECURE_BIT	   (1<<7)
#define F_PROG_VA_MASK	    0xfffff000

#define REG_MMU_PROG_DSC		0x18

#define REG_MMU_INVLD	    (0x20)
#define F_MMU_INV_ALL		(1<<2)
#define F_MMU_INV_RANGE	    (1<<0)

#define REG_MMU_INVLD_SA		 (0x24)
#define REG_MMU_INVLD_EA	  (0x28)


#define REG_MMU_INVLD_SEC	   (0x2c)
#define F_MMU_INV_SEC_RANGE	0x1

#define REG_MMU_INVLD_SA_SEC	(0x30)
#define REG_MMU_INVLD_EA_SEC	(0x34)

#define REG_INVLID_SEL		   (0x38)
#define F_MMU_INV_EN_L2	 (1<<1)
#define F_MMU_INV_EN_L1	 (1<<0)

#define REG_INVLID_SEL_SEC	  (0x3c)
#define F_MMU_INV_SEC_DBG	   (1<<5)
#define F_MMU_INV_SEC_INV_INT_EN    (1<<4)
#define F_MMU_INV_SEC_INV_INT_CLR   (1<<3)
#define F_MMU_INV_SEC_INV_DONE      (1<<2)
#define F_MMU_INV_SEC_EN_L2	     (1<<1)
#define F_MMU_INV_SEC_EN_L1	     (1<<0)

#define REG_MMU_SEC_ABORT_INFO      (0x40)
#define F_MMU_M4U_DOMAIN_ABORT    F_BIT_VAL(en, 31)
#define F_MMU_SEC_ABORT_DOMAIN    F_MSK_SHIFT(regval, 20, 16)
#define F_MMU_SEC_ABORT_ID    F_MSK_SHIFT(regval, 15, 0)

#define REG_MMU_DUMMY	       (0x44)
#define F_REG_MMU_IDLE_ENABLE	     F_BIT_SET(0)

#define REG_MMU_STANDARD_AXI_MODE   (0x48)

#define REG_MMU_PRIORITY	(0x4c)
#define F_MMU1_COMMAND_GROUPING_EN  F_BIT_SET(4)
#define F_MMU0_COMMAND_GROUPING_EN  F_BIT_SET(3)
#define F_MMU_RS_FULL_ULTRA_EN  F_BIT_SET(2)
#define F_MMU_AUTO_PF_ULTRA_BIT F_BIT_SET(1)
#define F_MMU_TABLE_WALK_ULTRA_BIT F_BIT_SET(0)

#define REG_MMU_DCM_DIS	 (0x50)
#define F_MMU_MTLB_LOGIC_STALL_DCM    F_BIT_SET(14)
#define F_MMU_RS_ENTRY_STALL_DCM    F_BIT_SET(13)
#define F_MMU_FIFO_ENTRY_STALL_DCM    F_BIT_SET(12)
#define F_MMU_SLICE_ENTRY_STALL_DCM    F_BIT_SET(11)
#define F_MMU_ULTRA_SLICE_STALL_ENTRY_DCM    F_BIT_SET(10)
#define F_MMU_L2_LOGIC_STALL_DCM    F_BIT_SET(9)
#define F_MMU_TOP_STALL_DCM     F_BIT_SET(8)
#define F_MMU_APB_DCM	   F_BIT_SET(7)
#define F_MMU_MTLB_LOGIC_DCM    F_BIT_SET(6)
#define F_MMU_RS_ENTRY_DCM    F_BIT_SET(5)
#define F_MMU_FIFO_ENTRY_DCM     F_BIT_SET(4)
#define F_MMU_SLICE_ENTRY_DCM       F_BIT_SET(3)
#define F_MMU_ULTRA_SLICE_ENTRY_DCM    F_BIT_SET(2)
#define F_MMU_L2_LOGIC_DCM    F_BIT_SET(1)
#define F_MMU_TOP_DCM     F_BIT_SET(0)

#define REG_MMU_WR_LEN	  (0x54)
#define F_MMU_WR_THROT_DIS(sel)    F_VAL(sel, 11, 10)
#define F_MMU_MMU1_WRITE_LEN    F_MSK(9, 5)
#define F_MMU_MMU0_WRITE_LEN    F_MSK(4, 0)

#define REG_MMU_HW_DEBUG	(0x58)
#define F_MMU_HW_DBG_L2_SCAN_ALL    F_BIT_SET(1)
#define F_MMU_HW_DBG_PFQ_BRDCST     F_BIT_SET(0)

#define REG_MMU_NON_BLOCKING_DIS    0x5C
#define F_MMU_MMU1_HALF_ENTRY_MODE	F_BIT_SET(3)
#define F_MMU_MMU0_HALF_ENTRY_MODE    F_BIT_SET(2)
#define F_MMU_MMU1_BLOCKING_MODE	F_BIT_SET(1)
#define F_MMU_MMU0_BLOCKING_MODE	F_BIT_SET(0)

#define REG_MMU_LEGACY_4KB_MODE     (0x60)
#define REG_MMU_DBG0		(0X64)
#define REG_MMU_DBG1		(0x68)
#define REG_MMU_DBG2		(0x6c)
#define REG_MMU_SMI_COMMON_DBG0     (0x78)

#define REG_MMU_MMU_COHERENCE_EN	  0x80
#define REG_MMU_IN_ORDER_WR_EN	    0x84
#define REG_MMU_MMU_TABLE_WALK_DIS	0x88
#define REG_MMU_MMU_MTLB_RESERVE_MODE_DIS 0x8c

#define REG_MMU_READ_ENTRY       0x100
#define F_READ_ENTRY_EN		 F_BIT_SET(31)
#define F_READ_ENTRY_VICT_TLB_SEL F_BIT_SET(30)
#define F_READ_ENTRY_MM0_MAIN	   F_BIT_SET(27)
#define F_READ_ENTRY_MMx_MAIN(id)       F_BIT_SET(27+id)
#define F_READ_ENTRY_PFH		F_BIT_SET(26)
#define F_READ_ENTRY_MMU1_IDX(idx)      F_VAL(idx, 24, 19)
#define F_READ_ENTRY_MAIN_IDX(idx)      F_VAL(idx, 17, 12)
#define F_READ_ENTRY_PFH_IDX(idx)       F_VAL(idx, 9, 5)
#define F_READ_ENTRY_PFH_HI_LO(high)    F_VAL(high, 4, 4)
#define F_READ_ENTRY_PFH_PAGE_IDX(idx)    F_VAL(idx, 3, 2)
#define F_READ_ENTRY_PFH_WAY(way)       F_VAL(way, 1, 0)

#define REG_MMU_DES_RDATA	0x104

#define REG_MMU_PFH_TAG_RDATA    0x108
#define F_PFH_TAG_VA_GET(mmu, tag)    \
	((mmu == 0)?F_MMU0_PFH_TAG_VA_GET(tag) : F_MMU1_PFH_TAG_VA_GET(tag))
#define F_MMU0_PFH_TAG_VA_GET(tag)    \
	(F_MSK_SHIFT(tag, 15, 4)<<(MMU_SET_MSB_OFFSET(0)+1))
#define F_MMU1_PFH_TAG_VA_GET(tag)    \
	(F_MSK_SHIFT(tag, 15, 4)<<(MMU_SET_MSB_OFFSET(1)+1))
#define F_MMU_PFH_TAG_VA_LAYER0_MSK(mmu)  \
	((mmu == 0)?F_MSK(31, 28):F_MSK(31, 28))
#define F_PFH_TAG_LAYER_BIT	 F_BIT_SET(3)
/* this bit is always 0 -- cost down. */
#define F_PFH_TAG_16X_BIT	   F_BIT_SET(2)
#define F_PFH_TAG_SEC_BIT	   F_BIT_SET(1)
#define F_PFH_TAG_AUTO_PFH	  F_BIT_SET(0)

#define REG_MMU_VICT_VLD 0x10c
#define F_MMU_VICT_VLD_BIT(way)      F_BIT_SET((way)&0xf)

/* tag related macro */
#define MMU0_SET_ORDER	 5
#define MMU1_SET_ORDER	 5
#define MMU_SET_ORDER(mmu)      ((mmu == 0) ? MMU0_SET_ORDER : MMU1_SET_ORDER)
#define MMU_SET_NR(mmu)    (1<<MMU_SET_ORDER(mmu))
#define MMU_SET_LSB_OFFSET	       15
#define MMU_SET_MSB_OFFSET(mmu)	 (MMU_SET_LSB_OFFSET+MMU_SET_ORDER(mmu)-1)
#define MMU_PFH_VA_TO_SET(mmu, va)     \
	F_MSK_SHIFT(va, MMU_SET_MSB_OFFSET(mmu), MMU_SET_LSB_OFFSET)

#define MMU_PAGE_PER_LINE      4
#define MMU_WAY_NR  4
#define MMU_PFH_TOTAL_LINE(mmu) (MMU_SET_NR(mmu)*MMU_WAY_NR)


#define REG_MMU_CTRL_REG	 0x110
#define F_MMU_CTRL_HIT_AT_PFQ_EN(en)    F_BIT_VAL(en, 11)
#define F_MMU_CTRL_HIT_AT_PFQ_L2_EN(en) F_BIT_VAL(en, 10)
#define F_MMU_CTRL_HANG_PREVENTION(en)  F_BIT_VAL(en,  9)
#define F_MMU_CTRL_INVALID_FIFO_EN(en)  F_BIT_VAL(en,  8)
#define F_MMU_CTRL_LAYER2_PFH_DIS(dis)  F_BIT_VAL(dis, 7)
#define F_MMU_CTRL_INT_HANG_EN(en)      F_BIT_VAL(en,  6)
#define F_MMU_CTRL_TF_PROTECT_SEL(en)   F_VAL(en, 5, 4)
#define F_MMU_CTRL_MONITOR_CLR(clr)     F_BIT_VAL(clr, 2)
#define F_MMU_CTRL_MONITOR_EN(en)       F_BIT_VAL(en,  1)
#define F_MMU_CTRL_PFH_DIS(dis)	 F_BIT_VAL(dis, 0)

#define REG_MMU_IVRP_PADDR       0x114
#define F_MMU_IVRP_PA_SET(PA)  \
	((((unsigned long long)PA) & F_MSK(31, 7)) | \
	((((unsigned long long)PA) >> 32) & F_MSK(1, 0)))

#define REG_MMU_INT_L2_CONTROL      0x120
#define F_INT_L2_CLR_BIT (1<<12)
#define F_INT_L2_MULTI_HIT_FAULT		   F_BIT_SET(0)
#define F_INT_L2_TABLE_WALK_FAULT		  F_BIT_SET(1)
#define F_INT_L2_PFH_DMA_FIFO_OVERFLOW	     F_BIT_SET(2)
#define F_INT_L2_MISS_DMA_FIFO_OVERFLOW	    F_BIT_SET(3)
#define F_INT_L2_INVALID_DONE		       F_BIT_SET(4)
#define F_INT_L2_PFH_FIFO_ERROR		    F_BIT_SET(5)
#define F_INT_L2_MISS_FIFO_ERR		     F_BIT_SET(6)

#define REG_MMU_INT_MAIN_CONTROL    0x124
#define F_INT_TRANSLATION_FAULT(MMU)                 F_BIT_SET(0+(MMU)*7)
#define F_INT_MAIN_MULTI_HIT_FAULT(MMU)              F_BIT_SET(1+(MMU)*7)
#define F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(MMU)    F_BIT_SET(2+(MMU)*7)
#define F_INT_ENTRY_REPLACEMENT_FAULT(MMU)           F_BIT_SET(3+(MMU)*7)
#define F_INT_TLB_MISS_FAULT(MMU)                    F_BIT_SET(4+(MMU)*7)
#define F_INT_MISS_FIFO_ERR(MMU)                     F_BIT_SET(5+(MMU)*7)
#define F_INT_PFH_FIFO_ERR(MMU)                      F_BIT_SET(6+(MMU)*7)


#define F_INT_MAU(mmu, set)     F_BIT_SET(14+(set)+(mmu*4))
/* Dual AXI (14+(set)+(mmu*4));  Single AXI (7+(set)+(mmu*4)); */

#define F_INT_MMU0_MAIN_MSK	  F_MSK(6, 0)
#define F_INT_MMU1_MAIN_MSK	  F_MSK(13, 7)
#define F_INT_MMU0_MAU_MSK	   F_MSK(17, 14)
#define F_INT_MMU1_MAU_MSK	   F_MSK(21, 18)

#define REG_MMU_CPE_DONE_SEC    0x128
#define REG_MMU_CPE_DONE	0x12C

#define REG_MMU_L2_FAULT_ST	 0x130
#define F_INT_L2_MISS_IN_FIFO_ERR		     F_BIT_SET(8)
#define F_INT_L2_MISS_OUT_FIFO_ERROR	     F_BIT_SET(7)
#define F_INT_L2_PFH_IN_FIFO_ERROR		    F_BIT_SET(6)
#define F_INT_L2_PFH_OUT_FIFO_ERROR		   F_BIT_SET(5)

#define REG_MMU_MAIN_FAULT_ST       0x134

#define REG_MMU_TBWALK_FAULT_VA	 0x138
#define F_MMU_TBWALK_FAULT_VA_MSK   F_MSK(31, 12)
#define F_MMU_TBWALK_FAULT_LAYER(regval) F_MSK_SHIFT(regval, 0, 0)

#define REG_MMU_FAULT_VA(mmu)	 (0x13c+((mmu)<<3))
#define F_MMU_FAULT_VA_MSK	F_MSK(31, 12)
#define F_MMU_FAULT_VA_WRITE_BIT    F_BIT_SET(1)
#define F_MMU_FAULT_VA_LAYER_BIT    F_BIT_SET(0)

#define REG_MMU_INVLD_PA(mmu)	 (0x140+((mmu)<<3))
#define REG_MMU_INT_ID(mmu)	     (0x150+((mmu)<<2))
#define F_MMU0_INT_ID_TF_MSK	(~0x3)      /* only for MM iommu. */

#define REG_MMU_PF_MSCNT	    0x160
#define REG_MMU_PF_CNT	      0x164
#define REG_MMU_ACC_CNT(mmu)	\
	(0x168+(((mmu)<<3)|((mmu)<<2)))     /* (0x168+((mmu)*12) */
#define REG_MMU_MAIN_MSCNT(mmu)     (0x16c+(((mmu)<<3)|((mmu)<<2)))
#define REG_MMU_RS_PERF_CNT(mmu)    (0x170+(((mmu)<<3)|((mmu)<<2)))

#define REG_MMU_PFH_VLD_0   (0x180)
#define REG_MMU_PFH_VLD(mmu, set, way)     \
	(REG_MMU_PFH_VLD_0+(((set)>>5)<<2)+\
	((way)<<((mmu == 0)?(MMU0_SET_ORDER - 3):(MMU1_SET_ORDER - 3))))
#define F_MMU_PFH_VLD_BIT(set, way)      F_BIT_SET((set)&0x1f)  /* set%32 */

#define MMU01_SQ_OFFSET (0x600-0x300)
#define REG_MMU_SQ_START(mmu, x)	     \
	(0x300+((x)<<3)+((mmu)*MMU01_SQ_OFFSET))
#define F_SQ_VA_MASK		F_MSK(31, 20)
#define F_SQ_EN_BIT		 (1<<19)
/* #define F_SQ_MULTI_ENTRY_VAL(x)     (((x)&0xf)<<13) */
#define REG_MMU_SQ_END(mmu, x)	       (0x304+((x)<<3)+((mmu)*MMU01_SQ_OFFSET))


#define MMU_TOTAL_RS_NR	 8
#define REG_MMU_RSx_VA(mmu, x)      (0x380+((x)<<4)+((mmu)*MMU01_SQ_OFFSET))
#define F_MMU_RSx_VA_GET(regval)    ((regval)&F_MSK(31, 12))
#define F_MMU_RSx_VA_VALID(regval)  F_MSK_SHIFT(regval, 11, 11)
#define F_MMU_RSx_VA_PID(regval)    F_MSK_SHIFT(regval, 9, 0)

#define REG_MMU_RSx_PA(mmu, x)      (0x384+((x)<<4)+((mmu)*MMU01_SQ_OFFSET))
#define F_MMU_RSx_PA_GET(regval)    ((regval)&F_MSK(31, 12))

#define REG_MMU_RSx_2ND_BASE(mmu, x) (0x388+((x)<<4)+((mmu)*MMU01_SQ_OFFSET))

#define REG_MMU_RSx_ST(mmu, x)      (0x38c+((x)<<4)+((mmu)*MMU01_SQ_OFFSET))
#define F_MMU_RSx_ST_LID(regval)    F_MSK_SHIFT(regval, 22, 20)
#define F_MMU_RSx_ST_WRT(regval)    F_MSK_SHIFT(regval, 12, 12)
#define F_MMU_RSx_ST_OTHER(regval)  F_MSK_SHIFT(regval, 8, 0)

#define REG_MMU_MAIN_TAG(mmu, x)       (0x500+((x)<<2)+((mmu)*MMU01_SQ_OFFSET))
#define F_MAIN_TLB_VA_MSK	   F_MSK(31, 12)
#define F_MAIN_TLB_LOCK_BIT	 (1<<11)
#define F_MAIN_TLB_VALID_BIT	(1<<10)
#define F_MAIN_TLB_LAYER_BIT	F_BIT_SET(9)
#define F_MAIN_TLB_16X_BIT	  F_BIT_SET(8)
#define F_MAIN_TLB_SEC_BIT	  F_BIT_SET(7)
#define F_MAIN_TLB_INV_DES_BIT      (1<<6)
#define F_MAIN_TLB_SQ_EN_BIT	(1<<5)
#define F_MAIN_TLB_SQ_INDEX_MSK     F_MSK(4, 1)
#define F_MAIN_TLB_SQ_INDEX_GET(regval)     F_MSK_SHIFT(regval, 4, 1)


#define REG_MMU_MAU_START(mmu, mau)	      (0x900+((mau)*0x20)+((mmu)*0xa4))
#define REG_MMU_MAU_START_BIT32(mmu, mau)	\
	(0x904+((mau)*0x20)+((mmu)*0xa4))
#define REG_MMU_MAU_END(mmu, mau)		\
	(0x908+((mau)*0x20)+((mmu)*0xa4))
#define REG_MMU_MAU_END_BIT32(mmu, mau)	  (0x90C+((mau)*0x20)+((mmu)*0xa4))

#define REG_MMU_MAU_LARB_EN(mmu)		(0x910+((mmu)*0xa4))
#define F_MAU_LARB_VAL(mau, larb)	 ((larb)<<(mau*8))
#define F_MAU_LARB_MSK(mau)	     (0xff<<(mau*8))
#define REG_MMU_MAU_PORT_EN(mmu, mau)	    (0x914+((mau)*0x20)+((mmu)*0xa4))
#define REG_MMU_MAU_ASSERT_ID(mmu, mau)	  (0x918+((mau)*0x20)+((mmu)*0xa4))
#define F_MMU_MAU_ASSERT_ID_LARB(regval)    F_MSK_SHIFT(regval, 7, 5)
#define F_MMU_MAU_ASSERT_ID_PORT(regval)    F_MSK_SHIFT(regval, 4, 0)

#define REG_MMU_MAU_ADDR(mmu, mau)	       (0x91C+((mau)*0x20)+((mmu)*0xa4))
#define REG_MMU_MAU_ADDR_BIT32(mmu, mau)	 \
	(0x920+((mau)*0x20)+((mmu)*0xa4))

#define REG_MMU_MAU_CLR(mmu)		(0x924+((mmu)*0xa4))
#define REG_MMU_MAU_IO(mmu)		(0x928+((mmu)*0xa4))
#define F_MAU_BIT_VAL(val, mau)     F_BIT_VAL(val, mau)
#define REG_MMU_MAU_RW(mmu)		(0x92c+((mmu)*0xa4))
#define REG_MMU_MAU_VA(mmu)		(0x930+((mmu)*0xa4))
#define REG_MMU_MAU_ASSERT_ST(mmu)		(0x934+((mmu)*0xa4))

#define MMU_TOTAL_PROG_DIST_NR	 8
#define REG_MMU_PROG_DIST0	   0xb00
#define REG_MMU_PROG_DIST1	   0xb04
#define REG_MMU_PROG_DIST2	   0xb08
#define REG_MMU_PROG_DIST3	   0xb0c
#define REG_MMU_PROG_DIST4	   0xb10
#define REG_MMU_PROG_DIST5	   0xb14
#define REG_MMU_PROG_DIST6	   0xb18
#define REG_MMU_PROG_DIST7	   0xb1c
#define REG_MMU_PROG_DIST(dist)      (0xb00+((dist)<<2))
#define F_PF_ID_COMP_SEL(sel)    F_BIT_VAL(sel, 16)
#define F_PF_DIR(dir)	    F_BIT_VAL(dir, 15)
#define F_PF_DIST_MSB		14
#define F_PF_DIST_LSB		11
#define F_PF_ID(larb, port, mm_id)      ((larb) << 7 | ((port) << 2) | mm_id)
#define F_PF_ID_MSB		10
#define F_PF_ID_LSB		1
#define F_PF_EN(en)	      F_BIT_VAL(en, 0)
#define REG_MMU_SMI_ASYNC_CFG	0xb80


/* ================================================================ */
/* SMI larb */
/* ================================================================ */

#define SMI_LARB_NON_SEC_CONx(larb_port)	(0x380 + ((larb_port)<<2))
	#define F_SMI_NON_SEC_MMU_EN(en)	F_BIT_VAL(en, 0)
#define F_SMI_MMU_EN          F_BIT_SET(0)

#define SMI_LARB_SEC_CONx(larb_port)	(0xf80 + ((larb_port)<<2))
	#define F_SMI_SEC_MMU_EN(en)	F_BIT_VAL(en, 0)
	#define F_SMI_SEC_EN(sec)	F_BIT_VAL(sec, 1)
#define F_SMI_DOMN(domain)	F_VAL(domain, 8, 4)
/* ========================================================================= */
/* peripheral system */
/* ========================================================================= */
#define REG_PERIAXI_BUS_CTL3   (0x208)
#define F_PERI_MMU_EN(port, en)       ((en)<<((port)))

#include <sync_write.h>


static inline unsigned int COM_ReadReg32(unsigned long addr)
{
	return ioread32((void *)addr);
}

static inline void COM_WriteReg32(unsigned long addr, unsigned int Val)
{
	mt_reg_sync_writel(Val, (void *)addr);
}

static inline unsigned int M4U_ReadReg32(
		unsigned long M4uBase, unsigned int Offset)
{
	unsigned int val;

	val = COM_ReadReg32((M4uBase + Offset));

	return val;
}

static inline void M4U_WriteReg32(unsigned long M4uBase,
	unsigned int Offset, unsigned int Val)
{
	COM_WriteReg32((M4uBase + Offset), Val);
}

static inline unsigned int m4uHw_set_field(unsigned long M4UBase,
		unsigned int Reg, unsigned int bit_width, unsigned int shift,
		unsigned int value) {
	unsigned int mask = ((1 << bit_width) - 1) << shift;
	unsigned int old;

	value = (value << shift) & mask;
	old = M4U_ReadReg32(M4UBase, Reg);
	M4U_WriteReg32(M4UBase, Reg, (old & (~mask)) | value);
	return (old & mask) >> shift;
}

static inline void m4uHw_set_field_by_mask(
	unsigned long M4UBase, unsigned int reg,
					   unsigned long mask, unsigned int val)
{
	unsigned int regval;

	regval = M4U_ReadReg32(M4UBase, reg);
	regval = (regval & (~mask)) | val;
	M4U_WriteReg32(M4UBase, reg, regval);
}

static inline unsigned int m4uHw_get_field_by_mask(
		unsigned long M4UBase, unsigned int reg,
						   unsigned int mask)
{
	return M4U_ReadReg32(M4UBase, reg) & mask;
}




#endif
