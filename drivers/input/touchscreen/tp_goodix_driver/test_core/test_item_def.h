/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : test_item_def.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : define test item id or test item error code or struct
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TEST_ITEM_DEF_H
#define TEST_ITEM_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tp_test_item_id.h"
#include "tp_dev_def.h"

/*test item callback function pointer define*/
typedef s32(*TEST_CALLBACK_PTR) (ptr32);

/*test item callback function pointer define*/
typedef TEST_CALLBACK_PTR TEST_ITEM_FUNC_PTR;

/*sub test item id set*/
typedef struct SUB_TEST_ITEM_ID_SET {
	u8 len;
	u8 *p_id_set;
} ST_SUB_TEST_ITEM_ID_SET, *PST_SUB_TEST_ITEM_ID_SET;

/*test item  define*/
typedef struct TEST_ITEM {
	/*test item id*/
	u16 test_item_id;

	/*sub test item id set*/
	PST_SUB_TEST_ITEM_ID_SET p_sub_id_set;

	/*touch panel device*/
	PST_TP_DEV p_tp_dev;

	/*we use this variable optimal
	//(1)In arm test board platform,this variable store starting address of test item parameter in flash
	//(2)In other platform ,this variable store pointer of test item parameter,now it is not used
	*/
	union {
		u32 item_addr_u32;
		ptr32 item_addr_ptr;
	} item_addr_start;

	/*current data*/
	union {
		u32 n_cur_data;	/*interger data*/
		ptr32 ptr_cur_data;	/*pointer data*/
	} cur_data;

	/*you can use this for parameter limit or other infomation*/
	union {
		u32 n_param;	/*interger data*/
		ptr32 ptr_param;	/*pointer data*/
	} param;

	/*test callback function*/
	TEST_ITEM_FUNC_PTR ptr_general_func;
	ptr32 ptr_general_func_param;

	/*this function may use p_tp_dev to decide some branch
	//the return value 1 means ok,0 means fail
	//the parameter of function is 'TEST_ITEM'
	*/
	TEST_ITEM_FUNC_PTR ptr_test_func;

	/*this function is used to do some word for preparing testing,such as
	//(1)set some hardware
	//(2)get test parameter pointer
	//(3)notify test starting
	//the return value 1 means ok,0 means fail
	*/
	TEST_ITEM_FUNC_PTR ptr_test_item_before_func;
	ptr32 ptr_test_item_before_func_param;

	/*this function is used to do some word for finish testing,such as
	//(1)recover some hardware
	//(2)notify function
	//(3)save test result
	//(4)notify test finished
	//the return value 1 means ok,0 means fail
	*/
	TEST_ITEM_FUNC_PTR ptr_test_item_finished_func;
	ptr32 ptr_test_item_finished_func_param;

	/*test result*/
	union {
		u32 n_res;	/*integer data*/
		ptr32 ptr_res;	/*pointer data*/
	} test_res;

	/*shared data,you can use this data to communicate with other module*/
	ptr32 ptr_share_data;

	/*this pointer to ST_TEST_PROC_DATA*/
	ptr32 ptr_test_proc_data;

	/*this pointer point special struct which contains special callback function
	//you must cast this pointer to you declared struct
	*/
	ptr32 ptr_special_callback_data;

} ST_TEST_ITEM, *PST_TEST_ITEM;

/*test result enum define */
typedef enum TEST_RESULT {
	TEST_OK = 0x00,
	TEST_NG = 0x01,
	TEST_WARNING = 0x02
} ENUM_TEST_RESULT;

/*test status define*/
typedef enum TEST_STATUS {
	TEST_NOT_START = 0x00,
	TEST_START = 0x01,
	TEST_DOING = 0x02,
	TEST_FINISH = 0x03,
	TEST_ABORTED = 0x04,
	TEST_START_PRE = 0x05,
} ENUM_TEST_STATUS;

/*test item error code set*/
typedef struct TEST_ITEM_ERR_CODE_SET {
	u8 item_num;
	u16 *p_item_arr;
} ST_TEST_ITEM_ERR_CODE_SET, *PST_TEST_ITEM_ERR_CODE_SET;

typedef struct ITEM_ERR_CODE {
	u16 len;
	u16 *ptr_err_code_set;
} ST_ITEM_ERR_CODE, *PST_ST_ITEM_ERR_CODE;

/*test item result define*/
typedef struct TEST_ITEM_RES {
	/*test ietm id */
	u16 test_id;

	/*test progress*/
	u16 test_progress;

	/*sub test item id set*/
	PST_SUB_TEST_ITEM_ID_SET p_sub_id_set;

	ENUM_TEST_RESULT test_result;
	ENUM_TEST_STATUS test_status;

	ST_ITEM_ERR_CODE test_item_err_code;

} ST_TEST_ITEM_RES, *P_TEST_ITEM_RES;

/*test process data*/
typedef struct TEST_PROC_DATA {
	u16 test_item_num;
	ENUM_TEST_STATUS test_proc_status;
	u8 test_break_flag;

	PST_TP_DEV p_dev;	/*touc pannel device pointer*/

	PST_TEST_ITEM *pp_test_item_arr;

	TEST_CALLBACK_PTR p_test_items_before_func_ptr;
	ptr32 p_test_items_before_func_ptr_param;
	TEST_CALLBACK_PTR p_test_items_finished_func_ptr;
	ptr32 p_test_items_finished_func_ptr_param;
} ST_TEST_PROC_DATA, *PST_TEST_PROC_DATA;
typedef const ST_TEST_PROC_DATA CST_TEST_PROC_DATA;
typedef const PST_TEST_PROC_DATA CPST_TEST_PROC_DATA;

#ifdef __cplusplus
}
#endif
#endif
#endif
