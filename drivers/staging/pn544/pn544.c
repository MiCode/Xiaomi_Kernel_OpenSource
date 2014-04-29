/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/platform_data/pn544.h>
#include <linux/suspend.h>
#include <linux/wakelock.h>
#include <linux/poll.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>


#define MAX_BUFFER_SIZE		512

/*
 * PN544 power control via ioctl
 * PN544_SET_PWR(0): power off
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN544_SET_PWR	_IOW(PN544_MAGIC, 0x01, unsigned int)

#define PN544_MAGIC	0xe9

#define MIN_GPIO_DELAY 10000
#define MAX_GPIO_DELAY 15000

enum polarity {
	UNKNOWN = -1,
	ACTIVE_LOW = 0,
	ACTIVE_HIGH = 1,
};

enum pn544_cmd_id {
	PN544_POWER_OFF,
	PN544_POWER_ON,
	PN544_POWER_ON_FIRM,
};

struct pn544_dev	{
	wait_queue_head_t	read_wq;
	struct i2c_client	*client;
	struct miscdevice	pn544_device;
	struct wake_lock	read_wake;
	struct gpio_desc	*ven_gpio;
	struct gpio_desc	*firm_gpio;
	struct gpio_desc	*irq_gpio;
	enum polarity		nfc_en_polarity;
	unsigned int		max_i2c_xfer_size;
};

static void pn544_platform_init(struct pn544_dev *pn544_dev)
{
	int polarity, retry, ret;
	struct device *i2c_dev = &pn544_dev->client->dev;
	char rset_cmd[] = {0x05, 0xf9, 0x04, 0x00, 0xc3, 0xe5};
	int count = sizeof(rset_cmd);

	dev_dbg(i2c_dev, "%s : detecting nfc_en polarity\n", __func__);

	/* disable fw download */
	gpiod_set_value(pn544_dev->firm_gpio, 0);

	for (polarity = ACTIVE_LOW; polarity <= ACTIVE_HIGH; polarity++) {

		retry = 3;
		while (retry--) {
			/* power off */
			gpiod_set_value(pn544_dev->ven_gpio,
					!polarity);
			/* default power on/off delay */
			usleep_range(MIN_GPIO_DELAY, MAX_GPIO_DELAY);
			/* power on */
			gpiod_set_value(pn544_dev->ven_gpio,
					polarity);
			/* default power on/off delay */
			usleep_range(MIN_GPIO_DELAY, MAX_GPIO_DELAY);
			/* send reset */
			dev_dbg(i2c_dev, "%s : sending reset cmd\n", __func__);
			ret = i2c_master_send(pn544_dev->client,
					rset_cmd, count);
			if (ret == count) {
				dev_dbg(i2c_dev,
					"%s : nfc_en polarity : active %s\n",
					__func__,
					(polarity == 0 ? "low" : "high"));
				goto out;
			}
		}
	}

	dev_err(i2c_dev, "%s : could not detect nfc_en polarity\n", __func__);

out:
	/* store the detected polarity */
	pn544_dev->nfc_en_polarity = polarity;

	/* power off */
	gpiod_set_value(pn544_dev->ven_gpio,
			!pn544_dev->nfc_en_polarity);
}

static irqreturn_t pn544_dev_irq_handler(int irq, void *dev_id)
{
	struct pn544_dev *pn544_dev = dev_id;
	struct device *i2c_dev = &pn544_dev->client->dev;

	dev_dbg(i2c_dev, "%s : IRQ ENTER\n", __func__);

	wake_lock_timeout(&pn544_dev->read_wake, 1*HZ);

	/* Wake up waiting readers */
	wake_up(&pn544_dev->read_wq);

	return IRQ_HANDLED;
}

