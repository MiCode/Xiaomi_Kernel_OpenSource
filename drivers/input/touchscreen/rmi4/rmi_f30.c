/*
 * Copyright (c) 2012 Synaptics Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#define FUNCTION_DATA rmi_fn_30_data
#define FNUM 30

#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include "rmi_driver.h"

#define MAX_LEN 256
#define FUNCTION_NUMBER 0x30

/* data specific to fn $30 that needs to be kept around */
union f30_query {
	struct {
		u8 extended_patterns:1;
		u8 has_mappable_buttons:1;
		u8 has_led:1;
		u8 has_gpio:1;
		u8 has_haptic:1;
		u8 has_gpio_driver_control:1;
		u8 reserved_1:2;
		u8 gpio_led_count:5;
		u8 reserved_2:3;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};

struct f30_gpio_ctrl_0n {
	u8 led_sel;
} __attribute__((__packed__));

struct f30_gpio_ctrl_0 {
	struct f30_gpio_ctrl_0n *regs;
	u16 address;
	u8 length;
};

union f30_gpio_ctrl_1 {
	struct {
		u8 gpio_debounce:1;
		u8 reserved:3;
		u8 halt:1;
		u8 halted:1;
		u8 reserved2:2;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

struct f30_gpio_ctrl_2n {
	u8 dir;
} __attribute__((__packed__));

struct f30_gpio_ctrl_2 {
	struct f30_gpio_ctrl_2n *regs;
	u16 address;
	u8 length;
};

struct f30_gpio_ctrl_3n {
	u8 gpiodata;
} __attribute__((__packed__));

struct f30_gpio_ctrl_3 {
	struct f30_gpio_ctrl_3n *regs;
	u16 address;
	u8 length;
};

struct f30_gpio_ctrl_4n {
	u8 led_act;
} __attribute__((__packed__));

struct f30_gpio_ctrl_4 {
	struct f30_gpio_ctrl_4n *regs;
	u16 address;
	u8 length;
};

struct f30_gpio_ctrl_5n {
	u8 ramp_period_a;
	u8 ramp_period_b;
} __attribute__((__packed__));

struct f30_gpio_ctrl_5 {
	struct f30_gpio_ctrl_5n *regs;
	u16 address;
	u8 length;
};

union f30_gpio_ctrl_6n {
	struct {
		u8 reserved:1;
		u8 SPCTRL:3;
		u8 STRPD:1;
		u8 reserved2:1;
		u8 STRPU:1;
		u8 reserved3:1;
	} __attribute__((__packed__));
	struct {
		u8 brightness:4;
		u8 pattern:4;
	} __attribute__((__packed__));
};

struct f30_gpio_ctrl_6 {
	union f30_gpio_ctrl_6n *regs;
	u16 address;
	u8 length;
};

struct f30_gpio_ctrl_7n {
	u8 capacity_btn_nbr:5;
	u8 valid:1;
	u8 invert:1;
	u8 open_drain:1;
} __attribute__((__packed__));

struct f30_gpio_ctrl_7 {
	struct f30_gpio_ctrl_7n *regs;
	u16 address;
	u8 length;
};

struct f30_gpio_ctrl_8n {
	u8 gpio_ctrl8_0;
	u8 gpio_ctrl8_1;
} __attribute__((__packed__));

struct f30_gpio_ctrl_8 {
	struct f30_gpio_ctrl_8n *regs;
	u16 address;
	u8 length;
};

union f30_gpio_ctrl_9 {
	struct {
		u8 haptic_duration;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

struct f30_control {
	struct f30_gpio_ctrl_0 *reg_0;
	union f30_gpio_ctrl_1 *reg_1;
	struct f30_gpio_ctrl_2 *reg_2;
	struct f30_gpio_ctrl_3 *reg_3;
	struct f30_gpio_ctrl_4 *reg_4;
	struct f30_gpio_ctrl_5 *reg_5;
	struct f30_gpio_ctrl_6 *reg_6;
	struct f30_gpio_ctrl_7 *reg_7;
	struct f30_gpio_ctrl_8 *reg_8;
	union f30_gpio_ctrl_9 *reg_9;
};

struct f30_data_0n {
	u8 gpi_led_data:1;
};

struct f30_data_0 {
	struct f30_data_0n *regs;
	u16 address;
	u8 length;
};

struct f30_data {
	struct f30_data_0 *datareg_0;
	u16 address;
	u8 length;
};

struct rmi_fn_30_data {
	union f30_query query;
	struct f30_data data;
	struct f30_control control;
	u8 gpioled_count;
	u8 gpioled_bitmask_size;
	u8 gpioled_byte_size;
	u8 *button_data_buffer;
	u8 button_bitmask_size;
	u16 *gpioled_map;
	char input_phys[MAX_LEN];
	struct input_dev *input;
	struct mutex control_mutex;
	struct mutex data_mutex;
	struct gpio_chip gpio;
	struct mutex gpio_mutex;
};

/* Query sysfs files */
show_union_struct_prototype(extended_patterns)
show_union_struct_prototype(has_mappable_buttons)
show_union_struct_prototype(has_led)
show_union_struct_prototype(has_gpio)
show_union_struct_prototype(has_haptic)
show_union_struct_prototype(has_gpio_driver_control)
show_union_struct_prototype(gpio_led_count)

static struct attribute *attrs1[] = {
	attrify(extended_patterns),
	attrify(has_mappable_buttons),
	attrify(has_led),
	attrify(has_gpio),
	attrify(has_haptic),
	attrify(has_gpio_driver_control),
	attrify(gpio_led_count),
	NULL
};

static struct attribute_group attrs_query = GROUP(attrs1);

/* Control sysfs files */

show_store_union_struct_prototype(led_sel)
show_store_union_struct_prototype(gpio_debounce)
show_store_union_struct_prototype(halt)
show_store_union_struct_prototype(halted)
show_store_union_struct_prototype(dir)
show_store_union_struct_prototype(gpiodata)
show_store_union_struct_prototype(led_act)
show_store_union_struct_prototype(ramp_period_a)
show_store_union_struct_prototype(ramp_period_b)
show_store_union_struct_prototype(SPCTRL)
show_store_union_struct_prototype(STRPD)
show_store_union_struct_prototype(STRPU)
show_store_union_struct_prototype(brightness)
show_store_union_struct_prototype(pattern)

show_store_union_struct_prototype(capacity_btn_nbr)
show_store_union_struct_prototype(valid)
show_store_union_struct_prototype(invert)
show_store_union_struct_prototype(open_drain)
show_store_union_struct_prototype(gpio_ctrl8_0)
show_store_union_struct_prototype(gpio_ctrl8_1)
show_store_union_struct_prototype(haptic_duration)

/* Data sysfs files */
show_store_union_struct_prototype(gpi_led_data)

/* gpio get and set */
static void rmi_f30_gpio_data_set(struct gpio_chip *gc, unsigned nr, int val)
{
	int reg_val = 32;
	struct rmi_function_dev *fn_dev = container_of(gc,
						struct rmi_fn_30_data, gpio);
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	//struct input_dev *input_dev;
	struct rmi_fn_30_data *f30 = fn_dev->data;
	int gpio_led_cnt = f30->query.gpio_led_count;
	int bytecnt = gpio_led_cnt / 7 + 1;
	//int regs_size = 0;
	int rc;

	mutex_lock(&f30->gpio_mutex);
	rc = rmi_read_block(rmi_dev, f30->data.datareg_0->address,
				(u8 *)reg_val, bytecnt);
	if (rc < 0) {
		dev_err(&fn_dev->dev,
		"Could not read query registers from 0x%04x\n",
		f30->data.datareg_0->address);
		mutex_unlock(&f30->gpio_mutex);
		return;
	}

	if (val)
		reg_val |= (1 << nr);
	else
		reg_val &= ~(1 << nr);

	/* Write gpio data Register value */
	rc = rmi_write_block(rmi_dev, f30->data.datareg_0->address,
				(u8 *)reg_val, bytecnt);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not read control 0 to 0x%x\n",
				__func__, rc, f30->control.reg_0->address);
	}

