/*
 * drivers/video/tegra/host/gr3d/pod_scaling.c
 *
 * Tegra Graphics Host 3D clock scaling
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Power-on-demand clock scaling for nvhost devices
 *
 * devfreq calls nvhost_pod_estimate_freq() for estimating the new
 * frequency for the device. The clocking is done using two properties:
 *
 *  (1) Usually the governor receives actively throughput hints that indicate
 *      whether scaling up or down is required.
 *  (2) The load of the device is estimated using the busy times from the
 *      device profile. This information indicates if the device frequency
 *      should be altered.
 *
 */

#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/slab.h>

#define CREATE_TRACE_POINTS
#include <trace/events/nvhost_podgov.h>

#include <mach/clk.h>
#include <mach/hardware.h>

#include <governor.h>

#include "scale3d.h"
#include "pod_scaling.h"
#include "dev.h"

/* time frame for load and hint tracking - when events come in at a larger
 * interval, this probably indicates the current estimates are stale
 */
#define GR3D_TIMEFRAME 1000000 /* 1 sec */

/* the number of frames to use in the running average of load estimates and
 * throughput hints. Choosing 6 frames targets a window of about 100 msec.
 * Large flucutuations in frame times require a window that's large enough to
 * prevent spiky scaling behavior, which in turn exacerbates frame rate
 * instability.
 */

static int podgov_is_enabled(struct device *dev);
static void podgov_enable(struct device *dev, int enable);
static int podgov_user_ctl(struct device *dev);
static void podgov_set_user_ctl(struct device *dev, int enable);

/*******************************************************************************
 * podgov_info_rec - gr3d scaling governor specific parameters
 ******************************************************************************/

struct podgov_info_rec {

	int			enable;
	int			init;
	int			fast_up_count;
	int			slow_down_count;
	int			is_scaled;

	unsigned long		idle_total;
	unsigned long		idle_short_term_total;

	ktime_t			last_throughput_hint;
	ktime_t			last_scale;
	ktime_t			last_adjust;
	ktime_t			last_estimation_window;
	ktime_t			estimation_window;
	ktime_t			last_notification;

	struct work_struct	work;
	struct delayed_work	idle_timer;

	unsigned int		scale;
	unsigned int		p_estimation_window;
	unsigned int		p_use_throughput_hint;
	unsigned int		p_hint_lo_limit;
	unsigned int		p_hint_hi_limit;
	unsigned int		p_scaleup_limit;
	unsigned int		p_scaledown_limit;
	unsigned int		p_smooth;
	unsigned int		p_idle_min;
	unsigned int		idle_min;
	unsigned int		p_idle_max;
	unsigned int		idle_max;
	unsigned int		p_adjust;
	unsigned int		p_user;
	unsigned int		p_freq_request;

	long			last_total_idle;
	long			total_idle;
	long			idle_estimate;

	int			adjustment_type;
	unsigned long		adjustment_frequency;

	int			last_event_type;

	struct devfreq		*power_manager;
	struct dentry		*debugdir;

	int			*freqlist;
	int			freq_count;

	unsigned int		idle_avg;
	unsigned int		hint_avg;
	int			block;

};

/*******************************************************************************
 * Adjustment type is used to tell the source that requested frequency re-
 * estimation. Type ADJUSTMENT_LOCAL indicates that the re-estimation was
 * initiated by the governor itself. This happens when one of the worker
 * threads want to adjust the frequency.
 *
 * ADJUSTMENT_DEVICE_REQ (default value) indicates that the adjustment was
 * initiated by a device event.
 ******************************************************************************/

enum podgov_adjustment_type {
	ADJUSTMENT_LOCAL = 0,
	ADJUSTMENT_DEVICE_REQ = 1
};


/* Some functions cannot get pointer to podgov anywhere :-(
 * Yes, this should be fixed */
struct podgov_info_rec *local_podgov;

/*******************************************************************************
 * scaling_limit(df, freq)
 *
 * Limit the given frequency
 ******************************************************************************/

static void scaling_limit(struct devfreq *df, unsigned long *freq)
{
	if (*freq < df->min_freq)
		*freq = df->min_freq;
	else if (*freq > df->max_freq)
		*freq = df->max_freq;
}

/*******************************************************************************
 * podgov_clocks_handler(work)
 *
 * This handler is called periodically to re-estimate the frequency using the
 * relation between "fast scale ups" and "slow scale downs".
 ******************************************************************************/

