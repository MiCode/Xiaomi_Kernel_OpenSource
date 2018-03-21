/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_detail_threshold.c
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: Set Detail Threshold for all IC
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "../include/focaltech_test_ini.h"
#include "../include/focaltech_test_detail_threshold.h"
#include "../include/focaltech_test_main.h"
#include "../../focaltech_global/focaltech_ic_table.h"


struct stCfg_MCap_DetailThreshold g_stCfg_MCap_DetailThreshold;
struct stCfg_SCap_DetailThreshold g_stCfg_SCap_DetailThreshold;
struct stCfg_Incell_DetailThreshold g_stCfg_Incell_DetailThreshold;

void set_max_channel_num(void)
{

	FTS_TEST_FUNC_ENTER();
	switch (g_ScreenSetParam.iSelectedIC>>4) {
	case IC_FT5822>>4:
	case IC_FT8006>>4:
	case IC_FTE716>>4:
		g_ScreenSetParam.iUsedMaxTxNum = TX_NUM_MAX;
		g_ScreenSetParam.iUsedMaxRxNum = RX_NUM_MAX;
		break;
	default:
		g_ScreenSetParam.iUsedMaxTxNum = 30;
		g_ScreenSetParam.iUsedMaxRxNum = 30;
		break;
	}

	FTS_TEST_DBG("MaxTxNum = %d, MaxRxNum = %d. ",  g_ScreenSetParam.iUsedMaxTxNum, g_ScreenSetParam.iUsedMaxRxNum);

	FTS_TEST_FUNC_EXIT();

}

int malloc_struct_DetailThreshold(void)
{
	FTS_TEST_FUNC_ENTER();


	g_stCfg_MCap_DetailThreshold.InvalidNode = (unsigned char (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(unsigned char));
	if (NULL == g_stCfg_MCap_DetailThreshold.InvalidNode)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.InvalidNode_SC = (unsigned char (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(unsigned char));
	if (NULL == g_stCfg_MCap_DetailThreshold.InvalidNode_SC)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.RawDataTest_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.RawDataTest_Min)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.RawDataTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.RawDataTest_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SITORawdata_RxLinearityTest_Base = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.SITORawdata_RxLinearityTest_Base)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SITORawdata_TxLinearityTest_Base = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.SITORawdata_TxLinearityTest_Base)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max = (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(short));
	if (NULL == g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min = (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(short));
	if (NULL == g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max = (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(short));
	if (NULL == g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min = (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(short));
	if (NULL == g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient)
		goto ERR;

	g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max)
		goto ERR;


	g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min)
		goto ERR;



	g_stCfg_Incell_DetailThreshold.InvalidNode =  (unsigned char (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(unsigned char));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.InvalidNode) {
		FTS_TEST_DBG("InvalidNode. \n");
		goto ERR;
	}


	g_stCfg_Incell_DetailThreshold.RawDataTest_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.RawDataTest_Min) {
		FTS_TEST_DBG("RawDataTest_Min. \n");
		goto ERR;
	}
	g_stCfg_Incell_DetailThreshold.RawDataTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.RawDataTest_Max) {
		FTS_TEST_DBG("RawDataTest_Max. \n");
		goto ERR;
	}
	g_stCfg_Incell_DetailThreshold.CBTest_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.CBTest_Min) {
		FTS_TEST_DBG("CBTest_Min. \n");
		goto ERR;
	}
	g_stCfg_Incell_DetailThreshold.CBTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.CBTest_Max) {
		FTS_TEST_DBG("CBTest_Max. \n");
		goto ERR;
	}

	g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity) {
		FTS_TEST_DBG("CBUniformityTest_CHY_Linearity. \n");
		goto ERR;
	}
	g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity) {
		FTS_TEST_DBG("CBUniformityTest_CHX_Linearity. \n");
		goto ERR;
	}

	g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHX_Linearity = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHX_Linearity) {
		FTS_TEST_DBG("RawDataUniformityTest_CHX_Linearity. \n");
		goto ERR;
	}

	g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHY_Linearity = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHY_Linearity) {
		FTS_TEST_DBG("RawDataUniformityTest_CHY_Linearity. \n");
		goto ERR;
	}

	g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CB_Data = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CB_Data) {
		FTS_TEST_DBG("RawDataUniformityTest_CB_Data. \n");
		goto ERR;
	}

	FTS_TEST_FUNC_EXIT();

	return 0;

