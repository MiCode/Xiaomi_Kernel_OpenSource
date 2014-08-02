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
#include <linux/uaccess.h>
#include "diagchar.h"
#include "diagfwd_cntl.h"
#include "diag_masks.h"

#define ALL_EQUIP_ID		100
#define ALL_SSID		-1

#define DIAG_SET_FEATURE_MASK(x) (feature_bytes[(x)/8] |= (1 << (x & 0x7)))

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
	{ .ssid_first = MSG_SSID_24, .ssid_last = MSG_SSID_24_LAST }
};

static int diag_apps_responds(void)
{
	/*
	 * Apps processor should respond to mask commands only if the
	 * Modem channel is up, the feature mask is received from Modem
	 * and if Modem supports Mask Centralization.
	 */
	if (chk_apps_only()) {
		if (driver->smd_data[MODEM_DATA].ch &&
			driver->rcvd_feature_mask[MODEM_DATA]) {
			if (driver->mask_centralization[MODEM_DATA])
				return 1;
			return 0;
		}
		return 1;
	}
	return 0;
}

static void diag_send_log_mask_update(struct diag_smd_info *smd_info,
				      int equip_id)
{
	int i;
	int err = 0;
	int send_once = 0;
	int header_len = sizeof(struct diag_ctrl_log_mask);
	uint8_t *buf = log_mask.update_buf;
	uint8_t *temp = NULL;
	uint32_t mask_size = 0;
	struct diag_ctrl_log_mask ctrl_pkt;
	struct diag_log_mask_t *mask = (struct diag_log_mask_t *)log_mask.ptr;

	if (!smd_info)
		return;

	if (!smd_info->ch) {
		pr_debug("diag: In %s, SMD channel is closed, peripheral: %d\n",
			 __func__, smd_info->peripheral);
		return;
	}

	switch (log_mask.status) {
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

	mutex_lock(&log_mask.lock);
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		if (equip_id != i && equip_id != ALL_EQUIP_ID)
			continue;

		ctrl_pkt.cmd_type = DIAG_CTRL_MSG_LOG_MASK;
		ctrl_pkt.stream_id = 1;
		ctrl_pkt.status = log_mask.status;
		if (log_mask.status == DIAG_CTRL_MASK_VALID) {
			mask_size = LOG_ITEMS_TO_SIZE(mask->num_items);
			ctrl_pkt.equip_id = i;
			ctrl_pkt.num_items = mask->num_items;
			ctrl_pkt.log_mask_size = mask_size;
		}
		ctrl_pkt.data_len = LOG_MASK_CTRL_HEADER_LEN + mask_size;

		if (header_len + mask_size > log_mask.update_buf_len) {
			temp = krealloc(buf, header_len + mask_size,
					GFP_KERNEL);
			if (!temp) {
				pr_err("diag: Unable to realloc log update buffer, new size: %d, equip_id: %d\n",
				       header_len + mask_size, equip_id);
				break;
			}
			log_mask.update_buf = temp;
			log_mask.update_buf_len = header_len + mask_size;
		}

		memcpy(buf, &ctrl_pkt, header_len);
		if (mask_size > 0)
			memcpy(buf + header_len, mask->ptr, mask_size);

		err = diag_smd_write(smd_info, buf, header_len + mask_size);
		if (err) {
			pr_err("diag: Unable to send log masks to peripheral %d, equip_id: %d, err: %d\n",
			       smd_info->peripheral, i, err);
		}
		if (send_once || equip_id != ALL_EQUIP_ID)
			break;

	}
	mutex_unlock(&log_mask.lock);
}