static void podgov_clocks_handler(struct work_struct *work)
{
	struct podgov_info_rec *podgov =
		container_of(work, struct podgov_info_rec, work);
	struct devfreq *df = podgov->power_manager;
	unsigned long freq;

	mutex_lock(&df->lock);

	if (!podgov->enable) {
		mutex_unlock(&df->lock);
		return;
	}
	if (podgov->scale == 0) {
		mutex_unlock(&df->lock);
		return;
	}

	freq = podgov->scale * (df->previous_freq / 100);
	scaling_limit(df, &freq);

	if (df->previous_freq != freq) {
		trace_podgov_clocks_handler(df->previous_freq, freq);
		podgov->adjustment_frequency = freq;
		podgov->adjustment_type = ADJUSTMENT_LOCAL;
		update_devfreq(df);
	}

	mutex_unlock(&df->lock);
}

/*******************************************************************************
 * nvhost_scale3d_suspend(dev)
 *
 * Prepare the device for suspend
 ******************************************************************************/

void nvhost_scale3d_suspend(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct devfreq *df = pdata->power_manager;
	struct podgov_info_rec *podgov;

	if (!df)
		return;

	mutex_lock(&df->lock);
	podgov = df->data;
	if (!podgov->enable) {
		mutex_unlock(&df->lock);
		return;
	}

	cancel_work_sync(&podgov->work);
	cancel_delayed_work(&podgov->idle_timer);

	mutex_unlock(&df->lock);
}

/*******************************************************************************
 * podgov_is_enabled(dev)
 *
 * Check whether the device is enabled or not.
 ******************************************************************************/

