/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* Copyright ?2012 Synaptics Incorporated. All rights reserved. */
/*  */
/* The information in this file is confidential under the terms */
/* of a non-disclosure agreement with Synaptics and is provided */
/* AS IS. */
/*  */
/* The information in this file shall remain the exclusive property */
/* of Synaptics and may be the subject of Synaptics?patents, in */
/* whole or part. Synaptics?intellectual property rights in the */
/* information in this file are not expressly or implicitly licensed */
/* or otherwise transferred to you as a result of such information */
/* being made available to you. */
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* FullRawCapacitance Support 0D button */
/*  */
#include "include/RefCode_F54.h"
#include "include/s3320_driver.h"
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
#include "include/TestLimits_vu3.h"
#else
#include "include/TestLimits.h"
#endif
#if 0
#ifdef _DEBUG
#define DEBUG_TOUCH_INFO_MSG(format, ...) TOUCH_INFO_MSG(format, __VA_ARGS__)
#else
#define DEBUG_TOUCH_INFO_MSG(format, ...)
#endif

#ifdef INTERACTIVE_SHELL
#define EXIT(x) exit(x)
#else
void EXIT(int x)
{
	fTOUCH_INFO_MSG(stderr, "Press any key to exit.\n");
	_getch();
	exit(x);
}
#endif
#endif

/* #define TRX_max 32 */
#define TRX_mapping_max 54
#define LOWER_ABS_ADC_RANGE_LIMIT 60
#define UPPER_ABS_ADC_RANGE_LIMIT 190
#define LOWER_ABS_RAW_CAP_LIMIT 1000	/* fF */
#define UPPER_ABS_RAW_CAP_LIMIT 14000	/* fF */
#define REPORT_DATA_OFFEST 3
#define VERSION "1.0"
#define TRX_MAX 32

int NSM_TEST;

#if 0
void fatal(const char *format, ...)
{
	va_list ap;

	DEBUG_TOUCH_INFO_MSG("Error in %s on line %d\n", __FILE__, __LINE__);

	va_start(ap, format);
	vfTOUCH_INFO_MSG(stderr, format, ap);
	va_end(ap);

	EXIT(EXIT_FAILURE);
}
#endif

unsigned int count;
const unsigned short DefaultTarget = 0;
const int DefaultTimeout = 10;	/* In counts */

int pageNum = 0;
int scanMaxPageCount = 5;
/* CCdciApi cdci; */
int input;

bool bHaveF01 = false;
bool bHaveF11 = false;
bool bHaveF1A = false;
bool bHaveF12 = false;
bool bHaveF34 = false;
bool bHaveF54 = false;
bool bHaveF55 = false;
bool SignalClarityOn = false;
bool bHaveF54Ctrl07 = false;
bool bHaveF54Ctrl41 = false;
bool bHaveF54Ctrl57 = false;
bool bHavePixelTouchThresholdTuning = false;
bool bHaveInterferenceMetric = false;
bool bHaveCtrl11 = false;
bool bHaveRelaxationControl = false;
bool bHaveSensorAssignment = false;
bool bHaveSenseFrequencyControl = false;
bool bHaveFirmwareNoiseMitigation = false;
bool bHaveIIRFilter = false;
bool bHaveCmnRemoval = false;
bool bHaveCmnMaximum = false;
bool bHaveTouchHysteresis = false;
bool bHaveEdgeCompensation = false;
bool bHavePerFrequencyNoiseControl = false;
bool bHaveSignalClarity = false;
bool bHaveMultiMetricStateMachine = false;
bool bHaveVarianceMetric = false;
bool bHave0DRelaxationControl = false;
bool bHave0DAcquisitionControl = false;
bool bHaveSlewMetric = false;
bool bHaveHBlank = false;
bool bHaveVBlank = false;
bool bHaveLongHBlank = false;
bool bHaveNoiseMitigation2 = false;
bool bHaveSlewOption = false;
bool bHaveEnhancedStretch = false;
bool bHaveStartupFastRelaxation = false;
bool bHaveESDControl = false;
bool bHaveEnergyRatioRelaxation = false;
bool bHaveCtrl86 = false;
bool bHaveCtrl87 = false;
bool bHaveCtrl88 = false;
bool bHaveCtrl89 = false;
bool bHaveCtrl90 = false;
bool bHaveCtrl91 = false;
bool bHaveCtrl92 = false;
bool bHaveCtrl93 = false;
bool bHaveCtrl94 = false;
bool bHaveCtrl95 = false;
bool bHaveCtrl96 = false;
bool bHaveCtrl97 = false;
bool bHaveCtrl98 = false;
bool bHaveCtrl99 = false;
bool bHaveCtrl100 = false;
bool bHaveCtrl101 = false;
bool bHaveCtrl102 = false;
bool bHaveCtrl103 = false;
bool bHaveCtrl104 = false;
bool bHaveCtrl105 = false;
bool bHaveCtrl106 = false;
bool bHaveCtrl107 = false;
bool bHaveCtrl108 = false;
bool bHaveCtrl109 = false;
bool bHaveCtrl110 = false;
bool bHaveCtrl111 = false;
bool bHaveCtrl112 = false;
bool bHaveCtrl113 = false;
bool bHaveCtrl114 = false;
bool bHaveCtrl115 = false;
bool bHaveCtrl116 = false;
bool bHaveCtrl117 = false;
bool bHaveCtrl118 = false;
bool bHaveCtrl119 = false;
bool bHaveCtrl120 = false;
bool bHaveCtrl121 = false;
bool bHaveCtrl122 = false;
bool bHaveCtrl123 = false;
bool bHaveCtrl124 = false;
bool bHaveCtrl125 = false;
bool bHaveCtrl126 = false;
bool bHaveCtrl127 = false;
bool bHaveCtrl128 = false;
bool bHaveCtrl129 = false;
bool bHaveCtrl130 = false;
bool bHaveCtrl131 = false;
bool bHaveCtrl132 = false;
bool bHaveCtrl133 = false;
bool bHaveCtrl134 = false;
bool bHaveCtrl135 = false;
bool bHaveCtrl136 = false;
bool bHaveCtrl137 = false;
bool bHaveCtrl138 = false;
bool bHaveCtrl139 = false;
bool bHaveCtrl140 = false;
bool bHaveCtrl141 = false;
bool bHaveCtrl142 = false;
bool bHaveCtrl143 = false;
bool bHaveCtrl144 = false;
bool bHaveCtrl145 = false;
bool bHaveCtrl146 = false;
bool bHaveCtrl147 = false;
bool bHaveCtrl148 = false;
bool bHaveCtrl149 = false;

bool bHaveF54Query13 = false;
bool bHaveF54Query15 = false;
bool bHaveF54Query16 = false;
bool bHaveF54Query17 = false;
bool bHaveF54Query18 = false;
bool bHaveF54Query19 = false;
bool bHaveF54Query20 = false;
bool bHaveF54Query21 = false;
bool bHaveF54Query22 = false;
bool bHaveF54Query23 = false;
bool bHaveF54Query24 = false;
bool bHaveF54Query25 = false;
bool bHaveF54Query26 = false;
bool bHaveF54Query27 = false;
bool bHaveF54Query28 = false;
bool bHaveF54Query29 = false;
bool bHaveF54Query30 = false;
bool bHaveF54Query31 = false;
bool bHaveF54Query32 = false;
bool bHaveF54Query33 = false;
bool bHaveF54Query34 = false;
bool bHaveF54Query35 = false;
bool bHaveF54Query36 = false;
bool bHaveF54Query37 = false;
bool bHaveF54Query38 = false;

bool ButtonShared = false;

unsigned char F54DataBase;
unsigned char F54QueryBase;
unsigned char F54CommandBase;
unsigned char F54ControlBase;
unsigned char F55QueryBase;
unsigned char F55ControlBase;
unsigned char F01ControlBase;
unsigned char F01CommandBase;
unsigned char RxChannelCount;
unsigned char TxChannelCount;
unsigned char TouchControllerFamily;
unsigned char CurveCompensationMode;
unsigned char NumberOfSensingFrequencies;
unsigned char F54Ctrl07Offset;
unsigned char F54Ctrl41Offset;
unsigned char F54Ctrl57Offset;
unsigned char F54Ctrl88Offset;
unsigned char F54Ctrl89Offset;
unsigned char F54Ctrl98Offset;
unsigned char F54Ctrl102Offset;
unsigned char F54Ctrl149Offset;
unsigned char F1AControlBase;
unsigned char F12ControlBase;
unsigned char F12QueryBase;
unsigned char F12_2DTxCount;
unsigned char F12_2DRxCount;
unsigned char ButtonTx[8];
unsigned char ButtonRx[8];
unsigned char ButtonCount;
unsigned char F12Support;
unsigned char F12ControlRegisterPresence;
unsigned char mask;

/* Assuming Tx = 32 & Rx = 32 to accommodate any configuration */
short Image1[TRX_max][TRX_max];
int ImagepF[TRX_max][TRX_max];
int AbsSigned32Data[TRX_mapping_max];
unsigned char AbsADCRangeData[TRX_mapping_max];
unsigned char Data[TRX_max * TRX_max * 4];
unsigned char TRxPhysical[TRX_mapping_max];

int MaxArrayLength;

