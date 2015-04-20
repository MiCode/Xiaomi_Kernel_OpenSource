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

#include "adreno.h"
#include "a5xx_reg.h"
#include "adreno_a3xx.h"
#include "adreno_a5xx.h"
#include "adreno_cp_parser.h"
#include "adreno_trace.h"
#include "adreno_pm4types.h"
#include "kgsl_sharedmem.h"
#include "kgsl_log.h"
#include "kgsl.h"

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

#define PWR_ON_BIT BIT(20)

/**
 * Number of times to check if the regulator enabled before
 * giving up and returning failure.
 */
#define PWR_RETRY 100

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
	struct adreno_gpudev *gpudev;

	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_is_a510(adreno_dev)) {
		gpudev->snapshot_data->sect_sizes->cp_meq = 32;
		gpudev->snapshot_data->sect_sizes->cp_merciu = 32;
		gpudev->snapshot_data->sect_sizes->roq = 256;
	}
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
	adreno_set_protected_registers(adreno_dev, &index, 0xE82, 1);
	adreno_set_protected_registers(adreno_dev, &index, 0xE87, 4);

	/* SMMU registers */
	iommu_regs = kgsl_mmu_get_prot_regs(&device->mmu);
	if (iommu_regs)
		adreno_set_protected_registers(adreno_dev, &index,
				iommu_regs->base, iommu_regs->range);
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
	unsigned int reg, retry = PWR_RETRY;
	struct kgsl_device *device = &adreno_dev->dev;
	if (!adreno_is_a530(adreno_dev))
		return 0;

	/* Set the default register values; set SW_COLLAPSE to 0 */
	kgsl_regwrite(device, A5XX_GPMU_SP_POWER_CNTL, 0x778000);
	/* Insert a delay between SPTP and RAC GDSC to avoid voltage droop */
	udelay(3);
	/*
	 * Poll the status register till the power-on bit is set or the max
	 * retries are exceeded.
	 */
	do {
		udelay(1);
		kgsl_regread(device, A5XX_GPMU_SP_PWR_CLK_STATUS, &reg);
	} while (!(reg & PWR_ON_BIT) && retry--);
	if (!(reg & PWR_ON_BIT)) {
		KGSL_PWR_ERR(device, "SPTP GDSC enable failed %x\n", reg);
		return -ENODEV;
	}

	kgsl_regwrite(device, A5XX_GPMU_RBCCU_POWER_CNTL, 0x778000);
	retry = PWR_RETRY;
	/*
	 * Poll the status register till the power-on bit is set or the max
	 * retries are exceeded.
	 */
	do {
		udelay(1);
		kgsl_regread(device, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, &reg);
	} while (!(reg & PWR_ON_BIT) && retry--);
	if (!(reg & PWR_ON_BIT)) {
		KGSL_PWR_ERR(device, "RBCCU GDSC enable failed %x\n", reg);
		return -ENODEV;
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
	unsigned int reg, retry = PWR_RETRY;
	struct kgsl_device *device = &adreno_dev->dev;
	if (!adreno_is_a530(adreno_dev))
		return;

	/* Set the default register values; set SW_COLLAPSE to 1 */
	kgsl_regwrite(device, A5XX_GPMU_SP_POWER_CNTL, 0x778001);
	/* Insert a delay between SPTP and RAC GDSC to avoid voltage droop */
	udelay(3);
	/*
	 * Poll the status register till the power-on bit is cleared or the max
	 * retries are exceeded.
	 */
	do {
		udelay(1);
		kgsl_regread(device, A5XX_GPMU_SP_PWR_CLK_STATUS, &reg);
	} while ((reg & PWR_ON_BIT) && retry--);
	if (reg & PWR_ON_BIT)
		KGSL_PWR_WARN(device, "SPTP GDSC disable failed %x\n", reg);

	kgsl_regwrite(device, A5XX_GPMU_RBCCU_POWER_CNTL, 0x778001);
	retry = PWR_RETRY;
	/*
	 * Poll the status register till the power-on bit is cleared or the max
	 * retries are exceeded.
	 */
	do {
		udelay(1);
		kgsl_regread(device, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, &reg);
	} while ((reg & PWR_ON_BIT) && retry--);
	if (reg & PWR_ON_BIT)
		KGSL_PWR_WARN(device, "RBCCU GDSC disable failed %x\n", reg);

	/* Reset VBIF before PC to avoid popping bogus FIFO entries */
	kgsl_regwrite(device, A5XX_RBBM_BLOCK_SW_RESET_CMD, 0x003C0000);
	kgsl_regwrite(device, A5XX_RBBM_BLOCK_SW_RESET_CMD, 0);
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

	/* Enable AHB error reporting */
	kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL1, 0xA6FFFFFF);
	kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL2, 0x0000003F);

	/*
	 * Turn on hang detection - this spews a lot of useful information
	 * into the RBBM registers on a hang
	 */
	kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_INT_CNTL, 0xFFFF);

	/* Turn on performance counters */
	kgsl_regwrite(device, A5XX_RBBM_PERFCTR_CNTL, 0x01);

	memset(&adreno_dev->busy_data, 0, sizeof(adreno_dev->busy_data));

	/*
	 * Set UCHE_WRITE_THRU_BASE to the UCHE_TRAP_BASE effectively
	 * disabling L2 bypass
	 */
	kgsl_regwrite(device, A5XX_UCHE_TRAP_BASE_LO, 0xffff0000);
	kgsl_regwrite(device, A5XX_UCHE_TRAP_BASE_HI, 0x0001ffff);
	kgsl_regwrite(device, A5XX_UCHE_WRITE_THRU_BASE_LO, 0xffff0000);
	kgsl_regwrite(device, A5XX_UCHE_WRITE_THRU_BASE_HI, 0x0001ffff);

	/* Enable flush of shared cachelines for each timer trigger of 10ms */
	kgsl_regwrite(device, A5XX_UCHE_SVM_CNTL, (1 << 16) | 0x2EE00);

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
			  struct kgsl_memdesc *ucode, size_t *ucode_size)
{
	struct kgsl_device *device = &adreno_dev->dev;
	const struct firmware *fw = NULL;
	int ret;

