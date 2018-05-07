/* Copyright (c) 2011-2013, 2015, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/export.h>
#include <linux/qcomwlan_secif.h>
#include <crypto/aes.h>

/* APIs for calling crypto routines from kernel
 */
struct crypto_ahash *wcnss_wlan_crypto_alloc_ahash(const char *alg_name,
						   u32 type, u32 mask)
{
	return crypto_alloc_ahash(alg_name, type, mask);
}
EXPORT_SYMBOL(wcnss_wlan_crypto_alloc_ahash);

int wcnss_wlan_crypto_ahash_digest(struct ahash_request *req)
{
	return crypto_ahash_digest(req);
}
EXPORT_SYMBOL(wcnss_wlan_crypto_ahash_digest);

void wcnss_wlan_crypto_free_ahash(struct crypto_ahash *tfm)
{
	crypto_free_ahash(tfm);
}
EXPORT_SYMBOL(wcnss_wlan_crypto_free_ahash);

int wcnss_wlan_crypto_ahash_setkey(struct crypto_ahash *tfm, const u8 *key,
				   unsigned int keylen)
{
	return crypto_ahash_setkey(tfm, key, keylen);
}
EXPORT_SYMBOL(wcnss_wlan_crypto_ahash_setkey);

void wcnss_wlan_ablkcipher_request_free(struct ablkcipher_request *req)
{
	ablkcipher_request_free(req);
}
EXPORT_SYMBOL(wcnss_wlan_ablkcipher_request_free);

void wcnss_wlan_crypto_free_ablkcipher(struct crypto_ablkcipher *tfm)
{
	crypto_free_ablkcipher(tfm);
}
EXPORT_SYMBOL(wcnss_wlan_crypto_free_ablkcipher);

void wcnss_wlan_crypto_free_cipher(struct crypto_cipher *tfm)
{
	crypto_free_cipher(tfm);
}
EXPORT_SYMBOL(wcnss_wlan_crypto_free_cipher);

struct crypto_cipher *
wcnss_wlan_crypto_alloc_cipher(const char *alg_name, u32 type, u32 mask)
{
	return crypto_alloc_cipher(alg_name, type, mask);
}
EXPORT_SYMBOL(wcnss_wlan_crypto_alloc_cipher);

static inline void xor_128(const u8 *a, const u8 *b, u8 *out)
{
	u8 i;

	for (i = 0; i < AES_BLOCK_SIZE; i++)
		out[i] = a[i] ^ b[i];
}

static inline void leftshift_onebit(const u8 *input, u8 *output)
{
	int i, overflow = 0;

	for (i = (AES_BLOCK_SIZE - 1); i >= 0; i--) {
		output[i] = input[i] << 1;
		output[i] |= overflow;
		overflow = (input[i] & 0x80) ? 1 : 0;
	}
}

static void generate_subkey(struct crypto_cipher *tfm, u8 *k1, u8 *k2)
{
	u8 l[AES_BLOCK_SIZE], tmp[AES_BLOCK_SIZE];
	u8 const_rb[AES_BLOCK_SIZE] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87};
	u8 const_zero[AES_BLOCK_SIZE] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	crypto_cipher_encrypt_one(tfm, l, const_zero);

	if ((l[0] & 0x80) == 0) { /* If MSB(l) = 0, then k1 = l << 1 */
		leftshift_onebit(l, k1);
	} else {    /* Else k1 = ( l << 1 ) (+) Rb */
		leftshift_onebit(l, tmp);
		xor_128(tmp, const_rb, k1);
	}

	if ((k1[0] & 0x80) == 0) {
		leftshift_onebit(k1, k2);
	} else {
		leftshift_onebit(k1, tmp);
		xor_128(tmp, const_rb, k2);
	}
}

static inline void padding(u8 *lastb, u8 *pad, u16 length)
{
	u8 j;

	/* original last block */
	for (j = 0; j < AES_BLOCK_SIZE; j++)  {
		if (j < length)
			pad[j] = lastb[j];
		else if (j == length)
			pad[j] = 0x80;
		else
			pad[j] = 0x00;
	}
}

void wcnss_wlan_cmac_calc_mic(struct crypto_cipher *tfm, u8 *m,
			      u16 length, u8 *mac)
{
	u8 x[AES_BLOCK_SIZE], y[AES_BLOCK_SIZE];
	u8 m_last[AES_BLOCK_SIZE], padded[AES_BLOCK_SIZE];
	u8 k1[AES_KEYSIZE_128], k2[AES_KEYSIZE_128];
	int cmpBlk;
	int i, nblocks = (length + 15) / AES_BLOCK_SIZE;

	generate_subkey(tfm, k1, k2);

	if (nblocks == 0) {
		nblocks = 1;
		cmpBlk = 0;
	} else {
		cmpBlk = ((length % AES_BLOCK_SIZE) == 0) ? 1 : 0;
	}

	if (cmpBlk) { /* Last block is complete block */
		xor_128(&m[AES_BLOCK_SIZE * (nblocks - 1)], k1, m_last);
	} else { /* Last block is not complete block */
		padding(&m[AES_BLOCK_SIZE * (nblocks - 1)], padded,
			length % AES_BLOCK_SIZE);
		xor_128(padded, k2, m_last);
	}

	for (i = 0; i < AES_BLOCK_SIZE; i++)
		x[i] = 0;

	for (i = 0; i < (nblocks - 1); i++) {
		xor_128(x, &m[AES_BLOCK_SIZE * i], y); /* y = Mi (+) x */
		crypto_cipher_encrypt_one(tfm, x, y); /* x = AES-128(KEY, y) */
	}

	xor_128(x, m_last, y);
	crypto_cipher_encrypt_one(tfm, x, y);

	memcpy(mac, x, CMAC_TLEN);
}
EXPORT_SYMBOL(wcnss_wlan_cmac_calc_mic);
