/* Qualcomm Crypto Engine driver.
 *
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <mach/dma.h>
#include <mach/clk.h>

#include "qce.h"
#include "qcryptohw_40.h"

/* ADM definitions */
#define LI_SG_CMD  (1 << 31)    /* last index in the scatter gather cmd */
#define SRC_INDEX_SG_CMD(index) ((index & 0x3fff) << 16)
#define DST_INDEX_SG_CMD(index) (index & 0x3fff)
#define ADM_DESC_LAST  (1 << 31)

/* Data xfer between DM and CE in blocks of 16 bytes */
#define ADM_CE_BLOCK_SIZE  16

#define ADM_DESC_LENGTH_MASK 0xffff
#define ADM_DESC_LENGTH(x)  (x & ADM_DESC_LENGTH_MASK)

struct dmov_desc {
	uint32_t addr;
	uint32_t len;
};

#define ADM_STATUS_OK 0x80000002

/* Misc definitions */

/* QCE max number of descriptor in a descriptor list */
#define QCE_MAX_NUM_DESC    128

/* State of DM channel */
enum qce_chan_st_enum {
	QCE_CHAN_STATE_IDLE = 0,
	QCE_CHAN_STATE_IN_PROG = 1,
	QCE_CHAN_STATE_COMP = 2,
	QCE_CHAN_STATE_LAST
};

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
	void __iomem *iobase;	    /* Virtual io base of CE HW  */
	unsigned int phy_iobase;    /* Physical io base of CE HW    */
	struct clk *ce_core_clk;	    /* Handle to CE clk */
	struct clk *ce_clk;	    /* Handle to CE clk */
	unsigned int crci_in;	      /* CRCI for CE DM IN Channel   */
	unsigned int crci_out;	      /* CRCI for CE DM OUT Channel   */
	unsigned int chan_ce_in;      /* ADM channel used for CE input
					* and auth result if authentication
					* only operation. */
	unsigned int chan_ce_out;     /* ADM channel used for CE output,
					and icv for esp */
	unsigned int *cmd_pointer_list_ce_in;
	dma_addr_t  phy_cmd_pointer_list_ce_in;

	unsigned int *cmd_pointer_list_ce_out;
	dma_addr_t  phy_cmd_pointer_list_ce_out;

	unsigned char *cmd_list_ce_in;
	dma_addr_t  phy_cmd_list_ce_in;

	unsigned char *cmd_list_ce_out;
	dma_addr_t  phy_cmd_list_ce_out;

	struct dmov_desc *ce_out_src_desc;
	dma_addr_t  phy_ce_out_src_desc;

	struct dmov_desc *ce_out_dst_desc;
	dma_addr_t  phy_ce_out_dst_desc;

	struct dmov_desc *ce_in_src_desc;
	dma_addr_t  phy_ce_in_src_desc;

	struct dmov_desc *ce_in_dst_desc;
	dma_addr_t  phy_ce_in_dst_desc;

	unsigned char *ce_out_ignore;
	dma_addr_t phy_ce_out_ignore;

	unsigned char *ce_pad;
	dma_addr_t phy_ce_pad;

	struct msm_dmov_cmd  *chan_ce_in_cmd;
	struct msm_dmov_cmd  *chan_ce_out_cmd;

	uint32_t ce_out_ignore_size;

	int ce_out_dst_desc_index;
	int ce_in_dst_desc_index;

	int ce_out_src_desc_index;
	int ce_in_src_desc_index;

	enum qce_chan_st_enum chan_ce_in_state;		/* chan ce_in state */
	enum qce_chan_st_enum chan_ce_out_state;	/* chan ce_out state */

	int chan_ce_in_status;		/* chan ce_in status      */
	int chan_ce_out_status;		/* chan ce_out status */

	unsigned char *dig_result;
	dma_addr_t phy_dig_result;

	/* cached aes key */
	uint32_t cipher_key[MAX_CIPHER_KEY_SIZE/sizeof(uint32_t)];

	uint32_t cipher_key_size;	/* cached aes key size in bytes */
	qce_comp_func_ptr_t qce_cb;	/* qce callback function pointer */

	int assoc_nents;
	int ivsize;
	int authsize;
	int src_nents;
	int dst_nents;

	void *areq;
	enum qce_cipher_mode_enum mode;

	dma_addr_t phy_iv_in;
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

static void _net_words_to_byte_stream(uint32_t *iv, unsigned char *b,
		unsigned int len)
{
	unsigned n = len  / sizeof(uint32_t);

	for (; n > 0; n--) {
		*b++ = (unsigned char) ((*iv >> 24)   & 0xff);
		*b++ = (unsigned char) ((*iv >> 16)   & 0xff);
		*b++ = (unsigned char) ((*iv >> 8)    & 0xff);
		*b++ = (unsigned char) (*iv           & 0xff);
		iv++;
	}
	n = len % sizeof(uint32_t);
	if (n == 3) {
		*b++ = (unsigned char) ((*iv >> 24)   & 0xff);
		*b++ = (unsigned char) ((*iv >> 16)   & 0xff);
		*b =   (unsigned char) ((*iv >> 8)    & 0xff);
	} else if (n == 2) {
		*b++ = (unsigned char) ((*iv >> 24)   & 0xff);
		*b =   (unsigned char) ((*iv >> 16)   & 0xff);
	} else if (n == 1) {
		*b =   (unsigned char) ((*iv >> 24)   & 0xff);
	}
}

static int count_sg(struct scatterlist *sg, int nbytes)
{
	int i;

	for (i = 0; nbytes > 0; i++, sg = sg_next(sg))
		nbytes -= sg->length;
	return i;
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

	val = readl_relaxed(pce_dev->iobase + CRYPTO_VERSION_REG);
	if (((val & 0xfffffff) != 0x0000042) &&
			((val & 0xfffffff) != 0x0000040)) {
		dev_err(pce_dev->pdev,
				"Unknown Qualcomm crypto device at 0x%x 0x%x\n",
				pce_dev->phy_iobase, val);
		return -EIO;
	};
	rev = (val & CRYPTO_CORE_REV_MASK);
	if (rev == 0x42) {
		dev_info(pce_dev->pdev,
				"Qualcomm Crypto 4.2 device found at 0x%x\n",
				pce_dev->phy_iobase);
	} else {
		if (rev == 0x40) {
			dev_info(pce_dev->pdev,
				"Qualcomm Crypto 4.0 device found at 0x%x\n",
							pce_dev->phy_iobase);
		}
	}

	dev_info(pce_dev->pdev,
			"IO base 0x%x, ce_in channel %d, "
			"ce_out channel %d, "
			"crci_in %d, crci_out %d\n",
			(unsigned int) pce_dev->iobase,
			pce_dev->chan_ce_in, pce_dev->chan_ce_out,
			pce_dev->crci_in, pce_dev->crci_out);

	pce_dev->cipher_key_size = 0;

	return 0;
};

