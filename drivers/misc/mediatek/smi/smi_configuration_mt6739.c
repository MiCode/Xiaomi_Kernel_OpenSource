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

#include <linux/io.h>
#include <linux/string.h>
#include "mtk_smi.h"
#include "smi_configuration.h"
#include "smi_common.h"
#include "smi_reg.h"
#include "smi_debug.h"

/* add static after all platform setting parameters moved to here */

unsigned short smi_subsys_larb_mapping_table[SMI_SUBSYS_NUM] = {
	0x3, 0x10, 0x6c, 0x80, 0
};

unsigned long smi_m4u_non_secure_offset[SMI_MAX_PORT_NUM] = {
	0x380, 0x384, 0x388, 0x38c, 0x390, 0x394, 0x398, 0x39c,
	0x3a0, 0x3a4, 0x3a8, 0x3ac, 0x3b0, 0x3b4, 0x3b8, 0x3bc,
	0x3c0, 0x3c4, 0x3c8, 0x3cc, 0x3d0, 0x3d4, 0x3d8, 0x3dc,
	0x3e0, 0x3e4, 0x3e8, 0x3ec, 0x3f0, 0x3f4, 0x3f8, 0x3fc
};
unsigned long smi_m4u_secure_offset[SMI_MAX_PORT_NUM] = {
	0xf80, 0xf84, 0xf88, 0xf8c, 0xf90, 0xf94, 0xf98, 0xf9c,
	0xfa0, 0xfa4, 0xfa8, 0xfac, 0xfb0, 0xfb4, 0xfb8, 0xfbc,
	0xfc0, 0xfc4, 0xfc8, 0xfcc, 0xfd0, 0xfd4, 0xfd8, 0xfdc,
	0xfe0, 0xfe4, 0xfe8, 0xfec, 0xff0, 0xff4, 0xff8, 0xffc
};

struct SMI_SETTING_VALUE smi_basic_common_setting[SMI_BASIC_COMMON_SETTING_NUM] = {
	{0x100, 0xb}, {0x220, 0x0},
	{0x228, 0x6 + (0x6 << 6) + (0x6 << 12) + (0x6 << 18) + (0x6 << 24)},
	{0x22C, 0x6 + (0x6 << 6) + (0x6 << 12)},
	{0x308, 0x6 + (0x6 << 6) + (0x6 << 12) + (0x6 << 18) + (0x6 << 24)},
	{0x30C, 0x6 + (0x6 << 6) + (0x6 << 12)},
	{0x310, 0x6 + (0x6 << 6) + (0x6 << 12) + (0x6 << 18) + (0x6 << 24)},
	{0x314, 0x6 + (0x6 << 6) + (0x6 << 12)},
	{0x318, 0x6 + (0x6 << 6) + (0x6 << 12) + (0x6 << 18) + (0x6 << 24)},
	{0x31C, 0x6 + (0x6 << 6) + (0x6 << 12)},
	{0x320, 0x6 + (0x6 << 6) + (0x6 << 12) + (0x6 << 18) + (0x6 << 24)},
	{0x324, 0x6 + (0x6 << 6) + (0x6 << 12)},
	{0x300, 0x1 + (0x78 << 1) + (0x4 << 8)},
	{0x444, 0x1}
};

struct SMI_SETTING_VALUE smi_basic_larb0_setting[SMI_BASIC_LARB0_SETTING_NUM] = {
	{0x40, 0x1}, {0x100, 0x5}, {0x104, 0x5}, {0x108, 0x5}
};

struct SMI_SETTING_VALUE smi_basic_larb1_setting[SMI_BASIC_LARB1_SETTING_NUM] = {
	{0x40, 0x1}
};
struct SMI_SETTING_VALUE smi_basic_larb2_setting[SMI_BASIC_LARB2_SETTING_NUM] = {
	{0x40, 0x1}
};

struct SMI_SETTING smi_basic_setting_config = {
	SMI_BASIC_COMMON_SETTING_NUM, smi_basic_common_setting,
	{SMI_BASIC_LARB0_SETTING_NUM, SMI_BASIC_LARB1_SETTING_NUM, SMI_BASIC_LARB2_SETTING_NUM},
	{smi_basic_larb0_setting, smi_basic_larb1_setting, smi_basic_larb2_setting}
};

