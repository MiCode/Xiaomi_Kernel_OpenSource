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

#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>

#include <mach/mt_charging.h>
#include <mt-plat/charging.h>

#include "mt6336.h"

/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/

#ifdef CONFIG_OF
#else
#define mt6336_SLAVE_ADDR_WRITE   0xD6
#define mt6336_SLAVE_ADDR_Read    0xD7

#ifdef I2C_SWITHING_CHARGER_CHANNEL
#define mt6336_BUSNUM I2C_SWITHING_CHARGER_CHANNEL
#else
#define mt6336_BUSNUM 0
#endif

#endif
static struct i2c_client *new_client;
static const struct i2c_device_id mt6336_i2c_id[] = { {"mt6336", 0}, {} };
kal_bool chargin_hw_init_done = false;
static DEFINE_MUTEX(mt6336_i2c_access);
int g_mt6336_hw_exist = 0;


/**********************************************************
  *
  *   [I2C Function For Read/Write mt6336]
  *
  *********************************************************/
#define CODA_ADDR_WIDTH 0x100
#define SLV_BASE_ADDR 0x52

unsigned int mt6336_read_byte(unsigned int reg, unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 1;
	unsigned short addr;
	unsigned char cmd;

	mutex_lock(&mt6336_i2c_access);

	addr = reg / CODA_ADDR_WIDTH + SLV_BASE_ADDR;
	cmd = reg % CODA_ADDR_WIDTH;

	do {
		struct i2c_msg msgs[2] = {
			{
				.addr = addr,
				.flags = 0,
				.len = 1,
				.buf = &cmd,
			},
			{

				.addr = addr,
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
			PMICLOG("skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&mt6336_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int mt6336_read_bytes(unsigned int reg, unsigned char *returnData, unsigned int len)
{
	unsigned char xfers = 2;
	int ret, retries = 1;
	unsigned short addr;
	unsigned char cmd;

	mutex_lock(&mt6336_i2c_access);

	addr = reg / CODA_ADDR_WIDTH + SLV_BASE_ADDR;
	cmd = reg % CODA_ADDR_WIDTH;

	do {
		struct i2c_msg msgs[2] = {
			{
				.addr = addr,
				.flags = 0,
				.len = 1,
				.buf = &cmd,
			},
			{

				.addr = addr,
				.flags = I2C_M_RD,
				.len = len,
				.buf = returnData,
			}
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			PMICLOG("skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&mt6336_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int mt6336_write_byte(unsigned int reg, unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 1;
	unsigned char buf[8];
	unsigned short addr;
	unsigned char cmd;

	mutex_lock(&mt6336_i2c_access);

	addr = reg / CODA_ADDR_WIDTH + SLV_BASE_ADDR;
	cmd = reg % CODA_ADDR_WIDTH;

	buf[0] = cmd;
	memcpy(&buf[1], &writeData, 1);

	do {
		struct i2c_msg msgs[1] = {
			{
				.addr = addr,
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
			PMICLOG("skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&mt6336_i2c_access);

	return ret == xfers ? 1 : -1;
}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
unsigned int mt6336_read_interface(unsigned int RegNum, unsigned char *val, unsigned char MASK,
				  unsigned char SHIFT)
{
	unsigned char mt6336_reg = 0;
	unsigned int ret = 0;
	unsigned char reg_val;

	ret = mt6336_read_byte(RegNum, &mt6336_reg);
	reg_val = mt6336_reg;

	mt6336_reg &= (MASK << SHIFT);
	*val = (mt6336_reg >> SHIFT);

	PMICLOG("[mt6336_read_interface] Reg[0x%x]=0x%x val=0x%x device_id=0x%x\n",
		RegNum, reg_val, *val, RegNum / CODA_ADDR_WIDTH + SLV_BASE_ADDR);


	return ret;
}

unsigned int mt6336_config_interface(unsigned int RegNum, unsigned char val, unsigned char MASK,
				    unsigned char SHIFT)
{
	unsigned char mt6336_reg = 0;
	unsigned int ret = 0;
	unsigned char reg_val;

	ret = mt6336_read_byte(RegNum, &mt6336_reg);
	reg_val = mt6336_reg;

	mt6336_reg &= ~(MASK << SHIFT);
	mt6336_reg |= (val << SHIFT);

	ret = mt6336_write_byte(RegNum, mt6336_reg);
	PMICLOG("[mt6336_config_interface] write Reg[0x%x] from 0x%x to 0x%x device_id=0x%x\n", RegNum,
		    reg_val, mt6336_reg, RegNum / CODA_ADDR_WIDTH + SLV_BASE_ADDR);

	return ret;
}

/* write one register directly */
unsigned int mt6336_set_register_value(unsigned int RegNum, unsigned char val)
{
	unsigned int ret = 0;

	ret = mt6336_write_byte(RegNum, val);

	return ret;
}

unsigned int mt6336_get_register_value(unsigned int RegNum)
{
	unsigned int ret = 0;
	unsigned char reg_val = 0;

	ret = mt6336_read_byte(RegNum, &reg_val);

	return reg_val;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
void mt6336_hw_component_detect(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = mt6336_read_interface(0x00, &val, 0xFF, 0x0);

	if (val != 0x36)
		g_mt6336_hw_exist = 0;
	else
		g_mt6336_hw_exist = 1;

	PMICLOG("[mt6336_hw_component_detect] exist=%d, Reg[0x03]=0x%x\n",
		 g_mt6336_hw_exist, val);
}

int is_mt6336_exist(void)
{
	PMICLOG("[is_mt6336_exist] g_mt6336_hw_exist=%d\n", g_mt6336_hw_exist);

	return g_mt6336_hw_exist;
}

void mt6336_dump_register(void)
{
}

void mt6336_hw_init(void)
{
	/*PMICLOG("[mt6336_hw_init] After HW init\n");*/
	mt6336_dump_register();
}

/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
unsigned char g_reg_value_mt6336 = 0;
static ssize_t show_mt6336_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	PMICLOG("[show_mt6336_access] 0x%x\n", g_reg_value_mt6336);
	return sprintf(buf, "%u\n", g_reg_value_mt6336);
}

static ssize_t store_mt6336_access(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	PMICLOG("[store_mt6336_access]\n");
	if (buf != NULL && size != 0) {
		PMICLOG("[store_mt6336_access] buf is %s , size is %d\n", buf, (int)size);
		/*reg_address = simple_strtoul(buf, &pvalue, 16);*/

		pvalue = (char *)buf;
		if (size > 5) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16, (unsigned int *)&reg_address);

		if (size > 5) {
			/*reg_value = simple_strtoul((pvalue + 1), NULL, 16);*/
			/*pvalue = (char *)buf + 1;*/
			val =  strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);

			PMICLOG("[store_mt6336_access] write PMU reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			ret = mt6336_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret = mt6336_read_interface(reg_address, &g_reg_value_mt6336, 0xFF, 0x0);
			PMICLOG("[store_mt6336_access] read PMU reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_mt6336);
			PMICLOG("[store_mt6336_access] use \"cat pmic_access\" to get value(decimal)\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(mt6336_access, 0664, show_mt6336_access, store_mt6336_access);	/* 664 */


/*****************************************************************************
 * HW Setting
 ******************************************************************************/
static int proc_dump_register_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "********** dump mt6336 registers**********\n");

	for (i = 0; i <= 0x0737; i = i + 10) {
		seq_printf(m, "Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x\n",
			i, mt6336_get_register_value(i), i + 1, mt6336_get_register_value(i + 1), i + 2,
			mt6336_get_register_value(i + 2), i + 3, mt6336_get_register_value(i + 3), i + 4,
			mt6336_get_register_value(i + 4));

		seq_printf(m, "Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x\n",
			i + 5, mt6336_get_register_value(i + 5), i + 6, mt6336_get_register_value(i + 6),
			i + 7, mt6336_get_register_value(i + 7), i + 8, mt6336_get_register_value(i + 8),
			i + 9, mt6336_get_register_value(i + 9));
	}

	return 0;
}

static int proc_dump_register_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_dump_register_show, NULL);
}

static const struct file_operations mt6336_dump_register_proc_fops = {
	.open = proc_dump_register_open,
	.read = seq_read,
};

void mt6336_debug_init(void)
{
	struct proc_dir_entry *mt6336_dir;

	mt6336_dir = proc_mkdir("mt6336", NULL);
	if (!mt6336_dir) {
		PMICLOG("fail to mkdir /proc/mt6336\n");
		return;
	}

	proc_create("dump_mt6336_reg", S_IRUGO | S_IWUSR, mt6336_dir, &mt6336_dump_register_proc_fops);
	PMICLOG("proc_create pmic_dump_register_proc_fops\n");
}

/*****************************************************************************
 * system function
 ******************************************************************************/
static int mt6336_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	PMICLOG("******** mt6336_user_space_probe!! ********\n");
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_mt6336_access);
	PMICLOG("[MT6336] device_create_file for EM : done.\n");

	mt6336_debug_init();
	PMICLOG("[MT6336] mt6336_debug_init : done.\n");

	return 0;
}

struct platform_device mt6336_user_space_device = {
	.name = "mt6336-user",
	.id = -1,
};

static struct platform_driver mt6336_user_space_driver = {
	.probe = mt6336_user_space_probe,
	.driver = {
		   .name = "mt6336-user",
		   },
};

static int mt6336_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	PMICLOG("[mt6336_driver_probe]\n");
	new_client = client;

	/* --------------------- */
	mt6336_hw_component_detect();
	mt6336_dump_register();

	/*MT6336 Interrupt Service*/
	MT6336_EINT_SETTING();
	PMICLOG("[MT6336_EINT_SETTING] Done\n");

	/* mt6336_hw_init(); //move to charging_hw_xxx.c */
	chargin_hw_init_done = true;
	PMICLOG("[mt6336_driver_probe] Done\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt6336_of_match[] = {
	{.compatible = "mediatek,sw_charger"},
	{},
};
#else
static struct i2c_board_info i2c_mt6336 __initdata = {
	I2C_BOARD_INFO("mt6336", (mt6336_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver mt6336_driver = {
	.driver = {
		   .name = "mt6336",
#ifdef CONFIG_OF
		   .of_match_table = mt6336_of_match,
#endif
		   },
	.probe = mt6336_driver_probe,
	.id_table = mt6336_i2c_id,
};

/*****************************************************************************
 * MT6336 mudule init/exit
 ******************************************************************************/
static int __init mt6336_init(void)
{
	int ret = 0;

#if !defined CONFIG_HAS_WAKELOCKS
	wakeup_source_init(&mt6336Thread_lock, "BatThread_lock_mt6336 wakelock");
#else
	wake_lock_init(&mt6336Thread_lock, WAKE_LOCK_SUSPEND, "BatThread_lock_mt6336 wakelock");
#endif

	/* i2c registeration using DTS instead of boardinfo*/
#ifdef CONFIG_OF
	PMICLOG("[mt6336_init] init start with i2c DTS");
#else
	PMICLOG("[mt6336_init] init start. ch=%d\n", mt6336_BUSNUM);
	i2c_register_board_info(mt6336_BUSNUM, &i2c_mt6336, 1);
#endif
	if (i2c_add_driver(&mt6336_driver) != 0)
		PMICLOG("[mt6336_init] failed to register mt6336 i2c driver.\n");
	else
		PMICLOG("[mt6336_init] Success to register mt6336 i2c driver.\n");

	/* mt6336 user space access interface */
	ret = platform_device_register(&mt6336_user_space_device);
	if (ret) {
		PMICLOG("****[mt6336_init] Unable to device register(%d)\n",
			    ret);
		return ret;
	}
	ret = platform_driver_register(&mt6336_user_space_driver);
	if (ret) {
		PMICLOG("****[mt6336_init] Unable to register driver (%d)\n",
			    ret);
		return ret;
	}

	return 0;
}

static void __exit mt6336_exit(void)
{
	i2c_del_driver(&mt6336_driver);
}
module_init(mt6336_init);
module_exit(mt6336_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C MT6336 Driver");
MODULE_AUTHOR("Jeter Chen");
