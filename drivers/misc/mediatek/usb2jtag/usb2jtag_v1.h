/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __USB2JTAG_H_
#define __USB2JTAG_H_
unsigned int usb2jtag_mode(void);
struct mtk_usb2jtag_driver *get_mtk_usb2jtag_drv(void);
struct mtk_usb2jtag_driver {
	int	(*usb2jtag_init)(void);
	int	(*usb2jtag_resume)(void);
	int	(*usb2jtag_suspend)(void);
};

extern struct clk *musb_clk;
extern int usb2jtag_usb_init(void);
#endif
