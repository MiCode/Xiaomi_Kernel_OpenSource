/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_UDI_MT6885__
#define __MTK_UDI_MT6885__

#include <linux/kernel.h>

/* CPU UDI pin mux ADDR */
/* 1. Write 0x10005360= 44044000 */
/* 2. Write 0x10005370= 00000004 */

#ifdef __KERNEL__
#define UDIPIN_BASE				(udipin_base)
#else
#define UDIPIN_BASE				0x10005000
#endif

/* 0x10005000 0x1000, UDI pinmux reg */
#define UDIPIN_UDI_MUX1			(UDIPIN_BASE+0x360)
#define UDIPIN_UDI_MUX1_VALUE	(0x44044000)
#define UDIPIN_UDI_MUX2			(UDIPIN_BASE+0x370)
#define UDIPIN_UDI_MUX2_VALUE	(0x00000004)



#endif /* __MTK_UDI_MT6885__ */

