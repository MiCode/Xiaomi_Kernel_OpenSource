/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _KD_IMGSENSOR_DATA_H
#define _KD_IMGSENSOR_DATA_H

/* #include "../camera/kd_camera_hw.h" */
#include "kd_camera_feature.h"

#define SENSOR_CLOCK_POLARITY_HIGH     0
#define SENSOR_CLOCK_POLARITY_LOW      1
/* #define CONFIG_COMPAT */
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif



/*************************************************
*
**************************************************/
/* In KERNEL mode,SHOULD be sync with mediatype.h */
/* CHECK before remove or modify */
/* #undef BOOL */
/* #define BOOL signed int */
#ifndef _MEDIA_TYPES_H
typedef unsigned char MUINT8;
typedef unsigned short MUINT16;
typedef unsigned int MUINT32;
typedef signed char MINT8;
typedef signed short MINT16;
typedef signed int MINT32;
#endif

/*******************************************************************************
*
********************************************************************************/
/*  */
/* msdk_isp_exp.h */
/*  */
#define BASEGAIN 0x40
#define BASEGAIN_SHIFT 6

typedef enum {
	ISP_DRIVING_2MA = 0,
	ISP_DRIVING_4MA,
	ISP_DRIVING_6MA,
	ISP_DRIVING_8MA
} ISP_DRIVING_CURRENT_ENUM;



enum {
	IMAGE_NORMAL = 0,
	IMAGE_H_MIRROR,
	IMAGE_V_MIRROR,
	IMAGE_HV_MIRROR
};


typedef enum {
	MSDK_SCENARIO_ID_CAMERA_PREVIEW = 0,
	MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG,
	MSDK_SCENARIO_ID_VIDEO_PREVIEW,
	MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO,
	MSDK_SCENARIO_ID_CAMERA_ZSD,
	MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW,
	MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE,
	MSDK_SCENARIO_ID_CAMERA_3D_VIDEO,
	MSDK_SCENARIO_ID_TV_OUT,
	MSDK_SCENARIO_ID_SLIM_VIDEO,
	MSDK_SCENARIO_ID_CUSTOM1,
	MSDK_SCENARIO_ID_CUSTOM2,
	MSDK_SCENARIO_ID_CUSTOM3,
	MSDK_SCENARIO_ID_CUSTOM4,
	MSDK_SCENARIO_ID_CUSTOM5,
	MSDK_SCENARIO_ID_MAX,
}   MSDK_SCENARIO_ID_ENUM;


typedef enum {
	MSDK_CAMERA_OPERATION_NORMAL_MODE = 0,
	MSDK_CAMERA_OPERATION_META_MODE
} ACDK_CAMERA_OPERATION_MODE_ENUM;


/*******************************************************************************
*
********************************************************************************/

/*  */
#define MAX_NUM_OF_SUPPORT_SENSOR 16
/*  */
#define SENSOR_CLOCK_POLARITY_HIGH    0
#define SENSOR_CLOCK_POLARITY_LOW 1
/*  */
#define LENS_DRIVER_ID_DO_NOT_CARE    0xFFFFFFFF
#define SENSOR_DOES_NOT_EXIST     0x00FFFFFF
#define SENSOR_DOES_NOT_KNOW      0xFFFFFFFF

#define SENSOR_FEATURE_START                     3000
typedef enum {
	SENSOR_FEATURE_BEGIN = SENSOR_FEATURE_START,
	SENSOR_FEATURE_GET_RESOLUTION,
	SENSOR_FEATURE_GET_PERIOD,
	SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ,
	SENSOR_FEATURE_SET_ESHUTTER,
	SENSOR_FEATURE_SET_NIGHTMODE,
	SENSOR_FEATURE_SET_GAIN,
	SENSOR_FEATURE_SET_GAIN_AND_ESHUTTER,
	SENSOR_FEATURE_SET_FLASHLIGHT,
	SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ,
	SENSOR_FEATURE_SET_REGISTER,
	SENSOR_FEATURE_GET_REGISTER,
	SENSOR_FEATURE_SET_CCT_REGISTER,
	SENSOR_FEATURE_GET_CCT_REGISTER,
	SENSOR_FEATURE_SET_ENG_REGISTER,
	SENSOR_FEATURE_GET_ENG_REGISTER,
	SENSOR_FEATURE_GET_REGISTER_DEFAULT,
	SENSOR_FEATURE_GET_CONFIG_PARA,
	SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR,
	SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA,
	SENSOR_FEATURE_GET_GROUP_COUNT,
	SENSOR_FEATURE_GET_GROUP_INFO,
	SENSOR_FEATURE_GET_ITEM_INFO,
	SENSOR_FEATURE_SET_ITEM_INFO,
	SENSOR_FEATURE_GET_ENG_INFO,
	SENSOR_FEATURE_GET_LENS_DRIVER_ID,
	SENSOR_FEATURE_SET_YUV_CMD,
	SENSOR_FEATURE_SET_VIDEO_MODE,
	SENSOR_FEATURE_SET_TARGET_FRAME_RATE,
	SENSOR_FEATURE_SET_CALIBRATION_DATA,
	SENSOR_FEATURE_SET_SENSOR_SYNC,
	SENSOR_FEATURE_INITIALIZE_AF,
	SENSOR_FEATURE_CONSTANT_AF,
	SENSOR_FEATURE_INFINITY_AF,
	SENSOR_FEATURE_MOVE_FOCUS_LENS,
	SENSOR_FEATURE_GET_AF_STATUS,
	SENSOR_FEATURE_GET_AE_STATUS,
	SENSOR_FEATURE_GET_AWB_STATUS,
	SENSOR_FEATURE_GET_AF_INF,
	SENSOR_FEATURE_GET_AF_MACRO,
	SENSOR_FEATURE_CHECK_SENSOR_ID,
	SENSOR_FEATURE_SET_AUTO_FLICKER_MODE,
	SENSOR_FEATURE_SET_TEST_PATTERN,
	SENSOR_FEATURE_SET_SOFTWARE_PWDN,
	SENSOR_FEATURE_SINGLE_FOCUS_MODE,
	SENSOR_FEATURE_CANCEL_AF,
	SENSOR_FEATURE_SET_AF_WINDOW,
	SENSOR_FEATURE_GET_EV_AWB_REF,
	SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN,
	SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS,
	SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS,
	SENSOR_FEATURE_SET_AE_WINDOW,
	SENSOR_FEATURE_GET_EXIF_INFO,
	SENSOR_FEATURE_GET_DELAY_INFO,
	SENSOR_FEATURE_SET_SLAVE_I2C_ID,
	SENSOR_FEATURE_SUSPEND,
	SENSOR_FEATURE_RESUME,
	SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO,
	SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO,
	SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO,
	SENSOR_FEATURE_AUTOTEST_CMD,
	SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE,
	SENSOR_FEATURE_GET_TEMPERATURE_VALUE,
	SENSOR_FEATURE_GET_SENSOR_CURRENT_TEMPERATURE,
	SENSOR_FEATURE_GET_AE_FLASHLIGHT_INFO,
	SENSOR_FEATURE_GET_TRIGGER_FLASHLIGHT_INFO, /* Trigger flashlight or not */
	SENSOR_FEATURE_SET_YUV_3A_CMD,
	SENSOR_FEATURE_SET_N3D_I2C_STREAM_REGDATA,
	SENSOR_FEATURE_SET_N3D_STOP_STREAMING,
	SENSOR_FEATURE_SET_N3D_START_STREAMING,
	SENSOR_FEATURE_GET_SENSOR_N3D_STREAM_TO_VSYNC_TIME,
	SENSOR_FEATURE_SET_ESHUTTER_GAIN,
	SENSOR_FEATURE_SET_OB_LOCK,
	SENSOR_FEATURE_SET_SENSOR_OTP_AWB_CMD,
	SENSOR_FEATURE_SET_SENSOR_OTP_LSC_CMD,
	SENSOR_FEATURE_GET_YUV_CAPTURE_OUTPUT_JPEG,
	SENSOR_FEATURE_SET_YUV_JPEG_PARA,
	SENSOR_FEATURE_GET_YUV_JPEG_INFO,
	SENSOR_FEATURE_SET_FRAMERATE,
	SENSOR_FEATURE_SET_HDR,
	SENSOR_FEATURE_GET_CROP_INFO,
	SENSOR_FEATURE_GET_VC_INFO,
	SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN,
	SENSOR_FEATURE_SET_AWB_GAIN,
	SENSOR_FEATURE_SET_MIN_MAX_FPS,
	SENSOR_FEATURE_GET_PDAF_INFO,
	SENSOR_FEATURE_GET_PDAF_DATA,
	SENSOR_FEATURE_SET_PDFOCUS_AREA,
	SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY,
	SENSOR_FEATURE_DEBUG_IMGSENSOR,
	SENSOR_FEATURE_SET_HDR_SHUTTER,
	SENSOR_FEATURE_SET_ISO,
	SENSOR_FEATURE_SET_PDAF,
	SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,
	SENSOR_FEATURE_SET_SHUTTER_BUF_MODE,
	SENSOR_FEATURE_SET_GAIN_BUF_MODE,
	SENSOR_FEATURE_SET_I2C_BUF_MODE_EN,
	SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY,
	SENSOR_FEATURE_CHECK_SENSOR_OTP,
	SENSOR_FEATURE_MAX
} ACDK_SENSOR_FEATURE_ENUM;

