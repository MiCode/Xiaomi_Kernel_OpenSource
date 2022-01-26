/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ziyi Li <Ziyi.Li@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "wl2868c.h"

#define wl2868c_MAX_CONFIG_NUM    16
#define wl2868c_IO_REG_LIMIT 20
#define wl2868c_IO_BUFFER_LIMIT 128
#define wl2868c_MISC_MAJOR 250

#define WL2868C_ADDR 0x28

#define WL2868C_REG_ID0 0x00
#define WL2868C_REG_ID1 0x01
#define WL2868C_REG_LDO1_VOUT 0x03
#define WL2868C_REG_LDO2_VOUT 0x04
#define WL2868C_REG_LDO3_VOUT 0x05
#define WL2868C_REG_LDO4_VOUT 0x06
#define WL2868C_REG_LDO5_VOUT 0x07
#define WL2868C_REG_LDO6_VOUT 0x08
#define WL2868C_REG_LDO7_VOUT 0x09
#define WL2868C_REG_LDOX_EN 0x0e

#define wl2868c_debug 1

#ifdef wl2868c_debug
#define WL2868C_LOGE(fmt, arg...) pr_debug(fmt, ##arg)
#else
#define WL2868C_LOGE(fmt, arg...) void(0)
#endif

//#define VIN1_REGULATOR_NAME "vs2"

/*
 *Hardware operate:
 *RST_N=VSYS=3.8V
 *VIN12=1.5V
 *VIN34=VIN5=VIN6=VIN7=3.6V
 */

#define LDO12_VOUT_MAX 1512
#define LDO34567_VOUT_MAX 3544

/*
 *LDO1-2 voltage output Range 0.496v ~ 1.512v
 *VOUT1/2 = 0.496v+LDO1/2_VOUT[6:0]*0.008V
 *LDO3-7 voltage output Range 1.504v ~ 3.544v
 *VOUT3/7 = 1.504V+LDO3/7_VOUT[7:0]*0.008V
 */



/*!
 * reg_value struct
 */

struct reg_value {
	u8 u8Add;
	u8 u8Val;
};

/*!
 * wl2868c_data_t struct
 */
struct wl2868c_data_t {
	struct i2c_client *i2c_client;
	struct regulator *vin1_regulator;
	u32 vin1_vol;
	struct regulator *vin2_regulator;
	u32 vin2_vol;
	int en_gpio;
//	int mp1601_gpio;
	u8 chip_id;
	u8 id_reg;
	u8 id_val;
	u8 init_num;
	struct reg_value inits[wl2868c_MAX_CONFIG_NUM];
	u32 offset;
	bool on;
};

enum wl2868c_ldo_num {
	WL2868C_LDO1 = 1,
	WL2868C_LDO2 = 2,
	WL2868C_LDO3 = 3,
	WL2868C_LDO4 = 4,
	WL2868C_LDO5 = 5,
	WL2868C_LDO6 = 6,
	WL2868C_LDO7 = 7,
};

const unsigned int LDO12_voltage_base = 496; // 496mv
const unsigned int LDO34567_voltage_base = 1504; // 1504mv

/*!
 * wl2868c_data
 */
static struct wl2868c_data_t wl2868c_data;

/*!
 * wl2868c write reg function
 *
 * @param reg u8
 * @param val u8
 * @return  Error code indicating success or failure
 */
