/* Qualcomm Crypto Engine driver.
 *
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/qcedev.h>
#include <linux/bitops.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <mach/dma.h>
#include <mach/clk.h>
#include <mach/socinfo.h>

#include "qce.h"
#include "qce50.h"
#include "qcryptohw_50.h"

#define CRYPTO_CONFIG_RESET 0xE001F

static DEFINE_MUTEX(bam_register_cnt);
struct bam_registration_info {
	uint32_t handle;
	uint32_t cnt;
};
static struct bam_registration_info bam_registry;

/*
 * CE HW device structure.
 * Each engine has an instance of the structure.
 * Each engine can only handle one crypto operation at one time. It is up to
 * the sw above to ensure single threading of operation on an engine.
 */
struct qce_device {
	struct device *pdev;        /* Handle to platform_device structure */

	unsigned char *coh_vmem;    /* Allocated coherent virtual memory */
	dma_addr_t coh_pmem;	    /* Allocated coherent physical memory */
	int memsize;				/* Memory allocated */

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

	void *areq;
	enum qce_cipher_mode_enum mode;
	struct ce_sps_data ce_sps;
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

	n = len  / sizeof(uint32_t) ;
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
				(((*(b+2)) << 8) & 0xff00)     ;
	} else if (n == 2) {
		*iv = ((*b << 24) & 0xff000000) |
				(((*(b+1)) << 16) & 0xff0000)   ;
	} else if (n == 1) {
		*iv = ((*b << 24) & 0xff000000) ;
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

	for (i = 0; nbytes > 0; i++, sg = sg_next(sg))
		nbytes -= sg->length;
	return i;
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

	if ((maj_rev != 0x05) || (min_rev > 0x02) || (step_rev > 0x02)) {
		pr_err("Unknown Qualcomm crypto device at 0x%x, rev %d.%d.%d\n",
			pce_dev->phy_iobase, maj_rev, min_rev, step_rev);
		return -EIO;
	};
	if ((min_rev > 0)  && (step_rev != 0)) {
		pr_err("Unknown Qualcomm crypto device at 0x%x, rev %d.%d.%d\n",
			pce_dev->phy_iobase, maj_rev, min_rev, step_rev);
		return -EIO;
	};
	pce_dev->ce_sps.minor_version = min_rev;

	dev_info(pce_dev->pdev, "Qualcomm Crypto %d.%d.%d device found @0x%x\n",
			maj_rev, min_rev, step_rev, pce_dev->phy_iobase);

	pce_dev->ce_sps.ce_burst_size = MAX_CE_BAM_BURST_SIZE;

	dev_info(pce_dev->pdev,
			"IO base, CE = 0x%x\n, "
			"Consumer (IN) PIPE %d,    "
			"Producer (OUT) PIPE %d\n"
			"IO base BAM = 0x%x\n"
			"BAM IRQ %d\n",
			(uint32_t) pce_dev->iobase,
			pce_dev->ce_sps.dest_pipe_index,
			pce_dev->ce_sps.src_pipe_index,
			(uint32_t)pce_dev->ce_sps.bam_iobase,
			pce_dev->ce_sps.bam_irq);
	return 0;
};

static int _ce_get_hash_cmdlistinfo(struct qce_device *pce_dev,
				struct qce_sha_req *sreq,
				struct qce_cmdlist_info **cmdplistinfo)
{
	struct qce_cmdlistptr_ops *cmdlistptr = &pce_dev->ce_sps.cmdlistptr;

	switch (sreq->alg) {
	case QCE_HASH_SHA1:
		*cmdplistinfo = &cmdlistptr->auth_sha1;
		break;

	case QCE_HASH_SHA256:
		*cmdplistinfo = &cmdlistptr->auth_sha256;
		break;

	case QCE_HASH_SHA1_HMAC:
		*cmdplistinfo = &cmdlistptr->auth_sha1_hmac;
			break;

	case QCE_HASH_SHA256_HMAC:
		*cmdplistinfo = &cmdlistptr->auth_sha256_hmac;
		break;

	case QCE_HASH_AES_CMAC:
		if (sreq->authklen == AES128_KEY_SIZE)
			*cmdplistinfo = &cmdlistptr->auth_aes_128_cmac;
		else
			*cmdplistinfo = &cmdlistptr->auth_aes_256_cmac;
		break;

	default:
		break;
	}
	return 0;
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

