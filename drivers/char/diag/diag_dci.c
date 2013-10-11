/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/diagchar.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/spinlock.h>
#include <linux/ratelimit.h>
#include <asm/current.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
#include "diag_dci.h"

static unsigned int dci_apps_tbl_size = 11;

unsigned int dci_max_reg = 100;
unsigned int dci_max_clients = 10;
unsigned char dci_cumulative_log_mask[DCI_LOG_MASK_SIZE];
unsigned char dci_cumulative_event_mask[DCI_EVENT_MASK_SIZE];
struct mutex dci_log_mask_mutex;
struct mutex dci_event_mask_mutex;
struct mutex dci_health_mutex;

spinlock_t ws_lock;
unsigned long ws_lock_flags;

/* Number of milliseconds anticipated to process the DCI data */
#define DCI_WAKEUP_TIMEOUT 1

#define DCI_CHK_CAPACITY(entry, new_data_len)				\
((entry->data_len + new_data_len > entry->total_capacity) ? 1 : 0)	\

#ifdef CONFIG_DEBUG_FS
struct diag_dci_data_info *dci_data_smd;
struct mutex dci_stat_mutex;

void diag_dci_smd_record_info(int read_bytes, uint8_t ch_type)
{
	static int curr_dci_data_smd;
	static unsigned long iteration;
	struct diag_dci_data_info *temp_data = dci_data_smd;
	if (!temp_data)
		return;
	mutex_lock(&dci_stat_mutex);
	if (curr_dci_data_smd == DIAG_DCI_DEBUG_CNT)
		curr_dci_data_smd = 0;
	temp_data += curr_dci_data_smd;
	temp_data->iteration = iteration + 1;
	temp_data->data_size = read_bytes;
	temp_data->ch_type = ch_type;
	diag_get_timestamp(temp_data->time_stamp);
	curr_dci_data_smd++;
	iteration++;
	mutex_unlock(&dci_stat_mutex);
}
#else
void diag_dci_smd_record_info(int read_bytes, uint8_t ch_type) { }
#endif

/* Process the data read from apps userspace client */
void diag_process_apps_dci_read_data(int data_type, void *buf, int recd_bytes)
{
	uint8_t cmd_code;

	if (!buf) {
		pr_err_ratelimited("diag: In %s, Null buf pointer\n", __func__);
		return;
	}

	if (data_type != DATA_TYPE_DCI_LOG && data_type != DATA_TYPE_DCI_EVENT)
		pr_err("diag: In %s, unsupported data_type: 0x%x\n",
				__func__, (unsigned int)data_type);

	cmd_code = *(uint8_t *)buf;

	if (cmd_code == LOG_CMD_CODE) {
		extract_dci_log(buf, recd_bytes, APPS_DATA);
	} else if (cmd_code == EVENT_CMD_CODE) {
		extract_dci_events(buf, recd_bytes, APPS_DATA);
	} else {
		pr_err("diag: In %s, unsupported command code: 0x%x, not log or event\n",
			__func__, cmd_code);
	}
}

/* Process the data read from the smd dci channel */
int diag_process_smd_dci_read_data(struct diag_smd_info *smd_info, void *buf,
								int recd_bytes)
{
	int read_bytes, dci_pkt_len;
	uint8_t recv_pkt_cmd_code;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	diag_dci_smd_record_info(recd_bytes, (uint8_t)smd_info->type);
	/* Each SMD read can have multiple DCI packets */
	read_bytes = 0;
	while (read_bytes < recd_bytes) {
		/* read actual length of dci pkt */
		dci_pkt_len = *(uint16_t *)(buf+2);

		/* Check if the length of the current packet is lesser than the
		 * remaining bytes in the received buffer. This includes space
		 * for the Start byte (1), Version byte (1), length bytes (2)
		 * and End byte (1)
		 */
		if ((dci_pkt_len+5) > (recd_bytes-read_bytes)) {
			pr_err("diag: Invalid length in %s, len: %d, dci_pkt_len: %d",
					__func__, recd_bytes, dci_pkt_len);
			diag_dci_try_deactivate_wakeup_source(smd_info->ch);
			return 0;
		}
		/* process one dci packet */
		pr_debug("diag: bytes read = %d, single dci pkt len = %d\n",
			read_bytes, dci_pkt_len);
		/* print_hex_dump(KERN_DEBUG, "Single DCI packet :",
		 DUMP_PREFIX_ADDRESS, 16, 1, buf, 5 + dci_pkt_len, 1); */
		recv_pkt_cmd_code = *(uint8_t *)(buf+4);
		if (recv_pkt_cmd_code == LOG_CMD_CODE) {
			/* Don't include the 4 bytes for command code */
			extract_dci_log(buf + 4, recd_bytes - 4,
					smd_info->peripheral);
		} else if (recv_pkt_cmd_code == EVENT_CMD_CODE) {
			/* Don't include the 4 bytes for command code */
			extract_dci_events(buf + 4, recd_bytes - 4,
					   smd_info->peripheral);
		} else
			extract_dci_pkt_rsp(smd_info, buf, recd_bytes);
		read_bytes += 5 + dci_pkt_len;
		buf += 5 + dci_pkt_len; /* advance to next DCI pkt */
	}
	/* Release wakeup source when there are no more clients to
	   process DCI data */
	if (driver->num_dci_client == 0)
		diag_dci_try_deactivate_wakeup_source(smd_info->ch);

	/* wake up all sleeping DCI clients which have some data */
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->data_len) {
			smd_info->in_busy_1 = 1;
			diag_update_sleeping_process(entry->client->tgid,
								DCI_DATA_TYPE);
		}
	}

	return 0;
}

static int process_dci_apps_buffer(struct diag_dci_client_tbl *entry,
				   uint16_t data_len)
{
	int ret = 0;
	int err = 0;

	if (!entry) {
		err = -EINVAL;
		return err;
	}

	if (!entry->dci_apps_data) {
		if (entry->apps_in_busy_1 == 0) {
			entry->dci_apps_data = entry->dci_apps_buffer;
			entry->apps_in_busy_1 = 1;
		} else {
			entry->dci_apps_data = diagmem_alloc(driver,
				driver->itemsize_dci,
				POOL_TYPE_DCI);
		}
		entry->apps_data_len = 0;
		if (!entry->dci_apps_data) {
			ret = -ENOMEM;
			return ret;
		}
	}

	/* If the data will not fit into the buffer */
	if ((int)driver->itemsize_dci - entry->apps_data_len <= data_len) {
		err = dci_apps_write(entry);
		if (err) {
			ret = -EIO;
			return ret;
		}
		entry->dci_apps_data = NULL;
		entry->apps_data_len = 0;
		if (entry->apps_in_busy_1 == 0) {
			entry->dci_apps_data = entry->dci_apps_buffer;
			entry->apps_in_busy_1 = 1;
		} else {
			entry->dci_apps_data = diagmem_alloc(driver,
				driver->itemsize_dci,
				POOL_TYPE_DCI);
		}

		if (!entry->dci_apps_data) {
			ret = -ENOMEM;
			return ret;
		}
	}

	return ret;
}

static inline struct diag_dci_client_tbl *__diag_dci_get_client_entry(
								int client_id)
{
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client->tgid == client_id)
			return entry;
	}
	return NULL;
}

static inline int __diag_dci_query_log_mask(struct diag_dci_client_tbl *entry,
							uint16_t log_code)
{
	uint16_t item_num;
	uint8_t equip_id, *log_mask_ptr, byte_mask;
	int byte_index, offset;

	if (!entry) {
		pr_err("diag: In %s, invalid client entry\n", __func__);
		return 0;
	}

	equip_id = LOG_GET_EQUIP_ID(log_code);
	item_num = LOG_GET_ITEM_NUM(log_code);
	byte_index = item_num/8 + 2;
	byte_mask = 0x01 << (item_num % 8);
	offset = equip_id * 514;

	if (offset + byte_index > DCI_LOG_MASK_SIZE) {
		pr_err("diag: In %s, invalid offset: %d, log_code: %d, byte_index: %d\n",
				__func__, offset, log_code, byte_index);
		return 0;
	}

	log_mask_ptr = entry->dci_log_mask;
	log_mask_ptr = log_mask_ptr + offset + byte_index;
	return ((*log_mask_ptr & byte_mask) == byte_mask) ? 1 : 0;

}

