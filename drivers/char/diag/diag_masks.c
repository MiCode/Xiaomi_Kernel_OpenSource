/* Copyright (c) 2008-2018, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include "diagchar.h"
#include "diagfwd_cntl.h"
#include "diag_masks.h"
#include "diagfwd_peripheral.h"
#include "diag_ipc_logging.h"

#define ALL_EQUIP_ID		100
#define ALL_SSID		-1

#define DIAG_SET_FEATURE_MASK(x) (feature_bytes[(x)/8] |= (1 << (x & 0x7)))

#define diag_check_update(x)	\
	(!info || (info && (info->peripheral_mask & MD_PERIPHERAL_MASK(x)))) \

struct diag_mask_info msg_mask;
struct diag_mask_info msg_bt_mask;
struct diag_mask_info log_mask;
struct diag_mask_info event_mask;

static const struct diag_ssid_range_t msg_mask_tbl[] = {
	{ .ssid_first = MSG_SSID_0, .ssid_last = MSG_SSID_0_LAST },
	{ .ssid_first = MSG_SSID_1, .ssid_last = MSG_SSID_1_LAST },
	{ .ssid_first = MSG_SSID_2, .ssid_last = MSG_SSID_2_LAST },
	{ .ssid_first = MSG_SSID_3, .ssid_last = MSG_SSID_3_LAST },
	{ .ssid_first = MSG_SSID_4, .ssid_last = MSG_SSID_4_LAST },
	{ .ssid_first = MSG_SSID_5, .ssid_last = MSG_SSID_5_LAST },
	{ .ssid_first = MSG_SSID_6, .ssid_last = MSG_SSID_6_LAST },
	{ .ssid_first = MSG_SSID_7, .ssid_last = MSG_SSID_7_LAST },
	{ .ssid_first = MSG_SSID_8, .ssid_last = MSG_SSID_8_LAST },
	{ .ssid_first = MSG_SSID_9, .ssid_last = MSG_SSID_9_LAST },
	{ .ssid_first = MSG_SSID_10, .ssid_last = MSG_SSID_10_LAST },
	{ .ssid_first = MSG_SSID_11, .ssid_last = MSG_SSID_11_LAST },
	{ .ssid_first = MSG_SSID_12, .ssid_last = MSG_SSID_12_LAST },
	{ .ssid_first = MSG_SSID_13, .ssid_last = MSG_SSID_13_LAST },
	{ .ssid_first = MSG_SSID_14, .ssid_last = MSG_SSID_14_LAST },
	{ .ssid_first = MSG_SSID_15, .ssid_last = MSG_SSID_15_LAST },
	{ .ssid_first = MSG_SSID_16, .ssid_last = MSG_SSID_16_LAST },
	{ .ssid_first = MSG_SSID_17, .ssid_last = MSG_SSID_17_LAST },
	{ .ssid_first = MSG_SSID_18, .ssid_last = MSG_SSID_18_LAST },
	{ .ssid_first = MSG_SSID_19, .ssid_last = MSG_SSID_19_LAST },
	{ .ssid_first = MSG_SSID_20, .ssid_last = MSG_SSID_20_LAST },
	{ .ssid_first = MSG_SSID_21, .ssid_last = MSG_SSID_21_LAST },
	{ .ssid_first = MSG_SSID_22, .ssid_last = MSG_SSID_22_LAST },
	{ .ssid_first = MSG_SSID_23, .ssid_last = MSG_SSID_23_LAST },
	{ .ssid_first = MSG_SSID_24, .ssid_last = MSG_SSID_24_LAST },
	{ .ssid_first = MSG_SSID_25, .ssid_last = MSG_SSID_25_LAST }
};

static int diag_apps_responds(void)
{
	/*
	 * Apps processor should respond to mask commands only if the
	 * Modem channel is up, the feature mask is received from Modem
	 * and if Modem supports Mask Centralization.
	 */
	if (!chk_apps_only())
		return 0;

	if (driver->diagfwd_cntl[PERIPHERAL_MODEM] &&
	    driver->diagfwd_cntl[PERIPHERAL_MODEM]->ch_open &&
	    driver->feature[PERIPHERAL_MODEM].rcvd_feature_mask) {
		if (driver->feature[PERIPHERAL_MODEM].mask_centralization)
			return 1;
		return 0;
	}
	return 1;
}

static void diag_send_log_mask_update(uint8_t peripheral, int equip_id)
{
	int err = 0, send_once = 0, i;
	int header_len = sizeof(struct diag_ctrl_log_mask);
	uint8_t *buf = NULL, *temp = NULL;
	uint8_t upd = 0;
	uint32_t mask_size = 0, pd_mask = 0;
	struct diag_ctrl_log_mask ctrl_pkt;
	struct diag_mask_info *mask_info = NULL;
	struct diag_log_mask_t *mask = NULL;
	struct diagfwd_info *fwd_info = NULL;

	if (peripheral >= NUM_PERIPHERALS)
		return;

	if (!driver->diagfwd_cntl[peripheral] ||
	    !driver->diagfwd_cntl[peripheral]->ch_open) {
		pr_debug("diag: In %s, control channel is not open, p: %d\n",
			 __func__, peripheral);
		return;
	}

	MD_PERIPHERAL_PD_MASK(TYPE_CNTL, peripheral, pd_mask);

	if (driver->md_session_mask != 0) {
		if (driver->md_session_mask & MD_PERIPHERAL_MASK(peripheral)) {
			if (driver->md_session_map[peripheral])
				mask_info =
				driver->md_session_map[peripheral]->log_mask;
		} else if (driver->md_session_mask & pd_mask) {
			upd = diag_mask_to_pd_value(driver->md_session_mask);
			if (upd && driver->md_session_map[upd])
				mask_info =
				driver->md_session_map[upd]->log_mask;
		} else {
			DIAG_LOG(DIAG_DEBUG_MASKS,
			"asking for mask update with unknown session mask\n");
			return;
		}
	} else {
		mask_info = &log_mask;
	}

	if (!mask_info || !mask_info->ptr || !mask_info->update_buf)
		return;

	mask = (struct diag_log_mask_t *)mask_info->ptr;
	if (!mask->ptr)
		return;
	buf = mask_info->update_buf;

	switch (mask_info->status) {
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
		send_once = 0;
		break;
	default:
		pr_debug("diag: In %s, invalid log_mask status\n", __func__);
		return;
	}

	mutex_lock(&mask_info->lock);
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		if (equip_id != i && equip_id != ALL_EQUIP_ID)
			continue;

		mutex_lock(&mask->lock);
		ctrl_pkt.cmd_type = DIAG_CTRL_MSG_LOG_MASK;
		ctrl_pkt.stream_id = 1;
		ctrl_pkt.status = mask_info->status;
		if (mask_info->status == DIAG_CTRL_MASK_VALID) {
			mask_size = LOG_ITEMS_TO_SIZE(mask->num_items_tools);
			ctrl_pkt.equip_id = i;
			ctrl_pkt.num_items = mask->num_items_tools;
			ctrl_pkt.log_mask_size = mask_size;
		}
		ctrl_pkt.data_len = LOG_MASK_CTRL_HEADER_LEN + mask_size;

		if (header_len + mask_size > mask_info->update_buf_len) {
			temp = krealloc(buf, header_len + mask_size,
					GFP_KERNEL);
			if (!temp) {
				pr_err_ratelimited("diag: Unable to realloc log update buffer, new size: %d, equip_id: %d\n",
				       header_len + mask_size, equip_id);
				mutex_unlock(&mask->lock);
				break;
			}
			mask_info->update_buf = temp;
			mask_info->update_buf_len = header_len + mask_size;
		}

		memcpy(buf, &ctrl_pkt, header_len);
		if (mask_size > 0)
			memcpy(buf + header_len, mask->ptr, mask_size);
		mutex_unlock(&mask->lock);

		DIAG_LOG(DIAG_DEBUG_MASKS,
			 "sending ctrl pkt to %d, e %d num_items %d size %d\n",
			 peripheral, i, ctrl_pkt.num_items,
			 ctrl_pkt.log_mask_size);

		err = diagfwd_write(peripheral, TYPE_CNTL,
				    buf, header_len + mask_size);
		if (err && err != -ENODEV)
			pr_err_ratelimited("diag: Unable to send log masks to peripheral %d, equip_id: %d, err: %d\n",
			       peripheral, i, err);
		if (send_once || equip_id != ALL_EQUIP_ID)
			break;

	}
	mutex_unlock(&mask_info->lock);
}