	if ((sreq->alg == QCE_HASH_SHA1_HMAC) ||
			(sreq->alg == QCE_HASH_SHA256_HMAC) ||
			(sreq->alg ==  QCE_HASH_AES_CMAC)) {
		uint32_t authk_size_in_word = sreq->authklen/sizeof(uint32_t);

		_byte_stream_to_net_words(mackey32, sreq->authkey,
						sreq->authklen);

		/* check for null key. If null, use hw key*/
		for (i = 0; i < authk_size_in_word; i++) {
			if (mackey32[i] != 0)
				break;
		}

		pce = cmdlistinfo->go_proc;
		if (i == authk_size_in_word) {
			pce->addr = (uint32_t)(CRYPTO_GOPROC_OEM_KEY_REG +
							pce_dev->phy_iobase);
		} else {
			pce->addr = (uint32_t)(CRYPTO_GOPROC_REG +
							pce_dev->phy_iobase);
			pce = cmdlistinfo->auth_key;
			for (i = 0; i < authk_size_in_word; i++, pce++)
				pce->data = mackey32[i];
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
	if (sreq->last_blk)
		pce->data |= 1 << CRYPTO_LAST;
	else
		pce->data &= ~(1 << CRYPTO_LAST);
	if (sreq->first_blk)
		pce->data |= 1 << CRYPTO_FIRST;
	else
		pce->data &= ~(1 << CRYPTO_FIRST);
go_proc:
	/* write auth seg size */
	pce = cmdlistinfo->auth_seg_size;
	pce->data = sreq->size;

	/* write auth seg size start*/
	pce = cmdlistinfo->auth_seg_start;
	pce->data = 0;

	/* write seg size */
	pce = cmdlistinfo->seg_size;
	pce->data = sreq->size;

	return 0;
}

static int _ce_get_cipher_cmdlistinfo(struct qce_device *pce_dev,
				struct qce_req *creq,
				struct qce_cmdlist_info **cmdlistinfo)
{
	struct qce_cmdlistptr_ops *cmdlistptr = &pce_dev->ce_sps.cmdlistptr;

	if (creq->alg != CIPHER_ALG_AES) {
		switch (creq->alg) {
		case CIPHER_ALG_DES:
			if (creq->mode ==  QCE_MODE_ECB)
				*cmdlistinfo = &cmdlistptr->cipher_des_ecb;
			else
				*cmdlistinfo = &cmdlistptr->cipher_des_cbc;
			break;

		case CIPHER_ALG_3DES:
			if (creq->mode ==  QCE_MODE_ECB)
				*cmdlistinfo =
					&cmdlistptr->cipher_3des_ecb;
			else
				*cmdlistinfo =
					&cmdlistptr->cipher_3des_cbc;
			break;
		default:
			break;
		}
	} else {
		switch (creq->mode) {
		case QCE_MODE_ECB:
			if (creq->encklen ==  AES128_KEY_SIZE)
				*cmdlistinfo = &cmdlistptr->cipher_aes_128_ecb;
			else
				*cmdlistinfo = &cmdlistptr->cipher_aes_256_ecb;
			break;

		case QCE_MODE_CBC:
		case QCE_MODE_CTR:
			if (creq->encklen ==  AES128_KEY_SIZE)
				*cmdlistinfo =
					&cmdlistptr->cipher_aes_128_cbc_ctr;
			else
				*cmdlistinfo =
					&cmdlistptr->cipher_aes_256_cbc_ctr;
			break;

		case QCE_MODE_XTS:
			if (creq->encklen ==  AES128_KEY_SIZE)
				*cmdlistinfo = &cmdlistptr->cipher_aes_128_xts;
			else
				*cmdlistinfo = &cmdlistptr->cipher_aes_256_xts;
			break;

		case QCE_MODE_CCM:
			if (creq->encklen ==  AES128_KEY_SIZE)
				*cmdlistinfo = &cmdlistptr->aead_aes_128_ccm;
			else
				*cmdlistinfo = &cmdlistptr->aead_aes_256_ccm;
			break;

		default:
			break;
		}
	}
	return 0;
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
	uint32_t encr_cfg = 0;
	uint32_t ivsize = creq->ivsize;
	int i;
	struct sps_command_element *pce = NULL;

	if (creq->mode == QCE_MODE_XTS)
		key_size = creq->encklen/2;
	else
		key_size = creq->encklen;

	_byte_stream_to_net_words(enckey32, creq->enckey, key_size);

	/* check for null key. If null, use hw key*/
	enck_size_in_word = key_size/sizeof(uint32_t);
	for (i = 0; i < enck_size_in_word; i++) {
		if (enckey32[i] != 0)
			break;
	}
	pce = cmdlistinfo->go_proc;
	if (i == enck_size_in_word) {
		use_hw_key = true;
		pce->addr = (uint32_t)(CRYPTO_GOPROC_OEM_KEY_REG +
						pce_dev->phy_iobase);
	} else {
		pce->addr = (uint32_t)(CRYPTO_GOPROC_REG +
						pce_dev->phy_iobase);
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

		/* TBD  NEW FEATURE partial AES CCM  pkt support set last bit */
		auth_cfg |= ((1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST));
		if (creq->dir == QCE_ENCRYPT)
			auth_cfg |= (CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
		else
			auth_cfg |= (CRYPTO_AUTH_POS_AFTER << CRYPTO_AUTH_POS);
		auth_cfg |= ((creq->authsize - 1) << CRYPTO_AUTH_SIZE);
		auth_cfg |= (CRYPTO_AUTH_MODE_CCM << CRYPTO_AUTH_MODE);
		if (creq->authklen ==  AES128_KEY_SIZE)
			auth_cfg |= (CRYPTO_AUTH_KEY_SZ_AES128 <<
						CRYPTO_AUTH_KEY_SIZE);
		else {
			if (creq->authklen ==  AES256_KEY_SIZE)
				auth_cfg |= (CRYPTO_AUTH_KEY_SZ_AES256 <<
							CRYPTO_AUTH_KEY_SIZE);
		}
		auth_cfg |= (CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG);
		auth_cfg |= ((MAX_NONCE/sizeof(uint32_t)) <<
						CRYPTO_AUTH_NONCE_NUM_WORDS);

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
		pce->data = totallen_in;
		pce = cmdlistinfo->auth_seg_start;
		pce->data = 0;
	}

	switch (creq->mode) {
	case QCE_MODE_ECB:
		encr_cfg |= (CRYPTO_ENCR_MODE_ECB << CRYPTO_ENCR_MODE);
		break;
	case QCE_MODE_CBC:
		encr_cfg |= (CRYPTO_ENCR_MODE_CBC << CRYPTO_ENCR_MODE);
		break;
	case QCE_MODE_XTS:
		encr_cfg |= (CRYPTO_ENCR_MODE_XTS << CRYPTO_ENCR_MODE);
		break;
	case QCE_MODE_CCM:
		encr_cfg |= (CRYPTO_ENCR_MODE_CCM << CRYPTO_ENCR_MODE);
		break;
	case QCE_MODE_CTR:
	default:
		encr_cfg |= (CRYPTO_ENCR_MODE_CTR << CRYPTO_ENCR_MODE);
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

			_byte_stream_to_net_words(xtskey32, (creq->enckey +
					creq->encklen/2), creq->encklen/2);
			/* write xts encr key */
			pce = cmdlistinfo->encr_xts_key;
			for (i = 0; i < xtsklen; i++, pce++)
				pce->data = xtskey32[i];

			/* write xts du size */
			pce = cmdlistinfo->encr_xts_du_size;
			pce->data = creq->cryptlen;
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
			encr_cfg |= CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG;
		} else {
			if (use_hw_key == false) {
				/* write encr key */
				pce = cmdlistinfo->encr_key;
				for (i = 0; i < enck_size_in_word; i++, pce++)
					pce->data = enckey32[i];
				switch (key_size) {
				case AES128_KEY_SIZE:
					encr_cfg |= (CRYPTO_ENCR_KEY_SZ_AES128
							 << CRYPTO_ENCR_KEY_SZ);
					break;
				case AES256_KEY_SIZE:
				default:
					encr_cfg |= (CRYPTO_ENCR_KEY_SZ_AES256
							 << CRYPTO_ENCR_KEY_SZ);
				break;
				} /* end of switch (creq->encklen) */
			}
			encr_cfg |= CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG;
		} /* else of if (creq->op == QCE_REQ_ABLK_CIPHER_NO_KEY) */
		break;
	} /* end of switch (creq->mode)  */

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

static int _qce_unlock_other_pipes(struct qce_device *pce_dev)
{
	int rc = 0;

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

	areq = (struct aead_request *) pce_dev->areq;
	if (areq->src != areq->dst) {
		dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
					DMA_FROM_DEVICE);
	}
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	dma_unmap_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
			DMA_TO_DEVICE);
	/* check MAC */
	memcpy(mac, (char *)(&pce_dev->ce_sps.result->auth_iv[0]),
						SHA256_DIGEST_SIZE);
	if (_qce_unlock_other_pipes(pce_dev))
		return -EINVAL;

	if (pce_dev->mode == QCE_MODE_CCM) {
		uint32_t result_status;
		result_status = pce_dev->ce_sps.result->status;
		result_status &= (1 << CRYPTO_MAC_FAILED);
		result_status |= (pce_dev->ce_sps.consumer_status |
					pce_dev->ce_sps.producer_status);
		pce_dev->qce_cb(areq, mac, NULL, result_status);
	} else {
		uint32_t ivsize = 0;
		struct crypto_aead *aead;
		unsigned char iv[NUM_OF_CRYPTO_CNTR_IV_REG * CRYPTO_REG_SIZE];

		aead = crypto_aead_reqtfm(areq);
		ivsize = crypto_aead_ivsize(aead);
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_iv_in,
				ivsize, DMA_TO_DEVICE);
		memcpy(iv, (char *)(pce_dev->ce_sps.result->encr_cntr_iv),
			sizeof(iv));
		pce_dev->qce_cb(areq, mac, iv, pce_dev->ce_sps.consumer_status |
			pce_dev->ce_sps.producer_status);

	}
	return 0;
};

static int _sha_complete(struct qce_device *pce_dev)
{
	struct ahash_request *areq;
	unsigned char digest[SHA256_DIGEST_SIZE];

	areq = (struct ahash_request *) pce_dev->areq;
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
				DMA_TO_DEVICE);
	memcpy(digest, (char *)(&pce_dev->ce_sps.result->auth_iv[0]),
						SHA256_DIGEST_SIZE);
	if (_qce_unlock_other_pipes(pce_dev))
		return -EINVAL;
	pce_dev->qce_cb(areq, digest,
			(char *)pce_dev->ce_sps.result->auth_byte_count,
				pce_dev->ce_sps.consumer_status);
	return 0;
};

static int _ablk_cipher_complete(struct qce_device *pce_dev)
{
	struct ablkcipher_request *areq;
	unsigned char iv[NUM_OF_CRYPTO_CNTR_IV_REG * CRYPTO_REG_SIZE];

	areq = (struct ablkcipher_request *) pce_dev->areq;

	if (areq->src != areq->dst) {
		dma_unmap_sg(pce_dev->pdev, areq->dst,
			pce_dev->dst_nents, DMA_FROM_DEVICE);
	}
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
		(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
						DMA_TO_DEVICE);
	if (_qce_unlock_other_pipes(pce_dev))
		return -EINVAL;

	if (pce_dev->mode == QCE_MODE_ECB) {
		pce_dev->qce_cb(areq, NULL, NULL,
					pce_dev->ce_sps.consumer_status |
					pce_dev->ce_sps.producer_status);
	} else {
		if (pce_dev->ce_sps.minor_version == 0) {
			if (pce_dev->mode == QCE_MODE_CBC)
				memcpy(iv, (char *)sg_virt(areq->src),
							sizeof(iv));

			if ((pce_dev->mode == QCE_MODE_CTR) ||
				(pce_dev->mode == QCE_MODE_XTS)) {
				uint32_t num_blk = 0;
				uint32_t cntr_iv = 0;

				memcpy(iv, areq->info, sizeof(iv));
				if (pce_dev->mode == QCE_MODE_CTR)
					num_blk = areq->nbytes/16;
				cntr_iv = (u32)(((u32)(*(iv + 14))) << 8) |
							(u32)(*(iv + 15));
				*(iv + 14) = (char)((cntr_iv + num_blk) >> 8);
				*(iv + 15) = (char)((cntr_iv + num_blk) & 0xFF);
			}
		} else {
			memcpy(iv,
				(char *)(pce_dev->ce_sps.result->encr_cntr_iv),
				sizeof(iv));
		}
		pce_dev->qce_cb(areq, NULL, iv,
			pce_dev->ce_sps.consumer_status |
			pce_dev->ce_sps.producer_status);
	}
	return 0;
};

