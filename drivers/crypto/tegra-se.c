/*
 * Cryptographic API.
 * drivers/crypto/tegra-se.c
 *
 * Support for Tegra Security Engine hardware crypto algorithms.
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/internal/rng.h>
#include <crypto/internal/hash.h>
#include <crypto/sha.h>
#include <linux/pm_runtime.h>
#include <mach/hardware.h>

#include "tegra-se.h"

#define DRIVER_NAME	"tegra-se"

/* Security Engine operation modes */
enum tegra_se_aes_op_mode {
	SE_AES_OP_MODE_CBC,	/* Cipher Block Chaining (CBC) mode */
	SE_AES_OP_MODE_ECB,	/* Electronic Codebook (ECB) mode */
	SE_AES_OP_MODE_CTR,	/* Counter (CTR) mode */
	SE_AES_OP_MODE_OFB,	/* Output feedback (CFB) mode */
	SE_AES_OP_MODE_RNG_X931,	/* Random number generator (RNG) mode */
	SE_AES_OP_MODE_RNG_DRBG,	/* Deterministic Random Bit Generator */
	SE_AES_OP_MODE_CMAC,	/* Cipher-based MAC (CMAC) mode */
	SE_AES_OP_MODE_SHA1,	/* Secure Hash Algorithm-1 (SHA1) mode */
	SE_AES_OP_MODE_SHA224,	/* Secure Hash Algorithm-224  (SHA224) mode */
	SE_AES_OP_MODE_SHA256,	/* Secure Hash Algorithm-256  (SHA256) mode */
	SE_AES_OP_MODE_SHA384,	/* Secure Hash Algorithm-384  (SHA384) mode */
	SE_AES_OP_MODE_SHA512	/* Secure Hash Algorithm-512  (SHA512) mode */
};

/* Security Engine key table type */
enum tegra_se_key_table_type {
	SE_KEY_TABLE_TYPE_KEY,	/* Key */
	SE_KEY_TABLE_TYPE_ORGIV,	/* Original IV */
	SE_KEY_TABLE_TYPE_UPDTDIV	/* Updated IV */
};

/* Security Engine request context */
struct tegra_se_req_context {
	enum tegra_se_aes_op_mode op_mode; /* Security Engine operation mode */
	bool encrypt;	/* Operation type */
};

struct tegra_se_chipdata {
	bool cprng_supported;
	bool drbg_supported;
	bool rsa_supported;
	unsigned long aes_freq;
	unsigned long rng_freq;
	unsigned long sha1_freq;
	unsigned long sha224_freq;
	unsigned long sha256_freq;
	unsigned long sha384_freq;
	unsigned long sha512_freq;
	unsigned long rsa_freq;
};

struct tegra_se_dev {
	struct device *dev;
	void __iomem *io_reg;	/* se device memory/io */
	void __iomem *pmc_io_reg;	/* pmc device memory/io */
	int irq;	/* irq allocated */
	spinlock_t lock;	/* spin lock */
	struct clk *pclk;	/* Security Engine clock */
	struct crypto_queue queue; /* Security Engine crypto queue */
	struct tegra_se_slot *slot_list;	/* pointer to key slots */
	struct tegra_se_rsa_slot *rsa_slot_list; /* rsa key slot pointer */
	u64 ctr;
	u32 *src_ll_buf;	/* pointer to source linked list buffer */
	dma_addr_t src_ll_buf_adr; /* Source linked list buffer dma address */
	u32 src_ll_size;	/* Size of source linked list buffer */
	u32 *dst_ll_buf;	/* pointer to destination linked list buffer */
	dma_addr_t dst_ll_buf_adr; /* Destination linked list dma address */
	u32 dst_ll_size;	/* Size of destination linked list buffer */
	u32 *ctx_save_buf;	/* LP context buffer pointer*/
	dma_addr_t ctx_save_buf_adr;	/* LP context buffer dma address*/
	struct completion complete;	/* Tells the task completion */
	bool work_q_busy;	/* Work queue busy status */
	struct tegra_se_chipdata *chipdata; /* chip specific data */
};

static struct tegra_se_dev *sg_tegra_se_dev;

/* Security Engine AES context */
struct tegra_se_aes_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	u32 keylen;	/* key length in bits */
	u32 op_mode;	/* AES operation mode */
};

/* Security Engine random number generator context */
struct tegra_se_rng_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	u32 *dt_buf;	/* Destination buffer pointer */
	dma_addr_t dt_buf_adr;	/* Destination buffer dma address */
	u32 *rng_buf;	/* RNG buffer pointer */
	dma_addr_t rng_buf_adr;	/* RNG buffer dma address */
	bool use_org_iv;	/* Tells whether original IV is be used
				or not. If it is false updated IV is used*/
};

/* Security Engine SHA context */
struct tegra_se_sha_context {
	struct tegra_se_dev	*se_dev;	/* Security Engine device */
	u32 op_mode;	/* SHA operation mode */
};

/* Security Engine AES CMAC context */
struct tegra_se_aes_cmac_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	u32 keylen;	/* key length in bits */
	u8 K1[TEGRA_SE_KEY_128_SIZE];	/* Key1 */
	u8 K2[TEGRA_SE_KEY_128_SIZE];	/* Key2 */
	dma_addr_t dma_addr;	/* DMA address of local buffer */
	u32 buflen;	/* local buffer length */
	u8	*buffer;	/* local buffer pointer */
};

/* Security Engine key slot */
struct tegra_se_slot {
	struct list_head node;
	u8 slot_num;	/* Key slot number */
	bool available; /* Tells whether key slot is free to use */
};

static struct tegra_se_slot ssk_slot = {
	.slot_num = 15,
	.available = false,
};

static struct tegra_se_slot srk_slot = {
	.slot_num = 0,
	.available = false,
};

/* Security Engine Linked List */
struct tegra_se_ll {
	dma_addr_t addr; /* DMA buffer address */
	u32 data_len; /* Data length in DMA buffer */
};

static LIST_HEAD(key_slot);
static LIST_HEAD(rsa_key_slot);
static DEFINE_SPINLOCK(rsa_key_slot_lock);

#define RSA_MIN_SIZE	64
#define RSA_MAX_SIZE	256
#define RNG_RESEED_INTERVAL	100
#define TEGRA_SE_RSA_CONTEXT_SAVE_KEYSLOT_COUNT	4

static DEFINE_SPINLOCK(key_slot_lock);
static DEFINE_MUTEX(se_hw_lock);

/* create a work for handling the async transfers */
static void tegra_se_work_handler(struct work_struct *work);
static DECLARE_WORK(se_work, tegra_se_work_handler);
static struct workqueue_struct *se_work_q;

#define PMC_SCRATCH43_REG_OFFSET 0x22c
#define GET_MSB(x)  ((x) >> (8*sizeof(x)-1))
static int force_reseed_count;
static void tegra_se_leftshift_onebit(u8 *in_buf, u32 size, u8 *org_msb)
{
	u8 carry;
	u32 i;

	*org_msb = GET_MSB(in_buf[0]);

	/* left shift one bit */
	in_buf[0] <<= 1;
	for (carry = 0, i = 1; i < size; i++) {
		carry = GET_MSB(in_buf[i]);
		in_buf[i-1] |= carry;
		in_buf[i] <<= 1;
	}
}

extern unsigned long long tegra_chip_uid(void);

static inline void se_writel(struct tegra_se_dev *se_dev,
	unsigned int val, unsigned int reg_offset)
{
	writel(val, se_dev->io_reg + reg_offset);
}

static inline unsigned int se_readl(struct tegra_se_dev *se_dev,
	unsigned int reg_offset)
{
	unsigned int val;

	val = readl(se_dev->io_reg + reg_offset);

	return val;
}

static void tegra_se_free_key_slot(struct tegra_se_slot *slot)
{
	if (slot) {
		spin_lock(&key_slot_lock);
		slot->available = true;
		spin_unlock(&key_slot_lock);
	}
}

static struct tegra_se_slot *tegra_se_alloc_key_slot(void)
{
	struct tegra_se_slot *slot = NULL;
	bool found = false;

	spin_lock(&key_slot_lock);
	list_for_each_entry(slot, &key_slot, node) {
		if (slot->available) {
			slot->available = false;
			found = true;
			break;
		}
	}
	spin_unlock(&key_slot_lock);
	return found ? slot : NULL;
}

static int tegra_init_key_slot(struct tegra_se_dev *se_dev)
{
	int i;

	se_dev->slot_list = kzalloc(sizeof(struct tegra_se_slot) *
					TEGRA_SE_KEYSLOT_COUNT, GFP_KERNEL);
	if (se_dev->slot_list == NULL) {
		dev_err(se_dev->dev, "slot list memory allocation failed\n");
		return -ENOMEM;
	}
	spin_lock_init(&key_slot_lock);
	spin_lock(&key_slot_lock);
	for (i = 0; i < TEGRA_SE_KEYSLOT_COUNT; i++) {
		/*
		 * Slot 0 and 15 are reserved and will not be added to the
		 * free slots pool. Slot 0 is used for SRK generation and
		 * Slot 15 is used for SSK operation
		 */
		if ((i == srk_slot.slot_num) || (i == ssk_slot.slot_num))
			continue;
		se_dev->slot_list[i].available = true;
		se_dev->slot_list[i].slot_num = i;
		INIT_LIST_HEAD(&se_dev->slot_list[i].node);
		list_add_tail(&se_dev->slot_list[i].node, &key_slot);
	}
	spin_unlock(&key_slot_lock);

	return 0;
}

static void tegra_se_key_read_disable(u8 slot_num)
{
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	u32 val;

	val = se_readl(se_dev,
			(SE_KEY_TABLE_ACCESS_REG_OFFSET + (slot_num * 4)));
	val &= ~(1 << SE_KEY_READ_DISABLE_SHIFT);
	se_writel(se_dev,
		val, (SE_KEY_TABLE_ACCESS_REG_OFFSET + (slot_num * 4)));
}

static void tegra_se_key_read_disable_all(void)
{
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	u8 slot_num;

	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	for (slot_num = 0; slot_num < TEGRA_SE_KEYSLOT_COUNT; slot_num++)
		tegra_se_key_read_disable(slot_num);

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);
}

static void tegra_se_config_algo(struct tegra_se_dev *se_dev,
	enum tegra_se_aes_op_mode mode, bool encrypt, u32 key_len)
{
	u32 val = 0;

	switch (mode) {
	case SE_AES_OP_MODE_CBC:
	case SE_AES_OP_MODE_CMAC:
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			if (key_len == TEGRA_SE_KEY_256_SIZE)
				val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
			else if (key_len == TEGRA_SE_KEY_192_SIZE)
				val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
			else
				val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
			val |= SE_CONFIG_DEC_ALG(ALG_NOP);
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (key_len == TEGRA_SE_KEY_256_SIZE)
				val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
			else if (key_len == TEGRA_SE_KEY_192_SIZE)
				val |= SE_CONFIG_DEC_MODE(MODE_KEY192);
			else
				val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
		}
		if (mode == SE_AES_OP_MODE_CMAC)
			val |= SE_CONFIG_DST(DST_HASHREG);
		else
			val |= SE_CONFIG_DST(DST_MEMORY);
		break;
	case SE_AES_OP_MODE_RNG_X931:
	case SE_AES_OP_MODE_RNG_DRBG:
		val = SE_CONFIG_ENC_ALG(ALG_RNG) |
			SE_CONFIG_ENC_MODE(MODE_KEY128) |
				SE_CONFIG_DST(DST_MEMORY);
		break;
	case SE_AES_OP_MODE_ECB:
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			if (key_len == TEGRA_SE_KEY_256_SIZE)
				val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
			else if (key_len == TEGRA_SE_KEY_192_SIZE)
				val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
			else
				val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (key_len == TEGRA_SE_KEY_256_SIZE)
				val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
			else if (key_len == TEGRA_SE_KEY_192_SIZE)
				val |= SE_CONFIG_DEC_MODE(MODE_KEY192);
			else
				val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
		}
		val |= SE_CONFIG_DST(DST_MEMORY);
		break;
	case SE_AES_OP_MODE_CTR:
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			if (key_len == TEGRA_SE_KEY_256_SIZE)
				val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
			else if (key_len == TEGRA_SE_KEY_192_SIZE)
				val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
			else
				val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (key_len == TEGRA_SE_KEY_256_SIZE) {
				val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
				val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
			} else if (key_len == TEGRA_SE_KEY_192_SIZE) {
				val |= SE_CONFIG_DEC_MODE(MODE_KEY192);
				val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
			} else {
				val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
				val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
			}
		}
		val |= SE_CONFIG_DST(DST_MEMORY);
		break;
	case SE_AES_OP_MODE_OFB:
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			if (key_len == TEGRA_SE_KEY_256_SIZE)
				val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
			else if (key_len == TEGRA_SE_KEY_192_SIZE)
				val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
			else
				val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (key_len == TEGRA_SE_KEY_256_SIZE) {
				val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
				val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
			} else if (key_len == TEGRA_SE_KEY_192_SIZE) {
				val |= SE_CONFIG_DEC_MODE(MODE_KEY192);
				val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
			} else {
				val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
				val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
			}
		}
		val |= SE_CONFIG_DST(DST_MEMORY);
		break;
	case SE_AES_OP_MODE_SHA1:
		val = SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA1) |
				SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA224:
		val = SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA224) |
				SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA256:
		val = SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA256) |
				SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA384:
		val = SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA384) |
				SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA512:
		val = SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA512) |
				SE_CONFIG_DST(DST_HASHREG);
		break;
	default:
		dev_warn(se_dev->dev, "Invalid operation mode\n");
		break;
	}

	se_writel(se_dev, val, SE_CONFIG_REG_OFFSET);
}

