/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#if !defined(CONFIG_ARCH_MT8167)
#include <linux/irq.h>
#endif
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/wait.h>
#endif

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/upmu_hw.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#else
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_pm_ldo.h>	/* hwPowerOn */
#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#endif
#endif
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_i2c.h>
#endif

#include "lcm_define.h"
#include "lcm_drv.h"
#include "lcm_i2c.h"


/*****************************************************************************
 * Define
 *****************************************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_MTK_LEGACY
#define LCM_I2C_ADDR 0x3E
#define LCM_I2C_BUSNUM  I2C_I2C_LCD_BIAS_CHANNEL	/* for I2C channel 0 */
#define LCM_I2C_ID_NAME "tps65132"
#else
#define LCM_I2C_ADDR 0x3E
#define LCM_I2C_BUSNUM  1	/* for I2C channel 0 */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
#endif


/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
#ifdef CONFIG_MTK_LEGACY
static struct i2c_board_info _lcm_i2c_board_info __initdata = {
	I2C_BOARD_INFO(LCM_I2C_ID_NAME, LCM_I2C_ADDR)
};
#else
static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	 .compatible = "mediatek,I2C_LCD_BIAS",
	 },
};
#endif

static struct i2c_client *_lcm_i2c_client;


/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);


/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;

};


static const struct i2c_device_id _lcm_i2c_id[] = {
	{LCM_I2C_ID_NAME, 0},
	{}
};


/* #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)) */
/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */
/* #endif */
static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect               = _lcm_i2c_detect, */
	.driver = {
		   .owner = THIS_MODULE,
		   .name = LCM_I2C_ID_NAME,
#ifdef CONFIG_MTK_LEGACY
#else
		   .of_match_table = _lcm_i2c_of_match,
#endif
		   },

};


/*****************************************************************************
 * Extern Area
 *****************************************************************************/



/*****************************************************************************
 * Function
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	pr_debug("[LCM][I2C] _lcm_i2c_probe\n");
	pr_debug("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name, client->addr);
	_lcm_i2c_client = client;
	return 0;
}


static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] _lcm_i2c_remove\n");
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}


static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_err("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}


/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_debug("[LCM][I2C] _lcm_i2c_init\n");
#ifdef CONFIG_MTK_LEGACY
	i2c_register_board_info(LCM_I2C_BUSNUM, &_lcm_i2c_board_info, 1);
	pr_debug("[LCM][I2C] _lcm_i2c_init2\n");
#endif
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] _lcm_i2c_init success\n");

	return 0;
}


static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] _lcm_i2c_exit\n");
	i2c_del_driver(&_lcm_i2c_driver);
}


static LCM_STATUS _lcm_i2c_check_data(char type, const LCM_DATA_T2 *t2)
{
	switch (type) {
	case LCM_I2C_WRITE:
		if (t2->cmd > 0xFF) {
			pr_err("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__, t2->cmd);
			return LCM_STATUS_ERROR;
		}
		if (t2->data > 0xFF) {
			pr_err("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__, t2->data);
			return LCM_STATUS_ERROR;
		}
		break;

	default:
		pr_err("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__, type);
		return LCM_STATUS_ERROR;
	}

	return LCM_STATUS_OK;
}
#endif


LCM_STATUS lcm_i2c_set_data(char type, const LCM_DATA_T2 *t2)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	unsigned int ret_code = 0;

	/* check parameter is valid */
	if (LCM_STATUS_OK == _lcm_i2c_check_data(type, t2)) {
		switch (type) {
		case LCM_I2C_WRITE:
			pr_debug("[LCM][I2C] %s/%d: %d, 0x%x, 0x%x\n", __func__, __LINE__, type,
				 t2->cmd, t2->data);
			ret_code =
			    _lcm_i2c_write_bytes((unsigned char)t2->cmd, (unsigned char)t2->data);
			break;
		default:
			pr_err("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__, type);
			return LCM_STATUS_ERROR;
		}
	} else {
		pr_err("[LCM][ERROR] %s/%d: %d, 0x%x, 0x%x\n", __func__, __LINE__, type, t2->cmd,
		       t2->data);
		return LCM_STATUS_ERROR;
	}

	if (ret_code < 0) {
		pr_err("[LCM][ERROR] %s/%d: 0x%x, 0x%x, %d\n", __func__, __LINE__,
		       (unsigned int)t2->cmd, (unsigned int)t2->data, ret_code);
		return LCM_STATUS_ERROR;
	}
#endif
	return LCM_STATUS_OK;
}



#ifndef CONFIG_FPGA_EARLY_PORTING
module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);

MODULE_AUTHOR("Joey Pan");
MODULE_DESCRIPTION("MTK LCM I2C Driver");
MODULE_LICENSE("GPL");
#endif
#endif
