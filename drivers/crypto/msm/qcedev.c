/* Qualcomm CE device driver.
 *
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/mman.h>
#include <linux/android_pmem.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/dmapool.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <crypto/hash.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <mach/scm.h>
#include <mach/msm_bus.h>
#include <linux/qcedev.h>
#include "qce.h"


#define CACHE_LINE_SIZE 32
#define CE_SHA_BLOCK_SIZE SHA256_BLOCK_SIZE

static uint8_t  _std_init_vector_sha1_uint8[] =   {
	0x67, 0x45, 0x23, 0x01, 0xEF, 0xCD, 0xAB, 0x89,
	0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x32, 0x54, 0x76,
	0xC3, 0xD2, 0xE1, 0xF0
};
/* standard initialization vector for SHA-256, source: FIPS 180-2 */
static uint8_t _std_init_vector_sha256_uint8[] = {
	0x6A, 0x09, 0xE6, 0x67, 0xBB, 0x67, 0xAE, 0x85,
	0x3C, 0x6E, 0xF3, 0x72, 0xA5, 0x4F, 0xF5, 0x3A,
	0x51, 0x0E, 0x52, 0x7F, 0x9B, 0x05, 0x68, 0x8C,
	0x1F, 0x83, 0xD9, 0xAB, 0x5B, 0xE0, 0xCD, 0x19
};

enum qcedev_crypto_oper_type {
  QCEDEV_CRYPTO_OPER_CIPHER	= 0,
  QCEDEV_CRYPTO_OPER_SHA	= 1,
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
	uint8_t		digest[QCEDEV_MAX_SHA_DIGEST];
	uint32_t	diglen;
	uint8_t		trailing_buf[64];
	uint32_t	trailing_buf_len;
	uint8_t		first_blk;
	uint8_t		last_blk;
	uint8_t		authkey[QCEDEV_MAX_SHA_BLOCK_SIZE];
};

struct qcedev_async_req {
	struct list_head			list;
	struct completion			complete;
	enum qcedev_crypto_oper_type		op_type;
	union {
		struct qcedev_cipher_op_req	cipher_op_req;
		struct qcedev_sha_op_req	sha_op_req;
	};
	union{
		struct qcedev_cipher_req	cipher_req;
		struct qcedev_sha_req		sha_req;
	};
	struct qcedev_handle			*handle;
	int					err;
};

static DEFINE_MUTEX(send_cmd_lock);
static DEFINE_MUTEX(sent_bw_req);
/**********************************************************************
 * Register ourselves as a misc device to be able to access the dev driver
 * from userspace. */


#define QCEDEV_DEV	"qcedev"

struct qcedev_control{

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
	struct	qcedev_sha_ctxt sha_ctxt;
};

/*-------------------------------------------------------------------------
* Resource Locking Service
* ------------------------------------------------------------------------*/
#define QCEDEV_CMD_ID				1
#define QCEDEV_CE_LOCK_CMD			1
#define QCEDEV_CE_UNLOCK_CMD			0
#define NUM_RETRY				1000
#define CE_BUSY					55

static int qcedev_scm_cmd(int resource, int cmd, int *response)
{
#ifdef CONFIG_MSM_SCM

	struct {
		int resource;
		int cmd;
	} cmd_buf;

	cmd_buf.resource = resource;
	cmd_buf.cmd = cmd;

	return scm_call(SCM_SVC_TZ, QCEDEV_CMD_ID, &cmd_buf,
		sizeof(cmd_buf), response, sizeof(*response));

#else
	return 0;
#endif
}

static int qcedev_ce_high_bw_req(struct qcedev_control *podev,
							bool high_bw_req)
{
	int ret = 0;

	mutex_lock(&sent_bw_req);
	if (high_bw_req) {
		if (podev->high_bw_req_count == 0)
			msm_bus_scale_client_update_request(
					podev->bus_scale_handle, 1);
		podev->high_bw_req_count++;
	} else {
		if (podev->high_bw_req_count == 1)
			msm_bus_scale_client_update_request(
					podev->bus_scale_handle, 0);
		podev->high_bw_req_count--;
	}
	mutex_unlock(&sent_bw_req);

	return ret;
}


static int qcedev_unlock_ce(struct qcedev_control *podev)
{
	int ret = 0;

	mutex_lock(&send_cmd_lock);
	if (podev->ce_lock_count == 1) {
		int response = 0;

		if (qcedev_scm_cmd(podev->platform_support.shared_ce_resource,
					QCEDEV_CE_UNLOCK_CMD, &response)) {
			pr_err("Failed to release CE lock\n");
			ret = -EIO;
		}
	}
	if (ret == 0) {
		if (podev->ce_lock_count)
			podev->ce_lock_count--;
		else {
			/* We should never be here */
			ret = -EIO;
			pr_err("CE hardware is already unlocked\n");
		}
	}
	mutex_unlock(&send_cmd_lock);

	return ret;
}

static int qcedev_lock_ce(struct qcedev_control *podev)
{
	int ret = 0;

	mutex_lock(&send_cmd_lock);
	if (podev->ce_lock_count == 0) {
		int response = -CE_BUSY;
		int i = 0;

		do {
			if (qcedev_scm_cmd(
				podev->platform_support.shared_ce_resource,
				QCEDEV_CE_LOCK_CMD, &response)) {
				response = -EINVAL;
				break;
			}
		} while ((response == -CE_BUSY) && (i++ < NUM_RETRY));

		if ((response == -CE_BUSY) && (i >= NUM_RETRY)) {
			ret = -EUSERS;
		} else {
			if (response < 0)
				ret = -EINVAL;
		}
	}
	if (ret == 0)
		podev->ce_lock_count++;
	mutex_unlock(&send_cmd_lock);
	return ret;
}

#define QCEDEV_MAGIC 0x56434544 /* "qced" */

static long qcedev_ioctl(struct file *file, unsigned cmd, unsigned long arg);
static int qcedev_open(struct inode *inode, struct file *file);
static int qcedev_release(struct inode *inode, struct file *file);
static int start_cipher_req(struct qcedev_control *podev);
static int start_sha_req(struct qcedev_control *podev);

static const struct file_operations qcedev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = qcedev_ioctl,
	.open = qcedev_open,
	.release = qcedev_release,
};

static struct qcedev_control qce_dev[] = {
	{
		.miscdevice = {
			.minor = MISC_DYNAMIC_MINOR,
			.name = "qce",
			.fops = &qcedev_fops,
		},
		.magic = QCEDEV_MAGIC,
	},
};

#define MAX_QCE_DEVICE ARRAY_SIZE(qce_dev)
#define DEBUG_MAX_FNAME  16
#define DEBUG_MAX_RW_BUF 1024

struct qcedev_stat {
	u32 qcedev_dec_success;
	u32 qcedev_dec_fail;
	u32 qcedev_enc_success;
	u32 qcedev_enc_fail;
	u32 qcedev_sha_success;
	u32 qcedev_sha_fail;
};

static struct qcedev_stat _qcedev_stat[MAX_QCE_DEVICE];
static struct dentry *_debug_dent;
static char _debug_read_buf[DEBUG_MAX_RW_BUF];
static int _debug_qcedev[MAX_QCE_DEVICE];

static struct qcedev_control *qcedev_minor_to_control(unsigned n)
{
	int i;

	for (i = 0; i < MAX_QCE_DEVICE; i++) {
		if (qce_dev[i].miscdevice.minor == n)
			return &qce_dev[i];
	}
	return NULL;
}

static int qcedev_open(struct inode *inode, struct file *file)
{
	struct qcedev_handle *handle;
	struct qcedev_control *podev;

	podev = qcedev_minor_to_control(MINOR(inode->i_rdev));
	if (podev == NULL) {
		pr_err("%s: no such device %d\n", __func__,
					MINOR(inode->i_rdev));
		return -ENOENT;
	}

	handle = kzalloc(sizeof(struct qcedev_handle), GFP_KERNEL);
	if (handle == NULL) {
		pr_err("Failed to allocate memory %ld\n",
					PTR_ERR(handle));
		return -ENOMEM;
	}

	handle->cntl = podev;
	file->private_data = handle;
	if (podev->platform_support.bus_scale_table != NULL)
		return qcedev_ce_high_bw_req(podev, true);
	return 0;
}

static int qcedev_release(struct inode *inode, struct file *file)
{
	struct qcedev_control *podev;
	struct qcedev_handle *handle;

	handle =  file->private_data;
	podev =  handle->cntl;
	if (podev != NULL && podev->magic != QCEDEV_MAGIC) {
		pr_err("%s: invalid handle %p\n",
					__func__, podev);
	}
	kzfree(handle);
	file->private_data = NULL;
	if (podev->platform_support.bus_scale_table != NULL)
		return qcedev_ce_high_bw_req(podev, false);
	return 0;
}