struct SMI_SETTING_VALUE smi_mmu_larb0_setting[SMI_MMU_LARB0_SETTING_NUM] = {
	{0x380, 0x3}, {0x384, 0x3}, {0x388, 0x3}
};

struct SMI_SETTING smi_mmu_setting_config = {
	0, NULL,
	{SMI_MMU_LARB0_SETTING_NUM, SMI_MMU_LARB1_SETTING_NUM, SMI_MMU_LARB2_SETTING_NUM},
	{smi_mmu_larb0_setting, NULL, NULL}
};

/* vc setting, no need in this platform */
struct SMI_SETTING_VALUE smi_vc_setting[SMI_VC_SETTING_NUM] = {
	{0, 0}, {0, 0}, {0, 0}
};

struct SMI_CLK_INFO smi_clk_info[SMI_CLK_CNT] = {
	{"", 0, 0}, {"", 0, 0}, {"", 0, 0}, {"", 0, 0}, {"", 0, 0},
	{"", 0, 0}, {"", 0, 0}, {"", 0, 0}
};

unsigned long smi_larb0_debug_offset[SMI_LARB0_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x40, 0x50, 0x60, 0x80, 0x84, 0xa0, 0xa4, 0xa8, 0xac,
	0xb0, 0xb4, 0xb8, 0xbc, 0xc0, 0xc8, 0xcc,
	0x100, 0x104, 0x108, 0x10c, 0x110, 0x114, 0x118, 0x11c, 0x120, 0x124, 0x128, 0x12c,
	0x130, 0x134, 0x138, 0x13c, 0x140, 0x144, 0x148, 0x14c,
	0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	0x230, 0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c,
	0x280, 0x284, 0x288, 0x28c, 0x290, 0x294, 0x298, 0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac,
	0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc, 0x2d0, 0x2d4, 0x2d8, 0x2dc,
	0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb1_debug_offset[SMI_LARB1_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x40, 0x50, 0x60, 0x80, 0x84, 0xa0, 0xa4, 0xa8, 0xac,
	0xb0, 0xb4, 0xb8, 0xbc, 0xc0, 0xc8, 0xcc,
	0x100, 0x104, 0x108, 0x10c, 0x110, 0x114, 0x118, 0x11c, 0x120, 0x124, 0x128, 0x12c,
	0x130, 0x134, 0x138, 0x13c, 0x140, 0x144, 0x148, 0x14c,
	0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	0x230, 0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c,
	0x280, 0x284, 0x288, 0x28c, 0x290, 0x294, 0x298, 0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac,
	0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc, 0x2d0, 0x2d4, 0x2d8, 0x2dc,
	0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_larb2_debug_offset[SMI_LARB2_DEBUG_OFFSET_NUM] = {
	0x0, 0x8, 0x10, 0x14, 0x24, 0x40, 0x50, 0x60, 0x80, 0x84, 0xa0, 0xa4, 0xa8, 0xac,
	0xb0, 0xb4, 0xb8, 0xbc, 0xc0, 0xc8, 0xcc,
	0x100, 0x104, 0x108, 0x10c, 0x110, 0x114, 0x118, 0x11c, 0x120, 0x124, 0x128, 0x12c,
	0x130, 0x134, 0x138, 0x13c, 0x140, 0x144, 0x148, 0x14c,
	0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224, 0x228, 0x22c,
	0x230, 0x234, 0x238, 0x23c, 0x240, 0x244, 0x248, 0x24c,
	0x280, 0x284, 0x288, 0x28c, 0x290, 0x294, 0x298, 0x29c, 0x2a0, 0x2a4, 0x2a8, 0x2ac,
	0x2b0, 0x2b4, 0x2b8, 0x2bc, 0x2c0, 0x2c4, 0x2c8, 0x2cc, 0x2d0, 0x2d4, 0x2d8, 0x2dc,
	0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc
};

unsigned long smi_common_debug_offset[SMI_COMMON_DEBUG_OFFSET_NUM] = {
	0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x118, 0x11c, 0x120,
	0x220, 0x228, 0x22c, 0x230, 0x234, 0x238, 0x23c,
	0x300, 0x308, 0x30c, 0x310, 0x314, 0x318, 0x31c, 0x320, 0x324,
	0x3c0, 0x400, 0x404, 0x408, 0x40C, 0x410, 0x414, 0x418, 0x41c,
	0x430, 0x434, 0x440, 0x444
};

int smi_larb_debug_offset_num[SMI_LARB_NUM] = {
	SMI_LARB0_DEBUG_OFFSET_NUM, SMI_LARB1_DEBUG_OFFSET_NUM, SMI_LARB2_DEBUG_OFFSET_NUM
};

unsigned long *smi_larb_debug_offset[SMI_LARB_NUM] = {
	smi_larb0_debug_offset, smi_larb1_debug_offset, smi_larb2_debug_offset
};

/* wfd */
struct SMI_SETTING_VALUE smi_profile_setting_larb0_wfd[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1}, {0x204, 0x1f}, {0x208, 0x1f}, {0x20c, 0x2}, {0x210, 0x1}, {0x214, 0x1}, {0x218, 0x5}
};

/* init (UI) */
#define SMI_PROFILE_SETTING_COMMON_INIT_NUM (SMI_COMMON_PORT_NUM + 1)

struct SMI_SETTING_VALUE smi_profile_setting_common_init[SMI_PROFILE_SETTING_COMMON_INIT_NUM] = {
	{0x220, 0x0},
	{REG_OFFSET_SMI_L1ARB0, 0x12f6}, {REG_OFFSET_SMI_L1ARB1, 0x1000}, {REG_OFFSET_SMI_L1ARB2, 0x1000}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_init[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1f}, {0x204, 0x1f}, {0x208, 0x1f}, {0x20c, 0x2}, {0x210, 0x1}, {0x214, 0x1}, {0x218, 0x5}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_init[SMI_LARB1_PORT_NUM] = {
	{0x200, 0x3}, {0x204, 0x1}, {0x208, 0x2}, {0x20c, 0x1}, {0x210, 0x1}, {0x214, 0x3},
	{0x218, 0x2}, {0x21c, 0x1}, {0x220, 0x1}, {0x224, 0x1}, {0x228, 0x5}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_init[SMI_LARB2_PORT_NUM] = {
	{0x200, 0xc}, {0x204, 0x6}, {0x208, 0x2}, {0x20c, 0x2}, {0x210, 0x2}, {0x214, 0x2},
	{0x218, 0x2}, {0x21c, 0x2}, {0x220, 0x4}, {0x224, 0x3}, {0x228, 0x1}
};

struct SMI_SETTING init_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init, smi_profile_setting_larb2_init}
};

/* init series */
struct SMI_SETTING mm_gpu_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init, smi_profile_setting_larb2_init}
};

