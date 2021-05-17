// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/clk/qcom.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include "adreno.h"
#include "adreno_cp_parser.h"
#include "adreno_a3xx.h"
#include "adreno_pm4types.h"
#include "adreno_snapshot.h"
#include "adreno_trace.h"

/*
 * Define registers for a3xx that contain addresses used by the
 * cp parser logic
 */
const unsigned int a3xx_cp_addr_regs[ADRENO_CP_ADDR_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_0,
				A3XX_VSC_PIPE_DATA_ADDRESS_0),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_0,
				A3XX_VSC_PIPE_DATA_LENGTH_0),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_1,
				A3XX_VSC_PIPE_DATA_ADDRESS_1),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_1,
				A3XX_VSC_PIPE_DATA_LENGTH_1),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_2,
				A3XX_VSC_PIPE_DATA_ADDRESS_2),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_2,
				A3XX_VSC_PIPE_DATA_LENGTH_2),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_3,
				A3XX_VSC_PIPE_DATA_ADDRESS_3),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_3,
				A3XX_VSC_PIPE_DATA_LENGTH_3),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_4,
				A3XX_VSC_PIPE_DATA_ADDRESS_4),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_4,
				A3XX_VSC_PIPE_DATA_LENGTH_4),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_5,
				A3XX_VSC_PIPE_DATA_ADDRESS_5),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_5,
				A3XX_VSC_PIPE_DATA_LENGTH_5),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_6,
				A3XX_VSC_PIPE_DATA_ADDRESS_6),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_6,
				A3XX_VSC_PIPE_DATA_LENGTH_6),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_7,
				A3XX_VSC_PIPE_DATA_ADDRESS_7),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_7,
				A3XX_VSC_PIPE_DATA_LENGTH_7),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_0,
				A3XX_VFD_FETCH_INSTR_1_0),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_1,
				A3XX_VFD_FETCH_INSTR_1_1),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_2,
				A3XX_VFD_FETCH_INSTR_1_2),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_3,
				A3XX_VFD_FETCH_INSTR_1_3),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_4,
				A3XX_VFD_FETCH_INSTR_1_4),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_5,
				A3XX_VFD_FETCH_INSTR_1_5),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_6,
				A3XX_VFD_FETCH_INSTR_1_6),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_7,
				A3XX_VFD_FETCH_INSTR_1_7),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_8,
				A3XX_VFD_FETCH_INSTR_1_8),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_9,
				A3XX_VFD_FETCH_INSTR_1_9),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_10,
				A3XX_VFD_FETCH_INSTR_1_A),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_11,
				A3XX_VFD_FETCH_INSTR_1_B),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_12,
				A3XX_VFD_FETCH_INSTR_1_C),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_13,
				A3XX_VFD_FETCH_INSTR_1_D),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_14,
				A3XX_VFD_FETCH_INSTR_1_E),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_15,
				A3XX_VFD_FETCH_INSTR_1_F),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_SIZE_ADDRESS,
				A3XX_VSC_SIZE_ADDRESS),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_SP_VS_PVT_MEM_ADDR,
				A3XX_SP_VS_PVT_MEM_ADDR_REG),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_SP_FS_PVT_MEM_ADDR,
				A3XX_SP_FS_PVT_MEM_ADDR_REG),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_SP_VS_OBJ_START_REG,
				A3XX_SP_VS_OBJ_START_REG),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_SP_FS_OBJ_START_REG,
				A3XX_SP_FS_OBJ_START_REG),
};

static const unsigned int _a3xx_pwron_fixup_fs_instructions[] = {
	0x00000000, 0x302CC300, 0x00000000, 0x302CC304,
	0x00000000, 0x302CC308, 0x00000000, 0x302CC30C,
	0x00000000, 0x302CC310, 0x00000000, 0x302CC314,
	0x00000000, 0x302CC318, 0x00000000, 0x302CC31C,
	0x00000000, 0x302CC320, 0x00000000, 0x302CC324,
	0x00000000, 0x302CC328, 0x00000000, 0x302CC32C,
	0x00000000, 0x302CC330, 0x00000000, 0x302CC334,
	0x00000000, 0x302CC338, 0x00000000, 0x302CC33C,
	0x00000000, 0x00000400, 0x00020000, 0x63808003,
	0x00060004, 0x63828007, 0x000A0008, 0x6384800B,
	0x000E000C, 0x6386800F, 0x00120010, 0x63888013,
	0x00160014, 0x638A8017, 0x001A0018, 0x638C801B,
	0x001E001C, 0x638E801F, 0x00220020, 0x63908023,
	0x00260024, 0x63928027, 0x002A0028, 0x6394802B,
	0x002E002C, 0x6396802F, 0x00320030, 0x63988033,
	0x00360034, 0x639A8037, 0x003A0038, 0x639C803B,
	0x003E003C, 0x639E803F, 0x00000000, 0x00000400,
	0x00000003, 0x80D60003, 0x00000007, 0x80D60007,
	0x0000000B, 0x80D6000B, 0x0000000F, 0x80D6000F,
	0x00000013, 0x80D60013, 0x00000017, 0x80D60017,
	0x0000001B, 0x80D6001B, 0x0000001F, 0x80D6001F,
	0x00000023, 0x80D60023, 0x00000027, 0x80D60027,
	0x0000002B, 0x80D6002B, 0x0000002F, 0x80D6002F,
	0x00000033, 0x80D60033, 0x00000037, 0x80D60037,
	0x0000003B, 0x80D6003B, 0x0000003F, 0x80D6003F,
	0x00000000, 0x03000000, 0x00000000, 0x00000000,
};

/**
 * _a3xx_pwron_fixup() - Initialize a special command buffer to run a
 * post-power collapse shader workaround
 * @adreno_dev: Pointer to a adreno_device struct
 *
 * Some targets require a special workaround shader to be executed after
 * power-collapse.  Construct the IB once at init time and keep it
 * handy
 *
 * Returns: 0 on success or negative on error
 */
