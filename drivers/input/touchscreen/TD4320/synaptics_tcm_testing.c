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

#include <linux/gpio.h>
#include "synaptics_tcm_core.h"
#include "synaptics_tcm_testing.h"

#define SYSFS_DIR_NAME "testing"

#define REPORT_TIMEOUT_MS 500

#define testing_sysfs_show(t_name) \
static ssize_t testing_sysfs_##t_name##_show(struct device *dev, \
		struct device_attribute *attr, char *buf) \
{ \
	int retval; \
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = testing_##t_name(); \
	if (retval < 0) { \
		LOG_ERR(tcm_hcd->pdev->dev.parent, \
				"Failed to do "#t_name" test\n"); \
		goto exit; \
	} \
\
	retval = snprintf(buf, PAGE_SIZE, \
			"%s\n", \
			testing_hcd->result ? "Passed" : "Failed"); \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
}

enum test_code {
	TEST_NOT_IMPLEMENTED = 0,
	TEST_TRX_TRX_SHORTS = 1,
	TEST_TRX_SENSOR_OPENS = 2,
	TEST_TRX_GROUND_SHORTS = 3,
	TEST_DYNAMIC_RANGE = 7,
	TEST_OPEN_SHORT_DETECTOR = 8,
	TEST_NOISE = 10,
	TEST_PT11 = 11,
	TEST_PT12 = 12,
	TEST_PT13 = 13,
	TEST_DYNAMIC_RANGE_DOZE = 14,
	TEST_NOISE_DOZE = 15,
};

struct testing_hcd {
	bool result;
	unsigned char report_type;
	unsigned int report_index;
	unsigned int num_of_reports;
	struct kobject *sysfs_dir;
	struct syna_tcm_buffer out;
	struct syna_tcm_buffer resp;
	struct syna_tcm_buffer report;
	struct syna_tcm_buffer process;
	struct syna_tcm_buffer output;
	struct syna_tcm_hcd *tcm_hcd;
	int (*collect_reports)(enum report_type report_type,
			unsigned int num_of_reports);
};

DECLARE_COMPLETION(report_complete);

DECLARE_COMPLETION(testing_remove_complete);

static struct testing_hcd *testing_hcd;

static int testing_dynamic_range(void);

static int testing_dynamic_range_lpwg(void);

static int testing_dynamic_range_doze(void);

static int testing_noise(void);

static int testing_noise_lpwg(void);

static int testing_noise_doze(void);

static int testing_open_short_detector(void);

static int testing_pt11(void);

static int testing_pt12(void);

static int testing_pt13(void);

static int testing_reset_open(void);

static int testing_lockdown(void);

static int testing_trx(enum test_code test_code);

SHOW_PROTOTYPE(testing, dynamic_range)
SHOW_PROTOTYPE(testing, dynamic_range_lpwg)
SHOW_PROTOTYPE(testing, dynamic_range_doze)
SHOW_PROTOTYPE(testing, noise)
SHOW_PROTOTYPE(testing, noise_lpwg)
SHOW_PROTOTYPE(testing, noise_doze)
SHOW_PROTOTYPE(testing, open_short_detector)
SHOW_PROTOTYPE(testing, pt11)
SHOW_PROTOTYPE(testing, pt12)
SHOW_PROTOTYPE(testing, pt13)
SHOW_PROTOTYPE(testing, reset_open)
SHOW_PROTOTYPE(testing, lockdown)
SHOW_PROTOTYPE(testing, trx_trx_shorts)
SHOW_PROTOTYPE(testing, trx_sensor_opens)
SHOW_PROTOTYPE(testing, trx_ground_shorts)
SHOW_PROTOTYPE(testing, size)

static struct device_attribute *attrs[] = {
	ATTRIFY(dynamic_range),
	ATTRIFY(dynamic_range_lpwg),
	ATTRIFY(dynamic_range_doze),
	ATTRIFY(noise),
	ATTRIFY(noise_lpwg),
	ATTRIFY(noise_doze),
	ATTRIFY(open_short_detector),
	ATTRIFY(pt11),
	ATTRIFY(pt12),
	ATTRIFY(pt13),
	ATTRIFY(reset_open),
	ATTRIFY(lockdown),
	ATTRIFY(trx_trx_shorts),
	ATTRIFY(trx_sensor_opens),
	ATTRIFY(trx_ground_shorts),
	ATTRIFY(size),
};

static ssize_t testing_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct bin_attribute bin_attr = {
	.attr = {
		.name = "data",
		.mode = 0444,
	},
	.size = 0,
	.read = testing_sysfs_data_show,
};

testing_sysfs_show(dynamic_range)

testing_sysfs_show(dynamic_range_lpwg)

testing_sysfs_show(dynamic_range_doze)

testing_sysfs_show(noise)

testing_sysfs_show(noise_lpwg)

testing_sysfs_show(noise_doze)

testing_sysfs_show(open_short_detector)

testing_sysfs_show(pt11)

testing_sysfs_show(pt12)

testing_sysfs_show(pt13)

testing_sysfs_show(reset_open)

testing_sysfs_show(lockdown)

static ssize_t testing_sysfs_trx_trx_shorts_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = testing_trx(TEST_TRX_TRX_SHORTS);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to do TRX-TRX shorts test\n");
		goto exit;
	}

	retval = snprintf(buf, PAGE_SIZE,
			"%s\n",
			testing_hcd->result ? "Passed" : "Failed");

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_trx_sensor_opens_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = testing_trx(TEST_TRX_SENSOR_OPENS);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to do TRX-sensor opens test\n");
		goto exit;
	}

	retval = snprintf(buf, PAGE_SIZE,
			"%s\n",
			testing_hcd->result ? "Passed" : "Failed");

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_trx_ground_shorts_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = testing_trx(TEST_TRX_GROUND_SHORTS);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to do TRX-ground shorts test\n");
		goto exit;
	}

	retval = snprintf(buf, PAGE_SIZE,
			"%s\n",
			testing_hcd->result ? "Passed" : "Failed");

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	LOCK_BUFFER(testing_hcd->output);

	retval = snprintf(buf, PAGE_SIZE,
			"%u\n",
			testing_hcd->output.data_length);

	UNLOCK_BUFFER(testing_hcd->output);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int readlen;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	LOCK_BUFFER(testing_hcd->output);

	readlen = MIN(count, testing_hcd->output.data_length - pos);

	retval = secure_memcpy(buf,
			count,
			&testing_hcd->output.buf[pos],
			testing_hcd->output.buf_size - pos,
			readlen);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
			"Failed to copy report data\n");
	} else {
		retval = readlen;
	}

	UNLOCK_BUFFER(testing_hcd->output);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int testing_run_prod_test_item(enum test_code test_code)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	if (tcm_hcd->features.dual_firmware &&
			tcm_hcd->id_info.mode != MODE_PRODUCTION_TEST) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_PRODUCTION_TEST);
		if (retval < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to run production test firmware\n");
			return retval;
		}
	} else if (tcm_hcd->id_info.mode != MODE_APPLICATION ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Application firmware not running\n");
		return -ENODEV;
	}

	LOCK_BUFFER(testing_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->out,
			1);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->out.buf\n");
		UNLOCK_BUFFER(testing_hcd->out);
		return retval;
	}

	testing_hcd->out.buf[0] = test_code;

	LOCK_BUFFER(testing_hcd->resp);
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_PRODUCTION_TEST,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_PRODUCTION_TEST));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		return retval;
	}

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);

	return 0;
}