typedef enum {
	SENSOR_AF_IDLE = 0,
	SENSOR_AF_FOCUSING,
	SENSOR_AF_FOCUSED,
	SENSOR_AF_ERROR,
	SENSOR_AF_SCENE_DETECTING,
	SENSOR_AF_STATUS_MAX
} ACDK_SENSOR_AF_STATUS_ENUM;

typedef enum {
	SENSOR_AE_IDLE = 0,
	SENSOR_AE_EXPOSING,
	SENSOR_AE_EXPOSED,
	SENSOR_AE_ERROR,
	SENSOR_AE_LOCKED
} ACDK_SENSOR_AE_STATUS_ENUM;

typedef enum {
	SENSOR_AWB_IDLE = 0,
	SENSOR_AWB_BALANCING,
	SENSOR_AWB_BALANCED,
	SENSOR_AWB_ERROR,
	SENSOR_AWB_LOCKED
} ACDK_SENSOR_AWB_STATUS_ENUM;



typedef enum {
	SENSOR_INTERFACE_TYPE_PARALLEL = 0,
	SENSOR_INTERFACE_TYPE_MIPI,
	SENSOR_INTERFACE_TYPE_SERIAL,   /* add for 72 ATV using serial interface */
	SENSOR_INTERFACE_TYPE_MAX
} ACDK_SENSOR_INTERFACE_TYPE_ENUM;

typedef enum {
	SENSOR_OUTPUT_FORMAT_RAW_B = 0,
	SENSOR_OUTPUT_FORMAT_RAW_Gb,
	SENSOR_OUTPUT_FORMAT_RAW_Gr,
	SENSOR_OUTPUT_FORMAT_RAW_R,
	SENSOR_OUTPUT_FORMAT_UYVY,
	SENSOR_OUTPUT_FORMAT_VYUY,
	SENSOR_OUTPUT_FORMAT_YUYV,
	SENSOR_OUTPUT_FORMAT_YVYU,
	SENSOR_OUTPUT_FORMAT_CbYCrY,
	SENSOR_OUTPUT_FORMAT_CrYCbY,
	SENSOR_OUTPUT_FORMAT_YCbYCr,
	SENSOR_OUTPUT_FORMAT_YCrYCb,
	SENSOR_OUTPUT_FORMAT_RAW8_B,
	SENSOR_OUTPUT_FORMAT_RAW8_Gb,
	SENSOR_OUTPUT_FORMAT_RAW8_Gr,
	SENSOR_OUTPUT_FORMAT_RAW8_R,
	SENSOR_OUTPUT_FORMAT_RAW_RWB_B,
	SENSOR_OUTPUT_FORMAT_RAW_RWB_Wb,
	SENSOR_OUTPUT_FORMAT_RAW_RWB_Wr,
	SENSOR_OUTPUT_FORMAT_RAW_RWB_R,
	SENSOR_OUTPUT_FORMAT_RAW_MONO
} ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM;

typedef enum {
	SENSOR_MIPI_1_LANE = 0,
	SENSOR_MIPI_2_LANE,
	SENSOR_MIPI_3_LANE,
	SENSOR_MIPI_4_LANE
} ACDK_SENSOR_MIPI_LANE_NUMBER_ENUM;

typedef enum {
	SCAM_1_DATA_CHANNEL = 0,
	SCAM_2_DATA_CHANNEL,
	SCAM_3_DATA_CHANNEL,
	SCAM_4_DATA_CHANNEL,
} SENSOR_SCAM_DATA_CHANNEL_ENUM;

typedef enum {
	MIPI_OPHY_NCSI2 = 0,
	MIPI_OPHY_CSI2  = 1,
	MIPI_CPHY       = 2,
} SENSOR_MIPI_TYPE_ENUM;

typedef enum {
	MIPI_SETTLEDELAY_AUTO = 0,
	MIPI_SETTLEDELAY_MANUAL = 1,
} SENSOR_SETTLEDELAY_MODE_ENUM;

typedef enum {
	COMP8_NONE  = 0,
	COMP8_DI_30 = 0x30,
	COMP8_DI_31 = 0x31,
	COMP8_DI_32 = 0x32,
	COMP8_DI_33 = 0x33,
	COMP8_DI_34 = 0x34,
	COMP8_DI_35 = 0x35,
	COMP8_DI_36 = 0x36,
	COMP8_DI_37 = 0x37,
	COMP8_DI_2A = 0x2A,
} SENSOR_DPCM_TYPE_ENUM;

