/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/types.h>
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

/*
 * CONFIG_FPGA_EARLY_PORTING is used in linux kernel for early porting.
 * if the macro name changed, please modify the code here too.
 */
#ifdef CONFIG_FPGA_EARLY_PORTING
static unsigned int disp_global_stage =
				MAGIC_CODE | DISP_HELPER_STAGE_EARLY_PORTING;
#else

/* please change this to DISP_HELPER_STAGE_NORMAL after bring up done */

/* static unsigned int disp_global_stage =
 * MAGIC_CODE | DISP_HELPER_STAGE_BRING_UP;
 */
static unsigned int disp_global_stage = MAGIC_CODE | DISP_HELPER_STAGE_NORMAL;
#endif /* CONFIG_FPGA_EARLY_PORTING */

static unsigned int _is_early_porting_stage(void)
{
	return (disp_global_stage & (~MAGIC_CODE)) ==
						DISP_HELPER_STAGE_EARLY_PORTING;
}

static unsigned int _is_bringup_stage(void)
{
	return (disp_global_stage & (~MAGIC_CODE)) ==
						DISP_HELPER_STAGE_BRING_UP;
}

static unsigned int _is_normal_stage(void)
{
	return (disp_global_stage & (~MAGIC_CODE)) == DISP_HELPER_STAGE_NORMAL;
}

