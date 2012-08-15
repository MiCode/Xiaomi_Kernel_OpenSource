/* Copyright (c) 2002,2007-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <mach/socinfo.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include <mach/msm_dcvs.h>
#include <mach/msm_dcvs_scm.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_cffdump.h"
#include "kgsl_sharedmem.h"
#include "kgsl_iommu.h"

#include "adreno.h"
#include "adreno_pm4types.h"

#include "a2xx_reg.h"
#include "a3xx_reg.h"

#define DRIVER_VERSION_MAJOR   3
#define DRIVER_VERSION_MINOR   1

/* Adreno MH arbiter config*/
#define ADRENO_CFG_MHARB \
	(0x10 \
		| (0 << MH_ARBITER_CONFIG__SAME_PAGE_GRANULARITY__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__L2_ARB_CONTROL__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PAGE_SIZE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_REORDER_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT_ENABLE__SHIFT) \
		| (0x8 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__CP_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__VGT_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__RB_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PA_CLNT_ENABLE__SHIFT))

#define ADRENO_MMU_CONFIG						\
	(0x01								\
	 | (MMU_CONFIG << MH_MMU_CONFIG__RB_W_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_W_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R0_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R1_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R2_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R3_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R4_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__VGT_R0_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__VGT_R1_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__TC_R_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__PA_W_CLNT_BEHAVIOR__SHIFT))

static const struct kgsl_functable adreno_functable;

static struct adreno_device device_3d0 = {
	.dev = {
		KGSL_DEVICE_COMMON_INIT(device_3d0.dev),
		.name = DEVICE_3D0_NAME,
		.id = KGSL_DEVICE_3D0,
		.mh = {
			.mharb  = ADRENO_CFG_MHARB,
			/* Remove 1k boundary check in z470 to avoid a GPU
			 * hang.  Notice that this solution won't work if
			 * both EBI and SMI are used
			 */
			.mh_intf_cfg1 = 0x00032f07,
			/* turn off memory protection unit by setting
			   acceptable physical address range to include
			   all pages. */
			.mpu_base = 0x00000000,
			.mpu_range =  0xFFFFF000,
		},
		.mmu = {
			.config = ADRENO_MMU_CONFIG,
		},
		.pwrctrl = {
			.irq_name = KGSL_3D0_IRQ,
		},
		.iomemname = KGSL_3D0_REG_MEMORY,
		.ftbl = &adreno_functable,
#ifdef CONFIG_HAS_EARLYSUSPEND
		.display_off = {
			.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING,
			.suspend = kgsl_early_suspend_driver,
			.resume = kgsl_late_resume_driver,
		},
#endif
	},
	.gmem_base = 0,
	.gmem_size = SZ_256K,
	.pfp_fw = NULL,
	.pm4_fw = NULL,
	.wait_timeout = 0, /* in milliseconds, 0 means disabled */
	.ib_check_level = 0,
};

/* This set of registers are used for Hang detection
 * If the values of these registers are same after
 * KGSL_TIMEOUT_PART time, GPU hang is reported in
 * kernel log.
 */
unsigned int hang_detect_regs[] = {
	A3XX_RBBM_STATUS,
	REG_CP_RB_RPTR,
	REG_CP_IB1_BASE,
	REG_CP_IB1_BUFSZ,
	REG_CP_IB2_BASE,
	REG_CP_IB2_BUFSZ,
};

const unsigned int hang_detect_regs_count = ARRAY_SIZE(hang_detect_regs);

/*
 * This is the master list of all GPU cores that are supported by this
 * driver.
 */

#define ANY_ID (~0)

static const struct {
	enum adreno_gpurev gpurev;
	unsigned int core, major, minor, patchid;
	const char *pm4fw;
	const char *pfpfw;
	struct adreno_gpudev *gpudev;
	unsigned int istore_size;
	unsigned int pix_shader_start;
	unsigned int instruction_size; /* Size of an instruction in dwords */
	unsigned int gmem_size; /* size of gmem for gpu*/
} adreno_gpulist[] = {
	{ ADRENO_REV_A200, 0, 2, ANY_ID, ANY_ID,
		"yamato_pm4.fw", "yamato_pfp.fw", &adreno_a2xx_gpudev,
		512, 384, 3, SZ_256K },
	{ ADRENO_REV_A203, 0, 1, 1, ANY_ID,
		"yamato_pm4.fw", "yamato_pfp.fw", &adreno_a2xx_gpudev,
		512, 384, 3, SZ_256K },
	{ ADRENO_REV_A205, 0, 1, 0, ANY_ID,
		"yamato_pm4.fw", "yamato_pfp.fw", &adreno_a2xx_gpudev,
		512, 384, 3, SZ_256K },
	{ ADRENO_REV_A220, 2, 1, ANY_ID, ANY_ID,
		"leia_pm4_470.fw", "leia_pfp_470.fw", &adreno_a2xx_gpudev,
		512, 384, 3, SZ_512K },
	/*
	 * patchlevel 5 (8960v2) needs special pm4 firmware to work around
	 * a hardware problem.
	 */
	{ ADRENO_REV_A225, 2, 2, 0, 5,
		"a225p5_pm4.fw", "a225_pfp.fw", &adreno_a2xx_gpudev,
		1536, 768, 3, SZ_512K },
	{ ADRENO_REV_A225, 2, 2, 0, 6,
		"a225_pm4.fw", "a225_pfp.fw", &adreno_a2xx_gpudev,
		1536, 768, 3, SZ_512K },
	{ ADRENO_REV_A225, 2, 2, ANY_ID, ANY_ID,
		"a225_pm4.fw", "a225_pfp.fw", &adreno_a2xx_gpudev,
		1536, 768, 3, SZ_512K },
	/* A3XX doesn't use the pix_shader_start */
	{ ADRENO_REV_A305, 3, 0, 5, ANY_ID,
		"a300_pm4.fw", "a300_pfp.fw", &adreno_a3xx_gpudev,
		512, 0, 2, SZ_256K },
	/* A3XX doesn't use the pix_shader_start */
	{ ADRENO_REV_A320, 3, 2, 0, ANY_ID,
		"a300_pm4.fw", "a300_pfp.fw", &adreno_a3xx_gpudev,
		512, 0, 2, SZ_512K },
	{ ADRENO_REV_A330, 3, 3, 0, 0,
		"a330_pm4.fw", "a330_pfp.fw", &adreno_a3xx_gpudev,
		512, 0, 2, SZ_1M },
};

static irqreturn_t adreno_irq_handler(struct kgsl_device *device)
{
	irqreturn_t result;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	result = adreno_dev->gpudev->irq_handler(adreno_dev);

	if (device->requested_state == KGSL_STATE_NONE) {
		if (device->pwrctrl.nap_allowed == true) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NAP);
			queue_work(device->work_queue, &device->idle_check_ws);
		} else if (device->pwrscale.policy != NULL) {
			queue_work(device->work_queue, &device->idle_check_ws);
		}
	}

	/* Reset the time-out in our idle timer */
	mod_timer_pending(&device->idle_timer,
		jiffies + device->pwrctrl.interval_timeout);
	return result;
}

static void adreno_cleanup_pt(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

	kgsl_mmu_unmap(pagetable, &rb->buffer_desc);

	kgsl_mmu_unmap(pagetable, &rb->memptrs_desc);

	kgsl_mmu_unmap(pagetable, &device->memstore);

	kgsl_mmu_unmap(pagetable, &device->mmu.setstate_memory);
}

static int adreno_setup_pt(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable)
{
	int result = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

	result = kgsl_mmu_map_global(pagetable, &rb->buffer_desc,
				     GSL_PT_PAGE_RV);
	if (result)
		goto error;

	result = kgsl_mmu_map_global(pagetable, &rb->memptrs_desc,
				     GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);
	if (result)
		goto unmap_buffer_desc;

	result = kgsl_mmu_map_global(pagetable, &device->memstore,
				     GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);
	if (result)
		goto unmap_memptrs_desc;

	result = kgsl_mmu_map_global(pagetable, &device->mmu.setstate_memory,
				     GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);
	if (result)
		goto unmap_memstore_desc;

	return result;

unmap_memstore_desc:
	kgsl_mmu_unmap(pagetable, &device->memstore);

unmap_memptrs_desc:
	kgsl_mmu_unmap(pagetable, &rb->memptrs_desc);

unmap_buffer_desc:
	kgsl_mmu_unmap(pagetable, &rb->buffer_desc);

error:
	return result;
}