static inline int __diag_dci_query_event_mask(struct diag_dci_client_tbl *entry,
							uint16_t event_id)
{
	uint8_t *event_mask_ptr, byte_mask;
	int byte_index, bit_index;

	if (!entry) {
		pr_err("diag: In %s, invalid client entry\n", __func__);
		return 0;
	}

	byte_index = event_id/8;
	bit_index = event_id % 8;
	byte_mask = 0x1 << bit_index;

	if (byte_index > DCI_EVENT_MASK_SIZE) {
		pr_err("diag: In %s, invalid, event_id: %d, byte_index: %d\n",
				__func__, event_id, byte_index);
		return 0;
	}

	event_mask_ptr = entry->dci_event_mask;
	event_mask_ptr = event_mask_ptr + byte_index;
	return ((*event_mask_ptr & byte_mask) == byte_mask) ? 1 : 0;
}

void extract_dci_pkt_rsp(struct diag_smd_info *smd_info, unsigned char *buf,
									int len)
{
	int i = 0, index = -1, cmd_code_len = 1;
	int curr_client_pid = 0, write_len;
	struct diag_dci_client_tbl *entry = NULL;
	void *temp_buf = NULL;
	uint8_t recv_pkt_cmd_code;

	recv_pkt_cmd_code = *(uint8_t *)(buf+4);
	if (recv_pkt_cmd_code != DCI_PKT_RSP_CODE)
		cmd_code_len = 4; /* delayed response */

	/* Skip the Start(1) and the version(1) bytes */
	write_len = (int)(*(uint16_t *)(buf+2));
	/* Check if the length embedded in the packet is correct.
	 * Include the start (1), version (1), length (2) and the end
	 * (1) bytes while checking. Total = 5 bytes
	 */
	if ((write_len <= 0) && ((write_len+5) > len)) {
		pr_err("diag: Invalid length in %s, len: %d, write_len: %d",
						__func__, len, write_len);
		return;
	}
	write_len -= cmd_code_len;
	pr_debug("diag: len = %d\n", write_len);
	/* look up DCI client with tag */
	for (i = 0; i < dci_max_reg; i++) {
		if (driver->req_tracking_tbl[i].tag ==
					 *(int *)(buf+(4+cmd_code_len))) {
			*(int *)(buf+4+cmd_code_len) =
					driver->req_tracking_tbl[i].uid;
			curr_client_pid =
					 driver->req_tracking_tbl[i].pid;
			index = i;
			break;
		}
	}
	if (index == -1) {
		pr_err("diag: No matching PID for DCI data\n");
		return;
	}

	entry = __diag_dci_get_client_entry(curr_client_pid);
	if (!entry) {
		pr_err("diag: In %s, couldn't find entry\n", __func__);
		return;
	}

	mutex_lock(&entry->data_mutex);
	if (DCI_CHK_CAPACITY(entry, 8 + write_len)) {
		pr_alert("diag: create capacity for pkt rsp\n");
		entry->total_capacity += 8 + write_len;
		temp_buf = krealloc(entry->dci_data, entry->total_capacity,
				    GFP_KERNEL);
		if (!temp_buf) {
			pr_err("diag: DCI realloc failed\n");
			mutex_unlock(&entry->data_mutex);
			return;
		} else {
			entry->dci_data = temp_buf;
		}
	}
	*(int *)(entry->dci_data+entry->data_len) = DCI_PKT_RSP_TYPE;
	entry->data_len += sizeof(int);
	*(int *)(entry->dci_data+entry->data_len) = write_len;
	entry->data_len += sizeof(int);
	memcpy(entry->dci_data+entry->data_len, buf+4+cmd_code_len, write_len);
	entry->data_len += write_len;
	mutex_unlock(&entry->data_mutex);
	/* delete immediate response entry */
	if (smd_info->buf_in_1[8+cmd_code_len] != 0x80)
		driver->req_tracking_tbl[index].pid = 0;
}

static void copy_dci_event_from_apps(uint8_t *event_data,
					unsigned int total_event_len,
					struct diag_dci_client_tbl *entry)
{
	int ret = 0;
	unsigned int total_length = 4 + total_event_len;

	if (!event_data) {
		pr_err_ratelimited("diag: In %s, event_data null pointer\n",
			__func__);
		return;
	}

	if (!entry) {
		pr_err_ratelimited("diag: In %s, entry null pointer\n",
			__func__);
		return;
	}

	mutex_lock(&dci_health_mutex);
	mutex_lock(&entry->data_mutex);

	ret = process_dci_apps_buffer(entry, total_length);

	if (ret != 0) {
		if (ret == -ENOMEM)
			pr_err_ratelimited("diag: In %s, DCI event drop, ret: %d. Reduce data rate.\n",
				__func__, ret);
		else
			pr_err_ratelimited("diag: In %s, DCI event drop, ret: %d\n",
				__func__, ret);
		entry->dropped_events++;
		mutex_unlock(&entry->data_mutex);
		mutex_unlock(&dci_health_mutex);
		return;
	}

	entry->received_events++;
	*(int *)(entry->dci_apps_data+entry->apps_data_len) = DCI_EVENT_TYPE;
	memcpy(entry->dci_apps_data + entry->apps_data_len + 4, event_data,
							total_event_len);
	entry->apps_data_len += total_length;

	mutex_unlock(&entry->data_mutex);
	mutex_unlock(&dci_health_mutex);

	check_drain_timer();

	return;
}

static void copy_dci_event_from_smd(uint8_t *event_data, int data_source,
					unsigned int total_event_len,
					struct diag_dci_client_tbl *entry)
{
	(void) data_source;

	if (!event_data) {
		pr_err_ratelimited("diag: In %s, event_data null pointer\n",
			__func__);
		return;
	}

	if (!entry) {
		pr_err_ratelimited("diag: In %s, entry null pointer\n",
			__func__);
		return;
	}

	mutex_lock(&dci_health_mutex);
	mutex_lock(&entry->data_mutex);

	if (DCI_CHK_CAPACITY(entry, 4 + total_event_len)) {
		pr_err("diag: In %s, DCI event drop\n", __func__);
		entry->dropped_events++;
		mutex_unlock(&entry->data_mutex);
		mutex_unlock(&dci_health_mutex);
		return;
	}
	entry->received_events++;
	*(int *)(entry->dci_data+entry->data_len) = DCI_EVENT_TYPE;
	/* 4 bytes for DCI_EVENT_TYPE */
	memcpy(entry->dci_data + entry->data_len + 4, event_data,
						total_event_len);
	entry->data_len += 4 + total_event_len;

	mutex_unlock(&entry->data_mutex);
	mutex_unlock(&dci_health_mutex);
}

