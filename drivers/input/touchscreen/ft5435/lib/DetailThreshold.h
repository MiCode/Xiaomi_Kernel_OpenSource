/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
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

struct stCfg_MCap_DetailThreshold {
	unsigned char InvalidNode[TX_NUM_MAX][RX_NUM_MAX];
	unsigned char InvalidNode_SC[TX_NUM_MAX][RX_NUM_MAX];

	int RawDataTest_Min[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_Max[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_Low_Min[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_Low_Max[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_High_Min[TX_NUM_MAX][RX_NUM_MAX];
	int RawDataTest_High_Max[TX_NUM_MAX][RX_NUM_MAX];

	int RxLinearityTest_Max[TX_NUM_MAX][RX_NUM_MAX];
	int TxLinearityTest_Max[TX_NUM_MAX][RX_NUM_MAX];

	int SCapRawDataTest_ON_Max[TX_NUM_MAX][RX_NUM_MAX];
	int SCapRawDataTest_ON_Min[TX_NUM_MAX][RX_NUM_MAX];
	int SCapRawDataTest_OFF_Max[TX_NUM_MAX][RX_NUM_MAX];
	int SCapRawDataTest_OFF_Min[TX_NUM_MAX][RX_NUM_MAX];
	short SCapCbTest_ON_Max[TX_NUM_MAX][RX_NUM_MAX];
	short SCapCbTest_ON_Min[TX_NUM_MAX][RX_NUM_MAX];
	short SCapCbTest_OFF_Max[TX_NUM_MAX][RX_NUM_MAX];
	short SCapCbTest_OFF_Min[TX_NUM_MAX][RX_NUM_MAX];

	int ForceTouch_SCapRawDataTest_ON_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapRawDataTest_ON_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapRawDataTest_OFF_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapRawDataTest_OFF_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_ON_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_ON_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_OFF_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_OFF_Min[FORCETOUCH_ROW][RX_NUM_MAX];

	int NoistTest_Coefficient[TX_NUM_MAX][RX_NUM_MAX];

};

struct stCfg_SCap_DetailThreshold {
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
	int DeltaCxTest_Sort[MAX_CHANNEL_NUM];
	int DeltaCxTest_Area[MAX_CHANNEL_NUM];

	int CbTest_Max[MAX_CHANNEL_NUM];
	int CbTest_Min[MAX_CHANNEL_NUM];
	int DeltaCbTest_Base[MAX_CHANNEL_NUM];
	int DifferTest_Base[MAX_CHANNEL_NUM];
	int CBDeviationTest_Base[MAX_CHANNEL_NUM];
	int K1DifferTest_Base[MAX_CHANNEL_NUM];

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