typedef struct {
	MUINT16 SensorPreviewWidth;
	MUINT16 SensorPreviewHeight;
	MUINT16 SensorFullWidth;
	MUINT16 SensorFullHeight;
	MUINT16 SensorVideoWidth;
	MUINT16 SensorVideoHeight;
	MUINT16 SensorHighSpeedVideoWidth;
	MUINT16 SensorHighSpeedVideoHeight;
	MUINT16 SensorSlimVideoWidth;
	MUINT16 SensorSlimVideoHeight;
	MUINT16 SensorCustom1Width;
	MUINT16 SensorCustom1Height;
	MUINT16 SensorCustom2Width;
	MUINT16 SensorCustom2Height;
	MUINT16 SensorCustom3Width;
	MUINT16 SensorCustom3Height;
	MUINT16 SensorCustom4Width;
	MUINT16 SensorCustom4Height;
	MUINT16 SensorCustom5Width;
	MUINT16 SensorCustom5Height;
	MUINT16 Sensor3DPreviewWidth;
	MUINT16 Sensor3DPreviewHeight;
	MUINT16 Sensor3DFullWidth;
	MUINT16 Sensor3DFullHeight;
	MUINT16 Sensor3DVideoWidth;
	MUINT16 Sensor3DVideoHeight;
	MUINT16 SensorEffectivePreviewWidth;
	MUINT16 SensorEffectivePreviewHeight;
	MUINT16 SensorEffectiveFullWidth;
	MUINT16 SensorEffectiveFullHeight;
	MUINT16 SensorEffectiveVideoWidth;
	MUINT16 SensorEffectiveVideoHeight;
	MUINT16 SensorEffectiveHighSpeedVideoWidth;
	MUINT16 SensorEffectiveHighSpeedVideoHeight;
	MUINT16 SensorEffective3DPreviewWidth;
	MUINT16 SensorEffective3DPreviewHeight;
	MUINT16 SensorEffective3DFullWidth;
	MUINT16 SensorEffective3DFullHeight;
	MUINT16 SensorEffective3DVideoWidth;
	MUINT16 SensorEffective3DVideoHeight;
	MUINT16 SensorPreviewWidthOffset;   /* from effective width to output width */
	MUINT16 SensorPreviewHeightOffset;  /* from effective height to output height */
	MUINT16 SensorFullWidthOffset;  /* from effective width to output width */
	MUINT16 SensorFullHeightOffset; /* from effective height to output height */
	MUINT16 SensorVideoWidthOffset; /* from effective width to output width */
	MUINT16 SensorVideoHeightOffset;    /* from effective height to output height */
	MUINT16 SensorHighSpeedVideoWidthOffset;    /* from effective width to output width */
	MUINT16 SensorHighSpeedVideoHeightOffset;   /* from effective height to output height */
	MUINT16 Sensor3DPreviewWidthOffset; /* from effective width to output width */
	MUINT16 Sensor3DPreviewHeightOffset;    /* from effective height to output height */
	MUINT16 Sensor3DFullWidthOffset;    /* from effective width to output width */
	MUINT16 Sensor3DFullHeightOffset;   /* from effective height to output height */
	MUINT16 Sensor3DVideoWidthOffset;   /* from effective width to output width */
	MUINT16 Sensor3DVideoHeightOffset;  /* from effective height to output height */
} ACDK_SENSOR_RESOLUTION_INFO_STRUCT, *PACDK_SENSOR_RESOLUTION_INFO_STRUCT;


typedef struct {
	MUINT16 SensorPreviewResolutionX;
	MUINT16 SensorPreviewResolutionY;
	MUINT16 SensorFullResolutionX;
	MUINT16 SensorFullResolutionY;
	MUINT8 SensorClockFreq; /* MHz */
	MUINT8 SensorCameraPreviewFrameRate;
	MUINT8 SensorVideoFrameRate;
	MUINT8 SensorStillCaptureFrameRate;
	MUINT8 SensorWebCamCaptureFrameRate;
	MUINT8 SensorClockPolarity; /* SENSOR_CLOCK_POLARITY_HIGH/SENSOR_CLOCK_POLARITY_Low */
	MUINT8 SensorClockFallingPolarity;
	MUINT8 SensorClockRisingCount;  /* 0..15 */
	MUINT8 SensorClockFallingCount; /* 0..15 */
	MUINT8 SensorClockDividCount;   /* 0..15 */
	MUINT8 SensorPixelClockCount;   /* 0..15 */
	MUINT8 SensorDataLatchCount;    /* 0..15 */
	MUINT8 SensorHsyncPolarity;
	MUINT8 SensorVsyncPolarity;
	MUINT8 SensorInterruptDelayLines;
	MINT32 SensorResetActiveHigh;
	MUINT32 SensorResetDelayCount;
	ACDK_SENSOR_INTERFACE_TYPE_ENUM SensroInterfaceType;
	ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM SensorOutputDataFormat;
	ACDK_SENSOR_MIPI_LANE_NUMBER_ENUM SensorMIPILaneNumber;
	MUINT32 CaptureDelayFrame;
	MUINT32 PreviewDelayFrame;
	MUINT32 VideoDelayFrame;
	MUINT32 HighSpeedVideoDelayFrame;
	MUINT32 SlimVideoDelayFrame;
	MUINT32 YUVAwbDelayFrame;
	MUINT32 YUVEffectDelayFrame;
	MUINT32 Custom1DelayFrame;
	MUINT32 Custom2DelayFrame;
	MUINT32 Custom3DelayFrame;
	MUINT32 Custom4DelayFrame;
	MUINT32 Custom5DelayFrame;
	MUINT16 SensorGrabStartX;
	MUINT16 SensorGrabStartY;
	MUINT16 SensorDrivingCurrent;
	MUINT8 SensorMasterClockSwitch;
	MUINT8 AEShutDelayFrame;    /* The frame of setting shutter default 0 for TG int */
	MUINT8 AESensorGainDelayFrame;  /* The frame of setting sensor gain */
	MUINT8 AEISPGainDelayFrame;
	MUINT8 FrameTimeDelayFrame; /* The delay frame of setting frame length  */
	MUINT8 MIPIDataLowPwr2HighSpeedTermDelayCount;
	MUINT8 MIPIDataLowPwr2HighSpeedSettleDelayCount;
	MUINT8 MIPICLKLowPwr2HighSpeedTermDelayCount;
	MUINT8 SensorWidthSampling;
	MUINT8 SensorHightSampling;
	MUINT8 SensorPacketECCOrder;
	SENSOR_MIPI_TYPE_ENUM MIPIsensorType;
	MUINT8 SensorCaptureOutputJPEG; /* JPEG file or not? */
	MUINT8 SensorModeNum;
	MUINT8 IHDR_Support;
	MUINT16 IHDR_LE_FirstLine;
	MUINT8 ZHDR_Mode;
	SENSOR_SETTLEDELAY_MODE_ENUM SettleDelayMode;
	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode(Full),
	 * 3:PDAF VC mode(Binning), 4: PDAF DualPD Raw Data mode, 5: PDAF DualPD VC mode
         */
	MUINT8 PDAF_Support;
	MUINT8 HDR_Support;/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
	MUINT8 DPCM_INFO;
	MUINT8 PerFrameCTL_Support;
	SENSOR_SCAM_DATA_CHANNEL_ENUM SCAM_DataNumber;
	MUINT8 SCAM_DDR_En;
	MUINT8 SCAM_CLK_INV;
	MUINT8 SCAM_DEFAULT_DELAY;
	MUINT8 SCAM_CRC_En;
	MUINT8 SCAM_SOF_src;
	MUINT32 SCAM_Timout_Cali;
	MUINT32 SensorMIPIDeskew;
	MUINT16 SensorHorFOV;
	MUINT16 SensorVerFOV;
} ACDK_SENSOR_INFO_STRUCT, *PACDK_SENSOR_INFO_STRUCT;

