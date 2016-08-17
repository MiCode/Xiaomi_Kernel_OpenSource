/*
 * drivers/misc/tegra-cryptodev.c
 *
 * crypto dev node for NVIDIA tegra aes hardware
 *
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/uaccess.h>
#include <crypto/rng.h>
#include <crypto/hash.h>
#include <mach/hardware.h>

#include "tegra-cryptodev.h"

#define NBUFS 2
#define XBUFSIZE 8
#define RNG_DRBG 1
#define RNG 0

struct tegra_crypto_ctx {
	struct crypto_ablkcipher *ecb_tfm;
	struct crypto_ablkcipher *cbc_tfm;
	struct crypto_ablkcipher *ofb_tfm;
	struct crypto_ablkcipher *ctr_tfm;
	struct crypto_ablkcipher *cmac_tfm;
	struct crypto_rng *rng;
	struct crypto_rng *rng_drbg;
	u8 seed[TEGRA_CRYPTO_RNG_SEED_SIZE];
	int use_ssk;
};

struct tegra_crypto_completion {
	struct completion restart;
	int req_err;
};

static int alloc_bufs(unsigned long *buf[NBUFS])
{
	int i;

	for (i = 0; i < NBUFS; i++) {
		buf[i] = (void *)__get_free_page(GFP_KERNEL);
		if (!buf[i])
			goto err_free_buf;
	}

	return 0;

err_free_buf:
	while (i-- > 0)
		free_page((unsigned long)buf[i]);

	return -ENOMEM;
}

static void free_bufs(unsigned long *buf[NBUFS])
{
	int i;

	for (i = 0; i < NBUFS; i++)
		free_page((unsigned long)buf[i]);
}

static int tegra_crypto_dev_open(struct inode *inode, struct file *filp)
{
	struct tegra_crypto_ctx *ctx;
	int ret = 0;

	ctx = kzalloc(sizeof(struct tegra_crypto_ctx), GFP_KERNEL);
	if (!ctx) {
		pr_err("no memory for context\n");
		return -ENOMEM;
	}

	ctx->ecb_tfm = crypto_alloc_ablkcipher("ecb-aes-tegra",
		CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC, 0);
	if (IS_ERR(ctx->ecb_tfm)) {
		pr_err("Failed to load transform for ecb-aes-tegra: %ld\n",
			PTR_ERR(ctx->ecb_tfm));
		ret = PTR_ERR(ctx->ecb_tfm);
		goto fail_ecb;
	}

	ctx->cbc_tfm = crypto_alloc_ablkcipher("cbc-aes-tegra",
		CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC, 0);
	if (IS_ERR(ctx->cbc_tfm)) {
		pr_err("Failed to load transform for cbc-aes-tegra: %ld\n",
			PTR_ERR(ctx->cbc_tfm));
		ret = PTR_ERR(ctx->cbc_tfm);
		goto fail_cbc;
	}

	if (tegra_get_chipid() != TEGRA_CHIPID_TEGRA2) {
		ctx->ofb_tfm = crypto_alloc_ablkcipher("ofb-aes-tegra",
			CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC, 0);
		if (IS_ERR(ctx->ofb_tfm)) {
			pr_err("Failed to load transform for ofb-aes-tegra: %ld\n",
				PTR_ERR(ctx->ofb_tfm));
			ret = PTR_ERR(ctx->ofb_tfm);
			goto fail_ofb;
		}

		ctx->ctr_tfm = crypto_alloc_ablkcipher("ctr-aes-tegra",
			CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC, 0);
		if (IS_ERR(ctx->ctr_tfm)) {
			pr_err("Failed to load transform for ctr-aes-tegra: %ld\n",
				PTR_ERR(ctx->ctr_tfm));
			ret = PTR_ERR(ctx->ctr_tfm);
			goto fail_ctr;
		}
	}

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11) {
		ctx->rng_drbg = crypto_alloc_rng("rng_drbg-aes-tegra",
			CRYPTO_ALG_TYPE_RNG, 0);
		if (IS_ERR(ctx->rng_drbg)) {
			pr_err("Failed to load transform for rng_drbg tegra: %ld\n",
				PTR_ERR(ctx->rng_drbg));
			ret = PTR_ERR(ctx->rng_drbg);
			goto fail_rng;
		}
	} else {
		ctx->rng = crypto_alloc_rng("rng-aes-tegra",
			CRYPTO_ALG_TYPE_RNG, 0);
		if (IS_ERR(ctx->rng)) {
			pr_err("Failed to load transform for tegra rng: %ld\n",
				PTR_ERR(ctx->rng));
			ret = PTR_ERR(ctx->rng);
			goto fail_rng;
		}
	}

	filp->private_data = ctx;
	return ret;

fail_rng:
	if (tegra_get_chipid() != TEGRA_CHIPID_TEGRA2)
		crypto_free_ablkcipher(ctx->ctr_tfm);
fail_ctr:
	if (tegra_get_chipid() != TEGRA_CHIPID_TEGRA2)
		crypto_free_ablkcipher(ctx->ofb_tfm);
fail_ofb:
	crypto_free_ablkcipher(ctx->cbc_tfm);

fail_cbc:
	crypto_free_ablkcipher(ctx->ecb_tfm);

fail_ecb:
	kfree(ctx);
	return ret;
}

static int tegra_crypto_dev_release(struct inode *inode, struct file *filp)
{
	struct tegra_crypto_ctx *ctx = filp->private_data;

	crypto_free_ablkcipher(ctx->ecb_tfm);
	crypto_free_ablkcipher(ctx->cbc_tfm);

	if (tegra_get_chipid() != TEGRA_CHIPID_TEGRA2) {
		crypto_free_ablkcipher(ctx->ofb_tfm);
		crypto_free_ablkcipher(ctx->ctr_tfm);
	}

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11)
		crypto_free_rng(ctx->rng_drbg);
	else
		crypto_free_rng(ctx->rng);
	kfree(ctx);
	filp->private_data = NULL;
	return 0;
}

static void tegra_crypt_complete(struct crypto_async_request *req, int err)
{
	struct tegra_crypto_completion *done = req->data;

	if (err != -EINPROGRESS) {
		done->req_err = err;
		complete(&done->restart);
	}
}

static int process_crypt_req(struct tegra_crypto_ctx *ctx, struct tegra_crypt_req *crypt_req)
{
	struct crypto_ablkcipher *tfm;
	struct ablkcipher_request *req = NULL;
	struct scatterlist in_sg;
	struct scatterlist out_sg;
	unsigned long *xbuf[NBUFS];
	int ret = 0, size = 0;
	unsigned long total = 0;
	const u8 *key = NULL;
	struct tegra_crypto_completion tcrypt_complete;

	if (crypt_req->op & TEGRA_CRYPTO_ECB) {
		req = ablkcipher_request_alloc(ctx->ecb_tfm, GFP_KERNEL);
		tfm = ctx->ecb_tfm;
	} else if (crypt_req->op & TEGRA_CRYPTO_CBC) {
		req = ablkcipher_request_alloc(ctx->cbc_tfm, GFP_KERNEL);
		tfm = ctx->cbc_tfm;
	} else if ((crypt_req->op & TEGRA_CRYPTO_OFB) &&
			(tegra_get_chipid() != TEGRA_CHIPID_TEGRA2)) {

		req = ablkcipher_request_alloc(ctx->ofb_tfm, GFP_KERNEL);
		tfm = ctx->ofb_tfm;
	} else if ((crypt_req->op & TEGRA_CRYPTO_CTR) &&
			(tegra_get_chipid() != TEGRA_CHIPID_TEGRA2)) {

		req = ablkcipher_request_alloc(ctx->ctr_tfm, GFP_KERNEL);
		tfm = ctx->ctr_tfm;
	}

	if (!req) {
		pr_err("%s: Failed to allocate request\n", __func__);
		return -ENOMEM;
	}

	if ((crypt_req->keylen < 0) || (crypt_req->keylen > AES_MAX_KEY_SIZE)) {
		ret = -EINVAL;
		pr_err("crypt_req keylen invalid");
		goto process_req_out;
	}

	crypto_ablkcipher_clear_flags(tfm, ~0);

	if (!ctx->use_ssk)
		key = crypt_req->key;

	ret = crypto_ablkcipher_setkey(tfm, key, crypt_req->keylen);
	if (ret < 0) {
		pr_err("setkey failed");
		goto process_req_out;
	}

	ret = alloc_bufs(xbuf);
	if (ret < 0) {
		pr_err("alloc_bufs failed");
		goto process_req_out;
	}

	init_completion(&tcrypt_complete.restart);

	ablkcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
		tegra_crypt_complete, &tcrypt_complete);

	total = crypt_req->plaintext_sz;
	while (total > 0) {
		size = min(total, PAGE_SIZE);
		ret = copy_from_user((void *)xbuf[0],
			(void __user *)crypt_req->plaintext, size);
		if (ret) {
			ret = -EFAULT;
			pr_debug("%s: copy_from_user failed (%d)\n", __func__, ret);
			goto process_req_buf_out;
		}
		sg_init_one(&in_sg, xbuf[0], size);
		sg_init_one(&out_sg, xbuf[1], size);

		ablkcipher_request_set_crypt(req, &in_sg,
			&out_sg, size, crypt_req->iv);

		INIT_COMPLETION(tcrypt_complete.restart);
		tcrypt_complete.req_err = 0;
		ret = crypt_req->encrypt ?
			crypto_ablkcipher_encrypt(req) :
			crypto_ablkcipher_decrypt(req);

		if ((ret == -EINPROGRESS) || (ret == -EBUSY)) {
			/* crypto driver is asynchronous */
			ret = wait_for_completion_interruptible(&tcrypt_complete.restart);

			if (ret < 0)
				goto process_req_buf_out;

			if (tcrypt_complete.req_err < 0) {
				ret = tcrypt_complete.req_err;
				goto process_req_buf_out;
			}
		} else if (ret < 0) {
			pr_debug("%scrypt failed (%d)\n",
				crypt_req->encrypt ? "en" : "de", ret);
			goto process_req_buf_out;
		}

		ret = copy_to_user((void __user *)crypt_req->result,
			(const void *)xbuf[1], size);
		if (ret) {
			ret = -EFAULT;
			pr_debug("%s: copy_to_user failed (%d)\n", __func__,
					ret);
			goto process_req_buf_out;
		}

		total -= size;
		crypt_req->result += size;
		crypt_req->plaintext += size;
	}

