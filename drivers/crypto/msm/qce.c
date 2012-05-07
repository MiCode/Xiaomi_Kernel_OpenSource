/* Qualcomm Crypto Engine driver.
 *
 * Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <linux/qcedev.h>
#include <linux/qcota.h>
#include <mach/dma.h>

#include "qce.h"
#include "qcryptohw_30.h"
#include "qce_ota.h"

/* ADM definitions */
#define LI_SG_CMD  (1 << 31)    /* last index in the scatter gather cmd */
#define SRC_INDEX_SG_CMD(index) ((index & 0x3fff) << 16)
#define DST_INDEX_SG_CMD(index) (index & 0x3fff)
#define ADM_DESC_LAST  (1 << 31)

/* Data xfer between DM and CE in blocks of 16 bytes */
#define ADM_CE_BLOCK_SIZE  16

#define QCE_FIFO_SIZE  0x8000

/* Data xfer between DM and CE in blocks of 64 bytes */
#define ADM_SHA_BLOCK_SIZE  64

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
	struct clk *ce_clk;	    /* Handle to CE clk */
	unsigned int crci_in;	      /* CRCI for CE DM IN Channel   */
	unsigned int crci_out;	      /* CRCI for CE DM OUT Channel   */
	unsigned int crci_hash;	      /* CRCI for CE HASH   */
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
	uint32_t aeskey[AES256_KEY_SIZE/sizeof(uint32_t)];

	uint32_t aes_key_size;		/* cached aes key size in bytes */
	int fastaes;			/* ce supports fast aes */
	int hmac;			/* ce support hmac-sha1 */
	bool ota;			/* ce support ota */

	qce_comp_func_ptr_t qce_cb;	/* qce callback function pointer */

	int assoc_nents;
	int src_nents;
	int dst_nents;

	void *areq;
	enum qce_cipher_mode_enum mode;

	dma_addr_t phy_iv_in;
	dma_addr_t phy_ota_src;
	dma_addr_t phy_ota_dst;
	unsigned int ota_size;
	int err;
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

/* Source: FIPS 197, Figure 7. S-box: substitution values for the byte xy */
static const uint32_t _s_box[256] = {
	0x63, 0x7c, 0x77, 0x7b,   0xf2, 0x6b, 0x6f, 0xc5,
	0x30, 0x01, 0x67, 0x2b,   0xfe, 0xd7, 0xab, 0x76,

	0xca, 0x82, 0xc9, 0x7d,   0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf,   0x9c, 0xa4, 0x72, 0xc0,

	0xb7, 0xfd, 0x93, 0x26,   0x36, 0x3f, 0xf7, 0xcc,
	0x34, 0xa5, 0xe5, 0xf1,   0x71, 0xd8, 0x31, 0x15,

	0x04, 0xc7, 0x23, 0xc3,   0x18, 0x96, 0x05, 0x9a,
	0x07, 0x12, 0x80, 0xe2,   0xeb, 0x27, 0xb2, 0x75,

	0x09, 0x83, 0x2c, 0x1a,   0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3,   0x29, 0xe3, 0x2f, 0x84,

	0x53, 0xd1, 0x00, 0xed,   0x20, 0xfc, 0xb1, 0x5b,
	0x6a, 0xcb, 0xbe, 0x39,   0x4a, 0x4c, 0x58, 0xcf,

	0xd0, 0xef, 0xaa, 0xfb,   0x43, 0x4d, 0x33, 0x85,
	0x45, 0xf9, 0x02, 0x7f,   0x50, 0x3c, 0x9f, 0xa8,

	0x51, 0xa3, 0x40, 0x8f,   0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21,   0x10, 0xff, 0xf3, 0xd2,

	0xcd, 0x0c, 0x13, 0xec,   0x5f, 0x97, 0x44, 0x17,
	0xc4, 0xa7, 0x7e, 0x3d,   0x64, 0x5d, 0x19, 0x73,

	0x60, 0x81, 0x4f, 0xdc,   0x22, 0x2a, 0x90, 0x88,
	0x46, 0xee, 0xb8, 0x14,   0xde, 0x5e, 0x0b, 0xdb,

	0xe0, 0x32, 0x3a, 0x0a,   0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62,   0x91, 0x95, 0xe4, 0x79,

	0xe7, 0xc8, 0x37, 0x6d,   0x8d, 0xd5, 0x4e, 0xa9,
	0x6c, 0x56, 0xf4, 0xea,   0x65, 0x7a, 0xae, 0x08,

	0xba, 0x78, 0x25, 0x2e,   0x1c, 0xa6, 0xb4, 0xc6,
	0xe8, 0xdd, 0x74, 0x1f,   0x4b, 0xbd, 0x8b, 0x8a,

	0x70, 0x3e, 0xb5, 0x66,   0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9,   0x86, 0xc1, 0x1d, 0x9e,

	0xe1, 0xf8, 0x98, 0x11,   0x69, 0xd9, 0x8e, 0x94,
	0x9b, 0x1e, 0x87, 0xe9,   0xce, 0x55, 0x28, 0xdf,

	0x8c, 0xa1, 0x89, 0x0d,   0xbf, 0xe6, 0x42, 0x68,
	0x41, 0x99, 0x2d, 0x0f,   0xb0, 0x54, 0xbb, 0x16 };


/*
 *	Source:	FIPS 197, Sec 5.2 Key Expansion, Figure 11. Pseudo Code for Key
 *		Expansion.
 */
