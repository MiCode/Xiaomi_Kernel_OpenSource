/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018-2019 Ian Su <ian.su@tw.synaptics.com>
 * Copyright (C) 2018-2019 Joey Zhou <joey.zhou@synaptics.com>
 * Copyright (C) 2018-2019 Yuehao Qiu <yuehao.qiu@synaptics.com>
 * Copyright (C) 2018-2019 Aaron Chen <aaron.chen@tw.synaptics.com>
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
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/firmware.h>
#include "synaptics_tcm_core.h"
#include "synaptics_tcm_testing.h"

#define SYSFS_DIR_NAME "testing"

#define REPORT_TIMEOUT_MS 5000

#define FORMAT_4D	"%4hd "
#define FORMAT_5D	"%5hd "
#define FORMAT_4U	"%4hu "
#define FORMAT_5U	"%5hu "

#if (USE_KOBJ_SYSFS)
#define testing_sysfs_show(t_name) \
static ssize_t testing_sysfs_##t_name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, char *buf) \
{ \
	int retval; \
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = testing_##t_name(); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
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
#else
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
		LOGE(tcm_hcd->pdev->dev.parent, \
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
#endif

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))

/* Sync with the Comm2 Release 21 */
/* Not all are implemented for every ASIC */
enum test_code {
	TEST_NOT_IMPLEMENTED = 0x00,
	TEST_PT1_TRX_TRX_SHORTS = 0x01,
	TEST_PT2_TRX_SENSOR_OPENS = 0x02,
	TEST_PT3_TRX_GROUND_SHORTS = 0x03,
	TEST_PT5_FULL_RAW_CAP = 0x05,
	TEST_PT6_EE_SHORT = 0x06,
	TEST_PT7_DYNAMIC_RANGE = 0x07,
	TEST_PT8_HIGH_RESISTANCE = 0x08,
	TEST_PT10_DELTA_NOISE = 0x0a,
	TEST_PT11_OPEN_DETECTION = 0x0b,
	TEST_PT12 = 0x0c,
	TEST_PT13 = 0x0d,
	TEST_PT14_DOZE_DYNAMIC_RANGE = 0x0e,
	TEST_PT15_DOZE_NOISE = 0x0f,
	TEST_PT16_SENSOR_SPEED = 0x10,
	TEST_PT17_ADC_RANGE = 0x11,
	TEST_PT18_HYBRID_ABS_RAW = 0x12,
	TEST_PT22_TRANS_RAW_CAP = 0x16,
	TEST_PT29_HYBRID_ABS_NOISE = 0x1D,
};

struct testing_hcd {
	bool result;
	unsigned char report_type;
	enum test_code test_item;
	unsigned int report_index;
	unsigned int num_of_reports;
	struct kobject *sysfs_dir;
	struct syna_tcm_buffer out;
	struct syna_tcm_buffer resp;
	struct syna_tcm_buffer report;
	struct syna_tcm_buffer process;
	struct syna_tcm_buffer output;
	struct syna_tcm_buffer pt_hi_limits;
	struct syna_tcm_buffer pt_lo_limits;
	struct syna_tcm_hcd *tcm_hcd;
	int (*collect_reports)(enum report_type report_type,
			unsigned int num_of_reports);
};

DECLARE_COMPLETION(report_complete);

DECLARE_COMPLETION(testing_remove_complete);

static struct testing_hcd *testing_hcd;


/* testing implementation */
static int testing_device_id(void);

static int testing_config_id(void);

static int testing_reset_open(void);

static int testing_pt01_trx_trx_short(void);

static int testing_pt03_trx_ground(void);

static int testing_pt05_full_raw(void);

static int testing_pt07_dynamic_range(void);

static int testing_pt10_noise(void);

static int testing_pt11_open_detection(void);

static int testing_pt18_hybrid_abs_raw(void);

static int testing_self_test(char *buf);

#if (USE_KOBJ_SYSFS)
KOBJ_SHOW_PROTOTYPE(testing, size)
KOBJ_SHOW_PROTOTYPE(testing, device_id)
KOBJ_SHOW_PROTOTYPE(testing, config_id)
KOBJ_SHOW_PROTOTYPE(testing, pt01_trx_trx_short)
KOBJ_SHOW_PROTOTYPE(testing, pt03_trx_ground)
KOBJ_SHOW_PROTOTYPE(testing, pt05_full_raw)
KOBJ_SHOW_PROTOTYPE(testing, pt07_dynamic_range)
KOBJ_SHOW_PROTOTYPE(testing, pt10_noise)
KOBJ_SHOW_PROTOTYPE(testing, pt11_open_detection)
KOBJ_SHOW_PROTOTYPE(testing, pt18_hybrid_abs_raw)
KOBJ_SHOW_PROTOTYPE(testing, reset_open)
KOBJ_SHOW_PROTOTYPE(testing, self_test)
KOBJ_SHOW_PROTOTYPE(testing, rid18_data)
KOBJ_SHOW_PROTOTYPE(testing, rid161_data)

static struct kobj_attribute *attrs[] = {
	KOBJ_ATTRIFY(size),
	KOBJ_ATTRIFY(device_id),
	KOBJ_ATTRIFY(config_id),
	KOBJ_ATTRIFY(pt01_trx_trx_short),
	KOBJ_ATTRIFY(pt03_trx_ground),
	KOBJ_ATTRIFY(pt05_full_raw),
	KOBJ_ATTRIFY(pt07_dynamic_range),
	KOBJ_ATTRIFY(pt10_noise),
	KOBJ_ATTRIFY(pt11_open_detection),
	KOBJ_ATTRIFY(pt18_hybrid_abs_raw),
	KOBJ_ATTRIFY(reset_open),
	KOBJ_ATTRIFY(self_test),
	KOBJ_ATTRIFY(rid18_data),
	KOBJ_ATTRIFY(rid161_data)
};
#else
/* nodes for testing */
SHOW_PROTOTYPE(testing, size)
SHOW_PROTOTYPE(testing, device_id)
SHOW_PROTOTYPE(testing, config_id)
SHOW_PROTOTYPE(testing, pt01_trx_trx_short)
SHOW_PROTOTYPE(testing, pt03_trx_ground)
SHOW_PROTOTYPE(testing, pt05_full_raw)
SHOW_PROTOTYPE(testing, pt07_dynamic_range)
SHOW_PROTOTYPE(testing, pt10_noise)
SHOW_PROTOTYPE(testing, pt11_open_detection)
SHOW_PROTOTYPE(testing, pt18_hybrid_abs_raw)
SHOW_PROTOTYPE(testing, reset_open)
SHOW_PROTOTYPE(testing, self_test)
SHOW_PROTOTYPE(testing, rid18_data)
SHOW_PROTOTYPE(testing, rid161_data)

static struct device_attribute *attrs[] = {
	ATTRIFY(size),
	ATTRIFY(device_id),
	ATTRIFY(config_id),
	ATTRIFY(pt01_trx_trx_short),
	ATTRIFY(pt03_trx_ground),
	ATTRIFY(pt05_full_raw),
	ATTRIFY(pt07_dynamic_range),
	ATTRIFY(pt10_noise),
	ATTRIFY(pt11_open_detection),
	ATTRIFY(pt18_hybrid_abs_raw),
	ATTRIFY(reset_open),
	ATTRIFY(self_test),
	ATTRIFY(rid18_data),
	ATTRIFY(rid161_data)
};
#endif

