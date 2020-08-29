// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*
 *=============================================================
 * Include files
 *=============================================================
 */


/* system includes */
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include <linux/device.h>
#include <linux/io.h>

#include "apusys_device.h"

#include "v1_50/mnoc_hw_v1_50.h"
#include "mnoc_drv.h"
#include "mnoc_util.h"



enum APUSYS_MNOC_SMC_ID {
	MNOC_INFRA2APU_SRAM_EN,
	MNOC_INFRA2APU_SRAM_DIS,
	MNOC_APU2INFRA_BUS_PROTECT_EN,
	MNOC_APU2INFRA_BUS_PROTECT_DIS,

	NR_APUSYS_MNOC_SMC_ID
};

static const char * const mni_int_sta_string[] = {
	"MNI_QOS_IRQ_FLAG",
	"ADDR_DEC_ERR_FLAG",
	"MST_PARITY_ERR_FLAG",
	"MST_MISRO_ERR_FLAG",
	"MST_CRDT_ERR_FLAG",
};

static const char * const sni_int_sta_string[] = {
	"SLV_PARITY_ERR_FLA",
	"SLV_MISRO_ERR_FLAG",
	"SLV_CRDT_ERR_FLAG",
};

static const char * const rt_int_sta_string[] = {
	"REQRT_MISRO_ERR_FLAG",
	"RSPRT_MISRO_ERR_FLAG",
	"REQRT_TO_ERR_FLAG",
	"RSPRT_TO_ERR_FLAG",
	"REQRT_CBUF_ERR_FLAG",
	"RSPRT_CBUF_ERR_FLAG",
	"REQRT_CRDT_ERR_FLAG",
	"RSPRT_CRDT_ERR_FLAG",
};

static const char * const mni_map_string[] = {
	"MNI_MDLA0_0",
	"MNI_MDLA0_1",
	"MNI_MDLA1_0",
	"MNI_MDLA1_1",
	"MNI_ADL",
	"MNI_XPU",
	"MNI_VPU0",
	"MNI_EDMA_0",
	"MNI_EDMA_1",
	"MNI_NONE",
	"MNI_VPU1",
	"MNI_VPU2",
	"MNI_MD32",
	"MNI_NONE",
	"MNI_NONE",
	"MNI_NONE",
};

static const char * const sni_map_string[] = {
	"SNI_TCM0",
	"SNI_TCM1",
	"SNI_TCM2",
	"SNI_TCM3",
	"SNI_EMI2",
	"SNI_EMI3",
	"SNI_EMI0",
	"SNI_EMI1",
	"SNI_VPU0",
	"SNI_EXT",
	"SNI_VPU1",
	"SNI_VPU2",
	"SNI_NONE",
	"SNI_MD32",
	"SNI_NONE",
	"SNI_NONE",
};

static const unsigned int mni_int_sta_offset[NR_MNI_INT_STA] = {
	MNI_QOS_IRQ_FLAG,
	ADDR_DEC_ERR_FLAG,
	MST_PARITY_ERR_FLAG,
	MST_MISRO_ERR_FLAG,
	MST_CRDT_ERR_FLAG,
};

static const unsigned int sni_int_sta_offset[NR_SNI_INT_STA] = {
	SLV_PARITY_ERR_FLA,
	SLV_MISRO_ERR_FLAG,
	SLV_CRDT_ERR_FLAG,
};

static const unsigned int rt_int_sta_offset[NR_RT_INT_STA] = {
	REQRT_MISRO_ERR_FLAG,
	RSPRT_MISRO_ERR_FLAG,
	REQRT_TO_ERR_FLAG,
	RSPRT_TO_ERR_FLAG,
	REQRT_CBUF_ERR_FLAG,
	RSPRT_CBUF_ERR_FLAG,
	REQRT_CRDT_ERR_FLAG,
	RSPRT_CRDT_ERR_FLAG,
};

