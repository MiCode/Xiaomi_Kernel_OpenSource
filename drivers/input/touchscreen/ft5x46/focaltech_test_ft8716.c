/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT8716.c
*
* Author: Software Development
*
* Created: 2015-12-24
*
* Abstract: test item for FT8716
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input/ft5x46_ts.h>

#include "focaltech_test.h"

#define IC_TEST_VERSION  "Test version: V1.0.0--2015-12-24, (sync version of FT_MultipleTest: V2.9.0.1--2015-12-22)"

#define DEVIDE_MODE_ADDR		0x00
#define REG_TX_NUM				0x02
#define REG_RX_NUM				0x03
#define FT8716_LEFT_KEY_REG		0X1E
#define FT8716_RIGHT_KEY_REG	0X1F

#define REG_CbAddrH				0x18
#define REG_CbAddrL				0x19
#define REG_CbBuf0				0x6E
#define REG_CLB					0x04

static int m_CBData[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} };
static BYTE m_ucTempData[TX_NUM_MAX * RX_NUM_MAX*2] = {0};
static unsigned char pReadBuffer[80 * 80 * 2] = {0};
static int iAdcData[TX_NUM_MAX * RX_NUM_MAX] =  {0};
static int shortRes[TX_NUM_MAX][RX_NUM_MAX] = { {0} };
static struct StruScreenSeting {
	int iTxNum;
	int iRxNum;
	int isNormalize;
	int iUsedMaxTxNum;
	int iUsedMaxRxNum;
	unsigned char iChannelsNum;
	unsigned char iKeyNum;
} g_ScreenSetParam;

static struct structSCapConfEx {
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
} g_stSCapConfEx;

static unsigned char GetPanelRows(unsigned char *pPanelRows);
static unsigned char GetPanelCols(unsigned char *pPanelCols);
static unsigned char GetTxRxCB(unsigned short StartNodeNo, unsigned short ReadNum, unsigned char *pReadBuffer);
static unsigned char GetChannelNum(void);
static int InitTest(void);
static unsigned char WeakShort_GetAdcData(int AllAdcDataLen, int *pRevBuffer);
static unsigned char ChipClb(unsigned char *pClbResult);
static void set_max_channel_num(void)
{
	g_ScreenSetParam.iUsedMaxTxNum = TX_NUM_MAX;
	g_ScreenSetParam.iUsedMaxRxNum = RX_NUM_MAX;

}
static int InitTest(void)
{

	g_stSCapConfEx.ChannelXNum = 0;
	g_stSCapConfEx.ChannelYNum = 0;
	g_stSCapConfEx.KeyNum = 0;
	g_stSCapConfEx.KeyNumTotal = 6;
	set_max_channel_num();
	return 0;
}

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

