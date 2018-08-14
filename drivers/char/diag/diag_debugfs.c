/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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

#ifdef CONFIG_DEBUG_FS

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include "diagchar.h"
#include "diagfwd.h"
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
#include "diagfwd_bridge.h"
#endif
#ifdef CONFIG_USB_QCOM_DIAG_BRIDGE
#include "diagfwd_hsic.h"
#include "diagfwd_smux.h"
#endif
#ifdef CONFIG_MSM_MHI
#include "diagfwd_mhi.h"
#endif
#include "diagmem.h"
#include "diag_dci.h"
#include "diag_usb.h"
#include "diagfwd_peripheral.h"
#include "diagfwd_smd.h"
#include "diagfwd_socket.h"
#include "diagfwd_glink.h"
#include "diag_debugfs.h"
#include "diag_ipc_logging.h"

#define DEBUG_BUF_SIZE	4096
static struct dentry *diag_dbgfs_dent;
static int diag_dbgfs_table_index;
static int diag_dbgfs_mempool_index;
static int diag_dbgfs_usbinfo_index;
static int diag_dbgfs_smdinfo_index;
static int diag_dbgfs_socketinfo_index;
static int diag_dbgfs_glinkinfo_index;
static int diag_dbgfs_hsicinfo_index;
static int diag_dbgfs_mhiinfo_index;
static int diag_dbgfs_bridgeinfo_index;
static int diag_dbgfs_finished;
static int diag_dbgfs_dci_data_index;
static int diag_dbgfs_dci_finished;
static struct mutex diag_dci_dbgfs_mutex;
static ssize_t diag_dbgfs_read_status(struct file *file, char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	char *buf;
	int ret, i;
	unsigned int buf_size;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	buf_size = ksize(buf);
	ret = scnprintf(buf, buf_size,
		"CPU Tools ID: %d\n"
		"Check Polling Response: %d\n"
		"Polling Registered: %d\n"
		"Uses Device Tree: %d\n"
		"Apps Supports Separate CMDRSP: %d\n"
		"Apps Supports HDLC Encoding: %d\n"
		"Apps Supports Header Untagging: %d\n"
		"Apps Supports Sockets: %d\n"
		"Logging Mode: %d\n"
		"RSP Buffer is Busy: %d\n"
		"HDLC Disabled: %d\n"
		"Time Sync Enabled: %d\n"
		"MD session mode: %d\n"
		"MD session mask: %d\n"
		"Uses Time API: %d\n"
		"Supports PD buffering: %d\n",
		chk_config_get_id(),
		chk_polling_response(),
		driver->polling_reg_flag,
		driver->use_device_tree,
		driver->supports_separate_cmdrsp,
		driver->supports_apps_hdlc_encoding,
		driver->supports_apps_header_untagging,
		driver->supports_sockets,
		driver->logging_mode,
		driver->rsp_buf_busy,
		driver->hdlc_disabled,
		driver->time_sync_enabled,
		driver->md_session_mode,
		driver->md_session_mask,
		driver->uses_time_api,
		driver->supports_pd_buffering);

	for (i = 0; i < NUM_PERIPHERALS; i++) {
		ret += scnprintf(buf+ret, buf_size-ret,
			"p: %s Feature: %02x %02x |%c%c%c%c%c%c%c%c%c%c|\n",
			PERIPHERAL_STRING(i),
			driver->feature[i].feature_mask[0],
			driver->feature[i].feature_mask[1],
			driver->feature[i].rcvd_feature_mask ? 'F':'f',
			driver->feature[i].separate_cmd_rsp ? 'C':'c',
			driver->feature[i].encode_hdlc ? 'H':'h',
			driver->feature[i].peripheral_buffering ? 'B':'b',
			driver->feature[i].mask_centralization ? 'M':'m',
			driver->feature[i].pd_buffering ? 'P':'p',
			driver->feature[i].stm_support ? 'Q':'q',
			driver->feature[i].sockets_enabled ? 'S':'s',
			driver->feature[i].sent_feature_mask ? 'T':'t',
			driver->feature[i].untag_header ? 'U':'u');
	}

#ifdef CONFIG_DIAG_OVER_USB
	ret += scnprintf(buf+ret, buf_size-ret,
		"USB Connected: %d\n",
		driver->usb_connected);
#endif

	for (i = 0; i < DIAG_NUM_PROC; i++) {
		ret += scnprintf(buf+ret, buf_size-ret,
				 "Real Time Mode: %d: %d\n", i,
				 driver->real_time_mode[i]);
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);

	kfree(buf);
	return ret;
}