static struct {
	enum DISP_HELPER_OPT opt;
	unsigned int val;
	const char *desc;
} help_info[] = {
	{DISP_OPT_USE_CMDQ, 0, "DISP_OPT_USE_CMDQ"}, /* must enable */
	{DISP_OPT_USE_M4U, 0, "DISP_OPT_USE_M4U"},   /* must enable */
	/* not use now */
	{DISP_OPT_MIPITX_ON_CHIP, 0, "DISP_OPT_MIPITX_ON_CHIP"},
	/* not use now */
	{DISP_OPT_USE_DEVICE_TREE, 0, "DISP_OPT_USE_DEVICE_TREE"},
	{DISP_OPT_FAKE_LCM_X, 0, "DISP_OPT_FAKE_LCM_X"},
	{DISP_OPT_FAKE_LCM_Y, 0, "DISP_OPT_FAKE_LCM_Y"},
	{DISP_OPT_FAKE_LCM_WIDTH, 0, "DISP_OPT_FAKE_LCM_WIDTH"},
	{DISP_OPT_FAKE_LCM_HEIGHT, 0, "DISP_OPT_FAKE_LCM_HEIGHT"},
	/* not use now */
	{DISP_OPT_OVL_WARM_RESET, 0, "DISP_OPT_OVL_WARM_RESET"},
	/* not use now */
	{DISP_OPT_DYNAMIC_SWITCH_UNDERFLOW_EN, 0,
			"DISP_OPT_DYNAMIC_SWITCH_UNDERFLOW_EN"},

	/* low power option start */
	{DISP_OPT_SODI_SUPPORT, 0, "DISP_OPT_SODI_SUPPORT"},
	{DISP_OPT_IDLE_MGR, 0, "DISP_OPT_IDLE_MGR"},
	{DISP_OPT_IDLEMGR_SWTCH_DECOUPLE, 0, "DISP_OPT_IDLEMGR_SWTCH_DECOUPLE"},
	{DISP_OPT_IDLEMGR_BY_REPAINT, 0, "DISP_OPT_IDLEMGR_BY_REPAINT"},
	{DISP_OPT_IDLEMGR_ENTER_ULPS, 0, "DISP_OPT_IDLEMGR_ENTER_ULPS"},
	{DISP_OPT_IDLEMGR_KEEP_LP11, 0, "DISP_OPT_IDLEMGR_KEEP_LP11"},
	{DISP_OPT_SHARE_SRAM, 0, "DISP_OPT_SHARE_SRAM"},
	{DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK, 0,
			"DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK"},
	{DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING, 0,
			"DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING"},
	{DISP_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ, 0,
			"DISP_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ"},
	{DISP_OPT_MET_LOG, 0, "DISP_OPT_MET_LOG"},
	/* low power option end */

	{DISP_OPT_DECOUPLE_MODE_USE_RGB565, 0,
			"DISP_OPT_DECOUPLE_MODE_USE_RGB565"}, /* not use now */
	{DISP_OPT_NO_LCM_FOR_LOW_POWER_MEASUREMENT, 0,
			"DISP_OPT_NO_LCM_FOR_LOW_POWER_MEASUREMENT"},
	{DISP_OPT_NO_LK, 0, "DISP_OPT_NO_LK"}, /* not use now */
	{DISP_OPT_USE_PQ, 0, "DISP_OPT_USE_PQ"},
	{DISP_OPT_ESD_CHECK_RECOVERY, 0, "DISP_OPT_ESD_CHECK_RECOVERY"},
	{DISP_OPT_ESD_CHECK_SWITCH, 0, "DISP_OPT_ESD_CHECK_SWITCH"},
	{DISP_OPT_PRESENT_FENCE, 1, "DISP_OPT_PRESENT_FENCE"},
	{DISP_OPT_PERFORMANCE_DEBUG, 0, "DISP_OPT_PERFORMANCE_DEBUG"},
	{DISP_OPT_SWITCH_DST_MODE, 0, "DISP_OPT_SWITCH_DST_MODE"},
	{DISP_OPT_MUTEX_EOF_EN_FOR_CMD_MODE, 1,
			"DISP_OPT_MUTEX_EOF_EN_FOR_CMD_MODE"},
	{DISP_OPT_SCREEN_CAP_FROM_DITHER, 0, "DISP_OPT_SCREEN_CAP_FROM_DITHER"},
	{DISP_OPT_BYPASS_OVL, 0, "DISP_OPT_BYPASS_OVL"},
	{DISP_OPT_FPS_CALC_WND, 10, "DISP_OPT_FPS_CALC_WND"},
	{DISP_OPT_FPS_EXT, 0, "DISP_OPT_FPS_EXT"},
	{DISP_OPT_FPS_EXT_INTERVAL, 0, "DISP_OPT_FPS_EXT_INTERVAL"},
	{DISP_OPT_SMART_OVL, 0, "DISP_OPT_SMART_OVL"},
	{DISP_OPT_DYNAMIC_DEBUG, 0, "DISP_OPT_DYNAMIC_DEBUG"}, /* not use now */
	{DISP_OPT_SHOW_VISUAL_DEBUG_INFO, 0, "DISP_OPT_SHOW_VISUAL_DEBUG_INFO"},
	{DISP_OPT_RDMA_UNDERFLOW_AEE, 0, "DISP_OPT_RDMA_UNDERFLOW_AEE"},
	{DISP_OPT_HRT, 1, "DISP_OPT_HRT"},

	{DISP_OPT_PARTIAL_UPDATE, 0, "DISP_OPT_PARTIAL_UPDATE"},
	{DISP_OPT_CV_BYSUSPEND, 0, "DISP_OPT_CV_BYSUSPEND"},
	{DISP_OPT_DELAYED_TRIGGER, 0, "DISP_OPT_DELAYED_TRIGGER"},
	{DISP_OPT_SHADOW_REGISTER, 0, "DISP_OPT_SHADOW_REGISTER"},
	{DISP_OPT_SHADOW_MODE, 0, "DISP_OPT_SHADOW_MODE"},
	{DISP_OPT_OVL_EXT_LAYER, 0, "DISP_OPT_OVL_EXT_LAYER"},
	{DISP_OPT_REG_PARSER_RAW_DUMP, 0, "DISP_OPT_REG_PARSER_RAW_DUMP"},
	{DISP_OPT_PQ_REG_DUMP, 0, "DISP_OPT_PQ_REG_DUMP"},
	{DISP_OPT_AOD, 0, "DISP_OPT_AOD"},
	{DISP_OPT_ARR_PHASE_1, 0, "DISP_OPT_ARR_PHASE_1"},
	{DISP_OPT_RSZ, 0, "DISP_OPT_RSZ"},
	{DISP_OPT_RPO, 0, "DISP_OPT_RPO"},
	{DISP_OPT_DUAL_PIPE, 0, "DISP_OPT_DUAL_PIPE"},
	{DISP_OPT_SHARE_WDMA0, 0, "DISP_OPT_SHARE_WDMA0"},
	{DISP_OPT_ROUND_CORNER, 0, "DISP_OPT_ROUND_CORNER"},
	{DISP_OPT_ROUND_CORNER_MODE, DISP_HELPER_SW_RC,
		"DISP_OPT_ROUND_CORNER_MODE"},
	{DISP_OPT_FRAME_QUEUE, 0, "DISP_OPT_FRAME_QUEUE"},
	{DISP_OPT_DC_BY_HRT, 0, "DISP_OPT_DC_BY_HRT"},
	{DISP_OPT_OVL_WCG, 0, "DISP_OPT_OVL_WCG"},
	{DISP_OPT_OVL_SBCH, 0, "DISP_OPT_OVL_SBCH"},
	{DISP_OPT_MMPATH, 0, "DISP_OPT_MMPATH"},
	{DISP_OPT_TUI_MODE, 0, "DISP_OPT_TUI_MODE"},
	{DISP_OPT_LCM_HBM, 0, "DISP_OPT_LCM_HBM"},
	/*DynFPS*/
	{DISP_OPT_DYNAMIC_FPS, 0, "DISP_OPT_DYNAMIC_FPS"},
};

