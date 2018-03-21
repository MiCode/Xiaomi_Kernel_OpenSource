/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
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
#include "../include/focaltech_ic_table.h"


struct stCfg_MCap_DetailThreshold ft8006m_g_stCfg_MCap_DetailThreshold;
struct stCfg_SCap_DetailThreshold ft8006m_g_stCfg_SCap_DetailThreshold;
struct stCfg_Incell_DetailThreshold ft8006m_g_stCfg_Incell_DetailThreshold;

void ft8006m_set_max_channel_num(void)
{

	FTS_TEST_FUNC_ENTER();
	switch (ft8006m_g_ScreenSetParam.iSelectedIC>>4) {
	case IC_FT5822>>4:
	case IC_FT8006M>>4:
	case IC_FTE716>>4:
	case IC_FT3D47>>4:
		ft8006m_g_ScreenSetParam.iUsedMaxTxNum = TX_NUM_MAX;
		ft8006m_g_ScreenSetParam.iUsedMaxRxNum = RX_NUM_MAX;
		break;
	default:
		ft8006m_g_ScreenSetParam.iUsedMaxTxNum = 30;
		ft8006m_g_ScreenSetParam.iUsedMaxRxNum = 30;
		break;
	}

	FTS_TEST_DBG("MaxTxNum = %d, MaxRxNum = %d. ",  ft8006m_g_ScreenSetParam.iUsedMaxTxNum, ft8006m_g_ScreenSetParam.iUsedMaxRxNum);

	FTS_TEST_FUNC_EXIT();

}

int ft8006m_malloc_struct_DetailThreshold(void)
{
	FTS_TEST_FUNC_ENTER();


	ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode = (unsigned char (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(unsigned char));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode_SC = (unsigned char (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(unsigned char));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode_SC)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Min = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Min)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max = (short (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(short));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min = (short (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(short));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max = (short (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(short));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min = (short (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(short));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient)
		goto ERR;

	ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max)
		goto ERR;
	ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL == ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min)
		goto ERR;



	ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode =  (unsigned char (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(unsigned char));
	if (NULL ==  ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode) {
		FTS_TEST_DBG("InvalidNode. \n");
		goto ERR;
	}


	ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min) {
		FTS_TEST_DBG("RawDataTest_Min. \n");
		goto ERR;
	}
	ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max) {
		FTS_TEST_DBG("RawDataTest_Max. \n");
		goto ERR;
	}
	ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min) {
		FTS_TEST_DBG("CBTest_Min. \n");
		goto ERR;
	}
	ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max) {
		FTS_TEST_DBG("CBTest_Max. \n");
		goto ERR;
	}

	ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity) {
		FTS_TEST_DBG("CBUniformityTest_CHY_Linearity. \n");
		goto ERR;
	}
	ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity = (int (*)[RX_NUM_MAX])Ft8006m_fts_malloc(NUM_MAX*sizeof(int));
	if (NULL ==  ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity) {
		FTS_TEST_DBG("CBUniformityTest_CHX_Linearity. \n");
		goto ERR;
	}

	FTS_TEST_FUNC_EXIT();

	return 0;

ERR:
	FTS_TEST_ERROR("Ft8006m_fts_malloc memory failed in function.");
	return -EPERM;
}

void ft8006m_free_struct_DetailThreshold(void)
{
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode);
		ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode = NULL;
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode_SC) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode_SC);
		ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode_SC = NULL;
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Min);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min);
	}
	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient);
		ft8006m_g_stCfg_MCap_DetailThreshold.NoistTest_Coefficient = NULL;
	}

	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max);
		ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max = NULL;
	}

	if (NULL != ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min);
		ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min = NULL;
	}

	if (NULL !=  ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode) {
		Ft8006m_fts_free(ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode);
		ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode = NULL;
	}

	if (NULL !=   ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min);
		ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min = NULL;
	}

	if (NULL !=   ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max);
		ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max = NULL;
	}

	if (NULL !=   ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max);
		ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max = NULL;
	}

	if (NULL !=   ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min) {
		Ft8006m_fts_free(ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min);
		ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min = NULL;
	}

	if (NULL !=   ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max) {
		Ft8006m_fts_free(ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max);
		ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max = NULL;
	}

	if (NULL !=   ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity) {
		Ft8006m_fts_free(ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity);
		ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHX_Linearity = NULL;
	}

	if (NULL !=   ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity) {
		Ft8006m_fts_free(ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity);
		ft8006m_g_stCfg_Incell_DetailThreshold.CBUniformityTest_CHY_Linearity = NULL;
	}



}