unsigned char TREX_mapped[7] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3f };
unsigned char TRX_Open[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
unsigned char TRX_Gnd[7] = { 0xff, 0xff, 0xff, 0xff, 0x3, 0xff, 0xfc };
unsigned char TRX_Short[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
int HighResistanceLowerLimit[3] = { -1000, -1000, -400 };
int HighResistanceUpperLimit[3] = { 450, 450, 200 };
unsigned int AbsShort[TRX_max * 2] = { 0 };
unsigned int AbsOpen[TRX_max * 2] = { 0 };

int AbsTxShortLimit = 0;
int AbsRxShortLimit = 0;
int AbsTxOpenLimit = 0;
int AbsRxOpenLimit = 1000;
int AbsRawRef[16] = {
	77, 11919, 14023, 15163, 16192, 18319, 19337, 21491, 22633, 24692, 26853, 27993, 30147,
	32253,
	34411, 37605
};

short NoiseDeltaMin[TRX_MAX][TRX_MAX];
short NoiseDeltaMax[TRX_MAX][TRX_MAX];
short NoiseLimitLow = -16;
short NoiseLimitHigh = 16;

enum {
	STARTTIME,
	ENDTIME,
	TIME_PROFILE_MAX
};

#define get_time_interval(a, b) (a >= b ? a-b : 1000000+a-b)
struct timeval t_interval[TIME_PROFILE_MAX];
static int outbuf;
static int out_buf;
char f54_wlog_buf[6000] = { 0 };
char wlog_buf[6000] = { 0 };

/* Function to switch beteen register pages. */
bool switchPage(int page)
{

	unsigned char values[1] = { 0 };
	unsigned char data = 0;

	pageNum = values[0] = page;

	count = 0;
	do {
		Write8BitRegisters(0xFF, values, 1);
		usleep_range(1000, 2000);
		Read8BitRegisters(0xFF, &data, 1);
		count++;
	} while ((int)data != page && (count < DefaultTimeout));
	if (count >= DefaultTimeout) {
		pr_debug("[s3528]Timeout -- Page switch fail !\n");
		return false;
	}
	return true;
}

void Reset(void)
{
	unsigned char data;

	switchPage(0x00);

	data = 0x01;
	Write8BitRegisters(F01CommandBase, &data, 1);

	usleep_range(10000, 11000);
}

/* Compare Report type #20 data against test limits */
int CompareImageReport(void)
{
	bool result = true;
	int i, j, node_crack_count = 0, rx_crack_count = 0, row_crack_count = 0;

	/* Compare 0D area */
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	if (ButtonCount > 0) {
		for (i = 0; i < ButtonCount; i++) {
			for (j = 0; j < (int)F12_2DTxCount; j++) {
				if ((LowerImageLimit[j][F12_2DRxCount + i] > 0)
				    && (UpperImageLimit[j][F12_2DRxCount + i] > 0)) {
					if ((ImagepF[j][F12_2DRxCount + i] <
					     LowerImageLimit[j][F12_2DRxCount + i])
					    || (ImagepF[j][F12_2DRxCount + i] >
						UpperImageLimit[j][F12_2DRxCount + i])) {
						TOUCH_INFO_MSG
						    ("[Touch] ButtonCheck-FAIL Tx[%d] Rx[%d]\n", j,
						     F12_2DRxCount + i);
						result = false;
						break;
					}
				}
			}
		}
	}
#else
	if (ButtonCount > 0) {
		for (i = 0; i < ButtonCount; i++) {
			if ((ImagepF[TxChannelCount - 1][F12_2DRxCount + i] <
			     LowerImageLimit[TxChannelCount - 1][F12_2DRxCount + i])
			    || (ImagepF[TxChannelCount - 1][F12_2DRxCount + i] >
				UpperImageLimit[TxChannelCount - 1][F12_2DRxCount + i])) {
				/* pr_debug("Failed: Button area: TxChannel [%d] RxChannel[%d]\n",
				   TxChannelCount-1, F12_2DRxCount + i); */
				result = false;
				break;
			}
		}

	}
#endif
	/* Compare 2D area */
	for (j = 0; j < (int)F12_2DRxCount; j++) {
		/* extern int f54_window_crack; */
		/* extern int f54_window_crack_check_mode; */

		rx_crack_count = 0;


		for (i = 0; i < (int)F12_2DTxCount; i++) {


			if ((ImagepF[i][j] < LowerImageLimit[i][j])
			    || (ImagepF[i][j] > UpperImageLimit[i][j])) {
				if (f54_window_crack_check_mode) {
					if (ImagepF[i][j] < 300) {
						rx_crack_count++;
						node_crack_count++;
					} else
						row_crack_count = 0;

					if (F12_2DTxCount <= rx_crack_count)
						row_crack_count++;

					if (2 < row_crack_count) {
						f54_window_crack = 1;
						break;
					}

					if ((int)(F12_2DTxCount * F12_2DRxCount * 20 / 100) <
					    node_crack_count) {
						result = false;
						f54_window_crack = 1;
						break;
					}

				} else {
					/* pr_debug("Failed: 2D area: Tx [%d] Rx [%d]\n",i, j); */
					/* outbuf += sprintf(f54_wlog_buf+outbuf, "FAIL, %d,%d,%d\n",
					   i, j, ImagepF[i][j]); */
					result = false;
					break;
				}
			}
		}
	}

	if (result)
		outbuf +=
		    sprintf(f54_wlog_buf + outbuf, "\nFull Raw Capacitance Image Test passed.\n\n");
	else
		outbuf +=
		    sprintf(f54_wlog_buf + outbuf, "\nFull Raw Capacitance Image Test failed.\n\n");

	return (result) ? 1 : 0;
}

/* Compare Report type #4 data against test limits */
int CompareHighResistance(int maxRxpF, int maxTxpF, int minpF)
{
	bool result = true;

	if (maxRxpF > HighResistanceUpperLimit[0] || maxRxpF < HighResistanceLowerLimit[0])
		result = false;
	if (maxTxpF > HighResistanceUpperLimit[1] || maxTxpF < HighResistanceLowerLimit[1])
		result = false;
	if (minpF > HighResistanceUpperLimit[2] || minpF < HighResistanceLowerLimit[2])
		result = false;

	if (result == false) {
		TOUCH_INFO_MSG("HighResistance Test failed.\n");
		outbuf += sprintf(f54_wlog_buf + outbuf, "HighResistance Test failed.\n\n");
	} else {
		TOUCH_INFO_MSG("HighResistance Test passed.\n");
		outbuf += sprintf(f54_wlog_buf + outbuf, "HighResistance Test passed.\n\n");
	}
	return (result) ? 1 : 0;
}


/* Compare Report type #22 data against test limits */
int CompareSensorSpeedReport(void)
{
	bool result = true;
	int i, j = 0;

	for (i = 0; i < (int)F12_2DTxCount; i++) {
		for (j = 0; j < (int)F12_2DRxCount; j++) {
			if ((ImagepF[i][j] < SensorSpeedLowerImageLimit[i][j])
			    || (ImagepF[i][j] > SensorSpeedUpperImageLimit[i][j])) {
				result = false;
				TOUCH_INFO_MSG
				    ("Failed : Tx[%d] Rx[%d] -> LOWER : %d Upper : %d  IMAGE DATA : %d\n",
				     i, j, SensorSpeedLowerImageLimit[i][j],
				     SensorSpeedUpperImageLimit[i][j], ImagepF[i][j]);
				out_buf +=
				    sprintf(wlog_buf + out_buf, "Failed : Tx[%2d] Rx[%2d] = %3d\n",
					    i, j, ImagepF[i][j]);
				break;
			}
		}
	}

	if (result) {
		TOUCH_INFO_MSG("Sensor Speed Test passed.\n");
		out_buf += sprintf(wlog_buf + out_buf, "\nSensor Speed Test passed.\n\n");
	} else {
		TOUCH_INFO_MSG("Sensor Speed Test failed.\n");
		out_buf += sprintf(wlog_buf + out_buf, "\nSensor Speed Test failed.\n\n");
	}

	return (result) ? 1 : 0;
}

/* Compare Report type #23 data against test limits */
int CompareADCReport(void)
{
	bool result = true;
	int i, j = 0;

	for (i = 0; i < (int)F12_2DTxCount; i++) {
		for (j = 0; j < (int)F12_2DRxCount; j++) {
			if ((Image1[i][j] < ADCLowerImageLimit[i][j])
			    || (Image1[i][j] > ADCUpperImageLimit[i][j])) {
				out_buf +=
				    sprintf(wlog_buf + out_buf, "Failed : Tx[%2d] Rx[%2d] = %3u\n",
					    i, j, Image1[i][j]);
				result = false;
				break;
			}
		}
	}

	if (result) {
		TOUCH_INFO_MSG("ADC Range Test passed.\n");
		out_buf += sprintf(wlog_buf + out_buf, "\nADC Range Test passed.\n\n");
	} else {
		TOUCH_INFO_MSG("ADC Range Test failed.\n");
		out_buf += sprintf(wlog_buf + out_buf, "\nADC Range Test failed.\n\n");
	}

	return (result) ? 1 : 0;
}

void CompareAbsADCRangeReport(void)
{
	bool result = true;
	int i = 0;

	for (i = 0; i < RxChannelCount + F12_2DTxCount; i++) {
		if (i == F12_2DRxCount)
			i = RxChannelCount;

		if ((AbsADCRangeData[i] < LOWER_ABS_ADC_RANGE_LIMIT)
		    || (AbsADCRangeData[i] > UPPER_ABS_ADC_RANGE_LIMIT)) {
			result = false;
			break;
		}
	}

	if (result)
		TOUCH_INFO_MSG("\nAbs Sensing ADC Range Test Passed.\n");
	else
		TOUCH_INFO_MSG("\nAbs Sensing ADC Range Test Failed.\n");

}

void CompareAbsRawReport(void)
{
	bool result = true;
	int i = 0;

	for (i = 0; i < RxChannelCount + F12_2DTxCount; i++) {
		if (i == F12_2DRxCount)
			i = RxChannelCount;

		if ((AbsSigned32Data[i] < LOWER_ABS_RAW_CAP_LIMIT)
		    || (AbsSigned32Data[i] > UPPER_ABS_RAW_CAP_LIMIT)) {
			result = false;
			break;
		}
	}

	if (result)
		TOUCH_INFO_MSG("\nAbs Sensing Raw Capacitance Test Passed.\n");
	else
		TOUCH_INFO_MSG("\nAbs Sensing Raw Capacitance Test Failed.\n");

}

int CompareAbsOpen(void)
{
	bool result = true;
	int i = 0;

	for (i = 0; i < ((int)F12_2DRxCount + (int)F12_2DTxCount); i++) {
		if (i < (int)F12_2DRxCount) {
			if (AbsOpen[i] <= AbsRxOpenLimit) {
				result = false;
				TOUCH_INFO_MSG("RX[%d] failed value:  %d\n", i, AbsOpen[i]);
			}

		} else {
			if (AbsOpen[i] <= AbsTxOpenLimit) {
				result = false;
				TOUCH_INFO_MSG("TX[%d] failed value:  %d\n", i - (int)F12_2DRxCount,
					       AbsOpen[i]);
			}
		}

	}

	TOUCH_INFO_MSG("AbsRxOpenLimit:  %d  AbsTxOpenLimit : %d\n", AbsRxOpenLimit,
		       AbsTxOpenLimit);

	if (result) {
		TOUCH_INFO_MSG("Abs Sensing Open Test Passed.\n");
		outbuf += sprintf(f54_wlog_buf + outbuf, "Abs Sensing Open Test passed.\n\n");
	} else {
		TOUCH_INFO_MSG("Abs Sensing Open Test Failed.\n");
		outbuf += sprintf(f54_wlog_buf + outbuf, "Abs Sensing Open Test failed.\n\n");
	}

	return (result) ? 1 : 0;
}

int CompareAbsShort(void)
{
	bool result = true;
	int i = 0;

	for (i = 0; i < ((int)F12_2DRxCount + (int)F12_2DTxCount); i++) {
		if (i < (int)F12_2DRxCount) {
			if (AbsShort[i] >= AbsRxShortLimit) {
				result = false;
				TOUCH_INFO_MSG("RX[%d] failed value:  %d\n", i, AbsShort[i]);
			}

		} else {
			if (AbsShort[i] >= AbsTxShortLimit) {
				result = false;
				TOUCH_INFO_MSG("TX[%d] failed value:  %d\n", i - (int)F12_2DRxCount,
					       AbsShort[i]);
			}

		}

	}

	TOUCH_INFO_MSG("AbsRxShortLimit:  %d  AbsTxShortLimit : %d\n", AbsRxShortLimit,
		       AbsTxShortLimit);

	if (result) {
		TOUCH_INFO_MSG("Abs Sensing Short Test Passed.\n");
		outbuf += sprintf(f54_wlog_buf + outbuf, "Abs Sensing Short Test passed.\n\n");
	} else {
		TOUCH_INFO_MSG("Abs Sensing Short Test Failed.\n");
		outbuf += sprintf(f54_wlog_buf + outbuf, "Abs Sensing Short Test failed.\n\n");
	}

	return (result) ? 1 : 0;

}

/* Compare Report type #24 data against test limits */
void CompareTRexOpenTestReport(int i)
{
	int index;

	for (index = 0; index < 7; index++) {
		if (Data[index] != TRX_Open[index]) {
			TOUCH_INFO_MSG("\nTRex Open Test failed.\n");
			return;
		}
	}

	TOUCH_INFO_MSG("\nTRex Open Test passed.\n");
}

/* Compare Report type #25 data against test limits */
int CompareTRexGroundTestReport(int i)
{
	int index;		/* , offset, temp; */

	for (index = 0; index < 7; index++) {
		if (Data[index] != TRX_Gnd[index])
			outbuf += sprintf(f54_wlog_buf + outbuf, "\nTRex Ground Test failed.\n\n");

	}

	outbuf += sprintf(f54_wlog_buf + outbuf, "\nTRex Ground Test passed.\n\n");
	return 1;
}

/* Compare Report type #26 data against test limits */
int CompareTRexShortTestReport(int i)
{
	int index;		/* , offset; */

	for (index = 0; index < 7; index++) {
		if (Data[index] != TRX_Short[index]) {
			outbuf +=
			    sprintf(f54_wlog_buf + outbuf, "\nTRex-TRex Short Test failed.\n\n");
			return 0;
			/* TOUCH_INFO_MSG("\nTRex Ground Test failed.\n"); */
			/* return; */
		}
	}

	outbuf += sprintf(f54_wlog_buf + outbuf, "\nTRex-TRex Short Test passed.\n\n");

	return 1;
}

/* Compare Report type #2 data against test limits */
int CompareNoiseReport(void)
{
	bool result = true;
	int i, j = 0;

	TOUCH_INFO_MSG("\n\nNoise Test Data :\n");
	out_buf += sprintf(wlog_buf + out_buf, "\nNoise Test Data :\n");
	out_buf +=
	    sprintf(wlog_buf + out_buf,
		    "===================================================================\n         :");

	for (i = 0; i < (int)RxChannelCount; i++)
		out_buf += sprintf(wlog_buf + out_buf, "%5d ", i);

	out_buf +=
	    sprintf(wlog_buf + out_buf,
		    "\n----------------------------------------------------------------\n");
	for (i = 0; i < TxChannelCount; i++) {
		pr_debug("[s3528][Touch] Tx[%2d]: ", i);
		out_buf += sprintf(wlog_buf + out_buf, "   %5d : ", i);
		for (j = 0; j < RxChannelCount; j++) {
			ImagepF[i][j] = NoiseDeltaMax[i][j] - NoiseDeltaMin[i][j];
			pr_debug("%3d,", ImagepF[i][j]);
			out_buf += sprintf(wlog_buf + out_buf, "%5d ", ImagepF[i][j]);
			/* int temp = ImagepF[i][j]; */
		}
		pr_debug("\n");
		out_buf += sprintf(wlog_buf + out_buf, "\n");
	}
	out_buf +=
	    sprintf(wlog_buf + out_buf,
		    "-------------------------------------------------------------------\n");

	/* Compare 0D area */
	/*      for (int32_t i = 1; i <= pdt.ButtonCount; i++){
	   if ((ImagepF[pdt.TxChannelCount - i][pdt._2DRxCount] < NoiseLimitLow)
	   || (ImagepF[pdt.TxChannelCount - i][pdt._2DRxCount] > NoiseLimitHigh)){
	   printf("\tFailed: Button area: TxChannel [%d] RxChannel[%d]\n",pdt.TxChannelCount-i, pdt._2DRxCount);
	   result = false;
	   }
	   }
	 */
	/* Compare 2D area */
	for (i = 0; i < F12_2DTxCount; i++) {
		for (j = 0; j < F12_2DRxCount; j++) {
			if ((ImagepF[i][j] < NoiseLimitLow) || (ImagepF[i][j] > NoiseLimitHigh)) {
				TOUCH_INFO_MSG("\tFailed: 2D area: Tx [%d] Rx [%d]\n", i, j);
				/* out_buf += sprintf(wlog_buf+out_buf, "Failed Tx [%2d] Rx [%2d] = %3d\n",
				   i, j, ImagepF[i][j]); */
				result = false;
			}
		}
	}

	if (result == false) {
		TOUCH_INFO_MSG("Noise Test failed.\n");
		out_buf += sprintf(wlog_buf + out_buf, "\nNoise Test failed.\n\n");
	} else {
		TOUCH_INFO_MSG("Noise Test passed.\n");
		out_buf += sprintf(wlog_buf + out_buf, "\nNoise Test passed.\n\n");
	}

	return (result) ? 1 : 0;
}

/* Construct data with Report Type #20 data */
int ReadImageReport(void)
{
	int ret = 0;
	int i, j, k = 0;

/* unsigned char data; */


	/* int ReadF54BitRegisters(unsigned short regAddr, unsigned char *data, int length) */

	ret = ReadF54BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], MaxArrayLength);

	if (ret < 0)
		TPD_LOG("F54 Read error");


	outbuf +=
	    sprintf(f54_wlog_buf + outbuf, "\nInfo: Tx = %d Rx = %d\n", (int)TxChannelCount,
		    (int)RxChannelCount);
	outbuf += sprintf(f54_wlog_buf + outbuf, "Image Data :\n");
	outbuf +=
	    sprintf(f54_wlog_buf + outbuf,
		    "===================================================================\n         :");

	for (i = 0; i < (int)RxChannelCount; i++)
		outbuf += sprintf(f54_wlog_buf + outbuf, "%5d ", i);

	outbuf +=
	    sprintf(f54_wlog_buf + outbuf,
		    "\n----------------------------------------------------------------\n");

	for (i = 0; i < (int)TxChannelCount; i++) {
		outbuf += sprintf(f54_wlog_buf + outbuf, "   %5d : ", i);


		for (j = 0; j < (int)RxChannelCount; j++) {

			TPD_LOG("Data[k] = %d, Data[k+1] =%d, Data[k]|Data[k+1]<<8 = %d ", Data[k],
				Data[k + 1], ((short)Data[k] | ((short)Data[k + 1] << 8)));
			Image1[i][j] = ((short)Data[k] | ((short)Data[k + 1] << 8));
			ImagepF[i][j] = Image1[i][j];
			outbuf += sprintf(f54_wlog_buf + outbuf, "%5d ", ImagepF[i][j]);
			k = k + 2;
		}


		outbuf += sprintf(f54_wlog_buf + outbuf, "\n");
	}
	outbuf +=
	    sprintf(f54_wlog_buf + outbuf,
		    "-------------------------------------------------------------------\n");

	ret = CompareImageReport();
	write_log_DS5(NULL, f54_wlog_buf);
	msleep(30);

	/* Reset Device */
	Reset();

	return ret;
}

