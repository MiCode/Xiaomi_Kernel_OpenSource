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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/mtk_gpu_utility.h>
#include "mt-plat/mtk_thermal_monitor.h"
/* fps update from display */
#include "disp_session.h"
/* switch device to sent the (fps limit)uevent */
#include <linux/switch.h>
#include "mach/mt_thermal.h"
#include <linux/uidgid.h>
#include "ged_dvfs.h"


/* 1: turn on adaptive fps cooler; 0: turn off */
#define ADAPTIVE_FPS_COOLER              (1)


#define mtk_cooler_fps_dprintk_always(fmt, args...) \
pr_debug("thermal/cooler/fps" fmt, ##args)

#define mtk_cooler_fps_dprintk(fmt, args...) \
do { \
	if (1 == cl_fps_klog_on) \
		pr_debug("thermal/cooler/fps" fmt, ##args); \
} while (0)

#define MAX_NUM_INSTANCE_MTK_COOLER_FPS  4

#define MTK_CL_FPS_GET_CURR_STATE(curr_state, state) \
{ curr_state = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_FPS_SET_CURR_STATE(curr_state, state) \
do { \
	if (0 == curr_state) \
		state &= ~0x1; \
	else \
		state |= 0x1; \
} while (0)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static int cl_fps_klog_on;
static struct thermal_cooling_device *cl_fps_dev[MAX_NUM_INSTANCE_MTK_COOLER_FPS] = { 0 };
static unsigned int cl_fps_param[MAX_NUM_INSTANCE_MTK_COOLER_FPS] = { 0 };
static unsigned long cl_fps_state[MAX_NUM_INSTANCE_MTK_COOLER_FPS] = { 0 };

static unsigned int cl_fps_cur_limit;
static unsigned int tm_input_fps;
static struct switch_dev fps_switch_data;

#if ADAPTIVE_FPS_COOLER
/* TODO: TBD */
#define MAX_FPS_LIMIT		60
#define MIN_FPS_LIMIT		10

#define MAX_FPS_LEVELS		(MAX_FPS_LIMIT - MIN_FPS_LIMIT)
#define DEFAULT_FPS_LEVEL	10
static int fps_level[MAX_FPS_LEVELS];
static int nr_fps_levels = MAX_FPS_LEVELS;
static int curr_fps_level;

#define MAX_FPS_SMA_LEN			10
static int fps_history[MAX_FPS_SMA_LEN] = {0};
static int fps_history_idx;
static int fps_sma_len = MAX_FPS_SMA_LEN;

#define MAX_TPCB_SMA_LEN		10
static int tpcb_history[MAX_TPCB_SMA_LEN] = {0};
static int tpcb_history_idx;
static int tpcb_sma_len = MAX_TPCB_SMA_LEN;

#define MAX_GPU_LOADING_SMA_LEN		10
static int gpu_loading_history[MAX_GPU_LOADING_SMA_LEN] = {0};
static int gpu_loading_history_idx;
static int gpu_loading_sma_len = MAX_GPU_LOADING_SMA_LEN;

static struct thermal_cooling_device *cl_adp_fps_dev;
static unsigned int cl_adp_fps_state;
static int cl_adp_fps_limit = MAX_FPS_LIMIT;

#define GPU_LOADING_THRESHOLD	60
/* in percentage */
/* ====FPS_DEBUGFS=========
static int gpu_loading_threshold = GPU_LOADING_THRESHOLD;
====FPS_DEBUGFS=========== */

/* in percentage */
static int fps_error_threshold = 10;
/* in round */
static int fps_stable_period = 10;
/* FPS is active when over stable tpcb or always */
static int fps_limit_always_on;

static int leave_fps_limit_duration = 1;
/* minimum fps that we regard as still in game playing */
static int in_game_low_fps = 5;

/* FIXME: need someone set/clear this */
static int in_game_whitelist = 1;
#endif

#ifndef __GED_TYPE_H__
typedef enum GED_INFO_TA {
GED_EVENT_GAS_MODE,
GED_UNDEFINED
} GED_INFO;
#endif

unsigned long  __attribute__ ((weak))
ged_query_info(GED_INFO eType)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

static int game_whitelist_check(void)
{
	unsigned long result = ged_query_info(GED_EVENT_GAS_MODE);

	if (1 == result)
		in_game_whitelist = 1;
	else if (0 == result)
		in_game_whitelist = 0;

	return 0;
}

static int fps_update(void)
{
	disp_session_info info;

	memset(&info, 0, sizeof(info));
	info.session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
	disp_mgr_get_session_info(&info);
	/* mtk_cooler_fps_dprintk("display update fps is: %d.%d\n", info.updateFPS/100, info.updateFPS%100); */
	/* mtk_cooler_fps_dprintk("is display fps stable: %d\n", info.is_updateFPS_stable); */
#if 0
	if (info.is_updateFPS_stable)
			tm_input_fps = info.updateFPS;
	else
		     tm_input_fps = 0;
#else
	tm_input_fps = info.updateFPS/100;
#endif

	return 0;
}

static void mtk_cl_fps_set_fps_limit(void)
{
	int i = 0;
	int min_limit = 60;
	unsigned int min_param = 60;

	for (; i < MAX_NUM_INSTANCE_MTK_COOLER_FPS; i++) {
		unsigned long curr_state;

		MTK_CL_FPS_GET_CURR_STATE(curr_state, cl_fps_state[i]);

		if (1 == curr_state) {
			int limit = 0;

		limit = cl_fps_param[i];

			/* a cooler with 0 fps is not allowed */
			if (limit == 0)
				goto err_unreg;

			if (limit <= min_limit) {
				min_limit = limit;
				min_param = cl_fps_param[i];
			}
		}
	}

#if ADAPTIVE_FPS_COOLER
	if (cl_adp_fps_limit < min_param)
		min_param = cl_adp_fps_limit;
#endif

	if (min_param != cl_fps_cur_limit) {
		cl_fps_cur_limit = min_param;
		switch_set_state(&fps_switch_data, cl_fps_cur_limit);
		mtk_cooler_fps_dprintk_always("[%s] fps limit: %d\n", __func__, cl_fps_cur_limit);
	}

err_unreg:
	return;
}

static int mtk_cl_fps_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_fps_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	MTK_CL_FPS_GET_CURR_STATE(*state, *((unsigned long *) cdev->devdata));
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_fps_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, state);
	MTK_CL_FPS_SET_CURR_STATE(state, *((unsigned long *) cdev->devdata));

	mtk_cl_fps_set_fps_limit();

	return 0;
}

#if ADAPTIVE_FPS_COOLER
static int adp_fps_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int adp_fps_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_adp_fps_state;
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

/* for gpu_loading, tpcb, fps */
static int get_sma_val(int vals[], int sma_len)
{
	int i, v = 0;

	for (i = 0; i < sma_len; i++)
		v += vals[i];

	v = v / sma_len;
	return v;
}

static void set_sma_val(int vals[], int sma_len, int *idx, int val)
{
	vals[*idx] = val;
	*idx = (*idx + 1) % sma_len;
}

/* increase by one level */
static int increase_fps_limit(void)
{
	if (curr_fps_level > 0)
		curr_fps_level--;

	return fps_level[curr_fps_level];
}

#if 0
/* decrease by one level */
static int decrease_fps_limit(void)
{
	if (curr_fps_level < (nr_fps_levels - 1))
		curr_fps_level++;

	return fps_level[curr_fps_level];
}
#endif

/**
 * floor function applied to fps_level
 * @retval index of fps_level
 */
static int find_fps_floor(int fps)
{
	int i;

	for (i = 0; i < nr_fps_levels; i++) {
		if (fps_level[i] <= fps)
			return i;
	}

	return nr_fps_levels - 1;
}

static int unlimit_fps_limit(void)
{
	curr_fps_level = 0;
	return fps_level[curr_fps_level];
}

/**
 * We consider to decrease fps limit to avoid unstable fps only if the
 * system already utilizes its full capacity.
 * e.g., gpu utilization already reaches a threshold, or maybe other index?
 */
static bool is_system_too_busy(void)
{
	int gpu_loading;

	/* GPU cases */
	gpu_loading = get_sma_val(gpu_loading_history, gpu_loading_sma_len);
	mtk_cooler_fps_dprintk("[%s] gpu_loading = %d\n", __func__, gpu_loading);
	if (gpu_loading >= GPU_LOADING_THRESHOLD)
		return true;

	/* TBD: other cases? */

	return false;
}

/* This function is actually an governor */
static int adp_calc_fps_limit(void)
{
	static int last_change_tpcb;
	static int period;
	int sma_tpcb, tpcb_change, sma_fps;
	int fps_limit = fps_level[curr_fps_level];

	if (period < fps_stable_period) {
		period++;
		return fps_limit;
	}
	period = 0;

	sma_tpcb = get_sma_val(tpcb_history, tpcb_sma_len);
	tpcb_change = sma_tpcb - last_change_tpcb;

	sma_fps = get_sma_val(fps_history, fps_sma_len);

	mtk_cooler_fps_dprintk("[%s] sma_tpcb = %d, tpcb_change = %d, sma_fps = %d\n",
		__func__, sma_tpcb,  tpcb_change, sma_fps);

	if (fps_limit_always_on ||
		(sma_fps < 40 && sma_tpcb >= mtk_thermal_get_tpcb_target())) {
		if (is_system_too_busy() &&
				fps_limit - sma_fps >= fps_limit * fps_error_threshold / 100) {
				mtk_cooler_fps_dprintk("[%s] fps_limit = %d, sma_fps = %d\n",
					__func__, fps_limit, sma_fps);
			/* we do not limit FPS if not in game */
			if (in_game_whitelist) {
				curr_fps_level = find_fps_floor(sma_fps);
				fps_limit = fps_level[curr_fps_level];
				mtk_cooler_fps_dprintk("[%s] curr_fps_level = %d, fps_limit = %d\n",
					__func__, curr_fps_level, fps_limit);
			}
#if 0
			else {
				/* FIXME: give hint to somebody, so that user
				 * can decides whether he/she wants fps to be
				 * limited or not */
				/* send_hint_to_user(); */
			}
#endif
		}
	}

	/* tpcb is falling and gpu loading is low, too */
	if (sma_tpcb < mtk_thermal_get_tpcb_target() && tpcb_change < 0)
		fps_limit = increase_fps_limit();

	if (tpcb_change)
		last_change_tpcb = sma_tpcb;

	return fps_limit;
}

static bool in_consistent_scene(void)
{
	static int duration;
	int fps = tm_input_fps;

	if (!in_game_whitelist)
		return false;

	if (fps <= in_game_low_fps)
		duration++;
	else /* TODO: TBD: should we reset duration or decrease */
		duration = 0;

	mtk_cooler_fps_dprintk("[%s] fps <= in_game_low_fps = %d\n", __func__, duration);

	if (duration >= leave_fps_limit_duration) {
		duration = 0;
		return false;
	} else
		return true;
}

static int adp_fps_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	int gpu_loading;

	if ((state != 0) && (state != 1)) {
		mtk_cooler_fps_dprintk("[%s] invalid input (0: no thro; 1: adp fps thro on)\n", __func__);
		return 0;
	}

	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, state);
	cl_adp_fps_state = state;

	/* check the fps update from display */
	fps_update();

	/* game? */
	game_whitelist_check();

	set_sma_val(fps_history, fps_sma_len, &fps_history_idx, tm_input_fps);
	set_sma_val(tpcb_history, tpcb_sma_len, &tpcb_history_idx,
			mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP));

	if (!mtk_get_gpu_loading(&gpu_loading))
		gpu_loading = 0;

	set_sma_val(gpu_loading_history, gpu_loading_sma_len, &gpu_loading_history_idx,
			gpu_loading);
	/* 1. update the parameter of "cl_adp_fps_limit" */
	/* do we already leave game? */
	if (!in_consistent_scene())
		unlimit_fps_limit();

	if (cl_adp_fps_state)
		cl_adp_fps_limit = adp_calc_fps_limit();
	else
		cl_adp_fps_limit = unlimit_fps_limit();

	/* 2. set the the limit */
	mtk_cl_fps_set_fps_limit();

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_adp_fps_ops = {
	.get_max_state = adp_fps_get_max_state,
	.get_cur_state = adp_fps_get_cur_state,
	.set_cur_state = adp_fps_set_cur_state,
};

