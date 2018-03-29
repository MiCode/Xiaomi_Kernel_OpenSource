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

#ifndef __MT_DVFSRC_REG__H__
#define __MT_DVFSRC_REG__H__

#include <linux/io.h>

extern void __iomem *dvfsrc_base;
#define DVFSRC_BASE (dvfsrc_base)

/**************************************
 * Define and Declare
 **************************************/

#define DVFSRC_BANDWIDTH_CONST0     (DVFSRC_BASE + 0x0)
#define DVFSRC_BANDWIDTH_CONST1     (DVFSRC_BASE + 0x4)
#define DVFSRC_BANDWIDTH_CONST2     (DVFSRC_BASE + 0x8)
#define DVFSRC_ENABLE               (DVFSRC_BASE + 0xC)
#define DVFSRC_M3733                (DVFSRC_BASE + 0x10)
#define DVFSRC_M3200                (DVFSRC_BASE + 0x14)
#define DVFSRC_M1600                (DVFSRC_BASE + 0x18)
#define DVFSRC_SLICE_COUNT          (DVFSRC_BASE + 0x1C)
#define DVFSRC_K_SLICE_COUNT        (DVFSRC_BASE + 0x20)
#define DVFSRC_DEGLITCH_COUNT       (DVFSRC_BASE + 0x24)
#define DVFSRC_TOTAL_AVG_MBW        (DVFSRC_BASE + 0x28)
#define DVFSRC_TOTAL_AVG_K_MBW      (DVFSRC_BASE + 0x2C)
#define DVFSRC_CPU_LEVEL_MASK       (DVFSRC_BASE + 0x30)
#define DVFSRC_MD32_LEVEL_MASK      (DVFSRC_BASE + 0x34)
#define DVFSRC_SPM_LEVEL_MASK       (DVFSRC_BASE + 0x38)
#define DVFSRC_MD_LEVEL_MASK        (DVFSRC_BASE + 0x3C)
#define DVFSRC_OVL_LEVEL_MASK       (DVFSRC_BASE + 0x40)
#define DVFSRC_OVL2_LEVEL_MASK      (DVFSRC_BASE + 0x44)
#define DVFSRC_CHANNEL_MASK         (DVFSRC_BASE + 0x48)
#define DVFSRC_MD_LEVEL_CTRL        (DVFSRC_BASE + 0x4C)
#define DVFSRC_SIGNAL_CTRL          (DVFSRC_BASE + 0x50)
#define DVFSRC_LEVEL_JMP_METHOD     (DVFSRC_BASE + 0x54)
#define DVFSRC_MD_LEVEL_CTRL_2	    (DVFSRC_BASE + 0x58)

#define DVFSRC_MD_MAP_BW_0          (DVFSRC_BASE + 0x80)
#define DVFSRC_MD_MAP_BW_1          (DVFSRC_BASE + 0x84)
#define DVFSRC_MD_MAP_BW_2          (DVFSRC_BASE + 0x88)
#define DVFSRC_MD_MAP_BW_3          (DVFSRC_BASE + 0x8C)
#define DVFSRC_MD_MAP_BW_4          (DVFSRC_BASE + 0x90)
#define DVFSRC_MD_MAP_BW_5          (DVFSRC_BASE + 0x94)
#define DVFSRC_MD_MAP_BW_6          (DVFSRC_BASE + 0x98)
#define DVFSRC_MD_MAP_BW_7          (DVFSRC_BASE + 0x9C)
#define DVFSRC_MD_MAP_BW_8          (DVFSRC_BASE + 0xA0)
#define DVFSRC_MD_MAP_BW_9          (DVFSRC_BASE + 0xA4)
#define DVFSRC_MD_MAP_BW_10         (DVFSRC_BASE + 0xA8)
#define DVFSRC_MD_MAP_BW_11         (DVFSRC_BASE + 0xAC)
#define DVFSRC_MD_MAP_BW_12         (DVFSRC_BASE + 0xB0)
#define DVFSRC_MD_MAP_BW_13         (DVFSRC_BASE + 0xB4)
#define DVFSRC_MD_MAP_BW_14         (DVFSRC_BASE + 0xB8)
#define DVFSRC_MD_MAP_BW_15         (DVFSRC_BASE + 0xBC)

#define DVFSRC_DEBUG_EN             (DVFSRC_BASE + 0x100)
#define DVFSRC_RECORD_0             (DVFSRC_BASE + 0x104)
#define DVFSRC_RECORD_1             (DVFSRC_BASE + 0x108)
#define DVFSRC_RECORD_2             (DVFSRC_BASE + 0x10C)
#define DVFSRC_RECORD_3             (DVFSRC_BASE + 0x110)
#define DVFSRC_RECORD_4             (DVFSRC_BASE + 0x114)
#define DVFSRC_RECORD_5             (DVFSRC_BASE + 0x118)
#define DVFSRC_RECORD_6             (DVFSRC_BASE + 0x11C)
#define DVFSRC_RECORD_7             (DVFSRC_BASE + 0x120)
#define DVFSRC_RECORD_8             (DVFSRC_BASE + 0x124)

#define DVFSRC_RECORD_0_L           (DVFSRC_BASE + 0x128)
#define DVFSRC_RECORD_1_L           (DVFSRC_BASE + 0x12C)
#define DVFSRC_RECORD_2_L           (DVFSRC_BASE + 0x130)
#define DVFSRC_RECORD_3_L           (DVFSRC_BASE + 0x134)
#define DVFSRC_RECORD_4_L           (DVFSRC_BASE + 0x138)
#define DVFSRC_RECORD_5_L           (DVFSRC_BASE + 0x13C)
#define DVFSRC_RECORD_6_L           (DVFSRC_BASE + 0x140)
#define DVFSRC_RECORD_7_L           (DVFSRC_BASE + 0x144)
#define DVFSRC_RECORD_8_L           (DVFSRC_BASE + 0x148)
#define DVFSRC_SW_RST               (DVFSRC_BASE + 0x14C)
#define DVFSRC_DBG_SEL              (DVFSRC_BASE + 0x150)

#define DVFSRC_RESERVED_0           (DVFSRC_BASE + 0x200)
#define DVFSRC_RESERVED_1           (DVFSRC_BASE + 0x204)
#define DVFSRC_RESERVED_STATUS      (DVFSRC_BASE + 0x208)
#define DVFSRC_RESERVED_2           (DVFSRC_BASE + 0x20C)
#define DVFSRC_RESERVED_3           (DVFSRC_BASE + 0x210)
#define DVFSRC_RESERVED_3           (DVFSRC_BASE + 0x210)

/* DVFSRC_MD32_LEVEL_MASK (0x10110000+0x034) */
#define MD32_LEVEL_MASK		((1U << 0) | (1U << 1) | (1U << 2) | (1U << 3))

/* DVFSRC_SPM_LEVEL_MASK (0x10110000+0x038) */
#define DVFS_REQ_TO_SPM_TIMEOUT	(1U << 17)

/* DVFSRC_DEBUG_EN (0x10110000+0x100) */
#define RG_REQUEST_VOLT_ONLY	((1U << 4) | (1U << 5))

#endif /* __MT_DVFSRC_REG__H__ */

