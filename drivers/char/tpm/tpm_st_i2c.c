/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <mach/gpio.h>
#include <mach/tpm_st_i2c.h>
#include <mach/msm_iomap.h>
#include "tpm.h"

#define DEVICE_NAME "tpm_st_i2c"

#define TPM_HEADER_LEN sizeof(struct tpm_input_header)
#define TPM_ST_I2C_BLOCK_MAX 40

struct tpm_st_i2c_dev {
	struct i2c_client *client;
	struct tpm_st_i2c_platform_data *pd;
	struct completion com[2];
};

/* for completion array */
#define ACCEPT_CMD_INDEX 0
#define DATA_AVAIL_INDEX 1

static struct tpm_st_i2c_dev *tpm_st_i2c_dev;

#define TPM_ST_I2C_REQ_COMPLETE_MASK 1

static u8 tpm_st_i2c_status(struct tpm_chip *chip)
{
	int gpio = tpm_st_i2c_dev->pd->data_avail_gpio;
	return gpio_get_value(gpio);
}

static void tpm_st_i2c_cancel(struct tpm_chip *chip)
{
	/* not supported */
	return;
}

static int tpm_st_i2c_transfer_buf(struct tpm_chip *chip, u8 *buf, size_t count,
				   int recv)
{
	struct i2c_msg msg = {
		.addr = tpm_st_i2c_dev->client->addr,
		.flags = 0,
		.buf = buf,
		.len = TPM_HEADER_LEN, /* must read/write header first */
	};
	int gpio;
	int irq;
	struct completion *com;
	__be32 *native_size;
	int read_header = 0;
	int rc = 0;
	int len = count;
	uint32_t size = count;
	int tmp;

	if (recv) {
		msg.flags |= I2C_M_RD;
		read_header = 1;
		gpio = tpm_st_i2c_dev->pd->data_avail_gpio;
		irq = tpm_st_i2c_dev->pd->data_avail_irq;
		com = &tpm_st_i2c_dev->com[DATA_AVAIL_INDEX];
	} else {
		gpio = tpm_st_i2c_dev->pd->accept_cmd_gpio;
		irq = tpm_st_i2c_dev->pd->accept_cmd_irq;
		com = &tpm_st_i2c_dev->com[ACCEPT_CMD_INDEX];
	}

	if (len < TPM_HEADER_LEN) {
		dev_dbg(chip->dev, "%s: invalid len\n", __func__);
		return -EINVAL;
	}

	do {
		if (!gpio_get_value(gpio)) {
			/* reset the completion in case the irq fired
			 * during the probe
			 */
			init_completion(com);
			enable_irq(irq);
			tmp = wait_for_completion_interruptible_timeout(
				com, HZ/2);
			if (!tmp) {
				dev_dbg(chip->dev, "%s timeout\n",
					__func__);
				return -EBUSY;
			}
		}
		rc = i2c_transfer(tpm_st_i2c_dev->client->adapter,
				  &msg, 1);
		if (rc < 0) {
			dev_dbg(chip->dev, "Error in I2C transfer\n");
			return rc;
		}
		if (read_header) {
			read_header = 0;
			native_size = (__force __be32 *) (buf + 2);
			size = be32_to_cpu(*native_size);
			if (count < size) {
				dev_dbg(chip->dev,
					"%s: invalid count\n",
					__func__);
				rc = -EIO;
			}
			len = size;
		}
		len -= msg.len;
		if (len) {
			buf += msg.len;
			msg.buf = buf;
			if (len > TPM_ST_I2C_BLOCK_MAX)
				msg.len = TPM_ST_I2C_BLOCK_MAX;
			else
				msg.len = len;
		}
	} while (len > 0);

	if (rc >= 0)
		return size;
	else
		return rc;
}

static int tpm_st_i2c_recv(struct tpm_chip *chip, u8 *buf, size_t count)
{
	return tpm_st_i2c_transfer_buf(chip, buf, count, 1);
}

static int tpm_st_i2c_send(struct tpm_chip *chip, u8 *buf, size_t len)
{
	return tpm_st_i2c_transfer_buf(chip, buf, len, 0);
}

#ifdef CONFIG_PM
static int tpm_st_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	return tpm_pm_suspend(&client->dev, msg);
}

static int tpm_st_i2c_resume(struct i2c_client *client)
{
	return tpm_pm_resume(&client->dev);
}
#endif

static const struct file_operations tpm_st_i2c_fs_ops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = tpm_open,
	.read = tpm_read,
	.write = tpm_write,
	.release = tpm_release,
};

static DEVICE_ATTR(pubek, S_IRUGO, tpm_show_pubek, NULL);
static DEVICE_ATTR(pcrs, S_IRUGO, tpm_show_pcrs, NULL);
static DEVICE_ATTR(enabled, S_IRUGO, tpm_show_enabled, NULL);
static DEVICE_ATTR(active, S_IRUGO, tpm_show_active, NULL);
static DEVICE_ATTR(owned, S_IRUGO, tpm_show_owned, NULL);
static DEVICE_ATTR(temp_deactivated, S_IRUGO, tpm_show_temp_deactivated,
		   NULL);
static DEVICE_ATTR(caps, S_IRUGO, tpm_show_caps_1_2, NULL);

static struct attribute *tpm_st_i2c_attrs[] = {
	&dev_attr_pubek.attr,
	&dev_attr_pcrs.attr,
	&dev_attr_enabled.attr,
	&dev_attr_active.attr,
	&dev_attr_owned.attr,
	&dev_attr_temp_deactivated.attr,
	&dev_attr_caps.attr,
	NULL,
};

static struct attribute_group tpm_st_i2c_attr_grp = {
	.attrs = tpm_st_i2c_attrs
};