static ssize_t testing_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct bin_attribute bin_attr = {
	.attr = {
		.name = "data",
		.mode = S_IRUGO,
	},
	.size = 0,
	.read = testing_sysfs_data_show,
};

testing_sysfs_show(device_id)

testing_sysfs_show(config_id)

testing_sysfs_show(pt01_trx_trx_short)

testing_sysfs_show(pt03_trx_ground)

testing_sysfs_show(pt05_full_raw)

testing_sysfs_show(pt07_dynamic_range)

testing_sysfs_show(pt10_noise)

testing_sysfs_show(pt11_open_detection)

testing_sysfs_show(pt18_hybrid_abs_raw)

testing_sysfs_show(reset_open)

#if (USE_KOBJ_SYSFS)
static ssize_t testing_sysfs_size_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
#else
static ssize_t testing_sysfs_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
#endif
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
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to copy report data\n");
	} else {
		retval = readlen;
	}

	UNLOCK_BUFFER(testing_hcd->output);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static void goto_next_line(char **p)
{
	while (**p != '\n') {
		if (**p == '\0') {
			return;
		}
		*p = *p + 1;
	}
	*p = *p + 1;
	return;
}

static int testing_get_limits_bytes(unsigned int *row, unsigned int *col, enum test_code code)
{
	unsigned int rows;
	unsigned int cols;
	unsigned int size = 0;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	switch(code) {
		case TEST_PT1_TRX_TRX_SHORTS:
		case TEST_PT3_TRX_GROUND_SHORTS:
			*row = 1;
			*col = PT1_PT3_LIMITS_BYTES_SIZE;
			size = PT1_PT3_LIMITS_BYTES_SIZE;
			break;
		case TEST_PT5_FULL_RAW_CAP:
			*row = rows;
			*col = cols;
			size = rows*cols*2;
			break;
		case TEST_PT22_TRANS_RAW_CAP:
			*row = rows;
			*col = cols;
			size = rows*cols*2;
			break;
		case TEST_PT10_DELTA_NOISE:
			*row = 1;
			*col = PT10_LIMITS_BYTES_SIZE;
			size = PT10_LIMITS_BYTES_SIZE*2;
			break;
		case TEST_PT18_HYBRID_ABS_RAW:
			*row = 1;
			*col = rows + cols;
			size = (rows + cols)*4;
			break;
		default:
			*row = rows;
			*col = cols;
			size = 0;
			break;
	}

	return size;
}

static int testing_copy_valid_data(struct syna_tcm_buffer *dest_buffer, char *src_buf, enum test_code code)
{
	int retval = 0;
	int i, j, n;
	int offset = 0;
	long data;
	unsigned int byte_cnt = 0;
	unsigned int limit_cnt = 0;
	char *pdest = dest_buffer->buf;
	char *psrc = src_buf;
	char data_buf[DATA_SIZE_MAX+2] = {0};
	unsigned int limit_rows;
	unsigned int limit_cols;
	unsigned int buf_size_bytes = 0;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	if (dest_buffer == NULL || src_buf == NULL) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid src or dest pointer\n");
		retval = -EINVAL;
		goto exit;
	}

	buf_size_bytes = testing_get_limits_bytes(&limit_rows, &limit_cols, code);
	if (buf_size_bytes == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get buf_size_bytes for PT%d\n",code);
		retval = -EINVAL;
		goto exit;
	}

	if (buf_size_bytes > dest_buffer->buf_size) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Incorrect buf_size[%d] for PT%d, expected buf bytes size[%d]\n",
				dest_buffer->buf_size, code, buf_size_bytes);
		retval = -EINVAL;
		goto exit;
	}

	byte_cnt = 0;
	limit_cnt = 0;
	for (i = 0; i < limit_rows; i++) {
		for (j = 0; j < limit_cols; j++) {
			/* get one data */
			n = 0;
			data_buf[0] = '\0';
			while (n < DATA_SIZE_MAX) {
				if (*psrc == '\n' || *psrc == '\0' || *psrc == '\r') {
					break;
				} else if (*psrc == ',') {
					psrc++;
					break;
				} else if (*psrc == ' ') {
					psrc++;
					continue;
				} else {
					data_buf[n] = *psrc;
					n++;
					psrc++;
				}
			}
			data_buf[n] = '\0';

			if (strlen(data_buf) == 0)
				continue;

			if (kstrtol(data_buf, 0, &data)) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Err to convert string(%s) to a long value for PT%d\n",
					data_buf, code);
				retval = -EINVAL;
				goto exit;
			}

			if (code == TEST_PT18_HYBRID_ABS_RAW) {
				uint_to_le4(pdest, data);
				offset = 4;
			} else if (code == TEST_PT1_TRX_TRX_SHORTS ||
						code == TEST_PT3_TRX_GROUND_SHORTS) {
				uint_to_le1(pdest, data);
				offset = 1;
			} else {
				uint_to_le2(pdest, data);
				offset = 2;
			}

			pdest += offset;
			byte_cnt += offset;
			limit_cnt += 1;
		}
		goto_next_line(&psrc);
	}

	if (byte_cnt != buf_size_bytes) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Incorrect valid limit bytes size[%d], expected bytes size[%d]\n",
			byte_cnt, buf_size_bytes);
		goto exit;
	}

	if (limit_cnt != (limit_rows*limit_cols)) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Incorrect valid limit data size[%d], expected size[%d]\n",
			limit_cnt, limit_rows*limit_cols);
		goto exit;
	}

	dest_buffer->data_length = byte_cnt;

exit:
	return retval;
}

static int testing_parse_csv_data(struct syna_tcm_buffer *dest_buffer, char *src_buf,
						char *name, enum test_code code)
{
	int retval;
	char *psrc = src_buf;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	if (!dest_buffer || !src_buf || !name) {
		retval = -EINVAL;
		LOGE(tcm_hcd->pdev->dev.parent,
			"Invalid buf pointer for %s\n", name);
		goto exit;
	}

	psrc = strstr(psrc, name);
	if (!psrc) {
		retval = -EINTR;
		LOGE(tcm_hcd->pdev->dev.parent, "search %s failed\n", name);
		goto exit;
	}

	goto_next_line(&psrc);
	if (!psrc || (strlen(psrc) == 0)) {
		retval = -EIO;
		LOGE(tcm_hcd->pdev->dev.parent,
			"there is no valid data for %s\n", name);
		goto exit;
	}

	retval = testing_copy_valid_data(dest_buffer, psrc, code);
	if(retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"load PT%d %s data: Failed[%d]\n", code, name, retval);
		goto exit;
	} else {
		LOGI(tcm_hcd->pdev->dev.parent,
			"load PT%d %s data: Success\n", code, name);
	}

exit:
	return retval;
}

