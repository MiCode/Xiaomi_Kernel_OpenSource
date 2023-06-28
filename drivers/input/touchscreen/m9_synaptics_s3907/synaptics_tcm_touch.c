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
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS, " AND SYNAPTICS
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

#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include "synaptics_tcm_core.h"
#include <linux/time.h>
#define TYPE_B_PROTOCOL

//#define USE_DEFAULT_TOUCH_REPORT_CONFIG

#define TOUCH_REPORT_CONFIG_SIZE 128

/* debug info */
#define TOUCH_DEBUG_INFO_BYTES_MAX 8
#define TOUCH_DEBUG_INFO_SHORTS_MAX ((TOUCH_DEBUG_INFO_BYTES_MAX + 1)/2)
#define TOUCH_DEBUG_INFO_PRINT_SIZE 64
#define TOUCH_DEBUG_INFO_PRINT_MASK 0x8000

#define TOUCH_REPORT_CRC16_CHECK (1)

#define TOUCH_FOD_INVALID_ID (-1)

enum touch_status {
	LIFT = 0,
	FINGER = 1,
	GLOVED_FINGER = 2,
	PALM = 6,
	NOP = -1,
};

enum gesture_id {
	NO_GESTURE_DETECTED = 0,
	GESTURE_DOUBLE_TAP = 0x01,
	GESTURE_SINGLE_TAP = 0x10,
	GESTURE_TOUCH_AND_HOLD_DOWN_EVENT = 0x80,
	GESTURE_TOUCH_AND_HOLD_UP_EVENT   = 0x81,
	GESTURE_TOUCH_AND_HOLD_MOVE_EVENT = 0x83,
};

enum touch_report_code {
	TOUCH_END = 0,
	TOUCH_FOREACH_ACTIVE_OBJECT,
	TOUCH_FOREACH_OBJECT,
	TOUCH_FOREACH_END,
	TOUCH_PAD_TO_NEXT_BYTE,
	TOUCH_TIMESTAMP,
	TOUCH_OBJECT_N_INDEX,
	TOUCH_OBJECT_N_CLASSIFICATION,
	TOUCH_OBJECT_N_X_POSITION,
	TOUCH_OBJECT_N_Y_POSITION,
	TOUCH_OBJECT_N_Z,
	TOUCH_OBJECT_N_X_WIDTH,
	TOUCH_OBJECT_N_Y_WIDTH,
	TOUCH_OBJECT_N_TX_POSITION_TIXELS,
	TOUCH_OBJECT_N_RX_POSITION_TIXELS,
	TOUCH_0D_BUTTONS_STATE,
	TOUCH_GESTURE_ID,
	TOUCH_FRAME_RATE,
	TOUCH_POWER_IM,
	TOUCH_CID_IM,
	TOUCH_RAIL_IM,
	TOUCH_CID_VARIANCE_IM,
	TOUCH_NSM_FREQUENCY,
	TOUCH_NSM_STATE,
	TOUCH_NUM_OF_ACTIVE_OBJECTS,
	TOUCH_NUM_OF_CPU_CYCLES_USED_SINCE_LAST_FRAME,
	TOUCH_FACE_DETECT,
	TOUCH_GESTURE_DATA,
	TOUCH_OBJECT_N_FORCE,
	TOUCH_FINGERPRINT_AREA_MEET,
	TOUCH_TUNING_GAUSSIAN_WIDTHS = 0x80,
	TOUCH_TUNING_SMALL_OBJECT_PARAMS,
	TOUCH_TUNING_0D_BUTTONS_VARIANCE,
	TOUCH_REPORT_PALM_DETECTED = 200,
	TOUCH_REPORT_DEBUG_INFO = 0xCC,
	TOUCH_REPORT_CRC16 = 0xCD,
};

struct object_data {
	unsigned char status;
	unsigned int x_pos;
	unsigned int y_pos;
	unsigned int x_width;
	unsigned int y_width;
	unsigned int z;
	unsigned int tx_pos;
	unsigned int rx_pos;
};

struct input_params {
	unsigned int max_x;
	unsigned int max_y;
	unsigned int max_objects;
};

struct gesture_data {
	unsigned char x_pos[2];
	unsigned char y_pos[2];
	unsigned char area[2];
};

struct touch_data {
	struct object_data *object_data;
	unsigned int timestamp;
	unsigned int buttons_state;
	unsigned int gesture_id;
	struct gesture_data gesture_data;
	unsigned short debug_info_data[TOUCH_DEBUG_INFO_SHORTS_MAX];
	unsigned int frame_rate;
	unsigned int power_im;
	unsigned int cid_im;
	unsigned int rail_im;
	unsigned int cid_variance_im;
	unsigned int nsm_frequency;
	unsigned int nsm_state;
	unsigned int num_of_active_objects;
	unsigned int num_of_cpu_cycles;
	unsigned int fd_data;
	unsigned int force_data;
	unsigned int fingerprint_area_meet;
	unsigned int crc16_data;
};

struct touch_hcd {
	bool irq_wake;
	bool init_touch_ok;
	bool suspend_touch;
	unsigned char *prev_status;
	unsigned int max_x;
	unsigned int max_y;
	unsigned int max_objects;
	struct mutex report_mutex;
	struct input_dev *input_dev;
	struct touch_data touch_data;
	struct input_params input_params;
	struct syna_tcm_buffer out;
	struct syna_tcm_buffer resp;
	struct syna_tcm_hcd *tcm_hcd;
};

static struct touch_hcd *touch_hcd;
static unsigned int pre_overlap = 0;
static int fod_id = TOUCH_FOD_INVALID_ID;
static unsigned int  g_end_of_foreach = 0;

