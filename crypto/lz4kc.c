/*
 * Cryptographic API.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/vmalloc.h>
#include <linux/lzo.h>
#include <linux/lz4k.h>

#define malloc(a) kmalloc(a, GFP_KERNEL)
#define free(a) kfree(a)

struct lz4k_ctx {
	void *lz4k_comp_mem;
};

static int lz4kc_init(struct crypto_tfm *tfm)
{
	struct lz4k_ctx *ctx = crypto_tfm_ctx(tfm);

	ctx->lz4k_comp_mem = vmalloc(LZO1X_MEM_COMPRESS);
	if (!ctx->lz4k_comp_mem)
		return -ENOMEM;

	return 0;
}

static void lz4kc_exit(struct crypto_tfm *tfm)
{
	struct lz4k_ctx *ctx = crypto_tfm_ctx(tfm);

	vfree(ctx->lz4k_comp_mem);
}

static int lz4kc_compress(struct crypto_tfm *tfm, const u8 *src,
			    unsigned int slen, u8 *dst, unsigned int *dlen)
{
	struct lz4k_ctx *ctx = crypto_tfm_ctx(tfm);
	/* static size_t in_size = 0, out_size = 0; */
	size_t tmp_len = *dlen; /* size_t(ulong) <-> uint on 64 bit */
	int err;

	static int count; /*= 0*/

	/* printk("lz4k_compress 2 count = %d\r\n", count); */

	count++;

	err = lz4k_compress(src, slen, dst, &tmp_len, ctx->lz4k_comp_mem);
	/* err = lzo1x_1_compress(src, slen, dst, &tmp_len, ctx->lz4k_comp_mem); */

	if (err != LZO_E_OK)
		return -EINVAL;
#if 0
	if (count%10 == 0) {
		in_size += slen;
		out_size += tmp_len;
		/* printk("lz4k_compress_ubifs result in_size = %d, out_size = %d\n", in_size, out_size); */
	}
#endif
	/* printk("lz4k_compress result in_size = %d, out_size = %d\n", in_size, out_size); */
	/* printk("lz4k_compress result slen = %d, tmp_len = %d\n", slen, tmp_len); */

	*dlen = tmp_len;
	return 0;
}

static int lz4kc_decompress(struct crypto_tfm *tfm, const u8 *src,
			      unsigned int slen, u8 *dst, unsigned int *dlen)
{
	static int count; /* = 0 */
	int err;
	size_t tmp_len = *dlen; /* size_t(ulong) <-> uint on 64 bit */

	/* printk("lz4k_decompress 2count = %d, *dlen = %d", count, *dlen); */

	err = lz4k_decompress_ubifs(src, slen, dst, &tmp_len);
	/* err = lzo1x_decompress_safe(src, slen, dst, &tmp_len); */


	count++;

	if (err != LZO_E_OK)
		return -EINVAL;

	*dlen = tmp_len;
	return 0;

}

static struct crypto_alg alg = {
	.cra_name		= "lz4k",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= sizeof(struct lz4k_ctx),
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(alg.cra_list),
	.cra_init		= lz4kc_init,
	.cra_exit		= lz4kc_exit,
	.cra_u			= { .compress = {
	.coa_compress	= lz4kc_compress,
	.coa_decompress	= lz4kc_decompress }
	}
};

static int __init lz4k_mod_init(void)
{
	return crypto_register_alg(&alg);
}

static void __exit lz4k_mod_fini(void)
{
	crypto_unregister_alg(&alg);
}

module_init(lz4k_mod_init);
module_exit(lz4k_mod_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZ77 with 4K Compression Algorithm");
