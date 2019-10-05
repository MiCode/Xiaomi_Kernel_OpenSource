/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6885-afe-gpio.h  --  Mediatek 6885 afe gpio ctrl definition
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Shane Chien <shane.chien@mediatek.com>
 */

#ifndef _MT6885_AFE_GPIO_H_
#define _MT6885_AFE_GPIO_H_

struct mtk_base_afe;

int mt6885_afe_gpio_init(struct mtk_base_afe *afe);

int mt6885_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink);

#endif
