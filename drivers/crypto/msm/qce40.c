/* Qualcomm Crypto Engine driver.
 *
 * Copyright (c) 2011 - 2013, The Linux Foundation. All rights reserved.
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
#include "qce40.h"
#include "qcryptohw_40.h"

/* ADM definitions */
#define LI_SG_CMD  (1 << 31)    /* last index in the scatter gather cmd */
#define SRC_INDEX_SG_CMD(index) ((index & 0x3fff) << 16)
#define DST_INDEX_SG_CMD(index) (index & 0x3fff)
#define ADM_DESC_LAST  (1 << 31)
#define QCE_FIFO_SIZE  0x8000

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

	qce_comp_func_ptr_t qce_cb;	/* qce callback function pointer */

	int assoc_nents;
	int ivsize;
	int authsize;
	int src_nents;
	int dst_nents;

	void *areq;
	enum qce_cipher_mode_enum mode;
	struct ce_dm_data ce_dm;
};

/* Standard initialization vector for SHA-1, source: FIPS 180-2 */
static uint8_t  _std_init_vector_sha1_uint8[] =   {
	0x67, 0x45, 0x23, 0x01, 0xEF, 0xCD, 0xAB, 0x89,
	0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x32, 0x54, 0x76,
	0xC3, 0xD2, 0xE1, 0xF0
};

/* Standard initialization vector for SHA-256, source: FIPS 180-2 */
static uint8_t _std_init_vector_sha256_uint8[] = {
	0x6A, 0x09, 0xE6, 0x67, 0xBB, 0x67, 0xAE, 0x85,
	0x3C, 0x6E, 0xF3, 0x72, 0xA5, 0x4F, 0xF5, 0x3A,
	0x51, 0x0E, 0x52, 0x7F, 0x9B, 0x05, 0x68, 0x8C,
	0x1F, 0x83, 0xD9, 0xAB, 0x5B, 0xE0, 0xCD, 0x19
};

static void _byte_stream_swap_to_net_words(uint32_t *iv, unsigned char *b,
		unsigned int len)
{
	unsigned i, j;
	unsigned char swap_iv[AES_IV_LENGTH];

	memset(swap_iv, 0, AES_IV_LENGTH);
	for (i = (AES_IV_LENGTH-len), j = len-1;  i < AES_IV_LENGTH; i++, j--)
		swap_iv[i] = b[j];
	memcpy(iv, swap_iv, AES_IV_LENGTH);
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

static int dma_map_pmem_sg(struct buf_info *pmem, unsigned entries,
						struct scatterlist *sg)
{
	int i;
	for (i = 0; i < entries; i++) {

		sg->dma_address = (dma_addr_t)pmem->offset;
		sg++;
		pmem++;
	}
	return 0;
}

static int _probe_ce_engine(struct qce_device *pce_dev)
{
	unsigned int val;
	unsigned int rev;
	unsigned int ret;

	val = (uint32_t)(*((uint32_t *)pce_dev->ce_dm.buffer.version));
	if (((val & 0xfffffff) != 0x0000043) &&
			((val & 0xfffffff) != 0x0000042) &&
			((val & 0xfffffff) != 0x0000040)) {
		dev_err(pce_dev->pdev,
				"Unknown Qualcomm crypto device at 0x%x 0x%x\n",
				pce_dev->phy_iobase, val);
		return -EIO;
	};
	rev = (val & CRYPTO_CORE_REV_MASK);
	if (rev >= 0x42) {
		dev_info(pce_dev->pdev,
				"Qualcomm Crypto 4.2 device found at 0x%x\n",
				pce_dev->phy_iobase);
		pce_dev->ce_dm.ce_block_size = 64;

		/* Configure the crypto register to support 64byte CRCI if it
		 * is not XPU protected and the HW version of device is greater
		 * than 0x42.
		 * Crypto config register returns a 0 when it is XPU protected.
		 */

		ret = readl_relaxed(pce_dev->iobase + CRYPTO_CONFIG_REG);
		if (ret) {
			val = BIT(CRYPTO_MASK_DOUT_INTR) |
					BIT(CRYPTO_MASK_DIN_INTR) |
					BIT(CRYPTO_MASK_OP_DONE_INTR) |
					BIT(CRYPTO_MASK_ERR_INTR) |
					(CRYPTO_REQ_SIZE_ENUM_64_BYTES <<
						CRYPTO_REQ_SIZE) |
					(CRYPTO_FIFO_ENUM_64_BYTES <<
						CRYPTO_FIFO_THRESHOLD);

			writel_relaxed(val, pce_dev->iobase +
					CRYPTO_CONFIG_REG);
		} /* end of if (ret) */
	} else {
		if (rev == 0x40) {
			dev_info(pce_dev->pdev,
				"Qualcomm Crypto 4.0 device found at 0x%x\n",
							pce_dev->phy_iobase);
			pce_dev->ce_dm.ce_block_size = 16;
		}
	}

	dev_info(pce_dev->pdev,
			"IO base 0x%x\n, ce_in channel %d     , "
			"ce_out channel %d\n, "
			"crci_in %d, crci_out %d\n",
			(unsigned int) pce_dev->iobase,
			pce_dev->ce_dm.chan_ce_in, pce_dev->ce_dm.chan_ce_out,
			pce_dev->ce_dm.crci_in, pce_dev->ce_dm.crci_out);

	return 0;
};


static void _check_probe_done_call_back(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;
	pce_dev = (struct qce_device *) cmd_ptr->user;

	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
						result);
		pce_dev->ce_dm.chan_ce_in_status = -1;
	} else {
		_probe_ce_engine(pce_dev);
		pce_dev->ce_dm.chan_ce_in_status = 0;
	}
	pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
};

static int _init_ce_engine(struct qce_device *pce_dev)
{
	int status;
	/* Reset ce */
	clk_reset(pce_dev->ce_core_clk, CLK_RESET_ASSERT);
	clk_reset(pce_dev->ce_core_clk, CLK_RESET_DEASSERT);

	/*
	* Ensure previous instruction (any writes to CLK registers)
	* to toggle the CLK reset lines was completed before configuring
	* ce engine. The ce engine configuration settings should not be lost
	* becasue of clk reset.
	*/
	mb();

	/*
	 * Clear ACCESS_VIOL bit in CRYPTO_STATUS REGISTER
	*/
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	*((uint32_t *)(pce_dev->ce_dm.buffer.status)) = status & (~0x40000);
	/*
	* Ensure ce configuration is completed.
	*/
	mb();

	pce_dev->ce_dm.chan_ce_in_cmd->complete_func =
				_check_probe_done_call_back;
	pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
				pce_dev->ce_dm.cmdptrlist.probe_ce_hw;
	pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IN_PROG;
	pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_COMP;
	msm_dmov_enqueue_cmd(pce_dev->ce_dm.chan_ce_in,
					pce_dev->ce_dm.chan_ce_in_cmd);

	return 0;
};

static int _ce_setup_hash_cmdrptrlist(struct qce_device *pce_dev,
						struct qce_sha_req *sreq)
{
	struct ce_cmdptrlists_ops *cmdptrlist = &pce_dev->ce_dm.cmdptrlist;

	switch (sreq->alg) {
	case QCE_HASH_SHA1:
		pce_dev->ce_dm.chan_ce_in_cmd->cmdptr = cmdptrlist->auth_sha1;
		break;

	case QCE_HASH_SHA256:
		pce_dev->ce_dm.chan_ce_in_cmd->cmdptr = cmdptrlist->auth_sha256;
		break;
	case QCE_HASH_SHA1_HMAC:
		pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
						cmdptrlist->auth_sha1_hmac;
			break;

	case QCE_HASH_SHA256_HMAC:
		pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
						cmdptrlist->auth_sha256_hmac;
		break;
	case QCE_HASH_AES_CMAC:
		if (sreq->authklen == AES128_KEY_SIZE)
			pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
				cmdptrlist->auth_aes_128_cmac;
		else
			pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
				cmdptrlist->auth_aes_256_cmac;
		break;

	default:
		break;
	}

	return 0;
}

