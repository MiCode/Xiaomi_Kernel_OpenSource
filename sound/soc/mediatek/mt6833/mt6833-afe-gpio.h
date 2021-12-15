/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6833-afe-gpio.h  --  Mediatek 6833 afe gpio ctrl definition
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Eason Yen <eason.yen@mediatek.com>
 */

#ifndef _MT6833_AFE_GPIO_H_
#define _MT6833_AFE_GPIO_H_

enum mt6833_afe_gpio {
	MT6833_AFE_GPIO_DAT_MISO0_OFF,
	MT6833_AFE_GPIO_DAT_MISO0_ON,
	MT6833_AFE_GPIO_DAT_MISO1_OFF,
	MT6833_AFE_GPIO_DAT_MISO1_ON,
	MT6833_AFE_GPIO_DAT_MISO2_OFF,
	MT6833_AFE_GPIO_DAT_MISO2_ON,
	MT6833_AFE_GPIO_DAT_MOSI_OFF,
	MT6833_AFE_GPIO_DAT_MOSI_ON,
	MT6833_AFE_GPIO_I2S0_OFF,
	MT6833_AFE_GPIO_I2S0_ON,
	MT6833_AFE_GPIO_I2S1_OFF,
	MT6833_AFE_GPIO_I2S1_ON,
	MT6833_AFE_GPIO_I2S2_OFF,
	MT6833_AFE_GPIO_I2S2_ON,
	MT6833_AFE_GPIO_I2S3_OFF,
	MT6833_AFE_GPIO_I2S3_ON,
	MT6833_AFE_GPIO_I2S5_OFF,
	MT6833_AFE_GPIO_I2S5_ON,
	MT6833_AFE_GPIO_VOW_DAT_OFF,
	MT6833_AFE_GPIO_VOW_DAT_ON,
	MT6833_AFE_GPIO_VOW_CLK_OFF,
	MT6833_AFE_GPIO_VOW_CLK_ON,
	MT6833_AFE_GPIO_GPIO_NUM
};

struct mtk_base_afe;

int mt6833_afe_gpio_init(struct mtk_base_afe *afe);

int mt6833_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink);

bool mt6833_afe_gpio_is_prepare(enum mt6833_afe_gpio type);

#endif
