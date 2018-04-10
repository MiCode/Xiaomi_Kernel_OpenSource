/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT6X36.c
*
* Author: Software Development Team, AE
*
* Created: 2015-10-08
*
* Abstract: test item for FT6X36/FT3X07/FT6416/FT6426
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
#include "Test_FT6X36.h"
#include "Config_FT6X36.h"


/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define IC_TEST_VERSION  "Test version: V1.0.0--2015-10-08, (sync version of FT_MultipleTest: V2.7.0.3--2015-07-13)"

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


#define C6208_SCAN_ADDR 0x08

#define C6X36_CHANNEL_NUM	0x0A
#define C6X36_KEY_NUM		0x0B

#define C6X36_CB_ADDR_W 0x32
#define C6X36_CB_ADDR_R 0x33
#define C6X36_CB_BUF  0x39
#define C6X36_RAWDATA_ADDR	0x34
#define C6X36_RAWDATA_BUF	0x35

#define C6206_FACTORY_TEST_MODE			0xAE
#define C6206_FACTORY_TEST_STATUS		0xAD

#define MAX_SCAP_CHANNEL_NUM		144
#define MAX_SCAP_KEY_NUM			8

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

static int m_RawData[MAX_SCAP_CHANNEL_NUM] = {0};

static unsigned char m_ucTempData[MAX_SCAP_CHANNEL_NUM*2] = {0};

static int m_CbData[MAX_SCAP_CHANNEL_NUM] = {0};
static int m_TempCbData[MAX_SCAP_CHANNEL_NUM] = {0};
static int m_DeltaCbData[MAX_SCAP_CHANNEL_NUM] = {0};
static int m_DeltaCb_DifferData[MAX_SCAP_CHANNEL_NUM] = {0};

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
static unsigned char GetPanelChannels(unsigned char *pPanelRows);
static unsigned char GetPanelKeys(unsigned char *pPanelCols);

static unsigned char GetRawData(void);
static unsigned char GetChannelNum(void);

static void InitTest(void);
static void FinishTest(void);
static void Save_Test_Data(int iData[MAX_SCAP_CHANNEL_NUM], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount);
static void InitStoreParamOfTestData(void);
static void MergeAllTestData(void);

static void AllocateMemory(void);
static void FreeMemory(void);
static void ShowRawData(void);

/************************************************************************
* Name: FT6X36_StartTest
* Brief:  Test entry. Determine which test item to test
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/
boolean FT6X36_StartTest()
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

		if (Code_FT6X36_ENTER_FACTORY_MODE == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT6X36_TestItem_EnterFactoryMode();
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
				break;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT6X36_RAWDATA_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT6X36_TestItem_RawDataTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT6X36_CB_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT6X36_TestItem_CbTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT6X36_DELTA_CB_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT6X36_TestItem_DeltaCbTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT6X36_CHANNELS_DEVIATION_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT6X36_TestItem_ChannelsDeviationTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

		if (Code_FT6X36_TWO_SIDES_DEVIATION_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
				) {
			ReCode = FT6X36_TestItem_TwoSidesDeviationTest(&bTempResult);
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
* Name: FT6X36_get_test_data
* Brief:  get data of test result
* Input: none
* Output: pTestData, the returned buff
* Return: the length of test data. if length > 0, got data;else ERR.
***********************************************************************/
int FT6X36_get_test_data(char *pTestData)
{
	if (NULL == pTestData) {
		printk("[focal] %s pTestData == NULL \n", __func__);
		return -EPERM;
	}
	memcpy(pTestData, g_pStoreAllData, (g_lenStoreMsgArea+g_lenStoreDataArea));
	return g_lenStoreMsgArea+g_lenStoreDataArea;
}