static void diag_send_event_mask_update(uint8_t peripheral)
{
	uint8_t *buf = NULL, *temp = NULL;
	uint8_t upd = 0;
	uint32_t pd_mask = 0;
	int num_bytes = EVENT_COUNT_TO_BYTES(driver->last_event_id);
	int write_len = 0, err = 0, i = 0, temp_len = 0;
	struct diag_ctrl_event_mask header;
	struct diag_mask_info *mask_info = NULL;
	struct diagfwd_info *fwd_info = NULL;

	if (num_bytes <= 0 || num_bytes > driver->event_mask_size) {
		pr_debug("diag: In %s, invalid event mask length %d\n",
			 __func__, num_bytes);
		return;
	}

	if (peripheral >= NUM_PERIPHERALS)
		return;

	if (!driver->diagfwd_cntl[peripheral] ||
	    !driver->diagfwd_cntl[peripheral]->ch_open) {
		pr_debug("diag: In %s, control channel is not open, p: %d\n",
			 __func__, peripheral);
		return;
	}

	MD_PERIPHERAL_PD_MASK(TYPE_CNTL, peripheral, pd_mask);

	if (driver->md_session_mask != 0) {
		if (driver->md_session_mask & MD_PERIPHERAL_MASK(peripheral)) {
			if (driver->md_session_map[peripheral])
				mask_info =
				driver->md_session_map[peripheral]->event_mask;
		} else if (driver->md_session_mask & pd_mask) {
			upd = diag_mask_to_pd_value(driver->md_session_mask);
			if (upd && driver->md_session_map[upd])
				mask_info =
				driver->md_session_map[upd]->event_mask;
		} else {
			DIAG_LOG(DIAG_DEBUG_MASKS,
			"asking for mask update with unknown session mask\n");
			return;
		}
	} else {
		mask_info = &event_mask;
	}

	if (!mask_info || !mask_info->ptr || !mask_info->update_buf)
		return;

	buf = mask_info->update_buf;
	mutex_lock(&mask_info->lock);
	header.cmd_type = DIAG_CTRL_MSG_EVENT_MASK;
	header.stream_id = 1;
	header.status = mask_info->status;

	switch (mask_info->status) {
	case DIAG_CTRL_MASK_ALL_DISABLED:
		header.event_config = 0;
		header.event_mask_size = 0;
		break;
	case DIAG_CTRL_MASK_ALL_ENABLED:
		header.event_config = 1;
		header.event_mask_size = 0;
		break;
	case DIAG_CTRL_MASK_VALID:
		header.event_config = 1;
		header.event_mask_size = num_bytes;
		if (num_bytes + sizeof(header) > mask_info->update_buf_len) {
			temp_len = num_bytes + sizeof(header);
			temp = krealloc(buf, temp_len, GFP_KERNEL);
			if (!temp) {
				pr_err("diag: Unable to realloc event mask update buffer\n");
				goto err;
			} else {
				mask_info->update_buf = temp;
				mask_info->update_buf_len = temp_len;
			}
		}
		memcpy(buf + sizeof(header), mask_info->ptr, num_bytes);
		write_len += num_bytes;
		break;
	default:
		pr_debug("diag: In %s, invalid status %d\n", __func__,
			 mask_info->status);
		goto err;
	}
	header.data_len = EVENT_MASK_CTRL_HEADER_LEN + header.event_mask_size;
	memcpy(buf, &header, sizeof(header));
	write_len += sizeof(header);

	err = diagfwd_write(peripheral, TYPE_CNTL, buf, write_len);
	if (err && err != -ENODEV)
		pr_err_ratelimited("diag: Unable to send event masks to peripheral %d\n",
		       peripheral);
err:
	mutex_unlock(&mask_info->lock);
}

static void diag_send_msg_mask_update(uint8_t peripheral, int first, int last)
{
	int i, err = 0, temp_len = 0;
	int header_len = sizeof(struct diag_ctrl_msg_mask);
	uint8_t *buf = NULL, *temp = NULL;
	uint8_t upd = 0;
	uint8_t msg_mask_tbl_count_local;
	uint32_t mask_size = 0, pd_mask = 0;
	struct diag_mask_info *mask_info = NULL;
	struct diag_msg_mask_t *mask = NULL;
	struct diag_ctrl_msg_mask header;
	struct diagfwd_info *fwd_info = NULL;

	if (peripheral >= NUM_PERIPHERALS)
		return;

	if (!driver->diagfwd_cntl[peripheral] ||
	    !driver->diagfwd_cntl[peripheral]->ch_open) {
		pr_debug("diag: In %s, control channel is not open, p: %d\n",
			 __func__, peripheral);
		return;
	}

	MD_PERIPHERAL_PD_MASK(TYPE_CNTL, peripheral, pd_mask);

	if (driver->md_session_mask != 0) {
		if (driver->md_session_mask & MD_PERIPHERAL_MASK(peripheral)) {
			if (driver->md_session_map[peripheral])
				mask_info =
				driver->md_session_map[peripheral]->msg_mask;
		} else if (driver->md_session_mask & pd_mask) {
			upd = diag_mask_to_pd_value(driver->md_session_mask);
			if (upd && driver->md_session_map[upd])
				mask_info =
				driver->md_session_map[upd]->msg_mask;
		} else {
			DIAG_LOG(DIAG_DEBUG_MASKS,
			"asking for mask update with unknown session mask\n");
			return;
		}
	} else {
		mask_info = &msg_mask;
	}

	if (!mask_info || !mask_info->ptr || !mask_info->update_buf)
		return;
	mutex_lock(&driver->msg_mask_lock);
	mask = (struct diag_msg_mask_t *)mask_info->ptr;
	if (!mask->ptr) {
		mutex_unlock(&driver->msg_mask_lock);
		return;
	}
	buf = mask_info->update_buf;
	msg_mask_tbl_count_local = driver->msg_mask_tbl_count;
	mutex_unlock(&driver->msg_mask_lock);
	mutex_lock(&mask_info->lock);
	switch (mask_info->status) {
	case DIAG_CTRL_MASK_ALL_DISABLED:
		mask_size = 0;
		break;
	case DIAG_CTRL_MASK_ALL_ENABLED:
		mask_size = 1;
		break;
	case DIAG_CTRL_MASK_VALID:
		break;
	default:
		pr_debug("diag: In %s, invalid status: %d\n", __func__,
			 mask_info->status);
		goto err;
	}

	for (i = 0; i < msg_mask_tbl_count_local; i++, mask++) {
		mutex_lock(&driver->msg_mask_lock);
		if (((mask->ssid_first > first) ||
			(mask->ssid_last_tools < last)) && first != ALL_SSID) {
			mutex_unlock(&driver->msg_mask_lock);
			continue;
		}

		mutex_lock(&mask->lock);
		if (mask_info->status == DIAG_CTRL_MASK_VALID) {
			mask_size =
				mask->ssid_last_tools - mask->ssid_first + 1;
			temp_len = mask_size * sizeof(uint32_t);
			if (temp_len + header_len <= mask_info->update_buf_len)
				goto proceed;
			temp = krealloc(mask_info->update_buf, temp_len,
					GFP_KERNEL);
			if (!temp) {
				pr_err("diag: In %s, unable to realloc msg_mask update buffer\n",
				       __func__);
				mask_size = (mask_info->update_buf_len -
					    header_len) / sizeof(uint32_t);
			} else {
				mask_info->update_buf = temp;
				mask_info->update_buf_len = temp_len;
				pr_debug("diag: In %s, successfully reallocated msg_mask update buffer to len: %d\n",
					 __func__, mask_info->update_buf_len);
			}
		} else if (mask_info->status == DIAG_CTRL_MASK_ALL_ENABLED) {
			mask_size = 1;
		}
proceed:
		header.cmd_type = DIAG_CTRL_MSG_F3_MASK;
		header.status = mask_info->status;
		header.stream_id = 1;
		header.msg_mode = 0;
		header.ssid_first = mask->ssid_first;
		header.ssid_last = mask->ssid_last_tools;
		header.msg_mask_size = mask_size;
		mask_size *= sizeof(uint32_t);
		header.data_len = MSG_MASK_CTRL_HEADER_LEN + mask_size;
		memcpy(buf, &header, header_len);
		if (mask_size > 0)
			memcpy(buf + header_len, mask->ptr, mask_size);
		mutex_unlock(&mask->lock);
		mutex_unlock(&driver->msg_mask_lock);

		err = diagfwd_write(peripheral, TYPE_CNTL, buf,
				    header_len + mask_size);
		if (err && err != -ENODEV)
			pr_err_ratelimited("diag: Unable to send msg masks to peripheral %d, error = %d\n",
			       peripheral, err);

		if (first != ALL_SSID)
			break;
	}
err:
	mutex_unlock(&mask_info->lock);
}

static void diag_send_time_sync_update(uint8_t peripheral)
{
	struct diag_ctrl_msg_time_sync time_sync_msg;
	int msg_size = sizeof(struct diag_ctrl_msg_time_sync);
	int err = 0;

	if (peripheral >= NUM_PERIPHERALS) {
		pr_err("diag: In %s, Invalid peripheral, %d\n",
				__func__, peripheral);
		return;
	}

	if (!driver->diagfwd_cntl[peripheral] ||
		!driver->diagfwd_cntl[peripheral]->ch_open) {
		pr_err("diag: In %s, control channel is not open, p: %d, %pK\n",
			__func__, peripheral, driver->diagfwd_cntl[peripheral]);
		return;
	}

	mutex_lock(&driver->diag_cntl_mutex);
	time_sync_msg.ctrl_pkt_id = DIAG_CTRL_MSG_TIME_SYNC_PKT;
	time_sync_msg.ctrl_pkt_data_len = 5;
	time_sync_msg.version = 1;
	time_sync_msg.time_api = driver->uses_time_api;

	err = diagfwd_write(peripheral, TYPE_CNTL, &time_sync_msg, msg_size);
	if (err)
		pr_err("diag: In %s, unable to write to peripheral: %d, type: %d, len: %d, err: %d\n",
				__func__, peripheral, TYPE_CNTL,
				msg_size, err);
	mutex_unlock(&driver->diag_cntl_mutex);
}

