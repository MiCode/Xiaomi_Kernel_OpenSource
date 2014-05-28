/* FIPS Known answer tests for QCEDEV / FIPS-non-FIPS separation .
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
 *
 */

#include <linux/mman.h>
#include <linux/types.h>
#include <linux/export.h>
#include <linux/qcedev.h>
#include "qcedevi.h"
#include "qcedev_fips.h"

/*
 * Initiate the session handle (like open /dev/qce)
 */
static int _fips_initiate_qcedev_handle(struct qcedev_control *podev,
				struct qcedev_async_req *qcedev_areq)
{
	struct  qcedev_handle *handle;

	handle = kzalloc(sizeof(struct qcedev_handle), GFP_KERNEL);
	if (handle == NULL) {
		pr_err("Failed to allocate memory %ld\n", PTR_ERR(handle));
		return -ENOMEM;
	}

	handle->cntl = podev;
	qcedev_areq->handle = handle;
	return 0;
}

/*
 *Initiate QCEDEV request for sha/hmac
 */
static
int _fips_initiate_qcedev_async_req_sha(struct qcedev_async_req *qcedev_areq,
		struct scatterlist *fips_sg,
		int tv_index)
{
	qcedev_areq->sha_op_req.alg =
		fips_test_vector_sha_hmac[tv_index].hash_alg;

	/* If HMAC setup key else make key length zero */
	if ((qcedev_areq->sha_op_req.alg == QCEDEV_ALG_SHA1_HMAC) ||
		(qcedev_areq->sha_op_req.alg == QCEDEV_ALG_SHA256_HMAC) ||
		(qcedev_areq->sha_op_req.alg == QCEDEV_ALG_AES_CMAC)) {
		qcedev_areq->sha_op_req.authkey =
			&fips_test_vector_sha_hmac[tv_index].key[0];
		qcedev_areq->sha_op_req.authklen  =
			fips_test_vector_sha_hmac[tv_index].klen;
	} else
		qcedev_areq->sha_op_req.authklen = 0;

	/* Setup input and digest */
	qcedev_areq->sha_op_req.data[0].vaddr =
		&fips_test_vector_sha_hmac[tv_index].input[0];
	qcedev_areq->sha_op_req.data[0].len =
		fips_test_vector_sha_hmac[tv_index].ilen;
	qcedev_areq->sha_op_req.data_len =
		fips_test_vector_sha_hmac[tv_index].ilen;

	/* Setup sha context and other parameters */
	qcedev_areq->sha_op_req.entries = 1;
	qcedev_areq->op_type = QCEDEV_CRYPTO_OPER_SHA;
	memset(&qcedev_areq->handle->sha_ctxt, 0,
		sizeof(struct qcedev_sha_ctxt));
	qcedev_areq->handle->sha_ctxt.first_blk = 1;

	/* Initialize digest and digest length */
	memset(&qcedev_areq->sha_op_req.digest[0], 0, QCEDEV_MAX_SHA_DIGEST);
	qcedev_areq->sha_op_req.diglen =
		fips_test_vector_sha_hmac[tv_index].diglen;
	switch (qcedev_areq->sha_op_req.alg) {
	case QCEDEV_ALG_SHA1:
	case QCEDEV_ALG_SHA1_HMAC:
		memcpy(&qcedev_areq->handle->sha_ctxt.digest[0],
			&_std_init_vector_sha1_uint8[0],
			SHA1_DIGEST_SIZE);
		break;
	case QCEDEV_ALG_SHA256:
	case QCEDEV_ALG_SHA256_HMAC:
		memcpy(&qcedev_areq->handle->sha_ctxt.digest[0],
			&_std_init_vector_sha256_uint8[0],
			SHA256_DIGEST_SIZE);
		break;
	case QCEDEV_ALG_AES_CMAC:
		qcedev_areq->handle->sha_ctxt.diglen =
			fips_test_vector_sha_hmac[tv_index].diglen;
		break;
	default:
		pr_err(" _fips_initiate_qcedev_async_req_sha : Invalid algo");
		return -EINVAL;
	}

	qcedev_areq->handle->sha_ctxt.init_done = true;
	qcedev_areq->handle->sha_ctxt.trailing_buf_len =
		qcedev_areq->sha_op_req.data_len;
	memcpy(&qcedev_areq->handle->sha_ctxt.trailing_buf[0],
		fips_test_vector_sha_hmac[tv_index].input,
		fips_test_vector_sha_hmac[tv_index].ilen);
	qcedev_areq->handle->sha_ctxt.last_blk = 1;
	qcedev_areq->sha_req.sreq.nbytes = qcedev_areq->sha_op_req.data_len;
	qcedev_areq->sha_req.cookie = qcedev_areq->handle;
	qcedev_areq->sha_req.sreq.src = fips_sg;
	sg_set_buf(qcedev_areq->sha_req.sreq.src,
		&qcedev_areq->handle->sha_ctxt.trailing_buf[0],
		qcedev_areq->sha_op_req.data_len);
	sg_mark_end(qcedev_areq->sha_req.sreq.src);
	return 0;
}

