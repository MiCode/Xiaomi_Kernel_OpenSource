/*
 * Raydium RM31080 touchscreen header
 *
 * Copyright (C) 2012-2013, Raydium Semiconductor Corporation.
 * Copyright (C) 2012-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef _RM31080A_TS_H_
#define _RM31080A_TS_H_


#define ENABLE_MANUAL_IDLE_MODE			0

#define PARAMETER_AMOUNT				384

/***************************************************************************
 *	Kernel CTRL Define
 *	DO NOT MODIFY
 *	NOTE: Need to sync with HAL
 ***************************************************************************/
#define OK						1
#define FAIL					0
#define DEBUG_DRIVER			0x01

#define RM_IOCTL_REPORT_POINT				0x1001
#define RM_IOCTL_SET_HAL_PID				0x1002
#define RM_IOCTL_INIT_START					0x1003
#define RM_IOCTL_INIT_END					0x1004
#define RM_IOCTL_FINISH_CALC				0x1005
#define RM_IOCTL_SCRIBER_CTRL				0x1006
#define RM_IOCTL_READ_RAW_DATA				0x1007
#define RM_IOCTL_AUTOSCAN_CTRL				0x1008
#define RM_IOCTL_GET_PARAMETER				0x100A
#define RM_IOCTL_SET_VARIABLE				0x1010
#define RM_VARIABLE_SELF_TEST_RESULT			0x01
#define RM_VARIABLE_SCRIBER_FLAG				0x02
#define RM_VARIABLE_AUTOSCAN_FLAG				0x03
#define RM_VARIABLE_VERSION						0x04
#define RM_VARIABLE_IDLEMODECHECK				0x05
#define RM_VARIABLE_REPEAT						0x06
#define RM_VARIABLE_WATCHDOG_FLAG				0x07
#define RM_VARIABLE_TEST_VERSION				0x08
#define RM_IOCTL_GET_VARIABLE				0x1011
#define RM_VARIABLE_PLATFORM_ID					0x01
#define RM_VARIABLE_GPIO_SELECT					0x02
#define RM_IOCTL_GET_SACN_MODE				0x1012
#define RM_IOCTL_SET_KRL_TBL				0x1013
#define RM_IOCTL_WATCH_DOG					0x1014

#define RM_INPUT_RESOLUTION_X				4096
#define RM_INPUT_RESOLUTION_Y				4096

#define RM_TS_SIGNAL					44
#define RM_TS_MAX_POINTS				16

#define RM_SIGNAL_INTR						0x00000001
#define RM_SIGNAL_SUSPEND					0x00000002
#define RM_SIGNAL_RESUME					0x00000003
#define RM_SIGNAL_CHANGE_PARA				0x00000004
#define RM_SIGNAL_WATCH_DOG_CHECK			0x00000005
#define RM_SIGNAL_PARA_SMOOTH				0x00
#define RM_SIGNAL_PARA_SELF_TEST			0x01


#define RM_SELF_TEST_STATUS_FINISH			0
#define RM_SELF_TEST_STATUS_TESTING			1
#define RM_SELF_TEST_RESULT_FAIL			0
#define RM_SELF_TEST_RESULT_PASS			1

/****************************************************************************
 * Platform define
 ***************************************************************************/
#define RM_PLATFORM_K007	0x00
#define RM_PLATFORM_K107	0x01
#define RM_PLATFORM_C210	0x02
#define RM_PLATFORM_D010	0x03
#define RM_PLATFORM_P005	0x04
#define RM_PLATFORM_R005	0x05
#define RM_PLATFORM_RAYPRJ	0x80

/***************************************************************************
 *	DO NOT MODIFY - Kernel CTRL Define
 *	NOTE: Need to sync with HAL
 ***************************************************************************/



/***************************************************************************
 *	Kernel Command Set
 *	DO NOT MODIFY
 *	NOTE: Need to sync with HAL
 ***************************************************************************/
#define KRL_TBL_CMD_LEN					3

