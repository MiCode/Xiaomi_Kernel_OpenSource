/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_ADC_H
#define _MTK_ADC_H

/* ------------------------------------------------------------------------- */

extern int IMM_IsAdcInitReady(void);
extern int IMM_get_adc_channel_num(char *channel_name, int len);
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_GetOneChannelValue_Cali(int Channel, int *voltage);

#endif	 /*_MTK_ADC_H*/