static int _init_ce_engine(struct qce_device *pce_dev)
{
	unsigned int val;

	/* Reset ce */
	clk_reset(pce_dev->ce_core_clk, CLK_RESET_ASSERT);
	clk_reset(pce_dev->ce_core_clk, CLK_RESET_DEASSERT);
	/*
	 * Ensure previous instruction (any writes to CLK registers)
	 * to toggle the CLK reset lines was completed.
	 */
	dsb();
	/* configure ce */
	val = (1 << CRYPTO_MASK_DOUT_INTR) | (1 << CRYPTO_MASK_DIN_INTR) |
			(1 << CRYPTO_MASK_OP_DONE_INTR) |
					(1 << CRYPTO_MASK_ERR_INTR);
	writel_relaxed(val, pce_dev->iobase + CRYPTO_CONFIG_REG);
	/*
	 * Ensure previous instruction (writel_relaxed to config register bit)
	 * was completed.
	 */
	dsb();
	val = readl_relaxed(pce_dev->iobase + CRYPTO_CONFIG_REG);
	if (!val) {
		dev_err(pce_dev->pdev,
				"unknown Qualcomm crypto device at 0x%x\n",
				pce_dev->phy_iobase);
		return -EIO;
	};
	if (_probe_ce_engine(pce_dev) < 0)
		return -EIO;
	return 0;
};