static void diag_send_feature_mask_update(uint8_t peripheral)
{
	void *buf = driver->buf_feature_mask_update;
	int header_size = sizeof(struct diag_ctrl_feature_mask);
	uint8_t feature_bytes[FEATURE_MASK_LEN] = {0, 0};
	struct diag_ctrl_feature_mask feature_mask;
	int total_len = 0;
	int err = 0;

	if (peripheral >= NUM_PERIPHERALS) {
		pr_err("diag: In %s, Invalid peripheral, %d\n",
			__func__, peripheral);
		return;
	}

	if (!driver->diagfwd_cntl[peripheral] ||
	    !driver->diagfwd_cntl[peripheral]->ch_open) {
		pr_err("diag: In %s, control channel is not open, p: %d, %pK\n",
		       __func__, peripheral, driver->diagfwd_cntl[peripheral]);
		return;
	}

	mutex_lock(&driver->diag_cntl_mutex);
	/* send feature mask update */
	feature_mask.ctrl_pkt_id = DIAG_CTRL_MSG_FEATURE;
	feature_mask.ctrl_pkt_data_len = sizeof(uint32_t) + FEATURE_MASK_LEN;
	feature_mask.feature_mask_len = FEATURE_MASK_LEN;
	memcpy(buf, &feature_mask, header_size);
	DIAG_SET_FEATURE_MASK(F_DIAG_FEATURE_MASK_SUPPORT);
	DIAG_SET_FEATURE_MASK(F_DIAG_LOG_ON_DEMAND_APPS);
	DIAG_SET_FEATURE_MASK(F_DIAG_STM);
	DIAG_SET_FEATURE_MASK(F_DIAG_DCI_EXTENDED_HEADER_SUPPORT);
	DIAG_SET_FEATURE_MASK(F_DIAG_DIAGID_SUPPORT);
	if (driver->supports_separate_cmdrsp)
		DIAG_SET_FEATURE_MASK(F_DIAG_REQ_RSP_SUPPORT);
	if (driver->supports_apps_hdlc_encoding)
		DIAG_SET_FEATURE_MASK(F_DIAG_APPS_HDLC_ENCODE);
	if (driver->supports_apps_header_untagging) {
		if (driver->feature[peripheral].untag_header) {
			DIAG_SET_FEATURE_MASK(F_DIAG_PKT_HEADER_UNTAG);
			driver->peripheral_untag[peripheral] =
				ENABLE_PKT_HEADER_UNTAGGING;
		}
	}
	DIAG_SET_FEATURE_MASK(F_DIAG_MASK_CENTRALIZATION);
	if (driver->supports_sockets)
		DIAG_SET_FEATURE_MASK(F_DIAG_SOCKETS_ENABLED);

	memcpy(buf + header_size, &feature_bytes, FEATURE_MASK_LEN);
	total_len = header_size + FEATURE_MASK_LEN;

	err = diagfwd_write(peripheral, TYPE_CNTL, buf, total_len);
	if (err) {
		pr_err_ratelimited("diag: In %s, unable to write feature mask to peripheral: %d, type: %d, len: %d, err: %d\n",
		       __func__, peripheral, TYPE_CNTL,
		       total_len, err);
		mutex_unlock(&driver->diag_cntl_mutex);
		return;
	}
	driver->feature[peripheral].sent_feature_mask = 1;
	mutex_unlock(&driver->diag_cntl_mutex);
}

static int diag_cmd_get_ssid_range(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int i;
	int write_len = 0;
	struct diag_msg_mask_t *mask_ptr = NULL;
	struct diag_msg_ssid_query_t rsp;
	struct diag_ssid_range_t ssid_range;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);
	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	if (!diag_apps_responds()) {
		mutex_unlock(&driver->md_session_lock);
		return 0;
	}
	mutex_lock(&driver->msg_mask_lock);
	rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
	rsp.sub_cmd = DIAG_CMD_OP_GET_SSID_RANGE;
	rsp.status = MSG_STATUS_SUCCESS;
	rsp.padding = 0;
	rsp.count = driver->msg_mask_tbl_count;
	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len += sizeof(rsp);
	mask_ptr = (struct diag_msg_mask_t *)mask_info->ptr;
	for (i = 0; i <  driver->msg_mask_tbl_count; i++, mask_ptr++) {
		if (write_len + sizeof(ssid_range) > dest_len) {
			pr_err("diag: In %s, Truncating response due to size limitations of rsp buffer\n",
			       __func__);
			break;
		}
		ssid_range.ssid_first = mask_ptr->ssid_first;
		ssid_range.ssid_last = mask_ptr->ssid_last_tools;
		memcpy(dest_buf + write_len, &ssid_range, sizeof(ssid_range));
		write_len += sizeof(ssid_range);
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&driver->md_session_lock);
	return write_len;
}

static int diag_cmd_get_build_mask(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int i = 0;
	int write_len = 0;
	int num_entries = 0;
	int copy_len = 0;
	struct diag_msg_mask_t *build_mask = NULL;
	struct diag_build_mask_req_t *req = NULL;
	struct diag_msg_build_mask_t rsp;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d\n",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	if (!diag_apps_responds())
		return 0;
	mutex_lock(&driver->msg_mask_lock);
	req = (struct diag_build_mask_req_t *)src_buf;
	rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
	rsp.sub_cmd = DIAG_CMD_OP_GET_BUILD_MASK;
	rsp.ssid_first = req->ssid_first;
	rsp.ssid_last = req->ssid_last;
	rsp.status = MSG_STATUS_FAIL;
	rsp.padding = 0;
	build_mask = (struct diag_msg_mask_t *)msg_bt_mask.ptr;
	for (i = 0; i < driver->bt_msg_mask_tbl_count; i++, build_mask++) {
		if (build_mask->ssid_first != req->ssid_first)
			continue;
		num_entries = req->ssid_last - req->ssid_first + 1;
		if (num_entries > build_mask->range) {
			pr_warn("diag: In %s, truncating ssid range for ssid_first: %d ssid_last %d\n",
				__func__, req->ssid_first, req->ssid_last);
			num_entries = build_mask->range;
			req->ssid_last = req->ssid_first + build_mask->range;
		}
		copy_len = num_entries * sizeof(uint32_t);
		if (copy_len + sizeof(rsp) > dest_len)
			copy_len = dest_len - sizeof(rsp);
		memcpy(dest_buf + sizeof(rsp), build_mask->ptr, copy_len);
		write_len += copy_len;
		rsp.ssid_last = build_mask->ssid_last;
		rsp.status = MSG_STATUS_SUCCESS;
		break;
	}
	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len += sizeof(rsp);
	mutex_unlock(&driver->msg_mask_lock);
	return write_len;
}

static int diag_cmd_get_msg_mask(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int i;
	int write_len = 0;
	uint32_t mask_size = 0;
	struct diag_msg_mask_t *mask = NULL;
	struct diag_build_mask_req_t *req = NULL;
	struct diag_msg_build_mask_t rsp;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!diag_apps_responds()) {
		mutex_unlock(&driver->md_session_lock);
		return 0;
	}

	mutex_lock(&driver->msg_mask_lock);
	req = (struct diag_build_mask_req_t *)src_buf;
	rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
	rsp.sub_cmd = DIAG_CMD_OP_GET_MSG_MASK;
	rsp.ssid_first = req->ssid_first;
	rsp.ssid_last = req->ssid_last;
	rsp.status = MSG_STATUS_FAIL;
	rsp.padding = 0;
	mask = (struct diag_msg_mask_t *)mask_info->ptr;
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		if ((req->ssid_first < mask->ssid_first) ||
		    (req->ssid_first > mask->ssid_last_tools)) {
			continue;
		}
		mask_size = mask->range * sizeof(uint32_t);
		/* Copy msg mask only till the end of the rsp buffer */
		if (mask_size + sizeof(rsp) > dest_len)
			mask_size = dest_len - sizeof(rsp);
		memcpy(dest_buf + sizeof(rsp), mask->ptr, mask_size);
		write_len += mask_size;
		rsp.status = MSG_STATUS_SUCCESS;
		break;
	}
	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len += sizeof(rsp);
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&driver->md_session_lock);
	return write_len;
}

