/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Focaltech_test_config_ft8716.c
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: Set Config for FT8716
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>

#include "../include/focaltech_test_ini.h"
#include "../include/focaltech_test_supported_ic.h"
#include "../focaltech_test_config.h"

#if (FTS_CHIP_TEST_TYPE == FT8716_TEST)


struct stCfg_FT8716_TestItem g_stCfg_FT8716_TestItem;
struct stCfg_FT8716_BasicThreshold g_stCfg_FT8716_BasicThreshold;

void OnInit_FT8716_TestItem(char *strIniFile)
{
	char str[512];

	FTS_TEST_FUNC_ENTER();



	GetPrivateProfileString("TestItem", "FW_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.FW_VERSION_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "FACTORY_ID_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.FACTORY_ID_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "PROJECT_CODE_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.PROJECT_CODE_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "IC_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.IC_VERSION_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "RAWDATA_TEST", "1", str, strIniFile);
	g_stCfg_FT8716_TestItem.RAWDATA_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "CHANNEL_NUM_TEST", "1", str, strIniFile);
	g_stCfg_FT8716_TestItem.CHANNEL_NUM_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "INT_PIN_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.INT_PIN_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "RESET_PIN_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.RESET_PIN_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.NOISE_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "CB_TEST", "1", str, strIniFile);
	g_stCfg_FT8716_TestItem.CB_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "SHORT_CIRCUIT_TEST", "1", str, strIniFile);
	g_stCfg_FT8716_TestItem.SHORT_CIRCUIT_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "OPEN_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.OPEN_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "CB_UNIFORMITY_TEST", "1", str, strIniFile);
	g_stCfg_FT8716_TestItem.CB_UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "DIFFER_UNIFORMITY_TEST", "1", str, strIniFile);
	g_stCfg_FT8716_TestItem.DIFFER_UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "DIFFER2_UNIFORMITY_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.DIFFER2_UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "LCD_NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT8716_TestItem.LCD_NOISE_TEST = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();
}

