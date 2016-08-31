/*
 * Copyright (c) 2012 Synaptics Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#define FUNCTION_DATA f19_data
#define FNUM 19

#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/input.h>
#include <linux/slab.h>
#include "rmi_driver.h"

#define QUERY_BASE_INDEX 1
#define MAX_LEN 256
#define MAX_BUFFER_LEN 80

#define SENSOR_MAP_MIN			0
#define SENSOR_MAP_MAX			127
#define SENSITIVITY_ADJ_MIN		0
#define SENSITIVITY_ADJ_MAX		31
#define HYSTERESIS_THRESHOLD_MIN	0
#define HYSTERESIS_THRESHOLD_MAX	15
#define FUNCTION_NUMBER			0x19

union f19_0d_query {
		struct {
			u8 configurable:1;
			u8 has_sensitivity_adjust:1;
			u8 has_hysteresis_threshold:1;
		u8 reserved_1:5;

		u8 button_count:5;
		u8 reserved_2:3;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};

union f19_0d_control_0 {
		struct {
			u8 button_usage:2;
			u8 filter_mode:2;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};
/* rewrite control regs */
struct f19_0d_control_1n {
	u8 int_enabled_button;
} __attribute__((__packed__));

struct f19_0d_control_1 {
	struct f19_0d_control_1n *regs;
	u16 address;
	u8 length;
} __attribute__((__packed__));

struct f19_0d_control_2n {
	u8 single_button;
} __attribute__((__packed__));

struct f19_0d_control_2 {
	struct f19_0d_control_2n *regs;
	u16 address;
	u8 length;
} __attribute__((__packed__));

struct f19_0d_control_3n {
	u8 sensor_map_button:7;
} __attribute__((__packed__));

struct f19_0d_control_3 {
	struct f19_0d_control_3n *regs;
	u16 address;
	u8 length;
} __attribute__((__packed__));

struct f19_0d_control_4n {
	u8 sensitivity_button:7;
} __attribute__((__packed__));

struct f19_0d_control_4 {
	struct f19_0d_control_4n *regs;
	u16 address;
	u8 length;
} __attribute__((__packed__));