/* Construct data with Report Type #20 data */
int GetImageReport(char *buf)
{
	int ret = 0;
	int i, j, k = 0;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], MaxArrayLength);

	*buf = 0;
	ret +=
	    sprintf(buf + ret, "\n\nInfo: Tx = %d Rx = %d\n\n", (int)TxChannelCount,
		    (int)RxChannelCount);
	ret +=
	    sprintf(buf + ret,
		    "========================================================\n         :");

	for (i = 0; i < (int)RxChannelCount; i++)
		ret += sprintf(buf + ret, "%5d ", i);

	ret += sprintf(buf + ret, "\n-----------------------------------------------------\n");

	for (i = 0; i < (int)TxChannelCount; i++) {
		ret += sprintf(buf + ret, "   %5d : ", i);
		for (j = 0; j < (int)RxChannelCount; j++) {
			Image1[i][j] = ((short)Data[k] | (short)Data[k + 1] << 8);
			ImagepF[i][j] = Image1[i][j];
			ret += sprintf(buf + ret, "%5d ", ImagepF[i][j]);
			k = k + 2;
		}
		ret += sprintf(buf + ret, "\n");
	}
	ret += sprintf(buf + ret, "--------------------------------------------------------\n");

	/* Reset Device */
	Reset();

	return ret;
}

/* Construct data with Report Type #2 data */
int ReadNoiseReport(void)
{
	int ret = 0;
	int i, j, k = 0;

	/* set FIFO index */
	unsigned char fifoIndex[2] = { 0, 0 };

	Write8BitRegisters(F54DataBase + 1, fifoIndex, sizeof(fifoIndex));

	ReadF54BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], MaxArrayLength);

	for (i = 0; i < (int)TxChannelCount; i++) {
		for (j = 0; j < (int)RxChannelCount; j++) {
			Image1[i][j] = (short)Data[k] | ((short)Data[k + 1] << 8);
			ImagepF[i][j] = Image1[i][j];

			if (ImagepF[i][j] < NoiseDeltaMin[i][j])
				NoiseDeltaMin[i][j] = ImagepF[i][j];

			if (ImagepF[i][j] > NoiseDeltaMax[i][j])
				NoiseDeltaMax[i][j] = ImagepF[i][j];


			k = k + 2;
		}
	}
	ret = CompareNoiseReport();
	write_log_DS5(NULL, wlog_buf);
	msleep(30);

	Reset();

	return ret;
}

/* Construct data with Report Type #4 data */
int ReadHighResistanceReport(void)
{
	short maxRx, maxTx, min;
	int maxRxpF, maxTxpF, minpF;
	int ret = 0;
	int i = 0;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), Data, 6);

	maxRx = ((short)Data[0] | (short)Data[1] << 8);
	maxTx = ((short)Data[2] | (short)Data[3] << 8);
	min = ((short)Data[4] | (short)Data[5] << 8);

	maxRxpF = maxRx;
	maxTxpF = maxTx;
	minpF = min;

	outbuf += sprintf(f54_wlog_buf + outbuf, "Max Rx Offset(pF) = %d\n", maxRxpF);
	outbuf += sprintf(f54_wlog_buf + outbuf, "Max Tx Offset(pF) = %d\n", maxTxpF);
	outbuf += sprintf(f54_wlog_buf + outbuf, "Min(pF) = %d\n", minpF);

	outbuf +=
	    sprintf(f54_wlog_buf + outbuf,
		    "\n=====================================================\n");
	outbuf += sprintf(f54_wlog_buf + outbuf, "\tHigh Resistance Test\n");
	outbuf +=
	    sprintf(f54_wlog_buf + outbuf,
		    "=====================================================\n");
	outbuf += sprintf(f54_wlog_buf + outbuf, " Parameters: ");
	outbuf += sprintf(f54_wlog_buf + outbuf, "%5d %5d %5d ", maxRxpF, maxTxpF, minpF);
	outbuf += sprintf(f54_wlog_buf + outbuf, "\n\n Limits(+) : ");
	for (i = 0; i < 3; i++)
		outbuf += sprintf(f54_wlog_buf + outbuf, "%5d ", HighResistanceUpperLimit[i]);

	outbuf += sprintf(f54_wlog_buf + outbuf, "\n Limits(-) : ");
	for (i = 0; i < 3; i++)
		outbuf += sprintf(f54_wlog_buf + outbuf, "%5d ", HighResistanceLowerLimit[i]);

	outbuf +=
	    sprintf(f54_wlog_buf + outbuf,
		    "\n-----------------------------------------------------\n");

	ret = CompareHighResistance(maxRxpF, maxTxpF, minpF);
	write_log_DS5(NULL, f54_wlog_buf);
	msleep(30);

	/* Reset Device */
	Reset();

	return ret;
}

/* Construct data with Report Type #13 data */
void ReadMaxMinReport(void)
{
	short max, min;
	int maxpF, minpF;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), Data, 4);

	max = ((short)Data[0] | (short)Data[1] << 8);
	min = ((short)Data[2] | (short)Data[3] << 8);
	maxpF = max;
	minpF = min;

	TOUCH_INFO_MSG("\nRaw Capacitance Maximum and Minimum Test:\n");
	/*TOUCH_INFO_MSG("Max = 0x%x\n", max);
	   TOUCH_INFO_MSG("Min = 0x%x\n", min); */
	TOUCH_INFO_MSG("Max(pF) = %d\n", maxpF);
	TOUCH_INFO_MSG("Min(pF) = %d\n", minpF);

	/* Reset Device */
	Reset();
}

/* Construct data with Report Type #23 data */
int ReadADCRangeReport(void)
{
	int temp = TxChannelCount;
	int ret = 0;
	int i, j, k = 0;

	if (SignalClarityOn) {
		if ((TxChannelCount / 4) != 0) {
			temp = (4 - (TxChannelCount % 4)) + TxChannelCount;
			Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), Data,
					  (temp * RxChannelCount * 2));
		}
	} else {
		Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], MaxArrayLength);
	}

	k = 0;

	TOUCH_INFO_MSG("ADC Range Data:\n");
	/* outbuf += sprintf(f54_wlog_buf+outbuf, "ADC Range Test Data :\n"); */
	for (i = 0; i < (int)TxChannelCount; i++) {
		/* pr_debug("[Touch] Tx[%2d]: ", i); */
		/* outbuf += sprintf(f54_wlog_buf+outbuf, "   %5d : ", i); */
		for (j = 0; j < (int)RxChannelCount; j++) {
			/* Image1[i][j] = ((unsigned short)Data[k]  | (unsigned short)Data[k+1] << 8); */
			Image1[i][j] = ((unsigned short)Data[k]);
			/* ImagepF[i][j] = Image1[i][j]/1000.0; */
			/* pr_debug("%3u,", Image1[i][j]); */
			/* outbuf += sprintf(f54_wlog_buf+outbuf, "%5u ", Image1[i][j]); */
			k = k + 2;
		}
		/* pr_debug("\n"); */
		/* outbuf += sprintf(f54_wlog_buf+outbuf, "\n"); */
	}

	ret = CompareADCReport();
	write_log_DS5(NULL, wlog_buf);
	msleep(20);

	/* Reset Device */
	Reset();

	return ret;
}

void ReadAbsADCRangeReport(void)
{
	int i, k = 0;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0],
			  2 * (RxChannelCount + TxChannelCount));

	TOUCH_INFO_MSG("Abs Sensing ADC Range Data:\n");
	TOUCH_INFO_MSG("Rx: ");
	for (i = 0; i < (int)RxChannelCount; i++) {
		AbsADCRangeData[k / 2] = (unsigned char)Data[k];
		TOUCH_INFO_MSG("%d ", AbsADCRangeData[k / 2]);
		k = k + 2;
	}
	TOUCH_INFO_MSG("\n");
	TOUCH_INFO_MSG("Tx: ");
	for (i = 0; i < (int)TxChannelCount; i++) {
		AbsADCRangeData[k / 2] = (unsigned char)Data[k];
		TOUCH_INFO_MSG("%d ", AbsADCRangeData[k / 2]);
		k = k + 2;
	}
	TOUCH_INFO_MSG("\n");

	CompareAbsADCRangeReport();

	Reset();
}

