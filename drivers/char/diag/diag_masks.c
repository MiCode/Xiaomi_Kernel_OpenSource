/* Copyright (c) 2008-2012, The Linux Foundation. All rights reserved.
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

int diag_event_config;
int diag_event_num_bytes;

#define ALL_EQUIP_ID		100
#define ALL_SSID		-1
#define MAX_SSID_PER_RANGE	100

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

#define WAIT_FOR_SMD(num_delays, delay_time)		\
do {							\
	int count;					\
	for (count = 0; count < (num_delays); count++)	\
		udelay((delay_time));			\
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
			if (CHK_OVERFLOW(ptr_buffer_start, ptr, ptr_buffer_end,
					  (((end - start)+1)*4))) {
				pr_debug("diag: update ssid start %d, end %d\n",
								 start, end);
				memcpy(ptr, buf , ((end - start)+1)*4);
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
	mutex_unlock(&driver->diagchar_mutex);
	diag_print_mask_table();
}

void diag_toggle_event_mask(int toggle)
{
	uint8_t *ptr = driver->event_masks;

	mutex_lock(&driver->diagchar_mutex);
	if (toggle)
		memset(ptr, 0xFF, EVENT_MASK_SIZE);
	else
		memset(ptr, 0, EVENT_MASK_SIZE);
	mutex_unlock(&driver->diagchar_mutex);
}


static void diag_update_event_mask(uint8_t *buf, int toggle, int num_bytes)
{
	uint8_t *ptr = driver->event_masks;
	uint8_t *temp = buf + 2;

	mutex_lock(&driver->diagchar_mutex);
	if (!toggle)
		memset(ptr, 0 , EVENT_MASK_SIZE);
	else
		if (CHK_OVERFLOW(ptr, ptr,
				 ptr+EVENT_MASK_SIZE, num_bytes))
			memcpy(ptr, temp , num_bytes);
		else
			printk(KERN_CRIT "Not enough buffer space for EVENT_MASK\n");
	mutex_unlock(&driver->diagchar_mutex);
}

static void diag_disable_log_mask(void)
{
	int i = 0;
	struct mask_info *parse_ptr = (struct mask_info *)(driver->log_masks);

	pr_debug("diag: disable log masks\n");
	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < MAX_EQUIP_ID; i++) {
		pr_debug("diag: equip id %d\n", parse_ptr->equip_id);
		if (!(parse_ptr->equip_id)) /* Reached a null entry */
			break;
		memset(driver->log_masks + parse_ptr->index, 0,
			    (parse_ptr->num_items + 7)/8);
		parse_ptr++;
	}
	mutex_unlock(&driver->diagchar_mutex);
}

int chk_equip_id_and_mask(int equip_id, uint8_t *buf)
{
	int i = 0, flag = 0, num_items, offset;
	unsigned char *ptr_data;
	struct mask_info *ptr = (struct mask_info *)(driver->log_masks);

	pr_debug("diag: received equip id = %d\n", equip_id);
	/* Check if this is valid equipment ID */
	for (i = 0; i < MAX_EQUIP_ID; i++) {
		if ((ptr->equip_id == equip_id) && (ptr->index != 0)) {
			offset = ptr->index;
			num_items = ptr->num_items;
			flag = 1;
			break;
		}
		ptr++;
	}
	if (!flag)
		return -EPERM;
	ptr_data = driver->log_masks + offset;
	memcpy(buf, ptr_data, (num_items+7)/8);
	return 0;
}

