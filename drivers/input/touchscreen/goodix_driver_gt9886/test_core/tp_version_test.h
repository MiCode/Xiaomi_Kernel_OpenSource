/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : version_test.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : test touch panel version or update firmware
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef VERSION_TEST_H
#define VERSION_TEST_H

#include "tp_version_test_param.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VERSION_TEST_NONE_ERROR					0x00
#define VERSION_TEST_VERSION_MATCH_FAIL			0x01
#define VERSION_TEST_FW_UPDATE_FAIL				0x02
#define VERSION_TEST_VERSIONMATCH_FAIL_AFTER	0x03

/*//---------------------Version Test----------------------//*/

/*//test result*/
typedef struct VERSION_TEST_RES {
	ST_TEST_ITEM_RES item_res_header;
	u8 cur_ver_data_len;
	u8 cur_ver_data[MAX_VERSION_LEN];
} ST_VERSION_TEST_RES, *PST_VERSION_TEST_RES;

extern s32 fw_version_test(ptr32 p_data);
extern s32 fw_update(ptr32 p_data, PST_UPDATA_BIN_PARAM p_update_bin);

typedef struct CHK_FW_RUN_STATE_RES {
	ST_TEST_ITEM_RES item_res_header;
} ST_CHK_FW_RUN_STATE_RES, *PST_CHK_FW_RUN_STATE_RES;
extern s32 check_fw_run_state(ptr32 p_data);

/*//--------------------Error code---------------------//*/
#define GEST_TEST_ERROR_NONE		0x00
#define GEST_TEST_ENTER_DOZE_FAIL	0x01
#define GEST_TEST_EXIT_DOZE_FAIL	0x02
#define GEST_TEST_VER_CMP_ERROR		0x03
typedef struct GESTURE_TEST_RES {
	ST_TEST_ITEM_RES item_res_header;
	u8 ver_buf[MAX_GESTURE_VER_BUF_SIZE];
	u8 ver_buf_len;
} ST_GESTURE_TEST_RES, *PST_GESTURE_TEST_RES;
extern s32 tp_gesture_test(ptr32 p_data);

#ifdef __cplusplus
}
#endif
#endif
#endif
