// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<HWMSEN> "fmt

#include <generated/autoconf.h>
#include <linux/atomic.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/hwmon-sysfs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <hwmsen_helper.h>
#include <hwmsensor.h>

#define hex2int(c) ((c >= '0') && (c <= '9') ? (c - '0') : ((c & 0xf) + 9))

#define C_MAX_REG_LEN (4)

int hwmsen_set_bits(struct i2c_client *client, u8 addr, u8 bits)
{
	int err;
	u8 cur, nxt;

	err = hwmsen_read_byte(client, addr, &cur);
	if (err) {
		pr_debug("read err: 0x%02X\n", addr);
		return -EFAULT;
	}

	nxt = (cur | bits);

	if (nxt ^ cur) {
		err = hwmsen_write_byte(client, addr, nxt);
		if (err) {
			pr_debug("write err: 0x%02X\n", addr);
			return -EFAULT;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(hwmsen_set_bits);

int hwmsen_clr_bits(struct i2c_client *client, u8 addr, u8 bits)
{
	int err;
	u8 cur, nxt;

	err = hwmsen_read_byte(client, addr, &cur);
	if (err) {
		pr_debug("read err: 0x%02X\n", addr);
		return -EFAULT;
	}

	nxt = cur & (~bits);

	if (nxt ^ cur) {
		err = hwmsen_write_byte(client, addr, nxt);
		if (err) {
			pr_debug("write err: 0x%02X\n", addr);
			return -EFAULT;
		}
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_clr_bits);
/*----------------------------------------------------------------------------*/
int hwmsen_read_byte(struct i2c_client *client, u8 addr, u8 *data)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = {
		{
			.flags = 0,
			.len = 1,
			.buf = &beg
		},
		{
			.flags = I2C_M_RD,
			.len = 1,
			.buf = data,
		}
	};

	if (!client)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[1].addr = client->addr;

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		pr_debug("i2c_transfer error: (%d %p) %d\n", addr, data, err);
		err = -EIO;
	}

	err = 0;

	return err;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_read_byte);
/*----------------------------------------------------------------------------*/
int hwmsen_write_byte(struct i2c_client *client, u8 addr, u8 data)
{
	u8 buf[] = {addr, data};
	int ret = 0;

	ret = i2c_master_send(client, (const char *)buf, sizeof(buf));
	if (ret < 0) {
		pr_debug("send command error!!\n");
		return -EFAULT;
	}
#if defined(HWMSEN_DEBUG)
	pr_debug("%s(0x%02X)= %02X\n", __func__, addr, data);
#endif

	return 0;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_write_byte);
/*----------------------------------------------------------------------------*/
int hwmsen_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = {{0}, {0} };

	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		pr_debug(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	if (len == 1)
		return hwmsen_read_byte(client, addr, data);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		pr_debug("i2c_transfer error: (%d %p %d) %d\n", addr, data, len,
			err);
		err = -EIO;
	}
#if defined(HWMSEN_DEBUG)
	static char buf[128];
	int idx, buflen = 0;

	for (idx = 0; idx < len; idx++)
		buflen += snprintf(buf + buflen, sizeof(buf) - buflen, "%02X ",
				   data[idx]);
	pr_debug("%s(0x%02X,%2d) = %s\n", __func__, addr, len, buf);
#endif
	err = 0; /*no error*/

	return err;
}
EXPORT_SYMBOL_GPL(hwmsen_read_block);
/*----------------------------------------------------------------------------*/
int hwmsen_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	int err, idx, num;
	char buf[C_I2C_FIFO_SIZE];

	if (!client)
		return -EINVAL;
	else if (len >= C_I2C_FIFO_SIZE) {
		pr_debug(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		pr_debug("send command error!!\n");
		return -EFAULT;
	}
#if defined(HWMSEN_DEBUG)
	static char buf[128];
	int idx, buflen = 0;

	for (idx = 0; idx < len; idx++)
		buflen += snprintf(buf + buflen, sizeof(buf) - buflen, "%02X ",
				   data[idx]);
	pr_debug("%s(0x%02X,%2d)= %s\n", __func__, addr, len, buf);
#endif
	err = 0; /*no error*/

	return err;
}
EXPORT_SYMBOL_GPL(hwmsen_write_block);
/*----------------------------------------------------------------------------*/
static int hwmsen_chrs_to_integer(u8 dat[C_MAX_REG_LEN], int datlen)
{
	int idx;
	u32 val = 0;

	for (idx = 0; idx < datlen; idx++)
		val += (dat[idx] << (8 * idx));
	return val;
}
/*----------------------------------------------------------------------------*/
static int hwmsen_byte_to_integer(u8 dat, int datlen)
{
	int idx;
	u32 val = 0;

	for (idx = 0; idx < datlen; idx++)
		val += (dat << (8 * idx));
	return val;
}
/*----------------------------------------------------------------------------*/
void hwmsen_single_rw(struct i2c_client *client, struct hwmsen_reg *regs,
		      int reg_num)
{
	int idx, pos, num, err = 0, cmp1, cmp2;
	u8 pattern[] = {0x55, 0xAA};
	u8 old[C_MAX_REG_LEN], val[C_MAX_REG_LEN], mask;
	struct hwmsen_reg *ptr;

	for (idx = 0; idx < reg_num; idx++) {
		if (regs[idx].mode != REG_RW)
			continue;
		if (regs[idx].len > C_MAX_REG_LEN) {
			pr_debug("exceed maximum length\n");
			continue;
		}
		ptr = &regs[idx];

		err = hwmsen_read_block(client, ptr->addr, old, ptr->len);
		if (err) {
			pr_debug("read %s fails\n", ptr->name);
			continue;
		}
		for (pos = 0; pos < ARRAY_SIZE(pattern); pos++) {
			mask = ptr->mask;
			for (num = 0; num < ptr->len; num++)
				val[num] = pattern[pos] & mask;
			err = hwmsen_write_block(client, ptr->addr, val,
						 ptr->len);
			if (err) {
				pr_debug("test: write %s fails, pat[0x%02X]\n",
					ptr->name, pattern[pos]);
				continue;
			}
			err = hwmsen_read_block(client, ptr->addr, val,
						ptr->len);
			if (err) {
				pr_debug("test: read %s fails\n", ptr->name);
				continue;
			}

			cmp1 = hwmsen_chrs_to_integer(val, ptr->len);
			cmp2 = hwmsen_byte_to_integer(pattern[pos], ptr->len);
			if ((cmp1 ^ cmp2) & ptr->mask)
				pr_debug("test: reg %s[%d] 0x%08X <-> 0x%08X\n",
					ptr->name, num, cmp1, cmp2);
		}

		err = hwmsen_write_block(client, ptr->addr, old, ptr->len);
		if (err) {
			pr_debug("restore: write %s\n", ptr->name);
			continue;
		}

		err = hwmsen_read_block(client, ptr->addr, val, ptr->len);
		if (err) {
			pr_debug("restore: read %s\n", ptr->name);
			continue;
		}

		cmp1 = hwmsen_chrs_to_integer(val, ptr->len);
		cmp2 = hwmsen_chrs_to_integer(old, ptr->len);
		if ((cmp1 ^ cmp2) & ptr->mask) {
			pr_debug("restore %s fails\n", ptr->name);
			err = -EFAULT;
		}
	}
	if (!err)
		pr_debug("%s pass!!\n", __func__);
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_single_rw);
/*----------------------------------------------------------------------------*/
void hwmsen_multi_rw(struct i2c_client *client, find_reg_t findreg,
		     struct hwmsen_reg_test_multi *items, int inum)
{
	u8 pattern[] = {0x55, 0xAA};
	u8 buf[C_I2C_FIFO_SIZE], dat[C_I2C_FIFO_SIZE], old[C_I2C_FIFO_SIZE];
	int pos, idx, p, err = 0;

	for (idx = 0; idx < inum; idx++) {
		u8 addr = items[idx].addr;
		u8 len = items[idx].len;
		u8 mode = items[idx].mode;

		for (pos = 0; pos < ARRAY_SIZE(pattern); pos++) {
			err = hwmsen_read_block(client, addr, old, len);
			if (err) {
				pr_debug("(%d) save block fail!!\n", idx);
				continue;
			}
			if (!(mode & REG_WO))
				continue;

			memset(buf, pattern[pos], sizeof(buf));
			for (p = 0; p < len; p++)
				buf[p] = buf[p] & findreg(addr + p)->mask;
			err = hwmsen_write_block(client, addr, buf, len);
			if (err) {
				pr_debug("(%d) block write fail!!\n", idx);
				continue;
			}
			memset(dat, 0x00, sizeof(dat));
			err = hwmsen_read_block(client, addr, dat, len);
			if (err) {
				pr_debug("(%d) block read fail!!\n", idx);
				continue;
			}
			err = hwmsen_write_block(client, addr, old, len);
			if (err) {
				pr_debug("(%d) restore block fail!!\n", idx);
				continue;
			}

			if (memcmp(buf, dat, len)) {
				pr_debug("(%d) data compare fail!!\n", idx);
				pr_debug("buf:");
				for (p = 0; p < len; p++)
					pr_debug("%02X", buf[p]);
				pr_debug("\n");
				pr_debug("dat:");
				for (p = 0; p < len; p++)
					pr_debug("%02X", dat[p]);
				pr_debug("\n");
				err = 1;
			}
		}
	}
	if (!err)
		pr_debug("%s success : %d cases\n", __func__, inum);
	else
		pr_debug("%s: %d cases\n", __func__, inum);
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_multi_rw);
/*----------------------------------------------------------------------------*/
ssize_t hwmsen_show_dump(struct i2c_client *client, u8 startAddr, u8 *regtbl,
			 u32 regnum, find_reg_t findreg, char *buf, u32 buflen)
{
	int err = 0;
	u8 addr = startAddr;
	u8 max_len = 8, read_len;
	const char *name = NULL;

	if (!client)
		return -EINVAL;

	memset(regtbl, 0x00, regnum);

	do {
		read_len = ((regnum - addr) > max_len) ? (max_len)
						       : (regnum - addr);
		if (!read_len)
			break;

		err = hwmsen_read_block(client, addr, &regtbl[addr - startAddr],
					read_len);
		if (!err)
			addr += read_len;
		/* SEN_VER("read block data: %d %d\n", addr, read_len); */
	} while (!err);

	if (!err) {
		int idx;
		ssize_t len = 0;

		for (idx = startAddr; idx < regnum; idx++) {
			name = findreg(idx)->name;
			if (name != NULL)
				len += snprintf(buf + len, buflen - len,
						"%-16s = 0x%02X\n", name,
						regtbl[idx - startAddr]);
		}
		return len;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_show_dump);
/*----------------------------------------------------------------------------*/
ssize_t hwmsen_read_all_regs(struct i2c_client *client, struct hwmsen_reg *regs,
			     u32 num, char *buf, u32 buflen)
{
	int err = 0, pos, idx, val;
	struct hwmsen_reg *ptr;
	u8 dat[4];
	ssize_t len = 0;

	if (!client)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		ptr = &regs[idx];
		memset(dat, 0x00, sizeof(dat));
		if (ptr->len > sizeof(dat)) {
			pr_debug("exceeds capacity, %d\n", ptr->len);
			break;
		}
		err = hwmsen_read_block(client, ptr->addr, dat, ptr->len);
		if (err) {
			pr_debug("read reg %s (0x%2X) fail!!\n", ptr->name,
				ptr->addr);
			break;
		}

		val = 0;
		for (pos = 0; pos < ptr->len; pos++)
			val += (dat[pos] << (8 * pos));
		len += snprintf(buf + len, buflen - len, "%-16s = %8X\n",
				ptr->name, val);
	}
	return (err) ? (0) : len;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_read_all_regs);
/*----------------------------------------------------------------------------*/
ssize_t hwmsen_show_reg(struct i2c_client *client, u8 addr, char *buf,
			u32 buflen)
{
	u8 data = 0;
	int err;

	err = hwmsen_read_byte(client, addr, &data);
	if (err) {
		pr_debug("reading address 0x%02X fail!!", addr);
		return 0;
	} else
		return snprintf(buf, buflen, "0x%02X\n", data);
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_show_reg);
/*----------------------------------------------------------------------------*/
ssize_t hwmsen_store_reg(struct i2c_client *client, u8 addr, const char *buf,
			 size_t count)
{
	int err, val;
	u8 data;

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		pr_debug("format not match: (0xAB) <-> '%s'\n", buf);
		return count;
	}
	data = (u8)val;

	err = hwmsen_write_byte(client, addr, data);
	if (err)
		pr_debug("write address 0x%02X fail!!\n", addr);
	return count;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_store_reg);