static struct tpm_vendor_specific tpm_st_i2c_vendor = {
	.status = tpm_st_i2c_status,
	.recv = tpm_st_i2c_recv,
	.send = tpm_st_i2c_send,
	.cancel = tpm_st_i2c_cancel,
	.req_complete_mask = TPM_ST_I2C_REQ_COMPLETE_MASK,
	.req_complete_val = TPM_ST_I2C_REQ_COMPLETE_MASK,
	.req_canceled = 0xff,  /* not supported */
	.attr_group = &tpm_st_i2c_attr_grp,
	.miscdev = {
		    .fops = &tpm_st_i2c_fs_ops,},
};

static irqreturn_t tpm_st_i2c_isr(int irq, void *dev_id)
{
	disable_irq_nosync(irq);
	if (irq == tpm_st_i2c_dev->pd->accept_cmd_irq)
		complete(&tpm_st_i2c_dev->com[ACCEPT_CMD_INDEX]);
	else
		complete(&tpm_st_i2c_dev->com[DATA_AVAIL_INDEX]);
	return IRQ_HANDLED;
}

static int tpm_st_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int rc = 0;
	struct tpm_st_i2c_platform_data *pd;
	struct  tpm_chip *chip;
	int high;

	dev_dbg(&client->dev, "%s()\n", __func__);

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE |
				     I2C_FUNC_SMBUS_I2C_BLOCK |
				     I2C_FUNC_I2C)) {
		dev_err(&client->dev, "incompatible adapter\n");
		return -ENODEV;
	}

	pd = client->dev.platform_data;
	if (!pd || !pd->gpio_setup || !pd->gpio_release) {
		dev_err(&client->dev, "platform data not setup\n");
		rc = -EFAULT;
		goto no_platform_data;
	}
	rc = pd->gpio_setup();
	if (rc) {
		dev_err(&client->dev, "gpio_setup failed\n");
		goto gpio_setup_fail;
	}

	gpio_direction_input(pd->accept_cmd_gpio);
	gpio_direction_input(pd->data_avail_gpio);

	tpm_st_i2c_dev = kzalloc(sizeof(struct tpm_st_i2c_dev), GFP_KERNEL);
	if (!tpm_st_i2c_dev) {
		printk(KERN_ERR "%s Unable to allocate memory for struct\n",
		       __func__);
		rc = -ENOMEM;
		goto kzalloc_fail;
	}

	tpm_st_i2c_dev->client = client;
	tpm_st_i2c_dev->pd = pd;

	init_completion(&tpm_st_i2c_dev->com[ACCEPT_CMD_INDEX]);
	init_completion(&tpm_st_i2c_dev->com[DATA_AVAIL_INDEX]);
	/* This logic allows us to setup irq but not have it enabled, in
	 * case the lines are already active
	 */
	high = gpio_get_value(pd->data_avail_gpio);
	rc = request_irq(pd->data_avail_irq, tpm_st_i2c_isr, IRQF_TRIGGER_HIGH,
			 DEVICE_NAME "-data", NULL);
	if (rc) {
		dev_err(&client->dev, "request for data irq failed\n");
		goto data_irq_fail;
	}
	if (!high)
		disable_irq(pd->data_avail_irq);
	high = gpio_get_value(pd->accept_cmd_gpio);
	rc = request_irq(pd->accept_cmd_irq, tpm_st_i2c_isr, IRQF_TRIGGER_HIGH,
			 DEVICE_NAME "-cmd", NULL);
	if (rc) {
		dev_err(&client->dev, "request for cmd irq failed\n");
		goto cmd_irq_fail;
	}
	if (!high)
		disable_irq(pd->accept_cmd_irq);

	tpm_st_i2c_vendor.irq = pd->data_avail_irq;

	chip = tpm_register_hardware(&client->dev, &tpm_st_i2c_vendor);
	if (!chip) {
		dev_err(&client->dev, "Could not register tpm hardware\n");
		rc = -ENODEV;
		goto tpm_reg_fail;
	}

	dev_info(&client->dev, "added\n");

	return 0;

tpm_reg_fail:
	free_irq(pd->accept_cmd_irq, NULL);
cmd_irq_fail:
	free_irq(pd->data_avail_irq, NULL);
data_irq_fail:
kzalloc_fail:
	pd->gpio_release();
gpio_setup_fail:
no_platform_data:

	return rc;
}

static int __exit tpm_st_i2c_remove(struct i2c_client *client)
{
	free_irq(tpm_st_i2c_dev->pd->accept_cmd_irq, NULL);
	free_irq(tpm_st_i2c_dev->pd->data_avail_irq, NULL);
	tpm_remove_hardware(&client->dev);
	tpm_st_i2c_dev->pd->gpio_release();
	kfree(tpm_st_i2c_dev);

	return 0;
}

static const struct i2c_device_id tpm_st_i2c_id[] = {
	{ DEVICE_NAME, 0 },
	{ }
};

static struct i2c_driver tpm_st_i2c_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tpm_st_i2c_probe,
	.remove =  __exit_p(tpm_st_i2c_remove),
#ifdef CONFIG_PM
	.suspend = tpm_st_i2c_suspend,
	.resume = tpm_st_i2c_resume,
#endif
	.id_table = tpm_st_i2c_id,
};

static int __init tpm_st_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&tpm_st_i2c_driver);
	if (ret)
		printk(KERN_ERR "%s: failed to add i2c driver\n", __func__);

	return ret;
}

static void __exit tpm_st_i2c_exit(void)
{
	i2c_del_driver(&tpm_st_i2c_driver);
}

module_init(tpm_st_i2c_init);
module_exit(tpm_st_i2c_exit);
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("ST19NP18-TPM-I2C driver");
