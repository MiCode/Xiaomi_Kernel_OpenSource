/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * QTI crypto Driver
 *
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __CRYPTO_MSM_QCEDEVI_H
#define __CRYPTO_MSM_QCEDEVI_H

#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <crypto/hash.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <linux/fips_status.h>
#include "qce.h"
#include "qcedev_smmu.h"

#define CACHE_LINE_SIZE 32
#define CE_SHA_BLOCK_SIZE SHA256_BLOCK_SIZE

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
 * Register ourselves as a char device to be able to access the dev driver
 * from userspace.
 */

#define QCEDEV_DEV	"qce"

struct qcedev_control {

	/* CE features supported by platform */
	struct msm_ce_hw_support platform_support;

	uint32_t ce_lock_count;
	uint32_t high_bw_req_count;

	/* CE features/algorithms supported by HW engine*/
	struct ce_hw_support ce_support;

	/* replaced msm_bus with interconnect path */
	struct icc_path *icc_path;

	/* char device */
	struct cdev cdev;

	int minor;

	/* qce handle */
	void *qce;

	/* platform device */
	struct platform_device *pdev;

	unsigned int magic;

	struct list_head ready_commands;
	struct qcedev_async_req *active_command;
	spinlock_t lock;
	struct tasklet_struct done_tasklet;
	struct list_head context_banks;
	struct qcedev_mem_client *mem_client;
};

struct qcedev_handle {
	/* qcedev control handle */
	struct qcedev_control *cntl;
	/* qce internal sha context*/
	struct qcedev_sha_ctxt sha_ctxt;
	/* qcedev mapped buffer list */
	struct qcedev_buffer_list registeredbufs;
};

void qcedev_cipher_req_cb(void *cookie, unsigned char *icv,
	unsigned char *iv, int ret);

void qcedev_sha_req_cb(void *cookie, unsigned char *digest,
	unsigned char *authdata, int ret);

#endif  /* __CRYPTO_MSM_QCEDEVI_H */