/**
 * MNI offset 0 -> MNI05_QOS_CTRL0
 * MNI offset 1 -> MNI06_QOS_CTRL0
 * MNI offset 2 -> MNI07_QOS_CTRL0
 * MNI offset 3 -> MNI08_QOS_CTRL0
 * MNI offset 4 -> MNI13_QOS_CTRL0
 * MNI offset 5 -> MNI15_QOS_CTRL0
 * MNI offset 6 -> MNI00_QOS_CTRL0
 * MNI offset 7 -> MNI09_QOS_CTRL0
 * MNI offset 8 -> MNI10_QOS_CTRL0
 * MNI offset 9 -> MNI03_QOS_CTRL0
 * MNI offset 10 -> MNI01_QOS_CTRL0
 * MNI offset 11 -> MNI02_QOS_CTRL0
 * MNI offset 12 -> MNI04_QOS_CTRL0
 * MNI offset 13 -> MNI11_QOS_CTRL0
 * MNI offset 14 -> MNI12_QOS_CTRL0
 * MNI offset 15 -> MNI14_QOS_CTRL0
 * VPU0		-> MNI00 -> offset 6
 * VPU1		-> MNI01 -> offset 10
 * VPU2		-> MNI02 -> offset 11
 * MDLA0_0	-> MNI05 -> offset 0
 * MDLA0_1	-> MNI06 -> offset 1
 * MDLA1_0	-> MNI07 -> offset 2
 * MDLA1_1	-> MNI08 -> offset 3
 * EDMA_0	-> MNI09 -> offset 7
 * EDMA_1	-> MNI10 -> offset 8
 * MD32		-> MNI04 -> offset 12
 */
static char mni_map[NR_APU_QOS_MNI] = {6, 10, 11, 0, 1, 2, 3, 7, 8, 12};

static bool arr_mni_pre_ultra[NR_APU_QOS_MNI] = {0};
static bool arr_mni_lt_guardian_pre_ultra[NR_APU_QOS_MNI] = {0};

static struct mnoc_int_dump mnoc_int_dump;

/*
 * south -> iommu2 -> emi1
 * north -> iommu3 -> emi0
 */
static int iommu_emi_rule[NR_IOMMU] = {1, 0};
static phys_addr_t iommu_tfrp[NR_IOMMU];
static void *protect[NR_IOMMU];
static bool iommu_tfrp_init_flag;

/* register to apusys power on callback */
static void mnoc_qos_reg_init(void)
{
	int i;
	unsigned long flags;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);

	/* time slot setting */
	for (i = 0; i < NR_APU_QOS_MNI; i++) {
		/* QoS watcher BW time slot set to 1.26 ms */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			2, mni_map[i]), 1:0, 0x1);
		/* QoS guardian BW time slot set to 1.26 ms */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			16, mni_map[i]), 1:0, 0x1);

		/* 26M cycle count = {QW_LT_PRD,8'h0} << QW_LT_PRD_SHF */
		/* QW_LT_PRD = 0x80, QW_LT_PRD_SHF = 0x0 */
		/* QoS watcher LT time slot set to 1.26 ms */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			5, mni_map[i]), 7:0, 0x80);
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			5, mni_map[i]), 10:8, 0x0);
		/* QoS guardian LT time slot set to 1.26 ms */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			19, mni_map[i]), 7:0, 0x80);
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			19, mni_map[i]), 10:8, 0x0);

		/* MNI to SNI path setting */
		/* set QoS guardian to monitor DRAM only */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			31, mni_map[i]), 31:16, 0xF000);
		/* set QoS watcher to monitor DRAM+TCM */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			31, mni_map[i]), 15:0, 0xFF00);

		/* set QW_BW_INT_EN = 1 to enable monitor */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			1, mni_map[i]), 2:2, 0x1);
		/* set QG_BW_INT_EN = 1 to enable monitor */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			15, mni_map[i]), 2:2, 0x1);
		/* set QW_LT_INT_EN = 1 to enable monitor */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			1, mni_map[i]), 4:4, 0x1);
		/* set QG_LT_INT_EN = 1 to enable monitor */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			15, mni_map[i]), 4:4, 0x1);
	}

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}


