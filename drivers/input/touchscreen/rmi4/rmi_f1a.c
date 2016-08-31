/*
 * Copyright (c) 2012 Synaptics Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#define FUNCTION_DATA f1a_data
#define FNUM 1a

#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/input.h>
#include <linux/slab.h>
#include "rmi_driver.h"

#define QUERY_BASE_INDEX 1
#define MAX_NAME_LEN 256
#define MAX_BUFFER_LEN 80

#define FILTER_MODE_MIN				0
#define FILTER_MODE_MAX				3
#define MULTI_BUTTON_REPORT_MIN			0
#define MULTI_BUTTON_REPORT_MAX			3
#define TX_RX_BUTTON_MIN			0
#define TX_RX_BUTTON_MAX			255
#define THREADHOLD_BUTTON_MIN			0
#define THREADHOLD_BUTTON_MAX			255
#define RELEASE_THREADHOLD_BUTTON_MIN		0
#define RELEASE_THREADHOLD_BUTTON_MAX		255
#define STRONGEST_BUTTON_HYSTERESIS_MIN		0
#define STRONGEST_BUTTON_HYSTERESIS_MAX		255
#define FILTER_STRENGTH_MIN			0
#define FILTER_STRENGTH_MAX			255
#define FUNCTION_NUMBER				0x1a

union f1a_0d_query {
	struct {
		u8 max_button_count:3;
		u8 reserved:5;

		u8 has_general_control:1;
		u8 has_interrupt_enable:1;
		u8 has_multibutton_select:1;
		u8 has_tx_rx_map:1;
		u8 has_perbutton_threshold:1;
		u8 has_release_threshold:1;
		u8 has_strongestbtn_hysteresis:1;
		u8 has_filter_strength:1;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};

union f1a_0d_control_0 {
	struct {
		u8 multibutton_report:2;
		u8 filter_mode:2;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

struct f1a_0d_control_1n {
	u8 interrupt_enabled_button;
};

struct f1a_0d_control_1 {
	struct f1a_0d_control_1n *regs;
	u16 address;
	u8 length;
} __attribute__((__packed__));

struct f1a_0d_control_2n {
	u8 multi_button;
};

struct f1a_0d_control_2 {
	struct f1a_0d_control_2n *regs;
	u16 address;
	u8 length;
} __attribute__((__packed__));

struct f1a_0d_control_3_4n {
	u8 transmitterbtn;
	u8 receiverbtn;
} __attribute__((__packed__));

struct f1a_0d_control_3_4 {
	struct f1a_0d_control_3_4n *regs;
	u16 address;
	u8 length;
} __attribute__((__packed__));

struct f1a_0d_control_5n {
	u8 threshold_button;
};

struct f1a_0d_control_5 {
	struct f1a_0d_control_5n *regs;
	u16 address;
	u8 length;
} __attribute__((__packed__));

union f1a_0d_control_6 {
	struct {
		u8 button_release_threshold;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f1a_0d_control_7 {
	struct {
		u8 strongest_button_hysteresis;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f1a_0d_control_8 {
	struct {
		u8 filter_strength;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

struct f1a_0d_control {
	union f1a_0d_control_0 *reg_0;
	struct f1a_0d_control_1 *reg_1;
	struct f1a_0d_control_2 *reg_2;
	struct f1a_0d_control_3_4 *reg_3_4;
	struct f1a_0d_control_5 *reg_5;
	union f1a_0d_control_6 *reg_6;
	union f1a_0d_control_7 *reg_7;
	union f1a_0d_control_8 *reg_8;
};

/* data specific to fn $1a that needs to be kept around */
struct f1a_data {
	struct f1a_0d_control control;
	union f1a_0d_query query;
	u8 sensor_button_count;
	int button_bitmask_size;
	u8 *button_data_buffer;
	u8 button_map_size;
	u8 *button_map;
	char input_phys[MAX_NAME_LEN];
	struct input_dev *input;
	struct mutex control_mutex;
	struct mutex data_mutex;
};

