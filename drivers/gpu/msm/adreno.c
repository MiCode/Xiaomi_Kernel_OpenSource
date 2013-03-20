/* Copyright (c) 2002,2007-2013, The Linux Foundation. All rights reserved.
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
#include "kgsl_trace.h"

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
		.shadermemname = KGSL_3D0_SHADER_MEMORY,
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
	0,
	0,
	0,
	0,
	0,
	0
};

const unsigned int hang_detect_regs_count = ARRAY_SIZE(hang_detect_regs);

/*
 * This is the master list of all GPU cores that are supported by this
 * driver.
 */

#define ANY_ID (~0)
#define NO_VER (~0)

static const struct {
	enum adreno_gpurev gpurev;
	unsigned int core, major, minor, patchid;
	const char *pm4fw;
	const char *pfpfw;
	struct adreno_gpudev *gpudev;
	unsigned int istore_size;
	unsigned int pix_shader_start;
	/* Size of an instruction in dwords */
	unsigned int instruction_size;
	/* size of gmem for gpu*/
	unsigned int gmem_size;
	/* version of pm4 microcode that supports sync_lock
	   between CPU and GPU for IOMMU-v0 programming */
	unsigned int sync_lock_pm4_ver;
	/* version of pfp microcode that supports sync_lock
	   between CPU and GPU for IOMMU-v0 programming */
	unsigned int sync_lock_pfp_ver;
} adreno_gpulist[] = {
	{ ADRENO_REV_A200, 0, 2, ANY_ID, ANY_ID,
		"yamato_pm4.fw", "yamato_pfp.fw", &adreno_a2xx_gpudev,
		512, 384, 3, SZ_256K, NO_VER, NO_VER },
	{ ADRENO_REV_A203, 0, 1, 1, ANY_ID,
		"yamato_pm4.fw", "yamato_pfp.fw", &adreno_a2xx_gpudev,
		512, 384, 3, SZ_256K, NO_VER, NO_VER },
	{ ADRENO_REV_A205, 0, 1, 0, ANY_ID,
		"yamato_pm4.fw", "yamato_pfp.fw", &adreno_a2xx_gpudev,
		512, 384, 3, SZ_256K, NO_VER, NO_VER },
	{ ADRENO_REV_A220, 2, 1, ANY_ID, ANY_ID,
		"leia_pm4_470.fw", "leia_pfp_470.fw", &adreno_a2xx_gpudev,
		512, 384, 3, SZ_512K, NO_VER, NO_VER },
	/*
	 * patchlevel 5 (8960v2) needs special pm4 firmware to work around
	 * a hardware problem.
	 */
	{ ADRENO_REV_A225, 2, 2, 0, 5,
		"a225p5_pm4.fw", "a225_pfp.fw", &adreno_a2xx_gpudev,
		1536, 768, 3, SZ_512K, NO_VER, NO_VER },
	{ ADRENO_REV_A225, 2, 2, 0, 6,
		"a225_pm4.fw", "a225_pfp.fw", &adreno_a2xx_gpudev,
		1536, 768, 3, SZ_512K, 0x225011, 0x225002 },
	{ ADRENO_REV_A225, 2, 2, ANY_ID, ANY_ID,
		"a225_pm4.fw", "a225_pfp.fw", &adreno_a2xx_gpudev,
		1536, 768, 3, SZ_512K, 0x225011, 0x225002 },
	/* A3XX doesn't use the pix_shader_start */
	{ ADRENO_REV_A305, 3, 0, 5, 0,
		"a300_pm4.fw", "a300_pfp.fw", &adreno_a3xx_gpudev,
		512, 0, 2, SZ_256K, 0x3FF037, 0x3FF016 },
	/* A3XX doesn't use the pix_shader_start */
	{ ADRENO_REV_A320, 3, 2, ANY_ID, ANY_ID,
		"a300_pm4.fw", "a300_pfp.fw", &adreno_a3xx_gpudev,
		512, 0, 2, SZ_512K, 0x3FF037, 0x3FF016 },
	{ ADRENO_REV_A330, 3, 3, 0, ANY_ID,
		"a330_pm4.fw", "a330_pfp.fw", &adreno_a3xx_gpudev,
		512, 0, 2, SZ_1M, NO_VER, NO_VER },
	{ ADRENO_REV_A305B, 3, 0, 5, 0x10,
		"a330_pm4.fw", "a330_pfp.fw", &adreno_a3xx_gpudev,
		512, 0, 2, SZ_128K, NO_VER, NO_VER },
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

	result = kgsl_mmu_map_global(pagetable, &rb->buffer_desc);
	if (result)
		goto error;

	result = kgsl_mmu_map_global(pagetable, &rb->memptrs_desc);
	if (result)
		goto unmap_buffer_desc;

	result = kgsl_mmu_map_global(pagetable, &device->memstore);
	if (result)
		goto unmap_memptrs_desc;

	result = kgsl_mmu_map_global(pagetable, &device->mmu.setstate_memory);
	if (result)
		goto unmap_memstore_desc;

	/*
	 * Set the mpu end to the last "normal" global memory we use.
	 * For the IOMMU, this will be used to restrict access to the
	 * mapped registers.
	 */
	device->mh.mpu_range = device->mmu.setstate_memory.gpuaddr +
				device->mmu.setstate_memory.size;
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
	unsigned int link[230];
	unsigned int *cmds = &link[0];
	int sizedwords = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int num_iommu_units, i;
	struct kgsl_context *context;
	struct adreno_context *adreno_ctx = NULL;

	if (!adreno_dev->drawctxt_active)
		return kgsl_mmu_device_setstate(&device->mmu, flags);
	num_iommu_units = kgsl_mmu_get_num_iommu_units(&device->mmu);

	context = idr_find(&device->context_idr, context_id);
	if (context == NULL)
		return;
	adreno_ctx = context->devctxt;

	if (kgsl_mmu_enable_clk(&device->mmu,
				KGSL_IOMMU_CONTEXT_USER))
		return;

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

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	/* Acquire GPU-CPU sync Lock here */
	cmds += kgsl_mmu_sync_lock(&device->mmu, cmds);

	pt_val = kgsl_mmu_get_pt_base_addr(&device->mmu,
					device->mmu.hwpagetable);
	if (flags & KGSL_MMUFLAGS_PTUPDATE) {
		/*
		 * We need to perfrom the following operations for all
		 * IOMMU units
		 */
		for (i = 0; i < num_iommu_units; i++) {
			reg_pt_val = (pt_val + kgsl_mmu_get_pt_lsb(&device->mmu,
						i, KGSL_IOMMU_CONTEXT_USER));
			/*
			 * Set address of the new pagetable by writng to IOMMU
			 * TTBR0 register
			 */
			*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
			*cmds++ = kgsl_mmu_get_reg_gpuaddr(&device->mmu, i,
				KGSL_IOMMU_CONTEXT_USER, KGSL_IOMMU_CTX_TTBR0);
			*cmds++ = reg_pt_val;
			*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
			*cmds++ = 0x00000000;

			/*
			 * Read back the ttbr0 register as a barrier to ensure
			 * above writes have completed
			 */
			cmds += adreno_add_read_cmds(device, cmds,
				kgsl_mmu_get_reg_gpuaddr(&device->mmu, i,
				KGSL_IOMMU_CONTEXT_USER, KGSL_IOMMU_CTX_TTBR0),
				reg_pt_val,
				device->mmu.setstate_memory.gpuaddr +
				KGSL_IOMMU_SETSTATE_NOP_OFFSET);
		}
	}
	if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
		/*
		 * tlb flush
		 */
		for (i = 0; i < num_iommu_units; i++) {
			reg_pt_val = (pt_val + kgsl_mmu_get_pt_lsb(&device->mmu,
						i, KGSL_IOMMU_CONTEXT_USER));

			*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
			*cmds++ = kgsl_mmu_get_reg_gpuaddr(&device->mmu, i,
				KGSL_IOMMU_CONTEXT_USER,
				KGSL_IOMMU_CTX_TLBIALL);
			*cmds++ = 1;

			cmds += __adreno_add_idle_indirect_cmds(cmds,
			device->mmu.setstate_memory.gpuaddr +
			KGSL_IOMMU_SETSTATE_NOP_OFFSET);

			cmds += adreno_add_read_cmds(device, cmds,
				kgsl_mmu_get_reg_gpuaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TTBR0),
				reg_pt_val,
				device->mmu.setstate_memory.gpuaddr +
				KGSL_IOMMU_SETSTATE_NOP_OFFSET);
		}
	}

	/* Release GPU-CPU sync Lock here */
	cmds += kgsl_mmu_sync_unlock(&device->mmu, cmds);

	if (cpu_is_msm8960())
		cmds += adreno_add_change_mh_phys_limit_cmds(cmds,
			kgsl_mmu_get_reg_gpuaddr(&device->mmu, 0,
						0, KGSL_IOMMU_GLOBAL_BASE),
			device->mmu.setstate_memory.gpuaddr +
			KGSL_IOMMU_SETSTATE_NOP_OFFSET);
	else
		cmds += adreno_add_bank_change_cmds(cmds,
			KGSL_IOMMU_CONTEXT_PRIV,
			device->mmu.setstate_memory.gpuaddr +
			KGSL_IOMMU_SETSTATE_NOP_OFFSET);

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	sizedwords += (cmds - &link[0]);
	if (sizedwords) {
		/* invalidate all base pointers */
		*cmds++ = cp_type3_packet(CP_INVALIDATE_STATE, 1);
		*cmds++ = 0x7fff;
		sizedwords += 2;
		/* This returns the per context timestamp but we need to
		 * use the global timestamp for iommu clock disablement */
		adreno_ringbuffer_issuecmds(device, adreno_ctx,
			KGSL_CMD_FLAGS_PMODE,
			&link[0], sizedwords);
		kgsl_mmu_disable_clk_on_ts(&device->mmu,
		adreno_dev->ringbuffer.timestamp[KGSL_MEMSTORE_GLOBAL], true);
	}

	if (sizedwords > (ARRAY_SIZE(link))) {
		KGSL_DRV_ERR(device, "Temp command buffer overflow\n");
		BUG();
	}
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
		if (context == NULL)
			return;
		adreno_ctx = context->devctxt;

		if (flags & KGSL_MMUFLAGS_PTUPDATE) {
			/* wait for graphics pipe to be idle */
			*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
			*cmds++ = 0x00000000;

			/* set page table base */
			*cmds++ = cp_type0_packet(MH_MMU_PT_BASE, 1);
			*cmds++ = kgsl_mmu_get_pt_base_addr(&device->mmu,
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
	else if ((cpu_is_msm8625() || cpu_is_msm8625q()) && minorid == 0)
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
	adreno_dev->gpulist_index = i;
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

	if (adreno_of_read_property(parent, "qcom,step-pwrlevel",
		&pdata->step_mul))
		pdata->step_mul = 1;

	if (pdata->init_level < 0 || pdata->init_level > pdata->num_levels) {
		KGSL_CORE_ERR("Initial power level out of range\n");
		pdata->init_level = 1;
	}

	ret = 0;
done:
	return ret;

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

	info->power_param.num_freq = count;

	info->freq_tbl = kzalloc(info->power_param.num_freq *
			sizeof(struct msm_dcvs_freq_entry),
			GFP_KERNEL);

	if (info->freq_tbl == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			info->power_param.num_freq *
			sizeof(struct msm_dcvs_freq_entry));
		ret = -ENOMEM;
		goto err;
	}

	for_each_child_of_node(node, child) {
		unsigned int index;

		if (adreno_of_read_property(child, "reg", &index))
			goto err;

		if (index >= info->power_param.num_freq) {
			KGSL_CORE_ERR("DCVS freq entry %d is out of range\n",
				index);
			continue;
		}

		if (adreno_of_read_property(child, "qcom,freq",
			&info->freq_tbl[index].freq))
			goto err;

		if (adreno_of_read_property(child, "qcom,voltage",
			&info->freq_tbl[index].voltage))
			info->freq_tbl[index].voltage = 0;

		if (adreno_of_read_property(child, "qcom,is_trans_level",
			&info->freq_tbl[index].is_trans_level))
			info->freq_tbl[index].is_trans_level = 0;

		if (adreno_of_read_property(child, "qcom,active-energy-offset",
			&info->freq_tbl[index].active_energy_offset))
			info->freq_tbl[index].active_energy_offset = 0;

		if (adreno_of_read_property(child, "qcom,leakage-energy-offset",
			&info->freq_tbl[index].leakage_energy_offset))
			info->freq_tbl[index].leakage_energy_offset = 0;
	}

	if (adreno_of_read_property(node, "qcom,num-cores", &info->num_cores))
		goto err;

	info->sensors = kzalloc(info->num_cores *
			sizeof(int),
			GFP_KERNEL);

	for (count = 0; count < info->num_cores; count++) {
		if (adreno_of_read_property(node, "qcom,sensors",
			&(info->sensors[count])))
			goto err;
	}

	if (adreno_of_read_property(node, "qcom,core-core-type",
		&info->core_param.core_type))
		goto err;

	if (adreno_of_read_property(node, "qcom,algo-disable-pc-threshold",
		&info->algo_param.disable_pc_threshold))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-em-win-size-min-us",
		&info->algo_param.em_win_size_min_us))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-em-win-size-max-us",
		&info->algo_param.em_win_size_max_us))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-em-max-util-pct",
		&info->algo_param.em_max_util_pct))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-group-id",
		&info->algo_param.group_id))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-max-freq-chg-time-us",
		&info->algo_param.max_freq_chg_time_us))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-slack-mode-dynamic",
		&info->algo_param.slack_mode_dynamic))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-slack-weight-thresh-pct",
		&info->algo_param.slack_weight_thresh_pct))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-slack-time-min-us",
		&info->algo_param.slack_time_min_us))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-slack-time-max-us",
		&info->algo_param.slack_time_max_us))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-ss-win-size-min-us",
		&info->algo_param.ss_win_size_min_us))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-ss-win-size-max-us",
		&info->algo_param.ss_win_size_max_us))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-ss-util-pct",
		&info->algo_param.ss_util_pct))
		goto err;
	if (adreno_of_read_property(node, "qcom,algo-ss-no-corr-below-freq",
		&info->algo_param.ss_no_corr_below_freq))
		goto err;

	if (adreno_of_read_property(node, "qcom,energy-active-coeff-a",
		&info->energy_coeffs.active_coeff_a))
		goto err;
	if (adreno_of_read_property(node, "qcom,energy-active-coeff-b",
		&info->energy_coeffs.active_coeff_b))
		goto err;
	if (adreno_of_read_property(node, "qcom,energy-active-coeff-c",
		&info->energy_coeffs.active_coeff_c))
		goto err;
	if (adreno_of_read_property(node, "qcom,energy-leakage-coeff-a",
		&info->energy_coeffs.leakage_coeff_a))
		goto err;
	if (adreno_of_read_property(node, "qcom,energy-leakage-coeff-b",
		&info->energy_coeffs.leakage_coeff_b))
		goto err;
	if (adreno_of_read_property(node, "qcom,energy-leakage-coeff-c",
		&info->energy_coeffs.leakage_coeff_c))
		goto err;
	if (adreno_of_read_property(node, "qcom,energy-leakage-coeff-d",
		&info->energy_coeffs.leakage_coeff_d))
		goto err;

	if (adreno_of_read_property(node, "qcom,power-current-temp",
		&info->power_param.current_temp))
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

		ret = of_property_read_u32_array(child, "reg", reg_val, 2);
		if (ret) {
			KGSL_CORE_ERR("Unable to read KGSL IOMMU 'reg'\n");
			goto err;
		}
		if (msm_soc_version_supports_iommu_v0())
			ctxs[ctx_index].ctx_id = (reg_val[0] -
				data->physstart) >> KGSL_IOMMU_CTX_SHIFT;
		else
			ctxs[ctx_index].ctx_id = ((reg_val[0] -
				data->physstart) >> KGSL_IOMMU_CTX_SHIFT) - 8;

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

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,idle-timeout",
		&pdata->idle_timeout))
		pdata->idle_timeout = HZ/12;

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,nap-allowed",
		&pdata->nap_allowed))
		pdata->nap_allowed = 1;

	pdata->strtstp_sleepwake = of_property_read_bool(pdev->dev.of_node,
						"qcom,strtstp-sleepwake");

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,clk-map",
		&pdata->clk_map))
		goto err;

	device = (struct kgsl_device *)pdev->id_entry->driver_data;

	if (device->id != KGSL_DEVICE_3D0)
		goto err;

	/* Bus Scale Data */

	pdata->bus_scale_table = msm_bus_cl_get_pdata(pdev);
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
	if (!(adreno_is_a330(adreno_dev) ||
		adreno_is_a305b(adreno_dev)))
		return 0;

	/* OCMEM is only needed once, do not support consective allocation */
	if (adreno_dev->ocmem_hdl != NULL)
		return 0;

	adreno_dev->ocmem_hdl =
		ocmem_allocate(OCMEM_GRAPHICS, adreno_dev->gmem_size);
	if (adreno_dev->ocmem_hdl == NULL)
		return -ENOMEM;

	adreno_dev->gmem_size = adreno_dev->ocmem_hdl->len;
	adreno_dev->ocmem_base = adreno_dev->ocmem_hdl->addr;

	return 0;
}

