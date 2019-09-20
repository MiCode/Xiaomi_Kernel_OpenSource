/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include "qcn_sdio.h"

static bool tx_dump;
module_param(tx_dump, bool, S_IRUGO | S_IWUSR | S_IWGRP);

static bool rx_dump;
module_param(rx_dump, bool, S_IRUGO | S_IWUSR | S_IWGRP);

static int dump_len = 32;
module_param(dump_len, int, S_IRUGO | S_IWUSR | S_IWGRP);

static bool retune;
module_param(retune, bool, S_IRUGO | S_IWUSR | S_IWGRP);

/* driver_state :
 *	QCN_SDIO_SW_RESET = 0,
 *	QCN_SDIO_SW_PBL,
 *	QCN_SDIO_SW_SBL,
 *	QCN_SDIO_SW_RDDM,
 *	QCN_SDIO_SW_MROM,
*/
static int driver_state;
module_param(driver_state, int, S_IRUGO | S_IRUSR | S_IRGRP);

static struct mmc_host *current_host;

#define HEX_DUMP(mode, buf, len)				\
	print_hex_dump(KERN_ERR, mode, 2, 32, 4, buf,		\
			dump_len > len ? len : dump_len, 0)

struct qcn_sdio {
	enum qcn_sdio_sw_mode curr_sw_mode;
	struct sdio_func *func;
	const struct sdio_device_id *id;
	struct qcn_sdio_ch_info *ch[QCN_SDIO_CH_MAX];
	atomic_t ch_status[QCN_SDIO_CH_MAX];
	spinlock_t lock_free_q;
	spinlock_t lock_wait_q;
	u32 rx_addr_base;
	u32 tx_addr_base;
	u8 rx_cnum_base;
	u8 tx_cnum_base;
	struct qcn_sdio_rw_info rw_req_info[QCN_SDIO_RW_REQ_MAX];
	struct list_head rw_free_q;
	struct list_head rw_wait_q;
	atomic_t free_list_count;
	atomic_t wait_list_count;
	struct workqueue_struct *qcn_sdio_wq;
	struct work_struct sdio_rw_w;
};

static struct qcn_sdio *sdio_ctxt;
struct completion client_probe_complete;
static struct mutex lock;
static struct list_head cinfo_head;
static atomic_t status;
static atomic_t xport_status;
static spinlock_t async_lock;
static struct task_struct *reset_task;

static int qcn_create_sysfs(struct device *dev);

#if (QCN_SDIO_META_VER_0)
#define	META_INFO(event, data)						  \
	((u32)((u32)data << QCN_SDIO_HMETA_DATA_SHFT) |			  \
	(u32)(((u32)event << QCN_SDIO_HMETA_EVENT_SHFT) &		  \
	QCN_SDIO_HMETA_EVENT_BMSK) | (u32)(((u32)(sdio_ctxt->curr_sw_mode)\
	<< QCN_SDIO_HMETA_SW_SHFT) & QCN_SDIO_HMETA_SW_BMSK) |		  \
	(u32)(QCN_SDIO_HMETA_FMT_VER & QCN_SDIO_HMETA_VER_BMSK))
#elif (QCN_SDIO_META_VER_1)
#define	META_INFO(even, data)						  \
	((u32)(((u32)event << QCN_SDIO_HMETA_EVENT_SHFT) &		  \
	QCN_SDIO_HMETA_EVENT_BMSK) | (u32)(((u32)data <<		  \
	QCN_SDIO_HMETA_DATA_SHFT) & QCN_SDIO_HMETA_DATA_BMSK))
#endif

#define	SDIO_RW_OFFSET		31
#define	SDIO_RW_MASK		1
#define	SDIO_FUNCTION_OFFSET	28
#define	SDIO_FUNCTION_MASK	7
#define	SDIO_MODE_OFFSET	27
#define	SDIO_MODE_MASK		1
#define	SDIO_OPCODE_OFFSET	26
#define	SDIO_OPCODE_MASK	1
#define	SDIO_ADDRESS_OFFSET	9
#define	SDIO_ADDRESS_MASK	0x1FFFF
#define	SDIO_RAW_OFFSET		27
#define	SDIO_RAW_MASK		1
#define	SDIO_STUFF_OFFSET1	26
#define	SDIO_STUFF_OFFSET2	8
#define	SDIO_STUFF_MASK		1
#define	SDIO_BLOCKSZ_MASK	0x1FF
#define	SDIO_DATA_MASK		0xFF

static inline
void qcn_sdio_set_cmd53_arg(u32 *arg, u8 rw, u8 func, u8 mode, u8 opcode,
							u32 addr, u16 blksz)
{
	*arg = (((rw & SDIO_RW_MASK) << SDIO_RW_OFFSET) |
		((func & SDIO_FUNCTION_MASK) << SDIO_FUNCTION_OFFSET) |
		((mode & SDIO_MODE_MASK) << SDIO_MODE_OFFSET) |
		((opcode & SDIO_OPCODE_MASK) << SDIO_OPCODE_OFFSET) |
		((addr & SDIO_ADDRESS_MASK) << SDIO_ADDRESS_OFFSET) |
		(blksz & SDIO_BLOCKSZ_MASK));
}

static inline
void qcn_sdio_set_cmd52_arg(u32 *arg, u8 rw, u8 func, u8 raw, u32 addr, u8 val)
{
	*arg = ((rw & SDIO_RW_MASK) << SDIO_RW_OFFSET) |
		((func & SDIO_FUNCTION_MASK) << SDIO_FUNCTION_OFFSET) |
		((raw & SDIO_RAW_MASK) << SDIO_RAW_OFFSET) |
		(SDIO_STUFF_MASK << SDIO_STUFF_OFFSET1) |
		((addr & SDIO_ADDRESS_MASK) << SDIO_ADDRESS_OFFSET) |
		(SDIO_STUFF_MASK << SDIO_STUFF_OFFSET2) |
		(val & SDIO_DATA_MASK);
}

static void qcn_sdio_free_rw_req(struct qcn_sdio_rw_info *rw_req)
{
	spin_lock(&sdio_ctxt->lock_free_q);
	list_add_tail(&rw_req->list, &sdio_ctxt->rw_free_q);
	atomic_inc(&sdio_ctxt->free_list_count);
	spin_unlock(&sdio_ctxt->lock_free_q);
}

