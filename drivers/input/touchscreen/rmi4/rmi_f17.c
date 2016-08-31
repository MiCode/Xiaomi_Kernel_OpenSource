/*
 * Copyright (c) 2012 Synaptics Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/input.h>
#include <linux/slab.h>
#include "rmi_driver.h"

#define QUERY_BASE_INDEX 1
#define NAME_BUFFER_SIZE 256
#define FUNCTION_NUMBER 0x17

union f17_device_query {
	struct {
		u8 number_of_sticks:3;
	} __attribute__((__packed__));
	u8 regs[1];
};

#define F17_MANUFACTURER_SYNAPTICS 0
#define F17_MANUFACTURER_NMB 1
#define F17_MANUFACTURER_ALPS 2

struct f17_stick_query {
	union {
		struct {
			u8 manufacturer:4;
			u8 resistive:1;
			u8 ballistics:1;
			u8 reserved1:2;
			u8 has_relative:1;
			u8 has_absolute:1;
			u8 has_gestures:1;
			u8 has_dribble:1;
			u8 reserved2:4;
		} __attribute__((__packed__));
		u8 regs[2];
	} general;

	union {
		struct {
			u8 has_single_tap:1;
			u8 has_tap_and_hold:1;
			u8 has_double_tap:1;
			u8 has_early_tap:1;
			u8 has_press:1;
		} __attribute__((__packed__));
		u8 regs[1];
	} gestures;
};

union f17_device_controls {
	struct {
		u8 reporting_mode:3;
		u8 dribble:1;
	} __attribute__((__packed__));
	u8 regs[1];
};

struct f17_stick_controls {
	union {
		struct {
			u8 z_force_threshold;
			u8 radial_force_threshold;
		} __attribute__((__packed__));
		u8 regs[3];
	} general;

	union {
		struct {
			u8 motion_sensitivity:4;
			u8 antijitter:1;
		} __attribute__((__packed__));
		u8 regs[1];
	} relative;

	union {
		struct {
			u8 single_tap:1;
			u8 tap_and_hold:1;

			u8 double_tap:1;
			u8 early_tap:1;
			u8 press:1;
		} __attribute__((__packed__));
		u8 regs[1];
	} enable;

	u8 maximum_tap_time;
	u8 minimum_press_time;
	u8 maximum_radial_force;
};


union f17_device_commands {
	struct {
		u8 rezero:1;
	} __attribute__((__packed__));
	u8 regs[1];
};

struct f17_stick_data {
	union {
		struct {
			u8 x_force_high:8;
			u8 y_force_high:8;
			u8 y_force_low:4;
			u8 x_force_low:4;
			u8 z_force:8;
		} __attribute__((__packed__));
		struct {
			u8 regs[4];
			u16 address;
		} __attribute__((__packed__));
	} abs;
	union {
		struct {
			s8 x_delta:8;
			s8 y_delta:8;
		} __attribute__((__packed__));
		struct {
			u8 regs[2];
			u16 address;
		} __attribute__((__packed__));
	} rel;
	union {
		struct {
			u8 single_tap:1;
			u8 tap_and_hold:1;
			u8 double_tap:1;
			u8 early_tap:1;
			u8 press:1;
			u8 reserved:3;
		} __attribute__((__packed__));
		struct {
			u8 regs[1];
			u16 address;
		} __attribute__((__packed__));
	} gestures;
};


/* data specific to f17 that needs to be kept around */

struct rmi_f17_stick_data {
	struct f17_stick_query query;
	struct f17_stick_controls controls;
	struct f17_stick_data data;

	u16 control_address;

	int index;

	char input_phys[NAME_BUFFER_SIZE];
	struct input_dev *input;
	char mouse_phys[NAME_BUFFER_SIZE];
	struct input_dev *mouse;
};

struct rmi_f17_device_data {
	u16 control_address;

	union f17_device_query query;
	union f17_device_commands commands;
	union f17_device_controls controls;

	struct rmi_f17_stick_data *sticks;

};

static ssize_t f17_rezero_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct rmi_function_dev *fn_dev;
	struct rmi_f17_device_data *f17;

	fn_dev = to_rmi_function_dev(dev);
	f17 = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			f17->commands.rezero);

}

static ssize_t f17_rezero_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct rmi_function_dev *fn_dev;
	struct rmi_f17_device_data *data;
	unsigned int new_value;
	int len;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;
	len = sscanf(buf, "%u", &new_value);
	if (new_value != 0 && new_value != 1) {
		dev_err(dev,
			"%s: Error - rezero is not a valid value 0x%x.\n",
			__func__, new_value);
		return -EINVAL;
	}
	data->commands.rezero = new_value;
	len = rmi_write(fn_dev->rmi_dev, fn_dev->fd.command_base_addr,
		data->commands.rezero);

	if (len < 0) {
		dev_err(dev, "%s : Could not write rezero to 0x%x\n",
				__func__, fn_dev->fd.command_base_addr);
		return -EINVAL;
	}
	return count;
}

