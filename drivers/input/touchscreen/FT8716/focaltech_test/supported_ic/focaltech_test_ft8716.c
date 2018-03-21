/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_ft8716.c
*
* Author: Software Development
*
* Created: 2016-08-01
*
* Abstract: test item for FT8716
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "../include/focaltech_test_detail_threshold.h"
#include "../include/focaltech_test_supported_ic.h"
#include "../include/focaltech_test_main.h"
#include "../focaltech_test_config.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#if (FTS_CHIP_TEST_TYPE == FT8716_TEST)





#define DEVIDE_MODE_ADDR                0x00
#define REG_LINE_NUM                    0x01
#define REG_TX_NUM                      0x02
#define REG_RX_NUM                      0x03
#define FT8716_LEFT_KEY_REG             0X1E
#define FT8716_RIGHT_KEY_REG            0X1F

#define REG_CbAddrH                     0x18
#define REG_CbAddrL                     0x19
#define REG_OrderAddrH                  0x1A
#define REG_OrderAddrL                  0x1B

#define REG_RawBuf0                     0x6A
#define REG_RawBuf1                     0x6B
#define REG_OrderBuf0                   0x6C
#define REG_CbBuf0                      0x6E

#define REG_K1Delay                     0x31
#define REG_K2Delay                     0x32
#define REG_SCChannelCf                 0x34
#define REG_LCD_NOISE_FRAME                   0X12
#define REG_LCD_NOISE_START                   	0X11
#define REG_LCD_NOISE_NUMBER                 0X13
#define REG_LCD_NOISE_DATA_READY         0X00
#define REG_FWVERSION       				0xA6
#define REG_FACTORYID       				0xA8
#define REG_FWCNT           				0x17
#define pre                                 1
#define REG_CLB                          0x04


/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

enum NOISE_TYPE {
	NT_AvgData = 0,
	NT_MaxData = 1,
	NT_MaxDevication = 2,
	NT_DifferData = 3,
};

unsigned char localbitWise;
void SetKeyBitVal(unsigned char val)
{

	localbitWise = val;

}

bool IsKeyAutoFit(void)
{
	return ((localbitWise & 0x0f) == 1);
}


/*****************************************************************************
* Static variables
*****************************************************************************/

static int m_RawData[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} } ;
static int m_CBData[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} } ;
static BYTE m_ucTempData[TX_NUM_MAX * RX_NUM_MAX*2] = {0};
static int m_iTempRawData[TX_NUM_MAX * RX_NUM_MAX] = {0};



static int LCD_Noise[TX_NUM_MAX][RX_NUM_MAX] = {{0} } ;







/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern struct stCfg_FT8716_BasicThreshold g_stCfg_FT8716_BasicThreshold;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/


static int StartScan(void);
static unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer);
static unsigned char GetPanelRows(unsigned char *pPanelRows);
static unsigned char GetPanelCols(unsigned char *pPanelCols);
static unsigned char GetTxRxCB(unsigned short StartNodeNo, unsigned short ReadNum, unsigned char *pReadBuffer);
static unsigned char GetRawData(void);
static unsigned char GetChannelNum(void);
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount);
static unsigned char ChipClb(unsigned char *pClbResult);





boolean FT8716_StartTest(void);
int FT8716_get_test_data(char *pTestData);

unsigned char FT8716_TestItem_EnterFactoryMode(void);
unsigned char FT8716_TestItem_RawDataTest(bool *bTestResult);
unsigned char FT8716_TestItem_CbTest(bool *bTestResult);
unsigned char FT8716_TestItem_ChannelsTest(bool *bTestResult);
unsigned char FT8716_TestItem_LCDNoiseTest(bool *bTestResult);
unsigned char FT8716_CheckItem_FactoryIdTest(bool *bTestResult);

