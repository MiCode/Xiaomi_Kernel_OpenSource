// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2008-2019, The Linux Foundation. All rights reserved.
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

#define MAX_USERSPACE_BUF_SIZ	100000
struct diag_mask_info msg_mask;
struct diag_mask_info msg_bt_mask;
struct diag_mask_info log_mask;
struct diag_mask_info event_mask;

static int diag_subid_info[MAX_SIM_NUM] = {[0 ... (MAX_SIM_NUM - 1)] =
	INVALID_INDEX};

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

static int diag_save_user_msg_mask(struct diag_md_session_t *info);
static int diag_save_user_log_mask(struct diag_md_session_t *info);

static int __diag_multisim_mask_init(struct diag_mask_info *mask_info,
	int mask_len, int subid_index);

/*
 * diag_get_ms_ptr_index(struct diag_multisim_masks *ms_ptr, int subid_index)
 *
 * Input:
 * ms_ptr = Head pointer to multisim mask (mask_info->ms_ptr)
 * subid_index = Index of required subscription's mask
 *
 * Return:
 * Function will return multisim mask pointer corresponding to given
 * subid_index by iterating through the list
 * Function will return NULL if no multisim mask is present for given
 * subid_index or having invalid sub_ptr (ms_ptr->sub_ptr)
 *
 */

struct diag_multisim_masks
	*diag_get_ms_ptr_index(struct diag_multisim_masks *ms_ptr,
	int subid_index)
{
	struct diag_multisim_masks *temp = NULL;

	if (!ms_ptr)
		return NULL;

	temp = ms_ptr;
	while ((subid_index > 0) && temp && temp->next) {
		temp = temp->next;
		subid_index--;
	}
	if (subid_index == 0 && temp && temp->sub_ptr)
		return temp;
	else
		return NULL;
}

static int diag_check_update(int md_peripheral, int pid)
{
	int ret;
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);
	ret = (!info || (info &&
		(info->peripheral_mask[DIAG_LOCAL_PROC] &
		MD_PERIPHERAL_MASK(md_peripheral))));
	mutex_unlock(&driver->md_session_lock);

	return ret;
}

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

static void diag_send_log_mask_update(uint8_t peripheral,
		int equip_id, int sub_index, int preset_id)
{
	int err = 0, send_once = 0, i;
	int header_len = sizeof(struct diag_ctrl_log_mask);
	uint8_t *buf = NULL, *temp = NULL;
	uint8_t upd = 0, status, eq_id;
	uint32_t mask_size = 0, pd_mask = 0, num_items = 0;
	struct diag_ctrl_log_mask ctrl_pkt;
	struct diag_ctrl_log_mask_sub ctrl_pkt_sub;
	struct diag_mask_info *mask_info = NULL;
	struct diag_log_mask_t *mask = NULL;
	struct diagfwd_info *fwd_info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;
	int proc = DIAG_LOCAL_PROC;

	if (peripheral >= NUM_PERIPHERALS)
		return;

	if (!driver->diagfwd_cntl[peripheral] ||
	    !driver->diagfwd_cntl[peripheral]->ch_open) {
		pr_debug("diag: In %s, control channel is not open, p: %d\n",
			 __func__, peripheral);
		return;
	}

	MD_PERIPHERAL_PD_MASK(TYPE_CNTL, peripheral, pd_mask);

	if (driver->md_session_mask[proc] != 0) {
		if (driver->md_session_mask[proc] &
			MD_PERIPHERAL_MASK(peripheral)) {
			if (driver->md_session_map[proc][peripheral])
				mask_info =
			driver->md_session_map[proc][peripheral]->log_mask;
		} else if (driver->md_session_mask[proc] & pd_mask) {
			upd =
			diag_mask_to_pd_value(driver->md_session_mask[proc]);
			if (upd && driver->md_session_map[proc][upd])
				mask_info =
				driver->md_session_map[proc][upd]->log_mask;
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

	mutex_lock(&mask_info->lock);
	if (sub_index >= 0) {
		ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr, sub_index);
		if (!ms_ptr)
			goto err;
		mask = (struct diag_log_mask_t *)ms_ptr->sub_ptr;
		status = ms_ptr->status;
	} else {
		mask = (struct diag_log_mask_t *)mask_info->ptr;
		status = mask_info->status;
	}

	if (!mask || !mask->ptr)
		goto err;
	buf = mask_info->update_buf;

	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		if (!mask->ptr)
			continue;

		if (equip_id != i && equip_id != ALL_EQUIP_ID)
			continue;

		mutex_lock(&mask->lock);
		switch (status) {
		case DIAG_CTRL_MASK_ALL_DISABLED:
		case DIAG_CTRL_MASK_ALL_ENABLED:
			eq_id = 0;
			num_items = 0;
			mask_size = 0;
			send_once = 1;
			break;
		case DIAG_CTRL_MASK_VALID:
			mask_size = LOG_ITEMS_TO_SIZE(mask->num_items_tools);
			eq_id = i;
			num_items = mask->num_items_tools;
			break;
		default:
			pr_debug("diag: In %s, invalid log_mask status\n",
				__func__);
			mutex_unlock(&mask->lock);
			mutex_unlock(&mask_info->lock);
			return;
		}
		if (sub_index >= 0 && preset_id > 0)
			goto proceed_sub_pkt;

		ctrl_pkt.cmd_type = DIAG_CTRL_MSG_LOG_MASK;
		ctrl_pkt.stream_id = 1;
		ctrl_pkt.status = mask_info->status;
		ctrl_pkt.equip_id = eq_id;
		ctrl_pkt.num_items = num_items;
		ctrl_pkt.log_mask_size = mask_size;
		ctrl_pkt.data_len = LOG_MASK_CTRL_HEADER_LEN + mask_size;
		header_len = sizeof(struct diag_ctrl_msg_mask);
		goto send_cntrl_pkt;
proceed_sub_pkt:
		ctrl_pkt_sub.cmd_type = DIAG_CTRL_MSG_LOG_MS_MASK;
		ctrl_pkt_sub.version = 1;
		ctrl_pkt_sub.preset_id = preset_id;
		if (sub_index >= 0) {
			ctrl_pkt_sub.id_valid = 1;
			ctrl_pkt_sub.sub_id = diag_subid_info[sub_index];
		} else {
			ctrl_pkt_sub.id_valid = 0;
			ctrl_pkt_sub.sub_id = 0;
		}
		ctrl_pkt_sub.stream_id = 1;
		ctrl_pkt_sub.status = status;
		ctrl_pkt_sub.equip_id = eq_id;
		ctrl_pkt_sub.num_items = num_items;
		ctrl_pkt_sub.log_mask_size = mask_size;
		ctrl_pkt_sub.data_len = LOG_MASK_CTRL_HEADER_LEN_SUB +
			mask_size;
		header_len = sizeof(struct diag_ctrl_msg_mask_sub);
send_cntrl_pkt:
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
			buf = temp;
		}

		if (sub_index >= 0 && preset_id > 0)
			memcpy(buf, &ctrl_pkt_sub, sizeof(ctrl_pkt_sub));
		else
			memcpy(buf, &ctrl_pkt, sizeof(ctrl_pkt));

		if (mask_size > 0)
			memcpy(buf + header_len, mask->ptr, mask_size);
		mutex_unlock(&mask->lock);

		DIAG_LOG(DIAG_DEBUG_MASKS,
			 "sending ctrl pkt to %d, equip_id %d num_items %d size %d\n",
			 peripheral, eq_id, num_items,
			 mask_size);

		err = diagfwd_write(peripheral, TYPE_CNTL,
				    buf, header_len + mask_size);
		if (err && err != -ENODEV)
			pr_err_ratelimited("diag: Unable to send log masks to peripheral %d, equip_id: %d, err: %d\n",
			       peripheral, i, err);
		if (send_once || equip_id != ALL_EQUIP_ID)
			break;

	}
err:
	mutex_unlock(&mask_info->lock);
}