/* ICFP: SW_PDE for VR related only */
#define SMI_PROFILE_SETTING_COMMON_ICFP_NUM (SMI_COMMON_PORT_NUM + 1)

struct SMI_SETTING_VALUE smi_profile_setting_common_icfp[SMI_PROFILE_SETTING_COMMON_ICFP_NUM] = {
	{0x220, 0x0},
	{REG_OFFSET_SMI_L1ARB0, 0x119a}, {REG_OFFSET_SMI_L1ARB1, 0x118f}, {REG_OFFSET_SMI_L1ARB2, 0x1250}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb0_icfp[SMI_LARB0_PORT_NUM] = {
	{0x200, 0x1f}, {0x204, 0x1f}, {0x208, 0x1f}, {0x20c, 0xe}, {0x210, 0x1}, {0x214, 0x1}, {0x218, 0x7}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb1_icfp[SMI_LARB1_PORT_NUM] = {
	{0x200, 0x1}, {0x204, 0x1}, {0x208, 0x1}, {0x20c, 0x1}, {0x210, 0x1}, {0x214, 0x1},
	{0x218, 0x1}, {0x21c, 0x1}, {0x220, 0x1}, {0x224, 0x1}, {0x228, 0x4}
};

struct SMI_SETTING_VALUE smi_profile_setting_larb2_icfp[SMI_LARB2_PORT_NUM] = {
	{0x200, 0xa}, {0x204, 0x4}, {0x208, 0x2}, {0x20c, 0x2}, {0x210, 0x2}, {0x214, 0x2},
	{0x218, 0x2}, {0x21c, 0x2}, {0x220, 0x4}, {0x224, 0x2}, {0x228, 0x1}
};

struct SMI_SETTING icfp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init, smi_profile_setting_larb2_init}
};