static void qcn_sdio_purge_rw_buff(void)
{
	struct qcn_sdio_rw_info *rw_req = NULL;

	while (!list_empty(&sdio_ctxt->rw_wait_q)) {
		rw_req = list_first_entry(&sdio_ctxt->rw_wait_q,
						struct qcn_sdio_rw_info, list);
		list_del(&rw_req->list);
		qcn_sdio_free_rw_req(rw_req);
	}
}

void qcn_sdio_client_probe_complete(int id)
{
	complete(&client_probe_complete);
}
EXPORT_SYMBOL(qcn_sdio_client_probe_complete);

static struct qcn_sdio_rw_info *qcn_sdio_alloc_rw_req(void)
{
	struct qcn_sdio_rw_info *rw_req = NULL;

	spin_lock(&sdio_ctxt->lock_free_q);
	if (list_empty(&sdio_ctxt->rw_free_q)) {
		spin_unlock(&sdio_ctxt->lock_free_q);
		return rw_req;
	}

	rw_req = list_first_entry(&sdio_ctxt->rw_free_q,
						struct qcn_sdio_rw_info, list);
	list_del(&rw_req->list);
	atomic_dec(&sdio_ctxt->free_list_count);
	spin_unlock(&sdio_ctxt->lock_free_q);

	return rw_req;
}

static void qcn_sdio_add_rw_req(struct qcn_sdio_rw_info *rw_req)
{
	spin_lock_bh(&sdio_ctxt->lock_wait_q);
	list_add_tail(&rw_req->list, &sdio_ctxt->rw_wait_q);
	atomic_inc(&sdio_ctxt->wait_list_count);
	spin_unlock_bh(&sdio_ctxt->lock_wait_q);
}

static int qcn_enable_async_irq(bool enable)
{
	unsigned int num = 0;
	int ret = 0;
	u32 data = 0;

	num = sdio_ctxt->func->num;
	sdio_claim_host(sdio_ctxt->func);
	sdio_ctxt->func->num = 0;
	data = sdio_readb(sdio_ctxt->func, SDIO_CCCR_INTERRUPT_EXTENSION, NULL);
	if (enable)
		data |= SDIO_ENABLE_ASYNC_INTR;
	else
		data &= ~SDIO_ENABLE_ASYNC_INTR;
	sdio_writeb(sdio_ctxt->func, data, SDIO_CCCR_INTERRUPT_EXTENSION, &ret);
	sdio_ctxt->func->num = num;
	sdio_release_host(sdio_ctxt->func);

	return ret;
}

static int qcn_send_io_abort(void)
{
	unsigned int num = 0;
	int ret = 0;

	num = sdio_ctxt->func->num;
	sdio_claim_host(sdio_ctxt->func);
	sdio_ctxt->func->num = 0;
	sdio_writeb(sdio_ctxt->func, 0x1, SDIO_CCCR_ABORT, &ret);
	sdio_ctxt->func->num = num;
	sdio_release_host(sdio_ctxt->func);

	return ret;
}

static int qcn_send_meta_info(u8 event, u32 data)
{
	int ret = 0;
	u32 i = 0;
	u32 value = 0;
	u8 temp = 0;

	value =	META_INFO(event, data);

	sdio_claim_host(sdio_ctxt->func);
	if (sdio_ctxt->curr_sw_mode < QCN_SDIO_SW_SBL) {
		for (i = 0; i < 4; i++) {
			temp = (u8)((value >> (i * 8)) & 0x000000FF);
			sdio_writeb(sdio_ctxt->func, temp,
						(SDIO_QCN_HRQ_PUSH + i), &ret);
		}
	} else {
		sdio_writel(sdio_ctxt->func, value, SDIO_QCN_HRQ_PUSH, &ret);
	}

	sdio_release_host(sdio_ctxt->func);

	return ret;
}

static int qcn_read_crq_info(void)
{
	int ret = 0;
	u32 i = 0;
	u32 temp = 0;
	u32 data = 0;
	u32 len = 0;
	u8 cid = 0;

	struct sdio_al_channel_handle *ch_handle = NULL;

	sdio_claim_host(sdio_ctxt->func);
	if (sdio_ctxt->curr_sw_mode < QCN_SDIO_SW_SBL) {
		for (i = 0; i < 4; i++) {
			temp = sdio_readb(sdio_ctxt->func,
						(SDIO_QCN_CRQ_PULL + i), &ret);
			temp = temp << (i * 8);
			data |= temp;
		}
	} else {
		data = sdio_readl(sdio_ctxt->func, SDIO_QCN_CRQ_PULL, &ret);
	}

	sdio_release_host(sdio_ctxt->func);
	if (ret)
		return ret;

	if (data & SDIO_QCN_CRQ_PULL_TRANS_MASK) {
		cid = (u8)(data & SDIO_QCN_CRQ_PULL_CH_NUM_MASK);
		cid -= sdio_ctxt->rx_cnum_base;
		len = (data & SDIO_QCN_CRQ_PULL_BLK_CNT_MASK) >>
			SDIO_QCN_CRQ_PULL_BLK_CNT_SHIFT;

		if (data & SDIO_QCN_CRQ_PULL_BLK_MASK)
			len *= sdio_ctxt->func->cur_blksize;
		temp = (data & SDIO_QCN_CRQ_PULL_UD_MASK) >>
						SDIO_QCN_CRQ_PULL_UD_SHIFT;

		if (!sdio_ctxt->ch[cid]) {
			pr_err("Client Id not initialized\n");
			return -EINVAL;
		}
		switch (temp) {
		case QCN_SDIO_CRQ_START:
			sdio_ctxt->ch[cid]->crq_len = len;
			return ret;
		case QCN_SDIO_CRQ_END:
			sdio_ctxt->ch[cid]->crq_len += len;
			break;
		default:
			sdio_ctxt->ch[cid]->crq_len = len;
		}

		ch_handle = &(sdio_ctxt->ch[cid]->ch_handle);
		if (sdio_ctxt->ch[cid]->ch_data.dl_data_avail_cb)
			sdio_ctxt->ch[cid]->ch_data.dl_data_avail_cb(ch_handle,
					sdio_ctxt->ch[cid]->crq_len);
	}

	return ret;
}

