/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>

#include "kgsl.h"
#include "adreno.h"
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"
#include "a3xx_reg.h"

/* Simple macro to facilitate bit setting in the gmem2sys and sys2gmem
 * functions.
 */

#define _SET(_shift, _val) ((_val) << (_shift))

/*
 ****************************************************************************
 *
 * Context state shadow structure:
 *
 * +---------------------+------------+-------------+---------------------+---+
 * | ALU Constant Shadow | Reg Shadow | C&V Buffers | Shader Instr Shadow |Tex|
 * +---------------------+------------+-------------+---------------------+---+
 *
 *		 8K - ALU Constant Shadow (8K aligned)
 *		 4K - H/W Register Shadow (8K aligned)
 *		 5K - Command and Vertex Buffers
 *		 8K - Shader Instruction Shadow
 *		 ~6K - Texture Constant Shadow
 *
 *
 ***************************************************************************
 */

/* Sizes of all sections in state shadow memory */
#define ALU_SHADOW_SIZE      (8*1024) /* 8KB */
#define REG_SHADOW_SIZE      (4*1024) /* 4KB */
#define CMD_BUFFER_SIZE      (5*1024) /* 5KB */
#define TEX_SIZE_MEM_OBJECTS 896      /* bytes */
#define TEX_SIZE_MIPMAP      1936     /* bytes */
#define TEX_SIZE_SAMPLER_OBJ 256      /* bytes */
#define TEX_SHADOW_SIZE                            \
	((TEX_SIZE_MEM_OBJECTS + TEX_SIZE_MIPMAP + \
	TEX_SIZE_SAMPLER_OBJ)*2) /* ~6KB */
#define SHADER_SHADOW_SIZE   (8*1024) /* 8KB */

/* Total context size, excluding GMEM shadow */
#define CONTEXT_SIZE                         \
	(ALU_SHADOW_SIZE+REG_SHADOW_SIZE +   \
	CMD_BUFFER_SIZE+SHADER_SHADOW_SIZE + \
	TEX_SHADOW_SIZE)

/* Offsets to different sections in context shadow memory */
#define REG_OFFSET ALU_SHADOW_SIZE
#define CMD_OFFSET (REG_OFFSET+REG_SHADOW_SIZE)
#define SHADER_OFFSET (CMD_OFFSET+CMD_BUFFER_SIZE)
#define TEX_OFFSET (SHADER_OFFSET+SHADER_SHADOW_SIZE)
#define VS_TEX_OFFSET_MEM_OBJECTS TEX_OFFSET
#define VS_TEX_OFFSET_MIPMAP (VS_TEX_OFFSET_MEM_OBJECTS+TEX_SIZE_MEM_OBJECTS)
#define VS_TEX_OFFSET_SAMPLER_OBJ (VS_TEX_OFFSET_MIPMAP+TEX_SIZE_MIPMAP)
#define FS_TEX_OFFSET_MEM_OBJECTS \
	(VS_TEX_OFFSET_SAMPLER_OBJ+TEX_SIZE_SAMPLER_OBJ)
#define FS_TEX_OFFSET_MIPMAP (FS_TEX_OFFSET_MEM_OBJECTS+TEX_SIZE_MEM_OBJECTS)
#define FS_TEX_OFFSET_SAMPLER_OBJ (FS_TEX_OFFSET_MIPMAP+TEX_SIZE_MIPMAP)

/* The offset for fragment shader data in HLSQ context */
#define SSIZE (16*1024)

#define HLSQ_SAMPLER_OFFSET 0x000
#define HLSQ_MEMOBJ_OFFSET  0x400
#define HLSQ_MIPMAP_OFFSET  0x800

#ifdef GSL_USE_A3XX_HLSQ_SHADOW_RAM
/* Use shadow RAM */
#define HLSQ_SHADOW_BASE		(0x10000+SSIZE*2)
#else
/* Use working RAM */
#define HLSQ_SHADOW_BASE		0x10000
#endif

#define REG_TO_MEM_LOOP_COUNT_SHIFT	15

#define BUILD_PC_DRAW_INITIATOR(prim_type, source_select, index_size, \
	vis_cull_mode) \
	(((prim_type)      << PC_DRAW_INITIATOR_PRIM_TYPE) | \
	((source_select)   << PC_DRAW_INITIATOR_SOURCE_SELECT) | \
	((index_size & 1)  << PC_DRAW_INITIATOR_INDEX_SIZE) | \
	((index_size >> 1) << PC_DRAW_INITIATOR_SMALL_INDEX) | \
	((vis_cull_mode)   << PC_DRAW_INITIATOR_VISIBILITY_CULLING_MODE) | \
	(1                 << PC_DRAW_INITIATOR_PRE_DRAW_INITIATOR_ENABLE))

/*
 * List of context registers (starting from dword offset 0x2000).
 * Each line contains start and end of a range of registers.
 */
static const unsigned int context_register_ranges[] = {
	A3XX_GRAS_CL_CLIP_CNTL, A3XX_GRAS_CL_CLIP_CNTL,
	A3XX_GRAS_CL_GB_CLIP_ADJ, A3XX_GRAS_CL_GB_CLIP_ADJ,
	A3XX_GRAS_CL_VPORT_XOFFSET, A3XX_GRAS_CL_VPORT_ZSCALE,
	A3XX_GRAS_SU_POINT_MINMAX, A3XX_GRAS_SU_POINT_SIZE,
	A3XX_GRAS_SU_POLY_OFFSET_SCALE, A3XX_GRAS_SU_POLY_OFFSET_OFFSET,
	A3XX_GRAS_SU_MODE_CONTROL, A3XX_GRAS_SU_MODE_CONTROL,
	A3XX_GRAS_SC_CONTROL, A3XX_GRAS_SC_CONTROL,
	A3XX_GRAS_SC_SCREEN_SCISSOR_TL, A3XX_GRAS_SC_SCREEN_SCISSOR_BR,
	A3XX_GRAS_SC_WINDOW_SCISSOR_TL, A3XX_GRAS_SC_WINDOW_SCISSOR_BR,
	A3XX_RB_MODE_CONTROL, A3XX_RB_MRT_BLEND_CONTROL3,
	A3XX_RB_BLEND_RED, A3XX_RB_COPY_DEST_INFO,
	A3XX_RB_DEPTH_CONTROL, A3XX_RB_DEPTH_CONTROL,
	A3XX_PC_VSTREAM_CONTROL, A3XX_PC_VSTREAM_CONTROL,
	A3XX_PC_VERTEX_REUSE_BLOCK_CNTL, A3XX_PC_VERTEX_REUSE_BLOCK_CNTL,
	A3XX_PC_PRIM_VTX_CNTL, A3XX_PC_RESTART_INDEX,
	A3XX_HLSQ_CONTROL_0_REG, A3XX_HLSQ_CONST_FSPRESV_RANGE_REG,
	A3XX_HLSQ_CL_NDRANGE_0_REG, A3XX_HLSQ_CL_NDRANGE_0_REG,
	A3XX_HLSQ_CL_NDRANGE_2_REG, A3XX_HLSQ_CL_CONTROL_1_REG,
	A3XX_HLSQ_CL_KERNEL_CONST_REG, A3XX_HLSQ_CL_KERNEL_GROUP_Z_REG,
	A3XX_HLSQ_CL_WG_OFFSET_REG, A3XX_HLSQ_CL_WG_OFFSET_REG,
	A3XX_VFD_CONTROL_0, A3XX_VFD_VS_THREADING_THRESHOLD,
	A3XX_SP_SP_CTRL_REG, A3XX_SP_SP_CTRL_REG,
	A3XX_SP_VS_CTRL_REG0, A3XX_SP_VS_OUT_REG_7,
	A3XX_SP_VS_VPC_DST_REG_0, A3XX_SP_VS_PVT_MEM_SIZE_REG,
	A3XX_SP_VS_LENGTH_REG, A3XX_SP_FS_PVT_MEM_SIZE_REG,
	A3XX_SP_FS_FLAT_SHAD_MODE_REG_0, A3XX_SP_FS_FLAT_SHAD_MODE_REG_1,
	A3XX_SP_FS_OUTPUT_REG, A3XX_SP_FS_OUTPUT_REG,
	A3XX_SP_FS_MRT_REG_0, A3XX_SP_FS_IMAGE_OUTPUT_REG_3,
	A3XX_SP_FS_LENGTH_REG, A3XX_SP_FS_LENGTH_REG,
	A3XX_TPL1_TP_VS_TEX_OFFSET, A3XX_TPL1_TP_FS_BORDER_COLOR_BASE_ADDR,
	A3XX_VPC_ATTR, A3XX_VPC_VARY_CYLWRAP_ENABLE_1,
};

/* Global registers that need to be saved separately */
static const unsigned int global_registers[] = {
	A3XX_GRAS_CL_USER_PLANE_X0, A3XX_GRAS_CL_USER_PLANE_Y0,
	A3XX_GRAS_CL_USER_PLANE_Z0, A3XX_GRAS_CL_USER_PLANE_W0,
	A3XX_GRAS_CL_USER_PLANE_X1, A3XX_GRAS_CL_USER_PLANE_Y1,
	A3XX_GRAS_CL_USER_PLANE_Z1, A3XX_GRAS_CL_USER_PLANE_W1,
	A3XX_GRAS_CL_USER_PLANE_X2, A3XX_GRAS_CL_USER_PLANE_Y2,
	A3XX_GRAS_CL_USER_PLANE_Z2, A3XX_GRAS_CL_USER_PLANE_W2,
	A3XX_GRAS_CL_USER_PLANE_X3, A3XX_GRAS_CL_USER_PLANE_Y3,
	A3XX_GRAS_CL_USER_PLANE_Z3, A3XX_GRAS_CL_USER_PLANE_W3,
	A3XX_GRAS_CL_USER_PLANE_X4, A3XX_GRAS_CL_USER_PLANE_Y4,
	A3XX_GRAS_CL_USER_PLANE_Z4, A3XX_GRAS_CL_USER_PLANE_W4,
	A3XX_GRAS_CL_USER_PLANE_X5, A3XX_GRAS_CL_USER_PLANE_Y5,
	A3XX_GRAS_CL_USER_PLANE_Z5, A3XX_GRAS_CL_USER_PLANE_W5,
	A3XX_VSC_BIN_SIZE,
	A3XX_VSC_PIPE_CONFIG_0, A3XX_VSC_PIPE_CONFIG_1,
	A3XX_VSC_PIPE_CONFIG_2, A3XX_VSC_PIPE_CONFIG_3,
	A3XX_VSC_PIPE_CONFIG_4, A3XX_VSC_PIPE_CONFIG_5,
	A3XX_VSC_PIPE_CONFIG_6, A3XX_VSC_PIPE_CONFIG_7,
	A3XX_VSC_PIPE_DATA_ADDRESS_0, A3XX_VSC_PIPE_DATA_ADDRESS_1,
	A3XX_VSC_PIPE_DATA_ADDRESS_2, A3XX_VSC_PIPE_DATA_ADDRESS_3,
	A3XX_VSC_PIPE_DATA_ADDRESS_4, A3XX_VSC_PIPE_DATA_ADDRESS_5,
	A3XX_VSC_PIPE_DATA_ADDRESS_6, A3XX_VSC_PIPE_DATA_ADDRESS_7,
	A3XX_VSC_PIPE_DATA_LENGTH_0, A3XX_VSC_PIPE_DATA_LENGTH_1,
	A3XX_VSC_PIPE_DATA_LENGTH_2, A3XX_VSC_PIPE_DATA_LENGTH_3,
	A3XX_VSC_PIPE_DATA_LENGTH_4, A3XX_VSC_PIPE_DATA_LENGTH_5,
	A3XX_VSC_PIPE_DATA_LENGTH_6, A3XX_VSC_PIPE_DATA_LENGTH_7,
	A3XX_VSC_SIZE_ADDRESS
};

#define GLOBAL_REGISTER_COUNT ARRAY_SIZE(global_registers)

/* A scratchpad used to build commands during context create */
static struct tmp_ctx {
	unsigned int *cmd; /* Next available dword in C&V buffer */

	/* Addresses in comamnd buffer where registers are saved */
	uint32_t reg_values[GLOBAL_REGISTER_COUNT];
	uint32_t gmem_base; /* Base GPU address of GMEM */
} tmp_ctx;

#ifndef GSL_CONTEXT_SWITCH_CPU_SYNC
/*
 * Function for executing dest = ( (reg & and) ROL rol ) | or
 */
static unsigned int *rmw_regtomem(unsigned int *cmd,
				  unsigned int reg, unsigned int and,
				  unsigned int rol, unsigned int or,
				  unsigned int dest)
{
	/* CP_SCRATCH_REG2 = (CP_SCRATCH_REG2 & 0x00000000) | reg */
	*cmd++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmd++ = (1 << 30) | A3XX_CP_SCRATCH_REG2;
	*cmd++ = 0x00000000;	/* AND value */
	*cmd++ = reg;		/* OR address */

	/* CP_SCRATCH_REG2 = ( (CP_SCRATCH_REG2 & and) ROL rol ) |  or */
	*cmd++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmd++ = (rol << 24) | A3XX_CP_SCRATCH_REG2;
	*cmd++ = and;		/* AND value */
	*cmd++ = or;		/* OR value */

	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_CP_SCRATCH_REG2;
	*cmd++ = dest;

	return cmd;
}
#endif

static void build_regconstantsave_cmds(struct adreno_device *adreno_dev,
				       struct adreno_context *drawctxt)
{
	unsigned int *cmd = tmp_ctx.cmd;
	unsigned int *start = cmd;
	unsigned int i;

	drawctxt->constant_save_commands[0].hostptr = cmd;
	drawctxt->constant_save_commands[0].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	cmd++;

	*cmd++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;

#ifndef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	/*
	 * Context registers are already shadowed; just need to
	 * disable shadowing to prevent corruption.
	 */