static void reset_fps_level(void)
{
	int i, fps;

	for (i = 0, fps = MAX_FPS_LIMIT;
	    fps >= MIN_FPS_LIMIT && i < MAX_FPS_LEVELS;
	    i++, fps -= DEFAULT_FPS_LEVEL)
		fps_level[i] = fps;

	nr_fps_levels = i;
}

static int clfps_level_read(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "%d ", nr_fps_levels);
	for (i = 0; i < nr_fps_levels; i++)
		seq_printf(m, "%d ", fps_level[i]);
	seq_puts(m, "\n");

	return 0;
}

static ssize_t clfps_level_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char *buf;
	int i, ret = -EINVAL;

	/* we do not allow change fps_level during fps throttling,
	 * value of curr_fps_level would be changed. */
	if (curr_fps_level != 0)
		return -EAGAIN;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -EFAULT;

	if (copy_from_user(buf, buffer, count)) {
		ret = -EFAULT;
		goto exit;
	}
	buf[count] = '\0';

	mtk_cooler_fps_dprintk_always("[%s] buf: %s\n", __func__, buf);

	if (1 <= sscanf(buf, "%d %d %d %d %d %d %d",
					&nr_fps_levels, &fps_level[0], &fps_level[1], &fps_level[2],
					&fps_level[3], &fps_level[4], &fps_level[5])) {

		if ((nr_fps_levels > MAX_FPS_LEVELS) || (nr_fps_levels < 0)) {
			mtk_cooler_fps_dprintk_always("[%s] nr_fps_levels: %d\n", __func__, nr_fps_levels);
			ret = -EINVAL;
			goto exit;
		}

		for (i = 0; i < nr_fps_levels; i++) {
			if (fps_level[i] > MAX_FPS_LIMIT || fps_level[i] < MIN_FPS_LIMIT) {
				mtk_cooler_fps_dprintk_always("[%s] fps_level: %d\n", __func__, fps_level[i]);
				ret = -EINVAL;
				goto exit;
			}
		}

		kfree(buf);
		return count;
	}

