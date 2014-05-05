/* Qualcomm Crypto Engine driver API
 *
 * Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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


#ifndef __CRYPTO_MSM_QCE_H
#define __CRYPTO_MSM_QCE_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/crypto.h>

#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/sha.h>
#include <crypto/aead.h>
#include <crypto/authenc.h>
#include <crypto/scatterwalk.h>

/* SHA digest size  in bytes */
#define SHA256_DIGESTSIZE		32
#define SHA1_DIGESTSIZE			20

#define AES_CE_BLOCK_SIZE		16

/* key size in bytes */
#define HMAC_KEY_SIZE			(SHA1_DIGESTSIZE)    /* hmac-sha1 */
#define SHA_HMAC_KEY_SIZE		64
#define DES_KEY_SIZE			8
#define TRIPLE_DES_KEY_SIZE		24
#define AES128_KEY_SIZE			16
#define AES192_KEY_SIZE			24
#define AES256_KEY_SIZE			32
#define MAX_CIPHER_KEY_SIZE		AES256_KEY_SIZE

/* iv length in bytes */
#define AES_IV_LENGTH			16
#define DES_IV_LENGTH                   8
#define MAX_IV_LENGTH			AES_IV_LENGTH

/* Maximum number of bytes per transfer */
#define QCE_MAX_OPER_DATA		0xFF00

/* Maximum Nonce bytes  */
#define MAX_NONCE  16

typedef void (*qce_comp_func_ptr_t)(void *areq,
		unsigned char *icv, unsigned char *iv, int ret);

/* Cipher algorithms supported */
enum qce_cipher_alg_enum {
	CIPHER_ALG_DES = 0,
	CIPHER_ALG_3DES = 1,
	CIPHER_ALG_AES = 2,
	CIPHER_ALG_LAST
};

/* Hash and hmac algorithms supported */
enum qce_hash_alg_enum {
	QCE_HASH_SHA1   = 0,
	QCE_HASH_SHA256 = 1,
	QCE_HASH_SHA1_HMAC   = 2,
	QCE_HASH_SHA256_HMAC = 3,
	QCE_HASH_AES_CMAC = 4,
	QCE_HASH_LAST
};

/* Cipher encryption/decryption operations */
enum qce_cipher_dir_enum {
	QCE_ENCRYPT = 0,
	QCE_DECRYPT = 1,
	QCE_CIPHER_DIR_LAST
};

/* Cipher algorithms modes */
enum qce_cipher_mode_enum {
	QCE_MODE_CBC = 0,
	QCE_MODE_ECB = 1,
	QCE_MODE_CTR = 2,
	QCE_MODE_XTS = 3,
	QCE_MODE_CCM = 4,
	QCE_CIPHER_MODE_LAST
};

/* Cipher operation type */
enum qce_req_op_enum {
	QCE_REQ_ABLK_CIPHER = 0,
	QCE_REQ_ABLK_CIPHER_NO_KEY = 1,
	QCE_REQ_AEAD = 2,
	QCE_REQ_LAST
};

/* Algorithms/features supported in CE HW engine */
struct ce_hw_support {
	bool sha1_hmac_20; /* Supports 20 bytes of HMAC key*/
	bool sha1_hmac; /* supports max HMAC key of 64 bytes*/
	bool sha256_hmac; /* supports max HMAC key of 64 bytes*/
	bool sha_hmac; /* supports SHA1 and SHA256 MAX HMAC key of 64 bytes*/
	bool cmac;
	bool aes_key_192;
	bool aes_xts;
	bool aes_ccm;
	bool ota;
	bool aligned_only;
	bool bam;
	bool is_shared;
	bool hw_key;
	bool use_sw_aes_cbc_ecb_ctr_algo;
	bool use_sw_aead_algo;
	bool use_sw_aes_xts_algo;
	bool use_sw_ahash_algo;
	bool use_sw_hmac_algo;
	bool use_sw_aes_ccm_algo;
	bool clk_mgmt_sus_res;
	unsigned int ce_device;
};

/* Sha operation parameters */
struct qce_sha_req {
	qce_comp_func_ptr_t qce_cb;	/* call back */
	enum qce_hash_alg_enum alg;	/* sha algorithm */
	unsigned char *digest;		/* sha digest  */
	struct scatterlist *src;	/* pointer to scatter list entry */
	uint32_t  auth_data[4];		/* byte count */
	unsigned char *authkey;		/* auth key */
	unsigned int  authklen;		/* auth key length */
	bool first_blk;			/* first block indicator */
	bool last_blk;			/* last block indicator */
	unsigned int size;		/* data length in bytes */
	void *areq;
	unsigned int  flags;
};

struct qce_req {
	enum qce_req_op_enum op;	/* operation type */
	qce_comp_func_ptr_t qce_cb;	/* call back */
	void *areq;
	enum qce_cipher_alg_enum   alg;	/* cipher algorithms*/
	enum qce_cipher_dir_enum dir;	/* encryption? decryption? */
	enum qce_cipher_mode_enum mode;	/* algorithm mode  */
	enum qce_hash_alg_enum auth_alg;/* authentication algorithm for aead */
	unsigned char *authkey;		/* authentication key  */
	unsigned int authklen;		/* authentication key kength */
	unsigned int authsize;		/* authentication key kength */
	unsigned char  nonce[MAX_NONCE];/* nonce for ccm mode */
	unsigned char *assoc;		/* Ptr to formatted associated data */
	unsigned int assoclen;		/* Formatted associated data length  */
	struct scatterlist *asg;	/* Formatted associated data sg  */
	unsigned char *enckey;		/* cipher key  */
	unsigned int encklen;		/* cipher key length */
	unsigned char *iv;		/* initialization vector */
	unsigned int ivsize;		/* initialization vector size*/
	unsigned int cryptlen;		/* data length */
	unsigned int use_pmem;		/* is source of data PMEM allocated? */
	struct qcedev_pmem_info *pmem;	/* pointer to pmem_info structure*/
	unsigned int  flags;
};

struct qce_pm_table {
	int (*suspend)(void *handle);
	int (*resume)(void *handle);
};

extern struct qce_pm_table qce_pm_table;

void *qce_open(struct platform_device *pdev, int *rc);
int qce_close(void *handle);
int qce_aead_req(void *handle, struct qce_req *req);
int qce_ablk_cipher_req(void *handle, struct qce_req *req);
int qce_hw_support(void *handle, struct ce_hw_support *support);
int qce_process_sha_req(void *handle, struct qce_sha_req *s_req);
int qce_enable_clk(void *handle);
int qce_disable_clk(void *handle);

#endif /* __CRYPTO_MSM_QCE_H */
