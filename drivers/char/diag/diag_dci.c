/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/reboot.h>
#include <asm/current.h>
#include <soc/qcom/restart.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
#include "diag_dci.h"
#include "diag_masks.h"
#include "diagfwd_bridge.h"
#include "diagfwd_peripheral.h"
#include "diag_ipc_logging.h"

static struct timer_list dci_drain_timer;
static int dci_timer_in_progress;
static struct work_struct dci_data_drain_work;

struct diag_dci_partial_pkt_t partial_pkt;

unsigned int dci_max_reg = 100;
unsigned int dci_max_clients = 10;
struct mutex dci_log_mask_mutex;
struct mutex dci_event_mask_mutex;

/*
 * DCI_HANDSHAKE_RETRY_TIME: Time to wait (in microseconds) before checking the
 * connection status again.
 *
 * DCI_HANDSHAKE_WAIT_TIME: Timeout (in milliseconds) to check for dci
 * connection status
 */
#define DCI_HANDSHAKE_RETRY_TIME	500000
#define DCI_HANDSHAKE_WAIT_TIME		200

spinlock_t ws_lock;
unsigned long ws_lock_flags;

struct dci_ops_tbl_t dci_ops_tbl[NUM_DCI_PROC] = {
	{
		.ctx = 0,
		.send_log_mask = diag_send_dci_log_mask,
		.send_event_mask = diag_send_dci_event_mask,
		.peripheral_status = 0,
		.mempool = 0,
	},
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	{
		.ctx = DIAGFWD_MDM_DCI,
		.send_log_mask = diag_send_dci_log_mask_remote,
		.send_event_mask = diag_send_dci_event_mask_remote,
		.peripheral_status = 0,
		.mempool = POOL_TYPE_MDM_DCI_WRITE,
	}
#endif
};

struct dci_channel_status_t dci_channel_status[NUM_DCI_PROC] = {
	{
		.id = 0,
		.open = 0,
		.retry_count = 0
	},
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	{
		.id = DIAGFWD_MDM_DCI,
		.open = 0,
		.retry_count = 0
	}
#endif
};

/* Number of milliseconds anticipated to process the DCI data */
#define DCI_WAKEUP_TIMEOUT 1

#define DCI_CAN_ADD_BUF_TO_LIST(buf)					\
	(buf && buf->data && !buf->in_busy && buf->data_len > 0)	\

#ifdef CONFIG_DEBUG_FS
struct diag_dci_data_info *dci_traffic;
struct mutex dci_stat_mutex;
void diag_dci_record_traffic(int read_bytes, uint8_t ch_type,
			     uint8_t peripheral, uint8_t proc)
{
	static int curr_dci_data;
	static unsigned long iteration;
	struct diag_dci_data_info *temp_data = dci_traffic;
	if (!temp_data)
		return;
	mutex_lock(&dci_stat_mutex);
	if (curr_dci_data == DIAG_DCI_DEBUG_CNT)
		curr_dci_data = 0;
	temp_data += curr_dci_data;
	temp_data->iteration = iteration + 1;
	temp_data->data_size = read_bytes;
	temp_data->peripheral = peripheral;
	temp_data->ch_type = ch_type;
	temp_data->proc = proc;
	diag_get_timestamp(temp_data->time_stamp);
	curr_dci_data++;
	iteration++;
	mutex_unlock(&dci_stat_mutex);
}
#else
void diag_dci_record_traffic(int read_bytes, uint8_t ch_type,
			     uint8_t peripheral, uint8_t proc) { }
#endif
static void create_dci_log_mask_tbl(unsigned char *mask, uint8_t dirty)
{
	unsigned char *temp = mask;
	uint8_t i;

	if (!mask)
		return;

	/* create hard coded table for log mask with 16 categories */
	for (i = 0; i < DCI_MAX_LOG_CODES; i++) {
		*temp = i;
		temp++;
		*temp = dirty ? 1 : 0;
		temp++;
		memset(temp, 0, DCI_MAX_ITEMS_PER_LOG_CODE);
		temp += DCI_MAX_ITEMS_PER_LOG_CODE;
	}
}

static void create_dci_event_mask_tbl(unsigned char *tbl_buf)
{
	if (tbl_buf)
		memset(tbl_buf, 0, DCI_EVENT_MASK_SIZE);
}

void dci_drain_data(unsigned long data)
{
	queue_work(driver->diag_dci_wq, &dci_data_drain_work);
}

static void dci_check_drain_timer(void)
{
	if (!dci_timer_in_progress) {
		dci_timer_in_progress = 1;
		mod_timer(&dci_drain_timer, jiffies + msecs_to_jiffies(200));
	}
}

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
static void dci_handshake_work_fn(struct work_struct *work)
{
	int err = 0;
	int max_retries = 5;

	struct dci_channel_status_t *status = container_of(work,
						struct dci_channel_status_t,
						handshake_work);

	if (status->open) {
		pr_debug("diag: In %s, remote dci channel is open, index: %d\n",
			 __func__, status->id);
		return;
	}

	if (status->retry_count == max_retries) {
		status->retry_count = 0;
		pr_info("diag: dci channel connection handshake timed out, id: %d\n",
			status->id);
		err = diagfwd_bridge_close(TOKEN_TO_BRIDGE(status->id));
		if (err) {
			pr_err("diag: In %s, unable to close dci channel id: %d, err: %d\n",
			       __func__, status->id, err);
		}
		return;
	}
	status->retry_count++;
	/*
	 * Sleep for sometime to check for the connection status again. The
	 * value should be optimum to include a roundabout time for a small
	 * packet to the remote processor.
	 */
	usleep_range(DCI_HANDSHAKE_RETRY_TIME, DCI_HANDSHAKE_RETRY_TIME + 100);
	mod_timer(&status->wait_time,
		  jiffies + msecs_to_jiffies(DCI_HANDSHAKE_WAIT_TIME));
}

static void dci_chk_handshake(unsigned long data)
{
	int index = (int)data;

	if (index < 0 || index > NUM_DCI_PROC)
		return;

	queue_work(driver->diag_dci_wq,
		   &dci_channel_status[index].handshake_work);
}
#endif

static int diag_dci_init_buffer(struct diag_dci_buffer_t *buffer, int type)
{
	if (!buffer || buffer->data)
		return -EINVAL;

	switch (type) {
	case DCI_BUF_PRIMARY:
		buffer->capacity = IN_BUF_SIZE;
		buffer->data = kzalloc(buffer->capacity, GFP_KERNEL);
		if (!buffer->data)
			return -ENOMEM;
		break;
	case DCI_BUF_SECONDARY:
		buffer->data = NULL;
		buffer->capacity = IN_BUF_SIZE;
		break;
	case DCI_BUF_CMD:
		buffer->capacity = DIAG_MAX_REQ_SIZE + DCI_BUF_SIZE;
		buffer->data = kzalloc(buffer->capacity, GFP_KERNEL);
		if (!buffer->data)
			return -ENOMEM;
		break;
	default:
		pr_err("diag: In %s, unknown type %d", __func__, type);
		return -EINVAL;
	}

	buffer->data_len = 0;
	buffer->in_busy = 0;
	buffer->buf_type = type;
	mutex_init(&buffer->data_mutex);

	return 0;
}

static inline int diag_dci_check_buffer(struct diag_dci_buffer_t *buf, int len)
{
	if (!buf)
		return -EINVAL;

	/* Return 1 if the buffer is not busy and can hold new data */
	if ((buf->data_len + len < buf->capacity) && !buf->in_busy)
		return 1;

	return 0;
}

static void dci_add_buffer_to_list(struct diag_dci_client_tbl *client,
				   struct diag_dci_buffer_t *buf)
{
	if (!buf || !client || !buf->data)
		return;

	if (buf->in_list || buf->data_len == 0)
		return;

	mutex_lock(&client->write_buf_mutex);
	list_add_tail(&buf->buf_track, &client->list_write_buf);
	/*
	 * In the case of DCI, there can be multiple packets in one read. To
	 * calculate the wakeup source reference count, we must account for each
	 * packet in a single read.
	 */
	diag_ws_on_read(DIAG_WS_DCI, buf->data_len);
	mutex_lock(&buf->data_mutex);
	buf->in_busy = 1;
	buf->in_list = 1;
	mutex_unlock(&buf->data_mutex);
	mutex_unlock(&client->write_buf_mutex);
}

static int diag_dci_get_buffer(struct diag_dci_client_tbl *client,
			       int data_source, int len)
{
	struct diag_dci_buffer_t *buf_primary = NULL;
	struct diag_dci_buffer_t *buf_temp = NULL;
	struct diag_dci_buffer_t *curr = NULL;

	if (!client)
		return -EINVAL;
	if (len < 0 || len > IN_BUF_SIZE)
		return -EINVAL;

	curr = client->buffers[data_source].buf_curr;
	buf_primary = client->buffers[data_source].buf_primary;

	if (curr && diag_dci_check_buffer(curr, len) == 1)
		return 0;

	dci_add_buffer_to_list(client, curr);
	client->buffers[data_source].buf_curr = NULL;

	if (diag_dci_check_buffer(buf_primary, len) == 1) {
		client->buffers[data_source].buf_curr = buf_primary;
		return 0;
	}

	buf_temp = kzalloc(sizeof(struct diag_dci_buffer_t), GFP_KERNEL);
	if (!buf_temp)
		return -EIO;

	if (!diag_dci_init_buffer(buf_temp, DCI_BUF_SECONDARY)) {
		buf_temp->data = diagmem_alloc(driver, IN_BUF_SIZE,
					       POOL_TYPE_DCI);
		if (!buf_temp->data) {
			kfree(buf_temp);
			buf_temp = NULL;
			return -ENOMEM;
		}
		client->buffers[data_source].buf_curr = buf_temp;
		return 0;
	}

	kfree(buf_temp);
	buf_temp = NULL;
	return -EIO;
}

void diag_dci_wakeup_clients()
{
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	mutex_lock(&driver->dci_mutex);
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);

		/*
		 * Don't wake up the client when there is no pending buffer to
		 * write or when it is writing to user space
		 */
		if (!list_empty(&entry->list_write_buf) && !entry->in_service) {
			mutex_lock(&entry->write_buf_mutex);
			entry->in_service = 1;
			mutex_unlock(&entry->write_buf_mutex);
			diag_update_sleeping_process(entry->client->tgid,
						     DCI_DATA_TYPE);
		}
	}
	mutex_unlock(&driver->dci_mutex);
}

void dci_data_drain_work_fn(struct work_struct *work)
{
	int i;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	struct diag_dci_buf_peripheral_t *proc_buf = NULL;
	struct diag_dci_buffer_t *buf_temp = NULL;

	mutex_lock(&driver->dci_mutex);
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		for (i = 0; i < entry->num_buffers; i++) {
			proc_buf = &entry->buffers[i];

			mutex_lock(&proc_buf->buf_mutex);
			buf_temp = proc_buf->buf_primary;
			if (DCI_CAN_ADD_BUF_TO_LIST(buf_temp))
				dci_add_buffer_to_list(entry, buf_temp);

			buf_temp = proc_buf->buf_cmd;
			if (DCI_CAN_ADD_BUF_TO_LIST(buf_temp))
				dci_add_buffer_to_list(entry, buf_temp);

			buf_temp = proc_buf->buf_curr;
			if (DCI_CAN_ADD_BUF_TO_LIST(buf_temp)) {
				dci_add_buffer_to_list(entry, buf_temp);
				proc_buf->buf_curr = NULL;
			}
			mutex_unlock(&proc_buf->buf_mutex);
		}
		if (!list_empty(&entry->list_write_buf) && !entry->in_service) {
			mutex_lock(&entry->write_buf_mutex);
			entry->in_service = 1;
			mutex_unlock(&entry->write_buf_mutex);
			diag_update_sleeping_process(entry->client->tgid,
						     DCI_DATA_TYPE);
		}
	}
	mutex_unlock(&driver->dci_mutex);
	dci_timer_in_progress = 0;
}

