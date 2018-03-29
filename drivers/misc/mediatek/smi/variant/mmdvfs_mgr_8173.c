/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/uaccess.h>
#include "mt_smi.h"
#include <mt_vcore_dvfs.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/suspend.h>

#include <linux/mtk_gpu_utility.h>

#include "mmdvfs_mgr_8173.h"

#define MMDVFS_ENABLE_FLIPER_CONTROL 0

#if MMDVFS_ENABLE_FLIPER_CONTROL
#include <mach/fliper.h>
#endif


#define MMDVFS_ENABLE_WQHD 0

#define MMDVFS_GPU_LOADING_NUM	30

/* mmdvfs MM sizes */
#define MMDVFS_PIXEL_NUM_1080P	(1920 * 1080)

/* mmdvfs MM sizes */
#define MMDVFS_PIXEL_NUM_2160P	(3840 * 2160)

/* mmdvfs display sizes */
#define MMDVFS_DISPLAY_SIZE_FHD	(1920 * 1216)

/* mmdvfs display for 2K case */
#define MMDVFS_DISPLAY_SIZE_2K	(2048 * 1536)

enum {
	MMDVFS_CAM_MON_SCEN = SMI_BWC_SCEN_CNT,
	MMDVFS_SCEN_MHL,
	MMDVFS_SCEN_COUNT
};

static mmdvfs_voltage_enum g_mmdvfs_scenario_voltage[MMDVFS_SCEN_COUNT] = {MMDVFS_VOLTAGE_DEFAULT};
static mmdvfs_voltage_enum g_mmdvfs_current_step;
static unsigned int g_mmdvfs_concurrency;
static MTK_SMI_BWC_MM_INFO *g_mmdvfs_info;

typedef struct {
	/* linux timer */
	struct timer_list timer;

	/* work q */
	struct workqueue_struct *work_queue;
	struct work_struct work;

	/* data payload */
	unsigned int gpu_loadings[MMDVFS_GPU_LOADING_NUM];
	int gpu_loading_index;
} mmdvfs_gpu_monitor_struct;

typedef struct {
	spinlock_t scen_lock;
	int is_mhl_enable;
	mmdvfs_gpu_monitor_struct gpu_monitor;

} mmdvfs_context_struct;

/* mmdvfs_query() return value, remember to sync with user space */
typedef enum {
	MMDVFS_STEP_LOW = 0,
	MMDVFS_STEP_HIGH,

	MMDVFS_STEP_LOW2LOW,	/* LOW */
	MMDVFS_STEP_HIGH2LOW,	/* LOW */
	MMDVFS_STEP_LOW2HIGH,	/* HIGH */
	MMDVFS_STEP_HIGH2HIGH,  /* HIGH */
} mmdvfs_step_enum;

/* lcd size */
typedef enum {
	MMDVFS_LCD_SIZE_FHD,
	MMDVFS_LCD_SIZE_2K,
	MMDVFS_LCD_SIZE_WQHD,
	MMDVFS_LCD_SIZE_END_OF_ENUM
} mmdvfs_lcd_size_enum;

static mmdvfs_context_struct g_mmdvfs_mgr_cntx;
static mmdvfs_context_struct * const g_mmdvfs_mgr = &g_mmdvfs_mgr_cntx;
static MTK_MMDVFS_CMD g_mmdvfs_cmd;

static mmdvfs_lcd_size_enum mmdvfs_get_lcd_resolution(void)
{
	uint32_t disp_size = DISP_GetScreenWidth() * DISP_GetScreenHeight();

	if (disp_size <= MMDVFS_DISPLAY_SIZE_FHD)
		return MMDVFS_LCD_SIZE_FHD;
	else if (disp_size <= MMDVFS_DISPLAY_SIZE_2K)
		return MMDVFS_LCD_SIZE_2K;
	else
		return MMDVFS_LCD_SIZE_WQHD;
}

static mmdvfs_voltage_enum mmdvfs_get_default_step(void)
{
	mmdvfs_lcd_size_enum lcd_resolution = mmdvfs_get_lcd_resolution();

	if ((lcd_resolution == MMDVFS_LCD_SIZE_FHD) ||
		(lcd_resolution == MMDVFS_LCD_SIZE_2K))
		return MMDVFS_VOLTAGE_LOW;
	else
		return MMDVFS_VOLTAGE_HIGH;
}

static mmdvfs_voltage_enum mmdvfs_get_current_step(void)
{
	return g_mmdvfs_current_step;
}