static int _ce_setup_hash(struct qce_device *pce_dev, struct qce_sha_req *sreq)
{
	uint32_t diglen;
	int i;
	uint32_t auth_cfg = 0;
	bool sha1 = false;

	if (sreq->alg ==  QCE_HASH_AES_CMAC) {

		memcpy(pce_dev->ce_dm.buffer.auth_key, sreq->authkey,
						sreq->authklen);
		auth_cfg |= (1 << CRYPTO_LAST);
		auth_cfg |= (CRYPTO_AUTH_MODE_CMAC << CRYPTO_AUTH_MODE);
		auth_cfg |= (CRYPTO_AUTH_SIZE_ENUM_16_BYTES <<
							CRYPTO_AUTH_SIZE);
		auth_cfg |= CRYPTO_AUTH_ALG_AES << CRYPTO_AUTH_ALG;

		switch (sreq->authklen) {
		case AES128_KEY_SIZE:
			auth_cfg |= (CRYPTO_AUTH_KEY_SZ_AES128 <<
						CRYPTO_AUTH_KEY_SIZE);
			break;
		case AES256_KEY_SIZE:
			auth_cfg |= (CRYPTO_AUTH_KEY_SZ_AES256 <<
					CRYPTO_AUTH_KEY_SIZE);
			break;
		default:
			break;
		}

		goto go_proc;
	}

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

	if ((sreq->alg == QCE_HASH_SHA1_HMAC) ||
				(sreq->alg == QCE_HASH_SHA256_HMAC)) {

		memcpy(pce_dev->ce_dm.buffer.auth_key, sreq->authkey,
						sreq->authklen);
		auth_cfg |= (CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE);
	} else {
		auth_cfg |= (CRYPTO_AUTH_MODE_HASH << CRYPTO_AUTH_MODE);
	}

	/* write 20/32 bytes, 5/8 words into auth_iv for SHA1/SHA256 */
	if (sreq->first_blk) {
		if (sha1)
			memcpy(pce_dev->ce_dm.buffer.auth_iv,
				_std_init_vector_sha1_uint8, diglen);
		else
			memcpy(pce_dev->ce_dm.buffer.auth_iv,
				_std_init_vector_sha256_uint8, diglen);
	} else {
		memcpy(pce_dev->ce_dm.buffer.auth_iv, sreq->digest,
								diglen);
	}

	/* write auth_bytecnt 0/1/2/3, start with 0 */
	for (i = 0; i < 4; i++)
		*(((uint32_t *)(pce_dev->ce_dm.buffer.auth_byte_count) + i)) =
						sreq->auth_data[i];

	/* write seg_cfg */
	if (sha1)
		auth_cfg |= (CRYPTO_AUTH_SIZE_SHA1 << CRYPTO_AUTH_SIZE);
	else
		auth_cfg |= (CRYPTO_AUTH_SIZE_SHA256 << CRYPTO_AUTH_SIZE);

	if (sreq->last_blk)
		auth_cfg |= 1 << CRYPTO_LAST;

	auth_cfg |= CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG;

go_proc:
	auth_cfg |= (CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);

	/* write auth seg cfg */
	*((uint32_t *)(pce_dev->ce_dm.buffer.auth_seg_cfg_size_start)) =
								auth_cfg;
	/* write auth seg size */
	*((uint32_t *)(pce_dev->ce_dm.buffer.auth_seg_cfg_size_start) + 1) =
								sreq->size;

	/* write auth seg size start*/
	*((uint32_t *)(pce_dev->ce_dm.buffer.auth_seg_cfg_size_start)+2) = 0;

	/* write seg size */
	*((uint32_t *)(pce_dev->ce_dm.buffer.seg_size)) = sreq->size;

	/* clear status */
	*((uint32_t *)(pce_dev->ce_dm.buffer.status)) = 0;

	_ce_setup_hash_cmdrptrlist(pce_dev, sreq);

	return 0;
}

static int _ce_setup_cipher_cmdrptrlist(struct qce_device *pce_dev,
							struct qce_req *creq)
{
	struct ce_cmdptrlists_ops *cmdptrlist =
				&pce_dev->ce_dm.cmdptrlist;

	if (creq->alg != CIPHER_ALG_AES) {
		switch (creq->alg) {
		case CIPHER_ALG_DES:
			if (creq->mode ==  QCE_MODE_ECB) {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->cipher_des_ecb;
			} else {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->cipher_des_cbc;
			}
			break;

		case CIPHER_ALG_3DES:
			if (creq->mode ==  QCE_MODE_ECB) {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
						cmdptrlist->cipher_3des_ecb;
			} else {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
						cmdptrlist->cipher_3des_cbc;
			}
			break;
		default:
			break;
		}
	} else {
		switch (creq->mode) {
		case QCE_MODE_ECB:
			if (creq->encklen ==  AES128_KEY_SIZE) {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
						cmdptrlist->cipher_aes_128_ecb;
			} else {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
						cmdptrlist->cipher_aes_256_ecb;
			}
			break;

		case QCE_MODE_CBC:
			if (creq->encklen ==  AES128_KEY_SIZE) {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->cipher_aes_128_cbc_ctr;
			} else {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->cipher_aes_256_cbc_ctr;
			}
			break;

		case QCE_MODE_CTR:
			if (creq->encklen ==  AES128_KEY_SIZE) {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->cipher_aes_128_cbc_ctr;
			} else {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->cipher_aes_256_cbc_ctr;
			}
			break;

		case QCE_MODE_XTS:
			if (creq->encklen ==  AES128_KEY_SIZE) {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->cipher_aes_128_xts;
			} else {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->cipher_aes_256_xts;
			}
			break;
		case QCE_MODE_CCM:
			if (creq->encklen ==  AES128_KEY_SIZE) {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->aead_aes_128_ccm;
			} else {
				pce_dev->ce_dm.chan_ce_in_cmd->cmdptr =
					cmdptrlist->aead_aes_256_ccm;
			}
			break;
		default:
			break;
		}
	}

	switch (creq->mode) {
	case QCE_MODE_CCM:
		pce_dev->ce_dm.chan_ce_out_cmd->cmdptr =
					cmdptrlist->aead_ce_out;
		break;
	case QCE_MODE_ECB:
		pce_dev->ce_dm.chan_ce_out_cmd->cmdptr =
					cmdptrlist->cipher_ce_out;
		break;
	default:
		pce_dev->ce_dm.chan_ce_out_cmd->cmdptr =
					cmdptrlist->cipher_ce_out_get_iv;
		break;
	}

	return 0;
}

static int _ce_setup_cipher(struct qce_device *pce_dev, struct qce_req *creq,
		uint32_t totallen_in, uint32_t coffset)
{
	uint32_t enck_size_in_word = creq->encklen / sizeof(uint32_t);
	uint32_t encr_cfg = 0;
	uint32_t ivsize = creq->ivsize;
	struct ce_reg_buffer_addr *buffer = &pce_dev->ce_dm.buffer;

	if (creq->mode ==  QCE_MODE_XTS)
		memcpy(buffer->encr_key, creq->enckey,
						creq->encklen/2);
	else
		memcpy(buffer->encr_key, creq->enckey, creq->encklen);

	if ((creq->op == QCE_REQ_AEAD) && (creq->mode == QCE_MODE_CCM)) {
		uint32_t noncelen32 = MAX_NONCE/sizeof(uint32_t);
		uint32_t auth_cfg = 0;

		/* write nonce */
		memcpy(buffer->auth_nonce_info, creq->nonce, MAX_NONCE);
		memcpy(buffer->auth_key, creq->enckey, creq->encklen);

		auth_cfg |= (noncelen32 << CRYPTO_AUTH_NONCE_NUM_WORDS);
		auth_cfg &= ~(1 << CRYPTO_USE_HW_KEY_AUTH);
		auth_cfg |= (1 << CRYPTO_LAST);
		if (creq->dir == QCE_ENCRYPT)
			auth_cfg |= (CRYPTO_AUTH_POS_BEFORE << CRYPTO_AUTH_POS);
		else
			auth_cfg |= (CRYPTO_AUTH_POS_AFTER << CRYPTO_AUTH_POS);
		auth_cfg |= (((creq->authsize >> 1) - 2) << CRYPTO_AUTH_SIZE);
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
		*((uint32_t *)(buffer->auth_seg_cfg_size_start)) = auth_cfg;

		if (creq->dir == QCE_ENCRYPT)
			*((uint32_t *)(buffer->auth_seg_cfg_size_start) + 1) =
								totallen_in;
		else
			*((uint32_t *)(buffer->auth_seg_cfg_size_start) + 1) =
						(totallen_in - creq->authsize);
		*((uint32_t *)(buffer->auth_seg_cfg_size_start) + 2) = 0;
	}

	*((uint32_t *)(buffer->auth_seg_cfg_size_start) + 2) = 0;

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
		if (creq->mode !=  QCE_MODE_ECB)
			memcpy(buffer->encr_cntr_iv, creq->iv, ivsize);

		encr_cfg |= ((CRYPTO_ENCR_KEY_SZ_DES << CRYPTO_ENCR_KEY_SZ)  |
				(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG));
		break;

	case CIPHER_ALG_3DES:
		if (creq->mode !=  QCE_MODE_ECB)
			memcpy(buffer->encr_cntr_iv, creq->iv, ivsize);

		encr_cfg |= ((CRYPTO_ENCR_KEY_SZ_3DES << CRYPTO_ENCR_KEY_SZ)  |
				(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG));
		break;

	case CIPHER_ALG_AES:
	default:
		if (creq->mode ==  QCE_MODE_XTS) {
			memcpy(buffer->encr_xts_key, (creq->enckey +
					creq->encklen/2), creq->encklen/2);
			*((uint32_t *)(buffer->encr_xts_du_size)) =
							creq->cryptlen;

		}
		if (creq->mode !=  QCE_MODE_ECB) {
			if (creq->mode ==  QCE_MODE_XTS)
				_byte_stream_swap_to_net_words(
					(uint32_t *)(buffer->encr_cntr_iv),
							creq->iv, ivsize);
			else
				memcpy(buffer->encr_cntr_iv, creq->iv,
								ivsize);
		}
		/* set number of counter bits */
		*((uint32_t *)(buffer->encr_mask)) = (uint32_t)0xffffffff;

		if (creq->op == QCE_REQ_ABLK_CIPHER_NO_KEY) {
				encr_cfg |= (CRYPTO_ENCR_KEY_SZ_AES128 <<
						CRYPTO_ENCR_KEY_SZ);
			encr_cfg |= CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG;
		} else {
			uint32_t key_size;

			if (creq->mode == QCE_MODE_XTS) {
				key_size = creq->encklen/2;
				enck_size_in_word = key_size/sizeof(uint32_t);
			} else {
				key_size = creq->encklen;
			}

			switch (key_size) {
			case AES128_KEY_SIZE:
				encr_cfg |= (CRYPTO_ENCR_KEY_SZ_AES128 <<
							CRYPTO_ENCR_KEY_SZ);
				break;
			case AES256_KEY_SIZE:
			default:
				encr_cfg |= (CRYPTO_ENCR_KEY_SZ_AES256 <<
							CRYPTO_ENCR_KEY_SZ);
				break;
			} /* end of switch (creq->encklen) */

			encr_cfg |= CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG;
		} /* else of if (creq->op == QCE_REQ_ABLK_CIPHER_NO_KEY) */
		break;
	} /* end of switch (creq->mode)  */

	/* write encr seg cfg */
	encr_cfg |= ((creq->dir == QCE_ENCRYPT) ? 1 : 0) << CRYPTO_ENCODE;

	/* write encr seg cfg */
	*((uint32_t *)(buffer->encr_seg_cfg_size_start)) = encr_cfg;
	/* write encr seg size */
	if ((creq->mode == QCE_MODE_CCM) && (creq->dir == QCE_DECRYPT))
		*((uint32_t *)(buffer->encr_seg_cfg_size_start) + 1) =
					(creq->cryptlen + creq->authsize);
	else
		*((uint32_t *)(buffer->encr_seg_cfg_size_start) + 1) =
							creq->cryptlen;


	*((uint32_t *)(buffer->encr_seg_cfg_size_start) + 2) =
						(coffset & 0xffff);

	*((uint32_t *)(buffer->seg_size)) = totallen_in;

	/* clear status */
	*((uint32_t *)(pce_dev->ce_dm.buffer.status)) = 0;

	_ce_setup_cipher_cmdrptrlist(pce_dev, creq);
	return 0;
};

