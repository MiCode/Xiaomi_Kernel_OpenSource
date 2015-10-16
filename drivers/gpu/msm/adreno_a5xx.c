/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/firmware.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/pm_opp.h>

#include "adreno.h"
#include "a5xx_reg.h"
#include "adreno_a3xx.h"
#include "adreno_a5xx.h"
#include "adreno_cp_parser.h"
#include "adreno_trace.h"
#include "adreno_pm4types.h"
#include "adreno_perfcounter.h"
#include "kgsl_sharedmem.h"
#include "kgsl_log.h"
#include "kgsl.h"

static int zap_ucode_loaded;

void a5xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);

static const struct adreno_vbif_data a530_vbif[] = {
	{A5XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000003},
	{0, 0},
};

static const struct adreno_vbif_platform a5xx_vbif_platforms[] = {
	{ adreno_is_a530, a530_vbif },
	{ adreno_is_a510, a530_vbif },
};

/**
 * Number of times to check if the regulator enabled before
 * giving up and returning failure.
 */
#define PWR_RETRY 100

/**
 * Number of times to check if the GPMU firmware is initialized before
 * giving up and returning failure.
 */
#define GPMU_FW_INIT_RETRY 100

#define GPMU_HEADER_ID		1
#define GPMU_FIRMWARE_ID	2
#define GPMU_SEQUENCE_ID	3
#define GPMU_INST_RAM_SIZE	0xFFF

#define HEADER_MAJOR	1
#define HEADER_MINOR	2
#define HEADER_DATE	3
#define HEADER_TIME	4
#define HEADER_SEQUENCE	5

#define MAX_HEADER_SIZE	10

#define LM_SEQUENCE_ID		1
#define HWCG_SEQUENCE_ID	2
#define MAX_SEQUENCE_ID		3

/* GPMU communication protocal AGC */
#define AGC_INIT_BASE			A5XX_GPMU_DATA_RAM_BASE
#define AGC_RVOUS_MAGIC			(AGC_INIT_BASE + 0)
#define AGC_KMD_GPMU_ADDR		(AGC_INIT_BASE + 1)
#define AGC_KMD_GPMU_BYTES		(AGC_INIT_BASE + 2)
#define AGC_GPMU_KMD_ADDR		(AGC_INIT_BASE + 3)
#define AGC_GPMU_KMD_BYTES		(AGC_INIT_BASE + 4)
#define AGC_INIT_MSG_MAGIC		(AGC_INIT_BASE + 5)
#define AGC_RESERVED			(AGC_INIT_BASE + 6)
#define AGC_MSG_BASE			(AGC_INIT_BASE + 7)

#define AGC_MSG_STATE			(AGC_MSG_BASE + 0)
#define AGC_MSG_COMMAND			(AGC_MSG_BASE + 1)
#define AGC_MSG_RETURN			(AGC_MSG_BASE + 2)
#define AGC_MSG_PAYLOAD_SIZE		(AGC_MSG_BASE + 3)
#define AGC_MSG_MAX_RETURN_SIZE		(AGC_MSG_BASE + 4)
#define AGC_MSG_PAYLOAD			(AGC_MSG_BASE + 5)

#define AGC_INIT_MSG_VALUE	0xBABEFACE

#define AGC_POWER_CONFIG_PRODUCTION_ID	1

/*
 * a5xx_preemption_start() - Setup state to start preemption
 */
static void a5xx_preemption_start(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = &(adreno_dev->dev);
	struct kgsl_iommu *iommu = device->mmu.priv;
	uint64_t ttbr0;

	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		offsetof(struct a5xx_cp_preemption_record, wptr), rb->wptr);
	kgsl_regwrite(device, A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_LO,
		_lo_32(rb->preemption_desc.gpuaddr));
	kgsl_regwrite(device, A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_HI,
		_hi_32(rb->preemption_desc.gpuaddr));
	kgsl_sharedmem_readq(&rb->pagetable_desc, &ttbr0,
		offsetof(struct adreno_ringbuffer_pagetable_info, ttbr0));
	kgsl_sharedmem_writeq(device, &iommu->smmu_info,
		offsetof(struct a5xx_cp_smmu_info, ttbr0), ttbr0);
}

/*
 * a5xx_preemption_save() - Save the state after preemption is done
 */
static void a5xx_preemption_save(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb)
{
	/* save the rptr from ctxrecord here */
	kgsl_sharedmem_readl(&rb->preemption_desc, &rb->rptr,
		offsetof(struct a5xx_cp_preemption_record, rptr));
}

/*
 * a5xx_preemption_init() - Init preemption
 */
static void a5xx_preemption_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_iommu *iommu = device->mmu.priv;
	struct adreno_ringbuffer *rb;
	uint i, ret;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	/* Allocate mem for storing preemption switch record */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		ret = kgsl_allocate_global(&adreno_dev->dev,
			&rb->preemption_desc,
			A5XX_CP_CTXRECORD_SIZE_IN_BYTES, 0,
			KGSL_MEMDESC_PRIVILEGED);
		if (!ret)
			/* Initialize the context switch record here */
			kgsl_sharedmem_writel(rb->device, &rb->preemption_desc,
				offsetof(struct a5xx_cp_preemption_record,
				magic), A5XX_CP_CTXRECORD_MAGIC_REF);
		else {
			adreno_preemption_disable(adreno_dev);
			WARN(1, "gpu preemption: disabled due to low memory");
		}
	}

	/* Allocate mem for storing preemption smmu record */
	ret = kgsl_allocate_global(device, &iommu->smmu_info, PAGE_SIZE,
			   KGSL_MEMDESC_PRIVILEGED, 0);
	if (ret) {
		adreno_preemption_disable(adreno_dev);
		WARN(1, "preemption: disabled due to low memory");
	} else {
		/* Initialize the context switch record here */
		kgsl_sharedmem_writel(device, &iommu->smmu_info,
				offsetof(struct a5xx_cp_smmu_info, magic),
				A5XX_CP_SMMU_INFO_MAGIC_REF);
		kgsl_sharedmem_writeq(device, &iommu->smmu_info,
				offsetof(struct a5xx_cp_smmu_info, ttbr0),
				kgsl_mmu_get_default_ttbr0(&device->mmu,
				KGSL_IOMMU_CONTEXT_USER));
		adreno_writereg64(adreno_dev,
				ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
				ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
				iommu->smmu_info.gpuaddr);
	}
}

/*
 * a5xx_preemption_token() - Preempt token on a5xx
 * PM4 commands for preempt token on a5xx. These commands are
 * submitted to ringbuffer to trigger preemption.
 */
static int a5xx_preemption_token(struct adreno_device *adreno_dev,
			struct adreno_ringbuffer *rb, unsigned int *cmds,
			uint64_t gpuaddr)
{
	unsigned int *cmds_orig = cmds;

	/* Enable yield in RB only */
	*cmds++ = cp_type7_packet(CP_YIELD_ENABLE, 1);
	*cmds++ = 1;

	*cmds++ = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	cmds += cp_gpuaddr(adreno_dev, cmds, gpuaddr);
	*cmds++ = 1;
	/* generate interrupt on preemption completion */
	*cmds++ = 1;

	return cmds - cmds_orig;

}

/*
 * a5xx_preemption_pre_ibsubmit() - Below PM4 commands are
 * added at the beginning of every cmdbatch submission.
 */
static int a5xx_preemption_pre_ibsubmit(
			struct adreno_device *adreno_dev,
			struct adreno_ringbuffer *rb, unsigned int *cmds,
			struct kgsl_context *context, uint64_t cond_addr,
			struct kgsl_memobj_node *ib)
{
	unsigned int *cmds_orig = cmds;
	uint64_t gpuaddr = rb->preemption_desc.gpuaddr;

	/*
	 * CP_PREEMPT_ENABLE_GLOBAL(global preemption) can only be set by KMD
	 * in ringbuffer.
	 * 1) set global preemption to 0x0 to disable global preemption.
	 *    Only RB level preemption is allowed in this mode
	 * 2) Set global preemption to defer(0x2) for finegrain preemption.
	 *    when global preemption is set to defer(0x2),
	 *    CP_PREEMPT_ENABLE_LOCAL(local preemption) determines the
	 *    preemption point. Local preemption
	 *    can be enabled by both UMD(within IB) and KMD.
	 */
	*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_GLOBAL, 1);
	*cmds++ = (ADRENO_PREEMPT_STYLE(context->flags)
			   == KGSL_CONTEXT_PREEMPT_STYLE_FINEGRAIN) ? 2 : 0;

	/* Turn CP protection OFF */
	*cmds++ = cp_type7_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

	/*
	 * CP during context switch will save context switch info to
	 * a5xx_cp_preemption_record pointed by CONTEXT_SWITCH_SAVE_ADDR
	 */
	*cmds++ = cp_type4_packet(A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_LO, 1);
	*cmds++ = _lo_32(gpuaddr);
	*cmds++ = cp_type4_packet(A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_HI, 1);
	*cmds++ = _hi_32(gpuaddr);

	/* Turn CP protection ON */
	*cmds++ = cp_type7_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 1;

	/*
	 * Enable local preemption for finegrain preemption in case of
	 * a misbehaving IB
	 */
	if (ADRENO_PREEMPT_STYLE(context->flags) ==
				KGSL_CONTEXT_PREEMPT_STYLE_FINEGRAIN) {
		*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_LOCAL, 1);
		*cmds++ = 1;
	}

	return cmds - cmds_orig;
}

/*
 * a5xx_preemption_post_ibsubmit() - Below PM4 commands are
 * added after every cmdbatch submission.
 */
static int a5xx_preemption_post_ibsubmit(
			struct adreno_device *adreno_dev,
			struct adreno_ringbuffer *rb, unsigned int *cmds,
			struct kgsl_context *context)
{
	unsigned int *cmds_orig = cmds;

	/*
	 * SRM -- set render mode (ex binning, direct render etc)
	 * SRM is set by UMD usually at start of IB to tell CP the type of
	 * preemption.
	 * KMD needs to set SRM to NULL to indicate CP that rendering is
	 * done by IB.
	 */
	*cmds++ = cp_type7_packet(CP_SET_RENDER_MODE, 5);
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;

	cmds += a5xx_preemption_token(adreno_dev, rb, cmds,
				rb->device->memstore.gpuaddr +
				KGSL_MEMSTORE_OFFSET(context->id, preempted));

	return cmds - cmds_orig;
}

/*
 * a5xx_gpudev_init() - Initialize gpudev specific fields
 * @adreno_dev: Pointer to adreno device
 */
static void a5xx_gpudev_init(struct adreno_device *adreno_dev)
{
	uint64_t addr;
	struct adreno_gpudev *gpudev;

	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_is_a510(adreno_dev)) {
		gpudev->snapshot_data->sect_sizes->cp_meq = 32;
		gpudev->snapshot_data->sect_sizes->cp_merciu = 32;
		gpudev->snapshot_data->sect_sizes->roq = 256;

		/* A510 has 3 XIN ports in VBIF */
		gpudev->vbif_xin_halt_ctrl0_mask =
				A510_VBIF_XIN_HALT_CTRL0_MASK;
	}

	/* Calculate SP local and private mem addresses */
	addr = ALIGN(ADRENO_UCHE_GMEM_BASE + adreno_dev->gmem_size, SZ_64K);
	adreno_dev->sp_local_gpuaddr = addr;
	adreno_dev->sp_pvt_gpuaddr = addr + SZ_64K;
}

/**
 * a5xx_protect_init() - Initializes register protection on a5xx
 * @device: Pointer to the device structure
 * Performs register writes to enable protected access to sensitive
 * registers
 */
static void a5xx_protect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	int index = 0;
	struct kgsl_protected_registers *iommu_regs;

	/* enable access protection to privileged registers */
	kgsl_regwrite(device, A5XX_CP_PROTECT_CNTL, 0x00000007);

	/* RBBM registers */
	adreno_set_protected_registers(adreno_dev, &index, 0x4, 2);
	adreno_set_protected_registers(adreno_dev, &index, 0x8, 3);
	adreno_set_protected_registers(adreno_dev, &index, 0x10, 4);
	adreno_set_protected_registers(adreno_dev, &index, 0x20, 5);
	adreno_set_protected_registers(adreno_dev, &index, 0x40, 6);
	adreno_set_protected_registers(adreno_dev, &index, 0x80, 6);

	/* Content protection registers */
	adreno_set_protected_registers(adreno_dev, &index,
		   A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO, 4);
	adreno_set_protected_registers(adreno_dev, &index,
		   A5XX_RBBM_SECVID_TRUST_CNTL, 1);

	/* CP registers */
	adreno_set_protected_registers(adreno_dev, &index, 0x800, 6);
	adreno_set_protected_registers(adreno_dev, &index, 0x840, 3);
	adreno_set_protected_registers(adreno_dev, &index, 0x880, 5);
	adreno_set_protected_registers(adreno_dev, &index, 0x0AA0, 0);

	/* RB registers */
	adreno_set_protected_registers(adreno_dev, &index, 0xCC0, 0);
	adreno_set_protected_registers(adreno_dev, &index, 0xCF0, 1);

	/* VPC registers */
	adreno_set_protected_registers(adreno_dev, &index, 0xE68, 3);
	adreno_set_protected_registers(adreno_dev, &index, 0xE70, 4);

	/* UCHE registers */
	adreno_set_protected_registers(adreno_dev, &index, 0xE87, 4);

	/* SMMU registers */
	iommu_regs = kgsl_mmu_get_prot_regs(&device->mmu);
	if (iommu_regs)
		adreno_set_protected_registers(adreno_dev, &index,
				iommu_regs->base, iommu_regs->range);
}