static int testing_collect_reports(enum report_type report_type,
		unsigned int num_of_reports)
{
	int retval;
	bool completed;
	unsigned int timeout;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	testing_hcd->report_index = 0;
	testing_hcd->report_type = report_type;
	testing_hcd->num_of_reports = num_of_reports;

	reinit_completion(&report_complete);

	LOCK_BUFFER(testing_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->out,
			1);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->out.buf\n");
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	testing_hcd->out.buf[0] = testing_hcd->report_type;

	LOCK_BUFFER(testing_hcd->resp);
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_ENABLE_REPORT,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ENABLE_REPORT));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);

	completed = false;
	timeout = REPORT_TIMEOUT_MS * num_of_reports;

	retval = wait_for_completion_timeout(&report_complete,
			msecs_to_jiffies(timeout));
	if (retval == 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Timed out waiting for report collection\n");
	} else {
		completed = true;
	}

	LOCK_BUFFER(testing_hcd->out);

	testing_hcd->out.buf[0] = testing_hcd->report_type;

	LOCK_BUFFER(testing_hcd->resp);
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_DISABLE_REPORT,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_DISABLE_REPORT));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);

	if (completed)
		retval = 0;
	else
		retval = -EIO;

exit:
	testing_hcd->report_type = 0;

	return retval;
}

