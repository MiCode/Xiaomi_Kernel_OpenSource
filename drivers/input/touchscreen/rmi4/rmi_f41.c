/*
 * Copyright (c) 2012 Synaptics Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#define FUNCTION_DATA rmi_fn_41_data
#define FNUM 41

#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/input.h>
#include <linux/slab.h>
#include "rmi_driver.h"

#define MAX_STR_LEN 256
#define FUNCTION_NUMBER	0x41

union f41_ap_query {
	struct {
		/* Query 0 */
		u8 has_reduced_reporting:1;
		u8 has_modal_control:1;
		u8 has_force:1;
		u8 has_orientation:1;
		u8 has_serial_number:1;
		u8 has_battery:1;
		u8 has_z:1;
		u8 has_single_tap:1;

		/* Query 1 */
		u8 number_of_buttons:3;
		u8 f41_ap_query1_b3__7:5;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};


union f41_ap_control_0 {
	struct {
		/* control 0 */
		u8 reporting_mode:2;
		u8 modal_ctrl:1;
		u8 f41_ap_control0_b3__4:2;
		u8 single_tap_interrupt_enable:1;
		u8 in_range_interrupt_enable:1;
		u8 new_sn_interrupt_enable:1;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f41_ap_control_1 {
	struct {
		/* control 1 */
		u8 button_1_interrupt_enable:1;
		u8 button_2_interrupt_enable:1;
		u8 button_3_interrupt_enable:1;
		u8 button_4_interrupt_enable:1;
		u8 button_5_interrupt_enable:1;
		u8 button_6_interrupt_enable:1;
		u8 button_7_interrupt_enable:1;
		u8 f41_ap_control1_b7:1;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f41_ap_control_2__5 {
	struct {
		u16 max_x_position;
		u16 max_y_position;
	} __attribute__((__packed__));
	struct {
		u8 regs[4];
		u16 address;
	} __attribute__((__packed__));
};

union f41_ap_control_6__9 {
	struct {
		u16 x_reduced_reporting_distance;
		u16 y_reduced_reporting_distance;
	} __attribute__((__packed__));
	struct {
		u8 regs[4];
		u16 address;
	} __attribute__((__packed__));
};

union f41_ap_control_10 {
	struct {
		u8 max_tap_time;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

struct f41_ap_control {
	union f41_ap_control_0 *reg_0;
	union f41_ap_control_1 *reg_1;
	union f41_ap_control_2__5 *reg_2__5;
	union f41_ap_control_6__9 *reg_6__9;
	union f41_ap_control_10 *reg_10;
};


union f41_ap_data_0__3 {
	struct {
		/* data 0-1 */
		u16 x_position;

		/* data 2-3 */
		u16 y_position;
	} __attribute__((__packed__));
	struct {
		u8 regs[4];
	} __attribute__((__packed__));
};

union f41_ap_data_4 {
	struct {
		/* data 4 */
		u8 force;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
	} __attribute__((__packed__));
};

union f41_ap_data_5 {
	struct {
		/* data 5 */
		u8 button_1_state:1;
		u8 button_2_state:1;
		u8 button_3_state:1;
		u8 button_4_state:1;
		u8 button_5_state:1;
		u8 button_6_state:1;
		u8 button_7_state:1;
		u8 f41_ap_data_5_b7:1;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
	} __attribute__((__packed__));
};

union f41_ap_data_6 {
	struct {
		/* data 6 */
		u8 single_tap:1;
		u8 in_range:1;
		u8 new_serial_number:1;
		u8 f41_ap_data_6_b3__7:5;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
	} __attribute__((__packed__));
};

union f41_ap_data_7__8 {
	struct {
		/* data 7 */
		u8 azimuth;

		/* data 8 */
		u8 altitude;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
	} __attribute__((__packed__));
};

union f41_ap_data_9 {
	struct {
		/* data 9 */
		u8 z_position;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
	} __attribute__((__packed__));
};

union f41_ap_data_10 {
	struct {
		/* data 10 */
		u8 battery_level;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
	} __attribute__((__packed__));
};

union f41_ap_data_11__14 {
	struct {
		/* data 11-14 */
		u32 pen_serial_number;
	} __attribute__((__packed__));
	struct {
		u8 regs[4];
	} __attribute__((__packed__));
};

struct f41_ap_data {
	union f41_ap_data_0__3 *reg_0__3;
	union f41_ap_data_4 *reg_4;
	union f41_ap_data_5 *reg_5;
	union f41_ap_data_6 *reg_6;
	union f41_ap_data_7__8 *reg_7__8;
	union f41_ap_data_9 *reg_9;
	union f41_ap_data_10 *reg_10;
	union f41_ap_data_11__14 *reg_11__14;
};

/* Query */
show_union_struct_prototype(has_reduced_reporting)
show_union_struct_prototype(has_modal_control)
show_union_struct_prototype(has_force)
show_union_struct_prototype(has_orientation)
show_union_struct_prototype(has_serial_number)
show_union_struct_prototype(has_battery)
show_union_struct_prototype(has_z)
show_union_struct_prototype(has_single_tap)
show_union_struct_prototype(number_of_buttons)

static struct attribute *attrs_q[] = {
	attrify(has_reduced_reporting),
	attrify(has_modal_control),
	attrify(has_force),
	attrify(has_orientation),
	attrify(has_serial_number),
	attrify(has_battery),
	attrify(has_z),
	attrify(has_single_tap),
	attrify(number_of_buttons),
	NULL
};
static struct attribute_group attrs_query = GROUP(attrs_q);

/* control 0 */
show_store_union_struct_prototype(reporting_mode)
show_store_union_struct_prototype(modal_ctrl)
show_store_union_struct_prototype(single_tap_interrupt_enable)
show_store_union_struct_prototype(in_range_interrupt_enable)
show_store_union_struct_prototype(new_sn_interrupt_enable)

static struct attribute *attrs_reg_0a[] = {
	attrify(reporting_mode),
	NULL
};
static struct attribute *attrs_reg_0b[] = {
	attrify(modal_ctrl),
	NULL
};
static struct attribute *attrs_reg_0c[] = {
	attrify(single_tap_interrupt_enable),
	NULL
};
static struct attribute *attrs_reg_0d[] = {
	attrify(in_range_interrupt_enable),
	NULL
};
static struct attribute *attrs_reg_0e[] = {
	attrify(new_sn_interrupt_enable),
	NULL
};

/* control 1 */
show_store_union_struct_prototype(button_1_interrupt_enable)
show_store_union_struct_prototype(button_2_interrupt_enable)
show_store_union_struct_prototype(button_3_interrupt_enable)
show_store_union_struct_prototype(button_4_interrupt_enable)
show_store_union_struct_prototype(button_5_interrupt_enable)
show_store_union_struct_prototype(button_6_interrupt_enable)
show_store_union_struct_prototype(button_7_interrupt_enable)
static struct attribute *attrs_reg_1[] = {
	attrify(button_1_interrupt_enable),
	attrify(button_2_interrupt_enable),
	attrify(button_3_interrupt_enable),
	attrify(button_4_interrupt_enable),
	attrify(button_5_interrupt_enable),
	attrify(button_6_interrupt_enable),
	attrify(button_7_interrupt_enable),
	NULL
};

/* control 2-5 */
show_store_union_struct_prototype(max_x_position)
show_store_union_struct_prototype(max_y_position)
static struct attribute *attrs_reg_2__5[] = {
	attrify(max_x_position),
	attrify(max_y_position),
	NULL
};

/* control 6-9 */
show_store_union_struct_prototype(x_reduced_reporting_distance)
show_store_union_struct_prototype(y_reduced_reporting_distance)
static struct attribute *attrs_reg_6__9[] = {
	attrify(x_reduced_reporting_distance),
	attrify(y_reduced_reporting_distance),
	NULL
};

/* control 10 */
show_store_union_struct_prototype(max_tap_time)
static struct attribute *attrs_reg_10[] = {
	attrify(max_tap_time),
	NULL
};


static struct attribute_group attrs_ctrl[] = {
	GROUP(attrs_reg_0a),
	GROUP(attrs_reg_0b),
	GROUP(attrs_reg_0c),
	GROUP(attrs_reg_0d),
	GROUP(attrs_reg_0e),
	GROUP(attrs_reg_1),
	GROUP(attrs_reg_2__5),
	GROUP(attrs_reg_6__9),
	GROUP(attrs_reg_10)
};

enum ctrl_reg_group {
	f41_ctrl_reg_0a = 0,
	f41_ctrl_reg_0b = 1,
	f41_ctrl_reg_0c = 2,
	f41_ctrl_reg_0d = 3,
	f41_ctrl_reg_0e = 4,
	f41_ctrl_reg_1 = 5,
	f41_ctrl_reg_2__5 = 6,
	f41_ctrl_reg_6__9 = 7,
	f41_ctrl_reg_10 = 8
};

/* data 0-3 */
show_union_struct_prototype(x_position)
show_union_struct_prototype(y_position)
static struct attribute *attrs_data_reg_0__3[] = {
	attrify(x_position),
	attrify(y_position),
	NULL
};

/* data 4 */
show_union_struct_prototype(force)
static struct attribute *attrs_data_reg_4[] = {
	attrify(force),
	NULL
};

/* data 5 */
/* Handled in button map */

/* data 6 */
show_union_struct_prototype(single_tap)
show_union_struct_prototype(in_range)
show_union_struct_prototype(new_serial_number)
static struct attribute *attrs_data_reg_6a[] = {
	attrify(single_tap),
	NULL
};
static struct attribute *attrs_data_reg_6b[] = {
	attrify(in_range),
	NULL
};
static struct attribute *attrs_data_reg_6c[] = {
	attrify(new_serial_number),
	NULL
};

/* data 7-8 */
show_union_struct_prototype(azimuth)
show_union_struct_prototype(altitude)
static struct attribute *attrs_data_reg_7__8[] = {
	attrify(azimuth),
	attrify(altitude),
	NULL
};

/* data 9 */
show_union_struct_prototype(z_position)
static struct attribute *attrs_data_reg_9[] = {
	attrify(z_position),
	NULL
};

/* data 10 */
show_union_struct_prototype(battery_level)
static struct attribute *attrs_data_reg_10[] = {
	attrify(battery_level),
	NULL
};

/* data 11-14 */
show_union_struct_prototype(pen_serial_number)
static struct attribute *attrs_data_reg_11__14[] = {
	attrify(pen_serial_number),
	NULL
};

static struct attribute_group attrs_data[] = {
	GROUP(attrs_data_reg_0__3),
	GROUP(attrs_data_reg_4),
	GROUP(attrs_data_reg_6a),
	GROUP(attrs_data_reg_6b),
	GROUP(attrs_data_reg_6c),
	GROUP(attrs_data_reg_7__8),
	GROUP(attrs_data_reg_9),
	GROUP(attrs_data_reg_10),
	GROUP(attrs_data_reg_11__14)
};

enum data_reg_group {
	f41_data_reg_0__3 = 0,
	f41_data_reg_4 = 1,
	f41_data_reg_6a = 2,
	f41_data_reg_6b = 3,
	f41_data_reg_6c = 4,
	f41_data_reg_7__8 = 5,
	f41_data_reg_9 = 6,
	f41_data_reg_10 = 7,
	f41_data_reg_11__14 = 8
};

/* data specific to fn $41 that needs to be kept around */
struct rmi_fn_41_data {
	union f41_ap_query query;
	struct f41_ap_control control;
	struct f41_ap_data data;
	u8 *data_block;
	u8 data_block_size;

	bool attrs_ctrl_regs_exist[ARRAY_SIZE(attrs_ctrl)];
	bool attrs_data_regs_exist[ARRAY_SIZE(attrs_data)];

	struct mutex control_mutex;
	struct mutex data_mutex;

	u8 button_count;
	u16 *button_map;
	char input_phys[MAX_STR_LEN];
	struct input_dev *input;
};

static void rmi_f41_free_memory(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_41_data *f41 = fn_dev->data;
	int i;

	/* query */
	sysfs_remove_group(&fn_dev->dev.kobj, &attrs_query);
	/* control */
	for (i = 0; i < ARRAY_SIZE(attrs_ctrl); i++) {
		if (f41->attrs_ctrl_regs_exist[i])
			sysfs_remove_group(&fn_dev->dev.kobj, &attrs_ctrl[i]);
	}
	for (i = 0; i < ARRAY_SIZE(attrs_data); i++) {
		if (f41->attrs_data_regs_exist[i])
			sysfs_remove_group(&fn_dev->dev.kobj, &attrs_data[i]);
	}
}


static int rmi_f41_initialize(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_41_data *f41;
	struct rmi_device_platform_data *pdata;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	int retval = 0;
	u16 next_loc;
	int i;
	u8 *data_loc;

	f41 = devm_kzalloc(&fn_dev->dev, sizeof(struct rmi_fn_41_data),
				GFP_KERNEL);
	if (!f41) {
		dev_err(&fn_dev->dev, "Failed to allocate rmi_fn_41_data.\n");
		return -ENOMEM;
	}
	fn_dev->data = f41;

	/* Read F41 Query Data */
	f41->query.address = fn_dev->fd.query_base_addr;
	retval = rmi_read_block(fn_dev->rmi_dev, f41->query.address,
		(u8 *)&f41->query, sizeof(f41->query.regs));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not read query registers from 0x%04x\n",
				f41->query.address);
		return retval;
	}

	/* Initialize Control Data */
	next_loc = fn_dev->fd.control_base_addr;

	if (f41->query.has_reduced_reporting == 1 ||
			f41->query.has_modal_control == 1 ||
			f41->query.has_single_tap == 1 ||
			f41->query.has_z == 1 ||
			f41->query.has_serial_number == 1) {
		f41->control.reg_0 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f41_ap_control_0),
				GFP_KERNEL);
		if (!f41->control.reg_0) {
			dev_err(&fn_dev->dev, "Failed to allocate control register 0.");
			return -ENOMEM;
		}
		f41->control.reg_0->address = next_loc;
		next_loc += sizeof(f41->control.reg_0->regs);
		retval = rmi_read_block(fn_dev->rmi_dev,
				f41->control.reg_0->address,
				f41->control.reg_0->regs,
				sizeof(f41->control.reg_0->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "Could not read Control register 0 from 0x%04x\n",
					f41->control.reg_0->address);
			return retval;
		}
	}

	f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0a] =
					f41->query.has_reduced_reporting == 1;
	f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0b] =
					f41->query.has_modal_control == 1;
	f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0c] =
					f41->query.has_single_tap == 1;
	f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0d] =
					f41->query.has_z == 1;
	f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0e] =
					f41->query.has_serial_number == 1;

	if (f41->query.number_of_buttons > 0) {
		f41->control.reg_1 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f41_ap_control_1),
					GFP_KERNEL);
		if (!f41->control.reg_1) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		f41->control.reg_1->address = next_loc;
		next_loc += sizeof(f41->control.reg_1->regs);
		retval = rmi_read_block(fn_dev->rmi_dev,
				f41->control.reg_1->address,
				f41->control.reg_1->regs,
				sizeof(f41->control.reg_1->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "Could not read Control register 1 from 0x%04x\n",
						f41->control.reg_1->address);
			return retval;
		}
		f41->attrs_ctrl_regs_exist[f41_ctrl_reg_1] = true;
	}

	f41->control.reg_2__5 =	devm_kzalloc(&fn_dev->dev,
					sizeof(union f41_ap_control_2__5),
					GFP_KERNEL);
	if (!f41->control.reg_2__5) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers 2-5.");
		return -ENOMEM;
	}
	f41->control.reg_2__5->address = next_loc;
	next_loc += sizeof(f41->control.reg_2__5->regs);
	retval = rmi_read_block(fn_dev->rmi_dev, f41->control.reg_2__5->address,
				f41->control.reg_2__5->regs,
				sizeof(f41->control.reg_2__5->regs));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not read Control registers 2-5 from 0x%04x\n",
				f41->control.reg_2__5->address);
		return retval;
	}
	f41->attrs_ctrl_regs_exist[f41_ctrl_reg_2__5] = true;

	if (f41->query.has_reduced_reporting == 1) {
		f41->control.reg_6__9 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f41_ap_control_6__9), GFP_KERNEL);
		if (!f41->control.reg_6__9) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers 6-9.");
			return -ENOMEM;
		}
		f41->control.reg_6__9->address = next_loc;
		next_loc += sizeof(f41->control.reg_6__9->regs);
		retval = rmi_read_block(fn_dev->rmi_dev,
				f41->control.reg_6__9->address,
				f41->control.reg_6__9->regs,
				sizeof(f41->control.reg_6__9->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "Could not read Control registers 6-9 from 0x%04x\n",
						f41->control.reg_6__9->address);
			return retval;
		}
		f41->attrs_ctrl_regs_exist[f41_ctrl_reg_6__9] = true;
	}

	if (f41->query.has_single_tap == 1) {
		f41->control.reg_10 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f41_ap_control_10),
					GFP_KERNEL);
		if (!f41->control.reg_10) {
			dev_err(&fn_dev->dev, "Failed to allocate control register 10.");
			return -ENOMEM;
		}
		f41->control.reg_10->address = next_loc;
		next_loc += sizeof(f41->control.reg_10->regs);
		retval = rmi_read_block(fn_dev->rmi_dev,
					f41->control.reg_10->address,
					f41->control.reg_10->regs,
					sizeof(f41->control.reg_10->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "Could not read Control register 10 from 0x%04x\n",
						f41->control.reg_10->address);
			return retval;
		}
		f41->attrs_ctrl_regs_exist[f41_ctrl_reg_10] = true;
	}


	/* initialize data registers */

	f41->data_block = devm_kzalloc(&fn_dev->dev,
				       sizeof(union f41_ap_data_0__3)
					+ sizeof(union f41_ap_data_4)
					+ sizeof(union f41_ap_data_5)
					+ sizeof(union f41_ap_data_6)
					+ sizeof(union f41_ap_data_7__8)
					+ sizeof(union f41_ap_data_9)
					+ sizeof(union f41_ap_data_10)
					+ sizeof(union f41_ap_data_11__14),
				GFP_KERNEL);
	if (!f41->data_block) {
		dev_err(&fn_dev->dev, "Failed to allocate data registers.");
			return -ENOMEM;
	}
	data_loc = f41->data_block;
	/* data 0-3 */

	f41->data.reg_0__3 = (union f41_ap_data_0__3 *) data_loc;
	data_loc += sizeof(union f41_ap_data_0__3);
	f41->attrs_data_regs_exist[f41_data_reg_0__3] = true;

	/* data 4 */
	if (f41->query.has_force == 1) {
		f41->data.reg_4 = (union f41_ap_data_4 *) data_loc;
		data_loc += sizeof(union f41_ap_data_4);
		f41->attrs_data_regs_exist[f41_data_reg_4] = true;
	}

	/* data 5 */
	/* button map */
	/* call devm_kcalloc when it will be defined in the kernel */
	f41->button_map = devm_kzalloc(&fn_dev->dev,
		f41->query.number_of_buttons*sizeof(u16), GFP_KERNEL);
	if (!f41->button_map) {
		dev_err(&fn_dev->dev, "Failed to allocate button map.\n");
		return -ENOMEM;
	}
	if (f41->query.number_of_buttons > 0) {
		f41->data.reg_5 = (union f41_ap_data_5 *) data_loc;
		data_loc += sizeof(union f41_ap_data_5);
	}
	pdata = to_rmi_platform_data(rmi_dev);
	if (pdata) {
		if (!pdata->f41_button_map)
			dev_warn(&fn_dev->dev, "button_map is NULL");
		else if (!pdata->f41_button_map->map)
			dev_warn(&fn_dev->dev,
				 "Platformdata button map is missing!\n");
		else {
			if (pdata->f41_button_map->nbuttons !=
						f41->query.number_of_buttons)
				dev_warn(&fn_dev->dev,
					"Platformdata button map size (%d) != number of buttons on device (%d).\n",
					pdata->f41_button_map->nbuttons,
					f41->query.number_of_buttons);
			f41->button_count = min(
					(u8) f41->query.number_of_buttons,
					pdata->f41_button_map->nbuttons);
			for (i = 0; i < f41->button_count; i++)
				f41->button_map[i] =
					pdata->f41_button_map->map[i];
		}
	}

	/* data 6 */
	if (f41->query.has_z == 1 || f41->query.has_single_tap == 1
					|| f41->query.has_serial_number == 1) {
		f41->data.reg_6 = (union f41_ap_data_6 *) data_loc;
		data_loc += sizeof(union f41_ap_data_6);
	}
	f41->attrs_data_regs_exist[f41_data_reg_6a] =
					f41->query.has_single_tap == 1;
	f41->attrs_data_regs_exist[f41_data_reg_6b] = f41->query.has_z == 1;
	f41->attrs_data_regs_exist[f41_data_reg_6c] =
					f41->query.has_serial_number == 1;

	/* data 7-8 */
	if (f41->query.has_orientation == 1) {
		f41->data.reg_7__8 = (union f41_ap_data_7__8 *) data_loc;
		data_loc += sizeof(union f41_ap_data_7__8);
		f41->attrs_data_regs_exist[f41_data_reg_7__8] = true;
	}

	/* data 9 */
	if (f41->query.has_z == 1) {
		f41->data.reg_9 = (union f41_ap_data_9 *) data_loc;
		data_loc += sizeof(union f41_ap_data_9);
		f41->attrs_data_regs_exist[f41_data_reg_9] = true;
	}

	/* data 10 */
	if (f41->query.has_battery == 1) {
		f41->data.reg_10 = (union f41_ap_data_10 *) data_loc;
		data_loc += sizeof(union f41_ap_data_10);
		f41->attrs_data_regs_exist[f41_data_reg_10] = true;
	}

	/* data 11-14 */
	if (f41->query.has_serial_number > 0) {
		f41->data.reg_11__14 = (union f41_ap_data_11__14 *) data_loc;
		data_loc += sizeof(union f41_ap_data_11__14);
		f41->attrs_data_regs_exist[f41_data_reg_11__14] = true;
	}

	f41->data_block_size = data_loc - f41->data_block;
	mutex_init(&f41->control_mutex);
	mutex_init(&f41->data_mutex);
	return 0;
}


