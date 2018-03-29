
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

#include <linux/scatterlist.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include "drivers/mmc/card/queue.h"

#include "emmc_rpmb.h"
#include "mt_sd.h"

/* TEE usage */
#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
#include "mobicore_driver_api.h"
#include "drrpmb_Api.h"
#include "drrpmb_gp_Api.h"

static struct mc_uuid_t rpmb_uuid = RPMB_UUID;
static struct mc_session_handle rpmb_session = {0};
static u32 rpmb_devid = MC_DEVICE_ID_DEFAULT;
static dciMessage_t *rpmb_dci;

static struct mc_uuid_t rpmb_gp_uuid = RPMB_GP_UUID;
static struct mc_session_handle rpmb_gp_session = {0};
static u32 rpmb_gp_devid = MC_DEVICE_ID_DEFAULT;
static dciMessage_t *rpmb_gp_dci;

#endif

#define RPMB_NAME "emmcrpmb"

#define DEFAULT_HANDLES_NUM (64)
#define MAX_OPEN_SESSIONS (0xffffffff - 1)


/* Debug message event */
#define DBG_EVT_NONE (0) /* No event */
#define DBG_EVT_CMD  (1 << 0)/* SEC CMD related event */
#define DBG_EVT_FUNC (1 << 1)/* SEC function event */
#define DBG_EVT_INFO (1 << 2)/* SEC information event */
#define DBG_EVT_WRN  (1 << 30) /* Warning event */
#define DBG_EVT_ERR  (1 << 31) /* Error event */
#define DBG_EVT_ALL  (0xffffffff)

#define DBG_EVT_MASK (DBG_EVT_ERR)

#define MSG(evt, fmt, args...) \
do {\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		pr_err("[%s] "fmt, RPMB_NAME, ##args); \
	} \
} while (0)

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
#define RPMB_DATA_BUFF_SIZE (1024 * 33)
static unsigned char *rpmb_buffer;
#endif

struct task_struct *open_th;
struct task_struct *rpmbDci_th;
struct task_struct *rpmb_gp_Dci_th;


static struct cdev rpmb_dev;
static struct class *rpmb_class;

static DEFINE_MUTEX(rpmb_lock);

/*
 * This is an alternative way to get mmc_card strcuture from mmc_host which set from msdc driver with
 * this callback function.
 * The strength is we don't have to extern msdc_host_host global variable, extern global is very bad...
 * The weakness is every platform driver needs to add this callback to give rpmb driver the mmc_host
 * structure and then we could know card.
 *
 * Finally, I decide to ignore its strength, because the weakness is more important.
 * If every projects have to add this callback, the operation is complicated.
*/

#if 0
struct mmc_host *emmc_rpmb_host;

void emmc_rpmb_set_host(void *mmc_host)
{
	emmc_rpmb_host = mmc_host;
}
#endif