static int diag_cmd_set_msg_mask(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	uint32_t mask_size = 0, offset = 0;
	uint32_t *temp = NULL;
	int write_len = 0, i = 0, found = 0, peripheral;
	int header_len = sizeof(struct diag_msg_build_mask_t);
	struct diag_msg_mask_t *mask = NULL;
	struct diag_msg_build_mask_t *req = NULL;
	struct diag_msg_build_mask_t rsp;
	struct diag_mask_info *mask_info = NULL;
	struct diag_msg_mask_t *mask_next = NULL;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	req = (struct diag_msg_build_mask_t *)src_buf;
	mutex_lock(&mask_info->lock);
	mutex_lock(&driver->msg_mask_lock);
	mask = (struct diag_msg_mask_t *)mask_info->ptr;
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&mask_info->lock);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		if (i < (driver->msg_mask_tbl_count - 1)) {
			mask_next = mask;
			mask_next++;
		} else
			mask_next = NULL;

		if ((req->ssid_first < mask->ssid_first) ||
		    (req->ssid_first > mask->ssid_first + MAX_SSID_PER_RANGE) ||
		    (mask_next && (req->ssid_first >= mask_next->ssid_first))) {
			continue;
		}
		mask_next = NULL;
		found = 1;
		mutex_lock(&mask->lock);
		mask_size = req->ssid_last - req->ssid_first + 1;
		if (mask_size > MAX_SSID_PER_RANGE) {
			pr_warn("diag: In %s, truncating ssid range, %d-%d to max allowed: %d\n",
				__func__, mask->ssid_first, mask->ssid_last,
				MAX_SSID_PER_RANGE);
			mask_size = MAX_SSID_PER_RANGE;
			mask->range_tools = MAX_SSID_PER_RANGE;
			mask->ssid_last_tools =
				mask->ssid_first + mask->range_tools;
		}
		if (req->ssid_last > mask->ssid_last_tools) {
			pr_debug("diag: Msg SSID range mismatch\n");
			if (mask_size != MAX_SSID_PER_RANGE)
				mask->ssid_last_tools = req->ssid_last;
			mask->range_tools =
				mask->ssid_last_tools - mask->ssid_first + 1;
			temp = krealloc(mask->ptr,
					mask->range_tools * sizeof(uint32_t),
					GFP_KERNEL);
			if (!temp) {
				pr_err_ratelimited("diag: In %s, unable to allocate memory for msg mask ptr, mask_size: %d\n",
						   __func__, mask_size);
				mutex_unlock(&mask->lock);
				mutex_unlock(&driver->msg_mask_lock);
				mutex_unlock(&mask_info->lock);
				mutex_unlock(&driver->md_session_lock);
				return -ENOMEM;
			}
			mask->ptr = temp;
		}

		offset = req->ssid_first - mask->ssid_first;
		if (offset + mask_size > mask->range_tools) {
			pr_err("diag: In %s, Not in msg mask range, mask_size: %d, offset: %d\n",
			       __func__, mask_size, offset);
			mutex_unlock(&mask->lock);
			break;
		}
		mask_size = mask_size * sizeof(uint32_t);
		memcpy(mask->ptr + offset, src_buf + header_len, mask_size);
		mutex_unlock(&mask->lock);
		mask_info->status = DIAG_CTRL_MASK_VALID;
		break;
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	if (diag_check_update(APPS_DATA))
		diag_update_userspace_clients(MSG_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
	rsp.sub_cmd = DIAG_CMD_OP_SET_MSG_MASK;
	rsp.ssid_first = req->ssid_first;
	rsp.ssid_last = req->ssid_last;
	rsp.status = found;
	rsp.padding = 0;
	memcpy(dest_buf, &rsp, header_len);
	write_len += header_len;
	if (!found)
		goto end;
	if (mask_size + write_len > dest_len)
		mask_size = dest_len - write_len;
	memcpy(dest_buf + write_len, src_buf + header_len, mask_size);
	write_len += mask_size;
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA)
			continue;
		if (!diag_check_update(i))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		mutex_lock(&driver->md_session_lock);
		diag_send_msg_mask_update(peripheral, req->ssid_first,
			req->ssid_last);
		mutex_unlock(&driver->md_session_lock);
	}
end:
	return write_len;
}

static int diag_cmd_set_all_msg_mask(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int i, write_len = 0, peripheral;
	int header_len = sizeof(struct diag_msg_config_rsp_t);
	struct diag_msg_config_rsp_t rsp;
	struct diag_msg_config_rsp_t *req = NULL;
	struct diag_msg_mask_t *mask = NULL;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	req = (struct diag_msg_config_rsp_t *)src_buf;

	mutex_lock(&mask_info->lock);
	mutex_lock(&driver->msg_mask_lock);

	mask = (struct diag_msg_mask_t *)mask_info->ptr;
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&mask_info->lock);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	mask_info->status = (req->rt_mask) ? DIAG_CTRL_MASK_ALL_ENABLED :
					   DIAG_CTRL_MASK_ALL_DISABLED;
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		mutex_lock(&mask->lock);
		memset(mask->ptr, req->rt_mask,
		       mask->range * sizeof(uint32_t));
		mutex_unlock(&mask->lock);
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	if (diag_check_update(APPS_DATA))
		diag_update_userspace_clients(MSG_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
	rsp.sub_cmd = DIAG_CMD_OP_SET_ALL_MSG_MASK;
	rsp.status = MSG_STATUS_SUCCESS;
	rsp.padding = 0;
	rsp.rt_mask = req->rt_mask;
	memcpy(dest_buf, &rsp, header_len);
	write_len += header_len;

	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA)
			continue;
		if (!diag_check_update(i))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		mutex_lock(&driver->md_session_lock);
		diag_send_msg_mask_update(peripheral, ALL_SSID, ALL_SSID);
		mutex_unlock(&driver->md_session_lock);
	}

	return write_len;
}

static int diag_cmd_get_event_mask(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int write_len = 0;
	uint32_t mask_size;
	struct diag_event_mask_config_t rsp;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d\n",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	if (!diag_apps_responds())
		return 0;

	mask_size = EVENT_COUNT_TO_BYTES(driver->last_event_id);
	if (mask_size + sizeof(rsp) > dest_len) {
		pr_err("diag: In %s, invalid mask size: %d\n", __func__,
		       mask_size);
		return -ENOMEM;
	}

	rsp.cmd_code = DIAG_CMD_GET_EVENT_MASK;
	rsp.status = EVENT_STATUS_SUCCESS;
	rsp.padding = 0;
	rsp.num_bits = driver->last_event_id + 1;
	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len += sizeof(rsp);
	memcpy(dest_buf + write_len, event_mask.ptr, mask_size);
	write_len += mask_size;

	return write_len;
}

static int diag_cmd_update_event_mask(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int i, write_len = 0, mask_len = 0, peripheral;
	int header_len = sizeof(struct diag_event_mask_config_t);
	struct diag_event_mask_config_t rsp;
	struct diag_event_mask_config_t *req;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);
	mask_info = (!info) ? &event_mask : info->event_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	req = (struct diag_event_mask_config_t *)src_buf;
	mask_len = EVENT_COUNT_TO_BYTES(req->num_bits);
	if (mask_len <= 0 || mask_len > event_mask.mask_len) {
		pr_err("diag: In %s, invalid event mask len: %d\n", __func__,
		       mask_len);
		mutex_unlock(&driver->md_session_lock);
		return -EIO;
	}

	mutex_lock(&mask_info->lock);
	memcpy(mask_info->ptr, src_buf + header_len, mask_len);
	mask_info->status = DIAG_CTRL_MASK_VALID;
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	if (diag_check_update(APPS_DATA))
		diag_update_userspace_clients(EVENT_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	rsp.cmd_code = DIAG_CMD_SET_EVENT_MASK;
	rsp.status = EVENT_STATUS_SUCCESS;
	rsp.padding = 0;
	rsp.num_bits = driver->last_event_id + 1;
	memcpy(dest_buf, &rsp, header_len);
	write_len += header_len;
	memcpy(dest_buf + write_len, mask_info->ptr, mask_len);
	write_len += mask_len;

	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA)
			continue;
		if (!diag_check_update(i))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		mutex_lock(&driver->md_session_lock);
		diag_send_event_mask_update(peripheral);
		mutex_unlock(&driver->md_session_lock);
	}

	return write_len;
}

static int diag_cmd_toggle_events(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int write_len = 0, i, peripheral;
	uint8_t toggle = 0;
	struct diag_event_report_t header;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);
	mask_info = (!info) ? &event_mask : info->event_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	toggle = *(src_buf + 1);
	mutex_lock(&mask_info->lock);
	if (toggle) {
		mask_info->status = DIAG_CTRL_MASK_ALL_ENABLED;
		memset(mask_info->ptr, 0xFF, mask_info->mask_len);
	} else {
		mask_info->status = DIAG_CTRL_MASK_ALL_DISABLED;
		memset(mask_info->ptr, 0, mask_info->mask_len);
	}
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	if (diag_check_update(APPS_DATA))
		diag_update_userspace_clients(EVENT_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	header.cmd_code = DIAG_CMD_EVENT_TOGGLE;
	header.padding = 0;
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA)
			continue;
		if (!diag_check_update(i))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		mutex_lock(&driver->md_session_lock);
		diag_send_event_mask_update(peripheral);
		mutex_unlock(&driver->md_session_lock);
	}
	memcpy(dest_buf, &header, sizeof(header));
	write_len += sizeof(header);

	return write_len;
}

