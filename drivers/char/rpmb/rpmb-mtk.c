// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/memory.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <crypto/hash.h>

#include <linux/rpmb.h>
#include "rpmb-mtk.h"

#include <linux/scatterlist.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include "queue.h"
#include "mmc_ops.h"
#include "core.h"
#include "mtk_sd.h"
#include "card.h"

#ifdef CONFIG_MTK_UFS_SUPPORT
#include "ufs-mtk.h"
#endif
#include <mt-plat/mtk_boot.h>

/* #define __RPMB_MTK_DEBUG_MSG */
/* #define __RPMB_MTK_DEBUG_HMAC_VERIFY */

/* TEE usage */
#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
#include "mobicore_driver_api.h"
#include "drrpmb_Api.h"
#include "drrpmb_gp_Api.h"

#ifndef CONFIG_TEE
static struct mc_uuid_t rpmb_uuid = RPMB_UUID;
static struct mc_session_handle rpmb_session = {0};
static u32 rpmb_devid = MC_DEVICE_ID_DEFAULT;
static struct dciMessage_t *rpmb_dci;
#endif

static struct mc_uuid_t rpmb_gp_uuid = RPMB_GP_UUID;
static struct mc_session_handle rpmb_gp_session = {0};
static u32 rpmb_gp_devid = MC_DEVICE_ID_DEFAULT;
static struct dciMessage_t *rpmb_gp_dci;

#endif

/*
 * Dummy definition for MAX_RPMB_TRANSFER_BLK.
 *
 * For UFS RPMB driver, MAX_RPMB_TRANSFER_BLK will be always
 * used however it will NOT be defined in projects w/o Security
 * OS. Thus we add a dummy definition here to avoid build errors.
 *
 * For eMMC RPMB driver, MAX_RPMB_TRANSFER_BLK will be used
 * only if RPMB_MULTI_BLOCK_ACCESS is defined. thus
 * build error will not happen on projects w/o Security OS.
 *
 * NOTE: This dummy definition shall be located after
 *       #include "drrpmb_Api.h" and
 *       #include "rpmb-mtk.h"
 *       since MAX_RPMB_TRANSFER_BLK will be defined in those
 *       header files if security OS is enabled.
 */
#ifndef MAX_RPMB_TRANSFER_BLK
#define MAX_RPMB_TRANSFER_BLK (1)
#endif

#define RPMB_NAME "rpmb"

#define DEFAULT_HANDLES_NUM (64)
#define MAX_OPEN_SESSIONS (0xffffffff - 1)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/* Debug message event */
#define DBG_EVT_NONE (0) /* No event */
#define DBG_EVT_CMD  (1 << 0)/* SEC CMD related event */
#define DBG_EVT_FUNC (1 << 1)/* SEC function event */
#define DBG_EVT_INFO (1 << 2)/* SEC information event */
#define DBG_EVT_WRN  (1 << 30) /* Warning event */
#define DBG_EVT_ERR  (1 << 31) /* Error event */
#ifdef __RPMB_MTK_DEBUG_MSG
#define DBG_EVT_DBG_INFO  (1 << 31) /* Error event */
#else
#define DBG_EVT_DBG_INFO  (1 << 2) /* Information event */
#endif
#define DBG_EVT_ALL  (0xffffffff)

#define DBG_EVT_MASK (DBG_EVT_ERR)

#define MSG(evt, fmt, args...) \
do {\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		pr_notice("[%s] "fmt, RPMB_NAME, ##args); \
	} \
} while (0)

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
#define RPMB_DATA_BUFF_SIZE (1024 * 24)
#define RPMB_ONE_FRAME_SIZE (512)
static unsigned char *rpmb_buffer;
#endif

struct task_struct *open_th;
struct task_struct *rpmbDci_th;
struct task_struct *rpmb_gp_Dci_th;


static struct cdev rpmb_dev;
static struct class *mtk_rpmb_class;

/*
 * This is an alternative way to get mmc_card strcuture from mmc_host which set
 * from msdc driver with this callback function.
 * The strength is we don't have to extern msdc_host_host global variable,
 * extern global is very bad...
 * The weakness is every platform driver needs to add this callback to give
 * rpmb driver the mmc_host structure and then we could know card.
 *
 * Finally, I decide to ignore its strength, because the weakness is more
 * important.
 * If every projects have to add this callback, the operation is complicated.
 */

#ifdef EMMC_RPMB_SET_HOST
struct mmc_host *emmc_rpmb_host;

void emmc_rpmb_set_host(void *mmc_host)
{
	emmc_rpmb_host = mmc_host;
}
#endif

int hmac_sha256(const char *key, u32 klen, const char *str, u32 len, u8 *hmac)
{
	struct shash_desc *shash;
	struct crypto_shash *hmacsha256 =
				crypto_alloc_shash("hmac(sha256)", 0, 0);
	u32 size = 0;
	int err = 0;

	if (IS_ERR(hmacsha256))
		return -1;

	size = sizeof(struct shash_desc) + crypto_shash_descsize(hmacsha256);

	shash = kmalloc(size, GFP_KERNEL);
	if (!shash) {
		err = -1;
		goto malloc_err;
	}
	shash->tfm = hmacsha256;
	shash->flags = 0x0;

	err = crypto_shash_setkey(hmacsha256, key, klen);
	if (err) {
		err = -1;
		goto hash_err;
	}

	err = crypto_shash_init(shash);
	if (err) {
		err = -1;
		goto hash_err;
	}

	crypto_shash_update(shash, str, len);
	err = crypto_shash_final(shash, hmac);

hash_err:
	kfree(shash);
malloc_err:
	crypto_free_shash(hmacsha256);

	return err;
}

#ifdef __RPMB_MTK_DEBUG_HMAC_VERIFY
unsigned char rpmb_key[32] = {
	0x64, 0x76, 0xEE, 0xF0, 0xF1, 0x6B, 0x30, 0x47,
	0xE9, 0x79, 0x31, 0x58, 0xF6, 0x42, 0xDA, 0x46,
	0xF7, 0x3B, 0x53, 0xFD, 0xC5, 0xF8, 0x84, 0xCE,
	0x03, 0x73, 0x15, 0xBC, 0x54, 0x47, 0xD4, 0x6A
};

int rpmb_cal_hmac(struct rpmb_frame *frame, int blk_cnt, u8 *key, u8 *key_mac)
{
	int i;
	u8 *buf, *buf_start;

	buf = buf_start = kzalloc(284 * blk_cnt, 0);

	for (i = 0; i < blk_cnt; i++) {
		memcpy(buf, frame[i].data, 284);
		buf += 284;
	}

	hmac_sha256(key, 32, buf_start, 284 * blk_cnt, key_mac);

	kfree(buf_start);

	return 0;
}
#endif

/*
 * CHECK THIS!!! Copy from block.c mmc_blk_data structure.
 */
struct emmc_rpmb_blk_data {
	spinlock_t lock;
	struct device	*parent;
	struct gendisk *disk;
	struct mmc_queue queue;
	struct list_head part;

	unsigned int flags;
	unsigned int usage;
	unsigned int read_only;
	unsigned int part_type;
	/* unsigned int name_idx; */
	unsigned int reset_done;

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with mmc_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int area_type;
};

static void rpmb_dump_frame(u8 *data_frame)
{
	MSG(DBG_INFO, "mac, frame[196] = 0x%x\n", data_frame[196]);
	MSG(DBG_INFO, "mac, frame[197] = 0x%x\n", data_frame[197]);
	MSG(DBG_INFO, "mac, frame[198] = 0x%x\n", data_frame[198]);
	MSG(DBG_INFO, "data,frame[228] = 0x%x\n", data_frame[228]);
	MSG(DBG_INFO, "data,frame[229] = 0x%x\n", data_frame[229]);
	MSG(DBG_INFO, "nonce, frame[484] = 0x%x\n", data_frame[484]);
	MSG(DBG_INFO, "nonce, frame[485] = 0x%x\n", data_frame[485]);
	MSG(DBG_INFO, "nonce, frame[486] = 0x%x\n", data_frame[486]);
	MSG(DBG_INFO, "nonce, frame[487] = 0x%x\n", data_frame[487]);
	MSG(DBG_INFO, "wc, frame[500] = 0x%x\n", data_frame[500]);
	MSG(DBG_INFO, "wc, frame[501] = 0x%x\n", data_frame[501]);
	MSG(DBG_INFO, "wc, frame[502] = 0x%x\n", data_frame[502]);
	MSG(DBG_INFO, "wc, frame[503] = 0x%x\n", data_frame[503]);
	MSG(DBG_INFO, "addr, frame[504] = 0x%x\n", data_frame[504]);
	MSG(DBG_INFO, "addr, frame[505] = 0x%x\n", data_frame[505]);
	MSG(DBG_INFO, "blkcnt,frame[506] = 0x%x\n", data_frame[506]);
	MSG(DBG_INFO, "blkcnt,frame[507] = 0x%x\n", data_frame[507]);
	MSG(DBG_INFO, "result, frame[508] = 0x%x\n", data_frame[508]);
	MSG(DBG_INFO, "result, frame[509] = 0x%x\n", data_frame[509]);
	MSG(DBG_INFO, "type, frame[510] = 0x%x\n", data_frame[510]);
	MSG(DBG_INFO, "type, frame[511] = 0x%x\n", data_frame[511]);
}

/*
 * CHECK THIS!!! Copy from block.c mmc_blk_part_switch.
 * Since it is static inline function, we cannot extern to use it.
 * For syncing block data, this is the only way.
 */
int emmc_rpmb_switch(struct mmc_card *card, struct emmc_rpmb_blk_data *md)
{
	int ret;
	struct emmc_rpmb_blk_data *main_md = dev_get_drvdata(&card->dev);

	if (main_md->part_curr == md->part_type)
		return 0;

#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
	if (mmc_card_cmdq(card)) {
		ret = mmc_cmdq_disable(card);
		if (ret) {
			MSG(ERR, "CQ disabled failed!!!(%x)\n", ret);
			return ret;
		}
	}
#endif

	if (mmc_card_mmc(card)) {
		u8 part_config = card->ext_csd.part_config;

		part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
		part_config |= md->part_type;

		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_PART_CONFIG, part_config,
				 card->ext_csd.part_time);
		if (ret)
			return ret;

		card->ext_csd.part_config = part_config;
	}

#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
	/* enable cmdq at user partition */
	if (!mmc_card_cmdq(card)
	&& (md->part_type <= 0)) {
		ret = mmc_cmdq_enable(card);
		if (ret)
			pr_notice("%s enable CMDQ error %d, so just work without CMDQ\n",
					mmc_hostname(card->host), ret);
	}
#endif

#if defined(CONFIG_MTK_EMMC_HW_CQ)
	card->part_curr = md->part_type;
#endif
	main_md->part_curr = md->part_type;
	return 0;
}

static int emmc_rpmb_send_command(
	struct mmc_card *card,
	u8 *buf,
	__u16 blks,
	__u16 type,
	u8 req_type
	)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_command sbc = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	u8 *transfer_buf = NULL;

	if (blks == 0) {
		MSG(ERR, "%s: Invalid blks: 0\n", __func__);
		return -EINVAL;
	}

	mrq.sbc = &sbc;
	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = NULL;
	transfer_buf = kzalloc(512 * blks, GFP_KERNEL);
	if (!transfer_buf)
		return -ENOMEM;

	/*
	 * set CMD23
	 */
	sbc.opcode = MMC_SET_BLOCK_COUNT;
	sbc.arg = blks;
	if ((req_type == RPMB_REQ && type == RPMB_WRITE_DATA) ||
					type == RPMB_PROGRAM_KEY)
		sbc.arg |= 1 << 31;
	sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;

	/*
	 * set CMD25/18
	 */
	sg_init_one(&sg, transfer_buf, 512 * blks);
	if (req_type == RPMB_REQ) {
		cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
		sg_copy_from_buffer(&sg, 1, buf, 512 * blks);
		data.flags |= MMC_DATA_WRITE;
	} else {
		cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
		data.flags |= MMC_DATA_READ;
	}

	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	data.blksz = 512;
	data.blocks = blks;
	data.sg = &sg;
	data.sg_len = 1;

	mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(card->host, &mrq);

	if (req_type != RPMB_REQ)
		sg_copy_to_buffer(&sg, 1, buf, 512 * blks);

	kfree(transfer_buf);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}


