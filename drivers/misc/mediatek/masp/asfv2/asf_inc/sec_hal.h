/*
 * Copyright (C) 2012 MediaTek, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MT_SEC_HAL_H__
#define __MT_SEC_HAL_H__

typedef enum {
	HACC_USER1 = 0,
	HACC_USER2,
	HACC_USER3,
	HACC_USER4
} HACC_USER;

/*int masp_hal_get_uuid(unsigned int *uuid);*/
int masp_hal_sbc_enabled(void);
int masp_hal_get_sbc_checksum(unsigned int *pChecksum);
unsigned char masp_hal_secure_algo_init(void);
unsigned char masp_hal_secure_algo_deinit(void);
void masp_hal_secure_algo(unsigned char Direction, unsigned char *ContentAddr,
			  unsigned int ContentLen, unsigned char *CustomSeed,
			  unsigned char *ResText);
unsigned int masp_hal_sp_hacc_init(unsigned char *sec_seed, unsigned int size);
unsigned int masp_hal_sp_hacc_blk_sz(void);
unsigned char *masp_hal_sp_hacc_enc(unsigned char *buf, unsigned int size, unsigned char bAC,
				    HACC_USER user, unsigned char bDoLock);
unsigned char *masp_hal_sp_hacc_dec(unsigned char *buf, unsigned int size, unsigned char bAC,
				    HACC_USER user, unsigned char bDoLock);

#endif				/* !__MT_SEC_HAL_H__ */