static void _aes_expand_key_schedule(uint32_t keysize, uint32_t *AES_KEY,
		uint32_t *AES_RND_KEY)
{
	uint32_t i;
	uint32_t Nk;
	uint32_t Nr, rot_data;
	uint32_t Rcon = 0x01000000;
	uint32_t temp;
	uint32_t data_in;
	uint32_t MSB_store;
	uint32_t byte_for_sub;
	uint32_t word_sub[4];

	switch (keysize) {
	case 192:
		Nk = 6;
		Nr = 12;
		break;

	case 256:
		Nk = 8;
		Nr = 14;
		break;

	case 128:
	default:  /* default to AES128 */
		Nk = 4;
		Nr = 10;
		break;
	}

	/* key expansion */
	i = 0;
	while (i < Nk) {
		AES_RND_KEY[i] = AES_KEY[i];
		i = i + 1;
	}

	i = Nk;
	while (i < (4 * (Nr + 1))) {
		temp = AES_RND_KEY[i-1];
		if (Nr == 14) {
			switch (i) {
			case 8:
				Rcon = 0x01000000;
				break;

			case 16:
				Rcon = 0x02000000;
				break;

			case 24:
				Rcon = 0x04000000;
				break;

			case 32:
				Rcon = 0x08000000;
				break;

			case 40:
				Rcon = 0x10000000;
				break;

			case 48:
				Rcon = 0x20000000;
				break;

			case 56:
				Rcon = 0x40000000;
				break;
			}
		} else if (Nr == 12) {
			switch (i) {
			case  6:
				Rcon = 0x01000000;
				break;

			case 12:
				Rcon = 0x02000000;
				break;

			case 18:
				Rcon = 0x04000000;
				break;

			case 24:
				Rcon = 0x08000000;
				break;

			case 30:
				Rcon = 0x10000000;
				break;

			case 36:
				Rcon = 0x20000000;
				break;

			case 42:
				Rcon = 0x40000000;
				break;

			case 48:
				Rcon = 0x80000000;
				break;
			}
		} else if (Nr == 10) {
			switch (i) {
			case 4:
				Rcon = 0x01000000;
				break;

			case 8:
				Rcon = 0x02000000;
				break;

			case 12:
				Rcon = 0x04000000;
				break;

			case 16:
				Rcon = 0x08000000;
				break;

			case 20:
				Rcon = 0x10000000;
				break;

			case 24:
				Rcon = 0x20000000;
				break;

			case 28:
				Rcon = 0x40000000;
				break;

			case 32:
				Rcon = 0x80000000;
				break;

			case 36:
				Rcon = 0x1b000000;
				break;

			case 40:
				Rcon = 0x36000000;
				break;
			}
		}

		if ((i % Nk) == 0) {
			data_in   = temp;
			MSB_store = (data_in >> 24 & 0xff);
			rot_data  = (data_in << 8) | MSB_store;
			byte_for_sub = rot_data;
			word_sub[0] = _s_box[(byte_for_sub & 0xff)];
			word_sub[1] = (_s_box[((byte_for_sub & 0xff00) >> 8)]
								<< 8);
			word_sub[2] = (_s_box[((byte_for_sub & 0xff0000) >> 16)]
								<< 16);
			word_sub[3] = (_s_box[((byte_for_sub & 0xff000000)
								>> 24)] << 24);
			word_sub[0] =  word_sub[0] | word_sub[1] | word_sub[2] |
							word_sub[3];
			temp = word_sub[0] ^ Rcon;
		} else if ((Nk > 6) && ((i % Nk) == 4)) {
			byte_for_sub = temp;
			word_sub[0] = _s_box[(byte_for_sub & 0xff)];
			word_sub[1] = (_s_box[((byte_for_sub & 0xff00) >> 8)]
								<< 8);
			word_sub[2] = (_s_box[((byte_for_sub & 0xff0000) >> 16)]
								<< 16);
			word_sub[3] = (_s_box[((byte_for_sub & 0xff000000) >>
								 24)] << 24);
			word_sub[0] =  word_sub[0] | word_sub[1] | word_sub[2] |
						word_sub[3];
			temp = word_sub[0];
		}

		AES_RND_KEY[i] = AES_RND_KEY[i-Nk]^temp;
		i = i+1;
	}
}

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
	int i = 0;
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
	unsigned int eng_availability;	/* engine available functions    */

	val = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if ((val & 0xfffffff) != 0x0200004) {
		dev_err(pce_dev->pdev,
				"unknown Qualcomm crypto device at 0x%x 0x%x\n",
				pce_dev->phy_iobase, val);
		return -EIO;
	};
	rev = (val & CRYPTO_CORE_REV_MASK) >> CRYPTO_CORE_REV;
	if (rev == 0x2) {
		dev_info(pce_dev->pdev,
				"Qualcomm Crypto 3e device found at 0x%x\n",
				pce_dev->phy_iobase);
	} else if (rev == 0x1) {
		dev_info(pce_dev->pdev,
				"Qualcomm Crypto 3 device found at 0x%x\n",
				pce_dev->phy_iobase);
	} else if (rev == 0x0) {
		dev_info(pce_dev->pdev,
				"Qualcomm Crypto 2 device found at 0x%x\n",
				pce_dev->phy_iobase);
	} else {
		dev_err(pce_dev->pdev,
				"unknown Qualcomm crypto device at 0x%x\n",
				pce_dev->phy_iobase);
		return -EIO;
	}

	eng_availability = readl_relaxed(pce_dev->iobase +
						CRYPTO_ENGINES_AVAIL);

	if (((eng_availability & CRYPTO_AES_SEL_MASK) >> CRYPTO_AES_SEL)
			== CRYPTO_AES_SEL_FAST)
		pce_dev->fastaes = 1;
	else
		pce_dev->fastaes = 0;

	if (eng_availability & (1 << CRYPTO_HMAC_SEL))
		pce_dev->hmac = 1;
	else
		pce_dev->hmac = 0;

	if ((eng_availability & (1 << CRYPTO_F9_SEL)) &&
			(eng_availability & (1 << CRYPTO_F8_SEL)))
		pce_dev->ota = true;
	else
		pce_dev->ota = false;

	pce_dev->aes_key_size = 0;

	return 0;
};

static int _init_ce_engine(struct qce_device *pce_dev)
{
	unsigned int val;

	/* reset qce */
	writel_relaxed(1 << CRYPTO_SW_RST, pce_dev->iobase + CRYPTO_CONFIG_REG);

	/* Ensure previous instruction (write to reset bit)
	 * was completed.
	 */
	mb();
	/* configure ce */
	val = (1 << CRYPTO_MASK_DOUT_INTR) | (1 << CRYPTO_MASK_DIN_INTR) |
			(1 << CRYPTO_MASK_AUTH_DONE_INTR) |
					(1 << CRYPTO_MASK_ERR_INTR);
	writel_relaxed(val, pce_dev->iobase + CRYPTO_CONFIG_REG);

	if (_probe_ce_engine(pce_dev) < 0)
		return -EIO;
	if (readl_relaxed(pce_dev->iobase + CRYPTO_CONFIG_REG) != val) {
		dev_err(pce_dev->pdev,
				"unknown Qualcomm crypto device at 0x%x\n",
				pce_dev->phy_iobase);
		return -EIO;
	};
	return 0;
};

