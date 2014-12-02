/* Qualcomm Crypto Engine driver.
 *
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) "QCE50: %s: " fmt, __func__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/bitops.h>
#include <linux/clk/msm-clk.h>
#include <linux/qcrypto.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <soc/qcom/socinfo.h>

#include "qce.h"
#include "qce50.h"
#include "qcryptohw_50.h"
#include "qce_ota.h"

#define CRYPTO_CONFIG_RESET 0xE001F
#define QCE_MAX_NUM_DSCR    0x500
#define QCE_SECTOR_SIZE	    0x200
#define CE_CLK_100MHZ	100000000
#define CE_CLK_DIV	1000000

static DEFINE_MUTEX(bam_register_lock);
static DEFINE_MUTEX(qce_iomap_mutex);

struct bam_registration_info {
	struct list_head qlist;
	unsigned long handle;
	uint32_t cnt;
	uint32_t bam_mem;
	void __iomem *bam_iobase;
	bool support_cmd_dscr;
};
static LIST_HEAD(qce50_bam_list);

/*
 * CE HW device structure.
 * Each engine has an instance of the structure.
 * Each engine can only handle one crypto operation at one time. It is up to
 * the sw above to ensure single threading of operation on an engine.
 */
struct qce_device {
	struct device *pdev;        /* Handle to platform_device structure */
	struct bam_registration_info *pbam;

	unsigned char *coh_vmem;    /* Allocated coherent virtual memory */
	dma_addr_t coh_pmem;	    /* Allocated coherent physical memory */
	int memsize;				/* Memory allocated */
	uint32_t bam_mem;		/* bam physical address, from DT */
	uint32_t bam_mem_size;		/* bam io size, from DT */
	int is_shared;			/* CE HW is shared */
	bool support_cmd_dscr;
	bool support_hw_key;
	bool support_clk_mgmt_sus_res;
	bool support_only_core_src_clk;

	void __iomem *iobase;	    /* Virtual io base of CE HW  */
	unsigned int phy_iobase;    /* Physical io base of CE HW    */

	struct clk *ce_core_src_clk;	/* Handle to CE src clk*/
	struct clk *ce_core_clk;	/* Handle to CE clk */
	struct clk *ce_clk;		/* Handle to CE clk */
	struct clk *ce_bus_clk;	/* Handle to CE AXI clk*/

	qce_comp_func_ptr_t qce_cb;	/* qce callback function pointer */

	int assoc_nents;
	int ivsize;
	int authsize;
	int src_nents;
	int dst_nents;

	dma_addr_t phy_iv_in;
	unsigned char dec_iv[16];
	int dir;
	void *areq;
	enum qce_cipher_mode_enum mode;
	struct qce_ce_cfg_reg_setting reg;
	struct ce_sps_data ce_sps;
	uint32_t engines_avail;
	dma_addr_t phy_ota_src;
	dma_addr_t phy_ota_dst;
	unsigned int ota_size;
	unsigned int ce_opp_freq_hz;

	bool use_sw_aes_cbc_ecb_ctr_algo;
	bool use_sw_aead_algo;
	bool use_sw_aes_xts_algo;
	bool use_sw_ahash_algo;
	bool use_sw_hmac_algo;
	bool use_sw_aes_ccm_algo;
};

/* Standard initialization vector for SHA-1, source: FIPS 180-2 */
static uint32_t  _std_init_vector_sha1[] =   {
	0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
};

/* Standard initialization vector for SHA-256, source: FIPS 180-2 */
static uint32_t _std_init_vector_sha256[] = {
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C,	0x1F83D9AB, 0x5BE0CD19
};

static void _byte_stream_to_net_words(uint32_t *iv, unsigned char *b,
		unsigned int len)
{
	unsigned n;

	n = len  / sizeof(uint32_t);
	for (; n > 0; n--) {
		*iv =  ((*b << 24)      & 0xff000000) |
				(((*(b+1)) << 16) & 0xff0000)   |
				(((*(b+2)) << 8) & 0xff00)     |
				(*(b+3)          & 0xff);
		b += sizeof(uint32_t);
		iv++;
	}

	n = len %  sizeof(uint32_t);
	if (n == 3) {
		*iv = ((*b << 24) & 0xff000000) |
				(((*(b+1)) << 16) & 0xff0000)   |
				(((*(b+2)) << 8) & 0xff00);
	} else if (n == 2) {
		*iv = ((*b << 24) & 0xff000000) |
				(((*(b+1)) << 16) & 0xff0000);
	} else if (n == 1) {
		*iv = ((*b << 24) & 0xff000000);
	}
}

static void _byte_stream_swap_to_net_words(uint32_t *iv, unsigned char *b,
		unsigned int len)
{
	unsigned i, j;
	unsigned char swap_iv[AES_IV_LENGTH];

	memset(swap_iv, 0, AES_IV_LENGTH);
	for (i = (AES_IV_LENGTH-len), j = len-1;  i < AES_IV_LENGTH; i++, j--)
		swap_iv[i] = b[j];
	_byte_stream_to_net_words(iv, swap_iv, AES_IV_LENGTH);
}

static int count_sg(struct scatterlist *sg, int nbytes)
{
	int i;

	for (i = 0; nbytes > 0; i++, sg = scatterwalk_sg_next(sg))
		nbytes -= sg->length;
	return i;
}

static int qce_dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
	enum dma_data_direction direction)
{
	int i;

	for (i = 0; i < nents; ++i) {
		dma_map_sg(dev, sg, 1, direction);
		sg = scatterwalk_sg_next(sg);
	}

	return nents;
}

static int qce_dma_unmap_sg(struct device *dev, struct scatterlist *sg,
	int nents, enum dma_data_direction direction)
{
	int i;

	for (i = 0; i < nents; ++i) {
		dma_unmap_sg(dev, sg, 1, direction);
		sg = scatterwalk_sg_next(sg);
	}

	return nents;
}

static int _probe_ce_engine(struct qce_device *pce_dev)
{
	unsigned int rev;
	unsigned int maj_rev, min_rev, step_rev;

	rev = readl_relaxed(pce_dev->iobase + CRYPTO_VERSION_REG);
	mb();
	maj_rev = (rev & CRYPTO_CORE_MAJOR_REV_MASK) >> CRYPTO_CORE_MAJOR_REV;
	min_rev = (rev & CRYPTO_CORE_MINOR_REV_MASK) >> CRYPTO_CORE_MINOR_REV;
	step_rev = (rev & CRYPTO_CORE_STEP_REV_MASK) >> CRYPTO_CORE_STEP_REV;

	if (maj_rev != 0x05) {
		pr_err("Unknown Qualcomm crypto device at 0x%x, rev %d.%d.%d\n",
			pce_dev->phy_iobase, maj_rev, min_rev, step_rev);
		return -EIO;
	};
	pce_dev->ce_sps.minor_version = min_rev;

	pce_dev->engines_avail = readl_relaxed(pce_dev->iobase +
					CRYPTO_ENGINES_AVAIL);
	dev_info(pce_dev->pdev, "Qualcomm Crypto %d.%d.%d device found @0x%x\n",
			maj_rev, min_rev, step_rev, pce_dev->phy_iobase);

	pce_dev->ce_sps.ce_burst_size = MAX_CE_BAM_BURST_SIZE;

	dev_info(pce_dev->pdev,
			"CE device = 0x%x\n, "
			"IO base, CE = 0x%p\n, "
			"Consumer (IN) PIPE %d,    "
			"Producer (OUT) PIPE %d\n"
			"IO base BAM = 0x%p\n"
			"BAM IRQ %d\n"
			"Engines Availability = 0x%x\n",
			pce_dev->ce_sps.ce_device,
			pce_dev->iobase,
			pce_dev->ce_sps.dest_pipe_index,
			pce_dev->ce_sps.src_pipe_index,
			pce_dev->ce_sps.bam_iobase,
			pce_dev->ce_sps.bam_irq,
			pce_dev->engines_avail);
	return 0;
};

static struct qce_cmdlist_info *_ce_get_hash_cmdlistinfo(
			struct qce_device *pce_dev, struct qce_sha_req *sreq)
{
	struct qce_cmdlistptr_ops *cmdlistptr = &pce_dev->ce_sps.cmdlistptr;

	switch (sreq->alg) {
	case QCE_HASH_SHA1:
		return &cmdlistptr->auth_sha1;
	case QCE_HASH_SHA256:
		return &cmdlistptr->auth_sha256;
	case QCE_HASH_SHA1_HMAC:
		return &cmdlistptr->auth_sha1_hmac;
	case QCE_HASH_SHA256_HMAC:
		return &cmdlistptr->auth_sha256_hmac;
	case QCE_HASH_AES_CMAC:
		if (sreq->authklen == AES128_KEY_SIZE)
			return &cmdlistptr->auth_aes_128_cmac;
		return &cmdlistptr->auth_aes_256_cmac;
	default:
		return NULL;
	}
	return NULL;
}

static int _ce_setup_hash(struct qce_device *pce_dev,
				struct qce_sha_req *sreq,
				struct qce_cmdlist_info *cmdlistinfo)
{
	uint32_t auth32[SHA256_DIGEST_SIZE / sizeof(uint32_t)];
	uint32_t diglen;
	int i;
	uint32_t mackey32[SHA_HMAC_KEY_SIZE/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	bool sha1 = false;
	struct sps_command_element *pce = NULL;
	bool use_hw_key = false;
	bool use_pipe_key = false;
	uint32_t authk_size_in_word = sreq->authklen/sizeof(uint32_t);
	uint32_t auth_cfg;

	if ((sreq->alg == QCE_HASH_SHA1_HMAC) ||
			(sreq->alg == QCE_HASH_SHA256_HMAC) ||
			(sreq->alg ==  QCE_HASH_AES_CMAC)) {


		/* no more check for null key. use flag */
		if ((sreq->flags & QCRYPTO_CTX_USE_HW_KEY)
						== QCRYPTO_CTX_USE_HW_KEY)
			use_hw_key = true;
		else if ((sreq->flags & QCRYPTO_CTX_USE_PIPE_KEY) ==
						QCRYPTO_CTX_USE_PIPE_KEY)
			use_pipe_key = true;
		pce = cmdlistinfo->go_proc;
		if (use_hw_key == true) {
			pce->addr = (uint32_t)(CRYPTO_GOPROC_QC_KEY_REG +
							pce_dev->phy_iobase);
		} else {
			pce->addr = (uint32_t)(CRYPTO_GOPROC_REG +
							pce_dev->phy_iobase);
			pce = cmdlistinfo->auth_key;
			if (use_pipe_key == false) {
				_byte_stream_to_net_words(mackey32,
						sreq->authkey,
						sreq->authklen);
				for (i = 0; i < authk_size_in_word; i++, pce++)
					pce->data = mackey32[i];
			}
		}
	}

	if (sreq->alg ==  QCE_HASH_AES_CMAC)
		goto go_proc;

	/* if not the last, the size has to be on the block boundary */
	if (sreq->last_blk == 0 && (sreq->size % SHA256_BLOCK_SIZE))
		return -EIO;

	switch (sreq->alg) {
	case QCE_HASH_SHA1:
	case QCE_HASH_SHA1_HMAC:
		diglen = SHA1_DIGEST_SIZE;
		sha1 = true;
		break;
	case QCE_HASH_SHA256:
	case QCE_HASH_SHA256_HMAC:
		diglen = SHA256_DIGEST_SIZE;
		break;
	default:
		return -EINVAL;
	}

	/* write 20/32 bytes, 5/8 words into auth_iv for SHA1/SHA256 */
	if (sreq->first_blk) {
		if (sha1) {
			for (i = 0; i < 5; i++)
				auth32[i] = _std_init_vector_sha1[i];
		} else {
			for (i = 0; i < 8; i++)
				auth32[i] = _std_init_vector_sha256[i];
		}
	} else {
		_byte_stream_to_net_words(auth32, sreq->digest, diglen);
	}

	pce = cmdlistinfo->auth_iv;
	for (i = 0; i < 5; i++, pce++)
		pce->data = auth32[i];

	if ((sreq->alg == QCE_HASH_SHA256) ||
			(sreq->alg == QCE_HASH_SHA256_HMAC)) {
		for (i = 5; i < 8; i++, pce++)
			pce->data = auth32[i];
	}

	/* write auth_bytecnt 0/1, start with 0 */
	pce = cmdlistinfo->auth_bytecount;
	for (i = 0; i < 2; i++, pce++)
		pce->data = sreq->auth_data[i];

	/* Set/reset  last bit in CFG register  */
	pce = cmdlistinfo->auth_seg_cfg;
	auth_cfg = pce->data & ~(1 << CRYPTO_LAST |
				1 << CRYPTO_FIRST |
				1 << CRYPTO_USE_PIPE_KEY_AUTH |
				1 << CRYPTO_USE_HW_KEY_AUTH);
	if (sreq->last_blk)
		auth_cfg |= 1 << CRYPTO_LAST;
	if (sreq->first_blk)
		auth_cfg |= 1 << CRYPTO_FIRST;
	if (use_hw_key)
		auth_cfg |= 1 << CRYPTO_USE_HW_KEY_AUTH;
	if (use_pipe_key)
		auth_cfg |= 1 << CRYPTO_USE_PIPE_KEY_AUTH;
	pce->data = auth_cfg;
go_proc:
	/* write auth seg size */
	pce = cmdlistinfo->auth_seg_size;
	pce->data = sreq->size;

	pce = cmdlistinfo->encr_seg_cfg;
	pce->data = 0;

	/* write auth seg size start*/
	pce = cmdlistinfo->auth_seg_start;
	pce->data = 0;

	/* write seg size */
	pce = cmdlistinfo->seg_size;
	pce->data = sreq->size;

	return 0;
}

static struct qce_cmdlist_info *_ce_get_aead_cmdlistinfo(
			struct qce_device *pce_dev, struct qce_req *creq)
{
	switch (creq->alg) {
	case CIPHER_ALG_DES:
		switch (creq->mode) {
		case QCE_MODE_CBC:
			if (creq->auth_alg == QCE_HASH_SHA1_HMAC)
				return &pce_dev->ce_sps.
					cmdlistptr.aead_hmac_sha1_cbc_des;
			else if (creq->auth_alg == QCE_HASH_SHA256_HMAC)
				return &pce_dev->ce_sps.
					cmdlistptr.aead_hmac_sha256_cbc_des;
			else
				return NULL;
			break;
		default:
			return NULL;
		}
		break;
	case CIPHER_ALG_3DES:
		switch (creq->mode) {
		case QCE_MODE_CBC:
			if (creq->auth_alg == QCE_HASH_SHA1_HMAC)
				return &pce_dev->ce_sps.
					cmdlistptr.aead_hmac_sha1_cbc_3des;
			else if (creq->auth_alg == QCE_HASH_SHA256_HMAC)
				return &pce_dev->ce_sps.
					cmdlistptr.aead_hmac_sha256_cbc_3des;
			else
				return NULL;
			break;
		default:
			return NULL;
		}
		break;
	case CIPHER_ALG_AES:
		switch (creq->mode) {
		case QCE_MODE_CBC:
			if (creq->encklen ==  AES128_KEY_SIZE) {
				if (creq->auth_alg == QCE_HASH_SHA1_HMAC)
					return &pce_dev->ce_sps.cmdlistptr.
						aead_hmac_sha1_cbc_aes_128;
				else if (creq->auth_alg ==
						QCE_HASH_SHA256_HMAC)
					return &pce_dev->ce_sps.cmdlistptr.
						aead_hmac_sha256_cbc_aes_128;
				else
					return NULL;
			} else if (creq->encklen ==  AES256_KEY_SIZE) {
				if (creq->auth_alg == QCE_HASH_SHA1_HMAC)
					return &pce_dev->ce_sps.cmdlistptr.
						aead_hmac_sha1_cbc_aes_256;
				else if (creq->auth_alg ==
						QCE_HASH_SHA256_HMAC)
					return &pce_dev->ce_sps.cmdlistptr.
						aead_hmac_sha256_cbc_aes_256;
				else
					return NULL;
			} else
				return NULL;
			break;
		default:
			return NULL;
		}
		break;

	default:
		return NULL;
	}
	return NULL;
}

static int _ce_setup_aead(struct qce_device *pce_dev, struct qce_req *q_req,
		uint32_t totallen_in, uint32_t coffset,
		struct qce_cmdlist_info *cmdlistinfo)
{
	int32_t authk_size_in_word = SHA_HMAC_KEY_SIZE/sizeof(uint32_t);
	int i;
	uint32_t mackey32[SHA_HMAC_KEY_SIZE/sizeof(uint32_t)] = {0};
	struct sps_command_element *pce;
	uint32_t a_cfg;
	uint32_t enckey32[(MAX_CIPHER_KEY_SIZE*2)/sizeof(uint32_t)] = {0};
	uint32_t enciv32[MAX_IV_LENGTH/sizeof(uint32_t)] = {0};
	uint32_t enck_size_in_word = 0;
	uint32_t enciv_in_word;
	uint32_t key_size;
	uint32_t encr_cfg = 0;
	uint32_t ivsize = q_req->ivsize;

	key_size = q_req->encklen;
	enck_size_in_word = key_size/sizeof(uint32_t);

	switch (q_req->alg) {
	case CIPHER_ALG_DES:
		enciv_in_word = 2;
		break;
	case CIPHER_ALG_3DES:
		enciv_in_word = 2;
		break;
	case CIPHER_ALG_AES:
		if ((key_size != AES128_KEY_SIZE) &&
				(key_size != AES256_KEY_SIZE))
			return -EINVAL;
		enciv_in_word = 4;
		break;
	default:
		return -EINVAL;
	}

	switch (q_req->mode) {
	case QCE_MODE_CBC:
		pce_dev->mode = q_req->mode;
		break;
	default:
		return -EINVAL;
	}
	if (q_req->mode !=  QCE_MODE_ECB) {
		_byte_stream_to_net_words(enciv32, q_req->iv, ivsize);
		pce = cmdlistinfo->encr_cntr_iv;
		for (i = 0; i < enciv_in_word; i++, pce++)
			pce->data = enciv32[i];
	}

	/*
	 * write encr key
	 * do not use  hw key or pipe key
	 */
	_byte_stream_to_net_words(enckey32, q_req->enckey, key_size);
	pce = cmdlistinfo->encr_key;
	for (i = 0; i < enck_size_in_word; i++, pce++)
		pce->data = enckey32[i];

	/* write encr seg cfg */
	pce = cmdlistinfo->encr_seg_cfg;
	encr_cfg = pce->data;
	if (q_req->dir == QCE_ENCRYPT)
		encr_cfg |= (1 << CRYPTO_ENCODE);
	else
		encr_cfg &= ~(1 << CRYPTO_ENCODE);
	pce->data = encr_cfg;

	/* we only support sha1-hmac and sha256-hmac at this point */
	_byte_stream_to_net_words(mackey32, q_req->authkey,
					q_req->authklen);
	pce = cmdlistinfo->auth_key;
	for (i = 0; i < authk_size_in_word; i++, pce++)
		pce->data = mackey32[i];
	pce = cmdlistinfo->auth_iv;

	if (q_req->auth_alg == QCE_HASH_SHA1_HMAC)
		for (i = 0; i < 5; i++, pce++)
			pce->data = _std_init_vector_sha1[i];
	else
		for (i = 0; i < 8; i++, pce++)
			pce->data = _std_init_vector_sha256[i];

	/* write auth_bytecnt 0/1, start with 0 */
	pce = cmdlistinfo->auth_bytecount;
	for (i = 0; i < 2; i++, pce++)
		pce->data = 0;

	pce = cmdlistinfo->auth_seg_cfg;
	a_cfg = pce->data;
	a_cfg &= ~(CRYPTO_AUTH_POS_MASK);
	if (q_req->dir == QCE_ENCRYPT)
		a_cfg |= (CRYPTO_AUTH_POS_AFTER << CRYPTO_AUTH_POS);
	else
		a_cfg |= (CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
	pce->data = a_cfg;

	/* write auth seg size */
	pce = cmdlistinfo->auth_seg_size;
	pce->data = totallen_in;

	/* write auth seg size start*/
	pce = cmdlistinfo->auth_seg_start;
	pce->data = 0;

	/* write seg size */
	pce = cmdlistinfo->seg_size;
	pce->data = totallen_in;

	/* write encr seg size */
	pce = cmdlistinfo->encr_seg_size;
	pce->data = q_req->cryptlen;

	/* write encr seg start */
	pce = cmdlistinfo->encr_seg_start;
	pce->data = (coffset & 0xffff);

	return 0;

};

static struct qce_cmdlist_info *_ce_get_cipher_cmdlistinfo(
			struct qce_device *pce_dev, struct qce_req *creq)
{
	struct qce_cmdlistptr_ops *cmdlistptr = &pce_dev->ce_sps.cmdlistptr;