static void adreno_iommu_setstate(struct kgsl_device *device,
					unsigned int context_id,
					uint32_t flags)
{
	unsigned int pt_val, reg_pt_val;
	unsigned int link[200];
	unsigned int *cmds = &link[0];
	int sizedwords = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_memdesc **reg_map_desc;
	void *reg_map_array = NULL;
	int num_iommu_units, i;
	struct kgsl_context *context;
	struct adreno_context *adreno_ctx = NULL;

	if (!adreno_dev->drawctxt_active)
		return kgsl_mmu_device_setstate(&device->mmu, flags);
	num_iommu_units = kgsl_mmu_get_reg_map_desc(&device->mmu,
							&reg_map_array);

	context = idr_find(&device->context_idr, context_id);
	adreno_ctx = context->devctxt;

	reg_map_desc = reg_map_array;

	if (kgsl_mmu_enable_clk(&device->mmu,
				KGSL_IOMMU_CONTEXT_USER))
		goto done;

	cmds += __adreno_add_idle_indirect_cmds(cmds,
		device->mmu.setstate_memory.gpuaddr +
		KGSL_IOMMU_SETSTATE_NOP_OFFSET);

	if (cpu_is_msm8960())
		cmds += adreno_add_change_mh_phys_limit_cmds(cmds, 0xFFFFF000,
					device->mmu.setstate_memory.gpuaddr +
					KGSL_IOMMU_SETSTATE_NOP_OFFSET);
	else
		cmds += adreno_add_bank_change_cmds(cmds,
					KGSL_IOMMU_CONTEXT_USER,
					device->mmu.setstate_memory.gpuaddr +
					KGSL_IOMMU_SETSTATE_NOP_OFFSET);

	pt_val = kgsl_mmu_pt_get_base_addr(device->mmu.hwpagetable);
	if (flags & KGSL_MMUFLAGS_PTUPDATE) {
		/*
		 * We need to perfrom the following operations for all
		 * IOMMU units
		 */
		for (i = 0; i < num_iommu_units; i++) {
			reg_pt_val = (pt_val &
				(KGSL_IOMMU_TTBR0_PA_MASK <<
				KGSL_IOMMU_TTBR0_PA_SHIFT)) +
				kgsl_mmu_get_pt_lsb(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER);
			/*
			 * Set address of the new pagetable by writng to IOMMU
			 * TTBR0 register
			 */
			*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
			*cmds++ = reg_map_desc[i]->gpuaddr +
				(KGSL_IOMMU_CONTEXT_USER <<
				KGSL_IOMMU_CTX_SHIFT) + KGSL_IOMMU_TTBR0;
			*cmds++ = reg_pt_val;
			*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
			*cmds++ = 0x00000000;

			/*
			 * Read back the ttbr0 register as a barrier to ensure
			 * above writes have completed
			 */
			cmds += adreno_add_read_cmds(device, cmds,
				reg_map_desc[i]->gpuaddr +
				(KGSL_IOMMU_CONTEXT_USER <<
				KGSL_IOMMU_CTX_SHIFT) + KGSL_IOMMU_TTBR0,
				reg_pt_val,
				device->mmu.setstate_memory.gpuaddr +
				KGSL_IOMMU_SETSTATE_NOP_OFFSET);
		}
		/* invalidate all base pointers */
		*cmds++ = cp_type3_packet(CP_INVALIDATE_STATE, 1);
		*cmds++ = 0x7fff;

		cmds += __adreno_add_idle_indirect_cmds(cmds,
			device->mmu.setstate_memory.gpuaddr +
			KGSL_IOMMU_SETSTATE_NOP_OFFSET);
	}
	if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
		/*
		 * tlb flush
		 */
		for (i = 0; i < num_iommu_units; i++) {
			reg_pt_val = (pt_val &
				(KGSL_IOMMU_TTBR0_PA_MASK <<
				KGSL_IOMMU_TTBR0_PA_SHIFT)) +
				kgsl_mmu_get_pt_lsb(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER);

			*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
			*cmds++ = (reg_map_desc[i]->gpuaddr +
				(KGSL_IOMMU_CONTEXT_USER <<
				KGSL_IOMMU_CTX_SHIFT) +
				KGSL_IOMMU_CTX_TLBIALL);
			*cmds++ = 1;

			cmds += __adreno_add_idle_indirect_cmds(cmds,
			device->mmu.setstate_memory.gpuaddr +
			KGSL_IOMMU_SETSTATE_NOP_OFFSET);

			cmds += adreno_add_read_cmds(device, cmds,
				reg_map_desc[i]->gpuaddr +
				(KGSL_IOMMU_CONTEXT_USER <<
				KGSL_IOMMU_CTX_SHIFT) + KGSL_IOMMU_TTBR0,
				reg_pt_val,
				device->mmu.setstate_memory.gpuaddr +
				KGSL_IOMMU_SETSTATE_NOP_OFFSET);
		}
	}

	if (cpu_is_msm8960())
		cmds += adreno_add_change_mh_phys_limit_cmds(cmds,
			reg_map_desc[num_iommu_units - 1]->gpuaddr - PAGE_SIZE,
			device->mmu.setstate_memory.gpuaddr +
			KGSL_IOMMU_SETSTATE_NOP_OFFSET);
	else
		cmds += adreno_add_bank_change_cmds(cmds,
			KGSL_IOMMU_CONTEXT_PRIV,
			device->mmu.setstate_memory.gpuaddr +
			KGSL_IOMMU_SETSTATE_NOP_OFFSET);

	sizedwords += (cmds - &link[0]);
	if (sizedwords) {
		/*
		 * add an interrupt at the end of commands so that the smmu
		 * disable clock off function will get called
		 */
		*cmds++ = cp_type3_packet(CP_INTERRUPT, 1);
		*cmds++ = CP_INT_CNTL__RB_INT_MASK;
		sizedwords += 2;
		/* This returns the per context timestamp but we need to
		 * use the global timestamp for iommu clock disablement */
		adreno_ringbuffer_issuecmds(device, adreno_ctx,
			KGSL_CMD_FLAGS_PMODE,
			&link[0], sizedwords);
		kgsl_mmu_disable_clk_on_ts(&device->mmu,
		adreno_dev->ringbuffer.timestamp[KGSL_MEMSTORE_GLOBAL], true);
	}
done:
	if (num_iommu_units)
		kfree(reg_map_array);
}

static void adreno_gpummu_setstate(struct kgsl_device *device,
					unsigned int context_id,
					uint32_t flags)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int link[32];
	unsigned int *cmds = &link[0];
	int sizedwords = 0;
	unsigned int mh_mmu_invalidate = 0x00000003; /*invalidate all and tc */
	struct kgsl_context *context;
	struct adreno_context *adreno_ctx = NULL;

	/*
	 * Fix target freeze issue by adding TLB flush for each submit
	 * on A20X based targets.
	 */
	if (adreno_is_a20x(adreno_dev))
		flags |= KGSL_MMUFLAGS_TLBFLUSH;
	/*
	 * If possible, then set the state via the command stream to avoid
	 * a CPU idle.  Otherwise, use the default setstate which uses register
	 * writes For CFF dump we must idle and use the registers so that it is
	 * easier to filter out the mmu accesses from the dump
	 */
	if (!kgsl_cff_dump_enable && adreno_dev->drawctxt_active) {
		context = idr_find(&device->context_idr, context_id);
		adreno_ctx = context->devctxt;

		if (flags & KGSL_MMUFLAGS_PTUPDATE) {
			/* wait for graphics pipe to be idle */
			*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
			*cmds++ = 0x00000000;

			/* set page table base */
			*cmds++ = cp_type0_packet(MH_MMU_PT_BASE, 1);
			*cmds++ = kgsl_mmu_pt_get_base_addr(
					device->mmu.hwpagetable);
			sizedwords += 4;
		}

		if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
			if (!(flags & KGSL_MMUFLAGS_PTUPDATE)) {
				*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE,
								1);
				*cmds++ = 0x00000000;
				sizedwords += 2;
			}
			*cmds++ = cp_type0_packet(MH_MMU_INVALIDATE, 1);
			*cmds++ = mh_mmu_invalidate;
			sizedwords += 2;
		}

		if (flags & KGSL_MMUFLAGS_PTUPDATE &&
			adreno_is_a20x(adreno_dev)) {
			/* HW workaround: to resolve MMU page fault interrupts
			* caused by the VGT.It prevents the CP PFP from filling
			* the VGT DMA request fifo too early,thereby ensuring
			* that the VGT will not fetch vertex/bin data until
			* after the page table base register has been updated.
			*
			* Two null DRAW_INDX_BIN packets are inserted right
			* after the page table base update, followed by a
			* wait for idle. The null packets will fill up the
			* VGT DMA request fifo and prevent any further
			* vertex/bin updates from occurring until the wait
			* has finished. */
			*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
			*cmds++ = (0x4 << 16) |
				(REG_PA_SU_SC_MODE_CNTL - 0x2000);
			*cmds++ = 0;	  /* disable faceness generation */
			*cmds++ = cp_type3_packet(CP_SET_BIN_BASE_OFFSET, 1);
			*cmds++ = device->mmu.setstate_memory.gpuaddr;
			*cmds++ = cp_type3_packet(CP_DRAW_INDX_BIN, 6);
			*cmds++ = 0;	  /* viz query info */
			*cmds++ = 0x0003C004; /* draw indicator */
			*cmds++ = 0;	  /* bin base */
			*cmds++ = 3;	  /* bin size */
			*cmds++ =
			device->mmu.setstate_memory.gpuaddr; /* dma base */
			*cmds++ = 6;	  /* dma size */
			*cmds++ = cp_type3_packet(CP_DRAW_INDX_BIN, 6);
			*cmds++ = 0;	  /* viz query info */
			*cmds++ = 0x0003C004; /* draw indicator */
			*cmds++ = 0;	  /* bin base */
			*cmds++ = 3;	  /* bin size */
			/* dma base */
			*cmds++ = device->mmu.setstate_memory.gpuaddr;
			*cmds++ = 6;	  /* dma size */
			*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
			*cmds++ = 0x00000000;
			sizedwords += 21;
		}


		if (flags & (KGSL_MMUFLAGS_PTUPDATE | KGSL_MMUFLAGS_TLBFLUSH)) {
			*cmds++ = cp_type3_packet(CP_INVALIDATE_STATE, 1);
			*cmds++ = 0x7fff; /* invalidate all base pointers */
			sizedwords += 2;
		}

		adreno_ringbuffer_issuecmds(device, adreno_ctx,
					KGSL_CMD_FLAGS_PMODE,
					&link[0], sizedwords);
	} else {
		kgsl_mmu_device_setstate(&device->mmu, flags);
	}
}

static void adreno_setstate(struct kgsl_device *device,
			unsigned int context_id,
			uint32_t flags)
{
	/* call the mmu specific handler */
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_get_mmutype())
		return adreno_gpummu_setstate(device, context_id, flags);
	else if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype())
		return adreno_iommu_setstate(device, context_id, flags);
}

