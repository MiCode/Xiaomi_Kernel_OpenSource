/*
 * drivers/crypto/tegra-aes.c
 *
 * Driver for NVIDIA Tegra AES hardware engines residing inside the
 * Bit Stream Engine for Video (BSEV) and Bit Stream Engine for Audio
 * (BSEA) hardware blocks.
 *
 * The programming sequence for these engines is with the help
 * of commands which travel via a command queue residing between the
 * CPU and the BSEV/A block. The BSEV engine has an internal RAM (VRAM)
 * where the final input plaintext, keys and the IV have to be copied
 * before starting the encrypt/decrypt operation. The BSEA engine operates
 * with the help of IRAM.
 *
 * Copyright (c) 2010, NVIDIA Corporation.
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
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <mach/arb_sema.h>
#include <mach/clk.h>
#include "../video/tegra/nvmap/nvmap.h"

#include <crypto/scatterwalk.h>
#include <crypto/aes.h>
#include <crypto/internal/rng.h>

#include "tegra-aes.h"

#define FLAGS_MODE_MASK			0x00FF
#define FLAGS_ENCRYPT			BIT(0)
#define FLAGS_CBC			BIT(1)
#define FLAGS_GIV			BIT(2)
#define FLAGS_RNG			BIT(3)
#define FLAGS_OFB			BIT(4)
#define FLAGS_INIT			BIT(5)
#define FLAGS_BUSY			1

/*
 * Defines AES engine Max process bytes size in one go, which takes 1 msec.
 * AES engine spends about 176 cycles/16-bytes or 11 cycles/byte
 * The duration CPU can use the BSE to 1 msec, then the number of available
 * cycles of AVP/BSE is 216K. In this duration, AES can process 216/11 ~= 19KB
 * Based on this AES_HW_DMA_BUFFER_SIZE_BYTES is configured to 16KB.
 */
#define AES_HW_DMA_BUFFER_SIZE_BYTES	0x4000

/*
 * The key table length is 64 bytes
 * (This includes first upto 32 bytes key + 16 bytes original initial vector
 * and 16 bytes updated initial vector)
 */
#define AES_HW_KEY_TABLE_LENGTH_BYTES	64

#define AES_HW_IV_SIZE	16
#define AES_HW_KEYSCHEDULE_LEN	256
#define ARB_SEMA_TIMEOUT	500

/*
 * The memory being used is divides as follows:
 * 1. Key - 32 bytes
 * 2. Original IV - 16 bytes
 * 3. Updated IV - 16 bytes
 * 4. Key schedule - 256 bytes
 *
 * 1+2+3 constitute the hw key table.
 */
#define AES_IVKEY_SIZE (AES_HW_KEY_TABLE_LENGTH_BYTES + AES_HW_KEYSCHEDULE_LEN)

#define DEFAULT_RNG_BLK_SZ	16

/* As of now only 4 commands are USED for AES encryption/Decryption */
#define AES_HW_MAX_ICQ_LENGTH	4

#define ICQBITSHIFT_BLKCNT	0

/* memdma_vd command */
#define MEMDMA_DIR_DTOVRAM		0 /* sdram -> vram */
#define MEMDMA_DIR_VTODRAM		1 /* vram -> sdram */
#define MEMDMA_DIR_SHIFT		25
#define MEMDMA_NUM_WORDS_SHIFT		12

/* command queue bit shifts */
enum {
	CMDQ_KEYTABLEADDR_SHIFT = 0,
	CMDQ_KEYTABLEID_SHIFT = 17,
	CMDQ_VRAMSEL_SHIFT = 23,
	CMDQ_TABLESEL_SHIFT = 24,
	CMDQ_OPCODE_SHIFT = 26,
};

/* Define commands required for AES operation */
enum {
	CMD_BLKSTARTENGINE = 0x0E,
	CMD_DMASETUP = 0x10,
	CMD_DMACOMPLETE = 0x11,
	CMD_SETTABLE = 0x15,
	CMD_MEMDMAVD = 0x22,
};

/* Define sub-commands */
enum {
	SUBCMD_VRAM_SEL = 0x1,
	SUBCMD_CRYPTO_TABLE_SEL = 0x3,
	SUBCMD_KEY_TABLE_SEL = 0x8,
};

#define UCQCMD_KEYTABLEADDRMASK 0x1FFFF

#define AES_NR_KEYSLOTS	8
#define SSK_SLOT_NUM	4

struct tegra_aes_slot {
	struct list_head node;
	int slot_num;
	bool available;
};

struct tegra_aes_reqctx {
	unsigned long mode;
};

#define TEGRA_AES_QUEUE_LENGTH	500

struct tegra_aes_engine {
	struct tegra_aes_dev *dd;
	struct tegra_aes_ctx *ctx;
	struct clk *iclk;
	struct clk *pclk;
	struct ablkcipher_request *req;
	struct scatterlist *in_sg;
	struct completion op_complete;
	struct scatterlist *out_sg;
	void __iomem *io_base;
	void __iomem *ivkey_base;
	unsigned long phys_base;
	unsigned long iram_phys;
	void *iram_virt;
	dma_addr_t ivkey_phys_base;
	dma_addr_t dma_buf_in;
	dma_addr_t dma_buf_out;
	size_t total;
	size_t in_offset;
	size_t out_offset;
	u32 engine_offset;
	u32 *buf_in;
	u32 *buf_out;
	int res_id;
	unsigned long busy;
	u8 irq;
	u32 status;
};

struct tegra_aes_dev {
	struct device *dev;
	struct tegra_aes_slot *slots;
	struct tegra_aes_engine bsev;
	struct tegra_aes_engine bsea;
	struct nvmap_client *client;
	struct nvmap_handle_ref *h_ref;
	struct crypto_queue queue;
	spinlock_t lock;
	u64 ctr;
	unsigned long flags;
	u8 dt[DEFAULT_RNG_BLK_SZ];
};

static struct tegra_aes_dev *aes_dev;

struct tegra_aes_ctx {
	struct tegra_aes_dev *dd;
	struct tegra_aes_engine *eng;
	struct tegra_aes_slot *slot;
	int key[AES_MAX_KEY_SIZE];
	int keylen;
	bool use_ssk;
	u8 dt[DEFAULT_RNG_BLK_SZ];
};

static struct tegra_aes_ctx rng_ctx;

/* keep registered devices data here */
static struct list_head dev_list;
static DEFINE_SPINLOCK(list_lock);

/* Engine specific work queues */
static void bsev_workqueue_handler(struct work_struct *work);
static void bsea_workqueue_handler(struct work_struct *work);

static DECLARE_WORK(bsev_work, bsev_workqueue_handler);
static DECLARE_WORK(bsea_work, bsea_workqueue_handler);