ERR:
	FTS_TEST_ERROR("fts_malloc memory failed in function.");
	return -EPERM;
}

void free_struct_DetailThreshold(void)
{
	if (NULL != g_stCfg_MCap_DetailThreshold.InvalidNode) {
		fts_free(g_stCfg_MCap_DetailThreshold.InvalidNode);
		g_stCfg_MCap_DetailThreshold.InvalidNode = NULL;
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.InvalidNode_SC) {
		fts_free(g_stCfg_MCap_DetailThreshold.InvalidNode_SC);
		g_stCfg_MCap_DetailThreshold.InvalidNode_SC = NULL;
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.RawDataTest_Min) {
		fts_free(g_stCfg_MCap_DetailThreshold.RawDataTest_Min);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.RawDataTest_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.RawDataTest_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min) {
		fts_free(g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min) {
		fts_free(g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SITORawdata_RxLinearityTest_Base) {
		fts_free(g_stCfg_MCap_DetailThreshold.SITORawdata_RxLinearityTest_Base);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SITORawdata_TxLinearityTest_Base) {
		fts_free(g_stCfg_MCap_DetailThreshold.SITORawdata_TxLinearityTest_Base);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min) {
		fts_free(g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min) {
		fts_free(g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min) {
		fts_free(g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min) {
		fts_free(g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min);
	}
	if (NULL != g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient) {
		fts_free(g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient);
		g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient = NULL;
	}

	if (NULL != g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max) {
		fts_free(g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max);
		g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max = NULL;
	}

	if (NULL != g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min) {
		fts_free(g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min);
		g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min = NULL;
	}

	if (NULL !=  g_stCfg_Incell_DetailThreshold.InvalidNode) {
		fts_free(g_stCfg_Incell_DetailThreshold.InvalidNode);
		g_stCfg_Incell_DetailThreshold.InvalidNode = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.RawDataTest_Min) {
		fts_free(g_stCfg_Incell_DetailThreshold.RawDataTest_Min);
		g_stCfg_Incell_DetailThreshold.RawDataTest_Min = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.RawDataTest_Max) {
		fts_free(g_stCfg_Incell_DetailThreshold.RawDataTest_Max);
		g_stCfg_Incell_DetailThreshold.RawDataTest_Max = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.RawDataTest_Max) {
		fts_free(g_stCfg_Incell_DetailThreshold.RawDataTest_Max);
		g_stCfg_Incell_DetailThreshold.RawDataTest_Max = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.CBTest_Min) {
		fts_free(g_stCfg_Incell_DetailThreshold.CBTest_Min);
		g_stCfg_Incell_DetailThreshold.CBTest_Min = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.CBTest_Max) {
		fts_free(g_stCfg_Incell_DetailThreshold.CBTest_Max);
		g_stCfg_Incell_DetailThreshold.CBTest_Max = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity) {
		fts_free(g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity);
		g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity) {
		fts_free(g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity);
		g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHX_Linearity) {
		fts_free(g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHX_Linearity);
		g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHX_Linearity = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHY_Linearity) {
		fts_free(g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHY_Linearity);
		g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHY_Linearity = NULL;
	}

	if (NULL !=   g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CB_Data) {
		fts_free(g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CB_Data);
		g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CB_Data = NULL;
	}

}

void OnInit_SCap_DetailThreshold(char *strIniFile)
{
	FTS_TEST_FUNC_ENTER();

	OnGetTestItemParam("RawDataTest_Max", strIniFile, 12500);
	memcpy(g_stCfg_SCap_DetailThreshold.RawDataTest_Max, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("RawDataTest_Min", strIniFile, 16500);
	memcpy(g_stCfg_SCap_DetailThreshold.RawDataTest_Min, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("CiTest_Max", strIniFile, 5);
	memcpy(g_stCfg_SCap_DetailThreshold.CiTest_Max, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("CiTest_Min", strIniFile, 250);
	memcpy(g_stCfg_SCap_DetailThreshold.CiTest_Min, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("DeltaCiTest_Base", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.DeltaCiTest_Base, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("DeltaCiTest_AnotherBase1", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.DeltaCiTest_AnotherBase1, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("DeltaCiTest_AnotherBase2", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.DeltaCiTest_AnotherBase2, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("NoiseTest_Max", strIniFile, 20);
	memcpy(g_stCfg_SCap_DetailThreshold.NoiseTest_Max, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("CiDeviation_Base", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.CiDeviationTest_Base, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("DeltaCxTest_Sort", strIniFile, 1);
	memcpy(g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	OnGetTestItemParam("DeltaCxTest_Area", strIniFile, 1);
	memcpy(g_stCfg_SCap_DetailThreshold.DeltaCxTest_Area, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));



	OnGetTestItemParam("CbTest_Max", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.CbTest_Max, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	OnGetTestItemParam("CbTest_Min", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.CbTest_Min, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	OnGetTestItemParam("DeltaCbTest_Base", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.DeltaCbTest_Base, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	OnGetTestItemParam("DifferTest_Base", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.DifferTest_Base, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	OnGetTestItemParam("CBDeviation_Base", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.CBDeviationTest_Base, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	OnGetTestItemParam("K1DifferTest_Base", strIniFile, 0);
	memcpy(g_stCfg_SCap_DetailThreshold.K1DifferTest_Base, g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	FTS_TEST_FUNC_EXIT();
}

void OnGetTestItemParam(char *strItemName, char *strIniFile, int iDefautValue)
{

	char strValue[800];
	char str_tmp[128];
	int iValue = 0;
	int dividerPos = 0;
	int index = 0;
	int i = 0, j = 0, k = 0;
	memset(g_stCfg_SCap_DetailThreshold.TempData, 0, sizeof(g_stCfg_SCap_DetailThreshold.TempData));
	sprintf(str_tmp, "%d", iDefautValue);
	GetPrivateProfileString("Basic_Threshold", strItemName, str_tmp, strValue, strIniFile);
	iValue = fts_atoi(strValue);
	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		g_stCfg_SCap_DetailThreshold.TempData[i] = iValue;
	}

	dividerPos = GetPrivateProfileString("SpecialSet", strItemName, "", strValue, strIniFile);
	if (dividerPos > 0) {
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_SCap_DetailThreshold.TempData[k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
}

void OnInit_MCap_DetailThreshold(char *strIniFile)
{

	FTS_TEST_FUNC_ENTER();

	set_max_channel_num();

	OnInit_InvalidNode(strIniFile);
	OnInit_DThreshold_RawDataTest(strIniFile);
	OnInit_DThreshold_SCapRawDataTest(strIniFile);
	OnInit_DThreshold_SCapCbTest(strIniFile);

	OnInit_DThreshold_ForceTouch_SCapRawDataTest(strIniFile);
	OnInit_DThreshold_ForceTouch_SCapCbTest(strIniFile);

	OnInit_DThreshold_SITORawdata_TxLinearityTest(strIniFile);
	OnInit_DThreshold_SITORawdata_RxLinearityTest(strIniFile);

	OnInit_DThreshold_RxLinearityTest(strIniFile);
	OnInit_DThreshold_TxLinearityTest(strIniFile);

	OnInit_DThreshold_PanelDifferTest(strIniFile);
	OnInit_DThreshold_NoiseTest(strIniFile);

	FTS_TEST_FUNC_EXIT();
}
void OnInit_InvalidNode(char *strIniFile)
{

	char str[MAX_PATH] = {0}, strTemp[MAX_PATH] = {0};
	int i = 0, j = 0;



	FTS_TEST_FUNC_ENTER();

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			sprintf(strTemp, "InvalidNode[%d][%d]", (i+1), (j+1));

			GetPrivateProfileString("INVALID_NODE", strTemp, "1", str, strIniFile);
			if (fts_atoi(str) == 0) {
				g_stCfg_MCap_DetailThreshold.InvalidNode[i][j] = 0;
				g_stCfg_Incell_DetailThreshold.InvalidNode[i][j] = 0;
				FTS_TEST_DBG("node (%d, %d) \n", (i+1),  (j+1));

			} else if (fts_atoi(str) == 2) {
				g_stCfg_MCap_DetailThreshold.InvalidNode[i][j] = 2;
				g_stCfg_Incell_DetailThreshold.InvalidNode[i][j] = 2;
			} else {
				g_stCfg_MCap_DetailThreshold.InvalidNode[i][j] = 1;
				g_stCfg_Incell_DetailThreshold.InvalidNode[i][j] = 1;
			}




		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			sprintf(strTemp, "InvalidNodeS[%d][%d]", (i+1), (j+1));
			GetPrivateProfileString("INVALID_NODES", strTemp, "1", str, strIniFile);
			if (fts_atoi(str) == 0) {
				g_stCfg_MCap_DetailThreshold.InvalidNode_SC[i][j] = 0;
			} else if (fts_atoi(str) == 2) {
				g_stCfg_MCap_DetailThreshold.InvalidNode_SC[i][j] = 2;
			} else
				g_stCfg_MCap_DetailThreshold.InvalidNode_SC[i][j] = 1;
		}

	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_RawDataTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	if ((g_ScreenSetParam.iSelectedIC >> 4 == IC_FT8716 >> 4) || (g_ScreenSetParam.iSelectedIC >> 4 == IC_FT8736 >> 4) || (g_ScreenSetParam.iSelectedIC >> 4 == IC_FTE716 >> 4)) {
		return;
	}


	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "10000", str, strIniFile);
	MaxValue = fts_atoi(str);



	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.RawDataTest_Max[i][j] = MaxValue;
			g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][j] = MaxValue;

		}
	}

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Max_Tx%d", (i + 1));

		dividerPos = GetPrivateProfileString("SpecialSet", str, "111", strTemp, strIniFile);

		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.RawDataTest_Max[i][k] = (short)(fts_atoi(str_tmp));
				g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}

	}

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "7000", str, strIniFile);
	MinValue = fts_atoi(str);

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.RawDataTest_Min[i][j] = MinValue;
			g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Min_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.RawDataTest_Min[i][k] = (short)(fts_atoi(str_tmp));
				g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Max", "15000", str, strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Max_Low_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Min", "3000", str, strIniFile);
	MinValue = fts_atoi(str);

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Min_Low_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Max", "15000", str, strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max[i][j] = MaxValue;
		}
	}
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Min", "3000", str, strIniFile);
	MinValue = fts_atoi(str);

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Max_High_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Min_High_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_SCapRawDataTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Min", "150", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Max", "1000", str, strIniFile);
	MaxValue = fts_atoi(str);


	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapRawData_OFF_Max_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapRawData_OFF_Min_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Min", "150", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Max", "1000", str, strIniFile);
	MaxValue = fts_atoi(str);


	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapRawData_ON_Max_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapRawData_ON_Min_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_SCapCbTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Min", "0", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Max", "240", str, strIniFile);
	MaxValue = fts_atoi(str);


	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapCB_ON_Max_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapCB_ON_Min_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Min", "0", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Max", "240", str, strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapCB_OFF_Max_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapCB_OFF_Min_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_PanelDifferTest(char *strIniFile)
{

	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int dividerPos = 0;
	int index = 0;
	int  k = 0, i = 0, j = 0;
	char str_tmp[128];

	FTS_TEST_FUNC_ENTER();

	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Max", "1000", str, strIniFile);
	MaxValue = fts_atoi(str);
	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max[i][j] = MaxValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "Panel_Differ_Max_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Min", "150", str, strIniFile);
	MinValue = fts_atoi(str);
	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min[i][j] = MinValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "Panel_Differ_Min_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();

}


void OnInit_DThreshold_NoiseTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("Basic_Threshold", "NoiseTest_GloveMode", "0", str, strIniFile);
	if (1 == fts_atoi(str))
		GetPrivateProfileString("Basic_Threshold", "GloveNoiseTest_Coefficient", "100", str, strIniFile);
	else
		GetPrivateProfileString("Basic_Threshold", "NoiseTest_Max", "50", str, strIniFile);

	MaxValue = fts_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient[i][j] = MaxValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "NoistTestCoefficient_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}

	}



}


void OnInit_DThreshold_RxLinearityTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("Basic_Threshold", "RxLinearityTest_Max", "50", str, strIniFile);
	MaxValue = fts_atoi(str);



	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max[i][j] = MaxValue;
		}
	}

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "Rx_Linearity_Max_Tx%d", (i + 1));

		dividerPos = GetPrivateProfileString("SpecialSet", str, "111", strTemp, strIniFile);

		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}

	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_TxLinearityTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("Basic_Threshold", "TxLinearityTest_Max", "50", str, strIniFile);
	MaxValue = fts_atoi(str);



	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max[i][j] = MaxValue;
		}
	}

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "Tx_Linearity_Max_Tx%d", (i + 1));

		dividerPos = GetPrivateProfileString("SpecialSet", str, "111", strTemp, strIniFile);

		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}

	}
	FTS_TEST_FUNC_EXIT();
}


