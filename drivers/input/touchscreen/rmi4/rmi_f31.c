/*
 * Copyright (c) 2012 Synaptics Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/rmi.h>
#include <linux/slab.h>

#define MAX_NUMBER_OF_LEDS 12
#define FUNCTION_NUMBER 0x31

struct f31_led_query {
	union {
		struct {
			u8 has_brightness:1;
		};
	u8 f31_query0;
	};
	union {
		struct {
			u8 number_of_leds:4;
		};
		u8 f31_query1;
	};
};

struct f31_led_ctrl_0 {
	u8 brightness;
};

struct f31_led_ctrl {
	struct f31_led_ctrl_0 *brightness_adj;
};

struct f31_data {
	struct f31_led_query led_query;
	struct f31_led_ctrl led_ctrl;
	unsigned char led_count;
	unsigned char selected_led;
};

static ssize_t rmi_f31_led_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rmi_function_dev *fn_dev;
	struct f31_data *data;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_count);
}

static ssize_t rmi_f31_has_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rmi_function_dev *fn_dev;
	struct f31_data *data;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
					data->led_query.has_brightness);
}

static ssize_t rmi_f31_selected_led_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_dev *fn_dev;
	struct f31_data *data;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
				   data->selected_led);
}

static ssize_t rmi_f31_selected_led_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct rmi_function_dev *fn_dev;
	struct f31_data *data;
	unsigned int new_value;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	if (sscanf(buf, "%u", &new_value) != 1) {
		dev_err(dev,
				"%s: Error - selected_led_store has an "
				"invalid len.\n",
				__func__);
		return -EINVAL;
	}

	if (new_value > data->led_count - 1) {
		dev_err(dev, "%s: Error - actve_led_store has an "
				"invalid value %d.\n",
				__func__, new_value);
		return -EINVAL;
	}

	data->selected_led = new_value;

	return count;
}

static ssize_t rmi_f31_selected_brightness_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_dev *fn_dev;
	struct f31_data *data;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
		data->led_ctrl.brightness_adj[data->selected_led].brightness);
}

static ssize_t rmi_f31_selected_brightness_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct rmi_function_dev *fn_dev;
	struct f31_data *data;
	u8 write_addr;
	unsigned int new_value;
	int result;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	if (sscanf(buf, "%u", &new_value) != 1) {
		dev_err(dev, "Selected_brightness_store has an invalid len.\n");
		return -EINVAL;
	}

	if (new_value > 255) {
		dev_err(dev, "Actve_led_store has an invalid value %d.\n",
			new_value);
		return -EINVAL;
	}

	data->led_ctrl.brightness_adj[data->selected_led].brightness = new_value;

	write_addr = fn_dev->fd.control_base_addr +
			(data->selected_led * sizeof(struct f31_led_ctrl_0));

	result = rmi_write_block(fn_dev->rmi_dev, write_addr,
				&(data->led_ctrl.brightness_adj[data->selected_led].brightness),
				1);
	if (result < 0) {
		dev_err(dev, "%s: failed to write brightness. error = %d.",
			__func__, result);
		return result;
	}

	return count;
}
static struct device_attribute attrs[] = {
	__ATTR(led_count, RMI_RO_ATTR,
		   rmi_f31_led_count_show,
		   NULL),
	__ATTR(has_brightness, RMI_RO_ATTR,
		   rmi_f31_has_brightness_show,
		   NULL),
};

static struct device_attribute brightness_attrs[] = {
	__ATTR(selected_led, RMI_RW_ATTR,
		   rmi_f31_selected_led_show,
		   rmi_f31_selected_led_store),
	__ATTR(selected_brightness, RMI_RW_ATTR,
		   rmi_f31_selected_brightness_show,
		   rmi_f31_selected_brightness_store),
};
static int rmi_f31_alloc_memory(struct rmi_function_dev *fn_dev)
{
	struct f31_data *f31;
	int rc;

	f31 = kzalloc(sizeof(struct f31_data), GFP_KERNEL);
	if (!f31) {
		dev_err(&fn_dev->dev, "Failed to allocate function data.\n");
		return -ENOMEM;
	}
	fn_dev->data = f31;

	rc = rmi_read_block(fn_dev->rmi_dev, fn_dev->fd.query_base_addr,
			(u8 *)&f31->led_query, sizeof(struct f31_led_query));
	if (rc < 0) {
		dev_err(&fn_dev->dev, "Failed to read query register.\n");
		return rc;
	}

	dev_err(&fn_dev->dev,
			"F31 READ led_count = %u, has brightness = %u",
			f31->led_query.f31_query1,
			f31->led_query.has_brightness);

	f31->led_count = f31->led_query.f31_query1;

	if (f31->led_query.has_brightness) {
		f31->led_ctrl.brightness_adj =
			kcalloc(f31->led_count,
				sizeof(struct f31_led_ctrl_0), GFP_KERNEL);
		if (!f31->led_ctrl.brightness_adj) {
			dev_err(&fn_dev->dev, "Failed to allocate brightness_adj.\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static void rmi_f31_free_memory(struct rmi_function_dev *fn_dev)
{
	struct f31_data *f31 = fn_dev->data;

	if (f31) {
		kfree(f31->led_ctrl.brightness_adj);
		kfree(f31);
		fn_dev->data = NULL;
	}
}

static int rmi_f31_initialize(struct rmi_function_dev *fn_dev)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f31_data *f31 = fn_dev->data;
	u8 read_addr;
	int i;
	int rc;

	dev_info(&fn_dev->dev, "Intializing F31 values.");

	read_addr = fn_dev->fd.control_base_addr;

	f31->selected_led = 0;

	if (f31->led_query.has_brightness) {
		for (i = 0; i < f31->led_count; i++) {
			rc = rmi_read_block(rmi_dev, read_addr,
				&(f31->led_ctrl.brightness_adj[i].brightness),
				sizeof(struct f31_led_ctrl_0));
			if (rc < 0) {
				dev_err(&rmi_dev->dev, "Failed to read F31 ctrl, code %d.\n",
					rc);
				return rc;
			}

			read_addr = read_addr + sizeof(struct f31_led_ctrl_0);
		}
	}

	return 0;
}

static int rmi_f31_create_sysfs(struct rmi_function_dev *fn_dev)
{
	int attr_count = 0;
	int brightness_attr_count = 0;
	int rc;
	struct f31_data *f31 = fn_dev->data;

	if (f31) {
		dev_dbg(&fn_dev->dev, "Creating sysfs files.\n");
		/* Set up sysfs device attributes. */
		for (attr_count = 0; attr_count < ARRAY_SIZE(attrs);
				attr_count++) {
			if (sysfs_create_file
				(&fn_dev->dev.kobj, &attrs[attr_count].attr) < 0) {
				dev_err(&fn_dev->dev, "Failed to create sysfs file for %s.",
						attrs[attr_count].attr.name);
				rc = -ENODEV;
				goto err_remove_sysfs;
			}
		}

		if (f31->led_query.f31_query0) {
			for (brightness_attr_count = 0;
				 brightness_attr_count < ARRAY_SIZE(brightness_attrs);
				 brightness_attr_count++) {
				if (sysfs_create_file
					(&fn_dev->dev.kobj, &brightness_attrs[brightness_attr_count].attr) < 0) {
					dev_err(&fn_dev->dev, "Failed to create sysfs file for %s ",
						brightness_attrs[brightness_attr_count].attr.name);
					rc = ENODEV;
					goto err_remove_sysfs;
				}
			}
		}
	}

	return 0;

