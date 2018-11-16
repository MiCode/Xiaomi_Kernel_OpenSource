/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/completion.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/mempool.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <linux/qcrypto.h>
#include <linux/workqueue.h>
#include <linux/backing-dev.h>
#include <linux/atomic.h>
#include <linux/scatterlist.h>
#include <linux/device-mapper.h>
#include <linux/printk.h>

#include <asm/page.h>
#include <asm/unaligned.h>
#include <crypto/scatterwalk.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/algapi.h>
#include <crypto/ice.h>

#define DM_MSG_PREFIX "req-crypt"

#define MAX_SG_LIST	1024
#define REQ_DM_512_KB (512*1024)
#define MAX_ENCRYPTION_BUFFERS 1
#define MIN_IOS 256
#define MIN_POOL_PAGES 32
#define KEY_SIZE_XTS 64
#define AES_XTS_IV_LEN 16
#define MAX_MSM_ICE_KEY_LUT_SIZE 32
#define SECTOR_SIZE 512
#define MIN_CRYPTO_TRANSFER_SIZE (4 * 1024)

#define DM_REQ_CRYPT_ERROR -1
#define DM_REQ_CRYPT_ERROR_AFTER_PAGE_MALLOC -2

/*
 * ENCRYPTION_MODE_CRYPTO means dm-req-crypt would invoke crypto operations
 * for all of the requests. Crypto operations are performed by crypto engine
 * plugged with Linux Kernel Crypto APIs
 */
#define DM_REQ_CRYPT_ENCRYPTION_MODE_CRYPTO 0
/*
 * ENCRYPTION_MODE_TRANSPARENT means dm-req-crypt would not invoke crypto
 * operations for any of the requests. Data would be encrypted or decrypted
 * using Inline Crypto Engine(ICE) embedded in storage hardware
 */
#define DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT 1

#define DM_REQ_CRYPT_QUEUE_SIZE 256

struct req_crypt_result {
	struct completion completion;
	int err;
};

#define FDE_KEY_ID	0
#define PFE_KEY_ID	1

static struct dm_dev *dev;
static struct kmem_cache *_req_crypt_io_pool;
static struct kmem_cache *_req_dm_scatterlist_pool;
static sector_t start_sector_orig;
static struct workqueue_struct *req_crypt_queue;
static struct workqueue_struct *req_crypt_split_io_queue;
static mempool_t *req_io_pool;
static mempool_t *req_page_pool;
static mempool_t *req_scatterlist_pool;
static bool is_fde_enabled;
static struct crypto_ablkcipher *tfm;
static unsigned int encryption_mode;
static struct ice_crypto_setting *ice_settings;

unsigned int num_engines;
unsigned int num_engines_fde, fde_cursor;
unsigned int num_engines_pfe, pfe_cursor;
struct crypto_engine_entry *fde_eng, *pfe_eng;
DEFINE_MUTEX(engine_list_mutex);

struct req_dm_crypt_io {
	struct ice_crypto_setting ice_settings;
	struct work_struct work;
	struct request *cloned_request;
	int error;
	atomic_t pending;
	struct timespec start_time;
	bool should_encrypt;
	bool should_decrypt;
	u32 key_id;
};

struct req_dm_split_req_io {
	struct work_struct work;
	struct scatterlist *req_split_sg_read;
	struct req_crypt_result result;
	struct crypto_engine_entry *engine;
	u8 IV[AES_XTS_IV_LEN];
	int size;
	struct request *clone;
};

#ifdef CONFIG_FIPS_ENABLE
static struct qcrypto_func_set dm_qcrypto_func;
#else
static struct qcrypto_func_set dm_qcrypto_func = {
		qcrypto_cipher_set_device_hw,
		qcrypto_cipher_set_flag,
		qcrypto_get_num_engines,
		qcrypto_get_engine_list
};
#endif
static void req_crypt_cipher_complete
		(struct crypto_async_request *req, int err);
static void req_cryptd_split_req_queue_cb
		(struct work_struct *work);
static void req_cryptd_split_req_queue
		(struct req_dm_split_req_io *io);
static void req_crypt_split_io_complete
		(struct req_crypt_result *res, int err);

static  bool req_crypt_should_encrypt(struct req_dm_crypt_io *req)
{
	int ret = 0;
	bool should_encrypt = false;
	struct bio *bio = NULL;
	bool is_encrypted = false;
	bool is_inplace = false;

	if (!req || !req->cloned_request || !req->cloned_request->bio)
		return false;

	if (encryption_mode == DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT)
		return false;
	bio = req->cloned_request->bio;

	/* req->key_id = key_id; @todo support more than 1 pfe key */
	if ((ret == 0) && (is_encrypted || is_inplace)) {
		should_encrypt = true;
		req->key_id = PFE_KEY_ID;
	} else if (is_fde_enabled) {
		should_encrypt = true;
		req->key_id = FDE_KEY_ID;
	}

	return should_encrypt;
}

static  bool req_crypt_should_deccrypt(struct req_dm_crypt_io *req)
{
	int ret = 0;
	bool should_deccrypt = false;
	struct bio *bio = NULL;
	bool is_encrypted = false;
	bool is_inplace = false;

	if (!req || !req->cloned_request || !req->cloned_request->bio)
		return false;
	if (encryption_mode == DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT)
		return false;

	bio = req->cloned_request->bio;

	/* req->key_id = key_id; @todo support more than 1 pfe key */
	if ((ret == 0) && (is_encrypted && !is_inplace)) {
		should_deccrypt = true;
		req->key_id = PFE_KEY_ID;
	} else if (is_fde_enabled) {
		should_deccrypt = true;
		req->key_id = FDE_KEY_ID;
	}

	return should_deccrypt;
}