union f19_0d_control_5 {
	struct {
		u8 sensitivity_adj:5;
	};
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f19_0d_control_6 {
	struct {
		u8 hysteresis_threshold:4;
	};
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

struct f19_0d_control {
	union f19_0d_control_0 *reg_0;
	struct f19_0d_control_1 *reg_1;
	struct f19_0d_control_2 *reg_2;
	struct f19_0d_control_3 *reg_3;
	struct f19_0d_control_4 *reg_4;
	union f19_0d_control_5 *reg_5;
	union f19_0d_control_6 *reg_6;
};

/* data specific to fn $19 that needs to be kept around */
struct f19_data {
	struct f19_0d_control control;
	union f19_0d_query query;
	u8 button_rezero;
	u8 button_count;
	u8 button_bitmask_size;
	u8 *button_data_buffer;
	u16 *button_map;
	char input_phys[MAX_LEN];
	struct input_dev *input;
	struct mutex control_mutex;
	struct mutex data_mutex;
};

/* Query sysfs files */
show_union_struct_prototype(configurable)
show_union_struct_prototype(has_sensitivity_adjust)
show_union_struct_prototype(has_hysteresis_threshold)
show_union_struct_prototype(button_count)

static struct attribute *attrs[] = {
	attrify(configurable),
	attrify(has_sensitivity_adjust),
	attrify(has_hysteresis_threshold),
	attrify(button_count),
	NULL
};

static struct attribute_group attrs_query = GROUP(attrs);
/* Control sysfs files */
show_store_union_struct_prototype(button_usage)
show_store_union_struct_prototype(filter_mode)
show_store_union_struct_prototype(int_enabled_button)
show_store_union_struct_prototype(single_button)
show_store_union_struct_prototype(sensitivity_button)
show_store_union_struct_prototype(sensitivity_adj)
show_store_union_struct_prototype(hysteresis_threshold)


static struct attribute *attrsCtrl[] = {
	attrify(button_usage),
	attrify(filter_mode),
	attrify(int_enabled_button),
	attrify(single_button),
	attrify(sensitivity_button),
	NULL
};
static struct attribute_group attrs_control = GROUP(attrsCtrl);

static ssize_t rmi_f19_sensor_map_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct rmi_function_dev *fn_dev;
	struct f19_data *data;
	int reg_length;
	int result, size = 0;
	char *temp;
	int i;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	/* Read current regtype values */
	reg_length = data->control.reg_3->length;
	result = rmi_read_block(fn_dev->rmi_dev, data->control.reg_3->address,
			 data->control.reg_3->regs, reg_length * sizeof(u8));
	if (result < 0) {
		dev_dbg(dev, "%s : Could not read regtype at 0x%x\n"
					"Data may be outdated.", __func__,
					data->control.reg_3->address);
	}
	temp = buf;
	for (i = 0; i < reg_length; i++) {
		result = snprintf(temp, PAGE_SIZE - size, "%u ",
				data->control.reg_3->regs[i].sensor_map_button);
		if (result < 0) {
			dev_err(dev, "%s : Could not write output.", __func__);
			return result;
			}
		size += result;
		temp += result;
		}
	result = snprintf(temp, PAGE_SIZE - size, "\n");
	if (result < 0) {
		dev_err(dev, "%s : Could not write output.", __func__);
		return result;
	}
	return size + result;
}

static ssize_t rmi_f19_sensor_map_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct rmi_function_dev *fn_dev;
	struct f19_data *data;
	int reg_length;
	int result;
	const char *temp;
	int i;
	unsigned int newval;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	/* Read current regtype values */
	reg_length = data->control.reg_3->length;
	result = rmi_read_block(fn_dev->rmi_dev, data->control.reg_3->address,
			 data->control.reg_3->regs, reg_length * sizeof(u8));

	if (result < 0) {
		dev_dbg(dev, "%s: Could not read regtype at %#06x. "
					"Data may be outdated.", __func__,
					data->control.reg_3->address);
	}

	/* parse input */
	temp = buf;
	for (i = 0; i < reg_length; i++) {
		if (sscanf(temp, "%u", &newval) == 1) {
			data->control.reg_3->regs[i].sensor_map_button = newval;
		} else {
			/* If we don't read a value for each position, abort,
			 * restore previous values locally by rereading */
			result = rmi_read_block(fn_dev->rmi_dev,
					data->control.reg_3->address,
					data->control.reg_3->regs,
					reg_length * sizeof(u8));
			if (result < 0) {
				dev_dbg(dev, "%s: Couldn't read regtype at "
					"%#06x. Local data may be inaccurate",
					__func__,
					data->control.reg_3->address);
			}
			return -EINVAL;
		}
		/* move to next number */
		while (*temp != 0) {
			temp++;
			if (isspace(*(temp - 1)) && !isspace(*temp))
				break;
	}
		}
	result = rmi_write_block(fn_dev->rmi_dev, data->control.reg_3->address,
			 data->control.reg_3->regs,
			reg_length * sizeof(u8));
	if (result < 0) {
		dev_dbg(dev, "%s: Could not write new values to %#06x\n",
				__func__, data->control.reg_3->address);
		return result;
	}
	return count;
}
static struct device_attribute sensor_map_attr = __ATTR(sensor_map_button,
			RMI_RW_ATTR, rmi_f19_sensor_map_show,
			rmi_f19_sensor_map_store);

static struct device_attribute sensor_map_ro_attr =  __ATTR(sensor_map_button,
			RMI_RO_ATTR, rmi_f19_sensor_map_show,
			 NULL);