void OnInit_DThreshold_SITORawdata_RxLinearityTest(char *strIniFile)
{

	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int i = 0, j = 0;
	char str_tmp[128];
	int  dividerPos = 0;
	int index = 0;
	int  k = 0;

	if (IC_FT5X46>>4 != g_ScreenSetParam.iSelectedIC>>4 && IC_FT3C47U>>4 != g_ScreenSetParam.iSelectedIC>>4) {
		return;
	}


	if (IC_FT5X46>>4 == g_ScreenSetParam.iSelectedIC>>4 || IC_FT3C47U>>4 == g_ScreenSetParam.iSelectedIC>>4) {
		GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Rx_Hole", "10", str, strIniFile);
		MaxValue = 0/*atoi(str)*/;
	}

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SITORawdata_RxLinearityTest_Base[i][j] = MaxValue;
		}
	}

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "SITORawdata_Rx_Linearity_Base_Rx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SITORawdata_RxLinearityTest_Base[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

}



void OnInit_DThreshold_SITORawdata_TxLinearityTest(char *strIniFile)
{

	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int i = 0, j = 0;
	char str_tmp[128];
	int  dividerPos = 0;
	int index = 0;
	int  k = 0;

	if (IC_FT5X46>>4 != g_ScreenSetParam.iSelectedIC>>4 && IC_FT3C47U>>4 != g_ScreenSetParam.iSelectedIC>>4) {
		return;
	}


	if (IC_FT5X46>>4 == g_ScreenSetParam.iSelectedIC>>4 || IC_FT3C47U>>4 == g_ScreenSetParam.iSelectedIC>>4) {
		GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Tx_Hole", "10", str, strIniFile);
		MaxValue = 0/*atoi(str)*/;
	}

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.SITORawdata_TxLinearityTest_Base[i][j] = MaxValue;
		}
	}

	for (i = 0; i < g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "SITORawdata_Rx_Linearity_Base_Tx%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.SITORawdata_TxLinearityTest_Base[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

}