	if (creq->alg != CIPHER_ALG_AES) {
		switch (creq->alg) {
		case CIPHER_ALG_DES:
			if (creq->mode == QCE_MODE_ECB)
				return &cmdlistptr->cipher_des_ecb;
			return &cmdlistptr->cipher_des_cbc;
		case CIPHER_ALG_3DES:
			if (creq->mode == QCE_MODE_ECB)
				return &cmdlistptr->cipher_3des_ecb;
			return &cmdlistptr->cipher_3des_cbc;
		default:
			return NULL;
		}
	} else {
		switch (creq->mode) {
		case QCE_MODE_ECB:
			if (creq->encklen == AES128_KEY_SIZE)
				return &cmdlistptr->cipher_aes_128_ecb;
			return &cmdlistptr->cipher_aes_256_ecb;
		case QCE_MODE_CBC:
		case QCE_MODE_CTR:
			if (creq->encklen == AES128_KEY_SIZE)
				return &cmdlistptr->cipher_aes_128_cbc_ctr;
			return &cmdlistptr->cipher_aes_256_cbc_ctr;
		case QCE_MODE_XTS:
			if (creq->encklen/2 == AES128_KEY_SIZE)
				return &cmdlistptr->cipher_aes_128_xts;
			return &cmdlistptr->cipher_aes_256_xts;
		case QCE_MODE_CCM:
			if (creq->encklen == AES128_KEY_SIZE)
				return &cmdlistptr->aead_aes_128_ccm;
			return &cmdlistptr->aead_aes_256_ccm;
		default:
			return NULL;
		}
	}
	return NULL;
}

static int _ce_setup_cipher(struct qce_device *pce_dev, struct qce_req *creq,
		uint32_t totallen_in, uint32_t coffset,
		struct qce_cmdlist_info *cmdlistinfo)
{
	uint32_t enckey32[(MAX_CIPHER_KEY_SIZE * 2)/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t enciv32[MAX_IV_LENGTH / sizeof(uint32_t)] = {
			0, 0, 0, 0};
	uint32_t enck_size_in_word = 0;
	uint32_t key_size;
	bool use_hw_key = false;
	bool use_pipe_key = false;
	uint32_t encr_cfg = 0;
	uint32_t ivsize = creq->ivsize;
	int i;
	struct sps_command_element *pce = NULL;

	if (creq->mode == QCE_MODE_XTS)
		key_size = creq->encklen/2;
	else
		key_size = creq->encklen;

	pce = cmdlistinfo->go_proc;
	if ((creq->flags & QCRYPTO_CTX_USE_HW_KEY) == QCRYPTO_CTX_USE_HW_KEY) {
		use_hw_key = true;
	} else {
		if ((creq->flags & QCRYPTO_CTX_USE_PIPE_KEY) ==
					QCRYPTO_CTX_USE_PIPE_KEY)
			use_pipe_key = true;
	}
	pce = cmdlistinfo->go_proc;
	if (use_hw_key == true)
		pce->addr = (uint32_t)(CRYPTO_GOPROC_QC_KEY_REG +
						pce_dev->phy_iobase);
	else
		pce->addr = (uint32_t)(CRYPTO_GOPROC_REG +
						pce_dev->phy_iobase);
	if ((use_pipe_key == false) && (use_hw_key == false)) {
		_byte_stream_to_net_words(enckey32, creq->enckey, key_size);
		enck_size_in_word = key_size/sizeof(uint32_t);
	}

	if ((creq->op == QCE_REQ_AEAD) && (creq->mode == QCE_MODE_CCM)) {
		uint32_t authklen32 = creq->encklen/sizeof(uint32_t);
		uint32_t noncelen32 = MAX_NONCE/sizeof(uint32_t);
		uint32_t nonce32[MAX_NONCE/sizeof(uint32_t)] = {0, 0, 0, 0};
		uint32_t auth_cfg = 0;

		/* write nonce */
		_byte_stream_to_net_words(nonce32, creq->nonce, MAX_NONCE);
		pce = cmdlistinfo->auth_nonce_info;
		for (i = 0; i < noncelen32; i++, pce++)
			pce->data = nonce32[i];

		if (creq->authklen ==  AES128_KEY_SIZE)
			auth_cfg = pce_dev->reg.auth_cfg_aes_ccm_128;
		else {
			if (creq->authklen ==  AES256_KEY_SIZE)
				auth_cfg = pce_dev->reg.auth_cfg_aes_ccm_256;
		}
		if (creq->dir == QCE_ENCRYPT)
			auth_cfg |= (CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
		else
			auth_cfg |= (CRYPTO_AUTH_POS_AFTER << CRYPTO_AUTH_POS);
		auth_cfg |= ((creq->authsize - 1) << CRYPTO_AUTH_SIZE);

		if (use_hw_key == true)	{
			auth_cfg |= (1 << CRYPTO_USE_HW_KEY_AUTH);
		} else {
			auth_cfg &= ~(1 << CRYPTO_USE_HW_KEY_AUTH);
			/* write auth key */
			pce = cmdlistinfo->auth_key;
			for (i = 0; i < authklen32; i++, pce++)
				pce->data = enckey32[i];
		}

		pce = cmdlistinfo->auth_seg_cfg;
		pce->data = auth_cfg;

		pce = cmdlistinfo->auth_seg_size;
		if (creq->dir == QCE_ENCRYPT)
			pce->data = totallen_in;
		else
			pce->data = totallen_in - creq->authsize;
		pce = cmdlistinfo->auth_seg_start;
		pce->data = 0;
	} else {
		if (creq->op != QCE_REQ_AEAD) {
			pce = cmdlistinfo->auth_seg_cfg;
			pce->data = 0;
		}
	}
	switch (creq->mode) {
	case QCE_MODE_ECB:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_ecb_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_ecb_256;
		break;
	case QCE_MODE_CBC:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_cbc_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_cbc_256;
		break;
	case QCE_MODE_XTS:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_xts_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_xts_256;
		break;
	case QCE_MODE_CCM:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_ccm_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_ccm_256;
		encr_cfg |= (CRYPTO_ENCR_MODE_CCM << CRYPTO_ENCR_MODE) |
				(CRYPTO_LAST_CCM_XFR << CRYPTO_LAST_CCM);
		break;
	case QCE_MODE_CTR:
	default:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_ctr_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_ctr_256;
		break;
	}
	pce_dev->mode = creq->mode;

	switch (creq->alg) {
	case CIPHER_ALG_DES:
		if (creq->mode !=  QCE_MODE_ECB) {
			_byte_stream_to_net_words(enciv32, creq->iv, ivsize);
			pce = cmdlistinfo->encr_cntr_iv;
			pce->data = enciv32[0];
			pce++;
			pce->data = enciv32[1];
		}
		if (use_hw_key == false) {
			pce = cmdlistinfo->encr_key;
			pce->data = enckey32[0];
			pce++;
			pce->data = enckey32[1];
		}
		break;
	case CIPHER_ALG_3DES:
		if (creq->mode !=  QCE_MODE_ECB) {
			_byte_stream_to_net_words(enciv32, creq->iv, ivsize);
			pce = cmdlistinfo->encr_cntr_iv;
			pce->data = enciv32[0];
			pce++;
			pce->data = enciv32[1];
		}
		if (use_hw_key == false) {
			/* write encr key */
			pce = cmdlistinfo->encr_key;
			for (i = 0; i < 6; i++, pce++)
				pce->data = enckey32[i];
		}
		break;
	case CIPHER_ALG_AES:
	default:
		if (creq->mode ==  QCE_MODE_XTS) {
			uint32_t xtskey32[MAX_CIPHER_KEY_SIZE/sizeof(uint32_t)]
					= {0, 0, 0, 0, 0, 0, 0, 0};
			uint32_t xtsklen =
					creq->encklen/(2 * sizeof(uint32_t));

			if ((use_hw_key == false) && (use_pipe_key == false)) {
				_byte_stream_to_net_words(xtskey32,
					(creq->enckey + creq->encklen/2),
							creq->encklen/2);
				/* write xts encr key */
				pce = cmdlistinfo->encr_xts_key;
				for (i = 0; i < xtsklen; i++, pce++)
					pce->data = xtskey32[i];
			}
			/* write xts du size */
			pce = cmdlistinfo->encr_xts_du_size;
			switch (creq->flags & QCRYPTO_CTX_XTS_MASK) {
			case QCRYPTO_CTX_XTS_DU_SIZE_512B:
				pce->data = min((unsigned int)QCE_SECTOR_SIZE,
						creq->cryptlen);
				break;
			case QCRYPTO_CTX_XTS_DU_SIZE_1KB:
				pce->data =
					min((unsigned int)QCE_SECTOR_SIZE * 2,
					creq->cryptlen);
				break;
			default:
				pce->data = creq->cryptlen;
				break;
			}
		}
		if (creq->mode !=  QCE_MODE_ECB) {
			if (creq->mode ==  QCE_MODE_XTS)
				_byte_stream_swap_to_net_words(enciv32,
							creq->iv, ivsize);
			else
				_byte_stream_to_net_words(enciv32, creq->iv,
								ivsize);
			/* write encr cntr iv */
			pce = cmdlistinfo->encr_cntr_iv;
			for (i = 0; i < 4; i++, pce++)
				pce->data = enciv32[i];

			if (creq->mode ==  QCE_MODE_CCM) {
				/* write cntr iv for ccm */
				pce = cmdlistinfo->encr_ccm_cntr_iv;
				for (i = 0; i < 4; i++, pce++)
					pce->data = enciv32[i];
				/* update cntr_iv[3] by one */
				pce = cmdlistinfo->encr_cntr_iv;
				pce += 3;
				pce->data += 1;
			}
		}

		if (creq->op == QCE_REQ_ABLK_CIPHER_NO_KEY) {
				encr_cfg |= (CRYPTO_ENCR_KEY_SZ_AES128 <<
						CRYPTO_ENCR_KEY_SZ);
		} else {
			if (use_hw_key == false) {
				/* write encr key */
				pce = cmdlistinfo->encr_key;
				for (i = 0; i < enck_size_in_word; i++, pce++)
					pce->data = enckey32[i];
			}
		} /* else of if (creq->op == QCE_REQ_ABLK_CIPHER_NO_KEY) */
		break;
	} /* end of switch (creq->mode)  */

	if (use_pipe_key)
		encr_cfg |= (CRYPTO_USE_PIPE_KEY_ENCR_ENABLED
					<< CRYPTO_USE_PIPE_KEY_ENCR);

	/* write encr seg cfg */
	pce = cmdlistinfo->encr_seg_cfg;
	if ((creq->alg == CIPHER_ALG_DES) || (creq->alg == CIPHER_ALG_3DES)) {
		if (creq->dir == QCE_ENCRYPT)
			pce->data |= (1 << CRYPTO_ENCODE);
		else
			pce->data &= ~(1 << CRYPTO_ENCODE);
		encr_cfg = pce->data;
	}  else	{
		encr_cfg |=
			((creq->dir == QCE_ENCRYPT) ? 1 : 0) << CRYPTO_ENCODE;
	}
	if (use_hw_key == true)
		encr_cfg |= (CRYPTO_USE_HW_KEY << CRYPTO_USE_HW_KEY_ENCR);
	else
		encr_cfg &= ~(CRYPTO_USE_HW_KEY << CRYPTO_USE_HW_KEY_ENCR);
	pce->data = encr_cfg;

	/* write encr seg size */
	pce = cmdlistinfo->encr_seg_size;
	if ((creq->mode == QCE_MODE_CCM) && (creq->dir == QCE_DECRYPT))
		pce->data = (creq->cryptlen + creq->authsize);
	else
		pce->data = creq->cryptlen;

	/* write encr seg start */
	pce = cmdlistinfo->encr_seg_start;
	pce->data = (coffset & 0xffff);

	/* write seg size  */
	pce = cmdlistinfo->seg_size;
	pce->data = totallen_in;

	return 0;
};

static int _ce_f9_setup(struct qce_device *pce_dev, struct qce_f9_req *req,
		struct qce_cmdlist_info *cmdlistinfo)
{
	uint32_t ikey32[OTA_KEY_SIZE/sizeof(uint32_t)];
	uint32_t key_size_in_word = OTA_KEY_SIZE/sizeof(uint32_t);
	uint32_t cfg;
	struct sps_command_element *pce;
	int i;

	switch (req->algorithm) {
	case QCE_OTA_ALGO_KASUMI:
		cfg = pce_dev->reg.auth_cfg_kasumi;
		break;
	case QCE_OTA_ALGO_SNOW3G:
	default:
		cfg = pce_dev->reg.auth_cfg_snow3g;
		break;
	};

	/* write key in CRYPTO_AUTH_IV0-3_REG */
	_byte_stream_to_net_words(ikey32, &req->ikey[0], OTA_KEY_SIZE);
	pce = cmdlistinfo->auth_iv;
	for (i = 0; i < key_size_in_word; i++, pce++)
		pce->data = ikey32[i];

	/* write last bits  in CRYPTO_AUTH_IV4_REG  */
	pce->data = req->last_bits;

	/* write fresh to CRYPTO_AUTH_BYTECNT0_REG */
	pce = cmdlistinfo->auth_bytecount;
	pce->data = req->fresh;

	/* write count-i  to CRYPTO_AUTH_BYTECNT1_REG */
	pce++;
	pce->data = req->count_i;

	/* write auth seg cfg */
	pce = cmdlistinfo->auth_seg_cfg;
	if (req->direction == QCE_OTA_DIR_DOWNLINK)
		cfg |= BIT(CRYPTO_F9_DIRECTION);
	pce->data = cfg;

	/* write auth seg size */
	pce = cmdlistinfo->auth_seg_size;
	pce->data = req->msize;

	/* write auth seg start*/
	pce = cmdlistinfo->auth_seg_start;
	pce->data = 0;

	/* write seg size  */
	pce = cmdlistinfo->seg_size;
	pce->data = req->msize;


	/* write go */
	pce = cmdlistinfo->go_proc;
	pce->addr = (uint32_t)(CRYPTO_GOPROC_REG + pce_dev->phy_iobase);
	return 0;
}

static int _ce_f8_setup(struct qce_device *pce_dev, struct qce_f8_req *req,
		bool key_stream_mode, uint16_t npkts, uint16_t cipher_offset,
		uint16_t cipher_size,
		struct qce_cmdlist_info *cmdlistinfo)
{
	uint32_t ckey32[OTA_KEY_SIZE/sizeof(uint32_t)];
	uint32_t key_size_in_word = OTA_KEY_SIZE/sizeof(uint32_t);
	uint32_t cfg;
	struct sps_command_element *pce;
	int i;

	switch (req->algorithm) {
	case QCE_OTA_ALGO_KASUMI:
		cfg = pce_dev->reg.encr_cfg_kasumi;
		break;
	case QCE_OTA_ALGO_SNOW3G:
	default:
		cfg = pce_dev->reg.encr_cfg_snow3g;
		break;
	};
	/* write key */
	_byte_stream_to_net_words(ckey32, &req->ckey[0], OTA_KEY_SIZE);
	pce = cmdlistinfo->encr_key;
	for (i = 0; i < key_size_in_word; i++, pce++)
		pce->data = ckey32[i];

	/* write encr seg cfg */
	pce = cmdlistinfo->encr_seg_cfg;
	if (key_stream_mode)
		cfg |= BIT(CRYPTO_F8_KEYSTREAM_ENABLE);
	if (req->direction == QCE_OTA_DIR_DOWNLINK)
		cfg |= BIT(CRYPTO_F8_DIRECTION);
	pce->data = cfg;

	/* write encr seg start */
	pce = cmdlistinfo->encr_seg_start;
	pce->data = (cipher_offset & 0xffff);

	/* write encr seg size  */
	pce = cmdlistinfo->encr_seg_size;
	pce->data = cipher_size;

	/* write seg size  */
	pce = cmdlistinfo->seg_size;
	pce->data = req->data_len;

	/* write cntr0_iv0 for countC */
	pce = cmdlistinfo->encr_cntr_iv;
	pce->data = req->count_c;
	/* write cntr1_iv1 for nPkts, and bearer */
	pce++;
	if (npkts == 1)
		npkts = 0;
	pce->data = req->bearer << CRYPTO_CNTR1_IV1_REG_F8_BEARER |
				npkts << CRYPTO_CNTR1_IV1_REG_F8_PKT_CNT;

	/* write go */
	pce = cmdlistinfo->go_proc;
	pce->addr = (uint32_t)(CRYPTO_GOPROC_REG + pce_dev->phy_iobase);

	return 0;
}

static void _qce_dump_descr_fifos(struct qce_device *pce_dev)
{
	int i, j, ents;
	struct sps_iovec *iovec = pce_dev->ce_sps.in_transfer.iovec;
	uint32_t cmd_flags = SPS_IOVEC_FLAG_CMD;

	pr_info("==============================================\n");
	pr_info("CONSUMER (TX/IN/DEST) PIPE DESCRIPTOR\n");
	pr_info("==============================================\n");
	for (i = 0; i <  pce_dev->ce_sps.in_transfer.iovec_count; i++) {
		pr_info(" [%d] addr=0x%x  size=0x%x  flags=0x%x\n", i,
					iovec->addr, iovec->size, iovec->flags);
		if (iovec->flags & cmd_flags) {
			struct sps_command_element *pced;

			pced = (struct sps_command_element *)
					(GET_VIRT_ADDR(iovec->addr));
			ents = iovec->size/(sizeof(struct sps_command_element));
			for (j = 0; j < ents; j++) {
				pr_info("      [%d] [0x%x] 0x%x\n", j,
					pced->addr, pced->data);
				pced++;
			}
		}
		iovec++;
	}

	pr_info("==============================================\n");
	pr_info("PRODUCER (RX/OUT/SRC) PIPE DESCRIPTOR\n");
	pr_info("==============================================\n");
	iovec = pce_dev->ce_sps.out_transfer.iovec;
	for (i = 0; i <  pce_dev->ce_sps.out_transfer.iovec_count; i++) {
		pr_info(" [%d] addr=0x%x  size=0x%x  flags=0x%x\n", i,
				iovec->addr, iovec->size, iovec->flags);
		iovec++;
	}
}

#ifdef QCE_DEBUG

static void _qce_dump_descr_fifos_dbg(struct qce_device *pce_dev)
{
	_qce_dump_descr_fifos(pce_dev);
}

#define QCE_WRITE_REG(val, addr)					\
{									\
	pr_info("      [0x%p] 0x%x\n", addr, (uint32_t)val);		\
	writel_relaxed(val, addr);					\
}

#else

static void _qce_dump_descr_fifos_dbg(struct qce_device *pce_dev)
{
}

#define QCE_WRITE_REG(val, addr)					\
	writel_relaxed(val, addr)

#endif

static int _ce_setup_hash_direct(struct qce_device *pce_dev,
				struct qce_sha_req *sreq)
{
	uint32_t auth32[SHA256_DIGEST_SIZE / sizeof(uint32_t)];
	uint32_t diglen;
	bool use_hw_key = false;
	bool use_pipe_key = false;
	int i;
	uint32_t mackey32[SHA_HMAC_KEY_SIZE/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t authk_size_in_word = sreq->authklen/sizeof(uint32_t);
	bool sha1 = false;
	uint32_t auth_cfg = 0;

	/* clear status */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_STATUS_REG);

	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_be, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/*
	 * Ensure previous instructions (setting the CONFIG register)
	 * was completed before issuing starting to set other config register
	 * This is to ensure the configurations are done in correct endian-ness
	 * as set in the CONFIG registers
	 */
	mb();

	if (sreq->alg == QCE_HASH_AES_CMAC) {
		/* write seg_cfg */
		QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);
		/* write seg_cfg */
		QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);
		/* write seg_cfg */
		QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_ENCR_SEG_SIZE_REG);

		/* Clear auth_ivn, auth_keyn registers  */
		for (i = 0; i < 16; i++) {
			QCE_WRITE_REG(0, (pce_dev->iobase +
				(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t))));
			QCE_WRITE_REG(0, (pce_dev->iobase +
				(CRYPTO_AUTH_KEY0_REG + i*sizeof(uint32_t))));
		}
		/* write auth_bytecnt 0/1/2/3, start with 0 */
		for (i = 0; i < 4; i++)
			QCE_WRITE_REG(0, pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT0_REG +
						i * sizeof(uint32_t));

		if (sreq->authklen == AES128_KEY_SIZE)
			auth_cfg = pce_dev->reg.auth_cfg_cmac_128;
		else
			auth_cfg = pce_dev->reg.auth_cfg_cmac_256;
	}

	if ((sreq->alg == QCE_HASH_SHA1_HMAC) ||
			(sreq->alg == QCE_HASH_SHA256_HMAC) ||
			(sreq->alg ==  QCE_HASH_AES_CMAC)) {

		_byte_stream_to_net_words(mackey32, sreq->authkey,
						sreq->authklen);

		/* no more check for null key. use flag to check*/

		if ((sreq->flags & QCRYPTO_CTX_USE_HW_KEY) ==
					QCRYPTO_CTX_USE_HW_KEY) {
			use_hw_key = true;
		} else if ((sreq->flags & QCRYPTO_CTX_USE_PIPE_KEY) ==
						QCRYPTO_CTX_USE_PIPE_KEY) {
			use_pipe_key = true;
		} else {
			/* setup key */
			for (i = 0; i < authk_size_in_word; i++)
				QCE_WRITE_REG(mackey32[i], (pce_dev->iobase +
					(CRYPTO_AUTH_KEY0_REG +
							i*sizeof(uint32_t))));
		}
	}

	if (sreq->alg ==  QCE_HASH_AES_CMAC)
		goto go_proc;

	/* if not the last, the size has to be on the block boundary */
	if (sreq->last_blk == 0 && (sreq->size % SHA256_BLOCK_SIZE))
		return -EIO;

	switch (sreq->alg) {
	case QCE_HASH_SHA1:
		auth_cfg = pce_dev->reg.auth_cfg_sha1;
		diglen = SHA1_DIGEST_SIZE;
		sha1 = true;
		break;
	case QCE_HASH_SHA1_HMAC:
		auth_cfg = pce_dev->reg.auth_cfg_hmac_sha1;
		diglen = SHA1_DIGEST_SIZE;
		sha1 = true;
		break;
	case QCE_HASH_SHA256:
		auth_cfg = pce_dev->reg.auth_cfg_sha256;
		diglen = SHA256_DIGEST_SIZE;
		break;
	case QCE_HASH_SHA256_HMAC:
		auth_cfg = pce_dev->reg.auth_cfg_hmac_sha256;
		diglen = SHA256_DIGEST_SIZE;
		break;
	default:
		return -EINVAL;
	}

	/* write 20/32 bytes, 5/8 words into auth_iv for SHA1/SHA256 */
	if (sreq->first_blk) {
		if (sha1) {
			for (i = 0; i < 5; i++)
				auth32[i] = _std_init_vector_sha1[i];
		} else {
			for (i = 0; i < 8; i++)
				auth32[i] = _std_init_vector_sha256[i];
		}
	} else {
		_byte_stream_to_net_words(auth32, sreq->digest, diglen);
	}

	/* Set auth_ivn, auth_keyn registers  */
	for (i = 0; i < 5; i++)
		QCE_WRITE_REG(auth32[i], (pce_dev->iobase +
			(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t))));

	if ((sreq->alg == QCE_HASH_SHA256) ||
			(sreq->alg == QCE_HASH_SHA256_HMAC)) {
		for (i = 5; i < 8; i++)
			QCE_WRITE_REG(auth32[i], (pce_dev->iobase +
				(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t))));
	}


	/* write auth_bytecnt 0/1/2/3, start with 0 */
	for (i = 0; i < 2; i++)
		QCE_WRITE_REG(sreq->auth_data[i], pce_dev->iobase +
					CRYPTO_AUTH_BYTECNT0_REG +
						i * sizeof(uint32_t));

	/* Set/reset  last bit in CFG register  */
	if (sreq->last_blk)
		auth_cfg |= 1 << CRYPTO_LAST;
	else
		auth_cfg &= ~(1 << CRYPTO_LAST);
	if (sreq->first_blk)
		auth_cfg |= 1 << CRYPTO_FIRST;
	else
		auth_cfg &= ~(1 << CRYPTO_FIRST);
	if (use_hw_key)
		auth_cfg |= 1 << CRYPTO_USE_HW_KEY_AUTH;
	if (use_pipe_key)
		auth_cfg |= 1 << CRYPTO_USE_PIPE_KEY_AUTH;
