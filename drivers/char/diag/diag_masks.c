/* Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/diagchar.h>
#include <linux/kmemleak.h>
#include <linux/workqueue.h>
#include "diagchar.h"
#include "diagfwd_cntl.h"
#include "diag_masks.h"

int diag_event_num_bytes;

#define DIAG_CTRL_MASK_INVALID		0
#define DIAG_CTRL_MASK_ALL_DISABLED	1
#define DIAG_CTRL_MASK_ALL_ENABLED	2
#define DIAG_CTRL_MASK_VALID		3

#define ALL_EQUIP_ID		100
#define ALL_SSID		-1

#define FEATURE_MASK_LEN_BYTES		2

struct mask_info {
	int equip_id;
	int num_items;
	int index;
};

#define CREATE_MSG_MASK_TBL_ROW(XX)					\
do {									\
	*(int *)(msg_mask_tbl_ptr) = MSG_SSID_ ## XX;			\
	msg_mask_tbl_ptr += 4;						\
	*(int *)(msg_mask_tbl_ptr) = MSG_SSID_ ## XX ## _LAST;		\
	msg_mask_tbl_ptr += 4;						\
	/* mimic the last entry as actual_last while creation */	\
	*(int *)(msg_mask_tbl_ptr) = MSG_SSID_ ## XX ## _LAST;		\
	msg_mask_tbl_ptr += 4;						\
	/* increment by MAX_SSID_PER_RANGE cells */			\
	msg_mask_tbl_ptr += MAX_SSID_PER_RANGE * sizeof(int);		\
} while (0)

static void diag_print_mask_table(void)
{
/* Enable this to print mask table when updated */
#ifdef MASK_DEBUG
	int first, last, actual_last;
	uint8_t *ptr = driver->msg_masks;
	int i = 0;
	pr_info("diag: F3 message mask table\n");
	while (*(uint32_t *)(ptr + 4)) {
		first = *(uint32_t *)ptr;
		ptr += 4;
		last = *(uint32_t *)ptr;
		ptr += 4;
		actual_last = *(uint32_t *)ptr;
		ptr += 4;
		pr_info("diag: SSID %d, %d - %d\n", first, last, actual_last);
		for (i = 0 ; i <= actual_last - first ; i++)
			pr_info("diag: MASK:%x\n", *((uint32_t *)ptr + i));
		ptr += MAX_SSID_PER_RANGE*4;
	}
#endif
}

void diag_create_msg_mask_table(void)
{
	uint8_t *msg_mask_tbl_ptr = driver->msg_masks;

	CREATE_MSG_MASK_TBL_ROW(0);
	CREATE_MSG_MASK_TBL_ROW(1);
	CREATE_MSG_MASK_TBL_ROW(2);
	CREATE_MSG_MASK_TBL_ROW(3);
	CREATE_MSG_MASK_TBL_ROW(4);
	CREATE_MSG_MASK_TBL_ROW(5);
	CREATE_MSG_MASK_TBL_ROW(6);
	CREATE_MSG_MASK_TBL_ROW(7);
	CREATE_MSG_MASK_TBL_ROW(8);
	CREATE_MSG_MASK_TBL_ROW(9);
	CREATE_MSG_MASK_TBL_ROW(10);
	CREATE_MSG_MASK_TBL_ROW(11);
	CREATE_MSG_MASK_TBL_ROW(12);
	CREATE_MSG_MASK_TBL_ROW(13);
	CREATE_MSG_MASK_TBL_ROW(14);
	CREATE_MSG_MASK_TBL_ROW(15);
	CREATE_MSG_MASK_TBL_ROW(16);
	CREATE_MSG_MASK_TBL_ROW(17);
	CREATE_MSG_MASK_TBL_ROW(18);
	CREATE_MSG_MASK_TBL_ROW(19);
	CREATE_MSG_MASK_TBL_ROW(20);
	CREATE_MSG_MASK_TBL_ROW(21);
	CREATE_MSG_MASK_TBL_ROW(22);
	CREATE_MSG_MASK_TBL_ROW(23);
}

static void diag_set_msg_mask(int rt_mask)
{
	int first_ssid, last_ssid, i;
	uint8_t *parse_ptr, *ptr = driver->msg_masks;

	mutex_lock(&driver->diagchar_mutex);
	driver->msg_status = rt_mask ? DIAG_CTRL_MASK_ALL_ENABLED :
						DIAG_CTRL_MASK_ALL_DISABLED;
	while (*(uint32_t *)(ptr + 4)) {
		first_ssid = *(uint32_t *)ptr;
		ptr += 8; /* increment by 8 to skip 'last' */
		last_ssid = *(uint32_t *)ptr;
		ptr += 4;
		parse_ptr = ptr;
		pr_debug("diag: updating range %d %d\n", first_ssid, last_ssid);
		for (i = 0; i < last_ssid - first_ssid + 1; i++) {
			*(int *)parse_ptr = rt_mask;
			parse_ptr += 4;
		}
		ptr += MAX_SSID_PER_RANGE * 4;
	}
	mutex_unlock(&driver->diagchar_mutex);
}