void ReadAbsDeltaReport(void)
{
	int i, k = 0;
	int *p32data;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0],
			  4 * (RxChannelCount + TxChannelCount));

	p32data = (int *)&Data[0];

	outbuf += sprintf(f54_wlog_buf + outbuf, "Abs Sensing Delta Capacitance Data:\n");
	outbuf += sprintf(f54_wlog_buf + outbuf, "Rx: ");
	for (i = 0; i < (int)RxChannelCount; i++) {
		AbsSigned32Data[k] = (int)*p32data;
		outbuf += sprintf(f54_wlog_buf + outbuf, "%d ", AbsSigned32Data[k]);
		k++;
		p32data++;
	}

	outbuf += sprintf(f54_wlog_buf + outbuf, "\nTx: ");
	for (i = 0; i < (int)TxChannelCount; i++) {
		AbsSigned32Data[k] = (int)*p32data;
		outbuf += sprintf(f54_wlog_buf + outbuf, "%d ", AbsSigned32Data[k]);
		k++;
		p32data++;
	}

	outbuf += sprintf(f54_wlog_buf + outbuf, "\n");
	Reset();
}

void ReadAbsRawReport(void)
{
	int i, k = 0;
	int *p32data;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0],
			  4 * (RxChannelCount + TxChannelCount));

	p32data = (int *)&Data[0];

	TOUCH_INFO_MSG("Abs Sensing Raw Capacitance Data:\n");
	outbuf += sprintf(f54_wlog_buf + outbuf, "Abs Sensing Raw Capacitance Data:\n");
	TOUCH_INFO_MSG("Rx: ");
	outbuf += sprintf(f54_wlog_buf + outbuf, "Rx: ");
	for (i = 0; i < (int)RxChannelCount; i++) {
		AbsSigned32Data[k] = (int)*p32data;
		TOUCH_INFO_MSG("%d ", AbsSigned32Data[k]);
		outbuf += sprintf(f54_wlog_buf + outbuf, "%d ", AbsSigned32Data[k]);
		k++;
		p32data++;
	}
	TOUCH_INFO_MSG("\n");
	TOUCH_INFO_MSG("Tx: ");
	outbuf += sprintf(f54_wlog_buf + outbuf, "\nTx: ");
	for (i = 0; i < (int)TxChannelCount; i++) {
		AbsSigned32Data[k] = (int)*p32data;
		TOUCH_INFO_MSG("%d ", AbsSigned32Data[k]);
		outbuf += sprintf(f54_wlog_buf + outbuf, "%d ", AbsSigned32Data[k]);
		k++;
		p32data++;
	}
	/* TOUCH_INFO_MSG("\n"); */
	outbuf += sprintf(f54_wlog_buf + outbuf, "\n");

	CompareAbsRawReport();

	Reset();
}

/* Construct data with Report Type #38 data */
int ReadAbsRawOpen(void)
{
	int i = 0;
	int ret = 0;
	unsigned char k = 0;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0],
			  (F12_2DRxCount + F12_2DTxCount) * 4);

	/* TOUCH_INFO_MSG("Abs Sensing Open Test Data:\n"); */
	outbuf += sprintf(f54_wlog_buf + outbuf, "Abs Sensing Open Test Data:\n");

	for (i = 0; i < ((int)F12_2DRxCount + (int)F12_2DTxCount); i++) {

		AbsOpen[i] = (unsigned int)Data[k] | ((unsigned int)Data[k + 1] << 8) |
		    ((unsigned int)Data[k + 2] << 16) | ((unsigned int)Data[k + 3] << 24);

		k += 4;

		if (i < (int)F12_2DRxCount) {
			/* TOUCH_INFO_MSG("RX[%d]: %d, ", i, AbsOpen[i]); */
			outbuf += sprintf(f54_wlog_buf + outbuf, "%5d ", AbsOpen[i]);
		} else {
			/* TOUCH_INFO_MSG("TX[%d]: %d, ", i - (int)F12_2DRxCount, AbsOpen[i]); */
			outbuf += sprintf(f54_wlog_buf + outbuf, "%5d ", AbsOpen[i]);
		}

		if (i == ((int)F12_2DRxCount - 1)) {
			/* TOUCH_INFO_MSG("\n"); */
			outbuf += sprintf(f54_wlog_buf + outbuf, "\n");
		}
	}
	/* TOUCH_INFO_MSG("\n"); */

	ret = CompareAbsOpen();

	return ret;
}

/* Construct data with Report Type #38 data */
int ReadAbsRawShort(void)
{
	int i = 0;
	int ret = 0;
	unsigned char k = 0;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0],
			  (F12_2DRxCount + F12_2DTxCount) * 4);

	/* TOUCH_INFO_MSG("Abs Sensing Short Test Data:\n"); */
	outbuf += sprintf(f54_wlog_buf + outbuf, "Abs Sensing Short Test Data:\n");

	for (i = 0; i < ((int)F12_2DRxCount + (int)F12_2DTxCount); i++) {

		AbsShort[i] = (unsigned int)Data[k] | ((unsigned int)Data[k + 1] << 8) |
		    ((unsigned int)Data[k + 2] << 16) | ((unsigned int)Data[k + 3] << 24);

		k += 4;

		if (i < (int)F12_2DRxCount) {
			/* TOUCH_INFO_MSG("RX[%d]: %d, ", i, AbsShort[i]); */
			outbuf += sprintf(f54_wlog_buf + outbuf, "%5d ", AbsShort[i]);
		} else {
			/* TOUCH_INFO_MSG("TX[%d]: %d, ", i - (int)F12_2DRxCount, AbsShort[i]); */
			outbuf += sprintf(f54_wlog_buf + outbuf, "%5d ", AbsShort[i]);
		}

		if (i == ((int)F12_2DRxCount - 1)) {
			/* TOUCH_INFO_MSG("\n"); */
			outbuf += sprintf(f54_wlog_buf + outbuf, "\n");
		}
	}
	/* TOUCH_INFO_MSG("\n"); */

	ret = CompareAbsShort();

	return ret;
}

/* Construct data with Report Type #22 data */
int ReadSensorSpeedReport(void)
{
	int i, j, k = 0;
	int ret = 0;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], MaxArrayLength);

	/* TOUCH_INFO_MSG("Sensor speed Test Data :\n"); */
	/* outbuf += sprintf(f54_wlog_buf+outbuf, "Sensor speed Test Data:\n"); */

	for (i = 0; i < (int)TxChannelCount; i++) {
		/* pr_debug("[Touch] Tx[%2d]: ", i); */
		/* outbuf += sprintf(f54_wlog_buf+outbuf, "Tx[%d]: ", i); */
		for (j = 0; j < (int)RxChannelCount; j++) {
			Image1[i][j] = ((short)Data[k] | (short)Data[k + 1] << 8);
			ImagepF[i][j] = Image1[i][j];
			/* pr_debug("%3d,", ImagepF[i][j]); */
			/* outbuf += sprintf(f54_wlog_buf+outbuf, "%d  ", ImagepF[i][j]); */
			k = k + 2;
		}
		/* pr_debug("\n"); */
		/* outbuf += sprintf(f54_wlog_buf+outbuf, "\n"); */
	}

	ret = CompareSensorSpeedReport();
	write_log_DS5(NULL, wlog_buf);
	msleep(20);

	/* Reset Device */
	Reset();

	return ret;
}

int pow_func(int x, int y)
{
	int result = 1;
	int i = 0;

	for (i = 0; i < y; i++)
		result *= x;
	return result;
}

/* Construct data with Report Type #24 data */
void ReadTRexOpenReport(void)
{
	int i, j = 0;
	/* int k = ((int)TxChannelCount) / 8 + 1; */
	int k = 7, mask = 0x01, value;	/* Hardcode for Waikiki Test and it support up to 54 Tx */

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), Data, k);

	for (i = 0; i < k; i++) {
		value = Data[i];
		Data[i] = 0;
		for (j = 0; j < 8; j++) {
			if ((value & mask) == 1)
				Data[i] = Data[i] + (unsigned char)pow_func(2, (7 - j));

			value >>= 1;
		}
		/* TOUCH_INFO_MSG("TRex-Open Test Data = %#x,", Data[i]); */
	}
	/* TOUCH_INFO_MSG("\n"); */

	CompareTRexOpenTestReport(k * 8);

	/* Reset Device */
	Reset();
}

/* Construct data with Report Type #25 data */
int ReadTRexGroundReport(void)
{
	int ret = 0;
	int i, j = 0;
	/* int k = ((int)TxChannelCount) / 8 + 1; */
	int k = 7, mask = 0x01, value;	/* Hardcode for Waikiki Test and it support up to 54 Tx */

	ReadF54BitRegisters((F54DataBase + REPORT_DATA_OFFEST), Data, k);

	for (i = 0; i < k; i++) {
		value = Data[i];
		Data[i] = 0;
		for (j = 0; j < 8; j++) {
			if ((value & mask) == 1)
				Data[i] = Data[i] + (unsigned char)pow_func(2, (7 - j));

			value >>= 1;
		}

		outbuf += sprintf(f54_wlog_buf + outbuf, "TRex-Ground Test Data = %#x\n", Data[i]);
	}
	outbuf += sprintf(f54_wlog_buf + outbuf, "\n");

	ret = CompareTRexGroundTestReport(k * 8);
	write_log_DS5(NULL, f54_wlog_buf);
	msleep(30);
	/* Reset Device */
	Reset();

	return ret;
}

/* Construct data with Report Type #26 data */
int ReadTRexShortReport(void)
{
	int ret = 0;
	int i, j = 0;
	/* int k = ((int)TxChannelCount) / 8 + 1; */
	int k = 7, mask = 0x01, value;	/* Hardcode for Waikiki Test and it support up to 54 Tx */
	char buf[40] = { 0 };

	ReadF54BitRegisters((F54DataBase + REPORT_DATA_OFFEST), Data, k);

	for (i = 0; i < k; i++) {
		value = Data[i];
		Data[i] = 0;
		ret = 0;
		for (j = 0; j < 8; j++) {
			if ((value & mask) == 1) {
				Data[i] = Data[i] + (unsigned char)pow_func(2, (7 - j));
				ret += sprintf(buf + ret, "%d ", (i * 8 + (7 - j)));
			}
			value >>= 1;
		}
		outbuf +=
		    sprintf(f54_wlog_buf + outbuf, "TRex-TRex Short Test Data = %#x", Data[i]);

		if (ret) {
			outbuf += sprintf(f54_wlog_buf + outbuf, " (Short TRx Number: ");
			outbuf += sprintf(f54_wlog_buf + outbuf, buf);
			outbuf += sprintf(f54_wlog_buf + outbuf, ")");
		}
		outbuf += sprintf(f54_wlog_buf + outbuf, "\n");
	}

	outbuf += sprintf(f54_wlog_buf + outbuf, "\n");

	ret = CompareTRexShortTestReport(k * 8);
	write_log_DS5(NULL, f54_wlog_buf);
	msleep(30);

	/* Reset Device */
	Reset();

	return ret;
}

/* Function to handle report reads based on user input */
int ReadReport(unsigned char input, char *buf)
{
	int ret = 0;
	unsigned char data;

	/* Set the GetReport bit to run the AutoScan */
	data = 0x01;
	Write8BitRegisters(F54CommandBase, &data, 1);

	count = 0;
	do {
		Read8BitRegisters(F54CommandBase, &data, 1);
		usleep_range(1000, 2000);
		count++;
	} while (data != 0x00 && (count < DefaultTimeout));
	if (count >= DefaultTimeout) {
		outbuf +=
		    sprintf(f54_wlog_buf + outbuf, "Timeout -- Not supported Report Type in FW\n");
		Reset();
		return ret;
	}

	do_gettimeofday(&t_interval[ENDTIME]);

	TOUCH_INFO_MSG("Takes %lu ticks\n",
		       get_time_interval(t_interval[ENDTIME].tv_sec, t_interval[STARTTIME].tv_sec));

	switch (input) {
	case 'a':
		ret = ReadImageReport();
		break;
	case 'b':
		ret = ReadADCRangeReport();
		break;
	case 'c':
		ret = ReadSensorSpeedReport();
		break;
	case 'd':
		/* fTOUCH_INFO_MSG(stderr, "Press any key to continue after you have lowered the bar.\n"); */
		/* _getch(); */
		ReadTRexOpenReport();
		break;
	case 'e':
		ret = ReadTRexGroundReport();
		break;
	case 'f':
		ret = ReadTRexShortReport();
		break;
	case 'g':
		ret = ReadHighResistanceReport();
		break;
	case 'h':
		ReadMaxMinReport();
		break;
	case 'i':
		ReadAbsADCRangeReport();
		break;
	case 'j':
		ReadAbsDeltaReport();
		break;
	case 'k':
		ReadAbsRawReport();
		break;
	case 'l':
		ret = GetImageReport(buf);
		break;
	case 'm':
		ret = ReadNoiseReport();
		break;
	case 'n':
		ret = ReadAbsRawShort();
		break;
	case 'o':
		ret = ReadAbsRawOpen();
		break;
	default:
		break;
	}

	return ret;
}

