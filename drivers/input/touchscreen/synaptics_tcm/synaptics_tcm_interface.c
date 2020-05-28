/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017 Synaptics Incorporated. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#include <linux/firmware.h>
#include "synaptics_tcm_core.h"

#define SYSFS_DIR_NAME "xiaomi_interface"

#define INTF_REPORT_TIMEOUT_100MS 30


struct xiaomi_intf_hcd {
	/* handler for synaptiucs tcm device */
	struct syna_tcm_hcd *tcm_hcd;
	/* to create the sysfs */
	struct kobject *sysfs_dir;
	/* flag to indicate the error out during the process */
	bool err_flag;

	/* for report image using */
	int *report_data;
	unsigned char report_rows;
	unsigned char report_cols;
	unsigned char report_type;
	bool report_is_ready;
};

DECLARE_COMPLETION(xiaomi_intf_remove_complete);

static struct xiaomi_intf_hcd *intf_hcd;

SHOW_STORE_PROTOTYPE(intf, get_rawordiff_data)

static struct device_attribute *attrs[] = {
	ATTRIFY(get_rawordiff_data),
};

/*
 * helper function to set the report type
 * input:
 *    0 = raw image
 *    1 = delta image
 *
 * return:
 *    true = valid report ; false = invalid report
 */
static bool intf_helper_set_report_type(int input)
{
	bool is_valid = false;
	struct syna_tcm_hcd *tcm_hcd = intf_hcd->tcm_hcd;

	switch (input) {
	case 0: /* raw image */
		intf_hcd->report_type = REPORT_RAW;
		is_valid = true;
		break;
	case 1: /* delta image */
		intf_hcd->report_type = REPORT_DELTA;
		is_valid = true;
		break;
	default:
		LOGE(tcm_hcd->pdev->dev.parent,
			"unknown input [0:raw/1:delta] (input = %d)\n", input);
		is_valid = false;
		break;
	}

	return is_valid;
}
/*
 * helper function to enable/disable the report stream
 * input:
 *    true = enable / false = disable
 * return:
 *    0 = succeed ; otherwise, failure
 */
static int intf_helper_enable_report(bool enable)
{
	int retval;
	unsigned char report_touch = REPORT_TOUCH;
	struct syna_tcm_hcd *tcm_hcd = intf_hcd->tcm_hcd;
	unsigned char command;

	/* to disable the touch report if delta/raw report stream is enabled */
	if (enable) {
		retval = tcm_hcd->write_message(tcm_hcd,
						CMD_DISABLE_REPORT,
						&report_touch,
						1,
						&tcm_hcd->resp.buf,
						&tcm_hcd->resp.buf_size,
						&tcm_hcd->resp.data_length,
						NULL,
						0);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"failed to disable touch report\n");
			return -EINVAL;
		}
	}
	/* otherwise, resume the touch report */
	else {
		retval = tcm_hcd->write_message(tcm_hcd,
						CMD_ENABLE_REPORT,
						&report_touch,
						1,
						&tcm_hcd->resp.buf,
						&tcm_hcd->resp.buf_size,
						&tcm_hcd->resp.data_length,
						NULL,
						0);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"failed to disable touch report\n");
			return -EINVAL;
		}
	}

	if (enable)
		command = CMD_ENABLE_REPORT;
	else
		command = CMD_DISABLE_REPORT;

	retval = tcm_hcd->write_message(tcm_hcd,
					command,
					&intf_hcd->report_type,
					1,
					&tcm_hcd->resp.buf,
					&tcm_hcd->resp.buf_size,
					&tcm_hcd->resp.data_length,
					NULL,
					0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"failed to write command %s\n", STR(command));
		return -EINVAL;
	}

	return 0;
}

/*
 * function to get a raw/delta data image
 *
 * input:
 *    which  - 0 = raw image
 *             1 = delta image
 *    *data  - an integer array containing (Tx rows * Rx columns)
 * return:
 *    succeed- 0
 *    fail   - (-EINVAL)
 */
