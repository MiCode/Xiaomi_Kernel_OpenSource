// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "thermal_core.h"

#define SPACE_DELIMITER " \t"

static struct dentry *thermal_debugfs_parent;
static struct dentry *thermal_debugfs_config;

static int fetch_cdev(struct thermal_zone_device *tz, char *dev_token,
		char *upper_lim_token, char *lower_lim_token, int trip)
{
	unsigned long upper_limit, lower_limit;
	char cdev_name[THERMAL_NAME_LENGTH] = "";
	char limit_str[THERMAL_NAME_LENGTH] = "";
	struct thermal_instance *instance;
	bool match_found = false;

	dev_token = strim(dev_token);
	if (sscanf(dev_token, "%20[^ +\n\t]", cdev_name) != 1)
		return -EINVAL;
	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (!instance->cdev || instance->trip != trip)
			continue;

		if (strcmp(instance->cdev->type, cdev_name))
			continue;
		match_found = true;
		break;
	}
	if (!match_found)
		return -ENODEV;
	if (upper_lim_token) {
		if (sscanf(upper_lim_token, "%20[^ +\n\t]", limit_str) != 1)
			return -EINVAL;
		if (kstrtoul(limit_str, 0, &upper_limit))
			return -EINVAL;
		instance->upper = upper_limit;
	}
	if (lower_lim_token) {
		if (sscanf(lower_lim_token, "%20[^ +\n\t]", limit_str) != 1)
			return -EINVAL;
		if (kstrtoul(limit_str, 0, &lower_limit))
			return -EINVAL;
		instance->lower = lower_limit;
	}

	return 0;
}

static int fetch_and_update_cdev(struct thermal_zone_device *tz, int trip,
			char **dev_buf, char *dev_buf_end,
			char **up_buf, char *up_buf_end,
			char **low_buf, char *low_buf_end)
{
	int ret;
	char *dev_token, *upper_lim_token, *lower_lim_token, *dev1_token;

	dev_token = strsep(dev_buf, SPACE_DELIMITER);
	if (*up_buf) {
		upper_lim_token = strsep(up_buf, SPACE_DELIMITER);
		upper_lim_token = strnchr(*up_buf, up_buf_end - *up_buf,
						' ');
		if (upper_lim_token && upper_lim_token < up_buf_end)
			up_buf_end = upper_lim_token;
	}
	if (*low_buf) {
		lower_lim_token = strsep(low_buf, SPACE_DELIMITER);
		lower_lim_token = strnchr(*low_buf, low_buf_end - *low_buf,
						' ');
		if (lower_lim_token && lower_lim_token < low_buf_end)
			low_buf_end = lower_lim_token;
	}
	if (dev_token && *dev_buf < dev_buf_end) {
		do {
			ret = fetch_cdev(tz, *dev_buf, *up_buf,
					*low_buf, trip);
			if (ret)
				return ret;
			dev1_token = strnchr(*dev_buf, dev_buf_end - *dev_buf,
					'+');
			if (!dev1_token || dev1_token >= dev_buf_end)
				break;
			dev1_token = strsep(dev_buf, "+");
			if (*up_buf) {
				upper_lim_token = strnchr(*up_buf,
						up_buf_end - *up_buf,
						'+');
				if (!upper_lim_token ||
					upper_lim_token >= up_buf_end)
					return -EINVAL;
				upper_lim_token = strsep(up_buf, "+");
			}
			if (*low_buf) {
				lower_lim_token = strnchr(*low_buf,
						low_buf_end - *low_buf,
						'+');
				if (!lower_lim_token ||
					lower_lim_token >= low_buf_end)
					return -EINVAL;
				lower_lim_token = strsep(low_buf, "+");
			}
		} while (dev1_token);
	}

	return 0;
}

