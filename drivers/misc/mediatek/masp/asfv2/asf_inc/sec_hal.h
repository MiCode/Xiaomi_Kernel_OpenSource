/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2012 MediaTek Inc.
 */

#ifndef __MT_SEC_HAL_H__
#define __MT_SEC_HAL_H__

enum hacc_user {
	HACC_USER1 = 0,
	HACC_USER2,
	HACC_USER3,
	HACC_USER4
};

/*int masp_hal_get_uuid(unsigned int *uuid);*/
int masp_hal_sbc_enabled(void);
int masp_hal_get_sbc_checksum(unsigned int *pChecksum);
int masp_hal_set_dm_verity_error(void);
unsigned char masp_hal_secure_algo_init(void);
unsigned char masp_hal_secure_algo_deinit(void);
void masp_hal_secure_algo(unsigned char Direction, unsigned char *ContentAddr,
			  unsigned int ContentLen, unsigned char *CustomSeed,
			  unsigned char *ResText);
unsigned int masp_hal_sp_hacc_init(unsigned char *sec_seed, unsigned int size);
unsigned int masp_hal_sp_hacc_blk_sz(void);
unsigned char *masp_hal_sp_hacc_enc(unsigned char *buf, unsigned int size,
				    unsigned char bAC,
				    enum hacc_user user, unsigned char bDoLock);
unsigned char *masp_hal_sp_hacc_dec(unsigned char *buf, unsigned int size,
				    unsigned char bAC,
				    enum hacc_user user, unsigned char bDoLock);

#endif				/* !__MT_SEC_HAL_H__ */