static void
adreno_ocmem_gmem_free(struct adreno_device *adreno_dev)
{
	if (!(adreno_is_a330(adreno_dev) ||
		adreno_is_a305b(adreno_dev)))
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

	if (adreno_ringbuffer_read_pm4_ucode(device)) {
		KGSL_DRV_ERR(device, "Reading pm4 microcode failed %s\n",
			adreno_dev->pm4_fwfile);
		BUG_ON(1);
	}

	if (adreno_ringbuffer_read_pfp_ucode(device)) {
		KGSL_DRV_ERR(device, "Reading pfp microcode failed %s\n",
			adreno_dev->pfp_fwfile);
		BUG_ON(1);
	}

	if (adreno_dev->gpurev == ADRENO_REV_UNKNOWN) {
		KGSL_DRV_ERR(device, "Unknown chip ID %x\n",
			adreno_dev->chip_id);
		goto error_clk_off;
	}


	/*
	 * Check if firmware supports the sync lock PM4 packets needed
	 * for IOMMUv1
	 */

	if ((adreno_dev->pm4_fw_version >=
		adreno_gpulist[adreno_dev->gpulist_index].sync_lock_pm4_ver) &&
		(adreno_dev->pfp_fw_version >=
		adreno_gpulist[adreno_dev->gpulist_index].sync_lock_pfp_ver))
		device->mmu.flags |= KGSL_MMU_FLAGS_IOMMU_SYNC;

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

	/* Add A3XX specific registers for hang detection */
	if (adreno_is_a3xx(adreno_dev)) {
		hang_detect_regs[6] = A3XX_RBBM_PERFCTR_SP_7_LO;
		hang_detect_regs[7] = A3XX_RBBM_PERFCTR_SP_7_HI;
		hang_detect_regs[8] = A3XX_RBBM_PERFCTR_SP_6_LO;
		hang_detect_regs[9] = A3XX_RBBM_PERFCTR_SP_6_HI;
		hang_detect_regs[10] = A3XX_RBBM_PERFCTR_SP_5_LO;
		hang_detect_regs[11] = A3XX_RBBM_PERFCTR_SP_5_HI;
	}

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
	if (status)
		goto error_irq_off;

	/*
	 * While recovery is on we do not want timer to
	 * fire and attempt to change any device state
	 */

	if (KGSL_STATE_DUMP_AND_RECOVER != device->state)
		mod_timer(&device->idle_timer, jiffies + FIRST_TIMEOUT);

	device->reset_counter++;

	return 0;

error_irq_off:
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
			if (adreno_context->flags & (CTXT_FLAGS_GPU_HANG |
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
	vfree(rec_data->good_rb_buffer);
}

static int _find_start_of_cmd_seq(struct adreno_ringbuffer *rb,
					unsigned int *ptr,
					bool inc)
{
	int status = -EINVAL;
	unsigned int val1;
	unsigned int size = rb->buffer_desc.size;
	unsigned int start_ptr = *ptr;

	while ((start_ptr / sizeof(unsigned int)) != rb->wptr) {
		if (inc)
			start_ptr = adreno_ringbuffer_inc_wrapped(start_ptr,
									size);
		else
			start_ptr = adreno_ringbuffer_dec_wrapped(start_ptr,
									size);
		kgsl_sharedmem_readl(&rb->buffer_desc, &val1, start_ptr);
		if (KGSL_CMD_IDENTIFIER == val1) {
			if ((start_ptr / sizeof(unsigned int)) != rb->wptr)
				start_ptr = adreno_ringbuffer_dec_wrapped(
							start_ptr, size);
				*ptr = start_ptr;
				status = 0;
				break;
		}
	}
	return status;
}

static int _find_cmd_seq_after_eop_ts(struct adreno_ringbuffer *rb,
					unsigned int *rb_rptr,
					unsigned int global_eop,
					bool inc)
{
	int status = -EINVAL;
	unsigned int temp_rb_rptr = *rb_rptr;
	unsigned int size = rb->buffer_desc.size;
	unsigned int val[3];
	int i = 0;
	bool check = false;

	if (inc && temp_rb_rptr / sizeof(unsigned int) != rb->wptr)
		return status;

	do {
		/*
		 * when decrementing we need to decrement first and
		 * then read make sure we cover all the data
		 */
		if (!inc)
			temp_rb_rptr = adreno_ringbuffer_dec_wrapped(
					temp_rb_rptr, size);
		kgsl_sharedmem_readl(&rb->buffer_desc, &val[i],
					temp_rb_rptr);

		if (check && ((inc && val[i] == global_eop) ||
			(!inc && (val[i] ==
			cp_type3_packet(CP_MEM_WRITE, 2) ||
			val[i] == CACHE_FLUSH_TS)))) {
			/* decrement i, i.e i = (i - 1 + 3) % 3 if
			 * we are going forward, else increment i */
			i = (i + 2) % 3;
			if (val[i] == rb->device->memstore.gpuaddr +
				KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
						eoptimestamp)) {
				int j = ((i + 2) % 3);
				if ((inc && (val[j] == CACHE_FLUSH_TS ||
						val[j] == cp_type3_packet(
							CP_MEM_WRITE, 2))) ||
					(!inc && val[j] == global_eop)) {
						/* Found the global eop */
						status = 0;
						break;
				}
			}
			/* if no match found then increment i again
			 * since we decremented before matching */
			i = (i + 1) % 3;
		}
		if (inc)
			temp_rb_rptr = adreno_ringbuffer_inc_wrapped(
						temp_rb_rptr, size);

		i = (i + 1) % 3;
		if (2 == i)
			check = true;
	} while (temp_rb_rptr / sizeof(unsigned int) != rb->wptr);
	/* temp_rb_rptr points to the command stream after global eop,
	 * move backward till the start of command sequence */
	if (!status) {
		status = _find_start_of_cmd_seq(rb, &temp_rb_rptr, false);
		if (!status) {
			*rb_rptr = temp_rb_rptr;
			KGSL_DRV_ERR(rb->device,
			"Offset of cmd sequence after eop timestamp: 0x%x\n",
			temp_rb_rptr / sizeof(unsigned int));
		}
	}
	if (status)
		KGSL_DRV_ERR(rb->device,
		"Failed to find the command sequence after eop timestamp\n");
	return status;
}

static int _find_hanging_ib_sequence(struct adreno_ringbuffer *rb,
				unsigned int *rb_rptr,
				unsigned int ib1)
{
	int status = -EINVAL;
	unsigned int temp_rb_rptr = *rb_rptr;
	unsigned int size = rb->buffer_desc.size;
	unsigned int val[2];
	int i = 0;
	bool check = false;
	bool ctx_switch = false;

	while (temp_rb_rptr / sizeof(unsigned int) != rb->wptr) {
		kgsl_sharedmem_readl(&rb->buffer_desc, &val[i], temp_rb_rptr);

		if (check && val[i] == ib1) {
			/* decrement i, i.e i = (i - 1 + 2) % 2 */
			i = (i + 1) % 2;
			if (adreno_cmd_is_ib(val[i])) {
				/* go till start of command sequence */
				status = _find_start_of_cmd_seq(rb,
						&temp_rb_rptr, false);
				KGSL_DRV_INFO(rb->device,
				"Found the hanging IB at offset 0x%x\n",
				temp_rb_rptr / sizeof(unsigned int));
				break;
			}
			/* if no match the increment i since we decremented
			 * before checking */
			i = (i + 1) % 2;
		}
		/* Make sure you do not encounter a context switch twice, we can
		 * encounter it once for the bad context as the start of search
		 * can point to the context switch */
		if (val[i] == KGSL_CONTEXT_TO_MEM_IDENTIFIER) {
			if (ctx_switch) {
				KGSL_DRV_ERR(rb->device,
				"Context switch encountered before bad "
				"IB found\n");
				break;
			}
			ctx_switch = true;
		}
		i = (i + 1) % 2;
		if (1 == i)
			check = true;
		temp_rb_rptr = adreno_ringbuffer_inc_wrapped(temp_rb_rptr,
								size);
	}
	if  (!status)
		*rb_rptr = temp_rb_rptr;
	return status;
}

static int adreno_setup_recovery_data(struct kgsl_device *device,
					struct adreno_recovery_data *rec_data)
{
	int ret = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	struct kgsl_context *context;
	struct adreno_context *adreno_context;
	unsigned int rb_rptr = rb->wptr * sizeof(unsigned int);

	memset(rec_data, 0, sizeof(*rec_data));
	rec_data->start_of_replay_cmds = 0xFFFFFFFF;
	rec_data->replay_for_snapshot = 0xFFFFFFFF;

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

	rec_data->good_rb_buffer = vmalloc(rb->buffer_desc.size);
	if (!rec_data->good_rb_buffer) {
		KGSL_MEM_ERR(device, "vmalloc(%d) failed\n",
				rb->buffer_desc.size);
		ret = -ENOMEM;
		goto done;
	}
	rec_data->status = 0;

	/* find the start of bad command sequence in rb */
	context = idr_find(&device->context_idr, rec_data->context_id);
	/* Look for the command stream that is right after the global eop */

	if (!context) {
		/*
		 * If there is no context then recovery does not need to
		 * replay anything, just reset GPU and thats it
		 */
		goto done;
	}
	ret = _find_cmd_seq_after_eop_ts(rb, &rb_rptr,
					rec_data->global_eop + 1, false);
	if (ret)
		goto done;

	rec_data->start_of_replay_cmds = rb_rptr;

	if (!adreno_dev->ft_policy)
		adreno_dev->ft_policy = KGSL_FT_DEFAULT_POLICY;

	rec_data->ft_policy = adreno_dev->ft_policy;

	adreno_context = context->devctxt;
	if (adreno_context->flags & CTXT_FLAGS_PREAMBLE) {
		if (rec_data->ib1) {
			ret = _find_hanging_ib_sequence(rb,
					&rb_rptr, rec_data->ib1);
			if (ret) {
				KGSL_DRV_ERR(device,
				"Start not found for replay IB sequence\n");
				ret = 0;
				goto done;
			}
			rec_data->start_of_replay_cmds = rb_rptr;
			rec_data->replay_for_snapshot = rb_rptr;
		}
	}

done:
	if (ret) {
		vfree(rec_data->rb_buffer);
		vfree(rec_data->bad_rb_buffer);
		vfree(rec_data->good_rb_buffer);
	}
	return ret;
}

static int
_adreno_restart_device(struct kgsl_device *device,
					   struct kgsl_context *context)
{

	struct adreno_context *adreno_context = context->devctxt;

	/* restart device */
	if (adreno_stop(device)) {
		KGSL_DRV_ERR(device, "Device stop failed in recovery\n");
		return 1;
	}

	if (adreno_start(device, true)) {
		KGSL_DRV_ERR(device, "Device start failed in recovery\n");
		return 1;
	}

	if (context)
		kgsl_mmu_setstate(&device->mmu, adreno_context->pagetable,
			KGSL_MEMSTORE_GLOBAL);

	/* If iommu is used then we need to make sure that the iommu clocks
	 * are on since there could be commands in pipeline that touch iommu */
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype()) {
		if (kgsl_mmu_enable_clk(&device->mmu,
				KGSL_IOMMU_CONTEXT_USER))
			return 1;
	}

	return 0;
}