static int diag_process_single_dci_pkt(unsigned char *buf, int len,
				       int data_source, int token)
{
	uint8_t cmd_code = 0;

	if (!buf || len < 0) {
		pr_err("diag: Invalid input in %s, buf: %p, len: %d\n",
			__func__, buf, len);
		return -EIO;
	}

	cmd_code = *(uint8_t *)buf;

	switch (cmd_code) {
	case LOG_CMD_CODE:
		extract_dci_log(buf, len, data_source, token);
		break;
	case EVENT_CMD_CODE:
		extract_dci_events(buf, len, data_source, token);
		break;
	case DCI_PKT_RSP_CODE:
	case DCI_DELAYED_RSP_CODE:
		extract_dci_pkt_rsp(buf, len, data_source, token);
		break;
	case DCI_CONTROL_PKT_CODE:
		extract_dci_ctrl_pkt(buf, len, token);
		break;
	default:
		pr_err("diag: Unable to process single DCI packet, cmd_code: %d, data_source: %d",
			cmd_code, data_source);
		return -EINVAL;
	}

	return 0;
}

/* Process the data read from apps userspace client */
void diag_process_apps_dci_read_data(int data_type, void *buf, int recd_bytes)
{
	int err = 0;

	if (!buf) {
		pr_err_ratelimited("diag: In %s, Null buf pointer\n", __func__);
		return;
	}

	if (data_type != DATA_TYPE_DCI_LOG && data_type != DATA_TYPE_DCI_EVENT
						&& data_type != DCI_PKT_TYPE) {
		pr_err("diag: In %s, unsupported data_type: 0x%x\n",
				__func__, (unsigned int)data_type);
		return;
	}

	err = diag_process_single_dci_pkt(buf, recd_bytes, APPS_DATA,
					  DCI_LOCAL_PROC);
	if (err)
		return;

	/* wake up all sleeping DCI clients which have some data */
	diag_dci_wakeup_clients();
	dci_check_drain_timer();
}

void diag_process_remote_dci_read_data(int index, void *buf, int recd_bytes)
{
	int read_bytes = 0, err = 0;
	uint16_t dci_pkt_len;
	struct diag_dci_header_t *header = NULL;
	int header_len = sizeof(struct diag_dci_header_t);
	int token = BRIDGE_TO_TOKEN(index);

	if (!buf)
		return;

	diag_dci_record_traffic(recd_bytes, 0, 0, token);

	if (!partial_pkt.processing)
		goto start;

	if (partial_pkt.remaining > recd_bytes) {
		if ((partial_pkt.read_len + recd_bytes) >
							(MAX_DCI_PACKET_SZ)) {
			pr_err("diag: Invalid length %d, %d received in %s\n",
			       partial_pkt.read_len, recd_bytes, __func__);
			goto end;
		}
		memcpy(partial_pkt.data + partial_pkt.read_len, buf,
								recd_bytes);
		read_bytes += recd_bytes;
		buf += read_bytes;
		partial_pkt.read_len += recd_bytes;
		partial_pkt.remaining -= recd_bytes;
	} else {
		if ((partial_pkt.read_len + partial_pkt.remaining) >
							(MAX_DCI_PACKET_SZ)) {
			pr_err("diag: Invalid length during partial read %d, %d received in %s\n",
			       partial_pkt.read_len,
			       partial_pkt.remaining, __func__);
			goto end;
		}
		memcpy(partial_pkt.data + partial_pkt.read_len, buf,
						partial_pkt.remaining);
		read_bytes += partial_pkt.remaining;
		buf += read_bytes;
		partial_pkt.read_len += partial_pkt.remaining;
		partial_pkt.remaining = 0;
	}

	if (partial_pkt.remaining == 0) {
		/*
		 * Retrieve from the DCI control packet after the header = start
		 * (1 byte) + version (1 byte) + length (2 bytes)
		 */
		diag_process_single_dci_pkt(partial_pkt.data + 4,
				partial_pkt.read_len - header_len,
				DCI_REMOTE_DATA, token);
		partial_pkt.read_len = 0;
		partial_pkt.total_len = 0;
		partial_pkt.processing = 0;
		goto start;
	}
	goto end;

start:
	while (read_bytes < recd_bytes) {
		header = (struct diag_dci_header_t *)buf;
		dci_pkt_len = header->length;

		if (header->cmd_code != DCI_CONTROL_PKT_CODE &&
			driver->num_dci_client == 0) {
			read_bytes += header_len + dci_pkt_len;
			buf += header_len + dci_pkt_len;
			continue;
		}

		if (dci_pkt_len + header_len > MAX_DCI_PACKET_SZ) {
			pr_err("diag: Invalid length in the dci packet field %d\n",
								dci_pkt_len);
			break;
		}

		if ((dci_pkt_len + header_len) > (recd_bytes - read_bytes)) {
			partial_pkt.read_len = recd_bytes - read_bytes;
			partial_pkt.total_len = dci_pkt_len + header_len;
			partial_pkt.remaining = partial_pkt.total_len -
						partial_pkt.read_len;
			partial_pkt.processing = 1;
			memcpy(partial_pkt.data, buf, partial_pkt.read_len);
			break;
		}
		/*
		 * Retrieve from the DCI control packet after the header = start
		 * (1 byte) + version (1 byte) + length (2 bytes)
		 */
		err = diag_process_single_dci_pkt(buf + 4, dci_pkt_len,
						 DCI_REMOTE_DATA, DCI_MDM_PROC);
		if (err)
			break;
		read_bytes += header_len + dci_pkt_len;
		buf += header_len + dci_pkt_len; /* advance to next DCI pkt */
	}
end:
	if (err)
		return;
	/* wake up all sleeping DCI clients which have some data */
	diag_dci_wakeup_clients();
	dci_check_drain_timer();
	return;
}

/* Process the data read from the peripheral dci channels */
void diag_dci_process_peripheral_data(struct diagfwd_info *p_info, void *buf,
				      int recd_bytes)
{
	int read_bytes = 0, err = 0;
	uint16_t dci_pkt_len;
	struct diag_dci_pkt_header_t *header = NULL;
	uint8_t recv_pkt_cmd_code;

	if (!buf || !p_info)
		return;

	/*
	 * Release wakeup source when there are no more clients to
	 * process DCI data
	 */
	if (driver->num_dci_client == 0) {
		diag_ws_reset(DIAG_WS_DCI);
		return;
	}

	diag_dci_record_traffic(recd_bytes, p_info->type, p_info->peripheral,
				DCI_LOCAL_PROC);
	while (read_bytes < recd_bytes) {
		header = (struct diag_dci_pkt_header_t *)buf;
		recv_pkt_cmd_code = header->pkt_code;
		dci_pkt_len = header->len;

		/*
		 * Check if the length of the current packet is lesser than the
		 * remaining bytes in the received buffer. This includes space
		 * for the Start byte (1), Version byte (1), length bytes (2)
		 * and End byte (1)
		 */
		if ((dci_pkt_len + 5) > (recd_bytes - read_bytes)) {
			pr_err("diag: Invalid length in %s, len: %d, dci_pkt_len: %d",
				__func__, recd_bytes, dci_pkt_len);
			diag_ws_release();
			return;
		}
		/*
		 * Retrieve from the DCI control packet after the header = start
		 * (1 byte) + version (1 byte) + length (2 bytes)
		 */
		err = diag_process_single_dci_pkt(buf + 4, dci_pkt_len,
						  (int)p_info->peripheral,
						  DCI_LOCAL_PROC);
		if (err) {
			diag_ws_release();
			break;
		}
		read_bytes += 5 + dci_pkt_len;
		buf += 5 + dci_pkt_len; /* advance to next DCI pkt */
	}

	if (err)
		return;
	/* wake up all sleeping DCI clients which have some data */
	diag_dci_wakeup_clients();
	dci_check_drain_timer();
	return;
}