	mutex_unlock(&f30->gpio_mutex);
}

static int rmi_f30_gpio_data_get(struct gpio_chip *gc, unsigned nr)
{
	struct rmi_function_dev *fn_dev = container_of(gc,
						struct rmi_fn_30_data, gpio);
	struct rmi_fn_30_data *f30 = fn_dev->data;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	int gpio_led_cnt = f30->query.gpio_led_count;
	int bytecnt = gpio_led_cnt / 7 + 1;
	int gpio_offset = (nr + 7) / 8;
	int gpio_bit = nr % 8;

	rmi_read_block(rmi_dev, f30->data.datareg_0->address,
				(u8 *)f30->data.datareg_0, bytecnt);
	int dataVal = f30->data.datareg_0->regs[gpio_offset].gpi_led_data
							& (1 << gpio_bit);
	return dataVal;

}

static int rmi_f30_gpio_data_direction_in(struct gpio_chip *gc,
					  unsigned gpio_num)
{
//When switching a pin's direction from output to input, write 0 to DirN
//followed by writing 1 to DataN to force pull up
//nneds to check data setting accuracy.

	struct rmi_function_dev *fn_dev = container_of(gc,
						struct rmi_fn_30_data, gpio);
	struct rmi_fn_30_data *f30 = fn_dev->data;
	struct f30_control *control = &f30->control;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	f30->gpioled_count = f30->query.gpio_led_count;
	u8 reg = sizeof(u8)*(f30->gpioled_count + 7) / 8;
	u8 mask = 1 << (gpio_num % 8);
	int bit_no = gpio_num % 8;
	int retval;

	mutex_lock(&f30->gpio_mutex);
	//Need to set dirN bit of the ctrl_reg2_dirN
	retval = rmi_write_block(rmi_dev, control->reg_2->address,
			(u8 *)control->reg_2->regs, control->reg_2->length);

	set_bit(gpio_num, control->reg_2->address+reg);
	//how about setting ctrl_reg2_dataN?
	rmi_f30_gpio_data_set(gc, gpio_num, 1);
	mutex_unlock(&f30->gpio_mutex);
	return 0;
}

static int rmi_f30_gpio_data_direction_out(struct gpio_chip *gc,
					unsigned gpio_num, int val)
{
	// When switching a pin's direction from input to output,
	// write DataN followed by DirN(1).
	struct rmi_function_dev *fn_dev = container_of(gc,
						struct rmi_fn_30_data, gpio);
	struct rmi_fn_30_data *f30 = fn_dev->data;
	struct f30_control *control = &f30->control;
	u8 curr_dirs;
	unsigned short bit;
	f30->gpioled_count = f30->query.gpio_led_count;
	u8 offset = sizeof(u8)*(gpio_num + 7) / 8;
	u16 gpio_ba = f30->data.datareg_0->address;

	mutex_lock(&f30->gpio_mutex);
	//Need to set dirN bit of the ctrl_reg2_dirN

	//Set dataN
	rmi_f30_gpio_data_set(gc, gpio_num, val);
	bit = gpio_num % 8;

	curr_dirs = inb(gpio_ba + offset);
	if (curr_dirs & (1 << bit))
		outb(curr_dirs & ~(1 << bit), gpio_ba + offset);

	mutex_unlock(&f30->gpio_mutex);
	return 0;
}

static struct gpio_chip rmi_f30_gpio_data_core = {
	.label			= "rmi_f30_gpio_data_core",
	.owner			= THIS_MODULE,
	.direction_input	= rmi_f30_gpio_data_direction_in,
	.get			= rmi_f30_gpio_data_get,
	.direction_output	= rmi_f30_gpio_data_direction_out,
	.set			= rmi_f30_gpio_data_set,
};


static struct attribute *attrs_ctrl_reg_0[] = {
	attrify(led_sel),
	NULL
};

static struct attribute *attrs_ctrl_reg_1[] = {
	attrify(gpio_debounce),
	attrify(halt),
	attrify(halted),
	NULL
};

static struct attribute *attrs_ctrl_reg_2[] = {
	attrify(dir),
	NULL
};

static struct attribute *attrs_ctrl_reg_3[] = {
	attrify(gpiodata),
	NULL
};

static struct attribute *attrs_ctrl_reg_4[] = {
	attrify(led_act),
	NULL
};

static struct attribute *attrs_ctrl_reg_5[] = {
	attrify(ramp_period_a),
	attrify(ramp_period_b),
	NULL
};

static struct attribute *attrs_ctrl_reg_6_gpio[] = {
	attrify(SPCTRL),
	attrify(STRPD),
	attrify(STRPU),
	NULL
};

static struct attribute *attrs_ctrl_reg_6_led[] = {
	attrify(brightness),
	attrify(pattern),
	NULL
};

static struct attribute *attrs_ctrl_reg_7[] = {
	attrify(capacity_btn_nbr),
	attrify(valid),
	attrify(invert),
	attrify(open_drain),
	NULL
};

static struct attribute *attrs_ctrl_reg_8[] = {
	attrify(gpio_ctrl8_0),
	attrify(gpio_ctrl8_1),
	NULL
};

static struct attribute *attrs_ctrl_reg_9[] = {
	attrify(haptic_duration),
	NULL
};

static struct attribute_group attrs_ctrl_regs[] = {
	GROUP(attrs_ctrl_reg_0),
	GROUP(attrs_ctrl_reg_1),
	GROUP(attrs_ctrl_reg_2),
	GROUP(attrs_ctrl_reg_3),
	GROUP(attrs_ctrl_reg_4),
	GROUP(attrs_ctrl_reg_5),
	GROUP(attrs_ctrl_reg_6_gpio),
	GROUP(attrs_ctrl_reg_6_led),
	GROUP(attrs_ctrl_reg_7),
	GROUP(attrs_ctrl_reg_8),
	GROUP(attrs_ctrl_reg_9),
};

bool f30_attrs_regs_exist[ARRAY_SIZE(attrs_ctrl_regs)];

static struct attribute *attrs_gpileddata[] = {
	attrify(gpi_led_data),
	NULL
};

static struct attribute_group attrs_data = GROUP(attrs_gpileddata);

int rmi_f30_read_control_parameters(struct rmi_device *rmi_dev,
	struct rmi_fn_30_data *f30)
{
	int retval = 0;
	struct f30_control *control = &f30->control;