static void diag_send_event_mask_update(uint8_t peripheral, int sub_index,
		int preset_id)
{
	uint8_t *buf = NULL, *temp = NULL;
	uint8_t upd = 0;
	uint32_t pd_mask = 0, event_mask_size;
	uint8_t status, event_config;
	int num_bytes = EVENT_COUNT_TO_BYTES(driver->last_event_id);
	int header_len = 0;
	int write_len = 0, err = 0, i = 0, temp_len = 0;
	struct diag_ctrl_event_mask header;
	struct diag_ctrl_event_mask_sub header_sub;
	struct diag_mask_info *mask_info = NULL;
	struct diagfwd_info *fwd_info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;
	int proc = DIAG_LOCAL_PROC;

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

	if (driver->md_session_mask[proc] != 0) {
		if (driver->md_session_mask[proc] &
			MD_PERIPHERAL_MASK(peripheral)) {
			if (driver->md_session_map[proc][peripheral])
				mask_info =
			driver->md_session_map[proc][peripheral]->event_mask;
		} else if (driver->md_session_mask[proc] & pd_mask) {
			upd =
			diag_mask_to_pd_value(driver->md_session_mask[proc]);
			if (upd && driver->md_session_map[proc][upd])
				mask_info =
				driver->md_session_map[proc][upd]->event_mask;
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

	mutex_lock(&mask_info->lock);

	buf = mask_info->update_buf;
	if (sub_index >= 0) {
		ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr, sub_index);
		if (!ms_ptr)
			goto err;
		status = ms_ptr->status;
	} else {
		status = mask_info->status;
		if (!mask_info->ptr)
			goto err;
	}
	if (sub_index >= 0 && preset_id > 0)
		header_len = sizeof(header_sub);
	else
		header_len = sizeof(header);
	switch (status) {
	case DIAG_CTRL_MASK_ALL_DISABLED:
		event_config = 0;
		event_mask_size = 0;
		break;
	case DIAG_CTRL_MASK_ALL_ENABLED:
		event_config = 1;
		event_mask_size = 0;
		break;
	case DIAG_CTRL_MASK_VALID:
		event_config = 1;
		event_mask_size = num_bytes;
		if (num_bytes + header_len > mask_info->update_buf_len) {
			temp_len = num_bytes + header_len;
			temp = krealloc(buf, temp_len, GFP_KERNEL);
			if (!temp) {
				pr_err("diag: Unable to realloc event mask update buffer\n");
				goto err;
			} else {
				mask_info->update_buf = temp;
				mask_info->update_buf_len = temp_len;
				buf = temp;
			}
		}
		if (num_bytes > 0) {
			if (ms_ptr)
				memcpy(buf + header_len,
				ms_ptr->sub_ptr, num_bytes);
			else
				memcpy(buf + header_len,
				mask_info->ptr, num_bytes);
		} else {
			pr_err("diag: num_bytes(%d) is not satisfying length condition\n",
				num_bytes);
			goto err;
		}
		write_len += num_bytes;
		break;
	default:
		pr_debug("diag: In %s, invalid status %d\n", __func__,
			 status);
		goto err;
	}
	if (sub_index >= 0 && preset_id > 0)
		goto proceed_sub_pkt;

	header.cmd_type = DIAG_CTRL_MSG_EVENT_MASK;
	header.event_config = event_config;
	header.event_mask_size = event_mask_size;
	header.stream_id = 1;
	header.status = mask_info->status;
	header.data_len = EVENT_MASK_CTRL_HEADER_LEN + header.event_mask_size;
	memcpy(buf, &header, sizeof(header));
	write_len += sizeof(header);
	goto send_pkt;
proceed_sub_pkt:
	header_sub.cmd_type = DIAG_CTRL_MSG_EVENT_MS_MASK;
	header_sub.event_config = event_config;
	header_sub.event_mask_size = event_mask_size;
	header_sub.version = 1;
	header_sub.stream_id = 1;
	header_sub.preset_id = preset_id;
	header_sub.status = status;
	if (sub_index >= 0) {
		header_sub.id_valid = 1;
		header_sub.sub_id = diag_subid_info[sub_index];
	} else {
		header_sub.id_valid = 0;
		header_sub.sub_id = 0;
	}
	header_sub.data_len = EVENT_MASK_CTRL_HEADER_LEN_SUB +
		header_sub.event_mask_size;
	memcpy(buf, &header_sub, sizeof(header_sub));
	write_len += sizeof(header_sub);
send_pkt:
	err = diagfwd_write(peripheral, TYPE_CNTL, buf, write_len);
	if (err && err != -ENODEV)
		pr_err_ratelimited("diag: Unable to send event masks to peripheral %d\n",
		       peripheral);
err:
	mutex_unlock(&mask_info->lock);
}

static void diag_send_msg_mask_update(uint8_t peripheral, int first, int last,
	int sub_index, int preset)
{
	int i, err = 0, temp_len = 0, status = 0;
	int header_len = 0;
	uint8_t *buf = NULL, *temp = NULL;
	uint8_t upd = 0;
	uint8_t msg_mask_tbl_count_local = 0;
	uint32_t mask_size = 0, pd_mask = 0;
	struct diag_mask_info *mask_info = NULL;
	struct diag_msg_mask_t *mask = NULL;
	struct diag_ctrl_msg_mask header;
	struct diag_ctrl_msg_mask_sub header_sub;
	struct diagfwd_info *fwd_info = NULL;
	struct diag_md_session_t *md_session_info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;
	int proc = DIAG_LOCAL_PROC;

	if (peripheral >= NUM_PERIPHERALS)
		return;

	if (!driver->diagfwd_cntl[peripheral] ||
	    !driver->diagfwd_cntl[peripheral]->ch_open) {
		pr_debug("diag: In %s, control channel is not open, p: %d\n",
			 __func__, peripheral);
		return;
	}

	MD_PERIPHERAL_PD_MASK(TYPE_CNTL, peripheral, pd_mask);

	if (driver->md_session_mask[proc] != 0) {
		if (driver->md_session_mask[proc] &
			MD_PERIPHERAL_MASK(peripheral)) {
			if (driver->md_session_map[proc][peripheral]) {
				mask_info =
			driver->md_session_map[proc][peripheral]->msg_mask;
				md_session_info =
				driver->md_session_map[proc][peripheral];
			}
		} else if (driver->md_session_mask[proc] & pd_mask) {
			upd =
			diag_mask_to_pd_value(driver->md_session_mask[proc]);
			if (upd && driver->md_session_map[proc][upd]) {
				mask_info =
				driver->md_session_map[proc][upd]->msg_mask;
				md_session_info =
				driver->md_session_map[proc][upd];
			}
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
	if (sub_index >= 0 && preset > 0)
		header_len = sizeof(struct diag_ctrl_msg_mask_sub);
	else
		header_len = sizeof(struct diag_ctrl_msg_mask);
	mutex_lock(&driver->msg_mask_lock);
	if (md_session_info)
		msg_mask_tbl_count_local = md_session_info->msg_mask_tbl_count;
	else
		msg_mask_tbl_count_local = driver->msg_mask_tbl_count;
	mutex_unlock(&driver->msg_mask_lock);
	mutex_lock(&mask_info->lock);
	buf = mask_info->update_buf;
	if (sub_index >= 0) {
		ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr, sub_index);
		if (!ms_ptr)
			goto err;
		mask = (struct diag_msg_mask_t *)ms_ptr->sub_ptr;
		status = ms_ptr->status;
	} else {
		mask = (struct diag_msg_mask_t *)mask_info->ptr;
		status = mask_info->status;
	}

	if (!mask || !mask->ptr) {
		goto err;
	}
	switch (status) {
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
			status);
		goto err;
	}

	for (i = 0; i < msg_mask_tbl_count_local; i++, mask++) {
		if (!mask->ptr)
			continue;
		mutex_lock(&driver->msg_mask_lock);
		if (((mask->ssid_first > first) ||
			(mask->ssid_last_tools < last)) && first != ALL_SSID) {
			mutex_unlock(&driver->msg_mask_lock);
			continue;
		}

		mutex_lock(&mask->lock);
		if (status == DIAG_CTRL_MASK_VALID) {
			mask_size =
				mask->ssid_last_tools - mask->ssid_first + 1;
			temp_len = mask_size * sizeof(uint32_t);
			if (temp_len + header_len <=
				mask_info->update_buf_len) {
				if (sub_index >= 0 && preset > 0)
					goto proceed_sub_pkt;
				else
					goto proceed_legacy_pkt;
			}
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
				buf = temp;
				pr_debug("diag: In %s, successfully reallocated msg_mask update buffer to len: %d\n",
					 __func__, mask_info->update_buf_len);
			}
		} else if (status == DIAG_CTRL_MASK_ALL_ENABLED) {
			mask_size = 1;
		}
proceed_legacy_pkt:
		header.cmd_type = DIAG_CTRL_MSG_F3_MASK;
		header.status = mask_info->status;
		header.stream_id = 1;
		header.msg_mode = 0;
		header.ssid_first = mask->ssid_first;
		header.ssid_last = mask->ssid_last_tools;
		header.msg_mask_size = mask_size;
		mask_size *= sizeof(uint32_t);
		header.data_len = MSG_MASK_CTRL_HEADER_LEN + mask_size;
		memcpy(buf, &header, sizeof(header));
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
		else
			continue;
proceed_sub_pkt:
		header_sub.cmd_type = DIAG_CTRL_MSG_F3_MS_MASK;
		header_sub.version = 1;
		header_sub.preset_id = preset;
		if (sub_index >= 0) {
			header_sub.id_valid = 1;
			header_sub.sub_id = diag_subid_info[sub_index];
		} else {
			header_sub.id_valid = 0;
			header_sub.sub_id = 0;
		}
		header_sub.status = status;
		header_sub.stream_id = 1;
		header_sub.msg_mode = 0;
		header_sub.ssid_first = mask->ssid_first;
		header_sub.ssid_last = mask->ssid_last_tools;
		header_sub.msg_mask_size = mask_size;
		mask_size *= sizeof(uint32_t);
		header_sub.data_len = MSG_MASK_CTRL_HEADER_LEN_SUB + mask_size;
		memcpy(buf, &header_sub, sizeof(header_sub));
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
	if (driver->supports_pd_buffering)
		if (driver->feature[peripheral].pd_buffering)
			DIAG_SET_FEATURE_MASK(F_DIAG_PD_BUFFERING);
	if (driver->supports_diagid_v2_feature_mask)
		if (driver->feature[peripheral].diagid_v2_feature_mask)
			DIAG_SET_FEATURE_MASK(F_DIAGID_FEATURE_MASK);
	DIAG_SET_FEATURE_MASK(F_DIAG_MASK_CENTRALIZATION);
	if (driver->supports_sockets)
		DIAG_SET_FEATURE_MASK(F_DIAG_SOCKETS_ENABLED);
	DIAG_SET_FEATURE_MASK(F_DIAG_MULTI_SIM_SUPPORT);

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
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int i, sub_index;
	int write_len = 0;
	uint8_t msg_mask_tbl_count = 0;
	struct diag_msg_mask_t *mask_ptr = NULL;
	struct diag_msg_ssid_query_t rsp;
	struct diag_msg_ssid_query_sub_t rsp_ms;
	struct diag_ssid_range_t ssid_range;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

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

	if (!diag_apps_responds()) {
		mutex_unlock(&driver->md_session_lock);
		return 0;
	}
	mutex_lock(&driver->msg_mask_lock);
	msg_mask_tbl_count = (info) ? info->msg_mask_tbl_count :
		driver->msg_mask_tbl_count;
	if (cmd_ver) {
		memcpy(&rsp_ms, src_buf, src_len);
		rsp_ms.status = MSG_STATUS_SUCCESS;
		rsp_ms.reserved = 0;
		rsp_ms.count = driver->msg_mask_tbl_count;
		memcpy(dest_buf, &rsp_ms, sizeof(rsp_ms));
		write_len += sizeof(rsp_ms);
		if (rsp_ms.id_valid) {
			sub_index = diag_check_subid_mask_index(rsp_ms.sub_id,
				pid);
			ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr,
				sub_index);
			if (!ms_ptr)
				goto err;
			mask_ptr = (struct diag_msg_mask_t *)ms_ptr->sub_ptr;
		} else {
			mask_ptr = (struct diag_msg_mask_t *)mask_info->ptr;
		}
	} else {
		rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
		rsp.sub_cmd = DIAG_CMD_OP_GET_SSID_RANGE;
		rsp.status = MSG_STATUS_SUCCESS;
		rsp.padding = 0;
		rsp.count = msg_mask_tbl_count;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += sizeof(rsp);
		mask_ptr = (struct diag_msg_mask_t *)mask_info->ptr;
	}
	if (!mask_ptr || !mask_ptr->ptr) {
		pr_err("diag: In %s, Invalid mask\n",
			__func__);
		goto err;
	}
	for (i = 0; i < msg_mask_tbl_count; i++, mask_ptr++) {
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
err:
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&driver->md_session_lock);
	return write_len;
}

static int diag_cmd_get_build_mask(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int i = 0;
	int write_len = 0, ssid_last = 0, status = MSG_STATUS_FAIL;
	int num_entries = 0;
	int copy_len = 0;
	int header_len = 0;
	struct diag_msg_mask_t *build_mask = NULL;
	struct diag_build_mask_req_t *req = NULL;
	struct diag_msg_build_mask_t rsp;
	struct diag_build_mask_req_sub_t *req_sub = NULL;
	struct diag_msg_build_mask_sub_t rsp_sub;
	struct diag_ssid_range_t ssid_range;

	if (!src_buf || !dest_buf || dest_len <= 0 ||
		src_len < sizeof(struct diag_build_mask_req_t)) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d\n",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	if (!diag_apps_responds())
		return 0;
	mutex_lock(&driver->msg_mask_lock);
	build_mask = (struct diag_msg_mask_t *)msg_bt_mask.ptr;
	if (!cmd_ver) {
		if (src_len < sizeof(struct diag_build_mask_req_t))
			goto fail;
		req = (struct diag_build_mask_req_t *)src_buf;
		rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
		rsp.sub_cmd = DIAG_CMD_OP_GET_BUILD_MASK;
		rsp.ssid_first = req->ssid_first;
		rsp.ssid_last = req->ssid_last;
		rsp.status = MSG_STATUS_FAIL;
		rsp.padding = 0;
		ssid_range.ssid_first = req->ssid_first;
		ssid_range.ssid_last = req->ssid_last;
		header_len = sizeof(rsp);
	} else {
		if (src_len < sizeof(struct diag_build_mask_req_sub_t))
			goto fail;
		req_sub = (struct diag_build_mask_req_sub_t *)src_buf;
		rsp_sub.header.cmd_code = DIAG_CMD_MSG_CONFIG;
		rsp_sub.sub_cmd = DIAG_CMD_OP_GET_BUILD_MASK;
		rsp_sub.ssid_first = req_sub->ssid_first;
		rsp_sub.ssid_last = req_sub->ssid_last;
		rsp_sub.status = MSG_STATUS_FAIL;
		rsp_sub.reserved = 0;
		rsp_sub.sub_id = req_sub->sub_id;
		rsp_sub.id_valid = req_sub->id_valid;
		rsp_sub.header.subsys_id = req_sub->header.subsys_id;
		rsp_sub.header.subsys_cmd_code =
			req_sub->header.subsys_cmd_code;
		rsp_sub.version = req_sub->version;
		header_len = sizeof(rsp_sub);
		ssid_range.ssid_first = req_sub->ssid_first;
		ssid_range.ssid_last = req_sub->ssid_last;
	}
	for (i = 0; i < driver->bt_msg_mask_tbl_count; i++, build_mask++) {
		if (!build_mask->ptr)
			continue;
		if (build_mask->ssid_first != ssid_range.ssid_first)
			continue;
		num_entries = ssid_range.ssid_last - ssid_range.ssid_first + 1;
		if (num_entries > build_mask->range) {
			pr_warn("diag: In %s, truncating ssid range for ssid_first: %d ssid_last %d\n",
				__func__, ssid_range.ssid_first,
				ssid_range.ssid_last);
			num_entries = build_mask->range;
			ssid_range.ssid_last = ssid_range.ssid_first +
				build_mask->range;
		}
		copy_len = num_entries * sizeof(uint32_t);
		if (copy_len + header_len > dest_len)
			copy_len = dest_len - header_len;
		memcpy(dest_buf + header_len, build_mask->ptr, copy_len);
		write_len += copy_len;
		ssid_last = build_mask->ssid_last;
		status = MSG_STATUS_SUCCESS;
		break;
	}
	if (!cmd_ver) {
		rsp.ssid_last = ssid_last;
		rsp.status = status;
		memcpy(dest_buf, &rsp, sizeof(rsp));
	} else {
		rsp_sub.ssid_last = ssid_last;
		rsp_sub.status = status;
		memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
	}
	write_len += header_len;
fail:
	mutex_unlock(&driver->msg_mask_lock);
	return write_len;
}

