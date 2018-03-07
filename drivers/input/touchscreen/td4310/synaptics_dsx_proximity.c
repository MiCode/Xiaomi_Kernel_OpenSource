/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2016 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"

#define PROX_PHYS_NAME "synaptics_dsx/proximity"

#define HOVER_Z_MAX (255)

#define HOVERING_FINGER_EN (1 << 4)

static ssize_t synaptics_rmi4_hover_finger_en_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_hover_finger_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static struct device_attribute attrs[] = {
	__ATTR(hover_finger_en, (S_IRUGO | S_IWUGO),
			synaptics_rmi4_hover_finger_en_show,
			synaptics_rmi4_hover_finger_en_store),
};

struct synaptics_rmi4_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query6;
			struct {
				unsigned char ctrl0_is_present:1;
				unsigned char ctrl1_is_present:1;
				unsigned char ctrl2_is_present:1;
				unsigned char ctrl3_is_present:1;
				unsigned char ctrl4_is_present:1;
				unsigned char ctrl5_is_present:1;
				unsigned char ctrl6_is_present:1;
				unsigned char ctrl7_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl8_is_present:1;
				unsigned char ctrl9_is_present:1;
				unsigned char ctrl10_is_present:1;
				unsigned char ctrl11_is_present:1;
				unsigned char ctrl12_is_present:1;
				unsigned char ctrl13_is_present:1;
				unsigned char ctrl14_is_present:1;
				unsigned char ctrl15_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl16_is_present:1;
				unsigned char ctrl17_is_present:1;
				unsigned char ctrl18_is_present:1;
				unsigned char ctrl19_is_present:1;
				unsigned char ctrl20_is_present:1;
				unsigned char ctrl21_is_present:1;
				unsigned char ctrl22_is_present:1;
				unsigned char ctrl23_is_present:1;
			} __packed;
		};
		unsigned char data[4];
	};
};

struct synaptics_rmi4_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query9;
			struct {
				unsigned char data0_is_present:1;
				unsigned char data1_is_present:1;
				unsigned char data2_is_present:1;
				unsigned char data3_is_present:1;
				unsigned char data4_is_present:1;
				unsigned char data5_is_present:1;
				unsigned char data6_is_present:1;
				unsigned char data7_is_present:1;
			} __packed;
		};
		unsigned char data[2];
	};
};

struct prox_finger_data {
	union {
		struct {
			unsigned char object_type_and_status;
			unsigned char x_lsb;
			unsigned char x_msb;
			unsigned char y_lsb;
			unsigned char y_msb;
			unsigned char z;
		} __packed;
		unsigned char proximity_data[6];
	};
};

struct synaptics_rmi4_prox_handle {
	bool hover_finger_present;
	bool hover_finger_en;
	unsigned char intr_mask;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	unsigned short hover_finger_en_addr;
	unsigned short hover_finger_data_addr;
	struct input_dev *prox_dev;
	struct prox_finger_data *finger_data;
	struct synaptics_rmi4_data *rmi4_data;
};

static struct synaptics_rmi4_prox_handle *prox;

DECLARE_COMPLETION(prox_remove_complete);

static void prox_hover_finger_lift(void)
{
	input_report_key(prox->prox_dev, BTN_TOUCH, 0);
	input_report_key(prox->prox_dev, BTN_TOOL_FINGER, 0);
	input_sync(prox->prox_dev);
	prox->hover_finger_present = false;

	return;
}

static void prox_hover_finger_report(void)
{
	int retval;
	int x;
	int y;
	int z;
	struct prox_finger_data *data;
	struct synaptics_rmi4_data *rmi4_data = prox->rmi4_data;

	data = prox->finger_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			prox->hover_finger_data_addr,
			data->proximity_data,
			sizeof(data->proximity_data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read hovering finger data\n",
				__func__);
		return;
	}

	if (data->object_type_and_status != F12_HOVERING_FINGER_STATUS) {
		if (prox->hover_finger_present)
			prox_hover_finger_lift();

		return;
	}

	x = (data->x_msb << 8) | (data->x_lsb);
	y = (data->y_msb << 8) | (data->y_lsb);
	z = HOVER_Z_MAX - data->z;

	input_report_key(prox->prox_dev, BTN_TOUCH, 0);
	input_report_key(prox->prox_dev, BTN_TOOL_FINGER, 1);
	input_report_abs(prox->prox_dev, ABS_X, x);
	input_report_abs(prox->prox_dev, ABS_Y, y);
	input_report_abs(prox->prox_dev, ABS_DISTANCE, z);

	input_sync(prox->prox_dev);

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: x = %d y = %d z = %d\n",
			__func__, x, y, z);

	prox->hover_finger_present = true;

	return;
}

static int prox_set_hover_finger_en(void)
{
	int retval;
	unsigned char object_report_enable;
	struct synaptics_rmi4_data *rmi4_data = prox->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			prox->hover_finger_en_addr,
			&object_report_enable,
			sizeof(object_report_enable));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read from object report enable register\n",
				__func__);
		return retval;
	}

	if (prox->hover_finger_en)
		object_report_enable |= HOVERING_FINGER_EN;
	else
		object_report_enable &= ~HOVERING_FINGER_EN;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			prox->hover_finger_en_addr,
			&object_report_enable,
			sizeof(object_report_enable));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write to object report enable register\n",
				__func__);
		return retval;
	}

	return 0;
}