	retval = rmi_read_block(rmi_dev, control->reg_0->address,
			(u8 *)control->reg_0->regs,
			control->reg_0->length);
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg0 to 0x%x\n",
			__func__, control->reg_0->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_1->address,
			(u8 *)control->reg_1->regs,
			sizeof(control->reg_1->regs));
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg1 to 0x%x\n",
			 __func__, control->reg_1->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_2->address,
			(u8 *)control->reg_2->regs, control->reg_2->length);
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg_2 to 0x%x\n",
			 __func__, control->reg_2->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_3->address,
			(u8 *)control->reg_3->regs, control->reg_3->length);
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg_3 to 0x%x\n",
			 __func__, control->reg_3->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_4->address,
			(u8 *)control->reg_4->regs, control->reg_4->length);
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg4 to 0x%x\n",
			 __func__, control->reg_4->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_5->address,
			(u8 *)control->reg_5->regs,
			control->reg_5->length);
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg5 to 0x%x\n", __func__,
			control->reg_5->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_6->address,
			(u8 *)control->reg_6->regs,
			control->reg_6->length);
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg6 to 0x%x\n", __func__,
		control->reg_6->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_7->address,
		(u8 *)control->reg_7->regs,
		control->reg_7->length);
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg7 to 0x%x\n", __func__,
		control->reg_7->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_8->address,
		(u8 *)control->reg_8->regs,
		control->reg_8->length);
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg8 to 0x%x\n", __func__,
		control->reg_8->address);
		return retval;
	}

	retval = rmi_read_block(rmi_dev, control->reg_9->address,
		(u8 *)control->reg_9->regs,
		sizeof(control->reg_9->regs));
	if (retval < 0) {
		dev_err(&rmi_dev->dev,
			"%s : Could not read control reg9 to 0x%x\n", __func__,
		control->reg_9->address);
		return retval;
	}
	return 0;
}