static mmdvfs_voltage_enum mmdvfs_query(MTK_SMI_BWC_SCEN scenario, MTK_MMDVFS_CMD *cmd)
{
	mmdvfs_voltage_enum step = mmdvfs_get_default_step();
	unsigned int venc_size;
	MTK_MMDVFS_CMD cmd_default;

	venc_size = g_mmdvfs_info->video_record_size[0] * g_mmdvfs_info->video_record_size[1];

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
	case SMI_BWC_SCEN_VR:
		if (cmd->sensor_size >= MMDVFS_PIXEL_NUM_2160P) {
			/* VR4K high */
			step = MMDVFS_VOLTAGE_HIGH;
		} else if (cmd->camera_mode &
				   (MMDVFS_CAMERA_MODE_FLAG_PIP | MMDVFS_CAMERA_MODE_FLAG_VFB |
				   MMDVFS_CAMERA_MODE_FLAG_EIS_2_0)) {
			/* PIP or VFB or EIS keeps high for ISP clock */
			step = MMDVFS_VOLTAGE_HIGH;
		} else if (cmd->sensor_size >= MMDVFS_PIXEL_NUM_1080P && cmd->sensor_fps > 30) {
			/* FullHD @ 60fps keeps high */
			step = MMDVFS_VOLTAGE_HIGH;
		}
		break;

	case SMI_BWC_SCEN_VR_SLOW:
		/* >= 120 fps SLOW MOTION high */
		if ((cmd->sensor_fps >= 120) ||
			/* AVC @ 60 needs HPM */
			((cmd->sensor_fps >= 60) &&
			((g_mmdvfs_info->video_encode_codec == 2) || (venc_size >= MMDVFS_PIXEL_NUM_1080P)))) {
			step = MMDVFS_VOLTAGE_HIGH;
		}
		break;

	case SMI_BWC_SCEN_ICFP:
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


	MMDVFSMSG("update cm %d\n", cmd->camera_mode);
	g_mmdvfs_cmd.camera_mode = cmd->camera_mode;
}

static void mmdvfs_dump_info(void)
{
	MMDVFSMSG("CMD %d %d %d\n", g_mmdvfs_cmd.sensor_size, g_mmdvfs_cmd.sensor_fps, g_mmdvfs_cmd.camera_mode);
	MMDVFSMSG("INFO VR %d %d\n", g_mmdvfs_info->video_record_size[0], g_mmdvfs_info->video_record_size[1]);
}

int mmdvfs_set_step(MTK_SMI_BWC_SCEN scenario, mmdvfs_voltage_enum step)
{
	int i, scen_index;
	mmdvfs_voltage_enum final_step = mmdvfs_get_default_step();

	if (step == MMDVFS_VOLTAGE_DEFAULT_STEP)
		step = final_step;

	MMDVFSMSG("MMDVFS set voltage scen %d step %d\n", scenario, step);

	if ((scenario >= (MTK_SMI_BWC_SCEN)MMDVFS_SCEN_COUNT) || (scenario < SMI_BWC_SCEN_NORMAL)) {
		MMDVFSERR("invalid scenario\n");
		return -1;
	}

	/* dump information */
	mmdvfs_dump_info();

	/* go through all scenarios to decide the final step */
	scen_index = (int)scenario;

	spin_lock(&g_mmdvfs_mgr->scen_lock);

	g_mmdvfs_scenario_voltage[scen_index] = step;

	/* one high = final high */
	for (i = 0; i < MMDVFS_SCEN_COUNT; i++) {
		if (g_mmdvfs_scenario_voltage[i] == MMDVFS_VOLTAGE_HIGH) {
			final_step = MMDVFS_VOLTAGE_HIGH;
			break;
		}
	}

	g_mmdvfs_current_step = final_step;
	spin_unlock(&g_mmdvfs_mgr->scen_lock);

	MMDVFSMSG("MMDVFS set voltage scen %d step %d final %d\n", scenario, step, final_step);

	/* call vcore dvfs API */
	if (final_step == MMDVFS_VOLTAGE_HIGH) {
		MMDVFSMSG("request OPPI_PERF\n");
		vcorefs_request_dvfs_opp(KR_MM_SCEN, OPPI_PERF);
	} else {
		MMDVFSMSG("request OPPI_UNREQ\n");
		vcorefs_request_dvfs_opp(KR_MM_SCEN, OPPI_UNREQ);
	}

	return 0;
}