static void tegra_se_write_seed(struct tegra_se_dev *se_dev, u32 *pdata)
{
	u32 i;

	for (i = 0; i < SE_CRYPTO_CTR_REG_COUNT; i++)
		se_writel(se_dev, pdata[i], SE_CRYPTO_CTR_REG_OFFSET + (i * 4));
}

static void tegra_se_write_key_table(u8 *pdata, u32 data_len,
	u8 slot_num, enum tegra_se_key_table_type type)
{
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	u32 data_size = SE_KEYTABLE_REG_MAX_DATA;
	u32 *pdata_buf = (u32 *)pdata;
	u8 pkt = 0, quad = 0;
	u32 val = 0, i;

	if (pdata_buf == NULL)
		return;

	if ((type == SE_KEY_TABLE_TYPE_KEY) && (slot_num == ssk_slot.slot_num))
		return;

	if (type == SE_KEY_TABLE_TYPE_ORGIV)
		quad = QUAD_ORG_IV;
	else if (type == SE_KEY_TABLE_TYPE_UPDTDIV)
		quad = QUAD_UPDTD_IV;
	else
		quad = QUAD_KEYS_128;

	/* write data to the key table */
	do {
		for (i = 0; i < data_size; i += 4, data_len -= 4)
			se_writel(se_dev, *pdata_buf++,
				SE_KEYTABLE_DATA0_REG_OFFSET + i);

		pkt = SE_KEYTABLE_SLOT(slot_num) | SE_KEYTABLE_QUAD(quad);
		val = SE_KEYTABLE_OP_TYPE(OP_WRITE) |
			SE_KEYTABLE_TABLE_SEL(TABLE_KEYIV) |
				SE_KEYTABLE_PKT(pkt);

		se_writel(se_dev, val, SE_KEYTABLE_REG_OFFSET);

		data_size = data_len;
		quad = QUAD_KEYS_256;

	} while (data_len);
}

static void tegra_se_config_crypto(struct tegra_se_dev *se_dev,
	enum tegra_se_aes_op_mode mode, bool encrypt, u8 slot_num, bool org_iv)
{
	u32 val = 0;
	unsigned long freq = 0;
	int err = 0;

	switch (mode) {
	case SE_AES_OP_MODE_CMAC:
	case SE_AES_OP_MODE_CBC:
		if (encrypt) {
			val = SE_CRYPTO_INPUT_SEL(INPUT_AHB) |
				SE_CRYPTO_VCTRAM_SEL(VCTRAM_AESOUT) |
				SE_CRYPTO_XOR_POS(XOR_TOP) |
				SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		} else {
			val = SE_CRYPTO_INPUT_SEL(INPUT_AHB) |
				SE_CRYPTO_VCTRAM_SEL(VCTRAM_PREVAHB) |
				SE_CRYPTO_XOR_POS(XOR_BOTTOM) |
				SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
		}
		freq = se_dev->chipdata->aes_freq;
		break;
	case SE_AES_OP_MODE_RNG_X931:
		val = SE_CRYPTO_INPUT_SEL(INPUT_AHB) |
			SE_CRYPTO_XOR_POS(XOR_BYPASS) |
			SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		freq = se_dev->chipdata->rng_freq;
		break;
	case SE_AES_OP_MODE_RNG_DRBG:
		val = SE_CRYPTO_INPUT_SEL(INPUT_RANDOM) |
			SE_CRYPTO_XOR_POS(XOR_BYPASS) |
			SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA11))
			val = val | SE_CRYPTO_KEY_INDEX(slot_num);
		freq = se_dev->chipdata->rng_freq;
		break;
	case SE_AES_OP_MODE_ECB:
		if (encrypt) {
			val = SE_CRYPTO_INPUT_SEL(INPUT_AHB) |
				SE_CRYPTO_XOR_POS(XOR_BYPASS) |
				SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		} else {
			val = SE_CRYPTO_INPUT_SEL(INPUT_AHB) |
				SE_CRYPTO_XOR_POS(XOR_BYPASS) |
				SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
		}
		freq = se_dev->chipdata->aes_freq;
		break;
	case SE_AES_OP_MODE_CTR:
		val = SE_CRYPTO_INPUT_SEL(INPUT_LNR_CTR) |
			SE_CRYPTO_VCTRAM_SEL(VCTRAM_AHB) |
			SE_CRYPTO_XOR_POS(XOR_BOTTOM) |
			SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		freq = se_dev->chipdata->aes_freq;
		break;
	case SE_AES_OP_MODE_OFB:
		val = SE_CRYPTO_INPUT_SEL(INPUT_AESOUT) |
			SE_CRYPTO_VCTRAM_SEL(VCTRAM_AHB) |
			SE_CRYPTO_XOR_POS(XOR_BOTTOM) |
			SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		freq = se_dev->chipdata->aes_freq;
		break;
	default:
		dev_warn(se_dev->dev, "Invalid operation mode\n");
		break;
	}

	if (mode == SE_AES_OP_MODE_CTR) {
		val |= SE_CRYPTO_HASH(HASH_DISABLE) |
			SE_CRYPTO_KEY_INDEX(slot_num) |
			SE_CRYPTO_CTR_CNTN(1);
	} else {
		val |= SE_CRYPTO_HASH(HASH_DISABLE) |
			SE_CRYPTO_KEY_INDEX(slot_num) |
			(org_iv ? SE_CRYPTO_IV_SEL(IV_ORIGINAL) :
			SE_CRYPTO_IV_SEL(IV_UPDATED));
	}

	err = clk_set_rate(se_dev->pclk, freq);
	if (err) {
		dev_err(se_dev->dev, "clock set_rate failed.\n");
		return;
	}

	/* enable hash for CMAC */
	if (mode == SE_AES_OP_MODE_CMAC)
		val |= SE_CRYPTO_HASH(HASH_ENABLE);

	se_writel(se_dev, val, SE_CRYPTO_REG_OFFSET);

	if (mode == SE_AES_OP_MODE_RNG_DRBG) {
		if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA11) &&
						force_reseed_count <= 0) {
			se_writel(se_dev,
				SE_RNG_CONFIG_MODE(DRBG_MODE_FORCE_RESEED)|
				SE_RNG_CONFIG_SRC(DRBG_SRC_LFSR),
				SE_RNG_CONFIG_REG_OFFSET);
		force_reseed_count = RNG_RESEED_INTERVAL;
		} else {
			se_writel(se_dev,
				SE_RNG_CONFIG_MODE(DRBG_MODE_NORMAL)|
				SE_RNG_CONFIG_SRC(DRBG_SRC_LFSR),
				SE_RNG_CONFIG_REG_OFFSET);
		}
		if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA11))
			--force_reseed_count;

		se_writel(se_dev, RNG_RESEED_INTERVAL,
			SE_RNG_RESEED_INTERVAL_REG_OFFSET);
	}

	if (mode == SE_AES_OP_MODE_CTR)
		se_writel(se_dev, 1, SE_SPARE_0_REG_OFFSET);

	if (mode == SE_AES_OP_MODE_OFB)
		se_writel(se_dev, 1, SE_SPARE_0_REG_OFFSET);

}

static void tegra_se_config_sha(struct tegra_se_dev *se_dev, u32 count,
	unsigned long freq)
{
	int i;
	int err = 0;

	se_writel(se_dev, (count * 8), SE_SHA_MSG_LENGTH_REG_OFFSET);
	se_writel(se_dev, (count * 8), SE_SHA_MSG_LEFT_REG_OFFSET);
	for (i = 1; i < 4; i++) {
		se_writel(se_dev, 0, SE_SHA_MSG_LENGTH_REG_OFFSET + (4 * i));
		se_writel(se_dev, 0, SE_SHA_MSG_LEFT_REG_OFFSET + (4 * i));
	}

	err = clk_set_rate(se_dev->pclk, freq);
	if (err) {
		dev_err(se_dev->dev, "clock set_rate failed.\n");
		return;
	}
	se_writel(se_dev, SHA_ENABLE, SE_SHA_CONFIG_REG_OFFSET);
}