/* register to apusys power on callback */
static void mnoc_reg_init(void)
{
	int i;
	unsigned long flags;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);

	/* EMI fine tune: SLV12_QOS ~ SLV15_QOS = 0x7 */
	mnoc_write_field(MNOC_REG(SLV_QOS_CTRL1), 31:16, 0x7777);

	/* set request router timeout interrupt */
	for (i = 0; i < NR_MNOC_RT; i++) {
		/* all VC enabled */
		mnoc_write(MNOC_RT_PMU_REG(REQ_RT_PMU_BASE, 3, i), 0xFFFFFFFF);
		mnoc_write(MNOC_RT_PMU_REG(REQ_RT_PMU_BASE, 4, i), 0xFFFFFFFF);
		/* set timeout threshold to 510 cycles */
		mnoc_write_field(MNOC_RT_PMU_REG(REQ_RT_PMU_BASE, 2, i),
			8:0, 510);
		/* enable timeout counting */
		mnoc_write_field(MNOC_RT_PMU_REG(REQ_RT_PMU_BASE, 2, i),
			31:31, 1);
	}

	/* set response router timeout interrupt */
	for (i = 0; i < NR_MNOC_RT; i++) {
		/* all VC enabled */
		mnoc_write(MNOC_RT_PMU_REG(RSP_RT_PMU_BASE, 3, i), 0xFFFFFFFF);
		mnoc_write(MNOC_RT_PMU_REG(RSP_RT_PMU_BASE, 4, i), 0xFFFFFFFF);
		/* set timeout threshold to 510 cycles */
		mnoc_write_field(MNOC_RT_PMU_REG(RSP_RT_PMU_BASE, 2, i),
			8:0, 510);
		/* enable timeout counting */
		mnoc_write_field(MNOC_RT_PMU_REG(RSP_RT_PMU_BASE, 2, i),
			31:31, 1);
	}

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}

static void set_mni_pre_ultra_locked(unsigned int idx, bool endis)
{
	unsigned int map, val;

	LOG_DEBUG("+\n");

	/* bit 24 : force AW urgent enable
	 * bit 25 : force AR urgent enable
	 * bit 29 : AW pre-urgent value
	 * bit 31 : AR pre-urgent value
	 */
	map = (1 << 24) | (1 << 25) | (1 << 29) | (1 << 31);

	val = mnoc_read(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
		15, mni_map[idx]));
	if (endis)
		mnoc_write(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			15, mni_map[idx]), (val | map));
	else
		mnoc_write(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			15, mni_map[idx]), (val & (~map)));

	LOG_DEBUG("-\n");
}