static int _aead_complete(struct qce_device *pce_dev)
{
	struct aead_request *areq;

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
	if (pce_dev->mode == QCE_MODE_CCM) {
		int32_t result = 0;

		result =
			(uint32_t)(*((uint32_t *)pce_dev->ce_dm.buffer.status));
		result &= (1 << CRYPTO_MAC_FAILED);
		result |= (pce_dev->ce_dm.chan_ce_in_status |
					pce_dev->ce_dm.chan_ce_out_status);
		if (pce_dev->ce_dm.chan_ce_in_status |
			 pce_dev->ce_dm.chan_ce_out_status)
			result = -ENXIO;
		else if (result & (1 << CRYPTO_MAC_FAILED))
			result = -EBADMSG;

		pce_dev->qce_cb(areq, pce_dev->ce_dm.buffer.auth_result, NULL,
							result);
	}
	return 0;
};

static void _sha_complete(struct qce_device *pce_dev)
{
	struct ahash_request *areq;

	areq = (struct ahash_request *) pce_dev->areq;
	qce_dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
				DMA_TO_DEVICE);

	pce_dev->qce_cb(areq,  pce_dev->ce_dm.buffer.auth_result,
				pce_dev->ce_dm.buffer.auth_byte_count,
				pce_dev->ce_dm.chan_ce_in_status);

};

static int _ablk_cipher_complete(struct qce_device *pce_dev)
{
	struct ablkcipher_request *areq;

	areq = (struct ablkcipher_request *) pce_dev->areq;

	if (areq->src != areq->dst) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->dst,
			pce_dev->dst_nents, DMA_FROM_DEVICE);
	}
	qce_dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
		(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
						DMA_TO_DEVICE);

	if (pce_dev->mode == QCE_MODE_ECB) {
		pce_dev->qce_cb(areq, NULL, NULL,
					pce_dev->ce_dm.chan_ce_in_status |
					pce_dev->ce_dm.chan_ce_out_status);
	} else {

		pce_dev->qce_cb(areq, NULL, pce_dev->ce_dm.buffer.encr_cntr_iv,
			pce_dev->ce_dm.chan_ce_in_status |
			pce_dev->ce_dm.chan_ce_out_status);
	}

	return 0;
};

static int _ablk_cipher_use_pmem_complete(struct qce_device *pce_dev)
{
	struct ablkcipher_request *areq;

	areq = (struct ablkcipher_request *) pce_dev->areq;

	if (pce_dev->mode == QCE_MODE_ECB) {
		pce_dev->qce_cb(areq, NULL, NULL,
					pce_dev->ce_dm.chan_ce_in_status |
					pce_dev->ce_dm.chan_ce_out_status);
	} else {
		pce_dev->qce_cb(areq, NULL, pce_dev->ce_dm.buffer.encr_cntr_iv,
					pce_dev->ce_dm.chan_ce_in_status |
					pce_dev->ce_dm.chan_ce_out_status);
	}

	return 0;
};

static int qce_split_and_insert_dm_desc(struct dmov_desc *pdesc,
			unsigned int plen, unsigned int paddr, int *index)
{
	while (plen > QCE_FIFO_SIZE) {
		pdesc->len = QCE_FIFO_SIZE;
		if (paddr > 0) {
			pdesc->addr = paddr;
			paddr += QCE_FIFO_SIZE;
		}
		plen -= pdesc->len;
		if (plen > 0) {
			*index = (*index) + 1;
			if ((*index) >= QCE_MAX_NUM_DESC)
				return -ENOMEM;
			pdesc++;
		}
	}
	if ((plen > 0) && (plen <= QCE_FIFO_SIZE)) {
		pdesc->len = plen;
		if (paddr > 0)
			pdesc->addr = paddr;
	}

	return 0;
}

static int _chain_sg_buffer_in(struct qce_device *pce_dev,
		struct scatterlist *sg, unsigned int nbytes)
{
	unsigned int len;
	unsigned int dlen;
	struct dmov_desc *pdesc;

	pdesc = pce_dev->ce_dm.ce_in_src_desc +
				pce_dev->ce_dm.ce_in_src_desc_index;
	/*
	 * Two consective chunks may be handled by the old
	 * buffer descriptor.
	 */
	while (nbytes > 0) {
		len = min(nbytes, sg_dma_len(sg));
		dlen = pdesc->len & ADM_DESC_LENGTH_MASK;
		nbytes -= len;
		if (dlen == 0) {
			pdesc->addr  = sg_dma_address(sg);
			pdesc->len = len;
			if (pdesc->len > QCE_FIFO_SIZE)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					sg_dma_address(sg),
					&pce_dev->ce_dm.ce_in_src_desc_index);
		} else if (sg_dma_address(sg) == (pdesc->addr + dlen)) {
			pdesc->len  = dlen + len;
			if (pdesc->len > QCE_FIFO_SIZE)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					pdesc->addr,
					&pce_dev->ce_dm.ce_in_src_desc_index);
		} else {
			pce_dev->ce_dm.ce_in_src_desc_index++;
			if (pce_dev->ce_dm.ce_in_src_desc_index >=
							QCE_MAX_NUM_DESC)
				return -ENOMEM;
			pdesc++;
			pdesc->len = len;
			pdesc->addr = sg_dma_address(sg);
			if (pdesc->len > QCE_FIFO_SIZE)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					sg_dma_address(sg),
					&pce_dev->ce_dm.ce_in_src_desc_index);
		}
		if (nbytes > 0)
			sg = scatterwalk_sg_next(sg);
	}
	return 0;
}

static int _chain_pm_buffer_in(struct qce_device *pce_dev,
		unsigned int pmem, unsigned int nbytes)
{
	unsigned int dlen;
	struct dmov_desc *pdesc;

	pdesc = pce_dev->ce_dm.ce_in_src_desc +
				pce_dev->ce_dm.ce_in_src_desc_index;
	dlen = pdesc->len & ADM_DESC_LENGTH_MASK;
	if (dlen == 0) {
		pdesc->addr  = pmem;
		pdesc->len = nbytes;
	} else if (pmem == (pdesc->addr + dlen)) {
		pdesc->len  = dlen + nbytes;
	} else {
		pce_dev->ce_dm.ce_in_src_desc_index++;
		if (pce_dev->ce_dm.ce_in_src_desc_index >=
						QCE_MAX_NUM_DESC)
			return -ENOMEM;
		pdesc++;
		pdesc->len = nbytes;
		pdesc->addr = pmem;
	}
	return 0;
}

static void _chain_buffer_in_init(struct qce_device *pce_dev)
{
	struct dmov_desc *pdesc;

	pce_dev->ce_dm.ce_in_src_desc_index = 0;
	pce_dev->ce_dm.ce_in_dst_desc_index = 0;
	pdesc = pce_dev->ce_dm.ce_in_src_desc;
	pdesc->len = 0;
}