/* Query sysfs files */
show_union_struct_prototype(max_button_count)
show_union_struct_prototype(has_general_control)
show_union_struct_prototype(has_interrupt_enable)
show_union_struct_prototype(has_multibutton_select)
show_union_struct_prototype(has_tx_rx_map)
show_union_struct_prototype(has_perbutton_threshold)
show_union_struct_prototype(has_release_threshold)
show_union_struct_prototype(has_strongestbtn_hysteresis)
show_union_struct_prototype(has_filter_strength)

static struct attribute *attrs[] = {
	attrify(max_button_count),
	attrify(has_general_control),
	attrify(has_interrupt_enable),
	attrify(has_multibutton_select),
	attrify(has_tx_rx_map),
	attrify(has_perbutton_threshold),
	attrify(has_release_threshold),
	attrify(has_strongestbtn_hysteresis),
	attrify(has_filter_strength),
	NULL
};

static struct attribute_group attrs_query = GROUP(attrs);

/* Control sysfs files */
show_store_union_struct_prototype(multibutton_report)
show_store_union_struct_prototype(filter_mode)
show_store_union_struct_prototype(interrupt_enabled_button)
show_store_union_struct_prototype(multi_button)
show_union_struct_prototype(tx_rx_map)
show_store_union_struct_prototype(threshold_button)
show_store_union_struct_prototype(button_release_threshold)
show_store_union_struct_prototype(strongest_button_hysteresis)
show_store_union_struct_prototype(filter_strength)

static int rmi_f1a_read_control_parameters(struct rmi_device *rmi_dev,
	struct f1a_data *f1a)
{
	int retval = 0;
	union f1a_0d_query *query = &f1a->query;
	struct f1a_0d_control *control = &f1a->control;

	if (query->has_general_control) {
		retval = rmi_read_block(rmi_dev, control->reg_0->address,
				(u8 *)control->reg_0->regs,
				sizeof(control->reg_0->regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg0 to %#06x.\n",
					control->reg_0->address);
			return retval;
		}
	}

	if (query->has_interrupt_enable) {
		retval = rmi_read_block(rmi_dev, control->reg_1->address,
			(u8 *)control->reg_1->regs, f1a->button_bitmask_size);
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg1 to %#06x.\n",
				 control->reg_1->address);
			return retval;
		}
	}

	if (query->has_multibutton_select) {
		retval = rmi_read_block(rmi_dev, control->reg_2->address,
			(u8 *)control->reg_2->regs, f1a->button_bitmask_size);
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg2 to %#06x.\n",
				 control->reg_2->address);
			return retval;
		}
	}

	if (query->has_tx_rx_map) {
		retval = rmi_read_block(rmi_dev, control->reg_3_4->address,
			(u8 *)control->reg_3_4->regs,
			sizeof(struct f1a_0d_control_3_4n) *
					f1a->sensor_button_count);
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg 3_4 to %#06x.\n",
				 control->reg_3_4->address);
			return retval;
		}
	}

	if (query->has_perbutton_threshold) {
		retval = rmi_read_block(rmi_dev, control->reg_5->address,
			(u8 *)control->reg_5->regs, f1a->sensor_button_count);
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg 5 to %#06x.\n",
				 control->reg_5->address);
			return retval;
		}
	}

	if (query->has_release_threshold) {
		retval = rmi_read_block(rmi_dev, control->reg_6->address,
				(u8 *)control->reg_6->regs,
				sizeof(control->reg_6->regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg 6 to %#06x.\n",
					control->reg_6->address);
			return retval;
		}
	}

	if (query->has_strongestbtn_hysteresis) {
		retval = rmi_read_block(rmi_dev, control->reg_7->address,
				(u8 *)control->reg_7->regs,
				sizeof(control->reg_7->regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg 7 to %#06x.\n",
					control->reg_7->address);
			return retval;
		}
	}

	if (query->has_filter_strength) {
		retval = rmi_read_block(rmi_dev, control->reg_8->address,
				(u8 *)control->reg_8->regs,
				sizeof(control->reg_8->regs));
		if (retval < 0) {
			dev_err(&rmi_dev->dev, "Could not read control reg 8 to %#06x.\n",
					control->reg_8->address);
			return retval;
		}
	}
	return 0;
}

