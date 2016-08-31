/*
 * drivers/mfd/max77660-core.c
 * Max77660 mfd driver (I2C bus access)
 *
 * Copyright 2011 Maxim Integrated Products, Inc.
 * Copyright (C) 2011-2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/kthread.h>
#include <linux/mfd/core.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <linux/mfd/max77660/max77660-core.h>


struct max77660_chip *max77660_chip;

static struct resource gpio_resources[] = {
	{
		.start	= MAX77660_IRQ_INT_TOP_GPIO,
		.end	= MAX77660_IRQ_INT_TOP_GPIO,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct resource rtc_resources[] = {
	{
		.start	= MAX77660_IRQ_RTC,
		.end	= MAX77660_IRQ_RTC,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct resource adc_resources[] = {
	{
		.start	= MAX77660_IRQ_ADC,
		.end	= MAX77660_IRQ_ADC,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct resource fg_resources[] = {
	{
		.start	= MAX77660_IRQ_FG,
		.end	= MAX77660_IRQ_FG,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct resource chg_resources[] = {
	{
		.start	= MAX77660_IRQ_CHG,
		.end	= MAX77660_IRQ_CHG,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct resource max77660_sys_wdt_resources[] = {
	{
		.start	= MAX77660_IRQ_GLBL_WDTWRN_SYS,
		.end	= MAX77660_IRQ_GLBL_WDTWRN_SYS,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct resource max77660_chg_extcon_resources[] = {
	{
		.start	= MAX77660_IRQ_CHG,
		.end	= MAX77660_IRQ_CHG,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.start	= MAX77660_IRQ_GLBL_WDTWRN_CHG,
		.end	= MAX77660_IRQ_GLBL_WDTWRN_CHG,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct resource sim_resources[] = {
	{
		.start	= MAX77660_IRQ_SIM,
		.end	= MAX77660_IRQ_SIM,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct mfd_cell max77660_cells[] = {
	{
		.name = "max77660-pinctrl",
	},
	{
		.name = "max77660-gpio",
		.num_resources	= ARRAY_SIZE(gpio_resources),
		.resources	= &gpio_resources[0],
	},
	{
		.name = "max77660-pmic",
	},
	{
		.name = "max77660-leds",
	},
	{
		.name = "max77660-rtc",
		.num_resources	= ARRAY_SIZE(rtc_resources),
		.resources	= &rtc_resources[0],
	},
	{
		.name = "max77660-adc",
		.num_resources	= ARRAY_SIZE(adc_resources),
		.resources	= &adc_resources[0],
	},
	{
		.name = "max77660-fg",
		.num_resources	= ARRAY_SIZE(fg_resources),
		.resources	= &fg_resources[0],
	},
	{
		.name = "max77660-chg",
		.num_resources	= ARRAY_SIZE(chg_resources),
		.resources	= &chg_resources[0],
	},
	{
		.name = "max77660-charger-extcon",
		.num_resources	= ARRAY_SIZE(max77660_chg_extcon_resources),
		.resources	= &max77660_chg_extcon_resources[0],
	},
	{
		/* name matched in max77660 haptic driver */
		.name = "max77660-vibrator",
		/* no irq exists for haptic */
	},
	{
		.name = "max77660-sys-wdt",
		.num_resources	= ARRAY_SIZE(max77660_sys_wdt_resources),
		.resources	= &max77660_sys_wdt_resources[0],
	},
	{
		.name = "max77660-sim",
		.num_resources	= ARRAY_SIZE(sim_resources),
		.resources	= &sim_resources[0],
	},
};