/************************************************************************
* Name: FT8716_StartTest
* Brief:  Test entry. Determine which test item to test
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/

boolean FT8716_StartTest()
{
	bool bTestResult = true, bTempResult = 1;
	unsigned char ReCode;
	unsigned char ucDevice = 0;
	int iItemCount = 0;



	if (InitTest() < 0) {
		FTS_TEST_ERROR("[focal] Failed to init test.");
		return false;
	}


	if (0 == g_TestItemNum)
		bTestResult = false;


	for (iItemCount = 0; iItemCount < g_TestItemNum; iItemCount++) {
		m_ucTestItemCode = g_stTestItem[ucDevice][iItemCount].ItemCode;


		if (Code_FT8716_ENTER_FACTORY_MODE == g_stTestItem[ucDevice][iItemCount].ItemCode
		   ) {
			ReCode = FT8716_TestItem_EnterFactoryMode();
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
				break;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8716_RAWDATA_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
		   ) {
			ReCode = FT8716_TestItem_RawDataTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8716_CB_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
		   ) {
			ReCode = FT8716_TestItem_CbTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8716_CHANNEL_NUM_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode) {
			ReCode = FT8716_TestItem_ChannelsTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
				break;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;

		}


		if (Code_FT8716_LCD_NOISE_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode) {
			ReCode = FT8716_TestItem_LCDNoiseTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8716_FACTORY_ID_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode) {
			ReCode = FT8716_CheckItem_FactoryIdTest(&bTempResult);
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
* Name: FT8716_TestItem_EnterFactoryMode
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8716_TestItem_EnterFactoryMode(void)
{

	unsigned char ReCode = ERROR_CODE_INVALID_PARAM;
	int iRedo = 5;
	int i ;
	unsigned char keyFit = 0;
	SysDelay(150);
	FTS_TEST_DBG("Enter factory mode...");
	for (i = 1; i <= iRedo; i++) {
		ReCode = EnterFactory();
		if (ERROR_CODE_OK != ReCode) {
			FTS_TEST_ERROR("Failed to Enter factory mode...");
			if (i < iRedo) {
				SysDelay(50);
				continue;
			}
		} else {
			FTS_TEST_DBG(" success to Enter factory mode...");
			break;
		}

	}
	SysDelay(300);

	if (ReCode == ERROR_CODE_OK) {
		ReCode = GetChannelNum();

		ReCode = ReadReg(0xFC, &keyFit);
		SetKeyBitVal(keyFit);
	}
	return ReCode;
}

/************************************************************************
* Name: FT8716_TestItem_RawDataTest
* Brief:  TestItem: RawDataTest. Check if MCAP RawData is within the range.
* Input: bTestResult
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8716_TestItem_RawDataTest(bool *bTestResult)
{
	unsigned char ReCode;
	bool btmpresult = true;
	int RawDataMin;
	int RawDataMax;
	int iValue = 0;
	int i = 0;
	int iRow, iCol;
	bool bIncludeKey = false;



	FTS_TEST_INFO("\n\n==============================Test Item: -------- Raw Data Test\n");

	bIncludeKey = g_stCfg_FT8716_BasicThreshold.bRawDataTest_VKey_Check;


	for (i = 0 ; i < 3; i++) {
		ReCode = WriteReg(0x06, 0x00);
		SysDelay(10);
		ReCode = GetRawData();
	}

	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERROR("Failed to get Raw Data!! Error Code: %d",  ReCode);
		return ReCode;
	}



	FTS_TEST_DBG("\nVA Channels: ");
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++) {
		FTS_TEST_DBG("\nCh_%02d:  ", iRow+1);
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++) {
			FTS_TEST_DBG("%5d, ", m_RawData[iRow][iCol]);
		}
	}

	FTS_TEST_DBG("\nKeys:  ");
	if (IsKeyAutoFit()) {
		for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; iCol++)
			FTS_TEST_DBG("%5d, ",  m_RawData[g_stSCapConfEx.ChannelXNum][iCol]);
	} else {
		for (iCol = 0; iCol < g_stSCapConfEx.KeyNumTotal; iCol++) {
			FTS_TEST_DBG("%5d, ",  m_RawData[g_stSCapConfEx.ChannelXNum][iCol]);
		}
	}





	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++) {

		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++) {
			if (g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0)
				continue;
			RawDataMin = g_stCfg_Incell_DetailThreshold.RawDataTest_Min[iRow][iCol];
			RawDataMax = g_stCfg_Incell_DetailThreshold.RawDataTest_Max[iRow][iCol];
			iValue = m_RawData[iRow][iCol];

			if (iValue < RawDataMin || iValue > RawDataMax) {
				btmpresult = false;
				FTS_TEST_ERROR("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
							  iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
			}
		}
	}


	if (bIncludeKey) {

		iRow = g_stSCapConfEx.ChannelXNum;
		if (IsKeyAutoFit()) {
			for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; iCol++) {
				if (g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0)
					continue;
				RawDataMin = g_stCfg_Incell_DetailThreshold.RawDataTest_Min[iRow][iCol];
				RawDataMax = g_stCfg_Incell_DetailThreshold.RawDataTest_Max[iRow][iCol];
				iValue = m_RawData[iRow][iCol];

				if (iValue < RawDataMin || iValue > RawDataMax) {
					btmpresult = false;
					FTS_TEST_ERROR("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
								  iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
				}
			}
		} else {
			for (iCol = 0; iCol < g_stSCapConfEx.KeyNumTotal; iCol++) {
				if (g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0)
					continue;
				RawDataMin = g_stCfg_Incell_DetailThreshold.RawDataTest_Min[iRow][iCol];
				RawDataMax = g_stCfg_Incell_DetailThreshold.RawDataTest_Max[iRow][iCol];
				iValue = m_RawData[iRow][iCol];

				if (iValue < RawDataMin || iValue > RawDataMax) {
					btmpresult = false;
					FTS_TEST_ERROR("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
								  iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
				}
			}
		}
	}


	Save_Test_Data(m_RawData, 0, g_stSCapConfEx.ChannelXNum+1, g_stSCapConfEx.ChannelYNum, 1);


	TestResultLen += sprintf(TestResult+TestResultLen, "RawData Test is %s. \n\n", (btmpresult ? "OK" : "NG"));


	if (btmpresult) {
		*bTestResult = true;
		FTS_TEST_INFO("\n\n//RawData Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_INFO("\n\n//RawData Test is NG!");
	}
	return ReCode;
}


/************************************************************************
* Name: FT8716_TestItem_CbTest
* Brief:  TestItem: Cb Test. Check if Cb is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8716_TestItem_CbTest(bool *bTestResult)
{
	bool btmpresult = true;
	unsigned char ReCode = ERROR_CODE_OK;
	int iRow = 0;
	int iCol = 0;
	int iMaxValue = 0;
	int iMinValue = 0;
	int iValue = 0;
	bool bIncludeKey = false;

	unsigned char bClbResult = 0;
	unsigned char ucBits = 0;
	int ReadKeyLen = g_stSCapConfEx.KeyNumTotal;

	bIncludeKey = g_stCfg_FT8716_BasicThreshold.bCBTest_VKey_Check;

	FTS_TEST_INFO("\n\n==============================Test Item: --------  CB Test\n");


	ReCode = ChipClb(&bClbResult);
	if (ERROR_CODE_OK != ReCode) {
		btmpresult = false;
		FTS_TEST_ERROR("\r\n//========= auto clb Failed!");
	}

	ReCode = ReadReg(0x0B, &ucBits);
	if (ERROR_CODE_OK != ReCode) {
		btmpresult = false;
		FTS_TEST_ERROR("\r\n//=========  Read Reg Failed!");
	}

	ReadKeyLen = g_stSCapConfEx.KeyNumTotal;
	if (ucBits != 0) {
		ReadKeyLen = g_stSCapConfEx.KeyNumTotal * 2;
	}



	ReCode = GetTxRxCB(0, (short)(g_stSCapConfEx.ChannelXNum * g_stSCapConfEx.ChannelYNum  + ReadKeyLen), m_ucTempData);
	if (ERROR_CODE_OK != ReCode) {
		btmpresult = false;
		FTS_TEST_ERROR("Failed to get CB value...");
		goto TEST_ERR;
	}

	memset(m_CBData, 0, sizeof(m_CBData));

	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; ++iRow) {
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {
			m_CBData[iRow][iCol] = m_ucTempData[iRow * g_stSCapConfEx.ChannelYNum + iCol];
		}
	}


	for (iCol = 0; iCol < ReadKeyLen/*g_stSCapConfEx.KeyNumTotal*/; ++iCol) {
		if (ucBits != 0) {
			m_CBData[g_stSCapConfEx.ChannelXNum][iCol/2] = (short)((m_ucTempData[g_stSCapConfEx.ChannelXNum*g_stSCapConfEx.ChannelYNum + iCol] & 0x01)<<8) + m_ucTempData[g_stSCapConfEx.ChannelXNum*g_stSCapConfEx.ChannelYNum + iCol + 1];
			iCol++;
		} else {
			m_CBData[g_stSCapConfEx.ChannelXNum][iCol] = m_ucTempData[g_stSCapConfEx.ChannelXNum*g_stSCapConfEx.ChannelYNum + iCol];
		}

	}






	FTS_TEST_DBG("\nVA Channels: ");
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++) {
		FTS_TEST_DBG("\nCh_%02d:  ", iRow+1);
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++) {
			FTS_TEST_DBG("%3d, ", m_CBData[iRow][iCol]);
		}
	}
	FTS_TEST_DBG("\nKeys:  ");
	if (IsKeyAutoFit()) {
		for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; iCol++) {
			FTS_TEST_DBG("%3d, ",  m_CBData[g_stSCapConfEx.ChannelXNum][iCol]);

		}

	} else {
		for (iCol = 0; iCol < g_stSCapConfEx.KeyNumTotal; iCol++) {
			FTS_TEST_DBG("%3d, ",  m_CBData[g_stSCapConfEx.ChannelXNum][iCol]);
		}
	}



	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++) {
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++) {
			if ((0 == g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol])) {
				continue;
			}
			iMinValue = g_stCfg_Incell_DetailThreshold.CBTest_Min[iRow][iCol];
			iMaxValue = g_stCfg_Incell_DetailThreshold.CBTest_Max[iRow][iCol];
			iValue = focal_abs(m_CBData[iRow][iCol]);

			if (iValue < iMinValue || iValue > iMaxValue) {
				btmpresult = false;
				FTS_TEST_ERROR("CB test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
							  iRow+1, iCol+1, iValue, iMinValue, iMaxValue);
			}
		}
	}


	if (bIncludeKey) {

		iRow = g_stSCapConfEx.ChannelXNum;
		if (IsKeyAutoFit()) {
			for (iCol = 0; iCol < g_stSCapConfEx.KeyNum; iCol++) {
				if (g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0) {
					continue;
				}
				iMinValue = g_stCfg_Incell_DetailThreshold.CBTest_Min[iRow][iCol];
				iMaxValue = g_stCfg_Incell_DetailThreshold.CBTest_Max[iRow][iCol];
				iValue = focal_abs(m_CBData[iRow][iCol]);

				if (iValue < iMinValue || iValue > iMaxValue) {
					btmpresult = false;
					FTS_TEST_ERROR("CB test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
								  iRow+1, iCol+1, iValue, iMinValue, iMaxValue);
				}
			}
		} else {
			for (iCol = 0; iCol < g_stSCapConfEx.KeyNumTotal; iCol++) {
				if (g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0)
					continue;
				iMinValue = g_stCfg_Incell_DetailThreshold.CBTest_Min[iRow][iCol];
				iMaxValue = g_stCfg_Incell_DetailThreshold.CBTest_Max[iRow][iCol];
				iValue = focal_abs(m_CBData[iRow][iCol]);

				if (iValue < iMinValue || iValue > iMaxValue) {
					btmpresult = false;
					FTS_TEST_ERROR("CB test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
								  iRow+1, iCol+1, iValue, iMinValue, iMaxValue);
				}
			}
		}
	}


	Save_Test_Data(m_CBData, 0, g_stSCapConfEx.ChannelXNum+1, g_stSCapConfEx.ChannelYNum, 1);

	TestResultLen += sprintf(TestResult+TestResultLen, "CB Test is %s. \n\n", (btmpresult ? "OK" : "NG"));


	if (btmpresult) {
		*bTestResult = true;
		FTS_TEST_INFO("\n\n//CB Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_INFO("\n\n//CB Test is NG!");
	}

	return ReCode;

TEST_ERR:

	*bTestResult = false;
	FTS_TEST_INFO("\n\n//CB Test is NG!");

	TestResultLen += sprintf(TestResult+TestResultLen, "CB Test is NG. \n\n");

	return ReCode;
}