static int intf_helper_get_rawordiff_data(int which, int *data)
{
	int retval = -EINVAL;
	struct syna_tcm_hcd *tcm_hcd = intf_hcd->tcm_hcd;
	unsigned char timeout_count = 0;
	short *p_data_16;
	struct syna_tcm_buffer buf;
	int i, j;

	if (!data) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"invalid parameter, data buffer\n");
		goto exit;
	}

	if (tcm_hcd->id_info.mode != MODE_APPLICATION ||
			tcm_hcd->app_status != APP_STATUS_OK) {

		intf_hcd->err_flag = true;

		LOGE(tcm_hcd->pdev->dev.parent,
				"invalid app status (id_info.mode = 0x%x) (app_status = 0x%x)\n",
				tcm_hcd->id_info.mode, tcm_hcd->app_status);
		retval =  -EINVAL;
		goto exit;
	}

	if (!intf_helper_set_report_type(which)) {
		retval = -EINVAL;
		LOGE(tcm_hcd->pdev->dev.parent, "failed to set report type\n");
		goto exit;
	}

	intf_hcd->report_is_ready = false;

	intf_hcd->report_rows =
		le2_to_uint(tcm_hcd->app_info.num_of_image_rows);
	intf_hcd->report_cols =
		le2_to_uint(tcm_hcd->app_info.num_of_image_cols);

	INIT_BUFFER(buf, false);

	/* send tcm command - to enable the report */
	if (intf_helper_enable_report(true) < 0) {
		retval = -EINVAL;
		LOGE(tcm_hcd->pdev->dev.parent,
			"failed to enable the requested report\n");
		goto exit;
	}

	/* waiting for the completion of requested report */
	do {
		if (timeout_count == INTF_REPORT_TIMEOUT_100MS)
			break;

		msleep(100);
		timeout_count++;
	} while (!intf_hcd->report_is_ready);

	if (timeout_count == INTF_REPORT_TIMEOUT_100MS) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"timeout waiting for a report image\n");

		intf_helper_enable_report(false); /* close the report stream */

		retval = -EINVAL;
		goto exit;
	}

	/* clone the request report data */
	LOCK_BUFFER(buf);

	buf.buf = kzalloc(tcm_hcd->report.buffer.data_length, GFP_KERNEL);
	if (!buf.buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"failed to allocate temporary buffer\n");
		UNLOCK_BUFFER(buf);

		intf_helper_enable_report(false); /* close the report stream */

		RELEASE_BUFFER(buf);
		retval = -EINVAL;
		goto exit;
	}
	buf.buf_size = tcm_hcd->report.buffer.data_length;
	buf.data_length = 0;

	memcpy((void *)buf.buf,
			(const void *)tcm_hcd->report.buffer.buf,
			buf.buf_size);
	buf.data_length = tcm_hcd->report.buffer.data_length;

	UNLOCK_BUFFER(buf);

	/* to disable the report stream */
	if (intf_helper_enable_report(false) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"failed to disable the report\n");
		return -EINVAL;
	}

	/* move data to the output buffer */
	p_data_16 = (short *)buf.buf;
	for (i = 0; i < intf_hcd->report_rows; i++) {
		for (j = 0; j < intf_hcd->report_cols; j++) {
			data[i*intf_hcd->report_cols + j] = (int)*p_data_16;

			p_data_16++;
		}
	}

	retval = 0;
	RELEASE_BUFFER(buf);

exit:
	return retval;
}


/*
 * sysfs name  - get_rawordiff_data
 * description - funciton will call intf_helper_get_rawordiff_data to get
 *               a report image
 * usage       -
 *               $ echo [0/1] > get_rawordiff_data
 */
static ssize_t intf_sysfs_get_rawordiff_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct syna_tcm_hcd *tcm_hcd = intf_hcd->tcm_hcd;
	int report_size;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	intf_hcd->err_flag = false;

	intf_hcd->report_rows =
		le2_to_uint(tcm_hcd->app_info.num_of_image_rows);
	intf_hcd->report_cols =
		le2_to_uint(tcm_hcd->app_info.num_of_image_cols);
	report_size = intf_hcd->report_rows * intf_hcd->report_cols;

	/* allocate the buffer for one report data */
	intf_hcd->report_data = kzalloc(report_size*sizeof(int), GFP_KERNEL);
	if (!intf_hcd->report_data) {

		intf_hcd->err_flag = true;

		retval =  -ENOMEM;
		LOGE(tcm_hcd->pdev->dev.parent,
				"failed to allocate mem for intf_hcd->report_data\n");
		goto exit;
	}

	/* to get the requested report */
	retval = intf_helper_get_rawordiff_data((int)input, intf_hcd->report_data);
	if (retval < 0) {
		intf_hcd->err_flag = true;

		LOGE(tcm_hcd->pdev->dev.parent,
				"failed to get the requested image (type: %x)\n",
				input);
		goto exit;
	}

	retval = count;

exit:
	return retval;
}
/*
 * sysfs name  - get_rawordiff_data
 * description - funciton will show the image data onto the stdout
 * usage       -
 *               $ cat get_rawordiff_data
 */
static ssize_t intf_sysfs_get_rawordiff_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int row, col;
	int i, j;
	int cnt;
	int count = 0;
	int *p_data;

	if (!intf_hcd->report_data)
		return snprintf(buf, PAGE_SIZE,
				"\nerror: report data is not allocated\n\n");

	if (intf_hcd->err_flag) {
		kfree(intf_hcd->report_data);
		intf_hcd->report_data = NULL;

		return snprintf(buf, PAGE_SIZE,
				"\nerror: unable to get the requested image\n\n");
	}

	/* print out the report data */
	row = intf_hcd->report_rows;
	col = intf_hcd->report_cols;

	p_data = &intf_hcd->report_data[0];
	for (i = 0; i < row; i++) {
		for (j = 0; j < col; j++) {
			if (intf_hcd->report_type == REPORT_RAW)
				cnt = snprintf(buf, PAGE_SIZE - count, "%-4d ", *p_data);
			else
				cnt = snprintf(buf, PAGE_SIZE - count, "%-3d ", *p_data);

			buf += cnt;
			count += cnt;

			p_data++;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;
	}

	snprintf(buf, PAGE_SIZE - count, "\n");
	count++;

	/* release the allocated buffer */
	kfree(intf_hcd->report_data);
	intf_hcd->report_data = NULL;

	return count;
}