static struct workqueue_struct *bsev_wq;
static struct workqueue_struct *bsea_wq;

extern unsigned long long tegra_chip_uid(void);

static inline u32 aes_readl(struct tegra_aes_engine *engine, u32 offset)
{
	return readl(engine->io_base + offset);
}

static inline void aes_writel(struct tegra_aes_engine *engine,
	u32 val, u32 offset)
{
	writel(val, engine->io_base + offset);
}

static int alloc_iram(struct tegra_aes_dev *dd)
{
	size_t size;
	int err;

	dd->h_ref = NULL;

	/* [key+iv+u-iv=64B] * 8 = 512Bytes */
	size = AES_MAX_KEY_SIZE;
	dd->client = nvmap_create_client(nvmap_dev, "aes_bsea");
	if (IS_ERR(dd->client)) {
		dev_err(dd->dev, "nvmap_create_client failed\n");
		goto out;
	}

	dd->h_ref = nvmap_create_handle(dd->client, size);
	if (IS_ERR(dd->h_ref)) {
		dev_err(dd->dev, "nvmap_create_handle failed\n");
		goto out;
	}

	/* Allocate memory in the iram */
	err = nvmap_alloc_handle_id(dd->client, nvmap_ref_to_id(dd->h_ref),
		NVMAP_HEAP_CARVEOUT_IRAM, size, 0);
	if (err) {
		dev_err(dd->dev, "nvmap_alloc_handle_id failed\n");
		nvmap_free_handle_id(dd->client, nvmap_ref_to_id(dd->h_ref));
		goto out;
	}
	dd->bsea.iram_phys = nvmap_handle_address(dd->client,
					nvmap_ref_to_id(dd->h_ref));

	dd->bsea.iram_virt = nvmap_mmap(dd->h_ref);	/* get virtual address */
	if (!dd->bsea.iram_virt) {
		dev_err(dd->dev, "%s: no mem, BSEA IRAM alloc failure\n",
			__func__);
		goto out;
	}

	memset(dd->bsea.iram_virt, 0, size);
	return 0;

out:
	if (dd->bsea.iram_virt)
		nvmap_munmap(dd->h_ref, dd->bsea.iram_virt);

	if (dd->client) {
		nvmap_free_handle_id(dd->client, nvmap_ref_to_id(dd->h_ref));
		nvmap_client_put(dd->client);
	}

	return -ENOMEM;
}

static void free_iram(struct tegra_aes_dev *dd)
{
	if (dd->bsea.iram_virt)
		nvmap_munmap(dd->h_ref, dd->bsea.iram_virt);

	if (dd->client) {
		nvmap_free_handle_id(dd->client, nvmap_ref_to_id(dd->h_ref));
		nvmap_client_put(dd->client);
	}
}

static int aes_hw_init(struct tegra_aes_engine *engine)
{
	struct tegra_aes_dev *dd = aes_dev;
	int ret = 0;

	if (engine->pclk) {
		ret = clk_prepare_enable(engine->pclk);
		if (ret < 0) {
			dev_err(dd->dev, "%s: pclock enable fail(%d)\n",
			__func__, ret);
			return ret;
		}
	}

	if (engine->iclk) {
		ret = clk_prepare_enable(engine->iclk);
		if (ret < 0) {
			dev_err(dd->dev, "%s: iclock enable fail(%d)\n",
			__func__, ret);
			if (engine->pclk)
				clk_disable_unprepare(engine->pclk);
			return ret;
		}
	}

	return ret;
}

static void aes_hw_deinit(struct tegra_aes_engine *engine)
{
	if (engine->pclk)
		clk_disable_unprepare(engine->pclk);

	if (engine->iclk)
		clk_disable_unprepare(engine->iclk);
}

#define MIN_RETRIES	3
static int aes_start_crypt(struct tegra_aes_engine *eng, u32 in_addr,
	u32 out_addr, int nblocks, int mode, bool upd_iv)
{
	u32 cmdq[AES_HW_MAX_ICQ_LENGTH];
	int qlen = 0, i, eng_busy, icq_empty, ret;
	u32 value;
	int retries = MIN_RETRIES;

start:
	do {
		value = aes_readl(eng, TEGRA_AES_INTR_STATUS);
		eng_busy = value & BIT(0);
		icq_empty = value & BIT(3);
	} while (eng_busy || (!icq_empty));

	aes_writel(eng, 0xFFFFFFFF, TEGRA_AES_INTR_STATUS);

	/* enable error, dma xfer complete interrupts */
	aes_writel(eng, 0x33, TEGRA_AES_INT_ENB);

	cmdq[0] = CMD_DMASETUP << CMDQ_OPCODE_SHIFT;
	cmdq[1] = in_addr;
	cmdq[2] = CMD_BLKSTARTENGINE << CMDQ_OPCODE_SHIFT | (nblocks-1);
	cmdq[3] = CMD_DMACOMPLETE << CMDQ_OPCODE_SHIFT;

	value = aes_readl(eng, TEGRA_AES_CMDQUE_CONTROL);
	/* access SDRAM through AHB */
	value &= ~TEGRA_AES_CMDQ_CTRL_SRC_STM_SEL_FIELD;
	value &= ~TEGRA_AES_CMDQ_CTRL_DST_STM_SEL_FIELD;
	value |= TEGRA_AES_CMDQ_CTRL_SRC_STM_SEL_FIELD |
		 TEGRA_AES_CMDQ_CTRL_DST_STM_SEL_FIELD |
		 TEGRA_AES_CMDQ_CTRL_ICMDQEN_FIELD;
	aes_writel(eng, value, TEGRA_AES_CMDQUE_CONTROL);
	dev_dbg(aes_dev->dev, "cmd_q_ctrl=0x%x", value);

	value = (0x1 << TEGRA_AES_SECURE_INPUT_ALG_SEL_SHIFT) |
		((eng->ctx->keylen * 8) <<
			TEGRA_AES_SECURE_INPUT_KEY_LEN_SHIFT) |
		((u32)upd_iv << TEGRA_AES_SECURE_IV_SELECT_SHIFT);

	if (mode & FLAGS_CBC) {
		value |= ((((mode & FLAGS_ENCRYPT) ? 2 : 3)
				<< TEGRA_AES_SECURE_XOR_POS_SHIFT) |
			(((mode & FLAGS_ENCRYPT) ? 2 : 3)
				<< TEGRA_AES_SECURE_VCTRAM_SEL_SHIFT) |
			((mode & FLAGS_ENCRYPT) ? 1 : 0)
				<< TEGRA_AES_SECURE_CORE_SEL_SHIFT);
	} else if (mode & FLAGS_OFB) {
		value |= ((TEGRA_AES_SECURE_XOR_POS_FIELD) |
			(2 << TEGRA_AES_SECURE_INPUT_SEL_SHIFT) |
			(TEGRA_AES_SECURE_CORE_SEL_FIELD));
	} else if (mode & FLAGS_RNG) {
		value |= (((mode & FLAGS_ENCRYPT) ? 1 : 0)
				<< TEGRA_AES_SECURE_CORE_SEL_SHIFT |
			  TEGRA_AES_SECURE_RNG_ENB_FIELD);
	} else {
		value |= (((mode & FLAGS_ENCRYPT) ? 1 : 0)
				<< TEGRA_AES_SECURE_CORE_SEL_SHIFT);
	}

	dev_dbg(aes_dev->dev, "secure_in_sel=0x%x", value);
	aes_writel(eng, value, TEGRA_AES_SECURE_INPUT_SELECT);

	aes_writel(eng, out_addr, TEGRA_AES_SECURE_DEST_ADDR);
	INIT_COMPLETION(eng->op_complete);

	for (i = 0; i < 3; i++) {
		do {
			value = aes_readl(eng, TEGRA_AES_INTR_STATUS);
			eng_busy = value & TEGRA_AES_ENGINE_BUSY_FIELD;
			icq_empty = value & TEGRA_AES_ICQ_EMPTY_FIELD;
		} while (eng_busy || (!icq_empty));
		aes_writel(eng, cmdq[i], TEGRA_AES_ICMDQUE_WR);
	}

	ret = wait_for_completion_timeout(&eng->op_complete,
		msecs_to_jiffies(150));
	if (ret == 0) {
		dev_err(aes_dev->dev, "engine%d timed out (0x%x)\n",
			eng->res_id, aes_readl(eng, TEGRA_AES_INTR_STATUS));
		return -ETIMEDOUT;
	}

	aes_writel(eng, cmdq[3], TEGRA_AES_ICMDQUE_WR);

	if ((eng->status != 0) && (retries-- > 0)) {
		qlen = 0;
		goto start;
	}

	return 0;
}

