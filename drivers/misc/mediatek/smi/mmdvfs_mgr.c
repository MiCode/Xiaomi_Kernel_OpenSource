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

#if ((defined(SMI_D1) || defined(SMI_D2) || defined(SMI_D3)) && !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING))
#define MMDVFS_ENABLE 1
#endif

#include <linux/uaccess.h>
#include <aee.h>

#include <mt_smi.h>

#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <mach/mt_freqhopping.h>
#include <mach/mt_clkmgr.h>
#include <mt_vcore_dvfs.h>
#include <mt_freqhopping_drv.h>


#include "mmdvfs_mgr.h"

#undef pr_fmt
#define pr_fmt(fmt) "[" MMDVFS_LOG_TAG "]" fmt

#define MMDVFS_ENABLE_FLIPER_CONTROL    0
/* #define MMDVFS_USE_APMCU_CLK_MUX_SWITCH */

#if MMDVFS_ENABLE_FLIPER_CONTROL
#include <mach/fliper.h>
#endif

/* mmdvfs MM sizes */
#define MMDVFS_PIXEL_NUM_720P   (1280 * 720)
#define MMDVFS_PIXEL_NUM_2160P  (3840 * 2160)
#define MMDVFS_PIXEL_NUM_1080P  (2100 * 1300)
#define MMDVFS_PIXEL_NUM_2M     (2100 * 1300)
/* 13M sensor */
#define MMDVFS_PIXEL_NUM_SENSOR_FULL    (13000000)
#define MMDVFS_PIXEL_NUM_SENSOR_6M  (5800000)
#define MMDVFS_PIXEL_NUM_SENSOR_8M  (7800000)

/* mmdvfs display sizes */
#define MMDVFS_DISPLAY_SIZE_HD  (1280 * 832)
#define MMDVFS_DISPLAY_SIZE_FHD (1920 * 1216)

/* + 1 for MMDVFS_CAM_MON_SCEN */
static mmdvfs_voltage_enum g_mmdvfs_scenario_voltage[MMDVFS_SCEN_COUNT] = {
MMDVFS_VOLTAGE_DEFAULT};
static mmdvfs_voltage_enum g_mmdvfs_current_step;
static unsigned int g_mmdvfs_concurrency;
static MTK_SMI_BWC_MM_INFO *g_mmdvfs_info;
static int g_mmdvfs_profile_id = MMDVFS_PROFILE_UNKNOWN;
static MTK_MMDVFS_CMD g_mmdvfs_cmd;

struct mmdvfs_context_struct {
	spinlock_t scen_lock;
	int is_mhl_enable;
	int is_mjc_enable;
};

/* mmdvfs_query() return value, remember to sync with user space */
enum mmdvfs_step_enum {
	MMDVFS_STEP_LOW = 0, MMDVFS_STEP_HIGH,

	MMDVFS_STEP_LOW2LOW, /* LOW */
	MMDVFS_STEP_HIGH2LOW, /* LOW */
	MMDVFS_STEP_LOW2HIGH, /* HIGH */
	MMDVFS_STEP_HIGH2HIGH,
/* HIGH */
};

/* lcd size */
enum mmdvfs_lcd_size_enum {
	MMDVFS_LCD_SIZE_HD, MMDVFS_LCD_SIZE_FHD, MMDVFS_LCD_SIZE_WQHD, MMDVFS_LCD_SIZE_END_OF_ENUM
};

static struct mmdvfs_context_struct g_mmdvfs_mgr_cntx;
static struct mmdvfs_context_struct * const g_mmdvfs_mgr = &g_mmdvfs_mgr_cntx;

static enum mmdvfs_lcd_size_enum mmdvfs_get_lcd_resolution(void)
{
	if (DISP_GetScreenWidth() * DISP_GetScreenHeight()
	<= MMDVFS_DISPLAY_SIZE_HD)
		return MMDVFS_LCD_SIZE_HD;

	return MMDVFS_LCD_SIZE_FHD;
}

