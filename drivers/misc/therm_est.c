/*
 * drivers/misc/therm_est.c
 *
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/therm_est.h>
#include <linux/thermal.h>
#include <linux/module.h>
#include <linux/hwmon-sysfs.h>
#include <linux/suspend.h>

struct therm_estimator {
	struct thermal_zone_device *thz;
	int num_trips;
	struct thermal_trip_info *trips;
	struct thermal_zone_params *tzp;

	int num_timer_trips;
	struct therm_est_timer_trip_info *timer_trips;
	struct delayed_work timer_trip_work;
	struct mutex timer_trip_lock;

	struct thermal_cooling_device *cdev; /* activation device */
	struct workqueue_struct *workqueue;
	struct delayed_work therm_est_work;
	long cur_temp;
	long low_limit;
	long high_limit;
	int ntemp;
	long toffset;
	long polling_period;
	int polling_enabled;
	int tc1;
	int tc2;
	int ndevs;
	struct therm_est_subdevice *devs;

	int use_activator;
#ifdef CONFIG_PM
	struct notifier_block pm_nb;
#endif
};

#define TIMER_TRIP_INACTIVE		-2

#define TIMER_TRIP_STATE_NONE		0
#define TIMER_TRIP_STATE_START		BIT(0)
#define TIMER_TRIP_STATE_STOP		BIT(1)
#define TIMER_TRIP_STATE_UP		BIT(2)
#define TIMER_TRIP_STATE_DOWN		BIT(3)

static int __get_trip_temp(struct thermal_zone_device *thz, int trip,
			   long *temp);

static struct therm_est_timer_trip_info *
__find_timer_trip(struct therm_estimator *est, int trip)
{
	int i;

	/* Find matched timer trip info with trip. */
	for (i = 0; i < est->num_timer_trips; i++) {
		if (est->timer_trips[i].trip == trip)
			return &est->timer_trips[i];
	}
	return NULL;
}

static int __get_timer_trip_delay(struct therm_est_timer_trip_info *timer_info,
				 s64 now, s64 *delay)
{
	int cur = timer_info->cur;
	int next = (cur + 1 < timer_info->num_timers) ? cur + 1 : cur;

	if (cur == next) /* No more timer on this trip. */
		return -ENOENT;

	*delay = timer_info->timers[next].time_after -
		 (now - timer_info->last_tripped);
	return 0;
}

static int therm_est_subdev_match(struct thermal_zone_device *thz, void *data)
{
	return strcmp((char *)data, thz->type) == 0;
}

static int therm_est_subdev_get_temp(struct thermal_zone_device *thz,
					long *temp)
{
	if (!thz || thz->ops->get_temp(thz, temp))
		*temp = 25000;

	return 0;
}

static void therm_est_update_limits(struct therm_estimator *est)
{
	const int MAX_HIGH_TEMP = 128000;
	long low_temp = 0, high_temp = MAX_HIGH_TEMP;
	long trip_temp, passive_low_temp = MAX_HIGH_TEMP;
	enum thermal_trip_type trip_type;
	struct thermal_trip_info *trip_state;
	int i;

	for (i = 0; i < est->num_trips; i++) {
		trip_state = &est->trips[i];
		__get_trip_temp(est->thz, i, &trip_temp);
		est->thz->ops->get_trip_type(est->thz, i, &trip_type);

		if (!trip_state->tripped) { /* not tripped? update high */
			if (trip_temp < high_temp)
				high_temp = trip_temp;
		} else { /* tripped? update low */
			if (trip_type != THERMAL_TRIP_PASSIVE) {
				/* get highest ACTIVE */
				if (trip_temp > low_temp)
					low_temp = trip_temp;
			} else {
				/* get lowest PASSIVE */
				if (trip_temp < passive_low_temp)
					passive_low_temp = trip_temp;
			}
		}
	}

	if (passive_low_temp != MAX_HIGH_TEMP)
		low_temp = max(low_temp, passive_low_temp);

	est->low_limit = low_temp;
	est->high_limit = high_temp;
}

