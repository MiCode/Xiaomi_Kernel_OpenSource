/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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


#ifndef OIS_MAIN_H
#define OIS_MAIN_H

/* Compile Switch Purpose */
#define ENABLE_GYRO_DRIFT_COMP /* RHM_HT 2013/11/25    Added */

#define _STR_AREA_ "F:\\DEBUGBMP\\" /* For Image save (Debug purpose) */

/* ==> RHM_HT 2013/04/15        Add to the report for error details */
/* #define              OIS_TRUE        0 */
/* #define              OIS_FALSE       -1 */
/* #define              OIS_READY       1 */

#define ADJ_OK 0
#define ADJ_ERR -1

#define PROG_DL_ERR -2
#define COEF_DL_ERR -3

#define CURDAT_FIT_ERR -4
#define CURDAT_ADJ_ERR -5
#define HALOFS_X_ADJ_ERR -6
#define HALOFS_Y_ADJ_ERR -7
#define PSTXOF_FIT_ERR -8
#define PSTYOF_FIT_ERR -9
#define PSTXOF_ADJ_ERR -10
#define PSTYOF_ADJ_ERR -11
#define KGXG_ROUGH_ADJ_ERR -12
#define KGYG_ROUGH_ADJ_ERR -13

#define GX_OFS_ADJ_ERR -14
#define GY_OFS_ADJ_ERR -15

#define TAKE_PICTURE_ERR -20
#define KGXG_FINE_ADJ_ERR -21
#define KGYG_FINE_ADJ_ERR -22
#define KGXG_SENSITIVITY_ERR -23
#define KGYG_SENSITIVITY_ERR -24
#define KGXHG_ADJ_ERR -25
#define KGYHG_ADJ_ERR -26

#define ACGX_ADJ_OVER_P -27
#define ACGY_ADJ_OVER_P -28
#define ACGX_ADJ_OVER_N -29
#define ACGY_ADJ_OVER_N -30
#define ACGX_KGXG_ADJ_ERR -31
#define ACGY_KGYG_ADJ_ERR -32

#define TMP_X_ADJ_ERR -33 /* RHM_HT 2013/11/25    Added */
#define TMP_Y_ADJ_ERR -34 /* RHM_HT 2013/11/25    Added */

#define MALLOC1_ERR -51
#define MALLOC2_ERR -52
#define MALLOC3_ERR -53
#define MALLOC4_ERR -54

/* Error for sub-routine */
#define OIS_NO_ERROR ADJ_OK
#define OIS_INVALID_PARAMETERS -100
#define OIS_FILE_RENAME_ERROR -101
#define OIS_FILE_NOT_FOUND -102
#define OIS_BITMAP_READ_ERROR -103
#define OIS_MATRIX_INV_ERROR -104
#define OIS_SC2_XLIMIT_OVER -105
#define OIS_CHART_ARRAY_OVER -106
#define OIS_DC_GAIN_SENS_OVER -107

#define OIS_MALLOC1_ERROR -111
#define OIS_MALLOC2_ERROR -112
#define OIS_MALLOC3_ERROR -113
#define OIS_MALLOC4_ERROR -114
#define OIS_MALLOC5_ERROR -115
#define OIS_MALLOC6_ERROR -116
#define OIS_MALLOC7_ERROR -117
#define OIS_MALLOC8_ERROR -118
#define OIS_MALLOC9_ERROR -119
#define OIS_MALLOC10_ERROR -120
#define OIS_MALLOC11_ERROR -121
#define OIS_MALLOC12_ERROR -122
#define OIS_MALLOC13_ERROR -123
#define OIS_MALLOC14_ERROR -124 /* RHM_HT 2013/11/25    add */

struct _BMP_GET_POS {
	short int x;		   /* x start position for clipping */
	short int y;		   /* y start position for clipping */
	short int width;	   /* clipping width */
	short int height;	  /* clipping height */
	unsigned char slice_level; /* slice level of bitmap binalization */
	unsigned char filter;      /* median filter enable */
	char *direction;	   /* direction of detection */
};

struct _POS {
	float x;
	float y;
};

struct _APPROXRESULT {
	double a; /* position x */
	double b; /* position y */
	double r; /* Radius */
};

#include "OIS_defi.h"
/* #include	"windef.h" */

#define Wait(a) Wait_usec(a * 1000UL)

short int func_PROGRAM_DOWNLOAD(void);
void func_COEF_DOWNLOAD(unsigned short int u16_coef_type);
void download(unsigned short int u16_type, unsigned short int u16_coef_type);

short int func_SET_SCENE_PARAM(unsigned char u16_scene, unsigned char u16_mode,
			       unsigned char filter, unsigned char range,
			       const struct _FACT_ADJ *param);

void SET_FADJ_PARAM(const struct _FACT_ADJ *param);

short int func_SET_SCENE_PARAM_for_NewGYRO_Fil(unsigned char u16_scene,
					       unsigned char u16_mode,
					       unsigned char filter,
					       unsigned char range,
					       const struct _FACT_ADJ *param);

void HalfShutterOn(void);

void I2C_OIS_per_write(unsigned char u08_adr, unsigned short int u16_dat);

void I2C_OIS_mem_write(unsigned char u08_adr, unsigned short int u16_dat);

unsigned short int I2C_OIS_per__read(unsigned char u08_adr);

unsigned short int I2C_OIS_mem__read(unsigned char u08_adr);

void I2C_OIS_spcl_cmnd(unsigned char u08_on, unsigned char u08_dat);

void I2C_OIS_F0123_wr_(unsigned char u08_dat0, unsigned char u08_dat1,
		       unsigned short int u16_dat2);

unsigned short int I2C_OIS_F0123__rd(void);

void POWER_UP_AND_PS_DISABLE(void);

void POWER_DOWN_AND_PS_ENABLE(void);

void VCOSET0(void);

void VCOSET1(void);

void WR_I2C(unsigned char slvadr, unsigned char size, unsigned char *dat);

unsigned short int RD_I2C(unsigned char slvadr, unsigned char size,
			  unsigned char *dat);

void store_FADJ_MEM_to_non_volatile_memory(struct _FACT_ADJ param);

struct _FACT_ADJ get_FADJ_MEM_from_non_volatile_memory(void);

void Wait_usec(unsigned long int time);

extern void Main_OIS(void);

extern void OIS_Standby(void);

extern int setVCMPos(unsigned short DAC_Val);

extern void setOISMode(int Disable);

extern int s4EEPROM_ReadReg_BU63165AF(unsigned short addr,
				      unsigned short *data);

extern int s4AF_WriteReg_BU63165AF(unsigned short i2c_id,
				   unsigned char *a_pSendData,
				   unsigned short a_sizeSendData);

extern int s4AF_ReadReg_BU63165AF(unsigned short i2c_id,
				  unsigned char *a_pSendData,
				  unsigned short a_sizeSendData,
				  unsigned char *a_pRecvData,
				  unsigned short a_sizeRecvData);

/* #define      DEBUG_FADJ */
#ifdef DEBUG_FADJ
int debug_print(const char *format,
		...); /* RHM_HT 2013/04/15    Add for DEBUG */

#define DEBUG_printf(a) debug_print a
#else
#define DEBUG_printf(a)
#endif

#endif /* OIS_MAIN_H */
