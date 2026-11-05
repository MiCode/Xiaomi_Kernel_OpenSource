// SPDX-License-Identifier: GPL-2.0
//
// cs40l26-debugfs.c -- CS40L26 Boosted Haptic Driver with Integrated DSP and
// Waveform Memory with Advanced Closed Loop Algorithms and LRA protection
//
// Copyright 2022 Cirrus Logic, Inc.
//
// Author: Fred Treven <fred.treven@cirrus.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include <linux/mfd/cs40l26.h>

#ifdef CONFIG_DEBUG_FS
static int cs40l26_wseq_get(struct cs40l26_private *cs40l26,
		struct cs40l26_wseq *wseq, char **wseq_str)
{
	char str[CS40L26_WSEQ_STR_LINE_LEN];
	struct cs40l26_wseq_op *op;
	int error;

	if (list_empty(&wseq->ops) || wseq->num_ops <= 0) {
		dev_err(cs40l26->dev, "Write sequencer is empty\n");
		return -ENODATA;
	}

	list_for_each_entry(op, &wseq->ops, list) {
		error = snprintf(str, CS40L26_WSEQ_STR_LINE_LEN,
				"0x%08X: Code = 0x%02X, Addr = 0x%08X, Data = 0x%08X\n",
				wseq->base_addr + op->offset, op->operation, op->address, op->data);
		if (error <= 0)
			return -EINVAL;

		strncat(*wseq_str, str, CS40L26_WSEQ_STR_LINE_LEN);
	}

	return 0;
}

static ssize_t cs40l26_active_seq_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct cs40l26_private *cs40l26 = file->private_data;
	char *aseq_str = NULL;
	ssize_t error, aseq_str_size;

	mutex_lock(&cs40l26->lock);

	aseq_str_size = CS40L26_WSEQ_STR_LINE_LEN * cs40l26->aseq.num_ops;
	aseq_str = kzalloc(aseq_str_size, GFP_KERNEL);
	if (!aseq_str) {
		error = -ENOMEM;
		goto err_mutex;
	}

	error = cs40l26_wseq_get(cs40l26, &cs40l26->aseq, &aseq_str);
	if (error)
		goto err_free;

	error = simple_read_from_buffer(user_buf, count, ppos, aseq_str, aseq_str_size);

err_free:
	kfree(aseq_str);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t cs40l26_power_on_seq_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct cs40l26_private *cs40l26 = file->private_data;
	char *pseq_str = NULL;
	ssize_t error, pseq_str_size;

	mutex_lock(&cs40l26->lock);

	pseq_str_size = CS40L26_WSEQ_STR_LINE_LEN * cs40l26->pseq.num_ops;
	pseq_str = kzalloc(pseq_str_size, GFP_KERNEL);
	if (!pseq_str) {
		error = -ENOMEM;
		goto err_mutex;
	}

	error = cs40l26_wseq_get(cs40l26, &cs40l26->pseq, &pseq_str);
	if (error)
		goto err_free;

	error = simple_read_from_buffer(user_buf, count, ppos, pseq_str, pseq_str_size);

err_free:
	kfree(pseq_str);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t cs40l26_hw_reg_read(struct file *file, char __user *user_buf, size_t count,
		loff_t *ppos)
{
	struct cs40l26_private *cs40l26 = file->private_data;
	char result[CS40L26_HW_STR_LEN];
	ssize_t error;

	mutex_lock(&cs40l26->lock);

	error = snprintf(result, count, "0x%08X\n", cs40l26->dbg_hw_reg);
	if (error <= 0) {
		error = -EINVAL;
		goto err_mutex;
	}

	error = simple_read_from_buffer(user_buf, count, ppos, result, strlen(result));

err_mutex:
	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t cs40l26_hw_reg_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct cs40l26_private *cs40l26 = file->private_data;
	char str[CS40L26_HW_STR_LEN];
	ssize_t error;
	u32 reg;

	memset(str, 0, CS40L26_HW_STR_LEN);

	mutex_lock(&cs40l26->lock);

	simple_write_to_buffer(str, count, ppos, user_buf, count);

	error = kstrtou32(str, CS40L26_HW_STR_LEN, &reg);
	if (error)
		goto err_mutex;

	cs40l26->dbg_hw_reg = reg;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	return error ? error : count;
}

static ssize_t cs40l26_hw_val_read(struct file *file, char __user *user_buf, size_t count,
		loff_t *ppos)
{
	struct cs40l26_private *cs40l26 = file->private_data;
	char result[CS40L26_HW_STR_LEN];
	ssize_t error;
	u32 val;

	if (cs40l26->dbg_hw_reg % CL_DSP_BYTES_PER_WORD) {
		dev_err(cs40l26->dev, "Reg. address 0x%08X not multiple of 4\n",
				cs40l26->dbg_hw_reg);
		return -EINVAL;
	}

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = regmap_read(cs40l26->regmap, cs40l26->dbg_hw_reg, &val);
	if (error)
		goto err_mutex;

	error = snprintf(result, CS40L26_HW_STR_LEN, "0x%08X\n", val);
	if (error <= 0) {
		error = -EINVAL;
		goto err_mutex;
	}

	error = simple_read_from_buffer(user_buf, count, ppos, result, strlen(result));

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error;
}

