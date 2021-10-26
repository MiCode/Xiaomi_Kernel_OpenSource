/*
 * mt6392-codec.h --  MT6392 ALSA SoC codec driver
 *
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MT6392_CODEC_H__
#define __MT6392_CODEC_H__

#define PMIC_OFFSET              (0x00C00000)
#define PMIC_REG(reg)            (reg | PMIC_OFFSET)

/* mt6392 */
#define SPK_CON0                 PMIC_REG(0x0052)
#define SPK_CON1                 PMIC_REG(0x0054)
#define SPK_CON2                 PMIC_REG(0x0056)
#define SPK_CON3                 PMIC_REG(0x0058)
#define SPK_CON4                 PMIC_REG(0x005A)
#define SPK_CON5                 PMIC_REG(0x005C)
#define SPK_CON6                 PMIC_REG(0x005E)
#define SPK_CON7                 PMIC_REG(0x0060)
#define SPK_CON8                 PMIC_REG(0x0062)
#define SPK_CON9                 PMIC_REG(0x0064)
#define SPK_CON10                PMIC_REG(0x0066)
#define SPK_CON11                PMIC_REG(0x0068)
#define SPK_CON12                PMIC_REG(0x006A)

#define TOP_CKPDN1_CLR           PMIC_REG(0x010C)

#endif
