// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/ipc_logging.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/scmi_protocol.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/debugfs.h>

#define MAX_PRINT_SIZE		1024
#define MAX_BUF_NUM		4
#define MAX_RESIDUAL_SIZE	MAX_PRINT_SIZE
#define SIZE_ADJUST		4
#define SRC_OFFSET		4
#define CREATE_TRACE_POINTS
#include "trace_cpucp.h"
#if IS_ENABLED(CONFIG_QTI_SCMI_VENDOR_PROTOCOL)
#define CPUCP_CTRL_ALGO_STR (0x435055435043544c) /* CPUCPCTL ASCII */
#define LOG_NAME_MAX_LENGTH 32
static struct scmi_protocol_handle *ph;
static const struct qcom_scmi_vendor_ops *ops;

enum cpucp_ctrl_param_ids {
	LOGBUF_IDX = 1,
	DDR_LOGBUF_FLUSH = 3,
	MODULE_LOG_LEVEL = 5,
};

struct __packed scmi_module_log_level_t {
	uint32_t module_id;
	uint32_t log_level;
};

struct __packed scmi_module_log_data_t {
	uint32_t module_id;
	char module_name[LOG_NAME_MAX_LENGTH];
	uint32_t remaining_modules;
};
#endif

enum cpucp_log_type {
	CPUCP,
	PDP0,
	PDP1,
	NUM_CPUCP_LOG_TYPES
};

static const char * const cpucp_log_names[NUM_CPUCP_LOG_TYPES] = {
	[CPUCP]		= "cpucp",
	[PDP0]		= "pdp0",
	[PDP1]		= "pdp1",
};

struct remote_mem {
	void __iomem *start;
	u32 size;
	u32 phys_addr;
};

struct cpucp_buf {
	struct list_head node;
	char *buf;
	u32 size;
	u32 cpy_idx;
};

struct cpucp_log_info {
	struct remote_mem *rmem;
	struct mbox_client cl;
	struct mbox_chan *ch;
	struct mbox_chan *tx_ch;
	struct delayed_work work;
	struct device *dev;
	enum cpucp_log_type type;
	u32 enabled;
	void __iomem *base;
	unsigned int rmem_idx;
	unsigned int num_bufs;
	unsigned int total_buf_size;
	char *rem_buf;
	char *glb_buf;
	int  rem_len;
	struct list_head free_buffers_list;
	struct list_head full_buffers_list;
	spinlock_t free_list_lock;
	spinlock_t full_list_lock;
};

static struct workqueue_struct *cpucp_wq;
static struct dentry *pdp_log_dir;

#if IS_ENABLED(CONFIG_QTI_SCMI_VENDOR_PROTOCOL)
static int flush_cpucp_log(void *data, u64 val)
{
	int ret;

	ret =  ops->set_param(ph, &val, CPUCP_CTRL_ALGO_STR,
			DDR_LOGBUF_FLUSH, sizeof(val));
	if (ret < 0) {
		pr_err("failed to flush cpucp log, ret = %d\n", ret);
		return ret;
	}
	return 0;
}

static int set_log_level(void *data, u64 val)
{
	int ret;

	ret =  ops->set_param(ph, &val, CPUCP_CTRL_ALGO_STR,
			LOGBUF_IDX, sizeof(val));
	if (ret < 0) {
		pr_err("failed to set log level, ret = %d\n", ret);
		return ret;
	}
	return 0;
}

static int get_log_level(void *data, u64 *val)
{
	u64 log_level = 1;
	int ret;

	ret =  ops->get_param(ph, &log_level, CPUCP_CTRL_ALGO_STR,
			LOGBUF_IDX, 0, sizeof(log_level));
	if (ret < 0) {
		pr_err("failed to get log level, ret = %d\n", ret);
		return ret;
	}
	*val = log_level;
	return 0;
}

static ssize_t get_module_ids(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	int ret;
	struct scmi_module_log_data_t  rx_value;
	ssize_t r = 0, bytes = 0;
	char *kern_buf;
	u32 module_id = 0;

	module_id = 0;
	kern_buf = kzalloc(2048, GFP_KERNEL);
	if (!kern_buf)
		return -ENOMEM;
	bytes = scnprintf(kern_buf, 30, "Module ID Module Name\n");

	do {
		rx_value.module_id = module_id;
		ret =  ops->get_param(ph, &rx_value, CPUCP_CTRL_ALGO_STR,
				MODULE_LOG_LEVEL, sizeof(module_id),
				sizeof(struct scmi_module_log_data_t));
		if (!ret) {
			bytes += scnprintf(kern_buf + bytes, 2048 - bytes, "%9d %s\n",
					 rx_value.module_id,
					 rx_value.module_name);
			module_id++;
		} else
			pr_err("Failed to get supported module list %d\n", ret);

	} while ((rx_value.remaining_modules != 0) && (!ret));

	r = simple_read_from_buffer(user_buf, count, ppos, kern_buf, bytes);
	kfree(kern_buf);
	return r;
}

