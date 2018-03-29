/*
* Copyright (c) 2015 MediaTek Inc.
* Author: HenryC.Chen <henryc.chen@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#ifndef __SYM827_REGISTERS_H__
#define __SYM827_REGISTERS_H__

#define	SYM827_REG_VSEL_0				0x00
#define	SYM827_REG_VSEL_1				0x01
#define	SYM827_REG_CONTROL				0x02
#define	SYM827_REG_ID_1					0x03
#define	SYM827_REG_ID_2					0x04
#define	SYM827_REG_PGOOD				0x05

#define SYM827_BUCK_ENABLE				0x01
#define SYM827_BUCK_DISABLE				0x00

#define SYM827_BUCK_EN_SHIFT				0x07
#define SYM827_BUCK_EN_MASK				0x80
#define SYM827_BUCK_MODE_SHIFT				0x06
#define SYM827_BUCK_MODE_MASK				0x40
#define SYM827_BUCK_NSEL_SHIFT				0x00
#define SYM827_BUCK_NSEL_MASK				0x3F

#define	SYM827_REG_SLEW_RATE_SHIFT			0x04
#define	SYM827_REG_SLEW_RATE_MASK			0x07

#define SYM827_ID_VENDOR_SHIFT				0x05
#define SYM827_ID_VENDOR_MASK				0xE0
#define SYM827_ID_DIE_ID_SHIFT				0x00
#define SYM827_ID_DIE_ID_MASK				0x0F
#define SYM827_ID_DIE_REV_SHIFT				0x00
#define SYM827_ID_DIE_REV_MASK				0x0F

#define SYM827_PGOOD_SHIFT				0x07
#define SYM827_PGOOD_MASK				0x80

#define SYM827_VENDOR_ID				0x80
#ifdef CONFIG_ARCH_MT8163
extern void slp_cpu_dvs_en(bool en);
void set_slp_spm_deepidle_flags(bool en);
extern void spm_sodi_cpu_dvs_en(bool en);
#endif
#endif