void OnInit_DThreshold_ForceTouch_SCapRawDataTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_OFF_Min", "150", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_OFF_Max", "1000", str, strIniFile);
	MaxValue = fts_atoi(str);


	for (i = 0; i < 1; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapRawData_OFF_Max_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_OFF_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < 1; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapRawData_OFF_Min_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_OFF_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_ON_Min", "150", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_ON_Max", "1000", str, strIniFile);
	MaxValue = fts_atoi(str);


	for (i = 0; i < 1; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapRawData_ON_Max_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_ON_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < 1; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapRawData_ON_Min_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_ON_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_ForceTouch_SCapCbTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_ON_Min", "0", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_ON_Max", "240", str, strIniFile);
	MaxValue = fts_atoi(str);


	for (i = 0; i < 1; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapCB_ON_Max_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_ON_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < 1; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapCB_ON_Min_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		FTS_TEST_DBG("%s\r", strTemp);
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_ON_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_OFF_Min", "0", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_OFF_Max", "240", str, strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < 1; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapCB_OFF_Max_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_OFF_Max[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < 1; i++) {
		for (j = 0; j < g_ScreenSetParam.iUsedMaxRxNum; j++) {
			g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapCB_OFF_Min_%d", (i + 1));
		dividerPos = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_OFF_Min[i][k] = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_Incell_DetailThreshold(char *strIniFile)
{

	FTS_TEST_FUNC_ENTER();

	set_max_channel_num();

	OnInit_InvalidNode(strIniFile);

	OnInit_DThreshold_RawDataTest(strIniFile);
	OnInit_DThreshold_CBTest(strIniFile);
	OnInit_DThreshold_AllButtonCBTest(strIniFile);
	OnThreshold_VkAndVaRawDataSeparateTest(strIniFile);

	OnInit_DThreshold_RawDataUniformityTest(strIniFile);

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_CBTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue, MaxValue_Vkey, MinValue_Vkey;
	int ChannelNumTest_ChannelXNum, ChannelNumTest_ChannelYNum;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();
	if (g_ScreenSetParam.iSelectedIC >> 4 == IC_FT8606 >> 4) {
		return;
	}

	GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str, strIniFile);
	MaxValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "CBTest_Max_Vkey", "100", str, strIniFile);
	MaxValue_Vkey = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX", "15", str, strIniFile);
	ChannelNumTest_ChannelXNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY", "24", str, strIniFile);
	ChannelNumTest_ChannelYNum = fts_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.CBTest_Max[i][j] = MaxValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < RX_NUM_MAX; j++) {
				g_stCfg_Incell_DetailThreshold.CBTest_Max[i][j] = MaxValue_Vkey;
			}
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "CB_Max_Tx%d", (i + 1));

		dividerPos  = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.CBTest_Max[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}



	GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, strIniFile);
	MinValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "CBTest_Min_Vkey", "3", str, strIniFile);
	MinValue_Vkey = fts_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.CBTest_Min[i][j] = MinValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < RX_NUM_MAX; j++) {
				g_stCfg_Incell_DetailThreshold.CBTest_Min[i][j] = MinValue_Vkey;
			}
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "CB_Min_Tx%d", (i + 1));
		dividerPos  =  GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.CBTest_Min[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}