/*
 * Clean up of sha context after request completion
 */
static void _fips_clear_qcedev_handle(struct qcedev_sha_ctxt *sha_ctxt)
{
	sha_ctxt->first_blk = 0;
	sha_ctxt->last_blk = 0;
	sha_ctxt->auth_data[0] = 0;
	sha_ctxt->auth_data[1] = 0;
	sha_ctxt->trailing_buf_len = 0;
	sha_ctxt->init_done = false;
	memset(&sha_ctxt->trailing_buf[0], 0, 64);
}

/*
 * Self test for SHA / HMAC
 */
int _fips_qcedev_sha_selftest(struct qcedev_control *podev)
{
	int ret = 0, tv_index, num_tv;
	struct qce_sha_req sreq;
	struct qcedev_async_req qcedev_areq;
	struct scatterlist fips_sg;

	/* Initiate handle */
	if (_fips_initiate_qcedev_handle(podev, &qcedev_areq))
		return -ENOMEM;

	num_tv = (sizeof(fips_test_vector_sha_hmac))/
		(sizeof(struct _fips_test_vector_sha_hmac));

	/* Tests one by one */
	for (tv_index = 0; tv_index < num_tv; tv_index++) {
		init_completion(&qcedev_areq.complete);

		/* Initiate the qcedev request */
		if (_fips_initiate_qcedev_async_req_sha(&qcedev_areq,
			&fips_sg, tv_index))
			return -EINVAL;

		podev->active_command = &qcedev_areq;

		/* Initiate qce hash request */
		sreq.qce_cb = qcedev_sha_req_cb;
		if (qcedev_areq.sha_op_req.alg != QCEDEV_ALG_AES_CMAC) {
			sreq.digest = &qcedev_areq.handle->sha_ctxt.digest[0];
			sreq.first_blk = qcedev_areq.handle->sha_ctxt.first_blk;
			sreq.last_blk = qcedev_areq.handle->sha_ctxt.last_blk;
			sreq.auth_data[0] =
				qcedev_areq.handle->sha_ctxt.auth_data[0];
			sreq.auth_data[1] =
				qcedev_areq.handle->sha_ctxt.auth_data[1];
			sreq.auth_data[2] =
				qcedev_areq.handle->sha_ctxt.auth_data[2];
			sreq.auth_data[3] =
				qcedev_areq.handle->sha_ctxt.auth_data[3];
		}

		sreq.size = qcedev_areq.sha_req.sreq.nbytes;
		sreq.src = qcedev_areq.sha_req.sreq.src;
		sreq.areq = (void *)&qcedev_areq.sha_req;
		sreq.flags = 0;
		switch (qcedev_areq.sha_op_req.alg) {
		case QCEDEV_ALG_SHA1:
			sreq.alg = QCE_HASH_SHA1;
			break;
		case QCEDEV_ALG_SHA256:
			sreq.alg = QCE_HASH_SHA256;
			break;
		case QCEDEV_ALG_SHA1_HMAC:
			sreq.alg = QCE_HASH_SHA1_HMAC;
			sreq.authkey = &qcedev_areq.sha_op_req.authkey[0];
			sreq.authklen = qcedev_areq.sha_op_req.authklen;
			break;
		case QCEDEV_ALG_SHA256_HMAC:
			sreq.alg = QCE_HASH_SHA256_HMAC;
			sreq.authkey =
				&qcedev_areq.sha_op_req.authkey[0];
			sreq.authklen =
				qcedev_areq.sha_op_req.authklen;
			break;
		case QCEDEV_ALG_AES_CMAC:
			sreq.alg = QCE_HASH_AES_CMAC;
			sreq.authkey =
				&qcedev_areq.sha_op_req.authkey[0];
			sreq.authklen =
				qcedev_areq.sha_op_req.authklen;
			break;
		default:
			ret = -EINVAL;
			goto handle_free;
		}

		/*qce call */
		ret = qce_process_sha_req(podev->qce, &sreq);
		if (ret == 0)
			wait_for_completion(&qcedev_areq.complete);
		else
			goto handle_free;

		/* Known answer test */
		if (memcmp(&qcedev_areq.handle->sha_ctxt.digest[0],
			fips_test_vector_sha_hmac[tv_index].digest,
			fips_test_vector_sha_hmac[tv_index].diglen)) {
				ret = -1;
				goto handle_free;
		}
		_fips_clear_qcedev_handle(&qcedev_areq.handle->sha_ctxt);
	}

handle_free:
	kzfree(qcedev_areq.handle);
	return ret;
}

