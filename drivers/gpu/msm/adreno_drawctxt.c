/* Copyright (c) 2002,2007-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/slab.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "adreno_drawctxt.h"

/*
 *
 *  Memory Map for Register, Constant & Instruction Shadow, and Command Buffers
 *  (34.5KB)
 *
 *  +---------------------+------------+-------------+---+---------------------+
 *  | ALU Constant Shadow | Reg Shadow | C&V Buffers |Tex| Shader Instr Shadow |
 *  +---------------------+------------+-------------+---+---------------------+
 *    ________________________________/               \____________________
 *   /                                                                     |
 *  +--------------+-----------+------+-----------+------------------------+
 *  | Restore Regs | Save Regs | Quad | Gmem Save | Gmem Restore | unused  |
 *  +--------------+-----------+------+-----------+------------------------+
 *
 *              8K - ALU Constant Shadow (8K aligned)
 *              4K - H/W Register Shadow (8K aligned)
 *              4K - Command and Vertex Buffers
 *                         - Indirect command buffer : Const/Reg restore
 *                               - includes Loop & Bool const shadows
 *                         - Indirect command buffer : Const/Reg save
 *                         - Quad vertices & texture coordinates
 *                         - Indirect command buffer : Gmem save
 *                         - Indirect command buffer : Gmem restore
 *                         - Unused (padding to 8KB boundary)
 *             <1K - Texture Constant Shadow (768 bytes) (8K aligned)
 *       18K - Shader Instruction Shadow
 *               - 6K vertex (32 byte aligned)
 *               - 6K pixel  (32 byte aligned)
 *               - 6K shared (32 byte aligned)
 *
 *  Note: Reading constants into a shadow, one at a time using REG_TO_MEM, takes
 *  3 DWORDS per DWORD transfered, plus 1 DWORD for the shadow, for a total of
 *  16 bytes per constant.  If the texture constants were transfered this way,
 *  the Command & Vertex Buffers section would extend past the 16K boundary.
 *  By moving the texture constant shadow area to start at 16KB boundary, we
 *  only require approximately 40 bytes more memory, but are able to use the
 *  LOAD_CONSTANT_CONTEXT shadowing feature for the textures, speeding up
 *  context switching.
 *
 *  [Using LOAD_CONSTANT_CONTEXT shadowing feature for the Loop and/or Bool
 *  constants would require an additional 8KB each, for alignment.]
 *
 */

/* Constants */

#define ALU_CONSTANTS	2048	/* DWORDS */
#define NUM_REGISTERS	1024	/* DWORDS */
#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
#define CMD_BUFFER_LEN	9216	/* DWORDS */
#else
#define CMD_BUFFER_LEN	3072	/* DWORDS */
#endif
#define TEX_CONSTANTS		(32*6)	/* DWORDS */
#define BOOL_CONSTANTS		8	/* DWORDS */
#define LOOP_CONSTANTS		56	/* DWORDS */
#define SHADER_INSTRUCT_LOG2	9U	/* 2^n == SHADER_INSTRUCTIONS */

#if defined(PM4_IM_STORE)
/* 96-bit instructions */
#define SHADER_INSTRUCT		(1<<SHADER_INSTRUCT_LOG2)
#else
#define SHADER_INSTRUCT		0
#endif

/* LOAD_CONSTANT_CONTEXT shadow size */
#define LCC_SHADOW_SIZE		0x2000	/* 8KB */

#define ALU_SHADOW_SIZE		LCC_SHADOW_SIZE	/* 8KB */
#define REG_SHADOW_SIZE		0x1000	/* 4KB */
#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
#define CMD_BUFFER_SIZE		0x9000	/* 36KB */
#else
#define CMD_BUFFER_SIZE		0x3000	/* 12KB */
#endif
#define TEX_SHADOW_SIZE		(TEX_CONSTANTS*4)	/* 768 bytes */
#define SHADER_SHADOW_SIZE	(SHADER_INSTRUCT*12)	/* 6KB */

#define REG_OFFSET		LCC_SHADOW_SIZE
#define CMD_OFFSET		(REG_OFFSET + REG_SHADOW_SIZE)
#define TEX_OFFSET		(CMD_OFFSET + CMD_BUFFER_SIZE)
#define SHADER_OFFSET		((TEX_OFFSET + TEX_SHADOW_SIZE + 32) & ~31)

#define CONTEXT_SIZE		(SHADER_OFFSET + 3 * SHADER_SHADOW_SIZE)

/* temporary work structure */
struct tmp_ctx {
	unsigned int *start;	/* Command & Vertex buffer start */
	unsigned int *cmd;	/* Next available dword in C&V buffer */

	/* address of buffers, needed when creating IB1 command buffers. */
	uint32_t bool_shadow;	/* bool constants */
	uint32_t loop_shadow;	/* loop constants */

#if defined(PM4_IM_STORE)
	uint32_t shader_shared;	/* shared shader instruction shadow */
	uint32_t shader_vertex;	/* vertex shader instruction shadow */
	uint32_t shader_pixel;	/* pixel shader instruction shadow */
#endif

	/* Addresses in command buffer where separately handled registers
	 * are saved
	 */
	uint32_t reg_values[33];
	uint32_t chicken_restore;

	uint32_t gmem_base;	/* Base gpu address of GMEM */

};

/* Helper function to calculate IEEE754 single precision float values
*  without FPU
*/
unsigned int uint2float(unsigned int uintval)
{
	unsigned int exp, frac = 0;

	if (uintval == 0)
		return 0;

	exp = ilog2(uintval);

	/* Calculate fraction */
	if (23 > exp)
		frac = (uintval & (~(1 << exp))) << (23 - exp);

	/* Exp is biased by 127 and shifted 23 bits */
	exp = (exp + 127) << 23;

	return exp | frac;
}

/* context save (gmem -> sys) */

/* pre-compiled vertex shader program
*
*  attribute vec4  P;
*  void main(void)
*  {
*    gl_Position = P;
*  }
*/
#define GMEM2SYS_VTX_PGM_LEN	0x12

static unsigned int gmem2sys_vtx_pgm[GMEM2SYS_VTX_PGM_LEN] = {
	0x00011003, 0x00001000, 0xc2000000,
	0x00001004, 0x00001000, 0xc4000000,
	0x00001005, 0x00002000, 0x00000000,
	0x1cb81000, 0x00398a88, 0x00000003,
	0x140f803e, 0x00000000, 0xe2010100,
	0x14000000, 0x00000000, 0xe2000000
};

/* pre-compiled fragment shader program
*
*  precision highp float;
*  uniform   vec4  clear_color;
*  void main(void)
*  {
*     gl_FragColor = clear_color;
*  }
*/

#define GMEM2SYS_FRAG_PGM_LEN	0x0c

static unsigned int gmem2sys_frag_pgm[GMEM2SYS_FRAG_PGM_LEN] = {
	0x00000000, 0x1002c400, 0x10000000,
	0x00001003, 0x00002000, 0x00000000,
	0x140f8000, 0x00000000, 0x22000000,
	0x14000000, 0x00000000, 0xe2000000
};

/* context restore (sys -> gmem) */
/* pre-compiled vertex shader program
*
*  attribute vec4 position;
*  attribute vec4 texcoord;
*  varying   vec4 texcoord0;
*  void main()
*  {
*     gl_Position = position;
*     texcoord0 = texcoord;
*  }
*/

#define SYS2GMEM_VTX_PGM_LEN	0x18

static unsigned int sys2gmem_vtx_pgm[SYS2GMEM_VTX_PGM_LEN] = {
	0x00052003, 0x00001000, 0xc2000000, 0x00001005,
	0x00001000, 0xc4000000, 0x00001006, 0x10071000,
	0x20000000, 0x18981000, 0x0039ba88, 0x00000003,
	0x12982000, 0x40257b08, 0x00000002, 0x140f803e,
	0x00000000, 0xe2010100, 0x140f8000, 0x00000000,
	0xe2020200, 0x14000000, 0x00000000, 0xe2000000
};