static int _ce_setup_hash(struct qce_device *pce_dev, struct qce_sha_req *sreq)
{
	uint32_t auth32[SHA256_DIGEST_SIZE / sizeof(uint32_t)];
	uint32_t diglen;
	int i;
	uint32_t auth_cfg = 0;
	bool sha1 = false;

	if (sreq->alg ==  QCE_HASH_AES_CMAC) {
		uint32_t authkey32[SHA_HMAC_KEY_SIZE/sizeof(uint32_t)] = {
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		uint32_t authklen32 = sreq->authklen/(sizeof(uint32_t));
		/* Clear auth_ivn, auth_keyn registers  */
		for (i = 0; i < 16; i++) {
			writel_relaxed(0, (pce_dev->iobase +
				(CRYPTO_AUTH_IV0_REG + i * sizeof(uint32_t))));
			writel_relaxed(0, (pce_dev->iobase +
				(CRYPTO_AUTH_KEY0_REG + i * sizeof(uint32_t))));
		}
		/* write auth_bytecnt 0/1/2/3, start with 0 */
		for (i = 0; i < 4; i++)
			writel_relaxed(0, pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT0_REG +
						i * sizeof(uint32_t));

		_byte_stream_to_net_words(authkey32, sreq->authkey,
						sreq->authklen);
		for (i = 0; i < authklen32; i++)
			writel_relaxed(authkey32[i], pce_dev->iobase +
				CRYPTO_AUTH_KEY0_REG + (i * sizeof(uint32_t)));
		/*
		 * write seg_cfg
		 */
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
		uint32_t hmackey[SHA_HMAC_KEY_SIZE/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
		uint32_t hmacklen = sreq->authklen/(sizeof(uint32_t));

		_byte_stream_to_net_words(hmackey, sreq->authkey,
						sreq->authklen);
		/* write hmac key */
		for (i = 0; i < hmacklen; i++)
			writel_relaxed(hmackey[i], pce_dev->iobase +
				CRYPTO_AUTH_KEY0_REG + (i * sizeof(uint32_t)));

		auth_cfg |= (CRYPTO_AUTH_MODE_HMAC << CRYPTO_AUTH_MODE);
	} else {
		auth_cfg |= (CRYPTO_AUTH_MODE_HASH << CRYPTO_AUTH_MODE);
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

	for (i = 0; i < 5; i++)
		writel_relaxed(auth32[i], (pce_dev->iobase +
				(CRYPTO_AUTH_IV0_REG + i * sizeof(uint32_t))));

	if ((sreq->alg == QCE_HASH_SHA256) ||
			(sreq->alg == QCE_HASH_SHA256_HMAC)) {
		writel_relaxed(auth32[5], pce_dev->iobase +
							CRYPTO_AUTH_IV5_REG);
		writel_relaxed(auth32[6], pce_dev->iobase +
							CRYPTO_AUTH_IV6_REG);
		writel_relaxed(auth32[7], pce_dev->iobase +
							CRYPTO_AUTH_IV7_REG);
	}

	/* write auth_bytecnt 0/1, start with 0 */
	for (i = 0; i < 4; i++)
		writel_relaxed(sreq->auth_data[i], (pce_dev->iobase +
			(CRYPTO_AUTH_BYTECNT0_REG + i * sizeof(uint32_t))));

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

	 /* write seg_cfg */
	writel_relaxed(auth_cfg, pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);

	/* write seg_size   */
	writel_relaxed(sreq->size, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);

	/* write auth_seg_size */
	writel_relaxed(sreq->size, pce_dev->iobase + CRYPTO_AUTH_SEG_SIZE_REG);

	/* write auth_seg_start   */
	writel_relaxed(0, pce_dev->iobase + CRYPTO_AUTH_SEG_START_REG);
	/*
	 * Ensure previous instructions (write to all AUTH registers)
	 * was completed before accessing a register that is not in
	 * in the same 1K range.
	 */
	dsb();

	writel_relaxed(0, pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);
	/*
	 * Ensure previous instructions (setting all the CE registers)
	 * was completed before writing to GO register
	 */
	dsb();
	/* issue go to crypto   */
	writel_relaxed(1 << CRYPTO_GO, pce_dev->iobase + CRYPTO_GOPROC_REG);
	/*
	 * Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	dsb();

	return 0;
}

static int _ce_setup_cipher(struct qce_device *pce_dev, struct qce_req *creq,
		uint32_t totallen_in, uint32_t coffset)
{
	uint32_t enckey32[(MAX_CIPHER_KEY_SIZE * 2)/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t enciv32[MAX_IV_LENGTH / sizeof(uint32_t)] = {
			0, 0, 0, 0};
	uint32_t enck_size_in_word = creq->encklen / sizeof(uint32_t);
	int aes_key_chg;
	int i;
	uint32_t encr_cfg = 0;
	uint32_t ivsize = creq->ivsize;

	if (creq->mode ==  QCE_MODE_XTS)
		_byte_stream_to_net_words(enckey32, creq->enckey,
						creq->encklen/2);
	else
		_byte_stream_to_net_words(enckey32, creq->enckey,
							creq->encklen);

	if ((creq->op == QCE_REQ_AEAD) && (creq->mode == QCE_MODE_CCM)) {
		uint32_t authklen32 = creq->encklen/sizeof(uint32_t);
		uint32_t noncelen32 = MAX_NONCE/sizeof(uint32_t);
		uint32_t nonce32[MAX_NONCE/sizeof(uint32_t)] = {0, 0, 0, 0};
		uint32_t auth_cfg = 0;

		/* Clear auth_ivn, auth_keyn registers  */
		for (i = 0; i < 16; i++) {
			writel_relaxed(0, (pce_dev->iobase +
				(CRYPTO_AUTH_IV0_REG + i*sizeof(uint32_t))));
			writel_relaxed(0, (pce_dev->iobase +
				(CRYPTO_AUTH_KEY0_REG + i*sizeof(uint32_t))));
		}
		/* write auth_bytecnt 0/1/2/3, start with 0 */
		for (i = 0; i < 4; i++)
			writel_relaxed(0, pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT0_REG +
						i * sizeof(uint32_t));
		/* write auth key */
		for (i = 0; i < authklen32; i++)
			writel_relaxed(enckey32[i], pce_dev->iobase +
				CRYPTO_AUTH_KEY0_REG + (i*sizeof(uint32_t)));

		/* write nonce */
		_byte_stream_to_net_words(nonce32, creq->nonce, MAX_NONCE);
		for (i = 0; i < noncelen32; i++)
			writel_relaxed(nonce32[i], pce_dev->iobase +
				CRYPTO_AUTH_INFO_NONCE0_REG +
					(i*sizeof(uint32_t)));

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
		writel_relaxed(auth_cfg, pce_dev->iobase +
						CRYPTO_AUTH_SEG_CFG_REG);
		if (creq->dir == QCE_ENCRYPT)
			writel_relaxed(totallen_in, pce_dev->iobase +
						CRYPTO_AUTH_SEG_SIZE_REG);
		else
			writel_relaxed((totallen_in - creq->authsize),
				pce_dev->iobase + CRYPTO_AUTH_SEG_SIZE_REG);
		writel_relaxed(0, pce_dev->iobase + CRYPTO_AUTH_SEG_START_REG);
	} else {
		writel_relaxed(0, pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);
	}
	/*
	 * Ensure previous instructions (write to all AUTH registers)
	 * was completed before accessing a register that is not in
	 * in the same 1K range.
	 */
	dsb();

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
			writel_relaxed(enciv32[0], pce_dev->iobase +
						CRYPTO_CNTR0_IV0_REG);
			writel_relaxed(enciv32[1], pce_dev->iobase +
						CRYPTO_CNTR1_IV1_REG);
		}
		writel_relaxed(enckey32[0], pce_dev->iobase +
							CRYPTO_ENCR_KEY0_REG);
		writel_relaxed(enckey32[1], pce_dev->iobase +
							CRYPTO_ENCR_KEY1_REG);
		encr_cfg |= ((CRYPTO_ENCR_KEY_SZ_DES << CRYPTO_ENCR_KEY_SZ)  |
				(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG));
		break;

	case CIPHER_ALG_3DES:
		if (creq->mode !=  QCE_MODE_ECB) {
			_byte_stream_to_net_words(enciv32, creq->iv, ivsize);
			writel_relaxed(enciv32[0], pce_dev->iobase +
						CRYPTO_CNTR0_IV0_REG);
			writel_relaxed(enciv32[1], pce_dev->iobase +
						CRYPTO_CNTR1_IV1_REG);
		}
		for (i = 0; i < 6; i++)
			writel_relaxed(enckey32[0], (pce_dev->iobase +
				(CRYPTO_ENCR_KEY0_REG + i * sizeof(uint32_t))));

		encr_cfg |= ((CRYPTO_ENCR_KEY_SZ_3DES << CRYPTO_ENCR_KEY_SZ)  |
				(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG));
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
			for (i = 0; i < xtsklen; i++)
				writel_relaxed(xtskey32[i], pce_dev->iobase +
					CRYPTO_ENCR_XTS_KEY0_REG +
					(i * sizeof(uint32_t)));

				writel_relaxed(creq->cryptlen ,
						pce_dev->iobase +
						CRYPTO_ENCR_XTS_DU_SIZE_REG);
		}
		if (creq->mode !=  QCE_MODE_ECB) {
			if (creq->mode ==  QCE_MODE_XTS)
				_byte_stream_swap_to_net_words(enciv32,
							creq->iv, ivsize);
			else
				_byte_stream_to_net_words(enciv32, creq->iv,
								ivsize);
			for (i = 0; i <= 3; i++)
				writel_relaxed(enciv32[i], pce_dev->iobase +
							CRYPTO_CNTR0_IV0_REG +
							(i * sizeof(uint32_t)));
		}
		/* set number of counter bits */
		writel_relaxed(0xffffffff, pce_dev->iobase +
							CRYPTO_CNTR_MASK_REG);

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

				/* check for null key. If null, use hw key*/
				for (i = 0; i < enck_size_in_word; i++) {
					if (enckey32[i] != 0)
						break;
				}
				if (i == enck_size_in_word)
					encr_cfg |= 1 << CRYPTO_USE_HW_KEY;
				break;
			} /* end of switch (creq->encklen) */

			encr_cfg |= CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG;
			if (pce_dev->cipher_key_size !=  creq->encklen)
				aes_key_chg = 1;
			else {
				for (i = 0; i < enck_size_in_word; i++) {
					if (enckey32[i]
						!= pce_dev->cipher_key[i])
						break;
				}
				aes_key_chg = (i == enck_size_in_word) ? 0 : 1;
			}

			if (aes_key_chg) {
				for (i = 0; i < enck_size_in_word; i++)
					writel_relaxed(enckey32[i],
							pce_dev->iobase +
							CRYPTO_ENCR_KEY0_REG +
							(i * sizeof(uint32_t)));
				pce_dev->cipher_key_size = creq->encklen;
				for (i = 0; i < enck_size_in_word; i++)
					pce_dev->cipher_key[i] = enckey32[i];
			} /*if (aes_key_chg) { */
		} /* else of if (creq->op == QCE_REQ_ABLK_CIPHER_NO_KEY) */
		break;
	} /* end of switch (creq->mode)  */

	/* write encr seg cfg */
	encr_cfg |= ((creq->dir == QCE_ENCRYPT) ? 1 : 0) << CRYPTO_ENCODE;

	/* write encr seg cfg */
	writel_relaxed(encr_cfg, pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);

	/* write encr seg size */
	if ((creq->mode == QCE_MODE_CCM) && (creq->dir == QCE_DECRYPT))
		writel_relaxed((creq->cryptlen + creq->authsize),
				pce_dev->iobase + CRYPTO_ENCR_SEG_SIZE_REG);
	else
		writel_relaxed(creq->cryptlen,
				pce_dev->iobase + CRYPTO_ENCR_SEG_SIZE_REG);
	/* write encr seg start */
	writel_relaxed((coffset & 0xffff),
			pce_dev->iobase + CRYPTO_ENCR_SEG_START_REG);
	/* write seg size  */
	writel_relaxed(totallen_in, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);
	/*
	 * Ensure previous instructions (setting all the CE registers)
	 * was completed before writing to GO register
	 */
	dsb();
	/* issue go to crypto   */
	writel_relaxed(1 << CRYPTO_GO, pce_dev->iobase + CRYPTO_GOPROC_REG);
	/*
	 * Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	dsb();
	return 0;
};

static int _aead_complete(struct qce_device *pce_dev)
{
	struct aead_request *areq;
	int i;
	uint32_t ivsize;
	uint32_t iv_out[4];
	unsigned char iv[4 * sizeof(uint32_t)];

	areq = (struct aead_request *) pce_dev->areq;
	ivsize = pce_dev->ivsize;

	if (areq->src != areq->dst) {
		dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
					DMA_FROM_DEVICE);
	}
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);

	if (pce_dev->mode != QCE_MODE_CCM)
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_iv_in,
				ivsize, DMA_TO_DEVICE);
	dma_unmap_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
			DMA_TO_DEVICE);

	/* get iv out */
	if ((pce_dev->mode == QCE_MODE_ECB) ||
					(pce_dev->mode == QCE_MODE_CCM)) {
		if (pce_dev->mode == QCE_MODE_CCM) {
			int result;
			result = readl_relaxed(pce_dev->iobase +
							CRYPTO_STATUS_REG);
			result &= (1 << CRYPTO_MAC_FAILED);
			result |= (pce_dev->chan_ce_in_status |
						pce_dev->chan_ce_out_status);
			dsb();
			pce_dev->qce_cb(areq, pce_dev->dig_result, NULL,
								result);
		} else {
			pce_dev->qce_cb(areq, pce_dev->dig_result, NULL,
					pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
		}
	} else {
		for (i = 0; i < 4; i++)
			iv_out[i] = readl_relaxed(pce_dev->iobase +
				(CRYPTO_CNTR0_IV0_REG + i * sizeof(uint32_t)));

		_net_words_to_byte_stream(iv_out, iv, sizeof(iv));
		pce_dev->qce_cb(areq, pce_dev->dig_result, iv,
				pce_dev->chan_ce_in_status |
				pce_dev->chan_ce_out_status);
	};
	return 0;
};

