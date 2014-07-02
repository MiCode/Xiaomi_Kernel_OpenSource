/* Qcrypto: FIPS 140-2 Selftests
 *
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <crypto/hash.h>
#include <crypto/ctr.h>
#include <crypto/des.h>
#include <crypto/aes.h>
#include <crypto/sha.h>
#include <crypto/hash.h>
#include <crypto/algapi.h>
#include <crypto/aead.h>
#include <crypto/authenc.h>
#include <crypto/scatterwalk.h>
#include <crypto/internal/hash.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <mach/qcrypto.h>
#include "qcryptoi.h"
#include "qcrypto_fips.h"

/*
 * Callback function
 */
static void _fips_cb(struct crypto_async_request *crypto_async_req, int err)
{
	struct _fips_completion *fips_completion = crypto_async_req->data;
	if (err == -EINPROGRESS)
		return;

	fips_completion->err = err;
	complete(&fips_completion->completion);
}

/*
 * Function to prefix if needed
 */
static int _fips_get_alg_cra_name(char cra_name[],
				char *prefix, unsigned int size)
{
	char new_cra_name[CRYPTO_MAX_ALG_NAME];
	strlcpy(new_cra_name, prefix, CRYPTO_MAX_ALG_NAME);
	if (CRYPTO_MAX_ALG_NAME < size + strlen(prefix))
		return -EINVAL;

	strlcat(new_cra_name, cra_name, CRYPTO_MAX_ALG_NAME);
	strlcpy(cra_name, new_cra_name, CRYPTO_MAX_ALG_NAME);
	return 0;
}

/*
 * Sha/HMAC self tests
 */