static ssize_t diag_dbgfs_read_dcistats(struct file *file,
				char __user *ubuf, size_t count, loff_t *ppos)
{
	char *buf = NULL;
	unsigned int bytes_remaining, bytes_written = 0;
	unsigned int bytes_in_buf = 0, i = 0;
	struct diag_dci_data_info *temp_data = dci_traffic;
	unsigned int buf_size;

	buf_size = (count > DEBUG_BUF_SIZE) ? DEBUG_BUF_SIZE : count;

	if (diag_dbgfs_dci_finished) {
		diag_dbgfs_dci_finished = 0;
		return 0;
	}

	buf = kcalloc(buf_size, sizeof(char), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	buf_size = ksize(buf);
	bytes_remaining = buf_size;

	mutex_lock(&diag_dci_dbgfs_mutex);
	if (diag_dbgfs_dci_data_index == 0) {
		bytes_written =
			scnprintf(buf, buf_size,
			"number of clients: %d\n"
			"dci proc active: %d\n"
			"dci real time vote: %d\n",
			driver->num_dci_client,
			(driver->proc_active_mask & DIAG_PROC_DCI) ? 1 : 0,
			(driver->proc_rt_vote_mask[DIAG_LOCAL_PROC] &
							DIAG_PROC_DCI) ? 1 : 0);
		bytes_in_buf += bytes_written;
		bytes_remaining -= bytes_written;
#ifdef CONFIG_DIAG_OVER_USB
		bytes_written = scnprintf(buf+bytes_in_buf, bytes_remaining,
			"usb_connected: %d\n",
			driver->usb_connected);
		bytes_in_buf += bytes_written;
		bytes_remaining -= bytes_written;
#endif
		bytes_written = scnprintf(buf+bytes_in_buf,
					  bytes_remaining,
					  "dci power: active, relax: %lu, %lu\n",
					  driver->diag_dev->power.wakeup->
						active_count,
					  driver->diag_dev->
						power.wakeup->relax_count);
		bytes_in_buf += bytes_written;
		bytes_remaining -= bytes_written;

	}
	temp_data += diag_dbgfs_dci_data_index;
	for (i = diag_dbgfs_dci_data_index; i < DIAG_DCI_DEBUG_CNT; i++) {
		if (temp_data->iteration != 0) {
			bytes_written = scnprintf(
				buf + bytes_in_buf, bytes_remaining,
				"i %-5ld\t"
				"s %-5d\t"
				"p %-5d\t"
				"r %-5d\t"
				"c %-5d\t"
				"t %-15s\n",
				temp_data->iteration,
				temp_data->data_size,
				temp_data->peripheral,
				temp_data->proc,
				temp_data->ch_type,
				temp_data->time_stamp);
			bytes_in_buf += bytes_written;
			bytes_remaining -= bytes_written;
			/* Check if there is room for another entry */
			if (bytes_remaining < bytes_written)
				break;
		}
		temp_data++;
	}
	diag_dbgfs_dci_data_index = (i >= DIAG_DCI_DEBUG_CNT) ? 0 : i + 1;
	mutex_unlock(&diag_dci_dbgfs_mutex);
	bytes_written = simple_read_from_buffer(ubuf, count, ppos, buf,
								bytes_in_buf);
	kfree(buf);
	diag_dbgfs_dci_finished = 1;
	return bytes_written;
}

static ssize_t diag_dbgfs_read_power(struct file *file, char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	char *buf;
	int ret;
	unsigned int buf_size;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf_size = ksize(buf);
	ret = scnprintf(buf, buf_size,
		"DCI reference count: %d\n"
		"DCI copy count: %d\n"
		"DCI Client Count: %d\n\n"
		"Memory Device reference count: %d\n"
		"Memory Device copy count: %d\n"
		"Logging mode: %d\n\n"
		"Wakeup source active count: %lu\n"
		"Wakeup source relax count: %lu\n\n",
		driver->dci_ws.ref_count,
		driver->dci_ws.copy_count,
		driver->num_dci_client,
		driver->md_ws.ref_count,
		driver->md_ws.copy_count,
		driver->logging_mode,
		driver->diag_dev->power.wakeup->active_count,
		driver->diag_dev->power.wakeup->relax_count);

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);

	kfree(buf);
	return ret;
}

