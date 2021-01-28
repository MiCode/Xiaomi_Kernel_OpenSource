// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include "sec_hal.h"
#include "sec_error.h"
#include "sec_boot_lib.h"

/******************************************************************************
 *  DEFINITIONS
 ******************************************************************************/
#define MOD                         "ASF"

/******************************************************************************
 * CHECK IF SECROIMG IS USED
 ******************************************************************************/
unsigned char masp_secro_en(void)
{
	return 0;
}

/******************************************************************************
 * RETURN SECROIMG BLK SIZE
 ******************************************************************************/
unsigned int masp_secro_blk_sz(void)
{
	return masp_hal_sp_hacc_blk_sz();
}

/******************************************************************************
 * RETURN SECROIMG MD LENGTH
 ******************************************************************************/
unsigned int masp_secro_md_len(unsigned char *md_info)
{
	return 0;
}

/******************************************************************************
 * RETURN SECROIMG MD PLAINTEXT DATA
 ******************************************************************************/
unsigned int masp_secro_md_get_data(unsigned char *md_info, unsigned char *buf,
				    unsigned int offset, unsigned int len)
{
	unsigned int ret = 0;

	return ret;
}

/******************************************************************************
 * CHECK IF MODEM AUTH IS NEEDED
 ******************************************************************************/
int sec_modem_auth_enabled(void)
{
	return 0;
}

/**************************************************************************
 *  SECURE BOOT
 **************************************************************************/
int sec_boot_key_init(void)
{
	int ret = SEC_OK;

	return ret;
}

int masp_ccci_signfmt_verify_file(char *file_path, unsigned int *data_offset,
				  unsigned int *data_sec_len)
{
	unsigned int ret = SEC_OK;

	return ret;
}

int masp_ccci_version_info(void)
{
	return CCCI_VERSION;
}

int masp_ccci_is_cipherfmt(int fp_id, unsigned int start_off,
			   unsigned int *img_len)
{
	return 0;
}

int masp_ccci_decrypt_cipherfmt(int fp_id, unsigned int start_off, char *buf,
				unsigned int buf_len,
				unsigned int *data_offset)
{
	return 0;
}
