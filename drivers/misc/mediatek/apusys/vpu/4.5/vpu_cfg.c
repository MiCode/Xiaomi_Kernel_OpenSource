// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include "vpu_cfg.h"
#include "vpu_cmn.h"
#ifdef VPU_EFUSE_READY  // TODO: update header for get_devinfo_with_index()
#include "mtk_devinfo.h"
#endif
#include "vpu_debug.h"
#include <soc/mediatek/emi.h>

static void vpu_emi_mpu_set_dummy(unsigned long start, unsigned int size)
{
}

/* VPU EMI MPU setting for MT6885, MT6873, MT6853, MT6833 */
static void vpu_emi_mpu_set_mt68xx(unsigned long start, unsigned int size)
{
#define MPU_PROCT_REGION	21
#define MPU_PROCT_D0_AP		0
#define MPU_PROCT_D5_APUSYS	5

#if IS_ENABLED(CONFIG_MTK_EMI)
	struct emimpu_region_t md_region;

	mtk_emimpu_init_region(&md_region, MPU_PROCT_REGION);
	mtk_emimpu_set_addr(&md_region, start,
			    (start + (unsigned long)size) - 0x1);
	mtk_emimpu_set_apc(&md_region, MPU_PROCT_D0_AP,
			   MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&md_region, MPU_PROCT_D5_APUSYS,
			   MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_lock_region(&md_region, true);
	mtk_emimpu_set_protection(&md_region);
	mtk_emimpu_free_region(&md_region);
#endif

#undef MPU_PROCT_REGION
#undef MPU_PROCT_D0_AP
#undef MPU_PROCT_D5_APUSYS
}

/**
 * vpu_is_disabled - enable/disable vpu from efuse
 * @vd: struct vpu_device to get the id
 *
 * return 1: this vd->id is disabled
 * return 0: this vd->id is enabled
 */
static bool vpu_is_disabled_dummy(struct vpu_device *vd)
{
	return false;
}

static bool vpu_is_disabled_mt6885(struct vpu_device *vd)
{
#ifdef VPU_EFUSE_READY
#define EFUSE_VPU_OFFSET	5
#define EFUSE_VPU_MASK		0x7
#define EFUSE_VPU_SHIFT		16
#define EFUSE_SEG_OFFSET	30

	bool ret;
	unsigned int efuse;
	unsigned int seg;
	unsigned int mask;

	mask = 1 << vd->id;

	seg = get_devinfo_with_index(EFUSE_SEG_OFFSET);
	efuse = get_devinfo_with_index(EFUSE_VPU_OFFSET);
	efuse = (efuse >> EFUSE_VPU_SHIFT) & EFUSE_VPU_MASK;
	/* disabled by mask, or disabled by segment */
	ret = (efuse & mask) || ((seg == 0x1) && (vd->id >= 2));

	/* show efuse info to let user know */
	pr_info("%s: seg: 0x%x, efuse: 0x%x, core%d is %s\n",
		__func__, seg, efuse, vd->id,
		ret ? "disabled" : "enabled");

	return ret;

#undef EFUSE_VPU_OFFSET
#undef EFUSE_VPU_MASK
#undef EFUSE_VPU_SHIFT
#undef EFUSE_SEG_OFFSET
#else
	return false;
#endif
}


/* VPU register setting for MT6885, MT6873, MT6853, MT6833 */
struct vpu_register vpu_reg_mt68xx = {
	/* register defines */
	.mbox_inbox_0 = 0x000,
	.mbox_inbox_1 = 0x004,
	.mbox_inbox_2 = 0x008,
	.mbox_inbox_3 = 0x00c,
	.mbox_inbox_4 = 0x010,
	.mbox_inbox_5 = 0x014,
	.mbox_inbox_6 = 0x018,
	.mbox_inbox_7 = 0x01c,
	.mbox_inbox_8 = 0x020,
	.mbox_inbox_9 = 0x024,
	.mbox_inbox_10 = 0x028,
	.mbox_inbox_11 = 0x02c,
	.mbox_inbox_12 = 0x030,
	.mbox_inbox_13 = 0x034,
	.mbox_inbox_14 = 0x038,
	.mbox_inbox_15 = 0x03c,
	.mbox_inbox_16 = 0x040,
	.mbox_inbox_17 = 0x044,
	.mbox_inbox_18 = 0x048,
	.mbox_inbox_19 = 0x04c,
	.mbox_inbox_20 = 0x050,
	.mbox_inbox_21 = 0x054,
	.mbox_inbox_22 = 0x058,
	.mbox_inbox_23 = 0x05c,
	.mbox_inbox_24 = 0x060,
	.mbox_inbox_25 = 0x064,
	.mbox_inbox_26 = 0x068,
	.mbox_inbox_27 = 0x06c,
	.mbox_inbox_28 = 0x070,
	.mbox_inbox_29 = 0x074,
	.mbox_inbox_30 = 0x078,
	.mbox_inbox_31 = 0x07c,
	.mbox_dummy_0 = 0x080,
	.mbox_dummy_1 = 0x084,
	.mbox_dummy_2 = 0x088,
	.mbox_dummy_3 = 0x08c,
	.mbox_dummy_4 = 0x090,
	.mbox_dummy_5 = 0x094,
	.mbox_dummy_6 = 0x098,
	.mbox_dummy_7 = 0x09c,
	.mbox_inbox_irq = 0x0a0,
	.mbox_inbox_mask = 0x0a4,
	.mbox_inbox_pri_mask = 0x0a8,
	.cg_con = 0x100,
	.cg_clr = 0x108,
	.sw_rst = 0x10c,
	.done_st = 0x90c,
	.ctrl = 0x910,
	.xtensa_int = 0x200,
	.ctl_xtensa_int = 0x204,
	.default0 = 0x93c,
	.default1 = 0x940,
	.default2 = 0x944,
	.xtensa_info00 = 0x250,
	.xtensa_info01 = 0x254,
	.xtensa_info02 = 0x258,
	.xtensa_info03 = 0x25c,
	.xtensa_info04 = 0x260,
	.xtensa_info05 = 0x264,
	.xtensa_info06 = 0x268,
	.xtensa_info07 = 0x26c,
	.xtensa_info08 = 0x270,
	.xtensa_info09 = 0x274,
	.xtensa_info10 = 0x278,
	.xtensa_info11 = 0x27c,
	.xtensa_info12 = 0x280,
	.xtensa_info13 = 0x284,
	.xtensa_info14 = 0x288,
	.xtensa_info15 = 0x28c,
	.xtensa_info16 = 0x290,
	.xtensa_info17 = 0x294,
	.xtensa_info18 = 0x298,
	.xtensa_info19 = 0x29c,
	.xtensa_info20 = 0x2a0,
	.xtensa_info21 = 0x2a4,
	.xtensa_info22 = 0x2a8,
	.xtensa_info23 = 0x2ac,
	.xtensa_info24 = 0x2b0,
	.xtensa_info25 = 0x2b4,
	.xtensa_info26 = 0x2b8,
	.xtensa_info27 = 0x2bc,
	.xtensa_info28 = 0x2c0,
	.xtensa_info29 = 0x2c4,
	.xtensa_info30 = 0x2c8,
	.xtensa_info31 = 0x2cc,
	.debug_info00 = 0x2d0,
	.debug_info01 = 0x2d4,
	.debug_info02 = 0x2d8,
	.debug_info03 = 0x2dc,
	.debug_info04 = 0x2e0,
	.debug_info05 = 0x2e4,
	.debug_info06 = 0x2e8,
	.debug_info07 = 0x2ec,
	.xtensa_altresetvec = 0x2f8,

	/* register config: ctrl */
	.p_debug_enable = (1 << 31),
	.state_vector_select = (1 << 19),
	.pbclk_enable = (1 << 26),
	.prid = (0x1fffe),
	.pif_gated = (1 << 17),
	.stall = (1 << 23),

	/* register config: sw_rst */
	.apu_d_rst = (1 << 8),
	.apu_b_rst = (1 << 4),
	.ocdhaltonreset = (1 << 12),

	/* register config: default0 */
	.aruser = (0x2 << 23),
	.awuser = (0x2 << 18),
	.qos_swap = (1 << 28),

	/* register config: default1 */
	.aruser_idma = (0x2 << 0),
	.awuser_idma = (0x2 << 5),

	/* register config: default2 */
	.dbg_en = (0xf),

	/* register config: cg_clr */
	.jtag_cg_clr = (0x2),

	/* register mask: done_st */
	.pwaitmode = (1 << 7),
};

/* VPU register setting for MT6779, MT6785 */
struct vpu_register vpu_reg_mt67xx = {
	/* register defines */
	.cg_con = 0x0,
	.cg_clr = 0x8,
	.sw_rst = 0xc,
	.done_st = 0x10c,
	.ctrl = 0x110,
	.xtensa_int = 0x114,
	.ctl_xtensa_int = 0x118,
	.default0 = 0x13c,
	.default1 = 0x140,
	.default2 = 0x144,
	.xtensa_info00 = 0x150,
	.xtensa_info01 = 0x154,
	.xtensa_info02 = 0x158,
	.xtensa_info03 = 0x15c,
	.xtensa_info04 = 0x160,
	.xtensa_info05 = 0x164,
	.xtensa_info06 = 0x168,
	.xtensa_info07 = 0x16c,
	.xtensa_info08 = 0x170,
	.xtensa_info09 = 0x174,
	.xtensa_info10 = 0x178,
	.xtensa_info11 = 0x17c,
	.xtensa_info12 = 0x180,
	.xtensa_info13 = 0x184,
	.xtensa_info14 = 0x188,
	.xtensa_info15 = 0x18c,
	.xtensa_info16 = 0x190,
	.xtensa_info17 = 0x194,
	.xtensa_info18 = 0x198,
	.xtensa_info19 = 0x19c,
	.xtensa_info20 = 0x1a0,
	.xtensa_info21 = 0x1a4,
	.xtensa_info22 = 0x1a8,
	.xtensa_info23 = 0x1ac,
	.xtensa_info24 = 0x1b0,
	.xtensa_info25 = 0x1b4,
	.xtensa_info26 = 0x1b8,
	.xtensa_info27 = 0x1bc,
	.xtensa_info28 = 0x1c0,
	.xtensa_info29 = 0x1c4,
	.xtensa_info30 = 0x1c8,
	.xtensa_info31 = 0x1cc,
	.debug_info00 = 0x1d0,
	.debug_info01 = 0x1d4,
	.debug_info02 = 0x1d8,
	.debug_info03 = 0x1dc,
	.debug_info04 = 0x1e0,
	.debug_info05 = 0x1e4,
	.debug_info06 = 0x1e8,
	.debug_info07 = 0x1ec,
	.xtensa_altresetvec = 0x1f8,

	/* register config: ctrl */
	.p_debug_enable = (1 << 31),
	.state_vector_select = (1 << 19),
	.pbclk_enable = (1 << 26),
	.prid = (0x1fffe),
	.pif_gated = (1 << 17),
	.stall = (1 << 23),

	/* register config: sw_rst */
	.apu_d_rst = (1 << 8),
	.apu_b_rst = (1 << 4),
	.ocdhaltonreset = (1 << 12),

	/* register config: default0 */
	.aruser = (0x8 << 23),
	.awuser = (0x8 << 18),
	.qos_swap = (1 << 28),

	/* register config: default1 */
	.aruser_idma = (0x8 << 0),
	.awuser_idma = (0x8 << 5),

	/* register config: default2 */
	.dbg_en = (0xf),

	/* register config: cg_clr */
	.jtag_cg_clr = (0x2),

	/* register mask: done_st */
	.pwaitmode = (1 << 7),
};

/* VPU config for MT6885, MT6873, MT6853, MT6833 */
struct vpu_config vpu_cfg_mt68xx = {
	.host_ver = HOST_VERSION,
	.iova_bank = 0x300000000ULL,
	.iova_start = 0x70000000,
	.iova_heap = 0x80000000,
	.iova_end = 0x82600000,

	.bin_type = VPU_IMG_PRELOAD,  // VPU_IMG_LEGACY, VPU_IMG_PRELOAD

	.bin_sz_code = 0x2a10000,
	.bin_ofs_algo = 0xc00000,
	.bin_ofs_imem = (0x2a10000 - 0xc0000),
	.bin_ofs_header = (0x2a10000 - 0x30000),

	.cmd_timeout = 9000,
	.pw_off_latency_ms = 3000,

	.wait_cmd_latency_us = 2000,
	.wait_cmd_retry = 5,

	.wait_xos_latency_us = 500,
	.wait_xos_retry = 10,

	.xos = VPU_XOS,
	.xos_timeout = 1000000,
	.max_prio = VPU_MAX_PRIORITY,

	/* Log Buffer */
	.log_ofs = 0,
	.log_header_sz = 0x10,

	/* Dump: Sizes */
	.dmp_sz_reset = 0x400U,
	.dmp_sz_main = 0x40000U,    // 256 KB
	.dmp_sz_kernel = 0x20000U,  // 128 KB
	.dmp_sz_preload = 0x20000U, // 128 KB
	.dmp_sz_iram = 0x10000U,    // 64 KB
	.dmp_sz_work = 0x2000U,     // 8 KB
	.dmp_sz_reg =  0x1000U,     // 4 KB
	.dmp_sz_imem = 0x30000U,    // 192 KB
	.dmp_sz_dmem = 0x40000U,    // 256 KB

	/* Dump: Registers */
	.dmp_reg_cnt_info = 32,
	.dmp_reg_cnt_dbg = 8,
	.dmp_reg_cnt_mbox = 32,
};

// TODO: mt6779, mt6785 UT/IT
/* VPU config for MT6779, MT6785 */
struct vpu_config vpu_cfg_mt67xx = {
	.host_ver = HOST_VERSION,
	.iova_bank = 0,
	.iova_start = 0x70000000,
	.iova_heap = 0x70000000,
	.iova_end = 0x80000000,

	.bin_type = VPU_IMG_LEGACY,  // VPU_IMG_LEGACY, VPU_IMG_PRELOAD

	.bin_sz_code = 0x2a10000,
	.bin_ofs_algo = 0xc00000,
	.bin_ofs_imem = (0x2a10000 - 0xc0000),
	.bin_ofs_header = (0x2a10000 - 0x30000),

	.cmd_timeout = 9000,
	.pw_off_latency_ms = 3000,

	.wait_cmd_latency_us = 2000,
	.wait_cmd_retry = 5,

	.wait_xos_latency_us = 500,
	.wait_xos_retry = 10,

	.xos = VPU_NON_XOS,
	.xos_timeout = 1000000,
	.max_prio = 1,

	/* Log Buffer */
	.log_ofs = 0,
	.log_header_sz = 0x10,

	/* Dump: Sizes */
	.dmp_sz_reset = 0x400U,
	.dmp_sz_main = 0x40000U,    // 256 KB
	.dmp_sz_kernel = 0x20000U,  // 128 KB
	.dmp_sz_preload = 0x20000U, // 128 KB
	.dmp_sz_iram = 0x10000U,    // 64 KB
	.dmp_sz_work = 0x2000U,     // 8 KB
	.dmp_sz_reg =  0x1000U,     // 4 KB
	.dmp_sz_imem = 0x30000U,    // 192 KB
	.dmp_sz_dmem = 0x40000U,    // 256 KB

	/* Dump: Registers */
	.dmp_reg_cnt_info = 32,
	.dmp_reg_cnt_dbg = 8,
	.dmp_reg_cnt_mbox = 0,
};

struct vpu_misc_ops vpu_cops_mt6885 = {
	.emi_mpu_set = vpu_emi_mpu_set_mt68xx,
	.is_disabled = vpu_is_disabled_mt6885,
};

struct vpu_misc_ops vpu_cops_mt68xx = {
	.emi_mpu_set = vpu_emi_mpu_set_mt68xx,
	.is_disabled = vpu_is_disabled_dummy,
};

// TODO: implement misc ops for mt67xx
struct vpu_misc_ops vpu_cops_mt67xx = {
	.emi_mpu_set = vpu_emi_mpu_set_dummy,
	.is_disabled = vpu_is_disabled_dummy,
};


