/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

/*
 * APIs for calling crypto routines from kernel
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

struct crypto_ablkcipher *
wcnss_wlan_crypto_alloc_ablkcipher(const char *alg_name, u32 type, u32 mask)
{
	return crypto_alloc_ablkcipher(alg_name, type, mask);
}
EXPORT_SYMBOL(wcnss_wlan_crypto_alloc_ablkcipher);

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

