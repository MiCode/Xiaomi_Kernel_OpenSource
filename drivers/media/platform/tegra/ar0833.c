/*
 * ar0833.c - ar0833 sensor driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION, All Rights Reserved.
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
#include <linux/export.h>
#include <linux/module.h>

#include <media/ar0833.h>

#define SIZEOF_I2C_TRANSBUF 128

struct ar0833_reg {
	u16 addr;
	u16 val;
};

struct ar0833_reg_blob {
	u16 addr;
	u16 size;
	u8 *data;
};

struct ar0833_info {
	struct miscdevice		miscdev_info;
	struct ar0833_power_rail	power;
	struct ar0833_sensordata	sensor_data;
	struct i2c_client		*i2c_client;
	struct ar0833_platform_data	*pdata;
	struct clk			*mclk;
	atomic_t			in_use;
	const struct ar0833_reg		*mode;
#ifdef CONFIG_DEBUG_FS
	struct dentry			*debugfs_root;
	u32				debug_i2c_offset;
	int				enableDCBLC;
#endif
	u8				i2c_trans_buf[SIZEOF_I2C_TRANSBUF];
};

#define AR0833_TABLE_WAIT_MS	0xC000
#define AR0833_TABLE_NOP	0xC001
#define AR0833_TABLE_CALL	0xC002
#define AR0833_TABLE_BLOB	0xC003
#define AR0833_TABLE_END	0xC004
#define AR0833_TABLE_8BIT	0x8000

#include "ar0833_mode_tbls.c"

struct ar0833_mode_desc {
	u16			xres;
	u16			yres;
	u8			hdr_en;
	const struct ar0833_reg *mode_tbl;
	struct ar0833_modeinfo	mode_info;
};

static struct ar0833_mode_desc mode_table[] = {
	{
		.xres = 3264,
		.yres = 2448,
		.hdr_en = 0,
		.mode_tbl = mode_3264x2448_30fps,
	},
	{
		.xres = 3264,
		.yres = 2448,
		.hdr_en = 1,
		.mode_tbl = mode_3264x2448_HDR_30fps,
	},
	{
		.xres = 1920,
		.yres = 1080,
		.hdr_en = 0,
		.mode_tbl = mode_1920x1080_30fps,
	},
	{
		.xres = 1920,
		.yres = 1080,
		.hdr_en = 1,
		.mode_tbl = mode_1920x1080_HDR_30fps,
	},
	{
		.xres = 3264,
		.yres = 1836,
		.hdr_en = 0,
		.mode_tbl = mode_3264x1836_30fps,
	},
	{
		.xres = 3264,
		.yres = 1836,
		.hdr_en = 1,
		.mode_tbl = mode_3264x1836_HDR_30fps,
	},
	{ },
};

static long ar0833_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg);

static inline void ar0833_msleep(u32 t)
{
	usleep_range(t*1000, t*1000 + 500);
}

static inline void ar0833_get_coarse_time_regs(struct ar0833_reg *regs,
						u32 coarse_time)
{
	regs->addr = 0x0202;
	regs->val = coarse_time & 0xFFFF;
}

static inline void ar0833_get_coarse_time_short_regs(struct ar0833_reg *regs,
						u32 coarse_time)
{
	regs->addr = 0x3088;
	regs->val = coarse_time & 0xFFFF;
}

static inline void ar0833_get_gain_reg(struct ar0833_reg *regs, u16 gain)
{
	regs->addr = 0x305E;
	regs->val = gain & 0xFFFF;
}

static int ar0833_read_reg(struct i2c_client *client, u16 addr, u8 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	*val = data[2];

	return 0;
}

static int ar0833_write_bulk_reg(struct i2c_client *client, u8 *data, int len)
{
	int err;
	struct i2c_msg msg;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	dev_dbg(&client->dev,
		"%s {0x%04x,", __func__, (int)data[0] << 8 | data[1]);
	for (err = 2; err < len; err++)
		dev_dbg(&client->dev, " 0x%02x", data[err]);
	dev_dbg(&client->dev, "},\n");

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	dev_err(&client->dev, "ar0833: i2c bulk transfer failed at %x\n",
		(int)data[0] << 8 | data[1]);

	return err;
}

static int ar0833_write_reg8(struct i2c_client *client, u16 addr, u8 val)
{
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	dev_dbg(&client->dev, "0x%x = 0x%x\n", addr, val);
	return ar0833_write_bulk_reg(client, data, sizeof(data));
}

static int ar0833_write_reg16(struct i2c_client *client, u16 addr, u16 val)
{
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val >> 8);
	data[3] = (u8) (val & 0xff);

	dev_dbg(&client->dev, "0x%x = 0x%x\n", addr, val);
	return ar0833_write_bulk_reg(client, data, sizeof(data));
}

/* flush and reset buffer */
static int ar0833_flash_buffer(struct ar0833_info *info, int *len)
{
	int err;

	if (!(*len))
		return 0;

	err = ar0833_write_bulk_reg(info->i2c_client,
				info->i2c_trans_buf, *len);

	*len = 0;
	return err;
}

