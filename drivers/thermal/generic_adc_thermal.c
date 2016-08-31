/*
 * drivers/staging/iio/generic_adc_thermal.c
 *
 * Generic ADC thermal driver
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All rights reserved.
 *
 * Author: Jinyoung Park <jinyoungp@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/thermal.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/generic_adc_thermal.h>

struct gadc_thermal_driver_data {
	struct device *dev;
	struct thermal_zone_device *tz;
	struct iio_channel *channel;
	struct gadc_thermal_platform_data *pdata;
	struct dentry *dentry;
	int *adc_temp_lookup;
	unsigned int lookup_table_size;
	int first_index_temp;
	int last_index_temp;
	int temp_offset;
	bool dual_mode;
};

static int gadc_thermal_thermistor_adc_to_temp(
	struct gadc_thermal_driver_data *drvdata, int adc_raw)
{

	int *lookup_table = drvdata->adc_temp_lookup;
	int table_size = drvdata->lookup_table_size;
	int first_index_temp = drvdata->first_index_temp;
	int last_index_temp = drvdata->last_index_temp;
	int i;
	int temp, adc_low, adc_high, diff_temp;
	int temp_decrement = (first_index_temp > last_index_temp);

	for (i = 0; i < table_size - 1; ++i) {
		if (temp_decrement) {
			if (adc_raw <= lookup_table[i])
				break;
		} else {
			if (adc_raw >= lookup_table[i])
				break;
		}
	}

	if (i == 0)
		return first_index_temp;
	if (i == table_size)
		return last_index_temp;

	/* Find temp with interpolate reading */
	adc_low = lookup_table[i-1];
	adc_high = lookup_table[i];
	diff_temp = (adc_high - adc_low) * 1000;
	if (temp_decrement) {
		temp = (first_index_temp - i) * 1000;
		temp += ((adc_high - adc_raw) * 1000) / (adc_high - adc_low);
	} else {
		temp = (first_index_temp + i - 1) * 1000;
		temp += (adc_low - adc_raw) * 1000 / (adc_high - adc_low);
	}
	return temp;
}

static int gadc_thermal_read_channel(struct gadc_thermal_driver_data *drvdata,
				     int *val, int *val2)
{
	int ret;

	if (drvdata->dual_mode) {
		ret = iio_read_channel_processed_dual(drvdata->channel, val,
						      val2);
		if (ret < 0)
			ret = iio_read_channel_raw_dual(drvdata->channel, val,
							val2);
	} else {
		ret = iio_read_channel_processed(drvdata->channel, val);
		if (ret < 0)
			ret = iio_read_channel_raw(drvdata->channel, val);
	}
	return ret;
}

static int gadc_thermal_bind(struct thermal_zone_device *tz,
			     struct thermal_cooling_device *cdev)
{
	struct gadc_thermal_driver_data *drvdata = tz->devdata;
	struct thermal_trip_info *trip_state;
	int i, ret;

