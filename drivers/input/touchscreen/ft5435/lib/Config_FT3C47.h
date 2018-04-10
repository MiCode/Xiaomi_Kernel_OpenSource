/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Config_FT3C47.h
*
* Author: Software Development Team, AE
*
* Created: 2015-12-02
*
* Abstract: Set Config for FT3C47
*
************************************************************************/
#ifndef _CONFIG_FT3C47_H
#define _CONFIG_FT3C47_H

#include "test_lib.h"

struct stCfg_FT3C47_TestItem {
	bool FW_VERSION_TEST;
	bool FACTORY_ID_TEST;
	bool PROJECT_CODE_TEST;
	bool IC_VERSION_TEST;
	bool RAWDATA_TEST;
	bool ADC_DETECT_TEST;
	bool SCAP_CB_TEST;
	bool SCAP_RAWDATA_TEST;
	bool CHANNEL_NUM_TEST;
	bool INT_PIN_TEST;
	bool RESET_PIN_TEST;
	bool NOISE_TEST;
	bool WEAK_SHORT_CIRCUIT_TEST;
	bool UNIFORMITY_TEST;
	bool CM_TEST;

	bool RAWDATA_MARGIN_TEST;
	bool PANEL_DIFFER_TEST;
	bool PANEL_DIFFER_UNIFORMITY_TEST;

	bool LCM_ID_TEST;

	bool TE_TEST;
	bool SITO_RAWDATA_UNIFORMITY_TEST;
	bool PATTERN_TEST;

	bool FORCETOUCH_CHANNEL_NUM_TEST;
	bool FORCETOUCH_RAWDATA_TEST;
	bool FORCETOUCH_CB_TEST;
	bool FORCETOUCH_WEAK_SHORT_CIRCUIT_TEST;
	bool FORCETOUCH_FLATNESS_TEST;
};

struct stCfg_FT3C47_BasicThreshold {
	BYTE FW_VER_VALUE;
	BYTE Factory_ID_Number;
	char Project_Code[32];
	BYTE IC_Version;
	BYTE LCM_ID;
	int RawDataTest_low_Min;
	int RawDataTest_Low_Max;
	int RawDataTest_high_Min;
	int RawDataTest_high_Max;
	BYTE RawDataTest_SetLowFreq;
	BYTE RawDataTest_SetHighFreq;
	int AdcDetect_Max;

	int SCapCbTest_OFF_Min;
	int SCapCbTest_OFF_Max;
	int SCapCbTest_ON_Min;
	int SCapCbTest_ON_Max;
	bool SCapCbTest_LetTx_Disable;
	BYTE SCapCbTest_SetWaterproof_OFF;
	BYTE SCapCbTest_SetWaterproof_ON;

	int SCapRawDataTest_OFF_Min;
	int SCapRawDataTest_OFF_Max;
	int SCapRawDataTest_ON_Min;
	int SCapRawDataTest_ON_Max;
	bool SCapRawDataTest_LetTx_Disable;
	BYTE SCapRawDataTest_SetWaterproof_OFF;
	BYTE SCapRawDataTest_SetWaterproof_ON;
	bool bChannelTestMapping;
	bool bChannelTestNoMapping;
	BYTE ChannelNumTest_TxNum;
	BYTE ChannelNumTest_RxNum;
	BYTE ChannelNumTest_TxNpNum;
	BYTE ChannelNumTest_RxNpNum;
	BYTE ResetPinTest_RegAddr;
	BYTE IntPinTest_RegAddr;
	BYTE IntPinTest_TestNum;
	int NoiseTest_Max;
	int GloveNoiseTest_Coefficient;
	int NoiseTest_Frames;
	int NoiseTest_Time;
	BYTE NoiseTest_SampeMode;
	BYTE NoiseTest_NoiseMode;
	BYTE NoiseTest_ShowTip;
	bool bNoiseTest_GloveMode;
	int NoiseTest_RawdataMin;
	unsigned char Set_Frequency;
	bool bNoiseThreshold_Choose;
	int NoiseTest_Threshold;
	int NoiseTest_MinNgFrame;

	int WeakShortTest_CG;
	int WeakShortTest_CC;

	bool Uniformity_CheckTx;
	bool Uniformity_CheckRx;
	bool Uniformity_CheckMinMax;
	int  Uniformity_Tx_Hole;
	int  Uniformity_Rx_Hole;
	int  Uniformity_MinMax_Hole;
	bool CMTest_CheckMin;
	bool CMTest_CheckMax;
	int  CMTest_MinHole;
	int  CMTest_MaxHole;

	int RawdataMarginTest_Min;
	int RawdataMarginTest_Max;

	int PanelDifferTest_Min;
	int PanelDifferTest_Max;