static int rmi_f1a_alloc_memory(struct rmi_function_dev *fn_dev)
{
	struct f1a_data *f1a;
	int rc;
	int regSize;
	u16 ctrl_base_addr;

	f1a = devm_kzalloc(&fn_dev->dev, sizeof(struct f1a_data), GFP_KERNEL);
	if (!f1a) {
		dev_err(&fn_dev->dev, "Failed to allocate function data.\n");
		return -ENOMEM;
	}
	fn_dev->data = f1a;

	rc = rmi_read_block(fn_dev->rmi_dev, fn_dev->fd.query_base_addr,
			f1a->query.regs, sizeof(f1a->query.regs));
	if (rc < 0) {
		dev_err(&fn_dev->dev, "Failed to read query register.\n");
		return rc;
	}

	f1a->sensor_button_count = f1a->query.max_button_count+1;

	f1a->button_bitmask_size =
			sizeof(u8)*(f1a->sensor_button_count + 7) / 8;
	f1a->button_data_buffer = devm_kzalloc(&fn_dev->dev,
			f1a->button_bitmask_size, GFP_KERNEL);
	if (!f1a->button_data_buffer) {
		dev_err(&fn_dev->dev, "Failed to allocate button data buffer.\n");
		return -ENOMEM;
	}

	f1a->button_map = devm_kzalloc(&fn_dev->dev,
				f1a->sensor_button_count, GFP_KERNEL);
	if (!f1a->button_map) {
		dev_err(&fn_dev->dev, "Failed to allocate button map.\n");
		return -ENOMEM;
	}

	/* allocate memory for control reg */
	/* reg 0 */
	ctrl_base_addr = fn_dev->fd.control_base_addr;
	if (f1a->query.has_general_control) {
		f1a->control.reg_0 = devm_kzalloc(&fn_dev->dev,
				sizeof(f1a->control.reg_0->regs), GFP_KERNEL);
		if (!f1a->control.reg_0) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_0 control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_0->address = ctrl_base_addr;
		ctrl_base_addr += sizeof(f1a->control.reg_0->regs);
	}
	/* reg 1 */
	if (f1a->query.has_interrupt_enable) {
		f1a->control.reg_1 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f1a_0d_control_1), GFP_KERNEL);
		if (!f1a->control.reg_1) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_1 control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_1->regs = devm_kzalloc(&fn_dev->dev,
				f1a->button_bitmask_size, GFP_KERNEL);
		if (!f1a->control.reg_1->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_1->regs control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_1->address = ctrl_base_addr;
		f1a->control.reg_1->length = f1a->button_bitmask_size;
		ctrl_base_addr += f1a->button_bitmask_size;
	}

	/* reg 2 */
	if (f1a->query.has_multibutton_select) {
		f1a->control.reg_2 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f1a_0d_control_2), GFP_KERNEL);
		if (!f1a->control.reg_2) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_2 control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_2->regs = devm_kzalloc(&fn_dev->dev,
				f1a->button_bitmask_size, GFP_KERNEL);
		if (!f1a->control.reg_2->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_2->regs control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_2->address = ctrl_base_addr;
		f1a->control.reg_2->length = f1a->button_bitmask_size;
		ctrl_base_addr += f1a->button_bitmask_size;
	}

	/* reg 3_4*/
	if (f1a->query.has_tx_rx_map) {
		f1a->control.reg_3_4 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f1a_0d_control_3_4), GFP_KERNEL);
		if (!f1a->control.reg_3_4) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_3_4 control registers.");
			return -ENOMEM;
		}
		regSize = sizeof(struct f1a_0d_control_3_4n);
		f1a->control.reg_3_4->regs = devm_kzalloc(&fn_dev->dev,
				regSize*f1a->sensor_button_count, GFP_KERNEL);
		if (!f1a->control.reg_3_4->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_3_4->regs control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_3_4->address = ctrl_base_addr;
		f1a->control.reg_3_4->length =
					regSize * f1a->sensor_button_count;
		ctrl_base_addr += regSize * f1a->sensor_button_count;
	}

	/* reg 5 */
	if (f1a->query.has_perbutton_threshold) {
		f1a->control.reg_5 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f1a_0d_control_5), GFP_KERNEL);
		if (!f1a->control.reg_5) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_5 control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_5->regs = devm_kzalloc(&fn_dev->dev,
				f1a->sensor_button_count, GFP_KERNEL);
		if (!f1a->control.reg_5->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_5->regs control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_5->address = ctrl_base_addr;
		f1a->control.reg_5->length = f1a->sensor_button_count;
		ctrl_base_addr += f1a->sensor_button_count;
	}

	/* reg 6 */
	if (f1a->query.has_release_threshold) {
		f1a->control.reg_6 = devm_kzalloc(&fn_dev->dev,
			sizeof(f1a->control.reg_6->regs), GFP_KERNEL);
		if (!f1a->control.reg_6) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_6 control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_6->address = ctrl_base_addr;
		ctrl_base_addr += sizeof(f1a->control.reg_6->regs);
	}
	/* reg 7 */
	if (f1a->query.has_strongestbtn_hysteresis) {
		f1a->control.reg_7 = devm_kzalloc(&fn_dev->dev,
			sizeof(f1a->control.reg_7->regs), GFP_KERNEL);
		if (!f1a->control.reg_7) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_7 control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_7->address = ctrl_base_addr;
		ctrl_base_addr += sizeof(f1a->control.reg_7->regs);
	}
	/* reg 8 */
	if (f1a->query.has_filter_strength) {
		f1a->control.reg_8 = devm_kzalloc(&fn_dev->dev,
				sizeof(f1a->control.reg_8->regs), GFP_KERNEL);
		if (!f1a->control.reg_8) {
			dev_err(&fn_dev->dev, "Failed to allocate reg_8 control registers.");
			return -ENOMEM;
		}
		f1a->control.reg_8->address = ctrl_base_addr;
		ctrl_base_addr += sizeof(f1a->control.reg_8->regs);
	}
	return 0;
}