static int diag_cmd_get_msg_mask(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int i, sub_index;
	int write_len = 0, header_len = 0, status = MSG_STATUS_FAIL;
	uint32_t mask_size = 0;
	uint8_t msg_mask_tbl_count = 0;
	struct diag_msg_mask_t *mask = NULL;
	struct diag_build_mask_req_t *req = NULL;
	struct diag_msg_build_mask_t rsp;
	struct diag_msg_build_mask_sub_t *req_sub = NULL;
	struct diag_msg_build_mask_sub_t rsp_sub;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;
	struct diag_ssid_range_t ssid_range;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!src_buf || !dest_buf || dest_len <= 0 ||
	    !mask_info || (src_len < sizeof(struct diag_build_mask_req_t))) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (!diag_apps_responds()) {
		mutex_unlock(&driver->md_session_lock);
		return 0;
	}

	mutex_lock(&driver->msg_mask_lock);
	msg_mask_tbl_count = (info) ? info->msg_mask_tbl_count :
			driver->msg_mask_tbl_count;
	if (!cmd_ver) {
		if (src_len < sizeof(struct diag_build_mask_req_t))
			goto err;
		req = (struct diag_build_mask_req_t *)src_buf;
		rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
		rsp.sub_cmd = DIAG_CMD_OP_GET_MSG_MASK;
		rsp.ssid_first = req->ssid_first;
		rsp.ssid_last = req->ssid_last;
		rsp.status = MSG_STATUS_FAIL;
		rsp.padding = 0;
		mask = (struct diag_msg_mask_t *)mask_info->ptr;
		ssid_range.ssid_first = req->ssid_first;
		ssid_range.ssid_last = req->ssid_last;
		header_len = sizeof(rsp);
	} else {
		if (src_len < sizeof(struct diag_msg_build_mask_sub_t))
			goto err;
		req_sub = (struct diag_msg_build_mask_sub_t *)src_buf;
		rsp_sub = *req_sub;
		rsp_sub.status = MSG_STATUS_FAIL;
		sub_index = diag_check_subid_mask_index(req_sub->sub_id, pid);
		ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr, sub_index);
		if (!ms_ptr)
			goto err;
		mask = (struct diag_msg_mask_t *)ms_ptr->sub_ptr;
		ssid_range.ssid_first = req_sub->ssid_first;
		ssid_range.ssid_last = req_sub->ssid_last;
		header_len = sizeof(rsp_sub);
	}
	if (!mask || !mask->ptr) {
		pr_err("diag: In %s, Invalid mask\n",
			__func__);
		write_len = -EINVAL;
		goto err;
	}
	for (i = 0; i < msg_mask_tbl_count; i++, mask++) {
		if (!mask->ptr)
			continue;
		if ((ssid_range.ssid_first < mask->ssid_first) ||
		    (ssid_range.ssid_first > mask->ssid_last_tools)) {
			continue;
		}
		mask_size = mask->range * sizeof(uint32_t);
		/* Copy msg mask only till the end of the rsp buffer */
		if (mask_size + header_len > dest_len)
			mask_size = dest_len - header_len;
		memcpy(dest_buf + header_len, mask->ptr, mask_size);
		write_len += mask_size;
		status = MSG_STATUS_SUCCESS;
		break;
	}
	if (!cmd_ver) {
		rsp.status = status;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += header_len;
	} else {
		rsp_sub.status = status;
		memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
		write_len += header_len;
	}
err:
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&driver->md_session_lock);
	return write_len;
}


static int diag_cmd_set_msg_mask(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	uint32_t mask_size = 0, offset = 0;
	uint32_t *temp = NULL;
	int write_len = 0, i = 0, found = 0, peripheral, ret = -EINVAL;
	int sub_index = INVALID_INDEX;
	int header_len = 0, status = MSG_STATUS_FAIL;
	struct diag_msg_mask_t *mask = NULL, *mask_next = NULL;
	struct diag_msg_build_mask_t *req = NULL;
	struct diag_msg_build_mask_t rsp;
	struct diag_msg_build_mask_sub_t *req_sub = NULL;
	struct diag_msg_build_mask_sub_t rsp_sub;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;
	uint8_t msg_mask_tbl_count = 0;
	struct diag_ssid_range_t ssid_range;
	struct diag_multisim_masks *ms_ptr = NULL;
	int preset = 0, ret_val = 0;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!src_buf || !dest_buf || dest_len <= 0 || !mask_info ||
		(src_len < sizeof(struct diag_msg_build_mask_t))) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	mutex_lock(&mask_info->lock);
	mutex_lock(&driver->msg_mask_lock);
	if (!cmd_ver) {
		if (src_len < sizeof(struct diag_msg_build_mask_t))
			goto err;
		req = (struct diag_msg_build_mask_t *)src_buf;
		mask = (struct diag_msg_mask_t *)mask_info->ptr;
		ssid_range.ssid_first = req->ssid_first;
		ssid_range.ssid_last = req->ssid_last;
		header_len = sizeof(struct diag_msg_build_mask_t);
	} else {
		if (src_len < sizeof(struct diag_msg_build_mask_sub_t))
			goto err;
		req_sub = (struct diag_msg_build_mask_sub_t *)src_buf;
		ssid_range.ssid_first = req_sub->ssid_first;
		ssid_range.ssid_last = req_sub->ssid_last;
		header_len = sizeof(struct diag_msg_build_mask_sub_t);
		if (req_sub->id_valid) {
			sub_index = diag_check_subid_mask_index(req_sub->sub_id,
				pid);
			ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr,
				sub_index);
			if (!ms_ptr)
				goto err;
			mask = (struct diag_msg_mask_t *)ms_ptr->sub_ptr;
		} else {
			mask = (struct diag_msg_mask_t *)mask_info->ptr;
		}
		preset = req_sub->reserved;
	}
	if (!mask || !mask->ptr) {
		pr_err("diag: In %s, Invalid mask\n",
			__func__);
		goto err;
	}
	msg_mask_tbl_count = (info) ? info->msg_mask_tbl_count :
			driver->msg_mask_tbl_count;
	for (i = 0; i < msg_mask_tbl_count; i++, mask++) {
		if (!mask->ptr)
			continue;
		if (i < (msg_mask_tbl_count - 1)) {
			mask_next = mask;
			mask_next++;
		} else {
			mask_next = NULL;
		}

		if ((ssid_range.ssid_first < mask->ssid_first) ||
			(ssid_range.ssid_first >
			mask->ssid_first + MAX_SSID_PER_RANGE) ||
			(mask_next &&
			(ssid_range.ssid_first >= mask_next->ssid_first))) {
			continue;
		}
		mask_next = NULL;
		found = 1;
		mutex_lock(&mask->lock);
		mask_size = ssid_range.ssid_last - ssid_range.ssid_first + 1;
		if (mask_size > MAX_SSID_PER_RANGE) {
			pr_warn("diag: In %s, truncating ssid range, %d-%d to max allowed: %d\n",
				__func__, mask->ssid_first, mask->ssid_last,
				MAX_SSID_PER_RANGE);
			mask_size = MAX_SSID_PER_RANGE;
			mask->range_tools = MAX_SSID_PER_RANGE;
			mask->ssid_last_tools =
				mask->ssid_first + mask->range_tools;
		}
		if (ssid_range.ssid_last > mask->ssid_last_tools) {
			pr_debug("diag: Msg SSID range mismatch\n");
			if (mask_size != MAX_SSID_PER_RANGE)
				mask->ssid_last_tools = ssid_range.ssid_last;
			mask->range_tools =
				mask->ssid_last_tools - mask->ssid_first + 1;
			temp = krealloc(mask->ptr,
					mask->range_tools * sizeof(uint32_t),
					GFP_KERNEL);
			if (!temp) {
				pr_err_ratelimited("diag: In %s, unable to allocate memory for msg mask ptr, mask_size: %d\n",
						   __func__, mask_size);
				mutex_unlock(&mask->lock);
				ret = -ENOMEM;
				goto err;
			}
			mask->ptr = temp;
		}

		offset = ssid_range.ssid_first - mask->ssid_first;
		if (offset + mask_size > mask->range_tools) {
			pr_err("diag: In %s, Not in msg mask range, mask_size: %d, offset: %d\n",
			       __func__, mask_size, offset);
			mutex_unlock(&mask->lock);
			break;
		}
		mask_size = mask_size * sizeof(uint32_t);
		if (mask_size && src_len >= header_len + mask_size)
			memcpy(mask->ptr + offset, src_buf + header_len,
				mask_size);
		mutex_unlock(&mask->lock);
		status = DIAG_CTRL_MASK_VALID;
		break;
	}
	if (ms_ptr)
		ms_ptr->status = status;
	else
		mask_info->status = status;
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	if (!cmd_ver) {
		rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
		rsp.sub_cmd = DIAG_CMD_OP_SET_MSG_MASK;
		rsp.ssid_first = req->ssid_first;
		rsp.ssid_last = req->ssid_last;
		rsp.status = found;
		rsp.padding = 0;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += header_len;
	} else {
		rsp_sub = *req_sub;
		rsp_sub.status = found;
		memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
		write_len += header_len;
	}
	if (!found)
		goto end;
	if (mask_size + write_len > dest_len)
		mask_size = dest_len - write_len;
	if (mask_size && src_len >= header_len + mask_size)
		memcpy(dest_buf + write_len, src_buf + header_len, mask_size);
	write_len += mask_size;
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA) {
			mutex_lock(&driver->md_session_lock);
			info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
								APPS_DATA);
			ret_val = diag_save_user_msg_mask(info);
			if (ret_val < 0)
				pr_err("diag: unable to save msg mask to update userspace clients err:%d\n",
					ret_val);
			mutex_unlock(&driver->md_session_lock);
			if (diag_check_update(APPS_DATA, pid))
				diag_update_userspace_clients(MSG_MASKS_TYPE);
			continue;
		}
		if (!diag_check_update(i, pid))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		if (peripheral < 0 || peripheral >= NUM_PERIPHERALS)
			continue;
		if (sub_index >= 0 &&
			!driver->feature[peripheral].multi_sim_support)
			continue;
		mutex_lock(&driver->md_session_lock);
		diag_send_msg_mask_update(peripheral, ssid_range.ssid_first,
			ssid_range.ssid_last, sub_index, preset);
		mutex_unlock(&driver->md_session_lock);
	}
end:
	return write_len;
err:
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	return ret;
}