static ssize_t pn544_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	struct device *i2c_dev = &pn544_dev->client->dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret;
	char *tmp_p = tmp;
	int i2c_xfer_size;
	int i2c_xfer_ret;
	unsigned int max_i2c_xfer_size = pn544_dev->max_i2c_xfer_size;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (!gpiod_get_value(pn544_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		ret = wait_event_interruptible(pn544_dev->read_wq,
				gpiod_get_value(pn544_dev->irq_gpio));

		if (ret) {
			dev_err(i2c_dev, "%s : wait_event_interruptible: %d",
					__func__, ret);
			goto fail;
		}
	}

	/* Read data */
	ret = count;

	while (count) {
		i2c_xfer_size = count;
		if (max_i2c_xfer_size > 0 && i2c_xfer_size > max_i2c_xfer_size)
			i2c_xfer_size = max_i2c_xfer_size;

		i2c_xfer_ret = i2c_master_recv(pn544_dev->client,
				tmp_p, i2c_xfer_size);
		if (i2c_xfer_ret < 0) {
			dev_err(i2c_dev, "%s: i2c_master_recv returned %d\n",
					__func__, i2c_xfer_ret);
			return i2c_xfer_ret;
		}
		if (i2c_xfer_ret > i2c_xfer_size) {
			dev_err(i2c_dev,
				"%s: received too many bytes from i2c (%d)\n",
				__func__, i2c_xfer_ret);
			return -EIO;
		}

		count -= i2c_xfer_size;
		tmp_p += i2c_xfer_size;
	}

	if (copy_to_user(buf, tmp, ret)) {
		dev_warn(i2c_dev, "%s : failed to copy to user space\n",
					__func__);
		return -EFAULT;
	}

	/* Prevent the suspend after each read cycle for 1 sec
	 * to allow propagation of the event to upper layers of NFC
	 * stack
	 */
	wake_lock_timeout(&pn544_dev->read_wake, 1*HZ);

	/* Return the number of bytes read */
	dev_dbg(i2c_dev, "%s : Bytes read = %d: ", __func__, ret);

	return ret;

fail:
	dev_dbg(i2c_dev, "%s : wait_event is interrupted by a signal\n",
		__func__);
	return ret;
}

static unsigned int pn544_dev_poll(struct file *file, poll_table *wait)
{
	struct pn544_dev *pn544_dev = file->private_data;
	struct device *i2c_dev = &pn544_dev->client->dev;

	if (!gpiod_get_value(pn544_dev->irq_gpio)) {
		dev_dbg(i2c_dev, "%s : Waiting on available input data.\n",
				__func__);
		poll_wait(file, &pn544_dev->read_wq, wait);

		if (gpiod_get_value(pn544_dev->irq_gpio))
			return POLLIN | POLLRDNORM;
	} else
		return POLLIN | POLLRDNORM;

	dev_dbg(i2c_dev, "%s : No data on input stream.\n", __func__);
	return 0;
}

static ssize_t pn544_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev  *pn544_dev = filp->private_data;
	struct device *i2c_dev = &pn544_dev->client->dev;
	unsigned int max_i2c_xfer_size = pn544_dev->max_i2c_xfer_size;
	char tmp[MAX_BUFFER_SIZE];
	int ret;
	char *tmp_p = tmp;
	int i2c_xfer_size;
	int i2c_xfer_ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		dev_err(i2c_dev, "%s : failed to copy from user space\n",
				 __func__);
		return -EFAULT;
	}

	dev_dbg(i2c_dev, "%s : writing %zu bytes.\n", __func__, count);

	/* Write data */
	ret = count;

	while (count) {
		i2c_xfer_size = count;
		if (max_i2c_xfer_size > 0 && i2c_xfer_size > max_i2c_xfer_size)
			i2c_xfer_size = max_i2c_xfer_size;

		i2c_xfer_ret = i2c_master_send(pn544_dev->client,
				tmp_p, i2c_xfer_size);
		if (i2c_xfer_ret < 0) {
			dev_err(i2c_dev, "%s : i2c_master_send returned %d\n",
					__func__, i2c_xfer_ret);
			return i2c_xfer_ret;
		}
		if (i2c_xfer_ret != i2c_xfer_size) {
			dev_err(i2c_dev, "%s : i2c_master_send invalid size %d\n",
					__func__, i2c_xfer_ret);
			return -EIO;
		}

		count -= i2c_xfer_size;
		tmp_p += i2c_xfer_size;
	}

	return ret;
}

static int pn544_dev_open(struct inode *inode, struct file *filp)
{
	struct pn544_dev *pn544_dev = container_of(filp->private_data,
						struct pn544_dev,
						pn544_device);
	filp->private_data = pn544_dev;

	return 0;
}

