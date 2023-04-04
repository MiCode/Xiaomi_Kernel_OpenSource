/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __EMI_LEGACY_V0_H__
#define __EMI_LEGACY_V0_H__

#define NO_PROTECTION   0
#define SEC_RW          1
#define SEC_RW_NSEC_R   2
#define SEC_RW_NSEC_W   3
#define SEC_R_NSEC_R    4
#define FORBIDDEN       5
#define SEC_R_NSEC_RW   6

#define LOCK	1
#define UNLOCK	0

enum {
	MASTER_APMCU = 0,
	MASTER_MM = 1,
	MASTER_INFRA = 2,
	MASTER_MDMCU = 3,
	MASTER_MDDMA = 4,
	MASTER_MFG = 5,
	MASTER_ALL = 6
};

#define SET_ACCESS_PERMISSION(lock, d7, d6, d5, d4, d3, d2, d1, d0) \
((((unsigned int) d7) << 21) | (((unsigned int) d6) << 18) | (((unsigned int) d5) << 15) | \
(((unsigned int) d4)  << 12) | (((unsigned int) d3) <<  9) | (((unsigned int) d2) <<  6) | \
(((unsigned int) d1)  <<  3) | ((unsigned int) d0) | ((unsigned long long) lock << 26))
int emi_mpu_set_region_protection(unsigned long long start_addr,
	unsigned long long end_addr, int region, unsigned int access_permission);

int BM_GetEmiDcm(void);
int BM_SetEmiDcm(const unsigned int setting);
int BM_SetBW(const unsigned int BW_config);
unsigned int BM_GetBW(void);

void dump_last_bm(char *buf, unsigned int leng);
void dump_emi_outstanding(void);

#endif  /* !__EMI_LEGACY_V0_H__ */
