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
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/wcd9310/core.h>
#include <linux/mfd/wcd9310/pdata.h>
#include <linux/mfd/wcd9310/registers.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <sound/soc.h>

#define TABLA_SLIM_GLA_MAX_RETRIES 5
#define TABLA_REGISTER_START_OFFSET 0x800
#define TABLA_SLIM_RW_MAX_TRIES 3

#define MAX_TABLA_DEVICE	4
#define TABLA_I2C_MODE	0x03

struct tabla_i2c {
	struct i2c_client *client;
	struct i2c_msg xfer_msg[2];
	struct mutex xfer_lock;
	int mod_id;
};

struct tabla_i2c tabla_modules[MAX_TABLA_DEVICE];
static int tabla_intf;

static int tabla_read(struct tabla *tabla, unsigned short reg,
		       int bytes, void *dest, bool interface_reg)
{
	int ret;
	u8 *buf = dest;

	if (bytes <= 0) {
		dev_err(tabla->dev, "Invalid byte read length %d\n", bytes);
		return -EINVAL;
	}

	ret = tabla->read_dev(tabla, reg, bytes, dest, interface_reg);
	if (ret < 0) {
		dev_err(tabla->dev, "Tabla read failed\n");
		return ret;
	} else
		dev_dbg(tabla->dev, "Read 0x%02x from R%d(0x%x)\n",
			 *buf, reg, reg);

	return 0;
}
int tabla_reg_read(struct tabla *tabla, unsigned short reg)
{
	u8 val;
	int ret;

	mutex_lock(&tabla->io_lock);
	ret = tabla_read(tabla, reg, 1, &val, false);
	mutex_unlock(&tabla->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(tabla_reg_read);

static int tabla_write(struct tabla *tabla, unsigned short reg,
			int bytes, void *src, bool interface_reg)
{
	u8 *buf = src;

	if (bytes <= 0) {
		pr_err("%s: Error, invalid write length\n", __func__);
		return -EINVAL;
	}

	dev_dbg(tabla->dev, "Write %02x to R%d(0x%x)\n",
		 *buf, reg, reg);

	return tabla->write_dev(tabla, reg, bytes, src, interface_reg);
}

int tabla_reg_write(struct tabla *tabla, unsigned short reg,
		     u8 val)
{
	int ret;

	mutex_lock(&tabla->io_lock);
	ret = tabla_write(tabla, reg, 1, &val, false);
	mutex_unlock(&tabla->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(tabla_reg_write);

static u8 tabla_pgd_la;
static u8 tabla_inf_la;

int tabla_get_logical_addresses(u8 *pgd_la, u8 *inf_la)
{
	*pgd_la = tabla_pgd_la;
	*inf_la = tabla_inf_la;
	return 0;

}
EXPORT_SYMBOL_GPL(tabla_get_logical_addresses);

int tabla_interface_reg_read(struct tabla *tabla, unsigned short reg)
{
	u8 val;
	int ret;

	mutex_lock(&tabla->io_lock);
	ret = tabla_read(tabla, reg, 1, &val, true);
	mutex_unlock(&tabla->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(tabla_interface_reg_read);

int tabla_interface_reg_write(struct tabla *tabla, unsigned short reg,
		     u8 val)
{
	int ret;

	mutex_lock(&tabla->io_lock);
	ret = tabla_write(tabla, reg, 1, &val, true);
	mutex_unlock(&tabla->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(tabla_interface_reg_write);

int tabla_bulk_read(struct tabla *tabla, unsigned short reg,
		     int count, u8 *buf)
{
	int ret;

	mutex_lock(&tabla->io_lock);

	ret = tabla_read(tabla, reg, count, buf, false);

	mutex_unlock(&tabla->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(tabla_bulk_read);

int tabla_bulk_write(struct tabla *tabla, unsigned short reg,
		     int count, u8 *buf)
{
	int ret;

	mutex_lock(&tabla->io_lock);

	ret = tabla_write(tabla, reg, count, buf, false);

	mutex_unlock(&tabla->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(tabla_bulk_write);

static int tabla_slim_read_device(struct tabla *tabla, unsigned short reg,
				int bytes, void *dest, bool interface)
{
	int ret;
	struct slim_ele_access msg;
	int slim_read_tries = TABLA_SLIM_RW_MAX_TRIES;
	msg.start_offset = TABLA_REGISTER_START_OFFSET + reg;
	msg.num_bytes = bytes;
	msg.comp = NULL;

	while (1) {
		mutex_lock(&tabla->xfer_lock);
		ret = slim_request_val_element(interface ?
					       tabla->slim_slave : tabla->slim,
					       &msg, dest, bytes);
		mutex_unlock(&tabla->xfer_lock);
		if (likely(ret == 0) || (--slim_read_tries == 0))
			break;
		usleep_range(5000, 5000);
	}

	if (ret)
		pr_err("%s: Error, Tabla read failed (%d)\n", __func__, ret);

	return ret;
}
/* Interface specifies whether the write is to the interface or general
 * registers.
 */
static int tabla_slim_write_device(struct tabla *tabla, unsigned short reg,
				   int bytes, void *src, bool interface)
{
	int ret;
	struct slim_ele_access msg;
	int slim_write_tries = TABLA_SLIM_RW_MAX_TRIES;
	msg.start_offset = TABLA_REGISTER_START_OFFSET + reg;
	msg.num_bytes = bytes;
	msg.comp = NULL;

	while (1) {
		mutex_lock(&tabla->xfer_lock);
		ret = slim_change_val_element(interface ?
					      tabla->slim_slave : tabla->slim,
					      &msg, src, bytes);
		mutex_unlock(&tabla->xfer_lock);
		if (likely(ret == 0) || (--slim_write_tries == 0))
			break;
		usleep_range(5000, 5000);
	}

	if (ret)
		pr_err("%s: Error, Tabla write failed (%d)\n", __func__, ret);

	return ret;
}

static struct mfd_cell tabla_devs[] = {
	{
		.name = "tabla_codec",
	},
};

static void tabla_bring_up(struct tabla *tabla)
{
	tabla_reg_write(tabla, TABLA_A_LEAKAGE_CTL, 0x4);
	tabla_reg_write(tabla, TABLA_A_CDC_CTL, 0);
	usleep_range(5000, 5000);
	tabla_reg_write(tabla, TABLA_A_CDC_CTL, 3);
	tabla_reg_write(tabla, TABLA_A_LEAKAGE_CTL, 3);
}

static void tabla_bring_down(struct tabla *tabla)
{
	tabla_reg_write(tabla, TABLA_A_LEAKAGE_CTL, 0x7);
	tabla_reg_write(tabla, TABLA_A_LEAKAGE_CTL, 0x6);
	tabla_reg_write(tabla, TABLA_A_LEAKAGE_CTL, 0xe);
	tabla_reg_write(tabla, TABLA_A_LEAKAGE_CTL, 0x8);
}

static int tabla_reset(struct tabla *tabla)
{
	int ret;
	struct pm_gpio param = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull	   = PM_GPIO_PULL_NO,
		.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	if (tabla->reset_gpio) {
		ret = gpio_request(tabla->reset_gpio, "CDC_RESET");
		if (ret) {
			pr_err("%s: Failed to request gpio %d\n", __func__,
				tabla->reset_gpio);
			tabla->reset_gpio = 0;
			return ret;
		}

		ret = pm8xxx_gpio_config(tabla->reset_gpio, &param);
		if (ret)
			pr_err("%s: Failed to configure gpio\n", __func__);

		gpio_direction_output(tabla->reset_gpio, 1);
		msleep(20);
		gpio_direction_output(tabla->reset_gpio, 0);
		msleep(20);
		gpio_direction_output(tabla->reset_gpio, 1);
		msleep(20);
	}
	return 0;
}

static void tabla_free_reset(struct tabla *tabla)
{
	if (tabla->reset_gpio) {
		gpio_free(tabla->reset_gpio);
		tabla->reset_gpio = 0;
	}
}

struct tabla_regulator {
	const char *name;
	int min_uV;
	int max_uV;
	int optimum_uA;
	struct regulator *regulator;
};


/*
 *	format : TABLA_<POWER_SUPPLY_PIN_NAME>_CUR_MAX
 *
 *	<POWER_SUPPLY_PIN_NAME> from Tabla objective spec
*/

#define  TABLA_CDC_VDDA_CP_CUR_MAX	500000
#define  TABLA_CDC_VDDA_RX_CUR_MAX	20000
#define  TABLA_CDC_VDDA_TX_CUR_MAX	20000
#define  TABLA_VDDIO_CDC_CUR_MAX	5000

#define  TABLA_VDDD_CDC_D_CUR_MAX	5000
#define  TABLA_VDDD_CDC_A_CUR_MAX	5000

static struct tabla_regulator tabla_regulators[] = {
	{
		.name = "CDC_VDD_CP",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = TABLA_CDC_VDDA_CP_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_RX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = TABLA_CDC_VDDA_RX_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_TX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = TABLA_CDC_VDDA_TX_CUR_MAX,
	},
	{
		.name = "VDDIO_CDC",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = TABLA_VDDIO_CDC_CUR_MAX,
	},
	{
		.name = "VDDD_CDC_D",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = TABLA_VDDD_CDC_D_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_A_1P2V",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = TABLA_VDDD_CDC_A_CUR_MAX,
	},
};

static int tabla_device_init(struct tabla *tabla, int irq)
{
	int ret;

	mutex_init(&tabla->io_lock);
	mutex_init(&tabla->xfer_lock);

	mutex_init(&tabla->pm_lock);
	tabla->wlock_holders = 0;
	tabla->pm_state = TABLA_PM_SLEEPABLE;
	init_waitqueue_head(&tabla->pm_wq);
	wake_lock_init(&tabla->wlock, WAKE_LOCK_IDLE, "wcd9310-irq");

	dev_set_drvdata(tabla->dev, tabla);

	tabla_bring_up(tabla);

	ret = tabla_irq_init(tabla);
	if (ret) {
		pr_err("IRQ initialization failed\n");
		goto err;
	}

	ret = mfd_add_devices(tabla->dev, -1,
			      tabla_devs, ARRAY_SIZE(tabla_devs),
			      NULL, 0);
	if (ret != 0) {
		dev_err(tabla->dev, "Failed to add children: %d\n", ret);
		goto err_irq;
	}

	tabla->version = tabla_reg_read(tabla, TABLA_A_CHIP_VERSION) & 0x1F;
	pr_info("%s : Tabla version %u initialized\n",
		__func__, tabla->version);

	return ret;
err_irq:
	tabla_irq_exit(tabla);
err:
	tabla_bring_down(tabla);
	wake_lock_destroy(&tabla->wlock);
	mutex_destroy(&tabla->pm_lock);
	mutex_destroy(&tabla->io_lock);
	mutex_destroy(&tabla->xfer_lock);
	return ret;
}

static void tabla_device_exit(struct tabla *tabla)
{
	tabla_irq_exit(tabla);
	tabla_bring_down(tabla);
	tabla_free_reset(tabla);
	mutex_destroy(&tabla->pm_lock);
	wake_lock_destroy(&tabla->wlock);
	mutex_destroy(&tabla->io_lock);
	mutex_destroy(&tabla->xfer_lock);
}


#ifdef CONFIG_DEBUG_FS
struct tabla *debugTabla;

static struct dentry *debugfs_tabla_dent;
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
			tabla_interface_reg_write(debugTabla, param[0],
				param[1]);
		else
			rc = -EINVAL;
	} else if (!strncmp(access_str, "peek", 6)) {
		/* read */
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0x3FF) && (rc == 0))
			read_data = tabla_interface_reg_read(debugTabla,
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

static int tabla_enable_supplies(struct tabla *tabla)
{
	int ret;
	int i;

	tabla->supplies = kzalloc(sizeof(struct regulator_bulk_data) *
				   ARRAY_SIZE(tabla_regulators),
				   GFP_KERNEL);
	if (!tabla->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(tabla_regulators); i++)
		tabla->supplies[i].supply = tabla_regulators[i].name;

	ret = regulator_bulk_get(tabla->dev, ARRAY_SIZE(tabla_regulators),
				 tabla->supplies);
	if (ret != 0) {
		dev_err(tabla->dev, "Failed to get supplies: err = %d\n", ret);
		goto err_supplies;
	}

	for (i = 0; i < ARRAY_SIZE(tabla_regulators); i++) {
		ret = regulator_set_voltage(tabla->supplies[i].consumer,
			tabla_regulators[i].min_uV, tabla_regulators[i].max_uV);
		if (ret) {
			pr_err("%s: Setting regulator voltage failed for "
				"regulator %s err = %d\n", __func__,
				tabla->supplies[i].supply, ret);
			goto err_get;
		}

		ret = regulator_set_optimum_mode(tabla->supplies[i].consumer,
			tabla_regulators[i].optimum_uA);
		if (ret < 0) {
			pr_err("%s: Setting regulator optimum mode failed for "
				"regulator %s err = %d\n", __func__,
				tabla->supplies[i].supply, ret);
			goto err_get;
		}
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(tabla_regulators),
				    tabla->supplies);
	if (ret != 0) {
		dev_err(tabla->dev, "Failed to enable supplies: err = %d\n",
				ret);
		goto err_configure;
	}
	return ret;

err_configure:
	for (i = 0; i < ARRAY_SIZE(tabla_regulators); i++) {
		regulator_set_voltage(tabla->supplies[i].consumer, 0,
			tabla_regulators[i].max_uV);
		regulator_set_optimum_mode(tabla->supplies[i].consumer, 0);
	}
err_get:
	regulator_bulk_free(ARRAY_SIZE(tabla_regulators), tabla->supplies);
err_supplies:
	kfree(tabla->supplies);
err:
	return ret;
}

static void tabla_disable_supplies(struct tabla *tabla)
{
	int i;

	regulator_bulk_disable(ARRAY_SIZE(tabla_regulators),
				    tabla->supplies);
	for (i = 0; i < ARRAY_SIZE(tabla_regulators); i++) {
		regulator_set_voltage(tabla->supplies[i].consumer, 0,
			tabla_regulators[i].max_uV);
		regulator_set_optimum_mode(tabla->supplies[i].consumer, 0);
	}
	regulator_bulk_free(ARRAY_SIZE(tabla_regulators), tabla->supplies);
	kfree(tabla->supplies);
}

int tabla_get_intf_type(void)
{
	return tabla_intf;
}
EXPORT_SYMBOL_GPL(tabla_get_intf_type);

struct tabla_i2c *get_i2c_tabla_device_info(u16 reg)
{
	u16 mask = 0x0f00;
	int value = 0;
	struct tabla_i2c *tabla = NULL;
	value = ((reg & mask) >> 8) & 0x000f;
	switch (value) {
	case 0:
		tabla = &tabla_modules[0];
		break;
	case 1:
		tabla = &tabla_modules[1];
		break;
	case 2:
		tabla = &tabla_modules[2];
		break;
	case 3:
		tabla = &tabla_modules[3];
		break;
	default:
		break;
	}
	return tabla;
}

int tabla_i2c_write_device(u16 reg, u8 *value,
				u32 bytes)
{

	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	u8 data[bytes + 1];
	struct tabla_i2c *tabla;

	tabla = get_i2c_tabla_device_info(reg);
	if (tabla->client == NULL) {
		pr_err("failed to get device info\n");
		return -ENODEV;
	}
	reg_addr = (u8)reg;
	msg = &tabla->xfer_msg[0];
	msg->addr = tabla->client->addr;
	msg->len = bytes + 1;
	msg->flags = 0;
	data[0] = reg;
	data[1] = *value;
	msg->buf = data;
	ret = i2c_transfer(tabla->client->adapter, tabla->xfer_msg, 1);
	/* Try again if the write fails */
	if (ret != 1) {
		ret = i2c_transfer(tabla->client->adapter,
						tabla->xfer_msg, 1);
		if (ret != 1) {
			pr_err("failed to write the device\n");
			return ret;
		}
	}
	pr_debug("write sucess register = %x val = %x\n", reg, data[1]);
	return 0;
}


int tabla_i2c_read_device(unsigned short reg,
				  int bytes, unsigned char *dest)
{
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	struct tabla_i2c *tabla;
	u8 i = 0;

	tabla = get_i2c_tabla_device_info(reg);
	if (tabla->client == NULL) {
		pr_err("failed to get device info\n");
		return -ENODEV;
	}
	for (i = 0; i < bytes; i++) {
		reg_addr = (u8)reg++;
		msg = &tabla->xfer_msg[0];
		msg->addr = tabla->client->addr;
		msg->len = 1;
		msg->flags = 0;
		msg->buf = &reg_addr;

		msg = &tabla->xfer_msg[1];
		msg->addr = tabla->client->addr;
		msg->len = 1;
		msg->flags = I2C_M_RD;
		msg->buf = dest++;
		ret = i2c_transfer(tabla->client->adapter, tabla->xfer_msg, 2);

		/* Try again if read fails first time */
		if (ret != 2) {
			ret = i2c_transfer(tabla->client->adapter,
							tabla->xfer_msg, 2);
			if (ret != 2) {
				pr_err("failed to read tabla register\n");
				return ret;
			}
		}
	}
	return 0;
}

int tabla_i2c_read(struct tabla *tabla, unsigned short reg,
		   int bytes, void *dest, bool interface_reg)
{
	return tabla_i2c_read_device(reg, bytes, dest);
}

int tabla_i2c_write(struct tabla *tabla, unsigned short reg,
		    int bytes, void *src, bool interface_reg)
{
	return tabla_i2c_write_device(reg, src, bytes);
}

static int __devinit tabla_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tabla *tabla;
	struct tabla_pdata *pdata = client->dev.platform_data;
	int val = 0;
	int ret = 0;
	static int device_id;

	if (device_id > 0) {
		tabla_modules[device_id++].client = client;
		pr_info("probe for other slaves devices of tabla\n");
		return ret;
	}

	tabla = kzalloc(sizeof(struct tabla), GFP_KERNEL);
	if (tabla == NULL) {
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
	tabla->dev = &client->dev;
	tabla->reset_gpio = pdata->reset_gpio;

	ret = tabla_enable_supplies(tabla);
	if (ret) {
		pr_err("%s: Fail to enable Tabla supplies\n", __func__);
		goto err_tabla;
	}

	usleep_range(5, 5);
	ret = tabla_reset(tabla);
	if (ret) {
		pr_err("%s: Resetting Tabla failed\n", __func__);
		goto err_supplies;
	}
	tabla_modules[device_id++].client = client;

	tabla->read_dev = tabla_i2c_read;
	tabla->write_dev = tabla_i2c_write;
	tabla->irq = pdata->irq;
	tabla->irq_base = pdata->irq_base;

	/*read the tabla status before initializing the device type*/
	ret = tabla_read(tabla, TABLA_A_CHIP_STATUS, 1, &val, 0);
	if ((ret < 0) || (val != TABLA_I2C_MODE)) {
		pr_err("failed to read the tabla status\n");
		goto err_device_init;
	}

	ret = tabla_device_init(tabla, tabla->irq);
	if (ret) {
		pr_err("%s: error, initializing device failed\n", __func__);
		goto err_device_init;
	}
	tabla_intf = TABLA_INTERFACE_TYPE_I2C;

	return ret;
err_device_init:
	tabla_free_reset(tabla);
err_supplies:
	tabla_disable_supplies(tabla);
err_tabla:
	kfree(tabla);
fail:
	return ret;
}

static int __devexit tabla_i2c_remove(struct i2c_client *client)
{
	struct tabla *tabla;

	pr_debug("exit\n");
	tabla = dev_get_drvdata(&client->dev);
	tabla_device_exit(tabla);
	tabla_disable_supplies(tabla);
	kfree(tabla);
	return 0;
}

static int tabla_slim_probe(struct slim_device *slim)
{
	struct tabla *tabla;
	struct tabla_pdata *pdata;
	int ret = 0;
	int sgla_retry_cnt;

	dev_info(&slim->dev, "Initialized slim device %s\n", slim->name);
	pdata = slim->dev.platform_data;

	if (!pdata) {
		dev_err(&slim->dev, "Error, no platform data\n");
		ret = -EINVAL;
		goto err;
	}

	tabla = kzalloc(sizeof(struct tabla), GFP_KERNEL);
	if (tabla == NULL) {
		pr_err("%s: error, allocation failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	if (!slim->ctrl) {
		pr_err("Error, no SLIMBUS control data\n");
		ret = -EINVAL;
		goto err_tabla;
	}
	tabla->slim = slim;
	slim_set_clientdata(slim, tabla);
	tabla->reset_gpio = pdata->reset_gpio;
	tabla->dev = &slim->dev;

	ret = tabla_enable_supplies(tabla);
	if (ret) {
		pr_err("%s: Fail to enable Tabla supplies\n", __func__);
		goto err_tabla;
	}
	usleep_range(5, 5);

	ret = tabla_reset(tabla);
	if (ret) {
		pr_err("%s: Resetting Tabla failed\n", __func__);
		goto err_supplies;
	}

	ret = slim_get_logical_addr(tabla->slim, tabla->slim->e_addr,
		ARRAY_SIZE(tabla->slim->e_addr), &tabla->slim->laddr);
	if (ret) {
		pr_err("fail to get slimbus logical address %d\n", ret);
		goto err_reset;
	}
	tabla->read_dev = tabla_slim_read_device;
	tabla->write_dev = tabla_slim_write_device;
	tabla->irq = pdata->irq;
	tabla->irq_base = pdata->irq_base;
	tabla_pgd_la = tabla->slim->laddr;

	if (pdata->num_irqs < TABLA_NUM_IRQS) {
		pr_err("%s: Error, not enough interrupt lines allocated\n",
			__func__);
		goto err_reset;
	}

	tabla->slim_slave = &pdata->slimbus_slave_device;

	ret = slim_add_device(slim->ctrl, tabla->slim_slave);
	if (ret) {
		pr_err("%s: error, adding SLIMBUS device failed\n", __func__);
		goto err_reset;
	}

	sgla_retry_cnt = 0;

	while (1) {
		ret = slim_get_logical_addr(tabla->slim_slave,
			tabla->slim_slave->e_addr,
			ARRAY_SIZE(tabla->slim_slave->e_addr),
			&tabla->slim_slave->laddr);
		if (ret) {
			if (sgla_retry_cnt++ < TABLA_SLIM_GLA_MAX_RETRIES) {
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
	tabla_inf_la = tabla->slim_slave->laddr;
	tabla_intf = TABLA_INTERFACE_TYPE_SLIMBUS;

	ret = tabla_device_init(tabla, tabla->irq);
	if (ret) {
		pr_err("%s: error, initializing device failed\n", __func__);
		goto err_slim_add;
	}

#ifdef CONFIG_DEBUG_FS
	debugTabla = tabla;

	debugfs_tabla_dent = debugfs_create_dir
		("wcd9310_slimbus_interface_device", 0);
	if (!IS_ERR(debugfs_tabla_dent)) {
		debugfs_peek = debugfs_create_file("peek",
		S_IFREG | S_IRUGO, debugfs_tabla_dent,
		(void *) "peek", &codec_debug_ops);

		debugfs_poke = debugfs_create_file("poke",
		S_IFREG | S_IRUGO, debugfs_tabla_dent,
		(void *) "poke", &codec_debug_ops);
	}
#endif

	return ret;

err_slim_add:
	slim_remove_device(tabla->slim_slave);
err_reset:
	tabla_free_reset(tabla);
err_supplies:
	tabla_disable_supplies(tabla);
err_tabla:
	kfree(tabla);
err:
	return ret;
}

static int tabla_slim_remove(struct slim_device *pdev)
{
	struct tabla *tabla;

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_peek);
	debugfs_remove(debugfs_poke);
	debugfs_remove(debugfs_tabla_dent);
#endif

	tabla = slim_get_devicedata(pdev);
	tabla_device_exit(tabla);
	tabla_disable_supplies(tabla);
	slim_remove_device(tabla->slim_slave);
	kfree(tabla);

	return 0;
}

static int tabla_resume(struct tabla *tabla)
{
	int ret = 0;

	pr_debug("%s: enter\n", __func__);
	mutex_lock(&tabla->pm_lock);
	if (tabla->pm_state == TABLA_PM_ASLEEP) {
		pr_debug("%s: resuming system, state %d, wlock %d\n", __func__,
			 tabla->pm_state, tabla->wlock_holders);
		tabla->pm_state = TABLA_PM_SLEEPABLE;
	} else {
		pr_warn("%s: system is already awake, state %d wlock %d\n",
			__func__, tabla->pm_state, tabla->wlock_holders);
	}
	mutex_unlock(&tabla->pm_lock);
	wake_up_all(&tabla->pm_wq);

	return ret;
}

static int tabla_slim_resume(struct slim_device *sldev)
{
	struct tabla *tabla = slim_get_devicedata(sldev);
	return tabla_resume(tabla);
}

static int tabla_i2c_resume(struct i2c_client *i2cdev)
{
	struct tabla *tabla = dev_get_drvdata(&i2cdev->dev);
	return tabla_resume(tabla);
}

static int tabla_suspend(struct tabla *tabla, pm_message_t pmesg)
{
	int ret = 0;

	pr_debug("%s: enter\n", __func__);
	/* wake_lock() can be called after this suspend chain call started.
	 * thus suspend can be called while wlock is being held */
	mutex_lock(&tabla->pm_lock);
	if (tabla->pm_state == TABLA_PM_SLEEPABLE) {
		pr_debug("%s: suspending system, state %d, wlock %d\n",
			 __func__, tabla->pm_state, tabla->wlock_holders);
		tabla->pm_state = TABLA_PM_ASLEEP;
	} else if (tabla->pm_state == TABLA_PM_AWAKE) {
		/* unlock to wait for pm_state == TABLA_PM_SLEEPABLE
		 * then set to TABLA_PM_ASLEEP */
		pr_debug("%s: waiting to suspend system, state %d, wlock %d\n",
			 __func__, tabla->pm_state, tabla->wlock_holders);
		mutex_unlock(&tabla->pm_lock);
		if (!(wait_event_timeout(tabla->pm_wq,
					 tabla_pm_cmpxchg(tabla,
							  TABLA_PM_SLEEPABLE,
							  TABLA_PM_ASLEEP) ==
							     TABLA_PM_SLEEPABLE,
					 HZ))) {
			pr_debug("%s: suspend failed state %d, wlock %d\n",
				 __func__, tabla->pm_state,
				 tabla->wlock_holders);
			ret = -EBUSY;
		} else {
			pr_debug("%s: done, state %d, wlock %d\n", __func__,
				 tabla->pm_state, tabla->wlock_holders);
		}
		mutex_lock(&tabla->pm_lock);
	} else if (tabla->pm_state == TABLA_PM_ASLEEP) {
		pr_warn("%s: system is already suspended, state %d, wlock %dn",
			__func__, tabla->pm_state, tabla->wlock_holders);
	}
	mutex_unlock(&tabla->pm_lock);

	return ret;
}

static int tabla_slim_suspend(struct slim_device *sldev, pm_message_t pmesg)
{
	struct tabla *tabla = slim_get_devicedata(sldev);
	return tabla_suspend(tabla, pmesg);
}

static int tabla_i2c_suspend(struct i2c_client *i2cdev, pm_message_t pmesg)
{
	struct tabla *tabla = dev_get_drvdata(&i2cdev->dev);
	return tabla_suspend(tabla, pmesg);
}

static const struct slim_device_id slimtest_id[] = {
	{"tabla-slim", 0},
	{}
};

static struct slim_driver tabla_slim_driver = {
	.driver = {
		.name = "tabla-slim",
		.owner = THIS_MODULE,
	},
	.probe = tabla_slim_probe,
	.remove = tabla_slim_remove,
	.id_table = slimtest_id,
	.resume = tabla_slim_resume,
	.suspend = tabla_slim_suspend,
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
	.probe = tabla_slim_probe,
	.remove = tabla_slim_remove,
	.id_table = slimtest2x_id,
	.resume = tabla_slim_resume,
	.suspend = tabla_slim_suspend,
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
	.driver = {
		.owner = THIS_MODULE,
		.name = "tabla-i2c-core",
	},
	.id_table = tabla_id_table,
	.probe = tabla_i2c_probe,
	.remove = __devexit_p(tabla_i2c_remove),
	.resume	= tabla_i2c_resume,
	.suspend = tabla_i2c_suspend,
};

static int __init tabla_init(void)
{
	int ret1, ret2, ret3;

	ret1 = slim_driver_register(&tabla_slim_driver);
	if (ret1 != 0)
		pr_err("Failed to register tabla SB driver: %d\n", ret1);

	ret2 = slim_driver_register(&tabla2x_slim_driver);
	if (ret2 != 0)
		pr_err("Failed to register tabla2x SB driver: %d\n", ret2);

	ret3 = i2c_add_driver(&tabla_i2c_driver);
	if (ret3 != 0)
		pr_err("failed to add the I2C driver\n");

	return (ret1 && ret2 && ret3) ? -1 : 0;
}
module_init(tabla_init);

static void __exit tabla_exit(void)
{
}
module_exit(tabla_exit);

MODULE_DESCRIPTION("Tabla core driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