static void _sha_complete(struct qce_device *pce_dev)
{

	struct ahash_request *areq;
	uint32_t auth_data[4];
	uint32_t digest[8];
	int i;

	areq = (struct ahash_request *) pce_dev->areq;
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
				DMA_TO_DEVICE);

	for (i = 0; i < 4; i++)
		auth_data[i] = readl_relaxed(pce_dev->iobase +
				(CRYPTO_AUTH_BYTECNT0_REG +
					i * sizeof(uint32_t)));

	for (i = 0; i < 8; i++)
		digest[i] = readl_relaxed(pce_dev->iobase +
			CRYPTO_AUTH_IV0_REG + (i * sizeof(uint32_t)));

	_net_words_to_byte_stream(digest, pce_dev->dig_result,
						SHA256_DIGEST_SIZE);

	pce_dev->qce_cb(areq,  pce_dev->dig_result, (unsigned char *)auth_data,
				pce_dev->chan_ce_in_status);
};

static int _ablk_cipher_complete(struct qce_device *pce_dev)
{
	struct ablkcipher_request *areq;
	uint32_t iv_out[4];
	unsigned char iv[4 * sizeof(uint32_t)];

	areq = (struct ablkcipher_request *) pce_dev->areq;

	if (areq->src != areq->dst) {
		dma_unmap_sg(pce_dev->pdev, areq->dst,
			pce_dev->dst_nents, DMA_FROM_DEVICE);
	}
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
		(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
						DMA_TO_DEVICE);
	/* get iv out */
	if (pce_dev->mode == QCE_MODE_ECB) {
		pce_dev->qce_cb(areq, NULL, NULL, pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
	} else {
		int i;

		for (i = 0; i < 4; i++)
			iv_out[i] = readl_relaxed(pce_dev->iobase +
				(CRYPTO_CNTR0_IV0_REG + i * sizeof(uint32_t)));

		_net_words_to_byte_stream(iv_out, iv, sizeof(iv));
		pce_dev->qce_cb(areq, NULL, iv, pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
	}

	return 0;
};

static int _ablk_cipher_use_pmem_complete(struct qce_device *pce_dev)
{
	struct ablkcipher_request *areq;
	uint32_t iv_out[4];
	unsigned char iv[4 * sizeof(uint32_t)];

	areq = (struct ablkcipher_request *) pce_dev->areq;

	/* get iv out */
	if (pce_dev->mode == QCE_MODE_ECB) {
		pce_dev->qce_cb(areq, NULL, NULL, pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
	} else {
		int i;

		for (i = 0; i < 4; i++)
			iv_out[i] = readl_relaxed(pce_dev->iobase +
				CRYPTO_CNTR0_IV0_REG + (i * sizeof(uint32_t)));

		_net_words_to_byte_stream(iv_out, iv, sizeof(iv));
		pce_dev->qce_cb(areq, NULL, iv, pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
	}

	return 0;
};

static int qce_split_and_insert_dm_desc(struct dmov_desc *pdesc,
			unsigned int plen, unsigned int paddr, int *index)
{
	while (plen > 0x8000) {
		pdesc->len = 0x8000;
		if (paddr > 0) {
			pdesc->addr = paddr;
			paddr += 0x8000;
		}
		plen -= pdesc->len;
		if (plen > 0) {
			*index = (*index) + 1;
			if ((*index) >= QCE_MAX_NUM_DESC)
				return -ENOMEM;
			pdesc++;
		}
	}
	if ((plen > 0) && (plen <= 0x8000)) {
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

	pdesc = pce_dev->ce_in_dst_desc + pce_dev->ce_in_dst_desc_index;
	if (nbytes > 0x8000)
		qce_split_and_insert_dm_desc(pdesc, nbytes, 0,
				&pce_dev->ce_in_dst_desc_index);
	else
		pdesc->len = nbytes;

	pdesc = pce_dev->ce_in_src_desc + pce_dev->ce_in_src_desc_index;
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
			if (pdesc->len > 0x8000)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
						sg_dma_address(sg),
						&pce_dev->ce_in_src_desc_index);
		} else if (sg_dma_address(sg) == (pdesc->addr + dlen)) {
			pdesc->len  = dlen + len;
			if (pdesc->len > 0x8000)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
						pdesc->addr,
						&pce_dev->ce_in_src_desc_index);
		} else {
			pce_dev->ce_in_src_desc_index++;
			if (pce_dev->ce_in_src_desc_index >= QCE_MAX_NUM_DESC)
				return -ENOMEM;
			pdesc++;
			pdesc->len = len;
			pdesc->addr = sg_dma_address(sg);
			if (pdesc->len > 0x8000)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
						sg_dma_address(sg),
						&pce_dev->ce_in_src_desc_index);
		}
		if (nbytes > 0)
			sg = sg_next(sg);
	}
	return 0;
}

static int _chain_pm_buffer_in(struct qce_device *pce_dev,
		unsigned int pmem, unsigned int nbytes)
{
	unsigned int dlen;
	struct dmov_desc *pdesc;

	pdesc = pce_dev->ce_in_src_desc + pce_dev->ce_in_src_desc_index;
	dlen = pdesc->len & ADM_DESC_LENGTH_MASK;
	if (dlen == 0) {
		pdesc->addr  = pmem;
		pdesc->len = nbytes;
	} else if (pmem == (pdesc->addr + dlen)) {
		pdesc->len  = dlen + nbytes;
	} else {
		pce_dev->ce_in_src_desc_index++;
		if (pce_dev->ce_in_src_desc_index >= QCE_MAX_NUM_DESC)
			return -ENOMEM;
		pdesc++;
		pdesc->len = nbytes;
		pdesc->addr = pmem;
	}
	pdesc = pce_dev->ce_in_dst_desc + pce_dev->ce_in_dst_desc_index;
	pdesc->len += nbytes;

	return 0;
}