static s32 wl2868c_write_reg(u8 reg, u8 val)
{
	u8 au8Buf[2] = {0};

	au8Buf[0] = reg;
	au8Buf[1] = val;
	if (i2c_master_send(wl2868c_data.i2c_client, au8Buf, 2) < 0) {
		pr_debug("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
			return -1;
	}
	pr_debug("%s:reg 0x%x val 0x%x ok\n", __func__, reg, val);
	return 0;
}

/*!
 * wl2868c read reg function
 *
 * @param reg u8
 * @param val u8 *
 * @return  Error code indicating success or failure
 */
static int wl2868c_read_reg(u8 reg, u8 *val)
{
	u8 au8RegBuf[1] = {0};
	u8 u8RdVal = 0;

	au8RegBuf[0] = reg;
	if (i2c_master_send(wl2868c_data.i2c_client, au8RegBuf, 1) != 1) {
		pr_debug("%s:write reg error:reg=%x\n", __func__, reg);
		return -1;
	}
	if (i2c_master_recv(wl2868c_data.i2c_client, &u8RdVal, 1) != 1) {
		pr_debug("%s:read reg error:reg=%x,val=%x\n",
				__func__, reg, u8RdVal);
		return -1;
	}
	*val = u8RdVal;
	return 0;
}

void enable_wl2868c_gpio(int i)
{
	if (wl2868c_data.en_gpio)
		gpio_set_value(wl2868c_data.en_gpio, i?1:0);

	WL2868C_LOGE("%s WL2868C_GPIO_RST = %d\n", i?"enable":"disable",
		gpio_get_value(wl2868c_data.en_gpio));
}
EXPORT_SYMBOL(enable_wl2868c_gpio);

int wl2868c_voltage_output(unsigned int ldo_num, unsigned int vol)
{
	u8 ldo_en = 0;
	unsigned int ldo_vout_value = 0;

	switch (ldo_num) {
	case WL2868C_LDO1:
		vol = vol > LDO12_VOUT_MAX ? LDO12_VOUT_MAX : vol;
		ldo_vout_value = (vol-LDO12_voltage_base)/8;
		wl2868c_write_reg(WL2868C_REG_LDO1_VOUT, ldo_vout_value);
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en |= 0x81;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO2:
		vol = vol > LDO12_VOUT_MAX ? LDO12_VOUT_MAX : vol;
		ldo_vout_value = (vol-LDO12_voltage_base)/8;
		wl2868c_write_reg(WL2868C_REG_LDO2_VOUT, 0x4C);
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en |= 0x82;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO3:
		vol = vol > LDO34567_VOUT_MAX ? LDO12_VOUT_MAX : vol;
		ldo_vout_value = (vol-LDO34567_voltage_base)/8;
		wl2868c_write_reg(WL2868C_REG_LDO3_VOUT, ldo_vout_value);
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en |= 0x84;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO4:
		vol = vol > LDO34567_VOUT_MAX ? LDO12_VOUT_MAX : vol;
		ldo_vout_value = (vol-LDO34567_voltage_base)/8;
		wl2868c_write_reg(WL2868C_REG_LDO4_VOUT, 0xac);
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en |= 0x88;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO5:
		vol = vol > LDO34567_VOUT_MAX ? LDO12_VOUT_MAX : vol;
		ldo_vout_value = (vol-LDO34567_voltage_base)/8;
		wl2868c_write_reg(WL2868C_REG_LDO5_VOUT, ldo_vout_value);
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en |= 0x90;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO6:
		vol = vol > LDO34567_VOUT_MAX ? LDO12_VOUT_MAX : vol;
		ldo_vout_value = (vol-LDO34567_voltage_base)/8;
		wl2868c_write_reg(WL2868C_REG_LDO6_VOUT, ldo_vout_value);
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en |= 0xa0;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO7:
		vol = vol > LDO34567_VOUT_MAX ? LDO12_VOUT_MAX : vol;
		ldo_vout_value = (vol-LDO34567_voltage_base)/8;
		wl2868c_write_reg(WL2868C_REG_LDO7_VOUT, ldo_vout_value);
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en |= 0xc0;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	default:
		return -1;
	}
	wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
	WL2868C_LOGE("wl2868c ldo_en = %d\n", ldo_en);
	return 0;
}
EXPORT_SYMBOL(wl2868c_voltage_output);

/*!
 * wl2868c VOUTPUT
 *
 * @param ldo_num ldo number *
.* @param vol ldo output voltage mv *
 * @return  Error code indicating success or failure
 */
int wl2868c_voltage_disable(unsigned int ldo_num)
{
	u8 ldo_en = 0;

	switch (ldo_num) {
	case WL2868C_LDO1:
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en &= 0xFE;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO2:
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en &= 0xFD;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO3:
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en &= 0xFC;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO4:
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en &= 0xFB;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO5:
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en &= 0xFA;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO6:
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en &= 0xF9;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	case WL2868C_LDO7:
		wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
		ldo_en &= 0xF8;
		wl2868c_write_reg(WL2868C_REG_LDOX_EN, ldo_en);
		break;
	default:
		return -1;
	}
	wl2868c_read_reg(WL2868C_REG_LDOX_EN, &(ldo_en));
	WL2868C_LOGE("wl2868c ldo_en = %d\n", ldo_en);
	if (ldo_en == 0x80) {
		WL2868C_LOGE("wl2868c no ldo working -> disable\n");
		enable_wl2868c_gpio(0);
	}
	return 0;
}
EXPORT_SYMBOL(wl2868c_voltage_disable);


void wl2868c_ldo7_voltage_set(int status)
{
	if (status == 1) {
		gpio_get_value(wl2868c_data.en_gpio);
		if (wl2868c_data.en_gpio)
			enable_wl2868c_gpio(1);
		wl2868c_voltage_output(WL2868C_LDO7, 2800);
	} else if (status == 0) {
		wl2868c_voltage_disable(WL2868C_LDO7);
	}
}

void AF_poweron_20181(int status)
{
	wl2868c_ldo7_voltage_set(status);
}
EXPORT_SYMBOL(AF_poweron_20181);


/*!
 * wl2868c power on function
 *
 * @param dev struct device *
 * @return  Error code indicating success or failure
 */
static int wl2868c_power_on(struct device *dev)
{
	int ret = 0;

	wl2868c_data.vin1_regulator = devm_regulator_get(dev, "vin1");
	if (!IS_ERR(wl2868c_data.vin1_regulator)) {
		ret = of_property_read_u32(dev->of_node, "vin1_vol",
			(u32 *) &(wl2868c_data.vin1_vol));
		if (ret) {
			pr_debug("vin1_vol missing or invalid\n");
			return ret;
		}
		regulator_set_voltage(wl2868c_data.vin1_regulator,
			wl2868c_data.vin1_vol,
			wl2868c_data.vin1_vol);
		ret = regulator_enable(wl2868c_data.vin1_regulator);
		if (ret) {
			pr_debug("%s:vin1 set voltage error %d\n", __func__, ret);
			return ret;
		}
		pr_debug("%s:vin1 set voltage %d ok\n", __func__,
			wl2868c_data.vin1_vol);
	} else {
		pr_debug("%s: cannot get vin1 voltage error\n", __func__);
		wl2868c_data.vin1_regulator = NULL;
	}

	wl2868c_data.en_gpio = of_get_named_gpio(dev->of_node, "en-gpios", 0);
	if (!gpio_is_valid(wl2868c_data.en_gpio)) {
		pr_debug("no en pin available");
		return -EINVAL;
	}
	ret = devm_gpio_request_one(dev, wl2868c_data.en_gpio,
		GPIOF_OUT_INIT_HIGH, "wl2868c_en");
	if (ret < 0) {
		pr_debug("wl2868c_en request failed %d\n", ret);
		return ret;
	}
	pr_debug("%s: en request ok\n", __func__);

	gpio_direction_output(wl2868c_data.en_gpio, 1);

	return 0;
}


/*!
 * wl2868c match id function
 *
 * @param dev struct device *
 * @return  Error code indicating success or failure
 */
static int wl2868c_match_id(struct device *dev)
{
	int ret = 0;

	ret = of_property_read_u32(dev->of_node, "id_reg",
		(u32 *) &(wl2868c_data.id_reg));
	if (ret) {
		pr_debug("id_reg missing or invalid\n");
		return ret;
	}
	ret = of_property_read_u32(dev->of_node, "id_val",
				(u32 *) &(wl2868c_data.id_val));
	if (ret) {
		pr_debug("id_val missing or invalid\n");
		return ret;
	}
	ret = wl2868c_read_reg(wl2868c_data.id_reg, &(wl2868c_data.chip_id));
	if (ret < 0 || wl2868c_data.chip_id != wl2868c_data.id_val) {
		pr_debug("wl2868c: is not found %d %x\n", ret, wl2868c_data.chip_id);
		return -ENODEV;
	}
	pr_debug("wl2868c: is found 0x%x\n", wl2868c_data.chip_id);
	return 0;
}

/*!
 * wl2868c init dev function
 *
 * @param dev struct device *
 * @return  Error code indicating success or failure
 */
static int wl2868c_init_dev(struct device *dev)
{
	int ret = 0;
	u32 inits[32];

	ret = of_property_read_u32(dev->of_node, "init_num",
		(u32 *) &(wl2868c_data.init_num));

	ret = of_property_read_u32_array(dev->of_node, "inits",
	    inits, wl2868c_data.init_num * 2);

	//wl2868c_voltage_output(WL2868C_LDO1,1200); // LDO1 output 1200mv
	//wl2868c_voltage_output(WL2868C_LDO2,1100); // LDO2 output 1100mv
	//wl2868c_voltage_output(WL2868C_LDO3,2800); // LDO3 output 2800mv
	//wl2868c_voltage_output(WL2868C_LDO4,2900); // LDO4 output 2900mv
	//wl2868c_voltage_output(WL2868C_LDO5,0); // LDO5 output 0mv
	//wl2868c_voltage_output(WL2868C_LDO6,2800); // LDO6 output 2800mv
	//wl2868c_voltage_output(WL2868C_LDO7,2800); // LDO7 output 3800mv
	//wl2868c_voltage_disable(WL2868C_LDO1);
	//wl2868c_voltage_disable(WL2868C_LDO2);
	//wl2868c_voltage_disable(WL2868C_LDO3);
	//wl2868c_voltage_disable(WL2868C_LDO4);
	//wl2868c_voltage_disable(WL2868C_LDO5);
	//wl2868c_voltage_disable(WL2868C_LDO6);
	//wl2868c_voltage_disable(WL2868C_LDO7);
	return 0;
}

/*!
 * wl2868c GetHexCh function
 *
 * @param value u8
 * @param shift int
 * @return char value
 */
static char GetHexCh(
	u8 value,
	int shift)
{
	u8 data = (value >> shift) & 0x0F;
	char ch = 0;

	if (data >= 10)
		ch = data - 10  + 'A';
	else if (data >= 0)
		ch = data + '0';
	return ch;
}

/*!
 * wl2868c read function
 *
 * @param file struct file *
 * @param buf char __user *
 * @param count size_t
 * @param offset loff_t *
 * @return  read count
 */
static ssize_t wl2868c_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *offset)
{
	char *buffer = NULL;
	int ret = 0, num = 0, i = 0;
	u8 u8add = wl2868c_data.offset, u8val = 0;

	buffer = kmalloc(wl2868c_IO_BUFFER_LIMIT, GFP_KERNEL);
	if (buffer == NULL)
		return -ENOMEM;

	if (count > wl2868c_IO_REG_LIMIT) {
		pr_debug("wl2868c: read count %d > %d\n", count, wl2868c_IO_REG_LIMIT);
		return -ERANGE;
	}

	pr_debug("wl2868c: read %d registers from %02X to %02X.\n",
		count, u8add, (u8add + count - 1));
	for (i = 0; i < count; i++, u8add++) {
		ret = wl2868c_read_reg(u8add, &u8val);
		if (ret < 0) {
			pr_debug("wl2868c: read %X failed %d\n", u8add, ret);
			kfree(buffer);
			return ret;
		}
		buffer[num++] = GetHexCh(u8add, 4);
		buffer[num++] = GetHexCh(u8add, 0);
		buffer[num++] = ' ';
		buffer[num++] = GetHexCh(u8val, 4);
		buffer[num++] = GetHexCh(u8val, 0);
		buffer[num++] = ' ';
		pr_debug("wl2868c: read REG[%02X %02X]\n", u8add, u8val);
	}

	copy_to_user(buf, buffer, num);
	kfree(buffer);
	return count;
}