void extract_dci_events(unsigned char *buf, int len, int data_source)
{
	uint16_t event_id, event_id_packet, length, temp_len;
	uint8_t payload_len, payload_len_field;
	uint8_t timestamp[8], timestamp_len;
	uint8_t event_data[MAX_EVENT_SIZE];
	unsigned int total_event_len;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	length =  *(uint16_t *)(buf + 1); /* total length of event series */
	if (length == 0) {
		pr_err("diag: Incoming dci event length is invalid\n");
		return;
	}
	/* Move directly to the start of the event series. 1 byte for
	 * event code and 2 bytes for the length field.
	 */
	temp_len = 3;
	while (temp_len < (length - 1)) {
		event_id_packet = *(uint16_t *)(buf + temp_len);
		event_id = event_id_packet & 0x0FFF; /* extract 12 bits */
		if (event_id_packet & 0x8000) {
			/* The packet has the two smallest byte of the
			 * timestamp
			 */
			timestamp_len = 2;
		} else {
			/* The packet has the full timestamp. The first event
			 * will always have full timestamp. Save it in the
			 * timestamp buffer and use it for subsequent events if
			 * necessary.
			 */
			timestamp_len = 8;
			memcpy(timestamp, buf + temp_len + 2, timestamp_len);
		}
		/* 13th and 14th bit represent the payload length */
		if (((event_id_packet & 0x6000) >> 13) == 3) {
			payload_len_field = 1;
			payload_len = *(uint8_t *)
					(buf + temp_len + 2 + timestamp_len);
			if (payload_len < (MAX_EVENT_SIZE - 13)) {
				/* copy the payload length and the payload */
				memcpy(event_data + 12, buf + temp_len + 2 +
							timestamp_len, 1);
				memcpy(event_data + 13, buf + temp_len + 2 +
					timestamp_len + 1, payload_len);
			} else {
				pr_err("diag: event > %d, payload_len = %d\n",
					(MAX_EVENT_SIZE - 13), payload_len);
				return;
			}
		} else {
			payload_len_field = 0;
			payload_len = (event_id_packet & 0x6000) >> 13;
			/* copy the payload */
			memcpy(event_data + 12, buf + temp_len + 2 +
						timestamp_len, payload_len);
		}

		/* Before copying the data to userspace, check if we are still
		 * within the buffer limit. This is an error case, don't count
		 * it towards the health statistics.
		 *
		 * Here, the offset of 2 bytes(uint16_t) is for the
		 * event_id_packet length
		 */
		temp_len += sizeof(uint16_t) + timestamp_len +
						payload_len_field + payload_len;
		if (temp_len > len) {
			pr_err("diag: Invalid length in %s, len: %d, read: %d",
						__func__, len, temp_len);
			return;
		}

		/* 2 bytes for the event id & timestamp len is hard coded to 8,
		   as individual events have full timestamp */
		*(uint16_t *)(event_data) = 10 +
					payload_len_field + payload_len;
		*(uint16_t *)(event_data + 2) = event_id_packet & 0x7FFF;
		memcpy(event_data + 4, timestamp, 8);
		/* 2 bytes for the event length field which is added to
		   the event data */
		total_event_len = 2 + 10 + payload_len_field + payload_len;
		/* parse through event mask tbl of each client and check mask */
		list_for_each_safe(start, temp, &driver->dci_client_list) {
			entry = list_entry(start, struct diag_dci_client_tbl,
									track);
			if (__diag_dci_query_event_mask(entry, event_id)) {
				/* copy to client buffer */
				if (data_source == APPS_DATA) {
					copy_dci_event_from_apps(event_data,
							total_event_len,
							entry);
				} else {
					copy_dci_event_from_smd(event_data,
							data_source,
							total_event_len,
							entry);
				}
			}
		}
	}
}

int dci_apps_write(struct diag_dci_client_tbl *entry)
{
	int i, j;
	int err = -ENOMEM;
	int found_it = 0;

	if (!entry) {
		pr_err("diag: In %s, null dci client entry pointer\n",
			__func__);
		return -EINVAL;
	}

	/* Make sure we have a buffer and there is data in it */
	if (!entry->dci_apps_data || entry->apps_data_len <= 0) {
		pr_err("diag: In %s, Invalid dci apps data info, dci_apps_data: 0x%x, apps_data_len: %d\n",
			__func__, (unsigned int)entry->dci_apps_data,
			entry->apps_data_len);
		return -EINVAL;
	}

	for (i = 0; i < entry->dci_apps_tbl_size; i++) {
		if (entry->dci_apps_tbl[i].buf == NULL) {
			entry->dci_apps_tbl[i].buf = entry->dci_apps_data;
			entry->dci_apps_tbl[i].length = entry->apps_data_len;
			err = 0;
			for (j = 0; j < driver->num_clients; j++) {
				if (driver->client_map[j].pid ==
							entry->client->tgid) {
					driver->data_ready[j] |= DCI_DATA_TYPE;
					break;
				}
			}
			wake_up_interruptible(&driver->wait_q);
			found_it = 1;
			break;
		}
	}

	if (!found_it)
		pr_err_ratelimited("diag: In %s, Apps DCI data table full. Reduce data rate.\n",
					__func__);

	return err;
}

static void copy_dci_log_from_apps(unsigned char *buf, int len,
					struct diag_dci_client_tbl *entry)
{
	int ret = 0;
	uint16_t log_length, total_length = 0;

	if (!buf || !entry)
		return;

	log_length = *(uint16_t *)(buf + 2);
	total_length = 4 + log_length;

	/* Check if we are within the len. The check should include the
	 * first 4 bytes for the Cmd Code(2) and the length bytes (2)
	 */
	if (total_length > len) {
		pr_err("diag: Invalid length in %s, log_len: %d, len: %d",
				__func__, log_length, len);
		return;
	}

	mutex_lock(&dci_health_mutex);
	mutex_lock(&entry->data_mutex);

	ret = process_dci_apps_buffer(entry, total_length);

	if (ret != 0) {
		if (ret == -ENOMEM)
			pr_err_ratelimited("diag: In %s, DCI log drop, ret: %d. Reduce data rate.\n",
				__func__, ret);
		else
			pr_err_ratelimited("diag: In %s, DCI log drop, ret: %d\n",
				__func__, ret);
		entry->dropped_logs++;
		mutex_unlock(&entry->data_mutex);
		mutex_unlock(&dci_health_mutex);
		return;
	}

	entry->received_logs++;
	*(int *)(entry->dci_apps_data+entry->apps_data_len) = DCI_LOG_TYPE;
	memcpy(entry->dci_apps_data + entry->apps_data_len + 4, buf + 4,
								log_length);
	entry->apps_data_len += total_length;

	mutex_unlock(&entry->data_mutex);
	mutex_unlock(&dci_health_mutex);

	check_drain_timer();

	return;
}

static void copy_dci_log_from_smd(unsigned char *buf, int len, int data_source,
					struct diag_dci_client_tbl *entry)
{
	uint16_t log_length = *(uint16_t *)(buf + 2);
	(void)data_source;

	/* Check if we are within the len. The check should include the
	 * first 4 bytes for the Log code(2) and the length bytes (2)
	 */
	if ((log_length + sizeof(uint16_t) + 2) > len) {
		pr_err("diag: Invalid length in %s, log_len: %d, len: %d",
				__func__, log_length, len);
		return;
	}

	mutex_lock(&dci_health_mutex);
	mutex_lock(&entry->data_mutex);

	if (DCI_CHK_CAPACITY(entry, 4 + log_length)) {
		pr_err_ratelimited("diag: In %s, DCI log drop\n", __func__);
		entry->dropped_logs++;
		mutex_unlock(&entry->data_mutex);
		mutex_unlock(&dci_health_mutex);
		return;
	}

	entry->received_logs++;
	*(int *)(entry->dci_data+entry->data_len) = DCI_LOG_TYPE;
	memcpy(entry->dci_data + entry->data_len + 4, buf + 4, log_length);
	entry->data_len += 4 + log_length;

	mutex_unlock(&entry->data_mutex);
	mutex_unlock(&dci_health_mutex);
}

void extract_dci_log(unsigned char *buf, int len, int data_source)
{
	uint16_t log_code, read_bytes = 0;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	/* The first six bytes for the incoming log packet contains
	 * Command code (2), the length of the packet (2) and the length
	 * of the log (2)
	 */
	log_code = *(uint16_t *)(buf + 6);
	read_bytes += sizeof(uint16_t) + 6;
	if (read_bytes > len) {
		pr_err("diag: Invalid length in %s, len: %d, read: %d",
						__func__, len, read_bytes);
		return;
	}

	/* parse through log mask table of each client and check mask */
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (__diag_dci_query_log_mask(entry, log_code)) {
			pr_debug("\t log code %x needed by client %d",
				 log_code, entry->client->tgid);
			/* copy to client buffer */
			if (data_source == APPS_DATA) {
				copy_dci_log_from_apps(buf, len, entry);
			} else {
				copy_dci_log_from_smd(buf, len, data_source,
								entry);
			}
		}
	}
}