static const struct regmap_irq max77660_top_irqs[] = {
	[MAX77660_IRQ_FG] = {
		.mask = MAX77660_IRQ_TOP1_FUELG_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_CHG] = {
		.mask = MAX77660_IRQ_TOP1_CHARGER_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_CHG] = {
		.mask = MAX77660_IRQ_TOP1_CHARGER_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_RTC] = {
		.mask = MAX77660_IRQ_TOP1_RTC_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_INT_TOP_GPIO] = {
		.mask = MAX77660_IRQ_TOP1_GPIO_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_SIM] = {
		.mask = MAX77660_IRQ_TOP1_SIM_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_ADC] = {
		.mask = MAX77660_IRQ_TOP1_ADC_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_TOPSYSINT] = {
		.mask = MAX77660_IRQ_TOP1_TOPSYS_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_LDOINT] = {
		.mask = MAX77660_IRQ_TOP2_LDO_MASK,
		.reg_offset = 1,
	},
	[MAX77660_IRQ_BUCKINT] = {
		.mask = MAX77660_IRQ_TOP2_BUCK_MASK,
		.reg_offset = 1,
	},
};

static const struct regmap_irq max77660_global_irqs[] = {
	[MAX77660_IRQ_GLBL_TJALRM2 - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT1_TJALRM2_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_GLBL_TJALRM1 - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT1_TJALRM1_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_GLBL_SYSLOW - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT1_SYSLOW_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_GLBL_I2C_WDT - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT1_I2CWDT_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_GLBL_EN0_1SEC - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT1_EN0_1SEC_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_GLBL_EN0_F - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT1_EN0_F_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_GLBL_EN0_R - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT1_EN0_R_MASK,
		.reg_offset = 0,
	},
	[MAX77660_IRQ_GLBL_WDTWRN_CHG - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT2_WDTWRN_CHG_MASK,
		.reg_offset = 1,
	},
	[MAX77660_IRQ_GLBL_WDTWRN_SYS - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT2_WDTWRN_SYS_MASK,
		.reg_offset = 1,
	},
	[MAX77660_IRQ_GLBL_MR_F - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT2_MR_F_MASK,
		.reg_offset = 1,
	},
	[MAX77660_IRQ_GLBL_MR_R - MAX77660_IRQ_GLBL_BASE] = {
		.mask = MAX77660_IRQ_GLBLINT2_MR_R_MASK,
		.reg_offset = 1,
	},
};

static void max77660_power_off(void)
{
	struct max77660_chip *chip = max77660_chip;

	if (!chip)
		return;

	dev_info(chip->dev, "%s: Global shutdown\n", __func__);
	/*
	 * ES1.0 errata suggest that in place of doing read modify write,
	 * write direct valid value.
	 */
	max77660_reg_write(chip->dev, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG0,
			GLBLCNFG0_SFT_OFF_OFFRST_MASK);
}

static void max77660_power_reset(void)
{
	struct max77660_chip *chip = max77660_chip;

	if (!chip)
		return;

	dev_info(chip->dev, "%s: PMIC Reset\n", __func__);
	/*
	 * ES1.0 errata suggest that in place of doing read modify write,
	 * write direct valid value.
	 */
	max77660_reg_write(chip->dev, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG0, 1);
}

static int max77660_32kclk_init(struct max77660_chip *chip,
		struct max77660_platform_data *pdata)
{
	struct max77660_clk32k_platform_data *clk32_pdata = pdata->clk32k_pdata;
	u8 mask = 0;
	u8 val = 0;
	int ret;

	val |= (clk32_pdata->en_clk32out1 ? 1 : 0) << OUT1_EN_32KCLK_SHIFT;
	val |= (clk32_pdata->en_clk32out2 ? 1 : 0) << OUT2_EN_32KCLK_SHIFT;
	mask = OUT1_EN_32KCLK_MASK | OUT2_EN_32KCLK_MASK;

	ret = max77660_reg_update(chip->dev, MAX77660_PWR_SLAVE,
			MAX77660_REG_CNFG32K1, val, mask);

	switch (clk32_pdata->clk32k_mode) {
	case MAX77660_CLK_MODE_DEFAULT:
		goto skip_mod_config;
	case MAX77660_CLK_MODE_LOW_POWER:
		val = 0;
		break;
	case MAX77660_CLK_MODE_GLOBAL_LOW_POWER:
		val = 1;
		break;
	case MAX77660_CLK_MODE_LOW_JITTER:
		val = 3;
		break;
	default:
		val = 0;
		break;
	}

