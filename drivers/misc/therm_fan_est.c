/*
 * drivers/misc/therm_fan_est.c
 *
 * Copyright (C) 2010-2012 NVIDIA Corporation.
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

struct therm_fan_estimator {
	long cur_temp;
	long polling_period;
	struct workqueue_struct *workqueue;
	struct delayed_work therm_fan_est_work;
	long toffset;
	int ntemp;
	int ndevs;
	struct therm_fan_est_subdevice *devs;
	struct thermal_zone_device *thz;
	int current_trip_index;
	char *cdev_type;
	int active_trip_temps[MAX_ACTIVE_STATES];
	int active_hysteresis[MAX_ACTIVE_STATES];
	int active_trip_temps_hyst[(MAX_ACTIVE_STATES << 1) + 1];
};


static void fan_set_trip_temp_hyst(struct therm_fan_estimator *est, int trip,
							unsigned long hyst_temp,
							unsigned long trip_temp)
{
	est->active_hysteresis[trip] = hyst_temp;
	est->active_trip_temps[trip] = trip_temp;
	est->active_trip_temps_hyst[(trip << 1)] = trip_temp;
	est->active_trip_temps_hyst[((trip - 1) << 1) + 1] =
						trip_temp - hyst_temp;
}

static void therm_fan_est_work_func(struct work_struct *work)
{
	int i, j, index, trip_index, sum = 0;
	long temp;
	struct delayed_work *dwork = container_of(work,
					struct delayed_work, work);
	struct therm_fan_estimator *est = container_of(
					dwork,
					struct therm_fan_estimator,
					therm_fan_est_work);

	for (i = 0; i < est->ndevs; i++) {
		if (est->devs[i].get_temp(est->devs[i].dev_data, &temp))
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

	for (trip_index = 0;
		trip_index < ((MAX_ACTIVE_STATES << 1) + 1); trip_index++) {
		if (est->cur_temp < est->active_trip_temps_hyst[trip_index])
			break;
	}

	if (est->current_trip_index != (trip_index - 1)) {
		est->current_trip_index = trip_index - 1;
		if (!((trip_index - 1) % 2))
			thermal_zone_device_update(est->thz);
	}

	est->ntemp++;

	queue_delayed_work(est->workqueue, &est->therm_fan_est_work,
				msecs_to_jiffies(est->polling_period));
}

static int therm_fan_est_bind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	int i;
	struct therm_fan_estimator *est = thz->devdata;
	if (!strcmp(cdev->type, est->cdev_type)) {
		for (i = 0; i < MAX_ACTIVE_STATES; i++)
			thermal_zone_bind_cooling_device(thz, i, cdev, i, i);
	}

	return 0;
}

static int therm_fan_est_unbind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	int i;
	struct therm_fan_estimator *est = thz->devdata;
	if (!strcmp(cdev->type, est->cdev_type)) {
		for (i = 0; i < MAX_ACTIVE_STATES; i++)
			thermal_zone_unbind_cooling_device(thz, i, cdev);
	}

	return 0;
}

static int therm_fan_est_get_trip_type(struct thermal_zone_device *thz,
					int trip,
					enum thermal_trip_type *type)
{
	*type = THERMAL_TRIP_ACTIVE;
	return 0;
}

static int therm_fan_est_get_trip_temp(struct thermal_zone_device *thz,
					int trip,
					unsigned long *temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	*temp = est->active_trip_temps[trip];
	return 0;
}

static int therm_fan_est_set_trip_temp(struct thermal_zone_device *thz,
					int trip,
					unsigned long temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	/*Need trip 0 to remain as it is*/
	if (((temp - est->active_hysteresis[trip]) < 0) || (trip <= 0))
		return -EINVAL;

	fan_set_trip_temp_hyst(est, trip, est->active_hysteresis[trip], temp);
	return 0;
}

static int therm_fan_est_get_temp(struct thermal_zone_device *thz,
				unsigned long *temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	*temp = est->cur_temp;
	return 0;
}

