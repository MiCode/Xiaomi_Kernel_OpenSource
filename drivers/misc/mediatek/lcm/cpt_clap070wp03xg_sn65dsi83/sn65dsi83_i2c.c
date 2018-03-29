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

#ifndef BUILD_LK
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/of.h>
#include <linux/module.h>

#include "sn65dsi83_i2c.h"

/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/
#define sn65dsi83_SLAVE_ADDR_WRITE	0x58

static struct i2c_client *new_client;
static const struct i2c_device_id sn65dsi83_i2c_id[] = { {"sn65dsi83", 0}, {} };

static int sn65dsi83_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int sn65dsi83_driver_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static const struct of_device_id sn65dsi83_id[] = {
	{.compatible = "sn65dsi83"},
	{},
};

MODULE_DEVICE_TABLE(of, sn65dsi83_id);
#endif

static struct i2c_driver sn65dsi83_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sn65dsi83",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(sn65dsi83_id),
#endif
		   },
	.probe = sn65dsi83_driver_probe,
	.remove = sn65dsi83_driver_remove,
	.id_table = sn65dsi83_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
static DEFINE_MUTEX(sn65dsi83_i2c_access);
/**********************************************************
  *
  *   [I2C Function For Read/Write fan5405]
  *
  *********************************************************/
int sn65dsi83_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&sn65dsi83_i2c_access);
	/*
	   new_client->ext_flag =
	   ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;
	 */
	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], 1);
	ret = i2c_master_recv(new_client, &cmd_buf[1], 1);
	if (ret < 0) {
		/*new_client->ext_flag = 0; */

		mutex_unlock(&sn65dsi83_i2c_access);

		return 0;
	}

	readData = cmd_buf[1];
	*returnData = readData;

	/*new_client->ext_flag = 0; */

	mutex_unlock(&sn65dsi83_i2c_access);

	return 1;
}

int sn65dsi83_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&sn65dsi83_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	/*new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_DIRECTION_FLAG; */

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {

		/*new_client->ext_flag = 0; */
		mutex_unlock(&sn65dsi83_i2c_access);

		return 0;
	}

	/*new_client->ext_flag = 0; */
	mutex_unlock(&sn65dsi83_i2c_access);

	return 1;
}

static int sn65dsi83_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	pr_debug("[sn65dsi83_driver_probe]name=%s addr=0x%x\n", client->name, client->addr);
	new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!new_client) {
		err = -ENOMEM;
		goto exit;
	}

	memset(new_client, 0, sizeof(struct i2c_client));
	new_client = client;

	return 0;

 exit:
	return err;

}

static int sn65dsi83_driver_remove(struct i2c_client *client)
{
	pr_debug("sn65dsi83_driver_remove\n");

	new_client = NULL;
	i2c_unregister_device(client);

	return 0;
}

#define SN65DSI83_BUSNUM 4

/*
static struct i2c_board_info i2c_sn65dsi83 __initdata = {
	I2C_BOARD_INFO("sn65dsi83", (sn65dsi83_SLAVE_ADDR_WRITE >> 1))
};
*/

static int __init sn65dsi83_init(void)
{
	pr_notice("[sn65dsi83_init] init start\n");

	/* i2c_register_board_info(SN65DSI83_BUSNUM, &i2c_sn65dsi83, 1); */

	if (i2c_add_driver(&sn65dsi83_driver) != 0)
		pr_notice("[sn65dsi83_init] failed to register sn65dsi83 i2c driver.\n");
	else
		pr_notice("[sn65dsi83_init] Success to register sn65dsi83 i2c driver.\n");

	return 0;
}

static void __exit sn65dsi83_exit(void)
{
	i2c_del_driver(&sn65dsi83_driver);
}

late_initcall(sn65dsi83_init);
module_exit(sn65dsi83_exit);

#endif