static void testing_get_frame_size_words(unsigned int *size, bool image_only)
{
	unsigned int rows;
	unsigned int cols;
	unsigned int hybrid;
	unsigned int buttons;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);
	hybrid = le2_to_uint(app_info->has_hybrid_data);
	buttons = le2_to_uint(app_info->num_of_buttons);

	*size = rows * cols;

	if (!image_only) {
		if (hybrid)
			*size += rows + cols;
		*size += buttons;
	}
}

static void testing_doze_frame_output(unsigned int rows, unsigned int cols)
{
	int retval;
	unsigned int data_size;
	unsigned int header_size;
	unsigned int output_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	header_size = 2;

	data_size = rows * cols;

	if (le2_to_uint(app_info->num_of_buttons))
		data_size++;

	output_size = header_size + data_size * 2;

	LOCK_BUFFER(testing_hcd->output);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			output_size);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	testing_hcd->output.buf[0] = rows;
	testing_hcd->output.buf[1] = cols;

	output_size = header_size;

	LOCK_BUFFER(testing_hcd->resp);

	retval = secure_memcpy(testing_hcd->output.buf + header_size,
			testing_hcd->output.buf_size - header_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size += testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_hcd->output.data_length = output_size;

	UNLOCK_BUFFER(testing_hcd->output);
}

static void testing_standard_frame_output(bool image_only)
{
	int retval;
	unsigned int data_size;
	unsigned int header_size;
	unsigned int output_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	testing_get_frame_size_words(&data_size, image_only);

	header_size = sizeof(app_info->num_of_buttons) +
			sizeof(app_info->num_of_image_rows) +
			sizeof(app_info->num_of_image_cols) +
			sizeof(app_info->has_hybrid_data);

	output_size = header_size + data_size * 2;

	LOCK_BUFFER(testing_hcd->output);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			output_size);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			&app_info->num_of_buttons[0],
			header_size,
			header_size);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to copy header data\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size = header_size;

	LOCK_BUFFER(testing_hcd->resp);

	retval = secure_memcpy(testing_hcd->output.buf + header_size,
			testing_hcd->output.buf_size - header_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size += testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_hcd->output.data_length = output_size;

	UNLOCK_BUFFER(testing_hcd->output);
}

static int testing_dynamic_range_doze(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int data;
	unsigned int rows;
	unsigned int cols;
	unsigned int data_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	cols = le2_to_uint(app_info->num_of_image_cols);

	retval = testing_run_prod_test_item(TEST_DYNAMIC_RANGE_DOZE);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	data_size = testing_hcd->resp.data_length / 2;

	if (le2_to_uint(app_info->num_of_buttons))
		data_size--;

	if (data_size % cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Invalid max number of rows per burst\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	rows = data_size / cols;

	limits_rows = ARRAY_SIZE(drt_hi_limits);
	limits_cols = ARRAY_SIZE(drt_hi_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(drt_lo_limits);
	limits_cols = ARRAY_SIZE(drt_lo_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = le2_to_uint(&buf[idx * 2]);
			if (data > drt_hi_limits[row][col] ||
					data < drt_lo_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_doze_frame_output(rows, cols);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static int testing_dynamic_range_lpwg(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
			DC_IN_WAKEUP_GESTURE_MODE,
			1);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to enable wakeup gesture mode\n");
		return retval;
	}

	retval = testing_dynamic_range();
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to do dynamic range test\n");
		return retval;
	}

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
			DC_IN_WAKEUP_GESTURE_MODE,
			0);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to disable wakeup gesture mode\n");
		return retval;
	}

	return 0;
}