go_proc:
	 /* write seg_cfg */
	QCE_WRITE_REG(auth_cfg, pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);
	/* write auth seg_size   */
	QCE_WRITE_REG(sreq->size, pce_dev->iobase + CRYPTO_AUTH_SEG_SIZE_REG);

	/* write auth_seg_start   */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_SEG_START_REG);

	/* reset encr seg_cfg   */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);

	/* write seg_size   */
	QCE_WRITE_REG(sreq->size, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);

	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_le, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/* issue go to crypto   */
	if (use_hw_key == false) {
		QCE_WRITE_REG(((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
				pce_dev->iobase + CRYPTO_GOPROC_REG);
	} else {
		QCE_WRITE_REG(((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
				pce_dev->iobase + CRYPTO_GOPROC_QC_KEY_REG);
	}
	/*
	 * Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	mb();
	return 0;
}

static int _ce_setup_aead_direct(struct qce_device *pce_dev,
		struct qce_req *q_req, uint32_t totallen_in, uint32_t coffset)
{
	int32_t authk_size_in_word = SHA_HMAC_KEY_SIZE/sizeof(uint32_t);
	int i;
	uint32_t mackey32[SHA_HMAC_KEY_SIZE/sizeof(uint32_t)] = {0};
	uint32_t a_cfg;
	uint32_t enckey32[(MAX_CIPHER_KEY_SIZE*2)/sizeof(uint32_t)] = {0};
	uint32_t enciv32[MAX_IV_LENGTH/sizeof(uint32_t)] = {0};
	uint32_t enck_size_in_word = 0;
	uint32_t enciv_in_word;
	uint32_t key_size;
	uint32_t ivsize = q_req->ivsize;
	uint32_t encr_cfg;


	/* clear status */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_STATUS_REG);

	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_be, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/*
	 * Ensure previous instructions (setting the CONFIG register)
	 * was completed before issuing starting to set other config register
	 * This is to ensure the configurations are done in correct endian-ness
	 * as set in the CONFIG registers
	 */
	mb();

	key_size = q_req->encklen;
	enck_size_in_word = key_size/sizeof(uint32_t);

	switch (q_req->alg) {

	case CIPHER_ALG_DES:

		switch (q_req->mode) {
		case QCE_MODE_CBC:
			encr_cfg = pce_dev->reg.encr_cfg_des_cbc;
			break;
		default:
			return -EINVAL;
		}

		enciv_in_word = 2;
		break;

	case CIPHER_ALG_3DES:

		switch (q_req->mode) {
		case QCE_MODE_CBC:
			encr_cfg = pce_dev->reg.encr_cfg_3des_cbc;
			break;
		default:
			return -EINVAL;
		}

		enciv_in_word = 2;

		break;

	case CIPHER_ALG_AES:

		switch (q_req->mode) {
		case QCE_MODE_CBC:
			if (key_size == AES128_KEY_SIZE)
				encr_cfg = pce_dev->reg.encr_cfg_aes_cbc_128;
			else if (key_size  == AES256_KEY_SIZE)
				encr_cfg = pce_dev->reg.encr_cfg_aes_cbc_256;
			else
				return -EINVAL;
			break;
		default:
		return -EINVAL;
		}

		enciv_in_word = 4;
		break;

	default:
		return -EINVAL;
	}


	pce_dev->mode = q_req->mode;


	/* write CNTR0_IV0_REG */
	if (q_req->mode !=  QCE_MODE_ECB) {
		_byte_stream_to_net_words(enciv32, q_req->iv, ivsize);
		for (i = 0; i < enciv_in_word; i++)
			QCE_WRITE_REG(enciv32[i], pce_dev->iobase +
				(CRYPTO_CNTR0_IV0_REG + i * sizeof(uint32_t)));
	}

	/*
	 * write encr key
	 * do not use  hw key or pipe key
	 */
	_byte_stream_to_net_words(enckey32, q_req->enckey, key_size);
	for (i = 0; i < enck_size_in_word; i++)
		QCE_WRITE_REG(enckey32[i], pce_dev->iobase +
				(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t)));

	/* write encr seg cfg */
	if (q_req->dir == QCE_ENCRYPT)
		encr_cfg |= (1 << CRYPTO_ENCODE);
	QCE_WRITE_REG(encr_cfg, pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);

	/* we only support sha1-hmac and sha256-hmac at this point */
	_byte_stream_to_net_words(mackey32, q_req->authkey,
					q_req->authklen);
	for (i = 0; i < authk_size_in_word; i++)
		QCE_WRITE_REG(mackey32[i], pce_dev->iobase +
			(CRYPTO_AUTH_KEY0_REG + i * sizeof(uint32_t)));

	if (q_req->auth_alg == QCE_HASH_SHA1_HMAC) {
		for (i = 0; i < 5; i++)
			QCE_WRITE_REG(_std_init_vector_sha1[i],
				pce_dev->iobase +
				(CRYPTO_AUTH_IV0_REG + i * sizeof(uint32_t)));
	} else {
		for (i = 0; i < 8; i++)
			QCE_WRITE_REG(_std_init_vector_sha256[i],
				pce_dev->iobase +
				(CRYPTO_AUTH_IV0_REG + i * sizeof(uint32_t)));
	}

	/* write auth_bytecnt 0/1, start with 0 */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_BYTECNT0_REG);
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_BYTECNT1_REG);

	/* write encr seg size    */
	QCE_WRITE_REG(q_req->cryptlen, pce_dev->iobase +
			CRYPTO_ENCR_SEG_SIZE_REG);

	/* write encr start   */
	QCE_WRITE_REG(coffset & 0xffff, pce_dev->iobase +
			CRYPTO_ENCR_SEG_START_REG);

	if (q_req->auth_alg == QCE_HASH_SHA1_HMAC)
		a_cfg = pce_dev->reg.auth_cfg_aead_sha1_hmac;
	else
		a_cfg = pce_dev->reg.auth_cfg_aead_sha256_hmac;

	if (q_req->dir == QCE_ENCRYPT)
		a_cfg |= (CRYPTO_AUTH_POS_AFTER << CRYPTO_AUTH_POS);
	else
		a_cfg |= (CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);

	/* write auth seg_cfg */
	QCE_WRITE_REG(a_cfg, pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);

	/* write auth seg_size   */
	QCE_WRITE_REG(totallen_in, pce_dev->iobase + CRYPTO_AUTH_SEG_SIZE_REG);

	/* write auth_seg_start   */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_SEG_START_REG);


	/* write seg_size   */
	QCE_WRITE_REG(totallen_in, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);


	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_le, (pce_dev->iobase +

							CRYPTO_CONFIG_REG));
	/* issue go to crypto   */
	QCE_WRITE_REG(((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
				pce_dev->iobase + CRYPTO_GOPROC_REG);
	/*
	 * Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	mb();
	return 0;
};

static int _ce_setup_cipher_direct(struct qce_device *pce_dev,
		struct qce_req *creq, uint32_t totallen_in, uint32_t coffset)
{
	uint32_t enckey32[(MAX_CIPHER_KEY_SIZE * 2)/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t enciv32[MAX_IV_LENGTH / sizeof(uint32_t)] = {
			0, 0, 0, 0};
	uint32_t enck_size_in_word = 0;
	uint32_t key_size;
	bool use_hw_key = false;
	bool use_pipe_key = false;
	uint32_t encr_cfg = 0;
	uint32_t ivsize = creq->ivsize;
	int i;

	/* clear status */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_STATUS_REG);

	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_be, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/*
	 * Ensure previous instructions (setting the CONFIG register)
	 * was completed before issuing starting to set other config register
	 * This is to ensure the configurations are done in correct endian-ness
	 * as set in the CONFIG registers
	 */
	mb();

	if (creq->mode == QCE_MODE_XTS)
		key_size = creq->encklen/2;
	else
		key_size = creq->encklen;

	if ((creq->flags & QCRYPTO_CTX_USE_HW_KEY) == QCRYPTO_CTX_USE_HW_KEY) {
		use_hw_key = true;
	} else {
		if ((creq->flags & QCRYPTO_CTX_USE_PIPE_KEY) ==
					QCRYPTO_CTX_USE_PIPE_KEY)
			use_pipe_key = true;
	}
	if ((use_pipe_key == false) && (use_hw_key == false)) {
		_byte_stream_to_net_words(enckey32, creq->enckey, key_size);
		enck_size_in_word = key_size/sizeof(uint32_t);
	}
	if ((creq->op == QCE_REQ_AEAD) && (creq->mode == QCE_MODE_CCM)) {
		uint32_t authklen32 = creq->encklen/sizeof(uint32_t);
		uint32_t noncelen32 = MAX_NONCE/sizeof(uint32_t);
		uint32_t nonce32[MAX_NONCE/sizeof(uint32_t)] = {0, 0, 0, 0};
		uint32_t auth_cfg = 0;

		/* Clear auth_ivn, auth_keyn registers  */
		for (i = 0; i < 16; i++) {
			QCE_WRITE_REG(0, (pce_dev->iobase +
				(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t))));
			QCE_WRITE_REG(0, (pce_dev->iobase +
				(CRYPTO_AUTH_KEY0_REG + i*sizeof(uint32_t))));
		}
		/* write auth_bytecnt 0/1/2/3, start with 0 */
		for (i = 0; i < 4; i++)
			QCE_WRITE_REG(0, pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT0_REG +
						i * sizeof(uint32_t));
		/* write nonce */
		_byte_stream_to_net_words(nonce32, creq->nonce, MAX_NONCE);
		for (i = 0; i < noncelen32; i++)
			QCE_WRITE_REG(nonce32[i], pce_dev->iobase +
				CRYPTO_AUTH_INFO_NONCE0_REG +
					(i*sizeof(uint32_t)));

		if (creq->authklen ==  AES128_KEY_SIZE)
			auth_cfg = pce_dev->reg.auth_cfg_aes_ccm_128;
		else {
			if (creq->authklen ==  AES256_KEY_SIZE)
				auth_cfg = pce_dev->reg.auth_cfg_aes_ccm_256;
		}
		if (creq->dir == QCE_ENCRYPT)
			auth_cfg |= (CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
		else
			auth_cfg |= (CRYPTO_AUTH_POS_AFTER << CRYPTO_AUTH_POS);
		auth_cfg |= ((creq->authsize - 1) << CRYPTO_AUTH_SIZE);

		if (use_hw_key == true)	{
			auth_cfg |= (1 << CRYPTO_USE_HW_KEY_AUTH);
		} else {
			auth_cfg &= ~(1 << CRYPTO_USE_HW_KEY_AUTH);
			/* write auth key */
			for (i = 0; i < authklen32; i++)
				QCE_WRITE_REG(enckey32[i], pce_dev->iobase +
				CRYPTO_AUTH_KEY0_REG + (i*sizeof(uint32_t)));
		}
		QCE_WRITE_REG(auth_cfg, pce_dev->iobase +
						CRYPTO_AUTH_SEG_CFG_REG);
		if (creq->dir == QCE_ENCRYPT) {
			QCE_WRITE_REG(totallen_in, pce_dev->iobase +
						CRYPTO_AUTH_SEG_SIZE_REG);
		} else {
			QCE_WRITE_REG((totallen_in - creq->authsize),
				pce_dev->iobase + CRYPTO_AUTH_SEG_SIZE_REG);
		}
		QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_SEG_START_REG);
	} else {
		if (creq->op != QCE_REQ_AEAD)
			QCE_WRITE_REG(0, pce_dev->iobase +
						CRYPTO_AUTH_SEG_CFG_REG);
	}
	/*
	 * Ensure previous instructions (write to all AUTH registers)
	 * was completed before accessing a register that is not in
	 * in the same 1K range.
	 */
	mb();
	switch (creq->mode) {
	case QCE_MODE_ECB:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_ecb_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_ecb_256;
		break;
	case QCE_MODE_CBC:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_cbc_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_cbc_256;
		break;
	case QCE_MODE_XTS:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_xts_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_xts_256;
		break;
	case QCE_MODE_CCM:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_ccm_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_ccm_256;
		break;
	case QCE_MODE_CTR:
	default:
		if (key_size == AES128_KEY_SIZE)
			encr_cfg = pce_dev->reg.encr_cfg_aes_ctr_128;
		else
			encr_cfg = pce_dev->reg.encr_cfg_aes_ctr_256;
		break;
	}
	pce_dev->mode = creq->mode;

	switch (creq->alg) {
	case CIPHER_ALG_DES:
		if (creq->mode !=  QCE_MODE_ECB) {
			encr_cfg = pce_dev->reg.encr_cfg_des_cbc;
			_byte_stream_to_net_words(enciv32, creq->iv, ivsize);
			QCE_WRITE_REG(enciv32[0], pce_dev->iobase +
						CRYPTO_CNTR0_IV0_REG);
			QCE_WRITE_REG(enciv32[1], pce_dev->iobase +
						CRYPTO_CNTR1_IV1_REG);
		} else {
			encr_cfg = pce_dev->reg.encr_cfg_des_ecb;
		}
		if (use_hw_key == false) {
			QCE_WRITE_REG(enckey32[0], pce_dev->iobase +
							CRYPTO_ENCR_KEY0_REG);
			QCE_WRITE_REG(enckey32[1], pce_dev->iobase +
							CRYPTO_ENCR_KEY1_REG);
		}
		break;
	case CIPHER_ALG_3DES:
		if (creq->mode !=  QCE_MODE_ECB) {
			_byte_stream_to_net_words(enciv32, creq->iv, ivsize);
			QCE_WRITE_REG(enciv32[0], pce_dev->iobase +
						CRYPTO_CNTR0_IV0_REG);
			QCE_WRITE_REG(enciv32[1], pce_dev->iobase +
						CRYPTO_CNTR1_IV1_REG);
			encr_cfg = pce_dev->reg.encr_cfg_3des_cbc;
		} else {
			encr_cfg = pce_dev->reg.encr_cfg_3des_ecb;
		}
		if (use_hw_key == false) {
			/* write encr key */
			for (i = 0; i < 6; i++)
				QCE_WRITE_REG(enckey32[0], (pce_dev->iobase +
				(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t))));
		}
		break;
	case CIPHER_ALG_AES:
	default:
		if (creq->mode ==  QCE_MODE_XTS) {
			uint32_t xtskey32[MAX_CIPHER_KEY_SIZE/sizeof(uint32_t)]
					= {0, 0, 0, 0, 0, 0, 0, 0};
			uint32_t xtsklen =
					creq->encklen/(2 * sizeof(uint32_t));

			if ((use_hw_key == false) && (use_pipe_key == false)) {
				_byte_stream_to_net_words(xtskey32,
					(creq->enckey + creq->encklen/2),
							creq->encklen/2);
				/* write xts encr key */
				for (i = 0; i < xtsklen; i++)
					QCE_WRITE_REG(xtskey32[i],
						pce_dev->iobase +
						CRYPTO_ENCR_XTS_KEY0_REG +
						(i * sizeof(uint32_t)));
			}
			/* write xts du size */
			switch (creq->flags & QCRYPTO_CTX_XTS_MASK) {
			case QCRYPTO_CTX_XTS_DU_SIZE_512B:
				QCE_WRITE_REG(
					min((uint32_t)QCE_SECTOR_SIZE,
					creq->cryptlen), pce_dev->iobase +
					CRYPTO_ENCR_XTS_DU_SIZE_REG);
				break;
			case QCRYPTO_CTX_XTS_DU_SIZE_1KB:
				QCE_WRITE_REG(
					min((uint32_t)(QCE_SECTOR_SIZE * 2),
					creq->cryptlen), pce_dev->iobase +
					CRYPTO_ENCR_XTS_DU_SIZE_REG);
				break;
			default:
				QCE_WRITE_REG(creq->cryptlen,
					pce_dev->iobase +
					CRYPTO_ENCR_XTS_DU_SIZE_REG);
				break;
			}
		}
		if (creq->mode !=  QCE_MODE_ECB) {
			if (creq->mode ==  QCE_MODE_XTS)
				_byte_stream_swap_to_net_words(enciv32,
							creq->iv, ivsize);
			else
				_byte_stream_to_net_words(enciv32, creq->iv,
								ivsize);

			/* write encr cntr iv */
			for (i = 0; i <= 3; i++)
				QCE_WRITE_REG(enciv32[i], pce_dev->iobase +
							CRYPTO_CNTR0_IV0_REG +
							(i * sizeof(uint32_t)));

			if (creq->mode == QCE_MODE_CCM) {
				/* write cntr iv for ccm */
				for (i = 0; i <= 3; i++)
					QCE_WRITE_REG(enciv32[i],
						pce_dev->iobase +
						CRYPTO_ENCR_CCM_INT_CNTR0_REG +
							(i * sizeof(uint32_t)));
				/* update cntr_iv[3] by one */
				QCE_WRITE_REG((enciv32[3] + 1),
							pce_dev->iobase +
							CRYPTO_CNTR0_IV0_REG +
							(3 * sizeof(uint32_t)));
			}
		}

		if (creq->op == QCE_REQ_ABLK_CIPHER_NO_KEY) {
				encr_cfg |= (CRYPTO_ENCR_KEY_SZ_AES128 <<
						CRYPTO_ENCR_KEY_SZ);
		} else {
			if ((use_hw_key == false) && (use_pipe_key == false)) {
				for (i = 0; i < enck_size_in_word; i++)
					QCE_WRITE_REG(enckey32[i],
						pce_dev->iobase +
							CRYPTO_ENCR_KEY0_REG +
							(i * sizeof(uint32_t)));
			}
		} /* else of if (creq->op == QCE_REQ_ABLK_CIPHER_NO_KEY) */
		break;
	} /* end of switch (creq->mode)  */

	if (use_pipe_key)
		encr_cfg |= (CRYPTO_USE_PIPE_KEY_ENCR_ENABLED
					<< CRYPTO_USE_PIPE_KEY_ENCR);

	/* write encr seg cfg */
	encr_cfg |= ((creq->dir == QCE_ENCRYPT) ? 1 : 0) << CRYPTO_ENCODE;
	if (use_hw_key == true)
		encr_cfg |= (CRYPTO_USE_HW_KEY << CRYPTO_USE_HW_KEY_ENCR);
	else
		encr_cfg &= ~(CRYPTO_USE_HW_KEY << CRYPTO_USE_HW_KEY_ENCR);
	/* write encr seg cfg */
	QCE_WRITE_REG(encr_cfg, pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);

	/* write encr seg size */
	if ((creq->mode == QCE_MODE_CCM) && (creq->dir == QCE_DECRYPT)) {
		QCE_WRITE_REG((creq->cryptlen + creq->authsize),
				pce_dev->iobase + CRYPTO_ENCR_SEG_SIZE_REG);
	} else {
		QCE_WRITE_REG(creq->cryptlen,
				pce_dev->iobase + CRYPTO_ENCR_SEG_SIZE_REG);
	}

	/* write encr seg start */
	QCE_WRITE_REG((coffset & 0xffff),
			pce_dev->iobase + CRYPTO_ENCR_SEG_START_REG);

	/* write encr counter mask */
	QCE_WRITE_REG(0xffffffff,
			pce_dev->iobase + CRYPTO_CNTR_MASK_REG);
	QCE_WRITE_REG(0xffffffff,
			pce_dev->iobase + CRYPTO_CNTR_MASK_REG0);
	QCE_WRITE_REG(0xffffffff,
			pce_dev->iobase + CRYPTO_CNTR_MASK_REG1);
	QCE_WRITE_REG(0xffffffff,
			pce_dev->iobase + CRYPTO_CNTR_MASK_REG2);

	/* write seg size  */
	QCE_WRITE_REG(totallen_in, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);

	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_le, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/* issue go to crypto   */
	if (use_hw_key == false) {
		QCE_WRITE_REG(((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
				pce_dev->iobase + CRYPTO_GOPROC_REG);
	} else {
		QCE_WRITE_REG(((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
				pce_dev->iobase + CRYPTO_GOPROC_QC_KEY_REG);
	}
	/*
	 * Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	mb();
	return 0;
};

static int _ce_f9_setup_direct(struct qce_device *pce_dev,
				 struct qce_f9_req *req)
{
	uint32_t ikey32[OTA_KEY_SIZE/sizeof(uint32_t)];
	uint32_t key_size_in_word = OTA_KEY_SIZE/sizeof(uint32_t);
	uint32_t auth_cfg;
	int i;

	switch (req->algorithm) {
	case QCE_OTA_ALGO_KASUMI:
		auth_cfg = pce_dev->reg.auth_cfg_kasumi;
		break;
	case QCE_OTA_ALGO_SNOW3G:
	default:
		auth_cfg = pce_dev->reg.auth_cfg_snow3g;
		break;
	};

	/* clear status */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_STATUS_REG);

	/* set big endian configuration */
	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_be, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/*
	 * Ensure previous instructions (setting the CONFIG register)
	 * was completed before issuing starting to set other config register
	 * This is to ensure the configurations are done in correct endian-ness
	 * as set in the CONFIG registers
	 */
	mb();

	/* write enc_seg_cfg */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);

	/* write ecn_seg_size */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_ENCR_SEG_SIZE_REG);

	/* write key in CRYPTO_AUTH_IV0-3_REG */
	_byte_stream_to_net_words(ikey32, &req->ikey[0], OTA_KEY_SIZE);
	for (i = 0; i < key_size_in_word; i++)
		QCE_WRITE_REG(ikey32[i], (pce_dev->iobase +
			(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t))));

	/* write last bits  in CRYPTO_AUTH_IV4_REG  */
	QCE_WRITE_REG(req->last_bits, (pce_dev->iobase +
					CRYPTO_AUTH_IV4_REG));

	/* write fresh to CRYPTO_AUTH_BYTECNT0_REG */
	QCE_WRITE_REG(req->fresh, (pce_dev->iobase +
					 CRYPTO_AUTH_BYTECNT0_REG));

	/* write count-i  to CRYPTO_AUTH_BYTECNT1_REG */
	QCE_WRITE_REG(req->count_i, (pce_dev->iobase +
					 CRYPTO_AUTH_BYTECNT1_REG));

	/* write auth seg cfg */
	if (req->direction == QCE_OTA_DIR_DOWNLINK)
		auth_cfg |= BIT(CRYPTO_F9_DIRECTION);
	QCE_WRITE_REG(auth_cfg, pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);

	/* write auth seg size */
	QCE_WRITE_REG(req->msize, pce_dev->iobase + CRYPTO_AUTH_SEG_SIZE_REG);

	/* write auth seg start*/
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_SEG_START_REG);

	/* write seg size  */
	QCE_WRITE_REG(req->msize, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);

	/* set little endian configuration before go*/
	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_le, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/* write go */
	QCE_WRITE_REG(((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
				pce_dev->iobase +  CRYPTO_GOPROC_REG);
	/*
	 * Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	mb();
	return 0;
}

static int _ce_f8_setup_direct(struct qce_device *pce_dev,
		struct qce_f8_req *req, bool key_stream_mode,
		uint16_t npkts, uint16_t cipher_offset, uint16_t cipher_size)
{
	int i = 0;
	uint32_t encr_cfg = 0;
	uint32_t ckey32[OTA_KEY_SIZE/sizeof(uint32_t)];
	uint32_t key_size_in_word = OTA_KEY_SIZE/sizeof(uint32_t);

	switch (req->algorithm) {
	case QCE_OTA_ALGO_KASUMI:
		encr_cfg = pce_dev->reg.encr_cfg_kasumi;
		break;
	case QCE_OTA_ALGO_SNOW3G:
	default:
		encr_cfg = pce_dev->reg.encr_cfg_snow3g;
		break;
	};
	/* clear status */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_STATUS_REG);
	/* set big endian configuration */
	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_be, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/* write auth seg configuration */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);
	/* write auth seg size */
	QCE_WRITE_REG(0, pce_dev->iobase + CRYPTO_AUTH_SEG_SIZE_REG);

	/* write key */
	_byte_stream_to_net_words(ckey32, &req->ckey[0], OTA_KEY_SIZE);

	for (i = 0; i < key_size_in_word; i++)
		QCE_WRITE_REG(ckey32[i], (pce_dev->iobase +
			(CRYPTO_ENCR_KEY0_REG + i*sizeof(uint32_t))));
	/* write encr seg cfg */
	if (key_stream_mode)
		encr_cfg |= BIT(CRYPTO_F8_KEYSTREAM_ENABLE);
	if (req->direction == QCE_OTA_DIR_DOWNLINK)
		encr_cfg |= BIT(CRYPTO_F8_DIRECTION);
	QCE_WRITE_REG(encr_cfg, pce_dev->iobase +
		CRYPTO_ENCR_SEG_CFG_REG);

	/* write encr seg start */
	QCE_WRITE_REG((cipher_offset & 0xffff), pce_dev->iobase +
		CRYPTO_ENCR_SEG_START_REG);
	/* write encr seg size  */
	QCE_WRITE_REG(cipher_size, pce_dev->iobase +
		CRYPTO_ENCR_SEG_SIZE_REG);

	/* write seg size  */
	QCE_WRITE_REG(req->data_len, pce_dev->iobase +
		CRYPTO_SEG_SIZE_REG);

	/* write cntr0_iv0 for countC */
	QCE_WRITE_REG(req->count_c, pce_dev->iobase +
		CRYPTO_CNTR0_IV0_REG);
	/* write cntr1_iv1 for nPkts, and bearer */
	if (npkts == 1)
		npkts = 0;
	QCE_WRITE_REG(req->bearer << CRYPTO_CNTR1_IV1_REG_F8_BEARER |
				npkts << CRYPTO_CNTR1_IV1_REG_F8_PKT_CNT,
			pce_dev->iobase + CRYPTO_CNTR1_IV1_REG);

	/* set little endian configuration before go*/
	QCE_WRITE_REG(pce_dev->reg.crypto_cfg_le, (pce_dev->iobase +
							CRYPTO_CONFIG_REG));
	/* write go */
	QCE_WRITE_REG(((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
				pce_dev->iobase +  CRYPTO_GOPROC_REG);
	/*
	 * Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	mb();
	return 0;
}


static int _qce_unlock_other_pipes(struct qce_device *pce_dev)
{
	int rc = 0;

	if (pce_dev->support_cmd_dscr == false)
		return rc;

	pce_dev->ce_sps.consumer.event.callback = NULL;
	rc = sps_transfer_one(pce_dev->ce_sps.consumer.pipe,
	GET_PHYS_ADDR(pce_dev->ce_sps.cmdlistptr.unlock_all_pipes.cmdlist),
	0, NULL, (SPS_IOVEC_FLAG_CMD | SPS_IOVEC_FLAG_UNLOCK));
	if (rc) {
		pr_err("sps_xfr_one() fail rc=%d", rc);
		rc = -EINVAL;
	}
	return rc;
}

static int _aead_complete(struct qce_device *pce_dev)
{
	struct aead_request *areq;
	unsigned char mac[SHA256_DIGEST_SIZE];
	uint32_t status;
	int32_t result_status;

	areq = (struct aead_request *) pce_dev->areq;
	if (areq->src != areq->dst) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
					DMA_FROM_DEVICE);
	}
	qce_dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	qce_dma_unmap_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
			DMA_TO_DEVICE);
	/* check MAC */
	memcpy(mac, (char *)(&pce_dev->ce_sps.result->auth_iv[0]),
						SHA256_DIGEST_SIZE);

	/* read status before unlock */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);

	if (_qce_unlock_other_pipes(pce_dev))
		return -EINVAL;

	/*
	 * Don't use result dump status. The operation may not
	 * be complete.
	 * Instead, use the status we just read of device.
	 * In case, we need to use result_status from result
	 * dump the result_status needs to be byte swapped,
	 * since we set the device to little endian.
	 */
	result_status = 0;
	pce_dev->ce_sps.result->status = 0;

	if (status & ((1 << CRYPTO_SW_ERR) | (1 << CRYPTO_AXI_ERR)
			| (1 <<  CRYPTO_HSD_ERR))) {
		pr_err("aead operation error. Status %x\n", status);
		result_status = -ENXIO;
	} else if (pce_dev->ce_sps.consumer_status |
				pce_dev->ce_sps.producer_status)  {
		pr_err("aead sps operation error. sps status %x %x\n",
				pce_dev->ce_sps.consumer_status,
				pce_dev->ce_sps.producer_status);
		result_status = -ENXIO;
	} else if ((status & (1 << CRYPTO_OPERATION_DONE)) == 0) {
		pr_err("aead operation not done? Status %x, sps status %x %x\n",
				status,
				pce_dev->ce_sps.consumer_status,
				pce_dev->ce_sps.producer_status);
		result_status = -ENXIO;
	}

	if (pce_dev->mode == QCE_MODE_CCM) {

		if (result_status == 0 && (status & (1 << CRYPTO_MAC_FAILED)))
			result_status = -EBADMSG;
		pce_dev->qce_cb(areq, mac, NULL, result_status);

	} else {
		uint32_t ivsize = 0;
		struct crypto_aead *aead;
		unsigned char iv[NUM_OF_CRYPTO_CNTR_IV_REG * CRYPTO_REG_SIZE];
		aead = crypto_aead_reqtfm(areq);
		ivsize = crypto_aead_ivsize(aead);
		if (pce_dev->ce_sps.minor_version != 0)
			dma_unmap_single(pce_dev->pdev, pce_dev->phy_iv_in,
							ivsize, DMA_TO_DEVICE);
		memcpy(iv, (char *)(pce_dev->ce_sps.result->encr_cntr_iv),
			sizeof(iv));
		pce_dev->qce_cb(areq, mac, iv, result_status);

	}
	return 0;
};