static int diag_cmd_get_log_mask(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int i;
	int status = LOG_STATUS_INVALID;
	int write_len = 0;
	int read_len = 0;
	int req_header_len = sizeof(struct diag_log_config_req_t);
	int rsp_header_len = sizeof(struct diag_log_config_rsp_t);
	uint32_t mask_size = 0;
	struct diag_log_mask_t *log_item = NULL;
	struct diag_log_config_req_t *req;
	struct diag_log_config_rsp_t rsp;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &log_mask : info->log_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	if (!diag_apps_responds()) {
		mutex_unlock(&driver->md_session_lock);
		return 0;
	}

	req = (struct diag_log_config_req_t *)src_buf;
	read_len += req_header_len;

	rsp.cmd_code = DIAG_CMD_LOG_CONFIG;
	rsp.padding[0] = 0;
	rsp.padding[1] = 0;
	rsp.padding[2] = 0;
	rsp.sub_cmd = DIAG_CMD_OP_GET_LOG_MASK;
	/*
	 * Don't copy the response header now. Copy at the end after
	 * calculating the status field value
	 */
	write_len += rsp_header_len;

	log_item = (struct diag_log_mask_t *)mask_info->ptr;
	if (!log_item->ptr) {
		pr_err("diag: Invalid input in %s, mask: %pK\n",
			__func__, log_item);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	for (i = 0; i < MAX_EQUIP_ID; i++, log_item++) {
		if (log_item->equip_id != req->equip_id)
			continue;
		mutex_lock(&log_item->lock);
		mask_size = LOG_ITEMS_TO_SIZE(log_item->num_items_tools);
		/*
		 * Make sure we have space to fill the response in the buffer.
		 * Destination buffer should atleast be able to hold equip_id
		 * (uint32_t), num_items(uint32_t), mask (mask_size) and the
		 * response header.
		 */
		if ((mask_size + (2 * sizeof(uint32_t)) + rsp_header_len) >
								dest_len) {
			pr_err("diag: In %s, invalid length: %d, max rsp_len: %d\n",
				__func__, mask_size, dest_len);
			status = LOG_STATUS_FAIL;
			mutex_unlock(&log_item->lock);
			break;
		}
		*(uint32_t *)(dest_buf + write_len) = log_item->equip_id;
		write_len += sizeof(uint32_t);
		*(uint32_t *)(dest_buf + write_len) = log_item->num_items_tools;
		write_len += sizeof(uint32_t);
		if (mask_size > 0) {
			memcpy(dest_buf + write_len, log_item->ptr, mask_size);
			write_len += mask_size;
		}
		DIAG_LOG(DIAG_DEBUG_MASKS,
			 "sending log e %d num_items %d size %d\n",
			 log_item->equip_id, log_item->num_items_tools,
			 log_item->range_tools);
		mutex_unlock(&log_item->lock);
		status = LOG_STATUS_SUCCESS;
		break;
	}

	rsp.status = status;
	memcpy(dest_buf, &rsp, rsp_header_len);

	mutex_unlock(&driver->md_session_lock);
	return write_len;
}

static int diag_cmd_get_log_range(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	int i;
	int write_len = 0;
	struct diag_log_config_rsp_t rsp;
	struct diag_log_mask_t *mask = (struct diag_log_mask_t *)log_mask.ptr;

	if (!mask)
		return -EINVAL;

	if (!diag_apps_responds())
		return 0;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d\n",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	rsp.cmd_code = DIAG_CMD_LOG_CONFIG;
	rsp.padding[0] = 0;
	rsp.padding[1] = 0;
	rsp.padding[2] = 0;
	rsp.sub_cmd = DIAG_CMD_OP_GET_LOG_RANGE;
	rsp.status = LOG_STATUS_SUCCESS;
	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len += sizeof(rsp);

	for (i = 0; i < MAX_EQUIP_ID && write_len < dest_len; i++, mask++) {
		*(uint32_t *)(dest_buf + write_len) = mask->num_items_tools;
		write_len += sizeof(uint32_t);
	}

	return write_len;
}

static int diag_cmd_set_log_mask(unsigned char *src_buf, int src_len,
				 unsigned char *dest_buf, int dest_len,
				 int pid)
{
	int i, peripheral, write_len = 0;
	int status = LOG_STATUS_SUCCESS;
	int read_len = 0, payload_len = 0;
	int req_header_len = sizeof(struct diag_log_config_req_t);
	int rsp_header_len = sizeof(struct diag_log_config_set_rsp_t);
	uint32_t mask_size = 0;
	struct diag_log_config_req_t *req;
	struct diag_log_config_set_rsp_t rsp;
	struct diag_log_mask_t *mask = NULL;
	struct diag_mask_info *mask_info = NULL;
	unsigned char *temp_buf = NULL;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &log_mask : info->log_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	req = (struct diag_log_config_req_t *)src_buf;
	read_len += req_header_len;
	mask = (struct diag_log_mask_t *)mask_info->ptr;
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (req->equip_id >= MAX_EQUIP_ID) {
		pr_err("diag: In %s, Invalid logging mask request, equip_id: %d\n",
		       __func__, req->equip_id);
		status = LOG_STATUS_INVALID;
	}

	if (req->num_items == 0) {
		pr_err("diag: In %s, Invalid number of items in log mask request, equip_id: %d\n",
		       __func__, req->equip_id);
		status = LOG_STATUS_INVALID;
	}

	mutex_lock(&mask_info->lock);
	for (i = 0; i < MAX_EQUIP_ID && !status; i++, mask++) {
		if (mask->equip_id != req->equip_id)
			continue;
		mutex_lock(&mask->lock);

		DIAG_LOG(DIAG_DEBUG_MASKS, "e: %d current: %d %d new: %d %d",
			 mask->equip_id, mask->num_items_tools,
			 mask->range_tools, req->num_items,
			 LOG_ITEMS_TO_SIZE(req->num_items));
		/*
		 * If the size of the log mask cannot fit into our
		 * buffer, trim till we have space left in the buffer.
		 * num_items should then reflect the items that we have
		 * in our buffer.
		 */
		mask->num_items_tools = (req->num_items > MAX_ITEMS_ALLOWED) ?
					MAX_ITEMS_ALLOWED : req->num_items;
		mask_size = LOG_ITEMS_TO_SIZE(mask->num_items_tools);
		memset(mask->ptr, 0, mask->range_tools);
		if (mask_size > mask->range_tools) {
			DIAG_LOG(DIAG_DEBUG_MASKS,
				 "log range mismatch, e: %d old: %d new: %d\n",
				 req->equip_id, mask->range_tools,
				 LOG_ITEMS_TO_SIZE(mask->num_items_tools));
			/* Change in the mask reported by tools */
			temp_buf = krealloc(mask->ptr, mask_size, GFP_KERNEL);
			if (!temp_buf) {
				mask_info->status = DIAG_CTRL_MASK_INVALID;
				mutex_unlock(&mask->lock);
				break;
			}
			mask->ptr = temp_buf;
			memset(mask->ptr, 0, mask_size);
			mask->range_tools = mask_size;
		}
		req->num_items = mask->num_items_tools;
		if (mask_size > 0)
			memcpy(mask->ptr, src_buf + read_len, mask_size);
		DIAG_LOG(DIAG_DEBUG_MASKS,
			 "copying log mask, e %d num %d range %d size %d\n",
			 req->equip_id, mask->num_items_tools,
			 mask->range_tools, mask_size);
		mutex_unlock(&mask->lock);
		mask_info->status = DIAG_CTRL_MASK_VALID;
		break;
	}
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	if (diag_check_update(APPS_DATA))
		diag_update_userspace_clients(LOG_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	payload_len = LOG_ITEMS_TO_SIZE(req->num_items);
	if ((payload_len + rsp_header_len > dest_len) || (payload_len == 0)) {
		pr_err("diag: In %s, invalid length, payload_len: %d, header_len: %d, dest_len: %d\n",
		       __func__, payload_len, rsp_header_len, dest_len);
		status = LOG_STATUS_FAIL;
	}
	rsp.cmd_code = DIAG_CMD_LOG_CONFIG;
	rsp.padding[0] = 0;
	rsp.padding[1] = 0;
	rsp.padding[2] = 0;
	rsp.sub_cmd = DIAG_CMD_OP_SET_LOG_MASK;
	rsp.status = status;
	rsp.equip_id = req->equip_id;
	rsp.num_items = req->num_items;
	memcpy(dest_buf, &rsp, rsp_header_len);
	write_len += rsp_header_len;
	if (status != LOG_STATUS_SUCCESS)
		goto end;
	memcpy(dest_buf + write_len, src_buf + read_len, payload_len);
	write_len += payload_len;

	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA)
			continue;
		if (!diag_check_update(i))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		mutex_lock(&driver->md_session_lock);
		diag_send_log_mask_update(peripheral, req->equip_id);
		mutex_unlock(&driver->md_session_lock);
	}
end:
	return write_len;
}

