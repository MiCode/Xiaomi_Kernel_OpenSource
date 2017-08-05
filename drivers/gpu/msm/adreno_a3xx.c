/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/msm_kgsl.h>

#include "kgsl.h"
#include "adreno.h"
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"
#include "a3xx_reg.h"
#include "adreno_a3xx.h"
#include "adreno_a4xx.h"
#include "a4xx_reg.h"
#include "adreno_cp_parser.h"
#include "adreno_trace.h"
#include "adreno_pm4types.h"
#include "adreno_perfcounter.h"
#include "adreno_snapshot.h"

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

static unsigned int adreno_a3xx_rbbm_clock_ctl_default(struct adreno_device
							*adreno_dev)
{
	if (adreno_is_a320(adreno_dev))
		return A320_RBBM_CLOCK_CTL_DEFAULT;
	else if (adreno_is_a330v2(adreno_dev))
		return A3XX_RBBM_CLOCK_CTL_DEFAULT;
	else if (adreno_is_a330(adreno_dev))
		return A330_RBBM_CLOCK_CTL_DEFAULT;
	return A3XX_RBBM_CLOCK_CTL_DEFAULT;
}

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

static void a3xx_efuse_speed_bin(struct adreno_device *adreno_dev)
{
	unsigned int val;
	unsigned int speed_bin[3];
	struct kgsl_device *device = &adreno_dev->dev;

	if (of_property_read_u32_array(device->pdev->dev.of_node,
		"qcom,gpu-speed-bin", speed_bin, 3))
		return;

	adreno_efuse_read_u32(adreno_dev, speed_bin[0], &val);

	adreno_dev->speed_bin = (val & speed_bin[1]) >> speed_bin[2];
}

static const struct {
	int (*check)(struct adreno_device *adreno_dev);
	void (*func)(struct adreno_device *adreno_dev);
} a3xx_efuse_funcs[] = {
	{ adreno_is_a306a, a3xx_efuse_speed_bin },
};

static void a3xx_check_features(struct adreno_device *adreno_dev)
{
	unsigned int i;

	if (adreno_efuse_map(adreno_dev))
		return;

	for (i = 0; i < ARRAY_SIZE(a3xx_efuse_funcs); i++) {
		if (a3xx_efuse_funcs[i].check(adreno_dev))
			a3xx_efuse_funcs[i].func(adreno_dev);
	}

	adreno_efuse_unmap(adreno_dev);
}

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
	int ret;

	/* Return if the fixup is already in place */
	if (test_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv))
		return 0;

	ret = kgsl_allocate_global(KGSL_DEVICE(adreno_dev),
		&adreno_dev->pwron_fixup, PAGE_SIZE,
		KGSL_MEMFLAGS_GPUREADONLY, 0, "pwron_fixup");

	if (ret)
		return ret;

	cmds = adreno_dev->pwron_fixup.hostptr;

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
		(cmds - (unsigned int *) adreno_dev->pwron_fixup.hostptr);

	/* Mark the flag in ->priv to show that we have the fix */
	set_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv);
	return 0;
}

static void a3xx_platform_setup(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev;

	if (adreno_is_a306(adreno_dev) || adreno_is_a306a(adreno_dev)
			|| adreno_is_a304(adreno_dev)) {
		gpudev = ADRENO_GPU_DEVICE(adreno_dev);
		gpudev->vbif_xin_halt_ctrl0_mask =
				A30X_VBIF_XIN_HALT_CTRL0_MASK;
	}

	/* Check efuse bits for various capabilties */
	a3xx_check_features(adreno_dev);
}

static int a3xx_send_me_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, 18);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);
	if (cmds == NULL)
		return -ENOSPC;

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

	/* Enable protected mode registers for A3XX/A4XX */
	*cmds++ = 0x20000000;

	*cmds++ = 0x00000000;
	*cmds++ = 0x00000000;

	ret = adreno_ringbuffer_submit_spin(rb, NULL, 2000);
	if (ret) {
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

		dev_err(device->dev, "CP initialization failed to idle\n");
		kgsl_device_snapshot(device, NULL);
	}

	return ret;
}

static int a3xx_rb_start(struct adreno_device *adreno_dev,
			 unsigned int start_type)
{
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	int ret;

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2).
	 * Also disable the host RPTR shadow register as it might be unreliable
	 * in certain circumstances.
	 */

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_CNTL,
		(ilog2(KGSL_RB_DWORDS >> 1) & 0x3F) |
		(1 << 27));

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_BASE,
			rb->buffer_desc.gpuaddr);

	ret = a3xx_microcode_load(adreno_dev, start_type);
	if (ret == 0) {
		/* clear ME_HALT to start micro engine */
		adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, 0);

		ret = a3xx_send_me_init(adreno_dev, rb);
	}

	return ret;
}

/*
 * a3xx_init() - Initialize gpu specific data
 * @adreno_dev: Pointer to adreno device
 */
static void a3xx_init(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	_a3xx_pwron_fixup(adreno_dev);

	/* Adjust snapshot section sizes according to core */
	if ((adreno_is_a330(adreno_dev) || adreno_is_a305b(adreno_dev))) {
		gpudev->snapshot_data->sect_sizes->cp_pfp =
					A320_SNAPSHOT_CP_STATE_SECTION_SIZE;
		gpudev->snapshot_data->sect_sizes->roq =
					A320_SNAPSHOT_ROQ_SECTION_SIZE;
		gpudev->snapshot_data->sect_sizes->cp_merciu =
					A320_SNAPSHOT_CP_MERCIU_SECTION_SIZE;
	}
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
		KGSL_DRV_CRIT_RATELIMIT(device,
			"RBBM | AHB bus error | %s | addr=%x | ports=%x:%x\n",
			reg & (1 << 28) ? "WRITE" : "READ",
			(reg & 0xFFFFF) >> 2, (reg >> 20) & 0x3,
			(reg >> 24) & 0xF);

		/* Clear the error */
		kgsl_regwrite(device, A3XX_RBBM_AHB_CMD, (1 << 3));
		break;
	}
	case A3XX_INT_RBBM_ATB_BUS_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ATB bus oveflow\n");
		break;
	case A3XX_INT_CP_T0_PACKET_IN_IB:
		KGSL_DRV_CRIT_RATELIMIT(device,
			"ringbuffer TO packet in IB interrupt\n");
		break;
	case A3XX_INT_CP_OPCODE_ERROR:
		KGSL_DRV_CRIT_RATELIMIT(device,
			"ringbuffer opcode error interrupt\n");
		break;
	case A3XX_INT_CP_RESERVED_BIT_ERROR:
		KGSL_DRV_CRIT_RATELIMIT(device,
			"ringbuffer reserved bit error interrupt\n");
		break;
	case A3XX_INT_CP_HW_FAULT:
		kgsl_regread(device, A3XX_CP_HW_FAULT, &reg);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP | Ringbuffer HW fault | status=%x\n", reg);
		break;
	case A3XX_INT_CP_REG_PROTECT_FAULT:
		kgsl_regread(device, A3XX_CP_PROTECT_STATUS, &reg);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP | Protected mode error| %s | addr=%x\n",
			reg & (1 << 24) ? "WRITE" : "READ",
			(reg & 0xFFFFF) >> 2);
		break;
	case A3XX_INT_CP_AHB_ERROR_HALT:
		KGSL_DRV_CRIT_RATELIMIT(device,
			"ringbuffer AHB error interrupt\n");
		break;
	case A3XX_INT_UCHE_OOB_ACCESS:
		KGSL_DRV_CRIT_RATELIMIT(device, "UCHE: Out of bounds access\n");
		break;
	default:
		KGSL_DRV_CRIT_RATELIMIT(device, "Unknown interrupt\n");
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