	*cmd++ = cp_type3_packet(CP_LOAD_CONSTANT_CONTEXT, 3);
	*cmd++ = (drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000;
	*cmd++ = 4 << 16;	/* regs, start=0 */
	*cmd++ = 0x0;		/* count = 0 */

#else
	/*
	 * Make sure the HW context has the correct register values before
	 * reading them.
	 */

	/* Write context registers into shadow */
	for (i = 0; i < ARRAY_SIZE(context_register_ranges) / 2; i++) {
		unsigned int start = context_register_ranges[i * 2];
		unsigned int end = context_register_ranges[i * 2 + 1];
		*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
		*cmd++ = ((end - start + 1) << REG_TO_MEM_LOOP_COUNT_SHIFT) |
			start;
		*cmd++ = ((drawctxt->gpustate.gpuaddr + REG_OFFSET)
			  & 0xFFFFE000) + (start - 0x2000) * 4;
	}
#endif

	/* Need to handle some of the global registers separately */
	for (i = 0; i < ARRAY_SIZE(global_registers); i++) {
		*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
		*cmd++ = global_registers[i];
		*cmd++ = tmp_ctx.reg_values[i];
	}

	/* Save vertex shader constants */
	*cmd++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmd++ = drawctxt->cond_execs[2].gpuaddr >> 2;
	*cmd++ = drawctxt->cond_execs[2].gpuaddr >> 2;
	*cmd++ = 0x0000FFFF;
	*cmd++ = 3; /* EXEC_COUNT */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	drawctxt->constant_save_commands[1].hostptr = cmd;
	drawctxt->constant_save_commands[1].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	/*
	   From fixup:

	   dwords = SP_VS_CTRL_REG1.VSCONSTLENGTH / 4
	   src = (HLSQ_SHADOW_BASE + 0x2000) / 4

	   From register spec:
	   SP_VS_CTRL_REG1.VSCONSTLENGTH [09:00]: 0-512, unit = 128bits.
	 */
	*cmd++ = 0;	/* (dwords << REG_TO_MEM_LOOP_COUNT_SHIFT) | src  */
	/* ALU constant shadow base */
	*cmd++ = drawctxt->gpustate.gpuaddr & 0xfffffffc;

	/* Save fragment shader constants */
	*cmd++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmd++ = drawctxt->cond_execs[3].gpuaddr >> 2;
	*cmd++ = drawctxt->cond_execs[3].gpuaddr >> 2;
	*cmd++ = 0x0000FFFF;
	*cmd++ = 3; /* EXEC_COUNT */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	drawctxt->constant_save_commands[2].hostptr = cmd;
	drawctxt->constant_save_commands[2].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	/*
	   From fixup:

	   dwords = SP_FS_CTRL_REG1.FSCONSTLENGTH / 4
	   src = (HLSQ_SHADOW_BASE + 0x2000 + SSIZE) / 4

	   From register spec:
	   SP_FS_CTRL_REG1.FSCONSTLENGTH [09:00]: 0-512, unit = 128bits.
	 */
	*cmd++ = 0;	/* (dwords << REG_TO_MEM_LOOP_COUNT_SHIFT) | src  */

	/*
	   From fixup:

	   base = drawctxt->gpustate.gpuaddr (ALU constant shadow base)
	   offset = SP_FS_OBJ_OFFSET_REG.CONSTOBJECTSTARTOFFSET

	   From register spec:
	   SP_FS_OBJ_OFFSET_REG.CONSTOBJECTSTARTOFFSET [16:24]: Constant object
	   start offset in on chip RAM,
	   128bit aligned

	   dst = base + offset
	   Because of the base alignment we can use
	   dst = base | offset
	 */
	*cmd++ = 0;		/* dst */

	/* Save VS texture memory objects */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ =
	    ((TEX_SIZE_MEM_OBJECTS / 4) << REG_TO_MEM_LOOP_COUNT_SHIFT) |
		((HLSQ_SHADOW_BASE + HLSQ_MEMOBJ_OFFSET) / 4);
	*cmd++ =
	    (drawctxt->gpustate.gpuaddr +
	     VS_TEX_OFFSET_MEM_OBJECTS) & 0xfffffffc;

	/* Save VS texture mipmap pointers */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ =
	    ((TEX_SIZE_MIPMAP / 4) << REG_TO_MEM_LOOP_COUNT_SHIFT) |
		((HLSQ_SHADOW_BASE + HLSQ_MIPMAP_OFFSET) / 4);
	*cmd++ =
	    (drawctxt->gpustate.gpuaddr + VS_TEX_OFFSET_MIPMAP) & 0xfffffffc;

	/* Save VS texture sampler objects */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = ((TEX_SIZE_SAMPLER_OBJ / 4) << REG_TO_MEM_LOOP_COUNT_SHIFT) |
		((HLSQ_SHADOW_BASE + HLSQ_SAMPLER_OFFSET) / 4);
	*cmd++ =
	    (drawctxt->gpustate.gpuaddr +
	     VS_TEX_OFFSET_SAMPLER_OBJ) & 0xfffffffc;

	/* Save FS texture memory objects */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ =
	    ((TEX_SIZE_MEM_OBJECTS / 4) << REG_TO_MEM_LOOP_COUNT_SHIFT) |
		((HLSQ_SHADOW_BASE + HLSQ_MEMOBJ_OFFSET + SSIZE) / 4);
	*cmd++ =
	    (drawctxt->gpustate.gpuaddr +
	     FS_TEX_OFFSET_MEM_OBJECTS) & 0xfffffffc;

	/* Save FS texture mipmap pointers */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ =
	    ((TEX_SIZE_MIPMAP / 4) << REG_TO_MEM_LOOP_COUNT_SHIFT) |
		((HLSQ_SHADOW_BASE + HLSQ_MIPMAP_OFFSET + SSIZE) / 4);
	*cmd++ =
	    (drawctxt->gpustate.gpuaddr + FS_TEX_OFFSET_MIPMAP) & 0xfffffffc;

	/* Save FS texture sampler objects */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ =
	    ((TEX_SIZE_SAMPLER_OBJ / 4) << REG_TO_MEM_LOOP_COUNT_SHIFT) |
		((HLSQ_SHADOW_BASE + HLSQ_SAMPLER_OFFSET + SSIZE) / 4);
	*cmd++ =
	    (drawctxt->gpustate.gpuaddr +
	     FS_TEX_OFFSET_SAMPLER_OBJ) & 0xfffffffc;

	/* Create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->regconstant_save, start, cmd);

	tmp_ctx.cmd = cmd;
}

/* Copy GMEM contents to system memory shadow. */
static unsigned int *build_gmem2sys_cmds(struct adreno_device *adreno_dev,
					 struct adreno_context *drawctxt,
					 struct gmem_shadow_t *shadow)
{
	unsigned int *cmds = tmp_ctx.cmd;
	unsigned int *start = cmds;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_RB_MODE_CONTROL);

	/* RB_MODE_CONTROL */
	*cmds++ = _SET(RB_MODECONTROL_RENDER_MODE, RB_RESOLVE_PASS) |
		_SET(RB_MODECONTROL_MARB_CACHE_SPLIT_MODE, 1) |
		_SET(RB_MODECONTROL_PACKER_TIMER_ENABLE, 1);
	/* RB_RENDER_CONTROL */
	*cmds++ = _SET(RB_RENDERCONTROL_BIN_WIDTH, shadow->width >> 5) |
		_SET(RB_RENDERCONTROL_DISABLE_COLOR_PIPE, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_RB_COPY_CONTROL);
	/* RB_COPY_CONTROL */
	*cmds++ = _SET(RB_COPYCONTROL_RESOLVE_CLEAR_MODE,
		RB_CLEAR_MODE_RESOLVE) |
		_SET(RB_COPYCONTROL_COPY_GMEM_BASE,
		tmp_ctx.gmem_base >> 14);
	/* RB_COPY_DEST_BASE */
	*cmds++ = _SET(RB_COPYDESTBASE_COPY_DEST_BASE,
		shadow->gmemshadow.gpuaddr >> 5);
	/* RB_COPY_DEST_PITCH */
	*cmds++ = _SET(RB_COPYDESTPITCH_COPY_DEST_PITCH,
		(shadow->pitch * 4) / 32);
	/* RB_COPY_DEST_INFO */
	*cmds++ = _SET(RB_COPYDESTINFO_COPY_DEST_TILE,
		RB_TILINGMODE_LINEAR) |
		_SET(RB_COPYDESTINFO_COPY_DEST_FORMAT, RB_R8G8B8A8_UNORM) |
		_SET(RB_COPYDESTINFO_COPY_COMPONENT_ENABLE, 0X0F) |
		_SET(RB_COPYDESTINFO_COPY_DEST_ENDIAN, RB_ENDIAN_NONE);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_GRAS_SC_CONTROL);
	/* GRAS_SC_CONTROL */
	*cmds++ = _SET(GRAS_SC_CONTROL_RENDER_MODE, 2);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_VFD_CONTROL_0);
	/* VFD_CONTROL_0 */
	*cmds++ = _SET(VFD_CTRLREG0_TOTALATTRTOVS, 4) |
		_SET(VFD_CTRLREG0_PACKETSIZE, 2) |
		_SET(VFD_CTRLREG0_STRMDECINSTRCNT, 1) |
		_SET(VFD_CTRLREG0_STRMFETCHINSTRCNT, 1);
	/* VFD_CONTROL_1 */
	*cmds++ = _SET(VFD_CTRLREG1_MAXSTORAGE, 1) |
		_SET(VFD_CTRLREG1_REGID4VTX,  252) |
		_SET(VFD_CTRLREG1_REGID4INST,  252);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_VFD_FETCH_INSTR_0_0);
	/* VFD_FETCH_INSTR_0_0 */
	*cmds++ = _SET(VFD_FETCHINSTRUCTIONS_FETCHSIZE, 11) |
		_SET(VFD_FETCHINSTRUCTIONS_BUFSTRIDE, 12) |
		_SET(VFD_FETCHINSTRUCTIONS_STEPRATE, 1);
	/* VFD_FETCH_INSTR_1_0 */
	*cmds++ = _SET(VFD_BASEADDR_BASEADDR,
		shadow->quad_vertices.gpuaddr);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_VFD_DECODE_INSTR_0);
	/* VFD_DECODE_INSTR_0 */
	*cmds++ = _SET(VFD_DECODEINSTRUCTIONS_WRITEMASK, 0x0F) |
		_SET(VFD_DECODEINSTRUCTIONS_CONSTFILL, 1) |
		_SET(VFD_DECODEINSTRUCTIONS_FORMAT, 2) |
		_SET(VFD_DECODEINSTRUCTIONS_REGID, 5) |
		_SET(VFD_DECODEINSTRUCTIONS_SHIFTCNT, 12) |
		_SET(VFD_DECODEINSTRUCTIONS_LASTCOMPVALID, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_HLSQ_CONTROL_0_REG);
	/* HLSQ_CONTROL_0_REG */
	*cmds++ = _SET(HLSQ_CTRL0REG_FSTHREADSIZE, HLSQ_TWO_PIX_QUADS) |
		_SET(HLSQ_CTRL0REG_FSSUPERTHREADENABLE, 1) |
		_SET(HLSQ_CTRL0REG_SPSHADERRESTART, 1) |
		_SET(HLSQ_CTRL0REG_RESERVED2, 1) |
		_SET(HLSQ_CTRL0REG_CHUNKDISABLE, 1) |
		_SET(HLSQ_CTRL0REG_CONSTSWITCHMODE, 1) |
		_SET(HLSQ_CTRL0REG_LAZYUPDATEDISABLE, 1) |
		_SET(HLSQ_CTRL0REG_SPCONSTFULLUPDATE, 1) |
		_SET(HLSQ_CTRL0REG_TPFULLUPDATE, 1);
	/* HLSQ_CONTROL_1_REG */
	*cmds++ = _SET(HLSQ_CTRL1REG_VSTHREADSIZE, HLSQ_TWO_VTX_QUADS) |
		_SET(HLSQ_CTRL1REG_VSSUPERTHREADENABLE, 1) |
		_SET(HLSQ_CTRL1REG_RESERVED1, 4);
	/* HLSQ_CONTROL_2_REG */
	*cmds++ = _SET(HLSQ_CTRL2REG_PRIMALLOCTHRESHOLD, 31);
	/* HLSQ_CONTROL_3_REG */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_HLSQ_VS_CONTROL_REG);
	/* HLSQ_VS_CONTROL_REG */
	*cmds++ = _SET(HLSQ_VSCTRLREG_VSINSTRLENGTH, 1);
	/* HLSQ_FS_CONTROL_REG */
	*cmds++ = _SET(HLSQ_FSCTRLREG_FSCONSTLENGTH, 1) |
		_SET(HLSQ_FSCTRLREG_FSCONSTSTARTOFFSET, 272) |
		_SET(HLSQ_FSCTRLREG_FSINSTRLENGTH, 1);
	/* HLSQ_CONST_VSPRESV_RANGE_REG */
	*cmds++ = 0x00000000;
	/* HLSQ_CONST_FSPRESV_RANGE_REQ */
	*cmds++ = _SET(HLSQ_CONSTFSPRESERVEDRANGEREG_STARTENTRY, 32) |
		_SET(HLSQ_CONSTFSPRESERVEDRANGEREG_ENDENTRY, 32);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_SP_FS_LENGTH_REG);
	/* SP_FS_LENGTH_REG */
	*cmds++ = _SET(SP_SHADERLENGTH_LEN, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_SP_SP_CTRL_REG);
	/* SP_SP_CTRL_REG */
	*cmds++ = _SET(SP_SPCTRLREG_CONSTMODE, 1) |
		_SET(SP_SPCTRLREG_SLEEPMODE, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 12);
	*cmds++ = CP_REG(A3XX_SP_VS_CTRL_REG0);
	/* SP_VS_CTRL_REG0 */
	*cmds++ = _SET(SP_VSCTRLREG0_VSTHREADMODE, SP_MULTI) |
		_SET(SP_VSCTRLREG0_VSINSTRBUFFERMODE, SP_BUFFER_MODE) |
		_SET(SP_VSCTRLREG0_VSICACHEINVALID, 1) |
		_SET(SP_VSCTRLREG0_VSFULLREGFOOTPRINT, 3) |
		_SET(SP_VSCTRLREG0_VSTHREADSIZE, SP_TWO_VTX_QUADS) |
		_SET(SP_VSCTRLREG0_VSSUPERTHREADMODE, 1) |
		_SET(SP_VSCTRLREG0_VSLENGTH, 1);
	/* SP_VS_CTRL_REG1 */
	*cmds++ = _SET(SP_VSCTRLREG1_VSINITIALOUTSTANDING, 4);
	/* SP_VS_PARAM_REG */
	*cmds++ = _SET(SP_VSPARAMREG_POSREGID, 1) |
		_SET(SP_VSPARAMREG_PSIZEREGID, 252);
	/* SP_VS_OUT_REG_0 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG_1 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG_2 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG_3 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG_4 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG_5 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG_6 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG_7 */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 7);
	*cmds++ = CP_REG(A3XX_SP_VS_VPC_DST_REG_0);
	/* SP_VS_VPC_DST_REG_0 */
	*cmds++ = 0x00000000;
	/* SP_VS_VPC_DST_REG_1 */
	*cmds++ = 0x00000000;
	/* SP_VS_VPC_DST_REG_2 */
	*cmds++ = 0x00000000;
	/* SP_VS_VPC_DST_REG_3 */
	*cmds++ = 0x00000000;
	/* SP_VS_OBJ_OFFSET_REG */
	*cmds++ = 0x00000000;
	/* SP_VS_OBJ_START_REG */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 6);
	*cmds++ = CP_REG(A3XX_SP_VS_LENGTH_REG);
	/* SP_VS_LENGTH_REG */
	*cmds++ = _SET(SP_SHADERLENGTH_LEN, 1);
	/* SP_FS_CTRL_REG0 */
	*cmds++ = _SET(SP_FSCTRLREG0_FSTHREADMODE, SP_MULTI) |
		_SET(SP_FSCTRLREG0_FSINSTRBUFFERMODE, SP_BUFFER_MODE) |
		_SET(SP_FSCTRLREG0_FSICACHEINVALID, 1) |
		_SET(SP_FSCTRLREG0_FSFULLREGFOOTPRINT, 2) |
		_SET(SP_FSCTRLREG0_FSINOUTREGOVERLAP, 1) |
		_SET(SP_FSCTRLREG0_FSTHREADSIZE, SP_TWO_VTX_QUADS) |
		_SET(SP_FSCTRLREG0_FSSUPERTHREADMODE, 1) |
		_SET(SP_FSCTRLREG0_FSLENGTH, 1);
	/* SP_FS_CTRL_REG1 */
	*cmds++ = _SET(SP_FSCTRLREG1_FSCONSTLENGTH, 1) |
		_SET(SP_FSCTRLREG1_FSINITIALOUTSTANDING, 2) |
		_SET(SP_FSCTRLREG1_HALFPRECVAROFFSET, 63);
	/* SP_FS_OBJ_OFFSET_REG */
	*cmds++ = _SET(SP_OBJOFFSETREG_CONSTOBJECTSTARTOFFSET, 272) |
		_SET(SP_OBJOFFSETREG_SHADEROBJOFFSETINIC, 1);
	/* SP_FS_OBJ_START_REG */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_SP_FS_FLAT_SHAD_MODE_REG_0);
	/* SP_FS_FLAT_SHAD_MODE_REG_0 */
	*cmds++ = 0x00000000;
	/* SP_FS_FLAT_SHAD_MODE_REG_1 */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_SP_FS_OUTPUT_REG);
	/* SP_FS_OUTPUT_REG */
	*cmds++ = _SET(SP_IMAGEOUTPUTREG_PAD0, SP_PIXEL_BASED);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_SP_FS_MRT_REG_0);
	/* SP_FS_MRT_REG_0 */
	*cmds++ = _SET(SP_FSMRTREG_REGID, 1);
	/* SP_FS_MRT_REG_1 */
	*cmds++ = 0x00000000;
	/* SP_FS_MRT_REG_2 */
	*cmds++ = 0x00000000;
	/* SP_FS_MRT_REG_3 */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 11);
	*cmds++ = CP_REG(A3XX_VPC_ATTR);
	/* VPC_ATTR */
	*cmds++ = _SET(VPC_VPCATTR_THRHDASSIGN, 1) |
		_SET(VPC_VPCATTR_LMSIZE, 1);
	/* VPC_PACK */
	*cmds++ = 0x00000000;
	/* VPC_VARRYING_INTERUPT_MODE_0 */
	*cmds++ = 0x00000000;
	/* VPC_VARRYING_INTERUPT_MODE_1 */
	*cmds++ = 0x00000000;
	/* VPC_VARRYING_INTERUPT_MODE_2 */
	*cmds++ = 0x00000000;
	/* VPC_VARRYING_INTERUPT_MODE_3 */
	*cmds++ = 0x00000000;
	/* VPC_VARYING_PS_REPL_MODE_0 */
	*cmds++ = 0x00000000;
	/* VPC_VARYING_PS_REPL_MODE_1 */
	*cmds++ = 0x00000000;
	/* VPC_VARYING_PS_REPL_MODE_2 */
	*cmds++ = 0x00000000;
	/* VPC_VARYING_PS_REPL_MODE_3 */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 10);
	*cmds++ = (0 << CP_LOADSTATE_DSTOFFSET_SHIFT)
		| (HLSQ_DIRECT << CP_LOADSTATE_STATESRC_SHIFT)
		| (HLSQ_BLOCK_ID_SP_VS << CP_LOADSTATE_STATEBLOCKID_SHIFT)
		| (1 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (HLSQ_SP_VS_INSTR << CP_LOADSTATE_STATETYPE_SHIFT)
		| (0 << CP_LOADSTATE_EXTSRCADDR_SHIFT);

	/* (sy)(rpt3)mov.f32f32 r0.y, (r)r1.y; */
	*cmds++ = 0x00000005; *cmds++ = 0x30044b01;
	/* end; */
	*cmds++ = 0x00000000; *cmds++ = 0x03000000;
	/* nop; */
	*cmds++ = 0x00000000; *cmds++ = 0x00000000;
	/* nop; */
	*cmds++ = 0x00000000; *cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 10);
	*cmds++ = (0 << CP_LOADSTATE_DSTOFFSET_SHIFT)
		| (HLSQ_DIRECT << CP_LOADSTATE_STATESRC_SHIFT)
		| (HLSQ_BLOCK_ID_SP_FS << CP_LOADSTATE_STATEBLOCKID_SHIFT)
		| (1 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (HLSQ_SP_FS_INSTR << CP_LOADSTATE_STATETYPE_SHIFT)
		| (0 << CP_LOADSTATE_EXTSRCADDR_SHIFT);

	/* (sy)(rpt3)mov.f32f32 r0.y, (r)c0.x; */
	*cmds++ = 0x00000000; *cmds++ = 0x30244b01;
	/* end; */
	*cmds++ = 0x00000000; *cmds++ = 0x03000000;
	/* nop; */
	*cmds++ = 0x00000000; *cmds++ = 0x00000000;
	/* nop; */
	*cmds++ = 0x00000000; *cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_MSAA_CONTROL);
	/* RB_MSAA_CONTROL */
	*cmds++ = _SET(RB_MSAACONTROL_MSAA_DISABLE, 1) |
		_SET(RB_MSAACONTROL_SAMPLE_MASK, 0xFFFF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_DEPTH_CONTROL);
	/* RB_DEPTH_CONTROL */
	*cmds++ = _SET(RB_DEPTHCONTROL_Z_TEST_FUNC, RB_FRAG_NEVER);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_MRT_CONTROL0);
	/* RB_MRT_CONTROL0 */
	*cmds++ = _SET(RB_MRTCONTROL_READ_DEST_ENABLE, 1) |
		_SET(RB_MRTCONTROL_ROP_CODE, 12) |
		_SET(RB_MRTCONTROL_DITHER_MODE, RB_DITHER_ALWAYS) |
		_SET(RB_MRTCONTROL_COMPONENT_ENABLE, 0xF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_RB_MRT_BLEND_CONTROL0);
	/* RB_MRT_BLEND_CONTROL0 */
	*cmds++ = _SET(RB_MRTBLENDCONTROL_RGB_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_RGB_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_RGB_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_CLAMP_ENABLE, 1);
	/* RB_MRT_CONTROL1 */
	*cmds++ = _SET(RB_MRTCONTROL_READ_DEST_ENABLE, 1) |
		_SET(RB_MRTCONTROL_DITHER_MODE, RB_DITHER_DISABLE) |
		_SET(RB_MRTCONTROL_COMPONENT_ENABLE, 0xF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_RB_MRT_BLEND_CONTROL1);
	/* RB_MRT_BLEND_CONTROL1 */
	*cmds++ = _SET(RB_MRTBLENDCONTROL_RGB_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_RGB_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_RGB_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_CLAMP_ENABLE, 1);
	/* RB_MRT_CONTROL2 */
	*cmds++ = _SET(RB_MRTCONTROL_READ_DEST_ENABLE, 1) |
		_SET(RB_MRTCONTROL_DITHER_MODE, RB_DITHER_DISABLE) |
		_SET(RB_MRTCONTROL_COMPONENT_ENABLE, 0xF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_RB_MRT_BLEND_CONTROL2);
	/* RB_MRT_BLEND_CONTROL2 */
	*cmds++ = _SET(RB_MRTBLENDCONTROL_RGB_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_RGB_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_RGB_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_CLAMP_ENABLE, 1);
	/* RB_MRT_CONTROL3 */
	*cmds++ = _SET(RB_MRTCONTROL_READ_DEST_ENABLE, 1) |
		_SET(RB_MRTCONTROL_DITHER_MODE, RB_DITHER_DISABLE) |
		_SET(RB_MRTCONTROL_COMPONENT_ENABLE, 0xF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_MRT_BLEND_CONTROL3);
	/* RB_MRT_BLEND_CONTROL3 */
	*cmds++ = _SET(RB_MRTBLENDCONTROL_RGB_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_RGB_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_RGB_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_CLAMP_ENABLE, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_VFD_INDEX_MIN);
	/* VFD_INDEX_MIN */
	*cmds++ = 0x00000000;
	/* VFD_INDEX_MAX */
	*cmds++ = 0xFFFFFFFF;
	/* VFD_INSTANCEID_OFFSET */
	*cmds++ = 0x00000000;
	/* VFD_INDEX_OFFSET */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_VFD_VS_THREADING_THRESHOLD);
	/* VFD_VS_THREADING_THRESHOLD */
	*cmds++ = _SET(VFD_THREADINGTHRESHOLD_RESERVED6, 12) |
		_SET(VFD_THREADINGTHRESHOLD_REGID_VTXCNT, 252);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_TPL1_TP_VS_TEX_OFFSET);
	/* TPL1_TP_VS_TEX_OFFSET */
	*cmds++ = 0;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_TPL1_TP_FS_TEX_OFFSET);
	/* TPL1_TP_FS_TEX_OFFSET */
	*cmds++ = _SET(TPL1_TPTEXOFFSETREG_SAMPLEROFFSET, 16) |
		_SET(TPL1_TPTEXOFFSETREG_MEMOBJOFFSET, 16) |
		_SET(TPL1_TPTEXOFFSETREG_BASETABLEPTR, 224);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_PC_PRIM_VTX_CNTL);
	/* PC_PRIM_VTX_CNTL */
	*cmds++ = _SET(PC_PRIM_VTX_CONTROL_POLYMODE_FRONT_PTYPE,
		PC_DRAW_TRIANGLES) |
		_SET(PC_PRIM_VTX_CONTROL_POLYMODE_BACK_PTYPE,
		PC_DRAW_TRIANGLES) |
		_SET(PC_PRIM_VTX_CONTROL_PROVOKING_VTX_LAST, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_GRAS_SC_WINDOW_SCISSOR_TL);
	/* GRAS_SC_WINDOW_SCISSOR_TL */
	*cmds++ = 0x00000000;
	/* GRAS_SC_WINDOW_SCISSOR_BR */
	*cmds++ = _SET(GRAS_SC_WINDOW_SCISSOR_BR_BR_X, shadow->width - 1) |
		_SET(GRAS_SC_WINDOW_SCISSOR_BR_BR_Y, shadow->height - 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_GRAS_SC_SCREEN_SCISSOR_TL);
	/* GRAS_SC_SCREEN_SCISSOR_TL */
	*cmds++ = 0x00000000;
	/* GRAS_SC_SCREEN_SCISSOR_BR */
	*cmds++ = _SET(GRAS_SC_SCREEN_SCISSOR_BR_BR_X, shadow->width - 1) |
		_SET(GRAS_SC_SCREEN_SCISSOR_BR_BR_Y, shadow->height - 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_GRAS_CL_VPORT_XOFFSET);
	/* GRAS_CL_VPORT_XOFFSET */
	*cmds++ = 0x00000000;
	/* GRAS_CL_VPORT_XSCALE */
	*cmds++ = _SET(GRAS_CL_VPORT_XSCALE_VPORT_XSCALE, 0x3f800000);
	/* GRAS_CL_VPORT_YOFFSET */
	*cmds++ = 0x00000000;
	/* GRAS_CL_VPORT_YSCALE */
	*cmds++ = _SET(GRAS_CL_VPORT_YSCALE_VPORT_YSCALE, 0x3f800000);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_GRAS_CL_VPORT_ZOFFSET);
	/* GRAS_CL_VPORT_ZOFFSET */
	*cmds++ = 0x00000000;
	/* GRAS_CL_VPORT_ZSCALE */
	*cmds++ = _SET(GRAS_CL_VPORT_ZSCALE_VPORT_ZSCALE, 0x3f800000);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_GRAS_CL_CLIP_CNTL);
	/* GRAS_CL_CLIP_CNTL */
	*cmds++ = _SET(GRAS_CL_CLIP_CNTL_CLIP_DISABLE, 1) |
		_SET(GRAS_CL_CLIP_CNTL_ZFAR_CLIP_DISABLE, 1) |
		_SET(GRAS_CL_CLIP_CNTL_VP_CLIP_CODE_IGNORE, 1) |
		_SET(GRAS_CL_CLIP_CNTL_VP_XFORM_DISABLE, 1) |
		_SET(GRAS_CL_CLIP_CNTL_PERSP_DIVISION_DISABLE, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_GRAS_CL_GB_CLIP_ADJ);
	/* GRAS_CL_GB_CLIP_ADJ */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;

	/*
	 * Resolve using two draw calls with a dummy register
	 * write in between. This is a HLM workaround
	 * that should be removed later.
	 */
	*cmds++ = cp_type3_packet(CP_DRAW_INDX_2, 6);
	*cmds++ = 0x00000000; /* Viz query info */
	*cmds++ = BUILD_PC_DRAW_INITIATOR(PC_DI_PT_TRILIST,
					  PC_DI_SRC_SEL_IMMEDIATE,
					  PC_DI_INDEX_SIZE_32_BIT,
					  PC_DI_IGNORE_VISIBILITY);
	*cmds++ = 0x00000003; /* Num indices */
	*cmds++ = 0x00000000; /* Index 0 */
	*cmds++ = 0x00000001; /* Index 1 */
	*cmds++ = 0x00000002; /* Index 2 */

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_HLSQ_CL_CONTROL_0_REG);
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_DRAW_INDX_2, 6);
	*cmds++ = 0x00000000; /* Viz query info */
	*cmds++ = BUILD_PC_DRAW_INITIATOR(PC_DI_PT_TRILIST,
					  PC_DI_SRC_SEL_IMMEDIATE,
					  PC_DI_INDEX_SIZE_32_BIT,
					  PC_DI_IGNORE_VISIBILITY);
	*cmds++ = 0x00000003; /* Num indices */
	*cmds++ = 0x00000002; /* Index 0 */
	*cmds++ = 0x00000001; /* Index 1 */
	*cmds++ = 0x00000003; /* Index 2 */

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_HLSQ_CL_CONTROL_0_REG);
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;

	/* Create indirect buffer command for above command sequence */
	create_ib1(drawctxt, shadow->gmem_save, start, cmds);

	return cmds;
}