static void diag_send_event_mask_update(struct diag_smd_info *smd_info)
{
	uint8_t *buf = event_mask.update_buf;
	uint8_t *temp = NULL;
	struct diag_ctrl_event_mask header;
	int num_bytes = EVENT_COUNT_TO_BYTES(driver->last_event_id);
	int write_len = 0;
	int err = 0;
	int temp_len = 0;

	if (num_bytes <= 0 || num_bytes > driver->event_mask_size) {
		pr_debug("diag: In %s, invalid event mask length %d\n",
			 __func__, num_bytes);
		return;
	}

	if (!smd_info)
		return;

	if (!smd_info->ch) {
		pr_debug("diag: In %s, SMD channel is closed, peripheral: %d\n",
			 __func__, smd_info->peripheral);
		return;
	}

	mutex_lock(&event_mask.lock);
	header.cmd_type = DIAG_CTRL_MSG_EVENT_MASK;
	header.stream_id = 1;
	header.status = event_mask.status;

	switch (event_mask.status) {
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
		if (num_bytes + sizeof(header) > event_mask.update_buf_len) {
			temp_len = num_bytes + sizeof(header);
			temp = krealloc(buf, temp_len, GFP_KERNEL);
			if (!temp) {
				pr_err("diag: Unable to realloc event mask update buffer\n");
				goto err;
			} else {
				event_mask.update_buf = temp;
				event_mask.update_buf_len = temp_len;
			}
		}
		memcpy(buf + sizeof(header), event_mask.ptr, num_bytes);
		write_len += num_bytes;
		break;
	default:
		pr_debug("diag: In %s, invalid status %d\n", __func__,
			 event_mask.status);
		goto err;
	}
	header.data_len = EVENT_MASK_CTRL_HEADER_LEN + header.event_mask_size;
	memcpy(buf, &header, sizeof(header));
	write_len += sizeof(header);

	err = diag_smd_write(smd_info, buf, write_len);
	if (err) {
		pr_err("diag: Unable to send event masks to peripheral %d\n",
		       smd_info->peripheral);
	}
err:
	mutex_unlock(&event_mask.lock);
}

static void diag_send_msg_mask_update(struct diag_smd_info *smd_info,
				      int first, int last)
{
	int i;
	int err = 0;
	int header_len = sizeof(struct diag_ctrl_msg_mask);
	int temp_len = 0;
	uint8_t *buf = msg_mask.update_buf;
	uint8_t *temp = NULL;
	uint32_t mask_size = 0;
	struct diag_msg_mask_t *mask = (struct diag_msg_mask_t *)msg_mask.ptr;
	struct diag_ctrl_msg_mask header;

	if (!smd_info)
		return;

	if (!smd_info->ch) {
		pr_debug("diag: In %s, SMD channel is closed, peripheral: %d\n",
			 __func__, smd_info->peripheral);
		return;
	}

	mutex_lock(&msg_mask.lock);
	switch (msg_mask.status) {
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
			 msg_mask.status);
		goto err;
	}

	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		if (((first < mask->ssid_first) || (last > mask->ssid_last)) &&
							first != ALL_SSID) {
			continue;
		}

		if (msg_mask.status == DIAG_CTRL_MASK_VALID) {
			mask_size = mask->ssid_last - mask->ssid_first + 1;
			temp_len = mask_size * sizeof(uint32_t);
			if (temp_len + header_len <= msg_mask.update_buf_len)
				goto proceed;
			temp = krealloc(msg_mask.update_buf, temp_len,
					GFP_KERNEL);
			if (!temp) {
				pr_err("diag: In %s, unable to realloc msg_mask update buffer\n",
				       __func__);
				mask_size = (msg_mask.update_buf_len -
					    header_len) / sizeof(uint32_t);
			} else {
				msg_mask.update_buf = temp;
				msg_mask.update_buf_len = temp_len;
				pr_debug("diag: In %s, successfully reallocated msg_mask update buffer to len: %d\n",
					 __func__, msg_mask.update_buf_len);
			}
		}