static struct adreno_irq_funcs a3xx_irq_funcs[32] = {
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

static struct adreno_irq a3xx_irq = {
	.funcs = a3xx_irq_funcs,
	.mask = A3XX_INT_MASK,
};

/* VBIF registers start after 0x3000 so use 0x0 as end of list marker */
static const struct adreno_vbif_data a304_vbif[] = {
	{ A3XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x0003 },
	{0, 0},
};

static const struct adreno_vbif_data a305_vbif[] = {
	/* Set up 16 deep read/write request queues */
	{ A3XX_VBIF_IN_RD_LIM_CONF0, 0x10101010 },
	{ A3XX_VBIF_IN_RD_LIM_CONF1, 0x10101010 },
	{ A3XX_VBIF_OUT_RD_LIM_CONF0, 0x10101010 },
	{ A3XX_VBIF_OUT_WR_LIM_CONF0, 0x10101010 },
	{ A3XX_VBIF_DDR_OUT_MAX_BURST, 0x0000303 },
	{ A3XX_VBIF_IN_WR_LIM_CONF0, 0x10101010 },
	{ A3XX_VBIF_IN_WR_LIM_CONF1, 0x10101010 },
	/* Enable WR-REQ */
	{ A3XX_VBIF_GATE_OFF_WRREQ_EN, 0x0000FF },
	/* Set up round robin arbitration between both AXI ports */
	{ A3XX_VBIF_ARB_CTL, 0x00000030 },
	/* Set up AOOO */
	{ A3XX_VBIF_OUT_AXI_AOOO_EN, 0x0000003C },
	{ A3XX_VBIF_OUT_AXI_AOOO, 0x003C003C },
	{0, 0},
};

static const struct adreno_vbif_data a305b_vbif[] = {
	{ A3XX_VBIF_IN_RD_LIM_CONF0, 0x00181818 },
	{ A3XX_VBIF_IN_WR_LIM_CONF0, 0x00181818 },
	{ A3XX_VBIF_OUT_RD_LIM_CONF0, 0x00000018 },
	{ A3XX_VBIF_OUT_WR_LIM_CONF0, 0x00000018 },
	{ A3XX_VBIF_DDR_OUT_MAX_BURST, 0x00000303 },
	{ A3XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x0003 },
	{0, 0},
};

static const struct adreno_vbif_data a305c_vbif[] = {
	{ A3XX_VBIF_IN_RD_LIM_CONF0, 0x00101010 },
	{ A3XX_VBIF_IN_WR_LIM_CONF0, 0x00101010 },
	{ A3XX_VBIF_OUT_RD_LIM_CONF0, 0x00000010 },
	{ A3XX_VBIF_OUT_WR_LIM_CONF0, 0x00000010 },
	{ A3XX_VBIF_DDR_OUT_MAX_BURST, 0x00000101 },
	{ A3XX_VBIF_ARB_CTL, 0x00000010 },
	/* Set up AOOO */
	{ A3XX_VBIF_OUT_AXI_AOOO_EN, 0x00000007 },
	{ A3XX_VBIF_OUT_AXI_AOOO, 0x00070007 },
	{0, 0},
};

static const struct adreno_vbif_data a306_vbif[] = {
	{ A3XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x0003 },
	{ A3XX_VBIF_OUT_RD_LIM_CONF0, 0x0000000A },
	{ A3XX_VBIF_OUT_WR_LIM_CONF0, 0x0000000A },
	{0, 0},
};

static const struct adreno_vbif_data a306a_vbif[] = {
	{ A3XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x0003 },
	{ A3XX_VBIF_OUT_RD_LIM_CONF0, 0x00000010 },
	{ A3XX_VBIF_OUT_WR_LIM_CONF0, 0x00000010 },
	{0, 0},
};

static const struct adreno_vbif_data a310_vbif[] = {
	{ A3XX_VBIF_ABIT_SORT, 0x0001000F },
	{ A3XX_VBIF_ABIT_SORT_CONF, 0x000000A4 },
	/* Enable WR-REQ */
	{ A3XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000001 },
	/* Set up VBIF_ROUND_ROBIN_QOS_ARB */
	{ A3XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x3 },
	{ A3XX_VBIF_IN_RD_LIM_CONF0, 0x18180C0C },
	{ A3XX_VBIF_IN_WR_LIM_CONF0, 0x1818000C },
	{0, 0},
};

static const struct adreno_vbif_data a320_vbif[] = {
	/* Set up 16 deep read/write request queues */
	{ A3XX_VBIF_IN_RD_LIM_CONF0, 0x10101010 },
	{ A3XX_VBIF_IN_RD_LIM_CONF1, 0x10101010 },
	{ A3XX_VBIF_OUT_RD_LIM_CONF0, 0x10101010 },
	{ A3XX_VBIF_OUT_WR_LIM_CONF0, 0x10101010 },
	{ A3XX_VBIF_DDR_OUT_MAX_BURST, 0x0000303 },
	{ A3XX_VBIF_IN_WR_LIM_CONF0, 0x10101010 },
	{ A3XX_VBIF_IN_WR_LIM_CONF1, 0x10101010 },
	/* Enable WR-REQ */
	{ A3XX_VBIF_GATE_OFF_WRREQ_EN, 0x0000FF },
	/* Set up round robin arbitration between both AXI ports */
	{ A3XX_VBIF_ARB_CTL, 0x00000030 },
	/* Set up AOOO */
	{ A3XX_VBIF_OUT_AXI_AOOO_EN, 0x0000003C },
	{ A3XX_VBIF_OUT_AXI_AOOO, 0x003C003C },
	/* Enable 1K sort */
	{ A3XX_VBIF_ABIT_SORT, 0x000000FF },
	{ A3XX_VBIF_ABIT_SORT_CONF, 0x000000A4 },
	{0, 0},
};

static const struct adreno_vbif_data a330_vbif[] = {
	/* Set up 16 deep read/write request queues */
	{ A3XX_VBIF_IN_RD_LIM_CONF0, 0x18181818 },
	{ A3XX_VBIF_IN_RD_LIM_CONF1, 0x00001818 },
	{ A3XX_VBIF_OUT_RD_LIM_CONF0, 0x00001818 },
	{ A3XX_VBIF_OUT_WR_LIM_CONF0, 0x00001818 },
	{ A3XX_VBIF_DDR_OUT_MAX_BURST, 0x0000303 },
	{ A3XX_VBIF_IN_WR_LIM_CONF0, 0x18181818 },
	{ A3XX_VBIF_IN_WR_LIM_CONF1, 0x00001818 },
	/* Enable WR-REQ */
	{ A3XX_VBIF_GATE_OFF_WRREQ_EN, 0x00003F },
	/* Set up round robin arbitration between both AXI ports */
	{ A3XX_VBIF_ARB_CTL, 0x00000030 },
	/* Set up VBIF_ROUND_ROBIN_QOS_ARB */
	{ A3XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x0001 },
	/* Set up AOOO */
	{ A3XX_VBIF_OUT_AXI_AOOO_EN, 0x0000003F },
	{ A3XX_VBIF_OUT_AXI_AOOO, 0x003F003F },
	/* Enable 1K sort */
	{ A3XX_VBIF_ABIT_SORT, 0x0001003F },
	{ A3XX_VBIF_ABIT_SORT_CONF, 0x000000A4 },
	/* Disable VBIF clock gating. This is to enable AXI running
	 * higher frequency than GPU.
	 */
	{ A3XX_VBIF_CLKON, 1 },
	{0, 0},
};

/*
 * Most of the VBIF registers on 8974v2 have the correct values at power on, so
 * we won't modify those if we don't need to
 */
static const struct adreno_vbif_data a330v2_vbif[] = {
	/* Enable 1k sort */
	{ A3XX_VBIF_ABIT_SORT, 0x0001003F },
	{ A3XX_VBIF_ABIT_SORT_CONF, 0x000000A4 },
	/* Enable WR-REQ */
	{ A3XX_VBIF_GATE_OFF_WRREQ_EN, 0x00003F },
	{ A3XX_VBIF_DDR_OUT_MAX_BURST, 0x0000303 },
	/* Set up VBIF_ROUND_ROBIN_QOS_ARB */
	{ A3XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x0003 },
	{0, 0},
};

/*
 * Most of the VBIF registers on a330v2.1 have the correct values at power on,
 * so we won't modify those if we don't need to
 */
static const struct adreno_vbif_data a330v21_vbif[] = {
	/* Enable WR-REQ */
	{ A3XX_VBIF_GATE_OFF_WRREQ_EN, 0x1 },
	/* Set up VBIF_ROUND_ROBIN_QOS_ARB */
	{ A3XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x0003 },
	{ A3XX_VBIF_IN_RD_LIM_CONF0, 0x18180c0c },
	{0, 0},
};

static const struct adreno_vbif_platform a3xx_vbif_platforms[] = {
	{ adreno_is_a304, a304_vbif },
	{ adreno_is_a305, a305_vbif },
	{ adreno_is_a305c, a305c_vbif },
	{ adreno_is_a306, a306_vbif },
	{ adreno_is_a306a, a306a_vbif },
	{ adreno_is_a310, a310_vbif },
	{ adreno_is_a320, a320_vbif },
	/* A330v2.1 needs to be ahead of A330v2 so the right device matches */
	{ adreno_is_a330v21, a330v21_vbif},
	/* A330v2 needs to be ahead of A330 so the right device matches */
	{ adreno_is_a330v2, a330v2_vbif },
	{ adreno_is_a330, a330_vbif },
	{ adreno_is_a305b, a305b_vbif },
};

/*
 * Define the available perfcounter groups - these get used by
 * adreno_perfcounter_get and adreno_perfcounter_put
 */

static struct adreno_perfcount_register a3xx_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_CP_0_LO,
		A3XX_RBBM_PERFCTR_CP_0_HI, 0, A3XX_CP_PERFCOUNTER_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_rbbm[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RBBM_0_LO,
		A3XX_RBBM_PERFCTR_RBBM_0_HI, 1, A3XX_RBBM_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RBBM_1_LO,
		A3XX_RBBM_PERFCTR_RBBM_1_HI, 2, A3XX_RBBM_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_pc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PC_0_LO,
		A3XX_RBBM_PERFCTR_PC_0_HI, 3, A3XX_PC_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PC_1_LO,
		A3XX_RBBM_PERFCTR_PC_1_HI, 4, A3XX_PC_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PC_2_LO,
		A3XX_RBBM_PERFCTR_PC_2_HI, 5, A3XX_PC_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PC_3_LO,
		A3XX_RBBM_PERFCTR_PC_3_HI, 6, A3XX_PC_PERFCOUNTER3_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_vfd[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_VFD_0_LO,
		A3XX_RBBM_PERFCTR_VFD_0_HI, 7, A3XX_VFD_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_VFD_1_LO,
		A3XX_RBBM_PERFCTR_VFD_1_HI, 8, A3XX_VFD_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_hlsq[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_0_LO,
		A3XX_RBBM_PERFCTR_HLSQ_0_HI, 9,
		A3XX_HLSQ_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_1_LO,
		A3XX_RBBM_PERFCTR_HLSQ_1_HI, 10,
		A3XX_HLSQ_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_2_LO,
		A3XX_RBBM_PERFCTR_HLSQ_2_HI, 11,
		A3XX_HLSQ_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_3_LO,
		A3XX_RBBM_PERFCTR_HLSQ_3_HI, 12,
		A3XX_HLSQ_PERFCOUNTER3_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_4_LO,
		A3XX_RBBM_PERFCTR_HLSQ_4_HI, 13,
		A3XX_HLSQ_PERFCOUNTER4_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_5_LO,
		A3XX_RBBM_PERFCTR_HLSQ_5_HI, 14,
		A3XX_HLSQ_PERFCOUNTER5_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_vpc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_VPC_0_LO,
		A3XX_RBBM_PERFCTR_VPC_0_HI, 15, A3XX_VPC_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_VPC_1_LO,
		A3XX_RBBM_PERFCTR_VPC_1_HI, 16, A3XX_VPC_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_tse[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TSE_0_LO,
		A3XX_RBBM_PERFCTR_TSE_0_HI, 17, A3XX_GRAS_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TSE_1_LO,
		A3XX_RBBM_PERFCTR_TSE_1_HI, 18, A3XX_GRAS_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_ras[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RAS_0_LO,
		A3XX_RBBM_PERFCTR_RAS_0_HI, 19, A3XX_GRAS_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RAS_1_LO,
		A3XX_RBBM_PERFCTR_RAS_1_HI, 20, A3XX_GRAS_PERFCOUNTER3_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_0_LO,
		A3XX_RBBM_PERFCTR_UCHE_0_HI, 21,
		A3XX_UCHE_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_1_LO,
		A3XX_RBBM_PERFCTR_UCHE_1_HI, 22,
		A3XX_UCHE_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_2_LO,
		A3XX_RBBM_PERFCTR_UCHE_2_HI, 23,
		A3XX_UCHE_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_3_LO,
		A3XX_RBBM_PERFCTR_UCHE_3_HI, 24,
		A3XX_UCHE_PERFCOUNTER3_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_4_LO,
		A3XX_RBBM_PERFCTR_UCHE_4_HI, 25,
		A3XX_UCHE_PERFCOUNTER4_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_5_LO,
		A3XX_RBBM_PERFCTR_UCHE_5_HI, 26,
		A3XX_UCHE_PERFCOUNTER5_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_0_LO,
		A3XX_RBBM_PERFCTR_TP_0_HI, 27, A3XX_TP_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_1_LO,
		A3XX_RBBM_PERFCTR_TP_1_HI, 28, A3XX_TP_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_2_LO,
		A3XX_RBBM_PERFCTR_TP_2_HI, 29, A3XX_TP_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_3_LO,
		A3XX_RBBM_PERFCTR_TP_3_HI, 30, A3XX_TP_PERFCOUNTER3_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_4_LO,
		A3XX_RBBM_PERFCTR_TP_4_HI, 31, A3XX_TP_PERFCOUNTER4_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_5_LO,
		A3XX_RBBM_PERFCTR_TP_5_HI, 32, A3XX_TP_PERFCOUNTER5_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_0_LO,
		A3XX_RBBM_PERFCTR_SP_0_HI, 33, A3XX_SP_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_1_LO,
		A3XX_RBBM_PERFCTR_SP_1_HI, 34, A3XX_SP_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_2_LO,
		A3XX_RBBM_PERFCTR_SP_2_HI, 35, A3XX_SP_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_3_LO,
		A3XX_RBBM_PERFCTR_SP_3_HI, 36, A3XX_SP_PERFCOUNTER3_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_4_LO,
		A3XX_RBBM_PERFCTR_SP_4_HI, 37, A3XX_SP_PERFCOUNTER4_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_5_LO,
		A3XX_RBBM_PERFCTR_SP_5_HI, 38, A3XX_SP_PERFCOUNTER5_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_6_LO,
		A3XX_RBBM_PERFCTR_SP_6_HI, 39, A3XX_SP_PERFCOUNTER6_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_7_LO,
		A3XX_RBBM_PERFCTR_SP_7_HI, 40, A3XX_SP_PERFCOUNTER7_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RB_0_LO,
		A3XX_RBBM_PERFCTR_RB_0_HI, 41, A3XX_RB_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RB_1_LO,
		A3XX_RBBM_PERFCTR_RB_1_HI, 42, A3XX_RB_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PWR_0_LO,
		A3XX_RBBM_PERFCTR_PWR_0_HI, -1, 0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PWR_1_LO,
		A3XX_RBBM_PERFCTR_PWR_1_HI, -1, 0 },
};

static struct adreno_perfcount_register a3xx_perfcounters_vbif[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF_PERF_CNT0_LO,
		A3XX_VBIF_PERF_CNT0_HI, -1, 0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF_PERF_CNT1_LO,
		A3XX_VBIF_PERF_CNT1_HI, -1, 0 },
};
static struct adreno_perfcount_register a3xx_perfcounters_vbif_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF_PERF_PWR_CNT0_LO,
		A3XX_VBIF_PERF_PWR_CNT0_HI, -1, 0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF_PERF_PWR_CNT1_LO,
		A3XX_VBIF_PERF_PWR_CNT1_HI, -1, 0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF_PERF_PWR_CNT2_LO,
		A3XX_VBIF_PERF_PWR_CNT2_HI, -1, 0 },
};
static struct adreno_perfcount_register a3xx_perfcounters_vbif2[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF2_PERF_CNT_LOW0,
		A3XX_VBIF2_PERF_CNT_HIGH0, -1, A3XX_VBIF2_PERF_CNT_SEL0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF2_PERF_CNT_LOW1,
		A3XX_VBIF2_PERF_CNT_HIGH1, -1, A3XX_VBIF2_PERF_CNT_SEL1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF2_PERF_CNT_LOW2,
		A3XX_VBIF2_PERF_CNT_HIGH2, -1, A3XX_VBIF2_PERF_CNT_SEL2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF2_PERF_CNT_LOW3,
		A3XX_VBIF2_PERF_CNT_HIGH3, -1, A3XX_VBIF2_PERF_CNT_SEL3 },
};
/*
 * Placing EN register in select field since vbif perf counters
 * dont have select register to program
 */
