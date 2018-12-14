/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)，All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT8606.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: test item for FT8606
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "Global.h"
#include "Test_FT8606.h"
#include "DetailThreshold.h"
#include "Config_FT8606.h"


/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define IC_TEST_VERSION  "Test version: V1.0.0--2015-07-30, (sync version of FT_MultipleTest: V2.7.0.3--2015-07-13)"

#define MAX_NOISE_FRAMES    32

#define DEVIDE_MODE_ADDR	0x00
#define REG_LINE_NUM	0x01
#define REG_TX_NUM	0x02
#define REG_RX_NUM	0x03
#define FT_8606_LEFT_KEY_REG    0X1E
#define FT_8606_RIGHT_KEY_REG   0X1F

#define REG_CbAddrH  		0x18
#define REG_CbAddrL			0x19
#define REG_OrderAddrH		0x1A
#define REG_OrderAddrL		0x1B

#define REG_RawBuf0			0x6A
#define REG_RawBuf1			0x6B
#define REG_OrderBuf0		0x6C
#define REG_CbBuf0			0x6E

#define REG_K1Delay			0x31
#define REG_K2Delay			0x32
#define REG_SCChannelCf		0x34

#define pre 1


/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
struct structSCapConfEx
{
	unsigned char ChannelXNum;
	unsigned char ChannelYNum;
	unsigned char KeyNum;
	unsigned char KeyNumTotal;
	bool bLeftKey1;
	bool bLeftKey2;
	bool bLeftKey3;
	bool bRightKey1;
	bool bRightKey2;
	bool bRightKey3;
};
struct structSCapConfEx g_stSCapConfEx;

enum NOISE_TYPE
{
	NT_AvgData = 0,
	NT_MaxData = 1,
	NT_MaxDevication = 2,
	NT_DifferData = 3,
};

/*******************************************************************************
* Static variables
*******************************************************************************/

static int m_RawData[TX_NUM_MAX][RX_NUM_MAX] = {{0,0}};
static int m_NoiseData[TX_NUM_MAX][RX_NUM_MAX] = {{0,0}};
static int m_CBData[TX_NUM_MAX][RX_NUM_MAX] = {{0,0}};
static int m_AvgData[TX_NUM_MAX][RX_NUM_MAX] = {{0,0}};
static int m_iTempData[TX_NUM_MAX][RX_NUM_MAX] = {{0,0}};
static BYTE m_ucTempData[TX_NUM_MAX * RX_NUM_MAX*2] = {0};
static int m_iTempRawData[TX_NUM_MAX * RX_NUM_MAX] = {0};
static int m_TempNoiseData[MAX_NOISE_FRAMES][RX_NUM_MAX * TX_NUM_MAX] = {{0,0}};



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
static unsigned char GetTxRxCB(unsigned short StartNodeNo, unsigned short ReadNum, unsigned char *pReadBuffer);

static unsigned char GetRawData(void);
static unsigned char GetChannelNum(void);


static void InitTest(void);
static void FinishTest(void);
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount);
static void InitStoreParamOfTestData(void);
static void MergeAllTestData(void);

static void AllocateMemory(void);
static void FreeMemory(void);
static unsigned int SqrtNew(unsigned int n) ;