	ret = max77660_reg_update(chip->dev, MAX77660_PWR_SLAVE,
			MAX77660_REG_CNFG32K1, val, PWR_MODE_32KCLK_MASK);
	if (ret < 0) {
		dev_err(chip->dev, "CNFG32K1 read failed: %d\n", ret);
		return ret;
	}

skip_mod_config:
	switch (clk32_pdata->clk32k_load_cap) {
	case MAX77660_CLK_LOAD_CAP_DEFAULT:
		goto skip_cap_config;
	case MAX77660_CLK_LOAD_CAP_12pF:
		val = 0;
		break;
	case MAX77660_CLK_LOAD_CAP_22pF:
		val = 1;
		break;
	case MAX77660_CLK_LOAD_CAP_10pF:
		val = 3;
		break;
	default:
		val = 0;
		break;
	}

	ret = max77660_reg_update(chip->dev, MAX77660_PWR_SLAVE,
			MAX77660_REG_CNFG32K2, val,
			MAX77660_CNFG32K2_32K_LOAD_MASK);
	if (ret < 0) {
		dev_err(chip->dev, "CNFG32K2 read failed: %d\n", ret);
		return ret;
	}
skip_cap_config:
	return ret;
}

static struct regmap_irq_chip max77660_top_irq_chip = {
	.name = "max77660-top",
	.irqs = max77660_top_irqs,
	.num_irqs = ARRAY_SIZE(max77660_top_irqs),
	.num_regs = 2,
	.irq_reg_stride = 1,
	.status_base = MAX77660_REG_IRQ_TOP1,
	.mask_base = MAX77660_REG_IRQ_TOP1_MASK,
};

static struct regmap_irq_chip max77660_global_irq_chip = {
	.name = "max77660-global",
	.irqs = max77660_global_irqs,
	.num_irqs = ARRAY_SIZE(max77660_global_irqs),
	.num_regs = 2,
	.irq_reg_stride = 1,
	.status_base = MAX77660_REG_IRQ_GLBINT1,
	.mask_base = MAX77660_REG_IRQ_GLBINT1_MASK,
};

static int max77660_init_irqs(struct max77660_chip *chip,
		struct max77660_platform_data *pdata)
{
	int ret;

	/* Unmask the IQR_M */
	ret = max77660_reg_clr_bits(chip->dev, MAX77660_PWR_SLAVE,
			MAX77660_REG_IRQ_GLBINT1_MASK,
			MAX77660_IRQ_GLBLINT1_IRQ_M_MASK);
	if (ret < 0) {
		dev_err(chip->dev, "Clear IRQM failed %d\n", ret);
		return ret;
	}

	ret = regmap_add_irq_chip(chip->rmap[MAX77660_PWR_SLAVE],
		chip->chip_irq, IRQF_ONESHOT, pdata->irq_base,
		&max77660_top_irq_chip, &chip->top_irq_data);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to add top irq_chip %d\n", ret);
		return ret;
	}

	ret = regmap_add_irq_chip(chip->rmap[MAX77660_PWR_SLAVE],
		pdata->irq_base + MAX77660_IRQ_TOPSYSINT,
		IRQF_ONESHOT | IRQF_EARLY_RESUME,
		pdata->irq_base + MAX77660_IRQ_GLBL_BASE,
		&max77660_global_irq_chip, &chip->global_irq_data);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to add global irq_chip %d\n", ret);
		goto fail_global_irq;
	}
	return 0;

fail_global_irq:
	regmap_del_irq_chip(chip->chip_irq, chip->top_irq_data);
	chip->top_irq_data = NULL;
	return ret;
}