void diag_update_smd_dci_work_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						struct diag_smd_info,
						diag_notify_update_smd_work);
	int i, j;
	char dirty_bits[16];
	uint8_t *client_log_mask_ptr;
	uint8_t *log_mask_ptr;
	int ret;
	int index = smd_info->peripheral;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	/* Update apps and peripheral(s) with the dci log and event masks */
	memset(dirty_bits, 0, 16 * sizeof(uint8_t));

	/*
	 * From each log entry used by each client, determine
	 * which log entries in the cumulative logs that need
	 * to be updated on the peripheral.
	 */
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		client_log_mask_ptr = entry->dci_log_mask;
		for (j = 0; j < 16; j++) {
			if (*(client_log_mask_ptr+1))
				dirty_bits[j] = 1;
			client_log_mask_ptr += 514;
		}
	}

	mutex_lock(&dci_log_mask_mutex);
	/* Update the appropriate dirty bits in the cumulative mask */
	log_mask_ptr = dci_cumulative_log_mask;
	for (i = 0; i < 16; i++) {
		if (dirty_bits[i])
			*(log_mask_ptr+1) = dirty_bits[i];

		log_mask_ptr += 514;
	}
	mutex_unlock(&dci_log_mask_mutex);

	/* Send updated mask to userspace clients */
	diag_update_userspace_clients(DCI_LOG_MASKS_TYPE);
	/* Send updated log mask to peripherals */
	ret = diag_send_dci_log_mask(driver->smd_cntl[index].ch);

	/* Send updated event mask to userspace clients */
	diag_update_userspace_clients(DCI_EVENT_MASKS_TYPE);
	/* Send updated event mask to peripheral */
	ret = diag_send_dci_event_mask(driver->smd_cntl[index].ch);

	smd_info->notify_context = 0;
}

void diag_dci_notify_client(int peripheral_mask, int data)
{
	int stat;
	struct siginfo info;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	memset(&info, 0, sizeof(struct siginfo));
	info.si_code = SI_QUEUE;
	info.si_int = (peripheral_mask | data);

	/* Notify the DCI process that the peripheral DCI Channel is up */
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->list & peripheral_mask) {
			info.si_signo = entry->signal_type;
			stat = send_sig_info(entry->signal_type, &info,
								entry->client);
			if (stat)
				pr_err("diag: Err sending dci signal to client, signal data: 0x%x, stat: %d\n",
							info.si_int, stat);
		}
	}
}

int diag_send_dci_pkt(struct diag_master_table entry, unsigned char *buf,
					 int len, int index)
{
	int i, status = 0;
	unsigned int read_len = 0;

	/* The first 4 bytes is the uid tag and the next four bytes is
	   the minmum packet length of a request packet */
	if (len < DCI_PKT_REQ_MIN_LEN) {
		pr_err("diag: dci: Invalid pkt len %d in %s\n", len, __func__);
		return -EIO;
	}
	if (len > APPS_BUF_SIZE - 10) {
		pr_err("diag: dci: Invalid payload length in %s\n", __func__);
		return -EIO;
	}
	/* remove UID from user space pkt before sending to peripheral*/
	buf = buf + sizeof(int);
	read_len += sizeof(int);
	len = len - sizeof(int);
	mutex_lock(&driver->dci_mutex);
	/* prepare DCI packet */
	driver->apps_dci_buf[0] = CONTROL_CHAR; /* start */
	driver->apps_dci_buf[1] = 1; /* version */
	*(uint16_t *)(driver->apps_dci_buf + 2) = len + 4 + 1; /* length */
	driver->apps_dci_buf[4] = DCI_PKT_RSP_CODE;
	*(int *)(driver->apps_dci_buf + 5) =
		driver->req_tracking_tbl[index].tag;
	for (i = 0; i < len; i++)
		driver->apps_dci_buf[i+9] = *(buf+i);
	read_len += len;
	driver->apps_dci_buf[9+len] = CONTROL_CHAR; /* end */
	if ((read_len + 9) >= USER_SPACE_DATA) {
		pr_err("diag: dci: Invalid length while forming dci pkt in %s",
								__func__);
		mutex_unlock(&driver->dci_mutex);
		return -EIO;
	}

	for (i = 0; i < NUM_SMD_DCI_CHANNELS; i++) {
		struct diag_smd_info *smd_info = driver->separate_cmdrsp[i] ?
					&driver->smd_dci_cmd[i] :
					&driver->smd_dci[i];
		if (entry.client_id == smd_info->peripheral) {
			if (smd_info->ch) {
				smd_write(smd_info->ch,
					driver->apps_dci_buf, len + 10);
				status = DIAG_DCI_NO_ERROR;
			}
			break;
		}
	}

	if (status != DIAG_DCI_NO_ERROR) {
		pr_alert("diag: check DCI channel\n");
		status = DIAG_DCI_SEND_DATA_FAIL;
	}
	mutex_unlock(&driver->dci_mutex);
	return status;
}

int diag_register_dci_transaction(int uid)
{
	int i, new_dci_client = 1, ret = -1;

	for (i = 0; i < dci_max_reg; i++) {
		if (driver->req_tracking_tbl[i].pid == current->tgid) {
			new_dci_client = 0;
			break;
		}
	}
	mutex_lock(&driver->dci_mutex);
	/* Make an entry in kernel DCI table */
	driver->dci_tag++;
	for (i = 0; i < dci_max_reg; i++) {
		if (driver->req_tracking_tbl[i].pid == 0) {
			driver->req_tracking_tbl[i].pid = current->tgid;
			driver->req_tracking_tbl[i].uid = uid;
			driver->req_tracking_tbl[i].tag = driver->dci_tag;
			ret = i;
			break;
		}
	}
	mutex_unlock(&driver->dci_mutex);
	return ret;
}