static int qcn_sdio_config(struct qcn_sdio_client_info *cinfo)
{
	int ret = 0;
	u32 data = 0;

	sdio_claim_host(sdio_ctxt->func);
	ret = sdio_set_block_size(sdio_ctxt->func,
				  cinfo->cli_handle.block_size);

	if (ret) {
		sdio_release_host(sdio_ctxt->func);
		goto err;
	}

	data = SDIO_QCN_CONFIG_QE_MASK;

	sdio_writeb(sdio_ctxt->func, (u8)data, SDIO_QCN_CONFIG, &ret);
	if (ret) {
		sdio_release_host(sdio_ctxt->func);
		goto err;
	}

	data = (SDIO_QCN_IRQ_EN_LOCAL_MASK |
			SDIO_QCN_IRQ_EN_SYS_ERR_MASK |
			SDIO_QCN_IRQ_UNDERFLOW_MASK |
			SDIO_QCN_IRQ_OVERFLOW_MASK |
			SDIO_QCN_IRQ_CH_MISMATCH_MASK |
			SDIO_QCN_IRQ_CRQ_READY_MASK);

	sdio_writeb(sdio_ctxt->func, (u8)data, SDIO_QCN_IRQ_EN, &ret);
	sdio_release_host(sdio_ctxt->func);
	if (ret) {
		pr_err("%s: failed write config\n", __func__);
		goto err;
	}

	sdio_ctxt->rx_addr_base = SDIO_QCN_MC_DMA0_RX_CH0;
	sdio_ctxt->rx_cnum_base	= QCN_SDIO_DMA0_RX_CNUM;
	sdio_ctxt->tx_addr_base = SDIO_QCN_MC_DMA1_TX_CH0;
	sdio_ctxt->tx_cnum_base = QCN_SDIO_DMA1_TX_CNUM;

#if (QCN_SDIO_META_VER_0)
	data = ((cinfo->cli_handle.block_size / 8) - 1);
#elif (QCN_SDIO_META_VER_1)
	data = cinfo->cli_handle.block_size;
#endif
	ret = qcn_send_meta_info(QCN_SDIO_BLK_SZ_HEVENT, data);
err:
	return ret;
}


int qcn_sw_mode_change(enum qcn_sdio_sw_mode mode)
{
	struct qcn_sdio_client_info *cinfo = NULL;
	struct qcn_sdio_ch_info *chinfo = NULL;
	int ret = 0;

	if (!(mode) && !(mode < QCN_SDIO_SW_MAX))
		return -EINVAL;

	if (sdio_ctxt->curr_sw_mode == mode)
		return 0;

	if ((sdio_ctxt->curr_sw_mode == QCN_SDIO_SW_PBL) &&
						(mode == QCN_SDIO_SW_SBL)) {
		sdio_ctxt->curr_sw_mode = QCN_SDIO_SW_SBL;
		qcn_send_meta_info(QCN_SDIO_BLK_SZ_HEVENT,
						sdio_ctxt->func->cur_blksize);
		qcn_send_meta_info(QCN_SDIO_DOORBELL_HEVENT, (u32)0);
		return 0;
	}

	switch (sdio_ctxt->curr_sw_mode) {
	case QCN_SDIO_SW_PBL:
	case QCN_SDIO_SW_SBL:
	case QCN_SDIO_SW_RDDM:
		mutex_lock(&lock);
		list_for_each_entry(cinfo, &cinfo_head, cli_list) {
			while (!list_empty(&cinfo->ch_head)) {
				chinfo = list_first_entry(&cinfo->ch_head,
					      struct qcn_sdio_ch_info, ch_list);
				sdio_al_deregister_channel(&chinfo->ch_handle);
			}
			cinfo->cli_handle.func = NULL;

			if (cinfo->is_probed) {
				cinfo->cli_data.remove(&cinfo->cli_handle);
				cinfo->is_probed = 0;
			}
			if (((cinfo->cli_handle.id == QCN_SDIO_CLI_ID_WLAN) ||
			     (cinfo->cli_handle.id == QCN_SDIO_CLI_ID_QMI) ||
			     (cinfo->cli_handle.id == QCN_SDIO_CLI_ID_DIAG)) &&
			     (mode == QCN_SDIO_SW_MROM)) {
				qcn_send_meta_info((u8)QCN_SDIO_SW_MODE_HEVENT,
						(u32)(mode | QCN_SDIO_MAJOR_VER
						| QCN_SDIO_MINOR_VER));
				cinfo->cli_handle.block_size =
							QCN_SDIO_MROM_BLK_SZ;
				cinfo->cli_handle.func = sdio_ctxt->func;
				qcn_sdio_config(cinfo);
				cinfo->is_probed = !cinfo->cli_data.probe(
							&cinfo->cli_handle);
				qcn_send_meta_info(QCN_SDIO_DOORBELL_HEVENT,
									(u32)0);
			}
		}
		mutex_unlock(&lock);
		break;
	case QCN_SDIO_SW_RESET:
	case QCN_SDIO_SW_MROM:
		ret = wait_for_completion_timeout(&client_probe_complete,
							msecs_to_jiffies(3000));
		if (!ret)
			pr_err("Timeout waiting for clients\n");

		mutex_lock(&lock);
		list_for_each_entry(cinfo, &cinfo_head, cli_list) {
			while (!list_empty(&cinfo->ch_head)) {
				chinfo = list_first_entry(&cinfo->ch_head,
					      struct qcn_sdio_ch_info, ch_list);
				sdio_al_deregister_channel(&chinfo->ch_handle);
			}
			cinfo->cli_handle.func = NULL;


			if (cinfo->is_probed) {
				cinfo->cli_data.remove(&cinfo->cli_handle);
				cinfo->is_probed = 0;
			}

			if ((cinfo->cli_handle.id == QCN_SDIO_CLI_ID_TTY) &&
						   (mode <= QCN_SDIO_SW_MROM)) {
				qcn_send_meta_info((u8)QCN_SDIO_SW_MODE_HEVENT,
						(u32)(mode | QCN_SDIO_MAJOR_VER
						| QCN_SDIO_MINOR_VER));
				cinfo->cli_handle.block_size =
							QCN_SDIO_TTY_BLK_SZ;
				cinfo->cli_handle.func = sdio_ctxt->func;
				qcn_sdio_config(cinfo);
				cinfo->is_probed = !cinfo->cli_data.probe(
							&cinfo->cli_handle);
				qcn_send_meta_info(QCN_SDIO_DOORBELL_HEVENT,
									(u32)0);
			}
		}
		mutex_unlock(&lock);
		break;
	default:
		pr_err("Invalid mode\n");
	}

	driver_state = mode;
	sdio_ctxt->curr_sw_mode = mode;
	return 0;
}