int hmac_sha256(const char *key, u32 klen, const char *str, u32 len, u8 *hmac)
{
	struct shash_desc *shash;
	struct crypto_shash *hmacsha256 = crypto_alloc_shash("hmac(sha256)", 0, 0);
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


/*
 * CHECK THIS!!! Copy from block.c mmc_blk_data structure.
 */
struct emmc_rpmb_blk_data {
	spinlock_t lock;
	struct gendisk *disk;
	struct mmc_queue queue;
	struct list_head part;

	unsigned int flags;
	unsigned int usage;
	unsigned int read_only;
	unsigned int part_type;
	unsigned int name_idx;
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


/*
 * CHECK THIS!!! Copy from block.c mmc_blk_part_switch.
 * Since it is static inline function, we cannot extern to use it.
 * For syncing block data, this is the only way.
 */
int emmc_rpmb_switch(struct mmc_card *card, struct emmc_rpmb_blk_data *md)
{
	int ret;
	struct emmc_rpmb_blk_data *main_md = mmc_get_drvdata(card);

	if (main_md->part_curr == md->part_type)
		return 0;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (card->ext_csd.cmdq_mode_en) {
		ret = mmc_blk_cmdq_switch(card, 0);
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

	main_md->part_curr = md->part_type;
	return 0;
}

static void emmc_rpmb_dump_frame(u8 *data_frame)
{
	MSG(INFO, "mac, frame[196]=%x\n", data_frame[196]);
	MSG(INFO, "mac, frame[197]=%x\n", data_frame[197]);
	MSG(INFO, "mac, frame[198]=%x\n", data_frame[198]);
	MSG(INFO, "data,frame[228]=%x\n", data_frame[228]);
	MSG(INFO, "data,frame[229]=%x\n", data_frame[229]);
	MSG(INFO, "nonce, frame[484]=%x\n", data_frame[484]);
	MSG(INFO, "nonce, frame[485]=%x\n", data_frame[485]);
	MSG(INFO, "nonce, frame[486]=%x\n", data_frame[486]);
	MSG(INFO, "nonce, frame[487]=%x\n", data_frame[487]);
	MSG(INFO, "wc, frame[500]=%x\n", data_frame[500]);
	MSG(INFO, "wc, frame[501]=%x\n", data_frame[501]);
	MSG(INFO, "wc, frame[502]=%x\n", data_frame[502]);
	MSG(INFO, "wc, frame[503]=%x\n", data_frame[503]);
	MSG(INFO, "addr, frame[504]=%x\n", data_frame[504]);
	MSG(INFO, "addr, frame[505]=%x\n", data_frame[505]);
	MSG(INFO, "blkcnt,frame[506]=%x\n", data_frame[506]);
	MSG(INFO, "blkcnt,frame[507]=%x\n", data_frame[507]);
	MSG(INFO, "result, frame[508]=%x\n", data_frame[508]);
	MSG(INFO, "result, frame[509]=%x\n", data_frame[509]);
	MSG(INFO, "type, frame[510]=%x\n", data_frame[510]);
	MSG(INFO, "type, frame[511]=%x\n", data_frame[511]);
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
	if ((req_type == RPMB_REQ && type == RPMB_WRITE_DATA) || type == RPMB_PROGRAM_KEY)
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
		err = emmc_rpmb_send_command(card, data_frame, blks, type, RPMB_REQ);
	else
		err = emmc_rpmb_send_command(card, data_frame, 1, type, RPMB_REQ);

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
		err = emmc_rpmb_send_command(card, data_frame, 1, RPMB_RESULT_READ, RPMB_REQ);
		if (err) {
			MSG(ERR, "%s step 2, request result failed (%d)\n", __func__, err);
			goto out;
		}
	}

	/*
	 * STEP 3: get response from RPMB partition
	 */
	data_frame[RPMB_TYPE_BEG] = 0;
	data_frame[RPMB_TYPE_BEG + 1] = type;

	if (type == RPMB_READ_DATA)
		err = emmc_rpmb_send_command(card, data_frame, blks, type, RPMB_RESP);
	else
		err = emmc_rpmb_send_command(card, data_frame, 1, type, RPMB_RESP);

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

	/* emmc_rpmb_dump_frame(rpmb_req->data_frame);    */

	md = mmc_get_drvdata(card);

	list_for_each_entry(part_md, &md->part, part) {
		if (part_md->part_type == EXT_CSD_PART_CONFIG_ACC_RPMB)
			break;
	}

	/* MSG(INFO, "%s start.\n", __func__);   */

	mmc_claim_host(card->host);

	/*
	 * STEP1: Switch to RPMB partition.
	 */
	ret = emmc_rpmb_switch(card, part_md);
	if (ret) {
		MSG(ERR, "%s emmc_rpmb_switch failed. (%x)\n", __func__, ret);
		goto error;
	}

	/* MSG(INFO, "%s, emmc_rpmb_switch success.\n", __func__);    */

	/*
	 * STEP2: Start request. (CMD23, CMD25/18 procedure)
	 */
	ret = emmc_rpmb_req_start(card, rpmb_req);
	if (ret) {
		MSG(ERR, "%s emmc_rpmb_req_start failed!! (%x)\n", __func__, ret);
		goto error;
	}

	/* MSG(INFO, "%s end.\n", __func__);    */

error:
	mmc_release_host(card->host);

	emmc_rpmb_dump_frame(rpmb_req->data_frame);

	return ret;
}

/* ********************************************************************************
 *
 * Following are internal APIs. Stand-alone driver without TEE.
 *
 *
 **********************************************************************************/
int emmc_rpmb_req_set_key(struct mmc_card *card, u8 *key)
{
	struct emmc_rpmb_req rpmb_req;
	struct s_rpmb *rpmb_frame;
	int ret;

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
		MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
		goto free;
	}

	if (rpmb_frame->result) {
		MSG(ERR, "%s, result error!!! (%x)\n", __func__, cpu_to_be16p(&rpmb_frame->result));
		ret = RPMB_RESULT_ERROR;
	}

	MSG(INFO, "%s end!!!\n", __func__);

free:
	kfree(rpmb_frame);

	return ret;
}

int emmc_rpmb_req_get_wc(struct mmc_card *card, u8 *key, u32 *wc)
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
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
			break;
		}

		/*
		 * Authenticate response write counter frame.
		 */
		hmac_sha256(key, 32, rpmb_frame->data, 284, hmac);

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
			MSG(ERR, "%s, result error!!! (%x)\n", __func__, cpu_to_be16p(&rpmb_frame->result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		*wc = cpu_to_be32p(&rpmb_frame->write_counter);

	} while (0);

	MSG(INFO, "%s end!!!\n", __func__);

	kfree(rpmb_frame);

	return ret;
}