static void rmi_f1a_free_memory(struct rmi_function_dev *fn_dev)
{
	union f1a_0d_query *query;
	struct f1a_data *f1a = fn_dev->data;

	query = &f1a->query;
	sysfs_remove_group(&fn_dev->dev.kobj, &attrs_query);

	if (query->has_general_control) {
		sysfs_remove_file(&fn_dev->dev.kobj,
				  attrify(multibutton_report));
		sysfs_remove_file(&fn_dev->dev.kobj, attrify(filter_mode));
	}

	if (query->has_interrupt_enable)
		sysfs_remove_file(&fn_dev->dev.kobj,
			attrify(interrupt_enabled_button));

	if (query->has_multibutton_select)
		sysfs_remove_file(&fn_dev->dev.kobj, attrify(multi_button));

	if (query->has_tx_rx_map)
		sysfs_remove_file(&fn_dev->dev.kobj, attrify(tx_rx_map));

	if (query->has_perbutton_threshold)
		sysfs_remove_file(&fn_dev->dev.kobj, attrify(threshold_button));

	if (query->has_release_threshold)
		sysfs_remove_file(&fn_dev->dev.kobj,
				attrify(button_release_threshold));

	if (query->has_strongestbtn_hysteresis)
		sysfs_remove_file(&fn_dev->dev.kobj,
				attrify(strongest_button_hysteresis));

	if (query->has_filter_strength)
		sysfs_remove_file(&fn_dev->dev.kobj, attrify(filter_strength));

}

