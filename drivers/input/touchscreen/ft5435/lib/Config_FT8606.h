/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Config_FT8606.h
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: Set Config for FT8606
*
************************************************************************/
#ifndef _CONFIG_FT8606_H
#define _CONFIG_FT8606_H

#include "test_lib.h"


struct stCfg_FT8606_TestItem {
	bool FW_VERSION_TEST;
	bool FACTORY_ID_TEST;
	bool PROJECT_CODE_TEST;
	bool IC_VERSION_TEST;
	bool RAWDATA_TEST;
	bool CHANNEL_NUM_TEST;
	bool INT_PIN_TEST;
	bool RESET_PIN_TEST;
	bool NOISE_TEST;
	bool CB_TEST;
	bool SHORT_TEST;


};
struct stCfg_FT8606_BasicThreshold {
	BYTE FW_VER_VALUE;
	BYTE Factory_ID_Number;
	char Project_Code[32];
	BYTE IC_Version;
	int RawDataTest_Min;
	int RawDataTest_Max;
	BYTE ChannelNumTest_ChannelXNum;
	BYTE ChannelNumTest_ChannelYNum;
	BYTE ChannelNumTest_KeyNum;
	BYTE ResetPinTest_RegAddr;
	BYTE IntPinTest_RegAddr;
	int NoiseTest_Coefficient;
	int NoiseTest_Frames;
	int NoiseTest_Time;
	BYTE NoiseTest_SampeMode;
	BYTE NoiseTest_NoiseMode;
	BYTE NoiseTest_ShowTip;
	int CbTest_Min;
	int CbTest_Max;
	int ShortTest_Max;
	int ShortTest_K2Value;


};


enum enumTestItem_FT8606 {
	Code_FT8606_ENTER_FACTORY_MODE,
	Code_FT8606_DOWNLOAD,
	Code_FT8606_UPGRADE,
	Code_FT8606_FACTORY_ID_TEST,
	Code_FT8606_PROJECT_CODE_TEST,
	Code_FT8606_FW_VERSION_TEST,
	Code_FT8606_IC_VERSION_TEST,
	Code_FT8606_RAWDATA_TEST,
	Code_FT8606_CHANNEL_NUM_TEST,

	Code_FT8606_INT_PIN_TEST,
	Code_FT8606_RESET_PIN_TEST,
	Code_FT8606_NOISE_TEST,
	Code_FT8606_CB_TEST,



	Code_FT8606_WRITE_CONFIG,

	Code_FT8606_SHORT_CIRCUIT_TEST,
};

extern struct stCfg_FT8606_TestItem g_stCfg_FT8606_TestItem;
extern struct stCfg_FT8606_BasicThreshold g_stCfg_FT8606_BasicThreshold;

void OnInit_FT8606_TestItem(char *strIniFile);
void OnInit_FT8606_BasicThreshold(char *strIniFile);
void SetTestItem_FT8606(void);

#endif