static int testing_load_testlimits(enum test_code testcode, unsigned int gapdiff)
{
	int retval = 0;
	char *csv_buf = NULL;
	char limit_file_name[100] = {0};
	char test_name0[100] = {0};
	char test_name1[100] = {0};
	unsigned int rows;
	unsigned int cols;
	unsigned int buf_size_bytes = 0;
	const struct firmware *firmware = NULL;
	struct syna_tcm_buffer *dest_buffer0 = NULL;
	struct syna_tcm_buffer *dest_buffer1 = NULL;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	switch(testcode) {
		case TEST_PT1_TRX_TRX_SHORTS:
			sprintf(test_name0, "%s", CSV_PT1_TESTING_LIMITS);
			dest_buffer0 = &testing_hcd->pt_hi_limits;
			dest_buffer1 = NULL;
			break;
		case TEST_PT3_TRX_GROUND_SHORTS:
			sprintf(test_name0, "%s", CSV_PT3_TESTING_LIMITS);
			dest_buffer0 = &testing_hcd->pt_hi_limits;
			dest_buffer1 = NULL;
			break;
		case TEST_PT5_FULL_RAW_CAP:
			sprintf(test_name0, "%s", CSV_PT5_TESTING_LIMITS_MIN);
			dest_buffer0 = &testing_hcd->pt_lo_limits;

			sprintf(test_name1, "%s", CSV_PT5_TESTING_LIMITS_MAX);
			dest_buffer1 = &testing_hcd->pt_hi_limits;
			break;
		case TEST_PT22_TRANS_RAW_CAP:
			if (gapdiff) {
				sprintf(test_name0, "%s", CSV_GAP_DIFF_TESTING_LIMITS_MAX);
				dest_buffer0 = &testing_hcd->pt_hi_limits;
			} else {
				sprintf(test_name0, "%s", CSV_PT22_TESTING_LIMITS_MIN);
				dest_buffer0 = &testing_hcd->pt_lo_limits;

				sprintf(test_name1, "%s", CSV_PT22_TESTING_LIMITS_MAX);
				dest_buffer1 = &testing_hcd->pt_hi_limits;
			}
			break;
		case TEST_PT10_DELTA_NOISE:
			sprintf(test_name0, "%s", CSV_PT10_TESTING_LIMITS);
			dest_buffer0 = &testing_hcd->pt_hi_limits;
			dest_buffer1 = NULL;
			break;
		case TEST_PT18_HYBRID_ABS_RAW:
			sprintf(test_name0, "%s", CSV_PT18_TESTING_LIMITS_MIN);
			dest_buffer0 = &testing_hcd->pt_lo_limits;

			sprintf(test_name1, "%s", CSV_PT18_TESTING_LIMITS_MAX);
			dest_buffer1 = &testing_hcd->pt_hi_limits;
			break;
		default:
			dest_buffer0 = NULL;
			dest_buffer1 = NULL;
			break;
	}

	if (!dest_buffer0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Limit data is not in csv file for [PT%d]\n", testcode);
		goto exit;
	}

	buf_size_bytes = testing_get_limits_bytes(&rows, &cols, testcode);
	if (buf_size_bytes == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get buf size for PT%d\n", testcode);
		retval = -EINVAL;
		goto exit;
	}

	/* read limit csv file */
	sprintf(limit_file_name, "%s", SYNA_TCM_TESTING_LIMITS_FILE_NAME);
	LOGN(tcm_hcd->pdev->dev.parent,
			"limit_file_name:%s.\n", limit_file_name);

	retval = request_firmware(&firmware,
				limit_file_name, tcm_hcd->pdev->dev.parent);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to request limit file:%s\n",
				limit_file_name);
		goto exit;
	}

	if (firmware->size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"request_firmware, limits file length error\n");
		retval = -EINVAL;
		goto exit;
	}

	csv_buf = kzalloc(firmware->size + 1, GFP_KERNEL);
	if (!csv_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for csv_buf\n");
		retval = -ENOMEM;
		goto exit;
	}
	memcpy(csv_buf, firmware->data, firmware->size);

	if (dest_buffer0) {
		/* allocate the mem for limits buffer */
		retval = syna_tcm_alloc_mem(tcm_hcd, dest_buffer0, buf_size_bytes);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for PT%d buf\n", testcode);
			goto exit;
		}

		/* load the limits value */
		retval = testing_parse_csv_data(dest_buffer0, csv_buf, test_name0, testcode);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read limts data for %s [PT%d] from csv file\n",test_name0, testcode);
			goto exit;
		}
	}

	if (dest_buffer1) {
		/* allocate the mem for limits buffer */
		retval = syna_tcm_alloc_mem(tcm_hcd, dest_buffer1, buf_size_bytes);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for PT%d buf\n", testcode);
			goto exit;
		}

		/* load the limits */
		retval = testing_parse_csv_data(dest_buffer1, csv_buf, test_name1, testcode);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read limts data for %s [PT%d] from csv file\n",test_name1, testcode);
			goto exit;
		}
	}

exit:
	if (csv_buf)
		kfree(csv_buf);

	if (firmware)
		release_firmware(firmware);

	return retval;
}

#ifdef CONFIG_FACTORY_BUILD
#if TESTING_RESULT_IN_CSV
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static ssize_t fs_write(const void *buf, size_t size, struct file *fp)
{
	ssize_t len = -1;
	loff_t pos;

	pos = fp->f_pos;
	len = __kernel_write(fp, buf, size, &pos);
	fp->f_pos = pos;

	return len;
}
#else
static ssize_t fs_write(const void *buf, size_t size, struct file *fp)
{
	mm_segment_t old_fs;
	loff_t pos;
	ssize_t len;

	pos = fp->f_pos;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	len = vfs_write(fp, buf, size, &pos);
	set_fs(old_fs);
	fp->f_pos = pos;

	return len;
}
#endif

static int testing_result_save_csv(char *buf, unsigned int size)
{
	int retval = 0;
	char save_path[100] = {0};
	struct file *fp = NULL;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	/* open save file */
	sprintf(save_path, "%s", SYNA_TCM_TESTING_RESULT_SAVE_PATH);
	LOGI(tcm_hcd->pdev->dev.parent,
			"save_path:%s. Save result data, starting...\n", save_path);
	fp = filp_open(save_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (IS_ERR(fp)) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"open file:%s failed. fp:%ld\n", save_path, PTR_ERR(fp));
			retval = -EIO;
			return retval;
	}

	if ((buf != NULL) && size > 0) {
		/* save data to csv file */
		if (fp != NULL) {
			retval = fs_write(buf, size, fp);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write data to file\n");
				goto exit;
			}
		}
	}
	LOGI(tcm_hcd->pdev->dev.parent,
			"save_path:%s. Save result data, completed\n", save_path);
exit:
	filp_close(fp, NULL);
	return retval;
}
#endif
#endif