static void _ce_in_final(struct qce_device *pce_dev, unsigned total)
{
	struct dmov_desc *pdesc;
	dmov_sg *pcmd;

	pdesc = pce_dev->ce_dm.ce_in_src_desc +
				pce_dev->ce_dm.ce_in_src_desc_index;
	pdesc->len |= ADM_DESC_LAST;

	pdesc = pce_dev->ce_dm.ce_in_dst_desc;
	if (total > QCE_FIFO_SIZE) {
		qce_split_and_insert_dm_desc(pdesc, total, 0,
				&pce_dev->ce_dm.ce_in_dst_desc_index);
		pdesc = pce_dev->ce_dm.ce_in_dst_desc +
					pce_dev->ce_dm.ce_in_dst_desc_index;
		pdesc->len |= ADM_DESC_LAST;
	} else
		pdesc->len = ADM_DESC_LAST | total;

	pcmd = (dmov_sg *) pce_dev->ce_dm.cmdlist.ce_data_in;
	pcmd->cmd |= CMD_LC;

}

#ifdef QCE_DEBUG
static void _ce_in_dump(struct qce_device *pce_dev)
{
	int i;
	struct dmov_desc *pdesc;

	dev_info(pce_dev->pdev, "_ce_in_dump: src\n");
	for (i = 0; i <= pce_dev->ce_dm.ce_in_src_desc_index; i++) {
		pdesc = pce_dev->ce_dm.ce_in_src_desc + i;
		dev_info(pce_dev->pdev, "%x , %x\n", pdesc->addr,
				pdesc->len);
	}
	dev_info(pce_dev->pdev, "_ce_in_dump: dst\n");
	for (i = 0; i <= pce_dev->ce_dm.ce_in_dst_desc_index; i++) {
		pdesc = pce_dev->ce_dm.ce_in_dst_desc + i;
		dev_info(pce_dev->pdev, "%x , %x\n", pdesc->addr,
				pdesc->len);
	}
};

static void _ce_out_dump(struct qce_device *pce_dev)
{
	int i;
	struct dmov_desc *pdesc;

	dev_info(pce_dev->pdev, "_ce_out_dump: src\n");
	for (i = 0; i <= pce_dev->ce_dm.ce_out_src_desc_index; i++) {
		pdesc = pce_dev->ce_dm.ce_out_src_desc + i;
		dev_info(pce_dev->pdev, "%x , %x\n", pdesc->addr,
				pdesc->len);
	}

	dev_info(pce_dev->pdev, "_ce_out_dump: dst\n");
	for (i = 0; i <= pce_dev->ce_dm.ce_out_dst_desc_index; i++) {
		pdesc = pce_dev->ce_dm.ce_out_dst_desc + i;
		dev_info(pce_dev->pdev, "%x , %x\n", pdesc->addr,
				pdesc->len);
	}
};

#else

static void _ce_in_dump(struct qce_device *pce_dev)
{
};

static void _ce_out_dump(struct qce_device *pce_dev)
{
};

#endif

static int _chain_sg_buffer_out(struct qce_device *pce_dev,
		struct scatterlist *sg, unsigned int nbytes)
{
	unsigned int len;
	unsigned int dlen;
	struct dmov_desc *pdesc;

	pdesc = pce_dev->ce_dm.ce_out_dst_desc +
				pce_dev->ce_dm.ce_out_dst_desc_index;
	/*
	 * Two consective chunks may be handled by the old
	 * buffer descriptor.
	 */
	while (nbytes > 0) {
		len = min(nbytes, sg_dma_len(sg));
		dlen = pdesc->len & ADM_DESC_LENGTH_MASK;
		nbytes -= len;
		if (dlen == 0) {
			pdesc->addr  = sg_dma_address(sg);
			pdesc->len = len;
			if (pdesc->len > QCE_FIFO_SIZE)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					sg_dma_address(sg),
					&pce_dev->ce_dm.ce_out_dst_desc_index);
		} else if (sg_dma_address(sg) == (pdesc->addr + dlen)) {
			pdesc->len  = dlen + len;
			if (pdesc->len > QCE_FIFO_SIZE)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					pdesc->addr,
					&pce_dev->ce_dm.ce_out_dst_desc_index);

		} else {
			pce_dev->ce_dm.ce_out_dst_desc_index++;
			if (pce_dev->ce_dm.ce_out_dst_desc_index >=
							QCE_MAX_NUM_DESC)
				return -EIO;
			pdesc++;
			pdesc->len = len;
			pdesc->addr = sg_dma_address(sg);
			if (pdesc->len > QCE_FIFO_SIZE)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					sg_dma_address(sg),
					&pce_dev->ce_dm.ce_out_dst_desc_index);

		}
		if (nbytes > 0)
			sg = scatterwalk_sg_next(sg);
	}
	return 0;
}

static int _chain_pm_buffer_out(struct qce_device *pce_dev,
		unsigned int pmem, unsigned int nbytes)
{
	unsigned int dlen;
	struct dmov_desc *pdesc;

	pdesc = pce_dev->ce_dm.ce_out_dst_desc +
				pce_dev->ce_dm.ce_out_dst_desc_index;
	dlen = pdesc->len & ADM_DESC_LENGTH_MASK;

	if (dlen == 0) {
		pdesc->addr  = pmem;
		pdesc->len = nbytes;
	} else if (pmem == (pdesc->addr + dlen)) {
		pdesc->len  = dlen + nbytes;
	} else {
		pce_dev->ce_dm.ce_out_dst_desc_index++;
		if (pce_dev->ce_dm.ce_out_dst_desc_index >= QCE_MAX_NUM_DESC)
			return -EIO;
		pdesc++;
		pdesc->len = nbytes;
		pdesc->addr = pmem;
	}
	return 0;
};

static void _chain_buffer_out_init(struct qce_device *pce_dev)
{
	struct dmov_desc *pdesc;

	pce_dev->ce_dm.ce_out_dst_desc_index = 0;
	pce_dev->ce_dm.ce_out_src_desc_index = 0;
	pdesc = pce_dev->ce_dm.ce_out_dst_desc;
	pdesc->len = 0;
};

static void _ce_out_final(struct qce_device *pce_dev, unsigned total)
{
	struct dmov_desc *pdesc;
	dmov_sg *pcmd;

	pdesc = pce_dev->ce_dm.ce_out_dst_desc +
				pce_dev->ce_dm.ce_out_dst_desc_index;
	pdesc->len |= ADM_DESC_LAST;

	pdesc = pce_dev->ce_dm.ce_out_src_desc +
				pce_dev->ce_dm.ce_out_src_desc_index;
	if (total > QCE_FIFO_SIZE) {
		qce_split_and_insert_dm_desc(pdesc, total, 0,
				&pce_dev->ce_dm.ce_out_src_desc_index);
		pdesc = pce_dev->ce_dm.ce_out_src_desc +
				pce_dev->ce_dm.ce_out_src_desc_index;
		pdesc->len |= ADM_DESC_LAST;
	} else
		pdesc->len = ADM_DESC_LAST | total;

	pcmd = (dmov_sg *) pce_dev->ce_dm.cmdlist.ce_data_out;
	pcmd->cmd |= CMD_LC;
};

static void _aead_ce_in_call_back(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;

	pce_dev = (struct qce_device *) cmd_ptr->user;
	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
							result);
		pce_dev->ce_dm.chan_ce_in_status = -1;
	} else {
		pce_dev->ce_dm.chan_ce_in_status = 0;
	}

	pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->ce_dm.chan_ce_out_state == QCE_CHAN_STATE_COMP) {
		pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_IDLE;

		/* done */
		_aead_complete(pce_dev);
	}
};

static void _aead_ce_out_call_back(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;

	pce_dev = (struct qce_device *) cmd_ptr->user;
	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
							result);
		pce_dev->ce_dm.chan_ce_out_status = -1;
	} else {
		pce_dev->ce_dm.chan_ce_out_status = 0;
	};

	pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->ce_dm.chan_ce_in_state == QCE_CHAN_STATE_COMP) {
		pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_IDLE;

		/* done */
		_aead_complete(pce_dev);
	}

};

static void _sha_ce_in_call_back(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;

	pce_dev = (struct qce_device *) cmd_ptr->user;
	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
						result);
		pce_dev->ce_dm.chan_ce_in_status = -1;
	} else {
		pce_dev->ce_dm.chan_ce_in_status = 0;
	}
	pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
	_sha_complete(pce_dev);
};

static void _ablk_cipher_ce_in_call_back(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;

	pce_dev = (struct qce_device *) cmd_ptr->user;
	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
						result);
		pce_dev->ce_dm.chan_ce_in_status = -1;
	} else {
		pce_dev->ce_dm.chan_ce_in_status = 0;
	}

	pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->ce_dm.chan_ce_out_state == QCE_CHAN_STATE_COMP) {
		pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_IDLE;

		/* done */
		_ablk_cipher_complete(pce_dev);
	}
};

static void _ablk_cipher_ce_out_call_back(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;

	pce_dev = (struct qce_device *) cmd_ptr->user;

	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
						result);
		pce_dev->ce_dm.chan_ce_out_status = -1;
	} else {
		pce_dev->ce_dm.chan_ce_out_status = 0;
	};

	pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->ce_dm.chan_ce_in_state == QCE_CHAN_STATE_COMP) {
		pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_IDLE;

		/* done */
		_ablk_cipher_complete(pce_dev);
	}
};