static void build_shader_save_cmds(struct adreno_device *adreno_dev,
				   struct adreno_context *drawctxt)
{
	unsigned int *cmd = tmp_ctx.cmd;
	unsigned int *start;

	/* Reserve space for boolean values used for COND_EXEC packet */
	drawctxt->cond_execs[0].hostptr = cmd;
	drawctxt->cond_execs[0].gpuaddr = virt2gpu(cmd, &drawctxt->gpustate);
	*cmd++ = 0;
	drawctxt->cond_execs[1].hostptr = cmd;
	drawctxt->cond_execs[1].gpuaddr = virt2gpu(cmd, &drawctxt->gpustate);
	*cmd++ = 0;

	drawctxt->shader_save_commands[0].hostptr = cmd;
	drawctxt->shader_save_commands[0].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	*cmd++ = 0;
	drawctxt->shader_save_commands[1].hostptr = cmd;
	drawctxt->shader_save_commands[1].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	*cmd++ = 0;

	start = cmd;

	/* Save vertex shader */

	*cmd++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmd++ = drawctxt->cond_execs[0].gpuaddr >> 2;
	*cmd++ = drawctxt->cond_execs[0].gpuaddr >> 2;
	*cmd++ = 0x0000FFFF;
	*cmd++ = 3;		/* EXEC_COUNT */

	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	drawctxt->shader_save_commands[2].hostptr = cmd;
	drawctxt->shader_save_commands[2].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	/*
	   From fixup:

	   dwords = SP_VS_CTRL_REG0.VS_LENGTH * 8

	   From regspec:
	   SP_VS_CTRL_REG0.VS_LENGTH [31:24]: VS length, unit = 256bits.
	   If bit31 is 1, it means overflow
	   or any long shader.

	   src = (HLSQ_SHADOW_BASE + 0x1000)/4
	 */
	*cmd++ = 0;	/*(dwords << REG_TO_MEM_LOOP_COUNT_SHIFT) | src */
	*cmd++ = (drawctxt->gpustate.gpuaddr + SHADER_OFFSET) & 0xfffffffc;