static int diag_cmd_set_all_msg_mask(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int i, write_len = 0, peripheral, sub_index = INVALID_INDEX;
	int header_len = 0, status, ret = -EINVAL;
	struct diag_msg_config_rsp_t rsp;
	struct diag_msg_config_rsp_t *req = NULL;
	struct diag_msg_config_rsp_sub_t rsp_sub;
	struct diag_msg_config_rsp_sub_t *req_sub = NULL;
	struct diag_msg_mask_t *mask = NULL;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;
	uint8_t msg_mask_tbl_count = 0;
	uint32_t rt_mask;
	struct diag_multisim_masks *ms_ptr = NULL;
	int preset = 0, ret_val = 0;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!src_buf || !dest_buf || dest_len <= 0 || !mask_info ||
		(src_len < sizeof(struct diag_msg_config_rsp_t))) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	mutex_lock(&mask_info->lock);
	mutex_lock(&driver->msg_mask_lock);
	if (!cmd_ver) {
		if (src_len < sizeof(struct diag_msg_config_rsp_t))
			goto err;
		req = (struct diag_msg_config_rsp_t *)src_buf;
		mask = (struct diag_msg_mask_t *)mask_info->ptr;
		header_len = sizeof(struct diag_msg_config_rsp_t);
		rt_mask = req->rt_mask;
	} else {
		if (src_len < sizeof(struct diag_msg_config_rsp_sub_t))
			goto err;
		req_sub = (struct diag_msg_config_rsp_sub_t *)src_buf;
		header_len = sizeof(struct diag_msg_config_rsp_sub_t);
		if (req_sub->id_valid) {
			sub_index = diag_check_subid_mask_index(req_sub->sub_id,
				pid);
			ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr,
				sub_index);
			if (!ms_ptr)
				goto err;
			mask = (struct diag_msg_mask_t *)ms_ptr->sub_ptr;
		} else {
			mask = (struct diag_msg_mask_t *)mask_info->ptr;
		}
		rt_mask = req_sub->rt_mask;
		preset = req_sub->preset_id;
	}

	if (!mask || !mask->ptr) {
		pr_err("diag: In %s, Invalid mask\n",
			__func__);
		goto err;
	}
	msg_mask_tbl_count = (info) ? info->msg_mask_tbl_count :
			driver->msg_mask_tbl_count;

	status = (rt_mask) ? DIAG_CTRL_MASK_ALL_ENABLED :
					DIAG_CTRL_MASK_ALL_DISABLED;
	if (ms_ptr)
		ms_ptr->status = status;
	else
		mask_info->status = status;
	for (i = 0; i < msg_mask_tbl_count; i++, mask++) {
		if (mask && mask->ptr) {
			mutex_lock(&mask->lock);
			memset(mask->ptr, rt_mask,
			       mask->range * sizeof(uint32_t));
			mutex_unlock(&mask->lock);
		}
	}
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	if (!cmd_ver) {
		rsp.cmd_code = DIAG_CMD_MSG_CONFIG;
		rsp.sub_cmd = DIAG_CMD_OP_SET_ALL_MSG_MASK;
		rsp.status = MSG_STATUS_SUCCESS;
		rsp.padding = 0;
		rsp.rt_mask = req->rt_mask;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += header_len;
	} else {
		rsp_sub = *req_sub;
		rsp_sub.status = MSG_STATUS_SUCCESS;
		memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
		write_len += header_len;
	}
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA) {
			mutex_lock(&driver->md_session_lock);
			info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
								APPS_DATA);
			ret_val = diag_save_user_msg_mask(info);
			if (ret_val)
				pr_err("diag: unable to save msg mask to update userspace clients err:%d\n",
					ret_val);
			mutex_unlock(&driver->md_session_lock);
			if (diag_check_update(APPS_DATA, pid))
				diag_update_userspace_clients(MSG_MASKS_TYPE);
			continue;
		}
		if (!diag_check_update(i, pid))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		if (peripheral < 0 || peripheral >= NUM_PERIPHERALS)
			continue;
		if (sub_index >= 0 &&
			!driver->feature[peripheral].multi_sim_support)
			continue;
		mutex_lock(&driver->md_session_lock);
		diag_send_msg_mask_update(peripheral, ALL_SSID, ALL_SSID,
			sub_index, preset);
		mutex_unlock(&driver->md_session_lock);
	}

	return write_len;
err:
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	return ret;
}

static int diag_cmd_get_event_mask(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int write_len = 0, sub_index;
	uint32_t mask_size;
	struct diag_event_mask_config_t rsp;
	struct diag_event_mask_config_sub_t rsp_sub;
	struct diag_event_mask_req_sub_t *req = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

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

	if (!cmd_ver) {
		rsp.cmd_code = DIAG_CMD_GET_EVENT_MASK;
		rsp.status = EVENT_STATUS_SUCCESS;
		rsp.padding = 0;
		rsp.num_bits = driver->last_event_id + 1;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += sizeof(rsp);
		goto copy_mask;
	}
	if (src_len < sizeof(struct diag_event_mask_req_sub_t))
		return -EINVAL;
	req = (struct diag_event_mask_req_sub_t *)src_buf;
	rsp_sub.header.cmd_code = req->header.cmd_code;
	rsp_sub.header.subsys_id = req->header.subsys_id;
	rsp_sub.header.subsys_cmd_code = req->header.subsys_cmd_code;
	rsp_sub.version = req->version;
	rsp_sub.id_valid = req->id_valid;
	rsp_sub.sub_id = req->sub_id;
	rsp_sub.sub_cmd = req->sub_cmd;
	rsp_sub.preset_id = req->preset_id;
	rsp_sub.status = EVENT_STATUS_SUCCESS;
	rsp_sub.num_bits = driver->last_event_id + 1;
	memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
	write_len += sizeof(rsp_sub);
copy_mask:
	if (!cmd_ver || !req->id_valid)
		memcpy(dest_buf + write_len, event_mask.ptr, mask_size);
	else {
		sub_index = diag_check_subid_mask_index(req->sub_id, pid);
		ms_ptr = diag_get_ms_ptr_index(event_mask.ms_ptr, sub_index);
		if (!ms_ptr || !ms_ptr->sub_ptr)
			return 0;
		memcpy(dest_buf + write_len, ms_ptr->sub_ptr, mask_size);
	}
	write_len += mask_size;
	return write_len;
}

static int diag_cmd_update_event_mask(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int i, write_len = 0, mask_len = 0, peripheral;
	int sub_index = INVALID_INDEX, preset = 0;
	int header_len = 0, ret = -EINVAL;
	struct diag_event_mask_config_t rsp;
	struct diag_event_mask_config_t *req;
	struct diag_event_mask_config_sub_t rsp_sub;
	struct diag_event_mask_config_sub_t *req_sub;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);
	mask_info = (!info) ? &event_mask : info->event_mask;
	if (!src_buf || !dest_buf || dest_len <= 0 || !mask_info ||
		src_len < sizeof(struct diag_event_mask_config_t)) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	mutex_lock(&mask_info->lock);
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		goto err;
	}
	if (!cmd_ver) {
		if (src_len < sizeof(struct diag_event_mask_config_t))
			goto err;
		req = (struct diag_event_mask_config_t *)src_buf;
		mask_len = EVENT_COUNT_TO_BYTES(req->num_bits);
		header_len = sizeof(struct diag_event_mask_config_t);
	} else {
		if (src_len < sizeof(struct diag_event_mask_config_sub_t))
			goto err;
		req_sub = (struct diag_event_mask_config_sub_t *)src_buf;
		mask_len = EVENT_COUNT_TO_BYTES(req_sub->num_bits);
		header_len = sizeof(struct diag_event_mask_config_sub_t);
		preset = req_sub->preset_id;
	}
	if (mask_len <= 0 || mask_len > event_mask.mask_len) {
		pr_err("diag: In %s, invalid event mask len: %d\n", __func__,
			mask_len);
		ret = -EIO;
		goto err;
	}
	if (cmd_ver && req_sub->id_valid) {
		sub_index = diag_check_subid_mask_index(req_sub->sub_id, pid);
		if (sub_index < 0) {
			ret = sub_index;
			goto err;
		}
		ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr, sub_index);
		if (!ms_ptr)
			goto err;
		if (src_len >= header_len + mask_len - 1)
			memcpy(ms_ptr->sub_ptr, src_buf + header_len, mask_len);
		ms_ptr->status = DIAG_CTRL_MASK_VALID;
	} else {
		if (src_len >= header_len + mask_len - 1)
			memcpy(mask_info->ptr, src_buf + header_len, mask_len);
		mask_info->status = DIAG_CTRL_MASK_VALID;
	}
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	if (diag_check_update(APPS_DATA, pid))
		diag_update_userspace_clients(EVENT_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	if (!cmd_ver) {
		rsp.cmd_code = DIAG_CMD_SET_EVENT_MASK;
		rsp.status = EVENT_STATUS_SUCCESS;
		rsp.padding = 0;
		rsp.num_bits = driver->last_event_id + 1;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += header_len;
	} else {
		rsp_sub.header.cmd_code = req_sub->header.cmd_code;
		rsp_sub.header.subsys_id = req_sub->header.subsys_id;
		rsp_sub.header.subsys_cmd_code =
			req_sub->header.subsys_cmd_code;
		rsp_sub.version = req_sub->version;
		rsp_sub.id_valid = req_sub->id_valid;
		rsp_sub.sub_id = req_sub->sub_id;
		rsp_sub.sub_cmd = req_sub->sub_cmd;
		rsp_sub.preset_id = req_sub->preset_id;
		rsp_sub.status = EVENT_STATUS_SUCCESS;
		rsp_sub.num_bits = driver->last_event_id + 1;
		memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
		write_len += header_len;
	}
	memcpy(dest_buf + write_len, src_buf + header_len, mask_len);
	write_len += mask_len;

	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA)
			continue;
		if (!diag_check_update(i, pid))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		if (peripheral < 0 || peripheral >= NUM_PERIPHERALS)
			continue;
		if (sub_index >= 0 &&
			!driver->feature[peripheral].multi_sim_support)
			continue;
		mutex_lock(&driver->md_session_lock);
		diag_send_event_mask_update(peripheral, sub_index, preset);
		mutex_unlock(&driver->md_session_lock);
	}

	return write_len;
err:
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	return ret;
}

static int diag_cmd_toggle_events(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int write_len = 0, i, peripheral;
	int sub_index = INVALID_INDEX, preset = 0, ret = -EINVAL;
	uint8_t toggle = 0;
	struct diag_event_report_t header;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;
	struct diag_event_mask_req_sub_t *req = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);
	mask_info = (!info) ? &event_mask : info->event_mask;
	if (!src_buf || !dest_buf || src_len <= sizeof(uint8_t) ||
		dest_len <= 0 || !mask_info) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	mutex_lock(&mask_info->lock);
	if (!mask_info->ptr) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK\n",
			__func__, mask_info->ptr);
		goto err;
	}
	if (!cmd_ver)
		toggle = *(src_buf + sizeof(uint8_t));
	else {
		if (src_len < sizeof(struct diag_event_mask_req_sub_t))
			goto err;
		req = (struct diag_event_mask_req_sub_t *)src_buf;
		toggle = req->status;
		preset = req->preset_id;
	}
	if (cmd_ver && req->id_valid) {
		sub_index = diag_check_subid_mask_index(req->sub_id, pid);
		if (sub_index < 0) {
			ret = sub_index;
			goto err;
		}
		ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr, sub_index);
		if (!ms_ptr)
			goto err;
		if (toggle) {
			ms_ptr->status = DIAG_CTRL_MASK_ALL_ENABLED;
			memset(ms_ptr->sub_ptr, 0xFF, mask_info->mask_len);
		} else {
			ms_ptr->status = DIAG_CTRL_MASK_ALL_DISABLED;
			memset(ms_ptr->sub_ptr, 0, mask_info->mask_len);
		}
	} else {
		if (toggle) {
			mask_info->status = DIAG_CTRL_MASK_ALL_ENABLED;
			memset(mask_info->ptr, 0xFF, mask_info->mask_len);
		} else {
			mask_info->status = DIAG_CTRL_MASK_ALL_DISABLED;
			memset(mask_info->ptr, 0, mask_info->mask_len);
		}
	}
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	if (diag_check_update(APPS_DATA, pid))
		diag_update_userspace_clients(EVENT_MASKS_TYPE);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	if (!cmd_ver) {
		header.cmd_code = DIAG_CMD_EVENT_TOGGLE;
		header.padding = 0;
		memcpy(dest_buf, &header, sizeof(header));
		write_len += sizeof(header);
	} else {
		memcpy(dest_buf, src_buf, src_len);
		write_len += src_len;
	}
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA)
			continue;
		if (!diag_check_update(i, pid))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		if (peripheral < 0 || peripheral >= NUM_PERIPHERALS)
			continue;
		if (sub_index >= 0 &&
			!driver->feature[peripheral].multi_sim_support)
			continue;
		mutex_lock(&driver->md_session_lock);
		diag_send_event_mask_update(peripheral, sub_index, preset);
		mutex_unlock(&driver->md_session_lock);
	}

	return write_len;
err:
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	return ret;
}

