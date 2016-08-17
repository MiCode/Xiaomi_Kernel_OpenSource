/*
 * drivers/staging/iio/generic_adc_thermal.c
 *
 * Generic ADC thermal driver
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
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
};

static int gadc_thermal_get_temp(struct thermal_zone_device *tz,
				 unsigned long *temp)
{
	struct gadc_thermal_driver_data *drvdata = tz->devdata;
	int val;
	int ret;

	ret = iio_st_read_channel_raw(drvdata->channel, &val);
	if (ret < 0) {
		dev_err(drvdata->dev, "%s: Failed to read channel, %d\n",
			__func__, ret);
		return ret;
	}

	if (drvdata->pdata->adc_to_temp)
		*temp = drvdata->pdata->adc_to_temp(drvdata->pdata, val);
	else
		*temp = val;

	*temp += drvdata->pdata->temp_offset;
	return 0;
}

static struct thermal_zone_device_ops gadc_thermal_ops = {
	.get_temp = gadc_thermal_get_temp,
};

#ifdef CONFIG_DEBUG_FS
static int adc_temp_show(struct seq_file *s, void *p)
{
	struct gadc_thermal_driver_data *drvdata = s->private;
	int adc, temp;
	int ret;

	ret = iio_st_read_channel_raw(drvdata->channel, &adc);
	if (ret < 0) {
		dev_err(drvdata->dev, "%s: Failed to read channel, %d\n",
			__func__, ret);
		return ret;
	}

	if (drvdata->pdata->adc_to_temp)
		temp = drvdata->pdata->adc_to_temp(drvdata->pdata, adc);
	else
		temp = adc;

	temp += drvdata->pdata->temp_offset;

	seq_printf(s, "%d %d\n", adc, temp);
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

static int __devinit gadc_thermal_probe(struct platform_device *pdev)
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

	platform_set_drvdata(pdev, drvdata);
	drvdata->dev = &pdev->dev;
	drvdata->pdata = pdata;

	drvdata->channel = iio_st_channel_get(dev_name(&pdev->dev),
					pdata->iio_channel_name);
	if (IS_ERR(drvdata->channel)) {
		dev_err(&pdev->dev, "%s: Failed to get channel %s, %ld\n",
			__func__, pdata->iio_channel_name,
			PTR_ERR(drvdata->channel));
		return PTR_ERR(drvdata->channel);
	}

	drvdata->tz = thermal_zone_device_register(pdata->tz_name, 0, 0,
					drvdata, &gadc_thermal_ops, NULL, 0, 0);
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
	iio_st_channel_release(drvdata->channel);
	return ret;
}

static int __devexit gadc_thermal_remove(struct platform_device *pdev)
{
	struct gadc_thermal_driver_data *drvdata = platform_get_drvdata(pdev);

	if (drvdata->dentry)
		debugfs_remove_recursive(drvdata->dentry);
	thermal_zone_device_unregister(drvdata->tz);
	iio_st_channel_release(drvdata->channel);
	return 0;
}

static struct platform_driver gadc_thermal_driver = {
	.driver = {
		.name = "generic-adc-thermal",
		.owner = THIS_MODULE,
	},
	.probe = gadc_thermal_probe,
	.remove = __devexit_p(gadc_thermal_remove),
};

static int gadc_thermal_init(void)
{
	return platform_driver_register(&gadc_thermal_driver);
}
late_initcall(gadc_thermal_init);

static void gadc_thermal_exit(void)
{
	platform_driver_unregister(&gadc_thermal_driver);
}
module_exit(gadc_thermal_exit);

MODULE_AUTHOR("Jinyoung Park <jinyoungp@nvidia.com>");
MODULE_DESCRIPTION("Generic ADC thermal driver using IIO framework");
MODULE_LICENSE("GPL v2");