static unsigned int
a3xx_getchipid(struct kgsl_device *device)
{
	struct kgsl_device_platform_data *pdata =
		kgsl_device_get_drvdata(device);

	/*
	 * All current A3XX chipids are detected at the SOC level. Leave this
	 * function here to support any future GPUs that have working
	 * chip ID registers
	 */

	return pdata->chipid;
}

static unsigned int
a2xx_getchipid(struct kgsl_device *device)
{
	unsigned int chipid = 0;
	unsigned int coreid, majorid, minorid, patchid, revid;
	struct kgsl_device_platform_data *pdata =
		kgsl_device_get_drvdata(device);

	/* If the chip id is set at the platform level, then just use that */

	if (pdata->chipid != 0)
		return pdata->chipid;

	adreno_regread(device, REG_RBBM_PERIPHID1, &coreid);
	adreno_regread(device, REG_RBBM_PERIPHID2, &majorid);
	adreno_regread(device, REG_RBBM_PATCH_RELEASE, &revid);

	/*
	* adreno 22x gpus are indicated by coreid 2,
	* but REG_RBBM_PERIPHID1 always contains 0 for this field
	*/
	if (cpu_is_msm8x60())
		chipid = 2 << 24;
	else
		chipid = (coreid & 0xF) << 24;

	chipid |= ((majorid >> 4) & 0xF) << 16;

	minorid = ((revid >> 0)  & 0xFF);

	patchid = ((revid >> 16) & 0xFF);

	/* 8x50 returns 0 for patch release, but it should be 1 */
	/* 8x25 returns 0 for minor id, but it should be 1 */
	if (cpu_is_qsd8x50())
		patchid = 1;
	else if (cpu_is_msm8625() && minorid == 0)
		minorid = 1;

	chipid |= (minorid << 8) | patchid;

	return chipid;
}

static unsigned int
adreno_getchipid(struct kgsl_device *device)
{
	struct kgsl_device_platform_data *pdata =
		kgsl_device_get_drvdata(device);

	/*
	 * All A3XX chipsets will have pdata set, so assume !pdata->chipid is
	 * an A2XX processor
	 */

	if (pdata->chipid == 0 || ADRENO_CHIPID_MAJOR(pdata->chipid) == 2)
		return a2xx_getchipid(device);
	else
		return a3xx_getchipid(device);
}

static inline bool _rev_match(unsigned int id, unsigned int entry)
{
	return (entry == ANY_ID || entry == id);
}

static void
adreno_identify_gpu(struct adreno_device *adreno_dev)
{
	unsigned int i, core, major, minor, patchid;

	adreno_dev->chip_id = adreno_getchipid(&adreno_dev->dev);

	core = ADRENO_CHIPID_CORE(adreno_dev->chip_id);
	major = ADRENO_CHIPID_MAJOR(adreno_dev->chip_id);
	minor = ADRENO_CHIPID_MINOR(adreno_dev->chip_id);
	patchid = ADRENO_CHIPID_PATCH(adreno_dev->chip_id);

	for (i = 0; i < ARRAY_SIZE(adreno_gpulist); i++) {
		if (core == adreno_gpulist[i].core &&
		    _rev_match(major, adreno_gpulist[i].major) &&
		    _rev_match(minor, adreno_gpulist[i].minor) &&
		    _rev_match(patchid, adreno_gpulist[i].patchid))
			break;
	}

	if (i == ARRAY_SIZE(adreno_gpulist)) {
		adreno_dev->gpurev = ADRENO_REV_UNKNOWN;
		return;
	}

	adreno_dev->gpurev = adreno_gpulist[i].gpurev;
	adreno_dev->gpudev = adreno_gpulist[i].gpudev;
	adreno_dev->pfp_fwfile = adreno_gpulist[i].pfpfw;
	adreno_dev->pm4_fwfile = adreno_gpulist[i].pm4fw;
	adreno_dev->istore_size = adreno_gpulist[i].istore_size;
	adreno_dev->pix_shader_start = adreno_gpulist[i].pix_shader_start;
	adreno_dev->instruction_size = adreno_gpulist[i].instruction_size;
	adreno_dev->gmem_size = adreno_gpulist[i].gmem_size;
}

static struct platform_device_id adreno_id_table[] = {
	{ DEVICE_3D0_NAME, (kernel_ulong_t)&device_3d0.dev, },
	{},
};

MODULE_DEVICE_TABLE(platform, adreno_id_table);

static struct of_device_id adreno_match_table[] = {
	{ .compatible = "qcom,kgsl-3d0", },
	{}
};

static inline int adreno_of_read_property(struct device_node *node,
	const char *prop, unsigned int *ptr)
{
	int ret = of_property_read_u32(node, prop, ptr);
	if (ret)
		KGSL_CORE_ERR("Unable to read '%s'\n", prop);
	return ret;
}

static struct device_node *adreno_of_find_subnode(struct device_node *parent,
	const char *name)
{
	struct device_node *child;

	for_each_child_of_node(parent, child) {
		if (of_device_is_compatible(child, name))
			return child;
	}

	return NULL;
}

static int adreno_of_get_pwrlevels(struct device_node *parent,
	struct kgsl_device_platform_data *pdata)
{
	struct device_node *node, *child;
	int ret = -EINVAL;

	node = adreno_of_find_subnode(parent, "qcom,gpu-pwrlevels");

	if (node == NULL) {
		KGSL_CORE_ERR("Unable to find 'qcom,gpu-pwrlevels'\n");
		return -EINVAL;
	}

	pdata->num_levels = 0;

	for_each_child_of_node(node, child) {
		unsigned int index;
		struct kgsl_pwrlevel *level;

		if (adreno_of_read_property(child, "reg", &index))
			goto done;

		if (index >= KGSL_MAX_PWRLEVELS) {
			KGSL_CORE_ERR("Pwrlevel index %d is out of range\n",
				index);
			continue;
		}

		if (index >= pdata->num_levels)
			pdata->num_levels = index + 1;

		level = &pdata->pwrlevel[index];

		if (adreno_of_read_property(child, "qcom,gpu-freq",
			&level->gpu_freq))
			goto done;

		if (adreno_of_read_property(child, "qcom,bus-freq",
			&level->bus_freq))
			goto done;

		if (adreno_of_read_property(child, "qcom,io-fraction",
			&level->io_fraction))
			level->io_fraction = 0;
	}

	if (adreno_of_read_property(parent, "qcom,initial-pwrlevel",
		&pdata->init_level))
		pdata->init_level = 1;

	if (pdata->init_level < 0 || pdata->init_level > pdata->num_levels) {
		KGSL_CORE_ERR("Initial power level out of range\n");
		pdata->init_level = 1;
	}

	ret = 0;
done:
	return ret;

}
static void adreno_of_free_bus_scale_info(struct msm_bus_scale_pdata *pdata)
{
	int i;

	if (pdata == NULL)
		return;

	for (i = 0;  pdata->usecase && i < pdata->num_usecases; i++)
		kfree(pdata->usecase[i].vectors);

	kfree(pdata->usecase);
	kfree(pdata);
}

struct msm_bus_scale_pdata *adreno_of_get_bus_scale(struct device_node *node)
{
	static int bus_vectors_src[3] = {MSM_BUS_MASTER_GRAPHICS_3D,
		MSM_BUS_MASTER_GRAPHICS_3D_PORT1, MSM_BUS_MASTER_V_OCMEM_GFX3D};
	static int bus_vectors_dst[2] = {MSM_BUS_SLAVE_EBI_CH0,
		MSM_BUS_SLAVE_OCMEM};
	const unsigned int *vectors;
	struct msm_bus_scale_pdata *pdata;
	int i, j, len, num_paths;
	int ret = -EINVAL;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);

	if (!pdata) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n", sizeof(*pdata));
		return ERR_PTR(-ENOMEM);
	}

	if (adreno_of_read_property(node, "qcom,grp3d-num-bus-scale-usecases",
		&pdata->num_usecases)) {
		pdata->num_usecases = 0;
		goto err;
	}

	pdata->usecase =  kzalloc(pdata->num_usecases *
		sizeof(struct msm_bus_paths), GFP_KERNEL);

	if (pdata->usecase == NULL) {
		KGSL_CORE_ERR("kzalloc (%d) failed\n",
			pdata->num_usecases * sizeof(struct msm_bus_paths));
		ret = -ENOMEM;
		goto err;
	}

	if (adreno_of_read_property(node, "qcom,grp3d-num-vectors-per-usecase",
		&num_paths))
		goto err;

	vectors = of_get_property(node, "qcom,grp3d-vectors", &len);

	if (len != pdata->num_usecases * num_paths *
		sizeof(struct msm_bus_vectors)) {
		KGSL_CORE_ERR("Invalid size for the bus scale vectors\n");
		goto err;
	}

	for (i = 0; i < pdata->num_usecases; i++) {
		pdata->usecase[i].num_paths = num_paths;
		pdata->usecase[i].vectors = kzalloc(num_paths *
						sizeof(struct msm_bus_vectors),
						GFP_KERNEL);
		if (!pdata->usecase[i].vectors) {
			KGSL_CORE_ERR("kzalloc(%d) failed\n",
				num_paths * sizeof(struct msm_bus_vectors));
			ret = -ENOMEM;
			goto err;
		}
		for (j = 0; j < num_paths; j++) {
			int index = (i * num_paths + j) * 4;
			pdata->usecase[i].vectors[j].src =
				bus_vectors_src[be32_to_cpu(vectors[index])];
			pdata->usecase[i].vectors[j].dst =
				bus_vectors_dst[
					be32_to_cpu(vectors[index + 1])];
			pdata->usecase[i].vectors[j].ab =
				be32_to_cpu(vectors[index + 2]);
			pdata->usecase[i].vectors[j].ib =
				KGSL_CONVERT_TO_MBPS(
					be32_to_cpu(vectors[index + 3]));
		}
	}

	pdata->name = "grp3d";

	return pdata;

