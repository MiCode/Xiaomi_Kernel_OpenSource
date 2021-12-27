#ifndef _CAM_OIS_LC898218_H_
#define _CAM_OIS_LC898218_H_

/**
 * @brief		LC898128 Global declaration & prototype declaration
 *
 * @author		Copyright (C) 2016, ON Semiconductor, all right reserved.
 * @author		Copyright (C) 2021 XiaoMi, Inc.
 *
 * @file		OisLc898128.h
 * @date		svn:$Date:: 2016-06-17 16:42:32 +0900#$
 * @version	svn:$Revision: 54 $
 * @attention
 **/

/* firmware version */
#define FromCodeBlockSize_07_00		10
#define UpDataCodeSize_07_00		0x0000015e
#define UpDataCodeCheckSum_07_00	0x00007a7a646642d9


/* Burst Length for updating to PMEM Max:256*/
#define BURST_LENGTH_UC 		( 20 ) // 120 Total:122Byte
/* Burst Length for updating to Flash */
#define BURST_LENGTH_FC 		( 64 ) // 64 Total: 66~67Byte


/************************************************/
/*	Command										*/
/************************************************/
	// Calibration flags
	#define		HALL_CALB_FLG					0x00008000
	#define		HALL_CALB_BIT					0x00FF00FF
	#define		GYRO_GAIN_FLG					0x00004000
	#define		CAL_ANGLE_FLG					0x00000800			// angle correct calibration
	#define		HLLN_CALB_FLG					0x00000400			// Hall linear calibration
	#define		MIXI_CALB_FLG					0x00000200			// Mixing calibration
//==============================================================================
// Calibration Data Memory Map
//==============================================================================
// Calibration Status
#define	CALIBRATION_STATUS		(  0 )
// Hall Bias/Offset
#define	HALL_BIAS_OFFSET		(  1 )	// 0:XBIAS 1:XOFFSET 2:YBIAS 3:YOFFSET
// Loop Gain Calibration
#define	LOOP_GAIN_XY			(  2 ) // [1:0]X  [3:2]Y
// Lens Center Calibration
#define	LENS_OFFSET				(  3 ) // [1:0]X  [3:2]Y
// Gyro Gain Calibration
#define	GYRO_GAIN_X				(  4 )
#define	GYRO_GAIN_Y				(  5 )
// Liniearity correction
#define LN_POS1					(  6 ) // [3:2]Y  [1:0]X
#define LN_POS2					(  7 ) // [3:2]Y  [1:0]X
#define LN_POS3					(  8 ) // [3:2]Y  [1:0]X
#define LN_POS4					(  9 ) // [3:2]Y  [1:0]X
#define LN_POS5					( 10 ) // [3:2]Y  [1:0]X
#define LN_POS6					( 11 ) // [3:2]Y  [1:0]X
#define LN_POS7					( 12 ) // [3:2]Y  [1:0]X
#define LN_STEP					( 13 ) // [3:2]Y  [1:0]X
// Gyro mixing correction
#define MIXING_X				( 14 )	// [3:2]XY [1:0]XX
#define MIXING_Y				( 15 )	// [3:2]YX [1:0]YY
#define MIXING_SFT				( 16 )	// 1:YSFT 0:XSHT
//// Gyro Offset Calibration
//#define	G_OFFSET_XY				( 18 )	// [3:2]GY offset [1:0]GX offset
//#define	G_OFFSET_Z_AX			( 19 )	// [3:2]AX offset [1:0]GZ offset
//#define	A_OFFSET_YZ				( 20 )	// [3:2]AZ offset [1:0]AY offset
// back up hall max and min
#define	HL_XMAXMIN				( 18 )	// [3:2]MAX [1:0]MIN
#define	HL_YMAXMIN				( 19)	// [3:2]MAX [1:0]MIN
// Angle correct Correction.

#define	OPTCENTER				( 29 )	// [3:2]Y [1:0]X
// include check sum
#define	MAT0_CKSM				( 31 )	// [3:2]AZ offset [1:0]AY offset

#define	FT_REPRG				( 15 )
	#define	PRDCT_WR				0x55555555
	#define	USER_WR					0xAAAAAAAA
#define	MAT2_CKSM				( 29 )
#define	CHECKCODE1				( 30 )
	#define	CHECK_CODE1				0x99756768
#define	CHECKCODE2				( 31 )
	#define	CHECK_CODE2				0x01AC28AC

#define POSITION_X_BY_AF_Flg	(  0 )
#define POSITION_X_BY_AF0		(  1 )
#define POSITION_X_BY_AF1		(  2 )
#define POSITION_X_BY_AF2		(  3 )
#define POSITION_X_BY_AF3		(  4 )
#define POSITION_X_BY_AF4		(  5 )
#define POSITION_X_BY_AF5		(  6 )
#define POSITION_X_BY_AF6		(  7 )
#define POSITION_X_BY_AF7		(  8 )
#define POSITION_X_BY_AF8		(  9 )
#define POSITION_Y_BY_AF_Flg	( 16 )
#define POSITION_Y_BY_AF0		( 17 )
#define POSITION_Y_BY_AF1		( 18 )
#define POSITION_Y_BY_AF2		( 19 )
#define POSITION_Y_BY_AF3		( 20 )
#define POSITION_Y_BY_AF4		( 21 )
#define POSITION_Y_BY_AF5		( 22 )
#define POSITION_Y_BY_AF6		( 23 )
#define POSITION_Y_BY_AF7		( 24 )
#define POSITION_Y_BY_AF8		( 25 )


//==============================================================================
//DMA
//==============================================================================
#define		HallFilterD_HXDAZ1				0x0048
#define		HallFilterD_HYDAZ1				0x0098
#define		HALL_RAM_X_COMMON				0x00D8
#define			HALL_RAM_HXOFF					0x0000 + HALL_RAM_X_COMMON
#define			HALL_RAM_HXOFF1					0x0004 + HALL_RAM_X_COMMON
#define			HALL_RAM_HXOUT0					0x0008 + HALL_RAM_X_COMMON
#define			HALL_RAM_HXOUT1					0x000C + HALL_RAM_X_COMMON
#define			HALL_RAM_SINDX0					0x0010 + HALL_RAM_X_COMMON
#define			HALL_RAM_HXLOP					0x0014 + HALL_RAM_X_COMMON
#define			HALL_RAM_SINDX1					0x0018 + HALL_RAM_X_COMMON
#define			HALL_RAM_HALL_X_OUT				0x001C + HALL_RAM_X_COMMON
#define			XMoveAvg_D2						0x0030 + HALL_RAM_X_COMMON
#define			HALL_RAM_HXOUT2					0x0038 + HALL_RAM_X_COMMON
#define			HALL_RAM_HXOUT3					0x0048 + HALL_RAM_X_COMMON
#define		HALL_RAM_HALL_SwitchX			0x0124

