/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"mi-disp-procfs:[%s:%d] " fmt, __func__, __LINE__
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <drm/mi_disp.h>
#include "mi_disp_config.h"
#include "mi_disp_print.h"
#include "mi_disp_core.h"
#include "mi_disp_feature.h"
#include "mi_dsi_display.h"

#if MI_DISP_PROCFS_ENABLE

const char *mipi_rw_str[MI_DISP_MAX] = {
	[MI_DISP_PRIMARY] = "mipi_rw_prim",
	[MI_DISP_SECONDARY] = "mipi_rw_sec",
};

const char *tx_cmd_set_str[MI_DISP_MAX] = {
	[MI_DISP_PRIMARY] = "tx_cmd_set_prim",
	[MI_DISP_SECONDARY] = "tx_cmd_set_sec",
};

struct disp_procfs {
	struct proc_dir_entry *mipi_rw_proc[MI_DISP_MAX];
	struct proc_dir_entry *tx_cmd_set[MI_DISP_MAX];
};

static struct disp_procfs disp_procfs;

static ssize_t mi_disp_procfs_tx_cmd_set_write(struct file *filp,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct disp_display *dd_ptr = PDE_DATA(file_inode(filp));
	char *input = NULL;
	int type;
	int ret = 0;

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	input = kmalloc(count, GFP_KERNEL);
	if (!input) {
		return -ENOMEM;
	}
	if (copy_from_user(input, buf, count)) {
		DISP_ERROR("copy from user failed\n");
		ret = -EFAULT;
		goto exit;
	}
	input[count-1] = '\0';
	DISP_DEBUG("copy_from_user input: %s\n", input);

	/* removes leading and trailing whitespace from input_copy */
	input = strim(input);
	ret = kstrtoint(input, 10, &type);
	if (ret) {
		DISP_ERROR("input buffer conversion failed\n");
		ret = -EAGAIN;
		goto exit;
	}

	ret = mi_dsi_display_write_dsi_cmd_set(dd_ptr->display, type);

exit:
	kfree(input);
	return ret ? ret : count;
}

static int mi_disp_procfs_tx_cmd_set_show(struct seq_file *m, void *v)
{
	struct disp_display *dd_ptr = (struct disp_display *)m->private;
	char *buf = NULL;
	int ret = 0;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	ret =  mi_dsi_display_show_dsi_cmd_set_type(dd_ptr->display, buf, PAGE_SIZE);
	if (ret > 0) {
		seq_printf(m, "%s", buf);
		seq_printf(m,"\n");
		ret = 0;
	} else {
		seq_printf(m,"read failed!\n");
		ret = -EAGAIN;
	}

	kfree(buf);
	return ret;
}

static int mi_disp_procfs_tx_cmd_set_open(struct inode *inode, struct file *file)
{
	return single_open(file, mi_disp_procfs_tx_cmd_set_show, PDE_DATA(inode));
}

static const struct proc_ops tx_cmd_set_proc_fops = {
	.proc_open    = mi_disp_procfs_tx_cmd_set_open,
	.proc_write   = mi_disp_procfs_tx_cmd_set_write,
	.proc_read    = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static ssize_t mi_disp_procfs_mipi_rw_write(struct file *filp,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct disp_display *dd_ptr = PDE_DATA(file_inode(filp));
	char *input = NULL;
	int ret = 0;

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	input = kmalloc(count, GFP_KERNEL);
	if (!input) {
		return -ENOMEM;
	}
	if (copy_from_user(input, buf, count)) {
		DISP_ERROR("copy from user failed\n");
		ret = -EFAULT;
		goto exit;
	}
	input[count-1] = '\0';
	DISP_DEBUG("copy_from_user input: %s\n", input);

	ret = mi_dsi_display_set_mipi_rw(dd_ptr->display, input);

exit:
	kfree(input);
	return ret ? ret : count;
}

static int mi_disp_procfs_mipi_rw_show(struct seq_file *m, void *v)
{
	struct disp_display *dd_ptr = (struct disp_display *)m->private;
	char *rbuf = NULL;
	int ret = 0;

	rbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!rbuf) {
		return -ENOMEM;
	}

	ret = mi_dsi_display_show_mipi_rw(dd_ptr->display, rbuf, PAGE_SIZE);
	if (ret > 0) {
		seq_printf(m, "return value: ");
		seq_printf(m, "%s", rbuf);
		seq_printf(m,"\n");
		ret = 0;
	} else {
		seq_printf(m,"read failed!\n");
		ret = -EAGAIN;
	}

	kfree(rbuf);
	return ret;
}