static void prox_set_params(void)
{
	input_set_abs_params(prox->prox_dev, ABS_X, 0,
			prox->rmi4_data->sensor_max_x, 0, 0);
	input_set_abs_params(prox->prox_dev, ABS_Y, 0,
			prox->rmi4_data->sensor_max_y, 0, 0);
	input_set_abs_params(prox->prox_dev, ABS_DISTANCE, 0,
			HOVER_Z_MAX, 0, 0);

	return;
}

static int prox_reg_init(void)
{
	int retval;
	unsigned char ctrl_23_offset;
	unsigned char data_1_offset;
	struct synaptics_rmi4_f12_query_5 query_5;
	struct synaptics_rmi4_f12_query_8 query_8;
	struct synaptics_rmi4_data *rmi4_data = prox->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			prox->query_base_addr + 5,
			query_5.data,
			sizeof(query_5.data));
	if (retval < 0)
		return retval;

	ctrl_23_offset = query_5.ctrl0_is_present +
			query_5.ctrl1_is_present +
			query_5.ctrl2_is_present +
			query_5.ctrl3_is_present +
			query_5.ctrl4_is_present +
			query_5.ctrl5_is_present +
			query_5.ctrl6_is_present +
			query_5.ctrl7_is_present +
			query_5.ctrl8_is_present +
			query_5.ctrl9_is_present +
			query_5.ctrl10_is_present +
			query_5.ctrl11_is_present +
			query_5.ctrl12_is_present +
			query_5.ctrl13_is_present +
			query_5.ctrl14_is_present +
			query_5.ctrl15_is_present +
			query_5.ctrl16_is_present +
			query_5.ctrl17_is_present +
			query_5.ctrl18_is_present +
			query_5.ctrl19_is_present +
			query_5.ctrl20_is_present +
			query_5.ctrl21_is_present +
			query_5.ctrl22_is_present;

	prox->hover_finger_en_addr = prox->control_base_addr + ctrl_23_offset;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			prox->query_base_addr + 8,
			query_8.data,
			sizeof(query_8.data));
	if (retval < 0)
		return retval;

	data_1_offset = query_8.data0_is_present;
	prox->hover_finger_data_addr = prox->data_base_addr + data_1_offset;

	return retval;
}

static int prox_scan_pdt(void)
{
	int retval;
	unsigned char ii;
	unsigned char page;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	struct synaptics_rmi4_fn_desc fd;
	struct synaptics_rmi4_data *rmi4_data = prox->rmi4_data;

	for (page = 0; page < PAGES_TO_SERVICE; page++) {
		for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
			addr |= (page << 8);

			retval = synaptics_rmi4_reg_read(rmi4_data,
					addr,
					(unsigned char *)&fd,
					sizeof(fd));
			if (retval < 0)
				return retval;

			addr &= ~(MASK_8BIT << 8);

			if (fd.fn_number) {
				dev_dbg(rmi4_data->pdev->dev.parent,
						"%s: Found F%02x\n",
						__func__, fd.fn_number);
				switch (fd.fn_number) {
				case SYNAPTICS_RMI4_F12:
					goto f12_found;
					break;
				}
			} else {
				break;
			}

			intr_count += fd.intr_src_count;
		}
	}

	dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to find F12\n",
			__func__);
	return -EINVAL;

f12_found:
	prox->query_base_addr = fd.query_base_addr | (page << 8);
	prox->control_base_addr = fd.ctrl_base_addr | (page << 8);
	prox->data_base_addr = fd.data_base_addr | (page << 8);
	prox->command_base_addr = fd.cmd_base_addr | (page << 8);

	retval = prox_reg_init();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to initialize proximity registers\n",
				__func__);
		return retval;
	}

	prox->intr_mask = 0;
	intr_src = fd.intr_src_count;
	intr_off = intr_count % 8;
	for (ii = intr_off;
			ii < (intr_src + intr_off);
			ii++) {
		prox->intr_mask |= 1 << ii;
	}

	rmi4_data->intr_mask[0] |= prox->intr_mask;

	addr = rmi4_data->f01_ctrl_base_addr + 1;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			addr,
			&(rmi4_data->intr_mask[0]),
			sizeof(rmi4_data->intr_mask[0]));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set interrupt enable bit\n",
				__func__);
		return retval;
	}

	return 0;
}

static ssize_t synaptics_rmi4_hover_finger_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!prox)
		return -ENODEV;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			prox->hover_finger_en);
}

static ssize_t synaptics_rmi4_hover_finger_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = prox->rmi4_data;

	if (!prox)
		return -ENODEV;

	if (sscanf(buf, "%x", &input) != 1)
		return -EINVAL;

	if (input == 1)
		prox->hover_finger_en = true;
	else if (input == 0)
		prox->hover_finger_en = false;
	else
		return -EINVAL;

	retval = prox_set_hover_finger_en();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change hovering finger enable setting\n",
				__func__);
		return retval;
	}

	return count;
}

