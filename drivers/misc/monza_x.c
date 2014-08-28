/*
 * monza_x.c: driver for Impinj RFID chip
 *
 * (c) copyright 2013,2014 intel corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/log2.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/acpi.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define MONZAX_2K_BYTE_LEN 336
#define MONZAX_8K_BYTE_LEN 1088
#define MONZAX_KBUF_MAX 1088

#define MONZAX_2K_CLASSID_OFF 328
#define MONZAX_8K_CLASSID_OFF 40
#define MONZAX_GEN2_CLASSID 0xE2

enum slave_addr_num {
	MONZAX_8K_ADDR_NUM = 1,
	MONZAX_2K_ADDR_NUM
};

/*
 * word/2word write will take time before next write,
 * set 100ms threshold for safe.
 */
#define  WRITE_TIMEOUT 100

struct monza_data {
	struct mutex lock;
	struct bin_attribute bin;

	u8 *writebuf;
	unsigned write_max;
	unsigned num_addr;

	struct miscdevice miscdev;
	/* monzax_2k has 2 i2c slave addr */
	struct i2c_client *client[2];
};

static struct i2c_client *monza_translate_offset(struct monza_data *monza,
		unsigned *offset)
{
	unsigned i = 0;

	if (monza->num_addr == MONZAX_2K_ADDR_NUM) {
		i = *offset >> 8;
		*offset &= 0xff;
	}

	return monza->client[i];
}

static ssize_t monza_eeprom_read(struct monza_data *monza, char *buf,
		unsigned offset, size_t count)
{
	struct i2c_client *client;
	struct i2c_msg msg[2];
	u8 msgbuf[2];
	int status, i = 0;

	memset(msg, 0, sizeof(msg));
	client = monza_translate_offset(monza, &offset);

	/* for monzax 8k, eeprom offset is 16bit/2byt mode */
	if (monza->num_addr == MONZAX_8K_ADDR_NUM)
		msgbuf[i++] = offset >> 8;
	msgbuf[i++] = offset;

	msg[0].addr = client->addr;
	msg[0].buf = msgbuf;
	msg[0].len = i;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = count;

	status = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (status == ARRAY_SIZE(msg))
		status = count;
	dev_dbg(&client->dev, "read %lu@%d --> %d\n",
			count, offset, status);
	return status;
}

static ssize_t monza_read(struct monza_data *monza,
		char *buf, loff_t off, size_t count)
{
	ssize_t retval = 0;
	unsigned long timeout, read_time;
	ssize_t	status;

	/*
	 * Read data from chip, protecting against concurrent updates
	 * from this host, but not from other I2C masters.
	 */
	mutex_lock(&monza->lock);

	while (count) {
		/*
		 * Reads fail if the previous write didn't complete yet. We may
		 * loop a few times until this one succeeds
		 * Twrite is typicaly 4ms, try on each Twrite/2
		 * Timeout is experienced value got from integration testing
		 */
		timeout = jiffies + msecs_to_jiffies(WRITE_TIMEOUT);
		do {
			read_time = jiffies;
			status = monza_eeprom_read(monza, buf, off, count);
			if (status == count)
				break;
			usleep_range(2000, 2050);
		} while (time_before(read_time, timeout));

		/* exception handle */
		if (status < 0) {
			if (retval == 0)
				retval = status;
			break;
		}

		buf += status;
		off += status;
		count -= status;
		retval += status;
	}

	mutex_unlock(&monza->lock);

	return retval;
}

static ssize_t monza_bin_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr,
		char *buf, loff_t off, size_t count)
{
	struct monza_data *monza;

	monza = dev_get_drvdata(container_of(kobj, struct device, kobj));
	return monza_read(monza, buf, off, count);
}

static ssize_t monza_eeprom_write(struct monza_data *monza, const char *buf,
		unsigned offset, size_t count)
{
	struct i2c_client *client;
	struct i2c_msg msg;
	int status, i = 0;

	/* Get corresponding I2C address and adjust offset */
	client = monza_translate_offset(monza, &offset);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = monza->writebuf;
	/* for monzax 8k, eeprom offset is 16bit/2byt mode */
	if (monza->num_addr == MONZAX_8K_ADDR_NUM)
		msg.buf[i++] = offset >> 8;
	msg.buf[i++] = offset;
	memcpy(&msg.buf[i], buf, count);
	msg.len = i + count;

	status = i2c_transfer(client->adapter, &msg, 1);
	dev_dbg(&client->dev, "write %lu@%d --> %d\n",
			count, offset, status);
	if (status == 1)
		status = count;
	return status;
}

