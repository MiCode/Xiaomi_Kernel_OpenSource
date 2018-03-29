/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <asm/io.h>
#include <linux/string.h>
#include "mt_smi.h"
#include "smi_configuration.h"
#include "smi_common.h"
#include "smi_reg.h"

/* add static after all platform setting parameters moved to here */


#define SMI_LARB_NUM_MAX 8

#if defined(SMI_D1)
unsigned int smi_dbg_disp_mask = 1 << 0;
unsigned int smi_dbg_vdec_mask = 1 << 1;
unsigned int smi_dbg_imgsys_mask = 1 << 2;
unsigned int smi_dbg_venc_mask = 1 << 3;
unsigned int smi_dbg_mjc_mask = 0;

unsigned long smi_common_l1arb_offset[SMI_LARB_NR] = {
	REG_OFFSET_SMI_L1ARB0, REG_OFFSET_SMI_L1ARB1, REG_OFFSET_SMI_L1ARB2, REG_OFFSET_SMI_L1ARB3
};


struct SMI_CLK_INFO smi_clk_info[SMI_CLK_CNT] = {
	{"MTCMOS", 0, 0x060C}, {"DISP", 0, 0}, {"VDEC", 0, 0x8}, {"ISP", 0, 0}, {"VENC", 0, 0},
	{"", 0, 0}, {"", 0, 0}, {"", 0, 0}
};
unsigned long smi_larb0_debug_offset[SMI_LARB0_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb1_debug_offset[SMI_LARB1_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb2_debug_offset[SMI_LARB2_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb3_debug_offset[SMI_LARB3_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_common_debug_offset[SMI_COMMON_DEBUG_OFFSET_NUM] = {
	0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x220, 0x230, 0x234, 0x238, 0x300, 0x400, 0x404, 0x408,
	0x40C, 0x430, 0x440
};

int smi_larb_debug_offset_num[SMI_LARB_NR] = {
	SMI_LARB0_DEBUG_OFFSET_NUM, SMI_LARB1_DEBUG_OFFSET_NUM, SMI_LARB2_DEBUG_OFFSET_NUM,
	SMI_LARB3_DEBUG_OFFSET_NUM
};

unsigned long *smi_larb_debug_offset[SMI_LARB_NR] = {
	smi_larb0_debug_offset, smi_larb1_debug_offset, smi_larb2_debug_offset,
	smi_larb3_debug_offset
};

unsigned int smi_restore_num[SMI_LARB_NR];
struct SMI_SETTING_VALUE *smi_larb_restore[SMI_LARB_NR];

#define SMI_PROFILE_SETTING_COMMON_INIT_NUM 7

#define SMI_INITSETTING_LARB0_NUM (SMI_LARB0_PORT_NUM + 1) /* add vc setting */
#define SMI_INITSETTING_LARB1_NUM (SMI_LARB1_PORT_NUM + 1) /* add vc setting */
#define SMI_INITSETTING_LARB2_NUM (SMI_LARB2_PORT_NUM + 2) /* add vc&isp_hrt setting */
#define SMI_INITSETTING_LARB3_NUM (SMI_LARB3_PORT_NUM + 1) /* add vc setting */

/* vc setting */
struct SMI_SETTING_VALUE smi_vc_setting[SMI_VC_SETTING_NUM] = {
	{0x20, 0}, {0x20, 2}, {0x20, 1}, {0x20, 1}
};
/* init_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_init[SMI_PROFILE_SETTING_COMMON_INIT_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0}, {REG_OFFSET_SMI_L1ARB1, 0x1000},
	{REG_OFFSET_SMI_L1ARB2, 0x1000}, {REG_OFFSET_SMI_L1ARB3, 0x1000},
	{0x100, 0x1b},
	{0x234, (0x1 << 31) + (0x1d << 26) + (0x1f << 21) + (0x0 << 20) + (0x3 << 15)
	 + (0x4 << 10) + (0x4 << 5) + 0x5},
	{0x230, 0x1f + (0x8 << 4) + (0x7 << 9)}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_init[SMI_INITSETTING_LARB0_NUM] = {
	{0x20, 0}, {0x200, 0x1f}, {0x204, 4}, {0x208, 6}, {0x20c, 0x1f}, {0x210, 4}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_init[SMI_INITSETTING_LARB1_NUM] = {
	{0x20, 2}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_init[SMI_INITSETTING_LARB2_NUM] = {
	{0x20, 1}, {0x24, 0x370246}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1},
	{0x21c, 1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_init[SMI_INITSETTING_LARB3_NUM] = {
	{0x20, 1}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING init_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM, SMI_INITSETTING_LARB3_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init, smi_profile_setting_larb3_init}
};

#define SMI_PROFILE_SETTING_COMMON_VR_NUM SMI_LARB_NR
/* vr_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_vr[SMI_PROFILE_SETTING_COMMON_VR_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x11F1}, {REG_OFFSET_SMI_L1ARB1, 0x1000},
	{REG_OFFSET_SMI_L1ARB2, 0x120A}, {REG_OFFSET_SMI_L1ARB3, 0x11F3}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vr[SMI_LARB0_PORT_NUM] = {
	{0x200, 8}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 2}, {0x218, 4}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vr[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vr[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 4}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 4}, {0x22c, 1}, {0x230, 2}, {0x234, 2}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vr[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 2}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 2}, {0x228, 1}, {0x22c, 3}, {0x230, 2}
};

struct SMI_SETTING vr_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

#define SMI_PROFILE_SETTING_COMMON_VP_NUM SMI_LARB_NR
/* vp_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_vp[SMI_PROFILE_SETTING_COMMON_VP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x1262}, {REG_OFFSET_SMI_L1ARB1, 0x11E9},
	{REG_OFFSET_SMI_L1ARB2, 0x1000}, {REG_OFFSET_SMI_L1ARB3, 0x123D}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vp[SMI_LARB0_PORT_NUM] = {
	{0x200, 8}, {0x204, 1}, {0x208, 2}, {0x20c, 1}, {0x210, 3}, {0x214, 1}, {0x218, 4}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vp[SMI_LARB1_PORT_NUM] = {
	{0x200, 0xb}, {0x204, 0xe}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vp[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vp[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 2}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 3}, {0x230, 2}
};

struct SMI_SETTING vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp}
};

/* vr series */
struct SMI_SETTING icfp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

struct SMI_SETTING vr_slow_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

struct SMI_SETTING venc_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

/* vp series */
struct SMI_SETTING vpwfd_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp}
};

struct SMI_SETTING swdec_vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp}
};

/* init series */
struct SMI_SETTING mm_gpu_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM, SMI_INITSETTING_LARB3_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init, smi_profile_setting_larb3_init}
};

struct SMI_SETTING ui_idle_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi4k_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vss_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING n3d_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vpmjc_setting_config = { 0, NULL, {0}, {0} };

#elif defined(SMI_D3)
unsigned int smi_dbg_disp_mask = 1 << 0;
unsigned int smi_dbg_vdec_mask = 1 << 1;
unsigned int smi_dbg_imgsys_mask = 1 << 2;
unsigned int smi_dbg_venc_mask = 1 << 3;
unsigned int smi_dbg_mjc_mask = 0;

unsigned long smi_common_l1arb_offset[SMI_LARB_NR] = {
	REG_OFFSET_SMI_L1ARB0, REG_OFFSET_SMI_L1ARB1, REG_OFFSET_SMI_L1ARB2, REG_OFFSET_SMI_L1ARB3
};

