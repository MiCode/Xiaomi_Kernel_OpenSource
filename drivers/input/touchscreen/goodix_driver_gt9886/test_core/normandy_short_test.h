/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : normandy_short_test.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 10/26/2017
* Description        : normandy short test
*******************************************************************************/
#ifndef NORMANDY_SHORT_TEST_H
#define NORMANDY_SHORT_TEST_H

#include "user_test_type_def.h"

#include "test_item_def.h"
#include "tp_dev_control.h"
#include "public_fun.h"

/*------------------------------------define-----------------------------------*/
/*Address*/
#define NORM_SHORT_STA_ADDR					0x5095
#define NORM_ESD_KEY_ADDR					0x20B0
#define NORM_SHORT_TXRX_ADC_THD_ADDR		0x8408
#define NORM_SHORT_DIFF_THD_ADDR			0x840A
#define NORM_ADC_READ_DELAY_ADDR			0x840C

#define NORM_SEN_CONFIG_ADDR				0x8436
#define NORM_DRV_CONFIG_ADDR				0x840E
#define NORM_SHORT_CHECKSUM_ADDR			0x845A

#define NORM_SHORT_TESTEND_ADDR				0x8400
#define NORM_SHORT_TEST_RESULT_ADDR			0x8401
#define NORM_SHORT_NUMBER_ADDR				0x8402
#define NORM_SHORT_DRVSELFCODE_ADDR			0xA8E0
#define NORM_SHORT_DIFFCODE_ADDR			0xA97A
#define NORM_SHORT_TxTx_PIN_NUM_ADDR		0x8460
#define NORM_SHORT_RxRx_PIN_NUM_ADDR		0x91D0
#define NORM_SHORT_TxRx_PIN_NUM_ADDR		0x9CC8

/*Status*/
#define NORM_SHORT_STA_READY				0xAA
#define NORM_SHORT_START_TEST				0x00
#define NORM_SHORT_TX_TEST_END				0x08
#define NORM_SHORT_RX_TEST_END				0x80
#define NORM_SHORT_ALL_TEST_END				0x88

#define NORM_SHORT_HEAD						4
/*------------------------------------define-----------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

extern s32 normandy_short_test(ptr32 p_data);

#ifdef __cplusplus
}
#endif
#endif