void Ft8006m_OnInit_SCap_DetailThreshold(char *strIniFile)
{

	FTS_TEST_FUNC_ENTER();

	Ft8006m_OnGetTestItemParam("RawDataTest_Max", strIniFile, 12500);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.RawDataTest_Max, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("RawDataTest_Min", strIniFile, 16500);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.RawDataTest_Min, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("CiTest_Max", strIniFile, 5);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.CiTest_Max, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("CiTest_Min", strIniFile, 250);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.CiTest_Min, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("DeltaCiTest_Base", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.DeltaCiTest_Base, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("DeltaCiTest_AnotherBase1", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.DeltaCiTest_AnotherBase1, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("DeltaCiTest_AnotherBase2", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.DeltaCiTest_AnotherBase2, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("NoiseTest_Max", strIniFile, 20);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.NoiseTest_Max, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("CiDeviation_Base", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.CiDeviationTest_Base, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("DeltaCxTest_Sort", strIniFile, 1);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	Ft8006m_OnGetTestItemParam("DeltaCxTest_Area", strIniFile, 1);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.DeltaCxTest_Area, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));



	Ft8006m_OnGetTestItemParam("CbTest_Max", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.CbTest_Max, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	Ft8006m_OnGetTestItemParam("CbTest_Min", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.CbTest_Min, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	Ft8006m_OnGetTestItemParam("DeltaCbTest_Base", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.DeltaCbTest_Base, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	Ft8006m_OnGetTestItemParam("DifferTest_Base", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.DifferTest_Base, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	Ft8006m_OnGetTestItemParam("CBDeviation_Base", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.CBDeviationTest_Base, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));


	Ft8006m_OnGetTestItemParam("K1DifferTest_Base", strIniFile, 0);
	memcpy(ft8006m_g_stCfg_SCap_DetailThreshold.K1DifferTest_Base, ft8006m_g_stCfg_SCap_DetailThreshold.TempData, MAX_CHANNEL_NUM*sizeof(int));

	FTS_TEST_FUNC_EXIT();
}

void Ft8006m_OnGetTestItemParam(char *strItemName, char *strIniFile, int iDefautValue)
{

	char strValue[800];
	char str_tmp[128];
	int iValue = 0;
	int dividerPos = 0;
	int index = 0;
	int i = 0, j = 0, k = 0;
	memset(ft8006m_g_stCfg_SCap_DetailThreshold.TempData, 0, sizeof(ft8006m_g_stCfg_SCap_DetailThreshold.TempData));
	sprintf(str_tmp, "%d", iDefautValue);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", strItemName, str_tmp, strValue, strIniFile);
	iValue = ft8006m_atoi(strValue);
	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		ft8006m_g_stCfg_SCap_DetailThreshold.TempData[i] = iValue;
	}

	dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", strItemName, "", strValue, strIniFile);
	if (dividerPos > 0) {
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_SCap_DetailThreshold.TempData[k] = (short)(ft8006m_atoi(str_tmp));
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

void Ft8006m_OnInit_MCap_DetailThreshold(char *strIniFile)
{

	FTS_TEST_FUNC_ENTER();

	ft8006m_set_max_channel_num();

	Ft8006m_OnInit_InvalidNode(strIniFile);
	Ft8006m_OnInit_DThreshold_RawDataTest(strIniFile);
	Ft8006m_OnInit_DThreshold_SCapRawDataTest(strIniFile);
	Ft8006m_OnInit_DThreshold_SCapCbTest(strIniFile);

	Ft8006m_OnInit_DThreshold_ForceTouch_SCapRawDataTest(strIniFile);
	Ft8006m_OnInit_DThreshold_ForceTouch_SCapCbTest(strIniFile);

	Ft8006m_OnInit_DThreshold_RxLinearityTest(strIniFile);
	Ft8006m_OnInit_DThreshold_TxLinearityTest(strIniFile);

	Ft8006m_OnInit_DThreshold_PanelDifferTest(strIniFile);

	FTS_TEST_FUNC_EXIT();
}
void Ft8006m_OnInit_InvalidNode(char *strIniFile)
{

	char str[MAX_PATH] = {0}, strTemp[MAX_PATH] = {0};
	int i = 0, j = 0;



	FTS_TEST_FUNC_ENTER();

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			sprintf(strTemp, "InvalidNode[%d][%d]", (i+1), (j+1));

			Ft8006m_GetPrivateProfileString("INVALID_NODE", strTemp, "1", str, strIniFile);
			if (ft8006m_atoi(str) == 0) {
				ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode[i][j] = 0;
				ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode[i][j] = 0;
				FTS_TEST_DBG("node (%d, %d) \n", (i+1),  (j+1));

			} else if (ft8006m_atoi(str) == 2) {
				ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode[i][j] = 2;
				ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode[i][j] = 2;
			} else {
				ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode[i][j] = 1;
				ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode[i][j] = 1;
			}




		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			sprintf(strTemp, "InvalidNodeS[%d][%d]", (i+1), (j+1));
			Ft8006m_GetPrivateProfileString("INVALID_NODES", strTemp, "1", str, strIniFile);
			if (ft8006m_atoi(str) == 0) {
				ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode_SC[i][j] = 0;
			} else if (ft8006m_atoi(str) == 2) {
				ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode_SC[i][j] = 2;
			} else
				ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode_SC[i][j] = 1;
		}

	}

	FTS_TEST_FUNC_EXIT();
}

void Ft8006m_OnInit_DThreshold_RawDataTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	if ((ft8006m_g_ScreenSetParam.iSelectedIC >> 4 == IC_FT8716 >> 4) || (ft8006m_g_ScreenSetParam.iSelectedIC >> 4 == IC_FT8736 >> 4) || (ft8006m_g_ScreenSetParam.iSelectedIC >> 4 == IC_FTE716 >> 4)) {
		return;
	}


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "10000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);



	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Max[i][j] = MaxValue;
			ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][j] = MaxValue;

		}
	}

	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Max_Tx%d", (i + 1));

		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "111", strTemp, strIniFile);

		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
				ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "7000", str, strIniFile);
	MinValue = ft8006m_atoi(str);

	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Min[i][j] = MinValue;
			ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Min_Tx%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
				ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Max", "15000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);

	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Max_Low_Tx%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Min", "3000", str, strIniFile);
	MinValue = ft8006m_atoi(str);

	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Min_Low_Tx%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_Low_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Max", "15000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);

	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max[i][j] = MaxValue;
		}
	}
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Min", "3000", str, strIniFile);
	MinValue = ft8006m_atoi(str);

	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Max_High_Tx%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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


	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "RawData_Min_High_Tx%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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