static int ar0833_write_blob(
	struct ar0833_info *info, struct ar0833_reg_blob *pblob)
{
	u8 *pdata = pblob->data;
	u16 addr = pblob->addr;
	u16 size = pblob->size;
	int err = 0;
	u16 blk;

	dev_dbg(&info->i2c_client->dev, "ar0833_write_blob ++\n");
	while (size) {
		blk = size > sizeof(info->i2c_trans_buf) - 2 ?
			sizeof(info->i2c_trans_buf) - 2 : size;
		info->i2c_trans_buf[0] = addr >> 8;
		info->i2c_trans_buf[1] = (u8)addr;
		memcpy(info->i2c_trans_buf + 2, pdata, blk);
		err = ar0833_write_bulk_reg(info->i2c_client,
			info->i2c_trans_buf, blk + 2);
		if (err)
			break;

		size -= blk;
		pdata += blk;
		addr += blk;
	}
	dev_dbg(&info->i2c_client->dev, "ar0833_write_blob -- %d\n", err);
	return err;
}

static int ar0833_write_table(
	struct ar0833_info *info,
	const struct ar0833_reg table[],
	const struct ar0833_reg override_list[],
	int num_override_regs)
{
	int err = 0;
	const struct ar0833_reg *next, *n_next;
	u8 *b_ptr = info->i2c_trans_buf;
	unsigned int buf_filled = 0;
	int i;
	u16 val;

	dev_dbg(&info->i2c_client->dev, "%s ++\n", __func__);
	for (next = table; !err; next++) {
		switch (next->addr) {
		case AR0833_TABLE_END:
			dev_dbg(&info->i2c_client->dev, "ar0833_table_end\n");
			err = ar0833_flash_buffer(info, &buf_filled);
			return err;
		case AR0833_TABLE_NOP:
			continue;
		case AR0833_TABLE_WAIT_MS:
			dev_dbg(&info->i2c_client->dev,
				"ar0833_wait_ms %d\n", next->val);
			err = ar0833_flash_buffer(info, &buf_filled);
			if (err < 0)
				return err;
			ar0833_msleep(next->val);
			continue;
		case AR0833_TABLE_CALL:
			err = ar0833_flash_buffer(info, &buf_filled);
			if (next->val >= NUM_OF_SUBTBLS) {
				dev_err(&info->i2c_client->dev,
					"%s: invalid tbl index %d\n",
					__func__, next->val);
				return -EFAULT;
			}
			if (err < 0)
				return err;

			err = ar0833_write_table(
				info, sub_tbls[next->val], NULL, 0);
			if (err < 0)
				return err;
			continue;
		case AR0833_TABLE_BLOB:
			err = ar0833_flash_buffer(info, &buf_filled);
			if (next->val >= NUM_OF_SUBTBLS) {
				dev_err(&info->i2c_client->dev,
					"%s: invalid tbl index %d\n",
					__func__, next->val);
				return -EFAULT;
			}
			if (err < 0)
				return err;

			err = ar0833_write_blob(info, sub_tbls[next->val]);
			if (err < 0)
				return err;
			continue;
		}

		val = next->val;
		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list	*/
		if (override_list)
			for (i = 0; i < num_override_regs; i++)
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}

		if (!buf_filled) {
			b_ptr = info->i2c_trans_buf;
			*b_ptr++ = (next->addr & ~AR0833_TABLE_8BIT) >> 8;
			*b_ptr++ = next->addr & 0xff;
			buf_filled = 2;
		}
		if (!(next->addr & AR0833_TABLE_8BIT)) {
			*b_ptr++ = (u8)(val >> 8);
			buf_filled++;
		}
		*b_ptr++ = (u8)val;
		buf_filled++;

		n_next = next + 1;
		if (buf_filled < (sizeof(info->i2c_trans_buf) & 0xFFFE) &&
			n_next->addr == next->addr + 2)
			continue;
		err = ar0833_flash_buffer(info, &buf_filled);
		if (err < 0)
			return err;
	}
	dev_dbg(&info->i2c_client->dev, "%s --\n", __func__);
	return 0;
}