struct SMI_CLK_INFO smi_clk_info[SMI_CLK_CNT] = {
	{"MTCMOS", 0, 0x060C}, {"DISP", 0, 0}, {"VDEC", 0, 0x8}, {"ISP", 0, 0}, {"VENC", 0, 0},
	{"", 0, 0}, {"", 0, 0}, {"", 0, 0}
};

unsigned long smi_larb0_debug_offset[SMI_LARB0_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb1_debug_offset[SMI_LARB1_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb2_debug_offset[SMI_LARB2_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb3_debug_offset[SMI_LARB3_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_common_debug_offset[SMI_COMMON_DEBUG_OFFSET_NUM] = {
	0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x220, 0x230, 0x234, 0x238, 0x300, 0x400, 0x404, 0x408,
	0x40C, 0x430, 0x440
};

int smi_larb_debug_offset_num[SMI_LARB_NR] = {
	SMI_LARB0_DEBUG_OFFSET_NUM, SMI_LARB1_DEBUG_OFFSET_NUM, SMI_LARB2_DEBUG_OFFSET_NUM,
	SMI_LARB3_DEBUG_OFFSET_NUM
};

unsigned long *smi_larb_debug_offset[SMI_LARB_NR] = {
	smi_larb0_debug_offset, smi_larb1_debug_offset, smi_larb2_debug_offset,
	smi_larb3_debug_offset
};

unsigned int smi_restore_num[SMI_LARB_NR];
struct SMI_SETTING_VALUE *smi_larb_restore[SMI_LARB_NR];

#define SMI_PROFILE_SETTING_COMMON_INIT_NUM 7

#define SMI_INITSETTING_LARB0_NUM (SMI_LARB0_PORT_NUM + 1) /* add vc setting */
#define SMI_INITSETTING_LARB1_NUM (SMI_LARB1_PORT_NUM + 1) /* add vc setting */
#define SMI_INITSETTING_LARB2_NUM (SMI_LARB2_PORT_NUM + 2) /* add vc&isp_hrt setting */
#define SMI_INITSETTING_LARB3_NUM (SMI_LARB3_PORT_NUM + 1) /* add vc setting */

/* vc setting */
struct SMI_SETTING_VALUE smi_vc_setting[SMI_VC_SETTING_NUM] = {
	{0x20, 0}, {0x20, 2}, {0x20, 1}, {0x20, 1}
};

/* init_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_init[SMI_PROFILE_SETTING_COMMON_INIT_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0}, {REG_OFFSET_SMI_L1ARB1, 0x1000},
	{REG_OFFSET_SMI_L1ARB2, 0x1000}, {REG_OFFSET_SMI_L1ARB3, 0x1000},
	{0x100, 0xb},
	{0x234, (0x1 << 31) + (0x1d << 26) + (0x1f << 21) + (0x0 << 20) + (0x3 << 15)
	 + (0x4 << 10) + (0x4 << 5) + 0x5},
	{0x230, 0xf + (0x8 << 4) + (0x7 << 9)}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_init[SMI_INITSETTING_LARB0_NUM] = {
	{0x20, 0}, {0x200, 0x1f}, {0x204, 8}, {0x208, 6}, {0x20c, 0x1f}, {0x210, 4}, {0x214, 1}, {0x218, 1},
	    {0x21c, 2},
	{0x220, 1}, {0x224, 3}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_init[SMI_INITSETTING_LARB1_NUM] = {
	{0x20, 2}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_init[SMI_INITSETTING_LARB2_NUM] = {
	{0x20, 1}, {0x24, 0x370246}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1},
	{0x21c, 1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_init[SMI_INITSETTING_LARB3_NUM] = {
	{0x20, 1}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING init_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM, SMI_INITSETTING_LARB3_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init, smi_profile_setting_larb3_init}
};

#define SMI_PROFILE_SETTING_COMMON_VR_NUM SMI_LARB_NR
/* vr_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_vr[SMI_PROFILE_SETTING_COMMON_VR_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x1417}, {REG_OFFSET_SMI_L1ARB1, 0x1000},
	{REG_OFFSET_SMI_L1ARB2, 0x11D0}, {REG_OFFSET_SMI_L1ARB3, 0x11F8}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vr[SMI_LARB0_PORT_NUM] = {
	{0x200, 0xa}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1},
	    {0x21c, 4},
	{0x220, 1}, {0x224, 6}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vr[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vr[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 2}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 2}, {0x21c,
											     1},
	{0x220, 2}, {0x224, 1}, {0x228, 1}, {0x22c, 8}, {0x230, 1}, {0x234, 1}, {0x238, 2}, {0x23c,
											     2},
	{0x240, 2}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vr[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 2}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 2}, {0x228, 1}, {0x22c, 3}, {0x230, 2}
};

struct SMI_SETTING vr_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

#define SMI_PROFILE_SETTING_COMMON_VP_NUM SMI_LARB_NR
/* vp_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_vp[SMI_PROFILE_SETTING_COMMON_VP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x1262}, {REG_OFFSET_SMI_L1ARB1, 0x11E9},
	{REG_OFFSET_SMI_L1ARB2, 0x1000}, {REG_OFFSET_SMI_L1ARB3, 0x123D}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vp[SMI_LARB0_PORT_NUM] = {
	{0x200, 8}, {0x204, 1}, {0x208, 2}, {0x20c, 1}, {0x210, 3}, {0x214, 1}, {0x218, 4}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vp[SMI_LARB1_PORT_NUM] = {
	{0x200, 0xb}, {0x204, 0xe}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vp[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vp[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 2}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 3}, {0x230, 2}
};

struct SMI_SETTING vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp}
};

#define SMI_PROFILE_SETTING_COMMON_VPWFD_NUM SMI_LARB_NR
/* vpwfd_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_vpwfd[SMI_PROFILE_SETTING_COMMON_VPWFD_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x14B6}, {REG_OFFSET_SMI_L1ARB1, 0x11EE},
	{REG_OFFSET_SMI_L1ARB2, 0x1000}, {REG_OFFSET_SMI_L1ARB3, 0x11F2}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vpwfd[SMI_LARB0_PORT_NUM] = {
	{0x200, 0xc}, {0x204, 8}, {0x208, 6}, {0x20c, 0xc}, {0x210, 4}, {0x214, 1}, {0x218, 1},
	    {0x21c, 3},
	{0x220, 2}, {0x224, 5}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vpwfd[SMI_LARB1_PORT_NUM] = {
	{0x200, 0xb}, {0x204, 0xe}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vpwfd[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vpwfd[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 2}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 2}, {0x228, 1}, {0x22c, 3}, {0x230, 2}
};

struct SMI_SETTING vpwfd_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VPWFD_NUM, smi_profile_setting_common_vpwfd,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vpwfd, smi_profile_setting_larb1_vpwfd,
	 smi_profile_setting_larb2_vpwfd, smi_profile_setting_larb3_vpwfd}
};

#define SMI_PROFILE_SETTING_COMMON_ICFP_NUM SMI_LARB_NR
/* icfp_setting */

struct SMI_SETTING_VALUE smi_profile_setting_common_icfp[SMI_PROFILE_SETTING_COMMON_ICFP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x14E2}, {REG_OFFSET_SMI_L1ARB1, 0x1000},
	{REG_OFFSET_SMI_L1ARB2, 0x1310}, {REG_OFFSET_SMI_L1ARB3, 0x106F}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_icfp[SMI_LARB0_PORT_NUM] = {
	{0x200, 0xe}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1},
	    {0x21c, 2},
	{0x220, 2}, {0x224, 3}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_icfp[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_icfp[SMI_LARB2_PORT_NUM] = {
	{0x200, 0xc}, {0x204, 4}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1},
	    {0x21c, 1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 3}, {0x230, 1}, {0x234, 1}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_icfp[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING icfp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_ICFP_NUM, smi_profile_setting_common_icfp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_icfp, smi_profile_setting_larb1_icfp,
	 smi_profile_setting_larb2_icfp, smi_profile_setting_larb3_icfp}
};

/* vr series */
struct SMI_SETTING vr_slow_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

struct SMI_SETTING venc_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

/* vp series */
struct SMI_SETTING swdec_vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp}
};

/* init seris */
struct SMI_SETTING mm_gpu_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM, SMI_INITSETTING_LARB3_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init, smi_profile_setting_larb3_init}
};