int _fips_qcrypto_sha_selftest(struct fips_selftest_data *selftest_d)
{
	int rc = 0, err, tv_index = 0, num_tv;
	char *k_out_buf = NULL;
	struct scatterlist fips_sg;
	struct crypto_ahash *tfm;
	struct ahash_request *ahash_req;
	struct _fips_completion fips_completion;

	num_tv = (sizeof(fips_test_vector_sha_hmac)) /
	(sizeof(struct _fips_test_vector_sha_hmac));

	/* One-by-one testing */
	for (tv_index = 0; tv_index < num_tv; tv_index++) {
		k_out_buf = kzalloc(fips_test_vector_sha_hmac[tv_index].diglen,
			GFP_KERNEL);
		if (k_out_buf == NULL) {
			pr_err("qcrypto: Failed to allocate memory for k_out_buf %ld\n",
				PTR_ERR(k_out_buf));
			return -ENOMEM;
		}

		memset(k_out_buf, 0,
			fips_test_vector_sha_hmac[tv_index].diglen);
		init_completion(&fips_completion.completion);

		/* use_sw flags are set in dtsi file which makes
		default Linux API calls to go to s/w crypto instead
		of h/w crypto. This code makes sure that all selftests
		calls always go to h/w, independent of DTSI flags. */
		if (fips_test_vector_sha_hmac[tv_index].klen == 0) {
			if (selftest_d->prefix_ahash_algo)
				if (_fips_get_alg_cra_name(
					fips_test_vector_sha_hmac[tv_index]
					.hash_alg, selftest_d->algo_prefix,
					strlen(
					fips_test_vector_sha_hmac[tv_index]
					.hash_alg)
					)) {
					rc = -1;
					pr_err("Algo Name is too long for tv %d\n",
					tv_index);
					goto clr_buf;
				}
		} else {
			if (selftest_d->prefix_hmac_algo)
				if (_fips_get_alg_cra_name(
					fips_test_vector_sha_hmac[tv_index]
					.hash_alg, selftest_d->algo_prefix,
					strlen(
					fips_test_vector_sha_hmac[tv_index]
					.hash_alg)
					)) {
					rc = -1;
					pr_err("Algo Name is too long for tv %d\n",
					tv_index);
					goto clr_buf;
				}
		}

		tfm = crypto_alloc_ahash(
			fips_test_vector_sha_hmac[tv_index].hash_alg, 0, 0);
		if (IS_ERR(tfm)) {
			pr_err("qcrypto: %s algorithm not found\n",
			fips_test_vector_sha_hmac[tv_index].hash_alg);
			rc = PTR_ERR(tfm);
			goto clr_buf;
		}

		ahash_req = ahash_request_alloc(tfm, GFP_KERNEL);
		if (!ahash_req) {
			pr_err("qcrypto: ahash_request_alloc failed\n");
			rc = -ENOMEM;
			goto clr_tfm;
		}
		rc = qcrypto_ahash_set_device(ahash_req, selftest_d->ce_device);
		if (rc != 0) {
			pr_err("%s qcrypto_cipher_set_device failed with err %d\n",
				__func__, rc);
			goto clr_ahash_req;
		}
		ahash_request_set_callback(ahash_req,
			CRYPTO_TFM_REQ_MAY_BACKLOG,
			_fips_cb, &fips_completion);

		sg_init_one(&fips_sg,
			&fips_test_vector_sha_hmac[tv_index].input[0],
			fips_test_vector_sha_hmac[tv_index].ilen);

		crypto_ahash_clear_flags(tfm, ~0);
		if (fips_test_vector_sha_hmac[tv_index].klen != 0) {
			rc = crypto_ahash_setkey(tfm,
				fips_test_vector_sha_hmac[tv_index].key,
				fips_test_vector_sha_hmac[tv_index].klen);
			if (rc) {
				pr_err("qcrypto: crypto_ahash_setkey failed\n");
				goto clr_ahash_req;
			}
		}

		ahash_request_set_crypt(ahash_req, &fips_sg, k_out_buf,
			fips_test_vector_sha_hmac[tv_index].ilen);
		rc = crypto_ahash_digest(ahash_req);
		if (rc == -EINPROGRESS || rc == -EBUSY) {
			rc = wait_for_completion_interruptible(
				&fips_completion.completion);
			err = fips_completion.err;
			if (!rc && !err) {
				INIT_COMPLETION(fips_completion.completion);
			} else {
				pr_err("qcrypto:SHA: wait_for_completion failed\n");
				goto clr_ahash_req;
			}

		}

		if (memcmp(k_out_buf,
			fips_test_vector_sha_hmac[tv_index].digest,
			fips_test_vector_sha_hmac[tv_index].diglen))
			rc = -1;

clr_ahash_req:
		ahash_request_free(ahash_req);
clr_tfm:
		crypto_free_ahash(tfm);
clr_buf:
		kzfree(k_out_buf);

	/* For any failure, return error */
		if (rc)
			return rc;

	}
	return rc;
}