static int ar0833_set_coarse_time(struct ar0833_info *info, u32 coarse_time,
					bool group_hold)
{
	int ret;
	struct ar0833_reg reg_list;

	ar0833_get_coarse_time_regs(&reg_list, coarse_time);

	if (group_hold) {
		ret = ar0833_write_reg16(info->i2c_client, 0x0104, 0x1);
		if (ret)
			return ret;
	}

	ret = ar0833_write_reg16(info->i2c_client, reg_list.addr,
		reg_list.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = ar0833_write_reg16(info->i2c_client, 0x0104, 0x0);
		if (ret)
			return ret;
	}
	return ret;
}

static int ar0833_set_hdr_coarse_time(struct ar0833_info *info,
				 struct ar0833_hdr *values,
				 bool group_hold)
{
	int ret;
	struct ar0833_reg reg_list;
	struct ar0833_reg reg_list_short;

	ar0833_get_coarse_time_regs(&reg_list, values->coarse_time_long);
	ar0833_get_coarse_time_short_regs(&reg_list_short,
					values->coarse_time_short);

	if (group_hold) {
		ret = ar0833_write_reg16(info->i2c_client, 0x0104, 0x1);
		if (ret)
			return ret;
	}

	ret = ar0833_write_reg16(info->i2c_client, reg_list.addr,
		reg_list.val);
	if (ret)
		return ret;

	ret = ar0833_write_reg16(info->i2c_client, reg_list_short.addr,
		reg_list_short.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = ar0833_write_reg16(info->i2c_client, 0x0104, 0x0);
		if (ret)
			return ret;
	}
	return ret;
}

static int ar0833_set_gain(struct ar0833_info *info, u16 gain,
				bool group_hold)
{
	int ret;
	struct ar0833_reg reg_list;

	ar0833_get_gain_reg(&reg_list, gain);

	if (group_hold) {
		ret = ar0833_write_reg16(info->i2c_client, 0x0104, 0x1);
		if (ret)
			return ret;
	}

	ret = ar0833_write_reg16(info->i2c_client, reg_list.addr,
		reg_list.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = ar0833_write_reg16(info->i2c_client, 0x0104, 0x0);
		if (ret)
			return ret;
	}
	return ret;
}

