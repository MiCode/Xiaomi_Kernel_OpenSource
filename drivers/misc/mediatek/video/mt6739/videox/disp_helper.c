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

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "disp_drv_log.h"
#include "primary_display.h"

#include "mtk_boot.h"
#include "disp_helper.h"
#include "disp_drv_platform.h"
#include "primary_display.h"
#include "mt-plat/mtk_chip.h"

/* use this magic_code to detect memory corruption */
#define MAGIC_CODE 0xDEADAAA0U

/* CONFIG_MTK_FPGA is used in linux kernel for early porting. */
/* if the macro name changed, please modify the code here too. */

#ifdef CONFIG_FPGA_EARLY_PORTING
static unsigned int disp_global_stage = MAGIC_CODE | DISP_HELPER_STAGE_EARLY_PORTING;
#else
/* please change this to DISP_HELPER_STAGE_NORMAL after bring up done */
/*static unsigned int disp_global_stage = MAGIC_CODE | DISP_HELPER_STAGE_BRING_UP;*/
static unsigned int disp_global_stage = MAGIC_CODE | DISP_HELPER_STAGE_NORMAL;
#endif

#if 0 /* defined but not used */
static int _is_E1(void)
{
	unsigned int ver = mt_get_chip_sw_ver();

	if (ver == CHIP_SW_VER_01)
		return 1;

	return 0;
}

static int _is_E2(void)
{
	unsigned int ver = mt_get_chip_sw_ver();

	if (ver == CHIP_SW_VER_02)
		return 1;

	return 0;
}

static int _is_E3(void)
{
	return !(_is_E1() || _is_E2());
}
#endif

static unsigned int _is_early_porting_stage(void)
{
	return (disp_global_stage & (~MAGIC_CODE)) == DISP_HELPER_STAGE_EARLY_PORTING;
}

static unsigned int _is_bringup_stage(void)
{
	return (disp_global_stage & (~MAGIC_CODE)) == DISP_HELPER_STAGE_BRING_UP;
}

static unsigned int _is_normal_stage(void)
{
	return (disp_global_stage & (~MAGIC_CODE)) == DISP_HELPER_STAGE_NORMAL;
}

static int _disp_helper_option_value[DISP_OPT_NUM] = { 0 };

const char *disp_helper_option_string[DISP_OPT_NUM] = {
	"DISP_OPT_USE_CMDQ",
	"DISP_OPT_USE_M4U",
	"DISP_OPT_MIPITX_ON_CHIP",
	"DISP_OPT_USE_DEVICE_TREE",
	"DISP_OPT_FAKE_LCM_X",
	"DISP_OPT_FAKE_LCM_Y",
	"DISP_OPT_FAKE_LCM_WIDTH",
	"DISP_OPT_FAKE_LCM_HEIGHT",
	"DISP_OPT_OVL_WARM_RESET",
	"DISP_OPT_DYNAMIC_SWITCH_UNDERFLOW_EN",
	/* Begin: lowpower option*/
	"DISP_OPT_SODI_SUPPORT",
	"DISP_OPT_IDLE_MGR",
	"DISP_OPT_IDLEMGR_SWTCH_DECOUPLE",
	"DISP_OPT_IDLEMGR_ENTER_ULPS",
	"DISP_OPT_SHARE_SRAM",
	"DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK",
	"DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING",
	"DISP_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ",
	"DISP_OPT_MET_LOG", /* for met */
	/* End: lowpower option */
	"DISP_OPT_DECOUPLE_MODE_USE_RGB565",
	"DISP_OPT_NO_LCM_FOR_LOW_POWER_MEASUREMENT",
	"DISP_OPT_NO_LK",
	"DISP_OPT_BYPASS_PQ",
	"DISP_OPT_ESD_CHECK_RECOVERY",
	"DISP_OPT_ESD_CHECK_SWITCH",
	"DISP_OPT_PRESENT_FENCE",
	"DISP_OPT_PERFORMANCE_DEBUG",
	"DISP_OPT_SWITCH_DST_MODE",
	"DISP_OPT_MUTEX_EOF_EN_FOR_CMD_MODE",
	"DISP_OPT_SCREEN_CAP_FROM_DITHER",
	"DISP_OPT_BYPASS_OVL",
	"DISP_OPT_FPS_CALC_WND",
	"DISP_OPT_SMART_OVL",
	"DISP_OPT_DYNAMIC_DEBUG",
	"DISP_OPT_SHOW_VISUAL_DEBUG_INFO",
	"DISP_OPT_RDMA_UNDERFLOW_AEE",
	"DISP_OPT_HRT",
	"DISP_OPT_PARTIAL_UPDATE",
	"DISP_OPT_CV_BYSUSPEND",
	"DISP_OPT_DELAYED_TRIGGER",
	"DISP_OPT_SHADOW_REGISTER",
	"DISP_OPT_SHADOW_MODE",
	"DISP_OPT_OVL_EXT_LAYER",
	"DISP_OPT_REG_PARSER_RAW_DUMP",
	"DISP_OPT_AOD",
	"DISP_OPT_RSZ",
	"DISP_OPT_DUAL_PIPE",
	"DISP_OPT_ARR_PHASE_1",
	"DISP_OPT_GMO_OPTIMIZE",
	"DISP_OPT_MIRROR_MODE_FROCE_DISABLE_SODI",
};