static bool rd_wr_reg_power(struct device *dev, unsigned int reg)
{
	if (reg < 0xF2)
		return true;

	dev_err(dev, "non-existing reg %s() reg 0x%x\n", __func__, reg);
	BUG();
	return false;
}

static bool rd_wr_reg_rtc(struct device *dev, unsigned int reg)
{
	if (reg < 0x1C)
		return true;

	dev_err(dev, "non-existing reg %s() reg 0x%x\n", __func__, reg);
	BUG();
	return false;
}

static bool rd_wr_reg_fg(struct device *dev, unsigned int reg)
{
	if (reg <= 0xFF)
		return true;

	dev_err(dev, "non-existing reg %s() reg 0x%x\n", __func__, reg);
	BUG();
	return false;
}

static bool rd_wr_reg_chg(struct device *dev, unsigned int reg)
{

	switch (reg) {
	case MAX77660_CHARGER_USBCHGCTRL:
	case MAX77660_CHARGER_CHGINT ... MAX77660_CHARGER_MBATREGMAX:
		return true;
	default:
		return false;
	}
}

static bool rd_wr_reg_haptic(struct device *dev, unsigned int reg)
{
	if (reg <= 0xFF)
		return true;

	dev_err(dev, "non-existing reg %s() reg 0x%x\n", __func__, reg);
	BUG();
	return false;
}

static const struct regmap_config max77660_regmap_config[] = {
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xC0,
		.writeable_reg = rd_wr_reg_power,
		.readable_reg = rd_wr_reg_power,
	}, {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xF2,
		.writeable_reg = rd_wr_reg_rtc,
		.readable_reg = rd_wr_reg_rtc,
	}, {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xFF,
		.writeable_reg = rd_wr_reg_fg,
		.readable_reg = rd_wr_reg_fg,
	}, {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xFF,
		.writeable_reg = rd_wr_reg_chg,
		.readable_reg = rd_wr_reg_chg,
	}, {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xFF,
		.writeable_reg = rd_wr_reg_haptic,
		.readable_reg = rd_wr_reg_haptic,
	},
};

static int max77660_slave_address[MAX77660_NUM_SLAVES] = {
	MAX77660_PWR_I2C_ADDR,
	MAX77660_RTC_I2C_ADDR,
	MAX77660_CHG_I2C_ADDR,
	MAX77660_FG_I2C_ADDR,
	MAX77660_HAPTIC_I2C_ADDR,
};

static int max77660_read_es_version(struct max77660_chip *chip)
{
	int ret;
	u8 val;
	u8 cid;
	int i;

	for (i = MAX77660_REG_CID0; i <= MAX77660_REG_CID5; ++i) {
		ret = max77660_reg_read(chip->dev, MAX77660_PWR_SLAVE,
				i, &cid);
		if (ret < 0) {
			dev_err(chip->dev, "CID%d register read failed: %d\n",
					i - MAX77660_REG_CID0, ret);
			return ret;
		}
		dev_info(chip->dev, "CID%d: 0x%02x\n",
			i - MAX77660_REG_CID0, cid);
	}

	/* Read ES version */
	ret = max77660_reg_read(chip->dev, MAX77660_PWR_SLAVE,
			MAX77660_REG_CID5, &val);
	if (ret < 0) {
		dev_err(chip->dev, "CID5 read failed: %d\n", ret);
		return ret;
	}
	chip->es_minor_version = MAX77660_CID5_DIDM(val);
	chip->es_major_version = 1;
	return ret;
}

