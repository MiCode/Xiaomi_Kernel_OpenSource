/*
 * Core driver for MAXIM MAX77665
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77665.h>
#include <linux/slab.h>

#define MAX77665_INT_STS	0x22
#define MAX77665_INT_MSK	0x23
#define MAX77665_PMIC_FLASH	0x00 ... 0x10
#define MAX77665_PMIC_PMIC	0x20 ... 0x2D
#define MAX77665_PMIC_CHARGER	0xB0 ... 0xC6
#define MAX77665_MUIC		0x00 ... 0x0E
#define MAX77665_HAPTIC		0x00 ... 0x10

static u8 max77665_i2c_slave_address[] = {
	[MAX77665_I2C_SLAVE_PMIC] = 0x66,
	[MAX77665_I2C_SLAVE_MUIC] = 0x25,
	[MAX77665_I2C_SLAVE_HAPTIC] = 0x48,
};

static const struct resource charger_resource[] = {
	{
		.start = MAX77665_IRQ_CHARGER,
		.end = MAX77665_IRQ_CHARGER,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource flash_resource[] = {
	{
		.start = MAX77665_IRQ_FLASH,
		.end = MAX77665_IRQ_FLASH,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource muic_resource[] = {
	{
		.start = MAX77665_IRQ_MUIC,
		.end = MAX77665_IRQ_MUIC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell max77665s[] = {
	[MAX77665_CELL_CHARGER]	= {
			.name = "max77665-charger",
			.resources = charger_resource,
			.num_resources = ARRAY_SIZE(charger_resource),
	},

	[MAX77665_CELL_FLASH]	= {
			.name = "max77665-flash",
			.resources = flash_resource,
			.num_resources = ARRAY_SIZE(flash_resource),
	},

	[MAX77665_CELL_MUIC]	= {
			.name = "max77665-muic",
			.resources = muic_resource,
			.num_resources = ARRAY_SIZE(muic_resource),
	},

	[MAX77665_CELL_HAPTIC]	= {
			.name = "max77665-haptic",
	},
};

static const struct regmap_irq max77665_irqs[] = {
	[MAX77665_IRQ_CHARGER] = {
		.mask = MAX77665_INTSRC_CHGR_MASK,
	},
	[MAX77665_IRQ_TOP_SYS] = {
		.mask = MAX77665_INTSRC_TOP_MASK,
	},
	[MAX77665_IRQ_FLASH] = {
		.mask = MAX77665_INTSRC_FLASH_MASK,
	},
	[MAX77665_IRQ_MUIC] = {
		.mask = MAX77665_INTSRC_MUIC_MASK,
	},
};

static struct regmap_irq_chip max77665_irq_chip = {
	.name = "max77665-irq",
	.irqs = max77665_irqs,
	.num_irqs = ARRAY_SIZE(max77665_irqs),
	.num_regs = 1,
	.irq_reg_stride = 1,
	.status_base = MAX77665_REG_INTSRC,
	.mask_base = MAX77665_REG_INTSRC_MASK,
};

static int max77665_irq_init(struct max77665 *max77665, int irq,
	int irq_base, unsigned long irq_flag)
{
	int ret;

	if (!irq || (irq_base <= 0)) {
		dev_err(max77665->dev, "IRQ base not set, int not supported\n");
		return -EINVAL;
	}

	ret = regmap_add_irq_chip(max77665->regmap[MAX77665_I2C_SLAVE_PMIC],
			irq, irq_flag | IRQF_ONESHOT, irq_base,
			&max77665_irq_chip, &max77665->regmap_irq_data);
	if (ret < 0)
		dev_err(max77665->dev, "regmap irq registration failed %d\n",
			ret);
	return ret;
}

static irqreturn_t max77665_top_sys_irq_handler(int irq, void *data)
{
	struct max77665 *max77665 = data;
	uint8_t top_int_sts = 0;
	int ret;

	dev_info(max77665->dev, "Top Sys interrupt occured\n");

	ret = max77665_read(max77665->dev, MAX77665_I2C_SLAVE_PMIC,
			MAX77665_REG_TOP_SYS_INT_STS, &top_int_sts);
	if (ret < 0) {
		dev_err(max77665->dev,
			"TOP_SYS_INT_STS read failed: %d\n", ret);
		return ret;
	}

	dev_info(max77665->dev,
		"Top Sys interrupt status 0x%02x\n", top_int_sts);

	if (top_int_sts & MAX77665_TOP_SYS_INT_120C)
		dev_info(max77665->dev, "120C thermal interrupt detected\n");
	if (top_int_sts & MAX77665_TOP_SYS_INT_140C)
		dev_info(max77665->dev, "140C thermal interrupt detected\n");
	if (top_int_sts & MAX77665_TOP_SYS_INT_LOWSYS)
		dev_info(max77665->dev, "Low Sys interrupt detected\n");
	return IRQ_HANDLED;
}

static int max77665_init_top_sys_interrupt(struct max77665 *max77665,
	struct max77665_platform_data *pdata)
{
	struct max77665_system_interrupt *sys_int = pdata->system_interrupt;
	unsigned int mask = 0;
	unsigned int val = 0;
	int ret;

	if (!sys_int)
		return 0;

	if (sys_int->enable_thermal_interrupt)
		val |= MAX77665_TOP_SYS_INT_120C | MAX77665_TOP_SYS_INT_140C;
	if (sys_int->enable_low_sys_interrupt)
		val |= MAX77665_TOP_SYS_INT_LOWSYS;

	mask = MAX77665_TOP_SYS_INT_120C | MAX77665_TOP_SYS_INT_140C |
			MAX77665_TOP_SYS_INT_LOWSYS;

	ret = max77665_update_bits(max77665->dev, MAX77665_I2C_SLAVE_PMIC,
			MAX77665_REG_TOP_SYS_INT_MASK, mask, mask);
	if (ret < 0) {
		dev_err(max77665->dev,
			"TOP_SYS_INT_MASK update failed: %d\n", ret);
		return ret;
	}

	max77665->top_sys_irq = pdata->irq_base + MAX77665_IRQ_TOP_SYS;
	ret = request_threaded_irq(max77665->top_sys_irq, NULL,
		max77665_top_sys_irq_handler, 0, "max77665-top-sys",
		max77665);
	if (ret < 0) {
		dev_err(max77665->dev,
			"TopSysInterrupt register failed: %d\n", ret);
		max77665->top_sys_irq = 0;
		return ret;
	}

	ret = max77665_update_bits(max77665->dev, MAX77665_I2C_SLAVE_PMIC,
			MAX77665_REG_TOP_SYS_INT_MASK, mask, ~val);
	if (ret < 0) {
		dev_err(max77665->dev,
			"TOP_SYS_INT_MASK update failed: %d\n", ret);
		return ret;
	}
	return ret;
}

static void max77665_deinit_top_sys_interrupt(struct max77665 *max77665)
{
	if (max77665->top_sys_irq)
		free_irq(max77665->top_sys_irq, max77665);
}

static bool rd_wr_reg_pmic(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX77665_PMIC_FLASH:
	case MAX77665_PMIC_PMIC:
	case MAX77665_PMIC_CHARGER:
		return true;
	default:
		dev_dbg(dev, "non-existing reg %s() reg 0x%x\n", __func__, reg);
		return false;
	}
}

static bool rd_wr_reg_muic(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX77665_MUIC:
		return true;
	default:
		dev_dbg(dev, "non-existing reg %s() reg 0x%x\n", __func__, reg);
		return false;
	}
}

static bool rd_wr_reg_haptic(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX77665_HAPTIC:
		return true;
	default:
		dev_dbg(dev, "non-existing reg %s() reg 0x%x\n", __func__, reg);
		return false;
	}
}

static const struct regmap_config max77665_regmap_config[] = {
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xFF,
		.writeable_reg = rd_wr_reg_pmic,
		.readable_reg = rd_wr_reg_pmic,
		.cache_type = REGCACHE_RBTREE,
	}, {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0x0E,
		.writeable_reg = rd_wr_reg_muic,
		.readable_reg = rd_wr_reg_muic,
		.cache_type = REGCACHE_RBTREE,
	}, {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0x10,
		.writeable_reg = rd_wr_reg_haptic,
		.readable_reg = rd_wr_reg_haptic,
		.cache_type = REGCACHE_RBTREE,
	},
};

static int __devinit max77665_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct max77665_platform_data *pdata = client->dev.platform_data;
	struct max77665 *max77665;
	struct i2c_client *slv_client;
	int ret;
	int i;

	if (!pdata) {
		dev_err(&client->dev, "max77665 requires platform data\n");
		return -EINVAL;
	}

	max77665 = devm_kzalloc(&client->dev, sizeof(*max77665), GFP_KERNEL);
	if (!max77665) {
		dev_err(&client->dev, "mem alloc for max77665 failed\n");
		return -ENOMEM;
	}

	max77665->dev = &client->dev;

	for (i = 0; i < MAX77665_I2C_SLAVE_MAX; ++i) {
		if (i == 0)
			slv_client = client;
		else
			slv_client = i2c_new_dummy(client->adapter,
						max77665_i2c_slave_address[i]);
		if (!slv_client) {
			dev_err(&client->dev, "can't attach client %d\n", i);
			ret = -ENOMEM;
			goto err_exit;
		}
		max77665->client[i] = slv_client;
		i2c_set_clientdata(slv_client, max77665);

		max77665->regmap[i] = devm_regmap_init_i2c(slv_client,
					&max77665_regmap_config[i]);
		if (IS_ERR(max77665->regmap[i])) {
			ret = PTR_ERR(max77665->regmap[i]);
			dev_err(&client->dev,
				"regmap %d init failed with err: %d\n",	i, ret);
			goto err_exit;
		}
	}

	max77665_irq_init(max77665, client->irq,
				pdata->irq_base, pdata->irq_flag);

	max77665_init_top_sys_interrupt(max77665, pdata);

	max77665s[MAX77665_CELL_CHARGER].platform_data =
					pdata->charger_platform_data.pdata;
	max77665s[MAX77665_CELL_CHARGER].pdata_size =
					pdata->charger_platform_data.size;

	max77665s[MAX77665_CELL_FLASH].platform_data =
					pdata->flash_platform_data.pdata;
	max77665s[MAX77665_CELL_FLASH].pdata_size =
					pdata->flash_platform_data.size;

	max77665s[MAX77665_CELL_MUIC].platform_data =
					pdata->muic_platform_data.pdata;
	max77665s[MAX77665_CELL_MUIC].pdata_size =
					pdata->muic_platform_data.size;

	max77665s[MAX77665_CELL_HAPTIC].platform_data =
					pdata->haptic_platform_data.pdata;
	max77665s[MAX77665_CELL_HAPTIC].pdata_size =
					pdata->haptic_platform_data.size;

	ret = mfd_add_devices(max77665->dev, -1, max77665s,
		ARRAY_SIZE(max77665s), NULL, pdata->irq_base);
	if (ret) {
		dev_err(&client->dev, "add mfd devices failed with err: %d\n",
			ret);
		goto err_irq_exit;
	}

	return 0;

err_irq_exit:
	max77665_deinit_top_sys_interrupt(max77665);
	regmap_del_irq_chip(client->irq, max77665->regmap_irq_data);

err_exit:
	for (i = 0; i < MAX77665_I2C_SLAVE_MAX; ++i) {
		slv_client = max77665->client[i];
		if (slv_client && slv_client != client)
			i2c_unregister_device(slv_client);
	}
	return ret;
}

static int __devexit max77665_i2c_remove(struct i2c_client *client)
{
	struct max77665 *max77665 = i2c_get_clientdata(client);
	int i;
	struct i2c_client *slv_client;

	mfd_remove_devices(max77665->dev);
	max77665_deinit_top_sys_interrupt(max77665);
	regmap_del_irq_chip(client->irq, max77665->regmap_irq_data);

	for (i = 0; i < MAX77665_I2C_SLAVE_MAX; ++i) {
		slv_client = max77665->client[i];
		if (slv_client && slv_client != client)
			i2c_unregister_device(slv_client);
	}

	return 0;
}

static const struct i2c_device_id max77665_id_table[] = {
	{ "max77665", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max77665_id_table);

static struct i2c_driver max77665_driver = {
	.driver	= {
		.name	= "max77665",
		.owner	= THIS_MODULE,
	},
	.probe		= max77665_i2c_probe,
	.remove		= __devexit_p(max77665_i2c_remove),
	.id_table	= max77665_id_table,
};

static int __init max77665_init(void)
{
	return i2c_add_driver(&max77665_driver);
}
subsys_initcall(max77665_init);

static void __exit max77665_exit(void)
{
	i2c_del_driver(&max77665_driver);
}
module_exit(max77665_exit);

MODULE_DESCRIPTION("MAXIM MAX77665 core driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