static void diag_update_log_mask(int equip_id, uint8_t *buf, int num_items)
{
	uint8_t *temp = buf;
	int i = 0;
	unsigned char *ptr_data;
	int offset = (sizeof(struct mask_info))*MAX_EQUIP_ID;
	struct mask_info *ptr = (struct mask_info *)(driver->log_masks);

	pr_debug("diag: received equip id = %d\n", equip_id);
	mutex_lock(&driver->diagchar_mutex);
	/* Check if we already know index of this equipment ID */
	for (i = 0; i < MAX_EQUIP_ID; i++) {
		if ((ptr->equip_id == equip_id) && (ptr->index != 0)) {
			offset = ptr->index;
			break;
		}
		if ((ptr->equip_id == 0) && (ptr->index == 0)) {
			/* Reached a null entry */
			ptr->equip_id = equip_id;
			ptr->num_items = num_items;
			ptr->index = driver->log_masks_length;
			offset = driver->log_masks_length;
			driver->log_masks_length += ((num_items+7)/8);
			break;
		}
		ptr++;
	}
	ptr_data = driver->log_masks + offset;
	if (CHK_OVERFLOW(driver->log_masks, ptr_data, driver->log_masks
					 + LOG_MASK_SIZE, (num_items+7)/8))
		memcpy(ptr_data, temp , (num_items+7)/8);
	else
		pr_err("diag: Not enough buffer space for LOG_MASK\n");
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_modem_mask_update_fn(struct work_struct *work)
{
	diag_send_msg_mask_update(driver->ch_cntl, ALL_SSID,
					   ALL_SSID, MODEM_PROC);
	diag_send_log_mask_update(driver->ch_cntl, ALL_EQUIP_ID);
	diag_send_event_mask_update(driver->ch_cntl, diag_event_num_bytes);
}

void diag_lpass_mask_update_fn(struct work_struct *work)
{
	diag_send_msg_mask_update(driver->chlpass_cntl, ALL_SSID,
						   ALL_SSID, LPASS_PROC);
	diag_send_log_mask_update(driver->chlpass_cntl, ALL_EQUIP_ID);
	diag_send_event_mask_update(driver->chlpass_cntl, diag_event_num_bytes);
}

void diag_wcnss_mask_update_fn(struct work_struct *work)
{
	diag_send_msg_mask_update(driver->ch_wcnss_cntl, ALL_SSID,
						   ALL_SSID, WCNSS_PROC);
	diag_send_log_mask_update(driver->ch_wcnss_cntl, ALL_EQUIP_ID);
	diag_send_event_mask_update(driver->ch_wcnss_cntl,
						 diag_event_num_bytes);
}

void diag_send_log_mask_update(smd_channel_t *ch, int equip_id)
{
	void *buf = driver->buf_log_mask_update;
	int header_size = sizeof(struct diag_ctrl_log_mask);
	struct mask_info *ptr = (struct mask_info *)driver->log_masks;
	int i, size, wr_size = -ENOMEM, retry_count = 0;

	mutex_lock(&driver->diag_cntl_mutex);
	for (i = 0; i < MAX_EQUIP_ID; i++) {
		size = (ptr->num_items+7)/8;
		/* reached null entry */
		if ((ptr->equip_id == 0) && (ptr->index == 0))
			break;
		driver->log_mask->cmd_type = DIAG_CTRL_MSG_LOG_MASK;
		driver->log_mask->num_items = ptr->num_items;
		driver->log_mask->data_len  = 11 + size;
		driver->log_mask->stream_id = 1; /* 2, if dual stream */
		driver->log_mask->status = 3; /* status for valid mask */
		driver->log_mask->equip_id = ptr->equip_id;
		driver->log_mask->log_mask_size = size;
		/* send only desired update, NOT ALL */
		if (equip_id == ALL_EQUIP_ID || equip_id ==
					 driver->log_mask->equip_id) {
			memcpy(buf, driver->log_mask, header_size);
			memcpy(buf+header_size, driver->log_masks+ptr->index,
									 size);
			if (ch) {
				while (retry_count < 3) {
					wr_size = smd_write(ch, buf,
							 header_size + size);
					if (wr_size == -ENOMEM) {
						retry_count++;
						WAIT_FOR_SMD(5, 2000);
					} else
						break;
				}
				if (wr_size != header_size + size)
					pr_err("diag: log mask update failed %d, tried %d",
						wr_size, header_size + size);
				else
					pr_debug("diag: updated log equip ID %d,len %d\n",
					driver->log_mask->equip_id,
					driver->log_mask->log_mask_size);
			} else
				pr_err("diag: ch not valid for log update\n");
		}
		ptr++;
	}
	mutex_unlock(&driver->diag_cntl_mutex);
}

void diag_send_event_mask_update(smd_channel_t *ch, int num_bytes)
{
	void *buf = driver->buf_event_mask_update;
	int header_size = sizeof(struct diag_ctrl_event_mask);
	int wr_size = -ENOMEM, retry_count = 0;

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
	driver->event_mask->status = 3; /* status for valid mask */
	driver->event_mask->event_config = diag_event_config; /* event config */
	driver->event_mask->event_mask_size = num_bytes;
	memcpy(buf, driver->event_mask, header_size);
	memcpy(buf+header_size, driver->event_masks, num_bytes);
	if (ch) {
		while (retry_count < 3) {
			wr_size = smd_write(ch, buf, header_size + num_bytes);
			if (wr_size == -ENOMEM) {
				retry_count++;
				WAIT_FOR_SMD(5, 2000);
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

void diag_send_msg_mask_update(smd_channel_t *ch, int updated_ssid_first,
						int updated_ssid_last, int proc)
{
	void *buf = driver->buf_msg_mask_update;
	int first, last, actual_last, size = -ENOMEM, retry_count = 0;
	int header_size = sizeof(struct diag_ctrl_msg_mask);
	uint8_t *ptr = driver->msg_masks;

	mutex_lock(&driver->diag_cntl_mutex);
	while (*(uint32_t *)(ptr + 4)) {
		first = *(uint32_t *)ptr;
		ptr += 4;
		last = *(uint32_t *)ptr;
		ptr += 4;
		actual_last = *(uint32_t *)ptr;
		ptr += 4;
		if ((updated_ssid_first >= first && updated_ssid_last <=
			 actual_last) || (updated_ssid_first == ALL_SSID)) {
			/* send f3 mask update */
			driver->msg_mask->cmd_type = DIAG_CTRL_MSG_F3_MASK;
			driver->msg_mask->msg_mask_size = actual_last -
								 first + 1;
			driver->msg_mask->data_len = 11 +
					 4 * (driver->msg_mask->msg_mask_size);
			driver->msg_mask->stream_id = 1; /* 2, if dual stream */
			driver->msg_mask->status = 3; /* status valid mask */
			driver->msg_mask->msg_mode = 0; /* Legcay mode */
			driver->msg_mask->ssid_first = first;
			driver->msg_mask->ssid_last = actual_last;
			memcpy(buf, driver->msg_mask, header_size);
			memcpy(buf+header_size, ptr,
				 4 * (driver->msg_mask->msg_mask_size));
			if (ch) {
				while (retry_count < 3) {
					size = smd_write(ch, buf, header_size +
					 4*(driver->msg_mask->msg_mask_size));
					if (size == -ENOMEM) {
						retry_count++;
						WAIT_FOR_SMD(5, 2000);
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
		}
		ptr += MAX_SSID_PER_RANGE*4;
	}
	mutex_unlock(&driver->diag_cntl_mutex);
}

int diag_process_apps_masks(unsigned char *buf, int len)
{
	int packet_type = 1;
	int i;
	int ssid_first, ssid_last, ssid_range;
	int rt_mask, rt_first_ssid, rt_last_ssid, rt_mask_size;
	uint8_t *rt_mask_ptr;
	int equip_id, num_items;
#if defined(CONFIG_DIAG_OVER_USB)
	int payload_length;
#endif

	/* Set log masks */
	if (*buf == 0x73 && *(int *)(buf+4) == 3) {
		buf += 8;
		/* Read Equip ID and pass as first param below*/
		diag_update_log_mask(*(int *)buf, buf+8, *(int *)(buf+4));
		diag_update_userspace_clients(LOG_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			driver->apps_rsp_buf[0] = 0x73;
			*(int *)(driver->apps_rsp_buf + 4) = 0x3; /* op. ID */
			*(int *)(driver->apps_rsp_buf + 8) = 0x0; /* success */
			payload_length = 8 + ((*(int *)(buf + 4)) + 7)/8;
			for (i = 0; i < payload_length; i++)
				*(int *)(driver->apps_rsp_buf+12+i) = *(buf+i);
			if (driver->ch_cntl)
				diag_send_log_mask_update(driver->ch_cntl,
				*(int *)buf);
			if (driver->chlpass_cntl)
				diag_send_log_mask_update(driver->chlpass_cntl,
				*(int *)buf);
			if (driver->ch_wcnss_cntl)
				diag_send_log_mask_update(driver->ch_wcnss_cntl,
				*(int *)buf);
			encode_rsp_and_send(12 + payload_length - 1);
			return 0;
		}
#endif
	} /* Get log masks */
	else if (*buf == 0x73 && *(int *)(buf+4) == 4) {
#if defined(CONFIG_DIAG_OVER_USB)
		if (!(driver->ch) && chk_apps_only()) {
			equip_id = *(int *)(buf + 8);
			num_items = *(int *)(buf + 12);
			driver->apps_rsp_buf[0] = 0x73;
			driver->apps_rsp_buf[1] = 0x0;
			driver->apps_rsp_buf[2] = 0x0;
			driver->apps_rsp_buf[3] = 0x0;
			*(int *)(driver->apps_rsp_buf + 4) = 0x4;
			if (!chk_equip_id_and_mask(equip_id,
				driver->apps_rsp_buf+20))
				*(int *)(driver->apps_rsp_buf + 8) = 0x0;
			else
				*(int *)(driver->apps_rsp_buf + 8) = 0x1;
			*(int *)(driver->apps_rsp_buf + 12) = equip_id;
			*(int *)(driver->apps_rsp_buf + 16) = num_items;
			encode_rsp_and_send(20+(num_items+7)/8-1);
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
			if (driver->ch_cntl)
				diag_send_log_mask_update(driver->ch_cntl,
				ALL_EQUIP_ID);
			if (driver->chlpass_cntl)
				diag_send_log_mask_update(driver->chlpass_cntl,
				ALL_EQUIP_ID);
			if (driver->ch_wcnss_cntl)
				diag_send_log_mask_update(driver->ch_wcnss_cntl,
				ALL_EQUIP_ID);
			encode_rsp_and_send(11);
			return 0;
		}
#endif
	} /* Get runtime message mask  */
	else if ((*buf == 0x7d) && (*(buf+1) == 0x3)) {
		ssid_first = *(uint16_t *)(buf + 2);
		ssid_last = *(uint16_t *)(buf + 4);
#if defined(CONFIG_DIAG_OVER_USB)
		if (!(driver->ch) && chk_apps_only()) {
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
		ssid_range = 4 * (ssid_last - ssid_first + 1);
		pr_debug("diag: received mask update for ssid_first = %d, ssid_last = %d",
			ssid_first, ssid_last);
		diag_update_msg_mask(ssid_first, ssid_last , buf + 8);
		diag_update_userspace_clients(MSG_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			for (i = 0; i < 8 + ssid_range; i++)
				*(driver->apps_rsp_buf + i) = *(buf+i);
			*(driver->apps_rsp_buf + 6) = 0x1;
			if (driver->ch_cntl)
				diag_send_msg_mask_update(driver->ch_cntl,
				ssid_first, ssid_last, MODEM_PROC);
			if (driver->chlpass_cntl)
				diag_send_msg_mask_update(driver->chlpass_cntl,
				ssid_first, ssid_last, LPASS_PROC);
			if (driver->ch_wcnss_cntl)
				diag_send_msg_mask_update(driver->ch_wcnss_cntl,
				ssid_first, ssid_last, WCNSS_PROC);
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
			if (driver->ch_cntl)
				diag_send_msg_mask_update(driver->ch_cntl,
				ALL_SSID, ALL_SSID, MODEM_PROC);
			if (driver->chlpass_cntl)
				diag_send_msg_mask_update(driver->chlpass_cntl,
				ALL_SSID, ALL_SSID, LPASS_PROC);
			if (driver->ch_wcnss_cntl)
				diag_send_msg_mask_update(driver->ch_wcnss_cntl,
				ALL_SSID, ALL_SSID, WCNSS_PROC);
			encode_rsp_and_send(7);
			return 0;
		}
#endif
	} else if (*buf == 0x82) {	/* event mask change */
		buf += 4;
		diag_event_num_bytes =  (*(uint16_t *)buf)/8+1;
		diag_update_event_mask(buf, 1, (*(uint16_t *)buf)/8+1);
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
			if (driver->ch_cntl)
				diag_send_event_mask_update(driver->ch_cntl,
				diag_event_num_bytes);
			if (driver->chlpass_cntl)
				diag_send_event_mask_update(
				driver->chlpass_cntl,
				diag_event_num_bytes);
			if (driver->ch_wcnss_cntl)
				diag_send_event_mask_update(
				driver->ch_wcnss_cntl, diag_event_num_bytes);
			encode_rsp_and_send(6 + EVENT_LAST_ID/8);
			return 0;
		}
#endif
	} else if (*buf == 0x60) {
		diag_event_config = *(buf+1);
		diag_toggle_event_mask(*(buf+1));
		diag_update_userspace_clients(EVENT_MASKS_TYPE);
#if defined(CONFIG_DIAG_OVER_USB)
		if (chk_apps_only()) {
			driver->apps_rsp_buf[0] = 0x60;
			driver->apps_rsp_buf[1] = 0x0;
			driver->apps_rsp_buf[2] = 0x0;
			if (driver->ch_cntl)
				diag_send_event_mask_update(driver->ch_cntl,
				diag_event_num_bytes);
			if (driver->chlpass_cntl)
				diag_send_event_mask_update(
				driver->chlpass_cntl,
				diag_event_num_bytes);
			if (driver->ch_wcnss_cntl)
				diag_send_event_mask_update(
				driver->ch_wcnss_cntl, diag_event_num_bytes);
			encode_rsp_and_send(2);
			return 0;
		}
#endif
	}

	return  packet_type;
}

void diag_masks_init(void)
{
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
	diag_create_msg_mask_table();
	diag_event_num_bytes = 0;
	if (driver->log_masks == NULL) {
		driver->log_masks = kzalloc(LOG_MASK_SIZE, GFP_KERNEL);
		if (driver->log_masks == NULL)
			goto err;
		kmemleak_not_leak(driver->log_masks);
	}
	driver->log_masks_length = (sizeof(struct mask_info))*MAX_EQUIP_ID;
	if (driver->event_masks == NULL) {
		driver->event_masks = kzalloc(EVENT_MASK_SIZE, GFP_KERNEL);
		if (driver->event_masks == NULL)
			goto err;
		kmemleak_not_leak(driver->event_masks);
	}
#ifdef CONFIG_DIAG_OVER_USB
	INIT_WORK(&(driver->diag_modem_mask_update_work),
						 diag_modem_mask_update_fn);
	INIT_WORK(&(driver->diag_lpass_mask_update_work),
						 diag_lpass_mask_update_fn);
	INIT_WORK(&(driver->diag_wcnss_mask_update_work),
						 diag_wcnss_mask_update_fn);
#endif
	return;
err:
	pr_err("diag: Could not initialize diag mask buffers");
	kfree(driver->event_mask);
	kfree(driver->log_mask);
	kfree(driver->msg_mask);
	kfree(driver->msg_masks);
	kfree(driver->log_masks);
	kfree(driver->event_masks);
}

void diag_masks_exit(void)
{
	kfree(driver->event_mask);
	kfree(driver->log_mask);
	kfree(driver->msg_mask);
	kfree(driver->msg_masks);
	kfree(driver->log_masks);
	kfree(driver->event_masks);
}