proceed:
		header.cmd_type = DIAG_CTRL_MSG_F3_MASK;
		header.status = msg_mask.status;
		header.stream_id = 1;
		header.msg_mode = 0;
		header.ssid_first = mask->ssid_first;
		header.ssid_last = mask->ssid_last;
		header.msg_mask_size = mask_size;
		mask_size *= sizeof(uint32_t);
		header.data_len = MSG_MASK_CTRL_HEADER_LEN + mask_size;
		memcpy(buf, &header, header_len);
		if (mask_size > 0)
			memcpy(buf + header_len, mask->ptr, mask_size);

		err = diag_smd_write(smd_info, buf, header_len + mask_size);
		if (err) {
			pr_err("diag: Unable to send msg masks to peripheral %d\n",
			       smd_info->peripheral);
		}

		if (first != ALL_SSID)
			break;
	}
err:
	mutex_unlock(&msg_mask.lock);
}

static void diag_send_feature_mask_update(struct diag_smd_info *smd_info)
{
	void *buf = driver->buf_feature_mask_update;
	int header_size = sizeof(struct diag_ctrl_feature_mask);
	uint8_t feature_bytes[FEATURE_MASK_LEN] = {0, 0};
	struct diag_ctrl_feature_mask feature_mask;
	int total_len = 0;
	int err = 0;

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
	feature_mask.ctrl_pkt_id = DIAG_CTRL_MSG_FEATURE;
	feature_mask.ctrl_pkt_data_len = sizeof(uint32_t) + FEATURE_MASK_LEN;
	feature_mask.feature_mask_len = FEATURE_MASK_LEN;
	memcpy(buf, &feature_mask, header_size);
	DIAG_SET_FEATURE_MASK(F_DIAG_FEATURE_MASK_SUPPORT);
	DIAG_SET_FEATURE_MASK(F_DIAG_LOG_ON_DEMAND_APPS);
	DIAG_SET_FEATURE_MASK(F_DIAG_STM);
	if (driver->supports_separate_cmdrsp)
		DIAG_SET_FEATURE_MASK(F_DIAG_REQ_RSP_SUPPORT);
	if (driver->supports_apps_hdlc_encoding)
		DIAG_SET_FEATURE_MASK(F_DIAG_APPS_HDLC_ENCODE);
	DIAG_SET_FEATURE_MASK(F_DIAG_MASK_CENTRALIZATION);
	memcpy(buf + header_size, &feature_bytes, FEATURE_MASK_LEN);
	total_len = header_size + FEATURE_MASK_LEN;

	err = diag_smd_write(smd_info, buf, total_len);
	if (err) {
		pr_err("diag: In %s, unable to write to smd, peripheral: %d, type: %d, len: %d, err: %d\n",
		       __func__, smd_info->peripheral, smd_info->type,
		       total_len, err);
	}
	mutex_unlock(&driver->diag_cntl_mutex);
}

static int diag_cmd_get_ssid_range(unsigned char *src_buf, int src_len,
				   unsigned char *dest_buf, int dest_len)
{
	int i;
	int write_len = 0;
	struct diag_msg_mask_t *mask_ptr = NULL;
	struct diag_msg_ssid_query_t rsp;
	struct diag_ssid_range_t ssid_range;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	if (!diag_apps_responds())
		return 0;

	rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
	rsp.sub_cmd = DIAG_CMD_OP_GET_SSID_RANGE;
	rsp.status = MSG_STATUS_SUCCESS;
	rsp.padding = 0;
	rsp.count = driver->msg_mask_tbl_count;
	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len += sizeof(rsp);

	mask_ptr = (struct diag_msg_mask_t *)msg_mask.ptr;
	for (i = 0; i <  driver->msg_mask_tbl_count; i++, mask_ptr++) {
		if (write_len + sizeof(ssid_range) > dest_len) {
			pr_err("diag: In %s, Truncating response due to size limitations of rsp buffer\n",
			       __func__);
			break;
		}
		ssid_range.ssid_first = mask_ptr->ssid_first;
		ssid_range.ssid_last = mask_ptr->ssid_last;
		memcpy(dest_buf + write_len, &ssid_range, sizeof(ssid_range));
		write_len += sizeof(ssid_range);
	}

	return write_len;
}

