/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R),All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_ft8006.c
*
* Author: Software Development
*
* Created: 2016-08-01
*
* Abstract: test item for FT8006
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "../include/focaltech_test_detail_threshold.h"
#include "../include/focaltech_test_supported_ic.h"
#include "../include/focaltech_test_main.h"
#include "../focaltech_test_config.h"
#include "../../focaltech_core.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#if (FTS_CHIP_TEST_TYPE == FT8006_TEST)


#define DEVIDE_MODE_ADDR                0x00
#define REG_LINE_NUM                    0x01
#define REG_TX_NUM                      0x02
#define REG_RX_NUM                      0x03
#define FT8006_LEFT_KEY_REG             0X1E
#define FT8006_RIGHT_KEY_REG            0X1F

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

#define REG_FACTORYID       				0xA8

#define pre                                 1
#define REG_CLB                          0x04

#define DOUBLE_TX_NUM_MAX   160
#define DOUBLE_RX_NUM_MAX   160

#define REG_8006_LCD_NOISE_FRAME  0X12
#define REG_8006_LCD_NOISE_START  0X11
#define REG_8006_LCD_NOISE_NUMBER 0X13



/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

enum NOISE_TYPE {
	NT_AvgData = 0,
	NT_MaxData = 1,
	NT_MaxDevication = 2,
	NT_DifferData = 3,
};

enum CS_TYPE {
	CS_TWO_SINGLE_MASTER = 1,
	CS_TWO_SINGLE_SLAVE = 0,
	CS_SINGLE_CHIP = 3,
};
enum CS_DIRECTION {
	CS_LEFT_RIGHT = 0x00,
	CS_UP_DOWN    = 0x40,
};
enum CD_S0_ROLE {
	CS_S0_AS_MASTER = 0x00,
	CS_S0_AS_SLAVE  = 0x80,
};

struct CSInfoPacket {
	enum CS_TYPE             csType;
	enum CS_DIRECTION   csDirection;
	enum CD_S0_ROLE      csS0Role;
	BYTE                  csMasterAddr;
	BYTE                  csSalveAddr;
	BYTE                  csMarterTx;
	BYTE                  csMasterRx;
	BYTE                  csSlaveTx;
	BYTE                  csSlaveRx;
};

struct FT8006 {
	struct CSInfoPacket *m_csInfo;
	unsigned char  m_currentSlaveAddr;

};

struct CSChipAddrMgr {
	struct FT8006 *m_parent;
	BYTE m_slaveAddr;
};



/*******************************************************************************
* Static variables
*******************************************************************************/

static int m_RawData[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} } ;
static int m_CBData[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} } ;
static BYTE m_ucTempData[TX_NUM_MAX * RX_NUM_MAX*2] = {0};
static int m_iTempRawData[TX_NUM_MAX * RX_NUM_MAX] = {0};

static int bufferMaster[TX_NUM_MAX * RX_NUM_MAX] = {0};
static int bufferSlave[TX_NUM_MAX * RX_NUM_MAX] = {0};
static int bufferCated[DOUBLE_TX_NUM_MAX * DOUBLE_RX_NUM_MAX] = {0};
static int iAdcData[TX_NUM_MAX * RX_NUM_MAX] =  {0};
static unsigned char pReadBuffer[80 * 80 * 2] = {0};
static int shortRes[TX_NUM_MAX][RX_NUM_MAX] = { {0} };
static unsigned char ScreenNoiseData[TX_NUM_MAX*RX_NUM_MAX*2] = {0};
static int LCD_Noise[TX_NUM_MAX][RX_NUM_MAX] = {{0, 0} } ;



/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
extern struct stCfg_FT8006_BasicThreshold g_stCfg_FT8006_BasicThreshold;


/*******************************************************************************
* Static function prototypes
*******************************************************************************/


static int StartScan(void);
static unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer);
static unsigned char GetPanelRows(unsigned char *pPanelRows);
static unsigned char GetPanelCols(unsigned char *pPanelCols);
static unsigned char GetTxRxCB(unsigned short StartNodeNo, unsigned short ReadNum, unsigned char *pReadBuffer);
static unsigned char GetRawData(struct FT8006 *g_FT8006);
static unsigned char GetChannelNum(void);
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount);
static unsigned char ChipClb(unsigned char *pClbResult);
void CatSingleToOneScreen(struct FT8006 *g_FT8006);
void ReleaseCSInfo(struct FT8006 *g_FT8006);
unsigned char FT8006_EnterFactory(struct FT8006 *g_FT8006);
unsigned char FT8006_ChipClb(struct FT8006 *g_FT8006, unsigned char *pClbResult);
static unsigned char FT8006_GetTxRxCB(struct FT8006 *g_FT8006, unsigned short StartNodeNo, unsigned short ReadNum, unsigned char *pReadBuffer);
unsigned char FT8006_WeakShort_GetAdcData(struct FT8006 *g_FT8006, int AllAdcDataLen, int *pRevBuffer);
unsigned char ReadBytesByKey(BYTE key, int ByteNum, unsigned char *byteData);
unsigned char FT8006_EnterWork(struct FT8006 *g_FT8006);
int HY_SetSlaveAddr(struct FT8006 *g_FT8006, BYTE SlaveAddr);
unsigned char FT8006_WriteReg(struct FT8006 *g_FT8006, unsigned char RegAddr, unsigned char RegData);
bool ChipHasTwoHeart(struct FT8006 *g_FT8006);
unsigned char FT8006_StartScan(struct FT8006 *g_FT8006);
unsigned char FT8006_ReadRawData(struct FT8006 *g_FT8006, unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer);
unsigned char WeakShort_StartAdcScan(void);
unsigned char WeakShort_GetScanResult(void);
unsigned char WeakShort_GetAdcResult(int AllAdcDataLen, int *pRevBuffer);
static unsigned char WeakShort_GetAdcData(int AllAdcDataLen, int *pRevBuffer);
void InitCurrentAddress(struct FT8006 *g_FT8006);
void CSInfoPacket(struct CSInfoPacket *cspt);




boolean FT8006_StartTest(void);
int FT8006M_get_test_data(char *pTestData);