#if 0
/* Power up the sensor, configure the Control Bridge protocol converter to use the appropriate protocol. */
EError PowerOnSensor(void)
{
	EError nRet;

	nRet = cdci.Connect();
	nRet =
	    cdci.PowerOn((unsigned short)0, SENSOR_VDD_SUPPLY_MV, SENSOR_SUPPLY_MV,
			 SENSOR_SUPPLY_MV, DefaultTimeout);
	if (nRet == 0) {
		nRet =
		    cdci.ConfigI2CReg((unsigned short)0, ERmiAddressEightBit, EPullupsYes,
				      ELowSpeed, EAttnLow, DefaultTimeout);
	}
	return nRet;
}

/* Remove power from the sensor. */
EError PowerOffSensor(void)
{
	EError nRet;

	nRet = cdci.PowerOff(DefaultTarget, DefaultTimeout);
	cdci.Disconnect();
	return nRet;
}
#endif

/* Examples of reading query registers. Real applications often do not need to read query registers at all. */
void RunQueries(void)
{

	unsigned short cAddr = 0xEE;
	unsigned char cFunc = 0;
	int rxCount = 0;
	int txCount = 0;
	int offset = 0;
	int query_offset = 0;
	int i, j = 0;
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	int k = 0;
	int cnt = 0;
#endif

	/* Scan Page Description Table (PDT) to find all RMI functions presented by this device. */
	/* The Table starts at $00EE. This and every sixth register (decrementing) is a function number */
	/* except when this "function number" is $00, meaning end of PDT. */
	/* In an actual use case this scan might be done only once on first run or before compile. */
	do {
		Read8BitRegisters(cAddr, &cFunc, 1);
		if (cFunc == 0)
			break;

		pr_debug("[s3528]F0x%x\n", cFunc);
		switch (cFunc) {
		case 0x01:
			if (!bHaveF01) {
				Read8BitRegisters((cAddr - 3), &F01ControlBase, 1);
				Read8BitRegisters((cAddr - 4), &F01CommandBase, 1);
				break;
			}
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
		case 0x1a:
			if (!bHaveF1A) {
				k = 0;
				Read8BitRegisters((cAddr - 3), &F1AControlBase, 1);
				Read8BitRegisters(F1AControlBase + 1, &ButtonCount, 1);
				/* ButtonCount = log((double)ButtonCount+1 )/ log(2.0); */
				while (ButtonCount) {
					cnt++;
					ButtonCount = (ButtonCount >> 1);
				}
				ButtonCount = cnt;
				for (i = 0; i < ButtonCount; i++) {
					Read8BitRegisters(F1AControlBase + 3 + k, &ButtonTx[i], 1);
					Read8BitRegisters(F1AControlBase + 3 + k + 1, &ButtonRx[i],
							  1);
					k = k + 2;
				}
				TOUCH_INFO_MSG("Have 1A:%d,%d\n", ButtonTx[1], ButtonRx[1]);
				bHaveF1A = true;
			}
			break;
#endif
		case 0x12:
			if (!bHaveF12) {
				Read8BitRegisters((cAddr - 3), &F12ControlBase, 1);
				Read8BitRegisters((cAddr - 5), &F12QueryBase, 1);
				Read8BitRegisters((F12QueryBase), &F12Support, 1);

				if ((F12Support | 0x00) == 0) {
					/* TOUCH_INFO_MSG("Device not support F12.\n"); */
					break;
				}
				Read8BitRegisters((F12QueryBase + 5), Data, 2);
				mask = 0x01;
				for (j = 0; j < 8; j++) {
					if ((Data[1] & mask) == 1)
						offset++;
					Data[1] >>= 1;
				}
				Read8BitRegisters((F12ControlBase + offset), Data, 14);
				F12_2DRxCount = Data[12];
				F12_2DTxCount = Data[13];

				if (TRX_max <= F12_2DRxCount)
					F12_2DRxCount = TRX_max;
				if (TRX_max <= F12_2DTxCount)
					F12_2DTxCount = 16;

				offset = 0;
				break;
			}
		case 0x54:
			if (!bHaveF54) {
				Read8BitRegisters((cAddr - 2), &F54DataBase, 1);

				Read8BitRegisters((cAddr - 3), &F54ControlBase, 1);
				Read8BitRegisters((cAddr - 4), &F54CommandBase, 1);
				Read8BitRegisters((cAddr - 5), &F54QueryBase, 1);

				Read8BitRegisters(F54QueryBase, &RxChannelCount, 1);
				Read8BitRegisters((F54QueryBase + 1), &TxChannelCount, 1);

				if (TRX_max <= RxChannelCount)
					RxChannelCount = TRX_max;
				if (TRX_max <= TxChannelCount)
					TxChannelCount = 16;

				MaxArrayLength = (int)RxChannelCount * (int)TxChannelCount * 2;

				Read8BitRegisters(F54QueryBase, Data, 60);

				TouchControllerFamily = Data[5];
				offset++;	/* Ctrl 00 */


				if (TouchControllerFamily == 0x0 || TouchControllerFamily == 0x01)
					offset++;	/* Ctrl 01 */
				offset += 2;	/* Ctrl 02 */
				bHavePixelTouchThresholdTuning = ((Data[6] & 0x01) == 0x01);
				if (bHavePixelTouchThresholdTuning)
					offset++;	/* Ctrl 03; */
				if (TouchControllerFamily == 0x0 || TouchControllerFamily == 0x01)
					offset += 3;	/* Ctrl 04/05/06 */
				if (TouchControllerFamily == 0x01) {
					F54Ctrl07Offset = offset;
					offset++;	/* Ctrl 07; */
					bHaveF54Ctrl07 = true;
				}
				if (TouchControllerFamily == 0x0 || TouchControllerFamily == 0x01)
					offset += 2;	/* Ctrl 08 */
				if (TouchControllerFamily == 0x0 || TouchControllerFamily == 0x01)
					offset++;	/* Ctrl 09 */
				bHaveInterferenceMetric = ((Data[7] & 0x02) == 0x02);
				if (bHaveInterferenceMetric)
					offset++;	/* Ctrl 10 */
				bHaveCtrl11 = ((Data[7] & 0x10) == 0x10);
				if (bHaveCtrl11)
					offset += 2;	/* Ctrl 11 */
				bHaveRelaxationControl = ((Data[7] & 0x80) == 0x80);
				if (bHaveRelaxationControl)
					offset += 2;	/* Ctrl 12/13 */
				bHaveSensorAssignment = ((Data[7] & 0x01) == 0x01);
				if (bHaveSensorAssignment)
					offset++;	/* Ctrl 14 */
				if (bHaveSensorAssignment)
					offset += RxChannelCount;	/* Ctrl 15 */
				if (bHaveSensorAssignment)
					offset += TxChannelCount;	/* Ctrl 16 */
				bHaveSenseFrequencyControl = ((Data[7] & 0x04) == 0x04);

				if (bHaveSenseFrequencyControl)
					NumberOfSensingFrequencies = (Data[13] & 0x0F);

				if (bHaveSenseFrequencyControl)
					offset += (3 * (int)NumberOfSensingFrequencies);	/* Ctrl 17/18/19 */
				offset++;	/* Ctrl 20 */
				if (bHaveSenseFrequencyControl)
					offset += 2;	/* Ctrl 21 */
				bHaveFirmwareNoiseMitigation = ((Data[7] & 0x08) == 0x08);
				if (bHaveFirmwareNoiseMitigation)
					offset++;	/* Ctrl 22 */
				if (bHaveFirmwareNoiseMitigation)
					offset += 2;	/* Ctrl 23 */
				if (bHaveFirmwareNoiseMitigation)
					offset += 2;	/* Ctrl 24 */
				if (bHaveFirmwareNoiseMitigation)
					offset++;	/* Ctrl 25 */
				if (bHaveFirmwareNoiseMitigation)
					offset++;	/* Ctrl 26 */
				bHaveIIRFilter = ((Data[9] & 0x02) == 0x02);
				if (bHaveIIRFilter)
					offset++;	/* Ctrl 27 */
				if (bHaveFirmwareNoiseMitigation)
					offset += 2;	/* Ctrl 28 */
				bHaveCmnRemoval = ((Data[9] & 0x04) == 0x04);
				bHaveCmnMaximum = ((Data[9] & 0x08) == 0x08);
				if (bHaveCmnRemoval)
					offset++;	/* Ctrl 29 */
				if (bHaveCmnMaximum)
					offset++;	/* Ctrl 30 */
				bHaveTouchHysteresis = ((Data[9] & 0x10) == 0x10);
				if (bHaveTouchHysteresis)
					offset++;	/* Ctrl 31 */
				bHaveEdgeCompensation = ((Data[9] & 0x20) == 0x20);
				if (bHaveEdgeCompensation)
					offset += 2;	/* Ctrl 32 */
				if (bHaveEdgeCompensation)
					offset += 2;	/* Ctrl 33 */
				if (bHaveEdgeCompensation)
					offset += 2;	/* Ctrl 34 */

				if (bHaveEdgeCompensation)
					offset += 2;	/* Ctrl 35 */
				CurveCompensationMode = (Data[8] & 0x03);

				if (CurveCompensationMode == 0x02) {
					offset += (int)RxChannelCount;
				} else if (CurveCompensationMode == 0x01) {
					offset +=
					    ((int)RxChannelCount >
					     (int)TxChannelCount) ? (int)RxChannelCount : (int)
					    TxChannelCount;
				}
				/* Ctrl 36 */
				if (CurveCompensationMode == 0x02)
					offset += (int)TxChannelCount;	/* Ctrl 37 */
				bHavePerFrequencyNoiseControl = ((Data[9] & 0x40) == 0x40);
				if (bHavePerFrequencyNoiseControl)
					offset += (3 * (int)NumberOfSensingFrequencies);	/* Ctrl 38/39/40 */
				bHaveSignalClarity = ((Data[10] & 0x04) == 0x04);
				if (bHaveSignalClarity) {


					F54Ctrl41Offset = offset;	/* Ctrl 41 */
					offset++;

					SignalClarityOn = true;

				} else
					SignalClarityOn = false;

				bHaveMultiMetricStateMachine = ((Data[10] & 0x02) == 0x02);
				bHaveVarianceMetric = ((Data[10] & 0x08) == 0x08);
				if (bHaveVarianceMetric)
					offset += 2;	/* Ctr 42 */
				if (bHaveMultiMetricStateMachine)
					offset += 2;	/* Ctrl 43 */
				if (bHaveMultiMetricStateMachine)
					offset += 11;	/* Ctrl 44/45/46/47/48/49/50/51/52/53/54 */
				bHave0DRelaxationControl = ((Data[10] & 0x10) == 0x10);
				bHave0DAcquisitionControl = ((Data[10] & 0x20) == 0x20);

				if (bHave0DRelaxationControl)
					offset += 2;	/* Ctrl 55/56 */

				if (bHave0DAcquisitionControl)

					TPD_LOG("bHaveSignalClarity"); {
					F54Ctrl57Offset = offset;
					offset++;	/* Ctrl 57; */
					bHaveF54Ctrl57 = true;
					}
					if (bHave0DAcquisitionControl)
						offset += 1;	/* Ctrl 58 */
				bHaveSlewMetric = ((Data[10] & 0x80) == 0x80);
				bHaveHBlank = ((Data[11] & 0x01) == 0x01);
				bHaveVBlank = ((Data[11] & 0x02) == 0x02);
				bHaveLongHBlank = ((Data[11] & 0x04) == 0x04);
				bHaveNoiseMitigation2 = ((Data[11] & 0x20) == 0x20);
				bHaveSlewOption = ((Data[12] & 0x02) == 0x02);
				if (bHaveHBlank)
					offset += 1;	/* Ctrl 59 */
				if (bHaveHBlank || bHaveVBlank || bHaveLongHBlank)
					offset += 3;	/* Ctrl 60/61/62 */
				if (bHaveSlewMetric || bHaveHBlank || bHaveVBlank || bHaveLongHBlank
				    || bHaveNoiseMitigation2 || bHaveSlewOption)
					offset += 1;	/* Ctrl 63 */
				if (bHaveHBlank)
					offset += 28;	/* Ctrl 64/65/66/67 */
				else if (bHaveVBlank || bHaveLongHBlank)
					offset += 4;	/* Ctrl 64/65/66/67 */
				if (bHaveHBlank || bHaveVBlank || bHaveLongHBlank)
					offset += 8;	/* Ctrl 68/69/70/71/72/73 */
				if (bHaveSlewMetric)
					offset += 2;	/* Ctrl 74 */
				bHaveEnhancedStretch = ((Data[9] & 0x80) == 0x80);
				if (bHaveEnhancedStretch)
					offset += (int)NumberOfSensingFrequencies;	/* Ctrl 75 */
				bHaveStartupFastRelaxation = ((Data[11] & 0x08) == 0x08);
				if (bHaveStartupFastRelaxation)
					offset += 1;	/* Ctrl 76 */
				bHaveESDControl = ((Data[11] & 0x10) == 0x10);
				if (bHaveESDControl)
					offset += 2;	/* Ctrl 77/78 */
				if (bHaveNoiseMitigation2)
					offset += 5;	/* Ctrl 79/80/81/82/83 */
				bHaveEnergyRatioRelaxation = ((Data[11] & 0x80) == 0x80);
				if (bHaveEnergyRatioRelaxation)
					offset += 2;	/* Ctrl 84/85 */
				bHaveF54Query13 = ((Data[12] & 0x08) == 0x08);
				if (bHaveSenseFrequencyControl) {
					query_offset = 13;
					NumberOfSensingFrequencies = (Data[13] & 0x0F);
				} else
					query_offset = 12;
				if (bHaveF54Query13)
					query_offset++;
				bHaveCtrl86 = (bHaveF54Query13 && ((Data[13] & 0x01) == 0x01));
				bHaveCtrl87 = (bHaveF54Query13 && ((Data[13] & 0x02) == 0x02));
				bHaveCtrl88 = ((Data[12] & 0x40) == 0x40);
				if (bHaveCtrl86)
					offset += 1;	/* Ctrl 86 */
				if (bHaveCtrl87)
					offset += 1;	/* Ctrl 87 */
				if (bHaveCtrl88) {
					F54Ctrl88Offset = offset;
					offset++;	/* Ctrl 88; */
				}
				bHaveCtrl89 = ((Data[query_offset] & 0x20) == 0x20);
				if (bHaveCtrl89)
					offset++;
				bHaveF54Query15 = ((Data[12] & 0x80) == 0x80);
				if (bHaveF54Query15)
					query_offset++;
				bHaveCtrl90 = (bHaveF54Query15
					       && ((Data[query_offset] & 0x01) == 0x01));
				if (bHaveCtrl90)
					offset++;	/* offset = 1b */
				bHaveF54Query16 = ((Data[query_offset] & 0x8) == 0x8);

				bHaveF54Query20 = ((Data[query_offset] & 0x10) == 0x10);
				bHaveF54Query21 = ((Data[query_offset] & 0x20) == 0x20);

				bHaveF54Query22 = ((Data[query_offset] & 0x40) == 0x40);
				bHaveF54Query25 = ((Data[query_offset] & 0x80) == 0x80);
				if (bHaveF54Query16)
					query_offset++;	/* query_offset = 15 */
				bHaveF54Query17 = ((Data[query_offset] & 0x1) == 0x1);
				bHaveCtrl92 = ((Data[query_offset] & 0x4) == 0x4);
				bHaveCtrl93 = ((Data[query_offset] & 0x8) == 0x8);
				bHaveCtrl94 = ((Data[query_offset] & 0x10) == 0x10);
				bHaveF54Query18 = bHaveCtrl94;
				bHaveCtrl95 = ((Data[query_offset] & 0x20) == 0x20);
				bHaveF54Query19 = bHaveCtrl95;
				bHaveCtrl99 = ((Data[query_offset] & 0x40) == 0x40);
				bHaveCtrl100 = ((Data[query_offset] & 0x80) == 0x80);
				if (bHaveF54Query17)
					query_offset++;	/* query_offset = 16 */
				if (bHaveF54Query18)
					query_offset++;	/* query_offset = 17 */
				if (bHaveF54Query19)
					query_offset++;	/* query_offset = 18 */

				if (bHaveF54Query20)
					query_offset++;	/* query_offset = 19 */
				if (bHaveF54Query21)
					query_offset++;	/* query_offset = 20 */

				bHaveCtrl91 = ((Data[query_offset] & 0x4) == 0x4);
				bHaveCtrl96 = ((Data[query_offset] & 0x8) == 0x8);
				bHaveCtrl97 = ((Data[query_offset] & 0x10) == 0x10);
				bHaveCtrl98 = ((Data[query_offset] & 0x20) == 0x20);

				bHaveF54Query24 = ((Data[query_offset] & 0x80) == 0x80);

				if (bHaveF54Query22)
					query_offset++;	/* query_offset = 21 */
				bHaveCtrl101 = ((Data[query_offset] & 0x2) == 0x2);
				bHaveF54Query23 = ((Data[query_offset] & 0x8) == 0x8);

				bHaveF54Query26 = ((Data[query_offset] & 0x10) == 0x10);
				bHaveCtrl103 = ((Data[query_offset] & 0x10) == 0x10);
				bHaveCtrl104 = ((Data[query_offset] & 0x20) == 0x20);
				bHaveCtrl105 = ((Data[query_offset] & 0x40) == 0x40);
				bHaveF54Query28 = ((Data[query_offset] & 0x80) == 0x80);

				if (bHaveF54Query23) {
					query_offset++;	/* query_offset = 22 */
					bHaveCtrl102 = ((Data[query_offset] & 0x01) == 0x01);
				} else
					bHaveCtrl102 = false;
				if (bHaveCtrl91)
					offset++;
				if (bHaveCtrl92)
					offset++;
				if (bHaveCtrl93)
					offset++;
				if (bHaveCtrl94)
					offset++;
				if (bHaveCtrl95)
					offset++;
				if (bHaveCtrl96)
					offset++;
				if (bHaveCtrl97)
					offset++;
				if (bHaveCtrl98) {
					F54Ctrl98Offset = offset;
					offset++;
				}
				if (bHaveCtrl99)
					offset++;
				if (bHaveCtrl100)
					offset++;
				if (bHaveCtrl101)
					offset++;

				if (bHaveF54Query24)
					query_offset++;
				query_offset++;	/* Query 25 */
				bHaveCtrl106 = ((Data[query_offset] & 0x01) == 0x01);
				bHaveCtrl107 = ((Data[query_offset] & 0x04) == 0x04);
				bHaveCtrl108 = ((Data[query_offset] & 0x08) == 0x08);
				bHaveCtrl109 = ((Data[query_offset] & 0x10) == 0x10);
				bHaveF54Query27 = ((Data[query_offset] & 0x80) == 0x80);
				if (bHaveF54Query26)
					query_offset++;
				if (bHaveF54Query27) {
					query_offset++;
					bHaveCtrl110 = ((Data[query_offset] & 0x01) == 0x01);
					bHaveCtrl111 = ((Data[query_offset] & 0x04) == 0x04);
					bHaveCtrl112 = ((Data[query_offset] & 0x08) == 0x08);
					bHaveCtrl113 = ((Data[query_offset] & 0x10) == 0x10);
					bHaveCtrl114 = ((Data[query_offset] & 0x40) == 0x40);
					bHaveF54Query29 = ((Data[query_offset] & 0x80) == 0x80);
				}
				if (bHaveF54Query28)
					query_offset++;
				if (bHaveF54Query29) {
					query_offset++;
					bHaveCtrl115 = ((Data[query_offset] & 0x01) == 0x01);
					bHaveCtrl116 = ((Data[query_offset] & 0x10) == 0x10);
					bHaveF54Query30 = ((Data[query_offset] & 0x80) == 0x80);
				}
				if (bHaveF54Query30) {
					query_offset++;
					bHaveCtrl119 = ((Data[query_offset] & 0x02) == 0x02);
					bHaveCtrl120 = ((Data[query_offset] & 0x04) == 0x04);
					bHaveCtrl121 = ((Data[query_offset] & 0x08) == 0x08);
					bHaveCtrl122 = ((Data[query_offset] & 0x10) == 0x10);
					bHaveF54Query31 = ((Data[query_offset] & 0x10) == 0x10);
					bHaveCtrl123 = ((Data[query_offset] & 0x40) == 0x40);
					bHaveF54Query32 = ((Data[query_offset] & 0x80) == 0x80);
				}
				if (bHaveF54Query31)
					query_offset++;
				if (bHaveF54Query32) {
					query_offset++;
					bHaveCtrl125 = ((Data[query_offset] & 0x01) == 0x01);
					bHaveCtrl126 = ((Data[query_offset] & 0x02) == 0x02);
					bHaveCtrl127 = ((Data[query_offset] & 0x04) == 0x04);
					bHaveF54Query33 = ((Data[query_offset] & 0x10) == 0x10);
					bHaveF54Query34 = ((Data[query_offset] & 0x40) == 0x40);
					bHaveF54Query35 = ((Data[query_offset] & 0x80) == 0x80);
				}
				if (bHaveF54Query33) {
					query_offset++;
					bHaveCtrl132 = ((Data[query_offset] & 0x10) == 0x10);
					bHaveCtrl133 = ((Data[query_offset] & 0x20) == 0x20);
					bHaveCtrl134 = ((Data[query_offset] & 0x40) == 0x40);
					bHaveF54Query36 = ((Data[query_offset] & 0x80) == 0x80);
				}
				if (bHaveF54Query34)
					query_offset++;
				if (bHaveF54Query35) {
					query_offset++;
					bHaveCtrl137 = ((Data[query_offset] & 0x08) == 0x08);
					bHaveCtrl138 = ((Data[query_offset] & 0x10) == 0x10);
					bHaveCtrl139 = ((Data[query_offset] & 0x20) == 0x20);
					bHaveCtrl140 = ((Data[query_offset] & 0x80) == 0x80);
				}
				if (bHaveF54Query36) {
					query_offset++;
					bHaveCtrl142 = ((Data[query_offset] & 0x02) == 0x02);
					bHaveF54Query37 = ((Data[query_offset] & 0x04) == 0x04);
					bHaveCtrl143 = ((Data[query_offset] & 0x08) == 0x08);
					bHaveCtrl144 = ((Data[query_offset] & 0x10) == 0x10);
					bHaveCtrl145 = ((Data[query_offset] & 0x20) == 0x20);
					bHaveCtrl146 = ((Data[query_offset] & 0x40) == 0x40);
					bHaveF54Query38 = ((Data[query_offset] & 0x80) == 0x80);
				}
				if (bHaveF54Query37)
					query_offset++;
				if (bHaveF54Query38) {
					query_offset++;
					bHaveCtrl147 = ((Data[query_offset] & 0x01) == 0x01);
					bHaveCtrl148 = ((Data[query_offset] & 0x02) == 0x02);
					bHaveCtrl149 = ((Data[query_offset] & 0x04) == 0x04);
				}
				/* -----------------------------------------------------------from Ctrl 103 */
				bHaveCtrl117 = true;	/* Reserved */
				bHaveCtrl118 = false;	/* Reserved */
				bHaveCtrl124 = false;	/* Reserved */
				bHaveCtrl128 = false;	/* Reserved */
				bHaveCtrl129 = false;	/* Reserved */
				bHaveCtrl130 = false;	/* Reserved */
				bHaveCtrl131 = false;	/* Reserved */
				bHaveCtrl135 = false;	/* Reserved */
				bHaveCtrl136 = false;	/* Reserved */
				bHaveCtrl141 = false;	/* Reserved */
				offset = offset - 1;
				if (bHaveCtrl103)
					offset++;
				if (bHaveCtrl104)
					offset++;
				if (bHaveCtrl105)
					offset++;
				if (bHaveCtrl106)
					offset++;
				if (bHaveCtrl107)
					offset++;
				if (bHaveCtrl108)
					offset++;
				if (bHaveCtrl109)
					offset++;
				if (bHaveCtrl110)
					offset++;
				if (bHaveCtrl111)
					offset++;
				if (bHaveCtrl112)
					offset++;
				if (bHaveCtrl113)
					offset++;
				if (bHaveCtrl114)
					offset++;
				if (bHaveCtrl115)
					offset++;
				if (bHaveCtrl116)
					offset++;
				if (bHaveCtrl117)
					offset++;
				if (bHaveCtrl118)
					offset++;
				if (bHaveCtrl119)
					offset++;
				if (bHaveCtrl120)
					offset++;
				if (bHaveCtrl121)
					offset++;
				if (bHaveCtrl122)
					offset++;
				if (bHaveCtrl123)
					offset++;
				if (bHaveCtrl124)
					offset++;
				if (bHaveCtrl125)
					offset++;
				if (bHaveCtrl126)
					offset++;
				if (bHaveCtrl127)
					offset++;
				if (bHaveCtrl128)
					offset++;
				if (bHaveCtrl129)
					offset++;
				if (bHaveCtrl130)
					offset++;
				if (bHaveCtrl131)
					offset++;
				if (bHaveCtrl132)
					offset++;
				if (bHaveCtrl133)
					offset++;
				if (bHaveCtrl134)
					offset++;
				if (bHaveCtrl135)
					offset++;
				if (bHaveCtrl136)
					offset++;
				if (bHaveCtrl137)
					offset++;
				if (bHaveCtrl138)
					offset++;
				if (bHaveCtrl139)
					offset++;
				if (bHaveCtrl140)
					offset++;
				if (bHaveCtrl141)
					offset++;
				if (bHaveCtrl142)
					offset++;
				if (bHaveCtrl143)
					offset++;
				if (bHaveCtrl144)
					offset++;
				if (bHaveCtrl145)
					offset++;
				if (bHaveCtrl146)
					offset++;
				if (bHaveCtrl147)
					offset++;
				if (bHaveCtrl148)
					offset++;
				if (bHaveCtrl149) {
					offset++;
					F54Ctrl149Offset = offset;
				}
				break;
			}
		case 0x55:
			if (!bHaveF55) {
				Read8BitRegisters((cAddr - 3), &F55ControlBase, 1);
				Read8BitRegisters((cAddr - 5), &F55QueryBase, 1);

				Read8BitRegisters(F55QueryBase, &RxChannelCount, 1);
				Read8BitRegisters((F55QueryBase + 1), &TxChannelCount, 1);

				rxCount = 0;
				txCount = 0;
				/* Read Sensor Mapping */
				Read8BitRegisters((F55ControlBase + 1), Data, (int)RxChannelCount);
				for (i = 0; i < (int)RxChannelCount; i++) {
					if (Data[i] != 0xFF) {
						rxCount++;
						TRxPhysical[i] = Data[i];
					} else
						break;
				}
				Read8BitRegisters((F55ControlBase + 2), Data, (int)TxChannelCount);
				for (i = 0; i < (int)TxChannelCount; i++) {
					if (Data[i] != 0xFF) {
						TRxPhysical[rxCount + i] = Data[i];
						txCount++;
					} else
						break;
				}
				for (i = (rxCount + txCount); i < (TRX_mapping_max); i++)
					TRxPhysical[i] = 0xFF;


				RxChannelCount = rxCount;
				TxChannelCount = txCount;
				if (TRX_max <= RxChannelCount)
					RxChannelCount = TRX_max;
				if (TRX_max <= TxChannelCount)
					TxChannelCount = 16;

				MaxArrayLength = (int)RxChannelCount * (int)TxChannelCount * 2;
				if (((int)TxChannelCount - F12_2DTxCount == 0) && ButtonCount > 0)
					ButtonShared = true;


				break;
			}
		default:	/* Any other function */
			break;
		}
		cAddr -= 6;
	} while (true);
}