err:
	adreno_of_free_bus_scale_info(pdata);

	return ERR_PTR(ret);
}

static struct msm_dcvs_core_info *adreno_of_get_dcvs(struct device_node *parent)
{
	struct device_node *node, *child;
	struct msm_dcvs_core_info *info = NULL;
	int count = 0;
	int ret = -EINVAL;

	node = adreno_of_find_subnode(parent, "qcom,dcvs-core-info");
	if (node == NULL)
		return ERR_PTR(-EINVAL);

	info = kzalloc(sizeof(*info), GFP_KERNEL);

	if (info == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n", sizeof(*info));
		ret = -ENOMEM;
		goto err;
	}

	for_each_child_of_node(node, child)
		count++;

	info->core_param.num_freq = count;

	info->freq_tbl = kzalloc(info->core_param.num_freq *
			sizeof(struct msm_dcvs_freq_entry),
			GFP_KERNEL);

	if (info->freq_tbl == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			info->core_param.num_freq *
			sizeof(struct msm_dcvs_freq_entry));
		ret = -ENOMEM;
		goto err;
	}

	for_each_child_of_node(node, child) {
		unsigned int index;

		if (adreno_of_read_property(child, "reg", &index))
			goto err;

		if (index >= info->core_param.num_freq) {
			KGSL_CORE_ERR("DCVS freq entry %d is out of range\n",
				index);
			continue;
		}

		if (adreno_of_read_property(child, "qcom,freq",
			&info->freq_tbl[index].freq))
			goto err;

		if (adreno_of_read_property(child, "qcom,idle-energy",
			&info->freq_tbl[index].idle_energy))
			info->freq_tbl[index].idle_energy = 0;

		if (adreno_of_read_property(child, "qcom,active-energy",
			&info->freq_tbl[index].active_energy))
			info->freq_tbl[index].active_energy = 0;
	}

	if (adreno_of_read_property(node, "qcom,core-max-time-us",
		&info->core_param.max_time_us))
		goto err;

	if (adreno_of_read_property(node, "qcom,algo-slack-time-us",
		&info->algo_param.slack_time_us))
		goto err;

	if (adreno_of_read_property(node, "qcom,algo-disable-pc-threshold",
		&info->algo_param.disable_pc_threshold))
		goto err;

	if (adreno_of_read_property(node, "qcom,algo-ss-window-size",
		&info->algo_param.ss_window_size))
		goto err;

	if (adreno_of_read_property(node, "qcom,algo-ss-util-pct",
		&info->algo_param.ss_util_pct))
		goto err;

	if (adreno_of_read_property(node, "qcom,algo-em-max-util-pct",
		&info->algo_param.em_max_util_pct))
		goto err;

	if (adreno_of_read_property(node, "qcom,algo-ss-iobusy-conv",
		&info->algo_param.ss_iobusy_conv))
		goto err;

	return info;

err:
	if (info)
		kfree(info->freq_tbl);

	kfree(info);

	return ERR_PTR(ret);
}

static int adreno_of_get_iommu(struct device_node *parent,
	struct kgsl_device_platform_data *pdata)
{
	struct device_node *node, *child;
	struct kgsl_device_iommu_data *data = NULL;
	struct kgsl_iommu_ctx *ctxs = NULL;
	u32 reg_val[2];
	int ctx_index = 0;

	node = of_parse_phandle(parent, "iommu", 0);
	if (node == NULL)
		return -EINVAL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n", sizeof(*data));
		goto err;
	}

	if (of_property_read_u32_array(node, "reg", reg_val, 2))
		goto err;

	data->physstart = reg_val[0];
	data->physend = data->physstart + reg_val[1] - 1;

	data->iommu_ctx_count = 0;

	for_each_child_of_node(node, child)
		data->iommu_ctx_count++;

	ctxs = kzalloc(data->iommu_ctx_count * sizeof(struct kgsl_iommu_ctx),
		GFP_KERNEL);

	if (ctxs == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			data->iommu_ctx_count * sizeof(struct kgsl_iommu_ctx));
		goto err;
	}

	for_each_child_of_node(node, child) {
		int ret = of_property_read_string(child, "label",
				&ctxs[ctx_index].iommu_ctx_name);

		if (ret) {
			KGSL_CORE_ERR("Unable to read KGSL IOMMU 'label'\n");
			goto err;
		}

		if (adreno_of_read_property(child, "qcom,iommu-ctx-sids",
			&ctxs[ctx_index].ctx_id))
			goto err;

		ctx_index++;
	}

	data->iommu_ctxs = ctxs;

	pdata->iommu_data = data;
	pdata->iommu_count = 1;

	return 0;

err:
	kfree(ctxs);
	kfree(data);

	return -EINVAL;
}

static int adreno_of_get_pdata(struct platform_device *pdev)
{
	struct kgsl_device_platform_data *pdata = NULL;
	struct kgsl_device *device;
	int ret = -EINVAL;

	pdev->id_entry = adreno_id_table;

	pdata = pdev->dev.platform_data;
	if (pdata)
		return 0;

	if (of_property_read_string(pdev->dev.of_node, "label", &pdev->name)) {
		KGSL_CORE_ERR("Unable to read 'label'\n");
		goto err;
	}

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,id", &pdev->id))
		goto err;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n", sizeof(*pdata));
		ret = -ENOMEM;
		goto err;
	}

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,chipid",
		&pdata->chipid))
		goto err;

	/* pwrlevel Data */
	ret = adreno_of_get_pwrlevels(pdev->dev.of_node, pdata);
	if (ret)
		goto err;

	/* Default value is 83, if not found in DT */
	if (adreno_of_read_property(pdev->dev.of_node, "qcom,idle-timeout",
		&pdata->idle_timeout))
		pdata->idle_timeout = 83;

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,nap-allowed",
		&pdata->nap_allowed))
		pdata->nap_allowed = 1;

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,clk-map",
		&pdata->clk_map))
		goto err;

	device = (struct kgsl_device *)pdev->id_entry->driver_data;

	if (device->id != KGSL_DEVICE_3D0)
		goto err;

	/* Bus Scale Data */

	pdata->bus_scale_table = adreno_of_get_bus_scale(pdev->dev.of_node);
	if (IS_ERR_OR_NULL(pdata->bus_scale_table)) {
		ret = PTR_ERR(pdata->bus_scale_table);
		goto err;
	}

	pdata->core_info = adreno_of_get_dcvs(pdev->dev.of_node);
	if (IS_ERR_OR_NULL(pdata->core_info)) {
		ret = PTR_ERR(pdata->core_info);
		goto err;
	}

	ret = adreno_of_get_iommu(pdev->dev.of_node, pdata);
	if (ret)
		goto err;

	pdev->dev.platform_data = pdata;
	return 0;

err:
	if (pdata) {
		adreno_of_free_bus_scale_info(pdata->bus_scale_table);
		if (pdata->core_info)
			kfree(pdata->core_info->freq_tbl);
		kfree(pdata->core_info);

		if (pdata->iommu_data)
			kfree(pdata->iommu_data->iommu_ctxs);

		kfree(pdata->iommu_data);
	}

	kfree(pdata);

	return ret;
}

#ifdef CONFIG_MSM_OCMEM
static int
adreno_ocmem_gmem_malloc(struct adreno_device *adreno_dev)
{
	if (adreno_dev->gpurev != ADRENO_REV_A330)
		return 0;

	/* OCMEM is only needed once, do not support consective allocation */
	if (adreno_dev->ocmem_hdl != NULL)
		return 0;

	adreno_dev->ocmem_hdl =
		ocmem_allocate(OCMEM_GRAPHICS, adreno_dev->gmem_size);
	if (adreno_dev->ocmem_hdl == NULL)
		return -ENOMEM;

	adreno_dev->gmem_size = adreno_dev->ocmem_hdl->len;
	adreno_dev->gmem_base = adreno_dev->ocmem_hdl->addr;

	return 0;
}

static void
adreno_ocmem_gmem_free(struct adreno_device *adreno_dev)
{
	if (adreno_dev->gpurev != ADRENO_REV_A330)
		return;

	if (adreno_dev->ocmem_hdl == NULL)
		return;

	ocmem_free(OCMEM_GRAPHICS, adreno_dev->ocmem_hdl);
	adreno_dev->ocmem_hdl = NULL;
}
#else
static int
adreno_ocmem_gmem_malloc(struct adreno_device *adreno_dev)
{
	return 0;
}

static void
adreno_ocmem_gmem_free(struct adreno_device *adreno_dev)
{
}
#endif

static int __devinit
adreno_probe(struct platform_device *pdev)
{
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	int status = -EINVAL;
	bool is_dt;

	is_dt = of_match_device(adreno_match_table, &pdev->dev);

	if (is_dt && pdev->dev.of_node) {
		status = adreno_of_get_pdata(pdev);
		if (status)
			goto error_return;
	}

	device = (struct kgsl_device *)pdev->id_entry->driver_data;
	adreno_dev = ADRENO_DEVICE(device);
	device->parentdev = &pdev->dev;

	status = adreno_ringbuffer_init(device);
	if (status != 0)
		goto error;

	status = kgsl_device_platform_probe(device);
	if (status)
		goto error_close_rb;

	adreno_debugfs_init(device);

	kgsl_pwrscale_init(device);
	kgsl_pwrscale_attach_policy(device, ADRENO_DEFAULT_PWRSCALE_POLICY);

	device->flags &= ~KGSL_FLAGS_SOFT_RESET;
	return 0;

error_close_rb:
	adreno_ringbuffer_close(&adreno_dev->ringbuffer);
error:
	device->parentdev = NULL;
error_return:
	return status;
}