int emmc_rpmb_req_start(struct mmc_card *card, struct emmc_rpmb_req *req)
{
	int err = 0;
	u16 blks = req->blk_cnt;
	u16 type = req->type;
	u8 *data_frame = req->data_frame;

	/* MSG(INFO, "%s, start\n", __func__);    */

	/*
	 * STEP 1: send request to RPMB partition.
	 */
	if (type == RPMB_WRITE_DATA)
		err = emmc_rpmb_send_command(card, data_frame,
						blks, type, RPMB_REQ);
	else
		err = emmc_rpmb_send_command(card, data_frame,
						1, type, RPMB_REQ);

	if (err) {
		MSG(ERR, "%s step 1, request failed (%d)\n", __func__, err);
		goto out;
	}

	/*
	 * STEP 2: check write result. Only for WRITE_DATA or Program key.
	 */
	memset(data_frame, 0, 512 * blks);

	if (type == RPMB_WRITE_DATA || type == RPMB_PROGRAM_KEY) {
		data_frame[RPMB_TYPE_BEG + 1] = RPMB_RESULT_READ;
		err = emmc_rpmb_send_command(card, data_frame,
						1, RPMB_RESULT_READ, RPMB_REQ);
		if (err) {
			MSG(ERR, "%s step 2, request result failed (%d)\n",
				__func__, err);
			goto out;
		}
	}

	/*
	 * STEP 3: get response from RPMB partition
	 */
	data_frame[RPMB_TYPE_BEG] = 0;
	data_frame[RPMB_TYPE_BEG + 1] = type;

	if (type == RPMB_READ_DATA)
		err = emmc_rpmb_send_command(card, data_frame, blks,
						type, RPMB_RESP);
	else
		err = emmc_rpmb_send_command(card, data_frame, 1,
						type, RPMB_RESP);

	if (err)
		MSG(ERR, "%s step 3, response failed (%d)\n", __func__, err);

	/* MSG(INFO, "%s, end\n", __func__);    */

out:
	return err;

}

int emmc_rpmb_req_handle(struct mmc_card *card, struct emmc_rpmb_req *rpmb_req)
{
	struct emmc_rpmb_blk_data *md = NULL, *part_md;
	int ret;

	/* rpmb_dump_frame(rpmb_req->data_frame);    */

	md = dev_get_drvdata(&card->dev);

	list_for_each_entry(part_md, &md->part, part) {
		if (part_md->part_type == EXT_CSD_PART_CONFIG_ACC_RPMB)
			break;
	}

	/*  MSG(INFO, "%s start.\n", __func__); */

	mmc_get_card(card);

	/*
	 * STEP1: Switch to RPMB partition.
	 */
	ret = emmc_rpmb_switch(card, part_md);
	if (ret) {
		MSG(ERR, "%s emmc_rpmb_switch failed. (%x)\n", __func__, ret);
		goto error;
	}

	/* MSG(INFO, "%s, emmc_rpmb_switch success.\n", __func__); */

	/*
	 * STEP2: Start request. (CMD23, CMD25/18 procedure)
	 */
	ret = emmc_rpmb_req_start(card, rpmb_req);
	if (ret) {
		MSG(ERR, "%s emmc_rpmb_req_start failed!! (%x)\n",
			__func__, ret);
		goto error;
	}

	/* MSG(INFO, "%s end.\n", __func__); */

error:
	ret = emmc_rpmb_switch(card, dev_get_drvdata(&card->dev));
	if (ret)
		MSG(ERR, "%s emmc_rpmb_switch main failed. (%x)\n",
			__func__, ret);

	mmc_put_card(card);

	rpmb_dump_frame(rpmb_req->data_frame);

	return ret;
}

/* ****************************************************************************
 *
 * Following are internal APIs. Stand-alone driver without TEE.
 *
 *
 ******************************************************************************/
int emmc_rpmb_req_set_key(struct mmc_card *card, u8 *key)
{
	struct emmc_rpmb_req rpmb_req;
	struct s_rpmb *rpmb_frame;
	int ret;
	u8 user_key;

	if (get_user(user_key, key))
		return -EFAULT;

	MSG(INFO, "%s start!!!\n", __func__);

	rpmb_frame = kzalloc(sizeof(struct s_rpmb), 0);
	if (rpmb_frame == NULL)
		return RPMB_ALLOC_ERROR;

	memcpy(rpmb_frame->mac, key, RPMB_SZ_MAC);

	rpmb_req.type = RPMB_PROGRAM_KEY;
	rpmb_req.blk_cnt = 1;
	rpmb_req.data_frame = (u8 *)rpmb_frame;

	rpmb_frame->request = cpu_to_be16p(&rpmb_req.type);

	ret = emmc_rpmb_req_handle(card, &rpmb_req);
	if (ret) {
		MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
			__func__, ret);
		goto free;
	}

	if (rpmb_frame->result) {
		MSG(ERR, "%s, result error!!! (%x)\n",
			__func__, cpu_to_be16p(&rpmb_frame->result));
		ret = RPMB_RESULT_ERROR;
	}

	MSG(INFO, "%s end!!!\n", __func__);

free:
	kfree(rpmb_frame);

	return ret;
}

void rpmb_req_copy_data_for_hmac(u8 *buf, struct rpmb_frame *f)
{
	u32 size;

	/*
	 * Copy below members for HMAC calculation
	 * one by one with specifically assigning
	 * buf to each member to pass buffer-overrun checker.
	 *
	 * __u8   data[256];
	 * __u8   nonce[16];
	 * __be32 write_counter;
	 * __be16 addr;
	 * __be16 block_count;
	 * __be16 result;
	 * __be16 req_resp;
	 */

	memcpy(buf, f->data, RPMB_SZ_DATA);
	buf += RPMB_SZ_DATA;

	size = sizeof(f->nonce);
	memcpy(buf, f->nonce, size);
	buf += size;

	size = sizeof(f->write_counter);
	memcpy(buf, &f->write_counter, size);
	buf += size;

	size = sizeof(f->addr);
	memcpy(buf, &f->addr, size);
	buf += size;

	size = sizeof(f->block_count);
	memcpy(buf, &f->block_count, size);
	buf += size;

	size = sizeof(f->result);
	memcpy(buf, &f->result, size);
	buf += size;

	size = sizeof(f->req_resp);
	memcpy(buf, &f->req_resp, size);
	buf += size;
}

#ifdef CONFIG_MTK_UFS_SUPPORT
static struct rpmb_frame *rpmb_alloc_frames(unsigned int cnt)
{
	return kzalloc(sizeof(struct rpmb_frame) * cnt, 0);
}

int rpmb_req_get_wc_ufs(u8 *key, u32 *wc, u8 *frame)
{
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb;
	u8 nonce[RPMB_SZ_NONCE] = {0};
	u8 hmac[RPMB_SZ_MAC];
	int ret, i;

	MSG(INFO, "%s start!!!\n", __func__);

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	do {
		/*
		 * Initial frame buffers
		 */

		if (frame) {

			/*
			 * Use external frame if possible.
			 * External frame shall have below field ready,
			 *
			 * nonce
			 * req_resp
			 */
			data.icmd.frames = (struct rpmb_frame *)frame;
			data.ocmd.frames = (struct rpmb_frame *)frame;

		} else {

			data.icmd.frames = rpmb_alloc_frames(1);

			if (data.icmd.frames == NULL)
				return RPMB_ALLOC_ERROR;

			data.ocmd.frames = rpmb_alloc_frames(1);

			if (data.ocmd.frames == NULL) {
				kfree(data.icmd.frames);
				return RPMB_ALLOC_ERROR;
			}
		}

		/*
		 * Prepare frame contents.
		 *
		 * Input frame (in view of device) only needs nonce
		 */

		data.req_type = RPMB_GET_WRITE_COUNTER;
		data.icmd.nframes = 1;

		/* Fill-in essential field in self-prepared frame */

		if (!frame) {
			get_random_bytes(nonce, RPMB_SZ_NONCE);
			data.icmd.frames->req_resp =
				cpu_to_be16(RPMB_GET_WRITE_COUNTER);
			memcpy(data.icmd.frames->nonce, nonce, RPMB_SZ_NONCE);
		}

		/* Output frame (in view of device) */

		data.ocmd.nframes = 1;

		ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);

		if (ret) {
			MSG(ERR, "%s, rpmb_cmd_req IO error!!!(0x%x)\n",
				__func__, ret);
			break;
		}

		/* Verify HMAC only if key is available */

		if (key) {
			/*
			 * Authenticate response write counter frame.
			 */
			hmac_sha256(key, 32, data.ocmd.frames->data, 284, hmac);

			if (memcmp(hmac, data.ocmd.frames->key_mac, RPMB_SZ_MAC)
				!= 0) {
				MSG(ERR, "%s, hmac compare error!!!\n",
					__func__);
				ret = RPMB_HMAC_ERROR;
			}

			/*
			 * DEVICE ISSUE:
			 * We found some devices will return hmac vale with all
			 * zeros.
			 * For this kind of device, bypass hmac comparison.
			 */
			if (ret == RPMB_HMAC_ERROR) {
				for (i = 0; i < 32; i++) {
					if (data.ocmd.frames->key_mac[i]
						!= 0x0) {
						MSG(ERR,
					"%s, device hmac is not NULL!!!\n",
							__func__);
						break;
					}
				}

				MSG(ERR,
				"%s, device hmac has all zero, bypassed!!!\n",
					__func__);
				ret = RPMB_SUCCESS;
			}
		}

		/*
		 * Verify nonce and result only in self-prepared frame
		 * External frame shall be verified by frame provider,
		 * for example, TEE.
		 */
		if (!frame) {
			if (memcmp(nonce, data.ocmd.frames->nonce,
				RPMB_SZ_NONCE) != 0) {
				MSG(ERR, "%s, nonce compare error!!!\n",
					__func__);
				rpmb_dump_frame((u8 *)data.ocmd.frames);
				ret = RPMB_NONCE_ERROR;
				break;
			}

			if (data.ocmd.frames->result) {
				MSG(ERR, "%s, result error!!! (0x%x)\n",
				  __func__,
				cpu_to_be16(data.ocmd.frames->result));
				ret = RPMB_RESULT_ERROR;
				break;
			}
		}

		if (wc) {
			*wc = cpu_to_be32(data.ocmd.frames->write_counter);
			MSG(DBG_INFO, "%s: wc = %d (0x%x)\n",
				__func__, *wc, *wc);
		}
	} while (0);

	MSG(DBG_INFO, "%s: end\n", __func__);

	if (!frame) {
		kfree(data.icmd.frames);
		kfree(data.ocmd.frames);
	}

	return ret;
}

int rpmb_req_read_data_ufs(u8 *frame, u32 blk_cnt)
{
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb;
	int ret;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	MSG(DBG_INFO, "%s: blk_cnt: %d\n", __func__, blk_cnt);

	data.req_type = RPMB_READ_DATA;
	data.icmd.nframes = 1;
	data.icmd.frames = (struct rpmb_frame *)frame;

	/*
	 * We need to fill-in block_count by ourselves for UFS case.
	 * TEE does not fill-in this field because eMMC spec specifiy it as 0.
	 */
	data.icmd.frames->block_count = cpu_to_be16(blk_cnt);

	data.ocmd.nframes = blk_cnt;
	data.ocmd.frames = (struct rpmb_frame *)frame;

	ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);

	if (ret)
		MSG(ERR, "%s: rpmb_cmd_req IO error, ret %d (0x%x)\n",
			__func__, ret, ret);

	MSG(DBG_INFO, "%s: result 0x%x\n", __func__,
		cpu_to_be16(data.ocmd.frames->result));

	MSG(DBG_INFO, "%s: ret 0x%x\n", __func__, ret);

	return ret;
}