void OnInit_FT8716_BasicThreshold(char *strIniFile)
{
	char str[512];

	FTS_TEST_FUNC_ENTER();



	GetPrivateProfileString("Basic_Threshold", "FW_VER_VALUE", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.FW_VER_VALUE = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Factory_ID_Number", "255", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.Factory_ID_Number = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Project_Code", " ", str, strIniFile);

	sprintf(g_stCfg_FT8716_BasicThreshold.Project_Code, "%s", str);


	GetPrivateProfileString("Basic_Threshold", "IC_Version", "3", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.IC_Version = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "RawDataTest_VA_Check", "1", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.bRawDataTest_VA_Check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "5000", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.RawDataTest_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "11000", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.RawDataTest_Max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_VKey_Check", "1", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.bRawDataTest_VKey_Check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min_VKey", "5000", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.RawDataTest_Min_VKey = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max_VKey", "11000", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.RawDataTest_Max_VKey = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX", "15", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelXNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY", "24", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelYNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_KeyNum", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.ChannelNumTest_KeyNum = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "ResetPinTest_RegAddr", "136", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.ResetPinTest_RegAddr = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "IntPinTest_RegAddr", "175", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.IntPinTest_RegAddr = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Coefficient", "50", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_Coefficient = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Frames", "32", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_Frames = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Time", "1", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_Time = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_SampeMode", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_SampeMode = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_NoiseMode", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_NoiseMode = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_NoiseMode", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_NoiseMode = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_ShowTip", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_ShowTip = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "CBTest_VA_Check", "1", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.bCBTest_VA_Check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_VKey_Check", "1", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.bCBTest_VKey_Check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Min_Vkey", "3", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Min_Vkey = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Max_Vkey", "100", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Max_Vkey = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "CBTest_VKey_Double_Check", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.bCBTest_VKey_DCheck_Check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Min_DCheck_Vkey", "140", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Min_DCheck_Vkey = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Max_DCheck_Vkey", "180", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Max_DCheck_Vkey = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "ShortCircuit_ResMin", "1200", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.ShortCircuit_ResMin = fts_atoi(str);
	/*GetPrivateProfileString("Basic_Threshold","ShortCircuit_K2Value","150",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.ShortTest_K2Value = fts_atoi(str);*/


	GetPrivateProfileString("Basic_Threshold", "OpenTest_CBMin", "100", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.OpenTest_CBMin = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "OpenTest_Check_K1", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.OpenTest_Check_K1 = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "OpenTest_K1Threshold", "30", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.OpenTest_K1Threshold = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "OpenTest_Check_K2", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.OpenTest_Check_K2 = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "OpenTest_K2Threshold", "5", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.OpenTest_K2Threshold = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "CBUniformityTest_Check_CHX", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_Check_CHX = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBUniformityTest_Check_CHY", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_Check_CHY = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBUniformityTest_Check_MinMax", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_Check_MinMax = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBUniformityTest_CHX_Hole", "20", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_CHX_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBUniformityTest_CHY_Hole", "20", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_CHY_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBUniformityTest_MinMax_Hole", "70", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_MinMax_Hole = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_Delta_Vol", "1", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.DeltaVol = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_Check_CHX", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_Check_CHX = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_Check_CHY", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_Check_CHY = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_Check_MinMax", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_Check_MinMax = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_CHX_Hole", "20", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_CHX_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_CHY_Hole", "20", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_CHY_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_MinMax_Hole", "70", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_MinMax_Hole = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Differ2UniformityTest_Check_CHX", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_Check_CHX = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Differ2UniformityTest_Check_CHY", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_Check_CHY = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "Differ2UniformityTest_CHX_Hole", "20", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_CHX_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Differ2UniformityTest_CHY_Hole", "20", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_CHY_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Differ2UniformityTest_Differ_Min", "1000", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_Differ_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Differ2UniformityTest_Differ_Max", "8000", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_Differ_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "LCDNoiseTest_FrameNum", "200", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_FrameNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "LCDNoiseTest_Coefficient", "60", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_Coefficient = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "LCDNoiseTest_Coefficient_Key", "60", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_Coefficient_Key = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "LCDNoiseTest_NoiseMode", "0", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_NoiseMode = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "LCDNoiseTest_SequenceFrame", "5", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_SequenceFrame = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "LCDNoiseTest_MaxFrame", "6", str, strIniFile);
	g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_MaxFrame = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();

}
void SetTestItem_FT8716(void)
{
	g_TestItemNum = 0;

	FTS_TEST_FUNC_ENTER();


	if (g_stCfg_FT8716_TestItem.FACTORY_ID_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_FACTORY_ID_TEST);
	}


	if (g_stCfg_FT8716_TestItem.PROJECT_CODE_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_PROJECT_CODE_TEST);
	}


	if (g_stCfg_FT8716_TestItem.FW_VERSION_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_FW_VERSION_TEST);
	}


	if (g_stCfg_FT8716_TestItem.IC_VERSION_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_IC_VERSION_TEST);
	}


	fts_SetTestItemCodeName(Code_FT8716_ENTER_FACTORY_MODE);


	if (g_stCfg_FT8716_TestItem.CHANNEL_NUM_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_CHANNEL_NUM_TEST);
	}


	if (g_stCfg_FT8716_TestItem.CB_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_CB_TEST);
	}


	if (g_stCfg_FT8716_TestItem.CB_UNIFORMITY_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_CB_UNIFORMITY_TEST);
	}


	if (g_stCfg_FT8716_TestItem.RAWDATA_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_RAWDATA_TEST);
	}


	if (g_stCfg_FT8716_TestItem.NOISE_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_NOISE_TEST);
	}


	if (g_stCfg_FT8716_TestItem.LCD_NOISE_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_LCD_NOISE_TEST);
	}



	if (g_stCfg_FT8716_TestItem.DIFFER_UNIFORMITY_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_DIFFER_UNIFORMITY_TEST);
	}



	if (g_stCfg_FT8716_TestItem.OPEN_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_OPEN_TEST);
	}


	if (g_stCfg_FT8716_TestItem.SHORT_CIRCUIT_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_SHORT_CIRCUIT_TEST) ;
	}


	if (g_stCfg_FT8716_TestItem.DIFFER2_UNIFORMITY_TEST == 1) {
		 fts_SetTestItemCodeName(Code_FT8716_DIFFER2_UNIFORMITY_TEST);
	}

	FTS_TEST_FUNC_EXIT();

}

#endif