static struct adreno_perfcount_register a3xx_perfcounters_vbif2_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0,
		0, A3XX_VBIF2_PERF_PWR_CNT_LOW0,
		A3XX_VBIF2_PERF_PWR_CNT_HIGH0, -1,
		A3XX_VBIF2_PERF_PWR_CNT_EN0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0,
		0, A3XX_VBIF2_PERF_PWR_CNT_LOW1,
		A3XX_VBIF2_PERF_PWR_CNT_HIGH1, -1,
		A3XX_VBIF2_PERF_PWR_CNT_EN1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0,
		0, A3XX_VBIF2_PERF_PWR_CNT_LOW2,
		A3XX_VBIF2_PERF_PWR_CNT_HIGH2, -1,
		A3XX_VBIF2_PERF_PWR_CNT_EN2 },
};

#define A3XX_PERFCOUNTER_GROUP(offset, name) \
	ADRENO_PERFCOUNTER_GROUP(a3xx, offset, name)

#define A3XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a3xx, offset, name, flags)

static struct adreno_perfcount_group a3xx_perfcounter_groups[] = {
	A3XX_PERFCOUNTER_GROUP(CP, cp),
	A3XX_PERFCOUNTER_GROUP(RBBM, rbbm),
	A3XX_PERFCOUNTER_GROUP(PC, pc),
	A3XX_PERFCOUNTER_GROUP(VFD, vfd),
	A3XX_PERFCOUNTER_GROUP(HLSQ, hlsq),
	A3XX_PERFCOUNTER_GROUP(VPC, vpc),
	A3XX_PERFCOUNTER_GROUP(TSE, tse),
	A3XX_PERFCOUNTER_GROUP(RAS, ras),
	A3XX_PERFCOUNTER_GROUP(UCHE, uche),
	A3XX_PERFCOUNTER_GROUP(TP, tp),
	A3XX_PERFCOUNTER_GROUP(SP, sp),
	A3XX_PERFCOUNTER_GROUP(RB, rb),
	A3XX_PERFCOUNTER_GROUP_FLAGS(PWR, pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
	A3XX_PERFCOUNTER_GROUP(VBIF, vbif),
	A3XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, vbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
};

static struct adreno_perfcounters a3xx_perfcounters = {
	a3xx_perfcounter_groups,
	ARRAY_SIZE(a3xx_perfcounter_groups),
};

static struct adreno_ft_perf_counters a3xx_ft_perf_counters[] = {
	{KGSL_PERFCOUNTER_GROUP_SP, SP_ALU_ACTIVE_CYCLES},
	{KGSL_PERFCOUNTER_GROUP_SP, SP0_ICL1_MISSES},
	{KGSL_PERFCOUNTER_GROUP_SP, SP_FS_CFLOW_INSTRUCTIONS},
	{KGSL_PERFCOUNTER_GROUP_TSE, TSE_INPUT_PRIM_NUM},
};

static void a3xx_perfcounter_init(struct adreno_device *adreno_dev)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);

	/* SP[3] counter is broken on a330 so disable it if a330 device */
	if (adreno_is_a330(adreno_dev))
		a3xx_perfcounters_sp[3].countable = KGSL_PERFCOUNTER_BROKEN;

	if (counters &&
		(adreno_is_a306(adreno_dev) || adreno_is_a304(adreno_dev) ||
		adreno_is_a306a(adreno_dev))) {
		counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs =
			a3xx_perfcounters_vbif2;
		counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR].regs =
			a3xx_perfcounters_vbif2_pwr;
	}

	/*
	 * Enable the GPU busy count counter. This is a fixed counter on
	 * A3XX so we don't need to bother checking the return value
	 */
	adreno_perfcounter_get(adreno_dev, KGSL_PERFCOUNTER_GROUP_PWR, 1,
		NULL, NULL, PERFCOUNTER_FLAG_KERNEL);
}

