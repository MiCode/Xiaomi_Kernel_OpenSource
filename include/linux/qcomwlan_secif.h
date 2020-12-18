/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2011-2013, 2019-2021,The Linux Foundation. All rights reserved. */

#ifndef __QCOM_WLAN_SECIF_H__
#define __QCOM_WLAN_SECIF_H__

#include <crypto/hash.h>

#define CMAC_TLEN 8 /* CMAC TLen = 64 bits (8 octets) */

/*
 * Prototypes for WLAN Security Interface Functions
 */

struct crypto_ahash *
wcnss_wlan_crypto_alloc_ahash(const char *alg_name, u32 type, u32 mask);

int wcnss_wlan_crypto_ahash_digest(struct ahash_request *req);
void wcnss_wlan_crypto_free_ahash(struct crypto_ahash *tfm);
int wcnss_wlan_crypto_ahash_setkey(struct crypto_ahash *tfm,
				   const u8 *key, unsigned int keylen);
struct crypto_ablkcipher *
wcnss_wlan_crypto_alloc_ablkcipher(const char *alg_name, u32 type, u32 mask);
void wcnss_wlan_ablkcipher_request_free(struct ablkcipher_request *req);
void wcnss_wlan_crypto_free_cipher(struct crypto_cipher *tfm);
void wcnss_wlan_crypto_free_ablkcipher(struct crypto_ablkcipher *tfm);
struct crypto_cipher *
wcnss_wlan_crypto_alloc_cipher(const char *alg_name, u32 type, u32 mask);
void wcnss_wlan_cmac_calc_mic(struct crypto_cipher *tfm, u8 *m,
			      u16 length, u8 *mac);

#endif /* __QCOM_WLAN_SECIF_H__ */
