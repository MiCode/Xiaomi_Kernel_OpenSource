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

#ifndef __MPU_H__
#define __MPU_H__

#define EMI_MPU_TEST		0

#define EMI_MPU_MAX_CMD_LEN	128
#define EMI_MPU_MAX_TOKEN	4
#define CCCI_STR_MAX_LEN	60

#define NO_PROTECTION	0
#define SEC_RW		1
#define SEC_RW_NSEC_R	2
#define SEC_RW_NSEC_W	3
#define SEC_R_NSEC_R	4
#define FORBIDDEN	5
#define SEC_R_NSEC_RW	6

#define UNLOCK		0
#define LOCK		1

#define EMI_MPU_DGROUP_NUM	(EMI_MPU_DOMAIN_NUM / 8)
#if (EMI_MPU_DGROUP_NUM == 1)
#define SET_ACCESS_PERMISSION(apc_ary, lock, d7, d6, d5, d4, d3, d2, d1, d0) \
do {\
	apc_ary[0] = 0;\
	apc_ary[0] = \
		(((unsigned int) d7) << 21) | (((unsigned int) d6) << 18) | \
		(((unsigned int) d5) << 15) | (((unsigned int) d4) << 12) | \
		(((unsigned int) d3) << 9) | (((unsigned int) d2) << 6) | \
		(((unsigned int) d1) << 3) | ((unsigned int) d0) | \
		((unsigned int) lock << 31); \
} while (0)
#elif (EMI_MPU_DGROUP_NUM == 2)
#define SET_ACCESS_PERMISSION(apc_ary, lock, \
	d15, d14, d13, d12, d11, d10, d9, d8, d7, d6, d5, d4, d3, d2, d1, d0) \
do { \
	apc_ary[1] = \
		(((unsigned int) d15) << 21) | (((unsigned int) d14) << 18) | \
		(((unsigned int) d13) << 15) | (((unsigned int) d12) << 12) | \
		(((unsigned int) d11) << 9) | (((unsigned int) d10) << 6) | \
		(((unsigned int) d9) << 3) | ((unsigned int) d8); \
	apc_ary[0] = \
		(((unsigned int) d7) << 21) | (((unsigned int) d6) << 18) | \
		(((unsigned int) d5) << 15) | (((unsigned int) d4) << 12) | \
		(((unsigned int) d3) << 9) | (((unsigned int) d2) << 6) | \
		(((unsigned int) d1) << 3) | ((unsigned int) d0) | \
		((unsigned int) lock << 31); \
} while (0)
#endif

struct emi_region_info_t {
	unsigned long long start;
	unsigned long long end;
	unsigned int region;
	unsigned int apc[EMI_MPU_DGROUP_NUM];
};

struct mst_tbl_entry {
	u32 master;
	u32 port;
	u32 id_mask;
	u32 id_val;
	const char *note;
	const char *name;
};

extern int is_md_master(unsigned int master_id);
extern void set_ap_region_permission(unsigned int apc[EMI_MPU_DGROUP_NUM]);
extern int emi_mpu_set_protection(struct emi_region_info_t *region_info);

#endif /* __MPU_H__ */