typedef struct {
	MUINT16 SensorPreviewResolutionX;
	MUINT16 SensorPreviewResolutionY;
	MUINT16 SensorFullResolutionX;
	MUINT16 SensorFullResolutionY;
	MUINT8 SensorClockFreq; /* MHz */
	MUINT8 SensorCameraPreviewFrameRate;
	MUINT8 SensorVideoFrameRate;
	MUINT8 SensorStillCaptureFrameRate;
	MUINT8 SensorWebCamCaptureFrameRate;
	MUINT8 SensorClockPolarity; /* SENSOR_CLOCK_POLARITY_HIGH/SENSOR_CLOCK_POLARITY_Low */
	MUINT8 SensorClockFallingPolarity;
	MUINT8 SensorClockRisingCount;  /* 0..15 */
	MUINT8 SensorClockFallingCount; /* 0..15 */
	MUINT8 SensorClockDividCount;   /* 0..15 */
	MUINT8 SensorPixelClockCount;   /* 0..15 */
	MUINT8 SensorDataLatchCount;    /* 0..15 */
	MUINT8 SensorHsyncPolarity;
	MUINT8 SensorVsyncPolarity;
	MUINT8 SensorInterruptDelayLines;
	MINT32 SensorResetActiveHigh;
	MUINT32 SensorResetDelayCount;
	ACDK_SENSOR_INTERFACE_TYPE_ENUM SensroInterfaceType;
	ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM SensorOutputDataFormat;
	ACDK_SENSOR_MIPI_LANE_NUMBER_ENUM SensorMIPILaneNumber;
	MUINT32 CaptureDelayFrame;
	MUINT32 PreviewDelayFrame;
	MUINT32 VideoDelayFrame;
	MUINT32 HighSpeedVideoDelayFrame;
	MUINT32 SlimVideoDelayFrame;
	MUINT32 YUVAwbDelayFrame;
	MUINT32 YUVEffectDelayFrame;
	MUINT32 Custom1DelayFrame;
	MUINT32 Custom2DelayFrame;
	MUINT32 Custom3DelayFrame;
	MUINT32 Custom4DelayFrame;
	MUINT32 Custom5DelayFrame;
	MUINT16 SensorGrabStartX_PRV;
	MUINT16 SensorGrabStartY_PRV;
	MUINT16 SensorGrabStartX_CAP;
	MUINT16 SensorGrabStartY_CAP;
	MUINT16 SensorGrabStartX_VD;
	MUINT16 SensorGrabStartY_VD;
	MUINT16 SensorGrabStartX_VD1;
	MUINT16 SensorGrabStartY_VD1;
	MUINT16 SensorGrabStartX_VD2;
	MUINT16 SensorGrabStartY_VD2;
	MUINT16 SensorGrabStartX_CST1;
	MUINT16 SensorGrabStartY_CST1;
	MUINT16 SensorGrabStartX_CST2;
	MUINT16 SensorGrabStartY_CST2;
	MUINT16 SensorGrabStartX_CST3;
	MUINT16 SensorGrabStartY_CST3;
	MUINT16 SensorGrabStartX_CST4;
	MUINT16 SensorGrabStartY_CST4;
	MUINT16 SensorGrabStartX_CST5;
	MUINT16 SensorGrabStartY_CST5;
	MUINT16 SensorDrivingCurrent;
	MUINT8 SensorMasterClockSwitch;
	MUINT8 AEShutDelayFrame;    /* The frame of setting shutter default 0 for TG int */
	MUINT8 AESensorGainDelayFrame;  /* The frame of setting sensor gain */
	MUINT8 AEISPGainDelayFrame;
	MUINT8 FrameTimeDelayFrame; /* The delay frame of setting frame length  */
	MUINT8 MIPIDataLowPwr2HighSpeedTermDelayCount;
	MUINT8 MIPIDataLowPwr2HighSpeedSettleDelayCount;
	MUINT8 MIPIDataLowPwr2HSSettleDelayM0;
	MUINT8 MIPIDataLowPwr2HSSettleDelayM1;
	MUINT8 MIPIDataLowPwr2HSSettleDelayM2;
	MUINT8 MIPIDataLowPwr2HSSettleDelayM3;
	MUINT8 MIPIDataLowPwr2HSSettleDelayM4;
	MUINT8 MIPICLKLowPwr2HighSpeedTermDelayCount;
	MUINT8 SensorWidthSampling;
	MUINT8 SensorHightSampling;
	MUINT8 SensorPacketECCOrder;
	SENSOR_MIPI_TYPE_ENUM MIPIsensorType;
	MUINT8 SensorCaptureOutputJPEG; /* JPEG file or not? */
	MUINT8 SensorModeNum;
	MUINT8 IHDR_Support;
	MUINT16 IHDR_LE_FirstLine;
	MUINT8 ZHDR_Mode;
	SENSOR_SETTLEDELAY_MODE_ENUM SettleDelayMode;
	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode(Full),
	3:PDAF VC mode(Binning), 4: PDAF DualPD Raw Data mode, 5: PDAF DualPD VC mode
	*/
	MUINT8 PDAF_Support;
	MUINT8 HDR_Support;/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
	MUINT8 DPCM_INFO;
	MUINT8 IMGSENSOR_DPCM_TYPE_PRE;
	MUINT8 IMGSENSOR_DPCM_TYPE_CAP;
	MUINT8 IMGSENSOR_DPCM_TYPE_VD;
	MUINT8 IMGSENSOR_DPCM_TYPE_VD1;
	MUINT8 IMGSENSOR_DPCM_TYPE_VD2;
	MUINT8 PerFrameCTL_Support;
	SENSOR_SCAM_DATA_CHANNEL_ENUM SCAM_DataNumber;
	MUINT8 SCAM_DDR_En;
	MUINT8 SCAM_CLK_INV;
	MUINT8 SCAM_DEFAULT_DELAY;
	MUINT8 SCAM_CRC_En;
	MUINT8 SCAM_SOF_src;
	MUINT32 SCAM_Timout_Cali;
	MUINT32 SensorMIPIDeskew;
	MUINT16 SensorHorFOV;
	MUINT16 SensorVerFOV;
} ACDK_SENSOR_INFO2_STRUCT, *PACDK_SENSOR_INFO2_STRUCT;


typedef enum {
	ACDK_CCT_REG_ISP = 0,
	ACDK_CCT_REG_CMOS,
	ACDK_CCT_REG_CCD
} ACDK_CCT_REG_TYPE_ENUM;


/* R/W ISP/Sensor Register */
typedef struct {
	ACDK_CCT_REG_TYPE_ENUM Type;
	MUINT32 RegAddr;
	MUINT32 RegData;
} ACDK_CCT_REG_RW_STRUCT, *PACDK_CCT_REG_RW_STRUCT;

typedef struct {
	ACDK_CCT_REG_TYPE_ENUM Type;    /* ISP, CMOS_SENSOR, CCD_SENSOR */
	MUINT32 DeviceId;
	ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM StartPixelBayerPtn;
	MUINT16 GrabXOffset;
	MUINT16 GrabYOffset;
} ACDK_CCT_SENSOR_INFO_STRUCT, *PACDK_CCT_SENSOR_INFO_STRUCT;



typedef enum {
	CMOS_SENSOR = 0,
	CCD_SENSOR
} SENSOR_TYPE_ENUM;

typedef struct {
	MUINT16 SensorId;
	SENSOR_TYPE_ENUM SensorType;
	ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM SensorOutputDataFormat;
} ACDK_SENSOR_ENG_INFO_STRUCT;

typedef struct {
	MUINT32 RegAddr;
	MUINT32 RegData;
} ACDK_SENSOR_REG_INFO_STRUCT;

typedef struct {
	MUINT8  debugStruct[32];
	MUINT8  debugSubstruct[32];
	MUINT32 isGet;              /* 1 means get, 0 means set */
	MUINT32 value;
} ACDK_SENSOR_DBG_IMGSENSOR_STRUCT;