static int _sha_ce_setup(struct qce_device *pce_dev, struct qce_sha_req *sreq)
{
	uint32_t auth32[SHA256_DIGEST_SIZE / sizeof(uint32_t)];
	uint32_t diglen;
	int rc;
	int i;
	uint32_t cfg = 0;

	/* if not the last, the size has to be on the block boundary */
	if (sreq->last_blk == 0 && (sreq->size % SHA256_BLOCK_SIZE))
		return -EIO;

	switch (sreq->alg) {
	case QCE_HASH_SHA1:
		diglen = SHA1_DIGEST_SIZE;
		break;
	case QCE_HASH_SHA256:
		diglen = SHA256_DIGEST_SIZE;
		break;
	default:
		return -EINVAL;
	}
	/*
	 * write 20/32 bytes, 5/8 words into auth_iv
	 *  for SHA1/SHA256
	 */

	if (sreq->first_blk) {
		if (sreq->alg == QCE_HASH_SHA1) {
			for (i = 0; i < 5; i++)
				auth32[i] = _std_init_vector_sha1[i];
		} else {
			for (i = 0; i < 8; i++)
				auth32[i] = _std_init_vector_sha256[i];
		}
	} else
		_byte_stream_to_net_words(auth32, sreq->digest, diglen);

	rc = clk_enable(pce_dev->ce_clk);
	if (rc)
		return rc;

	writel_relaxed(auth32[0], pce_dev->iobase + CRYPTO_AUTH_IV0_REG);
	writel_relaxed(auth32[1], pce_dev->iobase + CRYPTO_AUTH_IV1_REG);
	writel_relaxed(auth32[2], pce_dev->iobase + CRYPTO_AUTH_IV2_REG);
	writel_relaxed(auth32[3], pce_dev->iobase + CRYPTO_AUTH_IV3_REG);
	writel_relaxed(auth32[4], pce_dev->iobase + CRYPTO_AUTH_IV4_REG);

	if (sreq->alg == QCE_HASH_SHA256) {
		writel_relaxed(auth32[5], pce_dev->iobase +
							CRYPTO_AUTH_IV5_REG);
		writel_relaxed(auth32[6], pce_dev->iobase +
							CRYPTO_AUTH_IV6_REG);
		writel_relaxed(auth32[7], pce_dev->iobase +
							CRYPTO_AUTH_IV7_REG);
	}
	/* write auth_bytecnt 0/1, start with 0 */
	writel_relaxed(sreq->auth_data[0], pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT0_REG);
	writel_relaxed(sreq->auth_data[1], pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT1_REG);

	/* write auth_seg_cfg */
	writel_relaxed(sreq->size << CRYPTO_AUTH_SEG_SIZE,
			pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);

	/*
	 * write seg_cfg
	 */

	if (sreq->alg == QCE_HASH_SHA1)
		cfg |= (CRYPTO_AUTH_SIZE_SHA1 << CRYPTO_AUTH_SIZE);
	else
		cfg = (CRYPTO_AUTH_SIZE_SHA256 << CRYPTO_AUTH_SIZE);

	if (sreq->first_blk)
		cfg |= 1 << CRYPTO_FIRST;
	if (sreq->last_blk)
		cfg |= 1 << CRYPTO_LAST;
	cfg |= CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG;
	writel_relaxed(cfg, pce_dev->iobase + CRYPTO_SEG_CFG_REG);

	/* write seg_size   */
	writel_relaxed(sreq->size, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);

	/* issue go to crypto   */
	writel_relaxed(1 << CRYPTO_GO, pce_dev->iobase + CRYPTO_GOPROC_REG);
	/* Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	mb();

	return 0;
}

static int _ce_setup(struct qce_device *pce_dev, struct qce_req *q_req,
		uint32_t totallen, uint32_t coffset)
{
	uint32_t hmackey[HMAC_KEY_SIZE/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0};
	uint32_t enckey32[MAX_CIPHER_KEY_SIZE/sizeof(uint32_t)] = {
			0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t enciv32[MAX_IV_LENGTH / sizeof(uint32_t)] = {
			0, 0, 0, 0};
	uint32_t enck_size_in_word = q_req->encklen / sizeof(uint32_t);
	int aes_key_chg;
	int i, rc;
	uint32_t aes_round_key[CRYPTO_AES_RNDKEYS];
	uint32_t cfg;
	uint32_t ivsize = q_req->ivsize;

	rc = clk_enable(pce_dev->ce_clk);
	if (rc)
		return rc;

	cfg = (1 << CRYPTO_FIRST) | (1 << CRYPTO_LAST);
	if (q_req->op == QCE_REQ_AEAD) {

		/* do authentication setup */

		cfg |= (CRYPTO_AUTH_SIZE_HMAC_SHA1 << CRYPTO_AUTH_SIZE)|
				(CRYPTO_AUTH_ALG_SHA << CRYPTO_AUTH_ALG);

		/* write sha1 init vector */
		writel_relaxed(_std_init_vector_sha1[0],
				pce_dev->iobase + CRYPTO_AUTH_IV0_REG);
		writel_relaxed(_std_init_vector_sha1[1],
				pce_dev->iobase + CRYPTO_AUTH_IV1_REG);
		writel_relaxed(_std_init_vector_sha1[2],
				pce_dev->iobase + CRYPTO_AUTH_IV2_REG);
		writel_relaxed(_std_init_vector_sha1[3],
				pce_dev->iobase + CRYPTO_AUTH_IV3_REG);
		writel_relaxed(_std_init_vector_sha1[4],
				pce_dev->iobase + CRYPTO_AUTH_IV4_REG);
		/* write hmac key */
		_byte_stream_to_net_words(hmackey, q_req->authkey,
						q_req->authklen);
		writel_relaxed(hmackey[0], pce_dev->iobase +
							CRYPTO_AUTH_IV5_REG);
		writel_relaxed(hmackey[1], pce_dev->iobase +
							CRYPTO_AUTH_IV6_REG);
		writel_relaxed(hmackey[2], pce_dev->iobase +
							CRYPTO_AUTH_IV7_REG);
		writel_relaxed(hmackey[3], pce_dev->iobase +
							CRYPTO_AUTH_IV8_REG);
		writel_relaxed(hmackey[4], pce_dev->iobase +
							CRYPTO_AUTH_IV9_REG);
		writel_relaxed(0, pce_dev->iobase + CRYPTO_AUTH_BYTECNT0_REG);
		writel_relaxed(0, pce_dev->iobase + CRYPTO_AUTH_BYTECNT1_REG);

		/* write auth_seg_cfg */
		writel_relaxed((totallen << CRYPTO_AUTH_SEG_SIZE) & 0xffff0000,
				pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);

	}

	_byte_stream_to_net_words(enckey32, q_req->enckey, q_req->encklen);

	switch (q_req->mode) {
	case QCE_MODE_ECB:
		cfg |= (CRYPTO_ENCR_MODE_ECB << CRYPTO_ENCR_MODE);
		break;

	case QCE_MODE_CBC:
		cfg |= (CRYPTO_ENCR_MODE_CBC << CRYPTO_ENCR_MODE);
		break;

	case QCE_MODE_CTR:
	default:
		cfg |= (CRYPTO_ENCR_MODE_CTR << CRYPTO_ENCR_MODE);
		break;
	}
	pce_dev->mode = q_req->mode;

	switch (q_req->alg) {
	case CIPHER_ALG_DES:
		if (q_req->mode !=  QCE_MODE_ECB) {
			_byte_stream_to_net_words(enciv32, q_req->iv, ivsize);
			writel_relaxed(enciv32[0], pce_dev->iobase +
						CRYPTO_CNTR0_IV0_REG);
			writel_relaxed(enciv32[1], pce_dev->iobase +
						CRYPTO_CNTR1_IV1_REG);
		}
		writel_relaxed(enckey32[0], pce_dev->iobase +
							CRYPTO_DES_KEY0_REG);
		writel_relaxed(enckey32[1], pce_dev->iobase +
							CRYPTO_DES_KEY1_REG);
		cfg |= ((CRYPTO_ENCR_KEY_SZ_DES << CRYPTO_ENCR_KEY_SZ)  |
				(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG));
		break;

	case CIPHER_ALG_3DES:
		if (q_req->mode !=  QCE_MODE_ECB) {
			_byte_stream_to_net_words(enciv32, q_req->iv, ivsize);
			writel_relaxed(enciv32[0], pce_dev->iobase +
						CRYPTO_CNTR0_IV0_REG);
			writel_relaxed(enciv32[1], pce_dev->iobase +
						CRYPTO_CNTR1_IV1_REG);
		}
		writel_relaxed(enckey32[0], pce_dev->iobase +
							CRYPTO_DES_KEY0_REG);
		writel_relaxed(enckey32[1], pce_dev->iobase +
							CRYPTO_DES_KEY1_REG);
		writel_relaxed(enckey32[2], pce_dev->iobase +
							CRYPTO_DES_KEY2_REG);
		writel_relaxed(enckey32[3], pce_dev->iobase +
							CRYPTO_DES_KEY3_REG);
		writel_relaxed(enckey32[4], pce_dev->iobase +
							CRYPTO_DES_KEY4_REG);
		writel_relaxed(enckey32[5], pce_dev->iobase +
							CRYPTO_DES_KEY5_REG);
		cfg |= ((CRYPTO_ENCR_KEY_SZ_3DES << CRYPTO_ENCR_KEY_SZ)  |
				(CRYPTO_ENCR_ALG_DES << CRYPTO_ENCR_ALG));
		break;

	case CIPHER_ALG_AES:
	default:
		if (q_req->mode !=  QCE_MODE_ECB) {
			_byte_stream_to_net_words(enciv32, q_req->iv, ivsize);
			writel_relaxed(enciv32[0], pce_dev->iobase +
						CRYPTO_CNTR0_IV0_REG);
			writel_relaxed(enciv32[1], pce_dev->iobase +
						CRYPTO_CNTR1_IV1_REG);
			writel_relaxed(enciv32[2], pce_dev->iobase +
						CRYPTO_CNTR2_IV2_REG);
			writel_relaxed(enciv32[3], pce_dev->iobase +
						CRYPTO_CNTR3_IV3_REG);
		}
		/* set number of counter bits */
		writel_relaxed(0xffff, pce_dev->iobase + CRYPTO_CNTR_MASK_REG);

		if (q_req->op == QCE_REQ_ABLK_CIPHER_NO_KEY) {
				cfg |= (CRYPTO_ENCR_KEY_SZ_AES128 <<
						CRYPTO_ENCR_KEY_SZ);
			cfg |= CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG;
		} else {
			switch (q_req->encklen) {
			case AES128_KEY_SIZE:
				cfg |= (CRYPTO_ENCR_KEY_SZ_AES128 <<
							CRYPTO_ENCR_KEY_SZ);
				break;
			case AES192_KEY_SIZE:
				cfg |= (CRYPTO_ENCR_KEY_SZ_AES192 <<
							CRYPTO_ENCR_KEY_SZ);
				break;
			case AES256_KEY_SIZE:
			default:
				cfg |= (CRYPTO_ENCR_KEY_SZ_AES256 <<
							CRYPTO_ENCR_KEY_SZ);

				/* check for null key. If null, use hw key*/
				for (i = 0; i < enck_size_in_word; i++) {
					if (enckey32[i] != 0)
						break;
				}
				if (i == enck_size_in_word)
					cfg |= 1 << CRYPTO_USE_HW_KEY;
				break;
			} /* end of switch (q_req->encklen) */

			cfg |= CRYPTO_ENCR_ALG_AES << CRYPTO_ENCR_ALG;
			if (pce_dev->aes_key_size !=  q_req->encklen)
				aes_key_chg = 1;
			else {
				for (i = 0; i < enck_size_in_word; i++) {
					if (enckey32[i] != pce_dev->aeskey[i])
						break;
				}
				aes_key_chg = (i == enck_size_in_word) ? 0 : 1;
			}

			if (aes_key_chg) {
				if (pce_dev->fastaes) {
					for (i = 0; i < enck_size_in_word;
									i++) {
						writel_relaxed(enckey32[i],
							pce_dev->iobase +
							CRYPTO_AES_RNDKEY0 +
							(i * sizeof(uint32_t)));
					}
				} else {
					/* size in bit */
					_aes_expand_key_schedule(
						q_req->encklen * 8,
						enckey32, aes_round_key);

					for (i = 0; i < CRYPTO_AES_RNDKEYS;
									i++) {
						writel_relaxed(aes_round_key[i],
							pce_dev->iobase +
							CRYPTO_AES_RNDKEY0 +
							(i * sizeof(uint32_t)));
					}
				}

				pce_dev->aes_key_size = q_req->encklen;
				for (i = 0; i < enck_size_in_word; i++)
					pce_dev->aeskey[i] = enckey32[i];
			} /*if (aes_key_chg) { */
		} /* else of if (q_req->op == QCE_REQ_ABLK_CIPHER_NO_KEY) */
		break;
	} /* end of switch (q_req->mode)  */

	if (q_req->dir == QCE_ENCRYPT)
		cfg |= (1 << CRYPTO_AUTH_POS);
	cfg |= ((q_req->dir == QCE_ENCRYPT) ? 1 : 0) << CRYPTO_ENCODE;

	/* write encr seg cfg */
	writel_relaxed((q_req->cryptlen << CRYPTO_ENCR_SEG_SIZE) |
			(coffset & 0xffff),      /* cipher offset */
			pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);

	/* write seg cfg and size */
	writel_relaxed(cfg, pce_dev->iobase + CRYPTO_SEG_CFG_REG);
	writel_relaxed(totallen, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);

	/* issue go to crypto   */
	writel_relaxed(1 << CRYPTO_GO, pce_dev->iobase + CRYPTO_GOPROC_REG);
	/* Ensure previous instructions (setting the GO register)
	 * was completed before issuing a DMA transfer request
	 */
	mb();
	return 0;
};

