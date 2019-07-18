#ifndef VERSION_TEST_PARAM_H
#define VERSION_TEST_PARAM_H

#include "user_test_type_def.h"
#include "test_item_def.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct UPDATA_BIN_PARAM {
	u32 update_bin_len;
	union {
		u32 bin_addr;
		u8 *bin_ptr;
	} update_bin_addr;

} ST_UPDATA_BIN_PARAM, *PST_UPDATA_BIN_PARAM;

/*//ver_cmp_opt */
#define VERSION_CMP_EQUAL			0
#define VERSION_CMP_GREATOR_EQUAL	1
/*test param */
#define MAX_VERSION_LEN 30
typedef struct VERSION_PARAM {
	u8 ver_param_len;
	u8 ver_cmp_opt;
	u8 b_auto_update;
	u8 b_force_update;
	u16 ic_ver_addr;
	u8 ver_param_str[MAX_VERSION_LEN];
	PST_UPDATA_BIN_PARAM p_update_bin_param;
} ST_VERSION_PARAM, *PST_VERSION_PARAM;

/*//---------------------Fw state check----------------------//*/
typedef struct CHK_FW_RUN_STATE_PARAM {
	u8 option;
} ST_CHK_FW_RUN_STATE_PARAM, *PST_CHK_FW_RUN_STATE_PARAM;

/*//---------------------Gesture test----------------------//*/
#define  MAX_GESTURE_VER_BUF_SIZE	10
typedef struct GESTURE_TEST_PARAM {
	u16 ver_addr;
	u8 ver_len;
	u8 option;
	u8 ver_buf[MAX_GESTURE_VER_BUF_SIZE];
} ST_GESTURE_TEST_PARAM, *PST_GESTURE_TEST_PARAM;

#ifdef __cplusplus
}
#endif
#endif