#define KRL_INDEX_FUNC_SET_IDLE			0
#define KRL_INDEX_FUNC_PAUSE_AUTO		1
#define KRL_INDEX_RM_START				2
#define KRL_INDEX_RM_END				3
#define KRL_INDEX_RM_READ_IMG			4
#define KRL_INDEX_RM_WATCHDOG			5
#define KRL_INDEX_RM_TESTMODE			6
#define KRL_INDEX_RM_SLOWSCAN			7

#define KRL_SIZE_SET_IDLE				128
#define KRL_SIZE_PAUSE_AUTO				64
#define KRL_SIZE_RM_START				64
#define KRL_SIZE_RM_END					64
#define KRL_SIZE_RM_READ_IMG			64
#define KRL_SIZE_RM_WATCHDOG			96
#define KRL_SIZE_RM_TESTMODE			96
#define KRL_SIZE_RM_SLOWSCAN			128

#define KRL_TBL_FIELD_POS_LEN_H					0
#define KRL_TBL_FIELD_POS_LEN_L					1
#define KRL_TBL_FIELD_POS_CASE_NUM				2
#define KRL_TBL_FIELD_POS_CMD_NUM				3

#define KRL_CMD_READ						0x11
#define KRL_CMD_WRITE_W_DATA				0x12
#define KRL_CMD_WRITE_WO_DATA				0x13
#define KRL_CMD_AND							0x18
#define KRL_CMD_OR							0x19
#define KRL_CMD_NOT							0x1A
#define KRL_CMD_XOR							0x1B

#define KRL_CMD_SEND_SIGNAL					0x20
#define KRL_CMD_CONFIG_RST					0x21
#define KRL_SUB_CMD_SET_RST_GPIO				0x00
#define KRL_SUB_CMD_SET_RST_VALUE				0x01
#define KRL_CMD_SET_TIMER					0x22
#define KRL_SUB_CMD_INIT_TIMER					0x00
#define KRL_SUB_CMD_ADD_TIMER					0x01
#define KRL_SUB_CMD_DEL_TIMER					0x02
#define KRL_CMD_CONFIG_3V3					0x23
#define KRL_SUB_CMD_SET_3V3_GPIO				0x00
#define KRL_SUB_CMD_SET_3V3_REGULATOR			0x01
#define KRL_CMD_CONFIG_1V8					0x24
#define KRL_SUB_CMD_SET_1V8_GPIO				0x00
#define KRL_SUB_CMD_SET_1V8_REGULATOR			0x01
#define KRL_CMD_CONFIG_CLK					0x25
#define KRL_SUB_CMD_SET_CLK						0x00

#define KRL_CMD_USLEEP						0x40
#define KRL_CMD_MSLEEP						0x41

#define KRL_CMD_FLUSH_QU					0x52
#define KRL_SUB_CMD_SENSOR_QU					0x00
#define KRL_SUB_CMD_TIMER_QU					0x01

#define KRL_CMD_READ_IMG					0x60

/***************************************************************************
 *	DO NOT MODIFY - Kernel Command Set
 *	NOTE: Need to sync with HAL
 ***************************************************************************/

typedef struct {
	unsigned char ucTouchCount;
	unsigned char ucID[RM_TS_MAX_POINTS];
	unsigned short usX[RM_TS_MAX_POINTS];
	unsigned short usY[RM_TS_MAX_POINTS];
	unsigned short usZ[RM_TS_MAX_POINTS];
} rm_touch_event;

struct rm_spi_ts_platform_data {
	int gpio_reset;
	int gpio_1v8;
	int gpio_3v3;
	int x_size;
	int y_size;
	unsigned char *config;
	int platform_id;
	unsigned char *name_of_clock;
	unsigned char *name_of_clock_con;
/* wait to be implemented...
	int gpio_sensor_select0;
	int gpio_sensor_select1;
*/
};

int rm_tch_spi_byte_write(unsigned char u8Addr, unsigned char u8Value);
int rm_tch_spi_byte_read(unsigned char u8Addr, unsigned char *pu8Value);
int rm_tch_spi_burst_write(unsigned char *pBuf, unsigned int u32Len);
void rm_tch_set_autoscan(unsigned char val);

#endif				/*_RM31080A_TS_H_*/