static int _aead_complete(struct qce_device *pce_dev)
{
	struct aead_request *areq;
	struct crypto_aead *aead;
	uint32_t ivsize;
	uint32_t iv_out[4];
	unsigned char iv[4 * sizeof(uint32_t)];
	uint32_t status;

	areq = (struct aead_request *) pce_dev->areq;
	aead = crypto_aead_reqtfm(areq);
	ivsize = crypto_aead_ivsize(aead);

	if (areq->src != areq->dst) {
		dma_unmap_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
					DMA_FROM_DEVICE);
	}
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	dma_unmap_single(pce_dev->pdev, pce_dev->phy_iv_in,
			ivsize, DMA_TO_DEVICE);
	dma_unmap_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
			DMA_TO_DEVICE);

	/* check ce error status */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if (status & (1 << CRYPTO_SW_ERR)) {
		pce_dev->err++;
		dev_err(pce_dev->pdev,
			"Qualcomm Crypto Error at 0x%x, status%x\n",
			pce_dev->phy_iobase, status);
		_init_ce_engine(pce_dev);
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, pce_dev->dig_result, NULL, -ENXIO);
		return 0;
	};

	/* get iv out */
	if (pce_dev->mode == QCE_MODE_ECB) {
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, pce_dev->dig_result, NULL,
				pce_dev->chan_ce_in_status |
				pce_dev->chan_ce_out_status);
	} else {

		iv_out[0] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR0_IV0_REG);
		iv_out[1] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR1_IV1_REG);
		iv_out[2] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR2_IV2_REG);
		iv_out[3] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR3_IV3_REG);

		_net_words_to_byte_stream(iv_out, iv, sizeof(iv));
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, pce_dev->dig_result, iv,
				pce_dev->chan_ce_in_status |
				pce_dev->chan_ce_out_status);
	};
	return 0;
};

static void _sha_complete(struct qce_device *pce_dev)
{

	struct ahash_request *areq;
	uint32_t auth_data[2];
	uint32_t status;

	areq = (struct ahash_request *) pce_dev->areq;
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
				DMA_TO_DEVICE);

	/* check ce error status */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if (status & (1 << CRYPTO_SW_ERR)) {
		pce_dev->err++;
		dev_err(pce_dev->pdev,
			"Qualcomm Crypto Error at 0x%x, status%x\n",
			pce_dev->phy_iobase, status);
		_init_ce_engine(pce_dev);
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, pce_dev->dig_result, NULL, -ENXIO);
		return;
	};

	auth_data[0] = readl_relaxed(pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT0_REG);
	auth_data[1] = readl_relaxed(pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT1_REG);
	/* Ensure previous instruction (retriving byte count information)
	 * was completed before disabling the clk.
	 */
	mb();
	clk_disable(pce_dev->ce_clk);
	pce_dev->qce_cb(areq,  pce_dev->dig_result, (unsigned char *)auth_data,
				pce_dev->chan_ce_in_status);
};