void Ft8006m_OnInit_DThreshold_SCapRawDataTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Min", "150", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Max", "1000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);


	for (i = 0; i < 2; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapRawData_OFF_Max_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapRawData_OFF_Min_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Min", "150", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Max", "1000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);


	for (i = 0; i < 2; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapRawData_ON_Max_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapRawData_ON_Min_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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

void Ft8006m_OnInit_DThreshold_SCapCbTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Min", "0", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Max", "240", str, strIniFile);
	MaxValue = ft8006m_atoi(str);


	for (i = 0; i < 2; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapCB_ON_Max_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapCB_ON_Min_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Min", "0", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Max", "240", str, strIniFile);
	MaxValue = ft8006m_atoi(str);

	for (i = 0; i < 2; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapCB_OFF_Max_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		sprintf(str, "ScapCB_OFF_Min_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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

void Ft8006m_OnInit_DThreshold_PanelDifferTest(char *strIniFile)
{

	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int dividerPos = 0;
	int index = 0;
	int  k = 0, i = 0, j = 0;
	char str_tmp[128];

	FTS_TEST_FUNC_ENTER();

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Max", "1000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);
	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max[i][j] = MaxValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "Panel_Differ_Max_Tx%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Min", "150", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min[i][j] = MinValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "Panel_Differ_Min_Tx%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.PanelDifferTest_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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



void Ft8006m_OnInit_DThreshold_RxLinearityTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RxLinearityTest_Max", "50", str, strIniFile);
	MaxValue = ft8006m_atoi(str);



	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max[i][j] = MaxValue;
		}
	}

	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "Rx_Linearity_Max_Tx%d", (i + 1));

		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "111", strTemp, strIniFile);

		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.RxLinearityTest_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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

