/* QTI crypto Driver
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

#ifndef __CRYPTO_MSM_QCEDEVI_H
#define __CRYPTO_MSM_QCEDEVI_H

#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <crypto/hash.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <linux/fips_status.h>
#include "qce.h"

#define CACHE_LINE_SIZE 32
#define CE_SHA_BLOCK_SIZE SHA256_BLOCK_SIZE

/* FIPS global status variable */
extern enum fips_status g_fips140_status;

/*FIPS140-2 call back for DRBG self test */
extern void *drbg_call_back;

enum qcedev_crypto_oper_type {
	QCEDEV_CRYPTO_OPER_CIPHER = 0,
	QCEDEV_CRYPTO_OPER_SHA = 1,
	QCEDEV_CRYPTO_OPER_LAST
};

struct qcedev_handle;

struct qcedev_cipher_req {
	struct ablkcipher_request creq;
	void *cookie;
};

struct qcedev_sha_req {
	struct ahash_request sreq;
	void *cookie;
};

struct	qcedev_sha_ctxt {
	uint32_t	auth_data[4];
	uint8_t	digest[QCEDEV_MAX_SHA_DIGEST];
	uint32_t	diglen;
	uint8_t	trailing_buf[64];
	uint32_t	trailing_buf_len;
	uint8_t	first_blk;
	uint8_t	last_blk;
	uint8_t	authkey[QCEDEV_MAX_SHA_BLOCK_SIZE];
	bool		init_done;
};

struct qcedev_async_req {
	struct list_head			list;
	struct completion			complete;
	enum qcedev_crypto_oper_type		op_type;
	union {
		struct qcedev_cipher_op_req	cipher_op_req;
		struct qcedev_sha_op_req	sha_op_req;
	};

	union {
		struct qcedev_cipher_req	cipher_req;
		struct qcedev_sha_req		sha_req;
	};
	struct qcedev_handle			*handle;
	int					err;
};

/**********************************************************************
 * Register ourselves as a misc device to be able to access the dev driver
 * from userspace. */

#define QCEDEV_DEV	"qcedev"

struct qcedev_control {

	/* CE features supported by platform */
	struct msm_ce_hw_support platform_support;

	uint32_t ce_lock_count;
	uint32_t high_bw_req_count;

	/* CE features/algorithms supported by HW engine*/
	struct ce_hw_support ce_support;

	uint32_t  bus_scale_handle;

	/* misc device */
	struct miscdevice miscdevice;

	/* qce handle */
	void *qce;

	/* platform device */
	struct platform_device *pdev;

	unsigned magic;

	struct list_head ready_commands;
	struct qcedev_async_req *active_command;
	spinlock_t lock;
	struct tasklet_struct done_tasklet;
};

struct qcedev_handle {
	/* qcedev control handle */
	struct qcedev_control *cntl;
	/* qce internal sha context*/
	struct qcedev_sha_ctxt sha_ctxt;
};

void qcedev_cipher_req_cb(void *cookie, unsigned char *icv,
	unsigned char *iv, int ret);

void qcedev_sha_req_cb(void *cookie, unsigned char *digest,
	unsigned char *authdata, int ret);

extern int _do_msm_fips_drbg_init(void *rng_dev);

#ifdef CONFIG_FIPS_ENABLE

/*
 * Self test for Cipher algorithms
 */
int _fips_qcedev_cipher_selftest(struct qcedev_control *podev);

/*
 * Self test for SHA / HMAC
 */

int _fips_qcedev_sha_selftest(struct qcedev_control *podev);

/*
 * Update FIPs Global status Status
 */
static inline enum fips_status _fips_update_status(enum fips_status status)
{
	return (status == FIPS140_STATUS_PASS) ?
		FIPS140_STATUS_QCRYPTO_ALLOWED :
		FIPS140_STATUS_FAIL;
}

#else

static inline int _fips_qcedev_cipher_selftest(struct qcedev_control *podev)
{
	return 0;
}
static inline int _fips_qcedev_sha_selftest(struct qcedev_control *podev)
{
	return 0;
}

static inline enum fips_status _fips_update_status(enum fips_status status)
{
	return FIPS140_STATUS_NA;
}

#endif  /* CONFIG_FIPS_ENABLE */

#endif  /* __CRYPTO_MSM_QCEDEVI_H */