/************************************************************************
* Name: FT5X46_StartTest
* Brief:  Test entry. Determine which test item to test
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/
boolean FT8606_StartTest()
{
	bool bTestResult = true, bTempResult = 1;
	unsigned char ReCode;
	unsigned char ucDevice = 0;
	int iItemCount = 0;



	InitTest();


	if (0 == g_TestItemNum)
		bTestResult = false;

	测试过程，即是顺序执行g_stTestItem结构体的测试项
	for (iItemCount = 0; iItemCount < g_TestItemNum; iItemCount++)
	{
		m_ucTestItemCode = g_stTestItem[ucDevice][iItemCount].ItemCode;


		if (Code_FT8606_ENTER_FACTORY_MODE == g_stTestItem[ucDevice][iItemCount].ItemCode
			)
		{
			ReCode = FT8606_TestItem_EnterFactoryMode();
			if (ERROR_CODE_OK != ReCode || (!bTempResult))
			{
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
				break;
			}
			else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8606_CHANNEL_NUM_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
			)
		{
			ReCode = FT8606_TestItem_ChannelsTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult))
			{
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
				break;
			}
			else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;

		}




		if (Code_FT8606_RAWDATA_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
			)
		{
			ReCode = FT8606_TestItem_RawDataTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult))
			{
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			}
			else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}



		if (Code_FT8606_NOISE_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
			)
		{

			ReCode = FT8606_TestItem_NoiseTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult))
			{
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			}
			else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}




		if (Code_FT8606_CB_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
			)
		{
			ReCode = FT8606_TestItem_CbTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult))
			{
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			}
			else
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

	g_stSCapConfEx.ChannelXNum = 0;
	g_stSCapConfEx.ChannelYNum = 0;
	g_stSCapConfEx.KeyNum = 0;
	g_stSCapConfEx.KeyNumTotal = 6;

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
* Name: FT8606_get_test_data
* Brief:  get data of test result
* Input: none
* Output: pTestData, the returned buff
* Return: the length of test data. if length > 0, got data;else ERR.
***********************************************************************/
int FT8606_get_test_data(char *pTestData)
{
	if (NULL == pTestData)
	{
		printk("[focal] %s pTestData == NULL \n", __func__);
		return -EPERM;
	}
	memcpy(pTestData, g_pStoreAllData, (g_lenStoreMsgArea+g_lenStoreDataArea));
	return (g_lenStoreMsgArea+g_lenStoreDataArea);
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
	/*g_pStoreAllData =NULL;
	if(NULL == g_pStoreAllData)
	g_pStoreAllData = kmalloc(1024*8, GFP_ATOMIC);
	g_pTmpBuff =NULL;*/
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

	/*if(NULL == g_pStoreAllData)
	kfree(g_pStoreAllData);*/

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

	g_lenStoreMsgArea += sprintf(g_pStoreMsgArea,"ECC, 85, 170, IC Name, %s, IC Code, %x\n", g_strIcName,  g_ScreenSetParam.iSelectedIC);



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


	iLen = sprintf(g_pTmpBuff,"TestItem, %d, ", m_iTestDataCount);
	memcpy(g_pStoreMsgArea+g_lenStoreMsgArea, g_pTmpBuff, iLen);
	g_lenStoreMsgArea+=iLen;


	memcpy(g_pStoreMsgArea+g_lenStoreMsgArea, g_pMsgAreaLine2, g_lenMsgAreaLine2);
	g_lenStoreMsgArea+=g_lenMsgAreaLine2;


	iLen = sprintf(g_pTmpBuff,"\n\n\n\n\n\n\n\n\n");
	memcpy(g_pStoreMsgArea+g_lenStoreMsgArea, g_pTmpBuff, iLen);
	g_lenStoreMsgArea+=iLen;


	memcpy(g_pStoreAllData, g_pStoreMsgArea, g_lenStoreMsgArea);


	if (0 != g_lenStoreDataArea)
	{
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


	iLen = sprintf(g_pTmpBuff,"NA, %d, %d, %d, %d, %d, ", \
		m_ucTestItemCode, Row, Col, m_iStartLine, ItemCount);
	memcpy(g_pMsgAreaLine2+g_lenMsgAreaLine2, g_pTmpBuff, iLen);
	g_lenMsgAreaLine2 += iLen;

	m_iStartLine += Row;
	m_iTestDataCount++;


	for (i = 0+iArrayIndex; i < Row+iArrayIndex; i++)
	{
		for (j = 0; j < Col; j++)
		{
			if (j == (Col -1))
				iLen = sprintf(g_pTmpBuff,"%d, \n", iData[i][j]);
			else
				iLen = sprintf(g_pTmpBuff,"%d, ", iData[i][j]);

			memcpy(g_pStoreDataArea+g_lenStoreDataArea, g_pTmpBuff, iLen);
			g_lenStoreDataArea += iLen;
		}
	}

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
	unsigned char RegVal = 0x00;
	unsigned char times = 0;
	const unsigned char MaxTimes = 20;	最长等待160ms
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;



	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
	if (ReCode == ERROR_CODE_OK)
	{
		RegVal |= 0x80;		最高位置1，启动扫描
		ReCode = WriteReg(DEVIDE_MODE_ADDR,RegVal);
		if (ReCode == ERROR_CODE_OK)
		{
			while(times++ < MaxTimes)		等待扫描完成
			{
				SysDelay(8);
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
				if (ReCode == ERROR_CODE_OK)
				{
					if ((RegVal>>7) == 0)	break;
				}
				else
				{
					break;
				}
			}
			if (times < MaxTimes)	ReCode = ERROR_CODE_OK;
			else ReCode = ERROR_CODE_COMM_ERROR;
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
static unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	unsigned char I2C_wBuffer[3] = {0};
	unsigned char pReadData[ByteNum];

	int i, iReadNum;
	unsigned short BytesNumInTestMode1 = 0;

	iReadNum = ByteNum/342;

	if (0 != (ByteNum%342)) iReadNum++;

	if (ByteNum <= 342)
	{
		BytesNumInTestMode1 = ByteNum;
	}
	else
	{
		BytesNumInTestMode1 = 342;
	}

	ReCode = WriteReg(REG_LINE_NUM, LineNum);



	I2C_wBuffer[0] = REG_RawBuf0;
	if (ReCode == ERROR_CODE_OK)
	{
		focal_msleep(10);
		ReCode = Comm_Base_IIC_IO(I2C_wBuffer, 1, pReadData, BytesNumInTestMode1);
	}

	for (i = 1; i < iReadNum; i++)
	{
		if (ReCode != ERROR_CODE_OK) break;

		if (i == iReadNum-1)
		{
			focal_msleep(10);
			ReCode = Comm_Base_IIC_IO(NULL, 0, pReadData+342*i, ByteNum-342*i);
		}
		else
		{
			focal_msleep(10);
			ReCode = Comm_Base_IIC_IO(NULL, 0, pReadData+342*i, 342);
		}

	}

	if (ReCode == ERROR_CODE_OK)
	{
		for (i = 0; i < (ByteNum>>1); i++)
		{
			pRevBuffer[i] = (pReadData[i<<1]<<8)+pReadData[(i<<1)+1];
			有符号位



		}
	}


	return ReCode;

}
/************************************************************************
* Name: GetTxRxCB(Same function name as FT_MultipleTest)
* Brief:  get CB of Tx/Rx
* Input: StartNodeNo, ReadNum
* Output: pReadBuffer
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetTxRxCB(unsigned short StartNodeNo, unsigned short ReadNum, unsigned char *pReadBuffer)
{
	unsigned char ReCode = ERROR_CODE_OK;
	unsigned short usReturnNum = 0;次要返回的个数
	unsigned short usTotalReturnNum = 0;总返回个数
	unsigned char wBuffer[4];
	int i, iReadNum;

	iReadNum = ReadNum/342;

	if (0 != (ReadNum%342)) iReadNum++;

	wBuffer[0] = REG_CbBuf0;

	usTotalReturnNum = 0;

	for (i = 1; i <= iReadNum; i++)
	{
		if (i*342 > ReadNum)
			usReturnNum = ReadNum - (i-1)*342;
		else
			usReturnNum = 342;

		wBuffer[1] = (StartNodeNo+usTotalReturnNum) >>8;地址偏移量高8位
		wBuffer[2] = (StartNodeNo+usTotalReturnNum)&0xff;地址偏移量低8位

		ReCode = WriteReg(REG_CbAddrH, wBuffer[1]);
		ReCode = WriteReg(REG_CbAddrL, wBuffer[2]);

		ReCode = Comm_Base_IIC_IO(wBuffer, 1, pReadBuffer+usTotalReturnNum, usReturnNum);

		usTotalReturnNum += usReturnNum;

		if (ReCode != ERROR_CODE_OK)return ReCode;


	}

	return ReCode;
}


获取PanelRows

static unsigned char GetPanelRows(unsigned char *pPanelRows)
{
	return ReadReg(REG_TX_NUM, pPanelRows);
}


获取PanelCols

static unsigned char GetPanelCols(unsigned char *pPanelCols)
{
	return ReadReg(REG_RX_NUM, pPanelCols);
}






/************************************************************************
* Name: FT8606_TestItem_EnterFactoryMode
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8606_TestItem_EnterFactoryMode(void)
{

	unsigned char ReCode = ERROR_CODE_INVALID_PARAM;
	int iRedo = 5;	如果不成功，重复进入5次
	int i ;
	SysDelay(150);
	printk("Enter factory mode...\n");
	for (i = 1; i <= iRedo; i++)
	{
		ReCode = EnterFactory();
		if (ERROR_CODE_OK != ReCode)
		{
			printk("Failed to Enter factory mode...\n");
			if (i < iRedo)
			{
				SysDelay(50);
				continue;
			}
		}
		else
		{

		}

	}
	SysDelay(300);

	if (ReCode == ERROR_CODE_OK)	进工厂模式成功后，就读出通道数
	{
		ReCode = GetChannelNum();
	}
	return ReCode;
}
/************************************************************************
* Name: GetChannelNum
* Brief:  Get Num of Ch_X, Ch_Y and key
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetChannelNum(void)
{
	unsigned char ReCode;

	int i ;
	unsigned char rBuffer[1];



	for (i = 0; i < 3; i++)
	{
		ReCode = GetPanelRows(rBuffer);
		if (ReCode == ERROR_CODE_OK)
		{
			if (0 < rBuffer[0] && rBuffer[0] < 80)
			{
				g_stSCapConfEx.ChannelXNum = rBuffer[0];
				if (g_stSCapConfEx.ChannelXNum > g_ScreenSetParam.iUsedMaxTxNum)
				{
					printk("Failed to get Channel X number, Get num = %d, UsedMaxNum = %d\n",
						g_stSCapConfEx.ChannelXNum, g_ScreenSetParam.iUsedMaxTxNum);
					return ERROR_CODE_INVALID_PARAM;
				}
				break;
			}
			else
			{
				SysDelay(150);
				continue;
			}
		}
		else
		{
			printk("Failed to get Channel X number\n");
			SysDelay(150);
		}
	}


	for (i = 0; i < 3; i++)
	{
		ReCode = GetPanelCols(rBuffer);
		if (ReCode == ERROR_CODE_OK)
		{
			if (0 < rBuffer[0] && rBuffer[0] < 80)
			{
				g_stSCapConfEx.ChannelYNum = rBuffer[0];
				if (g_stSCapConfEx.ChannelYNum > g_ScreenSetParam.iUsedMaxRxNum)
				{
					printk("Failed to get Channel Y number, Get num = %d, UsedMaxNum = %d\n",
						g_stSCapConfEx.ChannelYNum, g_ScreenSetParam.iUsedMaxRxNum);
					return ERROR_CODE_INVALID_PARAM;
				}
				break;
			}
			else
			{
				SysDelay(150);
				continue;
			}
		}
		else
		{
			printk("Failed to get Channel Y number\n");
			SysDelay(150);
		}
	}


	for (i = 0; i < 3; i++)
	{
		unsigned char regData = 0;
		g_stSCapConfEx.KeyNum = 0;
		ReCode = ReadReg(FT_8606_LEFT_KEY_REG, &regData);
		if (ReCode == ERROR_CODE_OK)
		{
			if (((regData >> 0) & 0x01)) { g_stSCapConfEx.bLeftKey1 = true; ++g_stSCapConfEx.KeyNum; }
			if (((regData >> 1) & 0x01)) { g_stSCapConfEx.bLeftKey2 = true; ++g_stSCapConfEx.KeyNum; }
			if (((regData >> 2) & 0x01)) { g_stSCapConfEx.bLeftKey3 = true; ++g_stSCapConfEx.KeyNum; }
		}
		else
		{
			printk("Failed to get Key number\n");
			SysDelay(150);
			continue;
		}
		ReCode = ReadReg(FT_8606_RIGHT_KEY_REG, &regData);
		if (ReCode == ERROR_CODE_OK)
		{
			if (((regData >> 0) & 0x01)) {g_stSCapConfEx.bRightKey1 = true; ++g_stSCapConfEx.KeyNum; }
			if (((regData >> 1) & 0x01)) {g_stSCapConfEx.bRightKey2 = true; ++g_stSCapConfEx.KeyNum; }
			if (((regData >> 2) & 0x01)) {g_stSCapConfEx.bRightKey3 = true; ++g_stSCapConfEx.KeyNum; }
			break;
		}
		else
		{
			printk("Failed to get Key number\n");
			SysDelay(150);
			continue;
		}
	}



	printk("CH_X = %d, CH_Y = %d, Key = %d\n", g_stSCapConfEx.ChannelXNum ,g_stSCapConfEx.ChannelYNum, g_stSCapConfEx.KeyNum);
	return ReCode;
}
/************************************************************************
* Name: FT8606_TestItem_ChannelsTest
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8606_TestItem_ChannelsTest(bool *bTestResult)
{
	unsigned char ReCode;

	printk("\n\n==============================Test Item: -------- Channel Test \n");

	ReCode = GetChannelNum();
	if (ReCode == ERROR_CODE_OK)
	{
		if ((g_stCfg_FT8606_BasicThreshold.ChannelNumTest_ChannelXNum == g_stSCapConfEx.ChannelXNum)
			&& (g_stCfg_FT8606_BasicThreshold.ChannelNumTest_ChannelYNum == g_stSCapConfEx.ChannelYNum)
			&& (g_stCfg_FT8606_BasicThreshold.ChannelNumTest_KeyNum == g_stSCapConfEx.KeyNum))
		{
			* bTestResult = true;
			printk("\n\nGet channels: (CHx: %d, CHy: %d, Key: %d), Set channels: (CHx: %d, CHy: %d, Key: %d)",
				g_stSCapConfEx.ChannelXNum, g_stSCapConfEx.ChannelYNum, g_stSCapConfEx.KeyNum,
				g_stCfg_FT8606_BasicThreshold.ChannelNumTest_ChannelXNum, g_stCfg_FT8606_BasicThreshold.ChannelNumTest_ChannelYNum, g_stCfg_FT8606_BasicThreshold.ChannelNumTest_KeyNum);

			printk("\n//Channel Test is OK!");
		}
		else
		{
			* bTestResult = false;
			printk("\n\nGet channels: (CHx: %d, CHy: %d, Key: %d), Set channels: (CHx: %d, CHy: %d, Key: %d)",
				g_stSCapConfEx.ChannelXNum, g_stSCapConfEx.ChannelYNum, g_stSCapConfEx.KeyNum,
				g_stCfg_FT8606_BasicThreshold.ChannelNumTest_ChannelXNum, g_stCfg_FT8606_BasicThreshold.ChannelNumTest_ChannelYNum, g_stCfg_FT8606_BasicThreshold.ChannelNumTest_KeyNum);

			printk("\n//Channel Test is NG!");
		}
	}
	return ReCode;
}
/************************************************************************
* Name: GetRawData
* Brief:  Get Raw Data of VA area and Key area
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetRawData(void)
{
	int ReCode = ERROR_CODE_OK;
	int iRow, iCol;


	ReCode = EnterFactory();
	if (ERROR_CODE_OK != ReCode)
	{
		printk("Failed to Enter Factory Mode...\n");
		return ReCode;
	}



	if (0 == (g_stSCapConfEx.ChannelXNum + g_stSCapConfEx.ChannelYNum))
	{
		ReCode = GetChannelNum();
		if (ERROR_CODE_OK != ReCode)
		{
			printk("Error Channel Num...\n");
			return ERROR_CODE_INVALID_PARAM;
		}
	}



	ReCode = StartScan();
	if (ERROR_CODE_OK != ReCode)
	{
		printk("Failed to Scan ...\n");
		return ReCode;
	}




	memset(m_RawData, 0, sizeof(m_RawData));
	memset(m_iTempRawData, 0, sizeof(m_iTempRawData));
	ReCode = ReadRawData(0, 0xAD, g_stSCapConfEx.ChannelXNum * g_stSCapConfEx.ChannelYNum * 2, m_iTempRawData);
	if (ERROR_CODE_OK != ReCode)
	{
		printk("Failed to Get RawData\n");
		return ReCode;
	}

	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; ++iRow)
	{
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol)
		{
			m_RawData[iRow][iCol] = m_iTempRawData[iRow * g_stSCapConfEx.ChannelYNum + iCol];
		}
	}


	memset(m_iTempRawData, 0, sizeof(m_iTempRawData));
	ReCode = ReadRawData(0, 0xAE, g_stSCapConfEx.KeyNum * 2, m_iTempRawData);
	if (ERROR_CODE_OK != ReCode)
	{
		printk("Failed to Get RawData\n");
		return ReCode;
	}

	for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; ++iCol)
	{
		m_RawData[g_stSCapConfEx.ChannelXNum][iCol] = m_iTempRawData[iCol];
	}

	return ReCode;

}
/************************************************************************
* Name: FT8606_TestItem_RawDataTest
* Brief:  TestItem: RawDataTest. Check if MCAP RawData is within the range.
* Input: bTestResult
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8606_TestItem_RawDataTest(bool *bTestResult)
{
	unsigned char ReCode;
	bool btmpresult = true;

	int RawDataMin;
	int RawDataMax;
	int iValue = 0;
	int i = 0;
	int iRow, iCol;

	printk("\n\n==============================Test Item: -------- Raw Data Test\n\n");



	for (i = 0 ; i < 3; i++)
		ReCode = GetRawData();
	if (ERROR_CODE_OK != ReCode)
	{
		printk("Failed to get Raw Data!! Error Code: %d\n", ReCode);
		return ReCode;
	}



	printk("\nVA Channels: ");
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++)
	{
	printk("\nCh_%02d:  ", iRow+1);
	for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
	{
	printk("%5d, ", m_RawData[iRow][iCol]);
	}
	}
	printk("\nKeys:  ");
	for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; iCol++)
	{
	printk("%5d, ",  m_RawData[g_stSCapConfEx.ChannelXNum][iCol]);
	}



	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++)
	{

		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
		{
			if (g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol] == 0)continue;
			RawDataMin = g_stCfg_MCap_DetailThreshold.RawDataTest_Min[iRow][iCol];
			RawDataMax = g_stCfg_MCap_DetailThreshold.RawDataTest_Max[iRow][iCol];
			iValue = m_RawData[iRow][iCol];
			if (iValue < RawDataMin || iValue > RawDataMax)
			{
				btmpresult = false;
				printk("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
					iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
			}
		}
	}

	iRow = g_stSCapConfEx.ChannelXNum;
	for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; iCol++)
	{
		if (g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol] == 0)continue;
		RawDataMin = g_stCfg_MCap_DetailThreshold.RawDataTest_Min[iRow][iCol];
		RawDataMax = g_stCfg_MCap_DetailThreshold.RawDataTest_Max[iRow][iCol];
		iValue = m_RawData[iRow][iCol];
		if (iValue < RawDataMin || iValue > RawDataMax)
		{
			btmpresult = false;
			printk("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
				iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
		}
	}


	Save_Test_Data(m_RawData, 0, g_stSCapConfEx.ChannelXNum+1, g_stSCapConfEx.ChannelYNum, 1);

	if (btmpresult)
	{
		* bTestResult = true;
		printk("\n\n//RawData Test is OK!\n");
	}
	else
	{
		* bTestResult = false;
		printk("\n\n//RawData Test is NG!\n");
	}
	return ReCode;
}
/************************************************************************
* Name: SqrtNew
* Brief:  calculate sqrt of input.
* Input: unsigned int n
* Output: none
* Return: sqrt of n.
***********************************************************************/
static unsigned int SqrtNew(unsigned int n)
{
    unsigned int  val = 0, last = 0;
    unsigned char i = 0;;

    if (n < 6)
    {
        if (n < 2)
        {
            return n;
        }
        return n/2;
    }
    val = n;
    i = 0;
    while (val > 1)
    {
        val >>= 1;
        i++;
    }
    val <<= (i >> 1);
    val = (val + val + val) >> 1;
    do
    {
      last = val;
      val = ((val + n/val) >> 1);
    }while(focal_abs(val-last) > pre);
    return val;
}
/************************************************************************
* Name: FT8606_TestItem_NoiseTest
* Brief:  TestItem: NoiseTest. Check if MCAP Noise is within the range.
* Input: bTestResult
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8606_TestItem_NoiseTest(bool *bTestResult)
{
	unsigned char ReCode;
	unsigned char chNoiseValue = 0xff;
	bool btmpresult = true;

	int iNoiseFrames = 0;
	int i,iRow,iCol;
	int iValue = 0;
	int iMinValue = 0, iMaxValue = 0;
	int n,temp;

	int *pTempNext = NULL;
	int *pTempPrev = NULL;


	printk("\n\n==============================Test Item: -------- Noise Test  \n\n");

	iNoiseFrames = g_stCfg_FT8606_BasicThreshold.NoiseTest_Frames;
	if (iNoiseFrames > MAX_NOISE_FRAMES)
		iNoiseFrames = MAX_NOISE_FRAMES;



	for (i = 0; i < 3; i++)
	{
		ReCode = GetRawData();
	}
	if (ReCode != ERROR_CODE_OK) goto TEST_ERR;


	memset(m_TempNoiseData, 0, sizeof(m_TempNoiseData));
	memset(m_NoiseData, 0, sizeof(m_NoiseData));
	for (i = 0; i < iNoiseFrames; i++)
	{
		ReCode = GetRawData();
		if (ReCode != ERROR_CODE_OK) goto TEST_ERR;

		for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
		{

			for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
			{
				m_TempNoiseData[i][iRow*g_stSCapConfEx.ChannelYNum + iCol] = m_RawData[iRow][iCol];
			}
		}

	}


		memset(m_NoiseData, 0, sizeof(m_NoiseData));

		for (i = 0; i < iNoiseFrames; i++)
		{
			for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
			{
				for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
				{
					iValue = m_TempNoiseData[i][iRow*g_stSCapConfEx.ChannelYNum + iCol];
					m_NoiseData[iRow][iCol] += iValue;
				}
			}
		}

		for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
		{
			for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
			{
				m_AvgData[iRow][iCol]  = m_NoiseData[iRow][iCol]  / iNoiseFrames;
			}
		}


	if (NT_AvgData == g_stCfg_FT8606_BasicThreshold.NoiseTest_NoiseMode)
	{


		memset(m_NoiseData, 0, sizeof(m_NoiseData));
		for (i = 0; i < iNoiseFrames; i++)
		{
			for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
			{
				for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
				{
					iValue = m_TempNoiseData[i][iRow*g_stSCapConfEx.ChannelYNum + iCol];
					m_NoiseData[iRow][iCol] += (iValue -m_AvgData[iRow][iCol])*(iValue -m_AvgData[iRow][iCol]);
				}
			}
		}

		for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
		{
			for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
			{
				m_NoiseData[iRow][iCol]  = SqrtNew(m_NoiseData[iRow][iCol]  / iNoiseFrames);
			}
		}

	}
	else if (NT_MaxData == g_stCfg_FT8606_BasicThreshold.NoiseTest_NoiseMode)
	{

		memset(m_NoiseData, 0, sizeof(m_NoiseData));
		for (i = 0; i < iNoiseFrames; i++)
		{
			for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
			{
				for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
				{
					iValue = focal_abs(m_TempNoiseData[i][iRow*g_stSCapConfEx.ChannelYNum + iCol]);
					iValue = focal_abs(iValue - m_AvgData[iRow][iCol]);
					if (iValue > m_NoiseData[iRow][iCol])
						m_NoiseData[iRow][iCol] = iValue;
				}
			}
		}
	}
	else if (NT_MaxDevication == g_stCfg_FT8606_BasicThreshold.NoiseTest_NoiseMode)
	{

		memset(m_iTempData, 0xffff, sizeof(m_iTempData));
		memset(m_NoiseData, 0, sizeof(m_NoiseData));
		for (i = 0; i < iNoiseFrames; i++)
		{
			for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
			{
				for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
				{
					iValue = m_TempNoiseData[i][iRow*g_stSCapConfEx.ChannelYNum + iCol];
					if (iValue < m_iTempData[iRow][iCol])
						m_iTempData[iRow][iCol] = iValue;
					if (iValue > m_NoiseData[iRow][iCol])
						m_NoiseData[iRow][iCol] = iValue;
				}
			}
		}

		for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
		{
			for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
			{
				m_NoiseData[iRow][iCol]  -= m_iTempData[iRow][iCol];
			}
		}

	}
	else if (NT_DifferData == g_stCfg_FT8606_BasicThreshold.NoiseTest_NoiseMode)
	{

		memset(m_NoiseData, 0, sizeof(m_NoiseData));
		for (n = 1; n < iNoiseFrames; n++)
		{
			pTempNext = m_TempNoiseData[n];
			pTempPrev = m_TempNoiseData[n - 1];
			for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
			{
				for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
				{


					temp = focal_abs(pTempNext[iRow*g_stSCapConfEx.ChannelYNum+iCol] - pTempPrev[iRow*g_stSCapConfEx.ChannelYNum+iCol]);

					if (m_NoiseData[iRow][iCol] < temp)
						m_NoiseData[iRow][iCol] = temp;
				}
			}
		}
		/*
		for(iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; iRow++)
		{
			for(iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
			{
				m_NoiseData[iRow][iCol]  /= iNoiseFrames;
			}
		}
		*/
	}



	printk("\nVA Channels: ");
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++)
	{
	printk("\nCh_%02d:  ", iRow+1);
	for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
	{
	printk("%5d, ", m_NoiseData[iRow][iCol]);
	}
	}
	printk("\nKeys:  ");
	for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; iCol++)
	{
	printk("%5d, ",  m_NoiseData[g_stSCapConfEx.ChannelXNum][iCol]);
	}


	SysDelay(150);
	ReCode = EnterWork();
	if (ReCode != ERROR_CODE_OK) goto TEST_ERR;
	SysDelay(50);

	ReCode = ReadReg(0x80, &chNoiseValue);
	if (ReCode != ERROR_CODE_OK) goto TEST_ERR;

	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) goto TEST_ERR;



		iMinValue = 0;
		iMaxValue = g_stCfg_FT8606_BasicThreshold.NoiseTest_Coefficient * chNoiseValue * 32 / 100;
		printk("\n");
		for (iRow = 0; iRow < (g_stSCapConfEx.ChannelXNum + 1); iRow++)
		{
			for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
			{
				if ((0 == g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol]))
				{
					continue;
				}
				if (iRow >= g_stSCapConfEx.ChannelXNum && iCol >= g_stSCapConfEx.KeyNum)
				{
					continue;
				}


				if (m_NoiseData[iRow][iCol] < iMinValue || m_NoiseData[iRow][iCol] > iMaxValue)
				{
					btmpresult = false;
					printk("noise test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d)  \n", \
						iRow+1, iCol+1, m_NoiseData[iRow][iCol], iMinValue, iMaxValue);
				}
			}
			printk("\n");
		}




	Save_Test_Data(m_NoiseData, 0, g_stSCapConfEx.ChannelXNum+1, g_stSCapConfEx.ChannelYNum, 1);

	if (btmpresult)
	{
		* bTestResult = true;
		printk("\n\n//Noise Test is OK!\n");
	}
	else
	{
		* bTestResult = false;
		printk("\n\n//Noise Test is NG!\n");
	}

	return ReCode;
