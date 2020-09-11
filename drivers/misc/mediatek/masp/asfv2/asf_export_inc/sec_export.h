/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef SEC_EXPORT_H
#define SEC_EXPORT_H

/**************************************************************************
 *  Security Module ERROR CODE
 **************************************************************************/
#define SEC_OK                                  0x0000

/* IMAGE CIPHER */
#define ERR_IMAGE_CIPHER_KEY_ERR                0x1000
#define ERR_IMAGE_CIPHER_IMG_NOT_FOUND          0x1001
#define ERR_IMAGE_CIPHER_READ_FAIL              0x1002
#define ERR_IMAGE_CIPHER_WRONG_OPERATION        0x1003
#define ERR_IMAGE_CIPHER_DEC_TEST_ERROR         0x1004
#define ERR_IMAGE_CIPHER_ENC_TEST_ERROR         0x1005
#define ERR_IMAGE_CIPHER_HEADER_NOT_FOUND       0x1006
#define ERR_IMAGE_CIPHER_DEC_Fail               0x1007

/* AUTH */
#define ERR_AUTH_IMAGE_VERIFY_FAIL              0x2000
#define ERR_AES_KEY_NOT_FOUND                   0x2005

/* LIB */
#define ERR_LIB_SEC_CFG_NOT_EXIST               0x3000
#define ERR_LIB_VER_INVALID                     0x3001
#define ERR_LIB_SEC_CFG_ERASE_FAIL              0x3002
#define ERR_LIB_SEC_CFG_CANNOT_WRITE            0x3003

/* SECURE DOWNLOAD / IMAGE VERIFICATION */
#define ERR_IMG_VERIFY_THIS_IMG_INFO_NOT_EXIST  0x4000
#define ERR_IMG_VERIFY_HASH_COMPARE_FAIL        0x4001
#define ERR_IMG_VERIFY_NO_SPACE_ADD_IMG_INFO    0x4002
#define ERR_SEC_DL_TOKEN_NOT_FOUND_IN_IMG       0x4003
#define ERR_SEC_DL_FLOW_ERROR                   0x4004

/* IMAGE DOWNLOAD LOCK */
#define ERR_IMG_LOCK_TABLE_NOT_EXIST            0x5000
#define ERR_IMG_LOCK_ALL_LOCK                   0x5001
#define ERR_IMG_LOCK_NO_SPACE_ADD_LOCK_INFO     0x5002
#define ERR_IMG_LOCK_THIS_IMG_INFO_NOT_EXIST    0x5003
#define ERR_IMG_LOCK_MAGIC_ERROR                0x5004

/* KERNEL DRIVER */
#define ERR_KERNEL_CRYPTO_INVALID_MODE          0xA000

/**************************************************************************
 *  Security Module Export API
 **************************************************************************/
int masp_boot_init(void);
void masp_secure_algo(unsigned char Direction,
		      unsigned char *ContentAddr,
		      unsigned int ContentLen,
		      unsigned char *CustomSeed,
		      unsigned char *ResText);
unsigned char masp_secure_algo_init(void);
unsigned char masp_secure_algo_deinit(void);
int masp_ccci_signfmt_verify_file(char *file_path,
				  unsigned int *data_offset,
				  unsigned int *data_sec_len);
int masp_ccci_version_info(void);
int masp_ccci_is_cipherfmt(int fp_id, unsigned int start_off,
			   unsigned int *img_len);
int masp_ccci_decrypt_cipherfmt(int fp_id,
				unsigned int start_off,
				char *buf,
				unsigned int buf_len,
				unsigned int *data_offset);
unsigned char masp_secro_en(void);
unsigned int masp_secro_md_len(unsigned char *md_info);
unsigned int masp_secro_md_get_data(unsigned char *md_info,
				    unsigned char *buf,
				    unsigned int offset, unsigned int len);
unsigned int masp_secro_blk_sz(void);

#endif				/* SEC_EXPORT_H */