#define CENTER_X_ZIYI 5400
#define CENTER_Y_ZIYI 21470
void touch_fod_test(int value) {
	if (value) {
		input_report_key(touch_hcd->input_dev, BTN_INFO, 1);
		input_sync(touch_hcd->input_dev);
		update_fod_press_status(1);
		mi_disp_set_fod_queue_work(1, true);
		input_mt_slot(touch_hcd->input_dev, 0);
		input_mt_report_slot_state(touch_hcd->input_dev, MT_TOOL_FINGER, 1);
		input_report_key(touch_hcd->input_dev, BTN_TOUCH, 1);
		input_report_key(touch_hcd->input_dev, BTN_TOOL_FINGER, 1);
		input_report_abs(touch_hcd->input_dev, ABS_MT_TRACKING_ID, 0);
		input_report_abs(touch_hcd->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(touch_hcd->input_dev, ABS_MT_WIDTH_MINOR, 1);
		input_report_abs(touch_hcd->input_dev, ABS_MT_POSITION_X, CENTER_X_ZIYI);
		input_report_abs(touch_hcd->input_dev, ABS_MT_POSITION_Y, CENTER_Y_ZIYI);
		input_sync(touch_hcd->input_dev);
	} else {
		input_mt_slot(touch_hcd->input_dev, 0);
		input_report_abs(touch_hcd->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_report_abs(touch_hcd->input_dev, ABS_MT_WIDTH_MINOR, 0);
		input_mt_report_slot_state(touch_hcd->input_dev, MT_TOOL_FINGER, 0);
		input_report_abs(touch_hcd->input_dev, ABS_MT_TRACKING_ID, -1);
		input_report_key(touch_hcd->input_dev, BTN_INFO, 0);
		update_fod_press_status(0);
		mi_disp_set_fod_queue_work(0, true);
		input_sync(touch_hcd->input_dev);
	}
}

static void touch_fod_down_event(void)
{
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;

	/* Todo: add customer FOD action. */
	if (!tcm_hcd->fod_display_enabled) {
		input_report_key(touch_hcd->input_dev, BTN_INFO, 1);
		input_sync(touch_hcd->input_dev);
		LOGI(tcm_hcd->pdev->dev.parent, "FOD DOWN Dfetected\n");
		tcm_hcd->fod_display_enabled = true;
		update_fod_press_status(1);
		mi_disp_set_fod_queue_work(1, true);
	}
}

static void touch_fod_up_event(void)
{
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;

	/* Todo: add customer FOD action. */
	tcm_hcd->fod_finger = false;
	LOGI(tcm_hcd->pdev->dev.parent, "FOD UP Detected\n");
	input_report_key(touch_hcd->input_dev, BTN_INFO, 0);
	input_mt_slot(touch_hcd->input_dev, fod_id);
	input_report_abs(touch_hcd->input_dev, ABS_MT_WIDTH_MAJOR, 0);
	input_report_abs(touch_hcd->input_dev, ABS_MT_WIDTH_MINOR, 0);
	input_sync(touch_hcd->input_dev);
	tcm_hcd->fod_display_enabled = false;
	fod_id = TOUCH_FOD_INVALID_ID;
	update_fod_press_status(0);
	mi_disp_set_fod_queue_work(0, true);
}

int touch_flush_slots(struct syna_tcm_hcd *tcm_hcd)
{
	unsigned int idx;

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);

	if (touch_hcd->input_dev == NULL)
		return 0;

	mutex_lock(&touch_hcd->report_mutex);

	/* finger down */
	for (idx = 0; idx < touch_hcd->max_objects; idx++) {
#ifdef TYPE_B_PROTOCOL
		input_mt_slot(touch_hcd->input_dev, idx);
		input_mt_report_slot_state(touch_hcd->input_dev,
				MT_TOOL_FINGER, 1);
#endif
		input_report_key(touch_hcd->input_dev,
				BTN_TOUCH, 1);
		input_report_key(touch_hcd->input_dev,
				BTN_TOOL_FINGER, 1);

#ifndef TYPE_B_PROTOCOL
		input_mt_sync(touch_hcd->input_dev);
#endif
	}
	input_sync(touch_hcd->input_dev);

	mutex_unlock(&touch_hcd->report_mutex);

	/* finger up */
	touch_free_objects(tcm_hcd);

	LOGN(tcm_hcd->pdev->dev.parent, "-----exit-----%s\n", __func__);

	return 0;
}

/**
 * touch_free_objects() - Free all touch objects
 *
 * Report finger lift events to the input subsystem for all touch objects.
 */
int touch_free_objects(struct syna_tcm_hcd *tcm_hcd)
{
#ifdef TYPE_B_PROTOCOL
	unsigned int idx;
#endif

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);

	if (touch_hcd->input_dev == NULL)
		return 0;

	mutex_lock(&touch_hcd->report_mutex);

	/* clear FOD finger flag */
	if (tcm_hcd->fod_finger) {
		tcm_hcd->fod_finger = false;
		/*For FOD_STATUS_DELETED */
		LOGN(tcm_hcd->pdev->dev.parent, "touch free FOD event\n");
		touch_fod_up_event();
	}

#ifdef TYPE_B_PROTOCOL
	for (idx = 0; idx < touch_hcd->max_objects; idx++) {
		input_mt_slot(touch_hcd->input_dev, idx);
		input_mt_report_slot_state(touch_hcd->input_dev,
				MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(touch_hcd->input_dev,
			BTN_TOUCH, 0);
	input_report_key(touch_hcd->input_dev,
			BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
	input_mt_sync(touch_hcd->input_dev);
#endif
	input_sync(touch_hcd->input_dev);

	mutex_unlock(&touch_hcd->report_mutex);
	LOGN(tcm_hcd->pdev->dev.parent, "-----exit-----%s\n", __func__);

	return 0;
}


/**
 * touch_update_fod_enable_value() - update fod_enable value
 *
 * Update fod_enable value according finger_unlock_status.
 */
int touch_update_fod_enable_value(struct syna_tcm_hcd *tcm_hcd)
{
	LOGD(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);

	if (tcm_hcd->finger_unlock_status == FOD_STATUS_INVALID || tcm_hcd->finger_unlock_status == FOD_STATUS_DISABLED ||
			tcm_hcd->finger_unlock_status == FOD_STATUS_DELETED) {
		tcm_hcd->fod_enabled = false;
		LOGI(tcm_hcd->pdev->dev.parent, "set fod_enabled false\n");
	}

	LOGD(tcm_hcd->pdev->dev.parent, "-----exit-----%s\n", __func__);

	return 0;
}


/**
 * touch_get_report_data() - Retrieve data from touch report
 *
 * Retrieve data from the touch report based on the bit offset and bit length
 * information from the touch report configuration.
 */
static int touch_get_report_data(unsigned int offset,
		unsigned int bits, unsigned int *data)
{
	unsigned char mask;
	unsigned char byte_data;
	unsigned int output_data;
	unsigned int bit_offset;
	unsigned int byte_offset;
	unsigned int data_bits;
	unsigned int available_bits;
	unsigned int remaining_bits;
	unsigned char *touch_report;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;

	if (bits == 0 || bits > 32) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid number of bits\n");
		return -EINVAL;
	}

	if (offset + bits > tcm_hcd->report.buffer.data_length * 8) {
		*data = 0;
		return 0;
	}

	touch_report = tcm_hcd->report.buffer.buf;

	output_data = 0;
	remaining_bits = bits;

	bit_offset = offset % 8;
	byte_offset = offset / 8;

	while (remaining_bits) {
		byte_data = touch_report[byte_offset];
		byte_data >>= bit_offset;

		available_bits = 8 - bit_offset;
		data_bits = MIN(available_bits, remaining_bits);
		mask = 0xff >> (8 - data_bits);

		byte_data &= mask;

		output_data |= byte_data << (bits - remaining_bits);

		bit_offset = 0;
		byte_offset += 1;
		remaining_bits -= data_bits;
	}

	*data = output_data;

	return 0;
}

/**
 * touch_debug_info_print() - print the debug info
 */
static void touch_debug_info_print(unsigned int len)
{
	char print_buf[TOUCH_DEBUG_INFO_PRINT_SIZE] = {0};
	int i, cnt, offset;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;
	struct touch_data *touch_data = &touch_hcd->touch_data;;

	if (len > TOUCH_DEBUG_INFO_BYTES_MAX) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"bigger len [%d], expected no more than %d\n", len, TOUCH_DEBUG_INFO_BYTES_MAX);
	}

	offset = 0;
	for (i = 0; i < len/2; i++) {
			cnt = snprintf(print_buf + offset, TOUCH_DEBUG_INFO_PRINT_SIZE - offset, " %04x",
							touch_data->debug_info_data[i]);
			offset += cnt;

			if (offset >= TOUCH_DEBUG_INFO_PRINT_SIZE) {
					LOGE(tcm_hcd->pdev->dev.parent, "print buf is too small\n");
					break;
			}
	}
	LOGI(tcm_hcd->pdev->dev.parent, "DebugInfo:%s\n", print_buf);

	return;
}

