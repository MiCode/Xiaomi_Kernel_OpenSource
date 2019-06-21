/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_open_test.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : touch panel circuit open test
data 1-dimension means drv data
data 2-dimension means sen data

*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_OPEN_TEST_H
#define TP_OPEN_TEST_H

#include "tp_open_test_param.h"

#ifdef __cplusplus
extern "C" {
#endif

/*//rawdata test sets error code define*/
#define RAWDATA_TEST_SUCCESSFULLY				0

#define RAWDATA_TEST_BREAK_OUT					1
#define GET_RAW_PARAM_ERR						2
#define SEND_TEST_CONFIG_ERR					3
#define RAWDATA_TEST_ENTER_MODE_FAIL			4
#define RAWDATA_TEST_SUB_ITEM_ERR				5
#define RAWDATA_TEST_GET_BUF_FAIL				6
#define RAWDATA_TEST_GET_DATA_TIME_OUT 			7
#define RAWDATA_TEST_GET_RES_BUF_FAIL			8
#define OPEN_HOPPING_ERR						9
#define ENTER_COORD_MODE_ERR					10
#define SAVE_CUR_DATA_ERR						11

#define RAWDATA_TEST_BEYOND_MAX_LIMIT			12
#define RAWDATA_TEST_BEYOND_MIN_LIMIT			13
#define RAWDATA_TEST_BEYOND_PARAM_LIMIT			14

#define RAWDATA_TEST_BEYOND_KEY_MAX_LIMIT		15
#define RAWDATA_TEST_BEYOND_KEY_MIN_LIMIT		16
#define RAWDATA_TEST_BEYOND_KEY_PARAM_LIMIT		17

#define RAWDATA_TEST_BEYOND_ACCORD_LIMIT		18
#define RAWDATA_TEST_BEYOND_OFFSET_LIMIT		19
#define RAWDATA_TEST_BEYOND_UNIFORM_LIMIT		20
#define RAWDATA_TEST_BEYOND_JITTER_LIMIT		21
#define RAWDATA_TEST_FAIL						22

/*//diffdata error code*/
#define DIFFDATA_TEST_SUCCESSFULLY				0
#define DIFFDATA_TEST_BREAK_OUT					1
#define GET_DIFF_PARAM_ERR						2
#define DIFFDATA_TEST_ENTER_MODE_FAIL			3
#define DIFFDATA_TEST_BEFORE_FUNC_FAIL			4
#define DIFFDATA_TEST_GET_DATA_TIME_OUT			5
#define DIFFTEST_JITTER_BEYOND_LIMIT			6

/*//Max Frame NUm*/
#define TOTAL_FRAME_NUM_MAX						64

/*//current rawdata define*/
typedef struct CUR_RAWDATA {
	u8 cur_frame_id;
	u16 data_len;	/*//rawdata len*/
	u8 *p_data_buf;	/*//storage rawdata by byte*/
} ST_CUR_RAWDATA, *PST_CUR_RAWDATA;

/*//-------------------------------------RAWDATA TEST RESULT SATRT-------------------------------------*/
/*//rawdata test tmp data*/
typedef struct RAW_TMP_DATA {
	u16 len;
	s32 *p_data_buf;
} ST_RAW_TMP_DATA, *PST_RAW_TMP_DATA;

#define OPEN_TEST_PASS					0x0000
#define BEYOND_RAWDATA_UPPER_LIMIT		0x0001
#define BEYOND_RAWDATA_LOWER_LIMIT		0x0002
#define KEY_BEYOND_UPPER_LIMIT			0x1000
#define KEY_BEYOND_LOWER_LIMIT			0x2000
#define BEYOND_ACCORD_LIMIT				0x0004
#define BEYOND_OFFSET_LIMIT				0x0008
#define BEYOND_JITTER_LIMIT				0x0010
#define BEYOND_UNIFORMITY_LIMIT			0x00800000
#define FIRST_TEST_FLAG					0x80000000
/*//rawdata test result*/
typedef struct RAWDATA_TEST_RES {
	ST_TEST_ITEM_RES item_res_header;
	s32 raw_avg;
	u8 total_frame_cnt;

	PST_RAW_TMP_DATA p_max_raw_tmp_data;
	PST_RAW_TMP_DATA p_min_raw_tmp_data;
	/*//test item test fail count*/
	u8 *p_beyond_rawdata_upper_limit_cnt;
	u8 *p_beyond_rawdata_lower_limit_cnt;
	u8 *p_beyond_jitter_limit_cnt;
	u8 *p_beyond_accord_limit_cnt;
	u8 *p_beyond_offset_limit_cnt;
	u8 *p_beyond_key_upper_limit_cnt;
	u8 *p_beyond_key_lower_limit_cnt;
	u8 beyond_uniform_limit_cnt;
	/*//the highest bit set if need another test*/
	s32 open_test_result;

} ST_RAWDATA_TEST_RES, *PST_RAWDATA_TEST_RES;

/*//rawdata test special callback set*/
typedef struct RAWDATA_TEST_CALLBACK_SET {
	TEST_CALLBACK_PTR ptr_save_cur_data_func;
	ptr32 ptr_save_cur_data_func_param;
} ST_RAWDATA_TEST_CALLBACK_SET, *PST_RAWDATA_TEST_CALLBACK_SET;
/*//-------------------------------------RAWDATA TEST RESULT END-------------------------------------*/

typedef ST_CUR_RAWDATA ST_CUR_DIFFDATA;
typedef PST_CUR_RAWDATA PST_CUR_DIFFDATA;

typedef ST_RAWDATA_TEST_CALLBACK_SET ST_DIFFDATA_TEST_CALLBACK_SET;
typedef PST_RAWDATA_TEST_CALLBACK_SET PST_DIFFDATA_TEST_CALLBACK_SET;

typedef ST_RAW_TMP_DATA ST_DIFF_TMP_DATA;
typedef PST_RAW_TMP_DATA PST_DIFF_TMP_DATA;

/*//-------------------------------------DIFFDATA TEST RESULT SATRT-------------------------------------*/
/*//diffdata test result*/
typedef struct DIFFDATA_TEST_RES {
	ST_TEST_ITEM_RES item_res_header;
	u8 total_frame_cnt;

	PST_DIFF_TMP_DATA p_max_diff_tmp_data;
} ST_DIFFDATA_TEST_RES, *PST_DIFFDATA_TEST_RES;
/*//-------------------------------------DIFFDATA TEST RESULT END-------------------------------------*/

/*************************************Public methods start********************************************/

/*//-----------------------------rawdata as data source---------------------------*/

/*//rawdata test */
extern s32 rawdata_test_sets_func(ptr32 p_data);

/*//sub test items*/
/*//rawdata test function*/
extern s32 rawdata_max_test_func(ptr32 p_data);
extern s32 rawdata_min_test_func(ptr32 p_data);
/*//accord test function*/
extern s32 rawdata_accord_test_func(ptr32 p_data);
/*//keydata test function*/
extern s32 keydata_max_test_func(ptr32 p_data);
extern s32 keydata_min_test_func(ptr32 p_data);
/*//rawdata jitter test function*/
extern s32 rawdata_jitter_test_func(ptr32 p_data);
/*//rawdata uniform test function*/
extern s32 rawdata_uniform_test_func(ptr32 p_data);
/*//rawdata offset test function*/
extern s32 rawdata_offset_test_func(ptr32 p_data);

/*//get rawdata*/
extern s32 get_rawdata(PST_TP_DEV p_tp_dev, u16 time_out_ms,
				u8 b_filter_data, PST_CUR_RAWDATA p_cur_data);

/*//-----------------------------diffdata as data source---------------------------*/
extern s32 diffdata_test_sets_func(ptr32 p_data);
extern s32 diffdata_jitter_test_func(ptr32 p_data);

/*//get diffdata */
extern s32 get_diffdata(PST_TP_DEV p_dev, u16 time_out_ms,
				u8 b_filter_data, PST_CUR_DIFFDATA p_cur_data);

/*//judge specialized channel whether need check or not*/
extern s32 get_chn_need_check(PST_TEST_ITEM p_test_item, u16 chn_id);

/*//get specialized channel rawdata*/
extern s32 get_chn_rawdata(PST_TEST_ITEM p_test_item, u16 chn_id);
/*//rawdata_limit_test_func parameter*/
extern s32 get_chn_upper_limit(PST_TEST_ITEM p_test_item, u16 chn_id);
extern s32 get_chn_lower_limit(PST_TEST_ITEM p_test_item, u16 chn_id);

extern u8 get_adjacent_node(u16 node_id, PST_TP_DEV p_dev, s16 *p_arr);
/*//---------------accord test------------*/
/*//get chn accord(*1000)*/
extern s32 get_chn_accord(PST_TEST_ITEM p_test_item, u16 chn_id);
/*//1000*/
extern s32 get_chn_accord_limit(PST_TEST_ITEM p_test_item, u16 chn_id);
extern u16 get_chn_accord_option(PST_TEST_ITEM p_test_item, u16 chn_id);

/*//-----------------keydata test--------------------*/
/*//get keydata*/
extern s32 get_keydata(IN PST_TEST_ITEM p_test_item, IN u8 key_id);
/*//judge specialized key whether need check or not*/
extern s32 get_key_need_check(PST_TEST_ITEM p_test_item, u16 key_id);
/*//key parameter limit*/
extern s32 get_key_upper_limit(PST_TEST_ITEM p_test_item, u8 key_id);
extern s32 get_key_lower_limit(PST_TEST_ITEM p_test_item, u8 key_id);

/*//-----------------uniform test-------------------*/
extern s32 get_uniform_value(PST_TEST_ITEM p_test_item);
extern s32 get_uniform_limit(PST_TEST_ITEM p_test_item);

/*//---------------rawdata jitter test---------------*/
extern s32 get_rawdata_jitter_value(PST_TEST_ITEM p_test_item);
extern s32 get_rawdata_jitter_limit(PST_TEST_ITEM p_test_item);
extern s32 get_chn_offset(IN PST_TEST_ITEM p_test_item, IN u16 chn_id);
extern s32 get_chn_offset_limit(IN PST_TEST_ITEM p_test_item,
				IN u16 chn_id);

/*//reshape data*/
extern void reshape_data(u8 data_option, PST_TP_DEV p_tp_dev,
				PST_CUR_RAWDATA p_cur_data);
extern u8 get_data_size(u8 data_option);

/*//get specialized channel diffdata*/
extern s32 get_chn_diffdata(PST_TEST_ITEM p_test_item, u16 chn_id);

/*************************************Public methods end********************************************/
#ifdef __cplusplus
}
#endif
/*************************************private methods start********************************************/
/*//-----------------------------rawdata as data source start---------------------------------*/
/*//get adjacent node*/
#define ADJ_POS_UP			0
#define ADJ_POS_DOWN		1
#define ADJ_POS_LEFT		2
#define ADJ_POS_RIGHT		3
/*************************************private methods end********************************************/
#endif
#endif
