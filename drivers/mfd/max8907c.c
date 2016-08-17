/*
 * max8907c.c - mfd driver for MAX8907c
 *
 * Copyright (C) 2010 Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max8907c.h>

static struct mfd_cell cells[] = {
	{.name = "max8907-regulator",},
	{.name = "max8907c-rtc",},
};

static int max8907c_i2c_read(struct i2c_client *i2c, u8 reg, u8 count, u8 *dest)
{
	struct i2c_msg xfer[2];
	int ret = 0;

	xfer[0].addr = i2c->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = &reg;

	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = count;
	xfer[1].buf = dest;

	ret = i2c_transfer(i2c->adapter, xfer, 2);
	if (ret < 0)
		return ret;
	if (ret != 2)
		return -EIO;

	return 0;
}

static int max8907c_i2c_write(struct i2c_client *i2c, u8 reg, u8 count, const u8 *src)
{
	u8 msg[0x100 + 1];
	int ret = 0;

	msg[0] = reg;
	memcpy(&msg[1], src, count);

	ret = i2c_master_send(i2c, msg, count + 1);
	if (ret < 0)
		return ret;
	if (ret != count + 1)
		return -EIO;

	return 0;
}

int max8907c_reg_read(struct i2c_client *i2c, u8 reg)
{
	int ret;
	u8 val;

	ret = max8907c_i2c_read(i2c, reg, 1, &val);

	pr_debug("max8907c: reg read  reg=%x, val=%x\n",
		 (unsigned int)reg, (unsigned int)val);

	if (ret != 0)
		pr_err("Failed to read max8907c I2C driver: %d\n", ret);
	return val;
}
EXPORT_SYMBOL_GPL(max8907c_reg_read);

int max8907c_reg_bulk_read(struct i2c_client *i2c, u8 reg, u8 count, u8 *val)
{
	int ret;

	ret = max8907c_i2c_read(i2c, reg, count, val);

	pr_debug("max8907c: reg read  reg=%x, val=%x\n",
		 (unsigned int)reg, (unsigned int)*val);

	if (ret != 0)
		pr_err("Failed to read max8907c I2C driver: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(max8907c_reg_bulk_read);

int max8907c_reg_write(struct i2c_client *i2c, u8 reg, u8 val)
{
	struct max8907c *max8907c = i2c_get_clientdata(i2c);
	int ret;

	pr_debug("max8907c: reg write  reg=%x, val=%x\n",
		 (unsigned int)reg, (unsigned int)val);

	mutex_lock(&max8907c->io_lock);
	ret = max8907c_i2c_write(i2c, reg, 1, &val);
	mutex_unlock(&max8907c->io_lock);

	if (ret != 0)
		pr_err("Failed to write max8907c I2C driver: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(max8907c_reg_write);

int max8907c_reg_bulk_write(struct i2c_client *i2c, u8 reg, u8 count, u8 *val)
{
	struct max8907c *max8907c = i2c_get_clientdata(i2c);
	int ret;

	pr_debug("max8907c: reg write  reg=%x, val=%x\n",
		 (unsigned int)reg, (unsigned int)*val);

	mutex_lock(&max8907c->io_lock);
	ret = max8907c_i2c_write(i2c, reg, count, val);
	mutex_unlock(&max8907c->io_lock);

	if (ret != 0)
		pr_err("Failed to write max8907c I2C driver: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(max8907c_reg_bulk_write);

int max8907c_set_bits(struct i2c_client *i2c, u8 reg, u8 mask, u8 val)
{
	struct max8907c *max8907c = i2c_get_clientdata(i2c);
	u8 tmp;
	int ret;

	pr_debug("max8907c: reg write  reg=%02X, val=%02X, mask=%02X\n",
		 (unsigned int)reg, (unsigned int)val, (unsigned int)mask);

	mutex_lock(&max8907c->io_lock);
	ret = max8907c_i2c_read(i2c, reg, 1, &tmp);
	if (ret == 0) {
		val = (tmp & ~mask) | (val & mask);
		ret = max8907c_i2c_write(i2c, reg, 1, &val);
	}
	mutex_unlock(&max8907c->io_lock);

	if (ret != 0)
		pr_err("Failed to write max8907c I2C driver: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(max8907c_set_bits);

static struct i2c_client *max8907c_client = NULL;
static void max8907c_power_off(void)
{
	if (!max8907c_client)
		return;

	max8907c_set_bits(max8907c_client, MAX8907C_REG_RESET_CNFG,
						MAX8907C_MASK_POWER_OFF, 0x40);
}

void max8907c_deep_sleep(int enter)
{
	if (!max8907c_client)
		return;

	if (enter) {
		max8907c_reg_write(max8907c_client, MAX8907C_REG_SDSEQCNT1,
						MAX8907C_POWER_UP_DELAY_CNT12);
		max8907c_reg_write(max8907c_client, MAX8907C_REG_SDSEQCNT2,
							MAX8907C_DELAY_CNT0);
		max8907c_reg_write(max8907c_client, MAX8907C_REG_SDCTL2,
							MAX8907C_SD_SEQ2);
	} else {
		max8907c_reg_write(max8907c_client, MAX8907C_REG_SDSEQCNT1,
							MAX8907C_DELAY_CNT0);
		max8907c_reg_write(max8907c_client, MAX8907C_REG_SDCTL2,
							MAX8907C_SD_SEQ1);
		max8907c_reg_write(max8907c_client, MAX8907C_REG_SDSEQCNT2,
				MAX8907C_POWER_UP_DELAY_CNT1 | MAX8907C_POWER_DOWN_DELAY_CNT12);
	}
}

static int max8907c_remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int max8907c_remove_subdevs(struct max8907c *max8907c)
{
	return device_for_each_child(max8907c->dev, NULL,
				     max8907c_remove_subdev);
}

static int max8097c_add_subdevs(struct max8907c *max8907c,
				struct max8907c_platform_data *pdata)
{
	struct platform_device *pdev;
	int ret;
	int i;

	for (i = 0; i < pdata->num_subdevs; i++) {
		pdev = platform_device_alloc(pdata->subdevs[i]->name,
					     pdata->subdevs[i]->id);

		pdev->dev.parent = max8907c->dev;
		pdev->dev.platform_data = pdata->subdevs[i]->dev.platform_data;

		ret = platform_device_add(pdev);
		if (ret)
			goto error;
	}
	return 0;

error:
	max8907c_remove_subdevs(max8907c);
	return ret;
}

int max8907c_pwr_en_config(void)
{
	int ret;
	u8 data;

	if (!max8907c_client)
		return -EINVAL;

	/*
	 * Enable/disable PWREN h/w control mechanism (PWREN signal must be
	 * inactive = high at this time)
	 */
	ret = max8907c_set_bits(max8907c_client, MAX8907C_REG_RESET_CNFG,
					MAX8907C_MASK_PWR_EN, MAX8907C_PWR_EN);
	if (ret != 0)
		return ret;

	/*
	 * When enabled, connect PWREN to SEQ2 by clearing SEQ2 configuration
	 * settings for silicon revision that requires s/w WAR. On other
	 * MAX8907B revisions PWREN is always connected to SEQ2.
	 */
	data = max8907c_reg_read(max8907c_client, MAX8907C_REG_II2RR);

	if (data == MAX8907B_II2RR_PWREN_WAR) {
		data = 0x00;
		ret = max8907c_reg_write(max8907c_client, MAX8907C_REG_SEQ2CNFG, data);
	}
	return ret;
}