static ssize_t set_module_log_level(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct scmi_module_log_level_t log_cfg;
	void *kern_buf;
	int ret;

	/* Copy the user space buf */
	kern_buf = memdup_user(user_buf, count);
	if (IS_ERR(kern_buf))
		return PTR_ERR(kern_buf);
	ret = sscanf(kern_buf, "%u %u", &log_cfg.module_id, &log_cfg.log_level);
	if (ret < 0)
		return ret;
	ret =  ops->set_param(ph, &log_cfg, CPUCP_CTRL_ALGO_STR,
			MODULE_LOG_LEVEL, sizeof(log_cfg));
	if (ret < 0) {
		pr_err("failed to set module log level, ret = %d\n", ret);
		kfree(kern_buf);
		return ret;
	}
	kfree(kern_buf);
	return count;
}

DEFINE_DEBUGFS_ATTRIBUTE(log_level_ops, get_log_level, set_log_level, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(flush_log_ops, NULL, flush_cpucp_log, "%llu\n");

static const struct file_operations avbl_module_ops = {
	.read = get_module_ids,
	.open = simple_open,
	.llseek = default_llseek,
};

static const struct file_operations set_module_log_level_fops = {
	.write = set_module_log_level,
};
#endif

static int set_log_enabled(void *data, u64 val)
{
	struct cpucp_log_info *info = data;
	int ret;
	u32 enable = val ? 1 : 0;

	ret = mbox_send_message(info->tx_ch, &enable);
	if (ret < 0) {
		pr_err("failed to set log enabled, ret = %d\n", ret);
		return ret;
	}
	mbox_client_txdone(info->tx_ch, 0);
	info->enabled = enable;
	return 0;
}

static int get_log_enabled(void *data, u64 *val)
{
	struct cpucp_log_info *info = data;
	*val = info->enabled;
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(log_enabled_ops, get_log_enabled, set_log_enabled, "%llu\n");

static inline bool get_last_newline(char *buf, int size, int *cnt)
{
	int i;

	for (i = (size - 1); i >= 0 ; i--) {
		if (buf[i] == '\n') {
			buf[i] = '\0';
			*cnt = i + 1;
			return true;
		}
	}

	*cnt = size;
	return false;
}

#if IS_ENABLED(CONFIG_QTI_SCMI_VENDOR_PROTOCOL)
static int scmi_cpucp_log_create_fs_entries(struct cpucp_log_info *info)
{
	struct dentry *ret, *dir;

	dir = debugfs_create_dir("cpucp_log", 0);
	if (IS_ERR(dir)) {
		dev_err(info->dev, "Debugfs cpucp directory creation failed\n");
		return -ENOENT;
	}
	ret = debugfs_create_file("log_level", 0644, dir, NULL, &log_level_ops);
	if (IS_ERR(ret)) {
		dev_err(info->dev, "Debugfs log_level file creation failed\n");
		return -ENOENT;
	}
	ret = debugfs_create_file("flush_log", 0200, dir, NULL, &flush_log_ops);
	if (IS_ERR(ret)) {
		dev_err(info->dev, "Debugfs flush_log file creation failed\n");
		return -ENOENT;
	}

	ret = debugfs_create_file("module_log_level", 0200, dir, NULL, &set_module_log_level_fops);
	if (IS_ERR(ret)) {
		dev_err(info->dev, "Debugfs directory creation failed for set_module_log_level\n");
		return -ENOENT;
	}
	ret = debugfs_create_file("available_modules", 0400, dir, NULL,
				  &avbl_module_ops);
	if (IS_ERR(ret)) {
		pr_err("Debugfs directory creation for available_modules failed\n");
		return PTR_ERR(ret);
	}
	return 0;
}
#endif

static int pdp_log_create_fs_entries(struct cpucp_log_info *info)
{
	struct dentry *ret, *dir;

	if (!pdp_log_dir) {
		pdp_log_dir = debugfs_create_dir("pdp_log", 0);
		if (IS_ERR(pdp_log_dir)) {
			dev_err(info->dev, "Debugfs pdp directory creation failed\n");
			return -ENOENT;
		}
	}

	dir = debugfs_create_dir(cpucp_log_names[info->type], pdp_log_dir);
	if (IS_ERR(dir)) {
		dev_err(info->dev, "Debugfs pdp directory creation failed\n");
		return -ENOENT;
	}
	ret = debugfs_create_file("log_enabled", 0644, dir, info, &log_enabled_ops);
	if (IS_ERR(ret)) {
		dev_err(info->dev, "Debugfs log_enabled file creation failed\n");
		return -ENOENT;
	}

	return 0;
}

static inline void print_to_trace(enum cpucp_log_type type, char *str)
{
	switch (type) {
	case CPUCP:
		trace_cpucp_log(str);
		break;
	case PDP0:
		trace_pdp0_log(str);
		break;
	case PDP1:
		trace_pdp1_log(str);
		break;
	default:
		break;
	}
}

static void cpucp_log_work(struct work_struct *work)
{
	struct cpucp_log_info *info = container_of(work,
						struct cpucp_log_info,
						work.work);
	char *src;
	int buf_start = 0;
	int cnt = 0, print_size = 0, buf_size = 0;
	bool ret;
	char tmp_buf[MAX_PRINT_SIZE + 1];
	struct cpucp_buf *buf_node;
	unsigned long flags;

	while (1) {
		spin_lock_irqsave(&info->full_list_lock, flags);
		if (list_empty(&info->full_buffers_list)) {
			spin_unlock_irqrestore(&info->full_list_lock, flags);
			return;
		}
		buf_node = list_first_entry(&info->full_buffers_list,
					struct cpucp_buf, node);
		list_del(&buf_node->node);
		spin_unlock_irqrestore(&info->full_list_lock, flags);
		buf_start = buf_node->cpy_idx - info->rem_len;
		src = &buf_node->buf[buf_start];
		buf_size = buf_node->size + info->rem_len;
		if (info->rem_len) {
			memcpy(&buf_node->buf[buf_start],
					info->rem_buf, info->rem_len);
			info->rem_len = 0;
		}
		do {
			print_size = (buf_size >= MAX_PRINT_SIZE) ?
						MAX_PRINT_SIZE : buf_size;
			ret = get_last_newline(src, print_size, &cnt);
			if (cnt == print_size) {
				if (!ret && buf_size < MAX_PRINT_SIZE) {
					info->rem_len = buf_size;
					memcpy(info->rem_buf, src, buf_size);
					goto out;
				} else {
					snprintf(tmp_buf, print_size + 1, "%s", src);
					print_to_trace(info->type, tmp_buf);
				}
			} else
				print_to_trace(info->type, src);

			buf_start += cnt;
			buf_size -= cnt;
			src = &buf_node->buf[buf_start];
		} while (buf_size > 0);

out:
		spin_lock_irqsave(&info->free_list_lock, flags);
		list_add_tail(&buf_node->node, &info->free_buffers_list);
		spin_unlock_irqrestore(&info->free_list_lock, flags);
	}
}

static struct cpucp_buf *get_free_buffer(struct cpucp_log_info *info)
{
	struct cpucp_buf *buf_node;
	unsigned long flags;

	spin_lock_irqsave(&info->free_list_lock, flags);
	if (list_empty(&info->free_buffers_list)) {
		spin_unlock_irqrestore(&info->free_list_lock, flags);
		return NULL;
	}

	buf_node = list_first_entry(&info->free_buffers_list,
					struct cpucp_buf, node);
	list_del(&buf_node->node);
	spin_unlock_irqrestore(&info->free_list_lock, flags);
	return buf_node;
}

static void cpucp_log_rx(struct mbox_client *client, void *msg)
{
	struct cpucp_log_info *info = dev_get_drvdata(client->dev);
	struct device *dev = info->dev;
	struct cpucp_buf *buf_node;
	struct remote_mem *rmem;
	void __iomem *src;
	u32 marker;
	u32 rmem_size;
	unsigned long flags;
	int src_offset = 0;
	int size_adj = 0;
	u64 *data = (u64 *)msg;
	u32 buf_start;
	u32 cap_size;
	int i;

	buf_node = get_free_buffer(info);
	if (!buf_node) {
		dev_err(dev, "global buffer full dropping buffers\n");
		return;
	}

	switch (info->type) {
	case CPUCP:
		marker = *(u32 *)(info->rmem)->start;
		if (marker <= info->rmem->size) {
			info->rmem_idx = 0;
			rmem_size = marker;
		} else if (marker <= info->total_buf_size) {
			info->rmem_idx = 1;
			rmem_size = marker - info->rmem->size;
		} else {
			pr_err("%s: Log marker incorrect: %u\n", __func__, marker);
			return;
		}

		if (info->rmem_idx == 0) {
			size_adj = SIZE_ADJUST;
			src_offset = SRC_OFFSET;
		}
		rmem = info->rmem + info->rmem_idx;
		rmem_size -= size_adj;
		src = rmem->start + src_offset;
		break;
	case PDP0:
	case PDP1:
		if (!data) {
			dev_err(dev, "pdp_log error: mbox data is NULL\n");
			return;
		}
		*data >>= 32;
		buf_start = FIELD_GET(GENMASK(23, 0), *data) << 8;
		cap_size = (FIELD_GET(GENMASK(31, 24), *data) + 1) << 8;
		for (i = 0; i < info->num_bufs; i++)  {
			if (buf_start == info->rmem[i].phys_addr)
				break;
		}
		if (i >= info->num_bufs) {
			dev_err(dev, "pdp_log error: couldn't match buf_start addr\n");
			return;
		}

		src = info->rmem[i].start;
		rmem_size = min(cap_size, info->rmem->size);
		break;
	default:
		return;
	}

	memcpy_fromio(&buf_node->buf[buf_node->cpy_idx], src, rmem_size);
	buf_node->size = rmem_size;
	spin_lock_irqsave(&info->full_list_lock, flags);
	list_add_tail(&buf_node->node, &info->full_buffers_list);
	spin_unlock_irqrestore(&info->full_list_lock, flags);

	if (!delayed_work_pending(&info->work))
		queue_delayed_work(cpucp_wq, &info->work, 0);
}

static int populate_free_buffers(struct platform_device *pdev,
				struct cpucp_log_info *info)
{
	struct device *dev = &pdev->dev;
	struct cpucp_buf *buf_nodes;
	int i = 0, prev_size = 0;
	struct remote_mem *rmem;
	void __iomem *mem_base;
	struct resource *res;

	rmem = kcalloc(pdev->num_resources, sizeof(struct remote_mem),
			GFP_KERNEL);
	if (!rmem)
		return -ENOMEM;

	info->rmem = rmem;

	for (i = 0; i < pdev->num_resources; i++) {
		struct remote_mem *rmem = &info->rmem[i];

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(dev,
				"Failed to get the device base address\n");
			return -ENODEV;
		}

		mem_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
		if (!mem_base) {
			dev_err(dev, "Failed to ioremap region\n");
			return -ENOMEM;
		}
		rmem->phys_addr = (u32)(res->start);
		rmem->start = mem_base;
		rmem->size = resource_size(res);
		if (prev_size && (rmem->size != prev_size))
			return -EINVAL;
		else if (!prev_size)
			prev_size = rmem->size;

		info->total_buf_size += rmem->size;
		info->num_bufs++;
	}
	info->glb_buf = devm_kzalloc(dev, MAX_BUF_NUM *
					(rmem->size + MAX_PRINT_SIZE),
					GFP_KERNEL);
	if (!info->glb_buf)
		return -ENOMEM;

	info->rem_buf = devm_kzalloc(dev, MAX_RESIDUAL_SIZE, GFP_KERNEL);
	if (!info->rem_buf)
		return -ENOMEM;

	INIT_LIST_HEAD(&info->free_buffers_list);
	INIT_LIST_HEAD(&info->full_buffers_list);

	buf_nodes = devm_kzalloc(info->dev,
				MAX_BUF_NUM * sizeof(struct cpucp_buf),
				GFP_KERNEL);
	if (!buf_nodes)
		return -ENOMEM;

	for (i = 0; i < MAX_BUF_NUM; i++) {
		buf_nodes[i].buf = &info->glb_buf[i * (rmem->size + MAX_PRINT_SIZE)];
		buf_nodes[i].size = rmem->size;
		buf_nodes[i].cpy_idx = MAX_PRINT_SIZE;
		list_add_tail(&buf_nodes[i].node, &info->free_buffers_list);
	}

	return 0;
}

static int cpucp_log_probe(struct platform_device *pdev)
{
	enum cpucp_log_type log_type = NUM_CPUCP_LOG_TYPES;
	struct device *dev = &pdev->dev;
	struct cpucp_log_info *info;
#if IS_ENABLED(CONFIG_QTI_SCMI_VENDOR_PROTOCOL)
	struct scmi_device *sdev;
#endif
	struct mbox_client *cl;
	int num_chans;
	int ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	ret = of_property_read_u32(dev->of_node, "qcom,log-type", &log_type);
	if (log_type >= NUM_CPUCP_LOG_TYPES || ret < 0) {
		dev_err(dev, "Invalid log type:%u, ret:%d\n", log_type, ret);
		return -ENODEV;
	}
	info->type = log_type;
	info->dev = dev;

#if IS_ENABLED(CONFIG_QTI_SCMI_VENDOR_PROTOCOL)
	if (ops || info->type != CPUCP)
		goto scmi_out;

	sdev = get_qcom_scmi_device();
	if (IS_ERR(sdev)) {
		ret = PTR_ERR(sdev);
		return dev_err_probe(dev, ret, "Error getting scmi_dev ret=%d\n", ret);
	}
	ops = sdev->handle->devm_protocol_get(sdev, QCOM_SCMI_VENDOR_PROTOCOL, &ph);
	if (IS_ERR(ops)) {
		ret = PTR_ERR(ops);
		ops = NULL;
		dev_err(dev, "Error getting vendor protocol ops: %d\n", ret);
	}
scmi_out:
	if (info->type == CPUCP && ops) {
		if (scmi_cpucp_log_create_fs_entries(info)) {
			dev_err(dev, "Failed to create debugfs entries\n");
			return -ENOENT;
		}
	}
#endif
	if (info->type == PDP0 || info->type == PDP1) {
		if (pdp_log_create_fs_entries(info)) {
			dev_err(dev, "Failed to create pdp debugfs entries\n");
			return -ENOENT;
		}
	}

	cl = &info->cl;
	cl->dev = dev;
	cl->tx_block = false;
	cl->knows_txdone = true;
	cl->rx_callback = cpucp_log_rx;

	dev_set_drvdata(dev, info);
	INIT_DEFERRABLE_WORK(&info->work, &cpucp_log_work);
	spin_lock_init(&info->free_list_lock);
	spin_lock_init(&info->full_list_lock);
	if (!cpucp_wq)
		cpucp_wq = create_freezable_workqueue("cpucp_wq");

	info->ch = mbox_request_channel(cl, 0);
	if (IS_ERR(info->ch)) {
		ret = PTR_ERR(info->ch);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request mbox info: %d\n", ret);
		return ret;
	}
	num_chans = of_count_phandle_with_args(dev->of_node, "mboxes", "#mbox-cells");
	if (num_chans >= 2) {
		if (num_chans > 2)
			dev_warn(dev, "Found %d mboxes which is not expected\n", num_chans);
		info->tx_ch = mbox_request_channel(cl, 1);
		if (IS_ERR(info->tx_ch)) {
			ret = PTR_ERR(info->tx_ch);
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "Failed to request tx mbox info: %d\n", ret);
			return ret;
		}

	} else
		info->tx_ch = info->ch;

	ret = populate_free_buffers(pdev, info);
	if (ret < 0) {
		kfree(info->rmem);
		return ret;
	}

	dev_dbg(dev, "CPUCP logging initialized\n");

	return 0;
}

static int cpucp_log_remove(struct platform_device *pdev)
{
	struct cpucp_log_info *info;
	bool tx_chan;

	info = dev_get_drvdata(&pdev->dev);
	tx_chan = (info->ch != info->tx_ch) ? true : false;

	kfree(info->rmem);
	info->rmem = NULL;

	mbox_free_channel(info->ch);
	if (tx_chan)
		mbox_free_channel(info->tx_ch);

	return 0;
}

static const struct of_device_id cpucp_log[] = {
	{ .compatible = "qcom,cpucp-log" },
	{ .compatible = "qcom,pdp-log" },
	{},
};

static struct platform_driver cpucp_log_driver = {
	.driver = {
		.name = "cpucp-log",
		.of_match_table = cpucp_log,
	},
	.probe = cpucp_log_probe,
	.remove = cpucp_log_remove,
};
builtin_platform_driver(cpucp_log_driver);

MODULE_LICENSE("GPL");
