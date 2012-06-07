/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mfd/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <sound/soc.h>

#define WCD9XXX_SLIM_GLA_MAX_RETRIES 5
#define WCD9XXX_REGISTER_START_OFFSET 0x800
#define WCD9XXX_SLIM_RW_MAX_TRIES 3

#define MAX_WCD9XXX_DEVICE	4
#define WCD9XXX_I2C_MODE	0x03

struct wcd9xxx_i2c {
	struct i2c_client *client;
	struct i2c_msg xfer_msg[2];
	struct mutex xfer_lock;
	int mod_id;
};

struct wcd9xxx_i2c wcd9xxx_modules[MAX_WCD9XXX_DEVICE];
static int wcd9xxx_intf = -1;

static int wcd9xxx_read(struct wcd9xxx *wcd9xxx, unsigned short reg,
		       int bytes, void *dest, bool interface_reg)
{
	int ret;
	u8 *buf = dest;

	if (bytes <= 0) {
		dev_err(wcd9xxx->dev, "Invalid byte read length %d\n", bytes);
		return -EINVAL;
	}

	ret = wcd9xxx->read_dev(wcd9xxx, reg, bytes, dest, interface_reg);
	if (ret < 0) {
		dev_err(wcd9xxx->dev, "Codec read failed\n");
		return ret;
	} else
		dev_dbg(wcd9xxx->dev, "Read 0x%02x from 0x%x\n",
			 *buf, reg);

	return 0;
}
int wcd9xxx_reg_read(struct wcd9xxx *wcd9xxx, unsigned short reg)
{
	u8 val;
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_read(wcd9xxx, reg, 1, &val, false);
	mutex_unlock(&wcd9xxx->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(wcd9xxx_reg_read);

static int wcd9xxx_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *src, bool interface_reg)
{
	u8 *buf = src;

	if (bytes <= 0) {
		pr_err("%s: Error, invalid write length\n", __func__);
		return -EINVAL;
	}

	dev_dbg(wcd9xxx->dev, "Write %02x to 0x%x\n",
		 *buf, reg);

	return wcd9xxx->write_dev(wcd9xxx, reg, bytes, src, interface_reg);
}

int wcd9xxx_reg_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		     u8 val)
{
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_write(wcd9xxx, reg, 1, &val, false);
	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_reg_write);

static u8 wcd9xxx_pgd_la;
static u8 wcd9xxx_inf_la;

int wcd9xxx_interface_reg_read(struct wcd9xxx *wcd9xxx, unsigned short reg)
{
	u8 val;
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_read(wcd9xxx, reg, 1, &val, true);
	mutex_unlock(&wcd9xxx->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(wcd9xxx_interface_reg_read);

int wcd9xxx_interface_reg_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		     u8 val)
{
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_write(wcd9xxx, reg, 1, &val, true);
	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_interface_reg_write);

int wcd9xxx_bulk_read(struct wcd9xxx *wcd9xxx, unsigned short reg,
		     int count, u8 *buf)
{
	int ret;

	mutex_lock(&wcd9xxx->io_lock);

	ret = wcd9xxx_read(wcd9xxx, reg, count, buf, false);

	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_bulk_read);

int wcd9xxx_bulk_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		     int count, u8 *buf)
{
	int ret;

	mutex_lock(&wcd9xxx->io_lock);

	ret = wcd9xxx_write(wcd9xxx, reg, count, buf, false);

	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_bulk_write);

static int wcd9xxx_slim_read_device(struct wcd9xxx *wcd9xxx, unsigned short reg,
				int bytes, void *dest, bool interface)
{
	int ret;
	struct slim_ele_access msg;
	int slim_read_tries = WCD9XXX_SLIM_RW_MAX_TRIES;
	msg.start_offset = WCD9XXX_REGISTER_START_OFFSET + reg;
	msg.num_bytes = bytes;
	msg.comp = NULL;

	while (1) {
		mutex_lock(&wcd9xxx->xfer_lock);
		ret = slim_request_val_element(interface ?
			       wcd9xxx->slim_slave : wcd9xxx->slim,
			       &msg, dest, bytes);
		mutex_unlock(&wcd9xxx->xfer_lock);
		if (likely(ret == 0) || (--slim_read_tries == 0))
			break;
		usleep_range(5000, 5000);
	}

	if (ret)
		pr_err("%s: Error, Codec read failed (%d)\n", __func__, ret);

	return ret;
}
/* Interface specifies whether the write is to the interface or general
 * registers.
 */
