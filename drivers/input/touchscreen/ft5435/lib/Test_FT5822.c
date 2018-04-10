/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT5822.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: test item for FT5822\FT5626\FT5726\FT5826B
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "Global.h"
#include "DetailThreshold.h"
#include "Test_FT5822.h"
#include "Config_FT5822.h"


/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define IC_TEST_VERSION  "Test version: V1.0.0--2015-07-30, (sync version of FT_MultipleTest: V2.7.0.3--2015-07-13)"

#define DEVIDE_MODE_ADDR	0x00
#define REG_LINE_NUM	0x01
#define REG_TX_NUM	0x02
#define REG_RX_NUM	0x03
#define REG_PATTERN_5422        0x53
#define REG_MAPPING_SWITCH      0x54
#define REG_TX_NOMAPPING_NUM        0x55
#define REG_RX_NOMAPPING_NUM      0x56
#define REG_NORMALIZE_TYPE      0x16
#define REG_ScCbBuf0	0x4E
#define REG_ScWorkMode	0x44
#define REG_ScCbAddrR	0x45
#define REG_RawBuf0 0x36
#define REG_WATER_CHANNEL_SELECT 0x09

#define REG_FREQUENCY           0x0A
#define REG_FIR                 0XFB

#define MIN_HOLE_LEVEL   (-1)
#define MAX_HOLE_LEVEL   0x7F

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
enum WaterproofType {
	WT_NeedProofOnTest,
	WT_NeedProofOffTest,
	WT_NeedTxOnVal,
	WT_NeedRxOnVal,
	WT_NeedTxOffVal,
	WT_NeedRxOffVal,
};
/*******************************************************************************
* Static variables
*******************************************************************************/

static int m_RawData[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} };
static int m_iTempRawData[TX_NUM_MAX * RX_NUM_MAX] = {0};
static unsigned char m_ucTempData[TX_NUM_MAX * RX_NUM_MAX*2] = {0};
static bool m_bV3TP;
static int RxLinearity[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} };
static int TxLinearity[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} };

static char g_pStoreAllData[1024*80] = {0};
static char *g_pTmpBuff;
static char *g_pStoreMsgArea;
static int g_lenStoreMsgArea;
static char *g_pMsgAreaLine2;
static int g_lenMsgAreaLine2;
static char *g_pStoreDataArea;
static int g_lenStoreDataArea;
static unsigned char m_ucTestItemCode;
static int m_iStartLine;
static int m_iTestDataCount;

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/


/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int StartScan(void);
static unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer);
static unsigned char GetPanelRows(unsigned char *pPanelRows);
static unsigned char GetPanelCols(unsigned char *pPanelCols);
static unsigned char GetTxSC_CB(unsigned char index, unsigned char *pcbValue);

static unsigned char GetRawData(void);
static unsigned char GetChannelNum(void);

static void InitTest(void);
static void FinishTest(void);
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount);
static void InitStoreParamOfTestData(void);
static void MergeAllTestData(void);

static void AllocateMemory(void);
static void FreeMemory(void);
static void ShowRawData(void);
static boolean GetTestCondition(int iTestType, unsigned char ucChannelValue);

static unsigned char GetChannelNumNoMapping(void);
static unsigned char SwitchToNoMapping(void);

/************************************************************************
* Name: FT5822_StartTest
* Brief:  Test entry. Determine which test item to test
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/
boolean FT5822_StartTest()
{
	bool bTestResult = true;
	bool bTempResult = 1;
	unsigned char ReCode = 0;
	unsigned char ucDevice = 0;
	int iItemCount = 0;

	InitTest();

	if (0 == g_TestItemNum)
		bTestResult = false;

	for (iItemCount = 0; iItemCount < g_TestItemNum; iItemCount++) {
		m_ucTestItemCode = g_stTestItem[ucDevice][iItemCount].ItemCode;

		if (Code_FT5822_ENTER_FACTORY_MODE == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT5822_TestItem_EnterFactoryMode();
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
				break;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT5822_RAWDATA_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT5822_TestItem_RawDataTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT5822_SCAP_CB_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT5822_TestItem_SCapCbTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT5822_SCAP_RAWDATA_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT5822_TestItem_SCapRawDataTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT5822_UNIFORMITY_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT5822_TestItem_UniformityTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

	}

	FinishTest();

	return bTestResult;
}
/************************************************************************
* Name: InitTest
* Brief:  Init all param before test
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void InitTest(void)
{
	AllocateMemory();
	InitStoreParamOfTestData();
	printk("[focal] %s \n", IC_TEST_VERSION);
}
/************************************************************************
* Name: FinishTest
* Brief:  Init all param before test
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void FinishTest(void)
{
	MergeAllTestData();
	FreeMemory();
}
/************************************************************************
* Name: FT5822_get_test_data
* Brief:  get data of test result
* Input: none
* Output: pTestData, the returned buff
* Return: the length of test data. if length > 0, got data;else ERR.
***********************************************************************/
int FT5822_get_test_data(char *pTestData)
{
	if (NULL == pTestData) {
		printk("[focal] %s pTestData == NULL \n", __func__);
		return -EPERM;
	}
	memcpy(pTestData, g_pStoreAllData, (g_lenStoreMsgArea+g_lenStoreDataArea));
	return g_lenStoreMsgArea+g_lenStoreDataArea;

}