static void _ablk_cipher_ce_in_call_back_pmem(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;
	pce_dev = (struct qce_device *) cmd_ptr->user;
	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
						result);
		pce_dev->ce_dm.chan_ce_in_status = -1;
	} else {
		pce_dev->ce_dm.chan_ce_in_status = 0;
	}

	pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->ce_dm.chan_ce_out_state == QCE_CHAN_STATE_COMP) {
		pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_IDLE;

		/* done */
		_ablk_cipher_use_pmem_complete(pce_dev);
	}
};

static void _ablk_cipher_ce_out_call_back_pmem(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;

	pce_dev = (struct qce_device *) cmd_ptr->user;
	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
						result);
		pce_dev->ce_dm.chan_ce_out_status = -1;
	} else {
		pce_dev->ce_dm.chan_ce_out_status = 0;
	};

	pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->ce_dm.chan_ce_in_state == QCE_CHAN_STATE_COMP) {
		pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_IDLE;

		/* done */
		_ablk_cipher_use_pmem_complete(pce_dev);
	}
};

static int qce_setup_cmd_buffers(struct qce_device *pce_dev,
						unsigned char **pvaddr)
{
	struct ce_reg_buffers *addr = (struct ce_reg_buffers *)(*pvaddr);
	struct ce_reg_buffer_addr *buffer = &pce_dev->ce_dm.buffer;

	/*
	 * Designate chunks of the allocated memory to various
	 * buffer pointers
	 */
	buffer->reset_buf_64 = addr->reset_buf_64;
	buffer->version = addr->version;
	buffer->encr_seg_cfg_size_start = addr->encr_seg_cfg_size_start;
	buffer->encr_key = addr->encr_key;
	buffer->encr_xts_key = addr->encr_xts_key;
	buffer->encr_xts_du_size = addr->encr_xts_du_size;
	buffer->encr_cntr_iv = addr->encr_cntr_iv;
	buffer->encr_mask = addr->encr_mask;
	buffer->auth_seg_cfg_size_start = addr->auth_seg_cfg_size_start;
	buffer->auth_key = addr->auth_key;
	buffer->auth_iv = addr->auth_iv;
	buffer->auth_result = addr->auth_result;
	buffer->auth_nonce_info = addr->auth_nonce_info;
	buffer->auth_byte_count = addr->auth_byte_count;
	buffer->seg_size = addr->seg_size;
	buffer->go_proc = addr->go_proc;
	buffer->status = addr->status;
	buffer->pad = addr->pad;

	memset(buffer->reset_buf_64, 0, 64);
	*((uint32_t *)buffer->encr_mask) = (uint32_t)(0xffffffff);
	*((uint32_t *)buffer->go_proc) = (uint32_t)(1 << CRYPTO_GO);

	*pvaddr += sizeof(struct ce_reg_buffers);

	return 0;

}

static int _setup_cipher_cmdlists(struct qce_device *pce_dev,
						unsigned char **pvaddr)
{
	dmov_s  *pscmd = (dmov_s  *)(*pvaddr);

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to cipher operation
	 */
	pce_dev->ce_dm.cmdlist.set_cipher_cfg = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_SEG_CFG_REG +
					pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 3;
	pscmd->src =
		GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_seg_cfg_size_start);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_aes_128_key = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_aes_256_key = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 8;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_des_key = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 2;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_3des_key = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 6;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_aes_128_xts_key = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_XTS_KEY0_REG +
							pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_xts_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_aes_256_xts_key = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
					CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_XTS_KEY0_REG +
							pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 8;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_xts_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_xts_du_size = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_XTS_DU_SIZE_REG +
							pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_xts_du_size);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_aes_iv = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
					CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_CNTR0_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_cntr_iv);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_des_iv = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_CNTR0_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 2;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_cntr_iv);
	pscmd++;

	pce_dev->ce_dm.cmdlist.get_cipher_iv = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->src = (unsigned) (CRYPTO_CNTR0_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->dst = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_cntr_iv);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_cipher_mask = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_CNTR_MASK_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.encr_mask);
	pscmd++;

	/* RESET CIPHER AND AUTH REGISTERS COMMAND LISTS*/

	pce_dev->ce_dm.cmdlist.reset_cipher_key  = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 8;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.reset_buf_64);
	pscmd++;

	pce_dev->ce_dm.cmdlist.reset_cipher_xts_key  = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_XTS_KEY0_REG +
							pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 8;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.reset_buf_64);
	pscmd++;

	pce_dev->ce_dm.cmdlist.reset_cipher_iv  = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_CNTR0_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.reset_buf_64);
	pscmd++;

	pce_dev->ce_dm.cmdlist.reset_cipher_cfg  = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_ENCR_SEG_CFG_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.reset_buf_64);
	pscmd++;

	*pvaddr = (unsigned char *) pscmd;

	return 0;
}

static int _setup_auth_cmdlists(struct qce_device *pce_dev,
						unsigned char **pvaddr)
{
	dmov_s  *pscmd = (dmov_s  *)(*pvaddr);

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to authentication operation
	 */
	pce_dev->ce_dm.cmdlist.set_auth_cfg = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_SEG_CFG_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 3;
	pscmd->src =
		GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_seg_cfg_size_start);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_auth_key_128 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_auth_key_256 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 8;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_auth_key_512 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 16;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_key);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_auth_iv_16 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_iv);
	pscmd++;

	pce_dev->ce_dm.cmdlist.get_auth_result_16 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->src = (unsigned) (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->dst = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_result);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_auth_iv_20 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 5;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_iv);
	pscmd++;

	pce_dev->ce_dm.cmdlist.get_auth_result_20 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->src = (unsigned) (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 5;
	pscmd->dst = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_result);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_auth_iv_32 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 8;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_iv);
	pscmd++;


	pce_dev->ce_dm.cmdlist.get_auth_result_32 = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->src = (unsigned) (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 8;
	pscmd->dst = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_result);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_auth_byte_count = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_BYTECNT0_REG +
							pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_byte_count);
	pscmd++;

	pce_dev->ce_dm.cmdlist.get_auth_byte_count = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->src = (unsigned) (CRYPTO_AUTH_BYTECNT0_REG +
							pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->dst = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_byte_count);
	pscmd++;

	pce_dev->ce_dm.cmdlist.set_auth_nonce_info = pscmd;
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES |
				CMD_SRC_SWAP_SHORTS | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_INFO_NONCE0_REG +
							pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.auth_nonce_info);
	pscmd++;

	/* RESET CIPHER AND AUTH REGISTERS COMMAND LISTS*/

	pce_dev->ce_dm.cmdlist.reset_auth_key = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_KEY0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 16;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.reset_buf_64);
	pscmd++;

	pce_dev->ce_dm.cmdlist.reset_auth_iv = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 16;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.reset_buf_64);
	pscmd++;

	pce_dev->ce_dm.cmdlist.reset_auth_cfg = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_SEG_CFG_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.reset_buf_64);
	pscmd++;


	pce_dev->ce_dm.cmdlist.reset_auth_byte_count = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_AUTH_BYTECNT0_REG +
							pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE * 4;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.reset_buf_64);
	pscmd++;

	/* WAIT UNTIL MAC OP IS DONE*/

	pce_dev->ce_dm.cmdlist.get_status_wait = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->src = (unsigned) (CRYPTO_STATUS_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->dst = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.status);
	pscmd++;

	*pvaddr = (unsigned char *) pscmd;

	return 0;
}

