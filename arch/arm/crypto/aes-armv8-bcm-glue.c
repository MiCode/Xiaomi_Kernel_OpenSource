/*
 * linux/arch/arm64/crypto/aes-armv8-bcm-glue.c - wrapper code for ARMv8 Aarch32 CE
 *
 * Copyright (C) 2014 Mediatek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/neon.h>
#include <crypto/aes.h>
#include <crypto/ablk_helper.h>
#include <crypto/algapi.h>
#include <linux/module.h>
#include <linux/cpufeature.h>

asmlinkage void aes_v8_cbc_encrypt(u8 out[], u8 const in[], u8 const rk[],
				int rounds, int blocks, u8 iv[], int first);
asmlinkage void aes_v8_cbc_decrypt(u8 out[], u8 const in[], u8 const rk[],
				int rounds, int blocks, u8 iv[], int first);

static int cbc_encrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	struct crypto_aes_ctx *ctx = crypto_blkcipher_ctx(desc->tfm);
	int err, first, rounds = 6 + ctx->key_length / 4;
	struct blkcipher_walk walk;
	unsigned int blocks;

	desc->flags &= ~CRYPTO_TFM_REQ_MAY_SLEEP;
	blkcipher_walk_init(&walk, dst, src, nbytes);
	err = blkcipher_walk_virt(desc, &walk);

	kernel_neon_begin();
	for (first = 1; (blocks = (walk.nbytes / AES_BLOCK_SIZE)); first = 0) {
		aes_v8_cbc_encrypt(walk.dst.virt.addr, walk.src.virt.addr,
				(u8 *)ctx->key_enc, rounds, blocks, walk.iv,
				first);
		err = blkcipher_walk_done(desc, &walk, walk.nbytes % AES_BLOCK_SIZE);
	}
	kernel_neon_end();
	return err;
}

static int cbc_decrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	struct crypto_aes_ctx *ctx = crypto_blkcipher_ctx(desc->tfm);
	int err, first, rounds = 6 + ctx->key_length / 4;
	struct blkcipher_walk walk;
	unsigned int blocks;

	desc->flags &= ~CRYPTO_TFM_REQ_MAY_SLEEP;
	blkcipher_walk_init(&walk, dst, src, nbytes);
	err = blkcipher_walk_virt(desc, &walk);

	kernel_neon_begin();
	for (first = 1; (blocks = (walk.nbytes / AES_BLOCK_SIZE)); first = 0) {
		aes_v8_cbc_decrypt(walk.dst.virt.addr, walk.src.virt.addr,
				(u8 *)ctx->key_dec, rounds, blocks, walk.iv,
				first);
		err = blkcipher_walk_done(desc, &walk, walk.nbytes % AES_BLOCK_SIZE);
	}
	kernel_neon_end();
	return err;
}

static struct crypto_alg aes_algs[] = { {
	.cra_name		    = "__cbc-aes-ce" ,
	.cra_driver_name	= "__driver-cbc-aes-ce" ,
	.cra_priority		= 0,
	.cra_flags		    = CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct crypto_aes_ctx),
	.cra_alignmask		= 7,
	.cra_type		    = &crypto_blkcipher_type,
	.cra_module		    = THIS_MODULE,
	.cra_blkcipher = {
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
		.ivsize		    = AES_BLOCK_SIZE,
		.setkey		    = crypto_aes_set_key,
		.encrypt	    = cbc_encrypt,
		.decrypt	    = cbc_decrypt,
	},
}, {
	.cra_name		= "cbc(aes)",
	.cra_driver_name	= "cbc-aes-ce",
	.cra_priority		= 300,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER|CRYPTO_ALG_ASYNC,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct async_helper_ctx),
	.cra_alignmask		= 7,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= ablk_init,
	.cra_exit		= ablk_exit,
	.cra_ablkcipher = {
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.setkey		= ablk_set_key,
		.encrypt	= ablk_encrypt,
		.decrypt	= ablk_decrypt,
	}
} };

static int __init aes_init(void)
{
	return crypto_register_algs(aes_algs, ARRAY_SIZE(aes_algs));
}

static void __exit aes_exit(void)
{
	crypto_unregister_algs(aes_algs, ARRAY_SIZE(aes_algs));
}

module_init(aes_init);
module_exit(aes_exit);

MODULE_DESCRIPTION("Bit sliced AES in CBC modes using CE");
MODULE_AUTHOR("Mediatek Inc.");
MODULE_LICENSE("GPL");