#define		HALL_RAM_Y_COMMON				0x0128
#define			HALL_RAM_HYOFF					0x0000 + HALL_RAM_Y_COMMON
#define			HALL_RAM_HYOFF1					0x0004 + HALL_RAM_Y_COMMON
#define			HALL_RAM_HYOUT0					0x0008 + HALL_RAM_Y_COMMON
#define			HALL_RAM_HYOUT1					0x000C + HALL_RAM_Y_COMMON
#define			HALL_RAM_SINDY0					0x0010 + HALL_RAM_Y_COMMON
#define			HALL_RAM_HYLOP					0x0014 + HALL_RAM_Y_COMMON
#define			HALL_RAM_SINDY1					0x0018 + HALL_RAM_Y_COMMON
#define			HALL_RAM_HALL_Y_OUT				0x001C + HALL_RAM_Y_COMMON
#define			YMoveAvg_D2						0x0030 + HALL_RAM_Y_COMMON
#define			HALL_RAM_HYOUT2					0x0038 + HALL_RAM_Y_COMMON
#define			HALL_RAM_HYOUT3					0x0048 + HALL_RAM_Y_COMMON
#define		HALL_RAM_HALL_SwitchY			0x0174


#define		HALL_RAM_COMMON					0x0178
				//  HallFilterDelay.h HALL_RAM_COMMON_t
#define			HALL_RAM_HXIDAT					0x0000 + HALL_RAM_COMMON
#define			HALL_RAM_HYIDAT					0x0004 + HALL_RAM_COMMON
#define			HALL_RAM_GYROX_OUT				0x0008 + HALL_RAM_COMMON
#define			HALL_RAM_GYROY_OUT				0x000C + HALL_RAM_COMMON

#define		GyroFilterDelayX_GXH1Z2			0x019C
#define		GyroFilterDelayY_GYH1Z2			0x01C4

#define		GYRO_RAM_X						0x01D8
				// GyroFilterDelay.h GYRO_RAM_t
#define			GYRO_RAM_GYROX_OFFSET			0x0000 + GYRO_RAM_X
#define			GYRO_RAM_GX2X4XF_IN				0x0004 + GYRO_RAM_X
#define			GYRO_RAM_GX2X4XF_OUT			0x0008 + GYRO_RAM_X
#define			GYRO_RAM_GXFAST					0x000C + GYRO_RAM_X
#define			GYRO_RAM_GXSLOW					0x0010 + GYRO_RAM_X
#define			GYRO_RAM_GYROX_G1OUT			0x0014 + GYRO_RAM_X
#define			GYRO_RAM_GYROX_G2OUT			0x0018 + GYRO_RAM_X
#define			GYRO_RAM_GYROX_G3OUT			0x001C + GYRO_RAM_X
#define			GYRO_RAM_GYROX_OUT				0x0020 + GYRO_RAM_X

#define		GYRO_RAM_Y						0x01FC
				// GyroFilterDelay.h GYRO_RAM_t
#define			GYRO_RAM_GYROY_OFFSET			0x0000 + GYRO_RAM_Y
#define			GYRO_RAM_GY2X4XF_IN				0x0004 + GYRO_RAM_Y
#define			GYRO_RAM_GY2X4XF_OUT			0x0008 + GYRO_RAM_Y
#define			GYRO_RAM_GYFAST					0x000C + GYRO_RAM_Y
#define			GYRO_RAM_GYSLOW					0x0010 + GYRO_RAM_Y
#define			GYRO_RAM_GYROY_G1OUT			0x0014 + GYRO_RAM_Y
#define			GYRO_RAM_GYROY_G2OUT			0x0018 + GYRO_RAM_Y
#define			GYRO_RAM_GYROY_G3OUT			0x001C + GYRO_RAM_Y
#define			GYRO_RAM_GYROY_OUT				0x0020 + GYRO_RAM_Y

#define		GYRO_RAM_COMMON					0x0220
				// GyroFilterDelay.h GYRO_RAM_COMMON_t
#define			GYRO_RAM_GX_ADIDAT				0x0000 + GYRO_RAM_COMMON
#define			GYRO_RAM_GY_ADIDAT				0x0004 + GYRO_RAM_COMMON
#define			GYRO_RAM_SINDX					0x0008 + GYRO_RAM_COMMON
#define			GYRO_RAM_SINDY					0x000C + GYRO_RAM_COMMON
#define			GYRO_RAM_GXLENSZ				0x0010 + GYRO_RAM_COMMON
#define			GYRO_RAM_GYLENSZ				0x0014 + GYRO_RAM_COMMON
#define			GYRO_RAM_GXOX_OUT				0x0018 + GYRO_RAM_COMMON
#define			GYRO_RAM_GYOX_OUT				0x001C + GYRO_RAM_COMMON
#define			GYRO_RAM_GXOFFZ					0x0020 + GYRO_RAM_COMMON
#define			GYRO_RAM_GYOFFZ					0x0024 + GYRO_RAM_COMMON
#define			GYRO_RAM_LIMITX					0x0028 + GYRO_RAM_COMMON
#define			GYRO_RAM_LIMITY					0x002C + GYRO_RAM_COMMON
#define			GYRO_RAM_GYROX_AFCnt			0x0030 + GYRO_RAM_COMMON
#define			GYRO_RAM_GYROY_AFCnt			0x0034 + GYRO_RAM_COMMON
#define			GYRO_RAM_GYRO_Switch			0x0038 + GYRO_RAM_COMMON		// 1Byte

#define		StMeasureFunc					0x0278
				// MeasureFilter.h	MeasureFunction_Type
#define			StMeasFunc_SiSampleNum			0x0000 + StMeasureFunc					//
#define			StMeasFunc_SiSampleMax			0x0004 + StMeasureFunc			//