static void diag_update_msg_mask(int start, int end , uint8_t *buf)
{
	int found = 0, first, last, actual_last;
	uint8_t *actual_last_ptr;
	uint8_t *ptr = driver->msg_masks;
	uint8_t *ptr_buffer_start = &(*(driver->msg_masks));
	uint8_t *ptr_buffer_end = &(*(driver->msg_masks)) + MSG_MASK_SIZE;
	uint32_t copy_len = (end - start + 1) * sizeof(int);

	mutex_lock(&driver->diagchar_mutex);
	/* First SSID can be zero : So check that last is non-zero */
	while (*(uint32_t *)(ptr + 4)) {
		first = *(uint32_t *)ptr;
		ptr += 4;
		last = *(uint32_t *)ptr;
		ptr += 4;
		actual_last = *(uint32_t *)ptr;
		actual_last_ptr = ptr;
		ptr += 4;
		if (start >= first && start <= actual_last) {
			ptr += (start - first)*4;
			if (end > actual_last) {
				pr_info("diag: ssid range mismatch\n");
				actual_last = end;
				*(uint32_t *)(actual_last_ptr) = end;
			}
			if (actual_last-first >= MAX_SSID_PER_RANGE) {
				pr_err("diag: In %s, truncating ssid range, %d-%d to max allowed: %d",
						__func__, first, actual_last,
						MAX_SSID_PER_RANGE);
				copy_len = MAX_SSID_PER_RANGE;
				actual_last = first + MAX_SSID_PER_RANGE;
				*(uint32_t *)actual_last_ptr = actual_last;
			}
			if (CHK_OVERFLOW(ptr_buffer_start, ptr, ptr_buffer_end,
								copy_len)) {
				pr_debug("diag: update ssid start %d, end %d\n",
								 start, end);
				memcpy(ptr, buf, copy_len);
			} else
				pr_alert("diag: Not enough space MSG_MASK\n");
			found = 1;
			break;
		} else {
			ptr += MAX_SSID_PER_RANGE*4;
		}
	}
	/* Entry was not found - add new table */
	if (!found) {
		if (CHK_OVERFLOW(ptr_buffer_start, ptr, ptr_buffer_end,
				  8 + ((end - start) + 1)*4)) {
			memcpy(ptr, &(start) , 4);
			ptr += 4;
			memcpy(ptr, &(end), 4);
			ptr += 4;
			memcpy(ptr, &(end), 4); /* create actual_last entry */
			ptr += 4;
			pr_debug("diag: adding NEW ssid start %d, end %d\n",
								 start, end);
			memcpy(ptr, buf , ((end - start) + 1)*4);
		} else
			pr_alert("diag: Not enough buffer space for MSG_MASK\n");
	}
	driver->msg_status = DIAG_CTRL_MASK_VALID;
	mutex_unlock(&driver->diagchar_mutex);
	diag_print_mask_table();
}

void diag_toggle_event_mask(int toggle)
{
	uint8_t *ptr = driver->event_masks;

	mutex_lock(&driver->diagchar_mutex);
	if (toggle) {
		driver->event_status = DIAG_CTRL_MASK_ALL_ENABLED;
		memset(ptr, 0xFF, EVENT_MASK_SIZE);
	} else {
		driver->event_status = DIAG_CTRL_MASK_ALL_DISABLED;
		memset(ptr, 0, EVENT_MASK_SIZE);
	}
	mutex_unlock(&driver->diagchar_mutex);
}


static void diag_update_event_mask(uint8_t *buf, int num_bytes)
{
	uint8_t *ptr = driver->event_masks;
	uint8_t *temp = buf + 2;

	mutex_lock(&driver->diagchar_mutex);
	if (CHK_OVERFLOW(ptr, ptr, ptr+EVENT_MASK_SIZE, num_bytes)) {
		memcpy(ptr, temp, num_bytes);
		driver->event_status = DIAG_CTRL_MASK_VALID;
	} else {
		pr_err("diag: In %s, not enough buffer space\n", __func__);
	}
	mutex_unlock(&driver->diagchar_mutex);
}

static void diag_disable_log_mask(void)
{
	int i = 0;
	struct diag_log_mask_t *log_item = NULL;

	mutex_lock(&driver->log_mask_mutex);
	log_item = (struct diag_log_mask_t *)driver->log_masks;
	for (i = 0; i < MAX_EQUIP_ID; i++, log_item++)
		memset(log_item->ptr, 0, MAX_ITEMS_PER_EQUIP_ID);

	driver->log_status = DIAG_CTRL_MASK_ALL_DISABLED;
	mutex_unlock(&driver->log_mask_mutex);
}