/* icfp series */
struct SMI_SETTING vr_setting_config = {
	SMI_PROFILE_SETTING_COMMON_ICFP_NUM, smi_profile_setting_common_icfp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_icfp, smi_profile_setting_larb1_icfp, smi_profile_setting_larb2_icfp}
};

struct SMI_SETTING swdec_vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init, smi_profile_setting_larb2_init}
};

struct SMI_SETTING vp_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init, smi_profile_setting_larb2_init}
};

struct SMI_SETTING vr_slow_setting_config = {
	SMI_PROFILE_SETTING_COMMON_ICFP_NUM, smi_profile_setting_common_icfp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_icfp, smi_profile_setting_larb1_icfp, smi_profile_setting_larb2_icfp}
};

struct SMI_SETTING vpwfd_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_wfd, smi_profile_setting_larb1_init, smi_profile_setting_larb2_init}
};

struct SMI_SETTING venc_setting_config = {
	SMI_PROFILE_SETTING_COMMON_INIT_NUM, smi_profile_setting_common_init,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_init, smi_profile_setting_larb1_init, smi_profile_setting_larb2_init}
};

struct SMI_SETTING vss_setting_config = {
	SMI_PROFILE_SETTING_COMMON_ICFP_NUM, smi_profile_setting_common_icfp,
	{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM},
	{smi_profile_setting_larb0_icfp, smi_profile_setting_larb1_icfp, smi_profile_setting_larb2_icfp}
};

struct SMI_SETTING vpmjc_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING n3d_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING ui_idle_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi_setting_config = { 0, NULL, {0}, {0} };
struct SMI_SETTING hdmi4k_setting_config = { 0, NULL, {0}, {0} };


struct SMI_PROFILE_CONFIG smi_profile_config[SMI_PROFILE_CONFIG_NUM] = {
	{SMI_BWC_SCEN_NORMAL, &init_setting_config},
	{SMI_BWC_SCEN_UI_IDLE, &ui_idle_setting_config},
	{SMI_BWC_SCEN_VPMJC, &vpmjc_setting_config},
	{SMI_BWC_SCEN_FORCE_MMDVFS, &init_setting_config},
	{SMI_BWC_SCEN_HDMI, &hdmi_setting_config},
	{SMI_BWC_SCEN_HDMI4K, &hdmi4k_setting_config},
	{SMI_BWC_SCEN_WFD, &vpwfd_setting_config},
	{SMI_BWC_SCEN_VENC, &venc_setting_config},
	{SMI_BWC_SCEN_SWDEC_VP, &swdec_vp_setting_config},
	{SMI_BWC_SCEN_VP, &vp_setting_config},
	{SMI_BWC_SCEN_VP_HIGH_FPS, &vp_setting_config},
	{SMI_BWC_SCEN_VP_HIGH_RESOLUTION, &vp_setting_config},
	{SMI_BWC_SCEN_VR, &vr_setting_config},
	{SMI_BWC_SCEN_VR_SLOW, &vr_slow_setting_config},
	{SMI_BWC_SCEN_VSS, &vss_setting_config},
	{SMI_BWC_SCEN_CAM_PV, &vr_setting_config},
	{SMI_BWC_SCEN_CAM_CP, &vr_setting_config},
	{SMI_BWC_SCEN_ICFP, &icfp_setting_config},
	{SMI_BWC_SCEN_MM_GPU, &mm_gpu_setting_config},
};