static void req_done(unsigned long data)
{
	struct qcedev_control *podev = (struct qcedev_control *)data;
	struct qcedev_async_req *areq;
	unsigned long flags = 0;
	struct qcedev_async_req *new_req = NULL;
	int ret = 0;

	spin_lock_irqsave(&podev->lock, flags);
	areq = podev->active_command;
	podev->active_command = NULL;

again:
	if (!list_empty(&podev->ready_commands)) {
		new_req = container_of(podev->ready_commands.next,
						struct qcedev_async_req, list);
		list_del(&new_req->list);
		podev->active_command = new_req;
		new_req->err = 0;
		if (new_req->op_type == QCEDEV_CRYPTO_OPER_CIPHER)
			ret = start_cipher_req(podev);
		else
			ret = start_sha_req(podev);
	}

	spin_unlock_irqrestore(&podev->lock, flags);

	if (areq)
		complete(&areq->complete);

	if (new_req && ret) {
		complete(&new_req->complete);
		spin_lock_irqsave(&podev->lock, flags);
		podev->active_command = NULL;
		areq = NULL;
		ret = 0;
		new_req = NULL;
		goto again;
	}

	return;
}

static void qcedev_sha_req_cb(void *cookie, unsigned char *digest,
	unsigned char *authdata, int ret)
{
	struct qcedev_sha_req *areq;
	struct qcedev_control *pdev;
	struct qcedev_handle *handle;

	uint32_t *auth32 = (uint32_t *)authdata;

	areq = (struct qcedev_sha_req *) cookie;
	handle = (struct qcedev_handle *) areq->cookie;
	pdev = handle->cntl;

	if (digest)
		memcpy(&handle->sha_ctxt.digest[0], digest, 32);

	if (authdata) {
		handle->sha_ctxt.auth_data[0] = auth32[0];
		handle->sha_ctxt.auth_data[1] = auth32[1];
		handle->sha_ctxt.auth_data[2] = auth32[2];
		handle->sha_ctxt.auth_data[3] = auth32[3];
	}

	tasklet_schedule(&pdev->done_tasklet);
};


static void qcedev_cipher_req_cb(void *cookie, unsigned char *icv,
	unsigned char *iv, int ret)
{
	struct qcedev_cipher_req *areq;
	struct qcedev_handle *handle;
	struct qcedev_control *podev;
	struct qcedev_async_req *qcedev_areq;

	areq = (struct qcedev_cipher_req *) cookie;
	handle = (struct qcedev_handle *) areq->cookie;
	podev = handle->cntl;
	qcedev_areq = podev->active_command;

	if (iv)
		memcpy(&qcedev_areq->cipher_op_req.iv[0], iv,
					qcedev_areq->cipher_op_req.ivlen);
	tasklet_schedule(&podev->done_tasklet);
};

static int start_cipher_req(struct qcedev_control *podev)
{
	struct qcedev_async_req *qcedev_areq;
	struct qce_req creq;
	int ret = 0;

	/* start the command on the podev->active_command */
	qcedev_areq = podev->active_command;

	qcedev_areq->cipher_req.cookie = qcedev_areq->handle;
	creq.use_pmem = qcedev_areq->cipher_op_req.use_pmem;
	if (qcedev_areq->cipher_op_req.use_pmem == QCEDEV_USE_PMEM)
		creq.pmem = &qcedev_areq->cipher_op_req.pmem;
	else
		creq.pmem = NULL;

	switch (qcedev_areq->cipher_op_req.alg) {
	case QCEDEV_ALG_DES:
		creq.alg = CIPHER_ALG_DES;
		break;
	case QCEDEV_ALG_3DES:
		creq.alg = CIPHER_ALG_3DES;
		break;
	case QCEDEV_ALG_AES:
		creq.alg = CIPHER_ALG_AES;
		break;
	default:
		return -EINVAL;
	};

	switch (qcedev_areq->cipher_op_req.mode) {
	case QCEDEV_AES_MODE_CBC:
	case QCEDEV_DES_MODE_CBC:
		creq.mode = QCE_MODE_CBC;
		break;
	case QCEDEV_AES_MODE_ECB:
	case QCEDEV_DES_MODE_ECB:
		creq.mode = QCE_MODE_ECB;
		break;
	case QCEDEV_AES_MODE_CTR:
		creq.mode = QCE_MODE_CTR;
		break;
	case QCEDEV_AES_MODE_XTS:
		creq.mode = QCE_MODE_XTS;
		break;
	default:
		return -EINVAL;
	};

	if ((creq.alg == CIPHER_ALG_AES) &&
		(creq.mode == QCE_MODE_CTR)) {
		creq.dir = QCE_ENCRYPT;
	} else {
		if (QCEDEV_OPER_ENC == qcedev_areq->cipher_op_req.op)
			creq.dir = QCE_ENCRYPT;
		else
			creq.dir = QCE_DECRYPT;
	}

	creq.iv = &qcedev_areq->cipher_op_req.iv[0];
	creq.ivsize = qcedev_areq->cipher_op_req.ivlen;

	creq.enckey =  &qcedev_areq->cipher_op_req.enckey[0];
	creq.encklen = qcedev_areq->cipher_op_req.encklen;

	creq.cryptlen = qcedev_areq->cipher_op_req.data_len;

	if (qcedev_areq->cipher_op_req.encklen == 0) {
		if ((qcedev_areq->cipher_op_req.op == QCEDEV_OPER_ENC_NO_KEY)
			|| (qcedev_areq->cipher_op_req.op ==
				QCEDEV_OPER_DEC_NO_KEY))
			creq.op = QCE_REQ_ABLK_CIPHER_NO_KEY;
		else {
			int i;

			for (i = 0; i < QCEDEV_MAX_KEY_SIZE; i++) {
				if (qcedev_areq->cipher_op_req.enckey[i] != 0)
					break;
			}

			if ((podev->platform_support.hw_key_support == 1) &&
						(i == QCEDEV_MAX_KEY_SIZE))
				creq.op = QCE_REQ_ABLK_CIPHER;
			else {
				ret = -EINVAL;
				goto unsupported;
			}
		}
	} else {
		creq.op = QCE_REQ_ABLK_CIPHER;
	}

	creq.qce_cb = qcedev_cipher_req_cb;
	creq.areq = (void *)&qcedev_areq->cipher_req;

	ret = qce_ablk_cipher_req(podev->qce, &creq);
unsupported:
	if (ret)
		qcedev_areq->err = -ENXIO;
	else
		qcedev_areq->err = 0;
	return ret;
};

static int start_sha_req(struct qcedev_control *podev)
{
	struct qcedev_async_req *qcedev_areq;
	struct qce_sha_req sreq;
	int ret = 0;
	struct qcedev_handle *handle;

	/* start the command on the podev->active_command */
	qcedev_areq = podev->active_command;
	handle = qcedev_areq->handle;

	switch (qcedev_areq->sha_op_req.alg) {
	case QCEDEV_ALG_SHA1:
		sreq.alg = QCE_HASH_SHA1;
		break;
	case QCEDEV_ALG_SHA256:
		sreq.alg = QCE_HASH_SHA256;
		break;
	case QCEDEV_ALG_SHA1_HMAC:
		if (podev->ce_support.sha_hmac) {
			sreq.alg = QCE_HASH_SHA1_HMAC;
			sreq.authkey = &handle->sha_ctxt.authkey[0];

		} else {
			sreq.alg = QCE_HASH_SHA1;
			sreq.authkey = NULL;
		}
		break;
	case QCEDEV_ALG_SHA256_HMAC:
		if (podev->ce_support.sha_hmac) {
			sreq.alg = QCE_HASH_SHA256_HMAC;
			sreq.authkey = &handle->sha_ctxt.authkey[0];

		} else {
			sreq.alg = QCE_HASH_SHA256;
			sreq.authkey = NULL;
		}
		break;
	case QCEDEV_ALG_AES_CMAC:
		sreq.alg = QCE_HASH_AES_CMAC;
		sreq.authkey = &handle->sha_ctxt.authkey[0];
		sreq.authklen = qcedev_areq->sha_op_req.authklen;
		break;
	default:
		break;
	};

	qcedev_areq->sha_req.cookie = handle;

	sreq.qce_cb = qcedev_sha_req_cb;
	if (qcedev_areq->sha_op_req.alg != QCEDEV_ALG_AES_CMAC) {
		sreq.auth_data[0] = handle->sha_ctxt.auth_data[0];
		sreq.auth_data[1] = handle->sha_ctxt.auth_data[1];
		sreq.auth_data[2] = handle->sha_ctxt.auth_data[2];
		sreq.auth_data[3] = handle->sha_ctxt.auth_data[3];
		sreq.digest = &handle->sha_ctxt.digest[0];
		sreq.first_blk = handle->sha_ctxt.first_blk;
		sreq.last_blk = handle->sha_ctxt.last_blk;
	}
	sreq.size = qcedev_areq->sha_req.sreq.nbytes;
	sreq.src = qcedev_areq->sha_req.sreq.src;
	sreq.areq = (void *)&qcedev_areq->sha_req;

	ret = qce_process_sha_req(podev->qce, &sreq);

	if (ret)
		qcedev_areq->err = -ENXIO;
	else
		qcedev_areq->err = 0;
	return ret;
};

static int submit_req(struct qcedev_async_req *qcedev_areq,
					struct qcedev_handle *handle)
{
	struct qcedev_control *podev;
	unsigned long flags = 0;
	int ret = 0;
	struct qcedev_stat *pstat;

	qcedev_areq->err = 0;
	podev = handle->cntl;

	if (podev->platform_support.ce_shared) {
		ret = qcedev_lock_ce(podev);
		if (ret)
			return ret;
	}

	spin_lock_irqsave(&podev->lock, flags);