static int wcd9xxx_slim_write_device(struct wcd9xxx *wcd9xxx,
		unsigned short reg, int bytes, void *src, bool interface)
{
	int ret;
	struct slim_ele_access msg;
	int slim_write_tries = WCD9XXX_SLIM_RW_MAX_TRIES;
	msg.start_offset = WCD9XXX_REGISTER_START_OFFSET + reg;
	msg.num_bytes = bytes;
	msg.comp = NULL;

	while (1) {
		mutex_lock(&wcd9xxx->xfer_lock);
		ret = slim_change_val_element(interface ?
			      wcd9xxx->slim_slave : wcd9xxx->slim,
			      &msg, src, bytes);
		mutex_unlock(&wcd9xxx->xfer_lock);
		if (likely(ret == 0) || (--slim_write_tries == 0))
			break;
		usleep_range(5000, 5000);
	}

	if (ret)
		pr_err("%s: Error, Codec write failed (%d)\n", __func__, ret);

	return ret;
}

static struct mfd_cell tabla1x_devs[] = {
	{
		.name = "tabla1x_codec",
	},
};

static struct mfd_cell tabla_devs[] = {
	{
		.name = "tabla_codec",
	},
};

static struct mfd_cell sitar_devs[] = {
	{
		.name = "sitar_codec",
	},
};

static void wcd9xxx_bring_up(struct wcd9xxx *wcd9xxx)
{
	wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_LEAKAGE_CTL, 0x4);
	wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_CDC_CTL, 0);
	usleep_range(5000, 5000);
	wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_CDC_CTL, 3);
	wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_LEAKAGE_CTL, 3);
}

static void wcd9xxx_bring_down(struct wcd9xxx *wcd9xxx)
{
	wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_LEAKAGE_CTL, 0x7);
	wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_LEAKAGE_CTL, 0x6);
	wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_LEAKAGE_CTL, 0xe);
	wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_LEAKAGE_CTL, 0x8);
}

static int wcd9xxx_reset(struct wcd9xxx *wcd9xxx)
{
	int ret;

	if (wcd9xxx->reset_gpio) {
		ret = gpio_request(wcd9xxx->reset_gpio, "CDC_RESET");
		if (ret) {
			pr_err("%s: Failed to request gpio %d\n", __func__,
				wcd9xxx->reset_gpio);
			wcd9xxx->reset_gpio = 0;
			return ret;
		}

		gpio_direction_output(wcd9xxx->reset_gpio, 1);
		msleep(20);
		gpio_direction_output(wcd9xxx->reset_gpio, 0);
		msleep(20);
		gpio_direction_output(wcd9xxx->reset_gpio, 1);
		msleep(20);
	}
	return 0;
}

static void wcd9xxx_free_reset(struct wcd9xxx *wcd9xxx)
{
	if (wcd9xxx->reset_gpio) {
		gpio_free(wcd9xxx->reset_gpio);
		wcd9xxx->reset_gpio = 0;
	}
}