static int therm_fan_est_set_trip_hyst(struct thermal_zone_device *thz,
				int trip, unsigned long hyst_temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	/*Need trip 0 to remain as it is*/
	if ((est->active_trip_temps[trip] - hyst_temp) < 0 || trip <= 0)
		return -EINVAL;

	fan_set_trip_temp_hyst(est, trip,
			hyst_temp, est->active_trip_temps[trip]);
	return 0;
}

static int therm_fan_est_get_trip_hyst(struct thermal_zone_device *thz,
				int trip, unsigned long *temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	*temp = est->active_hysteresis[trip];
	return 0;
}

static struct thermal_zone_device_ops therm_fan_est_ops = {
	.bind = therm_fan_est_bind,
	.unbind = therm_fan_est_unbind,
	.get_trip_type = therm_fan_est_get_trip_type,
	.get_trip_temp = therm_fan_est_get_trip_temp,
	.get_temp = therm_fan_est_get_temp,
	.set_trip_temp = therm_fan_est_set_trip_temp,
	.get_trip_hyst = therm_fan_est_get_trip_hyst,
	.set_trip_hyst = therm_fan_est_set_trip_hyst,
};

static ssize_t show_coeff(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
	ssize_t len, total_len = 0;
	int i, j;

	for (i = 0; i < est->ndevs; i++) {
		len = snprintf(buf + total_len, PAGE_SIZE, "[%d]", i);
		total_len += len;
		for (j = 0; j < HIST_LEN; j++) {
			len = snprintf(buf + total_len, PAGE_SIZE, " %ld",
					est->devs[i].coeffs[j]);
			total_len += len;
		}
		len = snprintf(buf + total_len, PAGE_SIZE, "\n");
		total_len += len;
	}
	return strlen(buf);
}

static ssize_t set_coeff(struct device *dev,
				struct device_attribute *da,
				const char *buf, size_t count)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
	int devid, scount;
	long coeff[20];

	if (HIST_LEN > 20)
		return -EINVAL;

	scount = sscanf(buf, "[%d] %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld " \
			"%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
			&devid,	&coeff[0], &coeff[1], &coeff[2], &coeff[3],
			&coeff[4], &coeff[5], &coeff[6], &coeff[7], &coeff[8],
			&coeff[9], &coeff[10], &coeff[11], &coeff[12],
			&coeff[13], &coeff[14],	&coeff[15], &coeff[16],
			&coeff[17], &coeff[18],	&coeff[19]);

	if (scount != HIST_LEN + 1)
		return -1;

	if (devid < 0 || devid >= est->ndevs)
		return -EINVAL;

	/* This has obvious locking issues but don't worry about it */
	memcpy(est->devs[devid].coeffs, coeff, sizeof(long) * HIST_LEN);

	return count;
}

static ssize_t show_offset(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);

	snprintf(buf, PAGE_SIZE, "%ld\n", est->toffset);
	return strlen(buf);
}

static ssize_t set_offset(struct device *dev,
				struct device_attribute *da,
				const char *buf, size_t count)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
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
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
	ssize_t total_len = 0;
	int i, j;
	int index;

	/* This has obvious locking issues but don't worry about it */
	for (i = 0; i < est->ndevs; i++) {
		total_len += snprintf(buf + total_len, PAGE_SIZE, "[%d]", i);
		for (j = 0; j < HIST_LEN; j++) {
			index = (est->ntemp - j + HIST_LEN) % HIST_LEN;
			total_len += snprintf(buf + total_len,
						PAGE_SIZE,
						" %ld",
						est->devs[i].hist[index]);
		}
		total_len += snprintf(buf + total_len, PAGE_SIZE, "\n");
	}
	return strlen(buf);
}

static struct sensor_device_attribute therm_fan_est_nodes[] = {
	SENSOR_ATTR(coeff, S_IRUGO | S_IWUSR, show_coeff, set_coeff, 0),
	SENSOR_ATTR(offset, S_IRUGO | S_IWUSR, show_offset, set_offset, 0),
	SENSOR_ATTR(temps, S_IRUGO, show_temps, 0, 0),
};