static void aes_release_key_slot(struct tegra_aes_slot *slot)
{
	if (slot->slot_num == SSK_SLOT_NUM)
		return;

	spin_lock(&list_lock);
	slot->available = true;
	slot = NULL;
	spin_unlock(&list_lock);
}

static struct tegra_aes_slot *aes_find_key_slot(void)
{
	struct tegra_aes_slot *slot;
	int found = 0;

	spin_lock(&list_lock);
	list_for_each_entry(slot, &dev_list, node) {
		if (slot->available) {
			slot->available = false;
			found = 1;
			break;
		}
	}
	spin_unlock(&list_lock);

	return found ? slot : NULL;
}

static int aes_set_key(struct tegra_aes_engine *eng, int slot_num)
{
	struct tegra_aes_dev *dd = aes_dev;
	u32 value, cmdq[2];
	int eng_busy, icq_empty, dma_busy;

	if (!eng) {
		dev_err(dd->dev, "%s: context invalid\n", __func__);
		return -EINVAL;
	}

	/* enable key schedule generation in hardware */
	value = aes_readl(eng, TEGRA_AES_SECURE_CONFIG_EXT);
	value &= ~TEGRA_AES_SECURE_KEY_SCH_DIS_FIELD;
	aes_writel(eng, value, TEGRA_AES_SECURE_CONFIG_EXT);

	/* select the key slot */
	value = aes_readl(eng, TEGRA_AES_SECURE_CONFIG);
	value &= ~TEGRA_AES_SECURE_KEY_INDEX_FIELD;
	value |= (slot_num << TEGRA_AES_SECURE_KEY_INDEX_SHIFT);
	aes_writel(eng, value, TEGRA_AES_SECURE_CONFIG);

	if (slot_num == SSK_SLOT_NUM)
		goto out;

	if (eng->res_id == TEGRA_ARB_BSEV) {
		memset(dd->bsev.ivkey_base, 0, AES_HW_KEY_TABLE_LENGTH_BYTES);
		memcpy(dd->bsev.ivkey_base, eng->ctx->key, eng->ctx->keylen);

		/* sync the buffer for device */
		dma_sync_single_for_device(dd->dev, dd->bsev.ivkey_phys_base,
				eng->ctx->keylen, DMA_TO_DEVICE);

		/* copy the key table from sdram to vram */
		cmdq[0] = CMD_MEMDMAVD << CMDQ_OPCODE_SHIFT |
			MEMDMA_DIR_DTOVRAM << MEMDMA_DIR_SHIFT |
			AES_HW_KEY_TABLE_LENGTH_BYTES / sizeof(u32) <<
				MEMDMA_NUM_WORDS_SHIFT;
		cmdq[1] = (u32)eng->ivkey_phys_base;

		aes_writel(eng, cmdq[0], TEGRA_AES_ICMDQUE_WR);
		aes_writel(eng, cmdq[1], TEGRA_AES_ICMDQUE_WR);

		do {
			value = aes_readl(eng, TEGRA_AES_INTR_STATUS);
			eng_busy = value & TEGRA_AES_ENGINE_BUSY_FIELD;
			icq_empty = value & TEGRA_AES_ICQ_EMPTY_FIELD;
			dma_busy = value & TEGRA_AES_DMA_BUSY_FIELD;
		} while (eng_busy & (!icq_empty) & dma_busy);

		/* settable command to get key into internal registers */
		value = CMD_SETTABLE << CMDQ_OPCODE_SHIFT |
			SUBCMD_CRYPTO_TABLE_SEL << CMDQ_TABLESEL_SHIFT |
			SUBCMD_VRAM_SEL << CMDQ_VRAMSEL_SHIFT |
			(SUBCMD_KEY_TABLE_SEL | slot_num)
				<< CMDQ_KEYTABLEID_SHIFT;
	} else {
		memset(dd->bsea.iram_virt, 0, AES_HW_KEY_TABLE_LENGTH_BYTES);
		memcpy(dd->bsea.iram_virt, eng->ctx->key, eng->ctx->keylen);

		/* set iram access cfg bit 0 if address >128K */
		if (dd->bsea.iram_phys > 0x00020000)
			aes_writel(eng, BIT(0), TEGRA_AES_IRAM_ACCESS_CFG);
		else
			aes_writel(eng, 0, TEGRA_AES_IRAM_ACCESS_CFG);

		/* settable command to get key into internal registers */
		value = CMD_SETTABLE << CMDQ_OPCODE_SHIFT  |
			SUBCMD_CRYPTO_TABLE_SEL << CMDQ_TABLESEL_SHIFT |
			(SUBCMD_KEY_TABLE_SEL | slot_num)
				<< CMDQ_KEYTABLEID_SHIFT |
			dd->bsea.iram_phys >> 2;
	}

	aes_writel(eng, value, TEGRA_AES_ICMDQUE_WR);
	do {
		value = aes_readl(eng, TEGRA_AES_INTR_STATUS);
		eng_busy = value & TEGRA_AES_ENGINE_BUSY_FIELD;
		icq_empty = value & TEGRA_AES_ICQ_EMPTY_FIELD;
	} while (eng_busy & (!icq_empty));

out:
	return 0;
}

