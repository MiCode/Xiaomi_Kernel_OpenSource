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
#include <linux/scsi/ufs/unipro.h>
#include "ufshci.h"

enum field_width {
	BYTE	= 1,
	WORD	= 2,
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

#define DOORBELL_CLR_TOUT_US	(1000 * 1000) /* 1 sec */

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

	pr_debug("%s: Resetting UFS error statistics", __func__);
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

	memset(stats->err_stats, 0, sizeof(hba->ufs_stats.err_stats));

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

static int ufsdbg_power_mode_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;
	char *names[] = {
		"INVALID MODE",
		"FAST MODE",
		"SLOW MODE",
		"INVALID MODE",
		"FASTAUTO MODE",
		"SLOWAUTO MODE",
		"INVALID MODE",
	};

	/* Print current status */
	seq_puts(file, "UFS current power mode [RX, TX]:");
	seq_printf(file, "gear=[%d,%d], lane=[%d,%d], pwr=[%s,%s], rate = %c",
		 hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		 hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		 names[hba->pwr_info.pwr_rx],
		 names[hba->pwr_info.pwr_tx],
		 hba->pwr_info.hs_rate == PA_HS_MODE_B ? 'B' : 'A');
	seq_puts(file, "\n\n");

	/* Print usage */
	seq_puts(file,
		"To change power mode write 'GGLLMM' where:\n"
		"G - selected gear\n"
		"L - number of lanes\n"
		"M - power mode:\n"
		"\t1 = fast mode\n"
		"\t2 = slow mode\n"
		"\t4 = fast-auto mode\n"
		"\t5 = slow-auto mode\n"
		"first letter is for RX, second letter is for TX.\n\n");

	return 0;
}

static bool ufsdbg_power_mode_validate(struct ufs_pa_layer_attr *pwr_mode)
{
	if (pwr_mode->gear_rx < UFS_PWM_G1 || pwr_mode->gear_rx > UFS_PWM_G7 ||
	    pwr_mode->gear_tx < UFS_PWM_G1 || pwr_mode->gear_tx > UFS_PWM_G7 ||
	    pwr_mode->lane_rx < 1 || pwr_mode->lane_rx > 2 ||
	    pwr_mode->lane_tx < 1 || pwr_mode->lane_tx > 2 ||
	    (pwr_mode->pwr_rx != FAST_MODE && pwr_mode->pwr_rx != SLOW_MODE &&
	     pwr_mode->pwr_rx != FASTAUTO_MODE &&
	     pwr_mode->pwr_rx != SLOWAUTO_MODE) ||
	    (pwr_mode->pwr_tx != FAST_MODE && pwr_mode->pwr_tx != SLOW_MODE &&
	     pwr_mode->pwr_tx != FASTAUTO_MODE &&
	     pwr_mode->pwr_tx != SLOWAUTO_MODE)) {
		pr_err("%s: power parameters are not valid\n", __func__);
		return false;
	}

	return true;
}

static int ufsdbg_cfg_pwr_param(struct ufs_hba *hba,
				struct ufs_pa_layer_attr *new_pwr,
				struct ufs_pa_layer_attr *final_pwr)
{
	int ret = 0;
	bool is_dev_sup_hs = false;
	bool is_new_pwr_hs = false;
	int dev_pwm_max_rx_gear;
	int dev_pwm_max_tx_gear;

	if (!hba->max_pwr_info.is_valid) {
		dev_err(hba->dev, "%s: device max power is not valid. can't configure power\n",
			__func__);
		return -EINVAL;
	}

	if (hba->max_pwr_info.info.pwr_rx == FAST_MODE)
		is_dev_sup_hs = true;

	if (new_pwr->pwr_rx == FAST_MODE || new_pwr->pwr_rx == FASTAUTO_MODE)
		is_new_pwr_hs = true;

	final_pwr->lane_rx = hba->max_pwr_info.info.lane_rx;
	final_pwr->lane_tx = hba->max_pwr_info.info.lane_tx;

	/* device doesn't support HS but requested power is HS */
	if (!is_dev_sup_hs && is_new_pwr_hs) {
		pr_err("%s: device doesn't support HS. requested power is HS\n",
			__func__);
		return -ENOTSUPP;
	} else if ((is_dev_sup_hs && is_new_pwr_hs) ||
		   (!is_dev_sup_hs && !is_new_pwr_hs)) {
		/*
		 * If device and requested power mode are both HS or both PWM
		 * then dev_max->gear_xx are the gears to be assign to
		 * final_pwr->gear_xx
		 */
		final_pwr->gear_rx = hba->max_pwr_info.info.gear_rx;
		final_pwr->gear_tx = hba->max_pwr_info.info.gear_tx;
	} else if (is_dev_sup_hs && !is_new_pwr_hs) {
		/*
		 * If device supports HS but requested power is PWM, then we
		 * need to find out what is the max gear in PWM the device
		 * supports
		 */

		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
			       &dev_pwm_max_rx_gear);