static ssize_t f19_rezero_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	unsigned int new_value;
	int len;
	struct f19_data *f19;
	struct rmi_function_dev *fn_dev = to_rmi_function_dev(dev);

	f19 = fn_dev->data;
	len = sscanf(buf, "%u", &new_value);
	if (new_value != 0 && new_value != 1) {
		dev_err(dev, "%s: Error - rezero is not a valid value 0x%x.\n",
			__func__, new_value);
		return -EINVAL;
		}
	f19->button_rezero = new_value & 1;
	len = rmi_write(fn_dev->rmi_dev, fn_dev->fd.command_base_addr,
		f19->button_rezero);

	if (len < 0) {
		dev_err(dev, "%s : Could not write rezero to 0x%x\n",
				__func__, fn_dev->fd.command_base_addr);
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(rezero, RMI_WO_ATTR,
		NULL,
		f19_rezero_store);

static struct attribute *attrsCommand[] = {
	attrify(rezero),
	NULL
};
static struct attribute_group attrs_command = GROUP(attrsCommand);

int rmi_f19_read_control_parameters(struct rmi_device *rmi_dev,
	struct f19_data *f19)
{
	int retval = 0;
	union f19_0d_query *query = &f19->query;
	struct f19_0d_control *control = &f19->control;


	retval = rmi_read_block(rmi_dev, control->reg_0->address,
			control->reg_0->regs, sizeof(control->reg_0->regs));
	if (retval < 0) {
		dev_err(&rmi_dev->dev, "Could not read control reg0 to %#06x.\n",
				control->reg_0->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_1->address,
			control->reg_1->regs, f19->button_bitmask_size);
	if (retval < 0) {
		dev_err(&rmi_dev->dev, "Could not read control reg1 to %#06x.\n",
			 control->reg_1->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_2->address,
			control->reg_2->regs, f19->button_bitmask_size);
	if (retval < 0) {
		dev_err(&rmi_dev->dev, "Could not read control reg2 to %#06x.\n",
			 control->reg_2->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_3->address,
			control->reg_3->regs, f19->button_count);
	if (retval < 0) {
		dev_err(&rmi_dev->dev, "Could not read control reg3 to %#06x.\n",
			 control->reg_3->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_4->address,
			control->reg_4->regs, f19->button_count);
	if (retval < 0) {
		dev_err(&rmi_dev->dev, "Could not read control reg3 to %#06x.\n",
			 control->reg_4->address);
		return retval;
	}

	if (query->has_sensitivity_adjust) {
		retval = rmi_read_block(rmi_dev, control->reg_5->address,
				control->reg_5->regs,
				sizeof(control->reg_5->regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg5 to %#06x.\n",
				control->reg_5->address);
			return retval;
		}
	}
	if (query->has_hysteresis_threshold) {
		retval = rmi_read_block(rmi_dev, control->reg_6->address,
				control->reg_6->regs,
				sizeof(control->reg_6->regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg6 to %#06x.\n",
				control->reg_6->address);
			return retval;
		}
	}
	return 0;
}

static int rmi_f19_alloc_memory(struct rmi_function_dev *fn_dev)
{
	struct f19_data *f19;
	int rc;
	u16 ctrl_base_addr;

	/* allow memory for fn19 data */
	f19 = devm_kzalloc(&fn_dev->dev, sizeof(struct f19_data), GFP_KERNEL);
	if (!f19) {
		dev_err(&fn_dev->dev, "Failed to allocate function data.\n");
		return -ENOMEM;
	}
	fn_dev->data = f19;
	rc = rmi_read_block(fn_dev->rmi_dev, fn_dev->fd.query_base_addr,
				&f19->query,
				sizeof(f19->query.regs));
	if (rc < 0) {
		dev_err(&fn_dev->dev, "Failed to read query register.\n");
		return rc;
	}
	f19->query.address = fn_dev->fd.query_base_addr;

	f19->button_bitmask_size = sizeof(u8)*(f19->query.button_count + 7)/8;
	f19->button_data_buffer = devm_kzalloc(&fn_dev->dev,
				f19->button_bitmask_size, GFP_KERNEL);
	if (!f19->button_data_buffer) {
		dev_err(&fn_dev->dev, "Failed to allocate button data buffer.\n");
		return -ENOMEM;
	}

	f19->button_map = devm_kzalloc(&fn_dev->dev, f19->query.button_count,
				GFP_KERNEL);
	if (!f19->button_map) {
		dev_err(&fn_dev->dev, "Failed to allocate button map.\n");
		return -ENOMEM;
	}

	/* allocate memory for control reg */
	/* reg 0 */
	ctrl_base_addr = fn_dev->fd.control_base_addr;
	f19->control.reg_0 = devm_kzalloc(&fn_dev->dev,
				sizeof(f19->control.reg_0->regs), GFP_KERNEL);
	if (!f19->control.reg_0) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_0 control registers.");
		return -ENOMEM;
	}
	f19->control.reg_0->address = ctrl_base_addr;
	ctrl_base_addr += sizeof(f19->control.reg_0->regs);

	/* reg 1 */
	f19->control.reg_1 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f19_0d_control_1), GFP_KERNEL);
	if (!f19->control.reg_1) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_1 control registers.");
		return -ENOMEM;
	}
	f19->control.reg_1->regs = devm_kzalloc(&fn_dev->dev,
					f19->button_bitmask_size, GFP_KERNEL);
	if (!f19->control.reg_1->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_1->regs control registers.");
		return -ENOMEM;
	}
	f19->control.reg_1->address = ctrl_base_addr;
	f19->control.reg_1->length = f19->button_bitmask_size;
	ctrl_base_addr += f19->button_bitmask_size;

	/* reg 2 */
	f19->control.reg_2 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f19_0d_control_2), GFP_KERNEL);
	if (!f19->control.reg_2) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_2 control registers.");
		return -ENOMEM;
	}
	f19->control.reg_2->regs = devm_kzalloc(&fn_dev->dev,
					f19->button_bitmask_size, GFP_KERNEL);
	if (!f19->control.reg_2->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_2->regs control registers.");
		return -ENOMEM;
	}
	f19->control.reg_2->address = ctrl_base_addr;
	f19->control.reg_2->length = f19->button_bitmask_size;
	ctrl_base_addr += f19->button_bitmask_size;

	/* reg 3 */
	f19->control.reg_3 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f19_0d_control_3), GFP_KERNEL);
	if (!f19->control.reg_3) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_3 control registers.");
		return -ENOMEM;
	}
	f19->control.reg_3->regs = devm_kzalloc(&fn_dev->dev,
				f19->query.button_count, GFP_KERNEL);
	if (!f19->control.reg_3->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_3->regs control registers.");
		return -ENOMEM;
	}
	f19->control.reg_3->address = ctrl_base_addr;
	f19->control.reg_3->length = f19->query.button_count;
	ctrl_base_addr += f19->query.button_count;

	/* reg 4 */
	f19->control.reg_4 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f19_0d_control_4), GFP_KERNEL);
	if (!f19->control.reg_4) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_3 control registers.");
		return -ENOMEM;
	}
	f19->control.reg_4->regs = devm_kzalloc(&fn_dev->dev,
				f19->query.button_count, GFP_KERNEL);
	if (!f19->control.reg_4->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate reg_3->regs control registers.");
		return -ENOMEM;
	}
	f19->control.reg_4->address = ctrl_base_addr;
	f19->control.reg_4->length = f19->query.button_count;
	ctrl_base_addr += f19->query.button_count;

	/* reg 5 */
	if (f19->query.has_sensitivity_adjust) {
		f19->control.reg_5 = devm_kzalloc(&fn_dev->dev,
			sizeof(f19->control.reg_5->regs), GFP_KERNEL);
		if (!f19->control.reg_5) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_5 control registers.");
			return -ENOMEM;
		}
		f19->control.reg_5->address = ctrl_base_addr;
		ctrl_base_addr += sizeof(f19->control.reg_5->regs);
	}
	/* reg 6 */
	if (f19->query.has_hysteresis_threshold) {
		f19->control.reg_6 =
			devm_kzalloc(&fn_dev->dev,
				sizeof(f19->control.reg_6->regs), GFP_KERNEL);
		if (!f19->control.reg_6) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_6 control registers.");
			return -ENOMEM;
		}
		f19->control.reg_6->address = ctrl_base_addr;
	}
	return 0;
}

