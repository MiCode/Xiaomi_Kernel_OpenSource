/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2012 MediaTek Inc.
 */

#ifndef __MT_SEC_HAL_H__
#define __MT_SEC_HAL_H__

int masp_hal_sbc_enabled(void);
int masp_hal_get_sbc_checksum(unsigned int *pChecksum);
int masp_hal_set_dm_verity_error(void);
unsigned char masp_hal_secure_algo_init(void);
unsigned char masp_hal_secure_algo_deinit(void);
void masp_hal_secure_algo(unsigned char Direction, unsigned char *ContentAddr,
			  unsigned int ContentLen, unsigned char *CustomSeed,
			  unsigned char *ResText);

#endif				/* __MT_SEC_HAL_H__ */