TEST_ERR:

	* bTestResult = false;
	printk("\n\n//Noise Test is NG!\n");
	return ReCode;
}
/************************************************************************
* Name: FT8606_TestItem_CbTest
* Brief:  TestItem: Cb Test. Check if Cb is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8606_TestItem_CbTest(bool *bTestResult)
{
	bool btmpresult = true;
	unsigned char ReCode = ERROR_CODE_OK;
	int iRow = 0;
	int iCol = 0;
	int iMaxValue = 0;
	int iMinValue = 0;

	printk("\n\n==============================Test Item: --------  CB Test\n\n");

	ReCode = GetTxRxCB(0, (short)(g_stSCapConfEx.ChannelXNum * g_stSCapConfEx.ChannelYNum + g_stSCapConfEx.KeyNum), m_ucTempData);

	ReCode = GetTxRxCB(0, (short)(g_stSCapConfEx.ChannelXNum * g_stSCapConfEx.ChannelYNum + g_stSCapConfEx.KeyNum), m_ucTempData);
	if (ERROR_CODE_OK != ReCode)
	{
		btmpresult = false;
		printk("Failed to get CB value...\n");
		goto TEST_ERR;
	}

	memset(m_CBData, 0, sizeof(m_CBData));

	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; ++iRow)
	{
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol)
		{
			m_CBData[iRow][iCol] = m_ucTempData[ iRow * g_stSCapConfEx.ChannelYNum + iCol ];
		}
	}

	for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; ++iCol)
	{
		m_CBData[g_stSCapConfEx.ChannelXNum][iCol] = m_ucTempData[ g_stSCapConfEx.ChannelXNum*g_stSCapConfEx.ChannelYNum + iCol ];
	}




	printk("\nVA Channels: ");
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++)
	{
	printk("\nCh_%02d:  ", iRow+1);
	for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
	{
	printk("%3d, ", m_CBData[iRow][iCol]);
	}
	}
	printk("\nKeys:  ");
	for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; iCol++)
	{
	printk("%3d, ",  m_CBData[g_stSCapConfEx.ChannelXNum][iCol]);
	}

	iMinValue = g_stCfg_FT8606_BasicThreshold.CbTest_Min;
	iMaxValue = g_stCfg_FT8606_BasicThreshold.CbTest_Max;
	for (iRow = 0; iRow < (g_stSCapConfEx.ChannelXNum + 1); iRow++)
	{
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++)
		{
			if ((0 == g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol]))
			{
				continue;
			}
			if (iRow >= g_stSCapConfEx.ChannelXNum && iCol >= g_stSCapConfEx.KeyNum)
			{
				continue;
			}

			if (focal_abs(m_CBData[iRow][iCol]) < iMinValue || focal_abs(m_CBData[iRow][iCol]) > iMaxValue)
			{
				btmpresult = false;
				printk("CB test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
					iRow+1, iCol+1, m_CBData[iRow][iCol], iMinValue, iMaxValue);
			}
		}
	}


	Save_Test_Data(m_CBData, 0, g_stSCapConfEx.ChannelXNum+1, g_stSCapConfEx.ChannelYNum, 1);

	if (btmpresult)
	{
		* bTestResult = true;
		printk("\n\n//CB Test is OK!\n");
	}
	else
	{
		* bTestResult = false;
		printk("\n\n//CB Test is NG!\n");
	}

	return ReCode;

TEST_ERR:

	* bTestResult = false;
	printk("\n\n//CB Test is NG!\n");
	return ReCode;
}