/* pre-compiled fragment shader program
*
*  precision mediump   float;
*  uniform   sampler2D tex0;
*  varying   vec4      texcoord0;
*  void main()
*  {
*     gl_FragColor = texture2D(tex0, texcoord0.xy);
*  }
*/

#define SYS2GMEM_FRAG_PGM_LEN	0x0f

static unsigned int sys2gmem_frag_pgm[SYS2GMEM_FRAG_PGM_LEN] = {
	0x00011002, 0x00001000, 0xc4000000, 0x00001003,
	0x10041000, 0x20000000, 0x10000001, 0x1ffff688,
	0x00000002, 0x140f8000, 0x00000000, 0xe2000000,
	0x14000000, 0x00000000, 0xe2000000
};

/* shader texture constants (sysmem -> gmem)  */
#define SYS2GMEM_TEX_CONST_LEN	6

static unsigned int sys2gmem_tex_const[SYS2GMEM_TEX_CONST_LEN] = {
	/* Texture, FormatXYZW=Unsigned, ClampXYZ=Wrap/Repeat,
	 * RFMode=ZeroClamp-1, Dim=1:2d
	 */
	0x00000002,		/* Pitch = TBD */

	/* Format=6:8888_WZYX, EndianSwap=0:None, ReqSize=0:256bit, DimHi=0,
	 * NearestClamp=1:OGL Mode
	 */
	0x00000800,		/* Address[31:12] = TBD */

	/* Width, Height, EndianSwap=0:None */
	0,			/* Width & Height = TBD */

	/* NumFormat=0:RF, DstSelXYZW=XYZW, ExpAdj=0, MagFilt=MinFilt=0:Point,
	 * Mip=2:BaseMap
	 */
	0 << 1 | 1 << 4 | 2 << 7 | 3 << 10 | 2 << 23,

	/* VolMag=VolMin=0:Point, MinMipLvl=0, MaxMipLvl=1, LodBiasH=V=0,
	 * Dim3d=0
	 */
	0,

	/* BorderColor=0:ABGRBlack, ForceBC=0:diable, TriJuice=0, Aniso=0,
	 * Dim=1:2d, MipPacking=0
	 */
	1 << 9			/* Mip Address[31:12] = TBD */
};

/* quad for copying GMEM to context shadow */
#define QUAD_LEN				12

static unsigned int gmem_copy_quad[QUAD_LEN] = {
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000
};

#define TEXCOORD_LEN			8

static unsigned int gmem_copy_texcoord[TEXCOORD_LEN] = {
	0x00000000, 0x3f800000,
	0x3f800000, 0x3f800000,
	0x00000000, 0x00000000,
	0x3f800000, 0x00000000
};

#define NUM_COLOR_FORMATS   13

static enum SURFACEFORMAT surface_format_table[NUM_COLOR_FORMATS] = {
	FMT_4_4_4_4,		/* COLORX_4_4_4_4 */
	FMT_1_5_5_5,		/* COLORX_1_5_5_5 */
	FMT_5_6_5,		/* COLORX_5_6_5 */
	FMT_8,			/* COLORX_8 */
	FMT_8_8,		/* COLORX_8_8 */
	FMT_8_8_8_8,		/* COLORX_8_8_8_8 */
	FMT_8_8_8_8,		/* COLORX_S8_8_8_8 */
	FMT_16_FLOAT,		/* COLORX_16_FLOAT */
	FMT_16_16_FLOAT,	/* COLORX_16_16_FLOAT */
	FMT_16_16_16_16_FLOAT,	/* COLORX_16_16_16_16_FLOAT */
	FMT_32_FLOAT,		/* COLORX_32_FLOAT */
	FMT_32_32_FLOAT,	/* COLORX_32_32_FLOAT */
	FMT_32_32_32_32_FLOAT,	/* COLORX_32_32_32_32_FLOAT */
};

static unsigned int format2bytesperpixel[NUM_COLOR_FORMATS] = {
	2,			/* COLORX_4_4_4_4 */
	2,			/* COLORX_1_5_5_5 */
	2,			/* COLORX_5_6_5 */
	1,			/* COLORX_8 */
	2,			/* COLORX_8_8 8*/
	4,			/* COLORX_8_8_8_8 */
	4,			/* COLORX_S8_8_8_8 */
	2,			/* COLORX_16_FLOAT */
	4,			/* COLORX_16_16_FLOAT */
	8,			/* COLORX_16_16_16_16_FLOAT */
	4,			/* COLORX_32_FLOAT */
	8,			/* COLORX_32_32_FLOAT */
	16,			/* COLORX_32_32_32_32_FLOAT */
};

/* shader linkage info */
#define SHADER_CONST_ADDR	(11 * 6 + 3)

/* gmem command buffer length */
#define PM4_REG(reg)		((0x4 << 16) | (GSL_HAL_SUBBLOCK_OFFSET(reg)))

/* functions */
static void config_gmemsize(struct gmem_shadow_t *shadow, int gmem_size)
{
	int w = 64, h = 64;	/* 16KB surface, minimum */

	shadow->format = COLORX_8_8_8_8;
	/* convert from bytes to 32-bit words */
	gmem_size = (gmem_size + 3) / 4;

	/* find the right surface size, close to a square. */
	while (w * h < gmem_size)
		if (w < h)
			w *= 2;
		else
			h *= 2;

	shadow->width = w;
	shadow->pitch = w;
	shadow->height = h;
	shadow->gmem_pitch = shadow->pitch;

	shadow->size = shadow->pitch * shadow->height * 4;
}

static unsigned int gpuaddr(unsigned int *cmd, struct kgsl_memdesc *memdesc)
{
	return memdesc->gpuaddr + ((char *)cmd - (char *)memdesc->hostptr);
}

static void
create_ib1(struct adreno_context *drawctxt, unsigned int *cmd,
	   unsigned int *start, unsigned int *end)
{
	cmd[0] = PM4_HDR_INDIRECT_BUFFER_PFD;
	cmd[1] = gpuaddr(start, &drawctxt->gpustate);
	cmd[2] = end - start;
}

static unsigned int *program_shader(unsigned int *cmds, int vtxfrag,
				    unsigned int *shader_pgm, int dwords)
{
	/* load the patched vertex shader stream */
	*cmds++ = pm4_type3_packet(PM4_IM_LOAD_IMMEDIATE, 2 + dwords);
	/* 0=vertex shader, 1=fragment shader */
	*cmds++ = vtxfrag;
	/* instruction start & size (in 32-bit words) */
	*cmds++ = ((0 << 16) | dwords);

	memcpy(cmds, shader_pgm, dwords << 2);
	cmds += dwords;

	return cmds;
}

static unsigned int *reg_to_mem(unsigned int *cmds, uint32_t dst,
				uint32_t src, int dwords)
{
	while (dwords-- > 0) {
		*cmds++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
		*cmds++ = src++;
		*cmds++ = dst;
		dst += 4;
	}

	return cmds;
}

#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES

static void build_reg_to_mem_range(unsigned int start, unsigned int end,
				   unsigned int **cmd,
				   struct adreno_context *drawctxt)
{
	unsigned int i = start;

	for (i = start; i <= end; i++) {
		*(*cmd)++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
		*(*cmd)++ = i;
		*(*cmd)++ =
		    ((drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000) +
		    (i - 0x2000) * 4;
	}
}

#endif

/* chicken restore */
static unsigned int *build_chicken_restore_cmds(
					struct adreno_context *drawctxt,
					struct tmp_ctx *ctx)
{
	unsigned int *start = ctx->cmd;
	unsigned int *cmds = start;

	*cmds++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmds++ = 0;

	*cmds++ = pm4_type0_packet(REG_TP0_CHICKEN, 1);
	ctx->chicken_restore = gpuaddr(cmds, &drawctxt->gpustate);
	*cmds++ = 0x00000000;