const char *disp_helper_option_spy(enum DISP_HELPER_OPT option)
{
	if (option >= DISP_OPT_NUM)
		return "unknown option!!";
	return disp_helper_option_string[option];
}

enum DISP_HELPER_OPT disp_helper_name_to_opt(const char *name)
{
	int i;

	for (i = 0; i < DISP_OPT_NUM; i++) {
		const char *opt_name = disp_helper_option_spy(i);

		if (strcmp(name, opt_name) == 0)
			return i;
	}
	DISPERR("%s: unknown name: %s\n", __func__, name);
	return DISP_OPT_NUM;
}

int disp_helper_set_option(enum DISP_HELPER_OPT option, int value)
{
	int ret;

	if (option == DISP_OPT_FPS_CALC_WND) {
		ret = primary_fps_ctx_set_wnd_sz(value);
		if (ret) {
			DISPERR("%s error to set fps_wnd_sz to %d\n", __func__, value);
			return ret;
		}
	}

	if (option < DISP_OPT_NUM) {
		DISPCHECK("Set Option %d(%s) from (%d) to (%d)\n", option,
			  disp_helper_option_spy(option), disp_helper_get_option(option), value);
		_disp_helper_option_value[option] = value;
		DISPCHECK("After set (%s) is (%d)\n", disp_helper_option_spy(option),
			  disp_helper_get_option(option));
	} else {
		DISPERR("Wrong option: %d\n", option);
	}
	return 0;
}

int disp_helper_set_option_by_name(const char *name, int value)
{
	enum DISP_HELPER_OPT opt;

	opt = disp_helper_name_to_opt(name);
	if (opt >= DISP_OPT_NUM)
		return -1;

	return disp_helper_set_option(opt, value);
}

int disp_helper_get_option(enum DISP_HELPER_OPT option)
{
	int ret = 0;

	if (option >= DISP_OPT_NUM) {
		DISPERR("%s: option invalid %d\n", __func__, option);
		return -1;
	}

	switch (option) {
	case DISP_OPT_USE_CMDQ:
	case DISP_OPT_SHOW_VISUAL_DEBUG_INFO:
		{
			return _disp_helper_option_value[option];
		}
	case DISP_OPT_USE_M4U:
		{
			return _disp_helper_option_value[option];
		}
	case DISP_OPT_MIPITX_ON_CHIP:
		{
			if (_is_normal_stage())
				return 1;
			else if (_is_bringup_stage())
				return 1;
			else if (_is_early_porting_stage())
				return 0;

			DISPERR("%s,get option MIPITX fail\n", __FILE__);
			return -1;
		}
	case DISP_OPT_FAKE_LCM_X:
		{
			int x = 0;

#ifdef CONFIG_CUSTOM_LCM_X
			ret = kstrtoint(CONFIG_CUSTOM_LCM_X, 0, &x);
			if (ret) {
				DISPERR("%s error to parse x: %s\n", __func__, CONFIG_CUSTOM_LCM_X);
				x = 0;
			}
#endif
			return x;
		}
	case DISP_OPT_FAKE_LCM_Y:
		{
			int y = 0;

#ifdef CONFIG_CUSTOM_LCM_Y
			ret = kstrtoint(CONFIG_CUSTOM_LCM_Y, 0, &y);
			if (ret) {
				DISPERR("%s error to parse x: %s\n", __func__, CONFIG_CUSTOM_LCM_Y);
				y = 0;
			}

#endif
			return y;
		}
	case DISP_OPT_FAKE_LCM_WIDTH:
		{
			int w = primary_display_get_virtual_width();

			if (w == 0)
				w = DISP_GetScreenWidth();
			return w;
		}
	case DISP_OPT_FAKE_LCM_HEIGHT:
		{
			int h = primary_display_get_virtual_height();

			if (h == 0)
				h = DISP_GetScreenHeight();
			return h;
		}
	case DISP_OPT_OVL_WARM_RESET:
		{
			return 0;
		}
	case DISP_OPT_NO_LK:
		{
			return 1;
		}
	case DISP_OPT_PERFORMANCE_DEBUG:
		{
			if (_is_normal_stage())
				return 0;
			else if (_is_bringup_stage())
				return 0;
			else if (_is_early_porting_stage())
				return 0;
		}
	case DISP_OPT_SWITCH_DST_MODE:
		{
			if (_is_normal_stage())
				return 0;
			else if (_is_bringup_stage())
				return 0;
			else if (_is_early_porting_stage())
				return 0;
		}
	default:
		{
			return _disp_helper_option_value[option];
		}
	}

	return ret;
}

