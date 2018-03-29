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

#ifndef _KD_CAMERA_HW_H_
#define _KD_CAMERA_HW_H_

/*#include <mach/mt_pm_ldo.h>*/
#include <kd_imgsensor.h>

#define MTKCAM_USING_PWRREG

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE  (1)
#endif
#ifndef BOOL
typedef unsigned char BOOL;
#endif

typedef enum {
	VDD_None,
	PDN,
	RST,
	SensorMCLK,
	AVDD,
	DVDD,
	DOVDD,
	AFVDD,
	LDO
} PowerType;

typedef enum {
	Vol_Low = 0,
	Vol_High = 1,
	Vol_900 = 900,
	Vol_1000 = 1000,
	Vol_1100 = 1100,
	Vol_1200 = 1200,
	Vol_1300 = 1300,
	Vol_1350 = 1350,
	Vol_1500 = 1500,
	Vol_1800 = 1800,
	Vol_2000 = 2000,
	Vol_2100 = 2100,
	Vol_2500 = 2500,
	Vol_2800 = 2800,
	Vol_3000 = 3000,
	Vol_3300 = 3300,
	Vol_3400 = 3400,
	Vol_3500 = 3500,
	Vol_3600 = 3600
} Voltage;


typedef struct {
	PowerType PowerType;
	Voltage Voltage;
	u32 Delay;
} PowerInformation;


typedef struct {
	char *SensorName;
	PowerInformation PowerInfo[12];
} PowerSequence;

typedef struct {
	PowerSequence PowerSeq[16];
} PowerUp;

typedef struct {
	u32 Gpio_Pin;
	u32 Gpio_Mode;
	Voltage Voltage;
} PowerCustInfo;

typedef struct {
	PowerCustInfo PowerCustInfo[6];
} PowerCust;


#ifndef BOOL
typedef unsigned char BOOL;
#endif

/* defined in kd_sensorlist.c */
extern void ISP_MCLK1_EN(BOOL En);
extern void ISP_MCLK2_EN(BOOL En);
extern int mtkcam_gpio_set(int PinIdx, int PwrType, int Val);
extern PowerUp PowerOnList;
extern const int camera_i2c_bus_num1;
extern const int camera_i2c_bus_num2;

/* Camera Power Regulator Control */
#ifdef MTKCAM_USING_PWRREG

extern BOOL CAMERA_Regulator_poweron(int PinIdx, int PwrType, int Voltage);
extern BOOL CAMERA_Regulator_powerdown(int PinIdx, int PwrType);

#endif


#endif