void OnInit_DThreshold_AllButtonCBTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	if (g_ScreenSetParam.iSelectedIC >> 4 != IC_FT8606 >> 4) {
		return;
	}

	GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str, strIniFile);
	MaxValue = fts_atoi(str);


	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.CBTest_Max[i][j] = MaxValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "CB_Max_Tx%d", (i + 1));

		dividerPos  = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.CBTest_Max[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}



	GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, strIniFile);
	MinValue = fts_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.CBTest_Min[i][j] = MinValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "CB_Min_Tx%d", (i + 1));
		dividerPos  =  GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.CBTest_Min[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnThreshold_VkAndVaRawDataSeparateTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue, MaxValue_Vkey, MinValue_Vkey;
	int ChannelNumTest_ChannelXNum, ChannelNumTest_ChannelYNum;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;


	FTS_TEST_FUNC_ENTER();

	if ((g_ScreenSetParam.iSelectedIC >> 4 != IC_FT8716 >> 4) && (g_ScreenSetParam.iSelectedIC >> 4 != IC_FT8736 >> 4) && (g_ScreenSetParam.iSelectedIC >> 4 != IC_FTE716 >> 4)) {
		return;
	}

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "1100", str, strIniFile);
	MaxValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max_VKey", "1100", str, strIniFile);
	MaxValue_Vkey = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX", "15", str, strIniFile);
	ChannelNumTest_ChannelXNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY", "24", str, strIniFile);
	ChannelNumTest_ChannelYNum = fts_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][j] = MaxValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < RX_NUM_MAX; j++) {
				g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][j] = MaxValue_Vkey;
			}
		}
	}


	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "RawData_Max_Tx%d", (i + 1));
		dividerPos  = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "5000", str, strIniFile);
	MinValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min_VKey", "5000", str, strIniFile);
	MinValue_Vkey = fts_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][j] = MinValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < RX_NUM_MAX; j++) {
				g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][j] = MinValue_Vkey;
			}
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "RawData_Min_Tx%d", (i + 1));
		dividerPos  = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_RawDataUniformityTest(char *strIniFile)
{

	int iBasicCHX = 0, iBasicCHY = 0;
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	GetPrivateProfileString("Basic_Threshold", "RawDataUniformityTest_CHX_Hole", "15", str, strIniFile);
	iBasicCHX = fts_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHX_Linearity[i][j] = iBasicCHX;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "RawDataUniformityTest_CHX%d", (i + 1));
		dividerPos  = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHX_Linearity[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	GetPrivateProfileString("Basic_Threshold", "RawDataUniformityTest_CHY_Hole", "15", str, strIniFile);
	iBasicCHY = fts_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHY_Linearity[i][j] = iBasicCHY;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "RawDataUniformityTest_CHY%d", (i + 1));
		dividerPos  = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CHY_Linearity[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}


	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CB_Data[i][j] = 20;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "RawDataUniformityTest_CB%d", (i + 1));
		dividerPos  = GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				g_stCfg_Incell_DetailThreshold.RawDataUniformityTest_CB_Data[i][k] = (short)(fts_atoi(str_tmp));

				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
}