/************************************************************************
* Name: FT8716_TestItem_ChannelsTest
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8716_TestItem_ChannelsTest(bool *bTestResult)
{
	unsigned char ReCode;

	FTS_TEST_DBG("\n\n==============================Test Item: -------- Channel Test ");

	ReCode = GetChannelNum();
	if (ReCode == ERROR_CODE_OK) {
		if ((g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelXNum == g_stSCapConfEx.ChannelXNum)
			&& (g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelYNum == g_stSCapConfEx.ChannelYNum)
			&& (g_stCfg_FT8716_BasicThreshold.ChannelNumTest_KeyNum == g_stSCapConfEx.KeyNum)) {
			*bTestResult = true;
			FTS_TEST_DBG("\n\nGet channels: (CHx: %d, CHy: %d, Key: %d), Set channels: (CHx: %d, CHy: %d, Key: %d)",
						g_stSCapConfEx.ChannelXNum, g_stSCapConfEx.ChannelYNum, g_stSCapConfEx.KeyNum,
						g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelXNum, g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelYNum, g_stCfg_FT8716_BasicThreshold.ChannelNumTest_KeyNum);

			FTS_TEST_INFO("\n//Channel Test is OK!");
		} else {
			*bTestResult = false;
			FTS_TEST_ERROR("\n\nGet channels: (CHx: %d, CHy: %d, Key: %d), Set channels: (CHx: %d, CHy: %d, Key: %d)",
						g_stSCapConfEx.ChannelXNum, g_stSCapConfEx.ChannelYNum, g_stSCapConfEx.KeyNum,
						g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelXNum, g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelYNum, g_stCfg_FT8716_BasicThreshold.ChannelNumTest_KeyNum);

			FTS_TEST_INFO("\n//Channel Test is NG!");
		}
	}


	TestResultLen += sprintf(TestResult+TestResultLen, "Channel Test is %s. \n\n", (*bTestResult ? "OK" : "NG"));

	return ReCode;
}

/************************************************************************
* Name: SqrtNew
* Brief:  calculate sqrt of input.
* Input: unsigned int n
* Output: none
* Return: sqrt of n.
***********************************************************************/
unsigned int SqrtNew(unsigned int n)
{
	unsigned int  val = 0, last = 0;
	unsigned char i = 0;;

	if (n < 6) {
		if (n < 2) {
			return n;
		}
		return n/2;
	}
	val = n;
	i = 0;
	while (val > 1) {
		val >>= 1;
		i++;
	}
	val <<= (i >> 1);
	val = (val + val + val) >> 1;
	do {
		last = val;
		val = ((val + n/val) >> 1);
	} while (focal_abs(val-last) > pre);
	return val;
}