static inline int rmi_f30_alloc_memory(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_30_data *f30;
	int retval;

	f30 = devm_kzalloc(&fn_dev->dev, sizeof(struct rmi_fn_30_data),
			   GFP_KERNEL);
	if (!f30) {
		dev_err(&fn_dev->dev, "Failed to allocate rmi_fn_30_data.\n");
		return -ENOMEM;
	}
	fn_dev->data = f30;

	retval = rmi_read_block(fn_dev->rmi_dev,
						fn_dev->fd.query_base_addr,
						f30->query.regs,
					ARRAY_SIZE(f30->query.regs));

	if (retval < 0) {
		dev_err(&fn_dev->dev, "Failed to read query register.\n");
		return retval;
	}

	f30->gpioled_count = f30->query.gpio_led_count;
	f30->button_bitmask_size = sizeof(u8)*(f30->gpioled_count + 7) / 8;
	f30->button_data_buffer =
	    devm_kzalloc(&fn_dev->dev, f30->button_bitmask_size, GFP_KERNEL);
	if (!f30->button_data_buffer) {
		dev_err(&fn_dev->dev, "Failed to allocate button data buffer.\n");
		return -ENOMEM;
	}

	f30->gpioled_map = devm_kzalloc(&fn_dev->dev,
		f30->gpioled_count*sizeof(u16), GFP_KERNEL);
	if (!f30->gpioled_map) {
		dev_err(&fn_dev->dev, "Failed to allocate button map.\n");
		return -ENOMEM;
	}
	return 0;
}

static inline void rmi_f30_free_memory(struct rmi_function_dev *fn_dev)
{
	u8 reg_num = 0;
	sysfs_remove_group(&fn_dev->dev.kobj, &attrs_query);
	sysfs_remove_group(&fn_dev->dev.kobj, &attrs_data);
	for (reg_num = 0; reg_num < ARRAY_SIZE(attrs_ctrl_regs); reg_num++)
		sysfs_remove_group(&fn_dev->dev.kobj,
				   &attrs_ctrl_regs[reg_num]);
}

int rmi_f30_attention(struct rmi_function_dev *fn_dev,
					unsigned long *irq_bits)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	int data_base_addr = fn_dev->fd.data_base_addr;
	struct rmi_fn_30_data *f30 = fn_dev->data;
	int error;
	int gpiled;
	bool gpiled_status = false;
	int status = 0;

	/* Read the button data. */

	error = rmi_read_block(rmi_dev, data_base_addr,
			f30->button_data_buffer,
			f30->button_bitmask_size);
	if (error < 0) {
		dev_err(&fn_dev->dev,
			"%s: Failed to read button data registers.\n",
			__func__);
		return error;
	}

	/* Read the gpi led data. */
	f30->data.address = fn_dev->fd.data_base_addr;
	error = rmi_read_block(fn_dev->rmi_dev, f30->data.address,
		(u8 *)&f30->data, f30->gpioled_count);

	if (error < 0) {
		dev_err(&fn_dev->dev, "%s: Failed to read f30 data registers.\n",
			__func__);
		return error;
	}
	/* Generate events for buttons that change state. */
	for (gpiled = 0; gpiled < f30->gpioled_count; gpiled++) {
		status = f30->data.datareg_0->regs[gpiled].gpi_led_data;
		dev_warn(&fn_dev->dev,
			"rmi_f30 attention gpiled=%d data status=%d\n",
			gpiled,
			f30->data.datareg_0->regs[gpiled].gpi_led_data);
		/* check if gpio */
		if (!(f30->control.reg_0->regs[gpiled].led_sel)) {
			if (f30->control.reg_2->regs[gpiled].dir == 0) {
				gpiled_status = status != 0;

		/* if the gpiled data state changed from the
		* last time report it and store the new state */
		/* Generate an event here. */
			dev_warn(&fn_dev->dev,
			"rmi_f30 attention call input_report_key\n");
			input_report_key(f30->input,
				f30->data.datareg_0->regs[gpiled].gpi_led_data,
				gpiled_status);
			}
		}
	}
	input_sync(f30->input); /* sync after groups of events */
	return 0;
}