	if (podev->active_command == NULL) {
		podev->active_command = qcedev_areq;
		if (qcedev_areq->op_type == QCEDEV_CRYPTO_OPER_CIPHER)
			ret = start_cipher_req(podev);
		else
			ret = start_sha_req(podev);
	} else {
		list_add_tail(&qcedev_areq->list, &podev->ready_commands);
	}

	if (ret != 0)
		podev->active_command = NULL;

	spin_unlock_irqrestore(&podev->lock, flags);

	if (ret == 0)
		wait_for_completion(&qcedev_areq->complete);

	if (podev->platform_support.ce_shared)
		ret = qcedev_unlock_ce(podev);

	if (ret)
		qcedev_areq->err = -EIO;

	pstat = &_qcedev_stat[podev->pdev->id];
	if (qcedev_areq->op_type == QCEDEV_CRYPTO_OPER_CIPHER) {
		switch (qcedev_areq->cipher_op_req.op) {
		case QCEDEV_OPER_DEC:
			if (qcedev_areq->err)
				pstat->qcedev_dec_fail++;
			else
				pstat->qcedev_dec_success++;
			break;
		case QCEDEV_OPER_ENC:
			if (qcedev_areq->err)
				pstat->qcedev_enc_fail++;
			else
				pstat->qcedev_enc_success++;
			break;
		default:
			break;
		};
	} else {
		if (qcedev_areq->err)
			pstat->qcedev_sha_fail++;
		else
			pstat->qcedev_sha_success++;
	}

	return qcedev_areq->err;
}

static int qcedev_sha_init(struct qcedev_async_req *areq,
				struct qcedev_handle *handle)
{
	struct qcedev_sha_ctxt *sha_ctxt = &handle->sha_ctxt;

	memset(sha_ctxt, 0, sizeof(struct qcedev_sha_ctxt));
	sha_ctxt->first_blk = 1;

	if ((areq->sha_op_req.alg == QCEDEV_ALG_SHA1) ||
			(areq->sha_op_req.alg == QCEDEV_ALG_SHA1_HMAC)) {
		memcpy(&sha_ctxt->digest[0],
			&_std_init_vector_sha1_uint8[0], SHA1_DIGEST_SIZE);
		sha_ctxt->diglen = SHA1_DIGEST_SIZE;
	} else {
		if ((areq->sha_op_req.alg == QCEDEV_ALG_SHA256) ||
			(areq->sha_op_req.alg == QCEDEV_ALG_SHA256_HMAC)) {
			memcpy(&sha_ctxt->digest[0],
					&_std_init_vector_sha256_uint8[0],
					SHA256_DIGEST_SIZE);
			sha_ctxt->diglen = SHA256_DIGEST_SIZE;
		}
	}
	return 0;
}


static int qcedev_sha_update_max_xfer(struct qcedev_async_req *qcedev_areq,
				struct qcedev_handle *handle)
{
	int err = 0;
	int i = 0;
	struct scatterlist sg_src[2];
	uint32_t total;

	uint8_t *user_src = NULL;
	uint8_t *k_src = NULL;
	uint8_t *k_buf_src = NULL;
	uint8_t *k_align_src = NULL;

	uint32_t sha_pad_len = 0;
	uint32_t trailing_buf_len = 0;
	uint32_t t_buf = handle->sha_ctxt.trailing_buf_len;
	uint32_t sha_block_size;

	total = qcedev_areq->sha_op_req.data_len + t_buf;

	if (qcedev_areq->sha_op_req.alg == QCEDEV_ALG_SHA1)
		sha_block_size = SHA1_BLOCK_SIZE;
	else
		sha_block_size = SHA256_BLOCK_SIZE;

	if (total <= sha_block_size) {
		uint32_t len =  qcedev_areq->sha_op_req.data_len;

		i = 0;

		k_src = &handle->sha_ctxt.trailing_buf[t_buf];

		/* Copy data from user src(s) */
		while (len > 0) {
			user_src =
			(void __user *)qcedev_areq->sha_op_req.data[i].vaddr;
			if (user_src && __copy_from_user(k_src,
				(void __user *)user_src,
				qcedev_areq->sha_op_req.data[i].len))
				return -EFAULT;

			len -= qcedev_areq->sha_op_req.data[i].len;
			k_src += qcedev_areq->sha_op_req.data[i].len;
			i++;
		}
		handle->sha_ctxt.trailing_buf_len = total;

		return 0;
	}


	k_buf_src = kmalloc(total + CACHE_LINE_SIZE * 2,
				GFP_KERNEL);
	if (k_buf_src == NULL) {
		pr_err("%s: Can't Allocate memory: k_buf_src 0x%x\n",
			__func__, (uint32_t)k_buf_src);
		return -ENOMEM;
	}

	k_align_src = (uint8_t *) ALIGN(((unsigned int)k_buf_src),
							CACHE_LINE_SIZE);
	k_src = k_align_src;

	/* check for trailing buffer from previous updates and append it */
	if (t_buf > 0) {
		memcpy(k_src, &handle->sha_ctxt.trailing_buf[0],
								t_buf);
		k_src += t_buf;
	}

	/* Copy data from user src(s) */
	user_src = (void __user *)qcedev_areq->sha_op_req.data[0].vaddr;
	if (user_src && __copy_from_user(k_src,
				(void __user *)user_src,
				qcedev_areq->sha_op_req.data[0].len)) {
		kfree(k_buf_src);
		return -EFAULT;
	}
	k_src += qcedev_areq->sha_op_req.data[0].len;
	for (i = 1; i < qcedev_areq->sha_op_req.entries; i++) {
		user_src = (void __user *)qcedev_areq->sha_op_req.data[i].vaddr;
		if (user_src && __copy_from_user(k_src,
					(void __user *)user_src,
					qcedev_areq->sha_op_req.data[i].len)) {
			kfree(k_buf_src);
			return -EFAULT;
		}
		k_src += qcedev_areq->sha_op_req.data[i].len;
	}

	/*  get new trailing buffer */
	sha_pad_len = ALIGN(total, CE_SHA_BLOCK_SIZE) - total;
	trailing_buf_len =  CE_SHA_BLOCK_SIZE - sha_pad_len;

	qcedev_areq->sha_req.sreq.src = (struct scatterlist *) &sg_src[0];
	sg_set_buf(qcedev_areq->sha_req.sreq.src, k_align_src,
						total-trailing_buf_len);
	sg_mark_end(qcedev_areq->sha_req.sreq.src);

	qcedev_areq->sha_req.sreq.nbytes = total - trailing_buf_len;

	/*  update sha_ctxt trailing buf content to new trailing buf */
	if (trailing_buf_len > 0) {
		memset(&handle->sha_ctxt.trailing_buf[0], 0, 64);
		memcpy(&handle->sha_ctxt.trailing_buf[0],
			(k_src - trailing_buf_len),
			trailing_buf_len);
	}
	handle->sha_ctxt.trailing_buf_len = trailing_buf_len;

	err = submit_req(qcedev_areq, handle);

	handle->sha_ctxt.last_blk = 0;
	handle->sha_ctxt.first_blk = 0;

	kfree(k_buf_src);
	return err;
}

static int qcedev_sha_update(struct qcedev_async_req *qcedev_areq,
				struct qcedev_handle *handle)
{
	int err = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	int num_entries = 0;
	uint32_t total = 0;

	/* verify address src(s) */
	for (i = 0; i < qcedev_areq->sha_op_req.entries; i++)
		if (!access_ok(VERIFY_READ,
			(void __user *)qcedev_areq->sha_op_req.data[i].vaddr,
			qcedev_areq->sha_op_req.data[i].len))
			return -EFAULT;