static int diag_cmd_disable_log_mask(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid)
{
	struct diag_mask_info *mask_info = NULL;
	struct diag_log_mask_t *mask = NULL;
	struct diag_log_config_rsp_t header;
	int write_len = 0, i, peripheral;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &log_mask : info->log_mask;
	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0 ||
	    !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	mask = (struct diag_log_mask_t *)mask_info->ptr;
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		mutex_lock(&mask->lock);
		memset(mask->ptr, 0, mask->range);
		mutex_unlock(&mask->lock);
	}
	mask_info->status = DIAG_CTRL_MASK_ALL_DISABLED;
	mutex_unlock(&driver->md_session_lock);
	if (diag_check_update(APPS_DATA))
		diag_update_userspace_clients(LOG_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	header.cmd_code = DIAG_CMD_LOG_CONFIG;
	header.padding[0] = 0;
	header.padding[1] = 0;
	header.padding[2] = 0;
	header.sub_cmd = DIAG_CMD_OP_LOG_DISABLE;
	header.status = LOG_STATUS_SUCCESS;
	memcpy(dest_buf, &header, sizeof(struct diag_log_config_rsp_t));
	write_len += sizeof(struct diag_log_config_rsp_t);
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA)
			continue;
		if (!diag_check_update(i))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		mutex_lock(&driver->md_session_lock);
		diag_send_log_mask_update(peripheral, ALL_EQUIP_ID);
		mutex_unlock(&driver->md_session_lock);
	}

	return write_len;
}

int diag_create_msg_mask_table_entry(struct diag_msg_mask_t *msg_mask,
				     struct diag_ssid_range_t *range)
{
	if (!msg_mask || !range)
		return -EIO;
	if (range->ssid_last < range->ssid_first)
		return -EINVAL;
	msg_mask->ssid_first = range->ssid_first;
	msg_mask->ssid_last = range->ssid_last;
	msg_mask->ssid_last_tools = range->ssid_last;
	msg_mask->range = msg_mask->ssid_last - msg_mask->ssid_first + 1;
	if (msg_mask->range < MAX_SSID_PER_RANGE)
		msg_mask->range = MAX_SSID_PER_RANGE;
	msg_mask->range_tools = msg_mask->range;
	mutex_init(&msg_mask->lock);
	if (msg_mask->range > 0) {
		msg_mask->ptr = kcalloc(msg_mask->range, sizeof(uint32_t),
					GFP_KERNEL);
		if (!msg_mask->ptr)
			return -ENOMEM;
		kmemleak_not_leak(msg_mask->ptr);
	}
	return 0;
}

static int diag_create_msg_mask_table(void)
{
	int i, err = 0;
	struct diag_msg_mask_t *mask = (struct diag_msg_mask_t *)msg_mask.ptr;
	struct diag_ssid_range_t range;

	mutex_lock(&msg_mask.lock);
	mutex_lock(&driver->msg_mask_lock);
	driver->msg_mask_tbl_count = MSG_MASK_TBL_CNT;
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		range.ssid_first = msg_mask_tbl[i].ssid_first;
		range.ssid_last = msg_mask_tbl[i].ssid_last;
		err = diag_create_msg_mask_table_entry(mask, &range);
		if (err)
			break;
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&msg_mask.lock);
	return err;
}

static int diag_create_build_time_mask(void)
{
	int i, err = 0;
	const uint32_t *tbl = NULL;
	uint32_t tbl_size = 0;
	struct diag_msg_mask_t *build_mask = NULL;
	struct diag_ssid_range_t range;

	mutex_lock(&msg_bt_mask.lock);
	mutex_lock(&driver->msg_mask_lock);
	driver->bt_msg_mask_tbl_count = MSG_MASK_TBL_CNT;
	build_mask = (struct diag_msg_mask_t *)msg_bt_mask.ptr;
	for (i = 0; i < driver->bt_msg_mask_tbl_count; i++, build_mask++) {
		range.ssid_first = msg_mask_tbl[i].ssid_first;
		range.ssid_last = msg_mask_tbl[i].ssid_last;
		err = diag_create_msg_mask_table_entry(build_mask, &range);
		if (err)
			break;
		switch (build_mask->ssid_first) {
		case MSG_SSID_0:
			tbl = msg_bld_masks_0;
			tbl_size = sizeof(msg_bld_masks_0);
			break;
		case MSG_SSID_1:
			tbl = msg_bld_masks_1;
			tbl_size = sizeof(msg_bld_masks_1);
			break;
		case MSG_SSID_2:
			tbl = msg_bld_masks_2;
			tbl_size = sizeof(msg_bld_masks_2);
			break;
		case MSG_SSID_3:
			tbl = msg_bld_masks_3;
			tbl_size = sizeof(msg_bld_masks_3);
			break;
		case MSG_SSID_4:
			tbl = msg_bld_masks_4;
			tbl_size = sizeof(msg_bld_masks_4);
			break;
		case MSG_SSID_5:
			tbl = msg_bld_masks_5;
			tbl_size = sizeof(msg_bld_masks_5);
			break;
		case MSG_SSID_6:
			tbl = msg_bld_masks_6;
			tbl_size = sizeof(msg_bld_masks_6);
			break;
		case MSG_SSID_7:
			tbl = msg_bld_masks_7;
			tbl_size = sizeof(msg_bld_masks_7);
			break;
		case MSG_SSID_8:
			tbl = msg_bld_masks_8;
			tbl_size = sizeof(msg_bld_masks_8);
			break;
		case MSG_SSID_9:
			tbl = msg_bld_masks_9;
			tbl_size = sizeof(msg_bld_masks_9);
			break;
		case MSG_SSID_10:
			tbl = msg_bld_masks_10;
			tbl_size = sizeof(msg_bld_masks_10);
			break;
		case MSG_SSID_11:
			tbl = msg_bld_masks_11;
			tbl_size = sizeof(msg_bld_masks_11);
			break;
		case MSG_SSID_12:
			tbl = msg_bld_masks_12;
			tbl_size = sizeof(msg_bld_masks_12);
			break;
		case MSG_SSID_13:
			tbl = msg_bld_masks_13;
			tbl_size = sizeof(msg_bld_masks_13);
			break;
		case MSG_SSID_14:
			tbl = msg_bld_masks_14;
			tbl_size = sizeof(msg_bld_masks_14);
			break;
		case MSG_SSID_15:
			tbl = msg_bld_masks_15;
			tbl_size = sizeof(msg_bld_masks_15);
			break;
		case MSG_SSID_16:
			tbl = msg_bld_masks_16;
			tbl_size = sizeof(msg_bld_masks_16);
			break;
		case MSG_SSID_17:
			tbl = msg_bld_masks_17;
			tbl_size = sizeof(msg_bld_masks_17);
			break;
		case MSG_SSID_18:
			tbl = msg_bld_masks_18;
			tbl_size = sizeof(msg_bld_masks_18);
			break;
		case MSG_SSID_19:
			tbl = msg_bld_masks_19;
			tbl_size = sizeof(msg_bld_masks_19);
			break;
		case MSG_SSID_20:
			tbl = msg_bld_masks_20;
			tbl_size = sizeof(msg_bld_masks_20);
			break;
		case MSG_SSID_21:
			tbl = msg_bld_masks_21;
			tbl_size = sizeof(msg_bld_masks_21);
			break;
		case MSG_SSID_22:
			tbl = msg_bld_masks_22;
			tbl_size = sizeof(msg_bld_masks_22);
			break;
		}
		if (!tbl)
			continue;
		if (tbl_size > build_mask->range * sizeof(uint32_t)) {
			pr_warn("diag: In %s, table %d has more ssid than max, ssid_first: %d, ssid_last: %d\n",
				__func__, i, build_mask->ssid_first,
				build_mask->ssid_last);
			tbl_size = build_mask->range * sizeof(uint32_t);
		}
		memcpy(build_mask->ptr, tbl, tbl_size);
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&msg_bt_mask.lock);
	return err;
}

static int diag_create_log_mask_table(void)
{
	struct diag_log_mask_t *mask = NULL;
	uint8_t i;
	int err = 0;

	mutex_lock(&log_mask.lock);
	mask = (struct diag_log_mask_t *)(log_mask.ptr);
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		mask->equip_id = i;
		mask->num_items = LOG_GET_ITEM_NUM(log_code_last_tbl[i]);
		mask->num_items_tools = mask->num_items;
		mutex_init(&mask->lock);
		if (LOG_ITEMS_TO_SIZE(mask->num_items) > MAX_ITEMS_PER_EQUIP_ID)
			mask->range = LOG_ITEMS_TO_SIZE(mask->num_items);
		else
			mask->range = MAX_ITEMS_PER_EQUIP_ID;
		mask->range_tools = mask->range;
		mask->ptr = kzalloc(mask->range, GFP_KERNEL);
		if (!mask->ptr) {
			err = -ENOMEM;
			break;
		}
		kmemleak_not_leak(mask->ptr);
	}
	mutex_unlock(&log_mask.lock);
	return err;
}

static int __diag_mask_init(struct diag_mask_info *mask_info, int mask_len,
			    int update_buf_len)
{
	if (!mask_info || mask_len < 0 || update_buf_len < 0)
		return -EINVAL;

	mask_info->status = DIAG_CTRL_MASK_INVALID;
	mask_info->mask_len = mask_len;
	mask_info->update_buf_len = update_buf_len;
	if (mask_len > 0) {
		mask_info->ptr = kzalloc(mask_len, GFP_KERNEL);
		if (!mask_info->ptr)
			return -ENOMEM;
		kmemleak_not_leak(mask_info->ptr);
	}
	if (update_buf_len > 0) {
		mask_info->update_buf = kzalloc(update_buf_len, GFP_KERNEL);
		if (!mask_info->update_buf) {
			kfree(mask_info->ptr);
			return -ENOMEM;
		}
		kmemleak_not_leak(mask_info->update_buf);
	}
	mutex_init(&mask_info->lock);
	return 0;
}