static int tegra_se_start_operation(struct tegra_se_dev *se_dev, u32 nbytes,
	bool context_save)
{
	u32 nblocks = nbytes / TEGRA_SE_AES_BLOCK_SIZE;
	int ret = 0;
	u32 val = 0;

	if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA11) &&
				nblocks > SE_MAX_LAST_BLOCK_SIZE)
		return -EINVAL;

	/* clear any pending interrupts */
	val = se_readl(se_dev, SE_INT_STATUS_REG_OFFSET);
	se_writel(se_dev, val, SE_INT_STATUS_REG_OFFSET);
	se_writel(se_dev, se_dev->src_ll_buf_adr, SE_IN_LL_ADDR_REG_OFFSET);
	se_writel(se_dev, se_dev->dst_ll_buf_adr, SE_OUT_LL_ADDR_REG_OFFSET);

	if (nblocks)
		se_writel(se_dev, nblocks-1, SE_BLOCK_COUNT_REG_OFFSET);

	/* enable interupts */
	val = SE_INT_ERROR(INT_ENABLE) | SE_INT_OP_DONE(INT_ENABLE);
	se_writel(se_dev, val, SE_INT_ENABLE_REG_OFFSET);

	INIT_COMPLETION(se_dev->complete);

	if (context_save)
		se_writel(se_dev, SE_OPERATION(OP_CTX_SAVE),
			SE_OPERATION_REG_OFFSET);
	else
		se_writel(se_dev, SE_OPERATION(OP_SRART),
			SE_OPERATION_REG_OFFSET);

	ret = wait_for_completion_timeout(&se_dev->complete,
			msecs_to_jiffies(1000));
	if (ret == 0) {
		dev_err(se_dev->dev, "operation timed out no interrupt\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static void tegra_se_read_hash_result(struct tegra_se_dev *se_dev,
	u8 *pdata, u32 nbytes, bool swap32)
{
	u32 *result = (u32 *)pdata;
	u32 i;

	for (i = 0; i < nbytes/4; i++) {
		result[i] = se_readl(se_dev, SE_HASH_RESULT_REG_OFFSET +
				(i * sizeof(u32)));
		if (swap32)
			result[i] = be32_to_cpu(result[i]);
	}
}

static int tegra_map_sg(struct device *dev, struct scatterlist *sg,
			unsigned int nents, enum dma_data_direction dir,
			struct tegra_se_ll *se_ll, u32 total)
{
	u32 total_loop = 0;
	total_loop = total;

		while (sg) {
			dma_map_sg(dev, sg, 1, dir);
			se_ll->addr = sg_dma_address(sg);
			se_ll->data_len = min(sg->length, total_loop);
			total_loop -= min(sg->length, total_loop);
				sg = scatterwalk_sg_next(sg);
			se_ll++;
		}
	return nents;
}

static void tegra_unmap_sg(struct device *dev, struct scatterlist *sg,
				enum dma_data_direction dir, u32 total)
{
	while (sg) {
		dma_unmap_sg(dev, sg, 1, dir);
			sg = scatterwalk_sg_next(sg);
	}
}

static int tegra_se_count_sgs(struct scatterlist *sl, u32 nbytes, int *chained)
{
	struct scatterlist *sg = sl;
	int sg_nents = 0;

	*chained = 0;
	while (sg) {
		sg_nents++;
		nbytes -= min(sl->length, nbytes);
		if (!sg_is_last(sg) && (sg + 1)->length == 0)
			*chained = 1;
		sg = scatterwalk_sg_next(sg);
	}

	return sg_nents;
}

static int tegra_se_alloc_ll_buf(struct tegra_se_dev *se_dev,
	u32 num_src_sgs, u32 num_dst_sgs)
{
	if (se_dev->src_ll_buf || se_dev->dst_ll_buf) {
		dev_err(se_dev->dev, "trying to allocate memory to allocated memory\n");
		return -EBUSY;
	}

	if (num_src_sgs) {
		se_dev->src_ll_size =
			(sizeof(struct tegra_se_ll) * num_src_sgs) +
				sizeof(u32);
		se_dev->src_ll_buf = dma_alloc_coherent(se_dev->dev,
					se_dev->src_ll_size,
					&se_dev->src_ll_buf_adr, GFP_KERNEL);
		if (!se_dev->src_ll_buf) {
			dev_err(se_dev->dev, "can not allocate src lldma buffer\n");
			return -ENOMEM;
		}
	}
	if (num_dst_sgs) {
		se_dev->dst_ll_size =
				(sizeof(struct tegra_se_ll) * num_dst_sgs) +
					sizeof(u32);
		se_dev->dst_ll_buf = dma_alloc_coherent(se_dev->dev,
					se_dev->dst_ll_size,
					&se_dev->dst_ll_buf_adr, GFP_KERNEL);
		if (!se_dev->dst_ll_buf) {
			dev_err(se_dev->dev, "can not allocate dst ll dma buffer\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static void tegra_se_free_ll_buf(struct tegra_se_dev *se_dev)
{
	if (se_dev->src_ll_buf) {
		dma_free_coherent(se_dev->dev, se_dev->src_ll_size,
			se_dev->src_ll_buf, se_dev->src_ll_buf_adr);
		se_dev->src_ll_buf = NULL;
	}

	if (se_dev->dst_ll_buf) {
		dma_free_coherent(se_dev->dev, se_dev->dst_ll_size,
			se_dev->dst_ll_buf, se_dev->dst_ll_buf_adr);
		se_dev->dst_ll_buf = NULL;
	}
}

static int tegra_se_setup_ablk_req(struct tegra_se_dev *se_dev,
	struct ablkcipher_request *req)
{
	struct scatterlist *src_sg, *dst_sg;
	struct tegra_se_ll *src_ll, *dst_ll;
	u32 total, num_src_sgs, num_dst_sgs;
	int src_chained, dst_chained;
	int ret = 0;

	num_src_sgs = tegra_se_count_sgs(req->src, req->nbytes, &src_chained);
	num_dst_sgs = tegra_se_count_sgs(req->dst, req->nbytes, &dst_chained);

	if ((num_src_sgs > SE_MAX_SRC_SG_COUNT) ||
		(num_dst_sgs > SE_MAX_DST_SG_COUNT)) {
			dev_err(se_dev->dev, "num of SG buffers are more\n");
			return -EINVAL;
	}

	*se_dev->src_ll_buf = num_src_sgs-1;
	*se_dev->dst_ll_buf = num_dst_sgs-1;

	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);

	src_sg = req->src;
	dst_sg = req->dst;
	total = req->nbytes;

	if (total) {
		tegra_map_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE,
					src_ll, total);
		tegra_map_sg(se_dev->dev, dst_sg, 1, DMA_FROM_DEVICE,
					dst_ll, total);
		WARN_ON(src_sg->length != dst_sg->length);
	}
	return ret;
}

static void tegra_se_dequeue_complete_req(struct tegra_se_dev *se_dev,
	struct ablkcipher_request *req)
{
	struct scatterlist *src_sg, *dst_sg;
	u32 total;

	if (req) {
		src_sg = req->src;
		dst_sg = req->dst;
		total = req->nbytes;
		tegra_unmap_sg(se_dev->dev, dst_sg,  DMA_FROM_DEVICE, total);
		tegra_unmap_sg(se_dev->dev, src_sg,  DMA_TO_DEVICE, total);
	}
}

static void tegra_se_process_new_req(struct crypto_async_request *async_req)
{
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	struct ablkcipher_request *req = ablkcipher_request_cast(async_req);
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);
	struct tegra_se_aes_context *aes_ctx =
			crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	int ret = 0;

	/* take access to the hw */
	mutex_lock(&se_hw_lock);

	/* write IV */
	if (req->info) {
		if (req_ctx->op_mode == SE_AES_OP_MODE_CTR) {
			tegra_se_write_seed(se_dev, (u32 *)req->info);
		} else {
			tegra_se_write_key_table(req->info,
				TEGRA_SE_AES_IV_SIZE,
				aes_ctx->slot->slot_num,
				SE_KEY_TABLE_TYPE_ORGIV);
		}
	}
	tegra_se_setup_ablk_req(se_dev, req);
	tegra_se_config_algo(se_dev, req_ctx->op_mode, req_ctx->encrypt,
		aes_ctx->keylen);
	tegra_se_config_crypto(se_dev, req_ctx->op_mode, req_ctx->encrypt,
			aes_ctx->slot->slot_num, req->info ? true : false);
	ret = tegra_se_start_operation(se_dev, req->nbytes, false);
	tegra_se_dequeue_complete_req(se_dev, req);

	mutex_unlock(&se_hw_lock);
	req->base.complete(&req->base, ret);
}

static irqreturn_t tegra_se_irq(int irq, void *dev)
{
	struct tegra_se_dev *se_dev = dev;
	u32 val, err_stat;

	val = se_readl(se_dev, SE_INT_STATUS_REG_OFFSET);
	se_writel(se_dev, val, SE_INT_STATUS_REG_OFFSET);

	if (val & SE_INT_ERROR(INT_SET)) {
		err_stat = se_readl(se_dev, SE_ERR_STATUS_0);
		dev_err(se_dev->dev, "tegra_se_irq::error status is %x\n",
								err_stat);
	}

	if (val & SE_INT_OP_DONE(INT_SET))
		complete(&se_dev->complete);

	return IRQ_HANDLED;
}

static void tegra_se_work_handler(struct work_struct *work)
{
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	struct crypto_async_request *async_req = NULL;
	struct crypto_async_request *backlog = NULL;

	pm_runtime_get_sync(se_dev->dev);

	do {
		spin_lock_irq(&se_dev->lock);
		backlog = crypto_get_backlog(&se_dev->queue);
		async_req = crypto_dequeue_request(&se_dev->queue);
		if (!async_req)
			se_dev->work_q_busy = false;

		spin_unlock_irq(&se_dev->lock);

		if (backlog) {
			backlog->complete(backlog, -EINPROGRESS);
			backlog = NULL;
		}

		if (async_req) {
			tegra_se_process_new_req(async_req);
			async_req = NULL;
		}
	} while (se_dev->work_q_busy);
	pm_runtime_put(se_dev->dev);
}

static int tegra_se_aes_queue_req(struct ablkcipher_request *req)
{

	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	unsigned long flags;
	bool idle = true;
	int err = 0;
	int chained;

	if (!tegra_se_count_sgs(req->src, req->nbytes, &chained))
		return -EINVAL;

	spin_lock_irqsave(&se_dev->lock, flags);
	err = ablkcipher_enqueue_request(&se_dev->queue, req);
	if (se_dev->work_q_busy)
		idle = false;
	spin_unlock_irqrestore(&se_dev->lock, flags);

	if (idle) {
		spin_lock_irq(&se_dev->lock);
		se_dev->work_q_busy = true;
		spin_unlock_irq(&se_dev->lock);
		queue_work(se_work_q, &se_work);
	}

	return err;
}

static int tegra_se_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_CBC;

	return tegra_se_aes_queue_req(req);
}

static int tegra_se_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_CBC;

	return tegra_se_aes_queue_req(req);
}

static int tegra_se_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_ECB;

	return tegra_se_aes_queue_req(req);
}

static int tegra_se_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_ECB;

	return tegra_se_aes_queue_req(req);
}

static int tegra_se_aes_ctr_encrypt(struct ablkcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_CTR;

	return tegra_se_aes_queue_req(req);
}

static int tegra_se_aes_ctr_decrypt(struct ablkcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_CTR;

	return tegra_se_aes_queue_req(req);
}

static int tegra_se_aes_ofb_encrypt(struct ablkcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_OFB;

	return tegra_se_aes_queue_req(req);
}

static int tegra_se_aes_ofb_decrypt(struct ablkcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = ablkcipher_request_ctx(req);

	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_OFB;

	return tegra_se_aes_queue_req(req);
}

static int tegra_se_aes_setkey(struct crypto_ablkcipher *tfm,
	const u8 *key, u32 keylen)
{
	struct tegra_se_aes_context *ctx = crypto_ablkcipher_ctx(tfm);
	struct tegra_se_dev *se_dev = NULL;
	struct tegra_se_slot *pslot;
	u8 *pdata = (u8 *)key;

	if (!ctx || !ctx->se_dev) {
		pr_err("invalid context or dev");
		return -EINVAL;
	}

	se_dev = ctx->se_dev;

	if ((keylen != TEGRA_SE_KEY_128_SIZE) &&
		(keylen != TEGRA_SE_KEY_192_SIZE) &&
		(keylen != TEGRA_SE_KEY_256_SIZE)) {
		dev_err(se_dev->dev, "invalid key size");
		return -EINVAL;
	}

	if (key) {
		if (!ctx->slot || (ctx->slot &&
		    ctx->slot->slot_num == ssk_slot.slot_num)) {
			pslot = tegra_se_alloc_key_slot();
			if (!pslot) {
				dev_err(se_dev->dev, "no free key slot\n");
				return -ENOMEM;
			}
			ctx->slot = pslot;
		}
		ctx->keylen = keylen;
	} else {
		tegra_se_free_key_slot(ctx->slot);
		ctx->slot = &ssk_slot;
		ctx->keylen = AES_KEYSIZE_128;
	}

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	/* load the key */
	tegra_se_write_key_table(pdata, keylen, ctx->slot->slot_num,
		SE_KEY_TABLE_TYPE_KEY);

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return 0;
}

static int tegra_se_aes_cra_init(struct crypto_tfm *tfm)
{
	struct tegra_se_aes_context *ctx = crypto_tfm_ctx(tfm);

	ctx->se_dev = sg_tegra_se_dev;
	tfm->crt_ablkcipher.reqsize = sizeof(struct tegra_se_req_context);

	return 0;
}

static void tegra_se_aes_cra_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_aes_context *ctx = crypto_tfm_ctx(tfm);

	tegra_se_free_key_slot(ctx->slot);
	ctx->slot = NULL;
}

static int tegra_se_rng_init(struct crypto_tfm *tfm)
{
	struct tegra_se_rng_context *rng_ctx = crypto_tfm_ctx(tfm);
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;

	rng_ctx->se_dev = se_dev;
	rng_ctx->dt_buf = dma_alloc_coherent(se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
		&rng_ctx->dt_buf_adr, GFP_KERNEL);
	if (!rng_ctx->dt_buf) {
		dev_err(se_dev->dev, "can not allocate rng dma buffer");
		return -ENOMEM;
	}

	rng_ctx->rng_buf = dma_alloc_coherent(rng_ctx->se_dev->dev,
		TEGRA_SE_RNG_DT_SIZE, &rng_ctx->rng_buf_adr, GFP_KERNEL);
	if (!rng_ctx->rng_buf) {
		dev_err(se_dev->dev, "can not allocate rng dma buffer");
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
					rng_ctx->dt_buf, rng_ctx->dt_buf_adr);
		return -ENOMEM;
	}

	rng_ctx->slot = tegra_se_alloc_key_slot();

	if (!rng_ctx->slot) {
		dev_err(rng_ctx->se_dev->dev, "no free slot\n");
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
					rng_ctx->dt_buf, rng_ctx->dt_buf_adr);
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
					rng_ctx->rng_buf, rng_ctx->rng_buf_adr);
		return -ENOMEM;
	}

	return 0;
}

static void tegra_se_rng_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_rng_context *rng_ctx = crypto_tfm_ctx(tfm);

	if (rng_ctx->dt_buf) {
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
			rng_ctx->dt_buf, rng_ctx->dt_buf_adr);
	}

	if (rng_ctx->rng_buf) {
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
			rng_ctx->rng_buf, rng_ctx->rng_buf_adr);
	}

	tegra_se_free_key_slot(rng_ctx->slot);
	rng_ctx->slot = NULL;
	rng_ctx->se_dev = NULL;
}

static int tegra_se_rng_get_random(struct crypto_rng *tfm, u8 *rdata, u32 dlen)
{
	struct tegra_se_rng_context *rng_ctx = crypto_rng_ctx(tfm);
	struct tegra_se_dev *se_dev = rng_ctx->se_dev;
	struct tegra_se_ll *src_ll, *dst_ll;
	unsigned char *dt_buf = (unsigned char *)rng_ctx->dt_buf;
	u8 *rdata_addr;
	int ret = 0, i, j, num_blocks, data_len = 0;

	num_blocks = (dlen / TEGRA_SE_RNG_DT_SIZE);

	data_len = (dlen % TEGRA_SE_RNG_DT_SIZE);
	if (data_len == 0)
		num_blocks = num_blocks - 1;

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->src_ll_buf = 0;
	*se_dev->dst_ll_buf = 0;
	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);
	src_ll->addr = rng_ctx->dt_buf_adr;
	src_ll->data_len = TEGRA_SE_RNG_DT_SIZE;
	dst_ll->addr = rng_ctx->rng_buf_adr;
	dst_ll->data_len = TEGRA_SE_RNG_DT_SIZE;

	tegra_se_config_algo(se_dev, SE_AES_OP_MODE_RNG_X931, true,
		TEGRA_SE_KEY_128_SIZE);
	tegra_se_config_crypto(se_dev, SE_AES_OP_MODE_RNG_X931, true,
				rng_ctx->slot->slot_num, rng_ctx->use_org_iv);
	for (j = 0; j <= num_blocks; j++) {
		ret = tegra_se_start_operation(se_dev,
				TEGRA_SE_RNG_DT_SIZE, false);

		if (!ret) {
			rdata_addr = (rdata + (j * TEGRA_SE_RNG_DT_SIZE));

			if (data_len && num_blocks == j) {
				memcpy(rdata_addr, rng_ctx->rng_buf, data_len);
			} else {
				memcpy(rdata_addr,
					rng_ctx->rng_buf, TEGRA_SE_RNG_DT_SIZE);
			}

			/* update DT vector */
			for (i = TEGRA_SE_RNG_DT_SIZE - 1; i >= 0; i--) {
				dt_buf[i] += 1;
				if (dt_buf[i] != 0)
					break;
			}
		} else {
			dlen = 0;
		}
		if (rng_ctx->use_org_iv) {
			rng_ctx->use_org_iv = false;
			tegra_se_config_crypto(se_dev,
				SE_AES_OP_MODE_RNG_X931, true,
				rng_ctx->slot->slot_num, rng_ctx->use_org_iv);
		}
	}

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return dlen;
}

