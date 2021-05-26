/*
 * Copyright (C) 2020 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT_RT5738_H
#define __MT_RT5738_H

/* Custom RT5738_NAME */
#define RT5738_NAME_0		"ext_buck_lp4"
#define	RT5738_NAME_1		"ext_buck_lp4x"
#define	RT5738_NAME_2		"ext_buck_vgpu"
/*#define RT5738_IS_EXIST_NAME	RT5738_NAME_2 */


/* RT5738 operation register */
#define RT5738_REG_VSEL0	(0x00)
#define RT5738_REG_VSEL1	(0x01)
#define RT5738_REG_CTRL1	(0x02)
#define RT5738_REG_ID1		(0x03)
#define RT5738_REG_ID2		(0x04)
#define RT5738_REG_MONITOR	(0x05)
#define RT5738_REG_CTRL2	(0x06)
#define RT5738_REG_CTRL3	(0x07)
#define RT5738_REG_CTRL4	(0x08)
/* Hidden mode */
/* #define RT5738_REG_CTRL5	(0x09) */



#if defined(RT5738_NAME_0)
#define RT5738_CMPT_STR_0	"mediatek,ext_buck_lp4"
#define RT5738_VSEL_0		RT5738_REG_VSEL0
#define RT5738_CTRL_0		RT5738_REG_CTRL1
#define	RT5738_CTRL_BIT_0	0x01
#define	RT5738_EN_0		RT5738_REG_MONITOR
#define	RT5738_EN_BIT_0		0x01
#endif

#if defined(RT5738_NAME_1)
#define RT5738_CMPT_STR_1	"mediatek,ext_buck_lp4x"
#define RT5738_VSEL_1		RT5738_REG_VSEL1
#define RT5738_CTRL_1		RT5738_REG_CTRL1
#define	RT5738_CTRL_BIT_1	0x02
#define	RT5738_EN_1		RT5738_REG_MONITOR
#define	RT5738_EN_BIT_1		0x02
#endif

#if defined(RT5738_NAME_2)
#define RT5738_CMPT_STR_2	"mediatek,ext_buck_vgpu"
#define RT5738_VSEL_2		RT5738_REG_VSEL1
#define RT5738_CTRL_2		RT5738_REG_CTRL1
#define	RT5738_CTRL_BIT_2	0x01
#define	RT5738_EN_2		RT5738_REG_MONITOR
#define	RT5738_EN_BIT_2		0x01
#endif

#endif /*--__MT_RT5738_H--*/
