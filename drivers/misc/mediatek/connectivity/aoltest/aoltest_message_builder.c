// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/completion.h>
#include <linux/sched.h>
#include "aoltest_message_builder.h"
#include "aoltest_core.h"

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                             M A C R O S
********************************************************************************
*/
#define GPS_ATTR_NUM 8

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
void aoltest_push_message(struct aoltest_core_rb *rb, unsigned int type, unsigned int *buf);
int aoltest_pop_message(struct aoltest_core_rb *rb, char* buf);
static void aoltest_format_wifi_msg(char* buf, struct aoltest_wifi_raw_data* data);
static void aoltest_format_bt_msg(char* buf, struct aoltest_bt_raw_data* data);
static void aoltest_format_gps_msg(char* buf, struct aoltest_gps_raw_data* data);
static unsigned int aoltest_array_msg_builder(char* buf, uint8_t *arr, unsigned int arr_len,
											unsigned int prev_wsize, unsigned int* accum_size,
											char* format_regular, char* format_end,
											unsigned char format_type);


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
static unsigned int aoltest_array_msg_builder(char* buf, uint8_t *arr, unsigned int arr_len,
									unsigned int prev_wsize, unsigned int* accum_size,
									char* format_regular, char* format_end,
									unsigned char format_type)
{
	int wsize = prev_wsize;
	int j = 0;
	unsigned int incr_size = *accum_size;

	for (j = 0; j < arr_len; j++) {
		if (wsize >= 0 && wsize < (MAX_BUF_LEN - incr_size)) {
			incr_size += wsize;

			if (format_type == AOLTEST_MSG_FORMAT_AUCSSID) {
				// Check if it is a valid character before printing
				if ((arr[j] == 0) || (arr[j] < 0) || (arr[j] > 127)) {
					wsize = 0;
					continue;
				}
			}

			if (j == (arr_len - 1)) {
				// Use format_end
				wsize = snprintf(buf + incr_size, (MAX_BUF_LEN - incr_size),
							format_end, arr[j]);
				if (wsize < 0) {
					pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
				}

			} else {
				wsize = snprintf(buf + incr_size, (MAX_BUF_LEN - incr_size),
							format_regular, arr[j]);
				if (wsize < 0) {
					pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
				}
			}
		}
	}

	*accum_size = incr_size;
	return wsize;
}

static void aoltest_format_wifi_msg(char* buf, struct aoltest_wifi_raw_data* data)
{
	int wsize = 0;
	unsigned int accum_size = 0;
	unsigned int i = 0;
	struct timespec ts;
	struct tm tm;

	if (data == NULL) {
		return;
	}

	getnstimeofday(&ts);
	time_to_tm(ts.tv_sec, 0, &tm);

	wsize = snprintf(buf, MAX_BUF_LEN, "Result=%u;Time=%ld-%02d-%02d %02d:%02d:%02d.%09lu;size=%u;",
		data->result, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec, ts.tv_nsec, data->size);

	if (wsize < 0) {
		pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
	}

	for (i = 0; i < data->size; i++) {
		if (wsize >= 0 && wsize < (MAX_BUF_LEN - accum_size)) {
			accum_size += wsize;
			wsize = snprintf(buf + accum_size, (MAX_BUF_LEN - accum_size),
							"\nFrame=%u;ucRcpiValue=%u;aucBSSID[%u]=",
							i, data->frames[i].ucRcpiValue, MAC_ADDR_LEN);

			if (wsize < 0) {
				pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
			}
		}

		wsize = aoltest_array_msg_builder(buf, (data->frames[i]).aucBSSID,
												MAC_ADDR_LEN, wsize, &accum_size, "%x:", "%x;",
												AOLTEST_MSG_FORMAT_DEFAULT);

		if (wsize >= 0 && wsize < (MAX_BUF_LEN - accum_size)) {
			accum_size += wsize;
			wsize = snprintf(buf + accum_size, (MAX_BUF_LEN - accum_size),
							"ucChannel=%u;ucBand=%u;aucSsid[%u]=",
							data->frames[i].ucChannel, data->frames[i].ucBand,
							ELEM_MAX_LEN_SSID);

			if (wsize < 0) {
				pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
			}
		}

		wsize = aoltest_array_msg_builder(buf, (data->frames[i]).aucSsid,
										ELEM_MAX_LEN_SSID, wsize, &accum_size, "%c", "%c",
										AOLTEST_MSG_FORMAT_AUCSSID);
	}

	pr_info("WiFi formatted msg: [%s], wsize=[%d], accum=[%d]\n", buf, wsize, accum_size);
}