static int _a3xx_pwron_fixup(struct adreno_device *adreno_dev)
{
	unsigned int *cmds;
	int count = ARRAY_SIZE(_a3xx_pwron_fixup_fs_instructions);

	/* Return if the fixup is already in place */
	if (test_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv))
		return 0;

	adreno_dev->pwron_fixup = kgsl_allocate_global(KGSL_DEVICE(adreno_dev),
		PAGE_SIZE, 0, KGSL_MEMFLAGS_GPUREADONLY, 0, "pwron_fixup");

	if (IS_ERR(adreno_dev->pwron_fixup))
		return PTR_ERR(adreno_dev->pwron_fixup);

	cmds = adreno_dev->pwron_fixup->hostptr;

	*cmds++ = cp_type0_packet(A3XX_UCHE_CACHE_INVALIDATE0_REG, 2);
	*cmds++ = 0x00000000;
	*cmds++ = 0x90000000;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmds++ = A3XX_RBBM_CLOCK_CTL;
	*cmds++ = 0xFFFCFFFF;
	*cmds++ = 0x00010000;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CONTROL_0_REG, 1);
	*cmds++ = 0x1E000150;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_HLSQ_CONTROL_0_REG);
	*cmds++ = 0x1E000150;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CONTROL_0_REG, 1);
	*cmds++ = 0x1E000150;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CONTROL_1_REG, 1);
	*cmds++ = 0x00000040;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CONTROL_2_REG, 1);
	*cmds++ = 0x80000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CONTROL_3_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_VS_CONTROL_REG, 1);
	*cmds++ = 0x00000001;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_FS_CONTROL_REG, 1);
	*cmds++ = 0x0D001002;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CONST_VSPRESV_RANGE_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CONST_FSPRESV_RANGE_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_NDRANGE_0_REG, 1);
	*cmds++ = 0x00401101;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_NDRANGE_1_REG, 1);
	*cmds++ = 0x00000400;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_NDRANGE_2_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_NDRANGE_3_REG, 1);
	*cmds++ = 0x00000001;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_NDRANGE_4_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_NDRANGE_5_REG, 1);
	*cmds++ = 0x00000001;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_NDRANGE_6_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_CONTROL_0_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_CONTROL_1_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_KERNEL_CONST_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_KERNEL_GROUP_X_REG, 1);
	*cmds++ = 0x00000010;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_KERNEL_GROUP_Y_REG, 1);
	*cmds++ = 0x00000001;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_KERNEL_GROUP_Z_REG, 1);
	*cmds++ = 0x00000001;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_WG_OFFSET_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_SP_CTRL_REG, 1);
	*cmds++ = 0x00040000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_CTRL_REG0, 1);
	*cmds++ = 0x0000000A;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_CTRL_REG1, 1);
	*cmds++ = 0x00000001;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_PARAM_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OUT_REG_0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OUT_REG_1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OUT_REG_2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OUT_REG_3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OUT_REG_4, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OUT_REG_5, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OUT_REG_6, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OUT_REG_7, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_VPC_DST_REG_0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_VPC_DST_REG_1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_VPC_DST_REG_2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_VPC_DST_REG_3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OBJ_OFFSET_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_OBJ_START_REG, 1);
	*cmds++ = 0x00000004;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_PVT_MEM_PARAM_REG, 1);
	*cmds++ = 0x04008001;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_PVT_MEM_ADDR_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_PVT_MEM_SIZE_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_VS_LENGTH_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_CTRL_REG0, 1);
	*cmds++ = 0x0DB0400A;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_CTRL_REG1, 1);
	*cmds++ = 0x00300402;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_OBJ_OFFSET_REG, 1);
	*cmds++ = 0x00010000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_OBJ_START_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_PVT_MEM_PARAM_REG, 1);
	*cmds++ = 0x04008001;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_PVT_MEM_ADDR_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_PVT_MEM_SIZE_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_FLAT_SHAD_MODE_REG_0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_FLAT_SHAD_MODE_REG_1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_OUTPUT_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_MRT_REG_0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_MRT_REG_1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_MRT_REG_2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_MRT_REG_3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_IMAGE_OUTPUT_REG_0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_IMAGE_OUTPUT_REG_1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_IMAGE_OUTPUT_REG_2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_IMAGE_OUTPUT_REG_3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_SP_FS_LENGTH_REG, 1);
	*cmds++ = 0x0000000D;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_CLIP_CNTL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_GB_CLIP_ADJ, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_VPORT_XOFFSET, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_VPORT_XSCALE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_VPORT_YOFFSET, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_VPORT_YSCALE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_VPORT_ZOFFSET, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_VPORT_ZSCALE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_X0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Y0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Z0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_W0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_X1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Y1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Z1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_W1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_X2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Y2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Z2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_W2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_X3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Y3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Z3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_W3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_X4, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Y4, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Z4, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_W4, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_X5, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Y5, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_Z5, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_CL_USER_PLANE_W5, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SU_POINT_MINMAX, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SU_POINT_SIZE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SU_POLY_OFFSET_OFFSET, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SU_POLY_OFFSET_SCALE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SU_MODE_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SC_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SC_SCREEN_SCISSOR_TL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SC_SCREEN_SCISSOR_BR, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SC_WINDOW_SCISSOR_BR, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_SC_WINDOW_SCISSOR_TL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_TSE_DEBUG_ECO, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_PERFCOUNTER0_SELECT, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_PERFCOUNTER1_SELECT, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_PERFCOUNTER2_SELECT, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_GRAS_PERFCOUNTER3_SELECT, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MODE_CONTROL, 1);
	*cmds++ = 0x00008000;
	*cmds++ = cp_type0_packet(A3XX_RB_RENDER_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MSAA_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_ALPHA_REFERENCE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_CONTROL0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_CONTROL1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_CONTROL2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_CONTROL3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BUF_INFO0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BUF_INFO1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BUF_INFO2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BUF_INFO3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BUF_BASE0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BUF_BASE1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BUF_BASE2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BUF_BASE3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BLEND_CONTROL0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BLEND_CONTROL1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BLEND_CONTROL2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_MRT_BLEND_CONTROL3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_BLEND_RED, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_BLEND_GREEN, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_BLEND_BLUE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_BLEND_ALPHA, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_CLEAR_COLOR_DW0, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_CLEAR_COLOR_DW1, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_CLEAR_COLOR_DW2, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_CLEAR_COLOR_DW3, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_COPY_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_COPY_DEST_BASE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_COPY_DEST_PITCH, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_COPY_DEST_INFO, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_DEPTH_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_DEPTH_CLEAR, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_DEPTH_BUF_INFO, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_DEPTH_BUF_PITCH, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_STENCIL_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_STENCIL_CLEAR, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_STENCIL_BUF_INFO, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_STENCIL_BUF_PITCH, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_STENCIL_REF_MASK, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_STENCIL_REF_MASK_BF, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_LRZ_VSC_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_WINDOW_OFFSET, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_SAMPLE_COUNT_CONTROL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_SAMPLE_COUNT_ADDR, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_Z_CLAMP_MIN, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_Z_CLAMP_MAX, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_GMEM_BASE_ADDR, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_DEBUG_ECO_CONTROLS_ADDR, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_PERFCOUNTER0_SELECT, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_PERFCOUNTER1_SELECT, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_RB_FRAME_BUFFER_DIMENSION, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 4);
	*cmds++ = (1 << CP_LOADSTATE_DSTOFFSET_SHIFT) |
		(0 << CP_LOADSTATE_STATESRC_SHIFT) |
		(6 << CP_LOADSTATE_STATEBLOCKID_SHIFT) |
		(1 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (1 << CP_LOADSTATE_STATETYPE_SHIFT) |
		(0 << CP_LOADSTATE_EXTSRCADDR_SHIFT);
	*cmds++ = 0x00400000;
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 4);
	*cmds++ = (2 << CP_LOADSTATE_DSTOFFSET_SHIFT) |
		(6 << CP_LOADSTATE_STATEBLOCKID_SHIFT) |
		(1 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (1 << CP_LOADSTATE_STATETYPE_SHIFT);
	*cmds++ = 0x00400220;
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 4);
	*cmds++ = (6 << CP_LOADSTATE_STATEBLOCKID_SHIFT) |
		(1 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = (1 << CP_LOADSTATE_STATETYPE_SHIFT);
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_LOAD_STATE, 2 + count);
	*cmds++ = (6 << CP_LOADSTATE_STATEBLOCKID_SHIFT) |
		(13 << CP_LOADSTATE_NUMOFUNITS_SHIFT);
	*cmds++ = 0x00000000;

	memcpy(cmds, _a3xx_pwron_fixup_fs_instructions, count << 2);

	cmds += count;

	*cmds++ = cp_type3_packet(CP_EXEC_CL, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CL_CONTROL_0_REG, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type0_packet(A3XX_HLSQ_CONTROL_0_REG, 1);
	*cmds++ = 0x1E000150;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
	*cmds++ = CP_REG(A3XX_HLSQ_CONTROL_0_REG);
	*cmds++ = 0x1E000050;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
	*cmds++ = A3XX_RBBM_CLOCK_CTL;
	*cmds++ = 0xFFFCFFFF;
	*cmds++ = 0x00000000;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;

	/*
	 * Remember the number of dwords in the command buffer for when we
	 * program the indirect buffer call in the ringbuffer
	 */
	adreno_dev->pwron_fixup_dwords =
		(cmds - (unsigned int *) adreno_dev->pwron_fixup->hostptr);

	/* Mark the flag in ->priv to show that we have the fix */
	set_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv);
	return 0;
}

