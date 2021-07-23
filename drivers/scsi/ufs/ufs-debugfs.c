// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * UFS debugfs - add debugfs interface to the ufshcd.
 * This is currently used for statistics collection and exporting from the
 * UFS driver.
 * This infrastructure can be used for debugging or direct tweaking
 * of the driver from userspace.
 *
 */

#include <linux/random.h>
#include "ufs-debugfs.h"
#include "unipro.h"
#include "ufshci.h"
#include "ufshcd.h"

enum field_width {
	BYTE	= 1,
	WORD	= 2,
	DWORD	= 4,
};

struct desc_field_offset {
	char *name;
	int offset;
	enum field_width width_byte;
};

#define UFS_ERR_STATS_PRINT(file, error_index, string, error_seen)	\
	do {								\
		if (err_stats[error_index]) {				\
			seq_printf(file, string,			\
					err_stats[error_index]);	\
			error_seen = true;				\
		}							\
	} while (0)

#define BUFF_LINE_SIZE 16 /* Must be a multiplication of sizeof(u32) */
#define TAB_CHARS 8

static int ufsdbg_tag_stats_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;
	struct ufs_stats *ufs_stats;
	int i, j;
	int max_depth;
	bool is_tag_empty = true;
	unsigned long flags;
	char *sep = " | * | ";

	if (!hba)
		goto exit;

	ufs_stats = &hba->ufs_stats;

	if (!ufs_stats->enabled) {
		pr_debug("%s: ufs statistics are disabled\n", __func__);
		seq_puts(file, "ufs statistics are disabled");
		goto exit;
	}

	max_depth = hba->nutrs;

	spin_lock_irqsave(hba->host->host_lock, flags);
	/* Header */
	seq_printf(file, " Tag Stat\t\t%s Number of pending reqs upon issue (Q fullness)\n",
		sep);
	for (i = 0; i < TAB_CHARS * (TS_NUM_STATS + 4); i++) {
		seq_puts(file, "-");
		if (i == (TAB_CHARS * 3 - 1))
			seq_puts(file, sep);
	}
	seq_printf(file,
		"\n #\tnum uses\t%s\t #\tAll\tRead\tWrite\tUrg.R\tUrg.W\tFlush\n",
		sep);

	/* values */
	for (i = 0; i < max_depth; i++) {
		if (ufs_stats->tag_stats[i][TS_TAG] <= 0 &&
				ufs_stats->tag_stats[i][TS_READ] <= 0 &&
				ufs_stats->tag_stats[i][TS_WRITE] <= 0 &&
				ufs_stats->tag_stats[i][TS_URGENT_READ] <= 0 &&
				ufs_stats->tag_stats[i][TS_URGENT_WRITE] <= 0 &&
				ufs_stats->tag_stats[i][TS_FLUSH] <= 0)
			continue;

		is_tag_empty = false;
		seq_printf(file, " %d\t ", i);
		for (j = 0; j < TS_NUM_STATS; j++) {
			seq_printf(file, "%llu\t", ufs_stats->tag_stats[i][j]);
			if (j != 0)
				continue;
			seq_printf(file, "\t%s\t %d\t%llu\t", sep, i,
				ufs_stats->tag_stats[i][TS_READ] +
				ufs_stats->tag_stats[i][TS_WRITE] +
				ufs_stats->tag_stats[i][TS_URGENT_READ] +
				ufs_stats->tag_stats[i][TS_URGENT_WRITE] +
				ufs_stats->tag_stats[i][TS_FLUSH]);
		}
		seq_puts(file, "\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (is_tag_empty)
		pr_debug("%s: All tags statistics are empty\n", __func__);

exit:
	return 0;
}

static int ufsdbg_tag_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_tag_stats_show, inode->i_private);
}

static ssize_t ufsdbg_tag_stats_write(struct file *filp,
				      const char __user *ubuf, size_t cnt,
				       loff_t *ppos)
{
	struct ufs_hba *hba = filp->f_mapping->host->i_private;
	struct ufs_stats *ufs_stats;
	int val = 0;
	int ret, bit = 0;
	unsigned long flags;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret) {
		dev_err(hba->dev, "%s: Invalid argument\n", __func__);
		return ret;
	}

	ufs_stats = &hba->ufs_stats;
	spin_lock_irqsave(hba->host->host_lock, flags);

	if (!val) {
		ufs_stats->enabled = false;
		pr_debug("%s: Disabling UFS tag statistics\n", __func__);
	} else {
		ufs_stats->enabled = true;
		pr_debug("%s: Enabling & Resetting UFS tag statistics\n",
			 __func__);
		memset(hba->ufs_stats.tag_stats[0], 0,
			sizeof(**hba->ufs_stats.tag_stats) *
			TS_NUM_STATS * hba->nutrs);

		/* initialize current queue depth */
		ufs_stats->q_depth = 0;
		for_each_set_bit_from(bit, &hba->outstanding_reqs, hba->nutrs)
			ufs_stats->q_depth++;
		pr_debug("%s: Enabled UFS tag statistics\n", __func__);
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return cnt;
}