static unsigned int testing_save_output(char *out_buf, unsigned int offset, char *pstr)
{
	unsigned int data_len;
	unsigned int cnt, sum;
	int data;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int idx;
	unsigned char *buf = NULL;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	sum = offset;
	if (out_buf == NULL) {
		LOGI(tcm_hcd->pdev->dev.parent, "Do not support save ouput data\n");
		goto exit;
	}

	app_info = &tcm_hcd->app_info;
	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	LOCK_BUFFER(testing_hcd->output);
	data_len = testing_hcd->output.data_length;
	if (data_len == 0)
		goto unlockbuffer;

	cnt = 0;
	if (pstr == NULL) {
		cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "PT%d Test Result = %s\n",
						testing_hcd->test_item, (testing_hcd->result)?"pass":"fail");
	} else {
		cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "%s Test Result = %s\n",
						pstr, (testing_hcd->result)?"pass":"fail");
	}
	sum += cnt;
	if (data_len == (rows*cols*2)) {
		cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "Rows=%d,Cols=%d\n", rows, cols);
		sum += cnt;
	}

	/* print data */
	buf = testing_hcd->output.buf;
	if (data_len == (rows*cols*2)) {
		idx = 0;
		for (row = 0; row < rows; row++) {
			cnt = 0;
			for (col = 0; col < cols; col++) {
				data = (short)((unsigned short)(buf[idx] & 0xff) |
						(unsigned short)(buf[idx+1] << 8));
				cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "%d,", data);
				sum += cnt;

				idx += 2;
			}

			cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "\n");
			sum += cnt;
		}
	} else if ((data_len == ((rows + cols)*4)) && (testing_hcd->test_item == TEST_PT18_HYBRID_ABS_RAW)){
		idx = 0;
		cnt = 0;

		for (col = 0; col < (cols + rows); col++) {
			data = (int)(buf[idx] & 0xff) |
					(int)(buf[idx+1] << 8) |
					(int)(buf[idx+2] << 16) |
					(int)(buf[idx+3] << 24);

			cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "%d,", data);
			sum += cnt;

			idx+=4;
		}

		cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "\n");
		sum += cnt;
	} else if (data_len <= ((rows + cols)*2)) {
		idx = 0;
		cnt = 0;

		for (col = 0; col < data_len; col++) {
			cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "0x%02X,", buf[idx]);
			sum += cnt;

			idx += 1;
		}

		cnt = snprintf(out_buf + sum, SAVE_BUF_SIZE - sum, "\n");
		sum += cnt;
	} else {
		LOGE(tcm_hcd->pdev->dev.parent, "Invalid data\n");
	}

unlockbuffer:
	UNLOCK_BUFFER(testing_hcd->output);
exit:
	return sum;
}