static int wcd9xxx_device_init(struct wcd9xxx *wcd9xxx, int irq)
{
	int ret;
	u8 idbyte_0, idbyte_1, idbyte_2, idbyte_3;
	struct mfd_cell *wcd9xxx_dev = NULL;
	int wcd9xxx_dev_size = 0;

	mutex_init(&wcd9xxx->io_lock);
	mutex_init(&wcd9xxx->xfer_lock);

	mutex_init(&wcd9xxx->pm_lock);
	wcd9xxx->wlock_holders = 0;
	wcd9xxx->pm_state = WCD9XXX_PM_SLEEPABLE;
	init_waitqueue_head(&wcd9xxx->pm_wq);
	wake_lock_init(&wcd9xxx->wlock, WAKE_LOCK_IDLE, "wcd9310-irq");

	dev_set_drvdata(wcd9xxx->dev, wcd9xxx);

	wcd9xxx_bring_up(wcd9xxx);

	ret = wcd9xxx_irq_init(wcd9xxx);
	if (ret) {
		pr_err("IRQ initialization failed\n");
		goto err;
	}

	idbyte_0 = wcd9xxx_reg_read(wcd9xxx, WCD9XXX_A_CHIP_ID_BYTE_0);
	idbyte_1 = wcd9xxx_reg_read(wcd9xxx, WCD9XXX_A_CHIP_ID_BYTE_1);
	idbyte_2 = wcd9xxx_reg_read(wcd9xxx, WCD9XXX_A_CHIP_ID_BYTE_2);
	idbyte_3 = wcd9xxx_reg_read(wcd9xxx, WCD9XXX_A_CHIP_ID_BYTE_3);

	wcd9xxx->version = wcd9xxx_reg_read(wcd9xxx,
			WCD9XXX_A_CHIP_VERSION) & 0x1F;
	pr_info("%s : Codec version %u initialized\n",
		__func__, wcd9xxx->version);
	pr_info("idbyte_0[%08x] idbyte_1[%08x] idbyte_2[%08x] idbyte_3[%08x]\n",
			idbyte_0, idbyte_1, idbyte_2, idbyte_3);

	if (wcd9xxx->slim != NULL) {
		if (!strncmp(wcd9xxx->slim->name, "tabla", 5)) {
			if (TABLA_IS_1_X(wcd9xxx->version)) {
				wcd9xxx_dev = tabla1x_devs;
				wcd9xxx_dev_size = ARRAY_SIZE(tabla1x_devs);
			} else {
				wcd9xxx_dev = tabla_devs;
				wcd9xxx_dev_size = ARRAY_SIZE(tabla_devs);
			}
		} else {
			wcd9xxx_dev = sitar_devs;
			wcd9xxx_dev_size = ARRAY_SIZE(sitar_devs);
		}
	} else {
		/* Need to add here check for Tabla.
		 * For now the read of version takes
		 * care of now only tabla.
		 */
		pr_debug("%s : Read codec version using I2C\n",	__func__);
		if (TABLA_IS_1_X(wcd9xxx->version)) {
			wcd9xxx_dev = tabla1x_devs;
			wcd9xxx_dev_size = ARRAY_SIZE(tabla1x_devs);
		} else if (TABLA_IS_2_0(wcd9xxx->version)) {
			wcd9xxx_dev = tabla_devs;
			wcd9xxx_dev_size = ARRAY_SIZE(tabla_devs);
		} else {
			wcd9xxx_dev = sitar_devs;
			wcd9xxx_dev_size = ARRAY_SIZE(sitar_devs);
		}
	}

	ret = mfd_add_devices(wcd9xxx->dev, -1,
		      wcd9xxx_dev, wcd9xxx_dev_size,
		      NULL, 0);
	if (ret != 0) {
		dev_err(wcd9xxx->dev, "Failed to add children: %d\n", ret);
		goto err_irq;
	}
	return ret;
err_irq:
	wcd9xxx_irq_exit(wcd9xxx);
err:
	wcd9xxx_bring_down(wcd9xxx);
	wake_lock_destroy(&wcd9xxx->wlock);
	mutex_destroy(&wcd9xxx->pm_lock);
	mutex_destroy(&wcd9xxx->io_lock);
	mutex_destroy(&wcd9xxx->xfer_lock);
	return ret;
}

static void wcd9xxx_device_exit(struct wcd9xxx *wcd9xxx)
{
	wcd9xxx_irq_exit(wcd9xxx);
	wcd9xxx_bring_down(wcd9xxx);
	wcd9xxx_free_reset(wcd9xxx);
	mutex_destroy(&wcd9xxx->pm_lock);
	wake_lock_destroy(&wcd9xxx->wlock);
	mutex_destroy(&wcd9xxx->io_lock);
	mutex_destroy(&wcd9xxx->xfer_lock);
	if (wcd9xxx_intf == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		slim_remove_device(wcd9xxx->slim_slave);
	kfree(wcd9xxx);
}


#ifdef CONFIG_DEBUG_FS
struct wcd9xxx *debugCodec;

static struct dentry *debugfs_wcd9xxx_dent;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_poke;

static unsigned char read_data;

static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (strict_strtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static ssize_t codec_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[8];

	snprintf(lbuf, sizeof(lbuf), "0x%x\n", read_data);
	return simple_read_from_buffer(ubuf, count, ppos, lbuf,
		strnlen(lbuf, 7));
}


static ssize_t codec_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *access_str = filp->private_data;
	char lbuf[32];
	int rc;
	long int param[5];

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	if (!strncmp(access_str, "poke", 6)) {
		/* write */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] <= 0x3FF) && (param[1] <= 0xFF) &&
			(rc == 0))
			wcd9xxx_interface_reg_write(debugCodec, param[0],
				param[1]);
		else
			rc = -EINVAL;
	} else if (!strncmp(access_str, "peek", 6)) {
		/* read */
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0x3FF) && (rc == 0))
			read_data = wcd9xxx_interface_reg_read(debugCodec,
				param[0]);
		else
			rc = -EINVAL;
	}

	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
	.read = codec_debug_read
};
#endif