static int tegra_aes_handle_req(struct tegra_aes_engine *eng)
{
	struct tegra_aes_dev *dd = aes_dev;
	struct tegra_aes_ctx *ctx;
	struct crypto_async_request *async_req, *backlog;
	struct tegra_aes_reqctx *rctx;
	struct ablkcipher_request *req;
	unsigned long irq_flags;
	int dma_max = AES_HW_DMA_BUFFER_SIZE_BYTES;
	int nblocks, total, ret = 0, count = 0;
	dma_addr_t addr_in, addr_out;
	struct scatterlist *in_sg, *out_sg;

	spin_lock_irqsave(&dd->lock, irq_flags);
	backlog = crypto_get_backlog(&dd->queue);
	async_req = crypto_dequeue_request(&dd->queue);
	if (!async_req)
		clear_bit(FLAGS_BUSY, &eng->busy);
	spin_unlock_irqrestore(&dd->lock, irq_flags);

	if (!async_req)
		return -ENODATA;

	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	req = ablkcipher_request_cast(async_req);
	dev_dbg(dd->dev, "%s: get new req (engine #%d)\n", __func__,
		eng->res_id);

	if (!req->src || !req->dst)
		return -EINVAL;

	/* take the hardware semaphore */
	if (tegra_arb_mutex_lock_timeout(eng->res_id, ARB_SEMA_TIMEOUT) < 0) {
		dev_err(dd->dev, "aes hardware (%d) not available\n",
		eng->res_id);
		return -EBUSY;
	}

	/* assign new request to device */
	eng->req = req;
	eng->total = req->nbytes;
	eng->in_offset = 0;
	eng->in_sg = req->src;
	eng->out_offset = 0;
	eng->out_sg = req->dst;

	in_sg = eng->in_sg;
	out_sg = eng->out_sg;
	total = eng->total;

	rctx = ablkcipher_request_ctx(req);
	ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	rctx->mode &= FLAGS_MODE_MASK;
	dd->flags = (dd->flags & ~FLAGS_MODE_MASK) | rctx->mode;
	eng->ctx = ctx;

	if (((dd->flags & FLAGS_CBC) || (dd->flags & FLAGS_OFB)) && req->info) {
		/* set iv to the aes hw slot
		 * Hw generates updated iv only after iv is set in slot.
		 * So key and iv is passed asynchronously.
		*/
		memcpy(eng->buf_in, (u8 *)req->info, AES_BLOCK_SIZE);

		/* sync the buffer for device  */
		dma_sync_single_for_device(dd->dev, eng->dma_buf_in,
				AES_HW_DMA_BUFFER_SIZE_BYTES, DMA_TO_DEVICE);

		ret = aes_start_crypt(eng, (u32)eng->dma_buf_in,
			(u32)eng->dma_buf_out, 1, FLAGS_CBC, false);
		if (ret < 0) {
			dev_err(dd->dev, "aes_start_crypt fail(%d)\n", ret);
			goto out;
		}

		/* sync the buffer for cpu  */
		dma_sync_single_for_cpu(dd->dev, eng->dma_buf_out,
				AES_HW_DMA_BUFFER_SIZE_BYTES, DMA_FROM_DEVICE);
	}

	while (total) {
		dev_dbg(dd->dev, "remain: %d\n", total);
		ret = dma_map_sg(dd->dev, in_sg, 1, DMA_TO_DEVICE);
		if (!ret) {
			dev_err(dd->dev, "dma_map_sg() error\n");
			goto out;
		}

		ret = dma_map_sg(dd->dev, out_sg, 1, DMA_FROM_DEVICE);
		if (!ret) {
			dev_err(dd->dev, "dma_map_sg() error\n");
			dma_unmap_sg(dd->dev, eng->in_sg,
				1, DMA_TO_DEVICE);
			goto out;
		}

		addr_in = sg_dma_address(in_sg);
		addr_out = sg_dma_address(out_sg);
		count = min_t(int, sg_dma_len(in_sg), dma_max);
		WARN_ON(sg_dma_len(in_sg) != sg_dma_len(out_sg));
		nblocks = DIV_ROUND_UP(count, AES_BLOCK_SIZE);

		ret = aes_start_crypt(eng, addr_in, addr_out, nblocks,
			dd->flags, true);

		dma_unmap_sg(dd->dev, out_sg, 1, DMA_FROM_DEVICE);
		dma_unmap_sg(dd->dev, in_sg, 1, DMA_TO_DEVICE);

		if (ret < 0) {
			dev_err(dd->dev, "aes_start_crypt fail(%d)\n", ret);
			goto out;
		}

		dev_dbg(dd->dev, "out: copied %d\n", count);
		total -= count;
		in_sg = sg_next(in_sg);
		out_sg = sg_next(out_sg);
		WARN_ON(((total != 0) && (!in_sg || !out_sg)));
	}

out:
	/* release the hardware semaphore */
	tegra_arb_mutex_unlock(eng->res_id);
	eng->total = total;

	if (eng->req->base.complete)
		eng->req->base.complete(&eng->req->base, ret);

	dev_dbg(dd->dev, "%s: exit\n", __func__);
	return ret;
}

static int tegra_aes_key_save(struct tegra_aes_ctx *ctx)
{
	struct tegra_aes_dev *dd = aes_dev;
	int retry_count, eng_busy, ret, eng_no;
	struct tegra_aes_engine *eng[2] = {&dd->bsev, &dd->bsea};
	unsigned long flags;

	/* check for engine free state */
	for (eng_no = 0; eng_no < ARRAY_SIZE(eng); eng_no++) {
		for (retry_count = 0; retry_count <= 10; retry_count++) {
			spin_lock_irqsave(&dd->lock, flags);
			eng_busy = test_and_set_bit(FLAGS_BUSY,
							&eng[eng_no]->busy);
			spin_unlock_irqrestore(&dd->lock, flags);

			if (!eng_busy)
				break;

			if (retry_count == 10) {
				dev_err(dd->dev,
					"%s: eng=%d busy, wait timeout\n",
					__func__, eng[eng_no]->res_id);
				ret = -EBUSY;
				goto out;
			}
			mdelay(5);
		}
	}

	/* save key in the engine */
	for (eng_no = 0;  eng_no < ARRAY_SIZE(eng); eng_no++) {
		ret = aes_hw_init(eng[eng_no]);
		if (ret < 0) {
			dev_err(dd->dev, "%s: eng=%d hw init fail(%d)\n",
			__func__, eng[eng_no]->res_id, ret);
			goto out;
		}
		eng[eng_no]->ctx = ctx;
		if (ctx->use_ssk)
			aes_set_key(eng[eng_no], SSK_SLOT_NUM);
		else
			aes_set_key(eng[eng_no], ctx->slot->slot_num);

		aes_hw_deinit(eng[eng_no]);
	}
out:
	spin_lock_irqsave(&dd->lock, flags);
	while (--eng_no >= 0)
		clear_bit(FLAGS_BUSY, &eng[eng_no]->busy);
	spin_unlock_irqrestore(&dd->lock, flags);

	return ret;
}