int rpmb_req_write_data_ufs(u8 *frame, u32 blk_cnt)
{
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb;
	int ret;
#ifdef __RPMB_MTK_DEBUG_HMAC_VERIFY
	u8 *key_mac;
#endif

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	MSG(DBG_INFO, "%s: blk_cnt: %d\n", __func__, blk_cnt);

	/*
	 * Alloc output frame to avoid overwriting input frame
	 * buffer provided by TEE
	 */
	data.ocmd.frames = rpmb_alloc_frames(1);

	if (data.ocmd.frames == NULL)
		return RPMB_ALLOC_ERROR;

	data.ocmd.nframes = 1;

	data.req_type = RPMB_WRITE_DATA;
	data.icmd.nframes = blk_cnt;
	data.icmd.frames = (struct rpmb_frame *)frame;

#ifdef __RPMB_MTK_DEBUG_HMAC_VERIFY
	key_mac = kzalloc(32, 0);

	rpmb_cal_hmac((struct rpmb_frame *)frame, blk_cnt, rpmb_key, key_mac);

	if (memcmp(key_mac,
		((struct rpmb_frame *)frame)[blk_cnt - 1].key_mac, 32)) {
		MSG(ERR, "%s, Key Mac is NOT matched!\n", __func__);
		kfree(key_mac);
		ret = 1;
		goto out;
	} else
		MSG(ERR, "%s, Key Mac check passed.\n", __func__);

	kfree(key_mac);
#endif

	ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);

	if (ret)
		MSG(ERR, "%s: rpmb_cmd_req IO error, ret %d (0x%x)\n",
			__func__, ret, ret);

	/*
	 * Microtrust TEE will check write counter in the first frame,
	 * thus we copy response frame to the first frame.
	 */
	memcpy(frame, data.ocmd.frames, 512);

	MSG(DBG_INFO, "%s: result 0x%x\n", __func__,
		cpu_to_be16(data.ocmd.frames->result));

	kfree(data.ocmd.frames);

	MSG(DBG_INFO, "%s: ret 0x%x\n", __func__, ret);

#ifdef __RPMB_MTK_DEBUG_HMAC_VERIFY
out:
#endif

	return ret;
}

#ifdef CFG_RPMB_KEY_PROGRAMED_IN_KERNEL
int rpmb_req_program_key_ufs(u8 *frame, u32 blk_cnt)
{
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb;
	int ret;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	MSG(DBG_INFO, "%s: blk_cnt: %d\n", __func__, blk_cnt);

	/*
	 * Alloc output frame to avoid overwriting input frame
	 * buffer provided by TEE
	 */
	data.ocmd.frames = rpmb_alloc_frames(1);

	if (data.ocmd.frames == NULL)
		return RPMB_ALLOC_ERROR;

	data.ocmd.nframes = 1;

	data.req_type = RPMB_PROGRAM_KEY;
	data.icmd.nframes = 1;
	data.icmd.frames = (struct rpmb_frame *)frame;

	ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);

	if (ret)
		MSG(ERR, "%s: rpmb_cmd_req IO error, ret %d (0x%x)\n",
			__func__, ret, ret);

	/*
	 * Microtrust TEE will check write counter in the first frame,
	 * thus we copy response frame to the first frame.
	 */
	memcpy(frame, data.ocmd.frames, 512);

	if (data.ocmd.frames->result) {
		MSG(ERR, "%s, result error!!! (%x)\n", __func__,
			cpu_to_be16(data.ocmd.frames->result));
		ret = RPMB_RESULT_ERROR;
	}

	kfree(data.ocmd.frames);

	MSG(DBG_INFO, "%s: ret 0x%x\n", __func__, ret);

	return ret;
}
#endif