static int copy_log_mask_equip(int equip_id, uint8_t *buf)
{
	int i, ret = 0;
	uint8_t *temp = buf;
	struct diag_log_mask_t *log_item = NULL;
	uint32_t mask_size = 0;

	if (!buf)
		return ret;

	log_item = (struct diag_log_mask_t *)driver->log_masks;
	for (i = 0; i < MAX_EQUIP_ID; i++, log_item++) {
		if (log_item->equip_id != equip_id)
			continue;
		*(int *)temp = log_item->equip_id;
		temp += sizeof(int);
		*(int *)(temp) = log_item->num_items;
		temp += sizeof(int);
		mask_size = LOG_ITEMS_TO_SIZE(log_item->num_items);
		if (mask_size > MAX_ITEMS_PER_EQUIP_ID) {
			pr_err("diag: Invalid length: %d in %s, perimissible: %d",
				mask_size, __func__, MAX_ITEMS_PER_EQUIP_ID);
			break;
		}
		if (mask_size > 0) {
			memcpy(temp, log_item->ptr, mask_size);
			/*
			 * Return the total number of bytes copied = size of
			 * equip_id (int) + size of num_items (int) + mask_size
			 */
			ret = (2 * sizeof(int)) + mask_size;
		}
		break;
	}

	return ret;
}

static void diag_update_log_mask(int equip_id, uint8_t *buf, int num_items)
{
	int i = 0;
	struct diag_log_mask_t *log_item = NULL;
	uint32_t mask_size = 0;

	mutex_lock(&driver->log_mask_mutex);
	driver->log_status = DIAG_CTRL_MASK_INVALID;
	if (!buf || (equip_id < 0 || equip_id >= MAX_EQUIP_ID) ||
							num_items < 1) {
		pr_err("diag: Invalid params in %s, buf: %x equip_id: %d, num_items: %d\n",
		       __func__, (unsigned int)buf, equip_id, num_items);
		mutex_unlock(&driver->log_mask_mutex);
		return;
	}
	mask_size = LOG_ITEMS_TO_SIZE(num_items);
	if (mask_size > MAX_ITEMS_PER_EQUIP_ID) {
		pr_err("diag: In %s, Invalid mask_size %d\n", __func__,
								mask_size);
		mutex_unlock(&driver->log_mask_mutex);
		return;
	}

	log_item = (struct diag_log_mask_t *)driver->log_masks;
	for (i = 0; i < MAX_EQUIP_ID; i++, log_item++) {
		if (log_item->equip_id != equip_id)
			continue;
		/* Found the equip id */
		log_item->num_items = num_items;
		if (mask_size > 0)
			memcpy(log_item->ptr, buf, mask_size);
		driver->log_status = DIAG_CTRL_MASK_VALID;
		break;
	}
	mutex_unlock(&driver->log_mask_mutex);
}

void diag_mask_update_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						struct diag_smd_info,
						diag_notify_update_smd_work);
	if (!smd_info) {
		pr_err("diag: In %s, smd info is null, cannot update masks for the peripheral\n",
			__func__);
		return;
	}

	diag_send_feature_mask_update(smd_info);
	diag_send_msg_mask_update(smd_info, ALL_SSID, ALL_SSID,
						smd_info->peripheral);
	diag_send_log_mask_update(smd_info, ALL_EQUIP_ID);
	diag_send_event_mask_update(smd_info, diag_event_num_bytes);

	if (smd_info->notify_context == SMD_EVENT_OPEN)
		diag_send_diag_mode_update_by_smd(smd_info,
						driver->real_time_mode);

	smd_info->notify_context = 0;
}

void diag_send_log_mask_update(struct diag_smd_info *smd_info, int equip_id)
{
	void *buf = driver->buf_log_mask_update;
	struct diag_log_mask_t *log_item = NULL;
	struct diag_ctrl_log_mask ctrl_pkt;
	uint32_t log_mask_size = 0;
	int wr_size = -ENOMEM, retry_count = 0;
	int i, header_size, send_once = 0;

	if (!smd_info) {
		pr_err("diag: In %s, null smd info pointer\n",
			__func__);
		return;
	}

	header_size = sizeof(struct diag_ctrl_log_mask);
	log_item = (struct diag_log_mask_t *)driver->log_masks;
	mutex_lock(&driver->diag_cntl_mutex);
	for (i = 0; i < MAX_EQUIP_ID; i++, log_item++) {
		if (equip_id != i && equip_id != ALL_EQUIP_ID)
			continue;
		log_mask_size = LOG_ITEMS_TO_SIZE(log_item->num_items);
		ctrl_pkt.cmd_type = DIAG_CTRL_MSG_LOG_MASK;
		ctrl_pkt.data_len = 11 + log_mask_size;
		ctrl_pkt.stream_id = 1;
		ctrl_pkt.status = driver->log_status;
		switch (driver->log_status) {
		case DIAG_CTRL_MASK_ALL_DISABLED:
			ctrl_pkt.equip_id = 0;
			ctrl_pkt.num_items = 0;
			ctrl_pkt.log_mask_size = 0;
			send_once = 1;
			break;
		case DIAG_CTRL_MASK_ALL_ENABLED:
			ctrl_pkt.equip_id = 0;
			ctrl_pkt.num_items = 0;
			ctrl_pkt.log_mask_size = 0;
			send_once = 1;
			break;
		case DIAG_CTRL_MASK_VALID:
			ctrl_pkt.equip_id = i;
			ctrl_pkt.num_items = log_item->num_items;
			ctrl_pkt.log_mask_size = log_mask_size;
			send_once = 0;
			break;
		default:
			pr_err("diag: In %s, invalid status %d", __func__,
				 driver->log_status);
			mutex_unlock(&driver->diag_cntl_mutex);
			return;
		}
		memcpy(buf, &ctrl_pkt, header_size);
		if (log_mask_size > 0) {
			memcpy(buf + header_size, log_item->ptr,
			       log_mask_size);
		}

		if (smd_info->ch) {
			while (retry_count < 3) {
				mutex_lock(&smd_info->smd_ch_mutex);
				wr_size = smd_write(smd_info->ch, buf,
						header_size + log_mask_size);
				mutex_unlock(&smd_info->smd_ch_mutex);
				if (wr_size == -ENOMEM) {
					retry_count++;
					usleep_range(10000, 10100);
				} else
				break;
			}
			if (wr_size != header_size + log_mask_size)
				pr_err("diag: log mask update failed %d, tried %d",
					wr_size, header_size + log_mask_size);
			else
				pr_debug("diag: updated log equip ID %d,len %d\n",
					 i, log_mask_size);
		} else
			pr_err("diag: ch not valid for log update\n");
		if (send_once)
			break;
	}

	mutex_unlock(&driver->diag_cntl_mutex);
}