	if (qcedev_areq->sha_op_req.data_len > QCE_MAX_OPER_DATA) {

		struct	qcedev_sha_op_req *saved_req;
		struct	qcedev_sha_op_req req;
		struct	qcedev_sha_op_req *sreq = &qcedev_areq->sha_op_req;

		/* save the original req structure */
		saved_req =
			kmalloc(sizeof(struct qcedev_sha_op_req), GFP_KERNEL);
		if (saved_req == NULL) {
			pr_err("%s:Can't Allocate mem:saved_req 0x%x\n",
			__func__, (uint32_t)saved_req);
			return -ENOMEM;
		}
		memcpy(&req, sreq, sizeof(struct qcedev_sha_op_req));
		memcpy(saved_req, sreq, sizeof(struct qcedev_sha_op_req));

		i = 0;
		/* Address 32 KB  at a time */
		while ((i < req.entries) && (err == 0)) {
			if (sreq->data[i].len > QCE_MAX_OPER_DATA) {
				sreq->data[0].len = QCE_MAX_OPER_DATA;
				if (i > 0) {
					sreq->data[0].vaddr =
							sreq->data[i].vaddr;
				}

				sreq->data_len = QCE_MAX_OPER_DATA;
				sreq->entries = 1;

				err = qcedev_sha_update_max_xfer(qcedev_areq,
									handle);

				sreq->data[i].len = req.data[i].len -
							QCE_MAX_OPER_DATA;
				sreq->data[i].vaddr = req.data[i].vaddr +
							QCE_MAX_OPER_DATA;
				req.data[i].vaddr = sreq->data[i].vaddr;
				req.data[i].len = sreq->data[i].len;
			} else {
				total = 0;
				for (j = i; j < req.entries; j++) {
					num_entries++;
					if ((total + sreq->data[j].len) >=
							QCE_MAX_OPER_DATA) {
						sreq->data[j].len =
						(QCE_MAX_OPER_DATA - total);
						total = QCE_MAX_OPER_DATA;
						break;
					}
					total += sreq->data[j].len;
				}

				sreq->data_len = total;
				if (i > 0)
					for (k = 0; k < num_entries; k++) {
						sreq->data[k].len =
							sreq->data[i+k].len;
						sreq->data[k].vaddr =
							sreq->data[i+k].vaddr;
					}
				sreq->entries = num_entries;

				i = j;
				err = qcedev_sha_update_max_xfer(qcedev_areq,
									handle);
				num_entries = 0;

				sreq->data[i].vaddr = req.data[i].vaddr +
							sreq->data[i].len;
				sreq->data[i].len = req.data[i].len -
							sreq->data[i].len;
				req.data[i].vaddr = sreq->data[i].vaddr;
				req.data[i].len = sreq->data[i].len;

				if (sreq->data[i].len == 0)
					i++;
			}
		} /* end of while ((i < req.entries) && (err == 0)) */

		/* Restore the original req structure */
		for (i = 0; i < saved_req->entries; i++) {
			sreq->data[i].len = saved_req->data[i].len;
			sreq->data[i].vaddr = saved_req->data[i].vaddr;
		}
		sreq->entries = saved_req->entries;
		sreq->data_len = saved_req->data_len;
		kfree(saved_req);
	} else
		err = qcedev_sha_update_max_xfer(qcedev_areq, handle);

	return err;
}

static int qcedev_sha_final(struct qcedev_async_req *qcedev_areq,
				struct qcedev_handle *handle)
{
	int err = 0;
	struct scatterlist sg_src;
	uint32_t total;

	uint8_t *k_buf_src = NULL;
	uint8_t *k_align_src = NULL;

	handle->sha_ctxt.first_blk = 0;
	handle->sha_ctxt.last_blk = 1;

	total = handle->sha_ctxt.trailing_buf_len;

	if (total) {
		k_buf_src = kmalloc(total + CACHE_LINE_SIZE * 2,
					GFP_KERNEL);
		if (k_buf_src == NULL) {
			pr_err("%s: Can't Allocate memory: k_buf_src 0x%x\n",
			__func__, (uint32_t)k_buf_src);
			return -ENOMEM;
		}

		k_align_src = (uint8_t *) ALIGN(((unsigned int)k_buf_src),
							CACHE_LINE_SIZE);
		memcpy(k_align_src, &handle->sha_ctxt.trailing_buf[0], total);
	}
	handle->sha_ctxt.last_blk = 1;
	handle->sha_ctxt.first_blk = 0;

	qcedev_areq->sha_req.sreq.src = (struct scatterlist *) &sg_src;
	sg_set_buf(qcedev_areq->sha_req.sreq.src, k_align_src, total);
	sg_mark_end(qcedev_areq->sha_req.sreq.src);

	qcedev_areq->sha_req.sreq.nbytes = total;

	err = submit_req(qcedev_areq, handle);

	handle->sha_ctxt.first_blk = 0;
	handle->sha_ctxt.last_blk = 0;
	handle->sha_ctxt.auth_data[0] = 0;
	handle->sha_ctxt.auth_data[1] = 0;
	handle->sha_ctxt.trailing_buf_len = 0;
	memset(&handle->sha_ctxt.trailing_buf[0], 0, 64);

	kfree(k_buf_src);
	return err;
}

static int qcedev_hash_cmac(struct qcedev_async_req *qcedev_areq,
					struct qcedev_handle *handle)
{
	int err = 0;
	int i = 0;
	struct scatterlist sg_src[2];
	uint32_t total;

	uint8_t *user_src = NULL;
	uint8_t *k_src = NULL;
	uint8_t *k_buf_src = NULL;

	total = qcedev_areq->sha_op_req.data_len;

	/* verify address src(s) */
	for (i = 0; i < qcedev_areq->sha_op_req.entries; i++)
		if (!access_ok(VERIFY_READ,
			(void __user *)qcedev_areq->sha_op_req.data[i].vaddr,
			qcedev_areq->sha_op_req.data[i].len))
			return -EFAULT;

	/* Verify Source Address */
	if (!access_ok(VERIFY_READ,
				(void __user *)qcedev_areq->sha_op_req.authkey,
				qcedev_areq->sha_op_req.authklen))
			return -EFAULT;
	if (__copy_from_user(&handle->sha_ctxt.authkey[0],
				(void __user *)qcedev_areq->sha_op_req.authkey,
				qcedev_areq->sha_op_req.authklen))
		return -EFAULT;


	k_buf_src = kmalloc(total, GFP_KERNEL);
	if (k_buf_src == NULL) {
		pr_err("%s: Can't Allocate memory: k_buf_src 0x%x\n",
			__func__, (uint32_t)k_buf_src);
		return -ENOMEM;
	}

	k_src = k_buf_src;

	/* Copy data from user src(s) */
	user_src = (void __user *)qcedev_areq->sha_op_req.data[0].vaddr;
	for (i = 0; i < qcedev_areq->sha_op_req.entries; i++) {
		user_src =
			(void __user *)qcedev_areq->sha_op_req.data[i].vaddr;
		if (user_src && __copy_from_user(k_src, (void __user *)user_src,
				qcedev_areq->sha_op_req.data[i].len)) {
			kfree(k_buf_src);
			return -EFAULT;
		}
		k_src += qcedev_areq->sha_op_req.data[i].len;
	}

	qcedev_areq->sha_req.sreq.src = (struct scatterlist *) &sg_src[0];
	sg_set_buf(qcedev_areq->sha_req.sreq.src, k_buf_src, total);
	sg_mark_end(qcedev_areq->sha_req.sreq.src);

	qcedev_areq->sha_req.sreq.nbytes = total;
	handle->sha_ctxt.diglen = qcedev_areq->sha_op_req.diglen;
	err = submit_req(qcedev_areq, handle);

	kfree(k_buf_src);
	return err;
}

static int qcedev_set_hmac_auth_key(struct qcedev_async_req *areq,
					struct qcedev_handle *handle)
{
	int err = 0;

	if (areq->sha_op_req.authklen <= QCEDEV_MAX_KEY_SIZE) {
		/* Verify Source Address */
		if (!access_ok(VERIFY_READ,
				(void __user *)areq->sha_op_req.authkey,
				areq->sha_op_req.authklen))
			return -EFAULT;
		if (__copy_from_user(&handle->sha_ctxt.authkey[0],
				(void __user *)areq->sha_op_req.authkey,
				areq->sha_op_req.authklen))
			return -EFAULT;
	} else {
		struct qcedev_async_req authkey_areq;

		init_completion(&authkey_areq.complete);

		authkey_areq.sha_op_req.entries = 1;
		authkey_areq.sha_op_req.data[0].vaddr =
						areq->sha_op_req.authkey;
		authkey_areq.sha_op_req.data[0].len = areq->sha_op_req.authklen;
		authkey_areq.sha_op_req.data_len = areq->sha_op_req.authklen;
		authkey_areq.sha_op_req.diglen = 0;
		memset(&authkey_areq.sha_op_req.digest[0], 0,
						QCEDEV_MAX_SHA_DIGEST);
		if (areq->sha_op_req.alg == QCEDEV_ALG_SHA1_HMAC)
				authkey_areq.sha_op_req.alg = QCEDEV_ALG_SHA1;
		if (areq->sha_op_req.alg == QCEDEV_ALG_SHA256_HMAC)
				authkey_areq.sha_op_req.alg = QCEDEV_ALG_SHA256;

		authkey_areq.op_type = QCEDEV_CRYPTO_OPER_SHA;

		qcedev_sha_init(&authkey_areq, handle);
		err = qcedev_sha_update(&authkey_areq, handle);
		if (!err)
			err = qcedev_sha_final(&authkey_areq, handle);
		else
			return err;
		memcpy(&handle->sha_ctxt.authkey[0],
				&handle->sha_ctxt.digest[0],
				handle->sha_ctxt.diglen);
	}
	return err;
}

static int qcedev_hmac_get_ohash(struct qcedev_async_req *qcedev_areq,
				struct qcedev_handle *handle)
{
	int err = 0;
	struct scatterlist sg_src;
	uint8_t *k_src = NULL;
	uint32_t sha_block_size = 0;
	uint32_t sha_digest_size = 0;

	if (qcedev_areq->sha_op_req.alg == QCEDEV_ALG_SHA1_HMAC) {
		sha_digest_size = SHA1_DIGEST_SIZE;
		sha_block_size = SHA1_BLOCK_SIZE;
	} else {
		if (qcedev_areq->sha_op_req.alg == QCEDEV_ALG_SHA256_HMAC) {
			sha_digest_size = SHA256_DIGEST_SIZE;
			sha_block_size = SHA256_BLOCK_SIZE;
		}
	}
	k_src = kmalloc(sha_block_size, GFP_KERNEL);
	if (k_src == NULL) {
		pr_err("%s: Can't Allocate memory: k_src 0x%x\n",
			__func__, (uint32_t)k_src);
		return -ENOMEM;
	}