int diag_process_dci_transaction(unsigned char *buf, int len)
{
	unsigned char *temp = buf;
	uint16_t subsys_cmd_code, log_code, item_num;
	int subsys_id, cmd_code, ret = -1, index = -1, found = 0;
	struct diag_master_table entry;
	int count, set_mask, num_codes, bit_index, event_id, offset = 0, i;
	unsigned int byte_index, read_len = 0;
	uint8_t equip_id, *log_mask_ptr, *head_log_mask_ptr, byte_mask;
	uint8_t *event_mask_ptr;
	struct diag_dci_client_tbl *dci_entry = NULL;

	if (!driver->smd_dci[MODEM_DATA].ch) {
		pr_err("diag: DCI smd channel for peripheral %d not valid for dci updates\n",
			driver->smd_dci[MODEM_DATA].peripheral);
		return DIAG_DCI_SEND_DATA_FAIL;
	}

	if (!temp) {
		pr_err("diag: Invalid buffer in %s\n", __func__);
		return -ENOMEM;
	}

	/* This is Pkt request/response transaction */
	if (*(int *)temp > 0) {
		if (len < DCI_PKT_REQ_MIN_LEN || len > USER_SPACE_DATA) {
			pr_err("diag: dci: Invalid length %d len in %s", len,
								__func__);
			return -EIO;
		}
		/* enter this UID into kernel table and return index */
		index = diag_register_dci_transaction(*(int *)temp);
		if (index < 0) {
			pr_alert("diag: registering new DCI transaction failed\n");
			return DIAG_DCI_NO_REG;
		}
		temp += sizeof(int);
		/*
		 * Check for registered peripheral and fwd pkt to
		 * appropriate proc
		 */
		cmd_code = (int)(*(char *)temp);
		temp++;
		subsys_id = (int)(*(char *)temp);
		temp++;
		subsys_cmd_code = *(uint16_t *)temp;
		temp += sizeof(uint16_t);
		read_len += sizeof(int) + 2 + sizeof(uint16_t);
		if (read_len >= USER_SPACE_DATA) {
			pr_err("diag: dci: Invalid length in %s\n", __func__);
			return -EIO;
		}
		pr_debug("diag: %d %d %d", cmd_code, subsys_id,
			subsys_cmd_code);
		for (i = 0; i < diag_max_reg; i++) {
			entry = driver->table[i];
			if (entry.process_id != NO_PROCESS) {
				if (entry.cmd_code == cmd_code &&
					entry.subsys_id == subsys_id &&
					entry.cmd_code_lo <= subsys_cmd_code &&
					entry.cmd_code_hi >= subsys_cmd_code) {
					ret = diag_send_dci_pkt(entry, buf,
								len, index);
				} else if (entry.cmd_code == 255
					  && cmd_code == 75) {
					if (entry.subsys_id == subsys_id &&
						entry.cmd_code_lo <=
						subsys_cmd_code &&
						entry.cmd_code_hi >=
						subsys_cmd_code) {
						ret = diag_send_dci_pkt(entry,
							buf, len, index);
					}
				} else if (entry.cmd_code == 255 &&
					entry.subsys_id == 255) {
					if (entry.cmd_code_lo <= cmd_code &&
						entry.cmd_code_hi >=
							cmd_code) {
						ret = diag_send_dci_pkt(entry,
							buf, len, index);
					}
				}
			}
		}
	} else if (*(int *)temp == DCI_LOG_TYPE) {
		/* Minimum length of a log mask config is 12 + 2 bytes for
		   atleast one log code to be set or reset */
		if (len < DCI_LOG_CON_MIN_LEN || len > USER_SPACE_DATA) {
			pr_err("diag: dci: Invalid length in %s\n", __func__);
			return -EIO;
		}
		/* find client table entry */
		dci_entry = diag_dci_get_client_entry();
		if (!dci_entry) {
			pr_err("diag: In %s, invalid client\n", __func__);
			return ret;
		}

		/* Extract each log code and put in client table */
		temp += sizeof(int);
		read_len += sizeof(int);
		set_mask = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);
		num_codes = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);

		if (num_codes == 0 || (num_codes >= (USER_SPACE_DATA - 8)/2)) {
			pr_err("diag: dci: Invalid number of log codes %d\n",
								num_codes);
			return -EIO;
		}

		head_log_mask_ptr = dci_entry->dci_log_mask;
		if (!head_log_mask_ptr) {
			pr_err("diag: dci: Invalid Log mask pointer in %s\n",
								__func__);
			return -ENOMEM;
		}
		pr_debug("diag: head of dci log mask %p\n", head_log_mask_ptr);
		count = 0; /* iterator for extracting log codes */
		while (count < num_codes) {
			if (read_len >= USER_SPACE_DATA) {
				pr_err("diag: dci: Invalid length for log type in %s",
								__func__);
				return -EIO;
			}
			log_code = *(uint16_t *)temp;
			equip_id = LOG_GET_EQUIP_ID(log_code);
			item_num = LOG_GET_ITEM_NUM(log_code);
			byte_index = item_num/8 + 2;
			if (byte_index >= (DCI_MAX_ITEMS_PER_LOG_CODE+2)) {
				pr_err("diag: dci: Log type, invalid byte index\n");
				return ret;
			}
			byte_mask = 0x01 << (item_num % 8);
			/*
			 * Parse through log mask table and find
			 * relevant range
			 */
			log_mask_ptr = head_log_mask_ptr;
			found = 0;
			offset = 0;
			while (log_mask_ptr && (offset < DCI_LOG_MASK_SIZE)) {
				if (*log_mask_ptr == equip_id) {
					found = 1;
					pr_debug("diag: find equip id = %x at %p\n",
						 equip_id, log_mask_ptr);
					break;
				} else {
					pr_debug("diag: did not find equip id = %x at %d\n",
						 equip_id, *log_mask_ptr);
					log_mask_ptr += 514;
					offset += 514;
				}
			}
			if (!found) {
				pr_err("diag: dci equip id not found\n");
				return ret;
			}
			*(log_mask_ptr+1) = 1; /* set the dirty byte */
			log_mask_ptr = log_mask_ptr + byte_index;
			if (set_mask)
				*log_mask_ptr |= byte_mask;
			else
				*log_mask_ptr &= ~byte_mask;
			/* add to cumulative mask */
			update_dci_cumulative_log_mask(
				offset, byte_index,
				byte_mask);
			temp += 2;
			read_len += 2;
			count++;
			ret = DIAG_DCI_NO_ERROR;
		}
		/* send updated mask to userspace clients */
		diag_update_userspace_clients(DCI_LOG_MASKS_TYPE);
		/* send updated mask to peripherals */
		ret = diag_send_dci_log_mask(driver->smd_cntl[MODEM_DATA].ch);
	} else if (*(int *)temp == DCI_EVENT_TYPE) {
		/* Minimum length of a event mask config is 12 + 4 bytes for
		  atleast one event id to be set or reset. */
		if (len < DCI_EVENT_CON_MIN_LEN || len > USER_SPACE_DATA) {
			pr_err("diag: dci: Invalid length in %s\n", __func__);
			return -EIO;
		}
		/* find client table entry */
		dci_entry = diag_dci_get_client_entry();
		if (!dci_entry) {
			pr_err("diag: In %s, invalid client\n", __func__);
			return ret;
		}
		/* Extract each log code and put in client table */
		temp += sizeof(int);
		read_len += sizeof(int);
		set_mask = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);
		num_codes = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);

		/* Check for positive number of event ids. Also, the number of
		   event ids should fit in the buffer along with set_mask and
		   num_codes which are 4 bytes each */
		if (num_codes == 0 || (num_codes >= (USER_SPACE_DATA - 8)/2)) {
			pr_err("diag: dci: Invalid number of event ids %d\n",
								num_codes);
			return -EIO;
		}

		event_mask_ptr = dci_entry->dci_event_mask;
		if (!event_mask_ptr) {
			pr_err("diag: dci: Invalid event mask pointer in %s\n",
								__func__);
			return -ENOMEM;
		}
		pr_debug("diag: head of dci event mask %p\n", event_mask_ptr);
		count = 0; /* iterator for extracting log codes */
		while (count < num_codes) {
			if (read_len >= USER_SPACE_DATA) {
				pr_err("diag: dci: Invalid length for event type in %s",
								__func__);
				return -EIO;
			}
			event_id = *(int *)temp;
			byte_index = event_id/8;
			if (byte_index >= DCI_EVENT_MASK_SIZE) {
				pr_err("diag: dci: Event type, invalid byte index\n");
				return ret;
			}
			bit_index = event_id % 8;
			byte_mask = 0x1 << bit_index;
			/*
			 * Parse through event mask table and set
			 * relevant byte & bit combination
			 */
			if (set_mask)
				*(event_mask_ptr + byte_index) |= byte_mask;
			else
				*(event_mask_ptr + byte_index) &= ~byte_mask;
			/* add to cumulative mask */
			update_dci_cumulative_event_mask(byte_index, byte_mask);
			temp += sizeof(int);
			read_len += sizeof(int);
			count++;
			ret = DIAG_DCI_NO_ERROR;
		}
		/* send updated mask to userspace clients */
		diag_update_userspace_clients(DCI_EVENT_MASKS_TYPE);
		/* send updated mask to peripherals */
		ret = diag_send_dci_event_mask(driver->smd_cntl[MODEM_DATA].ch);
	} else {
		pr_alert("diag: Incorrect DCI transaction\n");
	}
	return ret;
}


struct diag_dci_client_tbl *diag_dci_get_client_entry()
{
	return __diag_dci_get_client_entry(current->tgid);
}

void update_dci_cumulative_event_mask(int offset, uint8_t byte_mask)
{
	uint8_t *event_mask_ptr;
	uint8_t *update_ptr = dci_cumulative_event_mask;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	bool is_set = false;

	mutex_lock(&dci_event_mask_mutex);
	update_ptr += offset;
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		event_mask_ptr = entry->dci_event_mask;
		event_mask_ptr += offset;
		if ((*event_mask_ptr & byte_mask) == byte_mask) {
			is_set = true;
			/* break even if one client has the event mask set */
			break;
		}
	}
	if (is_set == false)
		*update_ptr &= ~byte_mask;
	else
		*update_ptr |= byte_mask;
	mutex_unlock(&dci_event_mask_mutex);
}

void diag_dci_invalidate_cumulative_event_mask()
{
	int i = 0;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	uint8_t *update_ptr, *event_mask_ptr;
	update_ptr = dci_cumulative_event_mask;

	mutex_lock(&dci_event_mask_mutex);
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		event_mask_ptr = entry->dci_event_mask;
		for (i = 0; i < DCI_EVENT_MASK_SIZE; i++)
			*(update_ptr+i) |= *(event_mask_ptr+i);
	}
	mutex_unlock(&dci_event_mask_mutex);
}