static void aoltest_format_bt_msg(char* buf, struct aoltest_bt_raw_data* data)
{
	int wsize = 0;
	unsigned int accum_size = 0;
	unsigned int i = 0;
	struct timespec ts;
	struct tm tm;

	if (data == NULL) {
		return;
	}

	getnstimeofday(&ts);
	time_to_tm(ts.tv_sec, 0, &tm);

	wsize = snprintf(buf, MAX_BUF_LEN, "Result=%u;Time=%ld-%02d-%02d %02d:%02d:%02d.%09lu;size=%u;",
		data->result, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec, ts.tv_nsec, data->size);

	if (wsize < 0) {
		pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
	}

	for (i = 0; i < data->size; i++) {
		if (wsize >= 0 && wsize < (MAX_BUF_LEN - accum_size)) {
			accum_size += wsize;
			wsize = snprintf(buf + accum_size, (MAX_BUF_LEN - accum_size),
							"\nEvent=%u;addr[%u]=",
							i, BT_MAX_ADDR);

			if (wsize < 0) {
				pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
			}
		}

		wsize = aoltest_array_msg_builder(buf, (data->events[i]).addr,
												BT_MAX_ADDR, wsize, &accum_size, "%x,", "%x;",
												AOLTEST_MSG_FORMAT_DEFAULT);

		if (wsize >= 0 && wsize < (MAX_BUF_LEN - accum_size)) {
			accum_size += wsize;
			wsize = snprintf(buf + accum_size, (MAX_BUF_LEN - accum_size),
							"rssi=%u;reserved=%u",
							data->events[i].rssi, data->events[i].reserved);

			if (wsize < 0) {
				pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
			}
		}
	}

	pr_info("BT formatted msg: [%s], wsize=[%d], accum=[%d]\n", buf, wsize, accum_size);
}

static void aoltest_format_double(double val, int* intPart, int* intFracPart) {
	double tmpVal = 0;
	double decimalPart = 0;

	tmpVal = (val < 0) ? -val : val;
	*intPart = (int)tmpVal;
	decimalPart = tmpVal - (*intPart);
	*intFracPart = (int)(decimalPart * 1000000); // six decimal place
}