	/* create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->chicken_restore, start, cmds);

	return cmds;
}

/****************************************************************************/
/* context save                                                             */
/****************************************************************************/

static const unsigned int register_ranges_a20x[] = {
	REG_RB_SURFACE_INFO, REG_RB_DEPTH_INFO,
	REG_COHER_DEST_BASE_0, REG_PA_SC_SCREEN_SCISSOR_BR,
	REG_PA_SC_WINDOW_OFFSET, REG_PA_SC_WINDOW_SCISSOR_BR,
	REG_RB_STENCILREFMASK_BF, REG_PA_CL_VPORT_ZOFFSET,
	REG_SQ_PROGRAM_CNTL, REG_SQ_WRAPPING_1,
	REG_PA_SC_LINE_CNTL, REG_SQ_PS_CONST,
	REG_PA_SC_AA_MASK, REG_PA_SC_AA_MASK,
	REG_RB_SAMPLE_COUNT_CTL, REG_RB_COLOR_DEST_MASK,
	REG_PA_SU_POLY_OFFSET_FRONT_SCALE, REG_PA_SU_POLY_OFFSET_BACK_OFFSET,
	REG_VGT_MAX_VTX_INDX, REG_RB_FOG_COLOR,
	REG_RB_DEPTHCONTROL, REG_RB_MODECONTROL,
	REG_PA_SU_POINT_SIZE, REG_PA_SC_LINE_STIPPLE,
	REG_PA_SC_VIZ_QUERY, REG_PA_SC_VIZ_QUERY,
	REG_VGT_VERTEX_REUSE_BLOCK_CNTL, REG_RB_DEPTH_CLEAR
};

static const unsigned int register_ranges_a22x[] = {
	REG_RB_SURFACE_INFO, REG_RB_DEPTH_INFO,
	REG_COHER_DEST_BASE_0, REG_PA_SC_SCREEN_SCISSOR_BR,
	REG_PA_SC_WINDOW_OFFSET, REG_PA_SC_WINDOW_SCISSOR_BR,
	REG_RB_STENCILREFMASK_BF, REG_PA_CL_VPORT_ZOFFSET,
	REG_SQ_PROGRAM_CNTL, REG_SQ_WRAPPING_1,
	REG_PA_SC_LINE_CNTL, REG_SQ_PS_CONST,
	REG_PA_SC_AA_MASK, REG_PA_SC_AA_MASK,
	REG_RB_SAMPLE_COUNT_CTL, REG_RB_COLOR_DEST_MASK,
	REG_PA_SU_POLY_OFFSET_FRONT_SCALE, REG_PA_SU_POLY_OFFSET_BACK_OFFSET,
	/* all the below registers are specific to Leia */
	REG_LEIA_PC_MAX_VTX_INDX, REG_LEIA_PC_INDX_OFFSET,
	REG_RB_COLOR_MASK, REG_RB_FOG_COLOR,
	REG_RB_DEPTHCONTROL, REG_RB_COLORCONTROL,
	REG_PA_CL_CLIP_CNTL, REG_PA_CL_VTE_CNTL,
	REG_RB_MODECONTROL, REG_RB_SAMPLE_POS,
	REG_PA_SU_POINT_SIZE, REG_PA_SU_LINE_CNTL,
	REG_LEIA_PC_VERTEX_REUSE_BLOCK_CNTL,
	REG_LEIA_PC_VERTEX_REUSE_BLOCK_CNTL,
	REG_RB_COPY_CONTROL, REG_RB_DEPTH_CLEAR
};


/* save h/w regs, alu constants, texture contants, etc. ...
*  requires: bool_shadow_gpuaddr, loop_shadow_gpuaddr
*/
static void build_regsave_cmds(struct adreno_device *adreno_dev,
			       struct adreno_context *drawctxt,
			       struct tmp_ctx *ctx)
{
	unsigned int *start = ctx->cmd;
	unsigned int *cmd = start;

	*cmd++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;

#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	/* Make sure the HW context has the correct register values
	 * before reading them. */
	*cmd++ = pm4_type3_packet(PM4_CONTEXT_UPDATE, 1);
	*cmd++ = 0;

	{
		unsigned int i = 0;
		unsigned int reg_array_size = 0;
		const unsigned int *ptr_register_ranges;

		/* Based on chip id choose the register ranges */
		if (adreno_is_a220(adreno_dev)) {
			ptr_register_ranges = register_ranges_a22x;
			reg_array_size = ARRAY_SIZE(register_ranges_a22x);
		} else {
			ptr_register_ranges = register_ranges_a20x;
			reg_array_size = ARRAY_SIZE(register_ranges_a20x);
		}


		/* Write HW registers into shadow */
		for (i = 0; i < (reg_array_size/2) ; i++) {
			build_reg_to_mem_range(ptr_register_ranges[i*2],
					ptr_register_ranges[i*2+1],
					&cmd, drawctxt);
		}
	}

	/* Copy ALU constants */
	cmd =
	    reg_to_mem(cmd, (drawctxt->gpustate.gpuaddr) & 0xFFFFE000,
		       REG_SQ_CONSTANT_0, ALU_CONSTANTS);

	/* Copy Tex constants */
	cmd =
	    reg_to_mem(cmd,
		       (drawctxt->gpustate.gpuaddr + TEX_OFFSET) & 0xFFFFE000,
		       REG_SQ_FETCH_0, TEX_CONSTANTS);
#else

	/* Insert a wait for idle packet before reading the registers.
	 * This is to fix a hang/reset seen during stress testing.  In this
	 * hang, CP encountered a timeout reading SQ's boolean constant
	 * register. There is logic in the HW that blocks reading of this
	 * register when the SQ block is not idle, which we believe is
	 * contributing to the hang.*/
	*cmd++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;

	/* H/w registers are already shadowed; just need to disable shadowing
	 * to prevent corruption.
	 */
	*cmd++ = pm4_type3_packet(PM4_LOAD_CONSTANT_CONTEXT, 3);
	*cmd++ = (drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000;
	*cmd++ = 4 << 16;	/* regs, start=0 */
	*cmd++ = 0x0;		/* count = 0 */

	/* ALU constants are already shadowed; just need to disable shadowing
	 * to prevent corruption.
	 */
	*cmd++ = pm4_type3_packet(PM4_LOAD_CONSTANT_CONTEXT, 3);
	*cmd++ = drawctxt->gpustate.gpuaddr & 0xFFFFE000;
	*cmd++ = 0 << 16;	/* ALU, start=0 */
	*cmd++ = 0x0;		/* count = 0 */

	/* Tex constants are already shadowed; just need to disable shadowing
	 *  to prevent corruption.
	 */
	*cmd++ = pm4_type3_packet(PM4_LOAD_CONSTANT_CONTEXT, 3);
	*cmd++ = (drawctxt->gpustate.gpuaddr + TEX_OFFSET) & 0xFFFFE000;
	*cmd++ = 1 << 16;	/* Tex, start=0 */
	*cmd++ = 0x0;		/* count = 0 */
#endif

	/* Need to handle some of the registers separately */
	*cmd++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
	*cmd++ = REG_SQ_GPR_MANAGEMENT;
	*cmd++ = ctx->reg_values[0];

	*cmd++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
	*cmd++ = REG_TP0_CHICKEN;
	*cmd++ = ctx->reg_values[1];

	if (adreno_is_a220(adreno_dev)) {
		unsigned int i;
		unsigned int j = 2;
		for (i = REG_LEIA_VSC_BIN_SIZE; i <=
				REG_LEIA_VSC_PIPE_DATA_LENGTH_7; i++) {
			*cmd++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
			*cmd++ = i;
			*cmd++ = ctx->reg_values[j];
			j++;
		}
	}

	/* Copy Boolean constants */
	cmd = reg_to_mem(cmd, ctx->bool_shadow, REG_SQ_CF_BOOLEANS,
			 BOOL_CONSTANTS);

	/* Copy Loop constants */
	cmd = reg_to_mem(cmd, ctx->loop_shadow, REG_SQ_CF_LOOP, LOOP_CONSTANTS);

	/* create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->reg_save, start, cmd);

	ctx->cmd = cmd;
}

/*copy colour, depth, & stencil buffers from graphics memory to system memory*/
static unsigned int *build_gmem2sys_cmds(struct adreno_device *adreno_dev,
					 struct adreno_context *drawctxt,
					 struct tmp_ctx *ctx,
					 struct gmem_shadow_t *shadow)
{
	unsigned int *cmds = shadow->gmem_save_commands;
	unsigned int *start = cmds;
	/* Calculate the new offset based on the adjusted base */
	unsigned int bytesperpixel = format2bytesperpixel[shadow->format];
	unsigned int addr = shadow->gmemshadow.gpuaddr;
	unsigned int offset = (addr - (addr & 0xfffff000)) / bytesperpixel;

	/* Store TP0_CHICKEN register */
	*cmds++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
	*cmds++ = REG_TP0_CHICKEN;
	if (ctx)
		*cmds++ = ctx->chicken_restore;
	else
		cmds++;

	*cmds++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmds++ = 0;

	/* Set TP0_CHICKEN to zero */
	*cmds++ = pm4_type0_packet(REG_TP0_CHICKEN, 1);
	*cmds++ = 0x00000000;

	/* Set PA_SC_AA_CONFIG to 0 */
	*cmds++ = pm4_type0_packet(REG_PA_SC_AA_CONFIG, 1);
	*cmds++ = 0x00000000;

	/* program shader */

	/* load shader vtx constants ... 5 dwords */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 4);
	*cmds++ = (0x1 << 16) | SHADER_CONST_ADDR;
	*cmds++ = 0;
	/* valid(?) vtx constant flag & addr */
	*cmds++ = shadow->quad_vertices.gpuaddr | 0x3;
	/* limit = 12 dwords */
	*cmds++ = 0x00000030;

	/* Invalidate L2 cache to make sure vertices are updated */
	*cmds++ = pm4_type0_packet(REG_TC_CNTL_STATUS, 1);
	*cmds++ = 0x1;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 4);
	*cmds++ = PM4_REG(REG_VGT_MAX_VTX_INDX);
	*cmds++ = 0x00ffffff;	/* REG_VGT_MAX_VTX_INDX */
	*cmds++ = 0x0;		/* REG_VGT_MIN_VTX_INDX */
	*cmds++ = 0x00000000;	/* REG_VGT_INDX_OFFSET */

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_SC_AA_MASK);
	*cmds++ = 0x0000ffff;	/* REG_PA_SC_AA_MASK */

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_COLORCONTROL);
	*cmds++ = 0x00000c20;

