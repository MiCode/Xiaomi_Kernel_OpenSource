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

#ifdef CONFIG_DEBUG_FS

#include <linux/slab.h>
#include <linux/debugfs.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_bridge.h"
#include "diagfwd_hsic.h"
#include "diagfwd_smux.h"
#include "diagmem.h"
#include "diag_dci.h"

#define DEBUG_BUF_SIZE	4096
static struct dentry *diag_dbgfs_dent;
static int diag_dbgfs_table_index;
static int diag_dbgfs_mempool_index;
static int diag_dbgfs_finished;
static int diag_dbgfs_dci_data_index;
static int diag_dbgfs_dci_finished;

static ssize_t diag_dbgfs_read_status(struct file *file, char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	char *buf;
	int ret, i;
	unsigned int buf_size;
	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}
	buf_size = ksize(buf);
	ret = scnprintf(buf, buf_size,
		"modem ch: 0x%p\n"
		"lpass ch: 0x%p\n"
		"riva ch: 0x%p\n"
		"modem dci ch: 0x%p\n"
		"lpass dci ch: 0x%p\n"
		"modem cntl_ch: 0x%p\n"
		"lpass cntl_ch: 0x%p\n"
		"riva cntl_ch: 0x%p\n"
		"modem cmd ch: 0x%p\n"
		"modem dci cmd ch: 0x%p\n"
		"CPU Tools id: %d\n"
		"Apps only: %d\n"
		"Apps master: %d\n"
		"Check Polling Response: %d\n"
		"polling_reg_flag: %d\n"
		"uses device tree: %d\n"
		"supports separate cmdrsp: %d\n"
		"Modem separate cmdrsp: %d\n"
		"LPASS separate cmdrsp: %d\n"
		"RIVA separate cmdrsp: %d\n"
		"Modem in_busy_1: %d\n"
		"Modem in_busy_2: %d\n"
		"LPASS in_busy_1: %d\n"
		"LPASS in_busy_2: %d\n"
		"RIVA in_busy_1: %d\n"
		"RIVA in_busy_2: %d\n"
		"DCI Modem in_busy_1: %d\n"
		"Modem CMD in_busy_1: %d\n"
		"Modem CMD in_busy_2: %d\n"
		"DCI CMD Modem in_busy_1: %d\n"
		"Modem supports STM: %d\n"
		"LPASS supports STM: %d\n"
		"RIVA supports STM: %d\n"
		"Modem STM state: %d\n"
		"LPASS STM state: %d\n"
		"RIVA STM state: %d\n"
		"APPS STM state: %d\n"
		"Modem STM requested state: %d\n"
		"LPASS STM requested state: %d\n"
		"RIVA STM requested state: %d\n"
		"APPS STM requested state: %d\n"
		"supports apps hdlc encoding: %d\n"
		"Modem hdlc encoding: %d\n"
		"Lpass hdlc encoding: %d\n"
		"RIVA hdlc encoding: %d\n"
		"Modem CMD hdlc encoding: %d\n"
		"Modem DATA in_buf_1_size: %d\n"
		"Modem DATA in_buf_2_size: %d\n"
		"ADSP DATA in_buf_1_size: %d\n"
		"ADSP DATA in_buf_2_size: %d\n"
		"RIVA DATA in_buf_1_size: %d\n"
		"RIVA DATA in_buf_2_size: %d\n"
		"Modem DATA in_buf_1_raw_size: %d\n"
		"Modem DATA in_buf_2_raw_size: %d\n"
		"ADSP DATA in_buf_1_raw_size: %d\n"
		"ADSP DATA in_buf_2_raw_size: %d\n"
		"RIVA DATA in_buf_1_raw_size: %d\n"
		"RIVA DATA in_buf_2_raw_size: %d\n"
		"Modem CMD in_buf_1_size: %d\n"
		"Modem CMD in_buf_1_raw_size: %d\n"
		"Modem CNTL in_buf_1_size: %d\n"
		"ADSP CNTL in_buf_1_size: %d\n"
		"RIVA CNTL in_buf_1_size: %d\n"
		"Modem DCI in_buf_1_size: %d\n"
		"Modem DCI CMD in_buf_1_size: %d\n"
		"Received Feature mask from Modem: %d\n"
		"Received Feature mask from LPASS: %d\n"
		"Received Feature mask from WCNSS: %d\n"
		"Mask Centralization Support on Modem: %d\n"
		"Mask Centralization Support on LPASS: %d\n"
		"Mask Centralization Support on WCNSS: %d\n"
		"logging_mode: %d\n"
		"rsp_in_busy: %d\n",
		driver->smd_data[MODEM_DATA].ch,
		driver->smd_data[LPASS_DATA].ch,
		driver->smd_data[WCNSS_DATA].ch,
		driver->smd_dci[MODEM_DATA].ch,
		driver->smd_dci[LPASS_DATA].ch,
		driver->smd_cntl[MODEM_DATA].ch,
		driver->smd_cntl[LPASS_DATA].ch,
		driver->smd_cntl[WCNSS_DATA].ch,
		driver->smd_cmd[MODEM_DATA].ch,
		driver->smd_dci_cmd[MODEM_DATA].ch,
		chk_config_get_id(),
		chk_apps_only(),
		chk_apps_master(),
		chk_polling_response(),
		driver->polling_reg_flag,
		driver->use_device_tree,
		driver->supports_separate_cmdrsp,
		driver->separate_cmdrsp[MODEM_DATA],
		driver->separate_cmdrsp[LPASS_DATA],
		driver->separate_cmdrsp[WCNSS_DATA],
		driver->smd_data[MODEM_DATA].in_busy_1,
		driver->smd_data[MODEM_DATA].in_busy_2,
		driver->smd_data[LPASS_DATA].in_busy_1,
		driver->smd_data[LPASS_DATA].in_busy_2,
		driver->smd_data[WCNSS_DATA].in_busy_1,
		driver->smd_data[WCNSS_DATA].in_busy_2,
		driver->smd_dci[MODEM_DATA].in_busy_1,
		driver->smd_cmd[MODEM_DATA].in_busy_1,
		driver->smd_cmd[MODEM_DATA].in_busy_2,
		driver->smd_dci_cmd[MODEM_DATA].in_busy_1,
		driver->peripheral_supports_stm[MODEM_DATA],
		driver->peripheral_supports_stm[LPASS_DATA],
		driver->peripheral_supports_stm[WCNSS_DATA],
		driver->stm_state[MODEM_DATA],
		driver->stm_state[LPASS_DATA],
		driver->stm_state[WCNSS_DATA],
		driver->stm_state[APPS_DATA],
		driver->stm_state_requested[MODEM_DATA],
		driver->stm_state_requested[LPASS_DATA],
		driver->stm_state_requested[WCNSS_DATA],
		driver->stm_state_requested[APPS_DATA],
		driver->supports_apps_hdlc_encoding,
		driver->smd_data[MODEM_DATA].encode_hdlc,
		driver->smd_data[LPASS_DATA].encode_hdlc,
		driver->smd_data[WCNSS_DATA].encode_hdlc,
		driver->smd_cmd[MODEM_DATA].encode_hdlc,
		(unsigned int)driver->smd_data[MODEM_DATA].buf_in_1_size,
		(unsigned int)driver->smd_data[MODEM_DATA].buf_in_2_size,
		(unsigned int)driver->smd_data[LPASS_DATA].buf_in_1_size,
		(unsigned int)driver->smd_data[LPASS_DATA].buf_in_2_size,
		(unsigned int)driver->smd_data[WCNSS_DATA].buf_in_1_size,
		(unsigned int)driver->smd_data[WCNSS_DATA].buf_in_2_size,
		(unsigned int)driver->smd_data[MODEM_DATA].buf_in_1_raw_size,
		(unsigned int)driver->smd_data[MODEM_DATA].buf_in_2_raw_size,
		(unsigned int)driver->smd_data[LPASS_DATA].buf_in_1_raw_size,
		(unsigned int)driver->smd_data[LPASS_DATA].buf_in_2_raw_size,
		(unsigned int)driver->smd_data[WCNSS_DATA].buf_in_1_raw_size,
		(unsigned int)driver->smd_data[WCNSS_DATA].buf_in_2_raw_size,
		(unsigned int)driver->smd_cmd[MODEM_DATA].buf_in_1_size,
		(unsigned int)driver->smd_cmd[MODEM_DATA].buf_in_1_raw_size,
		(unsigned int)driver->smd_cntl[MODEM_DATA].buf_in_1_size,
		(unsigned int)driver->smd_cntl[LPASS_DATA].buf_in_1_size,
		(unsigned int)driver->smd_cntl[WCNSS_DATA].buf_in_1_size,
		(unsigned int)driver->smd_dci[MODEM_DATA].buf_in_1_size,
		(unsigned int)driver->smd_dci_cmd[MODEM_DATA].buf_in_1_size,
		driver->rcvd_feature_mask[MODEM_DATA],
		driver->rcvd_feature_mask[LPASS_DATA],
		driver->rcvd_feature_mask[WCNSS_DATA],
		driver->mask_centralization[MODEM_DATA],
		driver->mask_centralization[LPASS_DATA],
		driver->mask_centralization[WCNSS_DATA],
		driver->logging_mode,
		driver->rsp_buf_busy);

