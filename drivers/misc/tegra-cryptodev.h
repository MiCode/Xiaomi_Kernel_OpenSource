/*
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __TEGRA_CRYPTODEV_H
#define __TEGRA_CRYPTODEV_H

#include <crypto/aes.h>

#include <asm-generic/ioctl.h>

/* ioctl arg = 1 if you want to use ssk. arg = 0 to use normal key */
#define TEGRA_CRYPTO_IOCTL_NEED_SSK	_IOWR(0x98, 100, int)
#define TEGRA_CRYPTO_IOCTL_PROCESS_REQ	_IOWR(0x98, 101, int*)
#define TEGRA_CRYPTO_IOCTL_SET_SEED	_IOWR(0x98, 102, int*)
#define TEGRA_CRYPTO_IOCTL_GET_RANDOM	_IOWR(0x98, 103, int*)
#define TEGRA_CRYPTO_IOCTL_GET_SHA	_IOWR(0x98, 104, int*)
#define TEGRA_CRYPTO_IOCTL_RSA_REQ	_IOWR(0x98, 105, int*)

#define TEGRA_CRYPTO_MAX_KEY_SIZE	AES_MAX_KEY_SIZE
#define RSA_KEY_SIZE		512
#define TEGRA_CRYPTO_IV_SIZE	AES_BLOCK_SIZE
#define DEFAULT_RNG_BLK_SZ	16

/* the seed consists of 16 bytes of key + 16 bytes of init vector */
#define TEGRA_CRYPTO_RNG_SEED_SIZE	AES_KEYSIZE_128 + DEFAULT_RNG_BLK_SZ
#define TEGRA_CRYPTO_RNG_SIZE	SZ_16

/* encrypt/decrypt operations */
#define TEGRA_CRYPTO_ECB	BIT(0)
#define TEGRA_CRYPTO_CBC	BIT(1)
#define TEGRA_CRYPTO_OFB	BIT(2)
#define TEGRA_CRYPTO_CTR	BIT(3)
#define TEGRA_CRYPTO_CMAC	BIT(4)
#define TEGRA_CRYPTO_RNG	BIT(5)

/* a pointer to this struct needs to be passed to:
 * TEGRA_CRYPTO_IOCTL_PROCESS_REQ
 */
struct tegra_crypt_req {
	int op; /* e.g. TEGRA_CRYPTO_ECB */
	bool encrypt;
	char key[TEGRA_CRYPTO_MAX_KEY_SIZE];
	int keylen;
	char iv[TEGRA_CRYPTO_IV_SIZE];
	int ivlen;
	u8 *plaintext;
	int plaintext_sz;
	u8 *result;
};

/* pointer to this struct should be passed to:
 * TEGRA_CRYPTO_IOCTL_SET_SEED
 * TEGRA_CRYPTO_IOCTL_GET_RANDOM
 */
struct tegra_rng_req {
	u8 seed[TEGRA_CRYPTO_RNG_SEED_SIZE];
	u8 *rdata; /* random generated data */
	int nbytes; /* random data length */
	int type;
};

struct tegra_rsa_req {
	char *key;
	int keylen;
	char *algo;
	char *message;
	int msg_len;
	int modlen;
	int pub_explen;
	int prv_explen;
	char *result;
};

struct tegra_sha_req {
	char key[TEGRA_CRYPTO_MAX_KEY_SIZE];
	int keylen;
	unsigned char *algo;
	unsigned char *plaintext;
	int plaintext_sz;
	unsigned char *result;
};

#endif
