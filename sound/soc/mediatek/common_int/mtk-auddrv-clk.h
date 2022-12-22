/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/***************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Clk.h
 *
 * Project:
 * --------
 *   MT6797  Audio Driver clock control
 *
 * Description:
 * ------------
 *   Audio clcok control
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *
 *---------------------------------------------------------------------------
 *
 ****************************************************************************
 */

#ifndef _AUDDRV_CLK_H_
#define _AUDDRV_CLK_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mtk-auddrv-common.h"

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                 FUNCTION       D E F I N I T I O N
 *****************************************************************************/
#include <linux/clk.h>

extern int AudDrv_Clk_probe(void *dev);
extern void AudDrv_Clk_Deinit(void *dev);

//void AudDrv_Clk_Global_Variable_Init(void);
void AudDrv_AUDINTBUS_Sel(int parentidx);

void AudDrv_Bus_Init(void);

void AudDrv_Clk_On(void);
void AudDrv_Clk_Off(void);

void AudDrv_ANA_Clk_On(void);
void AudDrv_ANA_Clk_Off(void);

void AudDrv_I2S_Clk_On(void);
void AudDrv_I2S_Clk_Off(void);

void AudDrv_TDM_Clk_On(void);
void AudDrv_TDM_Clk_Off(void);

void AudDrv_ADC_Clk_On(void);
void AudDrv_ADC_Clk_Off(void);
void AudDrv_ADC2_Clk_On(void);
void AudDrv_ADC2_Clk_Off(void);
void AudDrv_ADC3_Clk_On(void);
void AudDrv_ADC3_Clk_Off(void);
void AudDrv_ADC_Hires_Clk_On(void);
void AudDrv_ADC_Hires_Clk_Off(void);
void AudDrv_ADC2_Hires_Clk_On(void);
void AudDrv_ADC2_Hires_Clk_Off(void);

void AudDrv_HDMI_Clk_On(void);
void AudDrv_HDMI_Clk_Off(void);

void AudDrv_APLL24M_Clk_On(void);
void AudDrv_APLL24M_Clk_Off(void);
void AudDrv_APLL22M_Clk_On(void);
void AudDrv_APLL22M_Clk_Off(void);

void AudDrv_APLL1Tuner_Clk_On(void);
void AudDrv_APLL1Tuner_Clk_Off(void);
void AudDrv_APLL2Tuner_Clk_On(void);
void AudDrv_APLL2Tuner_Clk_Off(void);

void AudDrv_Emi_Clk_On(void);
void AudDrv_Emi_Clk_Off(void);

void AudDrv_ANC_Clk_On(void);
void AudDrv_ANC_Clk_Off(void);

/* APLL , low jitter mode setting */
unsigned int GetApllbySampleRate(unsigned int SampleRate);
void EnableALLbySampleRate(unsigned int SampleRate);
void DisableALLbySampleRate(unsigned int SampleRate);
void EnableI2SDivPower(unsigned int Diveder_name, bool bEnable);
void EnableApll1(bool bEnable);
void EnableApll2(bool bEnable);
void SetCLkBclk(unsigned int MckDiv, unsigned int SampleRate,
		unsigned int Channels, unsigned int Wlength);
unsigned int SetCLkMclk(unsigned int I2snum, unsigned int SampleRate);
void EnableI2SCLKDiv(unsigned int I2snum, bool bEnable);
void PowerDownAllI2SDiv(void);
void audio_clk_control(void);

extern void aud_intbus_mux_sel(unsigned int aud_idx);
#endif