static const struct file_operations ufsdbg_tag_stats_fops = {
	.open		= ufsdbg_tag_stats_open,
	.read		= seq_read,
	.write		= ufsdbg_tag_stats_write,
};

static int ufsdbg_query_stats_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;
	struct ufs_stats *ufs_stats = &hba->ufs_stats;
	int i, j;
	static const char *opcode_name[UPIU_QUERY_OPCODE_MAX] = {
		"QUERY_OPCODE_NOP:",
		"QUERY_OPCODE_READ_DESC:",
		"QUERY_OPCODE_WRITE_DESC:",
		"QUERY_OPCODE_READ_ATTR:",
		"QUERY_OPCODE_WRITE_ATTR:",
		"QUERY_OPCODE_READ_FLAG:",
		"QUERY_OPCODE_SET_FLAG:",
		"QUERY_OPCODE_CLEAR_FLAG:",
		"QUERY_OPCODE_TOGGLE_FLAG:",
	};

	seq_puts(file, "\n");
	seq_puts(file, "The following table shows how many TIMES each IDN was sent to device for each QUERY OPCODE:\n");
	seq_puts(file, "\n");

	for (i = 0; i < UPIU_QUERY_OPCODE_MAX; i++) {
		seq_printf(file, "%-30s", opcode_name[i]);

		for (j = 0; j < MAX_QUERY_IDN; j++) {
			/*
			 * we would like to print only the non-zero data,
			 * (non-zero number of times that IDN was sent
			 * to the device per opcode). There is no
			 * importance to the "table structure" of the output.
			 */
			if (ufs_stats->query_stats_arr[i][j])
				seq_printf(file, "IDN 0x%02X: %d,\t", j,
					   ufs_stats->query_stats_arr[i][j]);
		}
		seq_puts(file, "\n");
	}

	return 0;
}

static int ufsdbg_query_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_query_stats_show, inode->i_private);
}

static ssize_t ufsdbg_query_stats_write(struct file *filp,
				      const char __user *ubuf, size_t cnt,
				       loff_t *ppos)
{
	struct ufs_hba *hba = filp->f_mapping->host->i_private;
	struct ufs_stats *ufs_stats = &hba->ufs_stats;
	int i, j;

	mutex_lock(&hba->dev_cmd.lock);

	for (i = 0; i < UPIU_QUERY_OPCODE_MAX; i++)
		for (j = 0; j < MAX_QUERY_IDN; j++)
			ufs_stats->query_stats_arr[i][j] = 0;

	mutex_unlock(&hba->dev_cmd.lock);

	return cnt;
}

static const struct file_operations ufsdbg_query_stats_fops = {
	.open		= ufsdbg_query_stats_open,
	.read		= seq_read,
	.write		= ufsdbg_query_stats_write,
};

static int ufsdbg_err_stats_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;
	int *err_stats;
	unsigned long flags;
	bool error_seen = false;

	if (!hba)
		goto exit;

	err_stats = hba->ufs_stats.err_stats;

	spin_lock_irqsave(hba->host->host_lock, flags);

	seq_puts(file, "\n==UFS errors that caused controller reset==\n");

	UFS_ERR_STATS_PRINT(file, UFS_ERR_HIBERN8_EXIT,
			"controller reset due to hibern8 exit error:\t %d\n",
			error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_VOPS_SUSPEND,
			"controller reset due to vops suspend error:\t\t %d\n",
			error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_EH,
			"controller reset due to error handling:\t\t %d\n",
			error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_CLEAR_PEND_XFER_TM,
			"controller reset due to clear xfer/tm regs:\t\t %d\n",
			error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_INT_FATAL_ERRORS,
			"controller reset due to fatal interrupt:\t %d\n",
			error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_INT_UIC_ERROR,
			"controller reset due to uic interrupt error:\t %d\n",
			error_seen);

	if (error_seen)
		error_seen = false;
	else
		seq_puts(file,
			"so far, no errors that caused controller reset\n\n");

	seq_puts(file, "\n\n==UFS other errors==\n");

	UFS_ERR_STATS_PRINT(file, UFS_ERR_HIBERN8_ENTER,
			"hibern8 enter:\t\t %d\n", error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_RESUME,
			"resume error:\t\t %d\n", error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_SUSPEND,
			"suspend error:\t\t %d\n", error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_LINKSTARTUP,
			"linkstartup error:\t\t %d\n", error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_POWER_MODE_CHANGE,
			"power change error:\t %d\n", error_seen);

	UFS_ERR_STATS_PRINT(file, UFS_ERR_TASK_ABORT,
			"abort callback:\t\t %d\n\n", error_seen);

	if (!error_seen)
		seq_puts(file,
		"so far, no other UFS related errors\n\n");

	spin_unlock_irqrestore(hba->host->host_lock, flags);
