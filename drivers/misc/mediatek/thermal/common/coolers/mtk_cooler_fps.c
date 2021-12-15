// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
/*kernel4.4 #include <linux/mtk_gpu_utility.h>*/
#include "mt-plat/mtk_thermal_monitor.h"
/* fps update from display */
#ifdef CONFIG_MTK_FB
#include "disp_session.h"
#endif

#define SUPPORT_SWITCH_DEV	(0)

#if SUPPORT_SWITCH_DEV
/* switch device to sent the (fps limit)uevent */
#include <linux/switch.h>
#endif

#include "mach/mtk_thermal.h"
#include <linux/uidgid.h>
#ifdef CONFIG_MTK_GPU_SUPPORT
#include "ged_dvfs.h"
#endif
#include <mtk_cooler_setting.h>

/* 1: turn on adaptive fps cooler; 0: turn off */
#define ADAPTIVE_FPS_COOLER              (1)

#ifdef CONFIG_MTK_DYNAMIC_FPS_FRAMEWORK_SUPPORT

#if defined(CONFIG_MTK_FPSGO) || defined(CONFIG_MTK_FPSGO_V3)
	#define FPS_COOLER_USE_DFPS				(0)
#else
	#define FPS_COOLER_USE_DFPS				(1)
#endif

#else
	#define FPS_COOLER_USE_DFPS				(0)
#endif

#if FPS_COOLER_USE_DFPS
#include "dfrc.h"
#include "dfrc_drv.h"
#endif