static ssize_t diag_dbgfs_read_table(struct file *file, char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	char *buf;
	int ret = 0;
	int i = 0;
	int is_polling = 0;
	unsigned int bytes_remaining;
	unsigned int bytes_in_buffer = 0;
	unsigned int bytes_written;
	unsigned int buf_size;
	struct list_head *start;
	struct list_head *temp;
	struct diag_cmd_reg_t *item = NULL;

	mutex_lock(&driver->cmd_reg_mutex);
	if (diag_dbgfs_table_index == driver->cmd_reg_count) {
		diag_dbgfs_table_index = 0;
		mutex_unlock(&driver->cmd_reg_mutex);
		return 0;
	}

	buf_size = (count > DEBUG_BUF_SIZE) ? DEBUG_BUF_SIZE : count;

	buf = kcalloc(buf_size, sizeof(char), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		mutex_unlock(&driver->cmd_reg_mutex);
		return -ENOMEM;
	}
	buf_size = ksize(buf);
	bytes_remaining = buf_size;

	if (diag_dbgfs_table_index == 0) {
		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
					  "Client ids: Modem: %d, LPASS: %d, WCNSS: %d, SLPI: %d, APPS: %d\n",
					  PERIPHERAL_MODEM, PERIPHERAL_LPASS,
					  PERIPHERAL_WCNSS, PERIPHERAL_SENSORS,
					  APPS_DATA);
		bytes_in_buffer += bytes_written;
		bytes_remaining -= bytes_written;
	}

	list_for_each_safe(start, temp, &driver->cmd_reg_list) {
		item = list_entry(start, struct diag_cmd_reg_t, link);
		if (i < diag_dbgfs_table_index) {
			i++;
			continue;
		}

		is_polling = diag_cmd_chk_polling(&item->entry);
		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
					  "i: %3d, cmd_code: %4x, subsys_id: %4x, cmd_code_lo: %4x, cmd_code_hi: %4x, proc: %d, process_id: %5d %s\n",
					  i++,
					  item->entry.cmd_code,
					  item->entry.subsys_id,
					  item->entry.cmd_code_lo,
					  item->entry.cmd_code_hi,
					  item->proc,
					  item->pid,
					  (is_polling == DIAG_CMD_POLLING) ?
					  "<-- Polling Cmd" : "");

		bytes_in_buffer += bytes_written;

		/* Check if there is room to add another table entry */
		bytes_remaining = buf_size - bytes_in_buffer;

		if (bytes_remaining < bytes_written)
			break;
	}
	diag_dbgfs_table_index = i;
	mutex_unlock(&driver->cmd_reg_mutex);

	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}

static ssize_t diag_dbgfs_read_mempool(struct file *file, char __user *ubuf,
						size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int ret = 0;
	int i = 0;
	unsigned int buf_size;
	unsigned int bytes_remaining = 0;
	unsigned int bytes_written = 0;
	unsigned int bytes_in_buffer = 0;
	struct diag_mempool_t *mempool = NULL;

	if (diag_dbgfs_mempool_index >= NUM_MEMORY_POOLS) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_mempool_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;
	bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"%-24s\t"
			"%-10s\t"
			"%-5s\t"
			"%-5s\t"
			"%-5s\n",
			"POOL", "HANDLE", "COUNT", "SIZE", "ITEMSIZE");
	bytes_in_buffer += bytes_written;
	bytes_remaining = buf_size - bytes_in_buffer;

	for (i = diag_dbgfs_mempool_index; i < NUM_MEMORY_POOLS; i++) {
		mempool = &diag_mempools[i];
		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"%-24s\t"
			"%-10p\t"
			"%-5d\t"
			"%-5d\t"
			"%-5d\n",
			mempool->name,
			mempool->pool,
			mempool->count,
			mempool->poolsize,
			mempool->itemsize);
		bytes_in_buffer += bytes_written;

		/* Check if there is room to add another table entry */
		bytes_remaining = buf_size - bytes_in_buffer;

		if (bytes_remaining < bytes_written)
			break;
	}
	diag_dbgfs_mempool_index = i+1;
	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}