static int rmi_f41_create_sysfs(struct rmi_function_dev *fn_dev)
{
	u8 attr_num;
	struct rmi_fn_41_data *f41 = fn_dev->data;
	dev_dbg(&fn_dev->dev, "Creating F41 sysfs files.");

	/* Set up sysfs device attributes. */
	if (sysfs_create_group(&fn_dev->dev.kobj, &attrs_query) < 0) {
		dev_err(&fn_dev->dev, "Failed to create query sysfs files.");
		return -ENODEV;
	}
	for (attr_num = 0; attr_num < ARRAY_SIZE(attrs_ctrl); attr_num++) {
		if (f41->attrs_ctrl_regs_exist[attr_num]) {
			if (sysfs_create_group(&fn_dev->dev.kobj,
					&attrs_ctrl[attr_num]) < 0) {
				dev_err(&fn_dev->dev, "Failed to create sysfs file group for reg group %d.",
								attr_num);
				return -ENODEV;
			}
		}
	}
	for (attr_num = 0; attr_num < ARRAY_SIZE(attrs_data); attr_num++) {
		if (f41->attrs_data_regs_exist[attr_num]) {
			if (sysfs_create_group(&fn_dev->dev.kobj,
					&attrs_data[attr_num]) < 0) {
				dev_err(&fn_dev->dev, "Failed to create sysfs file group for reg group %d.",
								attr_num);
				return -ENODEV;
			}
		}
	}
	return 0;
}


