/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2019 Unisoc Communications, Inc.
 * Author: Sheng Xu <sheng.xu@unisoc.com>
 */

#include <linux/compat.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define SPRD_MAP_IOCTRL_MAGIC        'o'
#define MAP_USER_MINOR MISC_DYNAMIC_MINOR
#define MAP_USER_VIR  _IOWR(SPRD_MAP_IOCTRL_MAGIC, 0, struct sprd_pmem_info)

struct reserved_mem_cfg {
	bool no_reserved;
	unsigned long reserved_mem_addr;
	size_t reserved_mem_size;
};

static struct reserved_mem_cfg mem_cfg;

struct sprd_pmem_info {
	uint64_t phy_addr;
	//uint32_t phys_offset;
	uint64_t size;
};

static int map_user_open(struct inode *inode, struct file *file)
{
	struct sprd_pmem_info *mem_info = kzalloc(sizeof(*mem_info),
						  GFP_KERNEL);
	if (!mem_info)
		return -ENODEV;
	file->private_data = mem_info;

	return 0;
}

static int map_user_release(struct inode *inode, struct file *file)
{
	struct sprd_pmem_info *mem_info = file->private_data;

	kfree(mem_info);
	file->private_data = NULL;

	return 0;
}

static int map_user_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	struct sprd_pmem_info *mem_info = file->private_data;

	if (!mem_info)
		return -ENODEV;

	if (mem_info->phy_addr && mem_info->size) {
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
		pr_info("%s: phy_addr=0x%llx, size=0x%llx\n",
				 __func__, mem_info->phy_addr, mem_info->size);
		ret = vm_iomap_memory(vma, mem_info->phy_addr, mem_info->size);
	} else {
		pr_err("%s: phy_addr=0x%llx, size=0x%llx err!\n",
			   __func__, mem_info->phy_addr, mem_info->size);
		ret = -EINVAL;
	}

	return ret;
}

static long map_user_ioctl(struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	void __user *arg_user;
	struct sprd_pmem_info *mem_info = file->private_data;

	if (!mem_info)
		return -ENODEV;

	if (mem_cfg.no_reserved)
		return -ENOMEM;

	arg_user = (void __user *)arg;
	switch (cmd) {
	case MAP_USER_VIR:
	{
		struct sprd_pmem_info data;

		if (copy_from_user(&data, arg_user, sizeof(data))) {
			pr_err("%s, PHYS copy_from_user error!\n", __func__);
			return -EFAULT;
		}

		if ((data.phy_addr < mem_cfg.reserved_mem_addr) ||
				((data.phy_addr + data.size) > (mem_cfg.reserved_mem_addr +
								mem_cfg.reserved_mem_size))) {
			pr_err("%s, user use mem out of reserved memory size!\n", __func__);
			return -EFAULT;
		}

		mem_info->phy_addr = data.phy_addr;
		mem_info->size = data.size;
		pr_debug("%s: phy_addr=0x%llx, size=0x%llx\n", __func__,
				  data.phy_addr, data.size);
		break;
	}
	default:
		return -ENOTTY;

	}
	return 0;
}

#ifdef CONFIG_COMPAT

static long compat_map_user_ioctl(struct file *filp, unsigned int cmd,
		     unsigned long arg)
{
	long ret = -ENOTTY;

	if (!filp->f_op->unlocked_ioctl)
		return -ENOTTY;
	if (cmd == MAP_USER_VIR) {
		pr_debug("%s: compat_ioctl enter\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, MAP_USER_VIR,
						 (unsigned long)compat_ptr(arg));
	} else {
		pr_err("%s, map_user cmd error!\n", __func__);
	}

	return ret;
}
#else
#define compat_map_user_ioctl NULL
#endif

static const struct file_operations map_user_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = map_user_ioctl,
	.compat_ioctl = compat_map_user_ioctl,
	.mmap = map_user_mmap,
	.open = map_user_open,
	.release = map_user_release,
};

static struct miscdevice map_user_dev = {
	.minor = MAP_USER_MINOR,
	.name = "map_user",
	.fops = &map_user_fops,
};

static int map_user_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *reserved_mem_node;
	struct device_node *fd_reserved_node;
	struct resource r;

	reserved_mem_node = of_find_node_by_name(NULL, "reserved-memory");
	if (reserved_mem_node == 0) {
		mem_cfg.no_reserved = true;
		dev_err(&pdev->dev, "find reserved memory node failed\n");
	} else {
		fd_reserved_node = of_get_child_by_name(reserved_mem_node, "faceid-mem");
		if (fd_reserved_node == 0) {
			mem_cfg.no_reserved = true;
			dev_err(&pdev->dev, "find reserved memory node failed\n");
		} else {
			if (of_address_to_resource(fd_reserved_node, 0, &r)) {
				mem_cfg.no_reserved = true;
				dev_err(&pdev->dev, "invalid fd reserved memory node!\n");
			} else {
				mem_cfg.reserved_mem_addr = r.start;
				mem_cfg.reserved_mem_size = resource_size(&r);
				mem_cfg.no_reserved = false;
			}
		}
	}

	ret = misc_register(&map_user_dev);
	if (ret)
		dev_err(&pdev->dev, "can't register miscdev minor=%d (%d)\n",
		    MAP_USER_MINOR, ret);

	return ret;
}

static int map_user_remove(struct platform_device *pdev)
{
	misc_deregister(&map_user_dev);
	pr_debug("%s Success!\n", __func__);
	return 0;
}

static const struct of_device_id of_match_table_map[] = {
	{ .compatible = "unisoc,map-user", },
	{ },
};

static struct platform_driver map_user_driver = {
	.probe = map_user_probe,
	.remove = map_user_remove,
	.driver = {
		.name = "map_user",
		.of_match_table = of_match_table_map,
	}
};

module_platform_driver(map_user_driver);

MODULE_DESCRIPTION("map_user Driver");
MODULE_LICENSE("GPL");