int max8907c_pwr_en_attach(void)
{
	int ret;

	if (!max8907c_client)
		return -EINVAL;

	/* No sequencer delay for CPU rail when it is attached */
	ret = max8907c_reg_write(max8907c_client, MAX8907C_REG_SDSEQCNT1,
							MAX8907C_DELAY_CNT0);
	if (ret != 0)
		return ret;

	return max8907c_set_bits(max8907c_client, MAX8907C_REG_SDCTL1,
					MAX8907C_MASK_CTL_SEQ, MAX8907C_CTL_SEQ);
}

static int max8907c_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct max8907c *max8907c;
	struct max8907c_platform_data *pdata = i2c->dev.platform_data;
	int ret;
	int i;
	u8 tmp;

	max8907c = kzalloc(sizeof(struct max8907c), GFP_KERNEL);
	if (max8907c == NULL)
		return -ENOMEM;

	max8907c->dev = &i2c->dev;
	dev_set_drvdata(max8907c->dev, max8907c);

	max8907c->i2c_power = i2c;
	i2c_set_clientdata(i2c, max8907c);

	max8907c->i2c_rtc = i2c_new_dummy(i2c->adapter, RTC_I2C_ADDR);
	i2c_set_clientdata(max8907c->i2c_rtc, max8907c);

	mutex_init(&max8907c->io_lock);

	for (i = 0; i < ARRAY_SIZE(cells); i++) {
		cells[i].platform_data = max8907c;
		cells[i].pdata_size = sizeof(*max8907c);
	}
	ret = mfd_add_devices(max8907c->dev, -1, cells, ARRAY_SIZE(cells),
			      NULL, 0);
	if (ret != 0) {
		i2c_unregister_device(max8907c->i2c_rtc);
		kfree(max8907c);
		pr_debug("max8907c: failed to add MFD devices   %X\n", ret);
		return ret;
	}

	max8907c_client = i2c;

	max8907c_irq_init(max8907c, i2c->irq, pdata->irq_base);

	ret = max8097c_add_subdevs(max8907c, pdata);

	if (pdata->use_power_off && !pm_power_off)
		pm_power_off = max8907c_power_off;

	ret = max8907c_i2c_read(i2c, MAX8907C_REG_SYSENSEL, 1, &tmp);
	/*Mask HARD RESET, if enabled */
	if (ret == 0) {
		tmp &= ~(BIT(7));
		ret = max8907c_i2c_write(i2c, MAX8907C_REG_SYSENSEL, 1, &tmp);
	}

	if (ret != 0) {
		pr_err("Failed to write max8907c I2C driver: %d\n", ret);
		return ret;
	}

	if (pdata->max8907c_setup)
		return pdata->max8907c_setup();

	return ret;
}