int rpmb_req_ioctl_write_data_ufs(struct rpmb_ioc_param *param)
{
	int err = 0;
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb;
	u32 tran_size, left_size = param->data_len;
	u32 wc = 0xFFFFFFFF;
	u16 iCnt, tran_blkcnt, left_blkcnt;
	u16 blkaddr;
	u8 hmac[RPMB_SZ_MAC];
	u8 *dataBuf, *dataBuf_start;
	u8 key[32];
	u32 size_for_hmac;
	int i, ret = 0;
	u8 user_param_data;

	MSG(DBG_INFO, "%s start!!!\n", __func__);

	if (get_user(user_param_data, param->data))
		return -EFAULT;

	if (get_user(user_param_data, param->key))
		return -EFAULT;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	i = 0;
	tran_blkcnt = 0;
	dataBuf = NULL;
	dataBuf_start = NULL;

	/* Get user key */
	err = copy_from_user(key, param->key, 32);
	if (err) {
		MSG(ERR, "%s, copy from user failed: %x\n", __func__, err);
		return -EFAULT;
	}

	left_blkcnt = ((param->data_len % RPMB_SZ_DATA) ?
					(param->data_len / RPMB_SZ_DATA + 1) :
					(param->data_len / RPMB_SZ_DATA));

	/*
	 * For RPMB write data, the elements we need in the input data frame is
	 * 1. address.
	 * 2. write counter.
	 * 3. data.
	 * 4. block count.
	 * 5. MAC
	 */

	blkaddr = param->addr;

	while (left_blkcnt) {

#if (MAX_RPMB_TRANSFER_BLK > 1)
		if (left_blkcnt >= MAX_RPMB_TRANSFER_BLK)
			tran_blkcnt = MAX_RPMB_TRANSFER_BLK;
		else
			tran_blkcnt = left_blkcnt;
#else
		tran_blkcnt = 1;
#endif

		MSG(DBG_INFO, "%s, total_blkcnt = 0x%x, tran_blkcnt = 0x%x\n",
			__func__, left_blkcnt, tran_blkcnt);

		ret = rpmb_req_get_wc_ufs(key, &wc, NULL);
		if (ret) {
			MSG(ERR, "%s, rpmb_req_get_wc_ufs error!!!(0x%x)\n",
				__func__, ret);
			return ret;
		}

		/*
		 * Initial frame buffers
		 */

		data.icmd.frames = rpmb_alloc_frames(tran_blkcnt);

		if (data.icmd.frames == NULL)
			return RPMB_ALLOC_ERROR;

		data.ocmd.frames = rpmb_alloc_frames(1);

		if (data.ocmd.frames == NULL) {
			kfree(data.icmd.frames);
			return RPMB_ALLOC_ERROR;
		}

		/*
		 * Initial data buffer for HMAC computation.
		 * Since HAMC computation tool which we use needs consecutive
		 * data buffer.Pre-alloced it.
		 */

		dataBuf_start = dataBuf = kzalloc(284 * tran_blkcnt, 0);
		if (!dataBuf_start) {
			kfree(data.icmd.frames);
			kfree(data.ocmd.frames);
			return RPMB_ALLOC_ERROR;
		}

		/*
		 * Prepare frame contents
		 */

		data.req_type = RPMB_WRITE_DATA;


		/* Output frames (in view of device) */

		data.ocmd.nframes = 1;

		/*
		 * All input frames (in view of device) need below stuff,
		 * 1. address.
		 * 2. write counter.
		 * 3. data.
		 * 4. block count.
		 * 5. MAC
		 */

		data.icmd.nframes = tran_blkcnt;

		/* size for hmac calculation: 512 - 228 = 284 */
		size_for_hmac =
		sizeof(struct rpmb_frame) - offsetof(struct rpmb_frame, data);

		for (iCnt = 0; iCnt < tran_blkcnt; iCnt++) {

			/*
			 * Prepare write data frame. need addr, wc, blkcnt,
			 * data and mac.
			 */
			data.icmd.frames[iCnt].req_resp =
				cpu_to_be16(RPMB_WRITE_DATA);
			data.icmd.frames[iCnt].addr = cpu_to_be16(blkaddr);
			data.icmd.frames[iCnt].block_count =
				cpu_to_be16(tran_blkcnt);
			data.icmd.frames[iCnt].write_counter = cpu_to_be32(wc);

			if (left_size >= RPMB_SZ_DATA)
				tran_size = RPMB_SZ_DATA;
			else
				tran_size = left_size;

			err = copy_from_user(data.icmd.frames[iCnt].data,
				(param->data +
				 i * MAX_RPMB_TRANSFER_BLK * RPMB_SZ_DATA +
				 (iCnt * RPMB_SZ_DATA)),
				 tran_size);
			if (err) {
				MSG(ERR, "%s, copy from user failed: %x\n",
					__func__, err);
				ret = -EFAULT;
				goto out;
			}

			left_size -= tran_size;

			rpmb_req_copy_data_for_hmac(
				dataBuf, &data.icmd.frames[iCnt]);

			dataBuf += size_for_hmac;
		}

		iCnt--;

		hmac_sha256(key, 32, dataBuf_start, 284 * tran_blkcnt,
			data.icmd.frames[iCnt].key_mac);

		/*
		 * Send write data request.
		 */

		ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);

		if (ret) {
			MSG(ERR, "%s, rpmb_cmd_req IO error!!!(0x%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * Authenticate write result response.
		 * 1. authenticate hmac.
		 * 2. check result.
		 * 3. compare write counter is increamented.
		 */
		hmac_sha256(key, 32, data.ocmd.frames->data, 284, hmac);

		if (memcmp(hmac, data.ocmd.frames->key_mac, RPMB_SZ_MAC) != 0) {
			MSG(ERR, "%s, hmac compare error!!!\n", __func__);
			ret = RPMB_HMAC_ERROR;
			break;
		}

		if (data.ocmd.frames->result) {
			MSG(ERR, "%s, result error!!! (0x%x)\n", __func__,
				cpu_to_be16(data.ocmd.frames->result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		if (cpu_to_be32(data.ocmd.frames->write_counter) != wc + 1) {
			MSG(ERR, "%s, write counter error!!! (0x%x)\n",
				__func__,
				cpu_to_be32(data.ocmd.frames->write_counter));
			ret = RPMB_WC_ERROR;
			break;
		}

		blkaddr += tran_blkcnt;
		left_blkcnt -= tran_blkcnt;
		i++;

		kfree(data.icmd.frames);
		kfree(data.ocmd.frames);
		kfree(dataBuf_start);
	};

out:
	if (ret) {
		kfree(data.icmd.frames);
		kfree(data.ocmd.frames);
		kfree(dataBuf_start);
	}

	if (left_blkcnt || left_size) {
		MSG(ERR, "left_blkcnt or left_size is not empty!!!!!!\n");
		return RPMB_TRANSFER_NOT_COMPLETE;
	}

	MSG(DBG_INFO, "%s end!!!\n", __func__);

	return ret;
}

int rpmb_req_ioctl_read_data_ufs(struct rpmb_ioc_param *param)
{
	int err = 0;
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb;
	u32 tran_size, left_size = param->data_len;
	u16 iCnt, tran_blkcnt, left_blkcnt;
	u16 blkaddr;
	u8 nonce[RPMB_SZ_NONCE] = {0};
	u8 hmac[RPMB_SZ_MAC];
	u8 *dataBuf, *dataBuf_start;
	u8 key[32];
	u32 size_for_hmac;
	int i, ret = 0;
	u8 user_param_data;

	MSG(DBG_INFO, "%s start!!!\n", __func__);

	if (get_user(user_param_data, param->data))
		return -EFAULT;

	if (get_user(user_param_data, param->key))
		return -EFAULT;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	i = 0;
	tran_blkcnt = 0;
	dataBuf = NULL;
	dataBuf_start = NULL;

	/* Get user key */
	err = copy_from_user(key, param->key, 32);
	if (err) {
		MSG(ERR, "%s, copy from user failed: %x\n", __func__, err);
		return -EFAULT;
	}

	left_blkcnt = ((param->data_len % RPMB_SZ_DATA) ?
					(param->data_len / RPMB_SZ_DATA + 1) :
					(param->data_len / RPMB_SZ_DATA));

	blkaddr = param->addr;

	while (left_blkcnt) {

#if (MAX_RPMB_TRANSFER_BLK > 1)
		if (left_blkcnt >= MAX_RPMB_TRANSFER_BLK)
			tran_blkcnt = MAX_RPMB_TRANSFER_BLK;
		else
			tran_blkcnt = left_blkcnt;
#else
		tran_blkcnt = 1;
#endif

		MSG(DBG_INFO, "%s, left_blkcnt = 0x%x, tran_blkcnt = 0x%x\n",
			__func__, left_blkcnt, tran_blkcnt);

		/*
		 * initial frame buffers
		 */

		data.icmd.frames = rpmb_alloc_frames(1);

		if (data.icmd.frames == NULL)
			return RPMB_ALLOC_ERROR;

		data.ocmd.frames = rpmb_alloc_frames(tran_blkcnt);

		if (data.ocmd.frames == NULL) {
			kfree(data.icmd.frames);
			return RPMB_ALLOC_ERROR;
		}

		/*
		 * Initial data buffer for HMAC computation.
		 * Since HAMC computation tool which we use needs consecutive
		 * data buffer.Pre-alloced it.
		 */

		dataBuf_start = dataBuf = kzalloc(284 * tran_blkcnt, 0);
		if (!dataBuf_start) {
			kfree(data.icmd.frames);
			kfree(data.ocmd.frames);
			return RPMB_ALLOC_ERROR;
		}

		get_random_bytes(nonce, RPMB_SZ_NONCE);

		/*
		 * Prepare request read data frame.
		 *
		 * Input frame (in view of device) only needs addr and nonce.
		 */

		data.req_type = RPMB_READ_DATA;
		data.icmd.nframes = 1;
		data.icmd.frames->req_resp = cpu_to_be16(RPMB_READ_DATA);
		data.icmd.frames->addr = cpu_to_be16(blkaddr);
		data.icmd.frames->block_count = cpu_to_be16(tran_blkcnt);
		memcpy(data.icmd.frames->nonce, nonce, RPMB_SZ_NONCE);

		/* output frames (in view of device) */

		data.ocmd.nframes = tran_blkcnt;

		ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);

		if (ret) {
			MSG(ERR, "%s, rpmb_cmd_req IO error!!!(0x%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * Retrieve every data frame one by one.
		 */

		/* size for hmac calculation: 512 - 228 = 284 */
		size_for_hmac =
		sizeof(struct rpmb_frame) - offsetof(struct rpmb_frame, data);

		for (iCnt = 0; iCnt < tran_blkcnt; iCnt++) {

			if (left_size >= RPMB_SZ_DATA)
				tran_size = RPMB_SZ_DATA;
			else
				tran_size = left_size;

			/*
			 * dataBuf used for hmac calculation. we need to
			 * aggregate each block's data till to type field.
			 * each block has 284 bytes (size_for_hmac)
			 * need aggregation.
			 */
			rpmb_req_copy_data_for_hmac(
				dataBuf, &data.ocmd.frames[iCnt]);

			dataBuf += size_for_hmac;

			err = copy_to_user(
				(param->data +
				 i * MAX_RPMB_TRANSFER_BLK * RPMB_SZ_DATA +
				 (iCnt * RPMB_SZ_DATA)),
				 data.ocmd.frames[iCnt].data,
				 tran_size);
			if (err) {
				MSG(ERR, "%s, copy to user failed: %x\n",
					__func__, err);
				ret = -EFAULT;
				goto out;
			}
			left_size -= tran_size;
		}

		iCnt--;

		/*
		 * Authenticate response read data frame.
		 */
		hmac_sha256(key,
			32, dataBuf_start, size_for_hmac * tran_blkcnt, hmac);

		if (memcmp(hmac, data.ocmd.frames[iCnt].key_mac, RPMB_SZ_MAC)
			!= 0) {
			MSG(ERR, "%s, hmac compare error!!!\n", __func__);
			ret = RPMB_HMAC_ERROR;
			break;
		}

		if (memcmp(nonce, data.ocmd.frames[iCnt].nonce, RPMB_SZ_NONCE)
			!= 0) {
			MSG(ERR, "%s, nonce compare error!!!\n", __func__);
			ret = RPMB_NONCE_ERROR;
			break;
		}

		if (data.ocmd.frames[iCnt].result) {
			MSG(ERR, "%s, result error!!! (0x%x)\n",
			  __func__,
			cpu_to_be16p(&data.ocmd.frames[iCnt].result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		blkaddr += tran_blkcnt;
		left_blkcnt -= tran_blkcnt;
		i++;

		kfree(data.icmd.frames);
		kfree(data.ocmd.frames);
		kfree(dataBuf_start);
	};

out:
	if (ret) {
		kfree(data.icmd.frames);
		kfree(data.ocmd.frames);
		kfree(dataBuf_start);
	}

	if (left_blkcnt || left_size) {
		MSG(ERR, "left_blkcnt or left_size is not empty!!!!!!\n");
		return RPMB_TRANSFER_NOT_COMPLETE;
	}

	MSG(DBG_INFO, "%s end!!!\n", __func__);

	return ret;
}
#endif

int rpmb_req_get_wc_emmc(struct mmc_card *card, u8 *key, u32 *wc)
{
	struct emmc_rpmb_req rpmb_req;
	struct s_rpmb *rpmb_frame;
	u8 nonce[RPMB_SZ_NONCE] = {0};
	u8 hmac[RPMB_SZ_MAC];
	int ret;

	MSG(INFO, "%s start!!!\n", __func__);

	do {
		rpmb_frame = kzalloc(sizeof(struct s_rpmb), 0);
		if (rpmb_frame == NULL)
			return RPMB_ALLOC_ERROR;

		get_random_bytes(nonce, RPMB_SZ_NONCE);

		/*
		 * Prepare request. Get write counter.
		 */
		rpmb_req.type = RPMB_GET_WRITE_COUNTER;
		rpmb_req.blk_cnt = 1;
		rpmb_req.data_frame = (u8 *)rpmb_frame;

		/*
		 * Prepare get write counter frame. only need nonce.
		 */
		rpmb_frame->request = cpu_to_be16p(&rpmb_req.type);
		memcpy(rpmb_frame->nonce, nonce, RPMB_SZ_NONCE);

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * Authenticate response write counter frame.
		 */
		if (key) {
			hmac_sha256(key, 32, rpmb_frame->data, 284, hmac);
			if (memcmp(hmac, rpmb_frame->mac, RPMB_SZ_MAC) != 0) {
				MSG(ERR, "%s, hmac compare error!!!\n",
					__func__);
				ret = RPMB_HMAC_ERROR;
				break;
			}
		}

		if (memcmp(nonce, rpmb_frame->nonce, RPMB_SZ_NONCE) != 0) {
			MSG(ERR, "%s, nonce compare error!!!\n", __func__);
			ret = RPMB_NONCE_ERROR;
			break;
		}

		if (rpmb_frame->result) {
			MSG(ERR, "%s, result error!!! (%x)\n", __func__,
				cpu_to_be16p(&rpmb_frame->result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		*wc = cpu_to_be32p(&rpmb_frame->write_counter);

	} while (0);

	MSG(INFO, "%s end!!!\n", __func__);

	kfree(rpmb_frame);

	return ret;
}

int rpmb_req_ioctl_write_data_emmc(struct mmc_card *card,
	struct rpmb_ioc_param *param)
{
	struct emmc_rpmb_req rpmb_req;
	struct s_rpmb *rpmb_frame;
	u32 tran_size, left_size = param->data_len;
	u32 wc = 0xFFFFFFFF;
	u16 iCnt, total_blkcnt, tran_blkcnt, left_blkcnt;
	u16 blkaddr;
	u8 hmac[RPMB_SZ_MAC];
	u8 *dataBuf, *dataBuf_start;
	int i, ret = 0;
#ifdef RPMB_MULTI_BLOCK_ACCESS
	u8 write_blks_one_time = 0;
	u32 size_for_hmac;
#endif

	MSG(INFO, "%s start!!!\n", __func__);

	i = 0;
	tran_blkcnt = 0;
	dataBuf = NULL;
	dataBuf_start = NULL;

	left_blkcnt = total_blkcnt = ((param->data_len % RPMB_SZ_DATA) ?
					(param->data_len / RPMB_SZ_DATA + 1) :
					(param->data_len / RPMB_SZ_DATA));


#ifdef RPMB_MULTI_BLOCK_ACCESS

	/*
	 * For RPMB write data, the elements we need in the data frame is
	 * 1. address.
	 * 2. write counter.
	 * 3. data.
	 * 4. block count.
	 * 5. MAC
	 *
	 */

	blkaddr = param->addr;
	write_blks_one_time = MIN(MAX_RPMB_TRANSFER_BLK,
			card->ext_csd.rel_sectors * 2);
	while (left_blkcnt) {

		if (left_blkcnt > write_blks_one_time)
			tran_blkcnt = write_blks_one_time;
		else
			tran_blkcnt = left_blkcnt;

		MSG(INFO, "%s, total_blkcnt=%x, tran_blkcnt=%x\n",
			__func__, left_blkcnt, tran_blkcnt);

		ret = rpmb_req_get_wc_emmc(card, param->key, &wc);
		if (ret) {
			MSG(ERR, "%s, rpmb_req_get_wc_emmc error!!!(%x)\n",
				__func__, ret);
			return ret;
		}

		rpmb_frame = kzalloc(tran_blkcnt * sizeof(struct s_rpmb)
			+ tran_blkcnt * 512, 0);
		if (rpmb_frame == NULL)
			return RPMB_ALLOC_ERROR;

		dataBuf_start = dataBuf = (u8 *)(rpmb_frame + tran_blkcnt);

		/*
		 * Prepare request. write data.
		 */
		rpmb_req.type = RPMB_WRITE_DATA;
		rpmb_req.blk_cnt = tran_blkcnt;
		rpmb_req.data_frame = (u8 *)rpmb_frame;

		/*
		 * STEP 3(data), prepare every data frame one by one and
		 * hook HMAC to the last.
		 */

		/* size for hmac calculation: 512 - 228 = 284 */
		size_for_hmac =
		sizeof(struct rpmb_frame) - offsetof(struct rpmb_frame, data);

		for (iCnt = 0; iCnt < tran_blkcnt; iCnt++) {

			/*
			 * Prepare write data frame. need addr, wc,
			 * blkcnt, data and mac.
			 */
			rpmb_frame[iCnt].request = cpu_to_be16p(&rpmb_req.type);
			rpmb_frame[iCnt].address = cpu_to_be16p(&blkaddr);
			rpmb_frame[iCnt].write_counter = cpu_to_be32p(&wc);
			rpmb_frame[iCnt].block_count =
				cpu_to_be16p(&rpmb_req.blk_cnt);

			if (left_size >= RPMB_SZ_DATA)
				tran_size = RPMB_SZ_DATA;
			else
				tran_size = left_size;

			memcpy(rpmb_frame[iCnt].data,
				 param->data + (iCnt * RPMB_SZ_DATA)
				 + i * write_blks_one_time * RPMB_SZ_DATA,
				 tran_size);
			left_size -= tran_size;

			rpmb_req_copy_data_for_hmac(dataBuf,
				(struct rpmb_frame *) &rpmb_frame[iCnt]);

			dataBuf += size_for_hmac;
		}

		iCnt--;

		hmac_sha256(param->key, 32, dataBuf_start, 284 * tran_blkcnt,
				rpmb_frame[iCnt].mac);

		/*
		 * STEP 4, send write data request.
		 */
		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * STEP 5. authenticate write result response.
		 * 1. authenticate hmac.
		 * 2. check result.
		 * 3. compare write counter is increamented.
		 */
		hmac_sha256(param->key, 32, rpmb_frame->data, 284, hmac);

		if (memcmp(hmac, rpmb_frame->mac, RPMB_SZ_MAC) != 0) {
			MSG(ERR, "%s, hmac compare error!!!\n", __func__);
			ret = RPMB_HMAC_ERROR;
			break;
		}

		if (rpmb_frame->result) {
			MSG(ERR, "%s, result error!!! (%x)\n", __func__,
				cpu_to_be16p(&rpmb_frame->result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		if (cpu_to_be32p(&rpmb_frame->write_counter) != wc + 1) {
			MSG(ERR, "%s, write counter error!!! (%x)\n", __func__,
				cpu_to_be32p(&rpmb_frame->write_counter));
			ret = RPMB_WC_ERROR;
			break;
		}

		blkaddr += tran_blkcnt;
		left_blkcnt -= tran_blkcnt;
		i++;
		kfree(rpmb_frame);
	};

	if (ret)
		kfree(rpmb_frame);

	if (left_blkcnt || left_size) {
		MSG(ERR, "left_blkcnt or left_size is not empty!!!!!!\n");
		return RPMB_TRANSFER_NOT_COMPLETE;
	}

#else
	rpmb_frame = kzalloc(sizeof(struct s_rpmb), 0);
	if (rpmb_frame == NULL)
		return RPMB_ALLOC_ERROR;

	blkaddr = param->addr;

	for (iCnt = 0; iCnt < total_blkcnt; iCnt++) {

		ret = rpmb_req_get_wc_emmc(card, param->key, &wc);
		if (ret)
			break;

		memset(rpmb_frame, 0, sizeof(struct s_rpmb));

		/*
		 * Prepare request. write data.
		 */
		rpmb_req.type = RPMB_WRITE_DATA;
		rpmb_req.blk_cnt = 1;
		rpmb_req.data_frame = (u8 *)rpmb_frame;

		/*
		 * Prepare write data frame. need addr, wc,
		 * blkcnt, data and mac.
		 */
		rpmb_frame->request = cpu_to_be16p(&rpmb_req.type);
		rpmb_frame->address = cpu_to_be16p(&blkaddr);
		rpmb_frame->write_counter = cpu_to_be32p(&wc);
		rpmb_frame->block_count = cpu_to_be16p(&rpmb_req.blk_cnt);

		if (left_size >= RPMB_SZ_DATA)
			tran_size = RPMB_SZ_DATA;
		else
			tran_size = left_size;

		memcpy(rpmb_frame->data,
			param->data + iCnt * RPMB_SZ_DATA, tran_size);

		hmac_sha256(param->key, 32, rpmb_frame->data, 284,
			rpmb_frame->mac);

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * Authenticate response write data frame.
		 */
		hmac_sha256(param->key, 32, rpmb_frame->data, 284, hmac);

		if (memcmp(hmac, rpmb_frame->mac, RPMB_SZ_MAC) != 0) {
			MSG(ERR, "%s, hmac compare error!!!\n", __func__);
			ret = RPMB_HMAC_ERROR;
			break;
		}

		if (rpmb_frame->result) {
			MSG(ERR, "%s, result error!!! (%x)\n", __func__,
				cpu_to_be16p(&rpmb_frame->result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		if (cpu_to_be32p(&rpmb_frame->write_counter) != wc + 1) {
			MSG(ERR, "%s, write counter error!!! (%x)\n", __func__,
				cpu_to_be32p(&rpmb_frame->write_counter));
			ret = RPMB_WC_ERROR;
			break;
		}

		left_size -= tran_size;
		blkaddr++;
	}

	kfree(rpmb_frame);

#endif

	MSG(INFO, "%s end!!!\n", __func__);

	return ret;
}

int rpmb_req_ioctl_read_data_emmc(struct mmc_card *card,
	struct rpmb_ioc_param *param)
{
	struct emmc_rpmb_req rpmb_req;
	/* if we put a large static buffer here, it will build fail.
	 * rpmb_frame[MAX_RPMB_TRANSFER_BLK];
	 * so I use dynamic alloc.
	 */
	struct s_rpmb *rpmb_frame;
	u32 tran_size, left_size = param->data_len;
	u16 iCnt, total_blkcnt, tran_blkcnt, left_blkcnt;
	u16 blkaddr;
	u8 nonce[RPMB_SZ_NONCE] = {0};
	u8 hmac[RPMB_SZ_MAC];
	u8 *dataBuf, *dataBuf_start;
	int i, ret = 0;
#ifdef RPMB_MULTI_BLOCK_ACCESS
	u32 size_for_hmac;
#endif
	MSG(INFO, "%s start!!!\n", __func__);

	i = 0;
	tran_blkcnt = 0;
	dataBuf = NULL;
	dataBuf_start = NULL;
	left_blkcnt = total_blkcnt = ((param->data_len % RPMB_SZ_DATA) ?
					(param->data_len / RPMB_SZ_DATA + 1) :
					(param->data_len / RPMB_SZ_DATA));

#ifdef RPMB_MULTI_BLOCK_ACCESS

	blkaddr = param->addr;

	while (left_blkcnt) {

		if (left_blkcnt >= MAX_RPMB_TRANSFER_BLK)
			tran_blkcnt = MAX_RPMB_TRANSFER_BLK;
		else
			tran_blkcnt = left_blkcnt;

		MSG(INFO, "%s, left_blkcnt=%x, tran_blkcnt=%x\n", __func__,
			left_blkcnt, tran_blkcnt);

		/*
		 * initial buffer. (since HMAC computation of multi block needs
		 * multi buffer, pre-alloced it)
		 */
		rpmb_frame =
	kzalloc(tran_blkcnt * sizeof(struct s_rpmb) + tran_blkcnt * 512, 0);
		if (rpmb_frame == NULL)
			return RPMB_ALLOC_ERROR;

		dataBuf_start = dataBuf = (u8 *)(rpmb_frame + tran_blkcnt);

		get_random_bytes(nonce, RPMB_SZ_NONCE);

		/*
		 * Prepare request.
		 */
		rpmb_req.type = RPMB_READ_DATA;
		rpmb_req.blk_cnt = tran_blkcnt;
		rpmb_req.data_frame = (u8 *)rpmb_frame;

		/*
		 * Prepare request read data frame. only need addr and nonce.
		 */
		rpmb_frame->request = cpu_to_be16p(&rpmb_req.type);
		rpmb_frame->address = cpu_to_be16p(&blkaddr);
		memcpy(rpmb_frame->nonce, nonce, RPMB_SZ_NONCE);

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * STEP 3, retrieve every data frame one by one.
		 */

		/* size for hmac calculation: 512 - 228 = 284 */
		size_for_hmac =
		sizeof(struct rpmb_frame) - offsetof(struct rpmb_frame, data);

		for (iCnt = 0; iCnt < tran_blkcnt; iCnt++) {

			if (left_size >= RPMB_SZ_DATA)
				tran_size = RPMB_SZ_DATA;
			else
				tran_size = left_size;

			/*
			 * dataBuf used for hmac calculation. we need to
			 * aggregate each block's data till to type field.
			 * each block has 284 bytes need to aggregate.
			 */
			rpmb_req_copy_data_for_hmac(dataBuf,
				(struct rpmb_frame *) &rpmb_frame[iCnt]);

			dataBuf += size_for_hmac;

			/*
			 * sorry, I shouldn't copy read data to user's buffer
			 * now, it should be later
			 * after checking no problem,
			 * but for convenience...you know...
			 */
			memcpy(
param->data + i * MAX_RPMB_TRANSFER_BLK * RPMB_SZ_DATA + (iCnt * RPMB_SZ_DATA),
				 rpmb_frame[iCnt].data,
				 tran_size);
			left_size -= tran_size;
		}

		iCnt--;

		/*
		 * Authenticate response read data frame.
		 */
		hmac_sha256(param->key,
			32, dataBuf_start, 284 * tran_blkcnt, hmac);

		if (memcmp(hmac, rpmb_frame[iCnt].mac, RPMB_SZ_MAC) != 0) {
			MSG(ERR, "%s, hmac compare error!!!\n", __func__);
			ret = RPMB_HMAC_ERROR;
			break;
		}

		if (memcmp(nonce, rpmb_frame[iCnt].nonce, RPMB_SZ_NONCE) != 0) {
			MSG(ERR, "%s, nonce compare error!!!\n", __func__);
			ret = RPMB_NONCE_ERROR;
			break;
		}

		if (rpmb_frame[iCnt].result) {
			MSG(ERR, "%s, result error!!! (%x)\n", __func__,
				cpu_to_be16p(&rpmb_frame[iCnt].result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		blkaddr += tran_blkcnt;
		left_blkcnt -= tran_blkcnt;
		i++;
		kfree(rpmb_frame);
	};

	if (ret)
		kfree(rpmb_frame);

	if (left_blkcnt || left_size) {
		MSG(ERR, "left_blkcnt or left_size is not empty!!!!!!\n");
		return RPMB_TRANSFER_NOT_COMPLETE;
	}

#else

	rpmb_frame = kzalloc(sizeof(struct s_rpmb), 0);
	if (rpmb_frame == NULL)
		return RPMB_ALLOC_ERROR;

	blkaddr = param->addr;

	for (iCnt = 0; iCnt < total_blkcnt; iCnt++) {

		memset(rpmb_frame, 0, sizeof(struct s_rpmb));
		get_random_bytes(nonce, RPMB_SZ_NONCE);

		/*
		 * Prepare request.
		 */
		rpmb_req.type = RPMB_READ_DATA;
		rpmb_req.blk_cnt = 1;
		rpmb_req.data_frame = (u8 *)rpmb_frame;

		/*
		 * Prepare request read data frame. only need addr and nonce.
		 */
		rpmb_frame->request = cpu_to_be16p(&rpmb_req.type);
		rpmb_frame->address = cpu_to_be16p(&blkaddr);
		memcpy(rpmb_frame->nonce, nonce, RPMB_SZ_NONCE);

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * Authenticate response read data frame.
		 */
		hmac_sha256(param->key, 32, rpmb_frame->data, 284, hmac);

		if (memcmp(hmac, rpmb_frame->mac, RPMB_SZ_MAC) != 0) {
			MSG(ERR, "%s, hmac compare error!!!\n", __func__);
			ret = RPMB_HMAC_ERROR;
			break;
		}

		if (memcmp(nonce, rpmb_frame->nonce, RPMB_SZ_NONCE) != 0) {
			MSG(ERR, "%s, nonce compare error!!!\n", __func__);
			ret = RPMB_NONCE_ERROR;
			break;
		}

		if (rpmb_frame->result) {
			MSG(ERR, "%s, result error!!! (%x)\n", __func__,
				cpu_to_be16p(&rpmb_frame->result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		if (left_size >= RPMB_SZ_DATA)
			tran_size = RPMB_SZ_DATA;
		else
			tran_size = left_size;

		memcpy(param->data + RPMB_SZ_DATA * iCnt,
			rpmb_frame->data, tran_size);

		left_size -= tran_size;
		blkaddr++;
	}

	kfree(rpmb_frame);

#endif

	MSG(INFO, "%s end!!!\n", __func__);

	return ret;
}

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
int ut_rpmb_req_get_max_wr_size(struct mmc_card *card,
	unsigned int *max_wr_size)
{
	*max_wr_size = card->ext_csd.rel_sectors;

	return 0;
}
int ut_rpmb_req_get_wc(struct mmc_card *card, unsigned int *wc)
{
	struct emmc_rpmb_req rpmb_req;
	struct s_rpmb rpmb_frame;
	u8 nonce[RPMB_SZ_NONCE] = {0};
	int ret;

	memset(&rpmb_frame, 0, sizeof(rpmb_frame));
	get_random_bytes(nonce, RPMB_SZ_NONCE);

	/*
	 * Prepare request. Get write counter.
	 */
	rpmb_req.type = RPMB_GET_WRITE_COUNTER;
	rpmb_req.blk_cnt = 1;
	rpmb_req.data_frame = (u8 *)&rpmb_frame;

	/*
	 * Prepare get write counter frame. only need nonce.
	 */
	rpmb_frame.request = cpu_to_be16p(&rpmb_req.type);
	memcpy(rpmb_frame.nonce, nonce, RPMB_SZ_NONCE);

	ret = emmc_rpmb_req_handle(card, &rpmb_req);
	if (ret) {
		MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
			__func__, ret);
		return ret;
	}
	if (memcmp(nonce, rpmb_frame.nonce, RPMB_SZ_NONCE) != 0) {
		MSG(ERR, "%s, nonce compare error!!!\n", __func__);
		ret = RPMB_NONCE_ERROR;
		return ret;
	}
	if (rpmb_frame.result) {
		MSG(ERR, "%s, result error!!! (%x)\n", __func__,
			cpu_to_be16p(&rpmb_frame.result));
		ret = RPMB_RESULT_ERROR;
		return cpu_to_be16p(&rpmb_frame.result);
	}
	*wc = cpu_to_be32p(&rpmb_frame.write_counter);
	return ret;
}
EXPORT_SYMBOL(ut_rpmb_req_get_wc);

int ut_rpmb_req_read_data(struct mmc_card *card,
	struct s_rpmb *param, u32 blk_cnt)/*struct mmc_card *card, */
{
	struct emmc_rpmb_req rpmb_req;
	int ret;

	rpmb_req.type = RPMB_READ_DATA;
	rpmb_req.blk_cnt = blk_cnt;
	rpmb_req.data_frame = (u8 *)param;

	ret = emmc_rpmb_req_handle(card, &rpmb_req);
	if (ret)
		MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
			__func__, ret);

	return ret;
}
EXPORT_SYMBOL(ut_rpmb_req_read_data);

int ut_rpmb_req_write_data(struct mmc_card *card,
	struct s_rpmb *param, u32 blk_cnt)/*struct mmc_card *card, */
{
	struct emmc_rpmb_req rpmb_req;
	int ret;

	rpmb_req.type = RPMB_WRITE_DATA;
	rpmb_req.blk_cnt = blk_cnt;
	rpmb_req.data_frame = (u8 *)param;

	ret = emmc_rpmb_req_handle(card, &rpmb_req);
	if (ret)
		MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n",
			__func__, ret);

	return ret;
}
EXPORT_SYMBOL(ut_rpmb_req_write_data);
#endif /* CONFIG_MICROTRUST_TEE_SUPPORT */

/*
 * End of above.
 */


#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT

#ifdef CONFIG_MTK_UFS_SUPPORT
#ifndef CONFIG_TEE
static int rpmb_execute_ufs(u32 cmdId)
{
	int ret;

	switch (cmdId) {

	case DCI_RPMB_CMD_READ_DATA:

		MSG(DBG_INFO, "%s: DCI_RPMB_CMD_READ_DATA\n", __func__);

		ret = rpmb_req_read_data_ufs(rpmb_dci->request.frame,
						rpmb_dci->request.blks);

		break;

	case DCI_RPMB_CMD_GET_WCNT:

		MSG(DBG_INFO, "%s: DCI_RPMB_CMD_GET_WCNT\n", __func__);

		ret = rpmb_req_get_wc_ufs(NULL, NULL, rpmb_dci->request.frame);

		break;

	case DCI_RPMB_CMD_WRITE_DATA:

		MSG(DBG_INFO, "%s: DCI_RPMB_CMD_WRITE_DATA\n", __func__);

		ret = rpmb_req_write_data_ufs(rpmb_dci->request.frame,
						rpmb_dci->request.blks);

		break;

#ifdef CFG_RPMB_KEY_PROGRAMED_IN_KERNEL
	case DCI_RPMB_CMD_PROGRAM_KEY:
		MSG(INFO, "%s: DCI_RPMB_CMD_PROGRAM_KEY.\n", __func__);
		rpmb_dump_frame(rpmb_dci->request.frame);

		ret = rpmb_req_program_key_ufs(rpmb_dci->request.frame, 1);

		break;
#endif

	default:
		MSG(ERR, "%s: receive an unknown command id (%d).\n",
			__func__, cmdId);
		break;
	}

	return 0;
}
#endif

static int rpmb_gp_execute_ufs(u32 cmdId)
{
	int ret;

	switch (cmdId) {

	case DCI_RPMB_CMD_READ_DATA:

		MSG(DBG_INFO, "%s: DCI_RPMB_CMD_READ_DATA\n", __func__);

		ret = rpmb_req_read_data_ufs(rpmb_gp_dci->request.frame,
						rpmb_gp_dci->request.blks);

		break;

	case DCI_RPMB_CMD_GET_WCNT:

		MSG(DBG_INFO, "%s: DCI_RPMB_CMD_GET_WCNT\n", __func__);

		ret = rpmb_req_get_wc_ufs(NULL, NULL,
						rpmb_gp_dci->request.frame);

		break;

	case DCI_RPMB_CMD_WRITE_DATA:

		MSG(DBG_INFO, "%s: DCI_RPMB_CMD_WRITE_DATA\n", __func__);

		ret = rpmb_req_write_data_ufs(rpmb_gp_dci->request.frame,
						rpmb_gp_dci->request.blks);

		break;

#ifdef CFG_RPMB_KEY_PROGRAMED_IN_KERNEL
	case DCI_RPMB_CMD_PROGRAM_KEY:
		MSG(INFO, "%s: DCI_RPMB_CMD_PROGRAM_KEY.\n", __func__);
		rpmb_dump_frame(rpmb_gp_dci->request.frame);

		ret = rpmb_req_program_key_ufs(rpmb_gp_dci->request.frame, 1);

		break;
#endif

	default:
		MSG(ERR, "%s: receive an unknown command id(%d).\n",
			__func__, cmdId);
		break;

	}

	return 0;
}
#endif

#ifndef CONFIG_TEE
static int rpmb_execute_emmc(u32 cmdId)
{
	int ret;

	struct mmc_card *card = mtk_msdc_host[0]->mmc->card;
	struct emmc_rpmb_req rpmb_req;

	switch (cmdId) {

	case DCI_RPMB_CMD_READ_DATA:
		MSG(INFO, "%s: DCI_RPMB_CMD_READ_DATA.\n", __func__);

		rpmb_req.type = RPMB_READ_DATA;
		rpmb_req.blk_cnt = rpmb_dci->request.blks;
		rpmb_req.addr = rpmb_dci->request.addr;
		rpmb_req.data_frame = rpmb_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n",
				__func__, ret);

		break;

	case DCI_RPMB_CMD_GET_WCNT:
		MSG(INFO, "%s: DCI_RPMB_CMD_GET_WCNT.\n", __func__);

		rpmb_req.type = RPMB_GET_WRITE_COUNTER;
		rpmb_req.blk_cnt = rpmb_dci->request.blks;
		rpmb_req.addr = rpmb_dci->request.addr;
		rpmb_req.data_frame = rpmb_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n",
				__func__, ret);

		break;

	case DCI_RPMB_CMD_WRITE_DATA:
		MSG(INFO, "%s: DCI_RPMB_CMD_WRITE_DATA.\n", __func__);

		rpmb_req.type = RPMB_WRITE_DATA;
		rpmb_req.blk_cnt = rpmb_dci->request.blks;
		rpmb_req.addr = rpmb_dci->request.addr;
		rpmb_req.data_frame = rpmb_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n",
				__func__, ret);

		break;

#ifdef CFG_RPMB_KEY_PROGRAMED_IN_KERNEL
	case DCI_RPMB_CMD_PROGRAM_KEY:
		MSG(INFO, "%s: DCI_RPMB_CMD_PROGRAM_KEY.\n", __func__);
		rpmb_dump_frame(rpmb_dci->request.frame);

		rpmb_req.type = RPMB_PROGRAM_KEY;
		/* rpmb_req.blk_cnt = rpmb_dci->request.blks; */
		rpmb_req.blk_cnt = 1;
		rpmb_req.addr = rpmb_dci->request.addr;
		rpmb_req.data_frame = rpmb_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n",
				__func__, ret);

		break;
#endif

	default:
		MSG(ERR, "%s: receive an unknown command id(%d).\n",
			__func__, cmdId);
		break;

	}

	return 0;
}
#endif

static int rpmb_gp_execute_emmc(u32 cmdId)
{
	int ret;

	struct mmc_card *card = mtk_msdc_host[0]->mmc->card;
	struct emmc_rpmb_req rpmb_req;

	switch (cmdId) {

	case DCI_RPMB_CMD_READ_DATA:
		MSG(INFO, "%s: DCI_RPMB_CMD_READ_DATA.\n", __func__);

		rpmb_req.type = RPMB_READ_DATA;
		rpmb_req.blk_cnt = rpmb_gp_dci->request.blks;
		rpmb_req.addr = rpmb_gp_dci->request.addr;
		rpmb_req.data_frame = rpmb_gp_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n",
				__func__, ret);

		break;

	case DCI_RPMB_CMD_GET_WCNT:
		MSG(INFO, "%s: DCI_RPMB_CMD_GET_WCNT.\n", __func__);

		rpmb_req.type = RPMB_GET_WRITE_COUNTER;
		rpmb_req.blk_cnt = rpmb_gp_dci->request.blks;
		rpmb_req.addr = rpmb_gp_dci->request.addr;
		rpmb_req.data_frame = rpmb_gp_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n",
				__func__, ret);

		break;

	case DCI_RPMB_CMD_WRITE_DATA:
		MSG(INFO, "%s: DCI_RPMB_CMD_WRITE_DATA.\n", __func__);

		rpmb_req.type = RPMB_WRITE_DATA;
		rpmb_req.blk_cnt = rpmb_gp_dci->request.blks;
		rpmb_req.addr = rpmb_gp_dci->request.addr;
		rpmb_req.data_frame = rpmb_gp_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n",
				__func__, ret);

		break;

#ifdef CFG_RPMB_KEY_PROGRAMED_IN_KERNEL
	case DCI_RPMB_CMD_PROGRAM_KEY:
		MSG(INFO, "%s: DCI_RPMB_CMD_PROGRAM_KEY.\n", __func__);
		rpmb_dump_frame(rpmb_gp_dci->request.frame);

		rpmb_req.type = RPMB_PROGRAM_KEY;
		rpmb_req.blk_cnt = rpmb_gp_dci->request.blks;
		rpmb_req.addr = rpmb_gp_dci->request.addr;
		rpmb_req.data_frame = rpmb_gp_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n",
				__func__, ret);

		break;
#endif

	default:
		MSG(ERR, "%s: receive an unknown command id(%d).\n",
			__func__, cmdId);
		break;

	}

	return 0;
}

#ifndef CONFIG_TEE
int rpmb_listenDci(void *data)
{
	enum mc_result mc_ret;
	u32 cmdId;
	int boot_type;

	MSG(INFO, "%s: DCI listener.\n", __func__);

	for (;;) {

		MSG(INFO, "%s: Waiting for notification\n", __func__);

		/* Wait for notification from SWd */
		mc_ret = mc_wait_notification(&rpmb_session,
						MC_INFINITE_TIMEOUT);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s: mcWaitNotification failed, mc_ret=%d\n",
				__func__, mc_ret);
			break;
		}

		cmdId = rpmb_dci->command.header.commandId;

		MSG(INFO, "%s: wait notification done!! cmdId = %x\n",
			__func__, cmdId);

		/* Received exception. */
		boot_type = get_boot_type();
		if (boot_type == BOOTDEV_SDMMC)
			mc_ret = rpmb_execute_emmc(cmdId);
#ifdef CONFIG_MTK_UFS_SUPPORT
		else if (boot_type == BOOTDEV_UFS)
			mc_ret = rpmb_execute_ufs(cmdId);
#endif

		/* Notify the STH */
		mc_ret = mc_notify(&rpmb_session);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s: mcNotify returned: %d\n",
				__func__, mc_ret);
			break;
		}
	}

	return 0;
}
#endif

#ifndef CONFIG_TEE
static int rpmb_open_session(void)
{
	int cnt = 0;
	enum mc_result mc_ret = MC_DRV_ERR_UNKNOWN;

	MSG(INFO, "%s start\n", __func__);

	do {
		msleep(2000);

		/* open device */
		mc_ret = mc_open_device(rpmb_devid);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s, mc_open_device failed: %d\n",
				__func__, mc_ret);
			cnt++;
			continue;
		}

		MSG(INFO, "%s, mc_open_device success.\n", __func__);


		/* allocating WSM for DCI */
		mc_ret = mc_malloc_wsm(rpmb_devid, 0,
					sizeof(struct dciMessage_t),
					(uint8_t **)&rpmb_dci, 0);
		if (mc_ret != MC_DRV_OK) {
			mc_close_device(rpmb_devid);
			MSG(ERR, "%s, mc_malloc_wsm failed: %d\n",
				__func__, mc_ret);
			cnt++;
			continue;
		}

		MSG(INFO, "%s, mc_malloc_wsm success.\n", __func__);
		MSG(INFO, "uuid[0]=%d, uuid[1]=%d, uuid[2]=%d, uuid[3]=%d\n",
			rpmb_uuid.value[0], rpmb_uuid.value[1],
			rpmb_uuid.value[2], rpmb_uuid.value[3]);

		rpmb_session.device_id = rpmb_devid;

		/* open session */
		mc_ret = mc_open_session(&rpmb_session,
					 &rpmb_uuid,
					 (uint8_t *) rpmb_dci,
					 sizeof(struct dciMessage_t));

		if (mc_ret != MC_DRV_OK) {
			MSG(ERR,
			"%s, mc_open_session failed, result(%d), times(%d)\n",
				__func__, mc_ret, cnt);

			mc_ret = mc_free_wsm(rpmb_devid, (uint8_t *)rpmb_dci);
			MSG(ERR, "%s, free wsm result (%d)\n",
				__func__, mc_ret);

			mc_ret = mc_close_device(rpmb_devid);
			MSG(ERR, "%s, try free wsm and close device\n",
				__func__);
			cnt++;
			continue;
		}
		MSG(INFO, "%s, mc_open_session success.\n", __func__);

		/* create a thread for listening DCI signals */
		rpmbDci_th = kthread_run(rpmb_listenDci, NULL, "rpmb_Dci");
		if (IS_ERR(rpmbDci_th))
			MSG(ERR, "%s, init kthread_run failed!\n", __func__);
		else
			break;

	} while (cnt < 30);

	if (cnt >= 30)
		MSG(ERR, "%s, open session failed!!!\n", __func__);


	MSG(ERR, "%s end, mc_ret = %x\n", __func__, mc_ret);

	return mc_ret;
}
#endif

int rpmb_gp_listenDci(void *data)
{
	enum mc_result mc_ret;
	u32 cmdId;
	int boot_type;

	MSG(ERR, "%s: DCI listener.\n", __func__);

	for (;;) {

		MSG(INFO, "%s: Waiting for notification\n", __func__);

		/* Wait for notification from SWd */
		mc_ret = mc_wait_notification(&rpmb_gp_session,
						MC_INFINITE_TIMEOUT);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s: mcWaitNotification failed, mc_ret=%d\n",
				__func__, mc_ret);
			break;
		}

		cmdId = rpmb_gp_dci->command.header.commandId;

		MSG(INFO, "%s: wait notification done!! cmdId = %x\n",
			__func__, cmdId);

		/* Received exception. */
		boot_type = get_boot_type();
		if (boot_type == BOOTDEV_SDMMC)
			mc_ret = rpmb_gp_execute_emmc(cmdId);
#ifdef CONFIG_MTK_UFS_SUPPORT
		else if (boot_type == BOOTDEV_UFS)
			mc_ret = rpmb_gp_execute_ufs(cmdId);
#endif

		/* Notify the STH*/
		mc_ret = mc_notify(&rpmb_gp_session);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s: mcNotify returned: %d\n",
				__func__, mc_ret);
			break;
		}
	}

	return 0;
}

static int rpmb_gp_open_session(void)
{
	int cnt = 0;
	enum mc_result mc_ret = MC_DRV_ERR_UNKNOWN;

	MSG(INFO, "%s start\n", __func__);

	do {
		msleep(2000);

		/* open device */
		mc_ret = mc_open_device(rpmb_gp_devid);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s, mc_open_device failed: %d\n",
				__func__, mc_ret);
			cnt++;
			continue;
		}

		MSG(INFO, "%s, mc_open_device success.\n", __func__);


		/* allocating WSM for DCI */
		mc_ret = mc_malloc_wsm(rpmb_gp_devid, 0,
					sizeof(struct dciMessage_t),
					(uint8_t **)&rpmb_gp_dci, 0);
		if (mc_ret != MC_DRV_OK) {
			mc_close_device(rpmb_gp_devid);
			MSG(ERR, "%s, mc_malloc_wsm failed: %d\n",
				__func__, mc_ret);
			cnt++;
			continue;
		}

		MSG(INFO, "%s, mc_malloc_wsm success.\n", __func__);
		MSG(INFO, "uuid[0]=%d, uuid[1]=%d, uuid[2]=%d, uuid[3]=%d\n",
			rpmb_gp_uuid.value[0],
			rpmb_gp_uuid.value[1],
			rpmb_gp_uuid.value[2],
			rpmb_gp_uuid.value[3]
			);

		rpmb_gp_session.device_id = rpmb_gp_devid;

		/* open session */
		mc_ret = mc_open_session(&rpmb_gp_session,
					 &rpmb_gp_uuid,
					 (uint8_t *) rpmb_gp_dci,
					 sizeof(struct dciMessage_t));

		if (mc_ret != MC_DRV_OK) {
			MSG(ERR,
			"%s, mc_open_session failed, result(%d), times(%d)\n",
				__func__, mc_ret, cnt);

			mc_ret = mc_free_wsm(rpmb_gp_devid,
						(uint8_t *)rpmb_gp_dci);
			MSG(ERR, "%s, free wsm result (%d)\n",
				__func__, mc_ret);

			mc_ret = mc_close_device(rpmb_gp_devid);
			MSG(ERR, "%s, try free wsm and close device\n",
				__func__);
			cnt++;
			continue;
		}
		MSG(INFO, "%s, mc_open_session success.\n", __func__);

		/* create a thread for listening DCI signals */
		rpmb_gp_Dci_th = kthread_run(rpmb_gp_listenDci,
						NULL, "rpmb_gp_Dci");
		if (IS_ERR(rpmb_gp_Dci_th))
			MSG(ERR, "%s, init kthread_run failed!\n", __func__);
		else
			break;

	} while (cnt < 60);

	if (cnt >= 60)
		MSG(ERR, "%s, open session failed!!!\n", __func__);


	MSG(ERR, "%s end, mc_ret = %x\n", __func__, mc_ret);

	return mc_ret;
}


