/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018 XiaoMi, Inc.
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

#define APEN_PHYS_NAME "synaptics_dsx/active_pen"

#define ACTIVE_PEN_MAX_PRESSURE_16BIT 65535
#define ACTIVE_PEN_MAX_PRESSURE_8BIT 255

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

struct apen_data_8b_pressure {
	union {
		struct {
			unsigned char status_pen:1;
			unsigned char status_invert:1;
			unsigned char status_barrel:1;
			unsigned char status_reserved:5;
			unsigned char x_lsb;
			unsigned char x_msb;
			unsigned char y_lsb;
			unsigned char y_msb;
			unsigned char pressure_msb;
			unsigned char battery_state;
			unsigned char pen_id_0_7;
			unsigned char pen_id_8_15;
			unsigned char pen_id_16_23;
			unsigned char pen_id_24_31;
		} __packed;
		unsigned char data[11];
	};
};

struct apen_data {
	union {
		struct {
			unsigned char status_pen:1;
			unsigned char status_invert:1;
			unsigned char status_barrel:1;
			unsigned char status_reserved:5;
			unsigned char x_lsb;
			unsigned char x_msb;
			unsigned char y_lsb;
			unsigned char y_msb;
			unsigned char pressure_lsb;
			unsigned char pressure_msb;
			unsigned char battery_state;
			unsigned char pen_id_0_7;
			unsigned char pen_id_8_15;
			unsigned char pen_id_16_23;
			unsigned char pen_id_24_31;
		} __packed;
		unsigned char data[12];
	};
};

struct synaptics_rmi4_apen_handle {
	bool apen_present;
	unsigned char intr_mask;
	unsigned char battery_state;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	unsigned short apen_data_addr;
	unsigned short max_pressure;
	unsigned int pen_id;
	struct input_dev *apen_dev;
	struct apen_data *apen_data;
	struct synaptics_rmi4_data *rmi4_data;
};

static struct synaptics_rmi4_apen_handle *apen;

DECLARE_COMPLETION(apen_remove_complete);

static void apen_lift(void)
{
	input_report_key(apen->apen_dev, BTN_TOUCH, 0);
	input_report_key(apen->apen_dev, BTN_TOOL_PEN, 0);
	input_report_key(apen->apen_dev, BTN_TOOL_RUBBER, 0);
	input_sync(apen->apen_dev);
	apen->apen_present = false;

	return;
}

static void apen_report(void)
{
	int retval;
	int x;
	int y;
	int pressure;
	static int invert = -1;
	struct apen_data_8b_pressure *apen_data_8b;
	struct synaptics_rmi4_data *rmi4_data = apen->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			apen->apen_data_addr,
			apen->apen_data->data,
			sizeof(apen->apen_data->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read active pen data\n",
				__func__);
		return;
	}

	if (apen->apen_data->status_pen == 0) {
		if (apen->apen_present)
			apen_lift();

		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: No active pen data\n",
				__func__);

		return;
	}

	x = (apen->apen_data->x_msb << 8) | (apen->apen_data->x_lsb);
	y = (apen->apen_data->y_msb << 8) | (apen->apen_data->y_lsb);

	if ((x == -1) && (y == -1)) {
		if (apen->apen_present)
			apen_lift();

		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Active pen in range but no valid x & y\n",
				__func__);

		return;
	}

	if (!apen->apen_present)
		invert = -1;

	if (invert != -1 && invert != apen->apen_data->status_invert)
		apen_lift();

	invert = apen->apen_data->status_invert;

	if (apen->max_pressure == ACTIVE_PEN_MAX_PRESSURE_16BIT) {
		pressure = (apen->apen_data->pressure_msb << 8) |
				apen->apen_data->pressure_lsb;
		apen->battery_state = apen->apen_data->battery_state;
		apen->pen_id = (apen->apen_data->pen_id_24_31 << 24) |
				(apen->apen_data->pen_id_16_23 << 16) |
				(apen->apen_data->pen_id_8_15 << 8) |
				apen->apen_data->pen_id_0_7;
	} else {
		apen_data_8b = (struct apen_data_8b_pressure *)apen->apen_data;
		pressure = apen_data_8b->pressure_msb;
		apen->battery_state = apen_data_8b->battery_state;
		apen->pen_id = (apen_data_8b->pen_id_24_31 << 24) |
				(apen_data_8b->pen_id_16_23 << 16) |
				(apen_data_8b->pen_id_8_15 << 8) |
				apen_data_8b->pen_id_0_7;
	}

	input_report_key(apen->apen_dev, BTN_TOUCH, pressure > 0 ? 1 : 0);
	input_report_key(apen->apen_dev,
			apen->apen_data->status_invert > 0 ?
			BTN_TOOL_RUBBER : BTN_TOOL_PEN, 1);
	input_report_key(apen->apen_dev,
			BTN_STYLUS, apen->apen_data->status_barrel > 0 ?
			1 : 0);
	input_report_abs(apen->apen_dev, ABS_X, x);
	input_report_abs(apen->apen_dev, ABS_Y, y);
	input_report_abs(apen->apen_dev, ABS_PRESSURE, pressure);

	input_sync(apen->apen_dev);

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Active pen: "
			"status = %d, "
			"invert = %d, "
			"barrel = %d, "
			"x = %d, "
			"y = %d, "
			"pressure = %d\n",
			__func__,
			apen->apen_data->status_pen,
			apen->apen_data->status_invert,
			apen->apen_data->status_barrel,
			x, y, pressure);

	apen->apen_present = true;

	return;
}

