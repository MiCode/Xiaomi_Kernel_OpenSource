// SPDX-License-Identifier: GPL-2.0
//
// cl_dsp.c -- DSP Control for non-ALSA Cirrus Logic Devices
//
// Copyright 2021 Cirrus Logic, Inc.
//
// Author: Fred Treven <fred.treven@cirrus.com>

#include <linux/firmware/cirrus/cl_dsp.h>

#ifdef CONFIG_DEBUG_FS

static inline u32 host_buffer_field_reg(struct cl_dsp_logger *dl,
					unsigned long offset)
{
	return (u32)(CL_DSP_HALO_XMEM_UNPACKED24_BASE +
		     ((dl->host_buf_ptr + offset) * CL_DSP_BYTES_PER_WORD));
}

static inline u32 host_buffer_data_reg(struct cl_dsp_logger *dl, int offset)
{
	return (u32)(CL_DSP_HALO_XMEM_UNPACKED24_BASE +
		     ((dl->host_buf_base + offset) * CL_DSP_BYTES_PER_WORD));
}

static int cl_dsp_host_buffer_field_read(struct cl_dsp_debugfs *db,
					 unsigned long field_offset, u32 *data)
{
	struct regmap *regmap = db->core->regmap;
	__be32 raw;
	u32 reg;
	int ret;

	reg = host_buffer_field_reg(&db->dl, field_offset);

	ret = regmap_raw_read(regmap, reg, &raw, sizeof(raw));
	if (ret) {
		dev_err(db->core->dev, "Failed to get raw host buffer data\n");
		return ret;
	}

	*data = CL_DSP_HOST_BUFFER_DATA_MASK & be32_to_cpu(raw);
	return 0;
}

static int cl_dsp_host_buffer_field_write(struct cl_dsp_debugfs *db,
					  unsigned long field_offset, u32 data)
{
	struct regmap *regmap = db->core->regmap;
	struct device *dev = db->core->dev;
	int ret;
	u32 reg;

	reg = host_buffer_field_reg(&db->dl, field_offset);

	ret = regmap_write(regmap, reg, data);
	if (ret)
		dev_err(dev, "Failed to set host buffer data: %d\n", ret);

	return ret;
}

static ssize_t cl_dsp_debugfs_logger_en_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct cl_dsp_debugfs *db = file->private_data;
	struct regmap *regmap = db->core->regmap;
	char str[CL_DSP_DEBUGFS_TRACE_LOG_STRING_SIZE];
	u32 reg, val;
	ssize_t ret;

	ret = cl_dsp_get_reg(db->core, "ENABLED", CL_DSP_XM_UNPACKED_TYPE,
			db->dl.algo_id, &reg);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(db->core->dev);
	if (ret < 0) {
		dev_err(db->core->dev, "PM Runtime Resume Failed\n");
		return ret;
	}

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(db->core->dev, "Failed to get host buffer status\n");
		goto pm_exit;
	}

	ret = snprintf(str, CL_DSP_DEBUGFS_TRACE_LOG_STRING_SIZE, "%d\n", val);
	if (ret <= 0) {
		dev_err(db->core->dev, "Failed to parse host buffer status\n");
		goto pm_exit;
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));

pm_exit:
	pm_runtime_mark_last_busy(db->core->dev);
	pm_runtime_put_autosuspend(db->core->dev);

	return ret;
}