static void __diag_mask_exit(struct diag_mask_info *mask_info)
{
	if (!mask_info || !mask_info->ptr)
		return;

	mutex_lock(&mask_info->lock);
	kfree(mask_info->ptr);
	mask_info->ptr = NULL;
	kfree(mask_info->update_buf);
	mask_info->update_buf = NULL;
	mutex_unlock(&mask_info->lock);
}

int diag_log_mask_copy(struct diag_mask_info *dest, struct diag_mask_info *src)
{
	int i, err = 0;
	struct diag_log_mask_t *src_mask = NULL;
	struct diag_log_mask_t *dest_mask = NULL;

	if (!src)
		return -EINVAL;

	err = __diag_mask_init(dest, LOG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;

	mutex_lock(&dest->lock);
	src_mask = (struct diag_log_mask_t *)(src->ptr);
	dest_mask = (struct diag_log_mask_t *)(dest->ptr);

	dest->mask_len = src->mask_len;
	dest->status = src->status;

	for (i = 0; i < MAX_EQUIP_ID; i++, src_mask++, dest_mask++) {
		dest_mask->equip_id = src_mask->equip_id;
		dest_mask->num_items = src_mask->num_items;
		dest_mask->num_items_tools = src_mask->num_items_tools;
		mutex_init(&dest_mask->lock);
		dest_mask->range = src_mask->range;
		dest_mask->range_tools = src_mask->range_tools;
		dest_mask->ptr = kzalloc(dest_mask->range_tools, GFP_KERNEL);
		if (!dest_mask->ptr) {
			err = -ENOMEM;
			break;
		}
		kmemleak_not_leak(dest_mask->ptr);
		memcpy(dest_mask->ptr, src_mask->ptr, dest_mask->range_tools);
	}
	mutex_unlock(&dest->lock);

	return err;
}

void diag_log_mask_free(struct diag_mask_info *mask_info)
{
	int i;
	struct diag_log_mask_t *mask = NULL;

	if (!mask_info || !mask_info->ptr)
		return;

	mutex_lock(&mask_info->lock);
	mask = (struct diag_log_mask_t *)mask_info->ptr;
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&mask_info->lock);
		return;
	}
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		kfree(mask->ptr);
		mask->ptr = NULL;
	}
	mutex_unlock(&mask_info->lock);

	__diag_mask_exit(mask_info);

}

static int diag_msg_mask_init(void)
{
	int err = 0, i;

	err = __diag_mask_init(&msg_mask, MSG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;
	err = diag_create_msg_mask_table();
	if (err) {
		pr_err("diag: Unable to create msg masks, err: %d\n", err);
		return err;
	}
	mutex_lock(&driver->msg_mask_lock);
	driver->msg_mask = &msg_mask;
	for (i = 0; i < NUM_PERIPHERALS; i++)
		driver->max_ssid_count[i] = 0;
	mutex_unlock(&driver->msg_mask_lock);

	return 0;
}

int diag_msg_mask_copy(struct diag_mask_info *dest, struct diag_mask_info *src)
{
	int i, err = 0, mask_size = 0;
	struct diag_msg_mask_t *src_mask = NULL;
	struct diag_msg_mask_t *dest_mask = NULL;
	struct diag_ssid_range_t range;

	if (!src || !dest)
		return -EINVAL;

	err = __diag_mask_init(dest, MSG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;
	mutex_lock(&dest->lock);
	mutex_lock(&driver->msg_mask_lock);
	src_mask = (struct diag_msg_mask_t *)src->ptr;
	dest_mask = (struct diag_msg_mask_t *)dest->ptr;

	dest->mask_len = src->mask_len;
	dest->status = src->status;
	for (i = 0; i < driver->msg_mask_tbl_count; i++) {
		range.ssid_first = src_mask->ssid_first;
		range.ssid_last = src_mask->ssid_last;
		err = diag_create_msg_mask_table_entry(dest_mask, &range);
		if (err)
			break;
		if (src_mask->range_tools < dest_mask->range)
			mask_size = src_mask->range_tools * sizeof(uint32_t);
		else
			mask_size = dest_mask->range * sizeof(uint32_t);
		memcpy(dest_mask->ptr, src_mask->ptr, mask_size);
		src_mask++;
		dest_mask++;
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&dest->lock);
	return err;
}

void diag_msg_mask_free(struct diag_mask_info *mask_info)
{
	int i;
	struct diag_msg_mask_t *mask = NULL;

	if (!mask_info || !mask_info->ptr)
		return;
	mutex_lock(&mask_info->lock);
	mutex_lock(&driver->msg_mask_lock);
	mask = (struct diag_msg_mask_t *)mask_info->ptr;
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&mask_info->lock);
		return;
	}
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		kfree(mask->ptr);
		mask->ptr = NULL;
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	__diag_mask_exit(mask_info);
}

static void diag_msg_mask_exit(void)
{
	int i;
	struct diag_msg_mask_t *mask = NULL;

	mutex_lock(&driver->msg_mask_lock);
	mask = (struct diag_msg_mask_t *)(msg_mask.ptr);
	if (mask) {
		for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++)
			kfree(mask->ptr);
		kfree(msg_mask.ptr);
		msg_mask.ptr = NULL;
	}
	kfree(msg_mask.update_buf);
	msg_mask.update_buf = NULL;
	mutex_unlock(&driver->msg_mask_lock);
}

static int diag_build_time_mask_init(void)
{
	int err = 0;

	/* There is no need for update buffer for Build Time masks */
	err = __diag_mask_init(&msg_bt_mask, MSG_MASK_SIZE, 0);
	if (err)
		return err;
	err = diag_create_build_time_mask();
	if (err) {
		pr_err("diag: Unable to create msg build time masks, err: %d\n",
		       err);
		return err;
	}
	driver->build_time_mask = &msg_bt_mask;
	return 0;
}

static void diag_build_time_mask_exit(void)
{
	int i;
	struct diag_msg_mask_t *mask = NULL;

	mutex_lock(&driver->msg_mask_lock);
	mask = (struct diag_msg_mask_t *)(msg_bt_mask.ptr);
	if (mask) {
		for (i = 0; i < driver->bt_msg_mask_tbl_count; i++, mask++)
			kfree(mask->ptr);
		kfree(msg_bt_mask.ptr);
		msg_bt_mask.ptr = NULL;
	}
	mutex_unlock(&driver->msg_mask_lock);
}

static int diag_log_mask_init(void)
{
	int err = 0, i;

	err = __diag_mask_init(&log_mask, LOG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;
	err = diag_create_log_mask_table();
	if (err)
		return err;
	driver->log_mask = &log_mask;

	for (i = 0; i < NUM_PERIPHERALS; i++)
		driver->num_equip_id[i] = 0;

	return 0;
}

static void diag_log_mask_exit(void)
{
	int i;
	struct diag_log_mask_t *mask = NULL;

	mask = (struct diag_log_mask_t *)(log_mask.ptr);
	if (mask) {
		for (i = 0; i < MAX_EQUIP_ID; i++, mask++)
			kfree(mask->ptr);
		kfree(log_mask.ptr);
	}

	kfree(log_mask.update_buf);
}

static int diag_event_mask_init(void)
{
	int err = 0, i;

	err = __diag_mask_init(&event_mask, EVENT_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;
	driver->event_mask_size = EVENT_MASK_SIZE;
	driver->last_event_id = APPS_EVENT_LAST_ID;
	driver->event_mask = &event_mask;

	for (i = 0; i < NUM_PERIPHERALS; i++)
		driver->num_event_id[i] = 0;

	return 0;
}

int diag_event_mask_copy(struct diag_mask_info *dest,
			 struct diag_mask_info *src)
{
	int err = 0;

	if (!src || !dest)
		return -EINVAL;

	err = __diag_mask_init(dest, EVENT_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;

	mutex_lock(&dest->lock);
	dest->mask_len = src->mask_len;
	dest->status = src->status;
	memcpy(dest->ptr, src->ptr, dest->mask_len);
	mutex_unlock(&dest->lock);

	return err;
}

void diag_event_mask_free(struct diag_mask_info *mask_info)
{
	if (!mask_info)
		return;

	__diag_mask_exit(mask_info);
}

static void diag_event_mask_exit(void)
{
	kfree(event_mask.ptr);
	kfree(event_mask.update_buf);
}

int diag_copy_to_user_msg_mask(char __user *buf, size_t count,
			       struct diag_md_session_t *info)
{
	int i, err = 0, len = 0;
	int copy_len = 0, total_len = 0;
	struct diag_msg_mask_userspace_t header;
	struct diag_mask_info *mask_info = NULL;
	struct diag_msg_mask_t *mask = NULL;
	unsigned char *ptr = NULL;

	if (!buf || count == 0)
		return -EINVAL;

	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!mask_info)
		return -EIO;

	if (!mask_info->ptr || !mask_info->update_buf) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK, mask_info->update_buf: %pK\n",
			__func__, mask_info->ptr, mask_info->update_buf);
		return -EINVAL;
	}
	mutex_lock(&driver->diag_maskclear_mutex);
	if (driver->mask_clear) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag:%s: count = %zu\n", __func__, count);
		mutex_unlock(&driver->diag_maskclear_mutex);
		return -EIO;
	}
	mutex_unlock(&driver->diag_maskclear_mutex);
	mutex_lock(&mask_info->lock);
	mutex_lock(&driver->msg_mask_lock);

	mask = (struct diag_msg_mask_t *)(mask_info->ptr);
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&mask_info->lock);
		return -EINVAL;
	}
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		ptr = mask_info->update_buf;
		len = 0;
		mutex_lock(&mask->lock);
		header.ssid_first = mask->ssid_first;
		header.ssid_last = mask->ssid_last_tools;
		header.range = mask->range_tools;
		memcpy(ptr, &header, sizeof(header));
		len += sizeof(header);
		copy_len = (sizeof(uint32_t) * mask->range_tools);
		if ((len + copy_len) > mask_info->update_buf_len) {
			pr_err("diag: In %s, no space to update msg mask, first: %d, last: %d\n",
			       __func__, mask->ssid_first,
			       mask->ssid_last_tools);
			mutex_unlock(&mask->lock);
			continue;
		}
		memcpy(ptr + len, mask->ptr, copy_len);
		len += copy_len;
		mutex_unlock(&mask->lock);
		/* + sizeof(int) to account for data_type already in buf */
		if (total_len + sizeof(int) + len > count) {
			pr_err("diag: In %s, unable to send msg masks to user space, total_len: %d, count: %zu\n",
			       __func__, total_len, count);
			err = -ENOMEM;
			break;
		}
		err = copy_to_user(buf + total_len, (void *)ptr, len);
		if (err) {
			pr_err("diag: In %s Unable to send msg masks to user space clients, err: %d\n",
			       __func__, err);
			break;
		}
		total_len += len;
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	return err ? err : total_len;
}

