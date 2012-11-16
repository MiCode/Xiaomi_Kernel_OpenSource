/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#define DEBUG_BUF_SIZE	4096
static struct dentry *diag_dbgfs_dent;
static int diag_dbgfs_table_index;

static ssize_t diag_dbgfs_read_status(struct file *file, char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	char *buf;
	int ret;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	ret = scnprintf(buf, DEBUG_BUF_SIZE,
		"modem ch: 0x%x\n"
		"lpass ch: 0x%x\n"
		"riva ch: 0x%x\n"
		"dci ch: 0x%x\n"
		"modem cntl_ch: 0x%x\n"
		"lpass cntl_ch: 0x%x\n"
		"riva cntl_ch: 0x%x\n"
		"CPU Tools id: %d\n"
		"Apps only: %d\n"
		"Apps master: %d\n"
		"Check Polling Response: %d\n"
		"polling_reg_flag: %d\n"
		"uses device tree: %d\n"
		"in_busy_1: %d\n"
		"in_busy_2: %d\n"
		"in_busy_lpass_1: %d\n"
		"in_busy_lpass_2: %d\n"
		"in_busy_wcnss_1: %d\n"
		"in_busy_wcnss_2: %d\n"
		"in_busy_dci: %d\n"
		"logging_mode: %d\n",
		(unsigned int)driver->ch,
		(unsigned int)driver->chlpass,
		(unsigned int)driver->ch_wcnss,
		(unsigned int)driver->ch_dci,
		(unsigned int)driver->ch_cntl,
		(unsigned int)driver->chlpass_cntl,
		(unsigned int)driver->ch_wcnss_cntl,
		chk_config_get_id(),
		chk_apps_only(),
		chk_apps_master(),
		chk_polling_response(),
		driver->polling_reg_flag,
		driver->use_device_tree,
		driver->in_busy_1,
		driver->in_busy_2,
		driver->in_busy_lpass_1,
		driver->in_busy_lpass_2,
		driver->in_busy_wcnss_1,
		driver->in_busy_wcnss_2,
		driver->in_busy_dci,
		driver->logging_mode);

#ifdef CONFIG_DIAG_OVER_USB
	ret += scnprintf(buf+ret, DEBUG_BUF_SIZE,
		"usb_connected: %d\n",
		driver->usb_connected);
#endif
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);

	kfree(buf);
	return ret;
}