#ifdef QCE_DEBUG
static void _qce_dump_descr_fifos(struct qce_device *pce_dev)
{
	int i, j, ents;
	struct sps_iovec *iovec = pce_dev->ce_sps.in_transfer.iovec;
	uint32_t cmd_flags = SPS_IOVEC_FLAG_CMD;

	printk(KERN_INFO "==============================================\n");
	printk(KERN_INFO "CONSUMER (TX/IN/DEST) PIPE DESCRIPTOR\n");
	printk(KERN_INFO "==============================================\n");
	for (i = 0; i <  pce_dev->ce_sps.in_transfer.iovec_count; i++) {
		printk(KERN_INFO " [%d] addr=0x%x  size=0x%x  flags=0x%x\n", i,
					iovec->addr, iovec->size, iovec->flags);
		if (iovec->flags & cmd_flags) {
			struct sps_command_element *pced;

			pced = (struct sps_command_element *)
					(GET_VIRT_ADDR(iovec->addr));
			ents = iovec->size/(sizeof(struct sps_command_element));
			for (j = 0; j < ents; j++) {
				printk(KERN_INFO "      [%d] [0x%x] 0x%x\n", j,
					pced->addr, pced->data);
				pced++;
			}
		}
		iovec++;
	}

	printk(KERN_INFO "==============================================\n");
	printk(KERN_INFO "PRODUCER (RX/OUT/SRC) PIPE DESCRIPTOR\n");
	printk(KERN_INFO "==============================================\n");
	iovec = pce_dev->ce_sps.out_transfer.iovec;
	for (i = 0; i <  pce_dev->ce_sps.out_transfer.iovec_count; i++) {
		printk(KERN_INFO " [%d] addr=0x%x  size=0x%x  flags=0x%x\n", i,
				iovec->addr, iovec->size, iovec->flags);
		iovec++;
	}
}

#else
static void _qce_dump_descr_fifos(struct qce_device *pce_dev)
{
}
#endif

static void _qce_sps_iovec_count_init(struct qce_device *pce_dev)
{
	pce_dev->ce_sps.in_transfer.iovec_count = 0;
	pce_dev->ce_sps.out_transfer.iovec_count = 0;
}

static void _qce_set_flag(struct sps_transfer *sps_bam_pipe, uint32_t flag)
{
	struct sps_iovec *iovec = sps_bam_pipe->iovec +
					(sps_bam_pipe->iovec_count - 1);
	iovec->flags |= flag;
}

static void _qce_sps_add_data(uint32_t addr, uint32_t len,
		struct sps_transfer *sps_bam_pipe)
{
	struct sps_iovec *iovec = sps_bam_pipe->iovec +
					sps_bam_pipe->iovec_count;
	if (len) {
		iovec->size = len;
		iovec->addr = addr;
		iovec->flags = 0;
		sps_bam_pipe->iovec_count++;
	}
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
		sg_src++;
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

	_qce_dump_descr_fifos(pce_dev);
	rc = sps_transfer(pce_dev->ce_sps.consumer.pipe,
					  &pce_dev->ce_sps.in_transfer);
	if (rc) {
		pr_err("sps_xfr() fail (consumer pipe=0x%x) rc = %d,",
				(u32)pce_dev->ce_sps.consumer.pipe, rc);
		return rc;
	}
	rc = sps_transfer(pce_dev->ce_sps.producer.pipe,
					  &pce_dev->ce_sps.out_transfer);
	if (rc) {
		pr_err("sps_xfr() fail (producer pipe=0x%x) rc = %d,",
				(u32)pce_dev->ce_sps.producer.pipe, rc);
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
		pr_err("sps_get_config() fail pipe_handle=0x%x, rc = %d\n",
			(u32)sps_pipe_info, rc);
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
			SPS_O_AUTO_ENABLE | SPS_O_EOT | SPS_O_DESC_DONE;
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
	sps_connect_info->desc.size = 512;
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
		pr_err("sps_connect() fail pipe_handle=0x%x, rc = %d\n",
			(u32)sps_pipe_info, rc);
		goto sps_connect_err;
	}

	sps_event->mode = SPS_TRIGGER_CALLBACK;
	if (is_producer)
		sps_event->options = SPS_O_EOT | SPS_O_DESC_DONE;
	else
		sps_event->options = SPS_O_EOT;
	sps_event->xfer_done = NULL;
	sps_event->user = (void *)pce_dev;

	pr_debug("success, %s : pipe_handle=0x%x, desc fifo base (phy) = 0x%x\n",
		is_producer ? "PRODUCER(RX/OUT)" : "CONSUMER(TX/IN)",
		(u32)sps_pipe_info, sps_connect_info->desc.phys_base);
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
	struct sps_bam_props bam = {0};
	bool register_bam = false;

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
	bam.manage = SPS_BAM_MGR_LOCAL;
	bam.ee = 1;

	pr_debug("bam physical base=0x%x\n", (u32)bam.phys_addr);
	pr_debug("bam virtual base=0x%x\n", (u32)bam.virt_addr);

