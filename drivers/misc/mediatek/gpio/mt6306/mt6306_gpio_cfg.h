/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef MT6306_GPIO_CFG
#define MT6306_GPIO_CFG
struct mtk_pin_info {
	unsigned char pin;
	unsigned char offset;
	unsigned char bit;
	unsigned char width;
	unsigned char ip_num;
};

#define MTK_PIN_INFO(_pin, _offset, _bit, _width, _ip_num)	\
	{\
		.pin = _pin, \
		.offset = _offset, \
		.bit = _bit, \
		.width = _width, \
		.ip_num = _ip_num, \
	}
struct mtk_pin_info mtk_pin_6306_dir[] = {
	MTK_PIN_INFO(1, 0x01, 2, 1, 0),
	MTK_PIN_INFO(2, 0x00, 2, 1, 0),
	MTK_PIN_INFO(3, 0x08, 0, 1, 0),
	MTK_PIN_INFO(4, 0x01, 3, 1, 0),
	MTK_PIN_INFO(5, 0x00, 3, 1, 0),
	MTK_PIN_INFO(6, 0x08, 1, 1, 0),
	MTK_PIN_INFO(7, 0x05, 2, 1, 0),
	MTK_PIN_INFO(8, 0x04, 2, 1, 0),
	MTK_PIN_INFO(9, 0x08, 2, 1, 0),
	MTK_PIN_INFO(10, 0x05, 3, 1, 0),
	MTK_PIN_INFO(11, 0x04, 3, 1, 0),
	MTK_PIN_INFO(12, 0x08, 3, 1, 0),
};

struct mtk_pin_info mtk_pin_6306_datain[] = {
	MTK_PIN_INFO(1, 0x0A, 0, 1, 0),
	MTK_PIN_INFO(2, 0x0B, 0, 1, 0),
	MTK_PIN_INFO(3, 0x0C, 0, 1, 0),
	MTK_PIN_INFO(4, 0x0A, 1, 1, 0),
	MTK_PIN_INFO(5, 0x0B, 1, 1, 0),
	MTK_PIN_INFO(6, 0x0C, 1, 1, 0),
	MTK_PIN_INFO(7, 0x0A, 2, 1, 0),
	MTK_PIN_INFO(8, 0x0B, 2, 1, 0),
	MTK_PIN_INFO(9, 0x0C, 2, 1, 0),
	MTK_PIN_INFO(10, 0x0A, 3, 1, 0),
	MTK_PIN_INFO(11, 0x0B, 3, 1, 0),
	MTK_PIN_INFO(12, 0x0C, 3, 1, 0),
	MTK_PIN_INFO(13, 0x0D, 2, 1, 0),
	MTK_PIN_INFO(14, 0x0D, 0, 1, 0),
	MTK_PIN_INFO(15, 0x0E, 0, 1, 0),
	MTK_PIN_INFO(16, 0x0D, 3, 1, 0),
	MTK_PIN_INFO(17, 0x0D, 1, 1, 0),
	MTK_PIN_INFO(18, 0x0E, 1, 1, 0),
};

struct mtk_pin_info mtk_pin_6306_dataout[] = {
	MTK_PIN_INFO(1, 0x01, 0, 1, 0),
	MTK_PIN_INFO(2, 0x00, 0, 1, 0),
	MTK_PIN_INFO(3, 0x02, 0, 1, 0),
	MTK_PIN_INFO(4, 0x01, 1, 1, 0),
	MTK_PIN_INFO(5, 0x00, 1, 1, 0),
	MTK_PIN_INFO(6, 0x02, 1, 1, 0),
	MTK_PIN_INFO(7, 0x05, 0, 1, 0),
	MTK_PIN_INFO(8, 0x04, 0, 1, 0),
	MTK_PIN_INFO(9, 0x06, 0, 1, 0),
	MTK_PIN_INFO(10, 0x05, 1, 1, 0),
	MTK_PIN_INFO(11, 0x04, 1, 1, 0),
	MTK_PIN_INFO(12, 0x06, 1, 1, 0),
};
#endif	/*MT6306_GPIO_CFG*/
