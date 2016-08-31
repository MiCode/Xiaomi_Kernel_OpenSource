/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/export.h>
#include <linux/module.h>

#include <media/ov7695.h>

#define SIZEOF_I2C_TRANSBUF 128

#define	OV7695_TABLE_START	0x01
#define	OV7695_TABLE_END	0x02
#define	OV7695_WAIT_MS		0xFFFF

struct ov7695_reg {
	u16 addr;
	u16 val;
};

static struct ov7695_reg mode_640x480_30fps[] = {
	{OV7695_TABLE_START, 0x01},
	{0x0100, 0x00},
	{OV7695_WAIT_MS, 0x0a},
	{0x0103, 0x01},
	{0x3620, 0x2f},
	{0x3623, 0x12},
	{0x3718, 0x88},
	{0x3703, 0x80},
	{0x3712, 0x40},
	{0x3706, 0x40},
	{0x3631, 0x44},
	{0x3632, 0x05},
	{0x3013, 0xd0},
	{0x3705, 0x1d},
	{0x3713, 0x0e},
	{0x3012, 0x0a},
	{0x3717, 0x18},
	{0x3621, 0x47},
	{0x0309, 0x24},
	{0x4803, 0x08},
	{0x0101, 0x01},
	{0x3002, 0x09},
	{0x3024, 0x00},
	{0x3630, 0x69},
	{0x5090, 0x00},
	{0x3820, 0x90},
	{0x4500, 0x24},
	{0x4008, 0x02},
	{0x4009, 0x09},
	{0x3811, 0x07},
	{0x3813, 0x06},
	/* YUV order 0x4300
	 * 3F : YUYV(default)
	 * 33 : YVYU
	 * 31 : VYUY
	 * 30 : UYVY
	 */
	{0x4300, 0x30},
	{0x5000, 0xff},
	{0x5001, 0x3f},
	{0x5002, 0x48},
	{0x5910, 0x00},
	{0x3a0f, 0x48},
	{0x3a10, 0x40},
	{0x3a11, 0x90},
	{0x3a1b, 0x4a},
	{0x3a1e, 0x3e},
	{0x3a1f, 0x18},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3503, 0x00},
	{0x3a00, 0x7c},
	{0x382a, 0x08},
	{0x3a02, 0x03},
	{0x3a03, 0x20},
	{0x3a14, 0x03},
	{0x3a15, 0x20},
	{0x3a0d, 0x04},
	{0x3a0e, 0x03},
	{0x3a17, 0x02},
	{0x5100, 0x01},
	{0x5101, 0x50},
	{0x5102, 0x00},
	{0x5103, 0xf8},
	{0x5104, 0x03},
	{0x5105, 0x00},
	{0x5106, 0x00},
	{0x5107, 0x00},
	{0x5108, 0x01},
	{0x5109, 0x50},
	{0x510a, 0x00},
	{0x510b, 0xf8},
	{0x510c, 0x02},
	{0x510d, 0x00},
	{0x510e, 0x00},
	{0x510f, 0x00},
	{0x5110, 0x01},
	{0x5111, 0x50},
	{0x5112, 0x00},
	{0x5113, 0xf8},
	{0x5114, 0x02},
	{0x5115, 0x00},
	{0x5116, 0x00},
	{0x5117, 0x00},
	{0x5201, 0xd0},
	{0x520a, 0xf4},
	{0x520b, 0xf4},
	{0x520c, 0xf4},
	{0x5301, 0x05},
	{0x5302, 0x0c},
	{0x5303, 0x1c},
	{0x5304, 0x2a},
	{0x5305, 0x39},
	{0x5306, 0x45},
	{0x5307, 0x53},
	{0x5308, 0x5d},
	{0x5309, 0x68},
	{0x530a, 0x7f},
	{0x530b, 0x91},
	{0x530c, 0xa5},
	{0x530d, 0xc6},
	{0x530e, 0xde},
	{0x530f, 0xef},
	{0x5310, 0x16},
	{0x5003, 0x80},
	{0x5500, 0x08},
	{0x5501, 0x48},
	{0x5502, 0x16},
	{0x5503, 0x08},
	{0x5504, 0x08},
	{0x5505, 0x48},
	{0x5506, 0x02},
	{0x5507, 0x16},
	{0x5508, 0x2d},
	{0x5509, 0x08},
	{0x550a, 0x48},
	{0x550b, 0x06},
	{0x550c, 0x04},
	{0x550d, 0x01},
	{0x5600, 0x00},
	{0x5601, 0x23},
	{0x5602, 0x59},
	{0x5603, 0x04},
	{0x5604, 0x11},
	{0x5605, 0x57},
	{0x5606, 0x68},
	{0x5607, 0x68},
	{0x5608, 0x57},
	{0x5609, 0x11},
	{0x560a, 0x01},
	{0x560b, 0x98},
	{0x5800, 0x02},
	{0x5803, 0x38},
	{0x5804, 0x34},
	{0x5908, 0x62},
	{0x5909, 0x26},
	{0x590a, 0xe6},
	{0x590b, 0x6e},
	{0x590c, 0xea},
	{0x590d, 0xae},
	{0x590e, 0xa6},
	{0x590f, 0x6a},
	{0x0100, 0x01},
	{OV7695_TABLE_END, 0x01},
};