/*
 * a5xx_is_sptp_idle() - A530 SP/TP/RAC should be power collapsed to be
 * considered idle
 * @adreno_dev: The adreno_device pointer
 */
static bool a5xx_is_sptp_idle(struct adreno_device *adreno_dev)
{
	unsigned int reg;
	struct kgsl_device *device = &adreno_dev->dev;

	/* If feature is not supported or enabled, no worry */
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC) ||
		!test_bit(ADRENO_SPTP_PC_CTRL, &adreno_dev->pwrctrl_flag))
		return true;
	kgsl_regread(device, A5XX_GPMU_SP_PWR_CLK_STATUS, &reg);
	if (reg & BIT(20))
		return false;
	kgsl_regread(device, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, &reg);
	return !(reg & BIT(20));
}

/*
 * _poll_gdsc_status() - Poll the GDSC status register
 * @adreno_dev: The adreno device pointer
 * @status_reg: Offset of the status register
 * @status_value: The expected bit value
 *
 * Poll the status register till the power-on bit is equal to the
 * expected value or the max retries are exceeded.
 */
static int _poll_gdsc_status(struct adreno_device *adreno_dev,
				unsigned int status_reg,
				unsigned int status_value)
{
	unsigned int reg, retry = PWR_RETRY;
	struct kgsl_device *device = &adreno_dev->dev;

	/* Bit 20 is the power on bit of SPTP and RAC GDSC status register */
	do {
		udelay(1);
		kgsl_regread(device, status_reg, &reg);
	} while (((reg & BIT(20)) != (status_value << 20)) && retry--);
	if ((reg & BIT(20)) != (status_value << 20))
		return -ETIMEDOUT;
	return 0;
}

/*
 * a5xx_regulator_enable() - Enable any necessary HW regulators
 * @adreno_dev: The adreno device pointer
 *
 * Some HW blocks may need their regulators explicitly enabled
 * on a restart.  Clocks must be on during this call.
 */
static int a5xx_regulator_enable(struct adreno_device *adreno_dev)
{
	unsigned int ret;
	struct kgsl_device *device = &adreno_dev->dev;
	if (!adreno_is_a530(adreno_dev))
		return 0;

	/*
	 * Turn on smaller power domain first to reduce voltage droop.
	 * Set the default register values; set SW_COLLAPSE to 0.
	 */
	kgsl_regwrite(device, A5XX_GPMU_RBCCU_POWER_CNTL, 0x778000);
	/* Insert a delay between RAC and SPTP GDSC to reduce voltage droop */
	udelay(3);
	ret = _poll_gdsc_status(adreno_dev, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, 1);
	if (ret) {
		KGSL_PWR_ERR(device, "RBCCU GDSC enable failed\n");
		return ret;
	}

	kgsl_regwrite(device, A5XX_GPMU_SP_POWER_CNTL, 0x778000);
	ret = _poll_gdsc_status(adreno_dev, A5XX_GPMU_SP_PWR_CLK_STATUS, 1);
	if (ret) {
		KGSL_PWR_ERR(device, "SPTP GDSC enable failed\n");
		return ret;
	}

	return 0;
}

/*
 * a5xx_regulator_disable() - Disable any necessary HW regulators
 * @adreno_dev: The adreno device pointer
 *
 * Some HW blocks may need their regulators explicitly disabled
 * on a power down to prevent current spikes.  Clocks must be on
 * during this call.
 */
static void a5xx_regulator_disable(struct adreno_device *adreno_dev)
{
	unsigned int reg;
	struct kgsl_device *device = &adreno_dev->dev;

	if (adreno_is_a510(adreno_dev))
		return;

	/* If feature is not supported or not enabled */
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC) ||
		!test_bit(ADRENO_SPTP_PC_CTRL, &adreno_dev->pwrctrl_flag)) {
		/* Set the default register values; set SW_COLLAPSE to 1 */
		kgsl_regwrite(device, A5XX_GPMU_SP_POWER_CNTL, 0x778001);
		/*
		 * Insert a delay between SPTP and RAC GDSC to reduce voltage
		 * droop.
		 */
		udelay(3);
		if (_poll_gdsc_status(adreno_dev,
					A5XX_GPMU_SP_PWR_CLK_STATUS, 0))
			KGSL_PWR_WARN(device, "SPTP GDSC disable failed\n");

		kgsl_regwrite(device, A5XX_GPMU_RBCCU_POWER_CNTL, 0x778001);
		if (_poll_gdsc_status(adreno_dev,
					A5XX_GPMU_RBCCU_PWR_CLK_STATUS, 0))
			KGSL_PWR_WARN(device, "RBCCU GDSC disable failed\n");
	} else if (test_bit(ADRENO_DEVICE_GPMU_INITIALIZED,
			&adreno_dev->priv)) {
		/* GPMU firmware is supposed to turn off SPTP & RAC GDSCs. */
		kgsl_regread(device, A5XX_GPMU_SP_PWR_CLK_STATUS, &reg);
		if (reg & BIT(20))
			KGSL_PWR_WARN(device, "SPTP GDSC is not disabled\n");
		kgsl_regread(device, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, &reg);
		if (reg & BIT(20))
			KGSL_PWR_WARN(device, "RBCCU GDSC is not disabled\n");
		/*
		 * GPMU firmware is supposed to set GMEM to non-retention.
		 * Bit 14 is the memory core force on bit.
		 */
		kgsl_regread(device, A5XX_GPMU_RBCCU_CLOCK_CNTL, &reg);
		if (reg & BIT(14))
			KGSL_PWR_WARN(device, "GMEM is forced on\n");
	}

	if (adreno_is_a530(adreno_dev)) {
		/* Reset VBIF before PC to avoid popping bogus FIFO entries */
		kgsl_regwrite(device, A5XX_RBBM_BLOCK_SW_RESET_CMD,
			0x003C0000);
		kgsl_regwrite(device, A5XX_RBBM_BLOCK_SW_RESET_CMD, 0);
	}
}

struct kgsl_hwcg_reg {
	unsigned int off;
	unsigned int val;
};

static const struct kgsl_hwcg_reg a510_hwcg_regs[] = {
	{A5XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP1, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP1, 0x02222220},
	{A5XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP1, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP1, 0x00000080},
	{A5XX_RBBM_CLOCK_CNTL_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_TP0, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP1, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST3_TP0, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP1, 0x00007777},
	{A5XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY3_TP0, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP1, 0x00001111},
	{A5XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{A5XX_RBBM_CLOCK_HYST_UCHE, 0x00444444},
	{A5XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A5XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB0, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB1, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL_CCU0, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU1, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_RAC, 0x05522222},
	{A5XX_RBBM_CLOCK_CNTL2_RAC, 0x00555555},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU0, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU1, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RAC, 0x07444044},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_0, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_1, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RAC, 0x00010011},
	{A5XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A5XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
	{A5XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A5XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A5XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A5XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A5XX_RBBM_CLOCK_DELAY_VFD, 0x00002222}
};

static const struct kgsl_hwcg_reg a530_hwcg_regs[] = {
	{A5XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP1, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP2, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP3, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP1, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP2, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP3, 0x02222220},
	{A5XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP1, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP2, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP3, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP1, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP2, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP3, 0x00000080},
	{A5XX_RBBM_CLOCK_CNTL_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_TP0, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP1, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP2, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP3, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP2, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP3, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP2, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP3, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST3_TP0, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP1, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP2, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP3, 0x00007777},
	{A5XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP2, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP3, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP2, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP3, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY3_TP0, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP1, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP2, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP3, 0x00001111},
	{A5XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{A5XX_RBBM_CLOCK_HYST_UCHE, 0x00444444},
	{A5XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A5XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB0, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB1, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB2, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB3, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL_CCU0, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU1, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU2, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU3, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_RAC, 0x05522222},
	{A5XX_RBBM_CLOCK_CNTL2_RAC, 0x00555555},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU0, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU1, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU2, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU3, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RAC, 0x07444044},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_0, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_1, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_2, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_3, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RAC, 0x00010011},
	{A5XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A5XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
	{A5XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A5XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A5XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A5XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A5XX_RBBM_CLOCK_DELAY_VFD, 0x00002222}
};

static const struct {
	int (*devfunc)(struct adreno_device *adreno_dev);
	const struct kgsl_hwcg_reg *regs;
	unsigned int count;
} a5xx_hwcg_registers[] = {
	{ adreno_is_a530v2, a530_hwcg_regs, ARRAY_SIZE(a530_hwcg_regs) },
	{ adreno_is_a510, a510_hwcg_regs, ARRAY_SIZE(a510_hwcg_regs) },
};

static void a5xx_hwcg_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	const struct kgsl_hwcg_reg *regs;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(a5xx_hwcg_registers); i++) {
		if (a5xx_hwcg_registers[i].devfunc(adreno_dev))
			break;
	}

	if (i == ARRAY_SIZE(a5xx_hwcg_registers))
		return;

	regs = a5xx_hwcg_registers[i].regs;

	for (j = 0; j < a5xx_hwcg_registers[i].count; j++)
		kgsl_regwrite(device, regs[j].off, regs[j].val);

	/* enable top level HWCG */
	kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL, 0xAAA8AA00);
	kgsl_regwrite(device, A5XX_RBBM_ISDB_CNT, 0x00000182);
}

/*
 * a5xx_enable_pc() - Enable the GPMU based power collapse of the SPTP and RAC
 * blocks
 * @adreno_dev: The adreno device pointer
 */
static void a5xx_enable_pc(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC) ||
		!test_bit(ADRENO_SPTP_PC_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	kgsl_regwrite(device, A5XX_GPMU_PWR_COL_INTER_FRAME_CTRL, 0x0000007F);
	kgsl_regwrite(device, A5XX_GPMU_PWR_COL_BINNING_CTRL, 0);
	kgsl_regwrite(device, A5XX_GPMU_PWR_COL_INTER_FRAME_HYST, 0x000A0080);
	kgsl_regwrite(device, A5XX_GPMU_PWR_COL_STAGGER_DELAY, 0x00600040);

	trace_adreno_sp_tp((unsigned long) __builtin_return_address(0));
};

/*
 * a5xx_gpmu_ucode_load() - Load the ucode into the GPMU RAM
 * @adreno_dev: Pointer to adreno device

 * GPMU firmare format (one dword per field):
 *	Header block length (length dword field not inclusive)
 *	Header block ID
 *	Header Field 1 tag
 *	Header Field 1 data
 *	Header Field 2 tag
 *	Header Field 2 data (two tag/data pairs minimum for major and minor)
 *	. . .
 *	Header Field M tag
 *	Header Field M data
 *	Microcode block length (length dword field not inclusive)
 *	Microcode block ID
 *	Microcode data Dword 1
 *	Microcode data Dword 2
 *	. . .
 *	Microcode data Dword N
 */
