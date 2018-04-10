/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Config_FT6X36.h
*
* Author: Software Development Team, AE
*
* Created: 2015-10-08
*
* Abstract: Set Config for FT6X36/FT3X07/FT6416/FT6426
*
************************************************************************/
#ifndef _CONFIG_FT6X36_H
#define _CONFIG_FT6X36_H

#include "test_lib.h"

struct stCfg_FT6X36_TestItem {
	bool FW_VERSION_TEST;
	bool FACTORY_ID_TEST;
	bool PROJECT_CODE_TEST;
	bool IC_VERSION_TEST;
	bool RAWDATA_TEST;
	bool CHANNEL_NUM_TEST;
	bool CHANNEL_SHORT_TEST;
	bool INT_PIN_TEST;
	bool RESET_PIN_TEST;
	bool NOISE_TEST;
	bool CB_TEST;
	bool DELTA_CB_TEST;
	bool CHANNELS_DEVIATION_TEST;
	bool TWO_SIDES_DEVIATION_TEST;
	bool FPC_SHORT_TEST;
	bool FPC_OPEN_TEST;
	bool SREF_OPEN_TEST;
	bool TE_TEST;
	bool CB_DEVIATION_TEST;
	bool DIFFER_TEST;
	bool WEAK_SHORT_TEST;
	bool DIFFER_TEST2;
	bool K1_DIFFER_TEST;
};

struct stCfg_FT6X36_BasicThreshold {
	BYTE FW_VER_VALUE;
	BYTE Factory_ID_Number;
	char Project_Code[32];
	BYTE IC_Version;
	int RawDataTest_Min;
	int RawDataTest_Max;
	BYTE ChannelNumTest_ChannelNum;
	BYTE ChannelNumTest_KeyNum;
	int ChannelShortTest_K1;
	int ChannelShortTest_K2;
	int ChannelShortTest_CB;
	BYTE ResetPinTest_RegAddr;
	BYTE IntPinTest_RegAddr;
	int WeakShortThreshold;
	int NoiseTest_Max;
	int NoiseTest_Frames;
	int NoiseTest_Time;
	BYTE NoiseTest_SampeMode;
	BYTE NoiseTest_NoiseMode;
	BYTE NoiseTest_ShowTip;
	int FPCShort_CB_Min;
	int FPCShort_CB_Max;
	int FPCShort_RawData_Min;
	int FPCShort_RawData_Max;
	int FPCOpen_CB_Min;
	int FPCOpen_CB_Max;
	int FPCOpen_RawData_Min;
	int FPCOpen_RawData_Max;
	int SREFOpen_Hole_Base1;
	int SREFOpen_Hole_Base2;
	int SREFOpen_Hole;
	int CBDeviationTest_Hole;
	int Differ_Ave_Hole;
	int Differ_Max_Hole;
	int CbTest_Min;
	int CbTest_Max;
	int DeltaCbTest_Base;
	int DeltaCbTest_Differ_Max;
	bool DeltaCbTest_Include_Key_Test;
	int DeltaCbTest_Key_Differ_Max;
	int DeltaCbTest_Deviation_S1;
	int DeltaCbTest_Deviation_S2;
	int DeltaCbTest_Deviation_S3;
	int DeltaCbTest_Deviation_S4;
	int DeltaCbTest_Deviation_S5;
	int DeltaCbTest_Deviation_S6;
	bool DeltaCbTest_Set_Critical;
	int DeltaCbTest_Critical_S1;
	int DeltaCbTest_Critical_S2;
	int DeltaCbTest_Critical_S3;
	int DeltaCbTest_Critical_S4;
	int DeltaCbTest_Critical_S5;
	int DeltaCbTest_Critical_S6;