static struct ov7695_reg ov7695_Whitebalance_Auto[] = {
	{0x5200, 0x00},
	{OV7695_TABLE_END, 0x0000}
};

static struct ov7695_reg ov7695_Whitebalance_Daylight[] = {
	{0x5200, 0x20},
	{0x5204, 0x05},
	{0x5205, 0x1e},
	{0x5206, 0x04},
	{0x5207, 0x00},
	{0x5208, 0x04},
	{0x5209, 0x7a},
	{OV7695_TABLE_END, 0x0000}
};

static struct ov7695_reg ov7695_Whitebalance_Cloudy[] = {
	{0x5200, 0x20},
	{0x5204, 0x06},
	{0x5205, 0x00},
	{0x5206, 0x04},
	{0x5207, 0x00},
	{0x5208, 0x04},
	{0x5209, 0x20},
	{OV7695_TABLE_END, 0x0000}
};

static struct ov7695_reg ov7695_Whitebalance_Incandescent[] = {
	{0x5200, 0x20},
	{0x5204, 0x04},
	{0x5205, 0x00},
	{0x5206, 0x05},
	{0x5207, 0x0a},
	{0x5208, 0x08},
	{0x5209, 0xae},
	{OV7695_TABLE_END, 0x0000}
};

static struct ov7695_reg ov7695_Whitebalance_Fluorescent[] = {
	{0x5200, 0x20},
	{0x5204, 0x04},
	{0x5205, 0x00},
	{0x5206, 0x04},
	{0x5207, 0x00},
	{0x5208, 0x09},
	{0x5209, 0xf5},
	{OV7695_TABLE_END, 0x0000}
};


static struct ov7695_reg ov7695_EV_zero[] = {
	{0x3a0f, 0x48},
	{0x3a10, 0x40},
	{0x3a11, 0x90},
	{0x3a1b, 0x4a},
	{0x3a1e, 0x3e},
	{0x3a1f, 0x18},
	{OV7695_TABLE_END, 0x0000}
};

static struct ov7695_reg ov7695_EV_plus_1[] = {
	{0x3a0f, 0x50},
	{0x3a10, 0x48},
	{0x3a11, 0x98},
	{0x3a1b, 0x52},
	{0x3a1e, 0x46},
	{0x3a1f, 0x20},
	{OV7695_TABLE_END, 0x0000}
};

static struct ov7695_reg ov7695_EV_plus_2[] = {
	{0x3a0f, 0x58},
	{0x3a10, 0x50},
	{0x3a11, 0xa0},
	{0x3a1b, 0x5a},
	{0x3a1e, 0x4e},
	{0x3a1f, 0x28},
	{OV7695_TABLE_END, 0x0000}
};

static struct ov7695_reg ov7695_EV_minus_1[] = {
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a11, 0x88},
	{0x3a1b, 0x42},
	{0x3a1e, 0x36},
	{0x3a1f, 0x10},
	{OV7695_TABLE_END, 0x0000}
};