/*
 * Initiate QCEDEV request for cipher (Encryption/ Decryption requests)
 */
static
void _fips_initiate_qcedev_async_req_cipher(
			struct qcedev_async_req *qcedev_areq,
			enum qcedev_oper_enum qcedev_oper,
			struct scatterlist *fips_sg,
			uint8_t *k_align_src,
			int tv_index)
{
	uint8_t *k_align_dst = k_align_src;

	/* Setup Key */
	memset(qcedev_areq->cipher_op_req.enckey, 0,
		fips_test_vector_cipher[tv_index].klen);
	memcpy(qcedev_areq->cipher_op_req.enckey,
		fips_test_vector_cipher[tv_index].key,
		fips_test_vector_cipher[tv_index].klen);
	qcedev_areq->cipher_op_req.encklen =
		fips_test_vector_cipher[tv_index].klen;

	/* Setup IV */
	memset(qcedev_areq->cipher_op_req.iv, 0,
		fips_test_vector_cipher[tv_index].ivlen);
	memcpy(qcedev_areq->cipher_op_req.iv,
		fips_test_vector_cipher[tv_index].iv,
		fips_test_vector_cipher[tv_index].ivlen);
	qcedev_areq->cipher_op_req.ivlen =
		fips_test_vector_cipher[tv_index].ivlen;

	/* Setup other parameters */
	qcedev_areq->cipher_op_req.byteoffset  = 0;
	qcedev_areq->cipher_op_req.alg =
		fips_test_vector_cipher[tv_index].enc_alg;
	qcedev_areq->cipher_op_req.mode =
		fips_test_vector_cipher[tv_index].mode;
	qcedev_areq->cipher_op_req.use_pmem = 0;
	qcedev_areq->cipher_op_req.in_place_op = 1;
	qcedev_areq->cipher_op_req.entries = 1;
	qcedev_areq->cipher_op_req.op = qcedev_oper;
	qcedev_areq->op_type = QCEDEV_CRYPTO_OPER_CIPHER;

	/* Setup Input and output buffers */
	if (qcedev_oper == QCEDEV_OPER_ENC) {
		qcedev_areq->cipher_op_req.data_len =
			fips_test_vector_cipher[tv_index].pln_txt_len;
		qcedev_areq->cipher_op_req.vbuf.src[0].len =
			fips_test_vector_cipher[tv_index].pln_txt_len;
	} else {
		qcedev_areq->cipher_op_req.data_len =
			fips_test_vector_cipher[tv_index].enc_txt_len;
		qcedev_areq->cipher_op_req.vbuf.src[0].len =
			fips_test_vector_cipher[tv_index].enc_txt_len;
	}

	qcedev_areq->cipher_op_req.vbuf.src[0].vaddr =
		&k_align_src[0];
	qcedev_areq->cipher_op_req.vbuf.dst[0].vaddr =
		&k_align_dst[0];
	qcedev_areq->cipher_op_req.vbuf.dst[0].len =
		fips_test_vector_cipher[tv_index].enc_txt_len;

	qcedev_areq->cipher_req.creq.src = fips_sg;
	qcedev_areq->cipher_req.creq.dst = fips_sg;
	sg_set_buf(qcedev_areq->cipher_req.creq.src,
		k_align_src,
		qcedev_areq->cipher_op_req.data_len);
	sg_mark_end(qcedev_areq->cipher_req.creq.src);

	qcedev_areq->cipher_req.creq.nbytes =
		qcedev_areq->cipher_op_req.data_len;
	qcedev_areq->cipher_req.creq.info =
		qcedev_areq->cipher_op_req.iv;
	qcedev_areq->cipher_req.cookie = qcedev_areq->handle;
}