static int diag_cmd_get_log_mask(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int i, sub_index = INVALID_INDEX;
	int status = LOG_STATUS_INVALID;
	int write_len = 0;
	int rsp_header_len = 0;
	uint32_t mask_size = 0, equip_id;
	struct diag_log_mask_t *log_item = NULL;
	struct diag_log_config_get_req_t *req;
	struct diag_log_config_rsp_t rsp;
	struct diag_log_config_rsp_sub_t *req_sub;
	struct diag_log_config_rsp_sub_t rsp_sub;
	struct diag_mask_info *mask_info = NULL;
	struct diag_md_session_t *info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &log_mask : info->log_mask;
	if (!src_buf || !dest_buf || dest_len <= 0 || !mask_info ||
		src_len < sizeof(struct diag_log_config_get_req_t)) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	mutex_lock(&mask_info->lock);

	if (!diag_apps_responds())
		goto err;

	if (!cmd_ver) {
		log_item = (struct diag_log_mask_t *)mask_info->ptr;
		if (src_len < sizeof(struct diag_log_config_get_req_t))
			goto err;
		req = (struct diag_log_config_get_req_t *)src_buf;
		rsp.cmd_code = DIAG_CMD_LOG_CONFIG;
		rsp.padding[0] = 0;
		rsp.padding[1] = 0;
		rsp.padding[2] = 0;
		rsp.sub_cmd = DIAG_CMD_OP_GET_LOG_MASK;
		rsp_header_len = sizeof(struct diag_log_config_rsp_t);
		equip_id = req->equip_id;
	} else {
		if (src_len < sizeof(struct diag_log_config_rsp_sub_t))
			goto err;
		req_sub = (struct diag_log_config_rsp_sub_t *)src_buf;
		if (req_sub->id_valid) {
			sub_index = diag_check_subid_mask_index(req_sub->sub_id,
				pid);
			ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr,
				sub_index);
			if (!ms_ptr) {
				write_len = -EINVAL;
				goto err;
			}
			log_item = (struct diag_log_mask_t *)ms_ptr->sub_ptr;
		} else {
			log_item = (struct diag_log_mask_t *)mask_info->ptr;
		}
		rsp_sub.header.cmd_code = req_sub->header.cmd_code;
		rsp_sub.header.subsys_id = req_sub->header.subsys_id;
		rsp_sub.header.subsys_cmd_code =
			req_sub->header.subsys_cmd_code;
		rsp_sub.version = req_sub->version;
		rsp_sub.id_valid = req_sub->id_valid;
		rsp_sub.sub_id = req_sub->sub_id;
		rsp_sub.operation_code = DIAG_CMD_OP_GET_LOG_MASK;
		rsp_sub.preset_id = req_sub->preset_id;
		rsp_header_len = sizeof(rsp_sub);
		equip_id = *(uint32_t *)(src_buf +
			sizeof(struct diag_log_config_rsp_sub_t));
	}
	if (!log_item || !log_item->ptr) {
		pr_err("diag: In %s, Invalid mask\n",
			__func__);
		write_len = -EINVAL;
		goto err;
	}
	/*
	 * Don't copy the response header now. Copy at the end after
	 * calculating the status field value
	 */
	write_len += rsp_header_len;

	for (i = 0; i < MAX_EQUIP_ID; i++, log_item++) {
		if (!log_item || !log_item->ptr) {
			write_len = -EINVAL;
			goto err;
		}
		if (log_item->equip_id != equip_id)
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
	if (cmd_ver) {
		rsp_sub.status = status;
		memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
	} else {
		rsp.status = status;
		memcpy(dest_buf, &rsp, sizeof(rsp));
	}
err:
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);
	return write_len;
}

static int diag_cmd_get_log_range(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	int i, sub_index = INVALID_INDEX;
	int write_len = 0;
	struct diag_log_config_rsp_t rsp;
	struct diag_log_config_rsp_sub_t rsp_sub;
	struct diag_log_config_req_sub_t *req;
	struct diag_log_mask_t *mask = NULL;
	struct diag_multisim_masks *ms_ptr;

	if (!diag_apps_responds())
		return 0;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d\n",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	if (!cmd_ver) {
		mask = (struct diag_log_mask_t *)log_mask.ptr;
		rsp.cmd_code = DIAG_CMD_LOG_CONFIG;
		rsp.padding[0] = 0;
		rsp.padding[1] = 0;
		rsp.padding[2] = 0;
		rsp.sub_cmd = DIAG_CMD_OP_GET_LOG_RANGE;
		rsp.status = LOG_STATUS_SUCCESS;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += sizeof(rsp);
	} else {
		if (src_len < sizeof(struct diag_log_config_req_sub_t))
			return -EINVAL;
		req = (struct diag_log_config_req_sub_t *)src_buf;
		if (req->id_valid) {
			sub_index = diag_check_subid_mask_index(req->sub_id,
				pid);
			ms_ptr = diag_get_ms_ptr_index(log_mask.ms_ptr,
				sub_index);
			if (!ms_ptr)
				return -EINVAL;
			mask = (struct diag_log_mask_t *)ms_ptr->sub_ptr;
		} else {
			mask = (struct diag_log_mask_t *)log_mask.ptr;
		}
		rsp_sub.header.cmd_code = req->header.cmd_code;
		rsp_sub.header.subsys_id = req->header.subsys_id;
		rsp_sub.header.subsys_cmd_code = req->header.subsys_cmd_code;
		rsp_sub.version = req->version;
		rsp_sub.id_valid = req->id_valid;
		rsp_sub.sub_id = req->sub_id;
		rsp_sub.operation_code = DIAG_CMD_OP_GET_LOG_RANGE;
		rsp_sub.status = LOG_STATUS_SUCCESS;
		rsp_sub.preset_id = 0;
		memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
		write_len += sizeof(rsp_sub);
	}

	for (i = 0; i < MAX_EQUIP_ID && write_len < dest_len; i++, mask++) {
		if (!mask)
			return -EINVAL;
		*(uint32_t *)(dest_buf + write_len) = mask->num_items_tools;
		write_len += sizeof(uint32_t);
	}

	return write_len;
}

static int diag_cmd_set_log_mask(unsigned char *src_buf, int src_len,
				 unsigned char *dest_buf, int dest_len,
				 int pid, int cmd_ver)
{
	int i, peripheral, write_len = 0;
	int status = LOG_STATUS_SUCCESS;
	int sub_index = INVALID_INDEX, read_len = 0, payload_len = 0;
	int rsp_header_len = 0, preset = 0, ret_val = 0;
	uint32_t mask_size = 0;
	struct diag_log_config_req_t *req;
	struct diag_log_config_set_rsp_t rsp;
	struct diag_log_config_rsp_sub_t *req_sub;
	struct diag_log_config_rsp_sub_t rsp_sub;
	struct diag_log_mask_t *mask = NULL;
	struct diag_logging_range_t range;
	struct diag_mask_info *mask_info = NULL;
	unsigned char *temp_buf = NULL;
	struct diag_md_session_t *info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	mask_info = (!info) ? &log_mask : info->log_mask;
	if (!src_buf || !dest_buf || dest_len <= 0 || !mask_info ||
		src_len < sizeof(struct diag_log_config_req_t)) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d, mask_info: %pK\n",
		       __func__, src_buf, src_len, dest_buf, dest_len,
		       mask_info);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	mutex_lock(&mask_info->lock);
	if (!cmd_ver) {
		if (src_len < sizeof(struct diag_log_config_req_t)) {
			mutex_unlock(&mask_info->lock);
			mutex_unlock(&driver->md_session_lock);
			return -EINVAL;
		}
		req = (struct diag_log_config_req_t *)src_buf;
		read_len += sizeof(struct diag_log_config_req_t);
		mask = (struct diag_log_mask_t *)mask_info->ptr;
		rsp_header_len = sizeof(struct diag_log_config_set_rsp_t);
		range.equip_id = req->equip_id;
		range.num_items = req->num_items;
	} else {
		if (src_len < sizeof(req_sub) + sizeof(range)) {
			mutex_unlock(&mask_info->lock);
			mutex_unlock(&driver->md_session_lock);
			return -EINVAL;
		}
		req_sub = (struct diag_log_config_rsp_sub_t *)src_buf;
		read_len += sizeof(struct diag_log_config_rsp_sub_t);
		if (req_sub->id_valid) {
			sub_index = diag_check_subid_mask_index(req_sub->sub_id,
				pid);
			ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr,
				sub_index);
			if (!ms_ptr) {
				mutex_unlock(&mask_info->lock);
				mutex_unlock(&driver->md_session_lock);
				return -EINVAL;
			}
			mask = (struct diag_log_mask_t *)ms_ptr->sub_ptr;
		} else {
			mask = (struct diag_log_mask_t *)mask_info->ptr;
		}
		rsp_header_len = sizeof(struct diag_log_config_rsp_sub_t);
		range = *(struct diag_logging_range_t *)(src_buf + read_len);
		read_len += sizeof(struct diag_logging_range_t);
		preset = req_sub->preset_id;
	}
	if (!mask || !mask->ptr) {
		pr_err("diag: In %s, Invalid mask\n",
			__func__);
		mutex_unlock(&mask_info->lock);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	if (range.equip_id >= MAX_EQUIP_ID) {
		pr_err("diag: In %s, Invalid logging mask request, equip_id: %d\n",
		       __func__, range.equip_id);
		status = LOG_STATUS_INVALID;
	}

	if (range.num_items == 0) {
		pr_err("diag: In %s, Invalid number of items in log mask request, equip_id: %d\n",
		       __func__, range.equip_id);
		status = LOG_STATUS_INVALID;
	}

	for (i = 0; i < MAX_EQUIP_ID && !status; i++, mask++) {
		if (!mask || !mask->ptr)
			continue;
		if (mask->equip_id != range.equip_id)
			continue;
		mutex_lock(&mask->lock);

		DIAG_LOG(DIAG_DEBUG_MASKS, "e: %d current: %d %d new: %d %d",
			 mask->equip_id, mask->num_items_tools,
			 mask->range_tools, range.num_items,
			 LOG_ITEMS_TO_SIZE(range.num_items));
		/*
		 * If the size of the log mask cannot fit into our
		 * buffer, trim till we have space left in the buffer.
		 * num_items should then reflect the items that we have
		 * in our buffer.
		 */
		mask->num_items_tools = (range.num_items > MAX_ITEMS_ALLOWED) ?
					MAX_ITEMS_ALLOWED : range.num_items;
		mask_size = LOG_ITEMS_TO_SIZE(mask->num_items_tools);
		memset(mask->ptr, 0, mask->range_tools);
		if (mask_size > mask->range_tools) {
			DIAG_LOG(DIAG_DEBUG_MASKS,
				 "log range mismatch, e: %d old: %d new: %d\n",
				 range.equip_id, mask->range_tools,
				 LOG_ITEMS_TO_SIZE(mask->num_items_tools));
			/* Change in the mask reported by tools */
			temp_buf = krealloc(mask->ptr, mask_size, GFP_KERNEL);
			if (!temp_buf) {
				if (ms_ptr)
					ms_ptr->status = DIAG_CTRL_MASK_INVALID;
				else
					mask_info->status =
						DIAG_CTRL_MASK_INVALID;
				mutex_unlock(&mask->lock);
				break;
			}
			mask->ptr = temp_buf;
			memset(mask->ptr, 0, mask_size);
			mask->range_tools = mask_size;
		}
		range.num_items = mask->num_items_tools;
		if (mask_size > 0 && src_len >= read_len + mask_size)
			memcpy(mask->ptr, src_buf + read_len, mask_size);
		DIAG_LOG(DIAG_DEBUG_MASKS,
			 "copying log mask, e %d num %d range %d size %d\n",
			 range.equip_id, mask->num_items_tools,
			 mask->range_tools, mask_size);
		mutex_unlock(&mask->lock);
		if (ms_ptr)
			ms_ptr->status = DIAG_CTRL_MASK_VALID;
		else
			mask_info->status = DIAG_CTRL_MASK_VALID;
		break;
	}
	mutex_unlock(&mask_info->lock);
	mutex_unlock(&driver->md_session_lock);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	payload_len = LOG_ITEMS_TO_SIZE(range.num_items);
	if ((payload_len + rsp_header_len > dest_len) || (payload_len == 0)) {
		pr_err("diag: In %s, invalid length, payload_len: %d, header_len: %d, dest_len: %d\n",
		       __func__, payload_len, rsp_header_len, dest_len);
		status = LOG_STATUS_FAIL;
	}
	if (!cmd_ver) {
		rsp.cmd_code = DIAG_CMD_LOG_CONFIG;
		rsp.padding[0] = 0;
		rsp.padding[1] = 0;
		rsp.padding[2] = 0;
		rsp.sub_cmd = DIAG_CMD_OP_SET_LOG_MASK;
		rsp.status = status;
		rsp.equip_id = req->equip_id;
		rsp.num_items = req->num_items;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += rsp_header_len;
	} else {
		rsp_sub.header.cmd_code = req_sub->header.cmd_code;
		rsp_sub.header.subsys_id = req_sub->header.subsys_id;
		rsp_sub.header.subsys_cmd_code =
			req_sub->header.subsys_cmd_code;
		rsp_sub.version = req_sub->version;
		rsp_sub.id_valid = req_sub->id_valid;
		rsp_sub.sub_id = req_sub->sub_id;
		rsp_sub.operation_code = DIAG_CMD_OP_SET_LOG_MASK;
		rsp_sub.preset_id = req_sub->preset_id;
		rsp_sub.status = status;
		memcpy(dest_buf, &rsp_sub, sizeof(rsp_sub));
		write_len += sizeof(rsp_sub);
		memcpy(dest_buf + write_len, &range, sizeof(range));
		write_len += sizeof(range);
	}
	if (status != LOG_STATUS_SUCCESS)
		goto end;
	memcpy(dest_buf + write_len, src_buf + read_len, payload_len);
	write_len += payload_len;

	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA) {
			mutex_lock(&driver->md_session_lock);
			info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
								APPS_DATA);
			ret_val = diag_save_user_log_mask(info);
			if (ret_val < 0)
				pr_err("diag: unable to save log mask to update userspace clients err:%d\n",
					ret_val);
			mutex_unlock(&driver->md_session_lock);
			if (diag_check_update(APPS_DATA, pid))
				diag_update_userspace_clients(LOG_MASKS_TYPE);
			continue;
		}
		if (!diag_check_update(i, pid))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		if (peripheral < 0 || peripheral >= NUM_PERIPHERALS)
			continue;
		if (sub_index >= 0 &&
			!driver->feature[peripheral].multi_sim_support)
			continue;
		mutex_lock(&driver->md_session_lock);
		diag_send_log_mask_update(peripheral, range.equip_id,
			sub_index, preset);
		mutex_unlock(&driver->md_session_lock);
	}
