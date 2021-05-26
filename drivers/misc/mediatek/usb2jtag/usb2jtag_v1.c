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

#include <linux/slab.h>
#include "usb2jtag_v1.h"
#include <linux/module.h>

static unsigned int usb2jtag_mode_flag;
static int __init setup_usb2jtag_mode(char *str)
{
	usb2jtag_mode_flag = 0;

	if (*str++ != '=' || !*str)
		/*
		 * No options specified. Switch on full debugging.
		 */
		goto out;

	switch (*str) {
	case '0':
		usb2jtag_mode_flag = 0;
		pr_debug("disable usb2jtag\n");
		break;
	case '1':
		usb2jtag_mode_flag = 1;
		pr_debug("enable usb2jtag\n");
		break;
	default:
		pr_err("usb2jtag option '%c' unknown. skipped\n", *str);
	}
out:
	return 0;

}
__setup("usb2jtag_mode", setup_usb2jtag_mode);

unsigned int usb2jtag_mode(void)
{
	return usb2jtag_mode_flag;
}

static struct mtk_usb2jtag_driver mtk_usb2jtag_drv = {
	.usb2jtag_init = NULL,
	.usb2jtag_resume = NULL,
	.usb2jtag_suspend = NULL,
};

struct mtk_usb2jtag_driver *get_mtk_usb2jtag_drv(void)
{
	return &mtk_usb2jtag_drv;
}

static int mtk_usb2jtag_resume_default(void)
{
	return (usb2jtag_mode()) ?
		mtk_usb2jtag_drv.usb2jtag_init() : 0;
}

int mtk_usb2jtag_resume(void)
{
	return (mtk_usb2jtag_drv.usb2jtag_resume) ?
		mtk_usb2jtag_drv.usb2jtag_resume() :
		mtk_usb2jtag_resume_default();
}

static int __init mtk_usb2jtag_init(void)
{
	return (usb2jtag_mode() && mtk_usb2jtag_drv.usb2jtag_init) ?
		mtk_usb2jtag_drv.usb2jtag_init() : -1;
}

static void __exit mtk_usb2jtag_exit(void)
{
}

module_init(mtk_usb2jtag_init);
module_exit(mtk_usb2jtag_exit);
