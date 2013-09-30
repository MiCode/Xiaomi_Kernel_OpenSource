/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include "adreno.h"
#include "a4xx_reg.h"
#include "adreno_a3xx.h"

/*
 * Set of registers to dump for A4XX on postmortem and snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

const unsigned int a4xx_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0004, 0x0021, 0x0023, 0x0024, 0x0026, 0x0026,
	0x0028, 0x002B, 0x002E, 0x0034, 0x0037, 0x0044, 0x0047, 0x0066,
	0x0068, 0x0095, 0x009C, 0x0170, 0x0174, 0x01AF,
	/* CP */
	0x0200, 0x0233, 0x0240, 0x0250, 0x04C0, 0x04DD, 0x0500, 0x050B,
	0x0578, 0x058F,
	/* VSC */
	0x0C00, 0x0C03, 0x0C08, 0x0C41, 0x0C50, 0x0C51,
	/* GRAS */
	0x0C80, 0x0C81, 0x0C88, 0x0C8F,
	/* RB */
	0x0CC0, 0x0CC0, 0x0CC4, 0x0CD2,
	/* PC */
	0x0D00, 0x0D0C, 0x0D10, 0x0D17, 0x0D20, 0x0D23,
	/* HLSQ */
	0x0E00, 0x0E00, 0x0E04, 0x0E0E, 0x0E30, 0x0E30,
	/* VFD */
	0x0E40, 0x0E4A,
	/* VPC */
	0x0E60, 0x0E61, 0x0E63, 0x0E68,
	/* UCHE */
	0x0E80, 0x0E84, 0x0E88, 0x0E95,
	/* SP */
	0x0EC0, 0x0ECF,
	/* TPL1 */
	0x0F00, 0x0F0B,
	/* VMIDMT */
	0x1000, 0x1000, 0x1002, 0x1002, 0x1004, 0x1004, 0x1008, 0x100A,
	0x100C, 0x100D, 0x100F, 0x1010, 0x1012, 0x1016, 0x1024, 0x1024,
	0x1027, 0x1027, 0x1100, 0x1100, 0x1102, 0x1102, 0x1104, 0x1104,
	0x1110, 0x1110, 0x1112, 0x1116, 0x1124, 0x1124, 0x1300, 0x1300,
	0x1380, 0x1380,
	/* GRAS CTX 0 */
	0x2000, 0x2004, 0x2008, 0x2067, 0x2070, 0x2078, 0x207B, 0x216E,
	/* PC CTX 0 */
	0x21C0, 0x21C6, 0x21D0, 0x21D0, 0x21D9, 0x21D9, 0x21E5, 0x21E7,
	/* VFD CTX 0 */
	0x2200, 0x2204, 0x2208, 0x22A9,
	/* SP CTX 0 */
	0x22C0, 0x22C1, 0x22C4, 0x22E5, 0x22E8, 0x22F8, 0x2300, 0x2306,
	0x230C, 0x2312, 0x2318, 0x2339, 0x2340, 0x2360,
	/* TPL1 CTX 0 */
	0x2380, 0x2382, 0x2384, 0x238F, 0x23A0, 0x23A6,
	/* HLSQ CTX 0 */
	0x23C0, 0x23DB,
	/* GRAS CTX 1 */
	0x2400, 0x2404, 0x2408, 0x2467, 0x2470, 0x2478, 0x247B, 0x256E,
	/* PC CTX 1 */
	0x25C0, 0x25C6, 0x25D0, 0x25D0, 0x25D9, 0x25D9, 0x25E5, 0x25E7,
	/* VFD CTX 1 */
	0x2600, 0x2604, 0x2608, 0x26A9,
	/* SP CTX 1 */+
	0x26C0, 0x26C1, 0x26C4, 0x26E5, 0x26E8, 0x26F8, 0x2700, 0x2706,
	0x270C, 0x2712, 0x2718, 0x2739, 0x2740, 0x2760,
	/* TPL1 CTX 1 */
	0x2780, 0x2782, 0x2784, 0x278F, 0x27A0, 0x27A6,
	/* HLSQ CTXT 1 */
	0x27C0, 0x27DB,
	/* XPU */
	0x2C00, 0x2C01, 0x2C10, 0x2C10, 0x2C12, 0x2C16, 0x2C1D, 0x2C20,
	0x2C28, 0x2C28, 0x2C30, 0x2C30, 0x2C32, 0x2C36, 0x2C40, 0x2C40,
	0x2C50, 0x2C50, 0x2C52, 0x2C56, 0x2C80, 0x2C80, 0x2C94, 0x2C95,
	/* VBIF */
	0x3000, 0x3007, 0x300C, 0x3014, 0x3018, 0x301D, 0x3020, 0x3022,
	0x3024, 0x3026, 0x3028, 0x302A, 0x302C, 0x302D, 0x3030, 0x3031,
	0x3034, 0x3036, 0x3038, 0x3038, 0x303C, 0x303D, 0x3040, 0x3040,
	0x3049, 0x3049, 0x3058, 0x3058, 0x305B, 0x3061, 0x3064, 0x3068,
	0x306C, 0x306D, 0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094,
	0x3098, 0x3098, 0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8,
	0x30D0, 0x30D0, 0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100,
	0x3108, 0x3108, 0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120,
	0x3124, 0x3125, 0x3129, 0x3129, 0x3131, 0x3131, 0x330C, 0x330C,
	0x3310, 0x3310, 0x3400, 0x3401, 0x3410, 0x3410, 0x3412, 0x3416,
	0x341D, 0x3420, 0x3428, 0x3428, 0x3430, 0x3430, 0x3432, 0x3436,
	0x3440, 0x3440, 0x3450, 0x3450, 0x3452, 0x3456, 0x3480, 0x3480,
	0x3494, 0x3495, 0x4000, 0x4000, 0x4002, 0x4002, 0x4004, 0x4004,
	0x4008, 0x400A, 0x400C, 0x400D, 0x400F, 0x4012, 0x4014, 0x4016,
	0x401D, 0x401D, 0x4020, 0x4027, 0x4060, 0x4062, 0x4200, 0x4200,
	0x4300, 0x4300, 0x4400, 0x4400, 0x4500, 0x4500, 0x4800, 0x4802,
	0x480F, 0x480F, 0x4811, 0x4811, 0x4813, 0x4813, 0x4815, 0x4816,
	0x482B, 0x482B, 0x4857, 0x4857, 0x4883, 0x4883, 0x48AF, 0x48AF,
	0x48C5, 0x48C5, 0x48E5, 0x48E5, 0x4905, 0x4905, 0x4925, 0x4925,
	0x4945, 0x4945, 0x4950, 0x4950, 0x495B, 0x495B, 0x4980, 0x498E,
	0x4B00, 0x4B00, 0x4C00, 0x4C00, 0x4D00, 0x4D00, 0x4E00, 0x4E00,
	0x4E80, 0x4E80, 0x4F00, 0x4F00, 0x4F08, 0x4F08, 0x4F10, 0x4F10,
	0x4F18, 0x4F18, 0x4F20, 0x4F20, 0x4F30, 0x4F30, 0x4F60, 0x4F60,
	0x4F80, 0x4F81, 0x4F88, 0x4F89, 0x4FEE, 0x4FEE, 0x4FF3, 0x4FF3,
	0x6000, 0x6001, 0x6008, 0x600F, 0x6014, 0x6016, 0x6018, 0x601B,
	0x61FD, 0x61FD, 0x623C, 0x623C, 0x6380, 0x6380, 0x63A0, 0x63A0,
	0x63C0, 0x63C1, 0x63C8, 0x63C9, 0x63D0, 0x63D4, 0x63D6, 0x63D6,
	0x63EE, 0x63EE, 0x6400, 0x6401, 0x6408, 0x640F, 0x6414, 0x6416,
	0x6418, 0x641B, 0x65FD, 0x65FD, 0x663C, 0x663C, 0x6780, 0x6780,
	0x67A0, 0x67A0, 0x67C0, 0x67C1, 0x67C8, 0x67C9, 0x67D0, 0x67D4,
	0x67D6, 0x67D6, 0x67EE, 0x67EE, 0x6800, 0x6801, 0x6808, 0x680F,
	0x6814, 0x6816, 0x6818, 0x681B, 0x69FD, 0x69FD, 0x6A3C, 0x6A3C,
	0x6B80, 0x6B80, 0x6BA0, 0x6BA0, 0x6BC0, 0x6BC1, 0x6BC8, 0x6BC9,
	0x6BD0, 0x6BD4, 0x6BD6, 0x6BD6, 0x6BEE, 0x6BEE,
};

