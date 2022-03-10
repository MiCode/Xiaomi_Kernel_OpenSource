/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CAM_CAL_FORMAT_H
#define __CAM_CAL_FORMAT_H

/*****************************************************************************
 * Marco
 *****************************************************************************/

#define CAM_CAL_MAX_LSC_SIZE 0x840
#define MAX_SENSOR_CAL_SIZE             (2048)
#define CAM_CAL_SINGLE_LSC_SIZE 0x74c
#define MAX_MTK_SHADING_SLIM_TABLE_SIZE (2048)
#define MAX_SENSOR_SHADING_TALE_SIZE MAX_SENSOR_CAL_SIZE
#define CAM_CAL_PDAF_SIZE 0x1c00
#define CAM_CAL_Stereo_Data_SIZE 1360
#define CAM_CAL_AWB_BITEN (0x01<<0)
#define CAM_CAL_AF_BITEN (0x01<<1)
#define CAM_CAL_NONE_BITEN (0x00)

#define CAM_CAL_ERR_NO_DEVICE       0x8FFFFFFF
#define CAM_CAL_ERR_NO_CMD          0x1FFFFFFF
#define CAM_CAL_ERR_NO_ERR          0x00000000
#define CAM_CAL_ERR_NO_VERSION      0x00000001
#define CAM_CAL_ERR_NO_PARTNO       0x00000010
#define CAM_CAL_ERR_NO_SHADING      0x00000100
#define CAM_CAL_ERR_NO_3A_GAIN      0x00001000
#define CAM_CAL_ERR_NO_PDAF         0x00010000
#define CAM_CAL_ERR_NO_Stereo_Data  0x00100000
#define CAM_CAL_ERR_DUMP_FAILED     0x00200000
#define CAM_CAL_ERR_NO_LENS_ID      0x00400000

#define CamCalReturnErr_MAX 7

/*****************************************************************************
 * Enums
 *****************************************************************************/
/** @defgroup cam_cal_enum Enum
 *	@{
 */

enum ENUM_CAMERA_CAM_CAL_TYPE_ENUM {
	CAMERA_CAM_CAL_DATA_MODULE_VERSION = 0,
	CAMERA_CAM_CAL_DATA_PART_NUMBER,
	CAMERA_CAM_CAL_DATA_SHADING_TABLE,
	CAMERA_CAM_CAL_DATA_3A_GAIN,
	CAMERA_CAM_CAL_DATA_PDAF,
	CAMERA_CAM_CAL_DATA_STEREO_DATA,
	CAMERA_CAM_CAL_DATA_DUMP,
	CAMERA_CAM_CAL_DATA_LENS_ID,
	CAMERA_CAM_CAL_DATA_LIST
};

enum ENUM_CAM_CAL_DATA_VER_ENUM {
	CAM_CAL_SINGLE_EEPROM_DATA,
	CAM_CAL_DOUBLE_EEPROM_DATA,
	CAM_CAL_SINGLE_OTP_DATA,
	CAM_CAL_TYPE_NUM
};

/*****************************************************************************
 * Structures
 *****************************************************************************/

/** @brief This Structure defines the struct STRUCT_CAM_CAL_Stereo_Data_STRUCT.  */

struct STRUCT_CAM_CAL_Stereo_Data_STRUCT {
	unsigned int Size_of_Stereo_Data;
	unsigned char Data[CAM_CAL_Stereo_Data_SIZE];
};

/** @brief This Structure defines the LSC Table.  */

struct STRUCT_CAM_CAL_LSC_MTK_TYPE {
	unsigned char  MtkLscType;
	unsigned char  PixId;
	unsigned short TableSize;
	unsigned int   SlimLscType;
	unsigned int   PreviewWH;
	unsigned int   PreviewOffSet;
	unsigned int   CaptureWH;
	unsigned int   CaptureOffSet;
	unsigned int   PreviewTblSize;
	unsigned int   CaptureTblSize;
	unsigned int   PvIspReg[5];
	unsigned int   CapIspReg[5];
	unsigned char  CapTable[MAX_MTK_SHADING_SLIM_TABLE_SIZE];
};

