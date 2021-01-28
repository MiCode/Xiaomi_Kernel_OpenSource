// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <memory/mediatek/emi.h>

static struct platform_device *emiisu_pdev;

static int emiisu_probe(struct platform_device *pdev);

static ssize_t emiisu_ctrl_show(struct device_driver *driver, char *buf)
{
	struct emiisu_dev_t *emiisu_dev_ptr;
	unsigned int state;

	if (!emiisu_pdev)
		return 0;

	emiisu_dev_ptr =
		(struct emiisu_dev_t *)platform_get_drvdata(emiisu_pdev);

	if (!(emiisu_dev_ptr->con_addr))
		return 0;

	state = readl(emiisu_dev_ptr->con_addr);
	return snprintf(buf, PAGE_SIZE, "isu_state: 0x%x\n", state);
}

static ssize_t emiisu_ctrl_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	struct emiisu_dev_t *emiisu_dev_ptr;
	unsigned long state;
	char *command;
	char *backup_command;
	char *ptr;
	char *token[MTK_EMI_MAX_TOKEN];
	int i;

	if (!emiisu_pdev)
		return count;

	emiisu_dev_ptr =
		(struct emiisu_dev_t *)platform_get_drvdata(emiisu_pdev);

	if (!(emiisu_dev_ptr->ctrl_intf))
		return count;

	if (!(emiisu_dev_ptr->con_addr))
		return count;

	if ((strlen(buf) + 1) > MTK_EMI_MAX_CMD_LEN) {
		pr_info("%s: store command overflow\n", __func__);
		return count;
	}

	command = kmalloc((size_t)MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;
	backup_command = command;
	if (!command)
		return count;
	strncpy(command, buf, (size_t)MTK_EMI_MAX_CMD_LEN);

	for (i = 0; i < MTK_EMI_MAX_TOKEN; i++) {
		ptr = strsep(&command, " ");
		if (!ptr)
			break;
		token[i] = ptr;
	}

	if (i < 1)
		goto emiisu_ctrl_store_end;

	if (kstrtoul(token[0], 16, &state) == 0)
		writel((unsigned int)state, emiisu_dev_ptr->con_addr);

emiisu_ctrl_store_end:
	kfree(backup_command);

	return count;
}

static DRIVER_ATTR_RW(emiisu_ctrl);

static ssize_t dump_buf_read
	(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	struct emiisu_dev_t *emiisu_dev_ptr;
	ssize_t bytes;
	ssize_t header_bytes = 0;

	if (!emiisu_pdev)
		return 0;

	emiisu_dev_ptr =
		(struct emiisu_dev_t *)platform_get_drvdata(emiisu_pdev);
	bytes = len < (emiisu_dev_ptr->buf_size - *ppos) ?
		len : (emiisu_dev_ptr->buf_size - *ppos);

	if (!(emiisu_dev_ptr->buf_addr))
		return 0;

	if (!(emiisu_dev_ptr->ver_addr))
		return 0;

	if (*ppos == 0) {
		if (copy_to_user(data, (char *)(emiisu_dev_ptr->ver_addr), 4)
			|| (bytes < 4))
			return -EFAULT;

		data += 4;
		header_bytes += 4;
		bytes -= 4;
	}

	if (bytes) {
		if (copy_to_user(data, (char *)(emiisu_dev_ptr->buf_addr) +
			*ppos, bytes))
			return -EFAULT;
	}

	*ppos += bytes;

	return (bytes + header_bytes);
}

static int dump_buf_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static const struct file_operations dump_buf_fops = {
	.owner = THIS_MODULE,
	.read = dump_buf_read,
	.open = dump_buf_open
};