static int a5xx_gpmu_ucode_load(struct adreno_device *adreno_dev)
{
	static uint32_t *gpmu_fw_buffer;
	uint32_t *header, *gpmu_fw;
	unsigned int i;
	const struct firmware *fw = NULL;
	struct kgsl_device *device = &adreno_dev->dev;
	const struct adreno_gpu_core *gpucore = adreno_dev->gpucore;
	unsigned int gpmu_major_offset = 0, gpmu_minor_offset = 0;
	unsigned int gpmu_major = 0, gpmu_minor = 0;
	/* Dword size if not specified */
	uint32_t header_size, fw_size, size;
	int ret = -EIO;
	int result;
	uint32_t *cmds = NULL;
	uint32_t offset;
	uint32_t payload_size, payload_size_max = PM4_TYPE4_PKT_SIZE_MAX - 1;
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffers[0];

	if (gpucore->gpmufw_name == NULL)
		return -EINVAL;
	result = request_firmware(&fw, gpucore->gpmufw_name, device->dev);
	if (result) {
		KGSL_CORE_ERR("request_firmware (%s) failed: %d\n",
				gpucore->gpmufw_name, result);
		return result;
	}

	/* Read header block and verify versioning first */
	header = (uint32_t *)fw->data;
	fw_size = fw->size / sizeof(uint32_t);
	/*
	 * The minimum firmware binary size is eight dwords, one for each of
	 * the following fields: GPMU header block length, header block ID,
	 * major tag, major vlaue, minor tag, minor value, ucode block length,
	 * and ucode block ID.
	 */
	if (fw_size < 8) {
		KGSL_CORE_ERR("GPMU firmware size is less than 8 dwords, actual size %d\n",
			fw_size);
		goto err;
	}
	header_size = header[0];
	/*
	 * The minimum firmware header size is five dwords, one for each of
	 * the following fields: GPMU header block ID, major tag, major value,
	 * minor tag, minor value.
	 */
	if (header_size < 5) {
		KGSL_CORE_ERR("GPMU firmware header size is less than 5 dwords, actual size %d\n",
			header_size);
		goto err;
	}
	if (header_size >= fw_size) {
		KGSL_CORE_ERR("GPMU firmware header size (%d) is larger than firmware size (%d)\n",
				header_size, fw_size);
		goto err;
	}
	if (header[1] != GPMU_HEADER_ID) {
		KGSL_CORE_ERR("GPMU firmware header ID mis-match: expected 0x%X, actual 0x%X\n",
				GPMU_HEADER_ID, header[1]);
		goto err;
	}
	if ((header_size - 1) % 2 != 0) {
		KGSL_CORE_ERR("GPMU firmware header contains incompete (tag, value) pair\n");
		goto err;
	}
	for (i = 2; i < header_size; i += 2) {
		switch (header[i]) {
		case HEADER_MAJOR:
			gpmu_major_offset = i;
			gpmu_major = header[i + 1];
			break;
		case HEADER_MINOR:
			gpmu_minor_offset = i;
			gpmu_minor = header[i + 1];
			break;
		case HEADER_DATE:
		case HEADER_TIME:
			break;
		default:
			KGSL_CORE_ERR("GPMU unknown header ID %d\n",
					header[i]);
			goto err;
		}
	}
	if (gpmu_major_offset == 0) {
		KGSL_CORE_ERR("GPMU major version number is missing\n");
		goto err;
	}
	if (gpmu_minor_offset == 0) {
		KGSL_CORE_ERR("GPMU minor version number is missing\n");
		goto err;
	}
	/*
	 * A value of 0 for both the Major and Minor version fields indicates
	 * the code is un-versioned, and should be allowed to pass all
	 * versioning checks.
	 */
	if ((gpmu_major != 0) || (gpmu_minor != 0)) {
		if (gpucore->gpmu_major != gpmu_major) {
			KGSL_CORE_ERR(
				"GPMU major version mis-match: expected %d, actual %d\n",
				gpucore->gpmu_major, gpmu_major);
			goto err;
		}
		/*
		 * Driver should be compatible for firmware with
		 * smaller minor version number.
		 */
		if (gpucore->gpmu_minor < gpmu_minor) {
			KGSL_CORE_ERR(
				"GPMU minor version mis-match: expected %d, actual %d\n",
				gpucore->gpmu_minor, gpmu_minor);
			goto err;
		}
	}

	/*
	 * Read in the firmware now, 1 dword for header block length field and
	 * header_size for the remaining.
	 */
	gpmu_fw = header + header_size + 1;
	size = *gpmu_fw++;
	if (size == 0 || size > GPMU_INST_RAM_SIZE) {
		KGSL_CORE_ERR("GPMU firmware block size is zero or larger than RAM size\n");
		goto err;
	}
	/*
	 * Deduct one (block length field) from the firmware block size to
	 * get the microcode size.
	 */
	size -= 1;
	/*
	 * The actual microcode size is fw_size - header_size - 2, with
	 * the two block length dword fields deducted.
	 */
	if (size > fw_size - header_size - 2) {
		KGSL_CORE_ERR("GPMU firmware microcode size (%d) larger than actual (%d)\n",
				size, fw_size - header_size - 2);
		goto err;
	}
	if (*gpmu_fw++ != GPMU_FIRMWARE_ID) {
		KGSL_CORE_ERR("GPMU firmware id not mis-match: expected 0x%X, actual 0x%X\n",
			GPMU_FIRMWARE_ID,  header[header_size + 1 + 1]);
		goto err;
	}

	if (gpmu_fw_buffer == NULL) {
		gpmu_fw_buffer = kmalloc((PM4_TYPE4_PKT_SIZE_MAX << 2),
					GFP_KERNEL);
		if (gpmu_fw_buffer == NULL) {
			KGSL_CORE_ERR("Memory allocation failed.\n");
			ret = -ENOMEM;
			goto err;
		}
	}

	offset = 0;
	while (size > 0) {
		cmds = gpmu_fw_buffer;
		if (size >= payload_size_max) {
			*cmds++ = cp_type4_packet(
					A5XX_GPMU_INST_RAM_BASE + offset,
					payload_size_max);
			memcpy(cmds, gpmu_fw, payload_size_max << 2);
			gpmu_fw += payload_size_max;
			offset += payload_size_max;
			payload_size = payload_size_max;
		} else {
			*cmds++ = cp_type4_packet(
					A5XX_GPMU_INST_RAM_BASE + offset,
					size);
			memcpy(cmds, gpmu_fw, size << 2);
			payload_size = size;
		}
		size -= payload_size;

		ret = adreno_ringbuffer_issuecmds(rb, 0, gpmu_fw_buffer,
						payload_size + 1);
		if (ret) {
			KGSL_CORE_ERR("GPMU firmware loading failed (%d)\n",
					ret);
			goto err;
		}
	}

err:
	release_firmware(fw);
	return ret;
}

/*
 * a5xx_gpmu_start() - Initialize and start the GPMU
 * @adreno_dev: Pointer to adreno device
 *
 * Load the GPMU microcode, set up any features such as hardware clock gating
 * or IFPC, and take the GPMU out of reset.
 */
static void a5xx_gpmu_start(struct adreno_device *adreno_dev)
{
	int ret;
	unsigned int reg, retry = GPMU_FW_INIT_RETRY;
	struct kgsl_device *device = &adreno_dev->dev;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_GPMU))
		return;

	ret = a5xx_gpmu_ucode_load(adreno_dev);
	if (ret)
		return;

	/* GPMU clock gating setup */
	kgsl_regwrite(device, A5XX_GPMU_WFI_CONFIG, 0x00004014);

	/* Kick off GPMU firmware */
	kgsl_regwrite(device, A5XX_GPMU_CM3_SYSRESET, 0);
	/*
	 * The hardware team's estimation of GPMU firmware initialization
	 * latency is about 3000 cycles, that's about 5 to 24 usec.
	 */
	do {
		udelay(1);
		kgsl_regread(device, A5XX_GPMU_GENERAL_0, &reg);
	} while ((reg != 0xBABEFACE) && retry--);
	if (reg != 0xBABEFACE)
		KGSL_CORE_ERR("GPMU firmware initialization timed out\n");
	else
		set_bit(ADRENO_DEVICE_GPMU_INITIALIZED, &adreno_dev->priv);
}

static int _read_fw2_block_header(uint32_t *header, uint32_t id,
				uint32_t major, uint32_t minor)
{
	uint32_t header_size;
	int i = 1;

	if (header == NULL)
		return -ENOMEM;

	header_size = header[0];
	/* Headers have limited size and always occur as pairs of words */
	if (header_size > MAX_HEADER_SIZE || header_size % 2)
		return -EINVAL;
	/* Sequences must have an identifying id first thing in their header */
	if (id == GPMU_SEQUENCE_ID) {
		if (header[i] != HEADER_SEQUENCE ||
			(header[i + 1] >= MAX_SEQUENCE_ID))
			return -EINVAL;
		i += 2;
	}
	for (; i < header_size; i += 2) {
		switch (header[i]) {
		/* Major Version */
		case HEADER_MAJOR:
			if ((major > header[i + 1]) &&
				header[i + 1]) {
				KGSL_CORE_ERR(
					"GPMU major version mis-match %d, %d\n",
					major, header[i + 1]);
				return -EINVAL;
			}
			break;
		case HEADER_MINOR:
			if (minor > header[i + 1])
				KGSL_CORE_ERR(
					"GPMU minor version mis-match %d %d\n",
					minor, header[i + 1]);
			break;
		case HEADER_DATE:
		case HEADER_TIME:
			break;
		default:
			KGSL_CORE_ERR("GPMU unknown header ID %d\n",
					header[i]);
		}
	}
	return 0;
}

/*
 * Read in the register sequence file and save pointers to the
 * necessary sequences.
 *
 * GPU sequence file format (one dword per field unless noted):
 * Block 1 length (length dword field not inclusive)
 * Block 1 type = Sequence = 3
 * Block Header length (length dword field not inclusive)
 * BH field ID = Sequence field ID
 * BH field data = Sequence ID
 * BH field ID
 * BH field data
 * ...
 * Opcode 0 ID
 * Opcode 0 data M words
 * Opcode 1 ID
 * Opcode 1 data N words
 * ...
 * Opcode X ID
 * Opcode X data O words
 * Block 2 length...
 */
static void _load_regfile(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	const struct firmware *fw;
	uint32_t block_size = 0, block_total = 0, fw_size;
	uint32_t *block;
	int ret = -EINVAL;

	ret = request_firmware(&fw, adreno_dev->gpucore->regfw_name,
			device->dev);
	if (ret) {
		KGSL_PWR_ERR(device, "request firmware failed %d, %s\n",
				ret, adreno_dev->gpucore->regfw_name);
		return;
	}

	fw_size = fw->size / sizeof(uint32_t);
	/* Min valid file of size 6, see file description */
	if (fw_size < 6)
		goto err;
	block = (uint32_t *)fw->data;
	/* All offset numbers calculated from file description */
	while (block_total < fw_size) {
		block_size = block[0];
		if (block_size >= fw_size || block_size < 2)
			goto err;
		if (block[1] != GPMU_SEQUENCE_ID)
			goto err;

		/* For now ignore blocks other than the LM sequence */
		if (block[4] == LM_SEQUENCE_ID) {
			ret = _read_fw2_block_header(&block[2],
				GPMU_SEQUENCE_ID,
				adreno_dev->gpucore->lm_major,
				adreno_dev->gpucore->lm_minor);
			if (ret)
				goto err;

			adreno_dev->lm_fw = fw;
			adreno_dev->lm_sequence = block + block[2] + 3;
			adreno_dev->lm_size = block_size - block[2] - 2;
		}
		block_total += (block_size + 1);
		block += (block_size + 1);
	}
	if (adreno_dev->lm_sequence)
		return;

err:
	release_firmware(fw);
	KGSL_PWR_ERR(device,
		"Register file failed to load sz=%d bsz=%d header=%d\n",
		fw_size, block_size, ret);
	return;
}

static int _execute_reg_sequence(struct adreno_device *adreno_dev,
			uint32_t *opcode, uint32_t length)
{
	struct kgsl_device *device = &adreno_dev->dev;
	uint32_t *cur = opcode;
	uint64_t reg, val;

	/* todo double check the reg writes */
	while ((cur - opcode) < length) {
		switch (cur[0]) {
		/* Write a 32 bit value to a 64 bit reg */
		case 1:
			reg = cur[2];
			reg = (reg << 32) | cur[1];
			kgsl_regwrite(device, reg, cur[3]);
			cur += 4;
			break;
		/* Write a 64 bit value to a 64 bit reg */
		case 2:
			reg = cur[2];
			reg = (reg << 32) | cur[1];
			val = cur[4];
			val = (val << 32) | cur[3];
			kgsl_regwrite(device, reg, val);
			cur += 5;
			break;
		/* Delay for X usec */
		case 3:
			udelay(cur[1]);
			cur += 2;
			break;
		default:
			return -EINVAL;
	} }
	return 0;
}

static void _write_voltage_table(struct adreno_device *adreno_dev,
			unsigned int addr, uint32_t *length)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;
	struct opp *opp;
	int levels = pwr->num_pwrlevels - 1;
	unsigned int mvolt = 0;

	kgsl_regwrite(device, addr, adreno_dev->gpucore->max_power);
	kgsl_regwrite(device, addr + 1, levels);

	/* Write voltage in mV and frequency in MHz */
	for (i = 0; i < levels; i++) {
		opp = dev_pm_opp_find_freq_exact(&device->pdev->dev,
				pwr->pwrlevels[i].gpu_freq, true);
		/* _opp_get returns uV, convert to mV */
		if (!IS_ERR(opp))
			mvolt = dev_pm_opp_get_voltage(opp) / 1000;
		kgsl_regwrite(device, addr + 2 + i * 2, mvolt);
		kgsl_regwrite(device, addr + 3 + i * 2,
				pwr->pwrlevels[i].gpu_freq / 1000000);
	}
	*length = levels * 2 + 2;
}