void Ft8006m_OnInit_DThreshold_TxLinearityTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "TxLinearityTest_Max", "50", str, strIniFile);
	MaxValue = ft8006m_atoi(str);



	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max[i][j] = MaxValue;
		}
	}

	for (i = 0; i < ft8006m_g_ScreenSetParam.iUsedMaxTxNum; i++) {
		sprintf(str, "Tx_Linearity_Max_Tx%d", (i + 1));

		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "111", strTemp, strIniFile);

		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.TxLinearityTest_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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

void Ft8006m_OnInit_DThreshold_ForceTouch_SCapRawDataTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_OFF_Min", "150", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_OFF_Max", "1000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);


	for (i = 0; i < 1; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapRawData_OFF_Max_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_OFF_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapRawData_OFF_Min_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_OFF_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_ON_Min", "150", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_ON_Max", "1000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);


	for (i = 0; i < 1; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapRawData_ON_Max_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_ON_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapRawData_ON_Min_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapRawDataTest_ON_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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

void Ft8006m_OnInit_DThreshold_ForceTouch_SCapCbTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_ON_Min", "0", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_ON_Max", "240", str, strIniFile);
	MaxValue = ft8006m_atoi(str);


	for (i = 0; i < 1; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapCB_ON_Max_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_ON_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapCB_ON_Min_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		FTS_TEST_DBG("%s\r", strTemp);
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_ON_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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


	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_OFF_Min", "0", str, strIniFile);
	MinValue = ft8006m_atoi(str);
	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_OFF_Max", "240", str, strIniFile);
	MaxValue = ft8006m_atoi(str);

	for (i = 0; i < 1; i++) {
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapCB_OFF_Max_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_OFF_Max[i][k] = (short)(ft8006m_atoi(str_tmp));
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
		for (j = 0; j < ft8006m_g_ScreenSetParam.iUsedMaxRxNum; j++) {
			ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		sprintf(str, "ForceTouch_ScapCB_OFF_Min_%d", (i + 1));
		dividerPos = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_MCap_DetailThreshold.ForceTouch_SCapCbTest_OFF_Min[i][k] = (short)(ft8006m_atoi(str_tmp));
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

void Ft8006m_OnInit_Incell_DetailThreshold(char *strIniFile)
{

	FTS_TEST_FUNC_ENTER();

	ft8006m_set_max_channel_num();

	Ft8006m_OnInit_InvalidNode(strIniFile);

	Ft8006m_OnInit_DThreshold_RawDataTest(strIniFile);
	Ft8006m_OnInit_DThreshold_CBTest(strIniFile);
	Ft8006m_OnInit_DThreshold_AllButtonCBTest(strIniFile);
	Ft8006m_OnThreshold_VkAndVaRawDataSeparateTest(strIniFile);

	FTS_TEST_FUNC_EXIT();
}

void Ft8006m_OnInit_DThreshold_CBTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue, MaxValue_Vkey, MinValue_Vkey;
	int ChannelNumTest_ChannelXNum, ChannelNumTest_ChannelYNum;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();
	if (ft8006m_g_ScreenSetParam.iSelectedIC >> 4 == IC_FT8606 >> 4) {
		return;
	}

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str, strIniFile);
	MaxValue = ft8006m_atoi(str);

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Max_Vkey", "100", str, strIniFile);
	MaxValue_Vkey = ft8006m_atoi(str);

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX", "15", str, strIniFile);
	ChannelNumTest_ChannelXNum = ft8006m_atoi(str);

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY", "24", str, strIniFile);
	ChannelNumTest_ChannelYNum = ft8006m_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max[i][j] = MaxValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < RX_NUM_MAX; j++) {
				ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max[i][j] = MaxValue_Vkey;
			}
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "CB_Max_Tx%d", (i + 1));

		dividerPos  = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max[i][k] = (short)(ft8006m_atoi(str_tmp));

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



	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, strIniFile);
	MinValue = ft8006m_atoi(str);

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Min_Vkey", "3", str, strIniFile);
	MinValue_Vkey = ft8006m_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min[i][j] = MinValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < RX_NUM_MAX; j++) {
				ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min[i][j] = MinValue_Vkey;
			}
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "CB_Min_Tx%d", (i + 1));
		dividerPos  =  Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min[i][k] = (short)(ft8006m_atoi(str_tmp));

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