int emmc_rpmb_req_write_data(struct mmc_card *card, struct rpmb_ioc_param *param)
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



	while (left_blkcnt) {

		if (left_blkcnt >= MAX_RPMB_TRANSFER_BLK)
			tran_blkcnt = MAX_RPMB_TRANSFER_BLK;
		else
			tran_blkcnt = left_blkcnt;

		MSG(INFO, "%s, total_blkcnt=%x, tran_blkcnt=%x\n", __func__, left_blkcnt, tran_blkcnt);

		ret = emmc_rpmb_req_get_wc(card, param->key, &wc);
		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_get_wc error!!!(%x)\n", __func__, ret);
			return ret;
		}

		rpmb_frame = kzalloc(tran_blkcnt * sizeof(struct s_rpmb) + tran_blkcnt * 512, 0);
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
		 * STEP 3(data), prepare every data frame one by one and hook HMAC to the last.
		 */
		for (iCnt = 0; iCnt < tran_blkcnt; iCnt++) {

			/*
			 * Prepare write data frame. need addr, wc, blkcnt, data and mac.
			 */
			rpmb_frame[iCnt].request = cpu_to_be16p(&rpmb_req.type);
			rpmb_frame[iCnt].address = cpu_to_be16p(&blkaddr);
			rpmb_frame[iCnt].write_counter = cpu_to_be32p(&wc);
			rpmb_frame[iCnt].block_count = cpu_to_be16p(&rpmb_req.blk_cnt);

			if (left_size >= RPMB_SZ_DATA)
				tran_size = RPMB_SZ_DATA;
			else
				tran_size = left_size;

			memcpy(rpmb_frame[iCnt].data,
				 param->data + i * MAX_RPMB_TRANSFER_BLK * RPMB_SZ_DATA + (iCnt * RPMB_SZ_DATA),
				 tran_size);
			left_size -= tran_size;


			memcpy(dataBuf, rpmb_frame[iCnt].data, 284);
			dataBuf += 284;

		}

		iCnt--;

		hmac_sha256(param->key, 32, dataBuf_start, 284 * tran_blkcnt, rpmb_frame[iCnt].mac);

		/*
		 * STEP 4, send write data request.
		 */
		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
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
			MSG(ERR, "%s, result error!!! (%x)\n", __func__, cpu_to_be16p(&rpmb_frame->result));
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

		ret = emmc_rpmb_req_get_wc(card, param->key, &wc);
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
		 * Prepare write data frame. need addr, wc, blkcnt, data and mac.
		 */
		rpmb_frame->request = cpu_to_be16p(&rpmb_req.type);
		rpmb_frame->address = cpu_to_be16p(&blkaddr);
		rpmb_frame->write_counter = cpu_to_be32p(&wc);
		rpmb_frame->block_count = cpu_to_be16p(&rpmb_req.blk_cnt);

		if (left_size >= RPMB_SZ_DATA)
			tran_size = RPMB_SZ_DATA;
		else
			tran_size = left_size;

		memcpy(rpmb_frame->data, param->data + iCnt * RPMB_SZ_DATA, tran_size);

		hmac_sha256(param->key, 32, rpmb_frame->data, 284, rpmb_frame->mac);

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
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
			MSG(ERR, "%s, result error!!! (%x)\n", __func__, cpu_to_be16p(&rpmb_frame->result));
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

