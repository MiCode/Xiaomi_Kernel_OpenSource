/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/diagchar.h>
#include <linux/platform_device.h>
#include <linux/kmemleak.h>
#include <linux/delay.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
#include "diagfwd_hsic.h"
#include "diag_dci.h"
#include "diagmem.h"
#include "diag_masks.h"

#define FEATURE_SUPPORTED(x)	((feature_mask << (i * 8)) & (1 << x))

/* tracks which peripheral is undergoing SSR */
static uint16_t reg_dirty;

void diag_clean_reg_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						struct diag_smd_info,
						diag_notify_update_smd_work);
	if (!smd_info)
		return;

	pr_debug("diag: clean registration for peripheral: %d\n",
		smd_info->peripheral);

	reg_dirty |= smd_info->peripheral_mask;
	diag_clear_reg(smd_info->peripheral);
	reg_dirty ^= smd_info->peripheral_mask;

	/* Reset the feature mask flag */
	driver->rcvd_feature_mask[smd_info->peripheral] = 0;

	smd_info->notify_context = 0;
}

void diag_cntl_smd_work_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						struct diag_smd_info,
						diag_general_smd_work);

	if (!smd_info || smd_info->type != SMD_CNTL_TYPE)
		return;

	if (smd_info->general_context == UPDATE_PERIPHERAL_STM_STATE) {
		if (driver->peripheral_supports_stm[smd_info->peripheral] ==
								ENABLE_STM) {
			int status = 0;
			int index = smd_info->peripheral;
			status = diag_send_stm_state(smd_info,
				(uint8_t)(driver->stm_state_requested[index]));
			if (status == 1)
				driver->stm_state[index] =
					driver->stm_state_requested[index];
		}
	}
	smd_info->general_context = 0;
}

void diag_cntl_stm_notify(struct diag_smd_info *smd_info, int action)
{
	if (!smd_info || smd_info->type != SMD_CNTL_TYPE)
		return;

	if (action == CLEAR_PERIPHERAL_STM_STATE) {
		driver->peripheral_supports_stm[smd_info->peripheral] =
								DISABLE_STM;
		/*
		 * Turn off STM for now until such time as the
		 * tools can support SSR
		 */
		driver->stm_state[smd_info->peripheral] = DISABLE_STM;
		driver->stm_state_requested[smd_info->peripheral] = DISABLE_STM;
	}
}

static void enable_stm_feature(struct diag_smd_info *smd_info)
{
	driver->peripheral_supports_stm[smd_info->peripheral] = ENABLE_STM;
	smd_info->general_context = UPDATE_PERIPHERAL_STM_STATE;
	queue_work(driver->diag_cntl_wq, &(smd_info->diag_general_smd_work));
}

static void process_hdlc_encoding_feature(struct diag_smd_info *smd_info)
{
	/*
	 * Check if apps supports hdlc encoding and the
	 * peripheral supports apps hdlc encoding
	 */
	if (driver->supports_apps_hdlc_encoding) {
		driver->smd_data[smd_info->peripheral].encode_hdlc =
						ENABLE_APPS_HDLC_ENCODING;
		if (driver->separate_cmdrsp[smd_info->peripheral] &&
			smd_info->peripheral < NUM_SMD_CMD_CHANNELS)
			driver->smd_cmd[smd_info->peripheral].encode_hdlc =
						ENABLE_APPS_HDLC_ENCODING;
	} else {
		driver->smd_data[smd_info->peripheral].encode_hdlc =
						DISABLE_APPS_HDLC_ENCODING;
		if (driver->separate_cmdrsp[smd_info->peripheral] &&
			smd_info->peripheral < NUM_SMD_CMD_CHANNELS)
			driver->smd_cmd[smd_info->peripheral].encode_hdlc =
						DISABLE_APPS_HDLC_ENCODING;
	}
}

static void process_command_registration(uint8_t *buf, uint32_t len,
					 struct diag_smd_info *smd_info)
{
	uint8_t *ptr = buf;
	int i;
	int header_len = sizeof(struct diag_ctrl_cmd_reg);
	int read_len = 0;
	struct bindpkt_params_per_process *pkt_params = NULL;
	struct bindpkt_params *temp = NULL;
	struct diag_ctrl_cmd_reg *reg = NULL;
	struct cmd_code_range *range = NULL;