typedef struct {
	MUINT32 GroupIdx;
	MUINT32 ItemCount;
	MUINT8 *GroupNamePtr;
} ACDK_SENSOR_GROUP_INFO_STRUCT;

typedef struct {
	MUINT32 GroupIdx;
	MUINT32 ItemIdx;
	MUINT8 ItemNamePtr[50]; /* item name */
	MUINT32 ItemValue;  /* item value */
	MINT32 IsTrueFalse; /* is this item for enable/disable functions */
	MINT32 IsReadOnly;  /* is this item read only */
	MINT32 IsNeedRestart;   /* after set this item need restart */
	MUINT32 Min;        /* min value of item value */
	MUINT32 Max;        /* max value of item value */
} ACDK_SENSOR_ITEM_INFO_STRUCT;

typedef enum {
	ACDK_SENSOR_IMAGE_NORMAL = 0,
	ACDK_SENSOR_IMAGE_H_MIRROR,
	ACDK_SENSOR_IMAGE_V_MIRROR,
	ACDK_SENSOR_IMAGE_HV_MIRROR
} ACDK_SENSOR_IMAGE_MIRROR_ENUM;

typedef enum {
	ACDK_SENSOR_OPERATION_MODE_CAMERA_PREVIEW = 0,
	ACDK_SENSOR_OPERATION_MODE_VIDEO,
	ACDK_SENSOR_OPERATION_MODE_STILL_CAPTURE,
	ACDK_SENSOR_OPERATION_MODE_HIGHSP_VIDEO1,
	ACDK_SENSOR_OPERATION_MODE_HIGHSP_VIDEO2,
	ACDK_SENSOR_OPERATION_MODE_WEB_CAPTURE,
	ACDK_SENSOR_OPERATION_MODE_CUSTOM1,
	ACDK_SENSOR_OPERATION_MODE_CUSTOM2,
	ACDK_SENSOR_OPERATION_MODE_CUSTOM3,
	ACDK_SENSOR_OPERATION_MODE_CUSTOM4,
	ACDK_SENSOR_OPERATION_MODE_CUSTOM5,
	ACDK_SENSOR_OPERATION_MODE_MAX
} ACDK_SENSOR_OPERATION_MODE_ENUM;

typedef struct {
	MUINT16 GrabStartX; /* The first grabed column data of the image sensor in pixel clock count */
	MUINT16 GrabStartY; /* The first grabed row data of the image sensor in pixel clock count */
	MUINT16 ExposureWindowWidth;    /* Exposure window width of image sensor */
	MUINT16 ExposureWindowHeight;   /* Exposure window height of image sensor */
	MUINT16 ImageTargetWidth;   /* image captured width */
	MUINT16 ImageTargetHeight;  /* image captuerd height */
	MUINT16 ExposurePixel;  /* exposure window width of image sensor + dummy pixel */
	MUINT16 CurrentExposurePixel;   /* exposure window width of image sensor + dummy pixel */
	MUINT16 ExposureLine;   /* exposure window width of image sensor + dummy line */
	MUINT16 ZoomFactor; /* digital zoom factor */
} ACDK_SENSOR_EXPOSURE_WINDOW_STRUCT;

typedef struct {
	ACDK_SENSOR_IMAGE_MIRROR_ENUM SensorImageMirror;
	MINT32 EnableShutterTansfer;    /* capture only */
	MINT32 EnableFlashlightTansfer; /* flash light capture only */
	ACDK_SENSOR_OPERATION_MODE_ENUM SensorOperationMode;
	MUINT16 ImageTargetWidth;   /* image captured width */
	MUINT16 ImageTargetHeight;  /* image captuerd height */
	MUINT16 CaptureShutter; /* capture only */
	MUINT16 FlashlightDuty; /* flash light capture only */
	MUINT16 FlashlightOffset;   /* flash light capture only */
	MUINT16 FlashlightShutFactor;   /* flash light capture only */
	MUINT16 FlashlightMinShutter;
	ACDK_CAMERA_OPERATION_MODE_ENUM MetaMode;   /* capture only */
	MUINT32 DefaultPclk;    /* Sensor pixel clock(Ex:24000000) */
	MUINT32 Pixels;     /* Sensor active pixel number */
	MUINT32 Lines;      /* Sensor active line number */
	MUINT32 Shutter;    /* Sensor current shutter */
	MUINT32 FrameLines; /* valid+dummy lines for minimum shutter */
} ACDK_SENSOR_CONFIG_STRUCT;

typedef enum {
	MCLK_48MHZ_GROUP = 0x1,
	MCLK_52MHZ_GROUP = 0x2,
} ACKD_SENSOR_MCLK_ENUM;

typedef struct {
	MUINT8 on;
	ACKD_SENSOR_MCLK_ENUM freq;
} ACDK_SENSOR_MCLK_STRUCT;

typedef struct {
	MUINT16 full_w;
	MUINT16 full_h;
	MUINT16 x0_offset;
	MUINT16 y0_offset;
	MUINT16 w0_size;
	MUINT16 h0_size;
	MUINT16 scale_w;
	MUINT16 scale_h;
	MUINT16 x1_offset;
	MUINT16 y1_offset;
	MUINT16 w1_size;
	MUINT16 h1_size;
	MUINT16 x2_tg_offset;
	MUINT16 y2_tg_offset;
	MUINT16 w2_tg_size;
	MUINT16 h2_tg_size;
} SENSOR_WINSIZE_INFO_STRUCT;

typedef struct {
	MUINT32 SensorId;
	SENSOR_WINSIZE_INFO_STRUCT   SensorWinSize;
} SENSOR_WINSIZE_STRUCT, *pSENSOR_WINSIZE_STRUCT;

typedef struct {
	MUINT16 VC_Num;
	MUINT16 VC_PixelNum;
	MUINT16 ModeSelect;   /* 0: auto mode, 1:direct mode  */
	MUINT16 EXPO_Ratio;   /* 1/1, 1/2, 1/4, 1/8 */
	MUINT16 ODValue;      /* OD Value */
	MUINT16 RG_STATSMODE; /* STATS divistion mdoe 0: 16x16, 1:8x8, 2:4x4, 3:1x1*/
	MUINT16 VC0_ID;
	MUINT16 VC0_DataType;
	MUINT16 VC0_SIZEH;
	MUINT16 VC0_SIZEV;
	MUINT16 VC1_ID;
	MUINT16 VC1_DataType;
	MUINT16 VC1_SIZEH;
	MUINT16 VC1_SIZEV;
	MUINT16 VC2_ID;
	MUINT16 VC2_DataType;
	MUINT16 VC2_SIZEH;
	MUINT16 VC2_SIZEV;
	MUINT16 VC3_ID;
	MUINT16 VC3_DataType;
	MUINT16 VC3_SIZEH;
	MUINT16 VC3_SIZEV;
} SENSOR_VC_INFO_STRUCT, *pSENSOR_VC_INFO_STRUCT;

typedef struct {
	MUINT32 ABS_GAIN_GR;
	MUINT32 ABS_GAIN_R;
	MUINT32 ABS_GAIN_B;
	MUINT32 ABS_GAIN_GB;
} SET_SENSOR_AWB_GAIN, *PSET_SENSOR_AWB_GAIN;

typedef struct {
    MUINT32 SENSOR_MODE;
    MUINT32 ISO;

} SET_SENSOR_ISO, *PSET_SENSOR_ISO;