static unsigned char GetChannelNum(void)
{
	unsigned char ReCode;
	int i;
	unsigned char rBuffer[1];

	for (i = 0; i < 3; i++) {
		ReCode = GetPanelRows(rBuffer);
		if (ReCode == ERROR_CODE_OK) {
			if (0 < rBuffer[0] && rBuffer[0] < 80) {
				g_stSCapConfEx.ChannelXNum = rBuffer[0];
				if (g_stSCapConfEx.ChannelXNum > g_ScreenSetParam.iUsedMaxTxNum) {
					FTS_TEST_ERR("Failed to get Channel X number, Get num = %d, UsedMaxNum = %d",
						g_stSCapConfEx.ChannelXNum, g_ScreenSetParam.iUsedMaxTxNum);
					g_stSCapConfEx.ChannelXNum = 0;
					return ERROR_CODE_INVALID_PARAM;
				}
				break;
			}
			SysDelay(150);
			continue;
		} else {
			FTS_TEST_DBG("Failed to get Channel X number");
			SysDelay(150);
		}
	}

	for (i = 0; i < 3; i++) {
		ReCode = GetPanelCols(rBuffer);
		if (ReCode == ERROR_CODE_OK) {
			if (0 < rBuffer[0] && rBuffer[0] < 80) {
				g_stSCapConfEx.ChannelYNum = rBuffer[0];
				if (g_stSCapConfEx.ChannelYNum > g_ScreenSetParam.iUsedMaxRxNum) {

					FTS_TEST_ERR("Failed to get Channel Y number, Get num = %d, UsedMaxNum = %d",
						g_stSCapConfEx.ChannelYNum, g_ScreenSetParam.iUsedMaxRxNum);
					g_stSCapConfEx.ChannelYNum = 0;
					return ERROR_CODE_INVALID_PARAM;
				}
				break;
			}
			SysDelay(150);
			continue;
		} else {
			FTS_TEST_DBG("Failed to get Channel Y number");
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
			FTS_TEST_DBG("Failed to get Key number");
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
		}
		FTS_TEST_DBG("Failed to get Key number");
		SysDelay(150);
		continue;
	}

	FTS_TEST_DBG("CH_X = %d, CH_Y = %d, Key = %d",  g_stSCapConfEx.ChannelXNum, g_stSCapConfEx.ChannelYNum, g_stSCapConfEx.KeyNum);
	return ReCode;
}
unsigned char FT8716_TestItem_OpenTest(struct i2c_client *client)
{
	unsigned char ReCode = ERROR_CODE_OK;
	unsigned char chValue = 0xff;
	u16 iMin = 0;
	u16 iMax = 0;
	int iRow = 0;
	int iCol = 0;
	int iValue = 0;
	int btestresult = RESULT_PASS;
	unsigned char bClbResult;
	struct ft5x46_data *ft5x46 = i2c_get_clientdata(client);
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	FTS_TEST_DBG("\r\n\r\n======Test Item: --------  Open Test");
	InitTest();
	ReCode = EnterFactory();
	SysDelay(50);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERR("\r\n//=========  Enter Factory Failed!");
		goto TEST_ERR;
	}

	ReCode = ReadReg(0x20, &chValue);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERR("\r\n//=========  Read Reg Failed!");
		goto TEST_ERR;
	}

	ReCode = WriteReg(0x20, 0x02);
	SysDelay(50);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERR("\r\n//=========  Write Reg Failed!");
		goto TEST_ERR;
	}

	ReCode = ChipClb(&bClbResult);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERR("\r\n//========= auto clb Failed!");
		goto TEST_ERR;
	}

	if (0 == (g_stSCapConfEx.ChannelXNum + g_stSCapConfEx.ChannelYNum)) {
		ReCode = GetChannelNum();
		if (ERROR_CODE_OK != ReCode)
			FTS_TEST_ERR("Error Channel Num...");
	}
	ReCode = GetTxRxCB(0, (short)(g_stSCapConfEx.ChannelXNum * g_stSCapConfEx.ChannelYNum + g_stSCapConfEx.KeyNum), m_ucTempData);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERR("\r\n\r\n//=========get CB Failed!");
		goto TEST_ERR;
	}

	memset(m_CBData, 0, sizeof(m_CBData));
	FTS_TEST_DBG("%s,xnum:%d,ymun:%d", __func__, g_stSCapConfEx.ChannelXNum, g_stSCapConfEx.ChannelYNum);
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; ++iRow)
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol)
			m_CBData[iRow][iCol] = m_ucTempData[iRow * g_stSCapConfEx.ChannelYNum + iCol];
	FTS_TEST_DBG("CBData:\n");
	for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {
		for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; ++iRow)
			FTS_TEST_PRINT("%-4d  ", m_CBData[iRow][iCol]);
		FTS_TEST_PRINT("\n");
	}
	iMin = pdata->open_min;
	iMax = pdata->open_max;
	FTS_TEST_ERR("Short Circuit test , Set_Range=(%d, %d).\n", iMin, iMax);
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; ++iRow)
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {
			iValue =  m_CBData[iRow][iCol];
			if (iValue < iMin || iValue > iMax) {
				btestresult = RESULT_NG;
				FTS_TEST_PRINT(" Open test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d).\n", iRow+1, iCol+1, iValue, iMin, iMax);
			}
		}
	ReCode = WriteReg(0x20, chValue);
	SysDelay(50);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERR("\r\n//=========  Write Reg Failed!");
		goto TEST_ERR;
	}
	return btestresult;
TEST_ERR:
	FTS_TEST_DBG("\n\n//Open Test is invalid!");
	return RESULT_INVALID;
}

