// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
/* #include <linux/switch.h> */

#include "disp_drv_platform.h"
#include "debug.h"
#include "disp_drv_log.h"
#include "disp_lcm.h"
#include "disp_utils.h"
#include "mtkfb.h"
#include "disp_session.h"
#include "ddp_manager.h"
#include "mtkfb_fence.h"
#include "display_recorder.h"
#include "fbconfig_kdebug.h"
//#include "mtk_sync.h"
#include "disp_session.h"
#include "disp_helper.h"
#include "mtk_disp_mgr.h"
#include "mtkfb_console.h"
#include "disp_lowpower.h"
#include "disp_recovery.h"
#include "layering_rule.h"
#include "disp_rect.h"
#include "disp_partial.h"
#include "disp_arr.h"
#include "primary_display.h"

static struct mutex cb_table_lock;
#define DISP_MAX_FPSCHG_CALLBACK 5
static FPS_CHG_CALLBACK fps_chg_callback_table[DISP_MAX_FPSCHG_CALLBACK];

/* used by ARR2.0 */
int primary_display_get_cur_refresh_rate(void)
{
	return primary_display_force_get_vsync_fps();
}

int primary_display_get_max_refresh_rate(void)
{
	int ret = -1;

	/* _primary_path_lock(__func__); */
	if ((pgc && pgc->plcm && pgc->plcm->params) &&
		(pgc->plcm->params->max_refresh_rate != 0))
		ret = pgc->plcm->params->max_refresh_rate;
	else
		ret = 60;
	/* _primary_path_unlock(__func__); */

	return ret;
}

int primary_display_get_min_refresh_rate(void)
{
	int ret = -1;

	/* _primary_path_lock(__func__); */
	if ((pgc && pgc->plcm && pgc->plcm->params) &&
		(pgc->plcm->params->min_refresh_rate != 0))
		ret = pgc->plcm->params->min_refresh_rate;
	else
		ret = 60;
	/* _primary_path_unlock(__func__); */

	return ret;
}

int primary_display_set_refresh_rate(unsigned int refresh_rate)
{
	int ret = -1;
	int temp_refresh_rate_min = 0;
	int temp_refresh_rate_max = 0;

	temp_refresh_rate_min = primary_display_get_min_refresh_rate();
	temp_refresh_rate_max = primary_display_get_max_refresh_rate();

	if ((refresh_rate > temp_refresh_rate_max) ||
		(refresh_rate < temp_refresh_rate_min))
		return ret;

	/* AP set refresh rate */
	ret = primary_display_force_set_vsync_fps(refresh_rate, 0);
	return ret;
}

void disp_invoke_fps_chg_callbacks(unsigned int new_fps)
{
	unsigned int i = 0;

	DISPMSG("[fps]: %s,new_fps =%d\n", __func__, new_fps);
	mutex_lock(&cb_table_lock);
	for (i = 0; i < DISP_MAX_FPSCHG_CALLBACK; i++) {
		if (fps_chg_callback_table[i])
			fps_chg_callback_table[i](new_fps);
	}
	mutex_unlock(&cb_table_lock);
}