	/* check for trailing buffer from previous updates and append it */
	memcpy(k_src, &handle->sha_ctxt.trailing_buf[0],
			handle->sha_ctxt.trailing_buf_len);

	qcedev_areq->sha_req.sreq.src = (struct scatterlist *) &sg_src;
	sg_set_buf(qcedev_areq->sha_req.sreq.src, k_src, sha_block_size);
	sg_mark_end(qcedev_areq->sha_req.sreq.src);

	qcedev_areq->sha_req.sreq.nbytes = sha_block_size;
	memset(&handle->sha_ctxt.trailing_buf[0], 0, sha_block_size);
	memcpy(&handle->sha_ctxt.trailing_buf[0], &handle->sha_ctxt.digest[0],
					sha_digest_size);
	handle->sha_ctxt.trailing_buf_len = sha_digest_size;

	handle->sha_ctxt.first_blk = 1;
	handle->sha_ctxt.last_blk = 0;
	handle->sha_ctxt.auth_data[0] = 0;
	handle->sha_ctxt.auth_data[1] = 0;

	if (qcedev_areq->sha_op_req.alg == QCEDEV_ALG_SHA1_HMAC) {
		memcpy(&handle->sha_ctxt.digest[0],
			&_std_init_vector_sha1_uint8[0], SHA1_DIGEST_SIZE);
		handle->sha_ctxt.diglen = SHA1_DIGEST_SIZE;
	}

	if (qcedev_areq->sha_op_req.alg == QCEDEV_ALG_SHA256_HMAC) {
		memcpy(&handle->sha_ctxt.digest[0],
			&_std_init_vector_sha256_uint8[0], SHA256_DIGEST_SIZE);
		handle->sha_ctxt.diglen = SHA256_DIGEST_SIZE;
	}
	err = submit_req(qcedev_areq, handle);

	handle->sha_ctxt.last_blk = 0;
	handle->sha_ctxt.first_blk = 0;

	kfree(k_src);
	return err;
}

static int qcedev_hmac_update_iokey(struct qcedev_async_req *areq,
				struct qcedev_handle *handle, bool ikey)
{
	int i;
	uint32_t constant;
	uint32_t sha_block_size;

	if (ikey)
		constant = 0x36;
	else
		constant = 0x5c;

	if (areq->sha_op_req.alg == QCEDEV_ALG_SHA1_HMAC)
		sha_block_size = SHA1_BLOCK_SIZE;
	else
		sha_block_size = SHA256_BLOCK_SIZE;

	memset(&handle->sha_ctxt.trailing_buf[0], 0, sha_block_size);
	for (i = 0; i < sha_block_size; i++)
		handle->sha_ctxt.trailing_buf[i] =
				(handle->sha_ctxt.authkey[i] ^ constant);

	handle->sha_ctxt.trailing_buf_len = sha_block_size;
	return 0;
}

static int qcedev_hmac_init(struct qcedev_async_req *areq,
				struct qcedev_handle *handle)
{
	int err;
	struct qcedev_control *podev = handle->cntl;

	qcedev_sha_init(areq, handle);
	err = qcedev_set_hmac_auth_key(areq, handle);
	if (err)
		return err;
	if (!podev->ce_support.sha_hmac)
		qcedev_hmac_update_iokey(areq, handle, true);
	return 0;
}

static int qcedev_hmac_final(struct qcedev_async_req *areq,
				struct qcedev_handle *handle)
{
	int err;
	struct qcedev_control *podev = handle->cntl;

	err = qcedev_sha_final(areq, handle);
	if (podev->ce_support.sha_hmac)
		return err;

	qcedev_hmac_update_iokey(areq, handle, false);
	err = qcedev_hmac_get_ohash(areq, handle);
	if (err)
		return err;
	err = qcedev_sha_final(areq, handle);

	return err;
}

static int qcedev_hash_init(struct qcedev_async_req *areq,
				struct qcedev_handle *handle)
{
	if ((areq->sha_op_req.alg == QCEDEV_ALG_SHA1) ||
			(areq->sha_op_req.alg == QCEDEV_ALG_SHA256))
		return qcedev_sha_init(areq, handle);
	else
		return qcedev_hmac_init(areq, handle);
}

static int qcedev_hash_update(struct qcedev_async_req *qcedev_areq,
				struct qcedev_handle *handle)
{
	return qcedev_sha_update(qcedev_areq, handle);
}

static int qcedev_hash_final(struct qcedev_async_req *areq,
				struct qcedev_handle *handle)
{
	if ((areq->sha_op_req.alg == QCEDEV_ALG_SHA1) ||
			(areq->sha_op_req.alg == QCEDEV_ALG_SHA256))
		return qcedev_sha_final(areq, handle);
	else
		return qcedev_hmac_final(areq, handle);
}

#ifdef CONFIG_ANDROID_PMEM
static int qcedev_pmem_ablk_cipher_max_xfer(struct qcedev_async_req *areq,
						struct qcedev_handle *handle)
{
	int i = 0;
	int err = 0;
	struct scatterlist *sg_src = NULL;
	struct scatterlist *sg_dst = NULL;
	struct scatterlist *sg_ndex = NULL;
	struct file *file_src = NULL;
	struct file *file_dst = NULL;
	unsigned long paddr;
	unsigned long kvaddr;
	unsigned long len;

	sg_src = kmalloc((sizeof(struct scatterlist) *
				areq->cipher_op_req.entries),	GFP_KERNEL);
	if (sg_src == NULL) {
		pr_err("%s: Can't Allocate memory:sg_src 0x%x\n",
			__func__, (uint32_t)sg_src);
		return -ENOMEM;

	}
	memset(sg_src, 0, (sizeof(struct scatterlist) *
				areq->cipher_op_req.entries));
	sg_ndex = sg_src;
	areq->cipher_req.creq.src = sg_src;

	/* address src */
	get_pmem_file(areq->cipher_op_req.pmem.fd_src, &paddr,
					&kvaddr, &len, &file_src);

	for (i = 0; i < areq->cipher_op_req.entries; i++) {
		sg_set_buf(sg_ndex,
		((uint8_t *)(areq->cipher_op_req.pmem.src[i].offset) + kvaddr),
		areq->cipher_op_req.pmem.src[i].len);
		sg_ndex++;
	}
	sg_mark_end(--sg_ndex);

	for (i = 0; i < areq->cipher_op_req.entries; i++)
		areq->cipher_op_req.pmem.src[i].offset += (uint32_t)paddr;

	/* address dst */
	/* If not place encryption/decryption */
	if (areq->cipher_op_req.in_place_op != 1) {
		sg_dst = kmalloc((sizeof(struct scatterlist) *
				areq->cipher_op_req.entries), GFP_KERNEL);
		if (sg_dst == NULL) {
			pr_err("%s: Can't Allocate memory: sg_dst 0x%x\n",
			__func__, (uint32_t)sg_dst);
			return -ENOMEM;
		}
		memset(sg_dst, 0, (sizeof(struct scatterlist) *
					areq->cipher_op_req.entries));
		areq->cipher_req.creq.dst = sg_dst;
		sg_ndex = sg_dst;

		get_pmem_file(areq->cipher_op_req.pmem.fd_dst, &paddr,
					&kvaddr, &len, &file_dst);
		for (i = 0; i < areq->cipher_op_req.entries; i++)
			sg_set_buf(sg_ndex++,
			((uint8_t *)(areq->cipher_op_req.pmem.dst[i].offset)
			+ kvaddr), areq->cipher_op_req.pmem.dst[i].len);
		sg_mark_end(--sg_ndex);

		for (i = 0; i < areq->cipher_op_req.entries; i++)
			areq->cipher_op_req.pmem.dst[i].offset +=
							(uint32_t)paddr;
	} else {
		areq->cipher_req.creq.dst = sg_src;
		for (i = 0; i < areq->cipher_op_req.entries; i++) {
			areq->cipher_op_req.pmem.dst[i].offset =
				areq->cipher_op_req.pmem.src[i].offset;
			areq->cipher_op_req.pmem.dst[i].len =
				areq->cipher_op_req.pmem.src[i].len;
		}
	}

	areq->cipher_req.creq.nbytes = areq->cipher_op_req.data_len;
	areq->cipher_req.creq.info = areq->cipher_op_req.iv;

	err = submit_req(areq, handle);

	kfree(sg_src);
	kfree(sg_dst);

	if (file_dst)
		put_pmem_file(file_dst);
	if (file_src)
		put_pmem_file(file_src);

	return err;
};


static int qcedev_pmem_ablk_cipher(struct qcedev_async_req *qcedev_areq,
						struct qcedev_handle *handle)
{
	int err = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	int num_entries = 0;
	uint32_t total = 0;
	struct qcedev_cipher_op_req *saved_req;
	struct qcedev_cipher_op_req *creq = &qcedev_areq->cipher_op_req;

	saved_req = kmalloc(sizeof(struct qcedev_cipher_op_req), GFP_KERNEL);
	if (saved_req == NULL) {
		pr_err(KERN_ERR "%s:Can't Allocate mem:saved_req 0x%x\n",
		__func__, (uint32_t)saved_req);
		return -ENOMEM;
	}
	memcpy(saved_req, creq, sizeof(struct qcedev_cipher_op_req));