static int rmi_f30_register_device(struct rmi_function_dev *fn_dev)
{
	int i;
	int rc;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_fn_30_data *f30 = fn_dev->data;
	struct rmi_driver *driver = fn_dev->rmi_dev->driver;
	struct input_dev *input_dev = input_allocate_device();

	if (!input_dev) {
		dev_err(&fn_dev->dev, "Failed to allocate input device.\n");
		return -ENOMEM;
	}

	f30->input = input_dev;

	if (driver->set_input_params) {
		rc = driver->set_input_params(rmi_dev, input_dev);
		if (rc < 0) {
			dev_err(&fn_dev->dev, "%s: Error in setting input device.\n",
			__func__);
			goto error_free_device;
		}
	}
	sprintf(f30->input_phys, "%s/input0", dev_name(&fn_dev->dev));
	input_dev->phys = f30->input_phys;
	input_dev->dev.parent = &rmi_dev->dev;
	input_set_drvdata(input_dev, f30);

	/* Set up any input events. */
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	input_dev->keycode = f30->gpioled_map;
	input_dev->keycodesize = 1;
	input_dev->keycodemax = f30->gpioled_count;
	/* set bits for each qpio led pin... */
	for (i = 0; i < f30->gpioled_count; i++) {
		set_bit(f30->gpioled_map[i], input_dev->keybit);
		input_set_capability(input_dev, EV_KEY, f30->gpioled_map[i]);
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

static int rmi_f30_config(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_30_data *data = fn_dev->data;
	int gpio_led_cnt = data->query.gpio_led_count;
	int bytecnt = gpio_led_cnt / 7 + 1;
	int regs_size = 0;
	int rc;
	/* repeated register functions */

	/* Write Control Register values back to device */
	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_0->address,
				(u8 *)data->control.reg_0,
				bytecnt * sizeof(struct f30_gpio_ctrl_0n));
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 0 to 0x%x\n",
				__func__, rc, data->control.reg_0->address);
		return rc;
	}

	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_1->address,
			(u8 *) data->control.reg_1->regs,
			sizeof(union f30_gpio_ctrl_1));
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 1 to 0x%x\n",
				__func__, rc, data->control.reg_1->address);
		return rc;
	}

	regs_size = data->control.reg_2->length;

	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_2->address,
			(u8 *) data->control.reg_2->regs,
			regs_size);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 2 to 0x%x\n",
				__func__, rc, data->control.reg_2->address);
		return rc;
	}

	regs_size = data->control.reg_3->length;

	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_3->address,
			(u8 *) data->control.reg_3->regs,
			regs_size);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 3 to 0x%x\n",
				__func__, rc, data->control.reg_3->address);
		return rc;
	}

	regs_size = data->control.reg_4->length;
	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_4->address,
			(u8 *) data->control.reg_4->regs,
			regs_size);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 4 to 0x%x\n",
				__func__, rc, data->control.reg_4->address);
		return rc;
	}

	regs_size = data->control.reg_5->length;
	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_5->address,
			(u8 *) data->control.reg_5->regs,
			regs_size);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 5 to 0x%x\n",
				__func__, rc, data->control.reg_5->address);
		return rc;
	}

	regs_size = data->control.reg_6->length;
	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_6->address,
			(u8 *) data->control.reg_6->regs,
			regs_size);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 6 to 0x%x\n",
				__func__, rc, data->control.reg_6->address);
		return rc;
	}

	regs_size = data->control.reg_7->length;
	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_7->address,
			(u8 *) data->control.reg_7->regs,
			regs_size);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 7 to 0x%x\n",
			__func__, rc, data->control.reg_7->address);
		return rc;
	}

	regs_size = data->control.reg_8->length;
	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_8->address,
			(u8 *) data->control.reg_8->regs, regs_size);
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 9 to 0x%x\n",
			__func__, rc, data->control.reg_8->address);
		return rc;
	}

	rc = rmi_write_block(fn_dev->rmi_dev, data->control.reg_9->address,
			(u8 *) data->control.reg_9->regs,
			sizeof(union f30_gpio_ctrl_9));
	if (rc < 0) {
		dev_err(&fn_dev->dev, "%s error %d: Could not write control 9 to 0x%x\n",
				__func__, rc, data->control.reg_9->address);
		return rc;
	}

	return 0;
}