static void therm_est_update_timer_trips(struct therm_estimator *est)
{
	struct thermal_trip_info *trip_state;
	struct therm_est_timer_trip_info *timer_info;
	s64 now, delay, min_delay;
	int i;

	mutex_lock(&est->timer_trip_lock);
	min_delay = LLONG_MAX;
	now = ktime_to_ms(ktime_get());

	for (i = 0; i < est->num_timer_trips; i++) {
		timer_info = &est->timer_trips[i];
		trip_state = &est->trips[timer_info->trip];

		pr_debug("%s: i %d, trip %d, tripped %d, cur %d\n",
			__func__, i, timer_info->trip, trip_state->tripped,
			timer_info->cur);
		if ((timer_info->cur == TIMER_TRIP_INACTIVE) ||
			(__get_timer_trip_delay(timer_info, now, &delay) < 0))
			continue;

		if (delay > 0)
			min_delay = min(min_delay, delay);
		pr_debug("%s: delay %lld, min_delay %lld\n",
			__func__, delay, min_delay);
	}
	mutex_unlock(&est->timer_trip_lock);

	cancel_delayed_work(&est->timer_trip_work);
	if (min_delay != LLONG_MAX)
		queue_delayed_work(est->workqueue, &est->timer_trip_work,
				   msecs_to_jiffies(min_delay));
}

static void therm_est_timer_trip_work_func(struct work_struct *work)
{
	struct therm_estimator *est = container_of(work, struct therm_estimator,
						   timer_trip_work.work);
	struct thermal_trip_info *trip_state;
	struct therm_est_timer_trip_info *timer_info;
	s64 now, delay;
	int timer_trip_state, i;

	mutex_lock(&est->timer_trip_lock);
	timer_trip_state = TIMER_TRIP_STATE_NONE;
	now = ktime_to_ms(ktime_get());

	for (i = 0; i < est->num_timer_trips; i++) {
		timer_info = &est->timer_trips[i];
		trip_state = &est->trips[timer_info->trip];

		pr_debug("%s: i %d, trip %d, tripped %d, cur %d\n",
			__func__, i, timer_info->trip, trip_state->tripped,
			timer_info->cur);
		if ((timer_info->cur == TIMER_TRIP_INACTIVE) ||
			(__get_timer_trip_delay(timer_info, now, &delay) < 0))
			continue;

		if (delay <= 0) { /* Timer on this trip has expired. */
			if (timer_info->cur + 1 < timer_info->num_timers) {
				timer_info->last_tripped = now;
				timer_info->cur++;
				timer_trip_state |= TIMER_TRIP_STATE_UP;
			}
		}

		/* If delay > 0, timer on this trip has not yet expired.
		 * So need to restart timer with remaining delay. */
		timer_trip_state |= TIMER_TRIP_STATE_START;
		pr_debug("%s: new_cur %d, delay %lld, timer_trip_state 0x%x\n",
			__func__, timer_info->cur, delay, timer_trip_state);
	}
	mutex_unlock(&est->timer_trip_lock);

	if (timer_trip_state & (TIMER_TRIP_STATE_START | TIMER_TRIP_STATE_UP)) {
		therm_est_update_timer_trips(est);
		therm_est_update_limits(est);
	}
}

static void therm_est_work_func(struct work_struct *work)
{
	int i, j, index, sum = 0;
	long temp;
	struct delayed_work *dwork = container_of(work,
					struct delayed_work, work);
	struct therm_estimator *est = container_of(dwork,
					struct therm_estimator,
					therm_est_work);

	for (i = 0; i < est->ndevs; i++) {
		if (therm_est_subdev_get_temp(est->devs[i].sub_thz, &temp))
			continue;
		est->devs[i].hist[(est->ntemp % HIST_LEN)] = temp;
	}

	for (i = 0; i < est->ndevs; i++) {
		for (j = 0; j < HIST_LEN; j++) {
			index = (est->ntemp - j + HIST_LEN) % HIST_LEN;
			sum += est->devs[i].hist[index] *
				est->devs[i].coeffs[j];
		}
	}

	est->cur_temp = sum / 100 + est->toffset;
	est->ntemp++;

	if (est->thz && ((est->cur_temp < est->low_limit) ||
			(est->cur_temp >= est->high_limit))) {
		thermal_zone_device_update(est->thz);
		therm_est_update_timer_trips(est);
		therm_est_update_limits(est);
	}

	if (est->polling_enabled > 0 || !est->use_activator) {
		queue_delayed_work(est->workqueue, &est->therm_est_work,
			msecs_to_jiffies(est->polling_period));
	}
}