	if (qcedev_areq->cipher_op_req.data_len > QCE_MAX_OPER_DATA) {

		struct qcedev_cipher_op_req req;

		/* save the original req structure */
		memcpy(&req, creq, sizeof(struct qcedev_cipher_op_req));

		i = 0;
		/* Address 32 KB  at a time */
		while ((i < req.entries) && (err == 0)) {
			if (creq->pmem.src[i].len > QCE_MAX_OPER_DATA) {
				creq->pmem.src[0].len =	QCE_MAX_OPER_DATA;
				if (i > 0) {
					creq->pmem.src[0].offset =
						creq->pmem.src[i].offset;
				}

				creq->data_len = QCE_MAX_OPER_DATA;
				creq->entries = 1;

				err =
				qcedev_pmem_ablk_cipher_max_xfer(qcedev_areq,
								handle);

				creq->pmem.src[i].len =	req.pmem.src[i].len -
							QCE_MAX_OPER_DATA;
				creq->pmem.src[i].offset =
						req.pmem.src[i].offset +
						QCE_MAX_OPER_DATA;
				req.pmem.src[i].offset =
						creq->pmem.src[i].offset;
				req.pmem.src[i].len = creq->pmem.src[i].len;
			} else {
				total = 0;
				for (j = i; j < req.entries; j++) {
					num_entries++;
					if ((total + creq->pmem.src[j].len)
							>= QCE_MAX_OPER_DATA) {
						creq->pmem.src[j].len =
						QCE_MAX_OPER_DATA - total;
						total = QCE_MAX_OPER_DATA;
						break;
					}
					total += creq->pmem.src[j].len;
				}

				creq->data_len = total;
				if (i > 0)
					for (k = 0; k < num_entries; k++) {
						creq->pmem.src[k].len =
						creq->pmem.src[i+k].len;
						creq->pmem.src[k].offset =
						creq->pmem.src[i+k].offset;
					}
				creq->entries =  num_entries;

				i = j;
				err =
				qcedev_pmem_ablk_cipher_max_xfer(qcedev_areq,
								handle);
				num_entries = 0;

					creq->pmem.src[i].offset =
						req.pmem.src[i].offset +
						creq->pmem.src[i].len;
					creq->pmem.src[i].len =
						req.pmem.src[i].len -
						creq->pmem.src[i].len;
					req.pmem.src[i].offset =
						creq->pmem.src[i].offset;
					req.pmem.src[i].len =
						creq->pmem.src[i].len;

				if (creq->pmem.src[i].len == 0)
					i++;
			}

		} /* end of while ((i < req.entries) && (err == 0)) */

	} else
		err = qcedev_pmem_ablk_cipher_max_xfer(qcedev_areq, handle);

	/* Restore the original req structure */
	for (i = 0; i < saved_req->entries; i++) {
		creq->pmem.src[i].len = saved_req->pmem.src[i].len;
		creq->pmem.src[i].offset = saved_req->pmem.src[i].offset;
	}
	creq->entries = saved_req->entries;
	creq->data_len = saved_req->data_len;
	kfree(saved_req);

	return err;

}
#else
static int qcedev_pmem_ablk_cipher(struct qcedev_async_req *qcedev_areq,
						struct qcedev_handle *handle)
{
	return -EPERM;
}
#endif/*CONFIG_ANDROID_PMEM*/

static int qcedev_vbuf_ablk_cipher_max_xfer(struct qcedev_async_req *areq,
				int *di, struct qcedev_handle *handle,
				uint8_t *k_align_src)
{
	int err = 0;
	int i = 0;
	int dst_i = *di;
	struct scatterlist sg_src;
	uint32_t byteoffset = 0;
	uint8_t *user_src = NULL;
	uint8_t *k_align_dst = k_align_src;
	struct	qcedev_cipher_op_req *creq = &areq->cipher_op_req;


	if (areq->cipher_op_req.mode == QCEDEV_AES_MODE_CTR)
		byteoffset = areq->cipher_op_req.byteoffset;

	user_src = (void __user *)areq->cipher_op_req.vbuf.src[0].vaddr;
	if (user_src && __copy_from_user((k_align_src + byteoffset),
				(void __user *)user_src,
				areq->cipher_op_req.vbuf.src[0].len))
		return -EFAULT;

	k_align_src += areq->cipher_op_req.vbuf.src[0].len;

	for (i = 1; i < areq->cipher_op_req.entries; i++) {
		user_src =
			(void __user *)areq->cipher_op_req.vbuf.src[i].vaddr;
		if (user_src && __copy_from_user(k_align_src,
					(void __user *)user_src,
					areq->cipher_op_req.vbuf.src[i].len)) {
			return -EFAULT;
		}
		k_align_src += areq->cipher_op_req.vbuf.src[i].len;
	}

	/* restore src beginning */
	k_align_src = k_align_dst;
	areq->cipher_op_req.data_len += byteoffset;

	areq->cipher_req.creq.src = (struct scatterlist *) &sg_src;
	areq->cipher_req.creq.dst = (struct scatterlist *) &sg_src;

	/* In place encryption/decryption */
	sg_set_buf(areq->cipher_req.creq.src,
					k_align_dst,
					areq->cipher_op_req.data_len);
	sg_mark_end(areq->cipher_req.creq.src);

	areq->cipher_req.creq.nbytes = areq->cipher_op_req.data_len;
	areq->cipher_req.creq.info = areq->cipher_op_req.iv;
	areq->cipher_op_req.entries = 1;

	err = submit_req(areq, handle);

	/* copy data to destination buffer*/
	creq->data_len -= byteoffset;

	while (creq->data_len > 0) {
		if (creq->vbuf.dst[dst_i].len <= creq->data_len) {
			if (err == 0 && __copy_to_user(
				(void __user *)creq->vbuf.dst[dst_i].vaddr,
					(k_align_dst + byteoffset),
					creq->vbuf.dst[dst_i].len))
					return -EFAULT;

			k_align_dst += creq->vbuf.dst[dst_i].len +
						byteoffset;
			creq->data_len -= creq->vbuf.dst[dst_i].len;
			dst_i++;
		} else {
				if (err == 0 && __copy_to_user(
				(void __user *)creq->vbuf.dst[dst_i].vaddr,
				(k_align_dst + byteoffset),
				creq->data_len))
					return -EFAULT;

			k_align_dst += creq->data_len;
			creq->vbuf.dst[dst_i].len -= creq->data_len;
			creq->vbuf.dst[dst_i].vaddr += creq->data_len;
			creq->data_len = 0;
		}
	}
	*di = dst_i;

	return err;
};

static int qcedev_vbuf_ablk_cipher(struct qcedev_async_req *areq,
						struct qcedev_handle *handle)
{
	int err = 0;
	int di = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	uint32_t byteoffset = 0;
	int num_entries = 0;
	uint32_t total = 0;
	uint32_t len;
	uint8_t *k_buf_src = NULL;
	uint8_t *k_align_src = NULL;
	uint32_t max_data_xfer;
	struct qcedev_cipher_op_req *saved_req;
	struct	qcedev_cipher_op_req *creq = &areq->cipher_op_req;

	/* Verify Source Address's */
	for (i = 0; i < areq->cipher_op_req.entries; i++)
		if (!access_ok(VERIFY_READ,
			(void __user *)areq->cipher_op_req.vbuf.src[i].vaddr,
					areq->cipher_op_req.vbuf.src[i].len))
			return -EFAULT;

	/* Verify Destination Address's */
	if (areq->cipher_op_req.in_place_op != 1)
		for (i = 0; i < areq->cipher_op_req.entries; i++)
			if (!access_ok(VERIFY_READ,
			(void __user *)areq->cipher_op_req.vbuf.dst[i].vaddr,
					areq->cipher_op_req.vbuf.dst[i].len))
				return -EFAULT;

	if (areq->cipher_op_req.mode == QCEDEV_AES_MODE_CTR)
		byteoffset = areq->cipher_op_req.byteoffset;
	k_buf_src = kmalloc(QCE_MAX_OPER_DATA + CACHE_LINE_SIZE * 2,
				GFP_KERNEL);
	if (k_buf_src == NULL) {
		pr_err("%s: Can't Allocate memory: k_buf_src 0x%x\n",
			__func__, (uint32_t)k_buf_src);
		return -ENOMEM;
	}
	k_align_src = (uint8_t *) ALIGN(((unsigned int)k_buf_src),
							CACHE_LINE_SIZE);
	max_data_xfer = QCE_MAX_OPER_DATA - byteoffset;

	saved_req = kmalloc(sizeof(struct qcedev_cipher_op_req), GFP_KERNEL);
	if (saved_req == NULL) {
		pr_err("%s: Can't Allocate memory:saved_req 0x%x\n",
			__func__, (uint32_t)saved_req);
		kfree(k_buf_src);
		return -ENOMEM;

	}
	memcpy(saved_req, creq, sizeof(struct qcedev_cipher_op_req));

