/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Config_FT6X36.c
*
* Author: Software Development Team, AE
*
* Created: 2015-10-08
*
* Abstract: Set Config for FT6X36/FT3X07/FT6416/FT6426
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include "Config_FT6X36.h"
#include "ini.h"
#include "Global.h"


struct stCfg_FT6X36_TestItem g_stCfg_FT6X36_TestItem;
struct stCfg_FT6X36_BasicThreshold g_stCfg_FT6X36_BasicThreshold;


void OnInit_FT6X36_TestItem(char *strIniFile)
{
	char str[512] = {0};

	GetPrivateProfileString("TestItem", "FW_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.FW_VERSION_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "FACTORY_ID_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.FACTORY_ID_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "PROJECT_CODE_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.PROJECT_CODE_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "IC_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.IC_VERSION_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "RAWDATA_TEST", "1", str, strIniFile);
	g_stCfg_FT6X36_TestItem.RAWDATA_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "CHANNEL_NUM_TEST", "1", str, strIniFile);
	g_stCfg_FT6X36_TestItem.CHANNEL_NUM_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "CHANNEL_SHORT_TEST", "1", str, strIniFile);
	g_stCfg_FT6X36_TestItem.CHANNEL_SHORT_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "INT_PIN_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.INT_PIN_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "RESET_PIN_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.RESET_PIN_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.NOISE_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "CB_TEST", "1", str, strIniFile);
	g_stCfg_FT6X36_TestItem.CB_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "DELTA_CB_TEST", "1", str, strIniFile);
	g_stCfg_FT6X36_TestItem.DELTA_CB_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "CHANNELS_DEVIATION_TEST", "1", str, strIniFile);
	g_stCfg_FT6X36_TestItem.CHANNELS_DEVIATION_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "TWO_SIDES_DEVIATION_TEST", "1", str, strIniFile);
	g_stCfg_FT6X36_TestItem.TWO_SIDES_DEVIATION_TEST = atoi(str);


	GetPrivateProfileString("TestItem", "FPC_SHORT_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.FPC_SHORT_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "FPC_OPEN_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.FPC_OPEN_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "SREF_OPEN_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.SREF_OPEN_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "TE_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.TE_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "CB_DEVIATION_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.CB_DEVIATION_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "DIFFER_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.DIFFER_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "WEAK_SHORT_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.WEAK_SHORT_TEST = atoi(str);

	GetPrivateProfileString("TestItem", "DIFFER_TEST2", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.DIFFER_TEST2 = atoi(str);

	GetPrivateProfileString("TestItem", "K1_DIFFER_TEST", "0", str, strIniFile);
	g_stCfg_FT6X36_TestItem.K1_DIFFER_TEST = atoi(str);
}

void OnInit_FT6X36_BasicThreshold(char *strIniFile)
{
	char str[512] = {0};


	GetPrivateProfileString("Basic_Threshold", "FW_VER_VALUE", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FW_VER_VALUE = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "Factory_ID_Number", "255", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.Factory_ID_Number = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "Project_Code", " ", str, strIniFile);

	sprintf(g_stCfg_FT6X36_BasicThreshold.Project_Code, "%s", str);

	GetPrivateProfileString("Basic_Threshold", "IC_Version", "3", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.IC_Version = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "13000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.RawDataTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "17000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.RawDataTest_Max = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelNum", "22", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelNumTest_ChannelNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_KeyNum", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelNumTest_KeyNum = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelShortTest_K1", "255", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelShortTest_K1 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelShortTest_K2", "255", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelShortTest_K2 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelShortTest_CB", "255", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelShortTest_CB = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ResetPinTest_RegAddr", "136", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ResetPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "IntPinTest_RegAddr", "175", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.IntPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Max", "20", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.NoiseTest_Max = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Frames", "32", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.NoiseTest_Frames = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Time", "1", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.NoiseTest_Time = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_SampeMode", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.NoiseTest_SampeMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_NoiseMode", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.NoiseTest_NoiseMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_ShowTip", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.NoiseTest_ShowTip = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "CbTest_Min", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.CbTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CbTest_Max", "250", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.CbTest_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Base", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Base = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Differ_Max", "50", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Differ_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Include_Key_Test", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Include_Key_Test = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Key_Differ_Max", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Key_Differ_Max = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Deviation_S1", "15", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S1 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Deviation_S2", "15", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S2 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Deviation_S3", "12", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S3 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Deviation_S4", "12", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S4 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Deviation_S5", "12", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S5 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Deviation_S6", "12", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S6 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Set_Critical", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Set_Critical = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Critical_S1", "20", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S1 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Critical_S2", "20", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S2 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Critical_S3", "20", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S3 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Critical_S4", "20", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S4 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Critical_S5", "20", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S5 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DeltaCbTest_Critical_S6", "20", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S6 = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Deviation_S1", "8", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S1 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Deviation_S2", "8", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S2 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Deviation_S3", "8", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S3 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Deviation_S4", "8", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S4 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Deviation_S5", "8", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S5 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Deviation_S6", "8", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S6 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Set_Critical", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Set_Critical = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Critical_S1", "13", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S1 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Critical_S2", "13", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S2 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Critical_S3", "13", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S3 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Critical_S4", "13", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S4 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Critical_S5", "13", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S5 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelsDeviationTest_Critical_S6", "13", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S6 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Deviation_S1", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S1 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Deviation_S2", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S2 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Deviation_S3", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S3 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Deviation_S4", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S4 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Deviation_S5", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S5 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Deviation_S6", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S6 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Set_Critical", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Set_Critical = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Critical_S1", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S1 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Critical_S2", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S2 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Critical_S3", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S3 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Critical_S4", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S4 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Critical_S5", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S5 = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "TwoSidesDeviationTest_Critical_S6", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S6 = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "FPCShortTest_Min_CB", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FPCShort_CB_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "FPCShortTest_Max_CB", "1015", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FPCShort_CB_Max = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "FPCShortTest_Min_RawData", "5000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FPCShort_RawData_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "FPCShortTest_Max_RawData", "50000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FPCShort_RawData_Max = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "FPCOpenTest_Min_CB", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FPCOpen_CB_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "FPCOpenTest_Max_CB", "1015", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FPCOpen_CB_Max = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "FPCOpenTest_Min_RawData", "5000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FPCOpen_RawData_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "FPCOpenTest_Max_RawData", "50000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.FPCOpen_RawData_Max = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "SREFOpen_Test_Hole", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.SREFOpen_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SREFOpen_Test_Hole_Base1", "50", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.SREFOpen_Hole_Base1 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SREFOpen_Test_Hole_Base2", "50", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.SREFOpen_Hole_Base2 = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "CBDeviationTest_Hole", "50", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.CBDeviationTest_Hole = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "DifferTest_Ave_Hole", "500", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.Differ_Ave_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferTest_Max_Hole", "500", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.Differ_Max_Hole = atoi(str);



	GetPrivateProfileString("Basic_Threshold", "Weak_Short_Hole", "500", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.WeakShortThreshold = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_H_Min", "20000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DifferTest2_Data_H_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_H_Max", "24000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DifferTest2_Data_H_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_M_Min", "7100", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DifferTest2_Data_M_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_M_Max", "7300", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DifferTest2_Data_M_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_L_Min", "14000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DifferTest2_Data_L_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_L_Max", "16000", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.DifferTest2_Data_L_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_H", "1", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.bDifferTest2_Data_H = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_M", "1", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.bDifferTest2_Data_M = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "DifferTest2_Data_L", "1", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.bDifferTest2_Data_L = atoi(str);


	GetPrivateProfileString("Basic_Threshold", "K1DifferTest_StartK1", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.K1DifferTest_StartK1 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "K1DifferTest_EndK1", "25", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.K1DifferTest_EndK1 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "K1DifferTest_MinHole_STC2", "0", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.K1DifferTest_MinHold2 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "K1DifferTest_MaxHole_STC2", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.K1DifferTest_MaxHold2 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "K1DifferTest_MinHole_STC4", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.K1DifferTest_MinHold4 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "K1DifferTest_MaxHole_STC4", "10", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.K1DifferTest_MaxHold4 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "K1DifferTest_Deviation2", "1", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.K1DifferTest_Deviation2 = atoi(str);
	GetPrivateProfileString("Basic_Threshold", "K1DifferTest_Deviation4", "5", str, strIniFile);
	g_stCfg_FT6X36_BasicThreshold.K1DifferTest_Deviation4 = atoi(str);
}

void SetTestItem_FT6X36()
{

	g_TestItemNum = 0;

	if (g_stCfg_FT6X36_TestItem.FACTORY_ID_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_FACTORY_ID_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.PROJECT_CODE_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_PROJECT_CODE_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.FW_VERSION_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_FW_VERSION_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.IC_VERSION_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_IC_VERSION_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_ENTER_FACTORY_MODE;

	g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
	g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
	g_TestItemNum++;

	if (g_stCfg_FT6X36_TestItem.CHANNEL_NUM_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_CHANNEL_NUM_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.RAWDATA_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_RAWDATA_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.DIFFER_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_DIFFER_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.DIFFER_TEST2 == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_DIFFER_TEST2;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.CB_DEVIATION_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_CB_DEVIATION_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.FPC_SHORT_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_FPC_SHORT_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.FPC_OPEN_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_FPC_OPEN_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.CB_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_CB_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.DELTA_CB_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_DELTA_CB_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.CHANNELS_DEVIATION_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_CHANNELS_DEVIATION_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.TWO_SIDES_DEVIATION_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_TWO_SIDES_DEVIATION_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.CHANNEL_SHORT_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_CHANNEL_SHORT_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.SREF_OPEN_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_SREF_OPEN_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.NOISE_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_NOISE_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.WEAK_SHORT_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_WEAK_SHORT_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.K1_DIFFER_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_K1_DIFFER_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.RESET_PIN_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_RESET_PIN_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}
	if (g_stCfg_FT6X36_TestItem.INT_PIN_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_INT_PIN_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT6X36_TestItem.TE_TEST == 1) {
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT6X36_TE_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

}

