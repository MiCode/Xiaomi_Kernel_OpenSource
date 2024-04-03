/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */

// #define DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <ispv4_regops.h>
#include "ispv4_boot.h"
#include <linux/component.h>

#define BAR_MEMDUMP_SIZE 0x3ffffff

extern struct dentry *ispv4_debugfs;

struct ispv4_memdump_dev {
	struct device *dev;
	void *base;
	u32 max_len;
	struct dentry *debug_dir;
	u32 isp_addr;
	struct debugfs_blob_wrapper region;
	u32 wirte_off;
	struct mutex ops_lock;
	struct device comp_dev;
};

struct ispv4_pcie_reg {
	void __iomem *base;
	uint32_t map_addr;
};
struct ispv4_pcie_reg ispv4_reg_glob;

int memdump_map_show(struct seq_file *file, void *data)
{
	struct ispv4_memdump_dev *mdev = file->private;
	seq_printf(file, "v4 addr: 0x%08x\n", mdev->isp_addr);
	return 0;
}

int memdump_map_open(struct inode *inode, struct file *filp)
{
	struct ispv4_memdump_dev *mdev = inode->i_private;
	single_open(filp, memdump_map_show, mdev);
	return 0;
}

static void memdump_iatu_set(u32 map_addr)
{
	u32 val;
	u32 m = map_addr & (0xFFFFFFFF - BAR_MEMDUMP_SIZE);
	putreg32(m, IATU_LWR_TARGET_ADDR(2, 1));
	putreg32(0, IATU_UPPER_TARGET_ADDR(2, 1));
	putreg32(0, IATU_REGION_CTRL_1(2, 1));
	val = IATU_REGION_EN;
	val |= IATU_BAR_MARCH_MODE;
	val |= (4 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	putreg32(val, IATU_REGION_CTRL_2(2, 1));
}

ssize_t memdump_map_write(struct file *filp, const char __user *data,
			  size_t len, loff_t *ppos)
{
	u32 addr = 0;
	int ret;
	struct ispv4_memdump_dev *mdev;

	mdev = ((struct seq_file *)filp->private_data)->private;

	ret = kstrtou32_from_user(data, len, 0, &addr);
	if (ret != 0) {
		dev_err(mdev->dev, "invalid address, parse failed\n");
		return len;
	}
	dev_info(mdev->dev, " map v400 addr 0x%08x\n", addr);
	memdump_iatu_set(addr);
	mdev->wirte_off = addr & BAR_MEMDUMP_SIZE;
	mdev->isp_addr = addr & (0xFFFFFFFF - BAR_MEMDUMP_SIZE);

	return len;
}

static struct file_operations memdump_map_fops = {
	.open = memdump_map_open,
	.read = seq_read,
	.write = memdump_map_write,
};

ssize_t memdump_wr_write(struct file *filp, const char __user *data, size_t len,
			 loff_t *ppos)
{
	struct ispv4_memdump_dev *mdev;
	int avalid_range;
	int ret = 0;
	static u32 kbuf[1024];
	u32 remain_len = len;
	u32 start_offset = 0;
	u64 p = *ppos;

	mdev = filp->private_data;

	if (mdev->wirte_off & 3) {
		dev_err(mdev->dev, "inalign write offset 0x%08x\n",
			mdev->wirte_off);
		return -ENOPARAM;
	}

	avalid_range = mdev->max_len - mdev->wirte_off + p;
	if (avalid_range <= 0) {
		dev_err(mdev->dev,
			"invalid offset. max_len = 0x%08x, offset = 0x%08x\n",
			mdev->max_len, mdev->wirte_off + p);
		return -ENOPARAM;
	}

	if (len > avalid_range) {
		dev_err(mdev->dev, "file size larger than avalid len[%d].\n",
			mdev->max_len - mdev->wirte_off + p);
		return -ENOPARAM;
	}

	while (remain_len) {
		u32 deal_len;

		deal_len = min_t(u32, sizeof(kbuf), remain_len);
		if (copy_from_user(kbuf, data + start_offset, deal_len)) {
			dev_err(mdev->dev, "User space pointer error %p\n",
				data);
			ret = -EFAULT;
			goto out;
		}
		memcpy_toio((u8 *)mdev->base + mdev->wirte_off + start_offset +
				    p,
			    kbuf, deal_len);
		dev_dbg(mdev->dev,
			"write %d bytes [%d/%d] to 0x%08x, apva = %lx",
			deal_len, start_offset, len,
			mdev->isp_addr + mdev->wirte_off + start_offset + p,
			(u8 *)mdev->base + mdev->wirte_off + start_offset + p);
		start_offset += deal_len;
		remain_len -= deal_len;
	}

out:
	if (ret == 0) {
		*ppos += len;
		return len;
	} else
		return ret;
}

int memdump_wr_open(struct inode *inode, struct file *filp)
{
	struct ispv4_memdump_dev *mdev = inode->i_private;
	filp->private_data = mdev;
	return 0;
}

static struct file_operations memdump_write_fops = {
	.open = memdump_wr_open,
	.write = memdump_wr_write,
};

uint32_t pcie_read_reg(uint32_t addr)
{
	uint32_t reg = 0;
	uint32_t read_off = 0;
	uint32_t map = 0;

	map = addr & (0xFFFFFFFF - BAR_MEMDUMP_SIZE);
	if (ispv4_reg_glob.map_addr != map) {
		ispv4_reg_glob.map_addr = map;
		memdump_iatu_set(addr);
		pr_info("ispv4 pcie read change map 0x%lx", ispv4_reg_glob.map_addr);
	}
	read_off = addr & BAR_MEMDUMP_SIZE;
	if (read_off & 3) {
		pr_err("ispv4 pcie read reg inalign write offset 0x%lx\n", read_off);
		return -ENOPARAM;
	}
	reg = readl(ispv4_reg_glob.base + read_off);
	pr_info("ispv4 pcie read data 0x%lx = 0x%lx   map: %lx   off %lx ", addr, reg,
		ispv4_reg_glob.map_addr, read_off);
	return reg;
}
EXPORT_SYMBOL_GPL(pcie_read_reg);

uint32_t pcie_write_reg(uint32_t data, uint32_t addr)
{
	uint32_t reg = 0;
	uint32_t read_off = 0;
	uint32_t map = 0;

	map = addr & (0xFFFFFFFF - BAR_MEMDUMP_SIZE);
	if (ispv4_reg_glob.map_addr != map) {
		ispv4_reg_glob.map_addr = map;
		memdump_iatu_set(addr);
		pr_info("ispv4 pcie read change map 0x%lx", ispv4_reg_glob.map_addr);
	}
	read_off = addr & BAR_MEMDUMP_SIZE;
	if (read_off & 3) {
		pr_err("ispv4 pcie write reg inalign write offset 0x%lx\n", read_off);
		return -ENOPARAM;
	}
	writel(data, ispv4_reg_glob.base + read_off);
	pr_info("ispv4 pcie write data 0x%lx = 0x%lx   map: %lx   off %lx ", addr, data,
		ispv4_reg_glob.map_addr, read_off);
	return reg;
}
EXPORT_SYMBOL_GPL(pcie_write_reg);

void pcie_iatu_reset(uint32_t addr)
{
	memdump_iatu_set(addr);
}
EXPORT_SYMBOL_GPL(pcie_iatu_reset);

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	dev_info(comp, "avalid!!\n");
	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
}