struct STRUCT_CAM_CAL_LSC_SENSOR_TYPE {
	unsigned char	MtkLscType;
	unsigned char	PixId;
	unsigned short  TableSize;
	unsigned char	SensorTable[MAX_SENSOR_SHADING_TALE_SIZE];
	unsigned char	Reserve[CAM_CAL_MAX_LSC_SIZE -
		sizeof(char) - sizeof(char) - sizeof(short) -
		sizeof(char)*MAX_SENSOR_SHADING_TALE_SIZE];
};

union UNION_CAM_CAL_LSC_DATA {
	unsigned char Data[CAM_CAL_MAX_LSC_SIZE];
	struct STRUCT_CAM_CAL_LSC_MTK_TYPE	   MtkLcsData;
	struct STRUCT_CAM_CAL_LSC_SENSOR_TYPE SensorLcsData;
};

struct STRUCT_CAM_CAL_SINGLE_LSC_STRUCT {
	unsigned int TableRotation;
	union UNION_CAM_CAL_LSC_DATA LscTable;
};

/** @brief This Structure defines the 2A Table.  */

struct STRUCT_CAM_CAL_PREGAIN_STRUCT {
	unsigned int rGoldGainu4R;
	unsigned int rGoldGainu4G;
	unsigned int rGoldGainu4B;
	unsigned int rUnitGainu4R;
	unsigned int rUnitGainu4G;
	unsigned int rUnitGainu4B;

	unsigned int rGainSetNum;
	unsigned int rGoldGainu4R_mid;
	unsigned int rGoldGainu4G_mid;
	unsigned int rGoldGainu4B_mid;
	unsigned int rUnitGainu4R_mid;
	unsigned int rUnitGainu4G_mid;
	unsigned int rUnitGainu4B_mid;
	unsigned int rGoldGainu4R_low;
	unsigned int rGoldGainu4G_low;
	unsigned int rGoldGainu4B_low;
	unsigned int rUnitGainu4R_low;
	unsigned int rUnitGainu4G_low;
	unsigned int rUnitGainu4B_low;

	unsigned char rValueR;
	unsigned char rValueGr;
	unsigned char rValueGb;
	unsigned char rValueB;
	unsigned char rGoldenR;
	unsigned char rGoldenGr;
	unsigned char rGoldenGb;
	unsigned char rGoldenB;
};

struct STRUCT_CAM_CAL_AF_STRUCT {
	unsigned short Close_Loop_AF_Min_Position;
	unsigned short Close_Loop_AF_Max_Position;
	unsigned char  Close_Loop_AF_Hall_AMP_Offset;
	unsigned char  Close_Loop_AF_Hall_AMP_Gain;
	unsigned short AF_infinite_pattern_distance;
	unsigned short AF_Macro_pattern_distance;
	unsigned char  AF_infinite_calibration_temperature;
	unsigned char  AF_macro_calibration_temperature;
	unsigned char  AF_dac_code_bit_depth;
	unsigned short Posture_AF_infinite_calibration;
	unsigned short Posture_AF_macro_calibration;
	unsigned short AF_Middle_calibration;
	unsigned char  AF_Middle_calibration_temperature;
	unsigned char  Optical_zoom_cali_num;
	unsigned char  Optical_zoom_AF_cali[40];

	unsigned char  Reserved[9];
};

struct STRUCT_CAM_CAL_SINGLE_2A_STRUCT {
	unsigned char  S2aVer; //version : 01
	unsigned char  S2aBitEn; //bit enable: 03 Bit0: AF Bit1: WB
	unsigned char  S2aAfBitflagEn; //Bit: step 0(inf.), 1(marco), 2, 3, 4,5,6,7
	unsigned short S2aAf[8];      //0x012c
	struct STRUCT_CAM_CAL_PREGAIN_STRUCT S2aAwb; //0x012c
	struct STRUCT_CAM_CAL_AF_STRUCT S2aAF_t;
};

