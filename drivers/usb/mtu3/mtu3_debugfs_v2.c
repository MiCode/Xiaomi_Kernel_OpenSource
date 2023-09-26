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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>

#include "mtu3.h"
#include "mtu3_hal.h"
#include "mtu3_dr.h"

#define PROC_DIR_MTK_USB "mtk_usb"
#define PROC_FILE_SMTERRCOUNT "mtk_usb/smt_err_count"
#define PROC_FILE_IPPCREG "mtk_usb/ippc_reg"
#define PROC_FILE_MACREG "mtk_usb/mac_reg"
#define PROC_FILE_SPEED "mtk_usb/speed"

#define PROC_FILE_NUM 4
static struct proc_dir_entry *proc_files[PROC_FILE_NUM] = {
	NULL, NULL, NULL, NULL};

static u32 mac_value, mac_addr;
static u32 ippc_value, ippc_addr;

static int smt_err_count_get(void *data, u64 *val)
{
	struct ssusb_mtk *ssusb = NULL;

	if (IS_ERR_OR_NULL(data))
		return -EFAULT;
	ssusb = data;
	*val = ssusb_u3loop_back_test(ssusb);

	mtu3_printk(K_INFO, "%s %llu\n", __func__, *val);

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
	if (IS_ERR_OR_NULL(res) || offset >= resource_size(res)) {
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
	if (IS_ERR_OR_NULL(res) || offset >= resource_size(res)) {
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
	return single_open(file, mac_rw_show, PDE_DATA(inode));
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
	return single_open(file, ippc_rw_show, PDE_DATA(inode));
}

static const struct file_operations ippc_rw_fops = {
	.open = ippc_rw_open,
	.write = ippc_rw_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int mtu3_speed_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "mtu3_speed = %d\n", mtu3_speed);
	return 0;
}

static int mtu3_speed_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtu3_speed_show, PDE_DATA(inode));
}

static ssize_t mtu3_speed_write(struct file *file,
			const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[20];
	int val;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 10, &val) == 0  && val >= 0 && val <= 1)
		mtu3_speed = val;
	else
		return -EINVAL;

	return count;
}

static const struct file_operations mtu3_speed_fops = {
	.open = mtu3_speed_open,
	.write = mtu3_speed_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void ssusb_debugfs_init(struct ssusb_mtk *ssusb)
{
	int idx = 0;

	proc_mkdir(PROC_DIR_MTK_USB, NULL);

	if (ssusb->u3_loopb_support) {
		proc_files[idx] = proc_create_data(PROC_FILE_SMTERRCOUNT, 0644, NULL,
			&smt_err_count, ssusb);
		if (!proc_files[idx])
			dev_dbg(ssusb->dev, "file smt_err_count failed\n");
		idx++;
	}

	proc_files[idx] = proc_create_data(PROC_FILE_IPPCREG, 0644, NULL,
		&ippc_rw_fops, ssusb);
	if (!proc_files[idx])
		dev_dbg(ssusb->dev, "file ippc_reg failed\n");
	idx++;

	proc_files[idx] = proc_create_data(PROC_FILE_MACREG, 0644, NULL,
		&mac_rw_fops, ssusb);
	if (!proc_files[idx])
		dev_dbg(ssusb->dev, "file mac_reg failed\n");
	idx++;

	proc_files[idx] = proc_create_data(PROC_FILE_SPEED, 0644, NULL,
		&mtu3_speed_fops, ssusb);
	if (!proc_files[idx])
		dev_dbg(ssusb->dev, "file speed failed\n");

}

void ssusb_debugfs_exit(struct ssusb_mtk *ssusb)
{
	int idx = 0;

	for (; idx < PROC_FILE_NUM ; idx++) {
		if (proc_files[idx])
			proc_remove(proc_files[idx]);
	}
}


