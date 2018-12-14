/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)，All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: DetailThreshold.h
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: Set Detail Threshold for all IC
*
************************************************************************/

#ifndef _DETAIL_THRESHOLD_H
#define _DETAIL_THRESHOLD_H

#define TX_NUM_MAX			50
#define RX_NUM_MAX			50
#define MAX_PATH			256

#define BUFFER_LENGTH		512
#define MAX_TEST_ITEM		100
#define MAX_GRAPH_ITEM       20
#define MAX_CHANNEL_NUM	144

#define FORCETOUCH_ROW 2

struct stCfg_MCap_DetailThreshold
{
	unsigned char InvalidNode[TX_NUM_MAX][RX_NUM_MAX];无效节点，即不用测试的节点
	unsigned char InvalidNode_SC[TX_NUM_MAX][RX_NUM_MAX];无效节点，即不用测试的节点SCAP

	int RawDataTest_Min[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_Max[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_Low_Min[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_Low_Max[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_High_Min[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_High_Max[TX_NUM_MAX][RX_NUM_MAX];




	int RxLinearityTest_Max[TX_NUM_MAX][RX_NUM_MAX];
	int TxLinearityTest_Max[TX_NUM_MAX][RX_NUM_MAX];




	int SCapRawDataTest_ON_Max[TX_NUM_MAX][RX_NUM_MAX];从这开始都是自容的测试项，互容测试项必须全部放在上面
	int SCapRawDataTest_ON_Min[TX_NUM_MAX][RX_NUM_MAX];
	int SCapRawDataTest_OFF_Max[TX_NUM_MAX][RX_NUM_MAX];
	int SCapRawDataTest_OFF_Min[TX_NUM_MAX][RX_NUM_MAX];
	short SCapCbTest_ON_Max[TX_NUM_MAX][RX_NUM_MAX];
	short SCapCbTest_ON_Min[TX_NUM_MAX][RX_NUM_MAX];
	short SCapCbTest_OFF_Max[TX_NUM_MAX][RX_NUM_MAX];
	short SCapCbTest_OFF_Min[TX_NUM_MAX][RX_NUM_MAX];

	int ForceTouch_SCapRawDataTest_ON_Max[FORCETOUCH_ROW][RX_NUM_MAX];从这开始都是force touch的测试项，互容测试项必须全部放在上面
	int ForceTouch_SCapRawDataTest_ON_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapRawDataTest_OFF_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapRawDataTest_OFF_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_ON_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_ON_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_OFF_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_OFF_Min[FORCETOUCH_ROW][RX_NUM_MAX];



	int NoistTest_Coefficient[TX_NUM_MAX][RX_NUM_MAX];







	/*
	stCfg_MCap_DetailThreshold()
	{
		memset(InvalidNode, 1, sizeof(InvalidNode));

		memset(RawDataTest_Min, 7000, sizeof(RawDataTest_Min));
		memset(RawDataTest_Max, 10000, sizeof(RawDataTest_Max));

		memset(RawDataTest_Low_Min, 7000, sizeof(RawDataTest_Low_Min));
		memset(RawDataTest_Low_Max, 10000, sizeof(RawDataTest_Low_Max));
		memset(RawDataTest_High_Min, 7000, sizeof(RawDataTest_High_Min));
		memset(RawDataTest_High_Max, 10000, sizeof(RawDataTest_High_Max));

		memset(RxCrosstalkTest_Max, 300, sizeof(RxCrosstalkTest_Max));
		memset(RxCrosstalkTest_Min, -100, sizeof(RxCrosstalkTest_Min));

		memset(PanelDifferTest_Max, 1000, sizeof(PanelDifferTest_Max));
		memset(PanelDifferTest_Min, 150, sizeof(PanelDifferTest_Min));

		memset(RxLinearityTest_Max, 0, sizeof(RxLinearityTest_Max));
		memset(TxLinearityTest_Max, 0, sizeof(TxLinearityTest_Max));

		memset(TxShortTest_Max, 1000, sizeof(TxShortTest_Max));
		memset(TxShortTest_Min, 150, sizeof(TxShortTest_Min));

		memset( TxShortAdvance, 0, sizeof(TxShortAdvance) );

		memset(SCapRawDataTest_ON_Max, 1000, sizeof(SCapRawDataTest_ON_Max));
		memset(SCapRawDataTest_ON_Min, 150, sizeof(SCapRawDataTest_ON_Min));

		memset(SCapRawDataTest_OFF_Max, 1000, sizeof(SCapRawDataTest_OFF_Max));
		memset(SCapRawDataTest_OFF_Min, 150, sizeof(SCapRawDataTest_OFF_Min));

		memset(SCapCbTest_ON_Max, 240, sizeof(SCapCbTest_ON_Max));
		memset(SCapCbTest_ON_Min, 0, sizeof(SCapCbTest_ON_Min));

		memset(SCapCbTest_OFF_Max, 240, sizeof(SCapCbTest_OFF_Max));
		memset(SCapCbTest_OFF_Min, 0, sizeof(SCapCbTest_OFF_Min));

		memset(CMTest_Min, 5.0, sizeof(CMTest_Min));
		memset(CMTest_Max, 0.5, sizeof(CMTest_Max));

		memset(NoistTest_Coefficient, 0, sizeof(NoistTest_Coefficient));

		memset(SITORawdata_RxLinearityTest_Base, 0, sizeof(SITORawdata_RxLinearityTest_Base));
		memset(SITORawdata_TxLinearityTest_Base, 0, sizeof(SITORawdata_TxLinearityTest_Base));

		memset(UniformityRxLinearityTest_Hole, 0, sizeof(UniformityRxLinearityTest_Hole));
		memset(UniformityTxLinearityTest_Hole, 0, sizeof(UniformityTxLinearityTest_Hole));
	}
	*/

};

struct stCfg_SCap_DetailThreshold
{
	int TempData[MAX_CHANNEL_NUM];
	int RawDataTest_Max[MAX_CHANNEL_NUM];
	int RawDataTest_Min[MAX_CHANNEL_NUM];
	int CiTest_Max[MAX_CHANNEL_NUM];
	int CiTest_Min[MAX_CHANNEL_NUM];
	int DeltaCiTest_Base[MAX_CHANNEL_NUM];
	int DeltaCiTest_AnotherBase1[MAX_CHANNEL_NUM];
	int DeltaCiTest_AnotherBase2[MAX_CHANNEL_NUM];
	int CiDeviationTest_Base[MAX_CHANNEL_NUM];

	int NoiseTest_Max[MAX_CHANNEL_NUM];
	int DeltaCxTest_Sort[MAX_CHANNEL_NUM];         对6x06与6x36通用
	int DeltaCxTest_Area[MAX_CHANNEL_NUM];         对6x06与6x36通用

	int CbTest_Max[MAX_CHANNEL_NUM];
	int CbTest_Min[MAX_CHANNEL_NUM];
	int DeltaCbTest_Base[MAX_CHANNEL_NUM];
	int DifferTest_Base[MAX_CHANNEL_NUM];
	int CBDeviationTest_Base[MAX_CHANNEL_NUM];
	int K1DifferTest_Base[MAX_CHANNEL_NUM];

	/*
	stCfg_SCap_DetailThreshold()
	{
		memset(TempData, 0, sizeof(TempData));

		memset(RawDataTest_Max, 0, sizeof(RawDataTest_Max));
		memset(RawDataTest_Min, 1, sizeof(RawDataTest_Min));

		memset(CiTest_Max, 0, sizeof(RawDataTest_Max));
		memset(CiTest_Min, 1, sizeof(RawDataTest_Min));

		memset(DeltaCiTest_Base, 0, sizeof(DeltaCiTest_Base));
		memset(DeltaCiTest_AnotherBase1, 0, sizeof(DeltaCiTest_AnotherBase1));
		memset(DeltaCiTest_AnotherBase2, 0, sizeof(DeltaCiTest_AnotherBase2));

		memset(NoiseTest_Max, 0, sizeof(NoiseTest_Max));
		memset(CiDeviationTest_Base, 0, sizeof(CiDeviationTest_Base));

		memset(DeltaCxTest_Sort, 1, sizeof(DeltaCxTest_Sort));

		memset(CbTest_Max, 0, sizeof(CbTest_Max));
		memset(CbTest_Min, 1, sizeof(CbTest_Min));
		memset(DeltaCbTest_Base, 0, sizeof(DeltaCbTest_Base));
        	memset(DifferTest_Base, 0, sizeof(DifferTest_Base));
		memset(CBDeviationTest_Base, 0, sizeof(CBDeviationTest_Base));
	}
	*/

};




void OnInit_MCap_DetailThreshold(char *strIniFile);
void OnInit_SCap_DetailThreshold(char *strIniFile);

void OnInit_InvalidNode(char *strIniFile);
void OnGetTestItemParam(char *strItemName, char *strIniFile, int iDefautValue);
void OnInit_DThreshold_RawDataTest(char *strIniFile);
void OnInit_DThreshold_SCapRawDataTest(char *strIniFile);
void OnInit_DThreshold_SCapCbTest(char *strIniFile);

void OnInit_DThreshold_ForceTouch_SCapRawDataTest(char *strIniFile);
void OnInit_DThreshold_ForceTouch_SCapCbTest(char *strIniFile);

void OnInit_DThreshold_RxLinearityTest(char *strIniFile);
void OnInit_DThreshold_TxLinearityTest(char *strIniFile);

void set_max_channel_num(void);

#endif