process_req_buf_out:
	free_bufs(xbuf);
process_req_out:
	ablkcipher_request_free(req);

	return ret;
}

static int sha_async_hash_op(struct ahash_request *req,
				struct tegra_crypto_completion *tr,
				int ret)
{
	if (ret == -EINPROGRESS || ret == -EBUSY) {
		ret = wait_for_completion_interruptible(&tr->restart);
		if (!ret)
			ret = tr->req_err;
		INIT_COMPLETION(tr->restart);
	}
	return ret;
}

static int tegra_crypt_rsa(struct tegra_rsa_req *rsa_req)
{
	struct crypto_ahash *tfm;
	struct ahash_request *req;
	struct scatterlist sg[1];
	char *result = NULL;
	void *hash_buff;
	int ret = 0;
	unsigned long *xbuf[XBUFSIZE];
	struct tegra_crypto_completion rsa_complete;

	tfm = crypto_alloc_ahash(rsa_req->algo, 0, 0);
	if (IS_ERR(tfm)) {
		printk(KERN_ERR "alg: hash: Failed to load transform for %s: %ld\n",
			rsa_req->algo, PTR_ERR(tfm));
		goto alloc_fail;
	}

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		printk(KERN_ERR "alg: hash: Failed to allocate request for %s\n",
			rsa_req->algo);
		goto req_fail;
	}

	ret = alloc_bufs(xbuf);
	 if (ret < 0) {
		pr_err("alloc_bufs failed");
		goto buf_fail;
	}

	init_completion(&rsa_complete.restart);

	result = kzalloc(rsa_req->keylen >> 16, GFP_KERNEL);
	if (!result) {
		pr_err("\nresult alloc fail\n");
		goto result_fail;
	}

	hash_buff = xbuf[0];

	memcpy(hash_buff, rsa_req->message, rsa_req->msg_len);

	sg_init_one(&sg[0], hash_buff, rsa_req->msg_len);

	if (!(rsa_req->keylen))
		goto rsa_fail;

	ret = crypto_ahash_setkey(tfm, rsa_req->key, rsa_req->keylen);

	if (ret) {
		printk(KERN_ERR "alg: hash: setkey failed on: %s\n",
			rsa_req->algo);
		goto rsa_fail;
	}

	ahash_request_set_crypt(req, sg, result, rsa_req->msg_len);

	ret = crypto_ahash_digest(req);

	if (ret == -EINPROGRESS || ret == -EBUSY) {
		ret = wait_for_completion_interruptible(&rsa_complete.restart);
		if (!ret)
			ret = rsa_complete.req_err;
		INIT_COMPLETION(rsa_complete.restart);
	}

	if (ret) {
		pr_err("alg: hash: digest failed for %s\n", rsa_req->algo);
		goto rsa_fail;
	}

	ret = copy_to_user((void __user *)rsa_req->result, (const void *)result,
		crypto_ahash_digestsize(tfm));
	if (ret) {
		ret = -EFAULT;
		pr_err("alg: hash: copy_to_user failed (%d) for %s\n",
				ret, rsa_req->algo);
	}