/************************************************************************
* Name: FT5822_TestItem_EnterFactoryMode
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5822_TestItem_EnterFactoryMode(void)
{
	unsigned char ReCode = ERROR_CODE_INVALID_PARAM;
	int iRedo = 5;
	int i ;
	unsigned char chPattern = 0;

	SysDelay(150);
	for (i = 1; i <= iRedo; i++) {
		ReCode = EnterFactory();
		if (ERROR_CODE_OK != ReCode) {
			printk("Failed to Enter factory mode...\n");
			if (i < iRedo) {
				SysDelay(50);
				continue;
			}
		} else {
			break;
		}

	}
	SysDelay(300);

	if (ReCode != ERROR_CODE_OK) {
		return ReCode;
	}

	ReCode = GetChannelNum();

	ReCode = ReadReg(REG_PATTERN_5422, &chPattern);
	if (chPattern == 1) {
		m_bV3TP = true;
	} else {
		m_bV3TP = false;
	}

	return ReCode;
}
/************************************************************************
* Name: FT5822_TestItem_RawDataTest
* Brief:  TestItem: RawDataTest. Check if MCAP RawData is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5822_TestItem_RawDataTest(bool *bTestResult)
{
	unsigned char ReCode = 0;
	bool btmpresult = true;
	int RawDataMin;
	int RawDataMax;
	unsigned char ucFre;
	unsigned char strSwitch = 0;
	unsigned char OriginValue = 0xff;
	int index = 0;
	int iRow, iCol;
	int iValue = 0;


	printk("\n\n==============================Test Item: -------- Raw Data  Test \n\n");
	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		printk("\n\n// Failed to Enter factory Mode. Error Code: %d", ReCode);
		goto TEST_ERR;
	}

	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &strSwitch);
		if (strSwitch != 0) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 0);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
		}
	}

	ReCode = ReadReg(REG_NORMALIZE_TYPE, &OriginValue);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;


	if (g_ScreenSetParam.isNormalize == Auto_Normalize) {
		if (OriginValue != 1) {
			ReCode = WriteReg(REG_NORMALIZE_TYPE, 0x01);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
		}

		printk("\n=========Set Frequecy High\n");
		ReCode = WriteReg(0x0A, 0x81);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		printk("\n=========FIR State: ON\n");
		ReCode = WriteReg(0xFB, 1);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;
		for (index = 0; index < 3; ++index) {
			ReCode = GetRawData();
		}

		if (ReCode != ERROR_CODE_OK) {
			printk("\nGet Rawdata failed, Error Code: 0x%x",  ReCode);
			goto TEST_ERR;
		}

		ShowRawData();

		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
				if (g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol] == 0)
					continue;
				RawDataMin = g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min[iRow][iCol];
				RawDataMax = g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max[iRow][iCol];
				iValue = m_RawData[iRow][iCol];
				if (iValue < RawDataMin || iValue > RawDataMax) {
					btmpresult = false;
					printk("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
							iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
				}
			}
		}

		Save_Test_Data(m_RawData, 0, g_ScreenSetParam.iTxNum, g_ScreenSetParam.iRxNum, 1);
	} else {
		if (OriginValue != 0) {
			ReCode = WriteReg(REG_NORMALIZE_TYPE, 0x00);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
		}

		ReCode =  ReadReg(0x0A, &ucFre);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		if (g_stCfg_FT5822_BasicThreshold.RawDataTest_SetLowFreq) {
			printk("\n=========Set Frequecy Low\n");
			ReCode = WriteReg(0x0A, 0x80);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;

			printk("\n=========FIR State: OFF\n");
			ReCode = WriteReg(0xFB, 0);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
			SysDelay(100);
			for (index = 0; index < 3; ++index) {
				ReCode = GetRawData();
			}

			if (ReCode != ERROR_CODE_OK) {
				printk("\nGet Rawdata failed, Error Code: 0x%x",  ReCode);
				goto TEST_ERR;
			}
			ShowRawData();
			for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {

				for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
					if (g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol] == 0)
						continue;
					RawDataMin = g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min[iRow][iCol];
					RawDataMax = g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max[iRow][iCol];
					iValue = m_RawData[iRow][iCol];
					if (iValue < RawDataMin || iValue > RawDataMax) {
						btmpresult = false;
						printk("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
								iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
					}
				}
			}

			Save_Test_Data(m_RawData, 0, g_ScreenSetParam.iTxNum, g_ScreenSetParam.iRxNum, 1);
		}

		if (g_stCfg_FT5822_BasicThreshold.RawDataTest_SetHighFreq) {

			printk("\n=========Set Frequecy High\n");
			ReCode = WriteReg(0x0A, 0x81);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;

			printk("\n=========FIR State: OFF\n");
			ReCode = WriteReg(0xFB, 0);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
			SysDelay(100);
			for (index = 0; index < 3; ++index) {
				ReCode = GetRawData();
			}

			if (ReCode != ERROR_CODE_OK) {
				printk("\nGet Rawdata failed, Error Code: 0x%x",  ReCode);
				if (ReCode != ERROR_CODE_OK)
					goto TEST_ERR;
			}
			ShowRawData();

			for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {

				for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
					if (g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol] == 0)
						continue;
					RawDataMin = g_stCfg_MCap_DetailThreshold.RawDataTest_High_Min[iRow][iCol];
					RawDataMax = g_stCfg_MCap_DetailThreshold.RawDataTest_High_Max[iRow][iCol];
					iValue = m_RawData[iRow][iCol];
					if (iValue < RawDataMin || iValue > RawDataMax) {
						btmpresult = false;
						printk("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
								iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
					}
				}
			}

			Save_Test_Data(m_RawData, 0, g_ScreenSetParam.iTxNum, g_ScreenSetParam.iRxNum, 2);
		}

	}

	ReCode = WriteReg(REG_NORMALIZE_TYPE, OriginValue);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	if (m_bV3TP) {
		ReCode = WriteReg(REG_MAPPING_SWITCH, strSwitch);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;
	}


	if (btmpresult) {
		*bTestResult = true;
		printk("\n\n//RawData Test is OK!\n");
	} else {
		*bTestResult = false;
		printk("\n\n//RawData Test is NG!\n");
	}
	return ReCode;

TEST_ERR:

	*bTestResult = false;
	printk("\n\n//RawData Test is NG!\n");
	return ReCode;

}
/************************************************************************
* Name: FT5822_TestItem_SCapRawDataTest
* Brief:  TestItem: SCapRawDataTest. Check if SCAP RawData is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5822_TestItem_SCapRawDataTest(bool *bTestResult)
{
	int i = 0;
	int RawDataMin = 0;
	int RawDataMax = 0;
	int Value = 0;
	boolean bFlag = true;
	unsigned char ReCode = 0;
	boolean btmpresult = true;
	int iMax = 0;
	int iMin = 0;
	int iAvg = 0;
	int ByteNum = 0;
	unsigned char wc_value = 0;
	unsigned char ucValue = 0;
	int iCount = 0;
	int ibiggerValue = 0;

	printk("\n\n==============================Test Item: -------- Scap RawData Test \n\n");

	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		printk("\n\n// Failed to Enter factory Mode. Error Code: %d", ReCode);
		goto TEST_ERR;
	}


	ReCode = ReadReg(REG_WATER_CHANNEL_SELECT, &wc_value);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;


	ReCode = SwitchToNoMapping();
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;


	ReCode = StartScan();
	if (ReCode != ERROR_CODE_OK) {
		printk("Failed to Scan SCap RawData! \n");
		goto TEST_ERR;
	}
	for (i = 0; i < 3; i++) {
		memset(m_iTempRawData, 0, sizeof(m_iTempRawData));

		ByteNum = (g_ScreenSetParam.iTxNum + g_ScreenSetParam.iRxNum)*2;
		ReCode = ReadRawData(0, 0xAC, ByteNum, m_iTempRawData);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;
		memcpy(m_RawData[0+g_ScreenSetParam.iTxNum], m_iTempRawData, sizeof(int)*g_ScreenSetParam.iRxNum);
		memcpy(m_RawData[1+g_ScreenSetParam.iTxNum], m_iTempRawData + g_ScreenSetParam.iRxNum, sizeof(int)*g_ScreenSetParam.iTxNum);

		ByteNum = (g_ScreenSetParam.iTxNum + g_ScreenSetParam.iRxNum)*2;
		ReCode = ReadRawData(0, 0xAB, ByteNum, m_iTempRawData);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;
		memcpy(m_RawData[2+g_ScreenSetParam.iTxNum], m_iTempRawData, sizeof(int)*g_ScreenSetParam.iRxNum);
		memcpy(m_RawData[3+g_ScreenSetParam.iTxNum], m_iTempRawData + g_ScreenSetParam.iRxNum, sizeof(int)*g_ScreenSetParam.iTxNum);
	}

	bFlag = GetTestCondition(WT_NeedProofOnTest, wc_value);
	if (g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_SetWaterproof_ON && bFlag) {
		iCount = 0;
		RawDataMin = g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_ON_Min;
		RawDataMax = g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_ON_Max;
		iMax = -m_RawData[0+g_ScreenSetParam.iTxNum][0];
		iMin = 2 * m_RawData[0+g_ScreenSetParam.iTxNum][0];
		iAvg = 0;
		Value = 0;

		bFlag = GetTestCondition(WT_NeedRxOnVal, wc_value);
		if (bFlag)
			printk("Judge Rx in Waterproof-ON:\n");
		for (i = 0; bFlag && i < g_ScreenSetParam.iRxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[0][i] == 0)
				continue;
			RawDataMin = g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min[0][i];
			RawDataMax = g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max[0][i];
			Value = m_RawData[0+g_ScreenSetParam.iTxNum][i];
			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				printk("Failed. Num = %d, Value = %d, range = (%d, %d):\n", i+1, Value, RawDataMin, RawDataMax);
			}
			iCount++;
		}


		bFlag = GetTestCondition(WT_NeedTxOnVal, wc_value);
		if (bFlag)
			printk("Judge Tx in Waterproof-ON:\n");
		for (i = 0; bFlag && i < g_ScreenSetParam.iTxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[1][i] == 0)
				continue;
			RawDataMin = g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min[1][i];
			RawDataMax = g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max[1][i];
			Value = m_RawData[1+g_ScreenSetParam.iTxNum][i];
			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				printk("Failed. Num = %d, Value = %d, range = (%d, %d):\n", i+1, Value, RawDataMin, RawDataMax);
			}
			iCount++;
		}
		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg/iCount;

		printk("SCap RawData in Waterproof-ON, Max : %d, Min: %d, Deviation: %d, Average: %d\n", iMax, iMin, iMax - iMin, iAvg);
		ibiggerValue = g_ScreenSetParam.iTxNum > g_ScreenSetParam.iRxNum?g_ScreenSetParam.iTxNum:g_ScreenSetParam.iRxNum;
		Save_Test_Data(m_RawData, g_ScreenSetParam.iTxNum+0, 2, ibiggerValue, 1);
	}


	bFlag = GetTestCondition(WT_NeedProofOffTest, wc_value);
	if (g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_SetWaterproof_OFF && bFlag) {
		iCount = 0;
		RawDataMin = g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_OFF_Min;
		RawDataMax = g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_OFF_Max;
		iMax = -m_RawData[2+g_ScreenSetParam.iTxNum][0];
		iMin = 2 * m_RawData[2+g_ScreenSetParam.iTxNum][0];
		iAvg = 0;
		Value = 0;

		bFlag = GetTestCondition(WT_NeedRxOffVal, wc_value);
		if (bFlag)
			printk("Judge Rx in Waterproof-OFF:\n");
		for (i = 0; bFlag && i < g_ScreenSetParam.iRxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[0][i] == 0)
				continue;
			RawDataMin = g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min[0][i];
			RawDataMax = g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max[0][i];
			Value = m_RawData[2+g_ScreenSetParam.iTxNum][i];
			iAvg += Value;

			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				printk("Failed. Num = %d, Value = %d, range = (%d, %d):\n", i+1, Value, RawDataMin, RawDataMax);
			}
			iCount++;
		}

		bFlag = GetTestCondition(WT_NeedTxOffVal, wc_value);
		if (bFlag)
			printk("Judge Tx in Waterproof-OFF:\n");
		for (i = 0; bFlag && i < g_ScreenSetParam.iTxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[1][i] == 0)
				continue;

			Value = m_RawData[3+g_ScreenSetParam.iTxNum][i];
			RawDataMin = g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min[1][i];
			RawDataMax = g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max[1][i];

			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				printk("Failed. Num = %d, Value = %d, range = (%d, %d):\n", i+1, Value, RawDataMin, RawDataMax);
			}
			iCount++;
		}
		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg/iCount;

		printk("SCap RawData in Waterproof-OFF, Max : %d, Min: %d, Deviation: %d, Average: %d\n", iMax, iMin, iMax - iMin, iAvg);
		ibiggerValue = g_ScreenSetParam.iTxNum > g_ScreenSetParam.iRxNum?g_ScreenSetParam.iTxNum:g_ScreenSetParam.iRxNum;
		Save_Test_Data(m_RawData, g_ScreenSetParam.iTxNum+2, 2, ibiggerValue, 2);
	}

	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &ucValue);
		if (0 != ucValue) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 0);
			SysDelay(10);
			if (ReCode != ERROR_CODE_OK) {
				printk("Failed to switch mapping type!\n ");
				btmpresult = false;
			}
		}

		GetChannelNum();
	}
	if (btmpresult) {
		*bTestResult = true;
		printk("\n\n//SCap RawData Test is OK!\n");
	} else {
		*bTestResult = false;
		printk("\n\n//SCap RawData Test is NG!\n");
	}
	return ReCode;

TEST_ERR:
	*bTestResult = false;
	printk("\n\n//SCap RawData Test is NG!\n");
	return ReCode;
}

/************************************************************************
* Name: FT5822_TestItem_SCapCbTest
* Brief:  TestItem: SCapCbTest. Check if SCAP Cb is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5822_TestItem_SCapCbTest(bool *bTestResult)
{
	int i, index, Value, CBMin, CBMax;
	boolean bFlag = true;
	unsigned char ReCode;
	boolean btmpresult = true;
	int iMax, iMin, iAvg;
	unsigned char wc_value = 0;
	unsigned char ucValue = 0;
	int iCount = 0;
	int ibiggerValue = 0;

	printk("\n\n==============================Test Item: -----  Scap CB Test \n\n");


	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		printk("\n\n// Failed to Enter factory Mode. Error Code: %d", ReCode);
		goto TEST_ERR;
	}


	ReCode = ReadReg(REG_WATER_CHANNEL_SELECT, &wc_value);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;


	bFlag = SwitchToNoMapping();
	if (bFlag) {
		printk("Failed to SwitchToNoMapping! \n");
		goto TEST_ERR;
	}


	ReCode = StartScan();
	if (ReCode != ERROR_CODE_OK) {
		printk("Failed to Scan SCap RawData! \n");
		goto TEST_ERR;
	}


	for (i = 0; i < 3; i++) {
		memset(m_RawData, 0, sizeof(m_RawData));
		memset(m_ucTempData, 0, sizeof(m_ucTempData));

		ReCode = WriteReg(REG_ScWorkMode, 1);
		ReCode = StartScan();
		ReCode = WriteReg(REG_ScCbAddrR, 0);
		ReCode = GetTxSC_CB(g_ScreenSetParam.iTxNum + g_ScreenSetParam.iRxNum + 128, m_ucTempData);
		for (index = 0; index < g_ScreenSetParam.iRxNum; ++index) {
			m_RawData[0 + g_ScreenSetParam.iTxNum][index] = m_ucTempData[index];
		}
		for (index = 0; index < g_ScreenSetParam.iTxNum; ++index) {
			m_RawData[1 + g_ScreenSetParam.iTxNum][index] = m_ucTempData[index + g_ScreenSetParam.iRxNum];
		}

		ReCode = WriteReg(REG_ScWorkMode, 0);
		ReCode = StartScan();
		ReCode = WriteReg(REG_ScCbAddrR, 0);
		ReCode = GetTxSC_CB(g_ScreenSetParam.iRxNum + g_ScreenSetParam.iTxNum + 128, m_ucTempData);
		for (index = 0; index < g_ScreenSetParam.iRxNum; ++index) {
			m_RawData[2 + g_ScreenSetParam.iTxNum][index] = m_ucTempData[index];
		}
		for (index = 0; index < g_ScreenSetParam.iTxNum; ++index) {
			m_RawData[3 + g_ScreenSetParam.iTxNum][index] = m_ucTempData[index + g_ScreenSetParam.iRxNum];
		}

		if (ReCode != ERROR_CODE_OK) {
			printk("Failed to Get SCap CB!\n");
		}
	}

	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	bFlag = GetTestCondition(WT_NeedProofOnTest, wc_value);
	if (g_stCfg_FT5822_BasicThreshold.SCapCbTest_SetWaterproof_ON && bFlag) {
		printk("SCapCbTest in WaterProof On Mode:  \n");

		iMax = -m_RawData[0+g_ScreenSetParam.iTxNum][0];
		iMin = 2 * m_RawData[0+g_ScreenSetParam.iTxNum][0];
		iAvg = 0;
		Value = 0;
		iCount = 0;

		bFlag = GetTestCondition(WT_NeedRxOnVal, wc_value);
		if (bFlag)
			printk("SCap CB_Rx:  \n");
		for (i = 0; bFlag && i < g_ScreenSetParam.iRxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[0][i] == 0)
				continue;
			CBMin = g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min[0][i];
			CBMax = g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max[0][i];
			Value = m_RawData[0+g_ScreenSetParam.iTxNum][i];
			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				printk("Failed. Num = %d, Value = %d, range = (%d, %d):\n", i+1, Value, CBMin, CBMax);
			}
			iCount++;
		}
		bFlag = GetTestCondition(WT_NeedTxOnVal, wc_value);
		if (bFlag)
			printk("SCap CB_Tx:  \n");
		for (i = 0; bFlag &&  i < g_ScreenSetParam.iTxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[1][i] == 0)
				continue;
			CBMin = g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min[1][i];
			CBMax = g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max[1][i];
			Value = m_RawData[1+g_ScreenSetParam.iTxNum][i];
			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				printk("Failed. Num = %d, Value = %d, range = (%d, %d):\n", i+1, Value, CBMin, CBMax);
			}
			iCount++;
		}

		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg/iCount;

		printk("SCap CB in Waterproof-ON, Max : %d, Min: %d, Deviation: %d, Average: %d\n", iMax, iMin, iMax - iMin, iAvg);
		ibiggerValue = g_ScreenSetParam.iTxNum > g_ScreenSetParam.iRxNum?g_ScreenSetParam.iTxNum:g_ScreenSetParam.iRxNum;
		Save_Test_Data(m_RawData, g_ScreenSetParam.iTxNum+0, 2, ibiggerValue, 1);
	}

	bFlag = GetTestCondition(WT_NeedProofOffTest, wc_value);
	if (g_stCfg_FT5822_BasicThreshold.SCapCbTest_SetWaterproof_OFF && bFlag) {
		printk("SCapCbTest in WaterProof OFF Mode:  \n");
		iMax = -m_RawData[2+g_ScreenSetParam.iTxNum][0];
		iMin = 2 * m_RawData[2+g_ScreenSetParam.iTxNum][0];
		iAvg = 0;
		Value = 0;
		iCount = 0;

		bFlag = GetTestCondition(WT_NeedRxOffVal, wc_value);
		if (bFlag)
			printk("SCap CB_Rx:  \n");
		for (i = 0; bFlag &&  i < g_ScreenSetParam.iRxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[0][i] == 0)
				continue;
			CBMin = g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min[0][i];
			CBMax = g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max[0][i];
			Value = m_RawData[2+g_ScreenSetParam.iTxNum][i];
			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				printk("Failed. Num = %d, Value = %d, range = (%d, %d):\n", i+1, Value, CBMin, CBMax);
			}
			iCount++;
		}

		bFlag = GetTestCondition(WT_NeedTxOffVal, wc_value);
		if (bFlag)
			printk("SCap CB_Tx:  \n");
		for (i = 0; bFlag && i < g_ScreenSetParam.iTxNum; i++) {

			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[1][i] == 0)
				continue;
			CBMin = g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min[1][i];
			CBMax = g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max[1][i];
			Value = m_RawData[3+g_ScreenSetParam.iTxNum][i];

			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				printk("Failed. Num = %d, Value = %d, range = (%d, %d):\n", i+1, Value, CBMin, CBMax);
			}
			iCount++;
		}

		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg/iCount;

		printk("SCap CB in Waterproof-OFF, Max : %d, Min: %d, Deviation: %d, Average: %d\n", iMax, iMin, iMax - iMin, iAvg);
		ibiggerValue = g_ScreenSetParam.iTxNum > g_ScreenSetParam.iRxNum?g_ScreenSetParam.iTxNum:g_ScreenSetParam.iRxNum;
		Save_Test_Data(m_RawData, g_ScreenSetParam.iTxNum+2, 2, ibiggerValue, 2);
	}

	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &ucValue);
		if (0 != ucValue) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 0);
			SysDelay(10);
			if (ReCode != ERROR_CODE_OK) {
				printk("Failed to switch mapping type!\n ");
				btmpresult = false;
			}
		}

		GetChannelNum();
	}

	if (btmpresult) {
		*bTestResult = true;
		printk("\n\n//SCap CB Test Test is OK!\n");
	} else {
		*bTestResult = false;
		printk("\n\n//SCap CB Test Test is NG!\n");
	}
	return ReCode;

TEST_ERR:

	*bTestResult = false;
	printk("\n\n//SCap CB Test Test is NG!\n");
	return ReCode;
}

/************************************************************************
* Name: GetPanelRows(Same function name as FT_MultipleTest)
* Brief:  Get row of TP
* Input: none
* Output: pPanelRows
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetPanelRows(unsigned char *pPanelRows)
{
	return ReadReg(REG_TX_NUM, pPanelRows);
}

/************************************************************************
* Name: GetPanelCols(Same function name as FT_MultipleTest)
* Brief:  get column of TP
* Input: none
* Output: pPanelCols
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetPanelCols(unsigned char *pPanelCols)
{
	return ReadReg(REG_RX_NUM, pPanelCols);
}
/************************************************************************
* Name: StartScan(Same function name as FT_MultipleTest)
* Brief:  Scan TP, do it before read Raw Data
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static int StartScan(void)
{
	unsigned char RegVal = 0;
	unsigned char times = 0;
	const unsigned char MaxTimes = 20;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
	if (ReCode == ERROR_CODE_OK) {
		RegVal |= 0x80;
		ReCode = WriteReg(DEVIDE_MODE_ADDR, RegVal);
		if (ReCode == ERROR_CODE_OK) {
			while (times++ < MaxTimes) {
				SysDelay(8);
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
				if (ReCode == ERROR_CODE_OK) {
					if ((RegVal>>7) == 0)
						break;
				} else {
					break;
				}
			}
			if (times < MaxTimes)
				ReCode = ERROR_CODE_OK;
			else
				ReCode = ERROR_CODE_COMM_ERROR;
		}
	}
	return ReCode;

}
/************************************************************************
* Name: ReadRawData(Same function name as FT_MultipleTest)
* Brief:  read Raw Data
* Input: Freq(No longer used, reserved), LineNum, ByteNum
* Output: pRevBuffer
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	unsigned char I2C_wBuffer[3];
	int i, iReadNum;
	unsigned short BytesNumInTestMode1 = 0;

	iReadNum = ByteNum/342;

	if (0 != (ByteNum%342))
		iReadNum++;

	if (ByteNum <= 342) {
		BytesNumInTestMode1 = ByteNum;
	} else {
		BytesNumInTestMode1 = 342;
	}

	ReCode = WriteReg(REG_LINE_NUM, LineNum);

	I2C_wBuffer[0] = REG_RawBuf0;
	if (ReCode == ERROR_CODE_OK) {
		focal_msleep(10);
		ReCode = Comm_Base_IIC_IO(I2C_wBuffer, 1, m_ucTempData, BytesNumInTestMode1);
	}

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK)
			break;

		if (i == iReadNum-1) {
			focal_msleep(10);
			ReCode = Comm_Base_IIC_IO(NULL, 0, m_ucTempData+342*i, ByteNum-342*i);
		} else {
			focal_msleep(10);
			ReCode = Comm_Base_IIC_IO(NULL, 0, m_ucTempData+342*i, 342);
		}

	}

	if (ReCode == ERROR_CODE_OK) {
		for (i = 0; i < (ByteNum>>1); i++) {
			pRevBuffer[i] = (m_ucTempData[i<<1]<<8)+m_ucTempData[(i<<1)+1];

		}
	}

	return ReCode;

}
/************************************************************************
* Name: GetTxSC_CB(Same function name as FT_MultipleTest)
* Brief:  get CB of Tx SCap
* Input: index
* Output: pcbValue
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char GetTxSC_CB(unsigned char index, unsigned char *pcbValue)
{
	unsigned char ReCode = ERROR_CODE_OK;
	unsigned char wBuffer[4];

	if (index < 128) {
		*pcbValue = 0;
		WriteReg(REG_ScCbAddrR, index);
		ReCode = ReadReg(REG_ScCbBuf0, pcbValue);
	} else {
		WriteReg(REG_ScCbAddrR, 0);
		wBuffer[0] = REG_ScCbBuf0;
		ReCode = Comm_Base_IIC_IO(wBuffer, 1, pcbValue, index-128);

	}

	return ReCode;
}

/************************************************************************
* Name: AllocateMemory
* Brief:  Allocate pointer Memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void AllocateMemory(void)
{

	g_pStoreMsgArea = NULL;
	if (NULL == g_pStoreMsgArea)
		g_pStoreMsgArea = kmalloc(1024*8, GFP_ATOMIC);
	g_pMsgAreaLine2 = NULL;
	if (NULL == g_pMsgAreaLine2)
		g_pMsgAreaLine2 = kmalloc(1024*8, GFP_ATOMIC);
	g_pStoreDataArea = NULL;
	if (NULL == g_pStoreDataArea)
		g_pStoreDataArea = kmalloc(1024*80, GFP_ATOMIC);
	if (NULL == g_pTmpBuff)
		g_pTmpBuff = kmalloc(1024*16, GFP_ATOMIC);

}
/************************************************************************
* Name: FreeMemory
* Brief:  Release pointer memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void FreeMemory(void)
{

	if (NULL != g_pStoreMsgArea)
		kfree(g_pStoreMsgArea);

	if (NULL != g_pMsgAreaLine2)
		kfree(g_pMsgAreaLine2);

	if (NULL != g_pStoreDataArea)
		kfree(g_pStoreDataArea);

	if (NULL != g_pTmpBuff)
		kfree(g_pTmpBuff);
}

/************************************************************************
* Name: InitStoreParamOfTestData
* Brief:  Init store param of test data
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void InitStoreParamOfTestData(void)
{

	g_lenStoreMsgArea = 0;

	g_lenStoreMsgArea += sprintf(g_pStoreMsgArea, "ECC, 85, 170, IC Name, %s, IC Code, %x\n", g_strIcName,  g_ScreenSetParam.iSelectedIC);

	g_lenMsgAreaLine2 = 0;

	g_lenStoreDataArea = 0;
	m_iStartLine = 11;
	m_iTestDataCount = 0;
}
/************************************************************************
* Name: MergeAllTestData
* Brief:  Merge All Data of test result
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void MergeAllTestData(void)
{
	int iLen = 0;


	iLen = sprintf(g_pTmpBuff, "TestItem, %d, ", m_iTestDataCount);
	memcpy(g_pStoreMsgArea+g_lenStoreMsgArea, g_pTmpBuff, iLen);
	g_lenStoreMsgArea += iLen;


	memcpy(g_pStoreMsgArea+g_lenStoreMsgArea, g_pMsgAreaLine2, g_lenMsgAreaLine2);
	g_lenStoreMsgArea += g_lenMsgAreaLine2;


	iLen = sprintf(g_pTmpBuff, "\n\n\n\n\n\n\n\n\n");
	memcpy(g_pStoreMsgArea+g_lenStoreMsgArea, g_pTmpBuff, iLen);
	g_lenStoreMsgArea += iLen;

	memcpy(g_pStoreAllData, g_pStoreMsgArea, g_lenStoreMsgArea);

	if (0 != g_lenStoreDataArea) {
		memcpy(g_pStoreAllData+g_lenStoreMsgArea, g_pStoreDataArea, g_lenStoreDataArea);
	}

	printk("[focal] %s lenStoreMsgArea=%d,  lenStoreDataArea = %d\n", __func__, g_lenStoreMsgArea, g_lenStoreDataArea);
}

/************************************************************************
* Name: Save_Test_Data
* Brief:  Storage format of test data
* Input: int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount
* Output: none
* Return: none
***********************************************************************/
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount)
{
	int iLen = 0;
	int i = 0, j = 0;
	iLen = sprintf(g_pTmpBuff, "NA, %d, %d, %d, %d, %d, ", \
			m_ucTestItemCode, Row, Col, m_iStartLine, ItemCount);
	memcpy(g_pMsgAreaLine2+g_lenMsgAreaLine2, g_pTmpBuff, iLen);
	g_lenMsgAreaLine2 += iLen;

	m_iStartLine += Row;
	m_iTestDataCount++;

	for (i = 0+iArrayIndex; i < Row+iArrayIndex; i++) {
		for (j = 0; j < Col; j++) {
			if (j == (Col - 1))
				iLen = sprintf(g_pTmpBuff, "%d, \n", iData[i][j]);
			else
				iLen = sprintf(g_pTmpBuff, "%d, ", iData[i][j]);
			memcpy(g_pStoreDataArea+g_lenStoreDataArea, g_pTmpBuff, iLen);
			g_lenStoreDataArea += iLen;
		}
	}

}

