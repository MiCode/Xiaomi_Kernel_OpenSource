/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6833-afe-gpio.h  --  Mediatek 6833 afe gpio ctrl definition
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Eason Yen <eason.yen@mediatek.com>
 */

#ifndef _MT6833_AFE_GPIO_H_
#define _MT6833_AFE_GPIO_H_

struct mtk_base_afe;

int mt6833_afe_gpio_init(struct mtk_base_afe *afe);

int mt6833_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink);

#endif
