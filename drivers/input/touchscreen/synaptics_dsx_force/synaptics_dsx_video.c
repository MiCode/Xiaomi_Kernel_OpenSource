/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
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
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"

#define SYSFS_FOLDER_NAME "video"

/*
#define RMI_DCS_SUSPEND_RESUME
*/

static ssize_t video_sysfs_dcs_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t video_sysfs_param_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static int video_send_dcs_command(unsigned char command_opcode);

struct f38_command {
	union {
		struct {
			unsigned char command_opcode;
			unsigned char register_access:1;
			unsigned char gamma_page:1;
			unsigned char f38_control1_b2__7:6;
			unsigned char parameter_field_1;
			unsigned char parameter_field_2;
			unsigned char parameter_field_3;
			unsigned char parameter_field_4;
			unsigned char send_to_dcs:1;
			unsigned char f38_command6_b1__7:7;
		} __packed;
		unsigned char data[7];
	};
};

struct synaptics_rmi4_video_handle {
	unsigned char param;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	struct synaptics_rmi4_data *rmi4_data;
	struct kobject *sysfs_dir;
};

#ifdef RMI_DCS_SUSPEND_RESUME
struct dcs_command {
	unsigned char command;
	unsigned int wait_time;
};

static struct dcs_command suspend_sequence[] = {
	{
		.command = 0x28,
		.wait_time = 200,
	},
	{
		.command = 0x10,
		.wait_time = 200,
	},
};

static struct dcs_command resume_sequence[] = {
	{
		.command = 0x11,
		.wait_time = 200,
	},
	{
		.command = 0x29,
		.wait_time = 200,
	},
};
#endif

static struct device_attribute attrs[] = {
	__ATTR(dcs_write, S_IWUGO,
			synaptics_rmi4_show_error,
			video_sysfs_dcs_write_store),
	__ATTR(param, S_IWUGO,
			synaptics_rmi4_show_error,
			video_sysfs_param_store),
};

static struct synaptics_rmi4_video_handle *video;

DECLARE_COMPLETION(video_remove_complete);

static ssize_t video_sysfs_dcs_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;

	if (sscanf(buf, "%x", &input) != 1)
		return -EINVAL;

	retval = video_send_dcs_command((unsigned char)input);
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t video_sysfs_param_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%x", &input) != 1)
		return -EINVAL;

	video->param = (unsigned char)input;

	return count;
}

static int video_send_dcs_command(unsigned char command_opcode)
{
	int retval;
	struct f38_command command;
	struct synaptics_rmi4_data *rmi4_data = video->rmi4_data;

	memset(&command, 0x00, sizeof(command));

	command.command_opcode = command_opcode;
	command.parameter_field_1 = video->param;
	command.send_to_dcs = 1;

	video->param = 0;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			video->command_base_addr,
			command.data,
			sizeof(command.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to send DCS command\n",
				__func__);
		return retval;
	}

	return 0;
}

static int video_scan_pdt(void)
{
	int retval;
	unsigned char page;
	unsigned short addr;
	bool f38_found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_data *rmi4_data = video->rmi4_data;

	for (page = 0; page < PAGES_TO_SERVICE; page++) {
		for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
			addr |= (page << 8);

			retval = synaptics_rmi4_reg_read(rmi4_data,
					addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			addr &= ~(MASK_8BIT << 8);

			if (!rmi_fd.fn_number)
				break;

			if (rmi_fd.fn_number == SYNAPTICS_RMI4_F38) {
				f38_found = true;
				goto f38_found;
			}
		}
	}

	if (!f38_found) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to find F38\n",
				__func__);
		return -EINVAL;
	}

f38_found:
	video->query_base_addr = rmi_fd.query_base_addr | (page << 8);
	video->control_base_addr = rmi_fd.ctrl_base_addr | (page << 8);
	video->data_base_addr = rmi_fd.data_base_addr | (page << 8);
	video->command_base_addr = rmi_fd.cmd_base_addr | (page << 8);

	return 0;
}

static int synaptics_rmi4_video_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char attr_count;

	if (video) {
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Handle already exists\n",
				__func__);
		return 0;
	}

	video = kzalloc(sizeof(*video), GFP_KERNEL);
	if (!video) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for video\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	video->rmi4_data = rmi4_data;

	retval = video_scan_pdt();
	if (retval < 0) {
		retval = 0;
		goto exit_scan_pdt;
	}

	video->sysfs_dir = kobject_create_and_add(SYSFS_FOLDER_NAME,
			&rmi4_data->input_dev->dev.kobj);
	if (!video->sysfs_dir) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to create sysfs directory\n",
				__func__);
		retval = -ENODEV;
		goto exit_sysfs_dir;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(video->sysfs_dir,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			retval = -ENODEV;
			goto exit_sysfs_attrs;
		}
	}

	return 0;

exit_sysfs_attrs:
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(video->sysfs_dir, &attrs[attr_count].attr);

	kobject_put(video->sysfs_dir);

exit_sysfs_dir:
exit_scan_pdt:
	kfree(video);
	video = NULL;

exit:
	return retval;
}

static void synaptics_rmi4_video_remove(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char attr_count;

	if (!video)
		goto exit;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(video->sysfs_dir, &attrs[attr_count].attr);

	kobject_put(video->sysfs_dir);

	kfree(video);
	video = NULL;

exit:
	complete(&video_remove_complete);

	return;
}

static void synaptics_rmi4_video_reset(struct synaptics_rmi4_data *rmi4_data)
{
	if (!video)
		synaptics_rmi4_video_init(rmi4_data);

	return;
}

#ifdef RMI_DCS_SUSPEND_RESUME
static void synaptics_rmi4_video_suspend(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned char command;
	unsigned char num_of_cmds;

	if (!video)
		return;

	num_of_cmds = ARRAY_SIZE(suspend_sequence);

	for (ii = 0; ii < num_of_cmds; ii++) {
		command = suspend_sequence[ii].command;
		retval = video_send_dcs_command(command);
		if (retval < 0)
			return;
		msleep(suspend_sequence[ii].wait_time);
	}

	return;
}

static void synaptics_rmi4_video_resume(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned char command;
	unsigned char num_of_cmds;

	if (!video)
		return;

	num_of_cmds = ARRAY_SIZE(resume_sequence);

	for (ii = 0; ii < num_of_cmds; ii++) {
		command = resume_sequence[ii].command;
		retval = video_send_dcs_command(command);
		if (retval < 0)
			return;
		msleep(resume_sequence[ii].wait_time);
	}

	return;
}
#endif

static struct synaptics_rmi4_exp_fn video_module = {
	.fn_type = RMI_VIDEO,
	.init = synaptics_rmi4_video_init,
	.remove = synaptics_rmi4_video_remove,
	.reset = synaptics_rmi4_video_reset,
	.reinit = NULL,
	.early_suspend = NULL,
#ifdef RMI_DCS_SUSPEND_RESUME
	.suspend = synaptics_rmi4_video_suspend,
	.resume = synaptics_rmi4_video_resume,
#else
	.suspend = NULL,
	.resume = NULL,
#endif
	.late_resume = NULL,
	.attn = NULL,
};

static int __init rmi4_video_module_init(void)
{
	synaptics_rmi4_new_function_force(&video_module, true);

	return 0;
}

static void __exit rmi4_video_module_exit(void)
{
	synaptics_rmi4_new_function_force(&video_module, false);

	wait_for_completion(&video_remove_complete);

	return;
}

module_init(rmi4_video_module_init);
module_exit(rmi4_video_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Video Module");
MODULE_LICENSE("GPL v2");