static int max8907c_i2c_remove(struct i2c_client *i2c)
{
	struct max8907c *max8907c = i2c_get_clientdata(i2c);

	max8907c_remove_subdevs(max8907c);
	i2c_unregister_device(max8907c->i2c_rtc);
	mfd_remove_devices(max8907c->dev);
	max8907c_irq_free(max8907c);
	kfree(max8907c);

	return 0;
}

static const struct i2c_device_id max8907c_i2c_id[] = {
	{"max8907c", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, max8907c_i2c_id);

static const struct dev_pm_ops max8907c_pm_ops = {
	.suspend = max8907c_suspend,
	.resume = max8907c_resume,
};

static struct i2c_driver max8907c_i2c_driver = {
	.driver = {
		.name = "max8907c",
		.owner = THIS_MODULE,
		.pm = &max8907c_pm_ops,
	},
	.probe = max8907c_i2c_probe,
	.remove = max8907c_i2c_remove,
	.id_table = max8907c_i2c_id,
};

static int __init max8907c_i2c_init(void)
{
	int ret = -ENODEV;

	ret = i2c_add_driver(&max8907c_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register I2C driver: %d\n", ret);

	return ret;
}

subsys_initcall(max8907c_i2c_init);

static void __exit max8907c_i2c_exit(void)
{
	i2c_del_driver(&max8907c_i2c_driver);
}

module_exit(max8907c_i2c_exit);

MODULE_DESCRIPTION("MAX8907C multi-function core driver");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maxim-ic.com>");
MODULE_LICENSE("GPL");