exit:
	return 0;
}

static int ufsdbg_err_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_err_stats_show, inode->i_private);
}

static ssize_t ufsdbg_err_stats_write(struct file *filp,
				      const char __user *ubuf, size_t cnt,
				       loff_t *ppos)
{
	struct ufs_hba *hba = filp->f_mapping->host->i_private;
	struct ufs_stats *ufs_stats;
	unsigned long flags;

	ufs_stats = &hba->ufs_stats;
	spin_lock_irqsave(hba->host->host_lock, flags);

	pr_debug("%s: Resetting UFS error statistics\n", __func__);
	memset(ufs_stats->err_stats, 0, sizeof(hba->ufs_stats.err_stats));

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return cnt;
}

static const struct file_operations ufsdbg_err_stats_fops = {
	.open		= ufsdbg_err_stats_open,
	.read		= seq_read,
	.write		= ufsdbg_err_stats_write,
};

static int ufshcd_init_statistics(struct ufs_hba *hba)
{
	struct ufs_stats *stats = &hba->ufs_stats;
	int ret = 0;
	int i;

	stats->enabled = false;
	stats->tag_stats = kcalloc(hba->nutrs, sizeof(*stats->tag_stats),
			GFP_KERNEL);
	if (!hba->ufs_stats.tag_stats)
		goto no_mem;

	stats->tag_stats[0] = kzalloc(sizeof(**stats->tag_stats) *
			TS_NUM_STATS * hba->nutrs, GFP_KERNEL);
	if (!stats->tag_stats[0])
		goto no_mem;

	for (i = 1; i < hba->nutrs; i++)
		stats->tag_stats[i] = &stats->tag_stats[0][i * TS_NUM_STATS];

	memset(stats->err_stats, 0, sizeof(hba->ufs_stats.err_stats));

	goto exit;

no_mem:
	dev_err(hba->dev, "%s: Unable to allocate UFS tag_stats\n", __func__);
	ret = -ENOMEM;
exit:
	return ret;
}

void ufsdbg_pr_buf_to_std(struct ufs_hba *hba, int offset, int num_regs,
				char *str, void *priv)
{
	int i;
	char linebuf[38];
	int size = num_regs * sizeof(u32);
	int lines = size / BUFF_LINE_SIZE +
			(size % BUFF_LINE_SIZE ? 1 : 0);
	struct seq_file *file = priv;

	if (!hba || !file) {
		pr_err("%s called with NULL pointer\n", __func__);
		return;
	}

	for (i = 0; i < lines; i++) {
		hex_dump_to_buffer(hba->mmio_base + offset + i * BUFF_LINE_SIZE,
				min(BUFF_LINE_SIZE, size), BUFF_LINE_SIZE, 4,
				linebuf, sizeof(linebuf), false);
		seq_printf(file, "%s [%x]: %s\n", str, i * BUFF_LINE_SIZE,
				linebuf);
		size -= BUFF_LINE_SIZE/sizeof(u32);
	}
}

static int ufsdbg_host_regs_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold(hba, false);
	ufsdbg_pr_buf_to_std(hba, 0, UFSHCI_REG_SPACE_SIZE / sizeof(u32),
				"host regs", file);
	ufshcd_release(hba);
	pm_runtime_put_sync(hba->dev);
	return 0;
}

static int ufsdbg_host_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_host_regs_show, inode->i_private);
}

static const struct file_operations ufsdbg_host_regs_fops = {
	.open		= ufsdbg_host_regs_open,
	.read		= seq_read,
};