static int rmi_f1a_initialize(struct rmi_function_dev *fn_dev)
{
	int i;
	int retval;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_device_platform_data *pdata;
	struct f1a_data *f1a = fn_dev->data;

	dev_dbg(&fn_dev->dev, "Intializing F1A values.");

	/* initial all default values for f1a data here */
	pdata = to_rmi_platform_data(rmi_dev);
	if (pdata) {
		if (!pdata->f1a_button_map)
			dev_warn(&fn_dev->dev, "button_map is NULL");
		else if (!pdata->f1a_button_map->map)
			dev_warn(&fn_dev->dev,
				 "Platformdata button map is missing!\n");
		else {
			if (pdata->f1a_button_map->nbuttons !=
					f1a->sensor_button_count)
				dev_warn(&fn_dev->dev,
					"Platform data buttonmap size(%d) != number buttons on device(%d)\n",
					pdata->f1a_button_map->nbuttons,
					f1a->sensor_button_count);
			f1a->button_map_size = min(f1a->sensor_button_count,
					 pdata->f1a_button_map->nbuttons);
			for (i = 0; i < f1a->button_map_size; i++)
				f1a->button_map[i] =
					pdata->f1a_button_map->map[i];
		}
	}

	retval = rmi_f1a_read_control_parameters(rmi_dev, f1a);
	if (retval < 0) {
		dev_err(&fn_dev->dev,
			"Failed to initialize F1a control params.\n");
		return retval;
	}

	mutex_init(&f1a->control_mutex);
	mutex_init(&f1a->data_mutex);
	return 0;
}

static int rmi_f1a_register_device(struct rmi_function_dev *fn_dev)
{
	int i;
	int rc;
	struct input_dev *input_dev;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f1a_data *f1a = fn_dev->data;
	struct rmi_driver *driver = fn_dev->rmi_dev->driver;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&fn_dev->dev, "Failed to allocate input device.\n");
		return -ENOMEM;
	}

	f1a->input = input_dev;
	if (driver->set_input_params) {
		rc = driver->set_input_params(rmi_dev, input_dev);
		if (rc < 0) {
			dev_err(&fn_dev->dev, "%s: Error in setting input device.\n",
			__func__);
			goto error_free_device;
		}
	}
	sprintf(f1a->input_phys, "%s/input0", dev_name(&fn_dev->dev));
	input_dev->phys = f1a->input_phys;
	input_dev->dev.parent = &rmi_dev->dev;
	input_set_drvdata(input_dev, f1a);

	/* Set up any input events. */
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	/* manage button map using input subsystem */
	input_dev->keycode = f1a->button_map;
	input_dev->keycodesize = sizeof(f1a->button_map);
	input_dev->keycodemax = f1a->button_map_size;

	/* set bits for each button. */
	for (i = 0; i < f1a->button_map_size; i++) {
		set_bit(f1a->button_map[i], input_dev->keybit);
		input_set_capability(input_dev, EV_KEY, f1a->button_map[i]);
	}

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

static int rmi_f1a_create_sysfs(struct rmi_function_dev *fn_dev)
{
	struct f1a_data *f19 = fn_dev->data;
	union f1a_0d_query *query = &f19->query;

	dev_dbg(&fn_dev->dev, "Creating sysfs files.\n");
	/* Set up sysfs device attributes. */
	if (sysfs_create_group(&fn_dev->dev.kobj, &attrs_query) < 0) {
		dev_err(&fn_dev->dev, "Failed to create query sysfs files.");
		return -ENODEV;
	}

	if (query->has_general_control) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(multibutton_report)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(filter_mode)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
	}

	if (query->has_interrupt_enable) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(interrupt_enabled_button)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
	}

	if (query->has_multibutton_select) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(multi_button)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
	}

	if (query->has_tx_rx_map) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(tx_rx_map)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
	}

	if (query->has_perbutton_threshold) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(threshold_button)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
	}

	if (query->has_release_threshold) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(button_release_threshold)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
	}

	if (query->has_strongestbtn_hysteresis) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(strongest_button_hysteresis)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
	}

	if (query->has_filter_strength) {
		if (sysfs_create_file(&fn_dev->dev.kobj,
				attrify(filter_strength)) < 0) {
			dev_err(&fn_dev->dev, "Failed to create control sysfs files.");
			return -ENODEV;
		}
	}
	return 0;
}