static int a3xx_probe(struct platform_device *pdev,
		u32 chipid, const struct adreno_gpu_core *gpucore)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	int ret;

	adreno_dev = (struct adreno_device *)
		of_device_get_match_data(&pdev->dev);

	memset(adreno_dev, 0, sizeof(*adreno_dev));

	adreno_dev->gpucore = gpucore;
	adreno_dev->chipid = chipid;

	adreno_reg_offset_init(gpucore->gpudev->reg_offsets);


	device = KGSL_DEVICE(adreno_dev);

	timer_setup(&device->idle_timer, kgsl_timer, 0);

	INIT_WORK(&device->idle_check_ws, kgsl_idle_check);

	ret = adreno_device_probe(pdev, adreno_dev);
	if (ret)
		return ret;

	return adreno_dispatcher_init(adreno_dev);
}

static int a3xx_send_me_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, 18);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	*cmds++ = cp_type3_packet(CP_ME_INIT, 17);

	*cmds++ = 0x000003f7;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000080;
	*cmds++ = 0x00000100;
	*cmds++ = 0x00000180;
	*cmds++ = 0x00006600;
	*cmds++ = 0x00000150;
	*cmds++ = 0x0000014e;
	*cmds++ = 0x00000154;
	*cmds++ = 0x00000001;
	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;

	/* Enable protected mode registers for A3XX */
	*cmds++ = 0x20000000;

	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;

	/* Submit the command to the ringbuffer */
	kgsl_pwrscale_busy(device);
	kgsl_regwrite(device, A3XX_CP_RB_WPTR, rb->_wptr);
	rb->wptr = rb->_wptr;

	ret = adreno_spin_idle(adreno_dev, 2000);
	if (ret) {
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

		dev_err(device->dev, "CP initialization failed to idle\n");
		kgsl_device_snapshot(device, NULL, false);
	}

	return ret;
}

static void a3xx_microcode_load(struct adreno_device *adreno_dev);

static int a3xx_rb_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);

	memset(rb->buffer_desc->hostptr, 0xaa, KGSL_RB_SIZE);
	rb->wptr = 0;
	rb->_wptr = 0;
	rb->wptr_preempt_end = ~0;

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2).
	 * Also disable the host RPTR shadow register as it might be unreliable
	 * in certain circumstances.
	 */

	kgsl_regwrite(device, A3XX_CP_RB_CNTL,
		(ilog2(KGSL_RB_DWORDS >> 1) & 0x3F) |
		(1 << 27));

	kgsl_regwrite(device, A3XX_CP_RB_BASE, rb->buffer_desc->gpuaddr);

	a3xx_microcode_load(adreno_dev);

	/* clear ME_HALT to start micro engine */
	kgsl_regwrite(device, A3XX_CP_ME_CNTL, 0);

	return a3xx_send_me_init(adreno_dev, rb);
}

/*
 * a3xx soft fault detection
 *
 * a3xx targets do not have hardware fault detection so we need to do it the old
 * fashioned way by periodically reading a set of registers and counters and
 * checking that they are advancing. There are 6 registers and four 64 bit
 * counters that we keep an eye on.
 */

#define A3XX_SOFT_FAULT_DETECT_REGS 6
#define A3XX_SOFT_FAULT_DETECT_COUNTERS 4
#define A3XX_SOFT_FAULT_DETECT_COUNT \
	(A3XX_SOFT_FAULT_DETECT_REGS + (A3XX_SOFT_FAULT_DETECT_COUNTERS * 2))

static bool a3xx_soft_fault_detect_isidle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 reg;

	if (kgsl_state_is_awake(device)) {
		if (!adreno_rb_empty(adreno_dev->cur_rb))
			return false;

		/* only check rbbm status to determine if GPU is idle */
		kgsl_regread(device, A3XX_RBBM_STATUS, &reg);

		if (reg & 0x7ffffffe)
			return false;
	}

	memset(adreno_dev->soft_ft_vals, 0, A3XX_SOFT_FAULT_DETECT_COUNT << 2);
	return true;
}