static int qce_setup_cmdlists(struct qce_device *pce_dev,
						unsigned char **pvaddr)
{
	dmov_sg *pcmd;
	dmov_s  *pscmd;
	unsigned char *vaddr = *pvaddr;
	struct dmov_desc *pdesc;
	int i = 0;

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to operation define
	 * in ce_cmdlists structure.
	 */
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr), 16);
	*pvaddr = (unsigned char *) vaddr;

	_setup_cipher_cmdlists(pce_dev, pvaddr);
	_setup_auth_cmdlists(pce_dev, pvaddr);

	pscmd = (dmov_s  *)(*pvaddr);

	/* GET HW VERSION COMMAND LIST */
	pce_dev->ce_dm.cmdlist.get_hw_version = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE | CMD_OCB;
	pscmd->src = (unsigned) (CRYPTO_VERSION_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->dst = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.version);
	pscmd++;


	/* SET SEG SIZE REGISTER LIST */
	pce_dev->ce_dm.cmdlist.set_seg_size = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_SEG_SIZE_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.seg_size);
	pscmd++;


	/* Get status and OCU COMMAND LIST */
	pce_dev->ce_dm.cmdlist.get_status_ocu = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE | CMD_OCU;
	pscmd->src = (unsigned) (CRYPTO_STATUS_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->dst = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.status);
	pscmd++;

	/* CLEAR STATUS and OCU COMMAND LIST */
	pce_dev->ce_dm.cmdlist.clear_status = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE | CMD_OCU;
	pscmd->dst = (unsigned) (CRYPTO_STATUS_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.status);
	pscmd++;

	/* CLEAR STATUS and OCB COMMAND LIST */
	pce_dev->ce_dm.cmdlist.clear_status_ocb = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE | CMD_OCB;
	pscmd->dst = (unsigned) (CRYPTO_STATUS_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.status);
	pscmd++;

	/* SET GO_PROC REGISTERS COMMAND LIST */
	pce_dev->ce_dm.cmdlist.set_go_proc = pscmd;
	pscmd->cmd = CMD_LC | CMD_MODE_SINGLE;
	pscmd->dst = (unsigned) (CRYPTO_GOPROC_REG + pce_dev->phy_iobase);
	pscmd->len = CRYPTO_REG_SIZE;
	pscmd->src = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.go_proc);
	pscmd++;

	pcmd = (dmov_sg  *)pscmd;
	pce_dev->ce_dm.cmdlist.ce_data_in = pcmd;
	/* swap byte and half word , dst crci ,  scatter gather */
	pcmd->cmd = CMD_DST_SWAP_BYTES | CMD_DST_SWAP_SHORTS |
			CMD_DST_CRCI(pce_dev->ce_dm.crci_in) | CMD_MODE_SG;

	pdesc = pce_dev->ce_dm.ce_in_src_desc;
	pdesc->addr = 0;	/* to be filled in each operation */
	pdesc->len = 0;		/* to be filled in each operation */

	pdesc = pce_dev->ce_dm.ce_in_dst_desc;
	for (i = 0; i < QCE_MAX_NUM_DESC; i++) {
		pdesc->addr = (CRYPTO_DATA_SHADOW0 + pce_dev->phy_iobase);
		pdesc->len = 0; /* to be filled in each operation */
		pdesc++;
	}
	pcmd->src_dscr = GET_PHYS_ADDR(pce_dev->ce_dm.ce_in_src_desc);
	pcmd->dst_dscr = GET_PHYS_ADDR(pce_dev->ce_dm.ce_in_dst_desc);
	pcmd->_reserved = LI_SG_CMD | SRC_INDEX_SG_CMD(0) |
						DST_INDEX_SG_CMD(0);


	pcmd++;
	pce_dev->ce_dm.cmdlist.ce_data_out = pcmd;
	/* swap byte, half word, source crci, scatter gather */
	pcmd->cmd =   CMD_SRC_SWAP_BYTES | CMD_SRC_SWAP_SHORTS |
			CMD_SRC_CRCI(pce_dev->ce_dm.crci_out) | CMD_MODE_SG;

	pdesc = pce_dev->ce_dm.ce_out_src_desc;
	for (i = 0; i < QCE_MAX_NUM_DESC; i++) {
		pdesc->addr = (CRYPTO_DATA_SHADOW0 + pce_dev->phy_iobase);
		pdesc->len = 0;  /* to be filled in each operation */
		pdesc++;
	}

	pdesc = pce_dev->ce_dm.ce_out_dst_desc;
	pdesc->addr = 0;  /* to be filled in each operation */
	pdesc->len = 0;   /* to be filled in each operation */

	pcmd->src_dscr = GET_PHYS_ADDR(pce_dev->ce_dm.ce_out_src_desc);
	pcmd->dst_dscr = GET_PHYS_ADDR(pce_dev->ce_dm.ce_out_dst_desc);
	pcmd->_reserved = LI_SG_CMD | SRC_INDEX_SG_CMD(0) |
						DST_INDEX_SG_CMD(0);
	pcmd++;

	*pvaddr = (unsigned char *) pcmd;

	return 0;
}

static int _setup_cipher_cmdptrlists(struct qce_device *pce_dev,
						unsigned char **pvaddr)
{
	uint32_t * cmd_ptr_vaddr = (uint32_t *)(*pvaddr);
	struct ce_cmdlists *cmdlist = &pce_dev->ce_dm.cmdlist;
	struct ce_cmdptrlists_ops *cmdptrlist = &pce_dev->ce_dm.cmdptrlist;

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to cipher operations defined
	 * in ce_cmdptrlists_ops structure.
	 */
	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_aes_128_cbc_ctr = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_128_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_mask);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_aes_256_cbc_ctr = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_256_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_mask);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_aes_128_ecb = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_128_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_mask);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *)ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_aes_256_ecb = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_256_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_mask);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *)ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_aes_128_xts = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_128_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_128_xts_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_xts_du_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_mask);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_aes_256_xts = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_256_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_256_xts_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_xts_du_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_mask);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *)ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_des_cbc = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_des_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_des_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *)ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_des_ecb = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_des_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_3des_cbc = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_3des_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_des_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_3des_ecb = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_3des_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_ce_out = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_out);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->cipher_ce_out_get_iv = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_out);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_cipher_iv);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	*pvaddr = (unsigned char *) cmd_ptr_vaddr;

	return 0;
}

static int _setup_auth_cmdptrlists(struct qce_device *pce_dev,
						unsigned char **pvaddr)
{
	uint32_t * cmd_ptr_vaddr = (uint32_t *)(*pvaddr);
	struct ce_cmdlists *cmdlist = &pce_dev->ce_dm.cmdlist;
	struct ce_cmdptrlists_ops *cmdptrlist = &pce_dev->ce_dm.cmdptrlist;

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to authentication operations
	 * defined in ce_cmdptrlists_ops structure.
	 */
	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->auth_sha1 = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_iv_20);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_in);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_result_20);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->auth_sha256 = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_iv_32);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_in);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_result_32);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->auth_sha1_hmac = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_key_512);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_iv_20);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_in);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_result_20);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->auth_sha256_hmac = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_key_512);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_iv_32);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_in);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_result_32);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->auth_aes_128_cmac = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_key_128);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_in);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_result_16);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->auth_aes_256_cmac = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_key_256);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_in);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_auth_result_16);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	*pvaddr = (unsigned char *) cmd_ptr_vaddr;

	return 0;
}

static int _setup_aead_cmdptrlists(struct qce_device *pce_dev,
						unsigned char **pvaddr)
{
	uint32_t * cmd_ptr_vaddr = (uint32_t *)(*pvaddr);
	struct ce_cmdlists *cmdlist = &pce_dev->ce_dm.cmdlist;
	struct ce_cmdptrlists_ops *cmdptrlist = &pce_dev->ce_dm.cmdptrlist;

	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to aead operations
	 * defined in ce_cmdptrlists_ops structure.
	 */
	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->aead_aes_128_ccm = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_key_128);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_nonce_info);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_128_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_mask);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->aead_aes_256_ccm = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status_ocb);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_seg_size);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->reset_auth_byte_count);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_key_256);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_nonce_info);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_auth_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_cfg);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_256_key);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_aes_iv);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_cipher_mask);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->set_go_proc);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->ce_data_in);

	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->aead_ce_out = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->ce_data_out);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_status_wait);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	*pvaddr = (unsigned char *) cmd_ptr_vaddr;

	return 0;
}

static int qce_setup_cmdptrlists(struct qce_device *pce_dev,
					unsigned char **pvaddr)
{
	uint32_t * cmd_ptr_vaddr = (uint32_t *)(*pvaddr);
	struct ce_cmdlists *cmdlist = &pce_dev->ce_dm.cmdlist;
	struct ce_cmdptrlists_ops *cmdptrlist = &pce_dev->ce_dm.cmdptrlist;
	/*
	 * Designate chunks of the allocated memory to various
	 * command list pointers related to operations defined
	 * in ce_cmdptrlists_ops structure.
	 */
	cmd_ptr_vaddr = (uint32_t *) ALIGN(((unsigned int) cmd_ptr_vaddr), 16);
	cmdptrlist->probe_ce_hw = QCE_SET_CMD_PTR(cmd_ptr_vaddr);

	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->get_hw_version);
	*cmd_ptr_vaddr++ = QCE_SET_CMD_PTR(cmdlist->clear_status);
	*cmd_ptr_vaddr++ = QCE_SET_LAST_CMD_PTR(cmdlist->get_status_ocu);

	*pvaddr = (unsigned char *) cmd_ptr_vaddr;

	_setup_cipher_cmdptrlists(pce_dev, pvaddr);
	_setup_auth_cmdptrlists(pce_dev, pvaddr);
	_setup_aead_cmdptrlists(pce_dev, pvaddr);

	return 0;
}