rsa_fail:
	kfree(result);
result_fail:
	free_bufs(xbuf);
buf_fail:
	ahash_request_free(req);
req_fail:
	crypto_free_ahash(tfm);
alloc_fail:
	return ret;
}

static int tegra_crypto_sha(struct tegra_sha_req *sha_req)
{

	struct crypto_ahash *tfm;
	struct scatterlist sg[1];
	char result[64];
	struct ahash_request *req;
	struct tegra_crypto_completion sha_complete;
	void *hash_buff;
	unsigned long *xbuf[XBUFSIZE];
	int ret = -ENOMEM;

	tfm = crypto_alloc_ahash(sha_req->algo, 0, 0);
	if (IS_ERR(tfm)) {
		printk(KERN_ERR "alg: hash: Failed to load transform for %s: "
		       "%ld\n", sha_req->algo, PTR_ERR(tfm));
		goto out_alloc;
	}

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		printk(KERN_ERR "alg: hash: Failed to allocate request for "
		       "%s\n", sha_req->algo);
		goto out_noreq;
	}

	ret = alloc_bufs(xbuf);
	if (ret < 0) {
		pr_err("alloc_bufs failed");
		goto out_buf;
	}

	init_completion(&sha_complete.restart);

	memset(result, 0, 64);

	hash_buff = xbuf[0];

	memcpy(hash_buff, sha_req->plaintext, sha_req->plaintext_sz);
	sg_init_one(&sg[0], hash_buff, sha_req->plaintext_sz);

	if (sha_req->keylen) {
		crypto_ahash_clear_flags(tfm, ~0);
		ret = crypto_ahash_setkey(tfm, sha_req->key,
					  sha_req->keylen);
		if (ret) {
			printk(KERN_ERR "alg: hash: setkey failed on "
			       " %s: ret=%d\n", sha_req->algo,
			       -ret);
			goto out;
		}
	}

	ahash_request_set_crypt(req, sg, result, sha_req->plaintext_sz);

	ret = sha_async_hash_op(req, &sha_complete, crypto_ahash_init(req));
	if (ret) {
		pr_err("alg: hash: init failed on "
		       "for %s: ret=%d\n", sha_req->algo, -ret);
		goto out;
	}

	ret = sha_async_hash_op(req, &sha_complete, crypto_ahash_update(req));
	if (ret) {
		pr_err("alg: hash: update failed on "
		       "for %s: ret=%d\n", sha_req->algo, -ret);
		goto out;
	}

	ret = sha_async_hash_op(req, &sha_complete, crypto_ahash_final(req));
	if (ret) {
		pr_err("alg: hash: final failed on "
		       "for %s: ret=%d\n", sha_req->algo, -ret);
		goto out;
	}

	ret = copy_to_user((void __user *)sha_req->result,
		(const void *)result, crypto_ahash_digestsize(tfm));
	if (ret) {
		ret = -EFAULT;
		pr_err("alg: hash: copy_to_user failed (%d) for %s\n",
				ret, sha_req->algo);
	}