static int fetch_and_update_threshold(struct thermal_zone_device *tz, int trip,
			char **temp_buf, char *temp_buf_end, bool is_trip)
{
	int ret, trip_temp;
	char *tripT_token;

	tripT_token = strsep(temp_buf, SPACE_DELIMITER);
	if (tripT_token && *temp_buf < temp_buf_end) {
		if (kstrtoint(*temp_buf, 0, &trip_temp))
			return -EINVAL;
		if (is_trip)
			ret = tz->ops->set_trip_temp(tz, trip, trip_temp);
		else
			ret = tz->ops->set_trip_hyst(tz, trip, trip_temp);
		if (ret)
			return ret;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int parse_threshold(struct thermal_zone_device *tz, char *buf_ptr,
			size_t count)
{
	char *trip_buf_end = NULL, *temp_buf_end = NULL, *hyst_buf_end = NULL;
	char *trip_buf = NULL, *temp_buf = NULL, *hyst_buf = NULL;
	char *dev_buf_end = NULL, *up_buf_end = NULL, *low_buf_end = NULL;
	char *dev_buf = NULL, *up_buf = NULL, *low_buf = NULL;
	char *buf = buf_ptr, *token = NULL;
	int ret;

	trip_buf = strnstr(buf, "trip", count);
	if (!trip_buf)
		return -EINVAL;
	trip_buf_end = strnchr(trip_buf, buf_ptr + count - trip_buf, '\n');
	if (!trip_buf_end)
		return -EINVAL;

	temp_buf = strnstr(buf, "set_temp", count);
	if (!temp_buf)
		goto eval_device;
	if (!tz->ops->set_trip_temp)
		return -EPERM;

	temp_buf_end = strnchr(temp_buf, buf_ptr + count - temp_buf, '\n');
	if (!temp_buf_end)
		return -EINVAL;

	hyst_buf = strnstr(buf, "clr_temp", count);
	if (hyst_buf) {
		if (!tz->ops->set_trip_hyst)
			return -EPERM;
		hyst_buf_end = strnchr(hyst_buf,
				buf_ptr + count - hyst_buf, '\n');
		if (!hyst_buf_end)
			return -EINVAL;
	}

eval_device:
	dev_buf = strnstr(buf, "device", count);
	if (!dev_buf) {
		if (!temp_buf)
			return -EINVAL;
		goto parse_config;
	}
	dev_buf_end = strnchr(dev_buf, buf_ptr + count - dev_buf, '\n');
	if (!dev_buf_end)
		return -EINVAL;

	up_buf = strnstr(buf, "upper_limit", count);
	if (!up_buf)
		return -EINVAL;
	up_buf_end = strnchr(up_buf, buf_ptr + count - up_buf, '\n');
	if (!up_buf_end)
		return -EINVAL;

	low_buf = strnstr(buf, "lower_limit", count);
	if (!low_buf)
		return -EINVAL;
	low_buf_end = strnchr(low_buf, buf_ptr + count - low_buf, '\n');
	if (!low_buf_end)
		return -EINVAL;
parse_config:
	*trip_buf_end = '\0';
	if (temp_buf_end)
		*temp_buf_end = '\0';
	if (hyst_buf_end)
		*hyst_buf_end = '\0';
	if (dev_buf_end)
		*dev_buf_end = '\0';
	if (up_buf_end)
		*up_buf_end = '\0';
	if (low_buf_end)
		*low_buf_end = '\0';
	token = strnchr(trip_buf, trip_buf_end - trip_buf, ' ');
	while (token && token < trip_buf_end) {
		int trip;

		token = strsep((char **)&trip_buf, " ");
		if (kstrtoint(trip_buf, 0, &trip))
			return -EINVAL;

		if (temp_buf) {
			ret = fetch_and_update_threshold(tz, trip, &temp_buf,
							temp_buf_end, true);
			if (ret)
				return ret;
		}

		if (hyst_buf) {
			ret = fetch_and_update_threshold(tz, trip, &hyst_buf,
							hyst_buf_end, false);
			if (ret)
				return ret;
		}

		if (dev_buf) {
			ret = fetch_and_update_cdev(tz, trip, &dev_buf,
					dev_buf_end, &up_buf, up_buf_end,
					&low_buf, low_buf_end);
			if (ret)
				return ret;
		}
		token = strnchr(trip_buf, trip_buf_end - trip_buf, ' ');
	}

	return 0;
}

static int parse_delay(struct thermal_zone_device *tz, char *buf,
			size_t count, bool is_polling)
{
	int delay = 0;

	if (is_polling) {
		if (sscanf(buf, "polling_delay %d", &delay) != 1)
			return -EINVAL;
		tz->polling_delay = delay;
	} else {
		if (sscanf(buf, "passive_delay %d", &delay) != 1)
			return -EINVAL;
		tz->passive_delay = delay;
	}

	return 0;
}

static int parse_config(struct thermal_zone_device *tz, char *buf_ptr,
			size_t buf_ct)
{
	char *buf, *buf_end, *next_buf, *curr_buf;
	int count, ret = 0;
	enum thermal_device_mode mode = THERMAL_DEVICE_ENABLED;

	if (tz->ops->get_mode) {
		ret = tz->ops->get_mode(tz, &mode);
		if (ret)
			return ret;
	}
	if (tz->ops->set_mode) {
		ret = tz->ops->set_mode(tz, THERMAL_DEVICE_DISABLED);
		if (ret)
			return ret;
	}
	mutex_lock(&tz->lock);
	buf = kzalloc(sizeof(char) * (buf_ct + 1), GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto parse_exit;
	}
	strlcpy(buf, buf_ptr, (buf_ct + 1));
	buf_end = buf + buf_ct;
	if (strnstr(buf, "trip", buf_ct)) {
		ret = parse_threshold(tz, buf, buf_ct);
		if (ret)
			goto parse_exit;
	}
	strlcpy(buf, buf_ptr, (buf_ct + 1));
	curr_buf = next_buf = buf;
	strsep(&next_buf, "\n");
	while (curr_buf && curr_buf < buf_end) {
		if (next_buf)
			count = next_buf - curr_buf;
		else
			count = buf_end - curr_buf + 1;
		if (strnstr(curr_buf, "passive_delay", count))
			ret = parse_delay(tz, curr_buf, count, false);
		else if (strnstr(curr_buf, "polling_delay", count))
			ret = parse_delay(tz, curr_buf, count, true);

		if (ret)
			goto parse_exit;

		curr_buf = next_buf;
		if (next_buf < buf_end)
			strsep(&next_buf, "\n");
		else
			next_buf = NULL;
	}
parse_exit:
	mutex_unlock(&tz->lock);
	if (mode == THERMAL_DEVICE_ENABLED && tz->ops->set_mode)
		tz->ops->set_mode(tz, THERMAL_DEVICE_ENABLED);
	kfree(buf);
	return ret ? ret : buf_ct;
}

static ssize_t thermal_dbgfs_config_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct thermal_zone_device *tz = NULL;
	char *sensor_buf = NULL, sensor_name[THERMAL_NAME_LENGTH] = "", *buf;
	int ret = -EINVAL;

	buf = kzalloc(sizeof(char) * (count + 1), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto config_exit;
	}
	sensor_buf = strnstr(buf, "sensor", count);
	if (sensor_buf) {
		if (sscanf(sensor_buf, "sensor %20[^\n\t ]",
				sensor_name) != 1) {
			pr_err("sensor name not found\n");
			ret = -EINVAL;
			goto config_exit;
		}
		tz = thermal_zone_get_zone_by_name((const char *)sensor_name);
		if (IS_ERR(tz)) {
			ret = PTR_ERR(tz);
			pr_err("No thermal zone for sensor:%s. err:%d\n",
					sensor_name, ret);
			goto config_exit;
		}
		ret = parse_config(tz, buf, count);
	}

config_exit:
	kfree(buf);
	return ret;
}

static const struct file_operations thermal_dbgfs_config_fops = {
	.write = thermal_dbgfs_config_write,
};

int thermal_debug_init(void)
{
	int ret = 0;

	thermal_debugfs_parent = debugfs_create_dir("thermal", NULL);
	if (IS_ERR_OR_NULL(thermal_debugfs_parent)) {
		ret = PTR_ERR(thermal_debugfs_parent);
		pr_err("Error creating thermal debugfs directory. err:%d\n",
				ret);
		return ret;
	}
	thermal_debugfs_config = debugfs_create_file("config", 0200,
					thermal_debugfs_parent, NULL,
					&thermal_dbgfs_config_fops);
	if (IS_ERR_OR_NULL(thermal_debugfs_config)) {
		ret = PTR_ERR(thermal_debugfs_config);
		pr_err("Error creating thermal config debugfs. err:%d\n",
				ret);
		return ret;
	}

	return ret;
}

void thermal_debug_exit(void)
{
	debugfs_remove_recursive(thermal_debugfs_parent);
}