static int pn544_dev_release(struct inode *inode, struct file *filp)
{
	struct pn544_dev *pn544_dev = filp->private_data;

	filp->private_data = NULL;

	if (wake_lock_active(&pn544_dev->read_wake))
		wake_unlock(&pn544_dev->read_wake);

	return 0;
}

static long pn544_set_power(struct file *filp, unsigned long arg)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	struct device *i2c_dev = &pn544_dev->client->dev;

	if (pn544_dev->nfc_en_polarity == UNKNOWN)
		pn544_platform_init(pn544_dev);

	switch (arg) {
	case PN544_POWER_ON_FIRM:
		/* power on with firmware download (requires hw reset)
		 */
		dev_dbg(i2c_dev, "%s power on with firmware\n",
					__func__);
		gpiod_set_value(pn544_dev->ven_gpio,
				pn544_dev->nfc_en_polarity);
		gpiod_set_value(pn544_dev->firm_gpio, 1);
		/* default power on/off delay */
		usleep_range(MIN_GPIO_DELAY, MAX_GPIO_DELAY);
		gpiod_set_value(pn544_dev->ven_gpio,
				!pn544_dev->nfc_en_polarity);
		/* default power on/off delay */
		usleep_range(MIN_GPIO_DELAY, MAX_GPIO_DELAY);
		gpiod_set_value(pn544_dev->ven_gpio,
				pn544_dev->nfc_en_polarity);
		/* default power on/off delay */
		usleep_range(MIN_GPIO_DELAY, MAX_GPIO_DELAY);
		break;
	case PN544_POWER_ON:
		/* power on */
		dev_dbg(i2c_dev, "%s power on\n", __func__);
		gpiod_set_value(pn544_dev->firm_gpio, 0);
		gpiod_set_value(pn544_dev->ven_gpio,
				pn544_dev->nfc_en_polarity);
		/* default power on/off delay */
		usleep_range(MIN_GPIO_DELAY, MAX_GPIO_DELAY);
		break;
	case PN544_POWER_OFF:
		/* power off */
		dev_dbg(i2c_dev, "%s power off\n", __func__);
		gpiod_set_value(pn544_dev->firm_gpio, 0);
		gpiod_set_value(pn544_dev->ven_gpio,
				!pn544_dev->nfc_en_polarity);
		/* default power on/off delay */
		usleep_range(MIN_GPIO_DELAY, MAX_GPIO_DELAY);
		break;
	default:
		dev_err(i2c_dev, "%s bad arg %lu\n", __func__, arg);
		return -EINVAL;
	}
	return 0;
}

static long pn544_dev_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	struct device *i2c_dev = &pn544_dev->client->dev;

	switch (cmd) {
	case PN544_SET_PWR:
		return pn544_set_power(filp, arg);
	default:
		dev_err(i2c_dev, "%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn544_dev_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= pn544_dev_read,
	.write		= pn544_dev_write,
	.poll		= pn544_dev_poll,
	.open		= pn544_dev_open,
	.release	= pn544_dev_release,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl	= pn544_dev_ioctl,
#endif
#ifdef CONFIG_COMPAT
	.compat_ioctl = pn544_dev_ioctl
#endif
};

static int pn544_acpi_probe(struct i2c_client *client,
					struct pn544_dev *pn544_dev)
{
	const struct acpi_device_id *id;
	struct device *dev;
	unsigned long long data;
	acpi_status status;
	struct gpio_desc *gpio;
	int ret = 0;

	if (!client || !pn544_dev)
		return -EINVAL;

	dev = &client->dev;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);

	if (!id)
		return -ENODEV;

	gpio = devm_gpiod_get_index(dev, "irq_gpio", NFC_GPIO_IRQ);
	if (!IS_ERR(gpio)) {
		ret = gpiod_direction_input(gpio);
		if (ret)
			return ret;
		pn544_dev->irq_gpio = gpio;
	}

	gpio = devm_gpiod_get_index(dev, "enable_gpio", NFC_GPIO_ENABLE);
	if (!IS_ERR(gpio)) {
		ret = gpiod_direction_output(gpio, 0);
		if (ret)
			return ret;
		pn544_dev->ven_gpio = gpio;
	}

	gpio = devm_gpiod_get_index(dev, "firm_reset", NFC_GPIO_FW_RESET);
	if (!IS_ERR(gpio)) {
		ret = gpiod_direction_output(gpio, 0);
		if (ret)
			return ret;
		pn544_dev->firm_gpio = gpio;
	}

	status = acpi_evaluate_integer(ACPI_HANDLE(dev), "XFER",
				NULL, &data);
	if (ACPI_FAILURE(status)) {
		pr_err("Error evaluating ACPI XFER object");
		return -ENODEV;
	}

	pn544_dev->max_i2c_xfer_size = data;

	return 0;
}

