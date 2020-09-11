/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mdla_debug.h"

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "mdla_hw_reg.h"
#include "mdla_decoder.h"

static const char *mreg_str_exe[16] = {
	"none",
	"CNVL",
	"SBL",
	"CONV",
	"ELW_BN",
	"ELW_ACTI",
	"POOL",
	"STORE",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-"
};

// CNVL(1): 0 None, 1: conv_preload, 2: Used in fused
static const char *mreg_str_cbl[16] = {
	"none",
	"conv_preload",
	"fused",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-"
};

// SBL(2): 0: None, 1: Load 1-tensor or DMA, 2: Load 2-tensor
static const char *mreg_str_sbl[16] = {
	"none",
	"load 1-tensor/DMA",
	"load 2-tensor",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-"
};

// CONV(3): 0: None, 1: conv2D, 2:DW, 3 FC, 4: dilated; 5:De-convolution
static const char *mreg_str_conv[16] = {
	"none",
	"conv2D",
	"DW",
	"FC",
	"dilated",
	"De-convolution",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-"
};


// ELW_BN(4): 0: None, 1: BN_Add, 2: BN_Mul, 3: BN_Mul+Add,
//            4: EW_Abs, 5: EW_Add, 6: EW_Sub12, 7: EW_Sub21,
//            8: EW_Mul, 9: EW_Max, 10: EW_Min
static const char *mreg_str_elw[16] = {
	"none",
	"BN_Add",
	"BN_Mul",
	"BN_Mul+Add",
	"EW_Abs",
	"EW_Add",
	"EW_Sub12",
	"EW_Sub21",
	"EW_Mul",
	"EW_Max",
	"EW_Min",
	"-",
	"-",
	"-",
	"-",
	"-"
};

// ELW_ACTI(5): 0: None, 1: sigmoid, 2: tanh, 4: Relu1,
//              5: Relu6, 6: Relu, 7: Prelu, 8: Elu
static const char *mreg_str_atci[16] = {
	"none",
	"sigmoid",
	"tanh",
	"Relu1",
	"Relu6",
	"Relu",
	"Prelu",
	"Elu",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-"
};

// POOL(6): 0: None, 1: AVG, 2: L2, 3: MAX, 4:MIN, 5: Bilinear
static const char *mreg_str_pool[16] = {
	"none",
	"AVG",
	"L2",
	"MAX",
	"MIN",
	"Bilinear",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-"
};

// STORE(7): 0:None, 1: store to external memory
static const char *mreg_str_ste[16] = {
	"none",
	"external",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-",
	"-"
};

static const char **cmd_map[8] = {
	mreg_str_exe,  // used as dummy
	mreg_str_cbl,
	mreg_str_sbl,
	mreg_str_conv,
	mreg_str_elw,
	mreg_str_atci,
	mreg_str_pool,
	mreg_str_ste
};

static void
mdla_cmd_parse(u32 swid, u32 exec, int func_map[8], char *str, int size)
{
	int i;
	u8 cmd;
	char *ptr = str;
	int remain = size;
	int out;

	mdla_cmd_debug("%s: %xh: cbl=%d sbl=%d conv=%d elw=%d acti=%d pool=%d ste=%d\n",
		__func__,
		exec,
		func_map[1],
		func_map[2],
		func_map[3],
		func_map[4],
		func_map[5],
		func_map[6],
		func_map[7]);

	out = snprintf(ptr, remain, "mdla_cmd_id:%u,", swid);
	remain -= out;
	ptr += out;

	for (i = 0; i < 8; i++) {
		cmd = (exec & 0xF);
		if (!cmd)
			break;

		out = snprintf(ptr, remain, "%s%d:%s(%d:%s)",
			(i) ? "->" : "",
			cmd,
			mreg_str_exe[cmd],
			func_map[cmd],
			cmd_map[cmd][func_map[cmd]]);

		if (!out)
			break;

		remain -= out;
		ptr += out;

		exec = exec >> 4;
	}
}

void mdla_decode(const char *cmd, char *str, int size)
{
	#define _VAL32(offset) (*((const u32 *)(cmd+offset)))
	#define _VAL4(offset)  (_VAL32(offset) & 0xF)

	int func_map[8] = { 0,
		_VAL4(MREG_CMD_CBL_FUNC),
		_VAL4(MREG_CMD_SBL_FUNC),
		_VAL4(MREG_CMD_CONV_FUNC),
		_VAL4(MREG_CMD_ELW_FUNC),
		_VAL4(MREG_CMD_ACTI_FUNC),
		_VAL4(MREG_CMD_POOL_FUNC_0),
		_VAL4(MREG_CMD_STE_FUNC)};

	mdla_cmd_parse(
		_VAL32(MREG_CMD_SWCMD_ID),
		_VAL32(MREG_CMD_EXE_FLOW),
		func_map,
		str,
		size);

	#undef _VAL
}