	mutex_lock(&bam_register_cnt);
	if ((bam_registry.handle == 0) && (bam_registry.cnt == 0)) {
		/* Register CE Peripheral BAM device to SPS driver */
		rc = sps_register_bam_device(&bam, &bam_registry.handle);
		if (rc) {
			pr_err("sps_register_bam_device() failed! err=%d", rc);
			return -EIO;
		}
		bam_registry.cnt++;
		register_bam = true;
	} else {
		   bam_registry.cnt++;
	}
	mutex_unlock(&bam_register_cnt);
	pce_dev->ce_sps.bam_handle =  bam_registry.handle;
	pr_debug("BAM device registered. bam_handle=0x%x",
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
	if (register_bam)
		sps_deregister_bam_device(pce_dev->ce_sps.bam_handle);

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
	mutex_lock(&bam_register_cnt);
	if ((bam_registry.handle != 0) && (bam_registry.cnt == 1)) {
		sps_deregister_bam_device(pce_dev->ce_sps.bam_handle);
		bam_registry.cnt = 0;
		bam_registry.handle = 0;
	}
	if ((bam_registry.handle != 0) && (bam_registry.cnt > 1))
		bam_registry.cnt--;
	mutex_unlock(&bam_register_cnt);

	iounmap(pce_dev->ce_sps.bam_iobase);
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
	/* done */
	_aead_complete(pce_dev);
};

static void _aead_sps_consumer_callback(struct sps_event_notify *notify)
{
	struct qce_device *pce_dev = (struct qce_device *)
		((struct sps_event_notify *)notify)->user;

	pce_dev->ce_sps.notify = *notify;
	pr_debug("sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
			notify->event_id,
			notify->data.transfer.iovec.addr,
			notify->data.transfer.iovec.size,
			notify->data.transfer.iovec.flags);
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

static void _sha_sps_consumer_callback(struct sps_event_notify *notify)
{
	struct qce_device *pce_dev = (struct qce_device *)
		((struct sps_event_notify *)notify)->user;

	pce_dev->ce_sps.notify = *notify;
	pr_debug("sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
			notify->event_id,
			notify->data.transfer.iovec.addr,
			notify->data.transfer.iovec.size,
			notify->data.transfer.iovec.flags);
};

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
	/* done */
	_ablk_cipher_complete(pce_dev);
};

static void _ablk_cipher_sps_consumer_callback(struct sps_event_notify *notify)
{
	struct qce_device *pce_dev = (struct qce_device *)
		((struct sps_event_notify *)notify)->user;

	pce_dev->ce_sps.notify = *notify;
	pr_debug("sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
			notify->event_id,
			notify->data.transfer.iovec.addr,
			notify->data.transfer.iovec.size,
			notify->data.transfer.iovec.flags);
};

static void qce_add_cmd_element(struct qce_device *pdev,
			struct sps_command_element **cmd_ptr, u32 addr,
			u32 data, struct sps_command_element **populate)
{
	(*cmd_ptr)->addr = (uint32_t)(addr + pdev->phy_iobase);
	(*cmd_ptr)->data = data;
	(*cmd_ptr)->mask = 0xFFFFFFFF;
	if (populate != NULL)
		*populate = *cmd_ptr;
	(*cmd_ptr)++ ;
}

static int _setup_cipher_aes_cmdlistptrs(struct qce_device *pdev,
		unsigned char **pvaddr, enum qce_cipher_mode_enum mode,
		bool key_128)
{
	struct sps_command_element *ce_vaddr;
	uint32_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t encr_cfg = 0;
	uint32_t key_reg = 0;
	uint32_t xts_key_reg = 0;
	uint32_t iv_reg = 0;
	uint32_t crypto_cfg = 0;
	uint32_t beats = (pdev->ce_sps.ce_burst_size >> 3) - 1;
	uint32_t pipe_pair = pdev->ce_sps.pipe_pair_index;

	*pvaddr = (unsigned char *) ALIGN(((unsigned int)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	ce_vaddr_start = (uint32_t)(*pvaddr);
	crypto_cfg = (beats << CRYPTO_REQ_SIZE) |
			BIT(CRYPTO_MASK_DOUT_INTR) |
			BIT(CRYPTO_MASK_DIN_INTR) |
			BIT(CRYPTO_MASK_OP_DONE_INTR) |
			(0 << CRYPTO_HIGH_SPD_EN_N) |
			(pipe_pair << CRYPTO_PIPE_SET_SELECT);
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
							(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_128_cbc_ctr);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_AES128 <<
						CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_AES <<
						CRYPTO_ENCR_ALG);
			iv_reg = 4;
			key_reg = 4;
			xts_key_reg = 0;
		} else {
			cmdlistptr->cipher_aes_256_cbc_ctr.cmdlist =
							(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_256_cbc_ctr);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_AES256 <<
							CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_AES <<
							CRYPTO_ENCR_ALG);
			iv_reg = 4;
			key_reg = 8;
			xts_key_reg = 0;
		}
	break;
	case QCE_MODE_ECB:
		if (key_128 == true) {
			cmdlistptr->cipher_aes_128_ecb.cmdlist =
							(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_128_ecb);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_AES128 <<
						CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_AES <<
						CRYPTO_ENCR_ALG) |
					(CRYPTO_ENCR_MODE_ECB <<
						CRYPTO_ENCR_MODE);
			iv_reg = 0;
			key_reg = 4;
			xts_key_reg = 0;
		} else {
			cmdlistptr->cipher_aes_256_ecb.cmdlist =
							(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_256_ecb);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_AES256 <<
							CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_AES <<
							CRYPTO_ENCR_ALG) |
					(CRYPTO_ENCR_MODE_ECB <<
						CRYPTO_ENCR_MODE);
			iv_reg = 0;
			key_reg = 8;
			xts_key_reg = 0;
		}
	break;
	case QCE_MODE_XTS:
		if (key_128 == true) {
			cmdlistptr->cipher_aes_128_xts.cmdlist =
							(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_128_xts);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_AES128 <<
						CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_AES <<
						CRYPTO_ENCR_ALG) |
					(CRYPTO_ENCR_MODE_XTS <<
						CRYPTO_ENCR_MODE);
			iv_reg = 4;
			key_reg = 4;
			xts_key_reg = 4;
		} else {
			cmdlistptr->cipher_aes_256_xts.cmdlist =
							(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_aes_256_xts);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_AES256 <<
							CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_AES <<
							CRYPTO_ENCR_ALG) |
					(CRYPTO_ENCR_MODE_XTS <<
						CRYPTO_ENCR_MODE);
			iv_reg = 4;
			key_reg = 8;
			xts_key_reg = 8;
		}
	break;
	default:
	break;
	}

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG, crypto_cfg,
						&pcl_info->crypto_cfg);

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
				(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t)),
				0, NULL);
		qce_add_cmd_element(pdev, &ce_vaddr,
				CRYPTO_ENCR_XTS_DU_SIZE_REG, 0, NULL);
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
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
						0, &pcl_info->auth_seg_size);
	} else {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG,
						0, &pcl_info->auth_seg_size);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
						0, &pcl_info->auth_seg_size);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_START_REG,
						0, &pcl_info->auth_seg_size);
	}
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
				(crypto_cfg | CRYPTO_LITTLE_ENDIAN_MASK), NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uint32_t)ce_vaddr - (uint32_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_cipher_des_cmdlistptrs(struct qce_device *pdev,
		unsigned char **pvaddr, enum qce_cipher_alg_enum alg,
		bool mode_cbc)
{

	struct sps_command_element *ce_vaddr;
	uint32_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t encr_cfg = 0;
	uint32_t key_reg = 0;
	uint32_t iv_reg = 0;
	uint32_t crypto_cfg = 0;
	uint32_t beats = (pdev->ce_sps.ce_burst_size >> 3) - 1;
	uint32_t pipe_pair = pdev->ce_sps.pipe_pair_index;

	*pvaddr = (unsigned char *) ALIGN(((unsigned int)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	ce_vaddr_start = (uint32_t)(*pvaddr);
	crypto_cfg = (beats << CRYPTO_REQ_SIZE) |
			BIT(CRYPTO_MASK_DOUT_INTR) |
			BIT(CRYPTO_MASK_DIN_INTR) |
			BIT(CRYPTO_MASK_OP_DONE_INTR) |
			(0 << CRYPTO_HIGH_SPD_EN_N) |
			(pipe_pair << CRYPTO_PIPE_SET_SELECT);

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to cipher operations defined
	 * in ce_cmdlistptrs_ops structure.
	 */
	switch (alg) {
	case CIPHER_ALG_DES:
		if (mode_cbc) {
			cmdlistptr->cipher_des_cbc.cmdlist =
						(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_des_cbc);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_DES <<
						CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_DES <<
						CRYPTO_ENCR_ALG) |
					(CRYPTO_ENCR_MODE_CBC <<
							CRYPTO_ENCR_MODE);
			iv_reg = 2;
			key_reg = 2;
		} else {
			cmdlistptr->cipher_des_ecb.cmdlist =
						(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_des_ecb);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_DES <<
						CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_DES <<
						CRYPTO_ENCR_ALG) |
					(CRYPTO_ENCR_MODE_ECB <<
						CRYPTO_ENCR_MODE);
			iv_reg = 0;
			key_reg = 2;
		}
	break;
	case CIPHER_ALG_3DES:
		if (mode_cbc) {
			cmdlistptr->cipher_3des_cbc.cmdlist =
						(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_3des_cbc);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_3DES <<
						CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_DES <<
						CRYPTO_ENCR_ALG) |
					(CRYPTO_ENCR_MODE_CBC <<
							CRYPTO_ENCR_MODE);
			iv_reg = 2;
			key_reg = 6;
		} else {
			cmdlistptr->cipher_3des_ecb.cmdlist =
						(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->cipher_3des_ecb);

			encr_cfg = (CRYPTO_ENCR_KEY_SZ_3DES <<
						CRYPTO_ENCR_KEY_SZ) |
					(CRYPTO_ENCR_ALG_DES <<
						CRYPTO_ENCR_ALG) |
					(CRYPTO_ENCR_MODE_ECB <<
						CRYPTO_ENCR_MODE);
			iv_reg = 0;
			key_reg = 6;
		}
	break;
	default:
	break;
	}

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG, crypto_cfg,
						&pcl_info->crypto_cfg);

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
	/* Add dummy to  align size to burst-size multiple */
	if (!mode_cbc) {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG,
						0, &pcl_info->auth_seg_size);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
						0, &pcl_info->auth_seg_size);
	} else {
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_SIZE_REG,
						0, &pcl_info->auth_seg_size);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
						0, &pcl_info->auth_seg_size);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_START_REG,
						0, &pcl_info->auth_seg_size);
	}
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
			(crypto_cfg | CRYPTO_LITTLE_ENDIAN_MASK),
			NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uint32_t)ce_vaddr - (uint32_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_auth_cmdlistptrs(struct qce_device *pdev,
		unsigned char **pvaddr, enum qce_hash_alg_enum alg,
		bool key_128)
{
	struct sps_command_element *ce_vaddr;
	uint32_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t key_reg = 0;
	uint32_t auth_cfg = 0;
	uint32_t iv_reg = 0;
	uint32_t crypto_cfg = 0;
	uint32_t beats = (pdev->ce_sps.ce_burst_size >> 3) - 1;
	uint32_t pipe_pair = pdev->ce_sps.pipe_pair_index;

	*pvaddr = (unsigned char *) ALIGN(((unsigned int)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr_start = (uint32_t)(*pvaddr);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	crypto_cfg = (beats << CRYPTO_REQ_SIZE) |
			BIT(CRYPTO_MASK_DOUT_INTR) |
			BIT(CRYPTO_MASK_DIN_INTR) |
			BIT(CRYPTO_MASK_OP_DONE_INTR) |
			(0 << CRYPTO_HIGH_SPD_EN_N) |
			(pipe_pair << CRYPTO_PIPE_SET_SELECT);
	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to authentication operations
	 * defined in ce_cmdlistptrs_ops structure.
	 */
	switch (alg) {
	case QCE_HASH_SHA1:
		cmdlistptr->auth_sha1.cmdlist = (uint32_t)ce_vaddr;
		pcl_info = &(cmdlistptr->auth_sha1);

		auth_cfg = (CRYPTO_AUTH_MODE_HASH << CRYPTO_AUTH_MODE)|
				(CRYPTO_AUTH_SIZE_SHA1 << CRYPTO_AUTH_SIZE) |
				(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
				(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
		iv_reg = 5;
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					crypto_cfg, &pcl_info->crypto_cfg);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
							0, NULL);

	break;
	case QCE_HASH_SHA256:
		cmdlistptr->auth_sha256.cmdlist = (uint32_t)ce_vaddr;
		pcl_info = &(cmdlistptr->auth_sha256);

		auth_cfg = (CRYPTO_AUTH_MODE_HASH << CRYPTO_AUTH_MODE)|
				(CRYPTO_AUTH_SIZE_SHA256 << CRYPTO_AUTH_SIZE) |
				(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
				(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
		iv_reg = 8;
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					crypto_cfg, &pcl_info->crypto_cfg);
		/* 1 dummy write */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG,
								0, NULL);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
								0, NULL);
	break;
	case QCE_HASH_SHA1_HMAC:
		cmdlistptr->auth_sha1_hmac.cmdlist = (uint32_t)ce_vaddr;
		pcl_info = &(cmdlistptr->auth_sha1_hmac);

		auth_cfg = (CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE)|
				(CRYPTO_AUTH_SIZE_SHA1 << CRYPTO_AUTH_SIZE) |
				(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
				(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
		key_reg = 16;
		iv_reg = 5;
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					crypto_cfg, &pcl_info->crypto_cfg);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
							0, NULL);
	break;
	case QCE_AEAD_SHA1_HMAC:
		cmdlistptr->aead_sha1_hmac.cmdlist = (uint32_t)ce_vaddr;
		pcl_info = &(cmdlistptr->aead_sha1_hmac);

		auth_cfg = (CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE)|
				(CRYPTO_AUTH_SIZE_SHA1 << CRYPTO_AUTH_SIZE) |
				(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
				(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS) |
				(1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST);

		key_reg = 16;
		iv_reg = 5;
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					crypto_cfg, &pcl_info->crypto_cfg);
		/* 1 dummy write */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG,
								0, NULL);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
							0, NULL);
	break;
	case QCE_HASH_SHA256_HMAC:
		cmdlistptr->auth_sha256_hmac.cmdlist = (uint32_t)ce_vaddr;
		pcl_info = &(cmdlistptr->auth_sha256_hmac);

		auth_cfg = (CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE)|
				(CRYPTO_AUTH_SIZE_SHA256 << CRYPTO_AUTH_SIZE) |
				(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG) |
				(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
		key_reg = 16;
		iv_reg = 8;

		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					crypto_cfg, &pcl_info->crypto_cfg);
		/* 1 dummy write */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG,
								0, NULL);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
								0, NULL);
	break;
	case QCE_HASH_AES_CMAC:
		if (key_128 == true) {
			cmdlistptr->auth_aes_128_cmac.cmdlist =
						(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->auth_aes_128_cmac);

			auth_cfg = (1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST) |
				(CRYPTO_AUTH_MODE_CMAC << CRYPTO_AUTH_MODE)|
				(CRYPTO_AUTH_SIZE_ENUM_16_BYTES <<
							CRYPTO_AUTH_SIZE) |
				(CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG) |
				(CRYPTO_AUTH_KEY_SZ_AES128 <<
							CRYPTO_AUTH_KEY_SIZE) |
				(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
			key_reg = 4;
		} else {
			cmdlistptr->auth_aes_256_cmac.cmdlist =
							(uint32_t)ce_vaddr;
			pcl_info = &(cmdlistptr->auth_aes_256_cmac);

			auth_cfg = (1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST)|
				(CRYPTO_AUTH_MODE_CMAC << CRYPTO_AUTH_MODE)|
				(CRYPTO_AUTH_SIZE_ENUM_16_BYTES <<
							CRYPTO_AUTH_SIZE) |
				(CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG) |
				(CRYPTO_AUTH_KEY_SZ_AES256 <<
							CRYPTO_AUTH_KEY_SIZE) |
				(CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
			key_reg = 8;
		}
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					crypto_cfg, &pcl_info->crypto_cfg);
		/* 1 dummy write */
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG,
								0, NULL);
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_AUTH_SEG_CFG_REG,
								0, NULL);
	break;
	default:
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
			(crypto_cfg | CRYPTO_LITTLE_ENDIAN_MASK),
			NULL);

	if (alg != QCE_AEAD_SHA1_HMAC)
		qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uint32_t)ce_vaddr - (uint32_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_aead_cmdlistptrs(struct qce_device *pdev,
				unsigned char **pvaddr, bool key_128)
{
	struct sps_command_element *ce_vaddr;
	uint32_t ce_vaddr_start;
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;
	int i = 0;
	uint32_t encr_cfg = 0;
	uint32_t auth_cfg = 0;
	uint32_t key_reg = 0;
	uint32_t crypto_cfg = 0;
	uint32_t beats = (pdev->ce_sps.ce_burst_size >> 3) - 1;
	uint32_t pipe_pair = pdev->ce_sps.pipe_pair_index;

	*pvaddr = (unsigned char *) ALIGN(((unsigned int)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr_start = (uint32_t)(*pvaddr);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	crypto_cfg = (beats << CRYPTO_REQ_SIZE) |
			BIT(CRYPTO_MASK_DOUT_INTR) |
			BIT(CRYPTO_MASK_DIN_INTR) |
			BIT(CRYPTO_MASK_OP_DONE_INTR) |
			(0 << CRYPTO_HIGH_SPD_EN_N) |
			(pipe_pair << CRYPTO_PIPE_SET_SELECT);
	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to aead operations
	 * defined in ce_cmdlistptrs_ops structure.
	 */
	if (key_128 == true) {
		cmdlistptr->aead_aes_128_ccm.cmdlist = (uint32_t)ce_vaddr;
		pcl_info = &(cmdlistptr->aead_aes_128_ccm);

		auth_cfg = (1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST) |
			(CRYPTO_AUTH_MODE_CCM << CRYPTO_AUTH_MODE)|
			(CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG) |
			(CRYPTO_AUTH_KEY_SZ_AES128 << CRYPTO_AUTH_KEY_SIZE);
		auth_cfg &= ~(1 << CRYPTO_USE_HW_KEY_AUTH);
		encr_cfg = (CRYPTO_ENCR_KEY_SZ_AES128 << CRYPTO_ENCR_KEY_SZ) |
			(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
			((CRYPTO_ENCR_MODE_CCM << CRYPTO_ENCR_MODE));
		key_reg = 4;
	} else {

		cmdlistptr->aead_aes_256_ccm.cmdlist = (uint32_t)ce_vaddr;
		pcl_info = &(cmdlistptr->aead_aes_256_ccm);

		auth_cfg = (1 << CRYPTO_LAST) | (1 << CRYPTO_FIRST) |
			(CRYPTO_AUTH_MODE_CCM << CRYPTO_AUTH_MODE)|
			(CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG) |
			(CRYPTO_AUTH_KEY_SZ_AES256 << CRYPTO_AUTH_KEY_SIZE) |
			((MAX_NONCE/sizeof(uint32_t)) <<
						CRYPTO_AUTH_NONCE_NUM_WORDS);
		auth_cfg &= ~(1 << CRYPTO_USE_HW_KEY_AUTH);
		encr_cfg = (CRYPTO_ENCR_KEY_SZ_AES256 << CRYPTO_ENCR_KEY_SZ) |
			(CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG) |
			((CRYPTO_ENCR_MODE_CCM << CRYPTO_ENCR_MODE));
		key_reg = 8;
	}
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
				crypto_cfg, &pcl_info->crypto_cfg);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_ENCR_SEG_SIZE_REG, 0, NULL);
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
			(crypto_cfg | CRYPTO_LITTLE_ENDIAN_MASK),
			NULL);

	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_GOPROC_REG,
			((1 << CRYPTO_GO) | (1 << CRYPTO_RESULTS_DUMP)),
			&pcl_info->go_proc);

	pcl_info->size = (uint32_t)ce_vaddr - (uint32_t)ce_vaddr_start;
	*pvaddr = (unsigned char *) ce_vaddr;

	return 0;
}