static int
_adreno_recovery_resubmit(struct kgsl_device *device,
			struct adreno_ringbuffer *rb,
			struct kgsl_context *context,
			struct adreno_recovery_data *rec_data,
			unsigned int *buff, unsigned int size)
{
	unsigned int ret = 0;

	if (_adreno_restart_device(device, context))
		return 1;

	if (size) {

		/* submit commands and wait for them to pass */
		adreno_ringbuffer_restore(rb, buff, size);

		ret = adreno_idle(device);
	}

	return ret;
}


static int
_adreno_recover_hang(struct kgsl_device *device,
			struct adreno_recovery_data *rec_data)
{
	int ret = 0, i;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	struct kgsl_context *context;
	struct adreno_context *adreno_context = NULL;
	struct adreno_context *last_active_ctx = adreno_dev->drawctxt_active;

	context = idr_find(&device->context_idr, rec_data->context_id);
	if (context == NULL) {
		KGSL_DRV_ERR(device, "Last context unknown id:%d\n",
			rec_data->context_id);
		goto play_good_cmds;
	} else {
		adreno_context = context->devctxt;
		adreno_context->flags |= CTXT_FLAGS_GPU_HANG;
		/*
		 * set the invalid ts flag to 0 for this context since we have
		 * detected a hang for it
		 */
		context->wait_on_invalid_ts = false;

		if (!(adreno_context->flags & CTXT_FLAGS_PER_CONTEXT_TS)) {
			KGSL_DRV_ERR(device, "Fault tolerance not supported\n");
			goto play_good_cmds;
		}
	}