static int tegra_se_rng_reset(struct crypto_rng *tfm, u8 *seed, u32 slen)
{
	struct tegra_se_rng_context *rng_ctx = crypto_rng_ctx(tfm);
	struct tegra_se_dev *se_dev = rng_ctx->se_dev;
	u8 *iv = seed;
	u8 *key = (u8 *)(seed + TEGRA_SE_RNG_IV_SIZE);
	u8 *dt = key + TEGRA_SE_RNG_KEY_SIZE;
	struct timespec ts;
	u64 nsec, tmp[2];

	BUG_ON(!seed);

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	tegra_se_write_key_table(key, TEGRA_SE_RNG_KEY_SIZE,
		rng_ctx->slot->slot_num, SE_KEY_TABLE_TYPE_KEY);

	tegra_se_write_key_table(iv, TEGRA_SE_RNG_IV_SIZE,
		rng_ctx->slot->slot_num, SE_KEY_TABLE_TYPE_ORGIV);

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	if (slen < TEGRA_SE_RNG_SEED_SIZE) {
		getnstimeofday(&ts);
		nsec = timespec_to_ns(&ts);
		do_div(nsec, 1000);
		nsec ^= se_dev->ctr << 56;
		se_dev->ctr++;
		tmp[0] = nsec;
		tmp[1] = tegra_chip_uid();
		memcpy(rng_ctx->dt_buf, (u8 *)tmp, TEGRA_SE_RNG_DT_SIZE);
	} else {
		memcpy(rng_ctx->dt_buf, dt, TEGRA_SE_RNG_DT_SIZE);
	}

	rng_ctx->use_org_iv = true;

	return 0;
}

static int tegra_se_rng_drbg_init(struct crypto_tfm *tfm)
{
	struct tegra_se_rng_context *rng_ctx = crypto_tfm_ctx(tfm);
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;

	rng_ctx->se_dev = se_dev;
	rng_ctx->dt_buf = dma_alloc_coherent(se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
		&rng_ctx->dt_buf_adr, GFP_KERNEL);
	if (!rng_ctx->dt_buf) {
		dev_err(se_dev->dev, "can not allocate rng dma buffer");
		return -ENOMEM;
	}

	rng_ctx->rng_buf = dma_alloc_coherent(rng_ctx->se_dev->dev,
		TEGRA_SE_RNG_DT_SIZE, &rng_ctx->rng_buf_adr, GFP_KERNEL);
	if (!rng_ctx->rng_buf) {
		dev_err(se_dev->dev, "can not allocate rng dma buffer");
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
					rng_ctx->dt_buf, rng_ctx->dt_buf_adr);
		return -ENOMEM;
	}

	return 0;
}

static int tegra_se_rng_drbg_get_random(struct crypto_rng *tfm,
	u8 *rdata, u32 dlen)
{
	struct tegra_se_rng_context *rng_ctx = crypto_rng_ctx(tfm);
	struct tegra_se_dev *se_dev = rng_ctx->se_dev;
	struct tegra_se_ll *src_ll, *dst_ll;
	u8 *rdata_addr;
	int ret = 0, j, num_blocks, data_len = 0;

	num_blocks = (dlen / TEGRA_SE_RNG_DT_SIZE);

	data_len = (dlen % TEGRA_SE_RNG_DT_SIZE);
	if (data_len == 0)
		num_blocks = num_blocks - 1;

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->src_ll_buf = 0;
	*se_dev->dst_ll_buf = 0;
	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);
	src_ll->addr = rng_ctx->dt_buf_adr;
	src_ll->data_len = TEGRA_SE_RNG_DT_SIZE;
	dst_ll->addr = rng_ctx->rng_buf_adr;
	dst_ll->data_len = TEGRA_SE_RNG_DT_SIZE;

	tegra_se_config_algo(se_dev, SE_AES_OP_MODE_RNG_DRBG, true,
		TEGRA_SE_KEY_128_SIZE);
	tegra_se_config_crypto(se_dev, SE_AES_OP_MODE_RNG_DRBG, true,
				0, true);
	for (j = 0; j <= num_blocks; j++) {
		ret = tegra_se_start_operation(se_dev,
				TEGRA_SE_RNG_DT_SIZE, false);

		if (!ret) {
			rdata_addr = (rdata + (j * TEGRA_SE_RNG_DT_SIZE));

			if (data_len && num_blocks == j) {
				memcpy(rdata_addr, rng_ctx->rng_buf, data_len);
			} else {
				memcpy(rdata_addr,
					rng_ctx->rng_buf, TEGRA_SE_RNG_DT_SIZE);
			}
		} else {
			dlen = 0;
		}
	}
	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return dlen;
}

static int tegra_se_rng_drbg_reset(struct crypto_rng *tfm, u8 *seed, u32 slen)
{
	return 0;
}

static void tegra_se_rng_drbg_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_rng_context *rng_ctx = crypto_tfm_ctx(tfm);

	if (rng_ctx->dt_buf) {
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
			rng_ctx->dt_buf, rng_ctx->dt_buf_adr);
	}

	if (rng_ctx->rng_buf) {
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
			rng_ctx->rng_buf, rng_ctx->rng_buf_adr);
	}
	rng_ctx->se_dev = NULL;
}

int tegra_se_sha_init(struct ahash_request *req)
{
	return 0;
}

int tegra_se_sha_update(struct ahash_request *req)
{
	return 0;
}

int tegra_se_sha_finup(struct ahash_request *req)
{
	return 0;
}

int tegra_se_sha_final(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct tegra_se_sha_context *sha_ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	struct scatterlist *src_sg;
	struct tegra_se_ll *src_ll;
	u32 total, num_sgs;
	unsigned long freq = 0;
	int err = 0;
	int chained;

	if (!req->nbytes)
		return -EINVAL;

	if (crypto_ahash_digestsize(tfm) == SHA1_DIGEST_SIZE) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA1;
		freq = se_dev->chipdata->sha1_freq;
	}

	if (crypto_ahash_digestsize(tfm) == SHA224_DIGEST_SIZE) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA224;
		freq = se_dev->chipdata->sha224_freq;
	}

	if (crypto_ahash_digestsize(tfm) == SHA256_DIGEST_SIZE) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA256;
		freq = se_dev->chipdata->sha256_freq;
	}

	if (crypto_ahash_digestsize(tfm) == SHA384_DIGEST_SIZE) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA384;
		freq = se_dev->chipdata->sha384_freq;
	}

	if (crypto_ahash_digestsize(tfm) == SHA512_DIGEST_SIZE) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA512;
		freq = se_dev->chipdata->sha512_freq;
	}

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	num_sgs = tegra_se_count_sgs(req->src, req->nbytes, &chained);
	if ((num_sgs > SE_MAX_SRC_SG_COUNT)) {
		dev_err(se_dev->dev, "num of SG buffers are more\n");
		pm_runtime_put(se_dev->dev);
		mutex_unlock(&se_hw_lock);
		return -EINVAL;
	}

	*se_dev->src_ll_buf = num_sgs - 1;
	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	src_sg = req->src;
	total = req->nbytes;

	if (total)
		tegra_map_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE,
							src_ll, total);

	tegra_se_config_algo(se_dev, sha_ctx->op_mode, false, 0);
	tegra_se_config_sha(se_dev, req->nbytes, freq);
	err = tegra_se_start_operation(se_dev, 0, false);
	if (!err) {
		tegra_se_read_hash_result(se_dev, req->result,
			crypto_ahash_digestsize(tfm), true);
		if ((sha_ctx->op_mode == SE_AES_OP_MODE_SHA384) ||
			(sha_ctx->op_mode == SE_AES_OP_MODE_SHA512)) {
			u32 *result = (u32 *)req->result;
			u32 temp, i;

			for (i = 0; i < crypto_ahash_digestsize(tfm)/4;
				i += 2) {
				temp = result[i];
				result[i] = result[i+1];
				result[i+1] = temp;
			}
		}
	}

	src_sg = req->src;
	total = req->nbytes;
	if (total)
		tegra_unmap_sg(se_dev->dev, src_sg, DMA_TO_DEVICE, total);

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return err;
}

static int tegra_se_sha_digest(struct ahash_request *req)
{
	return tegra_se_sha_init(req) ?: tegra_se_sha_final(req);
}

int tegra_se_sha_cra_init(struct crypto_tfm *tfm)
{
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct tegra_se_sha_context));
	return 0;
}

void tegra_se_sha_cra_exit(struct crypto_tfm *tfm)
{
	/* do nothing */
}

int tegra_se_aes_cmac_init(struct ahash_request *req)
{

	return 0;
}

int tegra_se_aes_cmac_update(struct ahash_request *req)
{
	return 0;
}

int tegra_se_aes_cmac_final(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct tegra_se_aes_cmac_context *cmac_ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	struct scatterlist *src_sg;
	struct tegra_se_ll *src_ll;
	struct sg_mapping_iter miter;
	u32 num_sgs, blocks_to_process, last_block_bytes = 0, bytes_to_copy = 0;
	u8 piv[TEGRA_SE_AES_IV_SIZE];
	int total, ret = 0, i = 0, mapped_sg_count = 0;
	bool padding_needed = false;
	unsigned long flags;
	unsigned int sg_flags = SG_MITER_ATOMIC;
	u8 *temp_buffer = NULL;
	bool use_orig_iv = true;
	int chained;

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	blocks_to_process = req->nbytes / TEGRA_SE_AES_BLOCK_SIZE;
	/* num of bytes less than block size */
	if ((req->nbytes % TEGRA_SE_AES_BLOCK_SIZE) || !blocks_to_process) {
		padding_needed = true;
		last_block_bytes = req->nbytes % TEGRA_SE_AES_BLOCK_SIZE;
	} else {
		/* decrement num of blocks */
		blocks_to_process--;
		if (blocks_to_process) {
			/* there are blocks to process and find last block
				bytes */
			last_block_bytes = req->nbytes -
				(blocks_to_process * TEGRA_SE_AES_BLOCK_SIZE);
		} else {
			/* this is the last block and equal to block size */
			last_block_bytes = req->nbytes;
		}
	}

	/* first process all blocks except last block */
	if (blocks_to_process) {
		num_sgs = tegra_se_count_sgs(req->src, req->nbytes, &chained);
		if (num_sgs > SE_MAX_SRC_SG_COUNT) {
			dev_err(se_dev->dev, "num of SG buffers are more\n");
			goto out;
		}
		*se_dev->src_ll_buf = num_sgs - 1;
		src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
		src_sg = req->src;
		total = blocks_to_process * TEGRA_SE_AES_BLOCK_SIZE;
		while (total > 0) {
			ret = dma_map_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE);
			mapped_sg_count++;
			if (!ret) {
				dev_err(se_dev->dev, "dma_map_sg() error\n");
				goto out;
			}
			src_ll->addr = sg_dma_address(src_sg);
			if (total > src_sg->length)
				src_ll->data_len = src_sg->length;
			else
				src_ll->data_len = total;

			total -= src_sg->length;
			if (total > 0) {
					src_sg = scatterwalk_sg_next(src_sg);
				src_ll++;
			}
			WARN_ON(((total != 0) && (!src_sg)));
		}
		tegra_se_config_algo(se_dev, SE_AES_OP_MODE_CMAC, true,
			cmac_ctx->keylen);
		/* write zero IV */
		memset(piv, 0, TEGRA_SE_AES_IV_SIZE);
		tegra_se_write_key_table(piv, TEGRA_SE_AES_IV_SIZE,
					cmac_ctx->slot->slot_num,
					SE_KEY_TABLE_TYPE_ORGIV);
		tegra_se_config_crypto(se_dev, SE_AES_OP_MODE_CMAC, true,
					cmac_ctx->slot->slot_num, true);
		tegra_se_start_operation(se_dev,
			blocks_to_process * TEGRA_SE_AES_BLOCK_SIZE, false);
		src_sg = req->src;
		while (mapped_sg_count--) {
			dma_unmap_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE);
				src_sg = scatterwalk_sg_next(src_sg);
		}
		use_orig_iv = false;
	}

	/* get the last block bytes from the sg_dma buffer using miter */
	src_sg = req->src;
	num_sgs = tegra_se_count_sgs(req->src, req->nbytes, &chained);
	sg_flags |= SG_MITER_FROM_SG;
	sg_miter_start(&miter, req->src, num_sgs, sg_flags);
	local_irq_save(flags);
	total = 0;
	cmac_ctx->buffer = dma_alloc_coherent(se_dev->dev,
				TEGRA_SE_AES_BLOCK_SIZE,
				&cmac_ctx->dma_addr, GFP_KERNEL);

	if (!cmac_ctx->buffer)
		goto out;

	temp_buffer = cmac_ctx->buffer;
	while (sg_miter_next(&miter) && total < req->nbytes) {
		unsigned int len;
		len = min(miter.length, req->nbytes - total);
		if ((req->nbytes - (total + len)) <= last_block_bytes) {
			bytes_to_copy =
				last_block_bytes -
				(req->nbytes - (total + len));
			memcpy(temp_buffer, miter.addr + (len - bytes_to_copy),
				bytes_to_copy);
			last_block_bytes -= bytes_to_copy;
			temp_buffer += bytes_to_copy;
		}
		total += len;
	}
	sg_miter_stop(&miter);
	local_irq_restore(flags);

	/* process last block */
	if (padding_needed) {
		/* pad with 0x80, 0, 0 ... */
		last_block_bytes = req->nbytes % TEGRA_SE_AES_BLOCK_SIZE;
		cmac_ctx->buffer[last_block_bytes] = 0x80;
		for (i = last_block_bytes+1; i < TEGRA_SE_AES_BLOCK_SIZE; i++)
			cmac_ctx->buffer[i] = 0;
		/* XOR with K2 */
		for (i = 0; i < TEGRA_SE_AES_BLOCK_SIZE; i++)
			cmac_ctx->buffer[i] ^= cmac_ctx->K2[i];
	} else {
		/* XOR with K1 */
		for (i = 0; i < TEGRA_SE_AES_BLOCK_SIZE; i++)
			cmac_ctx->buffer[i] ^= cmac_ctx->K1[i];
	}
	*se_dev->src_ll_buf = 0;
	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	src_ll->addr = cmac_ctx->dma_addr;
	src_ll->data_len = TEGRA_SE_AES_BLOCK_SIZE;

	if (use_orig_iv) {
		/* use zero IV, this is when num of bytes is
			less <= block size */
		memset(piv, 0, TEGRA_SE_AES_IV_SIZE);
		tegra_se_write_key_table(piv, TEGRA_SE_AES_IV_SIZE,
					cmac_ctx->slot->slot_num,
					SE_KEY_TABLE_TYPE_ORGIV);
	}

	tegra_se_config_algo(se_dev, SE_AES_OP_MODE_CMAC, true,
				cmac_ctx->keylen);
	tegra_se_config_crypto(se_dev, SE_AES_OP_MODE_CMAC, true,
				cmac_ctx->slot->slot_num, use_orig_iv);
	tegra_se_start_operation(se_dev, TEGRA_SE_AES_BLOCK_SIZE, false);
	tegra_se_read_hash_result(se_dev, req->result,
				TEGRA_SE_AES_CMAC_DIGEST_SIZE, false);