static int therm_est_bind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	struct therm_estimator *est = thz->devdata;
	struct thermal_trip_info *trip_state;
	int i;

	for (i = 0; i < est->num_trips; i++) {
		trip_state = &est->trips[i];
		if (trip_state->cdev_type &&
		    !strncmp(trip_state->cdev_type, cdev->type,
			     THERMAL_NAME_LENGTH))
			thermal_zone_bind_cooling_device(thz, i, cdev,
							 trip_state->upper,
							 trip_state->lower);
	}

	return 0;
}

static int therm_est_unbind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	struct therm_estimator *est = thz->devdata;
	struct thermal_trip_info *trip_state;
	int i;

	for (i = 0; i < est->num_trips; i++) {
		trip_state = &est->trips[i];
		if (trip_state->cdev_type &&
		    !strncmp(trip_state->cdev_type, cdev->type,
			     THERMAL_NAME_LENGTH))
			thermal_zone_unbind_cooling_device(thz, i, cdev);
	}

	return 0;
}

static int therm_est_get_trip_type(struct thermal_zone_device *thz,
				   int trip, enum thermal_trip_type *type)
{
	struct therm_estimator *est = thz->devdata;

	*type = est->trips[trip].trip_type;
	return 0;
}

static int __get_trip_temp(struct thermal_zone_device *thz, int trip,
			   long *temp)
{
	struct therm_estimator *est = thz->devdata;
	struct thermal_trip_info *trip_state = &est->trips[trip];
	struct therm_est_timer_trip_info *timer_info;
	long zone_temp, trip_temp, hysteresis;
	int cur = TIMER_TRIP_INACTIVE;
	int ret = TIMER_TRIP_STATE_NONE;

	zone_temp = thz->temperature;
	trip_temp = trip_state->trip_temp;
	hysteresis = trip_state->hysteresis;

	timer_info = __find_timer_trip(est, trip);
	if (timer_info) {
		cur = timer_info->cur;
		/* If timer trip is available, use trip_temp and hysteresis in
		 * the timer trip to trip_temp for this trip. */
		if (timer_info->cur >= 0) {
			trip_temp = timer_info->timers[cur].trip_temp;
			hysteresis = timer_info->timers[cur].hysteresis;
		}
	}

	if (zone_temp >= trip_temp) {
		trip_temp -= hysteresis;
		if (timer_info && !trip_state->tripped)
			ret = TIMER_TRIP_STATE_START;
		trip_state->tripped = true;
	} else if (trip_state->tripped) {
		trip_temp -= hysteresis;
		if (zone_temp < trip_temp) {
			if (!timer_info) {
				trip_state->tripped = false;
			} else {
				if (cur == TIMER_TRIP_INACTIVE)
					trip_state->tripped = false;
				else
					ret = TIMER_TRIP_STATE_DOWN;
			}
		}
	}

	*temp = trip_temp;
	return ret;
}

static int therm_est_get_trip_temp(struct thermal_zone_device *thz,
				   int trip, unsigned long *temp)
{
	struct therm_estimator *est = thz->devdata;
	struct therm_est_timer_trip_info *timer_info;
	int ret;

	ret = __get_trip_temp(thz, trip, temp);
	if (ret & (TIMER_TRIP_STATE_START | TIMER_TRIP_STATE_DOWN)) {
		timer_info = __find_timer_trip(est, trip);

		mutex_lock(&est->timer_trip_lock);
		timer_info->last_tripped = ktime_to_ms(ktime_get());

		if (ret & TIMER_TRIP_STATE_START) {
			timer_info->cur = TIMER_TRIP_INACTIVE + 1;
		} else if (ret & TIMER_TRIP_STATE_DOWN) {
			if (--timer_info->cur < TIMER_TRIP_INACTIVE)
				timer_info->cur = TIMER_TRIP_INACTIVE;
		}
		mutex_unlock(&est->timer_trip_lock);

		/* Update limits, because trip temp was changed by timer trip
		 * changing. */
		therm_est_update_limits(est);
	}

	return 0;
}

static int therm_est_set_trip_temp(struct thermal_zone_device *thz,
				   int trip, unsigned long temp)
{
	struct therm_estimator *est = thz->devdata;

	est->trips[trip].trip_temp = temp;

	/* Update limits, because trip temp was changed. */
	therm_est_update_limits(est);
	return 0;
}

static int therm_est_get_temp(struct thermal_zone_device *thz,
				unsigned long *temp)
{
	struct therm_estimator *est = thz->devdata;

	*temp = est->cur_temp;
	return 0;
}