end:
	return write_len;
}

static int diag_cmd_disable_log_mask(unsigned char *src_buf, int src_len,
		unsigned char *dest_buf, int dest_len, int pid, int cmd_ver)
{
	struct diag_mask_info *mask_info = NULL;
	struct diag_log_mask_t *mask = NULL;
	struct diag_log_config_rsp_t header;
	struct diag_log_config_rsp_sub_t rsp;
	struct diag_log_config_rsp_sub_t *req;
	int write_len = 0, i, peripheral;
	int preset = 0, sub_index = INVALID_INDEX, ret_val = 0;
	struct diag_md_session_t *info = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

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

	if (!cmd_ver) {
		mask = (struct diag_log_mask_t *)mask_info->ptr;
	} else {
		ms_ptr = mask_info->ms_ptr;
		if (!ms_ptr || src_len < sizeof(req)) {
			mutex_unlock(&driver->md_session_lock);
			return -EINVAL;
		}
		req = (struct diag_log_config_rsp_sub_t *)src_buf;
		if (req->id_valid) {
			sub_index = diag_check_subid_mask_index(req->sub_id,
				pid);
			ms_ptr = diag_get_ms_ptr_index(mask_info->ms_ptr,
				sub_index);
			if (!ms_ptr) {
				mutex_unlock(&driver->md_session_lock);
				return -EINVAL;
			}
			mask = (struct diag_log_mask_t *)ms_ptr->sub_ptr;
		} else {
			mask = (struct diag_log_mask_t *)mask_info->ptr;
		}
	}
	if (!mask || !mask->ptr) {
		pr_err("diag: In %s, Invalid mask\n",
			__func__);
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}
	for (i = 0; i < MAX_EQUIP_ID; i++, mask++) {
		if (mask && mask->ptr) {
			mutex_lock(&mask->lock);
			memset(mask->ptr, 0, mask->range);
			mutex_unlock(&mask->lock);
		}
	}
	mask_info->status = DIAG_CTRL_MASK_ALL_DISABLED;
	mutex_unlock(&driver->md_session_lock);

	/*
	 * Apps processor must send the response to this command. Frame the
	 * response.
	 */
	if (!cmd_ver) {
		header.cmd_code = DIAG_CMD_LOG_CONFIG;
		header.padding[0] = 0;
		header.padding[1] = 0;
		header.padding[2] = 0;
		header.sub_cmd = DIAG_CMD_OP_LOG_DISABLE;
		header.status = LOG_STATUS_SUCCESS;
		memcpy(dest_buf, &header, sizeof(struct diag_log_config_rsp_t));
		write_len += sizeof(struct diag_log_config_rsp_t);
	} else {
		rsp.header.cmd_code = req->header.cmd_code;
		rsp.header.subsys_id = req->header.subsys_id;
		rsp.header.subsys_cmd_code = req->header.subsys_cmd_code;
		rsp.version = req->version;
		rsp.id_valid = req->id_valid;
		rsp.sub_id = req->sub_id;
		rsp.operation_code = DIAG_CMD_OP_LOG_DISABLE;
		rsp.preset_id = req->preset_id;
		rsp.status = LOG_STATUS_SUCCESS;
		memcpy(dest_buf, &rsp, sizeof(rsp));
		write_len += sizeof(rsp);
		preset = req->preset_id;
	}
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (i == APPS_DATA) {
			mutex_lock(&driver->md_session_lock);
			info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
								APPS_DATA);
			ret_val = diag_save_user_log_mask(info);
			if (ret_val < 0)
				pr_err("diag: unable to save log  mask to update userspace clients err:%d\n",
					ret_val);
			mutex_unlock(&driver->md_session_lock);
			if (diag_check_update(APPS_DATA, pid))
				diag_update_userspace_clients(LOG_MASKS_TYPE);
			continue;
		}
		if (!diag_check_update(i, pid))
			continue;
		if (i > NUM_PERIPHERALS)
			peripheral = diag_search_peripheral_by_pd(i);
		else
			peripheral = i;
		if (peripheral < 0 || peripheral >= NUM_PERIPHERALS)
			continue;
		if (sub_index >= 0 &&
			!driver->feature[peripheral].multi_sim_support)
			continue;
		mutex_lock(&driver->md_session_lock);
		diag_send_log_mask_update(peripheral, ALL_EQUIP_ID,
			sub_index, preset);
		mutex_unlock(&driver->md_session_lock);
	}

	return write_len;
}


int diag_create_msg_mask_table_entry(struct diag_msg_mask_t *msg_mask,
			struct diag_ssid_range_t *range, int subid_index)
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
	if (subid_index >= 0) {
		msg_mask->id_valid = 1;
		msg_mask->sub_id = diag_subid_info[subid_index];
	} else {
		msg_mask->id_valid = 0;
		msg_mask->sub_id = 0;
	}
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