static int testing_dynamic_range(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int data;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&frame_size_words, false);

	retval = testing_run_prod_test_item(TEST_DYNAMIC_RANGE);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size_words != testing_hcd->resp.data_length / 2) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(drt_hi_limits);
	limits_cols = ARRAY_SIZE(drt_hi_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(drt_lo_limits);
	limits_cols = ARRAY_SIZE(drt_lo_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = le2_to_uint(&buf[idx * 2]);
			if (data > drt_hi_limits[row][col] ||
					data < drt_lo_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(false);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static int testing_noise_doze(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int data_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	cols = le2_to_uint(app_info->num_of_image_cols);

	retval = testing_run_prod_test_item(TEST_NOISE_DOZE);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	data_size = testing_hcd->resp.data_length / 2;

	if (le2_to_uint(app_info->num_of_buttons))
		data_size--;

	if (data_size % cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Invalid max number of rows per burst\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	rows = data_size / cols;

	limits_rows = ARRAY_SIZE(noise_limits);
	limits_cols = ARRAY_SIZE(noise_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data > noise_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_doze_frame_output(rows, cols);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static int testing_noise_lpwg(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
			DC_IN_WAKEUP_GESTURE_MODE,
			1);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to enable wakeup gesture mode\n");
		return retval;
	}

	retval = testing_noise();
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to do noise test\n");
		return retval;
	}

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
			DC_IN_WAKEUP_GESTURE_MODE,
			0);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to disable wakeup gesture mode\n");
		return retval;
	}

	return 0;
}

static int testing_noise(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&frame_size_words, false);

	retval = testing_run_prod_test_item(TEST_NOISE);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size_words != testing_hcd->resp.data_length / 2) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(noise_limits);
	limits_cols = ARRAY_SIZE(noise_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data > noise_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(false);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static void testing_open_short_detector_output(void)
{
	int retval;
	unsigned int rows;
	unsigned int cols;
	unsigned int data_size;
	unsigned int header_size;
	unsigned int output_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);
	data_size = (rows * cols + 7) / 8;

	header_size = sizeof(app_info->num_of_buttons) +
			sizeof(app_info->num_of_image_rows) +
			sizeof(app_info->num_of_image_cols) +
			sizeof(app_info->has_hybrid_data);

	output_size = header_size + data_size * 2;

	LOCK_BUFFER(testing_hcd->output);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			output_size);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			&app_info->num_of_buttons[0],
			header_size,
			header_size);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to copy header data\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size = header_size;

	LOCK_BUFFER(testing_hcd->resp);

	retval = secure_memcpy(testing_hcd->output.buf + header_size,
			testing_hcd->output.buf_size - header_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size += testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_hcd->output.data_length = output_size;

	UNLOCK_BUFFER(testing_hcd->output);
}

static int testing_open_short_detector(void)
{
	int retval;
	unsigned int bit;
	unsigned int byte;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int data_size;
	unsigned char *data;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);
	data_size = (rows * cols + 7) / 8;

	retval = testing_run_prod_test_item(TEST_OPEN_SHORT_DETECTOR);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (data_size * 2 != testing_hcd->resp.data_length) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Data size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	testing_hcd->result = true;

	bit = 0;
	byte = 0;
	data = &testing_hcd->resp.buf[0];
	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			if (data[byte] & (1 << bit)) {
				testing_hcd->result = false;
				break;
			}
			if (bit++ > 7) {
				bit = 0;
				byte++;
			}
		}
	}

	if (testing_hcd->result == true) {
		bit = 0;
		byte = 0;
		data = &testing_hcd->resp.buf[data_size];
		for (row = 0; row < rows; row++) {
			for (col = 0; col < cols; col++) {
				if (data[byte] & (1 << bit)) {
					testing_hcd->result = false;
					break;
				}
				if (bit++ > 7) {
					bit = 0;
					byte++;
				}
			}
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_open_short_detector_output();

	retval = 0;

exit:
	if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to do reset\n");
	}

	return retval;
}