static int wcd9xxx_enable_supplies(struct wcd9xxx *wcd9xxx,
				struct wcd9xxx_pdata *pdata)
{
	int ret;
	int i;
	wcd9xxx->supplies = kzalloc(sizeof(struct regulator_bulk_data) *
				   ARRAY_SIZE(pdata->regulator),
				   GFP_KERNEL);
	if (!wcd9xxx->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++)
		wcd9xxx->supplies[i].supply = pdata->regulator[i].name;

	ret = regulator_bulk_get(wcd9xxx->dev, ARRAY_SIZE(pdata->regulator),
				 wcd9xxx->supplies);
	if (ret != 0) {
		dev_err(wcd9xxx->dev, "Failed to get supplies: err = %d\n",
							ret);
		goto err_supplies;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		ret = regulator_set_voltage(wcd9xxx->supplies[i].consumer,
			pdata->regulator[i].min_uV, pdata->regulator[i].max_uV);
		if (ret) {
			pr_err("%s: Setting regulator voltage failed for "
				"regulator %s err = %d\n", __func__,
				wcd9xxx->supplies[i].supply, ret);
			goto err_get;
		}

		ret = regulator_set_optimum_mode(wcd9xxx->supplies[i].consumer,
			pdata->regulator[i].optimum_uA);
		if (ret < 0) {
			pr_err("%s: Setting regulator optimum mode failed for "
				"regulator %s err = %d\n", __func__,
				wcd9xxx->supplies[i].supply, ret);
			goto err_get;
		}
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(pdata->regulator),
				    wcd9xxx->supplies);
	if (ret != 0) {
		dev_err(wcd9xxx->dev, "Failed to enable supplies: err = %d\n",
				ret);
		goto err_configure;
	}
	return ret;

err_configure:
	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		regulator_set_voltage(wcd9xxx->supplies[i].consumer, 0,
			pdata->regulator[i].max_uV);
		regulator_set_optimum_mode(wcd9xxx->supplies[i].consumer, 0);
	}
err_get:
	regulator_bulk_free(ARRAY_SIZE(pdata->regulator), wcd9xxx->supplies);
err_supplies:
	kfree(wcd9xxx->supplies);
err:
	return ret;
}

static void wcd9xxx_disable_supplies(struct wcd9xxx *wcd9xxx,
				     struct wcd9xxx_pdata *pdata)
{
	int i;

	regulator_bulk_disable(ARRAY_SIZE(pdata->regulator),
				    wcd9xxx->supplies);
	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		regulator_set_voltage(wcd9xxx->supplies[i].consumer, 0,
			pdata->regulator[i].max_uV);
		regulator_set_optimum_mode(wcd9xxx->supplies[i].consumer, 0);
	}
	regulator_bulk_free(ARRAY_SIZE(pdata->regulator), wcd9xxx->supplies);
	kfree(wcd9xxx->supplies);
}

int wcd9xxx_get_intf_type(void)
{
	return wcd9xxx_intf;
}
EXPORT_SYMBOL_GPL(wcd9xxx_get_intf_type);

struct wcd9xxx_i2c *get_i2c_wcd9xxx_device_info(u16 reg)
{
	u16 mask = 0x0f00;
	int value = 0;
	struct wcd9xxx_i2c *wcd9xxx = NULL;
	value = ((reg & mask) >> 8) & 0x000f;
	switch (value) {
	case 0:
		wcd9xxx = &wcd9xxx_modules[0];
		break;
	case 1:
		wcd9xxx = &wcd9xxx_modules[1];
		break;
	case 2:
		wcd9xxx = &wcd9xxx_modules[2];
		break;
	case 3:
		wcd9xxx = &wcd9xxx_modules[3];
		break;
	default:
		break;
	}
	return wcd9xxx;
}

int wcd9xxx_i2c_write_device(u16 reg, u8 *value,
				u32 bytes)
{

	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	u8 data[bytes + 1];
	struct wcd9xxx_i2c *wcd9xxx;

	wcd9xxx = get_i2c_wcd9xxx_device_info(reg);
	if (wcd9xxx == NULL || wcd9xxx->client == NULL) {
		pr_err("failed to get device info\n");
		return -ENODEV;
	}
	reg_addr = (u8)reg;
	msg = &wcd9xxx->xfer_msg[0];
	msg->addr = wcd9xxx->client->addr;
	msg->len = bytes + 1;
	msg->flags = 0;
	data[0] = reg;
	data[1] = *value;
	msg->buf = data;
	ret = i2c_transfer(wcd9xxx->client->adapter, wcd9xxx->xfer_msg, 1);
	/* Try again if the write fails */
	if (ret != 1) {
		ret = i2c_transfer(wcd9xxx->client->adapter,
						wcd9xxx->xfer_msg, 1);
		if (ret != 1) {
			pr_err("failed to write the device\n");
			return ret;
		}
	}
	pr_debug("write sucess register = %x val = %x\n", reg, data[1]);
	return 0;
}