static int tegra_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			    unsigned int keylen)
{
	struct tegra_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct tegra_aes_dev *dd = aes_dev;
	struct tegra_aes_slot *key_slot;
	int ret = 0;

	if (!ctx || !dd) {
		pr_err("ctx=0x%x, dd=0x%x\n",
			(unsigned int)ctx, (unsigned int)dd);
		return -EINVAL;
	}

	if ((keylen != AES_KEYSIZE_128) && (keylen != AES_KEYSIZE_192) &&
		(keylen != AES_KEYSIZE_256)) {
		dev_err(dd->dev, "unsupported key size\n");
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	/* take the hardware semaphore */
	if (tegra_arb_mutex_lock_timeout(dd->bsev.res_id, ARB_SEMA_TIMEOUT) < 0) {
		dev_err(dd->dev, "aes hardware (%d) not available\n", dd->bsev.res_id);
		return -EBUSY;
	}

	if (tegra_arb_mutex_lock_timeout(dd->bsea.res_id, ARB_SEMA_TIMEOUT) < 0) {
		dev_err(dd->dev, "aes hardware (%d) not available\n", dd->bsea.res_id);
		tegra_arb_mutex_unlock(dd->bsev.res_id);
		return -EBUSY;
	}

	dev_dbg(dd->dev, "keylen: %d\n", keylen);
	ctx->dd = dd;

	if (key) {
		if (!ctx->slot) {
			key_slot = aes_find_key_slot();
			if (!key_slot) {
				dev_err(dd->dev, "no empty slot\n");
				ret = -EBUSY;
				goto out;
			}
			ctx->slot = key_slot;
		}

		/* copy the key to the proper slot */
		memset(ctx->key, 0, AES_MAX_KEY_SIZE);
		memcpy(ctx->key, key, keylen);
		ctx->keylen = keylen;
		ctx->use_ssk = false;
	} else {
		ctx->use_ssk = true;
		ctx->keylen = AES_KEYSIZE_128;
	}

	ret = tegra_aes_key_save(ctx);
	if (ret != 0)
		dev_err(dd->dev, "%s failed\n", __func__);
out:
	tegra_arb_mutex_unlock(dd->bsev.res_id);
	tegra_arb_mutex_unlock(dd->bsea.res_id);
	dev_dbg(dd->dev, "done\n");
	return ret;
}

static void bsev_workqueue_handler(struct work_struct *work)
{
	struct tegra_aes_dev *dd = aes_dev;
	struct tegra_aes_engine *engine = &dd->bsev;
	int ret;

	aes_hw_init(engine);

	/* empty the crypto queue and then return */
	do {
		ret = tegra_aes_handle_req(engine);
	} while (!ret);

	aes_hw_deinit(engine);
}

static void bsea_workqueue_handler(struct work_struct *work)
{
	struct tegra_aes_dev *dd = aes_dev;
	struct tegra_aes_engine *engine = &dd->bsea;
	int ret;

	aes_hw_init(engine);

	/* empty the crypto queue and then return */
	do {
		ret = tegra_aes_handle_req(engine);
	} while (!ret);

	aes_hw_deinit(engine);
}

#define INT_ERROR_MASK	0xFFF000
static irqreturn_t aes_bsev_irq(int irq, void *dev_id)
{
	struct tegra_aes_dev *dd = (struct tegra_aes_dev *)dev_id;
	u32 value = aes_readl(&dd->bsev, TEGRA_AES_INTR_STATUS);

	dev_dbg(dd->dev, "bsev irq_stat: 0x%x", value);
	dd->bsev.status = 0;
	if (value & TEGRA_AES_INT_ERROR_MASK) {
		aes_writel(&dd->bsev, TEGRA_AES_INT_ERROR_MASK, TEGRA_AES_INTR_STATUS);
		dd->bsev.status = value & TEGRA_AES_INT_ERROR_MASK;
	}

	value = aes_readl(&dd->bsev, TEGRA_AES_INTR_STATUS);
	if (!(value & TEGRA_AES_ENGINE_BUSY_FIELD))
		complete(&dd->bsev.op_complete);

	return IRQ_HANDLED;
}

static irqreturn_t aes_bsea_irq(int irq, void *dev_id)
{
	struct tegra_aes_dev *dd = (struct tegra_aes_dev *)dev_id;
	u32 value = aes_readl(&dd->bsea, TEGRA_AES_INTR_STATUS);

	dev_dbg(dd->dev, "bsea irq_stat: 0x%x", value);
	dd->bsea.status = 0;
	if (value & TEGRA_AES_INT_ERROR_MASK) {
		aes_writel(&dd->bsea, TEGRA_AES_INT_ERROR_MASK, TEGRA_AES_INTR_STATUS);
		dd->bsea.status = value & TEGRA_AES_INT_ERROR_MASK;
	}

	value = aes_readl(&dd->bsea, TEGRA_AES_INTR_STATUS);
	if (!(value & TEGRA_AES_ENGINE_BUSY_FIELD))
		complete(&dd->bsea.op_complete);

	return IRQ_HANDLED;
}

static int tegra_aes_crypt(struct ablkcipher_request *req, unsigned long mode)
{
	struct tegra_aes_reqctx *rctx = ablkcipher_request_ctx(req);
	struct tegra_aes_dev *dd = aes_dev;
	unsigned long flags;
	int err = 0;
	int bsev_busy;
	int bsea_busy;

	dev_dbg(dd->dev, "nbytes: %d, enc: %d, cbc: %d, ofb: %d\n",
		req->nbytes, !!(mode & FLAGS_ENCRYPT), !!(mode & FLAGS_CBC),
		!!(mode & FLAGS_OFB));

	rctx->mode = mode;

	spin_lock_irqsave(&dd->lock, flags);
	err = ablkcipher_enqueue_request(&dd->queue, req);
	spin_unlock_irqrestore(&dd->lock, flags);
	bsev_busy = test_and_set_bit(FLAGS_BUSY, &dd->bsev.busy);
	if (!bsev_busy) {
		queue_work(bsev_wq, &bsev_work);
		goto finish;
	}

	bsea_busy = test_and_set_bit(FLAGS_BUSY, &dd->bsea.busy);
	if (!bsea_busy)
		queue_work(bsea_wq, &bsea_work);

finish:
	return err;
}

static int tegra_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	return tegra_aes_crypt(req, FLAGS_ENCRYPT);
}