static int _sha_complete(struct qce_device *pce_dev)
{
	struct ahash_request *areq;
	unsigned char digest[SHA256_DIGEST_SIZE];
	uint32_t bytecount32[2];
	int32_t result_status = pce_dev->ce_sps.result->status;
	uint32_t status;

	areq = (struct ahash_request *) pce_dev->areq;
	qce_dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
				DMA_TO_DEVICE);
	memcpy(digest, (char *)(&pce_dev->ce_sps.result->auth_iv[0]),
						SHA256_DIGEST_SIZE);
	_byte_stream_to_net_words(bytecount32,
		(unsigned char *)pce_dev->ce_sps.result->auth_byte_count,
					2 * CRYPTO_REG_SIZE);

	/* read status before unlock */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);

	if (_qce_unlock_other_pipes(pce_dev))
		return -EINVAL;

	/*
	 * Don't use result dump status. The operation may not be complete.
	 * Instead, use the status we just read of device.
	 * In case, we need to use result_status from result
	 * dump the result_status needs to be byte swapped,
	 * since we set the device to little endian.
	 */

	if (status & ((1 << CRYPTO_SW_ERR) | (1 << CRYPTO_AXI_ERR)
			| (1 <<  CRYPTO_HSD_ERR))) {

		pr_err("sha operation error. Status %x\n", status);
		result_status = -ENXIO;
	} else if (pce_dev->ce_sps.consumer_status) {
		pr_err("sha sps operation error. sps status %x\n",
			pce_dev->ce_sps.consumer_status);
		result_status = -ENXIO;
	} else if ((status & (1 << CRYPTO_OPERATION_DONE)) == 0) {
		pr_err("sha operation not done? Status %x, sps status %x\n",
			status, pce_dev->ce_sps.consumer_status);
		result_status = -ENXIO;
	} else {
		result_status = 0;
	}
	pce_dev->qce_cb(areq, digest, (char *)bytecount32,
				result_status);
	return 0;
};

static int _f9_complete(struct qce_device *pce_dev)
{
	uint32_t mac_i;
	uint32_t status;
	int32_t result_status;

	dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_src,
				pce_dev->ota_size, DMA_TO_DEVICE);
	_byte_stream_to_net_words(&mac_i,
		(char *)(&pce_dev->ce_sps.result->auth_iv[0]),
		CRYPTO_REG_SIZE);
	/* read status before unlock */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if (_qce_unlock_other_pipes(pce_dev)) {
		pce_dev->qce_cb(pce_dev->areq, NULL, NULL, -ENXIO);
		return -ENXIO;
	}
	if (status & ((1 << CRYPTO_SW_ERR) | (1 << CRYPTO_AXI_ERR)
				| (1 <<  CRYPTO_HSD_ERR))) {
		pr_err("f9 operation error. Status %x\n", status);
		result_status = -ENXIO;
	} else if (pce_dev->ce_sps.consumer_status |
				pce_dev->ce_sps.producer_status)  {
		pr_err("f9 sps operation error. sps status %x %x\n",
				pce_dev->ce_sps.consumer_status,
				pce_dev->ce_sps.producer_status);
		result_status = -ENXIO;
	} else if ((status & (1 << CRYPTO_OPERATION_DONE)) == 0) {
		pr_err("f9 operation not done? Status %x, sps status %x %x\n",
			status,
			pce_dev->ce_sps.consumer_status,
			pce_dev->ce_sps.producer_status);
		result_status = -ENXIO;
	} else {
		result_status = 0;
	}
	pce_dev->qce_cb(pce_dev->areq, (char *)&mac_i, NULL, result_status);

	return 0;
}

static int _ablk_cipher_complete(struct qce_device *pce_dev)
{
	struct ablkcipher_request *areq;
	unsigned char iv[NUM_OF_CRYPTO_CNTR_IV_REG * CRYPTO_REG_SIZE];
	uint32_t status;
	int32_t result_status;

	areq = (struct ablkcipher_request *) pce_dev->areq;

	if (areq->src != areq->dst) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->dst,
			pce_dev->dst_nents, DMA_FROM_DEVICE);
	}
	qce_dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
		(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
						DMA_TO_DEVICE);

	/* read status before unlock */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);

	if (_qce_unlock_other_pipes(pce_dev))
		return -EINVAL;

	/*
	 * Don't use result dump status. The operation may not be complete.
	 * Instead, use the status we just read of device.
	 * In case, we need to use result_status from result
	 * dump the result_status needs to be byte swapped,
	 * since we set the device to little endian.
	 */
	if (status & ((1 << CRYPTO_SW_ERR) | (1 << CRYPTO_AXI_ERR)
			| (1 <<  CRYPTO_HSD_ERR))) {
		pr_err("ablk_cipher operation error. Status %x\n",
				status);
		result_status = -ENXIO;
	} else if (pce_dev->ce_sps.consumer_status |
				pce_dev->ce_sps.producer_status)  {
		pr_err("ablk_cipher sps operation error. sps status %x %x\n",
				pce_dev->ce_sps.consumer_status,
				pce_dev->ce_sps.producer_status);
		result_status = -ENXIO;
	} else if ((status & (1 << CRYPTO_OPERATION_DONE)) == 0) {
		pr_err("ablk_cipher operation not done? Status %x, sps status %x %x\n",
			status,
			pce_dev->ce_sps.consumer_status,
			pce_dev->ce_sps.producer_status);
		result_status = -ENXIO;

	} else {
		result_status = 0;
	}

	if (pce_dev->mode == QCE_MODE_ECB) {
		pce_dev->qce_cb(areq, NULL, NULL,
					pce_dev->ce_sps.consumer_status |
					result_status);
	} else {
		if (pce_dev->ce_sps.minor_version == 0) {
			if (pce_dev->mode == QCE_MODE_CBC) {
				if  (pce_dev->dir == QCE_DECRYPT)
					memcpy(iv, (char *)pce_dev->dec_iv,
								sizeof(iv));
				else
					memcpy(iv, (unsigned char *)
						(sg_virt(areq->src) +
						areq->src->length - 16),
						sizeof(iv));
			}
			if ((pce_dev->mode == QCE_MODE_CTR) ||
				(pce_dev->mode == QCE_MODE_XTS)) {
				uint32_t num_blk = 0;
				uint32_t cntr_iv3 = 0;
				unsigned long long cntr_iv64 = 0;
				unsigned char *b = (unsigned char *)(&cntr_iv3);

				memcpy(iv, areq->info, sizeof(iv));
				if (pce_dev->mode != QCE_MODE_XTS)
					num_blk = areq->nbytes/16;
				else
					num_blk = 1;
				cntr_iv3 =  ((*(iv + 12) << 24) & 0xff000000) |
					(((*(iv + 13)) << 16) & 0xff0000) |
					(((*(iv + 14)) << 8) & 0xff00) |
					(*(iv + 15) & 0xff);
				cntr_iv64 =
					(((unsigned long long)cntr_iv3 &
					(unsigned long long)0xFFFFFFFFULL) +
					(unsigned long long)num_blk) %
					(unsigned long long)(0x100000000ULL);

				cntr_iv3 = (u32)(cntr_iv64 & 0xFFFFFFFF);
				*(iv + 15) = (char)(*b);
				*(iv + 14) = (char)(*(b + 1));
				*(iv + 13) = (char)(*(b + 2));
				*(iv + 12) = (char)(*(b + 3));
			}
		} else {
			memcpy(iv,
				(char *)(pce_dev->ce_sps.result->encr_cntr_iv),
				sizeof(iv));
		}
		pce_dev->qce_cb(areq, NULL, iv, result_status);
	}
	return 0;
};

static int _f8_complete(struct qce_device *pce_dev)
{
	uint32_t status;
	int32_t result_status;

	if (pce_dev->phy_ota_dst != 0)
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_dst,
				pce_dev->ota_size, DMA_FROM_DEVICE);
	if (pce_dev->phy_ota_src != 0)
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_src,
				pce_dev->ota_size, (pce_dev->phy_ota_dst) ?
				DMA_TO_DEVICE : DMA_BIDIRECTIONAL);
	/* read status before unlock */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if (_qce_unlock_other_pipes(pce_dev)) {
		pce_dev->qce_cb(pce_dev->areq, NULL, NULL, -ENXIO);
		return -ENXIO;
	}
	if (status & ((1 << CRYPTO_SW_ERR) | (1 << CRYPTO_AXI_ERR)
				| (1 <<  CRYPTO_HSD_ERR))) {
		pr_err("f8 operation error. Status %x\n", status);
		result_status = -ENXIO;
	} else if (pce_dev->ce_sps.consumer_status |
				pce_dev->ce_sps.producer_status)  {
		pr_err("f8 sps operation error. sps status %x %x\n",
				pce_dev->ce_sps.consumer_status,
				pce_dev->ce_sps.producer_status);
		result_status = -ENXIO;
	} else if ((status & (1 << CRYPTO_OPERATION_DONE)) == 0) {
		pr_err("f8 operation not done? Status %x, sps status %x %x\n",
			status,
			pce_dev->ce_sps.consumer_status,
			pce_dev->ce_sps.producer_status);
		result_status = -ENXIO;
	} else {
		result_status = 0;
	}
	pce_dev->qce_cb(pce_dev->areq, NULL, NULL, result_status);
	return 0;
}

static void _qce_sps_iovec_count_init(struct qce_device *pce_dev)
{
	pce_dev->ce_sps.in_transfer.iovec_count = 0;
	pce_dev->ce_sps.out_transfer.iovec_count = 0;
}

static void _qce_set_flag(struct sps_transfer *sps_bam_pipe, uint32_t flag)
{
	struct sps_iovec *iovec;

	if (sps_bam_pipe->iovec_count == 0)
		return;
	iovec  = sps_bam_pipe->iovec + (sps_bam_pipe->iovec_count - 1);
	iovec->flags |= flag;
}

static int _qce_sps_add_data(uint32_t addr, uint32_t len,
		struct sps_transfer *sps_bam_pipe)
{
	struct sps_iovec *iovec = sps_bam_pipe->iovec +
					sps_bam_pipe->iovec_count;
	uint32_t data_cnt;

	while (len > 0) {
		if (sps_bam_pipe->iovec_count == QCE_MAX_NUM_DSCR) {
			pr_err("Num of descrptor %d exceed max (%d)",
				sps_bam_pipe->iovec_count,
				(uint32_t)QCE_MAX_NUM_DSCR);
			return -ENOMEM;
		}
		if (len > SPS_MAX_PKT_SIZE)
			data_cnt = SPS_MAX_PKT_SIZE;
		else
			data_cnt = len;
		iovec->size = data_cnt;
		iovec->addr = addr;
		iovec->flags = 0;
		sps_bam_pipe->iovec_count++;
		iovec++;
		addr += data_cnt;
		len -= data_cnt;
	}
	return 0;
}

static int _qce_sps_add_sg_data(struct qce_device *pce_dev,
		struct scatterlist *sg_src, uint32_t nbytes,
		struct sps_transfer *sps_bam_pipe)
{
	uint32_t addr, data_cnt, len;
	struct sps_iovec *iovec = sps_bam_pipe->iovec +
						sps_bam_pipe->iovec_count;

	while (nbytes > 0) {
		len = min(nbytes, sg_dma_len(sg_src));
		nbytes -= len;
		addr = sg_dma_address(sg_src);
		if (pce_dev->ce_sps.minor_version == 0)
			len = ALIGN(len, pce_dev->ce_sps.ce_burst_size);
		while (len > 0) {
			if (sps_bam_pipe->iovec_count == QCE_MAX_NUM_DSCR) {
				pr_err("Num of descrptor %d exceed max (%d)",
						sps_bam_pipe->iovec_count,
						(uint32_t)QCE_MAX_NUM_DSCR);
				return -ENOMEM;
			}
			if (len > SPS_MAX_PKT_SIZE) {
				data_cnt = SPS_MAX_PKT_SIZE;
				iovec->size = data_cnt;
				iovec->addr = addr;
				iovec->flags = 0;
			} else {
				data_cnt = len;
				iovec->size = data_cnt;
				iovec->addr = addr;
				iovec->flags = 0;
			}
			iovec++;
			sps_bam_pipe->iovec_count++;
			addr += data_cnt;
			len -= data_cnt;
		}
		sg_src = scatterwalk_sg_next(sg_src);
	}
	return 0;
}

static int _qce_sps_add_cmd(struct qce_device *pce_dev, uint32_t flag,
				struct qce_cmdlist_info *cmdptr,
				struct sps_transfer *sps_bam_pipe)
{
	struct sps_iovec *iovec = sps_bam_pipe->iovec +
					sps_bam_pipe->iovec_count;
	iovec->size = cmdptr->size;
	iovec->addr = GET_PHYS_ADDR(cmdptr->cmdlist);
	iovec->flags = SPS_IOVEC_FLAG_CMD | flag;
	sps_bam_pipe->iovec_count++;

	return 0;
}

static int _qce_sps_transfer(struct qce_device *pce_dev)
{
	int rc = 0;

	_qce_dump_descr_fifos_dbg(pce_dev);
	if (pce_dev->ce_sps.in_transfer.iovec_count) {
		rc = sps_transfer(pce_dev->ce_sps.consumer.pipe,
					  &pce_dev->ce_sps.in_transfer);
		if (rc) {
			pr_err("sps_xfr() fail (consumer pipe=0x%lx) rc = %d\n",
				(uintptr_t)pce_dev->ce_sps.consumer.pipe, rc);
			_qce_dump_descr_fifos(pce_dev);
			return rc;
		}
	}
	rc = sps_transfer(pce_dev->ce_sps.producer.pipe,
					  &pce_dev->ce_sps.out_transfer);
	if (rc) {
		pr_err("sps_xfr() fail (producer pipe=0x%lx) rc = %d\n",
				(uintptr_t)pce_dev->ce_sps.producer.pipe, rc);
		return rc;
	}
	return rc;
}

/**
 * Allocate and Connect a CE peripheral's SPS endpoint
 *
 * This function allocates endpoint context and
 * connect it with memory endpoint by calling
 * appropriate SPS driver APIs.
 *
 * Also registers a SPS callback function with
 * SPS driver
 *
 * This function should only be called once typically
 * during driver probe.
 *
 * @pce_dev - Pointer to qce_device structure
 * @ep   - Pointer to sps endpoint data structure
 * @is_produce - 1 means Producer endpoint
 *		 0 means Consumer endpoint
 *
 * @return - 0 if successful else negative value.
 *
 */
static int qce_sps_init_ep_conn(struct qce_device *pce_dev,
				struct qce_sps_ep_conn_data *ep,
				bool is_producer)
{
	int rc = 0;
	struct sps_pipe *sps_pipe_info;
	struct sps_connect *sps_connect_info = &ep->connect;
	struct sps_register_event *sps_event = &ep->event;

	/* Allocate endpoint context */
	sps_pipe_info = sps_alloc_endpoint();
	if (!sps_pipe_info) {
		pr_err("sps_alloc_endpoint() failed!!! is_producer=%d",
			   is_producer);
		rc = -ENOMEM;
		goto out;
	}
	/* Now save the sps pipe handle */
	ep->pipe = sps_pipe_info;

	/* Get default connection configuration for an endpoint */
	rc = sps_get_config(sps_pipe_info, sps_connect_info);
	if (rc) {
		pr_err("sps_get_config() fail pipe_handle=0x%lx, rc = %d\n",
				(uintptr_t)sps_pipe_info, rc);
		goto get_config_err;
	}

	/* Modify the default connection configuration */
	if (is_producer) {
		/*
		* For CE producer transfer, source should be
		* CE peripheral where as destination should
		* be system memory.
		*/
		sps_connect_info->source = pce_dev->ce_sps.bam_handle;
		sps_connect_info->destination = SPS_DEV_HANDLE_MEM;
		/* Producer pipe will handle this connection */
		sps_connect_info->mode = SPS_MODE_SRC;
		sps_connect_info->options =
			SPS_O_AUTO_ENABLE | SPS_O_DESC_DONE;
	} else {
		/* For CE consumer transfer, source should be
		 * system memory where as destination should
		 * CE peripheral
		 */
		sps_connect_info->source = SPS_DEV_HANDLE_MEM;
		sps_connect_info->destination = pce_dev->ce_sps.bam_handle;
		sps_connect_info->mode = SPS_MODE_DEST;
		sps_connect_info->options =
			SPS_O_AUTO_ENABLE | SPS_O_EOT;
	}

	/* Producer pipe index */
	sps_connect_info->src_pipe_index = pce_dev->ce_sps.src_pipe_index;
	/* Consumer pipe index */
	sps_connect_info->dest_pipe_index = pce_dev->ce_sps.dest_pipe_index;
	/* Set pipe group */
	sps_connect_info->lock_group = pce_dev->ce_sps.pipe_pair_index;
	sps_connect_info->event_thresh = 0x10;
	/*
	 * Max. no of scatter/gather buffers that can
	 * be passed by block layer = 32 (NR_SG).
	 * Each BAM descritor needs 64 bits (8 bytes).
	 * One BAM descriptor is required per buffer transfer.
	 * So we would require total 256 (32 * 8) bytes of descriptor FIFO.
	 * But due to HW limitation we need to allocate atleast one extra
	 * descriptor memory (256 bytes + 8 bytes). But in order to be
	 * in power of 2, we are allocating 512 bytes of memory.
	 */
	sps_connect_info->desc.size = QCE_MAX_NUM_DSCR *
					sizeof(struct sps_iovec);
	sps_connect_info->desc.base = dma_alloc_coherent(pce_dev->pdev,
					sps_connect_info->desc.size,
					&sps_connect_info->desc.phys_base,
					GFP_KERNEL);
	if (sps_connect_info->desc.base == NULL) {
		rc = -ENOMEM;
		pr_err("Can not allocate coherent memory for sps data\n");
		goto get_config_err;
	}

	memset(sps_connect_info->desc.base, 0x00, sps_connect_info->desc.size);

	/* Establish connection between peripheral and memory endpoint */
	rc = sps_connect(sps_pipe_info, sps_connect_info);
	if (rc) {
		pr_err("sps_connect() fail pipe_handle=0x%lx, rc = %d\n",
				(uintptr_t)sps_pipe_info, rc);
		goto sps_connect_err;
	}

	sps_event->mode = SPS_TRIGGER_CALLBACK;
	if (is_producer)
		sps_event->options = SPS_O_EOT | SPS_O_DESC_DONE;
	else
		sps_event->options = SPS_O_EOT;
	sps_event->xfer_done = NULL;
	sps_event->user = (void *)pce_dev;

	pr_debug("success, %s : pipe_handle=0x%lx, desc fifo base (phy) = 0x%p\n",
		is_producer ? "PRODUCER(RX/OUT)" : "CONSUMER(TX/IN)",
		(uintptr_t)sps_pipe_info, &sps_connect_info->desc.phys_base);
	goto out;

sps_connect_err:
	dma_free_coherent(pce_dev->pdev,
			sps_connect_info->desc.size,
			sps_connect_info->desc.base,
			sps_connect_info->desc.phys_base);
get_config_err:
	sps_free_endpoint(sps_pipe_info);
out:
	return rc;
}