static int rmi_f1a_config(struct rmi_function_dev *fn_dev)
{
	int retval;
	struct f1a_data *f1a = fn_dev->data;
	union f1a_0d_query *query = &f1a->query;
	struct f1a_0d_control *control = &f1a->control;

	if (query->has_general_control) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					control->reg_0->address,
					control->reg_0->regs,
					sizeof(control->reg_0->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg0 to 0x%x\n",
					__func__, control->reg_0->address);
			return retval;
		}
	}
	if (query->has_interrupt_enable) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					control->reg_1->address,
					control->reg_1->regs,
					f1a->button_bitmask_size);
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg 1 to 0x%x\n",
				__func__, control->reg_1->address);
			return retval;
		}
	}
	if (query->has_multibutton_select) {
		retval = rmi_write_block(fn_dev->rmi_dev,
				control->reg_2->address, control->reg_2->regs,
				f1a->button_bitmask_size);
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg 2 to 0x%x\n",
				__func__, control->reg_2->address);
			return -EINVAL;
		}
	}
	if (query->has_tx_rx_map) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					control->reg_3_4->address,
					control->reg_3_4->regs,
					f1a->sensor_button_count);
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg_3_4 to 0x%x\n",
					__func__, control->reg_3_4->address);
			return -EINVAL;
		}
	}
	if (query->has_perbutton_threshold) {
		retval = rmi_write_block(fn_dev->rmi_dev,
				control->reg_5->address, control->reg_5->regs,
				f1a->sensor_button_count);
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write to reg 5 to 0x%x\n",
				__func__, control->reg_5->address);
			return retval;
		}
	}
	if (query->has_release_threshold) {
		retval = rmi_write_block(fn_dev->rmi_dev,
				control->reg_6->address, control->reg_6->regs,
				sizeof(control->reg_6->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write  reg 6 to 0x%x\n",
				__func__, control->reg_6->address);
			return -EINVAL;
		}
	}
	if (query->has_strongestbtn_hysteresis) {
		retval = rmi_write_block(fn_dev->rmi_dev,
				control->reg_7->address, control->reg_7->regs,
				sizeof(control->reg_7->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg 7 to 0x%x\n",
				__func__, control->reg_7->address);
			return -EINVAL;
		}
	}
	if (query->has_filter_strength) {
		retval = rmi_write_block(fn_dev->rmi_dev,
				control->reg_8->address, control->reg_8->regs,
				sizeof(control->reg_8->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg 8 to 0x%x\n",
				__func__, control->reg_8->address);
			return -EINVAL;
		}
	}
	return 0;
}


static int rmi_f1a_probe(struct rmi_function_dev *fn_dev)
{
	int rc;

	rc = rmi_f1a_alloc_memory(fn_dev);
	if (rc < 0)
		goto err_free_data;

	rc = rmi_f1a_initialize(fn_dev);
	if (rc < 0)
		goto err_free_data;

	rc = rmi_f1a_register_device(fn_dev);
	if (rc < 0)
		goto err_free_data;

	rc = rmi_f1a_create_sysfs(fn_dev);
	if (rc < 0)
		goto err_free_data;

	return 0;

err_free_data:
	rmi_f1a_free_memory(fn_dev);

	return rc;
}

static int rmi_f1a_remove(struct rmi_function_dev *fn_dev)
{
	struct f1a_data *f1a = fn_dev->data;

	input_unregister_device(f1a->input);
	rmi_f1a_free_memory(fn_dev);

	return 0;
}

