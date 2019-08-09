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

#ifndef __TRUSTZONE_SEJ_TA__
#define __TRUSTZONE_SEJ_TA__

#define TZ_CRYPTO_TA_UUID   "0d5fe516-821d-11e2-bdb4-d485645c4311"

/* Data Structure for Crypto TA
 * You should define data structure used both in REE/TEE here
 */
struct ta_crypto_data {
	unsigned int size;
	unsigned char bAC;
	unsigned int user;	/* HACC_USER */
	unsigned char bDoLock;
	unsigned int aes_type;	/* AES_OPS */
	unsigned char bEn;
};

struct ta_secure_algo_data {
	unsigned char direction;
	unsigned int contentAddr;
	unsigned int contentLen;
	unsigned char *customSeed;
	unsigned char *resText;
};

/* Command for Test TA */
#define TZCMD_HACC_INIT     0
#define TZCMD_HACC_INTERNAL 1
#define TZCMD_SECURE_ALGO   2
#define TZCMD_GET_UUID      3

#endif	/* __TRUSTZONE_SEJ_TA_TEST__ */