struct SMI_SETTING vss_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING ui_idle_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi4k_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING n3d_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vpmjc_setting_config = { 0, NULL, {0}, {0} };

#elif defined(SMI_J)
unsigned int smi_dbg_disp_mask = 1 << 0;
unsigned int smi_dbg_vdec_mask = 1 << 1;
unsigned int smi_dbg_imgsys_mask = 1 << 2;
unsigned int smi_dbg_venc_mask = 1 << 3;
unsigned int smi_dbg_mjc_mask = 0;

unsigned long smi_common_l1arb_offset[SMI_LARB_NR] = {
	REG_OFFSET_SMI_L1ARB0, REG_OFFSET_SMI_L1ARB1, REG_OFFSET_SMI_L1ARB2, REG_OFFSET_SMI_L1ARB3
};

struct SMI_CLK_INFO smi_clk_info[SMI_CLK_CNT] = {
	{"MTCMOS", 0, 0x060C}, {"DISP", 0, 0}, {"VDEC", 0, 0x8}, {"ISP", 0, 0}, {"VENC", 0, 0},
	{"", 0, 0}, {"", 0, 0}, {"", 0, 0}
};

unsigned long smi_larb0_debug_offset[SMI_LARB0_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb1_debug_offset[SMI_LARB1_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb2_debug_offset[SMI_LARB2_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb3_debug_offset[SMI_LARB3_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_common_debug_offset[SMI_COMMON_DEBUG_OFFSET_NUM] = {
	0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x220, 0x230, 0x234, 0x238, 0x300, 0x400, 0x404, 0x408,
	0x40C, 0x430, 0x440
};

int smi_larb_debug_offset_num[SMI_LARB_NR] = {
	SMI_LARB0_DEBUG_OFFSET_NUM, SMI_LARB1_DEBUG_OFFSET_NUM, SMI_LARB2_DEBUG_OFFSET_NUM,
	SMI_LARB3_DEBUG_OFFSET_NUM
};

unsigned long *smi_larb_debug_offset[SMI_LARB_NR] = {
	smi_larb0_debug_offset, smi_larb1_debug_offset, smi_larb2_debug_offset,
	smi_larb3_debug_offset
};

unsigned int smi_restore_num[SMI_LARB_NR];
struct SMI_SETTING_VALUE *smi_larb_restore[SMI_LARB_NR];

#define SMI_PROFILE_SETTING_COMMON_INIT_NUM 8

#define SMI_INITSETTING_LARB0_NUM (SMI_LARB0_PORT_NUM + 7) /* add ui_critical/vc/cmd throttle setting */
#define SMI_INITSETTING_LARB1_NUM (SMI_LARB1_PORT_NUM + 3) /* add vc/cmd throttle setting */
#define SMI_INITSETTING_LARB2_NUM (SMI_LARB2_PORT_NUM + 3) /* add vc/cmd throttle setting */
#define SMI_INITSETTING_LARB3_NUM (SMI_LARB3_PORT_NUM + 3) /* add vc/cmd throttle setting */

/* vc setting */
struct SMI_SETTING_VALUE smi_vc_setting[SMI_VC_SETTING_NUM] = {
	{0x20, 0}, {0x20, 2}, {0x20, 1}, {0x20, 1}
};

/* init_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_init[SMI_PROFILE_SETTING_COMMON_INIT_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x15AE}, {REG_OFFSET_SMI_L1ARB1, 0x1000},
	{REG_OFFSET_SMI_L1ARB2, 0x1000}, {REG_OFFSET_SMI_L1ARB3, 0x1000},
	{0x100, 0xb},
	{0x234, ((0x1 << 31) + (0x1d << 26) + (0x1f << 21) + (0x0 << 20) + (0x3 << 15)
		 + (0x4 << 10) + (0x4 << 5) + 0x5)},
	{0x230, 0xf + (0x8 << 4) + (0x7 << 9)},
	{0x300, 0x1 + (0x78 << 1) + (0x4 << 8)}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_init[SMI_INITSETTING_LARB0_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x20, 0}, {0x24, 0x370246},
	{0x200, 0x1f}, {0x204, 8}, {0x208, 6}, {0x20c, 0x1f}, {0x210, 4}, {0x214, 1}, {0x218, 0x1f},
	    {0x21c, 0x1f},
	{0x220, 2}, {0x224, 1}, {0x228, 3}, {0x100, 5}, {0x10c, 5}, {0x118, 5}, {0x11c, 5}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_init[SMI_INITSETTING_LARB1_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x20, 2}, {0x24, 0x370246}, {0x200, 1}, {0x204, 1},
	{0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_init[SMI_INITSETTING_LARB2_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x20, 1}, {0x24, 0x370246},
	{0x200, 1}, {0x204, 4}, {0x208, 2}, {0x20c, 2}, {0x210, 2}, {0x214, 1}, {0x218, 2}, {0x21c,
											     2},
	{0x220, 2}, {0x224, 1}, {0x228, 1}, {0x22c, 2}, {0x230, 1}, {0x234, 2}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_init[SMI_INITSETTING_LARB3_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x20, 1}, {0x24, 0x370246},
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING init_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM, SMI_INITSETTING_LARB3_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init, smi_profile_setting_larb3_init}
};

#define SMI_PROFILE_SETTING_COMMON_VR_NUM SMI_LARB_NR

/* vr_setting */

struct SMI_SETTING_VALUE smi_profile_setting_common_vr[SMI_PROFILE_SETTING_COMMON_VR_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x1393}, {REG_OFFSET_SMI_L1ARB1, 0x1000},
	{REG_OFFSET_SMI_L1ARB2, 0x1205}, {REG_OFFSET_SMI_L1ARB3, 0x11D4}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vr[SMI_LARB0_PORT_NUM] = {
	{0x200, 0xe}, {0x204, 8}, {0x208, 4}, {0x20c, 0xe}, {0x210, 4}, {0x214, 1}, {0x218, 0xe},
	    {0x21c, 0xe},
	{0x220, 2}, {0x224, 1}, {0x228, 2}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vr[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vr[SMI_LARB2_PORT_NUM] = {
	{0x200, 0x12}, {0x204, 6}, {0x208, 4}, {0x20c, 2}, {0x210, 4}, {0x214, 1}, {0x218, 4}, {0x21c,
											     2},
	{0x220, 2}, {0x224, 1}, {0x228, 1}, {0x22c, 2}, {0x230, 1}, {0x234, 2}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vr[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 2}, {0x228, 1}, {0x22c, 1}, {0x230, 4}
};

struct SMI_SETTING vr_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

#define SMI_PROFILE_SETTING_COMMON_VP_NUM SMI_LARB_NR

/* vp_setting */

struct SMI_SETTING_VALUE smi_profile_setting_common_vp[SMI_PROFILE_SETTING_COMMON_VP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x1510}, {REG_OFFSET_SMI_L1ARB1, 0x1169},
	{REG_OFFSET_SMI_L1ARB2, 0x1000}, {REG_OFFSET_SMI_L1ARB3, 0x11CE}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vp[SMI_LARB0_PORT_NUM] = {
	{0x200, 0xc}, {0x204, 8}, {0x208, 4}, {0x20c, 0xc}, {0x210, 4}, {0x214, 2}, {0x218, 0xc},
	    {0x21c, 0xc},
	{0x220, 2}, {0x224, 1}, {0x228, 3}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vp[SMI_LARB1_PORT_NUM] = {
	{0x200, 5}, {0x204, 2}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vp[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}, {0x23c,
											     1},
	{0x240, 1}, {0x244, 1}, {0x248, 1}, {0x24c, 1}, {0x250, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vp[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 2}, {0x228, 1}, {0x22c, 1}, {0x230, 4}
};

struct SMI_SETTING vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp}
};

/* vr series */
struct SMI_SETTING icfp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

struct SMI_SETTING vr_slow_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

struct SMI_SETTING vss_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

struct SMI_SETTING venc_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr}
};

/* vp series */
struct SMI_SETTING vpwfd_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp}
};

struct SMI_SETTING swdec_vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp}
};