	int ChannelsDeviationTest_Deviation_S1;
	int ChannelsDeviationTest_Deviation_S2;
	int ChannelsDeviationTest_Deviation_S3;
	int ChannelsDeviationTest_Deviation_S4;
	int ChannelsDeviationTest_Deviation_S5;
	int ChannelsDeviationTest_Deviation_S6;
	bool ChannelsDeviationTest_Set_Critical;
	int ChannelsDeviationTest_Critical_S1;
	int ChannelsDeviationTest_Critical_S2;
	int ChannelsDeviationTest_Critical_S3;
	int ChannelsDeviationTest_Critical_S4;
	int ChannelsDeviationTest_Critical_S5;
	int ChannelsDeviationTest_Critical_S6;

	int TwoSidesDeviationTest_Deviation_S1;
	int TwoSidesDeviationTest_Deviation_S2;
	int TwoSidesDeviationTest_Deviation_S3;
	int TwoSidesDeviationTest_Deviation_S4;
	int TwoSidesDeviationTest_Deviation_S5;
	int TwoSidesDeviationTest_Deviation_S6;
	bool TwoSidesDeviationTest_Set_Critical;
	int TwoSidesDeviationTest_Critical_S1;
	int TwoSidesDeviationTest_Critical_S2;
	int TwoSidesDeviationTest_Critical_S3;
	int TwoSidesDeviationTest_Critical_S4;
	int TwoSidesDeviationTest_Critical_S5;
	int TwoSidesDeviationTest_Critical_S6;

	int DifferTest2_Data_H_Min;
	int DifferTest2_Data_H_Max;
	int DifferTest2_Data_M_Min;
	int DifferTest2_Data_M_Max;
	int DifferTest2_Data_L_Min;
	int DifferTest2_Data_L_Max;
	bool bDifferTest2_Data_H;
	bool bDifferTest2_Data_M;
	bool bDifferTest2_Data_L;
	int  K1DifferTest_StartK1;
	int  K1DifferTest_EndK1;
	int  K1DifferTest_MinHold2;
	int  K1DifferTest_MaxHold2;
	int  K1DifferTest_MinHold4;
	int  K1DifferTest_MaxHold4;
	int  K1DifferTest_Deviation2;
	int  K1DifferTest_Deviation4;
};

enum enumTestItem_FT6X36 {
	Code_FT6X36_ENTER_FACTORY_MODE,
	Code_FT6X36_DOWNLOAD,
	Code_FT6X36_UPGRADE,
	Code_FT6X36_FACTORY_ID_TEST,
	Code_FT6X36_PROJECT_CODE_TEST,
	Code_FT6X36_FW_VERSION_TEST,
	Code_FT6X36_IC_VERSION_TEST,
	Code_FT6X36_RAWDATA_TEST,
	Code_FT6X36_CHANNEL_NUM_TEST,
	Code_FT6X36_CHANNEL_SHORT_TEST,
	Code_FT6X36_INT_PIN_TEST,
	Code_FT6X36_RESET_PIN_TEST,
	Code_FT6X36_NOISE_TEST,
	Code_FT6X36_CB_TEST,
	Code_FT6X36_DELTA_CB_TEST,
	Code_FT6X36_CHANNELS_DEVIATION_TEST,
	Code_FT6X36_TWO_SIDES_DEVIATION_TEST,
	Code_FT6X36_FPC_SHORT_TEST,
	Code_FT6X36_FPC_OPEN_TEST,
	Code_FT6X36_SREF_OPEN_TEST,
	Code_FT6X36_TE_TEST,
	Code_FT6X36_CB_DEVIATION_TEST,
	Code_FT6X36_WRITE_CONFIG,
	Code_FT6X36_DIFFER_TEST,
	Code_FT6X36_WEAK_SHORT_TEST,
	Code_FT6X36_DIFFER_TEST2,
	Code_FT6X36_K1_DIFFER_TEST,
};

extern struct stCfg_FT6X36_TestItem g_stCfg_FT6X36_TestItem;
extern struct stCfg_FT6X36_BasicThreshold g_stCfg_FT6X36_BasicThreshold;

void OnInit_FT6X36_TestItem(char *strIniFile);
void OnInit_FT6X36_BasicThreshold(char *strIniFile);
void SetTestItem_FT6X36(void);

#endif