static int rpmb_thread(void *context)
{
	int ret;

	MSG(INFO, "%s start\n", __func__);

#ifndef CONFIG_TEE
	ret = rpmb_open_session();
	MSG(INFO, "%s rpmb_open_session, ret = %x\n", __func__, ret);
#endif

	ret = rpmb_gp_open_session();
	MSG(INFO, "%s rpmb_gp_open_session, ret = %x\n", __func__, ret);

	return 0;
}
#endif

static int rpmb_open(struct inode *inode, struct file *file)
{
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	if (rpmb_buffer == NULL) {
		MSG(ERR, "%s, rpmb buffer is null!!!\n", __func__);
		return -1;
	}
	MSG(INFO, "%s, rpmb kzalloc memory done!!!\n", __func__);
#endif
	return 0;
}

#ifdef CONFIG_MTK_UFS_SUPPORT
long rpmb_ioctl_ufs(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct rpmb_ioc_param param;
#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	u32 rpmb_size = 0;
	u32 arg_k;
	struct rpmb_infor rpmbinfor;
	struct rpmb_dev *rawdev_ufs_rpmb;

	memset(&rpmbinfor, 0, sizeof(struct rpmb_infor));
#endif

	err = copy_from_user(&param, (void *)arg, sizeof(param));

	if (err) {
		MSG(ERR, "%s, copy from user failed: %x\n", __func__, err);
		return -EFAULT;
	}

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	if ((cmd == RPMB_IOCTL_SOTER_WRITE_DATA) ||
		(cmd == RPMB_IOCTL_SOTER_READ_DATA)) {
		if (rpmb_buffer == NULL) {
			MSG(ERR, "%s, rpmb_buffer is NULL!\n", __func__);
			return -1;
		}

		err = copy_from_user(&rpmb_size, (void *)arg, 4);

		if (err) {
			MSG(ERR, "%s, copy from user failed: %x\n",
				__func__, err);
			return -EFAULT;
		}
		rpmbinfor.size =  *(unsigned char *)&rpmb_size |
					(*((unsigned char *)&rpmb_size+1) << 8);
		rpmbinfor.size |= (*((unsigned char *)&rpmb_size+2) << 16) |
				(*((unsigned char *)&rpmb_size+3) << 24);
		if (rpmbinfor.size <= (RPMB_DATA_BUFF_SIZE-4)) {
			MSG(DBG_INFO, "%s, rpmbinfor.size is %d!\n",
				__func__, rpmbinfor.size);
			err = copy_from_user(rpmb_buffer,
					(void *)arg, 4 + rpmbinfor.size);
			if (err) {
				MSG(ERR, "%s, copy from user failed: %x\n",
					__func__, err);
				return -EFAULT;
			}
			rpmbinfor.data_frame = (rpmb_buffer + 4);
		} else {
			MSG(ERR, "%s, rpmbinfor.size(%d+4) is overflow (%d)!\n",
					__func__,
					rpmbinfor.size, RPMB_DATA_BUFF_SIZE);
			return -1;
		}
	}
#endif

	switch (cmd) {

	case RPMB_IOCTL_PROGRAM_KEY:

		MSG(DBG_INFO,
	"%s, cmd = RPMB_IOCTL_PROGRAM_KEY not supported !!!!!!!!!!!!!!\n",
			__func__);

		break;

	case RPMB_IOCTL_READ_DATA:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_READ_DATA!!!!!!!!!!!!!!\n",
			__func__);

		err = rpmb_req_ioctl_read_data_ufs(&param);

		if (err) {
			MSG(ERR,
			"%s, rpmb_req_ioctl_read_data IO error!!!(%x)\n",
				__func__, err);
			return err;
		}

		err = copy_to_user((void *)arg, &param, sizeof(param));

		if (err) {
			MSG(ERR, "%s, copy to user user failed: %x\n",
				__func__, err);
			return -EFAULT;
		}

		break;

	case RPMB_IOCTL_WRITE_DATA:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_WRITE_DATA!!!!!!!!!!!!!!\n",
			__func__);

		err = rpmb_req_ioctl_write_data_ufs(&param);

		if (err)
			MSG(ERR,
			"%s, rpmb_req_ioctl_write_data IO error!!!(%x)\n",
				__func__, err);

		break;

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	case RPMB_IOCTL_SOTER_WRITE_DATA:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_SOTER_WRITE_DATA\n",
			__func__);

		err = rpmb_req_write_data_ufs(rpmbinfor.data_frame,
					rpmbinfor.size / RPMB_ONE_FRAME_SIZE);

		if (err) {
			MSG(ERR,
			"%s, Microtrust rpmb write request IO error!!!(%x)\n",
				__func__, err);
			return err;
		}

		err = copy_to_user((void *)arg,
					rpmb_buffer, 4 + rpmbinfor.size);

		if (err) {
			MSG(ERR, "%s, copy to user user failed: %x\n",
				__func__, err);
			return -EFAULT;
		}

		break;

	case RPMB_IOCTL_SOTER_READ_DATA:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_SOTER_READ_DATA\n",
			__func__);

		err = rpmb_req_read_data_ufs(rpmbinfor.data_frame,
					rpmbinfor.size / RPMB_ONE_FRAME_SIZE);

		if (err) {
			MSG(ERR,
			"%s, Microtrust rpmb read request IO error!!!(%x)\n",
				__func__, err);
			return err;
		}

		err = copy_to_user((void *)arg,
					rpmb_buffer, 4 + rpmbinfor.size);

		if (err) {
			MSG(ERR, "%s, copy to user user failed: %x\n",
				__func__, err);
			return -EFAULT;
		}

		break;

	case RPMB_IOCTL_SOTER_GET_CNT:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_SOTER_GET_CNT\n", __func__);

		err = rpmb_req_get_wc_ufs(NULL, &arg_k, NULL);
		if (err) {
			MSG(ERR,
	"%s, Microtrust get rpmb write counter failed, error code (%x)\n",
				__func__, err);
			return err;
		}

		err = copy_to_user((void *)arg, &arg_k, sizeof(u32));

		if (err) {
			MSG(ERR, "%s, copy_to_user failed: %x\n",
				__func__, err);
			return -EFAULT;
		}

		break;

	case RPMB_IOCTL_SOTER_GET_WR_SIZE:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_SOTER_GET_WR_SIZE\n",
			__func__);

		rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

		if (rawdev_ufs_rpmb) {

			arg_k = rpmb_get_rw_size(rawdev_ufs_rpmb);
			err = copy_to_user((void *)arg, &arg_k, sizeof(u32));

			if (err) {
				MSG(ERR, "%s, copy_to_user failed: %x\n",
					__func__, err);
				return -EFAULT;
			}
		} else
			err = RPMB_ALLOC_ERROR;

		break;

