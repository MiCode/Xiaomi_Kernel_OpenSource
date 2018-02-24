/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <soc/qcom/scm.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include "lmh_dbg.h"

#define LMH_MON_NAME			"lmh_monitor"
#define LMH_DBGFS_READ			"data"
#define LMH_DBGFS_CONFIG_READ		"config"
#define LMH_DBGFS_READ_TYPES		"data_types"
#define LMH_DBGFS_CONFIG_TYPES		"config_types"
#define LMH_SCM_PAYLOAD_SIZE		10
#define LMH_READ_LINE_LENGTH            10
#define LMH_DEBUG_READ_TYPE		0x0
#define LMH_DEBUG_CONFIG_TYPE		0x1
#define LMH_DEBUG_SET			0x08
#define LMH_DEBUG_READ_BUF_SIZE		0x09
#define LMH_DEBUG_READ			0x0A
#define LMH_DEBUG_GET_TYPE		0x0B

struct lmh_driver_data {
	struct device			*dev;
	uint32_t			*read_type;
	uint32_t			*config_type;
	uint32_t			read_type_count;
	uint32_t			config_type_count;
	struct dentry			*debugfs_parent;
	struct dentry			*debug_read;
	struct dentry			*debug_config;
	struct dentry			*debug_read_type;
	struct dentry			*debug_config_type;
};

enum lmh_read_type {
	LMH_READ_TYPE = 0,
	LMH_CONFIG_TYPE,
};

static struct lmh_driver_data		*lmh_data;

static int lmh_debug_read(uint32_t **buf)
{
	int ret = 0, size = 0, tz_ret = 0;
	static uint32_t curr_size;
	struct scm_desc desc_arg;
	static uint32_t *payload;

	desc_arg.arginfo = SCM_ARGS(0);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_DEBUG_READ_BUF_SIZE), &desc_arg);
	size = desc_arg.ret[0];
	if (ret) {
		pr_err("Error in SCM get debug buffer size call. err:%d\n",
				ret);
		goto get_dbg_exit;
	}
	if (!size) {
		pr_err("No Debug data to read\n");
		ret = -ENODEV;
		goto get_dbg_exit;
	}
	size = SCM_BUFFER_SIZE(uint32_t) * size * LMH_READ_LINE_LENGTH;
	if (curr_size != size) {
		if (payload)
			devm_kfree(lmh_data->dev, payload);
		payload = devm_kzalloc(lmh_data->dev, PAGE_ALIGN(size),
				       GFP_KERNEL);
		if (!payload) {
			ret = -ENOMEM;
			goto get_dbg_exit;
		}
		curr_size = size;
	}

	/* &payload may be a physical address > 4 GB */
	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = curr_size;
	desc_arg.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
	dmac_flush_range(payload, (void *)payload + curr_size);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, LMH_DEBUG_READ),
			&desc_arg);
	dmac_inv_range(payload, (void *)payload + curr_size);
	tz_ret = desc_arg.ret[0];
	/* Have memory barrier before we access the TZ data */
	mb();
	if (ret) {
		pr_err("Error in get debug read. err:%d\n", ret);
		goto get_dbg_exit;
	}
	if (tz_ret) {
		pr_err("TZ API returned error. err:%d\n", tz_ret);
		ret = tz_ret;
		goto get_dbg_exit;
	}

get_dbg_exit:
	if (ret && payload) {
		devm_kfree(lmh_data->dev, payload);
		payload = NULL;
		curr_size = 0;
	}
	*buf = payload;

	return (ret < 0) ? ret : curr_size;
}

static int lmh_debug_config_write(uint32_t cmd_id, uint32_t *buf, int size)
{
	int ret = 0, size_bytes = 0;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL;

	size_bytes = (size - 3) * sizeof(uint32_t);
	payload = devm_kzalloc(lmh_data->dev, PAGE_ALIGN(size_bytes),
			       GFP_KERNEL);
	if (!payload) {
		ret = -ENOMEM;
		goto set_cfg_exit;
	}
	memcpy(payload, &buf[3], size_bytes);

	/* &payload may be a physical address > 4 GB */
	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = size_bytes;
	desc_arg.args[2] = buf[0];
	desc_arg.args[3] = buf[1];
	desc_arg.args[4] = buf[2];
	desc_arg.arginfo = SCM_ARGS(5, SCM_RO, SCM_VAL, SCM_VAL, SCM_VAL,
					SCM_VAL);
	dmac_flush_range(payload, (void *)payload + size_bytes);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, cmd_id), &desc_arg);
	/* Have memory barrier before we access the TZ data */
	mb();
	if (ret) {
		pr_err("Error in config debug read. err:%d\n", ret);
		goto set_cfg_exit;
	}