static int qcn_read_meta_info(void)
{
	int ret = 0;
	u32 i = 0;
	u32 data = 0;
	u32 temp = 0;

	sdio_claim_host(sdio_ctxt->func);

	if (sdio_ctxt->curr_sw_mode < QCN_SDIO_SW_SBL) {
		for (i = 0; i < 4; i++) {
			temp = sdio_readb(sdio_ctxt->func,
					(SDIO_QCN_LOCAL_INFO + i), &ret);
			temp = temp << (i * 8);
			data |= temp;
		}
	} else {
		data = sdio_readl(sdio_ctxt->func, SDIO_QCN_LOCAL_INFO,	&ret);
	}

	sdio_writeb(sdio_ctxt->func, (u8)SDIO_QCN_IRQ_CLR_LOCAL_MASK,
		    SDIO_QCN_IRQ_CLR, NULL);

	sdio_release_host(sdio_ctxt->func);

	if (ret)
		return ret;

	temp = (data & QCN_SDIO_LMETA_EVENT_BMSK) >> QCN_SDIO_LMETA_EVENT_SHFT;
	switch (temp) {
	case QCN_SDIO_SW_MODE_LEVENT:
		temp = (data & QCN_SDIO_LMETA_SW_BMSK) >>
						QCN_SDIO_LMETA_SW_SHFT;
		qcn_sw_mode_change((enum qcn_sdio_sw_mode)temp);
		break;
	default:
		if ((temp >= QCN_SDIO_META_START_CH0) &&
				(temp < QCN_SDIO_META_START_CH1)) {
			if (sdio_ctxt->ch[0] &&
				sdio_ctxt->ch[0]->ch_data.dl_meta_data_cb)
				sdio_ctxt->ch[0]->ch_data.dl_meta_data_cb(
					&(sdio_ctxt->ch[0]->ch_handle), data);
		} else if ((temp >= QCN_SDIO_META_START_CH1) &&
			(temp < QCN_SDIO_META_START_CH2)) {
			if (sdio_ctxt->ch[1] &&
				sdio_ctxt->ch[1]->ch_data.dl_meta_data_cb)
				sdio_ctxt->ch[1]->ch_data.dl_meta_data_cb(
					&(sdio_ctxt->ch[1]->ch_handle), data);
		} else if ((temp >= QCN_SDIO_META_START_CH2) &&
				(temp < QCN_SDIO_META_START_CH3)) {
			if (sdio_ctxt->ch[2] &&
				sdio_ctxt->ch[2]->ch_data.dl_meta_data_cb)
				sdio_ctxt->ch[2]->ch_data.dl_meta_data_cb(
					&(sdio_ctxt->ch[2]->ch_handle), data);
		} else if ((temp >= QCN_SDIO_META_START_CH3) &&
					(temp < QCN_SDIO_META_END)) {
			if (sdio_ctxt->ch[3] &&
				sdio_ctxt->ch[3]->ch_data.dl_meta_data_cb)
				sdio_ctxt->ch[3]->ch_data.dl_meta_data_cb(
					&(sdio_ctxt->ch[3]->ch_handle), data);
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}

static int reset_thread(void *data)
{
	qcn_sdio_purge_rw_buff();
	qcn_sdio_card_state(false);
	qcn_sdio_card_state(true);
	kthread_stop(reset_task);
	reset_task = NULL;

	return 0;
}

static void qcn_sdio_irq_handler(struct sdio_func *func)
{
	u8 data = 0;
	int ret = 0;

	sdio_claim_host(sdio_ctxt->func);
	data = sdio_readb(sdio_ctxt->func, SDIO_QCN_IRQ_STATUS, &ret);
	if (ret == -ETIMEDOUT) {
		sdio_release_host(sdio_ctxt->func);

		pr_err("%s: IRQ status read error ret = %d\n", __func__, ret);

		reset_task = kthread_run(reset_thread, NULL, "qcn_reset");
		if (IS_ERR(reset_task))
			pr_err("Failed to run qcn_reset thread\n");

		return;
	}
	sdio_release_host(sdio_ctxt->func);

	if (data & SDIO_QCN_IRQ_CRQ_READY_MASK) {
		qcn_read_crq_info();
	} else if (data & SDIO_QCN_IRQ_LOCAL_MASK) {
		qcn_read_meta_info();
	} else if (data & SDIO_QCN_IRQ_EN_SYS_ERR_MASK) {
		sdio_claim_host(sdio_ctxt->func);
		sdio_writeb(sdio_ctxt->func, (u8)SDIO_QCN_IRQ_CLR_SYS_ERR_MASK,
				SDIO_QCN_IRQ_CLR, NULL);
		sdio_release_host(sdio_ctxt->func);
		pr_err("%s: sys_err interrupt triggered\n", __func__);
	} else if (data & SDIO_QCN_IRQ_EN_UNDERFLOW_MASK) {
		sdio_claim_host(sdio_ctxt->func);
		sdio_writeb(sdio_ctxt->func,
					(u8)SDIO_QCN_IRQ_CLR_UNDERFLOW_MASK,
					SDIO_QCN_IRQ_CLR, NULL);
		sdio_release_host(sdio_ctxt->func);
		pr_err("%s: underflow interrupt triggered\n", __func__);
	} else if (data & SDIO_QCN_IRQ_EN_OVERFLOW_MASK) {
		sdio_claim_host(sdio_ctxt->func);
		sdio_writeb(sdio_ctxt->func, (u8)SDIO_QCN_IRQ_CLR_OVERFLOW_MASK,
				SDIO_QCN_IRQ_CLR, NULL);
		sdio_release_host(sdio_ctxt->func);
		pr_err("%s: overflow interrupt triggered\n", __func__);
	} else if (data & SDIO_QCN_IRQ_EN_CH_MISMATCH_MASK) {
		sdio_claim_host(sdio_ctxt->func);
		sdio_writeb(sdio_ctxt->func,
					(u8)SDIO_QCN_IRQ_CLR_CH_MISMATCH_MASK,
					SDIO_QCN_IRQ_CLR, NULL);
		sdio_release_host(sdio_ctxt->func);
		pr_err("%s: channel mismatch interrupt triggered\n", __func__);
	} else {
		sdio_claim_host(sdio_ctxt->func);
		sdio_writeb(sdio_ctxt->func, (u8)data, SDIO_QCN_IRQ_CLR, NULL);
		sdio_release_host(sdio_ctxt->func);
	}
}

static int qcn_sdio_send_buff(u32 cid, void *buff, size_t len)
{
	int ret = 0;

	sdio_claim_host(sdio_ctxt->func);
	ret = sdio_writesb(sdio_ctxt->func,
			(sdio_ctxt->tx_addr_base + (cid * (u32)4)), buff, len);

	if (ret)
		qcn_send_io_abort();

	sdio_release_host(sdio_ctxt->func);

	return ret;
}

static int qcn_sdio_recv_buff(u32 cid, void *buff, size_t len)
{
	int ret = 0;

	sdio_claim_host(sdio_ctxt->func);
	ret = sdio_readsb(sdio_ctxt->func, buff,
			(sdio_ctxt->rx_addr_base + (cid * (u32)4)), len);

	if (ret)
		qcn_send_io_abort();

	sdio_release_host(sdio_ctxt->func);

	return ret;
}

static void qcn_sdio_rw_work(struct work_struct *work)
{
	int ret = 0;
	struct qcn_sdio_rw_info *rw_req = NULL;
	struct sdio_al_xfer_result *result = NULL;
	struct sdio_al_channel_handle *ch_handle = NULL;

	while (1) {
		spin_lock_bh(&sdio_ctxt->lock_wait_q);
		if (list_empty(&sdio_ctxt->rw_wait_q)) {
			spin_unlock_bh(&sdio_ctxt->lock_wait_q);
			break;
		}
		rw_req = list_first_entry(&sdio_ctxt->rw_wait_q,
						struct qcn_sdio_rw_info, list);
		list_del(&rw_req->list);
		spin_unlock_bh(&sdio_ctxt->lock_wait_q);

		if (rw_req->dir) {
			ret = qcn_sdio_recv_buff(rw_req->cid, rw_req->buf,
								rw_req->len);
			if (rx_dump)
				HEX_DUMP("ASYNC_RECV: ", rw_req->buf,
								rw_req->len);
		} else {
			ret = qcn_sdio_send_buff(rw_req->cid, rw_req->buf,
								rw_req->len);
			if (tx_dump)
				HEX_DUMP("ASYNC_SEND: ", rw_req->buf,
								rw_req->len);
		}

		ch_handle = &sdio_ctxt->ch[rw_req->cid]->ch_handle;
		result = &sdio_ctxt->ch[rw_req->cid]->result;
		result->xfer_status = ret;
		result->buf_addr = rw_req->buf;
		result->xfer_len = rw_req->len;
		if (rw_req->dir)
			sdio_ctxt->ch[rw_req->cid]->ch_data.dl_xfer_cb(
					ch_handle, result, rw_req->ctxt);
		else
			sdio_ctxt->ch[rw_req->cid]->ch_data.ul_xfer_cb(
					ch_handle, result, rw_req->ctxt);
		atomic_set(&sdio_ctxt->ch_status[rw_req->cid], 0);
		qcn_sdio_free_rw_req(rw_req);
		atomic_dec(&sdio_ctxt->wait_list_count);
	}
}

static
int qcn_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret = 0;

	sdio_ctxt = kzalloc(sizeof(struct qcn_sdio), GFP_KERNEL);
	if (!sdio_ctxt)
		return -ENOMEM;

	sdio_ctxt->func = func;
	sdio_ctxt->id = id;
	sdio_set_drvdata(func, sdio_ctxt);
	sdio_ctxt->qcn_sdio_wq = create_singlethread_workqueue("qcn_sdio");
	if (!sdio_ctxt->qcn_sdio_wq) {
		pr_err("%s: Error: SDIO create wq\n", __func__);
		goto err;
	}

	for (ret = 0; ret < QCN_SDIO_CH_MAX; ret++) {
		sdio_ctxt->ch[ret] = NULL;
		atomic_set(&sdio_ctxt->ch_status[ret], -1);
	}

	spin_lock_init(&sdio_ctxt->lock_free_q);
	spin_lock_init(&sdio_ctxt->lock_wait_q);
	spin_lock_init(&async_lock);
	INIT_WORK(&sdio_ctxt->sdio_rw_w, qcn_sdio_rw_work);
	INIT_LIST_HEAD(&sdio_ctxt->rw_free_q);
	INIT_LIST_HEAD(&sdio_ctxt->rw_wait_q);

	for (ret = 0; ret < QCN_SDIO_RW_REQ_MAX; ret++)
		qcn_sdio_free_rw_req(&sdio_ctxt->rw_req_info[ret]);

	sdio_claim_host(sdio_ctxt->func);
	ret = sdio_enable_func(sdio_ctxt->func);
	if (ret) {
		pr_err("%s: Error:%d SDIO enable func\n", __func__, ret);
		sdio_release_host(sdio_ctxt->func);
		goto err;
	}
	ret = sdio_claim_irq(sdio_ctxt->func, qcn_sdio_irq_handler);
	if (ret) {
		pr_err("%s: Error:%d SDIO claim irq\n", __func__, ret);
		sdio_release_host(sdio_ctxt->func);
		goto err;
	}

	qcn_enable_async_irq(true);
	sdio_release_host(sdio_ctxt->func);

	if (qcn_read_meta_info()) {
		pr_err("%s: Error: SDIO Config\n", __func__);
		qcn_send_meta_info((u8)QCN_SDIO_SW_MODE_HEVENT, (u32)0);
	}

	current_host = func->card->host;

	if (!retune) {
		pr_debug("%s Probing driver with retune disabled\n", __func__);
		mmc_retune_disable(current_host);
	}

	atomic_set(&xport_status, 1);

	return 0;
err:
	kfree(sdio_ctxt);
	sdio_ctxt = NULL;
	return ret;
}

static void qcn_sdio_remove(struct sdio_func *func)
{
	struct qcn_sdio_client_info *cinfo = NULL;
	struct qcn_sdio_ch_info *ch_info = NULL;

	atomic_set(&xport_status, 0);
	sdio_claim_host(sdio_ctxt->func);
	qcn_enable_async_irq(false);
	sdio_release_irq(sdio_ctxt->func);
	sdio_release_host(sdio_ctxt->func);

	qcn_sdio_purge_rw_buff();

	destroy_workqueue(sdio_ctxt->qcn_sdio_wq);
	mutex_lock(&lock);
	list_for_each_entry(cinfo, &cinfo_head, cli_list) {
		while (!list_empty(&cinfo->ch_head)) {
			ch_info = list_first_entry(&cinfo->ch_head,
					struct qcn_sdio_ch_info, ch_list);
			sdio_al_deregister_channel(&ch_info->ch_handle);
		}
		mutex_unlock(&lock);
		if (cinfo->is_probed) {
			cinfo->cli_data.remove(&cinfo->cli_handle);
			cinfo->is_probed = 0;
		}
		mutex_lock(&lock);
	}
	mutex_unlock(&lock);

	kfree(sdio_ctxt);
	sdio_ctxt = NULL;
	mmc_retune_enable(current_host);
}

static const struct sdio_device_id qcn_sdio_devices[] = {
	{SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCN_BASE | 0x0))},
	{},
};

