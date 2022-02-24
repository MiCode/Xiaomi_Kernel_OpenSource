// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#include "ocp2138.h"

/* I2C Slave Setting */
#define ocp2138_SLAVE_ADDR_WRITE	0x3E

static struct i2c_client *new_client;
static const struct i2c_device_id ocp2138_i2c_id[] = { {"ocp2138", 0}, {} };

static int ocp2138_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ocp2138_driver_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static const struct of_device_id ocp2138_id[] = {
	{.compatible = "ocp2138"},
	{},
};

MODULE_DEVICE_TABLE(of, ocp2138_id);
#endif

static struct i2c_driver ocp2138_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ocp2138",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ocp2138_id),
#endif
	},
	.probe = ocp2138_driver_probe,
	.remove = ocp2138_driver_remove,
	.id_table = ocp2138_i2c_id,
};

static DEFINE_MUTEX(ocp2138_i2c_access);

/* I2C Function For Read/Write */
int ocp2138_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&ocp2138_i2c_access);

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], 1);
	ret = i2c_master_recv(new_client, &cmd_buf[1], 1);
	if (ret < 0) {
		mutex_unlock(&ocp2138_i2c_access);
		return 0;
	}

	readData = cmd_buf[1];
	*returnData = readData;

	mutex_unlock(&ocp2138_i2c_access);

	return 1;
}

int ocp2138_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	pr_notice("[KE/ocp2138] cmd: %02x, data: %02x,%s\n", cmd, writeData, __func__);

	mutex_lock(&ocp2138_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		mutex_unlock(&ocp2138_i2c_access);
		pr_notice("[ocp2138] I2C write fail!!!\n");

		return 0;
	}

	mutex_unlock(&ocp2138_i2c_access);

	return 1;
}

static int ocp2138_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;

	pr_notice("[KE/ocp2138] name=%s addr=0x%x\n",
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

static int ocp2138_driver_remove(struct i2c_client *client)
{
	pr_notice("[KE/ocp2138] %s\n", __func__);

	new_client = NULL;
	i2c_unregister_device(client);

	return 0;
}

#define ocp2138_BUSNUM 5

static int __init ocp2138_init(void)
{
	pr_notice("[KE/ocp2138] %s\n", __func__);

	if (i2c_add_driver(&ocp2138_driver) != 0)
		pr_notice("[KE/ocp2138] failed to register ocp2138 i2c driver.\n");
	else
		pr_notice("[KE/ocp2138] Success to register ocp2138 i2c driver.\n");

	return 0;
}

static void __exit ocp2138_exit(void)
{
	i2c_del_driver(&ocp2138_driver);
}

module_init(ocp2138_init);
module_exit(ocp2138_exit);

#endif