static int max77660_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct max77660_platform_data *pdata = client->dev.platform_data;
	struct max77660_chip *chip;
	int ret = 0;
	int i;

	if (!pdata) {
		dev_err(&client->dev, "probe: Invalid platform_data\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		dev_err(&client->dev, "Memory alloc for chip failed\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, chip);

	for (i = 0; i < MAX77660_NUM_SLAVES; i++) {
		if (max77660_slave_address[i] == client->addr)
			chip->clients[i] = client;
		else
			chip->clients[i] = i2c_new_dummy(client->adapter,
						max77660_slave_address[i]);
		if (!chip->clients[i]) {
			dev_err(&client->dev, "can't attach client %d\n", i);
			ret = -ENOMEM;
			goto fail_client_reg;
		}

		i2c_set_clientdata(chip->clients[i], chip);
		chip->rmap[i] = devm_regmap_init_i2c(chip->clients[i],
					&max77660_regmap_config[i]);
		if (IS_ERR(chip->rmap[i])) {
			ret = PTR_ERR(chip->rmap[i]);
			dev_err(&client->dev,
				"regmap %d init failed, err %d\n", i, ret);
			goto fail_client_reg;
		}
	}

	chip->dev = &client->dev;
	chip->pdata = pdata;
	chip->irq_base = pdata->irq_base;
	chip->chip_irq = client->irq;

	ret = max77660_read_es_version(chip);
	if (ret < 0) {
		dev_err(chip->dev, "Chip revision init failed: %d\n", ret);
		goto fail_client_reg;
	}

	dev_info(chip->dev, "MAX77660 Rev Number ES%d.%d\n",
		chip->es_major_version, chip->es_minor_version);

	ret = max77660_init_irqs(chip, pdata);
	if (ret < 0) {
		dev_err(chip->dev, "Irq initialisation failed: %d\n", ret);
		goto fail_client_reg;
	}

	max77660_chip = chip;
	if (pdata->use_power_off && !pm_power_off)
		pm_power_off = max77660_power_off;

	if (pdata->use_power_reset && !pm_power_reset)
		pm_power_reset = max77660_power_reset;

	ret = max77660_32kclk_init(chip, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "probe: Failed to initialize 32k clk\n");
		goto out_exit;
	}

	ret =  mfd_add_devices(&client->dev, -1, max77660_cells,
			ARRAY_SIZE(max77660_cells), NULL, chip->irq_base, NULL);
	if (ret < 0) {
		dev_err(&client->dev, "mfd add dev failed, e = %d\n", ret);
		goto out_exit;
	}

	return 0;

out_exit:
	regmap_del_irq_chip(pdata->irq_base + MAX77660_IRQ_GLBL_BASE,
		chip->global_irq_data);
	regmap_del_irq_chip(chip->chip_irq, chip->top_irq_data);

fail_client_reg:
	for (i = 0; i < MAX77660_NUM_SLAVES; i++) {
		if (chip->clients[i]  && (chip->clients[i] != client))
			i2c_unregister_device(chip->clients[i]);
	}

	max77660_chip = NULL;
	return ret;
}

static int max77660_remove(struct i2c_client *client)
{
	struct max77660_chip *chip = i2c_get_clientdata(client);
	struct max77660_platform_data *pdata = client->dev.platform_data;
	int i;

	mfd_remove_devices(chip->dev);
	regmap_del_irq_chip(pdata->irq_base + MAX77660_IRQ_GLBL_BASE,
		chip->global_irq_data);
	regmap_del_irq_chip(chip->chip_irq, chip->top_irq_data);
	for (i = 0; i < MAX77660_NUM_SLAVES; i++) {
		if (chip->clients[i] != client)
			i2c_unregister_device(chip->clients[i]);
	}
	max77660_chip = NULL;
	return 0;
}

static const struct i2c_device_id max77660_id[] = {
	{"max77660", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, max77660_id);

static struct i2c_driver max77660_driver = {
	.driver = {
		.name = "max77660",
		.owner = THIS_MODULE,
	},
	.probe = max77660_probe,
	.remove = max77660_remove,
	.id_table = max77660_id,
};

static int __init max77660_init(void)
{
	return i2c_add_driver(&max77660_driver);
}
subsys_initcall(max77660_init);

static void __exit max77660_exit(void)
{
	i2c_del_driver(&max77660_driver);
}
module_exit(max77660_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MAX77660 Multi Function Device Core Driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Maxim Integrated");
MODULE_ALIAS("i2c:max77660-core");
