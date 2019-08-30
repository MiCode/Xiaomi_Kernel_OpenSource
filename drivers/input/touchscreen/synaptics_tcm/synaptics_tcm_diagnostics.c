/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
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

#include "synaptics_tcm_core.h"

#define SYSFS_DIR_NAME "diagnostics"

enum pingpong_state {
	PING = 0,
	PONG = 1,
};

struct diag_hcd {
	pid_t pid;
	unsigned char report_type;
	enum pingpong_state state;
	struct kobject *sysfs_dir;
	struct siginfo sigio;
	struct task_struct *task;
	struct syna_tcm_buffer ping;
	struct syna_tcm_buffer pong;
	struct syna_tcm_hcd *tcm_hcd;
};

DECLARE_COMPLETION(diag_remove_complete);

static struct diag_hcd *diag_hcd;

STORE_PROTOTYPE(diag, pid)
SHOW_PROTOTYPE(diag, size)
STORE_PROTOTYPE(diag, type)
SHOW_PROTOTYPE(diag, rows)
SHOW_PROTOTYPE(diag, cols)
SHOW_PROTOTYPE(diag, hybrid)
SHOW_PROTOTYPE(diag, buttons)

static struct device_attribute *attrs[] = {
	ATTRIFY(pid),
	ATTRIFY(size),
	ATTRIFY(type),
	ATTRIFY(rows),
	ATTRIFY(cols),
	ATTRIFY(hybrid),
	ATTRIFY(buttons),
};

static ssize_t diag_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct bin_attribute bin_attr = {
	.attr = {
		.name = "data",
		.mode = S_IRUGO,
	},
	.size = 0,
	.read = diag_sysfs_data_show,
};

static ssize_t diag_sysfs_pid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	diag_hcd->pid = input;

	if (diag_hcd->pid) {
		diag_hcd->task = pid_task(find_vpid(diag_hcd->pid),
				PIDTYPE_PID);
		if (!diag_hcd->task) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to locate task\n");
			retval = -EINVAL;
			goto exit;
		}
	}

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t diag_sysfs_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	if (diag_hcd->state == PING) {
		LOCK_BUFFER(diag_hcd->ping);

		retval = snprintf(buf, PAGE_SIZE,
				"%u\n",
				diag_hcd->ping.data_length);

		UNLOCK_BUFFER(diag_hcd->ping);
	} else {
		LOCK_BUFFER(diag_hcd->pong);

		retval = snprintf(buf, PAGE_SIZE,
				"%u\n",
				diag_hcd->pong.data_length);

		UNLOCK_BUFFER(diag_hcd->pong);
	}

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t diag_sysfs_type_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	diag_hcd->report_type = (unsigned char)input;

	mutex_unlock(&tcm_hcd->extif_mutex);

	return count;
}

static ssize_t diag_sysfs_rows_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned int rows;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	if (tcm_hcd->id_info.mode != MODE_APPLICATION ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		retval = -ENODEV;
		goto exit;
	}

	app_info = &tcm_hcd->app_info;
	rows = le2_to_uint(app_info->num_of_image_rows);

	retval = snprintf(buf, PAGE_SIZE, "%u\n", rows);

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t diag_sysfs_cols_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned int cols;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	if (tcm_hcd->id_info.mode != MODE_APPLICATION ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		retval = -ENODEV;
		goto exit;
	}

	app_info = &tcm_hcd->app_info;
	cols = le2_to_uint(app_info->num_of_image_cols);

	retval = snprintf(buf, PAGE_SIZE, "%u\n", cols);

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t diag_sysfs_hybrid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned int hybrid;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	if (tcm_hcd->id_info.mode != MODE_APPLICATION ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		retval = -ENODEV;
		goto exit;
	}

	app_info = &tcm_hcd->app_info;
	hybrid = le2_to_uint(app_info->has_hybrid_data);

	retval = snprintf(buf, PAGE_SIZE, "%u\n", hybrid);

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t diag_sysfs_buttons_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned int buttons;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	if (tcm_hcd->id_info.mode != MODE_APPLICATION ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		retval = -ENODEV;
		goto exit;
	}

	app_info = &tcm_hcd->app_info;
	buttons = le2_to_uint(app_info->num_of_buttons);

	retval = snprintf(buf, PAGE_SIZE, "%u\n", buttons);

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t diag_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int readlen;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = 0;

	if (diag_hcd->state == PING) {
		LOCK_BUFFER(diag_hcd->ping);

		if (diag_hcd->ping.data_length == 0) {
			readlen = 0;
			goto exit;
		}

		readlen = MIN(count, diag_hcd->ping.data_length - pos);

		if (diag_hcd->ping.data_length) {
			retval = secure_memcpy(buf,
					count,
					&diag_hcd->ping.buf[pos],
					diag_hcd->ping.buf_size - pos,
					readlen);
		}

		UNLOCK_BUFFER(diag_hcd->ping);
	} else {
		LOCK_BUFFER(diag_hcd->pong);

		if (diag_hcd->pong.data_length == 0) {
			readlen = 0;
			goto exit;
		}

		readlen = MIN(count, diag_hcd->pong.data_length - pos);

		if (diag_hcd->pong.data_length) {
			retval = secure_memcpy(buf,
					count,
					&diag_hcd->pong.buf[pos],
					diag_hcd->pong.buf_size - pos,
					readlen);
		}

		UNLOCK_BUFFER(diag_hcd->pong);
	}