	if (areq->cipher_op_req.data_len > max_data_xfer) {
		struct qcedev_cipher_op_req req;

		/* save the original req structure */
		memcpy(&req, creq, sizeof(struct qcedev_cipher_op_req));

		i = 0;
		/* Address 32 KB  at a time */
		while ((i < req.entries) && (err == 0)) {
			if (creq->vbuf.src[i].len > max_data_xfer) {
				creq->vbuf.src[0].len =	max_data_xfer;
				if (i > 0) {
					creq->vbuf.src[0].vaddr =
						creq->vbuf.src[i].vaddr;
				}

				creq->data_len = max_data_xfer;
				creq->entries = 1;

				err = qcedev_vbuf_ablk_cipher_max_xfer(areq,
						&di, handle, k_align_src);
				if (err < 0) {
					kfree(k_buf_src);
					kfree(saved_req);
					return err;
				}

				creq->vbuf.src[i].len =	req.vbuf.src[i].len -
							max_data_xfer;
				creq->vbuf.src[i].vaddr =
						req.vbuf.src[i].vaddr +
						max_data_xfer;
				req.vbuf.src[i].vaddr =
						creq->vbuf.src[i].vaddr;
				req.vbuf.src[i].len = creq->vbuf.src[i].len;

			} else {
				total = areq->cipher_op_req.byteoffset;
				for (j = i; j < req.entries; j++) {
					num_entries++;
					if ((total + creq->vbuf.src[j].len)
							>= max_data_xfer) {
						creq->vbuf.src[j].len =
						max_data_xfer - total;
						total = max_data_xfer;
						break;
					}
					total += creq->vbuf.src[j].len;
				}

				creq->data_len = total;
				if (i > 0)
					for (k = 0; k < num_entries; k++) {
						creq->vbuf.src[k].len =
						creq->vbuf.src[i+k].len;
						creq->vbuf.src[k].vaddr =
						creq->vbuf.src[i+k].vaddr;
					}
				creq->entries =  num_entries;

				i = j;
				err = qcedev_vbuf_ablk_cipher_max_xfer(areq,
						&di, handle, k_align_src);
				if (err < 0) {
					kfree(k_buf_src);
					kfree(saved_req);
					return err;
				}

				num_entries = 0;
				areq->cipher_op_req.byteoffset = 0;

				creq->vbuf.src[i].vaddr = req.vbuf.src[i].vaddr
					+ creq->vbuf.src[i].len;
				creq->vbuf.src[i].len =	req.vbuf.src[i].len -
							creq->vbuf.src[i].len;

				req.vbuf.src[i].vaddr =
						creq->vbuf.src[i].vaddr;
				req.vbuf.src[i].len = creq->vbuf.src[i].len;

				if (creq->vbuf.src[i].len == 0)
					i++;
			}

			areq->cipher_op_req.byteoffset = 0;
			max_data_xfer = QCE_MAX_OPER_DATA;
			byteoffset = 0;

		} /* end of while ((i < req.entries) && (err == 0)) */
	} else
		err = qcedev_vbuf_ablk_cipher_max_xfer(areq, &di, handle,
								k_align_src);

	/* Restore the original req structure */
	for (i = 0; i < saved_req->entries; i++) {
		creq->vbuf.src[i].len = saved_req->vbuf.src[i].len;
		creq->vbuf.src[i].vaddr = saved_req->vbuf.src[i].vaddr;
	}
	for (len = 0, i = 0; len < saved_req->data_len; i++) {
		creq->vbuf.dst[i].len = saved_req->vbuf.dst[i].len;
		creq->vbuf.dst[i].vaddr = saved_req->vbuf.dst[i].vaddr;
		len += saved_req->vbuf.dst[i].len;
	}
	creq->entries = saved_req->entries;
	creq->data_len = saved_req->data_len;
	creq->byteoffset = saved_req->byteoffset;

	kfree(saved_req);
	kfree(k_buf_src);
	return err;

}

static int qcedev_check_cipher_params(struct qcedev_cipher_op_req *req,
						struct qcedev_control *podev)
{
	if ((req->entries == 0) || (req->data_len == 0))
		goto error;
	if ((req->alg >= QCEDEV_ALG_LAST) ||
		(req->mode >= QCEDEV_AES_DES_MODE_LAST))
		goto error;
	if (req->alg == QCEDEV_ALG_AES) {
		if ((req->mode == QCEDEV_AES_MODE_XTS) &&
					(!podev->ce_support.aes_xts))
			goto error;
		/* if intending to use HW key make sure key fields are set
		 * correctly and HW key is indeed supported in target
		 */
		if (req->encklen == 0) {
			int i;
			for (i = 0; i < QCEDEV_MAX_KEY_SIZE; i++)
				if (req->enckey[i])
					goto error;
			if ((req->op != QCEDEV_OPER_ENC_NO_KEY) &&
				(req->op != QCEDEV_OPER_DEC_NO_KEY))
				if (!podev->platform_support.hw_key_support)
					goto error;
		} else {
			if (req->encklen == QCEDEV_AES_KEY_192) {
				if (!podev->ce_support.aes_key_192)
					goto error;
			} else {
				/* if not using HW key make sure key
				 * length is valid
				 */
				if (!((req->encklen == QCEDEV_AES_KEY_128) ||
					(req->encklen == QCEDEV_AES_KEY_256)))
					goto error;
			}
		}
	}
	/* if using a byteoffset, make sure it is CTR mode using vbuf */
	if (req->byteoffset) {
		if (req->mode != QCEDEV_AES_MODE_CTR)
			goto error;
		else { /* if using CTR mode make sure not using Pmem */
			if (req->use_pmem)
				goto error;
		}
	}
	/* if using PMEM with non-zero byteoffset, ensure it is in_place_op */
	if (req->use_pmem) {
		if (!req->in_place_op)
			goto error;
	}
	/* Ensure zer ivlen for ECB  mode  */
	if (req->ivlen != 0) {
		if ((req->mode == QCEDEV_AES_MODE_ECB) ||
				(req->mode == QCEDEV_DES_MODE_ECB))
			goto error;
	} else {
		if ((req->mode != QCEDEV_AES_MODE_ECB) &&
				(req->mode != QCEDEV_DES_MODE_ECB))
			goto error;
	}

	return 0;
error:
	return -EINVAL;

}

static int qcedev_check_sha_params(struct qcedev_sha_op_req *req,
						struct qcedev_control *podev)
{
	if ((req->alg == QCEDEV_ALG_AES_CMAC) &&
				(!podev->ce_support.cmac))
		goto sha_error;

	if ((req->entries == 0) || (req->data_len == 0))
		goto sha_error;

	if (req->alg >= QCEDEV_ALG_SHA_ALG_LAST)
		goto sha_error;

	return 0;
sha_error:
	return -EINVAL;
}