#if 0
static void touch_config_print(void)
{
	unsigned char *config_data;
	unsigned int config_size;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;
	#define SYNA_PRINT_SIZE_MAX 1024
	char print_buf[SYNA_PRINT_SIZE_MAX] = {0};
	int i, cnt, offset, left;

	config_data = tcm_hcd->config.buf;
	config_size = tcm_hcd->config.data_length;

	offset = 0;
	for (i = 0; i < config_size; i++) {
		left = SYNA_PRINT_SIZE_MAX - offset;
		if (left <= 10) {
			//LOGI("There is unprinted data\n");
			break;
		}
		cnt = snprintf(print_buf + offset, 256 - offset, "%02x ", config_data[i]);
		offset += cnt;
	}
	cnt = snprintf(print_buf + offset, 256 - offset, "\n");
	offset += cnt;
	LOGI(tcm_hcd->pdev->dev.parent, "TouchCfg[%d]:%s", config_size, print_buf);

	return;
}
#endif

/**
 * touch_parse_report() - Parse touch report
 *
 * Traverse through the touch report configuration and parse the touch report
 * generated by the device accordingly to retrieve the touch data.
 */
static int touch_parse_report(void)
{
	int retval;
	bool active_only;
	bool num_of_active_objects;
	unsigned char code;
	unsigned int size;
	unsigned int idx;
	unsigned int obj;
	unsigned int next;
	unsigned int data;
	unsigned int bits, temp_bits;
	unsigned int offset;
	unsigned int objects;
	unsigned int active_objects;
	unsigned int report_size;
	unsigned int config_size;
	unsigned int left_bits;
	unsigned int i;
	unsigned char *config_data;
	unsigned char *temp_data;
	struct touch_data *touch_data;
	struct object_data *object_data;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;
	static unsigned int end_of_foreach;

	touch_data = &touch_hcd->touch_data;
	object_data = touch_hcd->touch_data.object_data;

	config_data = tcm_hcd->config.buf;
	config_size = tcm_hcd->config.data_length;
	// touch_config_print();

	report_size = tcm_hcd->report.buffer.data_length;

	size = sizeof(*object_data) * touch_hcd->max_objects;
	memset(touch_hcd->touch_data.object_data, 0x00, size);

	num_of_active_objects = false;

	idx = 0;
	offset = 0;
	objects = 0;
	active_objects = 0;
	active_only = false;

	while (idx < config_size) {
		code = config_data[idx++];
		switch (code) {
		case TOUCH_END:
			goto exit;
		case TOUCH_FOREACH_ACTIVE_OBJECT:
			obj = 0;
			next = idx;
			active_only = true;
			break;
		case TOUCH_FOREACH_OBJECT:
			obj = 0;
			next = idx;
			active_only = false;
			break;
		case TOUCH_FOREACH_END:
			end_of_foreach = idx;
			if (active_only) {
				if (num_of_active_objects) {
					objects++;
					if (objects < active_objects)
						idx = next;
				} else if (offset < report_size * 8) {
					idx = next;
				}
			} else {
				obj++;
				if (obj < touch_hcd->max_objects)
					idx = next;
			}
			break;
		case TOUCH_PAD_TO_NEXT_BYTE:
			offset = ceil_div(offset, 8) * 8;
			break;
		case TOUCH_TIMESTAMP:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get timestamp, retval=%d\n", retval);
				return retval;
			}
			touch_data->timestamp = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_INDEX:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &obj);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object index, retval=%d\n", retval);
				return retval;
			}
			offset += bits;
			break;
		case TOUCH_OBJECT_N_CLASSIFICATION:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object classification, retval=%d\n", retval);
				return retval;
			}
			object_data[obj].status = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_X_POSITION:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object x position, retval=%d\n", retval);
				return retval;
			}
			object_data[obj].x_pos = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_Y_POSITION:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object y position, retval=%d\n", retval);
				return retval;
			}
			object_data[obj].y_pos = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_Z:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object z, retval=%d\n", retval);
				return retval;
			}
			object_data[obj].z = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_X_WIDTH:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object x width, retval=%d\n", retval);
				return retval;
			}
			object_data[obj].x_width = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_Y_WIDTH:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object y width, retval=%d\n", retval);
				return retval;
			}
			object_data[obj].y_width = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_TX_POSITION_TIXELS:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object tx position, retval=%d\n", retval);
				return retval;
			}
			object_data[obj].tx_pos = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_RX_POSITION_TIXELS:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object rx position, retval=%d\n", retval);
				return retval;
			}
			object_data[obj].rx_pos = data;
			offset += bits;
			break;
		case TOUCH_OBJECT_N_FORCE:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object force, retval=%d\n", retval);
				return retval;
			}
			touch_data->force_data = data;
			offset += bits;
			break;
		case TOUCH_FINGERPRINT_AREA_MEET:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get object force, retval=%d\n", retval);
				return retval;
			}
			touch_data->fingerprint_area_meet = data;
			LOGN(tcm_hcd->pdev->dev.parent,
					"fingerprint_area_meet = %x\n",
					touch_data->fingerprint_area_meet);
			offset += bits;
			break;
		case TOUCH_0D_BUTTONS_STATE:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get 0D buttons state, retval=%d\n", retval);
				return retval;
			}
			touch_data->buttons_state = data;
			offset += bits;
			break;
		case TOUCH_GESTURE_ID:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get gesture double tap, retval=%d\n", retval);
				return retval;
			}
			touch_data->gesture_id = data;
			offset += bits;
			break;
		case TOUCH_FRAME_RATE:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get frame rate, retval=%d\n", retval);
				return retval;
			}
			touch_data->frame_rate = data;
			offset += bits;
			break;
		case TOUCH_POWER_IM:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get power IM, retval=%d\n", retval);
				return retval;
			}
			touch_data->power_im = data;
			offset += bits;
			break;
		case TOUCH_CID_IM:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get CID IM, retval=%d\n", retval);
				return retval;
			}
			touch_data->cid_im = data;
			offset += bits;
			break;
		case TOUCH_RAIL_IM:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get rail IM, retval=%d\n", retval);
				return retval;
			}
			touch_data->rail_im = data;
			offset += bits;
			break;
		case TOUCH_CID_VARIANCE_IM:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get CID variance IM, retval=%d\n", retval);
				return retval;
			}
			touch_data->cid_variance_im = data;
			offset += bits;
			break;
		case TOUCH_NSM_FREQUENCY:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get NSM frequency, retval=%d\n", retval);
				return retval;
			}
			touch_data->nsm_frequency = data;
			offset += bits;
			break;
		case TOUCH_NSM_STATE:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get NSM state, retval=%d\n", retval);
				return retval;
			}
			touch_data->nsm_state = data;
			offset += bits;
			break;
		case TOUCH_GESTURE_DATA:
			bits = config_data[idx++];
			if (bits != (sizeof(struct gesture_data)*8)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Incorrect Gesture Data length:%d\n", bits);
				return -EIO;
			}

			temp_data = (unsigned char *)&touch_data->gesture_data;
			left_bits = bits;
			i = 0;
			while (left_bits) {
				bits = (left_bits >= 8) ? 8 : left_bits;
				retval = touch_get_report_data(offset, bits, &data);
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
							"Failed to get number of active objects, retval=%d\n", retval);
					return retval;
				}
				temp_data[i++] = data&0xFF;
				left_bits -= bits;
				offset += bits;
			}
			break;
		case TOUCH_NUM_OF_ACTIVE_OBJECTS:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get number of active objects, retval=%d\n", retval);
				return retval;
			}
			active_objects = data;
			num_of_active_objects = true;
			touch_data->num_of_active_objects = data;
			offset += bits;
			if (touch_data->num_of_active_objects == 0) {
				if (0 == end_of_foreach) {
					LOGE(tcm_hcd->pdev->dev.parent,
						"Invalid report, num_active and end_foreach are 0\n");
					goto exit;
				}
				idx = end_of_foreach;
			}
			break;
		case TOUCH_NUM_OF_CPU_CYCLES_USED_SINCE_LAST_FRAME:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get num CPU cycles used since last frame, retval=%d\n", retval);
				return retval;
			}
			touch_data->num_of_cpu_cycles = data;
			offset += bits;
			break;
		case TOUCH_FACE_DETECT:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to detect face, retval=%d\n", retval);
				return retval;
			}
			touch_data->fd_data = data;
			offset += bits;
			break;
		case TOUCH_TUNING_GAUSSIAN_WIDTHS:
			bits = config_data[idx++];
			offset += bits;
			break;
		case TOUCH_TUNING_SMALL_OBJECT_PARAMS:
			bits = config_data[idx++];
			offset += bits;
			break;
		case TOUCH_TUNING_0D_BUTTONS_VARIANCE:
			bits = config_data[idx++];
			offset += bits;
			break;
		case TOUCH_REPORT_DEBUG_INFO:
			bits = config_data[idx++];
			temp_bits = bits;
			if (bits > (TOUCH_DEBUG_INFO_BYTES_MAX*8)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Incorrect Debug Info length:%d\n", bits);
				return -EIO;
			}

			memset(touch_data->debug_info_data, 0x00, sizeof(touch_data->debug_info_data));
			temp_data = (unsigned char *)&touch_data->debug_info_data[0];
			left_bits = bits;
			i = 0;
			while (left_bits) {
				bits = (left_bits >= 8) ? 8 : left_bits;
				retval = touch_get_report_data(offset, bits, &data);
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
							"Failed to get data of debug_info, retval=%d\n", retval);
					return retval;
				}
				temp_data[i++] = data&0xFF;
				left_bits -= bits;
				offset += bits;
			}
			/* print debug info */
			if (touch_data->debug_info_data[0]&TOUCH_DEBUG_INFO_PRINT_MASK) {
				touch_debug_info_print((temp_bits + 7)/8);
			}
			break;
