/*
 *  sound/soc/samsung/rt5509-calib.c
 *
 *  Copyright (C) 2016 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
/* vfs */
#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
/* alsa sound header */
#include <sound/soc.h>

#include "rt5509.h"

#define rt5509_calib_path "/data/local/tmp/"

struct rt5509_proc_file_t {
	const char *name;
	umode_t mode;
	const struct file_operations file_ops;
};

#define RT5509_CALIB_MAGIC (5526789)

#define rt5509_proc_file_m(_name, _mode) {\
	.name = #_name,\
	.mode = (_mode),\
	.file_ops = {\
		.owner = THIS_MODULE,\
		.open = simple_open,\
		.read = _name##_file_read,\
		.write = _name##_file_write,\
		.llseek = default_llseek,\
	},\
}

enum {
	RT5509_CALIB_CTRL_START = 0,
	RT5509_CALIB_CTRL_N20DB,
	RT5509_CALIB_CTRL_N15DB,
	RT5509_CALIB_CTRL_N10DB,
	RT5509_CALIB_CTRL_READOTP,
	RT5509_CALIB_CTRL_WRITEOTP,
	RT5509_CALIB_CTRL_WRITEFILE,
	RT5509_CALIB_CTRL_END,
	RT5509_CALIB_CTRL_MAX,
};

static int calib_start;
static int param_put;

static struct file *file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if(IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}
	return filp;
}

static int file_read(struct file *file, unsigned long long offset,
		     unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());
	ret = vfs_read(file, data, size, &offset);
	set_fs(oldfs);
	return ret;
}

static int file_size(struct file *file)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());
	ret = (file->f_path.dentry)->d_inode->i_size;
	set_fs(oldfs);
	return ret;
}

static int file_write(struct file *file, unsigned long long offset,
		     const unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());
	ret = vfs_write(file, data, size, &offset);
	set_fs(oldfs);
	return ret;
}

static void file_close(struct file *file)
{
	filp_close(file, NULL);
}

static struct rt5509_chip *get_chip_data(struct file *filp)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	return PDE_DATA(file_inode(filp));
#else
	return PDE(filp->f_path.dentry->d_inode)->data;
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)) */
}

static int rt5509_calib_choosen_db(struct rt5509_chip *chip, int choose)
{
	struct snd_soc_codec *codec = chip->codec;
	u32 data = 0;
	uint8_t mode_store;
	int ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	if (!calib_start)
		return -EINVAL;
	ret = snd_soc_read(codec, RT5509_REG_BST_MODE);
	if (ret < 0)
		return ret;
	mode_store = ret;
	ret = snd_soc_update_bits(codec, RT5509_REG_BST_MODE,
		0x03, 0x02);
	if (ret < 0)
		return ret;
	data = 0x0080;
	ret = snd_soc_write(codec, RT5509_REG_CALIB_REQ, data);
	if (ret < 0)
		return ret;
	switch (choose) {
	case RT5509_CALIB_CTRL_N20DB:
		data = 0x0ccc;
		break;
	case RT5509_CALIB_CTRL_N15DB:
		data = 0x16c3;
		break;
	case RT5509_CALIB_CTRL_N10DB:
		data = 0x287a;
		break;
	default:
		return -EINVAL;
	}
	ret = snd_soc_write(codec, RT5509_REG_CALIB_GAIN, data);
	if (ret < 0)
		return ret;
	ret = snd_soc_read(codec, RT5509_REG_CALIB_CTRL);
	if (ret < 0)
		return ret;
	data = ret;
	data |= 0x80;
	ret = snd_soc_write(codec, RT5509_REG_CALIB_CTRL, data);
	if (ret < 0)
		return ret;
	mdelay(100);
	data &= ~(0x80);
	ret = snd_soc_write(codec, RT5509_REG_CALIB_CTRL, data);
	if (ret < 0)
		return ret;
	ret = snd_soc_read(codec, RT5509_REG_CALIB_OUT0);
	param_put = ret;
	ret = snd_soc_update_bits(codec, RT5509_REG_BST_MODE,
		0x03, mode_store);
	if (ret < 0)
		return ret;
	dev_info(chip->dev, "param = %d\n", param_put);
	return 0;
}

static int rt5509_calib_read_otp(struct rt5509_chip *chip)
{
	struct snd_soc_codec *codec = chip->codec;
	int ret = 0;

	ret = snd_soc_read(codec, RT5509_REG_ISENSEGAIN);
	ret &= 0xffffff;
	if (ret < 0)
		return ret;
	param_put = ret;
	dev_info(chip->dev, "param = 0x%08x\n", param_put);
	return 0;
}