/*!
 * wl2868c GetHex function
 *
 * @param ch char
 * @return hex value
 */
static u8 GetHex(
	char ch)
{
	u8 value = 0;

	if (ch >= 'a')
		value = ch - 'a' + 10;
	else if (ch >= 'A')
		value = ch - 'A' + 10;
	else if (ch >= '0')
		value = ch - '0';

	return value;
}

/*!
 * wl2868c write function
 *
 * @param file struct file *
 * @param buf char __user *
 * @param count size_t
 * @param offset loff_t *
 * @return  write count
 */
static ssize_t wl2868c_write(
	struct file *file,
	const char __user *buf,
	size_t count,
	loff_t *offset)
{
	int ret = 0, i = 0;
	char *buffer = NULL;

	if (count > wl2868c_IO_BUFFER_LIMIT) {
		pr_debug("wl2868c: write size %d > %d\n", count, wl2868c_IO_BUFFER_LIMIT);
		return -ERANGE;
	}
	buffer = memdup_user(buf, count);
	if (IS_ERR(buffer)) {
		pr_debug("wl2868c: can't get user data\n");
		return PTR_ERR(buffer);
	}
	pr_debug("wl2868c: write %d bytes.\n", count);
	for (i = 0; i < count; i += 6) {
		u8 u8add = (GetHex(buffer[i + 0]) << 4) | GetHex(buffer[i + 1]);
		u8 u8val = (GetHex(buffer[i + 3]) << 4) | GetHex(buffer[i + 4]);

		ret = wl2868c_write_reg(u8add, u8val);
		if (ret < 0) {
			pr_debug("wl2868c: write failed %d\n", ret);
			kfree(buffer);
			return -ENODEV;
		}
		pr_debug("wl2868c: write REG[%02X %02X]\n", u8add, u8val);
	}
	kfree(buffer);
	return count;
}