static int vdec_ctrl_func_checked(vdec_ctrl_cb func, char *msg);
static int notify_cb_func_checked(clk_switch_cb func, int ori_mmsys_clk_mode, int update_mmsys_clk_mode, char *msg);
static int mmdfvs_adjust_mmsys_clk_by_hopping(int clk_mode);
static int default_clk_switch_cb(int ori_mmsys_clk_mode, int update_mmsys_clk_mode);
static int current_mmsys_clk = MMSYS_CLK_LOW;

static mmdvfs_voltage_enum mmdvfs_get_default_step(void)
{
	mmdvfs_voltage_enum result = MMDVFS_VOLTAGE_LOW;

	if (g_mmdvfs_profile_id == MMDVFS_PROFILE_D3)
		result = MMDVFS_VOLTAGE_LOW;
	else if (g_mmdvfs_profile_id == MMDVFS_PROFILE_D1_PLUS)
		result = MMDVFS_VOLTAGE_LOW;
	else
		if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_HD)
			result = MMDVFS_VOLTAGE_LOW;
		else
			/* D1 FHD always HPM. do not have to trigger vcore dvfs. */
			result = MMDVFS_VOLTAGE_HIGH;

	return result;
}

static mmdvfs_voltage_enum mmdvfs_get_current_step(void)
{
	return g_mmdvfs_current_step;
}

static mmdvfs_voltage_enum mmdvfs_query(MTK_SMI_BWC_SCEN scenario,
MTK_MMDVFS_CMD *cmd)
{
	mmdvfs_voltage_enum step = mmdvfs_get_default_step();
	unsigned int venc_size;
	MTK_MMDVFS_CMD cmd_default;

	venc_size = g_mmdvfs_info->video_record_size[0]
	* g_mmdvfs_info->video_record_size[1];

	/* use default info */
	if (cmd == NULL) {
		memset(&cmd_default, 0, sizeof(MTK_MMDVFS_CMD));
		cmd_default.camera_mode = MMDVFS_CAMERA_MODE_FLAG_DEFAULT;
		cmd = &cmd_default;
	}

	/* collect the final information */
	if (cmd->sensor_size == 0)
		cmd->sensor_size = g_mmdvfs_cmd.sensor_size;

	if (cmd->sensor_fps == 0)
		cmd->sensor_fps = g_mmdvfs_cmd.sensor_fps;

	if (cmd->camera_mode == MMDVFS_CAMERA_MODE_FLAG_DEFAULT)
		cmd->camera_mode = g_mmdvfs_cmd.camera_mode;

	/* HIGH level scenarios */
	switch (scenario) {
#if defined(SMI_D2)         /* D2 ISP >= 6M HIGH */
	case SMI_BWC_SCEN_VR_SLOW:
	case SMI_BWC_SCEN_VR:
	if (cmd->sensor_size >= MMDVFS_PIXEL_NUM_SENSOR_6M)
		step = MMDVFS_VOLTAGE_HIGH;

	break;
#endif

	case SMI_BWC_SCEN_ICFP:
		step = MMDVFS_VOLTAGE_HIGH;
		break;
	/* force HPM for engineering mode */
	case SMI_BWC_SCEN_FORCE_MMDVFS:
		step = MMDVFS_VOLTAGE_HIGH;
		break;
	default:
		break;
	}

	return step;
}

static void mmdvfs_update_cmd(MTK_MMDVFS_CMD *cmd)
{
	if (cmd == NULL)
		return;

	if (cmd->sensor_size)
		g_mmdvfs_cmd.sensor_size = cmd->sensor_size;

	if (cmd->sensor_fps)
		g_mmdvfs_cmd.sensor_fps = cmd->sensor_fps;

	MMDVFSMSG("update cm %d %d\n", cmd->camera_mode, cmd->sensor_size);
	g_mmdvfs_cmd.camera_mode = cmd->camera_mode;
}