int diag_send_dci_event_mask(smd_channel_t *ch)
{
	void *buf = driver->buf_event_mask_update;
	int header_size = sizeof(struct diag_ctrl_event_mask);
	int wr_size = -ENOMEM, retry_count = 0, timer;
	int ret = DIAG_DCI_NO_ERROR;

	mutex_lock(&driver->diag_cntl_mutex);
	/* send event mask update */
	driver->event_mask->cmd_type = DIAG_CTRL_MSG_EVENT_MASK;
	driver->event_mask->data_len = 7 + DCI_EVENT_MASK_SIZE;
	driver->event_mask->stream_id = DCI_MASK_STREAM;
	driver->event_mask->status = 3; /* status for valid mask */
	driver->event_mask->event_config = 1; /* event config */
	driver->event_mask->event_mask_size = DCI_EVENT_MASK_SIZE;
	memcpy(buf, driver->event_mask, header_size);
	memcpy(buf+header_size, dci_cumulative_event_mask, DCI_EVENT_MASK_SIZE);
	if (ch) {
		while (retry_count < 3) {
			wr_size = smd_write(ch, buf,
					 header_size + DCI_EVENT_MASK_SIZE);
			if (wr_size == -ENOMEM) {
				retry_count++;
				for (timer = 0; timer < 5; timer++)
					udelay(2000);
			} else {
				break;
			}
		}
		if (wr_size != header_size + DCI_EVENT_MASK_SIZE) {
			pr_err("diag: error writing dci event mask %d, tried %d\n",
				 wr_size, header_size + DCI_EVENT_MASK_SIZE);
			ret = DIAG_DCI_SEND_DATA_FAIL;
		}
	} else {
		pr_err("diag: ch not valid for dci event mask update\n");
		ret = DIAG_DCI_SEND_DATA_FAIL;
	}
	mutex_unlock(&driver->diag_cntl_mutex);

	return ret;
}

void update_dci_cumulative_log_mask(int offset, unsigned int byte_index,
						uint8_t byte_mask)
{
	int i;
	uint8_t *update_ptr = dci_cumulative_log_mask;
	uint8_t *log_mask_ptr;
	bool is_set = false;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	mutex_lock(&dci_log_mask_mutex);
	*update_ptr = 0;
	/* set the equipment IDs */
	for (i = 0; i < 16; i++)
		*(update_ptr + (i*514)) = i;

	update_ptr += offset;
	/* update the dirty bit */
	*(update_ptr+1) = 1;
	update_ptr = update_ptr + byte_index;
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		log_mask_ptr = entry->dci_log_mask;
		log_mask_ptr = log_mask_ptr + offset + byte_index;
		if ((*log_mask_ptr & byte_mask) == byte_mask) {
			is_set = true;
			/* break even if one client has the log mask set */
			break;
		}
	}

	if (is_set == false)
		*update_ptr &= ~byte_mask;
	else
		*update_ptr |= byte_mask;
	mutex_unlock(&dci_log_mask_mutex);
}

void diag_dci_invalidate_cumulative_log_mask()
{
	int i = 0;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	uint8_t *update_ptr, *log_mask_ptr;
	update_ptr = dci_cumulative_log_mask;

	mutex_lock(&dci_log_mask_mutex);
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		log_mask_ptr = entry->dci_log_mask;
		for (i = 0; i < DCI_LOG_MASK_SIZE; i++)
			*(update_ptr+i) |= *(log_mask_ptr+i);
	}
	mutex_unlock(&dci_log_mask_mutex);
}

int diag_send_dci_log_mask(smd_channel_t *ch)
{
	void *buf = driver->buf_log_mask_update;
	int header_size = sizeof(struct diag_ctrl_log_mask);
	uint8_t *log_mask_ptr = dci_cumulative_log_mask;
	int i, wr_size = -ENOMEM, retry_count = 0, timer;
	int ret = DIAG_DCI_NO_ERROR;

	if (!ch) {
		pr_err("diag: ch not valid for dci log mask update\n");
		return DIAG_DCI_SEND_DATA_FAIL;
	}

	mutex_lock(&driver->diag_cntl_mutex);
	for (i = 0; i < 16; i++) {
		retry_count = 0;
		driver->log_mask->cmd_type = DIAG_CTRL_MSG_LOG_MASK;
		driver->log_mask->num_items = 512;
		driver->log_mask->data_len  = 11 + 512;
		driver->log_mask->stream_id = DCI_MASK_STREAM;
		driver->log_mask->status = 3; /* status for valid mask */
		driver->log_mask->equip_id = *log_mask_ptr;
		driver->log_mask->log_mask_size = 512;
		memcpy(buf, driver->log_mask, header_size);
		memcpy(buf+header_size, log_mask_ptr+2, 512);
		/* if dirty byte is set and channel is valid */
		if (ch && *(log_mask_ptr+1)) {
			while (retry_count < 3) {
				wr_size = smd_write(ch, buf, header_size + 512);
				if (wr_size == -ENOMEM) {
					retry_count++;
					for (timer = 0; timer < 5; timer++)
						udelay(2000);
				} else
					break;
			}
			if (wr_size != header_size + 512) {
				pr_err("diag: dci log mask update failed %d, tried %d for equip_id %d\n",
					wr_size, header_size + 512,
					driver->log_mask->equip_id);
				ret = DIAG_DCI_SEND_DATA_FAIL;

			} else {
				*(log_mask_ptr+1) = 0; /* clear dirty byte */
				pr_debug("diag: updated dci log equip ID %d\n",
						 *log_mask_ptr);
			}
		}
		log_mask_ptr += 514;
	}
	mutex_unlock(&driver->diag_cntl_mutex);

	return ret;
}

void create_dci_log_mask_tbl(unsigned char *tbl_buf)
{
	uint8_t i; int count = 0;

	if (!tbl_buf)
		return;

	/* create hard coded table for log mask with 16 categories */
	for (i = 0; i < 16; i++) {
		*(uint8_t *)tbl_buf = i;
		pr_debug("diag: put value %x at %p\n", i, tbl_buf);
		memset(tbl_buf+1, 0, 513); /* set dirty bit as 0 */
		tbl_buf += 514;
		count += 514;
	}
}

void create_dci_event_mask_tbl(unsigned char *tbl_buf)
{
	memset(tbl_buf, 0, 512);
}

static int diag_dci_probe(struct platform_device *pdev)
{
	int err = 0;
	int index;

	if (pdev->id == SMD_APPS_MODEM) {
		index = MODEM_DATA;
		err = smd_open("DIAG_2",
			&driver->smd_dci[index].ch,
			&driver->smd_dci[index],
			diag_smd_notify);
		driver->smd_dci[index].ch_save =
			driver->smd_dci[index].ch;
		driver->dci_device = &pdev->dev;
		driver->dci_device->power.wakeup = wakeup_source_register
							("DIAG_DCI_WS");
		if (err)
			pr_err("diag: In %s, cannot open DCI port, Id = %d, err: %d\n",
				__func__, pdev->id, err);
	}

	return err;
}

static int diag_dci_cmd_probe(struct platform_device *pdev)
{
	int err = 0;
	int index;

	if (pdev->id == SMD_APPS_MODEM) {
		index = MODEM_DATA;
		err = smd_named_open_on_edge("DIAG_2_CMD",
			pdev->id,
			&driver->smd_dci_cmd[index].ch,
			&driver->smd_dci_cmd[index],
			diag_smd_notify);
		driver->smd_dci_cmd[index].ch_save =
			driver->smd_dci_cmd[index].ch;
		driver->dci_cmd_device = &pdev->dev;
		driver->dci_cmd_device->power.wakeup = wakeup_source_register
							("DIAG_DCI_CMD_WS");
		if (err)
			pr_err("diag: In %s, cannot open DCI port, Id = %d, err: %d\n",
				__func__, pdev->id, err);
	}

	return err;
}