static int _setup_unlock_pipe_cmdlistptrs(struct qce_device *pdev,
		unsigned char **pvaddr)
{
	struct sps_command_element *ce_vaddr;
	uint32_t ce_vaddr_start = (uint32_t)(*pvaddr);
	struct qce_cmdlistptr_ops *cmdlistptr = &pdev->ce_sps.cmdlistptr;
	struct qce_cmdlist_info *pcl_info = NULL;

	*pvaddr = (unsigned char *) ALIGN(((unsigned int)(*pvaddr)),
					pdev->ce_sps.ce_burst_size);
	ce_vaddr = (struct sps_command_element *)(*pvaddr);
	cmdlistptr->unlock_all_pipes.cmdlist = (uint32_t)ce_vaddr;
	pcl_info = &(cmdlistptr->unlock_all_pipes);

	/*
	 * Designate chunks of the allocated memory to command list
	 * to unlock pipes.
	 */
	qce_add_cmd_element(pdev, &ce_vaddr, CRYPTO_CONFIG_REG,
					CRYPTO_CONFIG_RESET, NULL);
	pcl_info->size = (uint32_t)ce_vaddr - (uint32_t)ce_vaddr_start;
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
		(struct sps_command_element *) ALIGN(((unsigned int) ce_vaddr),
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

	_setup_auth_cmdlistptrs(pdev, pvaddr, QCE_AEAD_SHA1_HMAC, false);

	_setup_aead_cmdlistptrs(pdev, pvaddr, true);
	_setup_aead_cmdlistptrs(pdev, pvaddr, false);
	_setup_unlock_pipe_cmdlistptrs(pdev, pvaddr);

	return 0;
}

static int qce_setup_ce_sps_data(struct qce_device *pce_dev)
{
	unsigned char *vaddr;

	vaddr = pce_dev->coh_vmem;
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr),
					pce_dev->ce_sps.ce_burst_size);
	/* Allow for 256 descriptor (cmd and data) entries per pipe */
	pce_dev->ce_sps.in_transfer.iovec = (struct sps_iovec *)vaddr;
	pce_dev->ce_sps.in_transfer.iovec_phys =
					(uint32_t)GET_PHYS_ADDR(vaddr);
	vaddr += MAX_BAM_DESCRIPTORS * 8;

	pce_dev->ce_sps.out_transfer.iovec = (struct sps_iovec *)vaddr;
	pce_dev->ce_sps.out_transfer.iovec_phys =
					(uint32_t)GET_PHYS_ADDR(vaddr);
	vaddr += MAX_BAM_DESCRIPTORS * 8;

	qce_setup_cmdlistptrs(pce_dev, &vaddr);
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr),
					pce_dev->ce_sps.ce_burst_size);
	pce_dev->ce_sps.result_dump = (uint32_t)vaddr;
	pce_dev->ce_sps.result = (struct ce_result_dump_format *)vaddr;
	vaddr += 128;

	return 0;
}