#if TOUCH_REPORT_CRC16_CHECK
		case TOUCH_REPORT_CRC16:
			bits = config_data[idx++];
			retval = touch_get_report_data(offset, bits, &data);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to get debug info, retval=%d\n", retval);
				return retval;
			}

			touch_data->crc16_data = data;
//			LOGI(tcm_hcd->pdev->dev.parent, "Get CRC: 0x%04x\n", touch_data->crc16_data);
			offset += bits;
			break;
#endif
		default:
			bits = config_data[idx++];
			offset += bits;
			break;
		}
	}

exit:
	g_end_of_foreach = end_of_foreach;
	return 0;
}

#if TOUCH_REPORT_CRC16_CHECK
/**
 * touch_crc16()
 *
 * Calculate the crc-16 for TouchCom packet.
 *
 * @param
 *    [ in] p:   byte array for the calculation
 *    [ in] len: length in bytes
 *    [ in] val: the initial value given
 *
 * @return
 *    the crc-16 value
 */
static inline unsigned short touch_crc16(unsigned char *p,
	unsigned int len, unsigned short val)
{
	unsigned short r = val;
	static unsigned short crc16_table[256] = {
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
		0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
		0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
		0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
		0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
		0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
		0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
		0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
		0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
		0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
		0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
		0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
		0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
		0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
		0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
		0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
		0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
		0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
		0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
		0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
		0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
		0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
		0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
		0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
		0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
		0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
	};

	if (len == 0)
		return r;

	while (len--)
		r = (r << 8) ^ crc16_table[(r >> 8) ^ *p++];

	return r;
}

/**
 * touch_report_data_match() - calc the report data crc16
 */