static ssize_t syna_data_dump_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int retval;
	int cnt = 0;
	u8 *buffer = NULL;

	if (*pos != 0)
		return -EINVAL;
	buffer = vmalloc(PAGE_SIZE + PAGE_SIZE / 2);
	if (!buffer)
		return -ENOMEM;
	retval = intf_sysfs_get_rawordiff_data_store(NULL, NULL, "0", 2);
	if (retval < 0) {
		snprintf(buffer, PAGE_SIZE, "get differ data error\n");
		goto out;
	}
	retval = intf_sysfs_get_rawordiff_data_show(NULL, NULL, buffer);
	if (retval < 0) {
		snprintf(buffer, PAGE_SIZE, "get differ data error\n");
		goto out;
	}
	cnt += strlen(buffer);
	retval = intf_sysfs_get_rawordiff_data_store(NULL, NULL, "1", 2);
	if (retval < 0) {
		snprintf(buffer, PAGE_SIZE, "get raw data error\n");
		goto out;
	}
	retval = intf_sysfs_get_rawordiff_data_show(NULL, NULL, buffer + cnt);
	if (retval < 0) {
		snprintf(buffer, PAGE_SIZE, "get raw data error\n");
		goto out;
	}
	cnt = strlen(buffer);

	if (copy_to_user(buf, buffer, strlen(buffer))) {
		retval = -EFAULT;
		goto out;
	}
out:
	cnt = strlen(buffer);
	*pos += cnt;
	vfree(buffer);
	buffer = NULL;
	if (retval < 0)
		return retval;
	else
		return cnt;
}


static const struct file_operations syna_data_dump_ops = {
	.read		= syna_data_dump_read,
};


/*
 * module initialization
 * to allocate the struct xiaomi_intf_hcd, then create the sysfs files
 */
static int xiaomi_intf_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;

	LOGN(tcm_hcd->pdev->dev.parent, "+\n");

	intf_hcd = kzalloc(sizeof(struct xiaomi_intf_hcd), GFP_KERNEL);
	if (!intf_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"failed to allocate memory for xiaomi_intf_hcd\n");
		return -ENOMEM;
	}

	intf_hcd->tcm_hcd = tcm_hcd;

	intf_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!intf_hcd->sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"failed to create sysfs directory\n");
		goto err_sysfs_create_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(intf_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	intf_hcd->err_flag = false;
	intf_hcd->report_data = NULL;
	intf_hcd->report_cols = 0;
	intf_hcd->report_rows = 0;
	proc_create("tp_data_dump", 0644, NULL, &syna_data_dump_ops);

	return 0;

err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(intf_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(intf_hcd->sysfs_dir);

err_sysfs_create_dir:
	kfree(intf_hcd);
	intf_hcd = NULL;

	return retval;
}

/*
 * to remove the module and sysfs files as well
 */
static int xiaomi_intf_remove(struct syna_tcm_hcd *tcm_hcd)
{
	int idx;

	LOGN(tcm_hcd->pdev->dev.parent, "+\n");

	if (!intf_hcd)
		goto exit;

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++)
		sysfs_remove_file(intf_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(intf_hcd->sysfs_dir);

	kfree(intf_hcd);
	intf_hcd = NULL;

exit:
	complete(&xiaomi_intf_remove_complete);

	return 0;
}

/*
 * call by synaptics_tcm_core.c when a tcm message is dispatched
 */
static int xiaomi_intf_syncbox(struct syna_tcm_hcd *tcm_hcd)
{
	if (!intf_hcd)
		return 0;

	/* once to receive a requested report */
	if (tcm_hcd->report.id == intf_hcd->report_type)
		intf_hcd->report_is_ready = true;

	return 0;
}


/*
 * module definition, xiaomi_intf_module
 */

static struct syna_tcm_module_cb xiaomi_intf_module = {
	.type = TCM_XIAOMI_INTERFACE,
	.init = xiaomi_intf_init,
	.remove = xiaomi_intf_remove,
	.syncbox = xiaomi_intf_syncbox,
	.asyncbox = NULL,
	.reset = NULL,
	.suspend = NULL,
	.resume = NULL,
};

static int __init xiaomi_intf_module_init(void)
{
	return syna_tcm_add_module(&xiaomi_intf_module, true);
}

static void __exit xiaomi_intf_module_exit(void)
{
	syna_tcm_add_module(&xiaomi_intf_module, false);

	wait_for_completion(&xiaomi_intf_remove_complete);

	return;
}

late_initcall(xiaomi_intf_module_init);
module_exit(xiaomi_intf_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM XIAOMI Interface Module");
MODULE_LICENSE("GPL v2");