int qce_aead_sha1_hmac_setup(struct qce_req *creq, struct crypto_aead *aead,
				struct qce_cmdlist_info *cmdlistinfo)
{
	uint32_t authk_size_in_word = creq->authklen/sizeof(uint32_t);
	uint32_t mackey32[SHA_HMAC_KEY_SIZE/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	struct sps_command_element *pce = NULL;
	struct aead_request *areq = (struct aead_request *)creq->areq;
	int i;

	_byte_stream_to_net_words(mackey32, creq->authkey,
					creq->authklen);
	pce = cmdlistinfo->auth_key;
	for (i = 0; i < authk_size_in_word; i++, pce++)
		pce->data = mackey32[i];
	pce = cmdlistinfo->auth_iv;
	for (i = 0; i < 5; i++, pce++)
		pce->data = _std_init_vector_sha1[i];
	/* write auth seg size */
	pce = cmdlistinfo->auth_seg_size;
	pce->data = creq->cryptlen + areq->assoclen + crypto_aead_ivsize(aead);

	/* write auth seg size start*/
	pce = cmdlistinfo->auth_seg_start;
	pce->data = 0;

	return 0;
}

int qce_aead_req(void *handle, struct qce_req *q_req)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	struct aead_request *areq = (struct aead_request *) q_req->areq;
	uint32_t authsize = q_req->authsize;
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	uint32_t ivsize = 0;
	uint32_t totallen_in, out_len;
	uint32_t hw_pad_out = 0;
	int rc = 0;
	int ce_burst_size;
	struct qce_cmdlist_info *cmdlistinfo = NULL;
	struct qce_cmdlist_info *auth_cmdlistinfo = NULL;

	if (q_req->mode != QCE_MODE_CCM)
		ivsize = crypto_aead_ivsize(aead);

	ce_burst_size = pce_dev->ce_sps.ce_burst_size;
	if (q_req->dir == QCE_ENCRYPT) {
		q_req->cryptlen = areq->cryptlen;
			totallen_in = q_req->cryptlen + areq->assoclen + ivsize;
		if (q_req->mode == QCE_MODE_CCM) {
			out_len = areq->cryptlen + authsize;
			hw_pad_out = ALIGN(authsize, ce_burst_size) - authsize;
		} else {
			out_len = areq->cryptlen;
		}
	} else {
		q_req->cryptlen = areq->cryptlen - authsize;
		if (q_req->mode == QCE_MODE_CCM)
			totallen_in = areq->cryptlen + areq->assoclen;
		else
			totallen_in = q_req->cryptlen + areq->assoclen + ivsize;
		out_len = q_req->cryptlen;
		hw_pad_out = authsize;
	}

	pce_dev->assoc_nents = count_sg(areq->assoc, areq->assoclen);
	pce_dev->src_nents = count_sg(areq->src, areq->cryptlen);
	pce_dev->ivsize = q_req->ivsize;
	pce_dev->authsize = q_req->authsize;
	pce_dev->phy_iv_in = 0;

