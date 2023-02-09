// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6886-afe-gpio.c  --  Mediatek 6886 afe gpio ctrl
 *
 *  Copyright (c) 2021 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>

#include "mt6886-afe-common.h"
#include "mt6886-afe-gpio.h"

struct pinctrl *aud_pinctrl;
struct audio_gpio_attr {
	const char *name;
	bool gpio_prepare;
	struct pinctrl_state *gpioctrl;
};

static struct audio_gpio_attr aud_gpios[MT6886_AFE_GPIO_GPIO_NUM] = {
	[MT6886_AFE_GPIO_DAT_MISO0_OFF] = {"aud-dat-miso0-off", false, NULL},
	[MT6886_AFE_GPIO_DAT_MISO0_ON] = {"aud-dat-miso0-on", false, NULL},
	[MT6886_AFE_GPIO_DAT_MISO1_OFF] = {"aud-dat-miso1-off", false, NULL},
	[MT6886_AFE_GPIO_DAT_MISO1_ON] = {"aud-dat-miso1-on", false, NULL},
	[MT6886_AFE_GPIO_DAT_MISO2_OFF] = {"aud-dat-miso2-off", false, NULL},
	[MT6886_AFE_GPIO_DAT_MISO2_ON] = {"aud-dat-miso2-on", false, NULL},
	[MT6886_AFE_GPIO_DAT_MOSI_OFF] = {"aud-dat-mosi-off", false, NULL},
	[MT6886_AFE_GPIO_DAT_MOSI_ON] = {"aud-dat-mosi-on", false, NULL},
	[MT6886_AFE_GPIO_I2S0_OFF] = {"aud-gpio-i2s0-off", false, NULL},
	[MT6886_AFE_GPIO_I2S0_ON] = {"aud-gpio-i2s0-on", false, NULL},
	[MT6886_AFE_GPIO_I2S1_OFF] = {"aud-gpio-i2s1-off", false, NULL},
	[MT6886_AFE_GPIO_I2S1_ON] = {"aud-gpio-i2s1-on", false, NULL},
	[MT6886_AFE_GPIO_I2S2_OFF] = {"aud-gpio-i2s2-off", false, NULL},
	[MT6886_AFE_GPIO_I2S2_ON] = {"aud-gpio-i2s2-on", false, NULL},
	[MT6886_AFE_GPIO_I2S3_OFF] = {"aud-gpio-i2s3-off", false, NULL},
	[MT6886_AFE_GPIO_I2S3_ON] = {"aud-gpio-i2s3-on", false, NULL},
	[MT6886_AFE_GPIO_I2S5_OFF] = {"aud-gpio-i2s5-off", false, NULL},
	[MT6886_AFE_GPIO_I2S5_ON] = {"aud-gpio-i2s5-on", false, NULL},
	[MT6886_AFE_GPIO_I2S6_OFF] = {"aud-gpio-i2s6-off", false, NULL},
	[MT6886_AFE_GPIO_I2S6_ON] = {"aud-gpio-i2s6-on", false, NULL},
	[MT6886_AFE_GPIO_I2S7_OFF] = {"aud-gpio-i2s7-off", false, NULL},
	[MT6886_AFE_GPIO_I2S7_ON] = {"aud-gpio-i2s7-on", false, NULL},
	[MT6886_AFE_GPIO_I2S8_OFF] = {"aud-gpio-i2s8-off", false, NULL},
	[MT6886_AFE_GPIO_I2S8_ON] = {"aud-gpio-i2s8-on", false, NULL},
	[MT6886_AFE_GPIO_I2S9_OFF] = {"aud-gpio-i2s9-off", false, NULL},
	[MT6886_AFE_GPIO_I2S9_ON] = {"aud-gpio-i2s9-on", false, NULL},
	[MT6886_AFE_GPIO_VOW_DAT_OFF] = {"vow-dat-miso-off", false, NULL},
	[MT6886_AFE_GPIO_VOW_DAT_ON] = {"vow-dat-miso-on", false, NULL},
	[MT6886_AFE_GPIO_VOW_CLK_OFF] = {"vow-clk-miso-off", false, NULL},
	[MT6886_AFE_GPIO_VOW_CLK_ON] = {"vow-clk-miso-on", false, NULL},
	[MT6886_AFE_GPIO_DAT_MOSI_CH34_OFF] = {"aud-dat-mosi-ch34-off",
		false, NULL
	},
	[MT6886_AFE_GPIO_DAT_MOSI_CH34_ON] = {"aud-dat-mosi-ch34-on",
		false, NULL
	},
};

static DEFINE_MUTEX(gpio_request_mutex);

int mt6886_afe_gpio_init(struct mtk_base_afe *afe)
{
	int ret;
	int i = 0;

	aud_pinctrl = devm_pinctrl_get(afe->dev);
	if (IS_ERR(aud_pinctrl)) {
		ret = PTR_ERR(aud_pinctrl);
		dev_err(afe->dev, "%s(), ret %d, cannot get aud_pinctrl!\n",
			__func__, ret);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(aud_gpios); i++) {
		aud_gpios[i].gpioctrl = pinctrl_lookup_state(aud_pinctrl,
					aud_gpios[i].name);
		if (IS_ERR(aud_gpios[i].gpioctrl)) {
			ret = PTR_ERR(aud_gpios[i].gpioctrl);
			dev_err(afe->dev, "%s(), pinctrl_lookup_state %s fail, ret %d\n",
				__func__, aud_gpios[i].name, ret);
		} else
			aud_gpios[i].gpio_prepare = true;
	}

	/* gpio status init */
	mt6886_afe_gpio_request(afe, false, MT6886_DAI_ADDA, 0);
	mt6886_afe_gpio_request(afe, false, MT6886_DAI_ADDA, 1);

	return 0;
}