/* init seris */
struct SMI_SETTING mm_gpu_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init, smi_profile_setting_larb3_init}
};

struct SMI_SETTING ui_idle_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi4k_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING n3d_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vpmjc_setting_config = { 0, NULL, {0}, {0} };

#elif defined(SMI_D2)
unsigned int smi_dbg_disp_mask = 1 << 0;
unsigned int smi_dbg_vdec_mask = 1 << 1;
unsigned int smi_dbg_imgsys_mask = 1 << 2;
unsigned int smi_dbg_venc_mask = 1 << 2;
unsigned int smi_dbg_mjc_mask = 0;

unsigned long smi_common_l1arb_offset[SMI_LARB_NR] = {
	REG_OFFSET_SMI_L1ARB0, REG_OFFSET_SMI_L1ARB1, REG_OFFSET_SMI_L1ARB2
};

struct SMI_CLK_INFO smi_clk_info[SMI_CLK_CNT] = {
	{"MTCMOS", 0, 0x060C}, {"DISP", 0, 0}, {"VDEC", 0, 0x8}, {"ISP", 0, 0}, {"VENC", 0, 0},
	{"", 0, 0}, {"", 0, 0}, {"", 0, 0}
};

unsigned long smi_larb0_debug_offset[SMI_LARB0_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb1_debug_offset[SMI_LARB1_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb2_debug_offset[SMI_LARB2_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_common_debug_offset[SMI_COMMON_DEBUG_OFFSET_NUM] = {
	0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x220, 0x230, 0x234, 0x238, 0x300, 0x400, 0x404, 0x408,
	0x40C, 0x430, 0x440
};

int smi_larb_debug_offset_num[SMI_LARB_NR] = {
	SMI_LARB0_DEBUG_OFFSET_NUM, SMI_LARB1_DEBUG_OFFSET_NUM, SMI_LARB2_DEBUG_OFFSET_NUM
};

unsigned long *smi_larb_debug_offset[SMI_LARB_NR] = {
	smi_larb0_debug_offset, smi_larb1_debug_offset, smi_larb2_debug_offset
};

unsigned int smi_restore_num[SMI_LARB_NR];
struct SMI_SETTING_VALUE *smi_larb_restore[SMI_LARB_NR];

#define SMI_PROFILE_SETTING_COMMON_INIT_NUM 6
#define SMI_VC_SETTING_INDEX 0

#define SMI_INITSETTING_LARB0_NUM (SMI_LARB0_PORT_NUM + 1) /* add vc setting */
#define SMI_INITSETTING_LARB1_NUM (SMI_LARB1_PORT_NUM + 1) /* add vc setting */
#define SMI_INITSETTING_LARB2_NUM (SMI_LARB2_PORT_NUM + 1) /* add vc setting */

/* vc setting */
struct SMI_SETTING_VALUE smi_vc_setting[SMI_VC_SETTING_NUM] = {
	{0x20, 0}, {0x20, 2}, {0x20, 1}
};


/* init_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_init[SMI_PROFILE_SETTING_COMMON_INIT_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0}, {REG_OFFSET_SMI_L1ARB1, 0}, {REG_OFFSET_SMI_L1ARB2, 0},
	{0x100, 0xb},
	{0x234, (0x1 << 31) + (0x1d << 26) + (0x1f << 21) + (0x0 << 20) + (0x3 << 15)
	 + (0x4 << 10) + (0x4 << 5) + 0x5},
	{0x230, (0x7 + (0x8 << 3) + (0x7 << 8))}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_init[SMI_INITSETTING_LARB0_NUM] = {
	{0x20, 0}, {0x200, 0x1f}, {0x204, 0x1f}, {0x208, 4}, {0x20c, 6}, {0x210, 4}, {0x214, 1}, {0x218, 1},
	    {0x21c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_init[SMI_INITSETTING_LARB1_NUM] = {
	{0x20, 2}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_init[SMI_INITSETTING_LARB2_NUM] = {
	{0x20, 1}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING init_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init}
};

#define SMI_PROFILE_SETTING_COMMON_ICFP_NUM SMI_LARB_NR
/* icfp_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_icfp[SMI_PROFILE_SETTING_COMMON_ICFP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x11da}, {REG_OFFSET_SMI_L1ARB1, 0x1000}, {REG_OFFSET_SMI_L1ARB2, 0x1318}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_icfp[SMI_LARB0_PORT_NUM] = {
	{0x200, 6}, {0x204, 6}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_icfp[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_icfp[SMI_LARB2_PORT_NUM] = {
	{0x200, 8}, {0x204, 6}, {0x208, 1}, {0x20c, 1}, {0x210, 2}, {0x214, 4}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING icfp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_ICFP_NUM, smi_profile_setting_common_icfp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_icfp, smi_profile_setting_larb1_icfp,
	 smi_profile_setting_larb2_icfp}
};

#define SMI_PROFILE_SETTING_COMMON_VR_NUM SMI_LARB_NR
/* vr_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_vr[SMI_PROFILE_SETTING_COMMON_VR_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x11ff}, {REG_OFFSET_SMI_L1ARB1, 0x1000}, {REG_OFFSET_SMI_L1ARB2, 0x1361}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vr[SMI_LARB0_PORT_NUM] = {
	{0x200, 6}, {0x204, 6}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vr[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vr[SMI_LARB2_PORT_NUM] = {
	{0x200, 8}, {0x204, 6}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 4}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 2}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING vr_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr}
};

#define SMI_PROFILE_SETTING_COMMON_VP_NUM SMI_LARB_NR
/* vp_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_vp[SMI_PROFILE_SETTING_COMMON_VP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x11ff}, {REG_OFFSET_SMI_L1ARB1, 0}, {REG_OFFSET_SMI_L1ARB2, 0x1361}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vp[SMI_LARB0_PORT_NUM] = {
	{0x200, 8}, {0x204, 8}, {0x208, 1}, {0x20c, 1}, {0x210, 3}, {0x214, 1}, {0x218, 4}, {0x21c,
											     1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vp[SMI_LARB1_PORT_NUM] = {
	{0x200, 0xb}, {0x204, 0xe}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vp[SMI_LARB2_PORT_NUM] = {
	{0x200, 8}, {0x204, 6}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 4}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 2}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp}
};

#define SMI_PROFILE_SETTING_COMMON_VPWFD_NUM SMI_LARB_NR
/* vfd_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_vpwfd[SMI_PROFILE_SETTING_COMMON_VPWFD_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x11ff}, {REG_OFFSET_SMI_L1ARB1, 0}, {REG_OFFSET_SMI_L1ARB2, 0x1361}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vpwfd[SMI_LARB0_PORT_NUM] = {
	{0x200, 8}, {0x204, 8}, {0x208, 1}, {0x20c, 1}, {0x210, 3}, {0x214, 1}, {0x218, 4}, {0x21c,
											     1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vpwfd[SMI_LARB1_PORT_NUM] = {
	{0x200, 0xb}, {0x204, 0xe}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vpwfd[SMI_LARB2_PORT_NUM] = {
	{0x200, 8}, {0x204, 6}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 4}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 2}, {0x22c, 1}, {0x230, 1}
};

struct SMI_SETTING vpwfd_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VPWFD_NUM, smi_profile_setting_common_vpwfd,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_vpwfd, smi_profile_setting_larb1_vpwfd,
	 smi_profile_setting_larb2_vpwfd}
};

/* vp series */
struct SMI_SETTING swdec_vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp}
};

/* vr series */
struct SMI_SETTING vr_slow_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr}
};