static int _ablk_cipher_complete(struct qce_device *pce_dev)
{
	struct ablkcipher_request *areq;
	uint32_t iv_out[4];
	unsigned char iv[4 * sizeof(uint32_t)];
	uint32_t status;

	areq = (struct ablkcipher_request *) pce_dev->areq;

	if (areq->src != areq->dst) {
		dma_unmap_sg(pce_dev->pdev, areq->dst,
			pce_dev->dst_nents, DMA_FROM_DEVICE);
	}
	dma_unmap_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
		(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
						DMA_TO_DEVICE);

	/* check ce error status */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if (status & (1 << CRYPTO_SW_ERR)) {
		pce_dev->err++;
		dev_err(pce_dev->pdev,
			"Qualcomm Crypto Error at 0x%x, status%x\n",
			pce_dev->phy_iobase, status);
		_init_ce_engine(pce_dev);
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, NULL, NULL, -ENXIO);
		return 0;
	};

	/* get iv out */
	if (pce_dev->mode == QCE_MODE_ECB) {
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, NULL, NULL, pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
	} else {
		iv_out[0] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR0_IV0_REG);
		iv_out[1] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR1_IV1_REG);
		iv_out[2] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR2_IV2_REG);
		iv_out[3] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR3_IV3_REG);

		_net_words_to_byte_stream(iv_out, iv, sizeof(iv));
		clk_disable(pce_dev->ce_clk);
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
	uint32_t status;

	areq = (struct ablkcipher_request *) pce_dev->areq;

	/* check ce error status */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if (status & (1 << CRYPTO_SW_ERR)) {
		pce_dev->err++;
		dev_err(pce_dev->pdev,
			"Qualcomm Crypto Error at 0x%x, status%x\n",
			pce_dev->phy_iobase, status);
		_init_ce_engine(pce_dev);
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, NULL, NULL, -ENXIO);
		return 0;
	};

	/* get iv out */
	if (pce_dev->mode == QCE_MODE_ECB) {
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, NULL, NULL, pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
	} else {
		iv_out[0] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR0_IV0_REG);
		iv_out[1] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR1_IV1_REG);
		iv_out[2] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR2_IV2_REG);
		iv_out[3] = readl_relaxed(pce_dev->iobase +
							CRYPTO_CNTR3_IV3_REG);

		_net_words_to_byte_stream(iv_out, iv, sizeof(iv));
		clk_disable(pce_dev->ce_clk);
		pce_dev->qce_cb(areq, NULL, iv, pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
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
			if (pdesc->len > QCE_FIFO_SIZE) {
				if (qce_split_and_insert_dm_desc(pdesc,
						pdesc->len, sg_dma_address(sg),
						&pce_dev->ce_in_src_desc_index))
					return -EIO;
			}
		} else if (sg_dma_address(sg) == (pdesc->addr + dlen)) {
			pdesc->len  = dlen + len;
			if (pdesc->len > QCE_FIFO_SIZE) {
				if (qce_split_and_insert_dm_desc(pdesc,
						pdesc->len, pdesc->addr,
						&pce_dev->ce_in_src_desc_index))
					return -EIO;
			}
		} else {
			pce_dev->ce_in_src_desc_index++;
			if (pce_dev->ce_in_src_desc_index >= QCE_MAX_NUM_DESC)
				return -ENOMEM;
			pdesc++;
			pdesc->len = len;
			pdesc->addr = sg_dma_address(sg);
			if (pdesc->len > QCE_FIFO_SIZE) {
				if (qce_split_and_insert_dm_desc(pdesc,
						pdesc->len, sg_dma_address(sg),
						&pce_dev->ce_in_src_desc_index))
					return -EIO;
			}
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

static void _ce_in_final(struct qce_device *pce_dev, int ncmd, unsigned total)
{
	struct dmov_desc *pdesc;
	dmov_sg *pcmd;

	pdesc = pce_dev->ce_in_src_desc + pce_dev->ce_in_src_desc_index;
	pdesc->len |= ADM_DESC_LAST;

	pdesc = pce_dev->ce_in_dst_desc;
	if (total > QCE_FIFO_SIZE) {
		qce_split_and_insert_dm_desc(pdesc, total, 0,
				&pce_dev->ce_in_dst_desc_index);
		pdesc = pce_dev->ce_in_dst_desc + pce_dev->ce_in_dst_desc_index;
		pdesc->len |= ADM_DESC_LAST;
	} else
		pdesc->len = ADM_DESC_LAST | total;

	pcmd = (dmov_sg *) pce_dev->cmd_list_ce_in;
	if (ncmd == 1)
		pcmd->cmd |= CMD_LC;
	else {
		dmov_s  *pscmd;

		pcmd->cmd &= ~CMD_LC;
		pcmd++;
		pscmd = (dmov_s *)pcmd;
		pscmd->cmd |= CMD_LC;
	}

#ifdef QCE_DEBUG
	dev_info(pce_dev->pdev, "_ce_in_final %d\n",
					pce_dev->ce_in_src_desc_index);
#endif
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
#endif

static int _chain_sg_buffer_out(struct qce_device *pce_dev,
		struct scatterlist *sg, unsigned int nbytes)
{
	unsigned int len;
	unsigned int dlen;
	struct dmov_desc *pdesc;

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
			if (pdesc->len > QCE_FIFO_SIZE) {
				if (qce_split_and_insert_dm_desc(pdesc,
					pdesc->len, sg_dma_address(sg),
					&pce_dev->ce_out_dst_desc_index))
					return -EIO;
			}
		} else if (sg_dma_address(sg) == (pdesc->addr + dlen)) {
			pdesc->len  = dlen + len;
			if (pdesc->len > QCE_FIFO_SIZE) {
				if (qce_split_and_insert_dm_desc(pdesc,
					pdesc->len, pdesc->addr,
					&pce_dev->ce_out_dst_desc_index))
					return -EIO;
			}
		} else {
			pce_dev->ce_out_dst_desc_index++;
			if (pce_dev->ce_out_dst_desc_index >= QCE_MAX_NUM_DESC)
				return -EIO;
			pdesc++;
			pdesc->len = len;
			pdesc->addr = sg_dma_address(sg);
			if (pdesc->len > QCE_FIFO_SIZE) {
				if (qce_split_and_insert_dm_desc(pdesc,
					pdesc->len, sg_dma_address(sg),
					&pce_dev->ce_out_dst_desc_index))
					return -EIO;
			}
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

static void _ce_out_final(struct qce_device *pce_dev, int ncmd, unsigned total)
{
	struct dmov_desc *pdesc;
	dmov_sg *pcmd;

	pdesc = pce_dev->ce_out_dst_desc + pce_dev->ce_out_dst_desc_index;
	pdesc->len |= ADM_DESC_LAST;

	pdesc = pce_dev->ce_out_src_desc;
	if (total > QCE_FIFO_SIZE) {
		qce_split_and_insert_dm_desc(pdesc, total, 0,
				&pce_dev->ce_out_src_desc_index);
		pdesc = pce_dev->ce_out_src_desc +
					pce_dev->ce_out_src_desc_index;
		pdesc->len |= ADM_DESC_LAST;
	} else
		pdesc->len = ADM_DESC_LAST | total;

	pcmd = (dmov_sg *) pce_dev->cmd_list_ce_out;
	if (ncmd == 1)
		pcmd->cmd |= CMD_LC;
	else {
		dmov_s  *pscmd;

		pcmd->cmd &= ~CMD_LC;
		pcmd++;
		pscmd = (dmov_s *)pcmd;
		pscmd->cmd |= CMD_LC;
	}
#ifdef QCE_DEBUG
	dev_info(pce_dev->pdev, "_ce_out_final %d\n",
			pce_dev->ce_out_dst_desc_index);
#endif

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
	} else
		pce_dev->chan_ce_in_status = 0;

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
	} else
		pce_dev->chan_ce_in_status = 0;
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
	} else
		pce_dev->chan_ce_in_status = 0;

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
	} else
		pce_dev->chan_ce_in_status = 0;

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
	dmov_s  *pscmd;
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

	/*
	 * 3. ce_in channel command list of one scatter gather command
	 *    and one simple command.
	 */
	pce_dev->cmd_list_ce_in = vaddr;
	pce_dev->phy_cmd_list_ce_in = pce_dev->coh_pmem
			 + (vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + sizeof(dmov_s) + sizeof(dmov_sg);

	/* 4. authentication result. */
	pce_dev->dig_result = vaddr;
	pce_dev->phy_dig_result = pce_dev->coh_pmem +
			(vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + SHA256_DIGESTSIZE;

	/*
	 * 5. ce_out channel command list of one scatter gather command
	 *    and one simple command.
	 */
	pce_dev->cmd_list_ce_out = vaddr;
	pce_dev->phy_cmd_list_ce_out = pce_dev->coh_pmem
			 + (vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + sizeof(dmov_s) + sizeof(dmov_sg);

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
	vaddr = vaddr + ADM_CE_BLOCK_SIZE;

	/* 9. ce_in channel command pointer list.	 */
	vaddr = (unsigned char *) ALIGN(((unsigned int) vaddr), 16);
	pce_dev->cmd_pointer_list_ce_in = (unsigned int *) vaddr;
	pce_dev->phy_cmd_pointer_list_ce_in = pce_dev->coh_pmem +
			(vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + sizeof(unsigned char *);

	/* 10. ce_ou channel command pointer list. */
	vaddr = (unsigned char *) ALIGN(((unsigned int) vaddr), 16);
	pce_dev->cmd_pointer_list_ce_out = (unsigned int *) vaddr;
	pce_dev->phy_cmd_pointer_list_ce_out =  pce_dev->coh_pmem +
			(vaddr - pce_dev->coh_vmem);
	vaddr = vaddr + sizeof(unsigned char *);

	/* 11. throw away area to store by-pass data from ce_out. */
	pce_dev->ce_out_ignore = (unsigned char *) vaddr;
	pce_dev->phy_ce_out_ignore  = pce_dev->coh_pmem
			+ (vaddr - pce_dev->coh_vmem);
	pce_dev->ce_out_ignore_size = (2 * PAGE_SIZE) - (vaddr -
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
	/*
	 * The second command is for the digested data of
	 * hashing operation only. For others, this command is not used.
	 */
	pscmd = (dmov_s *) pcmd;
	/* last command, swap byte, half word, src crci, single   */
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES | CMD_SRC_SWAP_SHORTS |
			CMD_SRC_CRCI(pce_dev->crci_hash) | CMD_MODE_SINGLE;
	pscmd->src = (unsigned) (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = SHA256_DIGESTSIZE;	/* to be filled.  */
	pscmd->dst = (unsigned) pce_dev->phy_dig_result;
	/* setup command pointer list */
	*(pce_dev->cmd_pointer_list_ce_in) = (CMD_PTR_LP | DMOV_CMD_LIST |
			DMOV_CMD_ADDR((unsigned int)
					pce_dev->phy_cmd_list_ce_in));
	pce_dev->chan_ce_in_cmd->user = (void *) pce_dev;
	pce_dev->chan_ce_in_cmd->exec_func = NULL;
	pce_dev->chan_ce_in_cmd->cmdptr = DMOV_CMD_ADDR(
			(unsigned int) pce_dev->phy_cmd_pointer_list_ce_in);
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
	/*
	 * The second command is for digested data of esp operation.
	 * For ciphering, this command is not used.
	 */
	pscmd = (dmov_s *) pcmd;
	/* last command, swap byte, half word, src crci, single   */
	pscmd->cmd = CMD_LC | CMD_SRC_SWAP_BYTES | CMD_SRC_SWAP_SHORTS |
			CMD_SRC_CRCI(pce_dev->crci_hash) | CMD_MODE_SINGLE;
	pscmd->src = (CRYPTO_AUTH_IV0_REG + pce_dev->phy_iobase);
	pscmd->len = SHA1_DIGESTSIZE;     /* we only support hmac(sha1) */
	pscmd->dst = (unsigned) pce_dev->phy_dig_result;
	/* setup command pointer list */
	*(pce_dev->cmd_pointer_list_ce_out) = (CMD_PTR_LP | DMOV_CMD_LIST |
			DMOV_CMD_ADDR((unsigned int)pce_dev->
						phy_cmd_list_ce_out));

	pce_dev->chan_ce_out_cmd->user = pce_dev;
	pce_dev->chan_ce_out_cmd->exec_func = NULL;
	pce_dev->chan_ce_out_cmd->cmdptr = DMOV_CMD_ADDR(
			(unsigned int) pce_dev->phy_cmd_pointer_list_ce_out);


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

static void _f9_complete(struct qce_device *pce_dev)
{
	uint32_t mac_i;
	uint32_t status;

	dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_src,
				pce_dev->ota_size, DMA_TO_DEVICE);

	/* check ce error status */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if (status & (1 << CRYPTO_SW_ERR)) {
		pce_dev->err++;
		dev_err(pce_dev->pdev,
			"Qualcomm Crypto Error at 0x%x, status%x\n",
			pce_dev->phy_iobase, status);
		_init_ce_engine(pce_dev);
		pce_dev->qce_cb(pce_dev->areq, NULL, NULL, -ENXIO);
		return;
	};

	mac_i = readl_relaxed(pce_dev->iobase + CRYPTO_AUTH_IV0_REG);
	pce_dev->qce_cb(pce_dev->areq, (void *) mac_i, NULL,
				pce_dev->chan_ce_in_status);
};

static void _f8_complete(struct qce_device *pce_dev)
{
	uint32_t status;

	if (pce_dev->phy_ota_dst != 0)
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_dst,
				pce_dev->ota_size, DMA_FROM_DEVICE);
	if (pce_dev->phy_ota_src != 0)
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_src,
				pce_dev->ota_size, (pce_dev->phy_ota_dst) ?
				DMA_TO_DEVICE : DMA_BIDIRECTIONAL);

	/* check ce error status */
	status = readl_relaxed(pce_dev->iobase + CRYPTO_STATUS_REG);
	if (status & (1 << CRYPTO_SW_ERR)) {
		pce_dev->err++;
		dev_err(pce_dev->pdev,
			"Qualcomm Crypto Error at 0x%x, status%x\n",
			pce_dev->phy_iobase, status);
		_init_ce_engine(pce_dev);
		pce_dev->qce_cb(pce_dev->areq, NULL, NULL, -ENXIO);
		return;
	};

	pce_dev->qce_cb(pce_dev->areq, NULL, NULL,
				pce_dev->chan_ce_in_status |
					pce_dev->chan_ce_out_status);
};


static void _f9_ce_in_call_back(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;

	pce_dev = (struct qce_device *) cmd_ptr->user;
	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
						result);
		pce_dev->chan_ce_in_status = -1;
	} else
		pce_dev->chan_ce_in_status = 0;
	pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
	_f9_complete(pce_dev);
};

static void _f8_ce_in_call_back(struct msm_dmov_cmd *cmd_ptr,
		unsigned int result, struct msm_dmov_errdata *err)
{
	struct qce_device *pce_dev;

	pce_dev = (struct qce_device *) cmd_ptr->user;
	if (result != ADM_STATUS_OK) {
		dev_err(pce_dev->pdev, "Qualcomm ADM status error %x\n",
						 result);
		pce_dev->chan_ce_in_status = -1;
	} else
		pce_dev->chan_ce_in_status = 0;

	pce_dev->chan_ce_in_state = QCE_CHAN_STATE_COMP;
	if (pce_dev->chan_ce_out_state == QCE_CHAN_STATE_COMP) {
		pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
		pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

		/* done */
		_f8_complete(pce_dev);
	}
};

static void _f8_ce_out_call_back(struct msm_dmov_cmd *cmd_ptr,
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
		_f8_complete(pce_dev);
	}
};