/**
 * Disconnect and Deallocate a CE peripheral's SPS endpoint
 *
 * This function disconnect endpoint and deallocates
 * endpoint context.
 *
 * This function should only be called once typically
 * during driver remove.
 *
 * @pce_dev - Pointer to qce_device structure
 * @ep   - Pointer to sps endpoint data structure
 *
 */
static void qce_sps_exit_ep_conn(struct qce_device *pce_dev,
				struct qce_sps_ep_conn_data *ep)
{
	struct sps_pipe *sps_pipe_info = ep->pipe;
	struct sps_connect *sps_connect_info = &ep->connect;

	sps_disconnect(sps_pipe_info);
	dma_free_coherent(pce_dev->pdev,
			sps_connect_info->desc.size,
			sps_connect_info->desc.base,
			sps_connect_info->desc.phys_base);
	sps_free_endpoint(sps_pipe_info);
}

static void qce_sps_release_bam(struct qce_device *pce_dev)
{
	struct bam_registration_info *pbam;

	mutex_lock(&bam_register_lock);
	pbam = pce_dev->pbam;
	if (pbam == NULL)
		goto ret;

	pbam->cnt--;
	if (pbam->cnt > 0)
		goto ret;

	if (pce_dev->ce_sps.bam_handle) {
		sps_deregister_bam_device(pce_dev->ce_sps.bam_handle);

		pr_debug("deregister bam handle 0x%lx\n",
					pce_dev->ce_sps.bam_handle);
		pce_dev->ce_sps.bam_handle = 0;
	}
	iounmap(pbam->bam_iobase);
	pr_debug("delete bam 0x%x\n", pbam->bam_mem);
	list_del(&pbam->qlist);
	kfree(pbam);

ret:
	pce_dev->pbam = NULL;
	mutex_unlock(&bam_register_lock);
}

static int qce_sps_get_bam(struct qce_device *pce_dev)
{
	int rc = 0;
	struct sps_bam_props bam = {0};
	struct bam_registration_info *pbam = NULL;
	struct bam_registration_info *p;
	uint32_t bam_cfg = 0;


	mutex_lock(&bam_register_lock);

	list_for_each_entry(p, &qce50_bam_list, qlist) {
		if (p->bam_mem == pce_dev->bam_mem) {
			pbam = p;  /* found */
			break;
		}
	}

	if (pbam) {
		pr_debug("found bam 0x%x\n", pbam->bam_mem);
		pbam->cnt++;
		pce_dev->ce_sps.bam_handle =  pbam->handle;
		pce_dev->ce_sps.bam_mem = pbam->bam_mem;
		pce_dev->ce_sps.bam_iobase = pbam->bam_iobase;
		pce_dev->pbam = pbam;
		pce_dev->support_cmd_dscr = pbam->support_cmd_dscr;
		goto ret;
	}

	pbam = kzalloc(sizeof(struct  bam_registration_info), GFP_KERNEL);
	if (!pbam) {
		pr_err("qce50 Memory allocation of bam FAIL, error %ld\n",
						PTR_ERR(pbam));

		rc = -ENOMEM;
		goto ret;
	}
	pbam->cnt = 1;
	pbam->bam_mem = pce_dev->bam_mem;
	pbam->bam_iobase = ioremap_nocache(pce_dev->bam_mem,
					pce_dev->bam_mem_size);
	if (!pbam->bam_iobase) {
		kfree(pbam);
		rc = -ENOMEM;
		pr_err("Can not map BAM io memory\n");
		goto ret;
	}
	pce_dev->ce_sps.bam_mem = pbam->bam_mem;
	pce_dev->ce_sps.bam_iobase = pbam->bam_iobase;
	pbam->handle = 0;
	pr_debug("allocate bam 0x%x\n", pbam->bam_mem);
	bam_cfg = readl_relaxed(pce_dev->ce_sps.bam_iobase +
					CRYPTO_BAM_CNFG_BITS_REG);
	pbam->support_cmd_dscr =  (bam_cfg & CRYPTO_BAM_CD_ENABLE_MASK) ?
					true : false;
	if (pbam->support_cmd_dscr == false) {
		pr_info("qce50 don't support command descriptor. bam_cfg%x\n",
								 bam_cfg);
	}
	pce_dev->support_cmd_dscr = pbam->support_cmd_dscr;

	bam.phys_addr = pce_dev->ce_sps.bam_mem;
	bam.virt_addr = pce_dev->ce_sps.bam_iobase;

	/*
	 * This event thresold value is only significant for BAM-to-BAM
	 * transfer. It's ignored for BAM-to-System mode transfer.
	 */
	bam.event_threshold = 0x10;	/* Pipe event threshold */
	/*
	 * This threshold controls when the BAM publish
	 * the descriptor size on the sideband interface.
	 * SPS HW will only be used when
	 * data transfer size >  64 bytes.
	 */
	bam.summing_threshold = 64;
	/* SPS driver wll handle the crypto BAM IRQ */
	bam.irq = (u32)pce_dev->ce_sps.bam_irq;
	/*
	 * Set flag to indicate BAM global device control is managed
	 * remotely.
	 */
	if ((pce_dev->support_cmd_dscr == false) || (pce_dev->is_shared))
		bam.manage = SPS_BAM_MGR_DEVICE_REMOTE;
	else
		bam.manage = SPS_BAM_MGR_LOCAL;

	bam.ee = 1;

	pr_debug("bam physical base=0x%lx\n", (uintptr_t)bam.phys_addr);
	pr_debug("bam virtual base=0x%p\n", bam.virt_addr);

	/* Register CE Peripheral BAM device to SPS driver */
	rc = sps_register_bam_device(&bam, &pbam->handle);
	if (rc) {
		pr_err("sps_register_bam_device() failed! err=%d", rc);
		rc = -EIO;
		iounmap(pbam->bam_iobase);
		kfree(pbam);
		goto ret;
	}

	pce_dev->pbam = pbam;
	list_add_tail(&pbam->qlist, &qce50_bam_list);
	pce_dev->ce_sps.bam_handle =  pbam->handle;

ret:
	mutex_unlock(&bam_register_lock);

	return rc;
}
/**
 * Initialize SPS HW connected with CE core
 *
 * This function register BAM HW resources with
 * SPS driver and then initialize 2 SPS endpoints
 *
 * This function should only be called once typically
 * during driver probe.
 *
 * @pce_dev - Pointer to qce_device structure
 *
 * @return - 0 if successful else negative value.
 *
 */
static int qce_sps_init(struct qce_device *pce_dev)
{
	int rc = 0;

	rc = qce_sps_get_bam(pce_dev);
	if (rc)
		return rc;
	pr_debug("BAM device registered. bam_handle=0x%lx\n",
		pce_dev->ce_sps.bam_handle);

	rc = qce_sps_init_ep_conn(pce_dev, &pce_dev->ce_sps.producer, true);
	if (rc)
		goto sps_connect_producer_err;
	rc = qce_sps_init_ep_conn(pce_dev, &pce_dev->ce_sps.consumer, false);
	if (rc)
		goto sps_connect_consumer_err;

	pce_dev->ce_sps.out_transfer.user = pce_dev->ce_sps.producer.pipe;
	pce_dev->ce_sps.in_transfer.user = pce_dev->ce_sps.consumer.pipe;
	pr_info(" Qualcomm MSM CE-BAM at 0x%016llx irq %d\n",
		(unsigned long long)pce_dev->ce_sps.bam_mem,
		(unsigned int)pce_dev->ce_sps.bam_irq);
	return rc;

sps_connect_consumer_err:
	qce_sps_exit_ep_conn(pce_dev, &pce_dev->ce_sps.producer);
sps_connect_producer_err:
	qce_sps_release_bam(pce_dev);
	return rc;
}

/**
 * De-initialize SPS HW connected with CE core
 *
 * This function deinitialize SPS endpoints and then
 * deregisters BAM resources from SPS driver.
 *
 * This function should only be called once typically
 * during driver remove.
 *
 * @pce_dev - Pointer to qce_device structure
 *
 */
static void qce_sps_exit(struct qce_device *pce_dev)
{
	qce_sps_exit_ep_conn(pce_dev, &pce_dev->ce_sps.consumer);
	qce_sps_exit_ep_conn(pce_dev, &pce_dev->ce_sps.producer);
	qce_sps_release_bam(pce_dev);
}

static void _aead_sps_producer_callback(struct sps_event_notify *notify)
{
	struct qce_device *pce_dev = (struct qce_device *)
		((struct sps_event_notify *)notify)->user;

	pce_dev->ce_sps.notify = *notify;
	pr_debug("sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
			notify->event_id,
			notify->data.transfer.iovec.addr,
			notify->data.transfer.iovec.size,
			notify->data.transfer.iovec.flags);

	if (pce_dev->ce_sps.producer_state == QCE_PIPE_STATE_COMP) {
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
		/* done */
		_aead_complete(pce_dev);
	} else {
		int rc = 0;
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		pce_dev->ce_sps.out_transfer.iovec_count = 0;
		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer);
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
				SPS_IOVEC_FLAG_INT);
		rc = sps_transfer(pce_dev->ce_sps.producer.pipe,
					  &pce_dev->ce_sps.out_transfer);
		if (rc) {
			pr_err("sps_xfr() fail (producer pipe=0x%lx) rc = %d\n",
				(uintptr_t)pce_dev->ce_sps.producer.pipe, rc);
		}
	}
};

static void _sha_sps_producer_callback(struct sps_event_notify *notify)
{
	struct qce_device *pce_dev = (struct qce_device *)
		((struct sps_event_notify *)notify)->user;

	pce_dev->ce_sps.notify = *notify;
	pr_debug("sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
			notify->event_id,
			notify->data.transfer.iovec.addr,
			notify->data.transfer.iovec.size,
			notify->data.transfer.iovec.flags);
	/* done */
	_sha_complete(pce_dev);
};

static void _f9_sps_producer_callback(struct sps_event_notify *notify)
{
	struct qce_device *pce_dev = (struct qce_device *)
		((struct sps_event_notify *)notify)->user;

	pce_dev->ce_sps.notify = *notify;
	pr_debug("sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
			notify->event_id,
			notify->data.transfer.iovec.addr,
			notify->data.transfer.iovec.size,
			notify->data.transfer.iovec.flags);
	/* done */
	_f9_complete(pce_dev);
}

static void _f8_sps_producer_callback(struct sps_event_notify *notify)
{
	struct qce_device *pce_dev = (struct qce_device *)
		((struct sps_event_notify *)notify)->user;

	pce_dev->ce_sps.notify = *notify;
	pr_debug("sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
			notify->event_id,
			notify->data.transfer.iovec.addr,
			notify->data.transfer.iovec.size,
			notify->data.transfer.iovec.flags);

	if (pce_dev->ce_sps.producer_state == QCE_PIPE_STATE_COMP) {
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
		/* done */
		_f8_complete(pce_dev);
	} else {
		int rc = 0;
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		pce_dev->ce_sps.out_transfer.iovec_count = 0;
		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer);
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_INT);
		rc = sps_transfer(pce_dev->ce_sps.producer.pipe,
					  &pce_dev->ce_sps.out_transfer);
		if (rc) {
			pr_err("sps_xfr() fail (producer pipe=0x%lx) rc = %d\n",
				(uintptr_t)pce_dev->ce_sps.producer.pipe, rc);
		}
	}
}

static void _ablk_cipher_sps_producer_callback(struct sps_event_notify *notify)
{
	struct qce_device *pce_dev = (struct qce_device *)
		((struct sps_event_notify *)notify)->user;

	pce_dev->ce_sps.notify = *notify;
	pr_debug("sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
			notify->event_id,
			notify->data.transfer.iovec.addr,
			notify->data.transfer.iovec.size,
			notify->data.transfer.iovec.flags);

	if (pce_dev->ce_sps.producer_state == QCE_PIPE_STATE_COMP) {
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
		/* done */
		_ablk_cipher_complete(pce_dev);
	} else {
		int rc = 0;
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		pce_dev->ce_sps.out_transfer.iovec_count = 0;
		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer);
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
				SPS_IOVEC_FLAG_INT);
		rc = sps_transfer(pce_dev->ce_sps.producer.pipe,
					  &pce_dev->ce_sps.out_transfer);
		if (rc) {
			pr_err("sps_xfr() fail (producer pipe=0x%lx) rc = %d\n",
				(uintptr_t)pce_dev->ce_sps.producer.pipe, rc);
		}
	}
};

static void qce_add_cmd_element(struct qce_device *pdev,
			struct sps_command_element **cmd_ptr, u32 addr,
			u32 data, struct sps_command_element **populate)
{
	(*cmd_ptr)->addr = (uint32_t)(addr + pdev->phy_iobase);
	(*cmd_ptr)->command = 0;
	(*cmd_ptr)->data = data;
	(*cmd_ptr)->mask = 0xFFFFFFFF;
	(*cmd_ptr)->reserved = 0;
	if (populate != NULL)
		*populate = *cmd_ptr;
	(*cmd_ptr)++;
}

static int _setup_cipher_aes_cmdlistptrs(struct qce_device *pdev,
		unsigned char **pvaddr, enum qce_cipher_mode_enum mode,
		bool key_128)
{
	struct sps_command_element *ce_vaddr;
	uintptr_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t encr_cfg = 0;
	uint32_t key_reg = 0;
	uint32_t xts_key_reg = 0;
	uint32_t iv_reg = 0;

	*pvaddr = (unsigned char *)ALIGN(((uintptr_t)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	ce_vaddr_start = (uintptr_t)(*pvaddr);
	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to AES cipher operations defined
	 * in ce_cmdlistptrs_ops structure.
	 */
	switch (mode) {
	case QCE_MODE_CBC:
	case QCE_MODE_CTR:
		if (key_128 == true) {
			cmdlistptr->cipher_aes_128_cbc_ctr.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_128_cbc_ctr);
			if (mode == QCE_MODE_CBC)
				encr_cfg = pdev->reg.encr_cfg_aes_cbc_128;
			else
				encr_cfg = pdev->reg.encr_cfg_aes_ctr_128;
			iv_reg = 4;
			key_reg = 4;
			xts_key_reg = 0;
		} else {
			cmdlistptr->cipher_aes_256_cbc_ctr.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_256_cbc_ctr);

			if (mode == QCE_MODE_CBC)
				encr_cfg = pdev->reg.encr_cfg_aes_cbc_256;
			else
				encr_cfg = pdev->reg.encr_cfg_aes_ctr_256;
			iv_reg = 4;
			key_reg = 8;
			xts_key_reg = 0;
		}
	break;
	case QCE_MODE_ECB:
		if (key_128 == true) {
			cmdlistptr->cipher_aes_128_ecb.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_128_ecb);

			encr_cfg = pdev->reg.encr_cfg_aes_ecb_128;
			iv_reg = 0;
			key_reg = 4;
			xts_key_reg = 0;
		} else {
			cmdlistptr->cipher_aes_256_ecb.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_256_ecb);

			encr_cfg = pdev->reg.encr_cfg_aes_ecb_256;
			iv_reg = 0;
			key_reg = 8;
			xts_key_reg = 0;
		}
	break;
	case QCE_MODE_XTS:
		if (key_128 == true) {
			cmdlistptr->cipher_aes_128_xts.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_128_xts);

			encr_cfg = pdev->reg.encr_cfg_aes_xts_128;
			iv_reg = 4;
			key_reg = 4;
			xts_key_reg = 4;
		} else {
			cmdlistptr->cipher_aes_256_xts.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_256_xts);

			encr_cfg = pdev->reg.encr_cfg_aes_xts_256;
			iv_reg = 4;
			key_reg = 8;
			xts_key_reg = 8;
		}
	break;
	default:
		pr_err("Unknown mode of operation %d received, exiting now\n",
			mode);
		return -EINVAL;
	break;
	}

	/* clear status register */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG, 0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_SEG_SIZE_REG, 0,
						&pcl_info->seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_CFG_REG, encr_cfg,
						&pcl_info->encr_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG, 0,
						&pcl_info->encr_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_START_REG, 0,
						&pcl_info->encr_seg_start);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR_MASK_REG,
				(uint32_t)0xffffffff, &pcl_info->encr_mask);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR_MASK_REG0,
				(uint32_t)0xffffffff, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR_MASK_REG1,
				(uint32_t)0xffffffff, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR_MASK_REG2,
				(uint32_t)0xffffffff, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG, 0,
						&pcl_info->auth_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_KEY0_REG, 0,
						&pcl_info->encr_key);
	for (i = 1; i < key_reg; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t)),
				0, NULL);
	if (xts_key_reg) {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_XTS_KEY0_REG,
					0, &pcl_info->encr_xts_key);
		for (i = 1; i < xts_key_reg; i++)
			qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_ENCR_XTS_KEY0_REG +
						i * sizeof(uint32_t)), 0, NULL);
		qce_add_cmd_element(pdev, &ce_vaddr,
				CRYPTO_ENCR_XTS_DU_SIZE_REG, 0,
					&pcl_info->encr_xts_du_size);
	}
	if (iv_reg) {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR0_IV0_REG, 0,
						&pcl_info->encr_cntr_iv);
		for (i = 1; i < iv_reg; i++)
			qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_CNTR0_IV0_REG + i * sizeof(uint32_t)),
				0, NULL);
	}
	/* Add dummy to  align size to burst-size multiple */
	if (mode == QCE_MODE_XTS) {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG,
						0, &pcl_info->auth_seg_size);
	} else {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG,
						0, &pcl_info->auth_seg_size);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_START_REG,
						0, &pcl_info->auth_seg_size);
	}
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_le, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uintptr_t)ce_vaddr - (uintptr_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_cipher_des_cmdlistptrs(struct qce_device *pdev,
		unsigned char **pvaddr, enum qce_cipher_alg_enum alg,
		bool mode_cbc)
{

	struct sps_command_element *ce_vaddr;
	uintptr_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t encr_cfg = 0;
	uint32_t key_reg = 0;
	uint32_t iv_reg = 0;

	*pvaddr = (unsigned char *)ALIGN(((uintptr_t)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	ce_vaddr_start = (uintptr_t)(*pvaddr);

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to cipher operations defined
	 * in ce_cmdlistptrs_ops structure.
	 */
	switch (alg) {
	case CIPHER_ALG_DES:
		if (mode_cbc) {
			cmdlistptr->cipher_des_cbc.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_des_cbc);


			encr_cfg = pdev->reg.encr_cfg_des_cbc;
			iv_reg = 2;
			key_reg = 2;
		} else {
			cmdlistptr->cipher_des_ecb.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_des_ecb);

			encr_cfg = pdev->reg.encr_cfg_des_ecb;
			iv_reg = 0;
			key_reg = 2;
		}
	break;
	case CIPHER_ALG_3DES:
		if (mode_cbc) {
			cmdlistptr->cipher_3des_cbc.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_3des_cbc);

			encr_cfg = pdev->reg.encr_cfg_3des_cbc;
			iv_reg = 2;
			key_reg = 6;
		} else {
			cmdlistptr->cipher_3des_ecb.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_3des_ecb);

			encr_cfg = pdev->reg.encr_cfg_3des_ecb;
			iv_reg = 0;
			key_reg = 6;
		}
	break;
	default:
		pr_err("Unknown algorithms %d received, exiting now\n", alg);
		return -EINVAL;
	break;
	}

	/* clear status register */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG, 0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_SEG_SIZE_REG, 0,
						&pcl_info->seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_CFG_REG, encr_cfg,
						&pcl_info->encr_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG, 0,
						&pcl_info->encr_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_START_REG, 0,
						&pcl_info->encr_seg_start);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG, 0,
						&pcl_info->auth_seg_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_KEY0_REG, 0,
						&pcl_info->encr_key);
	for (i = 1; i < key_reg; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t)),
				0, NULL);
	if (iv_reg) {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR0_IV0_REG, 0,
						&pcl_info->encr_cntr_iv);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR1_IV1_REG, 0,
								NULL);
	}

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_le, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uintptr_t)ce_vaddr - (uintptr_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_auth_cmdlistptrs(struct qce_device *pdev,
		unsigned char **pvaddr, enum qce_hash_alg_enum alg,
		bool key_128)
{
	struct sps_command_element *ce_vaddr;
	uintptr_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t key_reg = 0;
	uint32_t auth_cfg = 0;
	uint32_t iv_reg = 0;

	*pvaddr = (unsigned char *)ALIGN(((uintptr_t)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr_start = (uintptr_t)(*pvaddr);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to authentication operations
	 * defined in ce_cmdlistptrs_ops structure.
	 */
	switch (alg) {
	case QCE_HASH_SHA1:
		cmdlistptr->auth_sha1.cmdlist = (uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->auth_sha1);

		auth_cfg = pdev->reg.auth_cfg_sha1;
		iv_reg = 5;

		/* clear status register */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG,
					0, NULL);

		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);

	break;
	case QCE_HASH_SHA256:
		cmdlistptr->auth_sha256.cmdlist = (uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->auth_sha256);

		auth_cfg = pdev->reg.auth_cfg_sha256;
		iv_reg = 8;

		/* clear status register */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG,
					0, NULL);

		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);
		/* 1 dummy write */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG,
								0, NULL);
	break;
	case QCE_HASH_SHA1_HMAC:
		cmdlistptr->auth_sha1_hmac.cmdlist = (uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->auth_sha1_hmac);

		auth_cfg = pdev->reg.auth_cfg_hmac_sha1;
		key_reg = 16;
		iv_reg = 5;

		/* clear status register */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG,
					0, NULL);

		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);
	break;
	case QCE_HASH_SHA256_HMAC:
		cmdlistptr->auth_sha256_hmac.cmdlist = (uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->auth_sha256_hmac);

		auth_cfg = pdev->reg.auth_cfg_hmac_sha256;
		key_reg = 16;
		iv_reg = 8;

		/* clear status register */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG, 0,
					NULL);

		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);
		/* 1 dummy write */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG,
								0, NULL);
	break;
	case QCE_HASH_AES_CMAC:
		if (key_128 == true) {
			cmdlistptr->auth_aes_128_cmac.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->auth_aes_128_cmac);

			auth_cfg = pdev->reg.auth_cfg_cmac_128;
			key_reg = 4;
		} else {
			cmdlistptr->auth_aes_256_cmac.cmdlist =
						(uintptr_t)ce_vaddr;
			pcl_info = &(cmdlistptr->auth_aes_256_cmac);

			auth_cfg = pdev->reg.auth_cfg_cmac_256;
			key_reg = 8;
		}

		/* clear status register */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG, 0,
					NULL);

		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);
		/* 1 dummy write */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG,
								0, NULL);
	break;
	default:
		pr_err("Unknown algorithms %d received, exiting now\n", alg);
		return -EINVAL;
	break;
	}

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_SEG_SIZE_REG, 0,
						&pcl_info->seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_CFG_REG, 0,
						&pcl_info->encr_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
					auth_cfg, &pcl_info->auth_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG, 0,
						&pcl_info->auth_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_START_REG, 0,
						&pcl_info->auth_seg_start);

	if (alg == QCE_HASH_AES_CMAC) {
		/* reset auth iv, bytecount and key  registers */
		for (i = 0; i < 16; i++)
			qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_IV0_REG + i * sizeof(uint32_t)),
				0, NULL);
		for (i = 0; i < 16; i++)
			qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_KEY0_REG + i*sizeof(uint32_t)),
				0, NULL);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT0_REG,
						0, NULL);
	} else {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_IV0_REG, 0,
							&pcl_info->auth_iv);
		for (i = 1; i < iv_reg; i++)
			qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t)),
				0, NULL);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT0_REG,
						0, &pcl_info->auth_bytecount);
	}
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT1_REG, 0, NULL);

	if (key_reg) {
		qce_add_cmd_element(pdev, &ce_vaddr,
				CRYPTO_AUTH_KEY0_REG, 0, &pcl_info->auth_key);
		for (i = 1; i < key_reg; i++)
			qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_KEY0_REG + i*sizeof(uint32_t)),
				0, NULL);
	}
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					pdev->reg.crypto_cfg_le, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uintptr_t)ce_vaddr - (uintptr_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_aead_cmdlistptrs(struct qce_device *pdev,
				unsigned char **pvaddr,
				uint32_t alg,
				uint32_t mode,
				uint32_t key_size,
				bool     sha1)
{
	struct sps_command_element *ce_vaddr;
	uintptr_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	uint32_t key_reg;
	uint32_t iv_reg;
	uint32_t i;
	uint32_t  enciv_in_word;
	uint32_t encr_cfg;

	*pvaddr = (unsigned char *)ALIGN(((uintptr_t)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);

	ce_vaddr_start = (uintptr_t)(*pvaddr);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);

	switch (alg) {

	case CIPHER_ALG_DES:

		switch (mode) {

		case QCE_MODE_CBC:
			if (sha1) {
				cmdlistptr->aead_hmac_sha1_cbc_des.cmdlist =
					(uintptr_t)ce_vaddr;
				pcl_info = &(cmdlistptr->
					aead_hmac_sha1_cbc_des);
			} else {
				cmdlistptr->aead_hmac_sha256_cbc_des.cmdlist =
					(uintptr_t)ce_vaddr;
				pcl_info = &(cmdlistptr->
					aead_hmac_sha256_cbc_des);
			}
			encr_cfg = pdev->reg.encr_cfg_des_cbc;
			break;
		default:
			return -EINVAL;
		};

		enciv_in_word = 2;

		break;

	case CIPHER_ALG_3DES:
		switch (mode) {

		case QCE_MODE_CBC:
			if (sha1) {
				cmdlistptr->aead_hmac_sha1_cbc_3des.cmdlist =
					(uintptr_t)ce_vaddr;
				pcl_info = &(cmdlistptr->
					aead_hmac_sha1_cbc_3des);
			} else {
				cmdlistptr->aead_hmac_sha256_cbc_3des.cmdlist =
					(uintptr_t)ce_vaddr;
				pcl_info = &(cmdlistptr->
					aead_hmac_sha256_cbc_3des);
			}
			encr_cfg = pdev->reg.encr_cfg_3des_cbc;
			break;
		default:
			return -EINVAL;
		};

		enciv_in_word = 2;

		break;

	case CIPHER_ALG_AES:
		switch (mode) {

		case QCE_MODE_CBC:
			if (key_size ==  AES128_KEY_SIZE) {
				if (sha1) {
					cmdlistptr->
						aead_hmac_sha1_cbc_aes_128.
						cmdlist = (uintptr_t)ce_vaddr;
					pcl_info = &(cmdlistptr->
						aead_hmac_sha1_cbc_aes_128);
				} else {
					cmdlistptr->
						aead_hmac_sha256_cbc_aes_128.
						cmdlist = (uintptr_t)ce_vaddr;
					pcl_info = &(cmdlistptr->
						aead_hmac_sha256_cbc_aes_128);
				}
				encr_cfg = pdev->reg.encr_cfg_aes_cbc_128;
			} else if (key_size ==  AES256_KEY_SIZE) {
				if (sha1) {
					cmdlistptr->
						aead_hmac_sha1_cbc_aes_256.
						cmdlist = (uintptr_t)ce_vaddr;
					pcl_info = &(cmdlistptr->
						aead_hmac_sha1_cbc_aes_256);
				} else {
					cmdlistptr->
						aead_hmac_sha256_cbc_aes_256.
						cmdlist = (uintptr_t)ce_vaddr;
					pcl_info = &(cmdlistptr->
						aead_hmac_sha256_cbc_aes_256);
				}
				encr_cfg = pdev->reg.encr_cfg_aes_cbc_256;
			} else {
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		};

		enciv_in_word = 4;

		break;

	default:
		return -EINVAL;
	};


	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG, 0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);


	key_reg = key_size/sizeof(uint32_t);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_KEY0_REG, 0,
			&pcl_info->encr_key);
	for (i = 1; i < key_reg; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
			(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t)),
			0, NULL);

	if (mode != QCE_MODE_ECB) {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR0_IV0_REG, 0,
			&pcl_info->encr_cntr_iv);
		for (i = 1; i < enciv_in_word; i++)
			qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_CNTR0_IV0_REG + i * sizeof(uint32_t)),
				0, NULL);
	};

	if (sha1)
		iv_reg = 5;
	else
		iv_reg = 8;
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_IV0_REG, 0,
				&pcl_info->auth_iv);
	for (i = 1; i < iv_reg; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
			(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t)),
				0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT0_REG,
				0, &pcl_info->auth_bytecount);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT1_REG, 0, NULL);

	key_reg = SHA_HMAC_KEY_SIZE/sizeof(uint32_t);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_KEY0_REG, 0,
			 &pcl_info->auth_key);
	for (i = 1; i < key_reg; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
			(CRYPTO_AUTH_KEY0_REG + i*sizeof(uint32_t)), 0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_SEG_SIZE_REG, 0,
			&pcl_info->seg_size);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_CFG_REG, encr_cfg,
			&pcl_info->encr_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG, 0,
			&pcl_info->encr_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_START_REG, 0,
			&pcl_info->encr_seg_start);

	if (sha1)
		qce_add_cmd_element(
			pdev,
			&ce_vaddr,
			CRYPTO_AUTH_SEG_CFG_REG,
			pdev->reg.auth_cfg_aead_sha1_hmac,
			&pcl_info->auth_seg_cfg);
	else
		qce_add_cmd_element(
			pdev,
			&ce_vaddr,
			CRYPTO_AUTH_SEG_CFG_REG,
			pdev->reg.auth_cfg_aead_sha256_hmac,
			&pcl_info->auth_seg_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG, 0,
			&pcl_info->auth_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_START_REG, 0,
			&pcl_info->auth_seg_start);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					pdev->reg.crypto_cfg_le, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uintptr_t)ce_vaddr - (uintptr_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;
	return 0;
}