	/*
	 * Extract valid contents from rb which can still be executed after
	 * hang
	 */
	adreno_ringbuffer_extract(rb, rec_data);

	/* Do not try the bad commands if  hang is due to a fault */
	if (device->mmu.fault) {
		KGSL_DRV_ERR(device, "MMU fault skipping bad cmds\n");
		device->mmu.fault = 0;
		goto play_good_cmds;
	}

	if (rec_data->ft_policy & KGSL_FT_DISABLE) {
		KGSL_DRV_ERR(device, "NO FT policy play only good cmds\n");
		goto play_good_cmds;
	}

	if (rec_data->ft_policy & KGSL_FT_REPLAY) {

		ret = _adreno_recovery_resubmit(device, rb, context, rec_data,
				rec_data->bad_rb_buffer, rec_data->bad_rb_size);

		if (ret) {
			KGSL_DRV_ERR(device, "Replay status: 1\n");
			rec_data->status = 1;
		} else
			goto play_good_cmds;

	}

	if (rec_data->ft_policy & KGSL_FT_SKIPIB) {

		for (i = 0; i < rec_data->bad_rb_size; i++) {
			if ((rec_data->bad_rb_buffer[i] ==
				CP_HDR_INDIRECT_BUFFER_PFD) &&
				(rec_data->bad_rb_buffer[i+1] ==
				rec_data->ib1)) {

				rec_data->bad_rb_buffer[i] = cp_nop_packet(2);
				rec_data->bad_rb_buffer[i+1] =
							KGSL_NOP_IB_IDENTIFIER;
				rec_data->bad_rb_buffer[i+2] =
							KGSL_NOP_IB_IDENTIFIER;
				break;
			}
		}

		if ((i == (rec_data->bad_rb_size)) || (!rec_data->ib1)) {
			KGSL_DRV_ERR(device, "Bad IB to NOP not found\n");
			rec_data->status = 1;
			goto play_good_cmds;
		}

		ret = _adreno_recovery_resubmit(device, rb, context, rec_data,
				rec_data->bad_rb_buffer, rec_data->bad_rb_size);

		if (ret) {
			KGSL_DRV_ERR(device, "NOP faulty IB status: 1\n");
			rec_data->status = 1;
		} else {
			rec_data->status = 0;
			goto play_good_cmds;
		}
	}

