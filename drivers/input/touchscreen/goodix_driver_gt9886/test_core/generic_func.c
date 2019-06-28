/******************** (C) COPYRIGHT 2017 Goodix ********************
* File Name          : generic_func.cpp
* Author             :
* Version            : V1.0.0
* Date               : 07/03/2017
* Description        : init test param interface
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "generic_func.h"
#include "board_opr_interface.h"
#include "simple_mem_manager.h"
#include <linux/string.h>

static s32 test_res_all;
static u8 show_detail_info;

/*******************************************************************************
* Function Name	: set_detail_info_flag
* Description	: set detail test info flag
* Input			: u8 flag
* Output		: none
* Return		: none
*******************************************************************************/
extern void set_detail_info_flag(u8 flag)
{
	show_detail_info = flag;
}
/*******************************************************************************
* Function Name	: get_detail_info_flag
* Description	: get detail test info flag
* Input			: none
* Output		: none
* Return		: u8(1:save reult in csv file 0:no need save)
*******************************************************************************/
extern u8 get_detail_info_flag(void)
{
	return show_detail_info;
}

/*******************************************************************************
* Function Name	: getrid_space
* Description	: get rid of space
* Input			: s8* data
				: s32 len
* Output		: none
* Return		: s32 (length of data)
*******************************************************************************/
extern s32 getrid_space(s8 *data, s32 len)
{
	u8 *buf = NULL;
	s32 i;
	u32 count = 0;

	if (alloc_mem_in_heap((void **)&buf, len + 5) == 0)
		return (s32)(-1);
	for (i = 0; i < len; i++) {
		if (data[i] == ' ' || data[i] == '\r'
			|| data[i] == '\n') {
			continue;
		}
		buf[count++] = data[i];
	}
	buf[count++] = '\0';
	memcpy(data, buf, count);
	free_mem_in_heap(buf);
	return count;
}

extern int atoi(const char *str)
{
	int value = 0;
	int sign = 1;
	if (str == NULL)
		return 0;
	while (*str == ' ') {
		str++;
	}
	if (*str == '-') {
		sign = -1;
	}

	if (*str == '-' || *str == '+') {
		str++;
	}
	/*str to int*/
	while (*str >= '0' && *str <= '9') {
		value = value * 10 + *str - '0';
		str++;
	}
	value = value * sign;
	return value;
}
/*******************************************************************************
* Function Name	: atohex
* Description	: alpha to hex
* Input			: s8* buf
				: u16 len
* Output		:
* Return		: u16(0:Fail others:value)
*******************************************************************************/
extern u16 atohex(u8 *buf, u16 len)
{
	u16 value = 0;
	u8 data[10];
	u8 data_len = 0;
	u8 i = 0;
	if (len == 1) {
		data[0] = '0';
		data[1] = buf[0];
		data_len = 2;
	}

	if (len >= 2) {
		if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {
			for (i = 2; i < len; i++) {
				data[data_len] = buf[i];
				data_len++;
			}
		} else {
			for (i = 0; i < len; i++) {
				data[data_len] = buf[i];
				data_len++;
			}
		}
	}

	for (i = 0; i < data_len; i++) {
		if (HEX(data[i])) {
			value = value << 4;
			if (data[i] >= '0' && data[i] <= '9') {
				value |= (data[i] - '0');
			} else if (data[i] >= 'A' && data[i] <= 'F') {
				value |= (data[i] - 'A' + 10);
			} else if (data[i] >= 'a' && data[i] <= 'f') {
				value |= (data[i] - 'a' + 10);
			}
		} else {
			return 0;
			board_print_error("HEX string is illegal!\n");
		}
	}
	return value;
}

/*******************************************************************************
* Function Name	: decstr_to_array
* Description	: dec string to integer array(Only use for one byte)
* Input			: u8* tmp_value
* Input			: u8* txt
* Input			: s32 len(max array_value length)
* Output		:
* Return		: s32(array len)
*******************************************************************************/
extern s32 decstr_to_array(u8 *tmp_value, u8 *text, s32 len)
{
	s32 i = 0;
	u8 *field = NULL;

	field = (u8 *) strsep((char **)&tmp_value, ",");
	while (field != NULL) {
		text[i] = atoi((const char *)field);
		i++;
		field = (u8 *) strsep((char **)&tmp_value, ",");
		if (i >= len) {
			break;
		}
	}
	return i;
}

/*******************************************************************************
* Function Name	: hexstr_to_array
* Description	: hex string to integer array
* Input			: u8* array_value
				: s8* str_param
				: s32 len
* Output		:
* Return		: s32(array len)
*******************************************************************************/
extern s32 hexstr_to_array(u8 *array_value, s8 *str_param, s32 len)
{
	s32 i = 0;
	s32 data_len = 0;
	data_len = getrid_space(str_param, strlen((cstr) str_param));
	for (i = 0; i < (data_len + 1) / 5; i++) {
		if (i >= len) {
			break;
		}
		array_value[i] =
		(u8) atohex((u8 *) &str_param[i * 5], 4);
	}
	return i;
}

/*******************************************************************************
* Function Name	: str_to_int
* Description	: string to integer
* Input			: s8* data
* Output		:
* Return		: s32(value)
*******************************************************************************/
extern s32 str_to_int(s8 *data)
{
	s32 i = 0;
	s32 data_len = 0;
	data_len = getrid_space(data, strlen((cstr) data));
	i = atoi((cstr) data);
	return i;
}

/*******************************************************************************
* Function Name	: init_test_res
* Description	: init test result
* Input			: none
* Output		: none
* Return		: s32(test result)
*******************************************************************************/
extern void init_test_res(void)
{
	test_res_all = 0;
}

/*******************************************************************************
* Function Name	: get_test_res
* Description	: get test result
* Input			: none
* Output		: none
* Return		: s32(test result)
*******************************************************************************/
extern s32 get_test_res(void)
{
	return test_res_all;
}

/*******************************************************************************
* Function Name	: modify_test_result
* Description	: modify test result
* Input			: none
* Output		: none
* Return		: s32(test result)
*******************************************************************************/
extern void modify_test_result(s32 test_res)
{
	test_res_all |= test_res;
}

#ifdef __cplusplus
}
#endif
