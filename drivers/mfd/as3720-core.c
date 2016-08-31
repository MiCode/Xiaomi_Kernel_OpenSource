/*
 * as3720-core.c - core driver for AS3720 PMICs
 *
 * Copyright (C) 2012 ams AG
 *
 * Author: Bernhard Breinbauer <bernhard.breinbauer@ams.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/interrupt.h>
#include <linux/mfd/as3720.h>

#define AS3720_DRIVER_VERSION	"v0.0.6"

#define NUM_INT_REG 3

enum as3720_ids {
	AS3720_GPIO_ID,
	AS3720_REGULATOR_ID,
	AS3720_RTC_ID,
};

static const struct regmap_config as3720_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = AS3720_REGISTER_COUNT,
	.num_reg_defaults_raw = AS3720_REGISTER_COUNT,
};

static const struct resource as3720_rtc_resource[] = {
	{
		.name = "RTC_ALARM",
		.start = AS3720_IRQ_RTC_ALARM,
		.end = AS3720_IRQ_RTC_ALARM,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell as3720_devs[] = {
	{
		.name = "as3720-gpio",
		.id = AS3720_GPIO_ID,
	},
	{
		.name = "as3720-regulator",
		.id = AS3720_REGULATOR_ID,
	},
	{
		.name = "as3720-rtc",
		.num_resources = ARRAY_SIZE(as3720_rtc_resource),
		.resources = as3720_rtc_resource,
		.id = AS3720_RTC_ID,
	},
};

static const struct regmap_irq as3720_irqs[] = {
	/* INT1 IRQs */
	[AS3720_IRQ_LID] = {
		.mask = AS3720_IRQ_MASK_LID,
	},
	[AS3720_IRQ_ACOK] = {
		.mask = AS3720_IRQ_MASK_ACOK,
	},
	[AS3720_IRQ_CORE_PWRREQ] = {
		.mask = AS3720_IRQ_MASK_CORE_PWRREQ,
	},
	[AS3720_IRQ_SD0] = {
		.mask = AS3720_IRQ_MASK_SD0,
	},
	[AS3720_IRQ_ONKEY_LONG] = {
		.mask = AS3720_IRQ_MASK_ONKEY_LONG,
	},
	[AS3720_IRQ_ONKEY] = {
		.mask = AS3720_IRQ_MASK_ONKEY,
	},
	[AS3720_IRQ_OVTMP] = {
		.mask = AS3720_IRQ_MASK_OVTMP,
	},
	[AS3720_IRQ_LOWBAT] = {
		.mask = AS3720_IRQ_MASK_LOWBAT,
	},
	[AS3720_IRQ_RTC_REP] = {
		.mask = AS3720_IRQ_MASK_RTC_REP,
		.reg_offset = 1,
	},
	[AS3720_IRQ_RTC_ALARM] = {
		.mask = AS3720_IRQ_MASK_RTC_ALARM,
		.reg_offset = 2,
	},
};

static struct regmap_irq_chip as3720_irq_chip = {
	.name = "as3720",
	.irqs = as3720_irqs,
	.num_irqs = ARRAY_SIZE(as3720_irqs),
	.num_regs = 3,
	.status_base = AS3720_INTERRUPTSTATUS1_REG,
	.mask_base = AS3720_INTERRUPTMASK1_REG,
	.wake_base = 1,
};

static void as3720_reg_init(struct as3720 *as3720,
			struct as3720_reg_init *reg_data)
{
	int ret;

	while (reg_data->reg != AS3720_REG_INIT_TERMINATE) {
		ret = as3720_reg_write(as3720, reg_data->reg, reg_data->val);
		if (ret) {
			dev_err(as3720->dev,
				   "reg setup failed: %d\n", ret);
			return;
		}
		reg_data++;
	}
}

static int as3720_init(struct as3720 *as3720,
		       struct as3720_platform_data *pdata, int irq)
{
	u32 reg;
	int ret;