	/*
	 * Perform Basic sanity. The len field is the size of the data payload.
	 * This doesn't include the header size.
	 */
	if (!buf || !smd_info || len == 0)
		return;

	/* Peripheral undergoing SSR should not record new registration */
	if (reg_dirty & smd_info->peripheral_mask) {
		pr_err("diag: dropping command registration from peripheral %d\n",
		       smd_info->peripheral);
		return;
	}

	reg = (struct diag_ctrl_cmd_reg *)ptr;
	ptr += header_len;

	if (reg->count_entries == 0) {
		pr_debug("diag: In %s, received reg tbl with no entries\n",
			 __func__);
		return;
	}

	pkt_params = kzalloc(sizeof(struct bindpkt_params_per_process),
			     GFP_KERNEL);
	if (!pkt_params) {
		pr_err("diag: In %s, unable to allocate memory for new command table entry\n",
		       __func__);
		return;
	}
	pkt_params->count = reg->count_entries;
	pkt_params->params = kzalloc(pkt_params->count *
				     sizeof(struct bindpkt_params),
				     GFP_KERNEL);
	if (!pkt_params->params) {
		pr_err("diag: In %s, Memory alloc fail for cmd_code: %d, subsys: %d\n",
		       __func__, reg->cmd_code, reg->subsysid);
		kfree(pkt_params);
		return;
	}

	temp = pkt_params->params;
	for (i = 0; i < reg->count_entries && read_len < len; i++, temp++) {
		temp->cmd_code = reg->cmd_code;
		temp->subsys_id = reg->subsysid;
		temp->client_id = smd_info->peripheral;
		temp->proc_id = NON_APPS_PROC;
		range = (struct cmd_code_range *)ptr;
		temp->cmd_code_lo = range->cmd_code_lo;
		temp->cmd_code_hi = range->cmd_code_hi;
		ptr += sizeof(struct cmd_code_range);
		read_len += sizeof(struct cmd_code_range);
	}

	diagchar_ioctl(NULL, DIAG_IOCTL_COMMAND_REG, (unsigned long)pkt_params);
	kfree(pkt_params->params);
	kfree(pkt_params);
}

static void process_incoming_feature_mask(uint8_t *buf, uint32_t len,
					  struct diag_smd_info *smd_info)
{
	int i;
	int header_len = sizeof(struct diag_ctrl_feature_mask);
	int read_len = 0;
	struct diag_ctrl_feature_mask *header = NULL;
	uint32_t feature_mask_len = 0;
	uint32_t feature_mask = 0;
	uint8_t *ptr = buf;

	if (!buf || !smd_info || len == 0)
		return;

	header = (struct diag_ctrl_feature_mask *)ptr;
	ptr += header_len;
	feature_mask_len = header->feature_mask_len;

	if (feature_mask_len == 0) {
		pr_debug("diag: In %s, received invalid feature mask from peripheral %d\n",
			 __func__, smd_info->peripheral);
		return;
	}

	if (feature_mask_len > FEATURE_MASK_LEN) {
		pr_alert("diag: Receiving feature mask length more than Apps support\n");
		feature_mask_len = FEATURE_MASK_LEN;
	}

	driver->rcvd_feature_mask[smd_info->peripheral] = 1;

	for (i = 0; i < feature_mask_len && read_len < len; i++) {
		feature_mask = *(uint8_t *)ptr;
		driver->peripheral_feature[smd_info->peripheral][i] =
								feature_mask;
		ptr += sizeof(uint8_t);
		read_len += sizeof(uint8_t);

		if (FEATURE_SUPPORTED(F_DIAG_LOG_ON_DEMAND_APPS))
			driver->log_on_demand_support = 1;
		if (FEATURE_SUPPORTED(F_DIAG_REQ_RSP_SUPPORT))
			driver->separate_cmdrsp[smd_info->peripheral] = 1;
		if (FEATURE_SUPPORTED(F_DIAG_APPS_HDLC_ENCODE))
			process_hdlc_encoding_feature(smd_info);
		if (FEATURE_SUPPORTED(F_DIAG_STM))
			enable_stm_feature(smd_info);
		if (FEATURE_SUPPORTED(F_DIAG_MASK_CENTRALIZATION))
			driver->mask_centralization[smd_info->peripheral] = 1;
	}
}