	/* associated data input */
	dma_map_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
					 DMA_TO_DEVICE);
	/* cipher input */
	dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	/* cipher + mac output  for encryption    */
	if (areq->src != areq->dst) {
		pce_dev->dst_nents = count_sg(areq->dst, out_len);
		dma_map_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
	} else {
		pce_dev->dst_nents = pce_dev->src_nents;
	}

	_ce_get_cipher_cmdlistinfo(pce_dev, q_req, &cmdlistinfo);
	/* set up crypto device */
	rc = _ce_setup_cipher(pce_dev, q_req, totallen_in,
				areq->assoclen + ivsize, cmdlistinfo);
	if (rc < 0)
		goto bad;

	if (q_req->mode != QCE_MODE_CCM) {
		rc = qce_aead_sha1_hmac_setup(q_req, aead, auth_cmdlistinfo);
		if (rc < 0)
			goto bad;
		/* overwrite seg size */
		cmdlistinfo->seg_size->data = totallen_in;
		/* cipher iv for input */
		pce_dev->phy_iv_in = dma_map_single(pce_dev->pdev, q_req->iv,
			ivsize, DMA_TO_DEVICE);
	}

	/* setup for callback, and issue command to bam */
	pce_dev->areq = q_req->areq;
	pce_dev->qce_cb = q_req->qce_cb;

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.producer.event.callback = _aead_sps_producer_callback;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.consumer.event.callback = _aead_sps_consumer_callback;
	rc = sps_register_event(pce_dev->ce_sps.consumer.pipe,
					&pce_dev->ce_sps.consumer.event);
	if (rc) {
		pr_err("Consumer callback registration failed rc = %d\n", rc);
		goto bad;
	}

	_qce_sps_iovec_count_init(pce_dev);

	_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);

	if (pce_dev->ce_sps.minor_version == 0) {
		_qce_sps_add_sg_data(pce_dev, areq->src, totallen_in,
					&pce_dev->ce_sps.in_transfer);

		_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

		_qce_sps_add_sg_data(pce_dev, areq->dst, out_len +
					areq->assoclen + hw_pad_out,
				&pce_dev->ce_sps.out_transfer);
		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					&pce_dev->ce_sps.out_transfer);
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
					SPS_IOVEC_FLAG_INT);
	} else {
		_qce_sps_add_sg_data(pce_dev, areq->assoc, areq->assoclen,
					 &pce_dev->ce_sps.in_transfer);
		_qce_sps_add_data((uint32_t)pce_dev->phy_iv_in, ivsize,
					&pce_dev->ce_sps.in_transfer);
		_qce_sps_add_sg_data(pce_dev, areq->src, areq->cryptlen,
					&pce_dev->ce_sps.in_transfer);
		_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

		/* Pass through to ignore associated (+iv, if applicable) data*/
		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.ignore_buffer),
				(ivsize + areq->assoclen),
				&pce_dev->ce_sps.out_transfer);
		_qce_sps_add_sg_data(pce_dev, areq->dst, out_len,
					&pce_dev->ce_sps.out_transfer);
		/* Pass through to ignore hw_pad (padding of the MAC data) */
		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.ignore_buffer),
				hw_pad_out, &pce_dev->ce_sps.out_transfer);

		_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
			CRYPTO_RESULT_DUMP_SIZE, &pce_dev->ce_sps.out_transfer);
		_qce_set_flag(&pce_dev->ce_sps.out_transfer,
					SPS_IOVEC_FLAG_INT);
	}
	rc = _qce_sps_transfer(pce_dev);
	if (rc)
		goto bad;
	return 0;

bad:
	if (pce_dev->assoc_nents) {
		dma_unmap_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
				DMA_TO_DEVICE);
	}
	if (pce_dev->src_nents) {
		dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
				(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
								DMA_TO_DEVICE);
	}
	if (areq->src != areq->dst) {
		dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
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
	_ce_get_cipher_cmdlistinfo(pce_dev, c_req, &cmdlistinfo);

	/* cipher input */
	pce_dev->src_nents = count_sg(areq->src, areq->nbytes);

	dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
		(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	/* cipher output */
	if (areq->src != areq->dst) {
		pce_dev->dst_nents = count_sg(areq->dst, areq->nbytes);
			dma_map_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
							DMA_FROM_DEVICE);
	} else {
		pce_dev->dst_nents = pce_dev->src_nents;
	}
	/* set up crypto device */
	rc = _ce_setup_cipher(pce_dev, c_req, areq->nbytes, 0, cmdlistinfo);
	if (rc < 0)
		goto bad;

	/* setup for client callback, and issue command to BAM */
	pce_dev->areq = areq;
	pce_dev->qce_cb = c_req->qce_cb;

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.producer.event.callback =
				_ablk_cipher_sps_producer_callback;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.consumer.event.callback =
			_ablk_cipher_sps_consumer_callback;
	rc = sps_register_event(pce_dev->ce_sps.consumer.pipe,
					&pce_dev->ce_sps.consumer.event);
	if (rc) {
		pr_err("Consumer callback registration failed rc = %d\n", rc);
		goto bad;
	}

	_qce_sps_iovec_count_init(pce_dev);

	_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);
	_qce_sps_add_sg_data(pce_dev, areq->src, areq->nbytes,
					&pce_dev->ce_sps.in_transfer);
	_qce_set_flag(&pce_dev->ce_sps.in_transfer,
				SPS_IOVEC_FLAG_EOT|SPS_IOVEC_FLAG_NWD);

	_qce_sps_add_sg_data(pce_dev, areq->dst, areq->nbytes,
					&pce_dev->ce_sps.out_transfer);
	_qce_sps_add_data(GET_PHYS_ADDR(pce_dev->ce_sps.result_dump),
					CRYPTO_RESULT_DUMP_SIZE,
					  &pce_dev->ce_sps.out_transfer);
	_qce_set_flag(&pce_dev->ce_sps.out_transfer, SPS_IOVEC_FLAG_INT);
	rc = _qce_sps_transfer(pce_dev);
	if (rc)
		goto bad;
		return 0;