static ssize_t diag_dbgfs_read_usbinfo(struct file *file, char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int ret = 0;
	int i = 0;
	unsigned int buf_size;
	unsigned int bytes_remaining = 0;
	unsigned int bytes_written = 0;
	unsigned int bytes_in_buffer = 0;
	struct diag_usb_info *usb_info = NULL;

	if (diag_dbgfs_usbinfo_index >= NUM_DIAG_USB_DEV) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_usbinfo_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;
	for (i = diag_dbgfs_usbinfo_index; i < NUM_DIAG_USB_DEV; i++) {
		usb_info = &diag_usb[i];
		if (!usb_info->enabled)
			continue;
		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"id: %d\n"
			"name: %s\n"
			"hdl: %pK\n"
			"connected: %d\n"
			"diag state: %d\n"
			"enabled: %d\n"
			"mempool: %s\n"
			"read pending: %d\n"
			"read count: %lu\n"
			"write count: %lu\n"
			"read work pending: %d\n"
			"read done work pending: %d\n"
			"connect work pending: %d\n"
			"disconnect work pending: %d\n"
			"max size supported: %d\n\n",
			usb_info->id,
			usb_info->name,
			usb_info->hdl,
			atomic_read(&usb_info->connected),
			atomic_read(&usb_info->diag_state),
			usb_info->enabled,
			DIAG_MEMPOOL_GET_NAME(usb_info->mempool),
			atomic_read(&usb_info->read_pending),
			usb_info->read_cnt,
			usb_info->write_cnt,
			work_pending(&usb_info->read_work),
			work_pending(&usb_info->read_done_work),
			work_pending(&usb_info->connect_work),
			work_pending(&usb_info->disconnect_work),
			usb_info->max_size);
		bytes_in_buffer += bytes_written;

		/* Check if there is room to add another table entry */
		bytes_remaining = buf_size - bytes_in_buffer;

		if (bytes_remaining < bytes_written)
			break;
	}
	diag_dbgfs_usbinfo_index = i+1;
	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}

#ifdef CONFIG_DIAG_USES_SMD
static ssize_t diag_dbgfs_read_smdinfo(struct file *file, char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int ret = 0;
	int i = 0;
	int j = 0;
	unsigned int buf_size;
	unsigned int bytes_remaining = 0;
	unsigned int bytes_written = 0;
	unsigned int bytes_in_buffer = 0;
	struct diag_smd_info *smd_info = NULL;
	struct diagfwd_info *fwd_ctxt = NULL;

	if (diag_dbgfs_smdinfo_index >= NUM_PERIPHERALS) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_smdinfo_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf_size = DEBUG_BUF_SIZE;
	bytes_remaining = buf_size;
	for (i = 0; i < NUM_TYPES; i++) {
		for (j = 0; j < NUM_PERIPHERALS; j++) {
			switch (i) {
			case TYPE_DATA:
				smd_info = &smd_data[j];
				break;
			case TYPE_CNTL:
				smd_info = &smd_cntl[j];
				break;
			case TYPE_DCI:
				smd_info = &smd_dci[j];
				break;
			case TYPE_CMD:
				smd_info = &smd_cmd[j];
				break;
			case TYPE_DCI_CMD:
				smd_info = &smd_dci_cmd[j];
				break;
			default:
				return -EINVAL;
			}

			fwd_ctxt = (struct diagfwd_info *)(smd_info->fwd_ctxt);

			bytes_written = scnprintf(buf+bytes_in_buffer,
				bytes_remaining,
				"name\t\t:\t%s\n"
				"hdl\t\t:\t%pK\n"
				"inited\t\t:\t%d\n"
				"opened\t\t:\t%d\n"
				"diag_state\t:\t%d\n"
				"fifo size\t:\t%d\n"
				"open pending\t:\t%d\n"
				"close pending\t:\t%d\n"
				"read pending\t:\t%d\n"
				"buf_1 busy\t:\t%d\n"
				"buf_2 busy\t:\t%d\n"
				"bytes read\t:\t%lu\n"
				"bytes written\t:\t%lu\n"
				"fwd inited\t:\t%d\n"
				"fwd opened\t:\t%d\n"
				"fwd ch_open\t:\t%d\n\n",
				smd_info->name,
				smd_info->hdl,
				smd_info->inited,
				atomic_read(&smd_info->opened),
				atomic_read(&smd_info->diag_state),
				smd_info->fifo_size,
				work_pending(&smd_info->open_work),
				work_pending(&smd_info->close_work),
				work_pending(&smd_info->read_work),
				(fwd_ctxt && fwd_ctxt->buf_1) ?
				atomic_read(&fwd_ctxt->buf_1->in_busy) : -1,
				(fwd_ctxt && fwd_ctxt->buf_2) ?
				atomic_read(&fwd_ctxt->buf_2->in_busy) : -1,
				(fwd_ctxt) ? fwd_ctxt->read_bytes : 0,
				(fwd_ctxt) ? fwd_ctxt->write_bytes : 0,
				(fwd_ctxt) ? fwd_ctxt->inited : -1,
				(fwd_ctxt) ?
				atomic_read(&fwd_ctxt->opened) : -1,
				(fwd_ctxt) ? fwd_ctxt->ch_open : -1);
			bytes_in_buffer += bytes_written;

			/* Check if there is room to add another table entry */
			bytes_remaining = buf_size - bytes_in_buffer;

			if (bytes_remaining < bytes_written)
				break;
		}
	}
	diag_dbgfs_smdinfo_index = i+1;
	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}