static int _ce_f9_setup(struct qce_device *pce_dev, struct qce_f9_req * req)
{
	uint32_t cfg;
	uint32_t ikey[OTA_KEY_SIZE/sizeof(uint32_t)];

	_byte_stream_to_net_words(ikey, &req->ikey[0], OTA_KEY_SIZE);
	writel_relaxed(ikey[0], pce_dev->iobase + CRYPTO_AUTH_IV0_REG);
	writel_relaxed(ikey[1], pce_dev->iobase + CRYPTO_AUTH_IV1_REG);
	writel_relaxed(ikey[2], pce_dev->iobase + CRYPTO_AUTH_IV2_REG);
	writel_relaxed(ikey[3], pce_dev->iobase + CRYPTO_AUTH_IV3_REG);
	writel_relaxed(req->last_bits, pce_dev->iobase + CRYPTO_AUTH_IV4_REG);

	writel_relaxed(req->fresh, pce_dev->iobase + CRYPTO_AUTH_BYTECNT0_REG);
	writel_relaxed(req->count_i, pce_dev->iobase +
						CRYPTO_AUTH_BYTECNT1_REG);

	/* write auth_seg_cfg */
	writel_relaxed((uint32_t)req->msize << CRYPTO_AUTH_SEG_SIZE,
			pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);

	/* write seg_cfg */
	cfg = (CRYPTO_AUTH_ALG_F9 << CRYPTO_AUTH_ALG) | (1 << CRYPTO_FIRST) |
			(1 << CRYPTO_LAST);

	if (req->algorithm == QCE_OTA_ALGO_KASUMI)
		cfg |= (CRYPTO_AUTH_SIZE_UIA1 << CRYPTO_AUTH_SIZE);
	else
		cfg |= (CRYPTO_AUTH_SIZE_UIA2 << CRYPTO_AUTH_SIZE) ;

	if (req->direction == QCE_OTA_DIR_DOWNLINK)
		cfg |= 1 << CRYPTO_F9_DIRECTION;

	writel_relaxed(cfg, pce_dev->iobase + CRYPTO_SEG_CFG_REG);

	/* write seg_size   */
	writel_relaxed(req->msize, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);

	/* issue go to crypto   */
	writel_relaxed(1 << CRYPTO_GO, pce_dev->iobase + CRYPTO_GOPROC_REG);

	/*
	 * barrier to ensure previous instructions
	 * (including GO) to CE finish before issue DMA transfer
	 * request.
	 */
	mb();
	return 0;
};

static int _ce_f8_setup(struct qce_device *pce_dev, struct qce_f8_req *req,
		bool key_stream_mode, uint16_t npkts, uint16_t cipher_offset,
		uint16_t cipher_size)
{
	uint32_t cfg;
	uint32_t ckey[OTA_KEY_SIZE/sizeof(uint32_t)];

	if ((key_stream_mode && (req->data_len & 0xf || npkts > 1)) ||
				(req->bearer >= QCE_OTA_MAX_BEARER))
		return -EINVAL;

	/*  write seg_cfg */
	cfg = (CRYPTO_ENCR_ALG_F8 << CRYPTO_ENCR_ALG) | (1 << CRYPTO_FIRST) |
				(1 << CRYPTO_LAST);
	if (req->algorithm == QCE_OTA_ALGO_KASUMI)
		cfg |= (CRYPTO_ENCR_KEY_SZ_UEA1 << CRYPTO_ENCR_KEY_SZ);
	else
		cfg |= (CRYPTO_ENCR_KEY_SZ_UEA2 << CRYPTO_ENCR_KEY_SZ) ;
	if (key_stream_mode)
		cfg |= 1 << CRYPTO_F8_KEYSTREAM_ENABLE;
	if (req->direction == QCE_OTA_DIR_DOWNLINK)
		cfg |= 1 << CRYPTO_F8_DIRECTION;
	writel_relaxed(cfg, pce_dev->iobase + CRYPTO_SEG_CFG_REG);

	/* write seg_size   */
	writel_relaxed(req->data_len, pce_dev->iobase + CRYPTO_SEG_SIZE_REG);

	/* write 0 to auth_size, auth_offset */
	writel_relaxed(0, pce_dev->iobase + CRYPTO_AUTH_SEG_CFG_REG);

	/* write encr_seg_cfg seg_size, seg_offset */
	writel_relaxed((((uint32_t) cipher_size) << CRYPTO_ENCR_SEG_SIZE) |
			(cipher_offset & 0xffff),
				pce_dev->iobase + CRYPTO_ENCR_SEG_CFG_REG);

	/* write keys */
	_byte_stream_to_net_words(ckey, &req->ckey[0], OTA_KEY_SIZE);
	writel_relaxed(ckey[0], pce_dev->iobase + CRYPTO_DES_KEY0_REG);
	writel_relaxed(ckey[1], pce_dev->iobase + CRYPTO_DES_KEY1_REG);
	writel_relaxed(ckey[2], pce_dev->iobase + CRYPTO_DES_KEY2_REG);
	writel_relaxed(ckey[3], pce_dev->iobase + CRYPTO_DES_KEY3_REG);

	/* write cntr0_iv0 for countC */
	writel_relaxed(req->count_c, pce_dev->iobase + CRYPTO_CNTR0_IV0_REG);

	/* write cntr1_iv1 for nPkts, and bearer */
	if (npkts == 1)
		npkts = 0;
	writel_relaxed(req->bearer << CRYPTO_CNTR1_IV1_REG_F8_BEARER |
			npkts << CRYPTO_CNTR1_IV1_REG_F8_PKT_CNT,
				pce_dev->iobase + CRYPTO_CNTR1_IV1_REG);

	/* issue go to crypto   */
	writel_relaxed(1 << CRYPTO_GO, pce_dev->iobase + CRYPTO_GOPROC_REG);

	/*
	 * barrier to ensure previous instructions
	 * (including GO) to CE finish before issue DMA transfer
	 * request.
	 */
	mb();
	return 0;
};