out:
	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	if (cmac_ctx->buffer)
		dma_free_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
			cmac_ctx->buffer, cmac_ctx->dma_addr);

	return 0;
}

int tegra_se_aes_cmac_setkey(struct crypto_ahash *tfm, const u8 *key,
		unsigned int keylen)
{
	struct tegra_se_aes_cmac_context *ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	struct tegra_se_ll *src_ll, *dst_ll;
	struct tegra_se_slot *pslot;
	u8 piv[TEGRA_SE_AES_IV_SIZE];
	u32 *pbuf;
	dma_addr_t pbuf_adr;
	int ret = 0;
	u8 const rb = 0x87;
	u8 msb;

	if (!ctx) {
		dev_err(se_dev->dev, "invalid context");
		return -EINVAL;
	}

	if ((keylen != TEGRA_SE_KEY_128_SIZE) &&
		(keylen != TEGRA_SE_KEY_192_SIZE) &&
		(keylen != TEGRA_SE_KEY_256_SIZE)) {
		dev_err(se_dev->dev, "invalid key size");
		return -EINVAL;
	}

	if (key) {
		if (!ctx->slot || (ctx->slot &&
		    ctx->slot->slot_num == ssk_slot.slot_num)) {
			pslot = tegra_se_alloc_key_slot();
			if (!pslot) {
				dev_err(se_dev->dev, "no free key slot\n");
				return -ENOMEM;
			}
			ctx->slot = pslot;
		}
		ctx->keylen = keylen;
	} else {
		tegra_se_free_key_slot(ctx->slot);
		ctx->slot = &ssk_slot;
		ctx->keylen = AES_KEYSIZE_128;
	}

	pbuf = dma_alloc_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
		&pbuf_adr, GFP_KERNEL);
	if (!pbuf) {
		dev_err(se_dev->dev, "can not allocate dma buffer");
		return -ENOMEM;
	}
	memset(pbuf, 0, TEGRA_SE_AES_BLOCK_SIZE);

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->src_ll_buf = 0;
	*se_dev->dst_ll_buf = 0;
	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);

	src_ll->addr = pbuf_adr;
	src_ll->data_len = TEGRA_SE_AES_BLOCK_SIZE;
	dst_ll->addr = pbuf_adr;
	dst_ll->data_len = TEGRA_SE_AES_BLOCK_SIZE;

	/* load the key */
	tegra_se_write_key_table((u8 *)key, keylen,
				ctx->slot->slot_num, SE_KEY_TABLE_TYPE_KEY);

	/* write zero IV */
	memset(piv, 0, TEGRA_SE_AES_IV_SIZE);

	/* load IV */
	tegra_se_write_key_table(piv, TEGRA_SE_AES_IV_SIZE,
				ctx->slot->slot_num, SE_KEY_TABLE_TYPE_ORGIV);

	/* config crypto algo */
	tegra_se_config_algo(se_dev, SE_AES_OP_MODE_CBC, true, keylen);

	tegra_se_config_crypto(se_dev, SE_AES_OP_MODE_CBC, true,
		ctx->slot->slot_num, true);

	ret = tegra_se_start_operation(se_dev, TEGRA_SE_AES_BLOCK_SIZE, false);
	if (ret) {
		dev_err(se_dev->dev, "tegra_se_aes_cmac_setkey:: start op failed\n");
		goto out;
	}

	/* compute K1 subkey */
	memcpy(ctx->K1, pbuf, TEGRA_SE_AES_BLOCK_SIZE);
	tegra_se_leftshift_onebit(ctx->K1, TEGRA_SE_AES_BLOCK_SIZE, &msb);
	if (msb)
		ctx->K1[TEGRA_SE_AES_BLOCK_SIZE - 1] ^= rb;

	/* compute K2 subkey */
	memcpy(ctx->K2, ctx->K1, TEGRA_SE_AES_BLOCK_SIZE);
	tegra_se_leftshift_onebit(ctx->K2, TEGRA_SE_AES_BLOCK_SIZE, &msb);

	if (msb)
		ctx->K2[TEGRA_SE_AES_BLOCK_SIZE - 1] ^= rb;

out:
	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	if (pbuf) {
		dma_free_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
			pbuf, pbuf_adr);
	}

	return 0;
}

int tegra_se_aes_cmac_digest(struct ahash_request *req)
{
	return tegra_se_aes_cmac_init(req) ?: tegra_se_aes_cmac_final(req);
}

int tegra_se_aes_cmac_finup(struct ahash_request *req)
{
	return 0;
}

int tegra_se_aes_cmac_cra_init(struct crypto_tfm *tfm)
{
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct tegra_se_aes_cmac_context));

	return 0;
}
void tegra_se_aes_cmac_cra_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_aes_cmac_context *ctx = crypto_tfm_ctx(tfm);

	tegra_se_free_key_slot(ctx->slot);
	ctx->slot = NULL;
}

/* Security Engine rsa key slot */
struct tegra_se_rsa_slot {
	struct list_head node;
	u8 slot_num;	/* Key slot number */
	bool available; /* Tells whether key slot is free to use */
};


/* Security Engine AES RSA context */
struct tegra_se_aes_rsa_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_rsa_slot *slot;	/* Security Engine rsa key slot */
	u32 keylen;	/* key length in bits */
};

static void tegra_se_rsa_free_key_slot(struct tegra_se_rsa_slot *slot)
{
	if (slot) {
		spin_lock(&rsa_key_slot_lock);
		slot->available = true;
		spin_unlock(&rsa_key_slot_lock);
	}
}

static struct tegra_se_rsa_slot *tegra_se_alloc_rsa_key_slot(void)
{
	struct tegra_se_rsa_slot *slot = NULL;
	bool found = false;

	spin_lock(&rsa_key_slot_lock);
	list_for_each_entry(slot, &rsa_key_slot, node) {
		if (slot->available) {
			slot->available = false;
			found = true;
			break;
		}
	}
	spin_unlock(&rsa_key_slot_lock);
	return found ? slot : NULL;
}

static int tegra_init_rsa_key_slot(struct tegra_se_dev *se_dev)
{
	int i;

	se_dev->rsa_slot_list = kzalloc(sizeof(struct tegra_se_rsa_slot) *
					TEGRA_SE_RSA_KEYSLOT_COUNT, GFP_KERNEL);
	if (se_dev->rsa_slot_list == NULL) {
		dev_err(se_dev->dev, "rsa slot list memory allocation failed\n");
		return -ENOMEM;
	}
	spin_lock_init(&rsa_key_slot_lock);
	spin_lock(&rsa_key_slot_lock);
	for (i = 0; i < TEGRA_SE_RSA_KEYSLOT_COUNT; i++) {
		se_dev->rsa_slot_list[i].available = true;
		se_dev->rsa_slot_list[i].slot_num = i;
		INIT_LIST_HEAD(&se_dev->rsa_slot_list[i].node);
		list_add_tail(&se_dev->rsa_slot_list[i].node, &rsa_key_slot);
	}
	spin_unlock(&rsa_key_slot_lock);

	return 0;
}

int tegra_se_rsa_init(struct ahash_request *req)
{
	return 0;
}

int tegra_se_rsa_update(struct ahash_request *req)
{
	return 0;
}

int tegra_se_rsa_final(struct ahash_request *req)
{
	return 0;
}

int tegra_se_rsa_setkey(struct crypto_ahash *tfm, const u8 *key,
		unsigned int keylen)
{
	struct tegra_se_aes_rsa_context *ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	u32 module_key_length = 0;
	u32 exponent_key_length = 0;
	u32 pkt, val;
	u32 key_size_words;
	u32 key_word_size = 4;
	u32 *pkeydata = (u32 *)key;
	s32 i = 0;
	struct tegra_se_rsa_slot *pslot;
	unsigned long freq = 0;
	int err = 0;

	if (!ctx || !key)
		return -EINVAL;

	/* Allocate rsa key slot */
	pslot = tegra_se_alloc_rsa_key_slot();
	if (!pslot) {
		dev_err(se_dev->dev, "no free key slot\n");
		return -ENOMEM;
	}
	ctx->slot = pslot;
	ctx->keylen = keylen;

	module_key_length = (keylen >> 16);
	exponent_key_length = (keylen & (0xFFFF));

	if (!(((module_key_length / 64) >= 1) &&
			((module_key_length / 64) <= 4)))
		return -EINVAL;

	freq = se_dev->chipdata->rsa_freq;
	err = clk_set_rate(se_dev->pclk, freq);
	if (err) {
		dev_err(se_dev->dev, "clock set_rate failed.\n");
		return err;
	}

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	/* Write key length */
	se_writel(se_dev, ((module_key_length / 64) - 1),
		SE_RSA_KEY_SIZE_REG_OFFSET);

	/* Write exponent size in 32 bytes */
	se_writel(se_dev, (exponent_key_length / 4),
		SE_RSA_EXP_SIZE_REG_OFFSET);

	if (exponent_key_length) {
		key_size_words = (exponent_key_length / key_word_size);
		/* Write exponent */
		for (i = (key_size_words - 1); i >= 0; i--) {
			se_writel(se_dev, *pkeydata++, SE_RSA_KEYTABLE_DATA);
			pkt = RSA_KEY_INPUT_MODE(RSA_KEY_INPUT_MODE_REG) |
				RSA_KEY_NUM(ctx->slot->slot_num) |
				RSA_KEY_TYPE(RSA_KEY_TYPE_EXP) |
				RSA_KEY_PKT_WORD_ADDR(i);
			val = SE_RSA_KEY_OP(RSA_KEY_WRITE) |
					SE_RSA_KEYTABLE_PKT(pkt);

			se_writel(se_dev, val, SE_RSA_KEYTABLE_ADDR);
		}
	}

	if (module_key_length) {
		key_size_words = (module_key_length / key_word_size);
		/* Write moduleus */
		for (i = (key_size_words - 1); i >= 0; i--) {
			se_writel(se_dev, *pkeydata++, SE_RSA_KEYTABLE_DATA);
			pkt = RSA_KEY_INPUT_MODE(RSA_KEY_INPUT_MODE_REG) |
				RSA_KEY_NUM(ctx->slot->slot_num) |
				RSA_KEY_TYPE(RSA_KEY_TYPE_MOD) |
				RSA_KEY_PKT_WORD_ADDR(i);
			val = SE_RSA_KEY_OP(RSA_KEY_WRITE) |
					SE_RSA_KEYTABLE_PKT(pkt);

			se_writel(se_dev, val, SE_RSA_KEYTABLE_ADDR);
		}
	}
	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);
	return 0;
}

static void tegra_se_read_rsa_result(struct tegra_se_dev *se_dev,
	u8 *pdata, unsigned int nbytes)
{
	u32 *result = (u32 *)pdata;
	u32 i;

	for (i = 0; i < nbytes / 4; i++)
		result[i] = se_readl(se_dev, SE_RSA_OUTPUT +
				(i * sizeof(u32)));
}

