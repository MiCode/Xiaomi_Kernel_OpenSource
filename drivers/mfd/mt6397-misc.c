// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/rtc_misc.h>
#include <linux/pm.h>

#define RTC_BBPU		0x0000
#define RTC_BBPU_CBUSY		BIT(6)

#define RTC_WRTGR		0x003c

#define RTC_AL_HOU		0x001c
#define RTC_PDN1		0x002c
#define RTC_PDN2		0x002e
#define RTC_SPAR0		0x0030
#define RTC_CON			0x003e
#define RTC_BBPU_KEY		(0x43 << 8)
#define RTC_BBPU_AUTO		BIT(3)
#define RTC_BBPU_PWREN		BIT(0)
#define RTC_CON_F32KOB		BIT(5)
#define RTC_GPIO_USER_MASK	0x1f00

static const u16 rtc_spare_reg[][3] = {
	{RTC_AL_HOU, 0x7f, 8},
	{RTC_PDN1, 0xf, 0},
	{RTC_PDN1, 0x3, 4},
	{RTC_PDN1, 0x1, 6},
	{RTC_PDN1, 0x1, 7},
	{RTC_PDN1, 0x1, 13},
	{RTC_PDN1, 0x1, 14},
	{RTC_PDN1, 0x1, 15},
	{RTC_PDN2, 0x1, 4},
	{RTC_PDN2, 0x3, 5},
	{RTC_PDN2, 0x1, 7},
	{RTC_PDN2, 0x1, 15},
	{RTC_SPAR0, 0x1, 6},
	{RTC_SPAR0, 0x1, 7}
};

enum rtc_spare_enum {
	RTC_FGSOC = 0,
	RTC_ANDROID,
	RTC_FAC_RESET,
	RTC_BYPASS_PWR,
	RTC_PWRON_TIME,
	RTC_FAST_BOOT,
	RTC_KPOC,
	RTC_DEBUG,
	RTC_PWRON_AL,
	RTC_UART,
	RTC_AUTOBOOT,
	RTC_PWRON_LOGO,
	RTC_32K_LESS,
	RTC_LP_DET,
	RTC_SPAR_NUM
};

enum rtc_reg_set {
	RTC_REG,
	RTC_MASK,
	RTC_SHIFT
};

struct mt6397_misc {
	struct device	*dev;
	struct mutex	lock;
	struct regmap	*regmap;
	u32		addr_base;
};

static struct mt6397_misc *rtc_misc;

static int mtk_rtc_write_trigger(void)
{
	unsigned long timeout = jiffies + HZ;
	int ret;
	u32 data;

	ret = regmap_write(rtc_misc->regmap,
			   rtc_misc->addr_base + RTC_WRTGR, 1);
	if (ret < 0)
		return ret;

	while (1) {
		ret = regmap_read(rtc_misc->regmap,
				  rtc_misc->addr_base + RTC_BBPU, &data);
		if (ret < 0)
			break;
		if (!(data & RTC_BBPU_CBUSY))
			break;
		if (time_after(jiffies, timeout)) {
			ret = -ETIMEDOUT;
			break;
		}
		cpu_relax();
	}

	return ret;
}

static u32 __mtk_misc_get_spare_register(enum rtc_spare_enum cmd)
{
	int ret;
	u32 data;

	if (cmd >= 0 && cmd < RTC_SPAR_NUM) {
		ret = regmap_read(rtc_misc->regmap,
				  rtc_misc->addr_base
				  + rtc_spare_reg[cmd][RTC_REG], &data);

		data = (data >> rtc_spare_reg[cmd][RTC_SHIFT]) &
				rtc_spare_reg[cmd][RTC_MASK];
		return data;
	}
	return -EINVAL;
}

static void __mtk_misc_set_spare_register(enum rtc_spare_enum cmd, u32 val)
{
	int ret;
	u32 data, mask;

	if (cmd >= 0 && cmd < RTC_SPAR_NUM) {
		data = val << rtc_spare_reg[cmd][RTC_SHIFT];
		mask = rtc_spare_reg[cmd][RTC_MASK]
			<< rtc_spare_reg[cmd][RTC_SHIFT];
		ret = regmap_update_bits(rtc_misc->regmap,
			rtc_misc->addr_base + rtc_spare_reg[cmd][RTC_REG],
			mask, data);
		if (ret < 0)
			dev_err(rtc_misc->dev, "regmap write error!!!\n");

		mtk_rtc_write_trigger();
	}
}


u32 mtk_misc_get_spare_fg_value(void)
{
	u32 data;

	mutex_lock(&rtc_misc->lock);
	data = __mtk_misc_get_spare_register(RTC_FGSOC);
	mutex_unlock(&rtc_misc->lock);
	return data;
}

int mtk_misc_set_spare_fg_value(u32 val)
{
	if (val > 100)
		return 1;

	mutex_lock(&rtc_misc->lock);
	__mtk_misc_set_spare_register(RTC_FGSOC, val);
	mutex_unlock(&rtc_misc->lock);
	return 0;
}

bool mtk_misc_crystal_exist_status(void)
{
	u32 ret;

	mutex_lock(&rtc_misc->lock);
	ret = __mtk_misc_get_spare_register(RTC_32K_LESS);
	mutex_unlock(&rtc_misc->lock);
	return !!ret;
}
EXPORT_SYMBOL(mtk_misc_crystal_exist_status);