/************************************************************************
* Name: FT6X36_TestItem_EnterFactoryMode
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT6X36_TestItem_EnterFactoryMode(void)
{
	unsigned char ReCode = ERROR_CODE_INVALID_PARAM;
	int iRedo = 5;
	int i ;

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

	return ReCode;
}

/************************************************************************
* Name: GetPanelChannels(Same function name as FT_MultipleTest GetChannelNum)
* Brief:  Get row of TP
* Input: none
* Output: pPanelChannels
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetPanelChannels(unsigned char *pPanelChannels)
{
	return ReadReg(C6X36_CHANNEL_NUM, pPanelChannels);
}

/************************************************************************
* Name: GetPanelKeys(Same function name as FT_MultipleTest GetKeyNum)
* Brief:  get column of TP
* Input: none
* Output: pPanelKeys
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetPanelKeys(unsigned char *pPanelKeys)
{
	return ReadReg(C6X36_KEY_NUM, pPanelKeys);
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
	unsigned char RegVal = 0x01;
	unsigned int times = 0;
	const unsigned int MaxTimes = 500;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	ReCode = ReadReg(C6208_SCAN_ADDR, &RegVal);
	if (ReCode == ERROR_CODE_OK) {
		RegVal = 0x01;
		ReCode = WriteReg(C6208_SCAN_ADDR, RegVal);
		if (ReCode == ERROR_CODE_OK) {
			while (times++ < MaxTimes) {
				SysDelay(8);
				ReCode = ReadReg(C6208_SCAN_ADDR, &RegVal);
				if (ReCode == ERROR_CODE_OK) {
					if (RegVal == 0) {
						break;
					}
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
	unsigned short BytesNumInTestMode1 = 0;

	int i = 0;

	BytesNumInTestMode1 = ByteNum;


	I2C_wBuffer[0] = C6X36_RAWDATA_ADDR;
	I2C_wBuffer[1] = 0;
	ReCode = Comm_Base_IIC_IO(I2C_wBuffer, 2, NULL, 0);

	if ((ReCode == ERROR_CODE_OK)) {
		if (ReCode == ERROR_CODE_OK) {
			I2C_wBuffer[0] = C6X36_RAWDATA_BUF;

			ReCode = Comm_Base_IIC_IO(I2C_wBuffer, 1, m_ucTempData, BytesNumInTestMode1);

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
		g_pStoreMsgArea = kmalloc(1024*80, GFP_ATOMIC);
	g_pMsgAreaLine2 = NULL;
	if (NULL == g_pMsgAreaLine2)
		g_pMsgAreaLine2 = kmalloc(1024*80, GFP_ATOMIC);
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
static void Save_Test_Data(int iData[MAX_SCAP_CHANNEL_NUM], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount)
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
				iLen = sprintf(g_pTmpBuff, "%d, \n", iData[j]);
			else
				iLen = sprintf(g_pTmpBuff, "%d, ", iData[j]);
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


	ReCode = GetPanelChannels(rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iChannelsNum = rBuffer[0];
	} else {
		printk("Failed to get channel number\n");
	}

	ReCode = GetPanelKeys(rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iKeyNum = rBuffer[0];
	} else {
		printk("Failed to get Rx number\n");
	}

	return ReCode;

}

/************************************************************************
* Name: GetRawData
* Brief:  get panel rawdata by read rawdata function
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char GetRawData(void)
{
	int readlen = 0;
	unsigned char ReCode = ERROR_CODE_OK;
	ReCode = EnterFactory();
	if (ERROR_CODE_OK != ReCode) {
		printk("Failed to Enter Factory Mode...\n");
		return ReCode;
	}


	if (0 == (g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum)) {
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

	readlen = g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum;
	if (readlen <= 0 || readlen >= MAX_SCAP_CHANNEL_NUM)
		return ERROR_CODE_INVALID_PARAM;

	ReCode = ReadRawData(3, 0, readlen * 2, m_RawData);
	if (ReCode != ERROR_CODE_OK) {
		printk("Failed to Read RawData...\n");
		return ReCode;
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
	int iChannelsNum = 0, iKeyNum = 0;
	printk("\nChannels:  ");
	for (iChannelsNum = 0; iChannelsNum < g_ScreenSetParam.iChannelsNum; iChannelsNum++) {
		printk("%5d    ", m_RawData[iChannelsNum]);
	}
	printk("\nKeys:  ");
	for (iKeyNum = 0; iKeyNum < g_ScreenSetParam.iKeyNum; iKeyNum++) {
		printk("%5d    ", m_RawData[g_ScreenSetParam.iChannelsNum+iKeyNum]);
	}

	printk("\n\n\n\n");
}

/************************************************************************
* Name: FT6X36_TestItem_RawDataTest
* Brief:  TestItem: RawDataTest. Check if SCAP RawData is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT6X36_TestItem_RawDataTest(bool *bTestResult)
{
	int i = 0;

	unsigned char ReCode = ERROR_CODE_OK;
	bool btmpresult = true;
	int RawDataMin = 0, RawDataMax = 0;

	int iNgNum = 0;
	int iMax = 0, iMin = 0, iAvg = 0;

	RawDataMin = g_stCfg_FT6X36_BasicThreshold.RawDataTest_Min;
	RawDataMax = g_stCfg_FT6X36_BasicThreshold.RawDataTest_Max;

	printk("\r\n\r\n==============================Test Item: -------- RawData Test \r\n");

	for (i = 0; i < 3; i++) {
		ReCode = WriteReg(C6206_FACTORY_TEST_MODE, Proof_Normal);
		if (ERROR_CODE_OK == ReCode)
			ReCode = StartScan();
		if (ERROR_CODE_OK == ReCode)
			break;
	}

	ReCode = GetRawData();

	if (ReCode == ERROR_CODE_OK) {
		printk("\r\n//======= Test Data:  ");
		ShowRawData();
	} else {
		printk("\r\nRawData Test is Error. Failed to get Raw Data!!");
		btmpresult = false;
		goto TEST_END;
	}

	iNgNum = 0;
	iMax = m_RawData[0];
	iMin = m_RawData[0];
	iAvg = 0;

	for (i = 0; i < g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum; i++) {
		RawDataMin = g_stCfg_SCap_DetailThreshold.RawDataTest_Min[i];
		RawDataMax = g_stCfg_SCap_DetailThreshold.RawDataTest_Max[i];
		if (m_RawData[i] < RawDataMin || m_RawData[i] > RawDataMax) {
			btmpresult = false;

			if (iNgNum == 0)
				printk("\r\n//======= NG Data: \r\n");

			if (i < g_ScreenSetParam.iChannelsNum)
				printk("Ch_%02d: %d Set_Range=(%d, %d) ,	", i+1, m_RawData[i], RawDataMin, RawDataMax);
			else
				printk("Key_%d: %d Set_Range=(%d, %d) ,	", i+1 - g_ScreenSetParam.iChannelsNum, m_RawData[i], RawDataMin, RawDataMax);
			if (iNgNum % 6 == 0) {
				printk("\r\n");
			}
			iNgNum++;
		}

		iAvg += m_RawData[i];
		if (iMax < m_RawData[i])
			iMax = m_RawData[i];
		if (iMin > m_RawData[i])
			iMin = m_RawData[i];

	}

	iAvg /= g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum;
	printk("\r\n\r\n// Max Raw Value: %d, Min Raw Value: %d, Deviation Value: %d, Average Value: %d", iMax, iMin, iMax - iMin, iAvg);

	Save_Test_Data(m_RawData, 0, 1, g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum, 1);

TEST_END:
	if (btmpresult) {
		*bTestResult = true;
		printk("\r\n\r\n//RawData Test is OK!\r\n");
	} else {
		*bTestResult = false;
		printk("\r\n\r\n//RawData Test is NG!\r\n");
	}
	return ReCode;
}

/************************************************************************
* Name: FT6X36_TestItem_CbTest
* Brief:  TestItem: CB Test. Check if SCAP CB is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT6X36_TestItem_CbTest(bool *bTestResult)
{
	int readlen = 0;

	bool btmpresult = true;
	int iCbMin = 0, iCbMax = 0;
	unsigned char chOldMode = 0;


	unsigned char ReCode = ERROR_CODE_OK;
	BYTE pReadData[300] = {0};
	unsigned char I2C_wBuffer[1];

	int iNgNum = 0;
	int iMax = 0, iMin = 0, iAvg = 0;

	int i = 0;

	memset(m_CbData, 0, sizeof(m_CbData));
	readlen = g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum;

	printk("\r\n\r\n==============================Test Item: --------  CB Test");

	iCbMin = g_stCfg_FT6X36_BasicThreshold.CbTest_Min;
	iCbMax = g_stCfg_FT6X36_BasicThreshold.CbTest_Max;

	ReCode = ReadReg(C6206_FACTORY_TEST_MODE, &chOldMode);

	for (i = 0; i < 3; i++) {
		ReCode = WriteReg(C6206_FACTORY_TEST_MODE, Proof_NoWaterProof);
		if (ERROR_CODE_OK == ReCode)
			ReCode = StartScan();
		if (ERROR_CODE_OK == ReCode)
			break;
	}

	if ((ERROR_CODE_OK != ReCode)) {
		btmpresult = false;
		printk("\r\n\r\n//=========  CB test Failed!");
	} else {
		printk("\r\n\r\nGet Proof_NoWaterProof CB Data...");

		I2C_wBuffer[0] = 0x39;
		ReCode = WriteReg(0x33, 0);
		ReCode = Comm_Base_IIC_IO(I2C_wBuffer, 1, pReadData, readlen * 2);

		for (i = 0; i < readlen; i++) {
			m_TempCbData[i] = (unsigned short)(pReadData[i*2] << 8 | pReadData[i*2+1]);

			if (i == 0) {
				printk("\r\n\r\n//======= CB Data: ");
				printk("\r\nLeft Channel:	");
			} else if (i * 2 == g_ScreenSetParam.iChannelsNum) {
				printk("\r\nRight Channel:	");
			} else if (i ==  g_ScreenSetParam.iChannelsNum) {
				printk("\r\nKey:		");
			}
			printk("%3d	", m_TempCbData[i]);
		}
	}
	printk("\r\n\r\n");
	printk("Proof_Level0 CB Test...\r\n");
	for (i = 0; i < 3; i++) {
		ReCode = WriteReg(C6206_FACTORY_TEST_MODE, Proof_Level0);
		if (ERROR_CODE_OK == ReCode)
			ReCode = StartScan();
		if (ERROR_CODE_OK == ReCode)
			break;
	}
	if ((ERROR_CODE_OK != ReCode)) {
		btmpresult = false;
		printk("\r\n\r\n//========= CB test Failed!");
	} else {
		printk("\r\n\r\nGet Proof_Level0 CB Data...");
		I2C_wBuffer[0] = 0x39;
		ReCode = WriteReg(0x33, 0);
		ReCode = Comm_Base_IIC_IO(I2C_wBuffer, 1, pReadData, readlen * 2);

		for (i = 0; i < readlen; i++) {
			m_CbData[i] = (unsigned short)(pReadData[i*2] << 8 | pReadData[i*2+1]);
		}

		ReCode = WriteReg(C6206_FACTORY_TEST_MODE, chOldMode);
		iNgNum = 0;
		iMax = m_TempCbData[0];
		iMin = m_TempCbData[0];
		iAvg = 0;

		for (i = 0; i < g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum; i++) {

			iCbMin = g_stCfg_SCap_DetailThreshold.CbTest_Min[i];
			iCbMax = g_stCfg_SCap_DetailThreshold.CbTest_Max[i];

			if (m_TempCbData[i] < iCbMin || m_TempCbData[i] > iCbMax) {
				if (iNgNum == 0) {
					printk("\r\n//======= NG Data: \r\n");
				}
				btmpresult = false;
				if (i < g_ScreenSetParam.iChannelsNum)
					printk("Ch_%02d: %d Set_Range=(%d, %d) ,	", i+1, m_TempCbData[i], iCbMin, iCbMax);
				else
					printk("Key_%d: %d Set_Range=(%d, %d),	", i+1 - g_ScreenSetParam.iChannelsNum, m_TempCbData[i], iCbMin, iCbMax);
				if (iNgNum % 6 == 0) {
					printk("\r\n");
				}
				iNgNum++;
			}

			iAvg += m_TempCbData[i];
			if (iMax < m_TempCbData[i])
				iMax = m_TempCbData[i];
			if (iMin > m_TempCbData[i])
				iMin = m_TempCbData[i];

		}

		iAvg /= g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum;
		printk("\r\n\r\n// Max CB Value: %d, Min CB Value: %d, Deviation Value: %d, Average Value: %d", iMax, iMin, iMax - iMin, iAvg);

		if (btmpresult) {
			printk("\r\n\r\n//CB Test is OK!\r\n");
			*bTestResult = 1;
		} else {
			printk("\r\n\r\n//CB Test is NG!\r\n");
			*bTestResult = 0;
		}
	}

	Save_Test_Data(m_TempCbData, 0, 1, g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum, 1);

	return ReCode;

}

/************************************************************************
* Name: FT6X36_TestItem_DeltaCbTest
* Brief:  TestItem: Delta CB Test. Check if SCAP Delta CB is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT6X36_TestItem_DeltaCbTest(unsigned char *bTestResult)
{
	bool btmpresult = true;
	int readlen = g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum;
	int i = 0;

	int Delta_Ci_Differ = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S1;
	int Delta_Ci_Differ_S2 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S2;
	int Delta_Ci_Differ_S3 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S3;
	int Delta_Ci_Differ_S4 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S4;
	int Delta_Ci_Differ_S5 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S5;
	int Delta_Ci_Differ_S6 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S6;

	int Delta_Min = 0, Delta_Max = 0;

	int Critical_Delta_S1 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S1;
	int Critical_Delta_S2 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S2;
	int Critical_Delta_S3 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S3;
	int Critical_Delta_S4 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S4;
	int Critical_Delta_S5 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S5;
	int Critical_Delta_S6 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S6;

	bool bUseCriticalValue = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Set_Critical;

	bool bCriticalResult = false;

	int Sort1Min, Sort2Min, Sort3Min, Sort4Min, Sort5Min, Sort6Min;
	int Sort1Max, Sort2Max, Sort3Max, Sort4Max, Sort5Max, Sort6Max;
	int Sort1Min_ChNum, Sort2Min_ChNum, Sort3Min_ChNum, Sort4Min_ChNum, Sort5Min_ChNum, Sort6Min_ChNum;
	int Sort1Max_ChNum, Sort2Max_ChNum, Sort3Max_ChNum, Sort4Max_ChNum, Sort5Max_ChNum, Sort6Max_ChNum;
	bool bUseSort1 = false;
	bool bUseSort2 = false;
	bool bUseSort3 = false;
	bool bUseSort4 = false;
	bool bUseSort5 = false;
	bool bUseSort6 = false;

	int Num = 0;

	int Key_Delta_Max = 0;
	int SetKeyMax = 0;
	int set_Delta_Cb_Max = 0;

	printk("\r\n\r\n==============================Test Item: -------- Delta CB Test ");

	for (i = 0; i < readlen; i++) {
		m_DeltaCbData[i] = m_TempCbData[i] - m_CbData[i];
		if (i == 0) {
			printk("\r\n\r\n//======= Delta CB Data: ");
			printk("\r\nLeft Channel:	");
		} else if (i * 2 == g_ScreenSetParam.iChannelsNum) {
			printk("\r\nRight Channel:	");
		} else if (i ==  g_ScreenSetParam.iChannelsNum) {
			printk("\r\nKey:		");
		}
		printk("%3d	", m_DeltaCbData[i]);
	}
	printk("\r\n\r\n");
	for (i = 0; i < readlen; i++) {
		m_DeltaCb_DifferData[i] = m_DeltaCbData[i] - g_stCfg_SCap_DetailThreshold.DeltaCbTest_Base[i];
		if (i == 0) {
			printk("\r\n\r\n//======= Differ Data of Delta CB: ");
			printk("\r\nLeft Channel:	");
		} else if (i * 2 == g_ScreenSetParam.iChannelsNum) {
			printk("\r\nRight Channel:	");
		} else if (i ==  g_ScreenSetParam.iChannelsNum) {
			printk("\r\nKey:		");
		}
		printk("%3d	", m_DeltaCb_DifferData[i]);
	}
	printk("\r\n\r\n");

	Delta_Ci_Differ = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S1;
	Delta_Ci_Differ_S2 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S2;
	Delta_Ci_Differ_S3 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S3;
	Delta_Ci_Differ_S4 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S4;
	Delta_Ci_Differ_S5 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S5;
	Delta_Ci_Differ_S6 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Deviation_S6;

	Delta_Min = 0;
	Delta_Max = 0;

	Critical_Delta_S1 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S1;
	Critical_Delta_S2 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S2;
	Critical_Delta_S3 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S3;
	Critical_Delta_S4 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S4;
	Critical_Delta_S5 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S5;
	Critical_Delta_S6 = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Critical_S6;

	bUseCriticalValue = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Set_Critical;

	bCriticalResult = false;

	bUseSort1 = false;
	bUseSort2 = false;
	bUseSort3 = false;
	bUseSort4 = false;
	bUseSort5 = false;
	bUseSort6 = false;

	Num = 0;

	Sort1Min_ChNum = Sort2Min_ChNum = Sort3Min_ChNum = Sort4Min_ChNum = Sort5Min_ChNum = Sort6Min_ChNum = 0;
	Sort1Max_ChNum = Sort2Max_ChNum = Sort3Max_ChNum = Sort4Max_ChNum = Sort5Max_ChNum = Sort6Max_ChNum = 0;

	Sort1Min = Sort2Min = Sort3Min = Sort4Min = Sort5Min  = Sort6Min = 1000;
	Sort1Max = Sort2Max = Sort3Max = Sort4Max = Sort5Max = Sort6Max = -1000;

	for (i = 0; i < g_ScreenSetParam.iChannelsNum; i++) {
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 1) {
			bUseSort1 = true;
			if (m_DeltaCb_DifferData[i] < Sort1Min) {
				Sort1Min = m_DeltaCb_DifferData[i];
				Sort1Min_ChNum = i;
			}
			if (m_DeltaCb_DifferData[i] > Sort1Max) {
				Sort1Max = m_DeltaCb_DifferData[i];
				Sort1Max_ChNum = i;
			}
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 2) {
			bUseSort2 = true;
			if (m_DeltaCb_DifferData[i] < Sort2Min) {
				Sort2Min = m_DeltaCb_DifferData[i];
				Sort2Min_ChNum = i;
			}
			if (m_DeltaCb_DifferData[i] > Sort2Max) {
				Sort2Max = m_DeltaCb_DifferData[i];
				Sort2Max_ChNum = i;
			}
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 3) {
			bUseSort3 = true;
			if (m_DeltaCb_DifferData[i] < Sort3Min) {
				Sort3Min = m_DeltaCb_DifferData[i];
				Sort3Min_ChNum = i;
			}
			if (m_DeltaCb_DifferData[i] > Sort3Max) {
				Sort3Max = m_DeltaCb_DifferData[i];
				Sort3Max_ChNum = i;
			}
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 4) {
			bUseSort4 = true;
			if (m_DeltaCb_DifferData[i] < Sort4Min) {
				Sort4Min = m_DeltaCb_DifferData[i];
				Sort4Min_ChNum = i;
			}
			if (m_DeltaCb_DifferData[i] > Sort4Max) {
				Sort4Max = m_DeltaCb_DifferData[i];
				Sort4Max_ChNum = i;
			}
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 5) {
			bUseSort5 = true;
			if (m_DeltaCb_DifferData[i] < Sort5Min) {
				Sort5Min = m_DeltaCb_DifferData[i];
				Sort5Min_ChNum = i;
			}
			if (m_DeltaCb_DifferData[i] > Sort5Max) {
				Sort5Max = m_DeltaCb_DifferData[i];
				Sort5Max_ChNum = i;
			}
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 6) {
			bUseSort6 = true;
			if (m_DeltaCb_DifferData[i] < Sort6Min) {
				Sort6Min = m_DeltaCb_DifferData[i];
				Sort6Min_ChNum = i;
			}
			if (m_DeltaCb_DifferData[i] > Sort6Max) {
				Sort6Max = m_DeltaCb_DifferData[i];
				Sort6Max_ChNum = i;
			}
		}

	}
	if (bUseSort1) {
		if (Delta_Ci_Differ <= Sort1Max - Sort1Min) {
			if (bUseCriticalValue) {
				if (Sort1Max - Sort1Min >= Critical_Delta_S1) {
					btmpresult = false;
				} else {
					if (focal_abs(Sort1Max) > focal_abs(Sort1Min))
						Num = Sort1Max_ChNum;
					else
						Num = Sort1Min_ChNum;

					bCriticalResult = true;
				}
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Critical Deviation of Sort1: %d",
						Sort1Max, Sort1Min, Sort1Max - Sort1Min, Critical_Delta_S1);
			} else {
				btmpresult = false;
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort1: %d",
						Sort1Max, Sort1Min, Sort1Max - Sort1Min, Delta_Ci_Differ);
			}
		} else {
			printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort1: %d",
					Sort1Max, Sort1Min, Sort1Max - Sort1Min, Delta_Ci_Differ);
		}

		printk("\r\nMax Deviation,Sort1: %d, ", Sort1Max - Sort1Min);
	}
	if (bUseSort2) {
		if (Delta_Ci_Differ_S2 <= Sort2Max - Sort2Min) {
			if (bUseCriticalValue) {
				if (Sort2Max - Sort2Min >= Critical_Delta_S2) {
					btmpresult = false;
				} else {
					if (focal_abs(Sort2Max) > focal_abs(Sort2Min))
						Num = Sort2Max_ChNum;
					else
						Num = Sort2Min_ChNum;
					bCriticalResult = true;
				}
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Critical Deviation of Sort2: %d",
						Sort2Max, Sort2Min, Sort2Max - Sort2Min, Critical_Delta_S2);
			} else {
				btmpresult = false;
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort2: %d",
						Sort2Max, Sort2Min, Sort2Max - Sort2Min, Delta_Ci_Differ_S2);
			}
		} else {
			printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort2: %d",
					Sort2Max, Sort2Min, Sort2Max - Sort2Min, Delta_Ci_Differ_S2);
		}

		printk("\r\nSort2: %d, ", Sort2Max - Sort2Min);
	}
	if (bUseSort3) {
		if (Delta_Ci_Differ_S3 <= Sort3Max - Sort3Min) {
			if (bUseCriticalValue) {
				if (Sort3Max - Sort3Min >= Critical_Delta_S3) {
					btmpresult = false;
				} else {
					if (focal_abs(Sort3Max) > focal_abs(Sort3Min))
						Num = Sort3Max_ChNum;
					else
						Num = Sort3Min_ChNum;
					bCriticalResult = true;
				}
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Critical Deviation of Sort3: %d",
						Sort3Max, Sort3Min, Sort3Max - Sort3Min, Critical_Delta_S3);
			} else {
				btmpresult = false;
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort3: %d",
						Sort3Max, Sort3Min, Sort3Max - Sort3Min, Delta_Ci_Differ_S3);

			}
		} else {
			printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort3: %d",
					Sort3Max, Sort3Min, Sort3Max - Sort3Min, Delta_Ci_Differ_S3);

		}
		printk("\r\nSort3: %d, ", Sort3Max - Sort3Min);
	}
	if (bUseSort4) {
		if (Delta_Ci_Differ_S4 <= Sort4Max - Sort4Min) {
			if (bUseCriticalValue) {
				if (Sort4Max - Sort4Min >= Critical_Delta_S4) {
					btmpresult = false;
				} else {
					if (focal_abs(Sort4Max) > focal_abs(Sort4Min))
						Num = Sort4Max_ChNum;
					else
						Num = Sort4Min_ChNum;

					bCriticalResult = true;
				}
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Critical Deviation of Sort4: %d",
						Sort4Max, Sort4Min, Sort4Max - Sort4Min, Critical_Delta_S4);

			} else {
				btmpresult = false;
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort4: %d",
						Sort4Max, Sort4Min, Sort4Max - Sort4Min, Delta_Ci_Differ_S4);

			}
		} else {
			printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort4: %d",
					Sort4Max, Sort4Min, Sort4Max - Sort4Min, Delta_Ci_Differ_S4);

		}
		printk("\r\nSort4: %d, ", Sort4Max - Sort4Min);
	}
	if (bUseSort5) {
		if (Delta_Ci_Differ_S5 <= Sort5Max - Sort5Min) {
			if (bUseCriticalValue) {
				if (Sort5Max - Sort5Min >= Critical_Delta_S5) {
					btmpresult = false;
				} else {
					if (focal_abs(Sort5Max) > focal_abs(Sort5Min))
						Num = Sort5Max_ChNum;
					else
						Num = Sort5Min_ChNum;

					bCriticalResult = true;
				}
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Critical Deviation of Sort5: %d",
						Sort5Max, Sort5Min, Sort5Max - Sort5Min, Critical_Delta_S5);

			} else {
				btmpresult = false;
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort5: %d",
						Sort5Max, Sort5Min, Sort5Max - Sort5Min, Delta_Ci_Differ_S5);

			}
		} else {
			printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort5: %d",
					Sort5Max, Sort5Min, Sort5Max - Sort5Min, Delta_Ci_Differ_S5);

		}
		printk("\r\nSort5: %d, ", Sort5Max - Sort5Min);
	}
	if (bUseSort6) {
		if (Delta_Ci_Differ_S6 <= Sort6Max - Sort6Min) {
			if (bUseCriticalValue) {
				if (Sort6Max - Sort6Min >= Critical_Delta_S6) {
					btmpresult = false;
				} else {
					if (focal_abs(Sort6Max) > focal_abs(Sort6Min))
						Num = Sort6Max_ChNum;
					else
						Num = Sort6Min_ChNum;

					bCriticalResult = true;
				}
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Critical Deviation of Sort6: %d",
						Sort6Max, Sort6Min, Sort6Max - Sort6Min, Critical_Delta_S6);
			} else {
				btmpresult = false;
				printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort6: %d",
						Sort6Max, Sort6Min, Sort6Max - Sort6Min, Delta_Ci_Differ_S6);

			}
		} else {
			printk("\r\n\r\n// Condition1: Max of Delta CB_Differ: %d, Min of Delta CB_Differ: %d, Get Deviation: %d, Set Deviation of Sort6: %d",
					Sort6Max, Sort6Min, Sort6Max - Sort6Min, Delta_Ci_Differ_S6);

		}
		printk("\r\nSort6: %d, ", Sort6Max - Sort6Min);
	}

	Delta_Min = Delta_Max = focal_abs(m_DeltaCb_DifferData[0]);
	for (i = 1; i < g_ScreenSetParam.iChannelsNum; i++) {
		if (focal_abs(m_DeltaCb_DifferData[i]) < Delta_Min) {
			Delta_Min = focal_abs(m_DeltaCb_DifferData[i]);
		}
		if (focal_abs(m_DeltaCb_DifferData[i]) > Delta_Max) {
			Delta_Max = focal_abs(m_DeltaCb_DifferData[i]);
		}
	}

	set_Delta_Cb_Max = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Differ_Max;
	if (set_Delta_Cb_Max < focal_abs(Delta_Max)) {
		btmpresult = false;
	}
	printk("\r\n\r\n// Condition2: Get Max Differ Data of Delta_CB(abs): %d, Set Max Differ Data of Delta_CB(abs): %d", Delta_Max, set_Delta_Cb_Max);


	if (g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Include_Key_Test) {

		SetKeyMax = g_stCfg_FT6X36_BasicThreshold.DeltaCbTest_Key_Differ_Max;

		Key_Delta_Max = focal_abs(m_DeltaCb_DifferData[g_ScreenSetParam.iChannelsNum]);
		for (i = g_ScreenSetParam.iChannelsNum; i < g_ScreenSetParam.iChannelsNum + g_ScreenSetParam.iKeyNum; i++) {
			if (focal_abs(m_DeltaCb_DifferData[i]) > Key_Delta_Max) {
				Key_Delta_Max = focal_abs(m_DeltaCb_DifferData[i]);
			}
		}
		if (SetKeyMax <= Key_Delta_Max) {
			btmpresult = false;
		}
		printk("\r\n\r\n// Condition3: Include Key Test, Get Max Key Data: %d, Set Max Key Data: %d", Key_Delta_Max, SetKeyMax);
	}

	printk("\r\nMax Differ Data of Delta_CB(abs): %d ", Delta_Max);

	if (bCriticalResult && btmpresult) {
		printk("\r\n\r\nDelta CB Test has Critical Result(TBD)!");
	}
	if (btmpresult) {
		printk("\r\n\r\n//Delta CB Test is OK!\r\n");

		if (bCriticalResult)
			*bTestResult = 2;
		else
			*bTestResult = 1;
	} else {
		printk("\r\n\r\n//Delta CB Test is NG!\r\n");
		*bTestResult = 0;
	}

	Save_Test_Data(m_DeltaCbData, 0, 1, g_ScreenSetParam.iChannelsNum+g_ScreenSetParam.iKeyNum, 1);

	Save_Test_Data(m_DeltaCb_DifferData, 0, 1, g_ScreenSetParam.iChannelsNum+g_ScreenSetParam.iKeyNum, 2);

	return 0;
}

/************************************************************************
* Name: FT6X36_TestItem_ChannelsDeviationTest
* Brief:  TestItem: Channels Deviation Test. Check if Channels Deviation is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT6X36_TestItem_ChannelsDeviationTest(unsigned char *bTestResult)
{

	bool btmpresult = true;
	int i = 0;

	int DeviationMax_S1 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S1;
	int DeviationMax_S2 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S2;
	int DeviationMax_S3 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S3;
	int DeviationMax_S4 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S4;
	int DeviationMax_S5 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S5;
	int DeviationMax_S6 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S6;

	int Delta_Min = 0, Delta_Max = 0;

	int Critical_Channel_S1 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S1;
	int Critical_Channel_S2 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S2;
	int Critical_Channel_S3 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S3;
	int Critical_Channel_S4 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S4;
	int Critical_Channel_S5 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S5;
	int Critical_Channel_S6 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S6;

	bool bUseCriticalValue = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Set_Critical;

	bool bCriticalResult = false;

	int Sort1LastNum = 0xFFFF, Sort2LastNum = 0xFFFF, Sort3LastNum = 0xFFFF, Sort4LastNum = 0xFFFF, Sort5LastNum = 0xFFFF, Sort6LastNum = 0xFFFF;
	int GetDeviation;
	bool bFirstUseSort1 = true;
	bool bFirstUseSort2 = true;
	bool bFirstUseSort3 = true;
	bool bFirstUseSort4 = true;
	bool bFirstUseSort5 = true;
	bool bFirstUseSort6 = true;

	int MaxDev_AllS1 = 0, MaxDev_AllS2 = 0, MaxDev_AllS3 = 0, MaxDev_AllS4 = 0, MaxDev_AllS5 = 0, MaxDev_AllS6 = 0;

	printk("\r\n\r\n==============================Test Item: -------- Channels Deviation Test ");

	for (i = 0; i < g_ScreenSetParam.iChannelsNum; i++) {
		m_DeltaCb_DifferData[i] = m_DeltaCbData[i] - g_stCfg_SCap_DetailThreshold.DeltaCbTest_Base[i];
		if (i == 0) {
			printk("\r\n\r\n//======= Differ Data of Delta CB: ");
			printk("\r\nLeft Channel:	");
		} else if (i * 2 == g_ScreenSetParam.iChannelsNum) {
			printk("\r\nRight Channel:	");
		} else if (i ==  g_ScreenSetParam.iChannelsNum) {
			printk("\r\nKey:		");
		}
		printk("%3d	", m_DeltaCb_DifferData[i]);
	}

	printk("\r\n");

	DeviationMax_S1 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S1;
	DeviationMax_S2 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S2;
	DeviationMax_S3 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S3;
	DeviationMax_S4 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S4;
	DeviationMax_S5 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S5;
	DeviationMax_S6 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Deviation_S6;

	Delta_Min = 0;
	Delta_Max = 0;

	Critical_Channel_S1 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S1;
	Critical_Channel_S2 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S2;
	Critical_Channel_S3 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S3;
	Critical_Channel_S4 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S4;
	Critical_Channel_S5 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S5;
	Critical_Channel_S6 = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Critical_S6;

	bUseCriticalValue = g_stCfg_FT6X36_BasicThreshold.ChannelsDeviationTest_Set_Critical;

	bCriticalResult = false;

	Sort1LastNum = 0xFFFF;
	Sort2LastNum = 0xFFFF;
	Sort3LastNum = 0xFFFF;
	Sort4LastNum = 0xFFFF;
	Sort5LastNum = 0xFFFF;
	Sort6LastNum = 0xFFFF;
	GetDeviation = 0;
	bFirstUseSort1 = true;
	bFirstUseSort2 = true;
	bFirstUseSort3 = true;
	bFirstUseSort4 = true;
	bFirstUseSort5 = true;
	bFirstUseSort6 = true;

	MaxDev_AllS1 = 0;
	MaxDev_AllS2 = 0;
	MaxDev_AllS3 = 0;
	MaxDev_AllS4 = 0;
	MaxDev_AllS5 = 0;
	MaxDev_AllS6 = 0;

	for (i = 0; i < g_ScreenSetParam.iChannelsNum; i++) {
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 1) {
			if (bFirstUseSort1) {
				bFirstUseSort1 = false;
			} else {
				if (Sort1LastNum + 1 == i) {

					if (Sort1LastNum <= g_ScreenSetParam.iChannelsNum/2 - 1 && i >= g_ScreenSetParam.iChannelsNum/2) {
						Sort1LastNum = i;
						continue;
					} else if (Sort1LastNum <= g_ScreenSetParam.iChannelsNum/4 - 1 && i >= g_ScreenSetParam.iChannelsNum/4) {
						Sort1LastNum = i;
						continue;
					} else if (Sort1LastNum <= g_ScreenSetParam.iChannelsNum * 3/4 - 1 && i >= g_ScreenSetParam.iChannelsNum * 3/4) {
						Sort1LastNum = i;
						continue;
					} else {
						GetDeviation = focal_abs(m_DeltaCb_DifferData[i] - m_DeltaCb_DifferData[Sort1LastNum]);
						if (GetDeviation >= DeviationMax_S1) {
							if (bUseCriticalValue) {
								if (GetDeviation >= Critical_Channel_S1) {
									btmpresult = false;
									printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation 0f Sort1: %d",
											i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, Critical_Channel_S1);

								} else {

									bCriticalResult = true;
								}
							} else {
								btmpresult = false;
								printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation 0f Sort1: %d",
										i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, DeviationMax_S1);

							}
						}
						if (MaxDev_AllS1 < GetDeviation)
							MaxDev_AllS1 = GetDeviation;
					}
				}

			}
			Sort1LastNum = i;
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 2) {
			if (bFirstUseSort2) {
				bFirstUseSort2 = false;
			} else {
				if (Sort2LastNum + 1 == i) {
					if (Sort2LastNum <= g_ScreenSetParam.iChannelsNum/2 - 1 && i >= g_ScreenSetParam.iChannelsNum/2) {
						Sort2LastNum = i;
						continue;
					} else if (Sort2LastNum <= g_ScreenSetParam.iChannelsNum/4 - 1 && i >= g_ScreenSetParam.iChannelsNum/4) {
						Sort2LastNum = i;
						continue;
					} else if (Sort2LastNum <= g_ScreenSetParam.iChannelsNum * 3/4 - 1 && i >= g_ScreenSetParam.iChannelsNum * 3/4) {
						Sort2LastNum = i;
						continue;
					}
					GetDeviation = focal_abs(m_DeltaCb_DifferData[i] - m_DeltaCb_DifferData[Sort2LastNum]);
					if (GetDeviation >= DeviationMax_S2) {
						if (bUseCriticalValue) {
							if (GetDeviation >= Critical_Channel_S2) {
								btmpresult = false;
								printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation 0f Sort2: %d",
										i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, Critical_Channel_S2);

							} else {

								bCriticalResult = true;
							}
						} else {
							btmpresult = false;
							printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort2: %d",
									i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, DeviationMax_S2);

						}
					}
					if (MaxDev_AllS2 < GetDeviation)
						MaxDev_AllS2 = GetDeviation;
				}

			}
			Sort2LastNum = i;
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 3) {
			if (bFirstUseSort3) {
				bFirstUseSort3 = false;
			} else {
				if (Sort3LastNum + 1 == i) {
					if (Sort3LastNum <= g_ScreenSetParam.iChannelsNum/2 - 1 && i >= g_ScreenSetParam.iChannelsNum/2) {
						Sort3LastNum = i;
						continue;
					} else if (Sort3LastNum <= g_ScreenSetParam.iChannelsNum/4 - 1 && i >= g_ScreenSetParam.iChannelsNum/4) {
						Sort3LastNum = i;
						continue;
					} else if (Sort3LastNum <= g_ScreenSetParam.iChannelsNum * 3/4 - 1 && i >= g_ScreenSetParam.iChannelsNum * 3/4) {
						Sort3LastNum = i;
						continue;
					}
					GetDeviation = focal_abs(m_DeltaCb_DifferData[i] - m_DeltaCb_DifferData[Sort3LastNum]);
					if (GetDeviation >= DeviationMax_S3) {
						if (bUseCriticalValue) {
							if (GetDeviation >= Critical_Channel_S3) {
								btmpresult = false;
								printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation 0f Sort3: %d",
										i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, Critical_Channel_S3);

							} else {

								bCriticalResult = true;
							}
						} else {
							btmpresult = false;
							printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort3: %d",
									i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, DeviationMax_S3);

						}
					}
					if (MaxDev_AllS3 < GetDeviation)
						MaxDev_AllS3 = GetDeviation;
				}
			}
			Sort3LastNum = i;
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 4) {
			if (bFirstUseSort4) {
				bFirstUseSort4 = false;
			} else {
				if (Sort4LastNum + 1 == i) {
					if (Sort4LastNum <= g_ScreenSetParam.iChannelsNum/2 - 1 && i >= g_ScreenSetParam.iChannelsNum/2) {
						Sort4LastNum = i;
						continue;
					} else if (Sort4LastNum <= g_ScreenSetParam.iChannelsNum/4 - 1 && i >= g_ScreenSetParam.iChannelsNum/4) {
						Sort4LastNum = i;
						continue;
					} else if (Sort4LastNum <= g_ScreenSetParam.iChannelsNum * 3/4 - 1 && i >= g_ScreenSetParam.iChannelsNum * 3/4) {
						Sort4LastNum = i;
						continue;
					}
					GetDeviation = focal_abs(m_DeltaCb_DifferData[i] - m_DeltaCb_DifferData[Sort4LastNum]);
					if (GetDeviation >= DeviationMax_S4) {
						if (bUseCriticalValue) {
							if (GetDeviation >= Critical_Channel_S4) {
								btmpresult = false;
								printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation 0f Sort4: %d",
										i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, Critical_Channel_S4);

							} else {

								bCriticalResult = true;
							}
						} else {
							btmpresult = false;
							printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort4: %d",
									i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, DeviationMax_S4);

						}
					}
					if (MaxDev_AllS4 < GetDeviation)
						MaxDev_AllS4 = GetDeviation;
				}
			}
			Sort4LastNum = i;
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 5) {
			if (bFirstUseSort5) {
				bFirstUseSort5 = false;
			} else {
				if (Sort5LastNum + 1 == i) {
					if (Sort5LastNum <= g_ScreenSetParam.iChannelsNum/2 - 1 && i >= g_ScreenSetParam.iChannelsNum/2) {
						Sort5LastNum = i;
						continue;
					} else if (Sort5LastNum <= g_ScreenSetParam.iChannelsNum/4 - 1 && i >= g_ScreenSetParam.iChannelsNum/4) {
						Sort5LastNum = i;
						continue;
					} else if (Sort5LastNum <= g_ScreenSetParam.iChannelsNum * 3/4 - 1 && i >= g_ScreenSetParam.iChannelsNum * 3/4) {
						Sort5LastNum = i;
						continue;
					}
					GetDeviation = focal_abs(m_DeltaCb_DifferData[i] - m_DeltaCb_DifferData[Sort5LastNum]);
					if (GetDeviation >= DeviationMax_S5) {
						if (bUseCriticalValue) {
							if (GetDeviation >= Critical_Channel_S5) {
								btmpresult = false;
								printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation 0f Sort5: %d",
										i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, Critical_Channel_S5);

							} else {

								bCriticalResult = true;
							}
						} else {
							btmpresult = false;
							printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort5: %d",
									i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, DeviationMax_S5);

						}
					}
					if (MaxDev_AllS5 < GetDeviation)
						MaxDev_AllS5 = GetDeviation;
				}
			}
			Sort5LastNum = i;
		}

		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 6) {
			if (bFirstUseSort6) {
				bFirstUseSort6 = false;
			} else {
				if (Sort6LastNum + 1 == i) {
					if (Sort6LastNum <= g_ScreenSetParam.iChannelsNum/2 - 1 && i >= g_ScreenSetParam.iChannelsNum/2) {
						Sort6LastNum = i;
						continue;
					} else if (Sort6LastNum <= g_ScreenSetParam.iChannelsNum/4 - 1 && i >= g_ScreenSetParam.iChannelsNum/4) {
						Sort6LastNum = i;
						continue;
					} else if (Sort6LastNum <= g_ScreenSetParam.iChannelsNum * 3/4 - 1 && i >= g_ScreenSetParam.iChannelsNum * 3/4) {
						Sort6LastNum = i;
						continue;
					}
					GetDeviation = focal_abs(m_DeltaCb_DifferData[i] - m_DeltaCb_DifferData[Sort6LastNum]);
					if (GetDeviation >= DeviationMax_S6) {
						if (bUseCriticalValue) {
							if (GetDeviation >= Critical_Channel_S6) {
								btmpresult = false;
								printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation 0f Sort6: %d",
										i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, Critical_Channel_S6);

							} else {

								bCriticalResult = true;
							}
						} else {
							btmpresult = false;
							printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort6: %d",
									i-1 + 1, m_DeltaCb_DifferData[i-1], i + 1, m_DeltaCb_DifferData[i], GetDeviation, DeviationMax_S6);

						}
					}
					if (MaxDev_AllS6 < GetDeviation)
						MaxDev_AllS6 = GetDeviation;
				}
			}
			Sort6LastNum = i;
		}
	}
	if (!bFirstUseSort1) {
		printk("\r\n\r\nGet max deviation of Sort1: %d,  Set max deviation of Sort1: %d", MaxDev_AllS1, DeviationMax_S1);

	}
	if (!bFirstUseSort2) {
		printk("\r\n\r\nGet max deviation of Sort2: %d,  Set max deviation of Sort2: %d", MaxDev_AllS2, DeviationMax_S2);

	}
	if (!bFirstUseSort3) {
		printk("\r\n\r\nGet max deviation of Sort3: %d,  Set max deviation of Sort3: %d", MaxDev_AllS3, DeviationMax_S3);

	}
	if (!bFirstUseSort4) {
		printk("\r\n\r\nGet max deviation of Sort4: %d,  Set max deviation of Sort4: %d", MaxDev_AllS4, DeviationMax_S4);

	}
	if (!bFirstUseSort5) {
		printk("\r\n\r\nGet max deviation of Sort5: %d,  Set max deviation of Sort5: %d", MaxDev_AllS5, DeviationMax_S5);

	}
	if (!bFirstUseSort6) {
		printk("\r\n\r\nGet max deviation of Sort6: %d,  Set max deviation of Sort6: %d", MaxDev_AllS6, DeviationMax_S6);

	}

	printk("\r\n\r\nMax Deviation, ");
	if (!bFirstUseSort1) {
		printk("Sort1: %d, ", MaxDev_AllS1);
	}
	if (!bFirstUseSort2) {
		printk("Sort2: %d, ", MaxDev_AllS2);
	}
	if (!bFirstUseSort3) {
		printk("Sort3: %d, ", MaxDev_AllS3);
	}
	if (!bFirstUseSort4) {
		printk("Sort4: %d, ", MaxDev_AllS4);
	}
	if (!bFirstUseSort5) {
		printk("Sort5: %d, ", MaxDev_AllS5);
	}
	if (!bFirstUseSort6) {
		printk("Sort6: %d, ", MaxDev_AllS6);
	}

	if (bCriticalResult && btmpresult) {
		printk("\r\n\r\nChannels Deviation Test has Critical Result(TBD)!");
	}
	if (btmpresult) {
		printk("\r\n\r\n//Channels Deviation Test is OK!\r\n");
		*bTestResult = true;
		if (bCriticalResult)
			*bTestResult = 2;
		else
			*bTestResult = true;
	} else {
		*bTestResult = false;
		printk("\r\n\r\n//Channels Deviation Test is NG!\r\n");
	}

	return 0;
}

/************************************************************************
* Name: FT6X36_TestItem_TwoSidesDeviationTest
* Brief:  TestItem: Two Sides Deviation Test. Check if Two Sides Deviation is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT6X36_TestItem_TwoSidesDeviationTest(unsigned char *bTestResult)
{

	bool btmpresult = true;
	int i = 0;

	int DeviationMax = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S1;
	int DeviationMax_S2 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S2;
	int DeviationMax_S3 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S3;
	int DeviationMax_S4 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S4;
	int DeviationMax_S5 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S5;
	int DeviationMax_S6 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S6;

	int Critical_TwoSides_S1 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S1;
	int Critical_TwoSides_S2 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S2;
	int Critical_TwoSides_S3 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S3;
	int Critical_TwoSides_S4 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S4;
	int Critical_TwoSides_S5 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S5;
	int Critical_TwoSides_S6 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S6;

	bool bUseCriticalValue = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Set_Critical;

	bool bCriticalResult = false;


	bool bUseSort1 = false;
	bool bUseSort2 = false;
	bool bUseSort3 = false;
	bool bUseSort4 = false;
	bool bUseSort5 = false;
	bool bUseSort6 = false;

	int DevMax_AllS1 = 0, DevMax_AllS2 = 0, DevMax_AllS3 = 0, DevMax_AllS4 = 0, DevMax_AllS5 = 0, DevMax_AllS6 = 0;

	int GetDeviation = 0;

	printk("\r\n\r\n==============================Test Item: -------- Two Sides Deviation Test ");


	for (i = 0; i < g_ScreenSetParam.iChannelsNum; i++) {
		m_DeltaCb_DifferData[i] = m_DeltaCbData[i] - g_stCfg_SCap_DetailThreshold.DeltaCbTest_Base[i];
		if (i == 0) {
			printk("\r\n\r\n//======= Differ Data of Delta CB: ");
			printk("\r\nLeft Channel:	");
		} else if (i * 2 == g_ScreenSetParam.iChannelsNum) {
			printk("\r\nRight Channel:	");
		} else if (i ==  g_ScreenSetParam.iChannelsNum) {
			printk("\r\nKey:		");
		}
		printk("%3d	", m_DeltaCb_DifferData[i]);
	}

	printk("\r\n");

	DeviationMax = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S1;
	DeviationMax_S2 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S2;
	DeviationMax_S3 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S3;
	DeviationMax_S4 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S4;
	DeviationMax_S5 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S5;
	DeviationMax_S6 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Deviation_S6;

	Critical_TwoSides_S1 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S1;
	Critical_TwoSides_S2 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S2;
	Critical_TwoSides_S3 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S3;
	Critical_TwoSides_S4 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S4;
	Critical_TwoSides_S5 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S5;
	Critical_TwoSides_S6 = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Critical_S6;

	bUseCriticalValue = g_stCfg_FT6X36_BasicThreshold.TwoSidesDeviationTest_Set_Critical;

	bCriticalResult = false;

	bUseSort1 = false;
	bUseSort2 = false;
	bUseSort3 = false;
	bUseSort4 = false;
	bUseSort5 = false;
	bUseSort6 = false;

	DevMax_AllS1 = 0;
	DevMax_AllS2 = 0;
	DevMax_AllS3 = 0;
	DevMax_AllS4 = 0;
	DevMax_AllS5 = 0;
	DevMax_AllS6 = 0;

	GetDeviation = 0;

	for (i = 0; i < g_ScreenSetParam.iChannelsNum/2; i++) {
		GetDeviation = abs(m_DeltaCb_DifferData[i] - m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2]);
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 1) {
			bUseSort1 = true;
			if (GetDeviation >= DeviationMax) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S1) {
						btmpresult = false;
						printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation of Sort1: %d",
								i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, Critical_TwoSides_S1);

					} else {

						bCriticalResult = true;
					}
				} else {
					btmpresult = false;
					printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort1: %d",
							i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, DeviationMax);

				}
			}
			if (DevMax_AllS1 < GetDeviation)
				DevMax_AllS1 = GetDeviation;
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 2) {
			bUseSort2 = true;
			if (GetDeviation >= DeviationMax_S2) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S2) {
						btmpresult = false;
						printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation of Sort2: %d",
								i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, Critical_TwoSides_S2);

					} else {

						bCriticalResult = true;
					}
				} else {
					btmpresult = false;
					printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort2: %d",
							i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, DeviationMax_S2);

				}
			}
			if (DevMax_AllS2 < GetDeviation)
				DevMax_AllS2 = GetDeviation;
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 3) {
			bUseSort3 = true;
			if (GetDeviation >= DeviationMax_S3) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S3) {
						btmpresult = false;
						printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation of Sort3: %d",
								i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, Critical_TwoSides_S3);

					} else {

						bCriticalResult = true;
					}
				} else {
					btmpresult = false;
					printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort3: %d",
							i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, DeviationMax_S3);

				}
			}
			if (DevMax_AllS3 < GetDeviation)
				DevMax_AllS3 = GetDeviation;
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 4) {
			bUseSort4 = true;
			if (GetDeviation >= DeviationMax_S4) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S4) {
						btmpresult = false;
						printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation of Sort4: %d",
								i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, Critical_TwoSides_S4);

					} else {

						bCriticalResult = true;
					}
				} else {
					btmpresult = false;
					printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort4: %d",
							i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, DeviationMax_S4);

				}
			}
			if (DevMax_AllS4 < GetDeviation)
				DevMax_AllS4 = GetDeviation;
		}
		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 5) {
			bUseSort5 = true;
			if (GetDeviation >= DeviationMax_S5) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S5) {
						btmpresult = false;
						printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation of Sort5: %d",
								i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, Critical_TwoSides_S5);

					} else {

						bCriticalResult = true;
					}
				} else {
					btmpresult = false;
					printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort5: %d",
							i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, DeviationMax_S5);

				}
			}
			if (DevMax_AllS5 < GetDeviation)
				DevMax_AllS5 = GetDeviation;
		}


		if (g_stCfg_SCap_DetailThreshold.DeltaCxTest_Sort[i] == 6) {
			bUseSort6 = true;
			if (GetDeviation >= DeviationMax_S6) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S6) {
						btmpresult = false;
						printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Critical Deviation of Sort6: %d",
								i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, Critical_TwoSides_S6);

					} else {

						bCriticalResult = true;
					}
				} else {
					btmpresult = false;
					printk("\r\nCh_%2d, Value: %2d,	Ch_%2d, Value: %2d,	Deviation: %d,	Set Max Deviation of Sort6: %d",
							i + 1, m_DeltaCb_DifferData[i], i + g_ScreenSetParam.iChannelsNum/2, m_DeltaCb_DifferData[i + g_ScreenSetParam.iChannelsNum/2], GetDeviation, DeviationMax_S6);

				}
			}
			if (DevMax_AllS6 < GetDeviation)
				DevMax_AllS6 = GetDeviation;
		}

	}
	if (bUseSort1) {
		printk("\r\n\r\nGet Max Deviation of Sort1: %d,  Set Max Deviation of Sort1: %d", DevMax_AllS1, DeviationMax);

	}
	if (bUseSort2) {
		printk("\r\n\r\nGet Max Deviation of Sort2: %d,  Set Max Deviation of Sort2: %d", DevMax_AllS2, DeviationMax_S2);

	}
	if (bUseSort3) {
		printk("\r\n\r\nGet Max Deviation of Sort3: %d,  Set Max Deviation of Sort3: %d", DevMax_AllS3, DeviationMax_S3);

	}
	if (bUseSort4) {
		printk("\r\n\r\nGet Max Deviation of Sort4: %d,  Set Max Deviation of Sort4: %d", DevMax_AllS4, DeviationMax_S4);

	}
	if (bUseSort5) {
		printk("\r\n\r\nGet Max Deviation of Sort5: %d,  Set Max Deviation of Sort5: %d", DevMax_AllS5, DeviationMax_S5);

	}
	if (bUseSort6) {
		printk("\r\n\r\nGet Max Deviation of Sort6: %d,  Set Max Deviation of Sort6: %d", DevMax_AllS6, DeviationMax_S6);

	}

	printk("\r\nMax Deviation, ");
	if (bUseSort1) {
		printk("Sort1: %d, ", DevMax_AllS1);

	}
	if (bUseSort2) {
		printk("Sort2: %d, ", DevMax_AllS2);

	}
	if (bUseSort3) {
		printk("Sort3: %d, ", DevMax_AllS3);

	}
	if (bUseSort4) {
		printk("Sort4: %d, ", DevMax_AllS4);

	}
	if (bUseSort5) {
		printk("Sort5: %d, ", DevMax_AllS5);

	}
	if (bUseSort6) {
		printk("Sort6: %d, ", DevMax_AllS6);

	}

	if (bCriticalResult && btmpresult) {
		printk("\r\n\r\nTwo Sides Deviation Test has Critical Result(TBD)!");
	}
	if (btmpresult) {
		printk("\r\n\r\n//Two Sides Deviation Test is OK!\r\n");
		if (bCriticalResult)
			*bTestResult = 2;
		else
			*bTestResult = true;
	} else {
		*bTestResult = false;
		printk("\r\n\r\n//Two Sides Deviation Test is NG!\r\n");
	}

	return 0;
}
