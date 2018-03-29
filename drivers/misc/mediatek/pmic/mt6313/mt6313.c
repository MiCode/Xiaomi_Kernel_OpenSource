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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>

/*#include <asm/atomic.h>*/

#if defined CONFIG_MTK_LEGACY
#include <mt-plat/mt_gpio.h>
#endif
#include <mt-plat/mt_boot.h>
/*#include <mach/eint.h> TBD*/

#include <mt-plat/upmu_common.h>

#include "mt6313_upmu_hw.h"
#include "mt6313_api.h"
#include "mt6313.h"
#include "include/pmic.h"

#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <mach/mt_pmic.h>

#if defined(CONFIG_FPGA_EARLY_PORTING)
#else
#if defined CONFIG_MTK_LEGACY
/*#include <cust_i2c.h> TBD*/
/*#include <cust_eint.h> TBD*/
#endif
#endif

/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/
#define mt6313_SLAVE_ADDR_WRITE   0xD6
#define mt6313_SLAVE_ADDR_READ    0xD7

#define I2C_EXT_BUCK_CHANNEL 6 /*TBD for bring up only, need to change to DTS*/

#ifdef I2C_EXT_BUCK_CHANNEL
#define mt6313_BUSNUM I2C_EXT_BUCK_CHANNEL
#else
#define mt6313_BUSNUM 6
#endif

static struct i2c_client *new_client;
static const struct i2c_device_id mt6313_i2c_id[] = { {"mt6313", 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id mt6313_of_ids[] = {
			{.compatible = "mediatek,vproc_buck"},
			{},
};
#endif

static int mt6313_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver mt6313_driver = {
	.driver = {
			.name = "mt6313",
#ifdef CONFIG_OF
			.of_match_table = mt6313_of_ids,
#endif
		   },
	.probe = mt6313_driver_probe,
	.id_table = mt6313_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
static DEFINE_MUTEX(mt6313_i2c_access);
static DEFINE_MUTEX(mt6313_lock_mutex);

int g_mt6313_driver_ready = 0;
int g_mt6313_hw_exist = 0;

unsigned int g_mt6313_cid = 0;

/*----------------debug machanism-----------------------*/
unsigned int g_mt6313_logger;
unsigned int g_mt6313_dbgaddr;
unsigned char g_reg_value_mt6313 = 0;

unsigned int mt6313_logger_level_set(unsigned int level, unsigned int addr)
{
	g_mt6313_logger = level >= MT6313LOGLV ? level : MT6313LOGLV;
	g_mt6313_dbgaddr = addr;
	return 0;
}

static ssize_t mt6313_logger_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char *info, *pdbg_lv, *paddr;
	unsigned int dbg_lv_value = 0;
	unsigned int reg_addr = 0;
	int ret = 0;

	info = kmalloc_array(size, sizeof(char), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	memset(info, 0, size);

	if (copy_from_user(info, buf, size))
		return -EFAULT;

	info[size-1] = '\0';

	/* pr_err(PMICTAG "[logger_write] buf is %s, info is %s, size is %zu\n", buf, info, size); */

	if (size != 0) {
		if (size > 2) {
			pdbg_lv = strsep(&info, " ");
			/* pr_err(PMICTAG "[logger_write] pdbg_lv is %s, info is %s\n", pdbg_lv, info); */
			ret = kstrtou32(pdbg_lv, 16, (unsigned int *)&dbg_lv_value);
		}

		if (size > 2) {
			/*reg_value = simple_strtoul((pvalue + 1), NULL, 16);*/
			/*pvalue = (char *)buf + 1;*/
			paddr = strsep(&info, " ");
			/* pr_err(PMICTAG "[logger_write] paddr is %s, info is %s\n", paddr, info); */
			ret = kstrtou32(paddr, 16, (unsigned int *)&reg_addr);
		}
	}

	pr_err(PMICTAG "[logger_write] dbg_lv_value = %d, addr = 0x%x\n", dbg_lv_value, reg_addr);

	mt6313_logger_level_set(dbg_lv_value, reg_addr);

	pr_err(PMICTAG "mt6313_logger = %d\n", g_mt6313_logger);
	pr_err(PMICTAG "mt6313_dbg_addr = 0x%x\n", g_mt6313_dbgaddr);

	kfree(info);

	return size;
}

static ssize_t mt6313_access_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char *info, *addr = NULL, *val = NULL;
	unsigned int reg_value = 0;
	unsigned int reg_addr = 0;
	int ret = 0;

	info = kmalloc_array(size, sizeof(char), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	memset(info, 0, size);

	if (copy_from_user(info, buf, size))
		return -EFAULT;

	info[size-1] = '\0';

	/* pr_err(PMICTAG "[access_write] buf is %s, info is %s, size is %zu\n", buf, info, size); */

	if (size != 0) {
		if (size > 5) {
			addr = strsep(&info, " ");
			/* pr_err(PMICTAG "[access_write] paddr is %s, info is %s\n", addr, info); */
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_addr);
		} else
			ret = kstrtou32(info, 16, (unsigned int *)&reg_addr);

		if (size > 5) {
			val = strsep(&info, " ");
			/* pr_err(PMICTAG "[access_write] pvalue is %s, info is %s\n", val, info); */
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
			ret = mt6313_config_interface(reg_addr, reg_value, 0xFF, 0x0);
			g_reg_value_mt6313 = reg_value;
		} else {
			ret = mt6313_read_interface(reg_addr, &g_reg_value_mt6313, 0xFF, 0x0);

			pr_err(PMICTAG "[access_write] read mt6313 reg 0x%x with value 0x%x !\n",
				reg_addr, g_reg_value_mt6313);
		}
	}
	pr_err(PMICTAG "[access_write] addr = 0x%x, value = 0x%x\n", reg_addr, reg_value);
	pr_err(PMICTAG "[store_mt6313_access] use \"cat mt6313_access\"\r\n");

	kfree(info);

	return size;
}

static ssize_t mt6313_dump_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	mt6313_dump_register();
	return size;
}

static int mt6313_logger_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "mt6313_logger  = %d\n", g_mt6313_logger);
	seq_printf(s, "mt6313_dbg_addr = 0x%x\n", g_mt6313_dbgaddr);
	return 0;
}