static int ufsdbg_dump_health_desc_show(struct seq_file *file, void *data)
{
	int err = 0;
	size_t buff_len;
	u8 *desc_buf;
	struct ufs_hba *hba = (struct ufs_hba *)file->private;

	struct desc_field_offset health_desc_field_name[] = {
		{"bLength",		0x00, BYTE},
		{"bDescriptorType",	0x01, BYTE},
		{"bPreEOLInfo",		0x02, BYTE},
		{"bDeviceLifeTimeEstA",	0x03, BYTE},
		{"bDeviceLifeTimeEstB",	0x04, BYTE},
	};

	buff_len = max_t(size_t, hba->desc_size.dev_heal_desc,
			 QUERY_DESC_HEALTH_MAX_SIZE + 1);
	if (hba->desc_size.dev_heal_desc > (QUERY_DESC_HEALTH_MAX_SIZE + 1))
		dev_info(hba->dev, "%s: unexpected dev_heal_desc size: %d\n",
			__func__, hba->desc_size.dev_heal_desc);
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		err = -ENOMEM;
		goto out;
	}

	pm_runtime_get_sync(hba->dev);
	err = ufshcd_read_health_desc(hba, desc_buf, buff_len);
	pm_runtime_put_sync(hba->dev);


	if (!err) {
		int i;
		struct desc_field_offset *tmp;

		for (i = 0; i < ARRAY_SIZE(health_desc_field_name); ++i) {
			tmp = &health_desc_field_name[i];

			if (tmp->width_byte == BYTE) {
				seq_printf(file,
					   "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
					   tmp->offset,
					   tmp->name,
					   (u8)desc_buf[tmp->offset]);
			} else if (tmp->width_byte == WORD) {
				seq_printf(file,
					   "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
					   tmp->offset,
					   tmp->name,
					   *(u16 *)&desc_buf[tmp->offset]);
			} else {
				seq_printf(file,
					   "Device Descriptor[offset 0x%x]: %s. Wrong Width = %d",
					   tmp->offset, tmp->name, tmp->width_byte);
			}
		}
	} else {
		seq_printf(file, "Reading Device Descriptor failed. err = %d\n",
			   err);
	}

	if (desc_buf)
		kfree(desc_buf);
out:
	return err;
}


static int ufsdbg_dump_device_desc_show(struct seq_file *file, void *data)
{
	int err = 0;
	size_t buff_len;
	u8 *desc_buf;
	struct ufs_hba *hba = (struct ufs_hba *)file->private;

	struct desc_field_offset device_desc_field_name[] = {
		{"bLength",		0x00, BYTE},
		{"bDescriptorType",	0x01, BYTE},
		{"bDevice",		0x02, BYTE},
		{"bDeviceClass",	0x03, BYTE},
		{"bDeviceSubClass",	0x04, BYTE},
		{"bProtocol",		0x05, BYTE},
		{"bNumberLU",		0x06, BYTE},
		{"bNumberWLU",		0x07, BYTE},
		{"bBootEnable",		0x08, BYTE},
		{"bDescrAccessEn",	0x09, BYTE},
		{"bInitPowerMode",	0x0A, BYTE},
		{"bHighPriorityLUN",	0x0B, BYTE},
		{"bSecureRemovalType",	0x0C, BYTE},
		{"bSecurityLU",		0x0D, BYTE},
		{"Reserved",		0x0E, BYTE},
		{"bInitActiveICCLevel",	0x0F, BYTE},
		{"wSpecVersion",	0x10, WORD},
		{"wManufactureDate",	0x12, WORD},
		{"iManufactureName",	0x14, BYTE},
		{"iProductName",	0x15, BYTE},
		{"iSerialNumber",	0x16, BYTE},
		{"iOemID",		0x17, BYTE},
		{"wManufactureID",	0x18, WORD},
		{"bUD0BaseOffset",	0x1A, BYTE},
		{"bUDConfigPLength",	0x1B, BYTE},
		{"bDeviceRTTCap",	0x1C, BYTE},
		{"wPeriodicRTCUpdate",	0x1D, WORD},
		{"bUFSFeaturesSupport", 0x1F, BYTE},
		{"bFFUTimeout", 0x20, BYTE},
		{"bQueueDepth", 0x21, BYTE},
		{"wDeviceVersion", 0x22, WORD},
		{"bNumSecureWpArea", 0x24, BYTE},
		{"dPSAMaxDataSize", 0x25, DWORD},
		{"bPSAStateTimeout", 0x29, BYTE},
		{"iProductRevisionLevel", 0x2A, BYTE},
	};

	buff_len = max_t(size_t, hba->desc_size.dev_desc,
			 QUERY_DESC_DEVICE_DEF_SIZE + 1);
	if (hba->desc_size.dev_heal_desc > (QUERY_DESC_DEVICE_DEF_SIZE + 1))
		dev_info(hba->dev, "%s: unexpected dev_desc size: %d\n",
			__func__, hba->desc_size.dev_desc);
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		err = -ENOMEM;
		goto out;
	}

	pm_runtime_get_sync(hba->dev);
	err = ufshcd_read_device_desc(hba, desc_buf, buff_len);
	pm_runtime_put_sync(hba->dev);

	if (!err) {
		int i;
		struct desc_field_offset *tmp;

		for (i = 0; i < ARRAY_SIZE(device_desc_field_name); ++i) {
			tmp = &device_desc_field_name[i];

			if (tmp->width_byte == BYTE) {
				seq_printf(file,
					   "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
					   tmp->offset,
					   tmp->name,
					   (u8)desc_buf[tmp->offset]);
			} else if (tmp->width_byte == WORD) {
				seq_printf(file,
					   "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
					   tmp->offset,
					   tmp->name,
					   *(u16 *)&desc_buf[tmp->offset]);
			} else if (tmp->width_byte == DWORD) {
				seq_printf(file,
					   "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
					   tmp->offset,
					   tmp->name,
					   *(u32 *)&desc_buf[tmp->offset]);
			} else {
				seq_printf(file,
					   "Device Descriptor[offset 0x%x]: %s. Wrong Width = %d",
					   tmp->offset, tmp->name, tmp->width_byte);
			}
		}
	} else {
		seq_printf(file, "Reading Device Descriptor failed. err = %d\n",
			   err);
	}

	if (desc_buf)
		kfree(desc_buf);