static int emiisu_remove(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id emiisu_of_ids[] = {
	{.compatible = "mediatek,common-emiisu",},
	{}
};

static struct platform_driver emiisu_drv = {
	.probe = emiisu_probe,
	.remove = emiisu_remove,
	.driver = {
		.name = "emiisu_drv",
		.owner = THIS_MODULE,
		.of_match_table = emiisu_of_ids,
	},
};

static int emiisu_probe(struct platform_device *pdev)
{
	struct device_node *emiisu_node = pdev->dev.of_node;
	struct emiisu_dev_t *emiisu_dev_ptr;
	unsigned long long addr_temp;
	int ret;

	pr_info("%s: module probe.\n", __func__);
	emiisu_pdev = pdev;
	emiisu_dev_ptr = devm_kmalloc(&pdev->dev,
		sizeof(struct emiisu_dev_t), GFP_KERNEL);
	if (!emiisu_dev_ptr)
		return -ENOMEM;

	ret = of_property_read_u32(emiisu_node,
		"buf_size", &(emiisu_dev_ptr->buf_size));
	if (ret) {
		pr_info("%s: get buf_size fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u64(emiisu_node, "buf_addr", &addr_temp);
	if (ret) {
		pr_info("%s: get buf_addr fail\n", __func__);
		return -EINVAL;
	}
	if (addr_temp)
		emiisu_dev_ptr->buf_addr = ioremap_wc(
			(phys_addr_t)addr_temp, emiisu_dev_ptr->buf_size);
	else
		emiisu_dev_ptr->buf_addr = NULL;

	ret = of_property_read_u64(emiisu_node, "ver_addr", &addr_temp);
	if (ret) {
		pr_info("%s: get ver_addr fail\n", __func__);
		return -EINVAL;
	}
	if (addr_temp)
		emiisu_dev_ptr->ver_addr = ioremap((phys_addr_t)addr_temp, 4);
	else
		emiisu_dev_ptr->ver_addr = NULL;

	ret = of_property_read_u64(emiisu_node, "con_addr", &addr_temp);
	if (ret) {
		pr_info("%s: get con_addr fail\n", __func__);
		return -EINVAL;
	}
	if (addr_temp)
		emiisu_dev_ptr->con_addr = ioremap((phys_addr_t)addr_temp, 4);
	else
		emiisu_dev_ptr->con_addr = NULL;

	ret = of_property_read_u32(emiisu_node,
		"ctrl_intf", &(emiisu_dev_ptr->ctrl_intf));
	if (ret) {
		pr_info("%s: get ctrl_intf fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: %s(%x),%s(%lx),%s(%lx),%s(%lx),%s(%d)\n", __func__,
		"buf_size", emiisu_dev_ptr->buf_size,
		"buf_addr", (unsigned long)(emiisu_dev_ptr->buf_addr),
		"ver_addr", (unsigned long)(emiisu_dev_ptr->ver_addr),
		"con_addr", (unsigned long)(emiisu_dev_ptr->con_addr),
		"ctrl_intf", emiisu_dev_ptr->ctrl_intf);

	emiisu_dev_ptr->dump_dir = debugfs_create_dir("emiisu", NULL);
	if (!emiisu_dev_ptr->dump_dir) {
		pr_info("%s: fail to create dump_dir\n", __func__);
		return -EINVAL;
	}

	emiisu_dev_ptr->dump_buf = debugfs_create_file("dump_buf", 0444,
		emiisu_dev_ptr->dump_dir, NULL, &dump_buf_fops);
	if (!emiisu_dev_ptr->dump_buf) {
		pr_info("%s: fail to create dump_buf\n", __func__);
		return -EINVAL;
	}

	platform_set_drvdata(pdev, emiisu_dev_ptr);

	ret = driver_create_file(&emiisu_drv.driver,
		&driver_attr_emiisu_ctrl);
	if (ret)
		pr_info("%s: fail to create emiisu_ctrl\n", __func__);

	return ret;
}

static int __init emiisu_drv_init(void)
{
	int ret;

	ret = platform_driver_register(&emiisu_drv);
	if (ret) {
		pr_info("%s: init fail, ret 0x%x\n", __func__, ret);
		return ret;
	}

	return ret;
}

static void __exit emiisu_drv_exit(void)
{
	platform_driver_unregister(&emiisu_drv);
}

module_init(emiisu_drv_init);
module_exit(emiisu_drv_exit);

MODULE_DESCRIPTION("MediaTek EMIISU Driver v0.1");