static int rmi_f1a_attention(struct rmi_function_dev *fn_dev,
						unsigned long *irq_bits)
{
	int error;
	int button;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f1a_data *f1a = fn_dev->data;
	u16 data_base_addr = fn_dev->fd.data_base_addr;

	/* Read the button data. */
	error = rmi_read_block(rmi_dev, data_base_addr, f1a->button_data_buffer,
			f1a->button_bitmask_size);
	if (error < 0) {
		dev_err(&fn_dev->dev, "%s: Failed to read button data registers.\n",
			__func__);
		return error;
	}

	/* Generate events for buttons that change state. */
	for (button = 0; button < f1a->sensor_button_count; button++) {
		int button_reg;
		int button_shift;
		bool button_status;

		/* determine which data byte the button status is in */
		button_reg = button / 8;
		/* bit shift to get button's status */
		button_shift = button % 8;
		button_status =
		    ((f1a->button_data_buffer[button_reg] >> button_shift)
			& 0x01) != 0;

		dev_dbg(&fn_dev->dev, "%s: Button %d (code %d) -> %d.\n",
			__func__, button, f1a->button_map[button],
				button_status);
		/* Generate an event here. */
		input_report_key(f1a->input, f1a->button_map[button],
					button_status);
	}

	input_sync(f1a->input); /* sync after groups of events */
	return 0;
}

static struct rmi_function_driver function_driver = {
	.driver = {
		.name = "rmi_f1a",
	},
	.func = FUNCTION_NUMBER,
	.probe = rmi_f1a_probe,
	.remove = rmi_f1a_remove,
	.config = rmi_f1a_config,
	.attention = rmi_f1a_attention,
};

/* sysfs functions */
/* Query */
simple_show_union_struct_unsigned(query, max_button_count)
simple_show_union_struct_unsigned(query, has_general_control)
simple_show_union_struct_unsigned(query, has_interrupt_enable)
simple_show_union_struct_unsigned(query, has_multibutton_select)
simple_show_union_struct_unsigned(query, has_tx_rx_map)
simple_show_union_struct_unsigned(query, has_perbutton_threshold)
simple_show_union_struct_unsigned(query, has_release_threshold)
simple_show_union_struct_unsigned(query, has_strongestbtn_hysteresis)
simple_show_union_struct_unsigned(query, has_filter_strength)

/* Control */
show_store_union_struct_unsigned(control, reg_0, multibutton_report)
show_store_union_struct_unsigned(control, reg_0, filter_mode)
show_store_union_struct_unsigned(control, reg_6, button_release_threshold)
show_store_union_struct_unsigned(control, reg_7, strongest_button_hysteresis)
show_store_union_struct_unsigned(control, reg_8, filter_strength)

/* repeated register functions */
show_store_repeated_union_struct_unsigned(control, reg_1,
						interrupt_enabled_button)
show_store_repeated_union_struct_unsigned(control, reg_2, multi_button)
show_store_repeated_union_struct_unsigned(control, reg_5, threshold_button)

static ssize_t rmi_fn_1a_tx_rx_map_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct rmi_function_dev *fn_dev;
	struct FUNCTION_DATA *data;
	struct f1a_0d_control *control;
	int reg_length;
	int result, size = 0;
	char *temp;
	int i;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;
	control = &data->control;
	/* Read current regtype values */
	reg_length = control->reg_3_4->length;
	result = rmi_read_block(fn_dev->rmi_dev, control->reg_3_4->address,
			(u8 *)control->reg_3_4->regs,
			reg_length * sizeof(u8));

	if (result < 0) {
		dev_dbg(dev, "%s : Could not read regtype at 0x%x\n Data may be outdated.",
				__func__, control->reg_3_4->address);
	}
	temp = buf;
	for (i = 0; i < reg_length/2; i++) {
		result = snprintf(temp, PAGE_SIZE - size, "%u-%u ",
				control->reg_3_4->regs[i].transmitterbtn,
				control->reg_3_4->regs[i].receiverbtn);
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

module_rmi_function_driver(function_driver);

MODULE_AUTHOR("Vivian Ly <vly@synaptics.com>");
MODULE_DESCRIPTION("RMI F1a module");
MODULE_LICENSE("GPL");
MODULE_VERSION(RMI_DRIVER_VERSION);