static inline int rmi_f30_initialize(struct rmi_function_dev *fn_dev)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_device_platform_data *pdata;
	struct rmi_fn_30_data *instance_data = fn_dev->data;

	int retval = 0;
	u16 next_loc;
	int  gpio_led_cnt = 0;
	int regs_size = 0;
	u8 reg_num = 0;
	int reg_flg;
	int hasgpio, hasled, hasmbtn, hashaptic;
	struct f30_control *control = &instance_data->control;

	/* Read F30 Query Data */
	instance_data->query.address = fn_dev->fd.query_base_addr;
	retval = rmi_read_block(fn_dev->rmi_dev, instance_data->query.address,
		(u8 *)&instance_data->query, sizeof(instance_data->query));
	if (retval < 0) {
		dev_err(&fn_dev->dev,
		"Could not read query registers from 0x%04x\n",
		instance_data->query.address);
		return retval;
	}

	/* initialize gpioled_map data */
	hasgpio = instance_data->query.has_gpio;
	hasled = instance_data->query.has_led;
	hasmbtn = instance_data->query.has_mappable_buttons;
	hashaptic = instance_data->query.has_haptic ;
	gpio_led_cnt = instance_data->query.gpio_led_count;

	pdata = to_rmi_platform_data(rmi_dev);
	if (pdata) {
		if (!pdata->gpioled_map) {
			dev_warn(&fn_dev->dev,
			"%s - gpioled_map is NULL", __func__);
		} else if (pdata->gpioled_map->ngpioleds != gpio_led_cnt) {
			dev_warn(&fn_dev->dev,
				"Platformdata gpioled map size (%d) != number of buttons on device (%d) - ignored\n",
				pdata->gpioled_map->ngpioleds, gpio_led_cnt);
		} else if (!pdata->gpioled_map->map) {
			dev_warn(&fn_dev->dev,
				 "Platformdata button map is missing!\n");
		} else {
			int i;
			for (i = 0; i < pdata->gpioled_map->ngpioleds; i++)
				instance_data->gpioled_map[i] =
					pdata->gpioled_map->map[i];
		}
	}

	/* Initialize Control Data */

	next_loc = fn_dev->fd.control_base_addr;

	/* calculate reg size */

	instance_data->gpioled_bitmask_size = sizeof(u8)*(gpio_led_cnt + 7) / 8;
	instance_data->gpioled_byte_size = sizeof(u8)*gpio_led_cnt;

	/* reg_0 */
	control->reg_0 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_gpio_ctrl_0), GFP_KERNEL);
	if (!control->reg_0) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}

	if (hasgpio && hasled)
		reg_flg = 1;

	f30_attrs_regs_exist[reg_num] = true;
	regs_size = max(sizeof(struct f30_gpio_ctrl_0n) * reg_flg *
					instance_data->gpioled_bitmask_size, 1);
	control->reg_0->regs = devm_kzalloc(&fn_dev->dev, regs_size,
					    GFP_KERNEL);

	if (!control->reg_0->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_0->address = next_loc;
	control->reg_0->length = regs_size;
	next_loc += regs_size;
	reg_num++;

	/* reg_1 */
	control->reg_1 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f30_gpio_ctrl_1), GFP_KERNEL);
	if (!control->reg_1) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	f30_attrs_regs_exist[reg_num] = true;
	reg_num++;
	instance_data->control.reg_1->address = next_loc;
	next_loc += regs_size;

	/* reg_2 */
	instance_data->control.reg_2 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_gpio_ctrl_2), GFP_KERNEL);
	if (!control->reg_2) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}

	reg_flg = hasgpio;
	f30_attrs_regs_exist[reg_num] = true;
	regs_size = max(sizeof(struct f30_gpio_ctrl_2n)*reg_flg*
					instance_data->gpioled_bitmask_size, 1);
	control->reg_2->regs = devm_kzalloc(&fn_dev->dev, regs_size,
					    GFP_KERNEL);

	if (!control->reg_2->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_2->address = next_loc;
	control->reg_2->length = regs_size;
	next_loc += regs_size;
	reg_num++;

	/* reg_3 */
	instance_data->control.reg_3 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_gpio_ctrl_3), GFP_KERNEL);
	if (!control->reg_3) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}

	reg_flg = hasgpio;
	f30_attrs_regs_exist[reg_num] = true;
	regs_size = max(sizeof(struct f30_gpio_ctrl_3n) * reg_flg *
					instance_data->gpioled_bitmask_size, 1);
	control->reg_3->regs = devm_kzalloc(&fn_dev->dev, regs_size,
					    GFP_KERNEL);

	if (!control->reg_3->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_3->address = next_loc;
	control->reg_3->length = regs_size;
	next_loc += regs_size;
	reg_num++;

	/* reg_4 */
	control->reg_4 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_gpio_ctrl_4), GFP_KERNEL);
	if (!control->reg_4) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}

	reg_flg = hasled;
	f30_attrs_regs_exist[reg_num] = true;
	regs_size = max(sizeof(struct f30_gpio_ctrl_4n)*reg_flg*
					instance_data->gpioled_bitmask_size,
					sizeof(struct f30_gpio_ctrl_4n));
	control->reg_4->regs = devm_kzalloc(&fn_dev->dev, regs_size,
					    GFP_KERNEL);
	if (!control->reg_4->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_4->address = next_loc;
	control->reg_4->length = regs_size;
	next_loc += regs_size;
	reg_num++;

	/* reg_5 */
	control->reg_5 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_gpio_ctrl_5), GFP_KERNEL);
	if (!control->reg_5) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}

	reg_flg = hasled;
	f30_attrs_regs_exist[reg_num] = true;
	regs_size = max(6 * reg_flg, 2);
	control->reg_5->regs = devm_kzalloc(&fn_dev->dev, regs_size,
					    GFP_KERNEL);
	if (!control->reg_5->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_5->address = next_loc;
	control->reg_5->length = regs_size;
	next_loc += regs_size;
	reg_num++;

	/* reg_6 */
	control->reg_6 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_gpio_ctrl_6), GFP_KERNEL);
	if (!control->reg_6) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	reg_flg = hasled || (!hasled
		&& instance_data->query.has_gpio_driver_control);

	regs_size = max(sizeof(union f30_gpio_ctrl_6n)*reg_flg*gpio_led_cnt,
					sizeof(union f30_gpio_ctrl_6n));
	if (!hasled
		&& instance_data->query.has_gpio_driver_control)
		f30_attrs_regs_exist[reg_num] = true;

	reg_num++;
	if (hasled)
		f30_attrs_regs_exist[reg_num] = true;

	reg_num++;

	control->reg_6->regs = devm_kzalloc(&fn_dev->dev, regs_size,
					    GFP_KERNEL);
	if (!control->reg_6->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_6->address = next_loc;
	control->reg_6->length = regs_size;
	next_loc += regs_size;

	/* reg_7 */
	reg_flg = hasmbtn;
	control->reg_7 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_gpio_ctrl_7), GFP_KERNEL);
	if (!control->reg_7) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	if (hasmbtn)
		regs_size = sizeof(struct f30_gpio_ctrl_7n)*gpio_led_cnt;
	else
		regs_size = sizeof(struct f30_gpio_ctrl_7n);

	regs_size = max(sizeof(struct f30_gpio_ctrl_7n)*reg_flg*
					gpio_led_cnt,
					sizeof(struct f30_gpio_ctrl_7n));
	f30_attrs_regs_exist[reg_num] = true;
	control->reg_7->regs = devm_kzalloc(&fn_dev->dev, regs_size,
					    GFP_KERNEL);
	if (!control->reg_7->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_7->address = next_loc;
	control->reg_7->length = regs_size;
	next_loc += regs_size;
	reg_num++;

	/* reg_8 */
	control->reg_8 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_gpio_ctrl_8), GFP_KERNEL);
	if (!control->reg_8) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}

	regs_size = max(sizeof(struct f30_gpio_ctrl_8n)*hashaptic*
					gpio_led_cnt,
					sizeof(struct f30_gpio_ctrl_8n));
	f30_attrs_regs_exist[reg_num] = true;
	control->reg_8->regs =
			devm_kzalloc(&fn_dev->dev, regs_size, GFP_KERNEL);
	if (!control->reg_8->regs) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_8->address = next_loc;
	control->reg_8->length = regs_size;
	next_loc += regs_size;
	reg_num++;

	/* reg_9 */
	control->reg_9 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f30_gpio_ctrl_9), GFP_KERNEL);
	if (!control->reg_9) {
		dev_err(&fn_dev->dev, "Failed to allocate control register.");
		return -ENOMEM;
	}
	if (instance_data->query.has_haptic)
		f30_attrs_regs_exist[reg_num] = true;
	control->reg_9->address = next_loc;
	next_loc += sizeof(control->reg_9->regs);

	/* data reg_0 */
	instance_data->data.datareg_0 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f30_data_0), GFP_KERNEL);
	if (!instance_data->data.datareg_0) {
		dev_err(&fn_dev->dev, "Failed to allocate control register.");
		return -ENOMEM;
	}

	regs_size = sizeof(struct f30_data_0n)*
				instance_data->gpioled_byte_size;
	instance_data->data.datareg_0->address = fn_dev->fd.data_base_addr;
	next_loc += sizeof(instance_data->data.datareg_0->regs);

	retval = rmi_f30_read_control_parameters(rmi_dev, instance_data);
	if (retval < 0) {
		dev_err(&fn_dev->dev,
			"Failed to initialize F19 control params.\n");
		return retval;
	}

	mutex_init(&instance_data->control_mutex);
	mutex_init(&instance_data->data_mutex);
	return 0;
}