set_cfg_exit:
	return ret;
}

static int lmh_parse_and_extract(const char __user *user_buf, size_t count,
				enum lmh_read_type type)
{
	char *local_buf = NULL, *token = NULL, *curr_ptr = NULL, *token1 = NULL;
	char *next_line = NULL;
	int ret = 0, data_ct = 0, i = 0, size = 0;
	uint32_t *config_buf = NULL;

	/* Allocate two extra space to add ';' character and NULL terminate */
	local_buf = kzalloc(count + 2, GFP_KERNEL);
	if (!local_buf) {
		ret = -ENOMEM;
		goto dfs_cfg_write_exit;
	}
	if (copy_from_user(local_buf, user_buf, count)) {
		pr_err("user buf error\n");
		ret = -EFAULT;
		goto dfs_cfg_write_exit;
	}
	size = count + (strnchr(local_buf, count, '\n') ? 1 :  2);
	local_buf[size - 2] = ';';
	local_buf[size - 1] = '\0';
	curr_ptr = next_line = local_buf;
	while ((token1 = strnchr(next_line, local_buf + size - next_line, ';'))
		!= NULL) {
		data_ct = 0;
		*token1 = '\0';
		curr_ptr = next_line;
		next_line = token1 + 1;
		for (token = (char *)curr_ptr; token &&
			((token = strnchr(token, next_line - token, ' '))
			 != NULL); token++)
			data_ct++;
		if (data_ct < 2) {
			pr_err("Invalid format string:[%s]\n", curr_ptr);
			ret = -EINVAL;
			goto dfs_cfg_write_exit;
		}
		config_buf = kzalloc((++data_ct) * sizeof(uint32_t),
				GFP_KERNEL);
		if (!config_buf) {
			ret = -ENOMEM;
			goto dfs_cfg_write_exit;
		}
		pr_debug("Input:%s data_ct:%d\n", curr_ptr, data_ct);
		for (i = 0, token = (char *)curr_ptr; token && (i < data_ct);
			i++) {
			token = strnchr(token, next_line - token, ' ');
			if (token)
				*token = '\0';
			ret = kstrtouint(curr_ptr, 0, &config_buf[i]);
			if (ret < 0) {
				pr_err("Data[%s] scan error. err:%d\n",
					curr_ptr, ret);
				kfree(config_buf);
				goto dfs_cfg_write_exit;
			}
			if (token)
				curr_ptr = ++token;
		}
		switch (type) {
		case LMH_READ_TYPE:
		case LMH_CONFIG_TYPE:
			ret = lmh_debug_config_write(LMH_DEBUG_SET,
					config_buf, data_ct);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		kfree(config_buf);
		if (ret) {
			pr_err("Config error. type:%d err:%d\n", type, ret);
			goto dfs_cfg_write_exit;
		}
	}

dfs_cfg_write_exit:
	kfree(local_buf);
	return ret;
}

static ssize_t lmh_dbgfs_config_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	lmh_parse_and_extract(user_buf, count, LMH_CONFIG_TYPE);
	return count;
}

static int lmh_dbgfs_data_read(struct seq_file *seq_fp, void *data)
{
	static uint32_t *read_buf;
	static int read_buf_size;
	int idx = 0, ret = 0;

	if (!read_buf_size) {
		ret = lmh_debug_read(&read_buf);
		if (ret <= 0)
			goto dfs_read_exit;
		if (!read_buf || ret < sizeof(uint32_t)) {
			ret = -EINVAL;
			goto dfs_read_exit;
		}
		read_buf_size = ret;
		ret = 0;
	}

	do {
		seq_printf(seq_fp, "0x%x ", read_buf[idx]);
		if (seq_has_overflowed(seq_fp)) {
			pr_err("Seq overflow. idx:%d\n", idx);
			goto dfs_read_exit;
		}
		idx++;
		if ((idx % LMH_READ_LINE_LENGTH) == 0) {
			seq_puts(seq_fp, "\n");
			if (seq_has_overflowed(seq_fp)) {
				pr_err("Seq overflow. idx:%d\n", idx);
				goto dfs_read_exit;
			}
		}
	} while (idx < (read_buf_size / sizeof(uint32_t)));
	read_buf_size = 0;
	read_buf = NULL;

dfs_read_exit:
	return ret;
}