static int __devinit therm_fan_est_probe(struct platform_device *pdev)
{
	int i, j;
	long temp;
	struct therm_fan_estimator *est;
	struct therm_fan_est_subdevice *dev;
	struct therm_fan_est_data *data;

	est = devm_kzalloc(&pdev->dev,
				sizeof(struct therm_fan_estimator), GFP_KERNEL);
	if (IS_ERR_OR_NULL(est))
		return -ENOMEM;

	platform_set_drvdata(pdev, est);

	data = pdev->dev.platform_data;

	est->devs = data->devs;
	est->ndevs = data->ndevs;
	est->toffset = data->toffset;
	est->polling_period = data->polling_period;

	for (i = 0; i < MAX_ACTIVE_STATES; i++) {
		est->active_trip_temps[i] = data->active_trip_temps[i];
		est->active_hysteresis[i] = data->active_hysteresis[i];
	}

	est->active_trip_temps_hyst[0] = data->active_trip_temps[0];

	for (i = 1; i < MAX_ACTIVE_STATES; i++)
		fan_set_trip_temp_hyst(est, i,
			data->active_hysteresis[i], est->active_trip_temps[i]);

	/* initialize history */
	for (i = 0; i < data->ndevs; i++) {
		dev = &est->devs[i];

		if (dev->get_temp(dev->dev_data, &temp))
			return -EINVAL;

		for (j = 0; j < HIST_LEN; j++)
			dev->hist[j] = temp;
	}

	est->workqueue = alloc_workqueue(dev_name(&pdev->dev),
				    WQ_HIGHPRI | WQ_UNBOUND | WQ_RESCUER, 1);
	if (!est->workqueue)
		return -ENOMEM;

	est->current_trip_index = 0;

	INIT_DELAYED_WORK(&est->therm_fan_est_work, therm_fan_est_work_func);

	queue_delayed_work(est->workqueue,
				&est->therm_fan_est_work,
				msecs_to_jiffies(est->polling_period));
	est->cdev_type = data->cdev_type;
	est->thz = thermal_zone_device_register((char *) dev_name(&pdev->dev),
						10, 0x3FF, est,
						&therm_fan_est_ops, NULL, 0, 0);
	if (IS_ERR_OR_NULL(est->thz))
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(therm_fan_est_nodes); i++)
		device_create_file(&pdev->dev,
			&therm_fan_est_nodes[i].dev_attr);

	return 0;
}

static int __devexit therm_fan_est_remove(struct platform_device *pdev)
{
	struct therm_fan_estimator *est = platform_get_drvdata(pdev);

	if (!est)
		return -EINVAL;

	cancel_delayed_work(&est->therm_fan_est_work);
	thermal_zone_device_unregister(est->thz);

	return 0;
}

#if CONFIG_PM
static int therm_fan_est_suspend(struct platform_device *pdev,
							pm_message_t state)
{
	struct therm_fan_estimator *est = platform_get_drvdata(pdev);

	if (!est)
		return -EINVAL;

	cancel_delayed_work(&est->therm_fan_est_work);

	return 0;
}

static int therm_fan_est_resume(struct platform_device *pdev)
{
	struct therm_fan_estimator *est = platform_get_drvdata(pdev);

	if (!est)
		return -EINVAL;

	queue_delayed_work(est->workqueue,
				&est->therm_fan_est_work,
				msecs_to_jiffies(est->polling_period));
	return 0;
}
#endif

static struct platform_driver therm_fan_est_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = "therm-fan-est",
	},
	.probe  = therm_fan_est_probe,
	.remove = __devexit_p(therm_fan_est_remove),
#if CONFIG_PM
	.suspend = therm_fan_est_suspend,
	.resume = therm_fan_est_resume,
#endif
};

module_platform_driver(therm_fan_est_driver);

MODULE_DESCRIPTION("fan thermal estimator");
MODULE_AUTHOR("Anshul Jain <anshulj@nvidia.com>");
MODULE_LICENSE("GPL v2");
