// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define I2C_DRIVER_NAME "slave_test_i2c"
#define MAX_BUF_SIZE	128

/**
 * i2c_slave_write_test: To write data to master.
 * @client: Pointer to I2C client.
 *
 * This function will write word data to master.
 *
 * Return: 0 on success, negative number for error condition.
 */
int i2c_slave_write_test(struct i2c_client *client)
{
	u16 data = 0x1234;
	u8 command = 0;
	int ret;

	dev_err(&client->dev, "I2C-SLAVE: Running Writing Test...\n");
	ret = i2c_smbus_write_word_data(client, command, data);
	if (ret >= 0)
		dev_err(&client->dev, "I2C-SLAVE: I2c Slave Write success ret: %d\n", ret);
	else
		dev_err(&client->dev, "I2C-SLAVE: I2c Slave Write Failed ret: %d\n", ret);

	return ret;
}

/**
 * read_byte_show: To read byte data from master.
 * @dev: Pointer to i2c device.
 * @attr: device attribute.
 * @buf: data buffer.
 *
 * This function will read byte data from master.
 *
 * Return: Number of characters or values that have been
 * written to buffer.
 */
static ssize_t read_byte_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int data;

	data = i2c_smbus_read_byte(client);
	if (data < 0)
		return scnprintf(buf, MAX_BUF_SIZE,
				 "I2C-SLAVE-TEST: Read byte Failed ret:%d\n", data);

	return scnprintf(buf, MAX_BUF_SIZE, "0x%x\n", data);
}

/**
 * read_word_show: To read word data from master.
 * @dev: Pointer to i2c device.
 * @attr: device attribute.
 * @buf: data buffer.
 *
 * This function will read word data from master.
 *
 * Return: Number of characters or values that have been
 * written to buffer.
 */
static ssize_t read_word_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int data;
	u8 command = 0;

	data = i2c_smbus_read_word_data(client, command);
	if (data < 0)
		return scnprintf(buf, MAX_BUF_SIZE,
				 "I2C-SLAVE-TEST: Read word Failed ret:%d\n", data);

	return scnprintf(buf, MAX_BUF_SIZE,
			 "0x%x 0x%x\n", ((data & 0xFF00) >> 8), (data & 0xFF));
}

/**
 * read_block_show: To read block data from master.
 * @dev: Pointer to i2c device.
 * @attr: device attribute.
 * @buf: data buffer.
 *
 * This function will read block data from master.
 *
 * Return: Number of characters or values that have been
 * written to buffer.
 */
static ssize_t read_block_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 command = 0, data[I2C_SMBUS_BLOCK_MAX];
	int i, j, count = 0;

	count = i2c_smbus_read_block_data(client, command, data);
	if (count < 0)
		return scnprintf(buf, MAX_BUF_SIZE,
				 "I2C-SLAVE-TEST: Read block Failed ret:%d\n", count);

	if (count == 0)
		return scnprintf(buf, MAX_BUF_SIZE,
				 "I2C-SLAVE-TEST: Data not available\n");

	if (count > I2C_SMBUS_BLOCK_MAX)
		count = I2C_SMBUS_BLOCK_MAX;

	j = scnprintf(buf, MAX_BUF_SIZE, "Block Data: ");
	for (i = 0; i < count; i++)
		j += scnprintf(buf + j, MAX_BUF_SIZE, "0x%x ", data[i]);

	return scnprintf(buf, MAX_BUF_SIZE, "%s\n", buf);
}

/**
 * write_byte_store: To write byte data to master.
 * @dev: Pointer to i2c device.
 * @attr: device attribute.
 * @buf: data buffer.
 * @count: data count.
 *
 * This function will write byte data to master.
 *
 * Return: data count on success, negative number for error condition.
 */
static ssize_t write_byte_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val;
	int ret, command = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0) {
		dev_err(dev, "couldn't parse string %d\n", ret);
		return ret;
	}

	ret = i2c_smbus_write_byte_data(client, command, val);
	if (ret < 0)
		dev_err(&client->dev, "i2c_smbus_write_byte_data failed ret:%d\n", ret);

	return count;
}

/**
 * write_word_store: To write word data to master.
 * @dev: Pointer to i2c device.
 * @attr: device attribute.
 * @buf: data buffer.
 * @count: data count.
 *
 * This function will write word data to master.
 *
 * Return: data count on success, negative number for error condition.
 */
static ssize_t write_word_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret = 0, command = 0;
	unsigned long val;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0) {
		dev_err(dev, "couldn't parse string %d\n", ret);
		return ret;
	}

	ret = i2c_smbus_write_word_data(client, command, val);
	if (ret < 0)
		dev_err(&client->dev, "i2c_smbus_write_word_data failed ret:%d\n", ret);

	return count;
}

/**
 * write_block_store: To write block data to master.
 * @dev: Pointer to i2c device.
 * @attr: device attribute.
 * @buf: data buffer.
 * @count: data count.
 *
 * This function will write block data to master.
 *
 * Return: 0 for success, negative number for error condition.
 */
static ssize_t write_block_store(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 data[I2C_SMBUS_BLOCK_MAX];
	int ret = 0, command = 0;

	ret = kstrtoul(buf, 10, &count);
	if (ret < 0) {
		dev_err(dev, "couldn't parse string %d\n", ret);
		return ret;
	}

	if (count > I2C_SMBUS_BLOCK_MAX)
		count = I2C_SMBUS_BLOCK_MAX;

	get_random_bytes(data, count);
	ret = i2c_smbus_write_block_data(client, command, count, data);
	if (ret < 0)
		dev_err(&client->dev, "i2c_smbus_write_block_data failed ret:%d\n", ret);

	return count;
}