void diag_send_event_mask_update(struct diag_smd_info *smd_info, int num_bytes)
{
	void *buf = driver->buf_event_mask_update;
	int header_size = sizeof(struct diag_ctrl_event_mask);
	int wr_size = -ENOMEM, retry_count = 0;

	if (!smd_info) {
		pr_err("diag: In %s, null smd info pointer\n",
			__func__);
		return;
	}

	mutex_lock(&driver->diag_cntl_mutex);
	if (num_bytes == 0) {
		pr_debug("diag: event mask not set yet, so no update\n");
		mutex_unlock(&driver->diag_cntl_mutex);
		return;
	}
	/* send event mask update */
	driver->event_mask->cmd_type = DIAG_CTRL_MSG_EVENT_MASK;
	driver->event_mask->data_len = 7 + num_bytes;
	driver->event_mask->stream_id = 1; /* 2, if dual stream */
	driver->event_mask->status = driver->event_status;

	switch (driver->event_status) {
	case DIAG_CTRL_MASK_ALL_DISABLED:
		driver->event_mask->event_config = 0;
		driver->event_mask->event_mask_size = 0;
		break;
	case DIAG_CTRL_MASK_ALL_ENABLED:
		driver->event_mask->event_config = 1;
		driver->event_mask->event_mask_size = 0;
		break;
	case DIAG_CTRL_MASK_VALID:
		driver->event_mask->event_config = 1;
		driver->event_mask->event_mask_size = num_bytes;
		memcpy(buf + header_size, driver->event_masks, num_bytes);
		break;
	default:
		/* Event status is not set yet or the buffer is corrupted */
		pr_err("diag: In %s, invalid status %d", __func__,
							driver->event_status);
		driver->event_mask->status = DIAG_CTRL_MASK_INVALID;
	}

	if (driver->event_mask->status == DIAG_CTRL_MASK_INVALID) {
		mutex_unlock(&driver->diag_cntl_mutex);
		return;
	}
	memcpy(buf, driver->event_mask, header_size);
	if (smd_info->ch) {
		while (retry_count < 3) {
			mutex_lock(&smd_info->smd_ch_mutex);
			wr_size = smd_write(smd_info->ch, buf,
						header_size + num_bytes);
			mutex_unlock(&smd_info->smd_ch_mutex);
			if (wr_size == -ENOMEM) {
				retry_count++;
				usleep_range(10000, 10100);
			} else
				break;
		}
		if (wr_size != header_size + num_bytes)
			pr_err("diag: error writing event mask %d, tried %d\n",
					 wr_size, header_size + num_bytes);
	} else
		pr_err("diag: ch not valid for event update\n");
	mutex_unlock(&driver->diag_cntl_mutex);
}