	/* load the patched vertex shader stream */
	cmds = program_shader(cmds, 0, gmem2sys_vtx_pgm, GMEM2SYS_VTX_PGM_LEN);

	/* Load the patched fragment shader stream */
	cmds =
	    program_shader(cmds, 1, gmem2sys_frag_pgm, GMEM2SYS_FRAG_PGM_LEN);

	/* SQ_PROGRAM_CNTL / SQ_CONTEXT_MISC */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_SQ_PROGRAM_CNTL);
	if (adreno_is_a220(adreno_dev))
		*cmds++ = 0x10018001;
	else
		*cmds++ = 0x10010001;
	*cmds++ = 0x00000008;

	/* resolve */

	/* PA_CL_VTE_CNTL */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_CL_VTE_CNTL);
	/* disable X/Y/Z transforms, X/Y/Z are premultiplied by W */
	*cmds++ = 0x00000b00;

	/* program surface info */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_RB_SURFACE_INFO);
	*cmds++ = shadow->gmem_pitch;	/* pitch, MSAA = 1 */

	/* RB_COLOR_INFO Endian=none, Linear, Format=RGBA8888, Swap=0,
	 *                Base=gmem_base
	 */
	/* gmem base assumed 4K aligned. */
	if (ctx) {
		BUG_ON(ctx->gmem_base & 0xFFF);
		*cmds++ =
		    (shadow->
		     format << RB_COLOR_INFO__COLOR_FORMAT__SHIFT) | ctx->
		    gmem_base;
	} else {
		unsigned int temp = *cmds;
		*cmds++ = (temp & ~RB_COLOR_INFO__COLOR_FORMAT_MASK) |
			(shadow->format << RB_COLOR_INFO__COLOR_FORMAT__SHIFT);
	}

	/* disable Z */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_DEPTHCONTROL);
	if (adreno_is_a220(adreno_dev))
		*cmds++ = 0x08;
	else
		*cmds++ = 0;

	/* set REG_PA_SU_SC_MODE_CNTL
	 *              Front_ptype = draw triangles
	 *              Back_ptype = draw triangles
	 *              Provoking vertex = last
	 */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_SU_SC_MODE_CNTL);
	*cmds++ = 0x00080240;

	/* Use maximum scissor values -- quad vertices already have the
	 * correct bounds */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_PA_SC_SCREEN_SCISSOR_TL);
	*cmds++ = (0 << 16) | 0;
	*cmds++ = (0x1fff << 16) | (0x1fff);
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_PA_SC_WINDOW_SCISSOR_TL);
	*cmds++ = (unsigned int)((1U << 31) | (0 << 16) | 0);
	*cmds++ = (0x1fff << 16) | (0x1fff);

	/* load the viewport so that z scale = clear depth and
	 *  z offset = 0.0f
	 */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_PA_CL_VPORT_ZSCALE);
	*cmds++ = 0xbf800000;	/* -1.0f */
	*cmds++ = 0x0;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_COLOR_MASK);
	*cmds++ = 0x0000000f;	/* R = G = B = 1:enabled */

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_COLOR_DEST_MASK);
	*cmds++ = 0xffffffff;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_SQ_WRAPPING_0);
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;

	/* load the stencil ref value
	 * $AAM - do this later
	 */

	/* load the COPY state */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 6);
	*cmds++ = PM4_REG(REG_RB_COPY_CONTROL);
	*cmds++ = 0;		/* RB_COPY_CONTROL */
	*cmds++ = addr & 0xfffff000;	/* RB_COPY_DEST_BASE */
	*cmds++ = shadow->pitch >> 5;	/* RB_COPY_DEST_PITCH */

	/* Endian=none, Linear, Format=RGBA8888,Swap=0,!Dither,
	 *  MaskWrite:R=G=B=A=1
	 */
	*cmds++ = 0x0003c008 |
	    (shadow->format << RB_COPY_DEST_INFO__COPY_DEST_FORMAT__SHIFT);
	/* Make sure we stay in offsetx field. */
	BUG_ON(offset & 0xfffff000);
	*cmds++ = offset;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_MODECONTROL);
	*cmds++ = 0x6;		/* EDRAM copy */

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_CL_CLIP_CNTL);
	*cmds++ = 0x00010000;

	if (adreno_is_a220(adreno_dev)) {
		*cmds++ = pm4_type3_packet(PM4_SET_DRAW_INIT_FLAGS, 1);
		*cmds++ = 0;

		*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
		*cmds++ = PM4_REG(REG_LEIA_RB_LRZ_VSC_CONTROL);
		*cmds++ = 0x0000000;

		*cmds++ = pm4_type3_packet(PM4_DRAW_INDX, 3);
		*cmds++ = 0;           /* viz query info. */
		/* PrimType=RectList, SrcSel=AutoIndex, VisCullMode=Ignore */
		*cmds++ = 0x00004088;
		*cmds++ = 3;	       /* NumIndices=3 */
	} else {
		/* queue the draw packet */
		*cmds++ = pm4_type3_packet(PM4_DRAW_INDX, 2);
		*cmds++ = 0;		/* viz query info. */
		/* PrimType=RectList, NumIndices=3, SrcSel=AutoIndex */
		*cmds++ = 0x00030088;
	}

	/* create indirect buffer command for above command sequence */
	create_ib1(drawctxt, shadow->gmem_save, start, cmds);

	return cmds;
}

/* context restore */

/*copy colour, depth, & stencil buffers from system memory to graphics memory*/
static unsigned int *build_sys2gmem_cmds(struct adreno_device *adreno_dev,
					 struct adreno_context *drawctxt,
					 struct tmp_ctx *ctx,
					 struct gmem_shadow_t *shadow)
{
	unsigned int *cmds = shadow->gmem_restore_commands;
	unsigned int *start = cmds;