int diag_dci_query_log_mask(struct diag_dci_client_tbl *entry,
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

int diag_dci_query_event_mask(struct diag_dci_client_tbl *entry,
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

static int diag_dci_filter_commands(struct diag_pkt_header_t *header)
{
	if (!header)
		return -ENOMEM;

	switch (header->cmd_code) {
	case 0x7d: /* Msg Mask Configuration */
	case 0x73: /* Log Mask Configuration */
	case 0x81: /* Event Mask Configuration */
	case 0x82: /* Event Mask Change */
	case 0x60: /* Event Mask Toggle */
		return 1;
	}

	if (header->cmd_code == 0x4b && header->subsys_id == 0x12) {
		switch (header->subsys_cmd_code) {
		case 0x60: /* Extended Event Mask Config */
		case 0x61: /* Extended Msg Mask Config */
		case 0x62: /* Extended Log Mask Config */
		case 0x20C: /* Set current Preset ID */
		case 0x20D: /* Get current Preset ID */
		case 0x218: /* HDLC Disabled Command */
			return 1;
		}
	}

	return 0;
}

static struct dci_pkt_req_entry_t *diag_register_dci_transaction(int uid,
								 int client_id)
{
	struct dci_pkt_req_entry_t *entry = NULL;
	entry = kzalloc(sizeof(struct dci_pkt_req_entry_t), GFP_KERNEL);
	if (!entry)
		return NULL;

	driver->dci_tag++;
	entry->client_id = client_id;
	entry->uid = uid;
	entry->tag = driver->dci_tag;
	pr_debug("diag: Registering DCI cmd req, client_id: %d, uid: %d, tag:%d\n",
				entry->client_id, entry->uid, entry->tag);
	list_add_tail(&entry->track, &driver->dci_req_list);

	return entry;
}

static struct dci_pkt_req_entry_t *diag_dci_get_request_entry(int tag)
{
	struct list_head *start, *temp;
	struct dci_pkt_req_entry_t *entry = NULL;
	list_for_each_safe(start, temp, &driver->dci_req_list) {
		entry = list_entry(start, struct dci_pkt_req_entry_t, track);
		if (entry->tag == tag)
			return entry;
	}
	return NULL;
}

static int diag_dci_remove_req_entry(unsigned char *buf, int len,
				     struct dci_pkt_req_entry_t *entry)
{
	uint16_t rsp_count = 0, delayed_rsp_id = 0;
	if (!buf || len <= 0 || !entry) {
		pr_err("diag: In %s, invalid input buf: %p, len: %d, entry: %p\n",
			__func__, buf, len, entry);
		return -EIO;
	}

	/* It is an immediate response, delete it from the table */
	if (*buf != 0x80) {
		list_del(&entry->track);
		kfree(entry);
		return 1;
	}

	/* It is a delayed response. Check if the length is valid */
	if (len < MIN_DELAYED_RSP_LEN) {
		pr_err("diag: Invalid delayed rsp packet length %d\n", len);
		return -EINVAL;
	}

	/*
	 * If the delayed response id field (uint16_t at byte 8) is 0 then
	 * there is only one response and we can remove the request entry.
	 */
	delayed_rsp_id = *(uint16_t *)(buf + 8);
	if (delayed_rsp_id == 0) {
		list_del(&entry->track);
		kfree(entry);
		return 1;
	}

	/*
	 * Check the response count field (uint16 at byte 10). The request
	 * entry can be deleted it it is the last response in the sequence.
	 * It is the last response in the sequence if the response count
	 * is 1 or if the signed bit gets dropped.
	 */
	rsp_count = *(uint16_t *)(buf + 10);
	if (rsp_count > 0 && rsp_count < 0x1000) {
		list_del(&entry->track);
		kfree(entry);
		return 1;
	}

	return 0;
}

static void dci_process_ctrl_status(unsigned char *buf, int len, int token)
{
	struct diag_ctrl_dci_status *header = NULL;
	unsigned char *temp = buf;
	uint32_t read_len = 0;
	uint8_t i;
	int peripheral_mask, status;

	if (!buf || (len < sizeof(struct diag_ctrl_dci_status))) {
		pr_err("diag: In %s, invalid buf %p or length: %d\n",
		       __func__, buf, len);
		return;
	}

	if (!VALID_DCI_TOKEN(token)) {
		pr_err("diag: In %s, invalid DCI token %d\n", __func__, token);
		return;
	}

	header = (struct diag_ctrl_dci_status *)temp;
	temp += sizeof(struct diag_ctrl_dci_status);
	read_len += sizeof(struct diag_ctrl_dci_status);

	for (i = 0; i < header->count; i++) {
		if (read_len > len) {
			pr_err("diag: In %s, Invalid length len: %d\n",
			       __func__, len);
			return;
		}

		switch (*(uint8_t *)temp) {
		case PERIPHERAL_MODEM:
			peripheral_mask = DIAG_CON_MPSS;
			break;
		case PERIPHERAL_LPASS:
			peripheral_mask = DIAG_CON_LPASS;
			break;
		case PERIPHERAL_WCNSS:
			peripheral_mask = DIAG_CON_WCNSS;
			break;
		case PERIPHERAL_SENSORS:
			peripheral_mask = DIAG_CON_SENSORS;
			break;
		default:
			pr_err("diag: In %s, unknown peripheral, peripheral: %d\n",
				__func__, *(uint8_t *)temp);
			return;
		}
		temp += sizeof(uint8_t);
		read_len += sizeof(uint8_t);

		status = (*(uint8_t *)temp) ? DIAG_STATUS_OPEN :
							DIAG_STATUS_CLOSED;
		temp += sizeof(uint8_t);
		read_len += sizeof(uint8_t);
		diag_dci_notify_client(peripheral_mask, status, token);
	}
}

static void dci_process_ctrl_handshake_pkt(unsigned char *buf, int len,
					   int token)
{
	struct diag_ctrl_dci_handshake_pkt *header = NULL;
	unsigned char *temp = buf;
	int err = 0;

	if (!buf || (len < sizeof(struct diag_ctrl_dci_handshake_pkt)))
		return;

	if (!VALID_DCI_TOKEN(token))
		return;

	header = (struct diag_ctrl_dci_handshake_pkt *)temp;
	if (header->magic == DCI_MAGIC) {
		dci_channel_status[token].open = 1;
		err = dci_ops_tbl[token].send_log_mask(token);
		if (err) {
			pr_err("diag: In %s, unable to send log mask to token: %d, err: %d\n",
			       __func__, token, err);
		}
		err = dci_ops_tbl[token].send_event_mask(token);
		if (err) {
			pr_err("diag: In %s, unable to send event mask to token: %d, err: %d\n",
			       __func__, token, err);
		}
	}
}

void extract_dci_ctrl_pkt(unsigned char *buf, int len, int token)
{
	unsigned char *temp = buf;
	uint32_t ctrl_pkt_id;

	diag_ws_on_read(DIAG_WS_DCI, len);
	if (!buf) {
		pr_err("diag: Invalid buffer in %s\n", __func__);
		goto err;
	}

	if (len < (sizeof(uint8_t) + sizeof(uint32_t))) {
		pr_err("diag: In %s, invalid length %d\n", __func__, len);
		goto err;
	}

	/* Skip the Control packet command code */
	temp += sizeof(uint8_t);
	len -= sizeof(uint8_t);
	ctrl_pkt_id = *(uint32_t *)temp;
	switch (ctrl_pkt_id) {
	case DIAG_CTRL_MSG_DCI_CONNECTION_STATUS:
		dci_process_ctrl_status(temp, len, token);
		break;
	case DIAG_CTRL_MSG_DCI_HANDSHAKE_PKT:
		dci_process_ctrl_handshake_pkt(temp, len, token);
		break;
	default:
		pr_debug("diag: In %s, unknown control pkt %d\n",
			 __func__, ctrl_pkt_id);
		break;
	}

err:
	/*
	 * DCI control packets are not consumed by the clients. Mimic client
	 * consumption by setting and clearing the wakeup source copy_count
	 * explicitly.
	 */
	diag_ws_on_copy_fail(DIAG_WS_DCI);
}

void extract_dci_pkt_rsp(unsigned char *buf, int len, int data_source,
			 int token)
{
	int tag;
	struct diag_dci_client_tbl *entry = NULL;
	void *temp_buf = NULL;
	uint8_t dci_cmd_code, cmd_code_len, delete_flag = 0;
	uint32_t rsp_len = 0;
	struct diag_dci_buffer_t *rsp_buf = NULL;
	struct dci_pkt_req_entry_t *req_entry = NULL;
	unsigned char *temp = buf;
	int save_req_uid = 0;
	struct diag_dci_pkt_rsp_header_t pkt_rsp_header;

	if (!buf) {
		pr_err("diag: Invalid pointer in %s\n", __func__);
		return;
	}
	dci_cmd_code = *(uint8_t *)(temp);
	if (dci_cmd_code == DCI_PKT_RSP_CODE) {
		cmd_code_len = sizeof(uint8_t);
	} else if (dci_cmd_code == DCI_DELAYED_RSP_CODE) {
		cmd_code_len = sizeof(uint32_t);
	} else {
		pr_err("diag: In %s, invalid command code %d\n", __func__,
								dci_cmd_code);
		return;
	}
	temp += cmd_code_len;
	tag = *(int *)temp;
	temp += sizeof(int);

	/*
	 * The size of the response is (total length) - (length of the command
	 * code, the tag (int)
	 */
	rsp_len = len - (cmd_code_len + sizeof(int));
	if ((rsp_len == 0) || (rsp_len > (len - 5))) {
		pr_err("diag: Invalid length in %s, len: %d, rsp_len: %d",
						__func__, len, rsp_len);
		return;
	}

	mutex_lock(&driver->dci_mutex);
	req_entry = diag_dci_get_request_entry(tag);
	if (!req_entry) {
		pr_err_ratelimited("diag: No matching client for DCI data\n");
		mutex_unlock(&driver->dci_mutex);
		return;
	}

	entry = diag_dci_get_client_entry(req_entry->client_id);
	if (!entry) {
		pr_err("diag: In %s, couldn't find client entry, id:%d\n",
						__func__, req_entry->client_id);
		mutex_unlock(&driver->dci_mutex);
		return;
	}

	save_req_uid = req_entry->uid;
	/* Remove the headers and send only the response to this function */
	delete_flag = diag_dci_remove_req_entry(temp, rsp_len, req_entry);
	if (delete_flag < 0) {
		mutex_unlock(&driver->dci_mutex);
		return;
	}

	mutex_lock(&entry->buffers[data_source].buf_mutex);
	rsp_buf = entry->buffers[data_source].buf_cmd;

	mutex_lock(&rsp_buf->data_mutex);
	/*
	 * Check if we can fit the data in the rsp buffer. The total length of
	 * the rsp is the rsp length (write_len) + DCI_PKT_RSP_TYPE header (int)
	 * + field for length (int) + delete_flag (uint8_t)
	 */
	if ((rsp_buf->data_len + 9 + rsp_len) > rsp_buf->capacity) {
		pr_alert("diag: create capacity for pkt rsp\n");
		rsp_buf->capacity += 9 + rsp_len;
		temp_buf = krealloc(rsp_buf->data, rsp_buf->capacity,
				    GFP_KERNEL);
		if (!temp_buf) {
			pr_err("diag: DCI realloc failed\n");
			mutex_unlock(&rsp_buf->data_mutex);
			mutex_unlock(&entry->buffers[data_source].buf_mutex);
			mutex_unlock(&driver->dci_mutex);
			return;
		} else {
			rsp_buf->data = temp_buf;
		}
	}

	/* Fill in packet response header information */
	pkt_rsp_header.type = DCI_PKT_RSP_TYPE;
	/* Packet Length = Response Length + Length of uid field (int) */
	pkt_rsp_header.length = rsp_len + sizeof(int);
	pkt_rsp_header.delete_flag = delete_flag;
	pkt_rsp_header.uid = save_req_uid;
	memcpy(rsp_buf->data + rsp_buf->data_len, &pkt_rsp_header,
		sizeof(struct diag_dci_pkt_rsp_header_t));
	rsp_buf->data_len += sizeof(struct diag_dci_pkt_rsp_header_t);
	memcpy(rsp_buf->data + rsp_buf->data_len, temp, rsp_len);
	rsp_buf->data_len += rsp_len;
	rsp_buf->data_source = data_source;

	mutex_unlock(&rsp_buf->data_mutex);

	/*
	 * Add directly to the list for writing responses to the
	 * userspace as these shouldn't be buffered and shouldn't wait
	 * for log and event buffers to be full
	 */
	dci_add_buffer_to_list(entry, rsp_buf);
	mutex_unlock(&entry->buffers[data_source].buf_mutex);
	mutex_unlock(&driver->dci_mutex);
}

static void copy_dci_event(unsigned char *buf, int len,
			   struct diag_dci_client_tbl *client, int data_source)
{
	struct diag_dci_buffer_t *data_buffer = NULL;
	struct diag_dci_buf_peripheral_t *proc_buf = NULL;
	int err = 0, total_len = 0;

	if (!buf || !client) {
		pr_err("diag: Invalid pointers in %s", __func__);
		return;
	}

	total_len = sizeof(int) + len;

	proc_buf = &client->buffers[data_source];
	mutex_lock(&proc_buf->buf_mutex);
	mutex_lock(&proc_buf->health_mutex);
	err = diag_dci_get_buffer(client, data_source, total_len);
	if (err) {
		if (err == -ENOMEM)
			proc_buf->health.dropped_events++;
		else
			pr_err("diag: In %s, invalid packet\n", __func__);
		mutex_unlock(&proc_buf->health_mutex);
		mutex_unlock(&proc_buf->buf_mutex);
		return;
	}

	data_buffer = proc_buf->buf_curr;

	proc_buf->health.received_events++;
	mutex_unlock(&proc_buf->health_mutex);
	mutex_unlock(&proc_buf->buf_mutex);

	mutex_lock(&data_buffer->data_mutex);
	*(int *)(data_buffer->data + data_buffer->data_len) = DCI_EVENT_TYPE;
	data_buffer->data_len += sizeof(int);
	memcpy(data_buffer->data + data_buffer->data_len, buf, len);
	data_buffer->data_len += len;
	data_buffer->data_source = data_source;
	mutex_unlock(&data_buffer->data_mutex);

}

void extract_dci_events(unsigned char *buf, int len, int data_source, int token)
{
	uint16_t event_id, event_id_packet, length, temp_len;
	uint8_t payload_len, payload_len_field;
	uint8_t timestamp[8] = {0}, timestamp_len;
	unsigned char event_data[MAX_EVENT_SIZE];
	unsigned int total_event_len;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	length =  *(uint16_t *)(buf + 1); /* total length of event series */
	if (length == 0) {
		pr_err("diag: Incoming dci event length is invalid\n");
		return;
	}
	/*
	 * Move directly to the start of the event series. 1 byte for
	 * event code and 2 bytes for the length field.
	 * The length field indicates the total length removing the cmd_code
	 * and the lenght field. The event parsing in that case should happen
	 * till the end.
	 */
	temp_len = 3;
	while (temp_len < length) {
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
		mutex_lock(&driver->dci_mutex);
		list_for_each_safe(start, temp, &driver->dci_client_list) {
			entry = list_entry(start, struct diag_dci_client_tbl,
									track);
			if (entry->client_info.token != token)
				continue;
			if (diag_dci_query_event_mask(entry, event_id)) {
				/* copy to client buffer */
				copy_dci_event(event_data, total_event_len,
					       entry, data_source);
			}
		}
		mutex_unlock(&driver->dci_mutex);
	}
}

static void copy_dci_log(unsigned char *buf, int len,
			 struct diag_dci_client_tbl *client, int data_source)
{
	uint16_t log_length = 0;
	struct diag_dci_buffer_t *data_buffer = NULL;
	struct diag_dci_buf_peripheral_t *proc_buf = NULL;
	int err = 0, total_len = 0;

	if (!buf || !client) {
		pr_err("diag: Invalid pointers in %s", __func__);
		return;
	}

	log_length = *(uint16_t *)(buf + 2);
	if (log_length > USHRT_MAX - 4) {
		pr_err("diag: Integer overflow in %s, log_len: %d",
				__func__, log_length);
		return;
	}
	total_len = sizeof(int) + log_length;

	/* Check if we are within the len. The check should include the
	 * first 4 bytes for the Log code(2) and the length bytes (2)
	 */
	if ((log_length + sizeof(uint16_t) + 2) > len) {
		pr_err("diag: Invalid length in %s, log_len: %d, len: %d",
						__func__, log_length, len);
		return;
	}

	proc_buf = &client->buffers[data_source];
	mutex_lock(&proc_buf->buf_mutex);
	mutex_lock(&proc_buf->health_mutex);
	err = diag_dci_get_buffer(client, data_source, total_len);
	if (err) {
		if (err == -ENOMEM)
			proc_buf->health.dropped_logs++;
		else
			pr_err("diag: In %s, invalid packet\n", __func__);
		mutex_unlock(&proc_buf->health_mutex);
		mutex_unlock(&proc_buf->buf_mutex);
		return;
	}

	data_buffer = proc_buf->buf_curr;
	proc_buf->health.received_logs++;
	mutex_unlock(&proc_buf->health_mutex);
	mutex_unlock(&proc_buf->buf_mutex);

	mutex_lock(&data_buffer->data_mutex);
	if (!data_buffer->data) {
		mutex_unlock(&data_buffer->data_mutex);
		return;
	}

	*(int *)(data_buffer->data + data_buffer->data_len) = DCI_LOG_TYPE;
	data_buffer->data_len += sizeof(int);
	memcpy(data_buffer->data + data_buffer->data_len, buf + sizeof(int),
	       log_length);
	data_buffer->data_len += log_length;
	data_buffer->data_source = data_source;
	mutex_unlock(&data_buffer->data_mutex);
}

void extract_dci_log(unsigned char *buf, int len, int data_source, int token)
{
	uint16_t log_code, read_bytes = 0;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	if (!buf) {
		pr_err("diag: In %s buffer is NULL\n", __func__);
		return;
	}

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
	mutex_lock(&driver->dci_mutex);
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client_info.token != token)
			continue;
		if (diag_dci_query_log_mask(entry, log_code)) {
			pr_debug("\t log code %x needed by client %d",
				 log_code, entry->client->tgid);
			/* copy to client buffer */
			copy_dci_log(buf, len, entry, data_source);
		}
	}
	mutex_unlock(&driver->dci_mutex);
}