static int testing_run_prod_test_item(enum test_code test_code)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	if (tcm_hcd->features.dual_firmware &&
			tcm_hcd->id_info.mode != MODE_PRODUCTIONTEST_FIRMWARE) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_PRODUCTION_TEST);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to run production test firmware\n");
			return retval;
		}
	} else if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n",
				tcm_hcd->id_info.mode);
		return -ENODEV;
	}

	LOCK_BUFFER(testing_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->out,
			1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->out.buf\n");
		UNLOCK_BUFFER(testing_hcd->out);
		return retval;
	}

	testing_hcd->test_item = test_code;
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
		LOGE(tcm_hcd->pdev->dev.parent,
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

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	reinit_completion(&report_complete);
#else
	INIT_COMPLETION(report_complete);
#endif

	LOCK_BUFFER(testing_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->out,
			1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
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
		LOGE(tcm_hcd->pdev->dev.parent,
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
		LOGE(tcm_hcd->pdev->dev.parent,
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
		LOGE(tcm_hcd->pdev->dev.parent,
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

	return;
}

static void testing_copy_resp_to_output(void)
{
	int retval;
	unsigned int output_size;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	output_size = testing_hcd->resp.data_length;

	LOCK_BUFFER(testing_hcd->output);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			output_size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy test resp data\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	testing_hcd->output.data_length = output_size;

	UNLOCK_BUFFER(testing_hcd->output);

	return;
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
		LOGE(tcm_hcd->pdev->dev.parent,
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
		LOGE(tcm_hcd->pdev->dev.parent,
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
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size += testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_hcd->output.data_length = output_size;

	UNLOCK_BUFFER(testing_hcd->output);

	return;
}

static int testing_device_id(void)
{
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	struct syna_tcm_identification *id_info;
	char *strptr = NULL;
	int retval;

	mutex_lock(&tcm_hcd->extif_mutex);

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing\n");
	testing_hcd->result = false;

	retval = tcm_hcd->identify(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		/* 1-failed 2-passed */
		retval = 1;
		goto exit;
	}

	id_info = &tcm_hcd->id_info;

	strptr = strnstr(id_info->part_number,
					device_id_limit,
					sizeof(id_info->part_number));
	if (strptr != NULL)
		testing_hcd->result = true;
	else
		LOGE(tcm_hcd->pdev->dev.parent,
				"Device ID is mismatching, FW: %s (%s)\n",
				id_info->part_number, device_id_limit);

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");

	retval = (testing_hcd->result) ? 2 : 1; // 1-failed 2-passed

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);
	return retval;

}

static int testing_config_id(void)
{
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	struct syna_tcm_app_info *app_info;
	int i;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing\n");
	testing_hcd->result = false;

	app_info = &tcm_hcd->app_info;

	testing_hcd->result = true;
	for (i = 0; i < sizeof(config_id_limit); i++) {
		if (config_id_limit[i] !=
				tcm_hcd->app_info.customer_config_id[i]) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Config ID is mismatching at byte %-2d: limit=0x%02x, value=0x%02x\n",
					i, config_id_limit[i], tcm_hcd->app_info.customer_config_id[i]);
			testing_hcd->result = false;
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return 0;
}

static int testing_pt01_trx_trx_short(void)
{
	int retval;
	int i, j;
	int phy_pin;
	bool do_pin_test = false;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	unsigned int limits_size;
	unsigned int limits_row;
	unsigned int limits_col;
	unsigned char *buf;
	unsigned char pt1_limits;
	unsigned char data;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing:%s\n", STR(TEST_PT1_TRX_TRX_SHORTS));
	testing_hcd->result = false;

	app_info = &tcm_hcd->app_info;

	LOCK_BUFFER(testing_hcd->pt_hi_limits);
	limits_size = testing_get_limits_bytes(&limits_row, &limits_col, TEST_PT1_TRX_TRX_SHORTS);
	retval = testing_load_testlimits(TEST_PT1_TRX_TRX_SHORTS, 0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to load PT%d limits from csv file\n", TEST_PT1_TRX_TRX_SHORTS);
		retval = -EINVAL;
		goto exit;
	}

	if (limits_size != testing_hcd->pt_hi_limits.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent, "Mismatching limits size\n");
	}

	retval = testing_run_prod_test_item(TEST_PT1_TRX_TRX_SHORTS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		retval = -EIO;
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (limits_size < testing_hcd->resp.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (i = 0; i < testing_hcd->resp.data_length; i++) {
		data = buf[i];
		pt1_limits = testing_hcd->pt_hi_limits.buf[i];
		LOGI(tcm_hcd->pdev->dev.parent,
				"[%d]: 0x%02x, limit[0x%02x]\n",
				i, data, pt1_limits);
		for (j = 0; j < 8; j++) {
			phy_pin = (i*8 + j);

			do_pin_test = true;

			if (do_pin_test) {
				if (CHECK_BIT(data, j) != CHECK_BIT(pt1_limits, j)) {
					LOGE(tcm_hcd->pdev->dev.parent,
							"pin-%2d : fail\n", phy_pin);
					testing_hcd->result = false;
				}
				else
					LOGD(tcm_hcd->pdev->dev.parent,
							"pin-%2d : pass\n", phy_pin);
			}
		}
	}

	testing_copy_resp_to_output();

	UNLOCK_BUFFER(testing_hcd->resp);

exit:
	UNLOCK_BUFFER(testing_hcd->pt_hi_limits);

	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}


static int testing_pt03_trx_ground(void)
{
	int retval;
	int i, j;
	int phy_pin;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	unsigned int limits_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned char pt3_limits;
	unsigned char data;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing:%s\n", STR(TEST_PT3_TRX_GROUND_SHORTS));
	testing_hcd->result = false;

	LOCK_BUFFER(testing_hcd->pt_hi_limits);
	limits_size = testing_get_limits_bytes(&limits_rows, &limits_cols, TEST_PT3_TRX_GROUND_SHORTS);
	retval = testing_load_testlimits(TEST_PT3_TRX_GROUND_SHORTS, 0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to load PT%d limits from csv file\n", TEST_PT3_TRX_GROUND_SHORTS);
		retval = -EINVAL;
		goto exit;
	}

	if (limits_size != testing_hcd->pt_hi_limits.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent, "Mismatching limits size\n");
	}

	retval = testing_run_prod_test_item(TEST_PT3_TRX_GROUND_SHORTS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		retval = -EIO;
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (limits_size < testing_hcd->resp.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	testing_hcd->result = true;

	for (i = 0; i < testing_hcd->resp.data_length; i++) {

		data = testing_hcd->resp.buf[i];
		pt3_limits = testing_hcd->pt_hi_limits.buf[i];
		LOGI(tcm_hcd->pdev->dev.parent, "[%d]: 0x%02x, limit[0x%02x]\n",i , data, pt3_limits);

		for (j = 0; j < 8; j++) {

			phy_pin = (i*8 + j);

			if (CHECK_BIT(data, j) != CHECK_BIT(pt3_limits, j)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"pin-%2d : fail\n", phy_pin);
				testing_hcd->result = false;
			}
			else
				LOGD(tcm_hcd->pdev->dev.parent,
						"pin-%2d : pass\n", phy_pin);
		}
	}

	testing_copy_resp_to_output();

	UNLOCK_BUFFER(testing_hcd->resp);

exit:
	UNLOCK_BUFFER(testing_hcd->pt_hi_limits);

	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}

static int testing_pt05_full_raw(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	unsigned short data;
	unsigned short pt5_hi, pt5_lo;
	char *pt5_hi_limits;
	char *pt5_lo_limits;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing:%s\n",STR(TEST_PT5_FULL_RAW_CAP));
	testing_hcd->result = false;

	LOCK_BUFFER(testing_hcd->pt_hi_limits);
	LOCK_BUFFER(testing_hcd->pt_lo_limits);
	limits_size = testing_get_limits_bytes(&limits_rows, &limits_cols, TEST_PT5_FULL_RAW_CAP);
	retval = testing_load_testlimits(TEST_PT5_FULL_RAW_CAP, 0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to load PT%d limits from csv file\n", TEST_PT5_FULL_RAW_CAP);
		retval = -EINVAL;
		goto exit;
	}

	if ((limits_size != testing_hcd->pt_hi_limits.data_length) ||
		(limits_size != testing_hcd->pt_lo_limits.data_length)){
		LOGE(tcm_hcd->pdev->dev.parent, "Mismatching limits size\n");
	}

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	frame_size = rows * cols * 2;

	retval = testing_run_prod_test_item(TEST_PT5_FULL_RAW_CAP);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if ((limits_size != frame_size) &&
		(frame_size != testing_hcd->resp.data_length)) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	buf = testing_hcd->resp.buf;
	pt5_hi_limits = testing_hcd->pt_hi_limits.buf;
	pt5_lo_limits = testing_hcd->pt_lo_limits.buf;
	testing_hcd->result = true;

	idx = 0;
	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {

			data = (unsigned short)(buf[idx] & 0xff) |
					(unsigned short)(buf[idx+1] << 8);
			pt5_hi = (unsigned short)(pt5_hi_limits[idx] & 0xff) |
					(unsigned short)(pt5_hi_limits[idx+1] << 8);
			pt5_lo = (unsigned short)(pt5_lo_limits[idx] & 0xff) |
					(unsigned short)(pt5_lo_limits[idx+1] << 8);

			if (data > pt5_hi || data < pt5_lo) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"fail at (%2d, %2d) data = %5d, limit = [%4d, %4d]\n",
					row, col, data, pt5_lo, pt5_hi);

				testing_hcd->result = false;
			}

			idx += 2;
		}
	}

	testing_copy_resp_to_output();

	UNLOCK_BUFFER(testing_hcd->resp);

	retval = 0;

exit:
	UNLOCK_BUFFER(testing_hcd->pt_lo_limits);
	UNLOCK_BUFFER(testing_hcd->pt_hi_limits);

	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}


static int testing_pt07_dynamic_range(void)
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

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing\n");
	testing_hcd->result = false;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&frame_size_words, false);

	retval = testing_run_prod_test_item(TEST_PT7_DYNAMIC_RANGE);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows =
		sizeof(pt7_hi_limits) / sizeof(pt7_hi_limits[0]);
	limits_cols =
		sizeof(pt7_hi_limits[0]) / sizeof(pt7_hi_limits[0][0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows =
		sizeof(pt7_lo_limits) / sizeof(pt7_lo_limits[0]);
	limits_cols =
		sizeof(pt7_lo_limits[0]) / sizeof(pt7_lo_limits[0][0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
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
			if (data > pt7_hi_limits[row][col] ||
					data < pt7_lo_limits[row][col]) {

				LOGE(tcm_hcd->pdev->dev.parent,
					"fail at (%2d, %2d) data = %5d, limit = (%4d, %4d)\n",
					row, col, data, pt7_lo_limits[row][col],
					pt7_hi_limits[row][col]);

				testing_hcd->result = false;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(false);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}

static int testing_pt10_noise(void)
{
	int retval;
	short data;
	short pt10_limits;
	unsigned char *pt10_buf;
	unsigned char *buf;
	unsigned int idx;
	unsigned int limits_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int frame_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing:%s\n", STR(TEST_PT10_DELTA_NOISE));
	testing_hcd->result = false;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&frame_size_words, true);

	LOCK_BUFFER(testing_hcd->pt_hi_limits);

	limits_size = testing_get_limits_bytes(&limits_rows, &limits_cols, TEST_PT10_DELTA_NOISE);
	retval = testing_load_testlimits(TEST_PT10_DELTA_NOISE, 0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to load PT%d limits from csv file\n", TEST_PT10_DELTA_NOISE);
		retval = -EINVAL;
		goto exit;
	}

	if (limits_size != testing_hcd->pt_hi_limits.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent, "Mismatching limits size\n");
	}

	retval = testing_run_prod_test_item(TEST_PT10_DELTA_NOISE);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	pt10_buf = testing_hcd->pt_hi_limits.buf;
	pt10_limits = (unsigned short)(pt10_buf[0] & 0xff) |
					(unsigned short)(pt10_buf[1] << 8);
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data > pt10_limits) {

				LOGE(tcm_hcd->pdev->dev.parent,
					"fail at (%2d, %2d) data = %5d, limit = %4d\n",
					row, col, data, pt10_limits);

				testing_hcd->result = false;
			}
			idx++;
		}
	}

	testing_copy_resp_to_output();

	UNLOCK_BUFFER(testing_hcd->resp);

	retval = 0;

exit:
	UNLOCK_BUFFER(testing_hcd->pt_hi_limits);

	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}


static int testing_pt11_open_detection(void)
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

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing\n");
	testing_hcd->result = false;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&image_size_words, true);

	retval = testing_run_prod_test_item(TEST_PT11_OPEN_DETECTION);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (image_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Image size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows =
		sizeof(pt11_hi_limits) / sizeof(pt11_hi_limits[0]);
	limits_cols =
		sizeof(pt11_hi_limits[0]) / sizeof(pt11_hi_limits[0][0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows =
		sizeof(pt11_lo_limits) / sizeof(pt11_lo_limits[0]);
	limits_cols =
		sizeof(pt11_lo_limits[0]) / sizeof(pt11_lo_limits[0][0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
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

				LOGE(tcm_hcd->pdev->dev.parent,
					"fail at (%2d, %2d) data = %5d, limit = (%4d, %4d)\n",
					row, col, data, pt11_lo_limits[row][col],
					pt11_hi_limits[row][col]);

				testing_hcd->result = false;
			}
			idx++;
		}
	}

	testing_copy_resp_to_output();

	UNLOCK_BUFFER(testing_hcd->resp);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}

static int testing_pt18_hybrid_abs_raw(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	int data;
	int pt18_hi, pt18_lo;
	char *pt18_hi_limits;
	char *pt18_lo_limits;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing:%s\n", STR(TEST_PT18_HYBRID_ABS_RAW));
	testing_hcd->result = false;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	frame_size = (rows + cols) * 4;

	LOCK_BUFFER(testing_hcd->pt_hi_limits);
	LOCK_BUFFER(testing_hcd->pt_lo_limits);

	limits_size = testing_get_limits_bytes(&limits_rows, &limits_cols, TEST_PT18_HYBRID_ABS_RAW);
	retval = testing_load_testlimits(TEST_PT18_HYBRID_ABS_RAW, 0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to load PT%d limits from csv file\n", TEST_PT18_HYBRID_ABS_RAW);
		retval = -EINVAL;
		goto exit;
	}

	if ((limits_size != frame_size) ||
		(limits_size != testing_hcd->pt_hi_limits.data_length) ||
		(limits_size != testing_hcd->pt_lo_limits.data_length)){
		LOGE(tcm_hcd->pdev->dev.parent, "Mismatching limits size\n");
	}

	retval = testing_run_prod_test_item(TEST_PT18_HYBRID_ABS_RAW);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size != testing_hcd->resp.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size is mismatching\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	if ((rows +cols) != limits_cols || limits_rows != 1) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	pt18_hi_limits = testing_hcd->pt_hi_limits.buf;
	pt18_lo_limits = testing_hcd->pt_lo_limits.buf;
	testing_hcd->result = true;

	idx = 0;
	for (col = 0; col < limits_cols; col++) {
		data = (int)(buf[idx] & 0xff) |
				(int)(buf[idx+1] << 8) |
				(int)(buf[idx+2] << 16) |
				(int)(buf[idx+3] << 24);

		pt18_hi = (int)(pt18_hi_limits[idx] & 0xff) |
				(int)(pt18_hi_limits[idx+1] << 8) |
				(int)(pt18_hi_limits[idx+2] << 16) |
				(int)(pt18_hi_limits[idx+3] << 24);

		pt18_lo = (int)(pt18_lo_limits[idx] & 0xff) |
				(int)(pt18_lo_limits[idx+1] << 8) |
				(int)(pt18_lo_limits[idx+2] << 16) |
				(int)(pt18_lo_limits[idx+3] << 24);

		if ((data > pt18_hi) || (data < pt18_lo)) {
			testing_hcd->result = false;
			LOGE(tcm_hcd->pdev->dev.parent,
					"fail at index = %-2d. data = %d, limit = [%d, %d]\n",
					col, data, pt18_lo, pt18_hi);
		}
		idx+=4;
	}

	testing_copy_resp_to_output();

	UNLOCK_BUFFER(testing_hcd->resp);

	retval = 0;

exit:
	UNLOCK_BUFFER(testing_hcd->pt_lo_limits);
	UNLOCK_BUFFER(testing_hcd->pt_hi_limits);

	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}


static int testing_reset_open(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing\n");
	testing_hcd->result = false;

	if (bdata->reset_gpio < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Hardware reset unavailable\n");
		return -EINVAL;
	}

	mutex_lock(&tcm_hcd->reset_mutex);

#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, false);
#endif

	gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
	msleep(bdata->reset_active_ms);
	gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
	msleep(bdata->reset_delay_ms);

#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, true);
#endif

	mutex_unlock(&tcm_hcd->reset_mutex);

	if (tcm_hcd->id_info.mode == MODE_APPLICATION_FIRMWARE) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_BOOTLOADER);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enter bootloader mode\n");
			return retval;
		}
	} else {
		retval = tcm_hcd->identify(tcm_hcd, false);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
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
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware\n");
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}