void mnoc_set_mni_pre_ultra_v1_50(int dev_type, int dev_core, bool endis)
{
	unsigned long flags;
	unsigned int idx;
	int core;

	LOG_DEBUG("+\n");

	//core = apusys_dev_to_core_id(dev_type, dev_core);
	core = mnoc_drv.dev_2_core_id(dev_type, dev_core);

	if (core == -1) {
		LOG_ERR("illegal dev_type(%d), dev_core(%d)\n",
			dev_type, dev_core);
		return;
	}

	spin_lock_irqsave(&mnoc_spinlock, flags);

	switch (core) {
	case APU_QOS_ENGINE_VPU0:
	case APU_QOS_ENGINE_VPU1:
	case APU_QOS_ENGINE_VPU2:
		idx = core;
		arr_mni_pre_ultra[idx] = endis;
		if (mnoc_reg_valid)
			set_mni_pre_ultra_locked(idx, endis);
		break;
	case APU_QOS_ENGINE_MDLA0:
	case APU_QOS_ENGINE_MDLA1:
		idx = (core - APU_QOS_ENGINE_MDLA0) * 2 + MNI_MDLA0_0;
		arr_mni_pre_ultra[idx] = endis;
		arr_mni_pre_ultra[idx+1] = endis;
		if (mnoc_reg_valid) {
			set_mni_pre_ultra_locked(idx, endis);
			set_mni_pre_ultra_locked(idx+1, endis);
		}
		break;
	case APU_QOS_ENGINE_EDMA0:
	case APU_QOS_ENGINE_EDMA1:
	case APU_QOS_ENGINE_MD32:
		idx = core - APU_QOS_ENGINE_EDMA0 + MNI_EDMA0;
		arr_mni_pre_ultra[idx] = endis;
		if (mnoc_reg_valid)
			set_mni_pre_ultra_locked(idx, endis);
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}

static void set_lt_guardian_pre_ultra_locked(unsigned int idx, bool endis)
{
	LOG_DEBUG("+\n");

	if (endis) {
		/* set QG_LT_THH */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 20, mni_map[idx]),
			12:0, QG_LT_THH_PRE_ULTRA);
		/* set QG_LT_THL */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 20, mni_map[idx]),
			28:16, QG_LT_THL_PRE_ULTRA);
		/* set QCC_LT_LV_DIS[3:0] = 4'b1001 */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 29, mni_map[idx]),
			11:8, 0x9);
		/* set STM mode QCC_LT_TH_MODE = 1 */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 29, mni_map[idx]),
			16:16, 0x1);
		/* set QCC_TOP_URGENT_EN = 0 */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 29, mni_map[idx]),
			19:19, 0x0);
	} else {
		/* set QG_LT_THH */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 20, mni_map[idx]),
			12:0, 0x0);
		/* set QG_LT_THL */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 20, mni_map[idx]),
			28:16, 0x0);
		/* set QCC_LT_LV_DIS[3:0] = 4'b0000 */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 29, mni_map[idx]),
			11:8, 0x0);
		/* set STM mode QCC_LT_TH_MODE = 0 */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 29, mni_map[idx]),
			16:16, 0x0);
		/* set QCC_TOP_URGENT_EN = 1 */
		mnoc_write_field(
			MNI_QOS_REG(MNI_QOS_CTRL_BASE, 29, mni_map[idx]),
			19:19, 0x1);
	}

	LOG_DEBUG("-\n");
}