static ssize_t monza_write(struct monza_data *monza, const char *buf,
				loff_t off, size_t count)
{
	ssize_t retval = 0;
	unsigned long timeout, write_time;

	if ((off % 2 != 0) || (count % 2 != 0)) {
		dev_err(&monza->client[0]->dev, "word boundary error\n");
		return 0;
	}

	/*
	 * Write data to chip, protecting against concurrent updates
	 * from this host, but not from other I2C masters.
	 */
	mutex_lock(&monza->lock);

	while (count) {
		ssize_t	status;
		size_t cnt;
		/* write_max is at most a 2word/4byte */
		if (count > monza->write_max)
			cnt = monza->write_max;
		else
			cnt = count;
		/*
		 * Writes fail if the previous one didn't complete yet. We may
		 * loop a few times until this one succeeds.
		 */
		timeout = jiffies + msecs_to_jiffies(WRITE_TIMEOUT);
		do {
			write_time = jiffies;
			status = monza_eeprom_write(monza, buf, off, cnt);
			if (status == cnt)
				break;
			usleep_range(2000, 2050);
		} while (time_before(write_time, timeout));

		/* exception handle */
		if (status < 0) {
			if (retval == 0)
				retval = status;
			break;
		}

		buf += status;
		off += status;
		count -= status;
		retval += status;
	}

	mutex_unlock(&monza->lock);

	return retval;
}

static ssize_t monza_bin_write(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr,
		char *buf, loff_t off, size_t count)
{
	struct monza_data *monza;

	monza = dev_get_drvdata(container_of(kobj, struct device, kobj));
	return monza_write(monza, buf, off, count);
}

static int monza_check_ids(struct monza_data *monza)
{
	int status, off = MONZAX_2K_CLASSID_OFF;
	unsigned char buf[2] = { 0 };

	if (monza->num_addr == MONZAX_2K_ADDR_NUM)
		off = MONZAX_2K_CLASSID_OFF;
	else if (monza->num_addr == MONZAX_8K_ADDR_NUM)
		off = MONZAX_8K_CLASSID_OFF;

	status = monza_read(monza, buf, off, 1);
	if (status > 0 && buf[0] == MONZAX_GEN2_CLASSID)
		return 0;
	else
		return -ENODEV;
}

static int monza_misc_open(struct inode *inode, struct file *filp)
{
	struct monza_data *monza = container_of(filp->private_data,
					      struct monza_data, miscdev);
	filp->private_data = monza;
	return 0;
}