int diag_copy_to_user_log_mask(char __user *buf, size_t count,
			       struct diag_md_session_t *info)
{
	int i, err = 0, len = 0;
	int copy_len = 0, total_len = 0;
	struct diag_log_mask_userspace_t header;
	struct diag_log_mask_t *mask = NULL;
	struct diag_mask_info *mask_info = NULL;
	unsigned char *ptr = NULL;

	if (!buf || count == 0)
		return -EINVAL;

	mask_info = (!info) ? &log_mask : info->log_mask;
	if (!mask_info)
		return -EIO;

	if (!mask_info->ptr || !mask_info->update_buf) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK, mask_info->update_buf: %pK\n",
			__func__, mask_info->ptr, mask_info->update_buf);
		return -EINVAL;
	}

	mutex_lock(&mask_info->lock);
	mask = (struct diag_log_mask_t *)(mask_info->ptr);
	if (!mask->ptr) {
		pr_err("diag: Invalid input in %s, mask->ptr: %pK\n",
			__func__, mask->ptr);
		mutex_unlock(&mask_info->lock);
		return -EINVAL;
	}
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		ptr = mask_info->update_buf;
		len = 0;
		mutex_lock(&mask->lock);
		header.equip_id = mask->equip_id;
		header.num_items = mask->num_items_tools;
		memcpy(ptr, &header, sizeof(header));
		len += sizeof(header);
		copy_len = LOG_ITEMS_TO_SIZE(header.num_items);
		if ((len + copy_len) > mask_info->update_buf_len) {
			pr_err("diag: In %s, no space to update log mask, equip_id: %d\n",
			       __func__, mask->equip_id);
			mutex_unlock(&mask->lock);
			continue;
		}
		memcpy(ptr + len, mask->ptr, copy_len);
		len += copy_len;
		mutex_unlock(&mask->lock);
		/* + sizeof(int) to account for data_type already in buf */
		if (total_len + sizeof(int) + len > count) {
			pr_err("diag: In %s, unable to send log masks to user space, total_len: %d, count: %zu\n",
			       __func__, total_len, count);
			err = -ENOMEM;
			break;
		}
		err = copy_to_user(buf + total_len, (void *)ptr, len);
		if (err) {
			pr_err("diag: In %s Unable to send log masks to user space clients, err: %d\n",
			       __func__, err);
			break;
		}
		total_len += len;
	}
	mutex_unlock(&mask_info->lock);

	return err ? err : total_len;
}

void diag_send_updates_peripheral(uint8_t peripheral)
{
	if (!driver->feature[peripheral].sent_feature_mask)
		diag_send_feature_mask_update(peripheral);
	/*
	 * Masks (F3, logs and events) will be sent to
	 * peripheral immediately following feature mask update only
	 * if diag_id support is not present or
	 * diag_id support is present and diag_id has been sent to
	 * peripheral.
	 */
	if (!driver->feature[peripheral].diag_id_support ||
		driver->diag_id_sent[peripheral]) {
		if (driver->time_sync_enabled)
			diag_send_time_sync_update(peripheral);
		mutex_lock(&driver->md_session_lock);
		diag_send_msg_mask_update(peripheral, ALL_SSID, ALL_SSID);
		diag_send_log_mask_update(peripheral, ALL_EQUIP_ID);
		diag_send_event_mask_update(peripheral);
		mutex_unlock(&driver->md_session_lock);
		diag_send_real_time_update(peripheral,
				driver->real_time_mode[DIAG_LOCAL_PROC]);
		diag_send_peripheral_buffering_mode(
					&driver->buffering_mode[peripheral]);

		/*
		 * Clear mask_update variable afer updating
		 * logging masks to peripheral.
		 */
		mutex_lock(&driver->cntl_lock);
		driver->mask_update ^= PERIPHERAL_MASK(peripheral);
		mutex_unlock(&driver->cntl_lock);
	}
}

int diag_process_apps_masks(unsigned char *buf, int len, int pid)
{
	int size = 0, sub_cmd = 0;
	int (*hdlr)(unsigned char *src_buf, int src_len,
		    unsigned char *dest_buf, int dest_len, int pid) = NULL;

	if (!buf || len <= 0)
		return -EINVAL;

	if (*buf == DIAG_CMD_LOG_CONFIG) {
		sub_cmd = *(int *)(buf + sizeof(int));
		switch (sub_cmd) {
		case DIAG_CMD_OP_LOG_DISABLE:
			hdlr = diag_cmd_disable_log_mask;
			break;
		case DIAG_CMD_OP_GET_LOG_RANGE:
			hdlr = diag_cmd_get_log_range;
			break;
		case DIAG_CMD_OP_SET_LOG_MASK:
			hdlr = diag_cmd_set_log_mask;
			break;
		case DIAG_CMD_OP_GET_LOG_MASK:
			hdlr = diag_cmd_get_log_mask;
			break;
		}
	} else if (*buf == DIAG_CMD_MSG_CONFIG) {
		sub_cmd = *(uint8_t *)(buf + sizeof(uint8_t));
		switch (sub_cmd) {
		case DIAG_CMD_OP_GET_SSID_RANGE:
			hdlr = diag_cmd_get_ssid_range;
			break;
		case DIAG_CMD_OP_GET_BUILD_MASK:
			hdlr = diag_cmd_get_build_mask;
			break;
		case DIAG_CMD_OP_GET_MSG_MASK:
			hdlr = diag_cmd_get_msg_mask;
			break;
		case DIAG_CMD_OP_SET_MSG_MASK:
			hdlr = diag_cmd_set_msg_mask;
			break;
		case DIAG_CMD_OP_SET_ALL_MSG_MASK:
			hdlr = diag_cmd_set_all_msg_mask;
			break;
		}
	} else if (*buf == DIAG_CMD_GET_EVENT_MASK) {
		hdlr = diag_cmd_get_event_mask;
	} else if (*buf == DIAG_CMD_SET_EVENT_MASK) {
		hdlr = diag_cmd_update_event_mask;
	} else if (*buf == DIAG_CMD_EVENT_TOGGLE) {
		hdlr = diag_cmd_toggle_events;
	}

	if (hdlr)
		size = hdlr(buf, len, driver->apps_rsp_buf,
			    DIAG_MAX_RSP_SIZE, pid);

	return (size > 0) ? size : 0;
}

int diag_masks_init(void)
{
	int err = 0;

	err = diag_msg_mask_init();
	if (err)
		goto fail;

	err = diag_build_time_mask_init();
	if (err)
		goto fail;

	err = diag_log_mask_init();
	if (err)
		goto fail;

	err = diag_event_mask_init();
	if (err)
		goto fail;

	if (driver->buf_feature_mask_update == NULL) {
		driver->buf_feature_mask_update = kzalloc(sizeof(
					struct diag_ctrl_feature_mask) +
					FEATURE_MASK_LEN, GFP_KERNEL);
		if (driver->buf_feature_mask_update == NULL)
			goto fail;
		kmemleak_not_leak(driver->buf_feature_mask_update);
	}

	return 0;
fail:
	pr_err("diag: Could not initialize diag mask buffers\n");
	diag_masks_exit();
	return -ENOMEM;
}

void diag_masks_exit(void)
{
	diag_msg_mask_exit();
	diag_build_time_mask_exit();
	diag_log_mask_exit();
	diag_event_mask_exit();
	kfree(driver->buf_feature_mask_update);
}