static void req_crypt_inc_pending(struct req_dm_crypt_io *io)
{
	atomic_inc(&io->pending);
}

static void req_crypt_dec_pending_encrypt(struct req_dm_crypt_io *io)
{
	int error = 0;
	struct request *clone = NULL;

	if (io) {
		error = io->error;
		if (io->cloned_request) {
			clone = io->cloned_request;
		} else {
			DMERR("%s io->cloned_request is NULL\n",
								__func__);
			/*
			 * If Clone is NULL we cannot do anything,
			 * this should never happen
			 */
			BUG();
		}
	} else {
		DMERR("%s io is NULL\n", __func__);
		/*
		 * If Clone is NULL we cannot do anything,
		 * this should never happen
		 */
		BUG();
	}

	atomic_dec(&io->pending);

	if (error < 0) {
		dm_kill_unmapped_request(clone, error);
		mempool_free(io, req_io_pool);
	} else
		dm_dispatch_request(clone);
}

static void req_crypt_dec_pending_decrypt(struct req_dm_crypt_io *io)
{
	int error = 0;
	struct request *clone = NULL;

	if (io) {
		error = io->error;
		if (io->cloned_request) {
			clone = io->cloned_request;
		} else {
			DMERR("%s io->cloned_request is NULL\n",
								__func__);
			/*
			 * If Clone is NULL we cannot do anything,
			 * this should never happen
			 */
			BUG();
		}
	} else {
		DMERR("%s io is NULL\n",
							__func__);
		/*
		 * If Clone is NULL we cannot do anything,
		 * this should never happen
		 */
		BUG();
	}

	/* Should never get here if io or Clone is NULL */
	dm_end_request(clone, error);
	atomic_dec(&io->pending);
	mempool_free(io, req_io_pool);
}

/*
 * The callback that will be called by the worker queue to perform Decryption
 * for reads and use the dm function to complete the bios and requests.
 */
