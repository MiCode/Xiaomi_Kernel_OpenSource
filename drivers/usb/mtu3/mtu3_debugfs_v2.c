/*
 * mtu3_gadget.c - MediaTek usb3 DRD peripheral support
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>

#include "mtu3.h"
#include "mtu3_hal.h"


static u32 mac_value, mac_addr;
static u32 ippc_value, ippc_addr;

static int smt_err_count_get(void *data, u64 *val)
{
	struct ssusb_mtk *ssusb = data;

	*val = ssusb_u3loop_back_test(ssusb);

	mtu3_printk(K_INFO, "smt_err_count_get %llu\n", *val);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(smt_err_count, smt_err_count_get, NULL, "%llu\n");

static void mac_write32(struct ssusb_mtk *ssusb, int offset,
	int mask, int val)
{
	u32 cur_value;
	u32 new_value;
	struct resource *res;
	struct mtu3 *mtu = ssusb->u3d;
	struct platform_device *pdev = to_platform_device(ssusb->dev);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mac");
	if (offset >= resource_size(res)) {
		pr_info("%s error range\n", __func__);
		return;
	}
	cur_value = mtu3_readl(mtu->mac_base, offset);
	new_value = (cur_value & (~mask)) | val;
	mtu3_writel(mtu->mac_base, offset, new_value);
}

static void ippc_write32(struct ssusb_mtk *ssusb, int offset,
	int mask, int val)
{
	u32 cur_value;
	u32 new_value;
	struct resource *res;
	struct mtu3 *mtu = ssusb->u3d;
	struct platform_device *pdev = to_platform_device(ssusb->dev);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ippc");
	if (offset >= resource_size(res)) {
		pr_info("%s error range\n", __func__);
		return;
	}

	cur_value = mtu3_readl(mtu->ippc_base, offset);
	new_value = (cur_value & (~mask)) | val;
	mtu3_writel(mtu->ippc_base, offset, new_value);
}

static int mac_rw_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "MAC: [0x%x] = 0x%x\n", mac_addr, mac_value);

	return 0;
}

static ssize_t mac_rw_write(struct file *file,
	      const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct ssusb_mtk *ssusb = s->private;
	struct mtu3 *mtu = ssusb->u3d;
	char buf[40];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "w", 1)) {
		u32 offset;
		u32 value;
		u32 shift;
		u32 mask;

		if (sscanf(buf, "w32 0x%x:%d:0x%x:0x%x",
			&offset, &shift, &mask, &value) == 4) {
			if ((offset % 4) != 0) {
				pr_notice("Must use 32bits alignment address\n");
				return count;
			}
			mac_write32(ssusb, offset,
				mask << shift, value << shift);
		} else
			return -EFAULT;
	}

	if (!strncmp(buf, "r", 1)) {
		u32 offset;

		if (sscanf(buf, "r32 0x%x", &offset) == 1) {
			if ((offset % 4) != 0) {
				pr_notice("Must use 32bits alignment address\n");
				return count;
			}
			mac_addr = offset;
			mac_value = mtu3_readl(mtu->mac_base, offset);
		}
	}

	return count;
}

static int mac_rw_open(struct inode *inode, struct file *file)
{
	return single_open(file, mac_rw_show, inode->i_private);
}

static const struct file_operations mac_rw_fops = {
	.open = mac_rw_open,
	.write = mac_rw_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ippc_rw_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "IPPC: [0x%x] = 0x%x\n", ippc_addr, ippc_value);

	return 0;
}

static ssize_t ippc_rw_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct ssusb_mtk *ssusb = s->private;
	struct mtu3 *mtu = ssusb->u3d;
	char buf[40];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "w", 1)) {
		u32 offset;
		u32 value;
		u32 shift;
		u32 mask;

		if (sscanf(buf, "w32 0x%x:%d:0x%x:0x%x",
			&offset, &shift, &mask, &value) == 4) {
			if ((offset % 4) != 0) {
				pr_notice("Must use 32bits alignment address\n");
				return count;
			}
			ippc_write32(ssusb, offset,
					mask << shift, value << shift);
		} else
			return -EFAULT;
	}

	if (!strncmp(buf, "r", 1)) {
		u32 offset;

		if (sscanf(buf, "r32 0x%x", &offset) == 1) {
			if ((offset % 4) != 0) {
				pr_notice("Must use 32bits alignment address\n");
				return count;
			}
			ippc_addr = offset;
			ippc_value = mtu3_readl(mtu->ippc_base, offset);
		}
	}

	return count;
}

static int ippc_rw_open(struct inode *inode, struct file *file)
{
	return single_open(file, ippc_rw_show, inode->i_private);
}

static const struct file_operations ippc_rw_fops = {
	.open = ippc_rw_open,
	.write = ippc_rw_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


void ssusb_debugfs_init(struct ssusb_mtk *ssusb)
{
	struct dentry *root;
	struct dentry *file;

	root = debugfs_create_dir("musb-hdrc", NULL);
	if (IS_ERR_OR_NULL(root)) {
		if (!root)
			dev_dbg(ssusb->dev, "create debugfs root failed\n");
		return;
	}
	ssusb->dbgfs_root = root;

	if (ssusb->u3_loopb_support) {
		file = debugfs_create_file("smt_err_count", 0644, root,
			ssusb, &smt_err_count);
		if (!file)
			dev_dbg(ssusb->dev, "file smt_err_count failed\n");
	}

	file = debugfs_create_file("ippc_reg", 0644, root,
		ssusb, &ippc_rw_fops);
	if (!file)
		dev_dbg(ssusb->dev, "file ippc_reg failed\n");

	file = debugfs_create_file("mac_reg", 0644, root,
		ssusb, &mac_rw_fops);
	if (!file)
		dev_dbg(ssusb->dev, "file mac_reg failed\n");
}


void ssusb_debugfs_exit(struct ssusb_mtk *ssusb)
{
	debugfs_remove_recursive(ssusb->dbgfs_root);
}