static int tegra_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	return tegra_aes_crypt(req, 0);
}

static int tegra_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	return tegra_aes_crypt(req, FLAGS_ENCRYPT | FLAGS_CBC);
}

static int tegra_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	return tegra_aes_crypt(req, FLAGS_CBC);
}

static int tegra_aes_ofb_encrypt(struct ablkcipher_request *req)
{
	return tegra_aes_crypt(req, FLAGS_ENCRYPT | FLAGS_OFB);
}

static int tegra_aes_ofb_decrypt(struct ablkcipher_request *req)
{
	return tegra_aes_crypt(req, FLAGS_OFB);
}

static int tegra_aes_get_random(struct crypto_rng *tfm, u8 *rdata,
	unsigned int dlen)
{
	struct tegra_aes_dev *dd = aes_dev;
	struct tegra_aes_engine *eng = rng_ctx.eng;
	unsigned long flags;
	int ret, i;
	u8 *dest = rdata, *dt = rng_ctx.dt;

	/* take the hardware semaphore */
	if (tegra_arb_mutex_lock_timeout(eng->res_id, ARB_SEMA_TIMEOUT) < 0) {
		dev_err(dd->dev, "aes hardware (%d) not available\n",
		eng->res_id);
		return -EBUSY;
	}

	ret = aes_hw_init(eng);
	if (ret < 0) {
		dev_err(dd->dev, "%s: hw init fail(%d)\n", __func__, ret);
		dlen = ret;
		goto fail;
	}

	memset(eng->buf_in, 0, AES_BLOCK_SIZE);
	memcpy(eng->buf_in, dt, DEFAULT_RNG_BLK_SZ);

	/* sync the buffer for device */
	dma_sync_single_for_device(dd->dev, eng->dma_buf_in,
			AES_HW_DMA_BUFFER_SIZE_BYTES, DMA_TO_DEVICE);

	ret = aes_start_crypt(eng, (u32)eng->dma_buf_in, (u32)eng->dma_buf_out,
		1, FLAGS_ENCRYPT | FLAGS_RNG, true);
	if (ret < 0) {
		dev_err(dd->dev, "aes_start_crypt fail(%d)\n", ret);
		dlen = ret;
		goto out;
	}
	/* sync the buffer for cpu */
	dma_sync_single_for_cpu(dd->dev, eng->dma_buf_out,
			AES_HW_DMA_BUFFER_SIZE_BYTES, DMA_FROM_DEVICE);

	memcpy(dest, eng->buf_out, dlen);

	/* update the DT */
	for (i = DEFAULT_RNG_BLK_SZ - 1; i >= 0; i--) {
		dt[i] += 1;
		if (dt[i] != 0)
			break;
	}

out:
	aes_hw_deinit(eng);

	spin_lock_irqsave(&dd->lock, flags);
	clear_bit(FLAGS_BUSY, &eng->busy);
	spin_unlock_irqrestore(&dd->lock, flags);

fail:
	/* release the hardware semaphore */
	tegra_arb_mutex_unlock(eng->res_id);
	dev_dbg(dd->dev, "%s: done\n", __func__);
	return dlen;
}

static int tegra_aes_rng_reset(struct crypto_rng *tfm, u8 *seed,
	unsigned int slen)
{
	struct tegra_aes_dev *dd = aes_dev;
	struct tegra_aes_ctx *ctx = &rng_ctx;
	struct tegra_aes_engine *eng = NULL;
	struct tegra_aes_slot *key_slot;
	int bsea_busy = false;
	unsigned long flags;
	struct timespec ts;
	u64 nsec, tmp[2];
	int ret = 0;
	u8 *dt;

	if (!dd)
		return -EINVAL;

	if (slen < (DEFAULT_RNG_BLK_SZ + AES_KEYSIZE_128)) {
		return -ENOMEM;
	}

	spin_lock_irqsave(&dd->lock, flags);
	bsea_busy = test_and_set_bit(FLAGS_BUSY, &dd->bsea.busy);
	spin_unlock_irqrestore(&dd->lock, flags);

	if (!bsea_busy)
		eng = &dd->bsea;
	else
		return -EBUSY;

	ctx->eng = eng;
	dd->flags = FLAGS_ENCRYPT | FLAGS_RNG;

	if (!ctx->slot) {
		key_slot = aes_find_key_slot();
		if (!key_slot) {
			dev_err(dd->dev, "no empty slot\n");
			return -ENOMEM;
		}
		ctx->slot = key_slot;
	}

	/* take the hardware semaphore */
	if (tegra_arb_mutex_lock_timeout(eng->res_id, ARB_SEMA_TIMEOUT) < 0) {
		dev_err(dd->dev, "aes hardware (%d) not available\n",
		eng->res_id);
		return -EBUSY;
	}

	ret = aes_hw_init(eng);
	if (ret < 0) {
		dev_err(dd->dev, "%s: hw init fail(%d)\n", __func__, ret);
		goto fail;
	}

	memcpy(ctx->key, seed + DEFAULT_RNG_BLK_SZ, AES_KEYSIZE_128);

	eng->ctx = ctx;
	eng->ctx->keylen = AES_KEYSIZE_128;
	aes_set_key(eng, ctx->slot->slot_num);

	/* set seed to the aes hw slot */
	memset(eng->buf_in, 0, AES_BLOCK_SIZE);
	memcpy(eng->buf_in, seed, DEFAULT_RNG_BLK_SZ);

	/* sync the buffer for device */
	dma_sync_single_for_device(dd->dev, eng->dma_buf_in,
			AES_HW_DMA_BUFFER_SIZE_BYTES, DMA_TO_DEVICE);

	ret = aes_start_crypt(eng, (u32)eng->dma_buf_in,
	  (u32)eng->dma_buf_out, 1, FLAGS_CBC, false);
	if (ret < 0) {
		dev_err(dd->dev, "aes_start_crypt fail(%d)\n", ret);
		goto out;
	}
	/* sync the buffer for cpu */
	dma_sync_single_for_cpu(dd->dev, eng->dma_buf_out,
			AES_HW_DMA_BUFFER_SIZE_BYTES, DMA_FROM_DEVICE);

	if (slen >= (2 * DEFAULT_RNG_BLK_SZ + AES_KEYSIZE_128)) {
		dt = seed + DEFAULT_RNG_BLK_SZ + AES_KEYSIZE_128;
	} else {
		getnstimeofday(&ts);
		nsec = timespec_to_ns(&ts);
		do_div(nsec, 1000);
		nsec ^= dd->ctr << 56;
		dd->ctr++;
		tmp[0] = nsec;
		tmp[1] = tegra_chip_uid();
		dt = (u8 *)tmp;
	}

	memcpy(ctx->dt, dt, DEFAULT_RNG_BLK_SZ);

out:
	aes_hw_deinit(eng);

fail:
	/* release the hardware semaphore */
	tegra_arb_mutex_unlock(eng->res_id);

	dev_dbg(dd->dev, "%s: done\n", __func__);
	return ret;
}