static void req_cryptd_crypt_read_convert(struct req_dm_crypt_io *io)
{
	struct request *clone = NULL;
	int error = DM_REQ_CRYPT_ERROR;
	int total_sg_len = 0, total_bytes_in_req = 0, temp_size = 0, i = 0;
	struct scatterlist *sg = NULL;
	struct scatterlist *req_sg_read = NULL;

	unsigned int engine_list_total = 0;
	struct crypto_engine_entry *curr_engine_list = NULL;
	bool split_transfers = 0;
	sector_t tempiv;
	struct req_dm_split_req_io *split_io = NULL;

	if (io) {
		error = io->error;
		if (io->cloned_request) {
			clone = io->cloned_request;
		} else {
			DMERR("%s io->cloned_request is NULL\n",
								__func__);
			error = DM_REQ_CRYPT_ERROR;
			goto submit_request;
		}
	} else {
		DMERR("%s io is NULL\n",
							__func__);
		error = DM_REQ_CRYPT_ERROR;
		goto submit_request;
	}

	req_crypt_inc_pending(io);

	mutex_lock(&engine_list_mutex);

	engine_list_total = (io->key_id == FDE_KEY_ID ? num_engines_fde :
						   (io->key_id == PFE_KEY_ID ?
							num_engines_pfe : 0));

	curr_engine_list = (io->key_id == FDE_KEY_ID ? fde_eng :
						   (io->key_id == PFE_KEY_ID ?
							pfe_eng : NULL));

	mutex_unlock(&engine_list_mutex);

	req_sg_read = (struct scatterlist *)mempool_alloc(req_scatterlist_pool,
								GFP_KERNEL);
	if (!req_sg_read) {
		DMERR("%s req_sg_read allocation failed\n",
						__func__);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}
	memset(req_sg_read, 0, sizeof(struct scatterlist) * MAX_SG_LIST);

	total_sg_len = blk_rq_map_sg_no_cluster(clone->q, clone, req_sg_read);
	if ((total_sg_len <= 0) || (total_sg_len > MAX_SG_LIST)) {
		DMERR("%s Request Error%d", __func__, total_sg_len);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	total_bytes_in_req = clone->__data_len;
	if (total_bytes_in_req > REQ_DM_512_KB) {
		DMERR("%s total_bytes_in_req > 512 MB %d",
				__func__, total_bytes_in_req);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}


	if ((clone->__data_len >= (MIN_CRYPTO_TRANSFER_SIZE *
		engine_list_total))
		&& (engine_list_total > 1))
		split_transfers = 1;

	if (split_transfers) {
		split_io = kzalloc(sizeof(struct req_dm_split_req_io)
				* engine_list_total, GFP_KERNEL);
		if (!split_io) {
			DMERR("%s split_io allocation failed\n", __func__);
			error = DM_REQ_CRYPT_ERROR;
			goto ablkcipher_req_alloc_failure;
		}

		split_io[0].req_split_sg_read = sg = req_sg_read;
		split_io[engine_list_total - 1].size = total_bytes_in_req;
		for (i = 0; i < (engine_list_total); i++) {
			while ((sg) && i < (engine_list_total - 1)) {
				split_io[i].size += sg->length;
				split_io[engine_list_total - 1].size -=
						sg->length;
				if (split_io[i].size >=
						(total_bytes_in_req /
							engine_list_total)) {
					split_io[i + 1].req_split_sg_read =
							sg_next(sg);
					sg_mark_end(sg);
					break;
				}
				sg = sg_next(sg);
			}
			split_io[i].engine = &curr_engine_list[i];
			init_completion(&split_io[i].result.completion);
			memset(&split_io[i].IV, 0, AES_XTS_IV_LEN);
			tempiv = clone->__sector + (temp_size / SECTOR_SIZE);
			memcpy(&split_io[i].IV, &tempiv, sizeof(sector_t));
			temp_size +=  split_io[i].size;
			split_io[i].clone = clone;
			req_cryptd_split_req_queue(&split_io[i]);
		}
	} else {
		split_io = kzalloc(sizeof(struct req_dm_split_req_io),
				GFP_KERNEL);
		if (!split_io) {
			DMERR("%s split_io allocation failed\n", __func__);
			error = DM_REQ_CRYPT_ERROR;
			goto ablkcipher_req_alloc_failure;
		}
		split_io->engine = &curr_engine_list[0];
		init_completion(&split_io->result.completion);
		memcpy(split_io->IV, &clone->__sector, sizeof(sector_t));
		split_io->req_split_sg_read = req_sg_read;
		split_io->size = total_bytes_in_req;
		split_io->clone = clone;
		req_cryptd_split_req_queue(split_io);
	}

	if (!split_transfers) {
		wait_for_completion_interruptible(&split_io->result.completion);
		if (split_io->result.err) {
			DMERR("%s error = %d for request\n",
				 __func__, split_io->result.err);
			error = DM_REQ_CRYPT_ERROR;
			goto ablkcipher_req_alloc_failure;
		}
	} else {
		for (i = 0; i < (engine_list_total); i++) {
			wait_for_completion_interruptible(
					&split_io[i].result.completion);
			if (split_io[i].result.err) {
				DMERR("%s error = %d for %dst request\n",
					 __func__, split_io[i].result.err, i);
				error = DM_REQ_CRYPT_ERROR;
				goto ablkcipher_req_alloc_failure;
			}
		}
	}
	error = 0;
ablkcipher_req_alloc_failure:

	mempool_free(req_sg_read, req_scatterlist_pool);
	kfree(split_io);
submit_request:
	if (io)
		io->error = error;
	req_crypt_dec_pending_decrypt(io);
}

/*
 * This callback is called by the worker queue to perform non-decrypt reads
 * and use the dm function to complete the bios and requests.
 */
static void req_cryptd_crypt_read_plain(struct req_dm_crypt_io *io)
{
	struct request *clone = NULL;
	int error = 0;

	if (!io || !io->cloned_request) {
		DMERR("%s io is invalid\n", __func__);
		BUG(); /* should not happen */
	}

	clone = io->cloned_request;

	dm_end_request(clone, error);
	mempool_free(io, req_io_pool);
}

/*
 * The callback that will be called by the worker queue to perform Encryption
 * for writes and submit the request using the elevelator.
 */
static void req_cryptd_crypt_write_convert(struct req_dm_crypt_io *io)
{
	struct request *clone = NULL;
	struct bio *bio_src = NULL;
	unsigned int total_sg_len_req_in = 0, total_sg_len_req_out = 0,
		total_bytes_in_req = 0, error = DM_MAPIO_REMAPPED, rc = 0;
	struct ablkcipher_request *req = NULL;
	struct req_crypt_result result;
	struct scatterlist *req_sg_in = NULL;
	struct scatterlist *req_sg_out = NULL;
	int copy_bio_sector_to_req = 0;
	gfp_t gfp_mask = GFP_NOIO | __GFP_HIGHMEM;
	struct page *page = NULL;
	u8 IV[AES_XTS_IV_LEN];
	int size = 0, err = 0;
	struct crypto_engine_entry engine;
	unsigned int engine_list_total = 0;
	struct crypto_engine_entry *curr_engine_list = NULL;
	unsigned int *engine_cursor = NULL;
	unsigned int i;
	struct bio_vec *_bvec;
	struct bio *_bio;


	if (io) {
		if (io->cloned_request) {
			clone = io->cloned_request;
		} else {
			DMERR("%s io->cloned_request is NULL\n",
								__func__);
			error = DM_REQ_CRYPT_ERROR;
			goto submit_request;
		}
	} else {
		DMERR("%s io is NULL\n",
							__func__);
		error = DM_REQ_CRYPT_ERROR;
		goto submit_request;
	}

	req_crypt_inc_pending(io);

	req = ablkcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		DMERR("%s ablkcipher request allocation failed\n",
					__func__);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	ablkcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				req_crypt_cipher_complete, &result);

	mutex_lock(&engine_list_mutex);
	engine_list_total = (io->key_id == FDE_KEY_ID ? num_engines_fde :
						   (io->key_id == PFE_KEY_ID ?
							num_engines_pfe : 0));

	curr_engine_list = (io->key_id == FDE_KEY_ID ? fde_eng :
						(io->key_id == PFE_KEY_ID ?
						pfe_eng : NULL));

	engine_cursor = (io->key_id == FDE_KEY_ID ? &fde_cursor :
					(io->key_id == PFE_KEY_ID ? &pfe_cursor
					: NULL));
	if ((engine_list_total < 1) || (NULL == curr_engine_list)
	   || (NULL == engine_cursor)) {
		DMERR("%s Unknown Key ID!\n",
						   __func__);
		error = DM_REQ_CRYPT_ERROR;
		mutex_unlock(&engine_list_mutex);
		goto ablkcipher_req_alloc_failure;
	}

	engine = curr_engine_list[*engine_cursor];
	(*engine_cursor)++;
	(*engine_cursor) %= engine_list_total;

	err = (dm_qcrypto_func.cipher_set)(req, engine.ce_device,
				   engine.hw_instance);
	if (err) {
		DMERR("%s qcrypto_cipher_set_device_hw failed with err %d\n",
				__func__, err);
		mutex_unlock(&engine_list_mutex);
		goto ablkcipher_req_alloc_failure;
	}
	mutex_unlock(&engine_list_mutex);

	init_completion(&result.completion);

	(dm_qcrypto_func.cipher_flag)(req,
		QCRYPTO_CTX_USE_PIPE_KEY | QCRYPTO_CTX_XTS_DU_SIZE_512B);
	crypto_ablkcipher_clear_flags(tfm, ~0);
	crypto_ablkcipher_setkey(tfm, NULL, KEY_SIZE_XTS);

	req_sg_in = (struct scatterlist *)mempool_alloc(req_scatterlist_pool,
								GFP_KERNEL);
	if (!req_sg_in) {
		DMERR("%s req_sg_in allocation failed\n",
					__func__);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}
	memset(req_sg_in, 0, sizeof(struct scatterlist) * MAX_SG_LIST);

	req_sg_out = (struct scatterlist *)mempool_alloc(req_scatterlist_pool,
								GFP_KERNEL);
	if (!req_sg_out) {
		DMERR("%s req_sg_out allocation failed\n",
					__func__);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}
	memset(req_sg_out, 0, sizeof(struct scatterlist) * MAX_SG_LIST);

	total_sg_len_req_in = blk_rq_map_sg(clone->q, clone, req_sg_in);
	if ((total_sg_len_req_in <= 0) ||
			(total_sg_len_req_in > MAX_SG_LIST)) {
		DMERR("%s Request Error%d", __func__, total_sg_len_req_in);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	total_bytes_in_req = clone->__data_len;
	if (total_bytes_in_req > REQ_DM_512_KB) {
		DMERR("%s total_bytes_in_req > 512 MB %d",
				__func__, total_bytes_in_req);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	__rq_for_each_bio(_bio, clone) {
		bio_for_each_segment_all(_bvec, _bio, i) {
			if (_bvec->bv_len > size) {
				page = NULL;
				while (page == NULL) {
					page = mempool_alloc(req_page_pool,
							gfp_mask);
					if (!page) {
						DMERR("%s Page alloc failed",
							__func__);
						congestion_wait(BLK_RW_ASYNC,
								HZ/100);
					}
				}

				_bvec->bv_page = page;
				_bvec->bv_offset = 0;
				size = PAGE_SIZE - _bvec->bv_len;
				if (size < 0)
					BUG();
			} else {
				_bvec->bv_page = page;
				_bvec->bv_offset = PAGE_SIZE - size;
				size = size - _bvec->bv_len;
			}
		}
	}

	total_sg_len_req_out = blk_rq_map_sg(clone->q, clone, req_sg_out);
	if ((total_sg_len_req_out <= 0) ||
			(total_sg_len_req_out > MAX_SG_LIST)) {
		DMERR("%s Request Error %d", __func__, total_sg_len_req_out);
		error = DM_REQ_CRYPT_ERROR_AFTER_PAGE_MALLOC;
		goto ablkcipher_req_alloc_failure;
	}

	memset(IV, 0, AES_XTS_IV_LEN);
	memcpy(IV, &clone->__sector, sizeof(sector_t));

	ablkcipher_request_set_crypt(req, req_sg_in, req_sg_out,
			total_bytes_in_req, (void *) IV);

	rc = crypto_ablkcipher_encrypt(req);

	switch (rc) {
	case 0:
		break;

	case -EBUSY:
		/*
		 * Lets make this synchronous request by waiting on
		 * in progress as well
		 */
	case -EINPROGRESS:
		wait_for_completion_interruptible(&result.completion);
		if (result.err) {
			DMERR("%s error = %d encrypting the request\n",
				 __func__, result.err);
			error = DM_REQ_CRYPT_ERROR_AFTER_PAGE_MALLOC;
			goto ablkcipher_req_alloc_failure;
		}
		break;

	default:
		error = DM_REQ_CRYPT_ERROR_AFTER_PAGE_MALLOC;
		goto ablkcipher_req_alloc_failure;
	}

	__rq_for_each_bio(bio_src, clone) {
		if (copy_bio_sector_to_req == 0) {
			copy_bio_sector_to_req++;
		}
		blk_queue_bounce(clone->q, &bio_src);
	}

	/*
	 * Recalculate the phy_segments as we allocate new pages
	 * This is used by storage driver to fill the sg list.
	 */
	blk_recalc_rq_segments(clone);

ablkcipher_req_alloc_failure:
	if (req)
		ablkcipher_request_free(req);

	if (error == DM_REQ_CRYPT_ERROR_AFTER_PAGE_MALLOC) {
		__rq_for_each_bio(_bio, clone) {
			bio_for_each_segment_all(_bvec, _bio, i) {
				if (_bvec->bv_offset == 0) {
					mempool_free(_bvec->bv_page,
						req_page_pool);
					_bvec->bv_page = NULL;
				} else {
					_bvec->bv_page = NULL;
				}
			}
		}
	}

	mempool_free(req_sg_in, req_scatterlist_pool);
	mempool_free(req_sg_out, req_scatterlist_pool);
submit_request:
	if (io)
		io->error = error;
	req_crypt_dec_pending_encrypt(io);
}

/*
 * This callback is called by the worker queue to perform non-encrypted writes
 * and submit the request using the elevelator.
 */
static void req_cryptd_crypt_write_plain(struct req_dm_crypt_io *io)
{
	struct request *clone = NULL;

	if (!io || !io->cloned_request) {
		DMERR("%s io is invalid\n", __func__);
		BUG(); /* should not happen */
	}

	clone = io->cloned_request;
	io->error = 0;
	dm_dispatch_request(clone);
}

/* Queue callback function that will get triggered */
static void req_cryptd_crypt(struct work_struct *work)
{
	struct req_dm_crypt_io *io =
			container_of(work, struct req_dm_crypt_io, work);

	if (rq_data_dir(io->cloned_request) == WRITE) {
		if (io->should_encrypt)
			req_cryptd_crypt_write_convert(io);
		else
			req_cryptd_crypt_write_plain(io);
	} else if (rq_data_dir(io->cloned_request) == READ) {
		if (io->should_decrypt)
			req_cryptd_crypt_read_convert(io);
		else
			req_cryptd_crypt_read_plain(io);
	} else {
		DMERR("%s received non-write request for Clone 0x%p\n",
				__func__, io->cloned_request);
	}
}

static void req_cryptd_split_req_queue_cb(struct work_struct *work)
{
	struct req_dm_split_req_io *io =
			container_of(work, struct req_dm_split_req_io, work);
	struct ablkcipher_request *req = NULL;
	struct req_crypt_result result;
	int err = 0;
	struct crypto_engine_entry *engine = NULL;

	if ((!io) || (!io->req_split_sg_read) || (!io->engine)) {
		DMERR("%s Input invalid\n",
			 __func__);
		err = DM_REQ_CRYPT_ERROR;
		/* If io is not populated this should not be called */
		BUG();
	}
	req = ablkcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		DMERR("%s ablkcipher request allocation failed\n", __func__);
		err = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	ablkcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
					req_crypt_cipher_complete, &result);

	engine = io->engine;

	err = (dm_qcrypto_func.cipher_set)(req, engine->ce_device,
			engine->hw_instance);
	if (err) {
		DMERR("%s qcrypto_cipher_set_device_hw failed with err %d\n",
				__func__, err);
		goto ablkcipher_req_alloc_failure;
	}
	init_completion(&result.completion);
	(dm_qcrypto_func.cipher_flag)(req,
		QCRYPTO_CTX_USE_PIPE_KEY | QCRYPTO_CTX_XTS_DU_SIZE_512B);

	crypto_ablkcipher_clear_flags(tfm, ~0);
	crypto_ablkcipher_setkey(tfm, NULL, KEY_SIZE_XTS);

	ablkcipher_request_set_crypt(req, io->req_split_sg_read,
			io->req_split_sg_read, io->size, (void *) io->IV);

	err = crypto_ablkcipher_decrypt(req);
	switch (err) {
	case 0:
		break;

	case -EBUSY:
		/*
		 * Lets make this synchronous request by waiting on
		 * in progress as well
		 */
	case -EINPROGRESS:
		wait_for_completion_io(&result.completion);
		if (result.err) {
			DMERR("%s error = %d encrypting the request\n",
				 __func__, result.err);
			err = DM_REQ_CRYPT_ERROR;
			goto ablkcipher_req_alloc_failure;
		}
		break;

	default:
		err = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}
	err = 0;
ablkcipher_req_alloc_failure:
	if (req)
		ablkcipher_request_free(req);

	req_crypt_split_io_complete(&io->result, err);
}

static void req_cryptd_split_req_queue(struct req_dm_split_req_io *io)
{
	INIT_WORK(&io->work, req_cryptd_split_req_queue_cb);
	queue_work(req_crypt_split_io_queue, &io->work);
}

static void req_cryptd_queue_crypt(struct req_dm_crypt_io *io)
{
	INIT_WORK(&io->work, req_cryptd_crypt);
	queue_work(req_crypt_queue, &io->work);
}

/*
 * Cipher complete callback, this is triggered by the Linux crypto api once
 * the operation is done. This signals the waiting thread that the crypto
 * operation is complete.
 */
static void req_crypt_cipher_complete(struct crypto_async_request *req, int err)
{
	struct req_crypt_result *res = req->data;

	if (err == -EINPROGRESS)
		return;

	res->err = err;
	complete(&res->completion);
}

static void req_crypt_split_io_complete(struct req_crypt_result *res, int err)
{
	if (err == -EINPROGRESS)
		return;

	res->err = err;
	complete(&res->completion);
}
/*
 * If bio->bi_dev is a partition, remap the location
 */
static inline void req_crypt_blk_partition_remap(struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;

	if (bio_sectors(bio) && bdev != bdev->bd_contains) {
		struct hd_struct *p = bdev->bd_part;
		/*
		* Check for integer overflow, should never happen.
		*/
		if (p->start_sect > (UINT_MAX - bio->bi_iter.bi_sector))
			BUG();

		bio->bi_iter.bi_sector += p->start_sect;
		bio->bi_bdev = bdev->bd_contains;
	}
}

/*
 * The endio function is called from ksoftirqd context (atomic).
 * For write operations the new pages created form the mempool
 * is freed and returned.  * For read operations, decryption is
 * required, since this is called in a atomic  * context, the
 * request is sent to a worker queue to complete decryptiona and
 * free the request once done.
 */
static int req_crypt_endio(struct dm_target *ti, struct request *clone,
			    int error, union map_info *map_context)
{
	int err = 0;
	struct bio_vec *_bvec;
	struct bio *_bio;
	unsigned int i;
	struct req_dm_crypt_io *req_io = map_context->ptr;

	/* If it is for ICE, free up req_io and return */
	if (encryption_mode == DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT) {
		mempool_free(req_io, req_io_pool);
		err = error;
		goto submit_request;
	}

	if (rq_data_dir(clone) == WRITE) {
		__rq_for_each_bio(_bio, clone) {
			bio_for_each_segment_all(_bvec, _bio, i) {
				if (req_io->should_encrypt &&
						_bvec->bv_offset == 0) {
					mempool_free(_bvec->bv_page,
						req_page_pool);
					_bvec->bv_page = NULL;
				} else {
					_bvec->bv_page = NULL;
				}
			}
		}
		mempool_free(req_io, req_io_pool);
		goto submit_request;
	} else if (rq_data_dir(clone) == READ) {
		req_io->error = error;
		req_cryptd_queue_crypt(req_io);
		err = DM_ENDIO_INCOMPLETE;
		goto submit_request;
	}

submit_request:
	return err;
}

/*
 * This function is called with interrupts disabled
 * The function remaps the clone for the underlying device.
 * If it is a write request, it calls into the worker queue to
 * encrypt the data
 * and submit the request directly using the elevator
 * For a read request no pre-processing is required the request
 * is returned to dm once mapping is done
 */
static int req_crypt_map(struct dm_target *ti, struct request *clone,
			 union map_info *map_context)
{
	struct req_dm_crypt_io *req_io = NULL;
	int error = DM_REQ_CRYPT_ERROR, copy_bio_sector_to_req = 0;
	struct bio *bio_src = NULL;
	gfp_t gfp_flag = GFP_KERNEL;

	if (in_interrupt() || irqs_disabled())
		gfp_flag = GFP_NOWAIT;

	req_io = mempool_alloc(req_io_pool, gfp_flag);
	if (!req_io) {
		DMERR("%s req_io allocation failed\n", __func__);
		BUG();
		error = DM_REQ_CRYPT_ERROR;
		goto submit_request;
	}

	/* Save the clone in the req_io, the callback to the worker
	 * queue will get the req_io
	 */
	req_io->cloned_request = clone;
	map_context->ptr = req_io;
	atomic_set(&req_io->pending, 0);

	if (rq_data_dir(clone) == WRITE)
		req_io->should_encrypt = req_crypt_should_encrypt(req_io);
	if (rq_data_dir(clone) == READ)
		req_io->should_decrypt = req_crypt_should_deccrypt(req_io);

	/* Get the queue of the underlying original device */
	clone->q = bdev_get_queue(dev->bdev);
	clone->rq_disk = dev->bdev->bd_disk;

	__rq_for_each_bio(bio_src, clone) {
		bio_src->bi_bdev = dev->bdev;
		/* Currently the way req-dm works is that once the underlying
		 * device driver completes the request by calling into the
		 * block layer. The block layer completes the bios (clones) and
		 * then the cloned request. This is undesirable for req-dm-crypt
		 * hence added a flag BIO_DONTFREE, this flag will ensure that
		 * blk layer does not complete the cloned bios before completing
		 * the request. When the crypt endio is called, post-processing
		 * is done and then the dm layer will complete the bios (clones)
		 * and free them.
		 */
		if (encryption_mode == DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT)
			bio_src->bi_flags |= 1 << BIO_INLINECRYPT;
		else
			bio_src->bi_flags |= 1 << BIO_DONTFREE;

		/*
		 * If this device has partitions, remap block n
		 * of partition p to block n+start(p) of the disk.
		 */
		req_crypt_blk_partition_remap(bio_src);
		if (copy_bio_sector_to_req == 0) {
			clone->__sector = bio_src->bi_iter.bi_sector;
			copy_bio_sector_to_req++;
		}
		blk_queue_bounce(clone->q, &bio_src);
	}

	if (encryption_mode == DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT) {
		/* Set all crypto parameters for inline crypto engine */
		memcpy(&req_io->ice_settings, ice_settings,
					sizeof(struct ice_crypto_setting));
	} else {
		/* ICE checks for key_index which could be >= 0. If a chip has
		 * both ICE and GPCE and wanted to use GPCE, there could be
		 * issue. Storage driver send all requests to ICE driver. If
		 * it sees key_index as 0, it would assume it is for ICE while
		 * it is not. Hence set invalid key index by default.
		 */
		req_io->ice_settings.key_index = -1;

	}

	if (rq_data_dir(clone) == READ ||
		encryption_mode == DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT) {
		error = DM_MAPIO_REMAPPED;
		goto submit_request;
	} else if (rq_data_dir(clone) == WRITE) {
		req_cryptd_queue_crypt(req_io);
		error = DM_MAPIO_SUBMITTED;
		goto submit_request;
	}

submit_request:
	return error;

}

static void deconfigure_qcrypto(void)
{
	if (req_page_pool) {
		mempool_destroy(req_page_pool);
		req_page_pool = NULL;
	}

	if (req_scatterlist_pool) {
		mempool_destroy(req_scatterlist_pool);
		req_scatterlist_pool = NULL;
	}

	if (req_crypt_split_io_queue) {
		destroy_workqueue(req_crypt_split_io_queue);
		req_crypt_split_io_queue = NULL;
	}
	if (req_crypt_queue) {
		destroy_workqueue(req_crypt_queue);
		req_crypt_queue = NULL;
	}

	if (_req_dm_scatterlist_pool)
		kmem_cache_destroy(_req_dm_scatterlist_pool);

	mutex_lock(&engine_list_mutex);
	kfree(pfe_eng);
	pfe_eng = NULL;
	kfree(fde_eng);
	fde_eng = NULL;
	mutex_unlock(&engine_list_mutex);

	if (tfm) {
		crypto_free_ablkcipher(tfm);
		tfm = NULL;
	}
}

static void req_crypt_dtr(struct dm_target *ti)
{
	DMDEBUG("dm-req-crypt Destructor.\n");

	if (req_io_pool) {
		mempool_destroy(req_io_pool);
		req_io_pool = NULL;
	}

	if (encryption_mode == DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT) {
		kfree(ice_settings);
		ice_settings = NULL;
	} else {
		deconfigure_qcrypto();
	}

	if (_req_crypt_io_pool)
		kmem_cache_destroy(_req_crypt_io_pool);

	if (dev) {
		dm_put_device(ti, dev);
		dev = NULL;
	}
}

static int configure_qcrypto(void)
{
	struct crypto_engine_entry *eng_list = NULL;
	struct block_device *bdev = NULL;
	int err = DM_REQ_CRYPT_ERROR, i;
	struct request_queue *q = NULL;

	bdev = dev->bdev;
	q = bdev_get_queue(bdev);
	blk_queue_max_hw_sectors(q, DM_REQ_CRYPT_QUEUE_SIZE);

	/* Allocate the crypto alloc blk cipher and keep the handle */
	tfm = crypto_alloc_ablkcipher("qcom-xts(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		DMERR("%s ablkcipher tfm allocation failed : error\n",
						 __func__);
		tfm = NULL;
		goto exit_err;
	}

	num_engines_fde = num_engines_pfe = 0;

	mutex_lock(&engine_list_mutex);
	num_engines = (dm_qcrypto_func.get_num_engines)();
	if (!num_engines) {
		DMERR(KERN_INFO "%s qcrypto_get_num_engines failed\n",
					__func__);
		err = DM_REQ_CRYPT_ERROR;
		mutex_unlock(&engine_list_mutex);
		goto exit_err;
	}

	eng_list = kcalloc(num_engines, sizeof(*eng_list), 0);
	if (NULL == eng_list) {
		DMERR("%s engine list allocation failed\n", __func__);
		err = DM_REQ_CRYPT_ERROR;
		mutex_unlock(&engine_list_mutex);
		goto exit_err;
	}

	(dm_qcrypto_func.get_engine_list)(num_engines, eng_list);

	for (i = 0; i < num_engines; i++) {
		if (eng_list[i].ce_device == FDE_KEY_ID)
			num_engines_fde++;
		if (eng_list[i].ce_device == PFE_KEY_ID)
			num_engines_pfe++;
	}

	fde_eng = kcalloc(num_engines_fde, sizeof(*fde_eng), GFP_KERNEL);
	if (NULL == fde_eng) {
		DMERR("%s fde engine list allocation failed\n", __func__);
		mutex_unlock(&engine_list_mutex);
		goto exit_err;
	}

	pfe_eng = kcalloc(num_engines_pfe, sizeof(*pfe_eng), GFP_KERNEL);
	if (NULL == pfe_eng) {
		DMERR("%s pfe engine list allocation failed\n", __func__);
		mutex_unlock(&engine_list_mutex);
		goto exit_err;
	}

	fde_cursor = 0;
	pfe_cursor = 0;

	for (i = 0; i < num_engines; i++) {
		if (eng_list[i].ce_device == FDE_KEY_ID)
			fde_eng[fde_cursor++] = eng_list[i];
		if (eng_list[i].ce_device == PFE_KEY_ID)
			pfe_eng[pfe_cursor++] = eng_list[i];
	}

	fde_cursor = 0;
	pfe_cursor = 0;
	mutex_unlock(&engine_list_mutex);

	_req_dm_scatterlist_pool = kmem_cache_create("req_dm_scatterlist",
				sizeof(struct scatterlist) * MAX_SG_LIST,
				 __alignof__(struct scatterlist), 0, NULL);
	if (!_req_dm_scatterlist_pool)
		goto exit_err;

	req_crypt_queue = alloc_workqueue("req_cryptd",
					WQ_UNBOUND |
					WQ_CPU_INTENSIVE |
					WQ_MEM_RECLAIM,
					0);
	if (!req_crypt_queue) {
		DMERR("%s req_crypt_queue not allocated\n", __func__);
		goto exit_err;
	}

	req_crypt_split_io_queue = alloc_workqueue("req_crypt_split",
					WQ_UNBOUND |
					WQ_CPU_INTENSIVE |
					WQ_MEM_RECLAIM,
					0);
	if (!req_crypt_split_io_queue) {
		DMERR("%s req_crypt_split_io_queue not allocated\n", __func__);
		goto exit_err;
	}
	req_scatterlist_pool = mempool_create_slab_pool(MIN_IOS,
					_req_dm_scatterlist_pool);
	if (!req_scatterlist_pool) {
		DMERR("%s req_scatterlist_pool is not allocated\n", __func__);
		err = -ENOMEM;
		goto exit_err;
	}

	req_page_pool = mempool_create_page_pool(MIN_POOL_PAGES, 0);
	if (!req_page_pool) {
		DMERR("%s req_page_pool not allocated\n", __func__);
		goto exit_err;
	}

	err = 0;

exit_err:
	kfree(eng_list);
	return err;
}