const unsigned int a4xx_registers_count = ARRAY_SIZE(a4xx_registers) / 2;

static int a4xx_drawctxt_create(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt)
{
	int ret = 0;
	struct kgsl_device *device = &adreno_dev->dev;

	if (!(drawctxt->flags & CTXT_FLAGS_PREAMBLE)) {
		/* This option is not supported on a4xx */
		KGSL_DRV_ERR(device,
			"Preambles required for A4XX draw contexts\n");
		ret = -EPERM;
		goto done;
	}

	if (!(drawctxt->flags & CTXT_FLAGS_NOGMEMALLOC)) {
		/* This option is not supported on a4xx */
		KGSL_DRV_ERR(device,
			"Cannot create context with gmemalloc\n");
		ret = -EPERM;
	}

done:
	return ret;
}

static int a4xx_drawctxt_restore(struct adreno_device *adreno_dev,
			      struct adreno_context *context)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int cmds[5];
	int ret;

	if (context == NULL) {
		/* No context - set the default pagetable and thats it */
		unsigned int id;
		/*
		 * If there isn't a current context, the kgsl_mmu_setstate
		 * will use the CPU path so we don't need to give
		 * it a valid context id.
		 */
		id = (adreno_dev->drawctxt_active != NULL)
			? adreno_dev->drawctxt_active->base.id
			: KGSL_CONTEXT_INVALID;
		kgsl_mmu_setstate(&device->mmu, device->mmu.defaultpagetable,
				  id);
		return 0;
	}

	cmds[0] = cp_nop_packet(1);
	cmds[1] = KGSL_CONTEXT_TO_MEM_IDENTIFIER;
	cmds[2] = cp_type3_packet(CP_MEM_WRITE, 2);
	cmds[3] = device->memstore.gpuaddr +
		KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL, current_context);
	cmds[4] = context->base.id;
	ret = adreno_ringbuffer_issuecmds(device, context, KGSL_CMD_FLAGS_NONE,
					cmds, 5);
	if (ret)
		return ret;
	ret = kgsl_mmu_setstate(&device->mmu,
			context->base.proc_priv->pagetable,
			context->base.id);
	return ret;
}