static int rmi_f30_create_sysfs(struct rmi_function_dev *fn_dev)
{
	u8 reg_num;

	dev_dbg(&fn_dev->dev, "Creating sysfs files.");

	/* Set up sysfs device attributes. */
	if (sysfs_create_group(&fn_dev->dev.kobj, &attrs_query) < 0) {
		dev_err(&fn_dev->dev, "Failed to create query sysfs files.");
		return -ENODEV;
	}
	if (sysfs_create_group(&fn_dev->dev.kobj, &attrs_data) < 0) {
		dev_err(&fn_dev->dev, "Failed to create data sysfs files.");
		return -ENODEV;
	}

	for (reg_num = 0; reg_num < ARRAY_SIZE(attrs_ctrl_regs);
		reg_num++) {
		if (f30_attrs_regs_exist[reg_num]) {
			if (sysfs_create_group(&fn_dev->dev.kobj,
					&attrs_ctrl_regs[reg_num]) < 0) {
				dev_err(&fn_dev->dev, "Failed to create sysfs file group for reg group %d.",
					reg_num);
				return -ENODEV;
			}

		}
	}

	return 0;
}

static int rmi_f30_probe(struct rmi_function_dev *fn_dev)
{
	int rc;
	struct rmi_fn_30_data *f30 = fn_dev->data;

	rc = rmi_f30_alloc_memory(fn_dev);
	if (rc < 0)
		goto error_exit;

	rc = rmi_f30_initialize(fn_dev);
	if (rc < 0)
		goto error_exit;

	rc = rmi_f30_register_device(fn_dev);
	if (rc < 0)
		goto error_exit;

	rc = rmi_f30_create_sysfs(fn_dev);
	if (rc < 0)
		goto error_uregister_exit;
	return 0;

error_uregister_exit:
	input_unregister_device(f30->input);

error_exit:
	rmi_f30_free_memory(fn_dev);

	return rc;

}