static int tegra_aes_cra_init(struct crypto_tfm *tfm)
{
	tfm->crt_ablkcipher.reqsize = sizeof(struct tegra_aes_reqctx);
	return 0;
}

void tegra_aes_cra_exit(struct crypto_tfm *tfm)
{
	struct tegra_aes_ctx *ctx =
		crypto_ablkcipher_ctx((struct crypto_ablkcipher *)tfm);

	if (ctx && ctx->slot)
		aes_release_key_slot(ctx->slot);
}

static struct crypto_alg algs[] = {
	{
		.cra_name = "disabled_ecb(aes)",
		.cra_driver_name = "ecb-aes-tegra",
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_alignmask = 3,
		.cra_type = &crypto_ablkcipher_type,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.setkey = tegra_aes_setkey,
			.encrypt = tegra_aes_ecb_encrypt,
			.decrypt = tegra_aes_ecb_decrypt,
		},
	}, {
		.cra_name = "disabled_cbc(aes)",
		.cra_driver_name = "cbc-aes-tegra",
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_alignmask = 3,
		.cra_type = &crypto_ablkcipher_type,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_MIN_KEY_SIZE,
			.setkey = tegra_aes_setkey,
			.encrypt = tegra_aes_cbc_encrypt,
			.decrypt = tegra_aes_cbc_decrypt,
		}
	}, {
		.cra_name = "disabled_ofb(aes)",
		.cra_driver_name = "ofb-aes-tegra",
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_alignmask = 3,
		.cra_type = &crypto_ablkcipher_type,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_MIN_KEY_SIZE,
			.setkey = tegra_aes_setkey,
			.encrypt = tegra_aes_ofb_encrypt,
			.decrypt = tegra_aes_ofb_decrypt,
		}
	}, {
		.cra_name = "disabled_ansi_cprng",
		.cra_driver_name = "rng-aes-tegra",
		.cra_flags = CRYPTO_ALG_TYPE_RNG,
		.cra_ctxsize = sizeof(struct tegra_aes_ctx),
		.cra_type = &crypto_rng_type,
		.cra_u.rng = {
			.rng_make_random = tegra_aes_get_random,
			.rng_reset = tegra_aes_rng_reset,
			.seedsize = AES_KEYSIZE_128 + (2 * DEFAULT_RNG_BLK_SZ),
		}
	}
};