bad:
	if (pce_dev->dst_nents) {
		dma_unmap_sg(pce_dev->pdev, areq->dst,
		pce_dev->dst_nents, DMA_FROM_DEVICE);
	}
	if (pce_dev->src_nents) {
		dma_unmap_sg(pce_dev->pdev, areq->src,
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
	_ce_get_hash_cmdlistinfo(pce_dev, sreq, &cmdlistinfo);
	dma_map_sg(pce_dev->pdev, sreq->src, pce_dev->src_nents,
							DMA_TO_DEVICE);
	rc = _ce_setup_hash(pce_dev, sreq, cmdlistinfo);
	if (rc < 0)
		goto bad;

	pce_dev->areq = areq;
	pce_dev->qce_cb = sreq->qce_cb;

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.producer.event.callback = _sha_sps_producer_callback;
	rc = sps_register_event(pce_dev->ce_sps.producer.pipe,
					&pce_dev->ce_sps.producer.event);
	if (rc) {
		pr_err("Producer callback registration failed rc = %d\n", rc);
		goto bad;
	}

	/* Register callback event for EOT (End of transfer) event. */
	pce_dev->ce_sps.consumer.event.callback = _sha_sps_consumer_callback;
	rc = sps_register_event(pce_dev->ce_sps.consumer.pipe,
					&pce_dev->ce_sps.consumer.event);
	if (rc) {
		pr_err("Consumer callback registration failed rc = %d\n", rc);
		goto bad;
	}

	_qce_sps_iovec_count_init(pce_dev);

	_qce_sps_add_cmd(pce_dev, SPS_IOVEC_FLAG_LOCK, cmdlistinfo,
					&pce_dev->ce_sps.in_transfer);
	_qce_sps_add_sg_data(pce_dev, areq->src, areq->nbytes,
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
	if (pce_dev->src_nents) {
		dma_unmap_sg(pce_dev->pdev, sreq->src,
				pce_dev->src_nents, DMA_TO_DEVICE);
	}
	return rc;
}
EXPORT_SYMBOL(qce_process_sha_req);

static int __qce_get_device_tree_data(struct platform_device *pdev,
		struct qce_device *pce_dev)
{
	struct resource *resource;
	int rc = 0;

	if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,bam-pipe-pair",
				&pce_dev->ce_sps.pipe_pair_index)) {
		pr_err("Fail to get bam pipe pair information.\n");
		return -EINVAL;
	} else {
		pr_warn("bam_pipe_pair=0x%x", pce_dev->ce_sps.pipe_pair_index);
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
	pr_warn("ce_phy_reg_base=0x%x  ", pce_dev->phy_iobase);
	pr_warn("ce_virt_reg_base=0x%x\n", (uint32_t)pce_dev->iobase);

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"crypto-bam-base");
	if (resource) {
		pce_dev->ce_sps.bam_mem = resource->start;
		pce_dev->ce_sps.bam_iobase = ioremap_nocache(resource->start,
					resource_size(resource));
		if (!pce_dev->iobase) {
			rc = -ENOMEM;
			pr_err("Can not map BAM io memory\n");
			goto err_getting_bam_info;
		}
	} else {
		pr_err("CRYPTO BAM mem unavailable.\n");
		rc = -ENODEV;
		goto err_getting_bam_info;
	}
	pr_warn("ce_bam_phy_reg_base=0x%x  ", pce_dev->ce_sps.bam_mem);
	pr_warn("ce_bam_virt_reg_base=0x%x\n",
				(uint32_t)pce_dev->ce_sps.bam_iobase);

	resource  = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (resource) {
		pce_dev->ce_sps.bam_irq = resource->start;
		pr_warn("CRYPTO BAM IRQ = %d.\n", pce_dev->ce_sps.bam_irq);
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
	struct clk *ce_core_clk;
	struct clk *ce_clk;
	struct clk *ce_core_src_clk;
	struct clk *ce_bus_clk;

	/* Get CE3 src core clk. */
	ce_core_src_clk = clk_get(pce_dev->pdev, "core_clk_src");
	if (!IS_ERR(ce_core_src_clk)) {
		pce_dev->ce_core_src_clk = ce_core_src_clk;

		/* Set the core src clk @100Mhz */
		rc = clk_set_rate(pce_dev->ce_core_src_clk, 100000000);
		if (rc) {
			clk_put(pce_dev->ce_core_src_clk);
			pr_err("Unable to set the core src clk @100Mhz.\n");
			goto err_clk;
		}
	} else {
		pr_warn("Unable to get CE core src clk, set to NULL\n");
		pce_dev->ce_core_src_clk = NULL;
	}

	/* Get CE core clk */
	ce_core_clk = clk_get(pce_dev->pdev, "core_clk");
	if (IS_ERR(ce_core_clk)) {
		rc = PTR_ERR(ce_core_clk);
		pr_err("Unable to get CE core clk\n");
		if (pce_dev->ce_core_src_clk != NULL)
			clk_put(pce_dev->ce_core_src_clk);
		goto err_clk;
	}
	pce_dev->ce_core_clk = ce_core_clk;

	/* Get CE Interface clk */
	ce_clk = clk_get(pce_dev->pdev, "iface_clk");
	if (IS_ERR(ce_clk)) {
		rc = PTR_ERR(ce_clk);
		pr_err("Unable to get CE interface clk\n");
		if (pce_dev->ce_core_src_clk != NULL)
			clk_put(pce_dev->ce_core_src_clk);
		clk_put(pce_dev->ce_core_clk);
		goto err_clk;
	}
	pce_dev->ce_clk = ce_clk;

	/* Get CE AXI clk */
	ce_bus_clk = clk_get(pce_dev->pdev, "bus_clk");
	if (IS_ERR(ce_bus_clk)) {
		rc = PTR_ERR(ce_bus_clk);
		pr_err("Unable to get CE BUS interface clk\n");
		if (pce_dev->ce_core_src_clk != NULL)
			clk_put(pce_dev->ce_core_src_clk);
		clk_put(pce_dev->ce_core_clk);
		clk_put(pce_dev->ce_clk);
		goto err_clk;
	}
	pce_dev->ce_bus_clk = ce_bus_clk;

	/* Enable CE core clk */
	rc = clk_prepare_enable(pce_dev->ce_core_clk);
	if (rc) {
		pr_err("Unable to enable/prepare CE core clk\n");
		if (pce_dev->ce_core_src_clk != NULL)
			clk_put(pce_dev->ce_core_src_clk);
		clk_put(pce_dev->ce_core_clk);
		clk_put(pce_dev->ce_clk);
		goto err_clk;
	} else {
		/* Enable CE clk */
		rc = clk_prepare_enable(pce_dev->ce_clk);
		if (rc) {
			pr_err("Unable to enable/prepare CE iface clk\n");
			clk_disable_unprepare(pce_dev->ce_core_clk);
			if (pce_dev->ce_core_src_clk != NULL)
				clk_put(pce_dev->ce_core_src_clk);
			clk_put(pce_dev->ce_core_clk);
			clk_put(pce_dev->ce_clk);
			goto err_clk;
		}
		/* Enable AXI clk */
		rc = clk_prepare_enable(pce_dev->ce_bus_clk);
		if (rc) {
			pr_err("Unable to enable/prepare CE BUS clk\n");
			clk_disable_unprepare(pce_dev->ce_core_clk);
			if (pce_dev->ce_core_src_clk != NULL)
				clk_put(pce_dev->ce_core_src_clk);
			clk_put(pce_dev->ce_core_clk);
			clk_put(pce_dev->ce_clk);
			clk_put(pce_dev->ce_bus_clk);
			goto err_clk;
		}
	}
err_clk:
	if (rc)
		pr_err("Unable to init CE clks, rc = %d\n", rc);
	return rc;
}

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

	if (pdev->dev.of_node) {
		*rc = __qce_get_device_tree_data(pdev, pce_dev);
		if (*rc)
			goto err_pce_dev;
	} else {
		*rc = -EINVAL;
		pr_err("Device Node not found.\n");
		goto err_pce_dev;
	}

	pce_dev->memsize = 9 * PAGE_SIZE;
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

	if (_probe_ce_engine(pce_dev)) {
		*rc = -ENXIO;
		goto err;
	}
	*rc = 0;
	qce_setup_ce_sps_data(pce_dev);
	qce_sps_init(pce_dev);

	return pce_dev;
err:
	clk_disable_unprepare(pce_dev->ce_clk);
	clk_disable_unprepare(pce_dev->ce_core_clk);

	if (pce_dev->ce_core_src_clk != NULL)
		clk_put(pce_dev->ce_core_src_clk);
	clk_put(pce_dev->ce_clk);
	clk_put(pce_dev->ce_core_clk);
err_mem:
	if (pce_dev->coh_vmem)
		dma_free_coherent(pce_dev->pdev, pce_dev->memsize,
			pce_dev->coh_vmem, pce_dev->coh_pmem);
err_iobase:
	if (pce_dev->ce_sps.bam_iobase)
		iounmap(pce_dev->ce_sps.bam_iobase);
	if (pce_dev->iobase)
		iounmap(pce_dev->iobase);
err_pce_dev:
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

	if (pce_dev->iobase)
		iounmap(pce_dev->iobase);
	if (pce_dev->coh_vmem)
		dma_free_coherent(pce_dev->pdev, pce_dev->memsize,
				pce_dev->coh_vmem, pce_dev->coh_pmem);

	clk_disable_unprepare(pce_dev->ce_clk);
	clk_disable_unprepare(pce_dev->ce_core_clk);
	clk_disable_unprepare(pce_dev->ce_bus_clk);
	if (pce_dev->ce_core_src_clk != NULL)
		clk_put(pce_dev->ce_core_src_clk);
	clk_put(pce_dev->ce_clk);
	clk_put(pce_dev->ce_core_clk);
	clk_put(pce_dev->ce_bus_clk);

	qce_sps_exit(pce_dev);
	kfree(handle);

	return 0;
}
EXPORT_SYMBOL(qce_close);

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
	ce_support->ota = false;
	ce_support->bam = true;
	if (pce_dev->ce_sps.minor_version) {
		ce_support->aligned_only = false;
		ce_support->aes_ccm = true;
	} else {
		ce_support->aligned_only = true;
		ce_support->aes_ccm = false;
	}
	return 0;
}
EXPORT_SYMBOL(qce_hw_support);

static int __init qce_init(void)
{
	bam_registry.handle = 0;
	bam_registry.cnt = 0;
	return 0;
}

static void __exit qce_exit(void)
{
	bam_registry.handle = 0;
	bam_registry.cnt = 0;
}

module_init(qce_init);
module_exit(qce_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto Engine driver");