out:
	return err;
}

static int ufsdbg_string_desc_serial_show(struct seq_file *file, void *data)
{
	int err = 0;
	char *serial = ufs_get_serial();

	seq_printf(file, "serial:%s\n", serial);
	return err;
}

static int ufsdbg_show_hba_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;

	seq_printf(file, "hba->outstanding_tasks = 0x%x\n",
			(u32)hba->outstanding_tasks);
	seq_printf(file, "hba->outstanding_reqs = 0x%x\n",
			(u32)hba->outstanding_reqs);

	seq_printf(file, "hba->capabilities = 0x%x\n", hba->capabilities);
	seq_printf(file, "hba->nutrs = %d\n", hba->nutrs);
	seq_printf(file, "hba->nutmrs = %d\n", hba->nutmrs);
	seq_printf(file, "hba->ufs_version = 0x%x\n", hba->ufs_version);
	seq_printf(file, "hba->irq = 0x%x\n", hba->irq);
	seq_printf(file, "hba->auto_bkops_enabled = %d\n",
			hba->auto_bkops_enabled);

	seq_printf(file, "hba->ufshcd_state = 0x%x\n", hba->ufshcd_state);
	seq_printf(file, "hba->clk_gating.state = 0x%x\n",
			hba->clk_gating.state);
	seq_printf(file, "hba->eh_flags = 0x%x\n", hba->eh_flags);
	seq_printf(file, "hba->intr_mask = 0x%x\n", hba->intr_mask);
	seq_printf(file, "hba->ee_ctrl_mask = 0x%x\n", hba->ee_ctrl_mask);

	/* HBA Errors */
	seq_printf(file, "hba->errors = 0x%x\n", hba->errors);
	seq_printf(file, "hba->uic_error = 0x%x\n", hba->uic_error);
	seq_printf(file, "hba->saved_err = 0x%x\n", hba->saved_err);
	seq_printf(file, "hba->saved_uic_err = 0x%x\n", hba->saved_uic_err);

	seq_printf(file, "power_mode_change_cnt = %d\n",
			hba->ufs_stats.power_mode_change_cnt);
	seq_printf(file, "hibern8_exit_cnt = %d\n",
			hba->ufs_stats.hibern8_exit_cnt);

	seq_printf(file, "pa_err_cnt_total = %d\n",
			hba->ufs_stats.pa_err_cnt_total);
	seq_printf(file, "pa_lane_0_err_cnt = %d\n",
			hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_0]);
	seq_printf(file, "pa_lane_1_err_cnt = %d\n",
			hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_1]);
	seq_printf(file, "pa_line_reset_err_cnt = %d\n",
			hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LINE_RESET]);
	seq_printf(file, "dl_err_cnt_total = %d\n",
			hba->ufs_stats.dl_err_cnt_total);
	seq_printf(file, "dl_nac_received_err_cnt = %d\n",
			hba->ufs_stats.dl_err_cnt[UFS_EC_DL_NAC_RECEIVED]);
	seq_printf(file, "dl_tcx_replay_timer_expired_err_cnt = %d\n",
	hba->ufs_stats.dl_err_cnt[UFS_EC_DL_TCx_REPLAY_TIMER_EXPIRED]);
	seq_printf(file, "dl_afcx_request_timer_expired_err_cnt = %d\n",
	hba->ufs_stats.dl_err_cnt[UFS_EC_DL_AFCx_REQUEST_TIMER_EXPIRED]);
	seq_printf(file, "dl_fcx_protection_timer_expired_err_cnt = %d\n",
	hba->ufs_stats.dl_err_cnt[UFS_EC_DL_FCx_PROTECT_TIMER_EXPIRED]);
	seq_printf(file, "dl_crc_err_cnt = %d\n",
			hba->ufs_stats.dl_err_cnt[UFS_EC_DL_CRC_ERROR]);
	seq_printf(file, "dll_rx_buffer_overflow_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_RX_BUFFER_OVERFLOW]);
	seq_printf(file, "dl_max_frame_length_exceeded_err_cnt = %d\n",
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_MAX_FRAME_LENGTH_EXCEEDED]);
	seq_printf(file, "dl_wrong_sequence_number_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_WRONG_SEQUENCE_NUMBER]);
	seq_printf(file, "dl_afc_frame_syntax_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_AFC_FRAME_SYNTAX_ERROR]);
	seq_printf(file, "dl_nac_frame_syntax_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_NAC_FRAME_SYNTAX_ERROR]);
	seq_printf(file, "dl_eof_syntax_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_EOF_SYNTAX_ERROR]);
	seq_printf(file, "dl_frame_syntax_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_FRAME_SYNTAX_ERROR]);
	seq_printf(file, "dl_bad_ctrl_symbol_type_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_BAD_CTRL_SYMBOL_TYPE]);
	seq_printf(file, "dl_pa_init_err_cnt = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_PA_INIT_ERROR]);
	seq_printf(file, "dl_pa_error_ind_received = %d\n",
		   hba->ufs_stats.dl_err_cnt[UFS_EC_DL_PA_ERROR_IND_RECEIVED]);
	seq_printf(file, "dme_err_cnt = %d\n", hba->ufs_stats.dme_err_cnt);

	return 0;
}

static int ufsdbg_show_hba_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_show_hba_show, inode->i_private);
}

static const struct file_operations ufsdbg_show_hba_fops = {
	.open		= ufsdbg_show_hba_open,
	.read		= seq_read,
};

static int ufsdbg_dump_device_desc_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			   ufsdbg_dump_device_desc_show, inode->i_private);
}

static const struct file_operations ufsdbg_dump_device_desc = {
	.open		= ufsdbg_dump_device_desc_open,
	.read		= seq_read,
};

static int ufsdbg_string_desc_serial_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			   ufsdbg_string_desc_serial_show, inode->i_private);
}