static void process_last_event_report(uint8_t *buf, uint32_t len,
				      struct diag_smd_info *smd_info)
{
	struct diag_ctrl_last_event_report *header = NULL;
	uint8_t *ptr = buf;
	uint8_t *temp = NULL;
	uint32_t pkt_len = sizeof(uint32_t) + sizeof(uint16_t);
	uint16_t event_size = 0;

	if (!buf || !smd_info || len != pkt_len)
		return;

	mutex_lock(&event_mask.lock);
	header = (struct diag_ctrl_last_event_report *)ptr;
	event_size = ((header->event_last_id / 8) + 1);
	if (event_size >= driver->event_mask_size) {
		pr_debug("diag: In %s, receiving event mask size more that Apps can handle\n",
			 __func__);
		temp = krealloc(driver->event_mask->ptr, event_size,
				GFP_KERNEL);
		if (!temp) {
			pr_err("diag: In %s, unable to reallocate event mask to support events from %d\n",
			       __func__, smd_info->peripheral);
			goto err;
		}
		driver->event_mask->ptr = temp;
		driver->event_mask_size = event_size;
	}

	driver->num_event_id[smd_info->peripheral] = header->event_last_id;
	if (header->event_last_id > driver->last_event_id)
		driver->last_event_id = header->event_last_id;
err:
	mutex_unlock(&event_mask.lock);
}

static void process_log_range_report(uint8_t *buf, uint32_t len,
				     struct diag_smd_info *smd_info)
{
	int i;
	int read_len = 0;
	int peripheral = 0;
	int header_len = sizeof(struct diag_ctrl_log_range_report);
	uint8_t *ptr = buf;
	uint8_t *temp = NULL;
	uint32_t mask_size;
	struct diag_ctrl_log_range_report *header = NULL;
	struct diag_ctrl_log_range *log_range = NULL;
	struct diag_log_mask_t *mask_ptr = NULL;

	if (!buf || !smd_info || len < 0)
		return;

	peripheral = smd_info->peripheral;
	header = (struct diag_ctrl_log_range_report *)ptr;
	ptr += header_len;
	read_len += header_len;

	mutex_lock(&log_mask.lock);
	driver->num_equip_id[peripheral] = header->num_ranges;
	for (i = 0; i < header->num_ranges && read_len < len; i++) {
		log_range = (struct diag_ctrl_log_range *)ptr;
		ptr += sizeof(struct diag_ctrl_log_range);
		read_len += sizeof(struct diag_ctrl_log_range);

		if (log_range->equip_id >= MAX_EQUIP_ID) {
			pr_err("diag: receiving log equip id %d more than supported equip id: %d from peripheral: %d\n",
			       log_range->equip_id, MAX_EQUIP_ID, peripheral);
			continue;
		}
		mask_ptr = (struct diag_log_mask_t *)log_mask.ptr;
		mask_ptr = &mask_ptr[log_range->equip_id];
		mask_size = LOG_ITEMS_TO_SIZE(log_range->num_items);
		if (mask_size < mask_ptr->range)
			goto proceed;

		temp = krealloc(mask_ptr->ptr, mask_size, GFP_KERNEL);
		if (!temp) {
			pr_err("diag: In %s, Unable to reallocate log mask ptr to size: %d, equip_id: %d\n",
			       __func__, mask_size, log_range->equip_id);
			continue;
		}
		mask_ptr->ptr = temp;
		mask_ptr->range = mask_size;
proceed:
		if (log_range->num_items > mask_ptr->num_items)
			mask_ptr->num_items = log_range->num_items;
	}
	mutex_unlock(&log_mask.lock);
}

static int update_msg_mask_tbl_entry(struct diag_msg_mask_t *mask,
				     struct diag_ssid_range_t *range)
{
	uint32_t temp_range;
	uint32_t *temp = NULL;

	if (!mask || !range)
		return -EIO;
	if (range->ssid_last < range->ssid_first) {
		pr_err("diag: In %s, invalid ssid range, first: %d, last: %d\n",
		       __func__, range->ssid_first, range->ssid_last);
		return -EINVAL;
	}
	if (range->ssid_last >= mask->ssid_last) {
		temp_range = range->ssid_last - mask->ssid_first + 1;
		temp = krealloc(mask->ptr, temp_range * sizeof(uint32_t),
				GFP_KERNEL);
		if (!temp)
			return -ENOMEM;
		mask->ptr = temp;
		mask->ssid_last = range->ssid_last;
		mask->range = temp_range;
	}

	return 0;
}