		if (!dev_pwm_max_rx_gear) {
			pr_err("%s: couldn't get device max pwm rx gear\n",
				__func__);
			ret = -EINVAL;
			goto out;
		}

		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				    &dev_pwm_max_tx_gear);

		if (!dev_pwm_max_tx_gear) {
			pr_err("%s: couldn't get device max pwm tx gear\n",
				__func__);
			ret = -EINVAL;
			goto out;
		}

		final_pwr->gear_rx = dev_pwm_max_rx_gear;
		final_pwr->gear_tx = dev_pwm_max_tx_gear;
	}

	if ((new_pwr->gear_rx > final_pwr->gear_rx) ||
	    (new_pwr->gear_tx > final_pwr->gear_tx) ||
	    (new_pwr->lane_rx > final_pwr->lane_rx) ||
	    (new_pwr->lane_tx > final_pwr->lane_tx)) {
		pr_err("%s: (RX,TX) GG,LL: in PWM/HS new pwr [%d%d,%d%d] exceeds device limitation [%d%d,%d%d]\n",
			__func__,
			new_pwr->gear_rx, new_pwr->gear_tx,
			new_pwr->lane_rx, new_pwr->lane_tx,
			final_pwr->gear_rx, final_pwr->gear_tx,
			final_pwr->lane_rx, final_pwr->lane_tx);
		return -ENOTSUPP;
	}

	final_pwr->gear_rx = new_pwr->gear_rx;
	final_pwr->gear_tx = new_pwr->gear_tx;
	final_pwr->lane_rx = new_pwr->lane_rx;
	final_pwr->lane_tx = new_pwr->lane_tx;
	final_pwr->pwr_rx = new_pwr->pwr_rx;
	final_pwr->pwr_tx = new_pwr->pwr_tx;
	final_pwr->hs_rate = new_pwr->hs_rate;

out:
	return ret;
}

static int ufsdbg_config_pwr_mode(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *desired_pwr_mode)
{
	int ret;

	pm_runtime_get_sync(hba->dev);
	scsi_block_requests(hba->host);
	ret = ufshcd_wait_for_doorbell_clr(hba, DOORBELL_CLR_TOUT_US);
	if (!ret)
		ret = ufshcd_change_power_mode(hba, desired_pwr_mode);
	scsi_unblock_requests(hba->host);
	pm_runtime_put_sync(hba->dev);

	return ret;
}

static ssize_t ufsdbg_power_mode_write(struct file *file,
				const char __user *ubuf, size_t cnt,
				loff_t *ppos)
{
	struct ufs_hba *hba = file->f_mapping->host->i_private;
	struct ufs_pa_layer_attr pwr_mode;
	struct ufs_pa_layer_attr final_pwr_mode;
	char pwr_mode_str[BUFF_LINE_CAPACITY] = {0};
	loff_t buff_pos = 0;
	int ret;
	int idx = 0;

	ret = simple_write_to_buffer(pwr_mode_str, BUFF_LINE_CAPACITY,
		&buff_pos, ubuf, cnt);

	pwr_mode.gear_rx = pwr_mode_str[idx++] - '0';
	pwr_mode.gear_tx = pwr_mode_str[idx++] - '0';
	pwr_mode.lane_rx = pwr_mode_str[idx++] - '0';
	pwr_mode.lane_tx = pwr_mode_str[idx++] - '0';
	pwr_mode.pwr_rx = pwr_mode_str[idx++] - '0';
	pwr_mode.pwr_tx = pwr_mode_str[idx++] - '0';

	/*
	 * Switching between rates is not currently supported so use the
	 * current rate.
	 * TODO: add rate switching if and when it is supported in the future
	 */
	pwr_mode.hs_rate = hba->pwr_info.hs_rate;

	/* Validate user input */
	if (!ufsdbg_power_mode_validate(&pwr_mode))
		return -EINVAL;

	pr_debug("%s: new power mode requested [RX,TX]: Gear=[%d,%d], Lane=[%d,%d], Mode=[%d,%d]\n",
		__func__,
		pwr_mode.gear_rx, pwr_mode.gear_tx, pwr_mode.lane_rx,
		pwr_mode.lane_tx, pwr_mode.pwr_rx, pwr_mode.pwr_tx);

	ret = ufsdbg_cfg_pwr_param(hba, &pwr_mode, &final_pwr_mode);
	if (ret) {
		dev_err(hba->dev,
			"%s: failed to configure new power parameters, ret = %d\n",
			__func__, ret);
		return cnt;
	}

	ret = ufsdbg_config_pwr_mode(hba, &final_pwr_mode);
	if (ret == -EBUSY)
		dev_err(hba->dev,
			"%s: ufshcd_config_pwr_mode failed: system is busy, try again\n",
			__func__);
	else if (ret)
		dev_err(hba->dev,
			"%s: ufshcd_config_pwr_mode failed, ret=%d\n",
			__func__, ret);

	return cnt;
}