static int
ar0833_set_group_hold(struct ar0833_info *info, struct ar0833_ae *ae)
{
	int ret;
	int count = 0;
	bool groupHoldEnabled = false;
	struct ar0833_hdr values;

	values.coarse_time_long = ae->coarse_time;
	values.coarse_time_short = ae->coarse_time_short;

	if (ae->gain_enable)
		count++;
	if (ae->coarse_time_enable)
		count++;
	if (count >= 1)
		groupHoldEnabled = true;

	if (groupHoldEnabled) {
		ret = ar0833_write_reg16(info->i2c_client, 0x104, 0x1);
		if (ret)
			return ret;
	}

	if (ae->gain_enable)
		ar0833_set_gain(info, ae->gain, false);
	if (ae->coarse_time_enable)
		ar0833_set_hdr_coarse_time(info, &values, false);

	if (groupHoldEnabled) {
		ret = ar0833_write_reg16(info->i2c_client, 0x104, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static int ar0833_get_status(struct ar0833_info *info, u8 *status)
{
	int err;

	err = ar0833_read_reg(info->i2c_client, 0x380e, status);
	return err;
}

static void ar0833_mclk_disable(struct ar0833_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ar0833_mclk_enable(struct ar0833_info *info)
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

static int ar0833_open(struct inode *inode, struct file *file)
{
	int err;
	struct miscdevice	*miscdev = file->private_data;
	struct ar0833_info	*info;

	info = container_of(miscdev, struct ar0833_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		dev_info(&info->i2c_client->dev, "%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	err = ar0833_mclk_enable(info);
	if (err)
		return err;

	if (info->pdata && info->pdata->power_on)
		err = info->pdata->power_on(&info->power);
	else {
		dev_err(&info->i2c_client->dev,
			"%s:no valid power_on function.\n", __func__);
		err = -EEXIST;
	}

	if (err < 0)
		ar0833_mclk_disable(info);

	return err;
}

int ar0833_release(struct inode *inode, struct file *file)
{
	struct ar0833_info *info = file->private_data;

	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off(&info->power);

	ar0833_mclk_disable(info);

	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int ar0833_regulator_get(struct ar0833_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int ar0833_power_get(struct ar0833_info *info)
{
	struct ar0833_power_rail *pw = &info->power;

	ar0833_regulator_get(info, &pw->avdd, "vana"); /* ananlog 2.7v */
	ar0833_regulator_get(info, &pw->dvdd, "vdig"); /* digital 1.2v */
	ar0833_regulator_get(info, &pw->iovdd, "vif"); /* interface 1.8v */

	return 0;
}

static const struct file_operations ar0833_fileops = {
	.owner = THIS_MODULE,
	.open = ar0833_open,
	.unlocked_ioctl = ar0833_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ar0833_ioctl,
#endif
	.release = ar0833_release,
};

static struct miscdevice ar0833_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ar0833",
	.fops = &ar0833_fileops,
};

#ifdef CONFIG_DEBUG_FS
int control_DC_BLC(struct ar0833_info *info)
{
	u8 reg0x3d0a;
	u8 reg0x3d0b;
	u8 reg0x4006;

	if (info->enableDCBLC) {
		ar0833_write_reg8(info->i2c_client, 0x3d84, 0xdf);
		ar0833_write_reg8(info->i2c_client, 0x3d81, 0x01);
		ar0833_msleep(10);

		ar0833_read_reg(info->i2c_client, 0x3d0a, &reg0x3d0a);
		ar0833_read_reg(info->i2c_client, 0x3d0b, &reg0x3d0b);
		ar0833_write_reg8(info->i2c_client, 0x3d81, 0x00);

		if ((reg0x3d0b > 0x10 && reg0x3d0b < 0x20))
			reg0x4006 = reg0x3d0b;
		else if ((reg0x3d0a > 0x10) && (reg0x3d0a < 0x20))
			reg0x4006 = reg0x3d0a;
		else
			reg0x4006 = 0x20;

		ar0833_write_reg8(info->i2c_client, 0x4006, reg0x4006);
		dev_info(&info->i2c_client->dev,
				"ar0833: %s: wrote the DC BLC commands\n",
				__func__);
	} else {
		dev_info(&info->i2c_client->dev,
				"ar0833: %s: DID NOT do the DC BLC commands\n",
				__func__);
	}

	return 0;
}

static int ar0833_stats_show(struct seq_file *s, void *data)
{
	static struct ar0833_info *info;

	seq_printf(s, "%-20s : %-20s\n", "Name", "ar0833-debugfs-testing");
	seq_printf(s, "%-20s : 0x%X\n", "Current i2c-offset Addr",
			info->debug_i2c_offset);
	seq_printf(s, "%-20s : 0x%X\n", "DC BLC Enabled",
			info->debug_i2c_offset);
	return 0;
}

static int ar0833_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ar0833_stats_show, inode->i_private);
}

static const struct file_operations ar0833_stats_fops = {
	.open       = ar0833_stats_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int debug_i2c_offset_w(void *data, u64 val)
{
	struct ar0833_info *info = (struct ar0833_info *)(data);
	dev_info(&info->i2c_client->dev,
			"ar0833:%s setting i2c offset to 0x%X\n",
			__func__, (u32)val);
	info->debug_i2c_offset = (u32)val;
	dev_info(&info->i2c_client->dev,
			"ar0833:%s new i2c offset is 0x%X\n", __func__,
			info->debug_i2c_offset);
	return 0;
}

static int debug_i2c_offset_r(void *data, u64 *val)
{
	struct ar0833_info *info = (struct ar0833_info *)(data);
	*val = (u64)info->debug_i2c_offset;
	dev_info(&info->i2c_client->dev,
			"ar0833:%s reading i2c offset is 0x%X\n", __func__,
			info->debug_i2c_offset);
	return 0;
}

static int debug_i2c_read(void *data, u64 *val)
{
	struct ar0833_info *info = (struct ar0833_info *)(data);
	u8 temp1 = 0;
	u8 temp2 = 0;
	dev_info(&info->i2c_client->dev,
			"ar0833:%s reading offset 0x%X\n", __func__,
			info->debug_i2c_offset);
	if (ar0833_read_reg(info->i2c_client,
				info->debug_i2c_offset, &temp1)
		|| ar0833_read_reg(info->i2c_client,
			info->debug_i2c_offset+1, &temp2)) {
		dev_err(&info->i2c_client->dev,
				"ar0833:%s failed\n", __func__);
		return -EIO;
	}
	dev_info(&info->i2c_client->dev,
			"ar0833:%s read value is 0x%X\n", __func__,
			temp1<<8 | temp2);
	*val = (u64)(temp1<<8 | temp2);
	return 0;
}

static int debug_i2c_write(void *data, u64 val)
{
	struct ar0833_info *info = (struct ar0833_info *)(data);
	dev_info(&info->i2c_client->dev,
			"ar0833:%s writing 0x%X to offset 0x%X\n", __func__,
			(u16)val, info->debug_i2c_offset);
	if (ar0833_write_reg16(info->i2c_client,
				info->debug_i2c_offset, (u16)val)) {
		dev_err(&info->i2c_client->dev, "ar0833:%s failed\n", __func__);
		return -EIO;
	}
	return 0;
}

static int debug_dcblc_r(void *data, u64 *val)
{
	struct ar0833_info *info = (struct ar0833_info *)(data);
	*val = (u64)info->enableDCBLC;
	dev_info(&info->i2c_client->dev,
			"ar0833:%s read DC BLC [%d]\n", __func__,
			info->enableDCBLC);

	return 0;
}

static int debug_dcblc_w(void *data, u64 val)
{
	struct ar0833_info *info = (struct ar0833_info *)(data);
	if (val != 0) {
		info->enableDCBLC = 1;
		dev_info(&info->i2c_client->dev,
				"ar0833:%s enabled DC BLC\n", __func__);
	} else {
		info->enableDCBLC = 0;
		dev_info(&info->i2c_client->dev,
				"ar0833:%s disabled DC BLC\n", __func__);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_dcblc_fops, debug_dcblc_r,
		debug_dcblc_w, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_offset_fops, debug_i2c_offset_r,
		debug_i2c_offset_w, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_read_fops, debug_i2c_read,
		/*debug_i2c_dummy_w*/ NULL, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_write_fops, /*debug_i2c_dummy_r*/NULL,
		debug_i2c_write, "0x%llx\n");

static int ar0833_debug_init(struct ar0833_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s", __func__);

	info->debugfs_root = debugfs_create_dir(ar0833_device.name, NULL);

	if (!info->debugfs_root)
		goto err_out;

	if (!debugfs_create_file("stats", S_IRUGO,
			info->debugfs_root, info, &ar0833_stats_fops))
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

	if (!debugfs_create_file("DCBLC", S_IRUGO | S_IWUSR,
			info->debugfs_root, info, &debug_dcblc_fops))
		goto err_out;

	return 0;

err_out:
	dev_err(&info->i2c_client->dev, "ERROR:%s failed", __func__);
	if (info->debugfs_root)
		debugfs_remove_recursive(info->debugfs_root);
	return -ENOMEM;
}
#endif

static struct ar0833_modeinfo def_modeinfo = {
	.xres = 3264,
	.yres = 2448,
	.hdr = 0,
	.lanes = 4,
	.line_len = 0x0f68,
	.frame_len = 0x0a01,
	.coarse_time = 2400,
	.coarse_time_2nd = 300,
	.xstart = 8,
	.xend = 0x0cc7,
	.ystart = 8,
	.yend = 0x0997,
	.xsize = 0x0cc0,
	.ysize = 0x0990,
	.gain = 0x1000,
	.x_flip = 1,
	.y_flip = 0,
	.x_bin = 1,
	.y_bin = 1,
	.vt_pix_clk_div = 5,
	.vt_sys_clk_div = 1,
	.pre_pll_clk_div = 2,
	.pll_multi = 0x40,
	.op_pix_clk_div = 0x0a,
	.op_sys_clk_div = 1,
};

static struct ar0833_mode_desc *ar0833_get_mode(
	struct ar0833_info *info, struct ar0833_mode *mode)
{
	struct ar0833_mode_desc *mt = mode_table;

	while (mt->xres) {
		if ((mt->xres == mode->xres) &&
			(mt->yres == mode->yres) &&
			(mt->hdr_en == mode->hdr_en))
				break;
		mt++;
	}

	if (!mt->xres)
		mt = NULL;
	return mt;
}

static int ar0833_mode_info_init(struct ar0833_info *info)
{
	struct ar0833_mode_desc *md = mode_table;
	const struct ar0833_reg *mt;
	struct ar0833_modeinfo *mi;

	dev_dbg(&info->i2c_client->dev, "%s", __func__);
	while (md->xres) {
		mi = &md->mode_info;
		mt = md->mode_tbl;
		memcpy(mi, &def_modeinfo, sizeof(*mi));
		dev_dbg(&info->i2c_client->dev, "mode %d x %d %s",
			md->xres, md->yres, md->hdr_en ? "HDR" : "REG");
		mi->xres = md->xres;
		mi->yres = md->yres;
		mi->hdr = md->hdr_en;
		while (mt->addr != AR0833_TABLE_END) {
			switch (mt->addr) {
			case 0x0300:
				mi->vt_pix_clk_div = mt->val;
				break;
			case 0x0302:
				mi->vt_sys_clk_div = mt->val;
				break;
			case 0x0304:
				mi->pre_pll_clk_div = mt->val;
				break;
			case 0x0306:
				mi->pll_multi = mt->val;
				break;
			case 0x0308:
				mi->op_pix_clk_div = mt->val;
				break;
			case 0x030a:
				mi->op_sys_clk_div = mt->val;
				break;
			case 0x0202:
				mi->coarse_time = mt->val;
				break;
			case 0x3088:
				mi->coarse_time_2nd = mt->val;
				break;
			case 0x0340:
				mi->frame_len = mt->val;
				break;
			case 0x0342:
				mi->line_len = mt->val;
				break;
			case 0x0344:
				mi->xstart = mt->val;
				break;
			case 0x0346:
				mi->ystart = mt->val;
				break;
			case 0x0348:
				mi->xend = mt->val;
				break;
			case 0x034a:
				mi->yend = mt->val;
				break;
			case 0x034c:
				mi->xsize = mt->val;
				break;
			case 0x034e:
				mi->ysize = mt->val;
				break;
			case 0x305e:
				mi->gain = mt->val;
				break;
			case 0x31ae:
				mi->lanes = mt->val & 0x7;
				break;
			case 0x3040:
				if (mt->val & 0x8000)
					mi->y_flip = 1;
				if (mt->val & 0x4000)
					mi->x_flip = 1;
				switch (mt->val & 0x1c0) {
				case 1:
					mi->x_bin = 1;
					break;
				case 3:
					mi->x_bin = 2;
					break;
				case 7:
					mi->x_bin = 4;
					break;
				default:
					dev_warn(&info->i2c_client->dev,
						"%s :Unrecognized x_odd_inc"
						"setting in mode %d x %d %s,"
						" 0x3040 = 0x%x\n",
						__func__, md->xres, md->yres,
						md->hdr_en ? "HDR" : "REG",
						mt->val);
					break;
				}

				switch (mt->val & 0x3f) {
				case 1:
					mi->y_bin = 1;
					break;
				case 3:
					mi->y_bin = 2;
					break;
				case 7:
					mi->y_bin = 4;
					break;
				case 15:
					mi->y_bin = 8;
					break;
				case 31:
					mi->y_bin = 16;
					break;
				case 63:
					mi->y_bin = 32;
					break;
				default:
					dev_warn(&info->i2c_client->dev,
						"%s :Unrecognized y_odd_inc"
						"setting in mode %d x %d %s,"
						" 0x3040 = 0x%x\n",
						__func__, md->xres, md->yres,
						md->hdr_en ? "HDR" : "REG",
						mt->val);
					break;
				}
				break;
			};
			mt++;
		};
		md++;
	}
	return 0;
}

static struct ar0833_modeinfo *ar0833_get_mode_info(
	struct ar0833_info *info, struct ar0833_modeinfo *mi)
{
	struct ar0833_mode mode;
	struct ar0833_mode_desc *mode_desc;

	mode.xres = mi->xres;
	mode.yres = mi->yres;
	mode.hdr_en = mi->hdr;
	mode_desc = ar0833_get_mode(info, &mode);
	if (mode_desc == NULL) {
		dev_err(&info->i2c_client->dev,
			"%s: invalid params to get mode info %d %d %d\n",
			__func__, mi->xres, mi->yres, mi->hdr);
		return NULL;
	}

	return &mode_desc->mode_info;
}

static int ar0833_set_mode(struct ar0833_info *info, struct ar0833_mode *mode)
{
	struct ar0833_mode_desc *sensor_mode;
	struct ar0833_reg reg_list[4];
	int err;

	dev_info(&info->i2c_client->dev,
		"%s: xres %u yres %u hdr %d\n",
		__func__, mode->xres, mode->yres, mode->hdr_en);
	dev_info(&info->i2c_client->dev,
		"framelength %u coarsetime %u gain %x\n",
		mode->frame_length, mode->coarse_time, mode->gain);

	sensor_mode = ar0833_get_mode(info, mode);
	if (sensor_mode == NULL) {
		dev_err(&info->i2c_client->dev,
				"%s: invalid params supplied to set mode %d %d %d\n",
				__func__, mode->xres, mode->yres, mode->hdr_en);
		return -EINVAL;
	}

	if (mode->hdr_en == 1)  /* if HDR is enabled */
		dev_info(&info->i2c_client->dev, "ar0833 HDR enabled\n");
	else
		dev_info(&info->i2c_client->dev, "ar0833 HDR disabled\n");

	memset(reg_list, 0, sizeof(reg_list));
	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain.	*/
	ar0833_get_coarse_time_regs(reg_list, mode->coarse_time);
	ar0833_get_gain_reg(reg_list + 1, mode->gain);
	if (mode->hdr_en == 1)  /* if HDR is enabled */
		ar0833_get_coarse_time_short_regs(
			reg_list + 2, mode->coarse_time_short);

	err = ar0833_write_table(
		info, sensor_mode->mode_tbl, reg_list, mode->hdr_en ? 3 : 2);
	if (err)
		return err;

	info->mode = sensor_mode->mode_tbl;

#ifdef CONFIG_DEBUG_FS
	control_DC_BLC(info);
#endif

	return 0;
}

static long ar0833_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct ar0833_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(AR0833_IOCTL_SET_MODE):
	{
		struct ar0833_mode mode;

		dev_dbg(&info->i2c_client->dev, "AR0833_IOCTL_SET_MODE\n");
		if (copy_from_user(&mode, (const void __user *)arg,
			sizeof(struct ar0833_mode))) {
			err = -EFAULT;
			break;
		}
		err = ar0833_set_mode(info, &mode);
		break;
	}
	case _IOC_NR(AR0833_IOCTL_SET_FRAME_LENGTH):
		dev_dbg(&info->i2c_client->dev,
			"AR0833_IOCTL_SET_FRAME_LENGTH %x\n", (u32)arg);
		/* obsolete. we should not update frame length,
		   it is done by sensor automatically */
		break;
	case _IOC_NR(AR0833_IOCTL_SET_COARSE_TIME):
		dev_dbg(&info->i2c_client->dev,
			"AR0833_IOCTL_SET_COARSE_TIME %x\n", (u32)arg);
		err = ar0833_set_coarse_time(info, (u32)arg, true);
		break;
	case _IOC_NR(AR0833_IOCTL_SET_HDR_COARSE_TIME):
	{
		struct ar0833_hdr values;

		dev_dbg(&info->i2c_client->dev,
			"AR0833_IOCTL_SET_HDR_COARSE_TIME\n");
		if (copy_from_user(&values,
					 (const void __user *)arg,
					 sizeof(struct ar0833_hdr))) {
			err = -EFAULT;
			break;
		}
		err = ar0833_set_hdr_coarse_time(info, &values, true);
		break;
	}
	case _IOC_NR(AR0833_IOCTL_SET_GAIN):
		dev_dbg(&info->i2c_client->dev,
			"AR0833_IOCTL_SET_GAIN %x\n", (u32)arg);
		err = ar0833_set_gain(info, (u16)arg, true);
		break;
	case _IOC_NR(AR0833_IOCTL_SET_GROUP_HOLD):
	{
		struct ar0833_ae ae;
		if (copy_from_user(&ae, (const void __user *)arg,
				sizeof(struct ar0833_ae))) {
			dev_err(&info->i2c_client->dev,
				"%s:fail group hold\n", __func__);
			return -EFAULT;
		}
		return ar0833_set_group_hold(info, &ae);
	}
	case _IOC_NR(AR0833_IOCTL_GET_STATUS):
	{
		u8 status;

		dev_dbg(&info->i2c_client->dev, "AR0833_IOCTL_GET_STATUS\n");
		err = ar0833_get_status(info, &status);
		if (err)
			break;
		if (copy_to_user((void __user *)arg, &status, 2)) {
			err = -EFAULT;
			break;
		}
		break;
	}
	case _IOC_NR(AR0833_IOCTL_GET_MODE):
	{
		struct ar0833_modeinfo mode_info, *mi;

		dev_dbg(&info->i2c_client->dev, "AR0833_IOCTL_GET_MODE\n");
		if (copy_from_user(&mode_info,
					 (const void __user *)arg,
					 sizeof(struct ar0833_mode))) {
			err = -EFAULT;
			break;
		}
		mi = ar0833_get_mode_info(info, &mode_info);
		if (mi == NULL)
			err = -EFAULT;
		else {
			if (copy_to_user((void __user *)arg, mi, sizeof(*mi))) {
				err = -EFAULT;
				break;
			}
			dev_dbg(&info->i2c_client->dev, "mode %d x %d %s:\n",
				mi->xres, mi->yres, mi->hdr ? "HDR" : "REG");
			dev_dbg(&info->i2c_client->dev,
				"line_len = %d\n", mi->line_len);
			dev_dbg(&info->i2c_client->dev,
				"frame_len = %d\n", mi->frame_len);
			dev_dbg(&info->i2c_client->dev,
				"xsize = %d\n", mi->xsize);
			dev_dbg(&info->i2c_client->dev,
				"ysize = %d\n", mi->ysize);
			dev_dbg(&info->i2c_client->dev,
				"vt_pix_clk_div = %d\n", mi->vt_pix_clk_div);
			dev_dbg(&info->i2c_client->dev,
				"vt_sys_clk_div = %d\n", mi->vt_sys_clk_div);
			dev_dbg(&info->i2c_client->dev,
				"pre_pll_clk_div = %d\n", mi->pre_pll_clk_div);
			dev_dbg(&info->i2c_client->dev,
				"pll_multi = %d\n", mi->pll_multi);
			dev_dbg(&info->i2c_client->dev,
				"op_pix_clk_div = %d\n", mi->op_pix_clk_div);
			dev_dbg(&info->i2c_client->dev,
				"op_sys_clk_div = %d\n", mi->op_sys_clk_div);
		}
		break;
	}
	default:
		dev_dbg(&info->i2c_client->dev, "INVALID IOCTL\n");
		err = -EINVAL;
	}

	if (err)
		dev_dbg(&info->i2c_client->dev,
			"%s - %x: ERR = %d\n", __func__, cmd, err);
	return err;
}

static int ar0833_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct ar0833_info *info;
	const char *mclk_name;
	dev_info(&client->dev, "ar0833: probing sensor.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = NULL;

	i2c_set_clientdata(client, info);

	mclk_name = info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	ar0833_power_get(info);
	ar0833_mode_info_init(info);

	memcpy(&info->miscdev_info,
		&ar0833_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		dev_err(&info->i2c_client->dev,
				"ar0833: Unable to register misc device!\n");
		return err;
	}
#ifdef CONFIG_DEBUG_FS
	ar0833_debug_init(info);
	info->enableDCBLC = 0;
#endif
	return 0;
}

static int ar0833_remove(struct i2c_client *client)
{
	struct ar0833_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&info->miscdev_info);

#ifdef CONFIG_DEBUG_FS
	if (info->debugfs_root)
		debugfs_remove_recursive(info->debugfs_root);
#endif
	return 0;
}

static const struct i2c_device_id ar0833_id[] = {
	{ "ar0833", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ar0833_id);

static struct i2c_driver ar0833_i2c_driver = {
	.driver = {
		.name = "ar0833",
		.owner = THIS_MODULE,
	},
	.probe = ar0833_probe,
	.remove = ar0833_remove,
	.id_table = ar0833_id,
};

static int __init ar0833_init(void)
{
	pr_info("ar0833 sensor driver loading\n");
	return i2c_add_driver(&ar0833_i2c_driver);
}

static void __exit ar0833_exit(void)
{
	i2c_del_driver(&ar0833_i2c_driver);
}

module_init(ar0833_init);
module_exit(ar0833_exit);
MODULE_LICENSE("GPL v2");