/*!
 * wl2868c seek function
 *
 * @param file struct file *
 * @param offset loff_t
 * @param whence int
 * @return file pos
 */
loff_t wl2868c_llseek(
	struct file *file,
	loff_t offset,
	int whence)
{
	switch (whence) {
	case SEEK_CUR:
		wl2868c_data.offset += offset;
		break;
	default:
		wl2868c_data.offset = 0;
		break;
	}
	pr_debug("wl2868c: update read pos to %02X\n", wl2868c_data.offset);
	return file->f_pos;
}

/*!
 * wl2868c open function
 *
 * @param inode struct inode *
 * @param file struct file *
 * @return Error code indicating success or failure
 */
static int wl2868c_open(
	struct inode *inode,
	struct file *file)
{
	if (!wl2868c_data.on) {
		pr_debug("wl2868c: open failed.\n");
		return -ENODEV;
	}
	wl2868c_data.offset = 0;
	return 0;
}

/*!
 * file_operations struct
 */
static const struct file_operations wl2868c_fops = {
	.owner   = THIS_MODULE,
	.open    = wl2868c_open,
	.llseek  = wl2868c_llseek,
	.read    = wl2868c_read,
	.write   = wl2868c_write,
};

/*!
 * miscdevice struct
 */
static struct miscdevice wl2868c_miscdev = {
	.minor    = wl2868c_MISC_MAJOR,
	.name    = "wl2868c",
	.fops    = &wl2868c_fops,
};