int emmc_rpmb_req_read_data(struct mmc_card *card, struct rpmb_ioc_param *param)
{
	struct emmc_rpmb_req rpmb_req;
	/* //if we put a large static buffer here, it will build fail.
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

		MSG(INFO, "%s, left_blkcnt=%x, tran_blkcnt=%x\n", __func__, left_blkcnt, tran_blkcnt);

		/*
		 * initial buffer. (since HMAC computation of multi block needs multi buffer, pre-alloced it)
		 */
		rpmb_frame = kzalloc(tran_blkcnt * sizeof(struct s_rpmb) + tran_blkcnt * 512, 0);
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
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
			break;
		}


		/*
		 * STEP 3, retrieve every data frame one by one.
		 */
		for (iCnt = 0; iCnt < tran_blkcnt; iCnt++) {

			if (left_size >= RPMB_SZ_DATA)
				tran_size = RPMB_SZ_DATA;
			else
				tran_size = left_size;

			/*
			 * dataBuf used for hmac calculation. we need to aggregate each block's data till to type field.
			 * each block has 284 bytes need to aggregate.
			*/
			memcpy(dataBuf, rpmb_frame[iCnt].data, 284);
			dataBuf = dataBuf + 284;

			/*
			 * sorry, I shouldn't copy read data to user's buffer now, it should be later
			 * after checking no problem,
			 * but for convenience...you know...
			*/
			memcpy(param->data + i * MAX_RPMB_TRANSFER_BLK * RPMB_SZ_DATA + (iCnt * RPMB_SZ_DATA),
				 rpmb_frame[iCnt].data,
				 tran_size);
			left_size -= tran_size;
		}

		iCnt--;

		/*
		 * Authenticate response read data frame.
		 */
		hmac_sha256(param->key, 32, dataBuf_start, 284 * tran_blkcnt, hmac);

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
			MSG(ERR, "%s, result error!!! (%x)\n", __func__, cpu_to_be16p(&rpmb_frame[iCnt].result));
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
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
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
			MSG(ERR, "%s, result error!!! (%x)\n", __func__, cpu_to_be16p(&rpmb_frame->result));
			ret = RPMB_RESULT_ERROR;
			break;
		}

		if (left_size >= RPMB_SZ_DATA)
			tran_size = RPMB_SZ_DATA;
		else
			tran_size = left_size;

		memcpy(param->data + RPMB_SZ_DATA * iCnt, rpmb_frame->data, tran_size);

		left_size -= tran_size;
		blkaddr++;
	}

	kfree(rpmb_frame);

#endif

	MSG(INFO, "%s end!!!\n", __func__);

	return ret;
}
#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
int ut_rpmb_req_get_max_wr_size(struct mmc_card *card, unsigned int *max_wr_size)
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
		MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
		return ret;
	}
	if (memcmp(nonce, rpmb_frame.nonce, RPMB_SZ_NONCE) != 0) {
		MSG(ERR, "%s, nonce compare error!!!\n", __func__);
		ret = RPMB_NONCE_ERROR;
		return ret;
	}
	if (rpmb_frame.result) {
		MSG(ERR, "%s, result error!!! (%x)\n", __func__, cpu_to_be16p(&rpmb_frame.result));
		ret = RPMB_RESULT_ERROR;
		return cpu_to_be16p(&rpmb_frame.result);
	}
	*wc = cpu_to_be32p(&rpmb_frame.write_counter);
	return ret;
}
EXPORT_SYMBOL(ut_rpmb_req_get_wc);