/*
* Cipher algorithm self tests
*/
int _fips_qcrypto_cipher_selftest(struct fips_selftest_data *selftest_d)
{
	int rc = 0, err, tv_index, num_tv;
	struct crypto_ablkcipher *tfm;
	struct ablkcipher_request *ablkcipher_req;
	struct _fips_completion fips_completion;
	char *k_align_src = NULL;
	struct scatterlist fips_sg;

	num_tv = (sizeof(fips_test_vector_cipher)) /
		(sizeof(struct _fips_test_vector_cipher));

	/* One-by-one testing */
	for (tv_index = 0; tv_index < num_tv; tv_index++) {

		/* Single buffer allocation for in place operation */
		k_align_src = kzalloc(
			fips_test_vector_cipher[tv_index].pln_txt_len,
			GFP_KERNEL);
		if (k_align_src == NULL) {
			pr_err("qcrypto:, Failed to allocate memory for k_align_src %ld\n",
			PTR_ERR(k_align_src));
			return -ENOMEM;
		}

		memcpy(&k_align_src[0],
			fips_test_vector_cipher[tv_index].pln_txt,
			fips_test_vector_cipher[tv_index].pln_txt_len);

		/* use_sw flags are set in dtsi file which makes
		default Linux API calls to go to s/w crypto instead
		of h/w crypto. This code makes sure that all selftests
		calls always go to h/w, independent of DTSI flags. */
		if (!strcmp(fips_test_vector_cipher[tv_index].mod_alg,
			"xts(aes)")) {
			if (selftest_d->prefix_aes_xts_algo)
				if (_fips_get_alg_cra_name(
					fips_test_vector_cipher[tv_index]
					.mod_alg, selftest_d->algo_prefix,
					strlen(fips_test_vector_cipher[tv_index]
					.mod_alg))) {
					rc = -1;
					pr_err("Algo Name is too long for tv %d\n",
					tv_index);
					goto clr_buf;
				}
		} else {
			if (selftest_d->prefix_aes_cbc_ecb_ctr_algo)
				if (_fips_get_alg_cra_name(
					fips_test_vector_cipher[tv_index]
					.mod_alg, selftest_d->algo_prefix,
					strlen(fips_test_vector_cipher[tv_index]
					.mod_alg))) {
					rc = -1;
					pr_err("Algo Name is too long for tv %d\n",
					tv_index);
					goto clr_buf;
				}
		}

		tfm = crypto_alloc_ablkcipher(
			fips_test_vector_cipher[tv_index].mod_alg, 0, 0);
		if (IS_ERR(tfm)) {
			pr_err("qcrypto: %s algorithm not found\n",
			fips_test_vector_cipher[tv_index].mod_alg);
			rc = -ENOMEM;
			goto clr_buf;
		}

		ablkcipher_req = ablkcipher_request_alloc(tfm, GFP_KERNEL);
		if (!ablkcipher_req) {
			pr_err("qcrypto: ablkcipher_request_alloc failed\n");
			rc = -ENOMEM;
			goto clr_tfm;
		}
		rc = qcrypto_cipher_set_device(ablkcipher_req,
			selftest_d->ce_device);
		if (rc != 0) {
			pr_err("%s qcrypto_cipher_set_device failed with err %d\n",
				__func__, rc);
			goto clr_ablkcipher_req;
		}
		ablkcipher_request_set_callback(ablkcipher_req,
			CRYPTO_TFM_REQ_MAY_BACKLOG,
			_fips_cb, &fips_completion);

		crypto_ablkcipher_clear_flags(tfm, ~0);
		rc = crypto_ablkcipher_setkey(tfm,
			fips_test_vector_cipher[tv_index].key,
			fips_test_vector_cipher[tv_index].klen);
		if (rc) {
			pr_err("qcrypto: crypto_ablkcipher_setkey failed\n");
			goto clr_ablkcipher_req;
		}
		sg_set_buf(&fips_sg, k_align_src,
			fips_test_vector_cipher[tv_index].enc_txt_len);
		sg_mark_end(&fips_sg);
		ablkcipher_request_set_crypt(ablkcipher_req,
			&fips_sg, &fips_sg,
			fips_test_vector_cipher[tv_index].pln_txt_len,
			fips_test_vector_cipher[tv_index].iv);

		/**** Encryption Test ****/
		init_completion(&fips_completion.completion);
		rc = crypto_ablkcipher_encrypt(ablkcipher_req);
		if (rc == -EINPROGRESS || rc == -EBUSY) {
			rc = wait_for_completion_interruptible(
				&fips_completion.completion);
			err = fips_completion.err;
			if (!rc && !err) {
				INIT_COMPLETION(fips_completion.completion);
			} else {
				pr_err("qcrypto:cipher:ENC, wait_for_completion failed\n");
				goto clr_ablkcipher_req;
			}

		}

		if (memcmp(k_align_src,
			fips_test_vector_cipher[tv_index].enc_txt,
			fips_test_vector_cipher[tv_index].enc_txt_len)) {
			rc = -1;
			goto clr_ablkcipher_req;
		}

		/**** Decryption test ****/
		init_completion(&fips_completion.completion);
		rc = crypto_ablkcipher_decrypt(ablkcipher_req);
		if (rc == -EINPROGRESS || rc == -EBUSY) {
			rc = wait_for_completion_interruptible(
				&fips_completion.completion);
			err = fips_completion.err;
			if (!rc && !err) {
				INIT_COMPLETION(fips_completion.completion);
			} else {
				pr_err("qcrypto:cipher:DEC, wait_for_completion failed\n");
				goto clr_ablkcipher_req;
			}

		}

		if (memcmp(k_align_src,
			fips_test_vector_cipher[tv_index].pln_txt,
			fips_test_vector_cipher[tv_index].pln_txt_len))
			rc = -1;

clr_ablkcipher_req:
		ablkcipher_request_free(ablkcipher_req);
clr_tfm:
		crypto_free_ablkcipher(tfm);
clr_buf:
		kzfree(k_align_src);

		if (rc)
			return rc;

	}
	return rc;
}