#define		StMeasureFunc_MFA				0x0280
#define			StMeasFunc_MFA_SiMax1			0x0000 + StMeasureFunc_MFA
#define			StMeasFunc_MFA_SiMin1			0x0004 + StMeasureFunc_MFA
#define			StMeasFunc_MFA_UiAmp1			0x0008 + StMeasureFunc_MFA
#define			StMeasFunc_MFA_UiDUMMY1			0x000C + StMeasureFunc_MFA
#define			StMeasFunc_MFA_LLiIntegral1		0x0010 + StMeasureFunc_MFA
#define			StMeasFunc_MFA_LLiAbsInteg1		0x0018 + StMeasureFunc_MFA
#define			StMeasFunc_MFA_PiMeasureRam1	0x0020 + StMeasureFunc_MFA

#define		StMeasureFunc_MFB				0x02A8
#define			StMeasFunc_MFB_SiMax2			0x0000 + StMeasureFunc_MFB
#define			StMeasFunc_MFB_SiMin2			0x0004 + StMeasureFunc_MFB
#define			StMeasFunc_MFB_UiAmp2			0x0008 + StMeasureFunc_MFB
#define			StMeasFunc_MFB_UiDUMMY1			0x000C + StMeasureFunc_MFB
#define			StMeasFunc_MFB_LLiIntegral2		0x0010 + StMeasureFunc_MFB
#define			StMeasFunc_MFB_LLiAbsInteg2		0x0018 + StMeasureFunc_MFB
#define			StMeasFunc_MFB_PiMeasureRam2	0x0020 + StMeasureFunc_MFB

#define		MeasureFilterA_Delay			0x02D0
				// MeasureFilter.h	MeasureFilter_Delay_Type
#define			MeasureFilterA_Delay_z11		0x0000 + MeasureFilterA_Delay
#define			MeasureFilterA_Delay_z12		0x0004 + MeasureFilterA_Delay
#define			MeasureFilterA_Delay_z21		0x0008 + MeasureFilterA_Delay
#define			MeasureFilterA_Delay_z22		0x000C + MeasureFilterA_Delay

#define		MeasureFilterB_Delay			0x02E0
				// MeasureFilter.h	MeasureFilter_Delay_Type
#define			MeasureFilterB_Delay_z11		0x0000 + MeasureFilterB_Delay
#define			MeasureFilterB_Delay_z12		0x0004 + MeasureFilterB_Delay
#define			MeasureFilterB_Delay_z21		0x0008 + MeasureFilterB_Delay
#define			MeasureFilterB_Delay_z22		0x000C + MeasureFilterB_Delay

#define		SinWaveC						0x02F0
#define			SinWaveC_Pt						0x0000 + SinWaveC
#define			SinWaveC_Regsiter				0x0004 + SinWaveC
//#define			SinWaveC_SignFlag				0x0004 + SinWaveC_Regsiter

#define		SinWave							0x02FC
				// SinGenerator.h SinWave_t
#define			SinWave_Offset					0x0000 + SinWave
#define			SinWave_Phase					0x0004 + SinWave
#define			SinWave_Gain					0x0008 + SinWave
#define			SinWave_Output					0x000C + SinWave
#define			SinWave_OutAddr					0x0010 + SinWave
#define		CosWave							0x0310
				// SinGenerator.h SinWave_t
#define			CosWave_Offset					0x0000 + CosWave
#define			CosWave_Phase					0x0004 + CosWave
#define			CosWave_Gain					0x0008 + CosWave
#define			CosWave_Output					0x000C + CosWave
#define			CosWave_OutAddr					0x0010 + CosWave

#define		WaitTimerData					0x0324
				// CommonLibrary.h  WaitTimer_Type
#define			WaitTimerData_UiWaitCounter		0x0000 + WaitTimerData
#define			WaitTimerData_UiTargetCount		0x0004 + WaitTimerData

#define		PanTilt_DMA						0x0330
#define			PanTilt_DMA_ScTpdSts			0x000C + PanTilt_DMA

//#ifdef	SEL_SHIFT_COR
#define			GyroRAM_Z_GYRO_OFFSET		0x0370

#define			GYRO_ZRAM_GZ_ADIDAT			0x0394
#define			GYRO_ZRAM_GZOFFZ			0x03A0

#define		AcclFilDly_X					0x03B8
#define		AcclFilDly_Y					0x03E8
#define		AcclFilDly_Z					0x0418

#define		AcclRAM_X						0x0448
#define			ACCLRAM_X_AC_ADIDAT			0x0000 + AcclRAM_X
#define			ACCLRAM_X_AC_OFFSET			0x0004 + AcclRAM_X

#define		AcclRAM_Y						0x0474
#define			ACCLRAM_Y_AC_ADIDAT			0x0000 + AcclRAM_Y
#define			ACCLRAM_Y_AC_OFFSET			0x0004 + AcclRAM_Y

#define		AcclRAM_Z
#define			ACCLRAM_Z_AC_ADIDAT			0x04A0
#define			ACCLRAM_Z_AC_OFFSET			0x04A4
//#endif	//SEL_SHIFT_COR

#define		OpticalOffset_X					(0x558)
#define		OpticalOffset_Y					(0x55C)



#define		OIS_POS_BY_AF_X					0x06D0
#define			OIS_POS_BY_AF_X1				(0x0000 + OIS_POS_BY_AF_X )
#define			OIS_POS_BY_AF_X2				(0x0004 + OIS_POS_BY_AF_X )
#define			OIS_POS_BY_AF_X3				(0x0008 + OIS_POS_BY_AF_X )
#define			OIS_POS_BY_AF_X4				(0x000C + OIS_POS_BY_AF_X )
#define			OIS_POS_BY_AF_X5				(0x0010 + OIS_POS_BY_AF_X )
#define			OIS_POS_BY_AF_X6				(0x0014 + OIS_POS_BY_AF_X )
#define			OIS_POS_BY_AF_X7				(0x0018 + OIS_POS_BY_AF_X )
#define			OIS_POS_BY_AF_X8				(0x001C + OIS_POS_BY_AF_X )
#define			OIS_POS_BY_AF_X9				(0x0020 + OIS_POS_BY_AF_X )

#define		OIS_POS_BY_AF_Y					0x06F4
#define			OIS_POS_BY_AF_Y1				(0x0000 + OIS_POS_BY_AF_Y )
#define			OIS_POS_BY_AF_Y2				(0x0004 + OIS_POS_BY_AF_Y )
#define			OIS_POS_BY_AF_Y3				(0x0008 + OIS_POS_BY_AF_Y )
#define			OIS_POS_BY_AF_Y4				(0x000C + OIS_POS_BY_AF_Y )
#define			OIS_POS_BY_AF_Y5				(0x0010 + OIS_POS_BY_AF_Y )
#define			OIS_POS_BY_AF_Y6				(0x0014 + OIS_POS_BY_AF_Y )
#define			OIS_POS_BY_AF_Y7				(0x0018 + OIS_POS_BY_AF_Y )
#define			OIS_POS_BY_AF_Y8				(0x001C + OIS_POS_BY_AF_Y )
#define			OIS_POS_BY_AF_Y9				(0x0020 + OIS_POS_BY_AF_Y )

