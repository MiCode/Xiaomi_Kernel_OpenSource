/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include "debugfs.h"

enum field_width {
	BYTE	= 1,
	WORD	= 2,
};

struct desc_field_offset {
	char *name;
	int offset;
	enum field_width width_byte;
};

#ifdef CONFIG_UFS_FAULT_INJECTION

#define INJECT_COMMAND_HANG (0x0)

static DECLARE_FAULT_ATTR(fail_default_attr);
static char *fail_request;
module_param(fail_request, charp, 0);

static bool inject_fatal_err_tr(struct ufs_hba *hba, u8 ocs_err)
{
	int tag;

	tag = find_first_bit(&hba->outstanding_reqs, hba->nutrs);
	if (tag == hba->nutrs)
		return 0;

	ufshcd_writel(hba, ~(1 << tag), REG_UTP_TRANSFER_REQ_LIST_CLEAR);
	(&hba->lrb[tag])->utr_descriptor_ptr->header.dword_2 =
							cpu_to_be32(ocs_err);

	/* fatal error injected */
	return 1;
}

static bool inject_fatal_err_tm(struct ufs_hba *hba, u8 ocs_err)
{
	int tag;

	tag = find_first_bit(&hba->outstanding_tasks, hba->nutmrs);
	if (tag == hba->nutmrs)
		return 0;

	ufshcd_writel(hba, ~(1 << tag), REG_UTP_TASK_REQ_LIST_CLEAR);
	(&hba->utmrdl_base_addr[tag])->header.dword_2 =
						cpu_to_be32(ocs_err);

	/* fatal error injected */
	return 1;
}

static bool inject_cmd_hang_tr(struct ufs_hba *hba)
{
	int tag;

	tag = find_first_bit(&hba->outstanding_reqs, hba->nutrs);
	if (tag == hba->nutrs)
		return 0;

	__clear_bit(tag, &hba->outstanding_reqs);
	hba->lrb[tag].cmd = NULL;
	__clear_bit(tag, &hba->lrb_in_use);

	/* command hang injected */
	return 1;
}

static int inject_cmd_hang_tm(struct ufs_hba *hba)
{
	int tag;

	tag = find_first_bit(&hba->outstanding_tasks, hba->nutmrs);
	if (tag == hba->nutmrs)
		return 0;

	__clear_bit(tag, &hba->outstanding_tasks);
	__clear_bit(tag, &hba->tm_slots_in_use);

	/* command hang injected */
	return 1;
}

void ufsdbg_fail_request(struct ufs_hba *hba, u32 *intr_status)
{
	u8 ocs_err;
	static const u32 errors[] = {
		CONTROLLER_FATAL_ERROR,
		SYSTEM_BUS_FATAL_ERROR,
		INJECT_COMMAND_HANG,
	};

	if (!should_fail(&hba->debugfs_files.fail_attr, 1))
		goto out;

	*intr_status = errors[prandom_u32() % ARRAY_SIZE(errors)];
	dev_info(hba->dev, "%s: fault-inject error: 0x%x\n",
			__func__, *intr_status);

	switch (*intr_status) {
	case CONTROLLER_FATAL_ERROR: /* fall through */
		ocs_err = OCS_FATAL_ERROR;
		goto set_ocs;
	case SYSTEM_BUS_FATAL_ERROR:
		ocs_err = OCS_INVALID_CMD_TABLE_ATTR;
set_ocs:
		if (!inject_fatal_err_tr(hba, ocs_err))
			if (!inject_fatal_err_tm(hba, ocs_err))
				*intr_status = 0;
		break;
	case INJECT_COMMAND_HANG:
		if (!inject_cmd_hang_tr(hba))
			inject_cmd_hang_tm(hba);
		break;
	default:
		BUG();
		/* some configurations ignore panics caused by BUG() */
		break;
	}
out:
	return;
}