struct SMI_SETTING venc_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr}
};

/* init series */
struct SMI_SETTING mm_gpu_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init}
};
struct SMI_SETTING vss_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING ui_idle_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi4k_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING n3d_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vpmjc_setting_config = { 0, NULL, {0}, {0} };

#elif defined(SMI_R)
unsigned int smi_dbg_disp_mask = 1 << 0;
unsigned int smi_dbg_vdec_mask = 0;
unsigned int smi_dbg_imgsys_mask = 1 << 1;
unsigned int smi_dbg_venc_mask = 1 << 1;
unsigned int smi_dbg_mjc_mask = 0;

unsigned long smi_common_l1arb_offset[SMI_LARB_NR] = {
	REG_OFFSET_SMI_L1ARB0, REG_OFFSET_SMI_L1ARB1
};

struct SMI_CLK_INFO smi_clk_info[SMI_CLK_CNT] = {
	{"MTCMOS", 0, 0x060C}, {"DISP", 0, 0}, {"VDEC", 0, 0x8}, {"ISP", 0, 0}, {"VENC", 0, 0},
	{"", 0, 0}, {"", 0, 0}, {"", 0, 0}
};

unsigned long smi_larb0_debug_offset[SMI_LARB0_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb1_debug_offset[SMI_LARB1_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_common_debug_offset[SMI_COMMON_DEBUG_OFFSET_NUM] = {
	0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x220, 0x230, 0x234, 0x238, 0x300, 0x400, 0x404, 0x408,
	0x40C, 0x430, 0x440
};

int smi_larb_debug_offset_num[SMI_LARB_NR] = {
	SMI_LARB0_DEBUG_OFFSET_NUM, SMI_LARB1_DEBUG_OFFSET_NUM
};

unsigned long *smi_larb_debug_offset[SMI_LARB_NR] = {
	smi_larb0_debug_offset, smi_larb1_debug_offset
};

unsigned int smi_restore_num[SMI_LARB_NR];
struct SMI_SETTING_VALUE *smi_larb_restore[SMI_LARB_NR];

#define SMI_PROFILE_SETTING_COMMON_INIT_NUM 5
#define SMI_VC_SETTING_INDEX 0

#define SMI_INITSETTING_LARB0_NUM (SMI_LARB0_PORT_NUM + 1) /* add vc setting */
#define SMI_INITSETTING_LARB1_NUM (SMI_LARB1_PORT_NUM + 1) /* add vc setting */

/* vc setting */
struct SMI_SETTING_VALUE smi_vc_setting[SMI_VC_SETTING_NUM] = {
	{0x20, 0}, {0x20, 2}
};

/* init_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_init[SMI_PROFILE_SETTING_COMMON_INIT_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x14cb}, {REG_OFFSET_SMI_L1ARB1, 0x1001},
	{0x100, 0xb},
	{0x234,
	 (0x1 << 31) + (0x1d << 26) + (0x1f << 21) + (0x0 << 20) + (0x3 << 15)
	 + (0x4 << 10) + (0x4 << 5) + 0x5},
	{0x230, (0x3 + (0x8 << 2) + (0x7 << 7))}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_init[SMI_INITSETTING_LARB0_NUM] = {
	{0x20, 0}, {0x200, 0x1c}, {0x204, 4}, {0x208, 6}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_init[SMI_INITSETTING_LARB1_NUM] = {
	{0x20, 2}, {0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}
};

struct SMI_SETTING init_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init}
};

#define SMI_PROFILE_SETTING_COMMON_VR_NUM SMI_LARB_NR
/* vr_setting */

struct SMI_SETTING_VALUE smi_profile_setting_common_vr[SMI_PROFILE_SETTING_COMMON_VR_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x122b}, {REG_OFFSET_SMI_L1ARB1, 0x142c}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vr[SMI_LARB0_PORT_NUM] = {
	{0x200, 0xa}, {0x204, 1}, {0x208, 1}, {0x20c, 4}, {0x210, 2}, {0x214, 2}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vr[SMI_LARB1_PORT_NUM] = {
	{0x200, 8}, {0x204, 6}, {0x208, 1}, {0x20c, 1}, {0x210, 4}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 3}, {0x224, 2}, {0x228, 2}
};

struct SMI_SETTING vr_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr}
};

#define SMI_PROFILE_SETTING_COMMON_VP_NUM SMI_LARB_NR
/* vp_setting */

struct SMI_SETTING_VALUE smi_profile_setting_common_vp[SMI_PROFILE_SETTING_COMMON_VP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x11ff}, {REG_OFFSET_SMI_L1ARB1, 0}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vp[SMI_LARB0_PORT_NUM] = {
	{0x200, 8}, {0x204, 1}, {0x208, 1}, {0x20c, 3}, {0x210, 1}, {0x214, 4}, {0x218, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vp[SMI_LARB1_PORT_NUM] = {
	{0x200, 8}, {0x204, 6}, {0x208, 1}, {0x20c, 1}, {0x210, 4}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 3}, {0x224, 2}, {0x228, 2}
};

struct SMI_SETTING vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp}
};

/* vp series */
struct SMI_SETTING swdec_vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp}
};

/* vr series */
struct SMI_SETTING vr_slow_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr}
};

struct SMI_SETTING icfp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr}
};

struct SMI_SETTING venc_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr}
};

/* init series */
struct SMI_SETTING mm_gpu_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init}
};

struct SMI_SETTING vss_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vpwfd_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING ui_idle_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi4k_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING n3d_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vpmjc_setting_config = { 0, NULL, {0}, {0} };

#elif defined(SMI_EV)
unsigned int smi_dbg_disp_mask = 0x21;
unsigned int smi_dbg_vdec_mask = 1 << 1;
unsigned int smi_dbg_imgsys_mask = 0x44;
unsigned int smi_dbg_venc_mask = 1 << 3;
unsigned int smi_dbg_mjc_mask = 1 << 4;

struct SMI_CLK_INFO smi_clk_info[SMI_CLK_CNT] = {
	{"MTCMOS", 0, 0x060C}, {"DISP", 0, 0}, {"VDEC", 0, 0x8}, {"ISP", 0, 0}, {"VENC", 0, 0},
	{"", 0, 0}, {"", 0, 0}, {"", 0, 0}
};

unsigned long smi_common_l1arb_offset[SMI_LARB_NR] = {
	REG_OFFSET_SMI_L1ARB0, REG_OFFSET_SMI_L1ARB1, REG_OFFSET_SMI_L1ARB2, REG_OFFSET_SMI_L1ARB3,
	REG_OFFSET_SMI_L1ARB4, REG_OFFSET_SMI_L1ARB5, REG_OFFSET_SMI_L1ARB6
};