int wcd9xxx_i2c_read_device(unsigned short reg,
				  int bytes, unsigned char *dest)
{
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	struct wcd9xxx_i2c *wcd9xxx;
	u8 i = 0;

	wcd9xxx = get_i2c_wcd9xxx_device_info(reg);
	if (wcd9xxx == NULL || wcd9xxx->client == NULL) {
		pr_err("failed to get device info\n");
		return -ENODEV;
	}
	for (i = 0; i < bytes; i++) {
		reg_addr = (u8)reg++;
		msg = &wcd9xxx->xfer_msg[0];
		msg->addr = wcd9xxx->client->addr;
		msg->len = 1;
		msg->flags = 0;
		msg->buf = &reg_addr;

		msg = &wcd9xxx->xfer_msg[1];
		msg->addr = wcd9xxx->client->addr;
		msg->len = 1;
		msg->flags = I2C_M_RD;
		msg->buf = dest++;
		ret = i2c_transfer(wcd9xxx->client->adapter,
				wcd9xxx->xfer_msg, 2);

		/* Try again if read fails first time */
		if (ret != 2) {
			ret = i2c_transfer(wcd9xxx->client->adapter,
							wcd9xxx->xfer_msg, 2);
			if (ret != 2) {
				pr_err("failed to read wcd9xxx register\n");
				return ret;
			}
		}
	}
	return 0;
}

int wcd9xxx_i2c_read(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *dest, bool interface_reg)
{
	return wcd9xxx_i2c_read_device(reg, bytes, dest);
}

int wcd9xxx_i2c_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
			 int bytes, void *src, bool interface_reg)
{
	return wcd9xxx_i2c_write_device(reg, src, bytes);
}

static int __devinit wcd9xxx_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_pdata *pdata = client->dev.platform_data;
	int val = 0;
	int ret = 0;
	static int device_id;

	if (wcd9xxx_intf == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		pr_info("tabla card is already detected in slimbus mode\n");
		return -ENODEV;
	}
	if (device_id > 0) {
		wcd9xxx_modules[device_id++].client = client;
		pr_info("probe for other slaves devices of tabla\n");
		return ret;
	}

	wcd9xxx = kzalloc(sizeof(struct wcd9xxx), GFP_KERNEL);
	if (wcd9xxx == NULL) {
		pr_err("%s: error, allocation failed\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	if (!pdata) {
		dev_dbg(&client->dev, "no platform data?\n");
		ret = -EINVAL;
		goto fail;
	}
	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0) {
		dev_dbg(&client->dev, "can't talk I2C?\n");
		ret = -EIO;
		goto fail;
	}
	dev_set_drvdata(&client->dev, wcd9xxx);
	wcd9xxx->dev = &client->dev;
	wcd9xxx->reset_gpio = pdata->reset_gpio;
	ret = wcd9xxx_enable_supplies(wcd9xxx, pdata);
	if (ret) {
		pr_err("%s: Fail to enable Codec supplies\n", __func__);
		goto err_codec;
	}

	usleep_range(5, 5);
	ret = wcd9xxx_reset(wcd9xxx);
	if (ret) {
		pr_err("%s: Resetting Codec failed\n", __func__);
		goto err_supplies;
	}
	wcd9xxx_modules[device_id++].client = client;

	wcd9xxx->read_dev = wcd9xxx_i2c_read;
	wcd9xxx->write_dev = wcd9xxx_i2c_write;
	wcd9xxx->irq = pdata->irq;
	wcd9xxx->irq_base = pdata->irq_base;

	/*read the tabla status before initializing the device type*/
	ret = wcd9xxx_read(wcd9xxx, WCD9XXX_A_CHIP_STATUS, 1, &val, 0);
	if ((ret < 0) || (val != WCD9XXX_I2C_MODE)) {
		pr_err("failed to read the wcd9xxx status\n");
		goto err_device_init;
	}

	ret = wcd9xxx_device_init(wcd9xxx, wcd9xxx->irq);
	if (ret) {
		pr_err("%s: error, initializing device failed\n", __func__);
		goto err_device_init;
	}
	wcd9xxx_intf = WCD9XXX_INTERFACE_TYPE_I2C;

	return ret;