static int _setup_aead_ccm_cmdlistptrs(struct qce_device *pdev,
				unsigned char **pvaddr, bool key_128)
{
	struct sps_command_element *ce_vaddr;
	uintptr_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t encr_cfg = 0;
	uint32_t auth_cfg = 0;
	uint32_t key_reg = 0;

	*pvaddr = (unsigned char *)ALIGN(((uintptr_t)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr_start = (uintptr_t)(*pvaddr);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to aead operations
	 * defined in ce_cmdlistptrs_ops structure.
	 */
	if (key_128 == true) {
		cmdlistptr->aead_aes_128_ccm.cmdlist =
						(uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->aead_aes_128_ccm);

		auth_cfg = pdev->reg.auth_cfg_aes_ccm_128;
		encr_cfg = pdev->reg.encr_cfg_aes_ccm_128;
		key_reg = 4;
	} else {

		cmdlistptr->aead_aes_256_ccm.cmdlist =
						(uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->aead_aes_256_ccm);

		auth_cfg = pdev->reg.auth_cfg_aes_ccm_256;
		encr_cfg = pdev->reg.encr_cfg_aes_ccm_256;

		key_reg = 8;
	}

	/* clear status register */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG, 0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_CFG_REG, 0, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_START_REG, 0,
									NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_SEG_SIZE_REG, 0,
						&pcl_info->seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_CFG_REG,
					encr_cfg, &pcl_info->encr_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG, 0,
						&pcl_info->encr_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_START_REG, 0,
						&pcl_info->encr_seg_start);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR_MASK_REG,
				(uint32_t)0xffffffff, &pcl_info->encr_mask);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR_MASK_REG0,
				(uint32_t)0xffffffff, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR_MASK_REG1,
				(uint32_t)0xffffffff, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR_MASK_REG2,
				(uint32_t)0xffffffff, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
					auth_cfg, &pcl_info->auth_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG, 0,
						&pcl_info->auth_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_START_REG, 0,
						&pcl_info->auth_seg_start);
	/* reset auth iv, bytecount and key  registers */
	for (i = 0; i < 8; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_IV0_REG + i * sizeof(uint32_t)),
				0, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT0_REG,
					0, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT1_REG,
					0, NULL);
	for (i = 0; i < 16; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_KEY0_REG + i * sizeof(uint32_t)),
				0, NULL);
	/* set auth key */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_KEY0_REG, 0,
							&pcl_info->auth_key);
	for (i = 1; i < key_reg; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_KEY0_REG + i * sizeof(uint32_t)),
				0, NULL);
	/* set NONCE info */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_INFO_NONCE0_REG, 0,
						&pcl_info->auth_nonce_info);
	for (i = 1; i < 4; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_INFO_NONCE0_REG +
				i * sizeof(uint32_t)), 0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_KEY0_REG, 0,
						&pcl_info->encr_key);
	for (i = 1; i < key_reg; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t)),
				0, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR0_IV0_REG, 0,
						&pcl_info->encr_cntr_iv);
	for (i = 1; i < 4; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_CNTR0_IV0_REG + i * sizeof(uint32_t)),
				0, NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_CCM_INT_CNTR0_REG, 0,
						&pcl_info->encr_ccm_cntr_iv);
	for (i = 1; i < 4; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
			(CRYPTO_ENCR_CCM_INT_CNTR0_REG + i * sizeof(uint32_t)),
			0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					pdev->reg.crypto_cfg_le, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uintptr_t)ce_vaddr - (uintptr_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_f8_cmdlistptrs(struct qce_device *pdev,
	unsigned char **pvaddr, enum qce_ota_algo_enum alg)
{
	struct sps_command_element *ce_vaddr;
	uintptr_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t encr_cfg = 0;
	uint32_t key_reg = 4;

	*pvaddr = (unsigned char *)ALIGN(((uintptr_t)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	ce_vaddr_start = (uintptr_t)(*pvaddr);

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to f8 cipher algorithm defined
	 * in ce_cmdlistptrs_ops structure.
	 */

	switch (alg) {
	case QCE_OTA_ALGO_KASUMI:
		cmdlistptr->f8_kasumi.cmdlist = (uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->f8_kasumi);
		encr_cfg = pdev->reg.encr_cfg_kasumi;
		break;

	case QCE_OTA_ALGO_SNOW3G:
	default:
		cmdlistptr->f8_snow3g.cmdlist = (uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->f8_snow3g);
		encr_cfg = pdev->reg.encr_cfg_snow3g;
		break;
	}
	/* clear status register */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG,
							0, NULL);
	/* set config to big endian */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_SEG_SIZE_REG, 0,
						&pcl_info->seg_size);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_CFG_REG, encr_cfg,
						&pcl_info->encr_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG, 0,
						&pcl_info->encr_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_START_REG, 0,
						&pcl_info->encr_seg_start);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG, 0,
						&pcl_info->auth_seg_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG,
						0, &pcl_info->auth_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_START_REG,
						0, &pcl_info->auth_seg_start);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_KEY0_REG, 0,
						 &pcl_info->encr_key);
	for (i = 1; i < key_reg; i++)
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t)),
				0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR0_IV0_REG, 0,
						&pcl_info->encr_cntr_iv);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CNTR1_IV1_REG, 0,
								NULL);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					pdev->reg.crypto_cfg_le, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uintptr_t)ce_vaddr - (uintptr_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_f9_cmdlistptrs(struct qce_device *pdev,
	unsigned char **pvaddr, enum qce_ota_algo_enum alg)
{
	struct sps_command_element *ce_vaddr;
	uintptr_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t auth_cfg = 0;
	uint32_t iv_reg = 0;

	*pvaddr = (unsigned char *)ALIGN(((uintptr_t)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr_start = (uintptr_t)(*pvaddr);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to authentication operations
	 * defined in ce_cmdlistptrs_ops structure.
	 */
	switch (alg) {
	case QCE_OTA_ALGO_KASUMI:
		cmdlistptr->f9_kasumi.cmdlist = (uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->f9_kasumi);
		auth_cfg = pdev->reg.auth_cfg_kasumi;
		break;

	case QCE_OTA_ALGO_SNOW3G:
	default:
		cmdlistptr->f9_snow3g.cmdlist = (uintptr_t)ce_vaddr;
		pcl_info = &(cmdlistptr->f9_snow3g);
		auth_cfg = pdev->reg.auth_cfg_snow3g;
	};

	/* clear status register */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_STATUS_REG,
							0, NULL);
	/* set config to big endian */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			pdev->reg.crypto_cfg_be, &pcl_info->crypto_cfg);

	iv_reg = 5;

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_SEG_SIZE_REG, 0,
						&pcl_info->seg_size);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_CFG_REG, 0,
						&pcl_info->encr_seg_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
					auth_cfg, &pcl_info->auth_seg_cfg);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG, 0,
						&pcl_info->auth_seg_size);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_START_REG, 0,
						&pcl_info->auth_seg_start);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_IV0_REG, 0,
							&pcl_info->auth_iv);
	for (i = 1; i < iv_reg; i++) {
		qce_add_cmd_element(pdev, &ce_vaddr,
				(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t)),
				0, NULL);
	}
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT0_REG,
					0, &pcl_info->auth_bytecount);
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_BYTECNT1_REG, 0, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					pdev->reg.crypto_cfg_le, NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uintptr_t)ce_vaddr - (uintptr_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_unlock_pipe_cmdlistptrs(struct qce_device *pdev,
		unsigned char **pvaddr)
{
	struct sps_command_element *ce_vaddr;
	uintptr_t ce_vaddr_start = (uintptr_t)(*pvaddr);
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;

	*pvaddr = (unsigned char *)ALIGN(((uintptr_t)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	cmdlistptr->unlock_all_pipes.cmdlist = (uintptr_t)ce_vaddr;
	pcl_info = &(cmdlistptr->unlock_all_pipes);

	/*
	 * Designate chunks of the allocated memory to command list
	 * to unlock pipes.
	 */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					CRYPTO_CONFIG_RESET, NULL);
	pcl_info->size = (uintptr_t)ce_vaddr - (uintptr_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int qce_setup_cmdlistptrs(struct qce_device *pdev,
					unsigned char **pvaddr)
{
	struct sps_command_element *ce_vaddr =
				(struct sps_command_element *)(*pvaddr);
	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to operations defined
	 * in ce_cmdlistptrs_ops structure.
	 */
	ce_vaddr =
		(struct sps_command_element *)ALIGN(((uintptr_t) ce_vaddr),
					pdev->ce_sps.ce_burst_size);
	*pvaddr = (unsigned char *) ce_vaddr;

	_setup_cipher_aes_cmdlistptrs(pdev, pvaddr, QCE_MODE_CBC, true);
	_setup_cipher_aes_cmdlistptrs(pdev, pvaddr, QCE_MODE_CTR, true);
	_setup_cipher_aes_cmdlistptrs(pdev, pvaddr, QCE_MODE_ECB, true);
	_setup_cipher_aes_cmdlistptrs(pdev, pvaddr, QCE_MODE_XTS, true);
	_setup_cipher_aes_cmdlistptrs(pdev, pvaddr, QCE_MODE_CBC, false);
	_setup_cipher_aes_cmdlistptrs(pdev, pvaddr, QCE_MODE_CTR, false);
	_setup_cipher_aes_cmdlistptrs(pdev, pvaddr, QCE_MODE_ECB, false);
	_setup_cipher_aes_cmdlistptrs(pdev, pvaddr, QCE_MODE_XTS, false);

	_setup_cipher_des_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_DES, true);
	_setup_cipher_des_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_DES, false);
	_setup_cipher_des_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_3DES, true);
	_setup_cipher_des_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_3DES, false);

	_setup_auth_cmdlistptrs(pdev, pvaddr, QCE_HASH_SHA1, false);
	_setup_auth_cmdlistptrs(pdev, pvaddr, QCE_HASH_SHA256, false);

	_setup_auth_cmdlistptrs(pdev, pvaddr, QCE_HASH_SHA1_HMAC, false);
	_setup_auth_cmdlistptrs(pdev, pvaddr, QCE_HASH_SHA256_HMAC, false);

	_setup_auth_cmdlistptrs(pdev, pvaddr, QCE_HASH_AES_CMAC, true);
	_setup_auth_cmdlistptrs(pdev, pvaddr, QCE_HASH_AES_CMAC, false);

	_setup_aead_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_DES, QCE_MODE_CBC,
					DES_KEY_SIZE, true);
	_setup_aead_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_3DES, QCE_MODE_CBC,
					DES3_EDE_KEY_SIZE, true);
	_setup_aead_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_AES, QCE_MODE_CBC,
					AES128_KEY_SIZE, true);
	_setup_aead_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_AES, QCE_MODE_CBC,
					AES256_KEY_SIZE, true);
	_setup_aead_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_DES, QCE_MODE_CBC,
					DES_KEY_SIZE, false);
	_setup_aead_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_3DES, QCE_MODE_CBC,
					DES3_EDE_KEY_SIZE, false);
	_setup_aead_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_AES, QCE_MODE_CBC,
					AES128_KEY_SIZE, false);
	_setup_aead_cmdlistptrs(pdev, pvaddr, CIPHER_ALG_AES, QCE_MODE_CBC,
					AES256_KEY_SIZE, false);

	_setup_aead_ccm_cmdlistptrs(pdev, pvaddr, true);
	_setup_aead_ccm_cmdlistptrs(pdev, pvaddr, false);
	_setup_f8_cmdlistptrs(pdev, pvaddr, QCE_OTA_ALGO_KASUMI);
	_setup_f8_cmdlistptrs(pdev, pvaddr, QCE_OTA_ALGO_SNOW3G);
	_setup_f9_cmdlistptrs(pdev, pvaddr, QCE_OTA_ALGO_KASUMI);
	_setup_f9_cmdlistptrs(pdev, pvaddr, QCE_OTA_ALGO_SNOW3G);
	_setup_unlock_pipe_cmdlistptrs(pdev, pvaddr);

	return 0;
}

static int qce_setup_ce_sps_data(struct qce_device *pce_dev)
{
	unsigned char *vaddr;

	vaddr = pce_dev->coh_vmem;
	vaddr = (unsigned char *)ALIGN(((uintptr_t)vaddr),
					pce_dev->ce_sps.ce_burst_size);
	/* Allow for 256 descriptor (cmd and data) entries per pipe */
	pce_dev->ce_sps.in_transfer.iovec = (struct sps_iovec *)vaddr;
	pce_dev->ce_sps.in_transfer.iovec_phys =
					(uintptr_t)GET_PHYS_ADDR(vaddr);
	vaddr += QCE_MAX_NUM_DSCR * sizeof(struct sps_iovec);

	pce_dev->ce_sps.out_transfer.iovec = (struct sps_iovec *)vaddr;
	pce_dev->ce_sps.out_transfer.iovec_phys =
					(uintptr_t)GET_PHYS_ADDR(vaddr);
	vaddr += QCE_MAX_NUM_DSCR * sizeof(struct sps_iovec);

	if (pce_dev->support_cmd_dscr)
		qce_setup_cmdlistptrs(pce_dev, &vaddr);
	vaddr = (unsigned char *)ALIGN(((uintptr_t)vaddr),
					pce_dev->ce_sps.ce_burst_size);
	pce_dev->ce_sps.result_dump = (uintptr_t)vaddr;
	pce_dev->ce_sps.result = (struct ce_result_dump_format *)vaddr;
	vaddr += CRYPTO_RESULT_DUMP_SIZE;

	pce_dev->ce_sps.ignore_buffer = (uintptr_t)vaddr;
	vaddr += pce_dev->ce_sps.ce_burst_size * 2;

	if ((vaddr - pce_dev->coh_vmem) > pce_dev->memsize)
		panic("qce50: Not enough coherent memory. Allocate %x , need %lx\n",
				 pce_dev->memsize, (uintptr_t)vaddr -
				(uintptr_t)pce_dev->coh_vmem);
	return 0;
}