static int diag_dci_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diag_dci_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diag_dci_dev_pm_ops = {
	.runtime_suspend = diag_dci_runtime_suspend,
	.runtime_resume = diag_dci_runtime_resume,
};

struct platform_driver msm_diag_dci_driver = {
	.probe = diag_dci_probe,
	.driver = {
		.name = "DIAG_2",
		.owner = THIS_MODULE,
		.pm   = &diag_dci_dev_pm_ops,
	},
};

struct platform_driver msm_diag_dci_cmd_driver = {
	.probe = diag_dci_cmd_probe,
	.driver = {
		.name = "DIAG_2_CMD",
		.owner = THIS_MODULE,
		.pm   = &diag_dci_dev_pm_ops,
	},
};

int diag_dci_init(void)
{
	int success = 0;
	int i;

	driver->dci_tag = 0;
	driver->dci_client_id = 0;
	driver->num_dci_client = 0;
	driver->dci_device = NULL;
	driver->dci_cmd_device = NULL;
	mutex_init(&driver->dci_mutex);
	mutex_init(&dci_log_mask_mutex);
	mutex_init(&dci_event_mask_mutex);
	mutex_init(&dci_health_mutex);
	spin_lock_init(&ws_lock);

	for (i = 0; i < NUM_SMD_DCI_CHANNELS; i++) {
		success = diag_smd_constructor(&driver->smd_dci[i], i,
							SMD_DCI_TYPE);
		if (!success)
			goto err;
	}

	if (driver->supports_separate_cmdrsp) {
		for (i = 0; i < NUM_SMD_DCI_CMD_CHANNELS; i++) {
			success = diag_smd_constructor(&driver->smd_dci_cmd[i],
							i, SMD_DCI_CMD_TYPE);
			if (!success)
				goto err;
		}
	}

	if (driver->req_tracking_tbl == NULL) {
		driver->req_tracking_tbl = kzalloc(dci_max_reg *
			sizeof(struct dci_pkt_req_tracking_tbl), GFP_KERNEL);
		if (driver->req_tracking_tbl == NULL)
			goto err;
	}
	if (driver->apps_dci_buf == NULL) {
		driver->apps_dci_buf = kzalloc(APPS_BUF_SIZE, GFP_KERNEL);
		if (driver->apps_dci_buf == NULL)
			goto err;
	}
	INIT_LIST_HEAD(&driver->dci_client_list);

	driver->diag_dci_wq = create_singlethread_workqueue("diag_dci_wq");
	success = platform_driver_register(&msm_diag_dci_driver);
	if (success) {
		pr_err("diag: Could not register DCI driver\n");
		goto err;
	}
	if (driver->supports_separate_cmdrsp) {
		success = platform_driver_register(&msm_diag_dci_cmd_driver);
		if (success) {
			pr_err("diag: Could not register DCI cmd driver\n");
			goto err;
		}
	}
	return DIAG_DCI_NO_ERROR;
err:
	pr_err("diag: Could not initialize diag DCI buffers");
	kfree(driver->req_tracking_tbl);
	kfree(driver->apps_dci_buf);
	for (i = 0; i < NUM_SMD_DCI_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_dci[i]);

	if (driver->supports_separate_cmdrsp)
		for (i = 0; i < NUM_SMD_DCI_CMD_CHANNELS; i++)
			diag_smd_destructor(&driver->smd_dci_cmd[i]);

	if (driver->diag_dci_wq)
		destroy_workqueue(driver->diag_dci_wq);
	mutex_destroy(&driver->dci_mutex);
	mutex_destroy(&dci_log_mask_mutex);
	mutex_destroy(&dci_event_mask_mutex);
	mutex_destroy(&dci_health_mutex);
	return DIAG_DCI_NO_REG;
}

void diag_dci_exit(void)
{
	int i;

	for (i = 0; i < NUM_SMD_DCI_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_dci[i]);

	platform_driver_unregister(&msm_diag_dci_driver);

	if (driver->supports_separate_cmdrsp) {
		for (i = 0; i < NUM_SMD_DCI_CMD_CHANNELS; i++)
			diag_smd_destructor(&driver->smd_dci_cmd[i]);

		platform_driver_unregister(&msm_diag_dci_cmd_driver);
	}
	kfree(driver->req_tracking_tbl);
	kfree(driver->apps_dci_buf);
	mutex_destroy(&driver->dci_mutex);
	mutex_destroy(&dci_log_mask_mutex);
	mutex_destroy(&dci_event_mask_mutex);
	mutex_destroy(&dci_health_mutex);
	destroy_workqueue(driver->diag_dci_wq);
}

int diag_dci_clear_log_mask()
{
	int j, k, err = DIAG_DCI_NO_ERROR;
	uint8_t *log_mask_ptr, *update_ptr;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	entry = diag_dci_get_client_entry();
	if (!entry) {
		pr_err("diag: In %s, invalid client entry\n", __func__);
		return DIAG_DCI_TABLE_ERR;
	}

	mutex_lock(&dci_log_mask_mutex);
	create_dci_log_mask_tbl(entry->dci_log_mask);
	memset(dci_cumulative_log_mask, 0x0, DCI_LOG_MASK_SIZE);
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		update_ptr = dci_cumulative_log_mask;
		log_mask_ptr = entry->dci_log_mask;
		for (j = 0; j < 16; j++) {
			*update_ptr = j;
			*(update_ptr + 1) = 1;
			update_ptr += 2;
			log_mask_ptr += 2;
			for (k = 0; k < 513; k++) {
				*update_ptr |= *log_mask_ptr;
				update_ptr++;
				log_mask_ptr++;
			}
		}
	}
	mutex_unlock(&dci_log_mask_mutex);
	/* send updated mask to userspace clients */
	diag_update_userspace_clients(DCI_LOG_MASKS_TYPE);
	/* Send updated mask to peripherals */
	err = diag_send_dci_log_mask(driver->smd_cntl[MODEM_DATA].ch);
	return err;
}

int diag_dci_clear_event_mask()
{
	int j, err = DIAG_DCI_NO_ERROR;
	uint8_t *event_mask_ptr, *update_ptr;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	entry = diag_dci_get_client_entry();
	if (!entry) {
		pr_err("diag: In %s, invalid client entry\n", __func__);
		return DIAG_DCI_TABLE_ERR;
	}

	mutex_lock(&dci_event_mask_mutex);
	memset(entry->dci_event_mask, 0x0, DCI_EVENT_MASK_SIZE);
	memset(dci_cumulative_event_mask, 0x0, DCI_EVENT_MASK_SIZE);
	update_ptr = dci_cumulative_event_mask;

	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		event_mask_ptr = entry->dci_event_mask;
		for (j = 0; j < DCI_EVENT_MASK_SIZE; j++)
			*(update_ptr + j) |= *(event_mask_ptr + j);
	}
	mutex_unlock(&dci_event_mask_mutex);
	/* send updated mask to userspace clients */
	diag_update_userspace_clients(DCI_EVENT_MASKS_TYPE);
	/* Send updated mask to peripherals */
	err = diag_send_dci_event_mask(driver->smd_cntl[MODEM_DATA].ch);
	return err;
}

int diag_dci_query_log_mask(uint16_t log_code)
{
	return __diag_dci_query_log_mask(diag_dci_get_client_entry(),
					 log_code);
}

int diag_dci_query_event_mask(uint16_t event_id)
{
	return __diag_dci_query_event_mask(diag_dci_get_client_entry(),
					   event_id);
}

uint8_t diag_dci_get_cumulative_real_time()
{
	uint8_t real_time = MODE_NONREALTIME;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->real_time == MODE_REALTIME) {
			real_time = 1;
			break;
		}
	}
	return real_time;
}

int diag_dci_set_real_time(uint8_t real_time)
{
	struct diag_dci_client_tbl *entry = NULL;
	entry = diag_dci_get_client_entry();
	if (!entry) {
		pr_err("diag: In %s, invalid client entry\n", __func__);
		return 0;
	}
	entry->real_time = real_time;
	return 1;
}