static void a3xx_perfcounter_close(struct adreno_device *adreno_dev)
{
	adreno_perfcounter_put(adreno_dev, KGSL_PERFCOUNTER_GROUP_PWR, 1,
		PERFCOUNTER_FLAG_KERNEL);
}

/**
 * a3xx_protect_init() - Initializes register protection on a3xx
 * @adreno_dev: Pointer to the device structure
 * Performs register writes to enable protected access to sensitive
 * registers
 */
static void a3xx_protect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int index = 0;
	struct kgsl_protected_registers *iommu_regs;

	/* enable access protection to privileged registers */
	kgsl_regwrite(device, A3XX_CP_PROTECT_CTRL, 0x00000007);

	/* RBBM registers */
	adreno_set_protected_registers(adreno_dev, &index, 0x18, 0);
	adreno_set_protected_registers(adreno_dev, &index, 0x20, 2);
	adreno_set_protected_registers(adreno_dev, &index, 0x33, 0);
	adreno_set_protected_registers(adreno_dev, &index, 0x42, 0);
	adreno_set_protected_registers(adreno_dev, &index, 0x50, 4);
	adreno_set_protected_registers(adreno_dev, &index, 0x63, 0);
	adreno_set_protected_registers(adreno_dev, &index, 0x100, 4);

	/* CP registers */
	adreno_set_protected_registers(adreno_dev, &index, 0x1C0, 5);
	adreno_set_protected_registers(adreno_dev, &index, 0x1EC, 1);
	adreno_set_protected_registers(adreno_dev, &index, 0x1F6, 1);
	adreno_set_protected_registers(adreno_dev, &index, 0x1F8, 2);
	adreno_set_protected_registers(adreno_dev, &index, 0x45E, 2);
	adreno_set_protected_registers(adreno_dev, &index, 0x460, 4);

	/* RB registers */
	adreno_set_protected_registers(adreno_dev, &index, 0xCC0, 0);

	/* VBIF registers */
	adreno_set_protected_registers(adreno_dev, &index, 0x3000, 6);

	/* SMMU registers */
	iommu_regs = kgsl_mmu_get_prot_regs(&device->mmu);
	if (iommu_regs)
		adreno_set_protected_registers(adreno_dev, &index,
				iommu_regs->base, iommu_regs->range);
}