static int qce_init_ce_cfg_val(struct qce_device *pce_dev)
{
	uint32_t beats = (pce_dev->ce_sps.ce_burst_size >> 3) - 1;
	uint32_t pipe_pair = pce_dev->ce_sps.pipe_pair_index;

	pce_dev->reg.crypto_cfg_be = (beats << CRYPTO_REQ_SIZE) |
		BIT(CRYPTO_MASK_DOUT_INTR) | BIT(CRYPTO_MASK_DIN_INTR) |
		BIT(CRYPTO_MASK_OP_DONE_INTR) | (0 << CRYPTO_HIGH_SPD_EN_N) |
		(pipe_pair << CRYPTO_PIPE_SET_SELECT);

	pce_dev->reg.crypto_cfg_le =
		(pce_dev->reg.crypto_cfg_be | CRYPTO_LITTLE_ENDIAN_MASK);

	/* Initialize encr_cfg register for AES alg */
	pce_dev->reg.encr_cfg_aes_cbc_128 =
		(CRYPTO_ENCR_KEY_SZ_AES128 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_CBC << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_aes_cbc_256 =
		(CRYPTO_ENCR_KEY_SZ_AES256 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_CBC << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_aes_ctr_128 =
		(CRYPTO_ENCR_KEY_SZ_AES128 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_CTR << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_aes_ctr_256 =
		(CRYPTO_ENCR_KEY_SZ_AES256 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_CTR << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_aes_xts_128 =
		(CRYPTO_ENCR_KEY_SZ_AES128 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_XTS << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_aes_xts_256 =
		(CRYPTO_ENCR_KEY_SZ_AES256 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_XTS << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_aes_ecb_128 =
		(CRYPTO_ENCR_KEY_SZ_AES128 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_ECB << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_aes_ecb_256 =
		(CRYPTO_ENCR_KEY_SZ_AES256 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_ECB << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_aes_ccm_128 =
		(CRYPTO_ENCR_KEY_SZ_AES128 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_CCM << CRYPTO_ENCR_MODE)|
		(CRYPTO_LAST_CCM_XFR << CRYPTO_LAST_CCM);

	pce_dev->reg.encr_cfg_aes_ccm_256 =
		(CRYPTO_ENCR_KEY_SZ_AES256 << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_CCM << CRYPTO_ENCR_MODE) |
		(CRYPTO_LAST_CCM_XFR << CRYPTO_LAST_CCM);

	/* Initialize encr_cfg register for DES alg */
	pce_dev->reg.encr_cfg_des_ecb =
		(CRYPTO_ENCR_KEY_SZ_DES << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_ECB << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_des_cbc =
		(CRYPTO_ENCR_KEY_SZ_DES << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_CBC << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_3des_ecb =
		(CRYPTO_ENCR_KEY_SZ_3DES << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_ECB << CRYPTO_ENCR_MODE);

	pce_dev->reg.encr_cfg_3des_cbc =
		(CRYPTO_ENCR_KEY_SZ_3DES << CRYPTO_ENCR_KEY_SZ) |
		(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG) |
		(CRYPTO_ENCR_MODE_CBC << CRYPTO_ENCR_MODE);

	/* Initialize encr_cfg register for kasumi/snow3g  alg */
	pce_dev->reg.encr_cfg_kasumi =
		(CRYPTO_ENCR_ALG_KASUMI << CRYPTO_ENCR_ALG);

	pce_dev->reg.encr_cfg_snow3g =
		(CRYPTO_ENCR_ALG_SNOW_3G << CRYPTO_ENCR_ALG);

	/* Initialize auth_cfg register for CMAC alg */
	pce_dev->reg.auth_cfg_cmac_128 =
		(1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST) |
		(CRYPTO_AUTH_MODE_CMAC << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_SIZE_ENUM_16_BYTES << CRYPTO_AUTH_SIZE) |
		(CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG) |
		(CRYPTO_AUTH_KEY_SZ_AES128 << CRYPTO_AUTH_KEY_SIZE);

	pce_dev->reg.auth_cfg_cmac_256 =
		(1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST) |
		(CRYPTO_AUTH_MODE_CMAC << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_SIZE_ENUM_16_BYTES << CRYPTO_AUTH_SIZE) |
		(CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG) |
		(CRYPTO_AUTH_KEY_SZ_AES256 << CRYPTO_AUTH_KEY_SIZE);

	/* Initialize auth_cfg register for HMAC alg */
	pce_dev->reg.auth_cfg_hmac_sha1 =
		(CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_SIZE_SHA1 << CRYPTO_AUTH_SIZE) |
		(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
		(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);

	pce_dev->reg.auth_cfg_hmac_sha256 =
		(CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_SIZE_SHA256 << CRYPTO_AUTH_SIZE) |
		(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
		(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);

	/* Initialize auth_cfg register for SHA1/256 alg */
	pce_dev->reg.auth_cfg_sha1 =
		(CRYPTO_AUTH_MODE_HASH << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_SIZE_SHA1 << CRYPTO_AUTH_SIZE) |
		(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
		(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);

	pce_dev->reg.auth_cfg_sha256 =
		(CRYPTO_AUTH_MODE_HASH << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_SIZE_SHA256 << CRYPTO_AUTH_SIZE) |
		(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
		(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);

	/* Initialize auth_cfg register for AEAD alg */
	pce_dev->reg.auth_cfg_aead_sha1_hmac =
		(CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_SIZE_SHA1 << CRYPTO_AUTH_SIZE) |
		(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
		(1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST);

	pce_dev->reg.auth_cfg_aead_sha256_hmac =
		(CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_SIZE_SHA256 << CRYPTO_AUTH_SIZE) |
		(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
		(1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST);

	pce_dev->reg.auth_cfg_aes_ccm_128 =
		(1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST) |
		(CRYPTO_AUTH_MODE_CCM << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG) |
		(CRYPTO_AUTH_KEY_SZ_AES128 << CRYPTO_AUTH_KEY_SIZE) |
		((MAX_NONCE/sizeof(uint32_t)) << CRYPTO_AUTH_NONCE_NUM_WORDS);
	pce_dev->reg.auth_cfg_aes_ccm_128 &= ~(1 << CRYPTO_USE_HW_KEY_AUTH);

	pce_dev->reg.auth_cfg_aes_ccm_256 =
		(1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST) |
		(CRYPTO_AUTH_MODE_CCM << CRYPTO_AUTH_MODE)|
		(CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG) |
		(CRYPTO_AUTH_KEY_SZ_AES256 << CRYPTO_AUTH_KEY_SIZE) |
		((MAX_NONCE/sizeof(uint32_t)) << CRYPTO_AUTH_NONCE_NUM_WORDS);
	pce_dev->reg.auth_cfg_aes_ccm_256 &= ~(1 << CRYPTO_USE_HW_KEY_AUTH);

	/* Initialize auth_cfg register for kasumi/snow3g */
	pce_dev->reg.auth_cfg_kasumi =
			(CRYPTO_AUTH_ALG_KASUMI << CRYPTO_AUTH_ALG) |
				BIT(CRYPTO_FIRST) | BIT(CRYPTO_LAST);
	pce_dev->reg.auth_cfg_snow3g =
			(CRYPTO_AUTH_ALG_SNOW3G << CRYPTO_AUTH_ALG) |
				BIT(CRYPTO_FIRST) | BIT(CRYPTO_LAST);
	return 0;
}

static int _qce_aead_ccm_req(void *handle, struct qce_req *q_req)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	struct aead_request *areq = (struct aead_request *) q_req->areq;
	uint32_t authsize = q_req->authsize;
	uint32_t totallen_in, out_len;
	uint32_t hw_pad_out = 0;
	int rc = 0;
	int ce_burst_size;
	struct qce_cmdlist_info *cmdlistinfo = NULL;

	ce_burst_size = pce_dev->ce_sps.ce_burst_size;
	totallen_in = areq->cryptlen + areq->assoclen;
	if (q_req->dir == QCE_ENCRYPT) {
		q_req->cryptlen = areq->cryptlen;
		out_len = areq->cryptlen + authsize;
		hw_pad_out = ALIGN(authsize, ce_burst_size) - authsize;
	} else {
		q_req->cryptlen = areq->cryptlen - authsize;
		out_len = q_req->cryptlen;
		hw_pad_out = authsize;
	}

	if (pce_dev->ce_sps.minor_version == 0) {
		/*
		 * For crypto 5.0 that has burst size alignment requirement
		 * for data descritpor,
		 * the agent above(qcrypto) prepares the src scatter list with
		 * memory starting with associated data, followed by
		 * data stream to be ciphered.
		 * The destination scatter list is pointing to the same
		 * data area as source.
		 */
		pce_dev->src_nents = count_sg(areq->src, totallen_in);
	} else {
		pce_dev->src_nents = count_sg(areq->src, areq->cryptlen);
	}

	pce_dev->assoc_nents = count_sg(areq->assoc, areq->assoclen);
	pce_dev->authsize = q_req->authsize;

	/* associated data input */
	qce_dma_map_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
					 DMA_TO_DEVICE);
	/* cipher input */
	qce_dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	/* cipher + mac output  for encryption    */
	if (areq->src != areq->dst) {
		if (pce_dev->ce_sps.minor_version == 0)
			/*
			 * The destination scatter list is pointing to the same
			 * data area as src.
			 * Note, the associated data will be pass-through
			 * at the begining of destination area.
			 */
			pce_dev->dst_nents = count_sg(areq->dst,
						out_len + areq->assoclen);
		else
			pce_dev->dst_nents = count_sg(areq->dst, out_len);

		qce_dma_map_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
	} else {
		pce_dev->dst_nents = pce_dev->src_nents;
	}

	if (pce_dev->support_cmd_dscr) {
		cmdlistinfo = _ce_get_cipher_cmdlistinfo(pce_dev, q_req);
		if (cmdlistinfo == NULL) {
			pr_err("Unsupported cipher algorithm %d, mode %d\n",
						q_req->alg, q_req->mode);
			return -EINVAL;
		}
		/* set up crypto device */
		rc = _ce_setup_cipher(pce_dev, q_req, totallen_in,
					areq->assoclen, cmdlistinfo);
	} else {
		/* set up crypto device */
		rc = _ce_setup_cipher_direct(pce_dev, q_req, totallen_in,
					areq->assoclen);
	}
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to bam */
	pce_dev->areq = q_req->areq;
	pce_dev->qce_cb = q_req->qce_cb;

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.producer.event.callback = _aead_sps_producer_callback;
	pce_dev->ce_sps.producer.event.options = SPS_O_DESC_DONE;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}
	_qce_sps_iovec_count_init(pce_dev);

	if (pce_dev->support_cmd_dscr)
		_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);

	if (pce_dev->ce_sps.minor_version == 0) {
		if (_qce_sps_add_sg_data(pce_dev, areq->src, totallen_in,
					&pce_dev->ce_sps.in_transfer))
			goto bad;

		_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

		/*
		 * The destination data should be big enough to
		 * include  CCM padding.
		 */
		if (_qce_sps_add_sg_data(pce_dev, areq->dst, out_len +
					areq->assoclen + hw_pad_out,
				&pce_dev->ce_sps.out_transfer))
			goto bad;
		if (totallen_in > SPS_MAX_PKT_SIZE) {
			_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
			pce_dev->ce_sps.producer.event.options =
							SPS_O_DESC_DONE;
			pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
		} else {
			if (_qce_sps_add_data(GET_PHYS_ADDR(
					pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					&pce_dev->ce_sps.out_transfer))
				goto bad;
			_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
			pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		}
	} else {
		if (_qce_sps_add_sg_data(pce_dev, areq->assoc, areq->assoclen,
					 &pce_dev->ce_sps.in_transfer))
			goto bad;
		if (_qce_sps_add_sg_data(pce_dev, areq->src, areq->cryptlen,
					&pce_dev->ce_sps.in_transfer))
			goto bad;
		_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

		/* Pass through to ignore associated  data*/
		if (_qce_sps_add_data(
				GET_PHYS_ADDR(pce_dev->ce_sps.ignore_buffer),
				areq->assoclen,
				&pce_dev->ce_sps.out_transfer))
			goto bad;
		if (_qce_sps_add_sg_data(pce_dev, areq->dst, out_len,
					&pce_dev->ce_sps.out_transfer))
			goto bad;
		/* Pass through to ignore hw_pad (padding of the MAC data) */
		if (_qce_sps_add_data(
				GET_PHYS_ADDR(pce_dev->ce_sps.ignore_buffer),
				hw_pad_out, &pce_dev->ce_sps.out_transfer))
			goto bad;
		if (totallen_in > SPS_MAX_PKT_SIZE) {
			_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
			pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
		} else {
			if (_qce_sps_add_data(
				GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer))
				goto bad;
			_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
			pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		}
	}
	rc = _qce_sps_transfer(pce_dev);
	if (rc)
		goto bad;
	return 0;

bad:
	if (pce_dev->assoc_nents) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->assoc,
				pce_dev->assoc_nents, DMA_TO_DEVICE);
	}
	if (pce_dev->src_nents) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
				(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
								DMA_TO_DEVICE);
	}
	if (areq->src != areq->dst) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
	}

	return rc;
}

static int _qce_suspend(void *handle)
{
	struct qce_device *pce_dev = (struct qce_device *)handle;
	struct sps_pipe *sps_pipe_info;

	if (handle == NULL)
		return -ENODEV;

	qce_enable_clk(pce_dev);

	sps_pipe_info = pce_dev->ce_sps.consumer.pipe;
	sps_disconnect(sps_pipe_info);

	sps_pipe_info = pce_dev->ce_sps.producer.pipe;
	sps_disconnect(sps_pipe_info);

	qce_disable_clk(pce_dev);
	return 0;
}

static int _qce_resume(void *handle)
{
	struct qce_device *pce_dev = (struct qce_device *)handle;
	struct sps_pipe *sps_pipe_info;
	struct sps_connect *sps_connect_info;
	int rc;

	if (handle == NULL)
		return -ENODEV;

	qce_enable_clk(pce_dev);

	sps_pipe_info = pce_dev->ce_sps.consumer.pipe;
	sps_connect_info = &pce_dev->ce_sps.consumer.connect;
	memset(sps_connect_info->desc.base, 0x00, sps_connect_info->desc.size);
	rc = sps_connect(sps_pipe_info, sps_connect_info);
	if (rc) {
		pr_err("sps_connect() fail pipe_handle=0x%lx, rc = %d\n",
			(uintptr_t)sps_pipe_info, rc);
		return rc;
	}
	sps_pipe_info = pce_dev->ce_sps.producer.pipe;
	sps_connect_info = &pce_dev->ce_sps.producer.connect;
	memset(sps_connect_info->desc.base, 0x00, sps_connect_info->desc.size);
	rc = sps_connect(sps_pipe_info, sps_connect_info);
	if (rc)
		pr_err("sps_connect() fail pipe_handle=0x%lx, rc = %d\n",
			(uintptr_t)sps_pipe_info, rc);

	pce_dev->ce_sps.out_transfer.user = pce_dev->ce_sps.producer.pipe;
	pce_dev->ce_sps.in_transfer.user = pce_dev->ce_sps.consumer.pipe;

	qce_disable_clk(pce_dev);
	return rc;
}

struct qce_pm_table qce_pm_table  = {_qce_suspend, _qce_resume};
EXPORT_SYMBOL(qce_pm_table);

int qce_aead_req(void *handle, struct qce_req *q_req)
{
	struct qce_device *pce_dev;
	struct aead_request *areq;
	uint32_t authsize;
	struct crypto_aead *aead;
	uint32_t ivsize;
	uint32_t totallen;
	int rc;
	struct qce_cmdlist_info *cmdlistinfo = NULL;

	if (q_req->mode == QCE_MODE_CCM)
		return _qce_aead_ccm_req(handle, q_req);

	pce_dev = (struct qce_device *) handle;
	areq = (struct aead_request *) q_req->areq;
	aead = crypto_aead_reqtfm(areq);
	ivsize = crypto_aead_ivsize(aead);
	q_req->ivsize = ivsize;
	authsize = q_req->authsize;
	if (q_req->dir == QCE_ENCRYPT)
		q_req->cryptlen = areq->cryptlen;
	else
		q_req->cryptlen = areq->cryptlen - authsize;

	totallen = q_req->cryptlen + areq->assoclen + ivsize;

	if (pce_dev->support_cmd_dscr) {
		cmdlistinfo = _ce_get_aead_cmdlistinfo(pce_dev, q_req);
		if (cmdlistinfo == NULL) {
			pr_err("Unsupported aead ciphering algorithm %d, mode %d, ciphering key length %d, auth digest size %d\n",
				q_req->alg, q_req->mode, q_req->encklen,
					q_req->authsize);
			return -EINVAL;
		}
		/* set up crypto device */
		rc = _ce_setup_aead(pce_dev, q_req, totallen,
					areq->assoclen + ivsize, cmdlistinfo);
		if (rc < 0)
			return -EINVAL;
	};


	pce_dev->assoc_nents = count_sg(areq->assoc, areq->assoclen);

	if (pce_dev->ce_sps.minor_version == 0) {
		/*
		 * For crypto 5.0 that has burst size alignment requirement
		 * for data descritpor,
		 * the agent above(qcrypto) prepares the src scatter list with
		 * memory starting with associated data, followed by
		 * iv, and data stream to be ciphered.
		 */
		pce_dev->src_nents = count_sg(areq->src, totallen);
	} else {
		pce_dev->src_nents = count_sg(areq->src, q_req->cryptlen);
	};

	pce_dev->ivsize = q_req->ivsize;
	pce_dev->authsize = q_req->authsize;
	pce_dev->phy_iv_in = 0;

	/* associated data input */
	qce_dma_map_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
					 DMA_TO_DEVICE);
	/* cipher input */
	qce_dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	/* cipher output  for encryption    */
	if (areq->src != areq->dst) {
		if (pce_dev->ce_sps.minor_version == 0)
			/*
			 * The destination scatter list is pointing to the same
			 * data area as source.
			 */
			pce_dev->dst_nents = count_sg(areq->dst, totallen);
		else
			pce_dev->dst_nents = count_sg(areq->dst,
							 q_req->cryptlen);

		qce_dma_map_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
	}


	/* cipher iv for input */
	if (pce_dev->ce_sps.minor_version != 0)
		pce_dev->phy_iv_in = dma_map_single(pce_dev->pdev, q_req->iv,
			ivsize, DMA_TO_DEVICE);

	/* setup for callback, and issue command to bam */
	pce_dev->areq = q_req->areq;
	pce_dev->qce_cb = q_req->qce_cb;

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.producer.event.callback = _aead_sps_producer_callback;
	pce_dev->ce_sps.producer.event.options = SPS_O_DESC_DONE;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}
	_qce_sps_iovec_count_init(pce_dev);

	if (pce_dev->support_cmd_dscr) {
		_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);
	} else {
		rc = _ce_setup_aead_direct(pce_dev, q_req, totallen,
					areq->assoclen + ivsize);
		if (rc)
			goto bad;
	}

	if (pce_dev->ce_sps.minor_version == 0) {
		if (_qce_sps_add_sg_data(pce_dev, areq->src, totallen,
					&pce_dev->ce_sps.in_transfer))
			goto bad;

		_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

		if (_qce_sps_add_sg_data(pce_dev, areq->dst, totallen,
				&pce_dev->ce_sps.out_transfer))
			goto bad;
		if (totallen > SPS_MAX_PKT_SIZE) {
			_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
			pce_dev->ce_sps.producer.event.options =
							SPS_O_DESC_DONE;
			pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
		} else {
			if (_qce_sps_add_data(GET_PHYS_ADDR(
					pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					&pce_dev->ce_sps.out_transfer))
				goto bad;
			_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
			pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		}
	} else {
		if (_qce_sps_add_sg_data(pce_dev, areq->assoc, areq->assoclen,
					 &pce_dev->ce_sps.in_transfer))
			goto bad;
		if (_qce_sps_add_data((uint32_t)pce_dev->phy_iv_in, ivsize,
					&pce_dev->ce_sps.in_transfer))
			goto bad;
		if (_qce_sps_add_sg_data(pce_dev, areq->src, q_req->cryptlen,
					&pce_dev->ce_sps.in_transfer))
			goto bad;
		_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

		/* Pass through to ignore associated + iv data*/
		if (_qce_sps_add_data(
				GET_PHYS_ADDR(pce_dev->ce_sps.ignore_buffer),
				(ivsize + areq->assoclen),
				&pce_dev->ce_sps.out_transfer))
			goto bad;
		if (_qce_sps_add_sg_data(pce_dev, areq->dst, q_req->cryptlen,
					&pce_dev->ce_sps.out_transfer))
			goto bad;

		if (totallen > SPS_MAX_PKT_SIZE) {
			_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
			pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
		} else {
			if (_qce_sps_add_data(
				GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer))
				goto bad;
			_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
			pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		}
	}
	rc = _qce_sps_transfer(pce_dev);
	if (rc)
		goto bad;
	return 0;

bad:
	if (pce_dev->assoc_nents) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->assoc,
				pce_dev->assoc_nents, DMA_TO_DEVICE);
	}
	if (pce_dev->src_nents) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
				(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
								DMA_TO_DEVICE);
	}
	if (areq->src != areq->dst) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
	}
	if (pce_dev->phy_iv_in) {
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_iv_in,
				ivsize, DMA_TO_DEVICE);
	}

	return rc;
}
EXPORT_SYMBOL(qce_aead_req);

int qce_ablk_cipher_req(void *handle, struct qce_req *c_req)
{
	int rc = 0;
	struct qce_device *pce_dev = (struct qce_device *) handle;
	struct ablkcipher_request *areq = (struct ablkcipher_request *)
						c_req->areq;
	struct qce_cmdlist_info *cmdlistinfo = NULL;

	pce_dev->src_nents = 0;
	pce_dev->dst_nents = 0;

	/* cipher input */
	pce_dev->src_nents = count_sg(areq->src, areq->nbytes);

	qce_dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
		(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	/* cipher output */
	if (areq->src != areq->dst) {
		pce_dev->dst_nents = count_sg(areq->dst, areq->nbytes);
			qce_dma_map_sg(pce_dev->pdev, areq->dst,
				pce_dev->dst_nents, DMA_FROM_DEVICE);
	} else {
		pce_dev->dst_nents = pce_dev->src_nents;
	}
	pce_dev->dir = c_req->dir;
	if  ((pce_dev->ce_sps.minor_version == 0) && (c_req->dir == QCE_DECRYPT)
			&& (c_req->mode == QCE_MODE_CBC)) {
		memcpy(pce_dev->dec_iv, (unsigned char *)sg_virt(areq->src) +
					 areq->src->length - 16,
			NUM_OF_CRYPTO_CNTR_IV_REG * CRYPTO_REG_SIZE);
	}

	/* set up crypto device */
	if (pce_dev->support_cmd_dscr) {
		cmdlistinfo = _ce_get_cipher_cmdlistinfo(pce_dev, c_req);
		if (cmdlistinfo == NULL) {
			pr_err("Unsupported cipher algorithm %d, mode %d\n",
						c_req->alg, c_req->mode);
			return -EINVAL;
		}
		rc = _ce_setup_cipher(pce_dev, c_req, areq->nbytes, 0,
							cmdlistinfo);
	} else {
		rc = _ce_setup_cipher_direct(pce_dev, c_req, areq->nbytes, 0);
	}
	if (rc < 0)
		goto bad;

	/* setup for client callback, and issue command to BAM */
	pce_dev->areq = areq;
	pce_dev->qce_cb = c_req->qce_cb;

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.producer.event.callback =
				_ablk_cipher_sps_producer_callback;
	pce_dev->ce_sps.producer.event.options = SPS_O_DESC_DONE;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}
	_qce_sps_iovec_count_init(pce_dev);
	if (pce_dev->support_cmd_dscr)
		_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);
	if (_qce_sps_add_sg_data(pce_dev, areq->src, areq->nbytes,
					&pce_dev->ce_sps.in_transfer))
		goto bad;
	_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

	if (_qce_sps_add_sg_data(pce_dev, areq->dst, areq->nbytes,
					&pce_dev->ce_sps.out_transfer))
		goto bad;
	if (areq->nbytes > SPS_MAX_PKT_SIZE) {
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
	} else {
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		if (_qce_sps_add_data(
				GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
				CRYPTO_RESULT_DUMP_SIZE,
				&pce_dev->ce_sps.out_transfer))
			goto bad;
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
	}
	rc = _qce_sps_transfer(pce_dev);
	if (rc)
		goto bad;
		return 0;
bad:
	if (areq->src != areq->dst) {
		if (pce_dev->dst_nents) {
			qce_dma_unmap_sg(pce_dev->pdev, areq->dst,
			pce_dev->dst_nents, DMA_FROM_DEVICE);
		}
	}
	if (pce_dev->src_nents) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->src,
				pce_dev->src_nents,
				(areq->src == areq->dst) ?
				DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
	}
	return rc;
}
EXPORT_SYMBOL(qce_ablk_cipher_req);

int qce_process_sha_req(void *handle, struct qce_sha_req *sreq)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	int rc;

	struct ahash_request *areq = (struct ahash_request *)sreq->areq;
	struct qce_cmdlist_info *cmdlistinfo = NULL;

	pce_dev->src_nents = count_sg(sreq->src, sreq->size);
	qce_dma_map_sg(pce_dev->pdev, sreq->src, pce_dev->src_nents,
							DMA_TO_DEVICE);

	if (pce_dev->support_cmd_dscr) {
		cmdlistinfo = _ce_get_hash_cmdlistinfo(pce_dev, sreq);
		if (cmdlistinfo == NULL) {
			pr_err("Unsupported hash algorithm %d\n", sreq->alg);
			return -EINVAL;
		}
		rc = _ce_setup_hash(pce_dev, sreq, cmdlistinfo);
	} else {
		rc = _ce_setup_hash_direct(pce_dev, sreq);
	}
	if (rc < 0)
		goto bad;

	pce_dev->areq = areq;
	pce_dev->qce_cb = sreq->qce_cb;

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.producer.event.callback = _sha_sps_producer_callback;
	pce_dev->ce_sps.producer.event.options = SPS_O_DESC_DONE;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}
	_qce_sps_iovec_count_init(pce_dev);

	if (pce_dev->support_cmd_dscr)
		_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);
	if (_qce_sps_add_sg_data(pce_dev, areq->src, areq->nbytes,
						 &pce_dev->ce_sps.in_transfer))
		goto bad;
	if (areq->nbytes)
		_qce_set_flag(&pce_dev->ce_sps.in_transfer,
					SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);
	if (_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer))
		goto bad;
	_qce_set_flag(&pce_dev->ce_sps.out_transfer, SPS_IOVEC_FLAG_INT);
	rc = _qce_sps_transfer(pce_dev);
	if (rc)
		goto bad;
		return 0;