/************************************************************************
* Name: GetChannelNum
* Brief:  Get Channel Num(Tx and Rx)
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetChannelNum(void)
{
	unsigned char ReCode;
	unsigned char rBuffer[1];


	ReCode = GetPanelRows(rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iTxNum = rBuffer[0];
		if (g_ScreenSetParam.iTxNum+4 > g_ScreenSetParam.iUsedMaxTxNum) {
			printk("Failed to get Tx number, Get num = %d, UsedMaxNum = %d\n",
					g_ScreenSetParam.iTxNum, g_ScreenSetParam.iUsedMaxTxNum);
			return ERROR_CODE_INVALID_PARAM;
		}

	} else {
		printk("Failed to get Tx number\n");
	}


	ReCode = GetPanelCols(rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iRxNum = rBuffer[0];
		if (g_ScreenSetParam.iRxNum > g_ScreenSetParam.iUsedMaxRxNum) {
			printk("Failed to get Rx number, Get num = %d, UsedMaxNum = %d\n",
					g_ScreenSetParam.iRxNum, g_ScreenSetParam.iUsedMaxRxNum);
			return ERROR_CODE_INVALID_PARAM;
		}

	} else {
		printk("Failed to get Rx number\n");
	}

	return ReCode;

}
/************************************************************************
* Name: GetRawData
* Brief:  Get Raw Data of MCAP
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetRawData(void)
{
	unsigned char ReCode = ERROR_CODE_OK;
	int iRow = 0;
	int iCol = 0;


	ReCode = EnterFactory();
	if (ERROR_CODE_OK != ReCode) {
		printk("Failed to Enter Factory Mode...\n");
		return ReCode;
	}



	if (0 == (g_ScreenSetParam.iTxNum + g_ScreenSetParam.iRxNum)) {
		ReCode = GetChannelNum();
		if (ERROR_CODE_OK != ReCode) {
			printk("Error Channel Num...\n");
			return ERROR_CODE_INVALID_PARAM;
		}
	}


	printk("Start Scan ...\n");
	ReCode = StartScan();
	if (ERROR_CODE_OK != ReCode) {
		printk("Failed to Scan ...\n");
		return ReCode;
	}

	memset(m_RawData, 0, sizeof(m_RawData));
	ReCode = ReadRawData(1, 0xAA, (g_ScreenSetParam.iTxNum * g_ScreenSetParam.iRxNum)*2, m_iTempRawData);
	for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
		for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
			m_RawData[iRow][iCol] = m_iTempRawData[iRow*g_ScreenSetParam.iRxNum + iCol];
		}
	}
	return ReCode;
}
/************************************************************************
* Name: ShowRawData
* Brief:  Show RawData
* Input: none
* Output: none
* Return: none.
***********************************************************************/
static void ShowRawData(void)
{
	int iRow, iCol;

	for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
		printk("\nTx%2d:  ", iRow+1);
		for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
			printk("%5d    ", m_RawData[iRow][iCol]);
		}
	}
}