static int mt6313_logger_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt6313_logger_show, NULL);
}

static int mt6313_dump_show(struct seq_file *s, void *unused)
{
	return 0;
}

static int mt6313_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt6313_dump_show, NULL);
}

static int mt6313_access_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "[show_mt6313_access] 0x%x\n", g_reg_value_mt6313);
	return 0;
}

static int mt6313_access_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt6313_access_show, NULL);
}

static const struct file_operations mt6313_logger_op = {
	.open    = mt6313_logger_open,
	.read    = seq_read,
	.write   = mt6313_logger_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations mt6313_dump_op = {
	.open    = mt6313_dump_open,
	.read    = seq_read,
	.write   = mt6313_dump_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations mt6313_access_op = {
	.open    = mt6313_access_open,
	.read    = seq_read,
	.write   = mt6313_access_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init mt6313_debugfs_init(void)
{
	struct dentry *mtk_extbuck_dir;

	mtk_extbuck_dir = debugfs_create_dir("mtk_extbuck", NULL);
	if (!mtk_extbuck_dir) {
		pr_err(PMICTAG "fail to mkdir /sys/kernel/debug/mtk_extbuck\n");
		return -1;
	}
	debugfs_create_file("mt6313_logger", S_IFREG | S_IRUGO,
		mtk_extbuck_dir, NULL, &mt6313_logger_op);
	debugfs_create_file("mt6313_dump_all_reg", S_IFREG | S_IRUGO,
		mtk_extbuck_dir, NULL, &mt6313_dump_op);
	debugfs_create_file("mt6313_access", S_IFREG | S_IRUGO,
		mtk_extbuck_dir, NULL, &mt6313_access_op);
	return 0;
}

subsys_initcall(mt6313_debugfs_init);
/*----------------debug machanism-----------------------*/

/**********************************************************
  *
  *   [I2C Function For Read/Write mt6313]
  *
  *********************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
unsigned int mt6313_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[1] = { 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&mt6313_i2c_access);
#if 1
	new_client->ext_flag =
	    ((new_client->
	      ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_PUSHPULL_FLAG | I2C_HS_FLAG;
	new_client->timing = 3400;
#else
	new_client->ext_flag =
	    ((new_client->
	      ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_PUSHPULL_FLAG;
	new_client->timing = 100;
#endif

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], (1 << 8 | 1));
	if (ret < 0) {
		pr_err(PMICTAG "[mt6313_read_byte] ret=%d\n", ret);

		new_client->ext_flag = 0;
		mutex_unlock(&mt6313_i2c_access);
		return ret;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	new_client->ext_flag = 0;

	mutex_unlock(&mt6313_i2c_access);
	return 1;
}

unsigned int mt6313_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&mt6313_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

#if 1
	new_client->ext_flag =
	    ((new_client->
	      ext_flag) & I2C_MASK_FLAG) | I2C_DIRECTION_FLAG | I2C_PUSHPULL_FLAG | I2C_HS_FLAG;
	new_client->timing = 3400;
#else
	new_client->ext_flag =
	    ((new_client->
	      ext_flag) & I2C_MASK_FLAG) | I2C_PUSHPULL_FLAG;
	new_client->timing = 100;
#endif

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		pr_err(PMICTAG "[mt6313_write_byte] ret=%d\n", ret);

		new_client->ext_flag = 0;
		mutex_unlock(&mt6313_i2c_access);
		return ret;
	}

	new_client->ext_flag = 0;
	mutex_unlock(&mt6313_i2c_access);
	return 1;
}
#else
unsigned int mt6313_read_byte(unsigned char cmd, unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 1;

	mutex_lock(&mt6313_i2c_access);

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

		if (ret < 0) {
			pr_err(PMICTAG "skipping non-existent adapter %s\n", new_client->adapter->name);
			pr_err(PMICTAG "i2c_transfer error: (%d) %d\n", cmd, ret);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&mt6313_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int mt6313_write_byte(unsigned char cmd, unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 1;
	unsigned char buf[8];


	mutex_lock(&mt6313_i2c_access);

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

		if (ret < 0) {
			pr_err(PMICTAG "skipping non-existent adapter %s\n", new_client->adapter->name);
			pr_err(PMICTAG "i2c_transfer error: (%d) %d\n", cmd, ret);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&mt6313_i2c_access);

	return ret == xfers ? 1 : -1;
}

#endif
/*
 *   [Read / Write Function]
 */
unsigned int mt6313_read_interface(unsigned char RegNum, unsigned char *val, unsigned char MASK, unsigned char SHIFT)
{
#if 0
	pr_err(PMICTAG "[mt6313_read_interface] HW no mt6313\n");
	*val = 0;
	return 1;
#else
	unsigned char mt6313_reg = 0;
	unsigned int ret = 0;

	/* pr_err(PMICTAG "--------------------------------------------------\n"); */

	ret = mt6313_read_byte(RegNum, &mt6313_reg);

	/* pr_err(PMICTAG "[mt6313_read_interface] Reg[%x]=0x%x\n", RegNum, mt6313_reg); */

	mt6313_reg &= (MASK << SHIFT);
	*val = (mt6313_reg >> SHIFT);

	/* pr_err(PMICTAG "[mt6313_read_interface] val=0x%x\n", *val); */

	return ret;
#endif
}

unsigned int mt6313_config_interface(unsigned char RegNum, unsigned char val, unsigned char MASK, unsigned char SHIFT)
{
#if 0
	pr_err(PMICTAG "[mt6313_config_interface] HW no mt6313\n");
	return 1;
#else
	unsigned char mt6313_reg = 0;
	unsigned int ret = 0;

	/* pr_err(PMICTAG "--------------------------------------------------\n"); */

	ret = mt6313_read_byte(RegNum, &mt6313_reg);
	/* pr_err(PMICTAG "[mt6313_config_interface] Reg[%x]=0x%x\n", RegNum, mt6313_reg);*/

	mt6313_reg &= ~(MASK << SHIFT);
	mt6313_reg |= (val << SHIFT);

	ret = mt6313_write_byte(RegNum, mt6313_reg);
	/* pr_err(PMICTAG "[mt6313_config_interface] write Reg[%x]=0x%x\n", RegNum, mt6313_reg);*/

	/* Check*/
	/*ret = mt6313_read_byte(RegNum, &mt6313_reg);
	pr_err(PMICTAG "[mt6313_config_interface] Check Reg[%x]=0x%x\n", RegNum, mt6313_reg);
	*/

	return ret;
#endif
}

void mt6313_set_reg_value(unsigned int reg, unsigned int reg_val)
{
	unsigned int ret = 0;

	ret = mt6313_config_interface((unsigned char) reg, (unsigned char) reg_val, 0xFF, 0x0);
}

unsigned int mt6313_get_reg_value(unsigned int reg)
{
	unsigned int ret = 0;
	unsigned char reg_val = 0;

	ret = mt6313_read_interface((unsigned char) reg, &reg_val, 0xFF, 0x0);

	return reg_val;
}

/*
 *   mt6313 special API
 */
unsigned char mt6313_get_register_value(mt6313_flag pmureg)
{
	unsigned char val;
	unsigned int ret;

	ret = mt6313_read_interface(pmureg.addr, &val, pmureg.mask, pmureg.shift);

	return val;
}

unsigned int mt6313_set_register_value(mt6313_flag pmureg, unsigned int val)
{
	unsigned int ret;

	ret = mt6313_config_interface(pmureg.addr, val, pmureg.mask, pmureg.shift);

	return ret;
}

/*
 *   mt6313 buck control start
 */
struct mt6313_bucks_t mt6313_bucks_class[] = {
	MT6313_BUCK_GEN1(VDVFS11,
			MT6313_PMIC_VDVFS11_EN_ADDR, MT6313_PMIC_VDVFS11_EN_CTRL_ADDR,
			MT6313_PMIC_RG_VDVFS11_MODESET_ADDR, MT6313_PMIC_VDVFS11_VOSEL_DVFS0_ADDR,
			MT6313_PMIC_VDVFS11_VOSEL_DVFS1_ADDR, MT6313_PMIC_VDVFS11_VOSEL_SLEEP_ADDR,
			MT6313_PMIC_QI_VDVFS11_EN_MASK, MT6313_PMIC_NI_VDVFS11_VOSEL_ADDR,
			450000, 1150000, 6250),
	MT6313_BUCK_GEN1(VDVFS12,
			MT6313_PMIC_VDVFS12_EN_ADDR, MT6313_PMIC_VDVFS12_EN_CTRL_ADDR,
			MT6313_PMIC_RG_VDVFS12_MODESET_ADDR, MT6313_PMIC_VDVFS12_VOSEL_DVFS0_ADDR,
			MT6313_PMIC_VDVFS12_VOSEL_DVFS1_ADDR, MT6313_PMIC_VDVFS12_VOSEL_SLEEP_ADDR,
			MT6313_PMIC_QI_VDVFS12_EN_MASK, MT6313_PMIC_NI_VDVFS12_VOSEL_ADDR,
			450000, 1150000, 6250),
	MT6313_BUCK_GEN1(VDVFS13,
			MT6313_PMIC_VDVFS13_EN_ADDR, MT6313_PMIC_VDVFS13_EN_CTRL_ADDR,
			MT6313_PMIC_RG_VDVFS13_MODESET_ADDR, MT6313_PMIC_VDVFS13_VOSEL_DVFS0_ADDR,
			MT6313_PMIC_VDVFS13_VOSEL_DVFS1_ADDR, MT6313_PMIC_VDVFS13_VOSEL_SLEEP_ADDR,
			MT6313_PMIC_QI_VDVFS13_EN_MASK, MT6313_PMIC_NI_VDVFS13_VOSEL_ADDR,
			450000, 1150000, 6250),
	MT6313_BUCK_GEN1(VDVFS14,
			MT6313_PMIC_VDVFS14_EN_ADDR, MT6313_PMIC_VDVFS14_EN_CTRL_ADDR,
			MT6313_PMIC_RG_VDVFS14_MODESET_ADDR, MT6313_PMIC_VDVFS14_VOSEL_DVFS0_ADDR,
			MT6313_PMIC_VDVFS14_VOSEL_DVFS1_ADDR, MT6313_PMIC_VDVFS14_VOSEL_SLEEP_ADDR,
			MT6313_PMIC_QI_VDVFS14_EN_MASK, MT6313_PMIC_NI_VDVFS14_VOSEL_ADDR,
			450000, 1150000, 6250),
};

static unsigned int mt6313_bucks_size = ARRAY_SIZE(mt6313_bucks_class);

char mt6313_is_enabled(MT6313_BUCK_TYPE type)
{
	if (type >= mt6313_bucks_size) {
		pr_err(PMICTAG "[MT6313] Set Wrong buck type\n");
		return -1;
	}

	return mt6313_get_register_value(mt6313_bucks_class[type].da_qi_en);

}
/* en = 1 enable */
/* en = 0 disable */
char mt6313_enable(MT6313_BUCK_TYPE type, unsigned char en)
{
	if (type >= mt6313_bucks_size) {
		pr_err(PMICTAG "[MT6313] Set Wrong buck type\n");
		return -1;
	}

	if (en > 1) {
		pr_err(PMICTAG "[MT6313] Set en > 1, only 0 or 1!!!!\n");
		return -1;
	}
	/*--EN SW CTRL--*/
	mt6313_set_register_value(mt6313_bucks_class[type].en_ctl, 0);
	/*--EN SW --*/
	mt6313_set_register_value(mt6313_bucks_class[type].en, en);

	if (mt6313_get_register_value(mt6313_bucks_class[type].da_qi_en) == en)
		pr_err(PMICTAG "%s Set en pass: 0x%x\n", mt6313_bucks_class[type].name, en);
	else
		pr_err(PMICTAG "%s Set en fail: 0x%x\n", mt6313_bucks_class[type].name, en);

	/*--return mt6313_get_register_value(mt6313_bucks_class[type].da_qi_en);--*/
	return mt6313_is_enabled(type);

}
/* pmode = 1 force PWM mode */
/* pmode = 0 auto mode      */
char mt6313_set_mode(MT6313_BUCK_TYPE type, unsigned char pmode)
{
	if (type >= mt6313_bucks_size) {
		pr_err(PMICTAG "[MT6313] Set Wrong buck type\n");
		return -1;
	}

	if (pmode > 1) {
		pr_err(PMICTAG "[MT6313] Set mode > 1, only 0 or 1!!!!\n");
		return -1;
	}

	mt6313_bucks_class[type].mode.shift = (type + 4);
	/*--MODESET--*/
	mt6313_set_register_value(mt6313_bucks_class[type].mode, pmode);

	if (mt6313_get_register_value(mt6313_bucks_class[type].mode) == pmode)
		pr_err(PMICTAG "%s Set mode pass: 0x%x\n", mt6313_bucks_class[type].name, pmode);
	else
		pr_err(PMICTAG "%s Set mode fail: 0x%x\n", mt6313_bucks_class[type].name, pmode);

	return mt6313_get_register_value(mt6313_bucks_class[type].mode);
}

char mt6313_set_voltage(MT6313_BUCK_TYPE type, unsigned int voltage)
{
	unsigned char value = 0;

	if (type >= mt6313_bucks_size) {
		pr_err(PMICTAG "[MT6313] Set Wrong buck type\n");
		return -1;
	}
	if (voltage > mt6313_bucks_class[type].max_uV || voltage < mt6313_bucks_class[type].min_uV) {
		pr_err(PMICTAG "[MT6313] Set Wrong buck voltage, range (%duV - %duV)\n",
				mt6313_bucks_class[type].min_uV, mt6313_bucks_class[type].max_uV);
		return -1;
	}

	value = (voltage - mt6313_bucks_class[type].min_uV)/(mt6313_bucks_class[type].uV_step);

	pr_err(PMICTAG "%s Expected volt step: 0x%x\n", mt6313_bucks_class[type].name, value);

	/*--VOSEL SW CTRL--*/
	mt6313_set_register_value(mt6313_bucks_class[type].vosel, value);
	/*--VOSEL HW CTRL--*/
	mt6313_set_register_value(mt6313_bucks_class[type].vosel_on, value);

	if (mt6313_get_register_value(mt6313_bucks_class[type].da_ni_vosel) == value)
		pr_err(PMICTAG "Set %s Voltage to %d pass\n", mt6313_bucks_class[type].name, value);
	else
		pr_err(PMICTAG "Set %s Voltage to %d fail\n", mt6313_bucks_class[type].name, value);

	return mt6313_get_register_value(mt6313_bucks_class[type].da_ni_vosel);
}

unsigned int mt6313_get_voltage(MT6313_BUCK_TYPE type)
{
	unsigned short value = 0;
	unsigned int voltage = 0;

	if (type >= mt6313_bucks_size) {
		pr_err(PMICTAG "Get Wrong buck type\n");
		return 0;
	}

	value = mt6313_get_register_value(mt6313_bucks_class[type].da_ni_vosel);

	voltage = ((value * (mt6313_bucks_class[type].uV_step)) + mt6313_bucks_class[type].min_uV);

	if (voltage > mt6313_bucks_class[type].max_uV || voltage < mt6313_bucks_class[type].min_uV) {
		pr_err(PMICTAG "Get Wrong buck voltage, range (%d - %d)\n",
				mt6313_bucks_class[type].min_uV, mt6313_bucks_class[type].max_uV);
		return 0;
	}

	return voltage;
}
/*-----mt6313 buck control end---*/

/*
 *   [LOCK APIs]
 */
void mt6313_lock(void)
{
	mutex_lock(&mt6313_lock_mutex);
}

void mt6313_unlock(void)
{
	mutex_unlock(&mt6313_lock_mutex);
}

/*
 *   [Internal Function]
 */
void mt6313_dump_register(void)
{
	int i = 0;
	int i_max = 0xD0;	/*0xD5*/

	for (i = 0; i <= i_max; i += 8) {
		pr_err(PMICTAG "[0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x\n",
			i, mt6313_get_reg_value(i),
			i+1, mt6313_get_reg_value(i+1),
			i+2, mt6313_get_reg_value(i+2),
			i+3, mt6313_get_reg_value(i+3));
		pr_err(PMICTAG "[0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x\n",
			i+4, mt6313_get_reg_value(i+4),
			i+5, mt6313_get_reg_value(i+5),
			i+6, mt6313_get_reg_value(i+6),
			i+7, mt6313_get_reg_value(i+7));
	}
}

int get_mt6313_i2c_ch_num(void)
{
	return mt6313_BUSNUM;
}

unsigned int update_mt6313_chip_id(void)
{
	unsigned int id = 0;
	unsigned int id_l = 0;
	unsigned int id_r = 0;

	id_l = mt6313_get_cid();
	if (id_l < 0) {
		pr_err(PMICTAG "[update_mt6313_chip_id] id_l=%d\n", id_l);
		return id_l;
	}
	id_r = mt6313_get_swcid();
	if (id_r < 0) {
		pr_err(PMICTAG "[update_mt6313_chip_id] id_r=%d\n", id_r);
		return id_r;
	}
	id = ((id_l << 8) | (id_r));

	g_mt6313_cid = id;

	pr_err(PMICTAG "[update_mt6313_chip_id] id_l=0x%x, id_r=0x%x, id=0x%x\n", id_l, id_r, id);

	return id;
}

unsigned int mt6313_get_chip_id(void)
{
	unsigned int ret = 0;

	if (g_mt6313_cid == 0) {
		ret = update_mt6313_chip_id();
		if (ret < 0) {
			g_mt6313_hw_exist = 0;
			pr_err(PMICTAG "[mt6313_get_chip_id] ret=%d hw_exist:%d\n", ret, g_mt6313_hw_exist);
			return ret;
		}
	}

	pr_err(PMICTAG "[mt6313_get_chip_id] g_mt6313_cid=0x%x\n", g_mt6313_cid);

	return g_mt6313_cid;
}

void mt6313_hw_init(void)
{
	unsigned int ret = 0;

	pr_err(PMICTAG "[mt6313_hw_init] 20140513, CC Lee\n");

	/*put init setting from DE/SA*/
	/*ret=mt6313_config_interface(0x04,0x11,0xFF,0); set pin to interrupt, DVT only*/

	if (mt6313_get_chip_id() >= PMIC6313_E1_CID_CODE) {
		pr_err(PMICTAG "[mt6313_hw_init] 6313 PMIC Chip = 0x%x\n", mt6313_get_chip_id());
		pr_err(PMICTAG "[mt6313_hw_init] 2016-03-28\n");

	/*put init setting from DE/SA*/
	ret = mt6313_config_interface(0x10, 0x1, 0x1, 0);
	ret = mt6313_config_interface(0x15, 0x1, 0x1, 6);
	ret = mt6313_config_interface(0x15, 0x1, 0x1, 7);
	ret = mt6313_config_interface(0x1B, 0x1, 0x1, 5);
	ret = mt6313_config_interface(0x38, 0x1, 0x1, 7);
	ret = mt6313_config_interface(0x5A, 0x1, 0x1, 7);
	ret = mt6313_config_interface(0x62, 0x0, 0x7, 5);
	ret = mt6313_config_interface(0x73, 0xB, 0xF, 0);
	ret = mt6313_config_interface(0x73, 0xB, 0xF, 4);
	ret = mt6313_config_interface(0x74, 0xB, 0xF, 0);
	ret = mt6313_config_interface(0x74, 0xB, 0xF, 4);
	ret = mt6313_config_interface(0x76, 0x3, 0x3, 4);
	ret = mt6313_config_interface(0x76, 0x3, 0x3, 6);
	ret = mt6313_config_interface(0x77, 0x1, 0x3, 0);
	ret = mt6313_config_interface(0x77, 0x1, 0x3, 2);
	ret = mt6313_config_interface(0x77, 0x1, 0x3, 4);
	ret = mt6313_config_interface(0x77, 0x1, 0x3, 6);
	ret = mt6313_config_interface(0x94, 0x1, 0x1, 1);
	ret = mt6313_config_interface(0x94, 0x1, 0x1, 2);
	ret = mt6313_config_interface(0x97, 0x2, 0x7F, 0);
	ret = mt6313_config_interface(0x98, 0xB, 0x7F, 0);
	ret = mt6313_config_interface(0xA0, 0x3, 0x3, 0);
	ret = mt6313_config_interface(0xB4, 0x1, 0x1, 1);
	ret = mt6313_config_interface(0xB4, 0x1, 0x1, 2);
	ret = mt6313_config_interface(0xB7, 0x2, 0x7F, 0);
	ret = mt6313_config_interface(0xB8, 0xB, 0x7F, 0);
	ret = mt6313_config_interface(0xC0, 0x3, 0x3, 0);
	ret = mt6313_config_interface(0xC4, 0x1, 0x1, 1);
	ret = mt6313_config_interface(0xC4, 0x1, 0x1, 2);
	ret = mt6313_config_interface(0xC7, 0x2, 0x7F, 0);
	ret = mt6313_config_interface(0xC8, 0xB, 0x7F, 0);
	ret = mt6313_config_interface(0xD0, 0x3, 0x3, 0);

#if 1
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x10, mt6313_get_reg_value(0x10));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x15, mt6313_get_reg_value(0x15));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x1B, mt6313_get_reg_value(0x1B));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x38, mt6313_get_reg_value(0x38));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x5A, mt6313_get_reg_value(0x5A));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x62, mt6313_get_reg_value(0x62));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x73, mt6313_get_reg_value(0x73));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x74, mt6313_get_reg_value(0x74));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x77, mt6313_get_reg_value(0x77));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x94, mt6313_get_reg_value(0x94));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x97, mt6313_get_reg_value(0x97));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0x98, mt6313_get_reg_value(0x98));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0xA0, mt6313_get_reg_value(0xA0));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0xB4, mt6313_get_reg_value(0xB4));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0xB7, mt6313_get_reg_value(0xB7));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0xB8, mt6313_get_reg_value(0xB8));
	pr_err(PMICTAG "[mt6313] [0x%x]=0x%x\n", 0xC0, mt6313_get_reg_value(0xC0));
