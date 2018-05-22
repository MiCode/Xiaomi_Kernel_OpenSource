/*
 * aes-ce-cipher.c - core AES cipher using ARMv8 Crypto Extensions
 *
 * Copyright (C) 2013 - 2014 Linaro Ltd <ard.biesheuvel@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <crypto/aes.h>
#include <linux/cpufeature.h>
#include <linux/crypto.h>
#include <linux/module.h>

#include "aes-ce-setkey.h"

MODULE_DESCRIPTION("Synchronous AES cipher using ARMv8 Crypto Extensions");
MODULE_AUTHOR("Ard Biesheuvel <ard.biesheuvel@linaro.org>");
MODULE_LICENSE("GPL v2");

extern void aes_cipher_encrypt(struct crypto_tfm *tfm, u8 dst[], u8 const src[]);
extern void aes_cipher_decrypt(struct crypto_tfm *tfm, u8 dst[], u8 const src[]);

#ifdef CONFIG_CFI_CLANG
static inline void __cfi_aes_cipher_encrypt(struct crypto_tfm *tfm, u8 dst[], u8 const src[])
{
	aes_cipher_encrypt(tfm, dst, src);
}

static inline void __cfi_aes_cipher_decrypt(struct crypto_tfm *tfm, u8 dst[], u8 const src[])
{
	aes_cipher_decrypt(tfm, dst, src);
}

#define aes_cipher_encrypt __cfi_aes_cipher_encrypt
#define aes_cipher_decrypt __cfi_aes_cipher_decrypt
#endif

int ce_aes_setkey(struct crypto_tfm *tfm, const u8 *in_key,
		  unsigned int key_len)
{
	struct crypto_aes_ctx *ctx = crypto_tfm_ctx(tfm);
	int ret;

	ret = ce_aes_expandkey(ctx, in_key, key_len);
	if (!ret)
		return 0;

	tfm->crt_flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
	return -EINVAL;
}
EXPORT_SYMBOL(ce_aes_setkey);

static struct crypto_alg aes_alg = {
	.cra_name		= "aes",
	.cra_driver_name	= "aes-ce",
	.cra_priority		= 250,
	.cra_flags		= CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct crypto_aes_ctx),
	.cra_module		= THIS_MODULE,
	.cra_cipher = {
		.cia_min_keysize	= AES_MIN_KEY_SIZE,
		.cia_max_keysize	= AES_MAX_KEY_SIZE,
		.cia_setkey		= ce_aes_setkey,
		.cia_encrypt		= aes_cipher_encrypt,
		.cia_decrypt		= aes_cipher_decrypt
	}
};

static int __init aes_mod_init(void)
{
	return crypto_register_alg(&aes_alg);
}

static void __exit aes_mod_exit(void)
{
	crypto_unregister_alg(&aes_alg);
}

module_cpu_feature_match(AES, aes_mod_init);
module_exit(aes_mod_exit);