static int __devexit adreno_remove(struct platform_device *pdev)
{
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;

	device = (struct kgsl_device *)pdev->id_entry->driver_data;
	adreno_dev = ADRENO_DEVICE(device);

	kgsl_pwrscale_detach_policy(device);
	kgsl_pwrscale_close(device);

	adreno_ringbuffer_close(&adreno_dev->ringbuffer);
	kgsl_device_platform_remove(device);

	return 0;
}

static int adreno_start(struct kgsl_device *device, unsigned int init_ram)
{
	int status = -EINVAL;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (KGSL_STATE_DUMP_AND_RECOVER != device->state)
		kgsl_pwrctrl_set_state(device, KGSL_STATE_INIT);

	/* Power up the device */
	kgsl_pwrctrl_enable(device);

	/* Identify the specific GPU */
	adreno_identify_gpu(adreno_dev);

	if (adreno_dev->gpurev == ADRENO_REV_UNKNOWN) {
		KGSL_DRV_ERR(device, "Unknown chip ID %x\n",
			adreno_dev->chip_id);
		goto error_clk_off;
	}

	/* Set up the MMU */
	if (adreno_is_a2xx(adreno_dev)) {
		/*
		 * the MH_CLNT_INTF_CTRL_CONFIG registers aren't present
		 * on older gpus
		 */
		if (adreno_is_a20x(adreno_dev)) {
			device->mh.mh_intf_cfg1 = 0;
			device->mh.mh_intf_cfg2 = 0;
		}

		kgsl_mh_start(device);
	}

	/* Assign correct RBBM status register to hang detect regs
	 */
	hang_detect_regs[0] = adreno_dev->gpudev->reg_rbbm_status;

	status = kgsl_mmu_start(device);
	if (status)
		goto error_clk_off;

	status = adreno_ocmem_gmem_malloc(adreno_dev);
	if (status) {
		KGSL_DRV_ERR(device, "OCMEM malloc failed\n");
		goto error_mmu_off;
	}

	/* Start the GPU */
	adreno_dev->gpudev->start(adreno_dev);

	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
	device->ftbl->irqctrl(device, 1);

	status = adreno_ringbuffer_start(&adreno_dev->ringbuffer, init_ram);
	if (status == 0) {
		/* While recovery is on we do not want timer to
		 * fire and attempt to change any device state */
		if (KGSL_STATE_DUMP_AND_RECOVER != device->state)
			mod_timer(&device->idle_timer, jiffies + FIRST_TIMEOUT);
		return 0;
	}

	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);

error_mmu_off:
	kgsl_mmu_stop(&device->mmu);

error_clk_off:
	kgsl_pwrctrl_disable(device);

	return status;
}

static int adreno_stop(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	adreno_dev->drawctxt_active = NULL;

	adreno_ringbuffer_stop(&adreno_dev->ringbuffer);

	kgsl_mmu_stop(&device->mmu);

	device->ftbl->irqctrl(device, 0);
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
	del_timer_sync(&device->idle_timer);

	adreno_ocmem_gmem_free(adreno_dev);

	/* Power down the device */
	kgsl_pwrctrl_disable(device);

	return 0;
}

static void adreno_mark_context_status(struct kgsl_device *device,
					int recovery_status)
{
	struct kgsl_context *context;
	int next = 0;
	/*
	 * Set the reset status of all contexts to
	 * INNOCENT_CONTEXT_RESET_EXT except for the bad context
	 * since thats the guilty party, if recovery failed then
	 * mark all as guilty
	 */
	while ((context = idr_get_next(&device->context_idr, &next))) {
		struct adreno_context *adreno_context = context->devctxt;
		if (recovery_status) {
			context->reset_status =
					KGSL_CTX_STAT_GUILTY_CONTEXT_RESET_EXT;
			adreno_context->flags |= CTXT_FLAGS_GPU_HANG;
		} else if (KGSL_CTX_STAT_GUILTY_CONTEXT_RESET_EXT !=
			context->reset_status) {
			if (adreno_context->flags & (CTXT_FLAGS_GPU_HANG ||
				CTXT_FLAGS_GPU_HANG_RECOVERED))
				context->reset_status =
				KGSL_CTX_STAT_GUILTY_CONTEXT_RESET_EXT;
			else
				context->reset_status =
				KGSL_CTX_STAT_INNOCENT_CONTEXT_RESET_EXT;
		}
		next = next + 1;
	}
}

static void adreno_set_max_ts_for_bad_ctxs(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	struct kgsl_context *context;
	struct adreno_context *temp_adreno_context;
	int next = 0;

	while ((context = idr_get_next(&device->context_idr, &next))) {
		temp_adreno_context = context->devctxt;
		if (temp_adreno_context->flags & CTXT_FLAGS_GPU_HANG) {
			kgsl_sharedmem_writel(&device->memstore,
				KGSL_MEMSTORE_OFFSET(context->id,
				soptimestamp),
				rb->timestamp[context->id]);
			kgsl_sharedmem_writel(&device->memstore,
				KGSL_MEMSTORE_OFFSET(context->id,
				eoptimestamp),
				rb->timestamp[context->id]);
		}
		next = next + 1;
	}
}

static void adreno_destroy_recovery_data(struct adreno_recovery_data *rec_data)
{
	vfree(rec_data->rb_buffer);
	vfree(rec_data->bad_rb_buffer);
}

static int adreno_setup_recovery_data(struct kgsl_device *device,
					struct adreno_recovery_data *rec_data)
{
	int ret = 0;
	unsigned int ib1_sz, ib2_sz;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

	memset(rec_data, 0, sizeof(*rec_data));

	adreno_regread(device, REG_CP_IB1_BUFSZ, &ib1_sz);
	adreno_regread(device, REG_CP_IB2_BUFSZ, &ib2_sz);
	if (ib1_sz || ib2_sz)
		adreno_regread(device, REG_CP_IB1_BASE, &rec_data->ib1);

	kgsl_sharedmem_readl(&device->memstore, &rec_data->context_id,
			KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
			current_context));

	kgsl_sharedmem_readl(&device->memstore,
				&rec_data->global_eop,
				KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
				eoptimestamp));

	rec_data->rb_buffer = vmalloc(rb->buffer_desc.size);
	if (!rec_data->rb_buffer) {
		KGSL_MEM_ERR(device, "vmalloc(%d) failed\n",
				rb->buffer_desc.size);
		return -ENOMEM;
	}

	rec_data->bad_rb_buffer = vmalloc(rb->buffer_desc.size);
	if (!rec_data->bad_rb_buffer) {
		KGSL_MEM_ERR(device, "vmalloc(%d) failed\n",
				rb->buffer_desc.size);
		ret = -ENOMEM;
		goto done;
	}

done:
	if (ret) {
		vfree(rec_data->rb_buffer);
		vfree(rec_data->bad_rb_buffer);
	}
	return ret;
}

static int
_adreno_recover_hang(struct kgsl_device *device,
			struct adreno_recovery_data *rec_data,
			bool try_bad_commands)
{
	int ret;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	struct kgsl_context *context;
	struct adreno_context *adreno_context = NULL;
	struct adreno_context *last_active_ctx = adreno_dev->drawctxt_active;

	context = idr_find(&device->context_idr, rec_data->context_id);
	if (context == NULL) {
		KGSL_DRV_ERR(device, "Last context unknown id:%d\n",
			rec_data->context_id);
	} else {
		adreno_context = context->devctxt;
		adreno_context->flags |= CTXT_FLAGS_GPU_HANG;
	}

	/* Extract valid contents from rb which can still be executed after
	 * hang */
	ret = adreno_ringbuffer_extract(rb, rec_data);
	if (ret)
		goto done;

	/* restart device */
	ret = adreno_stop(device);
	if (ret) {
		KGSL_DRV_ERR(device, "Device stop failed in recovery\n");
		goto done;
	}

	ret = adreno_start(device, true);
	if (ret) {
		KGSL_DRV_ERR(device, "Device start failed in recovery\n");
		goto done;
	}

	if (context)
		kgsl_mmu_setstate(&device->mmu, adreno_context->pagetable,
			KGSL_MEMSTORE_GLOBAL);

	/* If iommu is used then we need to make sure that the iommu clocks
	 * are on since there could be commands in pipeline that touch iommu */
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype()) {
		ret = kgsl_mmu_enable_clk(&device->mmu,
			KGSL_IOMMU_CONTEXT_USER);
		if (ret)
			goto done;
	}

	/* Do not try the bad caommands if recovery has failed bad commands
	 * once already */
	if (!try_bad_commands)
		rec_data->bad_rb_size = 0;

	if (rec_data->bad_rb_size) {
		int idle_ret;
		/* submit the bad and good context commands and wait for
		 * them to pass */
		adreno_ringbuffer_restore(rb, rec_data->bad_rb_buffer,
					rec_data->bad_rb_size);
		idle_ret = adreno_idle(device);
		if (idle_ret) {
			ret = adreno_stop(device);
			if (ret) {
				KGSL_DRV_ERR(device,
				"Device stop failed in recovery\n");
				goto done;
			}
			ret = adreno_start(device, true);
			if (ret) {
				KGSL_DRV_ERR(device,
				"Device start failed in recovery\n");
				goto done;
			}
			if (context)
				kgsl_mmu_setstate(&device->mmu,
						adreno_context->pagetable,
						KGSL_MEMSTORE_GLOBAL);

			if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype()) {
				ret = kgsl_mmu_enable_clk(&device->mmu,
						KGSL_IOMMU_CONTEXT_USER);
				if (ret)
					goto done;
			}

			ret = idle_ret;
			KGSL_DRV_ERR(device,
			"Bad context commands hung in recovery\n");
		} else {
			KGSL_DRV_ERR(device,
			"Bad context commands succeeded in recovery\n");
			if (adreno_context)
				adreno_context->flags = (adreno_context->flags &
					~CTXT_FLAGS_GPU_HANG) |
					CTXT_FLAGS_GPU_HANG_RECOVERED;
			adreno_dev->drawctxt_active = last_active_ctx;
		}
	}
	/* If either the bad command sequence failed or we did not play it */
	if (ret || !rec_data->bad_rb_size) {
		adreno_ringbuffer_restore(rb, rec_data->rb_buffer,
				rec_data->rb_size);
		ret = adreno_idle(device);
		if (ret) {
			/* If we fail here we can try to invalidate another
			 * context and try recovering again */
			ret = -EAGAIN;
			goto done;
		}
		/* ringbuffer now has data from the last valid context id,
		 * so restore the active_ctx to the last valid context */
		if (rec_data->last_valid_ctx_id) {
			struct kgsl_context *last_ctx =
					idr_find(&device->context_idr,
					rec_data->last_valid_ctx_id);
			if (last_ctx)
				adreno_dev->drawctxt_active = last_ctx->devctxt;
		}
	}
