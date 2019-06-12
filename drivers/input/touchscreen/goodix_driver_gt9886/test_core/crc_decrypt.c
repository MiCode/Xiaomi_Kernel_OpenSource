/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name          : crc_decrypt.cpp
* Author             :
* Version            : V1.0.0
* Date               : 26/03/2018
* Description        : decrypt file
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "crc_decrypt.h"
#include "generic_func.h"
#include "board_opr_interface.h"
#include "simple_mem_manager.h"
#include <linux/string.h>
#include <linux/firmware.h>
#include "../goodix_ts_core.h"
const char default_order_name[] = "goodix_gt9886_limit_f11.tporder";

/*******************************************************************************
* Function Name	: decrypt_reflect
* Description	:
* Input			: u32 ref
				: s8 ch
* Output		:
* Return		: s32(0:Fail 1:ok)
*******************************************************************************/
extern u32 decrypt_reflect(u32 ref, s8 ch)
{
	u32 value = 0;
	s32 i;
	for (i = 1; i < (ch + 1); i++) {
		if (ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	} return value;
}

/*******************************************************************************
* Function Name	: decrypt_init_crc32_table
* Description	: init crc32 table
* Input			: u32 crc32_table[]
* Output		:
* Return		: s32(0:Fail 1:ok)
*******************************************************************************/
extern s32 decrypt_init_crc32_table(u32 *crc32_table)
{
	u32 crc, temp;
	u32 t1, t2;
	u32 flag;
	u32 ulPolynomial = 0x04c11db7;
	s32 i, j;
	for (i = 0; i <= 0xFF; i++) {
		temp = decrypt_reflect(i, 8);
		crc32_table[i] = temp << 24;
		for (j = 0; j < 8; j++) {
			flag = crc32_table[i] & 0x80000000;
			t1 = (crc32_table[i] << 1);
			if (flag == 0)
				t2 = 0;
			else
				t2 = ulPolynomial;
			crc32_table[i] = t1 ^ t2;

		}
		crc = crc32_table[i];
		crc32_table[i] = decrypt_reflect(crc32_table[i], 32);
	}
	return 1;
}

/*******************************************************************************
* Function Name	: decrypt_generate_crc32
* Description	: generate crc32 code
* Input			: u32 crc32_table[]
				: u8* buf
				: u32 len
* Output		:
* Return		: u32()
*******************************************************************************/
extern u32 decrypt_generate_crc32(u32 *crc32_table, u8 *buf, u32 len)
{
	u32 old_crc32 = 0xFFFFFFFF;
	u32 i = 0;
	for (i = 0; i < len; ++i) {
		u32 t = (old_crc32 ^ buf[i]) & 0xFF;
		old_crc32 = ((old_crc32 >> 8) & 0xFFFFFF) ^ crc32_table[t];
	}
	/*old_crc32 = ~old_crc32;*/
	return (~old_crc32) & 0xffffffff;
}

/*******************************************************************************
* Function Name	: read_file_decoder
* Description	: read file and decode file content
* Input			: PST_TP_DEV p_tp_dev
				: s8* ver_date
				: s8* file_content
* Output		:
* Return		: s32(0:Fail 1:ok)
*******************************************************************************/
extern s32 read_file_decoder(PST_TP_DEV p_tp_dev, s8 **ver_date,
		s8 **file_content)
{
	s32 ret = 0;
	s32 i = 0;
	u32 file_len = 0;
	u32 index_ver = 0;
	u32 index_cont = 0;
	s8 *cont_buf = NULL;
	u32 crc_calc = 0;
	u32 crc_origin = 0;
	u32 crc32_table[256];
	char order_name[64];
	const struct firmware *firmware;
	struct device *dev;
	struct goodix_ts_board_data *ts_bdata = board_data(goodix_core_data);

	dev = (struct device *)p_tp_dev->p_logic_dev;

	if (ts_bdata && ts_bdata->limit_name)
		strlcpy(order_name, ts_bdata->limit_name, sizeof(order_name));
	else
		strlcpy(order_name, default_order_name, sizeof(order_name));
	/*read content*/
	for (i = 0; i < 2; i++) {
		ret = request_firmware(&firmware, order_name, dev);
		if (ret < 0) {
			board_delay_ms(1);
		} else {
			ret = 1;
			break;
		}
	}
	if (i >= 2) {
		board_print_error("request firmware error:%s\n", order_name);
		ret = 0;
		goto DECODE_EXIT;
	}

	file_len = firmware->size;
	board_print_debug("read file len:%d\n", file_len);
	board_print_debug("malloc memory [crc]1!");
	/*malloc memory*/
	ret = alloc_mem_in_heap((void **)&cont_buf, file_len + 1);
	if (cont_buf == NULL) {
		board_print_error("malloc buf error!");
	}
	ret = alloc_mem_in_heap((void **)ver_date, VECTOR_BYTE_LEN + 1);
	ret = alloc_mem_in_heap((void **)file_content, file_len + 1);
	if ((cont_buf == NULL) || (*ver_date == NULL)
		|| (*file_content == NULL)) {
		board_print_error("cont buf malloc error!\n");
		release_firmware(firmware);
		firmware = NULL;
		return 0;
	}
	/*init*/
	cont_buf[file_len] = '\0';
	(*ver_date)[VECTOR_BYTE_LEN] = '\0';
	(*file_content)[file_len] = '\0';
	memcpy(cont_buf, firmware->data, file_len);
	release_firmware(firmware);
	firmware = NULL;
	decrypt_init_crc32_table(crc32_table);
	for (i = 0; i < 256; i++) {
		crc32_table[i] &= 0xffffffff;
	}

	crc_calc = decrypt_generate_crc32(crc32_table, (u8 *) cont_buf,
				file_len - CRC_BYTE_LEN);
	crc_origin += (u8) cont_buf[file_len - 1];
	crc_origin <<= 8;
	crc_origin += (u8) cont_buf[file_len - 2];
	crc_origin <<= 8;
	crc_origin += (u8) cont_buf[file_len - 3];
	crc_origin <<= 8;
	crc_origin += (u8) cont_buf[file_len - 4];
	crc_origin = crc_origin & 0xffffffff;
	if (crc_calc != crc_origin) {
		board_print_error
			("crc check error!crc_calc=%d,crc_origin=%d\n",
			crc_calc, crc_origin);
		ret = 0;
		goto DECODE_EXIT;
	}
	board_print_debug("crc check pass!crc_calc=%d,crc_origin=%d\n", crc_calc, crc_origin);
	for (i = 0; i < file_len - CRC_BYTE_LEN; i++) {
		if (i < VECTOR_BYTE_LEN) {
			(*ver_date)[index_ver++] = ((cont_buf[i] >> 4) & 0xFF);
			(*ver_date)[index_ver++] = (cont_buf[i] & 0xFF);
		} else {
			(*file_content)[index_cont++] = (u8) (cont_buf[i] - 1);
		}
	}
	ret = 1;
DECODE_EXIT:
	free_mem_in_heap(cont_buf);
	return ret;
}

#ifdef __cplusplus
}
#endif
