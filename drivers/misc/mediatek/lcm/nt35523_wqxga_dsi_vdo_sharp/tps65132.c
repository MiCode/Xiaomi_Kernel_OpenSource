/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/device.h>
#include <linux/debugfs.h>


#define I2C_ID_NAME "tps65132"

/*****************************************************************************
* GLobal Variable
*****************************************************************************/
static struct i2c_client *tps65132_i2c_client;

/*****************************************************************************
* Function Prototype
*****************************************************************************/
static int tps65132_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int tps65132_remove(struct i2c_client *client);
int tps65132_write_bytes(unsigned char addr, unsigned char value);

/*****************************************************************************
* Data Structure
*****************************************************************************/
struct tps65132_dev {
	struct i2c_client *client;
};

static const struct i2c_device_id tps65132_id[] = {
	{I2C_ID_NAME, 0},
	{}
};

static const struct of_device_id tps65132_of_match[] = {
	{.compatible = "mediatek,tps65132"},
	{},
};

static struct i2c_driver tps65132_i2c_driver = {
	.id_table = tps65132_id,
	.probe = tps65132_probe,
	.remove = tps65132_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "tps65132",
		.of_match_table = tps65132_of_match,
	},
};

static int dbg_reg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t dbg_reg_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int ret, cmd;

	sscanf(buf, "%d", &cmd);

	if (cmd == 1) {
		ret = tps65132_write_bytes(0x00, 0x0e);

		if (ret < 0)
			printk("%s ----tps6132--- i2c write error-----\n", __func__);
		else
			printk("%s ----tps6132--- i2c write success-----\n", __func__);
	} else {
		printk("%s: unknown command\n", __func__);
	}

	return count;
}

static const struct file_operations dbg_reg_fops = {
	.open = dbg_reg_open,
	.write = dbg_reg_write,
};

static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct dentry *dent = debugfs_create_dir("tps65132", NULL);

	pr_warn("tps65132_i2c_probe\n");
	pr_warn("TPS: info==>name=%s addr=0x%x\n", client->name, client->addr);
	tps65132_i2c_client = client;

	/* add debugfs nodes */
	debugfs_create_file("write_reg", S_IRUGO | S_IWUSR, dent, NULL, &dbg_reg_fops);

	return 0;
}

static int tps65132_remove(struct i2c_client *client)
{
	pr_warn("tps65132_remove\n");
	tps65132_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

int tps65132_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = tps65132_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_err("%s: no i2 client !!\n", __func__);
		return -EPERM;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_err("tps65132 write data fail !!\n");

	return ret;
}
EXPORT_SYMBOL(tps65132_write_bytes);

static int __init tps65132_i2c_init(void)
{
	pr_warn("tps65132_i2c_init\n");
	i2c_add_driver(&tps65132_i2c_driver);
	pr_warn("tps65132_i2c_init success\n");
	return 0;
}

static void __exit tps65132_i2c_exit(void)
{
	pr_warn("tps65132_i2c_exit\n");
	i2c_del_driver(&tps65132_i2c_driver);
}

module_init(tps65132_i2c_init);
module_exit(tps65132_i2c_exit);

MODULE_AUTHOR("Xiaokuan Shi");
MODULE_DESCRIPTION("MTK TPS65132 I2C Driver");
MODULE_LICENSE("GPL");