	/* Store TP0_CHICKEN register */
	*cmds++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
	*cmds++ = REG_TP0_CHICKEN;
	if (ctx)
		*cmds++ = ctx->chicken_restore;
	else
		cmds++;

	*cmds++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmds++ = 0;

	/* Set TP0_CHICKEN to zero */
	*cmds++ = pm4_type0_packet(REG_TP0_CHICKEN, 1);
	*cmds++ = 0x00000000;

	/* Set PA_SC_AA_CONFIG to 0 */
	*cmds++ = pm4_type0_packet(REG_PA_SC_AA_CONFIG, 1);
	*cmds++ = 0x00000000;
	/* shader constants */

	/* vertex buffer constants */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 7);

	*cmds++ = (0x1 << 16) | (9 * 6);
	/* valid(?) vtx constant flag & addr */
	*cmds++ = shadow->quad_vertices.gpuaddr | 0x3;
	/* limit = 12 dwords */
	*cmds++ = 0x00000030;
	/* valid(?) vtx constant flag & addr */
	*cmds++ = shadow->quad_texcoords.gpuaddr | 0x3;
	/* limit = 8 dwords */
	*cmds++ = 0x00000020;
	*cmds++ = 0;
	*cmds++ = 0;

	/* Invalidate L2 cache to make sure vertices are updated */
	*cmds++ = pm4_type0_packet(REG_TC_CNTL_STATUS, 1);
	*cmds++ = 0x1;

	cmds = program_shader(cmds, 0, sys2gmem_vtx_pgm, SYS2GMEM_VTX_PGM_LEN);

	/* Load the patched fragment shader stream */
	cmds =
	    program_shader(cmds, 1, sys2gmem_frag_pgm, SYS2GMEM_FRAG_PGM_LEN);

	/* SQ_PROGRAM_CNTL / SQ_CONTEXT_MISC */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_SQ_PROGRAM_CNTL);
	*cmds++ = 0x10030002;
	*cmds++ = 0x00000008;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_SC_AA_MASK);
	*cmds++ = 0x0000ffff;	/* REG_PA_SC_AA_MASK */

	if (!adreno_is_a220(adreno_dev)) {
		/* PA_SC_VIZ_QUERY */
		*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
		*cmds++ = PM4_REG(REG_PA_SC_VIZ_QUERY);
		*cmds++ = 0x0;		/*REG_PA_SC_VIZ_QUERY */
	}

	/* RB_COLORCONTROL */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_COLORCONTROL);
	*cmds++ = 0x00000c20;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 4);
	*cmds++ = PM4_REG(REG_VGT_MAX_VTX_INDX);
	*cmds++ = 0x00ffffff;	/* mmVGT_MAX_VTX_INDX */
	*cmds++ = 0x0;		/* mmVGT_MIN_VTX_INDX */
	*cmds++ = 0x00000000;	/* mmVGT_INDX_OFFSET */

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_VGT_VERTEX_REUSE_BLOCK_CNTL);
	*cmds++ = 0x00000002;	/* mmVGT_VERTEX_REUSE_BLOCK_CNTL */
	*cmds++ = 0x00000002;	/* mmVGT_OUT_DEALLOC_CNTL */

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_SQ_INTERPOLATOR_CNTL);
	*cmds++ = 0xffffffff;	/* mmSQ_INTERPOLATOR_CNTL */

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_SC_AA_CONFIG);
	*cmds++ = 0x00000000;	/* REG_PA_SC_AA_CONFIG */

	/* set REG_PA_SU_SC_MODE_CNTL
	 * Front_ptype = draw triangles
	 * Back_ptype = draw triangles
	 * Provoking vertex = last
	 */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_SU_SC_MODE_CNTL);
	*cmds++ = 0x00080240;

	/* texture constants */
	*cmds++ =
	    pm4_type3_packet(PM4_SET_CONSTANT, (SYS2GMEM_TEX_CONST_LEN + 1));
	*cmds++ = (0x1 << 16) | (0 * 6);
	memcpy(cmds, sys2gmem_tex_const, SYS2GMEM_TEX_CONST_LEN << 2);
	cmds[0] |= (shadow->pitch >> 5) << 22;
	cmds[1] |=
	    shadow->gmemshadow.gpuaddr | surface_format_table[shadow->format];
	cmds[2] |= (shadow->width - 1) | (shadow->height - 1) << 13;
	cmds += SYS2GMEM_TEX_CONST_LEN;

	/* program surface info */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_RB_SURFACE_INFO);
	*cmds++ = shadow->gmem_pitch;	/* pitch, MSAA = 1 */

	/* RB_COLOR_INFO Endian=none, Linear, Format=RGBA8888, Swap=0,
	 *                Base=gmem_base
	 */
	if (ctx)
		*cmds++ =
		    (shadow->
		     format << RB_COLOR_INFO__COLOR_FORMAT__SHIFT) | ctx->
		    gmem_base;
	else {
		unsigned int temp = *cmds;
		*cmds++ = (temp & ~RB_COLOR_INFO__COLOR_FORMAT_MASK) |
			(shadow->format << RB_COLOR_INFO__COLOR_FORMAT__SHIFT);
	}

	/* RB_DEPTHCONTROL */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_DEPTHCONTROL);

	if (adreno_is_a220(adreno_dev))
		*cmds++ = 8;		/* disable Z */
	else
		*cmds++ = 0;		/* disable Z */

	/* Use maximum scissor values -- quad vertices already
	 * have the correct bounds */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_PA_SC_SCREEN_SCISSOR_TL);
	*cmds++ = (0 << 16) | 0;
	*cmds++ = ((0x1fff) << 16) | 0x1fff;
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_PA_SC_WINDOW_SCISSOR_TL);
	*cmds++ = (unsigned int)((1U << 31) | (0 << 16) | 0);
	*cmds++ = ((0x1fff) << 16) | 0x1fff;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_CL_VTE_CNTL);
	/* disable X/Y/Z transforms, X/Y/Z are premultiplied by W */
	*cmds++ = 0x00000b00;

	/*load the viewport so that z scale = clear depth and z offset = 0.0f */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_PA_CL_VPORT_ZSCALE);
	*cmds++ = 0xbf800000;
	*cmds++ = 0x0;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_COLOR_MASK);
	*cmds++ = 0x0000000f;	/* R = G = B = 1:enabled */

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_COLOR_DEST_MASK);
	*cmds++ = 0xffffffff;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 3);
	*cmds++ = PM4_REG(REG_SQ_WRAPPING_0);
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;

	/* load the stencil ref value
	 *  $AAM - do this later
	 */
	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_RB_MODECONTROL);
	/* draw pixels with color and depth/stencil component */
	*cmds++ = 0x4;

	*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
	*cmds++ = PM4_REG(REG_PA_CL_CLIP_CNTL);
	*cmds++ = 0x00010000;

	if (adreno_is_a220(adreno_dev)) {
		*cmds++ = pm4_type3_packet(PM4_SET_DRAW_INIT_FLAGS, 1);
		*cmds++ = 0;

		*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
		*cmds++ = PM4_REG(REG_LEIA_RB_LRZ_VSC_CONTROL);
		*cmds++ = 0x0000000;

		*cmds++ = pm4_type3_packet(PM4_DRAW_INDX, 3);
		*cmds++ = 0;           /* viz query info. */
		/* PrimType=RectList, SrcSel=AutoIndex, VisCullMode=Ignore */
		*cmds++ = 0x00004088;
		*cmds++ = 3;	       /* NumIndices=3 */
	} else {
		/* queue the draw packet */
		*cmds++ = pm4_type3_packet(PM4_DRAW_INDX, 2);
		*cmds++ = 0;		/* viz query info. */
		/* PrimType=RectList, NumIndices=3, SrcSel=AutoIndex */
		*cmds++ = 0x00030088;
	}

	/* create indirect buffer command for above command sequence */
	create_ib1(drawctxt, shadow->gmem_restore, start, cmds);

	return cmds;
}