static int qce_setup_ce_dm_data(struct qce_device *pce_dev)
{
	unsigned char *vaddr;

	/* 1. ce_in channel data xfer command src descriptors, 128 entries */
	vaddr = pce_dev->coh_vmem;
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr),  16);
	pce_dev->ce_dm.ce_in_src_desc = (struct dmov_desc *) vaddr;
	vaddr = vaddr + (sizeof(struct dmov_desc) * QCE_MAX_NUM_DESC);

	/* 2. ce_in channel data xfer command dst descriptors, 128 entries */
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr), 16);
	pce_dev->ce_dm.ce_in_dst_desc = (struct dmov_desc *) vaddr;
	vaddr = vaddr + (sizeof(struct dmov_desc) * QCE_MAX_NUM_DESC);


	/* 3. ce_out channel data xfer command src descriptors, 128 entries */
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr), 16);
	pce_dev->ce_dm.ce_out_src_desc = (struct dmov_desc *) vaddr;
	vaddr = vaddr + (sizeof(struct dmov_desc) * QCE_MAX_NUM_DESC);

	/* 4. ce_out channel data xfer command dst descriptors, 128 entries. */
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr), 16);
	pce_dev->ce_dm.ce_out_dst_desc = (struct dmov_desc *) vaddr;
	vaddr = vaddr + (sizeof(struct dmov_desc) * QCE_MAX_NUM_DESC);

	qce_setup_cmd_buffers(pce_dev, &vaddr);
	qce_setup_cmdlists(pce_dev, &vaddr);
	qce_setup_cmdptrlists(pce_dev, &vaddr);

	pce_dev->ce_dm.buffer.ignore_data = vaddr;

	pce_dev->ce_dm.phy_ce_pad = GET_PHYS_ADDR(pce_dev->ce_dm.buffer.pad);
	pce_dev->ce_dm.phy_ce_out_ignore  =
			GET_PHYS_ADDR(pce_dev->ce_dm.buffer.ignore_data);

	pce_dev->ce_dm.chan_ce_in_cmd->user = (void *) pce_dev;
	pce_dev->ce_dm.chan_ce_in_cmd->exec_func = NULL;

	pce_dev->ce_dm.chan_ce_out_cmd->user = (void *) pce_dev;
	pce_dev->ce_dm.chan_ce_out_cmd->exec_func = NULL;

	return 0;
}

static int _qce_start_dma(struct qce_device *pce_dev, bool ce_in, bool ce_out)
{

	if (ce_in)
		pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IN_PROG;
	else
		pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_COMP;

	if (ce_out)
		pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_IN_PROG;
	else
		pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_COMP;

	if (ce_in)
		msm_dmov_enqueue_cmd(pce_dev->ce_dm.chan_ce_in,
					pce_dev->ce_dm.chan_ce_in_cmd);
	if (ce_out)
		msm_dmov_enqueue_cmd(pce_dev->ce_dm.chan_ce_out,
					pce_dev->ce_dm.chan_ce_out_cmd);

	return 0;
};