/* The following function is necessary to setup the Function $54 tests. The setup needs to be done once */
/* after entering into page 0x01. As long as the touch controller stays in page 1, the setup does not */
/* need to be repeated. */
bool TestPreparation(void)
{
	unsigned char data = 0;
	unsigned char addr = 0;

	/* if (!switchPage(0x01)) */
	/* return false; */

	/* Turn off CBC. */
	if (bHaveF54Ctrl07) {
		addr = F54ControlBase + F54Ctrl07Offset;
		Read8BitRegisters(addr, &data, 1);
		/* data = data & 0xEF; */
		data = 0;
		Write8BitRegisters(addr, &data, 1);
	} else if (bHaveCtrl88) {
		addr = F54ControlBase + F54Ctrl88Offset;
		Read8BitRegisters(addr, &data, 1);
		data = data & 0xDF;
		Write8BitRegisters(addr, &data, 1);
	}
	/* Turn off CBC2 */
	if (bHaveCtrl149) {
		addr = F54ControlBase + F54Ctrl149Offset;
		Read8BitRegisters(addr, &data, 1);
		data = data & 0xFE;
		Write8BitRegisters(addr, &data, 1);
	}
	/* Turn off 0D CBC. */
	if (bHaveF54Ctrl57) {
		addr = F54ControlBase + F54Ctrl57Offset;
		Read8BitRegisters(addr, &data, 1);
		/* ata = data & 0xEF; */
		data = 0;
		Write8BitRegisters(addr, &data, 1);
	}
	/* Turn off SignalClarity. ForceUpdate is required for the change to be effective */
	if (SignalClarityOn) {
		addr = F54ControlBase + F54Ctrl41Offset;
		Read8BitRegisters(addr, &data, 1);
		data = data | 0x01;
		Write8BitRegisters(addr, &data, 1);
	}
	/* Apply ForceUpdate. */
	Read8BitRegisters(F54CommandBase, &data, 1);
	pr_debug("s3528 Apply ForceUpdate => F54CommandBase\n");
	data = data | 0x04;
	Write8BitRegisters(F54CommandBase, &data, 1);
	pr_debug("s3528 Apply ForceUpdate=>F54CommandBase\n");
	/* Wait complete */
	count = 0;
	do {
		Read8BitRegisters(F54CommandBase, &data, 1);
		usleep_range(1000, 2000);
		count++;
	} while (data != 0x00 && (count < DefaultTimeout));

	if (count >= DefaultTimeout) {
		outbuf +=
		    sprintf(f54_wlog_buf + outbuf, "Timeout -- ForceUpdate can not complete\n");
		/* TOUCH_INFO_MSG("Timeout -- ForceUpdate can not complete\n"); */
		Reset();
		return false;
	}
	/* Apply ForceCal. */
	Read8BitRegisters(F54CommandBase, &data, 1);
	data = data | 0x02;
	Write8BitRegisters(F54CommandBase, &data, 1);

	/* Wait complete */
	count = 0;
	do {
		Read8BitRegisters(F54CommandBase, &data, 1);
		usleep_range(1000, 2000);
		count++;
	} while (data != 0x00 && (count < DefaultTimeout));

	if (count >= DefaultTimeout) {
		outbuf += sprintf(f54_wlog_buf + outbuf, "Timeout -- ForceCal can not complete\n");
		/* TOUCH_INFO_MSG("Timeout -- ForceCal can not complete\n"); */
		Reset();
		return false;
	}

	return true;
}