static int lmh_get_recurssive_data(struct scm_desc *desc_arg, uint32_t cmd_idx,
		uint32_t *payload, uint32_t *size, uint32_t **dest_buf)
{
	int idx = 0, ret = 0;
	uint32_t next = 0;

	do {
		desc_arg->args[cmd_idx] = next;
		dmac_flush_range(payload, (void *)payload +
				sizeof(*payload) * LMH_SCM_PAYLOAD_SIZE);
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, LMH_DEBUG_GET_TYPE),
				desc_arg);
		dmac_inv_range(payload, (void *)payload +
				sizeof(*payload) * LMH_SCM_PAYLOAD_SIZE);
		*size = desc_arg->ret[0];
		/* Have barrier before reading from TZ data */
		mb();
		if (ret) {
			pr_err("Error in SCM get type. cmd:%x err:%d\n",
				LMH_DEBUG_GET_TYPE, ret);
			return ret;
		}
		if (!*size) {
			pr_err("No LMH device supported\n");
			return -ENODEV;
		}
		if (!*dest_buf) {
			*dest_buf = devm_kcalloc(lmh_data->dev, *size,
				sizeof(**dest_buf), GFP_KERNEL);
			if (!*dest_buf)
				return -ENOMEM;
		}

		for (idx = next;
			idx < min((next + LMH_SCM_PAYLOAD_SIZE), *size);
			idx++)
			(*dest_buf)[idx] = payload[idx - next];
		next += LMH_SCM_PAYLOAD_SIZE;
	} while (next < *size);

	return ret;
}

static ssize_t lmh_dbgfs_data_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	lmh_parse_and_extract(user_buf, count, LMH_READ_TYPE);
	return count;
}

static int lmh_dbgfs_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, lmh_dbgfs_data_read, inode->i_private);
}

static int lmh_debug_get_types(bool is_read, uint32_t **buf)
{
	int ret = 0;
	uint32_t size = 0;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL, *dest_buf = NULL;

	if (is_read && lmh_data->read_type) {
		*buf = lmh_data->read_type;
		return lmh_data->read_type_count;
	} else if (!is_read && lmh_data->config_type) {
		*buf = lmh_data->config_type;
		return lmh_data->config_type_count;
	}
	payload = devm_kzalloc(lmh_data->dev,
				PAGE_ALIGN(LMH_SCM_PAYLOAD_SIZE *
				sizeof(*payload)), GFP_KERNEL);
	if (!payload)
		return -ENOMEM;
	/* &payload may be a physical address > 4 GB */
	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] =
		SCM_BUFFER_SIZE(uint32_t) * LMH_SCM_PAYLOAD_SIZE;
	desc_arg.args[2] = (is_read) ?
			LMH_DEBUG_READ_TYPE : LMH_DEBUG_CONFIG_TYPE;
	desc_arg.arginfo = SCM_ARGS(4, SCM_RW, SCM_VAL, SCM_VAL, SCM_VAL);
	ret = lmh_get_recurssive_data(&desc_arg, 3, payload, &size, &dest_buf);
	if (ret)
		goto get_type_exit;
	pr_debug("Total %s types:%d\n", (is_read) ? "read" : "config", size);
	if (is_read) {
		lmh_data->read_type = *buf = dest_buf;
		lmh_data->read_type_count = size;
	} else {
		lmh_data->config_type = *buf = dest_buf;
		lmh_data->config_type_count = size;
	}

get_type_exit:
	if (ret) {
		if (lmh_data->read_type_count) {
			devm_kfree(lmh_data->dev, lmh_data->read_type);
			lmh_data->read_type_count = 0;
		}
		if (lmh_data->config_type_count) {
			devm_kfree(lmh_data->dev, lmh_data->config_type);
			lmh_data->config_type_count = 0;
		}
	}
	if (payload)
		devm_kfree(lmh_data->dev, payload);

	return (ret) ? ret : size;
}

static int lmh_get_types(struct seq_file *seq_fp, enum lmh_read_type type)
{
	int ret = 0, idx = 0, size = 0;
	uint32_t *type_list = NULL;

	switch (type) {
	case LMH_READ_TYPE:
		ret = lmh_debug_get_types(true, &type_list);
		break;
	case LMH_CONFIG_TYPE:
		ret = lmh_debug_get_types(false, &type_list);
		break;
	default:
		return -EINVAL;
	}
	if (ret <= 0 || !type_list) {
		pr_err("No device information. err:%d\n", ret);
		return -ENODEV;
	}
	size = ret;
	for (idx = 0; idx < size; idx++)
		seq_printf(seq_fp, "0x%x ", type_list[idx]);
	seq_puts(seq_fp, "\n");

	return 0;
}

static int lmh_dbgfs_read_type(struct seq_file *seq_fp, void *data)
{
	return lmh_get_types(seq_fp, LMH_READ_TYPE);
}

static int lmh_dbgfs_read_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, lmh_dbgfs_read_type, inode->i_private);
}

static int lmh_dbgfs_config_type(struct seq_file *seq_fp, void *data)
{
	return lmh_get_types(seq_fp, LMH_CONFIG_TYPE);
}