int tegra_se_rsa_digest(struct ahash_request *req)
{
	struct crypto_ahash *tfm = NULL;
	struct tegra_se_aes_rsa_context *rsa_ctx = NULL;
	struct tegra_se_dev *se_dev = sg_tegra_se_dev;
	struct scatterlist *src_sg;
	struct tegra_se_ll *src_ll;
	u32 num_sgs;
	int total, ret = 0;
	u32 val = 0;
	int chained;

	if (!req)
		return -EINVAL;

	tfm = crypto_ahash_reqtfm(req);

	if (!tfm)
		return -EINVAL;

	rsa_ctx = crypto_ahash_ctx(tfm);

	if (!rsa_ctx || !rsa_ctx->slot)
		return -EINVAL;

	if (!req->nbytes)
		return -EINVAL;

	if ((req->nbytes < TEGRA_SE_RSA512_DIGEST_SIZE) ||
			(req->nbytes > TEGRA_SE_RSA2048_DIGEST_SIZE))
		return -EINVAL;

	num_sgs = tegra_se_count_sgs(req->src, req->nbytes, &chained);
	if (num_sgs > SE_MAX_SRC_SG_COUNT) {
		dev_err(se_dev->dev, "num of SG buffers are more\n");
		return -EINVAL;
	}

	*se_dev->src_ll_buf = num_sgs - 1;
	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	src_sg = req->src;
	total = req->nbytes;

	while (total > 0) {
		ret = dma_map_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE);
		if (!ret) {
			dev_err(se_dev->dev, "dma_map_sg() error\n");
			goto out;
		}
		src_ll->addr = sg_dma_address(src_sg);
		src_ll->data_len = src_sg->length;

		total -= src_sg->length;
		if (total > 0) {
				src_sg = scatterwalk_sg_next(src_sg);
			src_ll++;
		}
		WARN_ON(((total != 0) && (!src_sg)));
	}

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	val = SE_CONFIG_ENC_ALG(ALG_RSA) |
		SE_CONFIG_DEC_ALG(ALG_NOP) |
		SE_CONFIG_DST(DST_RSAREG);
	se_writel(se_dev, val, SE_CONFIG_REG_OFFSET);
	se_writel(se_dev, RSA_KEY_SLOT(rsa_ctx->slot->slot_num), SE_RSA_CONFIG);
	se_writel(se_dev, SE_CRYPTO_INPUT_SEL(INPUT_AHB), SE_CRYPTO_REG_OFFSET);

	ret = tegra_se_start_operation(se_dev, 256, false);
	if (ret) {
		dev_err(se_dev->dev, "tegra_se_aes_rsa_digest:: start op failed\n");
		pm_runtime_put_sync(se_dev->dev);
		mutex_unlock(&se_hw_lock);
		goto out;
	}

	tegra_se_read_rsa_result(se_dev, req->result, req->nbytes);

	pm_runtime_put_sync(se_dev->dev);
	mutex_unlock(&se_hw_lock);

out:
	return ret;
}

int tegra_se_rsa_finup(struct ahash_request *req)
{
	return 0;
}

int tegra_se_rsa_cra_init(struct crypto_tfm *tfm)
{
	return 0;
}

void tegra_se_rsa_cra_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_aes_rsa_context *ctx = crypto_tfm_ctx(tfm);

	tegra_se_rsa_free_key_slot(ctx->slot);
	ctx->slot = NULL;
}

static struct crypto_alg aes_algs[] = {
	{
		.cra_name = "cbc(aes)",
		.cra_driver_name = "cbc-aes-tegra",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = TEGRA_SE_AES_BLOCK_SIZE,
		.cra_ctxsize  = sizeof(struct tegra_se_aes_context),
		.cra_alignmask = 0,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_se_aes_cra_init,
		.cra_exit = tegra_se_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = TEGRA_SE_AES_MIN_KEY_SIZE,
			.max_keysize = TEGRA_SE_AES_MAX_KEY_SIZE,
			.ivsize = TEGRA_SE_AES_IV_SIZE,
			.setkey = tegra_se_aes_setkey,
			.encrypt = tegra_se_aes_cbc_encrypt,
			.decrypt = tegra_se_aes_cbc_decrypt,
		}
	}, {
		.cra_name = "ecb(aes)",
		.cra_driver_name = "ecb-aes-tegra",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = TEGRA_SE_AES_BLOCK_SIZE,
		.cra_ctxsize  = sizeof(struct tegra_se_aes_context),
		.cra_alignmask = 0,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_se_aes_cra_init,
		.cra_exit = tegra_se_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = TEGRA_SE_AES_MIN_KEY_SIZE,
			.max_keysize = TEGRA_SE_AES_MAX_KEY_SIZE,
			.ivsize = TEGRA_SE_AES_IV_SIZE,
			.setkey = tegra_se_aes_setkey,
			.encrypt = tegra_se_aes_ecb_encrypt,
			.decrypt = tegra_se_aes_ecb_decrypt,
		}
	}, {
		.cra_name = "ctr(aes)",
		.cra_driver_name = "ctr-aes-tegra",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = TEGRA_SE_AES_BLOCK_SIZE,
		.cra_ctxsize  = sizeof(struct tegra_se_aes_context),
		.cra_alignmask = 0,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_se_aes_cra_init,
		.cra_exit = tegra_se_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = TEGRA_SE_AES_MIN_KEY_SIZE,
			.max_keysize = TEGRA_SE_AES_MAX_KEY_SIZE,
			.ivsize = TEGRA_SE_AES_IV_SIZE,
			.setkey = tegra_se_aes_setkey,
			.encrypt = tegra_se_aes_ctr_encrypt,
			.decrypt = tegra_se_aes_ctr_decrypt,
			.geniv = "eseqiv",
		}
	}, {
		.cra_name = "ofb(aes)",
		.cra_driver_name = "ofb-aes-tegra",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = TEGRA_SE_AES_BLOCK_SIZE,
		.cra_ctxsize  = sizeof(struct tegra_se_aes_context),
		.cra_alignmask = 0,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_se_aes_cra_init,
		.cra_exit = tegra_se_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = TEGRA_SE_AES_MIN_KEY_SIZE,
			.max_keysize = TEGRA_SE_AES_MAX_KEY_SIZE,
			.ivsize = TEGRA_SE_AES_IV_SIZE,
			.setkey = tegra_se_aes_setkey,
			.encrypt = tegra_se_aes_ofb_encrypt,
			.decrypt = tegra_se_aes_ofb_decrypt,
			.geniv = "eseqiv",
		}
	}, {
		.cra_name = "ansi_cprng",
		.cra_driver_name = "rng-aes-tegra",
		.cra_priority = 100,
		.cra_flags = CRYPTO_ALG_TYPE_RNG,
		.cra_ctxsize = sizeof(struct tegra_se_rng_context),
		.cra_type = &crypto_rng_type,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_se_rng_init,
		.cra_exit = tegra_se_rng_exit,
		.cra_u = {
			.rng = {
				.rng_make_random = tegra_se_rng_get_random,
				.rng_reset = tegra_se_rng_reset,
				.seedsize = TEGRA_SE_RNG_SEED_SIZE,
			}
		}
	}, {
		.cra_name = "rng_drbg",
		.cra_driver_name = "rng_drbg-aes-tegra",
		.cra_priority = 100,
		.cra_flags = CRYPTO_ALG_TYPE_RNG,
		.cra_ctxsize = sizeof(struct tegra_se_rng_context),
		.cra_type = &crypto_rng_type,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_se_rng_drbg_init,
		.cra_exit = tegra_se_rng_drbg_exit,
		.cra_u = {
			.rng = {
				.rng_make_random = tegra_se_rng_drbg_get_random,
				.rng_reset = tegra_se_rng_drbg_reset,
				.seedsize = TEGRA_SE_RNG_SEED_SIZE,
			}
		}
	}
};

static struct ahash_alg hash_algs[] = {
	{
		.init = tegra_se_aes_cmac_init,
		.update = tegra_se_aes_cmac_update,
		.final = tegra_se_aes_cmac_final,
		.finup = tegra_se_aes_cmac_finup,
		.digest = tegra_se_aes_cmac_digest,
		.setkey = tegra_se_aes_cmac_setkey,
		.halg.digestsize = TEGRA_SE_AES_CMAC_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "cmac(aes)",
			.cra_driver_name = "tegra-se-cmac(aes)",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = TEGRA_SE_AES_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_aes_cmac_context),
			.cra_alignmask = 0,
			.cra_module	= THIS_MODULE,
			.cra_init	= tegra_se_aes_cmac_cra_init,
			.cra_exit	= tegra_se_aes_cmac_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.halg.digestsize = SHA1_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "sha1",
			.cra_driver_name = "tegra-se-sha1",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA1_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.halg.digestsize = SHA224_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "sha224",
			.cra_driver_name = "tegra-se-sha224",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA224_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.halg.digestsize = SHA256_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "sha256",
			.cra_driver_name = "tegra-se-sha256",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA256_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.halg.digestsize = SHA384_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "sha384",
			.cra_driver_name = "tegra-se-sha384",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA384_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.halg.digestsize = SHA512_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "sha512",
			.cra_driver_name = "tegra-se-sha512",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA512_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_rsa_init,
		.update = tegra_se_rsa_update,
		.final = tegra_se_rsa_final,
		.finup = tegra_se_rsa_finup,
		.digest = tegra_se_rsa_digest,
		.setkey = tegra_se_rsa_setkey,
		.halg.digestsize = TEGRA_SE_RSA512_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "rsa512",
			.cra_driver_name = "tegra-se-rsa512",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = TEGRA_SE_RSA512_DIGEST_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_aes_cmac_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_rsa_cra_init,
			.cra_exit = tegra_se_rsa_cra_exit,
		}
	}, {
		.init = tegra_se_rsa_init,
		.update = tegra_se_rsa_update,
		.final = tegra_se_rsa_final,
		.finup = tegra_se_rsa_finup,
		.digest = tegra_se_rsa_digest,
		.setkey = tegra_se_rsa_setkey,
		.halg.digestsize = TEGRA_SE_RSA1024_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "rsa1024",
			.cra_driver_name = "tegra-se-rsa1024",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = TEGRA_SE_RSA1024_DIGEST_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_aes_cmac_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_rsa_cra_init,
			.cra_exit = tegra_se_rsa_cra_exit,
		}
	}, {
		.init = tegra_se_rsa_init,
		.update = tegra_se_rsa_update,
		.final = tegra_se_rsa_final,
		.finup = tegra_se_rsa_finup,
		.digest = tegra_se_rsa_digest,
		.setkey = tegra_se_rsa_setkey,
		.halg.digestsize = TEGRA_SE_RSA1536_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "rsa1536",
			.cra_driver_name = "tegra-se-rsa1536",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = TEGRA_SE_RSA1536_DIGEST_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_aes_cmac_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_rsa_cra_init,
			.cra_exit = tegra_se_rsa_cra_exit,
		}
	}, {
		.init = tegra_se_rsa_init,
		.update = tegra_se_rsa_update,
		.final = tegra_se_rsa_final,
		.finup = tegra_se_rsa_finup,
		.digest = tegra_se_rsa_digest,
		.setkey = tegra_se_rsa_setkey,
		.halg.digestsize = TEGRA_SE_RSA2048_DIGEST_SIZE,
		.halg.base = {
			.cra_name = "rsa2048",
			.cra_driver_name = "tegra-se-rsa2048",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = TEGRA_SE_RSA2048_DIGEST_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_aes_cmac_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_rsa_cra_init,
			.cra_exit = tegra_se_rsa_cra_exit,
		}
	}
};

bool isAlgoSupported(struct tegra_se_dev *se_dev, const char *algo)
{
	if (!strcmp(algo, "ansi_cprng")) {
		if (se_dev->chipdata->cprng_supported)
			return true;
		else
			return false;
	}

	if (!strcmp(algo, "drbg")) {
		if (se_dev->chipdata->drbg_supported)
			return true;
		else
			return false;
	}

	if (!strcmp(algo, "rsa512") || !strcmp(algo, "rsa1024") ||
		!strcmp(algo, "rsa1536") || !strcmp(algo, "rsa2048")) {
		if (se_dev->chipdata->rsa_supported)
			return true;
		else
			return false;
	}

	return true;
}