#define		StPosition						(0x0718)
				// StPosition[8]
#define			StPosition_0					(0x0000 + StPosition )					// 0x05F0
#define			StPosition_1					(0x0004 + StPosition )					// 0x05F4
#define			StPosition_2					(0x0008 + StPosition )					// 0x05F8
#define			StPosition_3					(0x000C + StPosition )					// 0x05FC
#define			StPosition_4					(0x0010 + StPosition )					// 0x0600
#define			StPosition_5					(0x0014 + StPosition )					// 0x0604
#define			StPosition_6					(0x0018 + StPosition )					// 0x0608
#define			SiStepXY						(0x001C + StPosition )					// 0x060C



#define		FRA_DMA							(0xB40)
#define			FRA_DMA_Control				(0x04 + FRA_DMA	)
//#define			FRA_DMA_DeciCount			(0x0C + FRA_DMA	)
#define			FRA_DMA_DeciShift			(0x10 + FRA_DMA	)
#define			FRA_DMA_InputData			(0x18 + FRA_DMA	)
#define			FRA_DMA_OutputData			(0x1C + FRA_DMA	)

#define			FRA_DMA_Gain				(0x70 + FRA_DMA	)
#define			FRA_DMA_Phase				(0x74 + FRA_DMA	)
//==============================================================================
//DMB
//==============================================================================
#define		SiVerNum						0x8000
	#define		ACT_TVAXXXX			0x01
	#define		ACT_45DEG			0xff	// dummy

	#define		GYRO_ICM20690		0x00
	#define		GYRO_LSM6DSM		0x02
#define		SiCalID							0x8004
#define		SiActInf						0x8008

#define		StCalibrationData				0x8010
				// Calibration.h  CalibrationData_Type
#define			StCaliData_UsCalibrationStatus	0x0000 + StCalibrationData
#define			StCaliData_UiHallValue0			0x0004 + StCalibrationData
#define			StCaliData_UiHallValue1			0x0008 + StCalibrationData
#define			StCaliData_UiHallValue2			0x000C + StCalibrationData
#define			StCaliData_UiHallValue3			0x0010 + StCalibrationData
#define			StCaliData_UiHallValue4			0x0014 + StCalibrationData
#define			StCaliData_UiHallValue5			0x0018 + StCalibrationData
#define			StCaliData_UiHallValue6			0x001C + StCalibrationData
#define			StCaliData_UiHallValue7			0x0020 + StCalibrationData
#define			StCaliData_UiHallBias_X			0x0024 + StCalibrationData
#define			StCaliData_UiHallOffset_X		0x0028 + StCalibrationData
#define			StCaliData_UiHallBias_Y			0x002C + StCalibrationData
#define			StCaliData_UiHallOffset_Y		0x0030 + StCalibrationData
#define			StCaliData_SiLoopGain_X			0x0034 + StCalibrationData
#define			StCaliData_SiLoopGain_Y			0x0038 + StCalibrationData
#define			StCaliData_SiLensCen_Offset_X	0x003C + StCalibrationData
#define			StCaliData_SiLensCen_Offset_Y	0x0040 + StCalibrationData
#define			StCaliData_SiOtpCen_Offset_X	0x0044 + StCalibrationData
#define			StCaliData_SiOtpCen_Offset_Y	0x0048 + StCalibrationData
#define			StCaliData_SiGyroOffset_X		0x004C + StCalibrationData
#define			StCaliData_SiGyroOffset_Y		0x0050 + StCalibrationData
#define			StCaliData_SiGyroGain_X			0x0054 + StCalibrationData
#define			StCaliData_SiGyroGain_Y			0x0058 + StCalibrationData

#define		HallFilterCoeffX_hxgoutg		0x8094
//#define		HallFilterCoeffX_hxgain0		0x80F0
//#define		HallFilterCoeffX_hxgain1		0x80F4

#define		HallFilterCoeffY_hygoutg		0x8130
//#define		HallFilterCoeffY_hygain0		0x818C
//#define		HallFilterCoeffY_hygain1		0x8190

#define		HallFilterCoeffX				0x8090
				// HallFilterCoeff.h  DM_HFC_t
#define			HallFilterCoeffX_HXIGAIN		0x0000 + HallFilterCoeffX
#define			HallFilterCoeffX_GYROXOUTGAIN	0x0004 + HallFilterCoeffX_HXIGAIN
#define			HallFilterCoeffX_HXOFFGAIN		0x0004 + HallFilterCoeffX_GYROXOUTGAIN

#define			HallFilterCoeffX_hxiab			0x0004 + HallFilterCoeffX_HXOFFGAIN
#define			HallFilterCoeffX_hxiac			0x0004 + HallFilterCoeffX_hxiab
#define			HallFilterCoeffX_hxiaa			0x0004 + HallFilterCoeffX_hxiac
#define			HallFilterCoeffX_hxibb			0x0004 + HallFilterCoeffX_hxiaa
#define			HallFilterCoeffX_hxibc			0x0004 + HallFilterCoeffX_hxibb
#define			HallFilterCoeffX_hxiba			0x0004 + HallFilterCoeffX_hxibc
#define			HallFilterCoeffX_hxdab			0x0004 + HallFilterCoeffX_hxiba
#define			HallFilterCoeffX_hxdac			0x0004 + HallFilterCoeffX_hxdab
#define			HallFilterCoeffX_hxdaa			0x0004 + HallFilterCoeffX_hxdac
#define			HallFilterCoeffX_hxdbb			0x0004 + HallFilterCoeffX_hxdaa
#define			HallFilterCoeffX_hxdbc			0x0004 + HallFilterCoeffX_hxdbb
#define			HallFilterCoeffX_hxdba			0x0004 + HallFilterCoeffX_hxdbc
#define			HallFilterCoeffX_hxdcc			0x0004 + HallFilterCoeffX_hxdba
#define			HallFilterCoeffX_hxdcb			0x0004 + HallFilterCoeffX_hxdcc
#define			HallFilterCoeffX_hxdca			0x0004 + HallFilterCoeffX_hxdcb
#define			HallFilterCoeffX_hxpgain0		0x0004 + HallFilterCoeffX_hxdca
#define			HallFilterCoeffX_hxigain0		0x0004 + HallFilterCoeffX_hxpgain0
#define			HallFilterCoeffX_hxdgain0		0x0004 + HallFilterCoeffX_hxigain0
#define			HallFilterCoeffX_hxpgain1		0x0004 + HallFilterCoeffX_hxdgain0
#define			HallFilterCoeffX_hxigain1		0x0004 + HallFilterCoeffX_hxpgain1
#define			HallFilterCoeffX_hxdgain1		0x0004 + HallFilterCoeffX_hxigain1
#define			HallFilterCoeffX_hxgain0		0x0004 + HallFilterCoeffX_hxdgain1
#define			HallFilterCoeffX_hxgain1		0x0004 + HallFilterCoeffX_hxgain0

