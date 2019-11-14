/************************************************************************
* Copyright (C) 2012-2018, Focaltech Systems (R)ㄛAll Rights Reserved.
* Copyright (C) 2019 XiaoMi, Inc.
*
* File Name: Focaltech_test_ft5x46.c
*
* Author: Focaltech Driver Team
*
* Created: 2015-07-14
*
* Abstract:
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "../focaltech_test.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

#define DEVIDE_MODE_ADDR    0x00
#define REG_LINE_NUM    0x01
#define REG_TX_NUM  0x02
#define REG_RX_NUM  0x03
#define REG_PATTERN        0x53
#define REG_MAPPING_SWITCH      0x54
#define REG_TX_NOMAPPING_NUM        0x55
#define REG_RX_NOMAPPING_NUM      0x56
#define REG_NORMALIZE_TYPE      0x16
#define REG_ScCbBuf0    0x4E
#define REG_ScWorkMode  0x44
#define REG_ScCbAddrR   0x45
#define REG_RawBuf0 0x36
#define REG_WATER_CHANNEL_SELECT 0x09

#define WEAK_SHORT_ADCDATA_LEN  ((1 + (1 + TX_NUM_MAX + RX_NUM_MAX) * 2) * 2)

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
enum WaterproofType {
	WT_NeedProofOnTest,
	WT_NeedProofOffTest,
	WT_NeedTxOnVal,
	WT_NeedRxOnVal,
	WT_NeedTxOffVal,
	WT_NeedRxOffVal,
};
struct stCfg_FT5X46_TestItem {
	bool FW_VERSION_TEST;
	bool FACTORY_ID_TEST;
	bool PROJECT_CODE_TEST;
	bool RAWDATA_TEST;
	bool SCAP_CB_TEST;
	bool SCAP_RAWDATA_TEST;
	bool CHANNEL_NUM_TEST;
	bool NOISE_TEST;
	bool PANEL_DIFFER_TEST;
	bool PANEL_DIFFER_UNIFORMITY_TEST;
	bool WEAK_SHORT_CIRCUIT_TEST;
	bool UNIFORMITY_TEST;
	bool SITO_RAWDATA_UNIFORMITY_TEST;
	bool LCD_NOISE_TEST;
};
struct stCfg_FT5X46_BasicThreshold {
	u8 FW_VER_VALUE;
	u8 Factory_ID_Number;
	char Project_Code[32];
	int RawDataTest_low_Min;
	int RawDataTest_Low_Max;
	int RawDataTest_high_Min;
	int RawDataTest_high_Max;
	u8 RawDataTest_SetLowFreq;
	u8 RawDataTest_SetHighFreq;
	int SCapCbTest_OFF_Min;
	int SCapCbTest_OFF_Max;
	int SCapCbTest_ON_Min;
	int SCapCbTest_ON_Max;
	bool SCapCbTest_LetTx_Disable;
	u8 SCapCbTest_SetWaterproof_OFF;
	u8 SCapCbTest_SetWaterproof_ON;
	int SCapRawDataTest_OFF_Min;
	int SCapRawDataTest_OFF_Max;
	int SCapRawDataTest_ON_Min;
	int SCapRawDataTest_ON_Max;
	bool SCapRawDataTest_LetTx_Disable;
	u8 SCapRawDataTest_SetWaterproof_OFF;
	u8 SCapRawDataTest_SetWaterproof_ON;
	bool bChannelTestMapping;
	bool bChannelTestNoMapping;
	u8 ChannelNumTest_TxNum;
	u8 ChannelNumTest_RxNum;
	u8 ChannelNumTest_TxNpNum;
	u8 ChannelNumTest_RxNpNum;
	int NoiseTest_Max;
	int GloveNoiseTest_Coefficient;
	int NoiseTest_Frames;
	int NoiseTest_Time;
	u8 NoiseTest_SampeMode;
	u8 NoiseTest_NoiseMode;
	u8 NoiseTest_ShowTip;
	bool bNoiseTest_GloveMode;
	int NoiseTest_RawdataMin;
	unsigned char Set_Frequency;
	bool bNoiseThreshold_Choose;
	int NoiseTest_Threshold;
	int NoiseTest_MinNgFrame;
	int WeakShortTest_CG;
	int WeakShortTest_CC;
	int WeakShortTest_CC_Rsen;
	bool WeakShortTest_CapShortTest;
	bool Uniformity_CheckTx;
	bool Uniformity_CheckRx;
	bool Uniformity_CheckMinMax;
	int  Uniformity_Tx_Hole;
	int  Uniformity_Rx_Hole;
	int  Uniformity_MinMax_Hole;
	int PanelDifferTest_Min;
	int PanelDifferTest_Max;
	bool PanelDiffer_Uniformity_CheckTx;
	bool PanelDiffer_Uniformity_CheckRx;
	bool PanelDiffer_Uniformity_CheckMinMax;
	int  PanelDiffer_Uniformity_Tx_Hole;
	int  PanelDiffer_Uniformity_Rx_Hole;
	int  PanelDiffer_Uniformity_MinMax_Hole;
	bool SITO_RawdtaUniformityTest_Check_Tx;
	bool SITO_RawdtaUniformityTest_Check_Rx;
	int  SITO_RawdtaUniformityTest_Tx_Hole;
	int  SITO_RawdtaUniformityTest_Rx_Hole;
	int Lcd_Noise_MaxFrame;
	int Lcd_Noise_Conficient;
	int Lcd_Noise_Noise_Mode;
	int Lcd_Noise_MaxNgPoint;
	int Lcd_Noise_FrameNum;
	bool Lcd_Noise_NoiseThresholdMode;
	int Lcd_Noise_NoiseCoefficient;
	int Lcd_Noise_NoiseMax;
	int Lcd_Noise_SetFrequency;
};
enum enumTestItem_FT5X46 {
	Code_FT5X46_ENTER_FACTORY_MODE,
	Code_FT5X46_DOWNLOAD,
	Code_FT5X46_UPGRADE,
	Code_FT5X46_FACTORY_ID_TEST,
	Code_FT5X46_PROJECT_CODE_TEST,
	Code_FT5X46_FW_VERSION_TEST,
	Code_FT5X46_IC_VERSION_TEST,
	Code_FT5X46_RAWDATA_TEST,
	Code_FT5X46_ADCDETECT_TEST,
	Code_FT5X46_SCAP_CB_TEST,
	Code_FT5X46_SCAP_RAWDATA_TEST,
	Code_FT5X46_CHANNEL_NUM_TEST,
	Code_FT5X46_INT_PIN_TEST,
	Code_FT5X46_RESET_PIN_TEST,
	Code_FT5X46_NOISE_TEST,
	Code_FT5X46_WEAK_SHORT_CIRCUIT_TEST,
	Code_FT5X46_UNIFORMITY_TEST,
	Code_FT5X46_CM_TEST,
	Code_FT5X46_RAWDATA_MARGIN_TEST,
	Code_FT5X46_WRITE_CONFIG,
	Code_FT5X46_PANELDIFFER_TEST,
	Code_FT5X46_PANELDIFFER_UNIFORMITY_TEST,
	Code_FT5X46_LCM_ID_TEST,
	Code_FT5X46_JUDEG_NORMALIZE_TYPE,
	Code_FT5X46_TE_TEST,
	Code_FT5X46_SITO_RAWDATA_UNIFORMITY_TEST,
	Code_FT5X46_PATTERN_TEST,
	Code_FT5X46_GPIO_TEST,
	Code_FT5X46_LCD_NOISE_TEST,
};
/*****************************************************************************
* Static variables
*****************************************************************************/

static int m_RawData[TX_NUM_MAX][RX_NUM_MAX] = { {0} };
static int m_iTempRawData[TX_NUM_MAX * RX_NUM_MAX] = {0};
static unsigned char m_ucTempData[TX_NUM_MAX * RX_NUM_MAX * 2] = {0};
static bool m_bV3TP;
static int m_DifferData[TX_NUM_MAX][RX_NUM_MAX] = { {0} };
static int m_absDifferData[TX_NUM_MAX][RX_NUM_MAX] = { {0} };
static int TxLinearity[TX_NUM_MAX][RX_NUM_MAX] = { {0} };
static int RxLinearity[TX_NUM_MAX][RX_NUM_MAX] = { {0} };

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct stCfg_FT5X46_TestItem g_stCfg_FT5X46_TestItem;
struct stCfg_FT5X46_BasicThreshold g_stCfg_FT5X46_BasicThreshold;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

static int StartScan(void);
static unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer);
static unsigned char GetPanelRows(unsigned char *pPanelRows);
static unsigned char GetPanelCols(unsigned char *pPanelCols);
static unsigned char GetTxSC_CB(unsigned char index, unsigned char *pcbValue);
static unsigned char GetRawData(void);
static unsigned char GetChannelNum(void);
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], char *test_num, int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount);
static void ShowRawData(void);
static bool GetTestCondition(int iTestType, unsigned char ucChannelValue);
static unsigned char GetChannelNumNoMapping(void);
static unsigned char SwitchToNoMapping(void);
static unsigned char WeakShort_GetAdcData(int AllAdcDataLen, int *pRevBuffer);
unsigned char FT5X46_TestItem_EnterFactoryMode(void);
unsigned char FT5X46_TestItem_RawDataTest(bool *bTestResult);
unsigned char FT5X46_TestItem_SCapRawDataTest(bool *bTestResult);
unsigned char FT5X46_TestItem_SCapCbTest(bool *bTestResult);
unsigned char FT5X46_TestItem_PanelDifferTest(bool *bTestResult);
unsigned char FT5X46_TestItem_PanelDiffer_Uniformity_Test(bool *bTestResult);
unsigned char FT5X46_TestItem_WeakShortTest(bool *bTestResult);