void Ft8006m_OnInit_DThreshold_AllButtonCBTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	if (ft8006m_g_ScreenSetParam.iSelectedIC >> 4 != IC_FT8606 >> 4) {
		return;
	}

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str, strIniFile);
	MaxValue = ft8006m_atoi(str);


	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max[i][j] = MaxValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "CB_Max_Tx%d", (i + 1));

		dividerPos  = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max[i][k] = (short)(ft8006m_atoi(str_tmp));

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



	Ft8006m_GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, strIniFile);
	MinValue = ft8006m_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min[i][j] = MinValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "CB_Min_Tx%d", (i + 1));
		dividerPos  =  Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min[i][k] = (short)(ft8006m_atoi(str_tmp));

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


void Ft8006m_OnThreshold_VkAndVaRawDataSeparateTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue, MaxValue_Vkey, MinValue_Vkey;
	int ChannelNumTest_ChannelXNum, ChannelNumTest_ChannelYNum;
	int   dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int  k = 0, i = 0, j = 0;


	FTS_TEST_FUNC_ENTER();

	if ((ft8006m_g_ScreenSetParam.iSelectedIC >> 4 != IC_FT8716 >> 4) && (ft8006m_g_ScreenSetParam.iSelectedIC >> 4 != IC_FT8736 >> 4) && (ft8006m_g_ScreenSetParam.iSelectedIC >> 4 != IC_FTE716 >> 4)) {
		return;
	}

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "11000", str, strIniFile);
	MaxValue = ft8006m_atoi(str);

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max_VKey", "11000", str, strIniFile);
	MaxValue_Vkey = ft8006m_atoi(str);

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX", "15", str, strIniFile);
	ChannelNumTest_ChannelXNum = ft8006m_atoi(str);

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY", "24", str, strIniFile);
	ChannelNumTest_ChannelYNum = ft8006m_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][j] = MaxValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < RX_NUM_MAX; j++) {
				ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][j] = MaxValue_Vkey;
			}
		}
	}


	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "RawData_Max_Tx%d", (i + 1));
		dividerPos  = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max[i][k] = (short)(ft8006m_atoi(str_tmp));

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

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "5000", str, strIniFile);
	MinValue = ft8006m_atoi(str);

	Ft8006m_GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min_VKey", "5000", str, strIniFile);
	MinValue_Vkey = ft8006m_atoi(str);

	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][j] = MinValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < RX_NUM_MAX; j++) {
				ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][j] = MinValue_Vkey;
			}
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		sprintf(str, "RawData_Min_Tx%d", (i + 1));
		dividerPos  = Ft8006m_GetPrivateProfileString("SpecialSet", str, "NULL", strTemp, strIniFile);
		sprintf(strValue, "%s", strTemp);

		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min[i][k] = (short)(ft8006m_atoi(str_tmp));

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