exit:
	kfree(buf);
	if (ret < 0)
		reset_fps_level();

	return ret;
}

static int clfps_level_open(struct inode *inode, struct file *file)
{
	return single_open(file, clfps_level_read, NULL);
}

static const struct file_operations clfps_level_fops = {
	.owner = THIS_MODULE,
	.open = clfps_level_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clfps_level_write,
	.release = single_release,
};

static int clfps_adp_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d %d\n", MIN_FPS_LIMIT, MAX_FPS_LIMIT);

	return 0;
}

static ssize_t clfps_adp_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char *buf;
	int _k_tt, _k_sum_tt, _min, _max;
	int ret = -EINVAL;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -EFAULT;

	if (copy_from_user(buf, buffer, count)) {
		ret = -EFAULT;
		goto exit;
	}

	buf[count] = '\0';

	if (sscanf(buf, "%d %d %d %d", &_k_tt, &_k_sum_tt, &_min, &_max) == 5) {
		/* TODO: check the values are valid */
		ret = count;
	}

exit:
	kfree(buf);
	return ret;
}

static int clfps_adp_open(struct inode *inode, struct file *file)
{
	return single_open(file, clfps_adp_read, NULL);
}

static const struct file_operations clfps_adp_fops = {
	.owner = THIS_MODULE,
	.open = clfps_adp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clfps_adp_write,
	.release = single_release,
};
#endif