static long qcedev_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int err = 0;
	struct qcedev_handle *handle;
	struct qcedev_control *podev;
	struct qcedev_async_req qcedev_areq;
	struct qcedev_stat *pstat;

	handle =  file->private_data;
	podev =  handle->cntl;
	qcedev_areq.handle = handle;
	if (podev == NULL || podev->magic != QCEDEV_MAGIC) {
		pr_err("%s: invalid handle %p\n",
			__func__, podev);
		return -ENOENT;
	}

	/* Verify user arguments. */
	if (_IOC_TYPE(cmd) != QCEDEV_IOC_MAGIC)
		return -ENOTTY;

	init_completion(&qcedev_areq.complete);
	pstat = &_qcedev_stat[podev->pdev->id];

	switch (cmd) {
	case QCEDEV_IOCTL_LOCK_CE:
		if (podev->platform_support.ce_shared)
			err = qcedev_lock_ce(podev);
		else
			err = -ENOTTY;
		break;
	case QCEDEV_IOCTL_UNLOCK_CE:
		if (podev->platform_support.ce_shared)
			err = qcedev_unlock_ce(podev);
		else
			err = -ENOTTY;
		break;
	case QCEDEV_IOCTL_ENC_REQ:
	case QCEDEV_IOCTL_DEC_REQ:
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
				sizeof(struct qcedev_cipher_op_req)))
			return -EFAULT;

		if (__copy_from_user(&qcedev_areq.cipher_op_req,
				(void __user *)arg,
				sizeof(struct qcedev_cipher_op_req)))
			return -EFAULT;
		qcedev_areq.op_type = QCEDEV_CRYPTO_OPER_CIPHER;

		if (qcedev_check_cipher_params(&qcedev_areq.cipher_op_req,
				podev))
			return -EINVAL;

		if (qcedev_areq.cipher_op_req.use_pmem)
			err = qcedev_pmem_ablk_cipher(&qcedev_areq, handle);
		else
			err = qcedev_vbuf_ablk_cipher(&qcedev_areq, handle);
		if (err)
			return err;
		if (__copy_to_user((void __user *)arg,
					&qcedev_areq.cipher_op_req,
					sizeof(struct qcedev_cipher_op_req)))
				return -EFAULT;
		break;

	case QCEDEV_IOCTL_SHA_INIT_REQ:

		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
				sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;

		if (__copy_from_user(&qcedev_areq.sha_op_req,
					(void __user *)arg,
					sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;
		if (qcedev_check_sha_params(&qcedev_areq.sha_op_req, podev))
			return -EINVAL;
		qcedev_areq.op_type = QCEDEV_CRYPTO_OPER_SHA;
		err = qcedev_hash_init(&qcedev_areq, handle);
		if (err)
			return err;
		if (__copy_to_user((void __user *)arg, &qcedev_areq.sha_op_req,
					sizeof(struct qcedev_sha_op_req)))
				return -EFAULT;
		break;
	case QCEDEV_IOCTL_GET_CMAC_REQ:
		if (!podev->ce_support.cmac)
			return -ENOTTY;
	case QCEDEV_IOCTL_SHA_UPDATE_REQ:
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
				sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;

		if (__copy_from_user(&qcedev_areq.sha_op_req,
					(void __user *)arg,
					sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;
		if (qcedev_check_sha_params(&qcedev_areq.sha_op_req, podev))
			return -EINVAL;
		qcedev_areq.op_type = QCEDEV_CRYPTO_OPER_SHA;

		if (qcedev_areq.sha_op_req.alg == QCEDEV_ALG_AES_CMAC) {
			err = qcedev_hash_cmac(&qcedev_areq, handle);
			if (err)
				return err;
		} else {
			err = qcedev_hash_update(&qcedev_areq, handle);
			if (err)
				return err;
		}

		memcpy(&qcedev_areq.sha_op_req.digest[0],
				&handle->sha_ctxt.digest[0],
				handle->sha_ctxt.diglen);
		if (__copy_to_user((void __user *)arg, &qcedev_areq.sha_op_req,
					sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;
		break;

	case QCEDEV_IOCTL_SHA_FINAL_REQ:

		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
				sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;

		if (__copy_from_user(&qcedev_areq.sha_op_req,
					(void __user *)arg,
					sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;
		if (qcedev_check_sha_params(&qcedev_areq.sha_op_req, podev))
			return -EINVAL;
		qcedev_areq.op_type = QCEDEV_CRYPTO_OPER_SHA;
		err = qcedev_hash_final(&qcedev_areq, handle);
		if (err)
			return err;
		qcedev_areq.sha_op_req.diglen = handle->sha_ctxt.diglen;
		memcpy(&qcedev_areq.sha_op_req.digest[0],
				&handle->sha_ctxt.digest[0],
				handle->sha_ctxt.diglen);
		if (__copy_to_user((void __user *)arg, &qcedev_areq.sha_op_req,
					sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;
		break;

	case QCEDEV_IOCTL_GET_SHA_REQ:

		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
				sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;

		if (__copy_from_user(&qcedev_areq.sha_op_req,
					(void __user *)arg,
					sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;
		if (qcedev_check_sha_params(&qcedev_areq.sha_op_req, podev))
			return -EINVAL;
		qcedev_areq.op_type = QCEDEV_CRYPTO_OPER_SHA;
		qcedev_hash_init(&qcedev_areq, handle);
		err = qcedev_hash_update(&qcedev_areq, handle);
		if (err)
			return err;
		err = qcedev_hash_final(&qcedev_areq, handle);
		if (err)
			return err;
		qcedev_areq.sha_op_req.diglen =	handle->sha_ctxt.diglen;
		memcpy(&qcedev_areq.sha_op_req.digest[0],
				&handle->sha_ctxt.digest[0],
				handle->sha_ctxt.diglen);
		if (__copy_to_user((void __user *)arg, &qcedev_areq.sha_op_req,
					sizeof(struct qcedev_sha_op_req)))
			return -EFAULT;
		break;

	default:
		return -ENOTTY;
	}

	return err;
}

static int qcedev_probe(struct platform_device *pdev)
{
	void *handle = NULL;
	int rc = 0;
	struct qcedev_control *podev;
	struct msm_ce_hw_support *platform_support;

	if (pdev->id >= MAX_QCE_DEVICE) {
		pr_err("%s: device id %d  exceeds allowed %d\n",
			__func__, pdev->id, MAX_QCE_DEVICE);
		return -ENOENT;
	}
	podev = &qce_dev[pdev->id];

	platform_support = (struct msm_ce_hw_support *)pdev->dev.platform_data;
	podev->platform_support.ce_shared = platform_support->ce_shared;
	podev->platform_support.shared_ce_resource =
				platform_support->shared_ce_resource;
	podev->platform_support.hw_key_support =
				platform_support->hw_key_support;
	podev->platform_support.bus_scale_table =
				platform_support->bus_scale_table;
	podev->ce_lock_count = 0;
	podev->high_bw_req_count = 0;
	INIT_LIST_HEAD(&podev->ready_commands);
	podev->active_command = NULL;

	spin_lock_init(&podev->lock);

	tasklet_init(&podev->done_tasklet, req_done, (unsigned long)podev);

	/* open qce */
	handle = qce_open(pdev, &rc);
	if (handle == NULL) {
		platform_set_drvdata(pdev, NULL);
		return rc;
	}

	podev->qce = handle;
	podev->pdev = pdev;
	platform_set_drvdata(pdev, podev);
	qce_hw_support(podev->qce, &podev->ce_support);

	if (podev->platform_support.bus_scale_table != NULL) {
		podev->bus_scale_handle =
			msm_bus_scale_register_client(
				(struct msm_bus_scale_pdata *)
				podev->platform_support.bus_scale_table);
		if (!podev->bus_scale_handle) {
			printk(KERN_ERR "%s not able to get bus scale\n",
								__func__);
			rc =  -ENOMEM;
			goto err;
		}
	}
	rc = misc_register(&podev->miscdevice);

	if (rc >= 0)
		return 0;
	else
		if (podev->platform_support.bus_scale_table != NULL)
			msm_bus_scale_unregister_client(
						podev->bus_scale_handle);
err:

	if (handle)
		qce_close(handle);
	platform_set_drvdata(pdev, NULL);
	podev->qce = NULL;
	podev->pdev = NULL;
	return rc;
};

static int qcedev_remove(struct platform_device *pdev)
{
	struct qcedev_control *podev;

	podev = platform_get_drvdata(pdev);
	if (!podev)
		return 0;
	if (podev->qce)
		qce_close(podev->qce);

	if (podev->platform_support.bus_scale_table != NULL)
		msm_bus_scale_unregister_client(podev->bus_scale_handle);

	if (podev->miscdevice.minor != MISC_DYNAMIC_MINOR)
		misc_deregister(&podev->miscdevice);
	tasklet_kill(&podev->done_tasklet);
	return 0;
};

static struct platform_driver qcedev_plat_driver = {
	.probe = qcedev_probe,
	.remove = qcedev_remove,
	.driver = {
		.name = "qce",
		.owner = THIS_MODULE,
	},
};

static int _disp_stats(int id)
{
	struct qcedev_stat *pstat;
	int len = 0;

	pstat = &_qcedev_stat[id];
	len = snprintf(_debug_read_buf, DEBUG_MAX_RW_BUF - 1,
			"\nQualcomm QCE dev driver %d Statistics:\n",
				id + 1);

	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   Encryption operation success       : %d\n",
					pstat->qcedev_enc_success);
	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   Encryption operation fail   : %d\n",
					pstat->qcedev_enc_fail);
	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   Decryption operation success     : %d\n",
					pstat->qcedev_dec_success);

	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   Encryption operation fail          : %d\n",
					pstat->qcedev_dec_fail);

	return len;
}

static int _debug_stats_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t _debug_stats_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	int rc = -EINVAL;
	int qcedev = *((int *) file->private_data);
	int len;

	len = _disp_stats(qcedev);

	rc = simple_read_from_buffer((void __user *) buf, len,
			ppos, (void *) _debug_read_buf, len);

	return rc;
}

static ssize_t _debug_stats_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{

	int qcedev = *((int *) file->private_data);

	memset((char *)&_qcedev_stat[qcedev], 0, sizeof(struct qcedev_stat));
	return count;
};

static const struct file_operations _debug_stats_ops = {
	.open =         _debug_stats_open,
	.read =         _debug_stats_read,
	.write =        _debug_stats_write,
};

static int _qcedev_debug_init(void)
{
	int rc;
	char name[DEBUG_MAX_FNAME];
	int i;
	struct dentry *dent;

	_debug_dent = debugfs_create_dir("qcedev", NULL);
	if (IS_ERR(_debug_dent)) {
		pr_err("qcedev debugfs_create_dir fail, error %ld\n",
				PTR_ERR(_debug_dent));
		return PTR_ERR(_debug_dent);
	}

	for (i = 0; i < MAX_QCE_DEVICE; i++) {
		snprintf(name, DEBUG_MAX_FNAME-1, "stats-%d", i+1);
		_debug_qcedev[i] = i;
		dent = debugfs_create_file(name, 0644, _debug_dent,
				&_debug_qcedev[i], &_debug_stats_ops);
		if (dent == NULL) {
			pr_err("qcedev debugfs_create_file fail, error %ld\n",
					PTR_ERR(dent));
			rc = PTR_ERR(dent);
			goto err;
		}
	}
	return 0;
err:
	debugfs_remove_recursive(_debug_dent);
	return rc;
}

static int qcedev_init(void)
{
	int rc;

	rc = _qcedev_debug_init();
	if (rc)
		return rc;
	return platform_driver_register(&qcedev_plat_driver);
}

static void qcedev_exit(void)
{
	debugfs_remove_recursive(_debug_dent);
	platform_driver_unregister(&qcedev_plat_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Mona Hossain <mhossain@codeaurora.org>");
MODULE_DESCRIPTION("Qualcomm DEV Crypto driver");
MODULE_VERSION("1.25");

module_init(qcedev_init);
module_exit(qcedev_exit);