const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind,
};

static int ispv4_memdump_probe(struct platform_device *pdev)
{
	struct ispv4_memdump_dev *mdev;
	struct resource *res;
	struct dentry *tmp;
	int ret;

	mdev = devm_kzalloc(&pdev->dev, sizeof(*mdev), GFP_KERNEL);
	if (mdev == NULL) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "alloc memdump dev failed!\n");
		goto dev_data_alloc_err;
	}

	mdev->dev = &pdev->dev;
	mdev->isp_addr = 0;
	memdump_iatu_set(0);
	platform_set_drvdata(pdev, mdev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR_OR_NULL(res)) {
		ret = PTR_ERR(res);
		dev_err(&pdev->dev, "get resource failed! %d\n", ret);
		goto get_res_err;
	}

	mdev->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	ispv4_reg_glob.base=mdev->base;
	if (mdev->base == NULL) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "ioremap failed! %d\n", ret);
		goto get_res_err;
	}
	dev_info(&pdev->dev, "remap addr va=[%lx-%lx]\n", mdev->base,
		 (u8 *)mdev->base + resource_size(res) - 1);
	mdev->max_len = resource_size(res);
	mdev->region.size = resource_size(res);
	mdev->region.data = mdev->base;

	mdev->debug_dir = debugfs_create_dir("ispv4_memdump", ispv4_debugfs);
	if (IS_ERR_OR_NULL(mdev->debug_dir)) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "create debug dir failed! %d\n", ret);
		goto get_res_err;
	} else {
		tmp = debugfs_create_file("map", 0666, mdev->debug_dir, mdev,
					  &memdump_map_fops);
		if (IS_ERR_OR_NULL(tmp)) {
			ret = -EINVAL;
			goto debugfs_err;
		}
		tmp = debugfs_create_blob("memR", 0666, mdev->debug_dir,
					  &mdev->region);
		if (IS_ERR_OR_NULL(tmp)) {
			ret = -EINVAL;
			goto debugfs_err;
		}
		tmp = debugfs_create_file("memW", 0666, mdev->debug_dir, mdev,
					  &memdump_write_fops);
		if (IS_ERR_OR_NULL(tmp)) {
			ret = -EINVAL;
			goto debugfs_err;
		}
		debugfs_create_u32("memWoffset", 0666, mdev->debug_dir,
				   &mdev->wirte_off);
	}

	device_initialize(&mdev->comp_dev);
	dev_set_name(&mdev->comp_dev, "ispv4-memdump");
	pr_err("comp add %s! priv = %x, comp_name = %s\n", __FUNCTION__, mdev,
	       dev_name(&mdev->comp_dev));
	ret = component_add(&mdev->comp_dev, &comp_ops);
	if (ret != 0) {
		dev_err(&pdev->dev, "register memdump component failed.\n");
		goto component_err;
	}

	return 0;

debugfs_err:
	dev_err(&pdev->dev, "create debug file failed! %d\n", ret);
component_err:
	debugfs_remove(mdev->debug_dir);
get_res_err:
	devm_kfree(&pdev->dev, mdev);
dev_data_alloc_err:
	return ret;
}

static int ispv4_memdump_remove(struct platform_device *pdev)
{
	struct ispv4_memdump_dev *mdev = platform_get_drvdata(pdev);
	component_del(&mdev->comp_dev, &comp_ops);
	debugfs_remove(mdev->debug_dir);
	return 0;
}

static struct platform_driver ispv4_memdump_driver = {
	.probe = ispv4_memdump_probe,
	.remove = ispv4_memdump_remove,
	.driver = {
		.name = "ispv4-memdump",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ispv4_memdump_driver);
MODULE_AUTHOR("ChenHonglin<chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4.");
MODULE_LICENSE("GPL v2");