/* delay 4 seconds to go LPM to workaround camera ZSD + PIP issue */
#if !defined(SMI_D3)
static void mmdvfs_cam_work_handler(struct work_struct *work)
{
	MMDVFSMSG("CAM handler %d\n", jiffies_to_msecs(jiffies));
	mmdvfs_set_step(MMDVFS_CAM_MON_SCEN, mmdvfs_get_default_step());
}

static DECLARE_DELAYED_WORK(g_mmdvfs_cam_work, mmdvfs_cam_work_handler);

static void mmdvfs_stop_cam_monitor(void)
{
	cancel_delayed_work_sync(&g_mmdvfs_cam_work);
}

#define MMDVFS_CAM_MON_DELAY (4 * HZ)
static void mmdvfs_start_cam_monitor(void)
{
	mmdvfs_stop_cam_monitor();
	MMDVFSMSG("CAM start %d\n", jiffies_to_msecs(jiffies));
	mmdvfs_set_step(MMDVFS_CAM_MON_SCEN, MMDVFS_VOLTAGE_HIGH);
	/* 4 seconds for PIP switch preview aspect delays... */
	schedule_delayed_work(&g_mmdvfs_cam_work, MMDVFS_CAM_MON_DELAY);
}

#endif              /* !defined(SMI_D3) */

int mmdvfs_set_step(MTK_SMI_BWC_SCEN scenario, mmdvfs_voltage_enum step)
{
	int i, scen_index;
	unsigned int concurrency = 0;
	mmdvfs_voltage_enum final_step = mmdvfs_get_default_step();

#if !MMDVFS_ENABLE
	return 0;
#endif

	if (!is_vcorefs_can_work())
		return 0;

	/* D1 FHD always HPM. do not have to trigger vcore dvfs. */
	if (g_mmdvfs_profile_id == MMDVFS_PROFILE_D1
			&& mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_FHD)
			return 0;

	/* D1 plus FHD only allowed DISP as the client  */
	if (g_mmdvfs_profile_id == MMDVFS_PROFILE_D1_PLUS)
		if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_FHD
			&& scenario != (MTK_SMI_BWC_SCEN) MMDVFS_SCEN_DISP)
			return 0;


	if ((scenario >= (MTK_SMI_BWC_SCEN) MMDVFS_SCEN_COUNT) || (scenario < SMI_BWC_SCEN_NORMAL)) {
		MMDVFSERR("invalid scenario\n");
		return -1;
	}

	/* go through all scenarios to decide the final step */
	scen_index = (int)scenario;

	spin_lock(&g_mmdvfs_mgr->scen_lock);

	g_mmdvfs_scenario_voltage[scen_index] = step;

	concurrency = 0;
	for (i = 0; i < MMDVFS_SCEN_COUNT; i++) {
		if (g_mmdvfs_scenario_voltage[i] == MMDVFS_VOLTAGE_HIGH)
			concurrency |= 1 << i;
	}

	/* one high = final high */
	for (i = 0; i < MMDVFS_SCEN_COUNT; i++) {
		if (g_mmdvfs_scenario_voltage[i] == MMDVFS_VOLTAGE_HIGH) {
			final_step = MMDVFS_VOLTAGE_HIGH;
			break;
		}
	}

	g_mmdvfs_current_step = final_step;

	spin_unlock(&g_mmdvfs_mgr->scen_lock);

	MMDVFSMSG("Set vol scen:%d,step:%d,final:%d(0x%x),CMD(%d,%d,0x%x),INFO(%d,%d)\n",
		scenario, step, final_step, concurrency,
		g_mmdvfs_cmd.sensor_size, g_mmdvfs_cmd.sensor_fps, g_mmdvfs_cmd.camera_mode,
		g_mmdvfs_info->video_record_size[0], g_mmdvfs_info->video_record_size[1]);

#if MMDVFS_ENABLE
	/* call vcore dvfs API */
	if (final_step == MMDVFS_VOLTAGE_HIGH)
		vcorefs_request_dvfs_opp(KIR_MM, OPPI_PERF);
	else
		vcorefs_request_dvfs_opp(KIR_MM, OPPI_UNREQ);