typedef struct {
	MUINT32 i4OffsetX;      /* start offset of first PD block */
	MUINT32 i4OffsetY;
	MUINT32 i4PitchX;       /* PD block pitch */
	MUINT32 i4PitchY;
	MUINT32 i4PairNum;      /* PD pair num in one block */
	MUINT32 i4SubBlkW;      /* sub block width (one PD pair in one sub block) */
	MUINT32 i4SubBlkH;		/* sub block height */
	MUINT32 i4PosL[16][2];	/* left pd pixel position in one block*/
	MUINT32 i4PosR[16][2];	/* right pd pixel position in one block*/
	MUINT32 iMirrorFlip;	/* 0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR*/
	MUINT32 i4BlockNumX;
	MUINT32 i4BlockNumY;
	MUINT32 i4LeFirst;	   /* 1: 1st line is long exposure, 0: 1st line is short exposure*/
	MUINT32 i4Crop[10][2]; /* [scenario][crop] -> (xcrop, ycrop) */
} SET_PD_BLOCK_INFO_T, *PSET_PD_BLOCK_INFO_T;


typedef enum {
	SENSORIF_PARALLEL = 0,
	SENSORIF_SERIAL,
	SENSORIF_MIPI_DHY0,
	SENSORIF_MIPI_DHY1,
	SENSORIF_MIPI_DHY2
} SENSOR_PHY_TYPE_ENUM;

typedef enum {
	DATA_8BIT_FMT        = 0x0,
	DATA_10BIT_FMT       = 0x1,
	DATA_12BIT_FMT       = 0x2,
	DATA_YUV422_FMT      = 0x3,
} INDATA_FORMAT_ENUM;

typedef struct {
	MINT32 mSensorDev;
	MUINT8 GpioEnable;
	SENSOR_PHY_TYPE_ENUM SensroInterfaceType;
	INDATA_FORMAT_ENUM SensorIndataformat;
} IMGSENSOR_GPIO_STRUCT;
/*******************************************************************************
*
********************************************************************************/

#define MAXIMUM_NVRAM_CAMERA_SENSOR_FILE_SIZE_KERNEL       4096

#define NVRAM_CAMERA_SENSOR_FILE_VERSION        1



/* Sensor table */
#define MAXIMUM_SENSOR_CCT_REG_NUMBER   100
#define MAXIMUM_SENSOR_ENG_REG_NUMBER   100

typedef struct {
	MUINT32 Addr;
	MUINT32 Para;
} SENSOR_REG_STRUCT;

typedef struct {
	MUINT32 Version;
	MUINT32 SensorId;   /* ID of sensor module */
	SENSOR_REG_STRUCT SensorEngReg[MAXIMUM_SENSOR_ENG_REG_NUMBER];
	SENSOR_REG_STRUCT SensorCCTReg[MAXIMUM_SENSOR_CCT_REG_NUMBER];
	MUINT8 CameraData[MAXIMUM_NVRAM_CAMERA_SENSOR_FILE_SIZE_KERNEL / 2 - 8 - sizeof(SENSOR_REG_STRUCT) * (MAXIMUM_SENSOR_ENG_REG_NUMBER + MAXIMUM_SENSOR_CCT_REG_NUMBER)];
} NVRAM_SENSOR_DATA_STRUCT, *PNVRAM_SENSOR_DATA_STRUCT;

#define MAX_SENSOR_CAL_SIZE_KERNEL     (1024)   /* Byte */
#define MAX_SHADING_DATA_TBL ((MAX_SENSOR_CAL_SIZE_KERNEL-8)/4)
typedef struct {
	MUINT32 DataFormat;
	MUINT32 DataSize;
	MUINT32 ShadingData[MAX_SHADING_DATA_TBL];
} SET_SENSOR_CALIBRATION_DATA_STRUCT, *PSET_SENSOR_CALIBRATION_DATA_STRUCT;


#ifdef MTK_SUB2_IMGSENSOR
typedef struct {
	MSDK_SCENARIO_ID_ENUM ScenarioId[3];
	ACDK_SENSOR_INFO_STRUCT * pInfo[3];
	ACDK_SENSOR_CONFIG_STRUCT * pConfig[3];
} ACDK_SENSOR_GETINFO_STRUCT, *PACDK_SENSOR_GETINFO_STRUCT;

#else
typedef struct {
	MSDK_SCENARIO_ID_ENUM ScenarioId[2];
	ACDK_SENSOR_INFO_STRUCT * pInfo[2];
	ACDK_SENSOR_CONFIG_STRUCT * pConfig[2];
} ACDK_SENSOR_GETINFO_STRUCT, *PACDK_SENSOR_GETINFO_STRUCT;
#endif

typedef struct {
	MUINT32 SensorId;
	ACDK_SENSOR_INFO2_STRUCT *pInfo;
	ACDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution;
} IMAGESENSOR_GETINFO_STRUCT, *PIMAGESENSOR_GETINFO_STRUCT;

typedef struct {
	CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera;
	ACDK_SENSOR_FEATURE_ENUM FeatureId;
	MUINT8 *pFeaturePara;
	MUINT32 *pFeatureParaLen;
} ACDK_SENSOR_FEATURECONTROL_STRUCT, *PACDK_SENSOR_FEATURECONTROL_STRUCT;

typedef struct {
	CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera;
	MSDK_SCENARIO_ID_ENUM ScenarioId;
	ACDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow;
	ACDK_SENSOR_CONFIG_STRUCT *pSensorConfigData;
} ACDK_SENSOR_CONTROL_STRUCT;

#ifdef MTK_SUB2_IMGSENSOR
typedef struct {
	ACDK_SENSOR_RESOLUTION_INFO_STRUCT * pResolution[3];
} ACDK_SENSOR_PRESOLUTION_STRUCT;
#else
typedef struct {
	ACDK_SENSOR_RESOLUTION_INFO_STRUCT *pResolution[2];
} ACDK_SENSOR_PRESOLUTION_STRUCT;
#endif



typedef struct regval_list {
	MUINT32 reg_addr;
	MUINT32 value;
	MUINT32 bytes;
} REGVAL_LIST_STRUCT;

#define KDIMGSENSOR_REGVAL_LIST_MAX_NUM 256

typedef struct format_struct {
	MUINT8 *desc;
	MUINT32 pixelformat;
	/* REGVAL_LIST_STRUCT regs[KDIMGSENSOR_REGVAL_LIST_MAX_NUM]; */
	int (*pfInitCfg)(void);
} IMGSENSOR_FORMAT_STRUCT;

typedef struct {
	IMGSENSOR_FORMAT_STRUCT format;
	MUINT32 u4InClk;    /* Common part                                     //hard coded */
	MUINT32 u4OutClk;   /* Common part                                    // */
	MUINT32 u4TotalPixelPerLine;    /* By modes */
	MUINT32 u4TotalLinesPerFrame;   /* By modes and frame rate setting */
	MUINT32 u4ActualWidth;  /* By modes */
	MUINT32 u4ActualHeight; /* By modes */
	MUINT32 u4Width;    /* By modes */
	MUINT32 u4Height;   /* By modes */
	MUINT32 u4FrameTimeInus;    /* By modes and frame rate setting */
	MUINT32 u4MinFrameTimeInus; /* By modes */
	MUINT32 u4LineTimeInus; /* By modes */
	MUINT32 u4FinePixCntPerus;  /* Common part */
	MUINT32 u4MinFineTimeInus;  /* By modes */
	MUINT32 u4MaxFineTimeInus;  /* By modes */
	MUINT32 u4XStart;
	MUINT32 u4XEnd;
	MUINT32 u4YStart;
	MUINT32 u4YEnd;
} stImgSensorFormat;

