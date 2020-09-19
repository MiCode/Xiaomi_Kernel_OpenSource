/*
 * Copyright (C) 2020 MediaTek Inc.
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
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/time.h>

/* For Power Model */
#include <mtk_me_swpm_plat.h>
#include <mtk_swpm_interface.h>
#include <mtk_drm_crtc.h>

#ifdef SWPM_ME_ENABLE
static bool swpm_enable = true;
#else
static bool swpm_enable;
#endif

static struct me_swpm_rec_data *me_swpm;
static DEFINE_MUTEX(me_mutex);

void init_me_swpm(void)
{
#if IS_ENABLED(CONFIG_MTK_SWPM)
	int ret;
	phys_addr_t *ptr = NULL;

	if (!swpm_enable)
		return;

	ret = swpm_mem_addr_request(ME_SWPM_TYPE, &ptr);
	if (!ret) {
		me_swpm = (struct me_swpm_rec_data *)ptr;
		me_swpm->disp_resolution =
			DISP_GetScreenWidth() * DISP_GetScreenHeight();
	}
#endif
}

void set_swpm_me_freq(unsigned int venc_freq,
	unsigned int vdec_freq, unsigned int mdp_freq)
{
	if (!swpm_enable)
		return;

	if (!me_swpm) {
		pr_notice("%s: re-init me_swpm\n", __func__);
		init_me_swpm();
	}

	if (me_swpm) {
		me_swpm->venc_freq = venc_freq;
		me_swpm->vdec_freq = vdec_freq;
		me_swpm->mdp_freq = mdp_freq;
	}
}

static unsigned int cal_fps(unsigned int *start, unsigned int *end,
	unsigned long time[], const unsigned int length)
{
	unsigned int count = 0, fps = 0;
	unsigned int local_start = *start;
	unsigned int local_end = *end;
	unsigned long duration;
	ktime_t current_time;

	if (local_start != local_end
		|| time[local_end] != 0) {
		local_end = (local_end+1)%length;
		*end = local_end;
		if (local_start == local_end) {
			local_start = (local_start+1)%length;
			*start = local_start;
		}
	}
	current_time = ktime_get();
	time[local_end] = ktime_to_us(current_time);
	if (local_start > local_end)
		count = length;
	else
		count = local_end - local_start + 1;
	duration = time[local_end] - time[local_start];
	if (count > 1)
		fps = ((count-1) * 1000000 - 1) / duration + 1;
	pr_debug("%s: start(%d)=%d end(%d)=%d count=%d duration=%d fps=%d\n",
		__func__, *start, time[local_start], *end, time[local_end],
		count, duration, fps);
	return fps;
}

#define DISP_TIME_SIZE (10)
static unsigned long disp_time[DISP_TIME_SIZE];
static unsigned int disp_start, disp_end;


void set_swpm_disp_active(bool is_on)
{
	if (!swpm_enable)
		return;

	if (!me_swpm) {
		pr_notice("%s: re-init me_swpm\n", __func__);
		init_me_swpm();
	}

	if (me_swpm) {
		me_swpm->disp_active = is_on ? 1 : 0;
		pr_debug("%s: disp_active=%d\n",
			__func__, me_swpm->disp_active);
		if (!is_on) {
			disp_start = 0;
			disp_end = 0;
			memset(disp_time, 0, sizeof(disp_time));
		}
	}
}

void set_swpm_disp_work(void)
{
	if (!swpm_enable)
		return;

	if (!me_swpm) {
		pr_notice("%s: re-init me_swpm\n", __func__);
		init_me_swpm();
	}

	if (me_swpm) {
		// display call this api on interrupt handler so mark mutex
		//mutex_lock(&me_mutex);
		me_swpm->disp_fps = cal_fps(&disp_start, &disp_end,
			disp_time, DISP_TIME_SIZE);
		//mutex_unlock(&me_mutex);
		pr_debug("%s: disp fps=%d\n", __func__, me_swpm->disp_fps);
	}
}

#define VENC_TIME_SIZE (10)
static unsigned long venc_time[VENC_TIME_SIZE];
static unsigned int venc_start, venc_end;

void set_swpm_venc_active(bool is_on)
{
	if (!swpm_enable)
		return;

	if (!me_swpm) {
		pr_notice("%s: re-init me_swpm\n", __func__);
		init_me_swpm();
	}

	if (me_swpm) {
		me_swpm->venc_active = is_on ? 1 : 0;
		if (is_on) {
			mutex_lock(&me_mutex);
			me_swpm->venc_fps = cal_fps(&venc_start, &venc_end,
				venc_time, VENC_TIME_SIZE);
			mutex_unlock(&me_mutex);
			pr_debug("%s: venc fps=%d\n",
				__func__, me_swpm->venc_fps);
		}
	}
}

#define VDEC_TIME_SIZE (10)
static unsigned long vdec_time[VDEC_TIME_SIZE];
static unsigned int vdec_start, vdec_end;

void set_swpm_vdec_active(bool is_on)
{
	if (!swpm_enable)
		return;

	if (!me_swpm) {
		pr_notice("%s: re-init me_swpm\n", __func__);
		init_me_swpm();
	}

	if (me_swpm) {
		me_swpm->vdec_active = is_on ? 1 : 0;
		if (is_on) {
			mutex_lock(&me_mutex);
			me_swpm->vdec_fps = cal_fps(&vdec_start, &vdec_end,
				vdec_time, VDEC_TIME_SIZE);
			mutex_unlock(&me_mutex);
			pr_debug("%s: vdec fps=%d\n",
				__func__, me_swpm->vdec_fps);
		}
	}
}

void set_swpm_mdp_active(bool is_on)
{
	if (!swpm_enable)
		return;

	if (!me_swpm) {
		pr_notice("%s: re-init me_swpm\n", __func__);
		init_me_swpm();
	}

	if (me_swpm) {
		me_swpm->mdp_active = is_on ? 1 : 0;
		pr_debug("%s: mdp_active=%d\n",
			__func__, me_swpm->mdp_active);
	}
}

module_param(swpm_enable, bool, 0644);
MODULE_PARM_DESC(swpm_enable, "swpm me enable");