unsigned char FT8716_TestItem_ShortCircuitTest(struct i2c_client *client)
{

	unsigned char ReCode = ERROR_CODE_OK;
	int iAllAdcDataNum = 0;
	unsigned char iTxNum = 0, iRxNum = 0, iChannelNum = 0;
	int iRow = 0;
	int iCol = 0;
	int i = 0;
	int tmpAdc = 0;
	u16 iValueMin = 0;
	u32 iValueMax = 0;
	int iValue = 0;
	int btestresult = RESULT_PASS;
	struct ft5x46_data *ft5x46 = i2c_get_clientdata(client);
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	FTS_TEST_DBG("====Test Item: -------- Short Circuit Test \r\n");
	InitTest();
	ReCode = EnterFactory();
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERR(" Failed to Enter factory mode. Error Code: %d", ReCode);
		goto TEST_END;
	}

	ReCode = ReadReg(0x02, &iTxNum);
	ReCode = ReadReg(0x03, &iRxNum);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERR("// Failed to read reg. Error Code: %d", ReCode);
		goto TEST_END;
	}
	FTS_TEST_ERR(" iTxNum:%d.  iRxNum:%d.", iTxNum, iRxNum);
	iChannelNum = iTxNum + iRxNum;
	iAllAdcDataNum = iTxNum * iRxNum + g_stSCapConfEx.KeyNumTotal;
	memset(iAdcData, 0, sizeof(iAdcData));
	for (i = 0; i < 1; i++) {
		ReCode = WeakShort_GetAdcData(iAllAdcDataNum*2, iAdcData);
		SysDelay(50);
		if (ERROR_CODE_OK != ReCode) {
			FTS_TEST_ERR(" // Failed to get AdcData. Error Code: %d", ReCode);
			goto TEST_END;
		}
	}
	FTS_TEST_DBG("ADCData:\n");
	for (i = 0; i < iAllAdcDataNum; i++) {
		FTS_TEST_PRINT("%-4d  ", iAdcData[i]);
		if (0 == (i+1)%iRxNum)
			FTS_TEST_PRINT("\n");
	}
	FTS_TEST_PRINT("\n");
	FTS_TEST_DBG("shortRes data:\n");
	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum + 1; ++iRow) {
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; ++iCol) {
			tmpAdc = iAdcData[iRow * iRxNum + iCol];
			if (tmpAdc > 2007)
				tmpAdc = 2007;
			shortRes[iRow][iCol] = (tmpAdc * 100) / (2047 - tmpAdc);
			FTS_TEST_PRINT("%-4d  ", shortRes[iRow][iCol]);
		}
		FTS_TEST_PRINT("\n");
	}
	FTS_TEST_PRINT("\n");
	iValueMin = pdata->short_min;
	iValueMax = pdata->short_max;
	FTS_TEST_ERR("Short Circuit test , Set_Range=(%d, %u).\n", iValueMin, iValueMax);

	for (iRow = 0; iRow < g_stSCapConfEx.ChannelXNum; iRow++) {
		for (iCol = 0; iCol < g_stSCapConfEx.ChannelYNum; iCol++) {
			iValue = shortRes[iRow][iCol];
			if (iValue < iValueMin || iValue > iValueMax) {
				btestresult = RESULT_NG;
				FTS_TEST_PRINT(" Short Circuit test failure. Node=(%d, %d), Get_value=%d\n", iRow+1, iCol+1, iValue);
			}
		}
	}
	return btestresult;
TEST_END:
	return RESULT_INVALID;
}

static unsigned char WeakShort_GetAdcData(int AllAdcDataLen, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_OK;
	unsigned char RegMark = 0;
	int index = 0;
	int i = 0;
	int usReturnNum = 0;
	unsigned char wBuffer[2] = {0};
	int iReadNum = AllAdcDataLen / BYTES_PER_TIME;

	FTS_TEST_DBG("");

	memset(wBuffer, 0, sizeof(wBuffer));
	wBuffer[0] = 0x89;

	if ((AllAdcDataLen % BYTES_PER_TIME) > 0)
		++iReadNum;

	ReCode = WriteReg(0x0F, 1);
	for (index = 0; index < 50; ++index) {
		SysDelay(50);
		ReCode = ReadReg(0x10, &RegMark);
		if (ERROR_CODE_OK == ReCode && 0 == RegMark)
			break;
	}
	if (index >= 50) {
		FTS_TEST_ERR("ReadReg failed, ADC data not OK.");
		return 6;
	}
	usReturnNum = BYTES_PER_TIME;
	if (ReCode == ERROR_CODE_OK)
		ReCode = Comm_Base_IIC_IO(wBuffer, 1, pReadBuffer, usReturnNum);

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERR("Comm_Base_IIC_IO  error.   !!!");
			break;
		}
		if (i == iReadNum-1) {
			usReturnNum = AllAdcDataLen-BYTES_PER_TIME*i;
			ReCode = Comm_Base_IIC_IO(NULL, 0, pReadBuffer+BYTES_PER_TIME*i, usReturnNum);
		} else {
			usReturnNum = BYTES_PER_TIME;
			ReCode = Comm_Base_IIC_IO(NULL, 0, pReadBuffer+BYTES_PER_TIME*i, usReturnNum);
		}
	}

	for (index = 0; index < AllAdcDataLen/2; ++index)
		pRevBuffer[index] = (pReadBuffer[index * 2] << 8) + pReadBuffer[index * 2 + 1];
	FTS_TEST_DBG(" END.\n");
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
			}
		}
		if (TimeOutTimes == 0)
			*pClbResult = 0;
	}
	return ReCode;
}
