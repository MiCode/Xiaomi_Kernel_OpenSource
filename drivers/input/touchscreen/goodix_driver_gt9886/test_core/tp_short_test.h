/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : short_test_func.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : We use this file unify a interface for short test
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_SHORT_TEST_H
#define TP_SHORT_TEST_H

#include "tp_dev_def.h"
#include "tp_short_test_param.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHORT_TEST_NONE_ERROR						0x00
#define SHORT_TEST_PARAMETER_ERROR					0x01
#define SHORT_TEST_DOWNLOAD_FW_FAIL					0x02
#define SHORT_TEST_RUN_FW_FAIL						0x03
#define SHORT_TEST_SEND_PARAMETER_FAIL				0x04
#define SHORT_TEST_END_FAIL							0x05
#define SHORT_TEST_EXIST_SHORT						0x06
#define SHORT_TEST_ANALYSIS_RESULT_FAIL				0x07
#define SHORT_TEST_WRITE_LINE_SEQ_FAIL				0x08
#define WAIT_SHORT_CODE_RUN_FINISHED_ERROR			0x09
#define SHORT_TEST_RECOVERY_CHIP_ERROR				0x09
#define DISABLE_DEV_STATUS_FAIL						0x0A

/*//short res*/
/*//bit7=1 means Tx,bit7==0 means Rx*/
#define CHANNEL_TX_FLAG			0x80

/*//exclude 0x7f and 0xff from 0~0xff,so we can use 254 ids stand for channel*/
#define CHANNEL_GND				0x7f
#define CHANNEL_VDD				0xff

#define MAX_SHORT_NUM			15
typedef struct SHORT_TEST_RES {
	ST_TEST_ITEM_RES item_res_header;
	u8 short_num;
	/*//byte1:chn1
	//byte2:chn2
	//byte3~4:resistence,unit 0.1Kom*/
	u8 short_mess_arr[4 * MAX_SHORT_NUM];
} ST_SHORT_TEST_RES, *PST_SHORT_TEST_RES;
typedef const ST_SHORT_TEST_RES CST_SHORT_TEST_RES;
typedef const PST_SHORT_TEST_RES CPST_SHORT_TEST_RES;

/*************************************Public methods start********************************************/
/*//short test*/
extern s32 short_test_func(ptr32 p_data);

extern s32 clear_short_mess_in_res(PST_TEST_ITEM p_test_item);
/*//store short message in result*/
extern s32 store_short_mess_in_res(u8 chn1, u8 chn2, u16 r,
					PST_SHORT_TEST_RES p_short_res);

typedef enum SHORT_CHN_MAP_ENUM {
	TX_RX_SEPERATION = 0x00,
	TX_RX_SHARED = 0x01,
	TX_RX_PACK_ADC = 0x02
} ENUM_CHN_MAP_MODE;
extern void short_test_channel_map(ptr32 p_data,
					ENUM_CHN_MAP_MODE chn_enum);
extern s32 short_test_end(PST_TEST_ITEM p_test_item);

/*************************************Public methods end********************************************/

/*************************************Private methods start********************************************/

/*************************************Private methods end********************************************/
#ifdef __cplusplus
}
#endif
#endif
#endif
