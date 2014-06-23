/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/workqueue.h>
#include <linux/backing-dev.h>
#include <linux/atomic.h>
#include <linux/scatterlist.h>
#include <linux/device-mapper.h>
#include <linux/printk.h>
#include <linux/pft.h>

#include <crypto/scatterwalk.h>
#include <asm/page.h>
#include <asm/unaligned.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/algapi.h>
#include <linux/qcrypto.h>

#define DM_MSG_PREFIX "req-crypt"

#define MAX_SG_LIST	1024
#define REQ_DM_512_KB (512*1024)
#define MAX_ENCRYPTION_BUFFERS 1
#define MIN_IOS 16
#define MIN_POOL_PAGES 32
#define KEY_SIZE_XTS 64
#define AES_XTS_IV_LEN 16

#define DM_REQ_CRYPT_ERROR -1
#define DM_REQ_CRYPT_ERROR_AFTER_PAGE_MALLOC -2

struct req_crypt_result {
	struct completion completion;
	int err;
};

#define FDE_KEY_ID	0
#define PFE_KEY_ID	1

static struct dm_dev *dev;
static struct kmem_cache *_req_crypt_io_pool;
static sector_t start_sector_orig;
static struct workqueue_struct *req_crypt_queue;
static mempool_t *req_io_pool;
static mempool_t *req_page_pool;
static bool is_fde_enabled;
static struct crypto_ablkcipher *tfm;

struct req_dm_crypt_io {
	struct work_struct work;
	struct request *cloned_request;
	int error;
	atomic_t pending;
	struct timespec start_time;
	bool should_encrypt;
	bool should_decrypt;
	u32 key_id;
};

static void req_crypt_cipher_complete
		(struct crypto_async_request *req, int err);


