/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _DT_BINDINGS_MT635X_AUXADC_H
#define _DT_BINDINGS_MT635X_AUXADC_H

/* PMIC MT635x AUXADC channels */
#define AUXADC_BATADC				0x00
#define AUXADC_ISENSE				0x01
#define AUXADC_VCDT				0x02
#define AUXADC_BAT_TEMP				0x03
#define AUXADC_BATID				0x04
#define AUXADC_CHIP_TEMP			0x05
#define AUXADC_VCORE_TEMP			0x06
#define AUXADC_VPROC_TEMP			0x07
#define AUXADC_VGPU_TEMP			0x08
#define AUXADC_ACCDET				0x09
#define AUXADC_VDCXO				0x0a
#define AUXADC_TSX_TEMP				0x0b
#define AUXADC_HPOFS_CAL			0x0c
#define AUXADC_DCXO_TEMP			0x0d
#define AUXADC_VBIF				0x0e
#define AUXADC_CHAN_MIN                         AUXADC_BATADC
#if defined(CONFIG_MACH_MT6768)
#define AUXADC_CHAN_MAX                         AUXADC_VBIF
#elif defined(CONFIG_MACH_MT6739)
#define AUXADC_CHAN_MAX                         AUXADC_VBIF
#elif defined(CONFIG_MACH_MT6781)
#define AUXADC_CHAN_MAX                         AUXADC_VBIF
#elif defined(CONFIG_MACH_MT6877)
#define AUXADC_CHAN_MAX                         AUXADC_VBIF
#elif defined(CONFIG_MACH_MT6833)
#define AUXADC_CHAN_MAX                         AUXADC_VBIF
#elif defined(CONFIG_MACH_MT6853)
#define AUXADC_CHAN_MAX                         AUXADC_VBIF
#elif defined(CONFIG_MACH_MT6873)
#define AUXADC_CHAN_MAX                         AUXADC_VBIF
#else
#define AUXADC_IMP				0x0f
#define AUXADC_IMIX_R				0x10

#define AUXADC_CHAN_MAX				AUXADC_IMIX_R
#endif

#endif /* _DT_BINDINGS_MT635X_AUXADC_H */