static void process_ssid_range_report(uint8_t *buf, uint32_t len,
				      struct diag_smd_info *smd_info)
{
	int i;
	int j;
	int read_len = 0;
	int found = 0;
	int new_size = 0;
	int err = 0;
	struct diag_ctrl_ssid_range_report *header = NULL;
	struct diag_ssid_range_t *ssid_range = NULL;
	int header_len = sizeof(struct diag_ctrl_ssid_range_report);
	struct diag_msg_mask_t *mask_ptr = NULL;
	uint8_t *ptr = buf;
	uint8_t *temp = NULL;
	uint32_t min_len = header_len - sizeof(struct diag_ctrl_pkt_header_t);

	if (!buf || !smd_info || len < min_len)
		return;

	header = (struct diag_ctrl_ssid_range_report *)ptr;
	ptr += header_len;
	read_len += header_len;

	mutex_lock(&msg_mask.lock);
	driver->max_ssid_count[smd_info->peripheral] = header->count;
	for (i = 0; i < header->count && read_len < len; i++) {
		ssid_range = (struct diag_ssid_range_t *)ptr;
		ptr += sizeof(struct diag_ssid_range_t);
		read_len += sizeof(struct diag_ssid_range_t);
		mask_ptr = (struct diag_msg_mask_t *)msg_mask.ptr;
		found = 0;
		for (j = 0; j < driver->msg_mask_tbl_count; j++, mask_ptr++) {
			if (mask_ptr->ssid_first != ssid_range->ssid_first)
				continue;
			err = update_msg_mask_tbl_entry(mask_ptr, ssid_range);
			if (err == -ENOMEM) {
				pr_err("diag: In %s, unable to increase the msg mask table range\n",
				       __func__);
			}
			found = 1;
			break;
		}

		if (found)
			continue;

		new_size = (driver->msg_mask_tbl_count + 1) *
			   sizeof(struct diag_msg_mask_t);
		temp = krealloc(msg_mask.ptr, new_size, GFP_KERNEL);
		if (!temp) {
			pr_err("diag: In %s, Unable to add new ssid table to msg mask, ssid first: %d, last: %d\n",
			       __func__, ssid_range->ssid_first,
			       ssid_range->ssid_last);
			continue;
		}
		msg_mask.ptr = temp;
		err = diag_create_msg_mask_table_entry(mask_ptr, ssid_range);
		if (err) {
			pr_err("diag: In %s, Unable to create a new msg mask table entry, first: %d last: %d err: %d\n",
			       __func__, ssid_range->ssid_first,
			       ssid_range->ssid_last, err);
			continue;
		}
		driver->msg_mask_tbl_count += 1;
	}
	mutex_unlock(&msg_mask.lock);
}

static void diag_build_time_mask_update(uint8_t *buf,
					struct diag_ssid_range_t *range)
{
	int i;
	int j;
	int num_items = 0;
	int err = 0;
	int found = 0;
	int new_size = 0;
	uint8_t *temp = NULL;
	uint32_t *mask_ptr = (uint32_t *)buf;
	uint32_t *dest_ptr = NULL;
	struct diag_msg_mask_t *build_mask = NULL;

	if (!range || !buf)
		return;

	if (range->ssid_last < range->ssid_first) {
		pr_err("diag: In %s, invalid ssid range, first: %d, last: %d\n",
		       __func__, range->ssid_first, range->ssid_last);
		return;
	}

	build_mask = (struct diag_msg_mask_t *)(driver->build_time_mask->ptr);
	num_items = range->ssid_last - range->ssid_first + 1;

	mutex_lock(&driver->build_time_mask->lock);
	for (i = 0; i < driver->msg_mask_tbl_count; i++, build_mask++) {
		if (build_mask->ssid_first != range->ssid_first)
			continue;
		found = 1;
		err = update_msg_mask_tbl_entry(build_mask, range);
		if (err == -ENOMEM) {
			pr_err("diag: In %s, unable to increase the msg build mask table range\n",
			       __func__);
		}
		dest_ptr = build_mask->ptr;
		for (j = 0; j < build_mask->range; j++, mask_ptr++, dest_ptr++)
			*(uint32_t *)dest_ptr |= *mask_ptr;
		break;
	}