static const struct file_operations ufsdbg_dump_string_desc_serial = {
	.open		= ufsdbg_string_desc_serial_open,
	.read		= seq_read,
};


static int ufsdbg_dump_health_desc_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			   ufsdbg_dump_health_desc_show, inode->i_private);
}

static const struct file_operations ufsdbg_dump_health_desc = {
	.open		= ufsdbg_dump_health_desc_open,
	.read		= seq_read,
};

static int ufsdbg_dbg_print_en_read(void *data, u64 *attr_val)
{
	struct ufs_hba *hba = data;

	if (!hba)
		return -EINVAL;

	*attr_val = (u64)hba->ufshcd_dbg_print;
	return 0;
}

static int ufsdbg_dbg_print_en_set(void *data, u64 attr_id)
{
	struct ufs_hba *hba = data;

	if (!hba)
		return -EINVAL;

	if (attr_id & ~UFSHCD_DBG_PRINT_ALL)
		return -EINVAL;

	hba->ufshcd_dbg_print = (u32)attr_id;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(ufsdbg_dbg_print_en_ops,
			ufsdbg_dbg_print_en_read,
			ufsdbg_dbg_print_en_set,
			"%llu\n");

static ssize_t ufsdbg_req_stats_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct ufs_hba *hba = filp->f_mapping->host->i_private;
	int val;
	int ret;
	unsigned long flags;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret) {
		dev_err(hba->dev, "%s: Invalid argument\n", __func__);
		return ret;
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_init_req_stats(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return cnt;
}

static int ufsdbg_req_stats_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;
	int i;
	unsigned long flags;

	/* Header */
	seq_printf(file, "\t%-10s %-10s %-10s %-10s %-10s %-10s",
		"All", "Write", "Read", "Read(urg)", "Write(urg)", "Flush");

	spin_lock_irqsave(hba->host->host_lock, flags);

	seq_printf(file, "\n%s:\t", "Min");
	for (i = 0; i < TS_NUM_STATS; i++)
		seq_printf(file, "%-10llu ", hba->ufs_stats.req_stats[i].min);
	seq_printf(file, "\n%s:\t", "Max");
	for (i = 0; i < TS_NUM_STATS; i++)
		seq_printf(file, "%-10llu ", hba->ufs_stats.req_stats[i].max);
	seq_printf(file, "\n%s:\t", "Avg.");
	for (i = 0; i < TS_NUM_STATS; i++)
		seq_printf(file, "%-10llu ",
			div64_u64(hba->ufs_stats.req_stats[i].sum,
				hba->ufs_stats.req_stats[i].count));
	seq_printf(file, "\n%s:\t", "Count");
	for (i = 0; i < TS_NUM_STATS; i++)
		seq_printf(file, "%-10llu ", hba->ufs_stats.req_stats[i].count);
	seq_puts(file, "\n");
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return 0;
}

