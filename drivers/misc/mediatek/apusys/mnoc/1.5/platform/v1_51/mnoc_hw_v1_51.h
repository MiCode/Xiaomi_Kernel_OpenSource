/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MNOC_HW_H__
#define __APUSYS_MNOC_HW_H__


enum apu_qos_mni {
	MNI_VPU0,
	MNI_VPU1,
	MNI_MDLA0_0,
	MNI_MDLA0_1,
	MNI_EDMA0,
	MNI_MD32,

	NR_APU_QOS_MNI
};

enum apu_qos_engine {
	APU_QOS_ENGINE_VPU0,
	APU_QOS_ENGINE_VPU1,
	APU_QOS_ENGINE_MDLA0,
	APU_QOS_ENGINE_EDMA0,
	APU_QOS_ENGINE_MD32,

	NR_APU_QOS_ENGINE
};

enum mni_int_sta {
	MNOC_INT_MNI_QOS_IRQ_FLAG,
	MNOC_INT_ADDR_DEC_ERR_FLAG,
	MNOC_INT_MST_PARITY_ERR_FLAG,
	MNOC_INT_MST_MISRO_ERR_FLAG,
	MNOC_INT_MST_CRDT_ERR_FLAG,

	NR_MNI_INT_STA
};

enum sni_int_sta {
	MNOC_INT_SLV_PARITY_ERR_FLA,
	MNOC_INT_SLV_MISRO_ERR_FLAG,
	MNOC_INT_SLV_CRDT_ERR_FLAG,

	NR_SNI_INT_STA
};

enum rt_int_sta {
	MNOC_INT_REQRT_MISRO_ERR_FLAG,
	MNOC_INT_RSPRT_MISRO_ERR_FLAG,
	MNOC_INT_REQRT_TO_ERR_FLAG,
	MNOC_INT_RSPRT_TO_ERR_FLAG,
	MNOC_INT_REQRT_CBUF_ERR_FLAG,
	MNOC_INT_RSPRT_CBUF_ERR_FLAG,
	MNOC_INT_REQRT_CRDT_ERR_FLAG,
	MNOC_INT_RSPRT_CRDT_ERR_FLAG,

	NR_RT_INT_STA
};

#define NR_APU_ENGINE_VPU (2)
#define NR_APU_ENGINE_MDLA (1)
#define NR_APU_ENGINE_EDMA (1)

#define NR_MNOC_RT (5)
#define NR_GROUP (5)
#define NR_MNI_PER_GROUP (2)
#define NR_SNI_PER_GROUP (2)
#define NR_PMU_CNTR_PER_GRP (16)
#define NR_MNOC_PMU_CNTR (NR_PMU_CNTR_PER_GRP*NR_GROUP)

#define APU_NOC_TOP_BASE (0x1906E000)

#define APU_NOC_GROUP0 (0x0)
#define APU_NOC_GROUP1 (0x2000)
#define APU_NOC_GROUP2 (0x4000)
#define APU_NOC_GROUP3 (0x6000)
#define APU_NOC_GROUP4 (0x8000)

#define MNI_QOS_CTRL (0x1000)

#define QOS_MON_SLV_SEL_DRAM (0x30)
#define QOS_MON_SLV_SEL_DRAM_TCM (0x3F0)

/* 0x1906E000 */
#define APU_NOC_TOP_BASEADDR mnoc_base
/* 0x19001000 */
#define MNOC_INT_BASEADDR mnoc_int_base
/* 0x19020000 */
#define MNOC_APU_CONN_BASEADDR mnoc_apu_conn_base
/* 0x10001000 */
#define MNOC_SLP_PROT_BASEADDR1 mnoc_slp_prot_base1
/* 0x10215000 */
#define MNOC_SLP_PROT_BASEADDR2 mnoc_slp_prot_base2

/* MNoC register definition */
#define APUSYS_INT_EN (MNOC_INT_BASEADDR + 0x80)
#define APUSYS_INT_STA (MNOC_INT_BASEADDR + 0x34)

#define GRP_INT_MAP_0 (0x3 << 0)
#define GRP_INT_MAP_1 (0x3 << 23)
#define GRP_INT_MAP_2 (0x3 << 25)
#define GRP_INT_MAP_3 (0x3 << 27)
#define GRP_INT_MAP_4 (0x3 << 29)
/* bit 0~1, 23~30 */
#define MNOC_INT_MAP (GRP_INT_MAP_0 | GRP_INT_MAP_1 |\
					  GRP_INT_MAP_2 | GRP_INT_MAP_3 |\
					  GRP_INT_MAP_4)

#define APU_TCM_HASH_TRUNCATE_CTRL0 (MNOC_APU_CONN_BASEADDR + 0x7C)

/* #define APU_NOC_TOP_BASEADDR			(0x1906E000) */
#define APU_NOC_TOP_ADDR (0x1906E000)
#define APU_NOC_TOP_RANGE (0xA000)

#define APU_NOC_PMU_ADDR (0x1906E200)
#define APU_NOC_PMU_RANGE (0x41C)
#define APU_NOC_GRP_REG_SZ (0x2000)

#define MNI_QOS_REG(group, reg_offset, reg_num, mni_offset)\
	(APU_NOC_TOP_BASEADDR + grp_base_addr[group] + reg_offset +\
	reg_num*grp_nr_mni[group]*4 + mni_offset*4)

#define REQ_RT_PMU (0x500)
#define RSP_RT_PMU (0x600)

#define MNOC_RT_PMU_REG(group, reg_offset, reg_num)\
	(APU_NOC_TOP_BASEADDR + grp_base_addr[group] + reg_offset + reg_num*4)

#define MISC_CTRL (0x0)
#define SLV_QOS_CTRL0 (0x10)
#define MNI_QOS_IRQ_FLAG (0x18)
#define ADDR_DEC_ERR_FLAG (0x30)
#define MST_PARITY_ERR_FLAG (0x38)
#define SLV_PARITY_ERR_FLA (0x3C)
#define MST_MISRO_ERR_FLAG (0x40)
#define SLV_MISRO_ERR_FLAG (0x44)
#define REQRT_MISRO_ERR_FLAG (0x48)
#define RSPRT_MISRO_ERR_FLAG (0x4C)
#define REQRT_TO_ERR_FLAG (0x50)
#define RSPRT_TO_ERR_FLAG (0x54)
#define REQRT_CBUF_ERR_FLAG (0x188)
#define RSPRT_CBUF_ERR_FLAG (0x18C)
#define MST_CRDT_ERR_FLAG (0x190)
#define SLV_CRDT_ERR_FLAG (0x194)
#define REQRT_CRDT_ERR_FLAG (0x198)
#define RSPRT_CRDT_ERR_FLAG (0x19C)
#define APU_NOC_PMU_CTRL0 (0x200)

#define PMU_COUNTER0_OUT (0x240)

#define QG_LT_THL_PRE_ULTRA (0x1FFF)
#define QG_LT_THH_PRE_ULTRA (0x1FFF)

#define MNOC_REG(group, reg_offset)\
	(APU_NOC_TOP_BASEADDR + grp_base_addr[group] + reg_offset)

struct int_sta_info {
	uint32_t reg_val;
	uint64_t timestamp;
};

struct mnoc_int_dump {
	uint32_t count;
	struct int_sta_info mni_int_sta[NR_MNI_INT_STA];
	struct int_sta_info sni_int_sta[NR_SNI_INT_STA];
	struct int_sta_info rt_int_sta[NR_RT_INT_STA];
	struct int_sta_info sw_irq_sta;
};


#endif