static int testing_pt11(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int image_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&image_size_words, true);

	retval = testing_run_prod_test_item(TEST_PT11);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (image_size_words != testing_hcd->resp.data_length / 2) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Image size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(pt11_hi_limits);
	limits_cols = ARRAY_SIZE(pt11_hi_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(pt11_lo_limits);
	limits_cols = ARRAY_SIZE(pt11_lo_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data > pt11_hi_limits[row][col] ||
					data < pt11_lo_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(true);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static int testing_pt12(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int image_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&image_size_words, true);

	retval = testing_run_prod_test_item(TEST_PT12);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (image_size_words != testing_hcd->resp.data_length / 2) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Image size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(pt12_limits);
	limits_cols = ARRAY_SIZE(pt12_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data < pt12_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(true);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static int testing_pt13(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int image_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&image_size_words, true);

	retval = testing_run_prod_test_item(TEST_PT13);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (image_size_words != testing_hcd->resp.data_length / 2) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Image size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(pt13_limits);
	limits_cols = ARRAY_SIZE(pt13_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data < pt13_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(true);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static int testing_reset_open(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (bdata->reset_gpio < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Hardware reset unavailable\n");
		return -EINVAL;
	}

	mutex_lock(&tcm_hcd->reset_mutex);

	tcm_hcd->update_watchdog(tcm_hcd, false);

	gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
	msleep(bdata->reset_active_ms);
	gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
	msleep(bdata->reset_delay_ms);

	tcm_hcd->update_watchdog(tcm_hcd, true);

	mutex_unlock(&tcm_hcd->reset_mutex);

	if (tcm_hcd->id_info.mode == MODE_APPLICATION) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_BOOTLOADER);
		if (retval < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to enter bootloader mode\n");
			return retval;
		}
	} else {
		retval = tcm_hcd->identify(tcm_hcd, false);
		if (retval < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do identification\n");
			goto run_app_firmware;
		}
	}

	if (tcm_hcd->boot_info.last_reset_reason == reset_open_limit)
		testing_hcd->result = true;
	else
		testing_hcd->result = false;

	retval = 0;

run_app_firmware:
	if (tcm_hcd->switch_mode(tcm_hcd, FW_MODE_APPLICATION) < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware\n");
	}

	return retval;
}

static void testing_lockdown_output(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	LOCK_BUFFER(testing_hcd->output);
	LOCK_BUFFER(testing_hcd->resp);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	testing_hcd->output.data_length = testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->output);
}

static int testing_lockdown(void)
{
	int retval;
	unsigned int idx;
	unsigned int lockdown_size;
	unsigned int limits_size;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	if (tcm_hcd->read_flash_data == NULL) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Unable to read from flash\n");
		return -EINVAL;
	}

	LOCK_BUFFER(testing_hcd->resp);

	retval = tcm_hcd->read_flash_data(CUSTOM_OTP, true, &testing_hcd->resp);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to read lockdown data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		return retval;
	}

	lockdown_size = testing_hcd->resp.data_length;

	limits_size = sizeof(lockdown_limits) / sizeof(*lockdown_limits);

	if (lockdown_size != limits_size) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		return -EINVAL;
	}

	testing_hcd->result = true;

	for (idx = 0; idx < lockdown_size; idx++) {
		if (testing_hcd->resp.buf[idx] != lockdown_limits[idx]) {
			testing_hcd->result = false;
			break;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_lockdown_output();

	return 0;
}

static void testing_trx_output(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	LOCK_BUFFER(testing_hcd->output);
	LOCK_BUFFER(testing_hcd->resp);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	testing_hcd->output.data_length = testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->output);
}