static void rmi_f19_free_memory(struct rmi_function_dev *fn_dev)
{
	union f19_0d_query *query;
	struct f19_data *f19 = fn_dev->data;

	query = &f19->query;
	sysfs_remove_group(&fn_dev->dev.kobj, &attrs_query);
	sysfs_remove_group(&fn_dev->dev.kobj, &attrs_control);
	if (query->has_sensitivity_adjust)
		sysfs_remove_file(&fn_dev->dev.kobj, attrify(sensitivity_adj));

	if (query->has_hysteresis_threshold)
		sysfs_remove_file(&fn_dev->dev.kobj,
				attrify(hysteresis_threshold));
	sysfs_remove_group(&fn_dev->dev.kobj, &attrs_command);
	if (query->configurable)
		sysfs_remove_file(&fn_dev->dev.kobj, &sensor_map_attr.attr);
	else
		sysfs_remove_file(&fn_dev->dev.kobj, &sensor_map_ro_attr.attr);

}

static int rmi_f19_initialize(struct rmi_function_dev *fn_dev)
{
	int i;
	int rc;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_device_platform_data *pdata;
	struct f19_data *f19 = fn_dev->data;

	/* initial all default values for f19 data here */
	rc = rmi_read(rmi_dev, fn_dev->fd.command_base_addr,
		&f19->button_rezero);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "Failed to read command register.\n");
		return rc;
	}
	f19->button_rezero = f19->button_rezero & 1;

	pdata = to_rmi_platform_data(rmi_dev);
	if (pdata) {
		if (!pdata->f19_button_map)
			dev_warn(&fn_dev->dev, "F19 button_map is NULL");
		else if (!pdata->f19_button_map->map)
			dev_warn(&fn_dev->dev,
				 "Platformdata button map is missing!\n");
		else {
			if (pdata->f19_button_map->nbuttons !=
						f19->query.button_count)
				dev_warn(&fn_dev->dev, "Platformdata button map size (%d) != number of buttons on device (%d) - ignored.\n",
					pdata->f19_button_map->nbuttons,
					f19->query.button_count);
			f19->button_count = min(pdata->f19_button_map->nbuttons,
					 (u8) f19->query.button_count);
			for (i = 0; i < f19->button_count; i++)
				f19->button_map[i] =
					pdata->f19_button_map->map[i];
		}
	}
	rc = rmi_f19_read_control_parameters(rmi_dev, f19);
	if (rc < 0) {
		dev_err(&fn_dev->dev,
			"Failed to initialize F19 control params.\n");
		return rc;
	}

	mutex_init(&f19->control_mutex);
	mutex_init(&f19->data_mutex);
	return 0;
}