int ut_rpmb_req_read_data(struct mmc_card *card, struct s_rpmb *param, u32 blk_cnt)/*struct mmc_card *card, */
{
	struct emmc_rpmb_req rpmb_req;
	int ret;

	rpmb_req.type = RPMB_READ_DATA;
	rpmb_req.blk_cnt = blk_cnt;
	rpmb_req.data_frame = (u8 *)param;

	ret = emmc_rpmb_req_handle(card, &rpmb_req);
	if (ret)
		MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(ut_rpmb_req_read_data);

int ut_rpmb_req_write_data(struct mmc_card *card, struct s_rpmb *param, u32 blk_cnt)/*struct mmc_card *card, */
{
	struct emmc_rpmb_req rpmb_req;
	int ret;

	rpmb_req.type = RPMB_WRITE_DATA;
	rpmb_req.blk_cnt = blk_cnt;
	rpmb_req.data_frame = (u8 *)param;

	ret = emmc_rpmb_req_handle(card, &rpmb_req);
	if (ret)
		MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(ut_rpmb_req_write_data);
#endif

/*
 * End of above.
 *
 **********************************************************************************/


#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
static int emmc_rpmb_execute(u32 cmdId)
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
			MSG(ERR, "%s, emmc_rpmb_req_read_data failed!!(%x)\n", __func__, ret);

		break;

	case DCI_RPMB_CMD_GET_WCNT:
		MSG(INFO, "%s: DCI_RPMB_CMD_GET_WCNT.\n", __func__);

		rpmb_req.type = RPMB_GET_WRITE_COUNTER;
		rpmb_req.blk_cnt = rpmb_dci->request.blks;
		rpmb_req.addr = rpmb_dci->request.addr;
		rpmb_req.data_frame = rpmb_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n", __func__, ret);

		break;

	case DCI_RPMB_CMD_WRITE_DATA:
		MSG(INFO, "%s: DCI_RPMB_CMD_WRITE_DATA.\n", __func__);

		rpmb_req.type = RPMB_WRITE_DATA;
		rpmb_req.blk_cnt = rpmb_dci->request.blks;
		rpmb_req.addr = rpmb_dci->request.addr;
		rpmb_req.data_frame = rpmb_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n", __func__, ret);

		break;

	default:
		MSG(ERR, "%s: receive an unknown command id(%d).\n", __func__, cmdId);
		break;

	}

	return 0;
}

int emmc_rpmb_listenDci(void *data)
{
	enum mc_result mc_ret;
	u32 cmdId;

	MSG(INFO, "%s: DCI listener.\n", __func__);

	for (;;) {

		MSG(INFO, "%s: Waiting for notification\n", __func__);

		/* Wait for notification from SWd */
		mc_ret = mc_wait_notification(&rpmb_session, MC_INFINITE_TIMEOUT);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s: mcWaitNotification failed, mc_ret=%d\n", __func__, mc_ret);
			break;
		}

		cmdId = rpmb_dci->command.header.commandId;

		MSG(INFO, "%s: wait notification done!! cmdId = %x\n", __func__, cmdId);


		/* Received exception. */
		mc_ret = emmc_rpmb_execute(cmdId);

		/* Notify the STH*/
		mc_ret = mc_notify(&rpmb_session);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s: mcNotify returned: %d\n", __func__, mc_ret);
			break;
		}
	}

	return 0;
}


static int emmc_rpmb_open_session(void)
{
	int cnt = 0;
	enum mc_result mc_ret = MC_DRV_ERR_UNKNOWN;

	MSG(INFO, "%s start\n", __func__);

	do {
		msleep(2000);

		/* open device */
		mc_ret = mc_open_device(rpmb_devid);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s, mc_open_device failed: %d\n", __func__, mc_ret);
			cnt++;
			continue;
		}

		MSG(INFO, "%s, mc_open_device success.\n", __func__);


		/* allocating WSM for DCI */
		mc_ret = mc_malloc_wsm(rpmb_devid, 0, sizeof(dciMessage_t), (uint8_t **)&rpmb_dci, 0);
		if (mc_ret != MC_DRV_OK) {
			mc_close_device(rpmb_devid);
			MSG(ERR, "%s, mc_malloc_wsm failed: %d\n", __func__, mc_ret);
			cnt++;
			continue;
		}

		MSG(INFO, "%s, mc_malloc_wsm success.\n", __func__);
		MSG(INFO, "uuid[0]=%d, uuid[1]=%d, uuid[2]=%d, uuid[3]=%d\n",
			rpmb_uuid.value[0], rpmb_uuid.value[1], rpmb_uuid.value[2], rpmb_uuid.value[3]);

		rpmb_session.device_id = rpmb_devid;

		/* open session */
		mc_ret = mc_open_session(&rpmb_session,
					 &rpmb_uuid,
					 (uint8_t *) rpmb_dci,
					 sizeof(dciMessage_t));

		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s, mc_open_session failed.(%d)\n", __func__, cnt);

			mc_ret = mc_free_wsm(rpmb_devid, (uint8_t *)rpmb_dci);
			MSG(ERR, "%s, free wsm result (%d)\n", __func__, mc_ret);

			mc_ret = mc_close_device(rpmb_devid);
			MSG(ERR, "%s, try free wsm and close device\n", __func__);
			cnt++;
			continue;
		}

		/* create a thread for listening DCI signals */
		rpmbDci_th = kthread_run(emmc_rpmb_listenDci, NULL, "rpmb_Dci");
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