/************************************************************************
* Name: start_test_ft5x46
* Brief:  Test entry. Determine which test item to test
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/
bool start_test_ft5x46(void)
{
	bool bTestResult = true;
	bool bTempResult = 1;
	unsigned char ReCode = 0;
	int iItemCount = 0;

	FTS_TEST_FUNC_ENTER();


	if (0 == test_data.test_num)
		bTestResult = false;

	for (iItemCount = 0; iItemCount < test_data.test_num; iItemCount++) {
		test_data.test_item_code = test_data.test_item[iItemCount].itemcode;


		if (Code_FT5X46_ENTER_FACTORY_MODE == test_data.test_item[iItemCount].itemcode
		  ) {

			ReCode = FT5X46_TestItem_EnterFactoryMode();
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				test_data.test_item[iItemCount].testresult = RESULT_NG;
				break;
			} else
				test_data.test_item[iItemCount].testresult = RESULT_PASS;
		}


		if (Code_FT5X46_RAWDATA_TEST == test_data.test_item[iItemCount].itemcode
		  ) {

			ReCode = FT5X46_TestItem_RawDataTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				test_data.test_item[iItemCount].testresult = RESULT_NG;
			} else
				test_data.test_item[iItemCount].testresult = RESULT_PASS;
		}



		if (Code_FT5X46_SCAP_CB_TEST == test_data.test_item[iItemCount].itemcode
		  ) {
			ReCode = FT5X46_TestItem_SCapCbTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				test_data.test_item[iItemCount].testresult = RESULT_NG;
			} else
				test_data.test_item[iItemCount].testresult = RESULT_PASS;
		}


		if (Code_FT5X46_SCAP_RAWDATA_TEST == test_data.test_item[iItemCount].itemcode
		  ) {
			ReCode = FT5X46_TestItem_SCapRawDataTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				test_data.test_item[iItemCount].testresult = RESULT_NG;
			} else
				test_data.test_item[iItemCount].testresult = RESULT_PASS;
		}


		if (Code_FT5X46_PANELDIFFER_TEST == test_data.test_item[iItemCount].itemcode
		  ) {

			ReCode = FT5X46_TestItem_PanelDifferTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				test_data.test_item[iItemCount].testresult = RESULT_NG;
			} else
				test_data.test_item[iItemCount].testresult = RESULT_PASS;
		}


		if (Code_FT5X46_PANELDIFFER_UNIFORMITY_TEST == test_data.test_item[iItemCount].itemcode
		  ) {

			ReCode = FT5X46_TestItem_PanelDiffer_Uniformity_Test(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				test_data.test_item[iItemCount].testresult = RESULT_NG;
			} else
				test_data.test_item[iItemCount].testresult = RESULT_PASS;
		}


		if (Code_FT5X46_WEAK_SHORT_CIRCUIT_TEST == test_data.test_item[iItemCount].itemcode
		  ) {

			ReCode = FT5X46_TestItem_WeakShortTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				test_data.test_item[iItemCount].testresult = RESULT_NG;
			} else
				test_data.test_item[iItemCount].testresult = RESULT_PASS;
		}


	}


	return bTestResult;
}

bool start_selftest_ft5x46(int temp)
{
	bool bTestResult = true;
	bool bTempResult = 1;
	unsigned char ReCode = 0;

	FTS_TEST_FUNC_ENTER();


	if (temp) {

			ReCode = FT5X46_TestItem_EnterFactoryMode();
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				FTS_TEST_ERROR("EnterFactoryMode error.\n");
			}

		switch (temp) {
		case TP_SELFTEST_SHORT:

			ReCode = FT5X46_TestItem_WeakShortTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				FTS_TEST_ERROR("WeakShortTest error.\n");
			}
			break;
		case TP_SELFTEST_OPEN:

			ReCode = FT5X46_TestItem_PanelDifferTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				FTS_TEST_ERROR("PanelDifferTest error.\n");
				break;
			}

			ReCode = FT5X46_TestItem_PanelDiffer_Uniformity_Test(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				FTS_TEST_ERROR("Uniformity_Test error.\n");
			}
			break;
		}
	}


	return bTestResult;
}

/************************************************************************
* Name: FT5X46_TestItem_EnterFactoryMode
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5X46_TestItem_EnterFactoryMode(void)
{
	unsigned char ReCode = ERROR_CODE_INVALID_PARAM;
	int iRedo = 5;
	int i ;
	unsigned char chPattern = 0;

	FTS_TEST_FUNC_ENTER();

	sys_delay(150);
	for (i = 1; i <= iRedo; i++) {
		ReCode = enter_factory_mode();
		if (ERROR_CODE_OK != ReCode) {
			FTS_TEST_SAVE_INFO("\nFailed to Enter factory mode...\n");
			if (i < iRedo) {
				sys_delay(50);
				continue;
			}
		} else {
			break;
		}

	}
	sys_delay(300);


	if (ReCode != ERROR_CODE_OK) {
		return ReCode;
	}


	ReCode = GetChannelNum();


	ReCode = read_reg(REG_PATTERN, &chPattern);
	if (chPattern == 1) {
		m_bV3TP = true;
	} else {
		m_bV3TP = false;
	}

	return ReCode;
}
/************************************************************************
* Name: FT5X46_TestItem_RawDataTest
* Brief:  TestItem: RawDataTest. Check if MCAP RawData is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5X46_TestItem_RawDataTest(bool *bTestResult)
{
	unsigned char ReCode = 0;
	bool btmpresult = true;
	int RawDataMin;
	int RawDataMax;
	unsigned char ucFre;
	unsigned char ucFir;
	unsigned char strSwitch = 0;
	unsigned char OriginValue = 0xff;
	int index = 0;
	int iRow, iCol;
	int iValue = 0;
	int nRawDataOK = 0;


	FTS_TEST_SAVE_INFO("\n\n\n==============================Test Item: -------- Raw Data  Test \n");
	ReCode = enter_factory_mode();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n\n// Failed to Enter factory Mode. Error Code: %d", ReCode);
		goto TEST_ERR;
	}




	if (m_bV3TP) {
		ReCode = read_reg(REG_MAPPING_SWITCH, &strSwitch);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n Read REG_MAPPING_SWITCH error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}

		if (strSwitch != 0) {
			ReCode = write_reg(REG_MAPPING_SWITCH, 0);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n Write REG_MAPPING_SWITCH error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}

			ReCode = GetChannelNum();
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n GetChannelNum error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}
		}
	}


	ReCode = read_reg(REG_NORMALIZE_TYPE, &OriginValue);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Read  REG_NORMALIZE_TYPE error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}
	ReCode =  read_reg(0x0A, &ucFre);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Read  0x0A error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}
	ReCode =  read_reg(0xFB, &ucFir);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Read  0xFB error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}

	if (test_data.screen_param.normalize == AUTO_NORMALIZE) {
		if (OriginValue != 1) {
			ReCode = write_reg(REG_NORMALIZE_TYPE, 0x01);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n write  REG_NORMALIZE_TYPE error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}
		}


		FTS_TEST_SAVE_INFO("\n=========Set Frequecy High\n");
		ReCode = write_reg(0x0A, 0x81);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n Set Frequecy High error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}

		FTS_TEST_SAVE_INFO("\n=========FIR State: ON \n");
		ReCode = write_reg(0xFB, 1);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n FIR State: ON error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}


		for (index = 0; index < 3; ++index) {
			ReCode = GetRawData();
		}

		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\nGet Rawdata failed, Error Code: 0x%x",  ReCode);
			goto TEST_ERR;
		}

		ShowRawData();


		for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {
			for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
				if (test_data.mcap_detail_thr.invalid_node[iRow][iCol] == 0)
					continue;
				RawDataMin = test_data.mcap_detail_thr.rawdata_test_high_min[iRow][iCol];
				RawDataMax = test_data.mcap_detail_thr.rawdata_test_high_max[iRow][iCol];
				iValue = m_RawData[iRow][iCol];
				if (iValue < RawDataMin || iValue > RawDataMax) {
					btmpresult = false;
					FTS_TEST_SAVE_INFO("\nrawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
									   iRow + 1, iCol + 1, iValue, RawDataMin, RawDataMax);
				}
			}
		}


		Save_Test_Data(m_RawData, "RawData Test", 0, test_data.screen_param.tx_num, test_data.screen_param.rx_num, 2);
	} else {
		if (OriginValue != 0) {
			ReCode = write_reg(REG_NORMALIZE_TYPE, 0x00);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n write REG_NORMALIZE_TYPE error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}
		}


		if (g_stCfg_FT5X46_BasicThreshold.RawDataTest_SetLowFreq) {
			FTS_TEST_SAVE_INFO("\n\n=========Set Frequecy Low");
			ReCode = write_reg(0x0A, 0x80);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n write frequency error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}



			FTS_TEST_SAVE_INFO("\n\n=========FIR State: OFF\n");
			ReCode = write_reg(0xFB, 0);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n FIR State: OFF error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}
			sys_delay(100);
			for (index = 0, nRawDataOK = 0; index < 10 && nRawDataOK < 3; ++index) {
				ReCode = GetRawData();
				if (ReCode != ERROR_CODE_OK) {
					FTS_TEST_SAVE_INFO("\nGet Rawdata failed, index:%d. Error Code: 0x%x", index, ReCode);
				} else
					nRawDataOK++;
			}

			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\nGet Rawdata failed, Error Code: 0x%x",  ReCode);
				goto TEST_ERR;
			}
			ShowRawData();


			for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {

				for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
					if (test_data.mcap_detail_thr.invalid_node[iRow][iCol] == 0)
						continue;
					RawDataMin = test_data.mcap_detail_thr.rawdata_test_low_min[iRow][iCol];
					RawDataMax = test_data.mcap_detail_thr.rawdata_test_low_max[iRow][iCol];
					iValue = m_RawData[iRow][iCol];
					if (iValue < RawDataMin || iValue > RawDataMax) {
						btmpresult = false;
						FTS_TEST_SAVE_INFO("\nrawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) ",  \
										   iRow + 1, iCol + 1, iValue, RawDataMin, RawDataMax);
					}
				}
			}


			Save_Test_Data(m_RawData, "RawData Test", 0, test_data.screen_param.tx_num, test_data.screen_param.rx_num, 1);
		}



		if (g_stCfg_FT5X46_BasicThreshold.RawDataTest_SetHighFreq) {

			FTS_TEST_SAVE_INFO("\n=========Set Frequecy High");
			ReCode = write_reg(0x0A, 0x81);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n Set Frequecy High error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}



			FTS_TEST_SAVE_INFO("\n\n=========FIR State: OFF\n");
			ReCode = write_reg(0xFB, 0);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n FIR State: OFF error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}
			sys_delay(100);

			for (index = 0; index < 3; ++index) {
				ReCode = GetRawData();
			}

			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\nGet Rawdata failed, Error Code: 0x%x",  ReCode);
				if (ReCode != ERROR_CODE_OK)
					goto TEST_ERR;
			}
			ShowRawData();


			for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {

				for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
					if (test_data.mcap_detail_thr.invalid_node[iRow][iCol] == 0)
						continue;
					RawDataMin = test_data.mcap_detail_thr.rawdata_test_high_min[iRow][iCol];
					RawDataMax = test_data.mcap_detail_thr.rawdata_test_high_max[iRow][iCol];
					iValue = m_RawData[iRow][iCol];
					if (iValue < RawDataMin || iValue > RawDataMax) {
						btmpresult = false;
						FTS_TEST_SAVE_INFO("\nrawdata test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n",  \
										   iRow + 1, iCol + 1, iValue, RawDataMin, RawDataMax);
					}
				}
			}


			Save_Test_Data(m_RawData, "RawData Test", 0, test_data.screen_param.tx_num, test_data.screen_param.rx_num, 2);
		}

	}

	ReCode = write_reg(REG_NORMALIZE_TYPE, OriginValue);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Write REG_NORMALIZE_TYPE error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}
	ReCode = write_reg(0x0A, ucFre);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Write 0x0A error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}
	ReCode = write_reg(0xFB, ucFir);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Write 0xFB error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}


	if (m_bV3TP) {
		ReCode = write_reg(REG_MAPPING_SWITCH, strSwitch);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n Write REG_MAPPING_SWITCH error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}
	}


	if (btmpresult) {
		*bTestResult = true;
		FTS_TEST_SAVE_INFO("\n\n\n/==========RawData Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_SAVE_INFO("\n\n\n/==========RawData Test is NG!");
	}
	return ReCode;

TEST_ERR:

	*bTestResult = false;
	FTS_TEST_SAVE_INFO("\n\n\n/==========RawData Test is NG!\n");
	return ReCode;

}
/************************************************************************
* Name: FT5X46_TestItem_SCapRawDataTest
* Brief:  TestItem: SCapRawDataTest. Check if SCAP RawData is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5X46_TestItem_SCapRawDataTest(bool *bTestResult)
{
	int i = 0;
	int RawDataMin = 0;
	int RawDataMax = 0;
	int Value = 0;
	bool bFlag = true;
	unsigned char ReCode = 0;
	bool btmpresult = true;
	int iMax = 0;
	int iMin = 0;
	int iAvg = 0;
	int ByteNum = 0;
	unsigned char wc_value = 0;
	unsigned char ucValue = 0;
	int iCount = 0;

	FTS_TEST_SAVE_INFO("\n\n\n==============================Test Item: -------- Scap RawData Test \n");


	ReCode = enter_factory_mode();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n\n// Failed to Enter factory Mode. Error Code: %d", ReCode);
		goto TEST_ERR;
	}


	ReCode = read_reg(REG_WATER_CHANNEL_SELECT, &wc_value);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n\n// Failed to read REG_WATER_CHANNEL_SELECT. Error Code: %d", ReCode);
		goto TEST_ERR;
	}


	ReCode = SwitchToNoMapping();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n\n// Failed to SwitchToNoMapping. Error Code: %d", ReCode);
		goto TEST_ERR;
	}


	ReCode = StartScan();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nFailed to Scan SCap RawData! ");
		goto TEST_ERR;
	}
	for (i = 0; i < 3; i++) {
		memset(m_iTempRawData, 0, sizeof(m_iTempRawData));


		ByteNum = (test_data.screen_param.tx_num + test_data.screen_param.rx_num) * 2;
		ReCode = ReadRawData(0, 0xAC, ByteNum, m_iTempRawData);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nFailed to ReadRawData water! ");
			goto TEST_ERR;
		}

		memcpy(m_RawData[0 + test_data.screen_param.tx_num], m_iTempRawData, sizeof(int)*test_data.screen_param.rx_num);
		memcpy(m_RawData[1 + test_data.screen_param.tx_num], m_iTempRawData + test_data.screen_param.rx_num, sizeof(int)*test_data.screen_param.tx_num);


		ByteNum = (test_data.screen_param.tx_num + test_data.screen_param.rx_num) * 2;
		ReCode = ReadRawData(0, 0xAB, ByteNum, m_iTempRawData);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nFailed to ReadRawData no water! ");
			goto TEST_ERR;
		}
		memcpy(m_RawData[2 + test_data.screen_param.tx_num], m_iTempRawData, sizeof(int)*test_data.screen_param.rx_num);
		memcpy(m_RawData[3 + test_data.screen_param.tx_num], m_iTempRawData + test_data.screen_param.rx_num, sizeof(int)*test_data.screen_param.tx_num);
	}





	bFlag = GetTestCondition(WT_NeedProofOnTest, wc_value);
	if (g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_SetWaterproof_ON && bFlag) {


		FTS_TEST_SAVE_INFO("\n////////////SCapRawdataTest in WaterProof On Mode:  \n");
		FTS_TEST_SAVE_INFO("\nSCap Rawdata_Rx:  ");
		for (i = 0; i < test_data.screen_param.rx_num; i++) {
			FTS_TEST_SAVE_INFO("%5d    ", m_RawData[0 + test_data.screen_param.tx_num][i]);
		}
		FTS_TEST_SAVE_INFO("\n \nSCap Rawdata_Tx:  ");
		for (i = 0; i < test_data.screen_param.tx_num; i++) {
			FTS_TEST_SAVE_INFO("%5d    ", m_RawData[1 + test_data.screen_param.tx_num][i]);
		}

		iCount = 0;
		RawDataMin = g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_ON_Min;
		RawDataMax = g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_ON_Max;
		iMax = -m_RawData[0 + test_data.screen_param.tx_num][0];
		iMin = 2 * m_RawData[0 + test_data.screen_param.tx_num][0];
		iAvg = 0;
		Value = 0;
		bFlag = GetTestCondition(WT_NeedRxOnVal, wc_value);
		for (i = 0; bFlag && i < test_data.screen_param.rx_num; i++) {
			if (test_data.mcap_detail_thr.invalid_node_sc[0][i] == 0)
				continue;
			RawDataMin = test_data.mcap_detail_thr.scap_rawdata_on_min[0][i];
			RawDataMax = test_data.mcap_detail_thr.scap_rawdata_on_max[0][i];
			Value = m_RawData[0 + test_data.screen_param.tx_num][i];
			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\n\n Failed. Num = %d, Value = %d, range = (%d, %d) \n",  i + 1, Value, RawDataMin, RawDataMax);
			}
			iCount++;
		}
		bFlag = GetTestCondition(WT_NeedTxOnVal, wc_value);
		for (i = 0; bFlag && i < test_data.screen_param.tx_num; i++) {
			if (test_data.mcap_detail_thr.invalid_node_sc[1][i] == 0)
				continue;
			RawDataMin = test_data.mcap_detail_thr.scap_rawdata_on_min[1][i];
			RawDataMax = test_data.mcap_detail_thr.scap_rawdata_on_max[1][i];
			Value = m_RawData[1 + test_data.screen_param.tx_num][i];
			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\n\nFailed. Num = %d, Value = %d, range = (%d, %d) \n",  i + 1, Value, RawDataMin, RawDataMax);
			}
			iCount++;
		}
		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg / iCount;
		Save_Test_Data(m_RawData, "SCap RawData Test", test_data.screen_param.tx_num + 0, 2, test_data.screen_param.rx_num, 1);
	}


	bFlag = GetTestCondition(WT_NeedProofOffTest, wc_value);
	if (g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_SetWaterproof_OFF && bFlag) {

		FTS_TEST_SAVE_INFO("\n\n ////////////SCapRawdataTest in WaterProof Off Mode:  \n");
		FTS_TEST_SAVE_INFO("\nSCap Rawdata_Rx:  ");
		for (i = 0; i < test_data.screen_param.rx_num; i++) {
			FTS_TEST_SAVE_INFO("%5d    ", m_RawData[2 + test_data.screen_param.tx_num][i]);
		}
		FTS_TEST_SAVE_INFO("\n \nSCap Rawdata_Tx:  ");
		for (i = 0; i < test_data.screen_param.tx_num; i++) {
			FTS_TEST_SAVE_INFO("%5d    ", m_RawData[3 + test_data.screen_param.tx_num][i]);
		}

		iCount = 0;
		RawDataMin = g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_OFF_Min;
		RawDataMax = g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_OFF_Max;
		iMax = -m_RawData[2 + test_data.screen_param.tx_num][0];
		iMin = 2 * m_RawData[2 + test_data.screen_param.tx_num][0];
		iAvg = 0;
		Value = 0;
		bFlag = GetTestCondition(WT_NeedRxOffVal, wc_value);
		for (i = 0; bFlag && i < test_data.screen_param.rx_num; i++) {
			if (test_data.mcap_detail_thr.invalid_node_sc[0][i] == 0)
				continue;
			RawDataMin = test_data.mcap_detail_thr.scap_rawdata_off_min[0][i];
			RawDataMax = test_data.mcap_detail_thr.scap_rawdata_off_max[0][i];
			Value = m_RawData[2 + test_data.screen_param.tx_num][i];
			iAvg += Value;



			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\n\n Failed. Num = %d, Value = %d, range = (%d, %d) \n",  i + 1, Value, RawDataMin, RawDataMax);
			}
			iCount++;
		}
		bFlag = GetTestCondition(WT_NeedTxOffVal, wc_value);
		for (i = 0; bFlag && i < test_data.screen_param.tx_num; i++) {
			if (test_data.mcap_detail_thr.invalid_node_sc[1][i] == 0)
				continue;

			Value = m_RawData[3 + test_data.screen_param.tx_num][i];
			RawDataMin = test_data.mcap_detail_thr.scap_rawdata_off_min[1][i];
			RawDataMax = test_data.mcap_detail_thr.scap_rawdata_off_max[1][i];

			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\n\n Failed. Num = %d, Value = %d, range = (%d, %d) \n",  i + 1, Value, RawDataMin, RawDataMax);
			}
			iCount++;
		}
		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg / iCount;
		Save_Test_Data(m_RawData, "SCap RawData Test", test_data.screen_param.tx_num + 2, 2, test_data.screen_param.rx_num, 2);
	}

	if (m_bV3TP) {
		ReCode = read_reg(REG_MAPPING_SWITCH, &ucValue);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n Read REG_MAPPING_SWITCH error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}

		if (0 != ucValue) {
			ReCode = write_reg(REG_MAPPING_SWITCH, 0);
			sys_delay(10);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\nFailed to switch mapping type!\n ");
				btmpresult = false;
			}
		}


		ReCode = GetChannelNum();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n GetChannelNum error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}
	}


	if (btmpresult) {
		*bTestResult = true;
		FTS_TEST_SAVE_INFO("\n\n\n/==========SCap RawData Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_SAVE_INFO("\n\n\n/==========SCap RawData Test is NG!");
	}
	return ReCode;

TEST_ERR:
	*bTestResult = false;
	FTS_TEST_SAVE_INFO("\n\n\n/==========SCap RawData Test is NG!");
	return ReCode;
}

/************************************************************************
* Name: FT5X46_TestItem_SCapCbTest
* Brief:  TestItem: SCapCbTest. Check if SCAP Cb is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5X46_TestItem_SCapCbTest(bool *bTestResult)
{
	int i,/* j, iOutNum,*/index, Value, CBMin, CBMax;
	bool bFlag = true;
	unsigned char ReCode;
	bool btmpresult = true;
	int iMax, iMin, iAvg;
	unsigned char wc_value = 0;
	unsigned char sc_wrok_mode = 0;
	unsigned char ucValue = 0;
	int iCount = 0;


	FTS_TEST_SAVE_INFO("\n\n\n==============================Test Item: -----  Scap CB Test \n");


	ReCode = enter_factory_mode();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n\n// Failed to Enter factory Mode. Error Code: %d", ReCode);
		goto TEST_ERR;
	}


	ReCode = read_reg(REG_WATER_CHANNEL_SELECT, &wc_value);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Read REG_WATER_CHANNEL_SELECT error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}
	ReCode = read_reg(REG_ScWorkMode, &sc_wrok_mode);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Read REG_ScWorkMode error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}



	bFlag = SwitchToNoMapping();
	if (bFlag) {
		FTS_TEST_SAVE_INFO("\nFailed to SwitchToNoMapping! ReCode = %d. ",  ReCode);
		goto TEST_ERR;
	}


	ReCode = StartScan();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nFailed to Scan SCap RawData!ReCode = %d. ",  ReCode);
		goto TEST_ERR;
	}


	for (i = 0; i < 3; i++) {
		memset(m_RawData, 0, sizeof(m_RawData));
		memset(m_ucTempData, 0, sizeof(m_ucTempData));


		ReCode = write_reg(REG_ScWorkMode, 1);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nGet REG_ScWorkMode Failed!");
			goto TEST_ERR;
		}

		ReCode = StartScan();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nStartScan Failed!");
			goto TEST_ERR;
		}

		ReCode = write_reg(REG_ScCbAddrR, 0);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nWrite REG_ScCbAddrR Failed!");
			goto TEST_ERR;
		}

		ReCode = GetTxSC_CB(test_data.screen_param.tx_num + test_data.screen_param.rx_num + 128, m_ucTempData);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nGetTxSC_CB Failed!");
			goto TEST_ERR;
		}

		for (index = 0; index < test_data.screen_param.rx_num; ++index) {
			m_RawData[0 + test_data.screen_param.tx_num][index] = m_ucTempData[index];
		}
		for (index = 0; index < test_data.screen_param.tx_num; ++index) {
			m_RawData[1 + test_data.screen_param.tx_num][index] = m_ucTempData[index + test_data.screen_param.rx_num];
		}


		ReCode = write_reg(REG_ScWorkMode, 0);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nGet REG_ScWorkMode Failed!");
			goto TEST_ERR;
		}

		ReCode = StartScan();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nStartScan Failed!");
			goto TEST_ERR;
		}

		ReCode = write_reg(REG_ScCbAddrR, 0);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nWrite REG_ScCbAddrR Failed!");
			goto TEST_ERR;
		}

		ReCode = GetTxSC_CB(test_data.screen_param.tx_num + test_data.screen_param.rx_num + 128, m_ucTempData);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nGetTxSC_CB Failed!");
			goto TEST_ERR;
		}
		for (index = 0; index < test_data.screen_param.rx_num; ++index) {
			m_RawData[2 + test_data.screen_param.tx_num][index] = m_ucTempData[index];
		}
		for (index = 0; index < test_data.screen_param.tx_num; ++index) {
			m_RawData[3 + test_data.screen_param.tx_num][index] = m_ucTempData[index + test_data.screen_param.rx_num];
		}

		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nFailed to Get SCap CB!");
		}
	}

	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;




	bFlag = GetTestCondition(WT_NeedProofOnTest, wc_value);
	if (g_stCfg_FT5X46_BasicThreshold.SCapCbTest_SetWaterproof_ON && bFlag) {

		FTS_TEST_SAVE_INFO("\n////////////SCapCbTest in WaterProof On Mode:  \n");
		FTS_TEST_SAVE_INFO("\nSCap CB_Rx:  ");
		for (i = 0; i < test_data.screen_param.rx_num; i++) {
			FTS_TEST_SAVE_INFO("%5d", m_RawData[0 + test_data.screen_param.tx_num][i]);
		}
		FTS_TEST_SAVE_INFO("\n \nSCap CB_Tx:  ");
		for (i = 0; i < test_data.screen_param.tx_num; i++) {
			FTS_TEST_SAVE_INFO("%5d", m_RawData[1 + test_data.screen_param.tx_num][i]);
		}

		iMax = -m_RawData[0 + test_data.screen_param.tx_num][0];
		iMin = 2 * m_RawData[0 + test_data.screen_param.tx_num][0];
		iAvg = 0;
		Value = 0;
		iCount = 0;
		bFlag = GetTestCondition(WT_NeedRxOnVal, wc_value);
		for (i = 0; bFlag && i < test_data.screen_param.rx_num; i++) {
			if (test_data.mcap_detail_thr.invalid_node_sc[0][i] == 0)
				continue;
			CBMin = test_data.mcap_detail_thr.scap_cb_test_on_min[0][i];
			CBMax = test_data.mcap_detail_thr.scap_cb_test_on_max[0][i];
			Value = m_RawData[0 + test_data.screen_param.tx_num][i];
			iAvg += Value;

			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\n \n Failed. Num = %d, Value = %d, range = (%d, %d) \n",  i + 1, Value, CBMin, CBMax);
			}
			iCount++;
		}
		bFlag = GetTestCondition(WT_NeedTxOnVal, wc_value);
		for (i = 0; bFlag &&  i < test_data.screen_param.tx_num; i++) {
			if (test_data.mcap_detail_thr.invalid_node_sc[1][i] == 0)
				continue;
			CBMin = test_data.mcap_detail_thr.scap_cb_test_on_min[1][i];
			CBMax = test_data.mcap_detail_thr.scap_cb_test_on_max[1][i];
			Value = m_RawData[1 + test_data.screen_param.tx_num][i];
			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\n\n Failed. Num = %d, Value = %d, range = (%d, %d) \n",  i + 1, Value, CBMin, CBMax);
			}
			iCount++;
		}

		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg / iCount;
		Save_Test_Data(m_RawData, "SCap CB Test", test_data.screen_param.tx_num + 0, 2, test_data.screen_param.rx_num, 1);
	}

	bFlag = GetTestCondition(WT_NeedProofOffTest, wc_value);
	if (g_stCfg_FT5X46_BasicThreshold.SCapCbTest_SetWaterproof_OFF && bFlag) {

		FTS_TEST_SAVE_INFO("\n\n////////////SCapCbTest in WaterProof Off Mode:  \n");
		FTS_TEST_SAVE_INFO("\nSCap CB_Rx:  ");
		for (i = 0; i < test_data.screen_param.rx_num; i++) {
			FTS_TEST_SAVE_INFO("%5d", m_RawData[2 + test_data.screen_param.tx_num][i]);
		}
		FTS_TEST_SAVE_INFO("\n \nSCap CB_Tx:  ");
		for (i = 0; i < test_data.screen_param.tx_num; i++) {
			FTS_TEST_SAVE_INFO("%5d", m_RawData[3 + test_data.screen_param.tx_num][i]);
		}

		iMax = -m_RawData[2 + test_data.screen_param.tx_num][0];
		iMin = 2 * m_RawData[2 + test_data.screen_param.tx_num][0];
		iAvg = 0;
		Value = 0;
		iCount = 0;
		bFlag = GetTestCondition(WT_NeedRxOffVal, wc_value);
		for (i = 0; bFlag &&  i < test_data.screen_param.rx_num; i++) {
			if (test_data.mcap_detail_thr.invalid_node_sc[0][i] == 0)
				continue;
			CBMin = test_data.mcap_detail_thr.scap_cb_test_off_min[0][i];
			CBMax = test_data.mcap_detail_thr.scap_cb_test_off_max[0][i];
			Value = m_RawData[2 + test_data.screen_param.tx_num][i];
			iAvg += Value;

			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\n\n Failed. Num = %d, Value = %d, range = (%d, %d) \n",  i + 1, Value, CBMin, CBMax);
			}
			iCount++;
		}
		bFlag = GetTestCondition(WT_NeedTxOffVal, wc_value);
		for (i = 0; bFlag && i < test_data.screen_param.tx_num; i++) {

			if (test_data.mcap_detail_thr.invalid_node_sc[1][i] == 0)
				continue;
			CBMin = test_data.mcap_detail_thr.scap_cb_test_off_min[1][i];
			CBMax = test_data.mcap_detail_thr.scap_cb_test_off_max[1][i];
			Value = m_RawData[3 + test_data.screen_param.tx_num][i];

			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\n\n Failed. Num = %d, Value = %d, range = (%d, %d) \n",  i + 1, Value, CBMin, CBMax);
			}
			iCount++;
		}

		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg / iCount;
		Save_Test_Data(m_RawData, "SCap CB Test", test_data.screen_param.tx_num + 2, 2, test_data.screen_param.rx_num, 2);
	}

	if (m_bV3TP) {
		ReCode = read_reg(REG_MAPPING_SWITCH, &ucValue);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n Read REG_MAPPING_SWITCH error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}

		if (0 != ucValue) {
			ReCode = write_reg(REG_MAPPING_SWITCH, 0);
			sys_delay(10);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\nFailed to switch mapping type!\n ");
				btmpresult = false;
			}
		}


		ReCode = GetChannelNum();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n GetChannelNum error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}
	}

	ReCode = write_reg(REG_ScWorkMode, sc_wrok_mode);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Write REG_ScWorkMode error. Error Code: %d",  ReCode);
		goto TEST_ERR;
	}




	if (btmpresult) {
		*bTestResult = true;
		FTS_TEST_SAVE_INFO("\n\n\n/==========SCap CB Test Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_SAVE_INFO("\n\n\n/==========SCap CB Test Test is NG!");
	}
	return ReCode;

TEST_ERR:

	*bTestResult = false;
	FTS_TEST_SAVE_INFO("\n\n\n/==========SCap CB Test Test is NG!");
	return ReCode;
}