static int testing_pt22_trans_raw_cap(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row, col;
	unsigned int rows, cols;
	unsigned int limits_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	unsigned short data, pt22_hi, pt22_lo;
	char *pt22_hi_limits;
	char *pt22_lo_limits;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing:%s\n",STR(TEST_PT22_TRANS_RAW_CAP));
	testing_hcd->result = false;

	LOCK_BUFFER(testing_hcd->pt_hi_limits);
	LOCK_BUFFER(testing_hcd->pt_lo_limits);

	retval = testing_load_testlimits(TEST_PT22_TRANS_RAW_CAP, 0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to load PT%d limits from csv file\n", TEST_PT22_TRANS_RAW_CAP);
		retval = -EINVAL;
		goto exit;
	}

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	frame_size = rows * cols * 2;

	limits_size = testing_get_limits_bytes(&limits_rows, &limits_cols, TEST_PT22_TRANS_RAW_CAP);
	retval = testing_run_prod_test_item(TEST_PT22_TRANS_RAW_CAP);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	if ((frame_size != limits_size) ||
		(limits_size != testing_hcd->pt_hi_limits.data_length) ||
		(limits_size != testing_hcd->pt_lo_limits.data_length)){
		LOGE(tcm_hcd->pdev->dev.parent, "Mismatching limits size\n");
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size != testing_hcd->resp.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	buf = testing_hcd->resp.buf;
	pt22_hi_limits = testing_hcd->pt_hi_limits.buf;
	pt22_lo_limits = testing_hcd->pt_lo_limits.buf;
	testing_hcd->result = true;

	/* check PT22 min/max limits */
	idx = 0;
	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {

			data = (unsigned short)(buf[idx] & 0xff) |
					(unsigned short)(buf[idx+1] << 8);
			pt22_hi = (unsigned short)(pt22_hi_limits[idx] & 0xff) |
					(unsigned short)(pt22_hi_limits[idx+1] << 8);
			pt22_lo = (unsigned short)(pt22_lo_limits[idx] & 0xff) |
					(unsigned short)(pt22_lo_limits[idx+1] << 8);

			if (data > pt22_hi || data < pt22_lo) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"fail at (%2d, %2d) data = %5d, limit = [%4d, %4d]\n",
					row, col, data, pt22_lo, pt22_hi);

				testing_hcd->result = false;
			}

			idx += 2;
		}
	}

	testing_copy_resp_to_output();

	UNLOCK_BUFFER(testing_hcd->resp);

	retval = 0;

exit:
	UNLOCK_BUFFER(testing_hcd->pt_lo_limits);
	UNLOCK_BUFFER(testing_hcd->pt_hi_limits);

	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}