#define			HallFilterCoeffX_hxsb			0x0004 + HallFilterCoeffX_hxgain1
#define			HallFilterCoeffX_hxsc			0x0004 + HallFilterCoeffX_hxsb
#define			HallFilterCoeffX_hxsa			0x0004 + HallFilterCoeffX_hxsc

#define			HallFilterCoeffX_hxob			0x0004 + HallFilterCoeffX_hxsa
#define			HallFilterCoeffX_hxoc			0x0004 + HallFilterCoeffX_hxob
#define			HallFilterCoeffX_hxod			0x0004 + HallFilterCoeffX_hxoc
#define			HallFilterCoeffX_hxoe			0x0004 + HallFilterCoeffX_hxod
#define			HallFilterCoeffX_hxoa			0x0004 + HallFilterCoeffX_hxoe
#define			HallFilterCoeffX_hxpb			0x0004 + HallFilterCoeffX_hxoa
#define			HallFilterCoeffX_hxpc			0x0004 + HallFilterCoeffX_hxpb
#define			HallFilterCoeffX_hxpd			0x0004 + HallFilterCoeffX_hxpc
#define			HallFilterCoeffX_hxpe			0x0004 + HallFilterCoeffX_hxpd
#define			HallFilterCoeffX_hxpa			0x0004 + HallFilterCoeffX_hxpe

#define		HallFilterCoeffY				0x812c
				// HallFilterCoeff.h  DM_HFC_t
#define			HallFilterCoeffY_HYIGAIN		0x0000 + HallFilterCoeffY
#define			HallFilterCoeffY_GYROYOUTGAIN	0x0004 + HallFilterCoeffY_HYIGAIN
#define			HallFilterCoeffY_HYOFFGAIN		0x0004 + HallFilterCoeffY_GYROYOUTGAIN

#define			HallFilterCoeffY_hyiab			0x0004 + HallFilterCoeffY_HYOFFGAIN
#define			HallFilterCoeffY_hyiac			0x0004 + HallFilterCoeffY_hyiab
#define			HallFilterCoeffY_hyiaa			0x0004 + HallFilterCoeffY_hyiac
#define			HallFilterCoeffY_hyibb			0x0004 + HallFilterCoeffY_hyiaa
#define			HallFilterCoeffY_hyibc			0x0004 + HallFilterCoeffY_hyibb
#define			HallFilterCoeffY_hyiba			0x0004 + HallFilterCoeffY_hyibc
#define			HallFilterCoeffY_hydab			0x0004 + HallFilterCoeffY_hyiba
#define			HallFilterCoeffY_hydac			0x0004 + HallFilterCoeffY_hydab
#define			HallFilterCoeffY_hydaa			0x0004 + HallFilterCoeffY_hydac
#define			HallFilterCoeffY_hydbb			0x0004 + HallFilterCoeffY_hydaa
#define			HallFilterCoeffY_hydbc			0x0004 + HallFilterCoeffY_hydbb
#define			HallFilterCoeffY_hydba			0x0004 + HallFilterCoeffY_hydbc
#define			HallFilterCoeffY_hydcc			0x0004 + HallFilterCoeffY_hydba
#define			HallFilterCoeffY_hydcb			0x0004 + HallFilterCoeffY_hydcc
#define			HallFilterCoeffY_hydca			0x0004 + HallFilterCoeffY_hydcb
#define			HallFilterCoeffY_hypgain0		0x0004 + HallFilterCoeffY_hydca
#define			HallFilterCoeffY_hyigain0		0x0004 + HallFilterCoeffY_hypgain0
#define			HallFilterCoeffY_hydgain0		0x0004 + HallFilterCoeffY_hyigain0
#define			HallFilterCoeffY_hypgain1		0x0004 + HallFilterCoeffY_hydgain0
#define			HallFilterCoeffY_hyigain1		0x0004 + HallFilterCoeffY_hypgain1
#define			HallFilterCoeffY_hydgain1		0x0004 + HallFilterCoeffY_hyigain1
#define			HallFilterCoeffY_hygain0		0x0004 + HallFilterCoeffY_hydgain1
#define			HallFilterCoeffY_hygain1		0x0004 + HallFilterCoeffY_hygain0
#define			HallFilterCoeffY_hysb			0x0004 + HallFilterCoeffY_hygain1
#define			HallFilterCoeffY_hysc			0x0004 + HallFilterCoeffY_hysb
#define			HallFilterCoeffY_hysa			0x0004 + HallFilterCoeffY_hysc
#define			HallFilterCoeffY_hyob			0x0004 + HallFilterCoeffY_hysa
#define			HallFilterCoeffY_hyoc			0x0004 + HallFilterCoeffY_hyob
#define			HallFilterCoeffY_hyod			0x0004 + HallFilterCoeffY_hyoc
#define			HallFilterCoeffY_hyoe			0x0004 + HallFilterCoeffY_hyod
#define			HallFilterCoeffY_hyoa			0x0004 + HallFilterCoeffY_hyoe
#define			HallFilterCoeffY_hypb			0x0004 + HallFilterCoeffY_hyoa
#define			HallFilterCoeffY_hypc			0x0004 + HallFilterCoeffY_hypb
#define			HallFilterCoeffY_hypd			0x0004 + HallFilterCoeffY_hypc
#define			HallFilterCoeffY_hype			0x0004 + HallFilterCoeffY_hypd
#define			HallFilterCoeffY_hypa			0x0004 + HallFilterCoeffY_hype

#define		HallFilterLimitX				0x81c8
#define		HallFilterLimitY				0x81e0
#define		HallFilterShiftX				0x81f8
#define		HallFilterShiftY				0x81fe