/*
 * a5xx_lm_init() - Initialize LM/DPM on the GPMU
 * @adreno_dev: The adreno device pointer
 */
static void a5xx_lm_init(struct adreno_device *adreno_dev)
{
	uint32_t length;
	struct kgsl_device *device = &adreno_dev->dev;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
		!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	/* If something was wrong with the sequence file, return */
	if (adreno_dev->lm_sequence == NULL)
		return;

	/* Write LM registers including DPM ucode, coefficients, and config */
	if (_execute_reg_sequence(adreno_dev, adreno_dev->lm_sequence,
				adreno_dev->lm_size)) {
		/* If the sequence is invalid, it's not getting better */
		adreno_dev->lm_sequence = NULL;
		KGSL_PWR_WARN(device,
				"Invalid LM sequence\n");
		return;
	}

	kgsl_regwrite(device, A5XX_GPMU_TEMP_SENSOR_ID,
			adreno_dev->gpucore->gpmu_tsens);
	kgsl_regwrite(device, A5XX_GPMU_DELTA_TEMP_THRESHOLD, 0x1);
	kgsl_regwrite(device, A5XX_GPMU_TEMP_SENSOR_CONFIG, 0x1);

	kgsl_regwrite(device, A5XX_GPMU_GPMU_VOLTAGE,
			(0x80000000 | device->pwrctrl.default_pwrlevel));
	/* todo use the iddq fuse to correct this value at runtime */
	kgsl_regwrite(device, A5XX_GPMU_BASE_LEAKAGE, 0x00640002);
	/* default of 6A */
	kgsl_regwrite(device, A5XX_GPMU_GPMU_PWR_THRESHOLD, 0x80001000);

	kgsl_regwrite(device, A5XX_GPMU_BEC_ENABLE, 0x10001FFF);
	kgsl_regwrite(device, A5XX_GDPM_CONFIG1, 0x00201FF1);

	/* Send an initial message to the GPMU with the LM voltage table */
	kgsl_regwrite(device, AGC_MSG_STATE, 0x1);
	kgsl_regwrite(device, AGC_MSG_COMMAND, AGC_POWER_CONFIG_PRODUCTION_ID);
	_write_voltage_table(adreno_dev, AGC_MSG_PAYLOAD, &length);
	length *= sizeof(uint32_t);
	kgsl_regwrite(device, AGC_MSG_PAYLOAD_SIZE, length);
	kgsl_regwrite(device, AGC_INIT_MSG_MAGIC, AGC_INIT_MSG_VALUE);
}

/*
 * a5xx_lm_enable() - Enable the LM/DPM feature on the GPMU
 * @adreno_dev: The adreno device pointer
 */
static void a5xx_lm_enable(struct adreno_device *adreno_dev)
{
	uint32_t val;
	struct kgsl_device *device = &adreno_dev->dev;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
		!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	/* If no sequence properly initialized, return */
	if (adreno_dev->lm_sequence == NULL)
		return;

	kgsl_regwrite(device, A5XX_GDPM_INT_MASK, 0x00000000);
	kgsl_regwrite(device, A5XX_GDPM_INT_EN, 0x0000000A);
	kgsl_regwrite(device, A5XX_GPMU_GPMU_VOLTAGE_INTR_EN_MASK, 0x00000001);
	kgsl_regwrite(device, A5XX_GPMU_TEMP_THRESHOLD_INTR_EN_MASK,
			0x00050000);
	kgsl_regwrite(device, A5XX_GPMU_THROTTLE_UNMASK_FORCE_CTRL,
			0x00030000);
	if (adreno_is_a530(adreno_dev) && !adreno_is_a530v1(adreno_dev))
		val = 0x00060011;
	/* v3 value */
	else
		val = 0x00000011;
	kgsl_regwrite(device, A5XX_GPMU_CLOCK_THROTTLE_CTRL, val);
}

/*
 * a5xx_pwrlevel_change_settings() - Program the hardware during power level
 * transitions
 * @adreno_dev: The adreno device pointer
 * @prelevel: The previous power level
 * @postlevel: The new power level
 * @post: True if called after the clock change has taken effect
 */
static void a5xx_pwrlevel_change_settings(struct adreno_device *adreno_dev,
				unsigned int prelevel, unsigned int postlevel,
				bool post)
{
	struct kgsl_device *device = &adreno_dev->dev;
	static int pre;
	int on = 0;

	/* Only call through if PPD or LM is supported and enabled */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_PPD) &&
		test_bit(ADRENO_PPD_CTRL, &adreno_dev->pwrctrl_flag))
		on = ADRENO_PPD;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM) &&
		test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		on = ADRENO_LM;

	if (!on)
		return;

	/* if this is a real pre, or a post without a previous pre, set pre */
	if ((post == 0) || (pre == 0 && post == 1))
		pre = 1;
	else if (post == 1)
		pre = 0;

	if (pre)
		kgsl_regwrite(device, A5XX_GPMU_GPMU_VOLTAGE,
			(0x80000010 | postlevel));

	if (pre && post)
		udelay(3);

	if (post) {
		kgsl_regwrite(device, A5XX_GPMU_GPMU_VOLTAGE,
			(0x80000000 | postlevel));
		pre = 0;
	}
}

/*
 * a5xx_start() - Device start
 * @adreno_dev: Pointer to adreno device
 *
 * a5xx device start
 */
static void a5xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	uint val = 0;

	adreno_vbif_start(adreno_dev, a5xx_vbif_platforms,
			ARRAY_SIZE(a5xx_vbif_platforms));

	/* GPU comes up in secured mode, make it unsecured by default */
	kgsl_regwrite(device, A5XX_RBBM_SECVID_TRUST_CNTL, 0x0);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, A5XX_RBBM_PERFCTR_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/*
	 * Enable the RBBM error reporting bits.  This lets us get
	 * useful information on failure
	 */
	kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL0, 0x00000001);

	/*
	 * Turn on hang detection for a530 v2 and beyond. This spews a
	 * lot of useful information into the RBBM registers on a hang.
	 */
	if (!adreno_is_a530v1(adreno_dev)) {
		/*
		 * We have 4 RB units, and only RB0 activity signals are working
		 * correctly. Mask out RB1-3 activity signals from the HW hang
		 * detection logic as per recommendation of hardware team.
		 */
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL11,
					0xF0000000);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL12,
					0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL13,
					0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL14,
					0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL15,
					0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL16,
					0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL17,
					0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL18,
					0xFFFFFFFF);

		set_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv);
		gpudev->irq->mask |= (1 << A5XX_INT_MISC_HANG_DETECT);
		/*
		 * Set hang detection threshold to 1 million cycles
		 * (0xFFFF*16)
		 */
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_INT_CNTL,
					  (1 << 30) | 0xFFFF);
	}


	/* Turn on performance counters */
	kgsl_regwrite(device, A5XX_RBBM_PERFCTR_CNTL, 0x01);

	/*
	 * This is to increase performance by restricting VFD's cache access,
	 * so that LRZ and other data get evicted less.
	 */
	kgsl_regwrite(device, A5XX_UCHE_CACHE_WAYS, 0x02);

	/*
	 * Set UCHE_WRITE_THRU_BASE to the UCHE_TRAP_BASE effectively
	 * disabling L2 bypass
	 */
	kgsl_regwrite(device, A5XX_UCHE_TRAP_BASE_LO, 0xffff0000);
	kgsl_regwrite(device, A5XX_UCHE_TRAP_BASE_HI, 0x0001ffff);
	kgsl_regwrite(device, A5XX_UCHE_WRITE_THRU_BASE_LO, 0xffff0000);
	kgsl_regwrite(device, A5XX_UCHE_WRITE_THRU_BASE_HI, 0x0001ffff);

	/* Program the GMEM VA range for the UCHE path */
	kgsl_regwrite(device, A5XX_UCHE_GMEM_RANGE_MIN_LO,
				ADRENO_UCHE_GMEM_BASE);
	kgsl_regwrite(device, A5XX_UCHE_GMEM_RANGE_MIN_HI, 0x0);
	kgsl_regwrite(device, A5XX_UCHE_GMEM_RANGE_MAX_LO,
				ADRENO_UCHE_GMEM_BASE +
				adreno_dev->gmem_size - 1);
	kgsl_regwrite(device, A5XX_UCHE_GMEM_RANGE_MAX_HI, 0x0);

	/*
	 * Below CP registers are 0x0 by default, program init
	 * values based on a5xx flavor.
	 */
	if (adreno_is_a510(adreno_dev)) {
		kgsl_regwrite(device, A5XX_CP_MEQ_THRESHOLDS, 0x20);
		kgsl_regwrite(device, A5XX_CP_MERCIU_SIZE, 0x20);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_2, 0x40000030);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_1, 0x20100D0A);
	} else {
		kgsl_regwrite(device, A5XX_CP_MEQ_THRESHOLDS, 0x40);
		kgsl_regwrite(device, A5XX_CP_MERCIU_SIZE, 0x40);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_2, 0x80000060);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_1, 0x40201B16);
	}

	/*
	 * vtxFifo and primFifo thresholds default values
	 * are different for A510.
	 */
	if (adreno_is_a510(adreno_dev))
		kgsl_regwrite(device, A5XX_PC_DBG_ECO_CNTL,
						(0x200 << 11 | 0x200 << 22));
	else
		kgsl_regwrite(device, A5XX_PC_DBG_ECO_CNTL,
						(0x400 << 11 | 0x300 << 22));

	/*
	 * A5x USP LDST non valid pixel wrongly update read combine offset
	 * In A5xx we added optimization for read combine. There could be cases
	 * on a530 v1 there is no valid pixel but the active masks is not
	 * cleared and the offset can be wrongly updated if the invalid address
	 * can be combined. The wrongly latched value will make the returning
	 * data got shifted at wrong offset. workaround this issue by disabling
	 * LD combine, bit[25] of SP_DBG_ECO_CNTL (sp chicken bit[17]) need to
	 * be set to 1, default is 0(enable)
	 */
	if (adreno_is_a530v1(adreno_dev)) {
		kgsl_regread(device, A5XX_SP_DBG_ECO_CNTL, &val);
		val = (val | 1 << 25);
		kgsl_regwrite(device, A5XX_SP_DBG_ECO_CNTL, val);
	}

	/* Set the USE_RETENTION_FLOPS chicken bit */
	kgsl_regwrite(device, A5XX_CP_CHICKEN_DBG, 0x02000000);

	/* Enable ISDB mode if requested */
	if (test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv)) {
		if (!kgsl_active_count_get(device)) {
			/*
			* Disable ME/PFP split timeouts when the debugger is
			* enabled because the CP doesn't know when a shader is
			* in active debug
			*/
			kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL1, 0x06FFFFFF);

			/* Force the SP0/SP1 clocks on to enable ISDB */
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL_SP0, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL_SP1, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL_SP2, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL_SP3, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL2_SP0, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL2_SP1, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL2_SP2, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL2_SP3, 0x0);

			/* disable HWCG */
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_ISDB_CNT, 0x0);
		} else
			KGSL_CORE_ERR(
				"Active count failed while turning on ISDB.");
	} else {
		/* if not in ISDB mode enable ME/PFP split notification */
		kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL1, 0xA6FFFFFF);
		/* enable HWCG */
		a5xx_hwcg_init(adreno_dev);
	}

	kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL2, 0x0000003F);

	a5xx_protect_init(adreno_dev);
}

/*
 * a5xx_rb_init() - Initialize ringbuffer
 * @adreno_dev: Pointer to adreno device
 * @rb: Pointer to the ringbuffer of device
 *
 * Submit commands for ME initialization,
 */