static int diag_cmd_get_build_mask(unsigned char *src_buf, int src_len,
				   unsigned char *dest_buf, int dest_len)
{
	int i = 0;
	int write_len = 0;
	int num_entries = 0;
	int copy_len = 0;
	struct diag_msg_mask_t *build_mask = NULL;
	struct diag_build_mask_req_t *req = NULL;
	struct diag_msg_build_mask_t rsp;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	if (!diag_apps_responds())
		return 0;

	req = (struct diag_build_mask_req_t *)src_buf;
	rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
	rsp.sub_cmd = DIAG_CMD_OP_GET_BUILD_MASK;
	rsp.ssid_first = req->ssid_first;
	rsp.ssid_last = req->ssid_last;
	rsp.status = MSG_STATUS_FAIL;
	rsp.padding = 0;

	build_mask = (struct diag_msg_mask_t *)msg_bt_mask.ptr;
	for (i = 0; i < driver->msg_mask_tbl_count; i++, build_mask++) {
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

	return write_len;
}

static int diag_cmd_get_msg_mask(unsigned char *src_buf, int src_len,
				 unsigned char *dest_buf, int dest_len)
{
	int i;
	int write_len = 0;
	uint32_t mask_size = 0;
	struct diag_msg_mask_t *mask = NULL;
	struct diag_build_mask_req_t *req = NULL;
	struct diag_msg_build_mask_t rsp;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	if (!diag_apps_responds())
		return 0;

	req = (struct diag_build_mask_req_t *)src_buf;
	rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
	rsp.sub_cmd = DIAG_CMD_OP_GET_MSG_MASK;
	rsp.ssid_first = req->ssid_first;
	rsp.ssid_last = req->ssid_last;
	rsp.status = MSG_STATUS_FAIL;
	rsp.padding = 0;

	mask = (struct diag_msg_mask_t *)msg_mask.ptr;
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		if ((req->ssid_first < mask->ssid_first) ||
		    (req->ssid_first > mask->ssid_last)) {
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

	return write_len;
}

static int diag_cmd_set_msg_mask(unsigned char *src_buf, int src_len,
				 unsigned char *dest_buf, int dest_len)
{
	int i;
	int write_len = 0;
	int header_len = sizeof(struct diag_msg_build_mask_t);
	int found = 0;
	uint32_t mask_size = 0;
	uint32_t offset = 0;
	struct diag_msg_mask_t *mask = NULL;
	struct diag_msg_build_mask_t *req = NULL;
	struct diag_msg_build_mask_t rsp;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	req = (struct diag_msg_build_mask_t *)src_buf;

	mutex_lock(&msg_mask.lock);
	mask = (struct diag_msg_mask_t *)msg_mask.ptr;
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		if ((req->ssid_first < mask->ssid_first) ||
		    (req->ssid_first > mask->ssid_last)) {
			continue;
		}
		found = 1;
		if (req->ssid_last > mask->ssid_last) {
			pr_debug("diag: Msg SSID range mismatch\n");
			mask->ssid_last = req->ssid_last;
		}
		mask_size = req->ssid_last - req->ssid_first + 1;
		if (mask_size > mask->range) {
			pr_warn("diag: In %s, truncating ssid range, %d-%d to max allowed: %d\n",
				__func__, mask->ssid_first, mask->ssid_last,
				mask->range);
			mask_size = mask->range;
			mask->ssid_last = mask->ssid_first + mask->range;
		}
		offset = req->ssid_first - mask->ssid_first;
		if (offset + mask_size > mask->range) {
			pr_err("diag: In %s, Not enough space for msg mask, mask_size: %d\n",
			       __func__, mask_size);
			break;
		}
		mask_size = mask_size * sizeof(uint32_t);
		memcpy(mask->ptr + offset, src_buf + header_len, mask_size);
		msg_mask.status = DIAG_CTRL_MASK_VALID;
		break;
	}
	mutex_unlock(&msg_mask.lock);

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
	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
		diag_send_msg_mask_update(&driver->smd_cntl[i],
					  req->ssid_first, req->ssid_last);
	}
end:
	return write_len;
}