void mnoc_set_lt_guardian_pre_ultra_v1_50(int dev_type, int dev_core, bool endis)
{
	unsigned long flags;
	unsigned int idx;
	int core;

	LOG_DEBUG("+\n");

	//core = apusys_dev_to_core_id(dev_type, dev_core);
	core = mnoc_drv.dev_2_core_id(dev_type, dev_core);

	if (core == -1) {
		LOG_ERR("illegal dev_type(%d), dev_core(%d)\n",
			dev_type, dev_core);
		return;
	}

	spin_lock_irqsave(&mnoc_spinlock, flags);

	switch (core) {
	case APU_QOS_ENGINE_VPU0:
	case APU_QOS_ENGINE_VPU1:
	case APU_QOS_ENGINE_VPU2:
		idx = core;
		arr_mni_lt_guardian_pre_ultra[idx] = endis;
		if (mnoc_reg_valid)
			set_lt_guardian_pre_ultra_locked(idx, endis);
		break;
	case APU_QOS_ENGINE_MDLA0:
	case APU_QOS_ENGINE_MDLA1:
		idx = (core - APU_QOS_ENGINE_MDLA0) * 2 + MNI_MDLA0_0;
		arr_mni_lt_guardian_pre_ultra[idx] = endis;
		arr_mni_lt_guardian_pre_ultra[idx+1] = endis;
		if (mnoc_reg_valid) {
			set_lt_guardian_pre_ultra_locked(idx, endis);
			set_lt_guardian_pre_ultra_locked(idx+1, endis);
		}
		break;
	case APU_QOS_ENGINE_EDMA0:
	case APU_QOS_ENGINE_EDMA1:
	case APU_QOS_ENGINE_MD32:
		idx = core - APU_QOS_ENGINE_EDMA0 + MNI_EDMA0;
		arr_mni_lt_guardian_pre_ultra[idx] = endis;
		if (mnoc_reg_valid)
			set_lt_guardian_pre_ultra_locked(idx, endis);
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}

static void print_int_sta_info(struct seq_file *m,
	const char *str, struct int_sta_info *info)
{
	uint64_t t, nanosec_rem;

	t = info->timestamp;
	nanosec_rem = do_div(t, 1000000000);

	INT_STA_PRINTF(m, "%s = 0x%x [%lu.%06lu]\n",
		str,
		info->reg_val,
		(unsigned long) t,
		(unsigned long) (nanosec_rem / 1000));
}




/* ============== for IOMMU TFRP setting ============== */

static unsigned int unary_xor(uint32_t a)
{
	unsigned int ret = 0;
	int i;

	for (i = 0; i < 32; i++)
		ret ^= ((a >> i) & 0x1);

	return ret;
}

/*
 * Due to dual iommu depolyment on apusys, buffer allocation
 * for tfrp reg setting needs to aware emi hash rule to prevent
 * possible emi dispatch violation when iommu translation fault occurs.
 * -> Need to ensure the buffer allocated for north/south iommu
 * tfrp dispatched to correct emi port according to hash rule
 */
int mnoc_alloc_iommu_tfrp(void)
{
	void *infra_ao_base;
	uint64_t protect_va;
	phys_addr_t protect_base;
	uint32_t hash_rule;
	uint32_t dispatch_emi = 0, dispatch_channel = 0;
	uint32_t addr_extract = 0, offset = 0;
	unsigned int emi_id = 0, channel_id = 0;
	int i;

	LOG_DEBUG("+\n");

	infra_ao_base = ioremap(INFRA_AO_BASE, INFRA_AO_REG_SIZE);
	hash_rule = mnoc_read(infra_ao_base + EMI_HASH_RULE_OFFSET);
	iounmap(infra_ao_base);

	if (((hash_rule & 0x100) == 0) && ((hash_rule & 0xFF0000) != 0)) {
		dispatch_emi = (hash_rule & 0xF00000) >> 20;
		dispatch_channel = (hash_rule & 0xF0000) >> 16;
	} else {
		dispatch_emi = (hash_rule & 0xF0) >> 4;
		dispatch_channel = hash_rule & (0xF);
	}

	LOG_INFO("hash_rule = 0x%x(0x%x/0x%x)\n",
		hash_rule, dispatch_emi, dispatch_channel);

	for (i = 0; i < NR_IOMMU; i++) {
		protect[i] = kzalloc((size_t) (APU_TFRP_ALIGN * 2), GFP_KERNEL);
		if (!protect[i]) {
			LOG_ERR("apu_iommu(%d) tfrd buf allocation fail\n", i);
			return -ENOMEM;
		}
		protect_va = virt_to_phys(protect[i]);
		protect_base = ALIGN(protect_va, APU_TFRP_ALIGN);

		if (i == 0) {
			/* South IOMMU, dispatch emi = 1 */
			offset = (((~dispatch_emi) & 0xF) + 1) << 8;
			protect_base += offset;
		} else if (i == 1) {
			/* North IOMMU, dispatch emi = 0 */
			offset = ((~dispatch_emi) & 0xF) << 8;
			protect_base += offset;
		}

		/* if (protect_va > 0xffffffff)
		 *	protect_base |= ((protect_va >> 32) &
		 *			F_RP_PA_REG_BIT32);
		 */

		addr_extract = (protect_base & (0xf00)) >> 8;
		emi_id = unary_xor(addr_extract & dispatch_emi);
		channel_id = unary_xor(addr_extract & dispatch_channel);

		LOG_INFO("apu_iommu(%d): protect_va = 0x%llx, offset = 0x%x\n",
			i, protect_va, offset);
		LOG_INFO("emi/channel(%d/%d), protect_base = 0x%llx\n",
			emi_id, channel_id, protect_base);

		if ((dispatch_emi != 0) && (emi_id != iommu_emi_rule[i])) {
			LOG_ERR("apu_iommu(%d) tfrd violates hash rule\n", i);
			return -1;
		}

		iommu_tfrp[i] = protect_base;
	}

	LOG_DEBUG("-\n");
	return 0;
}

phys_addr_t get_apu_iommu_tfrp_v1_50(unsigned int id)
{
	int ret;

	if (id >= NR_IOMMU) {
		LOG_ERR("id >= NR_IOMMU(%d)\n", NR_IOMMU);
		return 0;
	}

	if (!iommu_tfrp_init_flag) {
		ret = mnoc_alloc_iommu_tfrp();
		if (ret)
			LOG_ERR("APU IOMMU tfrp allocation fail\n");
		iommu_tfrp_init_flag = true;
	}

	return iommu_tfrp[id];
}


int apusys_dev_to_core_id_v1_50(int dev_type, int dev_core)
{
	int ret = -1;

	switch (dev_type) {
	case APUSYS_DEVICE_VPU:
	case APUSYS_DEVICE_VPU_RT:
		if (dev_core >= 0 && dev_core < NR_APU_ENGINE_VPU)
			ret = dev_core;
		break;
	case APUSYS_DEVICE_MDLA:
	case APUSYS_DEVICE_MDLA_RT:
		if (dev_core >= 0 && dev_core < NR_APU_ENGINE_MDLA)
			ret = NR_APU_ENGINE_VPU + dev_core;
		break;
	case APUSYS_DEVICE_EDMA:
		if (dev_core >= 0 && dev_core < NR_APU_ENGINE_EDMA)
			ret = NR_APU_ENGINE_VPU + NR_APU_ENGINE_MDLA + dev_core;
		break;
	/* for midware UT */
	case APUSYS_DEVICE_SAMPLE:
	case APUSYS_DEVICE_SAMPLE_RT:
		ret = NR_APU_QOS_ENGINE;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}



void mnoc_int_endis_v1_50(bool endis)
{
	unsigned long flags;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);

	if (!mnoc_reg_valid) {
		spin_unlock_irqrestore(&mnoc_spinlock, flags);
		return;
	}
	if (endis)
		mnoc_set_bit(APUSYS_INT_EN, MNOC_INT_MAP);
	else
		mnoc_clr_bit(APUSYS_INT_EN, MNOC_INT_MAP);

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}

/*
 * print mnoc interrupt count and
 * last snapshot when each type of interrupt happened
 */
void print_int_sta_v1_50(struct seq_file *m)
{
	int idx, ni_idx;
	uint64_t t, nanosec_rem;
	unsigned int val;

	t = sched_clock();
	nanosec_rem = do_div(t, 1000000000);

	INT_STA_PRINTF(m, "[%lu.%06lu]\n",
		(unsigned long) t, (unsigned long) (nanosec_rem / 1000));

	INT_STA_PRINTF(m, "count = %d\n", mnoc_int_dump.count);

	print_int_sta_info(m, "apusys_int_sta",
		&(mnoc_int_dump.apusys_int_sta));

	for (idx = 0; idx < NR_MNI_INT_STA; idx++) {
		print_int_sta_info(m, mni_int_sta_string[idx],
			&(mnoc_int_dump.mni_int_sta[idx]));
		val = mnoc_int_dump.mni_int_sta[idx].reg_val;
		for (ni_idx = 0; ni_idx < NR_MNOC_MNI; ni_idx++)
			if ((val & (1 << ni_idx)) != 0)
				INT_STA_PRINTF(m, "\t-From %s\n",
					mni_map_string[ni_idx]);
	}

	for (idx = 0; idx < NR_SNI_INT_STA; idx++) {
		print_int_sta_info(m, sni_int_sta_string[idx],
			&(mnoc_int_dump.sni_int_sta[idx]));
		val = mnoc_int_dump.sni_int_sta[idx].reg_val;
		for (ni_idx = 0; ni_idx < NR_MNOC_SNI; ni_idx++)
			if ((val & (1 << ni_idx)) != 0)
				INT_STA_PRINTF(m, "\t-From %s\n",
					sni_map_string[ni_idx]);
	}

	for (idx = 0; idx < NR_RT_INT_STA; idx++) {
		print_int_sta_info(m, rt_int_sta_string[idx],
			&(mnoc_int_dump.rt_int_sta[idx]));
		val = mnoc_int_dump.rt_int_sta[idx].reg_val;
		for (ni_idx = 0; ni_idx < NR_MNOC_RT; ni_idx++)
			if ((val & (1 << ni_idx)) != 0)
				INT_STA_PRINTF(m, "\t-From RT %d\n", ni_idx);
	}

	print_int_sta_info(m, "sw_irq_sta",
		&(mnoc_int_dump.sw_irq_sta));
}

int mnoc_check_int_status_v1_50(void)
{
	int mnoc_irq_triggered = 0;
	unsigned int val, int_sta;
	int int_idx, ni_idx;
	struct mnoc_int_dump *d = &mnoc_int_dump;
	uint64_t cur_timestamp;

	LOG_DEBUG("+\n");

	int_sta = mnoc_read(APUSYS_INT_STA);

	LOG_DEBUG("APUSYS INT STA = 0x%x\n", int_sta);

	cur_timestamp = sched_clock();

	if (int_sta != 0) {
		d->apusys_int_sta.reg_val = int_sta;
		d->apusys_int_sta.timestamp = cur_timestamp;
	}

	if ((int_sta & MNOC_INT_MAP) == 0)
		return mnoc_irq_triggered;

	d->count++;

	for (int_idx = 0; int_idx < NR_MNI_INT_STA; int_idx++) {
		val = mnoc_read(MNOC_REG(mni_int_sta_offset[int_idx]));
		if ((val & 0xFFFF) != 0) {
			d->mni_int_sta[int_idx].reg_val = val;
			d->mni_int_sta[int_idx].timestamp = cur_timestamp;
			LOG_DEBUG("%s = 0x%x\n",
				mni_int_sta_string[int_idx], val);
			for (ni_idx = 0; ni_idx < NR_MNOC_MNI; ni_idx++)
				if ((val & (1 << ni_idx)) != 0)
					LOG_DEBUG("From %s\n",
						mni_map_string[ni_idx]);
			mnoc_write_field(
				MNOC_REG(mni_int_sta_offset[int_idx]),
				15:0, 0xFFFF);
			mnoc_irq_triggered = 1;
		}
	}

	for (int_idx = 0; int_idx < NR_SNI_INT_STA; int_idx++) {
		val = mnoc_read(MNOC_REG(sni_int_sta_offset[int_idx]));
		if ((val & 0xFFFF) != 0) {
			d->sni_int_sta[int_idx].reg_val = val;
			d->sni_int_sta[int_idx].timestamp = cur_timestamp;
			LOG_DEBUG("%s = 0x%x\n",
				sni_int_sta_string[int_idx], val);
			for (ni_idx = 0; ni_idx < NR_MNOC_SNI; ni_idx++)
				if ((val & (1 << ni_idx)) != 0)
					LOG_DEBUG("From %s\n",
						sni_map_string[ni_idx]);
			mnoc_write_field(
				MNOC_REG(sni_int_sta_offset[int_idx]),
				15:0, 0xFFFF);
			mnoc_irq_triggered = 1;
		}
	}

	for (int_idx = 0; int_idx < NR_RT_INT_STA; int_idx++) {
		val = mnoc_read(MNOC_REG(rt_int_sta_offset[int_idx]));
		if ((val & 0x1F) != 0) {
			d->rt_int_sta[int_idx].reg_val = val;
			d->rt_int_sta[int_idx].timestamp = cur_timestamp;
			LOG_DEBUG("%s = 0x%x\n",
				rt_int_sta_string[int_idx], val);
			for (ni_idx = 0; ni_idx < NR_MNOC_RT; ni_idx++)
				if ((val & (1 << ni_idx)) != 0)
					LOG_DEBUG("From RT %d\n", ni_idx);
			mnoc_write_field(
				MNOC_REG(rt_int_sta_offset[int_idx]),
				4:0, 0x1F);
			/* timeout interrupt may be only perf
			 * hint but not actually hang
			 */
			if (mnoc_irq_triggered != 1 && (
				int_idx == MNOC_INT_REQRT_TO_ERR_FLAG ||
				int_idx == MNOC_INT_RSPRT_TO_ERR_FLAG))
				mnoc_irq_triggered = 2;
			else
				mnoc_irq_triggered = 1;
		}
	}

	/* additional check: sw triggered irq */
	val = mnoc_read_field(MNOC_REG(MISC_CTRL), 18:16);
	if (val != 0) {
		d->sw_irq_sta.reg_val = val;
		d->sw_irq_sta.timestamp = cur_timestamp;
		LOG_DEBUG("From SW_IRQ = 0x%x\n", val);
		mnoc_write_field(MNOC_REG(MISC_CTRL),
			18:16, 0x0);
		mnoc_irq_triggered = 1;
	}

	LOG_DEBUG("-\n");

	return mnoc_irq_triggered;
}


void mnoc_hw_reinit_v1_50(void)
{
	unsigned long flags;
	int idx;

	LOG_DEBUG("+\n");

	mnoc_qos_reg_init();
	mnoc_reg_init();

	spin_lock_irqsave(&mnoc_spinlock, flags);
	for (idx = 0; idx < NR_APU_QOS_MNI; idx++) {
		if (arr_mni_pre_ultra[idx])
			set_mni_pre_ultra_locked(idx, 1);
		if (arr_mni_lt_guardian_pre_ultra[idx])
			set_lt_guardian_pre_ultra_locked(idx, 1);
	}
	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}



/* read PMU_COUNTER_OUT 0~15 value to pmu buffer */
void mnoc_get_pmu_counter_v1_50(unsigned int *buf)
{
	int i;
	unsigned long flags;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);
	if (mnoc_reg_valid)
		for (i = 0; i < NR_MNOC_PMU_CNTR; i++)
			buf[i] = mnoc_read(PMU_COUNTER0_OUT + 4*i);
	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}