int a5xx_rb_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	unsigned int *cmds;
	unsigned int rb_cntl;
	cmds = adreno_ringbuffer_allocspace(rb, 8);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);
	if (cmds == NULL)
		return -ENOSPC;

	/*
	 * For a5xx enable RB_ENA_PFP_UPDATE to allow PFP to write to
	 * context record during preemption
	 */
	if (adreno_is_preemption_enabled(adreno_dev)) {
		kgsl_regread(&adreno_dev->dev, A5XX_CP_RB_CNTL, &rb_cntl);
		rb_cntl = (rb_cntl | (1 << 26));
		kgsl_regwrite(&adreno_dev->dev, A5XX_CP_RB_CNTL, rb_cntl);
	}

	*cmds++ = cp_type7_packet(CP_ME_INIT, 7);
	/*
	 *  Mask -- look for all ordinals but drawcall
	 *  range and reset ucode scratch memory.
	 */
	*cmds++ = 0x0000000f;
	/* Multiple HW ctxs are unreliable on a530v1, use single hw context */
	if (adreno_is_a530v1(adreno_dev))
		*cmds++ = 0x00000000;
	else
		/* Use both contexts for 3D (bit0) 2D (bit1) */
		*cmds++ = 0x00000003;
	/* Enable register protection */
	*cmds++ = 0x20000000;
	/* Header dump address */
	*cmds++ = 0x00000000;
	/* Header dump enable and dump size */
	*cmds++ = 0x00000000;
	/* Below will be ignored by the CP unless bit4 in Mask is set */
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;

	adreno_ringbuffer_submit(rb, NULL);

	return 0;
}

static int _load_firmware(struct adreno_device *adreno_dev, const char *fwfile,
			  struct kgsl_memdesc *ucode, size_t *ucode_size,
			  unsigned int *ucode_version)
{
	struct kgsl_device *device = &adreno_dev->dev;
	const struct firmware *fw = NULL;
	int ret;

	ret = request_firmware(&fw, fwfile, device->dev);

	if (ret) {
		KGSL_DRV_ERR(device, "request_firmware(%s) failed: %d\n",
				fwfile, ret);
		return ret;
	}

	ret = kgsl_allocate_global(device, ucode, fw->size,
				KGSL_MEMFLAGS_GPUREADONLY, 0);

	if (ret)
		return ret;

	memcpy(ucode->hostptr, &fw->data[4], fw->size - 4);
	*ucode_size = (fw->size - 4) / sizeof(uint32_t);
	*ucode_version = *(unsigned int *)&fw->data[4];

	release_firmware(fw);

	return 0;
}

/*
 * a5xx_microcode_read() - Read microcode
 * @adreno_dev: Pointer to adreno device
 */
int a5xx_microcode_read(struct adreno_device *adreno_dev)
{
	int ret;

	ret = _load_firmware(adreno_dev,
			 adreno_dev->gpucore->pm4fw_name, &adreno_dev->pm4,
			 &adreno_dev->pm4_fw_size, &adreno_dev->pm4_fw_version);
	if (ret)
		return ret;

	ret = _load_firmware(adreno_dev,
			 adreno_dev->gpucore->pfpfw_name, &adreno_dev->pfp,
			 &adreno_dev->pfp_fw_size, &adreno_dev->pfp_fw_version);
	if (ret)
		return ret;

	if (!adreno_is_a510(adreno_dev))
		_load_regfile(adreno_dev);

	return ret;
}

/*
 * a5xx_microcode_load() - Load microcode
 * @adreno_dev: Pointer to adreno device
 * @start_type: type of device start cold/warm
 */
int a5xx_microcode_load(struct adreno_device *adreno_dev,
						unsigned int start_type)
{
	void *ptr;
	struct kgsl_device *device = &adreno_dev->dev;
	uint64_t gpuaddr;

	gpuaddr = adreno_dev->pm4.gpuaddr;
	kgsl_regwrite(device, A5XX_CP_PM4_INSTR_BASE_LO,
				(uint)gpuaddr);
	kgsl_regwrite(device, A5XX_CP_PM4_INSTR_BASE_HI,
				((uint64_t)(gpuaddr) >> 32));

	gpuaddr = adreno_dev->pfp.gpuaddr;
	kgsl_regwrite(device, A5XX_CP_PFP_INSTR_BASE_LO,
				(uint)gpuaddr);
	kgsl_regwrite(device, A5XX_CP_PFP_INSTR_BASE_HI,
				((uint64_t)(gpuaddr) >> 32));

	/* Load the zap shader firmware through PIL if its available */
	if (adreno_dev->gpucore->zap_name && !zap_ucode_loaded) {
		ptr = subsystem_get(adreno_dev->gpucore->zap_name);

		/* Disable content protecttion if the above call fails */
		if (IS_ERR_OR_NULL(ptr))
			device->mmu.secured = false;

		zap_ucode_loaded = 1;
	}

	return 0;
}

static struct adreno_perfcount_register a5xx_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_0_LO,
		A5XX_RBBM_PERFCTR_CP_0_HI, 0, A5XX_CP_PERFCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_1_LO,
		A5XX_RBBM_PERFCTR_CP_1_HI, 1, A5XX_CP_PERFCTR_CP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_2_LO,
		A5XX_RBBM_PERFCTR_CP_2_HI, 2, A5XX_CP_PERFCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_3_LO,
		A5XX_RBBM_PERFCTR_CP_3_HI, 3, A5XX_CP_PERFCTR_CP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_4_LO,
		A5XX_RBBM_PERFCTR_CP_4_HI, 4, A5XX_CP_PERFCTR_CP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_5_LO,
		A5XX_RBBM_PERFCTR_CP_5_HI, 5, A5XX_CP_PERFCTR_CP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_6_LO,
		A5XX_RBBM_PERFCTR_CP_6_HI, 6, A5XX_CP_PERFCTR_CP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_7_LO,
		A5XX_RBBM_PERFCTR_CP_7_HI, 7, A5XX_CP_PERFCTR_CP_SEL_7 },
};

/*
 * Note that PERFCTR_RBBM_0 is missing - it is used to emulate the PWR counters.
 * See below.
 */
static struct adreno_perfcount_register a5xx_perfcounters_rbbm[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RBBM_1_LO,
		A5XX_RBBM_PERFCTR_RBBM_1_HI, 9, A5XX_RBBM_PERFCTR_RBBM_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RBBM_2_LO,
		A5XX_RBBM_PERFCTR_RBBM_2_HI, 10, A5XX_RBBM_PERFCTR_RBBM_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RBBM_3_LO,
		A5XX_RBBM_PERFCTR_RBBM_3_HI, 11, A5XX_RBBM_PERFCTR_RBBM_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_pc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_0_LO,
		A5XX_RBBM_PERFCTR_PC_0_HI, 12, A5XX_PC_PERFCTR_PC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_1_LO,
		A5XX_RBBM_PERFCTR_PC_1_HI, 13, A5XX_PC_PERFCTR_PC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_2_LO,
		A5XX_RBBM_PERFCTR_PC_2_HI, 14, A5XX_PC_PERFCTR_PC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_3_LO,
		A5XX_RBBM_PERFCTR_PC_3_HI, 15, A5XX_PC_PERFCTR_PC_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_4_LO,
		A5XX_RBBM_PERFCTR_PC_4_HI, 16, A5XX_PC_PERFCTR_PC_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_5_LO,
		A5XX_RBBM_PERFCTR_PC_5_HI, 17, A5XX_PC_PERFCTR_PC_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_6_LO,
		A5XX_RBBM_PERFCTR_PC_6_HI, 18, A5XX_PC_PERFCTR_PC_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_7_LO,
		A5XX_RBBM_PERFCTR_PC_7_HI, 19, A5XX_PC_PERFCTR_PC_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vfd[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_0_LO,
		A5XX_RBBM_PERFCTR_VFD_0_HI, 20, A5XX_VFD_PERFCTR_VFD_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_1_LO,
		A5XX_RBBM_PERFCTR_VFD_1_HI, 21, A5XX_VFD_PERFCTR_VFD_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_2_LO,
		A5XX_RBBM_PERFCTR_VFD_2_HI, 22, A5XX_VFD_PERFCTR_VFD_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_3_LO,
		A5XX_RBBM_PERFCTR_VFD_3_HI, 23, A5XX_VFD_PERFCTR_VFD_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_4_LO,
		A5XX_RBBM_PERFCTR_VFD_4_HI, 24, A5XX_VFD_PERFCTR_VFD_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_5_LO,
		A5XX_RBBM_PERFCTR_VFD_5_HI, 25, A5XX_VFD_PERFCTR_VFD_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_6_LO,
		A5XX_RBBM_PERFCTR_VFD_6_HI, 26, A5XX_VFD_PERFCTR_VFD_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_7_LO,
		A5XX_RBBM_PERFCTR_VFD_7_HI, 27, A5XX_VFD_PERFCTR_VFD_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_hlsq[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_0_LO,
		A5XX_RBBM_PERFCTR_HLSQ_0_HI, 28, A5XX_HLSQ_PERFCTR_HLSQ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_1_LO,
		A5XX_RBBM_PERFCTR_HLSQ_1_HI, 29, A5XX_HLSQ_PERFCTR_HLSQ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_2_LO,
		A5XX_RBBM_PERFCTR_HLSQ_2_HI, 30, A5XX_HLSQ_PERFCTR_HLSQ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_3_LO,
		A5XX_RBBM_PERFCTR_HLSQ_3_HI, 31, A5XX_HLSQ_PERFCTR_HLSQ_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_4_LO,
		A5XX_RBBM_PERFCTR_HLSQ_4_HI, 32, A5XX_HLSQ_PERFCTR_HLSQ_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_5_LO,
		A5XX_RBBM_PERFCTR_HLSQ_5_HI, 33, A5XX_HLSQ_PERFCTR_HLSQ_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_6_LO,
		A5XX_RBBM_PERFCTR_HLSQ_6_HI, 34, A5XX_HLSQ_PERFCTR_HLSQ_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_7_LO,
		A5XX_RBBM_PERFCTR_HLSQ_7_HI, 35, A5XX_HLSQ_PERFCTR_HLSQ_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vpc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VPC_0_LO,
		A5XX_RBBM_PERFCTR_VPC_0_HI, 36, A5XX_VPC_PERFCTR_VPC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VPC_1_LO,
		A5XX_RBBM_PERFCTR_VPC_1_HI, 37, A5XX_VPC_PERFCTR_VPC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VPC_2_LO,
		A5XX_RBBM_PERFCTR_VPC_2_HI, 38, A5XX_VPC_PERFCTR_VPC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VPC_3_LO,
		A5XX_RBBM_PERFCTR_VPC_3_HI, 39, A5XX_VPC_PERFCTR_VPC_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_ccu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CCU_0_LO,
		A5XX_RBBM_PERFCTR_CCU_0_HI, 40, A5XX_RB_PERFCTR_CCU_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CCU_1_LO,
		A5XX_RBBM_PERFCTR_CCU_1_HI, 41, A5XX_RB_PERFCTR_CCU_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CCU_2_LO,
		A5XX_RBBM_PERFCTR_CCU_2_HI, 42, A5XX_RB_PERFCTR_CCU_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CCU_3_LO,
		A5XX_RBBM_PERFCTR_CCU_3_HI, 43, A5XX_RB_PERFCTR_CCU_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_tse[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TSE_0_LO,
		A5XX_RBBM_PERFCTR_TSE_0_HI, 44, A5XX_GRAS_PERFCTR_TSE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TSE_1_LO,
		A5XX_RBBM_PERFCTR_TSE_1_HI, 45, A5XX_GRAS_PERFCTR_TSE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TSE_2_LO,
		A5XX_RBBM_PERFCTR_TSE_2_HI, 46, A5XX_GRAS_PERFCTR_TSE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TSE_3_LO,
		A5XX_RBBM_PERFCTR_TSE_3_HI, 47, A5XX_GRAS_PERFCTR_TSE_SEL_3 },
};


static struct adreno_perfcount_register a5xx_perfcounters_ras[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RAS_0_LO,
		A5XX_RBBM_PERFCTR_RAS_0_HI, 48, A5XX_GRAS_PERFCTR_RAS_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RAS_1_LO,
		A5XX_RBBM_PERFCTR_RAS_1_HI, 49, A5XX_GRAS_PERFCTR_RAS_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RAS_2_LO,
		A5XX_RBBM_PERFCTR_RAS_2_HI, 50, A5XX_GRAS_PERFCTR_RAS_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RAS_3_LO,
		A5XX_RBBM_PERFCTR_RAS_3_HI, 51, A5XX_GRAS_PERFCTR_RAS_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_0_LO,
		A5XX_RBBM_PERFCTR_UCHE_0_HI, 52, A5XX_UCHE_PERFCTR_UCHE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_1_LO,
		A5XX_RBBM_PERFCTR_UCHE_1_HI, 53, A5XX_UCHE_PERFCTR_UCHE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_2_LO,
		A5XX_RBBM_PERFCTR_UCHE_2_HI, 54, A5XX_UCHE_PERFCTR_UCHE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_3_LO,
		A5XX_RBBM_PERFCTR_UCHE_3_HI, 55, A5XX_UCHE_PERFCTR_UCHE_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_4_LO,
		A5XX_RBBM_PERFCTR_UCHE_4_HI, 56, A5XX_UCHE_PERFCTR_UCHE_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_5_LO,
		A5XX_RBBM_PERFCTR_UCHE_5_HI, 57, A5XX_UCHE_PERFCTR_UCHE_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_6_LO,
		A5XX_RBBM_PERFCTR_UCHE_6_HI, 58, A5XX_UCHE_PERFCTR_UCHE_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_7_LO,
		A5XX_RBBM_PERFCTR_UCHE_7_HI, 59, A5XX_UCHE_PERFCTR_UCHE_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_0_LO,
		A5XX_RBBM_PERFCTR_TP_0_HI, 60, A5XX_TPL1_PERFCTR_TP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_1_LO,
		A5XX_RBBM_PERFCTR_TP_1_HI, 61, A5XX_TPL1_PERFCTR_TP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_2_LO,
		A5XX_RBBM_PERFCTR_TP_2_HI, 62, A5XX_TPL1_PERFCTR_TP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_3_LO,
		A5XX_RBBM_PERFCTR_TP_3_HI, 63, A5XX_TPL1_PERFCTR_TP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_4_LO,
		A5XX_RBBM_PERFCTR_TP_4_HI, 64, A5XX_TPL1_PERFCTR_TP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_5_LO,
		A5XX_RBBM_PERFCTR_TP_5_HI, 65, A5XX_TPL1_PERFCTR_TP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_6_LO,
		A5XX_RBBM_PERFCTR_TP_6_HI, 66, A5XX_TPL1_PERFCTR_TP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_7_LO,
		A5XX_RBBM_PERFCTR_TP_7_HI, 67, A5XX_TPL1_PERFCTR_TP_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_0_LO,
		A5XX_RBBM_PERFCTR_SP_0_HI, 68, A5XX_SP_PERFCTR_SP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_1_LO,
		A5XX_RBBM_PERFCTR_SP_1_HI, 69, A5XX_SP_PERFCTR_SP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_2_LO,
		A5XX_RBBM_PERFCTR_SP_2_HI, 70, A5XX_SP_PERFCTR_SP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_3_LO,
		A5XX_RBBM_PERFCTR_SP_3_HI, 71, A5XX_SP_PERFCTR_SP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_4_LO,
		A5XX_RBBM_PERFCTR_SP_4_HI, 72, A5XX_SP_PERFCTR_SP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_5_LO,
		A5XX_RBBM_PERFCTR_SP_5_HI, 73, A5XX_SP_PERFCTR_SP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_6_LO,
		A5XX_RBBM_PERFCTR_SP_6_HI, 74, A5XX_SP_PERFCTR_SP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_7_LO,
		A5XX_RBBM_PERFCTR_SP_7_HI, 75, A5XX_SP_PERFCTR_SP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_8_LO,
		A5XX_RBBM_PERFCTR_SP_8_HI, 76, A5XX_SP_PERFCTR_SP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_9_LO,
		A5XX_RBBM_PERFCTR_SP_9_HI, 77, A5XX_SP_PERFCTR_SP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_10_LO,
		A5XX_RBBM_PERFCTR_SP_10_HI, 78, A5XX_SP_PERFCTR_SP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_11_LO,
		A5XX_RBBM_PERFCTR_SP_11_HI, 79, A5XX_SP_PERFCTR_SP_SEL_11 },
};

static struct adreno_perfcount_register a5xx_perfcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_0_LO,
		A5XX_RBBM_PERFCTR_RB_0_HI, 80, A5XX_RB_PERFCTR_RB_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_1_LO,
		A5XX_RBBM_PERFCTR_RB_1_HI, 81, A5XX_RB_PERFCTR_RB_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_2_LO,
		A5XX_RBBM_PERFCTR_RB_2_HI, 82, A5XX_RB_PERFCTR_RB_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_3_LO,
		A5XX_RBBM_PERFCTR_RB_3_HI, 83, A5XX_RB_PERFCTR_RB_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_4_LO,
		A5XX_RBBM_PERFCTR_RB_4_HI, 84, A5XX_RB_PERFCTR_RB_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_5_LO,
		A5XX_RBBM_PERFCTR_RB_5_HI, 85, A5XX_RB_PERFCTR_RB_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_6_LO,
		A5XX_RBBM_PERFCTR_RB_6_HI, 86, A5XX_RB_PERFCTR_RB_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_7_LO,
		A5XX_RBBM_PERFCTR_RB_7_HI, 87, A5XX_RB_PERFCTR_RB_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vsc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VSC_0_LO,
		A5XX_RBBM_PERFCTR_VSC_0_HI, 88, A5XX_VSC_PERFCTR_VSC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VSC_1_LO,
		A5XX_RBBM_PERFCTR_VSC_1_HI, 89, A5XX_VSC_PERFCTR_VSC_SEL_1 },
};