static void aoltest_format_gps_msg(char* buf, struct aoltest_gps_raw_data* data)
{
	int i = 0;
	int wsize = 0;
	char* arr_name[GPS_ATTR_NUM] = {"lat", "lng", "alt", "bearing", "h_accuracy", "v_accuracy", "s_accuracy", "b_accuracy"};
	char* arr_sign[GPS_ATTR_NUM] = {" ", " ", " ", " ", " ", " ", " ", " "};
	int arr_int[GPS_ATTR_NUM];
	int arr_decimal[GPS_ATTR_NUM];
	double arr_val[GPS_ATTR_NUM];
	struct gnss_gfnc_location *location;
	struct timespec ts;
	struct tm tm;

	if (data == NULL) {
		return;
	}

	getnstimeofday(&ts);
	time_to_tm(ts.tv_sec, 0, &tm);

	// Result: GNSS_GFNC_TYPE_FIXED = 0; DENIED = 1; TIMEOUT = 2
	if ((data->result == 1) || (data->result == 2)) {
		wsize = snprintf(buf, MAX_BUF_LEN,
		"Result=%u;Time=%ld-%02d-%02d %02d:%02d:%02d.%09lu;Timestamp=%lld",
		data->result, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec, data->location.timestamp);
	} else {
		location = &data->location;

		arr_val[0] = (double)location->lat;
		arr_val[1] = (double)location->lng;
		arr_val[2] = (double)location->alt;
		arr_val[3] = (double)location->bearing;
		arr_val[4] = (double)location->h_accuracy;
		arr_val[5] = (double)location->v_accuracy;
		arr_val[6] = (double)location->s_accuracy;
		arr_val[7] = (double)location->b_accuracy;

		for (i = 0; i < GPS_ATTR_NUM; i++) {
			if (arr_val[i] < 0) {
				arr_sign[i] = "-";
			}

			aoltest_format_double(arr_val[i], &(arr_int[i]), &(arr_decimal[i]));
		}

		wsize = snprintf(buf, MAX_BUF_LEN,
			"Result=%u;Time=%ld-%02d-%02d %02d:%02d:%02d.%09lu;flags=%u;%s=%s%d.%d;%s=%s%d.%d;%s=%s%d.%d;%s=%s%d.%d;%s=%s%d.%d;%s=%s%d.%d;%s=%s%d.%d;%s=%s%d.%d;Timestamp=%lld",
			data->result, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec, data->location.flags,
			arr_name[0], arr_sign[0], arr_int[0], arr_decimal[0],
			arr_name[1], arr_sign[1], arr_int[1], arr_decimal[1],
			arr_name[2], arr_sign[2], arr_int[2], arr_decimal[2],
			arr_name[3], arr_sign[3], arr_int[3], arr_decimal[3],
			arr_name[4], arr_sign[4], arr_int[4], arr_decimal[4],
			arr_name[5], arr_sign[5], arr_int[5], arr_decimal[5],
			arr_name[6], arr_sign[6], arr_int[6], arr_decimal[6],
			arr_name[7], arr_sign[7], arr_int[7], arr_decimal[7],
			data->location.timestamp);
	}

	if (wsize < 0) {
		pr_info("[%s::%d] snprintf error\n", __func__, __LINE__);
	}

	pr_info("Gps formatted msg: [%s]\n", buf);
}

void aoltest_push_message(struct aoltest_core_rb *rb, unsigned int type, unsigned int *buf)
{
	unsigned long flags;
	struct aoltest_rb_data *rb_data = NULL;

	// Get free space from ring buffer
	spin_lock_irqsave(&(rb->lock), flags);
	rb_data = aoltest_core_rb_pop_free(rb);

	if (rb_data) {
		if (type == AOLTEST_MSG_ID_WIFI) {
			memcpy(&(rb_data->raw_data.wifi_raw), (struct aoltest_wifi_raw_data*)buf,
				sizeof(struct aoltest_wifi_raw_data));
		} else if (type == AOLTEST_MSG_ID_BT) {
			memcpy(&(rb_data->raw_data.bt_raw), (struct aoltest_bt_raw_data*)buf,
				sizeof(struct aoltest_bt_raw_data));
		} else if (type == AOLTEST_MSG_ID_GPS) {
			memcpy(&(rb_data->raw_data.gps_raw), (struct aoltest_gps_raw_data*)buf,
				sizeof(struct aoltest_gps_raw_data));
		}

		rb_data->type = type;
		aoltest_core_rb_push_active(rb, rb_data);
	} else {
		pr_info("[%s] rb is NULL", __func__);
	}

	spin_unlock_irqrestore(&(rb->lock), flags);
}

int aoltest_pop_message(struct aoltest_core_rb *rb, char* buf)
{
	unsigned long flags;
	struct aoltest_rb_data* rb_data = NULL;
	int type = -1;

	// Get data from ring buffer
	spin_lock_irqsave(&(rb->lock), flags);
	rb_data = aoltest_core_rb_pop_active(rb);

	// Format msg to string
	if (rb_data) {
		type = rb_data->type;
		if (type == AOLTEST_MSG_ID_WIFI) {
			aoltest_format_wifi_msg(buf, &(rb_data->raw_data.wifi_raw));
		} else if (type == AOLTEST_MSG_ID_BT) {
			aoltest_format_bt_msg(buf, &(rb_data->raw_data.bt_raw));
		} else if (type == AOLTEST_MSG_ID_GPS) {
			aoltest_format_gps_msg(buf, &(rb_data->raw_data.gps_raw));
		}

		// Free data
		aoltest_core_rb_push_free(rb, rb_data);
	}

	spin_unlock_irqrestore(&(rb->lock), flags);

	return type;
}