static void apen_set_params(void)
{
	input_set_abs_params(apen->apen_dev, ABS_X, 0,
			apen->rmi4_data->sensor_max_x, 0, 0);
	input_set_abs_params(apen->apen_dev, ABS_Y, 0,
			apen->rmi4_data->sensor_max_y, 0, 0);
	input_set_abs_params(apen->apen_dev, ABS_PRESSURE, 0,
			apen->max_pressure, 0, 0);

	return;
}

static int apen_pressure(struct synaptics_rmi4_f12_query_8 *query_8)
{
	int retval;
	unsigned char ii;
	unsigned char data_reg_presence;
	unsigned char size_of_query_9;
	unsigned char *query_9;
	unsigned char *data_desc;
	struct synaptics_rmi4_data *rmi4_data = apen->rmi4_data;

	data_reg_presence = query_8->data[1];

	size_of_query_9 = query_8->size_of_query9;
	query_9 = kmalloc(size_of_query_9, GFP_KERNEL);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			apen->query_base_addr + 9,
			query_9,
			size_of_query_9);
	if (retval < 0)
		goto exit;

	data_desc = query_9;

	for (ii = 0; ii < 6; ii++) {
		if (!(data_reg_presence & (1 << ii)))
			continue; /* The data register is not present */
		data_desc++; /* Jump over the size entry */
		while (*data_desc & (1 << 7))
			data_desc++;
		data_desc++; /* Go to the next descriptor */
	}

	data_desc++; /* Jump over the size entry */
	/* Check for the presence of subpackets 1 and 2 */
	if ((*data_desc & (3 << 1)) == (3 << 1))
		apen->max_pressure = ACTIVE_PEN_MAX_PRESSURE_16BIT;
	else
		apen->max_pressure = ACTIVE_PEN_MAX_PRESSURE_8BIT;

exit:
	kfree(query_9);

	return retval;
}

static int apen_reg_init(void)
{
	int retval;
	unsigned char data_offset;
	unsigned char size_of_query8;
	struct synaptics_rmi4_f12_query_8 query_8;
	struct synaptics_rmi4_data *rmi4_data = apen->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			apen->query_base_addr + 7,
			&size_of_query8,
			sizeof(size_of_query8));
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			apen->query_base_addr + 8,
			query_8.data,
			sizeof(query_8.data));
	if (retval < 0)
		return retval;

	if ((size_of_query8 >= 2) && (query_8.data6_is_present)) {
		data_offset = query_8.data0_is_present +
				query_8.data1_is_present +
				query_8.data2_is_present +
				query_8.data3_is_present +
				query_8.data4_is_present +
				query_8.data5_is_present;
		apen->apen_data_addr = apen->data_base_addr + data_offset;
		retval = apen_pressure(&query_8);
		if (retval < 0)
			return retval;
	} else {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Active pen support unavailable\n",
				__func__);
		retval = -ENODEV;
	}

	return retval;
}

static int apen_scan_pdt(void)
{
	int retval;
	unsigned char ii;
	unsigned char page;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	struct synaptics_rmi4_fn_desc fd;
	struct synaptics_rmi4_data *rmi4_data = apen->rmi4_data;

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
	apen->query_base_addr = fd.query_base_addr | (page << 8);
	apen->control_base_addr = fd.ctrl_base_addr | (page << 8);
	apen->data_base_addr = fd.data_base_addr | (page << 8);
	apen->command_base_addr = fd.cmd_base_addr | (page << 8);

	retval = apen_reg_init();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to initialize active pen registers\n",
				__func__);
		return retval;
	}

	apen->intr_mask = 0;
	intr_src = fd.intr_src_count;
	intr_off = intr_count % 8;
	for (ii = intr_off;
			ii < (intr_src + intr_off);
			ii++) {
		apen->intr_mask |= 1 << ii;
	}

	rmi4_data->intr_mask[0] |= apen->intr_mask;

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