static unsigned int touch_report_data_match(void)
{
	unsigned int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;
	struct touch_data *touch_data = &touch_hcd->touch_data;
	unsigned char *touch_report_data = tcm_hcd->report.buffer.buf;
	unsigned int toch_report_size = tcm_hcd->report.buffer.data_length;
	unsigned short crc16 = 0;
	unsigned short seed = 0x1234;

	if (toch_report_size > 2) {
		/* assert the last two bytes for crc16 value */
		crc16 = le2_to_uint(touch_report_data + (toch_report_size - 2));
		if (crc16 != touch_data->crc16_data) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"the crc is not at last two bytes (0x%04x, 0x%04x)\n",
				crc16, touch_data->crc16_data);
			goto exit;
		}

		crc16  = touch_crc16(touch_report_data, toch_report_size - 2, seed);
		if (crc16 == touch_data->crc16_data) {
			retval = 1;
		}
		LOGD(tcm_hcd->pdev->dev.parent,
			"crc value (0x%04x, 0x%04x)\n", crc16, touch_data->crc16_data);
	}
	else {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Invalid report len(%d)\n", toch_report_size);
	}

exit:
	if (retval == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Mismatch (0x%04x, 0x%04x)\n", crc16, touch_data->crc16_data);
	}

	return retval;
}
#endif

/**
 * touch_report() - Report touch events
 *
 * Retrieve data from the touch report generated by the device and report touch
 * events to the input subsystem.
 */

static void touch_report(void)
{
	int retval;
	unsigned int idx;
	struct timespec64 tv;
    struct rtc_time tm;
	unsigned long local_time;
	unsigned int x, fod_x;
	unsigned int y, fod_y;
	unsigned int fod_overlap;
	unsigned int temp;
	unsigned int status;
	unsigned int touch_count;
	struct touch_data *touch_data;
	struct object_data *object_data;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;
	static unsigned int prev_gesture_id = NO_GESTURE_DETECTED;
	unsigned int curr_gesture_id = NO_GESTURE_DETECTED;

	#define SYNA_OBJS_CNT_MAX 15
	static unsigned int prev_obj_present = 0;
	unsigned int curr_obj_present = 0;
	static unsigned int log_obj_x[SYNA_OBJS_CNT_MAX] = {0};
	static unsigned int log_obj_y[SYNA_OBJS_CNT_MAX] = {0};
	unsigned int i, xorVal;

	if (touch_hcd->max_objects > SYNA_OBJS_CNT_MAX) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"object cnt[%d] is larger than setting[%d]\n",
				touch_hcd->max_objects, SYNA_OBJS_CNT_MAX);
		return;
	}

	if (!touch_hcd->init_touch_ok)
		return;

	if (touch_hcd->input_dev == NULL)
		return;

	if (tcm_hcd->in_suspending)
		return;

	if (touch_hcd->suspend_touch)
		return;

	mutex_lock(&touch_hcd->report_mutex);

	retval = touch_parse_report();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to parse touch report, retval=%d\n", retval);
		goto exit;
	}

#if TOUCH_REPORT_CRC16_CHECK
	if (g_end_of_foreach) {
		temp = touch_report_data_match();
		if (!temp) {
			goto exit;
		}
	}
#endif

	touch_data = &touch_hcd->touch_data;
	object_data = touch_hcd->touch_data.object_data;

#if WAKEUP_GESTURE
	curr_gesture_id = touch_data->gesture_id;
	fod_x = le2_to_uint(touch_data->gesture_data.x_pos);
	fod_y = le2_to_uint(touch_data->gesture_data.y_pos);
	fod_overlap = touch_data->gesture_data.area[0];
	if (tcm_hcd->fod_enabled && (tcm_hcd->finger_unlock_status || tcm_hcd->fod_finger)) {
		if (touch_data->gesture_id == GESTURE_TOUCH_AND_HOLD_DOWN_EVENT || touch_data->gesture_id == GESTURE_TOUCH_AND_HOLD_MOVE_EVENT) {
			tcm_hcd->fod_finger = true;
			touch_fod_down_event();
			goto finger_pos;
		} else if ((touch_data->gesture_id == GESTURE_TOUCH_AND_HOLD_UP_EVENT) ||
				((prev_gesture_id == GESTURE_TOUCH_AND_HOLD_DOWN_EVENT || prev_gesture_id == GESTURE_TOUCH_AND_HOLD_MOVE_EVENT) &&
				(touch_data->gesture_id == NO_GESTURE_DETECTED))) {
			/* Todo: add customer FOD action. */
			if (tcm_hcd->fod_finger) {
				tcm_hcd->fod_finger = false;
				touch_fod_up_event();
				touch_update_fod_enable_value(tcm_hcd);
			}
			goto finger_pos;
		} 
	} else if (!tcm_hcd->fod_enabled && tcm_hcd->finger_unlock_status == 0 && tcm_hcd->fod_finger) {
		tcm_hcd->fod_finger = false;
		LOGI(tcm_hcd->pdev->dev.parent, "set fod up\n");
		touch_fod_up_event();
		touch_update_fod_enable_value(tcm_hcd);
	}

	if (tcm_hcd->in_suspend && tcm_hcd->wakeup_gesture_enabled) {
		if (touch_data->gesture_id == GESTURE_DOUBLE_TAP) {
			LOGI(tcm_hcd->pdev->dev.parent, "Double TAP Detected\n");
			input_report_key(touch_hcd->input_dev, KEY_WAKEUP, 1);
			input_sync(touch_hcd->input_dev);
			input_report_key(touch_hcd->input_dev, KEY_WAKEUP, 0);
			input_sync(touch_hcd->input_dev);
		} else if (touch_data->gesture_id == GESTURE_SINGLE_TAP) {
			LOGI(tcm_hcd->pdev->dev.parent, "Single TAP Detected\n");
			input_report_key(touch_hcd->input_dev, KEY_GOTO, 1);
			input_sync(touch_hcd->input_dev);
			input_report_key(touch_hcd->input_dev, KEY_GOTO, 0);
			input_sync(touch_hcd->input_dev);
		}
	}
#endif

	if (tcm_hcd->in_suspend)
		goto exit;