#ifdef CONFIG_DIAG_OVER_USB
	ret += scnprintf(buf+ret, buf_size-ret,
		"usb_connected: %d\n",
		driver->usb_connected);
#endif

	for (i = 0; i < DIAG_NUM_PROC; i++) {
		ret += scnprintf(buf+ret, buf_size-ret,
				 "real_time proc: %d: %d\n", i,
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
	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;

	if (diag_dbgfs_dci_finished) {
		diag_dbgfs_dci_finished = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;

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
	if (!buf) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

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

static ssize_t diag_dbgfs_read_workpending(struct file *file,
				char __user *ubuf, size_t count, loff_t *ppos)
{
	char *buf;
	int ret;
	unsigned int buf_size;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	ret = scnprintf(buf, buf_size,
		"Pending status for work_stucts:\n"
		"diag_drain_work: %d\n"
		"Modem data diag_read_smd_work: %d\n"
		"LPASS data diag_read_smd_work: %d\n"
		"RIVA data diag_read_smd_work: %d\n"
		"Modem cntl diag_read_smd_work: %d\n"
		"LPASS cntl diag_read_smd_work: %d\n"
		"RIVA cntl diag_read_smd_work: %d\n"
		"Modem dci diag_read_smd_work: %d\n"
		"Modem data diag_notify_update_smd_work: %d\n"
		"LPASS data diag_notify_update_smd_work: %d\n"
		"RIVA data diag_notify_update_smd_work: %d\n"
		"Modem cntl diag_notify_update_smd_work: %d\n"
		"LPASS cntl diag_notify_update_smd_work: %d\n"
		"RIVA cntl diag_notify_update_smd_work: %d\n"
		"Modem dci diag_notify_update_smd_work: %d\n",
		work_pending(&(driver->diag_drain_work)),
		work_pending(&(driver->smd_data[MODEM_DATA].
							diag_read_smd_work)),
		work_pending(&(driver->smd_data[LPASS_DATA].
							diag_read_smd_work)),
		work_pending(&(driver->smd_data[WCNSS_DATA].
							diag_read_smd_work)),
		work_pending(&(driver->smd_cntl[MODEM_DATA].
							diag_read_smd_work)),
		work_pending(&(driver->smd_cntl[LPASS_DATA].
							diag_read_smd_work)),
		work_pending(&(driver->smd_cntl[WCNSS_DATA].
							diag_read_smd_work)),
		work_pending(&(driver->smd_dci[MODEM_DATA].
							diag_read_smd_work)),
		work_pending(&(driver->smd_data[MODEM_DATA].
						diag_notify_update_smd_work)),
		work_pending(&(driver->smd_data[LPASS_DATA].
						diag_notify_update_smd_work)),
		work_pending(&(driver->smd_data[WCNSS_DATA].
						diag_notify_update_smd_work)),
		work_pending(&(driver->smd_cntl[MODEM_DATA].
						diag_notify_update_smd_work)),
		work_pending(&(driver->smd_cntl[LPASS_DATA].
						diag_notify_update_smd_work)),
		work_pending(&(driver->smd_cntl[WCNSS_DATA].
						diag_notify_update_smd_work)),
		work_pending(&(driver->smd_dci[MODEM_DATA].
						diag_notify_update_smd_work)));

#ifdef CONFIG_DIAG_OVER_USB
	ret += scnprintf(buf+ret, buf_size-ret,
		"diag_proc_hdlc_work: %d\n"
		"diag_read_work: %d\n",
		work_pending(&(driver->diag_proc_hdlc_work)),
		work_pending(&(driver->diag_read_work)));
#endif
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);

	kfree(buf);
	return ret;
}

static ssize_t diag_dbgfs_read_table(struct file *file, char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	char *buf;
	int ret = 0;
	int i;
	unsigned int bytes_remaining;
	unsigned int bytes_in_buffer = 0;
	unsigned int bytes_written;
	unsigned int buf_size;
	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;

	if (diag_dbgfs_table_index >= diag_max_reg) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_table_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}
	buf_size = ksize(buf);
	bytes_remaining = buf_size;

	if (diag_dbgfs_table_index == 0) {
		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"Client ids: Modem: %d, LPASS: %d, "
			"WCNSS: %d, APPS: %d\n",
			MODEM_DATA, LPASS_DATA, WCNSS_DATA, APPS_DATA);
		bytes_in_buffer += bytes_written;
		bytes_remaining -= bytes_written;
	}

	for (i = diag_dbgfs_table_index; i < diag_max_reg; i++) {
		/* Do not process empty entries in the table */
		if (driver->table[i].process_id == 0)
			continue;

		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"i: %3d, cmd_code: %4x, subsys_id: %4x, "
			"client: %2d, cmd_code_lo: %4x, "
			"cmd_code_hi: %4x, process_id: %5d %s\n",
			i,
			driver->table[i].cmd_code,
			driver->table[i].subsys_id,
			driver->table[i].client_id,
			driver->table[i].cmd_code_lo,
			driver->table[i].cmd_code_hi,
			driver->table[i].process_id,
			(diag_find_polling_reg(i) ? "<- Polling cmd reg" : ""));

		bytes_in_buffer += bytes_written;

		/* Check if there is room to add another table entry */
		bytes_remaining = buf_size - bytes_in_buffer;

		if (bytes_remaining < bytes_written)
			break;
	}
	diag_dbgfs_table_index = i+1;

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

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
static ssize_t diag_dbgfs_read_bridge(struct file *file, char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	char *buf;
	int ret;
	int i;
	unsigned int bytes_remaining;
	unsigned int bytes_in_buffer = 0;
	unsigned int bytes_written;
	unsigned int buf_size;
	int bytes_hsic_inited = 45;
	int bytes_hsic_not_inited = 410;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;

	if (diag_dbgfs_finished) {
		diag_dbgfs_finished = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;

	/* Only one smux for now */
	bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
		"Values for SMUX instance: 0\n"
		"smux ch: %d\n"
		"smux read_buf: %p\n"
		"smux read_len: %d\n"
		"smux enabled %d\n"
		"smux in busy %d\n"
		"smux connected %d\n\n",
		diag_smux->lcid,
		diag_smux->read_buf,
		diag_smux->read_len,
		diag_smux->enabled,
		diag_smux->in_busy,
		diag_smux->connected);

	bytes_in_buffer += bytes_written;
	bytes_remaining = buf_size - bytes_in_buffer;

	bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
		"HSIC diag_disconnect_work: %d\n",
		work_pending(&(driver->diag_disconnect_work)));

	bytes_in_buffer += bytes_written;
	bytes_remaining = buf_size - bytes_in_buffer;

	for (i = 0; i < MAX_HSIC_DATA_CH; i++) {
		if (diag_hsic[i].hsic_inited) {
			/* Check if there is room to add another HSIC entry */
			if (bytes_remaining < bytes_hsic_inited)
				break;
			bytes_written = scnprintf(buf+bytes_in_buffer,
							bytes_remaining,
			"Values for HSIC Instance: %d\n"
			"hsic ch: %d\n"
			"hsic_inited: %d\n"
			"hsic enabled: %d\n"
			"hsic_opened: %d\n"
			"hsic_suspend: %d\n"
			"in_busy_hsic_read_on_device: %d\n"
			"in_busy_hsic_write: %d\n"
			"HSIC write_len: %d\n"
			"num_hsic_buf_tbl_entries: %d\n"
			"HSIC usb_connected: %d\n"
			"HSIC diag_read_work: %d\n"
			"diag_read_hsic_work: %d\n"
			"diag_usb_read_complete_work: %d\n\n",
			i,
			diag_hsic[i].hsic_ch,
			diag_hsic[i].hsic_inited,
			diag_hsic[i].hsic_device_enabled,
			diag_hsic[i].hsic_device_opened,
			diag_hsic[i].hsic_suspend,
			diag_hsic[i].in_busy_hsic_read_on_device,
			diag_hsic[i].in_busy_hsic_write,
			diag_bridge[i].write_len,
			diag_hsic[i].num_hsic_buf_tbl_entries,
			diag_bridge[i].usb_connected,
			work_pending(&(diag_bridge[i].diag_read_work)),
			work_pending(&(diag_hsic[i].diag_read_hsic_work)),
			work_pending(&(diag_bridge[i].usb_read_complete_work)));
			if (bytes_written > bytes_hsic_inited)
				bytes_hsic_inited = bytes_written;
		} else {
			/* Check if there is room to add another HSIC entry */
			if (bytes_remaining < bytes_hsic_not_inited)
				break;
			bytes_written = scnprintf(buf+bytes_in_buffer,
				bytes_remaining,
				"HSIC Instance: %d has not been initialized\n\n",
				i);
			if (bytes_written > bytes_hsic_not_inited)
				bytes_hsic_not_inited = bytes_written;
		}

		bytes_in_buffer += bytes_written;

		bytes_remaining = buf_size - bytes_in_buffer;
	}

	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	diag_dbgfs_finished = 1;
	kfree(buf);
	return ret;
}