void mmdvfs_handle_cmd(MTK_MMDVFS_CMD *cmd)
{
	MMDVFSMSG("MMDVFS handle cmd %u s %d\n", cmd->type, cmd->scen);

	switch (cmd->type) {
	case MTK_MMDVFS_CMD_TYPE_SET:
		/* save cmd */
		mmdvfs_update_cmd(cmd);

		if (!(g_mmdvfs_concurrency & (1 << cmd->scen))) {
			MMDVFSMSG("invalid set scen %d\n", cmd->scen);
			cmd->ret = -1;
		} else
			cmd->ret = mmdvfs_set_step(cmd->scen, mmdvfs_query(cmd->scen, cmd));
		break;

	case MTK_MMDVFS_CMD_TYPE_QUERY:
	{	/* query with some parameters */
		if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_WQHD) {
			/* QUERY ALWAYS HIGH for WQHD */
			cmd->ret = (unsigned int)MMDVFS_STEP_HIGH2HIGH;
		} else {
			mmdvfs_voltage_enum query_voltage = mmdvfs_query(cmd->scen, cmd);
			mmdvfs_voltage_enum current_voltage = mmdvfs_get_current_step();

			if (current_voltage < query_voltage)
				cmd->ret = (unsigned int)MMDVFS_STEP_LOW2HIGH;
			else if (current_voltage > query_voltage)
				cmd->ret = (unsigned int)MMDVFS_STEP_HIGH2LOW;
			else
				cmd->ret = (unsigned int)(query_voltage == MMDVFS_VOLTAGE_HIGH ?
				MMDVFS_STEP_HIGH2HIGH : MMDVFS_STEP_LOW2LOW);
		}

		MMDVFSMSG("query %d\n", cmd->ret);
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
	MMDVFSMSG("leave %d\n", scen);

	/* reset scenario voltage to default when it exits */
	mmdvfs_set_step(scen, mmdvfs_get_default_step());
}

void mmdvfs_notify_scenario_enter(MTK_SMI_BWC_SCEN scen)
{
	MMDVFSMSG("enter %d\n", scen);

	if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_WQHD) {
#ifndef MMDVFS_WQHD_1_0V
		/* force IDLE LOW POWER back to HIGH for MM scenarios */
		if (scen != SMI_BWC_SCEN_NORMAL)
			mmdvfs_set_step(scen, MMDVFS_VOLTAGE_HIGH);
#else
		switch (scen) {
		case SMI_BWC_SCEN_WFD:
		case SMI_BWC_SCEN_ICFP:
			mmdvfs_set_step(scen, MMDVFS_VOLTAGE_HIGH);
			break;

		case SMI_BWC_SCEN_VR:
		case SMI_BWC_SCEN_VR_SLOW:
			mmdvfs_set_step(scen, mmdvfs_query(scen, NULL));
			/* workaround for ICFP...its mmdvfs_set() will come after leaving ICFP */
			mmdvfs_set_step(SMI_BWC_SCEN_ICFP, mmdvfs_get_default_step());
			break;

		default:
			break;
		}
#endif
#if MMDVFS_ENABLE_WQHD
		if (scen == SMI_BWC_SCEN_VP)
			mmdvfs_start_gpu_monitor(&g_mmdvfs_mgr->gpu_monitor);
#endif /* MMDVFS_ENABLE_WQHD */
	} else {	/* FHD and 2K */
		switch (scen) {
		case SMI_BWC_SCEN_ICFP:
			mmdvfs_set_step(scen, MMDVFS_VOLTAGE_HIGH);
			break;

		case SMI_BWC_SCEN_VR:
		case SMI_BWC_SCEN_VR_SLOW:
			mmdvfs_set_step(scen, mmdvfs_query(scen, NULL));
			/* workaround for ICFP...its mmdvfs_set() will come after leaving ICFP */
			mmdvfs_set_step(SMI_BWC_SCEN_ICFP, mmdvfs_get_default_step());
			break;

		default:
			break;
		}
	}
}



void mmdvfs_init(MTK_SMI_BWC_MM_INFO *info)
{
	spin_lock_init(&g_mmdvfs_mgr->scen_lock);

	/* set current step as the default step */
	g_mmdvfs_current_step = mmdvfs_get_default_step();
	g_mmdvfs_info = info;
}

void mmdvfs_notify_scenario_concurrency(unsigned int u4Concurrency)
{
	/*
	 * DO NOT CALL VCORE DVFS API HERE. THIS FUNCTION IS IN SMI SPIN LOCK.
	 */

	/*
	raise EMI monitor BW threshold in VP, VR, VR SLOW motion cases
	to make sure vcore stay MMDVFS level as long as possible
	*/
#if MMDVFS_ENABLE_FLIPER_CONTROL
	if (u4Concurrency & ((1 << SMI_BWC_SCEN_VP) | (1 << SMI_BWC_SCEN_VR) | (1 << SMI_BWC_SCEN_VR_SLOW))) {
		MMDVFSMSG("fliper high\n");
		fliper_set_bw(BW_THRESHOLD_HIGH);
	} else {
		MMDVFSMSG("fliper normal\n");
		fliper_restore_bw();
	}
#endif
	g_mmdvfs_concurrency = u4Concurrency;
}

int mmdvfs_is_default_step_need_perf(void)
{
	if (mmdvfs_get_default_step() == MMDVFS_VOLTAGE_LOW)
		return 0;

	return 1;
}

/* switch MM CLK callback from VCORE DVFS driver */
void mmdvfs_mm_clock_switch_notify(int is_before, int is_to_high)
{
	/* WQHD 1.0v feature */
	/* for WQHD 1.0v, we have to dynamically switch DL/DC */
}