finger_pos:

	touch_count = 0;

	for (idx = 0; idx < touch_hcd->max_objects; idx++) {
		if (touch_hcd->prev_status[idx] == LIFT &&
				object_data[idx].status == LIFT)
			status = NOP;
		else
			status = object_data[idx].status;

		switch (status) {
		case LIFT:
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(touch_hcd->input_dev, idx);
			input_mt_report_slot_state(touch_hcd->input_dev,
					MT_TOOL_FINGER, 0);
			last_touch_events_collect(idx, 0);
#endif
			if (tcm_hcd->palm_sensor_enable && tcm_hcd->palm_enable_status) {
				tcm_hcd->palm_enable_status = 0;
				update_palm_sensor_value(tcm_hcd->palm_enable_status);
			}
			break;
		case PALM:
			/* Todo: add customer code for palm handle */
			if (tcm_hcd->palm_sensor_enable && !tcm_hcd->palm_enable_status) {
				LOGI(tcm_hcd->pdev->dev.parent,
					"Palm %d detected, palm_sensor_enable = %d\n",
					idx, tcm_hcd->palm_sensor_enable);
				tcm_hcd->palm_enable_status = 1;
				update_palm_sensor_value(tcm_hcd->palm_enable_status);
			}
			break;	
		case FINGER:
		case GLOVED_FINGER:
			x = object_data[idx].x_pos;
			y = object_data[idx].y_pos;
			if (bdata->swap_axes) {
				temp = x;
				x = y;
				y = temp;
			}
			if (bdata->x_flip)
				x = touch_hcd->input_params.max_x - x;
			if (bdata->y_flip)
				y = touch_hcd->input_params.max_y - y;
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(touch_hcd->input_dev, idx);
			input_mt_report_slot_state(touch_hcd->input_dev,
					MT_TOOL_FINGER, 1);
#endif
			input_report_key(touch_hcd->input_dev,
					BTN_TOUCH, 1);
			input_report_key(touch_hcd->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(touch_hcd->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(touch_hcd->input_dev,
					ABS_MT_POSITION_Y, y);

			if (tcm_hcd->fod_finger) {
				if ((x == fod_x) && (y == fod_y)) {
					fod_id = idx;
				}
			}

			if (fod_id == idx && tcm_hcd->fod_enabled &&
					tcm_hcd->fod_finger && tcm_hcd->finger_unlock_status) {
				if (pre_overlap == fod_overlap)
					fod_overlap++;

				input_report_abs(touch_hcd->input_dev,
					ABS_MT_TOUCH_MINOR, fod_overlap);
				input_report_abs(touch_hcd->input_dev,
					ABS_MT_TOUCH_MAJOR, touch_data->gesture_data.area[1]);
			}

#ifndef TYPE_B_PROTOCOL
			input_mt_sync(touch_hcd->input_dev);
#endif
			if ((touch_hcd->prev_status[idx] != FINGER) &&
				(touch_hcd->prev_status[idx] != GLOVED_FINGER)) {
				last_touch_events_collect(idx, 1);
			}
			log_obj_x[idx] = x;
			log_obj_y[idx] = y;
			touch_count++;
			break;
		default:
			break;
		}

		touch_hcd->prev_status[idx] = object_data[idx].status;
		if ((status == PALM) || (status == FINGER) || (status == GLOVED_FINGER)) {
			curr_obj_present |= 1<<idx;
		}
	}

	if (touch_count == 0) {
		input_report_key(touch_hcd->input_dev,
				BTN_TOUCH, 0);
		input_report_key(touch_hcd->input_dev,
				BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(touch_hcd->input_dev);
#endif
	}

	input_sync(touch_hcd->input_dev);
	// output land/lift info
	ktime_get_real_ts64(&tv);
	local_time = (u32)(tv.tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time64_to_tm(local_time, &tm);
	if (prev_obj_present != curr_obj_present) {
		xorVal = prev_obj_present^curr_obj_present;
		for(i = 0; i<touch_hcd->max_objects; i++) {
			if (xorVal&(1<<i)) {
				
			}
		}
		prev_obj_present = curr_obj_present;
	}
exit:
	prev_gesture_id = curr_gesture_id;
	if (tcm_hcd->fod_finger)
		pre_overlap = fod_overlap;
	mutex_unlock(&touch_hcd->report_mutex);

	return;
}

/**
 * touch_set_input_params() - Set input parameters
 *
 * Set the input parameters of the input device based on the information
 * retrieved from the application information packet. In addition, set up an
 * array for tracking the status of touch objects.
 */
static int touch_set_input_params(void)
{
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;

	input_set_abs_params(touch_hcd->input_dev,
			ABS_MT_POSITION_X, 0, touch_hcd->max_x, 0, 0);
	input_set_abs_params(touch_hcd->input_dev,
			ABS_MT_POSITION_Y, 0, touch_hcd->max_y, 0, 0);
	input_set_abs_params(touch_hcd->input_dev,
			ABS_MT_TOUCH_MINOR, 0, 100, 0, 0);
	input_set_abs_params(touch_hcd->input_dev,
			ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
	input_mt_init_slots(touch_hcd->input_dev, touch_hcd->max_objects,
			INPUT_MT_DIRECT);

	touch_hcd->input_params.max_x = touch_hcd->max_x;
	touch_hcd->input_params.max_y = touch_hcd->max_y;
	touch_hcd->input_params.max_objects = touch_hcd->max_objects;

	if (touch_hcd->max_objects == 0)
		return 0;

	kfree(touch_hcd->prev_status);
	touch_hcd->prev_status = kzalloc(touch_hcd->max_objects, GFP_KERNEL);
	if (!touch_hcd->prev_status) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for touch_hcd->prev_status\n");
		return -ENOMEM;
	}

	return 0;
}

/**
 * touch_get_input_params() - Get input parameters
 *
 * Retrieve the input parameters to register with the input subsystem for
 * the input device from the application information packet. In addition,
 * the touch report configuration is retrieved and stored.
 */
static int touch_get_input_params(void)
{
	int retval;
	unsigned int temp;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	app_info = &tcm_hcd->app_info;
	touch_hcd->max_x = le2_to_uint(app_info->max_x);
	touch_hcd->max_y = le2_to_uint(app_info->max_y);
	touch_hcd->max_objects = le2_to_uint(app_info->max_objects);

	if (bdata->swap_axes) {
		temp = touch_hcd->max_x;
		touch_hcd->max_x = touch_hcd->max_y;
		touch_hcd->max_y = temp;
	}

	LOCK_BUFFER(tcm_hcd->config);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_TOUCH_REPORT_CONFIG,
			NULL,
			0,
			&tcm_hcd->config.buf,
			&tcm_hcd->config.buf_size,
			&tcm_hcd->config.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s, retval=%d\n",
				STR(CMD_GET_TOUCH_REPORT_CONFIG), retval);
		UNLOCK_BUFFER(tcm_hcd->config);
		return retval;
	}

	UNLOCK_BUFFER(tcm_hcd->config);

	return 0;
}

/**
 * touch_set_input_dev() - Set up input device
 *
 * Allocate an input device, configure the input device based on the particular
 * input events to be reported, and register the input device with the input
 * subsystem.
 */
static int touch_set_input_dev(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;

	touch_hcd->input_dev = input_allocate_device();
	if (touch_hcd->input_dev == NULL) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate input device\n");
		return -ENODEV;
	}

	touch_hcd->input_dev->name = TOUCH_INPUT_NAME;
	touch_hcd->input_dev->phys = TOUCH_INPUT_PHYS_PATH;
	touch_hcd->input_dev->id.product = SYNAPTICS_TCM_ID_PRODUCT;
	touch_hcd->input_dev->id.version = SYNAPTICS_TCM_ID_VERSION;
	touch_hcd->input_dev->dev.parent = tcm_hcd->pdev->dev.parent;
	input_set_drvdata(touch_hcd->input_dev, tcm_hcd);

	set_bit(EV_SYN, touch_hcd->input_dev->evbit);
	set_bit(EV_KEY, touch_hcd->input_dev->evbit);
	set_bit(EV_ABS, touch_hcd->input_dev->evbit);
	set_bit(BTN_TOUCH, touch_hcd->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, touch_hcd->input_dev->keybit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, touch_hcd->input_dev->propbit);
#endif

#if WAKEUP_GESTURE
	set_bit(KEY_WAKEUP, touch_hcd->input_dev->keybit);
	set_bit(BTN_INFO, touch_hcd->input_dev->keybit);
	set_bit(KEY_GOTO, touch_hcd->input_dev->keybit);
	input_set_capability(touch_hcd->input_dev, EV_KEY, KEY_WAKEUP);
	input_set_capability(touch_hcd->input_dev, EV_KEY, BTN_INFO);
	input_set_capability(touch_hcd->input_dev, EV_KEY, KEY_GOTO);
#endif

	retval = touch_set_input_params();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set input parameters, retval=%d\n", retval);
		input_free_device(touch_hcd->input_dev);
		touch_hcd->input_dev = NULL;
		return retval;
	}

	retval = input_register_device(touch_hcd->input_dev);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to register input device, retval=%d\n", retval);
		input_free_device(touch_hcd->input_dev);
		touch_hcd->input_dev = NULL;
		return retval;
	}

	return 0;
}

