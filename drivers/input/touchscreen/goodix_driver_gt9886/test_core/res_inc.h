/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name			: res_inc.h
* Author			: Bob Huang
* Version			: V1.0.0
* Date				: 01/22/2018
* Description		: resource include
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef RES_INC_H
#define RES_INC_H

/*#ifdef __cplusplus
extern "C" {
#endif*/

#include "user_test_type_def.h"
#include "tp_dev_def.h"
#if ARM_CODE == 1
#ifdef STM32F4XX
#include "stm32f4xx.h"
#endif
#ifdef STM32F1XX
/*STM32F10x Library Definitions*/
#include "Gpio_define.h"
#endif
#elif PC_CODE == 1
#include "TestInclude.h"

#ifdef _DEBUG
#include "MemoryLeakDetected.h"
#define new DEBUG_NEW
#endif

#elif QNX_CODE == 1

#endif
/****************************test board resouce define***************************/
#if ARM_CODE != 1

#define GPIO_Pin_0				((u16)0x0001)	/* Pin 0 selected */
#define GPIO_Pin_1				((u16)0x0002)	/* Pin 1 selected */
#define GPIO_Pin_2				((u16)0x0004)	/* Pin 2 selected */
#define GPIO_Pin_3				((u16)0x0008)	/* Pin 3 selected */
#define GPIO_Pin_4				((u16)0x0010)	/* Pin 4 selected */
#define GPIO_Pin_5				((u16)0x0020)	/* Pin 5 selected */
#define GPIO_Pin_6				((u16)0x0040)	/* Pin 6 selected */
#define GPIO_Pin_7				((u16)0x0080)	/* Pin 7 selected */
#define GPIO_Pin_8				((u16)0x0100)	/* Pin 8 selected */
#define GPIO_Pin_9				((u16)0x0200)	/* Pin 9 selected */
#define GPIO_Pin_10				((u16)0x0400)	/* Pin 10 selected */
#define GPIO_Pin_11				((u16)0x0800)	/* Pin 11 selected */
#define GPIO_Pin_12				((u16)0x1000)	/* Pin 12 selected */
#define GPIO_Pin_13				((u16)0x2000)	/* Pin 13 selected */
#define GPIO_Pin_14				((u16)0x4000)	/* Pin 14 selected */
#define GPIO_Pin_15				((u16)0x8000)	/* Pin 15 selected */
#define GPIO_Pin_All			((u16)0xFFFF)	/* All pins selected */

typedef enum {
	GPIO_Speed_10MHz = 1,
	GPIO_Speed_2MHz,
	GPIO_Speed_50MHz
} GPIOSpeed_TypeDef;

/*Test board Rst pin*/
#define MODULE_SHDN						GPIO_Pin_12
#define MODULE_SHDN_PORT				TPYE_GPIO_C

#define nV_LEVEL_EN_PORT				TPYE_GPIO_B
#define nV_LEVEL_EN_PIN					GPIO_Pin_4

#define G_IC_INT_PORT					TPYE_GPIO_B
#define G_IC_INT_PIN					GPIO_Pin_2

#define G_SPI_NSS_PORT					TPYE_GPIO_B
#define G_SPI_NSS_PIN					GPIO_Pin_12	/*SPI_CS*/

#define ADC_GF_INT_PORT					TPYE_GPIO_B
#define ADC_GF_INT_PIN					GPIO_Pin_1

#define GF_RST_PORT						TPYE_GPIO_B
#define GF_RST_PIN						GPIO_Pin_0

#define G_IC_SHDN_PORT					TPYE_GPIO_B
#define G_IC_SHDN_PIN					GPIO_Pin_11

#define DAC_PROPER_RANGE				(u16)15	/*if (1-x)% ~ (1+x)%  dac output ok*/
#define DVE_TYPE_IIC					0x00;
#define DVE_TYPE_SPI					0x01;
#else
#include "stm32f10x_gpio.h"
#endif

enum {
	TPYE_GPIO_A,
	TPYE_GPIO_B,
	TPYE_GPIO_C,
	TPYE_GPIO_D,
	TPYE_GPIO_E,
	TPYE_GPIO_F,
	TPYE_GPIO_G,
	TPYE_GPIO_H,
};


/*io mode type*/
typedef enum {
	GIO_LOW = 0x00,
	GIO_HIGH = 0x01
} GIOBit_Def;

typedef enum {
	Mode_AIN = 0x00,
	Mode_IPD = 0x28,
	Mode_IPU = 0x48,
	Mode_Out_OD = 0x14,
	Mode_Out_PP = 0x10,
	Mode_AF_OD = 0x1C,
	Mode_AF_PP = 0x18,

	MODE_PD = 0x02,		/*pull down*/
	MODE_PU = 0x03,		/*pull up*/
	Mode_IN_FLOATING = 0x04,	/*floating*/
	MODE_OUTPUT = 0x05	/*output*/
} GIOMode_Def;
/*define INT id*/
#define MODULE_INT_ID               5
/*
// in test board we use pin12 as shdn to reset chip
// how to know group of pin
// in doc we can see PA PB PC PD,for example,
// look for GUITAR TEST TOOL SCH V4.1_20150624.pdf
// we know line 53-PC12/USART5_TX is MODULE_SHDN ,so we can use GPIOC*/
#define MODULE_IC					GPIO_Pin_2
#define MODULE_EN					GPIO_Pin_4
/* PB5 GPIOB*/
#define MODULE_INT					GPIO_Pin_5
#define MODULE_SCL					GPIO_Pin_8
#define MODULE_SDA					GPIO_Pin_9

#define ADC_I_AVDD					GPIO_Pin_6
#define ADC_Is_AVDD					GPIO_Pin_7

/*ADC define*/
#define ADC_V_AVDD_VALUE			((u8)0x01)
#define ADC_V_VDDIO_VALUE			((u8)0x02)
#define ADC_VREF_PIN_VALUE			((u8)0x03)
#define ADC_I_AVDD_VALUE			((u8)0x04)
#define ADC_Is_AVDD_VALUE			((u8)0x05)
#define ADC_I_VDDIO_VALUE			((u8)0x0C)
#define ADC_Is_VDDIO_VALUE			((u8)0x0D)
#define ADC_M_SHDN_VALUE			((u8)0x08)
#define ADC_M_INT_VALUE				((u8)0x09)
#define ADC_M_SCL_VALUE				((u8)0x0A)
#define ADC_M_SDA_VALUE				((u8)0x0B)
#define ADC_GF_RST_VALUE			((u8)0x06)
#define ADC_GF_INT_VALUE			((u8)0x07)

/*ADS1015 define*/
#define ADS_FLAG				0x80
#define ADS_SPI_CS_VALUE		((u8)(0x4 | ADS_FLAG))
#define ADS_SPI_SCK_VALUE		((u8)(0x5 | ADS_FLAG))
#define ADS_SPI_MOSI_VALUE		((u8)(0x6 | ADS_FLAG))
#define ADS_SPI_MISO_VALUE		((u8)(0x7 | ADS_FLAG))

/*#ifdef __cplusplus
}
#endif*/

#endif
#endif
