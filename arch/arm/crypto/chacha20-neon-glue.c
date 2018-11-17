/*
 * ChaCha20 (RFC7539) and XChaCha20 stream ciphers, NEON accelerated
 *
 * Copyright (C) 2016 Linaro, Ltd. <ard.biesheuvel@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on:
 * ChaCha20 256-bit cipher algorithm, RFC7539, SIMD glue code
 *
 * Copyright (C) 2015 Martin Willi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <crypto/algapi.h>
#include <crypto/chacha.h>
#include <linux/crypto.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/hwcap.h>
#include <asm/neon.h>
#include <asm/simd.h>

asmlinkage void chacha20_block_xor_neon(u32 *state, u8 *dst, const u8 *src);
asmlinkage void chacha20_4block_xor_neon(u32 *state, u8 *dst, const u8 *src);
asmlinkage void hchacha20_block_neon(const u32 *state, u32 *out);

static void chacha20_dosimd(u32 *state, u8 *dst, const u8 *src,
			    unsigned int bytes)
{
	u8 buf[CHACHA_BLOCK_SIZE];

	while (bytes >= CHACHA_BLOCK_SIZE * 4) {
		chacha20_4block_xor_neon(state, dst, src);
		bytes -= CHACHA_BLOCK_SIZE * 4;
		src += CHACHA_BLOCK_SIZE * 4;
		dst += CHACHA_BLOCK_SIZE * 4;
		state[12] += 4;
	}
	while (bytes >= CHACHA_BLOCK_SIZE) {
		chacha20_block_xor_neon(state, dst, src);
		bytes -= CHACHA_BLOCK_SIZE;
		src += CHACHA_BLOCK_SIZE;
		dst += CHACHA_BLOCK_SIZE;
		state[12]++;
	}
	if (bytes) {
		memcpy(buf, src, bytes);
		chacha20_block_xor_neon(state, buf, buf);
		memcpy(dst, buf, bytes);
	}
}

static int chacha20_neon_stream_xor(struct blkcipher_desc *desc,
				    struct scatterlist *dst,
				    struct scatterlist *src,
				    unsigned int nbytes,
				    struct chacha_ctx *ctx, u8 *iv)
{
	struct blkcipher_walk walk;
	u32 state[16];
	int err;

	blkcipher_walk_init(&walk, dst, src, nbytes);
	err = blkcipher_walk_virt_block(desc, &walk, CHACHA_BLOCK_SIZE);

	crypto_chacha_init(state, ctx, iv);

	while (walk.nbytes >= CHACHA_BLOCK_SIZE) {
		kernel_neon_begin();
		chacha20_dosimd(state, walk.dst.virt.addr, walk.src.virt.addr,
				rounddown(walk.nbytes, CHACHA_BLOCK_SIZE));
		kernel_neon_end();
		err = blkcipher_walk_done(desc, &walk,
					  walk.nbytes % CHACHA_BLOCK_SIZE);
	}

	if (walk.nbytes) {
		kernel_neon_begin();
		chacha20_dosimd(state, walk.dst.virt.addr, walk.src.virt.addr,
				walk.nbytes);
		kernel_neon_end();
		err = blkcipher_walk_done(desc, &walk, 0);
	}
	return err;
}

static int chacha20_neon(struct blkcipher_desc *desc, struct scatterlist *dst,
			 struct scatterlist *src, unsigned int nbytes)
{
	struct chacha_ctx *ctx = crypto_blkcipher_ctx(desc->tfm);
	u8 *iv = desc->info;

	if (nbytes <= CHACHA_BLOCK_SIZE || !may_use_simd())
		return crypto_chacha_crypt(desc, dst, src, nbytes);

	return chacha20_neon_stream_xor(desc, dst, src, nbytes, ctx, iv);
}

static int xchacha20_neon(struct blkcipher_desc *desc, struct scatterlist *dst,
			  struct scatterlist *src, unsigned int nbytes)
{
	struct chacha_ctx *ctx = crypto_blkcipher_ctx(desc->tfm);
	u8 *iv = desc->info;
	struct chacha_ctx subctx;
	u32 state[16];
	u8 real_iv[16];

	if (nbytes <= CHACHA_BLOCK_SIZE || !may_use_simd())
		return crypto_xchacha_crypt(desc, dst, src, nbytes);

	crypto_chacha_init(state, ctx, iv);

	kernel_neon_begin();
	hchacha20_block_neon(state, subctx.key);
	kernel_neon_end();

	memcpy(&real_iv[0], iv + 24, 8);
	memcpy(&real_iv[8], iv + 16, 8);
	return chacha20_neon_stream_xor(desc, dst, src, nbytes, &subctx,
					real_iv);
}

static struct crypto_alg algs[] = {
	{
		.cra_name		= "chacha20",
		.cra_driver_name	= "chacha20-neon",
		.cra_priority		= 300,
		.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
		.cra_blocksize		= 1,
		.cra_type		= &crypto_blkcipher_type,
		.cra_ctxsize		= sizeof(struct chacha_ctx),
		.cra_alignmask		= sizeof(u32) - 1,
		.cra_module		= THIS_MODULE,
		.cra_u			= {
			.blkcipher = {
				.min_keysize	= CHACHA_KEY_SIZE,
				.max_keysize	= CHACHA_KEY_SIZE,
				.ivsize		= CHACHA_IV_SIZE,
				.geniv		= "seqiv",
				.setkey		= crypto_chacha20_setkey,
				.encrypt	= chacha20_neon,
				.decrypt	= chacha20_neon,
			},
		},
	}, {
		.cra_name		= "xchacha20",
		.cra_driver_name	= "xchacha20-neon",
		.cra_priority		= 300,
		.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
		.cra_blocksize		= 1,
		.cra_type		= &crypto_blkcipher_type,
		.cra_ctxsize		= sizeof(struct chacha_ctx),
		.cra_alignmask		= sizeof(u32) - 1,
		.cra_module		= THIS_MODULE,
		.cra_u			= {
			.blkcipher = {
				.min_keysize	= CHACHA_KEY_SIZE,
				.max_keysize	= CHACHA_KEY_SIZE,
				.ivsize		= XCHACHA_IV_SIZE,
				.geniv		= "seqiv",
				.setkey		= crypto_chacha20_setkey,
				.encrypt	= xchacha20_neon,
				.decrypt	= xchacha20_neon,
			},
		},
	},
};

static int __init chacha20_simd_mod_init(void)
{
	if (!(elf_hwcap & HWCAP_NEON))
		return -ENODEV;

	return crypto_register_algs(algs, ARRAY_SIZE(algs));
}

static void __exit chacha20_simd_mod_fini(void)
{
	crypto_unregister_algs(algs, ARRAY_SIZE(algs));
}

module_init(chacha20_simd_mod_init);
module_exit(chacha20_simd_mod_fini);

MODULE_AUTHOR("Ard Biesheuvel <ard.biesheuvel@linaro.org>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS_CRYPTO("chacha20");
MODULE_ALIAS_CRYPTO("chacha20-neon");
MODULE_ALIAS_CRYPTO("xchacha20");
MODULE_ALIAS_CRYPTO("xchacha20-neon");