static int therm_est_get_trend(struct thermal_zone_device *thz,
			       int trip, enum thermal_trend *trend)
{
	struct therm_estimator *est = thz->devdata;
	struct thermal_trip_info *trip_state = &est->trips[trip];
	long trip_temp;
	int new_trend;
	int cur_temp;

	__get_trip_temp(thz, trip, &trip_temp);

	cur_temp = thz->temperature;
	new_trend = (est->tc1 * (cur_temp - thz->last_temperature)) +
		    (est->tc2 * (cur_temp - trip_temp));

	switch (trip_state->trip_type) {
	case THERMAL_TRIP_ACTIVE:
		/* aggressive active cooling */
		*trend = THERMAL_TREND_RAISING;
		break;
	case THERMAL_TRIP_PASSIVE:
		if (new_trend > 0)
			*trend = THERMAL_TREND_RAISING;
		else if (new_trend < 0)
			*trend = THERMAL_TREND_DROPPING;
		else
			*trend = THERMAL_TREND_STABLE;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void therm_est_init_timer_trips(struct therm_estimator *est)
{
	int i;

	for (i = 0; i < est->num_timer_trips; i++)
		est->timer_trips[i].cur = TIMER_TRIP_INACTIVE;
}

static int therm_est_init_history(struct therm_estimator *est)
{
	int i, j;
	struct therm_est_subdevice *dev;
	long temp;

	for (i = 0; i < est->ndevs; i++) {
		dev = &est->devs[i];

		if (therm_est_subdev_get_temp(dev->sub_thz, &temp))
			return -EINVAL;

		for (j = 0; j < HIST_LEN; j++)
			dev->hist[j] = temp;
	}

	return 0;
}

static int therm_est_polling(struct therm_estimator *est,
				int polling)
{
	est->polling_enabled = polling > 0;

	if (est->polling_enabled > 0) {
		est->low_limit = 0;
		est->high_limit = 0;
		therm_est_init_history(est);
		therm_est_init_timer_trips(est);
		queue_delayed_work(est->workqueue,
			&est->therm_est_work,
			msecs_to_jiffies(est->polling_period));
	} else {
		est->cur_temp = 25000;
		cancel_delayed_work_sync(&est->therm_est_work);
	}
	return 0;
}

static struct thermal_zone_device_ops therm_est_ops = {
	.bind = therm_est_bind,
	.unbind = therm_est_unbind,
	.get_trip_type = therm_est_get_trip_type,
	.get_trip_temp = therm_est_get_trip_temp,
	.set_trip_temp = therm_est_set_trip_temp,
	.get_temp = therm_est_get_temp,
	.get_trend = therm_est_get_trend,
};

static ssize_t show_coeff(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	ssize_t len, total_len = 0;
	int i, j;
	for (i = 0; i < est->ndevs; i++) {
		len = snprintf(buf + total_len,
				PAGE_SIZE - total_len, "[%d]", i);
		total_len += len;
		for (j = 0; j < HIST_LEN; j++) {
			len = snprintf(buf + total_len,
					PAGE_SIZE - total_len, " %ld",
					est->devs[i].coeffs[j]);
			total_len += len;
		}
		len = snprintf(buf + total_len, PAGE_SIZE - total_len, "\n");
		total_len += len;
	}
	return strlen(buf);
}

static ssize_t set_coeff(struct device *dev,
				struct device_attribute *da,
				const char *buf, size_t count)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	int devid, scount;
	long coeff[20];

	if (HIST_LEN > 20)
		return -EINVAL;

	scount = sscanf(buf, "[%d] %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld " \
			"%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
			&devid,
			&coeff[0],
			&coeff[1],
			&coeff[2],
			&coeff[3],
			&coeff[4],
			&coeff[5],
			&coeff[6],
			&coeff[7],
			&coeff[8],
			&coeff[9],
			&coeff[10],
			&coeff[11],
			&coeff[12],
			&coeff[13],
			&coeff[14],
			&coeff[15],
			&coeff[16],
			&coeff[17],
			&coeff[18],
			&coeff[19]);

	if (scount != HIST_LEN + 1)
		return -1;

	if (devid < 0 || devid >= est->ndevs)
		return -EINVAL;

	/* This has obvious locking issues but don't worry about it */
	memcpy(est->devs[devid].coeffs, coeff, sizeof(coeff[0]) * HIST_LEN);

	return count;
}

static ssize_t show_offset(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	snprintf(buf, PAGE_SIZE, "%ld\n", est->toffset);
	return strlen(buf);
}

static ssize_t set_offset(struct device *dev,
				struct device_attribute *da,
				const char *buf, size_t count)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	int offset;

	if (kstrtoint(buf, 0, &offset))
		return -EINVAL;

	est->toffset = offset;

	return count;
}