void diag_send_msg_mask_update(struct diag_smd_info *smd_info,
				int updated_ssid_first, int updated_ssid_last,
				int proc)
{
	void *buf = driver->buf_msg_mask_update;
	int first, last, actual_last, size = -ENOMEM, retry_count = 0;
	int header_size = sizeof(struct diag_ctrl_msg_mask);
	uint8_t *ptr = driver->msg_masks;

	if (!smd_info) {
		pr_err("diag: In %s, null smd info pointer\n",
				__func__);
		return;
	}
	mutex_lock(&driver->diag_cntl_mutex);
	while (*(uint32_t *)(ptr + 4)) {
		first = *(uint32_t *)ptr;
		ptr += 4;
		last = *(uint32_t *)ptr;
		ptr += 4;
		actual_last = *(uint32_t *)ptr;
		ptr += 4;
		if (!((updated_ssid_first >= first && updated_ssid_last <=
			 actual_last) || (updated_ssid_first == ALL_SSID))) {
			ptr += MAX_SSID_PER_RANGE*4;
			continue;
		}
		/* send f3 mask update */
		driver->msg_mask->cmd_type = DIAG_CTRL_MSG_F3_MASK;
		driver->msg_mask->status = driver->msg_status;
		switch (driver->msg_status) {
		case DIAG_CTRL_MASK_ALL_DISABLED:
			driver->msg_mask->msg_mask_size = 0;
			break;
		case DIAG_CTRL_MASK_ALL_ENABLED:
			driver->msg_mask->msg_mask_size = 1;
			memcpy(buf+header_size, ptr,
				 4 * (driver->msg_mask->msg_mask_size));
			break;
		case DIAG_CTRL_MASK_VALID:
			driver->msg_mask->msg_mask_size = actual_last -
								first + 1;
			/* Limit the msg_mask_size to MAX_SSID_PER_RANGE */
			if (driver->msg_mask->msg_mask_size >
							MAX_SSID_PER_RANGE) {
				pr_err("diag: in %s, Invalid msg mask size %d, max: %d",
					__func__,
				       driver->msg_mask->msg_mask_size,
				       MAX_SSID_PER_RANGE);
				driver->msg_mask->msg_mask_size =
							MAX_SSID_PER_RANGE;
			}
			memcpy(buf+header_size, ptr,
				 4 * (driver->msg_mask->msg_mask_size));
			break;
		default:
			/* Msg status is not set or the buffer is corrupted */
			pr_err("diag: In %s, invalid status %d", __func__,
							driver->msg_status);
			driver->msg_mask->status = DIAG_CTRL_MASK_INVALID;
		}

		if (driver->msg_mask->status == DIAG_CTRL_MASK_INVALID) {
			mutex_unlock(&driver->diag_cntl_mutex);
			return;
		}
		driver->msg_mask->data_len = 11 +
					4 * (driver->msg_mask->msg_mask_size);
		driver->msg_mask->stream_id = 1; /* 2, if dual stream */
		driver->msg_mask->msg_mode = 0; /* Legcay mode */
		driver->msg_mask->ssid_first = first;
		driver->msg_mask->ssid_last = actual_last;
		memcpy(buf, driver->msg_mask, header_size);
		if (smd_info->ch) {
			while (retry_count < 3) {
				mutex_lock(&smd_info->smd_ch_mutex);
				size = smd_write(smd_info->ch, buf, header_size
					+ 4*(driver->msg_mask->msg_mask_size));
				mutex_unlock(&smd_info->smd_ch_mutex);
				if (size == -ENOMEM) {
					retry_count++;
					usleep_range(10000, 10100);
				} else
					break;
			}
			if (size != header_size +
				 4*(driver->msg_mask->msg_mask_size))
				pr_err("diag: proc %d, msg mask update fail %d, tried %d\n",
					proc, size, (header_size +
				4*(driver->msg_mask->msg_mask_size)));
			else
				pr_debug("diag: sending mask update for ssid first %d, last %d on PROC %d\n",
					first, actual_last, proc);
		} else
			pr_err("diag: proc %d, ch invalid msg mask update\n",
								proc);
		ptr += MAX_SSID_PER_RANGE*4;
	}
	mutex_unlock(&driver->diag_cntl_mutex);
}

void diag_send_feature_mask_update(struct diag_smd_info *smd_info)
{
	void *buf = driver->buf_feature_mask_update;
	int header_size = sizeof(struct diag_ctrl_feature_mask);
	int wr_size = -ENOMEM, retry_count = 0;
	uint8_t feature_bytes[FEATURE_MASK_LEN_BYTES] = {0, 0};
	int total_len = 0;

	if (!smd_info) {
		pr_err("diag: In %s, null smd info pointer\n",
			__func__);
		return;
	}

	if (!smd_info->ch) {
		pr_err("diag: In %s, smd channel not open for peripheral: %d, type: %d\n",
				__func__, smd_info->peripheral, smd_info->type);
		return;
	}

	mutex_lock(&driver->diag_cntl_mutex);
	/* send feature mask update */
	driver->feature_mask->ctrl_pkt_id = DIAG_CTRL_MSG_FEATURE;
	driver->feature_mask->ctrl_pkt_data_len = 4 + FEATURE_MASK_LEN_BYTES;
	driver->feature_mask->feature_mask_len = FEATURE_MASK_LEN_BYTES;
	memcpy(buf, driver->feature_mask, header_size);
	feature_bytes[0] |= F_DIAG_INT_FEATURE_MASK;
	feature_bytes[0] |= F_DIAG_LOG_ON_DEMAND_RSP_ON_MASTER;
	feature_bytes[0] |= driver->supports_separate_cmdrsp ?
				F_DIAG_REQ_RSP_CHANNEL : 0;
	feature_bytes[0] |= driver->supports_apps_hdlc_encoding ?
				F_DIAG_HDLC_ENCODE_IN_APPS_MASK : 0;
	feature_bytes[1] |= F_DIAG_OVER_STM;
	memcpy(buf+header_size, &feature_bytes, FEATURE_MASK_LEN_BYTES);
	total_len = header_size + FEATURE_MASK_LEN_BYTES;

	while (retry_count < 3) {
		mutex_lock(&smd_info->smd_ch_mutex);
		wr_size = smd_write(smd_info->ch, buf, total_len);
		mutex_unlock(&smd_info->smd_ch_mutex);
		if (wr_size == -ENOMEM) {
			retry_count++;
			/*
			 * The smd channel is full. Delay while
			 * smd processes existing data and smd
			 * has memory become available. The delay
			 * of 10000 was determined empirically as
			 * best value to use.
			 */
			usleep_range(10000, 10100);
		} else
			break;
	}
	if (wr_size != total_len)
		pr_err("diag: In %s, peripheral %d fail feature update, size: %d, tried: %d",
			__func__, smd_info->peripheral, wr_size, total_len);

	mutex_unlock(&driver->diag_cntl_mutex);
}