static int tegra_aes_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tegra_aes_dev *dd;
	struct resource *res;
	int err = -ENOMEM, i = 0, j;

	dd = devm_kzalloc(dev, sizeof(struct tegra_aes_dev), GFP_KERNEL);
	if (dd == NULL) {
		dev_err(dev, "unable to alloc data struct.\n");
		return err;
	}

	dd->dev = dev;
	platform_set_drvdata(pdev, dd);

	dd->slots = devm_kzalloc(dev, sizeof(struct tegra_aes_slot) *
				 AES_NR_KEYSLOTS, GFP_KERNEL);
	if (dd->slots == NULL) {
		dev_err(dev, "unable to alloc slot struct.\n");
		goto out;
	}

	spin_lock_init(&dd->lock);
	crypto_init_queue(&dd->queue, TEGRA_AES_QUEUE_LENGTH);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!devm_request_mem_region(&pdev->dev, res->start,
				     resource_size(res),
				     dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "Couldn't request MEM resource\n");
		return -ENODEV;
	}

	dd->bsev.io_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!dd->bsev.io_base) {
		dev_err(dev, "can't ioremap bsev register space\n");
		err = -ENOMEM;
		goto out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!devm_request_mem_region(&pdev->dev, res->start,
				     resource_size(res),
				     dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "Couldn't request MEM resource\n");
		return -ENODEV;
	}

	dd->bsea.io_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!dd->bsea.io_base) {
		dev_err(dev, "can't ioremap bsea register space\n");
		err = -ENOMEM;
		goto out;
	}

	err = alloc_iram(dd);
	if (err < 0) {
		dev_err(dev, "Failed to allocate IRAM for BSEA\n");
		goto out;
	}

	dd->bsev.res_id = TEGRA_ARB_BSEV;
	dd->bsea.res_id = TEGRA_ARB_BSEA;

	dd->bsev.pclk = clk_get(dev, "bsev");
	if (IS_ERR(dd->bsev.pclk)) {
		dev_err(dev, "v: pclock intialization failed.\n");
		err = -ENODEV;
		goto out;
	}

	dd->bsev.iclk = clk_get(dev, "vde");
	if (IS_ERR(dd->bsev.iclk)) {
		dev_err(dev, "v: iclock intialization failed.\n");
		err = -ENODEV;
		goto out;
	}

	dd->bsea.pclk = clk_get(dev, "bsea");
	if (IS_ERR(dd->bsea.pclk)) {
		dev_err(dev, "a: pclock intialization failed.\n");
		err = -ENODEV;
		goto out;
	}

	dd->bsea.iclk = clk_get(dev, "sclk");
	if (IS_ERR(dd->bsea.iclk)) {
		dev_err(dev, "a: iclock intialization failed.\n");
		err = -ENODEV;
		goto out;
	}

	err = clk_set_rate(dd->bsev.iclk, ULONG_MAX);
	if (err) {
		dev_err(dd->dev, "bsev iclk set_rate fail(%d)\n", err);
		goto out;
	}

	err = clk_set_rate(dd->bsea.iclk, ULONG_MAX);
	if (err) {
		dev_err(dd->dev, "bsea iclk set_rate fail(%d)\n", err);
		goto out;
	}

	/*
	 * the foll contiguous memory is allocated as follows -
	 * - hardware key table
	 * - key schedule
	 */
	dd->bsea.ivkey_base = NULL;
	dd->bsev.ivkey_base = dma_alloc_coherent(dev, SZ_512,
		&dd->bsev.ivkey_phys_base, GFP_KERNEL);
	if (!dd->bsev.ivkey_base) {
		dev_err(dev, "can not allocate iv/key buffer for BSEV\n");
		err = -ENOMEM;
		goto out;
	}
	memset(dd->bsev.ivkey_base, 0, SZ_512);

	dd->bsev.buf_in = dma_alloc_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES,
		&dd->bsev.dma_buf_in, GFP_KERNEL);
	dd->bsea.buf_in = dma_alloc_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES,
		&dd->bsea.dma_buf_in, GFP_KERNEL);
	if (!dd->bsev.buf_in || !dd->bsea.buf_in) {
		dev_err(dev, "can not allocate dma-in buffer\n");
		err = -ENOMEM;
		goto out;
	}

	dd->bsev.buf_out = dma_alloc_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES,
					      &dd->bsev.dma_buf_out, GFP_KERNEL);
	dd->bsea.buf_out = dma_alloc_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES,
					      &dd->bsea.dma_buf_out, GFP_KERNEL);
	if (!dd->bsev.buf_out || !dd->bsea.buf_out) {
		dev_err(dev, "can not allocate dma-out buffer\n");
		err = -ENOMEM;
		goto out;
	}

	init_completion(&dd->bsev.op_complete);
	init_completion(&dd->bsea.op_complete);

	bsev_wq = alloc_workqueue("bsev_wq", WQ_HIGHPRI | WQ_UNBOUND, 1);
	bsea_wq = alloc_workqueue("bsea_wq", WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!bsev_wq || !bsea_wq) {
		dev_err(dev, "alloc_workqueue failed\n");
		goto out;
	}

	/* get the irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "invalid resource type: IRQ\n");
		err = -ENODEV;
		goto out;
	}
	dd->bsev.irq = res->start;

	err = devm_request_irq(dev, dd->bsev.irq, aes_bsev_irq,
		IRQF_TRIGGER_HIGH | IRQF_SHARED, "tegra-aes", dd);
	if (err) {
		dev_err(dev, "request_irq failed fir BSEV Engine\n");
		goto out;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (!res) {
		dev_err(dev, "invalid resource type: IRQ\n");
		err = -ENODEV;
		goto out;
	}
	dd->bsea.irq = res->start;

	err = devm_request_irq(dev, dd->bsea.irq, aes_bsea_irq,
		IRQF_TRIGGER_HIGH | IRQF_SHARED, "tegra-aes", dd);
	if (err) {
		dev_err(dev, "request_irq failed for BSEA Engine\n");
		goto out;
	}

	INIT_LIST_HEAD(&dev_list);

	spin_lock_init(&list_lock);
	spin_lock(&list_lock);
	for (i = 0; i < AES_NR_KEYSLOTS; i++) {
		if (i == SSK_SLOT_NUM)
			continue;
		dd->slots[i].slot_num = i;
		dd->slots[i].available = true;
		INIT_LIST_HEAD(&dd->slots[i].node);
		list_add_tail(&dd->slots[i].node, &dev_list);
	}
	spin_unlock(&list_lock);

	aes_dev = dd;
	for (i = 0; i < ARRAY_SIZE(algs); i++) {
		INIT_LIST_HEAD(&algs[i].cra_list);

		algs[i].cra_priority = 300;
		algs[i].cra_ctxsize = sizeof(struct tegra_aes_ctx);
		algs[i].cra_module = THIS_MODULE;
		algs[i].cra_init = tegra_aes_cra_init;
		algs[i].cra_exit = tegra_aes_cra_exit;

		err = crypto_register_alg(&algs[i]);
		if (err)
			goto out;
	}

	dev_info(dev, "registered");
	return 0;

out:
	for (j = 0; j < i; j++)
		crypto_unregister_alg(&algs[j]);

	free_iram(dd);

	if (dd->bsev.ivkey_base) {
		dma_free_coherent(dev, AES_HW_KEY_TABLE_LENGTH_BYTES,
			dd->bsev.ivkey_base, dd->bsev.ivkey_phys_base);
	}

	if (dd->bsev.buf_in && dd->bsea.buf_in) {
		dma_free_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES,
			dd->bsev.buf_in, dd->bsev.dma_buf_in);
		dma_free_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES,
			dd->bsea.buf_in, dd->bsea.dma_buf_in);
	}

	if (dd->bsev.buf_out && dd->bsea.buf_out) {
		dma_free_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES,
			dd->bsev.buf_out, dd->bsev.dma_buf_out);
		dma_free_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES,
			dd->bsea.buf_out, dd->bsea.dma_buf_out);
	}

	if (dd->bsev.pclk)
		clk_put(dd->bsev.pclk);

	if (dd->bsev.iclk)
		clk_put(dd->bsev.iclk);

	if (dd->bsea.pclk)
		clk_put(dd->bsea.pclk);

	if (bsev_wq)
		destroy_workqueue(bsev_wq);

	if (bsea_wq)
		destroy_workqueue(bsea_wq);

	spin_lock(&list_lock);
	list_del(&dev_list);
	spin_unlock(&list_lock);
	aes_dev = NULL;

	dev_err(dev, "%s: initialization failed.\n", __func__);
	return err;
}

static int __devexit tegra_aes_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tegra_aes_dev *dd = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(algs); i++)
		crypto_unregister_alg(&algs[i]);

	cancel_work_sync(&bsev_work);
	cancel_work_sync(&bsea_work);
	destroy_workqueue(bsev_wq);
	destroy_workqueue(bsea_wq);
	free_irq(dd->bsev.irq, dd);
	free_irq(dd->bsea.irq, dd);
	spin_lock(&list_lock);
	list_del(&dev_list);
	spin_unlock(&list_lock);

	free_iram(dd);
	dma_free_coherent(dev, SZ_512, dd->bsev.ivkey_base,
		dd->bsev.ivkey_phys_base);
	dma_free_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES, dd->bsev.buf_in,
		dd->bsev.dma_buf_in);
	dma_free_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES, dd->bsea.buf_in,
		dd->bsea.dma_buf_in);
	dma_free_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES, dd->bsev.buf_out,
		dd->bsev.dma_buf_out);
	dma_free_coherent(dev, AES_HW_DMA_BUFFER_SIZE_BYTES, dd->bsea.buf_out,
		dd->bsea.dma_buf_out);

	clk_put(dd->bsev.iclk);
	clk_put(dd->bsev.pclk);
	clk_put(dd->bsea.pclk);

	aes_dev = NULL;

	return 0;
}

static struct of_device_id tegra_aes_of_match[] __devinitdata = {
	{ .compatible = "nvidia,tegra20-aes", },
	{ .compatible = "nvidia,tegra30-aes", },
	{ },
};

static struct platform_driver tegra_aes_driver = {
	.probe  = tegra_aes_probe,
	.remove = __devexit_p(tegra_aes_remove),
	.driver = {
		.name   = "tegra-aes",
		.owner  = THIS_MODULE,
		.of_match_table = tegra_aes_of_match,
	},
};

module_platform_driver(tegra_aes_driver);

MODULE_DESCRIPTION("Tegra AES/OFB/CPRNG hw acceleration support.");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL v2");