#endif
	default:
		MSG(ERR, "%s, wrong ioctl code (%d)!!!\n", __func__, cmd);
		return -ENOTTY;
	}

	return err;
}
#endif

long rpmb_ioctl_emmc(struct file *file, unsigned int cmd, unsigned long arg)
{
#if defined(RPMB_IOCTL_UT) || defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	int err = 0;
#endif
	struct mmc_card *card;
	int ret = 0;
#if defined(RPMB_IOCTL_UT)
	struct rpmb_ioc_param param;
	unsigned char *ukey, *udata;
#endif

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	u32 arg_k;
	u32 rpmb_size = 0;
	struct rpmb_infor rpmbinfor;
	unsigned int *arg_p = (unsigned int *)arg;
	unsigned int user_arg;

	memset(&rpmbinfor, 0, sizeof(struct rpmb_infor));
#endif

#if defined(CONFIG_MMC_MTK_PRO)
	if (!mtk_msdc_host[0] || !mtk_msdc_host[0]->mmc
		|| !mtk_msdc_host[0]->mmc->card)
		return -EFAULT;

	card = mtk_msdc_host[0]->mmc->card;
#else
	card = NULL;
	ret = -EFAULT;
#endif

#if defined(RPMB_IOCTL_UT)
	err = copy_from_user(&param, (void *)arg, sizeof(param));
	if (err) {
		MSG(ERR, "%s, copy from user failed: %x\n", __func__, err);
		return -EFAULT;
	}

	/* limit R/W arguments : less than RPMB area size
	 * follow block.c: limit transfer size 128K(don't use
	 * vmalloc for system performance)
	 */
	if ((param.data_len + param.addr * 256)
		> card->ext_csd.raw_rpmb_size_mult * 128 * 1024 ||
		param.data_len > RPMB_IOC_MAX_BYTES)
		return -EINVAL;

	if (!param.key || !param.data)
		return -EFAULT;

	ukey = param.key;
	udata = param.data;
	param.key = kmalloc(32, GFP_KERNEL);

	/* follow block.c :  at least one block(RPMB
	 * block size is:256) is allocated
	 */
	if (param.data_len < RPMB_SZ_DATA)
		param.data = kmalloc(RPMB_SZ_DATA, GFP_KERNEL);
	else
		param.data = kmalloc(param.data_len, GFP_KERNEL);
	if (param.key) {
		err = copy_from_user(param.key, ukey, 32);
		if (err != 0) {
			MSG(ERR, "%s, err=%x\n", __func__, err);
			ret = -1;
			goto end;
		}
	} else {
		ret = -1;
		goto end;
	}

	if (param.data) {
		err = copy_from_user(param.data, udata, param.data_len);
		if (err != 0) {
			MSG(ERR, "%s, err=%x\n", __func__, err);
			ret = -1;
			goto end;
		}
	} else {
		ret = -1;
		goto end;
	}
#endif

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	if ((cmd == RPMB_IOCTL_SOTER_WRITE_DATA) ||
		(cmd == RPMB_IOCTL_SOTER_READ_DATA)) {
		if (rpmb_buffer == NULL) {
			MSG(ERR, "%s, rpmb_buffer is NULL!\n", __func__);
			ret = -1;
			goto end;
		}
		err = copy_from_user(&rpmb_size, (void *)arg, 4);
		if (err) {
			MSG(ERR, "%s, copy from user failed: %x\n",
				__func__, err);
			ret = -1;
			goto end;
		}
		rpmbinfor.size =  *(unsigned char *)&rpmb_size |
					(*((unsigned char *)&rpmb_size+1) << 8);
		rpmbinfor.size |= (*((unsigned char *)&rpmb_size+2) << 16) |
				(*((unsigned char *)&rpmb_size+3) << 24);
		if (rpmbinfor.size <= (RPMB_DATA_BUFF_SIZE-4)) {
			MSG(INFO, "%s, rpmbinfor.size is %d!\n",
				__func__, rpmbinfor.size);
			err = copy_from_user(rpmb_buffer,
					(void *)arg, 4 + rpmbinfor.size);
			if (err) {
				MSG(ERR, "%s, copy from user failed: %x\n",
					__func__, err);
				ret = -1;
				goto end;
			}
			rpmbinfor.data_frame = (rpmb_buffer + 4);
		} else {
			MSG(ERR, "%s, rpmbinfor.size(%d+4) is overflow (%d)!\n",
					__func__, rpmbinfor.size,
					RPMB_DATA_BUFF_SIZE);
			ret = -1;
			goto end;
		}
	}
#endif

	switch (cmd) {
#if defined(RPMB_IOCTL_UT)
	case RPMB_IOCTL_PROGRAM_KEY:

		MSG(INFO, "%s, cmd = RPMB_IOCTL_PROGRAM_KEY!!!!!!!!!!!!!!\n",
			__func__);

		ret = emmc_rpmb_req_set_key(card, param.key);

		break;

	case RPMB_IOCTL_READ_DATA:

		MSG(INFO, "%s, cmd = RPMB_IOCTL_READ_DATA!!!!!!!!!!!!!!\n",
			__func__);

		ret = rpmb_req_ioctl_read_data_emmc(card, &param);

		err = copy_to_user(udata, param.data, param.data_len);
		if (err) {
			MSG(ERR, "%s, err=%x\n", __func__, err);
			ret = -1;
			goto end;
		}

		break;

	case RPMB_IOCTL_WRITE_DATA:

		MSG(INFO, "%s, cmd = RPMB_IOCTL_WRITE_DATA!!!!!!!!!!!!!!\n",
			__func__);

		ret = rpmb_req_ioctl_write_data_emmc(card, &param);

		break;
#endif

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	case RPMB_IOCTL_SOTER_WRITE_DATA:

		ret = ut_rpmb_req_write_data(card,
					(struct s_rpmb *)(rpmbinfor.data_frame),
					rpmbinfor.size/RPMB_ONE_FRAME_SIZE);

		if (ret) {
			MSG(ERR,
				"%s, ISEE rpmb write request IO error!!!(%x)\n",
				__func__, ret);
			goto end;
		}

		ret = copy_to_user((void *)arg,
					rpmb_buffer, 4 + rpmbinfor.size);

		if (ret)
			goto end;

		break;

	case RPMB_IOCTL_SOTER_READ_DATA:

		ret = ut_rpmb_req_read_data(card,
					(struct s_rpmb *)(rpmbinfor.data_frame),
					rpmbinfor.size/RPMB_ONE_FRAME_SIZE);

		if (ret) {
			MSG(ERR, "%s, ISEE rpmb read request IO error!!!(%x)\n",
				__func__, ret);
			goto end;
		}

		ret = copy_to_user((void *)arg,
					rpmb_buffer, 4 + rpmbinfor.size);

		if (ret)
			goto end;

		break;

	case RPMB_IOCTL_SOTER_GET_CNT:

		if (get_user(user_arg, arg_p)) {
			ret = -1;
			goto end;
		}

		ret = ut_rpmb_req_get_wc(card, (unsigned int *)&arg_k);

		if (ret) {
			MSG(ERR, "%s, ISEE rpmb get write counter error (%x)\n",
				__func__, ret);
			goto end;
		}

		ret = copy_to_user((void *)arg, &arg_k, sizeof(u32));

		if (ret) {
			MSG(ERR, "%s, copy_to_user failed: %x\n",
				__func__, ret);
			goto end;
		}

		break;

	case RPMB_IOCTL_SOTER_GET_WR_SIZE:

		if (get_user(user_arg, arg_p)) {
			ret = -1;
			goto end;
		}

		ret = ut_rpmb_req_get_max_wr_size(card, (unsigned int *)&arg_k);

		if (ret) {
			MSG(ERR,
			"%s, ISEE rpmb get max write block size error (%x)\n",
				__func__, ret);
			goto end;
		}

		ret = copy_to_user((void *)arg, &arg_k, sizeof(u32));

		if (ret) {
			MSG(ERR, "%s, copy_to_user failed: %x\n",
				__func__, ret);
			goto end;
		}

		break;

#endif
	default:
		MSG(ERR, "%s, wrong ioctl code (%d)!!!\n", __func__, cmd);
		ret = -ENOTTY;
		goto end;
	}
end:
#if defined(RPMB_IOCTL_UT)
	kfree(param.data);
	kfree(param.key);
#endif
	return ret;
}