static ssize_t diag_dbgfs_read_workpending(struct file *file,
				char __user *ubuf, size_t count, loff_t *ppos)
{
	char *buf;
	int ret;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	ret = scnprintf(buf, DEBUG_BUF_SIZE,
		"Pending status for work_stucts:\n"
		"diag_drain_work: %d\n"
		"diag_read_smd_work: %d\n"
		"diag_read_smd_cntl_work: %d\n"
		"diag_read_smd_lpass_work: %d\n"
		"diag_read_smd_lpass_cntl_work: %d\n"
		"diag_read_smd_wcnss_work: %d\n"
		"diag_read_smd_wcnss_cntl_work: %d\n"
		"diag_modem_mask_update_work: %d\n"
		"diag_lpass_mask_update_work: %d\n"
		"diag_wcnss_mask_update_work: %d\n"
		"diag_read_smd_dci_work: %d\n"
		"diag_update_smd_dci_work: %d\n",
		work_pending(&(driver->diag_drain_work)),
		work_pending(&(driver->diag_read_smd_work)),
		work_pending(&(driver->diag_read_smd_cntl_work)),
		work_pending(&(driver->diag_read_smd_lpass_work)),
		work_pending(&(driver->diag_read_smd_lpass_cntl_work)),
		work_pending(&(driver->diag_read_smd_wcnss_work)),
		work_pending(&(driver->diag_read_smd_wcnss_cntl_work)),
		work_pending(&(driver->diag_modem_mask_update_work)),
		work_pending(&(driver->diag_lpass_mask_update_work)),
		work_pending(&(driver->diag_wcnss_mask_update_work)),
		work_pending(&(driver->diag_read_smd_dci_work)),
		work_pending(&(driver->diag_update_smd_dci_work)));

#ifdef CONFIG_DIAG_OVER_USB
	ret += scnprintf(buf+ret, DEBUG_BUF_SIZE,
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
	int bytes_remaining;
	int bytes_in_buffer = 0;
	int bytes_written;
	int buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;

	if (diag_dbgfs_table_index >= diag_max_reg) {
		/* Done. Reset to prepare for future requests */
		diag_dbgfs_table_index = 0;
		return 0;
	}

	buf = kzalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (!buf) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	bytes_remaining = buf_size;
	for (i = diag_dbgfs_table_index; i < diag_max_reg; i++) {
		/* Do not process empty entries in the table */
		if (driver->table[i].process_id == 0)
			continue;

		bytes_written = scnprintf(buf+bytes_in_buffer, bytes_remaining,
			"i: %3d, cmd_code: %4x, subsys_id: %4x, "
			"client: %2d, cmd_code_lo: %4x, "
			"cmd_code_hi: %4x, process_id: %5d\n",
			i,
			driver->table[i].cmd_code,
			driver->table[i].subsys_id,
			driver->table[i].client_id,
			driver->table[i].cmd_code_lo,
			driver->table[i].cmd_code_hi,
			driver->table[i].process_id);

		bytes_in_buffer += bytes_written;

		/* Check if there is room to add another table entry */
		bytes_remaining = buf_size - bytes_in_buffer;
		if (bytes_remaining < bytes_written)
			break;
	}
	diag_dbgfs_table_index = i;

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

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		pr_err("diag: %s, Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	ret = scnprintf(buf, DEBUG_BUF_SIZE,
		"hsic ch: %d\n"
		"hsic_inited: %d\n"
		"hsic enabled: %d\n"
		"hsic_opened: %d\n"
		"hsic_suspend: %d\n"
		"in_busy_hsic_read_on_device: %d\n"
		"in_busy_hsic_write: %d\n"
		"count_hsic_pool: %d\n"
		"count_hsic_write_pool: %d\n"
		"diag_hsic_pool: %x\n"
		"diag_hsic_write_pool: %x\n"
		"HSIC write_len: %d\n"
		"num_hsic_buf_tbl_entries: %d\n"
		"HSIC usb_connected: %d\n"
		"HSIC diag_read_work: %d\n"
		"diag_read_hsic_work: %d\n"
		"diag_disconnect_work: %d\n"
		"diag_usb_read_complete_work: %d\n"
		"smux ch: %d"
		"smux enabled %d"
		"smux in busy %d"
		"smux connected %d",
		driver->hsic_ch,
		driver->hsic_inited,
		driver->hsic_device_enabled,
		driver->hsic_device_opened,
		driver->hsic_suspend,
		driver->in_busy_hsic_read_on_device,
		driver->in_busy_hsic_write,
		driver->count_hsic_pool,
		driver->count_hsic_write_pool,
		(unsigned int)driver->diag_hsic_pool,
		(unsigned int)driver->diag_hsic_write_pool,
			diag_bridge[HSIC].write_len,
		driver->num_hsic_buf_tbl_entries,
			diag_bridge[HSIC].usb_connected,
			work_pending(&(diag_bridge[HSIC].diag_read_work)),
		work_pending(&(driver->diag_read_hsic_work)),
		work_pending(&(driver->diag_disconnect_work)),
		work_pending(&(diag_bridge[HSIC].usb_read_complete_work)),
		driver->lcid,
		driver->diag_smux_enabled,
		driver->in_busy_smux,
		driver->smux_connected);

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);

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

const struct file_operations diag_dbgfs_table_ops = {
	.read = diag_dbgfs_read_table,
};

const struct file_operations diag_dbgfs_workpending_ops = {
	.read = diag_dbgfs_read_workpending,
};

void diag_debugfs_init(void)
{
	diag_dbgfs_dent = debugfs_create_dir("diag", 0);
	if (IS_ERR(diag_dbgfs_dent))
		return;

	debugfs_create_file("status", 0444, diag_dbgfs_dent, 0,
		&diag_dbgfs_status_ops);

	debugfs_create_file("table", 0444, diag_dbgfs_dent, 0,
		&diag_dbgfs_table_ops);

	debugfs_create_file("work_pending", 0444, diag_dbgfs_dent, 0,
		&diag_dbgfs_workpending_ops);

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	debugfs_create_file("bridge", 0444, diag_dbgfs_dent, 0,
		&diag_dbgfs_bridge_ops);
#endif

	diag_dbgfs_table_index = 0;
}

void diag_debugfs_cleanup(void)
{
	if (diag_dbgfs_dent) {
		debugfs_remove_recursive(diag_dbgfs_dent);
		diag_dbgfs_dent = NULL;
	}
}
#else
void diag_debugfs_init(void) { }
void diag_debugfs_cleanup(void) { }
#endif