int diffnode(unsigned short *ImagepTest)
{

	int i = 0;
	int k = 0;
	unsigned char data;

	if (!bHaveF54) {
		TOUCH_INFO_MSG("not bHaveF54\n");
		return -1;
	}
	if (!switchPage(0x01)) {
		TOUCH_INFO_MSG("not switchPage(0x01)\n");
		return -1;
	}

	if (TestPreparation()) {

		/* memcpy(LowerImageLimit, LowerImage, sizeof(LowerImageLimit)); */
		/* memcpy(UpperImageLimit, UpperImage, sizeof(UpperImageLimit)); */
		data = 20;	/* rawdata mode */
		Write8BitRegisters(F54DataBase, &data, 1);
		data = 0x01;
		Write8BitRegisters(F54CommandBase, &data, 1);
		count = 0;
		do {
			Read8BitRegisters(F54CommandBase, &data, 1);
			usleep_range(1000, 2000);
			count++;
		} while (data != 0x00 && (count < DefaultTimeout));
		if (count >= DefaultTimeout) {
			TOUCH_INFO_MSG("Timeout -- Not supported Report Type in FW\n");
			Reset();
			return -1;
		}

		Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], MaxArrayLength);

		for (i = 0; i < (int)TxChannelCount * (int)RxChannelCount; i++) {
			ImagepTest[i] = ((short)Data[k] | (short)Data[k + 1] << 8);
			k = k + 2;
		}
		/* Reset Device */
		Reset();
		TOUCH_INFO_MSG("diff_node success\n");
		return 0;

	} else {
		return -1;
	}

}

/* The following function illustrates the steps in getting a full raw image report (report #20) by Function $54. */
int ImageTest(int mode, char *buf)
{
	unsigned char data;

	if (TestPreparation()) {
		/* memcpy(LowerImageLimit, LowerImage, sizeof(LowerImageLimit)); */
		/* memcpy(UpperImageLimit, UpperImage, sizeof(UpperImageLimit)); */

		/* Assign report type for Full Raw Image */

		data = 20;	/* raw capacitance mode */
		TOUCH_INFO_MSG("[Touch][%s] raw capacitance mode!\n", __func__);

		Write8BitRegisters(F54DataBase, &data, 1);
		/* mode setting */
		do_gettimeofday(&t_interval[STARTTIME]);

		if (mode == 0)
			data = 'a';	/* rawdata store mode */
		else
			data = 'l';	/* rawdata display mode */

		return ReadReport(data, buf);

	} else {
		return -1;
	}
}

int DeltaTest(char *buf)
{
	unsigned char data;

	/* memcpy(LowerImageLimit, LowerImage, sizeof(LowerImageLimit)); */
	/* memcpy(UpperImageLimit, UpperImage, sizeof(UpperImageLimit)); */

	/* Assign report type for Full Raw Image */
	data = 0x02;		/* delta mode */
	TOUCH_INFO_MSG("[Touch][%s] delta mode!\n", __func__);

	Write8BitRegisters(F54DataBase, &data, 1);

	do_gettimeofday(&t_interval[STARTTIME]);

	data = 'l';		/* rawdata display mode */

	return ReadReport(data, buf);

}


int NoiseDeltaTest(char *buf)
{
	unsigned char data;

	memset(NoiseDeltaMin, 0, TRX_MAX * TRX_MAX * sizeof(short));
	memset(NoiseDeltaMax, 0, TRX_MAX * TRX_MAX * sizeof(short));

	TOUCH_INFO_MSG("[Touch][%s] Noise Delta mode!\n", __func__);

	/* Assign report type for Full Raw Image */
	data = 0x02;		/* delta mode */

	Write8BitRegisters(F54DataBase, &data, 1);

	data = 'm';		/* rawdata display mode */

	return ReadReport(data, buf);

}

/* The following function illustrates the steps in getting a sensor speed test report (report #22) by Function $54. */
int SensorSpeed(char *buf)
{
	unsigned char data;

	memcpy(SensorSpeedLowerImageLimit, SensorSpeedLowerImage,
	       sizeof(SensorSpeedLowerImageLimit));
	memcpy(SensorSpeedUpperImageLimit, SensorSpeedUpperImage,
	       sizeof(SensorSpeedUpperImageLimit));

	/* Assign report type for Sensor Speed Test */
	data = 22;
	Write8BitRegisters(F54DataBase, &data, 1);

	do_gettimeofday(&t_interval[STARTTIME]);

	data = 'c';
	return ReadReport(data, buf);

}

/* The following function illustrates the steps in getting a ADC Range report (report #23) by Function $54. */
int ADCRange(char *buf)
{
	unsigned char data = 0;

	memcpy(ADCLowerImageLimit, ADCLowerImage, sizeof(ADCLowerImageLimit));
	memcpy(ADCUpperImageLimit, ADCUpperImage, sizeof(ADCUpperImageLimit));

	Read8BitRegisters((F54ControlBase + F54Ctrl41Offset), &data, 1);
	if (data & 0x01)
		SignalClarityOn = false;
	else
		SignalClarityOn = true;

	/* Assign report type for ADC Range report */
	data = 23;
	Write8BitRegisters(F54DataBase, &data, 1);

	do_gettimeofday(&t_interval[STARTTIME]);
	/* startTime = GetTickCount(); */

	data = 'b';
	return ReadReport(data, buf);
}