#endif
	} else {
		pr_err(PMICTAG "[mt6313_hw_init] Unknown PMIC Chip (0x%x)\n", mt6313_get_chip_id());
	}
}

unsigned int mt6313_hw_component_detect(void)
{
	unsigned int ret = 0, chip_id = 0;

	ret = update_mt6313_chip_id();
	if (ret < 0) {
		g_mt6313_hw_exist = 0;
		pr_err(PMICTAG "[update_mt6313_chip_id] ret=%d hw_exist:%d\n", ret, g_mt6313_hw_exist);
		return ret;
	}

	chip_id = mt6313_get_chip_id();
	if (chip_id < 0) {
		pr_err(PMICTAG "[mt6313_get_chip_id] ret=%d\n", chip_id);
		return chip_id;
	}
	if ((chip_id == PMIC6313_E1_CID_CODE) ||
	(chip_id == PMIC6313_E2_CID_CODE) ||
	(chip_id == PMIC6313_E3_CID_CODE)
	){
		g_mt6313_hw_exist = 1;
	} else
		g_mt6313_hw_exist = 0;
	pr_err(PMICTAG "[mt6313_hw_component_detect] exist=%d\n", g_mt6313_hw_exist);
	return 0;
}

int is_mt6313_sw_ready(void)
{
	/*pr_err(PMICTAG "g_mt6313_driver_ready=%d\n", g_mt6313_driver_ready);*/

	return g_mt6313_driver_ready;
}