static int testing_gap_diff(void)
{
	int retval;
	unsigned char *buf;
	unsigned int row, col;
	unsigned int rows, cols;
	unsigned int limits_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	short data, d0, d1, d2, d3, dmax, diff;
	short gap_diff_limits;
	char *gap_diff_limits_buf;
	char *gap_diff_data_buf = NULL;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start testing:GAP_DIFF_TEST based on PT%d output data\n", testing_hcd->test_item);
	testing_hcd->result = false;

	LOCK_BUFFER(testing_hcd->pt_hi_limits);

	retval = testing_load_testlimits(testing_hcd->test_item, 1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to load PT%d limits from csv file\n", testing_hcd->test_item);
		goto exit;
	}

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	frame_size = rows * cols * 2;

	limits_size = testing_get_limits_bytes(&limits_rows, &limits_cols, testing_hcd->test_item);
	/* data in output buffer, no need to read from FW */
	if ((limits_size != frame_size) ||
		(limits_size != testing_hcd->pt_hi_limits.data_length)){
		LOGE(tcm_hcd->pdev->dev.parent, "Mismatching limits size\n");
	}

	/* calc the gap diff based on the data in output */
	LOCK_BUFFER(testing_hcd->output);

	if (frame_size != testing_hcd->output.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->output);
		retval = -EINVAL;
		goto unlock_bufffer;
	}

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->output);
		retval = -EINVAL;
		goto unlock_bufffer;
	}

	/* alloc the buf to store the gap diff data */
	gap_diff_data_buf = kzalloc(frame_size, GFP_KERNEL);
	if (!gap_diff_data_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for gap_diff_data_buf\n");
		retval = -ENOMEM;
		goto unlock_bufffer;
	}

	buf = testing_hcd->output.buf;
	gap_diff_limits_buf = testing_hcd->pt_hi_limits.buf;
	testing_hcd->result = true;

	/* calc the gap diff */
	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			gap_diff_limits = le2_to_uint(&gap_diff_limits_buf[(row*cols + col)*2]);
			data = le2_to_uint(&buf[(row*cols + col)*2]);

			d0 = (row == 0) ? data : (le2_to_uint(&buf[((row-1)*cols + col)*2]));
			d1 = (row == (rows-1)) ? data : (le2_to_uint(&buf[((row+1)*cols + col)*2]));
			d2 = (col == 0) ? data : (le2_to_uint(&buf[(row*cols + (col-1))*2]));
			d3 = (col == (cols-1)) ? data : (le2_to_uint(&buf[(row*cols + (col+1))*2]));

			dmax = MAX(ABS(data-d0), ABS(data-d1));
			dmax = MAX(dmax, ABS(data-d2));
			dmax = MAX(dmax, ABS(data-d3));

			if (data == 0) {
				diff = 100;
			} else {
				diff = (unsigned short)((unsigned int)dmax*100/data);
			}
			if (diff > gap_diff_limits) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"fail at (%2d, %2d), diff_max = %4d, data = %4d, gap_diff = %4d, limit = %4d\n",
					row, col, dmax, data, diff, gap_diff_limits);

				testing_hcd->result = false;
			}

			/* backup the diff data */
			uint_to_le2(&gap_diff_data_buf[(row*cols + col)*2], diff);
		}
	}

	/* save the diff data back  to output buf */
	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			gap_diff_data_buf,
			frame_size,
			frame_size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy test resp data\n");
		goto unlock_bufffer;
	}
	testing_hcd->output.data_length = frame_size;

unlock_bufffer:
	UNLOCK_BUFFER(testing_hcd->output);

exit:
	UNLOCK_BUFFER(testing_hcd->pt_hi_limits);

	if (gap_diff_data_buf)
		kfree(gap_diff_data_buf);
	LOGN(tcm_hcd->pdev->dev.parent,
			"Result = %s\n", (testing_hcd->result) ? "pass" : "fail");

	return retval;
}

static int testing_self_test(char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	char result_info[RESULT_INFO_LEN] = {0};
	char *save_buf = NULL;
	unsigned int offset = 0;
	unsigned int cnt;
	unsigned int result_flag = 0;
	unsigned int gap_diff_test_result = 0;
	unsigned int mask;

	mask = (1<<TEST_PT1_TRX_TRX_SHORTS) |
			(1<<TEST_PT3_TRX_GROUND_SHORTS) |
			(1<<TEST_PT22_TRANS_RAW_CAP) |
			(1<<TEST_PT10_DELTA_NOISE) |
			(1<<TEST_PT18_HYBRID_ABS_RAW);

	testing_hcd->result = false;

	retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to enable interrupt\n");
		goto exit;
	}

#ifdef CONFIG_FACTORY_BUILD
#if TESTING_RESULT_IN_CSV
	save_buf = kzalloc(SAVE_BUF_SIZE, GFP_KERNEL);
	if (!save_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for printBuf\n");
		retval = -ENOMEM;
		goto exit;
	}
	offset = 0;
#endif
#endif

	LOGI(tcm_hcd->pdev->dev.parent, "\n");
	LOGI(tcm_hcd->pdev->dev.parent, "Start Panel testing\n");

	/* PT1 Test */
	retval = testing_pt01_trx_trx_short();
	if (retval < 0) {
		goto exit;
	}
	result_flag |= (testing_hcd->result) ? (1<<TEST_PT1_TRX_TRX_SHORTS) : 0;
	offset = testing_save_output(save_buf, offset, NULL);

	/* PT03 Test */
	retval = testing_pt03_trx_ground();
	if (retval < 0) {
		goto exit;
	}
	result_flag |= (testing_hcd->result) ? (1<<TEST_PT3_TRX_GROUND_SHORTS) : 0;
	offset = testing_save_output(save_buf, offset, NULL);

	/* PT22 Test */
	retval = testing_pt22_trans_raw_cap();
	if (retval < 0) {
		goto exit;
	}
	result_flag |= (testing_hcd->result) ? (1<<TEST_PT22_TRANS_RAW_CAP) : 0;
	offset = testing_save_output(save_buf, offset, NULL);

	retval = testing_gap_diff();
	if (retval < 0) {
		goto exit;
	}
	gap_diff_test_result = (testing_hcd->result) ? 1 : 0;
	offset = testing_save_output(save_buf, offset, "GAP_DIFF");

	/* PT10 Test */
	retval = testing_pt10_noise();
	if (retval < 0) {
		goto exit;
	}
	result_flag |= (testing_hcd->result) ? (1<<TEST_PT10_DELTA_NOISE) : 0;
	offset = testing_save_output(save_buf, offset, NULL);

	/* PT18 Test */
	retval = testing_pt18_hybrid_abs_raw();
	if (retval < 0) {
		goto exit;
	}
	result_flag |= (testing_hcd->result) ? (1<<TEST_PT18_HYBRID_ABS_RAW) : 0;
	offset = testing_save_output(save_buf, offset, NULL);

	/* print the panel result */
	testing_hcd->result = ((result_flag & mask) == mask) ? true : false;

	cnt = snprintf(result_info, RESULT_INFO_LEN,
					"Panel Test Result = %s: PT1 = %s  PT3 = %s  PT22 = %s  GAP_DIFF = %s  PT10 = %s  PT18 = %s\n",
					(testing_hcd->result) ? "pass" : "fail",
					(result_flag & (1<<TEST_PT1_TRX_TRX_SHORTS)) ? "pass":"fail",
					(result_flag & (1<<TEST_PT3_TRX_GROUND_SHORTS)) ? "pass":"fail",
					(result_flag & (1<<TEST_PT22_TRANS_RAW_CAP)) ? "pass":"fail",
					(gap_diff_test_result) ? "pass":"fail",
					(result_flag & (1<<TEST_PT10_DELTA_NOISE)) ? "pass":"fail",
					(result_flag & (1<<TEST_PT18_HYBRID_ABS_RAW)) ? "pass":"fail");

	LOGI(tcm_hcd->pdev->dev.parent, "%s\n", result_info);