MODULE_DEVICE_TABLE(sdio, qcn_sdio_devices);

static struct sdio_driver qcn_sdio_driver = {
	.name = "qcn_sdio",
	.id_table = qcn_sdio_devices,
	.probe = qcn_sdio_probe,
	.remove = qcn_sdio_remove,
};

static int qcn_sdio_plat_probe(struct platform_device *pdev)
{
	int ret = 0;

	mutex_init(&lock);
	INIT_LIST_HEAD(&cinfo_head);
	atomic_set(&status, 1);

	ret = sdio_register_driver(&qcn_sdio_driver);
	if (ret) {
		pr_err("%s: SDIO driver registration failed: %d\n", __func__,
									ret);
		mutex_destroy(&lock);
		atomic_set(&status, 0);
	}

	init_completion(&client_probe_complete);

	qcn_create_sysfs(&pdev->dev);

	return ret;
}

static int qcn_sdio_plat_remove(struct platform_device *pdev)
{
	struct qcn_sdio_client_info *cinfo = NULL;

	mutex_lock(&lock);
	while (!list_empty(&cinfo_head)) {
		cinfo = list_first_entry(&cinfo_head, struct
						qcn_sdio_client_info, cli_list);
		mutex_unlock(&lock);
		sdio_al_deregister_client(&cinfo->cli_handle);
		mutex_lock(&lock);
		list_del(&cinfo->cli_list);
	}
	mutex_unlock(&lock);
	mutex_destroy(&lock);
	if (sdio_ctxt) {
		destroy_workqueue(sdio_ctxt->qcn_sdio_wq);
		sdio_release_irq(sdio_ctxt->func);
		kfree(sdio_ctxt);
		sdio_ctxt = NULL;
	}
	sdio_unregister_driver(&qcn_sdio_driver);
	atomic_set(&status, 0);

	return 0;
}