done:
	/* Turn off iommu clocks */
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype())
		kgsl_mmu_disable_clk_on_ts(&device->mmu, 0, false);
	return ret;
}

static int
adreno_recover_hang(struct kgsl_device *device,
			struct adreno_recovery_data *rec_data)
{
	int ret = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	unsigned int timestamp;

	KGSL_DRV_ERR(device,
	"Starting recovery from 3D GPU hang. Recovery parameters: IB1: 0x%X, "
	"Bad context_id: %u, global_eop: 0x%x\n",
	rec_data->ib1, rec_data->context_id, rec_data->global_eop);

	timestamp = rb->timestamp[KGSL_MEMSTORE_GLOBAL];
	KGSL_DRV_ERR(device, "Last issued global timestamp: %x\n", timestamp);

	/* We may need to replay commands multiple times based on whether
	 * multiple contexts hang the GPU */
	while (true) {
		if (!ret)
			ret = _adreno_recover_hang(device, rec_data, true);
		else
			ret = _adreno_recover_hang(device, rec_data, false);

		if (-EAGAIN == ret) {
			/* setup new recovery parameters and retry, this
			 * means more than 1 contexts are causing hang */
			adreno_destroy_recovery_data(rec_data);
			adreno_setup_recovery_data(device, rec_data);
			KGSL_DRV_ERR(device,
			"Retry recovery from 3D GPU hang. Recovery parameters: "
			"IB1: 0x%X, Bad context_id: %u, global_eop: 0x%x\n",
			rec_data->ib1, rec_data->context_id,
			rec_data->global_eop);
		} else {
			break;
		}
	}

	if (ret)
		goto done;

	/* Restore correct states after recovery */
	if (adreno_dev->drawctxt_active)
		device->mmu.hwpagetable =
			adreno_dev->drawctxt_active->pagetable;
	else
		device->mmu.hwpagetable = device->mmu.defaultpagetable;
	rb->timestamp[KGSL_MEMSTORE_GLOBAL] = timestamp;
	kgsl_sharedmem_writel(&device->memstore,
			KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
			eoptimestamp),
			rb->timestamp[KGSL_MEMSTORE_GLOBAL]);
done:
	adreno_set_max_ts_for_bad_ctxs(device);
	adreno_mark_context_status(device, ret);
	if (!ret)
		KGSL_DRV_ERR(device, "Recovery succeeded\n");
	else
		KGSL_DRV_ERR(device, "Recovery failed\n");
	return ret;
}

int
adreno_dump_and_recover(struct kgsl_device *device)
{
	int result = -ETIMEDOUT;
	struct adreno_recovery_data rec_data;

	if (device->state == KGSL_STATE_HUNG)
		goto done;
	if (device->state == KGSL_STATE_DUMP_AND_RECOVER) {
		mutex_unlock(&device->mutex);
		wait_for_completion(&device->recovery_gate);
		mutex_lock(&device->mutex);
		if (device->state != KGSL_STATE_HUNG)
			result = 0;
	} else {
		kgsl_pwrctrl_set_state(device, KGSL_STATE_DUMP_AND_RECOVER);
		INIT_COMPLETION(device->recovery_gate);
		/* Detected a hang */

		/* Get the recovery data as soon as hang is detected */
		result = adreno_setup_recovery_data(device, &rec_data);
		/*
		 * Trigger an automatic dump of the state to
		 * the console
		 */
		kgsl_postmortem_dump(device, 0);

		/*
		 * Make a GPU snapshot.  For now, do it after the PM dump so we
		 * can at least be sure the PM dump will work as it always has
		 */
		kgsl_device_snapshot(device, 1);

		result = adreno_recover_hang(device, &rec_data);
		adreno_destroy_recovery_data(&rec_data);
		if (result) {
			kgsl_pwrctrl_set_state(device, KGSL_STATE_HUNG);
		} else {
			kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);
			mod_timer(&device->idle_timer, jiffies + FIRST_TIMEOUT);
		}
		complete_all(&device->recovery_gate);
	}
done:
	return result;
}
EXPORT_SYMBOL(adreno_dump_and_recover);