static int rpmb_close(struct inode *inode, struct file *file)
{
	int ret = 0;

	MSG(INFO, "%s, !!!!!!!!!!!!\n", __func__);

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	if (rpmb_buffer)
		memset(rpmb_buffer, 0x0, RPMB_DATA_BUFF_SIZE);
#endif
	return ret;
}

#ifdef CONFIG_MTK_UFS_SUPPORT
static const struct file_operations rpmb_fops_ufs = {
	.owner = THIS_MODULE,
	.open = rpmb_open,
	.release = rpmb_close,
	.unlocked_ioctl = rpmb_ioctl_ufs,
	.write = NULL,
	.read = NULL,
};
#endif

static const struct file_operations rpmb_fops_emmc = {
	.owner = THIS_MODULE,
	.open = rpmb_open,
	.release = rpmb_close,
	.unlocked_ioctl = rpmb_ioctl_emmc,
	.write = NULL,
	.read = NULL,
};

static int __init rpmb_init(void)
{
	int alloc_ret = -1;
	int cdev_ret = -1;
	int major;
	dev_t dev;
	struct device *device = NULL;
	int boot_type;

	MSG(INFO, "%s start\n", __func__);

	alloc_ret = alloc_chrdev_region(&dev, 0, 1, RPMB_NAME);

	if (alloc_ret) {
		MSG(ERR, "%s, init alloc_chrdev_region failed!\n", __func__);
		goto error;
	}

	major = MAJOR(dev);

	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_SDMMC)
		cdev_init(&rpmb_dev, &rpmb_fops_emmc);