static int lmh_dbgfs_config_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, lmh_dbgfs_config_type, inode->i_private);
}

static const struct file_operations lmh_dbgfs_config_fops = {
	.write = lmh_dbgfs_config_write,
};
static const struct file_operations lmh_dbgfs_read_fops = {
	.open = lmh_dbgfs_data_open,
	.read = seq_read,
	.write = lmh_dbgfs_data_write,
	.llseek = seq_lseek,
	.release = single_release,
};
static const struct file_operations lmh_dbgfs_read_type_fops = {
	.open = lmh_dbgfs_read_type_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static const struct file_operations lmh_dbgfs_config_type_fops = {
	.open = lmh_dbgfs_config_type_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int lmh_check_tz_debug_cmds(void)
{
	if (!scm_is_call_available(SCM_SVC_LMH, LMH_DEBUG_SET)
		|| !scm_is_call_available(SCM_SVC_LMH, LMH_DEBUG_READ_BUF_SIZE)
		|| !scm_is_call_available(SCM_SVC_LMH, LMH_DEBUG_READ)
		|| !scm_is_call_available(SCM_SVC_LMH, LMH_DEBUG_GET_TYPE)) {
		pr_debug("LMH debug scm not available\n");
		return -ENODEV;
	}

	return 0;
}

static int lmh_debug_init(void)
{
	int ret = 0;

	if (lmh_check_tz_debug_cmds()) {
		pr_debug("Debug commands not available\n");
		return -ENODEV;
	}

	lmh_data->debugfs_parent = debugfs_create_dir(LMH_MON_NAME, NULL);
	if (IS_ERR(lmh_data->debugfs_parent)) {
		ret = PTR_ERR(lmh_data->debugfs_parent);
		pr_debug("Error creating debugfs dir:%s. err:%d\n",
					LMH_MON_NAME, ret);
		return ret;
	}
	lmh_data->debug_read = debugfs_create_file(LMH_DBGFS_READ, 0600,
					lmh_data->debugfs_parent, NULL,
					&lmh_dbgfs_read_fops);
	if (IS_ERR(lmh_data->debug_read)) {
		pr_err("Error creating" LMH_DBGFS_READ "entry\n");
		ret = PTR_ERR(lmh_data->debug_read);
		goto dbg_reg_exit;
	}
	lmh_data->debug_config = debugfs_create_file(LMH_DBGFS_CONFIG_READ,
					0200, lmh_data->debugfs_parent, NULL,
					&lmh_dbgfs_config_fops);
	if (IS_ERR(lmh_data->debug_config)) {
		pr_err("Error creating" LMH_DBGFS_CONFIG_READ "entry\n");
		ret = PTR_ERR(lmh_data->debug_config);
		goto dbg_reg_exit;
	}
	lmh_data->debug_read_type = debugfs_create_file(LMH_DBGFS_READ_TYPES,
					0400, lmh_data->debugfs_parent, NULL,
					&lmh_dbgfs_read_type_fops);
	if (IS_ERR(lmh_data->debug_read_type)) {
		pr_err("Error creating" LMH_DBGFS_READ_TYPES "entry\n");
		ret = PTR_ERR(lmh_data->debug_read_type);
		goto dbg_reg_exit;
	}
	lmh_data->debug_read_type = debugfs_create_file(
					LMH_DBGFS_CONFIG_TYPES,
					0400, lmh_data->debugfs_parent, NULL,
					&lmh_dbgfs_config_type_fops);
	if (IS_ERR(lmh_data->debug_config_type)) {
		pr_err("Error creating" LMH_DBGFS_CONFIG_TYPES "entry\n");
		ret = PTR_ERR(lmh_data->debug_config_type);
		goto dbg_reg_exit;
	}

dbg_reg_exit:
	if (ret)
		/*Clean up all the dbg nodes*/
		debugfs_remove_recursive(lmh_data->debugfs_parent);

	return ret;
}

int lmh_debug_register(struct platform_device *pdev)
{
	int ret = 0;

	if (lmh_data) {
		pr_debug("Reinitializing lmh hardware driver\n");
		return -EEXIST;
	}
	lmh_data = devm_kzalloc(&pdev->dev, sizeof(*lmh_data), GFP_KERNEL);
	if (!lmh_data)
		return -ENOMEM;
	lmh_data->dev = &pdev->dev;

	ret = lmh_debug_init();
	if (ret) {
		pr_debug("LMH debug init failed. err:%d\n", ret);
		goto probe_exit;
	}

	return ret;

probe_exit:
	lmh_data = NULL;
	return ret;
}
EXPORT_SYMBOL(lmh_debug_register);