/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_fps_ops = {
	.get_max_state = mtk_cl_fps_get_max_state,
	.get_cur_state = mtk_cl_fps_get_cur_state,
	.set_cur_state = mtk_cl_fps_set_cur_state,
};

static int mtk_cooler_fps_register_ltf(void)
{
	int i;

	mtk_cooler_fps_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_FPS; i-- > 0; ) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-fps%02d", i);
		/* put fps state to cooler devdata */
		cl_fps_dev[i] = mtk_thermal_cooling_device_register(temp, (void *) &cl_fps_state[i],
								&mtk_cl_fps_ops);
}

#if ADAPTIVE_FPS_COOLER
				cl_adp_fps_dev = mtk_thermal_cooling_device_register("mtk-cl-adp-fps", NULL,
							&mtk_cl_adp_fps_ops);
#endif

	return 0;
}

static void mtk_cooler_fps_unregister_ltf(void)
{
	int i;

	mtk_cooler_fps_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_FPS; i-- > 0; ) {
		if (cl_fps_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_fps_dev[i]);
			cl_fps_dev[i] = NULL;
			cl_fps_state[i] = 0;
		}
	}

#if ADAPTIVE_FPS_COOLER
	if (cl_adp_fps_dev) {
		mtk_thermal_cooling_device_unregister(cl_adp_fps_dev);
		cl_adp_fps_dev = NULL;
		cl_adp_fps_state = 0;
	}