int synaptics_rmi4_prox_hover_finger_en(bool enable)
{
	int retval;

	if (!prox)
		return -ENODEV;

	prox->hover_finger_en = enable;

	retval = prox_set_hover_finger_en();
	if (retval < 0)
		return retval;

	return 0;
}
EXPORT_SYMBOL(synaptics_rmi4_prox_hover_finger_en);

static void synaptics_rmi4_prox_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (!prox)
		return;

	if (prox->intr_mask & intr_mask)
		prox_hover_finger_report();

	return;
}

static int synaptics_rmi4_prox_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char attr_count;

	if (prox) {
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Handle already exists\n",
				__func__);
		return 0;
	}

	prox = kzalloc(sizeof(*prox), GFP_KERNEL);
	if (!prox) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for prox\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	prox->finger_data = kzalloc(sizeof(*(prox->finger_data)), GFP_KERNEL);
	if (!prox->finger_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for finger_data\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_prox;
	}

	prox->rmi4_data = rmi4_data;

	retval = prox_scan_pdt();
	if (retval < 0)
		goto exit_free_finger_data;

	prox->hover_finger_en = true;

	retval = prox_set_hover_finger_en();
	if (retval < 0)
		return retval;

	prox->prox_dev = input_allocate_device();
	if (prox->prox_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate proximity device\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_finger_data;
	}

	prox->prox_dev->name = PROXIMITY_DRIVER_NAME;
	prox->prox_dev->phys = PROX_PHYS_NAME;
	prox->prox_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	prox->prox_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	prox->prox_dev->dev.parent = rmi4_data->pdev->dev.parent;
	input_set_drvdata(prox->prox_dev, rmi4_data);

	set_bit(EV_KEY, prox->prox_dev->evbit);
	set_bit(EV_ABS, prox->prox_dev->evbit);
	set_bit(BTN_TOUCH, prox->prox_dev->keybit);
	set_bit(BTN_TOOL_FINGER, prox->prox_dev->keybit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, prox->prox_dev->propbit);
#endif

	prox_set_params();

	retval = input_register_device(prox->prox_dev);
	if (retval) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to register proximity device\n",
				__func__);
		goto exit_free_input_device;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			goto exit_free_sysfs;
		}
	}

	return 0;

exit_free_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	input_unregister_device(prox->prox_dev);
	prox->prox_dev = NULL;

exit_free_input_device:
	if (prox->prox_dev)
		input_free_device(prox->prox_dev);

exit_free_finger_data:
	kfree(prox->finger_data);

exit_free_prox:
	kfree(prox);
	prox = NULL;

exit:
	return retval;
}

static void synaptics_rmi4_prox_remove(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char attr_count;

	if (!prox)
		goto exit;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	input_unregister_device(prox->prox_dev);
	kfree(prox->finger_data);
	kfree(prox);
	prox = NULL;

exit:
	complete(&prox_remove_complete);

	return;
}

static void synaptics_rmi4_prox_reset(struct synaptics_rmi4_data *rmi4_data)
{
	if (!prox) {
		synaptics_rmi4_prox_init(rmi4_data);
		return;
	}

	prox_hover_finger_lift();

	prox_scan_pdt();

	prox_set_hover_finger_en();

	return;
}

static void synaptics_rmi4_prox_reinit(struct synaptics_rmi4_data *rmi4_data)
{
	if (!prox)
		return;

	prox_hover_finger_lift();

	prox_set_hover_finger_en();

	return;
}

static void synaptics_rmi4_prox_e_suspend(struct synaptics_rmi4_data *rmi4_data)
{
	if (!prox)
		return;

	prox_hover_finger_lift();

	return;
}

static void synaptics_rmi4_prox_suspend(struct synaptics_rmi4_data *rmi4_data)
{
	if (!prox)
		return;

	prox_hover_finger_lift();

	return;
}

static struct synaptics_rmi4_exp_fn proximity_module = {
	.fn_type = RMI_PROXIMITY,
	.init = synaptics_rmi4_prox_init,
	.remove = synaptics_rmi4_prox_remove,
	.reset = synaptics_rmi4_prox_reset,
	.reinit = synaptics_rmi4_prox_reinit,
	.early_suspend = synaptics_rmi4_prox_e_suspend,
	.suspend = synaptics_rmi4_prox_suspend,
	.resume = NULL,
	.late_resume = NULL,
	.attn = synaptics_rmi4_prox_attn,
};

static int __init rmi4_proximity_module_init(void)
{
	synaptics_rmi4_new_function(&proximity_module, true);

	return 0;
}

static void __exit rmi4_proximity_module_exit(void)
{
	synaptics_rmi4_new_function(&proximity_module, false);

	wait_for_completion(&prox_remove_complete);

	return;
}

module_init(rmi4_proximity_module_init);
module_exit(rmi4_proximity_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Proximity Module");
MODULE_LICENSE("GPL v2");