#ifdef CONFIG_FACTORY_BUILD
#if TESTING_RESULT_IN_CSV
	cnt = snprintf(save_buf + offset, SAVE_BUF_SIZE - offset, "%s\n", result_info);
	offset += cnt;
	retval = testing_result_save_csv(save_buf, offset);
#endif
#endif

	if (buf != NULL)
		snprintf(buf, RESULT_INFO_LEN, "%s", result_info);

exit:
	LOGN(tcm_hcd->pdev->dev.parent,
		"Panel Test Result = %s\n", (testing_hcd->result)?"pass":"fail");
	return retval;
}

static ssize_t testing_sysfs_self_test_show(struct kobject *kobj,
					struct kobj_attribute *attributes, char *buf)
{
	int retval;
	char result_info[RESULT_INFO_LEN + 1] = {0};
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = testing_self_test(result_info);

	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to do self_test\n");
		goto exit;
	}

	retval = snprintf(buf, PAGE_SIZE, "%s\n",
				testing_hcd->result ? "Passed" : "Failed");

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int testing_xiaomi_self_test(char *buf)
{
	int retval;
	char result_info[RESULT_INFO_LEN + 1] = {0};
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to enable interrupt\n");
		goto exit;
	}

	retval = testing_self_test(result_info);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to do self_test, testing_hcd->result = %d\n",
			testing_hcd->result);
		goto exit;
	}

exit:
	if (testing_hcd->result)
		retval = true;
	else
		retval = false;
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int testing_print_report_data(enum report_type report_type, char *buf)
{
	int retval;
	int data;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int has_hybrid;
	unsigned int idx;
	unsigned int cnt, offset;
	unsigned char *data_buf = NULL;
	char *pfmt = NULL;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;
	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);
	has_hybrid = le2_to_uint(app_info->has_hybrid_data);

	/* print data */
	LOCK_BUFFER(testing_hcd->report);
	pfmt = (report_type == REPORT_RID161) ? (FORMAT_4U) : (FORMAT_4D);
	offset = 0;
	cnt = snprintf(buf + offset, PAGE_SIZE - offset, "RID%d: Rows=%d Cols=%d\n", report_type, rows, cols);
	offset += cnt;
	data_buf = testing_hcd->report.buf;

	idx = 0;
	for (row = 0; row < rows; row++) {
		cnt = 0;
		for (col = 0; col < cols; col++) {
			data = (int)((unsigned short)(data_buf[idx] & 0xff) |
					(unsigned short)(data_buf[idx+1] << 8));
			cnt = snprintf(buf + offset, PAGE_SIZE - offset, pfmt, data);
			offset += cnt;

			idx += 2;
		}

		cnt = snprintf(buf + offset, PAGE_SIZE - offset, "\n");
		offset += cnt;
	}

	if (has_hybrid) {
		pfmt = (report_type == REPORT_RID161) ? (FORMAT_5U) : (FORMAT_5D);
		/* print hybrid-x data */
		cnt = snprintf(buf + offset, PAGE_SIZE - offset, "Hybird-X:\n");
		offset += cnt;
		for (col =0; col < cols; col++) {
			data = (int)((unsigned short)(data_buf[idx] & 0xff) |
					(unsigned short)(data_buf[idx+1] << 8));
			cnt = snprintf(buf + offset, PAGE_SIZE - offset, pfmt, data);
			offset += cnt;

			idx += 2;
		}
		cnt = snprintf(buf + offset, PAGE_SIZE - offset, "\n");
		offset += cnt;

		/* print hybrid-y data */
		cnt = snprintf(buf + offset, PAGE_SIZE - offset, "Hybird-Y:\n");
		offset += cnt;
		for (row = 0; row < rows; row++) {
			data = (int)((unsigned short)(data_buf[idx] & 0xff) |
					(unsigned short)(data_buf[idx+1] << 8));
			cnt = snprintf(buf + offset, PAGE_SIZE - offset, pfmt, data);
			offset += cnt;

			idx += 2;
		}
		cnt = snprintf(buf + offset, PAGE_SIZE - offset, "\n");
		offset += cnt;
	}

	retval = offset;
	UNLOCK_BUFFER(testing_hcd->report);

	return retval;
}

static int testing_xiaomi_report_data(int report_type, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	LOGI(tcm_hcd->pdev->dev.parent, "collect RID %d start...\n", report_type);

	retval = testing_hcd->collect_reports(report_type, 0x01);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to read RID %d data\n", report_type);
		goto exit;
	}

	LOGI(tcm_hcd->pdev->dev.parent, "collect RID %d done\n", report_type);

	retval = testing_print_report_data(report_type, buf);

	LOGI(tcm_hcd->pdev->dev.parent,
		"retval=%d, PAGE_SIZE=%ld\n", retval, PAGE_SIZE);

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_rid18_data_show(struct kobject *kobj,
					struct kobj_attribute *attributes, char *buf)
{
	int retval;

	retval = testing_xiaomi_report_data(REPORT_DELTA, buf);

	return retval;
}

static ssize_t testing_sysfs_rid161_data_show(struct kobject *kobj,
					struct kobj_attribute *attributes, char *buf)
{
	int retval;

	retval = testing_xiaomi_report_data(REPORT_RID161, buf);

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
			LOGE(tcm_hcd->pdev->dev.parent,
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
			LOGE(tcm_hcd->pdev->dev.parent,
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

	return;
}


static int testing_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;

	testing_hcd = kzalloc(sizeof(*testing_hcd), GFP_KERNEL);
	if (!testing_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
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
	INIT_BUFFER(testing_hcd->pt_hi_limits, false);
	INIT_BUFFER(testing_hcd->pt_lo_limits, false);

	testing_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!testing_hcd->sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(testing_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	retval = sysfs_create_bin_file(testing_hcd->sysfs_dir, &bin_attr);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs bin file\n");
		goto err_sysfs_create_bin_file;
	}

	tcm_hcd->testing_xiaomi_report_data = testing_xiaomi_report_data;
	tcm_hcd->testing_xiaomi_self_test = testing_xiaomi_self_test;
	tcm_hcd->testing_xiaomi_chip_id_read = testing_device_id;
	return 0;

err_sysfs_create_bin_file:
err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(testing_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(testing_hcd->sysfs_dir);

err_sysfs_create_dir:
	RELEASE_BUFFER(testing_hcd->pt_lo_limits);
	RELEASE_BUFFER(testing_hcd->pt_hi_limits);
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

	RELEASE_BUFFER(testing_hcd->pt_lo_limits);
	RELEASE_BUFFER(testing_hcd->pt_hi_limits);
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

static int testing_reinit(struct syna_tcm_hcd *tcm_hcd)
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
#ifdef REPORT_NOTIFIER
	.asyncbox = NULL,
#endif
	.reinit = testing_reinit,
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

	return;
}

module_init(testing_module_init);
module_exit(testing_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Testing Module");
MODULE_LICENSE("GPL v2");