static struct device_attribute attrs[] = {
	__ATTR(rezero, RMI_RW_ATTR,
		f17_rezero_show, f17_rezero_store),
};


int f17_read_control_parameters(struct rmi_device *rmi_dev,
	struct rmi_f17_device_data *f17)
{
	int retval = 0;

	/* TODO: read this or delete the function */

	return retval;
}

static int f17_alloc_memory(struct rmi_function_dev *fn_dev)
{
	struct rmi_f17_device_data *f17;
	int retval;
	int size;

	f17 = devm_kzalloc(&fn_dev->dev, sizeof(struct rmi_f17_device_data),
		GFP_KERNEL);
	if (!f17) {
		dev_err(&fn_dev->dev, "Failed to allocate function data.\n");
		return -ENOMEM;
	}
	fn_dev->data = f17;

	retval = rmi_read_block(fn_dev->rmi_dev, fn_dev->fd.query_base_addr,
				f17->query.regs, sizeof(f17->query.regs));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Failed to read query register.\n");
		return retval;
	}

	size = (f17->query.number_of_sticks+1)*
			sizeof(struct rmi_f17_stick_data);
	f17->sticks = devm_kzalloc(&fn_dev->dev, size, GFP_KERNEL);
	if (!f17->sticks) {
		dev_err(&fn_dev->dev, "Failed to allocate per stick data.\n");
		return -ENOMEM;
	}

	return 0;
}

static int f17_init_stick(struct rmi_device *rmi_dev,
			  struct rmi_f17_stick_data *stick,
			  u16 *next_query_reg, u16 *next_data_reg,
			  u16 *next_control_reg) {
	int retval = 0;

	retval = rmi_read_block(rmi_dev, *next_query_reg,
		stick->query.general.regs,
		sizeof(stick->query.general.regs));
	if (retval < 0) {
		dev_err(&rmi_dev->dev, "Failed to read stick general query.\n");
		return retval;
	}
	*next_query_reg += sizeof(stick->query.general.regs);

	dev_dbg(&rmi_dev->dev, "Stick %d found\n", stick->index);
	dev_dbg(&rmi_dev->dev, "    Manufacturer: %d.\n",
				stick->query.general.manufacturer);
	dev_dbg(&rmi_dev->dev, "    Resistive:    %d.\n",
				stick->query.general.resistive);
	dev_dbg(&rmi_dev->dev, "    Ballistics:   %d.\n",
				stick->query.general.ballistics);
	dev_dbg(&rmi_dev->dev, "    Manufacturer: %d.\n",
				stick->query.general.ballistics);
	dev_dbg(&rmi_dev->dev, "    Has relative: %d.\n",
				stick->query.general.has_relative);
	dev_dbg(&rmi_dev->dev, "    Has absolute: %d.\n",
				stick->query.general.has_absolute);
	dev_dbg(&rmi_dev->dev, "    Had dribble:  %d.\n",
				stick->query.general.has_dribble);
	dev_dbg(&rmi_dev->dev, "    Has gestures: %d.\n",
				stick->query.general.has_gestures);

	if (stick->query.general.has_gestures) {
		retval = rmi_read_block(rmi_dev, *next_query_reg,
			stick->query.gestures.regs,
			sizeof(stick->query.gestures.regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev,
				"Failed to read F17 gestures query, code %d.\n",
				retval);
			return retval;
		}
		*next_query_reg += sizeof(stick->query.gestures.regs);
		dev_dbg(&rmi_dev->dev, "        single tap: %d.\n",
					stick->query.gestures.has_single_tap);
		dev_dbg(&rmi_dev->dev, "        tap & hold: %d.\n",
					stick->query.gestures.has_tap_and_hold);
		dev_dbg(&rmi_dev->dev, "        double tap: %d.\n",
					stick->query.gestures.has_double_tap);
		dev_dbg(&rmi_dev->dev, "        early tap:  %d.\n",
					stick->query.gestures.has_early_tap);
		dev_dbg(&rmi_dev->dev, "        press:      %d.\n",
					stick->query.gestures.has_press);
	}
	if (stick->query.general.has_absolute) {
		stick->data.abs.address = *next_data_reg;
		*next_data_reg += sizeof(stick->data.abs.regs);
	}
	if (stick->query.general.has_relative) {
		stick->data.rel.address = *next_data_reg;
		*next_data_reg += sizeof(stick->data.rel.regs);
	}
	if (stick->query.general.has_gestures) {
		stick->data.gestures.address = *next_data_reg;
		*next_data_reg += sizeof(stick->data.gestures.regs);
	}

	return retval;
}

