#ifndef TP_OPEN_TEST_PARAM_H
#define TP_OPEN_TEST_PARAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"
#include "test_item_def.h"

/*//need check node define*/
typedef struct NEED_CHECK_NODE {
	u16 data_len;
	u8 *p_data_buf;
} ST_NEED_CHECK_NODE, *PST_NEED_CHECK_NODE;

/*//----------accord_option----------*/
#define ACCORD_CALC_UPON	0x0001
#define ACCORD_CALC_DOWN	0x0002
#define ACCORD_CALC_LEFT	0x0004
#define ACCORD_CALC_RIGHT	0x0008

#define DEF_ACCORD_CALC		(ACCORD_CALC_UPON|ACCORD_CALC_DOWN|ACCORD_CALC_LEFT|ACCORD_CALC_RIGHT)

/*//special node define*/
typedef struct SPECIAL_NODE_DEF {
	u16 node_id;
	s32 high_raw_limit;
	s32 low_raw_limit;
	s32 accord_limit;
	s32 offset_limit;
	u16 accord_option;
} ST_SPECIAL_NODE_DEF, *PST_SPECIAL_NODE_DEF;

/*//rawdata special node parameter define*/
typedef struct RAWDATA_SPECIAL_NODE_PARAM {
	u16 data_len;
	PST_SPECIAL_NODE_DEF p_data_buf;
} ST_RAWDATA_SPECIAL_NODE_PARAM, *PST_RAWDATA_SPECIAL_NODE_PARAM;

/*//rawdata limit test parameter*/
typedef struct RAWDATA_LIMIT_PARAM {
	s32 upper_limit;
	s32 lower_limit;
} ST_RAWDATA_LIMIT_PARAM, *PST_RAWDATA_LIMIT_PARAM;

/*//rawdata accord limit test parameter*/
typedef struct RAW_ACCORD_LIMIT_PARAM {
	s32 accord_limit;
	u16 accord_option;
} ST_RAW_ACCORD_LIMIT_PARAM, *PST_RAW_ACCORD_LIMIT_PARAM;

/*//rawdata offset limit test parameter*/
typedef struct RAW_OFFSET_LIMIT_PARAM {
	s32 offset_limit;
} ST_RAW_OFFSET_LIMIT_PARAM, *PST_RAW_OFFSET_LIMIT_PARAM;

/*//rawdata jitter limit test parameter*/
typedef struct RAW_JITTER_LIMIT_PARAM {
	s32 jitter_limit;
} ST_RAW_JITTER_LIMIT_PARAM, *PST_RAW_JITTER_LIMIT_PARAM;

/*//rawdata uniform limit test parameter*/
typedef struct RAW_UNIFORM_LIMIT_PARAM {
	s32 uniform_limit;
} ST_RAW_UNIFORM_LIMIT_PARAM, *PST_RAW_UNIFORM_LIMIT_PARAM;

/*//rawdata key limit test parameter*/
typedef struct NEED_CHECK_KEY {
	u16 len;
	u8 *p_data_buf;
} ST_NEED_CHECK_KEY, *PST_NEED_CHECK_KEY;
typedef struct RAW_KEY_LIMIT_PARAM {
	s32 upper_limit;
	s32 lower_limit;
	PST_NEED_CHECK_KEY p_need_check_key;
} ST_RAW_KEY_LIMIT_PARAM, *PST_RAW_KEY_LIMIT_PARAM;

/*//this struct is used for
(1)rawdata limit test
(2)accord limit test
(3)offset limit test
(4)rawdata jitter test
(5)uniform test
(6)key test
rawdata limit paramter define*/
typedef struct RAWDATA_TEST_PARAM {
	/*//In arm,the pointers follows below equal flash address
	//In Pc,the pointers point special struct*/

	/*//need check node*/
	PST_NEED_CHECK_NODE p_need_check_node;

	/*//special node*/
	PST_RAWDATA_SPECIAL_NODE_PARAM p_special_node;

	/*//rawdata limit parameter*/
	PST_RAWDATA_LIMIT_PARAM p_rawdata_limit;

	/*//rawdata acoord limit parameter*/
	PST_RAW_ACCORD_LIMIT_PARAM p_raw_accord_limit;

	/*//raw offset limit parameter*/
	PST_RAW_OFFSET_LIMIT_PARAM p_raw_offset_limit;

	/*//raw jitter limit parameter*/
	PST_RAW_JITTER_LIMIT_PARAM p_raw_jitter_limit;

	/*//raw uniform limit parameter*/
	PST_RAW_UNIFORM_LIMIT_PARAM p_raw_uniform_limit;

	/*//keydata limit parameter*/
	PST_RAW_KEY_LIMIT_PARAM p_raw_key_limit;

} ST_RAWDATA_TEST_PARAM, *PST_RAWDATA_TEST_PARAM;

/*//diff jitter limit parameter*/
typedef struct DIFF_JITTER_LIMIT_PARAM {
	s32 diff_jitter_limit;
} ST_DIFF_JITTER_LIMIT_PARAM, *PST_DIFF_JITTER_LIMIT_PARAM;

/*//diffdata test parameter*/
typedef struct DIFFDATA_TEST_PARAM {
	PST_DIFF_JITTER_LIMIT_PARAM p_diff_jitter_limit;
} ST_DIFFDATA_TEST_PARAM, *PST_DIFFDATA_TEST_PARAM;

#ifdef __cplusplus
}
#endif
#endif
