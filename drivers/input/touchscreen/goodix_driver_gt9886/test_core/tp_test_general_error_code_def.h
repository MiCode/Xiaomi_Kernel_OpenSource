/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_test_general_error_code_def.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 03/30/2018
* Description        : general error code start with 0x8000
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_TEST_GENERAL_ERROR_CODE_DEF_H
#define TP_TEST_GENERAL_ERROR_CODE_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

/*//--------------------------------------------test item error code define start------------------------------------------------//*/
#define ERROR_CODE_NONE					0	/*//ok*/
#define PROCESS_BREAK_OUT				0x8001	/*//process break out*/
#define PARAMETER_ERRROR				0x8002	/*//parameter error*/
#define COMMUNICATION_ERROR				0x8003	/*//communication error*/
#define MEMORY_ACCESS_ERROR				0x8004	/*//memory access error*/
#define POINTER_ERROR					0x8005	/*//pointer error*/
#define TEST_BEFORE_FUNC_FAIL			0x8006	/*//test before function fail*/
#define TEST_FINISH_FUNC_FAIL			0x8007	/*//test finshed function fail*/
#define TEST_RST_OPR_FAIL				0x8008	/*//test rst operation fail*/
#define CHIP_TYPE_NOT_EXIST				0x8009	/*//chip type is not exist*/

/*//--------------------------------------------test item error code define start------------------------------------------------//*/

#ifdef __cplusplus
}
#endif
#endif
#endif