#ifdef CONFIG_COMPAT
#ifdef MTK_SUB2_IMGSENSOR
/* Redifne structures for compat_ioctl */
typedef struct {
	MSDK_SCENARIO_ID_ENUM ScenarioId[3];
	compat_uptr_t pInfo[3];
	compat_uptr_t pConfig[3];
} COMPAT_ACDK_SENSOR_GETINFO_STRUCT;
#else
typedef struct {
	MSDK_SCENARIO_ID_ENUM ScenarioId[2];
	compat_uptr_t pInfo[2];
	compat_uptr_t pConfig[2];
} COMPAT_ACDK_SENSOR_GETINFO_STRUCT;

#endif

typedef struct {
	MUINT32 SensorId;
	compat_uptr_t pInfo;
	compat_uptr_t pSensorResolution;
} COMPAT_IMAGESENSOR_GETINFO_STRUCT;

typedef struct {
	CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera;
	ACDK_SENSOR_FEATURE_ENUM FeatureId;
	compat_uptr_t pFeaturePara;
	compat_uptr_t pFeatureParaLen;
} COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT;

typedef struct {
	CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera;
	MSDK_SCENARIO_ID_ENUM ScenarioId;
	compat_uptr_t pImageWindow;
	compat_uptr_t pSensorConfigData;
} COMPAT_ACDK_SENSOR_CONTROL_STRUCT;

#ifdef MTK_SUB2_IMGSENSOR
typedef struct {
	compat_uptr_t pResolution[3];
} COMPAT_ACDK_SENSOR_PRESOLUTION_STRUCT;
#else
typedef struct {
	compat_uptr_t pResolution[2];
} COMPAT_ACDK_SENSOR_PRESOLUTION_STRUCT;
#endif
#endif


/*******************************************************************************
*
********************************************************************************/
/*  */
/* adoption to winmo driver files */
/*  */

/* typedef */
/* #define kal_uint8 u8 */

/* #define MSDK_SCENARIO_ID_ENUM               ACDK_SCENARIO_ID_ENUM */
#define MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT      ACDK_SENSOR_EXPOSURE_WINDOW_STRUCT
#define MSDK_SENSOR_CONFIG_STRUCT               ACDK_SENSOR_CONFIG_STRUCT

#define MSDK_SENSOR_FEATURE_ENUM                ACDK_SENSOR_FEATURE_ENUM
#define MSDK_SENSOR_REG_INFO_STRUCT             ACDK_SENSOR_REG_INFO_STRUCT
#define MSDK_SENSOR_GROUP_INFO_STRUCT           ACDK_SENSOR_GROUP_INFO_STRUCT
#define MSDK_SENSOR_ITEM_INFO_STRUCT            ACDK_SENSOR_ITEM_INFO_STRUCT
#define MSDK_SENSOR_ENG_INFO_STRUCT             ACDK_SENSOR_ENG_INFO_STRUCT
#define MSDK_SENSOR_INFO_STRUCT                 ACDK_SENSOR_INFO_STRUCT
#define MSDK_SENSOR_RESOLUTION_INFO_STRUCT      ACDK_SENSOR_RESOLUTION_INFO_STRUCT
#define MSDK_SENSOR_DBG_IMGSENSOR_INFO_STRUCT   ACDK_SENSOR_DBG_IMGSENSOR_STRUCT

/* MSDK_SCENARIO_ID_ENUM => ACDK_SCENARIO_ID_ENUM */
/* #define MSDK_SCENARIO_ID_CAMERA_PREVIEW         ACDK_SCENARIO_ID_CAMERA_PREVIEW */
/* #define MSDK_SCENARIO_ID_VIDEO_PREVIEW          ACDK_SCENARIO_ID_VIDEO_PREVIEW */
/* #define MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG    ACDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG */
#define MSDK_SENSOR_OPERATION_MODE_VIDEO        ACDK_SENSOR_OPERATION_MODE_VIDEO
/* #define MSDK_SCENARIO_ID_CAMERA_ZSD                           ACDK_SCENARIO_ID_CAMERA_ZSD */
/* #define MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW            ACDK_SCENARIO_ID_CAMERA_3D_PREVIEW */
/* #define MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE            ACDK_SCENARIO_ID_CAMERA_3D_CAPTURE */
/* #define MSDK_SCENARIO_ID_CAMERA_3D_VIDEO              ACDK_SCENARIO_ID_CAMERA_3D_VIDEO */
/*******************************************************************************
*
********************************************************************************/