	bool PanelDiffer_UniformityTest_Check_Tx;
	bool PanelDiffer_UniformityTest_Check_Rx;
	bool PanelDiffer_UniformityTest_Check_MinMax;
	int  PanelDiffer_UniformityTest_Tx_Hole;
	int  PanelDiffer_UniformityTest_Rx_Hole;
	int  PanelDiffer_UniformityTest_MinMax_Hole;

	bool SITO_RawdtaUniformityTest_Check_Tx;
	bool SITO_RawdtaUniformityTest_Check_Rx;
	int  SITO_RawdtaUniformityTest_Tx_Hole;
	int  SITO_RawdtaUniformityTest_Rx_Hole;

	bool bPattern00;
	bool bPatternFF;
	bool bPattern55;
	bool bPatternAA;
	bool bPatternBin;

	int ForceTouch_ChannelNumTest_ChannelNum;

	int ForceTouch_SCapRawDataTest_OFF_Min;
	int ForceTouch_SCapRawDataTest_OFF_Max;
	int ForceTouch_SCapRawDataTest_ON_Min;
	int ForceTouch_SCapRawDataTest_ON_Max;
	BYTE ForceTouch_SCapRawDataTest_SetWaterproof_OFF;
	BYTE ForceTouch_SCapRawDataTest_SetWaterproof_ON;

	int ForceTouch_SCapCbTest_OFF_Min;
	int ForceTouch_SCapCbTest_OFF_Max;
	int ForceTouch_SCapCbTest_ON_Min;
	int ForceTouch_SCapCbTest_ON_Max;
	BYTE ForceTouch_SCapCBTest_SetWaterproof_OFF;
	BYTE ForceTouch_SCapCBTest_SetWaterproof_ON;

	int ForceTouch_WeakShortTest_CG;
	int ForceTouch_WeakShortTest_CC;
	bool bForceTouch_WeakShortTest_CapShortTest;

	int ForceTouch_FlatnessTest_Differ_Threshold;
	int ForceTouch_FlatnessTest_Differ_Coefficient;
	bool ForceTouch_FlatnessTest_Differ_Threshold_Check;
	bool ForceTouch_FlatnessTest_Differ_Coefficient_Check;

};
enum enumTestItem_FT3C47 {
	Code_FT3C47_ENTER_FACTORY_MODE,
	Code_FT3C47_DOWNLOAD,
	Code_FT3C47_UPGRADE,
	Code_FT3C47_FACTORY_ID_TEST,
	Code_FT3C47_PROJECT_CODE_TEST,
	Code_FT3C47_FW_VERSION_TEST,
	Code_FT3C47_IC_VERSION_TEST,
	Code_FT3C47_RAWDATA_TEST,
	Code_FT3C47_ADCDETECT_TEST,
	Code_FT3C47_SCAP_CB_TEST,
	Code_FT3C47_SCAP_RAWDATA_TEST,
	Code_FT3C47_CHANNEL_NUM_TEST,
	Code_FT3C47_INT_PIN_TEST,
	Code_FT3C47_RESET_PIN_TEST,
	Code_FT3C47_NOISE_TEST,
	Code_FT3C47_WEAK_SHORT_CIRCUIT_TEST,
	Code_FT3C47_UNIFORMITY_TEST,
	Code_FT3C47_CM_TEST,
	Code_FT3C47_RAWDATA_MARGIN_TEST,
	Code_FT3C47_WRITE_CONFIG,
	Code_FT3C47_PANELDIFFER_TEST,
	Code_FT3C47_PANELDIFFER_UNIFORMITY_TEST,
	Code_FT3C47_LCM_ID_TEST,
	Code_FT3C47_JUDEG_NORMALIZE_TYPE,
	Code_FT3C47_TE_TEST,
	Code_FT3C47_SITO_RAWDATA_UNIFORMITY_TEST,
	Code_FT3C47_PATTERN_TEST,

	Code_FT3C47_GPIO_TEST,
	Code_FT3C47_LCD_NOISE_TEST,
	Code_FT3C47_FORCE_TOUCH_CHANNEL_NUM_TEST,
	Code_FT3C47_FORCE_TOUCH_SCAP_RAWDATA_TEST,
	Code_FT3C47_FORCE_TOUCH_SCAP_CB_TEST,
	Code_FT3C47_FORCE_TOUCH_WEAK_SHORT_CIRCUIT_TEST,
	Code_FT3C47_FORCE_TOUCH_FLATNESS_TEST,
};

extern struct stCfg_FT3C47_TestItem g_stCfg_FT3C47_TestItem;
extern struct stCfg_FT3C47_BasicThreshold g_stCfg_FT3C47_BasicThreshold;

void OnInit_FT3C47_TestItem(char *strIniFile);
void OnInit_FT3C47_BasicThreshold(char *strIniFile);
void SetTestItem_FT3C47(void);

#endif