static void mtk_misc_set_gpio_32k_status(enum rtc_gpio_user_t user, bool enable)
{
	u32 pdn1, temp, con;
	int ret;

	ret = regmap_read(rtc_misc->regmap,
			  rtc_misc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;
	ret = regmap_read(rtc_misc->regmap,
			  rtc_misc->addr_base + RTC_CON, &con);
	if (ret < 0)
		goto exit;

	if (!enable) {
		temp = pdn1 & ~(1 << user);
		ret = regmap_write(rtc_misc->regmap,
				rtc_misc->addr_base + RTC_PDN1, temp);
		if (ret < 0)
			goto exit;
		mtk_rtc_write_trigger();
		if (!(pdn1 & RTC_GPIO_USER_MASK))
			con |= RTC_CON_F32KOB;
	} else {
		con &= ~RTC_CON_F32KOB;
		pdn1 |= (1 << user);
		ret = regmap_write(rtc_misc->regmap,
				rtc_misc->addr_base + RTC_PDN1, pdn1);
		if (ret < 0)
			goto exit;
		mtk_rtc_write_trigger();
	}

	ret = regmap_write(rtc_misc->regmap,
			rtc_misc->addr_base + RTC_CON, con);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger();

	return;
exit:
	dev_err(rtc_misc->dev, "regmap write/read error!!!\n");
}

void rtc_gpio_enable_32k(enum rtc_gpio_user_t user)
{
	if (user < RTC_GPIO_USER_WIFI || user > RTC_GPIO_USER_PMIC)
		return;
	dev_info(rtc_misc->dev, "enable 32k clock output!!!\n");

	mutex_lock(&rtc_misc->lock);
	mtk_misc_set_gpio_32k_status(user, true);
	mutex_unlock(&rtc_misc->lock);
}
EXPORT_SYMBOL(rtc_gpio_enable_32k);

void rtc_gpio_disable_32k(enum rtc_gpio_user_t user)
{
	if (user < RTC_GPIO_USER_WIFI || user > RTC_GPIO_USER_PMIC)
		return;

	mutex_lock(&rtc_misc->lock);
	mtk_misc_set_gpio_32k_status(user, false);
	mutex_unlock(&rtc_misc->lock);
}
EXPORT_SYMBOL(rtc_gpio_disable_32k);

bool mtk_misc_low_power_detected(void)
{
	u32 ret;

	mutex_lock(&rtc_misc->lock);
	ret = __mtk_misc_get_spare_register(RTC_LP_DET);
	mutex_unlock(&rtc_misc->lock);
	return !!ret;
}
EXPORT_SYMBOL(mtk_misc_low_power_detected);

void mtk_misc_mark_recovery(void)
{
	mutex_lock(&rtc_misc->lock);
	__mtk_misc_set_spare_register(RTC_FAC_RESET, 0x1);
	mutex_unlock(&rtc_misc->lock);
}

void mtk_misc_mark_fast(void)
{
	mutex_lock(&rtc_misc->lock);
	__mtk_misc_set_spare_register(RTC_FAST_BOOT, 0x1);
	mutex_unlock(&rtc_misc->lock);
}

static void mt_power_off(void)
{
	u32 bbpu;
	int ret;

	mutex_lock(&rtc_misc->lock);
	bbpu = RTC_BBPU_KEY | RTC_BBPU_AUTO | RTC_BBPU_PWREN;
	ret = regmap_write(rtc_misc->regmap,
			rtc_misc->addr_base + RTC_BBPU, bbpu);
	if (ret < 0)
		dev_err(rtc_misc->dev, "regmap write error!!!\n");
	mtk_rtc_write_trigger();
	mutex_unlock(&rtc_misc->lock);
}

static int mt6397_misc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *np = pdev->dev.parent->of_node;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6397_misc *misc;
	int pm_off = 0;

	if (!np)
		return -EINVAL;

	misc = devm_kzalloc(&pdev->dev, sizeof(struct mt6397_misc), GFP_KERNEL);
	if (!misc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	misc->addr_base = res->start;

	misc->regmap = mt6397_chip->regmap;
	misc->dev = &pdev->dev;
	mutex_init(&misc->lock);
	rtc_misc = misc;
	platform_set_drvdata(pdev, misc);

	pm_off = of_property_read_bool(np,
				"mediatek,system-power-controller");
	if (pm_off && !pm_power_off)
		pm_power_off = mt_power_off;

	return 0;
}

static const struct of_device_id mt6397_misc_of_match[] = {
	{ .compatible = "mediatek,mt6397-misc", },
	{ .compatible = "mediatek,mt6323-misc", },
	{ .compatible = "mediatek,mt6392-misc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6397_misc_of_match);

static struct platform_driver mt6397_misc_driver = {
	.driver = {
		.name = "mt6397-misc",
		.of_match_table = mt6397_misc_of_match,
	},
	.probe	= mt6397_misc_probe,
};

module_platform_driver(mt6397_misc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tianping Fang <tianping.fang@mediatek.com>");
MODULE_DESCRIPTION("Misc Driver for MediaTek MT6323 PMIC");
MODULE_ALIAS("platform:mt6397-misc");