static int rmi_f19_register_device(struct rmi_function_dev *fn_dev)
{
	int i;
	int rc;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f19_data *f19 = fn_dev->data;
	struct rmi_driver *driver = fn_dev->rmi_dev->driver;
	struct input_dev *input_dev = input_allocate_device();

	if (!input_dev) {
		dev_err(&fn_dev->dev, "Failed to allocate input device.\n");
		return -ENOMEM;
	}

	f19->input = input_dev;

	if (driver->set_input_params) {
		rc = driver->set_input_params(rmi_dev, input_dev);
		if (rc < 0) {
			dev_err(&fn_dev->dev, "%s: Error in setting input device.\n",
			__func__);
			goto error_free_device;
		}
	}
	sprintf(f19->input_phys, "%s/input0", dev_name(&fn_dev->dev));
	input_dev->phys = f19->input_phys;
	input_dev->dev.parent = &rmi_dev->dev;
	input_set_drvdata(input_dev, f19);

	/* Set up any input events. */
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	/* manage button map using input subsystem */
	input_dev->keycode = f19->button_map;
	input_dev->keycodesize = sizeof(f19->button_map);
	input_dev->keycodemax = f19->button_count;

	/* set bits for each button... */
	for (i = 0; i < f19->button_count; i++)
		set_bit(f19->button_map[i], input_dev->keybit);
	rc = input_register_device(input_dev);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "Failed to register input device.\n");
		goto error_free_device;
	}

	return 0;