static int rt5509_calib_write_otp(struct rt5509_chip *chip)
{
	struct snd_soc_codec *codec = chip->codec;
	uint8_t mode_store;
	uint32_t param_store;
	uint32_t bst_th;
	int ret = 0;

	ret = snd_soc_update_bits(codec, RT5509_REG_CHIPEN,
				  RT5509_SPKAMP_ENMASK, 0);
	if (ret < 0)
		return ret;
	ret = snd_soc_read(codec, RT5509_REG_BST_TH1);
	if (ret < 0)
		return ret;
	bst_th = ret;
	ret = snd_soc_read(codec, RT5509_REG_BST_MODE);
	if (ret < 0)
		return ret;
	mode_store = ret;
	ret = snd_soc_write(codec, RT5509_REG_BST_TH1, 0x029b);
	if (ret < 0)
		return ret;
	ret = snd_soc_update_bits(codec, RT5509_REG_BST_MODE,
		0x03, 0x02);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5509_REG_CALIB_DCR, param_put);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5509_REG_OTPCONF, 0x81);
	if (ret < 0)
		return ret;
	msleep(100);
	ret = snd_soc_write(codec, RT5509_REG_OTPCONF, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_update_bits(codec, RT5509_REG_BST_MODE,
		0x03, mode_store);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5509_REG_BST_TH1, bst_th);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5509_REG_CALIB_DCR, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5509_REG_OTPCONF, 0x82);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5509_REG_OTPCONF, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_read(codec, RT5509_REG_CALIB_DCR);
	param_store = ret & 0xffffff;
	dev_info(chip->dev, "store %08x, put %08x\n", param_store, param_put);
	if (param_store != param_put)
		return -EINVAL;
	ret = snd_soc_update_bits(codec, RT5509_REG_CHIPEN,
				  RT5509_SPKAMP_ENMASK,
				  RT5509_SPKAMP_ENMASK);
	if (ret < 0)
		return ret;
	return 0;
}

