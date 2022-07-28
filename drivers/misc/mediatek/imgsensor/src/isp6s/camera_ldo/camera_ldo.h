/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef _CAMERA_LDO_H_
#define _CAMERA_LDO_H_

#define CAMERA_DEVICEID_1_ADDR     0x00
#define CAMERA_DEVICEID_2_ADDR    		    0x01
#define CAMERA_R_SD_ADDR               0x02
//一供和三供	ET5907MV和FAN53870
#define CAMERA_LDO_LDO1_OUT_ADDR    0x04 //DVDD1 MAIN
#define CAMERA_LDO_LDO2_OUT_ADDR    0x05 //DVDD2 SUB
#define CAMERA_LDO_LDO3_OUT_ADDR    0x06 //VDDAF  MAIN
#define CAMERA_LDO_LDO4_OUT_ADDR    0x07 //VDDIO MAIN WEIJU JINGSHEN  SUB
#define CAMERA_LDO_LDO5_OUT_ADDR    0x08 //AVDD3  JINGSHEN WEIJU
#define CAMERA_LDO_LDO6_OUT_ADDR    0x09 //AVDD2   SUB
#define CAMERA_LDO_LDO7_OUT_ADDR    0x0A //AVDD1  MAIN

#define CAMERA_LDO_LDO21_SEQ_ADDR    0x0B
#define CAMERA_LDO_LDO43_SEQ_ADDR    0x0C
#define CAMERA_LDO_LDO65_SEQ_ADDR    0x0D
#define CAMERA_LDO_LDO07_SEQ_ADDR    0x0E

#define CAMERA_LDO_LDO_EN_ADDR          0x03  //bit0:LDO1 ~ bit6:LDO7
#define CAMERA_LDO_LDO_SEQ_CTRL_ADDR    0x0F

//二供	WL2868C
#define CAMERA_LDO_LDO1_OUT_ADDR_2    0x03
#define CAMERA_LDO_LDO2_OUT_ADDR_2    0x04
#define CAMERA_LDO_LDO3_OUT_ADDR_2    0x05
#define CAMERA_LDO_LDO4_OUT_ADDR_2    0x06
#define CAMERA_LDO_LDO5_OUT_ADDR_2    0x07
#define CAMERA_LDO_LDO6_OUT_ADDR_2    0x08
#define CAMERA_LDO_LDO7_OUT_ADDR_2    0x09

#define CAMERA_LDO_LDO_EN_ADDR_2          0x0E



#define CAMERA_LDO_PRINT pr_debug

#define ET5907MV_deviceid			  0x00
#define WL2868C_deviceid			  0x33
#define FAN53870_deviceid			  0x01

typedef enum {
	CAMERA_LDO_DVDD1=0,
	CAMERA_LDO_DVDD2,
	CAMERA_LDO_VDDAF,
	CAMERA_LDO_AVDD2,
	CAMERA_LDO_VDDIO,
	CAMERA_LDO_AVDD1,
	CAMERA_LDO_PSENSOR,
	CAMERA_LDO_MAX
} CAMERA_LDO_SELECT;


/* DTS state */
typedef enum {
	CAMERA_LDO_GPIO_STATE_ENP0,
	CAMERA_LDO_GPIO_STATE_ENP1,
	CAMERA_LDO_GPIO_STATE_MAX,	/* for array size */
} CAMERA_LDO_GPIO_STATE;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
extern void camera_ldo_gpio_select_state(CAMERA_LDO_GPIO_STATE s);
void camera_ldo_set_ldo_value(CAMERA_LDO_SELECT ldonum,unsigned int value);
void camera_ldo_set_en_ldo(CAMERA_LDO_SELECT ldonum,unsigned int en);



#endif /* _CAMERA_LDO_H_*/