error_free_device:
	input_free_device(input_dev);

	return rc;
}

static int rmi_f19_create_sysfs(struct rmi_function_dev *fn_dev)
{
	struct f19_data *f19 = fn_dev->data;
	union f19_0d_query *query = &f19->query;

	dev_dbg(&fn_dev->dev, "Creating sysfs files.\n");

	/* Set up sysfs device attributes. */
	if (sysfs_create_group(&fn_dev->dev.kobj, &attrs_query) < 0) {
		dev_err(&fn_dev->dev, "Failed to create query sysfs files.");
		return -ENODEV;
	}
	if (sysfs_create_group(&fn_dev->dev.kobj, &attrs_control) < 0) {
		dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
		return -ENODEV;
	}
	if (query->has_sensitivity_adjust) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(sensitivity_adj)) < 0) {
			dev_err(&fn_dev->dev,
				"Failed to create control sysfs files.");
			return -ENODEV;
		}
	}
	if (query->has_hysteresis_threshold) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(hysteresis_threshold)) < 0) {
			dev_err(&fn_dev->dev,
				"Failed to create control sysfs files.");
			return -ENODEV;
		}
	}
	if (sysfs_create_group(&fn_dev->dev.kobj, &attrs_command) < 0) {
		dev_err(&fn_dev->dev, "Failed to create command sysfs files.");
		return -ENODEV;
	}
	if (query->configurable) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
					&sensor_map_attr.attr) < 0) {
			dev_err(&fn_dev->dev,
				"Failed to create control sysfs files.");
			return -ENODEV;
	}
	} else {
		if (sysfs_create_file(&fn_dev->dev.kobj,
					&sensor_map_ro_attr.attr) < 0) {
			dev_err(&fn_dev->dev,
				"Failed to create control sysfs files.");
			return -ENODEV;
		}
	}
	return 0;
}

static int rmi_f19_config(struct rmi_function_dev *fn_dev)
{
	struct f19_data *f19 = fn_dev->data;
	int retval = 0;
	union f19_0d_query *query = &f19->query;
	struct f19_0d_control *control = &f19->control;

	retval = rmi_write_block(fn_dev->rmi_dev, control->reg_0->address,
			control->reg_0->regs,
			sizeof(u8));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not write control reg0 to %#06x.\n",
				control->reg_0->address);
		return retval;
	}

	retval = rmi_write_block(fn_dev->rmi_dev, control->reg_1->address,
			control->reg_1->regs, f19->button_bitmask_size);
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not write control reg1 to %#06x.\n",
			 control->reg_1->address);
		return retval;
	}

	retval = rmi_write_block(fn_dev->rmi_dev, control->reg_2->address,
			control->reg_2->regs, f19->button_bitmask_size);
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not write control reg2 to %#06x.\n",
			 control->reg_2->address);
		return retval;
	}

	retval = rmi_write_block(fn_dev->rmi_dev, control->reg_3->address,
			control->reg_3->regs, query->button_count);
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not write control reg3 to %#06x.\n",
			 control->reg_3->address);
		return retval;
	}

	retval = rmi_write_block(fn_dev->rmi_dev, control->reg_4->address,
			control->reg_4->regs, query->button_count);
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not write control reg3 to %#06x.\n",
			 control->reg_4->address);
		return retval;
		}

	if (query->has_sensitivity_adjust) {
		retval = rmi_write_block(fn_dev->rmi_dev,
				control->reg_5->address, control->reg_5->regs,
				sizeof(control->reg_5->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev,
				"Could not write control reg5 to %#06x.\n",
				control->reg_5->address);
			return retval;
		}
	}
	if (query->has_hysteresis_threshold) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					control->reg_6->address,
					control->reg_6->regs,
					sizeof(control->reg_6->regs));
	if (retval < 0) {
			dev_err(&fn_dev->dev,
				"Could not write control reg6 to %#06x.\n",
				control->reg_6->address);
			return retval;
	}
	}
	return 0;
}