void AbsADCRange(char *buf)
{
	unsigned char data;

	if (TestPreparation()) {
		/* Assign report type for Abs Sensing ADC Range report */
		data = 42;
		Write8BitRegisters(F54DataBase, &data, 1);

		do_gettimeofday(&t_interval[STARTTIME]);
		/* startTime = GetTickCount(); */

		data = 'i';
		ReadReport(data, buf);
	}
}

/* report type 40 */
int AbsDelta(char *buf)
{
	unsigned char data;

	/* --switchPage(0x01); */

	/* Assign report type for Abs Sensing Delta Capacitance report */
	data = 40;
	Write8BitRegisters(F54DataBase, &data, 1);

	do_gettimeofday(&t_interval[STARTTIME]);
	/* startTime = GetTickCount(); */

	data = 'j';
	return ReadReport(data, buf);
}

/* report type 38 */
int AbsRaw(int mode, char *buf)
{
	unsigned char data;

	/* switchPage(0x01); */
	if (bHaveCtrl98) {
		Read8BitRegisters((F54ControlBase + F54Ctrl98Offset), &Data[0], 6);

		AbsRxShortLimit = AbsRawRef[Data[0]] * 275 / 100;	/* AbsRx Low Reference */
		AbsTxShortLimit = AbsRawRef[Data[5]] * 275 / 100;	/* AbsTx Low Reference */
		AbsTxOpenLimit = AbsRawRef[Data[5]] * 75 / 100;	/* AbsTx Low Reference */
	}

	data = 38;
	Write8BitRegisters(F54DataBase, &data, 1);

	do_gettimeofday(&t_interval[STARTTIME]);
	/* startTime = GetTickCount(); */

	if (mode == 1)
		data = 'n';	/* Abs Sensing Short Test mode */
	else if (mode == 2)
		data = 'o';	/* Abs Sensing Open Test mode */
	else
		data = 'k';	/* Abs Sensing Raw Test mode */

	return ReadReport(data, buf);
}

/* The following function illustrates the steps in getting
a TRex-Opens(No sensor) report (report #24) by Function $54. */
void TRexOpenTest(char *buf)
{
	unsigned char data;

	/* fprintf(stderr, "Press any key to continue after you have lowered the bar.\n"); */
	/* _getch(); */

	if (TestPreparation()) {
		/* Assign report type for TRex Open Test */
		data = 24;
		Write8BitRegisters(F54DataBase, &data, 1);

		data = 'd';
		ReadReport(data, buf);
	}
}

/* The following function illustrates the steps in getting
a TRex-to-GND(No sensor) report (report #25) by Function $54. */
int TRexGroundTest(char *buf)
{
	unsigned char data;

	if (TestPreparation()) {
		/* Assign report type for TRex Ground Test */
		data = 25;
		Write8BitRegisters(F54DataBase, &data, 1);

		data = 'e';
		return ReadReport(data, buf);
	} else {
		return -1;
	}
}

/* The following function illustrates the steps in getting
a TRex-TRex short(No sensor) report (report #26) by Function $54. */
int TRexShortTest(char *buf)
{

	unsigned char data;

	if (TestPreparation()) {
		/* Assign report type for TRex Short Test */
		data = 26;
		Write8BitRegisters(F54DataBase, &data, 1);
		data = 'f';
		return ReadReport(data, buf);
	} else {
		return -1;
	}
}

/* This test is to retrieve the high resistance report, report type #4. */
int HighResistanceTest(char *buf)
{
	unsigned char data;

	if (TestPreparation()) {

		/* Assign report type for High Resistance report */
		data = 4;
		Write8BitRegisters(F54DataBase, &data, 1);
		data = 'g';
		return ReadReport(data, buf);
	} else {
		return -1;
	}
}

/* This test is to retrieve the maximum and minimum pixel report, report type #13. */
void MaxMinTest(char *buf)
{
	unsigned char data;

	if (TestPreparation()) {
		/* Assign report type for Max Min report */
		data = 13;
		Write8BitRegisters(F54DataBase, &data, 1);

		data = 'h';
		ReadReport(data, buf);
	}
}

void CheckCrash(char *rst, int min_caps_value)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int node_crack_count = 0;
	int rx_crack_count = 0;
	int row_crack_count = 0;
	unsigned char cmd;
	int ret = 0;

	if (TestPreparation() == false) {
		TOUCH_INFO_MSG("TestPreparation failed\n");
		goto error;
	}

	cmd = 20;
#ifdef DEF_DO_SAFE
	DO_SAFE(Write8BitRegisters(F54DataBase, &cmd, 1), error);
#else
	ret = Write8BitRegisters(F54DataBase, &cmd, 1);
	if (ret < 0) {
		pr_debug("[Touch E] Action Failed [%s %d]\n", __func__, __LINE__);
		goto error;
	}
#endif				/* DEF_DO_SAFE */

	cmd = 0x01;
#ifdef DEF_DO_SAFE
	DO_SAFE(Write8BitRegisters(F54CommandBase, &cmd, 1), error);
#else
	ret = Write8BitRegisters(F54CommandBase, &cmd, 1);
	if (ret < 0) {
		pr_debug("[Touch E] Action Failed [%s %d]\n", __func__, __LINE__);
		goto error;
	}
#endif				/* DEF_DO_SAFE */

	count = 0;
	do {
#ifdef DEF_DO_SAFE
		DO_SAFE(Read8BitRegisters(F54CommandBase, &cmd, 1), error);
#else
		ret = Read8BitRegisters(F54CommandBase, &cmd, 1);
		if (ret < 0) {
			pr_debug("[Touch E] Action Failed [%s %d]\n", __func__, __LINE__);
			goto error;
		}
#endif				/* DEF_DO_SAFE */
		usleep_range(1000, 2000);
		count++;
	} while (cmd != 0x00 && (count < DefaultTimeout));

#ifdef DEF_DO_SAFE
	DO_SAFE(Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), Data, MaxArrayLength), error);
#else
	ret = Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), Data, MaxArrayLength);
	if (ret < 0) {
		pr_debug("[Touch E] Action Failed [%s %d]\n", __func__, __LINE__);
		goto error;
	}
#endif				/* DEF_DO_SAFE */

	for (i = 0; i < (int)TxChannelCount; i++) {
		for (j = 0; j < (int)RxChannelCount; j++) {
			ImagepF[i][j] = ((short)Data[k] | (short)Data[k + 1] << 8);

			if ((ImagepF[i][j] < LowerImageLimit[i][j])
			    || (ImagepF[i][j] > UpperImageLimit[i][j])) {
				if (ImagepF[i][j] < min_caps_value) {
					rx_crack_count++;
					node_crack_count++;
				} else {
					row_crack_count = 0;
				}

				if (F12_2DRxCount <= rx_crack_count)
					row_crack_count++;

				if (5 < row_crack_count
				    || (F12_2DTxCount * F12_2DRxCount * 40 / 100) <
				    node_crack_count) {
					sprintf(rst, "%d", 1);
					return;
				}
			}

			k += 2;
		}

		rx_crack_count = 0;
	}

	sprintf(rst, "%d", 0);
	TOUCH_INFO_MSG("Tx [%d] Rx [%d] node_crack_count %d, row_crack_count %d\n",
		       i, j, node_crack_count, row_crack_count);

	return;
error:
	sprintf(rst, "%d", 0);
	pr_debug("[s3528]winodw crack check fail\n");
}

void SCAN_PDT(void)
{
	int i;

	for (i = 0; i < scanMaxPageCount; i++) {
		if (switchPage(i))
			RunQueries();
	}
}

/* Main entry point for the application */
int F54Test(int input, int mode, char *buf)
	/* int _tmain(int argc, _TCHAR* argv[]) */
{
	int ret = 0;
	unsigned char data;

	pr_debug("[s3528][F54TEST] input = %d, mode = %d\n", input, mode);

	/* if (PowerOnSensor()) */
	/* { */
	/* fatal("Error powering on sensor.\n"); */
	/* } */

	/* These four function calls are to scan the Page Description Table (PDT) */
	/* Function $01, $11 and $34 are on page 0 */
	/* Function $54 is on page 1 */
	/* Function $55 is on Page ? */
	/* Scan up to Page 4 */
	/* for (int i = 0; i < scanMaxPageCount ;i++) */
	/* { */
	/* if (switchPage(i)) */
	/* RunQueries(); */
	/* } */

	/* Application should exit with the absence of Function $54 */
/* if (!bHaveF54) */
/* return -1; */
	/* exit(0); */
	/* LoadTestLimits(); */
	/*
	   while (1){
	   printf("\nPress these keys for tests:\n");
	   printf(" a ) - Full Raw Capacitance Test\n");
	   printf(" b ) - ADC Range Test\n");
	   printf(" c ) - Sensor Speed Test\n");
	   printf(" d ) - TRex Open Test\n");
	   printf(" e ) - TRex Gnd Test\n");
	   printf(" f ) - TRx-to-TRx and TRx-to-Vdd shorts\n");
	   printf(" g ) - High Resistance Test\n");
	   printf(" h ) - Full Raw Capacitance Max/Min Test\n");
	   printf(" i ) - Abs Sensing ADC Range Test\n");
	   printf(" j ) - Abs Sensing Delta Capacitance\n");
	   printf(" k ) - Abs Sensing Raw Capcitance Test\n");
	   printf("---------------------------------------------------------------");
	   printf("\n z ) - Version\n");
	   printf("\nPress any other key to exit.\n");
	   input = _getch();
	 */
	if (!switchPage(0x01))
		return false;

	data = 0x00;
	Write8BitRegisters(F54DataBase + 1, &data, 1);
	Write8BitRegisters(F54DataBase + 2, &data, 1);

	outbuf = 0;
	out_buf = 0;
	memset(f54_wlog_buf, 0, sizeof(f54_wlog_buf));
	memset(wlog_buf, 0, sizeof(wlog_buf));

	switch (input) {
	case 'a':
		outbuf = sprintf(f54_wlog_buf, "a - Full Raw Capacitance Test\n");
		ret = ImageTest(mode, buf);
		break;
	case 'b':
		out_buf = sprintf(wlog_buf, "b - ADC Range Test\n");
		ret = ADCRange(buf);
		break;
	case 'c':
		out_buf = sprintf(wlog_buf, "c - Sensor Speed Test\n");
		ret = SensorSpeed(buf);
		break;
	case 'd':
		outbuf = sprintf(f54_wlog_buf, "d - TRex Open Test\n");
		TRexOpenTest(buf);
		break;
	case 'e':		/* B2Lite */
		outbuf = sprintf(f54_wlog_buf, "e - TRex Gnd Test\n");
		ret = TRexGroundTest(buf);
		break;
	case 'f':		/* B2Lite */
		outbuf = sprintf(f54_wlog_buf, "f - TRex Short Test\n");
		ret = TRexShortTest(buf);
		break;
	case 'g':		/* B2Lite */
		outbuf = sprintf(f54_wlog_buf, "g - High Resistance Test\n");
		ret = HighResistanceTest(buf);
		break;
	case 'h':
		outbuf = sprintf(f54_wlog_buf, "h - Full Raw Capacitance Max/Min Test\n");
		MaxMinTest(buf);
		break;
	case 'i':
		outbuf = sprintf(f54_wlog_buf, "i - Abs Sensing ADC Range Test\n");
		AbsADCRange(buf);
		break;
	case 'j':
		outbuf = sprintf(f54_wlog_buf, "j - Abs Sensing Delta Capacitance\n");
		ret = AbsDelta(buf);
		break;
	case 'k':
		outbuf = sprintf(f54_wlog_buf, "k - Abs Sensing Raw Capcitance Test\n");
		AbsRaw(mode, buf);
		break;
	case 'l':
		CheckCrash(buf, mode);
		break;
	case 'm':
		ret = DeltaTest(buf);
		break;
	case 'n':
		outbuf = sprintf(f54_wlog_buf, "n - Abs Sensing Raw Short Capcitance Test\n");
		ret = AbsRaw(mode, buf);
		break;
	case 'o':
		outbuf = sprintf(f54_wlog_buf, "k - Abs Sensing Raw Open Capcitance Test\n");
		ret = AbsRaw(mode, buf);
		break;
	case 'x':		/* B2Lite */
		out_buf = sprintf(wlog_buf, "x - Noise Delta Test\n");
		ret = NoiseDeltaTest(buf);
		break;
	case 'z':
		TOUCH_INFO_MSG("Version: %s\n", VERSION);
		break;
	default:
		return -1;
		/* PowerOffSensor(); */
		/* exit(0); */
	}
	return ret;
}