void diag_dci_try_activate_wakeup_source(smd_channel_t *channel)
{
	spin_lock_irqsave(&ws_lock, ws_lock_flags);
	if (channel == driver->smd_dci[MODEM_DATA].ch) {
		pm_wakeup_event(driver->dci_device, DCI_WAKEUP_TIMEOUT);
		pm_stay_awake(driver->dci_device);
	} else if (channel == driver->smd_dci_cmd[MODEM_DATA].ch) {
		pm_wakeup_event(driver->dci_cmd_device, DCI_WAKEUP_TIMEOUT);
		pm_stay_awake(driver->dci_cmd_device);
	}
	spin_unlock_irqrestore(&ws_lock, ws_lock_flags);
}

void diag_dci_try_deactivate_wakeup_source(smd_channel_t *channel)
{
	spin_lock_irqsave(&ws_lock, ws_lock_flags);
	if (channel == driver->smd_dci[MODEM_DATA].ch)
		pm_relax(driver->dci_device);
	else if (channel == driver->smd_dci_cmd[MODEM_DATA].ch)
		pm_relax(driver->dci_cmd_device);
	spin_unlock_irqrestore(&ws_lock, ws_lock_flags);
}

int diag_dci_register_client(uint16_t peripheral_list, int signal)
{
	int i;
	struct diag_dci_client_tbl *new_entry = NULL;

	if (driver->dci_state == DIAG_DCI_NO_REG)
		return DIAG_DCI_NO_REG;

	if (driver->num_dci_client >= MAX_DCI_CLIENTS)
		return DIAG_DCI_NO_REG;

	new_entry = kzalloc(sizeof(struct diag_dci_client_tbl), GFP_KERNEL);
	if (new_entry == NULL) {
		pr_err("diag: unable to alloc memory\n");
		return -ENOMEM;
	}

	mutex_lock(&driver->dci_mutex);
	if (!(driver->num_dci_client)) {
		for (i = 0; i < NUM_SMD_DCI_CHANNELS; i++)
			driver->smd_dci[i].in_busy_1 = 0;
		if (driver->supports_separate_cmdrsp)
			for (i = 0; i < NUM_SMD_DCI_CMD_CHANNELS; i++)
				driver->smd_dci_cmd[i].in_busy_1 = 0;
	}

	new_entry->client = current;
	new_entry->list = peripheral_list;
	new_entry->signal_type = signal;
	new_entry->dci_log_mask =  kzalloc(DCI_LOG_MASK_SIZE, GFP_KERNEL);
	if (!new_entry->dci_log_mask) {
		pr_err("diag: Unable to create log mask for client, %d",
							driver->dci_client_id);
		goto fail_alloc;
	}
	create_dci_log_mask_tbl(new_entry->dci_log_mask);
	new_entry->dci_event_mask =  kzalloc(DCI_EVENT_MASK_SIZE, GFP_KERNEL);
	if (!new_entry->dci_event_mask) {
		pr_err("diag: Unable to create event mask for client, %d",
							driver->dci_client_id);
		goto fail_alloc;
	}
	create_dci_event_mask_tbl(new_entry->dci_event_mask);
	new_entry->data_len = 0;
	new_entry->dci_data = kzalloc(IN_BUF_SIZE, GFP_KERNEL);
	if (!new_entry->dci_data) {
		pr_err("diag: Unable to allocate dci data memory for client, %d",
							driver->dci_client_id);
		goto fail_alloc;
	}
	new_entry->total_capacity = IN_BUF_SIZE;
	new_entry->dci_apps_buffer = kzalloc(driver->itemsize_dci, GFP_KERNEL);
	if (!new_entry->dci_apps_buffer) {
		pr_err("diag: Unable to allocate dci apps data memory for client, %d",
							driver->dci_client_id);
		goto fail_alloc;
	}
	new_entry->dci_apps_data = NULL;
	new_entry->apps_data_len = 0;
	new_entry->apps_in_busy_1 = 0;
	new_entry->dci_apps_tbl_size =
			(dci_apps_tbl_size < driver->poolsize_dci + 1) ?
			(driver->poolsize_dci + 1) : dci_apps_tbl_size;
	new_entry->dci_apps_tbl = kzalloc(dci_apps_tbl_size *
					  sizeof(struct diag_write_device),
					  GFP_KERNEL);
	if (!new_entry->dci_apps_tbl) {
		pr_err("diag: Unable to allocate dci apps table for client, %d",
							driver->dci_client_id);
		goto fail_alloc;
	}
	new_entry->dropped_logs = 0;
	new_entry->dropped_events = 0;
	new_entry->received_logs = 0;
	new_entry->received_events = 0;
	new_entry->real_time = 1;
	mutex_init(&new_entry->data_mutex);
	list_add(&new_entry->track, &driver->dci_client_list);
	driver->num_dci_client++;
	driver->dci_client_id++;
	if (driver->num_dci_client == 1)
		diag_update_proc_vote(DIAG_PROC_DCI, VOTE_UP);
	queue_work(driver->diag_real_time_wq, &driver->diag_real_time_work);
	mutex_unlock(&driver->dci_mutex);

	return driver->dci_client_id;

fail_alloc:
	kfree(new_entry->dci_log_mask);
	kfree(new_entry->dci_event_mask);
	kfree(new_entry->dci_apps_tbl);
	kfree(new_entry->dci_apps_buffer);
	kfree(new_entry->dci_data);
	kfree(new_entry);

	return -ENOMEM;
}

int diag_dci_deinit_client()
{
	int ret = DIAG_DCI_NO_ERROR, real_time = MODE_REALTIME, i;
	struct diag_dci_client_tbl *entry = diag_dci_get_client_entry();

	if (!entry)
		return DIAG_DCI_NOT_SUPPORTED;

	mutex_lock(&driver->dci_mutex);
	/*
	 * Remove the entry from the list before freeing the buffers
	 * to ensure that we don't have any invalid access.
	 */
	list_del(&entry->track);
	driver->num_dci_client--;
	/*
	 * Clear the client's log and event masks, update the cumulative
	 * masks and send the masks to peripherals
	 */
	kfree(entry->dci_log_mask);
	diag_update_userspace_clients(DCI_LOG_MASKS_TYPE);
	diag_dci_invalidate_cumulative_log_mask();
	ret = diag_send_dci_event_mask(driver->smd_cntl[MODEM_DATA].ch);
	if (ret != DIAG_DCI_NO_ERROR) {
		mutex_unlock(&driver->dci_mutex);
		return ret;
	}
	kfree(entry->dci_event_mask);
	diag_update_userspace_clients(DCI_EVENT_MASKS_TYPE);
	diag_dci_invalidate_cumulative_event_mask();
	ret = diag_send_dci_log_mask(driver->smd_cntl[MODEM_DATA].ch);
	if (ret != DIAG_DCI_NO_ERROR) {
		mutex_unlock(&driver->dci_mutex);
		return ret;
	}

	/* Clean up the client's apps buffer */
	mutex_lock(&entry->data_mutex);
	for (i = 0; i < entry->dci_apps_tbl_size; i++) {
		if (entry->dci_apps_tbl[i].buf != NULL &&
		    (entry->dci_apps_tbl[i].buf != entry->dci_apps_buffer)) {
			diagmem_free(driver, entry->dci_apps_tbl[i].buf,
				     POOL_TYPE_DCI);
		}
		entry->dci_apps_tbl[i].buf = NULL;
		entry->dci_apps_tbl[i].length = 0;
	}

	kfree(entry->dci_data);
	kfree(entry->dci_apps_buffer);
	kfree(entry->dci_apps_tbl);
	mutex_unlock(&entry->data_mutex);
	kfree(entry);


	if (driver->num_dci_client == 0) {
		diag_update_proc_vote(DIAG_PROC_DCI, VOTE_DOWN);
	} else {
		real_time = diag_dci_get_cumulative_real_time();
		diag_update_real_time_vote(DIAG_PROC_DCI, real_time);
	}
	queue_work(driver->diag_real_time_wq, &driver->diag_real_time_work);

	mutex_unlock(&driver->dci_mutex);

	return DIAG_DCI_NO_ERROR;
}