/**
 * touch_set_report_config() - Set touch report configuration
 *
 * Send the SET_TOUCH_REPORT_CONFIG command to configure the format and content
 * of the touch report.
 */
static int touch_set_report_config(void)
{
	int retval;
	unsigned int idx;
	unsigned int length;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;

#ifdef USE_DEFAULT_TOUCH_REPORT_CONFIG
	return 0;
#endif

	app_info = &tcm_hcd->app_info;
	length = le2_to_uint(app_info->max_touch_report_config_size);

	if (length < TOUCH_REPORT_CONFIG_SIZE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid maximum touch report config size\n");
		return -EINVAL;
	}

	LOCK_BUFFER(touch_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&touch_hcd->out,
			length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for touch_hcd->out.buf, retval=%d\n", retval);
		UNLOCK_BUFFER(touch_hcd->out);
		return retval;
	}

	idx = 0;
#if WAKEUP_GESTURE
	touch_hcd->out.buf[idx++] = TOUCH_GESTURE_ID;
	touch_hcd->out.buf[idx++] = 8;
	touch_hcd->out.buf[idx++] = TOUCH_GESTURE_DATA;
	touch_hcd->out.buf[idx++] = 8*6;
#endif
	touch_hcd->out.buf[idx++] = TOUCH_REPORT_DEBUG_INFO;
	touch_hcd->out.buf[idx++] = 8*TOUCH_DEBUG_INFO_BYTES_MAX;
#if TOUCH_REPORT_CRC16_CHECK
	touch_hcd->out.buf[idx++] = TOUCH_NUM_OF_ACTIVE_OBJECTS;
	touch_hcd->out.buf[idx++] = 8;
	touch_hcd->out.buf[idx++] = TOUCH_PAD_TO_NEXT_BYTE,
#endif
	touch_hcd->out.buf[idx++] = TOUCH_FOREACH_ACTIVE_OBJECT;
	touch_hcd->out.buf[idx++] = TOUCH_OBJECT_N_INDEX;
	touch_hcd->out.buf[idx++] = 4;
	touch_hcd->out.buf[idx++] = TOUCH_OBJECT_N_CLASSIFICATION;
	touch_hcd->out.buf[idx++] = 4;
	touch_hcd->out.buf[idx++] = TOUCH_OBJECT_N_X_POSITION;
	touch_hcd->out.buf[idx++] = 16;
	touch_hcd->out.buf[idx++] = TOUCH_OBJECT_N_Y_POSITION;
	touch_hcd->out.buf[idx++] = 16;
	touch_hcd->out.buf[idx++] = TOUCH_FOREACH_END;
#if TOUCH_REPORT_CRC16_CHECK
	touch_hcd->out.buf[idx++] = TOUCH_PAD_TO_NEXT_BYTE;
	/* put the crc at the last two bytes */
	touch_hcd->out.buf[idx++] = TOUCH_REPORT_CRC16;
	touch_hcd->out.buf[idx++] = 8*2;
#endif
	touch_hcd->out.buf[idx++] = TOUCH_END;

	LOCK_BUFFER(touch_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_SET_TOUCH_REPORT_CONFIG,
			touch_hcd->out.buf,
			length,
			&touch_hcd->resp.buf,
			&touch_hcd->resp.buf_size,
			&touch_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s, retval=%d\n",
				STR(CMD_SET_TOUCH_REPORT_CONFIG), retval);
		UNLOCK_BUFFER(touch_hcd->resp);
		UNLOCK_BUFFER(touch_hcd->out);
		return retval;
	}

	UNLOCK_BUFFER(touch_hcd->resp);
	UNLOCK_BUFFER(touch_hcd->out);

	LOGN(tcm_hcd->pdev->dev.parent,
			"Set touch config done\n");

	return 0;
}

/**
 * touch_check_input_params() - Check input parameters
 *
 * Check if any of the input parameters registered with the input subsystem for
 * the input device has changed.
 */
static int touch_check_input_params(void)
{
	unsigned int size;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;

	if (touch_hcd->max_x == 0 && touch_hcd->max_y == 0)
		return 0;

	if (touch_hcd->input_params.max_objects != touch_hcd->max_objects) {
		kfree(touch_hcd->touch_data.object_data);
		size = sizeof(*touch_hcd->touch_data.object_data);
		size *= touch_hcd->max_objects;
		touch_hcd->touch_data.object_data = kzalloc(size, GFP_KERNEL);
		if (!touch_hcd->touch_data.object_data) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for touch_hcd->touch_data.object_data\n");
			return -ENOMEM;
		}
		return 1;
	}

	if (touch_hcd->input_params.max_x != touch_hcd->max_x)
		return 1;

	if (touch_hcd->input_params.max_y != touch_hcd->max_y)
		return 1;

	return 0;
}

/**
 * touch_set_input_reporting() - Configure touch report and set up new input
 * device if necessary
 *
 * After a device reset event, configure the touch report and set up a new input
 * device if any of the input parameters has changed after the device reset.
 */
