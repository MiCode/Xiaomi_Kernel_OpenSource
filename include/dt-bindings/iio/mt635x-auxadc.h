/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#define AUXADC_IMP				0x0f
#define AUXADC_IMIX_R				0x10
#define AUXADC_VTREF				0x11
#define AUXADC_VSYSSNS				0x12
#define AUXADC_VIN1				0x13
#define AUXADC_VIN2				0x14
#define AUXADC_VIN3				0x15
#define AUXADC_VIN4				0x16
#define AUXADC_VIN5				0x17
#define AUXADC_VIN6				0x18
#define AUXADC_VIN7				0x19

#define AUXADC_CHAN_MIN				AUXADC_BATADC
#define AUXADC_CHAN_MAX				AUXADC_VIN7

#define ADC_PURES_100K				(0)
#define ADC_PURES_30K				(1)
#define ADC_PURES_400K				(2)
#define ADC_PURES_OPEN				(3)

#define ADC_PURES_100K_MASK			(ADC_PURES_100K << 8)
#define ADC_PURES_30K_MASK			(ADC_PURES_30K << 8)
#define ADC_PURES_400K_MASK			(ADC_PURES_400K << 8)
#define ADC_PURES_OPEN_MASK			(ADC_PURES_OPEN << 8)

#endif /* _DT_BINDINGS_MT635X_AUXADC_H */
