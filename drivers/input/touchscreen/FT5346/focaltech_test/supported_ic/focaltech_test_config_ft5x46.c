/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Focaltech_test_config_ft5x46.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: Set Config for FT5X46\FT5X46i\FT5526\FT3X17\FT5436\FT3X27\FT5526i\FT5416\FT5426\FT5435
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>

#include "../include/focaltech_test_ini.h"
#include "../include/focaltech_test_supported_ic.h"
#include "../focaltech_test_config.h"


#if (FT5X46_TEST)


struct stCfg_FT5X46_TestItem g_stCfg_FT5X46_TestItem;
struct stCfg_FT5X46_BasicThreshold g_stCfg_FT5X46_BasicThreshold;

void OnInit_FT5X46_TestItem(char *strIniFile)
{
	char str[512];

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("TestItem", "FW_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.FW_VERSION_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "FACTORY_ID_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.FACTORY_ID_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "PROJECT_CODE_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.PROJECT_CODE_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "IC_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.IC_VERSION_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "LCM_ID_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.LCM_ID_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "RAWDATA_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.RAWDATA_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "ADC_DETECT_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.ADC_DETECT_TEST = fts_atoi(str);

	GetPrivateProfileString("TestItem", "SCAP_CB_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.SCAP_CB_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "SCAP_RAWDATA_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.SCAP_RAWDATA_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "CHANNEL_NUM_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.CHANNEL_NUM_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "INT_PIN_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.INT_PIN_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "RESET_PIN_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.RESET_PIN_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.NOISE_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "WEAK_SHORT_CIRCUIT_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.WEAK_SHORT_CIRCUIT_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "UNIFORMITY_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "CM_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.CM_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "RAWDATA_MARGIN_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.RAWDATA_MARGIN_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "PANEL_DIFFER_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.PANEL_DIFFER_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "PANEL_DIFFER_UNIFORMITY_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.PANEL_DIFFER_UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "TE_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.TE_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "SITO_RAWDATA_UNIFORMITY_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.SITO_RAWDATA_UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "PATTERN_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.PATTERN_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "LCD_NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.LCD_NOISE_TEST = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();
}
void OnInit_FT5X46_BasicThreshold(char *strIniFile)
{
	char str[512];

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("Basic_Threshold", "FW_VER_VALUE", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.FW_VER_VALUE = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Factory_ID_Number", "255", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Factory_ID_Number = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Project_Code", " ", str, strIniFile);

	sprintf(g_stCfg_FT5X46_BasicThreshold.Project_Code, "%s", str);


	GetPrivateProfileString("Basic_Threshold", "IC_Version", "3", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.IC_Version = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "LCM_ID", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.LCM_ID = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Min", "3000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_low_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Max", "15000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_Low_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Min", "3000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_high_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Max", "15000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_high_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_LowFreq", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_SetLowFreq  = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_HighFreq", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_SetHighFreq = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "Adc_Detect_Max", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.AdcDetect_Max = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Min", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_OFF_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Max", "240", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_OFF_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Min", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_ON_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Max", "240", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_ON_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ScapCBTest_SetWaterproof_OFF", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_SetWaterproof_OFF = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ScapCBTest_SetWaterproof_ON", "240", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_SetWaterproof_ON = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapCBTest_LetTx_Disable", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_LetTx_Disable = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Min", "5000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_OFF_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Max", "8500", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_OFF_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Min", "5000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_ON_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Max", "8500", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_ON_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_SetWaterproof_OFF", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_SetWaterproof_OFF = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_SetWaterproof_ON", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_SetWaterproof_ON = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_LetTx_Disable", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_LetTx_Disable = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_Mapping", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bChannelTestMapping = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_NoMapping", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bChannelTestNoMapping = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_TxNum", "13", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ChannelNumTest_TxNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_RxNum", "24", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ChannelNumTest_RxNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_Tx_NP_Num", "13", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ChannelNumTest_TxNpNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_Rx_NP_Num", "24", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ChannelNumTest_RxNpNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ResetPinTest_RegAddr", "136", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ResetPinTest_RegAddr = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "IntPinTest_RegAddr", "79", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.IntPinTest_RegAddr = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "IntPinTest_TestNum", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.IntPinTest_TestNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Max", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_Max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Frames", "32", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_Frames = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Time", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_Time = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_SampeMode", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_SampeMode = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_NoiseMode", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_NoiseMode = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_ShowTip", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_ShowTip = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_GloveMode", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bNoiseTest_GloveMode = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_RawdataMin", "5000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_RawdataMin = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "GloveNoiseTest_Coefficient", "100", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.GloveNoiseTest_Coefficient = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "Set_Frequency", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Set_Frequency = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseThreshold_Choose", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bNoiseThreshold_Choose = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Threshold", "50", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_Threshold = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_MinNGFrame", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_MinNgFrame = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_TPToolProcess", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bNoiseTest_TPToolProcess = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CG", "2000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CG = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CC", "2000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CC = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_Tx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_CheckTx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_Rx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_CheckRx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_MinMax", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_CheckMinMax = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Tx_Hole", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_Tx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Rx_Hole", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_Rx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_MinMax_Hole", "70", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_MinMax_Hole = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "CMTest_Check_Min", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.CMTest_CheckMin = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CMTest_Check_Max", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.CMTest_CheckMax = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CMTest_Min_Hole", "0.5", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.CMTest_MinHole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CMTest_Max_Hole", "5", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.CMTest_MaxHole = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawdataMarginTest_Min", "10", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawdataMarginTest_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawdataMarginTest_Max", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawdataMarginTest_Max = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Min", "150", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDifferTest_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Max", "1000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDifferTest_Max = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Check_Tx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_UniformityTest_Check_Tx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Check_Rx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_UniformityTest_Check_Rx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Check_MinMax", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_UniformityTest_Check_MinMax = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Tx_Hole", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_UniformityTest_Tx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Rx_Hole", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_UniformityTest_Rx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_MinMax_Hole", "70", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_UniformityTest_MinMax_Hole = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Check_Tx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SITO_RawdtaUniformityTest_Check_Tx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Check_Rx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SITO_RawdtaUniformityTest_Check_Rx = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Tx_Hole", "10", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SITO_RawdtaUniformityTest_Tx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Rx_Hole", "10", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SITO_RawdtaUniformityTest_Rx_Hole = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "PramTest_Check_00", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bPattern00 = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PramTest_Check_FF", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bPatternFF = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PramTest_Check_55", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bPattern55 = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PramTest_Check_AA", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bPatternAA = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PramTest_Check_BIN", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bPatternBin = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CC_Rsen", "57", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CC_Rsen = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CapShortTest", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CapShortTest = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_Max_Frame", "200", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_MaxFrame = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_Conficient", "100", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_Conficient = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_Mode", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_Noise_Mode = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_MaxNgPoint", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_MaxNgPoint = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_FrameNum", "63", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_FrameNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_SetFrequency", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_SetFrequency = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_NoiseThresholdMode", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_NoiseThresholdMode = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_NoiseCoefficient", "200", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_NoiseCoefficient = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_NoiseMax", "50", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_NoiseMax = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();
}

void SetTestItem_FT5X46(void)
{

	g_TestItemNum = 0;

	FTS_TEST_FUNC_ENTER();


	if (g_stCfg_FT5X46_TestItem.RESET_PIN_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_RESET_PIN_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.FACTORY_ID_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_FACTORY_ID_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.PROJECT_CODE_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_PROJECT_CODE_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.FW_VERSION_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_FW_VERSION_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.LCM_ID_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_LCM_ID_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.IC_VERSION_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_IC_VERSION_TEST);
	}



	fts_SetTestItemCodeName(Code_FT5X46_ENTER_FACTORY_MODE);


	if (g_stCfg_FT5X46_TestItem.TE_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_TE_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.CHANNEL_NUM_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_CHANNEL_NUM_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.LCD_NOISE_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_LCD_NOISE_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.NOISE_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_NOISE_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.RAWDATA_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_RAWDATA_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.UNIFORMITY_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_UNIFORMITY_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.SITO_RAWDATA_UNIFORMITY_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_SITO_RAWDATA_UNIFORMITY_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.CM_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_CM_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.ADC_DETECT_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_ADCDETECT_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.SCAP_CB_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_SCAP_CB_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.SCAP_RAWDATA_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_SCAP_RAWDATA_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.RAWDATA_MARGIN_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_RAWDATA_MARGIN_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.WEAK_SHORT_CIRCUIT_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_WEAK_SHORT_CIRCUIT_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.PANEL_DIFFER_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_PANELDIFFER_TEST);
	}

	if (g_stCfg_FT5X46_TestItem.PANEL_DIFFER_UNIFORMITY_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_PANELDIFFER_UNIFORMITY_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.INT_PIN_TEST == 1) {

		fts_SetTestItemCodeName(Code_FT5X46_INT_PIN_TEST);
	}

	FTS_TEST_FUNC_EXIT();
}

#endif