/* Read the fault detect registers and compare them to the stored version */
static int a3xx_soft_fault_detect_read_compare(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	int i, ret = 0;
	unsigned int ts;

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return 1;

	/* Check to see if the device is idle - if so report no hang */
	if (a3xx_soft_fault_detect_isidle(adreno_dev))
		ret = 1;

	for (i = 0; i < A3XX_SOFT_FAULT_DETECT_COUNT; i++) {
		unsigned int val;

		if (!adreno_dev->soft_ft_regs[i])
			continue;

		kgsl_regread(device, adreno_dev->soft_ft_regs[i], &val);
		if (val != adreno_dev->soft_ft_vals[i])
			ret = 1;
		adreno_dev->soft_ft_vals[i] = val;
	}

	if (!adreno_rb_readtimestamp(adreno_dev, adreno_dev->cur_rb,
				KGSL_TIMESTAMP_RETIRED, &ts)) {
		if (ts != rb->fault_detect_ts)
			ret = 1;

		rb->fault_detect_ts = ts;
	}

	return ret;
}

/*
 * This is called on a regular basis while cmdobjs are inflight. Fault
 * detection registers are read and compared to the existing values - if they
 * changed then the GPU is still running. If they are the same between
 * subsequent calls then the GPU may have faulted
 */
static void a3xx_soft_fault_timer(struct timer_list *t)
{
	struct adreno_dispatcher *dispatcher = from_timer(dispatcher,
							t, fault_timer);
	struct adreno_device *adreno_dev = container_of(dispatcher,
					struct adreno_device, dispatcher);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Leave if the user decided to turn off fast hang detection */
	if (!adreno_soft_fault_detect(adreno_dev))
		return;

	if (adreno_gpu_fault(adreno_dev)) {
		adreno_dispatcher_schedule(device);
		return;
	}

	/*
	 * Read the fault registers - if it returns 0 then they haven't changed
	 * so mark the dispatcher as faulted and schedule the work loop.
	 */

	if (!a3xx_soft_fault_detect_read_compare(adreno_dev))
		adreno_dispatcher_fault(adreno_dev, ADRENO_SOFT_FAULT);
	else if (dispatcher->inflight > 0)
		adreno_dispatcher_start_fault_timer(adreno_dev);
}

/*
 * Start fault detection. The counters are only assigned while fault detection
 * is running so that they can be used for other purposes if fault detection is
 * disabled
 */
static void a3xx_soft_fault_detect_start(struct adreno_device *adreno_dev)
{
	u32 *regs = &adreno_dev->soft_ft_regs[A3XX_SOFT_FAULT_DETECT_COUNTERS];
	int ret = 0;

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return;

	if (adreno_dev->fast_hang_detect == 1)
		return;

	ret |= adreno_perfcounter_kernel_get(adreno_dev,
		KGSL_PERFCOUNTER_GROUP_SP, SP_ALU_ACTIVE_CYCLES,
		&regs[0], &regs[1]);

	ret |= adreno_perfcounter_kernel_get(adreno_dev,
		KGSL_PERFCOUNTER_GROUP_SP, SP0_ICL1_MISSES,
		&regs[2], &regs[3]);

	ret |= adreno_perfcounter_kernel_get(adreno_dev,
		KGSL_PERFCOUNTER_GROUP_SP, SP_FS_CFLOW_INSTRUCTIONS,
		&regs[4], &regs[5]);

	ret |= adreno_perfcounter_kernel_get(adreno_dev,
		KGSL_PERFCOUNTER_GROUP_TSE, TSE_INPUT_PRIM_NUM,
		&regs[6], &regs[7]);

	WARN(ret, "Unable to allocate one or more fault detect counters\n");
	adreno_dev->fast_hang_detect = 1;
}

/* Helper function to put back a counter */
static void put_counter(struct adreno_device *adreno_dev,
		int group, int countable, u32 *lo, u32 *hi)
{
	adreno_perfcounter_put(adreno_dev, group, countable,
		PERFCOUNTER_FLAG_KERNEL);

	*lo = 0;
	*hi = 0;
}

/* Stop fault detection and return the counters */
static void a3xx_soft_fault_detect_stop(struct adreno_device *adreno_dev)
{
	u32 *regs = &adreno_dev->soft_ft_regs[A3XX_SOFT_FAULT_DETECT_COUNTERS];

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return;

	if (!adreno_dev->fast_hang_detect)
		return;

	put_counter(adreno_dev, KGSL_PERFCOUNTER_GROUP_SP, SP_ALU_ACTIVE_CYCLES,
			&regs[0], &regs[1]);

	put_counter(adreno_dev, KGSL_PERFCOUNTER_GROUP_SP, SP0_ICL1_MISSES,
			&regs[2], &regs[3]);

	put_counter(adreno_dev, KGSL_PERFCOUNTER_GROUP_SP,
			SP_FS_CFLOW_INSTRUCTIONS, &regs[4], &regs[5]);

	put_counter(adreno_dev, KGSL_PERFCOUNTER_GROUP_TSE, TSE_INPUT_PRIM_NUM,
			&regs[6], &regs[7]);

	adreno_dev->fast_hang_detect = 0;
}

/* Initialize the registers and set up the data structures */
static void a3xx_soft_fault_detect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SOFT_FAULT_DETECT))
		return;

	/* Disable the fast hang detect bit until we know its a go */
	adreno_dev->fast_hang_detect = 0;

	adreno_dev->soft_ft_regs = devm_kcalloc(&device->pdev->dev,
		A3XX_SOFT_FAULT_DETECT_COUNT, sizeof(u32), GFP_KERNEL);

	adreno_dev->soft_ft_vals = devm_kcalloc(&device->pdev->dev,
		A3XX_SOFT_FAULT_DETECT_COUNT, sizeof(u32), GFP_KERNEL);

	if (!adreno_dev->soft_ft_regs || !adreno_dev->soft_ft_vals)
		return;

	adreno_dev->soft_ft_count = A3XX_SOFT_FAULT_DETECT_COUNT;

	adreno_dev->soft_ft_regs[0] = A3XX_RBBM_STATUS;
	adreno_dev->soft_ft_regs[1] = A3XX_CP_RB_RPTR;
	adreno_dev->soft_ft_regs[2] = A3XX_CP_IB1_BASE;
	adreno_dev->soft_ft_regs[3] = A3XX_CP_IB1_BUFSZ;
	adreno_dev->soft_ft_regs[4] = A3XX_CP_IB2_BASE;
	adreno_dev->soft_ft_regs[5] = A3XX_CP_IB2_BUFSZ;

	set_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv);

	a3xx_soft_fault_detect_start(adreno_dev);
}

static void a3xx_remove(struct adreno_device *adreno_dev)
{
	a3xx_soft_fault_detect_stop(adreno_dev);
}

