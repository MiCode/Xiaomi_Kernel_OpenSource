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

#ifndef __MTK_UDI_MT6768__
#define __MTK_UDI_MT6768__

#include <linux/kernel.h>

/* CPU UDI pin mux ADDR */
/* 1. Write 0x10005440= 00033300 */
/* 2. Write 0x10005450= 00003300 */


#ifdef CONFIG_OF
#define DEVICE_GPIO "mediatek,gpio"
/* 0x10005000 0x1000, UDI pinmux reg */
static void __iomem  *udipin_base;
#endif

#ifdef __KERNEL__
#define UDIPIN_BASE				(udipin_base)
#else
#define UDIPIN_BASE				0x10005000
#endif

/* 0x10005000 0x1000, UDI pinmux reg */
#define UDIPIN_UDI_MUX1			(UDIPIN_BASE+0x440)
#define UDIPIN_UDI_MUX1_VALUE	(0x00033300)
#define UDIPIN_UDI_MUX2			(UDIPIN_BASE+0x450)
#define UDIPIN_UDI_MUX2_VALUE	(0x00003300)



#endif /* __MTK_UDI_MT6768__ */

