/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Focaltech_test_config_ft8006.c
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: Set Config for FT8006
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>

#include "../include/focaltech_test_ini.h"
#include "../include/focaltech_test_supported_ic.h"
#include "../focaltech_test_config.h"

#if (FTS_CHIP_TEST_TYPE == FT8006_TEST)


struct stCfg_FT8006_TestItem g_stCfg_FT8006_TestItem;
struct stCfg_FT8006_BasicThreshold g_stCfg_FT8006_BasicThreshold;

void OnInit_FT8006_TestItem(char *strIniFile)
{
	char str[512];

	FTS_TEST_FUNC_ENTER();



	Ft8006m_GetPrivateProfileString("TestItem", "FW_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.FW_VERSION_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "FACTORY_ID_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.FACTORY_ID_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "PROJECT_CODE_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.PROJECT_CODE_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "IC_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.IC_VERSION_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "RAWDATA_TEST", "1", str, strIniFile);
	g_stCfg_FT8006_TestItem.RAWDATA_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "CHANNEL_NUM_TEST", "1", str, strIniFile);
	g_stCfg_FT8006_TestItem.CHANNEL_NUM_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "INT_PIN_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.INT_PIN_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "RESET_PIN_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.RESET_PIN_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.NOISE_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "CB_TEST", "1", str, strIniFile);
	g_stCfg_FT8006_TestItem.CB_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "SHORT_CIRCUIT_TEST", "1", str, strIniFile);
	g_stCfg_FT8006_TestItem.SHORT_CIRCUIT_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "LCD_NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.LCD_NOISE_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "OSC60MHZ_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.OSC60MHZ_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "OSCTRM_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.OSCTRM_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "SNR_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.SNR_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "DIFFER_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.DIFFER_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "DIFFER_UNIFORMITY_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.DIFFER_UNIFORMITY_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "LPWG_RAWDATA_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.LPWG_RAWDATA_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "LPWG_CB_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.LPWG_CB_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "LPWG_NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.LPWG_NOISE_TEST = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("TestItem", "DIFFER2_TEST", "0", str, strIniFile);
	g_stCfg_FT8006_TestItem.DIFFER2_TEST = ft8006m_atoi(str);


	FTS_TEST_FUNC_EXIT();
}