static int testing_trx(enum test_code test_code)
{
	int retval;
	unsigned char pass_vector;
	unsigned int idx;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	switch (test_code) {
	case TEST_TRX_TRX_SHORTS:
	case TEST_TRX_GROUND_SHORTS:
		pass_vector = 0xff;
		break;
	case TEST_TRX_SENSOR_OPENS:
		pass_vector = 0x00;
		break;
	default:
		return -EINVAL;
	}

	retval = testing_run_prod_test_item(test_code);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	testing_hcd->result = true;

	for (idx = 0; idx < testing_hcd->resp.data_length; idx++) {
		if (testing_hcd->resp.buf[idx] != pass_vector) {
			testing_hcd->result = false;
			break;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_trx_output();

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static void testing_report(void)
{
	int retval;
	unsigned int offset;
	unsigned int report_size;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	report_size = tcm_hcd->report.buffer.data_length;

	LOCK_BUFFER(testing_hcd->report);

	if (testing_hcd->report_index == 0) {
		retval = syna_tcm_alloc_mem(tcm_hcd,
				&testing_hcd->report,
				report_size * testing_hcd->num_of_reports);
		if (retval < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for testing_hcd->report.buf\n");
			UNLOCK_BUFFER(testing_hcd->report);
			return;
		}
	}

	if (testing_hcd->report_index < testing_hcd->num_of_reports) {
		offset = report_size * testing_hcd->report_index;

		retval = secure_memcpy(testing_hcd->report.buf + offset,
				testing_hcd->report.buf_size - offset,
				tcm_hcd->report.buffer.buf,
				tcm_hcd->report.buffer.buf_size,
				tcm_hcd->report.buffer.data_length);
		if (retval < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to copy report data\n");
			UNLOCK_BUFFER(testing_hcd->report);
			return;
		}

		testing_hcd->report_index++;
		testing_hcd->report.data_length += report_size;
	}

	UNLOCK_BUFFER(testing_hcd->report);

	if (testing_hcd->report_index == testing_hcd->num_of_reports)
		complete(&report_complete);
}

static int testing_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;

	testing_hcd = kzalloc(sizeof(*testing_hcd), GFP_KERNEL);
	if (!testing_hcd) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd\n");
		return -ENOMEM;
	}

	testing_hcd->tcm_hcd = tcm_hcd;

	testing_hcd->collect_reports = testing_collect_reports;

	INIT_BUFFER(testing_hcd->out, false);
	INIT_BUFFER(testing_hcd->resp, false);
	INIT_BUFFER(testing_hcd->report, false);
	INIT_BUFFER(testing_hcd->process, false);
	INIT_BUFFER(testing_hcd->output, false);

	testing_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!testing_hcd->sysfs_dir) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(testing_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOG_ERR(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	retval = sysfs_create_bin_file(testing_hcd->sysfs_dir, &bin_attr);
	if (retval < 0) {
		LOG_ERR(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs bin file\n");
		goto err_sysfs_create_bin_file;
	}

	return 0;

err_sysfs_create_bin_file:
err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(testing_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(testing_hcd->sysfs_dir);

err_sysfs_create_dir:
	RELEASE_BUFFER(testing_hcd->output);
	RELEASE_BUFFER(testing_hcd->process);
	RELEASE_BUFFER(testing_hcd->report);
	RELEASE_BUFFER(testing_hcd->resp);
	RELEASE_BUFFER(testing_hcd->out);

	kfree(testing_hcd);
	testing_hcd = NULL;

	return retval;
}

static int testing_remove(struct syna_tcm_hcd *tcm_hcd)
{
	int idx;

	if (!testing_hcd)
		goto exit;

	sysfs_remove_bin_file(testing_hcd->sysfs_dir, &bin_attr);

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++)
		sysfs_remove_file(testing_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(testing_hcd->sysfs_dir);

	RELEASE_BUFFER(testing_hcd->output);
	RELEASE_BUFFER(testing_hcd->process);
	RELEASE_BUFFER(testing_hcd->report);
	RELEASE_BUFFER(testing_hcd->resp);
	RELEASE_BUFFER(testing_hcd->out);

	kfree(testing_hcd);
	testing_hcd = NULL;

exit:
	complete(&testing_remove_complete);

	return 0;
}

static int testing_reset(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	if (!testing_hcd) {
		retval = testing_init(tcm_hcd);
		return retval;
	}

	return 0;
}

static int testing_syncbox(struct syna_tcm_hcd *tcm_hcd)
{
	if (!testing_hcd)
		return 0;

	if (tcm_hcd->report.id == testing_hcd->report_type)
		testing_report();

	return 0;
}

static struct syna_tcm_module_cb testing_module = {
	.type = TCM_TESTING,
	.init = testing_init,
	.remove = testing_remove,
	.syncbox = testing_syncbox,
	.asyncbox = NULL,
	.reset = testing_reset,
	.suspend = NULL,
	.resume = NULL,
	.early_suspend = NULL,
};

static int __init testing_module_init(void)
{
	return syna_tcm_add_module(&testing_module, true);
}

static void __exit testing_module_exit(void)
{
	syna_tcm_add_module(&testing_module, false);

	wait_for_completion(&testing_remove_complete);
}

module_init(testing_module_init);
module_exit(testing_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Testing Module");
MODULE_LICENSE("GPL v2");