out:
	free_bufs(xbuf);

out_buf:
	ahash_request_free(req);

out_noreq:
	crypto_free_ahash(tfm);

out_alloc:
	return ret;
}

static long tegra_crypto_dev_ioctl(struct file *filp,
	unsigned int ioctl_num, unsigned long arg)
{
	struct tegra_crypto_ctx *ctx = filp->private_data;
	struct tegra_crypt_req crypt_req;
	struct tegra_rng_req rng_req;
	struct tegra_sha_req sha_req;
	struct tegra_rsa_req rsa_req;
	char *rng;
	int ret = 0;

	switch (ioctl_num) {

	case TEGRA_CRYPTO_IOCTL_NEED_SSK:
		ctx->use_ssk = (int)arg;
		break;

	case TEGRA_CRYPTO_IOCTL_PROCESS_REQ:
		ret = copy_from_user(&crypt_req, (void __user *)arg,
			sizeof(crypt_req));
		if (ret) {
			ret = -EFAULT;
			pr_err("%s: copy_from_user fail(%d)\n", __func__, ret);
			break;
		}

		ret = process_crypt_req(ctx, &crypt_req);
		break;

	case TEGRA_CRYPTO_IOCTL_SET_SEED:
		if (copy_from_user(&rng_req, (void __user *)arg,
			sizeof(rng_req))) {
			ret = -EFAULT;
			pr_err("%s: copy_from_user fail(%d)\n", __func__, ret);
			return ret;
		}

		memcpy(ctx->seed, rng_req.seed, TEGRA_CRYPTO_RNG_SEED_SIZE);

		if (rng_req.type == RNG_DRBG)
			ret = crypto_rng_reset(ctx->rng_drbg, ctx->seed,
				crypto_rng_seedsize(ctx->rng_drbg));
		else
			ret = crypto_rng_reset(ctx->rng, ctx->seed,
				crypto_rng_seedsize(ctx->rng));
		break;

	case TEGRA_CRYPTO_IOCTL_GET_RANDOM:
		if (copy_from_user(&rng_req, (void __user *)arg,
			sizeof(rng_req))) {
			ret = -EFAULT;
			pr_err("%s: copy_from_user fail(%d)\n", __func__, ret);
			return ret;
		}

		rng = kzalloc(rng_req.nbytes, GFP_KERNEL);
		if (!rng) {
			if (rng_req.type == RNG_DRBG)
				pr_err("mem alloc for rng_drbg fail");
			else
				pr_err("mem alloc for rng fail");

			ret = -ENODATA;
			goto rng_out;
		}

		if (rng_req.type == RNG_DRBG)
			ret = crypto_rng_get_bytes(ctx->rng_drbg, rng,
				rng_req.nbytes);
		else
			ret = crypto_rng_get_bytes(ctx->rng, rng,
				rng_req.nbytes);

		if (ret != rng_req.nbytes) {
			if (rng_req.type == RNG_DRBG)
				pr_err("rng_drbg failed");
			else
				pr_err("rng failed");
			ret = -ENODATA;
			goto rng_out;
		}

		ret = copy_to_user((void __user *)rng_req.rdata,
			(const void *)rng, rng_req.nbytes);
		if (ret) {
			ret = -EFAULT;
			pr_err("%s: copy_to_user fail(%d)\n", __func__, ret);
			return ret;
		}

rng_out:
		if (rng)
			kfree(rng);
		break;

	case TEGRA_CRYPTO_IOCTL_GET_SHA:
		if (tegra_get_chipid() != TEGRA_CHIPID_TEGRA2) {
			if (copy_from_user(&sha_req, (void __user *)arg,
				sizeof(sha_req))) {
				ret = -EFAULT;
				pr_err("%s: copy_from_user fail(%d)\n",
						__func__, ret);
				return ret;
			}

			ret = tegra_crypto_sha(&sha_req);
		} else {
			ret = -EINVAL;
		}
		break;

	case TEGRA_CRYPTO_IOCTL_RSA_REQ:
		if (copy_from_user(&rsa_req, (void __user *)arg,
			sizeof(rsa_req))) {
			ret = -EFAULT;
			pr_err("%s: copy_from_user fail(%d)\n", __func__, ret);
			return ret;
		}

		ret = tegra_crypt_rsa(&rsa_req);
		break;

	default:
		pr_debug("invalid ioctl code(%d)", ioctl_num);
		ret = -EINVAL;
	}

	return ret;
}

const struct file_operations tegra_crypto_fops = {
	.owner = THIS_MODULE,
	.open = tegra_crypto_dev_open,
	.release = tegra_crypto_dev_release,
	.unlocked_ioctl = tegra_crypto_dev_ioctl,
};

struct miscdevice tegra_crypto_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tegra-crypto",
	.fops = &tegra_crypto_fops,
};

static int __init tegra_crypto_dev_init(void)
{
	return misc_register(&tegra_crypto_device);
}

late_initcall(tegra_crypto_dev_init);

MODULE_DESCRIPTION("Tegra AES hw device node.");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPLv2");