/************************************************************************
* Name: FT5X46_TestItem_PanelDifferTest
* Brief:  TestItem: PanelDifferTest. Check if Panel Differ is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5X46_TestItem_PanelDifferTest(bool *bTestResult)
{
	int index = 0;
	int iRow = 0, iCol = 0;
	int iValue = 0;
	unsigned char ReCode = 0, strSwitch = -1;
	bool btmpresult = true;
	int iMax, iMin;
	int maxValue = 0;
	int minValue = 32767;
	int AvgValue = 0;
	int InvalidNum = 0;
	int i = 0,  j = 0;

	unsigned char OriginRawDataType = 0xff;
	unsigned char OriginFrequecy = 0xff;
	unsigned char OriginFirState = 0xff;


	FTS_TEST_SAVE_INFO("\n\r\n\r\n\r\n==============================Test Item: -------- Panel Differ Test  \r\n\r\n");

	ReCode = enter_factory_mode();
	sys_delay(20);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n\n// Failed to Enter factory Mode. Error Code: %d", ReCode);
		goto TEST_ERR;
	}


	if (m_bV3TP) {
		ReCode = read_reg(REG_MAPPING_SWITCH, &strSwitch);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n Read REG_MAPPING_SWITCH error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}

		if (strSwitch != 0) {
			ReCode = write_reg(REG_MAPPING_SWITCH, 0);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n Write REG_MAPPING_SWITCH error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}

			ReCode = GetChannelNum();
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\n GetChannelNum error. Error Code: %d",  ReCode);
				goto TEST_ERR;
			}

			ReCode = GetRawData();
		}
	}


	FTS_TEST_SAVE_INFO("\n\r\n=========Set Auto Equalization:\r\n");
	ReCode = read_reg(REG_NORMALIZE_TYPE, &OriginRawDataType);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nRead  REG_NORMALIZE_TYPE error. Error Code: %d",  ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}

	if (OriginRawDataType != 0) {
		ReCode = write_reg(REG_NORMALIZE_TYPE, 0x00);
		sys_delay(50);
		if (ReCode != ERROR_CODE_OK) {
			btmpresult = false;
			FTS_TEST_SAVE_INFO("\nWrite reg REG_NORMALIZE_TYPE Failed.");
			goto TEST_ERR;
		}
	}



	FTS_TEST_SAVE_INFO("\n=========Set Frequecy High");
	ReCode = read_reg(0x0A, &OriginFrequecy);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nRead reg 0x0A error. Error Code: %d",  ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}

	ReCode = write_reg(0x0A, 0x81);
	sys_delay(10);
	if (ReCode != ERROR_CODE_OK) {
		btmpresult = false;
		FTS_TEST_SAVE_INFO("\nWrite reg 0x0A Failed.");
		goto TEST_ERR;
	}

	FTS_TEST_SAVE_INFO("\n=========FIR State: OFF");
	ReCode = read_reg(0xFB, &OriginFirState);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nRead reg 0xFB error. Error Code: %d",  ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}
	ReCode = write_reg(0xFB, 0);
	sys_delay(50);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nWrite reg 0xFB Failed.");
		btmpresult = false;
		goto TEST_ERR;
	}
	ReCode = GetRawData();

	for (index = 0; index < 4; ++index) {
		ReCode = GetRawData();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nGetRawData Failed.");
			btmpresult = false;
			goto TEST_ERR;
		}
	}


	for (i = 0; i < test_data.screen_param.tx_num; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			m_DifferData[i][j] = m_RawData[i][j] / 10;
		}
	}


#if 1
	FTS_TEST_SAVE_INFO("\nPannelDiffer :\n");
	for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {
		FTS_TEST_SAVE_INFO("\n\nTx%2d:    ", iRow + 1);
		for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
			iValue = m_DifferData[iRow][iCol];
			FTS_TEST_SAVE_INFO("%4d,  ", iValue);
		}
		FTS_TEST_SAVE_INFO("\n");
	}
	FTS_TEST_SAVE_INFO("\n");
#endif



	for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {
		for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
			if (test_data.mcap_detail_thr.invalid_node[iRow][iCol] == 0)
				continue;

			iValue = m_DifferData[iRow][iCol];
			iMin =  test_data.mcap_detail_thr.panel_differ_test_min[iRow][iCol];
			iMax = test_data.mcap_detail_thr.panel_differ_test_max[iRow][iCol];

			if (iValue < iMin || iValue > iMax) {
				btmpresult = false;
				FTS_TEST_SAVE_INFO("\nOut Of Range.  Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
								   iRow + 1, iCol + 1, iValue, iMin, iMax);
			}
		}
	}



	for (i = 0; i <  test_data.screen_param.tx_num; i++) {
		for (j = 0; j <  test_data.screen_param.rx_num; j++) {
			m_absDifferData[i][j] = abs(m_DifferData[i][j]);
			if (NODE_AST_TYPE == test_data.mcap_detail_thr.invalid_node[i][j] || NODE_INVALID_TYPE == test_data.mcap_detail_thr.invalid_node[i][j]) {
				InvalidNum++;
				continue;
			}
			maxValue = max(maxValue, m_DifferData[i][j]);
			minValue = min(minValue, m_DifferData[i][j]);
			AvgValue += m_DifferData[i][j];
		}
	}
	Save_Test_Data(m_absDifferData, "Panel Differ Test", 0, test_data.screen_param.tx_num, test_data.screen_param.rx_num, 1);


	AvgValue = AvgValue / (test_data.screen_param.tx_num * test_data.screen_param.rx_num - InvalidNum);
	FTS_TEST_SAVE_INFO("\nPanelDiffer:Max: %d, Min: %d, Avg: %d ", maxValue, minValue, AvgValue);

	ReCode = write_reg(REG_NORMALIZE_TYPE, OriginRawDataType);
	ReCode = write_reg(0x0A, OriginFrequecy);
	ReCode = write_reg(0xFB, OriginFirState);


	if (m_bV3TP) {
		ReCode = write_reg(REG_MAPPING_SWITCH, strSwitch);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nFailed to restore mapping type!");
			btmpresult = false;
		}
	}



	if (btmpresult) {
		*bTestResult = true;
		FTS_TEST_SAVE_INFO("\n\n\n/==========Panel Differ Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_SAVE_INFO("\n\n\n/==========Panel Differ Test is NG!");
	}
	return ReCode;

TEST_ERR:

	*bTestResult = false;
	FTS_TEST_SAVE_INFO("\n\n\n/==========Panel Differ Test is NG!");
	return ReCode;
}

/************************************************************************
* Name: FT5X46_TestItem_PanelDiffer_Uniformity_Test
* Brief:  TestItem: PanelDifferTest. Check if Panel Differ is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5X46_TestItem_PanelDiffer_Uniformity_Test(bool *bTestResult)
{
	int iRow = 0, iCol = 0;
	unsigned char ReCode = 0;
	bool btmpresult = true;
	int Deviation = 0;
	int Max = 0;
	int Min = 0;
	int base = 0;
	int Value = 0;
	int tmp = 0;

	FTS_TEST_SAVE_INFO("\n\r\n\r\n\r\n==============================Test Item: -------- Panel Differ Uniformity Test  \r\n\r\n");


	if (g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_CheckTx) {
		FTS_TEST_SAVE_INFO("\r\n=========PanelDiffer Uniformity Check Tx Linearity \r\n");
		for (iRow = 0; iRow < test_data.screen_param.tx_num; ++iRow) {
			for (iCol = 1; iCol <  test_data.screen_param.rx_num; ++iCol) {
				Deviation = abs(m_DifferData[iRow][iCol] - m_DifferData[iRow][iCol-1]);
				Max = max(m_DifferData[iRow][iCol], m_DifferData[iRow][iCol-1]);
				Max = Max ? Max : 1;
				TxLinearity[iRow][iCol] = 100 * Deviation / Max;
			}
		}
		/*To show value */