static int emmc_rpmb_gp_execute(u32 cmdId)
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
			MSG(ERR, "%s, emmc_rpmb_req_read_data failed!!(%x)\n", __func__, ret);

		break;

	case DCI_RPMB_CMD_GET_WCNT:
		MSG(INFO, "%s: DCI_RPMB_CMD_GET_WCNT.\n", __func__);

		rpmb_req.type = RPMB_GET_WRITE_COUNTER;
		rpmb_req.blk_cnt = rpmb_gp_dci->request.blks;
		rpmb_req.addr = rpmb_gp_dci->request.addr;
		rpmb_req.data_frame = rpmb_gp_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n", __func__, ret);

		break;

	case DCI_RPMB_CMD_WRITE_DATA:
		MSG(INFO, "%s: DCI_RPMB_CMD_WRITE_DATA.\n", __func__);

		rpmb_req.type = RPMB_WRITE_DATA;
		rpmb_req.blk_cnt = rpmb_gp_dci->request.blks;
		rpmb_req.addr = rpmb_gp_dci->request.addr;
		rpmb_req.data_frame = rpmb_gp_dci->request.frame;

		ret = emmc_rpmb_req_handle(card, &rpmb_req);
		if (ret)
			MSG(ERR, "%s, emmc_rpmb_req_handle failed!!(%x)\n", __func__, ret);

		break;

	default:
		MSG(ERR, "%s: receive an unknown command id(%d).\n", __func__, cmdId);
		break;

	}

	return 0;
}

int emmc_rpmb_gp_listenDci(void *data)
{
	enum mc_result mc_ret;
	u32 cmdId;

	MSG(INFO, "%s: DCI listener.\n", __func__);

	for (;;) {

		MSG(INFO, "%s: Waiting for notification\n", __func__);

		/* Wait for notification from SWd */
		mc_ret = mc_wait_notification(&rpmb_gp_session, MC_INFINITE_TIMEOUT);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s: mcWaitNotification failed, mc_ret=%d\n", __func__, mc_ret);
			break;
		}

		cmdId = rpmb_gp_dci->command.header.commandId;

		MSG(INFO, "%s: wait notification done!! cmdId = %x\n", __func__, cmdId);


		/* Received exception. */
		mc_ret = emmc_rpmb_gp_execute(cmdId);

		/* Notify the STH*/
		mc_ret = mc_notify(&rpmb_gp_session);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s: mcNotify returned: %d\n", __func__, mc_ret);
			break;
		}
	}

	return 0;
}

static int emmc_rpmb_gp_open_session(void)
{
	int cnt = 0;
	enum mc_result mc_ret = MC_DRV_ERR_UNKNOWN;

	MSG(INFO, "%s start\n", __func__);

	do {
		msleep(2000);

		/* open device */
		mc_ret = mc_open_device(rpmb_gp_devid);
		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s, mc_open_device failed: %d\n", __func__, mc_ret);
			cnt++;
			continue;
		}

		MSG(INFO, "%s, mc_open_device success.\n", __func__);


		/* allocating WSM for DCI */
		mc_ret = mc_malloc_wsm(rpmb_gp_devid, 0, sizeof(dciMessage_t), (uint8_t **)&rpmb_gp_dci, 0);
		if (mc_ret != MC_DRV_OK) {
			mc_close_device(rpmb_gp_devid);
			MSG(ERR, "%s, mc_malloc_wsm failed: %d\n", __func__, mc_ret);
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
					 sizeof(dciMessage_t));

		if (mc_ret != MC_DRV_OK) {
			MSG(ERR, "%s, mc_open_session failed.(%d)\n", __func__, cnt);

			mc_ret = mc_free_wsm(rpmb_gp_devid, (uint8_t *)rpmb_gp_dci);
			MSG(ERR, "%s, free wsm result (%d)\n", __func__, mc_ret);

			mc_ret = mc_close_device(rpmb_gp_devid);
			MSG(ERR, "%s, try free wsm and close device\n", __func__);
			cnt++;
			continue;
		}

		/* create a thread for listening DCI signals */
		rpmb_gp_Dci_th = kthread_run(emmc_rpmb_gp_listenDci, NULL, "rpmb_gp_Dci");
		if (IS_ERR(rpmb_gp_Dci_th))
			MSG(ERR, "%s, init kthread_run failed!\n", __func__);
		else
			break;

	} while (cnt < 30);

	if (cnt >= 30)
		MSG(ERR, "%s, open session failed!!!\n", __func__);


	MSG(ERR, "%s end, mc_ret = %x\n", __func__, mc_ret);

	return mc_ret;
}