#define mtk_cooler_fps_dprintk_always(fmt, args...) \
pr_debug("[Thermal/TC/fps]" fmt, ##args)

#define mtk_cooler_fps_dprintk(fmt, args...) \
do { \
	if (cl_fps_klog_on == 1) \
		pr_notice("[Thermal/TC/fps]" fmt, ##args); \
} while (0)

#define MAX_NUM_INSTANCE_MTK_COOLER_FPS  4

#define MTK_CL_FPS_GET_CURR_STATE(curr_state, state) \
{ curr_state = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_FPS_SET_CURR_STATE(curr_state, state) \
do { \
	if (curr_state == 0) \
		state &= ~0x1; \
	else \
		state |= 0x1; \
} while (0)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static int cl_fps_klog_on;
static struct thermal_cooling_device
			*cl_fps_dev[MAX_NUM_INSTANCE_MTK_COOLER_FPS] = { 0 };

static unsigned int cl_fps_param[MAX_NUM_INSTANCE_MTK_COOLER_FPS] = { 0 };
static unsigned long cl_fps_state[MAX_NUM_INSTANCE_MTK_COOLER_FPS] = { 0 };

static unsigned int cl_fps_cur_limit;
static unsigned int tm_input_fps;

#if SUPPORT_SWITCH_DEV
static struct switch_dev fps_switch_data;
#endif

#define FPS_STATS_WAKEUP_TIME_MS		(1000)
#define FPS_STATS_START_TIME_MS		(60000)
struct delayed_work fps_stats_work;

#if ADAPTIVE_FPS_COOLER
/* TODO: TBD */
#define CFG_MAX_FPS_LIMIT		60
#define CFG_MIN_FPS_LIMIT		20

static int max_fps_limit = CFG_MAX_FPS_LIMIT;
static int min_fps_limit = CFG_MIN_FPS_LIMIT;

/* TODO: TBD */
#define MAX_NR_FPS_LEVELS	8
struct fps_level {
	int start;
	int end;
};
struct fps_level fps_levels[MAX_NR_FPS_LEVELS];
static int nr_fps_levels = MAX_NR_FPS_LEVELS;

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
static int cl_adp_fps_limit = CFG_MAX_FPS_LIMIT;

#define GPU_LOADING_THRESHOLD	60

/* in percentage */
static int fps_error_threshold = 10;
static int fps_target_bias = 5;
/* in round */
static int fps_stable_period = 10;
/* FPS is active when over stable tpcb or always */
static int fps_limit_always_on;
static int in_game_mode;
#if FPS_COOLER_USE_DFPS
static unsigned int fps_target_adjust;
#endif
#endif

#ifndef __GED_TYPE_H__
enum GED_INFO {
GED_EVENT_GAS_MODE,
GED_UNDEFINED
};

enum {
	GAS_CATEGORY_GAME,
	GAS_CATEGORY_OTHERS,
};
#endif

int __attribute__ ((weak))
disp_mgr_get_session_info(struct disp_session_info *info)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

unsigned long  __attribute__ ((weak))
ged_query_info(GED_INFO eType)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

bool  __attribute__ ((weak))
mtk_get_gpu_loading(unsigned int *pLoading)
{
#ifdef CONFIG_MTK_GPU_SUPPORT
	pr_notice("E_WF: %s doesn't exist\n", __func__);
#endif
	return 0;
}

#if FPS_COOLER_USE_DFPS
void dfrc_fps_limit_cb(int fps_limit)
{
	ktime_t cur_time;
	static ktime_t pre_time;
	static int pre_fps_limit;
	static bool fps_adjust_check;

	if ((in_game_mode) && (fps_limit != DFRC_DRV_FPS_NON_ASSIGN)) {
		cur_time = ktime_get();

		if (fps_adjust_check) {
			if ((cl_fps_cur_limit > pre_fps_limit)
			& (pre_fps_limit != -1))
				fps_target_adjust +=
					((cl_fps_cur_limit - pre_fps_limit) *
					ktime_to_ms(ktime_sub(
							cur_time, pre_time)));

			mtk_cooler_fps_dprintk(
				"[%s] dfrc fps: %d, current limit: %d, target adjuct: %d\n",
				__func__, pre_fps_limit, cl_fps_cur_limit,
				fps_target_adjust);
		}

		pre_fps_limit = fps_limit;
		pre_time = cur_time;
		fps_adjust_check = 1;
	} else {
		fps_target_adjust = 0;
		fps_adjust_check = 0;
	}
}
EXPORT_SYMBOL(dfrc_fps_limit_cb);
#endif

int clfps_get_game_mode(void)
{
	return in_game_mode;
}

int clfps_get_disp_fps(void)
{
	return tm_input_fps;
}

static int game_mode_check(void)
{
	unsigned long result = ged_query_info(GED_EVENT_GAS_MODE);

	if (result == GAS_CATEGORY_GAME)
		in_game_mode = 1;
	else
		in_game_mode = 0;

	return 0;
}

static int fps_update(void)
{
#ifdef CONFIG_MTK_FB
	struct disp_session_info info;


	memset(&info, 0, sizeof(info));
	info.session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);

	disp_mgr_get_session_info(&info);

	/* mtk_cooler_fps_dprintk("display update fps is: %d.%d\n",
	 * info.updateFPS/100, info.updateFPS%100);
	 */
	/* mtk_cooler_fps_dprintk("is display fps stable: %d\n",
	 * info.is_updateFPS_stable);
	 */
#if 0
	if (info.is_updateFPS_stable)
		tm_input_fps = info.updateFPS;
	else
		tm_input_fps = 0;
#else
	tm_input_fps = info.updateFPS/100;
#endif
#endif
	return 0;
}

static void mtk_cl_fps_set_fps_limit(void)
{
	int i = 0;
	int min_limit = 60;
	unsigned int min_param = 60;
#if FPS_COOLER_USE_DFPS
	int ret = -1;
#endif

	for (; i < MAX_NUM_INSTANCE_MTK_COOLER_FPS; i++) {
		unsigned long curr_state;

		MTK_CL_FPS_GET_CURR_STATE(curr_state, cl_fps_state[i]);

		if (curr_state == 1) {
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
#if FPS_COOLER_USE_DFPS
		ret = dfrc_set_kernel_policy(DFRC_DRV_API_THERMAL,
			((cl_fps_cur_limit != 60) ? cl_fps_cur_limit : -1),
			DFRC_DRV_MODE_INTERNAL_SW, 0, 0);

		mtk_cooler_fps_dprintk_always("[DFPS] fps:%d, ret = %d\n",
							cl_fps_cur_limit, ret);
#else
#if SUPPORT_SWITCH_DEV
		switch_set_state(&fps_switch_data, cl_fps_cur_limit);
#endif
#endif
		mtk_cooler_fps_dprintk_always("[%s] fps limit: %d\n", __func__,
							cl_fps_cur_limit);
	}

err_unreg:
	return;
}

static int mtk_cl_fps_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_fps_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	MTK_CL_FPS_GET_CURR_STATE(*state, *((unsigned long *) cdev->devdata));
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_fps_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, state);
	MTK_CL_FPS_SET_CURR_STATE(state, *((unsigned long *) cdev->devdata));

	mtk_cl_fps_set_fps_limit();

	return 0;
}

#if ADAPTIVE_FPS_COOLER
static int adp_fps_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int adp_fps_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
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

/* Increase FPS limit: current limit + 10% (fps_error_threshold)
 * @retval new fps limit
 *
 * search in ascending order
 * For discrete range: same as before, we select the upper one level that
 *                     is just larger than current target.
 * For contiguous range: if the new limit is between [start,end], use new limit
 */
static int increase_fps_limit(void)
{
	int i, target_limit, fps_limit = max_fps_limit;

	target_limit = cl_adp_fps_limit * (100 + fps_error_threshold) / 100;

	if (target_limit >= max_fps_limit)
		return max_fps_limit;

	for (i = nr_fps_levels - 1; i >= 0; i--) {
		if (fps_levels[i].start == fps_levels[i].end) {
			if (fps_levels[i].end >= target_limit) {
				fps_limit = fps_levels[i].end;
				break;
			}
		} else {
			if (fps_levels[i].start >= target_limit) {
				if (target_limit > fps_levels[i].end)
					fps_limit = target_limit;
				else
					fps_limit = fps_levels[i].end;
				break;
			}
		}
	}

	if (i < 0)
		fps_limit = max_fps_limit;

	return fps_limit;
}

/**
 * floor function applied to fps_levels
 * @retval new fps limit
 * search in descending order, find the range whose 'end' is smaller than fps
 * should consider fps_error_threshold
 */
int find_fps_floor(int fps)
{
	int i;

	for (i = 0; i < nr_fps_levels; i++) {
		/* sma_fps >= fps_limit * 90% */
		if (fps >= fps_levels[i].end *
				(100 - fps_error_threshold) / 100) {
			if (fps_levels[i].start >= fps
			&& fps >= fps_levels[i].end)
				return fps;
			else
				return fps_levels[i].end;
		}
	}

	return min_fps_limit;
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
	mtk_cooler_fps_dprintk("[%s] gpu_loading = %d\n",
						__func__, gpu_loading);

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
#if FPS_COOLER_USE_DFPS
	static int fixedT_period;
#endif
	int sma_tpcb, tpcb_change, sma_fps;
	int fps_limit = cl_adp_fps_limit;

	if (period < fps_stable_period) {
		period++;
		return fps_limit;
	}
	period = 0;

	sma_tpcb = get_sma_val(tpcb_history, tpcb_sma_len);
	tpcb_change = sma_tpcb - last_change_tpcb;

	sma_fps = get_sma_val(fps_history, fps_sma_len);

	mtk_cooler_fps_dprintk(
			"[%s] sma_tpcb = %d, tpcb_change = %d, sma_fps = %d\n",
		__func__, sma_tpcb,  tpcb_change, sma_fps);

#if FPS_COOLER_USE_DFPS
	/* [Todo] adjust the fps target here */
	/* fps_limit = fps_limit -(fps_target_adjust/(fps_stable_period*1000));
	 */
	fps_target_adjust = 0;

	/* Increase fps limit when tpcb keep stable for 100 sec */
	if (tpcb_change == 0)
		fixedT_period++;
	else
		fixedT_period = 0;
#endif

	if (fps_limit_always_on || sma_tpcb >= mtk_thermal_get_tpcb_target()) {
		/* FPS variation is HIGH */
		if (fps_limit - sma_fps
		> fps_limit * (fps_error_threshold + fps_target_bias) / 100) {
			/* TODO: TBD: is "sma_fpa < 40" still necessary? */
#if FPS_COOLER_USE_DFPS
			if (is_system_too_busy() && (!is_cpu_power_unlimit())) {
				sma_fps = sma_fps +
					fps_limit * fps_target_bias / 100;
#else
			if (sma_fps < 40 && is_system_too_busy()) {
#endif
				fps_limit = find_fps_floor(sma_fps);
				mtk_cooler_fps_dprintk(
					"[%s] new_fps_limit = %d\n", __func__,
					fps_limit);
			}
		} else {
			/* For always-on and low tpcb */
#if FPS_COOLER_USE_DFPS
			if (fixedT_period
				>= fps_stable_period || tpcb_change < 0) {
				fps_limit = increase_fps_limit();
				fixedT_period = 0;
			}
#else
			if (sma_tpcb < mtk_thermal_get_tpcb_target()
			&& tpcb_change < 0)
				fps_limit = increase_fps_limit();
#endif
		}
	} else if (tpcb_change < 0) { /* not always-on and low tpcb */
		if (fps_limit - sma_fps
			<= fps_limit * fps_error_threshold / 100)
			fps_limit = increase_fps_limit();
	}

	if (tpcb_change)
		last_change_tpcb = sma_tpcb;

	return fps_limit;
}

static void clfps_fps_stats(struct work_struct *work)
{
	/* check the fps update from display */
	fps_update();
	set_sma_val(fps_history, fps_sma_len, &fps_history_idx, tm_input_fps);

	queue_delayed_work(system_unbound_wq, &fps_stats_work,
				msecs_to_jiffies(FPS_STATS_WAKEUP_TIME_MS));
}

static int adp_fps_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	int gpu_loading;

	if ((state != 0) && (state != 1)) {
		mtk_cooler_fps_dprintk(
				"[%s] invalid input (0: no thro; 1: adp fps thro on)\n",
				__func__);

		return 0;
	}

	mtk_cooler_fps_dprintk("[%s] %s %lu\n", __func__, cdev->type, state);
	cl_adp_fps_state = state;

	/* game? */
	game_mode_check();

	set_sma_val(tpcb_history, tpcb_sma_len, &tpcb_history_idx,
			mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP));

	if (!mtk_get_gpu_loading(&gpu_loading))
		gpu_loading = 0;

	set_sma_val(gpu_loading_history, gpu_loading_sma_len,
						&gpu_loading_history_idx,
			gpu_loading);

	/* 1. update the parameter of "cl_adp_fps_limit" */
	/* we do not limit FPS if not in game */
	if (in_game_mode && (cl_adp_fps_state || fps_limit_always_on))
		cl_adp_fps_limit = adp_calc_fps_limit();
	else
		cl_adp_fps_limit = max_fps_limit;

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

	for (i = 0; i < 4; i++) {
		fps = CFG_MAX_FPS_LIMIT / (i + 1);
		fps_levels[i].start = fps;
		fps_levels[i].end = fps;
	}

	nr_fps_levels = 4;
}