#endif

	return 0;
}

void mmdvfs_handle_cmd(MTK_MMDVFS_CMD *cmd)
{
#if !MMDVFS_ENABLE
	return;
#endif

	MMDVFSMSG("MMDVFS cmd %u %d\n", cmd->type, cmd->scen);

	switch (cmd->type) {
	case MTK_MMDVFS_CMD_TYPE_SET:
		/* save cmd */
		mmdvfs_update_cmd(cmd);
		if (!(g_mmdvfs_concurrency & (1 << cmd->scen)))
			MMDVFSMSG("invalid set scen %d\n", cmd->scen);
		cmd->ret = mmdvfs_set_step(cmd->scen,
		mmdvfs_query(cmd->scen, cmd));
		break;
	case MTK_MMDVFS_CMD_TYPE_QUERY: { /* query with some parameters */
		if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_FHD) {
			/* QUERY ALWAYS HIGH for FHD */
			cmd->ret = (unsigned int)MMDVFS_STEP_HIGH2HIGH;

		} else { /* FHD */
			mmdvfs_voltage_enum query_voltage = mmdvfs_query(cmd->scen, cmd);

			mmdvfs_voltage_enum current_voltage =	mmdvfs_get_current_step();

			if (current_voltage < query_voltage) {
				cmd->ret = (unsigned int)MMDVFS_STEP_LOW2HIGH;
			} else if (current_voltage > query_voltage) {
				cmd->ret = (unsigned int)MMDVFS_STEP_HIGH2LOW;
			} else {
				cmd->ret
				= (unsigned int)(query_voltage
				== MMDVFS_VOLTAGE_HIGH
							 ? MMDVFS_STEP_HIGH2HIGH
							 : MMDVFS_STEP_LOW2LOW);
			}
		}

		MMDVFSMSG("query %d\n", cmd->ret);
		/* cmd->ret = (unsigned int)query_voltage; */
		break;
	}

	default:
		MMDVFSMSG("invalid mmdvfs cmd\n");
		BUG();
		break;
	}
}

void mmdvfs_notify_scenario_exit(MTK_SMI_BWC_SCEN scen)
{
#if !MMDVFS_ENABLE
	return;
#endif

	MMDVFSMSG("leave %d\n", scen);

#if !defined(SMI_D3)        /* d3 does not need this workaround because the MMCLK is always the highest */
	/*
	 * keep HPM for 4 seconds after exiting camera scenarios to get rid of
	 * cam framework will let us go to normal scenario for a short time
	 * (ex: STOP PREVIEW --> NORMAL --> START PREVIEW)
	 * where the LPM mode (low MMCLK) may cause ISP failures
	 */
	if ((scen == SMI_BWC_SCEN_VR) || (scen == SMI_BWC_SCEN_VR_SLOW)
	|| (scen == SMI_BWC_SCEN_ICFP)) {
		mmdvfs_start_cam_monitor();
	}
#endif              /* !defined(SMI_D3) */

	/* reset scenario voltage to default when it exits */
	mmdvfs_set_step(scen, mmdvfs_get_default_step());
}

void mmdvfs_notify_scenario_enter(MTK_SMI_BWC_SCEN scen)
{
#if !MMDVFS_ENABLE
	return;
#endif

	MMDVFSMSG("enter %d\n", scen);

	/* ISP ON = high */
	switch (scen) {
#if defined(SMI_D2)         /* d2 sensor > 6M */
	case SMI_BWC_SCEN_VR:
	mmdvfs_set_step(scen, mmdvfs_query(scen, NULL));
	break;
#else       /* default VR high */
	case SMI_BWC_SCEN_VR:
#endif
	case SMI_BWC_SCEN_WFD:
	case SMI_BWC_SCEN_VR_SLOW:
	case SMI_BWC_SCEN_VSS:
		/* Fall through */
	case SMI_BWC_SCEN_ICFP:
		/* Fall through */
	case SMI_BWC_SCEN_FORCE_MMDVFS:
		mmdvfs_set_step(scen, MMDVFS_VOLTAGE_HIGH);
		break;

	default:
		break;
	}
}

