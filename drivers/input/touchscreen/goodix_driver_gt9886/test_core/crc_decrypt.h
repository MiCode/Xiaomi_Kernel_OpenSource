/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name          : crc_decrypt.h
* Author             :
* Version            : V1.0.0
* Date               : 26/03/2018
* Description        : decrypt file
*******************************************************************************/
#ifndef CRC_DECRYPT_H
#define CRC_DECRYPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"
#include "board_opr_interface.h"

#define CRC_BYTE_LEN			4
#define VECTOR_BYTE_LEN			5

/*************************************Public methods start********************************************/
	extern u32 decrypt_reflect(u32 ref, s8 ch);
	extern s32 decrypt_init_crc32_table(u32 *crc32_table);
	extern u32 decrypt_generate_crc32(u32 *crc32_table, u8 *buf, u32 len);
	extern s32 read_file_decoder(PST_TP_DEV p_tp_dev, s8 **ver_date,
				s8 **file_content);
/*************************************Public methods end********************************************/

#ifdef __cplusplus
}
#endif
#endif