static int rmi_f30_remove(struct rmi_function_dev *fn_dev)
{
	rmi_f30_free_memory(fn_dev);
	return 0;
}

static struct rmi_function_driver function_driver = {
	.driver = {
		.name = "rmi_f30",
		.remove = f30_remove_device,
	},
	.func = FUNCTION_NUMBER,
	.probe = rmi_f30_probe,
	.remove = rmi_f30_remove,
	.config = rmi_f30_config,
	.attention = rmi_f30_attention,
};


/* sysfs functions */
/* Query */
simple_show_union_struct_unsigned(query, extended_patterns)
simple_show_union_struct_unsigned(query, has_mappable_buttons)
simple_show_union_struct_unsigned(query, has_led)
simple_show_union_struct_unsigned(query, has_gpio)
simple_show_union_struct_unsigned(query, has_haptic)
simple_show_union_struct_unsigned(query, has_gpio_driver_control)
simple_show_union_struct_unsigned(query, gpio_led_count)

/* Control */
show_store_union_struct_unsigned(control, reg_1, gpio_debounce)
show_store_union_struct_unsigned(control, reg_1, halt)
show_store_union_struct_unsigned(control, reg_1, halted)
show_store_union_struct_unsigned(control, reg_9, haptic_duration)

/* repeated register functions */
show_store_repeated_union_struct_unsigned(control, reg_0, led_sel)
show_store_repeated_union_struct_unsigned(control, reg_2, dir)
show_store_repeated_union_struct_unsigned(control, reg_3, gpiodata)
show_store_repeated_union_struct_unsigned(control, reg_4, led_act)
show_store_repeated_union_struct_unsigned(control, reg_5, ramp_period_a)
show_store_repeated_union_struct_unsigned(control, reg_5, ramp_period_b)
show_store_repeated_union_struct_unsigned(control, reg_6, SPCTRL)
show_store_repeated_union_struct_unsigned(control, reg_6, STRPD)
show_store_repeated_union_struct_unsigned(control, reg_6, STRPU)
show_store_repeated_union_struct_unsigned(control, reg_6, brightness)
show_store_repeated_union_struct_unsigned(control, reg_6, pattern)
show_store_repeated_union_struct_unsigned(control, reg_7, capacity_btn_nbr)
show_store_repeated_union_struct_unsigned(control, reg_7, valid)
show_store_repeated_union_struct_unsigned(control, reg_7, invert)
show_store_repeated_union_struct_unsigned(control, reg_7, open_drain)
show_store_repeated_union_struct_unsigned(control, reg_8, gpio_ctrl8_0)
show_store_repeated_union_struct_unsigned(control, reg_8, gpio_ctrl8_1)

/* Data */
show_store_repeated_union_struct_unsigned(data, datareg_0, gpi_led_data)


module_rmi_function_driver(function_driver);

MODULE_AUTHOR("Allie Xiong <axiong@Synaptics.com>");
MODULE_DESCRIPTION("RMI f30 module");
MODULE_LICENSE("GPL");