#if 1
		FTS_TEST_SAVE_INFO("Tx Linearity:\n");
		for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {
			FTS_TEST_SAVE_INFO("\nTx%2d:    ", iRow+1);
			for (iCol = 1; iCol < test_data.screen_param.rx_num; iCol++) {
				Value = TxLinearity[iRow][iCol];
				FTS_TEST_SAVE_INFO("%4d,  ", Value);
			}
			FTS_TEST_SAVE_INFO("\n");
		}
		FTS_TEST_SAVE_INFO("\n");
#endif

		/*To Determine  if in Range or not*/
		for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {
			for (iCol = 1; iCol < test_data.screen_param.rx_num; iCol++) {
				if (test_data.mcap_detail_thr.invalid_node[iRow][iCol] == 0)
					continue;

				Min = 0;
				Max = g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_Tx_Hole;
				base = test_data.mcap_detail_thr.PanelDiffer_Uniformity_TxLinearityTest_Max[iRow][iCol];
				tmp = TxLinearity[iRow][iCol];
				Value = abs(tmp - base);
				if (Value < Min || Value > Max) {
					btmpresult = false;
					FTS_TEST_SAVE_INFO("Tx Linearity Out Of Range.  Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
								   iRow+1, iCol+1, Value, Min, Max);
				}
			}
		}
		Save_Test_Data(TxLinearity, "Check Tx Linearity", 0, test_data.screen_param.tx_num, test_data.screen_param.rx_num, 1);
	}

	if (g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_CheckRx) {
		FTS_TEST_SAVE_INFO("\r\n=========PanelDiffer Uniformity Check Rx Linearity \r\n");
		for (iRow = 1; iRow < test_data.screen_param.tx_num; ++iRow) {
			for (iCol = 0; iCol < test_data.screen_param.rx_num; ++iCol) {
				Deviation = abs(m_DifferData[iRow][iCol] - m_DifferData[iRow-1][iCol]);
				Max = max(m_DifferData[iRow][iCol], m_DifferData[iRow-1][iCol]);
				Max = Max ? Max : 1;
				RxLinearity[iRow][iCol] = 100 * Deviation / Max;
			}
		}

		/*To show value*/
#if 1
		FTS_TEST_SAVE_INFO("Rx Linearity:\n");
		for (iRow = 1; iRow < test_data.screen_param.tx_num; iRow++) {
			FTS_TEST_SAVE_INFO("\nTx%2d:    ", iRow+1);
			for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
				Value = RxLinearity[iRow][iCol];
				FTS_TEST_SAVE_INFO("%4d,  ", Value);
			}
			FTS_TEST_SAVE_INFO("\n");
		}
		FTS_TEST_SAVE_INFO("\n");