static int mi_disp_procfs_mipi_rw_open(struct inode *inode, struct file *file)
{
	return single_open(file, mi_disp_procfs_mipi_rw_show, PDE_DATA(inode));
}

static const struct proc_ops mipi_rw_proc_fops = {
	.proc_open    = mi_disp_procfs_mipi_rw_open,
	.proc_write   = mi_disp_procfs_mipi_rw_write,
	.proc_read    = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static int mi_disp_procfs_mipi_rw_init(void *d_display, int disp_id)
{
	struct disp_display *dd_ptr = (struct disp_display *)d_display;
	struct disp_core *disp_core = mi_get_disp_core();
	int ret = 0;

	if (!dd_ptr || !disp_core) {
		DISP_ERROR("invalid disp_display or disp_core ptr\n");
		return -EINVAL;
	}

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported %s display(%s intf)\n", get_disp_id_name(disp_id),
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	if (is_support_disp_id(disp_id)) {
		disp_procfs.mipi_rw_proc[disp_id] = proc_create_data(mipi_rw_str[disp_id],
			S_IRUGO | S_IWUSR, disp_core->procfs_dir,
			&mipi_rw_proc_fops, d_display);
		if (!disp_procfs.mipi_rw_proc[disp_id]) {
			DISP_ERROR("create procfs entry failed for %s\n", mipi_rw_str[disp_id]);
			ret = -ENODEV;
		} else {
			DISP_INFO("create procfs %s success!\n", mipi_rw_str[disp_id]);
			ret = 0;
		}

		disp_procfs.tx_cmd_set[disp_id] = proc_create_data(tx_cmd_set_str[disp_id],
			S_IRUGO | S_IWUSR, disp_core->procfs_dir,
			&tx_cmd_set_proc_fops, d_display);
		if (!disp_procfs.tx_cmd_set[disp_id]) {
			DISP_ERROR("create procfs entry failed for %s\n", tx_cmd_set_str[disp_id]);
			ret = -ENODEV;
		} else {
			DISP_INFO("create procfs %s success!\n", tx_cmd_set_str[disp_id]);
			ret = 0;
		}
	} else {
		DISP_ERROR("unknown display id\n");
		ret = -EINVAL;
	}

	return ret;
}

static int mi_disp_procfs_mipi_rw_deinit(void *d_display, int disp_id)
{
	struct disp_display *dd_ptr = (struct disp_display *)d_display;
	int ret = 0;

	if (!dd_ptr) {
		DISP_ERROR("invalid disp_display ptr\n");
		return -EINVAL;
	}

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported %s display(%s intf)\n", get_disp_id_name(disp_id),
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	if (is_support_disp_id(disp_id)) {
		if (disp_procfs.mipi_rw_proc[disp_id]) {
			proc_remove(disp_procfs.mipi_rw_proc[disp_id]);
			disp_procfs.mipi_rw_proc[disp_id] = NULL;
		}

		if (disp_procfs.tx_cmd_set[disp_id]) {
			proc_remove(disp_procfs.tx_cmd_set[disp_id]);
			disp_procfs.tx_cmd_set[disp_id] = NULL;
		}
		ret = 0;
	} else {
		DISP_ERROR("unknown display id\n");
		ret = -EINVAL;
	}

	return ret;
}

int mi_disp_procfs_init(void *d_display, int disp_id)
{
	int ret = 0;

	ret = mi_disp_procfs_mipi_rw_init(d_display, disp_id);

	return ret;
}

int mi_disp_procfs_deinit(void *d_display, int disp_id)
{
	int ret = 0;

	ret = mi_disp_procfs_mipi_rw_deinit(d_display, disp_id);

	return ret;
}

#endif /* MI_DISP_PROCFS_ENABLE */