void diag_dci_channel_open_work(struct work_struct *work)
{
	int i, j;
	char dirty_bits[16];
	uint8_t *client_log_mask_ptr;
	uint8_t *log_mask_ptr;
	int ret;
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
		if (entry->client_info.token != DCI_LOCAL_PROC)
			continue;
		client_log_mask_ptr = entry->dci_log_mask;
		for (j = 0; j < 16; j++) {
			if (*(client_log_mask_ptr+1))
				dirty_bits[j] = 1;
			client_log_mask_ptr += 514;
		}
	}

	mutex_lock(&dci_log_mask_mutex);
	/* Update the appropriate dirty bits in the cumulative mask */
	log_mask_ptr = dci_ops_tbl[DCI_LOCAL_PROC].log_mask_composite;
	for (i = 0; i < 16; i++) {
		if (dirty_bits[i])
			*(log_mask_ptr+1) = dirty_bits[i];

		log_mask_ptr += 514;
	}
	mutex_unlock(&dci_log_mask_mutex);

	/* Send updated mask to userspace clients */
	diag_update_userspace_clients(DCI_LOG_MASKS_TYPE);
	/* Send updated log mask to peripherals */
	ret = dci_ops_tbl[DCI_LOCAL_PROC].send_log_mask(DCI_LOCAL_PROC);

	/* Send updated event mask to userspace clients */
	diag_update_userspace_clients(DCI_EVENT_MASKS_TYPE);
	/* Send updated event mask to peripheral */
	ret = dci_ops_tbl[DCI_LOCAL_PROC].send_event_mask(DCI_LOCAL_PROC);
}

void diag_dci_notify_client(int peripheral_mask, int data, int proc)
{
	int stat;
	struct siginfo info;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	memset(&info, 0, sizeof(struct siginfo));
	info.si_code = SI_QUEUE;
	info.si_int = (peripheral_mask | data);
	if (data == DIAG_STATUS_OPEN)
		dci_ops_tbl[proc].peripheral_status |= peripheral_mask;
	else
		dci_ops_tbl[proc].peripheral_status &= ~peripheral_mask;

	/* Notify the DCI process that the peripheral DCI Channel is up */
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client_info.token != proc)
			continue;
		if (entry->client_info.notification_list & peripheral_mask) {
			info.si_signo = entry->client_info.signal_type;
			stat = send_sig_info(entry->client_info.signal_type,
					     &info, entry->client);
			if (stat)
				pr_err("diag: Err sending dci signal to client, signal data: 0x%x, stat: %d\n",
							info.si_int, stat);
		}
	}
}

static int diag_send_dci_pkt(struct diag_cmd_reg_t *entry,
			     unsigned char *buf, int len, int tag)
{
	int i, status = DIAG_DCI_NO_ERROR;
	uint32_t write_len = 0;
	struct diag_dci_pkt_header_t header;

	if (!entry)
		return -EIO;

	if (len < 1 || len > DIAG_MAX_REQ_SIZE) {
		pr_err("diag: dci: In %s, invalid length %d, max_length: %d\n",
		       __func__, len, (int)(DCI_REQ_BUF_SIZE - sizeof(header)));
		return -EIO;
	}

	if ((len + sizeof(header) + sizeof(uint8_t)) > DCI_BUF_SIZE) {
		pr_err("diag: dci: In %s, invalid length %d for apps_dci_buf, max_length: %d\n",
		       __func__, len, DIAG_MAX_REQ_SIZE);
		return -EIO;
	}

	mutex_lock(&driver->dci_mutex);
	/* prepare DCI packet */
	header.start = CONTROL_CHAR;
	header.version = 1;
	header.len = len + sizeof(int) + sizeof(uint8_t);
	header.pkt_code = DCI_PKT_RSP_CODE;
	header.tag = tag;
	memcpy(driver->apps_dci_buf, &header, sizeof(header));
	write_len += sizeof(header);
	memcpy(driver->apps_dci_buf + write_len , buf, len);
	write_len += len;
	*(uint8_t *)(driver->apps_dci_buf + write_len) = CONTROL_CHAR;
	write_len += sizeof(uint8_t);

	/* This command is registered locally on the Apps */
	if (entry->proc == APPS_DATA) {
		diag_update_pkt_buffer(driver->apps_dci_buf, write_len,
				       DCI_PKT_TYPE);
		diag_update_sleeping_process(entry->pid, DCI_PKT_TYPE);
		mutex_unlock(&driver->dci_mutex);
		return DIAG_DCI_NO_ERROR;
	}

	for (i = 0; i < NUM_PERIPHERALS; i++)
		if (entry->proc == i) {
			status = 1;
			break;
		}

	if (status) {
		status = diag_dci_write_proc(entry->proc,
					     DIAG_DATA_TYPE,
					     driver->apps_dci_buf,
					     write_len);
	} else {
		pr_err("diag: Cannot send packet to peripheral %d",
		       entry->proc);
		status = DIAG_DCI_SEND_DATA_FAIL;
	}
	mutex_unlock(&driver->dci_mutex);
	return status;
}

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
unsigned char *dci_get_buffer_from_bridge(int token)
{
	uint8_t retries = 0, max_retries = 3;
	unsigned char *buf = NULL;

	do {
		buf = diagmem_alloc(driver, DIAG_MDM_BUF_SIZE,
				    dci_ops_tbl[token].mempool);
		if (!buf) {
			usleep_range(5000, 5100);
			retries++;
		} else
			break;
	} while (retries < max_retries);

	return buf;
}

int diag_dci_write_bridge(int token, unsigned char *buf, int len)
{
	return diagfwd_bridge_write(TOKEN_TO_BRIDGE(token), buf, len);
}

int diag_dci_write_done_bridge(int index, unsigned char *buf, int len)
{
	int token = BRIDGE_TO_TOKEN(index);
	if (!VALID_DCI_TOKEN(token)) {
		pr_err("diag: Invalid DCI token %d in %s\n", token, __func__);
		return -EINVAL;
	}
	diagmem_free(driver, buf, dci_ops_tbl[token].mempool);
	return 0;
}
#endif

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
static int diag_send_dci_pkt_remote(unsigned char *data, int len, int tag,
				    int token)
{
	unsigned char *buf = NULL;
	struct diag_dci_header_t dci_header;
	int dci_header_size = sizeof(struct diag_dci_header_t);
	int ret = DIAG_DCI_NO_ERROR;
	uint32_t write_len = 0;

	if (!data)
		return -EIO;

	buf = dci_get_buffer_from_bridge(token);
	if (!buf) {
		pr_err("diag: In %s, unable to get dci buffers to write data\n",
			__func__);
		return -EAGAIN;
	}

	dci_header.start = CONTROL_CHAR;
	dci_header.version = 1;
	/*
	 * The Length of the DCI packet = length of the command + tag (int) +
	 * the command code size (uint8_t)
	 */
	dci_header.length = len + sizeof(int) + sizeof(uint8_t);
	dci_header.cmd_code = DCI_PKT_RSP_CODE;

	memcpy(buf + write_len, &dci_header, dci_header_size);
	write_len += dci_header_size;
	*(int *)(buf + write_len) = tag;
	write_len += sizeof(int);
	memcpy(buf + write_len, data, len);
	write_len += len;
	*(buf + write_len) = CONTROL_CHAR; /* End Terminator */
	write_len += sizeof(uint8_t);

	ret = diag_dci_write_bridge(token, buf, write_len);
	if (ret) {
		pr_err("diag: error writing dci pkt to remote proc, token: %d, err: %d\n",
			token, ret);
		diagmem_free(driver, buf, dci_ops_tbl[token].mempool);
	} else {
		ret = DIAG_DCI_NO_ERROR;
	}

	return ret;
}
#else
static int diag_send_dci_pkt_remote(unsigned char *data, int len, int tag,
				    int token)
{
	return DIAG_DCI_NO_ERROR;
}
#endif

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
int diag_dci_send_handshake_pkt(int index)
{
	int err = 0;
	int token = BRIDGE_TO_TOKEN(index);
	int write_len = 0;
	struct diag_ctrl_dci_handshake_pkt ctrl_pkt;
	unsigned char *buf = NULL;
	struct diag_dci_header_t dci_header;

	if (!VALID_DCI_TOKEN(token)) {
		pr_err("diag: In %s, invalid DCI token %d\n", __func__, token);
		return -EINVAL;
	}

	buf = dci_get_buffer_from_bridge(token);
	if (!buf) {
		pr_err("diag: In %s, unable to get dci buffers to write data\n",
			__func__);
		return -EAGAIN;
	}

	dci_header.start = CONTROL_CHAR;
	dci_header.version = 1;
	/* Include the cmd code (uint8_t) in the length */
	dci_header.length = sizeof(ctrl_pkt) + sizeof(uint8_t);
	dci_header.cmd_code = DCI_CONTROL_PKT_CODE;
	memcpy(buf, &dci_header, sizeof(dci_header));
	write_len += sizeof(dci_header);

	ctrl_pkt.ctrl_pkt_id = DIAG_CTRL_MSG_DCI_HANDSHAKE_PKT;
	/*
	 *  The control packet data length accounts for the version (uint32_t)
	 *  of the packet and the magic number (uint32_t).
	 */
	ctrl_pkt.ctrl_pkt_data_len = 2 * sizeof(uint32_t);
	ctrl_pkt.version = 1;
	ctrl_pkt.magic = DCI_MAGIC;
	memcpy(buf + write_len, &ctrl_pkt, sizeof(ctrl_pkt));
	write_len += sizeof(ctrl_pkt);

	*(uint8_t *)(buf + write_len) = CONTROL_CHAR;
	write_len += sizeof(uint8_t);

	err = diag_dci_write_bridge(token, buf, write_len);
	if (err) {
		pr_err("diag: error writing ack packet to remote proc, token: %d, err: %d\n",
		       token, err);
		diagmem_free(driver, buf, dci_ops_tbl[token].mempool);
		return err;
	}

	mod_timer(&(dci_channel_status[token].wait_time),
		  jiffies + msecs_to_jiffies(DCI_HANDSHAKE_WAIT_TIME));

	return 0;
}
#else
int diag_dci_send_handshake_pkt(int index)
{
	return 0;
}
#endif

