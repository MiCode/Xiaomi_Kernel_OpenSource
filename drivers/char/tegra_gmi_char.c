/*
 * Copyright (C) 2013, NVIDIA Corporation.  All rights reserved.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * drivers/char/tegra_gmi_char.c
 *
 * MTD mapping driver for the internal SNOR controller in Tegra SoCs
 *
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <mach/iomap.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/tegra_snor.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/platform_data/tegra_nor.h>

#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
#include <linux/tegra_gmi_access.h>
#endif

#define gmi_lock(info)  \
do {    \
	if ((info)->request_gmi_access) \
		(info)->request_gmi_access((info)->gmiLockHandle);      \
} while (0)

#define gmi_unlock(info)        \
do {    \
	if ((info)->release_gmi_access) \
		(info)->release_gmi_access();   \
} while (0)



struct tegra_gmi_char_info {
	struct tegra_nor_chip_parms *plat;
	struct device *dev;
	void __iomem *base;
	u32 init_config;
	u32 timing0_default, timing1_default;
	u32 timing0_read, timing1_read;
	unsigned long gmi_chrdev_is_open;
	u32 gmiLockHandle;
	int (*request_gmi_access)(u32 gmiLockHandle);
	void (*release_gmi_access)(void);
	dev_t gmi_dev_t;
	struct cdev gmi_cdev;
	struct class *gmi_class;
	struct device *gmi_device;
};

static struct tegra_gmi_char_info *info;


#define DRV_NAME "tegra-gmi-char"

static inline unsigned long snor_tegra_readl(struct tegra_gmi_char_info *tsnor,
					     unsigned long reg)
{
	return readl(tsnor->base + reg);
}

static inline void snor_tegra_writel(struct tegra_gmi_char_info *tsnor,
				     unsigned long val, unsigned long reg)
{
	writel(val, tsnor->base + reg);
}


static int tegra_gmi_char_prepare_regs(struct tegra_gmi_char_info *info)
{
	struct tegra_nor_chip_parms *chip_parm = info->plat;
	struct cs_info *csinfo = &chip_parm->csinfo;

	u32 width = chip_parm->BusWidth;
	u32 config = 0;

	config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(0);
	config |= TEGRA_SNOR_CONFIG_SNOR_CS(csinfo->cs);
	config &= ~TEGRA_SNOR_CONFIG_DEVICE_TYPE;
	config |= TEGRA_SNOR_CONFIG_WP;	/* Enable writes */

	switch (width) {
	case 2:
		config &= ~TEGRA_SNOR_CONFIG_WORDWIDE;	/* 16 bit */
		break;
	case 4:
		config |= TEGRA_SNOR_CONFIG_WORDWIDE;	/* 32 bit */
		break;
	default:
		return -EINVAL;
	}

	switch (chip_parm->MuxMode) {
	case NorMuxMode_ADNonMux:
		config &= ~TEGRA_SNOR_CONFIG_MUX_MODE;
		break;
	case NorMuxMode_ADMux:
		config |= TEGRA_SNOR_CONFIG_MUX_MODE;
		break;
	default:
		return -EINVAL;
	}

	switch (chip_parm->ReadyActive) {
	case NorReadyActive_WithData:
		config &= ~TEGRA_SNOR_CONFIG_RDY_ACTIVE;
		break;
	case NorReadyActive_BeforeData:
		config |= TEGRA_SNOR_CONFIG_RDY_ACTIVE;
		break;
	default:
		return -EINVAL;
	}

	info->init_config = config;
	info->timing0_default = chip_parm->timing_default.timing0;
	info->timing0_read = chip_parm->timing_read.timing0;
	info->timing1_default = chip_parm->timing_default.timing1;
	info->timing1_read = chip_parm->timing_read.timing1;

	return 0;
}

static int gmichar_chrdev_open(struct inode *inode, struct file *file)
{
	/* Only one active open supported */
	if (test_and_set_bit(0, &info->gmi_chrdev_is_open))
		return -EBUSY;
	return 0;
}

static int gmichar_chrdev_release(struct inode *inode, struct file *file)
{
	clear_bit(0, &info->gmi_chrdev_is_open);
	return 0;
}


static ssize_t gmichar_chrdev_read_helper(char *dest, int *src,
						size_t size)
{
	int i;

	snor_tegra_writel(info, info->init_config, TEGRA_SNOR_CONFIG_REG);
	snor_tegra_writel(info, info->timing1_read, TEGRA_SNOR_TIMING1_REG);
	snor_tegra_writel(info, info->timing0_read, TEGRA_SNOR_TIMING0_REG);

	udelay(1);

	for (i = 0; i < size; i++)
		dest[i] = __raw_readb(src + i);

	udelay(1);

	return size;
}

static loff_t gmichar_chrdev_llseek(struct file *filp,
						loff_t offset, int origin)
{
	ssize_t ret;
	struct tegra_nor_chip_parms *chip_parm = info->plat;
	struct cs_info *csinfo = &chip_parm->csinfo;

	if (info == NULL)
		return -EINVAL;

	switch (origin) {
	case SEEK_END:
		offset += csinfo->size;
		break;
	case SEEK_CUR:
		offset += filp->f_pos;
		break;
	case SEEK_SET:
		break;
	default:
		ret = -EINVAL;
	}

	if (offset < 0 || offset > csinfo->size)
		offset = -EINVAL;
	else
		filp->f_pos = offset;

	return offset;
}

