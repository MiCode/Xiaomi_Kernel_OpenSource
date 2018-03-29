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

#include <linux/kernel.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797) || \
	defined(CONFIG_ARCH_MT6757) || defined(CONFIG_ARCH_ELBRUS)
#include <ddp_clkmgr.h>
#endif
#endif
#include <cmdq_record.h>
#include <ddp_reg.h>
#include <ddp_path.h>
#include <ddp_dither.h>
#include <ddp_drv.h>
#include <disp_drv_platform.h>
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#include <disp_helper.h>
#endif

#if defined(CONFIG_ARCH_ELBRUS) || defined(CONFIG_ARCH_MT6757)
#define DITHER0_BASE_NAMING (DISPSYS_DITHER0_BASE)
#define DITHER0_MODULE_NAMING (DISP_MODULE_DITHER0)
#else
#define DITHER0_BASE_NAMING (DISPSYS_DITHER_BASE)
#define DITHER0_MODULE_NAMING (DISP_MODULE_DITHER)
#endif

#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#define DITHER_SUPPORT_PARTIAL_UPDATE
#endif

int dither_dbg_en = 0;
#define DITHER_ERR(fmt, arg...) pr_err("[DITHER] " fmt "\n", ##arg)
#define DITHER_DBG(fmt, arg...) \
	do { if (dither_dbg_en) pr_debug("[DITHER] " fmt "\n", ##arg); } while (0)

#define DITHER_REG(reg_base, index) ((reg_base) + 0x100 + (index) * 4)

void disp_dither_init(disp_dither_id_t id, int width, int height,
			     unsigned int dither_bpp, void *cmdq)
{
	unsigned long reg_base = DITHER0_BASE_NAMING;
	unsigned int enable;

	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 5), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 6), 0x00003002, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 7), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 8), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 9), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 10), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 11), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 12), 0x00000011, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 13), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 14), 0x00000000, ~0);

	enable = 0x1;
	if (dither_bpp == 16) {	/* 565 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x50500001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x50504040, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp == 18) {	/* 666 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x40400001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x40404040, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp == 24) {	/* 888 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x20200001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x20202020, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp > 24) {
		DITHER_DBG("High depth LCM (bpp = %d), no dither\n", dither_bpp);
		enable = 1;
	} else {
		DITHER_DBG("Invalid dither bpp = %d\n", dither_bpp);
		/* Bypass dither */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000000, ~0);
		enable = 0;
	}

	DISP_REG_MASK(cmdq, DISP_REG_DITHER_EN, enable, 0x1);
	DISP_REG_MASK(cmdq, DISP_REG_DITHER_CFG, enable << 1, 1 << 1);
	/* Disable dither MODULE_STALL / SUB_MODULE_STALL  */
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
	DISP_REG_MASK(cmdq, DISP_REG_DITHER_CFG, 0 << 8, 1 << 8);
#endif
	DISP_REG_SET(cmdq, DISP_REG_DITHER_SIZE, (width << 16) | height);

#ifdef DISP_PLATFORM_HAS_SHADOW_REG
	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
		if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
			/* full shadow mode*/
			DISP_REG_MASK(cmdq, DISP_REG_DITHER_0, 0x0, 0x7);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 1) {
			/* force commit */
			DISP_REG_MASK(cmdq, DISP_REG_DITHER_0, 0x1 << 1, 0x7);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 2) {
			/* bypass shadow */
			DISP_REG_MASK(cmdq, DISP_REG_DITHER_0, 0x1, 0x7);
		}
	}
#endif

	DITHER_DBG("disp_dither_init bpp = %d, width = %d height = %d", dither_bpp, width, height);
}


static int disp_dither_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmdq)
{
	if (pConfig->dst_dirty) {
		disp_dither_init(DISP_DITHER0, pConfig->dst_w, pConfig->dst_h,
				 pConfig->lcm_bpp, cmdq);
	}

	return 0;
}


static int disp_dither_bypass(DISP_MODULE_ENUM module, int bypass)
{
	int relay = 0;

	if (bypass)
		relay = 1;

	DISP_REG_MASK(NULL, DISP_REG_DITHER_CFG, relay, 0x1);

	DITHER_DBG("disp_dither_bypass(bypass = %d)", bypass);

	return 0;
}


