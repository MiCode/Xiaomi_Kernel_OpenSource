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
	0x0000, 0x0002, /* RBBM_HW_VERSION - RBBM_HW_CONFIGURATION */
	0x0020, 0x0020, /* RBBM_CLOCK_CTL */
	0x0021, 0x0021, /* RBBM_SP_HYST_CNT */
	0x0023, 0x0024, /* RBBM_AHB_CTL0 - RBBM_AHB_CTL1 */
	0x0026, 0x0026, /* RBBM_RB_SUB_BLOCK_SEL_CTL */
	0x0028, 0x0034, /* RBBM_RAM_ACC_63_32 - RBBM_INTERFACE_HANG_MASK_CTL4 */
	0x0037, 0x003f, /* RBBM_INT_0_MASK - RBBM_AHB_DEBUG_CTL */
	0x0041, 0x0045, /* RBBM_VBIF_DEBUG_CTL - BLOCK_SW_RESET_CMD */
	0x0047, 0x0049, /* RBBM_RESET_CYCLES - RBBM_EXT_TRACE_BUS_CTL */
	0x009c, 0x0170, /* RBBM_PERFCTR_CP_0_LO - RBBM_PERFCTR_CTL */
	0x0174, 0x0182, /* RBBM_PERFCTR_LOAD_VALUE_LO - RBBM_CLOCK_STATUS */
	0x0189, 0x019f, /* RBBM_AHB_STATUS - RBBM_INTERFACE_RRDY_STATUS5 */

	0x0206, 0x0217, /* CP_IB1_BASE - CP_ME_RB_DONE_DATA */
	0x0219, 0x0219, /* CP_QUEUE_THRESH2 */
	0x021b, 0x021b, /* CP_MERCIU_SIZE */
	0x0228, 0x0229, /* CP_SCRATCH_UMASK - CP_SCRATCH_ADDR */
	0x022a, 0x022c, /* CP_PREEMPT - CP_CNTL */
	0x022e, 0x022e, /* CP_DEBUG */
	0x0231, 0x0232, /* CP_DEBUG_ECO_CONTROL - CP_DRAW_STATE_ADDR */
	0x0240, 0x0250, /* CP_PROTECT_REG_0 - CP_PROTECT_CTRL */
	0x04c0, 0x04ce, /* CP_ST_BASE - CP_STQ_AVAIL */
	0x04d0, 0x04d0, /* CP_MERCIU_STAT */
	0x04d2, 0x04dd, /* CP_WFI_PEND_CTR - CP_EVENTS_IN_FLIGHT */
	0x0500, 0x050b, /* CP_PERFCTR_CP_SEL_0 - CP_PERFCOMBINER_SELECT */
	0x0578, 0x058f, /* CP_SCRATCH_REG0 - CP_SCRATCH_REG23 */

	0x0c00, 0x0c03, /* VSC_BIN_SIZE - VSC_DEBUG_ECO_CONTROL */
	0x0c08, 0x0c41, /* VSC_PIPE_CONFIG_0 - VSC_PIPE_PARTIAL_POSN_1 */
	0x0c50, 0x0c51, /* VSC_PERFCTR_VSC_SEL_0 - VSC_PERFCTR_VSC_SEL_1 */

	0x0e64, 0x0e68, /* VPC_DEBUG_ECO_CONTROL - VPC_PERFCTR_VPC_SEL_3 */
	0x2140, 0x216e, /* VPC_ATTR - VPC_SO_FLUSH_WADDR_3 - ctx0 */
	0x2540, 0x256e, /* VPC_ATTR - VPC_SO_FLUSH_WADDR_3 - ctx1 */

	0x0f00, 0x0f0b, /* TPL1_DEBUG_ECO_CONTROL - TPL1_PERFCTR_TP_SEL_7 */
	/* TPL1_TP_TEX_OFFSET - TPL1_TP_CS_TEXMEMOBJ_BASE_ADDR - ctx0 */
	0x2380, 0x23a6,
	/* TPL1_TP_TEX_OFFSET - TPL1_TP_CS_TEXMEMOBJ_BASE_ADDR - ctx1 */
	0x2780, 0x27a6,

	0x0ec0, 0x0ecf, /* SP_VS_STATUS - SP_PERFCTR_SP_SEL_11 */
	0x22c0, 0x22c1, /* SP_SP_CTRL - SP_INSTR_CACHE_CTRL - ctx0 */
	0x22c4, 0x2360, /* SP_VS_CTRL_0 - SP_GS_LENGTH - ctx0 */
	0x26c0, 0x26c1, /* SP_SP_CTRL - SP_INSTR_CACHE_CTRL - ctx1 */
	0x26c4, 0x2760, /* SP_VS_CTRL_0 - SP_GS_LENGTH - ctx1 */

	0x0cc0, 0x0cd2, /* RB_GMEM_BASE_ADDR - RB_PERFCTR_CCU_SEL_3 */
	0x20a0, 0x213f, /* RB_MODE_CONTROL - RB_VPORT_Z_CLAMP_MAX_15 - ctx0 */
	0x24a0, 0x253f, /* RB_MODE_CONTROL - RB_VPORT_Z_CLAMP_MAX_15 - ctx1 */

	0x0e40, 0x0e4a, /* VFD_DEBUG_CONTROL - VFD_PERFCTR_VFD_SEL_7 */
	0x2200, 0x2204, /* VFD_CONTROL_0 - VFD_CONTROL_4 - ctx 0 */
	0x2208, 0x22a9, /* VFD_INDEX_OFFSET - VFD_DECODE_INSTR_31 - ctx 0 */
	0x2600, 0x2604, /* VFD_CONTROL_0 - VFD_CONTROL_4 - ctx 1 */
	0x2608, 0x26a9, /* VFD_INDEX_OFFSET - VFD_DECODE_INSTR_31 - ctx 1 */

	0x0c80, 0x0c81, /* GRAS_TSE_STATUS - GRAS_DEBUG_ECO_CONTROL */
	0x0c88, 0x0c8b, /* GRAS_PERFCTR_TSE_SEL_0 - GRAS_PERFCTR_TSE_SEL_3 */
	0x2000, 0x2004, /* GRAS_CL_CLIP_CNTL - GRAS_CL_GB_CLIP_ADJ - ctx 0 */
	/* GRAS_CL_VPORT_XOFFSET_0 - GRAS_SC_EXTENT_WINDOW_TL - ctx 0 */
	0x2008, 0x209f,
	0x2400, 0x2404, /* GRAS_CL_CLIP_CNTL - GRAS_CL_GB_CLIP_ADJ - ctx 1 */
	/* GRAS_CL_VPORT_XOFFSET_0 - GRAS_SC_EXTENT_WINDOW_TL - ctx 1 */
	0x2408, 0x249f,

	0x0e80, 0x0e84, /* UCHE_CACHE_MODE_CONTROL - UCHE_TRAP_BASE_HI */
	0x0e88, 0x0e95, /* UCHE_CACHE_STATUS - UCHE_PERFCTR_UCHE_SEL_7 */

	0x0e00, 0x0e00, /* HLSQ_TIMEOUT_THRESHOLD - HLSQ_TIMEOUT_THRESHOLD */
	0x0e04, 0x0e0e, /* HLSQ_DEBUG_ECO_CONTROL - HLSQ_PERF_PIPE_MASK */
	0x23c0, 0x23db, /* HLSQ_CONTROL_0 - HLSQ_UPDATE_CONTROL - ctx 0 */
	0x27c0, 0x27db, /* HLSQ_CONTROL_0 - HLSQ_UPDATE_CONTROL - ctx 1 */

	0x0d00, 0x0d0c, /* PC_BINNING_COMMAND - PC_DRAWCALL_SETUP_OVERRIDE */
	0x0d10, 0x0d17, /* PC_PERFCTR_PC_SEL_0 - PC_PERFCTR_PC_SEL_7 */
	0x21c0, 0x21c6, /* PC_BIN_BASE - PC_RESTART_INDEX - ctx 0 */
	0x21e5, 0x21e7, /* PC_GS_PARAM - PC_HS_PARAM - ctx 0 */
	0x25c0, 0x25c6, /* PC_BIN_BASE - PC_RESTART_INDEX - ctx 1 */
	0x25e5, 0x25e7, /* PC_GS_PARAM - PC_HS_PARAM - ctx 1 */
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