static void a3xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	adreno_vbif_start(adreno_dev, a3xx_vbif_platforms,
			ARRAY_SIZE(a3xx_vbif_platforms));

	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, A3XX_RBBM_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/* Tune the hystersis counters for SP and CP idle detection */
	kgsl_regwrite(device, A3XX_RBBM_SP_HYST_CNT, 0x10);
	kgsl_regwrite(device, A3XX_RBBM_WAIT_IDLE_CLOCKS_CTL, 0x10);

	/* Enable the RBBM error reporting bits.  This lets us get
	   useful information on failure */

	kgsl_regwrite(device, A3XX_RBBM_AHB_CTL0, 0x00000001);

	/* Enable AHB error reporting */
	kgsl_regwrite(device, A3XX_RBBM_AHB_CTL1, 0xA6FFFFFF);

	/* Turn on the power counters */
	kgsl_regwrite(device, A3XX_RBBM_RBBM_CTL, 0x00030000);

	/* Turn on hang detection - this spews a lot of useful information
	 * into the RBBM registers on a hang */
	if (adreno_is_a330v2(adreno_dev)) {
		set_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv);
		gpudev->irq->mask |= (1 << A3XX_INT_MISC_HANG_DETECT);
		kgsl_regwrite(device, A3XX_RBBM_INTERFACE_HANG_INT_CTL,
				(1 << 31) | 0xFFFF);
	} else
		kgsl_regwrite(device, A3XX_RBBM_INTERFACE_HANG_INT_CTL,
				(1 << 16) | 0xFFF);

	/* Enable 64-byte cacheline size. HW Default is 32-byte (0x000000E0). */
	kgsl_regwrite(device, A3XX_UCHE_CACHE_MODE_CONTROL_REG, 0x00000001);

	/* Enable VFD to access most of the UCHE (7 ways out of 8) */
	kgsl_regwrite(device, A3XX_UCHE_CACHE_WAYS_VFD, 0x07);

	/* Enable Clock gating */
	kgsl_regwrite(device, A3XX_RBBM_CLOCK_CTL,
		adreno_a3xx_rbbm_clock_ctl_default(adreno_dev));

	if (adreno_is_a330v2(adreno_dev))
		kgsl_regwrite(device, A3XX_RBBM_GPR0_CTL,
			A330v2_RBBM_GPR0_CTL_DEFAULT);
	else if (adreno_is_a330(adreno_dev))
		kgsl_regwrite(device, A3XX_RBBM_GPR0_CTL,
			A330_RBBM_GPR0_CTL_DEFAULT);
	else if (adreno_is_a310(adreno_dev))
		kgsl_regwrite(device, A3XX_RBBM_GPR0_CTL,
			A310_RBBM_GPR0_CTL_DEFAULT);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_USES_OCMEM))
		kgsl_regwrite(device, A3XX_RB_GMEM_BASE_ADDR,
			(unsigned int)(adreno_dev->gmem_base >> 14));

	/* Turn on protection */
	a3xx_protect_init(adreno_dev);

	/* Turn on performance counters */
	kgsl_regwrite(device, A3XX_RBBM_PERFCTR_CTL, 0x01);

	kgsl_regwrite(device, A3XX_CP_DEBUG, A3XX_CP_DEBUG_DEFAULT);

	/* CP ROQ queue sizes (bytes) - RB:16, ST:16, IB1:32, IB2:64 */
	if (adreno_is_a305b(adreno_dev) ||
			adreno_is_a310(adreno_dev) ||
			adreno_is_a330(adreno_dev))
		kgsl_regwrite(device, A3XX_CP_QUEUE_THRESHOLDS, 0x003E2008);
	else
		kgsl_regwrite(device, A3XX_CP_QUEUE_THRESHOLDS, 0x000E0602);

}

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