static int emmc_rpmb_thread(void *context)
{
	int ret;

	MSG(INFO, "%s start\n", __func__);

	ret = emmc_rpmb_open_session();
	MSG(INFO, "%s emmc_rpmb_open_session, ret = %x\n", __func__, ret);

	ret = emmc_rpmb_gp_open_session();
	MSG(INFO, "%s emmc_rpmb_gp_open_session, ret = %x\n", __func__, ret);

	return 0;
}
#endif

static int emmc_rpmb_open(struct inode *inode, struct file *file)
{
	MSG(INFO, "%s, !!!!!!!!!!!!\n", __func__);
#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	rpmb_buffer = kzalloc(RPMB_DATA_BUFF_SIZE, 0);
	if (rpmb_buffer == NULL) {
		MSG(ERR, "%s, rpmb kzalloc memory fail!!!\n", __func__);
		return -1;
	}
	MSG(INFO, "%s, rpmb kzalloc memory done!!!\n", __func__);
#endif
	return 0;
}

static long emmc_rpmb_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct mmc_card *card = mtk_msdc_host[0]->mmc->card;
	struct rpmb_ioc_param param;
	int ret = 0;
#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	struct rpmb_infor rpmbinfor;

	memset(&rpmbinfor, 0, sizeof(struct rpmb_infor));
#endif

	/* MSG(INFO, "%s, !!!!!!!!!!!!\n", __func__);    */

	err = copy_from_user(&param, (void *)arg, sizeof(param));
	if (err < 0) {
		MSG(ERR, "%s, err=%x\n", __func__, err);
		return -1;
	}

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	if ((cmd == RPMB_IOCTL_SOTER_WRITE_DATA) || (cmd == RPMB_IOCTL_SOTER_READ_DATA)) {
		if (rpmb_buffer == NULL) {
			MSG(ERR, "%s, rpmb_buffer is NULL!\n", __func__);
			return -1;
		}
		err = copy_from_user(rpmb_buffer, (void *)arg, 4);
		if (err < 0) {
			MSG(ERR, "%s, err=%x\n", __func__, err);
			return -1;
		}
		rpmbinfor.size =  *(unsigned char *)rpmb_buffer | (*((unsigned char *)rpmb_buffer + 1) << 8);
		rpmbinfor.size |= (*((unsigned char *)rpmb_buffer+2) << 16) | (*((unsigned char *)rpmb_buffer+3) << 24);
		MSG(INFO, "%s, rpmbinfor.size is %d!\n", __func__, rpmbinfor.size);
		err = copy_from_user(rpmb_buffer, (void *)arg, 4 + rpmbinfor.size);
		if (err < 0) {
			MSG(ERR, "%s, err=%x\n", __func__, err);
			return -1;
		}
		rpmbinfor.data_frame = (rpmb_buffer + 4);
	}