static int diag_create_msg_mask_table(int subid_index)
{
	int i, err = 0;
	struct diag_msg_mask_t *mask;
	struct diag_multisim_masks *ms_mask = NULL;
	struct diag_ssid_range_t range;

	if (subid_index >= 0)
		ms_mask = diag_get_ms_ptr_index(msg_mask.ms_ptr,
			subid_index);
	if (ms_mask)
		mask = (struct diag_msg_mask_t *)ms_mask->sub_ptr;
	else
		mask = (struct diag_msg_mask_t *)msg_mask.ptr;

	mutex_lock(&msg_mask.lock);
	mutex_lock(&driver->msg_mask_lock);
	driver->msg_mask_tbl_count = MSG_MASK_TBL_CNT;
	for (i = 0; (i < driver->msg_mask_tbl_count) && mask;
			i++, mask++) {
		range.ssid_first = msg_mask_tbl[i].ssid_first;
		range.ssid_last = msg_mask_tbl[i].ssid_last;
		err = diag_create_msg_mask_table_entry(mask,
				&range, subid_index);
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
	for (i = 0; (i < driver->bt_msg_mask_tbl_count) && build_mask;
			i++, build_mask++) {
		range.ssid_first = msg_mask_tbl[i].ssid_first;
		range.ssid_last = msg_mask_tbl[i].ssid_last;
		err = diag_create_msg_mask_table_entry(build_mask, &range,
			INVALID_INDEX);
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

static int diag_create_log_mask_table(int subid_index)
{
	struct diag_log_mask_t *mask = NULL;
	struct diag_multisim_masks *ms_mask = NULL;
	uint8_t i;
	int err = 0;

	mutex_lock(&log_mask.lock);
	if (subid_index >= 0)
		ms_mask = diag_get_ms_ptr_index(log_mask.ms_ptr,
			subid_index);
	if (ms_mask)
		mask = (struct diag_log_mask_t *)ms_mask->sub_ptr;
	else
		mask = (struct diag_log_mask_t *)(log_mask.ptr);
	for (i = 0; (i < MAX_EQUIP_ID) && mask; i++, mask++) {
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
		if (subid_index >= 0) {
			mask->id_valid = 1;
			mask->sub_id = diag_subid_info[subid_index];
		} else {
			mask->id_valid = 0;
			mask->sub_id = 0;
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
			mask_info->ptr = NULL;
			return -ENOMEM;
		}
		kmemleak_not_leak(mask_info->update_buf);
		mask_info->update_buf_client = kzalloc(MAX_USERSPACE_BUF_SIZ,
							GFP_KERNEL);
		if (!mask_info->update_buf_client) {
			kfree(mask_info->update_buf);
			mask_info->update_buf = NULL;
			kfree(mask_info->ptr);
			mask_info->ptr = NULL;
			return -ENOMEM;
		}
		kmemleak_not_leak(mask_info->update_buf_client);
		mask_info->update_buf_client_len = 0;
	}
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
	kfree(mask_info->update_buf_client);
	mask_info->update_buf_client = NULL;
	mutex_unlock(&mask_info->lock);
}

static int diag_log_mask_copy_sub(struct diag_mask_info *dest,
		struct diag_mask_info *src, int sub_index)
{
	int i, err = 0;
	struct diag_multisim_masks *src_ms_ptr = NULL;
	struct diag_log_mask_t *src_mask = NULL;
	struct diag_multisim_masks *dest_ms_ptr = NULL;
	struct diag_log_mask_t *dest_mask = NULL;

	if (!src || !dest)
		return -EINVAL;

	if (sub_index < 0)
		return -EINVAL;

	err = __diag_multisim_mask_init(dest, LOG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;

	mutex_lock(&dest->lock);

	src_ms_ptr = diag_get_ms_ptr_index(src->ms_ptr, sub_index);
	if (!src_ms_ptr) {
		mutex_unlock(&dest->lock);
		return 0;
	}
	src_mask = (struct diag_log_mask_t *)src_ms_ptr->sub_ptr;

	dest_ms_ptr = diag_get_ms_ptr_index(dest->ms_ptr, sub_index);
	if (!dest_ms_ptr) {
		mutex_unlock(&dest->lock);
		return 0;
	}
	dest_mask = (struct diag_log_mask_t *)dest_ms_ptr->sub_ptr;

	dest_ms_ptr->status = src_ms_ptr->status;

	for (i = 0; i < MAX_EQUIP_ID; i++, src_mask++, dest_mask++) {
		dest_mask->equip_id = src_mask->equip_id;
		dest_mask->num_items = src_mask->num_items;
		dest_mask->num_items_tools = src_mask->num_items_tools;
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

int diag_log_mask_copy(struct diag_mask_info *dest, struct diag_mask_info *src)
{
	int i, err = 0;
	struct diag_log_mask_t *src_mask = NULL;
	struct diag_log_mask_t *dest_mask = NULL;

	if (!src || !dest)
		return -EINVAL;

	mutex_init(&dest->lock);
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

	for (i = 0; (i < MAX_SIM_NUM) && (diag_subid_info[i] != INVALID_INDEX);
		i++) {
		err = diag_log_mask_copy_sub(dest, src, i);
		if (err)
			break;
	}
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
	struct diag_md_session_t *session_info = NULL;

	mutex_init(&msg_mask.lock);
	err = __diag_mask_init(&msg_mask, MSG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;

	err = diag_create_msg_mask_table(INVALID_INDEX);
	if (err) {
		pr_err("diag: Unable to create msg masks, err: %d\n", err);
		return err;
	}
	mutex_lock(&driver->msg_mask_lock);
	driver->msg_mask = &msg_mask;
	for (i = 0; i < NUM_PERIPHERALS; i++)
		driver->max_ssid_count[i] = 0;
	mutex_unlock(&driver->msg_mask_lock);

	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
							APPS_DATA);
	err = diag_save_user_msg_mask(session_info);
	if (err < 0)
		pr_err("diag: unable to save msg mask to update userspace clients err:%d\n",
			err);
	mutex_unlock(&driver->md_session_lock);
	return 0;
}

static int diag_msg_mask_copy_sub(struct diag_md_session_t *new_session,
	struct diag_mask_info *dest, struct diag_mask_info *src, int sub_index)
{
	int i, err = 0, mask_size = 0;
	struct diag_ssid_range_t range;
	struct diag_multisim_masks *src_ms_ptr = NULL;
	struct diag_msg_mask_t *src_mask = NULL;
	struct diag_multisim_masks *dest_ms_ptr = NULL;
	struct diag_msg_mask_t *dest_mask = NULL;

	if (!src || !dest)
		return -EINVAL;

	if (sub_index < 0)
		return -EINVAL;

	mutex_lock(&dest->lock);
	mutex_lock(&driver->msg_mask_lock);

	err = __diag_multisim_mask_init(dest,
		(new_session->msg_mask_tbl_count *
		sizeof(struct diag_msg_mask_t)), APPS_BUF_SIZE);
	if (err) {
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&dest->lock);
		return err;
	}

	src_ms_ptr = diag_get_ms_ptr_index(src->ms_ptr, sub_index);
	if (!src_ms_ptr) {
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&dest->lock);
		return 0;
	}
	src_mask = (struct diag_msg_mask_t *)src_ms_ptr->sub_ptr;

	dest_ms_ptr = diag_get_ms_ptr_index(dest->ms_ptr, sub_index);
	if (!dest_ms_ptr) {
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&dest->lock);
		return 0;
	}
	dest_mask = (struct diag_msg_mask_t *)dest_ms_ptr->sub_ptr;

	dest_ms_ptr->status = src_ms_ptr->status;
	for (i = 0; i < new_session->msg_mask_tbl_count; i++) {
		range.ssid_first = src_mask->ssid_first;
		range.ssid_last = src_mask->ssid_last;
		err = diag_create_msg_mask_table_entry(dest_mask,
				&range, sub_index);
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

int diag_msg_mask_copy(struct diag_md_session_t *new_session,
	struct diag_mask_info *dest, struct diag_mask_info *src)
{
	int i, err = 0, mask_size = 0;
	struct diag_msg_mask_t *src_mask = NULL;
	struct diag_msg_mask_t *dest_mask = NULL;
	struct diag_ssid_range_t range;

	if (!src || !dest)
		return -EINVAL;

	mutex_init(&dest->lock);
	mutex_lock(&dest->lock);
	mutex_lock(&driver->msg_mask_lock);
	new_session->msg_mask_tbl_count =
		driver->msg_mask_tbl_count;
	err = __diag_mask_init(dest,
		(new_session->msg_mask_tbl_count *
		sizeof(struct diag_msg_mask_t)), APPS_BUF_SIZE);
	if (err) {
		mutex_unlock(&driver->msg_mask_lock);
		mutex_unlock(&dest->lock);
		return err;
	}
	src_mask = (struct diag_msg_mask_t *)src->ptr;
	dest_mask = (struct diag_msg_mask_t *)dest->ptr;

	dest->mask_len = src->mask_len;
	dest->status = src->status;
	for (i = 0; i < new_session->msg_mask_tbl_count; i++) {
		range.ssid_first = src_mask->ssid_first;
		range.ssid_last = src_mask->ssid_last;
		err = diag_create_msg_mask_table_entry(dest_mask, &range,
			INVALID_INDEX);
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

	for (i = 0; (i < MAX_SIM_NUM) && (diag_subid_info[i] != INVALID_INDEX);
		i++) {
		err = diag_msg_mask_copy_sub(new_session, dest, src, i);
		if (err)
			break;
	}
	return err;
}

void diag_msg_mask_free(struct diag_mask_info *mask_info,
	struct diag_md_session_t *session_info)
{
	int i;
	struct diag_msg_mask_t *mask = NULL;
	uint8_t msg_mask_tbl_count = 0;

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
	msg_mask_tbl_count = (session_info) ?
		session_info->msg_mask_tbl_count :
		driver->msg_mask_tbl_count;
	for (i = 0; i < msg_mask_tbl_count; i++, mask++) {
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
	struct diag_msg_mask_t *sub_mask = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

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
	ms_ptr = (struct diag_multisim_masks *)(msg_mask.ms_ptr);
	while (ms_ptr) {
		sub_mask = (struct diag_msg_mask_t *)ms_ptr->sub_ptr;
		if (sub_mask) {
			for (i = 0; i < driver->msg_mask_tbl_count;
				i++, sub_mask++) {
				kfree(sub_mask->ptr);
				sub_mask->ptr = NULL;
			}
			kfree(ms_ptr->sub_ptr);
			ms_ptr->sub_ptr = NULL;
		}
		ms_ptr = ms_ptr->next;
	}
	msg_mask.ms_ptr = NULL;
	kfree(msg_mask.update_buf_client);
	msg_mask.update_buf_client = NULL;
	mutex_unlock(&driver->msg_mask_lock);
}

static int diag_build_time_mask_init(void)
{
	int err = 0;

	/* There is no need for update buffer for Build Time masks */
	mutex_init(&msg_bt_mask.lock);
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
	struct diag_md_session_t *session_info = NULL;

	mutex_init(&log_mask.lock);
	err = __diag_mask_init(&log_mask, LOG_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;
	err = diag_create_log_mask_table(INVALID_INDEX);
	if (err)
		return err;
	driver->log_mask = &log_mask;

	for (i = 0; i < NUM_PERIPHERALS; i++)
		driver->num_equip_id[i] = 0;
	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
							APPS_DATA);
	err = diag_save_user_log_mask(session_info);
	if (err < 0)
		pr_err("diag: unable to save log  mask to update userspace clients err:%d\n",
			err);
	mutex_unlock(&driver->md_session_lock);
	return 0;
}

static void diag_log_mask_exit(void)
{
	int i;
	struct diag_log_mask_t *mask = NULL;
	struct diag_log_mask_t *sub_mask = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

	mask = (struct diag_log_mask_t *)(log_mask.ptr);
	if (mask) {
		for (i = 0; i < MAX_EQUIP_ID; i++, mask++)
			kfree(mask->ptr);
		kfree(log_mask.ptr);
	}

	kfree(log_mask.update_buf);
	ms_ptr = (struct diag_multisim_masks *)(log_mask.ms_ptr);
	while (ms_ptr) {
		sub_mask = (struct diag_log_mask_t *)ms_ptr->sub_ptr;
		if (sub_mask) {
			for (i = 0; i < MAX_EQUIP_ID; i++, sub_mask++) {
				kfree(sub_mask->ptr);
				sub_mask->ptr = NULL;
			}
			kfree(ms_ptr->sub_ptr);
			ms_ptr->sub_ptr = NULL;
		}
		ms_ptr = ms_ptr->next;
	}
	log_mask.ms_ptr = NULL;
	kfree(log_mask.update_buf_client);
	log_mask.update_buf_client = NULL;
}

static int diag_event_mask_init(void)
{
	int err = 0, i;

	mutex_init(&event_mask.lock);
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

static int diag_event_mask_copy_sub(struct diag_mask_info *dest,
			 struct diag_mask_info *src, int sub_index)
{
	int err = 0;
	struct diag_multisim_masks *src_ms_ptr = NULL;
	struct diag_multisim_masks *dest_ms_ptr = NULL;

	if (!src || !dest)
		return -EINVAL;

	if (sub_index < 0)
		return -EINVAL;

	err = __diag_multisim_mask_init(dest, EVENT_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;

	src_ms_ptr = diag_get_ms_ptr_index(src->ms_ptr, sub_index);
	if (!src_ms_ptr)
		return 0;

	dest_ms_ptr = diag_get_ms_ptr_index(dest->ms_ptr, sub_index);
	if (!dest_ms_ptr)
		return 0;

	mutex_lock(&dest->lock);
	dest_ms_ptr->status = src_ms_ptr->status;
	memcpy(dest_ms_ptr->sub_ptr, src_ms_ptr->sub_ptr, dest->mask_len);
	mutex_unlock(&dest->lock);

	return err;
}

int diag_event_mask_copy(struct diag_mask_info *dest,
			 struct diag_mask_info *src)
{
	int err = 0, i;

	if (!src || !dest)
		return -EINVAL;

	mutex_init(&dest->lock);
	err = __diag_mask_init(dest, EVENT_MASK_SIZE, APPS_BUF_SIZE);
	if (err)
		return err;

	mutex_lock(&dest->lock);
	dest->mask_len = src->mask_len;
	dest->status = src->status;
	memcpy(dest->ptr, src->ptr, dest->mask_len);
	mutex_unlock(&dest->lock);

	for (i = 0; (i < MAX_SIM_NUM) && (diag_subid_info[i] != INVALID_INDEX);
		i++) {
		err = diag_event_mask_copy_sub(dest, src, i);
		if (err)
			break;
	}
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
	struct diag_multisim_masks *ms_ptr = NULL;

	kfree(event_mask.ptr);
	kfree(event_mask.update_buf);
	ms_ptr = (struct diag_multisim_masks *)(event_mask.ms_ptr);
	while (ms_ptr) {
		kfree(ms_ptr->sub_ptr);
		ms_ptr->sub_ptr = NULL;
		ms_ptr = ms_ptr->next;
	}
	event_mask.ms_ptr = NULL;
}

int diag_copy_to_user_msg_mask(char __user *buf, size_t count,
			       struct diag_md_session_t *info)
{
	struct diag_mask_info *mask_info = NULL;
	int err = 0;

	if (!buf || count == 0)
		return -EINVAL;
	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!mask_info)
		return -EIO;

	if (!mask_info->ptr || !mask_info->update_buf_client) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK, mask_info->update_buf_client: %pK\n",
			__func__, mask_info->ptr, mask_info->update_buf_client);
		return -EINVAL;
	}

	err = copy_to_user(buf, mask_info->update_buf_client,
				mask_info->update_buf_client_len);
	if (err) {
		pr_err("diag: In %s Unable to send msg masks to user space clients, err: %d\n",
		       __func__, err);
	}
	return err ? err : mask_info->update_buf_client_len;
}

int diag_copy_to_user_log_mask(char __user *buf, size_t count,
			       struct diag_md_session_t *info)
{
	struct diag_mask_info *mask_info = NULL;
	int err = 0;

	if (!buf || count == 0)
		return -EINVAL;
	mask_info = (!info) ? &log_mask : info->log_mask;
	if (!mask_info)
		return -EIO;

	if (!mask_info->ptr || !mask_info->update_buf_client) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK, mask_info->update_buf_client: %pK\n",
			__func__, mask_info->ptr, mask_info->update_buf_client);
		return -EINVAL;
	}

	err = copy_to_user(buf, mask_info->update_buf_client,
				mask_info->update_buf_client_len);
	if (err) {
		pr_err("diag: In %s Unable to send msg masks to user space clients, err: %d\n",
		       __func__, err);
	}
	return err ? err : mask_info->update_buf_client_len;
}

static int diag_save_user_msg_mask(struct diag_md_session_t *info)
{
	int i, err = 0, len = 0;
	int copy_len = 0, total_len = 0;
	struct diag_msg_mask_userspace_t header;
	struct diag_mask_info *mask_info = NULL;
	struct diag_msg_mask_t *mask = NULL;
	unsigned char *ptr = NULL;
	uint8_t msg_mask_tbl_count = 0;

	mask_info = (!info) ? &msg_mask : info->msg_mask;
	if (!mask_info)
		return -EIO;

	if (!mask_info->ptr || !mask_info->update_buf) {
		pr_err("diag: In %s, invalid input mask_info->ptr: %pK, mask_info->update_buf: %pK\n",
			__func__, mask_info->ptr, mask_info->update_buf);
		return -EINVAL;
	}
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
	msg_mask_tbl_count = (info) ? info->msg_mask_tbl_count :
			driver->msg_mask_tbl_count;
	for (i = 0; i < msg_mask_tbl_count; i++, mask++) {
		if (!mask->ptr)
			continue;
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
		if (total_len + sizeof(int) + len > MAX_USERSPACE_BUF_SIZ) {
			pr_err("diag: In %s, unable to send msg masks to user space, total_len: %d,\n",
			       __func__, total_len);
			err = -ENOMEM;
			break;
		}
		memcpy(mask_info->update_buf_client + total_len,
			(void *)ptr, len);
		total_len += len;
	}
	mask_info->update_buf_client_len = total_len;
	mutex_unlock(&driver->msg_mask_lock);
	mutex_unlock(&mask_info->lock);
	return err ? err : total_len;
}

static int diag_save_user_log_mask(struct diag_md_session_t *info)
{
	int i, err = 0, len = 0;
	int copy_len = 0, total_len = 0;
	struct diag_log_mask_userspace_t header;
	struct diag_log_mask_t *mask = NULL;
	struct diag_mask_info *mask_info = NULL;
	unsigned char *ptr = NULL;

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
		if (!mask->ptr)
			continue;
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
		if (total_len + sizeof(int) + len > MAX_USERSPACE_BUF_SIZ) {
			pr_err("diag: In %s, unable to send log masks to user space, total_len: %d\n",
			       __func__, total_len);
			err = -ENOMEM;
			break;
		}
		memcpy(mask_info->update_buf_client + total_len,
			(void *)ptr, len);
		total_len += len;
	}
	mask_info->update_buf_client_len = total_len;
	mutex_unlock(&mask_info->lock);

	return err ? err : total_len;
}

void diag_send_updates_peripheral(uint8_t peripheral)
{
	if (!driver->feature[peripheral].rcvd_feature_mask)
		return;

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
		if (driver->set_mask_cmd) {
			diag_send_msg_mask_update(peripheral,
				ALL_SSID, ALL_SSID,
				INVALID_INDEX, LEGACY_MASK_CMD);
			diag_send_log_mask_update(peripheral, ALL_EQUIP_ID,
				INVALID_INDEX, LEGACY_MASK_CMD);
			diag_send_event_mask_update(peripheral, INVALID_INDEX,
				LEGACY_MASK_CMD);
		}
		mutex_unlock(&driver->md_session_lock);
		diag_send_real_time_update(peripheral,
				driver->real_time_mode[DIAG_LOCAL_PROC]);
		diag_send_peripheral_buffering_mode(
					&driver->buffering_mode[peripheral]);
		if (P_FMASK_DIAGID_V2(peripheral))
			diag_send_hw_accel_status(peripheral);
		/*
		 * Clear mask_update variable afer updating
		 * logging masks to peripheral.
		 */
		mutex_lock(&driver->cntl_lock);
		driver->mask_update ^= PERIPHERAL_MASK(peripheral);
		mutex_unlock(&driver->cntl_lock);
	}
}

static int diag_check_multisim_support(struct diag_pkt_header_t *header)
{
	if (!header)
		return 0;
	if (header->cmd_code == DIAG_CMD_DIAG_SUBSYS &&
		header->subsys_id == DIAG_SS_DIAG &&
		(header->subsys_cmd_code == DIAG_SUB_SYS_CMD_MSG ||
		header->subsys_cmd_code == DIAG_SUB_SYS_CMD_LOG ||
		header->subsys_cmd_code == DIAG_SUB_SYS_CMD_EVENT))
		return driver->multisim_feature_rcvd;
	else
		return 0;
}

int diag_process_apps_masks(unsigned char *buf, int len, int pid)
{
	int size = 0, sub_cmd = 0, subid_index = INVALID_INDEX;
	int cmd_version = LEGACY_MASK_CMD;
	uint8_t subid_valid;
	uint32_t subid;
	struct diag_pkt_header_t *header = NULL;
	int (*hdlr)(unsigned char *src_buf, int src_len,
			unsigned char *dest_buf, int dest_len, int pid,
			int cmd_ver) = NULL;

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
			driver->set_mask_cmd = 1;
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
			driver->set_mask_cmd = 1;
			break;
		case DIAG_CMD_OP_SET_ALL_MSG_MASK:
			hdlr = diag_cmd_set_all_msg_mask;
			driver->set_mask_cmd = 1;
			break;
		}
	} else if (*buf == DIAG_CMD_GET_EVENT_MASK) {
		hdlr = diag_cmd_get_event_mask;
	} else if (*buf == DIAG_CMD_SET_EVENT_MASK) {
		hdlr = diag_cmd_update_event_mask;
		driver->set_mask_cmd = 1;
	} else if (*buf == DIAG_CMD_EVENT_TOGGLE) {
		hdlr = diag_cmd_toggle_events;
		driver->set_mask_cmd = 1;
	}
	if (len >= sizeof(struct diag_pkt_header_t))
		header = (struct diag_pkt_header_t *)buf;
	if (diag_check_multisim_support(header)) {
		/*
		 * Multisim Mask command format
		 * header 4 bytes (cmd_code + subsys_id + subsys_cmd_code)
		 * version 1 byte, id_valid 1 byte, sub_id 4 bytes,
		 * sub_cmd 1 byte
		 */
		if (len < sizeof(struct diag_pkt_header_t) +
			7*sizeof(uint8_t))
			return 0;
		cmd_version = SUBID_CMD;
		subid_valid = *(uint8_t *)(buf +
			sizeof(struct diag_pkt_header_t) +
			sizeof(uint8_t));
		if (subid_valid) {
			subid = *(uint32_t *)(buf +
				sizeof(struct diag_pkt_header_t) +
				2*sizeof(uint8_t));
			subid_index = diag_check_subid_mask_index(subid, pid);
		}
		if (subid_valid && (subid_index < 0))
			return 0;
		if (header->subsys_cmd_code == DIAG_SUB_SYS_CMD_MSG) {
			sub_cmd = *(uint8_t *)(buf +
				sizeof(struct diag_pkt_header_t) +
				6*sizeof(uint8_t));
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
				driver->set_mask_cmd = 1;
				break;
			case DIAG_CMD_OP_SET_ALL_MSG_MASK:
				hdlr = diag_cmd_set_all_msg_mask;
				driver->set_mask_cmd = 1;
				break;
			}
		} else if (header->subsys_cmd_code == DIAG_SUB_SYS_CMD_LOG) {
			sub_cmd = *(uint8_t *)(buf +
				sizeof(struct diag_pkt_header_t) +
				6*sizeof(uint8_t));
			switch (sub_cmd) {
			case DIAG_CMD_OP_LOG_DISABLE:
				hdlr = diag_cmd_disable_log_mask;
				break;
			case DIAG_CMD_OP_GET_LOG_RANGE:
				hdlr = diag_cmd_get_log_range;
				break;
			case DIAG_CMD_OP_SET_LOG_MASK:
				hdlr = diag_cmd_set_log_mask;
				driver->set_mask_cmd = 1;
				break;
			case DIAG_CMD_OP_GET_LOG_MASK:
				hdlr = diag_cmd_get_log_mask;
				break;
			}
		} else if (header->subsys_cmd_code == DIAG_SUB_SYS_CMD_EVENT) {
			sub_cmd = *(uint8_t *)(buf +
				sizeof(struct diag_pkt_header_t) +
				6*sizeof(uint8_t));
			switch (sub_cmd) {
			case DIAG_CMD_OP_GET_EVENT_MSK:
				hdlr = diag_cmd_get_event_mask;
				break;
			case DIAG_CMD_OP_SET_EVENT_MSK:
				hdlr = diag_cmd_update_event_mask;
				driver->set_mask_cmd = 1;
				break;
			case DIAG_CMD_OP_EVENT_TOGGLE:
				hdlr = diag_cmd_toggle_events;
				driver->set_mask_cmd = 1;
				break;
			}
		}
	}

	if (hdlr)
		size = hdlr(buf, len, driver->apps_rsp_buf,
			    DIAG_MAX_RSP_SIZE, pid, cmd_version);

	return (size > 0) ? size : 0;
}