#endif

static ssize_t diag_dbgfs_read_socketinfo(struct file *file, char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int ret = 0;
	int i = 0;
	int j = 0;
	unsigned int buf_size;
	unsigned int bytes_remaining = 0;
	unsigned int bytes_written = 0;
	unsigned int bytes_in_buffer = 0;
	struct diag_socket_info *info = NULL;
	struct diagfwd_info *fwd_ctxt = NULL;

	if (diag_dbgfs_socketinfo_index >= NUM_PERIPHERALS) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_socketinfo_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;
	for (i = 0; i < NUM_TYPES; i++) {
		for (j = 0; j < NUM_PERIPHERALS; j++) {
			switch (i) {
			case TYPE_DATA:
				info = &socket_data[j];
				break;
			case TYPE_CNTL:
				info = &socket_cntl[j];
				break;
			case TYPE_DCI:
				info = &socket_dci[j];
				break;
			case TYPE_CMD:
				info = &socket_cmd[j];
				break;
			case TYPE_DCI_CMD:
				info = &socket_dci_cmd[j];
				break;
			default:
				return -EINVAL;
			}

			fwd_ctxt = (struct diagfwd_info *)(info->fwd_ctxt);

			bytes_written = scnprintf(buf+bytes_in_buffer,
				bytes_remaining,
				"name\t\t:\t%s\n"
				"hdl\t\t:\t%pK\n"
				"inited\t\t:\t%d\n"
				"opened\t\t:\t%d\n"
				"diag_state\t:\t%d\n"
				"buf_1 busy\t:\t%d\n"
				"buf_2 busy\t:\t%d\n"
				"flow ctrl count\t:\t%d\n"
				"data_ready\t:\t%d\n"
				"init pending\t:\t%d\n"
				"read pending\t:\t%d\n"
				"bytes read\t:\t%lu\n"
				"bytes written\t:\t%lu\n"
				"fwd inited\t:\t%d\n"
				"fwd opened\t:\t%d\n"
				"fwd ch_open\t:\t%d\n\n",
				info->name,
				info->hdl,
				info->inited,
				atomic_read(&info->opened),
				atomic_read(&info->diag_state),
				(fwd_ctxt && fwd_ctxt->buf_1) ?
				atomic_read(&fwd_ctxt->buf_1->in_busy) : -1,
				(fwd_ctxt && fwd_ctxt->buf_2) ?
				atomic_read(&fwd_ctxt->buf_2->in_busy) : -1,
				atomic_read(&info->flow_cnt),
				info->data_ready,
				work_pending(&info->init_work),
				work_pending(&info->read_work),
				(fwd_ctxt) ? fwd_ctxt->read_bytes : 0,
				(fwd_ctxt) ? fwd_ctxt->write_bytes : 0,
				(fwd_ctxt) ? fwd_ctxt->inited : -1,
				(fwd_ctxt) ?
				atomic_read(&fwd_ctxt->opened) : -1,
				(fwd_ctxt) ? fwd_ctxt->ch_open : -1);
			bytes_in_buffer += bytes_written;

			/* Check if there is room to add another table entry */
			bytes_remaining = buf_size - bytes_in_buffer;

			if (bytes_remaining < bytes_written)
				break;
		}
	}
	diag_dbgfs_socketinfo_index = i+1;
	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}