static int diag_dci_process_apps_pkt(struct diag_pkt_header_t *pkt_header,
				     unsigned char *req_buf, int req_len,
				     int tag)
{
	uint8_t cmd_code, subsys_id, i, goto_download = 0;
	uint8_t header_len = sizeof(struct diag_dci_pkt_header_t);
	uint16_t ss_cmd_code;
	uint32_t write_len = 0;
	unsigned char *dest_buf = driver->apps_dci_buf;
	unsigned char *payload_ptr = driver->apps_dci_buf + header_len;
	struct diag_dci_pkt_header_t dci_header;

	if (!pkt_header || !req_buf || req_len <= 0 || tag < 0)
		return -EIO;

	cmd_code = pkt_header->cmd_code;
	subsys_id = pkt_header->subsys_id;
	ss_cmd_code = pkt_header->subsys_cmd_code;

	if (cmd_code == DIAG_CMD_DOWNLOAD) {
		*payload_ptr = DIAG_CMD_DOWNLOAD;
		write_len = sizeof(uint8_t);
		goto_download = 1;
		goto fill_buffer;
	} else if (cmd_code == DIAG_CMD_VERSION) {
		if (chk_polling_response()) {
			for (i = 0; i < 55; i++, write_len++, payload_ptr++)
				*(payload_ptr) = 0;
			goto fill_buffer;
		}
	} else if (cmd_code == DIAG_CMD_EXT_BUILD) {
		if (chk_polling_response()) {
			*payload_ptr = DIAG_CMD_EXT_BUILD;
			write_len = sizeof(uint8_t);
			payload_ptr += sizeof(uint8_t);
			for (i = 0; i < 8; i++, write_len++, payload_ptr++)
				*(payload_ptr) = 0;
			*(int *)(payload_ptr) = chk_config_get_id();
			write_len += sizeof(int);
			goto fill_buffer;
		}
	} else if (cmd_code == DIAG_CMD_LOG_ON_DMND) {
		write_len = diag_cmd_log_on_demand(req_buf, req_len,
						   payload_ptr,
						   APPS_BUF_SIZE - header_len);
		goto fill_buffer;
	} else if (cmd_code != DIAG_CMD_DIAG_SUBSYS) {
		return DIAG_DCI_TABLE_ERR;
	}

	if (subsys_id == DIAG_SS_DIAG) {
		if (ss_cmd_code == DIAG_DIAG_MAX_PKT_SZ) {
			memcpy(payload_ptr, pkt_header,
					sizeof(struct diag_pkt_header_t));
			write_len = sizeof(struct diag_pkt_header_t);
			*(uint32_t *)(payload_ptr + write_len) =
							DIAG_MAX_REQ_SIZE;
			write_len += sizeof(uint32_t);
		} else if (ss_cmd_code == DIAG_DIAG_STM) {
			write_len = diag_process_stm_cmd(req_buf, payload_ptr);
		}
	} else if (subsys_id == DIAG_SS_PARAMS) {
		if (ss_cmd_code == DIAG_DIAG_POLL) {
			if (chk_polling_response()) {
				memcpy(payload_ptr, pkt_header,
					sizeof(struct diag_pkt_header_t));
				write_len = sizeof(struct diag_pkt_header_t);
				payload_ptr += write_len;
				for (i = 0; i < 12; i++, write_len++) {
					*(payload_ptr) = 0;
					payload_ptr++;
				}
			}
		} else if (ss_cmd_code == DIAG_DEL_RSP_WRAP) {
			memcpy(payload_ptr, pkt_header,
					sizeof(struct diag_pkt_header_t));
			write_len = sizeof(struct diag_pkt_header_t);
			*(int *)(payload_ptr + write_len) = wrap_enabled;
			write_len += sizeof(int);
		} else if (ss_cmd_code == DIAG_DEL_RSP_WRAP_CNT) {
			wrap_enabled = true;
			memcpy(payload_ptr, pkt_header,
					sizeof(struct diag_pkt_header_t));
			write_len = sizeof(struct diag_pkt_header_t);
			*(uint16_t *)(payload_ptr + write_len) = wrap_count;
			write_len += sizeof(uint16_t);
		} else if (ss_cmd_code == DIAG_EXT_MOBILE_ID) {
			write_len = diag_cmd_get_mobile_id(req_buf, req_len,
						   payload_ptr,
						   APPS_BUF_SIZE - header_len);
		}
	}

fill_buffer:
	if (write_len > 0) {
		/* Check if we are within the range of the buffer*/
		if (write_len + header_len > DIAG_MAX_REQ_SIZE) {
			pr_err("diag: In %s, invalid length %d\n", __func__,
						write_len + header_len);
			return -ENOMEM;
		}
		dci_header.start = CONTROL_CHAR;
		dci_header.version = 1;
		/*
		 * Length of the rsp pkt = actual data len + pkt rsp code
		 * (uint8_t) + tag (int)
		 */
		dci_header.len = write_len + sizeof(uint8_t) + sizeof(int);
		dci_header.pkt_code = DCI_PKT_RSP_CODE;
		dci_header.tag = tag;
		driver->in_busy_dcipktdata = 1;
		memcpy(dest_buf, &dci_header, header_len);
		diag_process_apps_dci_read_data(DCI_PKT_TYPE, dest_buf + 4,
						dci_header.len);
		driver->in_busy_dcipktdata = 0;

		if (goto_download) {
			/*
			 * Sleep for sometime so that the response reaches the
			 * client. The value 5000 empirically as an optimum
			 * time for the response to reach the client.
			 */
			usleep_range(5000, 5100);
			/* call download API */
			msm_set_restart_mode(RESTART_DLOAD);
			pr_alert("diag: download mode set, Rebooting SoC..\n");
			kernel_restart(NULL);
		}
		return DIAG_DCI_NO_ERROR;
	}

	return DIAG_DCI_TABLE_ERR;
}

static int diag_process_dci_pkt_rsp(unsigned char *buf, int len)
{
	int ret = DIAG_DCI_TABLE_ERR;
	int common_cmd = 0;
	struct diag_pkt_header_t *header = NULL;
	unsigned char *temp = buf;
	unsigned char *req_buf = NULL;
	uint8_t retry_count = 0, max_retries = 3;
	uint32_t read_len = 0, req_len = len;
	struct dci_pkt_req_entry_t *req_entry = NULL;
	struct diag_dci_client_tbl *dci_entry = NULL;
	struct dci_pkt_req_t req_hdr;
	struct diag_cmd_reg_t *reg_item;
	struct diag_cmd_reg_entry_t reg_entry;
	struct diag_cmd_reg_entry_t *temp_entry;

	if (!buf)
		return -EIO;

	if (len <= sizeof(struct dci_pkt_req_t) || len > DCI_REQ_BUF_SIZE) {
		pr_err("diag: dci: Invalid length %d len in %s", len, __func__);
		return -EIO;
	}

	req_hdr = *(struct dci_pkt_req_t *)temp;
	temp += sizeof(struct dci_pkt_req_t);
	read_len += sizeof(struct dci_pkt_req_t);
	req_len -= sizeof(struct dci_pkt_req_t);
	req_buf = temp; /* Start of the Request */
	header = (struct diag_pkt_header_t *)temp;
	temp += sizeof(struct diag_pkt_header_t);
	read_len += sizeof(struct diag_pkt_header_t);
	if (read_len >= DCI_REQ_BUF_SIZE) {
		pr_err("diag: dci: In %s, invalid read_len: %d\n", __func__,
		       read_len);
		return -EIO;
	}

	mutex_lock(&driver->dci_mutex);
	dci_entry = diag_dci_get_client_entry(req_hdr.client_id);
	if (!dci_entry) {
		pr_err("diag: Invalid client %d in %s\n",
		       req_hdr.client_id, __func__);
		mutex_unlock(&driver->dci_mutex);
		return DIAG_DCI_NO_REG;
	}

	/* Check if the command is allowed on DCI */
	if (diag_dci_filter_commands(header)) {
		pr_debug("diag: command not supported %d %d %d",
			 header->cmd_code, header->subsys_id,
			 header->subsys_cmd_code);
		mutex_unlock(&driver->dci_mutex);
		return DIAG_DCI_SEND_DATA_FAIL;
	}

	common_cmd = diag_check_common_cmd(header);
	if (common_cmd < 0) {
		pr_debug("diag: error in checking common command, %d\n",
			 common_cmd);
		mutex_unlock(&driver->dci_mutex);
		return DIAG_DCI_SEND_DATA_FAIL;
	}

	/*
	 * Previous packet is yet to be consumed by the client. Wait
	 * till the buffer is free.
	 */
	while (retry_count < max_retries) {
		retry_count++;
		if (driver->in_busy_dcipktdata)
			usleep_range(10000, 10100);
		else
			break;
	}
	/* The buffer is still busy */
	if (driver->in_busy_dcipktdata) {
		pr_err("diag: In %s, apps dci buffer is still busy. Dropping packet\n",
								__func__);
		mutex_unlock(&driver->dci_mutex);
		return -EAGAIN;
	}

	/* Register this new DCI packet */
	req_entry = diag_register_dci_transaction(req_hdr.uid,
						  req_hdr.client_id);
	if (!req_entry) {
		pr_alert("diag: registering new DCI transaction failed\n");
		mutex_unlock(&driver->dci_mutex);
		return DIAG_DCI_NO_REG;
	}
	mutex_unlock(&driver->dci_mutex);

	/*
	 * If the client has registered for remote data, route the packet to the
	 * remote processor
	 */
	if (dci_entry->client_info.token > 0) {
		ret = diag_send_dci_pkt_remote(req_buf, req_len, req_entry->tag,
					       dci_entry->client_info.token);
		return ret;
	}

	/* Check if it is a dedicated Apps command */
	ret = diag_dci_process_apps_pkt(header, req_buf, req_len,
					req_entry->tag);
	if ((ret == DIAG_DCI_NO_ERROR && !common_cmd) || ret < 0)
		return ret;

	reg_entry.cmd_code = header->cmd_code;
	reg_entry.subsys_id = header->subsys_id;
	reg_entry.cmd_code_hi = header->subsys_cmd_code;
	reg_entry.cmd_code_lo = header->subsys_cmd_code;

	temp_entry = diag_cmd_search(&reg_entry, ALL_PROC);
	if (temp_entry) {
		reg_item = container_of(temp_entry, struct diag_cmd_reg_t,
								entry);
		ret = diag_send_dci_pkt(reg_item, req_buf, req_len,
					req_entry->tag);
	} else {
		DIAG_LOG(DIAG_DEBUG_DCI, "Command not found: %02x %02x %02x\n",
				reg_entry.cmd_code, reg_entry.subsys_id,
				reg_entry.cmd_code_hi);
	}

	return ret;
}