void mmdvfs_init(MTK_SMI_BWC_MM_INFO *info)
{
#if !MMDVFS_ENABLE
	return;
#endif

	spin_lock_init(&g_mmdvfs_mgr->scen_lock);

	/* set current step as the default step */
	g_mmdvfs_profile_id = mmdvfs_get_mmdvfs_profile();

	g_mmdvfs_current_step = mmdvfs_get_default_step();

	g_mmdvfs_info = info;
}

void mmdvfs_mhl_enable(int enable)
{
	g_mmdvfs_mgr->is_mhl_enable = enable;
}

void mmdvfs_mjc_enable(int enable)
{
	g_mmdvfs_mgr->is_mjc_enable = enable;
}

void mmdvfs_notify_scenario_concurrency(unsigned int u4Concurrency)
{
	/* raise EMI monitor BW threshold in VP, VR, VR SLOW motion cases */
	/* to make sure vcore stay MMDVFS level as long as possible */
	if (u4Concurrency & ((1 << SMI_BWC_SCEN_VP) | (1 << SMI_BWC_SCEN_VR)
	| (1 << SMI_BWC_SCEN_VR_SLOW))) {
#if MMDVFS_ENABLE_FLIPER_CONTROL
		MMDVFSMSG("fliper high\n");
		fliper_set_bw(BW_THRESHOLD_HIGH);
#endif
	} else {
#if MMDVFS_ENABLE_FLIPER_CONTROL
		MMDVFSMSG("fliper normal\n");
		fliper_restore_bw();
#endif
	}
	g_mmdvfs_concurrency = u4Concurrency;
}

/* switch MM CLK callback from VCORE DVFS driver */
void mmdvfs_mm_clock_switch_notify(int is_before, int is_to_high)
{
	/* for WQHD 1.0v, we have to dynamically switch DL/DC */
#ifdef MMDVFS_WQHD_1_0V
	int session_id;

	if (mmdvfs_get_lcd_resolution() != MMDVFS_LCD_SIZE_WQHD)
		return;

	session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);

	if (!is_before && is_to_high) {
		MMDVFSMSG("DL\n");
		/* nonblocking switch to direct link after HPM */
		primary_display_switch_mode_for_mmdvfs(DISP_SESSION_DIRECT_LINK_MODE, session_id,
									 0);
	} else if (is_before && !is_to_high) {
		/* BLOCKING switch to decouple before switching to LPM */
		MMDVFSMSG("DC\n");
		primary_display_switch_mode_for_mmdvfs(DISP_SESSION_DECOUPLE_MODE, session_id, 1);
	}
#endif				/* MMDVFS_WQHD_1_0V */
}


int mmdvfs_get_mmdvfs_profile(void)
{

	int mmdvfs_profile_id = MMDVFS_PROFILE_UNKNOWN;
	unsigned int segment_code = 0;

	segment_code = _GET_BITS_VAL_(31 : 25, get_devinfo_with_index(47));

#if defined(SMI_D1)
	mmdvfs_profile_id = MMDVFS_PROFILE_D1;
	if (segment_code == 0x41 ||	segment_code == 0x42 ||
			segment_code == 0x43 ||	segment_code == 0x49 ||
			segment_code == 0x51)
			mmdvfs_profile_id = MMDVFS_PROFILE_D1_PLUS;
	else
			mmdvfs_profile_id = MMDVFS_PROFILE_D1;
#elif defined(SMI_D2)
	mmdvfs_profile_id = MMDVFS_PROFILE_D2;
	if (segment_code == 0x4A || segment_code == 0x4B)
			mmdvfs_profile_id = MMDVFS_PROFILE_D2_M_PLUS;
	else if (segment_code == 0x52 || segment_code == 0x53)
						mmdvfs_profile_id = MMDVFS_PROFILE_D2_P_PLUS;
	else
			mmdvfs_profile_id = MMDVFS_PROFILE_D2;
#elif defined(SMI_D3)
	mmdvfs_profile_id = MMDVFS_PROFILE_D3;
#elif defined(SMI_J)
	mmdvfs_profile_id = MMDVFS_PROFILE_J1;
#elif defined(SMI_EV)
	mmdvfs_profile_id = MMDVFS_PROFILE_E1;
#endif
	return mmdvfs_profile_id;

}