	/* Check that this is actually a AS3720 */
	ret = regmap_read(as3720->regmap, AS3720_ADDR_ASIC_ID1, &reg);
	if (ret != 0) {
		dev_err(as3720->dev,
			"Chip ID register read failed\n");
		return -EIO;
	}
	if (reg != AS3720_DEVICE_ID) {
		dev_err(as3720->dev,
			"Device is not an AS3720, ID is 0x%x\n"
			, reg);
		return -ENODEV;
	}

	ret = regmap_read(as3720->regmap, AS3720_ADDR_ASIC_ID2, &reg);
	if (ret < 0) {
		dev_err(as3720->dev,
			   "ID2 register read failed: %d\n", ret);
		return ret;
	}
	dev_info(as3720->dev, "AS3720 with revision %x found\n",
		   reg);

	/* do some initial platform register setup */
	if (pdata->core_init_data)
		as3720_reg_init(as3720, pdata->core_init_data);

	return 0;
}

static int as3720_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct as3720 *as3720;
	struct as3720_platform_data *pdata;
	int irq_flag;
	int ret;

	pdata = dev_get_platdata(&i2c->dev);
	if (!pdata) {
		dev_err(&i2c->dev, "as3720 requires platform data\n");
		return -EINVAL;
	}

	as3720 = devm_kzalloc(&i2c->dev, sizeof(struct as3720), GFP_KERNEL);
	if (as3720 == NULL) {
		dev_err(&i2c->dev, "mem alloc for as3720 failed\n");
		return -ENOMEM;
	}

	as3720->dev = &i2c->dev;
	as3720->chip_irq = i2c->irq;
	i2c_set_clientdata(i2c, as3720);

	as3720->regmap = devm_regmap_init_i2c(i2c, &as3720_regmap_config);
	if (IS_ERR(as3720->regmap)) {
		ret = PTR_ERR(as3720->regmap);
		dev_err(&i2c->dev, "regmap_init failed with err: %d\n", ret);
		return ret;
	}

	irq_flag = pdata->irq_type;
	irq_flag = IRQF_ONESHOT;
	ret = regmap_add_irq_chip(as3720->regmap, as3720->chip_irq,
			irq_flag, pdata->irq_base, &as3720_irq_chip,
			&as3720->irq_data);
	if (ret < 0) {
		dev_err(as3720->dev, "irq allocation failed for as3720 failed\n");
		return ret;
	}

	ret = as3720_init(as3720, pdata, i2c->irq);
	if (ret < 0)
		return ret;

	ret = mfd_add_devices(&i2c->dev, -1, as3720_devs,
		ARRAY_SIZE(as3720_devs), NULL,
		pdata->irq_base, NULL);
	if (ret) {
		dev_err(as3720->dev, "add mfd devices failed with err: %d\n",
			ret);
		return ret;
	}

	dev_info(as3720->dev,
		"AS3720 core driver %s initialized successfully\n",
		AS3720_DRIVER_VERSION);

	return 0;
}

static int as3720_i2c_remove(struct i2c_client *i2c)
{
	struct as3720 *as3720 = i2c_get_clientdata(i2c);

	mfd_remove_devices(as3720->dev);
	regmap_del_irq_chip(as3720->chip_irq, as3720->regmap);

	return 0;
}

static const struct i2c_device_id as3720_i2c_id[] = {
	{"as3720", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, as3720_i2c_id);

static struct i2c_driver as3720_i2c_driver = {
	.driver = {
		   .name = "as3720",
		   .owner = THIS_MODULE,
		   },
	.probe = as3720_i2c_probe,
	.remove = as3720_i2c_remove,
	.id_table = as3720_i2c_id,
};

static int __init as3720_i2c_init(void)
{
	return i2c_add_driver(&as3720_i2c_driver);
}

subsys_initcall(as3720_i2c_init);

static void __exit as3720_i2c_exit(void)
{
	i2c_del_driver(&as3720_i2c_driver);
}

module_exit(as3720_i2c_exit);

MODULE_DESCRIPTION("I2C support for AS3720 PMICs");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bernhard Breinbauer <Bernhard.Breinbauer@ams.com>");