static int __diag_multisim_mask_init(struct diag_mask_info *mask_info,
		int mask_len, int subid_index)
{
	struct diag_multisim_masks *temp = NULL;
	struct diag_multisim_masks *ms_ptr = NULL;

	if (!mask_info || mask_len <= 0 || subid_index < 0)
		return -EINVAL;

	if (mask_len > 0) {
		temp = kzalloc(sizeof(struct diag_multisim_masks), GFP_KERNEL);
		if (!temp)
			return -ENOMEM;
		kmemleak_not_leak(temp);
		temp->sub_ptr = kzalloc(mask_len, GFP_KERNEL);
		if (!temp->sub_ptr) {
			kfree(temp);
			temp = NULL;
			return -ENOMEM;
		}
		kmemleak_not_leak(temp->sub_ptr);
		temp->next = NULL;

		if (mask_info->ms_ptr) {
			ms_ptr = mask_info->ms_ptr;
			while (ms_ptr->next)
				ms_ptr = ms_ptr->next;
			ms_ptr->next = temp;
		} else {
			mask_info->ms_ptr = temp;
		}
	}

	return 0;
}

static int diag_multisim_msg_mask_init(int subid_index,
		struct diag_md_session_t *info)
{
	int err = 0;
	struct diag_mask_info *mask_info = NULL;

	mask_info = (!info) ? &msg_mask : info->msg_mask;

	err = __diag_multisim_mask_init(mask_info, MSG_MASK_SIZE,
			subid_index);
	if (err)
		return err;

	err = diag_create_msg_mask_table(subid_index);
	if (err) {
		pr_err("diag: Unable to create msg masks, err: %d\n", err);
		return err;
	}

	return 0;
}

static int diag_multisim_log_mask_init(int subid_index,
		struct diag_md_session_t *info)
{
	int err_no = 0;
	struct diag_mask_info *mask_info = NULL;

	mask_info = (!info) ? &log_mask : info->log_mask;

	err_no = __diag_multisim_mask_init(mask_info, LOG_MASK_SIZE,
			subid_index);
	if (err_no)
		goto err;

	err_no = diag_create_log_mask_table(subid_index);
	if (err_no)
		goto err;
err:
	return err_no;
}

static int diag_multisim_event_mask_init(int subid_index,
		struct diag_md_session_t *info)
{
	int err = 0;
	struct diag_mask_info *mask_info = NULL;

	mask_info = (!info) ? &event_mask : info->event_mask;

	err = __diag_multisim_mask_init(mask_info, EVENT_MASK_SIZE,
			subid_index);

	return err;
}

int diag_check_subid_mask_index(uint32_t subid, int pid)
{
	int err = 0, i = 0;
	struct diag_md_session_t *info = NULL;

	for (i = 0; (i < MAX_SIM_NUM) && (diag_subid_info[i] != INVALID_INDEX);
		i++) {
		if (diag_subid_info[i] == subid)
			return i;
	}
	if (i == MAX_SIM_NUM) {
		pr_err("diag: Reached maximum number of subid supported: %d\n",
				MAX_SIM_NUM);
		return -EINVAL;
	}

	diag_subid_info[i] = subid;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);

	err = diag_multisim_msg_mask_init(i, info);
	if (err)
		goto fail;
	err = diag_multisim_log_mask_init(i, info);
	if (err)
		goto fail;
	err = diag_multisim_event_mask_init(i, info);
	if (err)
		goto fail;

	mutex_unlock(&driver->md_session_lock);
	return i;
fail:
	mutex_unlock(&driver->md_session_lock);
	pr_err("diag: Could not initialize diag mask for subid: %d buffers\n",
		subid);
	return -ENOMEM;
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
