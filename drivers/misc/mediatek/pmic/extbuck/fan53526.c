/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/module.h>

#include "fan53526.h"
/*#include "mt_gpufreq.h"*//*TBD*/
#define PMIC_DEBUG_PR_DBG
/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/
#define FAN53526_SLAVE_ADDR_WRITE	0xC0
#define FAN53526_SLAVE_ADDR_READ	0xC1

#define fan53526_BUSNUM			7

static struct i2c_client *new_client;
static const struct i2c_device_id fan53526_i2c_id[] = { {"fan53526", 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id fan53526_of_ids[] = {
	{.compatible = "mediatek,ext_buck_lp4"}, /*TBD*/
	{},
};
#endif

static int fan53526_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver fan53526_driver = {
	.driver = {
		   .name = "fan53526",
#ifdef CONFIG_OF
		   .of_match_table = fan53526_of_ids,
#endif
		   },
	.probe = fan53526_driver_probe,
	.id_table = fan53526_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
unsigned char fan53526_reg[fan53526_REG_NUM] = { 0 };

static DEFINE_MUTEX(fan53526_i2c_access);
static DEFINE_MUTEX(fan53526_lock_mutex);

int g_fan53526_driver_ready = 0;
int g_fan53526_hw_exist = 0;

unsigned int g_fan53526_cid = 0;

#define PMICTAG                "[FAN53526] "
#if defined(PMIC_DEBUG_PR_DBG)
#define PMICLOG1(fmt, arg...)   pr_err(PMICTAG fmt, ##arg)
#else
#define PMICLOG1(fmt, arg...)
#endif

/**********************************************************
  *
  *   [I2C Function For Read/Write fan53526]
  *
  *********************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
unsigned int fan53526_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[1] = { 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&fan53526_i2c_access);


	/*
	   new_client->ext_flag =
	   ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;
	 */
	new_client->ext_flag =
	    ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_PUSHPULL_FLAG |
	    I2C_HS_FLAG;
	new_client->timing = 3400;


	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], (1 << 8 | 1));
	if (ret < 0) {
		PMICLOG1("[fan53526_read_byte] ret=%d\n", ret);

		new_client->ext_flag = 0;
		mutex_unlock(&fan53526_i2c_access);
		return ret;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	new_client->ext_flag = 0;

	mutex_unlock(&fan53526_i2c_access);
	return 1;
}

unsigned int fan53526_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&fan53526_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;


	/* new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_DIRECTION_FLAG; */
	new_client->ext_flag =
	    ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_DIRECTION_FLAG | I2C_PUSHPULL_FLAG |
	    I2C_HS_FLAG;
	new_client->timing = 3400;


	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		PMICLOG1("[fan53526_write_byte] ret=%d\n", ret);

		new_client->ext_flag = 0;
		mutex_unlock(&fan53526_i2c_access);
		return ret;
	}
	new_client->ext_flag = 0;
	mutex_unlock(&fan53526_i2c_access);
	return 1;
}
#else
unsigned int fan53526_read_byte(unsigned char cmd, unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 1;

	mutex_lock(&fan53526_i2c_access);

	do {
		struct i2c_msg msgs[2] = {
			{
			 .addr = new_client->addr,
			 .flags = 0,
			 .len = 1,
			 .buf = &cmd,
			 }, {

			     .addr = new_client->addr,
			     .flags = I2C_M_RD,
			     .len = 1,
			     .buf = returnData,
			     }
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			PMICLOG1("skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&fan53526_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int fan53526_write_byte(unsigned char cmd, unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 1;
	unsigned char buf[8];


	mutex_lock(&fan53526_i2c_access);

	buf[0] = cmd;
	memcpy(&buf[1], &writeData, 1);

	do {
		struct i2c_msg msgs[1] = {
			{
			 .addr = new_client->addr,
			 .flags = 0,
			 .len = 1 + 1,
			 .buf = buf,
			 },
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			PMICLOG1("skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&fan53526_i2c_access);

	return ret == xfers ? 1 : -1;
}
#endif

/*
 *   [Read / Write Function]
 */
unsigned int fan53526_read_interface(unsigned char RegNum, unsigned char *val, unsigned char MASK,
				     unsigned char SHIFT)
{
	unsigned char fan53526_reg = 0;
	unsigned int ret = 0;

	/* PMICLOG1("--------------------------------------------------\n"); */

	ret = fan53526_read_byte(RegNum, &fan53526_reg);

	/* PMICLOG1("[fan53526_read_interface] Reg[%x]=0x%x\n", RegNum, fan53526_reg); */

	fan53526_reg &= (MASK << SHIFT);
	*val = (fan53526_reg >> SHIFT);

	/* PMICLOG1("[fan53526_read_interface] val=0x%x\n", *val); */

	return ret;
}

unsigned int fan53526_config_interface(unsigned char RegNum, unsigned char val, unsigned char MASK,
				       unsigned char SHIFT)
{
	unsigned char fan53526_reg = 0;
	unsigned int ret = 0;

	/*PMICLOG1("--------------------------------------------------\n"); */

	ret = fan53526_read_byte(RegNum, &fan53526_reg);
	/* PMICLOG1("[fan53526_config_interface] Reg[%x]=0x%x\n", RegNum, fan53526_reg); */

	fan53526_reg &= ~(MASK << SHIFT);
	fan53526_reg |= (val << SHIFT);

	ret = fan53526_write_byte(RegNum, fan53526_reg);
	/*PMICLOG1("[fan53526_config_interface] write Reg[%x]=0x%x\n", RegNum, fan53526_reg); */

	/* Check */
	/*ret = fan53526_read_byte(RegNum, &fan53526_reg);
	   PMICLOG1("[fan53526_config_interface] Check Reg[%x]=0x%x\n", RegNum, fan53526_reg);
	 */

	return ret;
}

void fan53526_set_reg_value(unsigned int reg, unsigned int reg_val)
{
	unsigned int ret = 0;

	ret = fan53526_config_interface((unsigned char)reg, (unsigned char)reg_val, 0xFF, 0x0);
}

unsigned int fan53526_get_reg_value(unsigned int reg)
{
	unsigned int ret = 0;
	unsigned char reg_val = 0;

	ret = fan53526_read_interface((unsigned char)reg, &reg_val, 0xFF, 0x0);

	return reg_val;
}

/*
 *   [APIs]
 */
void fan53526_lock(void)
{
	mutex_lock(&fan53526_lock_mutex);
}

void fan53526_unlock(void)
{
	mutex_unlock(&fan53526_lock_mutex);
}


/*
 *   [Internal Function]
 */
void fan53526_dump_register(void)
{
	unsigned char i = 0;

	PMICLOG1("[fan53526] ");
	for (i = 0; i < fan53526_REG_NUM; i++)
		PMICLOG1("[0x%x]=0x%x\n", i, fan53526_get_reg_value(i));

}

int get_fan53526_i2c_ch_num(void)
{
	return fan53526_BUSNUM;
}

void fan53526_hw_init(void)
{
	PMICLOG1("[fan53526_hw_init] 20160313\n");
#if 0
	unsigned int ret = 0;

	ret = fan53526_config_interface(0x01, 0xB2, 0xFF, 0);	/* VSEL=high, 1.1V */
	/*
	   if(g_vproc_vsel_gpio_number!=0)
	   {
	   ext_buck_vproc_vsel(1);
	   pr_notice( "[fan53526_hw_init] ext_buck_vproc_vsel(1)\n");
	   }
	 */
	ret = fan53526_config_interface(0x00, 0x8A, 0xFF, 0);	/* VSEL=low, 0.7V */
	ret = fan53526_config_interface(0x02, 0xA0, 0xFF, 0);

	pr_notice("[fan53526_hw_init] Done\n");
#endif
}

void fan53526_hw_component_detect(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;
	/*unsigned char PGOOD = 0;*/

	ret = fan53526_read_interface(0x03, &val, 0x7, 5);
	/*ret = fan53526_read_interface(0x05, &PGOOD, 0x1, 7);*/

	/* check vender ID */
	/*if (val == 0x4 && PGOOD)*/
	if (val == 0x4)
		g_fan53526_hw_exist = 1;
	else
		g_fan53526_hw_exist = 0;


	/*PMICLOG1("[fan53526_hw_component_detect] exist=%d, pgood=%d, Reg[0x03][7:5]=0x%x\n",
		 PGOOD, g_fan53526_hw_exist, val);*/
	PMICLOG1("[fan53526_hw_component_detect] exist=%d, Reg[0x03][7:5]=0x%x\n", g_fan53526_hw_exist, val);
}

int is_fan53526_sw_ready(void)
{
	/*PMICLOG1("g_fan53526_driver_ready=%d\n", g_fan53526_driver_ready); */

	return g_fan53526_driver_ready;
}

int is_fan53526_exist(void)
{
	/*PMICLOG1("g_fan53526_hw_exist=%d\n", g_fan53526_hw_exist); */

	return g_fan53526_hw_exist;
}

#if 0
int fan53526_vosel(unsigned long val)
{
	int ret = 1;
	unsigned long reg_val = 0;

	/* 0.603~1.411V (step 12.826mv) */
	/*reg_val = (((val * 1000) - 603000) + 6413) / 12826;*/
	/* 0.6~1.39375V (step 6.25mv) */
	reg_val = (((val * 1000) - 600000)) / 6250;

	if (reg_val > 63)
		reg_val = 63;

	ret = fan53526_config_interface(0x00, reg_val, 0x3F, 0);
	/* ret = fan53526_config_interface(0x00, 0x1, 0x1, 7); */

	/* pr_notice("[fan53526_vosel] val=%ld, reg_val=%ld\n", val, reg_val); */

	return ret;
}
#endif

int fan53526_is_enabled(void)
{
	int ret = 1;
	unsigned char en = 0;

	ret = fan53526_read_interface(0x00, &en, 0x1, 7);
	pr_debug("[fan53526_is_enabled] en = %d\n", en);

	return en;
}

/* en = 1 enable */
/* en = 0 disable */
int fan53526_enable(unsigned char en)
{
	int ret = 1;

	if (en > 1) {
		pr_err("[fan53526_enable] error en = %d only 0 or 1\n", en);
		return -1;
	}

	ret = fan53526_config_interface(0x00, en, 0x1, 7);
	pr_debug("[fan53526_enable] en=%d\n", en);
	return ret;
}

int fan53526_set_voltage(unsigned long val)
{
	int ret = 1;
	unsigned long reg_val = 0;

	/* 0.603~1.411V (step 12.826mv) */
	/*reg_val = (((val * 1000) - 603000) + 6413) / 12826;*/
	/* 0.6~1.39375V (step 6.25mv) */
	if (val < 600000) {
		pr_err("[fan53526_set_voltage] val = %ld < 600000uV\n", val);
		return -1;
	}

	reg_val = (val - 600000) / 6250;

	if (reg_val > 127)
		reg_val = 127;

	ret = fan53526_config_interface(0x00, reg_val, 0x7F, 0);
	pr_notice("[fan53526_set_voltage] val=%ld, reg_val=%ld\n", val, reg_val);

	return ret;
}

unsigned int fan53526_get_voltage(void)
{
	unsigned int ret = 0, vol = 0;
	unsigned char step = 0;

	ret = fan53526_read_interface(0x00, &step, 0x7F, 0);

	vol = ((step * 6250)+600000);

	return vol;
}

/* mode = 1 force PWM mode */
/* mode = 0 auto mode      */
int fan53526_set_mode(unsigned char mode)
{
	int ret = 1;

	if (mode > 1) {
		pr_err("[fan53526_set_mode] error mode = %d only 0 or 1\n", mode);
		return -1;
	}

	ret = fan53526_config_interface(0x02, mode, 0x3, 0);
	pr_notice("[fan53526_set_mode] mode=%d\n", mode);
	return ret;
}

static int fan53526_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	PMICLOG1("[fan53526_driver_probe]\n");
	new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (new_client == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	memset(new_client, 0, sizeof(struct i2c_client));

	new_client = client;

	/* --------------------- */
	fan53526_hw_component_detect();
	if (g_fan53526_hw_exist == 1) {
		fan53526_hw_init();
		fan53526_dump_register();
	}
	g_fan53526_driver_ready = 1;

	PMICLOG1("[fan53526_driver_probe] g_fan53526_hw_exist=%d, g_fan53526_driver_ready=%d\n",
		 g_fan53526_hw_exist, g_fan53526_driver_ready);

	if (g_fan53526_hw_exist == 0) {
		PMICLOG1("[fan53526_driver_probe] return err\n");
		return err;
	}
	/*mt_gpufreq_fan53526_init();*//*TBD*/
	return 0;

exit:
	PMICLOG1("[fan53526_driver_probe] exit: return err\n");
	return err;
}

/*
 *   [platform_driver API]
 */
#ifdef FAN53526_AUTO_DETECT_DISABLE
    /* TBD */
#else
/*
 * fan53526_access
 */
unsigned char g_reg_value_fan53526 = 0;
static ssize_t show_fan53526_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	PMICLOG1("[show_fan53526_access] 0x%x\n", g_reg_value_fan53526);
	return sprintf(buf, "%u\n", g_reg_value_fan53526);
}

static ssize_t store_fan53526_access(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	int ret;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_err("[store_fan53526_access]\n");

	if (buf != NULL && size != 0) {
		/*PMICLOG1("[store_fan53526_access] buf is %s and size is %d\n",buf,size); */
		/*reg_address = simple_strtoul(buf, &pvalue, 16); */

		pvalue = (char *)buf;
		if (size > 5) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16, (unsigned int *)&reg_address);
		/*ret = kstrtoul(buf, 16, (unsigned long *)&reg_address); */

		if (size > 5) {
			/*reg_value = simple_strtoul((pvalue + 1), NULL, 16); */
			val = strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
			pr_err
			    ("[store_fan53526_access] write fan53526 reg 0x%x with value 0x%x !\n",
			     reg_address, reg_value);

			ret = fan53526_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret =
			    fan53526_read_interface(reg_address, &g_reg_value_fan53526, 0xFF, 0x0);

			pr_err("[store_fan53526_access] read fan53526 reg 0x%x with value 0x%x !\n",
			       reg_address, g_reg_value_fan53526);
			pr_err
			    ("[store_fan53526_access] use \"cat fan53526_access\" to get value(decimal)\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(fan53526_access, 0664, show_fan53526_access, store_fan53526_access);	/* 664 */

/*
 * fan53526_user_space_probe
 */
static int fan53526_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	PMICLOG1("******** fan53526_user_space_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_fan53526_access);

	return 0;
}

struct platform_device fan53526_user_space_device = {
	.name = "fan53526-user",
	.id = -1,
};

static struct platform_driver fan53526_user_space_driver = {
	.probe = fan53526_user_space_probe,
	.driver = {
		   .name = "fan53526-user",
		   },
};

/*
static struct i2c_board_info __initdata i2c_fan53526 =
{  I2C_BOARD_INFO("XXXXXXXX", (FAN53526_SLAVE_ADDR_WRITE >> 1)) };
*/

#endif

static int __init fan53526_init(void)
{
#ifdef FAN53526_AUTO_DETECT_DISABLE

	PMICLOG1("[fan53526_init] FAN53526_AUTO_DETECT_DISABLE\n");
	g_fan53526_hw_exist = 0;
	g_fan53526_driver_ready = 1;

#else

	int ret = 0;

	/* if (g_vproc_vsel_gpio_number != 0) { */
	if (1) {
		PMICLOG1("[fan53526_init] init start. ch=%d!!\n", fan53526_BUSNUM);

		/* i2c_register_board_info(fan53526_BUSNUM, &i2c_fan53526, 1); */

		if (i2c_add_driver(&fan53526_driver) != 0)
			PMICLOG1("[fan53526_init] failed to register fan53526 i2c driver.\n");
		else
			PMICLOG1("[fan53526_init] Success to register fan53526 i2c driver.\n");

		/* fan53526 user space access interface */
		ret = platform_device_register(&fan53526_user_space_device);
		if (ret) {
			PMICLOG1("****[fan53526_init] Unable to device register(%d)\n", ret);
			return ret;
		}
		ret = platform_driver_register(&fan53526_user_space_driver);
		if (ret) {
			PMICLOG1("****[fan53526_init] Unable to register driver (%d)\n", ret);
			return ret;
		}
	} else {
		pr_notice("[fan53526_init] DCT no define EXT BUCK\n");
		g_fan53526_hw_exist = 0;
		g_fan53526_driver_ready = 1;
	}

#endif

	return 0;
}

static void __exit fan53526_exit(void)
{
	i2c_del_driver(&fan53526_driver);
}
module_init(fan53526_init);
module_exit(fan53526_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C fan53526 Driver");
MODULE_AUTHOR("Jimmy-YJ Huang");