static int diag_cmd_set_all_msg_mask(unsigned char *src_buf, int src_len,
				     unsigned char *dest_buf, int dest_len)
{
	int i;
	int write_len = 0;
	int header_len = sizeof(struct diag_msg_config_rsp_t);
	struct diag_msg_config_rsp_t rsp;
	struct diag_msg_config_rsp_t *req = NULL;
	struct diag_msg_mask_t *mask = (struct diag_msg_mask_t *)msg_mask.ptr;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	req = (struct diag_msg_config_rsp_t *)src_buf;

	mutex_lock(&msg_mask.lock);
	msg_mask.status = (req->rt_mask) ? DIAG_CTRL_MASK_ALL_ENABLED :
					   DIAG_CTRL_MASK_ALL_DISABLED;
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		memset(mask->ptr, req->rt_mask,
		       mask->range * sizeof(uint32_t));
	}
	mutex_unlock(&msg_mask.lock);

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

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
		diag_send_msg_mask_update(&driver->smd_cntl[i], ALL_SSID,
					  ALL_SSID);
	}

	return write_len;
}

static int diag_cmd_get_event_mask(unsigned char *src_buf, int src_len,
				   unsigned char *dest_buf, int dest_len)
{
	int write_len = 0;
	uint32_t mask_size;
	struct diag_event_mask_config_t rsp;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
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
				      unsigned char *dest_buf, int dest_len)
{
	int i;
	int write_len = 0;
	int mask_len = 0;
	int header_len = sizeof(struct diag_event_mask_config_t);
	struct diag_event_mask_config_t rsp;
	struct diag_event_mask_config_t *req;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	req = (struct diag_event_mask_config_t *)src_buf;
	mask_len = EVENT_COUNT_TO_BYTES(req->num_bits);
	if (mask_len <= 0 || mask_len > event_mask.mask_len) {
		pr_err("diag: In %s, invalid event mask len: %d\n", __func__,
		       mask_len);
		return -EIO;
	}

	mutex_lock(&event_mask.lock);
	memcpy(event_mask.ptr, src_buf + header_len, mask_len);
	event_mask.status = DIAG_CTRL_MASK_VALID;
	mutex_unlock(&event_mask.lock);
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
	memcpy(dest_buf + write_len, event_mask.ptr, mask_len);
	write_len += mask_len;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_send_event_mask_update(&driver->smd_cntl[i]);

	return write_len;
}

static int diag_cmd_toggle_events(unsigned char *src_buf, int src_len,
				  unsigned char *dest_buf, int dest_len)
{
	int i;
	int write_len = 0;
	uint8_t toggle = 0;
	struct diag_event_report_t header;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	toggle = *(src_buf + 1);
	mutex_lock(&event_mask.lock);
	if (toggle) {
		event_mask.status = DIAG_CTRL_MASK_ALL_ENABLED;
		memset(event_mask.ptr, 0xFF, event_mask.mask_len);
	} else {
		event_mask.status = DIAG_CTRL_MASK_ALL_DISABLED;
		memset(event_mask.ptr, 0, event_mask.mask_len);
	}
	mutex_unlock(&event_mask.lock);
	diag_update_userspace_clients(EVENT_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	header.cmd_code = DIAG_CMD_EVENT_TOGGLE;
	header.padding = 0;
	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_send_event_mask_update(&driver->smd_cntl[i]);
	memcpy(dest_buf, &header, sizeof(header));
	write_len += sizeof(header);

	return write_len;
}

static int diag_cmd_get_log_mask(unsigned char *src_buf, int src_len,
				 unsigned char *dest_buf, int dest_len)
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

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	if (!diag_apps_responds())
		return 0;

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

	log_item = (struct diag_log_mask_t *)log_mask.ptr;
	for (i = 0; i < MAX_EQUIP_ID; i++, log_item++) {
		if (log_item->equip_id != req->equip_id)
			continue;
		mask_size = LOG_ITEMS_TO_SIZE(log_item->num_items);
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
			break;
		}
		*(uint32_t *)(dest_buf + write_len) = log_item->equip_id;
		write_len += sizeof(uint32_t);
		*(uint32_t *)(dest_buf + write_len) = log_item->num_items;
		write_len += sizeof(uint32_t);
		if (mask_size > 0) {
			memcpy(dest_buf + write_len, log_item->ptr, mask_size);
			write_len += mask_size;
		}
		status = LOG_STATUS_SUCCESS;
		break;
	}

	rsp.status = status;
	memcpy(dest_buf, &rsp, rsp_header_len);

	return write_len;
}