	if (found)
		goto end;
	new_size = (driver->msg_mask_tbl_count + 1) *
		   sizeof(struct diag_msg_mask_t);
	temp = krealloc(driver->build_time_mask->ptr, new_size, GFP_KERNEL);
	if (!temp) {
		pr_err("diag: In %s, unable to create a new entry for build time mask\n",
		       __func__);
		goto end;
	}
	driver->build_time_mask->ptr = temp;
	err = diag_create_msg_mask_table_entry(build_mask, range);
	if (err) {
		pr_err("diag: In %s, Unable to create a new msg mask table entry, err: %d\n",
		       __func__, err);
		goto end;
	}
	driver->msg_mask_tbl_count += 1;
end:
	mutex_unlock(&driver->build_time_mask->lock);
}

static void process_build_mask_report(uint8_t *buf, uint32_t len,
				      struct diag_smd_info *smd_info)
{
	int i;
	int read_len = 0;
	int num_items = 0;
	int header_len = sizeof(struct diag_ctrl_build_mask_report);
	uint8_t *ptr = buf;
	struct diag_ctrl_build_mask_report *header = NULL;
	struct diag_ssid_range_t *range = NULL;

	if (!buf || !smd_info || len < header_len)
		return;

	header = (struct diag_ctrl_build_mask_report *)ptr;
	ptr += header_len;
	read_len += header_len;

	for (i = 0; i < header->count && read_len < len; i++) {
		range = (struct diag_ssid_range_t *)ptr;
		ptr += sizeof(struct diag_ssid_range_t);
		read_len += sizeof(struct diag_ssid_range_t);
		num_items = range->ssid_last - range->ssid_first + 1;
		diag_build_time_mask_update(ptr, range);
		ptr += num_items * sizeof(uint32_t);
		read_len += num_items * sizeof(uint32_t);
	}
}

/* Process the data read from the smd control channel */
int diag_process_smd_cntl_read_data(struct diag_smd_info *smd_info, void *buf,
								int total_recd)
{
	int read_len = 0;
	int header_len = sizeof(struct diag_ctrl_pkt_header_t);
	uint8_t *ptr = buf;
	struct diag_ctrl_pkt_header_t *ctrl_pkt = NULL;

	if (!smd_info || !buf || total_recd <= 0)
		return -EIO;

	while (read_len + header_len < total_recd) {
		ctrl_pkt = (struct diag_ctrl_pkt_header_t *)ptr;
		switch (ctrl_pkt->pkt_id) {
		case DIAG_CTRL_MSG_REG:
			process_command_registration(ptr, ctrl_pkt->len,
						     smd_info);
			break;
		case DIAG_CTRL_MSG_FEATURE:
			process_incoming_feature_mask(ptr, ctrl_pkt->len,
						      smd_info);
			break;
		case DIAG_CTRL_MSG_LAST_EVENT_REPORT:
			process_last_event_report(ptr, ctrl_pkt->len,
						  smd_info);
			break;
		case DIAG_CTRL_MSG_LOG_RANGE_REPORT:
			process_log_range_report(ptr, ctrl_pkt->len, smd_info);
			break;
		case DIAG_CTRL_MSG_SSID_RANGE_REPORT:
			process_ssid_range_report(ptr, ctrl_pkt->len,
						  smd_info);
			break;
		case DIAG_CTRL_MSG_BUILD_MASK_REPORT:
			process_build_mask_report(ptr, ctrl_pkt->len,
						  smd_info);
			break;
		default:
			pr_debug("diag: Control packet %d not supported\n",
				 ctrl_pkt->pkt_id);
		}
		ptr += header_len + ctrl_pkt->len;
		read_len += header_len + ctrl_pkt->len;
	}

	return 0;
}

static int diag_compute_real_time(int idx)
{
	int real_time = MODE_REALTIME;
	if (driver->proc_active_mask == 0) {
		/*
		 * There are no DCI or Memory Device processes. Diag should
		 * be in Real Time mode irrespective of USB connection
		 */
		real_time = MODE_REALTIME;
	} else if (driver->proc_rt_vote_mask[idx] & driver->proc_active_mask) {
		/*
		 * Atleast one process is alive and is voting for Real Time
		 * data - Diag should be in real time mode irrespective of USB
		 * connection.
		 */
		real_time = MODE_REALTIME;
	} else if (driver->usb_connected) {
		/*
		 * If USB is connected, check individual process. If Memory
		 * Device Mode is active, set the mode requested by Memory
		 * Device process. Set to realtime mode otherwise.
		 */
		if ((driver->proc_rt_vote_mask[idx] &
						DIAG_PROC_MEMORY_DEVICE) == 0)
			real_time = MODE_NONREALTIME;
		else
			real_time = MODE_REALTIME;
	} else {
		/*
		 * We come here if USB is not connected and the active
		 * processes are voting for Non realtime mode.
		 */
		real_time = MODE_NONREALTIME;
	}
	return real_time;
}