const char *disp_helper_option_spy(enum DISP_HELPER_OPT option)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(help_info); i++) {
		if (help_info[i].opt == option)
			return help_info[i].desc;
	}

	return "unknown option!!";
}

enum DISP_HELPER_OPT disp_helper_name_to_opt(const char *name)
{
	int i;

	for (i = 0; i < DISP_OPT_NUM; i++) {
		const char *opt_name = disp_helper_option_spy(i);

		if (strcmp(name, opt_name) == 0)
			return i;
	}
	DISP_PR_INFO("%s: unknown name: %s\n", __func__, name);
	return DISP_OPT_NUM;
}

int disp_helper_set_option(enum DISP_HELPER_OPT option, int value)
{
	int ret;
	unsigned int i;

	if (option == DISP_OPT_FPS_CALC_WND) {
		ret = primary_fps_ctx_set_wnd_sz(value);
		if (ret) {
			DISP_PR_INFO("%s error to set fps_wnd_sz to %d\n",
				     __func__, value);
			return ret;
		}
	}

	if (option >= DISP_OPT_NUM) {
		DISP_PR_INFO("wrong option: %d\n", option);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(help_info); i++) {
		if (help_info[i].opt == option && help_info[i].val != value) {
			DISPCHECK("Set Option %d(%s) from (%d) to (%d)\n",
				  option, disp_helper_option_spy(option),
				  disp_helper_get_option(option), value);

			help_info[i].val = value;
		}
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
		DISP_PR_INFO("%s: option invalid %d\n", __func__, option);
		return -1;
	}

	switch (option) {
	case DISP_OPT_MIPITX_ON_CHIP:
	{
		if (_is_normal_stage())
			return 1;
		else if (_is_bringup_stage())
			return 1;
		else if (_is_early_porting_stage())
			return 0;

		DISP_PR_INFO("%s,get option MIPITX fail\n", __FILE__);
		return -1;
	}
	case DISP_OPT_FAKE_LCM_X:
	{
		int x = 0;

#ifdef CONFIG_CUSTOM_LCM_X
		ret = kstrtoint(CONFIG_CUSTOM_LCM_X, 0, &x);
		if (ret) {
			pr_err("%s error to parse x: %s\n",
			       __func__, CONFIG_CUSTOM_LCM_X);
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
			pr_err("%s error to parse x: %s\n",
			       __func__, CONFIG_CUSTOM_LCM_Y);
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
		return 1;/*avoid error with no break when default case*/
	}
	case DISP_OPT_SWITCH_DST_MODE:
	{
		if (_is_normal_stage())
			return 0;
		else if (_is_bringup_stage())
			return 0;
		else if (_is_early_porting_stage())
			return 0;
		else
			return 0;
	}
	default:
	{
		unsigned int i;

		for (i = 0; i < ARRAY_SIZE(help_info); i++) {
			if (help_info[i].opt == option)
				return help_info[i].val;
		}

		return 0;
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

	/*
	 * test solution for 6795 rdma underflow caused by ufoe LR mode
	 * (ufoe fifo is larger than rdma)
	 */
	disp_helper_set_option(DISP_OPT_DYNAMIC_SWITCH_UNDERFLOW_EN, 0);

	/* warm reset ovl before each trigger for cmd mode */
	disp_helper_set_option(DISP_OPT_OVL_WARM_RESET, 0);

	/* ================ Begin: lowpower option setting ================ */
	disp_helper_set_option(DISP_OPT_SODI_SUPPORT, 0);
	disp_helper_set_option(DISP_OPT_IDLE_MGR, 1);

	/* 1. vdo mode + screen idle(need idlemgr) */
	disp_helper_set_option(DISP_OPT_IDLEMGR_SWTCH_DECOUPLE,	1);
	disp_helper_set_option(DISP_OPT_IDLEMGR_BY_REPAINT, 1);
	disp_helper_set_option(DISP_OPT_SHARE_SRAM, 1);
	disp_helper_set_option(DISP_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ, 1);

	/* 2. cmd mode + screen idle(need idlemgr) */
	disp_helper_set_option(DISP_OPT_IDLEMGR_ENTER_ULPS, 1);
	disp_helper_set_option(DISP_OPT_IDLEMGR_KEEP_LP11, 0);

	/* 3. cmd mode + vdo mode */
	disp_helper_set_option(DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK, 0);
	disp_helper_set_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING, 1);

	disp_helper_set_option(DISP_OPT_MET_LOG, 0);
	/* ================ End: lowpower option setting ================== */

	disp_helper_set_option(DISP_OPT_PRESENT_FENCE, 1);

	/* use fake vsync timer for low power measurement */
	disp_helper_set_option(DISP_OPT_NO_LCM_FOR_LOW_POWER_MEASUREMENT, 0);

	/* use RGB565 format for decouple mode intermediate buffer */
	disp_helper_set_option(DISP_OPT_DECOUPLE_MODE_USE_RGB565, 0);

	disp_helper_set_option(DISP_OPT_USE_PQ, 1);
	disp_helper_set_option(DISP_OPT_MUTEX_EOF_EN_FOR_CMD_MODE, 1);
	disp_helper_set_option(DISP_OPT_ESD_CHECK_RECOVERY, 1);
	disp_helper_set_option(DISP_OPT_ESD_CHECK_SWITCH, 1);

	disp_helper_set_option(DISP_OPT_BYPASS_OVL, 0);
	disp_helper_set_option(DISP_OPT_FPS_CALC_WND, 10);
	/* report external fps statistics */
	disp_helper_set_option(DISP_OPT_FPS_EXT, 0);
	/* set external fps interval (ms) */
	disp_helper_set_option(DISP_OPT_FPS_EXT_INTERVAL, 1000);
	disp_helper_set_option(DISP_OPT_SMART_OVL, 0);
	disp_helper_set_option(DISP_OPT_DYNAMIC_DEBUG, 0);
	disp_helper_set_option(DISP_OPT_HRT, 1);

	/* display partial update */
#ifdef CONFIG_MTK_CONSUMER_PARTIAL_UPDATE_SUPPORT
	disp_helper_set_option(DISP_OPT_PARTIAL_UPDATE, 0);
#endif
	disp_helper_set_option(DISP_OPT_CV_BYSUSPEND, 0);
	disp_helper_set_option(DISP_OPT_DELAYED_TRIGGER, 1);
	disp_helper_set_option(DISP_OPT_SHADOW_REGISTER, 0);
	disp_helper_set_option(DISP_OPT_SHADOW_MODE, 0);

	/* smart layer OVL */
	disp_helper_set_option(DISP_OPT_OVL_EXT_LAYER, 1);

	disp_helper_set_option(DISP_OPT_REG_PARSER_RAW_DUMP, 1);
	disp_helper_set_option(DISP_OPT_PQ_REG_DUMP, 0);

	disp_helper_set_option(DISP_OPT_AOD, 1);

	/* ARR phase 1 option */
	disp_helper_set_option(DISP_OPT_ARR_PHASE_1, 0);

	disp_helper_set_option(DISP_OPT_RSZ, 0);
	disp_helper_set_option(DISP_OPT_RPO, 1);
	disp_helper_set_option(DISP_OPT_DUAL_PIPE, 0);
	disp_helper_set_option(DISP_OPT_SHARE_WDMA0, 0);
	disp_helper_set_option(DISP_OPT_ROUND_CORNER, 1);
	disp_helper_set_option(DISP_OPT_ROUND_CORNER_MODE, DISP_HELPER_HW_RC);
	disp_helper_set_option(DISP_OPT_FRAME_QUEUE, 0);
	disp_helper_set_option(DISP_OPT_DC_BY_HRT, 0);
	disp_helper_set_option(DISP_OPT_OVL_WCG, 1);
	/* OVL SBCH */
	disp_helper_set_option(DISP_OPT_OVL_SBCH, 1);
	disp_helper_set_option(DISP_OPT_MMPATH, 0);
	disp_helper_set_option(DISP_OPT_TUI_MODE, 0);
	disp_helper_set_option(DISP_OPT_LCM_HBM, 0);
	/*DynFPS*/
	disp_helper_set_option(DISP_OPT_DYNAMIC_FPS, 1);
}

int disp_helper_get_option_list(char *stringbuf, int buf_len)
{
	int len = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(help_info); i++) {
		if (stringbuf != NULL && buf_len > 0)
			len += scnprintf(stringbuf + len, buf_len - len,
					 "Option: [%d][%s] Value: [%d]\n",
					 i, disp_helper_option_spy(i),
					 disp_helper_get_option(i));
	}

	return len;
}