int is_mt6313_exist(void)
{
	/*pr_err(PMICTAG "g_mt6313_hw_exist=%d\n", g_mt6313_hw_exist);*/

	return g_mt6313_hw_exist;
}

/* mt6313 interrupt service */
#if 1
int g_mt6313_irq = 0;

#ifdef CUST_EINT_EXT_BUCK_OC_NUM
unsigned int g_eint_pmic_mt6313_num = CUST_EINT_EXT_BUCK_OC_NUM;
#else
unsigned int g_eint_pmic_mt6313_num = 14;	/*FPGA:0, phn:14*/
#endif

#ifdef CUST_EINT_EXT_BUCK_OC_DEBOUNCE_CN
unsigned int g_cust_eint_mt_pmic_mt6313_debounce_cn = CUST_EINT_EXT_BUCK_OC_DEBOUNCE_CN;
#else
unsigned int g_cust_eint_mt_pmic_mt6313_debounce_cn = 1;
#endif

#ifdef CUST_EINT_EXT_BUCK_OC_TYPE
unsigned int g_cust_eint_mt_pmic_mt6313_type = CUST_EINT_EXT_BUCK_OC_TYPE;
#else
unsigned int g_cust_eint_mt_pmic_mt6313_type = 4;
#endif

#ifdef CUST_EINT_EXT_BUCK_OC_DEBOUNCE_EN
unsigned int g_cust_eint_mt_pmic_mt6313_debounce_en = CUST_EINT_EXT_BUCK_OC_DEBOUNCE_EN;
#else
unsigned int g_cust_eint_mt_pmic_mt6313_debounce_en = 1;
#endif