/* restore h/w regs, alu constants, texture constants, etc. ... */
static unsigned *reg_range(unsigned int *cmd, unsigned int start,
			   unsigned int end)
{
	*cmd++ = PM4_REG(start);	/* h/w regs, start addr */
	*cmd++ = end - start + 1;	/* count */
	return cmd;
}

static void build_regrestore_cmds(struct adreno_device *adreno_dev,
				  struct adreno_context *drawctxt,
				  struct tmp_ctx *ctx)
{
	unsigned int *start = ctx->cmd;
	unsigned int *cmd = start;

	unsigned int i = 0;
	unsigned int reg_array_size = 0;
	const unsigned int *ptr_register_ranges;

	*cmd++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;

	/* H/W Registers */
	/* deferred pm4_type3_packet(PM4_LOAD_CONSTANT_CONTEXT, ???); */
	cmd++;
#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	/* Force mismatch */
	*cmd++ = ((drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000) | 1;
#else
	*cmd++ = (drawctxt->gpustate.gpuaddr + REG_OFFSET) & 0xFFFFE000;
#endif

	/* Based on chip id choose the registers ranges*/
	if (adreno_is_a220(adreno_dev)) {
		ptr_register_ranges = register_ranges_a22x;
		reg_array_size = ARRAY_SIZE(register_ranges_a22x);
	} else {
		ptr_register_ranges = register_ranges_a20x;
		reg_array_size = ARRAY_SIZE(register_ranges_a20x);
	}


	for (i = 0; i < (reg_array_size/2); i++) {
		cmd = reg_range(cmd, ptr_register_ranges[i*2],
				ptr_register_ranges[i*2+1]);
	}

	/* Now we know how many register blocks we have, we can compute command
	 * length
	 */
	start[2] =
	    pm4_type3_packet(PM4_LOAD_CONSTANT_CONTEXT, (cmd - start) - 3);
	/* Enable shadowing for the entire register block. */
#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	start[4] |= (0 << 24) | (4 << 16);	/* Disable shadowing. */
#else
	start[4] |= (1 << 24) | (4 << 16);
#endif

	/* Need to handle some of the registers separately */
	*cmd++ = pm4_type0_packet(REG_SQ_GPR_MANAGEMENT, 1);
	ctx->reg_values[0] = gpuaddr(cmd, &drawctxt->gpustate);
	*cmd++ = 0x00040400;

	*cmd++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;
	*cmd++ = pm4_type0_packet(REG_TP0_CHICKEN, 1);
	ctx->reg_values[1] = gpuaddr(cmd, &drawctxt->gpustate);
	*cmd++ = 0x00000000;

	if (adreno_is_a220(adreno_dev)) {
		unsigned int i;
		unsigned int j = 2;
		for (i = REG_LEIA_VSC_BIN_SIZE; i <=
				REG_LEIA_VSC_PIPE_DATA_LENGTH_7; i++) {
			*cmd++ = pm4_type0_packet(i, 1);
			ctx->reg_values[j] = gpuaddr(cmd, &drawctxt->gpustate);
			*cmd++ = 0x00000000;
			j++;
		}
	}

	/* ALU Constants */
	*cmd++ = pm4_type3_packet(PM4_LOAD_CONSTANT_CONTEXT, 3);
	*cmd++ = drawctxt->gpustate.gpuaddr & 0xFFFFE000;
#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	*cmd++ = (0 << 24) | (0 << 16) | 0;	/* Disable shadowing */
#else
	*cmd++ = (1 << 24) | (0 << 16) | 0;
#endif
	*cmd++ = ALU_CONSTANTS;

	/* Texture Constants */
	*cmd++ = pm4_type3_packet(PM4_LOAD_CONSTANT_CONTEXT, 3);
	*cmd++ = (drawctxt->gpustate.gpuaddr + TEX_OFFSET) & 0xFFFFE000;
#ifdef CONFIG_MSM_KGSL_DISABLE_SHADOW_WRITES
	/* Disable shadowing */
	*cmd++ = (0 << 24) | (1 << 16) | 0;
#else
	*cmd++ = (1 << 24) | (1 << 16) | 0;
#endif
	*cmd++ = TEX_CONSTANTS;

	/* Boolean Constants */
	*cmd++ = pm4_type3_packet(PM4_SET_CONSTANT, 1 + BOOL_CONSTANTS);
	*cmd++ = (2 << 16) | 0;

	/* the next BOOL_CONSTANT dwords is the shadow area for
	 *  boolean constants.
	 */
	ctx->bool_shadow = gpuaddr(cmd, &drawctxt->gpustate);
	cmd += BOOL_CONSTANTS;

	/* Loop Constants */
	*cmd++ = pm4_type3_packet(PM4_SET_CONSTANT, 1 + LOOP_CONSTANTS);
	*cmd++ = (3 << 16) | 0;

	/* the next LOOP_CONSTANTS dwords is the shadow area for
	 * loop constants.
	 */
	ctx->loop_shadow = gpuaddr(cmd, &drawctxt->gpustate);
	cmd += LOOP_CONSTANTS;

	/* create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->reg_restore, start, cmd);

	ctx->cmd = cmd;
}

/* quad for saving/restoring gmem */
static void set_gmem_copy_quad(struct gmem_shadow_t *shadow)
{
	/* set vertex buffer values */
	gmem_copy_quad[1] = uint2float(shadow->height);
	gmem_copy_quad[3] = uint2float(shadow->width);
	gmem_copy_quad[4] = uint2float(shadow->height);
	gmem_copy_quad[9] = uint2float(shadow->width);

	gmem_copy_quad[0] = uint2float(0);
	gmem_copy_quad[6] = uint2float(0);
	gmem_copy_quad[7] = uint2float(0);
	gmem_copy_quad[10] = uint2float(0);

	memcpy(shadow->quad_vertices.hostptr, gmem_copy_quad, QUAD_LEN << 2);

	memcpy(shadow->quad_texcoords.hostptr, gmem_copy_texcoord,
	       TEXCOORD_LEN << 2);
}

/* quad for saving/restoring gmem */
static void build_quad_vtxbuff(struct adreno_context *drawctxt,
		       struct tmp_ctx *ctx, struct gmem_shadow_t *shadow)
{
	unsigned int *cmd = ctx->cmd;

	/* quad vertex buffer location (in GPU space) */
	shadow->quad_vertices.hostptr = cmd;
	shadow->quad_vertices.gpuaddr = gpuaddr(cmd, &drawctxt->gpustate);

	cmd += QUAD_LEN;

	/* tex coord buffer location (in GPU space) */
	shadow->quad_texcoords.hostptr = cmd;
	shadow->quad_texcoords.gpuaddr = gpuaddr(cmd, &drawctxt->gpustate);

	cmd += TEXCOORD_LEN;

	set_gmem_copy_quad(shadow);

	ctx->cmd = cmd;
}

static void
build_shader_save_restore_cmds(struct adreno_context *drawctxt,
			       struct tmp_ctx *ctx)
{
	unsigned int *cmd = ctx->cmd;
	unsigned int *save, *restore, *fixup;
#if defined(PM4_IM_STORE)
	unsigned int *startSizeVtx, *startSizePix, *startSizeShared;
#endif
	unsigned int *partition1;
	unsigned int *shaderBases, *partition2;

#if defined(PM4_IM_STORE)
	/* compute vertex, pixel and shared instruction shadow GPU addresses */
	ctx->shader_vertex = drawctxt->gpustate.gpuaddr + SHADER_OFFSET;
	ctx->shader_pixel = ctx->shader_vertex + SHADER_SHADOW_SIZE;
	ctx->shader_shared = ctx->shader_pixel + SHADER_SHADOW_SIZE;
#endif

	/* restore shader partitioning and instructions */

	restore = cmd;		/* start address */

	/* Invalidate Vertex & Pixel instruction code address and sizes */
	*cmd++ = pm4_type3_packet(PM4_INVALIDATE_STATE, 1);
	*cmd++ = 0x00000300;	/* 0x100 = Vertex, 0x200 = Pixel */

	/* Restore previous shader vertex & pixel instruction bases. */
	*cmd++ = pm4_type3_packet(PM4_SET_SHADER_BASES, 1);
	shaderBases = cmd++;	/* TBD #5: shader bases (from fixup) */

	/* write the shader partition information to a scratch register */
	*cmd++ = pm4_type0_packet(REG_SQ_INST_STORE_MANAGMENT, 1);
	partition1 = cmd++;	/* TBD #4a: partition info (from save) */

#if defined(PM4_IM_STORE)
	/* load vertex shader instructions from the shadow. */
	*cmd++ = pm4_type3_packet(PM4_IM_LOAD, 2);
	*cmd++ = ctx->shader_vertex + 0x0;	/* 0x0 = Vertex */
	startSizeVtx = cmd++;	/* TBD #1: start/size (from save) */

	/* load pixel shader instructions from the shadow. */
	*cmd++ = pm4_type3_packet(PM4_IM_LOAD, 2);
	*cmd++ = ctx->shader_pixel + 0x1;	/* 0x1 = Pixel */
	startSizePix = cmd++;	/* TBD #2: start/size (from save) */

	/* load shared shader instructions from the shadow. */
	*cmd++ = pm4_type3_packet(PM4_IM_LOAD, 2);
	*cmd++ = ctx->shader_shared + 0x2;	/* 0x2 = Shared */
	startSizeShared = cmd++;	/* TBD #3: start/size (from save) */
#endif

	/* create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->shader_restore, restore, cmd);

	/*
	 *  fixup SET_SHADER_BASES data
	 *
	 *  since self-modifying PM4 code is being used here, a seperate
	 *  command buffer is used for this fixup operation, to ensure the
	 *  commands are not read by the PM4 engine before the data fields
	 *  have been written.
	 */

	fixup = cmd;		/* start address */

	/* write the shader partition information to a scratch register */
	*cmd++ = pm4_type0_packet(REG_SCRATCH_REG2, 1);
	partition2 = cmd++;	/* TBD #4b: partition info (from save) */

	/* mask off unused bits, then OR with shader instruction memory size */
	*cmd++ = pm4_type3_packet(PM4_REG_RMW, 3);
	*cmd++ = REG_SCRATCH_REG2;
	/* AND off invalid bits. */
	*cmd++ = 0x0FFF0FFF;
	/* OR in instruction memory size */
	*cmd++ = (unsigned int)((SHADER_INSTRUCT_LOG2 - 5U) << 29);

	/* write the computed value to the SET_SHADER_BASES data field */
	*cmd++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
	*cmd++ = REG_SCRATCH_REG2;
	/* TBD #5: shader bases (to restore) */
	*cmd++ = gpuaddr(shaderBases, &drawctxt->gpustate);

	/* create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->shader_fixup, fixup, cmd);

	/* save shader partitioning and instructions */

	save = cmd;		/* start address */

	*cmd++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;

	/* fetch the SQ_INST_STORE_MANAGMENT register value,
	 *  store the value in the data fields of the SET_CONSTANT commands
	 *  above.
	 */
	*cmd++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
	*cmd++ = REG_SQ_INST_STORE_MANAGMENT;
	/* TBD #4a: partition info (to restore) */
	*cmd++ = gpuaddr(partition1, &drawctxt->gpustate);
	*cmd++ = pm4_type3_packet(PM4_REG_TO_MEM, 2);
	*cmd++ = REG_SQ_INST_STORE_MANAGMENT;
	/* TBD #4b: partition info (to fixup) */
	*cmd++ = gpuaddr(partition2, &drawctxt->gpustate);

#if defined(PM4_IM_STORE)

	/* store the vertex shader instructions */
	*cmd++ = pm4_type3_packet(PM4_IM_STORE, 2);
	*cmd++ = ctx->shader_vertex + 0x0;	/* 0x0 = Vertex */
	/* TBD #1: start/size (to restore) */
	*cmd++ = gpuaddr(startSizeVtx, &drawctxt->gpustate);

	/* store the pixel shader instructions */
	*cmd++ = pm4_type3_packet(PM4_IM_STORE, 2);
	*cmd++ = ctx->shader_pixel + 0x1;	/* 0x1 = Pixel */
	/* TBD #2: start/size (to restore) */
	*cmd++ = gpuaddr(startSizePix, &drawctxt->gpustate);

	/* store the shared shader instructions if vertex base is nonzero */

	*cmd++ = pm4_type3_packet(PM4_IM_STORE, 2);
	*cmd++ = ctx->shader_shared + 0x2;	/* 0x2 = Shared */
	/* TBD #3: start/size (to restore) */
	*cmd++ = gpuaddr(startSizeShared, &drawctxt->gpustate);

#endif

	*cmd++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	*cmd++ = 0;

	/* create indirect buffer command for above command sequence */
	create_ib1(drawctxt, drawctxt->shader_save, save, cmd);

	ctx->cmd = cmd;
}

/* create buffers for saving/restoring registers, constants, & GMEM */
static int
create_gpustate_shadow(struct kgsl_device *device,
		       struct adreno_context *drawctxt,
		       struct tmp_ctx *ctx)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int result;

	/* Allocate vmalloc memory to store the gpustate */
	result = kgsl_allocate(&drawctxt->gpustate,
		drawctxt->pagetable, CONTEXT_SIZE);

	if (result)
		return result;

	drawctxt->flags |= CTXT_FLAGS_STATE_SHADOW;

	/* Blank out h/w register, constant, and command buffer shadows. */
	kgsl_sharedmem_set(&drawctxt->gpustate, 0, 0, CONTEXT_SIZE);

	/* set-up command and vertex buffer pointers */
	ctx->cmd = ctx->start
	    = (unsigned int *)((char *)drawctxt->gpustate.hostptr + CMD_OFFSET);

	/* build indirect command buffers to save & restore regs/constants */
	adreno_idle(device, KGSL_TIMEOUT_DEFAULT);
	build_regrestore_cmds(adreno_dev, drawctxt, ctx);
	build_regsave_cmds(adreno_dev, drawctxt, ctx);

	build_shader_save_restore_cmds(drawctxt, ctx);

	kgsl_cache_range_op(&drawctxt->gpustate,
			    KGSL_CACHE_OP_FLUSH);

	return 0;
}

/* create buffers for saving/restoring registers, constants, & GMEM */
static int
create_gmem_shadow(struct adreno_device *adreno_dev,
		   struct adreno_context *drawctxt,
		   struct tmp_ctx *ctx)
{
	struct kgsl_device *device = &adreno_dev->dev;
	int result;

