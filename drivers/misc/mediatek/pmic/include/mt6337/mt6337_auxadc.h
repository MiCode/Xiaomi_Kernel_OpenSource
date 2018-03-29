/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT6337_AUXADC_H
#define __MT6337_AUXADC_H
/* ADC Channel Number */
typedef enum {
	MT6337_AUX_BATSNS_AP =		0x000,
	MT6337_AUX_ISENSE_AP,
	MT6337_AUX_VCDT_AP,
	MT6337_AUX_BATON_AP, /* BATON/BIF */
	MT6337_AUX_CH4,
	MT6337_AUX_VACCDET_AP,
	MT6337_AUX_CH6,
	MT6337_AUX_TSX,
	MT6337_AUX_CH8,
	MT6337_AUX_CH9,
	MT6337_AUX_CH10,
	MT6337_AUX_CH11,
	MT6337_AUX_CH12,
	MT6337_AUX_CH13,
	MT6337_AUX_CH14,
	MT6337_AUX_CH15,
	MT6337_AUX_CH16,
	MT6337_AUX_CH4_DCXO = 0x10000004,
} mt6337_adc_ch_list_enum;

extern signed int MT6337_IMM_GetCurrent(void);
extern unsigned int MT6337_IMM_GetOneChannelValue(mt6337_adc_ch_list_enum dwChannel, int deCount, int trimd);

#endif /*--MT6337_AUXADC_H--*/
