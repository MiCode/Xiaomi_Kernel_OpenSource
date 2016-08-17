/*
 * drivers/misc/therm_est.c
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/of.h>
#include <linux/of_device.h>

struct therm_estimator {
	struct thermal_zone_device *thz;
	int num_trips;
	struct thermal_trip_info *trips;
	struct thermal_zone_params *tzp;

	int num_timer_trips;
	struct therm_est_timer_trip_info *timer_trips;
	struct delayed_work timer_trip_work;
	struct mutex timer_trip_lock;

	struct workqueue_struct *workqueue;
	struct delayed_work therm_est_work;
	long cur_temp;
	long low_limit;
	long high_limit;
	int ntemp;
	long toffset;
	long polling_period;
	int tc1;
	int tc2;
	int ndevs;
	struct therm_est_subdevice *devs;

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

static int therm_est_subdev_get_temp(void *data, long *temp)
{
	struct thermal_zone_device *thz;

	thz = thermal_zone_device_find(data, therm_est_subdev_match);

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
		if (therm_est_subdev_get_temp(est->devs[i].dev_data, &temp))
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

	queue_delayed_work(est->workqueue, &est->therm_est_work,
				msecs_to_jiffies(est->polling_period));
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

		if (therm_est_subdev_get_temp(dev->dev_data, &temp))
			return -EINVAL;

		for (j = 0; j < HIST_LEN; j++)
			dev->hist[j] = temp;
	}

	return 0;
}

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

#ifdef CONFIG_OF
static int __parse_dt_trip(struct device_node *np,
			   struct thermal_trip_info *trips)
{
	const char *str;
	u32 val;
	int ret;

	ret = of_property_read_string(np, "cdev-type", &str);
	if (ret < 0)
		return ret;
	trips->cdev_type = (char *)str;

	ret = of_property_read_string(np, "trip-type", &str);
	if (ret < 0)
		return ret;

	if (!strcasecmp("active", str))
		trips->trip_type = THERMAL_TRIP_ACTIVE;
	else if (!strcasecmp("passive", str))
		trips->trip_type = THERMAL_TRIP_PASSIVE;
	else if (!strcasecmp("hot", str))
		trips->trip_type = THERMAL_TRIP_HOT;
	else if (!strcasecmp("critical", str))
		trips->trip_type = THERMAL_TRIP_CRITICAL;
	else
		return -EINVAL;

	ret = of_property_read_u32(np, "trip-temp", &val);
	if (ret < 0)
		return ret;
	trips->trip_temp = val;

	trips->hysteresis = 0;
	if (of_property_read_u32(np, "hysteresis", &val) == 0)
		trips->hysteresis = val;

	trips->upper = THERMAL_NO_LIMIT;
	if (of_property_read_string(np, "upper", &str) == 0) {
		if (kstrtou32(str, 10, &val) == 0)
			trips->upper = val;
	}

	trips->lower = THERMAL_NO_LIMIT;
	if (of_property_read_string(np, "lower", &str) == 0) {
		if (kstrtou32(str, 10, &val) == 0)
			trips->lower = val;
	}

	return 0;
}

static int __parse_dt_subdev(struct device_node *np,
			     struct therm_est_subdevice *subdev)
{
	const char *str;
	char *sbegin;
	int i = 0;
	int ret;

	subdev->dev_data = (void *)of_get_property(np, "dev-data", NULL);
	if (!subdev->dev_data)
		return -ENODATA;

	ret = of_property_read_string(np, "coeffs", &str);
	if (ret < 0)
		return ret;

	while (str && (i < HIST_LEN)) {
		str = skip_spaces(str);
		sbegin = strsep((char **)&str, " ");
		if (!sbegin || (kstrtol((const char *)sbegin, 10,
				&subdev->coeffs[i++]) < 0))
			break;
	}

	if (i != HIST_LEN)
		return -EINVAL;

	return 0;
}

static int __parse_dt_tzp(struct device_node *np,
			  struct thermal_zone_params *tzp)
{
	const char *str;

	if (of_property_read_string(np, "governor", &str) == 0)
		strncpy(tzp->governor_name, str, THERMAL_NAME_LENGTH);

	return 0;
}

static int __parse_dt_timer_trip(struct device_node *np,
				 struct therm_est_timer_trip_info *timer_info)
{
	struct device_node *ch;
	u32 val;
	int n_timers;
	int ret;

	ret = of_property_read_u32(np, "trip", &val);
	if (ret < 0)
		return ret;
	timer_info->trip = val;

	n_timers = 0;
	for_each_child_of_node(np, ch) {
		if (!of_device_is_compatible(ch, "nvidia,therm-est-timer"))
			continue;

		ret = of_property_read_u32(ch, "time-after", &val);
		if (ret < 0)
			return ret;
		timer_info->timers[n_timers].time_after = val;

		ret = of_property_read_u32(ch, "trip-temp", &val);
		if (ret < 0)
			return ret;
		timer_info->timers[n_timers].trip_temp = val;

		timer_info->timers[n_timers].hysteresis = 0;
		if (of_property_read_u32(ch, "hysteresis", &val) == 0)
			timer_info->timers[n_timers].hysteresis = val;

		n_timers++;
	}

	timer_info->num_timers = n_timers;

	return 0;
}

static struct therm_est_data *therm_est_get_pdata(struct device *dev)
{
	struct therm_est_data *data;
	struct device_node *np;
	struct device_node *ch;
	u32 val;
	int i, j, k, l;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "nvidia,therm-est");
	if (!np)
		return dev->platform_data;

	data = devm_kzalloc(dev, sizeof(struct therm_est_data), GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_u32(np, "toffset", &val);
	if (ret < 0)
		return ERR_PTR(ret);
	data->toffset = val;

	ret = of_property_read_u32(np, "polling-period", &val);
	if (ret < 0)
		return ERR_PTR(ret);
	data->polling_period = val;

	ret = of_property_read_u32(np, "passive-delay", &val);
	if (ret < 0)
		return ERR_PTR(ret);
	data->passive_delay = val;

	ret = of_property_read_u32(np, "tc1", &val);
	if (ret < 0)
		return ERR_PTR(ret);
	data->tc1 = val;

	ret = of_property_read_u32(np, "tc2", &val);
	if (ret < 0)
		return ERR_PTR(ret);
	data->tc2 = val;

	i = j = k = l = 0;
	for_each_child_of_node(np, ch) {
		if (of_device_is_compatible(ch, "nvidia,therm-est-trip"))
			i++;
		else if (of_device_is_compatible(ch, "nvidia,therm-est-subdev"))
			j++;
		else if (of_device_is_compatible(ch, "nvidia,therm-est-tzp"))
			k++;
		else if (of_device_is_compatible(ch,
						 "nvidia,therm-est-timer-trip"))
			l++;
	}

	/* trip point information and subdevices are must required data. */
	if ((i == 0) || (j == 0))
		return ERR_PTR(-ENOENT);

	data->trips = devm_kzalloc(dev, sizeof(struct thermal_trip_info) * i,
				   GFP_KERNEL);
	if (!data->trips)
		return ERR_PTR(-ENOMEM);

	data->devs = devm_kzalloc(dev, sizeof(struct therm_est_subdevice) * j,
				  GFP_KERNEL);
	if (!data->devs)
		return ERR_PTR(-ENOMEM);

	/* thermal zone params is optional data. */
	if (k > 0) {
		data->tzp = devm_kzalloc(dev,
			sizeof(struct thermal_zone_params) * k, GFP_KERNEL);
		if (!data->tzp)
			return ERR_PTR(-ENOMEM);
	}

	/* timer trip point information is optional data. */
	if (l > 0) {
		data->timer_trips = devm_kzalloc(dev,
				sizeof(struct therm_est_timer_trip_info) * l,
				GFP_KERNEL);
		if (!data->timer_trips)
			return ERR_PTR(-ENOMEM);
	}

	i = j = l = 0;
	for_each_child_of_node(np, ch) {
		if (of_device_is_compatible(ch, "nvidia,therm-est-trip")) {
			ret = __parse_dt_trip(ch, &data->trips[i++]);
			if (ret < 0)
				return ERR_PTR(ret);
		} else if (of_device_is_compatible(ch,
						   "nvidia,therm-est-subdev")) {
			ret = __parse_dt_subdev(ch, &data->devs[j++]);
			if (ret < 0)
				return ERR_PTR(ret);
		} else if (of_device_is_compatible(ch,
						   "nvidia,therm-est-tzp")) {
			ret = __parse_dt_tzp(ch, data->tzp);
			if (ret < 0)
				return ERR_PTR(ret);
		} else if (of_device_is_compatible(ch,
					"nvidia,therm-est-timer-trip")) {
			ret = __parse_dt_timer_trip(ch, &data->timer_trips[l]);
			if (!ret)
				l++;
		}
	}

	data->num_trips = i;
	data->ndevs = j;
	data->num_timer_trips = l;

	return data;
}
#else
static struct therm_est_data *therm_est_get_pdata(struct device *dev)
{
	return dev->platform_data;
}
#endif /* CONFIG_OF */