#endif

		/*To Determine  if in Range or not*/
		for (iRow = 1; iRow < test_data.screen_param.tx_num; iRow++) {
			for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
				if (test_data.mcap_detail_thr.invalid_node[iRow][iCol] == 0)
					continue;

				Min = 0 ;
				Max = g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_Rx_Hole;
				base = test_data.mcap_detail_thr.PanelDiffer_Uniformity_RxLinearityTest_Max[iRow][iCol];
				tmp = RxLinearity[iRow][iCol];
				Value = abs(tmp - base);
				if (Value < Min || Value > Max) {
					btmpresult = false;
					FTS_TEST_SAVE_INFO("Rx Linearity Out Of Range.  Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
								iRow+1, iCol+1, Value, Min, Max);
				}
			}
		}
		Save_Test_Data(RxLinearity, "Check Rx Linearity", 0, test_data.screen_param.tx_num, test_data.screen_param.rx_num, 2);
	}

	/*Test Result*/
	if (btmpresult) {
		*bTestResult = true;
		FTS_TEST_SAVE_INFO("\n\n\n/==========Panel Differ Uniformity Test is OK!");
	} else {
		*bTestResult = false;
		FTS_TEST_SAVE_INFO("\n\n\n/==========Panel Differ Uniformity Test is NG!");
	}
	return ReCode;
}