static DEVICE_ATTR_RO(read_byte);
static DEVICE_ATTR_RO(read_word);
static DEVICE_ATTR_RO(read_block);
static DEVICE_ATTR_WO(write_byte);
static DEVICE_ATTR_WO(write_word);
static DEVICE_ATTR_WO(write_block);

/**
 * i2c_slave_create_sysfs: To create sysfe attribute.
 * @client: Pointer to I2C client.
 *
 * This function will create sysfs file to pass
 * data from user space .
 *
 * Return: 0 on success, Negative on error.
 */
int i2c_slave_create_sysfs(struct i2c_client *client)
{
	int ret = 0;

	ret = device_create_file(&client->dev, &dev_attr_read_byte);
	if (ret) {
		dev_err(&client->dev, "I2C-SLAVE-TEST: failed to create sysfs file ret: %d\n", ret);
		goto err;
	}

	ret = device_create_file(&client->dev, &dev_attr_write_byte);
	if (ret) {
		dev_err(&client->dev, "I2C-SLAVE-TEST: failed to create sysfs file ret: %d\n", ret);
		goto err_write_byte;
	}

	ret = device_create_file(&client->dev, &dev_attr_read_word);
	if (ret) {
		dev_err(&client->dev, "I2C-SLAVE-TEST: failed to create sysfs file ret: %d\n", ret);
		goto err_read_word;
	}

	ret = device_create_file(&client->dev, &dev_attr_write_word);
	if (ret) {
		dev_err(&client->dev, "I2C-SLAVE-TEST: failed to create sysfs file ret: %d\n", ret);
		goto err_write_word;
	}

	ret = device_create_file(&client->dev, &dev_attr_read_block);
	if (ret) {
		dev_err(&client->dev, "I2C-SLAVE-TEST: failed to create sysfs file ret: %d\n", ret);
		goto err_read_block;
	}

	ret = device_create_file(&client->dev, &dev_attr_write_block);
	if (ret) {
		dev_err(&client->dev, "I2C-SLAVE-TEST: failed to create sysfs file ret: %d\n", ret);
		goto err_write_block;
	}

	return 0;

err_write_block:
	device_remove_file(&client->dev, &dev_attr_read_block);
err_read_block:
	device_remove_file(&client->dev, &dev_attr_write_word);
err_write_word:
	device_remove_file(&client->dev, &dev_attr_read_word);
err_read_word:
	device_remove_file(&client->dev, &dev_attr_write_byte);
err_write_byte:
	device_remove_file(&client->dev, &dev_attr_read_byte);
err:
	return ret;
}

/**
 * i2c_slave_test_probe: Driver Probe function.
 * @client: Pointer to I2C client.
 * @dev_id: I2C device id.
 *
 * This function will performs pre-initialization tasks such as verify,
 * I2C functionality, create sysfs attributes and perform read/write operation.
 *
 * Return: 0 for success, negative number for error condition.
 */
static int i2c_slave_test_probe(struct i2c_client *client,
				const struct i2c_device_id *dev_id)
{
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&client->dev, "%s: Need I2C_FUNC_SMBUS_BYTE\n", __func__);
		return -EINVAL;
	}

	ret = i2c_slave_write_test(client);
	if (ret < 0) {
		dev_err(&client->dev, "I2C slave read test failed ret: $d\n", ret);
		goto err;
	}

	ret = i2c_slave_create_sysfs(client);
	if (ret) {
		dev_err(&client->dev, "I2C-SLAVE-TEST: failed to create sysfs file ret: %d\n", ret);
		goto err;
	}

	dev_err(&client->dev, "I2C-SLAVE-TEST: Probe done\n");
	return 0;

err:
	dev_err(&client->dev, "I2C-SLAVE-TEST: Probe failed ret: %d\n", ret);
	return ret;
}

/**
 * i2c_slave_test_remove: Driver remove function.
 * @client: Pointer to I2C client.
 *
 * This function will remove sysfs attributes.
 *
 * Return: 0 for success.
 */

static int i2c_slave_test_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_write_block);
	device_remove_file(&client->dev, &dev_attr_read_block);
	device_remove_file(&client->dev, &dev_attr_write_word);
	device_remove_file(&client->dev, &dev_attr_read_word);
	device_remove_file(&client->dev, &dev_attr_write_byte);
	device_remove_file(&client->dev, &dev_attr_read_byte);
	return 0;
}

static const struct i2c_device_id slave_test_id_table[] = {
	{I2C_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, slave_test_id_table);

static const struct of_device_id slave_test_of_match_table[] = {
	{
		.compatible = "qcom,i2c-slave-test",
	},
	{},
};
MODULE_DEVICE_TABLE(of, slave_test_of_match_table);

static struct i2c_driver i2c_slave_test_driver = {
		.driver = {
		.name = I2C_DRIVER_NAME,
		.of_match_table = slave_test_of_match_table,
	},
	.probe = i2c_slave_test_probe,
	.remove = i2c_slave_test_remove,
	.id_table = slave_test_id_table,
};

static int __init i2c_slave_test_init(void)
{
	return i2c_add_driver(&i2c_slave_test_driver);
}

static void __exit i2c_slave_test_exit(void)
{
	i2c_del_driver(&i2c_slave_test_driver);
}
module_init(i2c_slave_test_init);
module_exit(i2c_slave_test_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("I2C Slave Test driver");