static struct ov7695_reg ov7695_EV_minus_2[] = {
	{0x3a0f, 0x38},
	{0x3a10, 0x30},
	{0x3a11, 0x80},
	{0x3a1b, 0x3a},
	{0x3a1e, 0x2e},
	{0x3a1f, 0x08},
	{OV7695_TABLE_END, 0x0000}
};

struct ov7695_info {
	struct miscdevice		miscdev_info;
	struct ov7695_power_rail	power;
	struct ov7695_sensordata	sensor_data;
	struct i2c_client		*i2c_client;
	struct clk			*mclk;
	struct ov7695_platform_data	*pdata;
	struct regmap			*regmap;
	atomic_t			in_use;
	const struct ov7695_reg		*mode;
#ifdef CONFIG_DEBUG_FS
	struct dentry			*debugfs_root;
	u32				debug_i2c_offset;
#endif
	u8				i2c_trans_buf[SIZEOF_I2C_TRANSBUF];
};

struct ov7695_mode_desc {
	u16			xres;
	u16			yres;
	const struct ov7695_reg *mode_tbl;
	struct ov7695_modeinfo	mode_info;
};

static struct ov7695_mode_desc mode_table[] = {
	{
		.xres = 640,
		.yres = 480,
		.mode_tbl = mode_640x480_30fps,
	},
	{ },
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static long ov7695_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg);

static inline void ov7695_msleep(u32 t)
{
	usleep_range(t*1000, t*1000 + 500);
}

static inline int ov7695_read_reg(struct ov7695_info *info, u16 addr, u8 *val)
{
	return regmap_read(info->regmap, addr, (unsigned int *) val);
}

static int ov7695_write_reg8(struct ov7695_info *info, u16 addr, u8 val)
{
	dev_dbg(&info->i2c_client->dev, "0x%x = 0x%x\n", addr, val);
	return regmap_write(info->regmap, addr, val);
}

static int ov7695_write_reg16(struct ov7695_info *info, u16 addr, u16 val)
{
	unsigned char data[2];

	data[0] = (u8) (val >> 8);
	data[1] = (u8) (val & 0xff);

	dev_dbg(&info->i2c_client->dev, "0x%x = 0x%x\n", addr, val);
	return regmap_raw_write(info->regmap, addr, data, sizeof(data));
}

