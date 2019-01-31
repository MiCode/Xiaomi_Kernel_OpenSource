/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <mt-plat/upmu_common.h>
#include "mt6311-i2c.h"

/*
 * Global Variable
 */
/* mt6311 i2c device for mt6311 dirver */
static struct i2c_client *new_client;
static const struct i2c_device_id mt6311_i2c_id[] = {
	{"mt6311", 0},
	{}
};
static const struct of_device_id mt6311_of_ids[] = {
	{.compatible = "mediatek,vproc_buck"},
	{},
};
static DEFINE_MUTEX(mt6311_i2c_access);
static int g_mt6311_driver_ready;
static int g_mt6311_hw_exist;
static unsigned char is_mt6311_checked;

/*
 * Exported API
 */
int is_mt6311_sw_ready(void)
{
	return g_mt6311_driver_ready;
}

int is_mt6311_exist(void)
{

	if (is_mt6311_checked == 0) {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6356)
	/* get the MT6311 status which is recorded in
	 * MT6356 register at Preloader
	 */
		pmic_read_interface(MT6356_TOP_MDB_CONF0,
			&g_mt6311_hw_exist, 0x1, 15);
#endif
		is_mt6311_checked = 1;
	}
	return g_mt6311_hw_exist;
}

/*
 * Internal Function
 */
static int mt6311_read_device(
		unsigned char addr, unsigned int len, unsigned char *data)
{
	return i2c_smbus_read_i2c_block_data(new_client, addr, len, data);
}

static int mt6311_write_device(
		unsigned char addr, unsigned int len, unsigned char *data)
{
	return i2c_smbus_write_i2c_block_data(new_client, addr, len, data);
}

/*
 * MT6311 read/write API
 */
int mt6311_read_byte(unsigned char addr, unsigned char *data)
{
	int ret = 0;

	mutex_lock(&mt6311_i2c_access);
	ret = mt6311_read_device(addr, 1, data);
	mutex_unlock(&mt6311_i2c_access);
	if (ret < 0) {
		pr_notice(MT6311TAG "read Reg[0x%x] fail, ret = %d\n"
			  , addr, ret);
		return ret;
	}

	return 0;
}

int mt6311_write_byte(unsigned char addr, unsigned char data)
{
	int ret = 0;

	mutex_lock(&mt6311_i2c_access);
	ret =  mt6311_write_device(addr, 1, &data);
	mutex_unlock(&mt6311_i2c_access);
	if (ret < 0) {
		pr_notice(MT6311TAG "write Reg[0x%x]=0x%x fail, ret = %d\n"
			  , addr, data, ret);
		return ret;
	}

	return 0;
}

int mt6311_assign_bit(unsigned char reg, unsigned char mask, unsigned char data)
{
	unsigned char tmp = 0;
	unsigned char regval = 0;
	int ret = 0;

	mutex_lock(&mt6311_i2c_access);
	ret = mt6311_read_device(reg, 1, &regval);
	if (ret < 0) {
		pr_notice(MT6311TAG "[%s] read fail(ret=%d), Reg[0x%x] data=0x%x, mask=0x%x\n"
			  , __func__, ret, reg, data, mask);
		goto OUT_ASSIGN;
	}
	tmp = ((regval & 0xff) & ~mask);
	tmp |= (data & mask);
	ret = mt6311_write_device(reg, 1, &tmp);
	if (ret < 0) {
		pr_notice(MT6311TAG "[%s] assign bit fail(ret=%d), Reg[0x%x] data=0x%x, mask=0x%x\n"
			  , __func__, ret, reg, data, mask);
		goto OUT_ASSIGN;
	}
OUT_ASSIGN:
	mutex_unlock(&mt6311_i2c_access);
	return ret;
}

int mt6311_read_interface(unsigned char reg, unsigned char *data,
	unsigned char mask, unsigned char shift)
{
	unsigned char tmp = 0;
	unsigned char regval = 0;
	int ret = 0;

	mutex_lock(&mt6311_i2c_access);
	ret = mt6311_read_device(reg, 1, &regval);
	tmp = regval;
	if (ret < 0) {
		pr_notice(MT6311TAG "[%s] read fail(ret=%d), Reg[0x%x], mask=0x%x, shift=%d\n"
			  , __func__, ret, reg, mask, shift);
		goto OUT_ASSIGN;
	}

	tmp &= (mask << shift);
	*data = (tmp >> shift);

OUT_ASSIGN:
	mutex_unlock(&mt6311_i2c_access);
	return ret;
}


int mt6311_config_interface(unsigned char reg, unsigned char data,
	unsigned char mask, unsigned char shift)
{
	unsigned char tmp = 0;
	unsigned char regval = 0;
	int ret = 0;

	mutex_lock(&mt6311_i2c_access);
	ret = mt6311_read_device(reg, 1, &regval);
	tmp = regval;
	if (ret < 0) {
		pr_notice(MT6311TAG "[%s] read fail(ret=%d), Reg[0x%x] data=0x%x, mask=0x%x, shift=%d\n"
			  , __func__, ret, reg, data, mask, shift);
		goto OUT_ASSIGN;
	}

	tmp &= ~(mask << shift);
	tmp |= (data << shift);

	ret = mt6311_write_device(reg, 1, &tmp);
	if (ret < 0) {
		pr_notice("[%s] write fail(ret=%d), Reg[0x%x] data=0x%x, mask=0x%x, shift=%d\n"
			  , __func__, ret, reg, data, mask, shift);
		goto OUT_ASSIGN;
	}
OUT_ASSIGN:
	mutex_unlock(&mt6311_i2c_access);
	return ret;
}