static const struct adreno_vbif_data a420_vbif[] = {
	{ A4XX_VBIF_ABIT_SORT, 0x0001001F },
	{ A4XX_VBIF_ABIT_SORT_CONF, 0x000000A4 },
	{ A4XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000001 },
	{ A4XX_VBIF_IN_RD_LIM_CONF0, 0x18181818 },
	{ A4XX_VBIF_IN_RD_LIM_CONF1, 0x00000018 },
	{ A4XX_VBIF_IN_WR_LIM_CONF0, 0x18181818 },
	{ A4XX_VBIF_IN_WR_LIM_CONF1, 0x00000018 },
	{ A4XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000003 },
	{0, 0},
};

const struct adreno_vbif_platform a4xx_vbif_platforms[] = {
	{ adreno_is_a420, a420_vbif },
};

static void a4xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;

	adreno_vbif_start(device, a4xx_vbif_platforms,
			ARRAY_SIZE(a4xx_vbif_platforms));
	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, A4XX_RBBM_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/* Tune the hystersis counters for SP and CP idle detection */
	kgsl_regwrite(device, A4XX_RBBM_SP_HYST_CNT, 0x10);
	kgsl_regwrite(device, A4XX_RBBM_WAIT_IDLE_CLOCKS_CTL, 0x10);

	/*
	 * Enable the RBBM error reporting bits.  This lets us get
	 * useful information on failure
	 */

	kgsl_regwrite(device, A4XX_RBBM_AHB_CTL0, 0x00000001);

	/* Enable AHB error reporting */
	kgsl_regwrite(device, A4XX_RBBM_AHB_CTL1, 0xA6FFFFFF);

	/*
	 * Turn on hang detection - this spews a lot of useful information
	 * into the RBBM registers on a hang
	 */

	kgsl_regwrite(device, A4XX_RBBM_INTERFACE_HANG_INT_CTL,
			(1 << 16) | 0xFFF);

	/* Set the OCMEM base address for A4XX */
	kgsl_regwrite(device, A4XX_RB_GMEM_BASE_ADDR,
			(unsigned int)(adreno_dev->ocmem_base >> 14));
}