static ssize_t show_temps(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	ssize_t total_len = 0;
	int i, j;
	int index;

	/* This has obvious locking issues but don't worry about it */
	for (i = 0; i < est->ndevs; i++) {
		total_len += snprintf(buf + total_len,
					PAGE_SIZE - total_len, "[%d]", i);
		for (j = 0; j < HIST_LEN; j++) {
			index = (est->ntemp - j + HIST_LEN) % HIST_LEN;
			total_len += snprintf(buf + total_len,
						PAGE_SIZE - total_len, " %ld",
						est->devs[i].hist[index]);
		}
		total_len += snprintf(buf + total_len,
					PAGE_SIZE - total_len, "\n");
	}
	return strlen(buf);
}

static ssize_t show_tc1(struct device *dev,
			struct device_attribute *da,
			char *buf)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	snprintf(buf, PAGE_SIZE, "%d\n", est->tc1);
	return strlen(buf);
}

static ssize_t set_tc1(struct device *dev,
			struct device_attribute *da,
			const char *buf, size_t count)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	int tc1;

	if (kstrtoint(buf, 0, &tc1))
		return -EINVAL;

	est->tc1 = tc1;

	return count;
}

static ssize_t show_tc2(struct device *dev,
			struct device_attribute *da,
			char *buf)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	snprintf(buf, PAGE_SIZE, "%d\n", est->tc2);
	return strlen(buf);
}

static ssize_t set_tc2(struct device *dev,
			struct device_attribute *da,
			const char *buf, size_t count)
{
	struct therm_estimator *est = dev_get_drvdata(dev);
	int tc2;

	if (kstrtoint(buf, 0, &tc2))
		return -EINVAL;

	est->tc2 = tc2;

	return count;
}

static struct sensor_device_attribute therm_est_nodes[] = {
	SENSOR_ATTR(coeff, S_IRUGO | S_IWUSR, show_coeff, set_coeff, 0),
	SENSOR_ATTR(offset, S_IRUGO | S_IWUSR, show_offset, set_offset, 0),
	SENSOR_ATTR(tc1, S_IRUGO | S_IWUSR, show_tc1, set_tc1, 0),
	SENSOR_ATTR(tc2, S_IRUGO | S_IWUSR, show_tc2, set_tc2, 0),
	SENSOR_ATTR(temps, S_IRUGO, show_temps, 0, 0),
};

#ifdef CONFIG_PM
static int therm_est_pm_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct therm_estimator *est = container_of(
					nb,
					struct therm_estimator,
					pm_nb);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		cancel_delayed_work_sync(&est->therm_est_work);
		cancel_delayed_work_sync(&est->timer_trip_work);
		break;
	case PM_POST_SUSPEND:
		est->low_limit = 0;
		est->high_limit = 0;
		therm_est_init_history(est);
		therm_est_init_timer_trips(est);
		queue_delayed_work(est->workqueue,
				&est->therm_est_work,
				msecs_to_jiffies(est->polling_period));
		break;
	}

	return NOTIFY_OK;
}
#endif

static int
thermal_est_activation_get_max_state(struct thermal_cooling_device *cdev,
					unsigned long *max_state)
{
	*max_state = 1;
	return 0;
}

static int
thermal_est_activation_get_cur_state(struct thermal_cooling_device *cdev,
					unsigned long *cur_state)
{
	struct therm_estimator *est = cdev->devdata;
	*cur_state = est->polling_enabled;
	return 0;
}

static int
thermal_est_activation_set_cur_state(struct thermal_cooling_device *cdev,
					unsigned long cur_state)
{
	struct therm_estimator *est = cdev->devdata;
	if (est->use_activator)
		therm_est_polling(est, cur_state > 0);

	return 0;
}