static int ufsdbg_power_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_power_mode_show, inode->i_private);
}

static const struct file_operations ufsdbg_power_mode_desc = {
	.open		= ufsdbg_power_mode_open,
	.read		= seq_read,
	.write		= ufsdbg_power_mode_write,
};

static int ufsdbg_dme_read(void *data, u64 *attr_val, bool peer)
{
	int ret;
	struct ufs_hba *hba = data;
	u32 attr_id, read_val = 0;
	int (*read_func) (struct ufs_hba *, u32, u32 *);

	if (!hba)
		return -EINVAL;

	read_func = peer ? ufshcd_dme_peer_get : ufshcd_dme_get;
	attr_id = peer ? hba->debugfs_files.dme_peer_attr_id :
			 hba->debugfs_files.dme_local_attr_id;
	pm_runtime_get_sync(hba->dev);
	scsi_block_requests(hba->host);
	ret = ufshcd_wait_for_doorbell_clr(hba, DOORBELL_CLR_TOUT_US);
	if (!ret)
		ret = read_func(hba, UIC_ARG_MIB(attr_id), &read_val);
	scsi_unblock_requests(hba->host);
	pm_runtime_put_sync(hba->dev);

	if (!ret)
		*attr_val = (u64)read_val;

	return ret;
}

static int ufsdbg_dme_local_set_attr_id(void *data, u64 attr_id)
{
	struct ufs_hba *hba = data;

	if (!hba)
		return -EINVAL;

	hba->debugfs_files.dme_local_attr_id = (u32)attr_id;

	return 0;
}

static int ufsdbg_dme_local_read(void *data, u64 *attr_val)
{
	return ufsdbg_dme_read(data, attr_val, false);
}

DEFINE_SIMPLE_ATTRIBUTE(ufsdbg_dme_local_read_ops,
			ufsdbg_dme_local_read,
			ufsdbg_dme_local_set_attr_id,
			"%llu\n");

static int ufsdbg_dme_peer_read(void *data, u64 *attr_val)
{
	struct ufs_hba *hba = data;

	if (!hba)
		return -EINVAL;
	else
		return ufsdbg_dme_read(data, attr_val, true);
}

static int ufsdbg_dme_peer_set_attr_id(void *data, u64 attr_id)
{
	struct ufs_hba *hba = data;

	if (!hba)
		return -EINVAL;

	hba->debugfs_files.dme_peer_attr_id = (u32)attr_id;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(ufsdbg_dme_peer_read_ops,
			ufsdbg_dme_peer_read,
			ufsdbg_dme_peer_set_attr_id,
			"%llu\n");

void ufsdbg_add_debugfs(struct ufs_hba *hba)
{
	if (!hba) {
		pr_err("%s: NULL hba, exiting\n", __func__);
		return;
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

	hba->debugfs_files.err_stats =
		debugfs_create_file("err_stats", S_IRUSR,
					   hba->debugfs_files.debugfs_root, hba,
					   &ufsdbg_err_stats_fops);
	if (!hba->debugfs_files.err_stats) {
		dev_err(hba->dev, "%s:  NULL err stats file, exiting",
			__func__);
		goto err;
	}

	if (ufshcd_init_statistics(hba)) {
		dev_err(hba->dev, "%s: Error initializing statistics",
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

	hba->debugfs_files.power_mode =
		debugfs_create_file("power_mode", S_IRUSR | S_IWUSR,
				    hba->debugfs_files.debugfs_root, hba,
				    &ufsdbg_power_mode_desc);
	if (!hba->debugfs_files.power_mode) {
		dev_err(hba->dev,
			"%s:  NULL power_mode_desc file, exiting", __func__);
		goto err;
	}

	hba->debugfs_files.dme_local_read =
		debugfs_create_file("dme_local_read", S_IRUSR | S_IWUSR,
				    hba->debugfs_files.debugfs_root, hba,
				    &ufsdbg_dme_local_read_ops);
	if (!hba->debugfs_files.dme_local_read) {
		dev_err(hba->dev,
			"%s:  failed create dme_local_read debugfs entry\n",
			__func__);
		goto err;
	}

	hba->debugfs_files.dme_peer_read =
		debugfs_create_file("dme_peer_read", S_IRUSR | S_IWUSR,
				    hba->debugfs_files.debugfs_root, hba,
				    &ufsdbg_dme_peer_read_ops);
	if (!hba->debugfs_files.dme_peer_read) {
		dev_err(hba->dev,
			"%s:  failed create dme_peer_read debugfs entry\n",
			__func__);
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