	config_gmemsize(&drawctxt->context_gmem_shadow,
			adreno_dev->gmemspace.sizebytes);
	ctx->gmem_base = adreno_dev->gmemspace.gpu_base;

	result = kgsl_allocate(&drawctxt->context_gmem_shadow.gmemshadow,
		drawctxt->pagetable, drawctxt->context_gmem_shadow.size);

	if (result)
		return result;

	/* we've allocated the shadow, when swapped out, GMEM must be saved. */
	drawctxt->flags |= CTXT_FLAGS_GMEM_SHADOW | CTXT_FLAGS_GMEM_SAVE;

	/* blank out gmem shadow. */
	kgsl_sharedmem_set(&drawctxt->context_gmem_shadow.gmemshadow, 0, 0,
			   drawctxt->context_gmem_shadow.size);

	/* build quad vertex buffer */
	build_quad_vtxbuff(drawctxt, ctx, &drawctxt->context_gmem_shadow);

	/* build TP0_CHICKEN register restore command buffer */
	ctx->cmd = build_chicken_restore_cmds(drawctxt, ctx);

	/* build indirect command buffers to save & restore gmem */
	/* Idle because we are reading PM override registers */
	adreno_idle(device, KGSL_TIMEOUT_DEFAULT);
	drawctxt->context_gmem_shadow.gmem_save_commands = ctx->cmd;
	ctx->cmd =
	    build_gmem2sys_cmds(adreno_dev, drawctxt, ctx,
				&drawctxt->context_gmem_shadow);
	drawctxt->context_gmem_shadow.gmem_restore_commands = ctx->cmd;
	ctx->cmd =
	    build_sys2gmem_cmds(adreno_dev, drawctxt, ctx,
				&drawctxt->context_gmem_shadow);

