/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <mtk_spi.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */
#include "rt5734-spi.h"

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <mach/mtk_pmic_ipi.h>
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

#define RT5734_DRV_VERSION	"1.0.0_MTK"
#define RT5734_IRQ_ENABLE	0

static int rt5734_read_device(void *client, u32 addr, int len, void *dst)
{
	int ret;
#ifdef CONIFG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support for sspm\n", __func__);
	return 0;
#else
	struct spi_device *spi = (struct spi_device *)client;
	struct spi_transfer xfer = {0,}; /* must init spi_transfer here */
	struct spi_message msg;
	u32 tx_buf;
	u32 rx_buf;

	if (len != 1) {
		pr_notice("%s not support multi read now\n", __func__);
		return -EINVAL;
	}

	/* LSM first, TX: buf, size, reg, 89 */
	tx_buf = 0x00010080;
	rx_buf = 0xffffffff;

	tx_buf |= addr << 8;

	xfer.tx_buf = &tx_buf;
	xfer.rx_buf = &rx_buf;
	xfer.len = 4;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);
	if (ret < 0 || rx_buf == 0xffffffff)
		return ret;

	*(unsigned char *)dst = (rx_buf & 0xff000000) >> 24;
#if 0
	pr_info("%s addr 0x%02x = 0x%02x\n", __func__, addr, *val);
	pr_info("%s tx_buf = 0x%08x\n", __func__, tx_buf);
	pr_info("%s rx_buf = 0x%08x\n", __func__, rx_buf);
#endif
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	return ret;
}

int rt5734_write_device(void *client,
		uint32_t addr, int len, const void *src)
{
	int ret;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support sspm\n", __func__);
	return 0;
#else
	struct spi_device *spi = (struct spi_device *)client;
	struct spi_transfer xfer = {0,}; /* must init spi_transfer here */
	struct spi_message msg;
	unsigned char regval;
	u32 tx_buf;
	u32 rx_buf;

	if (len != 1) {
		pr_notice("%s not support multi write now\n", __func__);
		return -EINVAL;
	}

	tx_buf = 0x00010000;
	rx_buf = 0xffffffff;

	regval = *(unsigned char *)src;
	tx_buf |= ((regval << 24) | (addr << 8));

	xfer.tx_buf = &tx_buf;
	xfer.rx_buf = &rx_buf;
	xfer.len = 4;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);
	if (ret < 0 || rx_buf == 0xffffffff)
		return ret;

#if 0
	pr_info("%s addr 0x%02x = 0x%02x\n", __func__, addr, value);
	pr_info("%s tx_buf = 0x%08x\n", __func__, tx_buf);
	pr_info("%s rx_buf = 0x%08x\n", __func__, rx_buf);
#endif
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	return ret;
}

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT5734_CHIPNAME_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_FLT_RECORDTEMP_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_IRQ_MASK_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK1_DCM_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK1_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK1_MODE_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK1_RSPCFG1_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK2_DCM_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK2_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK2_MODE_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK2_RSPCFG1_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK3_DCM_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK3_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK3_MODE_R, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5734_BUCK3_RSPCFG1_R, 1, RT_VOLATILE, {});

static const rt_register_map_t rt5734_regmap[] = {
	RT_REG(RT5734_CHIPNAME_R),
	RT_REG(RT5734_FLT_RECORDTEMP_R),
	RT_REG(RT5734_IRQ_MASK_R),
	RT_REG(RT5734_BUCK1_DCM_R),
	RT_REG(RT5734_BUCK1_R),
	RT_REG(RT5734_BUCK1_MODE_R),
	RT_REG(RT5734_BUCK1_RSPCFG1_R),
	RT_REG(RT5734_BUCK2_DCM_R),
	RT_REG(RT5734_BUCK2_R),
	RT_REG(RT5734_BUCK2_MODE_R),
	RT_REG(RT5734_BUCK2_RSPCFG1_R),
	RT_REG(RT5734_BUCK3_DCM_R),
	RT_REG(RT5734_BUCK3_R),
	RT_REG(RT5734_BUCK3_MODE_R),
	RT_REG(RT5734_BUCK3_RSPCFG1_R),
};

static struct rt_regmap_properties rt5734_regmap_props = {
	.name = "rt5734",
	.aliases = "rt5734",
	.register_num = ARRAY_SIZE(rt5734_regmap),
	.rm = rt5734_regmap,
	.rt_regmap_mode = RT_CACHE_DISABLE,
};

static struct rt_regmap_fops rt5734_regmap_fops = {
	.read_device = rt5734_read_device,
	.write_device = rt5734_write_device,
};
#endif /* CONFIG_RT_REGMAP */

int rt5734_read_byte(void *client, uint32_t addr, uint32_t *value)
{
	int ret;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support for sspm\n", __func__);
	ret = 0;
#else
	struct spi_device *spi = (struct spi_device *)client;
	struct rt5734_chip *chip = spi_get_drvdata(spi);

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->regmap_dev, addr, 1, value);
#else
	ret = rt5734_read_device(chip->spi, addr, 1, value);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		pr_notice("%s read addr0x%02x fail\n", __func__, addr);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	return ret;
}
EXPORT_SYMBOL(rt5734_read_byte);