	/* Save fragment shader */
	*cmd++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmd++ = drawctxt->cond_execs[1].gpuaddr >> 2;
	*cmd++ = drawctxt->cond_execs[1].gpuaddr >> 2;
	*cmd++ = 0x0000FFFF;
	*cmd++ = 3;		/* EXEC_COUNT */

	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	drawctxt->shader_save_commands[3].hostptr = cmd;
	drawctxt->shader_save_commands[3].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	/*
	   From fixup:

	   dwords = SP_FS_CTRL_REG0.FS_LENGTH * 8

	   From regspec:
	   SP_FS_CTRL_REG0.FS_LENGTH [31:24]: FS length, unit = 256bits.
	   If bit31 is 1, it means overflow
	   or any long shader.

	   fs_offset = SP_FS_OBJ_OFFSET_REG.SHADEROBJOFFSETINIC * 32
	   From regspec:

	   SP_FS_OBJ_OFFSET_REG.SHADEROBJOFFSETINIC [31:25]:
	   First instruction of the whole shader will be stored from
	   the offset in instruction cache, unit = 256bits, a cache line.
	   It can start from 0 if no VS available.

	   src = (HLSQ_SHADOW_BASE + 0x1000 + SSIZE + fs_offset)/4
	 */
	*cmd++ = 0;	/*(dwords << REG_TO_MEM_LOOP_COUNT_SHIFT) | src */
	*cmd++ = (drawctxt->gpustate.gpuaddr + SHADER_OFFSET
		  + (SHADER_SHADOW_SIZE / 2)) & 0xfffffffc;

	/* Create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->shader_save, start, cmd);

	tmp_ctx.cmd = cmd;
}

/*
 * Make an IB to modify context save IBs with the correct shader instruction
 * and constant sizes and offsets.
 */

static void build_save_fixup_cmds(struct adreno_device *adreno_dev,
				  struct adreno_context *drawctxt)
{
	unsigned int *cmd = tmp_ctx.cmd;
	unsigned int *start = cmd;

	/* Flush HLSQ lazy updates */
	*cmd++ = cp_type3_packet(CP_EVENT_WRITE, 1);
	*cmd++ = 0x7;		/* HLSQ_FLUSH */
	*cmd++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;

	*cmd++ = cp_type0_packet(A3XX_UCHE_CACHE_INVALIDATE0_REG, 2);
	*cmd++ = 0x00000000; /* No start addr for full invalidate */
	*cmd++ = (unsigned int)
		UCHE_ENTIRE_CACHE << UCHE_INVALIDATE1REG_ALLORPORTION |
		UCHE_OP_INVALIDATE << UCHE_INVALIDATE1REG_OPCODE |
		0; /* No end addr for full invalidate */

	/* Make sure registers are flushed */
	*cmd++ = cp_type3_packet(CP_CONTEXT_UPDATE, 1);
	*cmd++ = 0;

#ifdef GSL_CONTEXT_SWITCH_CPU_SYNC

	/* Save shader sizes */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_VS_CTRL_REG0;
	*cmd++ = drawctxt->shader_save_commands[2].gpuaddr;

	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_FS_CTRL_REG0;
	*cmd++ = drawctxt->shader_save_commands[3].gpuaddr;

	/* Save shader offsets */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_FS_OBJ_OFFSET_REG;
	*cmd++ = drawctxt->shader_save_commands[1].gpuaddr;

	/* Save constant sizes */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_VS_CTRL_REG1;
	*cmd++ = drawctxt->constant_save_commands[1].gpuaddr;
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_FS_CTRL_REG1;
	*cmd++ = drawctxt->constant_save_commands[2].gpuaddr;

	/* Save FS constant offset */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_FS_OBJ_OFFSET_REG;
	*cmd++ = drawctxt->constant_save_commands[0].gpuaddr;


	/* Save VS instruction store mode */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_VS_CTRL_REG0;
	*cmd++ = drawctxt->cond_execs[0].gpuaddr;

	/* Save FS instruction store mode */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_FS_CTRL_REG0;
	*cmd++ = drawctxt->cond_execs[1].gpuaddr;
#else

	/* Shader save */
	cmd = rmw_regtomem(cmd, A3XX_SP_VS_CTRL_REG0, 0x7f000000,
			11+REG_TO_MEM_LOOP_COUNT_SHIFT,
			(HLSQ_SHADOW_BASE + 0x1000) / 4,
			drawctxt->shader_save_commands[2].gpuaddr);

	/* CP_SCRATCH_REG2 = (CP_SCRATCH_REG2 & 0x00000000) | SP_FS_CTRL_REG0 */
	*cmd++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmd++ = (1 << 30) | A3XX_CP_SCRATCH_REG2;
	*cmd++ = 0x00000000;	/* AND value */
	*cmd++ = A3XX_SP_FS_CTRL_REG0;	/* OR address */
	/* CP_SCRATCH_REG2 = ( (CP_SCRATCH_REG2 & 0x7f000000) >> 21 )
	   |  ((HLSQ_SHADOW_BASE+0x1000+SSIZE)/4) */
	*cmd++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmd++ = ((11 + REG_TO_MEM_LOOP_COUNT_SHIFT) << 24) |
		A3XX_CP_SCRATCH_REG2;
	*cmd++ = 0x7f000000;	/* AND value */
	*cmd++ = (HLSQ_SHADOW_BASE + 0x1000 + SSIZE) / 4;	/* OR value */

	/*
	 * CP_SCRATCH_REG3 = (CP_SCRATCH_REG3 & 0x00000000) |
	 * SP_FS_OBJ_OFFSET_REG
	 */

	*cmd++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmd++ = (1 << 30) | A3XX_CP_SCRATCH_REG3;
	*cmd++ = 0x00000000;	/* AND value */
	*cmd++ = A3XX_SP_FS_OBJ_OFFSET_REG;	/* OR address */
	/*
	 * CP_SCRATCH_REG3 = ( (CP_SCRATCH_REG3 & 0xfe000000) >> 25 ) |
	 * 0x00000000
	 */
	*cmd++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmd++ = A3XX_CP_SCRATCH_REG3;
	*cmd++ = 0xfe000000;	/* AND value */
	*cmd++ = 0x00000000;	/* OR value */
	/*
	 * CP_SCRATCH_REG2 =  (CP_SCRATCH_REG2 & 0xffffffff) | CP_SCRATCH_REG3
	 */
	*cmd++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmd++ = (1 << 30) | A3XX_CP_SCRATCH_REG2;
	*cmd++ = 0xffffffff;	/* AND value */
	*cmd++ = A3XX_CP_SCRATCH_REG3;	/* OR address */

	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_CP_SCRATCH_REG2;
	*cmd++ = drawctxt->shader_save_commands[3].gpuaddr;

	/* Constant save */
	cmd = rmw_regtomem(cmd, A3XX_SP_VS_CTRL_REG1, 0x000003ff,
			   17, (HLSQ_SHADOW_BASE + 0x2000) / 4,
			   drawctxt->constant_save_commands[1].gpuaddr);

	cmd = rmw_regtomem(cmd, A3XX_SP_FS_CTRL_REG1, 0x000003ff,
			   17, (HLSQ_SHADOW_BASE + 0x2000 + SSIZE) / 4,
			   drawctxt->constant_save_commands[2].gpuaddr);

	cmd = rmw_regtomem(cmd, A3XX_SP_FS_OBJ_OFFSET_REG, 0x00ff0000,
			   18, drawctxt->gpustate.gpuaddr & 0xfffffe00,
			   drawctxt->constant_save_commands[2].gpuaddr
			   + sizeof(unsigned int));

	/* Modify constant save conditionals */
	cmd = rmw_regtomem(cmd, A3XX_SP_VS_CTRL_REG1, 0x000003ff,
		0, 0, drawctxt->cond_execs[2].gpuaddr);

	cmd = rmw_regtomem(cmd, A3XX_SP_FS_CTRL_REG1, 0x000003ff,
		0, 0, drawctxt->cond_execs[3].gpuaddr);

	/* Save VS instruction store mode */

	cmd = rmw_regtomem(cmd, A3XX_SP_VS_CTRL_REG0, 0x00000002,
			   31, 0, drawctxt->cond_execs[0].gpuaddr);

	/* Save FS instruction store mode */
	cmd = rmw_regtomem(cmd, A3XX_SP_FS_CTRL_REG0, 0x00000002,
			   31, 0, drawctxt->cond_execs[1].gpuaddr);

#endif

	create_ib1(drawctxt, drawctxt->save_fixup, start, cmd);