static int monza_misc_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t monza_misc_read(struct file *filp, char __user *ubuf,
				size_t count, loff_t *pos)
{
	struct monza_data *monza = filp->private_data;
	u8 *kbuf;
	ssize_t cnt;

	kbuf = kmalloc(MONZAX_KBUF_MAX, GFP_KERNEL);
	if (kbuf == NULL)  {
		dev_err(&monza->client[0]->dev, "%s(%d): buf allocation failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	count = min_t(size_t, MONZAX_KBUF_MAX - *pos, count);
	cnt = monza_read(monza, kbuf, *pos, count);
	if (cnt <= 0)
		goto out;

	if (copy_to_user(ubuf, kbuf, cnt)) {
		cnt = -EFAULT;
		goto out;
	}
	*pos += cnt;
out:
	kfree(kbuf);
	return cnt;
}

static ssize_t monza_misc_write(struct file *filp, const char __user *ubuf,
			 size_t count, loff_t *pos)
{
	struct monza_data *monza = filp->private_data;
	u8 *kbuf;
	ssize_t cnt;

	kbuf = kmalloc(MONZAX_KBUF_MAX, GFP_KERNEL);
	if (kbuf == NULL)  {
		dev_err(&monza->client[0]->dev, "%s(%d): buf allocation failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	count = min_t(size_t, MONZAX_KBUF_MAX - *pos, count);
	if (copy_from_user(kbuf, ubuf, count)) {
		cnt = -EFAULT;
		goto out;
	}

	cnt = monza_write(monza, kbuf, *pos, count);
	if (cnt <= 0)
		goto out;

	*pos += count;
out:
	kfree(kbuf);
	return cnt;
}

static const struct file_operations monza_misc_fops = {
	.owner   = THIS_MODULE,
	.read    = monza_misc_read,
	.write	 = monza_misc_write,
	.llseek	 = generic_file_llseek,
	.open    = monza_misc_open,
	.release = monza_misc_release,
};

static const struct i2c_device_id i2c_monza_ids[] = {
	{ "MNZX2000", MONZAX_2K_ADDR_NUM },
	{ "MNZX8000", MONZAX_8K_ADDR_NUM },
	{ "IMPJ0003", MONZAX_8K_ADDR_NUM },
	{ /* END OF LIST */ }
};
MODULE_DEVICE_TABLE(i2c, i2c_monza_ids);

static const struct acpi_device_id acpi_monza_ids[] = {
	{ "MNZX2000", MONZAX_2K_ADDR_NUM },
	{ "MNZX8000", MONZAX_8K_ADDR_NUM },
	{ "IMPJ0003", MONZAX_8K_ADDR_NUM },
	{}
};
MODULE_DEVICE_TABLE(acpi, acpi_monza_ids);

static int monza_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct monza_data *monza;
	const struct acpi_device_id *aid;
	int err;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		return -ENODEV;
	}

	monza = kzalloc(sizeof(struct monza_data), GFP_KERNEL);
	if (!monza) {
		dev_err(&client->dev, "Fail to alloc monza\n");
		return -ENODEV;
	}

	mutex_init(&monza->lock);

	if (id)
		monza->num_addr = id->driver_data;
	else {
		/* acpi id detect */
		for (aid = acpi_monza_ids; aid->id[0]; aid++)
			if (!strncmp(aid->id, client->name, strlen(aid->id))) {
				monza->num_addr = aid->driver_data;
				dev_info(&client->dev, "acpi id: %s\n",
					client->name);
			}
	}
	if (!monza->num_addr) {
		dev_err(&client->dev, "Invalid id driver data error.\n");
		err = -ENODEV;
		goto err_struct;
	}

	monza->client[0] = client;
	/* use dummy device, since monzax-2k has 2 slave address */
	if (monza->num_addr == MONZAX_2K_ADDR_NUM) {
		monza->client[1] = i2c_new_dummy(client->adapter,
					client->addr + 1);
		if (!monza->client[1]) {
			dev_err(&client->dev, "address 0x%02x unavailable\n",
					client->addr + 1);
			err = -EADDRINUSE;
			goto err_struct;
		}
	}

	/* identify the real chip and address */
	err = monza_check_ids(monza);
	if (err) {
		dev_err(&client->dev, " detect chip failure.\n");
		goto err_clients;
	}

	/* Force one word or two word writing
	 * So, 2B memory address + 4B data
	 */
	monza->write_max = 4;
	monza->writebuf = kmalloc(monza->write_max + 2, GFP_KERNEL);
	if (!monza->writebuf) {
		err = -ENOMEM;
		goto err_clients;
	}

	/*
	 * Export the EEPROM bytes through sysfs, since that's convenient.
	 * By default, only root should see the data (maybe passwords etc)
	 */
	sysfs_bin_attr_init(&monza->bin);
	monza->bin.attr.name = "monzax_data";
	monza->bin.attr.mode = S_IRUSR | S_IWUSR;
	monza->bin.read = monza_bin_read;
	monza->bin.write = monza_bin_write;
	if (monza->num_addr == MONZAX_2K_ADDR_NUM)
		monza->bin.size = MONZAX_2K_BYTE_LEN;
	else if (monza->num_addr == MONZAX_8K_ADDR_NUM)
		monza->bin.size = MONZAX_8K_BYTE_LEN;
	else {
		err = -ENODEV;
		goto err_bin;
	}

	err = sysfs_create_bin_file(&client->dev.kobj, &monza->bin);
	if (err)
		goto err_bin;

	i2c_set_clientdata(client, monza);

	monza->miscdev.minor	= MISC_DYNAMIC_MINOR;
	monza->miscdev.name	= "monzax";
	monza->miscdev.fops	= &monza_misc_fops;

	if (misc_register(&monza->miscdev)) {
		dev_err(&client->dev, "misc_register failed\n");
		goto err_miscdev;
	}

	dev_dbg(&client->dev, "%zu byte %s EEPROM, %u bytes/write\n",
		monza->bin.size, client->name, monza->write_max);

	return 0;

err_miscdev:
	sysfs_remove_bin_file(&client->dev.kobj, &monza->bin);
err_bin:
	kfree(monza->writebuf);
err_clients:
	if (monza->client[1])
		i2c_unregister_device(monza->client[1]);
err_struct:
	kfree(monza);
	return err;
}

static int monza_remove(struct i2c_client *client)
{
	struct monza_data *monza;

	monza = i2c_get_clientdata(client);
	misc_deregister(&monza->miscdev);
	sysfs_remove_bin_file(&client->dev.kobj, &monza->bin);
	kfree(monza->writebuf);

	if (monza->client[1])
		i2c_unregister_device(monza->client[1]);

	kfree(monza);
	return 0;
}

static struct i2c_driver monza_driver = {
	.driver = {
		.name = "monzax",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(acpi_monza_ids),
	},
	.probe = monza_probe,
	.remove = monza_remove,
	.id_table = i2c_monza_ids,
};

module_i2c_driver(monza_driver);

MODULE_AUTHOR("Jiantao Zhou<jiantao.zhou@intel.com>");
MODULE_DESCRIPTION("MONZA-X-2K RFID chip driver");
MODULE_LICENSE("GPL v2");