static int mt6311_check_id(void)
{
	int ret = 0;
	unsigned char data = 0;

	ret = mt6311_read_device(MT6311_CID, 1, &data);
	if (ret < 0) {
		/* MT6311 IO fail */
		pr_notice(MT6311TAG "MT6311 check id fail\n");
		return -EIO;
	}

	if (data != MT6311_CID_CODE) {
		/*MT6311 CID(0x%02x) not match */
		pr_notice(MT6311TAG "MT6311 id(0x%x) error\n", data);
		return -EINVAL;
	}
	return 0;
}

static int mt6311_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;

	MT6311LOG("MT6311 probe\n"); /*mt6311_probe*/

	new_client = client;
	ret = mt6311_check_id();
	if (ret < 0)
		return ret;
	g_mt6311_hw_exist = 1;
#if 0
/* initial setting */

#endif

#ifdef IPIMB_MT6311
	pr_notice(MT6311TAG "regulator not support for SSPM\n");
	return 0;
#else
	ret = mt6311_regulator_init(&client->dev);
	if (ret < 0) {
		/*mt6311_probe regulator init fail*/
		MT6311LOG("MT6311 regulator init fail\n");
		return -EINVAL;
	}
	g_mt6311_driver_ready = 1;

	MT6311LOG("MT6311 probe done\n"); /*mt6311_probe --OK!!--*/
	return 0;
#endif
}

static int mt6311_i2c_remove(struct i2c_client *client)
{
#ifdef IPIMB_MT6311
	pr_notice(MT6311TAG "regulator not support for SSPM\n");
#else
	mt6311_regulator_deinit();
#endif
	return 0;
}

static struct i2c_driver mt6311_driver = {
	.driver = {
		.name = "mt6311",
		.of_match_table = of_match_ptr(mt6311_of_ids),
	},
	.probe = mt6311_i2c_probe,
	.remove = mt6311_i2c_remove,
	.id_table = mt6311_i2c_id,
};

/*
 * mt6311_access
 */
static unsigned char g_reg_value_mt6311;

static ssize_t show_mt6311_access(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice(MT6311TAG "[%s] 0x%x\n", __func__, g_reg_value_mt6311);
	return sprintf(buf, "0x%x\n", g_reg_value_mt6311);
}

static ssize_t store_mt6311_access(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
#ifdef IPIMB_MT6311
	pr_notice(MT6311TAG "mt6311_access not support for SSPM\n");
	return size;
#else
	int ret;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_notice(MT6311TAG "[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_notice(MT6311TAG "[%s] size is %d, buf is %s\n"
			  , __func__, (int)size, buf);
		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		if (addr)
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);
		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
			pr_notice(MT6311TAG "[%s] write mt6311 reg 0x%x with value 0x%x!\n"
				  , __func__, reg_address, reg_value);
			ret = mt6311_config_interface(reg_address
					, reg_value, 0xFF, 0x0);
		} else {
			ret = mt6311_read_interface(reg_address
					, &g_reg_value_mt6311, 0xFF, 0x0);
			pr_notice(MT6311TAG "[%s] read mt6311 reg 0x%x with value 0x%x !\n"
				  , __func__, reg_address, g_reg_value_mt6311);
			pr_notice(MT6311TAG "[%s] use \"cat mt6311_access\" to get value\n"
				  , __func__);
		}
	}
	return size;
#endif
}

/*664*/
static DEVICE_ATTR(mt6311_access
		, 0664, show_mt6311_access, store_mt6311_access);

/*
 * mt6311_user_space_probe
 */
static int mt6311_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	MT6311LOG("******** mt6311_user_space_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev)
					     , &dev_attr_mt6311_access);

	return 0;
}

static struct platform_device mt6311_user_space_device = {
	.name = "mt6311-user",
	.id = -1,
};

static struct platform_driver mt6311_user_space_driver = {
	.probe = mt6311_user_space_probe,
	.driver = {
		.name = "mt6311-user",
	},
};

static int __init mt6311_init(void)
{
	int ret = 0;

	/* MT6311 i2c driver register */
#ifdef IPIMB_MT6311
	pr_notice(MT6311TAG "Kernel driver not support for SSPM\n");
#else
	ret = i2c_add_driver(&mt6311_driver);
	if (ret != 0) {
		pr_notice(MT6311TAG "failed to register mt6311 i2c driver\n");
		return ret;
	}
	MT6311LOG("MT6311 i2c driver probe done\n");
#endif
	if (is_mt6311_exist()) {
		ret = platform_device_register(&mt6311_user_space_device);
		if (ret)
			pr_notice(MT6311TAG "failed to create mt6311_access file(device register fail)\n"
				  );
		ret = platform_driver_register(&mt6311_user_space_driver);
		if (ret)
			pr_notice(MT6311TAG "failed to create mt6311_access file(driver register fail)\n"
				  );
		MT6311LOG("MT6311 user space driver probe done\n");
	} else {
		MT6311LOG("MT6311 not exist!, user space driver not probe\n");
	}

	return ret;
}

static void __exit mt6311_exit(void)
{
	i2c_del_driver(&mt6311_driver);
}
module_init(mt6311_init);
module_exit(mt6311_exit);

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT6311 Device Driver");
MODULE_LICENSE("GPL");