int diag_process_dci_transaction(unsigned char *buf, int len)
{
	unsigned char *temp = buf;
	uint16_t log_code, item_num;
	int ret = -1, found = 0, client_id = 0, client_token = 0;
	int count, set_mask, num_codes, bit_index, event_id, offset = 0;
	unsigned int byte_index, read_len = 0;
	uint8_t equip_id, *log_mask_ptr, *head_log_mask_ptr, byte_mask;
	uint8_t *event_mask_ptr;
	struct diag_dci_client_tbl *dci_entry = NULL;

	if (!temp) {
		pr_err("diag: Invalid buffer in %s\n", __func__);
		return -ENOMEM;
	}

	/* This is Pkt request/response transaction */
	if (*(int *)temp > 0) {
		return diag_process_dci_pkt_rsp(buf, len);
	} else if (*(int *)temp == DCI_LOG_TYPE) {
		/* Minimum length of a log mask config is 12 + 2 bytes for
		   atleast one log code to be set or reset */
		if (len < DCI_LOG_CON_MIN_LEN || len > USER_SPACE_DATA) {
			pr_err("diag: dci: Invalid length in %s\n", __func__);
			return -EIO;
		}

		/* Extract each log code and put in client table */
		temp += sizeof(int);
		read_len += sizeof(int);
		client_id = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);
		set_mask = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);
		num_codes = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);

		/* find client table entry */
		mutex_lock(&driver->dci_mutex);
		dci_entry = diag_dci_get_client_entry(client_id);
		if (!dci_entry) {
			pr_err("diag: In %s, invalid client\n", __func__);
			mutex_unlock(&driver->dci_mutex);
			return ret;
		}
		client_token = dci_entry->client_info.token;

		if (num_codes == 0 || (num_codes >= (USER_SPACE_DATA - 8)/2)) {
			pr_err("diag: dci: Invalid number of log codes %d\n",
								num_codes);
			mutex_unlock(&driver->dci_mutex);
			return -EIO;
		}

		head_log_mask_ptr = dci_entry->dci_log_mask;
		if (!head_log_mask_ptr) {
			pr_err("diag: dci: Invalid Log mask pointer in %s\n",
								__func__);
			mutex_unlock(&driver->dci_mutex);
			return -ENOMEM;
		}
		pr_debug("diag: head of dci log mask %p\n", head_log_mask_ptr);
		count = 0; /* iterator for extracting log codes */

		while (count < num_codes) {
			if (read_len >= USER_SPACE_DATA) {
				pr_err("diag: dci: Invalid length for log type in %s",
								__func__);
				mutex_unlock(&driver->dci_mutex);
				return -EIO;
			}
			log_code = *(uint16_t *)temp;
			equip_id = LOG_GET_EQUIP_ID(log_code);
			item_num = LOG_GET_ITEM_NUM(log_code);
			byte_index = item_num/8 + 2;
			if (byte_index >= (DCI_MAX_ITEMS_PER_LOG_CODE+2)) {
				pr_err("diag: dci: Log type, invalid byte index\n");
				mutex_unlock(&driver->dci_mutex);
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
				mutex_unlock(&driver->dci_mutex);
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
				byte_mask, client_token);
			temp += 2;
			read_len += 2;
			count++;
			ret = DIAG_DCI_NO_ERROR;
		}
		/* send updated mask to userspace clients */
		if (client_token == DCI_LOCAL_PROC)
			diag_update_userspace_clients(DCI_LOG_MASKS_TYPE);
		/* send updated mask to peripherals */
		ret = dci_ops_tbl[client_token].send_log_mask(client_token);
		mutex_unlock(&driver->dci_mutex);
	} else if (*(int *)temp == DCI_EVENT_TYPE) {
		/* Minimum length of a event mask config is 12 + 4 bytes for
		  atleast one event id to be set or reset. */
		if (len < DCI_EVENT_CON_MIN_LEN || len > USER_SPACE_DATA) {
			pr_err("diag: dci: Invalid length in %s\n", __func__);
			return -EIO;
		}

		/* Extract each event id and put in client table */
		temp += sizeof(int);
		read_len += sizeof(int);
		client_id = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);
		set_mask = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);
		num_codes = *(int *)temp;
		temp += sizeof(int);
		read_len += sizeof(int);

		/* find client table entry */
		mutex_lock(&driver->dci_mutex);
		dci_entry = diag_dci_get_client_entry(client_id);
		if (!dci_entry) {
			pr_err("diag: In %s, invalid client\n", __func__);
			mutex_unlock(&driver->dci_mutex);
			return ret;
		}
		client_token = dci_entry->client_info.token;

		/* Check for positive number of event ids. Also, the number of
		   event ids should fit in the buffer along with set_mask and
		   num_codes which are 4 bytes each */
		if (num_codes == 0 || (num_codes >= (USER_SPACE_DATA - 8)/2)) {
			pr_err("diag: dci: Invalid number of event ids %d\n",
								num_codes);
			mutex_unlock(&driver->dci_mutex);
			return -EIO;
		}

		event_mask_ptr = dci_entry->dci_event_mask;
		if (!event_mask_ptr) {
			pr_err("diag: dci: Invalid event mask pointer in %s\n",
								__func__);
			mutex_unlock(&driver->dci_mutex);
			return -ENOMEM;
		}
		pr_debug("diag: head of dci event mask %p\n", event_mask_ptr);
		count = 0; /* iterator for extracting log codes */
		while (count < num_codes) {
			if (read_len >= USER_SPACE_DATA) {
				pr_err("diag: dci: Invalid length for event type in %s",
								__func__);
				mutex_unlock(&driver->dci_mutex);
				return -EIO;
			}
			event_id = *(int *)temp;
			byte_index = event_id/8;
			if (byte_index >= DCI_EVENT_MASK_SIZE) {
				pr_err("diag: dci: Event type, invalid byte index\n");
				mutex_unlock(&driver->dci_mutex);
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
			update_dci_cumulative_event_mask(byte_index, byte_mask,
							 client_token);
			temp += sizeof(int);
			read_len += sizeof(int);
			count++;
			ret = DIAG_DCI_NO_ERROR;
		}
		/* send updated mask to userspace clients */
		if (dci_entry->client_info.token == DCI_LOCAL_PROC)
			diag_update_userspace_clients(DCI_EVENT_MASKS_TYPE);
		/* send updated mask to peripherals */
		ret = dci_ops_tbl[client_token].send_event_mask(client_token);
		mutex_unlock(&driver->dci_mutex);
	} else {
		pr_alert("diag: Incorrect DCI transaction\n");
	}
	return ret;
}


struct diag_dci_client_tbl *diag_dci_get_client_entry(int client_id)
{
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client_info.client_id == client_id)
			return entry;
	}
	return NULL;
}

struct diag_dci_client_tbl *dci_lookup_client_entry_pid(int tgid)
{
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client->tgid == tgid)
			return entry;
	}
	return NULL;
}

void update_dci_cumulative_event_mask(int offset, uint8_t byte_mask, int token)
{
	uint8_t *event_mask_ptr;
	uint8_t *update_ptr = dci_ops_tbl[token].event_mask_composite;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	bool is_set = false;

	mutex_lock(&dci_event_mask_mutex);
	update_ptr += offset;
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client_info.token != token)
			continue;
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

void diag_dci_invalidate_cumulative_event_mask(int token)
{
	int i = 0;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	uint8_t *update_ptr, *event_mask_ptr;
	update_ptr = dci_ops_tbl[token].event_mask_composite;

	if (!update_ptr)
		return;

	mutex_lock(&dci_event_mask_mutex);
	create_dci_event_mask_tbl(update_ptr);
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client_info.token != token)
			continue;
		event_mask_ptr = entry->dci_event_mask;
		for (i = 0; i < DCI_EVENT_MASK_SIZE; i++)
			*(update_ptr+i) |= *(event_mask_ptr+i);
	}
	mutex_unlock(&dci_event_mask_mutex);
}

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
int diag_send_dci_event_mask_remote(int token)
{
	unsigned char *buf = NULL;
	struct diag_dci_header_t dci_header;
	struct diag_ctrl_event_mask event_mask;
	int dci_header_size = sizeof(struct diag_dci_header_t);
	int event_header_size = sizeof(struct diag_ctrl_event_mask);
	int i, ret = DIAG_DCI_NO_ERROR, err = DIAG_DCI_NO_ERROR;
	unsigned char *event_mask_ptr = dci_ops_tbl[token].
							event_mask_composite;
	uint32_t write_len = 0;

	buf = dci_get_buffer_from_bridge(token);
	if (!buf) {
		pr_err("diag: In %s, unable to get dci buffers to write data\n",
			__func__);
		return -EAGAIN;
	}

	/* Frame the DCI header */
	dci_header.start = CONTROL_CHAR;
	dci_header.version = 1;
	dci_header.length = event_header_size + DCI_EVENT_MASK_SIZE + 1;
	dci_header.cmd_code = DCI_CONTROL_PKT_CODE;

	event_mask.cmd_type = DIAG_CTRL_MSG_EVENT_MASK;
	event_mask.data_len = EVENT_MASK_CTRL_HEADER_LEN + DCI_EVENT_MASK_SIZE;
	event_mask.stream_id = DCI_MASK_STREAM;
	event_mask.status = DIAG_CTRL_MASK_VALID;
	event_mask.event_config = 0; /* event config */
	event_mask.event_mask_size = DCI_EVENT_MASK_SIZE;
	for (i = 0; i < DCI_EVENT_MASK_SIZE; i++) {
		if (event_mask_ptr[i] != 0) {
			event_mask.event_config = 1;
			break;
		}
	}
	memcpy(buf + write_len, &dci_header, dci_header_size);
	write_len += dci_header_size;
	memcpy(buf + write_len, &event_mask, event_header_size);
	write_len += event_header_size;
	memcpy(buf + write_len, event_mask_ptr, DCI_EVENT_MASK_SIZE);
	write_len += DCI_EVENT_MASK_SIZE;
	*(buf + write_len) = CONTROL_CHAR; /* End Terminator */
	write_len += sizeof(uint8_t);
	err = diag_dci_write_bridge(token, buf, write_len);
	if (err) {
		pr_err("diag: error writing event mask to remote proc, token: %d, err: %d\n",
		       token, err);
		diagmem_free(driver, buf, dci_ops_tbl[token].mempool);
		ret = err;
	} else {
		ret = DIAG_DCI_NO_ERROR;
	}

	return ret;
}
#endif

int diag_send_dci_event_mask(int token)
{
	void *buf = event_mask.update_buf;
	struct diag_ctrl_event_mask header;
	int header_size = sizeof(struct diag_ctrl_event_mask);
	int ret = DIAG_DCI_NO_ERROR, err = DIAG_DCI_NO_ERROR, i;
	unsigned char *event_mask_ptr = dci_ops_tbl[DCI_LOCAL_PROC].
							event_mask_composite;

	mutex_lock(&event_mask.lock);
	/* send event mask update */
	header.cmd_type = DIAG_CTRL_MSG_EVENT_MASK;
	header.data_len = EVENT_MASK_CTRL_HEADER_LEN + DCI_EVENT_MASK_SIZE;
	header.stream_id = DCI_MASK_STREAM;
	header.status = DIAG_CTRL_MASK_VALID;
	header.event_config = 0; /* event config */
	header.event_mask_size = DCI_EVENT_MASK_SIZE;
	for (i = 0; i < DCI_EVENT_MASK_SIZE; i++) {
		if (event_mask_ptr[i] != 0) {
			header.event_config = 1;
			break;
		}
	}
	memcpy(buf, &header, header_size);
	memcpy(buf+header_size, event_mask_ptr, DCI_EVENT_MASK_SIZE);
	for (i = 0; i < NUM_PERIPHERALS; i++) {
		/*
		 * Don't send to peripheral if its regular channel
		 * is down. It may also mean that the peripheral doesn't
		 * support DCI.
		 */
		err = diag_dci_write_proc(i, DIAG_CNTL_TYPE, buf,
					  header_size + DCI_EVENT_MASK_SIZE);
		if (err != DIAG_DCI_NO_ERROR)
			ret = DIAG_DCI_SEND_DATA_FAIL;
	}
	mutex_unlock(&event_mask.lock);

	return ret;
}

void update_dci_cumulative_log_mask(int offset, unsigned int byte_index,
						uint8_t byte_mask, int token)
{
	uint8_t *update_ptr = dci_ops_tbl[token].log_mask_composite;
	uint8_t *log_mask_ptr;
	bool is_set = false;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	mutex_lock(&dci_log_mask_mutex);
	update_ptr += offset;
	/* update the dirty bit */
	*(update_ptr+1) = 1;
	update_ptr = update_ptr + byte_index;
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client_info.token != token)
			continue;
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

void diag_dci_invalidate_cumulative_log_mask(int token)
{
	int i = 0;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;
	uint8_t *update_ptr, *log_mask_ptr;
	update_ptr = dci_ops_tbl[token].log_mask_composite;

	/* Clear the composite mask and redo all the masks */
	mutex_lock(&dci_log_mask_mutex);
	create_dci_log_mask_tbl(update_ptr, DCI_LOG_MASK_DIRTY);
	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->client_info.token != token)
			continue;
		log_mask_ptr = entry->dci_log_mask;
		for (i = 0; i < DCI_LOG_MASK_SIZE; i++)
			*(update_ptr+i) |= *(log_mask_ptr+i);
	}
	mutex_unlock(&dci_log_mask_mutex);
}

