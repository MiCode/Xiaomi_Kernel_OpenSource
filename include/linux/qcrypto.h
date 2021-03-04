/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DRIVERS_CRYPTO_MSM_QCRYPTO_H_
#define _DRIVERS_CRYPTO_MSM_QCRYPTO_H_

#include <linux/crypto.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>
#include <crypto/aead.h>

#define QCRYPTO_CTX_KEY_MASK		0x000000ff
#define QCRYPTO_CTX_USE_HW_KEY		0x00000001
#define QCRYPTO_CTX_USE_PIPE_KEY	0x00000002

#define QCRYPTO_CTX_XTS_MASK		0x0000ff00
#define QCRYPTO_CTX_XTS_DU_SIZE_512B	0x00000100
#define QCRYPTO_CTX_XTS_DU_SIZE_1KB	0x00000200


int qcrypto_cipher_set_device(struct skcipher_request *req, unsigned int dev);
int qcrypto_ahash_set_device(struct ahash_request *req, unsigned int dev);
int qcrypto_aead_set_device(struct aead_request *req, unsigned int dev);

int qcrypto_cipher_set_flag(struct skcipher_request *req, unsigned int flags);
int qcrypto_ahash_set_flag(struct ahash_request *req, unsigned int flags);
int qcrypto_aead_set_flag(struct aead_request *req, unsigned int flags);

int qcrypto_cipher_clear_flag(struct skcipher_request *req,
							unsigned int flags);
int qcrypto_ahash_clear_flag(struct ahash_request *req, unsigned int flags);
int qcrypto_aead_clear_flag(struct aead_request *req, unsigned int flags);

struct crypto_engine_entry {
	u32 hw_instance;
	u32 ce_device;
	int shared;
};

int qcrypto_get_num_engines(void);
void qcrypto_get_engine_list(size_t num_engines,
				struct crypto_engine_entry *arr);
int qcrypto_cipher_set_device_hw(struct skcipher_request *req,
				unsigned int fde_pfe,
				unsigned int hw_inst);


struct qcrypto_func_set {
	int (*cipher_set)(struct skcipher_request *req,
			unsigned int fde_pfe,
			unsigned int hw_inst);
	int (*cipher_flag)(struct skcipher_request *req, unsigned int flags);
	int (*get_num_engines)(void);
	void (*get_engine_list)(size_t num_engines,
				struct crypto_engine_entry *arr);
};

#endif /* _DRIVERS_CRYPTO_MSM_QCRYPTO_H */
