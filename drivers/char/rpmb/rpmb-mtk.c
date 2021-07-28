// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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

#include <linux/scatterlist.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/of.h>
#include <net/sock.h>
#include <net/net_namespace.h>
#include <linux/netlink.h>

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK)
#include "ufs-mediatek.h"
#endif

/* #define __RPMB_MTK_DEBUG_MSG */
/* #define __RPMB_MTK_DEBUG_HMAC_VERIFY */
/* #define __RPMB_KERNEL_NL_SUPPORT */
#define __RPMB_IOCTL_SUPPORT

/* TEE usage */
#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include "mobicore_driver_api.h"
#include "drrpmb_gp_Api.h"
#include "drrpmb_Api.h"

static struct mc_uuid_t rpmb_gp_uuid = RPMB_GP_UUID;
static struct mc_session_handle rpmb_gp_session = {0};
static u32 rpmb_gp_devid = MC_DEVICE_ID_DEFAULT;
static struct dciMessage_t *rpmb_gp_dci;
#endif

/* For nl socket */
#ifdef __RPMB_KERNEL_NL_SUPPORT
struct sock *rpmb_mtk_sock;
static u32 nl_pid = 100;
#define NETLINK_RPMB_MTK 30

wait_queue_head_t wait_rpmb;
static bool rpmb_done_flag;
struct mutex rpmb_lock;


/**
 * struct storage_rpmb_req - request format for STORAGE_RPMB_SEND
 * @reliable_write_size:        size in bytes of reliable write region
 * @write_size:                 size in bytes of write region
 * @read_size:                  number of bytes to read for a read request
 * @__reserved:                 unused, must be set to 0
 * @payload:                    start of reliable write region, followed by
 *                              write region.
 *                              MAX_RPMB_REQUEST_SIZE(reliable write region)) +
 *                              512(write region)
 *
 */
struct nl_rpmb_send_req {
	u32 reliable_write_size;
	u32 write_size;
	u32 read_size;
	u32 __reserved;
	u8  payload[MAX_RPMB_REQUEST_SIZE+512];
};

static struct nl_rpmb_send_req nl_rpmb_req;
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
#define MAX_RPMB_TRANSFER_BLK (1U)
#endif

#define RPMB_NAME "rpmb"

#define RPMB_IOCTL_PROGRAM_KEY  1
#define RPMB_IOCTL_WRITE_DATA   3
#define RPMB_IOCTL_READ_DATA    4

struct rpmb_ioc_param {
	unsigned char *keybytes;
	unsigned char *databytes;
	unsigned int  data_len;
	unsigned short addr;
	unsigned char *hmac;
	unsigned int hmac_len;
};

#define RPMB_SZ_MAC   32U
#define RPMB_SZ_DATA  256U
#define RPMB_SZ_STUFF 196U
#define RPMB_SZ_NONCE 16U
#define RPMB_SZ_KEY   32U
#define RPMB_SZ_CAL_HMAC 284UL
#define RPMB_SZ_FRAME 512UL

struct s_rpmb {
	unsigned char stuff[RPMB_SZ_STUFF];
	unsigned char mac[RPMB_SZ_MAC];
	unsigned char data[RPMB_SZ_DATA];
	unsigned char nonce[RPMB_SZ_NONCE];
	unsigned int write_counter;
	unsigned short address;
	unsigned short block_count;
	unsigned short result;
	unsigned short request;
};

enum {
	RPMB_SUCCESS = 0,
	RPMB_HMAC_ERROR,
	RPMB_RESULT_ERROR,
	RPMB_WC_ERROR,
	RPMB_NONCE_ERROR,
	RPMB_ALLOC_ERROR,
	RPMB_TRANSFER_NOT_COMPLETE,
};

#define RPMB_REQ               1       /* RPMB request mark */
#define RPMB_RESP              (1 << 1)/* RPMB response mark */
#define RPMB_AVAILABLE_SECTORS 8       /* 4K page size */

#define RPMB_TYPE_BEG          510
#define RPMB_RES_BEG           508
#define RPMB_BLKS_BEG          506
#define RPMB_ADDR_BEG          504
#define RPMB_WCOUNTER_BEG      500

#define RPMB_NONCE_BEG         484
#define RPMB_DATA_BEG          228
#define RPMB_MAC_BEG           196

#define DEFAULT_HANDLES_NUM (64)
#define MAX_OPEN_SESSIONS (0xffffffffU - 1)

/* Debug message event */
#define DBG_EVT_NONE (0) /* No event */
#define DBG_EVT_CMD  (1U << 0)/* SEC CMD related event */
#define DBG_EVT_FUNC (1U << 1U)/* SEC function event */
#define DBG_EVT_INFO (1U << 2U)/* SEC information event */
#define DBG_EVT_WRN  (1U << 30U) /* Warning event */
#define DBG_EVT_ERR  (0x80000000U) /* Error event, 1 << 31 */
#ifdef __RPMB_MTK_DEBUG_MSG
#define DBG_EVT_DBG_INFO  (DBG_EVT_ERR) /* Error event */
#else
#define DBG_EVT_DBG_INFO  (1U << 2U) /* Information event */
#endif
#define DBG_EVT_ALL  (0xffffffffU)

static u32 dbg_evt = DBG_EVT_ALL;
#define DBG_EVT_MASK (dbg_evt)