err_remove_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(&fn_dev->dev.kobj, &attrs[attr_count].attr);

	for (brightness_attr_count--; brightness_attr_count >= 0;
			brightness_attr_count--)
		sysfs_remove_file(&fn_dev->dev.kobj,
			&brightness_attrs[brightness_attr_count].attr);
	return rc;

}

static int rmi_f31_config(struct rmi_function_dev *fn_dev)
{
	u8 write_addr;
	int i;
	int rc;
	struct f31_data *f31 = fn_dev->data;

	write_addr = fn_dev->fd.control_base_addr;

	if (f31->led_query.has_brightness) {
		for (i = 0; i < f31->led_count; i++) {
			rc = rmi_write_block(fn_dev->rmi_dev, write_addr,
				&(f31->led_ctrl.brightness_adj[i].brightness),
				1);
			if (rc < 0) {
				dev_err(&fn_dev->dev, "Failed to read F31 ctrl, code %d\n",
					rc);
				return rc;
			}

			write_addr += sizeof(struct f31_led_ctrl_0);
		}
	}

	return 0;
}

static int rmi_f31_probe(struct rmi_function_dev *fn_dev)
{
	int rc;

	rc = rmi_f31_alloc_memory(fn_dev);
	if (rc < 0)
		goto err_free_data;

	rc = rmi_f31_initialize(fn_dev);
	if (rc < 0)
		goto err_free_data;

	rc = rmi_f31_create_sysfs(fn_dev);
	if (rc < 0)
		goto err_free_data;

	return 0;

err_free_data:
	rmi_f31_free_memory(fn_dev);

	return rc;
}

static int rmi_f31_remove(struct rmi_function_dev *fn_dev)
{
	struct f31_data *f31 = fn_dev->data;
	int attr_count;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(&fn_dev->dev.kobj, &attrs[attr_count].attr);

	if (f31->led_query.f31_query0) {
		for (attr_count = 0; attr_count < ARRAY_SIZE(brightness_attrs);
			 attr_count++)
			sysfs_remove_file(&fn_dev->dev.kobj,
				&brightness_attrs[attr_count].attr);
	}

	rmi_f31_free_memory(fn_dev);

	return 0;
}

static struct rmi_function_driver function_driver = {
	.driver = {
		.name = "rmi_f31",
	},
	.func = FUNCTION_NUMBER,
	.config = rmi_f31_config,
	.probe = rmi_f31_probe,
	.remove = rmi_f31_remove,
};

module_rmi_function_driver(function_driver);

MODULE_AUTHOR("Mario Lari <mlari@synaptics.com>");
MODULE_DESCRIPTION("RMI F31 module");
MODULE_LICENSE("GPL");
