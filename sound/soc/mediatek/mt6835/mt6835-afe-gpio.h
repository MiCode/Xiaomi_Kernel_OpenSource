/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6835-afe-gpio.h  --  Mediatek 6835 afe gpio ctrl definition
 *
 *  Copyright (c) 2021 MediaTek Inc.
 *  Author: Shu-wei Hsu <shu-wei.Hsu@mediatek.com>
 */

#ifndef _MT6835_AFE_GPIO_H_
#define _MT6835_AFE_GPIO_H_

enum mt6835_afe_gpio {
	MT6835_AFE_GPIO_DAT_MISO0_OFF,
	MT6835_AFE_GPIO_DAT_MISO0_ON,
	MT6835_AFE_GPIO_DAT_MISO1_OFF,
	MT6835_AFE_GPIO_DAT_MISO1_ON,
	MT6835_AFE_GPIO_DAT_MOSI_OFF,
	MT6835_AFE_GPIO_DAT_MOSI_ON,
	MT6835_AFE_GPIO_I2S0_OFF,
	MT6835_AFE_GPIO_I2S0_ON,
	MT6835_AFE_GPIO_I2S1_OFF,
	MT6835_AFE_GPIO_I2S1_ON,
	MT6835_AFE_GPIO_I2S2_OFF,
	MT6835_AFE_GPIO_I2S2_ON,
	MT6835_AFE_GPIO_I2S3_OFF,
	MT6835_AFE_GPIO_I2S3_ON,
	MT6835_AFE_GPIO_I2S5_OFF,
	MT6835_AFE_GPIO_I2S5_ON,
	MT6835_AFE_GPIO_GPIO_NUM,
};

struct mtk_base_afe;

int mt6835_afe_gpio_init(struct mtk_base_afe *afe);
int mt6835_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink);
bool mt6835_afe_gpio_is_prepared(enum mt6835_afe_gpio type);

#endif
