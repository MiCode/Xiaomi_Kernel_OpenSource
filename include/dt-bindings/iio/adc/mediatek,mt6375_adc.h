/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header provides macros for MT6375 ADC device bindings.
 *
 * Copyright (c) 2021 Mediatek Inc.
 * Author: ShuFan Lee <shufan_lee@richtek.com>
 */

#ifndef _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6375_ADC_H
#define _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6375_ADC_H

/* ADC channel idx. */
#define MT6375_ADC_VBATMON	0
#define MT6375_ADC_CHGVIN	1
#define MT6375_ADC_USBDP	2
#define MT6375_ADC_VSYS		3
#define MT6375_ADC_VBAT		4
#define MT6375_ADC_IBUS		5
#define MT6375_ADC_IBAT		6
#define MT6375_ADC_USBDM	7
#define MT6375_ADC_TEMPJC	8
#define MT6375_ADC_VREFTS	9
#define MT6375_ADC_TS		10
#define MT6375_ADC_PDVBUS	11
#define MT6375_ADC_CC1		12
#define MT6375_ADC_CC2		13
#define MT6375_ADC_SBU1		14
#define MT6375_ADC_SBU2		15
#define MT6375_ADC_FGCIC1	16
#define MT6375_ADC_MAX_CHANNEL	17

#endif