/*
 * Construct an encryption mapping:
 * <cipher> <key> <iv_offset> <dev_path> <start>
 */
static int req_crypt_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	int err = DM_REQ_CRYPT_ERROR;
	unsigned long long tmpll;
	char dummy;
	int ret;

	DMDEBUG("dm-req-crypt Constructor.\n");

	if (argc < 5) {
		DMERR(" %s Not enough args\n", __func__);
		err = DM_REQ_CRYPT_ERROR;
		goto ctr_exit;
	}

	if (argv[3]) {
		if (dm_get_device(ti, argv[3],
				dm_table_get_mode(ti->table), &dev)) {
			DMERR(" %s Device Lookup failed\n", __func__);
			err =  DM_REQ_CRYPT_ERROR;
			goto ctr_exit;
		}
	} else {
		DMERR(" %s Arg[3] invalid\n", __func__);
		err =  DM_REQ_CRYPT_ERROR;
		goto ctr_exit;
	}

	if (argv[4]) {
		if (sscanf(argv[4], "%llu%c", &tmpll, &dummy) != 1) {
			DMERR("%s Invalid device sector\n", __func__);
			err =  DM_REQ_CRYPT_ERROR;
			goto ctr_exit;
		}
	} else {
		DMERR(" %s Arg[4] invalid\n", __func__);
		err =  DM_REQ_CRYPT_ERROR;
		goto ctr_exit;
	}
	start_sector_orig = tmpll;

	/* Allow backward compatible */
	if (argc >= 6) {
		if (argv[5]) {
			if (!strcmp(argv[5], "fde_enabled"))
				is_fde_enabled = true;
			else
				is_fde_enabled = false;
		} else {
			DMERR(" %s Arg[5] invalid\n", __func__);
			err =  DM_REQ_CRYPT_ERROR;
			goto ctr_exit;
		}
	} else {
		DMERR(" %s Arg[5] missing, set FDE enabled.\n", __func__);
		is_fde_enabled = true; /* backward compatible */
	}

	_req_crypt_io_pool = KMEM_CACHE(req_dm_crypt_io, 0);
	if (!_req_crypt_io_pool) {
		err =  DM_REQ_CRYPT_ERROR;
		goto ctr_exit;
	}

	encryption_mode = DM_REQ_CRYPT_ENCRYPTION_MODE_CRYPTO;
	if (argc >= 7 && argv[6]) {
		if (!strcmp(argv[6], "ice"))
			encryption_mode =
				DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT;
	}

	if (encryption_mode == DM_REQ_CRYPT_ENCRYPTION_MODE_TRANSPARENT) {
		/* configure ICE settings */
		ice_settings =
			kzalloc(sizeof(struct ice_crypto_setting), GFP_KERNEL);
		if (!ice_settings) {
			err = -ENOMEM;
			goto ctr_exit;
		}
		ice_settings->key_size = ICE_CRYPTO_KEY_SIZE_128;
		ice_settings->algo_mode = ICE_CRYPTO_ALGO_MODE_AES_XTS;
		ice_settings->key_mode = ICE_CRYPTO_USE_LUT_SW_KEY;
		if (kstrtou16(argv[1], 0, &ice_settings->key_index) ||
			ice_settings->key_index < 0 ||
			ice_settings->key_index > MAX_MSM_ICE_KEY_LUT_SIZE) {
			DMERR("%s Err: key index %d received for ICE\n",
				__func__, ice_settings->key_index);
			err = DM_REQ_CRYPT_ERROR;
			goto ctr_exit;
		}
	} else {
		ret = configure_qcrypto();
		if (ret) {
			DMERR("%s failed to configure qcrypto\n", __func__);
			err = ret;
			goto ctr_exit;
		}
	}

	req_io_pool = mempool_create_slab_pool(MIN_IOS, _req_crypt_io_pool);
	if (!req_io_pool) {
		DMERR("%s req_io_pool not allocated\n", __func__);
		err = -ENOMEM;
		goto ctr_exit;
	}

	/*
	 * If underlying device supports flush, mapped target
	 * should also allow it
	 */
	ti->num_flush_bios = 1;
	/* TODO: Discard support */

	err = 0;
	DMINFO("%s: Mapping block_device %s to dm-req-crypt ok!\n",
	       __func__, argv[3]);