/*!
 * wl2868c I2C probe function
 *
 * @param client struct i2c_client *
 * @param id struct i2c_device_id *
 * @return  Error code indicating success or failure
 */
static int wl2868c_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	memset(&wl2868c_data, 0, sizeof(struct wl2868c_data_t));
	wl2868c_data.i2c_client = client;
	ret = wl2868c_power_on(&client->dev);
	if (ret) {
		pr_debug("wl2868c_power_on failed %d\n", ret);
		return ret;
	}
	ret = wl2868c_match_id(&client->dev);
	if (ret) {
		pr_debug("wl2868c_match_id failed %d\n", ret);
		return ret;
	}
	ret = wl2868c_init_dev(&client->dev);
	if (ret) {
		pr_debug("wl2868c_init_dev failed %d\n", ret);
		return -ENODEV;
	}
	ret = misc_register(&wl2868c_miscdev);
	if (ret < 0) {
		pr_debug("failed to register wl2868c device\n");
		return ret;
	}
	wl2868c_data.on = true;
	pr_debug("%s successded!\n", __func__);
	return 0;
}

/*!
 * wl2868c I2C remove function
 *
 * @param client struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int wl2868c_remove(
	struct i2c_client *client)
{
	misc_deregister(&wl2868c_miscdev);
	wl2868c_data.on = false;
	pr_debug("deregister wl2868c device ok\n");
	return 0;
}

/*!
 * i2c_device_id struct
 */
static const struct i2c_device_id wl2868c_id[] = {
	{"wl2868c-i2c", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, wl2868c_id);

/*!
 * i2c_driver struct
 */
static struct i2c_driver wl2868c_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "wl2868c",
	},
	.probe  = wl2868c_probe,
	.remove = wl2868c_remove,
	.id_table = wl2868c_id,
};

/*!
 * wl2868c init function
 *
 * @return  Error code indicating success or failure
 */
static __init int wl2868c_init(void)
{
	u8 ret = 0;

	ret = i2c_add_driver(&wl2868c_i2c_driver);
	if (ret != 0) {
		pr_debug("%s: add driver failed, error=%d\n",
			__func__, ret);
		return ret;
	}
	pr_debug("%s: add driver success\n", __func__);
	return ret;
}

/*!
 * wl2868c cleanup function
 */
static void __exit wl2868c_clean(void)
{
	i2c_del_driver(&wl2868c_i2c_driver);
}

module_init(wl2868c_init);
module_exit(wl2868c_clean);

MODULE_DESCRIPTION("wl2868c Power IC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