static ssize_t cs40l26_hw_val_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct cs40l26_private *cs40l26 = file->private_data;
	char str[CS40L26_HW_STR_LEN];
	ssize_t error;
	u32 val;

	if (cs40l26->dbg_hw_reg % CL_DSP_BYTES_PER_WORD) {
		dev_err(cs40l26->dev, "Reg. address 0x%08X not multiple of 4\n",
				cs40l26->dbg_hw_reg);
		return -EINVAL;
	}

	memset(str, 0, CS40L26_HW_STR_LEN);

	simple_write_to_buffer(str, count, ppos, user_buf, count);

	error = kstrtou32(str, CS40L26_HW_STR_LEN, &val);
	if (error)
		return error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = regmap_write(cs40l26->regmap, cs40l26->dbg_hw_reg, val);
	if (error)
		goto exit_mutex;

	error = cs40l26_wseq_write(cs40l26, cs40l26->dbg_hw_reg, (val & GENMASK(31, 16)) >> 16,
			true, CS40L26_WSEQ_OP_WRITE_H16, &cs40l26->pseq);
	if (error)
		goto exit_mutex;

	error = cs40l26_wseq_write(cs40l26, cs40l26->dbg_hw_reg, (val & GENMASK(15, 0)),
			true, CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);

exit_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}

static const struct {
	const char *name;
	const struct file_operations fops;
} cs40l26_hw_debugfs_fops[] = {
	{
		.name = "register",
		.fops = {
			.open = simple_open,
			.read = cs40l26_hw_reg_read,
			.write = cs40l26_hw_reg_write,
		},
	},
	{
		.name = "value",
		.fops = {
			.open = simple_open,
			.read = cs40l26_hw_val_read,
			.write = cs40l26_hw_val_write,
		},
	},
};

static const struct {
	const char *name;
	const struct file_operations fops;
} cs40l26_debugfs_fops[] = {
	{
		.name = "active_seq",
		.fops = {
			.open = simple_open,
			.read = cs40l26_active_seq_read,
		},
	},
	{
		.name = "power_on_seq",
		.fops = {
			.open = simple_open,
			.read = cs40l26_power_on_seq_read,
		},
	},
};

static void cs40l26_hw_debugfs_init(struct cs40l26_private *cs40l26)
{
	int i;

	cs40l26->debugfs_hw_node = debugfs_create_dir("hardware", cs40l26->debugfs_root);
	if (IS_ERR_OR_NULL(cs40l26->debugfs_hw_node)) {
		dev_err(cs40l26->dev, "Failed to mount hardware debugfs directory\n");
		kfree(cs40l26->debugfs_hw_node);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(cs40l26_hw_debugfs_fops); i++)
		debugfs_create_file(cs40l26_hw_debugfs_fops[i].name, CL_DSP_DEBUGFS_RW_FILE_MODE,
				cs40l26->debugfs_hw_node, cs40l26,
				&cs40l26_hw_debugfs_fops[i].fops);
}

void cs40l26_debugfs_init(struct cs40l26_private *cs40l26)
{
	struct dentry *root = NULL;
	int i;

	cs40l26_debugfs_cleanup(cs40l26);

	root = debugfs_create_dir("cs40l26", NULL);
	if (!root)
		return;

	for (i = 0; i < ARRAY_SIZE(cs40l26_debugfs_fops); i++)
		debugfs_create_file(cs40l26_debugfs_fops[i].name, CL_DSP_DEBUGFS_RW_FILE_MODE,
				root, cs40l26, &cs40l26_debugfs_fops[i].fops);

	cs40l26->debugfs_root = root;

	cs40l26_hw_debugfs_init(cs40l26);

	if (cs40l26->fw_id == CS40L26_FW_ID &&
			cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_EVENT_LOGGER_ALGO_ID)) {
		cs40l26->cl_dsp_db = cl_dsp_debugfs_create(cs40l26->dsp, cs40l26->debugfs_root,
				(u32) CS40L26_EVENT_LOGGER_ALGO_ID);

		if (IS_ERR_OR_NULL(cs40l26->cl_dsp_db))
			dev_err(cs40l26->dev, "Failed to create CL DSP Debugfs\n");
	}
}
EXPORT_SYMBOL_GPL(cs40l26_debugfs_init);

void cs40l26_debugfs_cleanup(struct cs40l26_private *cs40l26)
{
	cl_dsp_debugfs_destroy(cs40l26->cl_dsp_db);
	cs40l26->cl_dsp_db = NULL;
	debugfs_remove_recursive(cs40l26->debugfs_root);
}
EXPORT_SYMBOL_GPL(cs40l26_debugfs_cleanup);

#endif /* CONFIG_DEBUG_FS */