/************************************************************************
* Name: GetChannelNumNoMapping
* Brief:  get Tx&Rx num from other Register
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetChannelNumNoMapping(void)
{
	unsigned char ReCode;
	unsigned char rBuffer[1];


	printk("Get Tx Num...\n");
	ReCode = ReadReg(REG_TX_NOMAPPING_NUM,  rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iTxNum = rBuffer[0];
	} else {
		printk("Failed to get Tx number\n");
	}

	printk("Get Rx Num...\n");
	ReCode = ReadReg(REG_RX_NOMAPPING_NUM,  rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iRxNum = rBuffer[0];
	} else {
		printk("Failed to get Rx number\n");
	}

	return ReCode;
}
/************************************************************************
* Name: SwitchToNoMapping
* Brief:  If it is V3 pattern, Get Tx/Rx Num again
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char SwitchToNoMapping(void)
{
	unsigned char chPattern = -1;
	unsigned char ReCode = ERROR_CODE_OK;
	unsigned char RegData = -1;
	ReCode = ReadReg(REG_PATTERN_5422, &chPattern);

	if (1 == chPattern) {
		RegData = -1;
		ReCode = ReadReg(REG_MAPPING_SWITCH, &RegData);
		if (1 != RegData) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 1);
			focal_msleep(20);
			GetChannelNumNoMapping();
		}
	}

	if (ReCode != ERROR_CODE_OK) {
		printk("Switch To NoMapping Failed!\n");
	}
	return ReCode;
}
/************************************************************************
* Name: GetTestCondition
* Brief:  Check whether Rx or TX need to test, in Waterproof ON/OFF Mode.
* Input: none
* Output: none
* Return: true: need to test; false: Not tested.
***********************************************************************/
static boolean GetTestCondition(int iTestType, unsigned char ucChannelValue)
{
	boolean bIsNeeded = false;
	switch (iTestType) {
	case WT_NeedProofOnTest:
		bIsNeeded = !(ucChannelValue & 0x20);
		break;
	case WT_NeedProofOffTest:
		bIsNeeded = !(ucChannelValue & 0x80);
		break;
	case WT_NeedTxOnVal:
		bIsNeeded = !(ucChannelValue & 0x40) || !(ucChannelValue & 0x04);
		break;
	case WT_NeedRxOnVal:
		bIsNeeded = !(ucChannelValue & 0x40) || (ucChannelValue & 0x04);
		break;
	case WT_NeedTxOffVal:
		bIsNeeded = (0x00 == (ucChannelValue & 0x03)) || (0x02 == (ucChannelValue & 0x03));
		break;
	case WT_NeedRxOffVal:
		bIsNeeded = (0x01 == (ucChannelValue & 0x03)) || (0x02 == (ucChannelValue & 0x03));
		break;
	default:
		break;
	}
	return bIsNeeded;
}