/************************************************************************
* Name: FT5X46_TestItem_WeakShortTest
* Brief:  TestItem: WeakShortTest. Check if Tp is short or not .
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5X46_TestItem_WeakShortTest(bool *bTestResult)
{

	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	int i = 0;
	bool btmpresult = true;
	int iAllAdcDataNum = 63;
	int iMaxTx = 35;
	unsigned char tx_num, rx_num, iChannelNum;
	int iClbData_Ground, iClbData_Mutual, iOffset, iRsen, iCCRsen;
	unsigned char IcValue = 0;
	unsigned char strSwitch = 1;
	bool  bCapShortTest = false;
	int *iAdcData  = NULL;
	int fKcal = 0;
	int *fMShortResistance = NULL, *fGShortResistance = NULL;
	int iDoffset = 0, iDsen = 0, iDrefn = 0;
	int iMin_CG = 0;
	int iCount = 0;
	int iMin_CC = 0;
	int iDCal = 0;
	int iMa = 0;


	FTS_TEST_SAVE_INFO("\n\n\n\n\n==============================Test Item: -----  Weak Short-Circuit Test \r\n\r\n");

	ReCode = enter_work_mode();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n enter_work_mode failed.. Error Code: %d", ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}
	sys_delay(200);


	ReCode = read_reg(0xB1, &IcValue);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n\n Read 0xB1 IcValue error. Error Code: %d\n", ReCode);
		btmpresult = false;
		goto TEST_ERR;
	} else
		FTS_TEST_SAVE_INFO("\n IcValue:0x%02x.  \n", IcValue);

	iRsen = 57;

	iCCRsen = g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CC_Rsen;
	bCapShortTest = g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CapShortTest;
	FTS_TEST_SAVE_INFO("\n iCCRsen:%d.  \n", iCCRsen);
	FTS_TEST_SAVE_INFO("\n bCapShortTest:%d.  \n", bCapShortTest);

	ReCode = enter_factory_mode();
	sys_delay(100);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\n enter_factory_mode failed.. Error Code: %d", ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}




	if (m_bV3TP) {
		ReCode = read_reg(REG_MAPPING_SWITCH, &strSwitch);
		if (strSwitch != 1) {
			ReCode = write_reg(REG_MAPPING_SWITCH, 1);
			sys_delay(20);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\n\r\nFailed to restore mapping type!\r\n ");
				btmpresult = false;
			}
			GetChannelNumNoMapping();

			tx_num = test_data.screen_param.tx_num;
			rx_num = test_data.screen_param.rx_num;
		}
	} else {


		ReCode = read_reg(0x02, &tx_num);
		ReCode = read_reg(0x03, &rx_num);
		FTS_TEST_SAVE_INFO("\nNewly acquired TxNum:%d, RxNum:%d", tx_num, rx_num);
	}

	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nReCode  error. Error Code: %d\n", ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}
	FTS_TEST_INFO("\nNewly acquired TxNum:%d, RxNum:%d", tx_num, rx_num);
	iChannelNum = tx_num + rx_num;
	iMaxTx = tx_num;
	iAllAdcDataNum = 1 + (1 + tx_num + rx_num) * 2;
	FTS_TEST_INFO("iAllAdcDataNum:%d", iAllAdcDataNum);
	StartScan();

	for (i = 0; i < 3; i++) {
		GetRawData();
	}

	iAdcData = fts_malloc(iAllAdcDataNum * sizeof(int));
	memset(iAdcData, 0, iAllAdcDataNum);

	for (i = 0; i < 5; i++) {
		memset(iAdcData, 0, iAllAdcDataNum);
		ReCode = WeakShort_GetAdcData(iAllAdcDataNum * 2, iAdcData);
		sys_delay(50);
		if (ReCode != ERROR_CODE_OK) {
			continue;
		} else {
			if (0 == iAdcData[0] || 0 == iAdcData[1] || 0xFFFF == iAdcData[0] || 0xFFFF == iAdcData[1]) {
				continue;
			}

			else
				break;
		}
	}
	if (i >= 5) {
		FTS_TEST_SAVE_INFO("\nWeakShort_GetAdcData or ADC data error. tried times: %d", i);
		btmpresult = false;
		goto TEST_ERR;
	}


	iOffset = iAdcData[0];
	iClbData_Ground = iAdcData[1];
	iClbData_Mutual = iAdcData[2 + iChannelNum];


#if 0


	for (i = 0; i < iAllAdcDataNum/*iChannelNum*/; i++) {
		if (i <= (iChannelNum + 1)) {
			if (i == 0)
				FTS_TEST_SAVE_INFO("\n\n\n\nOffset %02d: %4d,\n", i, iAdcData[i]);
			else if (i == 1) {
				FTS_TEST_SAVE_INFO("\nGround %02d: %4d,\n", i, iAdcData[i]);
				FTS_TEST_SAVE_INFO("\n Tx:");
			} else if (i <= (iMaxTx + 1))
				FTS_TEST_SAVE_INFO("%4d,  ", iAdcData[i]);
			else  if (i <= (iChannelNum + 1))
				FTS_TEST_SAVE_INFO("%4d,  ", iAdcData[i]);

			if (i == (tx_num + 1))
				FTS_TEST_SAVE_INFO("\n Rx:");
		} else {
			if (i == (iChannelNum + 2)) {
				FTS_TEST_SAVE_INFO("\n\n\n\nMultual %02d: %4d\n", i, iAdcData[i]);
				FTS_TEST_SAVE_INFO("\n Tx:");
			} else if (i <= (iMaxTx) + (iChannelNum + 2))
				FTS_TEST_SAVE_INFO("%4d,  ", iAdcData[i]);
			else  if (i < iAllAdcDataNum)
				FTS_TEST_SAVE_INFO("%4d,  ", iAdcData[i]);

			if (i == (iChannelNum + 1 + tx_num + 1))
				FTS_TEST_SAVE_INFO("\n Rx:");
		}
	}
	FTS_TEST_SAVE_INFO("\n\r\n");

#endif



	fMShortResistance = fts_malloc(iChannelNum * sizeof(int));
	memset(fMShortResistance, 0, iChannelNum);
	fGShortResistance =  fts_malloc(iChannelNum * sizeof(int));
	memset(fGShortResistance, 0, iChannelNum);


	iMin_CG = g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CG;

	iDoffset = iOffset - 1024;
	iDrefn = iClbData_Ground;
	fKcal = 1;

	for (i = 0; i < iChannelNum; i++) {
		iDsen = iAdcData[i + 2];
		if ((2047 + iDoffset) - iDsen <= 0) {
			continue;
		}

		if (IcValue <= 0x05 || IcValue == 0xff) {

			fGShortResistance[i] = (iDsen - iDoffset + 410) * 25 * fKcal / (2047 + iDoffset - iDsen) - 3;
		} else {
			if (iDrefn - iDsen <= 0) {
				fGShortResistance[i] = iMin_CG;
				FTS_TEST_SAVE_INFO("\n%02d  ", fGShortResistance[i]);

				continue;
			}

			fGShortResistance[i] = (((iDsen - iDoffset + 384) / (iDrefn - iDsen) * 57) - 1);
		}
		if (fGShortResistance[i] < 0)
			fGShortResistance[i] = 0;



		if ((iMin_CG > fGShortResistance[i]) || (iDsen - iDoffset < 0)) {
			iCount++;
			if (i + 1 <= iMaxTx)
				FTS_TEST_SAVE_INFO("\nTx%02d: %02d (k次),	", i + 1, fGShortResistance[i]);
			else
				FTS_TEST_SAVE_INFO("\nRx%02d: %02d (k次),	", i + 1 - iMaxTx, fGShortResistance[i]);
			if (iCount % 10 == 0)
				FTS_TEST_SAVE_INFO("\n\n");
		}

	}



	if (iCount > 0) {

		btmpresult = false;
	}


	iMin_CC = g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CC;

	if ((IcValue == 0x06 || IcValue < 0xff) && iRsen != iCCRsen) {
		iRsen = iCCRsen;
	}
	iDoffset = iOffset - 1024;
	iDrefn = iClbData_Mutual;
	fKcal = 1.0;
	iCount = 0;
	iDCal = max(iDrefn, 116 + iDoffset);

	for (i = 0; i < iChannelNum; i++) {
		iDsen = iAdcData[i + iChannelNum + 3];
		if (IcValue <= 0x05 || IcValue == 0xff) {
			if (iDsen - iDrefn < 0)
				continue;
		}

		if (IcValue <= 0x05 || IcValue == 0xff) {
			iMa = iDsen - iDCal;
			iMa = iMa ? iMa : 1;
			fMShortResistance[i] = ((2047 + iDoffset - iDCal) * 24 / iMa - 27) * fKcal - 6;
		} else {
			if (iDrefn - iDsen <= 0) {
				fMShortResistance[i] = iMin_CC;
				FTS_TEST_SAVE_INFO("\n%02d  ", fMShortResistance[i]);
				continue;
			}

			fMShortResistance[i] = (iDsen - iDoffset - 123) * iRsen * fKcal / (iDrefn - iDsen /*temp*/) - 2;
		}

		if (fMShortResistance[i] < 0 && fMShortResistance[i] >= -240)
			fMShortResistance[i] = 0;
		else if (fMShortResistance[i] < -240)
			continue;

		if (fMShortResistance[i] <= 0  || fMShortResistance[i] < iMin_CC) {
			iCount++;
			if (i + 1 <= iMaxTx)
				FTS_TEST_SAVE_INFO("\nTx%02d: %02d(k次),	", i + 1, fMShortResistance[i]);
			else
				FTS_TEST_SAVE_INFO("\nRx%02d: %02d(k次),	", i + 1 - iMaxTx, fMShortResistance[i]);

			if (iCount % 10 == 0)
				FTS_TEST_SAVE_INFO("\n\n");

		}

	}

	if (iCount > 0 && !bCapShortTest) {

		btmpresult = false;
	}


	if (bCapShortTest && iCount) {
		FTS_TEST_SAVE_INFO("\n bCapShortTest && iCount.  need to add ......");
	}


	if (m_bV3TP) {
		ReCode = write_reg(REG_MAPPING_SWITCH, strSwitch);
		sys_delay(50);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nFailed to restore mapping type!\r\n ");
			btmpresult = false;
		}
		ReCode = GetChannelNum();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\n\n GetChannelNum error. Error Code: %d",  ReCode);
			goto TEST_ERR;
		}

		ReCode = GetRawData();
	}