#define MSG(evt, fmt, args...) \
do {\
	if (((DBG_EVT_##evt) & DBG_EVT_MASK) != 0U) { \
		(void)(pr_notice("[%s] "fmt, RPMB_NAME, ##args)); \
	} \
} while (false)

#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT)
static struct task_struct *open_th;
static struct task_struct *rpmb_gp_Dci_th;
#endif


static struct cdev rpmb_cdev;
#ifdef __RPMB_IOCTL_SUPPORT
static struct class *mtk_rpmb_class;
#endif

#ifdef __RPMB_KERNEL_NL_SUPPORT
static int rpmb_mtk_snd_msg(void *pbuf, u16 len);
#endif

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

static int hmac_sha256(const char *keybytes, u32 klen, const char *str,
			size_t len, u8 *hmac)
{
	struct shash_desc *shash;
	struct crypto_shash *hmacsha256 = crypto_alloc_shash("hmac(sha256)",
							     0, 0);
	size_t size = 0;
	int err = 0;
	unsigned int nbytes = (unsigned int)len;

	if (IS_ERR(hmacsha256))
		return -1;

	size = sizeof(struct shash_desc) + crypto_shash_descsize(hmacsha256);

	shash = kmalloc(size, GFP_KERNEL);
	if (shash == NULL) {
		err = -1;
		goto malloc_err;
	}
	shash->tfm = hmacsha256;

	err = crypto_shash_setkey(hmacsha256, keybytes, klen);
	if (err != 0) {
		err = -1;
		goto hash_err;
	}

	err = crypto_shash_init(shash);
	if (err != 0) {
		err = -1;
		goto hash_err;
	}

	err = crypto_shash_update(shash, str, nbytes);
	if (err != 0) {
		err = -1;
		goto hash_err;
	}

	err = crypto_shash_final(shash, hmac);

hash_err:
	kfree(shash);
malloc_err:
	crypto_free_shash(hmacsha256);

	return err;
}

#ifdef __RPMB_MTK_DEBUG_HMAC_VERIFY
unsigned char g_rpmb_key[RPMB_SZ_KEY] = {
	0x64, 0x76, 0xEE, 0xF0, 0xF1, 0x6B, 0x30, 0x47,
	0xE9, 0x79, 0x31, 0x58, 0xF6, 0x42, 0xDA, 0x46,
	0xF7, 0x3B, 0x53, 0xFD, 0xC5, 0xF8, 0x84, 0xCE,
	0x03, 0x73, 0x15, 0xBC, 0x54, 0x47, 0xD4, 0x6A
};

static int rpmb_cal_hmac(struct rpmb_frame *frame, int blk_cnt,
			u8 *key, u8 *key_mac)
{
	int i;
	u8 *buf, *buf_start;

	buf = buf_start = kzalloc(RPMB_SZ_CAL_HMAC * blk_cnt, 0);

	for (i = 0; i < blk_cnt; i++) {
		memcpy(buf, frame[i].data, RPMB_SZ_CAL_HMAC);
		buf += RPMB_SZ_CAL_HMAC;
	}

	if (hmac_sha256(key, RPMB_SZ_KEY, buf_start, RPMB_SZ_CAL_HMAC * blk_cnt, key_mac) != 0)
		MSG(ERR, "hmac_sha256() return error!\n");

	kfree(buf_start);

	return 0;
}
#endif

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

static struct rpmb_frame *rpmb_alloc_frames(unsigned int cnt)
{
	return kzalloc(sizeof(struct rpmb_frame) * cnt, 0);
}

#ifdef __RPMB_KERNEL_NL_SUPPORT
static int nl_rpmb_cmd_req(const struct rpmb_data *rpmbd)
{
	int ret = 0;

	u16 type, msg_len;
	u32 cnt_in, cnt_out;
	struct rpmb_frame *res_frame;
	u8 *write_buf, *read_buf;
	struct nl_rpmb_send_req *req = &nl_rpmb_req;

	cnt_in = rpmbd->icmd.nframes;
	cnt_out = rpmbd->ocmd.nframes;
	type = rpmbd->req_type;
	write_buf = req->payload;

	mutex_lock(&rpmb_lock);

	switch (type) {
	case RPMB_PROGRAM_KEY:
		cnt_in = 1;
		cnt_out = 1;
		fallthrough;

	case RPMB_WRITE_DATA:
		req->reliable_write_size = cnt_in * 512;
		memcpy(write_buf, rpmbd->icmd.frames,
			req->reliable_write_size);
		write_buf += req->reliable_write_size;

		res_frame = rpmbd->ocmd.frames;
		memset(res_frame, 0, sizeof(*res_frame));
		res_frame->req_resp = cpu_to_be16(RPMB_RESULT_READ);
		req->write_size = 512;
		memcpy(write_buf, res_frame, req->write_size);
		write_buf += req->write_size;

		req->read_size = cnt_out * 512;
		break;

	case RPMB_GET_WRITE_COUNTER:
		cnt_in = 1;
		cnt_out = 1;
		fallthrough;

	case RPMB_READ_DATA:
		req->reliable_write_size = cnt_in * 512;
		memcpy(write_buf, rpmbd->icmd.frames,
			req->reliable_write_size);
		write_buf += req->reliable_write_size;

		req->write_size = 0;
		req->read_size = cnt_out * 512;
		break;

	default:
		pr_info("%s, -EINVAL in line %d!\n",
			__func__, __LINE__);
		mutex_unlock(&rpmb_lock);
		return -EINVAL;
	}


	rpmb_done_flag = false; /* clear flag */
	msg_len = 16 + req->reliable_write_size + req->write_size;

	ret = rpmb_mtk_snd_msg(&nl_rpmb_req, msg_len);
	if (ret != 0) {
		MSG(ERR, "%s, rpmb message IO error!!!(0x%x)\n",
		    __func__, ret);
		mutex_unlock(&rpmb_lock);
		return -EINVAL;
	}

	ret = wait_event_timeout(wait_rpmb,
				 rpmb_done_flag,
				 msecs_to_jiffies(10000));
	if (ret == 0) {
		MSG(ERR, "[%s] rpmb operation timeout.", __func__);
		ret = -ETIMEDOUT;
	}

	/* copy frame to rpmb_data */
	read_buf = req->payload;
	if (req->read_size)
		memcpy(rpmbd->ocmd.frames, read_buf, req->read_size);

	mutex_unlock(&rpmb_lock);

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK)
static int rpmb_req_get_wc_ufs(u8 *keybytes, u32 *wc, u8 *frame)
{
	struct rpmb_data rpmbdata;
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
			rpmbdata.icmd.frames = (struct rpmb_frame *)frame;
			rpmbdata.ocmd.frames = (struct rpmb_frame *)frame;

		} else {

			rpmbdata.icmd.frames = rpmb_alloc_frames(1);

			if (rpmbdata.icmd.frames == NULL)
				return RPMB_ALLOC_ERROR;

			rpmbdata.ocmd.frames = rpmb_alloc_frames(1);

			if (rpmbdata.ocmd.frames == NULL) {
				kfree(rpmbdata.icmd.frames);
				return RPMB_ALLOC_ERROR;
			}
		}

		/*
		 * Prepare frame contents.
		 *
		 * Input frame (in view of device) only needs nonce
		 */

		rpmbdata.req_type = RPMB_GET_WRITE_COUNTER;
		rpmbdata.icmd.nframes = 1;

		/* Fill-in essential field in self-prepared frame */

		if (!frame) {
			get_random_bytes(nonce, (int)RPMB_SZ_NONCE);
			rpmbdata.icmd.frames->req_resp =
				cpu_to_be16(RPMB_GET_WRITE_COUNTER);
			memcpy(rpmbdata.icmd.frames->nonce, nonce,
				RPMB_SZ_NONCE);
		}

		/* Output frame (in view of device) */

		rpmbdata.ocmd.nframes = 1;

		#ifdef __RPMB_KERNEL_NL_SUPPORT
		ret = nl_rpmb_cmd_req(&rpmbdata);
		#else
		ret = rpmb_cmd_req(rawdev_ufs_rpmb, &rpmbdata);
		#endif

		if (ret) {
			MSG(ERR, "%s, nl_rpmb_cmd_req IO error!!!(0x%x)\n",
				__func__, ret);
			break;
		}

		/* Verify HMAC only if key is available */

		if (keybytes) {
			/*
			 * Authenticate response write counter frame.
			 */
			if (hmac_sha256(keybytes, RPMB_SZ_KEY,
					rpmbdata.ocmd.frames->data,
					RPMB_SZ_CAL_HMAC, hmac) != 0)
				MSG(ERR, "hmac_sha256() return error!\n");

			if (memcmp(hmac, rpmbdata.ocmd.frames->key_mac,
				   RPMB_SZ_MAC) != 0) {
				MSG(ERR, "%s, hmac compare error!!!\n",
					__func__);
				ret = RPMB_HMAC_ERROR;
			}

			/*
			 * DEVICE ISSUE:
			 * We found some devices will return hmac vale with
			 * all zeros.
			 * For this kind of device, bypass hmac comparison.
			 */
			if (ret == RPMB_HMAC_ERROR) {
				for (i = 0; i < RPMB_SZ_MAC; i++) {
					if (rpmbdata.ocmd.frames->key_mac[i] !=
					    0U) {
						MSG(ERR,
						    "%s, dev hmac not NULL!\n",
						    __func__);
						break;
					}
				}

				MSG(ERR,
				    "%s, device hmac has all zero, bypassed!\n",
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
			if (memcmp(nonce, rpmbdata.ocmd.frames->nonce,
				RPMB_SZ_NONCE) != 0) {
				MSG(ERR, "%s, nonce compare error!!!\n",
					__func__);
				rpmb_dump_frame((u8 *)rpmbdata.ocmd.frames);
				ret = RPMB_NONCE_ERROR;
				break;
			}

			if (rpmbdata.ocmd.frames->result) {
				MSG(ERR, "%s, result error!!! (0x%x)\n",
				    __func__,
				    cpu_to_be16(rpmbdata.ocmd.frames->result));
				ret = RPMB_RESULT_ERROR;
				break;
			}
		}

		if (wc) {
			*wc = cpu_to_be32(rpmbdata.ocmd.frames->write_counter);
			MSG(DBG_INFO, "%s: wc = %d (0x%x)\n",
				__func__, *wc, *wc);
		}
	} while (false);

	MSG(DBG_INFO, "%s: end\n", __func__);

	if (!frame) {
		kfree(rpmbdata.icmd.frames);
		kfree(rpmbdata.ocmd.frames);
	}

	return ret;
}

#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT)
static int rpmb_req_read_data_ufs(u8 *frame, u32 blk_cnt)
{
	struct rpmb_data rpmbdata;
	struct rpmb_dev *rawdev_ufs_rpmb;
	int ret;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	MSG(DBG_INFO, "%s: blk_cnt: %d\n", __func__, blk_cnt);

	rpmbdata.req_type = RPMB_READ_DATA;
	rpmbdata.icmd.nframes = 1;
	rpmbdata.icmd.frames = (struct rpmb_frame *)frame;

	/*
	 * We need to fill-in block_count by ourselves for UFS case.
	 * TEE does not fill-in this field because eMMC spec specifiy it as 0.
	 */
	rpmbdata.icmd.frames->block_count = cpu_to_be16((u16)blk_cnt);

	rpmbdata.ocmd.nframes = blk_cnt;
	rpmbdata.ocmd.frames = (struct rpmb_frame *)frame;

	#ifdef __RPMB_KERNEL_NL_SUPPORT
	ret = nl_rpmb_cmd_req(&rpmbdata);
	#else
	ret = rpmb_cmd_req(rawdev_ufs_rpmb, &rpmbdata);
	#endif

	if (ret)
		MSG(ERR, "%s: nl_rpmb_cmd_req IO error, ret %d (0x%x)\n",
			__func__, ret, ret);

	MSG(DBG_INFO, "%s: result 0x%x\n", __func__,
		rpmbdata.ocmd.frames->result);

	MSG(DBG_INFO, "%s: ret 0x%x\n", __func__, ret);

	return ret;
}

static int rpmb_req_write_data_ufs(u8 *frame, u32 blk_cnt)
{
	struct rpmb_data rpmbdata;
	struct rpmb_dev *rawdev_ufs_rpmb;
	int ret;
#ifdef __RPMB_MTK_DEBUG_HMAC_VERIFY
	u8 *key_mac;
#endif

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	MSG(DBG_INFO, "%s: blk_cnt: %d\n", __func__, blk_cnt);

	/*
	 * Alloc output frame to avoid overwriting input frame buffer
	 * provided by TEE
	 */
	rpmbdata.ocmd.frames = rpmb_alloc_frames(1);

	if (rpmbdata.ocmd.frames == NULL)
		return RPMB_ALLOC_ERROR;

	rpmbdata.ocmd.nframes = 1;

	rpmbdata.req_type = RPMB_WRITE_DATA;
	rpmbdata.icmd.nframes = blk_cnt;
	rpmbdata.icmd.frames = (struct rpmb_frame *)frame;

#ifdef __RPMB_MTK_DEBUG_HMAC_VERIFY
	key_mac = kzalloc(RPMB_SZ_MAC, 0);

	rpmb_cal_hmac((struct rpmb_frame *)frame, blk_cnt, g_rpmb_key, key_mac);

	if (memcmp(key_mac, ((struct rpmb_frame *)frame)[blk_cnt - 1].key_mac,
		32)) {
		MSG(ERR, "%s, Key Mac is NOT matched!\n", __func__);
		kfree(key_mac);
		ret = 1;
		goto out;
	} else
		MSG(ERR, "%s, Key Mac check passed.\n", __func__);

	kfree(key_mac);
#endif

	#ifdef __RPMB_KERNEL_NL_SUPPORT
	ret = nl_rpmb_cmd_req(&rpmbdata);
	#else
	ret = rpmb_cmd_req(rawdev_ufs_rpmb, &rpmbdata);
	#endif

	if (ret)
		MSG(ERR, "%s: nl_rpmb_cmd_req IO error, ret %d (0x%x)\n",
			__func__, ret, ret);

	/*
	 * Microtrust TEE will check write counter in the first frame,
	 * thus we copy response frame to the first frame.
	 */
	memcpy(frame, rpmbdata.ocmd.frames, RPMB_SZ_FRAME);

	MSG(DBG_INFO, "%s: result 0x%x\n", __func__,
		rpmbdata.ocmd.frames->result);

	kfree(rpmbdata.ocmd.frames);

	MSG(DBG_INFO, "%s: ret 0x%x\n", __func__, ret);

#ifdef __RPMB_MTK_DEBUG_HMAC_VERIFY
out:
#endif

	return ret;
}

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

static int rpmb_req_ioctl_write_data_ufs(struct rpmb_ioc_param *param)
{
	struct rpmb_data rpmbdata;
	struct rpmb_dev *rawdev_ufs_rpmb;
	u32 i, tran_size, left_size = param->data_len;
	u32 wc = 0xFFFFFFFFU;
	u16 iCnt, tran_blkcnt, left_blkcnt;
	u16 blkaddr;
	u8 hmac[RPMB_SZ_MAC], rpmb_key[RPMB_SZ_KEY];
	u8 *dataBuf, *dataBuf_start;
	size_t size_for_hmac;
	int ret = 0;
	u8 user_param_data;

	MSG(DBG_INFO, "%s start!!!\n", __func__);

	ret = get_user(user_param_data, param->databytes);
	if (ret != 0)
		return -EFAULT;

	ret = get_user(user_param_data, param->keybytes);
	if (ret != 0)
		return -EFAULT;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	/* copy key first */
	ret = copy_from_user(rpmb_key, param->keybytes, RPMB_SZ_KEY);
	if (ret != 0)
		return -EFAULT;

	i = 0;
	tran_blkcnt = 0;
	dataBuf = NULL;
	dataBuf_start = NULL;

	left_blkcnt = (u16)((param->data_len % RPMB_SZ_DATA) ?
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

		if (left_blkcnt >= MAX_RPMB_TRANSFER_BLK)
			tran_blkcnt = MAX_RPMB_TRANSFER_BLK;
		else
			tran_blkcnt = left_blkcnt;

		MSG(DBG_INFO, "%s, total_blkcnt = 0x%x, tran_blkcnt = 0x%x\n",
		    __func__, left_blkcnt, tran_blkcnt);

		ret = rpmb_req_get_wc_ufs(rpmb_key, &wc, NULL);
		if (ret) {
			MSG(ERR, "%s, rpmb_req_get_wc_ufs error!!!(0x%x)\n",
			    __func__, ret);
			return ret;
		}

		/*
		 * Initial frame buffers
		 */

		rpmbdata.icmd.frames = rpmb_alloc_frames(tran_blkcnt);

		if (rpmbdata.icmd.frames == NULL)
			return RPMB_ALLOC_ERROR;

		rpmbdata.ocmd.frames = rpmb_alloc_frames(1);

		if (rpmbdata.ocmd.frames == NULL) {
			kfree(rpmbdata.icmd.frames);
			return RPMB_ALLOC_ERROR;
		}

		/*
		 * Initial data buffer for HMAC computation.
		 * Since HAMC computation tool which we use needs
		 * consecutive data buffer.
		 * Pre-alloced it.
		 */

		dataBuf_start = dataBuf =
			kzalloc(RPMB_SZ_CAL_HMAC * tran_blkcnt, 0);
		if (!dataBuf_start) {
			kfree(rpmbdata.icmd.frames);
			kfree(rpmbdata.ocmd.frames);
			return RPMB_ALLOC_ERROR;
		}

		/*
		 * Prepare frame contents
		 */

		rpmbdata.req_type = RPMB_WRITE_DATA;


		/* Output frames (in view of device) */

		rpmbdata.ocmd.nframes = 1;

		/*
		 * All input frames (in view of device) need below stuff,
		 * 1. address.
		 * 2. write counter.
		 * 3. data.
		 * 4. block count.
		 * 5. MAC
		 */

		rpmbdata.icmd.nframes = tran_blkcnt;

		/* size for hmac calculation: 512 - 228 = 284 */
		size_for_hmac = sizeof(struct rpmb_frame) -
				offsetof(struct rpmb_frame, data);

		for (iCnt = 0; iCnt < tran_blkcnt; iCnt++) {
			/*
			 * Prepare write data frame. need addr, wc, blkcnt,
			 * data and mac.
			 */
			rpmbdata.icmd.frames[iCnt].req_resp =
				cpu_to_be16(RPMB_WRITE_DATA);
			rpmbdata.icmd.frames[iCnt].addr = cpu_to_be16(blkaddr);
			rpmbdata.icmd.frames[iCnt].block_count =
				cpu_to_be16(tran_blkcnt);
			rpmbdata.icmd.frames[iCnt].write_counter =
				cpu_to_be32(wc);

			if (left_size >= RPMB_SZ_DATA)
				tran_size = RPMB_SZ_DATA;
			else
				tran_size = left_size;

			ret = copy_from_user(rpmbdata.icmd.frames[iCnt].data,
				param->databytes +
				(i * MAX_RPMB_TRANSFER_BLK * RPMB_SZ_DATA +
				(iCnt * RPMB_SZ_DATA)),
				tran_size);
			if (ret) {
				MSG(ERR, "%s, copy from user failed: %x\n",
					__func__, ret);
				ret = -EFAULT;
				goto out;
			}

			left_size -= tran_size;

			rpmb_req_copy_data_for_hmac(
				dataBuf, &rpmbdata.icmd.frames[iCnt]);

			dataBuf += size_for_hmac;
		}

		iCnt--;

		if (hmac_sha256(rpmb_key, RPMB_SZ_KEY, dataBuf_start,
				RPMB_SZ_CAL_HMAC * tran_blkcnt,
				rpmbdata.icmd.frames[iCnt].key_mac) != 0)
			MSG(ERR, "hmac_sha256() return error!\n");

		/*
		 * Send write data request.
		 */
		#ifdef __RPMB_KERNEL_NL_SUPPORT
		ret = nl_rpmb_cmd_req(&rpmbdata);
		#else
		ret = rpmb_cmd_req(rawdev_ufs_rpmb, &rpmbdata);
		#endif

		if (ret) {
			MSG(ERR, "%s, nl_rpmb_cmd_req IO error!!!(0x%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * Authenticate write result response.
		 * 1. authenticate hmac.
		 * 2. check result.
		 * 3. compare write counter is increamented.
		 */
		if (hmac_sha256(rpmb_key, RPMB_SZ_KEY,
				rpmbdata.ocmd.frames->data,
				RPMB_SZ_CAL_HMAC, hmac) != 0)
			MSG(ERR, "hmac_sha256() return error!\n");

		if (memcmp(hmac, rpmbdata.ocmd.frames->key_mac,
			   RPMB_SZ_MAC) != 0) {
			MSG(ERR, "%s, hmac compare error!!!\n", __func__);
			ret = RPMB_HMAC_ERROR;
			break;
		}

		if (rpmbdata.ocmd.frames->result) {
			MSG(ERR, "%s, result error!!! (0x%x)\n", __func__,
				cpu_to_be16(rpmbdata.ocmd.frames->result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		if (cpu_to_be32(rpmbdata.ocmd.frames->write_counter) !=
			wc + 1) {
			MSG(ERR, "%s, write counter error!!! (0x%x)\n",
			    __func__,
			    cpu_to_be32(rpmbdata.ocmd.frames->write_counter));
			ret = RPMB_WC_ERROR;
			break;
		}

		blkaddr += tran_blkcnt;
		left_blkcnt -= tran_blkcnt;
		i++;

		kfree(rpmbdata.icmd.frames);
		kfree(rpmbdata.ocmd.frames);
		kfree(dataBuf_start);
	};

out:
	if (ret) {
		kfree(rpmbdata.icmd.frames);
		kfree(rpmbdata.ocmd.frames);
		kfree(dataBuf_start);
	}

	if (left_blkcnt || left_size) {
		MSG(ERR, "left_blkcnt or left_size is not empty!!!!!!\n");
		return RPMB_TRANSFER_NOT_COMPLETE;
	}

	MSG(DBG_INFO, "%s end!!!\n", __func__);

	return ret;
}

static int rpmb_req_ioctl_read_data_ufs(struct rpmb_ioc_param *param)
{
	struct rpmb_data rpmbdata;
	struct rpmb_dev *rawdev_ufs_rpmb;
	u32 i, tran_size, left_size = param->data_len;
	u16 iCnt, tran_blkcnt, left_blkcnt;
	u16 blkaddr;
	u8 nonce[RPMB_SZ_NONCE] = {0};
	u8 hmac[RPMB_SZ_MAC], rpmb_key[RPMB_SZ_KEY];
	u8 *dataBuf, *dataBuf_start;
	size_t size_for_hmac;
	int ret = 0;
	u8 user_param_data;

	MSG(DBG_INFO, "%s start!!!\n", __func__);

	ret = get_user(user_param_data, param->databytes);
	if (ret != 0)
		return -EFAULT;

	ret = get_user(user_param_data, param->keybytes);
	if (ret != 0)
		return -EFAULT;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	/* copy key first */
	ret = copy_from_user(rpmb_key, param->keybytes, RPMB_SZ_KEY);
	if (ret != 0)
		return -EFAULT;

	i = 0;
	tran_blkcnt = 0;
	dataBuf = NULL;
	dataBuf_start = NULL;

	left_blkcnt = (u16)((param->data_len % RPMB_SZ_DATA) ?
				(param->data_len / RPMB_SZ_DATA + 1) :
				(param->data_len / RPMB_SZ_DATA));

	blkaddr = param->addr;

	while (left_blkcnt) {

		if (left_blkcnt >= MAX_RPMB_TRANSFER_BLK)
			tran_blkcnt = MAX_RPMB_TRANSFER_BLK;
		else
			tran_blkcnt = left_blkcnt;

		MSG(DBG_INFO, "%s, left_blkcnt = 0x%x, tran_blkcnt = 0x%x\n",
			__func__, left_blkcnt, tran_blkcnt);

		/*
		 * initial frame buffers
		 */

		rpmbdata.icmd.frames = rpmb_alloc_frames(1);

		if (rpmbdata.icmd.frames == NULL)
			return RPMB_ALLOC_ERROR;

		rpmbdata.ocmd.frames = rpmb_alloc_frames(tran_blkcnt);

		if (rpmbdata.ocmd.frames == NULL) {
			kfree(rpmbdata.icmd.frames);
			return RPMB_ALLOC_ERROR;
		}

		/*
		 * Initial data buffer for HMAC computation.
		 * Since HAMC computation tool which we use needs
		 * consecutive data buffer.
		 * Pre-alloced it.
		 */

		dataBuf_start = dataBuf =
			kzalloc(RPMB_SZ_CAL_HMAC * tran_blkcnt, 0);
		if (!dataBuf_start) {
			kfree(rpmbdata.icmd.frames);
			kfree(rpmbdata.ocmd.frames);
			return RPMB_ALLOC_ERROR;
		}

		get_random_bytes(nonce, (int)RPMB_SZ_NONCE);

		/*
		 * Prepare request read data frame.
		 *
		 * Input frame (in view of device) only needs addr and nonce.
		 */

		rpmbdata.req_type = RPMB_READ_DATA;
		rpmbdata.icmd.nframes = 1;
		rpmbdata.icmd.frames->req_resp = cpu_to_be16(RPMB_READ_DATA);
		rpmbdata.icmd.frames->addr = cpu_to_be16(blkaddr);
		rpmbdata.icmd.frames->block_count = cpu_to_be16(tran_blkcnt);
		memcpy(rpmbdata.icmd.frames->nonce, nonce, RPMB_SZ_NONCE);

		/* output frames (in view of device) */

		rpmbdata.ocmd.nframes = tran_blkcnt;

		#ifdef __RPMB_KERNEL_NL_SUPPORT
		ret = nl_rpmb_cmd_req(&rpmbdata);
		#else
		ret = rpmb_cmd_req(rawdev_ufs_rpmb, &rpmbdata);
		#endif

		if (ret) {
			MSG(ERR, "%s, nl_rpmb_cmd_req IO error!!!(0x%x)\n",
				__func__, ret);
			break;
		}

		/*
		 * Retrieve every data frame one by one.
		 */

		/* size for hmac calculation: 512 - 228 = 284 */
		size_for_hmac = sizeof(struct rpmb_frame) -
				offsetof(struct rpmb_frame, data);

		for (iCnt = 0; iCnt < tran_blkcnt; iCnt++) {

			if (left_size >= RPMB_SZ_DATA)
				tran_size = RPMB_SZ_DATA;
			else
				tran_size = left_size;

			/*
			 * dataBuf used for hmac calculation. we need to
			 * aggregate each block's data till to type field.
			 * each block has 284 bytes (size_for_hmac) need
			 * aggregation.
			 */
			rpmb_req_copy_data_for_hmac(
				dataBuf, &rpmbdata.ocmd.frames[iCnt]);

			dataBuf += size_for_hmac;

			ret = copy_to_user(param->databytes  +
				i * MAX_RPMB_TRANSFER_BLK * RPMB_SZ_DATA +
				(iCnt * RPMB_SZ_DATA),
				rpmbdata.ocmd.frames[iCnt].data, tran_size);
			if (ret) {
				ret = -EFAULT;
				goto out;
			}

			left_size -= tran_size;
		}

		iCnt--;

		/*
		 * Authenticate response read data frame.
		 */
		if (hmac_sha256(rpmb_key, RPMB_SZ_KEY,
			    dataBuf_start, size_for_hmac * tran_blkcnt,
			    hmac) != 0)
			MSG(ERR, "hmac_sha256() return error!\n");

		if (memcmp(hmac, rpmbdata.ocmd.frames[iCnt].key_mac,
			RPMB_SZ_MAC) != 0) {
			MSG(ERR, "%s, hmac compare error!!!\n", __func__);
			ret = RPMB_HMAC_ERROR;
			break;
		}

		if (memcmp(nonce, rpmbdata.ocmd.frames[iCnt].nonce,
			RPMB_SZ_NONCE) != 0) {
			MSG(ERR, "%s, nonce compare error!!!\n", __func__);
			ret = RPMB_NONCE_ERROR;
			break;
		}

		if (rpmbdata.ocmd.frames[iCnt].result) {
			MSG(ERR, "%s, result error!!! (0x%x)\n",
			    __func__,
			    cpu_to_be16p(&rpmbdata.ocmd.frames[iCnt].result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		blkaddr += tran_blkcnt;
		left_blkcnt -= tran_blkcnt;
		i++;

		kfree(rpmbdata.icmd.frames);
		kfree(rpmbdata.ocmd.frames);
		kfree(dataBuf_start);
	};

out:
	if (ret) {
		kfree(rpmbdata.icmd.frames);
		kfree(rpmbdata.ocmd.frames);
		kfree(dataBuf_start);
	}

	if (left_blkcnt || left_size) {
		MSG(ERR, "left_blkcnt or left_size is not empty!!!!!!\n");
		return RPMB_TRANSFER_NOT_COMPLETE;
	}

	MSG(DBG_INFO, "%s end!!!\n", __func__);

	return ret;
}

/*
 * End of above.
 *
 ******************************************************************************/


#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT)
static enum mc_result rpmb_gp_execute_ufs(u32 cmdId)
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

		ret = rpmb_req_get_wc_ufs(NULL, NULL, rpmb_gp_dci->request.frame);

		break;

	case DCI_RPMB_CMD_WRITE_DATA:

		MSG(DBG_INFO, "%s: DCI_RPMB_CMD_WRITE_DATA\n", __func__);

		ret = rpmb_req_write_data_ufs(rpmb_gp_dci->request.frame,
					rpmb_gp_dci->request.blks);

		break;

	case DCI_RPMB_CMD_PROGRAM_KEY:
		MSG(INFO, "%s: DCI_RPMB_CMD_PROGRAM_KEY.\n", __func__);
		rpmb_dump_frame(rpmb_gp_dci->request.frame);

		ret = rpmb_req_program_key_ufs(rpmb_gp_dci->request.frame, 1);

		break;

	default:
		MSG(ERR, "%s: receive an unknown command id(%d).\n",
			__func__, cmdId);
		break;

	}

	return MC_DRV_OK;
}
#endif
#endif

#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT)
static int rpmb_gp_listenDci(void *arg)
{
	enum mc_result mc_ret;
	u32 cmdId;

	MSG(INFO, "%s: DCI listener.\n", __func__);

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
		mc_ret = rpmb_gp_execute_ufs(cmdId);

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

static enum mc_result rpmb_gp_open_session(void)
{
	int cnt = 0;
	enum mc_result mc_ret = MC_DRV_ERR_UNKNOWN;

	MSG(INFO, "%s start\n", __func__);

	do {
		msleep(2000);

		/* open device */
		mc_ret = mc_open_device(rpmb_gp_devid);
		if (mc_ret == MC_DRV_ERR_NOT_IMPLEMENTED) {
			MSG(INFO, "%s, mc_open_device not support\n", __func__);
			break;
		} else	if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s, mc_open_device failed: %d\n",
				__func__, mc_ret);
			cnt++;
			continue;
		}

		MSG(INFO, "%s, mc_open_device success.\n", __func__);


		/* allocating WSM for DCI */
		mc_ret = mc_malloc_wsm(rpmb_gp_devid, 0,
					(u32)sizeof(struct dciMessage_t),
					(uint8_t **)&rpmb_gp_dci, 0);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s, mc_malloc_wsm failed: %d\n",
				__func__, mc_ret);
			mc_ret = mc_close_device(rpmb_gp_devid);
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
					 (u32)sizeof(struct dciMessage_t));

		if (mc_ret != MC_DRV_OK) {
			MSG(ERR,
				"%s, mc_open_session failed, result(%d), cnt(%d)\n",
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
	enum mc_result ret;

	MSG(INFO, "%s start\n", __func__);

	ret = rpmb_gp_open_session();
	MSG(INFO, "%s rpmb_gp_open_session, ret = %x\n", __func__, ret);

	return 0;
}
#endif

static int rpmb_open(struct inode *pinode, struct file *pfile)
{
	return 0;
}

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK)
static long rpmb_ioctl_ufs(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	unsigned long n;
	struct rpmb_ioc_param param;

	n = copy_from_user(&param, (void *)arg, sizeof(param));

	if (n != 0UL) {
		MSG(ERR, "%s, copy from user failed: %x\n", __func__, err);
		return -EFAULT;
	}

	switch (cmd) {

	case RPMB_IOCTL_PROGRAM_KEY:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_PROGRAM_KEY not support\n",
		    __func__);

		break;

	case RPMB_IOCTL_READ_DATA:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_READ_DATA!!!!\n", __func__);

		err = rpmb_req_ioctl_read_data_ufs(&param);

		if (err != 0) {
			MSG(ERR, "%s, rpmb_req_ioctl_read_data_ufs() error!(%x)\n",
			    __func__, err);
			break;
		}

		n = copy_to_user((void *)arg, &param, sizeof(param));

		if (n != 0UL) {
			MSG(ERR, "%s, copy to user failed: %x\n",
			    __func__, err);
			err = -EFAULT;
			break;
		}

		break;

	case RPMB_IOCTL_WRITE_DATA:

		MSG(DBG_INFO, "%s, cmd = RPMB_IOCTL_WRITE_DATA!!!\n", __func__);

		err = rpmb_req_ioctl_write_data_ufs(&param);

		if (err != 0)
			MSG(ERR, "%s, rpmb_req_ioctl_write_data_ufs() error!(%x)\n",
			    __func__, err);

		break;

	default:
		MSG(ERR, "%s, wrong ioctl code (%d)!!!\n", __func__, cmd);
		err = -ENOTTY;
		break;
	}

	return err;
}
#endif

static int rpmb_close(struct inode *pinode, struct file *pfile)
{
	int ret = 0;

	MSG(INFO, "%s, !!!!!!!!!!!!\n", __func__);

	return ret;
}

#ifdef __RPMB_KERNEL_NL_SUPPORT
static int rpmb_mtk_snd_msg(void *pbuf, u16 len)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	static int nlseq;
	int ret;

	skb = nlmsg_new(len, GFP_ATOMIC);
	if (!skb) {
		MSG(ERR, "%s netlink alloc failure\n", __func__);
		ret = -ENOBUFS;
		goto send_fail;
	}

	nlh = nlmsg_put(skb, nl_pid, nlseq, NETLINK_RPMB_MTK, len, 0);
	if (!nlh) {
		MSG(ERR, "%s nlmsg_put failure\n", __func__);
		ret = -ENOBUFS;
		nlmsg_free(skb);
		goto send_fail_skb;
	}

	memcpy(nlmsg_data(nlh), pbuf, len);

	ret = netlink_unicast(rpmb_mtk_sock, skb, nl_pid, MSG_DONTWAIT);
	if (ret < 0) {
		MSG(ERR, "%s send failed ret=%d, pid=%d\n",
			__func__, ret, nl_pid);
		goto send_fail;
	}

	return 0;

send_fail_skb:
	kfree_skb(skb);

send_fail:
	return ret;
}

static void rpmb_mtk_rcv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	void *data;
	struct nl_rpmb_send_req *req = &nl_rpmb_req;

	if (skb->len >= NLMSG_HDRLEN) {

		nlh = nlmsg_hdr(skb);
		data = NLMSG_DATA(nlh);

		/* Copy data to rpmb frame */
		memcpy(req, data, 16);
		if (req->read_size % 512 == 0 &&
			req->read_size <= MAX_RPMB_REQUEST_SIZE) {
			memcpy(&req->payload[0], (void *)(data + 16),
				req->read_size);
		}

		/* Wakeup rpmb thread */
		rpmb_done_flag = true;
		wake_up(&wait_rpmb);
	}
}

static int rpmb_create_netlink(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = rpmb_mtk_rcv_msg,
	};

	rpmb_mtk_sock = netlink_kernel_create(&init_net,
		NETLINK_RPMB_MTK, &cfg);

	if (rpmb_mtk_sock == NULL) {
		MSG(ERR, "netlink_kernel_create error\n");
		return -EIO;
	}

	MSG(INFO, "%s netlink_kernel_create protocol = %d\n",
		__func__, NETLINK_RPMB_MTK);

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK)
static const struct file_operations rpmb_fops_ufs = {
	.owner = THIS_MODULE,
	.open = rpmb_open,
	.release = rpmb_close,
	.unlocked_ioctl = rpmb_ioctl_ufs,
	.write = NULL,
	.read = NULL,
};
#endif

static int __init rpmb_init(void)
{
	int alloc_ret;
	int cdev_ret = -1;
	unsigned int major;
	dev_t dev;
	struct device *device = NULL;

	MSG(INFO, "%s start\n", __func__);

	alloc_ret = alloc_chrdev_region(&dev, 0, 1, RPMB_NAME);

	if (alloc_ret) {
		MSG(ERR, "%s, init alloc_chrdev_region failed!\n", __func__);
		goto error;
	}

	major = MAJOR(dev);

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK)
	cdev_init(&rpmb_cdev, &rpmb_fops_ufs);
#endif
	rpmb_cdev.owner = THIS_MODULE;

	cdev_ret = cdev_add(&rpmb_cdev, MKDEV(major, 0U), 1);
	if (cdev_ret) {
		MSG(ERR, "%s, init cdev_add failed!\n", __func__);
		goto error;
	}

#ifdef __RPMB_IOCTL_SUPPORT
	mtk_rpmb_class = class_create(THIS_MODULE, RPMB_NAME);

	if (IS_ERR(mtk_rpmb_class)) {
		MSG(ERR, "%s, init class_create failed!\n", __func__);
		goto error;
	}

	device = device_create(mtk_rpmb_class, NULL, MKDEV(major, 0), NULL,
		RPMB_NAME "%d", 0);
#endif
	if (IS_ERR(device)) {
		MSG(ERR, "%s, init device_create failed!\n", __func__);
		goto error;
	}

#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT)
	open_th = kthread_run(rpmb_thread, NULL, "rpmb_open");
	if (IS_ERR(open_th))
		MSG(ERR, "%s, init kthread_run failed!\n", __func__);

#ifdef __RPMB_KERNEL_NL_SUPPORT
	if (rpmb_create_netlink()) {
		MSG(ERR, "%s, init netlink failed!\n", __func__);
		goto error;
	}

	init_waitqueue_head(&wait_rpmb);
	mutex_init(&rpmb_lock);
#endif
#endif

	MSG(INFO, "%s end!!!!\n", __func__);

	return 0;

error:
#ifdef __RPMB_IOCTL_SUPPORT
	if (mtk_rpmb_class)
		class_destroy(mtk_rpmb_class);
#endif
	if (cdev_ret == 0)
		cdev_del(&rpmb_cdev);

	if (alloc_ret == 0)
		unregister_chrdev_region(dev, 1);

	return 0;
}

late_initcall(rpmb_init);

MODULE_DESCRIPTION("RPMB class");
MODULE_LICENSE("GPL v2");