static int tegra_se_probe(struct platform_device *pdev)
{
	struct tegra_se_dev *se_dev = NULL;
	struct resource *res = NULL;
	int err = 0, i = 0, j = 0, k = 0;

	se_dev = kzalloc(sizeof(struct tegra_se_dev), GFP_KERNEL);
	if (!se_dev) {
		dev_err(&pdev->dev, "memory allocation failed\n");
		return -ENOMEM;
	}

	spin_lock_init(&se_dev->lock);
	crypto_init_queue(&se_dev->queue, TEGRA_SE_CRYPTO_QUEUE_LENGTH);
	platform_set_drvdata(pdev, se_dev);
	se_dev->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENXIO;
		dev_err(se_dev->dev, "platform_get_resource failed\n");
		goto fail;
	}

	se_dev->io_reg = ioremap(res->start, resource_size(res));
	if (!se_dev->io_reg) {
		err = -ENOMEM;
		dev_err(se_dev->dev, "ioremap failed\n");
		goto fail;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		err = -ENXIO;
		dev_err(se_dev->dev, "platform_get_resource failed\n");
		goto err_pmc;
	}

	se_dev->pmc_io_reg = ioremap(res->start, resource_size(res));
	if (!se_dev->pmc_io_reg) {
		err = -ENOMEM;
		dev_err(se_dev->dev, "pmc ioremap failed\n");
		goto err_pmc;
	}

	se_dev->irq = platform_get_irq(pdev, 0);
	if (!se_dev->irq) {
		err = -ENODEV;
		dev_err(se_dev->dev, "platform_get_irq failed\n");
		goto err_irq;
	}

	se_dev->chipdata =
		(struct tegra_se_chipdata *)pdev->id_entry->driver_data;

	/* Initialize the clock */
	se_dev->pclk = clk_get(se_dev->dev, "se");
	if (IS_ERR(se_dev->pclk)) {
		dev_err(se_dev->dev, "clock intialization failed (%d)\n",
			(int)se_dev->pclk);
		err = -ENODEV;
		goto clean;
	}

	err = clk_set_rate(se_dev->pclk, ULONG_MAX);
	if (err) {
		dev_err(se_dev->dev, "clock set_rate failed.\n");
		goto clean;
	}

	err = tegra_init_key_slot(se_dev);
	if (err) {
		dev_err(se_dev->dev, "init_key_slot failed\n");
		goto clean;
	}

	err = tegra_init_rsa_key_slot(se_dev);
	if (err) {
		dev_err(se_dev->dev, "init_rsa_key_slot failed\n");
		goto clean;
	}

	init_completion(&se_dev->complete);
	se_work_q = alloc_workqueue("se_work_q", WQ_HIGHPRI | WQ_UNBOUND, 16);
	if (!se_work_q) {
		dev_err(se_dev->dev, "alloc_workqueue failed\n");
		goto clean;
	}

	sg_tegra_se_dev = se_dev;
	pm_runtime_enable(se_dev->dev);
	tegra_se_key_read_disable_all();

	err = request_irq(se_dev->irq, tegra_se_irq, IRQF_DISABLED,
			DRIVER_NAME, se_dev);
	if (err) {
		dev_err(se_dev->dev, "request_irq failed - irq[%d] err[%d]\n",
			se_dev->irq, err);
		goto err_irq;
	}

	err = tegra_se_alloc_ll_buf(se_dev, SE_MAX_SRC_SG_COUNT,
		SE_MAX_DST_SG_COUNT);
	if (err) {
		dev_err(se_dev->dev, "can not allocate ll dma buffer\n");
		goto clean;
	}

	for (i = 0; i < ARRAY_SIZE(aes_algs); i++) {
		if (isAlgoSupported(se_dev, aes_algs[i].cra_name)) {
			INIT_LIST_HEAD(&aes_algs[i].cra_list);
			err = crypto_register_alg(&aes_algs[i]);
			if (err) {
				dev_err(se_dev->dev,
				"crypto_register_alg failed index[%d]\n", i);
				goto clean;
			}
		}
	}

	for (j = 0; j < ARRAY_SIZE(hash_algs); j++) {
		if (isAlgoSupported(se_dev, hash_algs[j].halg.base.cra_name)) {
			err = crypto_register_ahash(&hash_algs[j]);
			if (err) {
				dev_err(se_dev->dev,
				"crypto_register_sha alg failed index[%d]\n",
				i);
				goto clean;
			}
		}
	}

#if defined(CONFIG_PM)
	if (!se_dev->chipdata->drbg_supported)
		se_dev->ctx_save_buf = dma_alloc_coherent(se_dev->dev,
			SE_CONTEXT_BUFER_SIZE, &se_dev->ctx_save_buf_adr,
			GFP_KERNEL);
	else
		se_dev->ctx_save_buf = dma_alloc_coherent(se_dev->dev,
			SE_CONTEXT_DRBG_BUFER_SIZE,
			&se_dev->ctx_save_buf_adr, GFP_KERNEL);

	if (!se_dev->ctx_save_buf) {
		dev_err(se_dev->dev, "Context save buffer alloc filed\n");
		goto clean;
	}
#endif

	dev_info(se_dev->dev, "%s: complete", __func__);
	return 0;

clean:
	pm_runtime_disable(se_dev->dev);
	for (k = 0; k < i; k++)
		crypto_unregister_alg(&aes_algs[k]);

	for (k = 0; k < j; k++)
		crypto_unregister_ahash(&hash_algs[j]);

	tegra_se_free_ll_buf(se_dev);

	if (se_work_q)
		destroy_workqueue(se_work_q);

	if (se_dev->pclk)
		clk_put(se_dev->pclk);

	free_irq(se_dev->irq, &pdev->dev);

err_irq:
	iounmap(se_dev->pmc_io_reg);
err_pmc:
	iounmap(se_dev->io_reg);

fail:
	platform_set_drvdata(pdev, NULL);
	kfree(se_dev);
	sg_tegra_se_dev = NULL;

	return err;
}

static int __devexit tegra_se_remove(struct platform_device *pdev)
{
	struct tegra_se_dev *se_dev = platform_get_drvdata(pdev);
	int i;

	if (!se_dev)
		return -ENODEV;

	pm_runtime_disable(se_dev->dev);

	cancel_work_sync(&se_work);
	if (se_work_q)
		destroy_workqueue(se_work_q);
	free_irq(se_dev->irq, &pdev->dev);
	for (i = 0; i < ARRAY_SIZE(aes_algs); i++)
		crypto_unregister_alg(&aes_algs[i]);
	for (i = 0; i < ARRAY_SIZE(hash_algs); i++)
		crypto_unregister_ahash(&hash_algs[i]);
	if (se_dev->pclk)
		clk_put(se_dev->pclk);
	tegra_se_free_ll_buf(se_dev);
	if (se_dev->ctx_save_buf) {
		if (!se_dev->chipdata->drbg_supported)
			dma_free_coherent(se_dev->dev, SE_CONTEXT_BUFER_SIZE,
				se_dev->ctx_save_buf, se_dev->ctx_save_buf_adr);
		else
			dma_free_coherent(se_dev->dev,
				SE_CONTEXT_DRBG_BUFER_SIZE,
				se_dev->ctx_save_buf, se_dev->ctx_save_buf_adr);
		se_dev->ctx_save_buf = NULL;
	}
	iounmap(se_dev->io_reg);
	iounmap(se_dev->pmc_io_reg);
	kfree(se_dev);
	sg_tegra_se_dev = NULL;

	return 0;
}

#if defined(CONFIG_PM)
static int tegra_se_generate_rng_key(struct tegra_se_dev *se_dev)
{
	int ret = 0;
	u32 val = 0;

	*se_dev->src_ll_buf = 0;
	*se_dev->dst_ll_buf = 0;

	/* Configure algorithm */
	val = SE_CONFIG_ENC_ALG(ALG_RNG) | SE_CONFIG_ENC_MODE(MODE_KEY128) |
		SE_CONFIG_DST(DST_KEYTAB);
	se_writel(se_dev, val, SE_CONFIG_REG_OFFSET);

	/* Configure destination key index number */
	val = SE_CRYPTO_KEYTABLE_DST_KEY_INDEX(srk_slot.slot_num) |
		SE_CRYPTO_KEYTABLE_DST_WORD_QUAD(KEYS_0_3);
	se_writel(se_dev, val, SE_CRYPTO_KEYTABLE_DST_REG_OFFSET);

	/* Configure crypto */
	val = SE_CRYPTO_INPUT_SEL(INPUT_RANDOM) |
		SE_CRYPTO_XOR_POS(XOR_BYPASS) |
		SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) |
		SE_CRYPTO_HASH(HASH_DISABLE) |
		SE_CRYPTO_KEY_INDEX(ssk_slot.slot_num) |
		SE_CRYPTO_IV_SEL(IV_ORIGINAL);
	se_writel(se_dev, val, SE_CRYPTO_REG_OFFSET);

	ret = tegra_se_start_operation(se_dev, TEGRA_SE_KEY_128_SIZE, false);

	return ret;
}

static int tegra_se_generate_srk(struct tegra_se_dev *se_dev)
{
	int ret = 0;
	u32 val = 0;

	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	ret = tegra_se_generate_rng_key(se_dev);
	if (ret) {
		pm_runtime_put(se_dev->dev);
		mutex_unlock(&se_hw_lock);
		return ret;
	}

	*se_dev->src_ll_buf = 0;
	*se_dev->dst_ll_buf = 0;

	val = SE_CONFIG_ENC_ALG(ALG_RNG) | SE_CONFIG_ENC_MODE(MODE_KEY128) |
		SE_CONFIG_DEC_ALG(ALG_NOP) | SE_CONFIG_DST(DST_SRK);

	se_writel(se_dev, val, SE_CONFIG_REG_OFFSET);

	if (!se_dev->chipdata->drbg_supported)
		val = SE_CRYPTO_XOR_POS(XOR_BYPASS) |
				SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) |
				SE_CRYPTO_HASH(HASH_DISABLE) |
				SE_CRYPTO_KEY_INDEX(srk_slot.slot_num) |
				SE_CRYPTO_IV_SEL(IV_UPDATED);
	else
		val = SE_CRYPTO_XOR_POS(XOR_BYPASS) |
				SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) |
				SE_CRYPTO_HASH(HASH_DISABLE) |
				SE_CRYPTO_KEY_INDEX(srk_slot.slot_num) |
				SE_CRYPTO_IV_SEL(IV_UPDATED)|
				SE_CRYPTO_INPUT_SEL(INPUT_RANDOM);
	se_writel(se_dev, val, SE_CRYPTO_REG_OFFSET);

	if (se_dev->chipdata->drbg_supported) {
		se_writel(se_dev, SE_RNG_CONFIG_MODE(DRBG_MODE_FORCE_RESEED)|
			SE_RNG_CONFIG_SRC(DRBG_SRC_ENTROPY),
			SE_RNG_CONFIG_REG_OFFSET);
		se_writel(se_dev, RNG_RESEED_INTERVAL,
			SE_RNG_RESEED_INTERVAL_REG_OFFSET);
	}
	ret = tegra_se_start_operation(se_dev, TEGRA_SE_KEY_128_SIZE, false);

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return ret;
}

static int tegra_se_lp_generate_random_data(struct tegra_se_dev *se_dev)
{
	struct tegra_se_ll *src_ll, *dst_ll;
	int ret = 0;
	u32 val;

	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->src_ll_buf = 0;
	*se_dev->dst_ll_buf = 0;
	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);
	src_ll->addr = se_dev->ctx_save_buf_adr;
	src_ll->data_len = SE_CONTEXT_SAVE_RANDOM_DATA_SIZE;
	dst_ll->addr = se_dev->ctx_save_buf_adr;
	dst_ll->data_len = SE_CONTEXT_SAVE_RANDOM_DATA_SIZE;

	tegra_se_config_algo(se_dev, SE_AES_OP_MODE_RNG_X931, true,
		TEGRA_SE_KEY_128_SIZE);

	/* Configure crypto */
	val = SE_CRYPTO_INPUT_SEL(INPUT_RANDOM) |
		SE_CRYPTO_XOR_POS(XOR_BYPASS) |
		SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) |
		SE_CRYPTO_HASH(HASH_DISABLE) |
		SE_CRYPTO_KEY_INDEX(srk_slot.slot_num) |
		SE_CRYPTO_IV_SEL(IV_ORIGINAL);

	se_writel(se_dev, val, SE_CRYPTO_REG_OFFSET);
	if (se_dev->chipdata->drbg_supported)
		se_writel(se_dev, SE_RNG_CONFIG_MODE(DRBG_MODE_FORCE_RESEED)|
			SE_RNG_CONFIG_SRC(DRBG_SRC_ENTROPY),
			SE_RNG_CONFIG_REG_OFFSET);

	ret = tegra_se_start_operation(se_dev,
		SE_CONTEXT_SAVE_RANDOM_DATA_SIZE, false);

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return ret;

}

static int tegra_se_lp_encrypt_context_data(struct tegra_se_dev *se_dev,
	u32 context_offset, u32 data_size)
{
	struct tegra_se_ll *src_ll, *dst_ll;
	int ret = 0;

	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->src_ll_buf = 0;
	*se_dev->dst_ll_buf = 0;
	src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf + 1);
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);
	src_ll->addr = se_dev->ctx_save_buf_adr + context_offset;
	src_ll->data_len = data_size;
	dst_ll->addr = se_dev->ctx_save_buf_adr + context_offset;
	dst_ll->data_len = data_size;

	se_writel(se_dev, SE_CONTEXT_SAVE_SRC(MEM),
		SE_CONTEXT_SAVE_CONFIG_REG_OFFSET);

	ret = tegra_se_start_operation(se_dev, data_size, true);

	pm_runtime_put(se_dev->dev);

	mutex_unlock(&se_hw_lock);

	return ret;
}

static int tegra_se_lp_sticky_bits_context_save(struct tegra_se_dev *se_dev)
{
	struct tegra_se_ll *dst_ll;
	int ret = 0;
	u32 val = 0;
	int i = 0;

	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->src_ll_buf = 0;
	*se_dev->dst_ll_buf = 0;
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);
	dst_ll->addr = (se_dev->ctx_save_buf_adr +
		SE_CONTEXT_SAVE_STICKY_BITS_OFFSET);
	dst_ll->data_len = SE_CONTEXT_SAVE_STICKY_BITS_SIZE;

	if (!se_dev->chipdata->drbg_supported) {
		se_writel(se_dev, SE_CONTEXT_SAVE_SRC(STICKY_BITS),
			SE_CONTEXT_SAVE_CONFIG_REG_OFFSET);
		ret = tegra_se_start_operation(se_dev,
			SE_CONTEXT_SAVE_STICKY_BITS_SIZE, true);
	} else
		for (i = 0; i < 2; i++) {
			val = SE_CONTEXT_SAVE_SRC(STICKY_BITS) |
				SE_CONTEXT_SAVE_STICKY_WORD_QUAD(i);
			se_writel(se_dev, val,
				SE_CONTEXT_SAVE_CONFIG_REG_OFFSET);
			ret = tegra_se_start_operation(se_dev,
				SE_CONTEXT_SAVE_STICKY_BITS_SIZE, true);
			dst_ll->addr += SE_CONTEXT_SAVE_STICKY_BITS_SIZE;
		}

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return ret;
}