static void _chain_buffer_in_init(struct qce_device *pce_dev)
{
	struct dmov_desc *pdesc;

	pce_dev->ce_in_src_desc_index = 0;
	pce_dev->ce_in_dst_desc_index = 0;
	pdesc = pce_dev->ce_in_src_desc;
	pdesc->len = 0;
}

static void _ce_in_final(struct qce_device *pce_dev, unsigned total)
{
	struct dmov_desc *pdesc;
	dmov_sg *pcmd;

	pdesc = pce_dev->ce_in_src_desc + pce_dev->ce_in_src_desc_index;
	pdesc->len |= ADM_DESC_LAST;
	pdesc = pce_dev->ce_in_dst_desc + pce_dev->ce_in_dst_desc_index;
	pdesc->len |= ADM_DESC_LAST;

	pcmd = (dmov_sg *) pce_dev->cmd_list_ce_in;
	pcmd->cmd |= CMD_LC;
}

#ifdef QCE_DEBUG
static void _ce_in_dump(struct qce_device *pce_dev)
{
	int i;
	struct dmov_desc *pdesc;

	dev_info(pce_dev->pdev, "_ce_in_dump: src\n");
	for (i = 0; i <= pce_dev->ce_in_src_desc_index; i++) {
		pdesc = pce_dev->ce_in_src_desc + i;
		dev_info(pce_dev->pdev, "%x , %x\n", pdesc->addr,
				pdesc->len);
	}
	dev_info(pce_dev->pdev, "_ce_in_dump: dst\n");
	for (i = 0; i <= pce_dev->ce_in_dst_desc_index; i++) {
		pdesc = pce_dev->ce_in_dst_desc + i;
		dev_info(pce_dev->pdev, "%x , %x\n", pdesc->addr,
				pdesc->len);
	}
};

static void _ce_out_dump(struct qce_device *pce_dev)
{
	int i;
	struct dmov_desc *pdesc;

	dev_info(pce_dev->pdev, "_ce_out_dump: src\n");
	for (i = 0; i <= pce_dev->ce_out_src_desc_index; i++) {
		pdesc = pce_dev->ce_out_src_desc + i;
		dev_info(pce_dev->pdev, "%x , %x\n", pdesc->addr,
				pdesc->len);
	}

	dev_info(pce_dev->pdev, "_ce_out_dump: dst\n");
	for (i = 0; i <= pce_dev->ce_out_dst_desc_index; i++) {
		pdesc = pce_dev->ce_out_dst_desc + i;
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

	pdesc = pce_dev->ce_out_src_desc + pce_dev->ce_out_src_desc_index;
	if (nbytes > 0x8000)
		qce_split_and_insert_dm_desc(pdesc, nbytes, 0,
				&pce_dev->ce_out_src_desc_index);
	else
		pdesc->len = nbytes;

	pdesc = pce_dev->ce_out_dst_desc + pce_dev->ce_out_dst_desc_index;
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
			if (pdesc->len > 0x8000)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					sg_dma_address(sg),
					&pce_dev->ce_out_dst_desc_index);
		} else if (sg_dma_address(sg) == (pdesc->addr + dlen)) {
			pdesc->len  = dlen + len;
			if (pdesc->len > 0x8000)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					pdesc->addr,
					&pce_dev->ce_out_dst_desc_index);

		} else {
			pce_dev->ce_out_dst_desc_index++;
			if (pce_dev->ce_out_dst_desc_index >= QCE_MAX_NUM_DESC)
				return -EIO;
			pdesc++;
			pdesc->len = len;
			pdesc->addr = sg_dma_address(sg);
			if (pdesc->len > 0x8000)
				qce_split_and_insert_dm_desc(pdesc, pdesc->len,
					sg_dma_address(sg),
					&pce_dev->ce_out_dst_desc_index);

		}
		if (nbytes > 0)
			sg = sg_next(sg);
	}
	return 0;
}

static int _chain_pm_buffer_out(struct qce_device *pce_dev,
		unsigned int pmem, unsigned int nbytes)
{
	unsigned int dlen;
	struct dmov_desc *pdesc;

	pdesc = pce_dev->ce_out_dst_desc + pce_dev->ce_out_dst_desc_index;
	dlen = pdesc->len & ADM_DESC_LENGTH_MASK;

	if (dlen == 0) {
		pdesc->addr  = pmem;
		pdesc->len = nbytes;
	} else if (pmem == (pdesc->addr + dlen)) {
		pdesc->len  = dlen + nbytes;
	} else {
		pce_dev->ce_out_dst_desc_index++;
		if (pce_dev->ce_out_dst_desc_index >= QCE_MAX_NUM_DESC)
			return -EIO;
		pdesc++;
		pdesc->len = nbytes;
		pdesc->addr = pmem;
	}
	pdesc = pce_dev->ce_out_src_desc + pce_dev->ce_out_src_desc_index;
	pdesc->len += nbytes;

	return 0;
};

static void _chain_buffer_out_init(struct qce_device *pce_dev)
{
	struct dmov_desc *pdesc;

	pce_dev->ce_out_dst_desc_index = 0;
	pce_dev->ce_out_src_desc_index = 0;
	pdesc = pce_dev->ce_out_dst_desc;
	pdesc->len = 0;
};

static void _ce_out_final(struct qce_device *pce_dev, unsigned total)
{
	struct dmov_desc *pdesc;
	dmov_sg *pcmd;

	pdesc = pce_dev->ce_out_dst_desc + pce_dev->ce_out_dst_desc_index;
	pdesc->len |= ADM_DESC_LAST;
	pdesc = pce_dev->ce_out_src_desc + pce_dev->ce_out_src_desc_index;
	pdesc->len |= ADM_DESC_LAST;
	pcmd = (dmov_sg *) pce_dev->cmd_list_ce_out;
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
		pce_dev->chan_ce_in_status = -1;
	} else {
		pce_dev->chan_ce_in_status = 0;
	}

	pce_dev->chan_ce_in_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->chan_ce_out_state == QCE_CHAN_STATE_COMP) {
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

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
		pce_dev->chan_ce_out_status = -1;
	} else {
		pce_dev->chan_ce_out_status = 0;
	};

	pce_dev->chan_ce_out_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->chan_ce_in_state == QCE_CHAN_STATE_COMP) {
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

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
		pce_dev->chan_ce_in_status = -1;
	} else {
		pce_dev->chan_ce_in_status = 0;
	}
	pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
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
		pce_dev->chan_ce_in_status = -1;
	} else {
		pce_dev->chan_ce_in_status = 0;
	}

	pce_dev->chan_ce_in_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->chan_ce_out_state == QCE_CHAN_STATE_COMP) {
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

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
		pce_dev->chan_ce_out_status = -1;
	} else {
		pce_dev->chan_ce_out_status = 0;
	};

	pce_dev->chan_ce_out_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->chan_ce_in_state == QCE_CHAN_STATE_COMP) {
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

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
		pce_dev->chan_ce_in_status = -1;
	} else {
		pce_dev->chan_ce_in_status = 0;
	}

	pce_dev->chan_ce_in_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->chan_ce_out_state == QCE_CHAN_STATE_COMP) {
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

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
		pce_dev->chan_ce_out_status = -1;
	} else {
		pce_dev->chan_ce_out_status = 0;
	};

	pce_dev->chan_ce_out_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->chan_ce_in_state == QCE_CHAN_STATE_COMP) {
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

		/* done */
		_ablk_cipher_use_pmem_complete(pce_dev);
	}
};