static int a3xx_microcode_read(struct adreno_device *adreno_dev);

/*
 * a3xx_init() - Initialize gpu specific data
 * @adreno_dev: Pointer to adreno device
 */
static int a3xx_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU(device);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int ret;

	/*
	 * Set up the a3xx only soft fault timer before heading into the generic
	 * dispatcher setup
	 */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_SOFT_FAULT_DETECT))
		timer_setup(&dispatcher->fault_timer, a3xx_soft_fault_timer, 0);

	ret = a3xx_ringbuffer_init(adreno_dev);
	if (ret)
		return ret;

	ret = a3xx_microcode_read(adreno_dev);
	if (ret)
		return ret;

	_a3xx_pwron_fixup(adreno_dev);

	ret = adreno_allocate_global(device, &iommu->setstate, PAGE_SIZE,
			0, KGSL_MEMFLAGS_GPUREADONLY, 0, "setstate");

	if (!ret)
		kgsl_sharedmem_writel(iommu->setstate,
			KGSL_IOMMU_SETSTATE_NOP_OFFSET,
			cp_type3_packet(CP_NOP, 1));

	kgsl_mmu_set_feature(device, KGSL_MMU_NEED_GUARD_PAGE);

	/* Put the hardware in a responsive state to set up fault detection*/
	ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
	if (ret)
		return ret;

	a3xx_soft_fault_detect_init(adreno_dev);

	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
	return 0;
}

/*
 * a3xx_err_callback() - Call back for a3xx error interrupts
 * @adreno_dev: Pointer to device
 * @bit: Interrupt bit
 */
static void a3xx_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;

	switch (bit) {
	case A3XX_INT_RBBM_AHB_ERROR: {
		kgsl_regread(device, A3XX_RBBM_AHB_ERROR_STATUS, &reg);

		/*
		 * Return the word address of the erroring register so that it
		 * matches the register specification
		 */
		dev_crit_ratelimited(device->dev,
					"RBBM | AHB bus error | %s | addr=%x | ports=%x:%x\n",
					reg & (1 << 28) ? "WRITE" : "READ",
					(reg & 0xFFFFF) >> 2,
					(reg >> 20) & 0x3,
					(reg >> 24) & 0xF);

		/* Clear the error */
		kgsl_regwrite(device, A3XX_RBBM_AHB_CMD, (1 << 3));
		break;
	}
	case A3XX_INT_RBBM_ATB_BUS_OVERFLOW:
		dev_crit_ratelimited(device->dev,
					"RBBM: ATB bus oveflow\n");
		break;
	case A3XX_INT_CP_T0_PACKET_IN_IB:
		dev_crit_ratelimited(device->dev,
					"ringbuffer TO packet in IB interrupt\n");
		break;
	case A3XX_INT_CP_OPCODE_ERROR:
		dev_crit_ratelimited(device->dev,
					"ringbuffer opcode error interrupt\n");
		break;
	case A3XX_INT_CP_RESERVED_BIT_ERROR:
		dev_crit_ratelimited(device->dev,
					"ringbuffer reserved bit error interrupt\n");
		break;
	case A3XX_INT_CP_HW_FAULT:
		kgsl_regread(device, A3XX_CP_HW_FAULT, &reg);
		dev_crit_ratelimited(device->dev,
					"CP | Ringbuffer HW fault | status=%x\n",
					reg);
		break;
	case A3XX_INT_CP_REG_PROTECT_FAULT:
		kgsl_regread(device, A3XX_CP_PROTECT_STATUS, &reg);
		dev_crit_ratelimited(device->dev,
					"CP | Protected mode error| %s | addr=%x\n",
					reg & (1 << 24) ? "WRITE" : "READ",
					(reg & 0xFFFFF) >> 2);
		break;
	case A3XX_INT_CP_AHB_ERROR_HALT:
		dev_crit_ratelimited(device->dev,
					"ringbuffer AHB error interrupt\n");
		break;
	case A3XX_INT_UCHE_OOB_ACCESS:
		dev_crit_ratelimited(device->dev,
					"UCHE: Out of bounds access\n");
		break;
	default:
		dev_crit_ratelimited(device->dev, "Unknown interrupt\n");
	}
}

#define A3XX_INT_MASK \
	((1 << A3XX_INT_RBBM_AHB_ERROR) |        \
	 (1 << A3XX_INT_RBBM_ATB_BUS_OVERFLOW) | \
	 (1 << A3XX_INT_CP_T0_PACKET_IN_IB) |    \
	 (1 << A3XX_INT_CP_OPCODE_ERROR) |       \
	 (1 << A3XX_INT_CP_RESERVED_BIT_ERROR) | \
	 (1 << A3XX_INT_CP_HW_FAULT) |           \
	 (1 << A3XX_INT_CP_IB1_INT) |            \
	 (1 << A3XX_INT_CP_IB2_INT) |            \
	 (1 << A3XX_INT_CP_RB_INT) |             \
	 (1 << A3XX_INT_CACHE_FLUSH_TS) |	 \
	 (1 << A3XX_INT_CP_REG_PROTECT_FAULT) |  \
	 (1 << A3XX_INT_CP_AHB_ERROR_HALT) |     \
	 (1 << A3XX_INT_UCHE_OOB_ACCESS))

static const struct adreno_irq_funcs a3xx_irq_funcs[32] = {
	ADRENO_IRQ_CALLBACK(NULL),                    /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),  /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(NULL),  /* 2 - RBBM_REG_TIMEOUT */
	ADRENO_IRQ_CALLBACK(NULL),  /* 3 - RBBM_ME_MS_TIMEOUT */
	ADRENO_IRQ_CALLBACK(NULL),  /* 4 - RBBM_PFP_MS_TIMEOUT */
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),  /* 5 - RBBM_ATB_BUS_OVERFLOW */
	ADRENO_IRQ_CALLBACK(NULL),  /* 6 - RBBM_VFD_ERROR */
	ADRENO_IRQ_CALLBACK(NULL),	/* 7 - CP_SW */
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),  /* 8 - CP_T0_PACKET_IN_IB */
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),  /* 9 - CP_OPCODE_ERROR */
	/* 10 - CP_RESERVED_BIT_ERROR */
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),  /* 11 - CP_HW_FAULT */
	ADRENO_IRQ_CALLBACK(NULL),	             /* 12 - CP_DMA */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback),   /* 13 - CP_IB2_INT */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback),   /* 14 - CP_IB1_INT */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback),   /* 15 - CP_RB_INT */
	/* 16 - CP_REG_PROTECT_FAULT */
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),
	ADRENO_IRQ_CALLBACK(NULL),	       /* 17 - CP_RB_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL),	       /* 18 - CP_VS_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL),	       /* 19 - CP_PS_DONE_TS */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 20 - CP_CACHE_FLUSH_TS */
	/* 21 - CP_AHB_ERROR_FAULT */
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),
	ADRENO_IRQ_CALLBACK(NULL),	       /* 22 - Unused */
	ADRENO_IRQ_CALLBACK(NULL),	       /* 23 - Unused */
	/* 24 - MISC_HANG_DETECT */
	ADRENO_IRQ_CALLBACK(adreno_hang_int_callback),
	ADRENO_IRQ_CALLBACK(a3xx_err_callback),  /* 25 - UCHE_OOB_ACCESS */
};