	tmp_ctx.cmd = cmd;
}

/****************************************************************************/
/* Functions to build context restore IBs                                   */
/****************************************************************************/

static unsigned int *build_sys2gmem_cmds(struct adreno_device *adreno_dev,
					 struct adreno_context *drawctxt,
					 struct gmem_shadow_t *shadow)
{
	unsigned int *cmds = tmp_ctx.cmd;
	unsigned int *start = cmds;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_HLSQ_CONTROL_0_REG);
	/* HLSQ_CONTROL_0_REG */
	*cmds++ = _SET(HLSQ_CTRL0REG_FSTHREADSIZE, HLSQ_FOUR_PIX_QUADS) |
		_SET(HLSQ_CTRL0REG_SPSHADERRESTART, 1) |
		_SET(HLSQ_CTRL0REG_CHUNKDISABLE, 1) |
		_SET(HLSQ_CTRL0REG_SPCONSTFULLUPDATE, 1) |
		_SET(HLSQ_CTRL0REG_TPFULLUPDATE, 1);
	/* HLSQ_CONTROL_1_REG */
	*cmds++ = _SET(HLSQ_CTRL1REG_VSTHREADSIZE, HLSQ_TWO_VTX_QUADS);
	/* HLSQ_CONTROL_2_REG */
	*cmds++ = _SET(HLSQ_CTRL2REG_PRIMALLOCTHRESHOLD, 31);
	/* HLSQ_CONTROL3_REG */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_RB_MRT_BUF_INFO0);
	/* RB_MRT_BUF_INFO0 */
	*cmds++ = _SET(RB_MRTBUFINFO_COLOR_FORMAT, RB_R8G8B8A8_UNORM) |
		_SET(RB_MRTBUFINFO_COLOR_TILE_MODE, RB_TILINGMODE_32X32) |
		_SET(RB_MRTBUFINFO_COLOR_BUF_PITCH,
		(shadow->gmem_pitch * 4 * 8) / 256);
	/* RB_MRT_BUF_BASE0 */
	*cmds++ = _SET(RB_MRTBUFBASE_COLOR_BUF_BASE, tmp_ctx.gmem_base >> 5);

	/* Texture samplers */
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 4);
	*cmds++ = (16 << CP_LOADSTATE_DSTOFFSET_SHIFT)
		| (HLSQ_DIRECT << CP_LOADSTATE_STATESRC_SHIFT)
		| (HLSQ_BLOCK_ID_TP_TEX << CP_LOADSTATE_STATEBLOCKID_SHIFT)
		| (1 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (HLSQ_TP_TEX_SAMPLERS << CP_LOADSTATE_STATETYPE_SHIFT)
		| (0 << CP_LOADSTATE_EXTSRCADDR_SHIFT);
	*cmds++ = 0x00000240;
	*cmds++ = 0x00000000;

	/* Texture memobjs */
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 6);
	*cmds++ = (16 << CP_LOADSTATE_DSTOFFSET_SHIFT)
		| (HLSQ_DIRECT << CP_LOADSTATE_STATESRC_SHIFT)
		| (HLSQ_BLOCK_ID_TP_TEX << CP_LOADSTATE_STATEBLOCKID_SHIFT)
		| (1 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (HLSQ_TP_TEX_MEMOBJ << CP_LOADSTATE_STATETYPE_SHIFT)
		| (0 << CP_LOADSTATE_EXTSRCADDR_SHIFT);
	*cmds++ = 0x4cc06880;
	*cmds++ = shadow->height | (shadow->width << 14);
	*cmds++ = (shadow->pitch*4*8) << 9;
	*cmds++ = 0x00000000;

	/* Mipmap bases */
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 16);
	*cmds++ = (224 << CP_LOADSTATE_DSTOFFSET_SHIFT)
		| (HLSQ_DIRECT << CP_LOADSTATE_STATESRC_SHIFT)
		| (HLSQ_BLOCK_ID_TP_MIPMAP << CP_LOADSTATE_STATEBLOCKID_SHIFT)
		| (14 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (HLSQ_TP_MIPMAP_BASE << CP_LOADSTATE_STATETYPE_SHIFT)
		| (0 << CP_LOADSTATE_EXTSRCADDR_SHIFT);
	*cmds++ = shadow->gmemshadow.gpuaddr;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_HLSQ_VS_CONTROL_REG);
	/* HLSQ_VS_CONTROL_REG */
	*cmds++ = _SET(HLSQ_VSCTRLREG_VSINSTRLENGTH, 1);
	/* HLSQ_FS_CONTROL_REG */
	*cmds++ = _SET(HLSQ_FSCTRLREG_FSCONSTLENGTH, 1) |
		_SET(HLSQ_FSCTRLREG_FSCONSTSTARTOFFSET, 128) |
		_SET(HLSQ_FSCTRLREG_FSINSTRLENGTH, 2);
	/* HLSQ_CONST_VSPRESV_RANGE_REG */
	*cmds++ = 0x00000000;
	/* HLSQ_CONST_FSPRESV_RANGE_REG */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_SP_FS_LENGTH_REG);
	/* SP_FS_LENGTH_REG */
	*cmds++ = _SET(SP_SHADERLENGTH_LEN, 2);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 12);
	*cmds++ = CP_REG(A3XX_SP_VS_CTRL_REG0);
	/* SP_VS_CTRL_REG0 */
	*cmds++ = _SET(SP_VSCTRLREG0_VSTHREADMODE, SP_MULTI) |
		_SET(SP_VSCTRLREG0_VSINSTRBUFFERMODE, SP_BUFFER_MODE) |
		_SET(SP_VSCTRLREG0_VSICACHEINVALID, 1) |
		_SET(SP_VSCTRLREG0_VSFULLREGFOOTPRINT, 2) |
		_SET(SP_VSCTRLREG0_VSTHREADSIZE, SP_TWO_VTX_QUADS) |
		_SET(SP_VSCTRLREG0_VSLENGTH, 1);
	/* SP_VS_CTRL_REG1 */
	*cmds++ = _SET(SP_VSCTRLREG1_VSINITIALOUTSTANDING, 8);
	/* SP_VS_PARAM_REG */
	*cmds++ = _SET(SP_VSPARAMREG_POSREGID, 4) |
		_SET(SP_VSPARAMREG_PSIZEREGID, 252) |
		_SET(SP_VSPARAMREG_TOTALVSOUTVAR, 1);
	/* SP_VS_OUT_REG0 */
	*cmds++ = _SET(SP_VSOUTREG_COMPMASK0, 3);
	/* SP_VS_OUT_REG1 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG2 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG3 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG4 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG5 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG6 */
	*cmds++ = 0x00000000;
	/* SP_VS_OUT_REG7 */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 7);
	*cmds++ = CP_REG(A3XX_SP_VS_VPC_DST_REG_0);
	/* SP_VS_VPC_DST_REG0 */
	*cmds++ = _SET(SP_VSVPCDSTREG_OUTLOC0, 8);
	/* SP_VS_VPC_DST_REG1 */
	*cmds++ = 0x00000000;
	/* SP_VS_VPC_DST_REG2 */
	*cmds++ = 0x00000000;
	/* SP_VS_VPC_DST_REG3 */
	*cmds++ = 0x00000000;
	/* SP_VS_OBJ_OFFSET_REG */
	*cmds++ = 0x00000000;
	/* SP_VS_OBJ_START_REG */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 6);
	*cmds++ = CP_REG(A3XX_SP_VS_LENGTH_REG);
	/* SP_VS_LENGTH_REG */
	*cmds++ = _SET(SP_SHADERLENGTH_LEN, 1);
	/* SP_FS_CTRL_REG0 */
	*cmds++ = _SET(SP_FSCTRLREG0_FSTHREADMODE, SP_MULTI) |
		_SET(SP_FSCTRLREG0_FSINSTRBUFFERMODE, SP_BUFFER_MODE) |
		_SET(SP_FSCTRLREG0_FSICACHEINVALID, 1) |
		_SET(SP_FSCTRLREG0_FSFULLREGFOOTPRINT, 2) |
		_SET(SP_FSCTRLREG0_FSINOUTREGOVERLAP, 1) |
		_SET(SP_FSCTRLREG0_FSTHREADSIZE, SP_FOUR_PIX_QUADS) |
		_SET(SP_FSCTRLREG0_PIXLODENABLE, 1) |
		_SET(SP_FSCTRLREG0_FSLENGTH, 2);
	/* SP_FS_CTRL_REG1 */
	*cmds++ = _SET(SP_FSCTRLREG1_FSCONSTLENGTH, 1) |
		_SET(SP_FSCTRLREG1_FSINITIALOUTSTANDING, 2) |
		_SET(SP_FSCTRLREG1_HALFPRECVAROFFSET, 63);
	/* SP_FS_OBJ_OFFSET_REG */
	*cmds++ = _SET(SP_OBJOFFSETREG_CONSTOBJECTSTARTOFFSET, 128);
	/* SP_FS_OBJ_START_REG */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_SP_FS_FLAT_SHAD_MODE_REG_0);
	/* SP_FS_FLAT_SHAD_MODE_REG0 */
	*cmds++ = 0x00000000;
	/* SP_FS_FLAT_SHAD_MODE_REG1 */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_SP_FS_OUTPUT_REG);
	/* SP_FS_OUT_REG */
	*cmds++ = _SET(SP_FSOUTREG_PAD0, SP_PIXEL_BASED);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_SP_FS_MRT_REG_0);
	/* SP_FS_MRT_REG0 */
	*cmds++ = _SET(SP_FSMRTREG_REGID, 4);
	/* SP_FS_MRT_REG1 */
	*cmds++ = 0;
	/* SP_FS_MRT_REG2 */
	*cmds++ = 0;
	/* SP_FS_MRT_REG3 */
	*cmds++ = 0;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 11);
	*cmds++ = CP_REG(A3XX_VPC_ATTR);
	/* VPC_ATTR */
	*cmds++ = _SET(VPC_VPCATTR_TOTALATTR, 2) |
		_SET(VPC_VPCATTR_THRHDASSIGN, 1) |
		_SET(VPC_VPCATTR_LMSIZE, 1);
	/* VPC_PACK */
	*cmds++ = _SET(VPC_VPCPACK_NUMFPNONPOSVAR, 2) |
		_SET(VPC_VPCPACK_NUMNONPOSVSVAR, 2);
	/* VPC_VARYING_INTERP_MODE_0 */
	*cmds++ = 0x00000000;
	/* VPC_VARYING_INTERP_MODE1 */
	*cmds++ = 0x00000000;
	/* VPC_VARYING_INTERP_MODE2 */
	*cmds++ = 0x00000000;
	/* VPC_VARYING_IINTERP_MODE3 */
	*cmds++ = 0x00000000;
	/* VPC_VARRYING_PS_REPL_MODE_0 */
	*cmds++ = _SET(VPC_VPCVARPSREPLMODE_COMPONENT08, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT09, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0A,	1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0B, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0C, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0D, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0E, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0F, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT10, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT11, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT12, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT13, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT14, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT15, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT16, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT17, 2);
	/* VPC_VARRYING_PS_REPL_MODE_1 */
	*cmds++ = _SET(VPC_VPCVARPSREPLMODE_COMPONENT08, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT09, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0A,	1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0B, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0C, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0D, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0E, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0F, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT10, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT11, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT12, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT13, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT14, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT15, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT16, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT17, 2);
	/* VPC_VARRYING_PS_REPL_MODE_2 */
	*cmds++ = _SET(VPC_VPCVARPSREPLMODE_COMPONENT08, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT09, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0A,	1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0B, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0C, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0D, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0E, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0F, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT10, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT11, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT12, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT13, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT14, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT15, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT16, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT17, 2);
	/* VPC_VARRYING_PS_REPL_MODE_3 */
	*cmds++ = _SET(VPC_VPCVARPSREPLMODE_COMPONENT08, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT09, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0A,	1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0B, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0C, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0D, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0E, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT0F, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT10, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT11, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT12, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT13, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT14, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT15, 2) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT16, 1) |
		_SET(VPC_VPCVARPSREPLMODE_COMPONENT17, 2);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 11);
	*cmds++ = CP_REG(A3XX_SP_SP_CTRL_REG);
	/* SP_SP_CTRL_REG */
	*cmds++ = _SET(SP_SPCTRLREG_SLEEPMODE, 1);

	/* Load vertex shader */
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 10);
	*cmds++ = (0 << CP_LOADSTATE_DSTOFFSET_SHIFT)
		| (HLSQ_DIRECT << CP_LOADSTATE_STATESRC_SHIFT)
		| (HLSQ_BLOCK_ID_SP_VS << CP_LOADSTATE_STATEBLOCKID_SHIFT)
		| (1 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (HLSQ_SP_VS_INSTR << CP_LOADSTATE_STATETYPE_SHIFT)
		| (0 << CP_LOADSTATE_EXTSRCADDR_SHIFT);
	/* (sy)end; */
	*cmds++ = 0x00000000; *cmds++ = 0x13000000;
	/* nop; */
	*cmds++ = 0x00000000; *cmds++ = 0x00000000;
	/* nop; */
	*cmds++ = 0x00000000; *cmds++ = 0x00000000;
	/* nop; */
	*cmds++ = 0x00000000; *cmds++ = 0x00000000;

	/* Load fragment shader */
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 18);
	*cmds++ = (0 << CP_LOADSTATE_DSTOFFSET_SHIFT)
		| (HLSQ_DIRECT << CP_LOADSTATE_STATESRC_SHIFT)
		| (HLSQ_BLOCK_ID_SP_FS << CP_LOADSTATE_STATEBLOCKID_SHIFT)
		| (2 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (HLSQ_SP_FS_INSTR << CP_LOADSTATE_STATETYPE_SHIFT)
		| (0 << CP_LOADSTATE_EXTSRCADDR_SHIFT);
	/* (sy)(rpt1)bary.f (ei)r0.z, (r)0, r0.x; */
	*cmds++ = 0x00002000; *cmds++ = 0x57368902;
	/* (rpt5)nop; */
	*cmds++ = 0x00000000; *cmds++ = 0x00000500;
	/* sam (f32)r0.xyzw, r0.z, s#0, t#0; */
	*cmds++ = 0x00000005; *cmds++ = 0xa0c01f00;
	/* (sy)mov.f32f32 r1.x, r0.x; */
	*cmds++ = 0x00000000; *cmds++ = 0x30044004;
	/* mov.f32f32 r1.y, r0.y; */
	*cmds++ = 0x00000001; *cmds++ = 0x20044005;
	/* mov.f32f32 r1.z, r0.z; */
	*cmds++ = 0x00000002; *cmds++ = 0x20044006;
	/* mov.f32f32 r1.w, r0.w; */
	*cmds++ = 0x00000003; *cmds++ = 0x20044007;
	/* end; */
	*cmds++ = 0x00000000; *cmds++ = 0x03000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_VFD_CONTROL_0);
	/* VFD_CONTROL_0 */
	*cmds++ = _SET(VFD_CTRLREG0_TOTALATTRTOVS, 8) |
		_SET(VFD_CTRLREG0_PACKETSIZE, 2) |
		_SET(VFD_CTRLREG0_STRMDECINSTRCNT, 2) |
		_SET(VFD_CTRLREG0_STRMFETCHINSTRCNT, 2);
	/* VFD_CONTROL_1 */
	*cmds++ =  _SET(VFD_CTRLREG1_MAXSTORAGE, 2) |
		_SET(VFD_CTRLREG1_REGID4VTX, 252) |
		_SET(VFD_CTRLREG1_REGID4INST, 252);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_VFD_FETCH_INSTR_0_0);
	/* VFD_FETCH_INSTR_0_0 */
	*cmds++ = _SET(VFD_FETCHINSTRUCTIONS_FETCHSIZE, 7) |
		_SET(VFD_FETCHINSTRUCTIONS_BUFSTRIDE, 8) |
		_SET(VFD_FETCHINSTRUCTIONS_SWITCHNEXT, 1) |
		_SET(VFD_FETCHINSTRUCTIONS_STEPRATE, 1);
	/* VFD_FETCH_INSTR_1_0 */
	*cmds++ = _SET(VFD_BASEADDR_BASEADDR,
		shadow->quad_vertices_restore.gpuaddr);
	/* VFD_FETCH_INSTR_0_1 */
	*cmds++ = _SET(VFD_FETCHINSTRUCTIONS_FETCHSIZE, 11) |
		_SET(VFD_FETCHINSTRUCTIONS_BUFSTRIDE, 12) |
		_SET(VFD_FETCHINSTRUCTIONS_INDEXDECODE, 1) |
		_SET(VFD_FETCHINSTRUCTIONS_STEPRATE, 1);
	/* VFD_FETCH_INSTR_1_1 */
	*cmds++ = _SET(VFD_BASEADDR_BASEADDR,
		shadow->quad_vertices_restore.gpuaddr + 16);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_VFD_DECODE_INSTR_0);
	/* VFD_DECODE_INSTR_0 */
	*cmds++ = _SET(VFD_DECODEINSTRUCTIONS_WRITEMASK, 0x0F) |
		_SET(VFD_DECODEINSTRUCTIONS_CONSTFILL, 1) |
		_SET(VFD_DECODEINSTRUCTIONS_FORMAT, 1) |
		_SET(VFD_DECODEINSTRUCTIONS_SHIFTCNT, 8) |
		_SET(VFD_DECODEINSTRUCTIONS_LASTCOMPVALID, 1) |
		_SET(VFD_DECODEINSTRUCTIONS_SWITCHNEXT, 1);
	/* VFD_DECODE_INSTR_1 */
	*cmds++ = _SET(VFD_DECODEINSTRUCTIONS_WRITEMASK, 0x0F) |
		_SET(VFD_DECODEINSTRUCTIONS_CONSTFILL, 1) |
		_SET(VFD_DECODEINSTRUCTIONS_FORMAT, 2) |
		_SET(VFD_DECODEINSTRUCTIONS_REGID, 4) |
		_SET(VFD_DECODEINSTRUCTIONS_SHIFTCNT, 12) |
		_SET(VFD_DECODEINSTRUCTIONS_LASTCOMPVALID, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_DEPTH_CONTROL);
	/* RB_DEPTH_CONTROL */
	*cmds++ = _SET(RB_DEPTHCONTROL_Z_TEST_FUNC, RB_FRAG_NEVER);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_STENCIL_CONTROL);
	/* RB_STENCIL_CONTROL */
	*cmds++ = _SET(RB_STENCILCONTROL_STENCIL_FUNC, RB_REF_NEVER) |
		_SET(RB_STENCILCONTROL_STENCIL_FAIL, RB_STENCIL_KEEP) |
		_SET(RB_STENCILCONTROL_STENCIL_ZPASS, RB_STENCIL_KEEP) |
		_SET(RB_STENCILCONTROL_STENCIL_ZFAIL, RB_STENCIL_KEEP) |
		_SET(RB_STENCILCONTROL_STENCIL_FUNC_BF, RB_REF_NEVER) |
		_SET(RB_STENCILCONTROL_STENCIL_FAIL_BF, RB_STENCIL_KEEP) |
		_SET(RB_STENCILCONTROL_STENCIL_ZPASS_BF, RB_STENCIL_KEEP) |
		_SET(RB_STENCILCONTROL_STENCIL_ZFAIL_BF, RB_STENCIL_KEEP);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_MODE_CONTROL);
	/* RB_MODE_CONTROL */
	*cmds++ = _SET(RB_MODECONTROL_RENDER_MODE, RB_RENDERING_PASS) |
		_SET(RB_MODECONTROL_MARB_CACHE_SPLIT_MODE, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_RENDER_CONTROL);
	/* RB_RENDER_CONTROL */
	*cmds++ = _SET(RB_RENDERCONTROL_BIN_WIDTH, shadow->width >> 5) |
		_SET(RB_RENDERCONTROL_ALPHA_TEST_FUNC, 7);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_MSAA_CONTROL);
	/* RB_MSAA_CONTROL */
	*cmds++ = _SET(RB_MSAACONTROL_MSAA_DISABLE, 1) |
		_SET(RB_MSAACONTROL_SAMPLE_MASK, 0xFFFF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_MRT_CONTROL0);
	/* RB_MRT_CONTROL0 */
	*cmds++ = _SET(RB_MRTCONTROL_READ_DEST_ENABLE, 1) |
		_SET(RB_MRTCONTROL_ROP_CODE, 12) |
		_SET(RB_MRTCONTROL_DITHER_MODE, RB_DITHER_ALWAYS) |
		_SET(RB_MRTCONTROL_COMPONENT_ENABLE, 0xF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_RB_MRT_BLEND_CONTROL0);
	/* RB_MRT_BLENDCONTROL0 */
	*cmds++ = _SET(RB_MRTBLENDCONTROL_RGB_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_RGB_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_RGB_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_CLAMP_ENABLE, 1);
	/* RB_MRT_CONTROL1 */
	*cmds++ = _SET(RB_MRTCONTROL_READ_DEST_ENABLE, 1) |
		_SET(RB_MRTCONTROL_DITHER_MODE, RB_DITHER_DISABLE) |
		_SET(RB_MRTCONTROL_COMPONENT_ENABLE, 0xF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_RB_MRT_BLEND_CONTROL1);
	/* RB_MRT_BLENDCONTROL1 */
	*cmds++ = _SET(RB_MRTBLENDCONTROL_RGB_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_RGB_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_RGB_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_CLAMP_ENABLE, 1);
	/* RB_MRT_CONTROL2 */
	*cmds++ = _SET(RB_MRTCONTROL_READ_DEST_ENABLE, 1) |
		_SET(RB_MRTCONTROL_DITHER_MODE, RB_DITHER_DISABLE) |
		_SET(RB_MRTCONTROL_COMPONENT_ENABLE, 0xF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_RB_MRT_BLEND_CONTROL2);
	/* RB_MRT_BLENDCONTROL2 */
	*cmds++ = _SET(RB_MRTBLENDCONTROL_RGB_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_RGB_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_RGB_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_CLAMP_ENABLE, 1);
	/* RB_MRT_CONTROL3 */
	*cmds++ = _SET(RB_MRTCONTROL_READ_DEST_ENABLE, 1) |
		_SET(RB_MRTCONTROL_DITHER_MODE, RB_DITHER_DISABLE) |
		_SET(RB_MRTCONTROL_COMPONENT_ENABLE, 0xF);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_RB_MRT_BLEND_CONTROL3);
	/* RB_MRT_BLENDCONTROL3 */
	*cmds++ = _SET(RB_MRTBLENDCONTROL_RGB_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_RGB_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_RGB_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_SRC_FACTOR, RB_FACTOR_ONE) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_BLEND_OPCODE, RB_BLEND_OP_ADD) |
		_SET(RB_MRTBLENDCONTROL_ALPHA_DEST_FACTOR, RB_FACTOR_ZERO) |
		_SET(RB_MRTBLENDCONTROL_CLAMP_ENABLE, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_VFD_INDEX_MIN);
	/* VFD_INDEX_MIN */
	*cmds++ = 0x00000000;
	/* VFD_INDEX_MAX */
	*cmds++ = 0xFFFFFFFF;
	/* VFD_INDEX_OFFSET */
	*cmds++ = 0x00000000;
	/* TPL1_TP_VS_TEX_OFFSET */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_VFD_VS_THREADING_THRESHOLD);
	/* VFD_VS_THREADING_THRESHOLD */
	*cmds++ = _SET(VFD_THREADINGTHRESHOLD_RESERVED6, 12) |
		_SET(VFD_THREADINGTHRESHOLD_REGID_VTXCNT, 252);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_TPL1_TP_VS_TEX_OFFSET);
	/* TPL1_TP_VS_TEX_OFFSET */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_TPL1_TP_FS_TEX_OFFSET);
	/* TPL1_TP_FS_TEX_OFFSET */
	*cmds++ = _SET(TPL1_TPTEXOFFSETREG_SAMPLEROFFSET, 16) |
		_SET(TPL1_TPTEXOFFSETREG_MEMOBJOFFSET, 16) |
		_SET(TPL1_TPTEXOFFSETREG_BASETABLEPTR, 224);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_GRAS_SC_CONTROL);
	/* GRAS_SC_CONTROL */
	*cmds++ = _SET(GRAS_SC_CONTROL_RASTER_MODE, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_GRAS_SU_MODE_CONTROL);
	/* GRAS_SU_MODE_CONTROL */
	*cmds++ = 0x00000000;

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_GRAS_SC_WINDOW_SCISSOR_TL);
	/* GRAS_SC_WINDOW_SCISSOR_TL */
	*cmds++ = 0x00000000;
	/* GRAS_SC_WINDOW_SCISSOR_BR */
	*cmds++ = _SET(GRAS_SC_WINDOW_SCISSOR_BR_BR_X, shadow->width - 1) |
		_SET(GRAS_SC_WINDOW_SCISSOR_BR_BR_Y, shadow->height - 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_GRAS_SC_SCREEN_SCISSOR_TL);
	/* GRAS_SC_SCREEN_SCISSOR_TL */
	*cmds++ = 0x00000000;
	/* GRAS_SC_SCREEN_SCISSOR_BR */
	*cmds++ = _SET(GRAS_SC_SCREEN_SCISSOR_BR_BR_X, shadow->width - 1) |
		_SET(GRAS_SC_SCREEN_SCISSOR_BR_BR_Y, shadow->height - 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 5);
	*cmds++ = CP_REG(A3XX_GRAS_CL_VPORT_XOFFSET);
	/* GRAS_CL_VPORT_XOFFSET */
	*cmds++ = 0x00000000;
	/* GRAS_CL_VPORT_XSCALE */
	*cmds++ = _SET(GRAS_CL_VPORT_XSCALE_VPORT_XSCALE, 0x3F800000);
	/* GRAS_CL_VPORT_YOFFSET */
	*cmds++ = 0x00000000;
	/* GRAS_CL_VPORT_YSCALE */
	*cmds++ = _SET(GRAS_CL_VPORT_YSCALE_VPORT_YSCALE, 0x3F800000);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 3);
	*cmds++ = CP_REG(A3XX_GRAS_CL_VPORT_ZOFFSET);
	/* GRAS_CL_VPORT_ZOFFSET */
	*cmds++ = 0x00000000;
	/* GRAS_CL_VPORT_ZSCALE */
	*cmds++ = _SET(GRAS_CL_VPORT_ZSCALE_VPORT_ZSCALE, 0x3F800000);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_GRAS_CL_CLIP_CNTL);
	/* GRAS_CL_CLIP_CNTL */
	*cmds++ = _SET(GRAS_CL_CLIP_CNTL_IJ_PERSP_CENTER, 1);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_SP_FS_IMAGE_OUTPUT_REG_0);
	/* SP_FS_IMAGE_OUTPUT_REG_0 */
	*cmds++ = _SET(SP_IMAGEOUTPUTREG_MRTFORMAT, SP_R8G8B8A8_UNORM);

	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_PC_PRIM_VTX_CNTL);
	/* PC_PRIM_VTX_CONTROL */
	*cmds++ = _SET(PC_PRIM_VTX_CONTROL_STRIDE_IN_VPC, 2) |
		_SET(PC_PRIM_VTX_CONTROL_POLYMODE_FRONT_PTYPE,
		PC_DRAW_TRIANGLES) |
		_SET(PC_PRIM_VTX_CONTROL_POLYMODE_BACK_PTYPE,
		PC_DRAW_TRIANGLES) |
		_SET(PC_PRIM_VTX_CONTROL_PROVOKING_VTX_LAST, 1);

	*cmds++ = cp_type3_packet(CP_DRAW_INDX, 3);
	*cmds++ = 0x00000000; /* Viz query info */
	*cmds++ = BUILD_PC_DRAW_INITIATOR(PC_DI_PT_RECTLIST,
					  PC_DI_SRC_SEL_AUTO_INDEX,
					  PC_DI_INDEX_SIZE_16_BIT,
					  PC_DI_IGNORE_VISIBILITY);
	*cmds++ = 0x00000002; /* Num indices */

	/* Create indirect buffer command for above command sequence */
	create_ib1(drawctxt, shadow->gmem_restore, start, cmds);

	return cmds;
}