static ssize_t diag_dbgfs_read_glinkinfo(struct file *file, char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int ret = 0;
	int i = 0;
	int j = 0;
	unsigned int buf_size;
	unsigned int bytes_remaining = 0;
	unsigned int bytes_written = 0;
	unsigned int bytes_in_buffer = 0;
	struct diag_glink_info *info = NULL;
	struct diagfwd_info *fwd_ctxt = NULL;

	if (diag_dbgfs_glinkinfo_index >= NUM_PERIPHERALS) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_socketinfo_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf_size = ksize(buf);
	bytes_remaining = buf_size;
	for (i = 0; i < NUM_TYPES; i++) {
		for (j = 0; j < NUM_PERIPHERALS; j++) {
			switch (i) {
			case TYPE_DATA:
				info = &glink_data[j];
				break;
			case TYPE_CNTL:
				info = &glink_cntl[j];
				break;
			case TYPE_DCI:
				info = &glink_dci[j];
				break;
			case TYPE_CMD:
				info = &glink_cmd[j];
				break;
			case TYPE_DCI_CMD:
				info = &glink_dci_cmd[j];
				break;
			default:
				return -EINVAL;
			}

			fwd_ctxt = (struct diagfwd_info *)(info->fwd_ctxt);

			bytes_written = scnprintf(buf+bytes_in_buffer,
				bytes_remaining,
				"name\t\t:\t%s\n"
				"hdl\t\t:\t%pK\n"
				"inited\t\t:\t%d\n"
				"opened\t\t:\t%d\n"
				"diag_state\t:\t%d\n"
				"buf_1 busy\t:\t%d\n"
				"buf_2 busy\t:\t%d\n"
				"tx_intent_ready\t:\t%d\n"
				"open pending\t:\t%d\n"
				"close pending\t:\t%d\n"
				"read pending\t:\t%d\n"
				"bytes read\t:\t%lu\n"
				"bytes written\t:\t%lu\n"
				"fwd inited\t:\t%d\n"
				"fwd opened\t:\t%d\n"
				"fwd ch_open\t:\t%d\n\n",
				info->name,
				info->hdl,
				info->inited,
				atomic_read(&info->opened),
				atomic_read(&info->diag_state),
				(fwd_ctxt && fwd_ctxt->buf_1) ?
				atomic_read(&fwd_ctxt->buf_1->in_busy) : -1,
				(fwd_ctxt && fwd_ctxt->buf_2) ?
				atomic_read(&fwd_ctxt->buf_2->in_busy) : -1,
				atomic_read(&info->tx_intent_ready),
				work_pending(&info->open_work),
				work_pending(&info->close_work),
				work_pending(&info->read_work),
				(fwd_ctxt) ? fwd_ctxt->read_bytes : 0,
				(fwd_ctxt) ? fwd_ctxt->write_bytes : 0,
				(fwd_ctxt) ? fwd_ctxt->inited : -1,
				(fwd_ctxt) ?
				atomic_read(&fwd_ctxt->opened) : -1,
				(fwd_ctxt) ? fwd_ctxt->ch_open : -1);
			bytes_in_buffer += bytes_written;

			/* Check if there is room to add another table entry */
			bytes_remaining = buf_size - bytes_in_buffer;

			if (bytes_remaining < bytes_written)
				break;
		}
	}
	diag_dbgfs_glinkinfo_index = i+1;
	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}

#ifdef CONFIG_IPC_LOGGING
static ssize_t diag_dbgfs_write_debug(struct file *fp, const char __user *buf,
				      size_t count, loff_t *ppos)
{
	const int size = 10;
	unsigned char cmd[size];
	long value = 0;
	int len = 0;

	if (count < 1)
		return -EINVAL;

	len = (count < (size - 1)) ? count : size - 1;
	if (copy_from_user(cmd, buf, len))
		return -EFAULT;

	cmd[len] = 0;
	if (cmd[len-1] == '\n') {
		cmd[len-1] = 0;
		len--;
	}

	if (kstrtol(cmd, 10, &value))
		return -EINVAL;

	if (value < 0)
		return -EINVAL;

	diag_debug_mask = (uint16_t)value;
	return count;
}
#endif

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
#ifdef CONFIG_USB_QCOM_DIAG_BRIDGE
static ssize_t diag_dbgfs_read_hsicinfo(struct file *file, char __user *ubuf,
					size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int ret = 0;
	int i = 0;
	unsigned int buf_size;
	unsigned int bytes_remaining = 0;
	unsigned int bytes_written = 0;
	unsigned int bytes_in_buffer = 0;
	struct diag_hsic_info *hsic_info = NULL;

	if (diag_dbgfs_hsicinfo_index >= NUM_DIAG_USB_DEV) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_hsicinfo_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;
	for (i = diag_dbgfs_hsicinfo_index; i < NUM_HSIC_DEV; i++) {
		hsic_info = &diag_hsic[i];
		if (!hsic_info->enabled)
			continue;
		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"id: %d\n"
			"name: %s\n"
			"bridge index: %s\n"
			"opened: %d\n"
			"enabled: %d\n"
			"suspended: %d\n"
			"mempool: %s\n"
			"read work pending: %d\n"
			"open work pending: %d\n"
			"close work pending: %d\n\n",
			hsic_info->id,
			hsic_info->name,
			DIAG_BRIDGE_GET_NAME(hsic_info->dev_id),
			hsic_info->opened,
			hsic_info->enabled,
			hsic_info->suspended,
			DIAG_MEMPOOL_GET_NAME(hsic_info->mempool),
			work_pending(&hsic_info->read_work),
			work_pending(&hsic_info->open_work),
			work_pending(&hsic_info->close_work));
		bytes_in_buffer += bytes_written;

		/* Check if there is room to add another table entry */
		bytes_remaining = buf_size - bytes_in_buffer;

		if (bytes_remaining < bytes_written)
			break;
	}
	diag_dbgfs_hsicinfo_index = i+1;
	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}