static int disp_dither_power_on(DISP_MODULE_ENUM module, void *handle)
{
#if defined(CONFIG_ARCH_MT6755)
	/* dither is DCM , do nothing */
#else
#ifdef ENABLE_CLK_MGR
	if (module == DITHER0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
		enable_clock(MT_CG_DISP0_DISP_DITHER, "DITHER");
#else
		ddp_clk_enable(DISP0_DISP_DITHER);
#endif
	}
#endif
#endif
	return 0;
}

static int disp_dither_power_off(DISP_MODULE_ENUM module, void *handle)
{
#if defined(CONFIG_ARCH_MT6755)
	/* dither is DCM , do nothing */
#else
#ifdef ENABLE_CLK_MGR
	if (module == DITHER0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
		disable_clock(MT_CG_DISP0_DISP_DITHER, "DITHER");
#else
		ddp_clk_disable(DISP0_DISP_DITHER);
#endif
	}
#endif
#endif
	return 0;
}

#ifdef DITHER_SUPPORT_PARTIAL_UPDATE
static int _dither_partial_update(DISP_MODULE_ENUM module, void *arg, void *cmdq)
{
	struct disp_rect *roi = (struct disp_rect *) arg;
	int width = roi->width;
	int height = roi->height;

	DISP_REG_SET(cmdq, DISP_REG_DITHER_SIZE, (width << 16) | height);
	return 0;
}

static int dither_ioctl(DISP_MODULE_ENUM module, void *handle,
		DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_dither_partial_update(module, params, handle);
		ret = 0;
	}
	return ret;
}
#endif

DDP_MODULE_DRIVER ddp_driver_dither = {
	.config = disp_dither_config,
	.bypass = disp_dither_bypass,
	.init = disp_dither_power_on,
	.deinit = disp_dither_power_off,
	.power_on = disp_dither_power_on,
	.power_off = disp_dither_power_off,
#ifdef DITHER_SUPPORT_PARTIAL_UPDATE
	.ioctl = dither_ioctl,
#endif
};


void disp_dither_select(unsigned int dither_bpp, void *cmdq)
{
	unsigned long reg_base = DITHER0_BASE_NAMING;
	unsigned int enable;

	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 5), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 6), 0x00003004, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 7), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 8), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 9), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 10), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 11), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 12), 0x00000011, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 13), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 14), 0x00000000, ~0);

	enable = 0x1;
	DISP_REG_MASK(cmdq, DISP_REG_DITHER_0, 0 << 4, 1 << 4);
	if (dither_bpp == 16) {	/* 565 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x50500001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x50504040, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp == 18) {	/* 666 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x40400001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x40404040, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp == 24) {	/* 888 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x20200001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x20202020, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp > 24) {
		DITHER_DBG("High depth LCM (bpp = %d), no dither\n", dither_bpp);
		enable = 1;
	} else {
		DITHER_DBG("Invalid dither bpp = %d\n", dither_bpp);
		/* Bypass dither */
		DISP_REG_MASK(cmdq, DISP_REG_DITHER_0, 1 << 4, 1 << 4);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000000, ~0);
		enable = 0;
	}
	DISP_REG_MASK(cmdq, DISP_REG_DITHER_EN, enable, 0x1);
	DISP_REG_MASK(cmdq, DISP_REG_DITHER_CFG, enable << 1, 1 << 1);
	DISP_REG_MASK(cmdq, DISP_REG_DITHER_CFG, 0x0, 0x1);
}


void disp_dither_dump(void)
{
}


void dither_test(const char *cmd, char *debug_output)
{
	debug_output[0] = '\0';
	DITHER_DBG("dither_test(%s)", cmd);

	if (strncmp(cmd, "log:", 4) == 0) {
		dither_dbg_en = (int)cmd[4];
		DITHER_DBG("dither dbg: %d", dither_dbg_en);
	} else if (strncmp(cmd, "sel:", 4) == 0) {
		if (cmd[4] == '0') {
			disp_dither_select(0, NULL);
			DITHER_DBG("bbp=0");
		} else if (cmd[4] == '1') {
			disp_dither_select(16, NULL);
			DITHER_DBG("bbp=16");
		} else if (cmd[4] == '2') {
			disp_dither_select(18, NULL);
			DITHER_DBG("bbp=18");
		} else if (cmd[4] == '3') {
			disp_dither_select(24, NULL);
			DITHER_DBG("bbp=24");
		} else {
			DITHER_DBG("Unknown bbp");
		}
	}
}