static int ov7695_write_table(
	struct ov7695_info *info,
	const struct ov7695_reg table[])
{
	int err;
	const struct ov7695_reg *next;
	u16 val;

	dev_dbg(&info->i2c_client->dev, "yuv %s\n", __func__);

	for (next = table; next->addr != OV7695_TABLE_END; next++) {
		if (next->addr == OV7695_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		val = next->val;

		dev_dbg(&info->i2c_client->dev,
			"%s: addr = 0x%4x, val = 0x%2x\n",
			__func__, next->addr, val);
		err = ov7695_write_reg8(info, next->addr, val);
		if (err)
			return err;
	}
	return 0;
}

static void ov7695_mclk_disable(struct ov7695_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ov7695_mclk_enable(struct ov7695_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static int ov7695_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct miscdevice	*miscdev = file->private_data;
	struct ov7695_info *info = dev_get_drvdata(miscdev->parent);

	dev_dbg(&info->i2c_client->dev, "ov7695: open.\n");
	info = container_of(miscdev, struct ov7695_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		dev_err(&info->i2c_client->dev, "%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	err = ov7695_mclk_enable(info);
	if (!err && info->pdata && info->pdata->power_on) {
		err = info->pdata->power_on(&info->power);
	} else {
		dev_err(&info->i2c_client->dev,
			"%s:no valid power_on function.\n", __func__);
		err = -EFAULT;
	}
	if (err < 0)
		ov7695_mclk_disable(info);
	return err;
}

int ov7695_release(struct inode *inode, struct file *file)
{
	struct ov7695_info *info = file->private_data;

	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off(&info->power);
	ov7695_mclk_disable(info);
	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int ov7695_regulator_get(struct ov7695_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = devm_regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else {
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);
	}

	*vreg = reg;
	return err;
}

static int ov7695_power_get(struct ov7695_info *info)
{
	struct ov7695_power_rail *pw = &info->power;
	int err;

	dev_dbg(&info->i2c_client->dev, "ov7695: %s\n", __func__);

	/* note: ov7695 uses i2c address 0x42,
	 *
	 * This needs us to define a new vif2 as 2-0021
	 * for platform board file that uses ov7695
	 * otherwise below could not get the regulator
	 *
	 * This rails of "vif2" and "vana" can be modified as needed
	 * for a new platform.
	 *
	 * ov7695: need to get 1.8v first
	 */
	/* interface 1.8v */
	err = ov7695_regulator_get(info, &pw->iovdd, "vif2");
	if (unlikely(IS_ERR(ERR_PTR(err))))
		return err;

	/* ananlog 2.7v */
	err = ov7695_regulator_get(info, &pw->avdd, "vana");
	if (unlikely(IS_ERR(ERR_PTR(err))))
		return err;

	return 0;
}

static const struct file_operations ov7695_fileops = {
	.owner = THIS_MODULE,
	.open = ov7695_open,
	.unlocked_ioctl = ov7695_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ov7695_ioctl,
#endif
	.release = ov7695_release,
};

static struct miscdevice ov7695_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ov7695",
	.fops = &ov7695_fileops,
};

#ifdef CONFIG_DEBUG_FS
static int ov7695_stats_show(struct seq_file *s, void *data)
{
	static struct ov7695_info *info;

	seq_printf(s, "%-20s : %-20s\n", "Name", "ov7695-debugfs-testing");
	seq_printf(s, "%-20s : 0x%X\n", "Current i2c-offset Addr",
			info->debug_i2c_offset);
	return 0;
}

static int ov7695_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ov7695_stats_show, inode->i_private);
}

static const struct file_operations ov7695_stats_fops = {
	.open       = ov7695_stats_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int debug_i2c_offset_w(void *data, u64 val)
{
	struct ov7695_info *info = (struct ov7695_info *)(data);
	dev_dbg(&info->i2c_client->dev,
			"ov7695:%s setting i2c offset to 0x%X\n",
			__func__, (u32)val);
	info->debug_i2c_offset = (u32)val;
	dev_dbg(&info->i2c_client->dev,
			"ov7695:%s new i2c offset is 0x%X\n", __func__,
			info->debug_i2c_offset);
	return 0;
}

static int debug_i2c_offset_r(void *data, u64 *val)
{
	struct ov7695_info *info = (struct ov7695_info *)(data);
	*val = (u64)info->debug_i2c_offset;
	dev_dbg(&info->i2c_client->dev,
			"ov7695:%s reading i2c offset is 0x%X\n", __func__,
			info->debug_i2c_offset);
	return 0;
}

static int debug_i2c_read(void *data, u64 *val)
{
	struct ov7695_info *info = (struct ov7695_info *)(data);
	u8 temp1 = 0;
	u8 temp2 = 0;
	dev_dbg(&info->i2c_client->dev,
			"ov7695:%s reading offset 0x%X\n", __func__,
			info->debug_i2c_offset);
	if (ov7695_read_reg(info, info->debug_i2c_offset, &temp1)
		|| ov7695_read_reg(info, info->debug_i2c_offset+1, &temp2)) {
		dev_err(&info->i2c_client->dev,
				"ov7695:%s failed\n", __func__);
		return -EIO;
	}
	dev_dbg(&info->i2c_client->dev,
			"ov7695:%s read value is 0x%X\n", __func__,
			temp1<<8 | temp2);
	*val = (u64)(temp1<<8 | temp2);
	return 0;
}

static int debug_i2c_write(void *data, u64 val)
{
	struct ov7695_info *info = (struct ov7695_info *)(data);
	dev_dbg(&info->i2c_client->dev,
			"ov7695:%s writing 0x%X to offset 0x%X\n", __func__,
			(u16)val, info->debug_i2c_offset);
	if (ov7695_write_reg16(info, info->debug_i2c_offset, (u16)val)) {
		dev_err(&info->i2c_client->dev,
			"ov7695:%s failed\n", __func__);
		return -EIO;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2c_offset_fops, debug_i2c_offset_r,
		debug_i2c_offset_w, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_read_fops, debug_i2c_read,
		/*debug_i2c_dummy_w*/ NULL, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_write_fops, /*debug_i2c_dummy_r*/NULL,
		debug_i2c_write, "0x%llx\n");

static int ov7695_debug_init(struct ov7695_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s", __func__);

	info->debugfs_root = debugfs_create_dir(ov7695_device.name, NULL);

	if (!info->debugfs_root)
		goto err_out;

	if (!debugfs_create_file("stats", S_IRUGO,
			info->debugfs_root, info, &ov7695_stats_fops))
		goto err_out;

	if (!debugfs_create_file("i2c_offset", S_IRUGO | S_IWUSR,
			info->debugfs_root, info, &i2c_offset_fops))
		goto err_out;

	if (!debugfs_create_file("i2c_read", S_IRUGO,
			info->debugfs_root, info, &i2c_read_fops))
		goto err_out;

	if (!debugfs_create_file("i2c_write", S_IWUSR,
			info->debugfs_root, info, &i2c_write_fops))
		goto err_out;

	return 0;

err_out:
	dev_err(&info->i2c_client->dev, "ERROR:%s failed", __func__);
	debugfs_remove_recursive(info->debugfs_root);
	return -ENOMEM;
}
#endif	/* CONFIG_DEBUG_FS */

static struct ov7695_modeinfo def_modeinfo = {
	.xres = 640,
	.yres = 480,
};

static struct ov7695_mode_desc *ov7695_get_mode(
	struct ov7695_info *info, struct ov7695_mode *mode)
{
	struct ov7695_mode_desc *mt = mode_table;

	while (mt->xres) {
		if ((mt->xres == mode->xres) &&
			(mt->yres == mode->yres))
				break;
		mt++;
	}

	if (!mt->xres)
		mt = NULL;
	return mt;
}

static int ov7695_mode_info_init(struct ov7695_info *info)
{
	struct ov7695_mode_desc *md = mode_table;
	const struct ov7695_reg *mt;
	struct ov7695_modeinfo *mi;

	dev_dbg(&info->i2c_client->dev, "%s", __func__);
	while (md->xres) {
		mi = &md->mode_info;
		mt = md->mode_tbl;
		memcpy(mi, &def_modeinfo, sizeof(*mi));
		dev_dbg(&info->i2c_client->dev, "mode %d x %d ",
			md->xres, md->yres);
		mi->xres = md->xres;
		mi->yres = md->yres;
		md++;
	}
	return 0;
}

static int ov7695_set_mode(struct ov7695_info *info,
	struct ov7695_mode *mode)
{
	struct ov7695_mode_desc *sensor_mode;
	int err;

	dev_info(&info->i2c_client->dev,
		"%s: xres %u yres %u\n", __func__, mode->xres, mode->yres);

	sensor_mode = ov7695_get_mode(info, mode);
	if (sensor_mode == NULL) {
		dev_err(&info->i2c_client->dev,
			"%s: invalid params supplied to set mode %d %d\n",
				__func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	err = ov7695_write_table(
		info, sensor_mode->mode_tbl);
	if (err)
		return err;

	info->mode = sensor_mode->mode_tbl;

	return 0;
}

static long ov7695_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct ov7695_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(OV7695_SENSOR_IOCTL_SET_MODE):
	{
		struct ov7695_mode mode;

		dev_dbg(&info->i2c_client->dev, "OV7695_IOCTL_SET_MODE\n");
		if (copy_from_user(&mode, (const void __user *)arg,
			sizeof(struct ov7695_mode))) {
			err = -EFAULT;
			break;
		}
		err = ov7695_set_mode(info, &mode);
		break;
	}

	case _IOC_NR(OV7695_SENSOR_IOCTL_SET_WHITE_BALANCE):
	{
		u8 whitebalance;

		if (copy_from_user(&whitebalance, (const void __user *)arg,
			sizeof(whitebalance))) {
			return -EFAULT;
		}
		switch (whitebalance) {
		case OV7695_YUV_Whitebalance_Auto:
			err = ov7695_write_table(info,
					ov7695_Whitebalance_Auto);
			break;
		case OV7695_YUV_Whitebalance_Daylight:
			err = ov7695_write_table(info,
					ov7695_Whitebalance_Daylight);
			break;
		case OV7695_YUV_Whitebalance_CloudyDaylight:
			err = ov7695_write_table(info,
					ov7695_Whitebalance_Cloudy);
			break;
		case OV7695_YUV_Whitebalance_Incandescent:
			err = ov7695_write_table(info,
					ov7695_Whitebalance_Incandescent);
			break;
		case OV7695_YUV_Whitebalance_Fluorescent:
			err = ov7695_write_table(info,
					ov7695_Whitebalance_Fluorescent);
			break;
		default:
			/* unsupported white balance mode*/
			break;
		}

		if (err)
			return err;

		return 0;
	}

	case _IOC_NR(OV7695_SENSOR_IOCTL_SET_EV):
	{
		short ev;

		if (copy_from_user(&ev,
				(const void __user *)arg,
				sizeof(short)))
			return -EFAULT;
		switch (ev) {
		case 0:
			err = ov7695_write_table(info, ov7695_EV_zero);
			break;
		case 1:
			err = ov7695_write_table(info, ov7695_EV_plus_1);
			break;
		case 2:
			err = ov7695_write_table(info, ov7695_EV_plus_2);
			break;
		case -1:
			err = ov7695_write_table(info, ov7695_EV_minus_1);
			break;
		case -2:
			err = ov7695_write_table(info, ov7695_EV_minus_2);
			break;
		default:
			/* unsupported EV setting */
			break;
		}

		if (err)
			return err;

		return 0;
	}

	case _IOC_NR(OV7695_SENSOR_IOCTL_GET_EV):
	{
		short ev;
		u8 val;

		err = ov7695_read_reg(info, 0x3a0f, &val);

		if (err) {
			dev_err(&info->i2c_client->dev,
				"IOCTL_GET_EV fail: err= 0x%x\n", err);
			return err;
		}

		if (val == 0x38)
			ev = -2;
		else if (val == 0x40)
			ev = -1;
		else if (val == 0x48)
			ev = 0;
		else if (val == 0x50)
			ev = 1;
		else if (val == 0x58)
			ev = 2;

		if (copy_to_user((void __user *)arg, &ev, sizeof(short)))
			return -EFAULT;

		return 0;
	}

	default:
		dev_dbg(&info->i2c_client->dev, "INVALID IOCTL\n");
		err = -EINVAL;
	}

	if (err)
		dev_err(&info->i2c_client->dev,
			"%s - %x: ERR = %d\n", __func__, cmd, err);
	return err;
}

static int ov7695_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct ov7695_info *info;
	const char *mclk_name;
	dev_dbg(&client->dev, "ov7695: probing sensor.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(info->regmap));
		return -ENODEV;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = NULL;

	i2c_set_clientdata(client, info);

	err = ov7695_power_get(info);
	if (err) {
		dev_err(&info->i2c_client->dev,
			"ov7695: Unable to get regulators\n");
		return err;
	}
	ov7695_mode_info_init(info);

	memcpy(&info->miscdev_info,
		&ov7695_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		dev_err(&info->i2c_client->dev,
			"ov7695: Unable to register misc device!\n");
		return err;
	}

	mclk_name = info->pdata && info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

#ifdef CONFIG_DEBUG_FS
	ov7695_debug_init(info);
#endif

	return 0;
}

static int ov7695_remove(struct i2c_client *client)
{
	struct ov7695_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&ov7695_device);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(info->debugfs_root);
#endif

	kfree(info);

	return 0;
}

static const struct i2c_device_id ov7695_id[] = {
	{ "ov7695", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ov7695_id);

static struct i2c_driver ov7695_i2c_driver = {
	.driver = {
		.name = "ov7695",
		.owner = THIS_MODULE,
	},
	.probe = ov7695_probe,
	.remove = ov7695_remove,
	.id_table = ov7695_id,
};

static int __init ov7695_init(void)
{
	pr_info("ov7695 sensor driver loading\n");
	return i2c_add_driver(&ov7695_i2c_driver);
}

static void __exit ov7695_exit(void)
{
	i2c_del_driver(&ov7695_i2c_driver);
}

module_init(ov7695_init);
module_exit(ov7695_exit);
MODULE_LICENSE("GPL v2");