static void diag_create_diag_mode_ctrl_pkt(unsigned char *dest_buf,
					   int real_time)
{
	struct diag_ctrl_msg_diagmode diagmode;
	int msg_size = sizeof(struct diag_ctrl_msg_diagmode);

	if (!dest_buf)
		return;

	diagmode.ctrl_pkt_id = DIAG_CTRL_MSG_DIAGMODE;
	diagmode.ctrl_pkt_data_len = DIAG_MODE_PKT_LEN;
	diagmode.version = 1;
	diagmode.sleep_vote = real_time ? 1 : 0;
	/*
	 * 0 - Disables real-time logging (to prevent
	 *     frequent APPS wake-ups, etc.).
	 * 1 - Enable real-time logging
	 */
	diagmode.real_time = real_time;
	diagmode.use_nrt_values = 0;
	diagmode.commit_threshold = 0;
	diagmode.sleep_threshold = 0;
	diagmode.sleep_time = 0;
	diagmode.drain_timer_val = 0;
	diagmode.event_stale_timer_val = 0;

	memcpy(dest_buf, &diagmode, msg_size);
}

void diag_update_proc_vote(uint16_t proc, uint8_t vote, int index)
{
	int i;

	mutex_lock(&driver->real_time_mutex);
	if (vote)
		driver->proc_active_mask |= proc;
	else {
		driver->proc_active_mask &= ~proc;
		if (index == ALL_PROC) {
			for (i = 0; i < DIAG_NUM_PROC; i++)
				driver->proc_rt_vote_mask[i] |= proc;
		} else {
			driver->proc_rt_vote_mask[index] |= proc;
		}
	}
	mutex_unlock(&driver->real_time_mutex);
}

void diag_update_real_time_vote(uint16_t proc, uint8_t real_time, int index)
{
	int i;

	mutex_lock(&driver->real_time_mutex);
	if (index == ALL_PROC) {
		for (i = 0; i < DIAG_NUM_PROC; i++) {
			if (real_time)
				driver->proc_rt_vote_mask[i] |= proc;
			else
				driver->proc_rt_vote_mask[i] &= ~proc;
		}
	} else {
		if (real_time)
			driver->proc_rt_vote_mask[index] |= proc;
		else
			driver->proc_rt_vote_mask[index] &= ~proc;
	}
	mutex_unlock(&driver->real_time_mutex);
}


#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
static void diag_send_diag_mode_update_by_hsic(int index, int real_time)
{
	unsigned char *buf = NULL;
	int err = 0;
	struct diag_dci_header_t dci_header;
	int dci_header_size = sizeof(struct diag_dci_header_t);
	int msg_size = sizeof(struct diag_ctrl_msg_diagmode);
	uint32_t write_len = 0;

	if (index < 0 || index > MAX_HSIC_DCI_CH) {
		pr_err("diag: Invalid HSIC channel in %s, index: %d\n",
							__func__, index);
		return;
	}

	if (real_time != MODE_REALTIME && real_time != MODE_NONREALTIME) {
		pr_err("diag: Invalid real time value in %s, type: %d\n",
							__func__, real_time);
		return;
	}

	if (!diag_hsic_dci[index].hsic_ch) {
		pr_debug("diag: In %s, hsic dci channel %d is not enabled.\n",
							__func__, index);
		return;
	}

	buf = dci_get_buffer_from_bridge(index);
	if (!buf) {
		pr_err("diag: In %s, unable to get dci buffers to write data\n",
			__func__);
		return;
	}
	/* Frame the DCI header */
	dci_header.start = CONTROL_CHAR;
	dci_header.version = 1;
	dci_header.length = msg_size + 1;
	dci_header.cmd_code = DCI_CONTROL_PKT_CODE;

	memcpy(buf + write_len, &dci_header, dci_header_size);
	write_len += dci_header_size;
	diag_create_diag_mode_ctrl_pkt(buf + write_len, real_time);
	write_len += msg_size;
	*(buf + write_len) = CONTROL_CHAR; /* End Terminator */
	write_len += sizeof(uint8_t);
	err = diag_dci_write_bridge(index, buf, write_len);
	if (err != write_len) {
		pr_err("diag: cannot send nrt mode ctrl pkt, err: %d\n", err);
		diagmem_free(driver, buf, POOL_TYPE_HSIC_DCI_WRITE + index);
	} else {
		driver->real_time_mode[index + 1] = real_time;
	}
}
#else
static inline void diag_send_diag_mode_update_by_hsic(int index, int real_time)
{
}
#endif