int qce_aead_req(void *handle, struct qce_req *q_req)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	struct aead_request *areq = (struct aead_request *) q_req->areq;
	uint32_t authsize = q_req->authsize;
	uint32_t totallen_in, totallen_out, out_len;
	uint32_t pad_len_in, pad_len_out;
	int rc = 0;
	int ce_block_size;

	ce_block_size = pce_dev->ce_dm.ce_block_size;
	if (q_req->dir == QCE_ENCRYPT) {
		uint32_t pad_mac_len_out;

		q_req->cryptlen = areq->cryptlen;
		totallen_in = q_req->cryptlen + areq->assoclen;
		pad_len_in = ALIGN(totallen_in, ce_block_size) - totallen_in;

		out_len = areq->cryptlen + authsize;
		totallen_out = q_req->cryptlen + authsize + areq->assoclen;
		pad_mac_len_out = ALIGN(authsize, ce_block_size) - authsize;
		totallen_out += pad_mac_len_out;
		pad_len_out = ALIGN(totallen_out, ce_block_size) -
					totallen_out + pad_mac_len_out;

	} else {
		q_req->cryptlen = areq->cryptlen - authsize;
		totallen_in = areq->cryptlen + areq->assoclen;
		pad_len_in = ALIGN(totallen_in, ce_block_size) - totallen_in;

		out_len = q_req->cryptlen;
		totallen_out = totallen_in;
		pad_len_out = ALIGN(totallen_out, ce_block_size) - totallen_out;
		pad_len_out += authsize;
	}

	_chain_buffer_in_init(pce_dev);
	_chain_buffer_out_init(pce_dev);

	pce_dev->assoc_nents = 0;
	pce_dev->src_nents = 0;
	pce_dev->dst_nents = 0;
	pce_dev->ivsize = q_req->ivsize;
	pce_dev->authsize = q_req->authsize;

	/* associated data input */
	pce_dev->assoc_nents = count_sg(areq->assoc, areq->assoclen);
	qce_dma_map_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
					 DMA_TO_DEVICE);
	if (_chain_sg_buffer_in(pce_dev, areq->assoc, areq->assoclen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}
	/* cipher input */
	pce_dev->src_nents = count_sg(areq->src, areq->cryptlen);
	qce_dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	if (_chain_sg_buffer_in(pce_dev, areq->src, areq->cryptlen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}
	/* pad data in */
	if (pad_len_in) {
		if (_chain_pm_buffer_in(pce_dev, pce_dev->ce_dm.phy_ce_pad,
						pad_len_in) < 0) {
			rc = -ENOMEM;
			goto bad;
		}
	}

	/* ignore associated data */
	if (_chain_pm_buffer_out(pce_dev, pce_dev->ce_dm.phy_ce_out_ignore,
				areq->assoclen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}
	/* cipher + mac output  for encryption    */
	if (areq->src != areq->dst) {
		pce_dev->dst_nents = count_sg(areq->dst, out_len);
		qce_dma_map_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
	};
	if (_chain_sg_buffer_out(pce_dev, areq->dst, out_len) < 0) {
		rc = -ENOMEM;
		goto bad;
	}
	/* pad data out */
	if (pad_len_out) {
		if (_chain_pm_buffer_out(pce_dev, pce_dev->ce_dm.phy_ce_pad,
						pad_len_out) < 0) {
			rc = -ENOMEM;
			goto bad;
		}
	}

	/* finalize the ce_in and ce_out channels command lists */
	_ce_in_final(pce_dev, ALIGN(totallen_in, ce_block_size));
	_ce_out_final(pce_dev, ALIGN(totallen_out, ce_block_size));

	/* set up crypto device */
	rc = _ce_setup_cipher(pce_dev, q_req, totallen_in, areq->assoclen);
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to adm */
	pce_dev->areq = q_req->areq;
	pce_dev->qce_cb = q_req->qce_cb;

	pce_dev->ce_dm.chan_ce_in_cmd->complete_func = _aead_ce_in_call_back;
	pce_dev->ce_dm.chan_ce_out_cmd->complete_func = _aead_ce_out_call_back;

	_ce_in_dump(pce_dev);
	_ce_out_dump(pce_dev);

	rc = _qce_start_dma(pce_dev, true, true);
	if (rc == 0)
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
	if (pce_dev->dst_nents) {
		qce_dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
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

	uint32_t pad_len = ALIGN(areq->nbytes, pce_dev->ce_dm.ce_block_size)
						- areq->nbytes;

	_chain_buffer_in_init(pce_dev);
	_chain_buffer_out_init(pce_dev);

	pce_dev->src_nents = 0;
	pce_dev->dst_nents = 0;

	/* cipher input */
	pce_dev->src_nents = count_sg(areq->src, areq->nbytes);

	if (c_req->use_pmem != 1)
		qce_dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
								DMA_TO_DEVICE);
	else
		dma_map_pmem_sg(&c_req->pmem->src[0], pce_dev->src_nents,
								areq->src);

	if (_chain_sg_buffer_in(pce_dev, areq->src, areq->nbytes) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	/* cipher output */
	if (areq->src != areq->dst) {
		pce_dev->dst_nents = count_sg(areq->dst, areq->nbytes);
		if (c_req->use_pmem != 1)
			qce_dma_map_sg(pce_dev->pdev, areq->dst,
					pce_dev->dst_nents, DMA_FROM_DEVICE);
		else
			dma_map_pmem_sg(&c_req->pmem->dst[0],
					pce_dev->dst_nents, areq->dst);
	};
	if (_chain_sg_buffer_out(pce_dev, areq->dst, areq->nbytes) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	/* pad data */
	if (pad_len) {
		if (_chain_pm_buffer_in(pce_dev, pce_dev->ce_dm.phy_ce_pad,
						pad_len) < 0) {
			rc = -ENOMEM;
			goto bad;
		}
		if (_chain_pm_buffer_out(pce_dev, pce_dev->ce_dm.phy_ce_pad,
						pad_len) < 0) {
			rc = -ENOMEM;
			goto bad;
		}
	}

	/* finalize the ce_in and ce_out channels command lists */
	_ce_in_final(pce_dev, areq->nbytes + pad_len);
	_ce_out_final(pce_dev, areq->nbytes + pad_len);

	_ce_in_dump(pce_dev);
	_ce_out_dump(pce_dev);

	/* set up crypto device */
	rc = _ce_setup_cipher(pce_dev, c_req, areq->nbytes, 0);
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to adm */
	pce_dev->areq = areq;
	pce_dev->qce_cb = c_req->qce_cb;
	if (c_req->use_pmem == 1) {
		pce_dev->ce_dm.chan_ce_in_cmd->complete_func =
					_ablk_cipher_ce_in_call_back_pmem;
		pce_dev->ce_dm.chan_ce_out_cmd->complete_func =
					_ablk_cipher_ce_out_call_back_pmem;
	} else {
		pce_dev->ce_dm.chan_ce_in_cmd->complete_func =
					_ablk_cipher_ce_in_call_back;
		pce_dev->ce_dm.chan_ce_out_cmd->complete_func =
					_ablk_cipher_ce_out_call_back;
	}
	rc = _qce_start_dma(pce_dev, true, true);

	if (rc == 0)
		return 0;
bad:
	if (c_req->use_pmem != 1) {
			if (pce_dev->dst_nents) {
				qce_dma_unmap_sg(pce_dev->pdev, areq->dst,
				pce_dev->dst_nents, DMA_FROM_DEVICE);
			}
		if (pce_dev->src_nents) {
			qce_dma_unmap_sg(pce_dev->pdev, areq->src,
					pce_dev->src_nents,
					(areq->src == areq->dst) ?
						DMA_BIDIRECTIONAL :
						DMA_TO_DEVICE);
		}
	}
	return rc;
}
EXPORT_SYMBOL(qce_ablk_cipher_req);

int qce_process_sha_req(void *handle, struct qce_sha_req *sreq)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	int rc;
	uint32_t pad_len = ALIGN(sreq->size, pce_dev->ce_dm.ce_block_size) -
								sreq->size;
	struct ahash_request *areq = (struct ahash_request *)sreq->areq;

	_chain_buffer_in_init(pce_dev);
	pce_dev->src_nents = count_sg(sreq->src, sreq->size);
	qce_dma_map_sg(pce_dev->pdev, sreq->src, pce_dev->src_nents,
							DMA_TO_DEVICE);

	if (_chain_sg_buffer_in(pce_dev, sreq->src, sreq->size) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	if (pad_len) {
		if (_chain_pm_buffer_in(pce_dev, pce_dev->ce_dm.phy_ce_pad,
						pad_len) < 0) {
				rc = -ENOMEM;
				goto bad;
			}
	}
	 _ce_in_final(pce_dev, sreq->size + pad_len);

	_ce_in_dump(pce_dev);

	rc = _ce_setup_hash(pce_dev, sreq);

	if (rc < 0)
		goto bad;

	pce_dev->areq = areq;
	pce_dev->qce_cb = sreq->qce_cb;
	pce_dev->ce_dm.chan_ce_in_cmd->complete_func = _sha_ce_in_call_back;

	rc =  _qce_start_dma(pce_dev, true, false);

	if (rc == 0)
		return 0;
bad:
	if (pce_dev->src_nents) {
		qce_dma_unmap_sg(pce_dev->pdev, sreq->src,
				pce_dev->src_nents, DMA_TO_DEVICE);
	}

	return rc;
}
EXPORT_SYMBOL(qce_process_sha_req);

int qce_enable_clk(void *handle)
{
	return 0;
}
EXPORT_SYMBOL(qce_enable_clk);

int qce_disable_clk(void *handle)
{
	return 0;
}
EXPORT_SYMBOL(qce_disable_clk);

/* crypto engine open function. */
void *qce_open(struct platform_device *pdev, int *rc)
{
	struct qce_device *pce_dev;
	struct resource *resource;
	struct clk *ce_core_clk;
	struct clk *ce_clk;
	struct clk *ce_core_src_clk;
	int ret = 0;

	pce_dev = kzalloc(sizeof(struct qce_device), GFP_KERNEL);
	if (!pce_dev) {
		*rc = -ENOMEM;
		dev_err(&pdev->dev, "Can not allocate memory\n");
		return NULL;
	}
	pce_dev->pdev = &pdev->dev;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing MEM resource\n");
		goto err_pce_dev;
	};
	pce_dev->phy_iobase = resource->start;
	pce_dev->iobase = ioremap_nocache(resource->start,
				resource->end - resource->start + 1);
	if (!pce_dev->iobase) {
		*rc = -ENOMEM;
		dev_err(pce_dev->pdev, "Can not map io memory\n");
		goto err_pce_dev;
	}

	pce_dev->ce_dm.chan_ce_in_cmd = kzalloc(sizeof(struct msm_dmov_cmd),
			GFP_KERNEL);
	pce_dev->ce_dm.chan_ce_out_cmd = kzalloc(sizeof(struct msm_dmov_cmd),
			GFP_KERNEL);
	if (pce_dev->ce_dm.chan_ce_in_cmd == NULL ||
			pce_dev->ce_dm.chan_ce_out_cmd == NULL) {
		dev_err(pce_dev->pdev, "Can not allocate memory\n");
		*rc = -ENOMEM;
		goto err_dm_chan_cmd;
	}

	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
					"crypto_channels");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA channel resource\n");
		goto err_dm_chan_cmd;
	};
	pce_dev->ce_dm.chan_ce_in = resource->start;
	pce_dev->ce_dm.chan_ce_out = resource->end;
	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
				"crypto_crci_in");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA crci in resource\n");
		goto err_dm_chan_cmd;
	};
	pce_dev->ce_dm.crci_in = resource->start;
	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
				"crypto_crci_out");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA crci out resource\n");
		goto err_dm_chan_cmd;
	};
	pce_dev->ce_dm.crci_out = resource->start;
	pce_dev->memsize = 2 * PAGE_SIZE;
	pce_dev->coh_vmem = dma_alloc_coherent(pce_dev->pdev,
			pce_dev->memsize, &pce_dev->coh_pmem, GFP_KERNEL);

	if (pce_dev->coh_vmem == NULL) {
		*rc = -ENOMEM;
		dev_err(pce_dev->pdev, "Can not allocate coherent memory.\n");
		goto err;
	}

	/* Get CE3 src core clk. */
	ce_core_src_clk = clk_get(pce_dev->pdev, "ce3_core_src_clk");
	if (!IS_ERR(ce_core_src_clk)) {
		pce_dev->ce_core_src_clk = ce_core_src_clk;

		/* Set the core src clk @100Mhz */
		ret = clk_set_rate(pce_dev->ce_core_src_clk, 100000000);
		if (ret) {
			clk_put(pce_dev->ce_core_src_clk);
			goto err;
		}
	} else
		pce_dev->ce_core_src_clk = NULL;

	/* Get CE core clk */
	ce_core_clk = clk_get(pce_dev->pdev, "core_clk");
	if (IS_ERR(ce_core_clk)) {
		*rc = PTR_ERR(ce_core_clk);
		if (pce_dev->ce_core_src_clk != NULL)
			clk_put(pce_dev->ce_core_src_clk);
		goto err;
	}
	pce_dev->ce_core_clk = ce_core_clk;
	/* Get CE clk */
	ce_clk = clk_get(pce_dev->pdev, "iface_clk");
	if (IS_ERR(ce_clk)) {
		*rc = PTR_ERR(ce_clk);
		if (pce_dev->ce_core_src_clk != NULL)
			clk_put(pce_dev->ce_core_src_clk);
		clk_put(pce_dev->ce_core_clk);
		goto err;
	}
	pce_dev->ce_clk = ce_clk;

	/* Enable CE core clk */
	*rc = clk_prepare_enable(pce_dev->ce_core_clk);
	if (*rc) {
		if (pce_dev->ce_core_src_clk != NULL)
			clk_put(pce_dev->ce_core_src_clk);
		clk_put(pce_dev->ce_core_clk);
		clk_put(pce_dev->ce_clk);
		goto err;
	} else {
		/* Enable CE clk */
		*rc = clk_prepare_enable(pce_dev->ce_clk);
		if (*rc) {
			clk_disable_unprepare(pce_dev->ce_core_clk);
			if (pce_dev->ce_core_src_clk != NULL)
				clk_put(pce_dev->ce_core_src_clk);
			clk_put(pce_dev->ce_core_clk);
			clk_put(pce_dev->ce_clk);
			goto err;

		}
	}
	qce_setup_ce_dm_data(pce_dev);

	pce_dev->ce_dm.chan_ce_in_state = QCE_CHAN_STATE_IDLE;
	pce_dev->ce_dm.chan_ce_out_state = QCE_CHAN_STATE_IDLE;
	if (_init_ce_engine(pce_dev)) {
		*rc = -ENXIO;
		goto err;
	}
	*rc = 0;
	return pce_dev;

err:
	if (pce_dev->coh_vmem)
		dma_free_coherent(pce_dev->pdev, pce_dev->memsize,
			pce_dev->coh_vmem, pce_dev->coh_pmem);
err_dm_chan_cmd:
	kfree(pce_dev->ce_dm.chan_ce_in_cmd);
	kfree(pce_dev->ce_dm.chan_ce_out_cmd);
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

	if (pce_dev->ce_core_src_clk != NULL)
		clk_put(pce_dev->ce_core_src_clk);

	clk_put(pce_dev->ce_clk);
	clk_put(pce_dev->ce_core_clk);

	kfree(pce_dev->ce_dm.chan_ce_in_cmd);
	kfree(pce_dev->ce_dm.chan_ce_out_cmd);
	kfree(handle);

	return 0;
}
EXPORT_SYMBOL(qce_close);

int qce_hw_support(void *handle, struct ce_hw_support *ce_support)
{
	if (ce_support == NULL)
		return -EINVAL;

	ce_support->sha1_hmac_20 = false;
	ce_support->sha1_hmac = false;
	ce_support->sha256_hmac = false;
	ce_support->sha_hmac = false;
	ce_support->cmac  = true;
	ce_support->aes_key_192 = false;
	ce_support->aes_xts = true;
	ce_support->aes_ccm = true;
	ce_support->ota = false;
	ce_support->aligned_only = false;
	ce_support->is_shared = false;
	ce_support->bam = false;
	return 0;
}
EXPORT_SYMBOL(qce_hw_support);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto Engine driver");
