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

#ifndef __MTK_DVFSRC_REG_H__
#define __MTK_DVFSRC_REG_H__

#include <linux/io.h>

extern void __iomem *dvfsrc_base;
#define DVFSRC_BASE (dvfsrc_base)

/**************************************
 * Define and Declare
 **************************************/

#define DVFSRC_BASIC_CONTROL      (DVFSRC_BASE + 0x0)
#define DVFSRC_EMI_REQUEST        (DVFSRC_BASE + 0x4)
#define DVFSRC_EMI_REQUEST2       (DVFSRC_BASE + 0x8)
#define DVFSRC_EMI_HRT            (DVFSRC_BASE + 0xC)
#define DVFSRC_EMI_MD2SPM0        (DVFSRC_BASE + 0x10)
#define DVFSRC_EMI_MD2SPM1        (DVFSRC_BASE + 0x14)
#define DVFSRC_VCORE_REQUEST      (DVFSRC_BASE + 0x18)
#define DVFSRC_VCORE_HRT          (DVFSRC_BASE + 0x1C)
#define DVFSRC_VCORE_MD2SPM0      (DVFSRC_BASE + 0x20)
#define DVFSRC_VCORE_MD2SPM1      (DVFSRC_BASE + 0x24)
#define DVFSRC_MD_REQUEST         (DVFSRC_BASE + 0x28)
#define DVFSRC_MD_SW_CONTROL      (DVFSRC_BASE + 0x2C)
#define DVFSRC_MD_VMODEM_REMAP    (DVFSRC_BASE + 0x30)
#define DVFSRC_MD_VMD_REMAP       (DVFSRC_BASE + 0x34)
#define DVFSRC_MD_VSRAM_REMAP     (DVFSRC_BASE + 0x38)
#define DVFSRC_HALT_SW_CONTROL    (DVFSRC_BASE + 0x3C)
#define DVFSRC_INT                (DVFSRC_BASE + 0x40)
#define DVFSRC_DEBOUNCE_FOUR      (DVFSRC_BASE + 0x44)
#define DVFSRC_DEBOUNCE_RISE_FALL (DVFSRC_BASE + 0x48)
#define DVFSRC_TIMEOUT_NEXTREQ    (DVFSRC_BASE + 0x4C)
#define DVFSRC_LEVEL              (DVFSRC_BASE + 0x50)
#define DVFSRC_LEVEL_LABEL_0_1    (DVFSRC_BASE + 0x60)
#define DVFSRC_LEVEL_LABEL_2_3    (DVFSRC_BASE + 0x64)
#define DVFSRC_LEVEL_LABEL_4_5    (DVFSRC_BASE + 0x68)
#define DVFSRC_LEVEL_LABEL_6_7    (DVFSRC_BASE + 0x6C)
#define DVFSRC_LEVEL_LABEL_8_9    (DVFSRC_BASE + 0x70)
#define DVFSRC_LEVEL_LABEL_10_11  (DVFSRC_BASE + 0x74)
#define DVFSRC_LEVEL_LABEL_12_13  (DVFSRC_BASE + 0x78)
#define DVFSRC_LEVEL_LABEL_14_15  (DVFSRC_BASE + 0x7C)
#define DVFSRC_FORCE              (DVFSRC_BASE + 0x80)
#define DVFSRC_SEC_SW_REQ         (DVFSRC_BASE + 0x84)
#define DVFSRC_LAST               (DVFSRC_BASE + 0x88)
#define DVFSRC_MD_GEAR            (DVFSRC_BASE + 0x8C)
#define DVFSRC_OPT_MASK           (DVFSRC_BASE + 0x90)
#define DVFSRC_RECORD_0_0         (DVFSRC_BASE + 0x100)
#define DVFSRC_RECORD_0_1         (DVFSRC_BASE + 0x104)
#define DVFSRC_RECORD_1_0         (DVFSRC_BASE + 0x108)
#define DVFSRC_RECORD_1_1         (DVFSRC_BASE + 0x10C)
#define DVFSRC_RECORD_2_0         (DVFSRC_BASE + 0x110)
#define DVFSRC_RECORD_2_1         (DVFSRC_BASE + 0x114)
#define DVFSRC_RECORD_3_0         (DVFSRC_BASE + 0x118)
#define DVFSRC_RECORD_3_1         (DVFSRC_BASE + 0x11C)
#define DVFSRC_RECORD_4_0         (DVFSRC_BASE + 0x120)
#define DVFSRC_RECORD_4_1         (DVFSRC_BASE + 0x124)
#define DVFSRC_RECORD_5_0         (DVFSRC_BASE + 0x128)
#define DVFSRC_RECORD_5_1         (DVFSRC_BASE + 0x12C)
#define DVFSRC_RECORD_6_0         (DVFSRC_BASE + 0x130)
#define DVFSRC_RECORD_6_1         (DVFSRC_BASE + 0x134)
#define DVFSRC_RECORD_7_0         (DVFSRC_BASE + 0x138)
#define DVFSRC_RECORD_7_1         (DVFSRC_BASE + 0x13C)
#define DVFSRC_RECORD_0_L_0       (DVFSRC_BASE + 0x140)
#define DVFSRC_RECORD_0_L_1       (DVFSRC_BASE + 0x144)
#define DVFSRC_RECORD_1_L_0       (DVFSRC_BASE + 0x148)
#define DVFSRC_RECORD_1_L_1       (DVFSRC_BASE + 0x14C)
#define DVFSRC_RECORD_2_L_0       (DVFSRC_BASE + 0x150)
#define DVFSRC_RECORD_2_L_1       (DVFSRC_BASE + 0x154)
#define DVFSRC_RECORD_3_L_0       (DVFSRC_BASE + 0x158)
#define DVFSRC_RECORD_3_L_1       (DVFSRC_BASE + 0x15C)
#define DVFSRC_RECORD_4_L_0       (DVFSRC_BASE + 0x160)
#define DVFSRC_RECORD_4_L_1       (DVFSRC_BASE + 0x164)
#define DVFSRC_RECORD_5_L_0       (DVFSRC_BASE + 0x168)
#define DVFSRC_RECORD_5_L_1       (DVFSRC_BASE + 0x16C)
#define DVFSRC_RECORD_6_L_0       (DVFSRC_BASE + 0x170)
#define DVFSRC_RECORD_6_L_1       (DVFSRC_BASE + 0x174)
#define DVFSRC_RECORD_7_L_0       (DVFSRC_BASE + 0x178)
#define DVFSRC_RECORD_7_L_1       (DVFSRC_BASE + 0x17C)
#define DVFSRC_RECORD_MD_0        (DVFSRC_BASE + 0x180)
#define DVFSRC_RECORD_MD_1        (DVFSRC_BASE + 0x184)
#define DVFSRC_RECORD_MD_2        (DVFSRC_BASE + 0x188)
#define DVFSRC_RECORD_MD_3        (DVFSRC_BASE + 0x18C)
#define DVFSRC_RECORD_MD_4        (DVFSRC_BASE + 0x190)
#define DVFSRC_RECORD_MD_5        (DVFSRC_BASE + 0x194)
#define DVFSRC_RECORD_MD_6        (DVFSRC_BASE + 0x198)
#define DVFSRC_RECORD_MD_7        (DVFSRC_BASE + 0x19C)
#define DVFSRC_RECORD_COUNT       (DVFSRC_BASE + 0x200)
#define DVFSRC_RSRV_0             (DVFSRC_BASE + 0x300)
#define DVFSRC_RSRV_1             (DVFSRC_BASE + 0x304)
#define DVFSRC_RSRV_2             (DVFSRC_BASE + 0x308)
#define DVFSRC_RSRV_3             (DVFSRC_BASE + 0x30C)

#endif /* __MTK_DVFSRC_REG_H__ */