/*
 * Initiate QCE request for cipher (Encryption/ Decryption requests)
 */
static int _fips_initiate_qce_req_cipher(struct qcedev_async_req *qcedev_areq,
			struct qce_req *creq,
			enum qce_cipher_dir_enum cipher_dir)
{
	creq->dir = cipher_dir;
	creq->iv = &qcedev_areq->cipher_op_req.iv[0];
	creq->ivsize = qcedev_areq->cipher_op_req.ivlen;
	creq->enckey =  &qcedev_areq->cipher_op_req.enckey[0];
	creq->encklen = qcedev_areq->cipher_op_req.encklen;
	creq->cryptlen = qcedev_areq->cipher_op_req.data_len;
	creq->op = QCE_REQ_ABLK_CIPHER;
	creq->qce_cb = qcedev_cipher_req_cb;
	creq->areq = (void *)&qcedev_areq->cipher_req;
	creq->flags = 0;
	switch (qcedev_areq->cipher_op_req.alg) {
	case QCEDEV_ALG_3DES:
		creq->alg = CIPHER_ALG_3DES;
		break;
	case QCEDEV_ALG_AES:
		creq->alg = CIPHER_ALG_AES;
		break;
	default:
		pr_err(" _fips_initiate_qce_req_cipher : Invalid algo");
		return -EINVAL;
	}

	switch (qcedev_areq->cipher_op_req.mode) {
	case QCEDEV_AES_MODE_CBC:
	case QCEDEV_DES_MODE_CBC:
		creq->mode = QCE_MODE_CBC;
		break;
	case QCEDEV_AES_MODE_ECB:
	case QCEDEV_DES_MODE_ECB:
		creq->mode = QCE_MODE_ECB;
		break;
	case QCEDEV_AES_MODE_CTR:
		creq->mode = QCE_MODE_CTR;
		break;
	case QCEDEV_AES_MODE_XTS:
		creq->mode = QCE_MODE_XTS;
		break;
	case QCEDEV_AES_MODE_CCM:
		creq->mode = QCE_MODE_CCM;
		break;
	default:
		pr_err(" _fips_initiate_qce_req_cipher : Invalid algo");
		return -EINVAL;
	}

	return 0;
}

/*
 * Self test for Cipher algorithms
 */