static int dci_fill_log_mask(unsigned char *dest_ptr, unsigned char *src_ptr)
{
	struct diag_ctrl_log_mask header;
	int header_len = sizeof(struct diag_ctrl_log_mask);

	header.cmd_type = DIAG_CTRL_MSG_LOG_MASK;
	header.num_items = DCI_MAX_ITEMS_PER_LOG_CODE;
	header.data_len = 11 + DCI_MAX_ITEMS_PER_LOG_CODE;
	header.stream_id = DCI_MASK_STREAM;
	header.status = 3;
	header.equip_id = *src_ptr;
	header.log_mask_size = DCI_MAX_ITEMS_PER_LOG_CODE;
	memcpy(dest_ptr, &header, header_len);
	memcpy(dest_ptr + header_len, src_ptr + 2, DCI_MAX_ITEMS_PER_LOG_CODE);

	return header_len + DCI_MAX_ITEMS_PER_LOG_CODE;
}

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
int diag_send_dci_log_mask_remote(int token)
{

	unsigned char *buf = NULL;
	struct diag_dci_header_t dci_header;
	int dci_header_size = sizeof(struct diag_dci_header_t);
	int log_header_size = sizeof(struct diag_ctrl_log_mask);
	uint8_t *log_mask_ptr = dci_ops_tbl[token].log_mask_composite;
	int i, ret = DIAG_DCI_NO_ERROR, err = DIAG_DCI_NO_ERROR;
	int updated;
	uint32_t write_len = 0;

	/* DCI header is common to all equipment IDs */
	dci_header.start = CONTROL_CHAR;
	dci_header.version = 1;
	dci_header.length = log_header_size + DCI_MAX_ITEMS_PER_LOG_CODE + 1;
	dci_header.cmd_code = DCI_CONTROL_PKT_CODE;

	for (i = 0; i < DCI_MAX_LOG_CODES; i++) {
		updated = 1;
		write_len = 0;
		if (!*(log_mask_ptr + 1)) {
			log_mask_ptr += 514;
			continue;
		}

		buf = dci_get_buffer_from_bridge(token);
		if (!buf) {
			pr_err("diag: In %s, unable to get dci buffers to write data\n",
				__func__);
			return -EAGAIN;
		}

		memcpy(buf + write_len, &dci_header, dci_header_size);
		write_len += dci_header_size;
		write_len += dci_fill_log_mask(buf + write_len, log_mask_ptr);
		*(buf + write_len) = CONTROL_CHAR; /* End Terminator */
		write_len += sizeof(uint8_t);
		err = diag_dci_write_bridge(token, buf, write_len);
		if (err) {
			pr_err("diag: error writing log mask to remote processor, equip_id: %d, token: %d, err: %d\n",
			       i, token, err);
			diagmem_free(driver, buf, dci_ops_tbl[token].mempool);
			updated = 0;
		}
		if (updated)
			*(log_mask_ptr + 1) = 0; /* clear dirty byte */
		log_mask_ptr += 514;
	}

	return ret;
}
#endif

int diag_send_dci_log_mask(int token)
{
	void *buf = log_mask.update_buf;
	int write_len = 0;
	uint8_t *log_mask_ptr = dci_ops_tbl[DCI_LOCAL_PROC].log_mask_composite;
	int i, j, ret = DIAG_DCI_NO_ERROR, err = DIAG_DCI_NO_ERROR;
	int updated;

	mutex_lock(&log_mask.lock);
	for (i = 0; i < 16; i++) {
		updated = 1;
		/* Dirty bit is set don't update the mask for this equip id */
		if (!(*(log_mask_ptr + 1))) {
			log_mask_ptr += 514;
			continue;
		}
		write_len = dci_fill_log_mask(buf, log_mask_ptr);
		for (j = 0; j < NUM_PERIPHERALS && write_len; j++) {
			err = diag_dci_write_proc(j, DIAG_CNTL_TYPE, buf,
						  write_len);
			if (err != DIAG_DCI_NO_ERROR) {
				updated = 0;
				ret = DIAG_DCI_SEND_DATA_FAIL;
			}
		}
		if (updated)
			*(log_mask_ptr+1) = 0; /* clear dirty byte */
		log_mask_ptr += 514;
	}
	mutex_unlock(&log_mask.lock);

	return ret;
}

static int diag_dci_init_local(void)
{
	struct dci_ops_tbl_t *temp = &dci_ops_tbl[DCI_LOCAL_PROC];

	create_dci_log_mask_tbl(temp->log_mask_composite, DCI_LOG_MASK_CLEAN);
	create_dci_event_mask_tbl(temp->event_mask_composite);
	temp->peripheral_status |= DIAG_CON_APSS;

	return 0;
}

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
static void diag_dci_init_handshake_remote(void)
{
	int i;
	struct dci_channel_status_t *temp = NULL;

	for (i = DCI_REMOTE_BASE; i < NUM_DCI_PROC; i++) {
		temp = &dci_channel_status[i];
		temp->id = i;
		setup_timer(&temp->wait_time, dci_chk_handshake, i);
		INIT_WORK(&temp->handshake_work, dci_handshake_work_fn);
	}
}

static int diag_dci_init_remote(void)
{
	int i;
	struct dci_ops_tbl_t *temp = NULL;

	diagmem_init(driver, POOL_TYPE_MDM_DCI_WRITE);

	for (i = DCI_REMOTE_BASE; i < DCI_REMOTE_LAST; i++) {
		temp = &dci_ops_tbl[i];
		create_dci_log_mask_tbl(temp->log_mask_composite,
					DCI_LOG_MASK_CLEAN);
		create_dci_event_mask_tbl(temp->event_mask_composite);
	}

	partial_pkt.data = kzalloc(MAX_DCI_PACKET_SZ, GFP_KERNEL);
	if (!partial_pkt.data) {
		pr_err("diag: Unable to create partial pkt data\n");
		return -ENOMEM;
	}

	partial_pkt.total_len = 0;
	partial_pkt.read_len = 0;
	partial_pkt.remaining = 0;
	partial_pkt.processing = 0;

	diag_dci_init_handshake_remote();

	return 0;
}
#else
static int diag_dci_init_remote(void)
{
	return 0;
}
#endif

static int diag_dci_init_ops_tbl(void)
{
	int err = 0;

	err = diag_dci_init_local();
	if (err)
		goto err;
	err = diag_dci_init_remote();
	if (err)
		goto err;

	return 0;

err:
	return -ENOMEM;
}

int diag_dci_init(void)
{
	int ret = 0;

	driver->dci_tag = 0;
	driver->dci_client_id = 0;
	driver->num_dci_client = 0;
	mutex_init(&driver->dci_mutex);
	mutex_init(&dci_log_mask_mutex);
	mutex_init(&dci_event_mask_mutex);
	spin_lock_init(&ws_lock);

	ret = diag_dci_init_ops_tbl();
	if (ret)
		goto err;

	if (driver->apps_dci_buf == NULL) {
		driver->apps_dci_buf = kzalloc(DCI_BUF_SIZE, GFP_KERNEL);
		if (driver->apps_dci_buf == NULL)
			goto err;
	}
	INIT_LIST_HEAD(&driver->dci_client_list);
	INIT_LIST_HEAD(&driver->dci_req_list);

	driver->diag_dci_wq = create_singlethread_workqueue("diag_dci_wq");
	if (!driver->diag_dci_wq)
		goto err;

	INIT_WORK(&dci_data_drain_work, dci_data_drain_work_fn);

	setup_timer(&dci_drain_timer, dci_drain_data, 0);
	return DIAG_DCI_NO_ERROR;
err:
	pr_err("diag: Could not initialize diag DCI buffers");
	kfree(driver->apps_dci_buf);

	if (driver->diag_dci_wq)
		destroy_workqueue(driver->diag_dci_wq);
	kfree(partial_pkt.data);
	mutex_destroy(&driver->dci_mutex);
	mutex_destroy(&dci_log_mask_mutex);
	mutex_destroy(&dci_event_mask_mutex);
	return DIAG_DCI_NO_REG;
}

void diag_dci_channel_init(void)
{
	uint8_t peripheral;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		diagfwd_open(peripheral, TYPE_DCI);
		diagfwd_open(peripheral, TYPE_DCI_CMD);
	}
}

void diag_dci_exit(void)
{
	kfree(partial_pkt.data);
	kfree(driver->apps_dci_buf);
	mutex_destroy(&driver->dci_mutex);
	mutex_destroy(&dci_log_mask_mutex);
	mutex_destroy(&dci_event_mask_mutex);
	destroy_workqueue(driver->diag_dci_wq);
}

int diag_dci_clear_log_mask(int client_id)
{
	int err = DIAG_DCI_NO_ERROR, token = DCI_LOCAL_PROC;
	uint8_t *update_ptr;
	struct diag_dci_client_tbl *entry = NULL;

	entry = diag_dci_get_client_entry(client_id);
	if (!entry) {
		pr_err("diag: In %s, invalid client entry\n", __func__);
		return DIAG_DCI_TABLE_ERR;
	}
	token = entry->client_info.token;
	update_ptr = dci_ops_tbl[token].log_mask_composite;

	create_dci_log_mask_tbl(entry->dci_log_mask, DCI_LOG_MASK_CLEAN);
	diag_dci_invalidate_cumulative_log_mask(token);

	/*
	 * Send updated mask to userspace clients only if the client
	 * is registered on the local processor
	 */
	if (token == DCI_LOCAL_PROC)
		diag_update_userspace_clients(DCI_LOG_MASKS_TYPE);
	/* Send updated mask to peripherals */
	err = dci_ops_tbl[token].send_log_mask(token);
	return err;
}

int diag_dci_clear_event_mask(int client_id)
{
	int err = DIAG_DCI_NO_ERROR, token = DCI_LOCAL_PROC;
	uint8_t *update_ptr;
	struct diag_dci_client_tbl *entry = NULL;

	entry = diag_dci_get_client_entry(client_id);
	if (!entry) {
		pr_err("diag: In %s, invalid client entry\n", __func__);
		return DIAG_DCI_TABLE_ERR;
	}
	token = entry->client_info.token;
	update_ptr = dci_ops_tbl[token].event_mask_composite;

	create_dci_event_mask_tbl(entry->dci_event_mask);
	diag_dci_invalidate_cumulative_event_mask(token);

	/*
	 * Send updated mask to userspace clients only if the client is
	 * registerted on the local processor
	 */
	if (token == DCI_LOCAL_PROC)
		diag_update_userspace_clients(DCI_EVENT_MASKS_TYPE);
	/* Send updated mask to peripherals */
	err = dci_ops_tbl[token].send_event_mask(token);
	return err;
}

uint8_t diag_dci_get_cumulative_real_time(int token)
{
	uint8_t real_time = MODE_NONREALTIME;
	struct list_head *start, *temp;
	struct diag_dci_client_tbl *entry = NULL;

	list_for_each_safe(start, temp, &driver->dci_client_list) {
		entry = list_entry(start, struct diag_dci_client_tbl, track);
		if (entry->real_time == MODE_REALTIME &&
					entry->client_info.token == token) {
			real_time = 1;
			break;
		}
	}
	return real_time;
}

int diag_dci_set_real_time(struct diag_dci_client_tbl *entry, uint8_t real_time)
{
	if (!entry) {
		pr_err("diag: In %s, invalid client entry\n", __func__);
		return 0;
	}
	entry->real_time = real_time;
	return 1;
}