/*----------------------------------------------------------------------------*/
ssize_t hwmsen_show_byte(struct device *dev, struct device_attribute *attr,
			 char *buf, u32 buflen)
{
	int err, index = to_sensor_dev_attr(attr)->index;
	struct i2c_client *client = to_i2c_client(dev);
	u8 addr = (u8)index;
	u8 dat;

	err = hwmsen_read_byte(client, addr, &dat);
	if (err) {
		pr_debug("reading address 0x%02X fail!!", addr);
		return 0;
	}
	return snprintf(buf, buflen, "0x%02X\n", dat);
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_show_byte);
/*----------------------------------------------------------------------------*/
ssize_t hwmsen_store_byte(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int err, val;
	int index = to_sensor_dev_attr(attr)->index;
	struct i2c_client *client = to_i2c_client(dev);
	u8 addr = (u8)index;
	u8 dat;

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		pr_debug("format not match: (0xAB) <-> '%s'\n", buf);
		return count;
	}
	dat = (u8)val;

	err = hwmsen_write_byte(client, addr, dat);
	if (err)
		pr_debug("write address 0x%02X fail!!\n", addr);
	return count;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_store_byte);
/*----------------------------------------------------------------------------*/
ssize_t hwmsen_show_word(struct device *dev, struct device_attribute *attr,
			 char *buf, u32 buflen)
{
	int err, index = to_sensor_dev_attr(attr)->index;
	struct i2c_client *client = to_i2c_client(dev);
	u8 addr = (u8)index;
	u8 dat[2];

	err = hwmsen_read_block(client, addr, dat, sizeof(dat));
	if (err) {
		pr_debug("reading address 0x%02X fail!!", addr);
		return 0;
	}
	return snprintf(buf, buflen, "0x%04X\n", (dat[0] | (dat[1] << 8)));
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_show_word);
/*----------------------------------------------------------------------------*/
ssize_t hwmsen_store_word(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int err, val;
	int index = to_sensor_dev_attr(attr)->index;
	struct i2c_client *client = to_i2c_client(dev);
	u8 addr = (u8)index;
	u8 dat[2];

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		pr_debug("format not match: (0xAB) <-> '%s'\n", buf);
		return count;
	}
	dat[0] = (u8)((val & 0x00FF));
	dat[1] = (u8)((val & 0xFF00) >> 8);

	err = hwmsen_write_block(client, addr, dat, sizeof(dat));
	if (err)
		pr_debug("write address 0x%02X fail!!\n", addr);
	return count;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_store_word);
/*----------------------------------------------------------------------------*/
struct hwmsen_convert map[] = {
	{ { 1, 1, 1}, {0, 1, 2} },
	{ {-1, 1, 1}, {1, 0, 2} },
	{ {-1, -1, 1}, {0, 1, 2} },
	{ { 1, -1, 1}, {1, 0, 2} },

	{ {-1, 1, -1}, {0, 1, 2} },
	{ { 1, 1, -1}, {1, 0, 2} },
	{ { 1, -1, -1}, {0, 1, 2} },
	{ {-1, -1, -1}, {1, 0, 2} },

};
/*----------------------------------------------------------------------------*/
int hwmsen_get_convert(int direction, struct hwmsen_convert *cvt)
{
	if (!cvt)
		return -EINVAL;
	else if (direction >= ARRAY_SIZE(map))
		return -EINVAL;

	*cvt = map[direction];
	return 0;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(hwmsen_get_convert);
/*----------------------------------------------------------------------------*/