static ssize_t diag_dbgfs_read_bridge_dci(struct file *file, char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	char *buf;
	int ret;
	int i;
	unsigned int bytes_remaining;
	unsigned int bytes_in_buffer = 0;
	unsigned int bytes_written;
	unsigned int buf_size;
	int bytes_hsic_inited = 45;
	int bytes_hsic_not_inited = 410;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;

	if (diag_dbgfs_finished) {
		diag_dbgfs_finished = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	buf_size = ksize(buf);
	bytes_remaining = buf_size;

	for (i = 0; i < MAX_HSIC_DCI_CH; i++) {
		if (diag_hsic_dci[i].hsic_inited) {
			/* Check if there is room to add another HSIC entry */
			if (bytes_remaining < bytes_hsic_inited)
				break;
			bytes_written = scnprintf(buf+bytes_in_buffer,
							bytes_remaining,
			"Values for HSIC DCI Instance: %d\n"
			"hsic ch: %d\n"
			"hsic_inited: %d\n"
			"hsic enabled: %d\n"
			"hsic_opened: %d\n"
			"hsic_suspend: %d\n"
			"in_busy_hsic_write: %d\n"
			"data: %p\n"
			"data_buf: %p\n"
			"data_len: %d\n"
			"diag_read_hsic_work: %d\n"
			"diag_process_hsic_work: %d\n\n",
			i,
			diag_hsic_dci[i].hsic_ch,
			diag_hsic_dci[i].hsic_inited,
			diag_hsic_dci[i].hsic_device_enabled,
			diag_hsic_dci[i].hsic_device_opened,
			diag_hsic_dci[i].hsic_suspend,
			diag_hsic_dci[i].in_busy_hsic_write,
			diag_hsic_dci[i].data,
			diag_hsic_dci[i].data_buf,
			diag_hsic_dci[i].data_len,
			work_pending(&(diag_hsic_dci[i].diag_read_hsic_work)),
			work_pending(&(diag_hsic_dci[i].
				       diag_process_hsic_work)));
			if (bytes_written > bytes_hsic_inited)
				bytes_hsic_inited = bytes_written;
		} else {
			/* Check if there is room to add another HSIC entry */
			if (bytes_remaining < bytes_hsic_not_inited)
				break;
			bytes_written = scnprintf(buf+bytes_in_buffer,
				bytes_remaining,
				"HSIC DCI Instance: %d has not been initialized\n\n",
				i);
			if (bytes_written > bytes_hsic_not_inited)
				bytes_hsic_not_inited = bytes_written;
		}

		bytes_in_buffer += bytes_written;

		bytes_remaining = buf_size - bytes_in_buffer;
	}

	*ppos = 0;
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, bytes_in_buffer);

	diag_dbgfs_finished = 1;
	kfree(buf);
	return ret;
}