err_device_init:
	wcd9xxx_free_reset(wcd9xxx);
err_supplies:
	wcd9xxx_disable_supplies(wcd9xxx, pdata);
err_codec:
	kfree(wcd9xxx);
fail:
	return ret;
}

static int __devexit wcd9xxx_i2c_remove(struct i2c_client *client)
{
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_pdata *pdata = client->dev.platform_data;
	pr_debug("exit\n");
	wcd9xxx = dev_get_drvdata(&client->dev);
	wcd9xxx_disable_supplies(wcd9xxx, pdata);
	wcd9xxx_device_exit(wcd9xxx);
	return 0;
}

static int wcd9xxx_slim_probe(struct slim_device *slim)
{
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_pdata *pdata;
	int ret = 0;
	int sgla_retry_cnt;

	dev_info(&slim->dev, "Initialized slim device %s\n", slim->name);
	pdata = slim->dev.platform_data;

	if (!pdata) {
		dev_err(&slim->dev, "Error, no platform data\n");
		ret = -EINVAL;
		goto err;
	}

	wcd9xxx = kzalloc(sizeof(struct wcd9xxx), GFP_KERNEL);
	if (wcd9xxx == NULL) {
		pr_err("%s: error, allocation failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	if (!slim->ctrl) {
		pr_err("Error, no SLIMBUS control data\n");
		ret = -EINVAL;
		goto err_codec;
	}
	wcd9xxx->slim = slim;
	slim_set_clientdata(slim, wcd9xxx);
	wcd9xxx->reset_gpio = pdata->reset_gpio;
	wcd9xxx->dev = &slim->dev;

	ret = wcd9xxx_enable_supplies(wcd9xxx, pdata);
	if (ret)
		goto err_codec;
	usleep_range(5, 5);

	ret = wcd9xxx_reset(wcd9xxx);
	if (ret) {
		pr_err("%s: Resetting Codec failed\n", __func__);
		goto err_supplies;
	}

	ret = slim_get_logical_addr(wcd9xxx->slim, wcd9xxx->slim->e_addr,
		ARRAY_SIZE(wcd9xxx->slim->e_addr), &wcd9xxx->slim->laddr);
	if (ret) {
		pr_err("fail to get slimbus logical address %d\n", ret);
		goto err_reset;
	}
	wcd9xxx->read_dev = wcd9xxx_slim_read_device;
	wcd9xxx->write_dev = wcd9xxx_slim_write_device;
	wcd9xxx->irq = pdata->irq;
	wcd9xxx->irq_base = pdata->irq_base;
	wcd9xxx_pgd_la = wcd9xxx->slim->laddr;

	if (pdata->num_irqs < TABLA_NUM_IRQS) {
		pr_err("%s: Error, not enough interrupt lines allocated\n",
			__func__);
		goto err_reset;
	}

	wcd9xxx->slim_slave = &pdata->slimbus_slave_device;

	ret = slim_add_device(slim->ctrl, wcd9xxx->slim_slave);
	if (ret) {
		pr_err("%s: error, adding SLIMBUS device failed\n", __func__);
		goto err_reset;
	}

	sgla_retry_cnt = 0;

	while (1) {
		ret = slim_get_logical_addr(wcd9xxx->slim_slave,
			wcd9xxx->slim_slave->e_addr,
			ARRAY_SIZE(wcd9xxx->slim_slave->e_addr),
			&wcd9xxx->slim_slave->laddr);
		if (ret) {
			if (sgla_retry_cnt++ < WCD9XXX_SLIM_GLA_MAX_RETRIES) {
				/* Give SLIMBUS slave time to report present
				   and be ready.
				 */
				usleep_range(1000, 1000);
				pr_debug("%s: retry slim_get_logical_addr()\n",
					__func__);
				continue;
			}
			pr_err("fail to get slimbus slave logical address"
				" %d\n", ret);
			goto err_slim_add;
		}
		break;
	}
	wcd9xxx_inf_la = wcd9xxx->slim_slave->laddr;
	wcd9xxx_intf = WCD9XXX_INTERFACE_TYPE_SLIMBUS;

	ret = wcd9xxx_device_init(wcd9xxx, wcd9xxx->irq);
	if (ret) {
		pr_err("%s: error, initializing device failed\n", __func__);
		goto err_slim_add;
	}
	wcd9xxx_init_slimslave(wcd9xxx, wcd9xxx_pgd_la);
#ifdef CONFIG_DEBUG_FS
	debugCodec = wcd9xxx;

	debugfs_wcd9xxx_dent = debugfs_create_dir
		("wcd9310_slimbus_interface_device", 0);
	if (!IS_ERR(debugfs_wcd9xxx_dent)) {
		debugfs_peek = debugfs_create_file("peek",
		S_IFREG | S_IRUGO, debugfs_wcd9xxx_dent,
		(void *) "peek", &codec_debug_ops);

		debugfs_poke = debugfs_create_file("poke",
		S_IFREG | S_IRUGO, debugfs_wcd9xxx_dent,
		(void *) "poke", &codec_debug_ops);
	}
#endif

	return ret;

err_slim_add:
	slim_remove_device(wcd9xxx->slim_slave);
err_reset:
	wcd9xxx_free_reset(wcd9xxx);
err_supplies:
	wcd9xxx_disable_supplies(wcd9xxx, pdata);
err_codec:
	kfree(wcd9xxx);
err:
	return ret;
}
static int wcd9xxx_slim_remove(struct slim_device *pdev)
{
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_pdata *pdata = pdev->dev.platform_data;

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_peek);
	debugfs_remove(debugfs_poke);
	debugfs_remove(debugfs_wcd9xxx_dent);
#endif
	wcd9xxx = slim_get_devicedata(pdev);
	wcd9xxx_deinit_slimslave(wcd9xxx);
	slim_remove_device(wcd9xxx->slim_slave);
	wcd9xxx_disable_supplies(wcd9xxx, pdata);
	wcd9xxx_device_exit(wcd9xxx);
	return 0;
}