#endif

	switch (cmd) {

	case RPMB_IOCTL_PROGRAM_KEY:

		MSG(INFO, "%s, cmd = RPMB_IOCTL_PROGRAM_KEY!!!!!!!!!!!!!!\n", __func__);

		ret = emmc_rpmb_req_set_key(card, param.key);

		break;

	case RPMB_IOCTL_READ_DATA:

		MSG(INFO, "%s, cmd = RPMB_IOCTL_READ_DATA!!!!!!!!!!!!!!\n", __func__);

		ret = emmc_rpmb_req_read_data(card, &param);

		err = copy_to_user((void *)arg, &param, sizeof(param));
		if (err < 0) {
			MSG(ERR, "%s, err=%x\n", __func__, err);
			return -1;
		}

		break;

	case RPMB_IOCTL_WRITE_DATA:

		MSG(INFO, "%s, cmd = RPMB_IOCTL_WRITE_DATA!!!!!!!!!!!!!!\n", __func__);

		ret = emmc_rpmb_req_write_data(card, &param);

		break;

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	case RPMB_IOCTL_SOTER_WRITE_DATA:

		ret = ut_rpmb_req_write_data(card, (struct s_rpmb *)(rpmbinfor.data_frame), rpmbinfor.size/1024);

		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
			goto end;
		}

		ret = copy_to_user((void *)arg, rpmb_buffer, 4 + rpmbinfor.size);

	    break;

	case RPMB_IOCTL_SOTER_READ_DATA:

		ret = ut_rpmb_req_read_data(card, (struct s_rpmb *)(rpmbinfor.data_frame), rpmbinfor.size/1024);

		if (ret) {
			MSG(ERR, "%s, emmc_rpmb_req_handle IO error!!!(%x)\n", __func__, ret);
			goto end;
		}

		ret = copy_to_user((void *)arg, rpmb_buffer, 4 + rpmbinfor.size);

	    break;

	case RPMB_IOCTL_SOTER_GET_CNT:

		ret = ut_rpmb_req_get_wc(card, (unsigned int *)arg);
			break;

	case RPMB_IOCTL_SOTER_GET_WR_SIZE:

			ret = ut_rpmb_req_get_max_wr_size(card, (unsigned int *)arg);

			break;

#endif
	default:
		MSG(ERR, "%s, wrong ioctl code (%d)!!!\n", __func__, cmd);
		return -ENOTTY;
	}
#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
end:
#endif
	return ret;
}

static int emmc_rpmb_close(struct inode *inode, struct file *file)
{
	int ret = 0;

	MSG(INFO, "%s, !!!!!!!!!!!!\n", __func__);

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))
	kfree(rpmb_buffer);
	rpmb_buffer = NULL;
	MSG(INFO, "%s, rpmb free memory done!!!\n", __func__);
#endif
	return ret;
}

static const struct file_operations emmc_rpmb_fops = {
	.owner = THIS_MODULE,
	.open = emmc_rpmb_open,
	.release = emmc_rpmb_close,
	.unlocked_ioctl = emmc_rpmb_ioctl,
	.write = NULL,
	.read = NULL,
};

static int __init emmc_rpmb_init(void)
{
	int alloc_ret = -1;
	int cdev_ret = -1;
	int major;
	dev_t dev;
	struct device *device = NULL;

	MSG(INFO, "%s start\n", __func__);

	alloc_ret = alloc_chrdev_region(&dev, 0, 1, RPMB_NAME);

	if (alloc_ret)
		goto error;

	major = MAJOR(dev);

	cdev_init(&rpmb_dev, &emmc_rpmb_fops);
	rpmb_dev.owner = THIS_MODULE;

	cdev_ret = cdev_add(&rpmb_dev, MKDEV(major, 0), 1);
	if (cdev_ret)
		goto error;

	rpmb_class = class_create(THIS_MODULE, RPMB_NAME);

	if (IS_ERR(rpmb_class))
		goto error;

	device = device_create(rpmb_class, NULL, MKDEV(major, 0), NULL,
		RPMB_NAME "%d", 0);

	if (IS_ERR(device))
		goto error;

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
	open_th = kthread_run(emmc_rpmb_thread, NULL, "rpmb_open");
	if (IS_ERR(open_th))
		MSG(ERR, "%s, init kthread_run failed!\n", __func__);
#endif

	MSG(INFO, "emmc_rpmb_init end!!!!\n");

	return 0;

error:

	if (rpmb_class)
		class_destroy(rpmb_class);

	if (cdev_ret == 0)
		cdev_del(&rpmb_dev);

	if (alloc_ret == 0)
		unregister_chrdev_region(dev, 1);

	return -1;
}

late_initcall(emmc_rpmb_init);