/* Register offset defines for A4XX, in order of enum adreno_regs */
static unsigned int a4xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_REG_CP_DEBUG, A4XX_CP_DEBUG),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_WADDR, A4XX_CP_ME_RAM_WADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_DATA, A4XX_CP_ME_RAM_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PFP_UCODE_DATA, A4XX_CP_PFP_UCODE_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PFP_UCODE_ADDR, A4XX_CP_PFP_UCODE_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_WFI_PEND_CTR, A4XX_CP_WFI_PEND_CTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A4XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR, A4XX_CP_RB_RPTR_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A4XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A4XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PROTECT_CTRL, A4XX_CP_PROTECT_CTRL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, A4XX_CP_ME_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_CNTL, A4XX_CP_RB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, A4XX_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, A4XX_CP_IB1_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, A4XX_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, A4XX_CP_IB2_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_TIMESTAMP, A4XX_CP_SCRATCH_REG0),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_RADDR, A4XX_CP_ME_RAM_RADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_SCRATCH_ADDR, A4XX_CP_SCRATCH_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_SCRATCH_UMSK, A4XX_CP_SCRATCH_UMASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A4XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_CTL, A4XX_RBBM_PERFCTR_CTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
					A4XX_RBBM_PERFCTR_LOAD_CMD0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
					A4XX_RBBM_PERFCTR_LOAD_CMD1),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
				A4XX_RBBM_PERFCTR_LOAD_CMD2),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_PWR_1_LO,
					A4XX_RBBM_PERFCTR_PWR_1_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A4XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_STATUS, A4XX_RBBM_INT_0_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_AHB_ERROR_STATUS,
					A4XX_RBBM_AHB_ERROR_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_AHB_CMD, A4XX_RBBM_AHB_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_VPC_DEBUG_RAM_SEL,
					A4XX_VPC_DEBUG_RAM_SEL),
	ADRENO_REG_DEFINE(ADRENO_REG_VPC_DEBUG_RAM_READ,
					A4XX_VPC_DEBUG_RAM_READ),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_CLEAR_CMD,
				A4XX_RBBM_INT_CLEAR_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_VSC_PIPE_DATA_ADDRESS_0,
				A4XX_VSC_PIPE_DATA_ADDRESS_0),
	ADRENO_REG_DEFINE(ADRENO_REG_VSC_PIPE_DATA_LENGTH_7,
					A4XX_VSC_PIPE_DATA_LENGTH_7),
	ADRENO_REG_DEFINE(ADRENO_REG_VSC_SIZE_ADDRESS, A4XX_VSC_SIZE_ADDRESS),
	ADRENO_REG_DEFINE(ADRENO_REG_VFD_CONTROL_0, A4XX_VFD_CONTROL_0),
	ADRENO_REG_DEFINE(ADRENO_REG_VFD_FETCH_INSTR_0_0,
					A4XX_VFD_FETCH_INSTR_0_0),
	ADRENO_REG_DEFINE(ADRENO_REG_VFD_FETCH_INSTR_1_F,
					A4XX_VFD_FETCH_INSTR_1_31),
	ADRENO_REG_DEFINE(ADRENO_REG_SP_VS_PVT_MEM_ADDR_REG,
				A4XX_SP_VS_PVT_MEM_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_SP_FS_PVT_MEM_ADDR_REG,
				A4XX_SP_FS_PVT_MEM_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_SP_VS_OBJ_START_REG,
				A4XX_SP_VS_OBJ_START),
	ADRENO_REG_DEFINE(ADRENO_REG_SP_FS_OBJ_START_REG,
				A4XX_SP_FS_OBJ_START),
};

const struct adreno_reg_offsets a4xx_reg_offsets = {
	.offsets = a4xx_register_offsets,
	.offset_0 = ADRENO_REG_REGISTER_MAX,
};

struct adreno_gpudev adreno_a4xx_gpudev = {
	.reg_offsets = &a4xx_reg_offsets,

	.ctxt_create = a4xx_drawctxt_create,
	.ctxt_restore = a4xx_drawctxt_restore,
	.rb_init = a3xx_rb_init,
	.irq_control = a3xx_irq_control,
	.irq_handler = a3xx_irq_handler,
	.irq_pending = a3xx_irq_pending,
	.busy_cycles = a3xx_busy_cycles,
	.start = a4xx_start,
};