int _fips_qcedev_cipher_selftest(struct qcedev_control *podev)
{
	int ret = 0, tv_index = 0, num_tv;
	struct qcedev_async_req qcedev_areq;
	struct qce_req creq;
	struct scatterlist fips_sg;
	uint8_t *k_align_src = NULL;

	/* initiate handle */
	if (_fips_initiate_qcedev_handle(podev, &qcedev_areq))
		return -ENOMEM;

	num_tv = (sizeof(fips_test_vector_cipher)) /
		(sizeof(struct _fips_test_vector_cipher));

	/* tests one by one */
	for (tv_index = 0; tv_index < num_tv; tv_index++) {

		/* Allocate single buffer for in-place operation */
		k_align_src = kzalloc(QCE_MAX_OPER_DATA, GFP_KERNEL);
		if (k_align_src == NULL) {
			pr_err("qcedev: Failed to allocate memory for k_align_src %ld\n",
				PTR_ERR(k_align_src));
			kzfree(qcedev_areq.handle);
			return -ENOMEM;
		}

		/**************** Encryption Tests *****************/
		init_completion(&qcedev_areq.complete);
		memcpy(&k_align_src[0],
			fips_test_vector_cipher[tv_index].pln_txt,
			fips_test_vector_cipher[tv_index].pln_txt_len);

		/* Initiate qcedev request */
		_fips_initiate_qcedev_async_req_cipher(&qcedev_areq,
			QCEDEV_OPER_ENC, &fips_sg,
			k_align_src, tv_index);
		podev->active_command = &qcedev_areq;

		/* Initiate qce cipher request */
		if (_fips_initiate_qce_req_cipher(&qcedev_areq,
			&creq, QCE_ENCRYPT)) {
			ret = -EINVAL;
			kzfree(k_align_src);
			goto free_handle;
		}

		/* qce call */
		ret = qce_ablk_cipher_req(podev->qce, &creq);
		if (ret == 0)
			wait_for_completion(&qcedev_areq.complete);
		else {
			kzfree(k_align_src);
			goto free_handle;
		}

		/* Known answer test for encryption */
		if (memcmp(k_align_src,
			fips_test_vector_cipher[tv_index].enc_txt,
			fips_test_vector_cipher[tv_index].enc_txt_len)) {
			ret = -1;
			kzfree(k_align_src);
			goto free_handle;
		}

		/**************** Decryption Tests *****************/
		init_completion(&qcedev_areq.complete);
		memset(&k_align_src[0], 0,
			fips_test_vector_cipher[tv_index].pln_txt_len);
		memcpy(&k_align_src[0],
			fips_test_vector_cipher[tv_index].enc_txt,
			fips_test_vector_cipher[tv_index].enc_txt_len);

		/* Initiate qcedev request */
		_fips_initiate_qcedev_async_req_cipher(&qcedev_areq,
			QCEDEV_OPER_DEC, &fips_sg,
			k_align_src, tv_index);
		podev->active_command = &qcedev_areq;

		/*Initiate qce cipher request */
		if (_fips_initiate_qce_req_cipher(&qcedev_areq,
			&creq, QCE_DECRYPT)) {
			ret = -EINVAL;
			kzfree(k_align_src);
			goto free_handle;
		}

		/* qce call */
		ret = qce_ablk_cipher_req(podev->qce, &creq);
		if (ret == 0)
			wait_for_completion(&qcedev_areq.complete);
		else {
			kzfree(k_align_src);
			goto free_handle;
		}

		/* Known answer test for Decryption */
		if (memcmp(k_align_src,
			fips_test_vector_cipher[tv_index].pln_txt,
			fips_test_vector_cipher[tv_index].pln_txt_len)) {
			ret = -1;
			kzfree(k_align_src);
			goto free_handle;
		}
		podev->active_command = NULL;
		kzfree(k_align_src);
	}

free_handle:
	kzfree(qcedev_areq.handle);
	return ret;
}

void fips_reg_drbg_callback(void *src)
{
	drbg_call_back = src;
}
EXPORT_SYMBOL(fips_reg_drbg_callback);