static const struct of_device_id qcn_sdio_dt_match[] = {
	{.compatible = "qcom,qcn-sdio"},
	{}
};
MODULE_DEVICE_TABLE(of, qcn_sdio_dt_match);

static struct platform_driver qcn_sdio_plat_driver = {
	.probe  = qcn_sdio_plat_probe,
	.remove = qcn_sdio_plat_remove,
	.driver = {
		.name = "qcn-sdio",
		.owner = THIS_MODULE,
		.of_match_table = qcn_sdio_dt_match,
	},
};

static int __init qcn_sdio_init(void)
{
	return platform_driver_register(&qcn_sdio_plat_driver);
}

static void __exit qcn_sdio_exit(void)
{
	platform_driver_unregister(&qcn_sdio_plat_driver);
}

module_init(qcn_sdio_init);
module_exit(qcn_sdio_exit);

int sdio_al_is_ready(void)
{
	if (atomic_read(&status))
		return 0;
	else
		return -EBUSY;
}
EXPORT_SYMBOL(sdio_al_is_ready);

struct sdio_al_client_handle *sdio_al_register_client(
					struct sdio_al_client_data *client_data)
{
	struct qcn_sdio_client_info *client_info = NULL;

	if (!((client_data) && (client_data->name) &&
			(client_data->probe) && (client_data->remove))) {
		pr_err("%s: SDIO: Invalid param\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	client_info = (struct qcn_sdio_client_info *)
		kzalloc(sizeof(struct qcn_sdio_client_info), GFP_KERNEL);
	if (!client_info)
		return ERR_PTR(-ENOMEM);

	memcpy(&client_info->cli_data, client_data,
					sizeof(struct sdio_al_client_data));

	if (!strcmp(client_data->name, "SDIO_AL_CLIENT_TTY")) {
		client_info->cli_handle.id = QCN_SDIO_CLI_ID_TTY;
		client_info->cli_handle.block_size = QCN_SDIO_TTY_BLK_SZ;
	} else if (!strcmp(client_data->name, "SDIO_AL_CLIENT_WLAN")) {
		client_info->cli_handle.id = QCN_SDIO_CLI_ID_WLAN;
		client_info->cli_handle.block_size = QCN_SDIO_MROM_BLK_SZ;
	} else if (!strcmp(client_data->name, "SDIO_AL_CLIENT_QMI")) {
		client_info->cli_handle.id = QCN_SDIO_CLI_ID_QMI;
		client_info->cli_handle.block_size = QCN_SDIO_MROM_BLK_SZ;
	} else if (!strcmp(client_data->name, "SDIO_AL_CLIENT_DIAG")) {
		client_info->cli_handle.id = QCN_SDIO_CLI_ID_DIAG;
		client_info->cli_handle.block_size = QCN_SDIO_MROM_BLK_SZ;
	} else {
		pr_err("%s: SDIO: Invalid name\n", __func__);
		kfree(client_info);
		return ERR_PTR(-EINVAL);
	}
	client_info->cli_handle.client_data = &client_info->cli_data;

	INIT_LIST_HEAD(&client_info->ch_head);
	mutex_lock(&lock);
	list_add_tail(&client_info->cli_list, &cinfo_head);
	mutex_unlock(&lock);

	client_info->is_probed = 0;
	if ((sdio_ctxt) && (sdio_ctxt->curr_sw_mode)) {
		if ((sdio_ctxt->curr_sw_mode == QCN_SDIO_SW_MROM) &&
			(client_info->cli_handle.id > QCN_SDIO_CLI_ID_TTY)) {
			qcn_sdio_config(client_info);
			client_info->is_probed = !client_data->probe(
						&client_info->cli_handle);
			qcn_send_meta_info(QCN_SDIO_DOORBELL_HEVENT, (u32)0);
		}
	}

	return &client_info->cli_handle;
}
EXPORT_SYMBOL(sdio_al_register_client);

void sdio_al_deregister_client(struct sdio_al_client_handle *handle)
{
	struct qcn_sdio_ch_info	*ch_info = NULL;
	struct qcn_sdio_client_info *client_info = NULL;

	if (!handle) {
		pr_err("%s: SDIO: Invalid param\n", __func__);
		return;
	}

	client_info = container_of(handle, struct qcn_sdio_client_info,
								cli_handle);

	while (!list_empty(&client_info->ch_head)) {
		ch_info = list_first_entry(&client_info->ch_head,
					struct qcn_sdio_ch_info, ch_list);
		sdio_al_deregister_channel(&ch_info->ch_handle);
	}
	mutex_lock(&lock);
	list_del(&client_info->cli_list);
	kfree(client_info);
	mutex_unlock(&lock);
}
EXPORT_SYMBOL(sdio_al_deregister_client);

struct sdio_al_channel_handle *sdio_al_register_channel(
		struct sdio_al_client_handle *client_handle,
		struct sdio_al_channel_data *channel_data)
{
	struct qcn_sdio_ch_info	*ch_info = NULL;
	struct qcn_sdio_client_info *client_info = NULL;

	if (!((channel_data) && (channel_data->name) && (client_handle) &&
				(channel_data->client_data))) {
		pr_err("%s: SDIO: Invalid param\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	ch_info = kzalloc(sizeof(struct qcn_sdio_ch_info), GFP_KERNEL);
	if (!ch_info)
		return ERR_PTR(-ENOMEM);

	memcpy(&ch_info->ch_data, channel_data,
					sizeof(struct sdio_al_channel_data));

	if ((!strcmp(channel_data->name, "SDIO_AL_WLAN_CH0")) ||
			(!strcmp(channel_data->name, "SDIO_AL_TTY_CH0"))) {
		if (atomic_read(&sdio_ctxt->ch_status[QCN_SDIO_CH_0]) < 0)
			ch_info->ch_handle.channel_id = QCN_SDIO_CH_0;
	} else if (!strcmp(channel_data->name, "SDIO_AL_WLAN_CH1")) {
		if (atomic_read(&sdio_ctxt->ch_status[QCN_SDIO_CH_1]) < 0)
			ch_info->ch_handle.channel_id = QCN_SDIO_CH_1;
	} else if (!strcmp(channel_data->name, "SDIO_AL_QMI_CH0")) {
		if (atomic_read(&sdio_ctxt->ch_status[QCN_SDIO_CH_2]) < 0)
			ch_info->ch_handle.channel_id = QCN_SDIO_CH_2;
	} else if (!strcmp(channel_data->name, "SDIO_AL_DIAG_CH0")) {
		if (atomic_read(&sdio_ctxt->ch_status[QCN_SDIO_CH_3]) < 0)
			ch_info->ch_handle.channel_id = QCN_SDIO_CH_3;
	} else {
		pr_err("%s: SDIO: Invalid CH name: %s\n", __func__,
							channel_data->name);
		kfree(ch_info);
		return ERR_PTR(-EINVAL);
	}

	client_info = container_of(client_handle, struct qcn_sdio_client_info,
								cli_handle);
	ch_info->ch_handle.channel_data = &ch_info->ch_data;
	ch_info->chandle = &client_info->cli_handle;
	list_add_tail(&ch_info->ch_list, &client_info->ch_head);
	sdio_ctxt->ch[ch_info->ch_handle.channel_id] = ch_info;
	atomic_set(&sdio_ctxt->ch_status[ch_info->ch_handle.channel_id], 0);

	return &ch_info->ch_handle;
}
EXPORT_SYMBOL(sdio_al_register_channel);

void sdio_al_deregister_channel(struct sdio_al_channel_handle *ch_handle)
{
	int ret = 0;
	struct qcn_sdio_ch_info *ch_info = NULL;

	if (!ch_handle) {
		pr_err("%s: Error: Invalid Param\n", __func__);
		return;
	}

	do {
		ret = atomic_cmpxchg(
			&sdio_ctxt->ch_status[ch_handle->channel_id], 0, 1);
		if (ret) {
			if (ret == -1)
				return;

			usleep_range(1000, 1500);
		}
	} while (ret);

	ch_info = sdio_ctxt->ch[ch_handle->channel_id];
	if (ch_info) {
		list_del(&ch_info->ch_list);
		sdio_ctxt->ch[ch_handle->channel_id] = NULL;
		atomic_set(&sdio_ctxt->ch_status[ch_handle->channel_id], -1);
		kfree(ch_info);
	}
}
EXPORT_SYMBOL(sdio_al_deregister_channel);

int sdio_al_queue_transfer_async(struct sdio_al_channel_handle *handle,
		enum sdio_al_dma_direction dir,
		void *buf, size_t len, int priority, void *ctxt)
{
	struct qcn_sdio_rw_info *rw_req = NULL;
	u32 cid = QCN_SDIO_CH_MAX;

	if (!atomic_read(&xport_status))
		return -ENODEV;

	if (!handle) {
		pr_err("%s: Error: Invalid Param\n", __func__);
		return -EINVAL;
	}

	cid = handle->channel_id;

	if (!(cid < QCN_SDIO_CH_MAX) &&
				(atomic_read(&sdio_ctxt->ch_status[cid]) < 0))
		return -EINVAL;

	if (dir == SDIO_AL_TX && atomic_read(&sdio_ctxt->free_list_count) <= 8)
		return -ENOMEM;

	rw_req = qcn_sdio_alloc_rw_req();
	if (!rw_req)
		return -ENOMEM;

	rw_req->cid = cid;
	rw_req->dir = dir;
	rw_req->buf = buf;
	rw_req->len = len;
	rw_req->ctxt = ctxt;

	if (dir == SDIO_AL_RX)
		spin_lock(&async_lock);

	qcn_sdio_add_rw_req(rw_req);
	queue_work(sdio_ctxt->qcn_sdio_wq, &sdio_ctxt->sdio_rw_w);

	if (dir == SDIO_AL_RX)
		spin_unlock(&async_lock);

	return 0;
}
EXPORT_SYMBOL(sdio_al_queue_transfer_async);

int sdio_al_queue_transfer(struct sdio_al_channel_handle *ch_handle,
		enum sdio_al_dma_direction dir,
		void *buf, size_t len, int priority)
{
	int ret = 0;
	u32 cid = QCN_SDIO_CH_MAX;

	if (!atomic_read(&xport_status))
		return -ENODEV;

	if (!ch_handle) {
		pr_err("%s: SDIO: Invalid Param\n", __func__);
		return -EINVAL;
	}

	if (dir == SDIO_AL_RX && !list_empty(&sdio_ctxt->rw_wait_q) &&
				!atomic_read(&sdio_ctxt->wait_list_count)) {
		sdio_al_queue_transfer_async(ch_handle, dir, buf, len, true,
							(void *)(uintptr_t)len);
		pr_info("%s: switching to async\n", __func__);
		ret = 1;
	} else {
		cid = ch_handle->channel_id;

		if (!(cid < QCN_SDIO_CH_MAX))
			return -EINVAL;

		if (dir == SDIO_AL_RX) {
			if (!atomic_read(&sdio_ctxt->wait_list_count))
				ret = qcn_sdio_recv_buff(cid, buf, len);
			else {
				sdio_al_queue_transfer_async(ch_handle, dir,
					buf, len, true, (void *)(uintptr_t)len);
				pr_info("%s switching to async\n", __func__);
				ret = 1;
			}

			if (rx_dump)
				HEX_DUMP("SYNC_RECV: ", buf, len);
		} else if (dir == SDIO_AL_TX) {
			ret = qcn_sdio_send_buff(cid, buf, len);
			if (tx_dump)
				HEX_DUMP("SYNC_SEND: ", buf, len);
		} else
			ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(sdio_al_queue_transfer);

int sdio_al_meta_transfer(struct sdio_al_channel_handle *handle,
					unsigned int data, unsigned int trans)
{
	u32 cid = QCN_SDIO_CH_MAX;
	u8 event = 0;

	if (!atomic_read(&xport_status))
		return -ENODEV;

	if (!handle)
		return -EINVAL;

	cid = handle->channel_id;

	if (!(cid < QCN_SDIO_CH_MAX))
		return -EINVAL;

	event = (u8)((data & QCN_SDIO_HMETA_EVENT_BMSK) >>
						QCN_SDIO_HMETA_EVENT_SHFT);

	if (cid == QCN_SDIO_CH_0) {
		if ((event < QCN_SDIO_META_START_CH0) &&
					(event >= QCN_SDIO_META_START_CH1)) {
			return -EINVAL;
		}
	} else if (cid == QCN_SDIO_CH_1) {
		if ((event < QCN_SDIO_META_START_CH1) &&
					(event >= QCN_SDIO_META_START_CH2)) {
			return -EINVAL;
		}
	} else if (cid == QCN_SDIO_CH_2) {
		if ((event < QCN_SDIO_META_START_CH2) &&
					(event >= QCN_SDIO_META_START_CH3)) {
			return -EINVAL;
		}
	} else if (cid == QCN_SDIO_CH_3) {
		if ((event < QCN_SDIO_META_START_CH3) &&
					(event >= QCN_SDIO_META_END)) {
			return -EINVAL;
		}
	}

	return qcn_send_meta_info(event, data);
}
EXPORT_SYMBOL(sdio_al_meta_transfer);

int qcn_sdio_card_state(bool enable)
{
	int ret = 0;

	if (!current_host)
		return -ENODEV;

	mmc_try_claim_host(current_host, 2000);
	if (enable) {
		if (!atomic_read(&xport_status)) {
			ret = mmc_add_host(current_host);
			if (ret)
				pr_err("%s ret = %d\n", __func__, ret);
		}
	} else {
		if (atomic_read(&xport_status))
			mmc_remove_host(current_host);
	}
	mmc_release_host(current_host);

	return ret;
}
EXPORT_SYMBOL(qcn_sdio_card_state);

static ssize_t qcn_card_state(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf,
			      size_t count)
{
	int state = 0;

	if (sscanf(buf, "%du", &state) != 1)
		return -EINVAL;

	qcn_sdio_card_state(state);

	return count;
}
static DEVICE_ATTR(card_state, 0220, NULL, qcn_card_state);

static int qcn_create_sysfs(struct device *dev)
{
	int ret = 0;

	ret = device_create_file(dev, &dev_attr_card_state);
	if (ret) {
		pr_err("Failed to create device file, err = %d\n", ret);
		goto out;
	}

	return 0;
out:
	return ret;
}
