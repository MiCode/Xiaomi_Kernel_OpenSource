/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Config_FT5X46.c
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
#include "Config_FT5822.h"
#include "ini.h"
#include "Global.h"


struct stCfg_FT5822_TestItem g_stCfg_FT5822_TestItem;
struct stCfg_FT5822_BasicThreshold g_stCfg_FT5822_BasicThreshold;

void OnInit_FT5822_TestItem(char *strIniFile)
{
	char str[512];


	GetPrivateProfileString("TestItem","FW_VERSION_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.FW_VERSION_TEST = atoi(str);


	GetPrivateProfileString("TestItem","FACTORY_ID_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.FACTORY_ID_TEST = atoi(str);


	GetPrivateProfileString("TestItem","PROJECT_CODE_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.PROJECT_CODE_TEST = atoi(str);


	GetPrivateProfileString("TestItem","IC_VERSION_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.IC_VERSION_TEST = atoi(str);


	GetPrivateProfileString("TestItem","LCM_ID_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.LCM_ID_TEST = atoi(str);


	GetPrivateProfileString("TestItem","RAWDATA_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.RAWDATA_TEST = atoi(str);


	GetPrivateProfileString("TestItem","ADC_DETECT_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.ADC_DETECT_TEST = atoi(str);










	GetPrivateProfileString("TestItem","SCAP_CB_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.SCAP_CB_TEST = atoi(str);


	GetPrivateProfileString("TestItem","SCAP_RAWDATA_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.SCAP_RAWDATA_TEST = atoi(str);


	GetPrivateProfileString("TestItem","CHANNEL_NUM_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.CHANNEL_NUM_TEST = atoi(str);


	GetPrivateProfileString("TestItem","INT_PIN_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.INT_PIN_TEST = atoi(str);


	GetPrivateProfileString("TestItem","RESET_PIN_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.RESET_PIN_TEST = atoi(str);


	GetPrivateProfileString("TestItem","NOISE_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.NOISE_TEST = atoi(str);


































	GetPrivateProfileString("TestItem","WEAK_SHORT_CIRCUIT_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.WEAK_SHORT_CIRCUIT_TEST = atoi(str);


	GetPrivateProfileString("TestItem","UNIFORMITY_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.UNIFORMITY_TEST = atoi(str);


	GetPrivateProfileString("TestItem","CM_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.CM_TEST = atoi(str);


	GetPrivateProfileString("TestItem","RAWDATA_MARGIN_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.RAWDATA_MARGIN_TEST = atoi(str);


	GetPrivateProfileString("TestItem","PANEL_DIFFER_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.PANEL_DIFFER_TEST = atoi(str);


	GetPrivateProfileString("TestItem","PANEL_DIFFER_UNIFORMITY_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.PANEL_DIFFER_UNIFORMITY_TEST = atoi(str);


	GetPrivateProfileString("TestItem","TE_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.TE_TEST = atoi(str);


	GetPrivateProfileString("TestItem","SITO_RAWDATA_UNIFORMITY_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.SITO_RAWDATA_UNIFORMITY_TEST = atoi(str);


	GetPrivateProfileString("TestItem","PATTERN_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.PATTERN_TEST = atoi(str);
}

void OnInit_FT5822_BasicThreshold(char *strIniFile)
{
	char str[512] = {0};



	GetPrivateProfileString("Basic_Threshold", "FW_VER_VALUE", "0", str, strIniFile);
	g_stCfg_FT5822_BasicThreshold.FW_VER_VALUE = atoi(str);


	GetPrivateProfileString("Basic_Threshold","Factory_ID_Number","255", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Factory_ID_Number = atoi(str);


	GetPrivateProfileString("Basic_Threshold","Project_Code"," ", str,strIniFile);

	sprintf(g_stCfg_FT5822_BasicThreshold.Project_Code, "%s", str);


	GetPrivateProfileString("Basic_Threshold","IC_Version","3", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.IC_Version = atoi(str);


	GetPrivateProfileString("Basic_Threshold","LCM_ID","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.LCM_ID = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawDataTest_Low_Min","3000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_low_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawDataTest_Low_Max","15000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_Low_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawDataTest_High_Min","3000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_high_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawDataTest_High_Max","15000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_high_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawDataTest_LowFreq","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_SetLowFreq  = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawDataTest_HighFreq","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_SetHighFreq = atoi(str);

	GetPrivateProfileString("Basic_Threshold","Adc_Detect_Max","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.AdcDetect_Max = atoi(str);











	GetPrivateProfileString("Basic_Threshold","SCapCbTest_OFF_Min","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_OFF_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","SCapCbTest_OFF_Max","240", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_OFF_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapCbTest_ON_Min","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_ON_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","SCapCbTest_ON_Max","240", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_ON_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ScapCBTest_SetWaterproof_OFF","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_SetWaterproof_OFF = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ScapCBTest_SetWaterproof_ON","240", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_SetWaterproof_ON = atoi(str);








	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_OFF_Min","5000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_OFF_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_OFF_Max","8500", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_OFF_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_ON_Min","5000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_ON_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_ON_Max","8500", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_ON_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_SetWaterproof_OFF","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_SetWaterproof_OFF = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_SetWaterproof_ON","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_SetWaterproof_ON = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_Mapping","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.bChannelTestMapping = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_NoMapping","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.bChannelTestNoMapping = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_TxNum","13", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ChannelNumTest_TxNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_RxNum","24", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ChannelNumTest_RxNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_Tx_NP_Num","13", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ChannelNumTest_TxNpNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_Rx_NP_Num","24", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ChannelNumTest_RxNpNum = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ResetPinTest_RegAddr","136", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ResetPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold","IntPinTest_RegAddr","79", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.IntPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_Max","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_Max = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_Frames","32", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_Frames = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_Time","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_Time = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_SampeMode","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_SampeMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_NoiseMode","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_NoiseMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_ShowTip","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_ShowTip = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_GloveMode","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.bNoiseTest_GloveMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_RawdataMin","5000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_RawdataMin = atoi(str);

	GetPrivateProfileString("Basic_Threshold","GloveNoiseTest_Coefficient","100", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.GloveNoiseTest_Coefficient = atoi(str);

























	GetPrivateProfileString("Basic_Threshold","WeakShortTest_CG","2000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.WeakShortTest_CG = atoi(str);
	GetPrivateProfileString("Basic_Threshold","WeakShortTest_CC","2000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.WeakShortTest_CC = atoi(str);


	GetPrivateProfileString("Basic_Threshold","UniformityTest_Check_Tx","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_CheckTx = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Check_Rx","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_CheckRx = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Check_MinMax","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_CheckMinMax = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Tx_Hole","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_Tx_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Rx_Hole","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_Rx_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_MinMax_Hole","70", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_MinMax_Hole = atoi(str);

	GetPrivateProfileString("Basic_Threshold","CMTest_Check_Min","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.CMTest_CheckMin = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CMTest_Check_Max","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.CMTest_CheckMax = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CMTest_Min_Hole","0.5", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.CMTest_MinHole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CMTest_Max_Hole","5", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.CMTest_MaxHole = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawdataMarginTest_Min","10", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawdataMarginTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawdataMarginTest_Max","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawdataMarginTest_Max = atoi(str);


	GetPrivateProfileString("Basic_Threshold","PanelDifferTest_Min","150", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDifferTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDifferTest_Max","1000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDifferTest_Max = atoi(str);


	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Check_Tx","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Check_Tx = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Check_Rx","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Check_Rx = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Check_MinMax","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Check_MinMax = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Tx_Hole","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Tx_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Rx_Hole","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Rx_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_MinMax_Hole","70", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_MinMax_Hole = atoi(str);
}

void SetTestItem_FT5822()
{

	g_TestItemNum = 0;



	if (g_stCfg_FT5822_TestItem.RESET_PIN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_RESET_PIN_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.FACTORY_ID_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_FACTORY_ID_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.PROJECT_CODE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_PROJECT_CODE_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.FW_VERSION_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_FW_VERSION_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.LCM_ID_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_LCM_ID_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.IC_VERSION_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_IC_VERSION_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_ENTER_FACTORY_MODE;

	g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
	g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
	g_TestItemNum++;
/*
#if ENABLE_DOUBLE_CHECK
	//////////////////////////////////////////////////judge Normalize type
	//if (g_ScreenSetParam.isNormalize != None)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_JUDEG_NORMALIZE_TYPE;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_JUDEG_NORMALIZE_TYPE];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;
	}
#endif
*/

	if (g_stCfg_FT5822_TestItem.TE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_TE_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.CHANNEL_NUM_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_CHANNEL_NUM_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.NOISE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_NOISE_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.RAWDATA_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_RAWDATA_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_UNIFORMITY_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.SITO_RAWDATA_UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_SITO_RAWDATA_UNIFORMITY_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.CM_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_CM_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.ADC_DETECT_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_ADCDETECT_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.SCAP_CB_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_SCAP_CB_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.SCAP_RAWDATA_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_SCAP_RAWDATA_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.RAWDATA_MARGIN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_RAWDATA_MARGIN_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.WEAK_SHORT_CIRCUIT_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_WEAK_SHORT_CIRCUIT_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.PANEL_DIFFER_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_PANELDIFFER_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}

	if (g_stCfg_FT5822_TestItem.PANEL_DIFFER_UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_PANELDIFFER_UNIFORMITY_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}


	if (g_stCfg_FT5822_TestItem.INT_PIN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_INT_PIN_TEST;

		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult = RESULT_NULL;
		g_TestItemNum++;
	}
}