enum DISP_HELPER_STAGE disp_helper_get_stage(void)
{
	return disp_global_stage & (~MAGIC_CODE);
}

const char *disp_helper_stage_spy(void)
{
	if (disp_helper_get_stage() == DISP_HELPER_STAGE_EARLY_PORTING)
		return "EARLY_PORTING";
	else if (disp_helper_get_stage() == DISP_HELPER_STAGE_BRING_UP)
		return "BRINGUP";
	else if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL)
		return "NORMAL";

	return "UNKNOWN";
}

void disp_helper_option_init(void)
{
	disp_helper_set_option(DISP_OPT_USE_CMDQ, 1);
	disp_helper_set_option(DISP_OPT_USE_M4U, 1);

	/* test solution for 6795 rdma underflow caused by ufoe LR mode(ufoe fifo is larger than rdma) */
	disp_helper_set_option(DISP_OPT_DYNAMIC_SWITCH_UNDERFLOW_EN, 0);

	/* warm reset ovl before each trigger for cmd mode */
	disp_helper_set_option(DISP_OPT_OVL_WARM_RESET, 0);

	/* ===================Begin: lowpower option setting==================== */
	disp_helper_set_option(DISP_OPT_SODI_SUPPORT, 1);
	disp_helper_set_option(DISP_OPT_IDLE_MGR, 1);

	/* 1. vdo mode + screen idle(need idlemgr) */
	disp_helper_set_option(DISP_OPT_IDLEMGR_SWTCH_DECOUPLE,	1);
	disp_helper_set_option(DISP_OPT_SHARE_SRAM,	1);
	disp_helper_set_option(DISP_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ, 1);

	/* 2. cmd mode + screen idle(need idlemgr) */
	disp_helper_set_option(DISP_OPT_IDLEMGR_ENTER_ULPS,	0);

	/* 3. cmd mode + vdo mode */
	disp_helper_set_option(DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK, 0);
	disp_helper_set_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING, 1);

	disp_helper_set_option(DISP_OPT_MET_LOG, 0);
	/* ===================End: lowpower option setting==================== */

	disp_helper_set_option(DISP_OPT_PRESENT_FENCE, 1);

	/* use fake vsync timer for low power measurement */
	disp_helper_set_option(DISP_OPT_NO_LCM_FOR_LOW_POWER_MEASUREMENT, 0);

	/* use RGB565 format for decouple mode intermediate buffer */
	disp_helper_set_option(DISP_OPT_DECOUPLE_MODE_USE_RGB565, 0);

	disp_helper_set_option(DISP_OPT_BYPASS_PQ, 0);
	disp_helper_set_option(DISP_OPT_MUTEX_EOF_EN_FOR_CMD_MODE, 0);
	disp_helper_set_option(DISP_OPT_ESD_CHECK_RECOVERY, 1);
	disp_helper_set_option(DISP_OPT_ESD_CHECK_SWITCH, 1);

	disp_helper_set_option(DISP_OPT_BYPASS_OVL, 1);
	disp_helper_set_option(DISP_OPT_FPS_CALC_WND, 10);
	disp_helper_set_option(DISP_OPT_SMART_OVL, 0);
	disp_helper_set_option(DISP_OPT_DYNAMIC_DEBUG, 0);
	disp_helper_set_option(DISP_OPT_HRT, 1);

	/* display partial update */
#ifdef CONFIG_MTK_CONSUMER_PARTIAL_UPDATE_SUPPORT
	disp_helper_set_option(DISP_OPT_PARTIAL_UPDATE, 0);
#endif
	disp_helper_set_option(DISP_OPT_CV_BYSUSPEND, 0);
	disp_helper_set_option(DISP_OPT_DELAYED_TRIGGER, 0);
	disp_helper_set_option(DISP_OPT_SHADOW_REGISTER, 0);
	disp_helper_set_option(DISP_OPT_SHADOW_MODE, 0);

	/* smart layer OVL*/
	disp_helper_set_option(DISP_OPT_OVL_EXT_LAYER, 1);

	disp_helper_set_option(DISP_OPT_REG_PARSER_RAW_DUMP, 0);

	disp_helper_set_option(DISP_OPT_AOD, 0);
	disp_helper_set_option(DISP_OPT_RSZ, 0);
	disp_helper_set_option(DISP_OPT_DUAL_PIPE, 0);

	/* ARR phase 1 option*/
	disp_helper_set_option(DISP_OPT_ARR_PHASE_1, 0);

	disp_helper_set_option(DISP_OPT_GMO_OPTIMIZE, 1);
	disp_helper_set_option(DISP_OPT_MIRROR_MODE_FROCE_DISABLE_SODI, 0);
}

int disp_helper_get_option_list(char *stringbuf, int buf_len)
{
	int len = 0;
	int i = 0;

	for (i = 0; i < DISP_OPT_NUM; i++) {
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "Option: [%d][%s] Value: [%d]\n", i,
			      disp_helper_option_spy(i), disp_helper_get_option(i));
	}

	return len;
}