static int clfps_level_read(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "%d ", nr_fps_levels);
	for (i = 0; i < nr_fps_levels; i++)
		seq_printf(m, "%d-%d ", fps_levels[i].start, fps_levels[i].end);
	seq_puts(m, "\n");

	return 0;
}

/* format example: 4 60-45 30-30 20-20 15-15
 * compatible: 4 60 30 20 15
 * mixed: 4 60-45 30 20 15
 */
static ssize_t clfps_level_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char *buf, *sepstr, *substr;
	int ret = -EINVAL, new_nr_fps_levels, i, start_fps, end_fps;
	struct fps_level *new_levels;

	/* we do not allow change fps_level during fps throttling,
	 * because fps_levels would be changed.
	 */
	if (cl_adp_fps_limit != max_fps_limit)
		return -EAGAIN;

	if (count >= 128 || count < 1)
		return -EINVAL;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	new_levels = kmalloc(sizeof(fps_levels), GFP_KERNEL);
	if (new_levels == NULL) {
		ret = -ENOMEM;
		goto err_freebuf;
	}

	if (copy_from_user(buf, buffer, count)) {
		ret = -EFAULT;
		goto err;
	}
	buf[count] = '\0';
	sepstr = buf;

	substr = strsep(&sepstr, " ");
	if (kstrtoint(substr, 10, &new_nr_fps_levels) != 0 ||
		((new_nr_fps_levels > MAX_NR_FPS_LEVELS) ||
		(new_nr_fps_levels <= 0)))  {
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < new_nr_fps_levels; i++) {
		substr = strsep(&sepstr, " ");
		if (!substr) {
			ret = -EINVAL;
			goto err;
		}
		if (strchr(substr, '-')) { /* maybe contiguous */
			if (sscanf(substr, "%d-%d",
				&start_fps, &end_fps) != 2) {
				ret = -EINVAL;
				goto err;
			}
			new_levels[i].start = start_fps;
			new_levels[i].end = end_fps;
		} else { /* discrete */
			if (kstrtoint(substr, 10, &start_fps) != 0) {
				ret = -EINVAL;
				goto err;
			}
			new_levels[i].start = start_fps;
			new_levels[i].end = start_fps;
		}
	}

	for (i = 0; i < new_nr_fps_levels; i++) {
		/* check if they are interleaving */
		if (new_levels[i].end > new_levels[i].start ||
		    (i > 0 && new_levels[i].start > new_levels[i - 1].end)) {
			ret = -EINVAL;
			goto err;
		}
	}

	ret = count;
	nr_fps_levels = new_nr_fps_levels;
	memcpy(fps_levels, new_levels, sizeof(fps_levels));
	max_fps_limit = fps_levels[0].start;
	min_fps_limit = fps_levels[nr_fps_levels - 1].end;