exit:
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy report data\n");
	} else {
		retval = readlen;
	}

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static void diag_report(void)
{
	int retval;
	static enum pingpong_state state = PING;
	struct syna_tcm_hcd *tcm_hcd = diag_hcd->tcm_hcd;

	if (state == PING) {
		LOCK_BUFFER(diag_hcd->ping);

		retval = syna_tcm_alloc_mem(tcm_hcd,
				&diag_hcd->ping,
				tcm_hcd->report.buffer.data_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for diag_hcd->ping.buf\n");
			UNLOCK_BUFFER(diag_hcd->ping);
			return;
		}

		retval = secure_memcpy(diag_hcd->ping.buf,
				diag_hcd->ping.buf_size,
				tcm_hcd->report.buffer.buf,
				tcm_hcd->report.buffer.buf_size,
				tcm_hcd->report.buffer.data_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy report data\n");
			UNLOCK_BUFFER(diag_hcd->ping);
			return;
		}

		diag_hcd->ping.data_length = tcm_hcd->report.buffer.data_length;

		UNLOCK_BUFFER(diag_hcd->ping);

		diag_hcd->state = state;
		state = PONG;
	} else {
		LOCK_BUFFER(diag_hcd->pong);

		retval = syna_tcm_alloc_mem(tcm_hcd,
				&diag_hcd->pong,
				tcm_hcd->report.buffer.data_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for diag_hcd->pong.buf\n");
			UNLOCK_BUFFER(diag_hcd->pong);
			return;
		}

		retval = secure_memcpy(diag_hcd->pong.buf,
				diag_hcd->pong.buf_size,
				tcm_hcd->report.buffer.buf,
				tcm_hcd->report.buffer.buf_size,
				tcm_hcd->report.buffer.data_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy report data\n");
			UNLOCK_BUFFER(diag_hcd->pong);
			return;
		}

		diag_hcd->pong.data_length = tcm_hcd->report.buffer.data_length;

		UNLOCK_BUFFER(diag_hcd->pong);

		diag_hcd->state = state;
		state = PING;
	}

	if (diag_hcd->pid)
		send_sig_info(SIGIO, &diag_hcd->sigio, diag_hcd->task);

	return;
}

static int diag_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;

	diag_hcd = kzalloc(sizeof(*diag_hcd), GFP_KERNEL);
	if (!diag_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for diag_hcd\n");
		return -ENOMEM;
	}

	diag_hcd->tcm_hcd = tcm_hcd;
	diag_hcd->state = PING;

	INIT_BUFFER(diag_hcd->ping, false);
	INIT_BUFFER(diag_hcd->pong, false);

	memset(&diag_hcd->sigio, 0x00, sizeof(diag_hcd->sigio));
	diag_hcd->sigio.si_signo = SIGIO;
	diag_hcd->sigio.si_code = SI_USER;

	diag_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!diag_hcd->sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(diag_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	retval = sysfs_create_bin_file(diag_hcd->sysfs_dir, &bin_attr);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs bin file\n");
		goto err_sysfs_create_bin_file;
	}

	return 0;

err_sysfs_create_bin_file:
err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(diag_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(diag_hcd->sysfs_dir);

err_sysfs_create_dir:
	RELEASE_BUFFER(diag_hcd->pong);
	RELEASE_BUFFER(diag_hcd->ping);

	kfree(diag_hcd);
	diag_hcd = NULL;

	return retval;
}

static int diag_remove(struct syna_tcm_hcd *tcm_hcd)
{
	int idx;

	if (!diag_hcd)
		goto exit;

	sysfs_remove_bin_file(diag_hcd->sysfs_dir, &bin_attr);

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++)
		sysfs_remove_file(diag_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(diag_hcd->sysfs_dir);

	RELEASE_BUFFER(diag_hcd->pong);
	RELEASE_BUFFER(diag_hcd->ping);

	kfree(diag_hcd);
	diag_hcd = NULL;

exit:
	complete(&diag_remove_complete);

	return 0;
}

static int diag_syncbox(struct syna_tcm_hcd *tcm_hcd)
{
	if (!diag_hcd)
		return 0;

	if (tcm_hcd->report.id == diag_hcd->report_type)
		diag_report();

	return 0;
}

static int diag_reset(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	if (!diag_hcd) {
		retval = diag_init(tcm_hcd);
		return retval;
	}

	return 0;
}

static struct syna_tcm_module_cb diag_module = {
	.type = TCM_DIAGNOSTICS,
	.init = diag_init,
	.remove = diag_remove,
	.syncbox = diag_syncbox,
	.asyncbox = NULL,
	.reset = diag_reset,
	.suspend = NULL,
	.resume = NULL,
	.early_suspend = NULL,
};

static int __init diag_module_init(void)
{
	return syna_tcm_add_module(&diag_module, true);
}

static void __exit diag_module_exit(void)
{
	syna_tcm_add_module(&diag_module, false);

	wait_for_completion(&diag_remove_complete);

	return;
}

late_initcall(diag_module_init);
module_exit(diag_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Diagnostics Module");
MODULE_LICENSE("GPL v2");