static  bool req_crypt_should_encrypt(struct req_dm_crypt_io *req)
{
	int ret;
	bool should_encrypt = false;
	struct bio *bio = NULL;
	u32 key_id = 0;
	bool is_encrypted = false;
	bool is_inplace = false;

	if (!req || !req->cloned_request || !req->cloned_request->bio)
		return false;

	bio = req->cloned_request->bio;

	ret = pft_get_key_index(bio, &key_id, &is_encrypted, &is_inplace);
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
	int ret;
	bool should_deccrypt = false;
	struct bio *bio = NULL;
	u32 key_id = 0;
	bool is_encrypted = false;
	bool is_inplace = false;

	if (!req || !req->cloned_request || !req->cloned_request->bio)
		return false;

	bio = req->cloned_request->bio;

	ret = pft_get_key_index(bio, &key_id, &is_encrypted, &is_inplace);
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
	int error = 0;
	int total_sg_len = 0, rc = 0, total_bytes_in_req = 0;
	struct ablkcipher_request *req = NULL;
	struct req_crypt_result result;
	struct scatterlist *req_sg_read = NULL;
	int err = 0;
	u8 IV[AES_XTS_IV_LEN];

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

	if (error != 0) {
		err = error;
		goto submit_request;
	}

	req = ablkcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		DMERR("%s ablkcipher request allocation failed\n", __func__);
		err = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	ablkcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
					req_crypt_cipher_complete, &result);
	init_completion(&result.completion);
	err = qcrypto_cipher_set_device(req, io->key_id);
	if (err != 0) {
		DMERR("%s qcrypto_cipher_set_device failed with err %d\n",
				__func__, err);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}
	qcrypto_cipher_set_flag(req,
		QCRYPTO_CTX_USE_PIPE_KEY | QCRYPTO_CTX_XTS_DU_SIZE_512B);
	crypto_ablkcipher_clear_flags(tfm, ~0);
	crypto_ablkcipher_setkey(tfm, NULL, KEY_SIZE_XTS);

	req_sg_read = kzalloc(sizeof(struct scatterlist) *
			MAX_SG_LIST, GFP_KERNEL);
	if (!req_sg_read) {
		DMERR("%s req_sg_read allocation failed\n",
						__func__);
		err = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	total_sg_len = blk_rq_map_sg(clone->q, clone, req_sg_read);
	if ((total_sg_len <= 0) || (total_sg_len > MAX_SG_LIST)) {
		DMERR("%s Request Error%d", __func__, total_sg_len);
		err = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	total_bytes_in_req = clone->__data_len;
	if (total_bytes_in_req > REQ_DM_512_KB) {
		DMERR("%s total_bytes_in_req > 512 MB %d",
				__func__, total_bytes_in_req);
		err = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	memset(IV, 0, AES_XTS_IV_LEN);
	memcpy(IV, &clone->__sector, sizeof(sector_t));

	ablkcipher_request_set_crypt(req, req_sg_read, req_sg_read,
			total_bytes_in_req, (void *) IV);

	rc = crypto_ablkcipher_decrypt(req);

	switch (rc) {
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
		}
		break;

	default:
		err = DM_REQ_CRYPT_ERROR;
		break;
	}

ablkcipher_req_alloc_failure:

	if (req)
		ablkcipher_request_free(req);

	kfree(req_sg_read);

submit_request:
	if (io)
		io->error = err;
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
	struct req_iterator iter = {0, NULL};
	struct req_iterator iter1 = {0, NULL};
	struct ablkcipher_request *req = NULL;
	struct req_crypt_result result;
	struct bio_vec *bvec = NULL;
	struct scatterlist *req_sg_in = NULL;
	struct scatterlist *req_sg_out = NULL;
	int copy_bio_sector_to_req = 0;
	gfp_t gfp_mask = GFP_NOIO | __GFP_HIGHMEM;
	struct page *page = NULL;
	u8 IV[AES_XTS_IV_LEN];
	int remaining_size = 0;
	int err = 0;

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

	init_completion(&result.completion);
	err = qcrypto_cipher_set_device(req, io->key_id);
	if (err != 0) {
		DMERR("%s qcrypto_cipher_set_device failed with err %d\n",
				__func__, err);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}
	qcrypto_cipher_set_flag(req,
		QCRYPTO_CTX_USE_PIPE_KEY | QCRYPTO_CTX_XTS_DU_SIZE_512B);
	crypto_ablkcipher_clear_flags(tfm, ~0);
	crypto_ablkcipher_setkey(tfm, NULL, KEY_SIZE_XTS);

	req_sg_in = kzalloc(sizeof(struct scatterlist) * MAX_SG_LIST,
			GFP_KERNEL);
	if (!req_sg_in) {
		DMERR("%s req_sg_in allocation failed\n",
					__func__);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

	req_sg_out = kzalloc(sizeof(struct scatterlist) * MAX_SG_LIST,
			GFP_KERNEL);
	if (!req_sg_out) {
		DMERR("%s req_sg_out allocation failed\n",
					__func__);
		error = DM_REQ_CRYPT_ERROR;
		goto ablkcipher_req_alloc_failure;
	}

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

	rq_for_each_segment(bvec, clone, iter) {
		if (bvec->bv_len > remaining_size) {
			page = NULL;
			while (page == NULL) {
				page = mempool_alloc(req_page_pool, gfp_mask);
				if (!page) {
					DMERR("%s Crypt page alloc failed",
							__func__);
					congestion_wait(BLK_RW_ASYNC, HZ/100);
				}
			}

			bvec->bv_page = page;
			bvec->bv_offset = 0;
			remaining_size = PAGE_SIZE -  bvec->bv_len;
			if (remaining_size < 0)
				BUG();
		} else {
			bvec->bv_page = page;
			bvec->bv_offset = PAGE_SIZE - remaining_size;
			remaining_size = remaining_size -  bvec->bv_len;
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
			clone->buffer = bio_data(bio_src);
			copy_bio_sector_to_req++;
		}
		blk_queue_bounce(clone->q, &bio_src);
	}


ablkcipher_req_alloc_failure:
	if (req)
		ablkcipher_request_free(req);

	if (error == DM_REQ_CRYPT_ERROR_AFTER_PAGE_MALLOC) {
		bvec = NULL;
		rq_for_each_segment(bvec, clone, iter1) {
			if (bvec->bv_offset == 0) {
				mempool_free(bvec->bv_page, req_page_pool);
				bvec->bv_page = NULL;
			} else
				bvec->bv_page = NULL;
		}
	}


	kfree(req_sg_in);

	kfree(req_sg_out);

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
		DMERR("%s received non-write request for Clone %u\n",
				__func__, (unsigned int)io->cloned_request);
	}
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
		if (p->start_sect > (UINT_MAX - bio->bi_sector))
			BUG();

		bio->bi_sector += p->start_sect;
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
	struct req_iterator iter1;
	struct bio_vec *bvec = NULL;
	struct req_dm_crypt_io *req_io = map_context->ptr;

	/* If it is a write request, do nothing just return. */
	bvec = NULL;
	if (rq_data_dir(clone) == WRITE) {
		rq_for_each_segment(bvec, clone, iter1) {
			if (req_io->should_encrypt && bvec->bv_offset == 0) {
				mempool_free(bvec->bv_page, req_page_pool);
				bvec->bv_page = NULL;
			} else
				bvec->bv_page = NULL;
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

	if ((rq_data_dir(clone) != READ) &&
			 (rq_data_dir(clone) != WRITE)) {
		error = DM_REQ_CRYPT_ERROR;
		DMERR("%s Unknown request\n", __func__);
		goto submit_request;
	}

	req_io = mempool_alloc(req_io_pool, GFP_NOWAIT);
	if (!req_io) {
		DMERR("%s req_io allocation failed\n", __func__);
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
		 * the request. When the crypt endio is called, post-processsing
		 * is done and then the dm layer will complete the bios (clones)
		 * and free them.
		 */
		bio_src->bi_flags |= 1 << BIO_DONTFREE;

		/*
		 * If this device has partitions, remap block n
		 * of partition p to block n+start(p) of the disk.
		 */
		req_crypt_blk_partition_remap(bio_src);
		if (copy_bio_sector_to_req == 0) {
			clone->__sector = bio_src->bi_sector;
			clone->buffer = bio_data(bio_src);
			copy_bio_sector_to_req++;
		}
		blk_queue_bounce(clone->q, &bio_src);
	}

	if (rq_data_dir(clone) == READ) {
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

static void req_crypt_dtr(struct dm_target *ti)
{
	DMDEBUG("dm-req-crypt Destructor.\n");

	if (req_crypt_queue) {
		destroy_workqueue(req_crypt_queue);
		req_crypt_queue = NULL;
	}
	if (req_io_pool) {
		mempool_destroy(req_io_pool);
		req_io_pool = NULL;
	}
	if (req_page_pool) {
		mempool_destroy(req_page_pool);
		req_page_pool = NULL;
	}
	if (tfm) {
		crypto_free_ablkcipher(tfm);
		tfm = NULL;
	}
}


/*
 * Construct an encryption mapping:
 * <cipher> <key> <iv_offset> <dev_path> <start>
 */
static int req_crypt_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	unsigned long long tmpll;
	char dummy;
	int err = DM_REQ_CRYPT_ERROR;

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

	if (argv[5]) {
		if (!strcmp(argv[5], "fde_enabled"))
			is_fde_enabled = true;
		else
			is_fde_enabled = false;
	} else {
		DMERR(" %s Arg[5] invalid, set FDE eanbled.\n", __func__);
		is_fde_enabled = true; /* backward compatible */
	}
	DMDEBUG("%s is_fde_enabled=%d\n", __func__, is_fde_enabled);

	req_crypt_queue = alloc_workqueue("req_cryptd",
					WQ_NON_REENTRANT |
					WQ_HIGHPRI |
					WQ_CPU_INTENSIVE|
					WQ_MEM_RECLAIM,
					1);
	if (!req_crypt_queue) {
		DMERR("%s req_crypt_queue not allocated\n", __func__);
		err =  DM_REQ_CRYPT_ERROR;
		goto ctr_exit;
	}

	/* Allocate the crypto alloc blk cipher and keep the handle */
	tfm = crypto_alloc_ablkcipher("qcom-xts(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		DMERR("%s ablkcipher tfm allocation failed : error\n",
					 __func__);
		err =  DM_REQ_CRYPT_ERROR;
		goto ctr_exit;
	}

	req_io_pool = mempool_create_slab_pool(MIN_IOS, _req_crypt_io_pool);
	BUG_ON(!req_io_pool);
	if (!req_io_pool) {
		DMERR("%s req_io_pool not allocated\n", __func__);
		err =  DM_REQ_CRYPT_ERROR;
		goto ctr_exit;
	}

	req_page_pool = mempool_create_page_pool(MIN_POOL_PAGES, 0);
	if (!req_page_pool) {
		DMERR("%s req_page_pool not allocated\n", __func__);
		err =  DM_REQ_CRYPT_ERROR;
		goto ctr_exit;
	}
	err = 0;
ctr_exit:
	if (err != 0) {
		if (req_crypt_queue) {
			destroy_workqueue(req_crypt_queue);
			req_crypt_queue = NULL;
		}
		if (req_io_pool) {
			mempool_destroy(req_io_pool);
			req_io_pool = NULL;
		}
		if (req_page_pool) {
			mempool_destroy(req_page_pool);
			req_page_pool = NULL;
		}
		if (tfm) {
			crypto_free_ablkcipher(tfm);
			tfm = NULL;
		}
	}
	return err;
}

static int req_crypt_iterate_devices(struct dm_target *ti,
				 iterate_devices_callout_fn fn, void *data)
{
	return fn(ti, dev, start_sector_orig, ti->len, data);
}

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

	_req_crypt_io_pool = KMEM_CACHE(req_dm_crypt_io, 0);
	if (!_req_crypt_io_pool)
		return -ENOMEM;

	r = dm_register_target(&req_crypt_target);
	if (r < 0) {
		DMERR("register failed %d", r);
		kmem_cache_destroy(_req_crypt_io_pool);
	}

	DMINFO("dm-req-crypt successfully initalized.\n");

	return r;
}

static void __exit req_dm_crypt_exit(void)
{
	kmem_cache_destroy(_req_crypt_io_pool);
	dm_unregister_target(&req_crypt_target);
}

module_init(req_dm_crypt_init);
module_exit(req_dm_crypt_exit);

MODULE_DESCRIPTION(DM_NAME " target for request based transparent encryption / decryption");
MODULE_LICENSE("GPL v2");