static int diag_cmd_get_log_range(unsigned char *src_buf, int src_len,
				  unsigned char *dest_buf, int dest_len)
{
	int i;
	int write_len = 0;
	struct diag_log_config_rsp_t rsp;
	struct diag_log_mask_t *mask = (struct diag_log_mask_t *)log_mask.ptr;

	if (!diag_apps_responds())
		return 0;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
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
		*(uint32_t *)(dest_buf + write_len) = mask->num_items;
		write_len += sizeof(uint32_t);
	}

	return write_len;
}

static int diag_cmd_set_log_mask(unsigned char *src_buf, int src_len,
				 unsigned char *dest_buf, int dest_len)
{
	int i;
	int write_len = 0;
	int status = LOG_STATUS_SUCCESS;
	int read_len = 0;
	int payload_len = 0;
	int req_header_len = sizeof(struct diag_log_config_req_t);
	int rsp_header_len = sizeof(struct diag_log_config_set_rsp_t);
	uint32_t mask_size = 0;
	struct diag_log_mask_t *mask = (struct diag_log_mask_t *)log_mask.ptr;
	struct diag_log_config_req_t *req;
	struct diag_log_config_set_rsp_t rsp;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	req = (struct diag_log_config_req_t *)src_buf;
	read_len += req_header_len;

	if (req->equip_id >= MAX_EQUIP_ID) {
		pr_err("diag: In %s, Invalid logging mask request, equip_id: %d\n",
		       __func__, req->equip_id);
		status = LOG_STATUS_INVALID;
	}
	mutex_lock(&log_mask.lock);
	for (i = 0; i < MAX_EQUIP_ID && !status; i++, mask++) {
		if (mask->equip_id != req->equip_id)
			continue;
		if (mask->num_items < req->num_items)
			mask->num_items = req->num_items;
		mask_size = LOG_ITEMS_TO_SIZE(req->num_items);
		if (mask_size > mask->range) {
			/*
			 * If the size of the log mask cannot fit into our
			 * buffer, trim till we have space left in the buffer.
			 * num_items should then reflect the items that we have
			 * in our buffer.
			 */
			mask_size = mask->range;
			mask->num_items = LOG_SIZE_TO_ITEMS(mask_size);
			req->num_items = mask->num_items;
		}
		if (mask_size > 0)
			memcpy(mask->ptr, src_buf + read_len, mask_size);
		log_mask.status = DIAG_CTRL_MASK_VALID;
		break;
	}
	mutex_unlock(&log_mask.lock);
	diag_update_userspace_clients(LOG_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	payload_len = LOG_ITEMS_TO_SIZE(req->num_items);
	if (payload_len + rsp_header_len > dest_len) {
		pr_err("diag: In %s, invalid length, payload_len: %d, header_len: %d, dest_len: %d\n",
		       __func__, payload_len, rsp_header_len , dest_len);
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

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_send_log_mask_update(&driver->smd_cntl[i], req->equip_id);
end:
	return write_len;
}

static int diag_cmd_disable_log_mask(unsigned char *src_buf, int src_len,
				     unsigned char *dest_buf, int dest_len)
{
	struct diag_log_mask_t *mask = (struct diag_log_mask_t *)log_mask.ptr;
	struct diag_log_config_rsp_t header;
	int write_len = 0;
	int i;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %p, src_len: %d, dest_buf: %p, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	mutex_lock(&log_mask.lock);
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++)
		memset(mask->ptr, 0, mask->range);
	log_mask.status = DIAG_CTRL_MASK_ALL_DISABLED;
	mutex_unlock(&log_mask.lock);
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
	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_send_log_mask_update(&driver->smd_cntl[i], ALL_EQUIP_ID);

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
	msg_mask->range = msg_mask->ssid_last - msg_mask->ssid_first + 1;
	if (msg_mask->range < MAX_SSID_PER_RANGE)
		msg_mask->range = MAX_SSID_PER_RANGE;
	if (msg_mask->range > 0) {
		msg_mask->ptr = kzalloc(msg_mask->range * sizeof(uint32_t),
					GFP_KERNEL);
		if (!msg_mask->ptr)
			return -ENOMEM;
	}
	return 0;
}

static int diag_create_msg_mask_table(void)
{
	int i;
	int err = 0;
	struct diag_msg_mask_t *mask = (struct diag_msg_mask_t *)msg_mask.ptr;
	struct diag_ssid_range_t range;

	mutex_lock(&msg_mask.lock);
	driver->msg_mask_tbl_count = MSG_MASK_TBL_CNT;
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		range.ssid_first = msg_mask_tbl[i].ssid_first;
		range.ssid_last = msg_mask_tbl[i].ssid_last;
		err = diag_create_msg_mask_table_entry(mask, &range);
		if (err)
			break;
	}
	mutex_unlock(&msg_mask.lock);
	return err;
}