unsigned char FT8006_TestItem_EnterFactoryMode(struct FT8006 *g_FT8006);
unsigned char FT8006_TestItem_RawDataTest(struct FT8006 *g_FT8006, bool *bTestResult);
unsigned char FT8006_TestItem_CbTest(struct FT8006  *g_FT8006, bool *bTestResult);
unsigned char FT8006_TestItem_ShortCircuitTest(struct FT8006  *g_FT8006, bool *bTestResult);
unsigned char FT8006_TestItem_LCDNoiseTest(struct FT8006  *g_FT8006, bool *bTestResult);
unsigned char FT8006_TestItem_ChannelsTest(struct FT8006 *g_FT8006, bool *bTestResult);
unsigned char FT8006_CheckItem_FactoryIdTest(struct FT8006  *g_FT8006, bool *bTestResult);
/************************************************************************
* Name: FT8006_StartTest
* Brief:  Test entry. Determine which test item to test
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/

boolean FT8006_StartTest()
{
	bool bTestResult = true, bTempResult = 1;
	unsigned char ReCode;
	unsigned char ucDevice = 0;
	int iItemCount = 0;

	struct FT8006  g_FT8006;
	struct CSInfoPacket cspt;



	CSInfoPacket(&cspt);
	InitCurrentAddress(&g_FT8006);
	g_FT8006.m_csInfo = &cspt;



	if (Ft8006m_InitTest() < 0) {
		FTS_TEST_ERROR("[focal] Failed to init test.");
		return false;
	}


	if (0 == ft8006m_g_TestItemNum)
		bTestResult = false;


	for (iItemCount = 0; iItemCount < ft8006m_g_TestItemNum; iItemCount++) {
		ft8006m_m_ucTestItemCode = ft8006m_g_stTestItem[ucDevice][iItemCount].ItemCode;


		if (Code_FT8006_ENTER_FACTORY_MODE == ft8006m_g_stTestItem[ucDevice][iItemCount].ItemCode
		  ) {
			ReCode = FT8006_TestItem_EnterFactoryMode(&g_FT8006);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
				break;
			} else
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8006_RAWDATA_TEST == ft8006m_g_stTestItem[ucDevice][iItemCount].ItemCode
		  ) {
			ReCode = FT8006_TestItem_RawDataTest(&g_FT8006, &bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8006_CB_TEST == ft8006m_g_stTestItem[ucDevice][iItemCount].ItemCode
		  ) {
			ReCode = FT8006_TestItem_CbTest(&g_FT8006, &bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8006_CHANNEL_NUM_TEST == ft8006m_g_stTestItem[ucDevice][iItemCount].ItemCode) {
			ReCode = FT8006_TestItem_ChannelsTest(&g_FT8006, &bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
				break;
			} else
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;

		}


		if (Code_FT8006_SHORT_CIRCUIT_TEST == ft8006m_g_stTestItem[ucDevice][iItemCount].ItemCode
		  ) {
			ReCode = FT8006_TestItem_ShortCircuitTest(&g_FT8006, &bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8006_LCD_NOISE_TEST == ft8006m_g_stTestItem[ucDevice][iItemCount].ItemCode
		  ) {
			ReCode = FT8006_TestItem_LCDNoiseTest(&g_FT8006, &bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}


		if (Code_FT8006_FACTORY_ID_TEST == ft8006m_g_stTestItem[ucDevice][iItemCount].ItemCode) {
			ReCode = FT8006_CheckItem_FactoryIdTest(&g_FT8006, &bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_NG;
			} else
				ft8006m_g_stTestItem[ucDevice][iItemCount].TestResult = RESULT_PASS;
		}

	}


	Ft8006m_FinishTest();


	return bTestResult;

}


/************************************************************************
* Name:
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
void InitCurrentAddress(struct FT8006 *g_FT8006)
{
	g_FT8006->m_currentSlaveAddr = 0x70;
}
struct CSInfoPacket *GetCSInfo(struct FT8006 *g_FT8006)
{
	return g_FT8006->m_csInfo;
}
unsigned char  GetCurrentAddr(struct FT8006 *g_FT8006)
{
	return g_FT8006->m_currentSlaveAddr;
}
int GetCSType(struct CSInfoPacket *cspt)
{
	return cspt->csType;
}
int GetCSDirection(struct CSInfoPacket *cspt)
{
	return cspt->csDirection;
}
int GetS0Role(struct CSInfoPacket *cspt)
{
	return cspt->csS0Role;
}
void SetMasterAddr(struct CSInfoPacket *cspt,  BYTE master)
{
	cspt->csMasterAddr = master;
}
BYTE GetMasterAddr(struct CSInfoPacket *cspt)
{
	return cspt->csMasterAddr;
}
void SetSlaveAddr(struct CSInfoPacket *cspt,  BYTE slave)
{
	cspt->csSalveAddr = slave;
}
BYTE GetSlaveAddr(struct CSInfoPacket *cspt)
{
	return cspt->csSalveAddr;
}
void SetMasterTx(struct CSInfoPacket *cspt,  BYTE tx)
{
	cspt->csMarterTx = tx;
}
BYTE GetMasterTx(struct CSInfoPacket *cspt)
{
	return cspt->csMarterTx;
}
void SetMasterRx(struct CSInfoPacket *cspt,  BYTE rx)
{
	cspt->csMasterRx = rx;
}
BYTE GetMasterRx(struct CSInfoPacket *cspt)
{
	return cspt->csMasterRx;
}
void SetSlaveTx(struct CSInfoPacket *cspt,  BYTE tx)
{
	cspt->csSlaveTx = tx;
}
BYTE GetSlaveTx(struct CSInfoPacket *cspt)
{
	return cspt->csSlaveTx;
}
void SetSlaveRx(struct CSInfoPacket *cspt, BYTE rx)
{
	cspt->csSlaveRx = rx;
}
BYTE GetSlaveRx(struct CSInfoPacket *cspt)
{
	return cspt->csSlaveRx;
}
void Initialize(struct CSInfoPacket *cspt,  BYTE bRegCfg)
{
	cspt->csType          = (enum CS_TYPE)(bRegCfg & 0x3F);
	cspt->csDirection     = (enum CS_DIRECTION)(bRegCfg & 0x40);
	cspt->csS0Role        = (enum CD_S0_ROLE)(bRegCfg & 0x80);
	cspt->csMasterAddr    = 0x70;
	cspt->csSalveAddr     = 0x72;
	cspt->csMarterTx      = 0;
	cspt->csMasterRx      = 0;
	cspt->csSlaveTx       = 0;
	cspt->csSlaveRx       = 0;
}
void CSInfoPacket(struct CSInfoPacket *cspt)
{
	cspt->csType          = CS_SINGLE_CHIP;
	cspt->csDirection     = CS_UP_DOWN;
	cspt->csS0Role        = CS_S0_AS_MASTER;
	cspt->csMasterAddr    = 0x72;
	cspt->csSalveAddr     = 0x70;
	cspt->csMarterTx      = 0;
	cspt->csMasterRx      = 0;
	cspt->csSlaveTx       = 0;
	cspt->csSlaveRx       = 0;
}


void ReleaseCSInfo(struct FT8006  *g_FT8006)
{

	memset(g_FT8006->m_csInfo, 0, sizeof(struct CSInfoPacket));

}

void UpDateCSInfo(struct FT8006  *g_FT8006)
{
	unsigned char ReCode = ERROR_CODE_OK, RegValue = 0;

	ReCode = Ft8006m_ReadReg(0x26, &RegValue);
	Initialize(g_FT8006->m_csInfo, RegValue);

	ReCode = Ft8006m_ReadReg(0x50, &RegValue);
	SetMasterTx(g_FT8006->m_csInfo,  RegValue);

	ReCode = Ft8006m_ReadReg(0x51, &RegValue);
	SetMasterRx(g_FT8006->m_csInfo,  RegValue);

	ReCode = Ft8006m_ReadReg(0x52, &RegValue);
	SetSlaveTx(g_FT8006->m_csInfo,  RegValue);

	ReCode = Ft8006m_ReadReg(0x53, &RegValue);
	SetSlaveRx(g_FT8006->m_csInfo,  RegValue);

	ReCode = Ft8006m_WriteReg(0x17, 0);
	ReCode = Ft8006m_ReadReg(0x81, &RegValue);
	SetMasterAddr(g_FT8006->m_csInfo,  RegValue);

	ReCode = Ft8006m_WriteReg(0x17, 12);
	ReCode = Ft8006m_ReadReg(0x81, &RegValue);
	SetSlaveAddr(g_FT8006->m_csInfo,  RegValue);

	FTS_TEST_DBG("csType = %d, csDirection = %d, csS0Role = %d, csMasterAddr = 0x%x, csSalveAddr =  0x%x,\
	csMarterTx = %d, csMasterRx = %d, csSlaveTx = %d, csSlaveRx = %d.",
				g_FT8006->m_csInfo->csType,
				g_FT8006->m_csInfo->csDirection,
				g_FT8006->m_csInfo->csS0Role,
				g_FT8006->m_csInfo->csMasterAddr,
				g_FT8006->m_csInfo->csSalveAddr,
				g_FT8006->m_csInfo->csMarterTx,
				g_FT8006->m_csInfo->csMasterRx,
				g_FT8006->m_csInfo->csSlaveTx,
				g_FT8006->m_csInfo->csSlaveRx
				);

}

bool ChipHasTwoHeart(struct FT8006  *g_FT8006)
{
	if (GetCSType(g_FT8006->m_csInfo) == CS_SINGLE_CHIP)
		return false;

	return true;
}
void CSChipAddrMgr_Init(struct CSChipAddrMgr *pMgr, struct FT8006 *parent)
{

	pMgr->m_parent = parent;
	pMgr->m_slaveAddr = GetCurrentAddr(parent);

}

void CSChipAddrMgr_Exit(struct CSChipAddrMgr *pMgr)
{

	if (pMgr->m_slaveAddr != GetCurrentAddr(pMgr->m_parent)) {

		HY_SetSlaveAddr(pMgr->m_parent, pMgr->m_slaveAddr);
	}


}

int HY_SetSlaveAddr(struct FT8006 *g_FT8006,  BYTE SlaveAddr)
{
	unsigned char value = 0;
	unsigned char ReCode = ERROR_CODE_OK;
	int tmp = 0;

	g_FT8006->m_currentSlaveAddr = SlaveAddr;
	tmp = ft8006m_i2c_client->addr;

	FTS_TEST_DBG("Original i2c addr 0x%x ", ft8006m_i2c_client->addr);
	FTS_TEST_DBG("CurrentAddr 0x%x ", (SlaveAddr>>1));

	if (ft8006m_i2c_client->addr != (SlaveAddr>>1)) {
		ft8006m_i2c_client->addr = (SlaveAddr>>1);
		FTS_TEST_DBG("Change i2c addr 0x%x to 0x%x", tmp, ft8006m_i2c_client->addr);
	}


	ReCode = Ft8006m_ReadReg(0x20, &value);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERROR("[focal] Ft8006m_ReadReg Error, code: %d ",  ReCode);
	} else {
		FTS_TEST_DBG("[focal] Ft8006m_ReadReg successed, Addr: 0x20, value: 0x%02x ",  value);
	}


	return 0;
}

void WorkAsMaster(struct CSChipAddrMgr *pMgr)
{
	struct CSInfoPacket *pcs = GetCSInfo(pMgr->m_parent);

	if (GetMasterAddr(pcs) != GetCurrentAddr(pMgr->m_parent)) {
		HY_SetSlaveAddr(pMgr->m_parent, GetMasterAddr(pcs));
	}

}

void WorkAsSlave(struct CSChipAddrMgr *pMgr)
{
	struct CSInfoPacket *pcs = GetCSInfo(pMgr->m_parent);

	if (GetSlaveAddr(pcs) != GetCurrentAddr(pMgr->m_parent)) {
		HY_SetSlaveAddr(pMgr->m_parent, GetSlaveAddr(pcs));
	}
}

void CatSingleToOneScreen(struct FT8006 *g_FT8006)
{
	int leftChannelNum = 0, rightChannelNum = 0;
	int upChannelNum = 0, downChannelNum = 0;
	unsigned char splitRx = 0;
	unsigned char splitTx = 0;
	int iRow = 0, iCol = 0;
	int iRelativeRx = 0;
	int iRelativeTx = 0;
	int iTotalRx = 0;
	int iTotalTx = 0;
	int  *bufferLeft = NULL;
	int  *bufferRight = NULL;
	int  *bufferUp = NULL;
	int  *bufferDown = NULL;

	FTS_TEST_FUNC_ENTER();


	if (CS_SINGLE_CHIP == GetCSType(g_FT8006->m_csInfo))
		return;


	if (CS_LEFT_RIGHT == GetCSDirection(g_FT8006->m_csInfo)) {
		bufferLeft = Ft8006m_fts_malloc(TX_NUM_MAX * RX_NUM_MAX*sizeof(int));
		bufferRight = Ft8006m_fts_malloc(TX_NUM_MAX * RX_NUM_MAX*sizeof(int));

		if (CS_S0_AS_MASTER == GetS0Role(g_FT8006->m_csInfo)) {
			splitRx =  GetMasterRx(g_FT8006->m_csInfo);
			bufferLeft  = bufferMaster;
			bufferRight = bufferSlave;
			leftChannelNum  = GetMasterTx(g_FT8006->m_csInfo) * GetMasterRx(g_FT8006->m_csInfo);
			rightChannelNum = GetSlaveTx(g_FT8006->m_csInfo) *  GetSlaveRx(g_FT8006->m_csInfo);
		} else {
			splitRx = GetSlaveRx(g_FT8006->m_csInfo);
			bufferLeft  = bufferSlave;
			bufferRight = bufferMaster;
			leftChannelNum  = GetSlaveTx(g_FT8006->m_csInfo) * GetSlaveRx(g_FT8006->m_csInfo);
			rightChannelNum = GetMasterTx(g_FT8006->m_csInfo) *  GetMasterRx(g_FT8006->m_csInfo);
		}

		for (iRow = 0; iRow < GetMasterTx(g_FT8006->m_csInfo); ++iRow) {
			for (iCol = 0; iCol < GetMasterRx(g_FT8006->m_csInfo) + GetSlaveRx(g_FT8006->m_csInfo); ++iCol) {
				iRelativeRx = iCol - splitRx;
				iTotalRx = GetMasterRx(g_FT8006->m_csInfo) + GetSlaveRx(g_FT8006->m_csInfo);
				if (iCol >= splitRx) {

					bufferCated[iRow * iTotalRx + iCol] = bufferRight[iRow * (iTotalRx - splitRx) + iRelativeRx];
				} else {

					bufferCated[iRow * iTotalRx + iCol] = bufferLeft[iRow * splitRx + iCol];
				}
			}
		}

		memcpy(bufferCated + GetMasterTx(g_FT8006->m_csInfo) * (GetMasterRx(g_FT8006->m_csInfo) + GetSlaveRx(g_FT8006->m_csInfo)),  bufferLeft + leftChannelNum, 6);
		memcpy(bufferCated + GetMasterTx(g_FT8006->m_csInfo) * (GetMasterRx(g_FT8006->m_csInfo) + GetSlaveRx(g_FT8006->m_csInfo)) + 6,  bufferRight + rightChannelNum, 6);
	}

	else if (CS_UP_DOWN == GetCSDirection(g_FT8006->m_csInfo)) {
		bufferUp = Ft8006m_fts_malloc(TX_NUM_MAX * RX_NUM_MAX*sizeof(int));
		bufferDown = Ft8006m_fts_malloc(TX_NUM_MAX * RX_NUM_MAX*sizeof(int));

		if (CS_S0_AS_MASTER == GetS0Role(g_FT8006->m_csInfo)) {
			splitTx = GetMasterTx(g_FT8006->m_csInfo);
			bufferUp  = bufferMaster;
			bufferDown = bufferSlave;
			upChannelNum  = GetMasterTx(g_FT8006->m_csInfo) * GetMasterRx(g_FT8006->m_csInfo);
			downChannelNum = GetSlaveTx(g_FT8006->m_csInfo) *  GetSlaveRx(g_FT8006->m_csInfo);
		} else {
			splitTx = GetSlaveTx(g_FT8006->m_csInfo);
			bufferUp  = bufferSlave;
			bufferDown = bufferMaster;
			upChannelNum  = GetSlaveTx(g_FT8006->m_csInfo) * GetSlaveRx(g_FT8006->m_csInfo);
			downChannelNum = GetMasterTx(g_FT8006->m_csInfo) *  GetMasterRx(g_FT8006->m_csInfo);
		}
		for (iRow = 0; iRow < GetMasterTx(g_FT8006->m_csInfo) + GetSlaveTx(g_FT8006->m_csInfo); ++iRow) {
			for (iCol = 0; iCol < GetMasterRx(g_FT8006->m_csInfo); ++iCol) {
				iRelativeTx = iRow - splitTx;
				iTotalTx = GetMasterTx(g_FT8006->m_csInfo) + GetSlaveTx(g_FT8006->m_csInfo);
				if (iRow >= splitTx) {

					bufferCated[iRow * GetMasterRx(g_FT8006->m_csInfo) + iCol] = bufferDown[iRelativeTx * GetMasterRx(g_FT8006->m_csInfo) + iCol];
				} else {

					bufferCated[iRow * GetMasterRx(g_FT8006->m_csInfo) + iCol] = bufferUp[iRow * GetMasterRx(g_FT8006->m_csInfo) + iCol];
				}
			}
		}

		memcpy(bufferCated + (GetMasterTx(g_FT8006->m_csInfo) + GetSlaveTx(g_FT8006->m_csInfo)) * GetMasterRx(g_FT8006->m_csInfo),  bufferUp + upChannelNum, 6);
		memcpy(bufferCated + (GetMasterTx(g_FT8006->m_csInfo) + GetSlaveTx(g_FT8006->m_csInfo)) * GetMasterRx(g_FT8006->m_csInfo) + 6,  bufferDown + downChannelNum, 6);
	}


	if (bufferLeft != NULL) {
		Ft8006m_fts_free(bufferLeft);
	}
	if (bufferRight != NULL) {
		Ft8006m_fts_free(bufferRight);
	}
	if (bufferUp != NULL) {
		Ft8006m_fts_free(bufferUp);
	}
	if (bufferDown != NULL) {
		Ft8006m_fts_free(bufferDown);
	}

	FTS_TEST_FUNC_EXIT();

}

/************************************************************************
* Name: FT8006_TestItem_EnterFactoryMode
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8006_TestItem_EnterFactoryMode(struct FT8006  *g_FT8006)
{

	unsigned char ReCode = ERROR_CODE_INVALID_PARAM;
	int iRedo = 5;
	int i ;
	Ft8006m_SysDelay(150);
	FTS_TEST_DBG("Enter factory mode...");
	for (i = 1; i <= iRedo; i++) {
		ReCode = FT8006_EnterFactory(g_FT8006);
		if (ERROR_CODE_OK != ReCode) {
			FTS_TEST_ERROR("Failed to Enter factory mode...");
			if (i < iRedo) {
				Ft8006m_SysDelay(50);
				continue;
			}
		} else {
			FTS_TEST_DBG(" success to Enter factory mode...");
			break;
		}

	}
	Ft8006m_SysDelay(300);

	if (ReCode == ERROR_CODE_OK) {
		ReCode = GetChannelNum();

	}
	return ReCode;
}

/************************************************************************
* Name: FT8006_TestItem_RawDataTest
* Brief:  TestItem: RawDataTest. Check if MCAP RawData is within the range.
* Input: bTestResult
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8006_TestItem_RawDataTest(struct FT8006  *g_FT8006, bool *bTestResult)
{
	unsigned char ReCode;
	bool btmpresult = true;
	int RawDataMin;
	int RawDataMax;
	int iValue = 0;
	int i = 0;
	int iRow, iCol;


	FTS_TEST_INFO("\n\n==============================Test Item: -------- Raw Data Test\n");



	for (i = 0 ; i < 3; i++) {
		ReCode = GetRawData(g_FT8006);
	}

	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERROR("Failed to get Raw Data!! Error Code: %d",  ReCode);
		return ReCode;
	}



	FTS_TEST_DBG("\nVA Channels: ");
	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum; iRow++) {
		FTS_TEST_DBG("\nCh_%02d:  ", iRow+1);
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; iCol++) {
			FTS_TEST_DBG("%5d, ", m_RawData[iRow][iCol]);
		}
	}

	FTS_TEST_DBG("\nKeys:  ");
	for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.KeyNumTotal; iCol++)
		FTS_TEST_DBG("%5d, ",  m_RawData[ft8006m_g_stSCapConfEx.ChannelXNum][iCol]);







	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum; iRow++) {

		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; iCol++) {
			if (ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0)
				continue;
			RawDataMin = ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min[iRow][iCol];
			RawDataMax = ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max[iRow][iCol];
			iValue = m_RawData[iRow][iCol];

			if (iValue < RawDataMin || iValue > RawDataMax) {
				btmpresult = false;
				FTS_TEST_ERROR("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
							 iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
			}
		}
	}



	iRow = ft8006m_g_stSCapConfEx.ChannelXNum;
	for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.KeyNumTotal; iCol++) {
		if (ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0)
			continue;
		RawDataMin = ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Min[iRow][iCol];
		RawDataMax = ft8006m_g_stCfg_Incell_DetailThreshold.RawDataTest_Max[iRow][iCol];
		iValue = m_RawData[iRow][iCol];

		if (iValue < RawDataMin || iValue > RawDataMax) {
			btmpresult = false;
			FTS_TEST_ERROR("rawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
						 iRow+1, iCol+1, iValue, RawDataMin, RawDataMax);
		}
	}




	Ft8006m_TestResultLen += sprintf(Ft8006m_TestResult+Ft8006m_TestResultLen, "RawData Test is %s. \n\n", (btmpresult ? "OK" : "NG"));


	Save_Test_Data(m_RawData, 0, ft8006m_g_stSCapConfEx.ChannelXNum+1, ft8006m_g_stSCapConfEx.ChannelYNum, 1);

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
* Name: FT8006_TestItem_CbTest
* Brief:  TestItem: Cb Test. Check if Cb is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8006_TestItem_CbTest(struct FT8006  *g_FT8006, bool *bTestResult)
{
	bool btmpresult = true;
	unsigned char ReCode = ERROR_CODE_OK;
	int iRow = 0;
	int iCol = 0;
	int i = 0;
	int iMaxValue = 0;
	int iMinValue = 0;
	int iValue = 0;
	bool bIncludeKey = false;
	unsigned char bClbResult = 0;


	bIncludeKey = g_stCfg_FT8006_BasicThreshold.bCBTest_VKey_Check;

	FTS_TEST_INFO("\n\n==============================Test Item: --------  CB Test\n");

	for (i = 0; i < 10; i++) {
		ReCode = FT8006_ChipClb(g_FT8006,  &bClbResult);
		Ft8006m_SysDelay(50);
		if (0 != bClbResult) {
			break;
		}
	}
	if (false == bClbResult) {
		FTS_TEST_ERROR("\r\nReCalib Failed\r\n");
		btmpresult = false;
	}

	ReCode = FT8006_GetTxRxCB(g_FT8006, 0, (short)(ft8006m_g_stSCapConfEx.ChannelXNum * ft8006m_g_stSCapConfEx.ChannelYNum  + ft8006m_g_stSCapConfEx.KeyNumTotal), m_ucTempData);
	if (ERROR_CODE_OK != ReCode) {
		btmpresult = false;
		FTS_TEST_ERROR("\nFailed to get CB value...\n");
		goto TEST_ERR;
	}

	memset(m_CBData, 0, sizeof(m_CBData));

	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum; ++iRow) {
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; ++iCol) {
			m_CBData[iRow][iCol] = m_ucTempData[iRow * ft8006m_g_stSCapConfEx.ChannelYNum + iCol];
		}
	}

	for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.KeyNumTotal; ++iCol) {
		m_CBData[ft8006m_g_stSCapConfEx.ChannelXNum][iCol] = m_ucTempData[ft8006m_g_stSCapConfEx.ChannelXNum*ft8006m_g_stSCapConfEx.ChannelYNum + iCol];

	}






	FTS_TEST_DBG("\nVA Channels: ");
	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum; iRow++) {
		FTS_TEST_DBG("\nCh_%02d:  ", iRow+1);
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; iCol++) {
			FTS_TEST_DBG("%3d, ", m_CBData[iRow][iCol]);
		}
	}

	FTS_TEST_DBG("\nKeys:  ");
	for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.KeyNumTotal; iCol++)
		FTS_TEST_DBG("%3d, ",  m_CBData[ft8006m_g_stSCapConfEx.ChannelXNum][iCol]);







	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum; iRow++) {
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; iCol++) {
			if ((0 == ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol])) {
				continue;
			}
			iMinValue = ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min[iRow][iCol];
			iMaxValue = ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max[iRow][iCol];
			iValue = ft8006m_focal_abs(m_CBData[iRow][iCol]);

			if (iValue < iMinValue || iValue > iMaxValue) {
				btmpresult = false;
				FTS_TEST_ERROR("CB test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
							 iRow+1, iCol+1, iValue, iMinValue, iMaxValue);
			}
		}
	}


	if (bIncludeKey) {

		iRow = ft8006m_g_stSCapConfEx.ChannelXNum;
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.KeyNumTotal; iCol++) {
			if (ft8006m_g_stCfg_Incell_DetailThreshold.InvalidNode[iRow][iCol] == 0) {
				continue;
			}
			iMinValue = ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Min[iRow][iCol];
			iMaxValue = ft8006m_g_stCfg_Incell_DetailThreshold.CBTest_Max[iRow][iCol];
			iValue = ft8006m_focal_abs(m_CBData[iRow][iCol]);
			if (iValue < iMinValue || iValue > iMaxValue) {
				btmpresult = false;
				FTS_TEST_ERROR("CB  test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
							 iRow+1, iCol+1, iValue, iMinValue, iMaxValue);
			}
		}


	}

	Ft8006m_TestResultLen += sprintf(Ft8006m_TestResult+Ft8006m_TestResultLen, "CB Test is %s. \n\n", (btmpresult ? "OK" : "NG"));


	Save_Test_Data(m_CBData, 0, ft8006m_g_stSCapConfEx.ChannelXNum+1, ft8006m_g_stSCapConfEx.ChannelYNum, 1);

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
	Ft8006m_TestResultLen += sprintf(Ft8006m_TestResult+Ft8006m_TestResultLen, "CB Test is NG. \n\n");
	return ReCode;
}


/************************************************************************
* Name: FT8006_TestItem_ChannelsTest
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8006_TestItem_ChannelsTest(struct FT8006 *g_FT8006, bool *bTestResult)
{
	unsigned char ReCode;

	FTS_TEST_DBG("\n\n==============================Test Item: -------- Channel Test ");

	ReCode = GetChannelNum();
	if (ReCode == ERROR_CODE_OK) {
		if ((g_stCfg_FT8006_BasicThreshold.ChannelNumTest_ChannelXNum == ft8006m_g_stSCapConfEx.ChannelXNum)
			&& (g_stCfg_FT8006_BasicThreshold.ChannelNumTest_ChannelYNum == ft8006m_g_stSCapConfEx.ChannelYNum)
			&& (g_stCfg_FT8006_BasicThreshold.ChannelNumTest_KeyNum == ft8006m_g_stSCapConfEx.KeyNum)) {
			*bTestResult = true;
			FTS_TEST_DBG("\n\nGet channels: (CHx: %d, CHy: %d, Key: %d), Set channels: (CHx: %d, CHy: %d, Key: %d)",
						ft8006m_g_stSCapConfEx.ChannelXNum, ft8006m_g_stSCapConfEx.ChannelYNum, ft8006m_g_stSCapConfEx.KeyNum,
						g_stCfg_FT8006_BasicThreshold.ChannelNumTest_ChannelXNum, g_stCfg_FT8006_BasicThreshold.ChannelNumTest_ChannelYNum, g_stCfg_FT8006_BasicThreshold.ChannelNumTest_KeyNum);

			FTS_TEST_INFO("\n//Channel Test is OK!");
		} else {
			*bTestResult = false;
			FTS_TEST_ERROR("\n\nGet channels: (CHx: %d, CHy: %d, Key: %d), Set channels: (CHx: %d, CHy: %d, Key: %d)",
						ft8006m_g_stSCapConfEx.ChannelXNum, ft8006m_g_stSCapConfEx.ChannelYNum, ft8006m_g_stSCapConfEx.KeyNum,
						g_stCfg_FT8006_BasicThreshold.ChannelNumTest_ChannelXNum, g_stCfg_FT8006_BasicThreshold.ChannelNumTest_ChannelYNum, g_stCfg_FT8006_BasicThreshold.ChannelNumTest_KeyNum);

			FTS_TEST_INFO("\n//Channel Test is NG!");
		}
	}


	Ft8006m_TestResultLen += sprintf(Ft8006m_TestResult+Ft8006m_TestResultLen, "Channel Test is %s. \n\n", (*bTestResult ? "OK" : "NG"));

	return ReCode;
}

/************************************************************************
* Name: FT8006_CheckItem_FactoryIdTest
* Brief:   Factory Id Testt.
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT8006_CheckItem_FactoryIdTest(struct FT8006  *g_FT8006, bool *bTestResult)
{
	unsigned char ReCode = ERROR_CODE_OK;
	bool btmpresult = true;
	unsigned char regData = 0;
	unsigned char FactoryId = 0;

	FTS_TEST_INFO("\n\n==============================Test Item: -------- Factory Id Test\n");

	ReCode = Ft8006m_EnterWork();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERROR(" EnterWork failed.. Error Code: %d \n", ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}


	Ft8006m_SysDelay(60);
	ReCode = Ft8006m_ReadReg(REG_FACTORYID, &regData);

	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERROR("  Failed to Read Register: %x. Error Code: %d \n", REG_FACTORYID, ReCode);
		goto TEST_ERR;
	}

	FactoryId = g_stCfg_FT8006_BasicThreshold.Factory_ID_Number;

	FTS_TEST_DBG("\r\nGet FactoryId:0x%02x, Set FactoryId: 0x%02x  \n", regData, FactoryId);

	if (FactoryId == regData) {
		btmpresult = true;

	} else {
		btmpresult = false;
	}

	Ft8006m_TestResultLen += sprintf(Ft8006m_TestResult+Ft8006m_TestResultLen, " FactoryId Test is %s. \n\n", (btmpresult  ? "OK" : "NG"));

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

	Ft8006m_TestResultLen += sprintf(Ft8006m_TestResult+Ft8006m_TestResultLen, " FactoryId Test is NG. \n\n");

	return ReCode;
}


unsigned char FT8006_TestItem_ShortCircuitTest(struct FT8006  *g_FT8006, bool *bTestResult)
{
	unsigned char ReCode = ERROR_CODE_OK;
	bool bTempResult = true;
	int ResMin = 0;
	int iAllAdcDataNum = 0;
	unsigned char iTxNum = 0, iRxNum = 0, iChannelNum = 0;
	int iRow = 0, iCol = 0;
	int tmpAdc = 0;
	int iValueMin = 0;
	int iValueMax = 0;
	int iValue = 0;
	int i = 0;

	FTS_TEST_INFO("\r\n\r\n==============================Test Item: -------- Short Circuit Test \r\n");

	ReCode = FT8006_EnterFactory(g_FT8006);
	if (ERROR_CODE_OK != ReCode) {
		bTempResult = false;
		FTS_TEST_ERROR("\r\n\r\n// Failed to Enter factory mode. Error Code: %d", ReCode);
		goto TEST_END;
	}

	ResMin = g_stCfg_FT8006_BasicThreshold.ShortCircuit_ResMin;
	ReCode = Ft8006m_ReadReg(0x02, &iTxNum);
	ReCode = Ft8006m_ReadReg(0x03, &iRxNum);
	if (ERROR_CODE_OK != ReCode) {
		bTempResult = false;
		FTS_TEST_ERROR("\r\n\r\n// Failed to read reg. Error Code: %d", ReCode);
		goto TEST_END;
	}

	iChannelNum = iTxNum + iRxNum;
	iAllAdcDataNum = iTxNum * iRxNum + ft8006m_g_stSCapConfEx.KeyNumTotal;
	memset(iAdcData, 0, sizeof(iAdcData));

	for (i = 0; i < 1; i++) {
		ReCode =  FT8006_WeakShort_GetAdcData(g_FT8006, iAllAdcDataNum*2, iAdcData);
		Ft8006m_SysDelay(50);
		if (ERROR_CODE_OK != ReCode) {
			bTempResult = false;
			FTS_TEST_ERROR("\r\n\r\n// Failed to get AdcData. Error Code: %d", ReCode);
			goto TEST_END;
		}
	}


	FTS_TEST_DBG("shortRes data:\n");
	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum + 1; ++iRow) {
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; ++iCol) {
			tmpAdc = iAdcData[iRow * iRxNum + iCol];
			if (tmpAdc > 4050)
				tmpAdc = 4050;
			shortRes[iRow][iCol] = (tmpAdc * 100) / (4095 - tmpAdc);

			FTS_TEST_DBG("%-4d  ", shortRes[iRow][iCol]);
		}
		FTS_TEST_DBG(" \n");
	}
	FTS_TEST_DBG(" \n");




	iValueMin = ResMin;
	iValueMax = 100000000;
	FTS_TEST_DBG(" Short Circuit test , Set_Range=(%d, %d). \n", \
				iValueMin, iValueMax);

	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum; iRow++) {
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; iCol++) {
			if (ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol] == 0)
				continue;

			iValue = shortRes[iRow][iCol];
			if (iValue < iValueMin || iValue > iValueMax) {
				bTempResult = false;
				FTS_TEST_ERROR(" Short Circuit test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d). \n", \
							 iRow+1, iCol+1, iValue, iValueMin, iValueMax);
			}
		}
	}

TEST_END:

	if (bTempResult) {
		FTS_TEST_INFO("\r\n\r\n//Short Circuit Test is OK!");
		*bTestResult = true;
	} else {
		FTS_TEST_INFO("\r\n\r\n//Short Circuit Test is NG!");
		*bTestResult = false;
	}

	Ft8006m_TestResultLen += sprintf(Ft8006m_TestResult+Ft8006m_TestResultLen, "Short Circuit Test is %s. \n\n", (bTempResult ? "OK" : "NG"));

	return ReCode;
}
unsigned char FT8006_TestItem_LCDNoiseTest(struct FT8006  *g_FT8006, bool *bTestResult)
{
	unsigned char ReCode = ERROR_CODE_OK;
	bool bResultFlag = true;
	unsigned char oldMode = 0;
	int iRetry = 0;
	unsigned char status = 0xff;
	int iRow = 0, iCol = 0;
	int index = 0;
	unsigned char chNoiseValue = 0xff;
	int iValueMin = 0;
	int iValueMax = 0;
	int iValue = 0;

	FTS_TEST_INFO("\r\n\r\n==============================Test Item: -------- LCD Noise Test \r\n");


	Ft8006m_ReadReg(0x06, &oldMode);
	FT8006_WriteReg(g_FT8006,  0x06, 1);


	ReCode = FT8006_WriteReg(g_FT8006, REG_8006_LCD_NOISE_FRAME, g_stCfg_FT8006_BasicThreshold.iLCDNoiseTestFrame & 0xff);
	ReCode = FT8006_WriteReg(g_FT8006,  REG_8006_LCD_NOISE_FRAME+1, (g_stCfg_FT8006_BasicThreshold.iLCDNoiseTestFrame >> 8) & 0xff);


	ReCode = FT8006_WriteReg(g_FT8006,  0x01, 0xAD);


	ReCode = FT8006_WriteReg(g_FT8006, REG_8006_LCD_NOISE_START, 0x01);


	for (iRetry = 0; iRetry < 50; ++iRetry) {

		ReCode = Ft8006m_ReadReg(REG_8006_LCD_NOISE_START, &status);
		if (status == 0x00)
			break;
		Ft8006m_SysDelay(500);
	}

	if (iRetry == 50) {
		bResultFlag = false;
		FTS_TEST_ERROR("\r\nScan Noise Time Out!");
		goto TEST_END;
	}


	memset(m_RawData, 0, sizeof(m_RawData));
	memset(ScreenNoiseData, 0, sizeof(ScreenNoiseData));

	ReadBytesByKey(0x6A, ft8006m_g_stSCapConfEx.ChannelXNum * ft8006m_g_stSCapConfEx.ChannelYNum * 2 + ft8006m_g_stSCapConfEx.KeyNumTotal * 2, ScreenNoiseData);
	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum; ++iRow) {
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; ++iCol) {
			index = iRow * ft8006m_g_stSCapConfEx.ChannelYNum + iCol;
			m_RawData[iRow][iCol] = (int)(short)((ScreenNoiseData[index * 2] << 8) + ScreenNoiseData[index * 2 + 1]);
		}
	}

	for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.KeyNumTotal; ++iCol) {
		index = ft8006m_g_stSCapConfEx.ChannelXNum * ft8006m_g_stSCapConfEx.ChannelYNum + iCol;
		m_RawData[ft8006m_g_stSCapConfEx.ChannelXNum][iCol] = (int)(short)((ScreenNoiseData[index * 2] << 8) + ScreenNoiseData[index * 2 + 1]);
	}

	ReCode = FT8006_EnterWork(g_FT8006);
	Ft8006m_SysDelay(100);
	ReCode = Ft8006m_ReadReg(0x80, &chNoiseValue);
	ReCode = FT8006_EnterFactory(g_FT8006);
	Ft8006m_SysDelay(100);

	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum + 1; ++iRow) {
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; ++iCol) {
			LCD_Noise[iRow][iCol] = ft8006m_focal_abs(m_RawData[iRow][iCol]);
		}
	}

	FTS_TEST_DBG("\nVA Channels: ");
	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum+1; ++iRow) {
		FTS_TEST_DBG("\nCh_%02d:  ", iRow+1);
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; ++iCol) {

			FTS_TEST_DBG("%4d, ", LCD_Noise[iRow][iCol]);
		}
	}
	FTS_TEST_DBG("\n");

	iValueMin = 0;
	iValueMax = g_stCfg_FT8006_BasicThreshold.iLCDNoiseCoefficient * chNoiseValue * 32 / 100;

	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum+1; iRow++) {
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; iCol++) {
			if (ft8006m_g_stCfg_MCap_DetailThreshold.InvalidNode[iRow][iCol] == 0)
				continue;

			iValue = LCD_Noise[iRow][iCol];
			if (iValue < iValueMin || iValue > iValueMax) {
				bResultFlag = false;
				FTS_TEST_ERROR(" LCD Noise test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d). \n",
							 iRow+1, iCol+1, iValue, iValueMin, iValueMax);
			}
		}
	}

	Save_Test_Data(LCD_Noise, 0, ft8006m_g_stSCapConfEx.ChannelXNum, ft8006m_g_stSCapConfEx.ChannelYNum, 1);


TEST_END:

	FT8006_WriteReg(g_FT8006, 0x06, oldMode);
	Ft8006m_SysDelay(20);

	if (bResultFlag) {
		FTS_TEST_INFO("\r\n\r\n//LCD Noise Test is OK!");
		*bTestResult = true;
	} else {
		FTS_TEST_INFO("\r\n\r\n//LCD Noise Test is NG!");
		*bTestResult = false;
	}

	Ft8006m_TestResultLen += sprintf(Ft8006m_TestResult+Ft8006m_TestResultLen, "LCD Noise  Test is %s. \n\n", (bResultFlag ? "OK" : "NG"));

	return ReCode;
}


static unsigned char ChipClb(unsigned char *pClbResult)
{
	unsigned char RegData = 0;
	unsigned char TimeOutTimes = 50;
	unsigned char ReCode = ERROR_CODE_OK;

	ReCode = Ft8006m_WriteReg(REG_CLB, 4);

	if (ReCode == ERROR_CODE_OK) {
		while (TimeOutTimes--) {
			Ft8006m_SysDelay(100);
			ReCode = Ft8006m_WriteReg(DEVIDE_MODE_ADDR, 0x04<<4);
			ReCode = Ft8006m_ReadReg(0x04, &RegData);
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
* Name: FT8006M_get_test_data
* Brief:  get data of test result
* Input: none
* Output: pTestData, the returned buff
* Return: the length of test data. if length > 0, got data;else ERR.
***********************************************************************/
int FT8006M_get_test_data(char *pTestData)
{
	if (NULL == pTestData) {
		FTS_TEST_DBG(" pTestData == NULL ");
		return -EPERM;
	}
	memcpy(pTestData, ft8006m_g_pStoreAllData, (ft8006m_g_lenStoreMsgArea+ft8006m_g_lenStoreDataArea));
	return (ft8006m_g_lenStoreMsgArea+ft8006m_g_lenStoreDataArea);
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


	iLen = sprintf(ft8006m_g_pTmpBuff, "NA, %d, %d, %d, %d, %d, ", \
				ft8006m_m_ucTestItemCode, Row, Col, ft8006m_m_iStartLine, ItemCount);
	memcpy(ft8006m_g_pMsgAreaLine2+ft8006m_g_lenMsgAreaLine2, ft8006m_g_pTmpBuff, iLen);
	ft8006m_g_lenMsgAreaLine2 += iLen;

	ft8006m_m_iStartLine += Row;
	ft8006m_m_iTestDataCount++;


	for (i = 0+iArrayIndex; (i < Row+iArrayIndex) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < Col) && (j < RX_NUM_MAX); j++) {
			if (j == (Col - 1))
				iLen = sprintf(ft8006m_g_pTmpBuff, "%d, \n",  iData[i][j]);
			else
				iLen = sprintf(ft8006m_g_pTmpBuff, "%d, ", iData[i][j]);

			memcpy(ft8006m_g_pStoreDataArea+ft8006m_g_lenStoreDataArea, ft8006m_g_pTmpBuff, iLen);
			ft8006m_g_lenStoreDataArea += iLen;
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
static int StartScan()
{
	unsigned char RegVal = 0x00;
	unsigned char times = 0;
	const unsigned char MaxTimes = 250;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;



	ReCode = Ft8006m_ReadReg(DEVIDE_MODE_ADDR, &RegVal);
	printk("hb1 :RegVal:0x%x, ReCode:0x%x\n", RegVal, ReCode);
	if (ReCode == ERROR_CODE_OK) {
		RegVal |= 0x80;
		ReCode = Ft8006m_WriteReg(DEVIDE_MODE_ADDR, RegVal);
		printk("hb2 :RegVal:0x%x, ReCode:0x%x\n", RegVal, ReCode);
		if (ReCode == ERROR_CODE_OK) {
			while (times++ < MaxTimes) {
				Ft8006m_SysDelay(16);
				ReCode = Ft8006m_ReadReg(DEVIDE_MODE_ADDR, &RegVal);
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
			} else {
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

	ReCode = Ft8006m_WriteReg(REG_LINE_NUM, LineNum);



	I2C_wBuffer[0] = REG_RawBuf0;
	if (ReCode == ERROR_CODE_OK) {
		ft8006m_focal_msleep(10);
		ReCode = Ft8006m_Comm_Base_IIC_IO(I2C_wBuffer, 1, pReadData, BytesNumInTestMode1);
	}

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK)
			break;

		if (i == iReadNum-1) {
			ft8006m_focal_msleep(10);
			ReCode = Ft8006m_Comm_Base_IIC_IO(NULL, 0, pReadData+BYTES_PER_TIME*i, ByteNum-BYTES_PER_TIME*i);
		} else {
			ft8006m_focal_msleep(10);
			ReCode = Ft8006m_Comm_Base_IIC_IO(NULL, 0, pReadData+BYTES_PER_TIME*i, BYTES_PER_TIME);
		}

	}

	if (ReCode == ERROR_CODE_OK) {
		for (i = 0; i < (ByteNum>>1); i++) {
			pRevBuffer[i] = (pReadData[i<<1]<<8)+pReadData[(i<<1)+1];
		}
	}


	return ReCode;

}
static unsigned char FT8006_GetTxRxCB(struct FT8006 *g_FT8006, unsigned short StartNodeNo, unsigned short ReadNum, unsigned char *pReadBuffer)
{
	unsigned char ReCode = ERROR_CODE_OK;
	struct CSChipAddrMgr mgr;

	FTS_TEST_FUNC_ENTER();

	if (ChipHasTwoHeart(g_FT8006)) {
		memset(bufferMaster, 0, sizeof(bufferMaster));
		memset(bufferSlave, 0, sizeof(bufferSlave));
		memset(bufferCated, 0, sizeof(bufferCated));

		CSChipAddrMgr_Init(&mgr, g_FT8006);

		WorkAsMaster(&mgr);
		ReCode = GetTxRxCB(0, GetMasterTx(g_FT8006->m_csInfo) * GetMasterRx(g_FT8006->m_csInfo)  + 6, (unsigned char *)bufferMaster);


		WorkAsSlave(&mgr);
		ReCode = GetTxRxCB(0, GetSlaveTx(g_FT8006->m_csInfo) * GetSlaveRx(g_FT8006->m_csInfo) + 6, (unsigned char *)bufferSlave);

		CatSingleToOneScreen(g_FT8006);

		memcpy(pReadBuffer, (unsigned char *)bufferCated + StartNodeNo, ReadNum);

		CSChipAddrMgr_Exit(&mgr);
	} else {
		ReCode = GetTxRxCB(StartNodeNo, ReadNum, pReadBuffer);
	}

	FTS_TEST_FUNC_EXIT();

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
	unsigned char wBuffer[4] = {0};
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

		ReCode = Ft8006m_WriteReg(REG_CbAddrH, wBuffer[1]);
		ReCode = Ft8006m_WriteReg(REG_CbAddrL, wBuffer[2]);

		ReCode = Ft8006m_Comm_Base_IIC_IO(wBuffer, 1, pReadBuffer+usTotalReturnNum, usReturnNum);

		usTotalReturnNum += usReturnNum;

		if (ReCode != ERROR_CODE_OK)
			return ReCode;


	}

	return ReCode;
}




static unsigned char GetPanelRows(unsigned char *pPanelRows)
{
	return Ft8006m_ReadReg(REG_TX_NUM, pPanelRows);
}




static unsigned char GetPanelCols(unsigned char *pPanelCols)
{
	return Ft8006m_ReadReg(REG_RX_NUM, pPanelCols);
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
				ft8006m_g_stSCapConfEx.ChannelXNum = rBuffer[0];
				if (ft8006m_g_stSCapConfEx.ChannelXNum > ft8006m_g_ScreenSetParam.iUsedMaxTxNum) {
					FTS_TEST_ERROR("Failed to get Channel X number, Get num = %d, UsedMaxNum = %d",
								 ft8006m_g_stSCapConfEx.ChannelXNum, ft8006m_g_ScreenSetParam.iUsedMaxTxNum);
					ft8006m_g_stSCapConfEx.ChannelXNum = 0;
					return ERROR_CODE_INVALID_PARAM;
				}

				break;
			} else {
				Ft8006m_SysDelay(150);
				continue;
			}
		} else {
			FTS_TEST_ERROR("Failed to get Channel X number");
			Ft8006m_SysDelay(150);
		}
	}


	for (i = 0; i < 3; i++) {
		ReCode = GetPanelCols(rBuffer);
		if (ReCode == ERROR_CODE_OK) {
			if (0 < rBuffer[0] && rBuffer[0] < 80) {
				ft8006m_g_stSCapConfEx.ChannelYNum = rBuffer[0];
				if (ft8006m_g_stSCapConfEx.ChannelYNum > ft8006m_g_ScreenSetParam.iUsedMaxRxNum) {

					FTS_TEST_ERROR("Failed to get Channel Y number, Get num = %d, UsedMaxNum = %d",
								 ft8006m_g_stSCapConfEx.ChannelYNum, ft8006m_g_ScreenSetParam.iUsedMaxRxNum);
					ft8006m_g_stSCapConfEx.ChannelYNum = 0;
					return ERROR_CODE_INVALID_PARAM;
				}
				break;
			} else {
				Ft8006m_SysDelay(150);
				continue;
			}
		} else {
			FTS_TEST_ERROR("Failed to get Channel Y number");
			Ft8006m_SysDelay(150);
		}
	}


	for (i = 0; i < 3; i++) {
		unsigned char regData = 0;
		ft8006m_g_stSCapConfEx.KeyNum = 0;
		ReCode = Ft8006m_ReadReg(FT8006_LEFT_KEY_REG, &regData);
		if (ReCode == ERROR_CODE_OK) {
			if (((regData >> 0) & 0x01)) {
				ft8006m_g_stSCapConfEx.bLeftKey1 = true;
				++ft8006m_g_stSCapConfEx.KeyNum;
			}
			if (((regData >> 1) & 0x01)) {
				ft8006m_g_stSCapConfEx.bLeftKey2 = true;
				++ft8006m_g_stSCapConfEx.KeyNum;
			}
			if (((regData >> 2) & 0x01)) {
				ft8006m_g_stSCapConfEx.bLeftKey3 = true;
				++ft8006m_g_stSCapConfEx.KeyNum;
			}

		} else {
			FTS_TEST_ERROR("Failed to get Key number");
			Ft8006m_SysDelay(150);
			continue;
		}
		ReCode = Ft8006m_ReadReg(FT8006_RIGHT_KEY_REG, &regData);
		if (ReCode == ERROR_CODE_OK) {
			if (((regData >> 0) & 0x01)) {
				ft8006m_g_stSCapConfEx.bRightKey1 = true;
				++ft8006m_g_stSCapConfEx.KeyNum;
			}
			if (((regData >> 1) & 0x01)) {
				ft8006m_g_stSCapConfEx.bRightKey2 = true;
				++ft8006m_g_stSCapConfEx.KeyNum;
			}
			if (((regData >> 2) & 0x01)) {
				ft8006m_g_stSCapConfEx.bRightKey3 = true;
				++ft8006m_g_stSCapConfEx.KeyNum;
			}


			break;
		} else {
			FTS_TEST_ERROR("Failed to get Key number");
			Ft8006m_SysDelay(150);
			continue;
		}
	}



	FTS_TEST_INFO("CH_X = %d, CH_Y = %d, Key = %d",  ft8006m_g_stSCapConfEx.ChannelXNum , ft8006m_g_stSCapConfEx.ChannelYNum, ft8006m_g_stSCapConfEx.KeyNum);
	return ReCode;
}