#endif
}

static int mtk_cl_fps_proc_read(struct seq_file *m, void *v)
{
    /**
     * The format to print out:
     *  kernel_log <0 or 1>
     *  <mtk-cl-fps<ID>> <limited fps> <param> <state>
     *  ..
     */
	{
		int i = 0;

		seq_printf(m, "klog %d\n", cl_fps_klog_on);
		seq_printf(m, "curr_limit %d\n", cl_fps_cur_limit);

		for (; i < MAX_NUM_INSTANCE_MTK_COOLER_FPS; i++) {
			unsigned int active;
			unsigned long curr_state;

			active = cl_fps_param[i];

			MTK_CL_FPS_GET_CURR_STATE(curr_state, cl_fps_state[i]);

			seq_printf(m, "mtk-cl-fps%02d %u 0x%x, state %lu\n", i, active, cl_fps_param[i], curr_state);
		}
	}
	return 0;
}

static ssize_t mtk_cl_fps_proc_write(struct file *filp, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[128];
	int klog_on, fps0, fps1, fps2, fps3;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	/**
		* sscanf format <klog_on> <mtk-cl-fps00> <mtk-cl-fps01> <mtk-cl-fps02> ...
		* <klog_on> can only be 0 or 1
		*/

	if (NULL == data) {
		mtk_cooler_fps_dprintk("[%s] null data\n", __func__);
		return -EINVAL;
	}

/* WARNING: Modify here if MAX_NUM_INSTANCE_MTK_COOLER_FPS is changed to other than 4 */
#if (4 == MAX_NUM_INSTANCE_MTK_COOLER_FPS)

	if (1 <= sscanf(desc, "%d %d %d %d %d",
					&klog_on, &fps0, &fps1, &fps2, &fps3)) {
		if (klog_on == 0 || klog_on == 1)
			cl_fps_klog_on = klog_on;

		if (fps0 == 0)
			cl_fps_param[0] = 0;
		else if (fps0 >= 10 && fps0 <= 60)
			cl_fps_param[0] =  fps0;

		if (fps1 == 0)
			cl_fps_param[1] = 0;
		else if (fps1 >= 10 && fps1 <= 60)
			cl_fps_param[1] = fps1;

		if (fps2 == 0)
			cl_fps_param[2] = 0;
		else if (fps2 >= 10 && fps2 <= 60)
			cl_fps_param[2] = fps2;

		if (fps3 == 0)
			cl_fps_param[3] = 0;
		else if (fps3 >= 10 && fps3 <= 60)
			cl_fps_param[3] = fps3;

		return count;
	}
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_FPS!"
#endif

	mtk_cooler_fps_dprintk("[%s] bad arg\n", __func__);
	return -EINVAL;
}

static int mtk_cl_fps_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_cl_fps_proc_read, NULL);
}

static const struct file_operations cl_fps_fops = {
	.owner = THIS_MODULE,
	.open = mtk_cl_fps_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_cl_fps_proc_write,
	.release = single_release,
};


static ssize_t fps_tm_count_write(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	char tmp[32] = {0};

	len = (len < (sizeof(tmp) - 1)) ? len : (sizeof(tmp) - 1);

	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	if (kstrtoint(tmp, 10, &tm_input_fps) == 0) {

		mtk_cooler_fps_dprintk("[%s] = %d\n", __func__, tm_input_fps);
		return len;
	}

	mtk_cooler_fps_dprintk("[%s] invalid input\n", __func__);

	return -EINVAL;
}