static DEFINE_MUTEX(pmic_mutex_mt6313);
static struct task_struct *pmic_6313_thread_handle;
struct wake_lock pmicThread_lock_mt6313;

void wake_up_pmic_mt6313(void)
{
	pr_err(PMICTAG "[wake_up_pmic_mt6313]\n");
	wake_up_process(pmic_6313_thread_handle);
	wake_lock(&pmicThread_lock_mt6313);
}
EXPORT_SYMBOL(wake_up_pmic_mt6313);

void mt_pmic_eint_irq_mt6313(void)
{
	pr_err(PMICTAG "[mt_pmic_eint_irq_mt6313] receive interrupt\n");
	wake_up_pmic_mt6313();
}

void mt6313_int_test(void)
{
	pr_err(PMICTAG "[mt6313_int_test] start\n");

	mt6313_config_interface(0x20, 0x0F, 0xFF, 0);	/* pg dis */
	mt6313_set_rg_auxadc_ck_pdn(0);
	mt6313_set_rg_auxadc_1m_ck_pdn(0);
	mt6313_config_interface(0xB5, 0xC0, 0xFF, 0);	/* cc EN */
	mt6313_config_interface(0xAE, 0x03, 0xFF, 0);	/* ADC EN */
	mt6313_config_interface(0xAE, 0x00, 0xFF, 0);	/* ADC CLR */

	mt6313_set_auxadc_lbat_irq_en_max(0);
	mt6313_set_auxadc_lbat_irq_en_min(0);
	mt6313_set_auxadc_lbat_en_max(0);
	mt6313_set_auxadc_lbat_en_min(0);

	mt6313_set_auxadc_lbat_volt_max_1(0);
	mt6313_set_auxadc_lbat_volt_max_0(0);
	mt6313_set_auxadc_lbat_volt_min_1(0);
	mt6313_set_auxadc_lbat_volt_min_0(0);
	mt6313_set_auxadc_lbat_det_prd_19_16(0);
	mt6313_set_auxadc_lbat_det_prd_15_8(0);
	mt6313_set_auxadc_lbat_det_prd_7_0(0x1);
	mt6313_set_auxadc_lbat_debt_min(0x1);
	mt6313_set_auxadc_lbat_debt_max(0x1);

	mt6313_set_rg_int_pol(0);	/* high active */
	mt6313_set_rg_int_en(1);

	mt6313_set_auxadc_lbat_irq_en_max(1);
	mt6313_set_auxadc_lbat_irq_en_min(0);
	mt6313_set_auxadc_lbat_en_max(1);
	mt6313_set_auxadc_lbat_en_min(0);

	pr_err(PMICTAG "[mt6313_int_test] done, wait for interrupt..\n");
}