TEST_ERR:

	if (NULL != iAdcData) {
		fts_free(iAdcData);
		iAdcData = NULL;
	}
	if (NULL != fMShortResistance) {
		fts_free(fMShortResistance);
		fMShortResistance = NULL;
	}

	if (NULL != fGShortResistance) {
		fts_free(fGShortResistance);
		fGShortResistance = NULL;
	}
	if (btmpresult) {
		FTS_TEST_SAVE_INFO("\n\r\n\r\n//Weak Short Test is OK.");
		*bTestResult = true;
	} else {
		FTS_TEST_SAVE_INFO("\n\r\n\r\n//Weak Short Test is NG.");
		*bTestResult = false;
	}


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
	return read_reg(REG_TX_NUM, pPanelRows);
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
	return read_reg(REG_RX_NUM, pPanelCols);
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
	const unsigned char MaxTimes = 250;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	ReCode = read_reg(DEVIDE_MODE_ADDR, &RegVal);
	if (ReCode == ERROR_CODE_OK) {
		RegVal |= 0x80;
		ReCode = write_reg(DEVIDE_MODE_ADDR, RegVal);
		if (ReCode == ERROR_CODE_OK) {
			while (times++ < MaxTimes) {
				sys_delay(16);
				ReCode = read_reg(DEVIDE_MODE_ADDR, &RegVal);
				if (ReCode == ERROR_CODE_OK) {
					if ((RegVal >> 7) == 0)
						break;
				} else {
					FTS_TEST_SAVE_INFO("\nStartScan read DEVIDE_MODE_ADDR error.");
					break;
				}
			}
			if (times < MaxTimes)
				ReCode = ERROR_CODE_OK;
			else {
				ReCode = ERROR_CODE_COMM_ERROR;
				FTS_TEST_SAVE_INFO("\ntimes NOT < MaxTimes. error.");
			}
		} else
			FTS_TEST_SAVE_INFO("\nStartScan write DEVIDE_MODE_ADDR error.");
	} else
		FTS_TEST_SAVE_INFO("\nStartScan read DEVIDE_MODE_ADDR error.");
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


	iReadNum = ByteNum / BYTES_PER_TIME;

	if (0 != (ByteNum % BYTES_PER_TIME))
		iReadNum++;

	if (ByteNum <= BYTES_PER_TIME) {
		BytesNumInTestMode1 = ByteNum;
	} else {
		BytesNumInTestMode1 = BYTES_PER_TIME;
	}

	ReCode = write_reg(REG_LINE_NUM, LineNum);

	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nFailed to write REG_LINE_NUM! ");
		goto READ_ERR;
	}


	I2C_wBuffer[0] = REG_RawBuf0;
	if (ReCode == ERROR_CODE_OK) {
		ReCode = fts_i2c_read_write(I2C_wBuffer, 1, m_ucTempData, BytesNumInTestMode1);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nread rawdata fts_i2c_read_write Failed!1 ");
			goto READ_ERR;
		}
	}

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK)
			break;

		if (i == iReadNum - 1) {
			ReCode = fts_i2c_read_write(NULL, 0, m_ucTempData + BYTES_PER_TIME * i, ByteNum - BYTES_PER_TIME * i);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\nread rawdata fts_i2c_read_write Failed!2 ");
				goto READ_ERR;
			}
		} else {
			ReCode = fts_i2c_read_write(NULL, 0, m_ucTempData + BYTES_PER_TIME * i, BYTES_PER_TIME);

			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\nread rawdata fts_i2c_read_write Failed!3 ");
				goto READ_ERR;
			}
		}

	}

	if (ReCode == ERROR_CODE_OK) {
		for (i = 0; i < (ByteNum >> 1); i++) {
			pRevBuffer[i] = (m_ucTempData[i << 1] << 8) + m_ucTempData[(i << 1) + 1];




		}
	}

READ_ERR:
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
	int i = 0;
	int byte_num = 0;
	int read_num = 0;
	int packet_num = 0;
	int packet_remainder = 0;

	if (index < 128) {
		*pcbValue = 0;
		write_reg(REG_ScCbAddrR, index);
		ReCode = read_reg(REG_ScCbBuf0, pcbValue);
	} else {
		byte_num = index - 128;
		packet_num = byte_num / BYTES_PER_TIME;
		packet_remainder = byte_num % BYTES_PER_TIME;
		if (packet_remainder)
			packet_num++;
		if (byte_num < BYTES_PER_TIME) {
			read_num = byte_num;
		} else {
			read_num = BYTES_PER_TIME;
		}

		for (i = 0; i < packet_num; i++) {
			if ((i == (packet_num - 1)) && packet_remainder) {
				read_num = packet_remainder;
			}
			write_reg(REG_ScCbAddrR, 0);
			wBuffer[0] = REG_ScCbBuf0;
			ReCode = fts_i2c_read_write(wBuffer, 1, pcbValue, read_num);
		}

	}

	return ReCode;
}


/************************************************************************
* Name: Save_Test_Data
* Brief:  Storage format of test data
* Input: int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount
* Output: none
* Return: none
***********************************************************************/
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], char *test_num, int iArrayIndex, unsigned char Row, unsigned char Col, unsigned char ItemCount)
{
	int iLen = 0;
	int i = 0, j = 0;


	iLen = snprintf(test_data.tmp_buffer, PAGE_SIZE, \
				   "%s, %d, %d, %d, %d, %d, ", test_num, test_data.test_item_code, Row, Col, test_data.start_line, ItemCount);
	memcpy(test_data.msg_area_line2 + test_data.len_msg_area_line2, test_data.tmp_buffer, iLen);
	test_data.len_msg_area_line2 += iLen;

	test_data.start_line += Row;
	test_data.test_data_count++;


	for (i = 0 + iArrayIndex; (i < Row + iArrayIndex)  && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < Col) && (j < RX_NUM_MAX); j++) {
			if (j == (Col - 1))
				iLen = snprintf(test_data.tmp_buffer, PAGE_SIZE, "%d, \n",  iData[i][j]);
			else
				iLen = snprintf(test_data.tmp_buffer, PAGE_SIZE, "%d, ", iData[i][j]);

			memcpy(test_data.store_data_area + test_data.len_store_data_area, test_data.tmp_buffer, iLen);
			test_data.len_store_data_area += iLen;
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
		test_data.screen_param.tx_num = rBuffer[0];
		if (test_data.screen_param.tx_num > test_data.screen_param.used_max_tx_num) {
			FTS_TEST_SAVE_INFO("\nFailed to get Tx number, Get num = %d, UsedMaxNum = %d",
							   test_data.screen_param.tx_num, test_data.screen_param.used_max_tx_num);
			test_data.screen_param.tx_num = 0;
			return ERROR_CODE_INVALID_PARAM;
		}
	} else {
		FTS_TEST_SAVE_INFO("\nFailed to get Tx number");
	}



	ReCode = GetPanelCols(rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		test_data.screen_param.rx_num = rBuffer[0];
		if (test_data.screen_param.rx_num > test_data.screen_param.used_max_rx_num) {
			FTS_TEST_SAVE_INFO("\nFailed to get Rx number, Get num = %d, UsedMaxNum = %d",
							   test_data.screen_param.rx_num, test_data.screen_param.used_max_rx_num);
			test_data.screen_param.rx_num = 0;
			return ERROR_CODE_INVALID_PARAM;
		}
	} else {
		FTS_TEST_SAVE_INFO("\nFailed to get Rx number");
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


	ReCode = enter_factory_mode();
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_SAVE_INFO("\nFailed to Enter Factory Mode...");
		return ReCode;
	}



	if (0 == (test_data.screen_param.tx_num + test_data.screen_param.rx_num)) {
		ReCode = GetChannelNum();
		if (ERROR_CODE_OK != ReCode) {
			FTS_TEST_SAVE_INFO("\nError Channel Num...");
			return ERROR_CODE_INVALID_PARAM;
		}
	}


	ReCode = StartScan();
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_SAVE_INFO("\nFailed to Scan ...");
		return ReCode;
	}


	memset(m_RawData, 0, sizeof(m_RawData));
	ReCode = ReadRawData(1, 0xAA, (test_data.screen_param.tx_num * test_data.screen_param.rx_num) * 2, m_iTempRawData);
	for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {
		for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
			m_RawData[iRow][iCol] = m_iTempRawData[iRow * test_data.screen_param.rx_num + iCol];
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

	for (iRow = 0; iRow < test_data.screen_param.tx_num; iRow++) {
		FTS_TEST_SAVE_INFO("\nTx%2d:", iRow + 1);
		for (iCol = 0; iCol < test_data.screen_param.rx_num; iCol++) {
			FTS_TEST_SAVE_INFO("%5d    ", m_RawData[iRow][iCol]);
		}
		FTS_TEST_SAVE_INFO("\n");
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


	FTS_TEST_SAVE_INFO("\nGet Tx Num...");
	ReCode = read_reg(REG_TX_NOMAPPING_NUM,  rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		test_data.screen_param.tx_num = rBuffer[0];
	} else {
		FTS_TEST_SAVE_INFO("\nFailed to get Tx number");
	}


	FTS_TEST_SAVE_INFO("\nGet Rx Num...");
	ReCode = read_reg(REG_RX_NOMAPPING_NUM,  rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		test_data.screen_param.rx_num = rBuffer[0];
	} else {
		FTS_TEST_SAVE_INFO("\nFailed to get Rx number");
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
	ReCode = read_reg(REG_PATTERN, &chPattern);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nSwitch To NoMapping Failed!");
		goto READ_ERR;
	}

	if (1 == chPattern) {
		RegData = -1;
		ReCode = read_reg(REG_MAPPING_SWITCH, &RegData);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_SAVE_INFO("\nread REG_MAPPING_SWITCH Failed!");
			goto READ_ERR;
		}

		if (1 != RegData) {
			ReCode = write_reg(REG_MAPPING_SWITCH, 1);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\nwrite REG_MAPPING_SWITCH Failed!");
				goto READ_ERR;
			}
			sys_delay(20);
			ReCode = GetChannelNumNoMapping();

			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_SAVE_INFO("\nGetChannelNumNoMapping Failed!");
				goto READ_ERR;
			}
		}
	}