void OnInit_FT8006_BasicThreshold(char *strIniFile)
{
	char str[512];

	FTS_TEST_FUNC_ENTER();


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "FW_VER_VALUE", "0", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.FW_VER_VALUE = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "Factory_ID_Number", "255", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.Factory_ID_Number = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "Project_Code", " ", str, strIniFile);
	sprintf(g_stCfg_FT8006_BasicThreshold.Project_Code, "%s", str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "IC_Version", "3", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.IC_Version = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "5000", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.RawDataTest_Min = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "11000", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.RawDataTest_Max = ft8006m_atoi(str);



	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX", "15", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.ChannelNumTest_ChannelXNum = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY", "24", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.ChannelNumTest_ChannelYNum = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_KeyNum", "0", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.ChannelNumTest_KeyNum = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ResetPinTest_RegAddr", "136", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.ResetPinTest_RegAddr = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "IntPinTest_RegAddr", "175", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.IntPinTest_RegAddr = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "NoiseTest_Coefficient", "50", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.NoiseTest_Coefficient = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "NoiseTest_Frames", "32", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.NoiseTest_Frames = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "NoiseTest_Time", "1", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.NoiseTest_Time = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "NoiseTest_SampeMode", "0", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.NoiseTest_SampeMode = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "NoiseTest_NoiseMode", "0", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.NoiseTest_NoiseMode = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "NoiseTest_ShowTip", "0", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.NoiseTest_ShowTip = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "NoiseTest_IsDiffer", "1", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.IsDifferMode = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_VA_Check", "1", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.bCBTest_VA_Check = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.CbTest_Min = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.CbTest_Max = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_VKey_Check", "1", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.bCBTest_VKey_Check = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Min_Vkey", "3", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.CbTest_Min_Vkey = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Max_Vkey", "100", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.CbTest_Max_Vkey = ft8006m_atoi(str);



	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ShortCircuit_ResMin", "200", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.ShortCircuit_ResMin = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LCD_NoiseTest_Frame", "50", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.iLCDNoiseTestFrame = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LCD_NoiseTest_Max_Screen", "32", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.iLCDNoiseTestMaxScreen = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LCD_NoiseTest_Max_Frame", "32", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.iLCDNoiseTestMaxFrame = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LCD_NoiseTest_Coefficient", "50", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.iLCDNoiseCoefficient = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "OSC60MHZTest_OSCMin", "12", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.OSC60MHZTest_OSCMin = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "OSC60MHZTest_OSCMax", "17", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.OSC60MHZTest_OSCMax = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "OSCTRMTest_OSCMin", "15", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.OSCTRMTest_OSCMin = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "OSCTRMTest_OSCMax", "17", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.OSCTRMTest_OSCMax = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "OSCTRMTest_OSCDetMin", "15", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.OSCTRMTest_OSCDetMin = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "OSCTRMTest_OSCDetMax", "17", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.OSCTRMTest_OSCDetMax = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SNRTest_FrameNum", "32", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.SNRTest_FrameNum = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SNRTest_Min", "10", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.SNRTest_Min = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DIFFERTest_Frame_Num", "32", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DIFFERTest_FrameNum = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DIFFERTest_Differ_Max", "100", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DIFFERTest_DifferMax = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DIFFERTest_Differ_Min", "10", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DIFFERTest_DifferMin = ft8006m_atoi(str);



	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_Check_CHX", "0", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DifferUniformityTest_Check_CHX = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_Check_CHY", "0", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DifferUniformityTest_Check_CHY = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_Check_MinMax", "0", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DifferUniformityTest_Check_MinMax = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_CHX_Hole", "20", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DifferUniformityTest_CHX_Hole = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_CHY_Hole", "20", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DifferUniformityTest_CHY_Hole = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "DifferUniformityTest_MinMax_Hole", "70", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.DifferUniformityTest_MinMax_Hole = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_RawDataTest_Min", "5000", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.LPWG_RawDataTest_Min = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_RawDataTest_Max", "11000", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.LPWG_RawDataTest_Max = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_CBTest_VA_Check", "1", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.bLPWG_CBTest_VA_Check = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_CBTest_Min", "3", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.LPWG_CbTest_Min = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_CBTest_Max", "60", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.LPWG_CbTest_Max = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_CBTest_VKey_Check", "1", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.bLPWG_CBTest_VKey_Check = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_CBTest_Min_Vkey", "3", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.LPWG_CbTest_Min_Vkey = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_CBTest_Max_Vkey", "100", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.LPWG_CbTest_Max_Vkey = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_NoiseTest_Coefficient", "50", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.LPWG_NoiseTest_Coefficient = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "LPWG_NoiseTest_Frames", "50", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.LPWG_NoiseTest_Frames = ft8006m_atoi(str);


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "Differ2Test_Min", "1500", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.Differ2Test_Min = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "Differ2Test_Max", "2000", str, strIniFile);
	g_stCfg_FT8006_BasicThreshold.Differ2Test_Max = ft8006m_atoi(str);




	FTS_TEST_FUNC_EXIT();

}
void SetTestItem_FT8006(void)
{
	ft8006m_g_TestItemNum = 0;

	FTS_TEST_FUNC_ENTER();


	if (g_stCfg_FT8006_TestItem.FACTORY_ID_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_FACTORY_ID_TEST);
	}


	if (g_stCfg_FT8006_TestItem.PROJECT_CODE_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_PROJECT_CODE_TEST);
	}


	if (g_stCfg_FT8006_TestItem.FW_VERSION_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_FW_VERSION_TEST);
	}


	if (g_stCfg_FT8006_TestItem.IC_VERSION_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_IC_VERSION_TEST);
	}


	ft8006m_SetTestItemCodeName(Code_FT8006_ENTER_FACTORY_MODE);


	if (g_stCfg_FT8006_TestItem.CHANNEL_NUM_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_CHANNEL_NUM_TEST);
	}


	if (g_stCfg_FT8006_TestItem.SHORT_CIRCUIT_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_SHORT_CIRCUIT_TEST) ;
	}


	if (g_stCfg_FT8006_TestItem.OSC60MHZ_TEST == 1) {

		ft8006m_SetTestItemCodeName(Code_FT8006_OSC60MHZ_TEST);
	}


	if (g_stCfg_FT8006_TestItem.OSCTRM_TEST == 1) {

		ft8006m_SetTestItemCodeName(Code_FT8006_OSCTRM_TEST);
	}


	if (g_stCfg_FT8006_TestItem.DIFFER2_TEST == 1) {

		ft8006m_SetTestItemCodeName(Code_FT8006_DIFFER2_TEST);
	}


	if (g_stCfg_FT8006_TestItem.CB_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_CB_TEST);
	}


	if (g_stCfg_FT8006_TestItem.NOISE_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_NOISE_TEST);
	}


	if (g_stCfg_FT8006_TestItem.LCD_NOISE_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_LCD_NOISE_TEST);
	}


	if (g_stCfg_FT8006_TestItem.RAWDATA_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_RAWDATA_TEST);
	}


	if (g_stCfg_FT8006_TestItem.SNR_TEST == 1) {

		ft8006m_SetTestItemCodeName(Code_FT8006_SNR_TEST);
	}



	if (g_stCfg_FT8006_TestItem.DIFFER_TEST == 1) {

		ft8006m_SetTestItemCodeName(Code_FT8006_DIFFER_TEST);
	}



	if (g_stCfg_FT8006_TestItem.DIFFER_UNIFORMITY_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_DIFFER_UNIFORMITY_TEST);
	}


	if (g_stCfg_FT8006_TestItem.RESET_PIN_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_RESET_PIN_TEST);
	}


	if (g_stCfg_FT8006_TestItem.INT_PIN_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_RESET_PIN_TEST);
	}


	if (g_stCfg_FT8006_TestItem.RESET_PIN_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_INT_PIN_TEST);
	}


	if (g_stCfg_FT8006_TestItem.LPWG_CB_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_LPWG_CB_TEST);
	}


	if (g_stCfg_FT8006_TestItem.LPWG_NOISE_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_LPWG_NOISE_TEST);
	}


	if (g_stCfg_FT8006_TestItem.LPWG_RAWDATA_TEST == 1) {
		ft8006m_SetTestItemCodeName(Code_FT8006_LPWG_RAWDATA_TEST);
	}



	FTS_TEST_FUNC_EXIT();

}

#endif