static int rmi_f41_config(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_41_data *f41 = fn_dev->data;
	int retval;
	/* Write Control Register values back to device */
	if (f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0a]
		|| f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0b]
		|| f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0c]
		|| f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0d]
		|| f41->attrs_ctrl_regs_exist[f41_ctrl_reg_0e]) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					f41->control.reg_0->address,
					(u8 *)f41->control.reg_0,
					sizeof(f41->control.reg_0->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg0 to 0x%x\n",
					__func__, f41->control.reg_0->address);
			return retval;
		}
	}
	if (f41->attrs_ctrl_regs_exist[f41_ctrl_reg_1]) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					f41->control.reg_1->address,
					(u8 *)f41->control.reg_1,
					sizeof(f41->control.reg_1->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg1 to 0x%x\n",
					__func__, f41->control.reg_1->address);
			return retval;
		}
	}
	if (f41->attrs_ctrl_regs_exist[f41_ctrl_reg_2__5]) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					f41->control.reg_2__5->address,
					(u8 *)f41->control.reg_2__5,
					sizeof(f41->control.reg_2__5->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg2_5 to 0x%x\n",
					__func__,
					f41->control.reg_2__5->address);
			return retval;
		}
	}
	if (f41->attrs_ctrl_regs_exist[f41_ctrl_reg_6__9]) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					f41->control.reg_6__9->address,
					(u8 *)f41->control.reg_6__9,
					sizeof(f41->control.reg_6__9->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg6_9 to 0x%x\n",
					__func__,
					f41->control.reg_6__9->address);
			return retval;
		}
	}
	if (f41->attrs_ctrl_regs_exist[f41_ctrl_reg_10]) {
		retval = rmi_write_block(fn_dev->rmi_dev,
					f41->control.reg_10->address,
					(u8 *)f41->control.reg_10,
					sizeof(f41->control.reg_10->regs));
		if (retval < 0) {
			dev_err(&fn_dev->dev, "%s : Could not write reg10 to 0x%x\n",
					__func__, f41->control.reg_10->address);
			return retval;
		}
	}

	return 0;
}