static int mt6886_afe_gpio_select(struct mtk_base_afe *afe,
				  enum mt6886_afe_gpio type)
{
	int ret = 0;

	if (type < MT6886_AFE_GPIO_DAT_MISO0_OFF || type >= MT6886_AFE_GPIO_GPIO_NUM) {
		dev_err(afe->dev, "%s(), error, invalid gpio type %d\n",
			__func__, type);
		return -EINVAL;
	}

	if (!aud_gpios[type].gpio_prepare) {
		dev_info(afe->dev, "%s(), error, gpio type %d not prepared\n",
			 __func__, type);
		return -EIO;
	}

	ret = pinctrl_select_state(aud_pinctrl,
				   aud_gpios[type].gpioctrl);
	if (ret)
		dev_err(afe->dev, "%s(), error, can not set gpio type %d\n",
			__func__, type);

	return ret;
}

static int mt6886_afe_gpio_adda_dl(struct mtk_base_afe *afe, bool enable)
{
	if (enable)
		return mt6886_afe_gpio_select(afe,
					      MT6886_AFE_GPIO_DAT_MOSI_ON);
	else
		return mt6886_afe_gpio_select(afe,
					      MT6886_AFE_GPIO_DAT_MOSI_OFF);
}

static int mt6886_afe_gpio_adda_ul(struct mtk_base_afe *afe, bool enable)
{
	int ret = 0;

	if (mt6886_afe_gpio_is_prepared(MT6886_AFE_GPIO_DAT_MISO0_ON)) {
		ret = mt6886_afe_gpio_select(afe, enable ?
					     MT6886_AFE_GPIO_DAT_MISO0_ON :
					     MT6886_AFE_GPIO_DAT_MISO0_OFF);
		/* if error happened, skip miso1 select */
		if (ret)
			return ret;
	}

	if (mt6886_afe_gpio_is_prepared(MT6886_AFE_GPIO_DAT_MISO1_ON))
		ret = mt6886_afe_gpio_select(afe, enable ?
					     MT6886_AFE_GPIO_DAT_MISO1_ON :
					     MT6886_AFE_GPIO_DAT_MISO1_OFF);

	return ret;
}

static int mt6886_afe_gpio_adda_ch34_dl(struct mtk_base_afe *afe, bool enable)
{
	if (enable)
		return mt6886_afe_gpio_select(afe,
					      MT6886_AFE_GPIO_DAT_MOSI_CH34_ON);
	else
		return mt6886_afe_gpio_select(afe,
					      MT6886_AFE_GPIO_DAT_MOSI_CH34_OFF);
}

static int mt6886_afe_gpio_adda_ch34_ul(struct mtk_base_afe *afe, bool enable)
{
	if (enable)
		return mt6886_afe_gpio_select(afe,
					      MT6886_AFE_GPIO_DAT_MISO2_ON);
	else
		return mt6886_afe_gpio_select(afe,
					      MT6886_AFE_GPIO_DAT_MISO2_OFF);
}

int mt6886_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink)
{
	mutex_lock(&gpio_request_mutex);
	switch (dai) {
	case MT6886_DAI_ADDA:
		if (uplink)
			mt6886_afe_gpio_adda_ul(afe, enable);
		else
			mt6886_afe_gpio_adda_dl(afe, enable);
		break;
	case MT6886_DAI_ADDA_CH34:
		if (uplink)
			mt6886_afe_gpio_adda_ch34_ul(afe, enable);
		else
			mt6886_afe_gpio_adda_ch34_dl(afe, enable);
		break;
	case MT6886_DAI_I2S_0:
		if (enable)
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S0_ON);
		else
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S0_OFF);
		break;
	case MT6886_DAI_I2S_1:
		if (enable)
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S1_ON);
		else
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S1_OFF);
		break;
	case MT6886_DAI_I2S_2:
		if (enable)
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S2_ON);
		else
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S2_OFF);
		break;
	case MT6886_DAI_I2S_3:
		if (enable)
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S3_ON);
		else
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S3_OFF);
		break;
	case MT6886_DAI_I2S_5:
		if (enable)
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S5_ON);
		else
			mt6886_afe_gpio_select(afe, MT6886_AFE_GPIO_I2S5_OFF);
		break;
	case MT6886_DAI_VOW:
		if (enable) {
			mt6886_afe_gpio_select(afe,
					       MT6886_AFE_GPIO_VOW_CLK_ON);
			mt6886_afe_gpio_select(afe,
					       MT6886_AFE_GPIO_VOW_DAT_ON);
		} else {
			mt6886_afe_gpio_select(afe,
					       MT6886_AFE_GPIO_VOW_CLK_OFF);
			mt6886_afe_gpio_select(afe,
					       MT6886_AFE_GPIO_VOW_DAT_OFF);
		}
		break;
	default:
		mutex_unlock(&gpio_request_mutex);
		dev_info(afe->dev, "%s(), invalid dai %d\n", __func__, dai);
		return -EINVAL;
	}
	mutex_unlock(&gpio_request_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(mt6886_afe_gpio_request);

bool mt6886_afe_gpio_is_prepared(enum mt6886_afe_gpio type)
{
	return aud_gpios[type].gpio_prepare;
}
EXPORT_SYMBOL(mt6886_afe_gpio_is_prepared);