bad:
	if (pce_dev->src_nents) {
		qce_dma_unmap_sg(pce_dev->pdev, sreq->src,
				pce_dev->src_nents, DMA_TO_DEVICE);
	}
	return rc;
}
EXPORT_SYMBOL(qce_process_sha_req);

int qce_f8_req(void *handle, struct qce_f8_req *req,
			void *cookie, qce_comp_func_ptr_t qce_cb)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	bool key_stream_mode;
	dma_addr_t dst;
	int rc;
	struct qce_cmdlist_info *cmdlistinfo;

	switch (req->algorithm) {
	case QCE_OTA_ALGO_KASUMI:
		cmdlistinfo = &pce_dev->ce_sps.cmdlistptr.f8_kasumi;
		break;
	case QCE_OTA_ALGO_SNOW3G:
		cmdlistinfo = &pce_dev->ce_sps.cmdlistptr.f8_snow3g;
		break;
	default:
		return -EINVAL;
	};

	key_stream_mode = (req->data_in == NULL);

	if ((key_stream_mode && (req->data_len & 0xf)) ||
				(req->bearer >= QCE_OTA_MAX_BEARER))
		return -EINVAL;

	/* F8 cipher input       */
	if (key_stream_mode)
		pce_dev->phy_ota_src = 0;
	else {
		pce_dev->phy_ota_src = dma_map_single(pce_dev->pdev,
					req->data_in, req->data_len,
					(req->data_in == req->data_out) ?
					DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
	}

	/* F8 cipher output     */
	if (req->data_in != req->data_out) {
		dst = dma_map_single(pce_dev->pdev, req->data_out,
				req->data_len, DMA_FROM_DEVICE);
		pce_dev->phy_ota_dst = dst;
	} else {
		/* in place ciphering */
		dst = pce_dev->phy_ota_src;
		pce_dev->phy_ota_dst = 0;
	}
	pce_dev->ota_size = req->data_len;


	/* set up crypto device */
	if (pce_dev->support_cmd_dscr)
		rc = _ce_f8_setup(pce_dev, req, key_stream_mode, 1, 0,
				 req->data_len, cmdlistinfo);
	else
		rc = _ce_f8_setup_direct(pce_dev, req, key_stream_mode, 1, 0,
				 req->data_len);
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to sps */
	pce_dev->areq = cookie;
	pce_dev->qce_cb = qce_cb;

	/* Register producer callback event for DESC_DONE event. */
	pce_dev->ce_sps.producer.event.callback =
				_f8_sps_producer_callback;
	pce_dev->ce_sps.producer.event.options = SPS_O_DESC_DONE;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}
	_qce_sps_iovec_count_init(pce_dev);

	if (pce_dev->support_cmd_dscr)
		_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);

	if (!key_stream_mode) {
		_qce_sps_add_data((uint32_t)pce_dev->phy_ota_src, req->data_len,
					&pce_dev->ce_sps.in_transfer);
		_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);
	}

	_qce_sps_add_data((uint32_t)dst, req->data_len,
					&pce_dev->ce_sps.out_transfer);

	if (req->data_len > SPS_MAX_PKT_SIZE) {
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
	} else {
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer);
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
	}
	rc = _qce_sps_transfer(pce_dev);
	if (rc)
		goto bad;
	return 0;
bad:
	if (pce_dev->phy_ota_dst != 0)
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_dst,
				req->data_len, DMA_FROM_DEVICE);
	if (pce_dev->phy_ota_src != 0)
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_src,
				req->data_len,
				(req->data_in == req->data_out) ?
					DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
	return rc;
}
EXPORT_SYMBOL(qce_f8_req);

int qce_f8_multi_pkt_req(void *handle, struct qce_f8_multi_pkt_req *mreq,
			void *cookie, qce_comp_func_ptr_t qce_cb)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	uint16_t num_pkt = mreq->num_pkt;
	uint16_t cipher_start = mreq->cipher_start;
	uint16_t cipher_size = mreq->cipher_size;
	struct qce_f8_req *req = &mreq->qce_f8_req;
	uint32_t total;
	dma_addr_t dst = 0;
	int rc = 0;
	struct qce_cmdlist_info *cmdlistinfo;

	switch (req->algorithm) {
	case QCE_OTA_ALGO_KASUMI:
		cmdlistinfo = &pce_dev->ce_sps.cmdlistptr.f8_kasumi;
		break;
	case QCE_OTA_ALGO_SNOW3G:
		cmdlistinfo = &pce_dev->ce_sps.cmdlistptr.f8_snow3g;
		break;
	default:
		return -EINVAL;
	};

	total = num_pkt *  req->data_len;

	/* F8 cipher input       */
	pce_dev->phy_ota_src = dma_map_single(pce_dev->pdev,
				req->data_in, total,
				(req->data_in == req->data_out) ?
				DMA_BIDIRECTIONAL : DMA_TO_DEVICE);

	/* F8 cipher output      */
	if (req->data_in != req->data_out) {
		dst = dma_map_single(pce_dev->pdev, req->data_out, total,
						DMA_FROM_DEVICE);
		pce_dev->phy_ota_dst = dst;
	} else {
		/* in place ciphering */
		dst = pce_dev->phy_ota_src;
		pce_dev->phy_ota_dst = 0;
	}

	pce_dev->ota_size = total;

	/* set up crypto device */
	if (pce_dev->support_cmd_dscr)
		rc = _ce_f8_setup(pce_dev, req, false, num_pkt, cipher_start,
			cipher_size, cmdlistinfo);
	else
		rc = _ce_f8_setup_direct(pce_dev, req, false, num_pkt,
			cipher_start, cipher_size);
	if (rc)
		goto bad;

	/* setup for callback, and issue command to sps */
	pce_dev->areq = cookie;
	pce_dev->qce_cb = qce_cb;

	/* Register producer callback event for DESC_DONE event. */
	pce_dev->ce_sps.producer.event.callback =
				_f8_sps_producer_callback;
	pce_dev->ce_sps.producer.event.options = SPS_O_DESC_DONE;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}
	_qce_sps_iovec_count_init(pce_dev);

	if (pce_dev->support_cmd_dscr)
		_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);

	_qce_sps_add_data((uint32_t)pce_dev->phy_ota_src, total,
					&pce_dev->ce_sps.in_transfer);
	_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

	_qce_sps_add_data((uint32_t)dst, total,
					&pce_dev->ce_sps.out_transfer);

	if (total > SPS_MAX_PKT_SIZE) {
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_IDLE;
	} else {
		pce_dev->ce_sps.producer_state = QCE_PIPE_STATE_COMP;
		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer);
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
							SPS_IOVEC_FLAG_INT);
	}
	rc = _qce_sps_transfer(pce_dev);

	if (rc == 0)
		return 0;
bad:
	if (pce_dev->phy_ota_dst)
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_dst, total,
				DMA_FROM_DEVICE);
	dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_src, total,
				(req->data_in == req->data_out) ?
				DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
	return rc;
}
EXPORT_SYMBOL(qce_f8_multi_pkt_req);

int qce_f9_req(void *handle, struct qce_f9_req *req, void *cookie,
			qce_comp_func_ptr_t qce_cb)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	int rc;
	struct qce_cmdlist_info *cmdlistinfo;

	switch (req->algorithm) {
	case QCE_OTA_ALGO_KASUMI:
		cmdlistinfo = &pce_dev->ce_sps.cmdlistptr.f9_kasumi;
		break;
	case QCE_OTA_ALGO_SNOW3G:
		cmdlistinfo = &pce_dev->ce_sps.cmdlistptr.f9_snow3g;
		break;
	default:
		return -EINVAL;
	};

	pce_dev->phy_ota_src = dma_map_single(pce_dev->pdev, req->message,
			req->msize, DMA_TO_DEVICE);

	pce_dev->ota_size = req->msize;

	if (pce_dev->support_cmd_dscr)
		rc = _ce_f9_setup(pce_dev, req, cmdlistinfo);
	else
		rc = _ce_f9_setup_direct(pce_dev, req);
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to sps */
	pce_dev->areq = cookie;
	pce_dev->qce_cb = qce_cb;

	/* Register producer callback event for DESC_DONE event. */
	pce_dev->ce_sps.producer.event.callback = _f9_sps_producer_callback;
	pce_dev->ce_sps.producer.event.options = SPS_O_DESC_DONE;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}

	_qce_sps_iovec_count_init(pce_dev);
	if (pce_dev->support_cmd_dscr)
		_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);
	_qce_sps_add_data((uint32_t)pce_dev->phy_ota_src, req->msize,
					&pce_dev->ce_sps.in_transfer);
	_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

	_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer);
	_qce_set_flag(&pce_dev->ce_sps.out_transfer, SPS_IOVEC_FLAG_INT);
	rc = _qce_sps_transfer(pce_dev);
	if (rc)
		goto bad;
	return 0;
bad:
	dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_src,
				req->msize, DMA_TO_DEVICE);
	return rc;
}
EXPORT_SYMBOL(qce_f9_req);

static int __qce_get_device_tree_data(struct platform_device *pdev,
		struct qce_device *pce_dev)
{
	struct resource *resource;
	int rc = 0;

	pce_dev->is_shared = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,ce-hw-shared");
	pce_dev->support_hw_key = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,ce-hw-key");

	pce_dev->use_sw_aes_cbc_ecb_ctr_algo =
				of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-sw-aes-cbc-ecb-ctr-algo");
	pce_dev->use_sw_aead_algo =
				of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-sw-aead-algo");
	pce_dev->use_sw_aes_xts_algo =
				of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-sw-aes-xts-algo");
	pce_dev->use_sw_ahash_algo =
				of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-sw-ahash-algo");
	pce_dev->use_sw_hmac_algo =
				of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-sw-hmac-algo");
	pce_dev->use_sw_aes_ccm_algo =
				of_property_read_bool((&pdev->dev)->of_node,
				"qcom,use-sw-aes-ccm-algo");
	pce_dev->support_clk_mgmt_sus_res = of_property_read_bool(
		(&pdev->dev)->of_node, "qcom,clk-mgmt-sus-res");
	pce_dev->support_only_core_src_clk = of_property_read_bool(
		(&pdev->dev)->of_node, "qcom,support-core-clk-only");

	if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,bam-pipe-pair",
				&pce_dev->ce_sps.pipe_pair_index)) {
		pr_err("Fail to get bam pipe pair information.\n");
		return -EINVAL;
	}
	if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ce-device",
				&pce_dev->ce_sps.ce_device)) {
		pr_err("Fail to get CE device information.\n");
		return -EINVAL;
	}
	if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ce-hw-instance",
				&pce_dev->ce_sps.ce_hw_instance)) {
		pr_err("Fail to get CE hw instance information.\n");
		return -EINVAL;
	}
	if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ce-opp-freq",
				&pce_dev->ce_opp_freq_hz)) {
		pr_info("CE operating frequency is not defined, setting to default 100MHZ\n");
		pce_dev->ce_opp_freq_hz = CE_CLK_100MHZ;
	}
	pce_dev->ce_sps.dest_pipe_index	= 2 * pce_dev->ce_sps.pipe_pair_index;
	pce_dev->ce_sps.src_pipe_index	= pce_dev->ce_sps.dest_pipe_index + 1;

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"crypto-base");
	if (resource) {
		pce_dev->phy_iobase = resource->start;
		pce_dev->iobase = ioremap_nocache(resource->start,
					resource_size(resource));
		if (!pce_dev->iobase) {
			pr_err("Can not map CRYPTO io memory\n");
			return -ENOMEM;
		}
	} else {
		pr_err("CRYPTO HW mem unavailable.\n");
		return -ENODEV;
	}

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"crypto-bam-base");
	if (resource) {
		pce_dev->bam_mem = resource->start;
		pce_dev->bam_mem_size = resource_size(resource);
	} else {
		pr_err("CRYPTO BAM mem unavailable.\n");
		rc = -ENODEV;
		goto err_getting_bam_info;
	}

	resource  = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (resource) {
		pce_dev->ce_sps.bam_irq = resource->start;
	} else {
		pr_err("CRYPTO BAM IRQ unavailable.\n");
		goto err_dev;
	}
	return rc;
err_dev:
	if (pce_dev->ce_sps.bam_iobase)
		iounmap(pce_dev->ce_sps.bam_iobase);

err_getting_bam_info:
	if (pce_dev->iobase)
		iounmap(pce_dev->iobase);

	return rc;
}

static int __qce_init_clk(struct qce_device *pce_dev)
{
	int rc = 0;

	pce_dev->ce_core_src_clk = clk_get(pce_dev->pdev, "core_clk_src");
	if (!IS_ERR(pce_dev->ce_core_src_clk)) {
		rc = clk_set_rate(pce_dev->ce_core_src_clk,
						pce_dev->ce_opp_freq_hz);
		if (rc) {
			pr_err("Unable to set the core src clk @%uMhz.\n",
					pce_dev->ce_opp_freq_hz/CE_CLK_DIV);
			goto exit_put_core_src_clk;
		}
	} else {
		if (pce_dev->support_only_core_src_clk) {
			rc = PTR_ERR(pce_dev->ce_core_src_clk);
			pce_dev->ce_core_src_clk = NULL;
			pr_err("Unable to get CE core src clk\n");
			return rc;
		} else {
			pr_warn("Unable to get CE core src clk, set to NULL\n");
			pce_dev->ce_core_src_clk = NULL;
		}
	}

	if (pce_dev->support_only_core_src_clk) {
		pce_dev->ce_core_clk = NULL;
		pce_dev->ce_clk = NULL;
		pce_dev->ce_bus_clk = NULL;
	} else {
		pce_dev->ce_core_clk = clk_get(pce_dev->pdev, "core_clk");
		if (IS_ERR(pce_dev->ce_core_clk)) {
			rc = PTR_ERR(pce_dev->ce_core_clk);
			pr_err("Unable to get CE core clk\n");
			goto exit_put_core_src_clk;
		}
		pce_dev->ce_clk = clk_get(pce_dev->pdev, "iface_clk");
		if (IS_ERR(pce_dev->ce_clk)) {
			rc = PTR_ERR(pce_dev->ce_clk);
			pr_err("Unable to get CE interface clk\n");
			goto exit_put_core_clk;
		}

		pce_dev->ce_bus_clk = clk_get(pce_dev->pdev, "bus_clk");
		if (IS_ERR(pce_dev->ce_bus_clk)) {
			rc = PTR_ERR(pce_dev->ce_bus_clk);
			pr_err("Unable to get CE BUS interface clk\n");
			goto exit_put_iface_clk;
		}
	}
	return rc;

exit_put_iface_clk:
	clk_put(pce_dev->ce_clk);
exit_put_core_clk:
	if (pce_dev->ce_core_clk)
		clk_put(pce_dev->ce_core_clk);
exit_put_core_src_clk:
	clk_put(pce_dev->ce_core_src_clk);
	pr_err("Unable to init CE clks, rc = %d\n", rc);
	return rc;
}

static void __qce_deinit_clk(struct qce_device *pce_dev)
{
	if (pce_dev->ce_bus_clk)
		clk_put(pce_dev->ce_bus_clk);
	if (pce_dev->ce_clk)
		clk_put(pce_dev->ce_clk);
	if (pce_dev->ce_core_clk)
		clk_put(pce_dev->ce_core_clk);
	if (pce_dev->ce_core_src_clk)
		clk_put(pce_dev->ce_core_src_clk);
}

int qce_enable_clk(void *handle)
{
	struct qce_device *pce_dev = (struct qce_device *)handle;
	int rc = 0;

	if (pce_dev->support_only_core_src_clk) {
		if (pce_dev->ce_core_src_clk)
			rc = clk_prepare_enable(pce_dev->ce_core_src_clk);
	} else {
		if (pce_dev->ce_core_clk)
			rc = clk_prepare_enable(pce_dev->ce_core_clk);
	}
	if (rc) {
		pr_err("Unable to enable/prepare CE core clk\n");
		return rc;
	}

	if (pce_dev->ce_clk) {
		rc = clk_prepare_enable(pce_dev->ce_clk);
		if (rc) {
			pr_err("Unable to enable/prepare CE iface clk\n");
			goto exit_disable_core_clk;
		}
	}

	if (pce_dev->ce_bus_clk) {
		rc = clk_prepare_enable(pce_dev->ce_bus_clk);
		if (rc) {
			pr_err("Unable to enable/prepare CE BUS clk\n");
			goto exit_disable_ce_clk;
		}
	}
	return rc;

exit_disable_ce_clk:
	clk_disable_unprepare(pce_dev->ce_clk);
exit_disable_core_clk:
	if (pce_dev->support_only_core_src_clk)
		clk_disable_unprepare(pce_dev->ce_core_src_clk);
	else
		clk_disable_unprepare(pce_dev->ce_core_clk);
	return rc;
}
EXPORT_SYMBOL(qce_enable_clk);

int qce_disable_clk(void *handle)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	int rc = 0;

	if (pce_dev->ce_bus_clk)
		clk_disable_unprepare(pce_dev->ce_bus_clk);
	if (pce_dev->ce_clk)
		clk_disable_unprepare(pce_dev->ce_clk);
	if (pce_dev->support_only_core_src_clk) {
		if (pce_dev->ce_core_src_clk)
			clk_disable_unprepare(pce_dev->ce_core_src_clk);
	} else {
		if (pce_dev->ce_core_clk)
			clk_disable_unprepare(pce_dev->ce_core_clk);
	}

	return rc;
}
EXPORT_SYMBOL(qce_disable_clk);

/* crypto engine open function. */
void *qce_open(struct platform_device *pdev, int *rc)
{
	struct qce_device *pce_dev;

	pce_dev = kzalloc(sizeof(struct qce_device), GFP_KERNEL);
	if (!pce_dev) {
		*rc = -ENOMEM;
		pr_err("Can not allocate memory: %d\n", *rc);
		return NULL;
	}
	pce_dev->pdev = &pdev->dev;

	mutex_lock(&qce_iomap_mutex);
	if (pdev->dev.of_node) {
		*rc = __qce_get_device_tree_data(pdev, pce_dev);
		if (*rc)
			goto err_pce_dev;
	} else {
		*rc = -EINVAL;
		pr_err("Device Node not found.\n");
		goto err_pce_dev;
	}

	pce_dev->memsize = 10 * PAGE_SIZE;
	pce_dev->coh_vmem = dma_alloc_coherent(pce_dev->pdev,
			pce_dev->memsize, &pce_dev->coh_pmem, GFP_KERNEL);
	if (pce_dev->coh_vmem == NULL) {
		*rc = -ENOMEM;
		pr_err("Can not allocate coherent memory for sps data\n");
		goto err_iobase;
	}

	*rc = __qce_init_clk(pce_dev);
	if (*rc)
		goto err_mem;

	*rc = qce_enable_clk(pce_dev);
	if (*rc)
		goto err_enable_clk;

	if (_probe_ce_engine(pce_dev)) {
		*rc = -ENXIO;
		goto err;
	}
	*rc = 0;

	qce_init_ce_cfg_val(pce_dev);
	*rc  = qce_sps_init(pce_dev);
	if (*rc)
		goto err;
	qce_setup_ce_sps_data(pce_dev);
	qce_disable_clk(pce_dev);
	mutex_unlock(&qce_iomap_mutex);
	return pce_dev;
err:
	qce_disable_clk(pce_dev);

err_enable_clk:
	__qce_deinit_clk(pce_dev);

err_mem:
	if (pce_dev->coh_vmem)
		dma_free_coherent(pce_dev->pdev, pce_dev->memsize,
			pce_dev->coh_vmem, pce_dev->coh_pmem);
err_iobase:
	if (pce_dev->iobase)
		iounmap(pce_dev->iobase);
err_pce_dev:
	mutex_unlock(&qce_iomap_mutex);
	kfree(pce_dev);
	return NULL;
}
EXPORT_SYMBOL(qce_open);

/* crypto engine close function. */
int qce_close(void *handle)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;

	if (handle == NULL)
		return -ENODEV;

	mutex_lock(&qce_iomap_mutex);
	qce_enable_clk(pce_dev);
	qce_sps_exit(pce_dev);

	if (pce_dev->iobase)
		iounmap(pce_dev->iobase);
	if (pce_dev->coh_vmem)
		dma_free_coherent(pce_dev->pdev, pce_dev->memsize,
				pce_dev->coh_vmem, pce_dev->coh_pmem);

	qce_disable_clk(pce_dev);
	__qce_deinit_clk(pce_dev);
	mutex_unlock(&qce_iomap_mutex);
	kfree(handle);

	return 0;
}
EXPORT_SYMBOL(qce_close);

#define OTA_SUPPORT_MASK (1 << CRYPTO_ENCR_SNOW3G_SEL |\
				1 << CRYPTO_ENCR_KASUMI_SEL |\
				1 << CRYPTO_AUTH_SNOW3G_SEL |\
				1 << CRYPTO_AUTH_KASUMI_SEL)

int qce_hw_support(void *handle, struct ce_hw_support *ce_support)
{
	struct qce_device *pce_dev = (struct qce_device *)handle;

	if (ce_support == NULL)
		return -EINVAL;

	ce_support->sha1_hmac_20 = false;
	ce_support->sha1_hmac = false;
	ce_support->sha256_hmac = false;
	ce_support->sha_hmac = true;
	ce_support->cmac  = true;
	ce_support->aes_key_192 = false;
	ce_support->aes_xts = true;
	if ((pce_dev->engines_avail & OTA_SUPPORT_MASK) == OTA_SUPPORT_MASK)
		ce_support->ota = true;
	else
		ce_support->ota = false;
	ce_support->bam = true;
	ce_support->is_shared = (pce_dev->is_shared == 1) ? true : false;
	ce_support->hw_key = pce_dev->support_hw_key;
	ce_support->aes_ccm = true;
	ce_support->clk_mgmt_sus_res = pce_dev->support_clk_mgmt_sus_res;
	if (pce_dev->ce_sps.minor_version)
		ce_support->aligned_only = false;
	else
		ce_support->aligned_only = true;

	ce_support->use_sw_aes_cbc_ecb_ctr_algo =
				pce_dev->use_sw_aes_cbc_ecb_ctr_algo;
	ce_support->use_sw_aead_algo =
				pce_dev->use_sw_aead_algo;
	ce_support->use_sw_aes_xts_algo =
				pce_dev->use_sw_aes_xts_algo;
	ce_support->use_sw_ahash_algo =
				pce_dev->use_sw_ahash_algo;
	ce_support->use_sw_hmac_algo =
				pce_dev->use_sw_hmac_algo;
	ce_support->use_sw_aes_ccm_algo =
				pce_dev->use_sw_aes_ccm_algo;
	ce_support->ce_device = pce_dev->ce_sps.ce_device;
	ce_support->ce_hw_instance = pce_dev->ce_sps.ce_hw_instance;
	return 0;
}
EXPORT_SYMBOL(qce_hw_support);


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto Engine driver");