#define		HF_MIXING						0x8214
#define			HF_hx45x						0x0000 + HF_MIXING
#define			HF_hx45y						0x0004 + HF_MIXING
#define			HF_hy45y						0x0008 + HF_MIXING
#define			HF_hy45x						0x000C + HF_MIXING
#define			HF_ShiftX						0x0010 + HF_MIXING

#define		HAL_LN_CORRECT					0x8228
#define			HAL_LN_COEFAX					0x0000 + HAL_LN_CORRECT
#define			HAL_LN_COEFBX					0x000C + HAL_LN_COEFAX
#define			HAL_LN_ZONEX					0x000C + HAL_LN_COEFBX
#define			HAL_LN_COEFAY					0x000A + HAL_LN_ZONEX
#define			HAL_LN_COEFBY					0x000C + HAL_LN_COEFAY
#define			HAL_LN_ZONEY					0x000C + HAL_LN_COEFBY

#define		GyroFilterTableX				0x8270
				// GyroFilterCoeff.h  DM_GFC_t
#define			GyroFilterTableX_gx45x			0x0000 + GyroFilterTableX
#define			GyroFilterTableX_gx45y			0x0004 + GyroFilterTableX
#define			GyroFilterTableX_gxgyro			0x0008 + GyroFilterTableX
#define			GyroFilterTableX_gxsengen		0x000C + GyroFilterTableX
#define			GyroFilterTableX_gxl1b			0x0010 + GyroFilterTableX
#define			GyroFilterTableX_gxl1c			0x0014 + GyroFilterTableX
#define			GyroFilterTableX_gxl1a			0x0018 + GyroFilterTableX
#define			GyroFilterTableX_gxl2b			0x001C + GyroFilterTableX
#define			GyroFilterTableX_gxl2c			0x0020 + GyroFilterTableX
#define			GyroFilterTableX_gxl2a			0x0024 + GyroFilterTableX
#define			GyroFilterTableX_gxigain		0x0028 + GyroFilterTableX
#define			GyroFilterTableX_gxh1b			0x002C + GyroFilterTableX
#define			GyroFilterTableX_gxh1c			0x0030 + GyroFilterTableX
#define			GyroFilterTableX_gxh1a			0x0034 + GyroFilterTableX
#define			GyroFilterTableX_gxk1b			0x0038 + GyroFilterTableX
#define			GyroFilterTableX_gxk1c			0x003C + GyroFilterTableX
#define			GyroFilterTableX_gxk1a			0x0040 + GyroFilterTableX
#define			GyroFilterTableX_gxgain			0x0044 + GyroFilterTableX
#define			GyroFilterTableX_gxzoom			0x0048 + GyroFilterTableX
#define			GyroFilterTableX_gxlenz			0x004C + GyroFilterTableX
#define			GyroFilterTableX_gxt2b			0x0050 + GyroFilterTableX
#define			GyroFilterTableX_gxt2c			0x0054 + GyroFilterTableX
#define			GyroFilterTableX_gxt2a			0x0058 + GyroFilterTableX
#define			GyroFilterTableX_afzoom			0x005C + GyroFilterTableX

#define		GyroFilterTableY				0x82D0
				// GyroFilterCoeff.h  DM_GFC_t
#define			GyroFilterTableY_gy45y			0x0000 + GyroFilterTableY
#define			GyroFilterTableY_gy45x			0x0004 + GyroFilterTableY
#define			GyroFilterTableY_gygyro			0x0008 + GyroFilterTableY
#define			GyroFilterTableY_gysengen		0x000C + GyroFilterTableY
#define			GyroFilterTableY_gyl1b			0x0010 + GyroFilterTableY
#define			GyroFilterTableY_gyl1c			0x0014 + GyroFilterTableY
#define			GyroFilterTableY_gyl1a			0x0018 + GyroFilterTableY
#define			GyroFilterTableY_gyl2b			0x001C + GyroFilterTableY
#define			GyroFilterTableY_gyl2c			0x0020 + GyroFilterTableY
#define			GyroFilterTableY_gyl2a			0x0024 + GyroFilterTableY
#define			GyroFilterTableY_gyigain		0x0028 + GyroFilterTableY
#define			GyroFilterTableY_gyh1b			0x002C + GyroFilterTableY
#define			GyroFilterTableY_gyh1c			0x0030 + GyroFilterTableY
#define			GyroFilterTableY_gyh1a			0x0034 + GyroFilterTableY
#define			GyroFilterTableY_gyk1b			0x0038 + GyroFilterTableY
#define			GyroFilterTableY_gyk1c			0x003C + GyroFilterTableY
#define			GyroFilterTableY_gyk1a			0x0040 + GyroFilterTableY
#define			GyroFilterTableY_gygain			0x0044 + GyroFilterTableY
#define			GyroFilterTableY_gyzoom			0x0048 + GyroFilterTableY
#define			GyroFilterTableY_gylenz			0x004C + GyroFilterTableY
#define			GyroFilterTableY_gyt2b			0x0050 + GyroFilterTableY
#define			GyroFilterTableY_gyt2c			0x0054 + GyroFilterTableY
#define			GyroFilterTableY_gyt2a			0x0058 + GyroFilterTableY
#define			GyroFilterTableY_afzoom			0x005C + GyroFilterTableY

#define		Gyro_Limiter_X					0x8330
#define		Gyro_Limiter_Y   		        0x8334

#define		GyroFilterShiftX				0x8338
				// GyroFilterCoeff.h  GF_Shift_t
#define			RG_GX2X4XF						0x0000 + GyroFilterShiftX
#define			RG_GX2X4XB						0x0001 + GyroFilterShiftX
#define			RG_GXOX							0x0002 + GyroFilterShiftX
#define			RG_GXAFZ						0x0003 + GyroFilterShiftX

#define		GyroFilterShiftY				0x833C
				// GyroFilterCoeff.h  GF_Shift_t
#define			RG_GY2X4XF						0x0000 + GyroFilterShiftY
#define			RG_GY2X4XB						0x0001 + GyroFilterShiftY
#define			RG_GYOX							0x0002 + GyroFilterShiftY
#define			RG_GYAFZ						0x0003 + GyroFilterShiftY

#define		MeasureFilterA_Coeff			0x8380
				// MeasureFilter.h  MeasureFilter_Type