static struct adreno_perfcount_register a5xx_perfcounters_lrz[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_0_LO,
		A5XX_RBBM_PERFCTR_LRZ_0_HI, 90, A5XX_GRAS_PERFCTR_LRZ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_1_LO,
		A5XX_RBBM_PERFCTR_LRZ_1_HI, 91, A5XX_GRAS_PERFCTR_LRZ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_2_LO,
		A5XX_RBBM_PERFCTR_LRZ_2_HI, 92, A5XX_GRAS_PERFCTR_LRZ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_3_LO,
		A5XX_RBBM_PERFCTR_LRZ_3_HI, 93, A5XX_GRAS_PERFCTR_LRZ_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_cmp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_0_LO,
		A5XX_RBBM_PERFCTR_CMP_0_HI, 94, A5XX_RB_PERFCTR_CMP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_1_LO,
		A5XX_RBBM_PERFCTR_CMP_1_HI, 95, A5XX_RB_PERFCTR_CMP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_2_LO,
		A5XX_RBBM_PERFCTR_CMP_2_HI, 96, A5XX_RB_PERFCTR_CMP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_3_LO,
		A5XX_RBBM_PERFCTR_CMP_3_HI, 97, A5XX_RB_PERFCTR_CMP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vbif[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_CNT_LOW0,
		A5XX_VBIF_PERF_CNT_HIGH0, -1, A5XX_VBIF_PERF_CNT_SEL0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_CNT_LOW1,
		A5XX_VBIF_PERF_CNT_HIGH1, -1, A5XX_VBIF_PERF_CNT_SEL1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_CNT_LOW2,
		A5XX_VBIF_PERF_CNT_HIGH2, -1, A5XX_VBIF_PERF_CNT_SEL2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_CNT_LOW3,
		A5XX_VBIF_PERF_CNT_HIGH3, -1, A5XX_VBIF_PERF_CNT_SEL3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vbif_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_PWR_CNT_LOW0,
		A5XX_VBIF_PERF_PWR_CNT_HIGH0, -1, A5XX_VBIF_PERF_PWR_CNT_EN0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_PWR_CNT_LOW1,
		A5XX_VBIF_PERF_PWR_CNT_HIGH1, -1, A5XX_VBIF_PERF_PWR_CNT_EN1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_PWR_CNT_LOW2,
		A5XX_VBIF_PERF_PWR_CNT_HIGH2, -1, A5XX_VBIF_PERF_PWR_CNT_EN2 },
};

static struct adreno_perfcount_register a5xx_perfcounters_alwayson[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_ALWAYSON_COUNTER_LO,
		A5XX_RBBM_ALWAYSON_COUNTER_HI, -1 },
};

/*
 * 5XX targets don't really have physical PERFCTR_PWR registers - we emulate
 * them using similar performance counters from the RBBM block. The difference
 * betweeen using this group and the RBBM group is that the RBBM counters are
 * reloaded after a power collapse which is not how the PWR counters behaved on
 * legacy hardware. In order to limit the disruption on the rest of the system
 * we go out of our way to ensure backwards compatability. Since RBBM counters
 * are in short supply, we don't emulate PWR:0 which nobody uses - mark it as
 * broken.
 */
static struct adreno_perfcount_register a5xx_perfcounters_pwr[] = {
	{ KGSL_PERFCOUNTER_BROKEN, 0, 0, 0, 0, -1, 0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RBBM_0_LO,
		A5XX_RBBM_PERFCTR_RBBM_0_HI, -1, 0},
};

static struct adreno_perfcount_register a5xx_pwrcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_SP_POWER_COUNTER_0_LO,
		A5XX_SP_POWER_COUNTER_0_HI, -1, A5XX_SP_POWERCTR_SP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_SP_POWER_COUNTER_1_LO,
		A5XX_SP_POWER_COUNTER_1_HI, -1, A5XX_SP_POWERCTR_SP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_SP_POWER_COUNTER_2_LO,
		A5XX_SP_POWER_COUNTER_2_HI, -1, A5XX_SP_POWERCTR_SP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_SP_POWER_COUNTER_3_LO,
		A5XX_SP_POWER_COUNTER_3_HI, -1, A5XX_SP_POWERCTR_SP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_TP_POWER_COUNTER_0_LO,
		A5XX_TP_POWER_COUNTER_0_HI, -1, A5XX_TPL1_POWERCTR_TP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_TP_POWER_COUNTER_1_LO,
		A5XX_TP_POWER_COUNTER_1_HI, -1, A5XX_TPL1_POWERCTR_TP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_TP_POWER_COUNTER_2_LO,
		A5XX_TP_POWER_COUNTER_2_HI, -1, A5XX_TPL1_POWERCTR_TP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_TP_POWER_COUNTER_3_LO,
		A5XX_TP_POWER_COUNTER_3_HI, -1, A5XX_TPL1_POWERCTR_TP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RB_POWER_COUNTER_0_LO,
		A5XX_RB_POWER_COUNTER_0_HI, -1, A5XX_RB_POWERCTR_RB_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RB_POWER_COUNTER_1_LO,
		A5XX_RB_POWER_COUNTER_1_HI, -1, A5XX_RB_POWERCTR_RB_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RB_POWER_COUNTER_2_LO,
		A5XX_RB_POWER_COUNTER_2_HI, -1, A5XX_RB_POWERCTR_RB_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RB_POWER_COUNTER_3_LO,
		A5XX_RB_POWER_COUNTER_3_HI, -1, A5XX_RB_POWERCTR_RB_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_ccu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CCU_POWER_COUNTER_0_LO,
		A5XX_CCU_POWER_COUNTER_0_HI, -1, A5XX_RB_POWERCTR_CCU_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CCU_POWER_COUNTER_1_LO,
		A5XX_CCU_POWER_COUNTER_1_HI, -1, A5XX_RB_POWERCTR_CCU_SEL_1 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_UCHE_POWER_COUNTER_0_LO,
		A5XX_UCHE_POWER_COUNTER_0_HI, -1,
		A5XX_UCHE_POWERCTR_UCHE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_UCHE_POWER_COUNTER_1_LO,
		A5XX_UCHE_POWER_COUNTER_1_HI, -1,
		A5XX_UCHE_POWERCTR_UCHE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_UCHE_POWER_COUNTER_2_LO,
		A5XX_UCHE_POWER_COUNTER_2_HI, -1,
		A5XX_UCHE_POWERCTR_UCHE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_UCHE_POWER_COUNTER_3_LO,
		A5XX_UCHE_POWER_COUNTER_3_HI, -1,
		A5XX_UCHE_POWERCTR_UCHE_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CP_POWER_COUNTER_0_LO,
		A5XX_CP_POWER_COUNTER_0_HI, -1, A5XX_CP_POWERCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CP_POWER_COUNTER_1_LO,
		A5XX_CP_POWER_COUNTER_1_HI, -1, A5XX_CP_POWERCTR_CP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CP_POWER_COUNTER_2_LO,
		A5XX_CP_POWER_COUNTER_2_HI, -1, A5XX_CP_POWERCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CP_POWER_COUNTER_3_LO,
		A5XX_CP_POWER_COUNTER_3_HI, -1, A5XX_CP_POWERCTR_CP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_gpmu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_0_LO,
		A5XX_GPMU_POWER_COUNTER_0_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_1_LO,
		A5XX_GPMU_POWER_COUNTER_1_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_2_LO,
		A5XX_GPMU_POWER_COUNTER_2_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_3_LO,
		A5XX_GPMU_POWER_COUNTER_3_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_4_LO,
		A5XX_GPMU_POWER_COUNTER_4_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_5_LO,
		A5XX_GPMU_POWER_COUNTER_5_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_1 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_alwayson[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_ALWAYS_ON_COUNTER_LO,
		A5XX_GPMU_ALWAYS_ON_COUNTER_HI, -1 },
};