/*******************************************************************************
*
********************************************************************************/
/* for new simplifed sensor driver */
typedef struct {
	MUINT32(*SensorOpen)(void);
	MUINT32(*SensorGetInfo)(MUINT32 *pScenarioId[2], MSDK_SENSOR_INFO_STRUCT * pSensorInfo[2], MSDK_SENSOR_CONFIG_STRUCT * pSensorConfigData[2]);
	MUINT32(*SensorGetResolution)(MSDK_SENSOR_RESOLUTION_INFO_STRUCT * pSensorResolution[2]);
	MUINT32(*SensorFeatureControl)(CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera, MSDK_SENSOR_FEATURE_ENUM FeatureId, MUINT8 *pFeaturePara, MUINT32 *pFeatureParaLen);
	MUINT32(*SensorControl)(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
	MUINT32(*SensorClose)(void);
} MULTI_SENSOR_FUNCTION_STRUCT, *PMULTI_SENSOR_FUNCTION_STRUCT;

typedef struct {
	MUINT32(*SensorOpen)(void);
	MUINT32(*SensorGetInfo)(MUINT32 *pScenarioId[2], MSDK_SENSOR_INFO_STRUCT * pSensorInfo[2], MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData[2]);
	MUINT32(*SensorGetResolution)(MSDK_SENSOR_RESOLUTION_INFO_STRUCT * pSensorResolution[2]);
	MUINT32(*SensorFeatureControl)(CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera, MSDK_SENSOR_FEATURE_ENUM FeatureId, MUINT8 *pFeaturePara, MUINT32 *pFeatureParaLen);
	MUINT32(*SensorControl)(CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera, MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
	MUINT32(*SensorClose)(void);
} MULTI_SENSOR_FUNCTION_STRUCT2, *PMULTI_SENSOR_FUNCTION_STRUCT2;


typedef struct {
	MUINT32(*SensorOpen)(void);
	MUINT32(*SensorGetInfo)(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
				MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
	MUINT32(*SensorGetResolution)(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
	MUINT32(*SensorFeatureControl)(MSDK_SENSOR_FEATURE_ENUM FeatureId, MUINT8 *pFeaturePara, MUINT32 *pFeatureParaLen);
	MUINT32(*SensorControl)(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
	MUINT32(*SensorClose)(void);
#if 1 /* isp suspend resume patch */
	MSDK_SCENARIO_ID_ENUM ScenarioId;
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT imageWindow;
	MSDK_SENSOR_CONFIG_STRUCT sensorConfigData;
#endif
} SENSOR_FUNCTION_STRUCT, *PSENSOR_FUNCTION_STRUCT;

typedef struct {
	MUINT32 SensorId;
	MUINT8 drvname[32];
	MUINT32(*SensorInit)(PSENSOR_FUNCTION_STRUCT *pfFunc);
} ACDK_KD_SENSOR_INIT_FUNCTION_STRUCT, *PACDK_KD_SENSOR_INIT_FUNCTION_STRUCT;

#define KDIMGSENSOR_DUAL_SHIFT 16
#define KDIMGSENSOR_DUAL_MASK_MSB 0xFFFF0000
#define KDIMGSENSOR_DUAL_MASK_LSB 0x0000FFFF

#define KDIMGSENSOR_NOSENSOR    "non_sensor"
#ifdef MTK_SUB2_IMGSENSOR
#define KDIMGSENSOR_MAX_INVOKE_DRIVERS  (3)
#define KDIMGSENSOR_INVOKE_DRIVER_0     (0)
#define KDIMGSENSOR_INVOKE_DRIVER_1     (1)
#define KDIMGSENSOR_INVOKE_DRIVER_2     (2)
#else
#define KDIMGSENSOR_MAX_INVOKE_DRIVERS  (2)
#define KDIMGSENSOR_INVOKE_DRIVER_0     (0)
#define KDIMGSENSOR_INVOKE_DRIVER_1     (1)
#endif

/* For sensor synchronize the exposure time / sensor gain and isp gain. */
typedef struct {
	MUINT16 u2ISPNewRGain;
	MUINT16 u2ISPNewGrGain;
	MUINT16 u2ISPNewGbGain;
	MUINT16 u2ISPNewBGain;
	MUINT16 u2SensorNewExpTime;
	MUINT16 u2SensorNewGain;
	MUINT8 uSensorExpDelayFrame;
	MUINT8 uSensorGainDelayFrame;
	MUINT8 uISPGainDelayFrame;
	MUINT8 uDummy;
} ACDK_KD_SENSOR_SYNC_STRUCT, *PACDK_KD_SENSOR_SYNC_STRUCT;


typedef struct {
	MUINT16 AeRefLV05Shutter;   /* Sensor AE Shutter under Lv05 */
	MUINT16 AeRefLV13Shutter;   /* Sensor AE Shutter under Lv13 */
	MUINT16 AeRefLV05Gain;  /* Sensor AE Gain under Lv05 */
	MUINT16 AeRefLV13Gain;  /* Sensor AE Gain under Lv13 */
} SENSOR_AE_REF_STRUCT, *PSENSOR_AE_REF_STRUCT;


typedef struct {
	MUINT16 AwbRefD65Rgain; /* Sensor AWB R Gain under D65 */
	MUINT16 AwbRefD65Bgain; /* Sensor AWB B Gain under D65 */
	MUINT16 AwbRefCWFRgain; /* Sensor AWB R Gain under CWF */
	MUINT16 AwbRefCWFBgain; /* Sensor AWB B Gain under CWF */
} SENSOR_AWB_GAIN_REF_STRUCT, *PSENSOR_AWB_GAIN_REF_STRUCT;


typedef struct {
	SENSOR_AE_REF_STRUCT SensorAERef;   /* AE Ref information for ASD usage */
	SENSOR_AWB_GAIN_REF_STRUCT SensorAwbGainRef;    /* AWB Gain Ref information for ASD usage */
	MUINT32 SensorLV05LV13EVRef;    /* EV calculate  for ASD usage */
} SENSOR_AE_AWB_REF_STRUCT, *PSENSOR_AE_AWB_REF_STRUCT;



typedef struct {
	MUINT16 AeCurShutter;   /* Current Sensor AE Shutter */
	MUINT16 AeCurGain;  /* Current Sensor AE Gain */
} SENSOR_AE_CUR_STRUCT, *PSENSOR_AE_CUR_STRUCT;


typedef struct {
	MUINT16 AwbCurRgain;    /* Current Sensor AWB R Gain */
	MUINT16 AwbCurBgain;    /* Current Sensor AWB R Gain */
} SENSOR_AWB_GAIN_CUR_STRUCT, *PSENSOR_AWB_GAIN_CUR_STRUCT;


typedef struct {
	SENSOR_AE_CUR_STRUCT SensorAECur;   /* AE Current information for ASD usage */
	SENSOR_AWB_GAIN_CUR_STRUCT SensorAwbGainCur;    /* AWB Gain Current information for ASD usage */
} SENSOR_AE_AWB_CUR_STRUCT, *PSENSOR_AE_AWB_CUR_STRUCT;


typedef struct {
	MUINT32 FNumber;
	MUINT32 AEISOSpeed;
	MUINT32 AWBMode;
	MUINT32 CapExposureTime;
	MUINT32 FlashLightTimeus;
	MUINT32 RealISOValue;
} SENSOR_EXIF_INFO_STRUCT, *PSENSOR_EXIF_INFO_STRUCT;

typedef struct {
	MUINT32 InitDelay;
	MUINT32 EffectDelay;
	MUINT32 AwbDelay;
	MUINT32 AFSwitchDelayFrame;
	MUINT32 EvDelay;
	MUINT32 SatDelay;
	MUINT32 BrightDelay;
	MUINT32 ContrastDelay;
} SENSOR_DELAY_INFO_STRUCT, *PSENSOR_DELAY_INFO_STRUCT;

typedef struct {
	MUINT32 u4Fno;
	MUINT32 Exposuretime;
	MUINT32 Gain;
	MUINT32 GAIN_BASE;
} SENSOR_FLASHLIGHT_AE_INFO_STRUCT, *PSENSOR_FLASHLIGHT_AE_INFO_STRUCT;

/* multisensor driver */

typedef struct {
	MUINT32 drvIndex[KDIMGSENSOR_MAX_INVOKE_DRIVERS];   /* max 2 driver sumultaneously */
} SENSOR_DRIVER_INDEX_STRUCT, *PSENSOR_DRIVER_INDEX_STRUCT;

/* hardcode by GPIO module, should be sync with.(cust_gpio_usage.h) */
#define GPIO_CAMERA_INVALID 0xFF
/*  */
typedef enum {
	IMGSENSOR_SOCKET_POS_NONE = 0xFFFFFFFF,
	IMGSENSOR_SOCKET_POS_RIGHT = 0x1,
	IMGSENSOR_SOCKET_POS_LEFT = 0x2,
} IMGSENSOR_SOCKET_POSITION_ENUM;
/*  */
typedef enum {
	IMGSENSOR_SET_I2C_ID_STATE = 0x00,
	IMGSENSOR_SET_I2C_ID_FORCE
} IMGSENSOR_SET_I2C_ID_ENUM;



typedef enum {
	SENSOR_3A_AE_LOCK = 0,
	SENSOR_3A_AE_UNLOCK,
	SENSOR_3A_AWB_LOCK,
	SENSOR_3A_AWB_UNLOCK,
	SENSOR_3A_AE_ON,
	SENSOR_3A_AE_OFF,
	SENSOR_3A_AWB_ON,
	SENSOR_3A_AWB_OFF,
	SENSOR_3A_AF_INFINITY,
	/* SENSOR_3A_AF_LOCK, */
	/* SENSOR_3A_AF_UNLOCK, */
} ACDK_SENSOR_3A_LOCK_ENUM;


typedef struct {
	MUINT32 tgtWidth;
	MUINT32 tgtHeight;
	MUINT32 quality;

} ACDK_SENSOR_JPEG_OUTPUT_PARA;


/* for JPEG Sensor Info */
typedef struct {
	MUINT32 u4FileSize; /* For input sensor width */
	MUINT32 u4SrcW;     /* For input sensor width */
	MUINT32 u4SrcH;     /* For input sensor height */
} ACDK_SENSOR_JPEG_INFO;


#endif              /* _KD_IMGSENSOR_DATA_H */