static int wcd9xxx_resume(struct wcd9xxx *wcd9xxx)
{
	int ret = 0;

	pr_debug("%s: enter\n", __func__);
	mutex_lock(&wcd9xxx->pm_lock);
	if (wcd9xxx->pm_state == WCD9XXX_PM_ASLEEP) {
		pr_debug("%s: resuming system, state %d, wlock %d\n", __func__,
			 wcd9xxx->pm_state, wcd9xxx->wlock_holders);
		wcd9xxx->pm_state = WCD9XXX_PM_SLEEPABLE;
	} else {
		pr_warn("%s: system is already awake, state %d wlock %d\n",
			__func__, wcd9xxx->pm_state, wcd9xxx->wlock_holders);
	}
	mutex_unlock(&wcd9xxx->pm_lock);
	wake_up_all(&wcd9xxx->pm_wq);

	return ret;
}

static int wcd9xxx_slim_resume(struct slim_device *sldev)
{
	struct wcd9xxx *wcd9xxx = slim_get_devicedata(sldev);
	return wcd9xxx_resume(wcd9xxx);
}

static int wcd9xxx_i2c_resume(struct i2c_client *i2cdev)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(&i2cdev->dev);
	if (wcd9xxx)
		return wcd9xxx_resume(wcd9xxx);
	else
		return 0;
}

static int wcd9xxx_suspend(struct wcd9xxx *wcd9xxx, pm_message_t pmesg)
{
	int ret = 0;

	pr_debug("%s: enter\n", __func__);
	/* wake_lock() can be called after this suspend chain call started.
	 * thus suspend can be called while wlock is being held */
	mutex_lock(&wcd9xxx->pm_lock);
	if (wcd9xxx->pm_state == WCD9XXX_PM_SLEEPABLE) {
		pr_debug("%s: suspending system, state %d, wlock %d\n",
			 __func__, wcd9xxx->pm_state, wcd9xxx->wlock_holders);
		wcd9xxx->pm_state = WCD9XXX_PM_ASLEEP;
	} else if (wcd9xxx->pm_state == WCD9XXX_PM_AWAKE) {
		/* unlock to wait for pm_state == WCD9XXX_PM_SLEEPABLE
		 * then set to WCD9XXX_PM_ASLEEP */
		pr_debug("%s: waiting to suspend system, state %d, wlock %d\n",
			 __func__, wcd9xxx->pm_state, wcd9xxx->wlock_holders);
		mutex_unlock(&wcd9xxx->pm_lock);
		if (!(wait_event_timeout(wcd9xxx->pm_wq,
					 wcd9xxx_pm_cmpxchg(wcd9xxx,
						  WCD9XXX_PM_SLEEPABLE,
						  WCD9XXX_PM_ASLEEP) ==
							WCD9XXX_PM_SLEEPABLE,
					 HZ))) {
			pr_debug("%s: suspend failed state %d, wlock %d\n",
				 __func__, wcd9xxx->pm_state,
				 wcd9xxx->wlock_holders);
			ret = -EBUSY;
		} else {
			pr_debug("%s: done, state %d, wlock %d\n", __func__,
				 wcd9xxx->pm_state, wcd9xxx->wlock_holders);
		}
		mutex_lock(&wcd9xxx->pm_lock);
	} else if (wcd9xxx->pm_state == WCD9XXX_PM_ASLEEP) {
		pr_warn("%s: system is already suspended, state %d, wlock %dn",
			__func__, wcd9xxx->pm_state, wcd9xxx->wlock_holders);
	}
	mutex_unlock(&wcd9xxx->pm_lock);

	return ret;
}