static int podgov_is_enabled(struct device *dev)
{
	struct platform_device *d = to_platform_device(dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(d);
	struct devfreq *df = pdata->power_manager;
	struct podgov_info_rec *podgov;
	int enable;

	if (!df)
		return 0;

	mutex_lock(&df->lock);
	podgov = df->data;
	enable = podgov->enable;
	mutex_unlock(&df->lock);

	return enable;
}

/*******************************************************************************
 * podgov_enable(dev, enable)
 *
 * This function enables (enable=1) or disables (enable=0) the automatic scaling
 * of the device. If the device is disabled, the device's clock is set to its
 * maximum.
 ******************************************************************************/

static void podgov_enable(struct device *dev, int enable)
{
	struct platform_device *d = to_platform_device(dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(d);
	struct devfreq *df = pdata->power_manager;
	struct podgov_info_rec *podgov;

	if (!df)
		return;

	mutex_lock(&df->lock);
	podgov = df->data;

	trace_podgov_enabled(enable);

	if (enable && df->min_freq != df->max_freq) {
		podgov->enable = 1;
	} else {
		cancel_work_sync(&podgov->work);
		cancel_delayed_work(&podgov->idle_timer);
		podgov->enable = 0;
		podgov->adjustment_frequency = df->max_freq;
		podgov->adjustment_type = ADJUSTMENT_LOCAL;
		update_devfreq(df);
	}
	mutex_unlock(&df->lock);
}

/*******************************************************************************
 * podgov_user_ctl(dev)
 *
 * Check whether the gpu scaling is set to user space control.
 ******************************************************************************/

static int podgov_user_ctl(struct device *dev)
{
	struct platform_device *d = to_platform_device(dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(d);
	struct devfreq *df = pdata->power_manager;
	struct podgov_info_rec *podgov;
	int user;

	if (!df)
		return 0;

	mutex_lock(&df->lock);
	podgov = df->data;
	user = podgov->p_user;
	mutex_unlock(&df->lock);

	return user;
}

/*****************************************************************************
 * podgov_set_user_ctl(dev, user)
 *
 * This function enables or disables user control of the gpu. If user control
 * is enabled, setting the freq_request controls the gpu frequency, and other
 * gpu scaling mechanisms are disabled.
 ******************************************************************************/

static void podgov_set_user_ctl(struct device *dev, int user)
{
	struct platform_device *d = to_platform_device(dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(d);
	struct devfreq *df = pdata->power_manager;
	struct podgov_info_rec *podgov;
	int cancel = 0;

	if (!df)
		return;

	mutex_lock(&df->lock);
	podgov = df->data;

	trace_podgov_set_user_ctl(user);

	if (podgov->enable && user && !podgov->p_user) {
		cancel = 1;
		cancel_delayed_work(&podgov->idle_timer);

		podgov->adjustment_frequency = podgov->p_freq_request;
		podgov->adjustment_type = ADJUSTMENT_LOCAL;
		update_devfreq(df);
	}

	podgov->p_user = user;

	mutex_unlock(&df->lock);
	if (cancel)
		cancel_work_sync(&podgov->work);
}

/*******************************************************************************
 * podgov_get_freq_request(dev)
 *
 * return the latest freq request if anybody asks
 ******************************************************************************/

static int podgov_get_freq_request(struct device *dev)
{
	struct platform_device *d = to_platform_device(dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(d);
	struct devfreq *df = pdata->power_manager;
	struct podgov_info_rec *podgov;
	int freq_request;

	if (!df)
		return 0;

	mutex_lock(&df->lock);
	podgov = df->data;
	freq_request = podgov->p_freq_request;
	mutex_unlock(&df->lock);

	return freq_request;
}

/*****************************************************************************
 * podgov_set_freq_request(dev, user)
 *
 * Set the current freq request. If scaling is enabled, and podgov user space
 * control is enabled, this will set the gpu frequency.
 ******************************************************************************/

static void podgov_set_freq_request(struct device *dev, int freq_request)
{
	struct platform_device *d = to_platform_device(dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(d);
	struct devfreq *df = pdata->power_manager;
	struct podgov_info_rec *podgov;

	if (!df)
		return;

	mutex_lock(&df->lock);
	podgov = df->data;

	trace_podgov_set_freq_request(freq_request);

	podgov->p_freq_request = freq_request;

	if (podgov->enable && podgov->p_user) {

		podgov->adjustment_frequency = freq_request;
		podgov->adjustment_type = ADJUSTMENT_LOCAL;
		update_devfreq(df);
	}

	mutex_unlock(&df->lock);
}

/*******************************************************************************
 * scaling_adjust(podgov, time)
 *
 * use scale up / scale down hint counts to adjust scaling parameters.
 *
 * hint_ratio is 100 x the ratio of scale up to scale down hints. Three cases
 * are distinguished:
 *
 * hint_ratio < HINT_RATIO_MIN - set parameters to maximize scaling effect
 * hint_ratio > HINT_RATIO_MAX - set parameters to minimize scaling effect
 * hint_ratio between limits - scale parameters linearly
 *
 * the parameters adjusted are
 *
 *  - fast_response time
 *  - period - time for scaling down estimate
 *  - idle_min percentage
 *  - idle_max percentage
 *
 ******************************************************************************/

#define SCALING_ADJUST_PERIOD	1000000
#define HINT_RATIO_MAX		400
#define HINT_RATIO_MIN		100
#define HINT_RATIO_MID		((HINT_RATIO_MAX + HINT_RATIO_MIN) / 2)
#define HINT_RATIO_DIFF		(HINT_RATIO_MAX - HINT_RATIO_MIN)

static void scaling_adjust(struct podgov_info_rec *podgov, ktime_t time)
{
	long hint_ratio;
	int idle_min_adjustment;
	int idle_max_adjustment;
	unsigned long dt;

	dt = (unsigned long) ktime_us_delta(time, podgov->last_adjust);
	if (dt < SCALING_ADJUST_PERIOD)
		return;

	hint_ratio = (100 * (podgov->fast_up_count + 1)) /
				 (podgov->slow_down_count + 1);

	if (hint_ratio > HINT_RATIO_MAX) {
		idle_min_adjustment = podgov->p_idle_min;
		idle_max_adjustment = podgov->p_idle_max;
	} else if (hint_ratio < HINT_RATIO_MIN) {
		idle_min_adjustment = -((int) podgov->p_idle_min) / 2;
		idle_max_adjustment = -((int) podgov->p_idle_max) / 2;
	} else {
		int diff;
		int factor;

		diff = HINT_RATIO_MID - hint_ratio;
		if (diff < 0)
			factor = -diff * 2;
		else {
			factor = -diff;
			diff *= 2;
		}

		idle_min_adjustment =
			(factor * (int) podgov->p_idle_min) / HINT_RATIO_DIFF;
		idle_max_adjustment =
			(factor * (int) podgov->p_idle_max) / HINT_RATIO_DIFF;
	}

	podgov->idle_min = podgov->p_idle_min + idle_min_adjustment;
	podgov->idle_max = podgov->p_idle_max + idle_max_adjustment;

	trace_podgov_stats(podgov->fast_up_count, podgov->slow_down_count,
		podgov->idle_min, podgov->idle_max);

	podgov->fast_up_count = 0;
	podgov->slow_down_count = 0;
	podgov->last_adjust = time;
}

#undef SCALING_ADJUST_PERIOD
#undef HINT_RATIO_MAX
#undef HINT_RATIO_MIN
#undef HINT_RATIO_MID
#undef HINT_RATIO_DIFF

/*******************************************************************************
 * freq = scaling_state_check(df, time)
 *
 * This handler is called to adjust the frequency of the device. The function
 * returns the desired frequency for the clock. If there is no need to tune the
 * clock immediately, 0 is returned.
 ******************************************************************************/

static unsigned long scaling_state_check(struct devfreq *df, ktime_t time)
{
	struct podgov_info_rec *podgov = df->data;
	unsigned long dt;

	/* adjustment: set scale parameters (idle_min, idle_max) +/- 25%
	 * based on ratio of scale up to scale down hints
	 */
	if (podgov->p_adjust)
		scaling_adjust(podgov, time);
	else {
		podgov->idle_min = podgov->p_idle_min;
		podgov->idle_max = podgov->p_idle_max;
	}

	dt = (unsigned long) ktime_us_delta(time, podgov->last_scale);
	if (dt < podgov->p_estimation_window)
		return 0;

	podgov->last_scale = time;

	/* if too busy, scale up */
	if (podgov->idle_estimate < podgov->idle_min) {
		podgov->is_scaled = 0;
		podgov->fast_up_count++;

		trace_podgov_busy(1000 - podgov->idle_estimate);
		trace_podgov_scaling_state_check(df->previous_freq,
			df->max_freq);

		return df->max_freq;

	}

	trace_podgov_idle(podgov->idle_estimate);

	if (podgov->idle_estimate > podgov->idle_max) {
		if (!podgov->is_scaled)
			podgov->is_scaled = 1;

		podgov->slow_down_count++;
		/* if idle time is high, clock down */
		podgov->scale =
			100 - (podgov->idle_estimate - podgov->idle_min) / 10;
		schedule_work(&podgov->work);
	}

	return 0;
}

/*******************************************************************************
 * update_load_estimate(df)
 *
 * The idle estimate is done by keeping 2 time stamps, initially set to the
 * same time. Once the estimation_window time has been exceeded, one time
 * stamp is moved up to the current time. The idle estimate is calculated
 * based on the idle time percentage from the earlier estimate. The next time
 * an estimation_window time is exceeded, the previous idle time and estimates
 * are moved up - this is intended to prevent abrupt changes to the idle
 * estimate.
 ******************************************************************************/

static void update_load_estimate(struct devfreq *df)
{
	struct podgov_info_rec *podgov = df->data;
	unsigned long window;
	unsigned long t;

	ktime_t now = ktime_get();
	t = ktime_us_delta(now, podgov->last_notification);

	/* if the last event was over GR3D_TIMEFRAME usec ago (1 sec), the
	 * current load tracking data is probably stale
	 */
	if (t > GR3D_TIMEFRAME) {
		podgov->last_notification = now;
		podgov->estimation_window = now;
		podgov->last_estimation_window = now;
		podgov->total_idle = 0;
		podgov->last_total_idle = 0;
		podgov->idle_estimate =
			(podgov->last_event_type == DEVICE_IDLE) ? 1000 : 0;
		return;
	}

	podgov->last_notification = now;

	window = ktime_us_delta(now, podgov->last_estimation_window);
	/* prevent division by 0 if events come in less than 1 usec apart */
	if (window > 0)
		podgov->idle_estimate =
			(1000 * podgov->last_total_idle) / window;

	/* move up to the last estimation window */
	if (ktime_us_delta(now, podgov->estimation_window) >
		podgov->p_estimation_window) {
		podgov->last_estimation_window = podgov->estimation_window;
		podgov->last_total_idle = podgov->total_idle;
		podgov->total_idle = 0;
		podgov->estimation_window = now;
	}
}

/*******************************************************************************
 * freqlist_up(podgov, target, steps)
 *
 * This function determines the frequency that is "steps" frequency steps
 * higher compared to the target frequency.
 ******************************************************************************/

int freqlist_up(struct podgov_info_rec *podgov, long target, int steps)
{
	int i, pos;

	for (i = 0; i < podgov->freq_count; i++)
		if (podgov->freqlist[i] >= target)
			break;

	pos = min(podgov->freq_count - 1, i + steps);
	return podgov->freqlist[pos];
}

/*******************************************************************************
 * freqlist_down(podgov, target, steps)
 *
 * This function determines the frequency that is "steps" frequency steps
 * lower compared to the target frequency.
 ******************************************************************************/

int freqlist_down(struct podgov_info_rec *podgov, long target, int steps)
{
	int i, pos;

	for (i = podgov->freq_count - 1; i >= 0; i--)
		if (podgov->freqlist[i] <= target)
			break;

	pos = max(0, i - steps);
	return podgov->freqlist[pos];
}

/*******************************************************************************
 * podgov_idle_handler(work)
 *
 * This handler is called after the device has been idle long enough. This
 * handler forms a (positive) feedback loop by notifying idle to the device.
 ******************************************************************************/

static void podgov_idle_handler(struct work_struct *work)
{
	struct delayed_work *idle_timer =
		container_of(work, struct delayed_work, work);
	struct podgov_info_rec *podgov =
		container_of(idle_timer, struct podgov_info_rec, idle_timer);
	struct devfreq *df = podgov->power_manager;

	/* Retrieve device driver ops and the device struct */
	struct device *d = df->dev.parent;
	struct platform_device *dev = to_platform_device(d);
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	int notify_idle = 0;

	mutex_lock(&df->lock);

	if (!podgov->enable) {
		mutex_unlock(&df->lock);
		return;
	}

	if (podgov->last_event_type == DEVICE_IDLE &&
		df->previous_freq > df->min_freq)
		notify_idle = 1;

	mutex_unlock(&df->lock);

	if (pdata->idle && notify_idle)
		pdata->idle(dev);
}

/*******************************************************************************
 * nvhost_scale3d_set_throughput_hint(hint)
 *
 * This function can be used to request scaling up or down based on the
 * required throughput
 ******************************************************************************/

void nvhost_scale3d_set_throughput_hint(int hint)
{
	struct podgov_info_rec *podgov = local_podgov;
	struct devfreq *df;

	long idle;
	long curr, target;
	int avg_idle, avg_hint, scale_score;
	unsigned int smooth;

	if (!podgov)
		return;
	df = podgov->power_manager;
	if (!df)
		return;

	mutex_lock(&podgov->power_manager->lock);

	podgov->block--;

	if (!podgov->enable ||
		!podgov->p_use_throughput_hint ||
		podgov->block > 0) {
		mutex_unlock(&podgov->power_manager->lock);
		return;
	}

	trace_podgov_hint(podgov->idle_estimate, hint);
	podgov->last_throughput_hint = ktime_get();

	curr = podgov->power_manager->previous_freq;
	idle = podgov->idle_estimate;
	smooth = podgov->p_smooth;

	/* compute averages usings exponential-moving-average */
	avg_idle = ((smooth*podgov->idle_avg + idle)/(smooth+1));
	podgov->idle_avg = avg_idle;
	avg_hint = ((smooth*podgov->hint_avg + hint)/(smooth+1));
	podgov->hint_avg = avg_hint;

	/* set the target using avg_hint and avg_idle */
	target = curr;
	if (avg_hint < podgov->p_hint_lo_limit) {
		target = freqlist_up(podgov, curr, 1);
	} else {
		scale_score = avg_idle + avg_hint;
		if (scale_score > podgov->p_scaledown_limit)
			target = freqlist_down(podgov, curr, 1);
		else if (scale_score < podgov->p_scaleup_limit
				&& hint < podgov->p_hint_hi_limit)
			target = freqlist_up(podgov, curr, 1);
	}

	/* clamp and apply target */
	scaling_limit(df, &target);
	if (target != curr) {
		podgov->block = podgov->p_smooth;
		trace_podgov_do_scale(df->previous_freq, target);
		podgov->adjustment_frequency = target;
		podgov->adjustment_type = ADJUSTMENT_LOCAL;
		update_devfreq(df);
	}

	trace_podgov_print_target(idle, avg_idle, curr / 1000000, target, hint,
		avg_hint);

	mutex_unlock(&podgov->power_manager->lock);
}
EXPORT_SYMBOL(nvhost_scale3d_set_throughput_hint);

/*******************************************************************************
 * debugfs interface for controlling 3d clock scaling on the fly
 ******************************************************************************/

#ifdef CONFIG_DEBUG_FS

static void nvhost_scale3d_debug_init(struct devfreq *df)
{
	struct podgov_info_rec *podgov = df->data;
	struct platform_device *dev = to_platform_device(df->dev.parent);
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct dentry *f;

	if (!podgov)
		return;

	podgov->debugdir = debugfs_create_dir("scaling", pdata->debugfs);
	if (!podgov->debugdir) {
		pr_err("podgov: can\'t create debugfs directory\n");
		return;
	}

#define CREATE_PODGOV_FILE(fname) \
	do {\
		f = debugfs_create_u32(#fname, S_IRUGO | S_IWUSR, \
			podgov->debugdir, &podgov->p_##fname); \
		if (NULL == f) { \
			pr_err("podgov: can\'t create file " #fname "\n"); \
			return; \
		} \
	} while (0)

	CREATE_PODGOV_FILE(estimation_window);
	CREATE_PODGOV_FILE(idle_min);
	CREATE_PODGOV_FILE(idle_max);
	CREATE_PODGOV_FILE(adjust);
	CREATE_PODGOV_FILE(use_throughput_hint);
	CREATE_PODGOV_FILE(hint_hi_limit);
	CREATE_PODGOV_FILE(hint_lo_limit);
	CREATE_PODGOV_FILE(scaleup_limit);
	CREATE_PODGOV_FILE(scaledown_limit);
	CREATE_PODGOV_FILE(smooth);
#undef CREATE_PODGOV_FILE
}

static void nvhost_scale3d_debug_deinit(struct devfreq *df)
{
	struct podgov_info_rec *podgov = df->data;

	debugfs_remove_recursive(podgov->debugdir);
}

#else
static void nvhost_scale3d_debug_init(struct devfreq *df)
{
	(void)df;
}

static void nvhost_scale3d_debug_deinit(struct devfreq *df)
{
	(void)df;
}
#endif

/*******************************************************************************
 * sysfs interface for enabling/disabling 3d scaling
 ******************************************************************************/

static ssize_t enable_3d_scaling_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t res;

	res = snprintf(buf, PAGE_SIZE, "%d\n", podgov_is_enabled(dev));

	return res;
}

static ssize_t enable_3d_scaling_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	podgov_enable(dev, val);

	return count;
}

static DEVICE_ATTR(enable_3d_scaling, S_IRUGO | S_IWUSR,
	enable_3d_scaling_show, enable_3d_scaling_store);

/*******************************************************************************
 * sysfs interface for user space control
 * user = [0,1] disables / enabled user space control
 * freq_request is the sysfs node user space writes frequency requests to
 ******************************************************************************/

static ssize_t user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t res;

	res = snprintf(buf, PAGE_SIZE, "%d\n", podgov_user_ctl(dev));

	return res;
}

static ssize_t user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	podgov_set_user_ctl(dev, val);

	return count;
}

static DEVICE_ATTR(user, S_IRUGO | S_IWUSR,
	user_show, user_store);

static ssize_t freq_request_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t res;

	res = snprintf(buf, PAGE_SIZE, "%d\n", podgov_get_freq_request(dev));

	return res;
}

static ssize_t freq_request_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	podgov_set_freq_request(dev, val);

	return count;
}

static DEVICE_ATTR(freq_request, S_IRUGO | S_IWUSR,
	freq_request_show, freq_request_store);

/*******************************************************************************
 * nvhost_pod_estimate_freq(df, freq)
 *
 * This function is called for re-estimating the frequency. The function is
 * called in three conditions:
 *
 *  (1) Internal request to change the frequency. In this case a new clock
 *      target is immediately set for the device.
 *  (2) Call from the client (something has happened and re-estimation
 *      is required).
 *  (3) Some other reason (i.e. periodic call)
 *
 ******************************************************************************/

static int nvhost_pod_estimate_freq(struct devfreq *df,
				    unsigned long *freq)
{
	struct podgov_info_rec *podgov = df->data;
	struct devfreq_dev_status dev_stat;
	struct nvhost_devfreq_ext_stat *ext_stat;
	long delay;
	int current_event;
	int stat;
	ktime_t now;

	stat = df->profile->get_dev_status(df->dev.parent, &dev_stat);
	if (stat < 0)
		return stat;

	/* Ensure maximal clock when scaling is disabled */
	if (!podgov->enable) {
		*freq = df->max_freq;
		return 0;
	}

	if (podgov->p_user) {
		*freq = podgov->p_freq_request;
		return 0;
	}

	current_event = DEVICE_IDLE;
	stat = 0;
	now = ktime_get();

	/* Local adjustments (i.e. requests from kernel threads) are
	 * handled here */

	if (podgov->adjustment_type == ADJUSTMENT_LOCAL) {

		podgov->adjustment_type = ADJUSTMENT_DEVICE_REQ;

		/* Do not do unnecessary scaling */
		scaling_limit(df, &podgov->adjustment_frequency);
		if (df->previous_freq == podgov->adjustment_frequency)
			return GET_TARGET_FREQ_DONTSCALE;

		trace_podgov_estimate_freq(df->previous_freq,
			podgov->adjustment_frequency);

		*freq = podgov->adjustment_frequency;
		return 0;
	}

	/* Retrieve extended data */
	ext_stat = dev_stat.private_data;
	if (!ext_stat)
		return -EINVAL;

	current_event = ext_stat->busy;
	*freq = dev_stat.current_frequency;
	df->min_freq = ext_stat->min_freq;
	df->max_freq = ext_stat->max_freq;

	/* Sustain local variables */
	podgov->last_event_type = current_event;
	podgov->total_idle += (dev_stat.total_time - dev_stat.busy_time);
	podgov->last_total_idle += (dev_stat.total_time - dev_stat.busy_time);

	/* update the load estimate based on idle time */
	update_load_estimate(df);

	/* if throughput hint enabled, and last hint is recent enough, return */
	if (podgov->p_use_throughput_hint &&
		ktime_us_delta(now, podgov->last_throughput_hint) < 1000000)
		return GET_TARGET_FREQ_DONTSCALE;

	switch (current_event) {

	case DEVICE_IDLE:
		/* delay idle_max % of 2 * fast_response time (given in
		 * microseconds) */
		*freq = scaling_state_check(df, now);
		delay = (podgov->idle_max * podgov->p_estimation_window)
			/ 500000;
		schedule_delayed_work(&podgov->idle_timer,
			msecs_to_jiffies(delay));
		break;
	case DEVICE_BUSY:
		cancel_delayed_work(&podgov->idle_timer);
		*freq = scaling_state_check(df, now);
		break;
	case DEVICE_UNKNOWN:
		*freq = scaling_state_check(df, now);
		break;

	}

	if (!(*freq) || (*freq == df->previous_freq))
		return GET_TARGET_FREQ_DONTSCALE;

	trace_podgov_estimate_freq(df->previous_freq, *freq);


	return 0;
}

/*******************************************************************************
 * nvhost_pod_init(struct devfreq *df)
 *
 * Governor initialisation.
 ******************************************************************************/

#define MAX_FREQ_COUNT 0x40 /* 64 frequencies should be enough for anyone */

static int nvhost_pod_init(struct devfreq *df)
{
	struct podgov_info_rec *podgov;
	struct platform_device *d = to_platform_device(df->dev.parent);
	struct nvhost_device_data *pdata = platform_get_drvdata(d);
	ktime_t now = ktime_get();
	int error = 0;

	struct nvhost_devfreq_ext_stat *ext_stat;
	struct devfreq_dev_status dev_stat;
	int stat = 0;

	long rate;
	int freqs[MAX_FREQ_COUNT];

	podgov = kzalloc(sizeof(struct podgov_info_rec), GFP_KERNEL);
	if (!podgov)
		goto err_alloc_podgov;
	df->data = (void *)podgov;

	/* This should be removed after the governor include also the hint
	 * interface */
	local_podgov = podgov;

	/* Initialise workers */
	INIT_WORK(&podgov->work, podgov_clocks_handler);
	INIT_DELAYED_WORK(&podgov->idle_timer, podgov_idle_handler);

	/* Set scaling parameter defaults */
	podgov->enable = 1;
	podgov->p_adjust = 0;
	podgov->block = 0;
	podgov->p_use_throughput_hint = 1;
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11) {
		podgov->idle_min = podgov->p_idle_min = 400;
		podgov->idle_max = podgov->p_idle_max = 500;
		podgov->p_hint_lo_limit = 500;
		podgov->p_hint_hi_limit = 997;
		podgov->p_scaleup_limit = 1100;
		podgov->p_scaledown_limit = 1300;
		podgov->p_smooth = 3;
	} else {
		podgov->idle_min = podgov->p_idle_min = 100;
		podgov->idle_max = podgov->p_idle_max = 150;
		podgov->p_hint_lo_limit = 800;
		podgov->p_hint_hi_limit = 1015;
		podgov->p_scaleup_limit = 1275;
		podgov->p_scaledown_limit = 1475;
		podgov->p_smooth = 7;
	}
	podgov->p_estimation_window = 10000;
	podgov->adjustment_type = ADJUSTMENT_DEVICE_REQ;
	podgov->p_user = 0;

	/* Reset clock counters */
	podgov->last_throughput_hint = now;
	podgov->last_scale = now;
	podgov->last_adjust = now;
	podgov->last_estimation_window = now;
	podgov->estimation_window = now;
	podgov->last_notification = now;

	podgov->power_manager = df;

	/* Get the current status of the device */
	stat = df->profile->get_dev_status(df->dev.parent, &dev_stat);
	if (!dev_stat.private_data) {
		pr_err("podgov: device does not support ext_stat.\n");
		goto err_get_current_status;
	}
	ext_stat = dev_stat.private_data;
	df->previous_freq = dev_stat.current_frequency;
	df->min_freq = ext_stat->min_freq;
	df->max_freq = ext_stat->max_freq;

	podgov->p_freq_request = ext_stat->max_freq;

	/* Create sysfs entries for controlling this governor */
	error = device_create_file(&d->dev,
			&dev_attr_enable_3d_scaling);
	if (error)
		goto err_create_sysfs_entry;

	error = device_create_file(&d->dev,
			&dev_attr_user);
	if (error)
		goto err_create_sysfs_entry;

	error = device_create_file(&d->dev,
			&dev_attr_freq_request);
	if (error)
		goto err_create_sysfs_entry;

	rate = 0;
	podgov->freq_count = 0;
	while (rate <= df->max_freq) {
		long rounded_rate;
		if (unlikely(podgov->freq_count == MAX_FREQ_COUNT)) {
			pr_err("%s: too many frequencies\n", __func__);
			break;
		}
		rounded_rate =
			clk_round_rate(clk_get_parent(pdata->clk[0]), rate);
		freqs[podgov->freq_count++] = rounded_rate;
		rate = rounded_rate + 2000;
	}

	podgov->freqlist =
		kmalloc(podgov->freq_count * sizeof(int), GFP_KERNEL);
	if (podgov->freqlist == NULL)
		goto err_allocate_freq_list;

	memcpy(podgov->freqlist, freqs,
		podgov->freq_count * sizeof(int));

	podgov->idle_avg = 0;
	podgov->hint_avg = 0;

	nvhost_scale3d_debug_init(df);

	return 0;

err_allocate_freq_list:
	device_remove_file(&d->dev, &dev_attr_enable_3d_scaling);
	device_remove_file(&d->dev, &dev_attr_user);
	device_remove_file(&d->dev, &dev_attr_freq_request);
err_create_sysfs_entry:
	dev_err(&d->dev, "failed to create sysfs attributes");
err_get_current_status:
	kfree(podgov);
err_alloc_podgov:
	return -ENOMEM;
}

/*******************************************************************************
 * nvhost_pod_exit(struct devfreq *df)
 *
 * Clean up governor data structures
 ******************************************************************************/

static void nvhost_pod_exit(struct devfreq *df)
{
	struct podgov_info_rec *podgov = df->data;
	struct platform_device *d = to_platform_device(df->dev.parent);

	cancel_work_sync(&podgov->work);
	cancel_delayed_work(&podgov->idle_timer);

	kfree(podgov->freqlist);

	device_remove_file(&d->dev, &dev_attr_enable_3d_scaling);
	device_remove_file(&d->dev, &dev_attr_user);
	device_remove_file(&d->dev, &dev_attr_freq_request);

	nvhost_scale3d_debug_deinit(df);

	kfree(podgov);
	local_podgov = NULL;
}

const struct devfreq_governor nvhost_podgov = {
	.name = "pod",
	.init = nvhost_pod_init,
	.exit = nvhost_pod_exit,
	.get_target_freq = nvhost_pod_estimate_freq,
	.no_central_polling = true,
};