static int touch_set_input_reporting(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = touch_hcd->tcm_hcd;

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n",
				tcm_hcd->id_info.mode);

		return 0;
	}

	touch_hcd->init_touch_ok = false;

	touch_free_objects(tcm_hcd);

	mutex_lock(&touch_hcd->report_mutex);

	retval = touch_set_report_config();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set report config, retval=%d\n", retval);
		goto exit;
	}

	retval = touch_get_input_params();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get input parameters, retval=%d\n", retval);
		goto exit;
	}

	retval = touch_check_input_params();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to check input parameters, retval=%d\n", retval);
		goto exit;
	} else if (retval == 0) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Input parameters unchanged\n");
		goto exit;
	}

	if (touch_hcd->input_dev != NULL) {
		input_unregister_device(touch_hcd->input_dev);
		touch_hcd->input_dev = NULL;
	}

	retval = touch_set_input_dev();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set up input device, retval=%d\n", retval);
		goto exit;
	}

exit:
	mutex_unlock(&touch_hcd->report_mutex);

	touch_hcd->init_touch_ok = retval < 0 ? false : true;

	return retval;
}


int touch_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	LOGE(tcm_hcd->pdev->dev.parent, "touch_init enter\n");
	touch_hcd = kzalloc(sizeof(*touch_hcd), GFP_KERNEL);
	if (!touch_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for touch_hcd\n");
		return -ENOMEM;
	}

	touch_hcd->tcm_hcd = tcm_hcd;

	mutex_init(&touch_hcd->report_mutex);

	INIT_BUFFER(touch_hcd->out, false);
	INIT_BUFFER(touch_hcd->resp, false);

	retval = touch_set_input_reporting();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set up input reporting, retval=%d\n", retval);
		goto err_set_input_reporting;
	}

	tcm_hcd->report_touch = touch_report;

	return 0;

err_set_input_reporting:
	kfree(touch_hcd->touch_data.object_data);
	kfree(touch_hcd->prev_status);

	RELEASE_BUFFER(touch_hcd->resp);
	RELEASE_BUFFER(touch_hcd->out);

	kfree(touch_hcd);
	touch_hcd = NULL;

	return retval;
}

int touch_remove(struct syna_tcm_hcd *tcm_hcd)
{
	if (!touch_hcd)
		goto exit;

	tcm_hcd->report_touch = NULL;

	if (touch_hcd->input_dev)
		input_unregister_device(touch_hcd->input_dev);

	kfree(touch_hcd->touch_data.object_data);
	kfree(touch_hcd->prev_status);

	RELEASE_BUFFER(touch_hcd->resp);
	RELEASE_BUFFER(touch_hcd->out);

	kfree(touch_hcd);
	touch_hcd = NULL;

exit:

	return 0;
}

int touch_reinit(struct syna_tcm_hcd *tcm_hcd)
{
	int retval = 0;

	if (!touch_hcd) {
		retval = touch_init(tcm_hcd);
		return retval;
	}

	touch_free_objects(tcm_hcd);

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode)) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Application mode is not running (firmware mode = %d)\n",
				tcm_hcd->id_info.mode);
		return 0;
	}

	retval = tcm_hcd->identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification, retval=%d\n", retval);
		return retval;
	}

	retval = touch_set_input_reporting();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set up input reporting, retval=%d\n", retval);
	}

	return retval;
}

int touch_early_suspend(struct syna_tcm_hcd *tcm_hcd)
{

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter---- %s\n", __func__);
	if (!touch_hcd)
		return 0;

	if (tcm_hcd->wakeup_gesture_enabled)
		touch_hcd->suspend_touch = false;
	else
		touch_hcd->suspend_touch = true;

	if (touch_hcd->suspend_touch)
		LOGE(tcm_hcd->pdev->dev.parent, "enter: touch_early_suspend---true---\n");

	touch_free_objects(tcm_hcd);
	LOGN(tcm_hcd->pdev->dev.parent, "-----exit---- %s\n", __func__);
	return 0;
}

int touch_suspend(struct syna_tcm_hcd *tcm_hcd)
{
	int retval = 0;

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter---- %s, wakeup_gesture[0x%02x]\n",
		__func__, ((tcm_hcd->wakeup_gesture_enabled) ? 1 : 0));

	if (!touch_hcd)
		return retval;

	touch_hcd->suspend_touch = true;

	if (tcm_hcd->wakeup_gesture_enabled) {
		if (!touch_hcd->irq_wake) {
			enable_irq_wake(tcm_hcd->irq);
			touch_hcd->irq_wake = true;
		}

		touch_hcd->suspend_touch = false;

		if (tcm_hcd->nonui_status != 1 && (tcm_hcd->fod_icon_status || tcm_hcd->aod_enable))
			tcm_hcd->gesture_type |= (0x0001<<13);
		else {
			/* disable single tap */
			tcm_hcd->gesture_type &= ~(0x0001<<13);
		}

		if (tcm_hcd->doubletap_enable)
			tcm_hcd->gesture_type |= 0x0001;
		else {
			/* disable double tap */
			tcm_hcd->gesture_type &= ~0x0001;
		}

		LOGI(tcm_hcd->pdev->dev.parent,
			"set DC_GESTURE_TYPE_ENABLE: 0x%02x\n", tcm_hcd->gesture_type);
		retval = tcm_hcd->set_dynamic_config(tcm_hcd,
				DC_GESTURE_TYPE_ENABLE,
				tcm_hcd->gesture_type);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable gesture type, retval=%d\n", retval);
			goto exit;
		}

		LOGN(tcm_hcd->pdev->dev.parent, "set DC_IN_WAKEUP_GESTURE_MODE\n");
		retval = tcm_hcd->set_dynamic_config(tcm_hcd,
				DC_IN_WAKEUP_GESTURE_MODE,
				1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable wakeup gesture mode, retval=%d\n", retval);
			goto exit;
		}
	}
exit:
	LOGN(tcm_hcd->pdev->dev.parent, "-----exit---- %s\n", __func__);
	return retval;
}

int touch_resume(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter---- %s\n", __func__);

	if (!touch_hcd)
		return 0;

	touch_hcd->suspend_touch = false;

	if (tcm_hcd->wakeup_gesture_enabled) {
		if (touch_hcd->irq_wake) {
			disable_irq_wake(tcm_hcd->irq);
			touch_hcd->irq_wake = false;
		}

		LOGN(tcm_hcd->pdev->dev.parent, "clear DC_IN_WAKEUP_GESTURE_MODE\n");
		retval = tcm_hcd->set_dynamic_config(tcm_hcd,
				DC_IN_WAKEUP_GESTURE_MODE,
				0);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to disable wakeup gesture mode, retval=%d\n", retval);
			return retval;
		}
	}
	LOGN(tcm_hcd->pdev->dev.parent, "-----exit---- %s\n", __func__);
	return 0;
}