static int rt5509_calib_rwotp(struct rt5509_chip *chip, int choose)
{
	int ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	if (!calib_start)
		return -EINVAL;
	switch (choose) {
	case RT5509_CALIB_CTRL_READOTP:
		ret = rt5509_calib_read_otp(chip);
		if (ret < 0)
			return ret;
		break;
	case RT5509_CALIB_CTRL_WRITEOTP:
		ret = rt5509_calib_write_otp(chip);
		if (ret < 0)
			return ret;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rt5509_calib_write_file(struct rt5509_chip *chip)
{
	struct file *calib_file;
	char tmp[100] = {0};
	char file_name[100] = {0};

	sprintf(file_name, rt5509_calib_path "rt5509_calib.%d", chip->pdev->id);
	calib_file = file_open(file_name, O_WRONLY | O_CREAT,
			       S_IRUGO | S_IWUSR);
	if (!calib_file) {
		dev_err(chip->dev, "open file fail\n");
		return -EFAULT;
	}
	dev_info(chip->dev, "rspk=%d\n", param_put);
	sprintf(tmp, "rspk=%d\n", param_put);
	file_write(calib_file, 0, tmp, strlen(tmp));
	file_close(calib_file);
	return 0;
}

static int rt5509_calib_start_process(struct rt5509_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt5509_calib_end_process(struct rt5509_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

#define calib_file_read NULL
static ssize_t calib_file_write(struct file *filp, const char __user *buf,
				size_t cnt, loff_t *ppos)
{
	struct rt5509_chip *chip = get_chip_data(filp);
	int ctrl = 0, ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	if (sscanf(buf, "%d", &ctrl) < 1)
		return -EINVAL;
	if (ctrl < RT5509_CALIB_MAGIC)
		return -EINVAL;
	if (ctrl >= (RT5509_CALIB_MAGIC + RT5509_CALIB_CTRL_MAX))
		return -EINVAL;
	ctrl -= RT5509_CALIB_MAGIC;
	dev_dbg(chip->dev, "ctrl = %d\n", ctrl);
	switch (ctrl) {
	case RT5509_CALIB_CTRL_N20DB:
	case RT5509_CALIB_CTRL_N15DB:
	case RT5509_CALIB_CTRL_N10DB:
		ret = rt5509_calib_choosen_db(chip, ctrl);
		if (ret < 0)
			return ret;
		break;
	case RT5509_CALIB_CTRL_READOTP:
	case RT5509_CALIB_CTRL_WRITEOTP:
		ret = rt5509_calib_rwotp(chip, ctrl);
		if (ret < 0)
			return ret;
		break;
	case RT5509_CALIB_CTRL_WRITEFILE:
		ret = rt5509_calib_write_file(chip);
		if (ret < 0)
			return ret;
		break;
	case RT5509_CALIB_CTRL_START:
		if (param_put == RT5509_CALIB_MAGIC) {
			ret = rt5509_calib_start_process(chip);
			if (ret < 0)
				return ret;
			calib_start = 1;
			dev_info(chip->dev, "calib started\n");
		} else
			return -EINVAL;
		break;
	case RT5509_CALIB_CTRL_END:
		if (param_put == RT5509_CALIB_MAGIC) {
			ret = rt5509_calib_end_process(chip);
			if (ret < 0)
				return ret;
			calib_start = 0;
			param_put = 0;
			dev_info(chip->dev, "calib end\n");
		} else
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}
	return cnt;
}

static ssize_t param_file_read(struct file *filp, char __user *buf,
			       size_t cnt, loff_t *ppos)
{
	struct rt5509_chip *chip = get_chip_data(filp);
	char tmp[50] = {0};
	int len = 0;

	dev_info(chip->dev, "%s\n", __func__);
	len = scnprintf(tmp, ARRAY_SIZE(tmp), "%d\n", param_put);
	return simple_read_from_buffer(buf, cnt, ppos, tmp, len);
}

static ssize_t param_file_write(struct file *filp, const char __user *buf,
				size_t cnt, loff_t *ppos)
{
	struct rt5509_chip *chip = get_chip_data(filp);
	int param = 0;

	dev_info(chip->dev, "%s\n", __func__);
	if (sscanf(buf, "%d", &param) < 1)
		return -EINVAL;
	param_put = param;
	return cnt;
}

static ssize_t calib_data_file_read(struct file *filp, char __user *buf,
			       size_t cnt, loff_t *ppos)
{
	struct rt5509_chip *chip = get_chip_data(filp);
	struct file *calib_file;
	char tmp[100] = {0};
	char file_name[100] = {0};

	sprintf(file_name, rt5509_calib_path "rt5509_calib.%d", chip->pdev->id);
	calib_file = file_open(file_name,
			       O_RDONLY, S_IRUGO | S_IWUSR);
	if (!calib_file) {
		dev_info(chip->dev, "open file fail\n");
		return -EIO;
	}
	file_read(calib_file, 0, tmp, file_size(calib_file));
	file_close(calib_file);
	return simple_read_from_buffer(buf, cnt, ppos, tmp, strlen(tmp));
}

#define calib_data_file_write NULL

static ssize_t chip_rev_file_read(struct file *filp, char __user *buf,
			       size_t cnt, loff_t *ppos)
{
	struct rt5509_chip *chip = get_chip_data(filp);
	char tmp[100] = {0};

	sprintf(tmp, "%d\n", chip->chip_rev);
	return simple_read_from_buffer(buf, cnt, ppos, tmp, strlen(tmp));
}

#define chip_rev_file_write NULL

static const struct rt5509_proc_file_t rt5509_proc_file[] = {
	rt5509_proc_file_m(calib, S_IRUGO | S_IWUSR),
	rt5509_proc_file_m(param, S_IRUGO | S_IWUSR),
	rt5509_proc_file_m(calib_data, S_IRUGO),
	rt5509_proc_file_m(chip_rev, S_IRUGO),
};

void rt5509_calib_destroy(struct rt5509_chip *chip)
{
	int i = 0;
	char proc_name[100] = {0};

	dev_dbg(chip->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(rt5509_proc_file); i++)
		remove_proc_entry(rt5509_proc_file[i].name, chip->root_entry);
	sprintf(proc_name, "rt5509_calib.%d", chip->pdev->id);
	remove_proc_entry(proc_name, NULL);
	chip->root_entry = NULL;
}
EXPORT_SYMBOL_GPL(rt5509_calib_destroy);

int rt5509_calib_create(struct rt5509_chip *chip)
{
	struct proc_dir_entry *entry;
	char proc_name[100] = {0};
	int i = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	sprintf(proc_name, "rt5509_calib.%d", chip->pdev->id);
	entry = proc_mkdir(proc_name, NULL);
	if (!entry) {
		dev_err(chip->dev, "%s: create proc folder fail\n", __func__);
		return -EINVAL;
	}
	chip->root_entry = entry;
	for (i = 0; i < ARRAY_SIZE(rt5509_proc_file); i++) {
		entry = proc_create_data(rt5509_proc_file[i].name, 0644,
					 chip->root_entry,
					 &rt5509_proc_file[i].file_ops,
					 chip);
		if (!entry) {
			dev_err(chip->dev, "%s, proc file fail\n", __func__);
			goto out_create_proc;
		}
	}
	return 0;
out_create_proc:
	while (--i >= 0)
		remove_proc_entry(rt5509_proc_file[i].name, chip->root_entry);
	remove_proc_entry(proc_name, NULL);
	return -EFAULT;
}
EXPORT_SYMBOL_GPL(rt5509_calib_create);