static int f17_initialize(struct rmi_function_dev *fn_dev)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_f17_device_data *f17 = fn_dev->data;
	int i;
	int retval;
	u16 next_query_reg = fn_dev->fd.query_base_addr;
	u16 next_data_reg = fn_dev->fd.data_base_addr;
	u16 next_control_reg = fn_dev->fd.control_base_addr;

	retval = rmi_read_block(fn_dev->rmi_dev, fn_dev->fd.query_base_addr,
				f17->query.regs, sizeof(f17->query.regs));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Failed to read query register.\n");
		return retval;
	}
	dev_info(&fn_dev->dev, "Found F17 with %d sticks.\n",
		 f17->query.number_of_sticks + 1);
	next_query_reg += sizeof(f17->query.regs);

	retval = rmi_read_block(rmi_dev, fn_dev->fd.command_base_addr,
		f17->commands.regs, sizeof(f17->commands.regs));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Failed to read command register.\n");
		return retval;
	}

	f17->control_address = fn_dev->fd.control_base_addr;
	retval = f17_read_control_parameters(rmi_dev, f17);
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Failed to initialize F17 control params.\n");
		return retval;
	}

	for (i = 0; i < f17->query.number_of_sticks + 1; i++) {
		f17->sticks[i].index = i;
		retval = f17_init_stick(rmi_dev, &f17->sticks[i],
					&next_query_reg, &next_data_reg,
					&next_control_reg);
		if (!retval) {
			dev_err(&fn_dev->dev, "Failed to init stick %d.\n", i);
			return retval;
		}
	}

	return retval;
}

static int f17_register_stick(struct rmi_function_dev *fn_dev,
			      struct rmi_f17_stick_data *stick) {
	int retval = 0;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_driver *driver = fn_dev->rmi_dev->driver;

	if (stick->query.general.has_absolute) {
		struct input_dev *input_dev;
		input_dev = input_allocate_device();
		if (!input_dev) {
			dev_err(&rmi_dev->dev, "Failed to allocate stick device %d.\n",
				stick->index);
			return -ENOMEM;
		}

		if (driver->set_input_params) {
			retval = driver->set_input_params(rmi_dev, input_dev);
			if (retval < 0) {
				dev_err(&fn_dev->dev, "%s: Error in setting input device.\n",
				__func__);
				return retval;
			}
		}
		sprintf(stick->input_phys, "%s.abs/input0",
			dev_name(&fn_dev->dev));
		input_dev->phys = stick->input_phys;

		input_dev->dev.parent = &fn_dev->dev;
		input_set_drvdata(input_dev, stick);

		retval = input_register_device(input_dev);
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Failed to register stick device %d.\n",
				stick->index);
			goto error_free_device;
		}
		stick->input = input_dev;
	}

	if (stick->query.general.has_relative) {
		struct input_dev *input_dev_mouse;
		/*create input device for mouse events  */
		input_dev_mouse = input_allocate_device();
		if (!input_dev_mouse) {
			retval = -ENOMEM;
			goto error_free_device;
		}

		if (driver->set_input_params) {
			retval = driver->set_input_params(rmi_dev,
						input_dev_mouse);
			if (retval < 0) {
				dev_err(&fn_dev->dev, "%s: Error in setting relative input device.\n",
				__func__);
				return retval;
			}
		}
		sprintf(stick->mouse_phys, "%s.rel/input0",
			dev_name(&fn_dev->dev));
		input_dev_mouse->phys = stick->mouse_phys;
		input_dev_mouse->dev.parent = &fn_dev->dev;

		set_bit(EV_REL, input_dev_mouse->evbit);
		set_bit(REL_X, input_dev_mouse->relbit);
		set_bit(REL_Y, input_dev_mouse->relbit);

		set_bit(BTN_MOUSE, input_dev_mouse->evbit);
		/* Register device's buttons and keys */
		set_bit(EV_KEY, input_dev_mouse->evbit);
		set_bit(BTN_LEFT, input_dev_mouse->keybit);
		set_bit(BTN_MIDDLE, input_dev_mouse->keybit);
		set_bit(BTN_RIGHT, input_dev_mouse->keybit);

		retval = input_register_device(input_dev_mouse);
		if (retval < 0)
			goto error_free_device;
		stick->mouse = input_dev_mouse;
	}

	return 0;

error_free_device:
	if (stick->input) {
		input_free_device(stick->input);
		stick->input = NULL;
	}
	if (stick->mouse) {
		input_free_device(stick->mouse);
		stick->mouse = NULL;
	}
	return retval;
}

static int f17_register_devices(struct rmi_function_dev *fn_dev)
{
	struct rmi_f17_device_data *f17 = fn_dev->data;
	int i;
	int retval = 0;

	for (i = 0; i < f17->query.number_of_sticks + 1 && !retval; i++)
		retval = f17_register_stick(fn_dev, &f17->sticks[i]);

	return retval;
}