const struct file_operations diag_dbgfs_bridge_ops = {
	.read = diag_dbgfs_read_bridge,
};

const struct file_operations diag_dbgfs_bridge_dci_ops = {
	.read = diag_dbgfs_read_bridge_dci,
};
#endif

const struct file_operations diag_dbgfs_status_ops = {
	.read = diag_dbgfs_read_status,
};

const struct file_operations diag_dbgfs_table_ops = {
	.read = diag_dbgfs_read_table,
};

const struct file_operations diag_dbgfs_workpending_ops = {
	.read = diag_dbgfs_read_workpending,
};

const struct file_operations diag_dbgfs_mempool_ops = {
	.read = diag_dbgfs_read_mempool,
};

const struct file_operations diag_dbgfs_dcistats_ops = {
	.read = diag_dbgfs_read_dcistats,
};

const struct file_operations diag_dbgfs_power_ops = {
	.read = diag_dbgfs_read_power,
};

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

	entry = debugfs_create_file("table", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_table_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("work_pending", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_workpending_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("mempool", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_mempool_ops);
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

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	entry = debugfs_create_file("bridge", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_bridge_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("bridge_dci", 0444, diag_dbgfs_dent, 0,
				    &diag_dbgfs_bridge_dci_ops);
	if (!entry)
		goto err;
#endif

	diag_dbgfs_table_index = 0;
	diag_dbgfs_mempool_index = 0;
	diag_dbgfs_finished = 0;
	diag_dbgfs_dci_data_index = 0;
	diag_dbgfs_dci_finished = 0;

	/* DCI related structures */
	dci_traffic = kzalloc(sizeof(struct diag_dci_data_info) *
				DIAG_DCI_DEBUG_CNT, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(dci_traffic))
		pr_warn("diag: could not allocate memory for dci debug info\n");

	mutex_init(&dci_stat_mutex);
	return 0;
err:
	kfree(dci_traffic);
	debugfs_remove_recursive(diag_dbgfs_dent);
	return -ENOMEM;
}

void diag_debugfs_cleanup(void)
{
	if (diag_dbgfs_dent) {
		debugfs_remove_recursive(diag_dbgfs_dent);
		diag_dbgfs_dent = NULL;
	}

	kfree(dci_traffic);
	mutex_destroy(&dci_stat_mutex);
}
#else
int diag_debugfs_init(void) { return 0; }
void diag_debugfs_cleanup(void) { }
#endif