static int pn544_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct pn544_dev *pn544_dev;

	pr_debug("%s : entering probe\n", __func__);

#ifndef HAVE_UNLOCKED_IOCTL
	pr_err("%s: must have IOCTL", __func__);
	return -ENODEV;
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	pn544_dev = devm_kzalloc(&client->dev, sizeof(*pn544_dev), GFP_KERNEL);
	if (!pn544_dev) {
		dev_err(&client->dev,
				"failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	if (ACPI_HANDLE(&client->dev)) {
		ret = pn544_acpi_probe(client, pn544_dev);
		if (ret)
			return ret;
	} else {
		dev_err(&client->dev, "gpio resources are not available\n");
		return -ENODEV;
	}

	pn544_dev->client   = client;

	pn544_dev->nfc_en_polarity = UNKNOWN;

	dev_dbg(&client->dev, "%s : irq gpio:      %d\n", __func__,
			desc_to_gpio(pn544_dev->irq_gpio));
	dev_dbg(&client->dev, "%s : ven gpio:      %d\n", __func__,
			desc_to_gpio(pn544_dev->ven_gpio));
	dev_dbg(&client->dev, "%s : fw gpio:       %d\n", __func__,
			desc_to_gpio(pn544_dev->firm_gpio));
	dev_dbg(&client->dev, "%s : i2c xfer size: %d\n", __func__,
			pn544_dev->max_i2c_xfer_size);

	/* init wakelock and queues */
	init_waitqueue_head(&pn544_dev->read_wq);

	wake_lock_init(&pn544_dev->read_wake, WAKE_LOCK_SUSPEND, "pn544_nfc");

	pn544_dev->pn544_device.minor = MISC_DYNAMIC_MINOR;
	pn544_dev->pn544_device.name = "pn544";
	pn544_dev->pn544_device.fops = &pn544_dev_fops;

	ret = misc_register(&pn544_dev->pn544_device);
	if (ret) {
		dev_err(&client->dev, "%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	dev_dbg(&client->dev, "%s : requesting IRQ %d\n", __func__,
				client->irq);

	ret = devm_request_irq(&client->dev, client->irq, pn544_dev_irq_handler,
			  IRQF_TRIGGER_RISING, client->name, pn544_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}

	enable_irq_wake(client->irq);

	i2c_set_clientdata(client, pn544_dev);

	return 0;

err_request_irq_failed:
	misc_deregister(&pn544_dev->pn544_device);
err_misc_register:
	wake_lock_destroy(&pn544_dev->read_wake);

	return ret;
}

static int pn544_remove(struct i2c_client *client)
{
	struct pn544_dev *pn544_dev;

	pn544_dev = i2c_get_clientdata(client);

	if (wake_lock_active(&pn544_dev->read_wake))
		wake_unlock(&pn544_dev->read_wake);

	wake_lock_destroy(&pn544_dev->read_wake);

	misc_deregister(&pn544_dev->pn544_device);

	return 0;
}

#ifdef CONFIG_PM

static int pn544_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	disable_irq(client->irq);

	return 0;
}

static int pn544_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	enable_irq(client->irq);

	return 0;
}

SIMPLE_DEV_PM_OPS(pn544_pm_ops, pn544_suspend, pn544_resume);

#endif

static const struct i2c_device_id pn544_id[] = {
	{ "pn544", 0 },
	{ }
};

static const struct acpi_device_id pn544_acpi_match[] = {
	{ "PN544", 0 },
	{ },
};

static struct i2c_driver pn544_driver = {
	.id_table	= pn544_id,
	.probe		= pn544_probe,
	.remove		= pn544_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn544",
		.acpi_match_table = ACPI_PTR(pn544_acpi_match),
#ifdef CONFIG_PM
		.pm = &pn544_pm_ops,
#endif
	},
};
module_i2c_driver(pn544_driver);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN544 driver");
MODULE_LICENSE("GPL");