static struct {
	u32 reg;
	u32 base;
	u32 count;
} a3xx_protected_blocks[] = {
	/* RBBM */
	{ A3XX_CP_PROTECT_REG_0,      0x0018, 0 },
	{ A3XX_CP_PROTECT_REG_0 + 1,  0x0020, 2 },
	{ A3XX_CP_PROTECT_REG_0 + 2,  0x0033, 0 },
	{ A3XX_CP_PROTECT_REG_0 + 3,  0x0042, 0 },
	{ A3XX_CP_PROTECT_REG_0 + 4,  0x0050, 4 },
	{ A3XX_CP_PROTECT_REG_0 + 5,  0x0063, 0 },
	{ A3XX_CP_PROTECT_REG_0 + 6,  0x0100, 4 },
	/* CP */
	{ A3XX_CP_PROTECT_REG_0 + 7,  0x01c0, 5 },
	{ A3XX_CP_PROTECT_REG_0 + 8,  0x01ec, 1 },
	{ A3XX_CP_PROTECT_REG_0 + 9,  0x01f6, 1 },
	{ A3XX_CP_PROTECT_REG_0 + 10, 0x01f8, 2 },
	{ A3XX_CP_PROTECT_REG_0 + 11, 0x045e, 2 },
	{ A3XX_CP_PROTECT_REG_0 + 12, 0x0460, 4 },
	/* RB */
	{ A3XX_CP_PROTECT_REG_0 + 13, 0x0cc0, 0 },
	/* VBIF */
	{ A3XX_CP_PROTECT_REG_0 + 14, 0x3000, 6 },
	/* SMMU */
	{ A3XX_CP_PROTECT_REG_0 + 15, 0xa000, 12 },
	/* There are no remaining protected mode registers for a3xx */
};

static void a3xx_protect_init(struct kgsl_device *device)
{
	int i;

	kgsl_regwrite(device, A3XX_CP_PROTECT_CTRL, 0x00000007);

	for (i = 0; i < ARRAY_SIZE(a3xx_protected_blocks); i++) {
		u32 val = 0x60000000 |
			(a3xx_protected_blocks[i].count << 24) |
			(a3xx_protected_blocks[i].base << 2);

		kgsl_regwrite(device, a3xx_protected_blocks[i].reg, val);
	}
}

static int a3xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a3xx_core *a3xx_core = to_a3xx_core(adreno_dev);
	int ret;

	ret = kgsl_mmu_start(device);
	if (ret)
		return ret;

	adreno_get_bus_counters(adreno_dev);
	adreno_perfcounter_restore(adreno_dev);

	if (adreno_dev->soft_ft_regs)
		memset(adreno_dev->soft_ft_regs, 0,
			adreno_dev->soft_ft_count << 2);

	adreno_dev->irq_mask = A3XX_INT_MASK;

	/* Set up VBIF registers from the GPU core definition */
	kgsl_regmap_multi_write(&device->regmap, a3xx_core->vbif,
		a3xx_core->vbif_count);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, A3XX_RBBM_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/* Tune the hystersis counters for SP and CP idle detection */
	kgsl_regwrite(device, A3XX_RBBM_SP_HYST_CNT, 0x10);
	kgsl_regwrite(device, A3XX_RBBM_WAIT_IDLE_CLOCKS_CTL, 0x10);

	/*
	 * Enable the RBBM error reporting bits.  This lets us get
	 * useful information on failure
	 */

	kgsl_regwrite(device, A3XX_RBBM_AHB_CTL0, 0x00000001);

	/* Enable AHB error reporting */
	kgsl_regwrite(device, A3XX_RBBM_AHB_CTL1, 0xA6FFFFFF);

	/* Turn on the power counters */
	kgsl_regwrite(device, A3XX_RBBM_RBBM_CTL, 0x00030000);

	/*
	 * Turn on hang detection - this spews a lot of useful information
	 * into the RBBM registers on a hang
	 */
	kgsl_regwrite(device, A3XX_RBBM_INTERFACE_HANG_INT_CTL,
		(1 << 16) | 0xFFF);

	/* Enable 64-byte cacheline size. HW Default is 32-byte (0x000000E0). */
	kgsl_regwrite(device, A3XX_UCHE_CACHE_MODE_CONTROL_REG, 0x00000001);

	/* Enable VFD to access most of the UCHE (7 ways out of 8) */
	kgsl_regwrite(device, A3XX_UCHE_CACHE_WAYS_VFD, 0x07);

	/* Enable Clock gating */
	kgsl_regwrite(device, A3XX_RBBM_CLOCK_CTL, A3XX_RBBM_CLOCK_CTL_DEFAULT);

	/* Turn on protection */
	a3xx_protect_init(device);

	/* Turn on performance counters */
	kgsl_regwrite(device, A3XX_RBBM_PERFCTR_CTL, 0x01);

	kgsl_regwrite(device, A3XX_CP_DEBUG, A3XX_CP_DEBUG_DEFAULT);

	/* CP ROQ queue sizes (bytes) - RB:16, ST:16, IB1:32, IB2:64 */
	kgsl_regwrite(device, A3XX_CP_QUEUE_THRESHOLDS, 0x000E0602);
	return 0;
}

#ifdef CONFIG_QCOM_KGSL_CORESIGHT
static struct adreno_coresight_register a3xx_coresight_registers[] = {
	{ A3XX_RBBM_DEBUG_BUS_CTL, 0x0001093F },
	{ A3XX_RBBM_EXT_TRACE_STOP_CNT, 0x00017fff },
	{ A3XX_RBBM_EXT_TRACE_START_CNT, 0x0001000f },
	{ A3XX_RBBM_EXT_TRACE_PERIOD_CNT, 0x0001ffff },
	{ A3XX_RBBM_EXT_TRACE_CMD, 0x00000001 },
	{ A3XX_RBBM_EXT_TRACE_BUS_CTL, 0x89100010 },
	{ A3XX_RBBM_DEBUG_BUS_STB_CTL0, 0x00000000 },
	{ A3XX_RBBM_DEBUG_BUS_STB_CTL1, 0xFFFFFFFE },
	{ A3XX_RBBM_INT_TRACE_BUS_CTL, 0x00201111 },
};