#ifdef CONFIG_DIAG_OVER_USB
void diag_real_time_work_fn(struct work_struct *work)
{
	int temp_real_time = MODE_REALTIME, i, j;

	for (i = 0; i < DIAG_NUM_PROC; i++) {
		temp_real_time = diag_compute_real_time(i);
		if (temp_real_time == driver->real_time_mode[i]) {
			pr_debug("diag: did not update real time mode on proc %d, already in the req mode %d",
				i, temp_real_time);
			continue;
		}

		if (i == DIAG_LOCAL_PROC) {
			for (j = 0; j < NUM_SMD_CONTROL_CHANNELS; j++)
				diag_send_diag_mode_update_by_smd(
					&driver->smd_cntl[j], temp_real_time);
		} else {
			diag_send_diag_mode_update_by_hsic(i - 1,
							   temp_real_time);
		}
	}

	if (driver->real_time_update_busy > 0)
		driver->real_time_update_busy--;
}
#else
void diag_real_time_work_fn(struct work_struct *work)
{
	int temp_real_time = MODE_REALTIME, i, j;

	for (i = 0; i < DIAG_NUM_PROC; i++) {
		if (driver->proc_active_mask == 0) {
			/*
			 * There are no DCI or Memory Device processes.
			 * Diag should be in Real Time mode.
			 */
			temp_real_time = MODE_REALTIME;
		} else if (!(driver->proc_rt_vote_mask[i] &
						driver->proc_active_mask)) {
			/* No active process is voting for real time mode */
			temp_real_time = MODE_NONREALTIME;
		}
		if (temp_real_time == driver->real_time_mode[i]) {
			pr_debug("diag: did not update real time mode on proc %d, already in the req mode %d",
				i, temp_real_time);
			continue;
		}

		if (i == DIAG_LOCAL_PROC) {
			for (j = 0; j < NUM_SMD_CONTROL_CHANNELS; j++)
				diag_send_diag_mode_update_by_smd(
					&driver->smd_cntl[j], temp_real_time);
		} else {
			diag_send_diag_mode_update_by_hsic(i - 1,
								temp_real_time);
		}
	}

	if (driver->real_time_update_busy > 0)
		driver->real_time_update_busy--;
}
#endif

void diag_send_diag_mode_update_by_smd(struct diag_smd_info *smd_info,
							int real_time)
{
	char buf[sizeof(struct diag_ctrl_msg_diagmode)];
	int msg_size = sizeof(struct diag_ctrl_msg_diagmode);
	struct diag_smd_info *data = NULL;
	int err = 0;

	if (!smd_info || smd_info->type != SMD_CNTL_TYPE) {
		pr_err("diag: In %s, invalid channel info, smd_info: %p type: %d\n",
					__func__, smd_info,
					((smd_info) ? smd_info->type : -1));
		return;
	}

	if (smd_info->peripheral < MODEM_DATA ||
					smd_info->peripheral > WCNSS_DATA) {
		pr_err("diag: In %s, invalid peripheral %d\n", __func__,
							smd_info->peripheral);
		return;
	}

	data = &driver->smd_data[smd_info->peripheral];
	if (!data)
		return;

	diag_create_diag_mode_ctrl_pkt(buf, real_time);

	mutex_lock(&driver->diag_cntl_mutex);
	err = diag_smd_write(smd_info, buf, msg_size);
	if (err) {
		pr_err("diag: In %s, unable to write to smd, peripheral: %d, type: %d, len: %d, err: %d\n",
		       __func__, smd_info->peripheral, smd_info->type,
		       msg_size, err);
	} else {
		driver->real_time_mode[DIAG_LOCAL_PROC] = real_time;
	}

	mutex_unlock(&driver->diag_cntl_mutex);
}