static int ufsdbg_req_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_req_stats_show, inode->i_private);
}

static const struct file_operations ufsdbg_req_stats_desc = {
	.open		= ufsdbg_req_stats_open,
	.read		= seq_read,
	.write		= ufsdbg_req_stats_write,
};

static int ufsdbg_clear_err_state(void *data, u64 val)
{
	struct ufs_hba *hba = data;

	if (!hba)
		return -EINVAL;

	/* clear the error state on any write attempt */
	hba->debugfs_files.err_occurred = false;

	return 0;
}

static int ufsdbg_read_err_state(void *data, u64 *val)
{
	struct ufs_hba *hba = data;

	if (!hba)
		return -EINVAL;

	*val = hba->debugfs_files.err_occurred ? 1 : 0;

	return 0;
}

void ufsdbg_set_err_state(struct ufs_hba *hba)
{
	hba->debugfs_files.err_occurred = true;
}

void ufsdbg_clr_err_state(struct ufs_hba *hba)
{
	hba->debugfs_files.err_occurred = false;
}

DEFINE_DEBUGFS_ATTRIBUTE(ufsdbg_err_state,
			ufsdbg_read_err_state,
			ufsdbg_clear_err_state,
			"%llu\n");

void ufsdbg_add_debugfs(struct ufs_hba *hba)
{
	char root_name[sizeof("ufshcd00")];

	if (!hba) {
		pr_err("%s: NULL hba, exiting\n", __func__);
		return;
	}

	snprintf(root_name, ARRAY_SIZE(root_name), "%s%d", UFSHCD, hba->host->host_no);


	hba->debugfs_files.debugfs_root = debugfs_create_dir(dev_name(hba->dev),
							     NULL);

	if (IS_ERR(hba->debugfs_files.debugfs_root))
		/* Don't complain -- debugfs just isn't enabled */
		goto err_no_root;
	if (!hba->debugfs_files.debugfs_root) {
		/*
		 * Complain -- debugfs is enabled, but it failed to
		 * create the directory
		 */
		dev_err(hba->dev,
			"%s: NULL debugfs root directory, exiting\n", __func__);
		goto err_no_root;
	}

	debugfs_create_symlink(root_name, NULL, dev_name(hba->dev));

	hba->debugfs_files.stats_folder = debugfs_create_dir("stats",
					hba->debugfs_files.debugfs_root);
	if (!hba->debugfs_files.stats_folder) {
		dev_err(hba->dev,
			"%s: NULL stats_folder, exiting\n", __func__);
		goto err;
	}

	hba->debugfs_files.tag_stats =
		debugfs_create_file("tag_stats", 0600,
					   hba->debugfs_files.stats_folder, hba,
					   &ufsdbg_tag_stats_fops);
	if (!hba->debugfs_files.tag_stats) {
		dev_err(hba->dev, "%s:  NULL tag_stats file, exiting\n",
			__func__);
		goto err;
	}

	hba->debugfs_files.query_stats =
		debugfs_create_file("query_stats", 0600,
					   hba->debugfs_files.stats_folder, hba,
					   &ufsdbg_query_stats_fops);
	if (!hba->debugfs_files.query_stats) {
		dev_err(hba->dev, "%s:  NULL query_stats file, exiting\n",
			__func__);
		goto err;
	}

	hba->debugfs_files.err_stats =
		debugfs_create_file("err_stats", 0600,
					   hba->debugfs_files.stats_folder, hba,
					   &ufsdbg_err_stats_fops);
	if (!hba->debugfs_files.err_stats) {
		dev_err(hba->dev, "%s:  NULL err_stats file, exiting\n",
			__func__);
		goto err;
	}

	if (ufshcd_init_statistics(hba)) {
		dev_err(hba->dev, "%s: Error initializing statistics\n",
			__func__);
		goto err;
	}

	hba->debugfs_files.host_regs = debugfs_create_file("host_regs", S_IRUGO,
				hba->debugfs_files.debugfs_root, hba,
				&ufsdbg_host_regs_fops);
	if (!hba->debugfs_files.host_regs) {
		dev_err(hba->dev, "%s:  NULL hcd regs file, exiting\n",
			__func__);
		goto err;
	}

	hba->debugfs_files.show_hba = debugfs_create_file("show_hba", S_IRUGO,
				hba->debugfs_files.debugfs_root, hba,
				&ufsdbg_show_hba_fops);
	if (!hba->debugfs_files.show_hba) {
		dev_err(hba->dev, "%s:  NULL hba file, exiting\n", __func__);
		goto err;
	}

	hba->debugfs_files.dump_dev_desc =
		debugfs_create_file("dump_device_desc", S_IRUGO,
				    hba->debugfs_files.debugfs_root, hba,
				    &ufsdbg_dump_device_desc);
	if (!hba->debugfs_files.dump_dev_desc) {
		dev_err(hba->dev,
			"%s:  NULL dump_device_desc file, exiting\n", __func__);
		goto err;
	}

	hba->debugfs_files.dump_string_desc_serial =
		debugfs_create_file("dump_string_desc_serial", S_IRUGO,
				    hba->debugfs_files.debugfs_root, hba,
				    &ufsdbg_dump_string_desc_serial);
	if (!hba->debugfs_files.dump_string_desc_serial) {
		dev_err(hba->dev,
			"%s:  NULL dump_device_desc file, exiting", __func__);
		goto err;
	}

	hba->debugfs_files.dump_heatlth_desc =
		debugfs_create_file("dump_health_desc", S_IRUGO,
				    hba->debugfs_files.debugfs_root, hba,
				    &ufsdbg_dump_health_desc);
	if (!hba->debugfs_files.dump_heatlth_desc) {
		dev_err(hba->dev,
			"%s:  NULL dump_health_desc file, exiting", __func__);
		goto err;
	}

	hba->debugfs_files.dbg_print_en =
		debugfs_create_file("dbg_print_en", 0600,
				    hba->debugfs_files.debugfs_root, hba,
				    &ufsdbg_dbg_print_en_ops);
	if (!hba->debugfs_files.dbg_print_en) {
		dev_err(hba->dev,
			"%s:  failed create dbg_print_en debugfs entry\n",
			__func__);
		goto err;
	}

	hba->debugfs_files.req_stats =
		debugfs_create_file("req_stats", 0600,
			hba->debugfs_files.stats_folder, hba,
			&ufsdbg_req_stats_desc);
	if (!hba->debugfs_files.req_stats) {
		dev_err(hba->dev,
			"%s:  failed create req_stats debugfs entry\n",
			__func__);
		goto err;
	}

	hba->debugfs_files.err_state =
		debugfs_create_file("err_state", 0655,
			hba->debugfs_files.debugfs_root, hba,
			&ufsdbg_err_state);
	if (!hba->debugfs_files.err_state) {
		dev_err(hba->dev,
		     "%s: failed create err_state debugfs entry\n", __func__);
		goto err;
	}

	if (!debugfs_create_bool("crash_on_err",
		0600, hba->debugfs_files.debugfs_root,
		&hba->crash_on_err))
		goto err;

	return;

err:
	debugfs_remove_recursive(hba->debugfs_files.debugfs_root);
	hba->debugfs_files.debugfs_root = NULL;
err_no_root:
	dev_err(hba->dev, "%s: failed to initialize debugfs\n", __func__);
}

void ufsdbg_remove_debugfs(struct ufs_hba *hba)
{
	debugfs_remove_recursive(hba->debugfs_files.debugfs_root);
	hba->debugfs_files.debugfs_root = NULL;
	kfree(hba->ufs_stats.tag_stats);
}
