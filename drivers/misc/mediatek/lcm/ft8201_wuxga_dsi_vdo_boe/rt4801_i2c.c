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
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/module.h>

#include "rt4801_i2c.h"

/* I2C Slave Setting */
#define rt4801_SLAVE_ADDR_WRITE	0x73

static struct i2c_client *new_client;
static const struct i2c_device_id rt4801_i2c_id[] = { {"rt4801", 0}, {} };

static int rt4801_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int rt4801_driver_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static const struct of_device_id rt4801_id[] = {
	{.compatible = "rt4801"},
	{},
};

MODULE_DEVICE_TABLE(of, rt4801_id);
#endif

static struct i2c_driver rt4801_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "rt4801",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(rt4801_id),
#endif
	},
	.probe = rt4801_driver_probe,
	.remove = rt4801_driver_remove,
	.id_table = rt4801_i2c_id,
};

static DEFINE_MUTEX(rt4801_i2c_access);

/* I2C Function For Read/Write */
int rt4801_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&rt4801_i2c_access);

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], 1);
	ret = i2c_master_recv(new_client, &cmd_buf[1], 1);
	if (ret < 0) {
		mutex_unlock(&rt4801_i2c_access);
		return 0;
	}

	readData = cmd_buf[1];
	*returnData = readData;

	mutex_unlock(&rt4801_i2c_access);

	return 1;
}

int rt4801_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	pr_notice("[KE/RT4801] %s\n", __func__);

	mutex_lock(&rt4801_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		mutex_unlock(&rt4801_i2c_access);
		pr_notice("[rt4801] I2C write fail!!!\n");

		return 0;
	}

	mutex_unlock(&rt4801_i2c_access);

	return 1;
}

static int rt4801_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;

	pr_notice("[KE/RT4801] name=%s addr=0x%x\n",
		client->name, client->addr);
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

static int rt4801_driver_remove(struct i2c_client *client)
{
	pr_notice("[KE/RT4801] %s\n", __func__);

	new_client = NULL;
	i2c_unregister_device(client);

	return 0;
}

#define RT4801_BUSNUM 1

static int __init rt4801_init(void)
{
	pr_notice("[KE/RT4801] %s\n", __func__);

	if (i2c_add_driver(&rt4801_driver) != 0)
		pr_notice("[KE/RT4801] failed to register rt4801 i2c driver.\n");
	else
		pr_notice("[KE/RT4801] Success to register rt4801 i2c driver.\n");

	return 0;
}

static void __exit rt4801_exit(void)
{
	i2c_del_driver(&rt4801_driver);
}

module_init(rt4801_init);
module_exit(rt4801_exit);

#endif