static ssize_t cl_dsp_debugfs_logger_en_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct cl_dsp_debugfs *db = file->private_data;
	struct regmap *regmap = db->core->regmap;
	struct device *dev = db->core->dev;
	u32 reg, val;
	ssize_t ret;
	char *str;

	str = kzalloc(count, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	ret = simple_write_to_buffer(str, count, ppos, user_buf, count);
	if (ret <= 0) {
		dev_err(dev, "Failed to write debugfs data\n");
		goto exit_free;
	}

	ret = kstrtou32(str, 10, &val);
	if (ret)
		goto exit_free;

	if (val != CL_DSP_DEBUGFS_TRACE_LOG_DISABLE &&
			val != CL_DSP_DEBUGFS_TRACE_LOG_ENABLE) {
		dev_err(dev, "Invalid trace log write: %u\n", val);
		ret = -EINVAL;
		goto exit_free;
	}

	ret = cl_dsp_get_reg(db->core, "ENABLED", CL_DSP_XM_UNPACKED_TYPE,
			     db->dl.algo_id, &reg);
	if (ret)
		goto exit_free;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(db->core->dev, "PM Runtime Resume Failed\n");
		goto exit_free;
	}

	ret = regmap_write(regmap, reg, val);
	if (ret) {
		dev_err(dev, "Failed to set trace log status\n");
		goto exit_pm;
	}

	if (val == CL_DSP_DEBUGFS_TRACE_LOG_DISABLE) {
		/* Set next_read_index to -1 to reset logger */
		ret = cl_dsp_host_buffer_field_write(db,
				HOST_BUFFER_FIELD(next_read_index),
				CL_DSP_HOST_BUFFER_READ_INDEX_RESET);
		if (ret) {
			dev_err(dev, "Failed to reset event logger\n");
			goto exit_pm;
		}

		db->dl.buf_data_size = 0;
		kfree(db->dl.buf_data);
	}

exit_pm:
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

exit_free:
	kfree(str);

	return ret ? ret : count;
}

static ssize_t cl_dsp_debugfs_timestamp_shift_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct cl_dsp_debugfs *db = file->private_data;
	struct regmap *regmap = db->core->regmap;
	char str[CL_DSP_DEBUGFS_TRACE_LOG_STRING_SIZE];
	u32 reg, val;
	ssize_t ret;

	ret = cl_dsp_get_reg(db->core, "TIMESTAMP_SHIFT",
			CL_DSP_XM_UNPACKED_TYPE, db->dl.algo_id, &reg);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(db->core->dev);
	if (ret < 0) {
		dev_err(db->core->dev, "PM Runtime Resume Failed\n");
		return ret;
	}

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(db->core->dev, "Failed to get timestamp shift\n");
		goto pm_exit;
	}

	ret = snprintf(str, CL_DSP_DEBUGFS_TRACE_LOG_STRING_SIZE, "%d\n", val);
	if (ret <= 0) {
		dev_err(db->core->dev, "Failed to parse host buffer status\n");
		goto pm_exit;
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));

pm_exit:
	pm_runtime_mark_last_busy(db->core->dev);
	pm_runtime_put_autosuspend(db->core->dev);

	return ret;
}

static int cl_dsp_host_buffer_data_read(struct cl_dsp_debugfs *db,
					u32 read_index, u32 num_words)
{
	u32 start_reg, offset = db->dl.buf_data_size;
	struct regmap *regmap = db->core->regmap;
	struct device *dev = db->core->dev;
	size_t new_data_size;
	u32 *new_data;
	int ret;

	start_reg = host_buffer_data_reg(&db->dl, (unsigned long)read_index);

	new_data_size = db->dl.buf_data_size + num_words;
	new_data = krealloc(db->dl.buf_data, new_data_size * sizeof(u32), GFP_KERNEL);
	if (IS_ERR_OR_NULL(new_data)) {
		dev_err(dev, "Failed to re-allocate buffer data space\n");
		return -ENOMEM;
	}
	db->dl.buf_data_size = new_data_size;

	db->dl.buf_data = new_data;

	ret = regmap_bulk_read(regmap, start_reg, db->dl.buf_data + offset,
			num_words);
	if (ret)
		dev_err(dev, "Failed to get host buffer data\n");

	return ret;
}