static int diag_create_build_time_mask(void)
{
	int i;
	int err = 0;
	const uint32_t *tbl = NULL;
	uint32_t tbl_size = 0;
	struct diag_msg_mask_t *build_mask = NULL;
	struct diag_ssid_range_t range;

	mutex_lock(&msg_bt_mask.lock);
	build_mask = (struct diag_msg_mask_t *)msg_bt_mask.ptr;
	for (i = 0; i < driver->msg_mask_tbl_count; i++, build_mask++) {
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
		if (LOG_ITEMS_TO_SIZE(mask->num_items) > MAX_ITEMS_PER_EQUIP_ID)
			mask->range = LOG_ITEMS_TO_SIZE(mask->num_items);
		else
			mask->range = MAX_ITEMS_PER_EQUIP_ID;
		mask->ptr = kzalloc(mask->range, GFP_KERNEL);
		if (!mask->ptr) {
			err = -ENOMEM;
			break;
		}
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

static int diag_msg_mask_init(void)
{
	int err = 0;
	int i;

	err = __diag_mask_init(&msg_mask, MSG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;
	err = diag_create_msg_mask_table();
	if (err) {
		pr_err("diag: Unable to create msg masks, err: %d\n", err);
		return err;
	}
	driver->msg_mask = &msg_mask;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		driver->max_ssid_count[i] = 0;

	return 0;
}

static void diag_msg_mask_exit(void)
{
	int i;
	struct diag_msg_mask_t *mask = NULL;

	mask = (struct diag_msg_mask_t *)(msg_mask.ptr);
	if (mask) {
		for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++)
			kfree(mask->ptr);
		kfree(msg_mask.ptr);
	}

	kfree(msg_mask.update_buf);
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

	mask = (struct diag_msg_mask_t *)(msg_bt_mask.ptr);
	if (mask) {
		for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++)
			kfree(mask->ptr);
		kfree(msg_mask.ptr);
	}
}

static int diag_log_mask_init(void)
{
	int err = 0;
	int i;

	err = __diag_mask_init(&log_mask, LOG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;
	err = diag_create_log_mask_table();
	if (err)
		return err;
	driver->log_mask = &log_mask;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
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
	int err = 0;
	int i;

	err = __diag_mask_init(&event_mask, EVENT_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;
	driver->event_mask_size = EVENT_MASK_SIZE;
	driver->last_event_id = APPS_EVENT_LAST_ID;
	driver->event_mask = &event_mask;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		driver->num_event_id[i] = 0;

	return 0;
}

static void diag_event_mask_exit(void)
{
	kfree(event_mask.ptr);
	kfree(event_mask.update_buf);
}

int diag_copy_to_user_msg_mask(char __user *buf, size_t count)
{
	int i;
	int err = 0;
	int len = 0;
	int copy_len = 0;
	int total_len = 0;
	struct diag_msg_mask_userspace_t header;
	struct diag_msg_mask_t *mask = NULL;
	unsigned char *ptr = NULL;

	if (!buf || count == 0)
		return -EINVAL;

	mutex_lock(&msg_mask.lock);
	mask = (struct diag_msg_mask_t *)(msg_mask.ptr);
	for (i = 0; i < driver->msg_mask_tbl_count; i++, mask++) {
		ptr = msg_mask.update_buf;
		len = 0;
		header.ssid_first = mask->ssid_first;
		header.ssid_last = mask->ssid_last;
		header.range = mask->range;
		memcpy(ptr, &header, sizeof(header));
		len += sizeof(header);
		copy_len = (sizeof(uint32_t) * mask->range);
		if ((len + copy_len) > msg_mask.update_buf_len) {
			pr_err("diag: In %s, no space to update msg mask, first: %d, last: %d\n",
			       __func__, mask->ssid_first, mask->ssid_last);
			continue;
		}
		memcpy(ptr + len, mask->ptr, copy_len);
		len += copy_len;
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
	mutex_unlock(&msg_mask.lock);

	return err ? err : total_len;
}

int diag_copy_to_user_log_mask(char __user *buf, size_t count)
{
	int i;
	int err = 0;
	int len = 0;
	int copy_len = 0;
	int total_len = 0;
	struct diag_log_mask_userspace_t header;
	struct diag_log_mask_t *mask = NULL;
	unsigned char *ptr = NULL;

	if (!buf || count == 0)
		return -EINVAL;

	mutex_lock(&log_mask.lock);
	mask = (struct diag_log_mask_t *)(log_mask.ptr);
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		ptr = log_mask.update_buf;
		len = 0;
		header.equip_id = mask->equip_id;
		header.num_items = mask->num_items;
		memcpy(ptr, &header, sizeof(header));
		len += sizeof(header);
		copy_len = LOG_ITEMS_TO_SIZE(header.num_items);
		if ((len + copy_len) > log_mask.update_buf_len) {
			pr_err("diag: In %s, no space to update log mask, equip_id: %d\n",
			       __func__, mask->equip_id);
			continue;
		}
		memcpy(ptr + len, mask->ptr, copy_len);
		len += copy_len;
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
	mutex_unlock(&log_mask.lock);

	return err ? err : total_len;
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
	diag_send_msg_mask_update(smd_info, ALL_SSID, ALL_SSID);
	diag_send_log_mask_update(smd_info, ALL_EQUIP_ID);
	diag_send_event_mask_update(smd_info);

	if (smd_info->notify_context == SMD_EVENT_OPEN)
		diag_send_diag_mode_update_by_smd(smd_info,
				driver->real_time_mode[DIAG_LOCAL_PROC]);

	smd_info->notify_context = 0;
}

int diag_process_apps_masks(unsigned char *buf, int len)
{
	int size = 0;
	int sub_cmd = 0;
	int (*hdlr)(unsigned char *src_buf, int src_len,
		    unsigned char *dest_buf, int dest_len) = NULL;

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
		size = hdlr(buf, len, driver->apps_rsp_buf, APPS_BUF_SIZE);

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