/************************************************************************
* Name: GetRawData
* Brief:  Get Raw Data of VA area and Key area
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetRawData(struct FT8006  *g_FT8006)
{
	int ReCode = ERROR_CODE_OK;
	int iRow, iCol;

	FTS_TEST_FUNC_ENTER();



	ReCode = FT8006_StartScan(g_FT8006);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERROR("Failed to Scan ...");
		return ReCode;
	}




	memset(m_RawData, 0, sizeof(m_RawData));
	memset(m_iTempRawData, 0, sizeof(m_iTempRawData));
	ReCode = FT8006_ReadRawData(g_FT8006, 0, 0xAD, ft8006m_g_stSCapConfEx.ChannelXNum * ft8006m_g_stSCapConfEx.ChannelYNum * 2, m_iTempRawData);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERROR("Failed to Get RawData");
		return ReCode;
	}

	for (iRow = 0; iRow < ft8006m_g_stSCapConfEx.ChannelXNum; ++iRow) {
		for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.ChannelYNum; ++iCol) {
			m_RawData[iRow][iCol] = m_iTempRawData[iRow * ft8006m_g_stSCapConfEx.ChannelYNum + iCol];
		}
	}


	memset(m_iTempRawData, 0, sizeof(m_iTempRawData));
	ReCode = FT8006_ReadRawData(g_FT8006,  0, 0xAE, ft8006m_g_stSCapConfEx.KeyNumTotal * 2, m_iTempRawData);
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_ERROR("Failed to Get RawData");
		return ReCode;
	}

	for (iCol = 0; iCol < ft8006m_g_stSCapConfEx.KeyNumTotal; ++iCol) {

		m_RawData[ft8006m_g_stSCapConfEx.ChannelXNum][iCol] = m_iTempRawData[iCol];
	}

	FTS_TEST_FUNC_EXIT();

	return ReCode;

}
unsigned char FT8006_ReadRawData(struct FT8006 *g_FT8006, unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_OK;

	FTS_TEST_FUNC_ENTER();

	if (ChipHasTwoHeart(g_FT8006)) {

		struct CSChipAddrMgr mgr;
		CSChipAddrMgr_Init(&mgr, g_FT8006);

		WorkAsMaster(&mgr);
		ReCode = ReadRawData(Freq, LineNum, ByteNum, pRevBuffer);

		CSChipAddrMgr_Exit(&mgr);
	} else {

		ReCode = ReadRawData(Freq, LineNum, ByteNum, pRevBuffer);
	}

	FTS_TEST_FUNC_EXIT();

	return ReCode;
}

unsigned char FT8006_EnterFactory(struct FT8006  *g_FT8006)
{
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;


	ReleaseCSInfo(g_FT8006);

	ReCode = Ft8006m_EnterFactory();


	UpDateCSInfo(g_FT8006);


	return ReCode;
}
unsigned char FT8006_EnterWork(struct FT8006 *g_FT8006)
{
	unsigned char ReCode = ERROR_CODE_OK;
	if (ChipHasTwoHeart(g_FT8006)) {

		struct CSChipAddrMgr mgr;

		CSChipAddrMgr_Init(&mgr, g_FT8006);

		WorkAsMaster(&mgr);
		ReCode = Ft8006m_EnterWork();
		CSChipAddrMgr_Exit(&mgr);

	} else {

		ReCode = Ft8006m_EnterWork();
	}

	return ReCode;
}

unsigned char FT8006_ChipClb(struct FT8006 *g_FT8006, unsigned char *pClbResult)
{
	unsigned char ReCode = ERROR_CODE_OK;

	FTS_TEST_FUNC_ENTER();

	if (ChipHasTwoHeart(g_FT8006)) {

		struct CSChipAddrMgr mgr;
		CSChipAddrMgr_Init(&mgr, g_FT8006);

		WorkAsMaster(&mgr);
		ReCode = (ReCode == ERROR_CODE_OK) ?  ChipClb(pClbResult) : ReCode;

		WorkAsSlave(&mgr);
		ReCode = (ReCode == ERROR_CODE_OK) ?  ChipClb(pClbResult) : ReCode;

		CSChipAddrMgr_Exit(&mgr);
	} else {

		ReCode =  ChipClb(pClbResult);
	}

	FTS_TEST_FUNC_EXIT();

	return ReCode;
}
unsigned char FT8006_WeakShort_GetAdcData(struct FT8006 *g_FT8006, int AllAdcDataLen, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_OK;
	struct CSChipAddrMgr mgr;

	FTS_TEST_FUNC_ENTER();

	if (ChipHasTwoHeart(g_FT8006)) {
		memset(bufferMaster, 0, sizeof(bufferMaster));
		memset(bufferSlave, 0, sizeof(bufferSlave));
		memset(bufferCated, 0, sizeof(bufferCated));

		CSChipAddrMgr_Init(&mgr, g_FT8006);

		WorkAsMaster(&mgr);
		ReCode = WeakShort_StartAdcScan();

		WorkAsMaster(&mgr);
		ReCode =  (ReCode == ERROR_CODE_OK) ? WeakShort_GetScanResult() : ReCode;
		ReCode =  (ReCode == ERROR_CODE_OK) ? WeakShort_GetAdcResult(AllAdcDataLen, bufferMaster) : ReCode;

		WorkAsSlave(&mgr);
		ReCode = (ReCode == ERROR_CODE_OK) ?  WeakShort_GetScanResult() : ReCode;
		ReCode = (ReCode == ERROR_CODE_OK) ?  WeakShort_GetAdcResult(AllAdcDataLen, bufferSlave) : ReCode;


		CatSingleToOneScreen(g_FT8006);

		memcpy(pRevBuffer,  bufferCated, AllAdcDataLen * sizeof(unsigned int));

		CSChipAddrMgr_Exit(&mgr);

	} else {
		ReCode = WeakShort_GetAdcData(AllAdcDataLen, pRevBuffer);
	}

	FTS_TEST_FUNC_EXIT();

	return ReCode;

}
unsigned char WeakShort_StartAdcScan()
{
	return Ft8006m_WriteReg(0x0F, 1);
}

unsigned char FT8006_WriteReg(struct FT8006 *g_FT8006, unsigned char RegAddr, unsigned char RegData)
{
	unsigned char ReCode = ERROR_CODE_OK;
	if (ChipHasTwoHeart(g_FT8006)) {

		struct CSChipAddrMgr mgr;

		CSChipAddrMgr_Init(&mgr, g_FT8006);

		WorkAsMaster(&mgr);
		ReCode = (ReCode == ERROR_CODE_OK) ? Ft8006m_WriteReg(RegAddr, RegData) : ReCode;

		WorkAsSlave(&mgr);
		ReCode = (ReCode == ERROR_CODE_OK) ? Ft8006m_WriteReg(RegAddr, RegData) : ReCode;

		CSChipAddrMgr_Exit(&mgr);
	} else {

		ReCode = Ft8006m_WriteReg(RegAddr, RegData);
	}

	return ReCode;
}

unsigned char WeakShort_GetScanResult()
{
	unsigned char RegMark = 0;
	unsigned char ReCode = ERROR_CODE_OK;
	int index = 0;

	for (index = 0; index < 50; ++index) {
		Ft8006m_SysDelay(50);
		ReCode = Ft8006m_ReadReg(0x10, &RegMark);
		if (ERROR_CODE_OK == ReCode && 0 == RegMark)
			break;
	}

	return ReCode;
}

unsigned char WeakShort_GetAdcResult(int AllAdcDataLen, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_OK;
	int index = 0;
	int i = 0;
	int usReturnNum = 0;
	unsigned char wBuffer[2] = {0};
	int iReadNum = 0;

	FTS_TEST_FUNC_ENTER();

	iReadNum =  AllAdcDataLen / BYTES_PER_TIME;
	if ((AllAdcDataLen % BYTES_PER_TIME) > 0)
		++iReadNum;

	memset(wBuffer, 0, sizeof(wBuffer));
	wBuffer[0] = 0x89;
	memset(pReadBuffer, 0, sizeof(pReadBuffer));

	usReturnNum = BYTES_PER_TIME;
	if (ReCode == ERROR_CODE_OK) {
		ReCode = Ft8006m_Comm_Base_IIC_IO(wBuffer, 1, pReadBuffer, usReturnNum);
	}

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERROR("Comm_Base_IIC_IO  error.   !!!");
			break;
		}

		if (i == iReadNum-1) {
			usReturnNum = AllAdcDataLen-BYTES_PER_TIME*i;
			ReCode = Ft8006m_Comm_Base_IIC_IO(NULL, 0, pReadBuffer+BYTES_PER_TIME*i, usReturnNum);
		} else {
			usReturnNum = BYTES_PER_TIME;
			ReCode = Ft8006m_Comm_Base_IIC_IO(NULL, 0, pReadBuffer+BYTES_PER_TIME*i, usReturnNum);
		}
	}

	for (index = 0; index < AllAdcDataLen/2; ++index) {
		pRevBuffer[index] = (pReadBuffer[index * 2] << 8) + pReadBuffer[index * 2 + 1];
	}

	FTS_TEST_FUNC_EXIT();

	return ReCode;
}
/************************************************************************
* Name: WeakShort_GetAdcData
* Brief:  Get Adc Data
* Input: AllAdcDataLen
* Output: pRevBuffer
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char WeakShort_GetAdcData(int AllAdcDataLen, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_OK;
	unsigned char RegMark = 0;
	int index = 0;
	int i = 0;
	int usReturnNum = 0;
	unsigned char wBuffer[2] = {0};

	int iReadNum = 0;

	FTS_TEST_FUNC_ENTER();

	memset(wBuffer, 0, sizeof(wBuffer));
	wBuffer[0] = 0x89;

	iReadNum = AllAdcDataLen / BYTES_PER_TIME;
	if ((AllAdcDataLen % BYTES_PER_TIME) > 0)
		++iReadNum;

	ReCode = Ft8006m_WriteReg(0x0F, 1);

	for (index = 0; index < 50; ++index) {
		Ft8006m_SysDelay(50);
		ReCode = Ft8006m_ReadReg(0x10, &RegMark);
		if (ERROR_CODE_OK == ReCode && 0 == RegMark)
			break;
	}
	if (index >= 50) {
		FTS_TEST_ERROR("Ft8006m_ReadReg failed, ADC data not OK.");
		return 6;
	}

	{
		usReturnNum = BYTES_PER_TIME;
		if (ReCode == ERROR_CODE_OK) {
			ReCode = Ft8006m_Comm_Base_IIC_IO(wBuffer, 1, pReadBuffer, usReturnNum);
		}

		for (i = 1; i < iReadNum; i++) {
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_ERROR("Comm_Base_IIC_IO  error.   !!!");
				break;
			}

			if (i == iReadNum-1) {
				usReturnNum = AllAdcDataLen-BYTES_PER_TIME*i;
				ReCode = Ft8006m_Comm_Base_IIC_IO(NULL, 0, pReadBuffer+BYTES_PER_TIME*i, usReturnNum);
			} else {
				usReturnNum = BYTES_PER_TIME;
				ReCode = Ft8006m_Comm_Base_IIC_IO(NULL, 0, pReadBuffer+BYTES_PER_TIME*i, usReturnNum);
			}
		}
	}

	for (index = 0; index < AllAdcDataLen/2; ++index) {
		pRevBuffer[index] = (pReadBuffer[index * 2] << 8) + pReadBuffer[index * 2 + 1];
	}

	FTS_TEST_FUNC_EXIT();

	return ReCode;
}

unsigned char ReadBytesByKey(BYTE key, int ByteNum, unsigned char *byteData)
{

	unsigned char ReCode = ERROR_CODE_OK;
	unsigned char I2C_wBuffer[3] = {0};
	unsigned short BytesNumInTestMode1 = 0;
	int i = 0;
	int iReadNum = 0;

	FTS_TEST_FUNC_ENTER();

	iReadNum =  ByteNum/BYTES_PER_TIME;

	if (0 != (ByteNum%BYTES_PER_TIME))
		iReadNum++;

	if (ByteNum <= BYTES_PER_TIME) {
		BytesNumInTestMode1 = ByteNum;
	} else {
		BytesNumInTestMode1 = BYTES_PER_TIME;
	}


	I2C_wBuffer[0] = key;
	if (ReCode == ERROR_CODE_OK) {
		ReCode = Ft8006m_Comm_Base_IIC_IO(I2C_wBuffer, 1, byteData, BytesNumInTestMode1);
	}

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK)
			break;

		if (i == iReadNum-1) {
			ReCode = Ft8006m_Comm_Base_IIC_IO(NULL, 0, byteData + BYTES_PER_TIME*i, ByteNum-BYTES_PER_TIME*i);
		} else
			ReCode = Ft8006m_Comm_Base_IIC_IO(NULL, 0, byteData + BYTES_PER_TIME*i, BYTES_PER_TIME);

	}

	FTS_TEST_FUNC_EXIT();

	return ReCode;

}

unsigned char FT8006_StartScan(struct FT8006  *g_FT8006)
{
	unsigned char ReCode = ERROR_CODE_OK;

	FTS_TEST_FUNC_ENTER();

	if (ChipHasTwoHeart(g_FT8006)) {

		struct CSChipAddrMgr mgr;

		CSChipAddrMgr_Init(&mgr, g_FT8006);

		WorkAsMaster(&mgr);
		ReCode = StartScan();
		CSChipAddrMgr_Exit(&mgr);
	} else {

		ReCode = StartScan();
	}


	FTS_TEST_FUNC_EXIT();

	return ReCode;
}



#endif
