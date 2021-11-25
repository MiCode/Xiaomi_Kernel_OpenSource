/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header provides macros for MT6375 AUXADC device bindings.
 *
 * Copyright (c) 2021 Mediatek Inc.
 * Author: ShuFan Lee <shufan_lee@richtek.com>
 */

#ifndef _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6375_AUXADC_H
#define _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6375_AUXADC_H

/* ADC channel idx. */
#define MT6375_AUXADC_BATSNS		0
#define MT6375_AUXADC_BATON		1
#define MT6375_AUXADC_IMP		2
#define MT6375_AUXADC_IMIX_R		3
#define MT6375_AUXADC_VREF		4
#define MT6375_AUXADC_BATSNS_DBG	5
#define MT6375_AUXADC_MAX_CHANNEL	6

#define RG_INT_STATUS_BAT_H		0
#define RG_INT_STATUS_BAT_L		1
#define RG_INT_STATUS_BAT2_H		2
#define RG_INT_STATUS_BAT2_L		3
#define RG_INT_STATUS_BAT_TEMP_H	4
#define RG_INT_STATUS_BAT_TEMP_L	5
#define RG_INT_STATUS_AUXADC_IMP	8
#define RG_INT_STATUS_NAG_C_DLTV	9

#endif