static unsigned int a3xx_int_bits[ADRENO_INT_BITS_MAX] = {
	ADRENO_INT_DEFINE(ADRENO_INT_RBBM_AHB_ERROR, A3XX_INT_RBBM_AHB_ERROR),
};

/* Register offset defines for A3XX */
static unsigned int a3xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_WADDR, A3XX_CP_ME_RAM_WADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_DATA, A3XX_CP_ME_RAM_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PFP_UCODE_DATA, A3XX_CP_PFP_UCODE_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PFP_UCODE_ADDR, A3XX_CP_PFP_UCODE_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_WFI_PEND_CTR, A3XX_CP_WFI_PEND_CTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A3XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE_HI, ADRENO_REG_SKIP),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A3XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A3XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CNTL, A3XX_CP_CNTL),
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
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_RADDR, A3XX_CP_ME_RAM_RADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_ADDR, A3XX_CP_ROQ_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_DATA, A3XX_CP_ROQ_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_ADDR, A3XX_CP_MERCIU_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_DATA, A3XX_CP_MERCIU_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_DATA2, A3XX_CP_MERCIU_DATA2),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MEQ_ADDR, A3XX_CP_MEQ_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MEQ_DATA, A3XX_CP_MEQ_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PROTECT_REG_0, A3XX_CP_PROTECT_REG_0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A3XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_CTL, A3XX_RBBM_PERFCTR_CTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
					A3XX_RBBM_PERFCTR_LOAD_CMD0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
					A3XX_RBBM_PERFCTR_LOAD_CMD1),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_PWR_1_LO,
					A3XX_RBBM_PERFCTR_PWR_1_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A3XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_STATUS, A3XX_RBBM_INT_0_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_CLEAR_CMD,
				A3XX_RBBM_INT_CLEAR_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_CLOCK_CTL, A3XX_RBBM_CLOCK_CTL),
	ADRENO_REG_DEFINE(ADRENO_REG_VPC_DEBUG_RAM_SEL,
				A3XX_VPC_VPC_DEBUG_RAM_SEL),
	ADRENO_REG_DEFINE(ADRENO_REG_VPC_DEBUG_RAM_READ,
				A3XX_VPC_VPC_DEBUG_RAM_READ),
	ADRENO_REG_DEFINE(ADRENO_REG_PA_SC_AA_CONFIG, A3XX_PA_SC_AA_CONFIG),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PM_OVERRIDE2, A3XX_RBBM_PM_OVERRIDE2),
	ADRENO_REG_DEFINE(ADRENO_REG_SQ_GPR_MANAGEMENT, A3XX_SQ_GPR_MANAGEMENT),
	ADRENO_REG_DEFINE(ADRENO_REG_SQ_INST_STORE_MANAGMENT,
				A3XX_SQ_INST_STORE_MANAGMENT),
	ADRENO_REG_DEFINE(ADRENO_REG_TP0_CHICKEN, A3XX_TP0_CHICKEN),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_RBBM_CTL, A3XX_RBBM_RBBM_CTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, A3XX_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_UCHE_INVALIDATE0,
			A3XX_UCHE_CACHE_INVALIDATE0_REG),
	ADRENO_REG_DEFINE(ADRENO_REG_UCHE_INVALIDATE1,
			A3XX_UCHE_CACHE_INVALIDATE1_REG),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_RBBM_0_LO,
			A3XX_RBBM_PERFCTR_RBBM_0_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_RBBM_0_HI,
			A3XX_RBBM_PERFCTR_RBBM_0_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
				A3XX_RBBM_PERFCTR_LOAD_VALUE_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
				A3XX_RBBM_PERFCTR_LOAD_VALUE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_XIN_HALT_CTRL0,
				A3XX_VBIF_XIN_HALT_CTRL0),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_XIN_HALT_CTRL1,
				A3XX_VBIF_XIN_HALT_CTRL1),
};

static const struct adreno_reg_offsets a3xx_reg_offsets = {
	.offsets = a3xx_register_offsets,
	.offset_0 = ADRENO_REG_REGISTER_MAX,
};