int diag_process_apps_masks(unsigned char *buf, int len)
{
	int packet_type = 1;
	int i;
	int ssid_first, ssid_last, ssid_range;
	int rt_mask, rt_first_ssid, rt_last_ssid, rt_mask_size;
	uint8_t *rt_mask_ptr;
	int equip_id, copy_len;
#if defined(CONFIG_DIAG_OVER_USB)
	int payload_length;
#endif

	/* Set log masks */
	if (*buf == 0x73 && *(int *)(buf+4) == 3) {
		buf += 8;
		diag_update_log_mask(*(int *)buf, buf+8, *(int *)(buf+4));
		diag_update_userspace_clients(LOG_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			driver->apps_rsp_buf[0] = 0x73;
			*(int *)(driver->apps_rsp_buf + 4) = 0x3; /* op. ID */
			*(int *)(driver->apps_rsp_buf + 8) = 0x0; /* success */
			payload_length = 8 +
					LOG_ITEMS_TO_SIZE(*(int *)(buf + 4));
			if (payload_length > APPS_BUF_SIZE - 12) {
				pr_err("diag: log masks: buffer overflow\n");
				return -EIO;
			}
			for (i = 0; i < payload_length; i++)
				*(int *)(driver->apps_rsp_buf+12+i) = *(buf+i);

			for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
				if (driver->smd_cntl[i].ch)
					diag_send_log_mask_update(
						&driver->smd_cntl[i],
						*(int *)buf);
			}
			encode_rsp_and_send(12 + payload_length - 1);
			return 0;
		}
#endif
	} /* Get log masks */
	else if (*buf == 0x73 && *(int *)(buf+4) == 4) {
#if defined(CONFIG_DIAG_OVER_USB)
		if (!(driver->smd_data[MODEM_DATA].ch) &&
						chk_apps_only()) {
			equip_id = *(int *)(buf + 8);
			driver->apps_rsp_buf[0] = 0x73;
			driver->apps_rsp_buf[1] = 0x0;
			driver->apps_rsp_buf[2] = 0x0;
			driver->apps_rsp_buf[3] = 0x0;
			*(int *)(driver->apps_rsp_buf + 4) = 0x4;
			copy_len = copy_log_mask_equip(equip_id,
						driver->apps_rsp_buf + 12);
			*(int *)(driver->apps_rsp_buf + 8) =
						(copy_len == 0) ? 1 : 0;
			encode_rsp_and_send(12 + copy_len);
			return 0;
		}
#endif
	} /* Disable log masks */
	else if (*buf == 0x73 && *(int *)(buf+4) == 0) {
		/* Disable mask for each log code */
		diag_disable_log_mask();
		diag_update_userspace_clients(LOG_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			driver->apps_rsp_buf[0] = 0x73;
			driver->apps_rsp_buf[1] = 0x0;
			driver->apps_rsp_buf[2] = 0x0;
			driver->apps_rsp_buf[3] = 0x0;
			*(int *)(driver->apps_rsp_buf + 4) = 0x0;
			*(int *)(driver->apps_rsp_buf + 8) = 0x0; /* status */
			for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
				if (driver->smd_cntl[i].ch)
					diag_send_log_mask_update(
						&driver->smd_cntl[i],
						ALL_EQUIP_ID);

			}
			encode_rsp_and_send(11);
			return 0;
		}