int is_mmdvfs_supported(void)
{
	int mmdvfs_profile_id = mmdvfs_get_mmdvfs_profile();

	if (mmdvfs_profile_id == MMDVFS_PROFILE_D1 && mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_FHD)
		return 0;
	else if (mmdvfs_profile_id == MMDVFS_PROFILE_UNKNOWN)
		return 0;
	else
		return 1;
}

static clk_switch_cb notify_cb_func = default_clk_switch_cb;
static clk_switch_cb notify_cb_func_nolock;
static vdec_ctrl_cb vdec_suspend_cb_func;
static vdec_ctrl_cb vdec_resume_cb_func;

int register_mmclk_switch_vdec_ctrl_cb(vdec_ctrl_cb vdec_suspend_cb,
vdec_ctrl_cb vdec_resume_cb)
{
	vdec_suspend_cb_func = vdec_suspend_cb;
	vdec_resume_cb_func = vdec_resume_cb;

	return 1;
}

int register_mmclk_switch_cb(clk_switch_cb notify_cb,
clk_switch_cb notify_cb_nolock)
{
	notify_cb_func = notify_cb;
	notify_cb_func_nolock = notify_cb_nolock;

	return 1;
}



/* This desing is only for CLK Mux switch relate flows */
int mmdvfs_notify_mmclk_switch_request(int event)
{
	/* This API should only be used in J1 MMDVFS profile */
	return 0;
}



static int mmdfvs_adjust_mmsys_clk_by_hopping(int clk_mode)
{
	int result = 1;

	if (g_mmdvfs_profile_id != MMDVFS_PROFILE_D2_M_PLUS &&
		g_mmdvfs_profile_id != MMDVFS_PROFILE_D2_P_PLUS) {
		result = 0;
		return result;
	}

	if (!is_vcorefs_can_work()) {
		result = 0;
		return result;
	}

	if (clk_mode == MMSYS_CLK_HIGH) {
		if (current_mmsys_clk == MMSYS_CLK_MEDIUM)
			mt_dfs_vencpll(0xE0000);

		vdec_ctrl_func_checked(vdec_suspend_cb_func, "VDEC suspend");
		freqhopping_config(FH_VENC_PLLID , 0, false);
		notify_cb_func_checked(notify_cb_func, MMSYS_CLK_LOW, MMSYS_CLK_HIGH,
			"notify_cb_func");
		freqhopping_config(FH_VENC_PLLID , 0, true);
		vdec_ctrl_func_checked(vdec_resume_cb_func, "VDEC resume");

		current_mmsys_clk = MMSYS_CLK_HIGH;

	} else if (clk_mode == MMSYS_CLK_MEDIUM) {
		if (current_mmsys_clk == MMSYS_CLK_HIGH) {
			vdec_ctrl_func_checked(vdec_suspend_cb_func, "VDEC suspend");
			freqhopping_config(FH_VENC_PLLID , 0, false);
			notify_cb_func_checked(notify_cb_func, MMSYS_CLK_HIGH, MMSYS_CLK_LOW, "notify_cb_func");
			freqhopping_config(FH_VENC_PLLID , 0, true);
			vdec_ctrl_func_checked(vdec_resume_cb_func, "VDEC resume");
		}
		mt_dfs_vencpll(0x1713B1);
		notify_cb_func_checked(notify_cb_func, current_mmsys_clk, MMSYS_CLK_MEDIUM,
			"notify_cb_func");
		current_mmsys_clk = MMSYS_CLK_MEDIUM;
	} else if (clk_mode == MMSYS_CLK_LOW) {
		if (current_mmsys_clk == MMSYS_CLK_HIGH) {
			vdec_ctrl_func_checked(vdec_suspend_cb_func, "VDEC suspend");
			freqhopping_config(FH_VENC_PLLID , 0, false);
			notify_cb_func_checked(notify_cb_func, MMSYS_CLK_HIGH, MMSYS_CLK_LOW, "notify_cb_func");
			freqhopping_config(FH_VENC_PLLID , 0, true);
			vdec_ctrl_func_checked(vdec_resume_cb_func, "VDEC resume");
		}
		mt_dfs_vencpll(0xE0000);
		current_mmsys_clk = MMSYS_CLK_LOW;

	} else {
		MMDVFSMSG("Don't change CLK: mode=%d\n", clk_mode);
		result = 0;
	}

	return result;
}