static struct thermal_cooling_device_ops thermal_est_activation_device_ops = {
	.get_max_state = thermal_est_activation_get_max_state,
	.get_cur_state = thermal_est_activation_get_cur_state,
	.set_cur_state = thermal_est_activation_set_cur_state,
};

struct thermal_cooling_device *thermal_est_activation_device_register(
						struct therm_estimator *est,
						char *type)
{
	struct thermal_cooling_device *cdev;

	cdev = thermal_cooling_device_register(
		type,
		est,
		&thermal_est_activation_device_ops);

	if (IS_ERR(cdev))
		return NULL;

	pr_debug("Therm_est: Cooling-device REGISTERED\n");

	return cdev;
}

static int therm_est_probe(struct platform_device *pdev)
{
	int i;
	struct therm_estimator *est;
	struct therm_est_data *data;
	struct thermal_zone_device *thz;

	est = kzalloc(sizeof(struct therm_estimator), GFP_KERNEL);
	if (IS_ERR_OR_NULL(est))
		return -ENOMEM;

	platform_set_drvdata(pdev, est);

	data = pdev->dev.platform_data;

	for (i = 0; i < data->ndevs; i++) {
		thz = thermal_zone_device_find(data->devs[i].dev_data,
							therm_est_subdev_match);
		if (!thz)
			goto err;
		data->devs[i].sub_thz = thz;
	}

	est->devs = data->devs;
	est->ndevs = data->ndevs;
	est->toffset = data->toffset;
	est->polling_period = data->polling_period;
	est->polling_enabled = 0; /* By default polling is switched off */
	est->tc1 = data->tc1;
	est->tc2 = data->tc2;
	est->use_activator = data->use_activator;

	/* initialize history */
	therm_est_init_history(est);

	/* initialize timer trips */
	est->num_timer_trips = data->num_timer_trips;
	est->timer_trips = data->timer_trips;
	therm_est_init_timer_trips(est);
	mutex_init(&est->timer_trip_lock);
	INIT_DELAYED_WORK(&est->timer_trip_work,
			  therm_est_timer_trip_work_func);

	est->workqueue = alloc_workqueue(dev_name(&pdev->dev),
				    WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!est->workqueue)
		goto err;

	INIT_DELAYED_WORK(&est->therm_est_work, therm_est_work_func);

	est->cdev = thermal_est_activation_device_register(est,
							"therm_est_activ");

	est->num_trips = data->num_trips;
	est->trips = data->trips;
	est->tzp = data->tzp;

	est->thz = thermal_zone_device_register(dev_name(&pdev->dev),
						est->num_trips,
						(1ULL << est->num_trips) - 1,
						est,
						&therm_est_ops,
						est->tzp,
						data->passive_delay,
						0);
	if (IS_ERR_OR_NULL(est->thz))
		goto err;

	for (i = 0; i < ARRAY_SIZE(therm_est_nodes); i++)
		device_create_file(&pdev->dev, &therm_est_nodes[i].dev_attr);

#ifdef CONFIG_PM
	est->pm_nb.notifier_call = therm_est_pm_notify,
	register_pm_notifier(&est->pm_nb);
#endif

	if (!est->use_activator)
		queue_delayed_work(est->workqueue, &est->therm_est_work,
			msecs_to_jiffies(est->polling_period));

	return 0;

err:
	if (est->workqueue)
		destroy_workqueue(est->workqueue);
	kfree(est);
	return -EINVAL;
}

static int therm_est_remove(struct platform_device *pdev)
{
	struct therm_estimator *est = platform_get_drvdata(pdev);
	int i;

	cancel_delayed_work_sync(&est->therm_est_work);
	cancel_delayed_work_sync(&est->timer_trip_work);

#ifdef CONFIG_PM
	unregister_pm_notifier(&est->pm_nb);
#endif
	for (i = 0; i < ARRAY_SIZE(therm_est_nodes); i++)
		device_remove_file(&pdev->dev, &therm_est_nodes[i].dev_attr);
	thermal_zone_device_unregister(est->thz);
	thermal_cooling_device_unregister(est->cdev);
	kfree(est->thz);
	destroy_workqueue(est->workqueue);
	kfree(est);
	return 0;
}

static struct platform_driver therm_est_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = "therm_est",
	},
	.probe  = therm_est_probe,
	.remove = therm_est_remove,
};

static int __init therm_est_driver_init(void)
{
	return platform_driver_register(&therm_est_driver);
}
module_init(therm_est_driver_init);