void cust_pmic_interrupt_en_setting_mt6313(void)
{
#if 1
	mt6313_set_rg_int_pol(0);	/* high active */
	mt6313_set_rg_int_en(1);
#endif

#if 0
	mt6313_int_test();
#endif
}

void mt6313_lbat_min_int_handler(void)
{
	/*unsigned int ret=0;*/
	pr_err(PMICTAG "[mt6313_lbat_min_int_handler]....\n");
	/*ret=mt6313_config_interface(MT6313_TOP_INT_MON,0x1,0x1,0);*/
}

void mt6313_lbat_max_int_handler(void)
{
	/*unsigned int ret=0;*/
	pr_err(PMICTAG "[mt6313_lbat_max_int_handler]....\n");

#if 0
	mt6313_set_auxadc_lbat_irq_en_max(0);
	mt6313_set_auxadc_lbat_irq_en_min(0);
	mt6313_set_auxadc_lbat_en_max(0);
	mt6313_set_auxadc_lbat_en_min(0);
	mt6313_set_rg_int_en(0);
#endif

	/*ret=mt6313_config_interface(MT6313_TOP_INT_MON,0x1,0x1,1);*/
}

unsigned int thr_l_int_status = 0;
unsigned int thr_h_int_status = 0;

void mt6313_clr_thr_l_int_status(void)
{
	thr_l_int_status = 0;
	pr_err(PMICTAG "[mt6313_clr_thr_l_int_status]....\n");
}

void mt6313_clr_thr_h_int_status(void)
{
	thr_h_int_status = 0;
	pr_err(PMICTAG "[mt6313_clr_thr_h_int_status]....\n");
}

unsigned int mt6313_get_thr_l_int_status(void)
{
	pr_err(PMICTAG "[mt6313_get_thr_l_int_status]....\n");

	return thr_l_int_status;
}

unsigned int mt6313_get_thr_h_int_status(void)
{
	pr_err(PMICTAG "[mt6313_get_thr_h_int_status]....\n");

	return thr_h_int_status;
}

void mt6313_thr_l_int_handler(void)
{
	/*unsigned int ret=0;*/
	thr_l_int_status = 1;
	pr_err(PMICTAG "[mt6313_thr_l_int_handler]....\n");
	/*return thr_l_int_status;*/

	/*ret=mt6313_config_interface(MT6313_TOP_INT_MON,0x1,0x1,2);*/
}

void mt6313_thr_h_int_handler(void)
{
	/*unsigned int ret=0;*/
	thr_h_int_status = 1;
	pr_err(PMICTAG "[mt6313_thr_h_int_handler]....\n");
	/*ret=mt6313_config_interface(MT6313_TOP_INT_MON,0x1,0x1,3);*/
}

