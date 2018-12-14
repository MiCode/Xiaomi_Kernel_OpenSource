/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)，All Rights Reserved.
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


struct stCfg_FT8606_TestItem
{
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

	/*
	stCfg_FT8606_TestItem()
	{
		FW_VERSION_TEST = 0;
		FACTORY_ID_TEST = 0;
		PROJECT_CODE_TEST = 0;
		IC_VERSION_TEST = 0;
		RAWDATA_TEST = 0;
		CHANNEL_NUM_TEST = 0;
		INT_PIN_TEST = 0;
		RESET_PIN_TEST = 0;
		NOISE_TEST = 0;
		CB_TEST = 0;
		SHORT_TEST = 0;
	}
	*/

};
struct stCfg_FT8606_BasicThreshold
{
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

	/*
	stCfg_FT8606_BasicThreshold()
	{
		FW_VER_VALUE = 0;
		Factory_ID_Number = 0;
		Project_Code = "";
		IC_Version = 0;
		RawDataTest_Min = 0;
		RawDataTest_Max = 0;
		ChannelNumTest_ChannelXNum = 0;
		ChannelNumTest_ChannelYNum = 0;
		ChannelNumTest_KeyNum = 0;
		ResetPinTest_RegAddr = 0;
		IntPinTest_RegAddr = 0;
		NoiseTest_Coefficient = 0;
		NoiseTest_Frames = 0;
		NoiseTest_Time = 0;
		NoiseTest_SampeMode = 0;
		NoiseTest_NoiseMode = 0;
		NoiseTest_ShowTip = 0;
		CbTest_Min = 0;
		CbTest_Max = 0;
		ShortTest_Max = 0;
		ShortTest_K2Value = 0;
	}
	*/


};


enum enumTestItem_FT8606
{
	Code_FT8606_ENTER_FACTORY_MODE,所有IC都必备的测试项
	Code_FT8606_DOWNLOAD,所有IC都必备的测试项
	Code_FT8606_UPGRADE,所有IC都必备的测试项
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








	Code_FT8606_WRITE_CONFIG,所有IC都必备的测试项

	Code_FT8606_SHORT_CIRCUIT_TEST,
};




extern struct stCfg_FT8606_TestItem g_stCfg_FT8606_TestItem;
extern struct stCfg_FT8606_BasicThreshold g_stCfg_FT8606_BasicThreshold;

void OnInit_FT8606_TestItem(char *strIniFile);
void OnInit_FT8606_BasicThreshold(char *strIniFile);
void SetTestItem_FT8606(void);

#endif