const struct file_operations diag_dbgfs_hsicinfo_ops = {
	.read = diag_dbgfs_read_hsicinfo,
};
#endif
#ifdef CONFIG_MSM_MHI
static ssize_t diag_dbgfs_read_mhiinfo(struct file *file, char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int ret = 0;
	int i = 0;
	unsigned int buf_size;
	unsigned int bytes_remaining = 0;
	unsigned int bytes_written = 0;
	unsigned int bytes_in_buffer = 0;
	struct diag_mhi_info *mhi_info = NULL;

	if (diag_dbgfs_mhiinfo_index >= NUM_MHI_DEV) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_mhiinfo_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;
	for (i = diag_dbgfs_mhiinfo_index; i < NUM_MHI_DEV; i++) {
		mhi_info = &diag_mhi[i];
		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"id: %d\n"
			"name: %s\n"
			"bridge index: %s\n"
			"mempool: %s\n"
			"read ch opened: %d\n"
			"read ch hdl: %pK\n"
			"write ch opened: %d\n"
			"write ch hdl: %pK\n"
			"read work pending: %d\n"
			"read done work pending: %d\n"
			"open work pending: %d\n"
			"close work pending: %d\n\n",
			mhi_info->id,
			mhi_info->name,
			DIAG_BRIDGE_GET_NAME(mhi_info->dev_id),
			DIAG_MEMPOOL_GET_NAME(mhi_info->mempool),
			atomic_read(&mhi_info->read_ch.opened),
			mhi_info->read_ch.hdl,
			atomic_read(&mhi_info->write_ch.opened),
			mhi_info->write_ch.hdl,
			work_pending(&mhi_info->read_work),
			work_pending(&mhi_info->read_done_work),
			work_pending(&mhi_info->open_work),
			work_pending(&mhi_info->close_work));
		bytes_in_buffer += bytes_written;

		/* Check if there is room to add another table entry */
		bytes_remaining = buf_size - bytes_in_buffer;

		if (bytes_remaining < bytes_written)
			break;
	}
	diag_dbgfs_mhiinfo_index = i+1;
	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}


const struct file_operations diag_dbgfs_mhiinfo_ops = {
	.read = diag_dbgfs_read_mhiinfo,
};

#endif
static ssize_t diag_dbgfs_read_bridge(struct file *file, char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int ret = 0;
	int i = 0;
	unsigned int buf_size;
	unsigned int bytes_remaining = 0;
	unsigned int bytes_written = 0;
	unsigned int bytes_in_buffer = 0;
	struct diagfwd_bridge_info *info = NULL;

	if (diag_dbgfs_bridgeinfo_index >= NUM_DIAG_USB_DEV) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_bridgeinfo_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;
	for (i = diag_dbgfs_bridgeinfo_index; i < NUM_REMOTE_DEV; i++) {
		info = &bridge_info[i];
		if (!info->inited)
			continue;
		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"id: %d\n"
			"name: %s\n"
			"type: %d\n"
			"inited: %d\n"
			"ctxt: %d\n"
			"dev_ops: %pK\n"
			"dci_read_buf: %pK\n"
			"dci_read_ptr: %pK\n"
			"dci_read_len: %d\n\n",
			info->id,
			info->name,
			info->type,
			info->inited,
			info->ctxt,
			info->dev_ops,
			info->dci_read_buf,
			info->dci_read_ptr,
			info->dci_read_len);
		bytes_in_buffer += bytes_written;

		/* Check if there is room to add another table entry */
		bytes_remaining = buf_size - bytes_in_buffer;

		if (bytes_remaining < bytes_written)
			break;
	}
	diag_dbgfs_bridgeinfo_index = i+1;
	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	kfree(buf);
	return ret;
}