	if (rec_data->ft_policy & KGSL_FT_SKIPFRAME) {

		for (i = 0; i < rec_data->bad_rb_size; i++) {
			if (rec_data->bad_rb_buffer[i] ==
				KGSL_END_OF_FRAME_IDENTIFIER) {
				rec_data->bad_rb_buffer[0] = cp_nop_packet(i);
				break;
			}
		}

		/* EOF not found in RB, discard till EOF in
		   next IB submission */
		if (i == rec_data->bad_rb_size) {
			adreno_context->flags |= CTXT_FLAGS_SKIP_EOF;
			KGSL_DRV_INFO(device,
			"EOF not found in RB, skip next issueib till EOF\n");
			rec_data->bad_rb_buffer[0] = cp_nop_packet(i);
		}

		ret = _adreno_recovery_resubmit(device, rb, context, rec_data,
				rec_data->bad_rb_buffer, rec_data->bad_rb_size);

		if (ret) {
			KGSL_DRV_ERR(device, "Skip EOF unsuccessful\n");
			rec_data->status = 1;
		} else {
			rec_data->status = 0;
			goto play_good_cmds;
		}
	}

play_good_cmds:

	if (rec_data->status)
		KGSL_DRV_ERR(device, "Bad context commands failed\n");
	else {

		if (adreno_context) {
			adreno_context->flags = (adreno_context->flags &
			~CTXT_FLAGS_GPU_HANG) | CTXT_FLAGS_GPU_HANG_RECOVERED;
		}
		adreno_dev->drawctxt_active = last_active_ctx;
	}