static int f17_create_sysfs(struct rmi_function_dev *fn_dev)
{
	int attr_count = 0;
	int rc;

	dev_dbg(&fn_dev->dev, "Creating sysfs files.\n");
	/* Set up sysfs device attributes. */
	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		if (sysfs_create_file
		    (&fn_dev->dev.kobj, &attrs[attr_count].attr) < 0) {
			dev_err(&fn_dev->dev,
				"Failed to create sysfs file for %s.",
				attrs[attr_count].attr.name);
			rc = -ENODEV;
			goto err_remove_sysfs;
		}
	}

	return 0;

err_remove_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(&fn_dev->dev.kobj, &attrs[attr_count].attr);
	return rc;

}

static int f17_config(struct rmi_function_dev *fn_dev)
{
	struct rmi_f17_device_data *f17 = fn_dev->data;
	int retval;

	retval = rmi_write_block(fn_dev->rmi_dev, f17->control_address,
		f17->controls.regs, sizeof(f17->controls.regs));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not write stick controls to 0x%04x\n",
				f17->control_address);
		return retval;
	}

	return retval;
}

static int rmi_f17_remove(struct rmi_function_dev *fn_dev)
{
	struct rmi_f17_device_data *f17 = fn_dev->data;
	int i;

	for (i = 0; i < ARRAY_SIZE(attrs); i++)
		sysfs_remove_file(&fn_dev->dev.kobj, &attrs[i].attr);

	for (i = 0; i < f17->query.number_of_sticks + 1; i++)
		input_unregister_device(f17->sticks[i].input);

	return 0;
}

static int f17_process_stick(struct rmi_device *rmi_dev,
			     struct rmi_f17_stick_data *stick) {
	int retval = 0;

	if (stick->query.general.has_absolute) {
		retval = rmi_read_block(rmi_dev, stick->data.abs.address,
			stick->data.abs.regs, sizeof(stick->data.abs.regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev,
				"Failed to read abs data for stick %d, code %d.\n",
				stick->index, retval);
			goto error_exit;
		}
	}
	if (stick->query.general.has_relative) {
		retval = rmi_read_block(rmi_dev, stick->data.rel.address,
			stick->data.rel.regs, sizeof(stick->data.rel.regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev,
				"Failed to read rel data for stick %d, code %d.\n",
				stick->index, retval);
			goto error_exit;
		}
		dev_dbg(&rmi_dev->dev, "Reporting dX: %d, dy: %d\n",
			stick->data.rel.x_delta, stick->data.rel.y_delta);
		input_report_rel(stick->mouse, REL_X, stick->data.rel.x_delta);
		input_report_rel(stick->mouse, REL_Y, stick->data.rel.y_delta);
	}
	if (stick->query.general.has_gestures) {
		retval = rmi_read_block(rmi_dev, stick->data.gestures.address,
			stick->data.gestures.regs,
			sizeof(stick->data.gestures.regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev,
				"Failed to read gestures for stick %d, code %d.\n",
				stick->index, retval);
			goto error_exit;
		}
	}
	retval = 0;

error_exit:
	if (stick->input)
		input_sync(stick->input);
	if (stick->mouse)
		input_sync(stick->mouse);
	return retval;
}

static int rmi_f17_probe(struct rmi_function_dev *fn_dev)
{
	int retval;

	retval = f17_alloc_memory(fn_dev);
	if (retval < 0)
		return retval;

	retval = f17_initialize(fn_dev);
	if (retval < 0)
		return retval;

	retval = f17_register_devices(fn_dev);
	if (retval < 0)
		return retval;
	retval = f17_create_sysfs(fn_dev);
	if (retval < 0)
		return retval;
	return 0;
}

static int f17_attention(struct rmi_function_dev *fn_dev,
						unsigned long *irq_bits)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_f17_device_data *f17 = fn_dev->data;
	int i;
	int retval = 0;

	for (i = 0; i < f17->query.number_of_sticks + 1 && !retval; i++)
		retval = f17_process_stick(rmi_dev, &f17->sticks[i]);

	return retval;
}

static struct rmi_function_driver function_driver = {
	.driver = {
		.name = "rmi_f17",
	},
	.func = FUNCTION_NUMBER,
	.probe = rmi_f17_probe,
	.remove = rmi_f17_remove,
	.config = f17_config,
	.attention = f17_attention,
};

module_rmi_function_driver(function_driver);

MODULE_AUTHOR("Christopher Heiny <cheiny@synaptics.com>");
MODULE_DESCRIPTION("RMI F17 module");
MODULE_LICENSE("GPL");
MODULE_VERSION(RMI_DRIVER_VERSION);