ctr_exit:
	if (err)
		req_crypt_dtr(ti);

	return err;
}

static int req_crypt_iterate_devices(struct dm_target *ti,
				 iterate_devices_callout_fn fn, void *data)
{
	return fn(ti, dev, start_sector_orig, ti->len, data);
}
void set_qcrypto_func_dm(void *dev,
			void *flag,
			void *engines,
			void *engine_list)
{
	dm_qcrypto_func.cipher_set  = dev;
	dm_qcrypto_func.cipher_flag = flag;
	dm_qcrypto_func.get_num_engines = engines;
	dm_qcrypto_func.get_engine_list = engine_list;
}
EXPORT_SYMBOL(set_qcrypto_func_dm);

static struct target_type req_crypt_target = {
	.name   = "req-crypt",
	.version = {1, 0, 0},
	.module = THIS_MODULE,
	.ctr    = req_crypt_ctr,
	.dtr    = req_crypt_dtr,
	.map_rq = req_crypt_map,
	.rq_end_io = req_crypt_endio,
	.iterate_devices = req_crypt_iterate_devices,
};

static int __init req_dm_crypt_init(void)
{
	int r;


	r = dm_register_target(&req_crypt_target);
	if (r < 0) {
		DMERR("register failed %d", r);
		return r;
	}

	DMINFO("dm-req-crypt successfully initalized.\n");

	return r;
}

static void __exit req_dm_crypt_exit(void)
{
	dm_unregister_target(&req_crypt_target);
}

module_init(req_dm_crypt_init);
module_exit(req_dm_crypt_exit);

MODULE_DESCRIPTION(DM_NAME " target for request based transparent encryption / decryption");
MODULE_LICENSE("GPL v2");