static void synaptics_rmi4_apen_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (!apen)
		return;

	if (apen->intr_mask & intr_mask)
		apen_report();

	return;
}

static int synaptics_rmi4_apen_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	if (apen) {
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Handle already exists\n",
				__func__);
		return 0;
	}

	apen = kzalloc(sizeof(*apen), GFP_KERNEL);
	if (!apen) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for apen\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	apen->apen_data = kzalloc(sizeof(*(apen->apen_data)), GFP_KERNEL);
	if (!apen->apen_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for apen_data\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_apen;
	}

	apen->rmi4_data = rmi4_data;

	retval = apen_scan_pdt();
	if (retval < 0)
		goto exit_free_apen_data;

	apen->apen_dev = input_allocate_device();
	if (apen->apen_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate active pen device\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_apen_data;
	}

	apen->apen_dev->name = ACTIVE_PEN_DRIVER_NAME;
	apen->apen_dev->phys = APEN_PHYS_NAME;
	apen->apen_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	apen->apen_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	apen->apen_dev->dev.parent = rmi4_data->pdev->dev.parent;
	input_set_drvdata(apen->apen_dev, rmi4_data);

	set_bit(EV_KEY, apen->apen_dev->evbit);
	set_bit(EV_ABS, apen->apen_dev->evbit);
	set_bit(BTN_TOUCH, apen->apen_dev->keybit);
	set_bit(BTN_TOOL_PEN, apen->apen_dev->keybit);
	set_bit(BTN_TOOL_RUBBER, apen->apen_dev->keybit);
	set_bit(BTN_STYLUS, apen->apen_dev->keybit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, apen->apen_dev->propbit);
#endif

	apen_set_params();

	retval = input_register_device(apen->apen_dev);
	if (retval) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to register active pen device\n",
				__func__);
		goto exit_free_input_device;
	}

	return 0;

exit_free_input_device:
	input_free_device(apen->apen_dev);

exit_free_apen_data:
	kfree(apen->apen_data);

exit_free_apen:
	kfree(apen);
	apen = NULL;

exit:
	return retval;
}

static void synaptics_rmi4_apen_remove(struct synaptics_rmi4_data *rmi4_data)
{
	if (!apen)
		goto exit;

	input_unregister_device(apen->apen_dev);
	kfree(apen->apen_data);
	kfree(apen);
	apen = NULL;

exit:
	complete(&apen_remove_complete);

	return;
}

static void synaptics_rmi4_apen_reset(struct synaptics_rmi4_data *rmi4_data)
{
	if (!apen) {
		synaptics_rmi4_apen_init(rmi4_data);
		return;
	}

	apen_lift();

	apen_scan_pdt();

	return;
}

static void synaptics_rmi4_apen_reinit(struct synaptics_rmi4_data *rmi4_data)
{
	if (!apen)
		return;

	apen_lift();

	return;
}

static void synaptics_rmi4_apen_e_suspend(struct synaptics_rmi4_data *rmi4_data)
{
	if (!apen)
		return;

	apen_lift();

	return;
}

static void synaptics_rmi4_apen_suspend(struct synaptics_rmi4_data *rmi4_data)
{
	if (!apen)
		return;

	apen_lift();

	return;
}

static struct synaptics_rmi4_exp_fn active_pen_module = {
	.fn_type = RMI_ACTIVE_PEN,
	.init = synaptics_rmi4_apen_init,
	.remove = synaptics_rmi4_apen_remove,
	.reset = synaptics_rmi4_apen_reset,
	.reinit = synaptics_rmi4_apen_reinit,
	.early_suspend = synaptics_rmi4_apen_e_suspend,
	.suspend = synaptics_rmi4_apen_suspend,
	.resume = NULL,
	.late_resume = NULL,
	.attn = synaptics_rmi4_apen_attn,
};

static int __init rmi4_active_pen_module_init(void)
{
	synaptics_rmi4_new_function_force(&active_pen_module, true);

	return 0;
}

static void __exit rmi4_active_pen_module_exit(void)
{
	synaptics_rmi4_new_function_force(&active_pen_module, false);

	wait_for_completion(&apen_remove_complete);

	return;
}

module_init(rmi4_active_pen_module_init);
module_exit(rmi4_active_pen_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Active Pen Module");
MODULE_LICENSE("GPL v2");