int cl_dsp_logger_update(struct cl_dsp_debugfs *db)
{
	struct cl_dsp_logger *dl = &db->dl;
	struct device *dev = db->core->dev;
	u32 n_read_index, n_write_index, num_words;
	u32 nirq, irq, error_code;
	int ret;

	/* Check if interrupt was asserted due to an error */
	ret = cl_dsp_host_buffer_field_read(db, HOST_BUFFER_FIELD(error),
			&error_code);
	if (ret)
		return ret;

	if (error_code) {
		if (error_code != CL_DSP_HOST_BUFFER_ERROR_OVERFLOW) {
			dev_err(dev, "Fatal Host Buffer Error with code 0x%X\n",
				error_code);
			return -ENOTRECOVERABLE;
		}
		dev_warn(dev, "Data lost from Host Buffer Overflow\n");
	}

	/* Check if next read index is != -1 in order to continue */
	ret = cl_dsp_host_buffer_field_read(db,
			HOST_BUFFER_FIELD(next_read_index), &n_read_index);
	if (ret)
		return ret;

	if (n_read_index == CL_DSP_HOST_BUFFER_READ_INDEX_RESET) {
		dev_err(dev, "Host Buffer Not Initialized\n");
		return -EPERM;
	}

	ret = cl_dsp_host_buffer_field_read(db, HOST_BUFFER_FIELD(irq_count),
					    &nirq);
	if (ret)
		return ret;

	ret = cl_dsp_host_buffer_field_read(db, HOST_BUFFER_FIELD(irq_ack),
					    &irq);
	if (ret)
		return ret;

	ret = cl_dsp_host_buffer_field_read(
		db, HOST_BUFFER_FIELD(next_write_index), &n_write_index);
	if (ret)
		return ret;

	if (n_write_index < n_read_index)
		num_words = (n_write_index + dl->host_buf_size_words) -
			    n_read_index;
	else
		num_words = n_write_index - n_read_index;

	/* Get all messages in buffer */
	ret = cl_dsp_host_buffer_data_read(db, n_read_index, num_words);
	if (ret)
		return ret;

	/* Set next_read_index to next_write_index */
	ret = cl_dsp_host_buffer_field_write(db,
			HOST_BUFFER_FIELD(next_read_index), n_write_index - 1);
	if (ret)
		return ret;

	/* Reset irq_ack by writing irq_count | 0x1 */
	ret = cl_dsp_host_buffer_field_write(db, HOST_BUFFER_FIELD(irq_ack),
			nirq | CL_DSP_HOST_BUFFER_IRQ_MASK);
	if (ret)
		return ret;

	ret = cl_dsp_host_buffer_field_read(db,
			HOST_BUFFER_FIELD(irq_ack), &irq);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(cl_dsp_logger_update);

static int cl_dsp_debugfs_logger_open(struct inode *inode, struct file *file)
{
	struct cl_dsp_debugfs *db;
	int ret;

	ret = simple_open(inode, file);
	if (ret)
		return ret;

	db = file->private_data;

	return 0;
}

static ssize_t cl_dsp_debugfs_logger_read(struct file *file,
					  char __user *user_buf, size_t count,
					  loff_t *ppos)
{
	struct cl_dsp_debugfs *db = file->private_data;
	struct cl_dsp_logger *dl = &db->dl;
	struct device *dev = db->core->dev;
	ssize_t ret, buf_str_size;
	char *str, *buf_str;
	int i;

	if (dl->buf_data_size == 0)
		return -ENODATA;

	buf_str_size =
		CL_DSP_HOST_BUFFER_DATA_SLOT_SIZE * dl->buf_data_size;
	buf_str = kzalloc(buf_str_size, GFP_KERNEL);
	if (!buf_str)
		return -ENOMEM;

	str = kzalloc(CL_DSP_HOST_BUFFER_DATA_SLOT_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		goto err_free2;
	}

	for (i = 0; i < dl->buf_data_size; i++) {
		ret = snprintf(str, CL_DSP_HOST_BUFFER_DATA_SLOT_SIZE, "%08X ",
			       dl->buf_data[i]);
		if (ret <= 0) {
			dev_err(dev, "Failed to get host buffer data string\n");
			goto err_free1;
		}

		strncat(buf_str, str, CL_DSP_HOST_BUFFER_DATA_SLOT_SIZE);
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buf_str,
				      strlen(buf_str));

err_free1:
	kfree(str);
err_free2:
	kfree(buf_str);

	return ret;
}

static const struct {
	const char *name;
	const struct file_operations fops;
} cl_dsp_debugfs_fops[] = {
	{
		.name = "log_data",
		.fops = {
			.owner		= THIS_MODULE,
			.open		= cl_dsp_debugfs_logger_open,
			.read		= cl_dsp_debugfs_logger_read,
		},
	},
	{
		.name = "logger_en",
		.fops = {
			.owner		= THIS_MODULE,
			.open		= simple_open,
			.read		= cl_dsp_debugfs_logger_en_read,
			.write		= cl_dsp_debugfs_logger_en_write,
		},
	},
	{
		.name = "timestamp_shift",
		.fops = {
			.owner		= THIS_MODULE,
			.open		= simple_open,
			.read		= cl_dsp_debugfs_timestamp_shift_read,
		}
	},
};

static int cl_dsp_logger_init(struct cl_dsp_debugfs *db)
{
	struct regmap *regmap = db->core->regmap;
	struct cl_dsp *dsp = db->core;
	u32 reg;
	int ret;

	ret = cl_dsp_get_reg(dsp, "EVENT_LOG_HEADER", CL_DSP_XM_UNPACKED_TYPE,
			     db->dl.algo_id, &reg);
	if (ret)
		return ret;

	ret = regmap_read(regmap, reg, &db->dl.host_buf_ptr);
	if (ret) {
		dev_err(db->core->dev, "Failed to get host buffer address\n");
		return ret;
	}

	ret = cl_dsp_host_buffer_field_read(db, HOST_BUFFER_FIELD(buf1_base),
			&db->dl.host_buf_base);
	if (ret)
		return ret;

	ret = cl_dsp_host_buffer_field_read(db,
			HOST_BUFFER_FIELD(buf_total_size),
			&db->dl.host_buf_size_words);
	if (ret)
		return ret;

	ret = cl_dsp_host_buffer_field_read(db,
			HOST_BUFFER_FIELD(high_water_mark),
			&db->dl.high_watermark);
	if (ret)
		return ret;

	/* Set next_read_index to -1 to reset logger */
	ret = cl_dsp_host_buffer_field_write(db,
			HOST_BUFFER_FIELD(next_read_index),
			CL_DSP_HOST_BUFFER_READ_INDEX_RESET);
	if (ret)
		dev_err(db->core->dev, "Failed to reset event logger\n");

	return ret;
}
struct cl_dsp_debugfs *cl_dsp_debugfs_create(struct cl_dsp *dsp,
					     struct dentry *parent_node,
					     u32 event_log_algo_id)
{
	struct cl_dsp_debugfs *db;
	int ret, i;

	if (!dsp || !parent_node)
		return NULL;

	db = kzalloc(sizeof(*db), GFP_KERNEL);
	if (!db)
		return ERR_PTR(-ENOMEM);

	db->core = dsp;
	db->debugfs_root = parent_node ? parent_node : NULL;

	db->debugfs_node = debugfs_create_dir("cl_dsp", db->debugfs_root);
	if (IS_ERR(db->debugfs_node)) {
		ret = PTR_ERR(db->debugfs_node);
		kfree(db);
		return ERR_PTR(ret);
	}

	for (i = 0; i < CL_DSP_DEBUGFS_NUM_CONTROLS; i++)
		debugfs_create_file(cl_dsp_debugfs_fops[i].name,
			CL_DSP_DEBUGFS_RW_FILE_MODE, db->debugfs_node,
			db, &cl_dsp_debugfs_fops[i].fops);

	db->dl.algo_id = event_log_algo_id;

	ret = cl_dsp_logger_init(db);
	if (ret)
		return ERR_PTR(ret);

	debugfs_create_u32("high_watermark", CL_DSP_DEBUGFS_RO_FILE_MODE,
			   db->debugfs_node, &db->dl.high_watermark);

	return db;
}
EXPORT_SYMBOL_GPL(cl_dsp_debugfs_create);

void cl_dsp_debugfs_destroy(struct cl_dsp_debugfs *db)
{
	if (IS_ERR_OR_NULL(db))
		return;

	debugfs_remove_recursive(db->debugfs_node);
	kfree(db);
}
EXPORT_SYMBOL_GPL(cl_dsp_debugfs_destroy);

#endif /* CONFIG_DEBUG_FS */

MODULE_DESCRIPTION("CL DSP Debugfs Driver");
MODULE_AUTHOR("Fred Treven, Cirrus Logic Inc. <fred.treven@cirrus.com>");
MODULE_LICENSE("GPL");