unsigned char FT5822_TestItem_UniformityTest(bool *bTestResult)
{
	unsigned char ReCode = ERROR_CODE_OK;
	bool btmpresult = true;
	unsigned char ucFre = 0;
	unsigned char FirValue = 0;

	int iMin = 100000;
	int iMax = -100000;
	int iDeviation = 0;
	int iRow = 0;
	int iCol = 0;
	int iUniform = 0;
	int index = 0;

	printk("\n\n==============================Test Item: --------RawData Uniformity Test \n\n");

	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		printk("\n\n// Failed to Enter factory Mode. Error Code: %d", ReCode);
		goto TEST_END;
	}

	ReCode =  ReadReg(REG_FREQUENCY, &ucFre);
	if (ReCode != ERROR_CODE_OK) {
		goto TEST_END;
	}

	ReCode = WriteReg(REG_FREQUENCY, 0x81);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK) {
		goto TEST_END;
	}

	ReCode = ReadReg(REG_FIR, &FirValue);
	if (ReCode != ERROR_CODE_OK) {
		goto TEST_END;
	}


	ReCode = WriteReg(REG_FIR, 0);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK) {
		goto TEST_END;
	}

	for (index = 0; index < 3; ++index) {
		ReCode = GetRawData();
	}

	if (g_stCfg_FT5822_BasicThreshold.Uniformity_CheckTx) {
		printk("\n\n=========Check Tx Linearity \n\n");

		memset(TxLinearity,  0, sizeof(TxLinearity));
		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 1; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				iDeviation = focal_abs(m_RawData[iRow][iCol] - m_RawData[iRow][iCol-1]);
				iMax = m_RawData[iRow][iCol] > m_RawData[iRow][iCol-1] ? m_RawData[iRow][iCol]:m_RawData[iRow][iCol-1] ;
				iMax = iMax ? iMax : 1;
				TxLinearity[iRow][iCol] = 100 * iDeviation / iMax;
			}
		}

		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 1; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				if (0 == g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol])
					continue;
				if (2 == g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol])
					continue;

				if (TxLinearity[iRow][iCol] < MIN_HOLE_LEVEL ||
						TxLinearity[iRow][iCol] > g_stCfg_FT5822_BasicThreshold.Uniformity_Tx_Hole) {
					printk("Tx Linearity Out Of Range, TX=%d, RX=%d, TxLinearity=%d, Tx_Hole=%d.\n",  iCol, iRow, TxLinearity[iRow][iCol], g_stCfg_FT5822_BasicThreshold.Uniformity_Tx_Hole);

					btmpresult = false;
				}
			}
		}

		Save_Test_Data(TxLinearity,  0, g_ScreenSetParam.iTxNum,  g_ScreenSetParam.iRxNum, 1);

	}

	if (g_stCfg_FT5822_BasicThreshold.Uniformity_CheckRx) {
		printk("\n\n=========Check Rx Linearity \n\n");

		for (iRow = 1; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				iDeviation = focal_abs(m_RawData[iRow][iCol] - m_RawData[iRow-1][iCol]);
				iMax = m_RawData[iRow][iCol] > m_RawData[iRow-1][iCol] ? m_RawData[iRow][iCol] : m_RawData[iRow-1][iCol] ;
				iMax = iMax ? iMax : 1;
				RxLinearity[iRow][iCol] = 100 * iDeviation / iMax;
			}
		}

		for (iRow = 1; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				if (0 == g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol])
					continue;
				if (2 == g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol])
					continue;

				if (RxLinearity[iRow][iCol] < MIN_HOLE_LEVEL ||
						RxLinearity[iRow][iCol] > g_stCfg_FT5822_BasicThreshold.Uniformity_Rx_Hole) {
					printk("Rx Linearity Out Of Range, TX=%d, RX=%d, RxLinearity=%d, Rx_Hole=%d.\n",  iCol, iRow, RxLinearity[iRow][iCol], g_stCfg_FT5822_BasicThreshold.Uniformity_Rx_Hole);

					btmpresult = false;
				}
			}
		}

		Save_Test_Data(RxLinearity,  0, g_ScreenSetParam.iTxNum,  g_ScreenSetParam.iRxNum, 2);

	}

	if (g_stCfg_FT5822_BasicThreshold.Uniformity_CheckMinMax) {
		printk("\n\n=========Check Min/Max \n\n");
		iMin = 100000;
		iMax = -100000;

		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				if (0 == g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol]) {
					continue;
				}
				if (2 == g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol]) {
					continue;
				}
				if (iMin > m_RawData[iRow][iCol])
					iMin = m_RawData[iRow][iCol] ;
				if (iMax < m_RawData[iRow][iCol])
					iMax = m_RawData[iRow][iCol] ;
			}
		}
		iMax = !iMax ? 1 : iMax;
		iUniform = 100 * focal_abs(iMin) / focal_abs(iMax);

		printk("\n\n Min: %d, Max: %d, , Get Value of Min/Max: %d.\n",  iMin, iMax, iUniform);

		if (iUniform < g_stCfg_FT5822_BasicThreshold.Uniformity_MinMax_Hole) {
			btmpresult = false;
			printk("\n\n MinMax Out Of Range, Set Value: %d.\n", g_stCfg_FT5822_BasicThreshold.Uniformity_MinMax_Hole);
		}
	}

	ReCode =  WriteReg(REG_FREQUENCY, ucFre);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK) {
		goto TEST_END;
	}
	ReCode =  WriteReg(REG_FIR, FirValue);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK) {
		goto TEST_END;
	}

TEST_END:

	if (btmpresult && ReCode == ERROR_CODE_OK) {
		*bTestResult = true;
		printk("\n\n\n\n//Uniformity Test is OK!\n\n");
	} else {
		*bTestResult = false;
		printk("\n\n\n\n//Uniformity Test is NG!\n\n");
	}
	return ReCode;
}