static void build_regrestore_cmds(struct adreno_device *adreno_dev,
				  struct adreno_context *drawctxt)
{
	unsigned int *start = tmp_ctx.cmd;
	unsigned int *cmd = start;
	unsigned int *lcc_start;

	int i;

	/* Flush HLSQ lazy updates */
	*cmd++ = cp_type3_packet(CP_EVENT_WRITE, 1);
	*cmd++ = 0x7;		/* HLSQ_FLUSH */
	*cmd++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;

	*cmd++ = cp_type0_packet(A3XX_UCHE_CACHE_INVALIDATE0_REG, 2);
	*cmd++ = 0x00000000;    /* No start addr for full invalidate */
	*cmd++ = (unsigned int)
		UCHE_ENTIRE_CACHE << UCHE_INVALIDATE1REG_ALLORPORTION |
		UCHE_OP_INVALIDATE << UCHE_INVALIDATE1REG_OPCODE |
		0;  /* No end addr for full invalidate */

	lcc_start = cmd;

	/* deferred cp_type3_packet(CP_LOAD_CONSTANT_CONTEXT, ???); */
	cmd++;

#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	/* Force mismatch */
	*cmd++ = ((drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000) | 1;
#else
	*cmd++ = (drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000;
#endif

	for (i = 0; i < ARRAY_SIZE(context_register_ranges) / 2; i++) {
		cmd = reg_range(cmd, context_register_ranges[i * 2],
				context_register_ranges[i * 2 + 1]);
	}

	lcc_start[0] = cp_type3_packet(CP_LOAD_CONSTANT_CONTEXT,
					(cmd - lcc_start) - 1);

#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	lcc_start[2] |= (0 << 24) | (4 << 16);	/* Disable shadowing. */
#else
	lcc_start[2] |= (1 << 24) | (4 << 16);
#endif

	for (i = 0; i < ARRAY_SIZE(global_registers); i++) {
		*cmd++ = cp_type0_packet(global_registers[i], 1);
		tmp_ctx.reg_values[i] = virt2gpu(cmd, &drawctxt->gpustate);
		*cmd++ = 0x00000000;
	}

	create_ib1(drawctxt, drawctxt->reg_restore, start, cmd);
	tmp_ctx.cmd = cmd;
}

static void build_constantrestore_cmds(struct adreno_device *adreno_dev,
				       struct adreno_context *drawctxt)
{
	unsigned int *cmd = tmp_ctx.cmd;
	unsigned int *start = cmd;
	unsigned int mode = 4;	/* Indirect mode */
	unsigned int stateblock;
	unsigned int numunits;
	unsigned int statetype;

	drawctxt->cond_execs[2].hostptr = cmd;
	drawctxt->cond_execs[2].gpuaddr = virt2gpu(cmd, &drawctxt->gpustate);
	*cmd++ = 0;
	drawctxt->cond_execs[3].hostptr = cmd;
	drawctxt->cond_execs[3].gpuaddr = virt2gpu(cmd, &drawctxt->gpustate);
	*cmd++ = 0;

#ifndef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	*cmd++ = cp_type3_packet(CP_LOAD_CONSTANT_CONTEXT, 3);
	*cmd++ = (drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000;
	*cmd++ = 4 << 16;
	*cmd++ = 0x0;
#endif
	/* HLSQ full update */
	*cmd++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmd++ = CP_REG(A3XX_HLSQ_CONTROL_0_REG);
	*cmd++ = 0x68000240;	/* A3XX_HLSQ_CONTROL_0_REG */

#ifndef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	/* Re-enable shadowing */
	*cmd++ = cp_type3_packet(CP_LOAD_CONSTANT_CONTEXT, 3);
	*cmd++ = (drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000;
	*cmd++ = (4 << 16) | (1 << 24);
	*cmd++ = 0x0;
#endif

	/* Load vertex shader constants */
	*cmd++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmd++ = drawctxt->cond_execs[2].gpuaddr >> 2;
	*cmd++ = drawctxt->cond_execs[2].gpuaddr >> 2;
	*cmd++ = 0x0000ffff;
	*cmd++ = 3; /* EXEC_COUNT */
	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	drawctxt->constant_load_commands[0].hostptr = cmd;
	drawctxt->constant_load_commands[0].gpuaddr = virt2gpu(cmd,
		&drawctxt->gpustate);

	/*
	   From fixup:

	   mode = 4 (indirect)
	   stateblock = 4 (Vertex constants)
	   numunits = SP_VS_CTRL_REG1.VSCONSTLENGTH * 2; (256bit units)

	   From register spec:
	   SP_VS_CTRL_REG1.VSCONSTLENGTH [09:00]: 0-512, unit = 128bits.

	   ord1 = (numunits<<22) | (stateblock<<19) | (mode<<16);
	 */

	*cmd++ = 0;		/* ord1 */
	*cmd++ = ((drawctxt->gpustate.gpuaddr) & 0xfffffffc) | 1;

	/* Load fragment shader constants */
	*cmd++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmd++ = drawctxt->cond_execs[3].gpuaddr >> 2;
	*cmd++ = drawctxt->cond_execs[3].gpuaddr >> 2;
	*cmd++ = 0x0000ffff;
	*cmd++ = 3; /* EXEC_COUNT */
	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	drawctxt->constant_load_commands[1].hostptr = cmd;
	drawctxt->constant_load_commands[1].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	/*
	   From fixup:

	   mode = 4 (indirect)
	   stateblock = 6 (Fragment constants)
	   numunits = SP_FS_CTRL_REG1.FSCONSTLENGTH * 2; (256bit units)

	   From register spec:
	   SP_FS_CTRL_REG1.FSCONSTLENGTH [09:00]: 0-512, unit = 128bits.

	   ord1 = (numunits<<22) | (stateblock<<19) | (mode<<16);
	 */

	*cmd++ = 0;		/* ord1 */
	drawctxt->constant_load_commands[2].hostptr = cmd;
	drawctxt->constant_load_commands[2].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	/*
	   From fixup:
	   base = drawctxt->gpustate.gpuaddr (ALU constant shadow base)
	   offset = SP_FS_OBJ_OFFSET_REG.CONSTOBJECTSTARTOFFSET

	   From register spec:
	   SP_FS_OBJ_OFFSET_REG.CONSTOBJECTSTARTOFFSET [16:24]: Constant object
	   start offset in on chip RAM,
	   128bit aligned

	   ord2 = base + offset | 1
	   Because of the base alignment we can use
	   ord2 = base | offset | 1
	 */
	*cmd++ = 0;		/* ord2 */

	/* Restore VS texture memory objects */
	stateblock = 0;
	statetype = 1;
	numunits = (TEX_SIZE_MEM_OBJECTS / 7) / 4;

	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	*cmd++ = (numunits << 22) | (stateblock << 19) | (mode << 16);
	*cmd++ = ((drawctxt->gpustate.gpuaddr + VS_TEX_OFFSET_MEM_OBJECTS)
	    & 0xfffffffc) | statetype;

	/* Restore VS texture mipmap addresses */
	stateblock = 1;
	statetype = 1;
	numunits = TEX_SIZE_MIPMAP / 4;
	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	*cmd++ = (numunits << 22) | (stateblock << 19) | (mode << 16);
	*cmd++ = ((drawctxt->gpustate.gpuaddr + VS_TEX_OFFSET_MIPMAP)
	    & 0xfffffffc) | statetype;

	/* Restore VS texture sampler objects */
	stateblock = 0;
	statetype = 0;
	numunits = (TEX_SIZE_SAMPLER_OBJ / 2) / 4;
	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	*cmd++ = (numunits << 22) | (stateblock << 19) | (mode << 16);
	*cmd++ = ((drawctxt->gpustate.gpuaddr + VS_TEX_OFFSET_SAMPLER_OBJ)
	    & 0xfffffffc) | statetype;

	/* Restore FS texture memory objects */
	stateblock = 2;
	statetype = 1;
	numunits = (TEX_SIZE_MEM_OBJECTS / 7) / 4;
	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	*cmd++ = (numunits << 22) | (stateblock << 19) | (mode << 16);
	*cmd++ = ((drawctxt->gpustate.gpuaddr + FS_TEX_OFFSET_MEM_OBJECTS)
	    & 0xfffffffc) | statetype;

	/* Restore FS texture mipmap addresses */
	stateblock = 3;
	statetype = 1;
	numunits = TEX_SIZE_MIPMAP / 4;
	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	*cmd++ = (numunits << 22) | (stateblock << 19) | (mode << 16);
	*cmd++ = ((drawctxt->gpustate.gpuaddr + FS_TEX_OFFSET_MIPMAP)
	    & 0xfffffffc) | statetype;

	/* Restore FS texture sampler objects */
	stateblock = 2;
	statetype = 0;
	numunits = (TEX_SIZE_SAMPLER_OBJ / 2) / 4;
	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	*cmd++ = (numunits << 22) | (stateblock << 19) | (mode << 16);
	*cmd++ = ((drawctxt->gpustate.gpuaddr + FS_TEX_OFFSET_SAMPLER_OBJ)
	    & 0xfffffffc) | statetype;

	create_ib1(drawctxt, drawctxt->constant_restore, start, cmd);
	tmp_ctx.cmd = cmd;
}

static void build_shader_restore_cmds(struct adreno_device *adreno_dev,
				      struct adreno_context *drawctxt)
{
	unsigned int *cmd = tmp_ctx.cmd;
	unsigned int *start = cmd;

	/* Vertex shader */
	*cmd++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmd++ = drawctxt->cond_execs[0].gpuaddr >> 2;
	*cmd++ = drawctxt->cond_execs[0].gpuaddr >> 2;
	*cmd++ = 1;
	*cmd++ = 3;		/* EXEC_COUNT */

	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	drawctxt->shader_load_commands[0].hostptr = cmd;
	drawctxt->shader_load_commands[0].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	/*
	   From fixup:

	   mode = 4 (indirect)
	   stateblock = 4 (Vertex shader)
	   numunits = SP_VS_CTRL_REG0.VS_LENGTH

	   From regspec:
	   SP_VS_CTRL_REG0.VS_LENGTH [31:24]: VS length, unit = 256bits.
	   If bit31 is 1, it means overflow
	   or any long shader.

	   ord1 = (numunits<<22) | (stateblock<<19) | (mode<<11)
	 */
	*cmd++ = 0;		/*ord1 */
	*cmd++ = (drawctxt->gpustate.gpuaddr + SHADER_OFFSET) & 0xfffffffc;

	/* Fragment shader */
	*cmd++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmd++ = drawctxt->cond_execs[1].gpuaddr >> 2;
	*cmd++ = drawctxt->cond_execs[1].gpuaddr >> 2;
	*cmd++ = 1;
	*cmd++ = 3;		/* EXEC_COUNT */

	*cmd++ = cp_type3_packet(CP_LOAD_STATE, 2);
	drawctxt->shader_load_commands[1].hostptr = cmd;
	drawctxt->shader_load_commands[1].gpuaddr =
	    virt2gpu(cmd, &drawctxt->gpustate);
	/*
	   From fixup:

	   mode = 4 (indirect)
	   stateblock = 6 (Fragment shader)
	   numunits = SP_FS_CTRL_REG0.FS_LENGTH

	   From regspec:
	   SP_FS_CTRL_REG0.FS_LENGTH [31:24]: FS length, unit = 256bits.
	   If bit31 is 1, it means overflow
	   or any long shader.

	   ord1 = (numunits<<22) | (stateblock<<19) | (mode<<11)
	 */
	*cmd++ = 0;		/*ord1 */
	*cmd++ = (drawctxt->gpustate.gpuaddr + SHADER_OFFSET
		  + (SHADER_SHADOW_SIZE / 2)) & 0xfffffffc;

	create_ib1(drawctxt, drawctxt->shader_restore, start, cmd);
	tmp_ctx.cmd = cmd;
}

static void build_hlsqcontrol_restore_cmds(struct adreno_device *adreno_dev,
					   struct adreno_context *drawctxt)
{
	unsigned int *cmd = tmp_ctx.cmd;
	unsigned int *start = cmd;

	*cmd++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmd++ = CP_REG(A3XX_HLSQ_CONTROL_0_REG);
	drawctxt->hlsqcontrol_restore_commands[0].hostptr = cmd;
	drawctxt->hlsqcontrol_restore_commands[0].gpuaddr
	    = virt2gpu(cmd, &drawctxt->gpustate);
	*cmd++ = 0;

	/* Create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->hlsqcontrol_restore, start, cmd);

	tmp_ctx.cmd = cmd;
}

/* IB that modifies the shader and constant sizes and offsets in restore IBs. */
static void build_restore_fixup_cmds(struct adreno_device *adreno_dev,
				     struct adreno_context *drawctxt)
{
	unsigned int *cmd = tmp_ctx.cmd;
	unsigned int *start = cmd;

#ifdef GSL_CONTEXT_SWITCH_CPU_SYNC
	/* Save shader sizes */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_VS_CTRL_REG0;
	*cmd++ = drawctxt->shader_load_commands[0].gpuaddr;

	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_FS_CTRL_REG0;
	*cmd++ = drawctxt->shader_load_commands[1].gpuaddr;

	/* Save constant sizes */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_VS_CTRL_REG1;
	*cmd++ = drawctxt->constant_load_commands[0].gpuaddr;

	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_FS_CTRL_REG1;
	*cmd++ = drawctxt->constant_load_commands[1].gpuaddr;

	/* Save constant offsets */
	*cmd++ = cp_type3_packet(CP_REG_TO_MEM, 2);
	*cmd++ = A3XX_SP_FS_OBJ_OFFSET_REG;
	*cmd++ = drawctxt->constant_load_commands[2].gpuaddr;
#else
	/* Save shader sizes */
	cmd = rmw_regtomem(cmd, A3XX_SP_VS_CTRL_REG0, 0x7f000000,
			   30, (4 << 19) | (4 << 16),
			   drawctxt->shader_load_commands[0].gpuaddr);

	cmd = rmw_regtomem(cmd, A3XX_SP_FS_CTRL_REG0, 0x7f000000,
			   30, (6 << 19) | (4 << 16),
			   drawctxt->shader_load_commands[1].gpuaddr);

	/* Save constant sizes */
	cmd = rmw_regtomem(cmd, A3XX_SP_VS_CTRL_REG1, 0x000003ff,
			   23, (4 << 19) | (4 << 16),
			   drawctxt->constant_load_commands[0].gpuaddr);

	cmd = rmw_regtomem(cmd, A3XX_SP_FS_CTRL_REG1, 0x000003ff,
			   23, (6 << 19) | (4 << 16),
			   drawctxt->constant_load_commands[1].gpuaddr);

	/* Modify constant restore conditionals */
	cmd = rmw_regtomem(cmd, A3XX_SP_VS_CTRL_REG1, 0x000003ff,
			0, 0, drawctxt->cond_execs[2].gpuaddr);

	cmd = rmw_regtomem(cmd, A3XX_SP_FS_CTRL_REG1, 0x000003ff,
			0, 0, drawctxt->cond_execs[3].gpuaddr);

	/* Save fragment constant shadow offset */
	cmd = rmw_regtomem(cmd, A3XX_SP_FS_OBJ_OFFSET_REG, 0x00ff0000,
			   18, (drawctxt->gpustate.gpuaddr & 0xfffffe00) | 1,
			   drawctxt->constant_load_commands[2].gpuaddr);
#endif

	/* Use mask value to avoid flushing HLSQ which would cause the HW to
	   discard all the shader data */

	cmd = rmw_regtomem(cmd,  A3XX_HLSQ_CONTROL_0_REG, 0x9ffffdff,
		0, 0, drawctxt->hlsqcontrol_restore_commands[0].gpuaddr);

	create_ib1(drawctxt, drawctxt->restore_fixup, start, cmd);

	tmp_ctx.cmd = cmd;
}

static int a3xx_create_gpustate_shadow(struct adreno_device *adreno_dev,
				     struct adreno_context *drawctxt)
{
	drawctxt->flags |= CTXT_FLAGS_STATE_SHADOW;

	build_regrestore_cmds(adreno_dev, drawctxt);
	build_constantrestore_cmds(adreno_dev, drawctxt);
	build_hlsqcontrol_restore_cmds(adreno_dev, drawctxt);
	build_regconstantsave_cmds(adreno_dev, drawctxt);
	build_shader_save_cmds(adreno_dev, drawctxt);
	build_shader_restore_cmds(adreno_dev, drawctxt);
	build_restore_fixup_cmds(adreno_dev, drawctxt);
	build_save_fixup_cmds(adreno_dev, drawctxt);

	return 0;
}

/* create buffers for saving/restoring registers, constants, & GMEM */
static int a3xx_create_gmem_shadow(struct adreno_device *adreno_dev,
				 struct adreno_context *drawctxt)
{
	calc_gmemsize(&drawctxt->context_gmem_shadow,
		adreno_dev->gmemspace.sizebytes);
	tmp_ctx.gmem_base = adreno_dev->gmemspace.gpu_base;

	if (drawctxt->flags & CTXT_FLAGS_GMEM_SHADOW) {
		int result =
		    kgsl_allocate(&drawctxt->context_gmem_shadow.gmemshadow,
			drawctxt->pagetable,
			drawctxt->context_gmem_shadow.size);

		if (result)
			return result;
	} else {
		memset(&drawctxt->context_gmem_shadow.gmemshadow, 0,
		       sizeof(drawctxt->context_gmem_shadow.gmemshadow));

		return 0;
	}

	build_quad_vtxbuff(drawctxt, &drawctxt->context_gmem_shadow,
		&tmp_ctx.cmd);

	/* Dow we need to idle? */
	/* adreno_idle(&adreno_dev->dev, KGSL_TIMEOUT_DEFAULT); */

	tmp_ctx.cmd = build_gmem2sys_cmds(adreno_dev, drawctxt,
		&drawctxt->context_gmem_shadow);
	tmp_ctx.cmd = build_sys2gmem_cmds(adreno_dev, drawctxt,
		&drawctxt->context_gmem_shadow);

	kgsl_cache_range_op(&drawctxt->context_gmem_shadow.gmemshadow,
		KGSL_CACHE_OP_FLUSH);

	return 0;
}

static int a3xx_drawctxt_create(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt)
{
	int ret;

	/*
	 * Allocate memory for the GPU state and the context commands.
	 * Despite the name, this is much more then just storage for
	 * the gpustate.  This contains command space for gmem save
	 * and texture and vertex buffer storage too
	 */

	ret = kgsl_allocate(&drawctxt->gpustate,
		drawctxt->pagetable, CONTEXT_SIZE);

	if (ret)
		return ret;

	kgsl_sharedmem_set(&drawctxt->gpustate, 0, 0, CONTEXT_SIZE);
	tmp_ctx.cmd = drawctxt->gpustate.hostptr + CMD_OFFSET;

	if (!(drawctxt->flags & CTXT_FLAGS_PREAMBLE)) {
		ret = a3xx_create_gpustate_shadow(adreno_dev, drawctxt);
		if (ret)
			goto done;

		drawctxt->flags |= CTXT_FLAGS_SHADER_SAVE;
	}

	if (!(drawctxt->flags & CTXT_FLAGS_NOGMEMALLOC))
		ret = a3xx_create_gmem_shadow(adreno_dev, drawctxt);

done:
	if (ret)
		kgsl_sharedmem_free(&drawctxt->gpustate);

	return ret;
}

static void a3xx_drawctxt_save(struct adreno_device *adreno_dev,
			   struct adreno_context *context)
{
	struct kgsl_device *device = &adreno_dev->dev;

	if (context == NULL)
		return;

	if (context->flags & CTXT_FLAGS_GPU_HANG)
		KGSL_CTXT_WARN(device,
			       "Current active context has caused gpu hang\n");

	if (!(context->flags & CTXT_FLAGS_PREAMBLE)) {
		/* Fixup self modifying IBs for save operations */
		adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_NONE,
			context->save_fixup, 3);

		/* save registers and constants. */
		adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_NONE,
			context->regconstant_save, 3);

		if (context->flags & CTXT_FLAGS_SHADER_SAVE) {
			/* Save shader instructions */
			adreno_ringbuffer_issuecmds(device,
				KGSL_CMD_FLAGS_PMODE, context->shader_save, 3);

			context->flags |= CTXT_FLAGS_SHADER_RESTORE;
		}
	}

	if ((context->flags & CTXT_FLAGS_GMEM_SAVE) &&
	    (context->flags & CTXT_FLAGS_GMEM_SHADOW)) {
		/*
		 * Save GMEM (note: changes shader. shader must
		 * already be saved.)
		 */

		adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_PMODE,
					    context->context_gmem_shadow.
					    gmem_save, 3);
		context->flags |= CTXT_FLAGS_GMEM_RESTORE;
	}
}

static void a3xx_drawctxt_restore(struct adreno_device *adreno_dev,
			      struct adreno_context *context)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int cmds[5];

	if (context == NULL) {
		/* No context - set the default pagetable and thats it */
		kgsl_mmu_setstate(device, device->mmu.defaultpagetable);
		return;
	}

	KGSL_CTXT_INFO(device, "context flags %08x\n", context->flags);

	cmds[0] = cp_nop_packet(1);
	cmds[1] = KGSL_CONTEXT_TO_MEM_IDENTIFIER;
	cmds[2] = cp_type3_packet(CP_MEM_WRITE, 2);
	cmds[3] = device->memstore.gpuaddr +
	    KGSL_DEVICE_MEMSTORE_OFFSET(current_context);
	cmds[4] = (unsigned int)context;
	adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_NONE, cmds, 5);
	kgsl_mmu_setstate(device, context->pagetable);

