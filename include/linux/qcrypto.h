/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef _DRIVERS_CRYPTO_MSM_QCRYPTO_H_
#define _DRIVERS_CRYPTO_MSM_QCRYPTO_H_

#include <linux/crypto.h>
#include <crypto/hash.h>

#define QCRYPTO_CTX_KEY_MASK		0x000000ff
#define QCRYPTO_CTX_USE_HW_KEY		0x00000001
#define QCRYPTO_CTX_USE_PIPE_KEY	0x00000002

#define QCRYPTO_CTX_XTS_MASK		0x0000ff00
#define QCRYPTO_CTX_XTS_DU_SIZE_512B	0x00000100
#define QCRYPTO_CTX_XTS_DU_SIZE_1KB	0x00000200


int qcrypto_cipher_set_device(struct ablkcipher_request *req, unsigned int dev);
int qcrypto_ahash_set_device(struct ahash_request *req, unsigned int dev);
int qcrypto_aead_set_device(struct aead_request *req, unsigned int dev);

int qcrypto_cipher_set_flag(struct ablkcipher_request *req, unsigned int flags);
int qcrypto_ahash_set_flag(struct ahash_request *req, unsigned int flags);
int qcrypto_aead_set_flag(struct aead_request *req, unsigned int flags);

int qcrypto_cipher_clear_flag(struct ablkcipher_request *req,
							unsigned int flags);
int qcrypto_ahash_clear_flag(struct ahash_request *req, unsigned int flags);
int qcrypto_aead_clear_flag(struct aead_request *req, unsigned int flags);

#endif /* _DRIVERS_CRYPTO_MSM_QCRYPTO_H */