	for (i = 0; i < drvdata->pdata->num_trips; i++) {
		trip_state = &drvdata->pdata->trips[i];
		if (trip_state->cdev_type &&
		    !strncmp(trip_state->cdev_type, cdev->type,
			     THERMAL_NAME_LENGTH)) {
			ret = thermal_zone_bind_cooling_device(tz, i, cdev,
					trip_state->upper, trip_state->lower);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

static int gadc_thermal_unbind(struct thermal_zone_device *tz,
			       struct thermal_cooling_device *cdev)
{
	struct gadc_thermal_driver_data *drvdata = tz->devdata;
	struct thermal_trip_info *trip_state;
	int i, ret;

	for (i = 0; i < drvdata->pdata->num_trips; i++) {
		trip_state = &drvdata->pdata->trips[i];
		if (trip_state->cdev_type &&
		    !strncmp(trip_state->cdev_type, cdev->type,
			     THERMAL_NAME_LENGTH)) {
			ret = thermal_zone_unbind_cooling_device(tz, i, cdev);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

static int gadc_thermal_get_temp(struct thermal_zone_device *tz,
				 unsigned long *temp)
{
	struct gadc_thermal_driver_data *drvdata = tz->devdata;
	int val = 0, val2 = 0;
	int ret;

	ret = gadc_thermal_read_channel(drvdata, &val, &val2);
	if (ret < 0) {
		dev_err(drvdata->dev, "%s: Failed to read channel, %d\n",
			__func__, ret);
		return ret;
	}

	if (drvdata->pdata->adc_to_temp)
		*temp = drvdata->pdata->adc_to_temp(drvdata->pdata, val, val2);
	else if (drvdata->pdata->adc_temp_lookup)
		*temp = gadc_thermal_thermistor_adc_to_temp(drvdata, val);
	else
		*temp = val;

	*temp += drvdata->temp_offset;
	return 0;
}

static int gadc_thermal_get_trip_type(struct thermal_zone_device *tz, int trip,
				      enum thermal_trip_type *type)
{
	struct gadc_thermal_driver_data *drvdata = tz->devdata;
	struct thermal_trip_info *trip_state = &drvdata->pdata->trips[trip];

	*type = trip_state->trip_type;
	return 0;
}

static int gadc_thermal_get_trip_temp(struct thermal_zone_device *tz, int trip,
				      unsigned long *temp)
{
	struct gadc_thermal_driver_data *drvdata = tz->devdata;
	struct thermal_trip_info *trip_state = &drvdata->pdata->trips[trip];
	long zone_temp = tz->temperature;
	long trip_temp = trip_state->trip_temp;
	long hysteresis = trip_state->hysteresis;

	if (zone_temp >= trip_temp) {
		trip_temp -= hysteresis;
		trip_state->tripped = true;
	} else if (trip_state->tripped) {
		trip_temp -= hysteresis;
		if (zone_temp < trip_temp)
			trip_state->tripped = false;
	}

	*temp = trip_temp;
	return 0;
}

static int gadc_thermal_set_trip_temp(struct thermal_zone_device *tz, int trip,
				      unsigned long temp)
{
	struct gadc_thermal_driver_data *drvdata = tz->devdata;
	struct thermal_trip_info *trip_state = &drvdata->pdata->trips[trip];

	trip_state->trip_temp = temp;
	return 0;
}

static struct thermal_zone_device_ops gadc_thermal_ops = {
	.bind = gadc_thermal_bind,
	.unbind = gadc_thermal_unbind,
	.get_temp = gadc_thermal_get_temp,
	.get_trip_type = gadc_thermal_get_trip_type,
	.get_trip_temp = gadc_thermal_get_trip_temp,
	.set_trip_temp = gadc_thermal_set_trip_temp,
};


#ifdef CONFIG_DEBUG_FS
static int adc_temp_show(struct seq_file *s, void *p)
{
	struct gadc_thermal_driver_data *drvdata = s->private;
	int val = 0, val2 = 0, temp = 0;
	int ret;

	ret = gadc_thermal_read_channel(drvdata, &val, &val2);
	if (ret < 0) {
		dev_err(drvdata->dev, "%s: Failed to read channel, %d\n",
			__func__, ret);
		return ret;
	}

	if (drvdata->pdata->adc_to_temp)
		temp = drvdata->pdata->adc_to_temp(drvdata->pdata, val, val2);
	else if (drvdata->pdata->adc_temp_lookup)
		temp = gadc_thermal_thermistor_adc_to_temp(drvdata, val);
	else
		temp = val;

	temp += drvdata->temp_offset;
	seq_printf(s, "%d %d %d\n", val, val2, temp);
	return 0;
}

static int adc_temp_open(struct inode *inode, struct file *file)
{
	return single_open(file, adc_temp_show, inode->i_private);
}

static const struct file_operations adc_temp_fops = {
	.open		= adc_temp_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int temp_offset_write(struct file *file, const char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	struct gadc_thermal_driver_data *drvdata =
			((struct seq_file *)(file->private_data))->private;
	char buf[32];
	ssize_t buf_size;
	char *start = buf;
	long val;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size)) {
		dev_err(drvdata->dev, "%s: Failed to copy from user\n",
			__func__);
		return -EFAULT;
	}
	buf[buf_size] = 0;

	while (*start == ' ')
		start++;

	if (kstrtol(start, 10, &val))
		return -EINVAL;

	drvdata->pdata->temp_offset = val;
	return buf_size;
}

static int temp_offset_show(struct seq_file *s, void *p)
{
	struct gadc_thermal_driver_data *drvdata = s->private;

	seq_printf(s, "%d\n", drvdata->pdata->temp_offset);
	return 0;
}

static int temp_offset_open(struct inode *inode, struct file *file)
{
	return single_open(file, temp_offset_show, inode->i_private);
}

static const struct file_operations temp_offset_fops = {
	.open		= temp_offset_open,
	.write		= temp_offset_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int gadc_thermal_debugfs_init(struct gadc_thermal_driver_data *drvdata)
{
	struct dentry *d_file;

	drvdata->dentry = debugfs_create_dir(dev_name(drvdata->dev), NULL);
	if (!drvdata->dentry)
		return -ENOMEM;

	d_file = debugfs_create_file("adc_temp", 0444, drvdata->dentry,
				     drvdata, &adc_temp_fops);
	if (!d_file)
		goto error;

	d_file = debugfs_create_file("temp_offset", 0644, drvdata->dentry,
				     drvdata, &temp_offset_fops);
	if (!d_file)
		goto error;

	return 0;

error:
	debugfs_remove_recursive(drvdata->dentry);
	return -ENOMEM;
}
#endif /*  CONFIG_DEBUG_FS */

static int gadc_thermal_probe(struct platform_device *pdev)
{
	struct gadc_thermal_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct gadc_thermal_driver_data *drvdata;
	int ret;

	if (!pdata) {
		dev_err(&pdev->dev, "%s: No platform data\n", __func__);
		return -ENODEV;
	}

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		dev_err(&pdev->dev,
			"%s: Failed to alloc memory for driver data\n",
			__func__);
		return -ENOMEM;
	}
	if (pdata->lookup_table_size) {
		drvdata->adc_temp_lookup =  devm_kzalloc(&pdev->dev,
				pdata->lookup_table_size * sizeof(unsigned int),
				GFP_KERNEL);
		if (!drvdata->adc_temp_lookup)
			return -ENOMEM;
	}

	platform_set_drvdata(pdev, drvdata);
	drvdata->dev = &pdev->dev;
	drvdata->pdata = pdata;
	drvdata->lookup_table_size = pdata->lookup_table_size;
	drvdata->first_index_temp = pdata->first_index_temp;
	drvdata->last_index_temp = pdata->last_index_temp;
	drvdata->temp_offset = pdata->temp_offset;
	drvdata->dual_mode = pdata->dual_mode;
	if (drvdata->lookup_table_size)
		memcpy(drvdata->adc_temp_lookup, pdata->adc_temp_lookup,
			pdata->lookup_table_size * sizeof(unsigned int));

	drvdata->channel = iio_channel_get(&pdev->dev,
					pdata->iio_channel_name);
	if (IS_ERR(drvdata->channel)) {
		dev_err(&pdev->dev, "%s: Failed to get channel %s, %ld\n",
			__func__, pdata->iio_channel_name,
			PTR_ERR(drvdata->channel));
		return PTR_ERR(drvdata->channel);
	}

	drvdata->tz = thermal_zone_device_register(pdata->tz_name,
					pdata->num_trips,
					(1ULL << pdata->num_trips) - 1,
					drvdata, &gadc_thermal_ops,
					pdata->tzp, 0, pdata->polling_delay);
	if (IS_ERR(drvdata->tz)) {
		dev_err(&pdev->dev,
			"%s: Failed to register thermal zone %s, %ld\n",
			__func__, pdata->tz_name, PTR_ERR(drvdata->tz));
		ret = PTR_ERR(drvdata->tz);
		goto error_release_channel;
	}

	gadc_thermal_debugfs_init(drvdata);

	return 0;

error_release_channel:
	iio_channel_release(drvdata->channel);
	return ret;
}

static int gadc_thermal_remove(struct platform_device *pdev)
{
	struct gadc_thermal_driver_data *drvdata = platform_get_drvdata(pdev);

	if (drvdata->dentry)
		debugfs_remove_recursive(drvdata->dentry);
	thermal_zone_device_unregister(drvdata->tz);
	iio_channel_release(drvdata->channel);
	return 0;
}

static struct platform_driver gadc_thermal_driver = {
	.driver = {
		.name = "generic-adc-thermal",
		.owner = THIS_MODULE,
	},
	.probe = gadc_thermal_probe,
	.remove = gadc_thermal_remove,
};

module_platform_driver(gadc_thermal_driver);

MODULE_AUTHOR("Jinyoung Park <jinyoungp@nvidia.com>");
MODULE_DESCRIPTION("Generic ADC thermal driver using IIO framework");
MODULE_LICENSE("GPL v2");