	ret = _adreno_recovery_resubmit(device, rb, context, rec_data,
			rec_data->good_rb_buffer, rec_data->good_rb_size);

	if (ret) {
		/* If we fail here we can try to invalidate another
		 * context and try fault tolerance again */
		ret = -EAGAIN;
		KGSL_DRV_ERR(device, "Playing good commands unsuccessful\n");
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

		ret = _adreno_recover_hang(device, rec_data);

		if (-EAGAIN == ret) {
			/* setup new recovery parameters and retry, this
			 * means more than 1 contexts are causing hang */
			adreno_destroy_recovery_data(rec_data);
			ret = adreno_setup_recovery_data(device, rec_data);
			if (ret)
				goto done;
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

	/* switch to NULL ctxt */
	if (adreno_dev->drawctxt_active != NULL)
		adreno_drawctxt_switch(adreno_dev, NULL, 0);

done:
	adreno_set_max_ts_for_bad_ctxs(device);
	adreno_mark_context_status(device, ret);
	KGSL_DRV_ERR(device, "policy 0x%X status 0x%x\n",
			rec_data->ft_policy, ret);
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

		if (!result) {
			result = adreno_recover_hang(device, &rec_data);
			adreno_destroy_recovery_data(&rec_data);
		}
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
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

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
				adreno_dev->fast_hang_detect = 1;
				kgsl_pwrscale_enable(device);
			} else {
				device->pwrctrl.nap_allowed = false;
				adreno_dev->fast_hang_detect = 0;
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

	do {
		if (time_after(jiffies, wait)) {
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
	wait_time = jiffies + msecs_to_jiffies(ADRENO_IDLE_TIMEOUT);
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
		wait_time = jiffies + msecs_to_jiffies(ADRENO_IDLE_TIMEOUT);
		goto retry;
	}
	return -ETIMEDOUT;
}

/**
 * is_adreno_rbbm_status_idle - Check if GPU core is idle by probing
 * rbbm_status register
 * @device - Pointer to the GPU device whose idle status is to be
 * checked
 * @returns - Returns whether the core is idle (based on rbbm_status)
 * false if the core is active, true if the core is idle
 */
static bool is_adreno_rbbm_status_idle(struct kgsl_device *device)
{
	unsigned int reg_rbbm_status;
	bool status = false;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Is the core idle? */
	adreno_regread(device,
		adreno_dev->gpudev->reg_rbbm_status,
		&reg_rbbm_status);

	if (adreno_is_a2xx(adreno_dev)) {
		if (reg_rbbm_status == 0x110)
			status = true;
	} else {
		if (!(reg_rbbm_status & 0x80000000))
			status = true;
	}
	return status;
}

static unsigned int adreno_isidle(struct kgsl_device *device)
{
	int status = false;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

	WARN_ON(device->state == KGSL_STATE_INIT);
	/* If the device isn't active, don't force it on. */
	if (device->state == KGSL_STATE_ACTIVE) {
		/* Is the ring buffer is empty? */
		GSL_RB_GET_READPTR(rb, &rb->rptr);
		if (!device->active_cnt && (rb->rptr == rb->wptr)) {
			/*
			 * Are there interrupts pending? If so then pretend we
			 * are not idle - this avoids the possiblity that we go
			 * to a lower power state without handling interrupts
			 * first.
			 */

			if (!adreno_dev->gpudev->irq_pending(adreno_dev)) {
				/* Is the core idle? */
				status = is_adreno_rbbm_status_idle(device);
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

		if (kgsl_mmu_pt_equal(&device->mmu, adreno_context->pagetable,
					pt_base)) {
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

	entry = kgsl_get_mem_entry(device, pt_base, gpuaddr, size);

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

/**
 * adreno_read - General read function to read adreno device memory
 * @device - Pointer to the GPU device struct (for adreno device)
 * @base - Base address (kernel virtual) where the device memory is mapped
 * @offsetwords - Offset in words from the base address, of the memory that
 * is to be read
 * @value - Value read from the device memory
 * @mem_len - Length of the device memory mapped to the kernel
 */
static void adreno_read(struct kgsl_device *device, void *base,
		unsigned int offsetwords, unsigned int *value,
		unsigned int mem_len)
{

	unsigned int *reg;
	BUG_ON(offsetwords*sizeof(uint32_t) >= mem_len);
	reg = (unsigned int *)(base + (offsetwords << 2));

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	/*ensure this read finishes before the next one.
	 * i.e. act like normal readl() */
	*value = __raw_readl(reg);
	rmb();
}

/**
 * adreno_regread - Used to read adreno device registers
 * @offsetwords - Word (4 Bytes) offset to the register to be read
 * @value - Value read from device register
 */
void adreno_regread(struct kgsl_device *device, unsigned int offsetwords,
	unsigned int *value)
{
	adreno_read(device, device->reg_virt, offsetwords, value,
						device->reg_len);
}

/**
 * adreno_shadermem_regread - Used to read GPU (adreno) shader memory
 * @device - GPU device whose shader memory is to be read
 * @offsetwords - Offset in words, of the shader memory address to be read
 * @value - Pointer to where the read shader mem value is to be stored
 */
void adreno_shadermem_regread(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int *value)
{
	adreno_read(device, device->shader_mem_virt, offsetwords, value,
					device->shader_mem_len);
}

void adreno_regwrite(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int value)
{
	unsigned int *reg;

	BUG_ON(offsetwords*sizeof(uint32_t) >= device->reg_len);

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	trace_kgsl_regwrite(device, offsetwords, value);

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

static unsigned int adreno_check_hw_ts(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp)
{
	int status = 0;
	unsigned int ref_ts, enableflag;
	unsigned int context_id = _get_context_id(context);

	/*
	 * If the context ID is invalid, we are in a race with
	 * the context being destroyed by userspace so bail.
	 */
	if (context_id == KGSL_CONTEXT_INVALID) {
		KGSL_DRV_WARN(device, "context was detached");
		return -EINVAL;
	}

	status = kgsl_check_timestamp(device, context, timestamp);
	if (status)
		return status;

	kgsl_sharedmem_readl(&device->memstore, &enableflag,
			KGSL_MEMSTORE_OFFSET(context_id, ts_cmp_enable));
	/*
	 * Barrier is needed here to make sure the read from memstore
	 * has posted
	 */

	mb();

	if (enableflag) {
		kgsl_sharedmem_readl(&device->memstore, &ref_ts,
				KGSL_MEMSTORE_OFFSET(context_id,
					ref_wait_ts));

		/* Make sure the memstore read has posted */
		mb();
		if (timestamp_cmp(ref_ts, timestamp) >= 0) {
			kgsl_sharedmem_writel(&device->memstore,
					KGSL_MEMSTORE_OFFSET(context_id,
						ref_wait_ts), timestamp);
			/* Make sure the memstore write is posted */
			wmb();
		}
	} else {
		kgsl_sharedmem_writel(&device->memstore,
				KGSL_MEMSTORE_OFFSET(context_id,
					ref_wait_ts), timestamp);
		enableflag = 1;
		kgsl_sharedmem_writel(&device->memstore,
				KGSL_MEMSTORE_OFFSET(context_id,
					ts_cmp_enable), enableflag);

		/* Make sure the memstore write gets posted */
		wmb();

		/*
		 * submit a dummy packet so that even if all
		 * commands upto timestamp get executed we will still
		 * get an interrupt
		 */

		if (context && device->state != KGSL_STATE_SLUMBER) {
			adreno_ringbuffer_issuecmds(device, context->devctxt,
					KGSL_CMD_FLAGS_NONE, NULL, 0);
		}
	}

	return 0;
}

/* Return 1 if the event timestmp has already passed, 0 if it was marked */
static int adreno_next_event(struct kgsl_device *device,
		struct kgsl_event *event)
{
	return adreno_check_hw_ts(device, event->context, event->timestamp);
}

static int adreno_check_interrupt_timestamp(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp)
{
	int status;

	mutex_lock(&device->mutex);
	status = adreno_check_hw_ts(device, context, timestamp);
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
	static unsigned long next_hang_detect_time;

	if (!adreno_dev->fast_hang_detect)
		return 0;

	if (is_adreno_rbbm_status_idle(device)) {

		/*
		 * On A2XX if the RPTR != WPTR and the device is idle, then
		 * the last write to WPTR probably failed to latch so write it
		 * again
		 */

		if (adreno_is_a2xx(adreno_dev)) {
			unsigned int rptr;
			adreno_regread(device, REG_CP_RB_RPTR, &rptr);
			if (rptr != adreno_dev->ringbuffer.wptr)
				adreno_regwrite(device, REG_CP_RB_WPTR,
					adreno_dev->ringbuffer.wptr);
		}

		return 0;
	}

	/*
	 * Time interval between hang detection should be KGSL_TIMEOUT_PART
	 * or more, if next hang detection is requested < KGSL_TIMEOUT_PART
	 * from the last time do nothing.
	 */
	if ((next_hang_detect_time) &&
		(time_before(jiffies, next_hang_detect_time)))
			return 0;
	else
		next_hang_detect_time = (jiffies +
			msecs_to_jiffies(KGSL_TIMEOUT_PART-1));

	for (i = 0; i < hang_detect_regs_count; i++) {

		if (hang_detect_regs[i] == 0)
			continue;

		adreno_regread(device, hang_detect_regs[i],
					   &curr_reg_val[i]);
		if (curr_reg_val[i] != prev_reg_val[i]) {
			prev_reg_val[i] = curr_reg_val[i];
			hang_detected = 0;
		}
	}

	return hang_detected;
}

/**
 * adreno_handle_hang - Process a hang detected in adreno_waittimestamp
 * @device - pointer to a KGSL device structure
 * @context - pointer to the active KGSL context
 * @timestamp - the timestamp that the process was waiting for
 *
 * Process a possible GPU hang and try to recover from it cleanly
 */
static int adreno_handle_hang(struct kgsl_device *device,
	struct kgsl_context *context, unsigned int timestamp)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int context_id = _get_context_id(context);
	unsigned int ts_issued;
	unsigned int rptr;

	/* Do one last check to see if we somehow made it through */
	if (kgsl_check_timestamp(device, context, timestamp))
		return 0;

	ts_issued = adreno_dev->ringbuffer.timestamp[context_id];

	adreno_regread(device, REG_CP_RB_RPTR, &rptr);

	/* Make sure timestamp check finished before triggering a hang */
	mb();

	KGSL_DRV_ERR(device,
		     "Device hang detected while waiting for timestamp: "
		     "<%d:0x%x>, last submitted timestamp: <%d:0x%x>, "
		     "retired timestamp: <%d:0x%x>, wptr: 0x%x, rptr: 0x%x\n",
		      context_id, timestamp, context_id, ts_issued, context_id,
			kgsl_readtimestamp(device, context,
			KGSL_TIMESTAMP_RETIRED),
		      adreno_dev->ringbuffer.wptr, rptr);

	/* Return 0 after a successful recovery */
	if (!adreno_dump_and_recover(device))
		return 0;

	return -ETIMEDOUT;
}

static int _check_pending_timestamp(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int context_id = _get_context_id(context);
	unsigned int ts_issued;

	if (context_id == KGSL_CONTEXT_INVALID)
		return -EINVAL;

	ts_issued = adreno_dev->ringbuffer.timestamp[context_id];

	if (timestamp_cmp(timestamp, ts_issued) <= 0)
		return 0;

	if (context && !context->wait_on_invalid_ts) {
		KGSL_DRV_ERR(device, "Cannot wait for invalid ts <%d:0x%x>, last issued ts <%d:0x%x>\n",
			context_id, timestamp, context_id, ts_issued);

			/* Only print this message once */
			context->wait_on_invalid_ts = true;
	}

	return -EINVAL;
}

/**
 * adreno_waittimestamp - sleep while waiting for the specified timestamp
 * @device - pointer to a KGSL device structure
 * @context - pointer to the active kgsl context
 * @timestamp - GPU timestamp to wait for
 * @msecs - amount of time to wait (in milliseconds)
 *
 * Wait 'msecs' milliseconds for the specified timestamp to expire. Wake up
 * every KGSL_TIMEOUT_PART milliseconds to check for a device hang and process
 * one if it happened.  Otherwise, spend most of our time in an interruptible
 * wait for the timestamp interrupt to be processed.  This function must be
 * called with the mutex already held.
 */
static int adreno_waittimestamp(struct kgsl_device *device,
				struct kgsl_context *context,
				unsigned int timestamp,
				unsigned int msecs)
{
	static unsigned int io_cnt;
	struct adreno_context *adreno_ctx = context ? context->devctxt : NULL;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int context_id = _get_context_id(context);
	unsigned int prev_reg_val[hang_detect_regs_count];
	unsigned int time_elapsed = 0;
	unsigned int wait;
	int ts_compare = 1;
	int io, ret = -ETIMEDOUT;

	/* Get out early if the context has already been destroyed */

	if (context_id == KGSL_CONTEXT_INVALID) {
		KGSL_DRV_WARN(device, "context was detached");
		return -EINVAL;
	}

	/*
	 * Check to see if the requested timestamp is "newer" then the last
	 * timestamp issued. If it is complain once and return error.  Only
	 * print the message once per context so that badly behaving
	 * applications don't spam the logs
	 */

	if (adreno_ctx && !(adreno_ctx->flags & CTXT_FLAGS_USER_GENERATED_TS)) {
		if (_check_pending_timestamp(device, context, timestamp))
			return -EINVAL;

		/* Reset the invalid timestamp flag on a valid wait */
		context->wait_on_invalid_ts = false;
	}


	/* Clear the registers used for hang detection */
	memset(prev_reg_val, 0, sizeof(prev_reg_val));

	/*
	 * On the first time through the loop only wait 100ms.
	 * this gives enough time for the engine to start moving and oddly
	 * provides better hang detection results than just going the full
	 * KGSL_TIMEOUT_PART right off the bat. The exception to this rule
	 * is if msecs happens to be < 100ms then just use the full timeout
	 */

	wait = 100;

	do {
		long status;

		/*
		 * if the timestamp happens while we're not
		 * waiting, there's a chance that an interrupt
		 * will not be generated and thus the timestamp
		 * work needs to be queued.
		 */

		if (kgsl_check_timestamp(device, context, timestamp)) {
			queue_work(device->work_queue, &device->ts_expired_ws);
			ret = 0;
			break;
		}

		/* Check to see if the GPU is hung */
		if (adreno_hang_detect(device, prev_reg_val)) {
			ret = adreno_handle_hang(device, context, timestamp);
			break;
		}

		/*
		 * For proper power accounting sometimes we need to call
		 * io_wait_interruptible_timeout and sometimes we need to call
		 * plain old wait_interruptible_timeout. We call the regular
		 * timeout N times out of 100, where N is a number specified by
		 * the current power level
		 */

		io_cnt = (io_cnt + 1) % 100;
		io = (io_cnt < pwr->pwrlevels[pwr->active_pwrlevel].io_fraction)
			? 0 : 1;

		mutex_unlock(&device->mutex);

		/* Wait for a timestamp event */
		status = kgsl_wait_event_interruptible_timeout(
			device->wait_queue,
			adreno_check_interrupt_timestamp(device, context,
				timestamp), msecs_to_jiffies(wait), io);

		mutex_lock(&device->mutex);

		/*
		 * If status is non zero then either the condition was satisfied
		 * or there was an error.  In either event, this is the end of
		 * the line for us
		 */

		if (status != 0) {
			ret = (status > 0) ? 0 : (int) status;
			break;
		}
		time_elapsed += wait;

		/* If user specified timestamps are being used, wait at least
		 * KGSL_SYNCOBJ_SERVER_TIMEOUT msecs for the user driver to
		 * issue a IB for a timestamp before checking to see if the
		 * current timestamp we are waiting for is valid or not
		 */

		if (ts_compare && (adreno_ctx &&
			(adreno_ctx->flags & CTXT_FLAGS_USER_GENERATED_TS))) {
			if (time_elapsed > KGSL_SYNCOBJ_SERVER_TIMEOUT) {
				ret = _check_pending_timestamp(device, context,
					timestamp);
				if (ret)
					break;

				/* Don't do this check again */
				ts_compare = 0;

				/*
				 * Reset the invalid timestamp flag on a valid
				 * wait
				 */
				context->wait_on_invalid_ts = false;
			}
		}

		/*
		 * We want to wait the floor of KGSL_TIMEOUT_PART
		 * and (msecs - time_elapsed).
		 */

		if (KGSL_TIMEOUT_PART < (msecs - time_elapsed))
			wait = KGSL_TIMEOUT_PART;
		else
			wait = (msecs - time_elapsed);

	} while (!msecs || time_elapsed < msecs);

	return ret;
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
		kgsl_sharedmem_readl(&device->memstore, &timestamp,
			KGSL_MEMSTORE_OFFSET(context_id, soptimestamp));
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
	.next_event = adreno_next_event,
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