#define A5XX_PERFCOUNTER_GROUP(offset, name) \
	ADRENO_PERFCOUNTER_GROUP(a5xx, offset, name)

#define A5XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a5xx, offset, name, flags)

#define A5XX_POWER_COUNTER_GROUP(offset, name) \
	ADRENO_POWER_COUNTER_GROUP(a5xx, offset, name)

static struct adreno_perfcount_group a5xx_perfcounter_groups
				[KGSL_PERFCOUNTER_GROUP_MAX] = {
	A5XX_PERFCOUNTER_GROUP(CP, cp),
	A5XX_PERFCOUNTER_GROUP(RBBM, rbbm),
	A5XX_PERFCOUNTER_GROUP(PC, pc),
	A5XX_PERFCOUNTER_GROUP(VFD, vfd),
	A5XX_PERFCOUNTER_GROUP(HLSQ, hlsq),
	A5XX_PERFCOUNTER_GROUP(VPC, vpc),
	A5XX_PERFCOUNTER_GROUP(CCU, ccu),
	A5XX_PERFCOUNTER_GROUP(CMP, cmp),
	A5XX_PERFCOUNTER_GROUP(TSE, tse),
	A5XX_PERFCOUNTER_GROUP(RAS, ras),
	A5XX_PERFCOUNTER_GROUP(LRZ, lrz),
	A5XX_PERFCOUNTER_GROUP(UCHE, uche),
	A5XX_PERFCOUNTER_GROUP(TP, tp),
	A5XX_PERFCOUNTER_GROUP(SP, sp),
	A5XX_PERFCOUNTER_GROUP(RB, rb),
	A5XX_PERFCOUNTER_GROUP(VSC, vsc),
	A5XX_PERFCOUNTER_GROUP_FLAGS(PWR, pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
	A5XX_PERFCOUNTER_GROUP(VBIF, vbif),
	A5XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, vbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
	A5XX_PERFCOUNTER_GROUP_FLAGS(ALWAYSON, alwayson,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
	A5XX_POWER_COUNTER_GROUP(SP, sp),
	A5XX_POWER_COUNTER_GROUP(TP, tp),
	A5XX_POWER_COUNTER_GROUP(RB, rb),
	A5XX_POWER_COUNTER_GROUP(CCU, ccu),
	A5XX_POWER_COUNTER_GROUP(UCHE, uche),
	A5XX_POWER_COUNTER_GROUP(CP, cp),
	A5XX_POWER_COUNTER_GROUP(GPMU, gpmu),
	A5XX_POWER_COUNTER_GROUP(ALWAYSON, alwayson),
};

static struct adreno_perfcounters a5xx_perfcounters = {
	a5xx_perfcounter_groups,
	ARRAY_SIZE(a5xx_perfcounter_groups),
};

struct adreno_ft_perf_counters a5xx_ft_perf_counters[] = {
	{KGSL_PERFCOUNTER_GROUP_SP, A5XX_SP_ALU_ACTIVE_CYCLES},
	{KGSL_PERFCOUNTER_GROUP_SP, A5XX_SP0_ICL1_MISSES},
	{KGSL_PERFCOUNTER_GROUP_SP, A5XX_SP_FS_CFLOW_INSTRUCTIONS},
	{KGSL_PERFCOUNTER_GROUP_TSE, A5XX_TSE_INPUT_PRIM_NUM},
};

/* Register offset defines for A5XX, in order of enum adreno_regs */
static unsigned int a5xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_REG_CP_WFI_PEND_CTR, A5XX_CP_WFI_PEND_CTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A5XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE_HI, A5XX_CP_RB_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A5XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A5XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, A5XX_CP_ME_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_CNTL, A5XX_CP_RB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, A5XX_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE_HI, A5XX_CP_IB1_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, A5XX_CP_IB1_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, A5XX_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE_HI, A5XX_CP_IB2_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, A5XX_CP_IB2_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_ADDR, A5XX_CP_ROQ_DBG_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_DATA, A5XX_CP_ROQ_DBG_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_ADDR, A5XX_CP_MERCIU_DBG_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_DATA, A5XX_CP_MERCIU_DBG_DATA_1),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_DATA2,
				A5XX_CP_MERCIU_DBG_DATA_2),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MEQ_ADDR, A5XX_CP_MEQ_DBG_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MEQ_DATA, A5XX_CP_MEQ_DBG_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PROTECT_REG_0, A5XX_CP_PROTECT_REG_0),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT, A5XX_CP_CONTEXT_SWITCH_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT_DEBUG, ADRENO_REG_SKIP),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT_DISABLE, ADRENO_REG_SKIP),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
				A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
				A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A5XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_CTL, A5XX_RBBM_PERFCTR_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
					A5XX_RBBM_PERFCTR_LOAD_CMD0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
					A5XX_RBBM_PERFCTR_LOAD_CMD1),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
					A5XX_RBBM_PERFCTR_LOAD_CMD2),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD3,
					A5XX_RBBM_PERFCTR_LOAD_CMD3),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A5XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_STATUS, A5XX_RBBM_INT_0_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_CLOCK_CTL, A5XX_RBBM_CLOCK_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_CLEAR_CMD,
				A5XX_RBBM_INT_CLEAR_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, A5XX_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD,
					  A5XX_RBBM_BLOCK_SW_RESET_CMD),
		ADRENO_REG_DEFINE(ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD2,
					  A5XX_RBBM_BLOCK_SW_RESET_CMD2),
	ADRENO_REG_DEFINE(ADRENO_REG_UCHE_INVALIDATE0, A5XX_UCHE_INVALIDATE0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
				A5XX_RBBM_PERFCTR_LOAD_VALUE_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
				A5XX_RBBM_PERFCTR_LOAD_VALUE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TRUST_CONTROL,
				A5XX_RBBM_SECVID_TRUST_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TRUST_CONFIG,
				A5XX_RBBM_SECVID_TRUST_CONFIG),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TSB_CONTROL,
				A5XX_RBBM_SECVID_TSB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE,
				A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_SIZE,
				A5XX_RBBM_SECVID_TSB_TRUSTED_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO,
				A5XX_RBBM_ALWAYSON_COUNTER_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI,
				A5XX_RBBM_ALWAYSON_COUNTER_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_XIN_HALT_CTRL0,
				A5XX_VBIF_XIN_HALT_CTRL0),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_XIN_HALT_CTRL1,
				A5XX_VBIF_XIN_HALT_CTRL1),

};

const struct adreno_reg_offsets a5xx_reg_offsets = {
	.offsets = a5xx_register_offsets,
	.offset_0 = ADRENO_REG_REGISTER_MAX,
};

void a5xx_cp_hw_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int status1, status2;

	kgsl_regread(device, A5XX_CP_INTERRUPT_STATUS, &status1);

	if (status1 & BIT(A5XX_CP_OPCODE_ERROR)) {
		unsigned int val;

		kgsl_regwrite(device, A5XX_CP_PFP_STAT_ADDR, 0);

		/*
		 * A5XX_CP_PFP_STAT_DATA is indexed, so read it twice to get the
		 * value we want
		 */
		kgsl_regread(device, A5XX_CP_PFP_STAT_DATA, &val);
		kgsl_regread(device, A5XX_CP_PFP_STAT_DATA, &val);

		KGSL_DRV_CRIT_RATELIMIT(device,
			"ringbuffer opcode error | possible opcode=0x%8.8X\n",
			val);
	}
	if (status1 & BIT(A5XX_CP_RESERVED_BIT_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device,
					"ringbuffer reserved bit error interrupt\n");
	if (status1 & BIT(A5XX_CP_HW_FAULT_ERROR)) {
		kgsl_regread(device, A5XX_CP_HW_FAULT, &status2);
		KGSL_DRV_CRIT_RATELIMIT(device,
					"CP | Ringbuffer HW fault | status=%x\n",
					status2);
	}
	if (status1 & BIT(A5XX_CP_DMA_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device, "CP | DMA error\n");
	if (status1 & BIT(A5XX_CP_REGISTER_PROTECTION_ERROR)) {
		kgsl_regread(device, A5XX_CP_PROTECT_STATUS, &status2);
		KGSL_DRV_CRIT_RATELIMIT(device,
					"CP | Protected mode error| %s | addr=%x | status=%x\n",
					status2 & (1 << 24) ? "WRITE" : "READ",
					(status2 & 0xFFFFF) >> 2, status2);
	}
	if (status1 & BIT(A5XX_CP_AHB_ERROR)) {
		kgsl_regread(device, A5XX_CP_AHB_FAULT, &status2);
		KGSL_DRV_CRIT_RATELIMIT(device,
					"ringbuffer AHB error interrupt | status=%x\n",
					status2);
	}
}

void a5xx_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int reg;

	switch (bit) {
	case A5XX_INT_RBBM_AHB_ERROR: {
		kgsl_regread(device, A5XX_RBBM_AHB_ERROR_STATUS, &reg);

		/*
		 * Return the word address of the erroring register so that it
		 * matches the register specification
		 */
		KGSL_DRV_CRIT(device,
			"RBBM | AHB bus error | %s | addr=%x | ports=%x:%x\n",
			reg & (1 << 28) ? "WRITE" : "READ",
			(reg & 0xFFFFF) >> 2, (reg >> 20) & 0x3,
			(reg >> 24) & 0xF);

		/* Clear the error */
		kgsl_regwrite(device, A5XX_RBBM_AHB_CMD, (1 << 4));
		return;
	}
	case A5XX_INT_RBBM_TRANSFER_TIMEOUT:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: AHB transfer timeout\n");
		break;
	case A5XX_INT_RBBM_ME_MS_TIMEOUT:
		kgsl_regread(device, A5XX_RBBM_AHB_ME_SPLIT_STATUS, &reg);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"RBBM | ME master split timeout | status=%x\n", reg);
		break;
	case A5XX_INT_RBBM_PFP_MS_TIMEOUT:
		kgsl_regread(device, A5XX_RBBM_AHB_PFP_SPLIT_STATUS, &reg);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"RBBM | PFP master split timeout | status=%x\n", reg);
		break;
	case A5XX_INT_RBBM_ETS_MS_TIMEOUT:
		KGSL_DRV_CRIT_RATELIMIT(device,
			"RBBM: ME master split timeout\n");
		break;
	case A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ATB ASYNC overflow\n");
		break;
	case A5XX_INT_RBBM_ATB_BUS_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ATB bus overflow\n");
		break;
	case A5XX_INT_UCHE_OOB_ACCESS:
		KGSL_DRV_CRIT_RATELIMIT(device, "UCHE: Out of bounds access\n");
		break;
	case A5XX_INT_UCHE_TRAP_INTR:
		KGSL_DRV_CRIT_RATELIMIT(device, "UCHE: Trap interrupt\n");
		break;
	default:
		KGSL_DRV_CRIT_RATELIMIT(device, "Unknown interrupt %d\n", bit);
	}
}

/*
* a5x_gpc_err_int_callback() - Isr for GPC error interrupts
* @adreno_dev: Pointer to device
* @bit: Interrupt bit
*/
void a5x_gpc_err_int_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = &adreno_dev->dev;

	/*
	 * GPC error is typically the result of mistake SW programming.
	 * Force GPU fault for this interrupt so that we can debug it
	 * with help of register dump.
	 */

	KGSL_DRV_CRIT(device, "RBBM: GPC error\n");
	adreno_irqctrl(adreno_dev, 0);

	/* Trigger a fault in the dispatcher - this will effect a restart */
	adreno_set_gpu_fault(ADRENO_DEVICE(device), ADRENO_SOFT_FAULT);
	adreno_dispatcher_schedule(device);
}

#define A5XX_INT_MASK \
	((1 << A5XX_INT_RBBM_AHB_ERROR) |		\
	 (1 << A5XX_INT_RBBM_TRANSFER_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ME_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_PFP_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ETS_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW) |		\
	 (1 << A5XX_INT_RBBM_GPC_ERROR) |		\
	 (1 << A5XX_INT_CP_HW_ERROR) |	\
	 (1 << A5XX_INT_CP_IB1) |			\
	 (1 << A5XX_INT_CP_IB2) |			\
	 (1 << A5XX_INT_CP_RB) |			\
	 (1 << A5XX_INT_RBBM_ATB_BUS_OVERFLOW) |	\
	 (1 << A5XX_INT_UCHE_OOB_ACCESS)) |		\
	 (1 << A5XX_INT_UCHE_TRAP_INTR)