static ADRENO_CORESIGHT_ATTR(config_debug_bus,
	&a3xx_coresight_registers[0]);
static ADRENO_CORESIGHT_ATTR(config_trace_stop_cnt,
	&a3xx_coresight_registers[1]);
static ADRENO_CORESIGHT_ATTR(config_trace_start_cnt,
	&a3xx_coresight_registers[2]);
static ADRENO_CORESIGHT_ATTR(config_trace_period_cnt,
	&a3xx_coresight_registers[3]);
static ADRENO_CORESIGHT_ATTR(config_trace_cmd,
	&a3xx_coresight_registers[4]);
static ADRENO_CORESIGHT_ATTR(config_trace_bus_ctl,
	&a3xx_coresight_registers[5]);

static struct attribute *a3xx_coresight_attrs[] = {
	&coresight_attr_config_debug_bus.attr.attr,
	&coresight_attr_config_trace_start_cnt.attr.attr,
	&coresight_attr_config_trace_stop_cnt.attr.attr,
	&coresight_attr_config_trace_period_cnt.attr.attr,
	&coresight_attr_config_trace_cmd.attr.attr,
	&coresight_attr_config_trace_bus_ctl.attr.attr,
	NULL,
};

static const struct attribute_group a3xx_coresight_group = {
	.attrs = a3xx_coresight_attrs,
};

static const struct attribute_group *a3xx_coresight_groups[] = {
	&a3xx_coresight_group,
	NULL,
};

static struct adreno_coresight a3xx_coresight = {
	.registers = a3xx_coresight_registers,
	.count = ARRAY_SIZE(a3xx_coresight_registers),
	.groups = a3xx_coresight_groups,
};
#endif

/* Register offset defines for A3XX */
static unsigned int a3xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A3XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE_HI, ADRENO_REG_SKIP),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A3XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A3XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, A3XX_CP_ME_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_CNTL, A3XX_CP_RB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, A3XX_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE_HI, ADRENO_REG_SKIP),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, A3XX_CP_IB1_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, A3XX_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE_HI, ADRENO_REG_SKIP),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, A3XX_CP_IB2_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_TIMESTAMP, A3XX_CP_SCRATCH_REG0),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_SCRATCH_REG6, A3XX_CP_SCRATCH_REG6),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_SCRATCH_REG7, A3XX_CP_SCRATCH_REG7),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PROTECT_REG_0, A3XX_CP_PROTECT_REG_0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A3XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_PWR_1_LO,
					A3XX_RBBM_PERFCTR_PWR_1_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A3XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_CLOCK_CTL, A3XX_RBBM_CLOCK_CTL),
	ADRENO_REG_DEFINE(ADRENO_REG_PA_SC_AA_CONFIG, A3XX_PA_SC_AA_CONFIG),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PM_OVERRIDE2, A3XX_RBBM_PM_OVERRIDE2),
	ADRENO_REG_DEFINE(ADRENO_REG_SQ_GPR_MANAGEMENT, A3XX_SQ_GPR_MANAGEMENT),
	ADRENO_REG_DEFINE(ADRENO_REG_SQ_INST_STORE_MANAGEMENT,
				A3XX_SQ_INST_STORE_MANAGEMENT),
	ADRENO_REG_DEFINE(ADRENO_REG_TP0_CHICKEN, A3XX_TP0_CHICKEN),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, A3XX_RBBM_SW_RESET_CMD),
};

static int _load_firmware(struct kgsl_device *device, const char *fwfile,
			  void **buf, int *len)
{
	const struct firmware *fw = NULL;
	int ret;

	ret = request_firmware(&fw, fwfile, &device->pdev->dev);

	if (ret) {
		dev_err(&device->pdev->dev, "request_firmware(%s) failed: %d\n",
			     fwfile, ret);
		return ret;
	}

	if (!fw)
		return -EINVAL;

	*buf = devm_kmemdup(&device->pdev->dev, fw->data, fw->size, GFP_KERNEL);
	*len = fw->size;

	release_firmware(fw);
	return (*buf) ? 0 : -ENOMEM;
}

static int a3xx_microcode_read(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_firmware *pm4_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PM4);
	struct adreno_firmware *pfp_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PFP);
	const struct adreno_a3xx_core *a3xx_core = to_a3xx_core(adreno_dev);

	if (pm4_fw->fwvirt == NULL) {
		int len;
		void *ptr;

		int ret = _load_firmware(device,
			a3xx_core->pm4fw_name, &ptr, &len);

		if (ret) {
			dev_err(device->dev,  "Failed to read pm4 ucode %s\n",
				a3xx_core->pm4fw_name);
			return ret;
		}

		/* PM4 size is 3 dword aligned plus 1 dword of version */
		if (len % ((sizeof(uint32_t) * 3)) != sizeof(uint32_t)) {
			dev_err(device->dev,
				     "Bad pm4 microcode size: %d\n",
				     len);
			kfree(ptr);
			return -ENOMEM;
		}

		pm4_fw->size = len / sizeof(uint32_t);
		pm4_fw->fwvirt = ptr;
		pm4_fw->version = pm4_fw->fwvirt[1];
	}

	if (pfp_fw->fwvirt == NULL) {
		int len;
		void *ptr;

		int ret = _load_firmware(device,
			a3xx_core->pfpfw_name, &ptr, &len);
		if (ret) {
			dev_err(device->dev, "Failed to read pfp ucode %s\n",
					   a3xx_core->pfpfw_name);
			return ret;
		}

		/* PFP size shold be dword aligned */
		if (len % sizeof(uint32_t) != 0) {
			dev_err(device->dev,
						"Bad PFP microcode size: %d\n",
						len);
			kfree(ptr);
			return -ENOMEM;
		}

		pfp_fw->size = len / sizeof(uint32_t);
		pfp_fw->fwvirt = ptr;
		pfp_fw->version = pfp_fw->fwvirt[1];
	}

	return 0;
}