/*
 * AEAD algorithm self tests
 */
int _fips_qcrypto_aead_selftest(struct fips_selftest_data *selftest_d)
{
	int rc = 0, err, tv_index, num_tv, authsize, buf_length;
	struct crypto_aead *tfm;
	struct aead_request *aead_req;
	struct _fips_completion fips_completion;
	struct scatterlist fips_sg, fips_assoc_sg;
	char *k_align_src = NULL;

	num_tv = (sizeof(fips_test_vector_aead)) /
		(sizeof(struct _fips_test_vector_aead));

	/* One-by-one testing */
	for (tv_index = 0; tv_index < num_tv; tv_index++) {

		if (fips_test_vector_aead[tv_index].pln_txt_len >
			fips_test_vector_aead[tv_index].enc_txt_len)
			buf_length =
				fips_test_vector_aead[tv_index].pln_txt_len;
		else
			buf_length =
				fips_test_vector_aead[tv_index].enc_txt_len;

		/* Single buffer allocation for in place operation */
		k_align_src = kzalloc(buf_length, GFP_KERNEL);
		if (k_align_src == NULL) {
			pr_err("qcrypto:, Failed to allocate memory for k_align_src %ld\n",
				PTR_ERR(k_align_src));
			return -ENOMEM;
		}
		memcpy(&k_align_src[0],
			fips_test_vector_aead[tv_index].pln_txt,
			fips_test_vector_aead[tv_index].pln_txt_len);

		/* use_sw flags are set in dtsi file which makes
		default Linux API calls to go to s/w crypto instead
		of h/w crypto. This code makes sure that all selftests
		calls always go to h/w, independent of DTSI flags. */
		if (selftest_d->prefix_aead_algo) {
			if (_fips_get_alg_cra_name(
				fips_test_vector_aead[tv_index].mod_alg,
				selftest_d->algo_prefix,
				strlen(fips_test_vector_aead[tv_index].mod_alg
				))) {
				rc = -1;
				pr_err("Algo Name is too long for tv %d\n",
					tv_index);
				goto clr_buf;
			}
		}
		tfm = crypto_alloc_aead(
			fips_test_vector_aead[tv_index].mod_alg, 0, 0);
		if (IS_ERR(tfm)) {
			pr_err("qcrypto: %s algorithm not found\n",
				fips_test_vector_aead[tv_index].mod_alg);
			rc = -ENOMEM;
			goto clr_buf;
		}
		aead_req = aead_request_alloc(tfm, GFP_KERNEL);
		if (!aead_req) {
			pr_err("qcrypto:aead_request_alloc failed\n");
			rc = -ENOMEM;
			goto clr_tfm;
		}
		rc = qcrypto_aead_set_device(aead_req, selftest_d->ce_device);
		if (rc != 0) {
			pr_err("%s qcrypto_cipher_set_device failed with err %d\n",
				__func__, rc);
			goto clr_aead_req;
		}
		init_completion(&fips_completion.completion);
		aead_request_set_callback(aead_req,
			CRYPTO_TFM_REQ_MAY_BACKLOG,
			_fips_cb, &fips_completion);
		crypto_aead_clear_flags(tfm, ~0);
		rc = crypto_aead_setkey(tfm,
			fips_test_vector_aead[tv_index].key,
			fips_test_vector_aead[tv_index].klen);
		if (rc) {
			pr_err("qcrypto:crypto_aead_setkey failed\n");
			goto clr_aead_req;
		}
		authsize = abs(fips_test_vector_aead[tv_index].enc_txt_len -
			fips_test_vector_aead[tv_index].pln_txt_len);
		rc = crypto_aead_setauthsize(tfm, authsize);
		if (rc) {
			pr_err("qcrypto:crypto_aead_setauthsize failed\n");
			goto clr_aead_req;
		}
		sg_init_one(&fips_sg, k_align_src,
		fips_test_vector_aead[tv_index].pln_txt_len + authsize);
		aead_request_set_crypt(aead_req,
			&fips_sg, &fips_sg,
			fips_test_vector_aead[tv_index].pln_txt_len ,
			fips_test_vector_aead[tv_index].iv);
		sg_init_one(&fips_assoc_sg,
			fips_test_vector_aead[tv_index].assoc,
			fips_test_vector_aead[tv_index].alen);
		aead_request_set_assoc(aead_req,
			&fips_assoc_sg,
			fips_test_vector_aead[tv_index].alen);
		/**** Encryption test ****/
		rc = crypto_aead_encrypt(aead_req);
		if (rc == -EINPROGRESS || rc == -EBUSY) {
			rc = wait_for_completion_interruptible(
				&fips_completion.completion);
			err = fips_completion.err;
			if (!rc && !err) {
				INIT_COMPLETION(fips_completion.completion);
			} else {
				pr_err("qcrypto:aead:ENC, wait_for_completion failed\n");
				goto clr_aead_req;
			}

		}
		if (memcmp(k_align_src,
			fips_test_vector_aead[tv_index].enc_txt,
			fips_test_vector_aead[tv_index].enc_txt_len)) {
			rc = -1;
			goto clr_aead_req;
		}

		/** Decryption test **/
		init_completion(&fips_completion.completion);
		aead_request_set_callback(aead_req,
			CRYPTO_TFM_REQ_MAY_BACKLOG,
			_fips_cb, &fips_completion);
		crypto_aead_clear_flags(tfm, ~0);
		rc = crypto_aead_setkey(tfm,
			fips_test_vector_aead[tv_index].key,
			fips_test_vector_aead[tv_index].klen);
		if (rc) {
			pr_err("qcrypto:aead:DEC, crypto_aead_setkey failed\n");
			goto clr_aead_req;
		}

		authsize = abs(fips_test_vector_aead[tv_index].enc_txt_len -
			fips_test_vector_aead[tv_index].pln_txt_len);
		rc = crypto_aead_setauthsize(tfm, authsize);
		if (rc) {
			pr_err("qcrypto:aead:DEC, crypto_aead_setauthsize failed\n");
			goto clr_aead_req;
		}

		sg_init_one(&fips_sg, k_align_src,
			fips_test_vector_aead[tv_index].enc_txt_len + authsize);
		aead_request_set_crypt(aead_req,
			&fips_sg, &fips_sg,
			fips_test_vector_aead[tv_index].enc_txt_len,
			fips_test_vector_aead[tv_index].iv);
		sg_init_one(&fips_assoc_sg,
			fips_test_vector_aead[tv_index].assoc,
			fips_test_vector_aead[tv_index].alen);
		aead_request_set_assoc(aead_req,
			&fips_assoc_sg,
			fips_test_vector_aead[tv_index].alen);
		rc = crypto_aead_decrypt(aead_req);
		if (rc == -EINPROGRESS || rc == -EBUSY) {
			rc = wait_for_completion_interruptible(
				&fips_completion.completion);
			err = fips_completion.err;
			if (!rc && !err) {
				INIT_COMPLETION(fips_completion.completion);
			} else {
				pr_err("qcrypto:aead:DEC, wait_for_completion failed\n");
				goto clr_aead_req;
			}

		}

		if (memcmp(k_align_src,
			fips_test_vector_aead[tv_index].pln_txt,
			fips_test_vector_aead[tv_index].pln_txt_len)) {
			rc = -1;
			goto clr_aead_req;
		}
clr_aead_req:
		aead_request_free(aead_req);
clr_tfm:
		crypto_free_aead(tfm);
clr_buf:
		kzfree(k_align_src);
	/* In case of any failure, return error */
		if (rc)
			return rc;
	}
	return rc;
}