static int rmi_f41_remove(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_41_data *f41 = fn_dev->data;

	input_unregister_device(f41->input);
	rmi_f41_free_memory(fn_dev);

	return 0;
}

/* sysfs functions */
/* Query */
simple_show_union_struct_unsigned(query, has_reduced_reporting)
simple_show_union_struct_unsigned(query, has_modal_control)
simple_show_union_struct_unsigned(query, has_force)
simple_show_union_struct_unsigned(query, has_orientation)
simple_show_union_struct_unsigned(query, has_serial_number)
simple_show_union_struct_unsigned(query, has_battery)
simple_show_union_struct_unsigned(query, has_z)
simple_show_union_struct_unsigned(query, has_single_tap)
simple_show_union_struct_unsigned(query, number_of_buttons)

/* Control */
show_store_union_struct_unsigned(control, reg_0, reporting_mode)
show_store_union_struct_unsigned(control, reg_0, modal_ctrl)
show_store_union_struct_unsigned(control, reg_0, single_tap_interrupt_enable)
show_store_union_struct_unsigned(control, reg_0, in_range_interrupt_enable)
show_store_union_struct_unsigned(control, reg_0, new_sn_interrupt_enable)
show_store_union_struct_unsigned(control, reg_1, button_1_interrupt_enable)
show_store_union_struct_unsigned(control, reg_1, button_2_interrupt_enable)
show_store_union_struct_unsigned(control, reg_1, button_3_interrupt_enable)
show_store_union_struct_unsigned(control, reg_1, button_4_interrupt_enable)
show_store_union_struct_unsigned(control, reg_1, button_5_interrupt_enable)
show_store_union_struct_unsigned(control, reg_1, button_6_interrupt_enable)
show_store_union_struct_unsigned(control, reg_1, button_7_interrupt_enable)
show_store_union_struct_unsigned(control, reg_2__5, max_x_position)
show_store_union_struct_unsigned(control, reg_2__5, max_y_position)
show_store_union_struct_unsigned(control,
					reg_6__9, x_reduced_reporting_distance)