void mt6313_buck_oc_int_handler(void)
{
	/*unsigned int ret=0;*/
	pr_err(PMICTAG "[mt6313_buck_oc_int_handler]....\n");
	/*ret=mt6313_config_interface(MT6313_TOP_INT_MON,0x1,0x1,4);*/
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static void mt6313_int_handler(void)
{
	unsigned int ret = 0;
	unsigned char mt6313_int_status_val_0 = 0;

	/*--------------------------------------------------------------------------------*/
	ret = mt6313_read_interface(MT6313_TOP_INT_MON, (&mt6313_int_status_val_0), 0xFF, 0x0);
	pr_err(PMICTAG "[MT6313_INT] mt6313_int_status_val_0=0x%x\n", mt6313_int_status_val_0);

	if ((((mt6313_int_status_val_0) & (0x01)) >> 0) == 1)
		mt6313_lbat_min_int_handler();
	if ((((mt6313_int_status_val_0) & (0x02)) >> 1) == 1)
		mt6313_lbat_max_int_handler();
	if ((((mt6313_int_status_val_0) & (0x04)) >> 2) == 1)
		mt6313_thr_l_int_handler();
	if ((((mt6313_int_status_val_0) & (0x08)) >> 3) == 1)
		mt6313_thr_h_int_handler();
	if ((((mt6313_int_status_val_0) & (0x10)) >> 4) == 1)
		mt6313_buck_oc_int_handler();
}

static int pmic_thread_kthread_mt6313(void *x)
{
	unsigned int ret = 0;
	unsigned char mt6313_int_status_val_0 = 0;
	struct sched_param param = {.sched_priority = 98 };

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	pr_err(PMICTAG "[MT6313_INT] enter\n");

	/* Run on a process content */
	while (1) {
		mutex_lock(&pmic_mutex_mt6313);

		mt6313_int_handler();

		cust_pmic_interrupt_en_setting_mt6313();

		ret =
		    mt6313_read_interface(MT6313_TOP_INT_MON, (&mt6313_int_status_val_0), 0xFF,
					  0x0);

		pr_err(PMICTAG "[MT6313_INT] after ,mt6313_int_status_val_0=0x%x\n",
			mt6313_int_status_val_0);

		mdelay(1);

		mutex_unlock(&pmic_mutex_mt6313);
		wake_unlock(&pmicThread_lock_mt6313);

		set_current_state(TASK_INTERRUPTIBLE);

		/* mt_eint_unmask(g_eint_pmic_mt6313_num);*/
		if (g_mt6313_irq != 0)
			enable_irq(g_mt6313_irq);

		schedule();
	}

	return 0;
}
#endif

irqreturn_t mt6313_eint_handler(int irq, void *desc)
{
	mt_pmic_eint_irq_mt6313();

	disable_irq_nosync(irq);
	return IRQ_HANDLED;
}

void mt6313_eint_setting(void)
{
	/*ON/OFF interrupt*/
	int ret;

	cust_pmic_interrupt_en_setting_mt6313();

#if 1
/*	g_mt6313_irq = mt_gpio_to_irq(g_eint_pmic_mt6313_num);*/

/*	mt_gpio_set_debounce(g_eint_pmic_mt6313_num, g_cust_eint_mt_pmic_mt6313_debounce_cn);*/

	ret = request_irq(g_mt6313_irq, mt6313_eint_handler, g_cust_eint_mt_pmic_mt6313_type, "mt6313-eint", NULL);
	if (ret)
		pr_err(PMICTAG "[CUST_EINT] Fail to register an irq=%d , err=%d\n", g_mt6313_irq, ret);

	pr_err(PMICTAG "[CUST_EINT] CUST_EINT_MT_PMIC_MT6313_NUM=%d\n", g_eint_pmic_mt6313_num);
	pr_err(PMICTAG "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n",
		g_cust_eint_mt_pmic_mt6313_debounce_cn);
	pr_err(PMICTAG "[CUST_EINT] CUST_EINT_PMIC_TYPE=%d\n", g_cust_eint_mt_pmic_mt6313_type);
	pr_err(PMICTAG "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n",
		g_cust_eint_mt_pmic_mt6313_debounce_en);
#else
	mt_eint_set_hw_debounce(g_eint_pmic_mt6313_num, g_cust_eint_mt_pmic_mt6313_debounce_cn);

	mt_eint_registration(g_eint_pmic_mt6313_num, g_cust_eint_mt_pmic_mt6313_type,
			     mt_pmic_eint_irq_mt6313, 0);

	mt_eint_unmask(g_eint_pmic_mt6313_num);

	pr_err(PMICTAG "[CUST_EINT] CUST_EINT_MT_PMIC_MT6313_NUM=%d\n", g_eint_pmic_mt6313_num);
	pr_err(PMICTAG "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n",
		g_cust_eint_mt_pmic_mt6313_debounce_cn);
	pr_err(PMICTAG "[CUST_EINT] CUST_EINT_PMIC_TYPE=%d\n", g_cust_eint_mt_pmic_mt6313_type);
	pr_err(PMICTAG "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n",
		g_cust_eint_mt_pmic_mt6313_debounce_en);
#endif

	/*for all interrupt events, turn on interrupt module clock*/
#if 0
	/*mt6313_set_rg_intrp_ck_pdn(0);*/ /* not used in mt6313      */
#else
	mt6313_set_rg_int_pol(0);	/* high active*/
	mt6313_set_rg_int_en(1);
#endif
}

void mt6313_eint_init(void)
{
	/*---------------------*/
#if defined(CONFIG_FPGA_EARLY_PORTING)
	pr_err(PMICTAG "[MT6313_EINT] disable when CONFIG_FPGA_EARLY_PORTING\n");
#else
	/*mt6313_eint_setting();*/
	/*PMIC Interrupt Service*/
	pmic_6313_thread_handle =
	    kthread_create(pmic_thread_kthread_mt6313, (void *)NULL, "pmic_6313_thread");
	if (IS_ERR(pmic_6313_thread_handle)) {
		pmic_6313_thread_handle = NULL;
		pr_err(PMICTAG "[MT6313_EINT] creation fails\n");
	} else {
		wake_up_process(pmic_6313_thread_handle);
		pr_err(PMICTAG "[MT6313_EINT] kthread_create Done\n");
	}

	pr_err(PMICTAG "[MT6313_EINT] TBD\n");
#endif

}
#endif
/* mt6313 interrupt service */

/*
 * mt6313 probe
 */
static int mt6313_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	unsigned int ret = 0;

	pr_err(PMICTAG "[mt6313_driver_probe]\n");

	new_client = client;

	/*---------------------        */
	/* force change GPIO to SDA/SCA mode */

	ret = mt6313_hw_component_detect();
	if (ret < 0) {
		err = -ENOMEM;
		goto exit;
	}
	if (g_mt6313_hw_exist == 1) {
		mt6313_hw_init();
		/* mt6313_dump_register(); */ /* debug only */

		mt6313_eint_init();

	}
	g_mt6313_driver_ready = 1;

	pr_err(PMICTAG "[mt6313_driver_probe] g_mt6313_hw_exist=%d, g_mt6313_driver_ready=%d\n",
		g_mt6313_hw_exist, g_mt6313_driver_ready);

	if (g_mt6313_hw_exist == 0) {
#ifdef BATTERY_OC_PROTECT
		/*re-init battery oc protect point for platform without extbuck*/
		battery_oc_protect_reinit();
#endif
		pr_err(PMICTAG "[mt6313_driver_probe] return err\n");
		return err;
	}

	return 0;

exit:
	pr_err(PMICTAG "[mt6313_driver_probe] exit: return err\n");
	return err;
}