/************************************************************************
* Name: FT8716_TestItem_LCDNoiseTest
* Brief:   obtain is differ mode  the data and calculate the corresponding type of noise value.
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8716_TestItem_LCDNoiseTest(bool *bTestResult)
{

	unsigned char ReCode = ERROR_CODE_OK;
	bool bResultFlag = true;
	int FrameNum = 0;
	int i = 0;
	int iRow = 0;
	int iCol = 0;
	int iValueMin = 0;
	int iValueMax = 0;
	int iValue = 0;
	int ikey = 0;
	unsigned char regData = 0, oldMode = 0, chNewMod = 0, DataReady = 0;
	unsigned char chNoiseValueVa = 0xff, chNoiseValueKey = 0xff;

	FTS_TEST_INFO("\r\n\r\n==============================Test Item: -------- LCD Noise Test \r\n");

	ReCode =  ReadReg(0x06, &oldMode);
	ReCode =  WriteReg(0x06, 0x01);


	SysDelay(50);
	ReCode = ReadReg(0x06, &chNewMod);
	if (ReCode != ERROR_CODE_OK || chNewMod != 1) {
		bResultFlag = false;
		FTS_TEST_ERROR("\r\nSwitch Mode Failed!\r\n");
		goto TEST_ERR;
	}

	FrameNum = g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_FrameNum/4;
	ReCode = WriteReg(REG_LCD_NOISE_FRAME, FrameNum);


	SysDelay(50);
	if (ReCode != ERROR_CODE_OK) {
		bResultFlag = false;
		FTS_TEST_ERROR("\r\nWrite Reg Failed!\r\n");
		goto TEST_ERR;
	}

	ReCode = WriteReg(REG_LCD_NOISE_START, 0x01);
	for (i = 0; i < 100; i++) {
		SysDelay(50);

		ReCode = ReadReg(REG_LCD_NOISE_DATA_READY, &DataReady);
		if (0x00 == (DataReady>>7)) {
			SysDelay(5);
			ReCode = ReadReg(REG_LCD_NOISE_START, &DataReady);
			if (DataReady == 0x00) {
				break;
			} else {
				continue;
			}
		} else {
			continue;
		}


		if (99 == i) {
			ReCode = WriteReg(REG_LCD_NOISE_START, 0x00);
			if (ReCode != ERROR_CODE_OK) {
				bResultFlag = false;
				FTS_TEST_ERROR("\r\nRestore Failed!\r\n");
				goto TEST_ERR;
			}

			bResultFlag = false;
			FTS_TEST_ERROR("\r\nTime Over!\r\n");
			goto TEST_ERR;
		}
	}

	memset(m_RawData, 0, sizeof(m_RawData));
	memset(m_iTempRawData, 0, sizeof(m_iTempRawData));


	ReCode = ReadRawData(0, 0xAD, g_stSCapConfEx.ChannelXNum * g_stSCapConfEx.ChannelYNum * 2, m_iTempRawData);
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; ++iRow) {
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {
			m_RawData[iRow][iCol] = m_iTempRawData[iRow * g_stSCapConfEx.ChannelYNum + iCol];
		}
	}


	ReCode = ReadRawData(0, 0xAE, g_stSCapConfEx.KeyNumTotal * 2, m_iTempRawData);
	for (iCol = 0; iCol < g_stSCapConfEx.KeyNumTotal; ++iCol) {
		m_RawData[g_stSCapConfEx.ChannelXNum][iCol] = m_iTempRawData[iCol];
	}

	ReCode = WriteReg(REG_LCD_NOISE_START, 0x00);
	if (ReCode != ERROR_CODE_OK) {
		bResultFlag = false;
		FTS_TEST_ERROR("\r\nRestore Failed!\r\n");
		goto TEST_ERR;
	}
	ReCode = ReadReg(REG_LCD_NOISE_NUMBER, &regData);
	if (regData <= 0) {
		regData = 1;
	}

	ReCode = WriteReg(0x06, oldMode);
	if (ReCode != ERROR_CODE_OK) {
		bResultFlag = false;
		FTS_TEST_ERROR("\r\nWrite Reg Failed!\r\n");
		goto TEST_ERR;
	}

	if (0 == g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_NoiseMode) {
		for (iRow = 0; iRow <= g_stSCapConfEx.ChannelXNum; ++iRow) {
			for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {
				LCD_Noise[iRow][iCol] = m_RawData[iRow][iCol];
			}
		}
	}

	if (1 == g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_NoiseMode) {
		for (iRow = 0; iRow <= g_stSCapConfEx.ChannelXNum; ++iRow) {
			for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {
				LCD_Noise[iRow][iCol] = SqrtNew(m_RawData[iRow][iCol] / regData);
			}
		}
	}

	ReCode = EnterWork();
	SysDelay(100);
	ReCode = ReadReg(0x80, &chNoiseValueVa);
	ReCode = ReadReg(0x82, &chNoiseValueKey);
	ReCode = EnterFactory();
	SysDelay(200);
	if (ReCode != ERROR_CODE_OK) {
		bResultFlag = false;
		FTS_TEST_ERROR("\r\nEnter factory mode failed.\r\n");
	}

#if 1
	FTS_TEST_DBG("\nVA Channels: ");
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum+1; ++iRow) {
		FTS_TEST_DBG("\nCh_%02d:  ", iRow+1);
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {

			FTS_TEST_DBG("%4d, ", LCD_Noise[iRow][iCol]);
		}
	}
	FTS_TEST_DBG("\n");
#endif


	iValueMin = 0;
	iValueMax = g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_Coefficient * chNoiseValueVa * 32 / 100;

	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++) {
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++) {
			if (g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0)
				continue;

			iValue = LCD_Noise[iRow][iCol];
			if (iValue < iValueMin || iValue > iValueMax) {
				bResultFlag = false;
				FTS_TEST_ERROR(" LCD Noise test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d). \n",
							  iRow+1, iCol+1, iValue, iValueMin, iValueMax);
			}
		}
	}

	FTS_TEST_DBG(" Va_Set_Range=(%d, %d). ", iValueMin, iValueMax);


	iValueMin = 0;
	iValueMax = g_stCfg_FT8716_BasicThreshold.LCDNoiseTest_Coefficient_Key * chNoiseValueKey * 32 / 100;
	for (ikey = 0; ikey < g_stSCapConfEx.KeyNumTotal; ikey++) {
		if (g_stCfg_Incell_DetailThreshold.InvalidNode[g_stSCapConfEx.ChannelXNum][ikey] == 0)
			continue;
		iValue = LCD_Noise[g_stSCapConfEx.ChannelXNum][ikey];
		if (iValue < iValueMin || iValue > iValueMax) {
			bResultFlag = false;
			FTS_TEST_ERROR(" LCD Noise test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d). \n",
						  g_stSCapConfEx.ChannelXNum+1, ikey+1, iValue, iValueMin, iValueMax);
		}
	}

	FTS_TEST_DBG("Key_Set_Range=(%d, %d). ", iValueMin, iValueMax);

	Save_Test_Data(LCD_Noise, 0,  g_stSCapConfEx.ChannelXNum + 1, g_stSCapConfEx.ChannelYNum, 1);

	TestResultLen += sprintf(TestResult+TestResultLen, " LCD Noise Test is %s. \n\n", (bResultFlag  ? "OK" : "NG"));

	if (bResultFlag) {
		*bTestResult = true;
		FTS_TEST_INFO("\n\n//LCD Noise Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_INFO("\n\n//LCD Noise Test  is NG!");
	}

	return ReCode;

TEST_ERR:

	*bTestResult = false;
	FTS_TEST_INFO("\n\n//LCD Noise Test is NG!");

	TestResultLen += sprintf(TestResult+TestResultLen, " LCD Noise Test is NG. \n\n");

	return ReCode;

}



/************************************************************************
* Name: FT8716_CheckItem_FactoryIdTest
* Brief:   Factory Id Testt.
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8716_CheckItem_FactoryIdTest(bool *bTestResult)
{
	unsigned char ReCode = ERROR_CODE_OK;
	bool btmpresult = true;
	unsigned char regData = 0;
	unsigned char FactoryId = 0;

	FTS_TEST_INFO("\n\n==============================Test Item: -------- Factory Id Test\n");

	ReCode = EnterWork();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERROR(" EnterWork failed.. Error Code: %d \n", ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}


	SysDelay(60);
	ReCode = ReadReg(REG_FACTORYID, &regData);

	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERROR("  Failed to Read Register: %x. Error Code: %d \n", REG_FACTORYID, ReCode);
		goto TEST_ERR;
	}

	FactoryId = g_stCfg_FT8716_BasicThreshold.Factory_ID_Number;

	FTS_TEST_DBG("\r\nGet FactoryId:0x%02x, Set FactoryId: 0x%02x  \n", regData, FactoryId);

	if (FactoryId == regData) {
		btmpresult = true;

	} else {
		btmpresult = false;
	}

	TestResultLen += sprintf(TestResult+TestResultLen, " FactoryId Test is %s. \n\n", (btmpresult  ? "OK" : "NG"));

	if (btmpresult) {
		*bTestResult = true;
		FTS_TEST_INFO("\n\n//FactoryId Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_INFO("\n\n//FactoryId Test is NG!");
	}
	return ReCode;

TEST_ERR:
	*bTestResult = false;
	FTS_TEST_INFO("\n\n//FactoryId Test is NG!");

	TestResultLen += sprintf(TestResult+TestResultLen, " FactoryId Test is NG. \n\n");

	return ReCode;
}


static unsigned char ChipClb(unsigned char *pClbResult)
{
	unsigned char RegData = 0;
	unsigned char TimeOutTimes = 50;
	unsigned char ReCode = ERROR_CODE_OK;

	ReCode = WriteReg(REG_CLB, 4);

	if (ReCode == ERROR_CODE_OK) {
		while (TimeOutTimes--) {
			SysDelay(100);
			ReCode = WriteReg(DEVIDE_MODE_ADDR, 0x04<<4);
			ReCode = ReadReg(0x04, &RegData);
			if (ReCode == ERROR_CODE_OK) {
				if (RegData == 0x02) {
					*pClbResult = 1;
					break;
				}
			} else {
				break;
			}
		}

		if (TimeOutTimes == 0) {
			*pClbResult = 0;
		}
	}
	return ReCode;
}
/************************************************************************
* Name: FT8716_get_test_data
* Brief:  get data of test result
* Input: none
* Output: pTestData, the returned buff
* Return: the length of test data. if length > 0, got data;else ERR.
***********************************************************************/
int FT8716_get_test_data(char *pTestData)
{
	if (NULL == pTestData) {
		FTS_TEST_ERROR(" pTestData == NULL ");
		return -EPERM;
	}
	memcpy(pTestData, g_pStoreAllData, (g_lenStoreMsgArea+g_lenStoreDataArea));
	return (g_lenStoreMsgArea+g_lenStoreDataArea);
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


	for (i = 0+iArrayIndex; (i < Row+iArrayIndex) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < Col) && (j < RX_NUM_MAX); j++) {
			if (j == (Col - 1))
				iLen = sprintf(g_pTmpBuff, "%d, \n",  iData[i][j]);
			else
				iLen = sprintf(g_pTmpBuff, "%d, ", iData[i][j]);

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
	const unsigned char MaxTimes = 20;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;



	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
	printk("hb1 :RegVal:0x%x, ReCode:0x%x\n", RegVal, ReCode);
	if (ReCode == ERROR_CODE_OK) {
		RegVal |= 0x80;
		ReCode = WriteReg(DEVIDE_MODE_ADDR, RegVal);
	printk("hb2 :RegVal:0x%x, ReCode:0x%x\n", RegVal, ReCode);
		if (ReCode == ERROR_CODE_OK) {
			while (times++ < MaxTimes) {
				SysDelay(8);
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
		printk("hb3 :RegVal:0x%x, ReCode:0x%x, times:0x%x\n", RegVal, ReCode, times);
				if (ReCode == ERROR_CODE_OK) {
					if ((RegVal>>7) == 0) {
			printk("hb4 :RegVal>>7:0x%x, ReCode:0x%x\n", RegVal>>7, ReCode);
			break;
			}
				} else {
					printk("hb5 :RegVal:0x%x, ReCode:0x%x\n", RegVal, ReCode);
					break;
				}
			}
			if (times < MaxTimes) {
				printk("hb6 :times:0x%x, MaxTimes:0x%x   ------OK\n", times, MaxTimes);
				ReCode = ERROR_CODE_OK;
		}
			else{
			printk("hb6 :times:0x%x, MaxTimes:0x%x   ------FAIL\n", times, MaxTimes);
			ReCode = ERROR_CODE_COMM_ERROR;
		}
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

	iReadNum = ByteNum/BYTES_PER_TIME;

	if (0 != (ByteNum%BYTES_PER_TIME))
		iReadNum++;

	if (ByteNum <= BYTES_PER_TIME) {
		BytesNumInTestMode1 = ByteNum;
	} else {
		BytesNumInTestMode1 = BYTES_PER_TIME;
	}

	ReCode = WriteReg(REG_LINE_NUM, LineNum);



	I2C_wBuffer[0] = REG_RawBuf0;
	if (ReCode == ERROR_CODE_OK) {
		focal_msleep(10);
		ReCode = Comm_Base_IIC_IO(I2C_wBuffer, 1, pReadData, BytesNumInTestMode1);
	}

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK)
			break;

		if (i == iReadNum-1) {
			focal_msleep(10);
			ReCode = Comm_Base_IIC_IO(NULL, 0, pReadData+BYTES_PER_TIME*i, ByteNum-BYTES_PER_TIME*i);
		} else {
			focal_msleep(10);
			ReCode = Comm_Base_IIC_IO(NULL, 0, pReadData+BYTES_PER_TIME*i, BYTES_PER_TIME);
		}

	}

	if (ReCode == ERROR_CODE_OK) {
		for (i = 0; i < (ByteNum>>1); i++) {
			pRevBuffer[i] = (pReadData[i<<1]<<8)+pReadData[(i<<1)+1];




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
	unsigned short usReturnNum = 0;
	unsigned short usTotalReturnNum = 0;
	unsigned char wBuffer[4];
	int i, iReadNum;

	iReadNum = ReadNum/BYTES_PER_TIME;

	if (0 != (ReadNum%BYTES_PER_TIME))
		iReadNum++;

	wBuffer[0] = REG_CbBuf0;

	usTotalReturnNum = 0;

	for (i = 1; i <= iReadNum; i++) {
		if (i*BYTES_PER_TIME > ReadNum)
			usReturnNum = ReadNum - (i-1)*BYTES_PER_TIME;
		else
			usReturnNum = BYTES_PER_TIME;

		wBuffer[1] = (StartNodeNo+usTotalReturnNum) >> 8;
		wBuffer[2] = (StartNodeNo+usTotalReturnNum)&0xff;

		ReCode = WriteReg(REG_CbAddrH, wBuffer[1]);
		ReCode = WriteReg(REG_CbAddrL, wBuffer[2]);

		ReCode = Comm_Base_IIC_IO(wBuffer, 1, pReadBuffer+usTotalReturnNum, usReturnNum);

		usTotalReturnNum += usReturnNum;

		if (ReCode != ERROR_CODE_OK)
			return ReCode;


	}

	return ReCode;
}




static unsigned char GetPanelRows(unsigned char *pPanelRows)
{
	return ReadReg(REG_TX_NUM, pPanelRows);
}




static unsigned char GetPanelCols(unsigned char *pPanelCols)
{
	return ReadReg(REG_RX_NUM, pPanelCols);
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



	for (i = 0; i < 3; i++) {
		ReCode = GetPanelRows(rBuffer);
		if (ReCode == ERROR_CODE_OK) {
			if (0 < rBuffer[0] && rBuffer[0] < 80) {
				g_stSCapConfEx.ChannelXNum = rBuffer[0];
				if (g_stSCapConfEx.ChannelXNum > g_ScreenSetParam.iUsedMaxTxNum) {
					FTS_TEST_ERROR("Failed to get Channel X number, Get num = %d, UsedMaxNum = %d",
								  g_stSCapConfEx.ChannelXNum, g_ScreenSetParam.iUsedMaxTxNum);
					g_stSCapConfEx.ChannelXNum = 0;
					return ERROR_CODE_INVALID_PARAM;
				}

				break;
			} else {
				SysDelay(150);
				continue;
			}
		} else {
			FTS_TEST_ERROR("Failed to get Channel X number");
			SysDelay(150);
		}
	}


	for (i = 0; i < 3; i++) {
		ReCode = GetPanelCols(rBuffer);
		if (ReCode == ERROR_CODE_OK) {
			if (0 < rBuffer[0] && rBuffer[0] < 80) {
				g_stSCapConfEx.ChannelYNum = rBuffer[0];
				if (g_stSCapConfEx.ChannelYNum > g_ScreenSetParam.iUsedMaxRxNum) {

					FTS_TEST_ERROR("Failed to get Channel Y number, Get num = %d, UsedMaxNum = %d",
								  g_stSCapConfEx.ChannelYNum, g_ScreenSetParam.iUsedMaxRxNum);
					g_stSCapConfEx.ChannelYNum = 0;
					return ERROR_CODE_INVALID_PARAM;
				}
				break;
			} else {
				SysDelay(150);
				continue;
			}
		} else {
			FTS_TEST_ERROR("Failed to get Channel Y number");
			SysDelay(150);
		}
	}


	for (i = 0; i < 3; i++) {
		unsigned char regData = 0;
		g_stSCapConfEx.KeyNum = 0;
		ReCode = ReadReg(FT8716_LEFT_KEY_REG, &regData);
		if (ReCode == ERROR_CODE_OK) {
			if (((regData >> 0) & 0x01)) {
				g_stSCapConfEx.bLeftKey1 = true;
				++g_stSCapConfEx.KeyNum;
			}
			if (((regData >> 1) & 0x01)) {
				g_stSCapConfEx.bLeftKey2 = true;
				++g_stSCapConfEx.KeyNum;
			}
			if (((regData >> 2) & 0x01)) {
				g_stSCapConfEx.bLeftKey3 = true;
				++g_stSCapConfEx.KeyNum;
			}
		} else {
			FTS_TEST_ERROR("Failed to get Key number");
			SysDelay(150);
			continue;
		}
		ReCode = ReadReg(FT8716_RIGHT_KEY_REG, &regData);
		if (ReCode == ERROR_CODE_OK) {
			if (((regData >> 0) & 0x01)) {
				g_stSCapConfEx.bRightKey1 = true;
				++g_stSCapConfEx.KeyNum;
			}
			if (((regData >> 1) & 0x01)) {
				g_stSCapConfEx.bRightKey2 = true;
				++g_stSCapConfEx.KeyNum;
			}
			if (((regData >> 2) & 0x01)) {
				g_stSCapConfEx.bRightKey3 = true;
				++g_stSCapConfEx.KeyNum;
			}
			break;
		} else {
			FTS_TEST_ERROR("Failed to get Key number");
			SysDelay(150);
			continue;
		}
	}



	FTS_TEST_DBG("CH_X = %d, CH_Y = %d, Key = %d",  g_stSCapConfEx.ChannelXNum , g_stSCapConfEx.ChannelYNum, g_stSCapConfEx.KeyNum);
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
	int i = 0;

	for (i = 0; i < 3; i++) {

		ReCode = EnterFactory();
		if (ERROR_CODE_OK != ReCode) {
			FTS_TEST_ERROR("Failed to Enter Factory Mode...");
			return ReCode;
		}



		if (0 == (g_stSCapConfEx.ChannelXNum + g_stSCapConfEx.ChannelYNum)) {
			ReCode = GetChannelNum();
			if (ERROR_CODE_OK != ReCode) {
				FTS_TEST_ERROR("Error Channel Num...");
				return ERROR_CODE_INVALID_PARAM;
			}
		}



	  ReCode = StartScan();
		if (ERROR_CODE_OK == ReCode) {
			FTS_TEST_INFO("scan pass...");
			break;
		}
	}
	if (i >= 3) {
		FTS_TEST_ERROR("scan fail...");
		return ReCode;
	}




	memset(m_RawData, 0, sizeof(m_RawData));
	memset(m_iTempRawData, 0, sizeof(m_iTempRawData));
	ReCode = ReadRawData(0, 0xAD, g_stSCapConfEx.ChannelXNum * g_stSCapConfEx.ChannelYNum * 2, m_iTempRawData);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERROR("Failed to Get RawData");
		return ReCode;
	}

	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; ++iRow) {
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {
			m_RawData[iRow][iCol] = m_iTempRawData[iRow * g_stSCapConfEx.ChannelYNum + iCol];
		}
	}


	memset(m_iTempRawData, 0, sizeof(m_iTempRawData));
	ReCode = ReadRawData(0, 0xAE, g_stSCapConfEx.KeyNumTotal * 2, m_iTempRawData);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERROR("Failed to Get RawData");
		return ReCode;
	}

	for (iCol = 0; iCol < g_stSCapConfEx.KeyNumTotal; ++iCol) {
		m_RawData[g_stSCapConfEx.ChannelXNum][iCol] = m_iTempRawData[iCol];
	}

	return ReCode;

}
unsigned char FT8716_GetTestResult(void)
{

	unsigned char ucDevice = 0;
	int iItemCount = 0;
	unsigned char ucResultData = 0;


	for (iItemCount = 0; iItemCount < g_TestItemNum; iItemCount++) {

		if (Code_FT8716_RAWDATA_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
		   ) {
			if (RESULT_PASS == g_stTestItem[ucDevice][iItemCount].TestResult)
				ucResultData |= 0x01<<2;
		}


		if (Code_FT8716_CB_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode) {
			if (RESULT_PASS == g_stTestItem[ucDevice][iItemCount].TestResult)
				ucResultData |= 0x01<<1;
		}


		if (Code_FT8716_SHORT_CIRCUIT_TEST == g_stTestItem[ucDevice][iItemCount].ItemCode
			) {
			if (RESULT_PASS == g_stTestItem[ucDevice][iItemCount].TestResult)
				ucResultData |= 0x01;
		}
	}

	FTS_TEST_DBG("Test_result:  0x%02x", ucResultData);





	return ucResultData;
}

#endif
