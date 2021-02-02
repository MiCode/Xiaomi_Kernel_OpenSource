/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name          : test_order.h
* Author             :
* Version            : V1.0.0
* Date               : 03/05/2018
* Description        : test order info
*******************************************************************************/
#ifndef TEST_ORDER_H
#define TEST_ORDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "test_item_def.h"
#include "tp_open_test_param.h"
#include "tp_short_test_param.h"
#include "tp_version_test_param.h"
#include "tp_additional_option_param.h"
#include "tp_dev_def.h"
#include "mxml.h"
#include "config.h"

/***********************************dev info**********************************/

#define IC_NAME_LEN					15
#define MAX_PARAM_LEN 				200

/***********************************test param**********************************/

typedef struct TEST_ITEM_ID_SET {
	u16 test_id;
	PST_SUB_TEST_ITEM_ID_SET p_sub_id_set;
} ST_TEST_ITEM_ID_SET, *PST_TEST_ITEM_ID_SET;

#define MAX_TEST_NUM				20
#define MAX_SUBTEST_NUM				10

typedef struct TEST_PARAM {
	u8 ic_name[15];
	u16 cfg_len;
	u8 *test_cfg;
	u16 send_cfg_option;
	u16 close_hop;	/*1:need close hopping 0:do nothing*/
	u8 mode_sen_line;
	/*test items*/
	u16 test_item_num;
	PST_TEST_ITEM_ID_SET p_test_item_id_set;
	/*rawdata test*/
	PST_RAWDATA_TEST_PARAM p_raw_test_param;
	/*diffdata test*/
	PST_DIFFDATA_TEST_PARAM p_diff_test_param;
	/*short test*/
	PST_SHORT_PARAM p_short_param;
	/*version test*/
	PST_VERSION_PARAM p_version_test_param;
	/*flash test*/
	PST_FLASH_TEST_PARAM p_flash_test_param;
} ST_TEST_PARAM, *PST_TEST_PARAM;

typedef struct ADDR_POS_INFO {
	u16 addr;
	u16 package_id;	/*package_id(Tx),*/
	u16 offset;	/*normandy*/
	u8 lsb;		/*least significant bit*/
	u8 hsb;		/*high significant bit*/
} ST_ADDR_POS_INFO, *PST_ADDR_POS_INFO;

/*************************************Public methods start********************************************/

/*malloc memory for test order param*/
extern int parse_test_order(PST_TEST_PARAM *pp_test_param,
				PST_TP_DEV p_tp_dev);
extern s32 release_test_order(PST_TEST_PARAM *pp_test_param);

/*parse dev info*/
extern s32 parse_init_dev_info(PST_TP_DEV p_tp_dev,
				PST_TEST_PARAM p_test_param);

/*init dev*/
extern s32 header_file_init_dev(PST_TP_DEV p_tp_dev, u8 *cfg, u16 *p_cfg_len);
extern void init_dev_const_param(PST_TP_DEV p_tp_dev);
extern s32 parse_key_data(PST_TP_DEV p_tp_dev, u8 *cfg);
extern s32 parse_cfg(PST_TP_DEV p_tp_dev, u8 *cfg);
extern u32 parse_addr_from_str(PST_TP_DEV p_dev, u8 *string_info, cu8 *config);
extern u32 parse_cfg_value(PST_TP_DEV p_dev, u8 *string_info, u8 *p_config);
extern s32 get_sc_drv_sen_key_num(PST_TP_DEV p_tp_dev,
		u8 key_start_addr, u8 key_en,
		u8 sen_as_key, u8 key_com_port_num,
		u8 max_key_num);
extern s32 disable_hopping(PST_TP_DEV p_tp_dev, u8 *str_info, u8 *cfg,
		u16 cfg_len);

/*parse test param*/
extern s32 parse_test_param(PST_TEST_PARAM *pp_test_param);
extern s32 release_test_param(PST_TEST_PARAM *pp_test_param);

/*parse test config*/
extern s32 parse_order_config(PST_TEST_PARAM p_test_param);
extern s32 release_order_config(PST_TEST_PARAM p_test_param);

/*parse test item id*/
extern s32 parse_test_item_id(PST_TEST_PARAM p_test_param);
extern s32 release_test_item_id(PST_TEST_PARAM p_test_param);

/*parse rawdata test param*/
extern s32 parse_raw_test_param(PST_TEST_PARAM p_test_param,
		PST_SUB_TEST_ITEM_ID_SET p_sub_id_set);
extern u16 parse_need_check_node(PST_RAWDATA_TEST_PARAM
		p_raw_test_param);
extern s32 parse_need_check_key(PST_RAWDATA_TEST_PARAM p_raw_test_param,
		u16 data_len);
extern s32 parse_special_node(PST_RAWDATA_TEST_PARAM p_raw_test_param,
		u16 data_len);
extern s32 parse_raw_max_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param);
extern s32 parse_raw_min_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param);
extern s32 parse_accord_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param);
extern s32 parse_offset_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param);
extern s32 parse_uniform_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param);
extern s32 parse_jitter_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param);
extern s32 parse_key_max_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param);
extern s32 parse_key_min_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param);
extern s32 release_raw_test_param(PST_TEST_PARAM p_test_param);

/*parse diffdata test param*/
extern s32 parse_diff_test_param(PST_TEST_PARAM p_test_param);
extern s32 release_diff_test_param(PST_TEST_PARAM p_test_param);

/*parse short test param*/
extern s32 parse_short_test_param(PST_TEST_PARAM p_test_param);
extern s32 release_short_test_param(PST_TEST_PARAM p_test_param);

/*parse version test param*/
extern s32 parse_version_test_param(PST_TEST_PARAM p_test_param);
extern s32 release_version_test_param(PST_TEST_PARAM p_test_param);

/*parse flash test param*/
extern s32 parse_flash_test_param(PST_TEST_PARAM p_test_param);
extern s32 release_flash_test_param(PST_TEST_PARAM p_test_param);
/*************************************Public methods end********************************************/

#ifdef __cplusplus
}
#endif
#endif