#endif
	} /* Get runtime message mask  */
	else if ((*buf == 0x7d) && (*(buf+1) == 0x3)) {
		ssid_first = *(uint16_t *)(buf + 2);
		ssid_last = *(uint16_t *)(buf + 4);
#if defined(CONFIG_DIAG_OVER_USB)
		if (!(driver->smd_data[MODEM_DATA].ch) &&
						chk_apps_only()) {
			driver->apps_rsp_buf[0] = 0x7d;
			driver->apps_rsp_buf[1] = 0x3;
			*(uint16_t *)(driver->apps_rsp_buf+2) = ssid_first;
			*(uint16_t *)(driver->apps_rsp_buf+4) = ssid_last;
			driver->apps_rsp_buf[6] = 0x1; /* Success Status */
			driver->apps_rsp_buf[7] = 0x0;
			rt_mask_ptr = driver->msg_masks;
			while (*(uint32_t *)(rt_mask_ptr + 4)) {
				rt_first_ssid = *(uint32_t *)rt_mask_ptr;
				rt_mask_ptr += 8; /* +8 to skip 'last' */
				rt_last_ssid = *(uint32_t *)rt_mask_ptr;
				rt_mask_ptr += 4;
				if (ssid_first == rt_first_ssid && ssid_last ==
					rt_last_ssid) {
					rt_mask_size = 4 * (rt_last_ssid -
						rt_first_ssid + 1);
					if (rt_mask_size > APPS_BUF_SIZE - 8) {
						pr_err("diag: rt masks: buffer overflow\n");
						return -EIO;
					}
					memcpy(driver->apps_rsp_buf+8,
						rt_mask_ptr, rt_mask_size);
					encode_rsp_and_send(8+rt_mask_size-1);
					return 0;
				}
				rt_mask_ptr += MAX_SSID_PER_RANGE*4;
			}
		}
#endif
	} /* Set runtime message mask  */
	else if ((*buf == 0x7d) && (*(buf+1) == 0x4)) {
		ssid_first = *(uint16_t *)(buf + 2);
		ssid_last = *(uint16_t *)(buf + 4);
		if (ssid_last < ssid_first) {
			pr_err("diag: Invalid msg mask ssid values, first: %d, last: %d\n",
				ssid_first, ssid_last);
			return -EIO;
		}
		ssid_range = 4 * (ssid_last - ssid_first + 1);
		if (ssid_range > APPS_BUF_SIZE - 8) {
			pr_err("diag: Not enough space for message mask, ssid_range: %d\n",
				ssid_range);
			return -EIO;
		}
		pr_debug("diag: received mask update for ssid_first = %d, ssid_last = %d",
			ssid_first, ssid_last);
		diag_update_msg_mask(ssid_first, ssid_last , buf + 8);
		diag_update_userspace_clients(MSG_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			for (i = 0; i < 8 + ssid_range; i++)
				*(driver->apps_rsp_buf + i) = *(buf+i);
			*(driver->apps_rsp_buf + 6) = 0x1;
			for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
				if (driver->smd_cntl[i].ch)
					diag_send_msg_mask_update(
						&driver->smd_cntl[i],
						ssid_first, ssid_last,
						driver->smd_cntl[i].peripheral);

			}
			encode_rsp_and_send(8 + ssid_range - 1);
			return 0;
		}
#endif
	} /* Set ALL runtime message mask  */
	else if ((*buf == 0x7d) && (*(buf+1) == 0x5)) {
		rt_mask = *(int *)(buf + 4);
		diag_set_msg_mask(rt_mask);
		diag_update_userspace_clients(MSG_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			driver->apps_rsp_buf[0] = 0x7d; /* cmd_code */
			driver->apps_rsp_buf[1] = 0x5; /* set subcommand */
			driver->apps_rsp_buf[2] = 1; /* success */
			driver->apps_rsp_buf[3] = 0; /* rsvd */
			*(int *)(driver->apps_rsp_buf + 4) = rt_mask;
			/* send msg mask update to peripheral */
			for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
				if (driver->smd_cntl[i].ch)
					diag_send_msg_mask_update(
						&driver->smd_cntl[i],
						ALL_SSID, ALL_SSID,
						driver->smd_cntl[i].peripheral);

			}
			encode_rsp_and_send(7);
			return 0;
		}
#endif
	} else if (*buf == 0x82) {	/* event mask change */
		buf += 4;
		diag_event_num_bytes = (*(uint16_t *)buf)/8+1;
		diag_update_event_mask(buf, diag_event_num_bytes);
		diag_update_userspace_clients(EVENT_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			driver->apps_rsp_buf[0] = 0x82;
			driver->apps_rsp_buf[1] = 0x0;
			*(uint16_t *)(driver->apps_rsp_buf + 2) = 0x0;
			*(uint16_t *)(driver->apps_rsp_buf + 4) =
				EVENT_LAST_ID + 1;
			memcpy(driver->apps_rsp_buf+6, driver->event_masks,
				EVENT_LAST_ID/8+1);
			for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
				if (driver->smd_cntl[i].ch)
					diag_send_event_mask_update(
						&driver->smd_cntl[i],
						diag_event_num_bytes);
			}
			encode_rsp_and_send(6 + EVENT_LAST_ID/8);
			return 0;
		}
#endif
	} else if (*buf == 0x60) {
		diag_toggle_event_mask(*(buf+1));
		diag_update_userspace_clients(EVENT_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			driver->apps_rsp_buf[0] = 0x60;
			driver->apps_rsp_buf[1] = 0x0;
			driver->apps_rsp_buf[2] = 0x0;
			for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
				if (driver->smd_cntl[i].ch)
					diag_send_event_mask_update(
						&driver->smd_cntl[i],
						diag_event_num_bytes);
			}
			encode_rsp_and_send(2);
			return 0;
		}
#endif
	} else if (*buf == 0x78) {
		if (!(driver->smd_cntl[MODEM_DATA].ch) ||
					(driver->log_on_demand_support)) {
			driver->apps_rsp_buf[0] = 0x78;
			/* Copy log code received */
			*(uint16_t *)(driver->apps_rsp_buf + 1) =
							*(uint16_t *)(buf + 1);
			driver->apps_rsp_buf[3] = 0x1;/* Unknown */
			encode_rsp_and_send(3);
		}
	}

	return  packet_type;
}