int qce_aead_req(void *handle, struct qce_req *q_req)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	struct aead_request *areq = (struct aead_request *) q_req->areq;
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	uint32_t ivsize = crypto_aead_ivsize(aead);
	uint32_t totallen;
	uint32_t pad_len;
	uint32_t authsize = crypto_aead_authsize(aead);
	int rc = 0;

	q_req->ivsize = ivsize;
	if (q_req->dir == QCE_ENCRYPT)
		q_req->cryptlen = areq->cryptlen;
	else
		q_req->cryptlen = areq->cryptlen - authsize;

	totallen = q_req->cryptlen + ivsize + areq->assoclen;
	pad_len = ALIGN(totallen, ADM_CE_BLOCK_SIZE) - totallen;

	_chain_buffer_in_init(pce_dev);
	_chain_buffer_out_init(pce_dev);

	pce_dev->assoc_nents = 0;
	pce_dev->phy_iv_in = 0;
	pce_dev->src_nents = 0;
	pce_dev->dst_nents = 0;

	pce_dev->assoc_nents = count_sg(areq->assoc, areq->assoclen);
	dma_map_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
					 DMA_TO_DEVICE);
	if (_chain_sg_buffer_in(pce_dev, areq->assoc, areq->assoclen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	/* cipher iv for input                                 */
	pce_dev->phy_iv_in = dma_map_single(pce_dev->pdev, q_req->iv,
			ivsize, DMA_TO_DEVICE);
	if (_chain_pm_buffer_in(pce_dev, pce_dev->phy_iv_in, ivsize) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	/* for output, ignore associated data and cipher iv */
	if (_chain_pm_buffer_out(pce_dev, pce_dev->phy_ce_out_ignore,
						ivsize + areq->assoclen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	/* cipher input       */
	pce_dev->src_nents = count_sg(areq->src, q_req->cryptlen);
	dma_map_sg(pce_dev->pdev, areq->src, pce_dev->src_nents,
			(areq->src == areq->dst) ? DMA_BIDIRECTIONAL :
							DMA_TO_DEVICE);
	if (_chain_sg_buffer_in(pce_dev, areq->src, q_req->cryptlen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	/* cipher output      */
	if (areq->src != areq->dst) {
		pce_dev->dst_nents = count_sg(areq->dst, q_req->cryptlen);
		dma_map_sg(pce_dev->pdev, areq->dst, pce_dev->dst_nents,
				DMA_FROM_DEVICE);
	};
	if (_chain_sg_buffer_out(pce_dev, areq->dst, q_req->cryptlen) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	/* pad data      */
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
	_ce_in_final(pce_dev, 1, ALIGN(totallen, ADM_CE_BLOCK_SIZE));
	_ce_out_final(pce_dev, 2, ALIGN(totallen, ADM_CE_BLOCK_SIZE));

	/* set up crypto device */
	rc = _ce_setup(pce_dev, q_req, totallen, ivsize + areq->assoclen);
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to adm */
	pce_dev->areq = q_req->areq;
	pce_dev->qce_cb = q_req->qce_cb;

	pce_dev->chan_ce_in_cmd->complete_func = _aead_ce_in_call_back;
	pce_dev->chan_ce_out_cmd->complete_func = _aead_ce_out_call_back;

	rc = _qce_start_dma(pce_dev, true, true);
	if (rc == 0)
		return 0;
bad:
	if (pce_dev->assoc_nents) {
		dma_unmap_sg(pce_dev->pdev, areq->assoc, pce_dev->assoc_nents,
				DMA_TO_DEVICE);
	}
	if (pce_dev->phy_iv_in) {
		dma_unmap_single(pce_dev->pdev, pce_dev->phy_iv_in,
				ivsize, DMA_TO_DEVICE);
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
	/* cipher input       */
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

	/* cipher output      */
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

	/* pad data      */
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
	_ce_in_final(pce_dev, 1, areq->nbytes + pad_len);
	_ce_out_final(pce_dev, 1, areq->nbytes + pad_len);

#ifdef QCE_DEBUG
	_ce_in_dump(pce_dev);
	_ce_out_dump(pce_dev);
#endif
	/* set up crypto device */
	rc = _ce_setup(pce_dev, c_req, areq->nbytes, 0);
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
	 _ce_in_final(pce_dev, 2, sreq->size + pad_len);

#ifdef QCE_DEBUG
	_ce_in_dump(pce_dev);
#endif

	rc =  _sha_ce_setup(pce_dev, sreq);

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

/*
 * crypto engine open function.
 */
void *qce_open(struct platform_device *pdev, int *rc)
{
	struct qce_device *pce_dev;
	struct resource *resource;
	struct clk *ce_clk;

	pce_dev = kzalloc(sizeof(struct qce_device), GFP_KERNEL);
	if (!pce_dev) {
		*rc = -ENOMEM;
		dev_err(&pdev->dev, "Can not allocate memory\n");
		return NULL;
	}
	pce_dev->pdev = &pdev->dev;
	ce_clk = clk_get(pce_dev->pdev, "core_clk");
	if (IS_ERR(ce_clk)) {
		kfree(pce_dev);
		*rc = PTR_ERR(ce_clk);
		return NULL;
	}
	pce_dev->ce_clk = ce_clk;
	*rc = clk_enable(pce_dev->ce_clk);
	if (*rc) {
		kfree(pce_dev);
		return NULL;
	}

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing MEM resource\n");
		goto err;
	};
	pce_dev->phy_iobase = resource->start;
	pce_dev->iobase = ioremap_nocache(resource->start,
				resource->end - resource->start + 1);
	if (!pce_dev->iobase) {
		*rc = -ENOMEM;
		dev_err(pce_dev->pdev, "Can not map io memory\n");
		goto err;
	}

	pce_dev->chan_ce_in_cmd = kzalloc(sizeof(struct msm_dmov_cmd),
			GFP_KERNEL);
	pce_dev->chan_ce_out_cmd = kzalloc(sizeof(struct msm_dmov_cmd),
			GFP_KERNEL);
	if (pce_dev->chan_ce_in_cmd == NULL ||
			pce_dev->chan_ce_out_cmd == NULL) {
		dev_err(pce_dev->pdev, "Can not allocate memory\n");
		*rc = -ENOMEM;
		goto err;
	}

	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
					"crypto_channels");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA channel resource\n");
		goto err;
	};
	pce_dev->chan_ce_in = resource->start;
	pce_dev->chan_ce_out = resource->end;
	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
				"crypto_crci_in");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA crci in resource\n");
		goto err;
	};
	pce_dev->crci_in = resource->start;
	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
				"crypto_crci_out");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA crci out resource\n");
		goto err;
	};
	pce_dev->crci_out = resource->start;
	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
				"crypto_crci_hash");
	if (!resource) {
		*rc = -ENXIO;
		dev_err(pce_dev->pdev, "Missing DMA crci hash resource\n");
		goto err;
	};
	pce_dev->crci_hash = resource->start;
	pce_dev->coh_vmem = dma_alloc_coherent(pce_dev->pdev,
			2*PAGE_SIZE, &pce_dev->coh_pmem, GFP_KERNEL);

	if (pce_dev->coh_vmem == NULL) {
		*rc = -ENOMEM;
		dev_err(pce_dev->pdev, "Can not allocate coherent memory.\n");
		goto err;
	}
	_setup_cmd_template(pce_dev);

	pce_dev->chan_ce_in_state = QCE_CHAN_STATE_IDLE;
	pce_dev->chan_ce_out_state = QCE_CHAN_STATE_IDLE;

	if (_init_ce_engine(pce_dev)) {
		*rc = -ENXIO;
		clk_disable(pce_dev->ce_clk);
		goto err;
	}
	*rc = 0;
	clk_disable(pce_dev->ce_clk);

	pce_dev->err = 0;

	return pce_dev;
err:
	if (pce_dev)
		qce_close(pce_dev);
	return NULL;
}
EXPORT_SYMBOL(qce_open);

/*
 * crypto engine close function.
 */
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
	kfree(pce_dev->chan_ce_in_cmd);
	kfree(pce_dev->chan_ce_out_cmd);

	clk_put(pce_dev->ce_clk);
	kfree(handle);
	return 0;
}
EXPORT_SYMBOL(qce_close);