const struct file_operations diag_dbgfs_bridge_ops = {
	.read = diag_dbgfs_read_bridge,
};

#endif

const struct file_operations diag_dbgfs_status_ops = {
	.read = diag_dbgfs_read_status,
};

#ifdef CONFIG_DIAG_USES_SMD
static const struct file_operations diag_dbgfs_smdinfo_ops = {
	.read = diag_dbgfs_read_smdinfo,
};
#endif

const struct file_operations diag_dbgfs_socketinfo_ops = {
	.read = diag_dbgfs_read_socketinfo,
};

const struct file_operations diag_dbgfs_glinkinfo_ops = {
	.read = diag_dbgfs_read_glinkinfo,
};

const struct file_operations diag_dbgfs_table_ops = {
	.read = diag_dbgfs_read_table,
};

const struct file_operations diag_dbgfs_mempool_ops = {
	.read = diag_dbgfs_read_mempool,
};

const struct file_operations diag_dbgfs_usbinfo_ops = {
	.read = diag_dbgfs_read_usbinfo,
};

const struct file_operations diag_dbgfs_dcistats_ops = {
	.read = diag_dbgfs_read_dcistats,
};

const struct file_operations diag_dbgfs_power_ops = {
	.read = diag_dbgfs_read_power,
};

#ifdef CONFIG_IPC_LOGGING
const struct file_operations diag_dbgfs_debug_ops = {
	.write = diag_dbgfs_write_debug
};
#endif

int diag_debugfs_init(void)
{
	struct dentry *entry = NULL;

	diag_dbgfs_dent = debugfs_create_dir("diag", 0);
	if (IS_ERR(diag_dbgfs_dent))
		return -ENOMEM;

	entry = debugfs_create_file("status", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_status_ops);
	if (!entry)
		goto err;

#ifdef CONFIG_DIAG_USES_SMD
	entry = debugfs_create_file("smdinfo", 0444, diag_dbgfs_dent, NULL,
				    &diag_dbgfs_smdinfo_ops);
	if (!entry)
		goto err;
#endif

	entry = debugfs_create_file("socketinfo", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_socketinfo_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("glinkinfo", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_glinkinfo_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("table", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_table_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("mempool", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_mempool_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("usbinfo", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_usbinfo_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("dci_stats", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_dcistats_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("power", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_power_ops);
	if (!entry)
		goto err;

#ifdef CONFIG_IPC_LOGGING
	entry = debugfs_create_file("debug", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_debug_ops);
	if (!entry)
		goto err;
#endif
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	entry = debugfs_create_file("bridge", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_bridge_ops);
	if (!entry)
		goto err;
#ifdef CONFIG_USB_QCOM_DIAG_BRIDGE
	entry = debugfs_create_file("hsicinfo", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_hsicinfo_ops);
	if (!entry)
		goto err;
#endif
#ifdef CONFIG_MSM_MHI
	entry = debugfs_create_file("mhiinfo", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_mhiinfo_ops);
	if (!entry)
		goto err;
#endif
#endif
	diag_dbgfs_table_index = 0;
	diag_dbgfs_mempool_index = 0;
	diag_dbgfs_usbinfo_index = 0;
	diag_dbgfs_smdinfo_index = 0;
	diag_dbgfs_socketinfo_index = 0;
	diag_dbgfs_hsicinfo_index = 0;
	diag_dbgfs_bridgeinfo_index = 0;
	diag_dbgfs_mhiinfo_index = 0;
	diag_dbgfs_finished = 0;
	diag_dbgfs_dci_data_index = 0;
	diag_dbgfs_dci_finished = 0;

	/* DCI related structures */
	dci_traffic = kzalloc(sizeof(struct diag_dci_data_info) *
				DIAG_DCI_DEBUG_CNT, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(dci_traffic))
		pr_warn("diag: could not allocate memory for dci debug info\n");

	mutex_init(&dci_stat_mutex);
	mutex_init(&diag_dci_dbgfs_mutex);
	return 0;
err:
	kfree(dci_traffic);
	debugfs_remove_recursive(diag_dbgfs_dent);
	return -ENOMEM;
}

void diag_debugfs_cleanup(void)
{
	debugfs_remove_recursive(diag_dbgfs_dent);
	diag_dbgfs_dent = NULL;
	kfree(dci_traffic);
	mutex_destroy(&dci_stat_mutex);
	mutex_destroy(&diag_dci_dbgfs_mutex);
}
#else
int diag_debugfs_init(void) { return 0; }
void diag_debugfs_cleanup(void) { }
#endif