static int tegra_se_lp_keytable_context_save(struct tegra_se_dev *se_dev)
{
	struct tegra_se_ll *dst_ll;
	int ret = 0, i, j;
	u32 val = 0;

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->dst_ll_buf = 0;
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);
	if (!se_dev->chipdata->drbg_supported)
		dst_ll->addr =
			(se_dev->ctx_save_buf_adr +
			SE_CONTEXT_SAVE_KEYS_OFFSET);
	else
		dst_ll->addr =
			(se_dev->ctx_save_buf_adr +
			SE11_CONTEXT_SAVE_KEYS_OFFSET);

	dst_ll->data_len = TEGRA_SE_KEY_128_SIZE;

	for (i = 0; i < TEGRA_SE_KEYSLOT_COUNT; i++) {
		for (j = 0; j < 2; j++) {
			val = SE_CONTEXT_SAVE_SRC(KEYTABLE) |
				SE_CONTEXT_SAVE_KEY_INDEX(i) |
				SE_CONTEXT_SAVE_WORD_QUAD(j);
			se_writel(se_dev,
				val, SE_CONTEXT_SAVE_CONFIG_REG_OFFSET);
			ret = tegra_se_start_operation(se_dev,
				TEGRA_SE_KEY_128_SIZE, true);
			if (ret)
				break;
			dst_ll->addr += TEGRA_SE_KEY_128_SIZE;
		}
	}

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return ret;
}

static int tegra_se_lp_rsakeytable_context_save(struct tegra_se_dev *se_dev)
{
	struct tegra_se_ll *dst_ll;
	int ret = 0, i, j;
	u32 val = 0;

	/* take access to the hw */
	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->dst_ll_buf = 0;
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);
	dst_ll->addr =
		(se_dev->ctx_save_buf_adr + SE_CONTEXT_SAVE_RSA_KEYS_OFFSET);
	dst_ll->data_len = TEGRA_SE_KEY_128_SIZE;

	for (i = 0; i < TEGRA_SE_RSA_CONTEXT_SAVE_KEYSLOT_COUNT; i++) {
		for (j = 0; j < 16; j++) {
			val = SE_CONTEXT_SAVE_SRC(RSA_KEYTABLE) |
				SE_CONTEXT_SAVE_RSA_KEY_INDEX(i) |
				SE_CONTEXT_RSA_WORD_QUAD(j);
			se_writel(se_dev,
				val, SE_CONTEXT_SAVE_CONFIG_REG_OFFSET);
			ret = tegra_se_start_operation(se_dev,
				TEGRA_SE_KEY_128_SIZE, true);
			if (ret) {
				dev_err(se_dev->dev, "tegra_se_lp_rsakeytable_context_save error\n");
				break;
			}
			dst_ll->addr += TEGRA_SE_KEY_128_SIZE;
		}
	}

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return ret;
}

static int tegra_se_lp_iv_context_save(struct tegra_se_dev *se_dev,
	bool org_iv, u32 context_offset)
{
	struct tegra_se_ll *dst_ll;
	int ret = 0, i;
	u32 val = 0;

	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	*se_dev->dst_ll_buf = 0;
	dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf + 1);
	dst_ll->addr = (se_dev->ctx_save_buf_adr + context_offset);
	dst_ll->data_len = TEGRA_SE_AES_IV_SIZE;

	for (i = 0; i < TEGRA_SE_KEYSLOT_COUNT; i++) {
		val = SE_CONTEXT_SAVE_SRC(KEYTABLE) |
			SE_CONTEXT_SAVE_KEY_INDEX(i) |
			(org_iv ? SE_CONTEXT_SAVE_WORD_QUAD(ORIG_IV) :
			SE_CONTEXT_SAVE_WORD_QUAD(UPD_IV));
		se_writel(se_dev, val, SE_CONTEXT_SAVE_CONFIG_REG_OFFSET);
		ret = tegra_se_start_operation(se_dev,
			TEGRA_SE_AES_IV_SIZE, true);
		if (ret)
			break;
		dst_ll->addr += TEGRA_SE_AES_IV_SIZE;
	}

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return ret;
}

static int tegra_se_save_SRK(struct tegra_se_dev *se_dev)
{
	int ret;
	int val;

	mutex_lock(&se_hw_lock);
	pm_runtime_get_sync(se_dev->dev);

	se_writel(se_dev, SE_CONTEXT_SAVE_SRC(SRK),
		SE_CONTEXT_SAVE_CONFIG_REG_OFFSET);
	ret = tegra_se_start_operation(se_dev, 0, true);

	if (ret < 0) {
		dev_err(se_dev->dev, "\n LP SRK operation failed\n");
		pm_runtime_put(se_dev->dev);
		mutex_unlock(&se_hw_lock);
		return ret;
	}

	if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA11) &&
				se_dev->chipdata->drbg_supported) {
		/* clear any pending interrupts */
		val = se_readl(se_dev, SE_INT_STATUS_REG_OFFSET);
		se_writel(se_dev, val, SE_INT_STATUS_REG_OFFSET);

		/* enable interupts */
		val = SE_INT_ERROR(INT_ENABLE) | SE_INT_OP_DONE(INT_ENABLE);
		se_writel(se_dev, val, SE_INT_ENABLE_REG_OFFSET);

		val = SE_CONFIG_ENC_ALG(ALG_NOP) |
				SE_CONFIG_DEC_ALG(ALG_NOP);
		se_writel(se_dev, val, SE_CRYPTO_REG_OFFSET);

		INIT_COMPLETION(se_dev->complete);

		se_writel(se_dev, SE_OPERATION(OP_CTX_SAVE),
			SE_OPERATION_REG_OFFSET);
		ret = wait_for_completion_timeout(&se_dev->complete,
				msecs_to_jiffies(1000));

		if (ret == 0) {
			dev_err(se_dev->dev, "\n LP SRK timed out no interrupt\n");
			pm_runtime_put(se_dev->dev);
			mutex_unlock(&se_hw_lock);
			return -ETIMEDOUT;
		}
	}

	pm_runtime_put(se_dev->dev);
	mutex_unlock(&se_hw_lock);

	return 0;
}

static int tegra_se_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_se_dev *se_dev = platform_get_drvdata(pdev);
	int err = 0, i;
	unsigned char *dt_buf = NULL;
	u8 pdata[SE_CONTEXT_KNOWN_PATTERN_SIZE] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

	if (!se_dev)
		return -ENODEV;

	/* Generate SRK */
	err = tegra_se_generate_srk(se_dev);
	if (err) {
		dev_err(se_dev->dev, "\n LP SRK genration failed\n");
		goto out;
	}

	/* Generate random data*/
	err = tegra_se_lp_generate_random_data(se_dev);
	if (err) {
		dev_err(se_dev->dev, "\n LP random pattern generation failed\n");
		goto out;
	}

	/* Encrypt random data */
	err = tegra_se_lp_encrypt_context_data(se_dev,
		SE_CONTEXT_SAVE_RANDOM_DATA_OFFSET,
		SE_CONTEXT_SAVE_RANDOM_DATA_SIZE);
	if (err) {
		dev_err(se_dev->dev, "\n LP random pattern encryption failed\n");
		goto out;
	}

	/* Sticky bits context save*/
	err = tegra_se_lp_sticky_bits_context_save(se_dev);
	if (err) {
		dev_err(se_dev->dev, "\n LP sticky bits context save failure\n");
		goto out;
	}

	/* Key table context save*/
	err = tegra_se_lp_keytable_context_save(se_dev);
	if (err) {
		dev_err(se_dev->dev, "\n LP key table  save failure\n");
		goto out;
	}

	/* Original iv context save*/
	err = tegra_se_lp_iv_context_save(se_dev,
		true,
		(se_dev->chipdata->drbg_supported ?
			SE11_CONTEXT_ORIGINAL_IV_OFFSET :
			SE_CONTEXT_ORIGINAL_IV_OFFSET));
	if (err) {
		dev_err(se_dev->dev, "\n LP original iv save failure\n");
		goto out;
	}

	/* Updated iv context save*/
	err = tegra_se_lp_iv_context_save(se_dev,
		false,
		(se_dev->chipdata->drbg_supported ?
		SE11_CONTEXT_UPDATED_IV_OFFSET :
		SE_CONTEXT_UPDATED_IV_OFFSET));
	if (err) {
		dev_err(se_dev->dev, "\n LP updated iv save failure\n");
		goto out;
	}

	if (se_dev->chipdata->drbg_supported) {
		/* rsa-key slot table context save*/
		err = tegra_se_lp_rsakeytable_context_save(se_dev);
		if (err) {
			dev_err(se_dev->dev, "\n LP RSA key table save failure\n");
			goto out;
		}
		/* Encrypt known pattern */
		dt_buf = (unsigned char *)se_dev->ctx_save_buf;
		dt_buf +=
			SE_CONTEXT_SAVE_RSA_KNOWN_PATTERN_OFFSET;
		for (i = 0; i < SE_CONTEXT_KNOWN_PATTERN_SIZE; i++)
			dt_buf[i] = pdata[i];
		err = tegra_se_lp_encrypt_context_data(se_dev,
			SE_CONTEXT_SAVE_RSA_KNOWN_PATTERN_OFFSET,
			SE_CONTEXT_KNOWN_PATTERN_SIZE);
	} else {
		/* Encrypt known pattern */
		dt_buf = (unsigned char *)se_dev->ctx_save_buf;
		dt_buf +=
			SE_CONTEXT_SAVE_KNOWN_PATTERN_OFFSET;
		for (i = 0; i < SE_CONTEXT_KNOWN_PATTERN_SIZE; i++)
			dt_buf[i] = pdata[i];
		err = tegra_se_lp_encrypt_context_data(se_dev,
			SE_CONTEXT_SAVE_KNOWN_PATTERN_OFFSET,
			SE_CONTEXT_KNOWN_PATTERN_SIZE);
	}
	if (err) {
		dev_err(se_dev->dev, "LP known pattern save failure\n");
		goto out;
	}

	/* Write lp context buffer address into PMC scratch register */
	writel(page_to_phys(vmalloc_to_page(se_dev->ctx_save_buf)),
		se_dev->pmc_io_reg + PMC_SCRATCH43_REG_OFFSET);

	/* Saves SRK in secure scratch */
	err = tegra_se_save_SRK(se_dev);
	if (err < 0) {
		dev_err(se_dev->dev, "LP SRK save failure\n");
		goto out;
	}

out:
	/* put the device into runtime suspend state - disable clock */
	pm_runtime_put_sync(dev);
	return err;
}

static int tegra_se_resume(struct device *dev)
{
	/* pair with tegra_se_suspend, no need to actually enable clock */
	pm_runtime_get_noresume(dev);
	return 0;
}
#endif

#if defined(CONFIG_PM_RUNTIME)
static int tegra_se_runtime_suspend(struct device *dev)
{
	/*
	 * do a dummy read, to avoid scenarios where you have unposted writes
	 * still on the bus, before disabling clocks
	 */
	se_readl(sg_tegra_se_dev, SE_CONFIG_REG_OFFSET);

	clk_disable_unprepare(sg_tegra_se_dev->pclk);
	return 0;
}

static int tegra_se_runtime_resume(struct device *dev)
{
	clk_prepare_enable(sg_tegra_se_dev->pclk);
	return 0;
}

static const struct dev_pm_ops tegra_se_dev_pm_ops = {
	.runtime_suspend = tegra_se_runtime_suspend,
	.runtime_resume = tegra_se_runtime_resume,
#if defined(CONFIG_PM)
	.suspend = tegra_se_suspend,
	.resume = tegra_se_resume,
#endif
};
#endif

static struct tegra_se_chipdata tegra_se_chipdata = {
	.rsa_supported = false,
	.cprng_supported = true,
	.drbg_supported = false,
	.aes_freq = 300000000,
	.rng_freq = 300000000,
	.sha1_freq = 300000000,
	.sha224_freq = 300000000,
	.sha256_freq = 300000000,
	.sha384_freq = 300000000,
	.sha512_freq = 300000000,
};

static struct tegra_se_chipdata tegra11_se_chipdata = {
	.rsa_supported = true,
	.cprng_supported = false,
	.drbg_supported = true,
	.aes_freq = 150000000,
	.rng_freq = 150000000,
	.sha1_freq = 200000000,
	.sha224_freq = 250000000,
	.sha256_freq = 250000000,
	.sha384_freq = 150000000,
	.sha512_freq = 150000000,
	.rsa_freq = 350000000,

};

static struct platform_device_id tegra_dev_se_devtype[] = {
	{
		.name = "tegra-se",
		.driver_data = (unsigned long)&tegra_se_chipdata,
	},
	{
			.name = "tegra11-se",
			.driver_data = (unsigned long)&tegra11_se_chipdata,
	}
};

static struct platform_driver tegra_se_driver = {
	.probe  = tegra_se_probe,
	.remove = __devexit_p(tegra_se_remove),
	.id_table = tegra_dev_se_devtype,
	.driver = {
		.name   = "tegra-se",
		.owner  = THIS_MODULE,
#if defined(CONFIG_PM_RUNTIME)
		.pm = &tegra_se_dev_pm_ops,
#endif
	},
};

static int __init tegra_se_module_init(void)
{
	return  platform_driver_register(&tegra_se_driver);
}

static void __exit tegra_se_module_exit(void)
{
	platform_driver_unregister(&tegra_se_driver);
}

module_init(tegra_se_module_init);
module_exit(tegra_se_module_exit);

MODULE_DESCRIPTION("Tegra Crypto algorithm support");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL");
MODULE_ALIAS("tegra-se");