static int adreno_getproperty(struct kgsl_device *device,
				enum kgsl_property_type type,
				void *value,
				unsigned int sizebytes)
{
	int status = -EINVAL;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	switch (type) {
	case KGSL_PROP_DEVICE_INFO:
		{
			struct kgsl_devinfo devinfo;

			if (sizebytes != sizeof(devinfo)) {
				status = -EINVAL;
				break;
			}

			memset(&devinfo, 0, sizeof(devinfo));
			devinfo.device_id = device->id+1;
			devinfo.chip_id = adreno_dev->chip_id;
			devinfo.mmu_enabled = kgsl_mmu_enabled();
			devinfo.gpu_id = adreno_dev->gpurev;
			devinfo.gmem_gpubaseaddr = adreno_dev->gmem_base;
			devinfo.gmem_sizebytes = adreno_dev->gmem_size;

			if (copy_to_user(value, &devinfo, sizeof(devinfo)) !=
					0) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_DEVICE_SHADOW:
		{
			struct kgsl_shadowprop shadowprop;

			if (sizebytes != sizeof(shadowprop)) {
				status = -EINVAL;
				break;
			}
			memset(&shadowprop, 0, sizeof(shadowprop));
			if (device->memstore.hostptr) {
				/*NOTE: with mmu enabled, gpuaddr doesn't mean
				 * anything to mmap().
				 */
				shadowprop.gpuaddr = device->memstore.gpuaddr;
				shadowprop.size = device->memstore.size;
				/* GSL needs this to be set, even if it
				   appears to be meaningless */
				shadowprop.flags = KGSL_FLAGS_INITIALIZED |
					KGSL_FLAGS_PER_CONTEXT_TIMESTAMPS;
			}
			if (copy_to_user(value, &shadowprop,
				sizeof(shadowprop))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_MMU_ENABLE:
		{
			int mmu_prop = kgsl_mmu_enabled();

			if (sizebytes != sizeof(int)) {
				status = -EINVAL;
				break;
			}
			if (copy_to_user(value, &mmu_prop, sizeof(mmu_prop))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_INTERRUPT_WAITS:
		{
			int int_waits = 1;
			if (sizebytes != sizeof(int)) {
				status = -EINVAL;
				break;
			}
			if (copy_to_user(value, &int_waits, sizeof(int))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	default:
		status = -EINVAL;
	}

	return status;
}

static int adreno_setproperty(struct kgsl_device *device,
				enum kgsl_property_type type,
				void *value,
				unsigned int sizebytes)
{
	int status = -EINVAL;

	switch (type) {
	case KGSL_PROP_PWRCTRL: {
			unsigned int enable;
			struct kgsl_device_platform_data *pdata =
				kgsl_device_get_drvdata(device);

			if (sizebytes != sizeof(enable))
				break;

			if (copy_from_user(&enable, (void __user *) value,
				sizeof(enable))) {
				status = -EFAULT;
				break;
			}

			if (enable) {
				if (pdata->nap_allowed)
					device->pwrctrl.nap_allowed = true;

				kgsl_pwrscale_enable(device);
			} else {
				device->pwrctrl.nap_allowed = false;
				kgsl_pwrscale_disable(device);
			}

			status = 0;
		}
		break;
	default:
		break;
	}

	return status;
}

static inline void adreno_poke(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	adreno_regwrite(device, REG_CP_RB_WPTR, adreno_dev->ringbuffer.wptr);
}

static int adreno_ringbuffer_drain(struct kgsl_device *device,
	unsigned int *regs)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	unsigned long wait;
	unsigned long timeout = jiffies + msecs_to_jiffies(ADRENO_IDLE_TIMEOUT);

	if (!(rb->flags & KGSL_FLAGS_STARTED))
		return 0;

	/*
	 * The first time into the loop, wait for 100 msecs and kick wptr again
	 * to ensure that the hardware has updated correctly.  After that, kick
	 * it periodically every KGSL_TIMEOUT_PART msecs until the timeout
	 * expires
	 */

	wait = jiffies + msecs_to_jiffies(100);

	adreno_poke(device);

	do {
		if (time_after(jiffies, wait)) {
			adreno_poke(device);

			/* Check to see if the core is hung */
			if (adreno_hang_detect(device, regs))
				return -ETIMEDOUT;

			wait = jiffies + msecs_to_jiffies(KGSL_TIMEOUT_PART);
		}
		GSL_RB_GET_READPTR(rb, &rb->rptr);

		if (time_after(jiffies, timeout)) {
			KGSL_DRV_ERR(device, "rptr: %x, wptr: %x\n",
				rb->rptr, rb->wptr);
			return -ETIMEDOUT;
		}
	} while (rb->rptr != rb->wptr);

	return 0;
}

/* Caller must hold the device mutex. */
int adreno_idle(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int rbbm_status;
	unsigned long wait_time;
	unsigned long wait_time_part;
	unsigned int prev_reg_val[hang_detect_regs_count];

	memset(prev_reg_val, 0, sizeof(prev_reg_val));

	kgsl_cffdump_regpoll(device->id,
		adreno_dev->gpudev->reg_rbbm_status << 2,
		0x00000000, 0x80000000);

retry:
	/* First, wait for the ringbuffer to drain */
	if (adreno_ringbuffer_drain(device, prev_reg_val))
		goto err;

	/* now, wait for the GPU to finish its operations */
	wait_time = jiffies + ADRENO_IDLE_TIMEOUT;
	wait_time_part = jiffies + msecs_to_jiffies(KGSL_TIMEOUT_PART);

	while (time_before(jiffies, wait_time)) {
		adreno_regread(device, adreno_dev->gpudev->reg_rbbm_status,
			&rbbm_status);
		if (adreno_is_a2xx(adreno_dev)) {
			if (rbbm_status == 0x110)
				return 0;
		} else {
			if (!(rbbm_status & 0x80000000))
				return 0;
		}

		/* Dont wait for timeout, detect hang faster.
		 */
		if (time_after(jiffies, wait_time_part)) {
				wait_time_part = jiffies +
					msecs_to_jiffies(KGSL_TIMEOUT_PART);
				if ((adreno_hang_detect(device, prev_reg_val)))
					goto err;
		}

	}

err:
	KGSL_DRV_ERR(device, "spun too long waiting for RB to idle\n");
	if (KGSL_STATE_DUMP_AND_RECOVER != device->state &&
		!adreno_dump_and_recover(device)) {
		wait_time = jiffies + ADRENO_IDLE_TIMEOUT;
		goto retry;
	}
	return -ETIMEDOUT;
}

static unsigned int adreno_isidle(struct kgsl_device *device)
{
	int status = false;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	unsigned int rbbm_status;

	WARN_ON(device->state == KGSL_STATE_INIT);
	/* If the device isn't active, don't force it on. */
	if (device->state == KGSL_STATE_ACTIVE) {
		/* Is the ring buffer is empty? */
		GSL_RB_GET_READPTR(rb, &rb->rptr);
		if (!device->active_cnt && (rb->rptr == rb->wptr)) {
			/* Is the core idle? */
			adreno_regread(device,
				adreno_dev->gpudev->reg_rbbm_status,
				&rbbm_status);

			if (adreno_is_a2xx(adreno_dev)) {
				if (rbbm_status == 0x110)
					status = true;
			} else {
				if (!(rbbm_status & 0x80000000))
					status = true;
			}
		}
	} else {
		status = true;
	}
	return status;
}

/* Caller must hold the device mutex. */
static int adreno_suspend_context(struct kgsl_device *device)
{
	int status = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* switch to NULL ctxt */
	if (adreno_dev->drawctxt_active != NULL) {
		adreno_drawctxt_switch(adreno_dev, NULL, 0);
		status = adreno_idle(device);
	}

	return status;
}

/* Find a memory structure attached to an adreno context */

struct kgsl_memdesc *adreno_find_ctxtmem(struct kgsl_device *device,
	unsigned int pt_base, unsigned int gpuaddr, unsigned int size)
{
	struct kgsl_context *context;
	struct adreno_context *adreno_context = NULL;
	int next = 0;

	while (1) {
		context = idr_get_next(&device->context_idr, &next);
		if (context == NULL)
			break;

		adreno_context = (struct adreno_context *)context->devctxt;

		if (kgsl_mmu_pt_equal(adreno_context->pagetable, pt_base)) {
			struct kgsl_memdesc *desc;

			desc = &adreno_context->gpustate;
			if (kgsl_gpuaddr_in_memdesc(desc, gpuaddr, size))
				return desc;

			desc = &adreno_context->context_gmem_shadow.gmemshadow;
			if (kgsl_gpuaddr_in_memdesc(desc, gpuaddr, size))
				return desc;
		}
		next = next + 1;
	}

	return NULL;
}

struct kgsl_memdesc *adreno_find_region(struct kgsl_device *device,
						unsigned int pt_base,
						unsigned int gpuaddr,
						unsigned int size)
{
	struct kgsl_mem_entry *entry;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *ringbuffer = &adreno_dev->ringbuffer;

	if (kgsl_gpuaddr_in_memdesc(&ringbuffer->buffer_desc, gpuaddr, size))
		return &ringbuffer->buffer_desc;

	if (kgsl_gpuaddr_in_memdesc(&ringbuffer->memptrs_desc, gpuaddr, size))
		return &ringbuffer->memptrs_desc;

	if (kgsl_gpuaddr_in_memdesc(&device->memstore, gpuaddr, size))
		return &device->memstore;

	if (kgsl_gpuaddr_in_memdesc(&device->mmu.setstate_memory, gpuaddr,
					size))
		return &device->mmu.setstate_memory;

	entry = kgsl_get_mem_entry(pt_base, gpuaddr, size);

	if (entry)
		return &entry->memdesc;

	return adreno_find_ctxtmem(device, pt_base, gpuaddr, size);
}

uint8_t *adreno_convertaddr(struct kgsl_device *device, unsigned int pt_base,
			    unsigned int gpuaddr, unsigned int size)
{
	struct kgsl_memdesc *memdesc;

	memdesc = adreno_find_region(device, pt_base, gpuaddr, size);

	return memdesc ? kgsl_gpuaddr_to_vaddr(memdesc, gpuaddr) : NULL;
}

void adreno_regread(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int *value)
{
	unsigned int *reg;
	BUG_ON(offsetwords*sizeof(uint32_t) >= device->reg_len);
	reg = (unsigned int *)(device->reg_virt + (offsetwords << 2));

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	/*ensure this read finishes before the next one.
	 * i.e. act like normal readl() */
	*value = __raw_readl(reg);
	rmb();
}

void adreno_regwrite(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int value)
{
	unsigned int *reg;

	BUG_ON(offsetwords*sizeof(uint32_t) >= device->reg_len);

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	kgsl_cffdump_regwrite(device->id, offsetwords << 2, value);
	reg = (unsigned int *)(device->reg_virt + (offsetwords << 2));

	/*ensure previous writes post before this one,
	 * i.e. act like normal writel() */
	wmb();
	__raw_writel(value, reg);
}

static unsigned int _get_context_id(struct kgsl_context *k_ctxt)
{
	unsigned int context_id = KGSL_MEMSTORE_GLOBAL;
	if (k_ctxt != NULL) {
		struct adreno_context *a_ctxt = k_ctxt->devctxt;
		if (k_ctxt->id == KGSL_CONTEXT_INVALID || a_ctxt == NULL)
			context_id = KGSL_CONTEXT_INVALID;
		else if (a_ctxt->flags & CTXT_FLAGS_PER_CONTEXT_TS)
			context_id = k_ctxt->id;
	}

	return context_id;
}

static int kgsl_check_interrupt_timestamp(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp)
{
	int status;
	unsigned int ref_ts, enableflag;
	unsigned int context_id;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	mutex_lock(&device->mutex);
	context_id = _get_context_id(context);
	/*
	 * If the context ID is invalid, we are in a race with
	 * the context being destroyed by userspace so bail.
	 */
	if (context_id == KGSL_CONTEXT_INVALID) {
		KGSL_DRV_WARN(device, "context was detached");
		status = -EINVAL;
		goto unlock;
	}

	status = kgsl_check_timestamp(device, context, timestamp);
	if (!status) {
		kgsl_sharedmem_readl(&device->memstore, &enableflag,
			KGSL_MEMSTORE_OFFSET(context_id, ts_cmp_enable));
		mb();

		if (enableflag) {
			kgsl_sharedmem_readl(&device->memstore, &ref_ts,
				KGSL_MEMSTORE_OFFSET(context_id,
					ref_wait_ts));
			mb();
			if (timestamp_cmp(ref_ts, timestamp) >= 0) {
				kgsl_sharedmem_writel(&device->memstore,
				KGSL_MEMSTORE_OFFSET(context_id,
					ref_wait_ts), timestamp);
				wmb();
			}
		} else {
			unsigned int cmds[2];
			kgsl_sharedmem_writel(&device->memstore,
				KGSL_MEMSTORE_OFFSET(context_id,
					ref_wait_ts), timestamp);
			enableflag = 1;
			kgsl_sharedmem_writel(&device->memstore,
				KGSL_MEMSTORE_OFFSET(context_id,
					ts_cmp_enable), enableflag);
			wmb();
			/* submit a dummy packet so that even if all
			* commands upto timestamp get executed we will still
			* get an interrupt */
			cmds[0] = cp_type3_packet(CP_NOP, 1);
			cmds[1] = 0;

			if (adreno_dev->drawctxt_active)
				adreno_ringbuffer_issuecmds_intr(device,
						context, &cmds[0], 2);
			else
				/* We would never call this function if there
				 * was no active contexts running */
				BUG();
		}
	}
unlock:
	mutex_unlock(&device->mutex);

	return status;
}

/*
 wait_event_interruptible_timeout checks for the exit condition before
 placing a process in wait q. For conditional interrupts we expect the
 process to already be in its wait q when its exit condition checking
 function is called.
*/
#define kgsl_wait_event_interruptible_timeout(wq, condition, timeout, io)\
({									\
	long __ret = timeout;						\
	if (io)						\
		__wait_io_event_interruptible_timeout(wq, condition, __ret);\
	else						\
		__wait_event_interruptible_timeout(wq, condition, __ret);\
	__ret;								\
})



unsigned int adreno_hang_detect(struct kgsl_device *device,
						unsigned int *prev_reg_val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int curr_reg_val[hang_detect_regs_count];
	unsigned int hang_detected = 1;
	unsigned int i;

	if (!adreno_dev->fast_hang_detect)
		return 0;

	for (i = 0; i < hang_detect_regs_count; i++) {
		adreno_regread(device, hang_detect_regs[i],
					   &curr_reg_val[i]);
		if (curr_reg_val[i] != prev_reg_val[i]) {
			prev_reg_val[i] = curr_reg_val[i];
			hang_detected = 0;
		}
	}

	return hang_detected;
}


/* MUST be called with the device mutex held */
static int adreno_waittimestamp(struct kgsl_device *device,
				struct kgsl_context *context,
				unsigned int timestamp,
				unsigned int msecs)
{
	long status = 0;
	uint io = 1;
	static uint io_cnt;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int retries = 0;
	unsigned int ts_issued;
	unsigned int context_id = _get_context_id(context);
	unsigned int time_elapsed = 0;
	unsigned int prev_reg_val[hang_detect_regs_count];
	unsigned int wait;

	memset(prev_reg_val, 0, sizeof(prev_reg_val));

	ts_issued = adreno_dev->ringbuffer.timestamp[context_id];

	/* Don't wait forever, set a max value for now */
	if (msecs == KGSL_TIMEOUT_DEFAULT)
		msecs = adreno_dev->wait_timeout;

	if (timestamp_cmp(timestamp, ts_issued) > 0) {
		KGSL_DRV_ERR(device, "Cannot wait for invalid ts <%d:0x%x>, "
			"last issued ts <%d:0x%x>\n",
			context_id, timestamp, context_id, ts_issued);
		status = -EINVAL;
		goto done;
	}

	/*
	 * Make the first timeout interval 100 msecs and then try to kick the
	 * wptr again.  This helps to ensure the wptr is updated properly.  If
	 * the requested timeout is less than 100 msecs, then wait 20msecs which
	 * is the minimum amount of time we can safely wait at 100HZ
	 */

	if (msecs == 0 || msecs >= 100)
		wait = 100;
	else
		wait = 20;

	do {
		/*
		 * If the context ID is invalid, we are in a race with
		 * the context being destroyed by userspace so bail.
		 */
		if (context_id == KGSL_CONTEXT_INVALID) {
			KGSL_DRV_WARN(device, "context was detached");
			status = -EINVAL;
			goto done;
		}
		if (kgsl_check_timestamp(device, context, timestamp)) {
			/* if the timestamp happens while we're not
			 * waiting, there's a chance that an interrupt
			 * will not be generated and thus the timestamp
			 * work needs to be queued.
			 */
			queue_work(device->work_queue, &device->ts_expired_ws);
			status = 0;
			goto done;
		}
		adreno_poke(device);
		io_cnt = (io_cnt + 1) % 100;
		if (io_cnt <
		    pwr->pwrlevels[pwr->active_pwrlevel].io_fraction)
			io = 0;

		if ((retries > 0) &&
			(adreno_hang_detect(device, prev_reg_val)))
			goto hang_dump;

		mutex_unlock(&device->mutex);
		/* We need to make sure that the process is
		 * placed in wait-q before its condition is called
		 */
		status = kgsl_wait_event_interruptible_timeout(
				device->wait_queue,
				kgsl_check_interrupt_timestamp(device,
					context, timestamp),
				msecs_to_jiffies(wait), io);

		mutex_lock(&device->mutex);

		if (status > 0) {
			/*completed before the wait finished */
			status = 0;
			goto done;
		} else if (status < 0) {
			/*an error occurred*/
			goto done;
		}
		/*this wait timed out*/

		time_elapsed += wait;
		wait = KGSL_TIMEOUT_PART;

		retries++;

	} while (!msecs || time_elapsed < msecs);

hang_dump:
	/*
	 * Check if timestamp has retired here because we may have hit
	 * recovery which can take some time and cause waiting threads
	 * to timeout
	 */
	if (kgsl_check_timestamp(device, context, timestamp))
		goto done;
	status = -ETIMEDOUT;
	KGSL_DRV_ERR(device,
		     "Device hang detected while waiting for timestamp: "
		     "<%d:0x%x>, last submitted timestamp: <%d:0x%x>, "
		     "wptr: 0x%x\n",
		      context_id, timestamp, context_id, ts_issued,
		      adreno_dev->ringbuffer.wptr);
	if (!adreno_dump_and_recover(device)) {
		/* The timestamp that this process wanted
		 * to wait on may be invalid or expired now
		 * after successful recovery */
			status = 0;
	}
done:
	return (int)status;
}

static unsigned int adreno_readtimestamp(struct kgsl_device *device,
		struct kgsl_context *context, enum kgsl_timestamp_type type)
{
	unsigned int timestamp = 0;
	unsigned int context_id = _get_context_id(context);

	/*
	 * If the context ID is invalid, we are in a race with
	 * the context being destroyed by userspace so bail.
	 */
	if (context_id == KGSL_CONTEXT_INVALID) {
		KGSL_DRV_WARN(device, "context was detached");
		return timestamp;
	}
	switch (type) {
	case KGSL_TIMESTAMP_QUEUED: {
		struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
		struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

		timestamp = rb->timestamp[context_id];
		break;
	}
	case KGSL_TIMESTAMP_CONSUMED:
		adreno_regread(device, REG_CP_TIMESTAMP, &timestamp);
		break;
	case KGSL_TIMESTAMP_RETIRED:
		kgsl_sharedmem_readl(&device->memstore, &timestamp,
			KGSL_MEMSTORE_OFFSET(context_id, eoptimestamp));
		break;
	}

	rmb();

	return timestamp;
}

static long adreno_ioctl(struct kgsl_device_private *dev_priv,
			      unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_drawctxt_set_bin_base_offset *binbase;
	struct kgsl_context *context;

	switch (cmd) {
	case IOCTL_KGSL_DRAWCTXT_SET_BIN_BASE_OFFSET:
		binbase = data;

		context = kgsl_find_context(dev_priv, binbase->drawctxt_id);
		if (context) {
			adreno_drawctxt_set_bin_base_offset(
				dev_priv->device, context, binbase->offset);
		} else {
			result = -EINVAL;
			KGSL_DRV_ERR(dev_priv->device,
				"invalid drawctxt drawctxt_id %d "
				"device_id=%d\n",
				binbase->drawctxt_id, dev_priv->device->id);
		}
		break;

	default:
		KGSL_DRV_INFO(dev_priv->device,
			"invalid ioctl code %08x\n", cmd);
		result = -ENOIOCTLCMD;
		break;
	}
	return result;

}

static inline s64 adreno_ticks_to_us(u32 ticks, u32 gpu_freq)
{
	gpu_freq /= 1000000;
	return ticks / gpu_freq;
}

static void adreno_power_stats(struct kgsl_device *device,
				struct kgsl_power_stats *stats)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int cycles;

	/* Get the busy cycles counted since the counter was last reset */
	/* Calling this function also resets and restarts the counter */

	cycles = adreno_dev->gpudev->busy_cycles(adreno_dev);

	/* In order to calculate idle you have to have run the algorithm *
	 * at least once to get a start time. */
	if (pwr->time != 0) {
		s64 tmp = ktime_to_us(ktime_get());
		stats->total_time = tmp - pwr->time;
		pwr->time = tmp;
		stats->busy_time = adreno_ticks_to_us(cycles, device->pwrctrl.
				pwrlevels[device->pwrctrl.active_pwrlevel].
				gpu_freq);
	} else {
		stats->total_time = 0;
		stats->busy_time = 0;
		pwr->time = ktime_to_us(ktime_get());
	}
}

void adreno_irqctrl(struct kgsl_device *device, int state)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	adreno_dev->gpudev->irq_control(adreno_dev, state);
}

static unsigned int adreno_gpuid(struct kgsl_device *device,
	unsigned int *chipid)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Some applications need to know the chip ID too, so pass
	 * that as a parameter */

	if (chipid != NULL)
		*chipid = adreno_dev->chip_id;

	/* Standard KGSL gpuid format:
	 * top word is 0x0002 for 2D or 0x0003 for 3D
	 * Bottom word is core specific identifer
	 */

	return (0x0003 << 16) | ((int) adreno_dev->gpurev);
}

static const struct kgsl_functable adreno_functable = {
	/* Mandatory functions */
	.regread = adreno_regread,
	.regwrite = adreno_regwrite,
	.idle = adreno_idle,
	.isidle = adreno_isidle,
	.suspend_context = adreno_suspend_context,
	.start = adreno_start,
	.stop = adreno_stop,
	.getproperty = adreno_getproperty,
	.waittimestamp = adreno_waittimestamp,
	.readtimestamp = adreno_readtimestamp,
	.issueibcmds = adreno_ringbuffer_issueibcmds,
	.ioctl = adreno_ioctl,
	.setup_pt = adreno_setup_pt,
	.cleanup_pt = adreno_cleanup_pt,
	.power_stats = adreno_power_stats,
	.irqctrl = adreno_irqctrl,
	.gpuid = adreno_gpuid,
	.snapshot = adreno_snapshot,
	.irq_handler = adreno_irq_handler,
	/* Optional functions */
	.setstate = adreno_setstate,
	.drawctxt_create = adreno_drawctxt_create,
	.drawctxt_destroy = adreno_drawctxt_destroy,
	.setproperty = adreno_setproperty,
	.postmortem_dump = adreno_dump,
};

static struct platform_driver adreno_platform_driver = {
	.probe = adreno_probe,
	.remove = __devexit_p(adreno_remove),
	.suspend = kgsl_suspend_driver,
	.resume = kgsl_resume_driver,
	.id_table = adreno_id_table,
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_3D_NAME,
		.pm = &kgsl_pm_ops,
		.of_match_table = adreno_match_table,
	}
};

static int __init kgsl_3d_init(void)
{
	return platform_driver_register(&adreno_platform_driver);
}

static void __exit kgsl_3d_exit(void)
{
	platform_driver_unregister(&adreno_platform_driver);
}

module_init(kgsl_3d_init);
module_exit(kgsl_3d_exit);

MODULE_DESCRIPTION("3D Graphics driver");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:kgsl_3d");