/*
 *   [platform_driver API]
 */
#ifdef mt6313_AUTO_DETECT_DISABLE
    /* TBD */
#else
/*
 * mt6313_access
 */
static ssize_t show_mt6313_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	PMICLOG("[show_mt6313_access] 0x%x\n", g_reg_value_mt6313);
	return sprintf(buf, "%u\n", g_reg_value_mt6313);
}

static ssize_t store_mt6313_access(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int ret;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_err(PMICTAG "[store_mt6313_access]\n");

	if (buf != NULL && size != 0) {
		/*PMICLOG("[store_mt6313_access] buf is %s and size is %d\n",buf,size);*/
		/*reg_address = simple_strtoul(buf, &pvalue, 16);*/

		pvalue = (char *)buf;
		if (size > 5) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16, (unsigned int *)&reg_address);
		/*ret = kstrtoul(buf, 16, (unsigned long *)&reg_address);*/

		if (size > 5) {
			/*reg_value = simple_strtoul((pvalue + 1), NULL, 16);*/
			val =  strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
			pr_err(PMICTAG "[store_mt6313_access] write mt6313 reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);

			ret = mt6313_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret = mt6313_read_interface(reg_address, &g_reg_value_mt6313, 0xFF, 0x0);

			pr_err(PMICTAG "[store_mt6313_access] read mt6313 reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_mt6313);
			pr_err(PMICTAG "[store_mt6313_access] use \"cat mt6313_access\" to get value(decimal)\n");
		}
	}
	return size;
}

static DEVICE_ATTR(mt6313_access, 0664, show_mt6313_access, store_mt6313_access);	/*664*/

/*
 * mt6313_vosel_pin
 */
int g_mt6313_vosel_pin = 0;
static ssize_t show_mt6313_vosel_pin(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_err(PMICTAG "[show_mt6313_vosel_pin] g_mt6313_vosel_pin=%d\n", g_mt6313_vosel_pin);
	return sprintf(buf, "%u\n", g_mt6313_vosel_pin);
}

static ssize_t store_mt6313_vosel_pin(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int val = 0, ret;
	char *pvalue = NULL;

	pr_err(PMICTAG "[store_mt6313_vosel_pin]\n");

	/*val = simple_strtoul(buf, &pvalue, 16);*/
	pvalue = (char *)buf;
	ret = kstrtou32(pvalue, 16, (unsigned int *)&val);

	g_mt6313_vosel_pin = val;

	pr_err(PMICTAG "[store_mt6313_vosel_pin] g_mt6313_vosel_pin(%d)\n", g_mt6313_vosel_pin);

	return size;
}

static DEVICE_ATTR(mt6313_vosel_pin, 0664, show_mt6313_vosel_pin, store_mt6313_vosel_pin);	/*664*/

/*
 * mt6313_user_space_probe
 */
static int mt6313_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	PMICLOG("******** mt6313_user_space_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_mt6313_access);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_mt6313_vosel_pin);

	return 0;
}

struct platform_device mt6313_user_space_device = {
	.name = "mt6313-user",
	.id = -1,
};

static struct platform_driver mt6313_user_space_driver = {
	.probe = mt6313_user_space_probe,
	.driver = {
		   .name = "mt6313-user",
		   },
};

/*static struct i2c_board_info i2c_mt6313 __initdata =
{ I2C_BOARD_INFO("mt6313", (mt6313_SLAVE_ADDR_WRITE >> 1)) };*/ /* auto add  by dts */

#endif

static int __init mt6313_init(void)
{
#ifdef mt6313_AUTO_DETECT_DISABLE

	PMICLOG("[mt6313_init] mt6313_AUTO_DETECT_DISABLE\n");
	g_mt6313_hw_exist = 0;
	g_mt6313_driver_ready = 1;

#else

	int ret = 0;

	wake_lock_init(&pmicThread_lock_mt6313, WAKE_LOCK_SUSPEND,
		       "pmicThread_lock_mt6313 wakelock");
	{
		PMICLOG("[mt6313_init] init start. ch=%d!!\n", mt6313_BUSNUM);

		/*i2c_register_board_info(mt6313_BUSNUM, &i2c_mt6313, 1);*/

		if (i2c_add_driver(&mt6313_driver) != 0)
			pr_err(PMICTAG "[mt6313_init] failed to register mt6313 i2c driver.\n");
		else
			pr_err(PMICTAG "[mt6313_init] Success to register mt6313 i2c driver.\n");

		/* mt6313 user space access interface*/
		ret = platform_device_register(&mt6313_user_space_device);
		if (ret) {
			pr_err(PMICTAG "****[mt6313_init] Unable to device register(%d)\n", ret);
			return ret;
		}
		ret = platform_driver_register(&mt6313_user_space_driver);
		if (ret) {
			pr_err(PMICTAG "****[mt6313_init] Unable to register driver (%d)\n", ret);
			return ret;
		}
	}
#endif

	return 0;
}

static void __exit mt6313_exit(void)
{
	i2c_del_driver(&mt6313_driver);
}
module_init(mt6313_init);
module_exit(mt6313_exit);

MODULE_AUTHOR("Argus Lin");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");