int mmdvfs_set_mmsys_clk(MTK_SMI_BWC_SCEN scenario, int mmsys_clk_mode)
{
	return mmdfvs_adjust_mmsys_clk_by_hopping(mmsys_clk_mode);
}

static int vdec_ctrl_func_checked(vdec_ctrl_cb func, char *msg)
{
	if (func == NULL) {
		MMDVFSMSG("vdec_ctrl_func is NULL, not invoked: %s\n", msg);
	} else {
		func();
		return 1;
	}
	return 0;
}

static int notify_cb_func_checked(clk_switch_cb func, int ori_mmsys_clk_mode, int update_mmsys_clk_mode, char *msg)
{
	if (func == NULL) {
		MMDVFSMSG("notify_cb_func is NULL, not invoked: %s, (%d,%d)\n", msg, ori_mmsys_clk_mode,
		update_mmsys_clk_mode);
	} else {
		if (ori_mmsys_clk_mode != update_mmsys_clk_mode)
			MMDVFSMSG("notify_cb_func: %s, (%d,%d)\n", msg, ori_mmsys_clk_mode, update_mmsys_clk_mode);

		func(ori_mmsys_clk_mode, update_mmsys_clk_mode);
		return 1;
	}
	return 0;
}

static int mmsys_clk_switch_impl(unsigned int venc_pll_con1_val)
{
	if (g_mmdvfs_profile_id != MMDVFS_PROFILE_D2_M_PLUS
		&& g_mmdvfs_profile_id != MMDVFS_PROFILE_D2_P_PLUS) {
		MMDVFSMSG("mmsys_clk_switch_impl is not support in profile:%d", g_mmdvfs_profile_id);
		return 0;
	}

#if defined(SMI_D2)
	clkmux_sel(MT_MUX_MM, 6, "SMI common");
	mt_set_vencpll_con1(venc_pll_con1_val);
	udelay(20);
	clkmux_sel(MT_MUX_MM, 1, "SMI common");
#endif

	return 1;
}

static int default_clk_switch_cb(int ori_mmsys_clk_mode, int update_mmsys_clk_mode)
{
	unsigned int venc_pll_con1_val = 0;

	if (ori_mmsys_clk_mode  == MMSYS_CLK_LOW && update_mmsys_clk_mode == MMSYS_CLK_HIGH) {
		if (g_mmdvfs_profile_id == MMDVFS_PROFILE_D2_M_PLUS)
			venc_pll_con1_val = 0x820F0000; /* 380MHz (35M+) */
		else
			venc_pll_con1_val = 0x82110000; /* 442MHz (35P+) */
	} else if (ori_mmsys_clk_mode  == MMSYS_CLK_HIGH && update_mmsys_clk_mode == MMSYS_CLK_LOW) {
			venc_pll_con1_val = 0x830E0000;
	} else {
		return 1;
	}

	if (venc_pll_con1_val != 0)
		mmsys_clk_switch_impl(venc_pll_con1_val);

	return 1;
}

int mmdvfs_get_stable_isp_clk(void)
{
	return MMSYS_CLK_MEDIUM;
}