int rt5734_write_byte(void *client, uint32_t addr, uint32_t data)
{
	int ret = 0;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support for sspm\n", __func__);
	ret = 0;
#else
	struct spi_device *spi = (struct spi_device *)client;
	struct rt5734_chip *chip = spi_get_drvdata(spi);

#ifdef CONFIG_RT_REGMAP
	ret =  rt_regmap_block_write(chip->regmap_dev, addr, 1, &data);
#else
	ret =  rt5734_write_device(chip->spi, addr, 1, &data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		pr_notice("%s write addr0x%02x fail\n", __func__, addr);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	return ret;
}

int rt5734_assign_bit(void *client,
	uint32_t reg, uint32_t mask, uint32_t data)
{
	int ret = 0;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support for sspm\n", __func__);
	return 0;
#else
	struct spi_device *spi = (struct spi_device *)client;
	struct rt5734_chip *ri = spi_get_drvdata(spi);
	unsigned char tmp = 0;
	uint32_t regval = 0;

	mutex_lock(&ri->io_lock);
	ret = rt5734_read_byte(spi, reg, &regval);
	if (ret < 0) {
		pr_notice("%s fail reg0x%02x data0x%02x\n",
				__func__, reg, data);
		goto OUT_ASSIGN;
	}
	tmp = ((regval & 0xff) & ~mask);
	tmp |= (data & mask);
	ret = rt5734_write_byte(spi, reg, tmp);
	if (ret < 0)
		pr_notice("%s fail reg0x%02x data0x%02x\n",
				__func__, reg, tmp);
OUT_ASSIGN:
	mutex_unlock(&ri->io_lock);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	return  ret;
}
EXPORT_SYMBOL(rt5734_assign_bit);

static struct mt_chip_conf rt5734_spi_config = {
	/* setuptime, holdtime --> timing for waveform of SPI */
	.setuptime = 3,
	.holdtime = 3,
	/* high_time, low_time --> set SCK */
	.high_time = 10,
	.low_time = 10,
	/* CS pin idle time */
	.cs_idletime = 2,
	/* cpol, cpha -->set type */
	.cpol = 0,
	.cpha = 0,
	/* rx_mlsb, tx_mlsb --> MSB first or not */
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	/* tx_endian, rx_endian -->
	 * Defines whether to reverse the endian order
	 */
	.tx_endian = 0,
	.rx_endian = 0,
	/* com_mod --> FIFO/DMA mode */
	.com_mod = FIFO_TRANSFER,
	/* pause --> if want to always let CS active, set this flag to 1*/
	.pause = 1,
	/* tckdly --> tune timing */
	.tckdly = 0,

	/* ?? */
	.ulthgh_thrsh = 0,
	.cs_pol = 0,
	.sample_sel = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
};

static void rt5734_spi_init(struct spi_device *spi)
{
	pr_info("%s inited\n", __func__);
	spi->bits_per_word = 32;
	spi->controller_data = &rt5734_spi_config;
	mdelay(100);
}

#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static void rt5734_reg_init(struct spi_device *spi)
{
}

static int rt5734_check_id(struct spi_device *spi)
{
	int ret;
	unsigned char data;

	ret = rt5734_read_device(spi, RT5734_CHIPNAME_R, 1, &data);
	if (ret < 0) {
		pr_notice("%s IO fail\n", __func__);
		return -EIO;
	}

	if (data != RT5734_CHIPNAME) {
		pr_notice("%s ID(0x%02x) not match\n", __func__, data);
		return -EINVAL;
	}
	return 0;
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

#if RT5734_IRQ_ENABLE
enum {
	RT5734_EVT_OT_SHUTDOWN_FALLING = 1,
	RT5734_EVT_OT_SHUTDOWN_RISING,
	RT5734_EVT_MAX,
};

static void rt5734_irq_evt_handler(void *info, int eventno)
{
	struct rt5734_chip *chip = info;

	dev_info(chip->dev, "%s eventno = %d\n", __func__, eventno);
	switch (eventno) {
	case RT5734_EVT_OT_SHUTDOWN_RISING:
		pr_notice("%s Enter OT Shutdown\n", __func__);
		break;
	case RT5734_EVT_OT_SHUTDOWN_FALLING:
		pr_notice("%s Exit OT Shutdown\n", __func__);
		break;
	}
}

typedef void (*rt_irq_handler)(void *info, int eventno);

static rt_irq_handler rt5734_handler[RT5734_EVT_MAX] = {
	[RT5734_EVT_OT_SHUTDOWN_RISING] = rt5734_irq_evt_handler,
	[RT5734_EVT_OT_SHUTDOWN_FALLING] = rt5734_irq_evt_handler,
};

static irqreturn_t rt5734_irq_handler(int irqno, void *param)
{
	struct rt5734_chip *chip = param;
	uint32_t regval = 0;
	int ret, i;

	ret = rt5734_read_byte(chip->spi,
			RT5734_FLT_RECORDTEMP_R, &regval);
	if (ret < 0) {
		pr_notice("%s get irq regval fail\n", __func__);
		return IRQ_HANDLED;
	}
	if (regval) {
		pr_info("%s thermal event 0x%02x\n", __func__, regval);
		for (i = 0; i < RT5734_EVT_MAX; i++) {
			if ((regval & (1 << i)) && rt5734_handler[i])
				rt5734_handler[i](chip, i);
		}
	}

	return IRQ_HANDLED;
}

static int rt5734_init_irq(struct rt5734_chip *chip, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;
	uint32_t regval = 0;

	if (np)
		chip->irq = irq_of_parse_and_map(np, 0);
	else {
		pr_notice("%s no dts node\n", __func__);
		return -ENODEV;
	}

	/* mask IRQ first */
	ret = rt5734_write_byte(chip->spi, RT5734_IRQ_MASK_R, 0x83);
	if (ret < 0) {
		pr_notice("%s mask IRQ fail\n", __func__);
		return -EINVAL;
	}
	/* clear IRQ */
	rt5734_read_byte(chip->spi, RT5734_FLT_RECORDTEMP_R, &regval);

	ret = devm_request_threaded_irq(chip->dev, chip->irq,
		rt5734_irq_handler, NULL, IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
		"rt5734-irq", chip);
	if (ret < 0) {
		pr_notice("%s request irq fail\n", __func__);
		return -EINVAL;
	}
	/* unmask IRQ */
	ret = rt5734_write_byte(chip->spi, RT5734_IRQ_MASK_R, 0x00);
	if (ret < 0) {
		pr_notice("%s unmask IRQ fail\n", __func__);
		return -EINVAL;
	}

	enable_irq_wake(chip->irq);
	return 0;
}
#endif /* if RT5734_IRQ_ENABLE */

static int rt5734_spi_probe(struct spi_device *spi)
{
	struct rt5734_chip *chip;
	int ret;

	pr_info("%s\n", __func__);

	chip = devm_kzalloc(&spi->dev,
		sizeof(struct rt5734_chip), GFP_KERNEL);

	chip->spi = spi;
	chip->dev = &spi->dev;
	mutex_init(&chip->io_lock);
#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	spi_set_drvdata(spi, chip);
#else
	pr_info("%s SSPM not need spi_set_drvdata\n", __func__);
#endif /* if not CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	rt5734_spi_init(spi);

#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	ret = rt5734_check_id(spi);
	if (ret < 0)
		return ret;
#else
	pr_info("%s SSPM not need check_id\n", __func__);
#endif /*-- !CONFIG_MTK_TINYSYS_SSPM_SUPPORT--*/

#ifdef CONFIG_RT_REGMAP
	chip->regmap_dev = rt_regmap_device_register_ex(&rt5734_regmap_props,
			&rt5734_regmap_fops, &spi->dev, spi, -1, chip);
	if (!chip->regmap_dev) {
		pr_notice("%s register regmap fail\n", __func__);
		return -EINVAL;
	}
#endif /* #ifdef CONFIG_RT_REGMAP */

#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	rt5734_reg_init(spi);
#else
	pr_info("%s SSPM not need reg_init\n", __func__);
#endif /*-- !CONFIG_MTK_TINYSYS_SSPM_SUPPORT--*/

	ret = rt5734_regulator_init(chip);
	if (ret < 0) {
		RT5734_pr_notice("%s regulator init fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s --OK!!--\n", __func__);
	return 0;

#if RT5734_IRQ_ENABLE
fail_irq:
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->regmap_dev);
#endif /* CONFIG_RT_REGMAP */
#endif /* CONFIG_IRQ_ENABLE */
	return ret;
}

static int rt5734_spi_remove(struct spi_device *spi)
{
	struct rt5734_chip *chip = spi_get_drvdata(spi);

	if (chip) {
		rt5734_regulator_deinit(chip);
		mutex_destroy(&chip->io_lock);
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->regmap_dev);
#endif /* #ifdef CONFIG_RT_REGMAP */
	}
	return 0;
}

static const struct of_device_id rt5734_id_table[] = {
	{.compatible = "mediatek,rt5734",},
};

static struct spi_driver rt5734_spi_driver = {
	.driver = {
		.name = "rt5734",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = rt5734_id_table,
	},
	.probe = rt5734_spi_probe,
	.remove = rt5734_spi_remove,
};

static int __init rt5734_init(void)
{
	pr_notice("%s ver(%s)\n", __func__, RT5734_DRV_VERSION);
	return spi_register_driver(&rt5734_spi_driver);
}

static void __exit rt5734_exit(void)
{
	spi_unregister_driver(&rt5734_spi_driver);
}
subsys_initcall(rt5734_init);
module_exit(rt5734_exit);

MODULE_DESCRIPTION("RT5734 Regulator Driver");
MODULE_VERSION(RT5734_DRV_VERSION);
MODULE_AUTHOR("Sakya <jeff_chang@richtek.com>");
MODULE_LICENSE("GPL");