#define			MeasureFilterA_Coeff_b1			0x0000 + MeasureFilterA_Coeff
#define			MeasureFilterA_Coeff_c1			0x0004 + MeasureFilterA_Coeff
#define			MeasureFilterA_Coeff_a1			0x0008 + MeasureFilterA_Coeff
#define			MeasureFilterA_Coeff_b2			0x000C + MeasureFilterA_Coeff
#define			MeasureFilterA_Coeff_c2			0x0010 + MeasureFilterA_Coeff
#define			MeasureFilterA_Coeff_a2			0x0014 + MeasureFilterA_Coeff

#define		MeasureFilterB_Coeff			0x8398
				// MeasureFilter.h  MeasureFilter_Type
#define			MeasureFilterB_Coeff_b1			0x0000 + MeasureFilterB_Coeff
#define			MeasureFilterB_Coeff_c1			0x0004 + MeasureFilterB_Coeff
#define			MeasureFilterB_Coeff_a1			0x0008 + MeasureFilterB_Coeff
#define			MeasureFilterB_Coeff_b2			0x000C + MeasureFilterB_Coeff
#define			MeasureFilterB_Coeff_c2			0x0010 + MeasureFilterB_Coeff
#define			MeasureFilterB_Coeff_a2			0x0014 + MeasureFilterB_Coeff


#define		Accl45Filter					0x8640
#define			Accl45Filter_XAmain				(0x0000 + Accl45Filter )
#define			Accl45Filter_XAsub				(0x0004 + Accl45Filter )
#define			Accl45Filter_YAmain				(0x0008 + Accl45Filter )
#define			Accl45Filter_YAsub				(0x000C + Accl45Filter )

#define		MotionSensor_Sel				0x865C
#define			MS_SEL_GX0						0x0000 + MotionSensor_Sel
#define			MS_SEL_GX1						0x0004 + MotionSensor_Sel
#define			MS_SEL_GY0						0x0008 + MotionSensor_Sel
#define			MS_SEL_GY1						0x000C + MotionSensor_Sel
#define			MS_SEL_GZ						0x0010 + MotionSensor_Sel
#define			MS_SEL_AX0						0x0014 + MotionSensor_Sel
#define			MS_SEL_AX1						0x0018 + MotionSensor_Sel
#define			MS_SEL_AY0						0x001C + MotionSensor_Sel
#define			MS_SEL_AY1						0x0020 + MotionSensor_Sel
#define			MS_SEL_AZ						0x0024 + MotionSensor_Sel

#define		AngleCorrect					0x86E8
#define			X_main							0x0000 + AngleCorrect
#define			X_sub							0x0004 + AngleCorrect
#define			Y_main							0x0008 + AngleCorrect
#define			Y_sub							0x000C + AngleCorrect
#define			SX_main							0x0010 + AngleCorrect
#define			SX_sub							0x0014 + AngleCorrect
#define			SY_main							0x0018 + AngleCorrect
#define			SY_sub							0x001C + AngleCorrect



#define			FRA_DMB_C0 					0x8CF0
#define			FRA_DMB_S0 					0x8CF4
#define			FRA_DMB_CN 					0x8CF8
#define			FRA_DMB_SN 					0x8CFC

//==============================================================================
//IO
//==============================================================================
// System Control
#define 		SYSDSP_DSPDIV						0xD00014
#define 		SYSDSP_SOFTRES						0xD0006C
#define 		SYSDSP_REMAP						0xD000AC
#define 		SYSDSP_CVER							0xD00100
// A/D D/A interface
#define			ADDA_FSCTRL							0xD01008
#define 		ADDA_ADDAINT						0xD0100C
#define 		ADDA_DASEL							0xD01050
#define 		ADDA_DAO							0xD01054

#define			ROMINFO								0xE050D4

/************************************************************************/
/*        Flash access													*/
/************************************************************************/
#define FLASHROM_128		0xE07000	// Flash Memory
#define 		FLASHROM_FLA_RDAT					(FLASHROM_128 + 0x00)
#define 		FLASHROM_FLA_WDAT					(FLASHROM_128 + 0x04)
#define 		FLASHROM_ACSCNT						(FLASHROM_128 + 0x08)
#define 		FLASHROM_FLA_ADR					(FLASHROM_128 + 0x0C)
	#define			USER_MAT				0
	#define			INF_MAT0				1
	#define			INF_MAT1				2
	#define			INF_MAT2				4
#define 		FLASHROM_CMD						(FLASHROM_128 + 0x10)
#define 		FLASHROM_FLAWP						(FLASHROM_128 + 0x14)
#define 		FLASHROM_FLAINT						(FLASHROM_128 + 0x18)
#define 		FLASHROM_FLAMODE					(FLASHROM_128 + 0x1C)
#define 		FLASHROM_TPECPW						(FLASHROM_128 + 0x20)
#define 		FLASHROM_TACC						(FLASHROM_128 + 0x24)

#define 		FLASHROM_ERR_FLA					(FLASHROM_128 + 0x98)
#define 		FLASHROM_RSTB_FLA					(FLASHROM_128 + 0x4CC)
#define 		FLASHROM_UNLK_CODE1					(FLASHROM_128 + 0x554)
#define 		FLASHROM_CLK_FLAON					(FLASHROM_128 + 0x664)
#define 		FLASHROM_UNLK_CODE2					(FLASHROM_128 + 0xAA8)
#define 		FLASHROM_UNLK_CODE3					(FLASHROM_128 + 0xCCC)


#define			AREA_ALL	0	// 1,2,4,8 ALL
#define			AREA_HALL	1	// HALL,GYRO OFFSET,ACCL OFFSET
#define			AREA_GYRO	2	// GYRO GAIN
#define			AREA_CRS	4	// CROSS TALK
#define			AREA_LIN	8	// LINEARITY

#define			CALIB_STATUS

/* copy from Ois.h*/
/**************** Model name *****************/
#define	SELECT_VENDOR		0x00	// --- select vender ---//
									// 0bit : SUNNY

/**************** FW version *****************/
 #define	FW_VER			0x02
 #define	SUB_VER			0x00			// ATMEL SUB Version

/**************** Select Mode **************/
#define		MODULE_VENDOR	0x07


//#define		NEUTRAL_CENTER				//!< Upper Position Current 0mA Measurement
//#define		NEUTRAL_CENTER_FINE			//!< Optimize natural center current
#define		SEL_SHIFT_COR				//!< Shift correction
#define		__OIS_UIOIS_GYRO_USE__
//#define		ACT02_AMP_NARROW
/**************** Filter sampling **************/
#define		FS_MODE		0		// 0 : originally
								// 1 : SLOW