static int rmi_f19_probe(struct rmi_function_dev *fn_dev)
{
	int rc = rmi_f19_alloc_memory(fn_dev);
	if (rc < 0)
		goto err_free_data;

	rc = rmi_f19_initialize(fn_dev);
	if (rc < 0)
		goto err_free_data;

	rc = rmi_f19_register_device(fn_dev);
	if (rc < 0)
		goto err_free_data;

	rc = rmi_f19_create_sysfs(fn_dev);
	if (rc < 0)
		goto err_free_data;

	return 0;

err_free_data:
	rmi_f19_free_memory(fn_dev);
	return rc;
}

static int rmi_f19_reset(struct rmi_function_dev *fn_dev)
{
	/* we do nnothing here */
	return 0;
}


static int rmi_f19_remove(struct rmi_function_dev *fn_dev)
{
	struct f19_data *f19 = fn_dev->data;

	input_unregister_device(f19->input);
	rmi_f19_free_memory(fn_dev);

	return 0;
}

static int rmi_f19_attention(struct rmi_function_dev *fn_dev,
						unsigned long *irq_bits)
{
	int error;
	int button;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f19_data *f19 = fn_dev->data;
	u16 data_base_addr = fn_dev->fd.data_base_addr;

	/* Read the button data. */
	error = rmi_read_block(rmi_dev, data_base_addr, f19->button_data_buffer,
			f19->button_bitmask_size);
	if (error < 0) {
		dev_err(&fn_dev->dev, "%s: Failed to read button data registers.\n",
			__func__);
		return error;
	}

	/* Generate events for buttons. */
	for (button = 0; button < f19->button_count; button++) {
		int button_reg;
		int button_shift;
		bool button_status;

		/* determine which data byte the button status is in */
		button_reg = button / 8;
		/* bit shift to get button's status */
		button_shift = button % 8;
		button_status =
		    ((f19->button_data_buffer[button_reg] >> button_shift)
			& 0x01) != 0;
		/* Generate an event here. */
		input_report_key(f19->input, f19->button_map[button],
				 button_status);
	}

	input_sync(f19->input); /* sync after groups of events */
	return 0;
}

static struct rmi_function_driver function_driver = {
	.driver = {
		.name = "rmi_f19",
	},
	.func = FUNCTION_NUMBER,
	.probe = rmi_f19_probe,
	.remove = rmi_f19_remove,
	.config = rmi_f19_config,
	.reset = rmi_f19_reset,
	.attention = rmi_f19_attention,
};


/* sysfs functions */
/* Query */
simple_show_union_struct_unsigned(query, configurable)
simple_show_union_struct_unsigned(query, has_sensitivity_adjust)
simple_show_union_struct_unsigned(query, has_hysteresis_threshold)
simple_show_union_struct_unsigned(query, button_count)

/* Control */
show_store_union_struct_unsigned(control, reg_0, button_usage)
show_store_union_struct_unsigned(control, reg_0, filter_mode)
show_store_union_struct_unsigned(control, reg_5, sensitivity_adj)
show_store_union_struct_unsigned(control, reg_6, hysteresis_threshold)

/* repeated register functions */
show_store_repeated_union_struct_unsigned(control, reg_1, int_enabled_button)
show_store_repeated_union_struct_unsigned(control, reg_2, single_button)
show_store_repeated_union_struct_unsigned(control, reg_4, sensitivity_button)


module_rmi_function_driver(function_driver);

MODULE_AUTHOR("Vivian Ly <vly@synaptics.com>");
MODULE_DESCRIPTION("RMI F19 module");
MODULE_LICENSE("GPL");
MODULE_VERSION(RMI_DRIVER_VERSION);