/* project not support clear pmu counter */
void mnoc_clear_pmu_counter_v1_50(unsigned int grp)
{
	LOG_DEBUG("+\n");
	LOG_DEBUG("-\n");
}

bool mnoc_pmu_reg_in_range_v1_50(unsigned int addr)
{
	unsigned int start, end;

	start = APU_NOC_PMU_ADDR;
	end = start + APU_NOC_PMU_RANGE;
	if (addr >= start && addr < end)
		return true;

	return false;
}

void mnoc_hw_v1_50_init(void)
{
	int idx;

	LOG_DEBUG("+\n");

	mnoc_int_dump.count = 0;
	mnoc_int_dump.apusys_int_sta.reg_val = 0;
	mnoc_int_dump.apusys_int_sta.timestamp = 0;
	for (idx = 0; idx < NR_MNI_INT_STA; idx++) {
		mnoc_int_dump.mni_int_sta[idx].reg_val = 0;
		mnoc_int_dump.mni_int_sta[idx].timestamp = 0;
	}
	for (idx = 0; idx < NR_SNI_INT_STA; idx++) {
		mnoc_int_dump.sni_int_sta[idx].reg_val = 0;
		mnoc_int_dump.sni_int_sta[idx].timestamp = 0;
	}
	for (idx = 0; idx < NR_RT_INT_STA; idx++) {
		mnoc_int_dump.rt_int_sta[idx].reg_val = 0;
		mnoc_int_dump.rt_int_sta[idx].timestamp = 0;
	}
	mnoc_int_dump.sw_irq_sta.reg_val = 0;
	mnoc_int_dump.sw_irq_sta.timestamp = 0;

	for (idx = 0; idx < NR_IOMMU; idx++)
		iommu_tfrp[idx] = 0;

	LOG_DEBUG("-\n");
}

void mnoc_hw_v1_50_exit(void)
{
	int i;

	LOG_DEBUG("+\n");

	if (iommu_tfrp_init_flag)
		for (i = 0; i < NR_IOMMU; i++)
			if (protect[i] != NULL)
				kfree(protect[i]);

	LOG_DEBUG("-\n");
}