static void diag_log_mask_init(void)
{
	struct diag_log_mask_t *log_item = NULL;
	uint8_t i;

	mutex_init(&driver->log_mask_mutex);
	log_item = (struct diag_log_mask_t *)driver->log_masks;
	for (i = 0; i < MAX_EQUIP_ID; i++, log_item++) {
		log_item->equip_id = i;
		log_item->num_items = LOG_GET_ITEM_NUM(log_code_last_tbl[i]);
	}
}

void diag_masks_init(void)
{
	driver->event_status = DIAG_CTRL_MASK_INVALID;
	driver->msg_status = DIAG_CTRL_MASK_INVALID;
	driver->log_status = DIAG_CTRL_MASK_INVALID;

	if (driver->event_mask == NULL) {
		driver->event_mask = kzalloc(sizeof(
			struct diag_ctrl_event_mask), GFP_KERNEL);
		if (driver->event_mask == NULL)
			goto err;
		kmemleak_not_leak(driver->event_mask);
	}
	if (driver->msg_mask == NULL) {
		driver->msg_mask = kzalloc(sizeof(
			struct diag_ctrl_msg_mask), GFP_KERNEL);
		if (driver->msg_mask == NULL)
			goto err;
		kmemleak_not_leak(driver->msg_mask);
	}
	if (driver->log_mask == NULL) {
		driver->log_mask = kzalloc(sizeof(
			struct diag_ctrl_log_mask), GFP_KERNEL);
		if (driver->log_mask == NULL)
			goto err;
		kmemleak_not_leak(driver->log_mask);
	}

	if (driver->buf_msg_mask_update == NULL) {
		driver->buf_msg_mask_update = kzalloc(APPS_BUF_SIZE,
								 GFP_KERNEL);
		if (driver->buf_msg_mask_update == NULL)
			goto err;
		kmemleak_not_leak(driver->buf_msg_mask_update);
	}
	if (driver->buf_log_mask_update == NULL) {
		driver->buf_log_mask_update = kzalloc(APPS_BUF_SIZE,
								 GFP_KERNEL);
		if (driver->buf_log_mask_update == NULL)
			goto err;
		kmemleak_not_leak(driver->buf_log_mask_update);
	}
	if (driver->buf_event_mask_update == NULL) {
		driver->buf_event_mask_update = kzalloc(APPS_BUF_SIZE,
								 GFP_KERNEL);
		if (driver->buf_event_mask_update == NULL)
			goto err;
		kmemleak_not_leak(driver->buf_event_mask_update);
	}
	if (driver->msg_masks == NULL) {
		driver->msg_masks = kzalloc(MSG_MASK_SIZE, GFP_KERNEL);
		if (driver->msg_masks == NULL)
			goto err;
		kmemleak_not_leak(driver->msg_masks);
	}
	if (driver->buf_feature_mask_update == NULL) {
		driver->buf_feature_mask_update = kzalloc(sizeof(
					struct diag_ctrl_feature_mask) +
					FEATURE_MASK_LEN_BYTES, GFP_KERNEL);
		if (driver->buf_feature_mask_update == NULL)
			goto err;
		kmemleak_not_leak(driver->buf_feature_mask_update);
	}
	if (driver->feature_mask == NULL) {
		driver->feature_mask = kzalloc(sizeof(
			struct diag_ctrl_feature_mask), GFP_KERNEL);
		if (driver->feature_mask == NULL)
			goto err;
		kmemleak_not_leak(driver->feature_mask);
	}
	diag_create_msg_mask_table();
	diag_event_num_bytes = 0;
	if (driver->log_masks == NULL) {
		driver->log_masks = kzalloc(LOG_MASK_SIZE, GFP_KERNEL);
		if (driver->log_masks == NULL)
			goto err;
		kmemleak_not_leak(driver->log_masks);
	}
	diag_log_mask_init();
	if (driver->event_masks == NULL) {
		driver->event_masks = kzalloc(EVENT_MASK_SIZE, GFP_KERNEL);
		if (driver->event_masks == NULL)
			goto err;
		kmemleak_not_leak(driver->event_masks);
	}
	return;
err:
	pr_err("diag: Could not initialize diag mask buffers");
	kfree(driver->event_mask);
	kfree(driver->log_mask);
	kfree(driver->msg_mask);
	kfree(driver->msg_masks);
	kfree(driver->log_masks);
	kfree(driver->event_masks);
	kfree(driver->feature_mask);
	kfree(driver->buf_feature_mask_update);
}

void diag_masks_exit(void)
{
	kfree(driver->event_mask);
	kfree(driver->log_mask);
	kfree(driver->msg_mask);
	kfree(driver->msg_masks);
	kfree(driver->log_masks);
	kfree(driver->event_masks);
	kfree(driver->feature_mask);
	kfree(driver->buf_feature_mask_update);
}