static ssize_t gmichar_chrdev_read(struct file *filp, char *data,
						size_t size, loff_t *loff)
{
	ssize_t ret;
	char *vbuf = NULL;
	unsigned int *ptr = NULL;
	struct tegra_nor_chip_parms *chip_parm = info->plat;
	struct cs_info *csinfo = &chip_parm->csinfo;

	if (info == NULL)
		return -EINVAL;

	if (*loff + size > csinfo->size)
		return -EINVAL;

	vbuf = vmalloc(size);

	if (vbuf == NULL)
		return -EFAULT;

	ptr = csinfo->virt;
	ptr = ptr + (*loff);

	gmi_lock(info);
	ret = gmichar_chrdev_read_helper(vbuf, ptr, size);

	if (copy_to_user(data, vbuf, size))
		return -EFAULT;

	*loff = *loff + ret;
	vfree(vbuf);
	gmi_unlock(info);
	return ret;
}


static ssize_t gmichar_chrdev_write_helper(unsigned int *dest, char *src,
						size_t size)
{
	int i;

	snor_tegra_writel(info, info->init_config, TEGRA_SNOR_CONFIG_REG);
	snor_tegra_writel(info, info->timing1_read, TEGRA_SNOR_TIMING1_REG);
	snor_tegra_writel(info, info->timing0_read, TEGRA_SNOR_TIMING0_REG);

	udelay(1);

	for (i = 0; i < size; i++)
		__raw_writeb(src[i], dest + i);

	udelay(1);
	return size;

}

static ssize_t gmichar_chrdev_write(struct file *filp, const char *data,
						size_t size, loff_t *loff)
{
	ssize_t ret;
	struct tegra_nor_chip_parms *chip_parm = info->plat;
	struct cs_info *csinfo = &chip_parm->csinfo;
	char *vbuf = NULL;
	unsigned int *ptr = NULL;

	if (info == NULL)
		return -EINVAL;

	vbuf = vmalloc(size);
	if (copy_from_user(vbuf, data, size))
		return -EFAULT;

	ptr = csinfo->virt;
	ptr = ptr +  *loff;
	gmi_lock(info);

	ret = gmichar_chrdev_write_helper(ptr, vbuf, size);
	*loff += size;

	gmi_unlock(info);
	vfree(vbuf);
	return ret;

}

const struct file_operations gmichar_fops = {
	.open           = gmichar_chrdev_open,
	.read           = gmichar_chrdev_read,
	.write          = gmichar_chrdev_write,
	.llseek		= gmichar_chrdev_llseek,
	.release        = gmichar_chrdev_release
};

static int __devinit tegra_gmi_char_probe(struct platform_device *pdev)
{
	int err = -ENODEV;
	struct tegra_nor_chip_parms *plat = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;

	if (!plat) {
		pr_err("%s: no platform device info\n", __func__);
		return -EINVAL;
	}
	info = devm_kzalloc(dev, sizeof(struct tegra_gmi_char_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->base = ((void __iomem *)IO_APB_VIRT +
				(TEGRA_SNOR_BASE - IO_APB_PHYS));
	info->plat = plat;
	info->dev = dev;

#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
	info->gmiLockHandle = register_gmi_device(DRV_NAME, 0);
	info->request_gmi_access = request_gmi_access;
	info->release_gmi_access = release_gmi_access;
	info->gmiLockHandle =	register_gmi_device(DRV_NAME, 0);
#else
	info->gmiLockHandle = NULL;
	info->request_gmi_access = NULL;
	info->release_gmi_access = NULL;
#endif

	err = tegra_gmi_char_prepare_regs(info);

	if (err) {
		dev_err(dev, "Error initializing reg values\n");
		return err;
	}

	platform_set_drvdata(pdev, info);

	info->gmi_class = class_create(THIS_MODULE, DRV_NAME);

	if (IS_ERR(info->gmi_class)) {
		err = PTR_ERR(info->gmi_class);
		return err;
	}

	err = alloc_chrdev_region(&info->gmi_dev_t, 0, 1, DRV_NAME);

	if (err < 0) {
		class_destroy(info->gmi_class);
		pr_err("%s: failed to allocate device region\n", DRV_NAME);
		return err;
	}

	cdev_init(&info->gmi_cdev, &gmichar_fops);
	err = cdev_add(&info->gmi_cdev, info->gmi_dev_t, 1);

	if (err)
		return err;

	info->gmi_device = device_create(info->gmi_class, NULL,
					info->gmi_dev_t, info, DRV_NAME);

	if (IS_ERR(info->gmi_device)) {
		pr_err("device create failed for %s\n", DRV_NAME);
		cdev_del(&info->gmi_cdev);
		goto fail;
	}

	return 0;
fail:
	pr_err("Tegra GMI CHAR probe failed\n");
	return err;
}

static int __devexit tegra_gmi_char_remove(struct platform_device *pdev)
{

	if (info->gmi_class)
		class_destroy(info->gmi_class);

	unregister_chrdev_region(info->gmi_dev_t, 1);

	return 0;
}

static struct platform_driver __refdata tegra_gmi_char_driver = {
	.probe = tegra_gmi_char_probe,
	.remove = __devexit_p(tegra_gmi_char_remove),
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   },
};


module_platform_driver(tegra_gmi_char_driver);

MODULE_AUTHOR("Nitin Sehgal <nsehgal@nvidia.com>");
MODULE_DESCRIPTION("GMI Bus character driver interface for NVIDIA Tegra based boards");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