/** @brief This structure defines the PDAF Table.  */

struct STRUCT_CAM_CAL_PDAF_STRUCT {
	unsigned int Size_of_PDAF;
	unsigned char Data[CAM_CAL_PDAF_SIZE];
};

/** @brief This enum defines the CAM_CAL Table.  */

struct STRUCT_CAM_CAL_DATA_STRUCT {
	enum ENUM_CAMERA_CAM_CAL_TYPE_ENUM Command;
	enum ENUM_CAM_CAL_DATA_VER_ENUM DataVer;
	unsigned char PartNumber[24];
	unsigned int  sensorID;
	unsigned int  deviceID;
	struct STRUCT_CAM_CAL_SINGLE_LSC_STRUCT   SingleLsc;
	struct STRUCT_CAM_CAL_SINGLE_2A_STRUCT    Single2A;
	struct STRUCT_CAM_CAL_PDAF_STRUCT         PDAF;
	struct STRUCT_CAM_CAL_Stereo_Data_STRUCT  Stereo_Data;
	unsigned char LensDrvId[10];
};

/**
 * Type define for data struct unbundle
 **/

struct STRUCT_CAM_CAL_MODULE_VERSION_STRUCT {
	enum ENUM_CAM_CAL_DATA_VER_ENUM DataVer;
};

struct STRUCT_CAM_CAL_PART_NUM_STRUCT {
	unsigned char PartNumber[24];
};

struct STRUCT_CAM_CAL_LSC_DATA_STRUCT {
	struct STRUCT_CAM_CAL_SINGLE_LSC_STRUCT   SingleLsc;
};

struct STRUCT_CAM_CAL_2A_DATA_STRUCT {
	struct STRUCT_CAM_CAL_SINGLE_2A_STRUCT    Single2A;
};

struct STRUCT_CAM_CAL_PDAF_DATA_STRUCT {
	struct STRUCT_CAM_CAL_PDAF_STRUCT         PDAF;
};

struct STRUCT_CAM_CAL_STEREO_DATA_STRUCT {
	struct STRUCT_CAM_CAL_Stereo_Data_STRUCT  Stereo_Data;
};

struct STRUCT_CAM_CAL_LENS_ID_STRUCT {
	unsigned char LensDrvId[10];
};

struct STRUCT_CAM_CAL_NEED_POWER_ON {
	enum ENUM_CAMERA_CAM_CAL_TYPE_ENUM Command;
	unsigned int sensorID;
	unsigned int deviceID;
	unsigned int needPowerOn;
};

/*****************************************************************************
 * Arrary
 *****************************************************************************/
/** @defgroup cam_cal_hanele_struct Struct
 *  @{
 **/

const static unsigned int CamCalReturnErr[CAMERA_CAM_CAL_DATA_LIST] = {
	CAM_CAL_ERR_NO_VERSION,
	CAM_CAL_ERR_NO_PARTNO,
	CAM_CAL_ERR_NO_SHADING,
	CAM_CAL_ERR_NO_3A_GAIN,
	CAM_CAL_ERR_NO_PDAF,
	CAM_CAL_ERR_NO_Stereo_Data,
	CAM_CAL_ERR_DUMP_FAILED,
	CAM_CAL_ERR_NO_LENS_ID
};

const static char CamCalErrString[CAMERA_CAM_CAL_DATA_LIST][24] = {
	{"ERR_NO_VERSION"},
	{"ERR_NO_PARTNO"},
	{"ERR_NO_SHADING"},
	{"ERR_NO_3A_GAIN"},
	{"ERR_NO_PDAF"},
	{"ERR_NO_Stereo_Data"},
	{"ERR_Dump_Failed"},
	{"ERR_NO_LENS_ID"}
};

#endif /* __CAM_CAL_FORMAT_H */