static int _setup_cmd_template(struct qce_device *pce_dev)
{
	dmov_sg *pcmd;
	struct dmov_desc *pdesc;
	unsigned char *vaddr;
	int i = 0;

	/* Divide up the 4K coherent memory */

	/* 1. ce_in channel 1st command src descriptors, 128 entries */
	vaddr = pce_dev->coh_vmem;
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr), 16);
	pce_dev->ce_in_src_desc = (struct dmov_desc *) vaddr;
	pce_dev->phy_ce_in_src_desc = pce_dev->coh_pmem +
			 (vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + (sizeof(struct dmov_desc) * QCE_MAX_NUM_DESC);

	/* 2. ce_in channel 1st command dst descriptor, 1 entry */
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr), 16);
	pce_dev->ce_in_dst_desc = (struct dmov_desc *) vaddr;
	pce_dev->phy_ce_in_dst_desc = pce_dev->coh_pmem +
			 (vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + (sizeof(struct dmov_desc) * QCE_MAX_NUM_DESC);

	/* 3. ce_in channel command list of one scatter gather command */
	pce_dev->cmd_list_ce_in = vaddr;
	pce_dev->phy_cmd_list_ce_in = pce_dev->coh_pmem
			 + (vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + sizeof(dmov_sg);

	/* 4. authentication result. */
	pce_dev->dig_result = vaddr;
	pce_dev->phy_dig_result = pce_dev->coh_pmem +
			(vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + SHA256_DIGESTSIZE;

	/* 5. ce_out channel command list of one scatter gather command */
	pce_dev->cmd_list_ce_out = vaddr;
	pce_dev->phy_cmd_list_ce_out = pce_dev->coh_pmem
			 + (vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + sizeof(dmov_sg);

	/* 6. ce_out channel command src descriptors, 1 entry */
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr), 16);
	pce_dev->ce_out_src_desc = (struct dmov_desc *) vaddr;
	pce_dev->phy_ce_out_src_desc = pce_dev->coh_pmem
			 + (vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + (sizeof(struct dmov_desc) * QCE_MAX_NUM_DESC);

	/* 7. ce_out channel command dst descriptors, 128 entries.  */
	vaddr = (unsigned char *) ALIGN(((unsigned int)vaddr), 16);
	pce_dev->ce_out_dst_desc = (struct dmov_desc *) vaddr;
	pce_dev->phy_ce_out_dst_desc = pce_dev->coh_pmem
			 + (vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + (sizeof(struct dmov_desc) * QCE_MAX_NUM_DESC);

	/* 8. pad area. */
	pce_dev->ce_pad = vaddr;
	pce_dev->phy_ce_pad = pce_dev->coh_pmem +
			(vaddr - pce_dev->coh_vmem);

	/* Padding length is set to twice for worst case scenario in AES-CCM */
	vaddr = vaddr + 2 * ADM_CE_BLOCK_SIZE;

	/* 9. ce_in channel command pointer list.	 */
	vaddr = (unsigned char *) ALIGN(((unsigned int) vaddr), 8);
	pce_dev->cmd_pointer_list_ce_in = (unsigned int *) vaddr;
	pce_dev->phy_cmd_pointer_list_ce_in = pce_dev->coh_pmem +
			(vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + sizeof(unsigned char *);

	/* 10. ce_ou channel command pointer list. */
	vaddr = (unsigned char *) ALIGN(((unsigned int) vaddr), 8);
	pce_dev->cmd_pointer_list_ce_out = (unsigned int *) vaddr;
	pce_dev->phy_cmd_pointer_list_ce_out =  pce_dev->coh_pmem +
			(vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + sizeof(unsigned char *);

	/* 11. throw away area to store by-pass data from ce_out. */
	pce_dev->ce_out_ignore = (unsigned char *) vaddr;
	pce_dev->phy_ce_out_ignore  = pce_dev->coh_pmem
			+ (vaddr - pce_dev->coh_vmem);
	pce_dev->ce_out_ignore_size = PAGE_SIZE - (vaddr -
			pce_dev->coh_vmem);  /* at least 1.5 K of space */
	/*
	 * The first command of command list ce_in is for the input of
	 * concurrent operation of encrypt/decrypt or for the input
	 * of authentication.
	 */
	pcmd = (dmov_sg *) pce_dev->cmd_list_ce_in;
	/* swap byte and half word , dst crci ,  scatter gather */
	pcmd->cmd = CMD_DST_SWAP_BYTES | CMD_DST_SWAP_SHORTS |
			CMD_DST_CRCI(pce_dev->crci_in) | CMD_MODE_SG;
	pdesc = pce_dev->ce_in_src_desc;
	pdesc->addr = 0;	/* to be filled in each operation */
	pdesc->len = 0;		/* to be filled in each operation */
	pcmd->src_dscr = (unsigned) pce_dev->phy_ce_in_src_desc;

	pdesc = pce_dev->ce_in_dst_desc;
	for (i = 0; i < QCE_MAX_NUM_DESC; i++) {
		pdesc->addr = (CRYPTO_DATA_SHADOW0 + pce_dev->phy_iobase);
		pdesc->len = 0; /* to be filled in each operation */
		pdesc++;
	}
	pcmd->dst_dscr = (unsigned) pce_dev->phy_ce_in_dst_desc;
	pcmd->_reserved = LI_SG_CMD | SRC_INDEX_SG_CMD(0) |
						DST_INDEX_SG_CMD(0);
	pcmd++;

	/* setup command pointer list */
	*(pce_dev->cmd_pointer_list_ce_in) = (CMD_PTR_LP | DMOV_CMD_LIST |
			DMOV_CMD_ADDR((unsigned int)
					pce_dev->phy_cmd_list_ce_in));
	pce_dev->chan_ce_in_cmd->user = (void *) pce_dev;
	pce_dev->chan_ce_in_cmd->exec_func = NULL;
	pce_dev->chan_ce_in_cmd->cmdptr = DMOV_CMD_ADDR(
			(unsigned int) pce_dev->phy_cmd_pointer_list_ce_in);
	pce_dev->chan_ce_in_cmd->crci_mask = msm_dmov_build_crci_mask(1,
			pce_dev->crci_in);


	/*
	 * The first command in the command list ce_out.
	 * It is for encry/decryp output.
	 * If hashing only, ce_out is not used.
	 */
	pcmd = (dmov_sg *) pce_dev->cmd_list_ce_out;
	/* swap byte, half word, source crci, scatter gather */
	pcmd->cmd =   CMD_SRC_SWAP_BYTES | CMD_SRC_SWAP_SHORTS |
			CMD_SRC_CRCI(pce_dev->crci_out) | CMD_MODE_SG;

	pdesc = pce_dev->ce_out_src_desc;
	for (i = 0; i < QCE_MAX_NUM_DESC; i++) {
		pdesc->addr = (CRYPTO_DATA_SHADOW0 + pce_dev->phy_iobase);
		pdesc->len = 0;  /* to be filled in each operation */
		pdesc++;
	}
	pcmd->src_dscr = (unsigned) pce_dev->phy_ce_out_src_desc;

	pdesc = pce_dev->ce_out_dst_desc;
	pdesc->addr = 0;  /* to be filled in each operation */
	pdesc->len = 0;   /* to be filled in each operation */
	pcmd->dst_dscr = (unsigned) pce_dev->phy_ce_out_dst_desc;
	pcmd->_reserved = LI_SG_CMD | SRC_INDEX_SG_CMD(0) |
						DST_INDEX_SG_CMD(0);

	pcmd++;

	/* setup command pointer list */
	*(pce_dev->cmd_pointer_list_ce_out) = (CMD_PTR_LP | DMOV_CMD_LIST |
			DMOV_CMD_ADDR((unsigned int)pce_dev->
						phy_cmd_list_ce_out));

	pce_dev->chan_ce_out_cmd->user = pce_dev;
	pce_dev->chan_ce_out_cmd->exec_func = NULL;
	pce_dev->chan_ce_out_cmd->cmdptr = DMOV_CMD_ADDR(
			(unsigned int) pce_dev->phy_cmd_pointer_list_ce_out);
	pce_dev->chan_ce_out_cmd->crci_mask = msm_dmov_build_crci_mask(1,
			pce_dev->crci_out);

	return 0;
};

static int _qce_start_dma(struct qce_device *pce_dev, bool ce_in, bool ce_out)
{

	if (ce_in)
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IN_PROG;
	else
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_COMP;

	if (ce_out)
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IN_PROG;
	else
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_COMP;

	if (ce_in)
		msm_dmov_enqueue_cmd(pce_dev->chan_ce_in,
					pce_dev->chan_ce_in_cmd);
	if (ce_out)
		msm_dmov_enqueue_cmd(pce_dev->chan_ce_out,
					pce_dev->chan_ce_out_cmd);

	return 0;
};

int qce_aead_req(void *handle, struct qce_req *q_req)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	struct aead_request *areq = (struct aead_request *) q_req->areq;
	uint32_t authsize = q_req->authsize;
	uint32_t totallen_in, totallen_out, out_len;
	uint32_t pad_len_in, pad_len_out;
	uint32_t pad_mac_len_out, pad_ptx_len_out;
	int rc = 0;

	if (q_req->dir == QCE_ENCRYPT) {
		q_req->cryptlen = areq->cryptlen;
		totallen_in = q_req->cryptlen + areq->assoclen;
		totallen_out = q_req->cryptlen + authsize + areq->assoclen;
		out_len = areq->cryptlen + authsize;
		pad_len_in = ALIGN(totallen_in, ADM_CE_BLOCK_SIZE) -
								totallen_in;
		pad_mac_len_out = ALIGN(authsize, ADM_CE_BLOCK_SIZE) -
								authsize;
		pad_ptx_len_out = ALIGN(q_req->cryptlen, ADM_CE_BLOCK_SIZE) -
							q_req->cryptlen;
		pad_len_out = pad_ptx_len_out + pad_mac_len_out;
		totallen_out += pad_len_out;
	} else {
		q_req->cryptlen = areq->cryptlen - authsize;
		totallen_in = areq->cryptlen + areq->assoclen;
		totallen_out = q_req->cryptlen + areq->assoclen;
		out_len = areq->cryptlen - authsize;
		pad_len_in = ALIGN(areq->cryptlen, ADM_CE_BLOCK_SIZE) -
							areq->cryptlen;
		pad_len_out = pad_len_in + authsize;
		totallen_out += pad_len_out;
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
	dma_map_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
					 DMA_TO_DEVICE);
	if (_chain_sg_buffer_in(pce_dev, areq->assoc, areq->assoclen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}
	/* cipher input */
	pce_dev->src_nents = count_sg(areq->src, areq->cryptlen);
	dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	if (_chain_sg_buffer_in(pce_dev, areq->src, areq->cryptlen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}
	/* pad data in */
	if (pad_len_in) {
		if (_chain_pm_buffer_in(pce_dev, pce_dev->phy_ce_pad,
						pad_len_in) < 0) {
			rc = -ENOMEM;
			goto bad;
		}
	}

	/* ignore associated data */
	if (_chain_pm_buffer_out(pce_dev, pce_dev->phy_ce_out_ignore,
				areq->assoclen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}
	/* cipher + mac output  for encryption    */
	if (areq->src != areq->dst) {
		pce_dev->dst_nents = count_sg(areq->dst, out_len);
		dma_map_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
	};
	if (_chain_sg_buffer_out(pce_dev, areq->dst, out_len) < 0) {
		rc = -ENOMEM;
		goto bad;
	}
	/* pad data out */
	if (pad_len_out) {
		if (_chain_pm_buffer_out(pce_dev, pce_dev->phy_ce_pad,
						pad_len_out) < 0) {
			rc = -ENOMEM;
			goto bad;
		}
	}

	/* finalize the ce_in and ce_out channels command lists */
	_ce_in_final(pce_dev, ALIGN(totallen_in, ADM_CE_BLOCK_SIZE));
	_ce_out_final(pce_dev, ALIGN(totallen_out, ADM_CE_BLOCK_SIZE));

	/* set up crypto device */
	rc = _ce_setup_cipher(pce_dev, q_req, totallen_in, areq->assoclen);
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to adm */
	pce_dev->areq = q_req->areq;
	pce_dev->qce_cb = q_req->qce_cb;

	pce_dev->chan_ce_in_cmd->complete_func = _aead_ce_in_call_back;
	pce_dev->chan_ce_out_cmd->complete_func = _aead_ce_out_call_back;

	_ce_in_dump(pce_dev);
	_ce_out_dump(pce_dev);

	rc = _qce_start_dma(pce_dev, true, true);
	if (rc == 0)
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
	if (pce_dev->dst_nents) {
		dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
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

	uint32_t pad_len = ALIGN(areq->nbytes, ADM_CE_BLOCK_SIZE)
						- areq->nbytes;

	_chain_buffer_in_init(pce_dev);
	_chain_buffer_out_init(pce_dev);

	pce_dev->src_nents = 0;
	pce_dev->dst_nents = 0;

	/* cipher input */
	pce_dev->src_nents = count_sg(areq->src, areq->nbytes);

	if (c_req->use_pmem != 1)
		dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
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
			dma_map_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
							DMA_FROM_DEVICE);
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
		if (_chain_pm_buffer_in(pce_dev, pce_dev->phy_ce_pad,
						pad_len) < 0) {
			rc = -ENOMEM;
			goto bad;
		}
		if (_chain_pm_buffer_out(pce_dev, pce_dev->phy_ce_pad,
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
		pce_dev->chan_ce_in_cmd->complete_func =
					_ablk_cipher_ce_in_call_back_pmem;
		pce_dev->chan_ce_out_cmd->complete_func =
					_ablk_cipher_ce_out_call_back_pmem;
	} else {
		pce_dev->chan_ce_in_cmd->complete_func =
					_ablk_cipher_ce_in_call_back;
		pce_dev->chan_ce_out_cmd->complete_func =
					_ablk_cipher_ce_out_call_back;
	}
	rc = _qce_start_dma(pce_dev, true, true);

	if (rc == 0)
		return 0;
bad:
	if (c_req->use_pmem != 1) {
			if (pce_dev->dst_nents) {
				dma_unmap_sg(pce_dev->pdev, areq->dst,
				pce_dev->dst_nents, DMA_FROM_DEVICE);
			}
		if (pce_dev->src_nents) {
			dma_unmap_sg(pce_dev->pdev, areq->src,
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
	uint32_t pad_len = ALIGN(sreq->size, ADM_CE_BLOCK_SIZE) - sreq->size;
	struct ahash_request *areq = (struct ahash_request *)sreq->areq;

	_chain_buffer_in_init(pce_dev);
	pce_dev->src_nents = count_sg(sreq->src, sreq->size);
	dma_map_sg(pce_dev->pdev, sreq->src, pce_dev->src_nents,
							DMA_TO_DEVICE);

	if (_chain_sg_buffer_in(pce_dev, sreq->src, sreq->size) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	if (pad_len) {
		if (_chain_pm_buffer_in(pce_dev, pce_dev->phy_ce_pad,
						pad_len) < 0) {
				rc = -ENOMEM;
				goto bad;
			}
	}
	 _ce_in_final(pce_dev, sreq->size + pad_len);

	_ce_in_dump(pce_dev);

	rc =  _ce_setup_hash(pce_dev, sreq);

	if (rc < 0)
		goto bad;

	pce_dev->areq = areq;
	pce_dev->qce_cb = sreq->qce_cb;
	pce_dev->chan_ce_in_cmd->complete_func = _sha_ce_in_call_back;

	rc =  _qce_start_dma(pce_dev, true, false);

	if (rc == 0)
		return 0;
bad:
	if (pce_dev->src_nents) {
		dma_unmap_sg(pce_dev->pdev, sreq->src,
				pce_dev->src_nents, DMA_TO_DEVICE);
	}

	return rc;
}
EXPORT_SYMBOL(qce_process_sha_req);

/* crypto engine open function. */
void *qce_open(struct platform_device *pdev, int *rc)
{
	struct qce_device *pce_dev;
	struct resource *resource;
	struct clk *ce_core_clk;
	struct clk *ce_clk;

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

	pce_dev->chan_ce_in_cmd = kzalloc(sizeof(struct msm_dmov_cmd),
			GFP_KERNEL);
	pce_dev->chan_ce_out_cmd = kzalloc(sizeof(struct msm_dmov_cmd),
			GFP_KERNEL);
	if (pce_dev->chan_ce_in_cmd == NULL ||
			pce_dev->chan_ce_out_cmd == NULL) {
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
	pce_dev->chan_ce_in = resource->start;
	pce_dev->chan_ce_out = resource->end;
	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
				"crypto_crci_in");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA crci in resource\n");
		goto err_dm_chan_cmd;
	};
	pce_dev->crci_in = resource->start;
	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
				"crypto_crci_out");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA crci out resource\n");
		goto err_dm_chan_cmd;
	};
	pce_dev->crci_out = resource->start;

	pce_dev->coh_vmem = dma_alloc_coherent(pce_dev->pdev,
			2*PAGE_SIZE, &pce_dev->coh_pmem, GFP_KERNEL);

	if (pce_dev->coh_vmem == NULL) {
		*rc = -ENOMEM;
		dev_err(pce_dev->pdev, "Can not allocate coherent memory.\n");
		goto err;
	}

	/* Get CE core clk */
	ce_core_clk = clk_get(pce_dev->pdev, "ce_clk");
	if (IS_ERR(ce_core_clk)) {
		*rc = PTR_ERR(ce_core_clk);
		goto err;
	}
	pce_dev->ce_core_clk = ce_core_clk;
	/* Get CE clk */
	ce_clk = clk_get(pce_dev->pdev, "ce_pclk");
	if (IS_ERR(ce_clk)) {
		*rc = PTR_ERR(ce_clk);
		clk_put(pce_dev->ce_core_clk);
		goto err;
	}
	pce_dev->ce_clk = ce_clk;

	/* Enable CE core clk */
	*rc = clk_enable(pce_dev->ce_core_clk);
	if (*rc) {
		clk_put(pce_dev->ce_core_clk);
		clk_put(pce_dev->ce_clk);
		goto err;
	} else {
		/* Enable CE clk */
		*rc = clk_enable(pce_dev->ce_clk);
		if (*rc) {
			clk_disable(pce_dev->ce_core_clk);
			clk_put(pce_dev->ce_core_clk);
			clk_put(pce_dev->ce_clk);
			goto err;

		}
	}
	_setup_cmd_template(pce_dev);

	pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
	pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

	if (_init_ce_engine(pce_dev)) {
		*rc = -ENXIO;
		goto err;
	}
	*rc = 0;
	return pce_dev;

err:
	if (pce_dev->coh_vmem)
		dma_free_coherent(pce_dev->pdev, PAGE_SIZE, pce_dev->coh_vmem,
						pce_dev->coh_pmem);
err_dm_chan_cmd:
	kfree(pce_dev->chan_ce_in_cmd);
	kfree(pce_dev->chan_ce_out_cmd);
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
		dma_free_coherent(pce_dev->pdev, 2*PAGE_SIZE, pce_dev->coh_vmem,
						pce_dev->coh_pmem);
	clk_disable(pce_dev->ce_clk);
	clk_disable(pce_dev->ce_core_clk);

	clk_put(pce_dev->ce_clk);
	clk_put(pce_dev->ce_core_clk);

	kfree(pce_dev->chan_ce_in_cmd);
	kfree(pce_dev->chan_ce_out_cmd);
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
	return 0;
}
EXPORT_SYMBOL(qce_hw_support);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Mona Hossain <mhossain@codeaurora.org>");
MODULE_DESCRIPTION("Crypto Engine driver");
MODULE_VERSION("2.04");