/*
 * Defined the size of sections dumped in snapshot, these values
 * may change after initialization based on the specific core
 */
static struct adreno_snapshot_sizes a3xx_snap_sizes = {
	.cp_pfp = 0x14,
	.vpc_mem = 512,
	.cp_meq = 16,
	.shader_mem = 0x4000,
	.cp_merciu = 0,
	.roq = 128,
};

static struct adreno_snapshot_data a3xx_snapshot_data = {
	.sect_sizes = &a3xx_snap_sizes,
};

static int _load_firmware(struct kgsl_device *device, const char *fwfile,
			  void **buf, int *len)
{
	const struct firmware *fw = NULL;
	int ret;

	ret = request_firmware(&fw, fwfile, device->dev);

	if (ret) {
		KGSL_DRV_ERR(device, "request_firmware(%s) failed: %d\n",
			     fwfile, ret);
		return ret;
	}

	if (fw)
		*buf = kmalloc(fw->size, GFP_KERNEL);
	else
		return -EINVAL;

	if (*buf) {
		memcpy(*buf, fw->data, fw->size);
		*len = fw->size;
	}

	release_firmware(fw);
	return (*buf != NULL) ? 0 : -ENOMEM;
}

int a3xx_microcode_read(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_dev->pm4_fw == NULL) {
		int len;
		void *ptr;

		int ret = _load_firmware(device,
			adreno_dev->gpucore->pm4fw_name, &ptr, &len);

		if (ret) {
			KGSL_DRV_FATAL(device, "Failed to read pm4 ucode %s\n",
					   adreno_dev->gpucore->pm4fw_name);
			return ret;
		}

		/* PM4 size is 3 dword aligned plus 1 dword of version */
		if (len % ((sizeof(uint32_t) * 3)) != sizeof(uint32_t)) {
			KGSL_DRV_ERR(device, "Bad pm4 microcode size: %d\n",
				len);
			kfree(ptr);
			return -ENOMEM;
		}

		adreno_dev->pm4_fw_size = len / sizeof(uint32_t);
		adreno_dev->pm4_fw = ptr;
		adreno_dev->pm4_fw_version = adreno_dev->pm4_fw[1];
	}

	if (adreno_dev->pfp_fw == NULL) {
		int len;
		void *ptr;

		int ret = _load_firmware(device,
			adreno_dev->gpucore->pfpfw_name, &ptr, &len);
		if (ret) {
			KGSL_DRV_FATAL(device, "Failed to read pfp ucode %s\n",
					   adreno_dev->gpucore->pfpfw_name);
			return ret;
		}

		/* PFP size shold be dword aligned */
		if (len % sizeof(uint32_t) != 0) {
			KGSL_DRV_ERR(device, "Bad PFP microcode size: %d\n",
						len);
			kfree(ptr);
			return -ENOMEM;
		}

		adreno_dev->pfp_fw_size = len / sizeof(uint32_t);
		adreno_dev->pfp_fw = ptr;
		adreno_dev->pfp_fw_version = adreno_dev->pfp_fw[5];
	}

	return 0;
}
/**
 * load_pm4_ucode() - Load pm4 ucode
 * @adreno_dev: Pointer to an adreno device
 * @start: Starting index in pm4 ucode to load
 * @end: Ending index of pm4 ucode to load
 * @addr: Address to load the pm4 ucode
 *
 * Load the pm4 ucode from @start at @addr.
 */
static inline void load_pm4_ucode(struct adreno_device *adreno_dev,
			unsigned int start, unsigned int end, unsigned int addr)
{
	int i;

	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_RAM_WADDR, addr);
	for (i = start; i < end; i++)
		adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_RAM_DATA,
					adreno_dev->pm4_fw[i]);
}
/**
 * load_pfp_ucode() - Load pfp ucode
 * @adreno_dev: Pointer to an adreno device
 * @start: Starting index in pfp ucode to load
 * @end: Ending index of pfp ucode to load
 * @addr: Address to load the pfp ucode
 *
 * Load the pfp ucode from @start at @addr.
 */
static inline void load_pfp_ucode(struct adreno_device *adreno_dev,
			unsigned int start, unsigned int end, unsigned int addr)
{
	int i;

	adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR, addr);
	for (i = start; i < end; i++)
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA,
						adreno_dev->pfp_fw[i]);
}

/**
 * _ringbuffer_bootstrap_ucode() - Bootstrap GPU Ucode
 * @adreno_dev: Pointer to an adreno device
 * @rb: The ringbuffer to boostrap the code into
 * @load_jt: If non zero only load Jump tables
 *
 * Bootstrap ucode for GPU
 * load_jt == 0, bootstrap full microcode
 * load_jt == 1, bootstrap jump tables of microcode
 *
 * For example a bootstrap packet would like below
 * Setup a type3 bootstrap packet
 * PFP size to bootstrap
 * PFP addr to write the PFP data
 * PM4 size to bootstrap
 * PM4 addr to write the PM4 data
 * PFP dwords from microcode to bootstrap
 * PM4 size dwords from microcode to bootstrap
 */
