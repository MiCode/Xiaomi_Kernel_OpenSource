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
#ifndef __APUSYS_MNOC_HW_H__
#define __APUSYS_MNOC_HW_H__

/*
 * BIT Operation
 */
#undef  BIT
#define BIT(_bit_) (unsigned int)(1 << (_bit_))
#define BITS(_bits_, _val_) ((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_) (((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_) (((_val_) & \
(BITMASK(_bits_))) >> ((0) ? _bits_))


/**
 * Read/Write a field of a register.
 * @addr:       Address of the register
 * @range:      The field bit range in the form of MSB:LSB
 * @val:        The value to be written to the field
 */
//#define mnoc_read(addr)	ioread32((void*) (uintptr_t) addr)
#define mnoc_read(addr)	__raw_readl((void __iomem *) (uintptr_t) (addr))
#define mnoc_write(addr,  val) \
__raw_writel(val, (void __iomem *) (uintptr_t) addr)
#define mnoc_read_field(addr, range) GET_BITS_VAL(range, mnoc_read(addr))
#define mnoc_write_field(addr, range, val) mnoc_write(addr, (mnoc_read(addr) \
& ~(BITMASK(range))) | BITS(range, val))
#define mnoc_set_bit(addr, set) mnoc_write(addr, (mnoc_read(addr) | (set)))
#define mnoc_clr_bit(addr, clr) mnoc_write(addr, (mnoc_read(addr) & ~(clr)))

enum apu_qos_mni {
	MNI_VPU0,
	MNI_VPU1,
	MNI_VPU2,
	MNI_MDLA0_0,
	MNI_MDLA0_1,
	MNI_MDLA1_0,
	MNI_MDLA1_1,
	MNI_EDMA0,
	MNI_EDMA1,
	MNI_MD32,

	NR_APU_QOS_MNI
};

enum apu_qos_engine {
	VPU0,
	VPU1,
	VPU2,
	MDLA0,
	MDLA1,
	EDMA0,
	EDMA1,
	MD32,

	NR_APU_QOS_ENGINE
};

#define MDLA_NUM (2)
#define MNOC_RT_NUM (5)

/* 0x1906E000 */
#define APU_NOC_TOP_BASEADDR mnoc_base
/* 0x19001000 */
#define MNOC_INT_BASEADDR mnoc_int_base
/* 0x10001000 */
#define MNOC_SLP_PROT_BASEADDR1 mnoc_slp_prot_base1
/* 0x10215000 */
#define MNOC_SLP_PROT_BASEADDR2 mnoc_slp_prot_base2

/* MNoC register definition */
#define MNOC_INT_EN (MNOC_INT_BASEADDR + 0x80)
#define MNOC_INT_STA (MNOC_INT_BASEADDR + 0x34)

/* #define APU_NOC_TOP_BASEADDR			(0x1906E000) */
#define APU_NOC_TOP_ADDR			(0x1906E000)
#define APU_NOC_TOP_RANGE			(0x2000)

#define MNI_QOS_CTRL_BASE (APU_NOC_TOP_BASEADDR + 0x1000)
#define MNI_QOS_INFO_BASE (APU_NOC_TOP_BASEADDR + 0x1800)
#define MNI_QOS_REG(base, reg_num, mni_offset) \
(base + reg_num*16*4 + mni_offset*4)

#define REQ_RT_PMU_BASE (APU_NOC_TOP_BASEADDR + 0x500)
#define RSP_RT_PMU_BASE (APU_NOC_TOP_BASEADDR + 0x600)
#define MNOC_RT_PMU_REG(base, reg_num, rt_num)	(base + reg_num*5*4 + rt_num*4)

#define MNI_QOS_IRQ_FLAG (APU_NOC_TOP_BASEADDR + 0x18)
#define ADDR_DEC_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x30)
#define MST_PARITY_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x38)
#define SLV_PARITY_ERR_FLA (APU_NOC_TOP_BASEADDR + 0x3C)
#define MST_MISRO_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x40)
#define SLV_MISRO_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x44)
#define REQRT_MISRO_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x48)
#define RSPRT_MISRO_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x4C)
#define REQRT_TO_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x50)
#define RSPRT_TO_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x54)
#define REQRT_CBUF_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x188)
#define RSPRT_CBUF_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x18C)
#define MST_CRDT_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x190)
#define SLV_CRDT_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x194)
#define REQRT_CRDT_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x198)
#define RSPRT_CRDT_ERR_FLAG (APU_NOC_TOP_BASEADDR + 0x19C)

void mnoc_qos_reg_init(void);
void mnoc_reg_init(void);
bool mnoc_check_int_status(void);

#endif
