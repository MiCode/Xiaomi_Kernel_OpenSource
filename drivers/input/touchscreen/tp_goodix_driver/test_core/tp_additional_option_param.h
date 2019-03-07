#ifndef TP_ADDITIONAL_OPTION_PARAM_H
#define TP_ADDITIONAL_OPTION_PARAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"
#include "test_item_def.h"
#include "custom_info_def.h"

/*chip config proc param*/
#define CHIP_CFG_SEND		0x0000
#define CHIP_CFG_CHECKED	0x0001
typedef struct CHIP_CONFIG_PROC_PARAM {
	u16 option;
	u16 cfg_len;
	u8 *config;
} ST_CHIP_CONFIG_PROC_PARAM, *PST_CHIP_CONFIG_PROC_PARAM;

/****************************send special config*******************************/
/*special config param(TP_SEND_SPEC_CFG_ITEM_ID)*/
typedef struct SPEC_CONFIG_PARAM {
	u16 cfg_len;
	u8 *config;
} ST_SPEC_CONFIG_PARAM, *PST_SPEC_CONFIG_PARAM;

/*check config param(TP_CFG_CHECK_TEST_ITEM_ID)*/
typedef ST_CHIP_CONFIG_PROC_PARAM ST_CHECK_CONFIG_PARAM;
typedef PST_CHIP_CONFIG_PROC_PARAM PST_CHECK_CONFIG_PARAM;

/*---------------option----------------*/
#define FLASH_TEST_SEND_CFG_OPT		0x0001
#define FLASH_TEST_DEFAULT_DELAY	0x0002
typedef struct FLASH_TEST_PARAM {
	u16 delay_time;
	u16 option;
	u16 cfg_len;
	u8 *config_arr;
} ST_FLASH_TEST_PARAM, *PST_FLASH_TEST_PARAM;

typedef struct UPDATE_SPEC_FW_PARAM {
	u8 *fw_buf;
	u32 fw_len;
} ST_UPDATE_SPEC_FW_PARAM, *PST_UPDATE_SPEC_FW_PARAM;

/*--------------------Option------------------------*/
#define CUSTOM_INFO_WRITE_OPTION	0x00
#define CUSTOM_INFO_READ_OPTION		0x01
#define CUSTOM_INFO_SMART_OPTION	0x02

typedef struct CUSTOM_INFO_PARAM {
	u8 option;
	u8 info_len;
	PST_CUSTOM_INFO_BYTE p_info;
} ST_CUSTOM_INFO_PARAM, *PST_CUSTOM_INFO_PARAM;

typedef struct CHECK_CHIP_TEST_PARAM {
	u8 reserved1;
} ST_CHECK_CHIP_TEST_PARAM, *PST_CHECK_CHIP_TEST_PARAM;

typedef struct CHECK_MODULE_TEST_PARAM {
	u8 reserved1;
} ST_CHECK_MODULE_TEST_PARAM, *PST_CHECK_MODULE_TEST_PARAM;

typedef struct RECOVER_CHIP_CFG_PARAM {
	u16 cfg_len;
	u8 *config_arr;
} ST_RECOVER_CHIP_CFG_PARAM, *PST_RECOVER_CHIP_CFG_PARAM;

typedef struct SAVE_RES_TO_CHIP_PARAM {
	u8 reserved1;
} ST_SAVE_RES_TO_CHIP_PARAM, *PST_SAVE_RES_TO_CHIP_PARAM;

/*--------------------Option---------------------*/
#define CHIP_UID_PROC_READ_OPT		0x00
#define CHIP_UID_PROC_WRITE_OPT		0x01
#define CHIP_UID_PROC_SMART_OPT		0x02
typedef struct CHIP_UID_PROCESS_PARAM {
	u8 option;
} ST_CHIP_UID_PROCESS_PARAM, *PST_CHIP_UID_PROCESS_PARAM;

#ifdef __cplusplus
}
#endif
#endif