int diag_dci_register_client(struct diag_dci_reg_tbl_t *reg_entry)
{
	int i, err = 0;
	struct diag_dci_client_tbl *new_entry = NULL;
	struct diag_dci_buf_peripheral_t *proc_buf = NULL;

	if (!reg_entry)
		return DIAG_DCI_NO_REG;
	if (!VALID_DCI_TOKEN(reg_entry->token)) {
		pr_alert("diag: Invalid DCI client token, %d\n",
						reg_entry->token);
		return DIAG_DCI_NO_REG;
	}

	if (driver->dci_state == DIAG_DCI_NO_REG)
		return DIAG_DCI_NO_REG;

	if (driver->num_dci_client >= MAX_DCI_CLIENTS)
		return DIAG_DCI_NO_REG;

	new_entry = kzalloc(sizeof(struct diag_dci_client_tbl), GFP_KERNEL);
	if (new_entry == NULL) {
		pr_err("diag: unable to alloc memory\n");
		return DIAG_DCI_NO_REG;
	}

	mutex_lock(&driver->dci_mutex);

	new_entry->client = current;
	new_entry->client_info.notification_list =
				reg_entry->notification_list;
	new_entry->client_info.signal_type =
				reg_entry->signal_type;
	new_entry->client_info.token = reg_entry->token;
	switch (reg_entry->token) {
	case DCI_LOCAL_PROC:
		new_entry->num_buffers = NUM_DCI_PERIPHERALS;
		break;
	case DCI_MDM_PROC:
		new_entry->num_buffers = 1;
		break;
	}
	new_entry->real_time = MODE_REALTIME;
	new_entry->in_service = 0;
	INIT_LIST_HEAD(&new_entry->list_write_buf);
	mutex_init(&new_entry->write_buf_mutex);
	new_entry->dci_log_mask =  kzalloc(DCI_LOG_MASK_SIZE, GFP_KERNEL);
	if (!new_entry->dci_log_mask) {
		pr_err("diag: Unable to create log mask for client, %d",
							driver->dci_client_id);
		goto fail_alloc;
	}
	create_dci_log_mask_tbl(new_entry->dci_log_mask, DCI_LOG_MASK_CLEAN);

	new_entry->dci_event_mask =  kzalloc(DCI_EVENT_MASK_SIZE, GFP_KERNEL);
	if (!new_entry->dci_event_mask) {
		pr_err("diag: Unable to create event mask for client, %d",
							driver->dci_client_id);
		goto fail_alloc;
	}
	create_dci_event_mask_tbl(new_entry->dci_event_mask);

	new_entry->buffers = kzalloc(new_entry->num_buffers *
				     sizeof(struct diag_dci_buf_peripheral_t),
				     GFP_KERNEL);
	if (!new_entry->buffers) {
		pr_err("diag: Unable to allocate buffers for peripherals in %s\n",
								__func__);
		goto fail_alloc;
	}

	for (i = 0; i < new_entry->num_buffers; i++) {
		proc_buf = &new_entry->buffers[i];
		if (!proc_buf)
			goto fail_alloc;

		mutex_init(&proc_buf->health_mutex);
		mutex_init(&proc_buf->buf_mutex);
		proc_buf->health.dropped_events = 0;
		proc_buf->health.dropped_logs = 0;
		proc_buf->health.received_events = 0;
		proc_buf->health.received_logs = 0;
		proc_buf->buf_primary = kzalloc(
					sizeof(struct diag_dci_buffer_t),
					GFP_KERNEL);
		if (!proc_buf->buf_primary)
			goto fail_alloc;
		proc_buf->buf_cmd = kzalloc(sizeof(struct diag_dci_buffer_t),
					    GFP_KERNEL);
		if (!proc_buf->buf_cmd)
			goto fail_alloc;
		err = diag_dci_init_buffer(proc_buf->buf_primary,
					   DCI_BUF_PRIMARY);
		if (err)
			goto fail_alloc;
		err = diag_dci_init_buffer(proc_buf->buf_cmd, DCI_BUF_CMD);
		if (err)
			goto fail_alloc;
		proc_buf->buf_curr = proc_buf->buf_primary;
	}

	list_add_tail(&new_entry->track, &driver->dci_client_list);
	driver->dci_client_id++;
	new_entry->client_info.client_id = driver->dci_client_id;
	reg_entry->client_id = driver->dci_client_id;
	driver->num_dci_client++;
	if (driver->num_dci_client == 1)
		diag_update_proc_vote(DIAG_PROC_DCI, VOTE_UP, reg_entry->token);
	queue_work(driver->diag_real_time_wq, &driver->diag_real_time_work);
	mutex_unlock(&driver->dci_mutex);

	return driver->dci_client_id;

fail_alloc:
	if (new_entry) {
		for (i = 0; i < new_entry->num_buffers; i++) {
			proc_buf = &new_entry->buffers[i];
			if (proc_buf) {
				mutex_destroy(&proc_buf->health_mutex);
				if (proc_buf->buf_primary) {
					kfree(proc_buf->buf_primary->data);
					mutex_destroy(
					   &proc_buf->buf_primary->data_mutex);
				}
				kfree(proc_buf->buf_primary);
				if (proc_buf->buf_cmd) {
					kfree(proc_buf->buf_cmd->data);
					mutex_destroy(
					   &proc_buf->buf_cmd->data_mutex);
				}
				kfree(proc_buf->buf_cmd);
			}
		}
		kfree(new_entry->dci_event_mask);
		kfree(new_entry->dci_log_mask);
	}
	kfree(new_entry);
	mutex_unlock(&driver->dci_mutex);
	return DIAG_DCI_NO_REG;
}

int diag_dci_deinit_client(struct diag_dci_client_tbl *entry)
{
	int ret = DIAG_DCI_NO_ERROR, real_time = MODE_REALTIME, i, peripheral;
	struct diag_dci_buf_peripheral_t *proc_buf = NULL;
	struct diag_dci_buffer_t *buf_entry, *temp;
	struct list_head *start, *req_temp;
	struct dci_pkt_req_entry_t *req_entry = NULL;
	int token = DCI_LOCAL_PROC;

	if (!entry)
		return DIAG_DCI_NOT_SUPPORTED;

	token = entry->client_info.token;

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
	diag_dci_invalidate_cumulative_log_mask(token);
	if (token == DCI_LOCAL_PROC)
		diag_update_userspace_clients(DCI_LOG_MASKS_TYPE);
	ret = dci_ops_tbl[token].send_log_mask(token);
	if (ret != DIAG_DCI_NO_ERROR) {
		mutex_unlock(&driver->dci_mutex);
		return ret;
	}
	kfree(entry->dci_event_mask);
	diag_dci_invalidate_cumulative_event_mask(token);
	if (token == DCI_LOCAL_PROC)
		diag_update_userspace_clients(DCI_EVENT_MASKS_TYPE);
	ret = dci_ops_tbl[token].send_event_mask(token);
	if (ret != DIAG_DCI_NO_ERROR) {
		mutex_unlock(&driver->dci_mutex);
		return ret;
	}

	list_for_each_safe(start, req_temp, &driver->dci_req_list) {
		req_entry = list_entry(start, struct dci_pkt_req_entry_t,
				       track);
		if (req_entry->client_id == entry->client_info.client_id) {
			list_del(&req_entry->track);
			kfree(req_entry);
		}
	}

	/* Clean up any buffer that is pending write */
	mutex_lock(&entry->write_buf_mutex);
	list_for_each_entry_safe(buf_entry, temp, &entry->list_write_buf,
							buf_track) {
		list_del(&buf_entry->buf_track);
		if (buf_entry->buf_type == DCI_BUF_SECONDARY) {
			mutex_lock(&buf_entry->data_mutex);
			diagmem_free(driver, buf_entry->data, POOL_TYPE_DCI);
			buf_entry->data = NULL;
			mutex_unlock(&buf_entry->data_mutex);
			kfree(buf_entry);
		} else if (buf_entry->buf_type == DCI_BUF_CMD) {
			peripheral = buf_entry->data_source;
			if (peripheral == APPS_DATA)
				continue;
		}
		/*
		 * These are buffers that can't be written to the client which
		 * means that the copy cannot be completed. Make sure that we
		 * remove those references in DCI wakeup source.
		 */
		diag_ws_on_copy_fail(DIAG_WS_DCI);
	}
	mutex_unlock(&entry->write_buf_mutex);

	for (i = 0; i < entry->num_buffers; i++) {
		proc_buf = &entry->buffers[i];
		buf_entry = proc_buf->buf_curr;
		mutex_lock(&proc_buf->buf_mutex);
		/* Clean up secondary buffer from mempool that is active */
		if (buf_entry && buf_entry->buf_type == DCI_BUF_SECONDARY) {
			mutex_lock(&buf_entry->data_mutex);
			diagmem_free(driver, buf_entry->data, POOL_TYPE_DCI);
			buf_entry->data = NULL;
			mutex_unlock(&buf_entry->data_mutex);
			mutex_destroy(&buf_entry->data_mutex);
			kfree(buf_entry);
		}

		mutex_lock(&proc_buf->buf_primary->data_mutex);
		kfree(proc_buf->buf_primary->data);
		mutex_unlock(&proc_buf->buf_primary->data_mutex);

		mutex_lock(&proc_buf->buf_cmd->data_mutex);
		kfree(proc_buf->buf_cmd->data);
		mutex_unlock(&proc_buf->buf_cmd->data_mutex);

		mutex_destroy(&proc_buf->health_mutex);
		mutex_destroy(&proc_buf->buf_primary->data_mutex);
		mutex_destroy(&proc_buf->buf_cmd->data_mutex);

		kfree(proc_buf->buf_primary);
		kfree(proc_buf->buf_cmd);
		mutex_unlock(&proc_buf->buf_mutex);
	}
	mutex_destroy(&entry->write_buf_mutex);

	kfree(entry);

	if (driver->num_dci_client == 0) {
		diag_update_proc_vote(DIAG_PROC_DCI, VOTE_DOWN, token);
	} else {
		real_time = diag_dci_get_cumulative_real_time(token);
		diag_update_real_time_vote(DIAG_PROC_DCI, real_time, token);
	}
	queue_work(driver->diag_real_time_wq, &driver->diag_real_time_work);

	mutex_unlock(&driver->dci_mutex);

	return DIAG_DCI_NO_ERROR;
}

int diag_dci_write_proc(uint8_t peripheral, int pkt_type, char *buf, int len)
{
	uint8_t dest_channel = TYPE_DATA;
	int err = 0;

	if (!buf || peripheral >= NUM_PERIPHERALS || len < 0 ||
	    !(driver->feature[PERIPHERAL_MODEM].rcvd_feature_mask)) {
		DIAG_LOG(DIAG_DEBUG_DCI,
			"buf: 0x%p, p: %d, len: %d, f_mask: %d\n",
				buf, peripheral, len,
				driver->feature[peripheral].rcvd_feature_mask);
		return -EINVAL;
	}

	if (pkt_type == DIAG_DATA_TYPE) {
		dest_channel = TYPE_DCI_CMD;
	} else if (pkt_type == DIAG_CNTL_TYPE) {
		dest_channel = TYPE_CNTL;
	} else {
		pr_err("diag: Invalid DCI pkt type in %s", __func__);
		return -EINVAL;
	}

	err = diagfwd_write(peripheral, dest_channel, buf, len);
	if (err && err != -ENODEV) {
		pr_err("diag: In %s, unable to write to peripheral: %d, type: %d, len: %d, err: %d\n",
		       __func__, peripheral, dest_channel, len, err);
	} else {
		err = DIAG_DCI_NO_ERROR;
	}

	return err;
}

int diag_dci_copy_health_stats(struct diag_dci_health_stats_proc *stats_proc)
{
	struct diag_dci_client_tbl *entry = NULL;
	struct diag_dci_health_t *health = NULL;
	struct diag_dci_health_stats *stats = NULL;
	int i, proc;

	if (!stats_proc)
		return -EINVAL;

	stats = &stats_proc->health;
	proc = stats_proc->proc;
	if (proc < ALL_PROC || proc > APPS_DATA)
		return -EINVAL;

	entry = diag_dci_get_client_entry(stats_proc->client_id);
	if (!entry)
		return DIAG_DCI_NOT_SUPPORTED;

	/*
	 * If the client has registered for remote processor, the
	 * proc field doesn't have any effect as they have only one buffer.
	 */
	if (entry->client_info.token)
		proc = 0;

	stats->stats.dropped_logs = 0;
	stats->stats.dropped_events = 0;
	stats->stats.received_logs = 0;
	stats->stats.received_events = 0;

	if (proc != ALL_PROC) {
		health = &entry->buffers[proc].health;
		stats->stats.dropped_logs = health->dropped_logs;
		stats->stats.dropped_events = health->dropped_events;
		stats->stats.received_logs = health->received_logs;
		stats->stats.received_events = health->received_events;
		if (stats->reset_status) {
			mutex_lock(&entry->buffers[proc].health_mutex);
			health->dropped_logs = 0;
			health->dropped_events = 0;
			health->received_logs = 0;
			health->received_events = 0;
			mutex_unlock(&entry->buffers[proc].health_mutex);
		}
		return DIAG_DCI_NO_ERROR;
	}

	for (i = 0; i < entry->num_buffers; i++) {
		health = &entry->buffers[i].health;
		stats->stats.dropped_logs += health->dropped_logs;
		stats->stats.dropped_events += health->dropped_events;
		stats->stats.received_logs += health->received_logs;
		stats->stats.received_events += health->received_events;
		if (stats->reset_status) {
			mutex_lock(&entry->buffers[i].health_mutex);
			health->dropped_logs = 0;
			health->dropped_events = 0;
			health->received_logs = 0;
			health->received_events = 0;
			mutex_unlock(&entry->buffers[i].health_mutex);
		}
	}
	return DIAG_DCI_NO_ERROR;
}

int diag_dci_get_support_list(struct diag_dci_peripherals_t *support_list)
{
	if (!support_list)
		return -ENOMEM;

	if (!VALID_DCI_TOKEN(support_list->proc))
		return -EIO;

	support_list->list = dci_ops_tbl[support_list->proc].peripheral_status;
	return DIAG_DCI_NO_ERROR;
}