	/*
	 * Restore GMEM.  (note: changes shader.
	 * Shader must not already be restored.)
	 */

	if (context->flags & CTXT_FLAGS_GMEM_RESTORE) {
		adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_PMODE,
					    context->context_gmem_shadow.
					    gmem_restore, 3);
		context->flags &= ~CTXT_FLAGS_GMEM_RESTORE;
	}

	if (!(context->flags & CTXT_FLAGS_PREAMBLE)) {
		adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_NONE,
			context->reg_restore, 3);

		/* Fixup self modifying IBs for restore operations */
		adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_NONE,
			context->restore_fixup, 3);

		adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_NONE,
			context->constant_restore, 3);

		if (context->flags & CTXT_FLAGS_SHADER_RESTORE)
			adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_NONE,
				context->shader_restore, 3);

		/* Restore HLSQ_CONTROL_0 register */
		adreno_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_NONE,
			context->hlsqcontrol_restore, 3);
	}
}

static void a3xx_rb_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	unsigned int *cmds, cmds_gpu;
	cmds = adreno_ringbuffer_allocspace(rb, 18);
	cmds_gpu = rb->buffer_desc.gpuaddr + sizeof(uint) * (rb->wptr - 18);

	GSL_RB_WRITE(cmds, cmds_gpu, cp_type3_packet(CP_ME_INIT, 17));
	GSL_RB_WRITE(cmds, cmds_gpu, 0x000003f7);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000000);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000000);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000000);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000080);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000100);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000180);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00006600);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000150);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x0000014e);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000154);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000001);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000000);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000000);
	/* Protected mode control - turned off for A3XX */
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000000);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000000);
	GSL_RB_WRITE(cmds, cmds_gpu, 0x00000000);

	adreno_ringbuffer_submit(rb);
}