err:
	kfree(new_levels);
err_freebuf:
	kfree(buf);

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
		cl_fps_dev[i] = mtk_thermal_cooling_device_register(
						temp, (void *) &cl_fps_state[i],
						&mtk_cl_fps_ops);
}

#if ADAPTIVE_FPS_COOLER
	cl_adp_fps_dev = mtk_thermal_cooling_device_register("mtk-cl-adp-fps",
							NULL,
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

			seq_printf(m, "mtk-cl-fps%02d %u 0x%x, state %lu\n", i,
					active, cl_fps_param[i], curr_state);
		}
	}
	return 0;
}

static ssize_t mtk_cl_fps_proc_write(
struct file *filp, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[128];
	int klog_on, fps0, fps1, fps2, fps3;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	/*
	 * sscanf format <klog_on> <mtk-cl-fps00> <mtk-cl-fps01> <mtk-cl-fps02>
	 * <klog_on> can only be 0 or 1
	 */

	if (data == NULL) {
		mtk_cooler_fps_dprintk("[%s] null data\n", __func__);
		return -EINVAL;
	}

/* WARNING: Modify here if MAX_NUM_INSTANCE_MTK_COOLER_FPS
 * is changed to other than 4
 */
#if (MAX_NUM_INSTANCE_MTK_COOLER_FPS == 4)

	if (sscanf(desc, "%d %d %d %d %d", &klog_on,
		&fps0, &fps1, &fps2, &fps3) >= 1) {
		if (klog_on == 0 || klog_on == 1)
			cl_fps_klog_on = klog_on;

		/* [Fix me] debug only */
		fps_limit_always_on = 0;
		if (klog_on == 2)
			fps_limit_always_on = 1;

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
#error	\
"Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_FPS!"
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


static ssize_t fps_tm_count_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
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
	seq_printf(m, "%d,%d,%d\n", tm_input_fps,
					cl_fps_cur_limit, in_game_mode);

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
 *#define debugfs_entry(name) \
 *do { \
 *		dentry_f = debugfs_create_u32
 *     (#name, S_IWUSR | S_IRUGO, _d, &name); \
 *		if (IS_ERR_OR_NULL(dentry_f)) {	\
 *		pr_notice("Unable to create debugfsfile: " #name "\n"); \
 *			return; \
 *		} \
 *} while (0)
 *
 *static void create_debugfs_entries(void)
 *{
 *	struct dentry *dentry_f;
 *	struct dentry *_d;
 *
 *	_d = debugfs_create_dir("clfps", NULL);
 *	if (IS_ERR_OR_NULL(_d)) {
 *		pr_info("unable to create debugfs directory\n");
 *		return;
 *	}
 *
 *	debugfs_entry(fps_error_threshold);
 *	debugfs_entry(fps_stable_period);
 *	debugfs_entry(in_game_mode);
 *	debugfs_entry(fps_limit_always_on);
 *}
 *#undef debugfs_entry
 *====FPS_DEBUGFS===========
 */

static int __init mtk_cooler_fps_init(void)
{
#if SUPPORT_SWITCH_DEV
	int ret = 0;
#endif
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

#if SUPPORT_SWITCH_DEV
	/* switch device to sent the (fps limit)uevent */
	fps_switch_data.name  = "fps";
	fps_switch_data.index = 0;
	fps_switch_data.state = 60;  /* original 60 frames */
	ret = switch_dev_register(&fps_switch_data);

	if (ret)
		mtk_cooler_fps_dprintk_always(
					"[%s] switch_dev_register failed, returned:%d!\n",
						__func__, ret);
#endif
	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;
		struct proc_dir_entry *dir_entry = NULL;
		struct proc_dir_entry *fps_tm_proc_dir = NULL;

		fps_tm_proc_dir = proc_mkdir("fps_tm", NULL);
		if (!fps_tm_proc_dir)
			mtk_cooler_fps_dprintk_always(
					"[%s]: mkdir /proc/fps_tm failed\n",
					__func__);
		else {
			entry = proc_create("fps_count", 0664,
						fps_tm_proc_dir, &tm_fps_fops);

			if (entry)
				proc_set_user(entry, uid, gid);
		}

		dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
		if (!dir_entry)
			mtk_cooler_fps_dprintk_always(
					"[%s]: mkdir /proc/driver/thermal failed\n",
					__func__);
		else {
			entry = proc_create("clfps", 0664,
						dir_entry, &cl_fps_fops);

			if (entry)
				proc_set_user(entry, uid, gid);
		}

#if ADAPTIVE_FPS_COOLER
		reset_fps_level();
		if (dir_entry) {
			entry = proc_create("clfps_level", 0664,
				dir_entry, &clfps_level_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
		}

/* ====FPS_DEBUGFS=========
 *		create_debugfs_entries();
 *====FPS_DEBUGFS===========
 */

#endif

	INIT_DEFERRABLE_WORK(&fps_stats_work, clfps_fps_stats);
	queue_delayed_work(system_unbound_wq, &fps_stats_work,
				msecs_to_jiffies(FPS_STATS_START_TIME_MS));

	return 0;
}
err_unreg:
	mtk_cooler_fps_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_fps_exit(void)
{
	mtk_cooler_fps_dprintk("exit\n");

	if (delayed_work_pending(&fps_stats_work))
		cancel_delayed_work(&fps_stats_work);

	/* remove the proc file */
	remove_proc_entry("clfps", NULL);

	mtk_cooler_fps_unregister_ltf();
}

module_init(mtk_cooler_fps_init);
module_exit(mtk_cooler_fps_exit);