static int __devinit therm_est_probe(struct platform_device *pdev)
{
	int i;
	struct therm_estimator *est;
	struct therm_est_data *data;

	est = kzalloc(sizeof(struct therm_estimator), GFP_KERNEL);
	if (IS_ERR_OR_NULL(est))
		return -ENOMEM;

	platform_set_drvdata(pdev, est);

	data = therm_est_get_pdata(&pdev->dev);

	est->devs = data->devs;
	est->ndevs = data->ndevs;
	est->toffset = data->toffset;
	est->polling_period = data->polling_period;
	est->tc1 = data->tc1;
	est->tc2 = data->tc2;

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
				    WQ_HIGHPRI | WQ_UNBOUND | WQ_RESCUER, 1);
	if (!est->workqueue)
		goto err;

	INIT_DELAYED_WORK(&est->therm_est_work, therm_est_work_func);

	queue_delayed_work(est->workqueue,
				&est->therm_est_work,
				msecs_to_jiffies(est->polling_period));

	est->num_trips = data->num_trips;
	est->trips = data->trips;
	est->tzp = data->tzp;

	est->thz = thermal_zone_device_register(dev_name(&pdev->dev),
						est->num_trips,
						(1 << est->num_trips) - 1,
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

	return 0;
err:
	cancel_delayed_work_sync(&est->therm_est_work);
	if (est->workqueue)
		destroy_workqueue(est->workqueue);
	kfree(est);
	return -EINVAL;
}

static int __devexit therm_est_remove(struct platform_device *pdev)
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
	destroy_workqueue(est->workqueue);
	kfree(est);
	return 0;
}

static void __devexit therm_est_shutdown(struct platform_device *pdev)
{
	therm_est_remove(pdev);
}

static struct platform_driver therm_est_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = "therm_est",
	},
	.probe  = therm_est_probe,
	.remove = __devexit_p(therm_est_remove),
	.shutdown = __devexit_p(therm_est_shutdown),
};

static int __init therm_est_driver_init(void)
{
	return platform_driver_register(&therm_est_driver);
}
module_init(therm_est_driver_init);