static int fps_tm_count_read(struct seq_file *m, void *v)
{

	seq_printf(m, "%d\n", tm_input_fps);

	mtk_cooler_fps_dprintk("[%s] %d\n", __func__, tm_input_fps);

	return 0;
}

static int fps_tm_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, fps_tm_count_read, PDE_DATA(inode));
}

static const struct file_operations tm_fps_fops = {
	.owner = THIS_MODULE,
	.open = fps_tm_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fps_tm_count_write,
	.release = single_release,
};

/* ====FPS_DEBUGFS=========
#define debugfs_entry(name) \
do { \
		dentry_f = debugfs_create_u32(#name, S_IWUSR | S_IRUGO, _d, &name); \
		if (IS_ERR_OR_NULL(dentry_f)) {	\
			pr_warn("Unable to create debugfsfile: " #name "\n"); \
			return; \
		} \
} while (0)

static void create_debugfs_entries(void)
{
	struct dentry *dentry_f;
	struct dentry *_d;

	_d = debugfs_create_dir("clfps", NULL);
	if (IS_ERR_OR_NULL(_d)) {
		pr_info("unable to create debugfs directory\n");
		return;
	}

	debugfs_entry(fps_error_threshold);
	debugfs_entry(fps_stable_period);
	debugfs_entry(curr_fps_level);
	debugfs_entry(in_game_whitelist);
	debugfs_entry(fps_limit_always_on);
	debugfs_entry(leave_fps_limit_duration);
	debugfs_entry(in_game_low_fps);
	debugfs_entry(gpu_loading_threshold);
}

#undef debugfs_entry
====FPS_DEBUGFS=========== */

static int __init mtk_cooler_fps_init(void)
{
	int ret = 0;
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_FPS; i-- > 0; ) {
		cl_fps_dev[i] = NULL;
		cl_fps_state[i] = 0;
	}

	mtk_cooler_fps_dprintk("init\n");

	err = mtk_cooler_fps_register_ltf();
	if (err)
		goto err_unreg;

	/* switch device to sent the (fps limit)uevent */
	fps_switch_data.name  = "fps";
	fps_switch_data.index = 0;
	fps_switch_data.state = 60;  /* original 60 frames */
	ret = switch_dev_register(&fps_switch_data);

	if (ret)
		mtk_cooler_fps_dprintk_always("[%s] switch_dev_register failed, returned:%d!\n",
						__func__, ret);

	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;
		struct proc_dir_entry *dir_entry = NULL;
		struct proc_dir_entry *fps_tm_proc_dir = NULL;

		fps_tm_proc_dir = proc_mkdir("fps_tm", NULL);
		if (!fps_tm_proc_dir)
			mtk_cooler_fps_dprintk_always("[%s]: mkdir /proc/fps_tm failed\n", __func__);
		else {
			entry = proc_create("fps_count", S_IRUGO | S_IWUSR | S_IWGRP, fps_tm_proc_dir, &tm_fps_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
		}

		dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
		if (!dir_entry)
			mtk_cooler_fps_dprintk_always("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
		else {
			entry = proc_create("clfps", S_IRUGO | S_IWUSR | S_IWGRP, dir_entry, &cl_fps_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
		}

#if ADAPTIVE_FPS_COOLER
		reset_fps_level();
		if (dir_entry) {
			entry = proc_create("clfps_adp", S_IRUGO | S_IWUSR | S_IWGRP,
					dir_entry, &clfps_adp_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
			entry = proc_create("clfps_level", S_IRUGO | S_IWUSR | S_IWGRP,
				dir_entry, &clfps_level_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
		}
/* ====FPS_DEBUGFS=========
		create_debugfs_entries();
====FPS_DEBUGFS=========== */


#endif

	return 0;
}
err_unreg:
	mtk_cooler_fps_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_fps_exit(void)
{
	mtk_cooler_fps_dprintk("exit\n");

	/* remove the proc file */
	remove_proc_entry("clfps", NULL);

	mtk_cooler_fps_unregister_ltf();
}

module_init(mtk_cooler_fps_init);
module_exit(mtk_cooler_fps_exit);