static void a3xx_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = &adreno_dev->dev;
	const char *err = "";

	switch (bit) {
	case A3XX_INT_RBBM_AHB_ERROR: {
		unsigned int reg;

		adreno_regread(device, A3XX_RBBM_AHB_ERROR_STATUS, &reg);

		/*
		 * Return the word address of the erroring register so that it
		 * matches the register specification
		 */

		KGSL_DRV_CRIT(device,
			"RBBM | AHB bus error | %s | addr=%x | ports=%x:%x\n",
			reg & (1 << 28) ? "WRITE" : "READ",
			(reg & 0xFFFFF) >> 2, (reg >> 20) & 0x3,
			(reg >> 24) & 0x3);

		/* Clear the error */
		adreno_regwrite(device, A3XX_RBBM_AHB_CMD, (1 << 3));
		return;
	}
	case A3XX_INT_RBBM_REG_TIMEOUT:
		err = "RBBM: AHB register timeout";
		break;
	case A3XX_INT_RBBM_ME_MS_TIMEOUT:
		err = "RBBM: ME master split timeout";
		break;
	case A3XX_INT_RBBM_PFP_MS_TIMEOUT:
		err = "RBBM: PFP master split timeout";
		break;
	case A3XX_INT_RBBM_ATB_BUS_OVERFLOW:
		err = "RBBM: ATB bus oveflow";
		break;
	case A3XX_INT_VFD_ERROR:
		err = "VFD: Out of bounds access";
		break;
	case A3XX_INT_CP_T0_PACKET_IN_IB:
		err = "ringbuffer TO packet in IB interrupt";
		break;
	case A3XX_INT_CP_OPCODE_ERROR:
		err = "ringbuffer opcode error interrupt";
		break;
	case A3XX_INT_CP_RESERVED_BIT_ERROR:
		err = "ringbuffer reserved bit error interrupt";
		break;
	case A3XX_INT_CP_HW_FAULT:
		err = "ringbuffer hardware fault";
		break;
	case A3XX_INT_CP_REG_PROTECT_FAULT:
		err = "ringbuffer protected mode error interrupt";
		break;
	case A3XX_INT_CP_AHB_ERROR_HALT:
		err = "ringbuffer AHB error interrupt";
		break;
	case A3XX_INT_MISC_HANG_DETECT:
		err = "MISC: GPU hang detected";
		break;
	case A3XX_INT_UCHE_OOB_ACCESS:
		err = "UCHE:  Out of bounds access";
		break;
	}

	KGSL_DRV_CRIT(device, "%s\n", err);
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
}

static void a3xx_cp_callback(struct adreno_device *adreno_dev, int irq)
{
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

	if (irq == A3XX_INT_CP_RB_INT) {
		kgsl_sharedmem_writel(&rb->device->memstore,
			KGSL_DEVICE_MEMSTORE_OFFSET(ts_cmp_enable), 0);
		wmb();
		KGSL_CMD_WARN(rb->device, "ringbuffer rb interrupt\n");
	}

	wake_up_interruptible_all(&rb->device->wait_queue);

	/* Schedule work to free mem and issue ibs */
	queue_work(rb->device->work_queue, &rb->device->ts_expired_ws);

	atomic_notifier_call_chain(&rb->device->ts_notifier_list,
				   rb->device->id, NULL);
}

#define A3XX_IRQ_CALLBACK(_c) { .func = _c }

#define A3XX_INT_MASK \
	((1 << A3XX_INT_RBBM_AHB_ERROR) |        \
	 (1 << A3XX_INT_RBBM_REG_TIMEOUT) |      \
	 (1 << A3XX_INT_RBBM_ME_MS_TIMEOUT) |    \
	 (1 << A3XX_INT_RBBM_PFP_MS_TIMEOUT) |   \
	 (1 << A3XX_INT_RBBM_ATB_BUS_OVERFLOW) | \
	 (1 << A3XX_INT_VFD_ERROR) |             \
	 (1 << A3XX_INT_CP_T0_PACKET_IN_IB) |    \
	 (1 << A3XX_INT_CP_OPCODE_ERROR) |       \
	 (1 << A3XX_INT_CP_RESERVED_BIT_ERROR) | \
	 (1 << A3XX_INT_CP_HW_FAULT) |           \
	 (1 << A3XX_INT_CP_IB1_INT) |            \
	 (1 << A3XX_INT_CP_IB2_INT) |            \
	 (1 << A3XX_INT_CP_RB_INT) |             \
	 (1 << A3XX_INT_CP_REG_PROTECT_FAULT) |  \
	 (1 << A3XX_INT_CP_AHB_ERROR_HALT) |     \
	 (1 << A3XX_INT_MISC_HANG_DETECT) |      \
	 (1 << A3XX_INT_UCHE_OOB_ACCESS))

static struct {
	void (*func)(struct adreno_device *, int);
} a3xx_irq_funcs[] = {
	A3XX_IRQ_CALLBACK(NULL),               /* 0 - RBBM_GPU_IDLE */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 1 - RBBM_AHB_ERROR */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 2 - RBBM_REG_TIMEOUT */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 3 - RBBM_ME_MS_TIMEOUT */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 4 - RBBM_PFP_MS_TIMEOUT */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 5 - RBBM_ATB_BUS_OVERFLOW */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 6 - RBBM_VFD_ERROR */
	A3XX_IRQ_CALLBACK(NULL),	       /* 7 - CP_SW */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 8 - CP_T0_PACKET_IN_IB */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 9 - CP_OPCODE_ERROR */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 10 - CP_RESERVED_BIT_ERROR */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 11 - CP_HW_FAULT */
	A3XX_IRQ_CALLBACK(NULL),	       /* 12 - CP_DMA */
	A3XX_IRQ_CALLBACK(a3xx_cp_callback),   /* 13 - CP_IB2_INT */
	A3XX_IRQ_CALLBACK(a3xx_cp_callback),   /* 14 - CP_IB1_INT */
	A3XX_IRQ_CALLBACK(a3xx_cp_callback),   /* 15 - CP_RB_INT */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 16 - CP_REG_PROTECT_FAULT */
	A3XX_IRQ_CALLBACK(NULL),	       /* 17 - CP_RB_DONE_TS */
	A3XX_IRQ_CALLBACK(NULL),	       /* 18 - CP_VS_DONE_TS */
	A3XX_IRQ_CALLBACK(NULL),	       /* 19 - CP_PS_DONE_TS */
	A3XX_IRQ_CALLBACK(NULL),	       /* 20 - CP_CACHE_FLUSH_TS */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 21 - CP_AHB_ERROR_FAULT */
	A3XX_IRQ_CALLBACK(NULL),	       /* 22 - Unused */
	A3XX_IRQ_CALLBACK(NULL),	       /* 23 - Unused */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 24 - MISC_HANG_DETECT */
	A3XX_IRQ_CALLBACK(a3xx_err_callback),  /* 25 - UCHE_OOB_ACCESS */
	/* 26 to 31 - Unused */
};

static irqreturn_t a3xx_irq_handler(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	irqreturn_t ret = IRQ_NONE;
	unsigned int status, tmp;
	int i;

	adreno_regread(&adreno_dev->dev, A3XX_RBBM_INT_0_STATUS, &status);

	for (tmp = status, i = 0; tmp && i < ARRAY_SIZE(a3xx_irq_funcs); i++) {
		if (tmp & 1) {
			if (a3xx_irq_funcs[i].func != NULL) {
				a3xx_irq_funcs[i].func(adreno_dev, i);
				ret = IRQ_HANDLED;
			} else {
				KGSL_DRV_CRIT(device,
					"Unhandled interrupt bit %x\n", i);
			}
		}

		tmp >>= 1;
	}

	if (status)
		adreno_regwrite(&adreno_dev->dev, A3XX_RBBM_INT_CLEAR_CMD,
			status);
	return ret;
}

static void a3xx_irq_control(struct adreno_device *adreno_dev, int state)
{
	struct kgsl_device *device = &adreno_dev->dev;

	if (state)
		adreno_regwrite(device, A3XX_RBBM_INT_0_MASK, A3XX_INT_MASK);
	else
		adreno_regwrite(device, A3XX_RBBM_INT_0_MASK, 0);
}

static unsigned int a3xx_busy_cycles(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int reg, val;

	/* Freeze the counter */
	adreno_regread(device, A3XX_RBBM_RBBM_CTL, &reg);
	reg &= ~RBBM_RBBM_CTL_ENABLE_PWR_CTR1;
	adreno_regwrite(device, A3XX_RBBM_RBBM_CTL, reg);

	/* Read the value */
	adreno_regread(device, A3XX_RBBM_PERFCTR_PWR_1_LO, &val);

	/* Reset the counter */
	reg |= RBBM_RBBM_CTL_RESET_PWR_CTR1;
	adreno_regwrite(device, A3XX_RBBM_RBBM_CTL, reg);

	/* Re-enable the counter */
	reg &= ~RBBM_RBBM_CTL_RESET_PWR_CTR1;
	reg |= RBBM_RBBM_CTL_ENABLE_PWR_CTR1;
	adreno_regwrite(device, A3XX_RBBM_RBBM_CTL, reg);

	return val;
}

static void a3xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;

	/* Reset the core */
	adreno_regwrite(device, A3XX_RBBM_SW_RESET_CMD,
		0x00000001);
	msleep(20);

	/*
	 * enable fixed master AXI port of 0x0 for all clients to keep
	 * traffic from going to random places
	 */

	adreno_regwrite(device, A3XX_VBIF_FIXED_SORT_EN, 0x0001003F);
	adreno_regwrite(device, A3XX_VBIF_FIXED_SORT_SEL0, 0x00000000);
	adreno_regwrite(device, A3XX_VBIF_FIXED_SORT_SEL1, 0x00000000);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	adreno_regwrite(device, A3XX_RBBM_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/* Enable the RBBM error reporting bits.  This lets us get
	   useful information on failure */

	adreno_regwrite(device, A3XX_RBBM_AHB_CTL0, 0x00000001);

	/* Enable AHB error reporting */
	adreno_regwrite(device, A3XX_RBBM_AHB_CTL1, 0xA6FFFFFF);

	/* Turn on the power counters */
	adreno_regwrite(device, A3XX_RBBM_RBBM_CTL, 0x00003000);
}

struct adreno_gpudev adreno_a3xx_gpudev = {
	.reg_rbbm_status = A3XX_RBBM_STATUS,
	.reg_cp_pfp_ucode_addr = A3XX_CP_PFP_UCODE_ADDR,
	.reg_cp_pfp_ucode_data = A3XX_CP_PFP_UCODE_DATA,

	.ctxt_create = a3xx_drawctxt_create,
	.ctxt_save = a3xx_drawctxt_save,
	.ctxt_restore = a3xx_drawctxt_restore,
	.rb_init = a3xx_rb_init,
	.irq_control = a3xx_irq_control,
	.irq_handler = a3xx_irq_handler,
	.busy_cycles = a3xx_busy_cycles,
	.start = a3xx_start,
};