static int wcd9xxx_slim_suspend(struct slim_device *sldev, pm_message_t pmesg)
{
	struct wcd9xxx *wcd9xxx = slim_get_devicedata(sldev);
	return wcd9xxx_suspend(wcd9xxx, pmesg);
}

static int wcd9xxx_i2c_suspend(struct i2c_client *i2cdev, pm_message_t pmesg)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(&i2cdev->dev);
	if (wcd9xxx)
		return wcd9xxx_suspend(wcd9xxx, pmesg);
	else
		return 0;
}

static const struct slim_device_id sitar_slimtest_id[] = {
	{"sitar-slim", 0},
	{}
};
static struct slim_driver sitar_slim_driver = {
	.driver = {
		.name = "sitar-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = sitar_slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
};

static const struct slim_device_id sitar1p1_slimtest_id[] = {
	{"sitar1p1-slim", 0},
	{}
};
static struct slim_driver sitar1p1_slim_driver = {
	.driver = {
		.name = "sitar1p1-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = sitar1p1_slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
};

static const struct slim_device_id slimtest_id[] = {
	{"tabla-slim", 0},
	{}
};

static struct slim_driver tabla_slim_driver = {
	.driver = {
		.name = "tabla-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
};

static const struct slim_device_id slimtest2x_id[] = {
	{"tabla2x-slim", 0},
	{}
};

static struct slim_driver tabla2x_slim_driver = {
	.driver = {
		.name = "tabla2x-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = slimtest2x_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
};

#define TABLA_I2C_TOP_LEVEL 0
#define TABLA_I2C_ANALOG       1
#define TABLA_I2C_DIGITAL_1    2
#define TABLA_I2C_DIGITAL_2    3

static struct i2c_device_id tabla_id_table[] = {
	{"tabla top level", TABLA_I2C_TOP_LEVEL},
	{"tabla analog", TABLA_I2C_TOP_LEVEL},
	{"tabla digital1", TABLA_I2C_TOP_LEVEL},
	{"tabla digital2", TABLA_I2C_TOP_LEVEL},
	{}
};
MODULE_DEVICE_TABLE(i2c, tabla_id_table);

static struct i2c_driver tabla_i2c_driver = {
	.driver                 = {
		.owner          =       THIS_MODULE,
		.name           =       "tabla-i2c-core",
	},
	.id_table               =       tabla_id_table,
	.probe                  =       wcd9xxx_i2c_probe,
	.remove                 =       __devexit_p(wcd9xxx_i2c_remove),
	.resume	= wcd9xxx_i2c_resume,
	.suspend = wcd9xxx_i2c_suspend,
};

static int __init wcd9xxx_init(void)
{
	int ret1, ret2, ret3, ret4, ret5;

	ret1 = slim_driver_register(&tabla_slim_driver);
	if (ret1 != 0)
		pr_err("Failed to register tabla SB driver: %d\n", ret1);

	ret2 = slim_driver_register(&tabla2x_slim_driver);
	if (ret2 != 0)
		pr_err("Failed to register tabla2x SB driver: %d\n", ret2);

	ret3 = i2c_add_driver(&tabla_i2c_driver);
	if (ret3 != 0)
		pr_err("failed to add the I2C driver\n");

	ret4 = slim_driver_register(&sitar_slim_driver);
	if (ret4 != 0)
		pr_err("Failed to register sitar SB driver: %d\n", ret4);

	ret5 = slim_driver_register(&sitar1p1_slim_driver);
	if (ret5 != 0)
		pr_err("Failed to register sitar SB driver: %d\n", ret5);

	return (ret1 && ret2 && ret3 && ret4 && ret5) ? -1 : 0;
}
module_init(wcd9xxx_init);

static void __exit wcd9xxx_exit(void)
{
}
module_exit(wcd9xxx_exit);

MODULE_DESCRIPTION("Codec core driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
