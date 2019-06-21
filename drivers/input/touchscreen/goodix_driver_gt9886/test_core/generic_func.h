/******************** (C) COPYRIGHT 2017 Goodix ********************
* File Name		: generic_func
* Author		:
* Version		: V1.0.0
* Date			: 07/03/2017
* Description	: init test param interface
*******************************************************************************/
#ifndef GENERIC_FUNC_H
#define GENERIC_FUNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"

#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#define LIB_VERSION					"Goodix Touch Panel Mutiple Production Test Core Version 1.0.2"
#define RESULT_LOG_VERSION			"GTLib01.00.0000.01"
#define MAX_BUFFER_SIZE				15000
#define PATH_LEN					250
#define HEX(a) ((a >= '0' && a <= '9') || (a >= 'A' && a <= 'F') || (a >= 'a' && a <= 'f'))

/*---------------------------------LOG DEFINE---------------------------------------*/
#define BOARD_PRINT(fmt, arg...)	do {\
	if (get_detail_info_flag()) {\
		board_print_info(fmt, ##arg);\
	} \
	else {\
		board_print_debug(fmt, ##arg);\
	} \
} while (0)

/**********************************operator file*************************************/

/*************************************Public methods start********************************************/
extern u16 atohex(u8 *buf, u16 len);
extern s32 getrid_space(s8 *data, s32 len);
extern s32 hexstr_to_array(u8 *array_value, s8 *str_param, s32 len);
extern s32 str_to_int(s8 *dat);
extern int atoi(const char *str);
extern s32 decstr_to_array(u8 *tmp_value, u8 *text, s32 len);

/*get test result value*/
extern s32 get_test_res(void);
/*add the result of test item*/
extern void modify_test_result(s32 test_res);
extern void init_test_res(void);

/*set/get detail info flag*/
extern void set_detail_info_flag(u8 flag);
extern u8 get_detail_info_flag(void);

/*************************************Public methods start********************************************/

#ifdef __cplusplus
}
#endif
#endif