#if FS_MODE == 0
#define	FS_FREQ			18044.61942F
#else
#define	FS_FREQ			15027.3224F
#endif

#define	GYRO_SENSITIVITY	65.5		//!< Gyro sensitivity LSB/dps

// Command Status
#define		EXE_END		0x00000002L		//!< Execute End (Adjust OK)
#define		EXE_ERR		0x00000003L		//!< Adjust NG : Execution Failure
#define		EXE_HXADJ	0x00000006L		//!< Adjust NG : X Hall NG (Gain or Offset)
#define		EXE_HYADJ	0x0000000AL		//!< Adjust NG : Y Hall NG (Gain or Offset)
#define		EXE_LXADJ	0x00000012L		//!< Adjust NG : X Loop NG (Gain)
#define		EXE_LYADJ	0x00000022L		//!< Adjust NG : Y Loop NG (Gain)
#define		EXE_GXADJ	0x00000042L		//!< Adjust NG : X Gyro NG (offset)
#define		EXE_GYADJ	0x00000082L		//!< Adjust NG : Y Gyro NG (offset)
#ifdef	SEL_SHIFT_COR
#define		EXE_GZADJ	0x00400002L		//!< Adjust NG : Z Gyro NG (offset)
#define		EXE_AZADJ	0x00200002L		// Adjust NG : Z ACCL NG (offset)
#define		EXE_AYADJ	0x00100002L		// Adjust NG : Y ACCL NG (offset)
#define		EXE_AXADJ	0x00080002L		// Adjust NG : X ACCL NG (offset)
#define		EXE_XSTRK	0x00040002L		// CONFIRM NG : X (offset)
#define		EXE_YSTRK	0x00020002L		// CONFIRM NG : Y (offset)
#endif	//SEL_SHIFT_COR
#define		EXE_HXMVER	0x06
#define		EXE_HYMVER	0x0A
#define		EXE_GXABOVE	0x06
#define		EXE_GXBELOW	0x0A
#define		EXE_GYABOVE	0x12
#define		EXE_GYBELOW	0x22


// Common Define
#define	SUCCESS			0x00			//!< Success
#define	FAILURE			0x01			//!< Failure

#ifndef ON
 #define	ON				0x01		//!< ON
 #define	OFF				0x00		//!< OFF
#endif
 #define	SPC				0x02		//!< Special Mode

#define	X_DIR			0x00			//!< X Direction
#define	Y_DIR			0x01			//!< Y Direction
#define	Z_DIR			0x02			//!< Z Direction(AF)

/************************************************/
/*	Command										*/
/************************************************/
#define		CMD_IO_ADR_ACCESS				0xC000				//!< IO Write Access
#define		CMD_IO_DAT_ACCESS				0xD000				//!< IO Read Access
#define		CMD_RETURN_TO_CENTER			0xF010				//!< Center Servo ON/OFF choose axis
	#define		BOTH_SRV_OFF					0x00000000			//!< Both   Servo OFF
	#define		XAXS_SRV_ON						0x00000001			//!< X axis Servo ON
	#define		YAXS_SRV_ON						0x00000002			//!< Y axis Servo ON
	#define		BOTH_SRV_ON						0x00000003			//!< Both   Servo ON
	#define		ZAXS_SRV_OFF					0x00000004			//!< Z axis Servo OFF
	#define		ZAXS_SRV_ON						0x00000005			//!< Z axis Servo ON
#define		CMD_PAN_TILT					0xF011				//!< Pan Tilt Enable/Disable
	#define		PAN_TILT_OFF					0x00000000			//!< Pan/Tilt OFF
	#define		PAN_TILT_ON						0x00000001			//!< Pan/Tilt ON
#define		CMD_OIS_ENABLE					0xF012				//!< Ois Enable/Disable
	#define		OIS_DISABLE						0x00000000			//!< OIS Disable
	#define		OIS_DIS_PUS						0x00000008			//!< OIS Disable ( pasue calcuration value )
	#define		OIS_ENABLE						0x00000001			//!< OIS Enable
	#define		OIS_ENA_NCL						0x00000002			//!< OIS Enable ( none Delay clear )
	#define		OIS_ENA_DOF						0x00000004			//!< OIS Enable ( Drift offset exec )
#define		CMD_MOVE_STILL_MODE				0xF013				//!< Select mode
	#define		MOVIE_MODE						0x00000000			//!< Movie mode
	#define		STILL_MODE						0x00000001			//!< Still mode
	#define		MOVIE_MODE1						0x00000002			//!< Movie Preview mode 1
	#define		STILL_MODE1						0x00000003			//!< Still Preview mode 1
	#define		MOVIE_MODE2						0x00000004			//!< Movie Preview mode 2
	#define		STILL_MODE2						0x00000005			//!< Still Preview mode 2
	#define		MOVIE_MODE3						0x00000006			//!< Movie Preview mode 3
	#define		STILL_MODE3						0x00000007			//!< Still Preview mode 3
#define		CMD_CALIBRATION					0xF014				//!< Gyro offset re-calibration
#define		CMD_GYROINITIALCOMMAND			0xF015				//!< Select gyro sensor
#define		CMD_LASER_LINEAR_DATA			0xF016				//!< Laser Linearity
#define		CMD_STANDBY_ENABLE				0xF019
	#define		ACTIVE_MODE						0x00000000			//!< Active mode
	#define		STANDBY_MODE					0x00000001			//!< Standby mode
#define		CMD_AF_POSITION					0xF01A				// AF Position
#define		CMD_SSC_ENABLE					0xF01C				//!< Select mode
	#define		SSC_DISABLE						0x00000000			//!< Ssc Disable
	#define		SSC_ENABLE						0x00000001			//!< Ssc Enable

#define		CMD_READ_STATUS					0xF100				//!< Status Read

#define		READ_STATUS_INI					0x01000000

#define		STBOSCPLL						0x00D00074			//!< STB OSC
	#define		OSC_STB							0x00000002			//!< OSC standby

// Calibration.h *******************************************************************
#define	HLXO				0x00000001			//!< D/A Converter Channel Select OIS X Offset
#define	HLYO				0x00000002			//!< D/A Converter Channel Select OIS Y Offset
#define	HLXBO				0x00000008			//!< D/A Converter Channel Select OIS X BIAS
#define	HLYBO				0x00000010			//!< D/A Converter Channel Select OIS Y BIAS

#endif
/* _CAM_OIS_LC898218_H_ */