static int _ringbuffer_bootstrap_ucode(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, unsigned int load_jt)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *cmds, bootstrap_size, rb_size;
	int i = 0;
	int ret;
	unsigned int pm4_size, pm4_idx, pm4_addr, pfp_size, pfp_idx, pfp_addr;

	/* Only bootstrap jump tables of ucode */
	if (load_jt) {
		pm4_idx = adreno_dev->gpucore->pm4_jt_idx;
		pm4_addr = adreno_dev->gpucore->pm4_jt_addr;
		pfp_idx = adreno_dev->gpucore->pfp_jt_idx;
		pfp_addr = adreno_dev->gpucore->pfp_jt_addr;
	} else {
		/* Bootstrap full ucode */
		pm4_idx = 1;
		pm4_addr = 0;
		pfp_idx = 1;
		pfp_addr = 0;
	}

	pm4_size = (adreno_dev->pm4_fw_size - pm4_idx);
	pfp_size = (adreno_dev->pfp_fw_size - pfp_idx);

	bootstrap_size = (pm4_size + pfp_size + 5);

	/*
	 * Overwrite the first entry in the jump table with the special
	 * bootstrap opcode
	 */

	if (adreno_is_a4xx(adreno_dev)) {
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR,
			0x400);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA,
			 0x6f0009);
		/*
		 * The support packets (the RMW and INTERRUPT) that are sent
		 * after the bootstrap packet should not be included in the size
		 * of the bootstrap packet but we do need to reserve enough
		 * space for those too
		 */
		rb_size = bootstrap_size + 6;
	} else {
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR,
			0x200);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA,
			 0x6f0005);
		rb_size = bootstrap_size;
	}

	/* clear ME_HALT to start micro engine */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, 0);

	cmds = adreno_ringbuffer_allocspace(rb, rb_size);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);
	if (cmds == NULL)
		return -ENOSPC;

	/* Construct the packet that bootsraps the ucode */
	*cmds++ = cp_type3_packet(CP_BOOTSTRAP_UCODE, (bootstrap_size - 1));
	*cmds++ = pfp_size;
	*cmds++ = pfp_addr;
	*cmds++ = pm4_size;
	*cmds++ = pm4_addr;

	/**
	 * Theory of operation:
	 *
	 * In A4x, we cannot have the PFP executing instructions while its
	 * instruction RAM is loading. We load the PFP's instruction RAM
	 * using type-0 writes from the ME.
	 *
	 * To make sure the PFP is not fetching instructions at the same
	 * time, we put it in a one-instruction loop:
	 * mvc (ME), (ringbuffer)
	 * which executes repeatedly until all of the data has been moved
	 * from the ring buffer to the ME.
	 */
	if (adreno_is_a4xx(adreno_dev)) {
		for (i = pm4_idx; i < adreno_dev->pm4_fw_size; i++)
			*cmds++ = adreno_dev->pm4_fw[i];
		for (i = pfp_idx; i < adreno_dev->pfp_fw_size; i++)
			*cmds++ = adreno_dev->pfp_fw[i];

		*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
		*cmds++ = 0x20000000 + A4XX_CP_RB_WPTR;
		*cmds++ = 0xffffffff;
		*cmds++ = 0x00000002;
		*cmds++ = cp_type3_packet(CP_INTERRUPT, 1);
		*cmds++ = 0;

		rb->_wptr = rb->_wptr - 2;
		adreno_ringbuffer_submit(rb, NULL);
		rb->_wptr = rb->_wptr + 2;
	} else {
		for (i = pfp_idx; i < adreno_dev->pfp_fw_size; i++)
			*cmds++ = adreno_dev->pfp_fw[i];
		for (i = pm4_idx; i < adreno_dev->pm4_fw_size; i++)
			*cmds++ = adreno_dev->pm4_fw[i];
		adreno_ringbuffer_submit(rb, NULL);
	}

	/* idle device to validate bootstrap */
	ret = adreno_spin_idle(adreno_dev, 2000);

	if (ret) {
		KGSL_DRV_ERR(device, "microcode bootstrap failed to idle\n");
		kgsl_device_snapshot(device, NULL);
	}

	/* Clear the chicken bit for speed up on A430 and its derivatives */
	if (!adreno_is_a420(adreno_dev))
		kgsl_regwrite(device, A4XX_CP_DEBUG,
					A4XX_CP_DEBUG_DEFAULT & ~(1 << 14));

	return ret;
}

int a3xx_microcode_load(struct adreno_device *adreno_dev,
				unsigned int start_type)
{
	int status;
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);

	if (start_type == ADRENO_START_COLD) {
		/* If bootstrapping if supported to load ucode */
		if (adreno_bootstrap_ucode(adreno_dev)) {

			/*
			 * load first pm4_bstrp_size + pfp_bstrp_size microcode
			 * dwords using AHB write, this small microcode has
			 * dispatcher + booter this initial microcode enables
			 * CP to understand CP_BOOTSTRAP_UCODE packet in
			 * function _ringbuffer_bootstrap_ucode.
			 * CP_BOOTSTRAP_UCODE packet loads rest of the
			 * microcode.
			 */

			load_pm4_ucode(adreno_dev, 1,
				adreno_dev->gpucore->pm4_bstrp_size+1, 0);

			load_pfp_ucode(adreno_dev, 1,
				adreno_dev->gpucore->pfp_bstrp_size+1, 0);

			/* Bootstrap rest of the ucode here */
			status = _ringbuffer_bootstrap_ucode(adreno_dev, rb, 0);
			if (status != 0)
				return status;

		} else {
			/* load the CP ucode using AHB writes */
			load_pm4_ucode(adreno_dev, 1, adreno_dev->pm4_fw_size,
				0);

			/* load the prefetch parser ucode using AHB writes */
			load_pfp_ucode(adreno_dev, 1, adreno_dev->pfp_fw_size,
				0);
		}
	} else if (start_type == ADRENO_START_WARM) {
			/* If bootstrapping if supported to load jump tables */
		if (adreno_bootstrap_ucode(adreno_dev)) {
			status = _ringbuffer_bootstrap_ucode(adreno_dev, rb, 1);
			if (status != 0)
				return status;

		} else {
			/* load the CP jump tables using AHB writes */
			load_pm4_ucode(adreno_dev,
				adreno_dev->gpucore->pm4_jt_idx,
				adreno_dev->pm4_fw_size,
				adreno_dev->gpucore->pm4_jt_addr);

			/*
			 * load the prefetch parser jump tables using AHB writes
			 */
			load_pfp_ucode(adreno_dev,
				adreno_dev->gpucore->pfp_jt_idx,
				adreno_dev->pfp_fw_size,
				adreno_dev->gpucore->pfp_jt_addr);
		}
	} else
		return -EINVAL;

	return 0;
}

struct adreno_gpudev adreno_a3xx_gpudev = {
	.reg_offsets = &a3xx_reg_offsets,
	.int_bits = a3xx_int_bits,
	.ft_perf_counters = a3xx_ft_perf_counters,
	.ft_perf_counters_count = ARRAY_SIZE(a3xx_ft_perf_counters),
	.perfcounters = &a3xx_perfcounters,
	.irq = &a3xx_irq,
	.irq_trace = trace_kgsl_a3xx_irq_status,
	.snapshot_data = &a3xx_snapshot_data,
	.num_prio_levels = 1,
	.vbif_xin_halt_ctrl0_mask = A3XX_VBIF_XIN_HALT_CTRL0_MASK,
	.platform_setup = a3xx_platform_setup,
	.rb_start = a3xx_rb_start,
	.init = a3xx_init,
	.microcode_read = a3xx_microcode_read,
	.perfcounter_init = a3xx_perfcounter_init,
	.perfcounter_close = a3xx_perfcounter_close,
	.start = a3xx_start,
	.snapshot = a3xx_snapshot,
	.coresight = &a3xx_coresight,
};
