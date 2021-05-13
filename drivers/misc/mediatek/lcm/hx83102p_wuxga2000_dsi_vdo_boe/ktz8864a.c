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

#include "ktz8864a.h"

/* I2C Slave Setting */
#define ktz8864a_SLAVE_ADDR_WRITE	0x11

static struct i2c_client *new_client;
static const struct i2c_device_id ktz8864a_i2c_id[] = { {"ktz8864a", 0}, {} };

static int ktz8864a_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ktz8864a_driver_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static const struct of_device_id ktz8864a_id[] = {
	{.compatible = "ktz8864a"},
	{},
};

MODULE_DEVICE_TABLE(of, ktz8864a_id);
#endif

static struct i2c_driver ktz8864a_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ktz8864a",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ktz8864a_id),
#endif
	},
	.probe = ktz8864a_driver_probe,
	.remove = ktz8864a_driver_remove,
	.id_table = ktz8864a_i2c_id,
};

static DEFINE_MUTEX(ktz8864a_i2c_access);

/* I2C Function For Read/Write */
int ktz8864a_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&ktz8864a_i2c_access);

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], 1);
	ret = i2c_master_recv(new_client, &cmd_buf[1], 1);
	if (ret < 0) {
		mutex_unlock(&ktz8864a_i2c_access);
		return 0;
	}

	readData = cmd_buf[1];
	*returnData = readData;

	mutex_unlock(&ktz8864a_i2c_access);

	return 1;
}

int ktz8864a_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	pr_notice("[KE/ktz8864a] cmd: %02x, data: %02x,%s\n", cmd, writeData, __func__);

	mutex_lock(&ktz8864a_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		mutex_unlock(&ktz8864a_i2c_access);
		pr_notice("[ktz8864a] I2C write fail!!!\n");

		return 0;
	}

	mutex_unlock(&ktz8864a_i2c_access);

	return 1;
}

static int ktz8864a_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;

	pr_notice("[KE/ktz8864a] name=%s addr=0x%x\n",
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

static int ktz8864a_driver_remove(struct i2c_client *client)
{
	pr_notice("[KE/ktz8864a] %s\n", __func__);

	new_client = NULL;
	i2c_unregister_device(client);

	return 0;
}

#define ktz8864a_BUSNUM 5

static int __init ktz8864a_init(void)
{
	pr_notice("[KE/ktz8864a] %s\n", __func__);

	if (i2c_add_driver(&ktz8864a_driver) != 0)
		pr_notice("[KE/ktz8864a] failed to register ktz8864a i2c driver.\n");
	else
		pr_notice("[KE/ktz8864a] Success to register ktz8864a i2c driver.\n");

	return 0;
}

static void __exit ktz8864a_exit(void)
{
	i2c_del_driver(&ktz8864a_driver);
}

module_init(ktz8864a_init);
module_exit(ktz8864a_exit);

#endif