static void ufsdbg_setup_fault_injection(struct ufs_hba *hba)
{
	hba->debugfs_files.fail_attr = fail_default_attr;

	if (fail_request)
		setup_fault_attr(&hba->debugfs_files.fail_attr, fail_request);

	/* suppress dump stack everytime failure is injected */
	hba->debugfs_files.fail_attr.verbose = 0;

	if (IS_ERR(fault_create_debugfs_attr("inject_fault",
					hba->debugfs_files.debugfs_root,
					&hba->debugfs_files.fail_attr)))
		dev_err(hba->dev, "%s: failed to create debugfs entry\n",
				__func__);
}
#else
void ufsdbg_fail_request(struct ufs_hba *hba, u32 *intr_status)
{
}

static void ufsdbg_setup_fault_injection(struct ufs_hba *hba)
{
}
#endif /* CONFIG_UFS_FAULT_INJECTION */

#define BUFF_LINE_CAPACITY 16
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
	seq_printf(file, " Tag Stat\t\t%s Queue Fullness\n", sep);
	for (i = 0; i < TAB_CHARS * (TS_NUM_STATS + 4); i++) {
		seq_puts(file, "-");
		if (i == (TAB_CHARS * 3 - 1))
			seq_puts(file, sep);
	}
	seq_printf(file,
		"\n #\tnum uses\t%s\t #\tAll\t Read\t Write\t Urgent\t Flush\n",
		sep);

	/* values */
	for (i = 0; i < max_depth; i++) {
		if (ufs_stats->tag_stats[i][0] <= 0 &&
				ufs_stats->tag_stats[i][1] <= 0 &&
				ufs_stats->tag_stats[i][2] <= 0 &&
				ufs_stats->tag_stats[i][3] <= 0 &&
				ufs_stats->tag_stats[i][4] <= 0)
			continue;

		is_tag_empty = false;
		seq_printf(file, " %d\t ", i);
		for (j = 0; j < TS_NUM_STATS; j++) {
			seq_printf(file, "%llu\t ", ufs_stats->tag_stats[i][j]);
			if (j == 0)
				seq_printf(file, "\t%s\t %d\t%llu\t ", sep, i,
						ufs_stats->tag_stats[i][j+1] +
						ufs_stats->tag_stats[i][j+2] +
						ufs_stats->tag_stats[i][j+3]);
		}
		seq_puts(file, "\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (is_tag_empty)
		pr_debug("%s: All tags statistics are empty", __func__);

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
		pr_debug("%s: Disabling UFS tag statistics", __func__);
	} else {
		ufs_stats->enabled = true;
		pr_debug("%s: Enabling & Resetting UFS tag statistics",
			 __func__);
		memset(hba->ufs_stats.tag_stats[0], 0,
			sizeof(**hba->ufs_stats.tag_stats) *
			TS_NUM_STATS * hba->nutrs);

		/* initialize current queue depth */
		ufs_stats->q_depth = 0;
		for_each_set_bit_from(bit, &hba->outstanding_reqs, hba->nutrs)
			ufs_stats->q_depth++;
		pr_debug("%s: Enabled UFS tag statistics", __func__);
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return cnt;
}

static const struct file_operations ufsdbg_tag_stats_fops = {
	.open		= ufsdbg_tag_stats_open,
	.read		= seq_read,
	.write		= ufsdbg_tag_stats_write,
};

static int ufshcd_init_tag_statistics(struct ufs_hba *hba)
{
	struct ufs_stats *stats = &hba->ufs_stats;
	int ret = 0;
	int i;

	stats->enabled = false;
	stats->tag_stats = kzalloc(sizeof(*stats->tag_stats) * hba->nutrs,
			GFP_KERNEL);
	if (!hba->ufs_stats.tag_stats)
		goto no_mem;

	stats->tag_stats[0] = kzalloc(sizeof(**stats->tag_stats) *
			TS_NUM_STATS * hba->nutrs, GFP_KERNEL);
	if (!stats->tag_stats[0])
		goto no_mem;

	for (i = 1; i < hba->nutrs; i++)
		stats->tag_stats[i] = &stats->tag_stats[0][i * TS_NUM_STATS];

	goto exit;

no_mem:
	dev_err(hba->dev, "%s: Unable to allocate UFS tag_stats", __func__);
	ret = -ENOMEM;
exit:
	return ret;
}

static void
ufsdbg_pr_buf_to_std(struct seq_file *file, void *buff, int size, char *str)
{
	int i;
	char linebuf[38];
	int lines = size/BUFF_LINE_CAPACITY +
			(size % BUFF_LINE_CAPACITY ? 1 : 0);

	for (i = 0; i < lines; i++) {
		hex_dump_to_buffer(buff + i * BUFF_LINE_CAPACITY,
				BUFF_LINE_CAPACITY, BUFF_LINE_CAPACITY, 4,
				linebuf, sizeof(linebuf), false);
		seq_printf(file, "%s [%x]: %s\n", str, i * BUFF_LINE_CAPACITY,
				linebuf);
	}
}

static int ufsdbg_host_regs_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;

	ufshcd_hold(hba, false);
	pm_runtime_get_sync(hba->dev);
	ufsdbg_pr_buf_to_std(file, hba->mmio_base, UFSHCI_REG_SPACE_SIZE,
				"host regs");
	pm_runtime_put_sync(hba->dev);
	ufshcd_release(hba);
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

static int ufsdbg_dump_device_desc_show(struct seq_file *file, void *data)
{
	int err = 0;
	int buff_len = QUERY_DESC_DEVICE_MAX_SIZE;
	u8 desc_buf[QUERY_DESC_DEVICE_MAX_SIZE];
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
		{"wPeriodicRTCUpdate",	0x1D, WORD}
	};

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

void ufsdbg_add_debugfs(struct ufs_hba *hba)
{
	if (!hba) {
		dev_err(hba->dev, "%s: NULL hba, exiting", __func__);
		goto err_no_root;
	}

	hba->debugfs_files.debugfs_root = debugfs_create_dir("ufs", NULL);
	if (IS_ERR(hba->debugfs_files.debugfs_root))
		/* Don't complain -- debugfs just isn't enabled */
		goto err_no_root;
	if (!hba->debugfs_files.debugfs_root) {
		/*
		 * Complain -- debugfs is enabled, but it failed to
		 * create the directory
		 */
		dev_err(hba->dev,
			"%s: NULL debugfs root directory, exiting", __func__);
		goto err_no_root;
	}

	hba->debugfs_files.tag_stats =
		debugfs_create_file("tag_stats", S_IRUSR,
					   hba->debugfs_files.debugfs_root, hba,
					   &ufsdbg_tag_stats_fops);
	if (!hba->debugfs_files.tag_stats) {
		dev_err(hba->dev, "%s:  NULL tag stats file, exiting",
			__func__);
		goto err;
	}

	if (ufshcd_init_tag_statistics(hba)) {
		dev_err(hba->dev, "%s: Error initializing tag stats",
			__func__);
		goto err;
	}

	hba->debugfs_files.host_regs = debugfs_create_file("host_regs", S_IRUSR,
				hba->debugfs_files.debugfs_root, hba,
				&ufsdbg_host_regs_fops);
	if (!hba->debugfs_files.host_regs) {
		dev_err(hba->dev, "%s:  NULL hcd regs file, exiting", __func__);
		goto err;
	}

	hba->debugfs_files.show_hba = debugfs_create_file("show_hba", S_IRUSR,
				hba->debugfs_files.debugfs_root, hba,
				&ufsdbg_show_hba_fops);
	if (!hba->debugfs_files.show_hba) {
		dev_err(hba->dev, "%s:  NULL hba file, exiting", __func__);
		goto err;
	}

	hba->debugfs_files.dump_dev_desc =
		debugfs_create_file("dump_device_desc", S_IRUSR,
				    hba->debugfs_files.debugfs_root, hba,
				    &ufsdbg_dump_device_desc);
	if (!hba->debugfs_files.dump_dev_desc) {
		dev_err(hba->dev,
			"%s:  NULL dump_device_desc file, exiting", __func__);
		goto err;
	}

	ufsdbg_setup_fault_injection(hba);

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
	kfree(hba->ufs_stats.tag_stats);

}