show_store_union_struct_unsigned(control,
					reg_6__9, y_reduced_reporting_distance)
show_store_union_struct_unsigned(control, reg_10, max_tap_time)


/* Data */
simple_show_union_struct_unsigned2(data, reg_0__3, x_position)
simple_show_union_struct_unsigned2(data, reg_0__3, y_position)
simple_show_union_struct_unsigned2(data, reg_4, force)
simple_show_union_struct_unsigned2(data, reg_6, single_tap)
simple_show_union_struct_unsigned2(data, reg_6, in_range)
simple_show_union_struct_unsigned2(data, reg_6, new_serial_number)
simple_show_union_struct_unsigned2(data, reg_7__8, azimuth)
simple_show_union_struct_unsigned2(data, reg_7__8, altitude)
simple_show_union_struct_unsigned2(data, reg_9, z_position)
simple_show_union_struct_unsigned2(data, reg_10, battery_level)
simple_show_union_struct_unsigned2(data, reg_11__14, pen_serial_number)

static int rmi_f41_register_device(struct rmi_function_dev *fn_dev)
{
	int i;
	int rc;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_fn_41_data *f41 = fn_dev->data;
	struct rmi_driver *driver = fn_dev->rmi_dev->driver;
	struct input_dev *input_dev = input_allocate_device();

	if (!input_dev) {
		dev_err(&fn_dev->dev, "Failed to allocate input device.\n");
		return -ENOMEM;
	}

	f41->input = input_dev;
	if (driver->set_input_params) {
		rc = driver->set_input_params(rmi_dev, input_dev);
		if (rc < 0) {
			dev_err(&fn_dev->dev, "%s: Error in setting input device.\n",
			__func__);
			goto error_free_device;
		}
	}
	sprintf(f41->input_phys, "%s/input0", dev_name(&fn_dev->dev));
	input_dev->phys = f41->input_phys;
	input_dev->dev.parent = &rmi_dev->dev;
	input_set_drvdata(input_dev, f41);

	/* Set up any input events. */
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	/* set bits for each button... */
	for (i = 0; i < f41->button_count; i++)
		set_bit(f41->button_map[i], input_dev->keybit);
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

static int rmi_f41_attention(struct rmi_function_dev *fn_dev,
						unsigned long *irq_bits)
{
	int error;
	int button;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_fn_41_data *f41 = fn_dev->data;


	/* Read all the data registers. */
	error = rmi_read_block(rmi_dev, fn_dev->fd.data_base_addr,
			       f41->data_block, f41->data_block_size);
	if (error < 0) {
		dev_err(&fn_dev->dev, "Failed to read data registers.\n");
		return error;
	}

	/* Generate events for buttons that change state. */
	for (button = 0; button < f41->button_count; button++) {
		bool button_status;
		/* bit shift to get button's status */
		button_status =
			((f41->data.reg_5->regs[0] >> button) & 0x01) != 0;
		/* Generate an event here. */
		input_report_key(f41->input, f41->button_map[button],
				 button_status);
	}

	input_sync(f41->input); /* sync after groups of events */
	return 0;
}

static int rmi_f41_probe(struct rmi_function_dev *fn_dev)
{
	int retval = 0;

	dev_dbg(&fn_dev->dev, "Intializing F41.");

	retval = rmi_f41_initialize(fn_dev);
	if (retval < 0)
		goto error_exit;

	retval = rmi_f41_register_device(fn_dev);
	if (retval < 0)
		goto error_exit;

	retval = rmi_f41_create_sysfs(fn_dev);
	if (retval < 0)
		goto error_exit;

	return retval;

error_exit:
	rmi_f41_free_memory(fn_dev);

	return retval;
}

static struct rmi_function_driver function_driver = {
	.driver = {
		.name = "rmi_f41",
	},
	.func = FUNCTION_NUMBER,
	.probe = rmi_f41_probe,
	.remove = rmi_f41_remove,
	.config = rmi_f41_config,
	.attention = rmi_f41_attention,
};

module_rmi_function_driver(function_driver);

MODULE_AUTHOR("Daniel Rosenberg <daniel.rosenberg@synaptics.com>");
MODULE_DESCRIPTION("RMI F41 module");
MODULE_LICENSE("GPL");