int diag_send_stm_state(struct diag_smd_info *smd_info,
			  uint8_t stm_control_data)
{
	struct diag_ctrl_msg_stm stm_msg;
	int msg_size = sizeof(struct diag_ctrl_msg_stm);
	int success = 0;
	int err = 0;

	if (!smd_info || (smd_info->type != SMD_CNTL_TYPE) ||
		(driver->peripheral_supports_stm[smd_info->peripheral] ==
								DISABLE_STM)) {
		return -EINVAL;
	}

	if (smd_info->ch) {
		stm_msg.ctrl_pkt_id = 21;
		stm_msg.ctrl_pkt_data_len = 5;
		stm_msg.version = 1;
		stm_msg.control_data = stm_control_data;
		err = diag_smd_write(smd_info, &stm_msg, msg_size);
		if (err) {
			pr_err("diag: In %s, unable to write to smd, peripheral: %d, type: %d, len: %d, err: %d\n",
			       __func__, smd_info->peripheral, smd_info->type,
			       msg_size, err);
		} else {
			success = 1;
		}
	} else {
		pr_err("diag: In %s, ch invalid, STM update on proc %d\n",
				__func__, smd_info->peripheral);
	}
	return success;
}

static int diag_smd_cntl_probe(struct platform_device *pdev)
{
	int r = 0;
	int index = -1;
	const char *channel_name = NULL;

	/* open control ports only on 8960 & newer targets */
	if (chk_apps_only()) {
		if (pdev->id == SMD_APPS_MODEM) {
			index = MODEM_DATA;
			channel_name = "DIAG_CNTL";
		}
		else if (pdev->id == SMD_APPS_QDSP) {
			index = LPASS_DATA;
			channel_name = "DIAG_CNTL";
		}
		else if (pdev->id == SMD_APPS_WCNSS) {
			index = WCNSS_DATA;
			channel_name = "APPS_RIVA_CTRL";
		}

		if (index != -1) {
			r = smd_named_open_on_edge(channel_name,
				pdev->id,
				&driver->smd_cntl[index].ch,
				&driver->smd_cntl[index],
				diag_smd_notify);
			driver->smd_cntl[index].ch_save =
				driver->smd_cntl[index].ch;
			diag_smd_buffer_init(&driver->smd_cntl[index]);
		}
		pr_debug("diag: In %s, open SMD CNTL port, Id = %d, r = %d\n",
			__func__, pdev->id, r);
	}

	return 0;
}

static int diagfwd_cntl_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diagfwd_cntl_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diagfwd_cntl_dev_pm_ops = {
	.runtime_suspend = diagfwd_cntl_runtime_suspend,
	.runtime_resume = diagfwd_cntl_runtime_resume,
};

static struct platform_driver msm_smd_ch1_cntl_driver = {

	.probe = diag_smd_cntl_probe,
	.driver = {
		.name = "DIAG_CNTL",
		.owner = THIS_MODULE,
		.pm   = &diagfwd_cntl_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_lite_cntl_driver = {

	.probe = diag_smd_cntl_probe,
	.driver = {
		.name = "APPS_RIVA_CTRL",
		.owner = THIS_MODULE,
		.pm   = &diagfwd_cntl_dev_pm_ops,
	},
};

int diagfwd_cntl_init(void)
{
	int ret;
	int i;

	reg_dirty = 0;
	driver->polling_reg_flag = 0;
	driver->log_on_demand_support = 1;
	driver->diag_cntl_wq = create_singlethread_workqueue("diag_cntl_wq");
	if (!driver->diag_cntl_wq)
		goto err;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
		ret = diag_smd_constructor(&driver->smd_cntl[i], i,
							SMD_CNTL_TYPE);
		if (ret)
			goto err;
	}

	platform_driver_register(&msm_smd_ch1_cntl_driver);
	platform_driver_register(&diag_smd_lite_cntl_driver);

	return 0;
err:
	pr_err("diag: Could not initialize diag buffers");

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_cntl[i]);

	if (driver->diag_cntl_wq)
		destroy_workqueue(driver->diag_cntl_wq);
	return -ENOMEM;
}

void diagfwd_cntl_exit(void)
{
	int i;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_cntl[i]);

	destroy_workqueue(driver->diag_cntl_wq);
	destroy_workqueue(driver->diag_real_time_wq);

	platform_driver_unregister(&msm_smd_ch1_cntl_driver);
	platform_driver_unregister(&diag_smd_lite_cntl_driver);
}