	ret = request_firmware(&fw, fwfile, device->dev);

	if (ret) {
		KGSL_DRV_FATAL(device, "request_firmware(%s) failed: %d\n",
				fwfile, ret);
		return ret;
	}

	ret = kgsl_allocate_global(device, ucode, fw->size,
				KGSL_MEMFLAGS_GPUREADONLY, 0);

	if (ret)
		return ret;

	memcpy(ucode->hostptr, &fw->data[4], fw->size - 4);
	*ucode_size = (fw->size - 4) / sizeof(uint32_t);

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
						 &adreno_dev->pm4_fw_size);
	if (ret)
		return ret;

	ret = _load_firmware(adreno_dev,
			 adreno_dev->gpucore->pfpfw_name, &adreno_dev->pfp,
						 &adreno_dev->pfp_fw_size);
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

	return 0;
}

static struct adreno_perfcount_register a5xx_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_0_LO,
		A5XX_RBBM_PERFCTR_CP_0_HI, 0, A5XX_CP_PERFCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_1_LO,
		A5XX_RBBM_PERFCTR_CP_1_HI, 0, A5XX_CP_PERFCTR_CP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_2_LO,
		A5XX_RBBM_PERFCTR_CP_2_HI, 0, A5XX_CP_PERFCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_3_LO,
		A5XX_RBBM_PERFCTR_CP_3_HI, 0, A5XX_CP_PERFCTR_CP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_4_LO,
		A5XX_RBBM_PERFCTR_CP_4_HI, 0, A5XX_CP_PERFCTR_CP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_5_LO,
		A5XX_RBBM_PERFCTR_CP_5_HI, 0, A5XX_CP_PERFCTR_CP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_6_LO,
		A5XX_RBBM_PERFCTR_CP_6_HI, 0, A5XX_CP_PERFCTR_CP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_7_LO,
		A5XX_RBBM_PERFCTR_CP_7_HI, 0, A5XX_CP_PERFCTR_CP_SEL_7 },
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

static struct adreno_perfcount_register a5xx_perfcounters_lrz[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_0_LO,
		A5XX_RBBM_PERFCTR_LRZ_0_HI, 48, A5XX_GRAS_PERFCTR_LRZ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_1_LO,
		A5XX_RBBM_PERFCTR_LRZ_1_HI, 49, A5XX_GRAS_PERFCTR_LRZ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_2_LO,
		A5XX_RBBM_PERFCTR_LRZ_2_HI, 50, A5XX_GRAS_PERFCTR_LRZ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_3_LO,
		A5XX_RBBM_PERFCTR_LRZ_3_HI, 51, A5XX_GRAS_PERFCTR_LRZ_SEL_3 },
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

static struct adreno_perfcount_register a5xx_perfcounters_cmp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_0_LO,
		A5XX_RBBM_PERFCTR_CMP_0_HI, 48, A5XX_RB_PERFCTR_CMP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_1_LO,
		A5XX_RBBM_PERFCTR_CMP_1_HI, 49, A5XX_RB_PERFCTR_CMP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_2_LO,
		A5XX_RBBM_PERFCTR_CMP_2_HI, 50, A5XX_RB_PERFCTR_CMP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_3_LO,
		A5XX_RBBM_PERFCTR_CMP_3_HI, 51, A5XX_RB_PERFCTR_CMP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vsc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VSC_0_LO,
		A5XX_RBBM_PERFCTR_VSC_0_HI, 88, A5XX_VSC_PERFCTR_VSC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VSC_1_LO,
		A5XX_RBBM_PERFCTR_VSC_1_HI, 89, A5XX_VSC_PERFCTR_VSC_SEL_1 },
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
		A5XX_RBBM_ALWAYSON_COUNTER_HI, 0 },
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

#define A5XX_PERFCOUNTER_GROUP(offset, name) \
	ADRENO_PERFCOUNTER_GROUP(a5xx, offset, name)

#define A5XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a5xx, offset, name, flags)

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
	case A5XX_INT_RBBM_GPC_ERROR:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: GPC error\n");
		break;
	case A5XX_INT_CP_HW_ERROR:
		kgsl_regread(device, A5XX_CP_HW_FAULT, &reg);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP | Ringbuffer HW fault | status=%x\n", reg);
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
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 7 - GPC_ERR */
	ADRENO_IRQ_CALLBACK(NULL),				/* 8 - CP_SW */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 9 - CP_HW_ERROR */
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

struct adreno_gpudev adreno_a5xx_gpudev = {
	.reg_offsets = &a5xx_reg_offsets,
	.ft_perf_counters = a5xx_ft_perf_counters,
	.ft_perf_counters_count = ARRAY_SIZE(a5xx_ft_perf_counters),
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
	.regulator_enable = a5xx_regulator_enable,
	.regulator_disable = a5xx_regulator_disable,
	.preemption_pre_ibsubmit = a5xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit =
				a5xx_preemption_post_ibsubmit,
	.preemption_token = a5xx_preemption_token,
	.preemption_start = a5xx_preemption_start,
	.preemption_save = a5xx_preemption_save,
	.preemption_init = a5xx_preemption_init,
};