static struct adreno_irq_funcs a5xx_irq_funcs[] = {
	ADRENO_IRQ_CALLBACK(NULL),              /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 2 - RBBM_TRANSFER_TIMEOUT */
	/* 3 - RBBM_ME_MASTER_SPLIT_TIMEOUT  */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback),
	/* 4 - RBBM_PFP_MASTER_SPLIT_TIMEOUT */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback),
	 /* 5 - RBBM_ETS_MASTER_SPLIT_TIMEOUT */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback),
	/* 6 - RBBM_ATB_ASYNC_OVERFLOW */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback),
	ADRENO_IRQ_CALLBACK(a5x_gpc_err_int_callback), /* 7 - GPC_ERR */
	ADRENO_IRQ_CALLBACK(NULL),				/* 8 - CP_SW */
	ADRENO_IRQ_CALLBACK(a5xx_cp_hw_err_callback), /* 9 - CP_HW_ERROR */
	/* 10 - CP_CCU_FLUSH_DEPTH_TS */
	ADRENO_IRQ_CALLBACK(NULL),
	 /* 11 - CP_CCU_FLUSH_COLOR_TS */
	ADRENO_IRQ_CALLBACK(NULL),
	 /* 12 - CP_CCU_RESOLVE_TS */
	ADRENO_IRQ_CALLBACK(NULL),
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 13 - CP_IB2_INT */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 14 - CP_IB1_INT */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 15 - CP_RB_INT */
	/* 16 - CCP_UNUSED_1 */
	ADRENO_IRQ_CALLBACK(NULL),
	ADRENO_IRQ_CALLBACK(NULL), /* 17 - CP_RB_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 18 - CP_WT_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 19 - UNKNOWN_1 */
	ADRENO_IRQ_CALLBACK(NULL), /* 20 - CP_CACHE_FLUSH_TS */
	/* 21 - UNUSED_2 */
	ADRENO_IRQ_CALLBACK(NULL),
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 22 - RBBM_ATB_BUS_OVERFLOW */
	/* 23 - MISC_HANG_DETECT */
	ADRENO_IRQ_CALLBACK(adreno_hang_int_callback),
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 24 - UCHE_OOB_ACCESS */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 25 - UCHE_TRAP_INTR */
	ADRENO_IRQ_CALLBACK(NULL), /* 27 - DEBBUS_INTR_0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 28 - DEBBUS_INTR_1 */
	ADRENO_IRQ_CALLBACK(NULL), /* 29 - GPMU_ERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 29 - GPMU_THERMAL */
	ADRENO_IRQ_CALLBACK(NULL), /* 30 - ISDB_CPU_IRQ */
	ADRENO_IRQ_CALLBACK(NULL), /* 31 - ISDB_UNDER_DEBUG */
};

static struct adreno_irq a5xx_irq = {
	.funcs = a5xx_irq_funcs,
	.funcs_count = ARRAY_SIZE(a5xx_irq_funcs),
	.mask = A5XX_INT_MASK,
};

/*
 * Default size for CP queues for A5xx targets. You must
 * overwrite these value in gpudev_init function for
 * A5xx derivatives if size differs.
 */
static struct adreno_snapshot_sizes a5xx_snap_sizes = {
	.cp_pfp = 36,
	.cp_me = 29,
	.cp_meq = 64,
	.cp_merciu = 64,
	.roq = 512,
};

static struct adreno_snapshot_data a5xx_snapshot_data = {
	.sect_sizes = &a5xx_snap_sizes,
};

static struct adreno_coresight_register a5xx_coresight_registers[] = {
	{ A5XX_RBBM_CFG_DBGBUS_SEL_A },
	{ A5XX_RBBM_CFG_DBGBUS_SEL_B },
	{ A5XX_RBBM_CFG_DBGBUS_SEL_C },
	{ A5XX_RBBM_CFG_DBGBUS_SEL_D },
	{ A5XX_RBBM_CFG_DBGBUS_CNTLT },
	{ A5XX_RBBM_CFG_DBGBUS_CNTLM },
	{ A5XX_RBBM_CFG_DBGBUS_OPL },
	{ A5XX_RBBM_CFG_DBGBUS_OPE },
	{ A5XX_RBBM_CFG_DBGBUS_IVTL_0 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTL_1 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTL_2 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTL_3 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKL_0 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKL_1 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKL_2 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKL_3 },
	{ A5XX_RBBM_CFG_DBGBUS_BYTEL_0 },
	{ A5XX_RBBM_CFG_DBGBUS_BYTEL_1 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTE_0 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTE_1 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTE_2 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTE_3 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKE_0 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKE_1 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKE_2 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKE_3 },
	{ A5XX_RBBM_CFG_DBGBUS_NIBBLEE },
	{ A5XX_RBBM_CFG_DBGBUS_PTRC0 },
	{ A5XX_RBBM_CFG_DBGBUS_PTRC1 },
	{ A5XX_RBBM_CFG_DBGBUS_LOADREG },
	{ A5XX_RBBM_CFG_DBGBUS_IDX },
	{ A5XX_RBBM_CFG_DBGBUS_CLRC },
	{ A5XX_RBBM_CFG_DBGBUS_LOADIVT },
	{ A5XX_RBBM_CFG_DBGBUS_EVENT_LOGIC },
	{ A5XX_RBBM_CFG_DBGBUS_OVER },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT0 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT1 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT2 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT3 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT4 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT5 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_ADDR },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF0 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF1 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF2 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF3 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF4 },
	{ A5XX_RBBM_CFG_DBGBUS_MISR0 },
	{ A5XX_RBBM_CFG_DBGBUS_MISR1 },
	{ A5XX_RBBM_AHB_DBG_CNTL },
	{ A5XX_RBBM_READ_AHB_THROUGH_DBG },
	{ A5XX_RBBM_DBG_LO_HI_GPIO },
	{ A5XX_RBBM_EXT_TRACE_BUS_CNTL },
	{ A5XX_RBBM_EXT_VBIF_DBG_CNTL },
};

static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_a, &a5xx_coresight_registers[0]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_b, &a5xx_coresight_registers[1]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_c, &a5xx_coresight_registers[2]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_d, &a5xx_coresight_registers[3]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlt, &a5xx_coresight_registers[4]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlm, &a5xx_coresight_registers[5]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_opl, &a5xx_coresight_registers[6]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ope, &a5xx_coresight_registers[7]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_0, &a5xx_coresight_registers[8]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_1, &a5xx_coresight_registers[9]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_2, &a5xx_coresight_registers[10]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_3, &a5xx_coresight_registers[11]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_0, &a5xx_coresight_registers[12]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_1, &a5xx_coresight_registers[13]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_2, &a5xx_coresight_registers[14]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_3, &a5xx_coresight_registers[15]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_0, &a5xx_coresight_registers[16]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_1, &a5xx_coresight_registers[17]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_0, &a5xx_coresight_registers[18]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_1, &a5xx_coresight_registers[19]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_2, &a5xx_coresight_registers[20]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_3, &a5xx_coresight_registers[21]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_0, &a5xx_coresight_registers[22]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_1, &a5xx_coresight_registers[23]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_2, &a5xx_coresight_registers[24]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_3, &a5xx_coresight_registers[25]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_nibblee, &a5xx_coresight_registers[26]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc0, &a5xx_coresight_registers[27]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc1, &a5xx_coresight_registers[28]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadreg, &a5xx_coresight_registers[29]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_idx, &a5xx_coresight_registers[30]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_clrc, &a5xx_coresight_registers[31]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadivt, &a5xx_coresight_registers[32]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_event_logic,
				&a5xx_coresight_registers[33]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_over, &a5xx_coresight_registers[34]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count0, &a5xx_coresight_registers[35]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count1, &a5xx_coresight_registers[36]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count2, &a5xx_coresight_registers[37]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count3, &a5xx_coresight_registers[38]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count4, &a5xx_coresight_registers[39]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count5, &a5xx_coresight_registers[40]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_addr,
				&a5xx_coresight_registers[41]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf0,
				&a5xx_coresight_registers[42]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf1,
				&a5xx_coresight_registers[43]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf2,
				&a5xx_coresight_registers[44]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf3,
				&a5xx_coresight_registers[45]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf4,
				&a5xx_coresight_registers[46]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_misr0, &a5xx_coresight_registers[47]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_misr1, &a5xx_coresight_registers[48]);
static ADRENO_CORESIGHT_ATTR(ahb_dbg_cntl, &a5xx_coresight_registers[49]);
static ADRENO_CORESIGHT_ATTR(read_ahb_through_dbg,
				&a5xx_coresight_registers[50]);
static ADRENO_CORESIGHT_ATTR(dbg_lo_hi_gpio, &a5xx_coresight_registers[51]);
static ADRENO_CORESIGHT_ATTR(ext_trace_bus_cntl, &a5xx_coresight_registers[52]);
static ADRENO_CORESIGHT_ATTR(ext_vbif_dbg_cntl, &a5xx_coresight_registers[53]);

static struct attribute *a5xx_coresight_attrs[] = {
	&coresight_attr_cfg_dbgbus_sel_a.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_b.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_c.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_d.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlt.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlm.attr.attr,
	&coresight_attr_cfg_dbgbus_opl.attr.attr,
	&coresight_attr_cfg_dbgbus_ope.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_0.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_3.attr.attr,
	&coresight_attr_cfg_dbgbus_nibblee.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc0.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc1.attr.attr,
	&coresight_attr_cfg_dbgbus_loadreg.attr.attr,
	&coresight_attr_cfg_dbgbus_idx.attr.attr,
	&coresight_attr_cfg_dbgbus_clrc.attr.attr,
	&coresight_attr_cfg_dbgbus_loadivt.attr.attr,
	&coresight_attr_cfg_dbgbus_event_logic.attr.attr,
	&coresight_attr_cfg_dbgbus_over.attr.attr,
	&coresight_attr_cfg_dbgbus_count0.attr.attr,
	&coresight_attr_cfg_dbgbus_count1.attr.attr,
	&coresight_attr_cfg_dbgbus_count2.attr.attr,
	&coresight_attr_cfg_dbgbus_count3.attr.attr,
	&coresight_attr_cfg_dbgbus_count4.attr.attr,
	&coresight_attr_cfg_dbgbus_count5.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_addr.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf0.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf1.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf2.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf3.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf4.attr.attr,
	&coresight_attr_cfg_dbgbus_misr0.attr.attr,
	&coresight_attr_cfg_dbgbus_misr1.attr.attr,
	&coresight_attr_ahb_dbg_cntl.attr.attr,
	&coresight_attr_read_ahb_through_dbg.attr.attr,
	&coresight_attr_dbg_lo_hi_gpio.attr.attr,
	&coresight_attr_ext_trace_bus_cntl.attr.attr,
	&coresight_attr_ext_vbif_dbg_cntl.attr.attr,
	NULL,
};

static const struct attribute_group a5xx_coresight_group = {
	.attrs = a5xx_coresight_attrs,
};

static const struct attribute_group *a5xx_coresight_groups[] = {
	&a5xx_coresight_group,
	NULL,
};

static struct adreno_coresight a5xx_coresight = {
	.registers = a5xx_coresight_registers,
	.count = ARRAY_SIZE(a5xx_coresight_registers),
	.groups = a5xx_coresight_groups,
};

struct adreno_gpudev adreno_a5xx_gpudev = {
	.reg_offsets = &a5xx_reg_offsets,
	.ft_perf_counters = a5xx_ft_perf_counters,
	.ft_perf_counters_count = ARRAY_SIZE(a5xx_ft_perf_counters),
	.coresight = &a5xx_coresight,
	.start = a5xx_start,
	.snapshot = a5xx_snapshot,
	.irq = &a5xx_irq,
	.snapshot_data = &a5xx_snapshot_data,
	.irq_trace = trace_kgsl_a5xx_irq_status,
	.num_prio_levels = 1,
	.gpudev_init = a5xx_gpudev_init,
	.rb_init = a5xx_rb_init,
	.microcode_read = a5xx_microcode_read,
	.microcode_load = a5xx_microcode_load,
	.perfcounters = &a5xx_perfcounters,
	.vbif_xin_halt_ctrl0_mask = A5XX_VBIF_XIN_HALT_CTRL0_MASK,
	.is_sptp_idle = a5xx_is_sptp_idle,
	.enable_pc = a5xx_enable_pc,
	.regulator_enable = a5xx_regulator_enable,
	.regulator_disable = a5xx_regulator_disable,
	.pwrlevel_change_settings = a5xx_pwrlevel_change_settings,
	.lm_init = a5xx_lm_init,
	.lm_enable = a5xx_lm_enable,
	.preemption_pre_ibsubmit = a5xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit =
				a5xx_preemption_post_ibsubmit,
	.preemption_token = a5xx_preemption_token,
	.preemption_start = a5xx_preemption_start,
	.preemption_save = a5xx_preemption_save,
	.preemption_init = a5xx_preemption_init,
	.gpmu_start = a5xx_gpmu_start,
};