	kgsl_cache_range_op(&drawctxt->context_gmem_shadow.gmemshadow,
			    KGSL_CACHE_OP_FLUSH);

	return 0;
}

/* create a new drawing context */

int adreno_drawctxt_create(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable,
			struct kgsl_context *context, uint32_t flags)
{
	struct adreno_context *drawctxt;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct tmp_ctx ctx;
	int ret;

	drawctxt = kzalloc(sizeof(struct adreno_context), GFP_KERNEL);

	if (drawctxt == NULL)
		return -ENOMEM;

	drawctxt->pagetable = pagetable;
	drawctxt->bin_base_offset = 0;

	ret = create_gpustate_shadow(device, drawctxt, &ctx);
	if (ret)
		goto err;

	/* Save the shader instruction memory on context switching */
	drawctxt->flags |= CTXT_FLAGS_SHADER_SAVE;

	if (!(flags & KGSL_CONTEXT_NO_GMEM_ALLOC)) {
		/* create gmem shadow */
		ret = create_gmem_shadow(adreno_dev, drawctxt, &ctx);
		if (ret != 0)
			goto err;
	}

	BUG_ON(ctx.cmd - ctx.start > CMD_BUFFER_LEN);

	context->devctxt = drawctxt;
	return 0;
err:
	kgsl_sharedmem_free(&drawctxt->gpustate);
	kfree(drawctxt);
	return ret;
}

/* destroy a drawing context */

void adreno_drawctxt_destroy(struct kgsl_device *device,
			  struct kgsl_context *context)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_context *drawctxt = context->devctxt;

	if (drawctxt == NULL)
		return;

	/* deactivate context */
	if (adreno_dev->drawctxt_active == drawctxt) {
		/* no need to save GMEM or shader, the context is
		 * being destroyed.
		 */
		drawctxt->flags &= ~(CTXT_FLAGS_GMEM_SAVE |
				     CTXT_FLAGS_SHADER_SAVE |
				     CTXT_FLAGS_GMEM_SHADOW |
				     CTXT_FLAGS_STATE_SHADOW);

		adreno_drawctxt_switch(adreno_dev, NULL, 0);
	}

	adreno_idle(device, KGSL_TIMEOUT_DEFAULT);

	kgsl_sharedmem_free(&drawctxt->gpustate);
	kgsl_sharedmem_free(&drawctxt->context_gmem_shadow.gmemshadow);

	kfree(drawctxt);
	context->devctxt = NULL;
}

/* set bin base offset */
void adreno_drawctxt_set_bin_base_offset(struct kgsl_device *device,
				      struct kgsl_context *context,
				      unsigned int offset)
{
	struct adreno_context *drawctxt = context->devctxt;

	if (drawctxt)
		drawctxt->bin_base_offset = offset;
}

/* switch drawing contexts */
void
adreno_drawctxt_switch(struct adreno_device *adreno_dev,
			struct adreno_context *drawctxt,
			unsigned int flags)
{
	struct adreno_context *active_ctxt =
	  adreno_dev->drawctxt_active;
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int cmds[5];

	if (drawctxt) {
		if (flags & KGSL_CONTEXT_SAVE_GMEM)
			/* Set the flag in context so that the save is done
			* when this context is switched out. */
			drawctxt->flags |= CTXT_FLAGS_GMEM_SAVE;
		else
			/* Remove GMEM saving flag from the context */
			drawctxt->flags &= ~CTXT_FLAGS_GMEM_SAVE;
	}
	/* already current? */
	if (active_ctxt == drawctxt)
		return;

	KGSL_CTXT_INFO(device, "from %p to %p flags %d\n",
			adreno_dev->drawctxt_active, drawctxt, flags);
	/* save old context*/
	if (active_ctxt && active_ctxt->flags & CTXT_FLAGS_GPU_HANG)
		KGSL_CTXT_WARN(device,
			"Current active context has caused gpu hang\n");

	if (active_ctxt != NULL) {
		KGSL_CTXT_INFO(device,
			"active_ctxt flags %08x\n", active_ctxt->flags);
		/* save registers and constants. */
		adreno_ringbuffer_issuecmds(device, 0,
				active_ctxt->reg_save, 3);

		if (active_ctxt->flags & CTXT_FLAGS_SHADER_SAVE) {
			/* save shader partitioning and instructions. */
			adreno_ringbuffer_issuecmds(device,
					KGSL_CMD_FLAGS_PMODE,
					active_ctxt->shader_save, 3);

			/* fixup shader partitioning parameter for
			 *  SET_SHADER_BASES.
			 */
			adreno_ringbuffer_issuecmds(device, 0,
					active_ctxt->shader_fixup, 3);

			active_ctxt->flags |= CTXT_FLAGS_SHADER_RESTORE;
		}

		if (active_ctxt->flags & CTXT_FLAGS_GMEM_SAVE
			&& active_ctxt->flags & CTXT_FLAGS_GMEM_SHADOW) {
			/* save gmem.
			 * (note: changes shader. shader must already be saved.)
			 */
			adreno_ringbuffer_issuecmds(device,
				KGSL_CMD_FLAGS_PMODE,
				active_ctxt->context_gmem_shadow.gmem_save, 3);

			/* Restore TP0_CHICKEN */
			adreno_ringbuffer_issuecmds(device, 0,
				active_ctxt->chicken_restore, 3);

			active_ctxt->flags |= CTXT_FLAGS_GMEM_RESTORE;
		}
	}

	adreno_dev->drawctxt_active = drawctxt;

	/* restore new context */
	if (drawctxt != NULL) {

		KGSL_CTXT_INFO(device,
			"drawctxt flags %08x\n", drawctxt->flags);
		cmds[0] = pm4_nop_packet(1);
		cmds[1] = KGSL_CONTEXT_TO_MEM_IDENTIFIER;
		cmds[2] = pm4_type3_packet(PM4_MEM_WRITE, 2);
		cmds[3] = device->memstore.gpuaddr +
				KGSL_DEVICE_MEMSTORE_OFFSET(current_context);
		cmds[4] = (unsigned int)adreno_dev->drawctxt_active;
		adreno_ringbuffer_issuecmds(device, 0, cmds, 5);
		kgsl_mmu_setstate(device, drawctxt->pagetable);

#ifndef CONFIG_MSM_KGSL_CFF_DUMP_NO_CONTEXT_MEM_DUMP
		kgsl_cffdump_syncmem(NULL, &drawctxt->gpustate,
			drawctxt->gpustate.gpuaddr, LCC_SHADOW_SIZE +
			REG_SHADOW_SIZE + CMD_BUFFER_SIZE + TEX_SHADOW_SIZE,
			false);
#endif

		/* restore gmem.
		 *  (note: changes shader. shader must not already be restored.)
		 */
		if (drawctxt->flags & CTXT_FLAGS_GMEM_RESTORE) {
			adreno_ringbuffer_issuecmds(device,
				KGSL_CMD_FLAGS_PMODE,
				drawctxt->context_gmem_shadow.gmem_restore, 3);

			/* Restore TP0_CHICKEN */
			adreno_ringbuffer_issuecmds(device, 0,
				drawctxt->chicken_restore, 3);

			drawctxt->flags &= ~CTXT_FLAGS_GMEM_RESTORE;
		}

		/* restore registers and constants. */
		adreno_ringbuffer_issuecmds(device, 0,
					  drawctxt->reg_restore, 3);

		/* restore shader instructions & partitioning. */
		if (drawctxt->flags & CTXT_FLAGS_SHADER_RESTORE) {
			adreno_ringbuffer_issuecmds(device, 0,
					  drawctxt->shader_restore, 3);
		}

		cmds[0] = pm4_type3_packet(PM4_SET_BIN_BASE_OFFSET, 1);
		cmds[1] = drawctxt->bin_base_offset;
		if (!adreno_is_a220(adreno_dev))
			adreno_ringbuffer_issuecmds(device, 0, cmds, 2);

	} else
		kgsl_mmu_setstate(device, device->mmu.defaultpagetable);
}