int qce_hw_support(void *handle, struct ce_hw_support *ce_support)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;

	if (ce_support == NULL)
		return -EINVAL;

	if (pce_dev->hmac == 1)
		ce_support->sha1_hmac_20 = true;
	else
		ce_support->sha1_hmac_20 = false;
	ce_support->sha1_hmac = false;
	ce_support->sha256_hmac = false;
	ce_support->sha_hmac = false;
	ce_support->cmac  = false;
	ce_support->aes_key_192 = true;
	ce_support->aes_xts  = false;
	ce_support->aes_ccm  = false;
	ce_support->ota = pce_dev->ota;
	ce_support->aligned_only = false;
	ce_support->bam = false;
	return 0;
}
EXPORT_SYMBOL(qce_hw_support);

int qce_f8_req(void *handle, struct qce_f8_req *req,
			void *cookie, qce_comp_func_ptr_t qce_cb)
{
	struct qce_device *pce_dev = (struct qce_device *) handle;
	bool key_stream_mode;
	dma_addr_t dst;
	int rc;
	uint32_t pad_len = ALIGN(req->data_len, ADM_CE_BLOCK_SIZE) -
						req->data_len;

	_chain_buffer_in_init(pce_dev);
	_chain_buffer_out_init(pce_dev);

	key_stream_mode = (req->data_in == NULL);

	/* F8 cipher input       */
	if (key_stream_mode)
		pce_dev->phy_ota_src = 0;
	else {
		pce_dev->phy_ota_src = dma_map_single(pce_dev->pdev,
					req->data_in, req->data_len,
					(req->data_in == req->data_out) ?
					DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
		if (_chain_pm_buffer_in(pce_dev, pce_dev->phy_ota_src,
				req->data_len) < 0) {
			pce_dev->phy_ota_dst = 0;
			rc =  -ENOMEM;
			goto bad;
		}
	}

	/* F8 cipher output     */
	if (req->data_in != req->data_out) {
		dst = dma_map_single(pce_dev->pdev, req->data_out,
				req->data_len, DMA_FROM_DEVICE);
		pce_dev->phy_ota_dst = dst;
	} else {
		dst = pce_dev->phy_ota_src;
		pce_dev->phy_ota_dst = 0;
	}
	if (_chain_pm_buffer_out(pce_dev, dst, req->data_len) < 0) {
		rc = -ENOMEM;
		goto bad;
	}

	pce_dev->ota_size = req->data_len;

	/* pad data      */
	if (pad_len) {
		if (!key_stream_mode && _chain_pm_buffer_in(pce_dev,
					pce_dev->phy_ce_pad, pad_len) < 0) {
			rc =  -ENOMEM;
			goto bad;
		}
		if (_chain_pm_buffer_out(pce_dev, pce_dev->phy_ce_pad,
						pad_len) < 0) {
			rc =  -ENOMEM;
			goto bad;
		}
	}

	/* finalize the ce_in and ce_out channels command lists */
	if (!key_stream_mode)
		_ce_in_final(pce_dev, 1, req->data_len + pad_len);
	_ce_out_final(pce_dev, 1, req->data_len + pad_len);

	/* set up crypto device */
	rc = _ce_f8_setup(pce_dev, req, key_stream_mode, 1, 0, req->data_len);
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to adm */
	pce_dev->areq = cookie;
	pce_dev->qce_cb = qce_cb;

	if (!key_stream_mode)
		pce_dev->chan_ce_in_cmd->complete_func = _f8_ce_in_call_back;

	pce_dev->chan_ce_out_cmd->complete_func = _f8_ce_out_call_back;

	rc =  _qce_start_dma(pce_dev, !(key_stream_mode), true);
	if (rc == 0)
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
	uint32_t pad_len;
	dma_addr_t dst = 0;
	int rc = 0;

	total = num_pkt *  req->data_len;
	pad_len = ALIGN(total, ADM_CE_BLOCK_SIZE) - total;

	_chain_buffer_in_init(pce_dev);
	_chain_buffer_out_init(pce_dev);

	/* F8 cipher input       */
	pce_dev->phy_ota_src = dma_map_single(pce_dev->pdev,
				req->data_in, total,
				(req->data_in == req->data_out) ?
				DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
	if (_chain_pm_buffer_in(pce_dev, pce_dev->phy_ota_src,
				total) < 0) {
		pce_dev->phy_ota_dst = 0;
		rc = -ENOMEM;
		goto bad;
	}
	/* F8 cipher output      */
	if (req->data_in != req->data_out) {
		dst = dma_map_single(pce_dev->pdev, req->data_out, total,
						DMA_FROM_DEVICE);
		pce_dev->phy_ota_dst = dst;
	} else {
		dst = pce_dev->phy_ota_src;
		pce_dev->phy_ota_dst = 0;
	}
	if (_chain_pm_buffer_out(pce_dev, dst, total) < 0) {
		rc = -ENOMEM;
		goto  bad;
	}

	pce_dev->ota_size = total;

	/* pad data      */
	if (pad_len) {
		if (_chain_pm_buffer_in(pce_dev, pce_dev->phy_ce_pad,
					pad_len) < 0) {
			rc = -ENOMEM;
			goto  bad;
		}
		if (_chain_pm_buffer_out(pce_dev, pce_dev->phy_ce_pad,
						pad_len) < 0) {
			rc = -ENOMEM;
			goto  bad;
		}
	}

	/* finalize the ce_in and ce_out channels command lists */
	_ce_in_final(pce_dev, 1, total + pad_len);
	_ce_out_final(pce_dev, 1, total + pad_len);


	/* set up crypto device */
	rc = _ce_f8_setup(pce_dev, req, false, num_pkt, cipher_start,
			cipher_size);
	if (rc)
		goto bad ;

	/* setup for callback, and issue command to adm */
	pce_dev->areq = cookie;
	pce_dev->qce_cb = qce_cb;

	pce_dev->chan_ce_in_cmd->complete_func = _f8_ce_in_call_back;
	pce_dev->chan_ce_out_cmd->complete_func = _f8_ce_out_call_back;

	rc = _qce_start_dma(pce_dev, true, true);
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
	uint32_t pad_len = ALIGN(req->msize, ADM_CE_BLOCK_SIZE) - req->msize;

	pce_dev->phy_ota_src = dma_map_single(pce_dev->pdev, req->message,
			req->msize, DMA_TO_DEVICE);

	_chain_buffer_in_init(pce_dev);
	rc = _chain_pm_buffer_in(pce_dev, pce_dev->phy_ota_src, req->msize);
	if (rc < 0) {
		rc =  -ENOMEM;
		goto bad;
	}

	pce_dev->ota_size = req->msize;
	if (pad_len) {
		rc = _chain_pm_buffer_in(pce_dev, pce_dev->phy_ce_pad,
				pad_len);
		if (rc < 0) {
			rc = -ENOMEM;
			goto bad;
		}
	}
	_ce_in_final(pce_dev, 2, req->msize + pad_len);
	rc = _ce_f9_setup(pce_dev, req);
	if (rc < 0)
		goto bad;

	/* setup for callback, and issue command to adm */
	pce_dev->areq = cookie;
	pce_dev->qce_cb = qce_cb;

	pce_dev->chan_ce_in_cmd->complete_func = _f9_ce_in_call_back;

	rc =  _qce_start_dma(pce_dev, true, false);
	if (rc == 0)
		return 0;
bad:
	dma_unmap_single(pce_dev->pdev, pce_dev->phy_ota_src,
				req->msize, DMA_TO_DEVICE);
	return rc;
}
EXPORT_SYMBOL(qce_f9_req);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto Engine driver");