READ_ERR:
	return ReCode;
}
/************************************************************************
* Name: GetTestCondition
* Brief:  Check whether Rx or TX need to test, in Waterproof ON/OFF Mode.
* Input: none
* Output: none
* Return: true: need to test; false: Not tested.
***********************************************************************/
static bool GetTestCondition(int iTestType, unsigned char ucChannelValue)
{
	bool bIsNeeded = false;
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

static unsigned char WeakShort_GetAdcData(int AllAdcDataLen, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	unsigned char pDataSend[WEAK_SHORT_ADCDATA_LEN] = { 0 };
	u8 cmd = 0;
	unsigned char Data = 0xff;
	int i = 0;
	bool bAdcOK = false;
	int read_num = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;

	FTS_TEST_FUNC_ENTER();
	if (AllAdcDataLen > WEAK_SHORT_ADCDATA_LEN) {
		FTS_TEST_SAVE_INFO("adc data len to large %d, please check tx & rx", AllAdcDataLen);
		return -EIO;
	}

	ReCode = write_reg(0x07, 0x01);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_SAVE_INFO("\nWriteReg error. \n");
		goto EndGetAdc;
	}

	sys_delay(100);

	for (i = 0; i < 100 * 5; i++) {
		sys_delay(10);
		ReCode = read_reg(0x07, &Data);
		if (ReCode == ERROR_CODE_OK) {
			if (Data == 0) {
				bAdcOK = true;
				break;
			}
		}
	}

	if (!bAdcOK) {
		FTS_TEST_SAVE_INFO("\nADC data NOT ready.  error.\n");
		ReCode = ERROR_CODE_COMM_ERROR;
		goto EndGetAdc;
	}
	sys_delay(300);
	cmd = 0xF4;

	packet_num = AllAdcDataLen / BYTES_PER_TIME;
	packet_remainder = AllAdcDataLen % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;
	if (AllAdcDataLen < BYTES_PER_TIME) {
		read_num = AllAdcDataLen;
	} else {
		read_num = BYTES_PER_TIME;
	}
	FTS_INFO("packet_num = %d, AllAdcDataLen = %d read_num = %d", packet_num, AllAdcDataLen, read_num);

	ReCode = fts_i2c_read_write(&cmd, 1, pDataSend, read_num);
	for (i = 1; i < packet_num; i++) {
		offset = read_num * i;
		if ((i == (packet_num - 1)) && packet_remainder) {
			read_num = packet_remainder;
		}
		ReCode = fts_i2c_read_write(NULL, 0, pDataSend + offset, read_num);
	}
	if (ReCode == ERROR_CODE_OK) {
		for (i = 0; i < AllAdcDataLen / 2; i++) {
			pRevBuffer[i] = (pDataSend[2 * i] << 8) + pDataSend[2 * i + 1];

		}
	} else {
		FTS_TEST_SAVE_INFO("\nfts_i2c_read_write error. error:%d. \n", ReCode);
	}
EndGetAdc:
	FTS_TEST_FUNC_EXIT();

	return ReCode;
}

void init_testitem_ft5x46(char *strIniFile)
{
	char str[512];
	FTS_TEST_FUNC_ENTER();


	GetPrivateProfileString("TestItem", "FW_VERSION_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.FW_VERSION_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "FACTORY_ID_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.FACTORY_ID_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "PROJECT_CODE_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.PROJECT_CODE_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "RAWDATA_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.RAWDATA_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "SCAP_CB_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.SCAP_CB_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "SCAP_RAWDATA_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.SCAP_RAWDATA_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "CHANNEL_NUM_TEST", "1", str, strIniFile);
	g_stCfg_FT5X46_TestItem.CHANNEL_NUM_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.NOISE_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "WEAK_SHORT_CIRCUIT_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.WEAK_SHORT_CIRCUIT_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "UNIFORMITY_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "PANEL_DIFFER_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.PANEL_DIFFER_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "PANEL_DIFFER_UNIFORMITY_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.PANEL_DIFFER_UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "SITO_RAWDATA_UNIFORMITY_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.SITO_RAWDATA_UNIFORMITY_TEST = fts_atoi(str);


	GetPrivateProfileString("TestItem", "LCD_NOISE_TEST", "0", str, strIniFile);
	g_stCfg_FT5X46_TestItem.LCD_NOISE_TEST = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();
}
void init_basicthreshold_ft5x46(char *strIniFile)
{
	char str[512];

	FTS_TEST_FUNC_ENTER();

	GetPrivateProfileString("Basic_Threshold", "FW_VER_VALUE", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.FW_VER_VALUE = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Factory_ID_Number", "255", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Factory_ID_Number = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Project_Code", " ", str, strIniFile);
	snprintf(g_stCfg_FT5X46_BasicThreshold.Project_Code, PAGE_SIZE, "%s", str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Min", "3000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_low_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Max", "15000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_Low_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Min", "3000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_high_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Max", "15000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_high_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_LowFreq", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_SetLowFreq  = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_HighFreq", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.RawDataTest_SetHighFreq = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Min", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_OFF_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Max", "240", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_OFF_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Min", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_ON_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Max", "240", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_ON_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ScapCBTest_SetWaterproof_OFF", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_SetWaterproof_OFF = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ScapCBTest_SetWaterproof_ON", "240", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_SetWaterproof_ON = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapCBTest_LetTx_Disable", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapCbTest_LetTx_Disable = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Min", "5000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_OFF_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Max", "8500", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_OFF_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Min", "5000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_ON_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Max", "8500", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_ON_Max = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_SetWaterproof_OFF", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_SetWaterproof_OFF = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_SetWaterproof_ON", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_SetWaterproof_ON = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_LetTx_Disable", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SCapRawDataTest_LetTx_Disable = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_Mapping", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bChannelTestMapping = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_NoMapping", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bChannelTestNoMapping = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_TxNum", "13", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ChannelNumTest_TxNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_RxNum", "24", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ChannelNumTest_RxNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_Tx_NP_Num", "13", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ChannelNumTest_TxNpNum = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_Rx_NP_Num", "24", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.ChannelNumTest_RxNpNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Max", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_Max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Frames", "32", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_Frames = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Time", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_Time = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "NoiseTest_SampeMode", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_SampeMode = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_NoiseMode", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_NoiseMode = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_ShowTip", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_ShowTip = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_GloveMode", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bNoiseTest_GloveMode = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_RawdataMin", "5000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_RawdataMin = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "GloveNoiseTest_Coefficient", "100", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.GloveNoiseTest_Coefficient = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "Set_Frequency", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Set_Frequency = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseThreshold_Choose", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.bNoiseThreshold_Choose = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_Threshold", "50", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_Threshold = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "NoiseTest_MinNGFrame", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.NoiseTest_MinNgFrame = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CG", "2000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CG = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CC", "2000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CC = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_Tx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_CheckTx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_Rx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_CheckRx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_MinMax", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_CheckMinMax = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Tx_Hole", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_Tx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Rx_Hole", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_Rx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_MinMax_Hole", "70", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Uniformity_MinMax_Hole = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Min", "150", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDifferTest_Min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Max", "1000", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDifferTest_Max = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Check_Tx", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_CheckTx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Tx_Hole", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_Tx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Check_Rx", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_CheckRx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Rx_Hole", "20", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_Rx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_Check_MinMax", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_CheckMinMax = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDiffer_UniformityTest_MinMax_Hole", "70", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.PanelDiffer_Uniformity_MinMax_Hole = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Check_Tx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SITO_RawdtaUniformityTest_Check_Tx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Check_Rx", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SITO_RawdtaUniformityTest_Check_Rx = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Tx_Hole", "10", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SITO_RawdtaUniformityTest_Tx_Hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SITO_RawdataUniformityTest_Rx_Hole", "10", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.SITO_RawdtaUniformityTest_Rx_Hole = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CC_Rsen", "57", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CC_Rsen = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CapShortTest", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.WeakShortTest_CapShortTest = fts_atoi(str);


	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_Max_Frame", "200", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_MaxFrame = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_Conficient", "100", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_Conficient = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_Mode", "1", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_Noise_Mode = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_MaxNgPoint", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_MaxNgPoint = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_FrameNum", "63", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_FrameNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_SetFrequency", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_SetFrequency = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_NoiseThresholdMode", "0", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_NoiseThresholdMode = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_NoiseCoefficient", "200", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_NoiseCoefficient = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "Lcd_Noise_NoiseMax", "50", str, strIniFile);
	g_stCfg_FT5X46_BasicThreshold.Lcd_Noise_NoiseMax = fts_atoi(str);
	FTS_TEST_FUNC_EXIT();
}

void init_detailthreshold_ft5x46(char *ini)
{
	FTS_TEST_FUNC_ENTER();

	OnInit_InvalidNode(ini);
	OnInit_DThreshold_RawDataTest(ini);
	OnInit_DThreshold_SCapRawDataTest(ini);
	OnInit_DThreshold_SCapCbTest(ini);

	OnInit_DThreshold_ForceTouch_SCapRawDataTest(ini);
	OnInit_DThreshold_ForceTouch_SCapCbTest(ini);

	OnInit_DThreshold_RxLinearityTest(ini);
	OnInit_DThreshold_TxLinearityTest(ini);

	OnInit_DThreshold_PanelDifferTest(ini);
	OnInit_DThreshold_PanelDiffer_Uniformity_Test(ini);

	FTS_TEST_FUNC_EXIT();
}

void set_testitem_sequence_ft5x46(void)
{

	test_data.test_num = 0;

	FTS_TEST_FUNC_ENTER();


	if (g_stCfg_FT5X46_TestItem.FACTORY_ID_TEST == 1) {

		fts_set_testitem(Code_FT5X46_FACTORY_ID_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.PROJECT_CODE_TEST == 1) {

		fts_set_testitem(Code_FT5X46_PROJECT_CODE_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.FW_VERSION_TEST == 1) {

		fts_set_testitem(Code_FT5X46_FW_VERSION_TEST);
	}


	fts_set_testitem(Code_FT5X46_ENTER_FACTORY_MODE);


	if (g_stCfg_FT5X46_TestItem.CHANNEL_NUM_TEST == 1) {

		fts_set_testitem(Code_FT5X46_CHANNEL_NUM_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.LCD_NOISE_TEST == 1) {

		fts_set_testitem(Code_FT5X46_LCD_NOISE_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.NOISE_TEST == 1) {

		fts_set_testitem(Code_FT5X46_NOISE_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.RAWDATA_TEST == 1) {

		fts_set_testitem(Code_FT5X46_RAWDATA_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.UNIFORMITY_TEST == 1) {

		fts_set_testitem(Code_FT5X46_UNIFORMITY_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.SITO_RAWDATA_UNIFORMITY_TEST == 1) {

		fts_set_testitem(Code_FT5X46_SITO_RAWDATA_UNIFORMITY_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.SCAP_CB_TEST == 1) {

		fts_set_testitem(Code_FT5X46_SCAP_CB_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.SCAP_RAWDATA_TEST == 1) {

		fts_set_testitem(Code_FT5X46_SCAP_RAWDATA_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.WEAK_SHORT_CIRCUIT_TEST == 1) {

		fts_set_testitem(Code_FT5X46_WEAK_SHORT_CIRCUIT_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.PANEL_DIFFER_TEST == 1) {

		fts_set_testitem(Code_FT5X46_PANELDIFFER_TEST);
	}


	if (g_stCfg_FT5X46_TestItem.PANEL_DIFFER_UNIFORMITY_TEST == 1) {

		fts_set_testitem(Code_FT5X46_PANELDIFFER_UNIFORMITY_TEST);
	}

	FTS_TEST_FUNC_EXIT();
}

struct test_funcs test_func_ft5x46 = {
	.ic_series = TEST_ICSERIES(IC_FT5X46),
	.init_testitem = init_testitem_ft5x46,
	.init_basicthreshold = init_basicthreshold_ft5x46,
	.init_detailthreshold = init_detailthreshold_ft5x46,
	.set_testitem_sequence  = set_testitem_sequence_ft5x46,
	.start_test = start_test_ft5x46,
};
