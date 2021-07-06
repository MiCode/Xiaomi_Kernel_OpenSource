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

#ifndef _MTK_ADC_H
#define _MTK_ADC_H

/* ------------------------------------------------------------------------- */

extern int IMM_IsAdcInitReady(void);
extern int IMM_get_adc_channel_num(char *channel_name, int len);
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_GetOneChannelValue_Cali(int Channel, int *voltage);

#endif	 /*_MTK_ADC_H*/