static void a3xx_microcode_load(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	size_t pm4_size = adreno_dev->fw[ADRENO_FW_PM4].size;
	size_t pfp_size = adreno_dev->fw[ADRENO_FW_PFP].size;

	/* load the CP ucode using AHB writes */
	kgsl_regwrite(device, A3XX_CP_ME_RAM_WADDR, 0);

	kgsl_regmap_bulk_write(&device->regmap, A3XX_CP_ME_RAM_DATA,
		&adreno_dev->fw[ADRENO_FW_PM4].fwvirt[1], pm4_size - 1);

	kgsl_regwrite(device, A3XX_CP_PFP_UCODE_ADDR, 0);

	kgsl_regmap_bulk_write(&device->regmap, A3XX_CP_PFP_UCODE_DATA,
		&adreno_dev->fw[ADRENO_FW_PFP].fwvirt[1], pfp_size - 1);
}

#if IS_ENABLED(CONFIG_COMMON_CLK_QCOM)
static void a3xx_clk_set_options(struct adreno_device *adreno_dev,
	const char *name, struct clk *clk, bool on)
{
	if (!clk || !adreno_is_a306a(adreno_dev))
		return;

	/* Handle clock settings for GFX PSCBCs */
	if (on) {
		if (!strcmp(name, "mem_iface_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_MEM);
		} else if (!strcmp(name, "core_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_RETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_RETAIN_MEM);
		}
	} else {
		if (!strcmp(name, "core_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_MEM);
		}
	}
}
#endif

static u64 a3xx_read_alwayson(struct adreno_device *adreno_dev)
{
	/* A3XX does not have a always on timer */
	return 0;
}

static irqreturn_t a3xx_irq_handler(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	irqreturn_t ret;
	u32 status;

	/* Get the current interrupt status */
	kgsl_regread(device, A3XX_RBBM_INT_0_STATUS, &status);

	/*
	 * Clear all the interrupt bits except A3XX_INT_RBBM_AHB_ERROR.
	 * The interrupt will stay asserted until it is cleared by the handler
	 * so don't touch it yet to avoid a storm
	 */

	kgsl_regwrite(device, A3XX_RBBM_INT_CLEAR_CMD,
		status & ~A3XX_INT_RBBM_AHB_ERROR);

	/* Call the helper to execute the callbacks */
	ret = adreno_irq_callbacks(adreno_dev, a3xx_irq_funcs, status);

	trace_kgsl_a3xx_irq_status(adreno_dev, status);

	/* Now clear AHB_ERROR if it was set */
	if (status & A3XX_INT_RBBM_AHB_ERROR)
		kgsl_regwrite(device, A3XX_RBBM_INT_CLEAR_CMD,
			A3XX_INT_RBBM_AHB_ERROR);

	return ret;
}

static bool a3xx_hw_isidle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status;

	kgsl_regread(device, A3XX_RBBM_STATUS, &status);

	if (status & 0x7ffffffe)
		return false;

	kgsl_regread(device, A3XX_RBBM_INT_0_STATUS, &status);

	/* Return busy if a interrupt is pending */
	return !((status & adreno_dev->irq_mask) ||
		atomic_read(&adreno_dev->pending_irq_refcnt));
}

static int a3xx_clear_pending_transactions(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 mask = A30X_VBIF_XIN_HALT_CTRL0_MASK;
	int ret;

	kgsl_regwrite(device, A3XX_VBIF_XIN_HALT_CTRL0, mask);
	ret = adreno_wait_for_halt_ack(device, A3XX_VBIF_XIN_HALT_CTRL1, mask);
	kgsl_regwrite(device, A3XX_VBIF_XIN_HALT_CTRL0, 0);

	return ret;
}

static bool a3xx_is_hw_collapsible(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/*
	 * Skip power collapse for A304, if power ctrl flag is set to
	 * non zero. As A304 soft_reset will not work, power collapse
	 * needs to disable to avoid soft_reset.
	 */
	if (adreno_is_a304(adreno_dev) && device->pwrctrl.ctrl_flags)
		return false;

	return adreno_isidle(adreno_dev);
}

static void a3xx_power_stats(struct adreno_device *adreno_dev,
		struct kgsl_power_stats *stats)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_busy_data *busy = &adreno_dev->busy_data;
	s64 freq = kgsl_pwrctrl_active_freq(&device->pwrctrl) / 1000000;
	u64 gpu_busy;

	/* Set the GPU busy counter for frequency scaling */
	gpu_busy = counter_delta(device, A3XX_RBBM_PERFCTR_PWR_1_LO,
		&busy->gpu_busy);

	stats->busy_time = gpu_busy / freq;

	if (!device->pwrctrl.bus_control)
		return;

	stats->ram_time = counter_delta(device, adreno_dev->ram_cycles_lo,
		&busy->bif_ram_cycles);

	stats->ram_wait = counter_delta(device, adreno_dev->starved_ram_lo,
		&busy->bif_starved_ram);
}

static int a3xx_setproperty(struct kgsl_device_private *dev_priv,
		u32 type, void __user *value, u32 sizebytes)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u32 enable;

	if (type != KGSL_PROP_PWRCTRL)
		return -ENODEV;

	if (sizebytes != sizeof(enable))
		return -EINVAL;

	if (copy_from_user(&enable, value, sizeof(enable)))
		return -EFAULT;

	mutex_lock(&device->mutex);
	if (enable) {
		device->pwrctrl.ctrl_flags = 0;

		if (!adreno_active_count_get(adreno_dev)) {
			a3xx_soft_fault_detect_start(adreno_dev);
			adreno_active_count_put(adreno_dev);
		}

		kgsl_pwrscale_enable(device);
	} else {
		kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
		device->pwrctrl.ctrl_flags = KGSL_PWR_ON;

		a3xx_soft_fault_detect_stop(adreno_dev);
		kgsl_pwrscale_disable(device, true);
	}
	mutex_unlock(&device->mutex);

	return 0;
}

const struct adreno_gpudev adreno_a3xx_gpudev = {
	.reg_offsets = a3xx_register_offsets,
	.irq_handler = a3xx_irq_handler,
	.probe = a3xx_probe,
	.rb_start = a3xx_rb_start,
	.init = a3xx_init,
	.start = a3xx_start,
	.snapshot = a3xx_snapshot,
#ifdef CONFIG_QCOM_KGSL_CORESIGHT
	.coresight = {&a3xx_coresight},
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_QCOM)
	.clk_set_options = a3xx_clk_set_options,
#endif
	.read_alwayson = a3xx_read_alwayson,
	.hw_isidle = a3xx_hw_isidle,
	.power_ops = &adreno_power_operations,
	.clear_pending_transactions = a3xx_clear_pending_transactions,
	.ringbuffer_submitcmd = a3xx_ringbuffer_submitcmd,
	.is_hw_collapsible = a3xx_is_hw_collapsible,
	.power_stats = a3xx_power_stats,
	.setproperty = a3xx_setproperty,
	.remove = a3xx_remove,
};
