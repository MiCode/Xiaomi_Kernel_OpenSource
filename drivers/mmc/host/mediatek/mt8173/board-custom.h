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

#ifndef __ARCH_ARM_MACH_MT6575_CUSTOM_BOARD_H
#define __ARCH_ARM_MACH_MT6575_CUSTOM_BOARD_H

#include <generated/autoconf.h>

/*=======================================================================*/
/* MT6575 SD                                                             */
/*=======================================================================*/
#ifdef CONFIG_FPGA_EARLY_PORTING
/*#define CFG_DEV_MSDC0*/
#else
#ifdef CONFIG_MTK_EMMC_SUPPORT
/*#define CFG_DEV_MSDC0*/
#endif
/*#define CFG_DEV_MSDC1*/
#endif
/*#define CFG_DEV_MSDC1*/
/*#define CFG_DEV_MSDC2*/

#define CFG_DEV_MSDC3

/*
SDIO slot index number used by connectivity combo chip: may be MSDC0~3
*/
#define CONFIG_MTK_COMBO_SDIO_SLOT  (3)

#ifdef CONFIG_MTK_COMBO_SDIO_SLOT
#include "mtk_wcn_cmb_stub.h"
#endif

#define NFI_DEFAULT_ACCESS_TIMING        (0x44333)

/*uboot only support 1 cs*/
#define NFI_CS_NUM                  (2)
#define NFI_DEFAULT_CS				(0)

#define USE_AHB_MODE                (1)

#define PLATFORM_EVB                (1)

#endif				/* __ARCH_ARM_MACH_MT6575_CUSTOM_BOARD_H */