unsigned long smi_larb0_debug_offset[SMI_LARB0_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb1_debug_offset[SMI_LARB1_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb2_debug_offset[SMI_LARB2_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb3_debug_offset[SMI_LARB3_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb4_debug_offset[SMI_LARB4_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb5_debug_offset[SMI_LARB5_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb6_debug_offset[SMI_LARB6_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x50, 0x60, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0,
	    0xc8,
	0xcc, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	    0x230,
	0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c, 0x280, 0x284, 0x288, 0x28c, 0x290, 0x294,
	    0x298,
	0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc,
	    0x2d0,
	0x2d4, 0x2d8, 0x2dc, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};
unsigned long smi_common_debug_offset[SMI_COMMON_DEBUG_OFFSET_NUM] = {
	0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x118, 0x11c, 0x220, 0x230, 0x234, 0x238, 0x300, 0x400, 0x404, 0x408,
	0x40C, 0x410, 0x414, 0x418, 0x430, 0x434, 0x440
};

int smi_larb_debug_offset_num[SMI_LARB_NR] = {
	SMI_LARB0_DEBUG_OFFSET_NUM, SMI_LARB1_DEBUG_OFFSET_NUM, SMI_LARB2_DEBUG_OFFSET_NUM,
	SMI_LARB3_DEBUG_OFFSET_NUM, SMI_LARB4_DEBUG_OFFSET_NUM, SMI_LARB5_DEBUG_OFFSET_NUM,
	SMI_LARB6_DEBUG_OFFSET_NUM
};

unsigned long *smi_larb_debug_offset[SMI_LARB_NR] = {
	smi_larb0_debug_offset, smi_larb1_debug_offset, smi_larb2_debug_offset,
	smi_larb3_debug_offset, smi_larb4_debug_offset, smi_larb5_debug_offset,
	smi_larb6_debug_offset
};

#define SMI_LARB0_RESTORE_NUM 5
#define SMI_LARB1_RESTORE_NUM 0
#define SMI_LARB2_RESTORE_NUM 0
#define SMI_LARB3_RESTORE_NUM 0
#define SMI_LARB4_RESTORE_NUM 0
#define SMI_LARB5_RESTORE_NUM 4
#define SMI_LARB6_RESTORE_NUM 0

unsigned int smi_restore_num[SMI_LARB_NR] = {
	SMI_LARB0_RESTORE_NUM, SMI_LARB1_RESTORE_NUM, SMI_LARB2_RESTORE_NUM, SMI_LARB3_RESTORE_NUM,
	SMI_LARB4_RESTORE_NUM, SMI_LARB5_RESTORE_NUM, SMI_LARB6_RESTORE_NUM
};
struct SMI_SETTING_VALUE smi_larb0_restore[SMI_LARB0_RESTORE_NUM] = {
	{0x100, 0xb}, {0x104, 0xb}, {0x108, 0xb}, {0x110, 5}, {0x118, 2}
};

struct SMI_SETTING_VALUE smi_larb5_restore[SMI_LARB5_RESTORE_NUM] = {
	{0x100, 0xb}, {0x104, 0xb}, {0x108, 0xb}, {0x118, 0xb}
};

struct SMI_SETTING_VALUE *smi_larb_restore[SMI_LARB_NR] = {
	smi_larb0_restore,
	NULL,
	NULL,
	NULL,
	NULL,
	smi_larb5_restore,
	NULL
};

#define SMI_PROFILE_SETTING_COMMON_INIT_NUM 13

#define SMI_INITSETTING_LARB0_NUM (SMI_LARB0_PORT_NUM + 7) /* add cmd throttle setting/dcm/cmd grouping*/
#define SMI_INITSETTING_LARB1_NUM (SMI_LARB1_PORT_NUM + 2) /* add cmd throttle setting/dcm*/
#define SMI_INITSETTING_LARB2_NUM (SMI_LARB2_PORT_NUM + 2) /* add cmd throttle setting/dcm*/
#define SMI_INITSETTING_LARB3_NUM (SMI_LARB3_PORT_NUM + 2) /* add cmd throttle setting/dcm*/
#define SMI_INITSETTING_LARB4_NUM (SMI_LARB4_PORT_NUM + 2) /* add cmd throttle setting/dcm*/
#define SMI_INITSETTING_LARB5_NUM (SMI_LARB5_PORT_NUM + 6) /* add cmd throttle setting/dcm/cmd grouping*/
#define SMI_INITSETTING_LARB6_NUM (SMI_LARB6_PORT_NUM + 2) /* add cmd throttle setting/dcm*/

/* vc setting */
struct SMI_SETTING_VALUE smi_vc_setting[SMI_VC_SETTING_NUM] = {
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
};
/* init_setting */
struct SMI_SETTING_VALUE smi_profile_setting_common_init[SMI_PROFILE_SETTING_COMMON_INIT_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x18FC}, {REG_OFFSET_SMI_L1ARB1, 0x1000}, {REG_OFFSET_SMI_L1ARB2, 0x1000},
	{REG_OFFSET_SMI_L1ARB3, 0x1000}, {REG_OFFSET_SMI_L1ARB4, 0x1000}, {REG_OFFSET_SMI_L1ARB5, 0x126A},
	{REG_OFFSET_SMI_L1ARB6, 0x1000},
	{0x100, 0xb},
	{0x220, 0x1554},
	{0x234, ((0x1 << 31) + (0x1d << 26) + (0x1f << 21) + (0x0 << 20) + (0x3 << 15)
		 + (0x4 << 10) + (0x4 << 5) + 0x5)},
	{0x238, (0x2 << 25) + (0x3 << 20) + (0x4 << 15) + (0x5 << 10) + (0x7 << 5) + 0x8},
	{0x230, 0x7f + (0x8 << 7) + (0x7 << 12)},
	{0x300, 0x1 + (0x1 << 1) + (0x4 << 8)}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_init[SMI_INITSETTING_LARB0_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x24, 0x370246}, {0x100, 0xb}, {0x104, 0xb}, {0x108, 0xb},
	{0x110, 5}, {0x118, 2},
	{0x200, 0x1f}, {0x204, 0x1f}, {0x208, 0x1f}, {0x20c, 0xa}, {0x210, 1}, {0x214, 1}, {0x218, 1},
	    {0x21c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_init[SMI_INITSETTING_LARB1_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x24, 0x370246}, {0x200, 1}, {0x204, 1}, {0x208, 1},
	{0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c, 1}, {0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_init[SMI_INITSETTING_LARB2_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x24, 0x370246},
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_init[SMI_INITSETTING_LARB3_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x24, 0x370246},
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb4_init[SMI_INITSETTING_LARB4_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x24, 0x370246},
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb5_init[SMI_INITSETTING_LARB5_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x24, 0x370246}, {0x100, 0xb}, {0x104, 0xb}, {0x108, 0xb},
	{0x118, 0xb},
	{0x200, 0x1f}, {0x204, 0x1f}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 6}, {0x214, 6}, {0x218, 0x1f}, {0x21c,
											     7},
	{0x220, 8}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb6_init[SMI_INITSETTING_LARB6_NUM] = {
	{0x14, (0x7 << 8) + (0xf << 4)}, {0x24, 0x370246},
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};
struct SMI_SETTING init_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM, SMI_INITSETTING_LARB3_NUM,
		SMI_INITSETTING_LARB4_NUM, SMI_INITSETTING_LARB5_NUM, SMI_INITSETTING_LARB6_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init, smi_profile_setting_larb3_init,
	 smi_profile_setting_larb4_init, smi_profile_setting_larb5_init,
	 smi_profile_setting_larb6_init}
};

/* VPMJC120 */
#define SMI_PROFILE_SETTING_COMMON_VPMJC_NUM SMI_LARB_NR

struct SMI_SETTING_VALUE smi_profile_setting_common_vpmjc[SMI_PROFILE_SETTING_COMMON_VPMJC_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x16DE}, {REG_OFFSET_SMI_L1ARB1, 0x115E}, {REG_OFFSET_SMI_L1ARB2, 0x1000},
	{REG_OFFSET_SMI_L1ARB3, 0x1000}, {REG_OFFSET_SMI_L1ARB4, 0x134C}, {REG_OFFSET_SMI_L1ARB5, 0x12EC},
	{REG_OFFSET_SMI_L1ARB6, 0x1000}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vpmjc[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1f}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 0xc}, {0x210, 3}, {0x214, 1}, {0x218, 0xa},
	    {0x21c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vpmjc[SMI_LARB1_PORT_NUM] = {
	{0x200, 0xb}, {0x204, 2}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 2}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vpmjc[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vpmjc[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb4_vpmjc[SMI_LARB4_PORT_NUM] = {
	{0x200, 6}, {0x204, 3}, {0x208, 7}, {0x20c, 3}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb5_vpmjc[SMI_LARB5_PORT_NUM] = {
	{0x200, 6}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 6}, {0x214, 6}, {0x218, 6}, {0x21c,
											     1},
	{0x220, 3}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb6_vpmjc[SMI_LARB6_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};
struct SMI_SETTING vpmjc_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VPMJC_NUM, smi_profile_setting_common_vpmjc,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_vpmjc, smi_profile_setting_larb1_vpmjc, smi_profile_setting_larb2_vpmjc,
	 smi_profile_setting_larb3_vpmjc, smi_profile_setting_larb4_vpmjc, smi_profile_setting_larb5_vpmjc,
	 smi_profile_setting_larb6_vpmjc}
};

/* VP4K */
#define SMI_PROFILE_SETTING_COMMON_VP_NUM SMI_LARB_NR

struct SMI_SETTING_VALUE smi_profile_setting_common_vp[SMI_PROFILE_SETTING_COMMON_VP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x15B4}, {REG_OFFSET_SMI_L1ARB1, 0x058E}, {REG_OFFSET_SMI_L1ARB2, 0x1000},
	{REG_OFFSET_SMI_L1ARB3, 0x1000}, {REG_OFFSET_SMI_L1ARB4, 0x1000}, {REG_OFFSET_SMI_L1ARB5, 0x126B},
	{REG_OFFSET_SMI_L1ARB6, 0x1000}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vp[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1c}, {0x204, 5}, {0x208, 0x1f}, {0x20c, 0xa}, {0x210, 4}, {0x214, 1}, {0x218, 5},
	    {0x21c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vp[SMI_LARB1_PORT_NUM] = {
	{0x200, 0x1f}, {0x204, 0x1f}, {0x208, 1}, {0x20c, 0x1f}, {0x210, 1}, {0x214, 0x1f}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vp[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vp[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb4_vp[SMI_LARB4_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb5_vp[SMI_LARB5_PORT_NUM] = {
	{0x200, 5}, {0x204, 5}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 6}, {0x214, 6}, {0x218, 5}, {0x21c,
											     0xa},
	{0x220, 0xa}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb6_vp[SMI_LARB6_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};
struct SMI_SETTING vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp, smi_profile_setting_larb4_vp, smi_profile_setting_larb5_vp,
	 smi_profile_setting_larb6_vp}
};

/* VR4K */
#define SMI_PROFILE_SETTING_COMMON_VR_NUM SMI_LARB_NR


struct SMI_SETTING_VALUE smi_profile_setting_common_vr[SMI_PROFILE_SETTING_COMMON_VR_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x1477}, {REG_OFFSET_SMI_L1ARB1, 0x1000}, {REG_OFFSET_SMI_L1ARB2, 0x1198},
	{REG_OFFSET_SMI_L1ARB3, 0x1444}, {REG_OFFSET_SMI_L1ARB4, 0x1000}, {REG_OFFSET_SMI_L1ARB5, 0x1000},
	{REG_OFFSET_SMI_L1ARB6, 0x13f0}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_vr[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1f}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 1}, {0x214, 4}, {0x218, 0xa},
	    {0x21c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vr[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vr[SMI_LARB2_PORT_NUM] = {
	{0x200, 2}, {0x204, 0x1f}, {0x208, 2}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vr[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 4}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 6}, {0x228, 3}, {0x22c, 1}, {0x230, 0xd}, {0x234, 1}, {0x238, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb4_vr[SMI_LARB4_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb5_vr[SMI_LARB5_PORT_NUM] = {
	{0x200, 6}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 6}, {0x214, 6}, {0x218, 6}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb6_vr[SMI_LARB6_PORT_NUM] = {
	{0x200, 7}, {0x204, 1}, {0x208, 5}, {0x20c, 6}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};
struct SMI_SETTING vr_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr, smi_profile_setting_larb4_vr, smi_profile_setting_larb5_vr,
	 smi_profile_setting_larb6_vr}
};

/* SMVR120 */
#define SMI_PROFILE_SETTING_COMMON_VR_SLOW_NUM SMI_LARB_NR

struct SMI_SETTING_VALUE smi_profile_setting_common_vr_slow[SMI_PROFILE_SETTING_COMMON_VR_SLOW_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x14ff}, {REG_OFFSET_SMI_L1ARB1, 0x1000}, {REG_OFFSET_SMI_L1ARB2, 0x1139},
	{REG_OFFSET_SMI_L1ARB3, 0x148E}, {REG_OFFSET_SMI_L1ARB4, 0x1000}, {REG_OFFSET_SMI_L1ARB5, 0x1000},
	{REG_OFFSET_SMI_L1ARB6, 0x1193}
};


struct SMI_SETTING_VALUE smi_profile_setting_larb0_vr_slow[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1f}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 1}, {0x214, 4}, {0x218, 0xc},
	    {0x21c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_vr_slow[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_vr_slow[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 0xa}, {0x208, 2}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_vr_slow[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 4}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 6}, {0x228, 3}, {0x22c, 1}, {0x230, 0xd}, {0x234, 1}, {0x238, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb4_vr_slow[SMI_LARB4_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb5_vr_slow[SMI_LARB5_PORT_NUM] = {
	{0x200, 6}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 6}, {0x214, 6}, {0x218, 6}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb6_vr_slow[SMI_LARB6_PORT_NUM] = {
	{0x200, 7}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};
struct SMI_SETTING vr_slow_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_SLOW_NUM, smi_profile_setting_common_vr_slow,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_vr_slow, smi_profile_setting_larb1_vr_slow, smi_profile_setting_larb2_vr_slow,
	 smi_profile_setting_larb3_vr_slow, smi_profile_setting_larb4_vr_slow, smi_profile_setting_larb5_vr_slow,
	 smi_profile_setting_larb6_vr_slow}
};

/* N3D */
#define SMI_PROFILE_SETTING_COMMON_N3D_NUM SMI_LARB_NR


struct SMI_SETTING_VALUE smi_profile_setting_common_n3d[SMI_PROFILE_SETTING_COMMON_N3D_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x13EC}, {REG_OFFSET_SMI_L1ARB1, 0x1000}, {REG_OFFSET_SMI_L1ARB2, 0x104B},
	{REG_OFFSET_SMI_L1ARB3, 0x113D}, {REG_OFFSET_SMI_L1ARB4, 0x1000}, {REG_OFFSET_SMI_L1ARB5, 0x1000},
	{REG_OFFSET_SMI_L1ARB6, 0x1654}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_n3d[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1f}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 1}, {0x214, 2}, {0x218, 3},
	    {0x21c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_n3d[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_n3d[SMI_LARB2_PORT_NUM] = {
	{0x200, 1}, {0x204, 4}, {0x208, 2}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_n3d[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 3}, {0x234, 1}, {0x238, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb4_n3d[SMI_LARB4_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb5_n3d[SMI_LARB5_PORT_NUM] = {
	{0x200, 6}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 6}, {0x214, 6}, {0x218, 6}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb6_n3d[SMI_LARB6_PORT_NUM] = {
	{0x200, 7}, {0x204, 2}, {0x208, 7}, {0x20c, 0xa}, {0x210, 2}, {0x214, 1}, {0x218, 3}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};
struct SMI_SETTING n3d_setting_config = {
	SMI_PROFILE_SETTING_COMMON_N3D_NUM, smi_profile_setting_common_n3d,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_n3d, smi_profile_setting_larb1_n3d, smi_profile_setting_larb2_n3d,
	 smi_profile_setting_larb3_n3d, smi_profile_setting_larb4_n3d, smi_profile_setting_larb5_n3d,
	 smi_profile_setting_larb6_n3d}
};

/* ICFP */
#define SMI_PROFILE_SETTING_COMMON_ICFP_NUM SMI_LARB_NR

struct SMI_SETTING_VALUE smi_profile_setting_common_icfp[SMI_PROFILE_SETTING_COMMON_ICFP_NUM] = {
	{REG_OFFSET_SMI_L1ARB0, 0x146F}, {REG_OFFSET_SMI_L1ARB1, 0x1000}, {REG_OFFSET_SMI_L1ARB2, 0x1252},
	{REG_OFFSET_SMI_L1ARB3, 0x1119}, {REG_OFFSET_SMI_L1ARB4, 0x1000}, {REG_OFFSET_SMI_L1ARB5, 0x1000},
	{REG_OFFSET_SMI_L1ARB6, 0x1278}
};


struct SMI_SETTING_VALUE smi_profile_setting_larb0_icfp[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1f}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 1}, {0x214, 4}, {0x218, 3},
	    {0x21c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_icfp[SMI_LARB1_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_icfp[SMI_LARB2_PORT_NUM] = {
	{0x200, 0xa}, {0x204, 6}, {0x208, 2}, {0x20c, 1}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb3_icfp[SMI_LARB3_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}, {0x210, 1}, {0x214, 4}, {0x218, 2}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}, {0x228, 1}, {0x22c, 1}, {0x230, 1}, {0x234, 1}, {0x238, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb4_icfp[SMI_LARB4_PORT_NUM] = {
	{0x200, 1}, {0x204, 1}, {0x208, 1}, {0x20c, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb5_icfp[SMI_LARB5_PORT_NUM] = {
	{0x200, 6}, {0x204, 6}, {0x208, 0x1f}, {0x20c, 1}, {0x210, 6}, {0x214, 6}, {0x218, 6}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb6_icfp[SMI_LARB6_PORT_NUM] = {
	{0x200, 7}, {0x204, 1}, {0x208, 2}, {0x20c, 2}, {0x210, 1}, {0x214, 1}, {0x218, 1}, {0x21c,
											     1},
	{0x220, 1}, {0x224, 1}
};
struct SMI_SETTING icfp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_ICFP_NUM, smi_profile_setting_common_icfp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_icfp, smi_profile_setting_larb1_icfp, smi_profile_setting_larb2_icfp,
	 smi_profile_setting_larb3_icfp, smi_profile_setting_larb4_icfp, smi_profile_setting_larb5_icfp,
	 smi_profile_setting_larb6_icfp}
};

/* vr series */
struct SMI_SETTING vss_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr, smi_profile_setting_larb4_vr, smi_profile_setting_larb5_vr,
	 smi_profile_setting_larb6_vr}
};

struct SMI_SETTING venc_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr, smi_profile_setting_larb4_vr, smi_profile_setting_larb5_vr,
	 smi_profile_setting_larb6_vr}
};

/* vp series */
struct SMI_SETTING vpwfd_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VR_NUM, smi_profile_setting_common_vr,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_vr, smi_profile_setting_larb1_vr, smi_profile_setting_larb2_vr,
	 smi_profile_setting_larb3_vr, smi_profile_setting_larb4_vr, smi_profile_setting_larb5_vr,
	 smi_profile_setting_larb6_vr}
};

struct SMI_SETTING swdec_vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_VP_NUM, smi_profile_setting_common_vp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
		SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM, SMI_LARB6_PORT_NUM},
	{smi_profile_setting_larb0_vp, smi_profile_setting_larb1_vp, smi_profile_setting_larb2_vp,
	 smi_profile_setting_larb3_vp, smi_profile_setting_larb4_vp, smi_profile_setting_larb5_vp,
	 smi_profile_setting_larb6_vp}
};

/* init seris */
struct SMI_SETTING mm_gpu_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_INITSETTING_LARB0_NUM, SMI_INITSETTING_LARB1_NUM, SMI_INITSETTING_LARB2_NUM, SMI_INITSETTING_LARB3_NUM,
		SMI_INITSETTING_LARB4_NUM, SMI_INITSETTING_LARB5_NUM, SMI_INITSETTING_LARB6_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init,
	 smi_profile_setting_larb2_init, smi_profile_setting_larb3_init,
	 smi_profile_setting_larb4_init, smi_profile_setting_larb5_init,
	 smi_profile_setting_larb6_init}
};

struct SMI_SETTING ui_idle_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi4k_setting_config = { 0, NULL, {0}, {0} };

#elif defined(SMI_BRINGUP)
struct SMI_SETTING init_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vr_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING swdec_vp_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vp_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vr_slow_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING mm_gpu_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vpwfd_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING venc_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING icfp_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vss_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING vpmjc_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING n3d_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING ui_idle_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi4k_setting_config = { 0, NULL, {0}, {0} };
#endif

struct SMI_PROFILE_CONFIG smi_profile_config[SMI_PROFILE_CONFIG_NUM] = {
	{SMI_BWC_SCEN_NORMAL, &init_setting_config},
	{SMI_BWC_SCEN_VR, &vr_setting_config},
	{SMI_BWC_SCEN_SWDEC_VP, &swdec_vp_setting_config},
	{SMI_BWC_SCEN_VP, &vp_setting_config},
	{SMI_BWC_SCEN_VP_HIGH_FPS, &vp_setting_config},
	{SMI_BWC_SCEN_VP_HIGH_RESOLUTION, &vp_setting_config},
	{SMI_BWC_SCEN_VR_SLOW, &vr_slow_setting_config},
	{SMI_BWC_SCEN_MM_GPU, &mm_gpu_setting_config},
	{SMI_BWC_SCEN_WFD, &vpwfd_setting_config},
	{SMI_BWC_SCEN_VENC, &venc_setting_config},
	{SMI_BWC_SCEN_ICFP, &icfp_setting_config},
	{SMI_BWC_SCEN_UI_IDLE, &ui_idle_setting_config},
	{SMI_BWC_SCEN_VSS, &vss_setting_config},
	{SMI_BWC_SCEN_FORCE_MMDVFS, &init_setting_config},
	{SMI_BWC_SCEN_HDMI, &hdmi_setting_config},
	{SMI_BWC_SCEN_HDMI4K, &hdmi4k_setting_config},
	{SMI_BWC_SCEN_VPMJC, &vpmjc_setting_config},
	{SMI_BWC_SCEN_N3D, &n3d_setting_config}
};