#ifdef CONFIG_MTK_UFS_SUPPORT
	else if (boot_type == BOOTDEV_UFS)
		cdev_init(&rpmb_dev, &rpmb_fops_ufs);
#endif

	rpmb_dev.owner = THIS_MODULE;

	cdev_ret = cdev_add(&rpmb_dev, MKDEV(major, 0), 1);
	if (cdev_ret) {
		MSG(ERR, "%s, init cdev_add failed!\n", __func__);
		goto error;
	}

	mtk_rpmb_class = class_create(THIS_MODULE, RPMB_NAME);

	if (IS_ERR(mtk_rpmb_class)) {
		MSG(ERR, "%s, init class_create failed!\n", __func__);
		goto error;
	}

	device = device_create(mtk_rpmb_class, NULL, MKDEV(major, 0), NULL,
		RPMB_NAME "%d", 0);

	if (IS_ERR(device)) {
		MSG(ERR, "%s, init device_create failed!\n", __func__);
		goto error;
	}

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
	open_th = kthread_run(rpmb_thread, NULL, "rpmb_open");
	if (IS_ERR(open_th))
		MSG(ERR, "%s, init kthread_run failed!\n", __func__);
#endif

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	rpmb_buffer = kzalloc(RPMB_DATA_BUFF_SIZE, 0);
	if (rpmb_buffer == NULL) {
		MSG(ERR, "%s, rpmb kzalloc memory fail!!!\n", __func__);
		return -1;
	}
	MSG(INFO, "%s, rpmb kzalloc memory done!!!\n", __func__);
#endif

	MSG(INFO, "%s end!!!!\n", __func__);

	return 0;

error:

	if (mtk_rpmb_class)
		class_destroy(mtk_rpmb_class);

	if (cdev_ret == 0)
		cdev_del(&rpmb_dev);

	if (alloc_ret == 0)
		unregister_chrdev_region(dev, 1);

	return -1;
}

late_initcall(rpmb_init);

