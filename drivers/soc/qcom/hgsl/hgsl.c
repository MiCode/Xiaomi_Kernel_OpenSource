// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
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

#include <asm/ioctl.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/uaccess.h>
#include <uapi/linux/hgsl.h>
#include <linux/delay.h>
#include "hgsl.h"
#include "hgsl_tcsr.h"

#define HGSL_DEVICE_NAME "hgsl"
#define HGSL_DEV_NUM 1

/* Support upto 3 GVMs: 3 DBQs(Low/Medium/High priority) per GVM */
#define MAX_DB_QUEUE 9

#define IORESOURCE_HWINF "hgsl_reg_hwinf"
#define IORESOURCE_GMUCX "hgsl_reg_gmucx"

/* Set-up profiling packets as needed by scope */
#define CMDBATCH_PROFILING 0x00000010

/* Ping the user of HFI when this command is done */
#define CMDBATCH_NOTIFY    0x00000020

#define CMDBATCH_EOF       0x00000100

/* Max retry count of waiting for free space of doorbell queue. */
#define HGSL_QFREE_MAX_RETRY_COUNT     (500)
#define GLB_DB_SRC_ISSUEIB_IRQ_ID_0    TCSR_SRC_IRQ_ID_0
#define GLB_DB_SRC_ISSUEIB_IRQ_ID_1    TCSR_SRC_IRQ_ID_1
#define GLB_DB_SRC_ISSUEIB_IRQ_ID_2    TCSR_SRC_IRQ_ID_2
#define GLB_DB_DEST_TS_RETIRE_IRQ_ID   TCSR_DEST_IRQ_ID_0
#define GLB_DB_DEST_TS_RETIRE_IRQ_MASK TCSR_DEST_IRQ_MASK_0

#define HGSL_TCSR_NUM 2

struct hw_version {
	unsigned int version;
	unsigned int release;
};

struct reg {
	unsigned long paddr;
	unsigned long size;
	void __iomem *vaddr;
};

struct db_buffer {
	int32_t dwords;
	void  *vaddr;
};

#define QHDR_STATUS_INACTIVE 0x00
#define QHDR_STATUS_ACTIVE 0x01

#define HGSL_CONTEXT_NUM                     (128)
#define HGSL_SEND_MSG_MAX_RETRY_COUNT        (150)

// Skip all commands from the bad context
#define HGSL_FT_POLICY_FLAG_KILL             BIT(2)

#define ALIGN_ADDRESS_4DWORD(addr)         (((addr)+15) & ((long long) ~15))
#define ALIGN_DWORD_ADDRESS_4DWORD(dwaddr) (ALIGN_ADDRESS_4DWORD((dwaddr) * \
				sizeof(uint32_t)) / sizeof(uint32_t))

enum HGSL_DBQ_METADATA_COMMAND_INFO {
	HGSL_DBQ_METADATA_CONTEXT_INFO,
	HGSL_DBQ_METADATA_QUEUE_INDEX,
	HGSL_DBQ_METADATA_COOPERATIVE_RESET,
};

#define HGSL_DBQ_CONTEXT_ANY                 (0x0)
#define HGSL_DBQ_OFFSET_ZERO                 (0x0)

#define HGSL_DBQ_WRITE_INDEX_OFFSET_IN_DWORD (0x0)
#define HGSL_DBQ_READ_INDEX_OFFSET_IN_DWORD  (0x1)

enum HGSL_DBQ_METADATA_COOPERATIVE_RESET_INFO {
	HGSL_DBQ_HOST_TO_GVM_HARDRESET_REQ,
	HGSL_DBQ_GVM_TO_HOST_HARDRESET_DISPATCH_IN_BUSY,
};

enum HGSL_DBQ_METADATA_CONTEXT_OFFSET_INFO {
	HGSL_DBQ_CONTEXT_CONTEXT_ID_OFFSET_IN_DWORD,
	HGSL_DBQ_CONTEXT_TIMESTAMP_OFFSET_IN_DWORD,
	HGSL_DBQ_CONTEXT_DESTROY_OFFSET_IN_DWORD,
	HGSL_DBQ_METADATA_CTXT_TOTAL_ENTITY_NUM,
};

/* DBQ structure
 *   IBs storage | reserved | w.idx/r.idx | ctxt.info | hard reset |
 * 0             1K         1.5K          2K          3.5K         |
 * |             |          |             |           |            |
 */

#define HGSL_DBQ_HFI_Q_INDEX_BASE_OFFSET_IN_DWORD            (1536 >> 2)
#define HGSL_DBQ_CONTEXT_INFO_BASE_OFFSET_IN_DWORD           (2048 >> 2)
#define HGSL_DBQ_COOPERATIVE_RESET_INFO_BASE_OFFSET_IN_DWORD (3584 >> 2)


static inline bool _timestamp_retired(struct hgsl_context *ctxt,
				unsigned int timestamp);

static inline void set_context_timestamp(struct hgsl_context *ctxt,
				unsigned int ts);
static void _signal_contexts(struct qcom_hgsl *hgsl);

static int db_get_busy_state(void *dbq_base);
static void db_set_busy_state(void *dbq_base, int in_busy);

static uint32_t hgsl_dbq_get_state_info(uint32_t *va_base, uint32_t command,
				uint32_t ctxt_id, uint32_t offset)
{
	uint32_t *dest = NULL;

	switch (command) {
	case HGSL_DBQ_METADATA_QUEUE_INDEX:
		dest = (uint32_t *)(va_base +
				HGSL_DBQ_HFI_Q_INDEX_BASE_OFFSET_IN_DWORD +
				offset);
		break;
	case HGSL_DBQ_METADATA_CONTEXT_INFO:
		dest = (uint32_t *)(va_base +
				HGSL_DBQ_CONTEXT_INFO_BASE_OFFSET_IN_DWORD +
				(HGSL_DBQ_METADATA_CTXT_TOTAL_ENTITY_NUM *
				ctxt_id) + offset);
		break;
	case HGSL_DBQ_METADATA_COOPERATIVE_RESET:
		dest = (uint32_t *)(va_base +
		HGSL_DBQ_COOPERATIVE_RESET_INFO_BASE_OFFSET_IN_DWORD +
				offset);
		break;
	default:
		break;
	}

	return *dest;
}

static void hgsl_dbq_set_state_info(uint32_t *va_base, uint32_t command,
				uint32_t ctxt_id, uint32_t offset,
				uint32_t value)
{
	uint32_t *dest = NULL;

	switch (command) {
	case HGSL_DBQ_METADATA_QUEUE_INDEX:
		dest = (uint32_t *)(va_base +
				HGSL_DBQ_HFI_Q_INDEX_BASE_OFFSET_IN_DWORD +
				(HGSL_DBQ_METADATA_CTXT_TOTAL_ENTITY_NUM *
				ctxt_id) + offset);
		*dest = value;
		break;
	case HGSL_DBQ_METADATA_CONTEXT_INFO:
		dest = (uint32_t *)(va_base +
				HGSL_DBQ_CONTEXT_INFO_BASE_OFFSET_IN_DWORD +
				(HGSL_DBQ_METADATA_CTXT_TOTAL_ENTITY_NUM *
				ctxt_id) + offset);
		*dest = value;
		break;
	case HGSL_DBQ_METADATA_COOPERATIVE_RESET:
		dest = (uint32_t *)(va_base +
		HGSL_DBQ_COOPERATIVE_RESET_INFO_BASE_OFFSET_IN_DWORD +
				offset);
		*dest = value;
		break;
	default:
		break;
	}
}

#define HFI_MSG_TYPE_CMD  0
#define HFI_MSG_TYPE_RET  1

/* HFI command define. */
#define HTOF_MSG_ISSUE_CMD 130

#define MSG_ISSUE_INF_SZ()	(sizeof(struct hgsl_db_cmds) >> 2)
#define MSG_ISSUE_IBS_SZ(numIB) \
		((numIB) * (sizeof(struct hgsl_fw_ib_desc) >> 2))

#define MSG_SEQ_NO_MASK     0xFFF00000
#define MSG_SEQ_NO_SHIFT    20
#define MSG_SEQ_NO_GET(x)   (((x) & MSG_SEQ_NO_MASK) >> MSG_SEQ_NO_SHIFT)
#define MSG_TYPE_MASK       0x000F0000
#define MSG_TYPE_SHIFT      16
#define MSG_TYPE_GET(x)     (((x) & MSG_TYPE_MASK) >> MSG_TYPE_SHIFT)
#define MSG_SZ_MASK         0x0000FF00
#define MSG_SZ_SHIFT        8
#define MSG_SZ_GET(x)       (((x) & MSG_SZ_MASK) >> MSG_SZ_SHIFT)
#define MSG_ID_MASK         0x000000FF
#define MSG_ID_GET(x)       ((x) & MSG_ID_MASK)

#define MAKE_HFI_MSG_HEADER(msgID, msgType, msgSize, msgSeqnum) \
				((msgID) | ((msgSize) << MSG_SZ_SHIFT) | \
				((msgType) << MSG_TYPE_SHIFT) | \
				((msgSeqnum) << MSG_SEQ_NO_SHIFT))

#define HFI_ISSUE_IB_HEADER(numIB, sz, msgSeqnum) \
					MAKE_HFI_MSG_HEADER( \
					HTOF_MSG_ISSUE_CMD, \
					HFI_MSG_TYPE_CMD, \
					sz,\
					msgSeqnum)

/*
 * GMU HFI memory allocation options:
 * RGS_GMU_HFI_BUFFER_DTCM: Allocated from GMU CM3 DTCM.
 * RGS_GMU_HFI_BUFFER_NON_CACHEMEM: POR mode. Allocated from non cached memory.
 */
enum db_buffer_mode_t {
	RGS_GMU_HFI_BUFFER_DTCM = 0,
	RGS_GMU_HFI_BUFFER_NON_CACHEMEM = 1,
	RGS_GMU_HFI_BUFFER_DEFAULT = 1
};

struct db_msg_request {
	int msg_has_response;
	int msg_has_ret_packet;
	int ignore_ret_packet;
	void *ptr_data;
	unsigned int msg_dwords;
} __packed;

struct db_msg_response {
	void *ptr_data;
	unsigned int size_dword;
} __packed;

/*
 * IB start address
 * IB size
 */
struct hgsl_fw_ib_desc {
	uint64_t addr;
	uint32_t sz;
} __packed;

struct hfi_msg_header_fields {
	uint32_t msg_id             : 8;   ///< 0~127 power, 128~255 eCP
	uint32_t msg_size_dword     : 8;   ///< unit in dword, maximum 255
	uint32_t msg_type           : 4;   ///< refer to adreno_hfi_msg_type_t
	uint32_t msg_packet_seq_no  : 12;
};

union hfi_msg_header {
	uint32_t u32_all;
	struct hfi_msg_header_fields fields;
};

/*
 * Context ID
 * cmd_flags
 * Per-context user space gsl timestamp. It has to be
 * greater than last retired timestamp.
 * Number of IB descriptors
 * An array of IB descriptors
 */
struct hgsl_db_cmds {
	union hfi_msg_header header;
	uint32_t ctx_id;
	uint32_t cmd_flags;
	uint32_t timestamp;
	uint64_t user_profile_gpuaddr;
	uint32_t num_ibs;
	uint32_t ib_desc_gmuaddr;
	struct hgsl_fw_ib_desc ib_descs[];
} __packed;

struct hgsl_db_msg_ret {
	uint32_t header;
	uint32_t ack;
	uint32_t err;
} __packed;

struct db_msg_id {
	uint32_t seq_no;
	uint32_t msg_id;
} __packed;

struct db_wait_retpacket {
	size_t event_signal;
	int in_use;
	struct db_msg_id db_msg_id;
	struct db_msg_response response;
} __packed;

struct db_ignore_retpacket {
	int in_use;
	struct db_msg_id db_msg_id;
} __packed;

struct doorbell_queue {
	struct dma_buf *dma;
	void  *vbase;
	struct db_buffer data;
	uint32_t state;
	struct mutex lock;
};

struct hgsl_active_wait {
	struct list_head head;
	struct hgsl_context *ctxt;
	unsigned int timestamp;
};

struct qcom_hgsl {
	struct device *dev;
	struct mutex lock;

	/* character device info */
	struct cdev cdev;
	dev_t device_no;
	struct class *driver_class;
	struct device *class_dev;

	/* registers mapping */
	struct reg reg_ver;
	struct reg reg_dbidx;

	atomic_t seq_num;

	struct doorbell_queue dbq[MAX_DB_QUEUE];

	/* Could disable db and use isync only */
	bool db_off;

	/* global doorbell tcsr */
	struct hgsl_tcsr *tcsr[HGSL_TCSR_NUM][HGSL_TCSR_ROLE_MAX];
	int tcsr_idx;
	struct hgsl_context **contexts;
	rwlock_t ctxt_lock;

	struct list_head active_wait_list;
	spinlock_t active_wait_lock;

	struct workqueue_struct *wq;
	struct work_struct ts_retire_work;

	struct hw_version *ver;
};

static int hgsl_reg_map(struct platform_device *pdev,
			char *res_name, struct reg *reg);

static void hgsl_reg_read(struct reg *reg, unsigned int off,
					unsigned int *value)
{
	if (reg == NULL)
		return;

	if (WARN(off > reg->size,
		"Invalid reg read:0x%x, reg size:0x%x\n",
						off, reg->size))
		return;
	*value = __raw_readl(reg->vaddr + off);

	/* ensure this read finishes before the next one.*/
	rmb();
}

static void hgsl_reg_write(struct reg *reg, unsigned int off,
					unsigned int value)
{
	if (reg == NULL)
		return;

	if (WARN(off > reg->size,
		"Invalid reg write:0x%x, reg size:0x%x\n",
						off, reg->size))
		return;

	/*
	 * ensure previous writes post before this one,
	 * i.e. act like normal writel()
	 */
	wmb();
	__raw_writel(value, (reg->vaddr + off));
}

static inline bool is_global_db(int tcsr_idx)
{
	return (tcsr_idx >= 0);
}

static void gmu_ring_local_db(struct qcom_hgsl  *hgsl, unsigned int value)
{
	hgsl_reg_write(&hgsl->reg_dbidx, 0, value);
}

static void tcsr_ring_global_db(struct qcom_hgsl *hgsl, uint32_t tcsr_idx,
				uint32_t dbq_idx)
{
	if (tcsr_idx < HGSL_TCSR_NUM)
		hgsl_tcsr_irq_trigger(hgsl->tcsr[tcsr_idx][HGSL_TCSR_ROLE_SENDER],
						GLB_DB_SRC_ISSUEIB_IRQ_ID_0 + dbq_idx);
}

static bool hgsl_ctx_dbq_ready(struct hgsl_priv  *priv)
{
	struct qcom_hgsl *hgsl;
	struct doorbell_queue *dbq;

	hgsl = priv->dev;
	dbq = &hgsl->dbq[priv->dbq_idx];

	if ((dbq->state & DB_STATE_Q_MASK) == DB_STATE_Q_INIT_DONE)
		return true;

	return false;
}

static uint32_t db_queue_freedwords(struct doorbell_queue *dbq)
{
	uint32_t queue_size;
	uint32_t queue_used;
	uint32_t wptr;
	uint32_t rptr;

	if (dbq == NULL)
		return 0;

	wptr = hgsl_dbq_get_state_info((uint32_t *)dbq->vbase,
			HGSL_DBQ_METADATA_QUEUE_INDEX, HGSL_DBQ_CONTEXT_ANY,
				HGSL_DBQ_WRITE_INDEX_OFFSET_IN_DWORD);

	rptr = hgsl_dbq_get_state_info((uint32_t *)dbq->vbase,
			HGSL_DBQ_METADATA_QUEUE_INDEX, HGSL_DBQ_CONTEXT_ANY,
			HGSL_DBQ_READ_INDEX_OFFSET_IN_DWORD);

	queue_size = dbq->data.dwords;
	queue_used = (wptr + queue_size - rptr) % queue_size;
	return (queue_size - queue_used - 1);
}

static int db_queue_wait_freewords(struct doorbell_queue *dbq, uint32_t size)
{
	unsigned int retry_count = 0;
	unsigned int hard_reset_req = false;

	if (size == 0)
		return 0;

	if (dbq == NULL)
		return -EINVAL;

	do {
		hard_reset_req = hgsl_dbq_get_state_info((uint32_t *)dbq->vbase,
			HGSL_DBQ_METADATA_COOPERATIVE_RESET,
			HGSL_DBQ_CONTEXT_ANY,
			HGSL_DBQ_HOST_TO_GVM_HARDRESET_REQ);

		/* ensure read is done before comparison */
		rmb();

		if (hard_reset_req == true) {
			if (db_get_busy_state(dbq->vbase) == true)
				db_set_busy_state(dbq->vbase, false);
		} else {
			if (db_queue_freedwords(dbq) >= size) {
				db_set_busy_state(dbq->vbase, true);
				return 0;
			}
		}

		udelay(1000);
	} while (retry_count++ < HGSL_QFREE_MAX_RETRY_COUNT);

	return -ETIMEDOUT;
}

static int db_get_busy_state(void *dbq_base)
{
	unsigned int busy_state = false;

	busy_state = hgsl_dbq_get_state_info((uint32_t *)dbq_base,
		HGSL_DBQ_METADATA_COOPERATIVE_RESET,
		HGSL_DBQ_CONTEXT_ANY,
		HGSL_DBQ_GVM_TO_HOST_HARDRESET_DISPATCH_IN_BUSY);

	/* ensure read is done before comparison */
	rmb();

	return busy_state;
}

static void db_set_busy_state(void *dbq_base, int in_busy)
{
	hgsl_dbq_set_state_info((uint32_t *)dbq_base,
		HGSL_DBQ_METADATA_COOPERATIVE_RESET,
		HGSL_DBQ_CONTEXT_ANY,
		HGSL_DBQ_GVM_TO_HOST_HARDRESET_DISPATCH_IN_BUSY,
		in_busy);

	/* confirm write to memory done */
	wmb();
}

static int db_send_msg(struct hgsl_priv  *priv,
			struct db_msg_id *db_msg_id,
			struct db_msg_request *msg_req,
			struct db_msg_response *msg_resp)
{
	uint32_t msg_size_align;
	int ret;
	uint8_t *src, *dst;
	uint32_t move_dwords, resid_move_dwords;
	uint32_t queue_size_dword;
	struct qcom_hgsl *hgsl;
	struct doorbell_queue *dbq;
	uint32_t wptr;
	struct hgsl_db_cmds *cmds;
	int retry_count = 0;
	uint32_t hard_reset_req = false;

	hgsl = priv->dev;
	dbq = &hgsl->dbq[priv->dbq_idx];

	mutex_lock(&dbq->lock);

	cmds = (struct hgsl_db_cmds *)msg_req->ptr_data;
	do {
		hard_reset_req = hgsl_dbq_get_state_info((uint32_t *)dbq->vbase,
		HGSL_DBQ_METADATA_COOPERATIVE_RESET,
		HGSL_DBQ_CONTEXT_ANY,
		HGSL_DBQ_HOST_TO_GVM_HARDRESET_REQ);

		/* ensure read is done before comparison */
		rmb();

		if (hard_reset_req) {
			udelay(1000);
			if (retry_count++ > HGSL_SEND_MSG_MAX_RETRY_COUNT) {
				ret = -ETIMEDOUT;
				goto quit;
			}
		}
	} while (hard_reset_req);

	db_set_busy_state(dbq->vbase, true);

	queue_size_dword = dbq->data.dwords;
	msg_size_align = ALIGN(msg_req->msg_dwords, 4);

	ret = db_queue_wait_freewords(dbq, msg_size_align);
	if (ret < 0) {
		dev_err(hgsl->dev,
			"Timed out waiting for queue to free up\n");
		goto quit;
	}

	wptr = hgsl_dbq_get_state_info((uint32_t *)dbq->vbase,
			HGSL_DBQ_METADATA_QUEUE_INDEX, HGSL_DBQ_CONTEXT_ANY,
			HGSL_DBQ_WRITE_INDEX_OFFSET_IN_DWORD);

	move_dwords = msg_req->msg_dwords;
	if ((msg_req->msg_dwords + wptr) >= queue_size_dword) {
		move_dwords = queue_size_dword - wptr;
		resid_move_dwords = msg_req->msg_dwords - move_dwords;
		dst = (uint8_t *)dbq->data.vaddr;
		src = msg_req->ptr_data + (move_dwords << 2);
		memcpy(dst, src, (resid_move_dwords << 2));
	}

	dst = dbq->data.vaddr + (wptr << 2);
	src = msg_req->ptr_data;
	memcpy(dst, src, (move_dwords << 2));

	wptr = (wptr + msg_size_align) % queue_size_dword;
	hgsl_dbq_set_state_info((uint32_t *)dbq->vbase,
				HGSL_DBQ_METADATA_QUEUE_INDEX,
				HGSL_DBQ_CONTEXT_ANY,
				HGSL_DBQ_WRITE_INDEX_OFFSET_IN_DWORD,
							wptr);

	hgsl_dbq_set_state_info((uint32_t *)dbq->vbase,
				HGSL_DBQ_METADATA_CONTEXT_INFO,
				cmds->ctx_id,
				HGSL_DBQ_CONTEXT_CONTEXT_ID_OFFSET_IN_DWORD,
				cmds->ctx_id);

	hgsl_dbq_set_state_info((uint32_t *)dbq->vbase,
				HGSL_DBQ_METADATA_CONTEXT_INFO,
				((struct hgsl_db_cmds *)src)->ctx_id,
				HGSL_DBQ_CONTEXT_TIMESTAMP_OFFSET_IN_DWORD,
				((struct hgsl_db_cmds *)src)->timestamp);

	/* confirm write to memory done before ring door bell. */
	wmb();

	if (is_global_db(hgsl->tcsr_idx))
		/* trigger TCSR interrupt for global doorbell */
		tcsr_ring_global_db(hgsl, hgsl->tcsr_idx, priv->dbq_idx);
	else
		/* trigger GMU interrupt */
		gmu_ring_local_db(hgsl, priv->dbq_idx);

quit:
	db_set_busy_state(dbq->vbase, false);

	mutex_unlock(&dbq->lock);
	return ret;
}

static int hgsl_db_issue_cmd(struct hgsl_priv  *priv,
			uint32_t ctx_id, uint32_t num_ibs,
			uint32_t gmu_cmd_flags,
			uint32_t timestamp,
			struct hgsl_fw_ib_desc ib_descs[])
{
	int ret;
	uint32_t msg_dwords;
	uint32_t msg_buf_sz;
	struct hgsl_db_cmds *cmds;
	struct db_msg_request req;
	struct db_msg_response resp;
	struct db_msg_id db_msg_id;
	struct doorbell_queue *dbq;
	struct qcom_hgsl  *hgsl = priv->dev;
	struct hgsl_context *ctxt;
	uint32_t seq_num;

	db_msg_id.msg_id = HTOF_MSG_ISSUE_CMD;
	seq_num = atomic_inc_return(&hgsl->seq_num);
	db_msg_id.seq_no = seq_num;

	ctxt = hgsl->contexts[ctx_id];
	dbq = &hgsl->dbq[priv->dbq_idx];

	msg_dwords = MSG_ISSUE_INF_SZ() + MSG_ISSUE_IBS_SZ(num_ibs);
	msg_buf_sz = ALIGN(msg_dwords, 4) << 2;

	if (msg_buf_sz > dbq->data.dwords) {
		dev_err(hgsl->dev, "number of IBs exceed\n");
		return -EINVAL;
	}

	cmds = kmalloc(msg_buf_sz, GFP_KERNEL);
	if (cmds == NULL)
		return -ENOMEM;

	cmds->header = (union hfi_msg_header)HFI_ISSUE_IB_HEADER(num_ibs,
					msg_dwords,
					db_msg_id.seq_no);
	cmds->ctx_id = ctx_id;
	cmds->num_ibs = num_ibs;
	cmds->cmd_flags = gmu_cmd_flags;
	cmds->timestamp = timestamp;
	memcpy(cmds->ib_descs, ib_descs, sizeof(ib_descs[0]) * num_ibs);

	req.msg_has_response = 0;
	req.msg_has_ret_packet = 0;
	req.ignore_ret_packet = 1;
	req.msg_dwords = msg_dwords;
	req.ptr_data = cmds;

	if (!ctxt->is_killed) {
		ret = db_send_msg(priv, &db_msg_id, &req, &resp);
	} else {
		/* Retire ts immediately*/
		set_context_timestamp(ctxt, timestamp);

		/* Trigger event to waitfor ts thread */
		_signal_contexts(hgsl);
		ret = 0;
	}

	if (ret == 0)
		ctxt->queued_ts = timestamp;

	kfree(cmds);
	return ret;
}

#define USRPTR(a) u64_to_user_ptr((uint64_t)(a))

static int hgsl_cmdstream_db_issueib(struct file *filep,
				       unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct hgsl_ibdesc *ibs;
	struct hgsl_mem_object *bos;
	struct hgsl_fw_ib_desc *fw_ib_list = NULL;
	struct hgsl_fhi_issud_cmds submit_info;
	struct hgsl_fhi_issud_cmds *submit;
	uint64_t submit_size;
	uint32_t gmu_flags = CMDBATCH_NOTIFY;
	int idx;
	int ret = 0;

	if (hgsl->db_off) {
		dev_err(hgsl->dev, "Doorbell not open\n");
		return -EPERM;
	}

	copy_from_user(&submit_info, USRPTR(arg), sizeof(submit_info));

	if (!hgsl_ctx_dbq_ready(priv)) {
		dev_err(hgsl->dev, "Doorbell invalid\n");
		return -EINVAL;
	}

	submit_size = sizeof(*submit) +
		submit_info.num_ibs * sizeof(ibs[0]) +
		submit_info.num_bos * sizeof(bos[0]);

	if (submit_size == 0 || submit_size > U32_MAX)
		return -EINVAL;

	submit = kmalloc(submit_size, GFP_KERNEL);
	if (submit == NULL)
		return -ENOMEM;

	submit->context_id = submit_info.context_id;
	submit->timestamp = submit_info.timestamp;
	submit->flags = submit_info.flags;
	submit->num_ibs = submit_info.num_ibs;
	submit->num_bos = submit_info.num_bos;
	bos = (void *)(submit) + sizeof(*submit);
	ibs = (void *)&(bos[submit->num_bos]);

	if (submit->num_ibs) {
		ret = copy_from_user(ibs, USRPTR(submit_info.ibs),
			(sizeof(ibs[0]) * submit->num_ibs));
		if (ret) {
			ret = -EFAULT;
			goto exit;
		}

		fw_ib_list = kmalloc((sizeof(*fw_ib_list) * submit->num_ibs),
								GFP_KERNEL);
		if (fw_ib_list == NULL) {
			ret = -ENOMEM;
			goto exit;
		}
	}

	if (submit->num_bos) {
		ret = copy_from_user(bos, USRPTR(submit_info.bos),
			(sizeof(bos[0]) * submit->num_bos));
		if (ret) {
			ret = -EFAULT;
			goto exit;
		}
	}

	for (idx = 0; idx < submit->num_ibs; ++idx) {
		fw_ib_list[idx].addr = ibs[idx].gpuaddr;
		fw_ib_list[idx].sz = ibs[idx].sizedwords << 2;
	}

	if (submit->num_ibs)
		ret = hgsl_db_issue_cmd(priv, submit->context_id,
					submit->num_ibs,
					gmu_flags,
					submit->timestamp,
					fw_ib_list);

exit:
	if (ret) {
		dev_err(hgsl->dev,
			  "issueib with ts (%d) from ctxt (%d) failed\n",
			  submit->timestamp, submit->context_id);
	}

	kfree(submit);
	kfree(fw_ib_list);

	return ret;
}

static int hgsl_dbq_get_state(struct file *filep,
				 unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct doorbell_queue *dbq = &hgsl->dbq[priv->dbq_idx];
	uint32_t state;

	state = dbq->state & DB_STATE_Q_MASK;

	if (copy_to_user(USRPTR(arg), &state, sizeof(state)))
		return -EFAULT;

	return 0;
}

static void hgsl_reset_dbq(struct doorbell_queue *dbq)
{
	if (dbq->dma) {
		dma_buf_end_cpu_access(dbq->dma,
				       DMA_BIDIRECTIONAL);
		if (dbq->vbase) {
			dma_buf_vunmap(dbq->dma, dbq->vbase);
			dbq->vbase = NULL;
		}
		dma_buf_put(dbq->dma);
		dbq->dma = NULL;
	}

	dbq->state = DB_STATE_Q_UNINIT;
}

static int hgsl_dbq_assign(struct file *filep, unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct doorbell_queue *dbq;
	unsigned int dbq_idx;

	if (hgsl->db_off) {
		dev_err(hgsl->dev, "Doorbell not open\n");
		return -EPERM;
	}

	if (copy_from_user(&dbq_idx, USRPTR(arg), sizeof(dbq_idx)))
		return -EFAULT;

	if (dbq_idx >= MAX_DB_QUEUE)
		return -EINVAL;

	priv->dbq_idx = dbq_idx;
	dbq = &hgsl->dbq[priv->dbq_idx];

	return dbq->state;
}

static inline uint32_t get_context_timestamp(struct hgsl_context *ctxt)
{
	unsigned int ts;

	ts = *(unsigned int *)(ctxt->shadow_vbase +
						ctxt->shadow_eop_off);

	/* ensure read is done before comparison */
	rmb();
	return ts;
}

static inline void set_context_timestamp(struct hgsl_context *ctxt,
				unsigned int ts)
{
	unsigned int *ts_mem;

	ts_mem = (unsigned int *)(ctxt->shadow_vbase +
						ctxt->shadow_eop_off);

	*ts_mem = ts;

	/* ensure update is done before return */
	wmb();
}

static inline bool _timestamp_retired(struct hgsl_context *ctxt,
				unsigned int timestamp)
{
	return (get_context_timestamp(ctxt) >= timestamp);
}

static inline void _destroy_context(struct kref *kref);
static void _signal_contexts(struct qcom_hgsl *hgsl)
{
	struct hgsl_context *ctxt;
	int i;
	uint32_t ts;

	for (i = 0; i < HGSL_CONTEXT_NUM; i++) {
		read_lock(&hgsl->ctxt_lock);
		ctxt = hgsl->contexts[i];
		read_unlock(&hgsl->ctxt_lock);

		if (ctxt == NULL)
			continue;

		kref_get(&ctxt->kref);
		ts = get_context_timestamp(ctxt);
		if (ts != ctxt->last_ts) {
			hgsl_hsync_timeline_signal(ctxt->timeline, ts);
			ctxt->last_ts = ts;
		}
		kref_put(&ctxt->kref, _destroy_context);

	}

}

static void ts_retire_worker(struct work_struct *work)
{
	struct qcom_hgsl *hgsl =
		container_of(work, struct qcom_hgsl, ts_retire_work);
	struct hgsl_active_wait *wait, *w;

	spin_lock(&hgsl->active_wait_lock);
	list_for_each_entry_safe(wait, w, &hgsl->active_wait_list, head) {
		if (_timestamp_retired(wait->ctxt, wait->timestamp))
			wake_up_all(&wait->ctxt->wait_q);
	}
	spin_unlock(&hgsl->active_wait_lock);

	_signal_contexts(hgsl);
}

static irqreturn_t hgsl_tcsr_isr(struct device *dev, uint32_t status)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct qcom_hgsl *hgsl = platform_get_drvdata(pdev);

	if ((status & GLB_DB_DEST_TS_RETIRE_IRQ_MASK) == 0)
		return IRQ_NONE;

	queue_work(hgsl->wq, &hgsl->ts_retire_work);

	return IRQ_HANDLED;
}

static int hgsl_init_global_db(struct qcom_hgsl *hgsl,
				enum hgsl_tcsr_role role, int idx)
{
	struct device *dev = hgsl->dev;
	struct device_node *np = dev->of_node;
	bool  is_sender = (role == HGSL_TCSR_ROLE_SENDER);
	const char *node_name = is_sender ? "qcom,glb-db-senders" :
			"qcom,glb-db-receivers";
	struct device_node *tcsr_np;
	struct platform_device *tcsr_pdev;
	struct hgsl_tcsr *tcsr;
	int ret;

	if (hgsl->tcsr[idx][role] != NULL)
		return 0;

	tcsr_np = of_parse_phandle(np, node_name, idx);
	if (IS_ERR_OR_NULL(tcsr_np)) {
		dev_err(dev, "failed to find %s node\n", node_name);
		ret = -ENODEV;
		goto fail;
	}

	tcsr_pdev = of_find_device_by_node(tcsr_np);
	if (IS_ERR_OR_NULL(tcsr_pdev)) {
		dev_err(dev,
			"failed to find %s tcsr dev from node\n",
			is_sender ? "sender" : "receiver");
		ret = -ENODEV;
		goto fail;
	}

	tcsr = hgsl_tcsr_request(tcsr_pdev, role, dev,
			is_sender ? NULL : hgsl_tcsr_isr);
	if (IS_ERR_OR_NULL(tcsr)) {
		dev_err(dev,
			"failed to request %s tcsr, ret %lx\n",
			is_sender ? "sender" : "receiver", PTR_ERR(tcsr));
		ret = tcsr ? PTR_ERR(tcsr) : -ENODEV;
		goto fail;
	}

	ret = hgsl_tcsr_enable(tcsr);
	if (ret) {
		dev_err(dev,
			"failed to enable %s tcsr, ret %d\n",
			is_sender ? "sender" : "receiver", ret);
		goto free_tcsr;
	}

	if (!is_sender) {
		hgsl->contexts = kzalloc(sizeof(struct hgsl_context *) *
					HGSL_CONTEXT_NUM, GFP_KERNEL);
		if (!hgsl->contexts) {
			ret = -ENOMEM;
			goto disable_tcsr;
		}

		hgsl->wq = create_workqueue("hgsl-wq");
		if (IS_ERR_OR_NULL(hgsl->wq)) {
			dev_err(dev, "failed to create workqueue\n");
			ret = PTR_ERR(hgsl->wq);
			goto free_contexts;
		}
		INIT_WORK(&hgsl->ts_retire_work, ts_retire_worker);

		INIT_LIST_HEAD(&hgsl->active_wait_list);
		spin_lock_init(&hgsl->active_wait_lock);
		rwlock_init(&hgsl->ctxt_lock);

		hgsl_tcsr_irq_enable(tcsr, GLB_DB_DEST_TS_RETIRE_IRQ_MASK,
					true);
	}

	hgsl->tcsr[idx][role] = tcsr;
	return 0;

free_contexts:
	kfree(hgsl->contexts);
	hgsl->contexts = NULL;
disable_tcsr:
	hgsl_tcsr_disable(tcsr);
free_tcsr:
	hgsl_tcsr_free(tcsr);
fail:
	return ret;
}

static int hgsl_init_local_db(struct qcom_hgsl *hgsl)
{
	struct platform_device *pdev = to_platform_device(hgsl->dev);

	if (hgsl->reg_dbidx.vaddr != NULL)
		return 0;
	else
		return hgsl_reg_map(pdev, IORESOURCE_GMUCX, &hgsl->reg_dbidx);
}

static int hgsl_init_db_signal(struct qcom_hgsl *hgsl, int tcsr_idx)
{
	int ret;

	if (is_global_db(tcsr_idx)) {
		ret = hgsl_init_global_db(hgsl, HGSL_TCSR_ROLE_SENDER,
						tcsr_idx);
		ret |= hgsl_init_global_db(hgsl, HGSL_TCSR_ROLE_RECEIVER,
						tcsr_idx);
	} else {
		ret = hgsl_init_local_db(hgsl);
	}

	return ret;
}

static int hgsl_dbq_init(struct file *filep, unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct dma_buf *dmabuf;
	struct doorbell_queue *dbq;
	struct hgsl_db_queue_inf param;
	int tcsr_idx;
	int ret;

	if (hgsl->db_off) {
		dev_err(hgsl->dev, "Doorbell not open\n");
		return -EPERM;
	}

	copy_from_user(&param, USRPTR(arg), sizeof(param));
	if (param.fd < 0) {
		dev_err(hgsl->dev, "Invalid dbq fd\n");
		return -EINVAL;
	}

	if (param.head_off_dwords > INT_MAX ||
				param.queue_off_dwords > INT_MAX) {
		dev_err(hgsl->dev, "Invalid dbq offset\n");
		return -EINVAL;
	}

	if ((param.db_signal <= DB_SIGNAL_INVALID) ||
		(param.db_signal > DB_SIGNAL_MAX)) {
		dev_err(hgsl->dev, "Invalid db signal %d\n", param.db_signal);
		return -EINVAL;
	}

	dbq = &hgsl->dbq[priv->dbq_idx];
	mutex_lock(&dbq->lock);
	if (dbq->state == DB_STATE_Q_INIT_DONE) {
		mutex_unlock(&dbq->lock);
		return 0;
	}

	dbq->state = DB_STATE_Q_FAULT;
	dmabuf = dma_buf_get(param.fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(hgsl->dev, "Import DBQ buffer fail\n");
		ret = -EFAULT;
		goto err;
	}
	dbq->dma = dmabuf;

	dma_buf_begin_cpu_access(dbq->dma, DMA_BIDIRECTIONAL);
	dbq->vbase = dma_buf_vmap(dbq->dma);
	if (dbq->vbase == NULL) {
		dma_buf_put(dmabuf);
		ret = -EFAULT;
		goto err;
	}
	WARN_ON(param.head_dwords < 2);

	dbq->data.vaddr = dbq->vbase + (param.queue_off_dwords << 2);
	dbq->data.dwords = param.queue_dwords;

	tcsr_idx = (param.db_signal != DB_SIGNAL_LOCAL) ?
				param.db_signal - DB_SIGNAL_GLOBAL_0 : -1;
	ret = hgsl_init_db_signal(hgsl, tcsr_idx);
	if (ret != 0) {
		dev_err(hgsl->dev, "failed to init dbq signal %d, idx %d\n",
			param.db_signal, priv->dbq_idx);
		goto err;
	}

	hgsl->tcsr_idx = tcsr_idx;
	dbq->state = DB_STATE_Q_INIT_DONE;

	mutex_unlock(&dbq->lock);
	return 0;
err:
	hgsl_reset_dbq(dbq);
	mutex_unlock(&dbq->lock);

	return ret;
}

static inline void _destroy_context(struct kref *kref)
{
	struct hgsl_context *ctxt =
			container_of(kref, struct hgsl_context, kref);

	dma_buf_vunmap(ctxt->shadow_dma, ctxt->shadow_vbase);
	dma_buf_end_cpu_access(ctxt->shadow_dma, DMA_FROM_DEVICE);
	dma_buf_put(ctxt->shadow_dma);
	kfree(ctxt);
}

static int hgsl_context_create(struct file *filep, unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct hgsl_ctxt_create_info param;
	struct dma_buf *dmabuf;
	void *vbase;
	struct hgsl_context *ctxt;
	int ret = 0;

	if (hgsl->db_off) {
		dev_err(hgsl->dev, "Doorbell not open\n");
		return -EPERM;
	}

	if (!hgsl->contexts) {
		dev_err(hgsl->dev, "DBQ not initialized properly\n");
		return -ENODEV;
	}

	if (!is_global_db(hgsl->tcsr_idx)) {
		dev_err(hgsl->dev, "Global doorbell not supported for this process\n");
		return -ENODEV;
	}

	copy_from_user(&param, USRPTR(arg), sizeof(param));
	if (param.shadow_fd < 0) {
		dev_err(hgsl->dev, "Invalid shadow fd %d\n",
						param.shadow_fd);
		return -EBADF;
	}

	if (param.context_id >= HGSL_CONTEXT_NUM) {
		dev_err(hgsl->dev, "Invalid context id %d\n",
						param.context_id);
		return -EINVAL;
	}

	dmabuf = dma_buf_get(param.shadow_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(hgsl->dev, "Import shadow buffer fail\n");
		return -EFAULT;
	}

	dma_buf_begin_cpu_access(dmabuf, DMA_FROM_DEVICE);
	vbase = dma_buf_vmap(dmabuf);
	if (vbase == NULL) {
		ret = -EFAULT;
		goto err_dma_put;
	}

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (ctxt == NULL) {
		ret = -ENOMEM;
		goto err_dma_unmap;
	}

	ctxt->context_id = param.context_id;
	ctxt->shadow_dma = dmabuf;
	ctxt->shadow_vbase = vbase;
	ctxt->shadow_sop_off = param.shadow_sop_offset;
	ctxt->shadow_eop_off = param.shadow_eop_offset;
	init_waitqueue_head(&ctxt->wait_q);

	write_lock(&hgsl->ctxt_lock);
	if (hgsl->contexts[param.context_id] != NULL) {
		write_unlock(&hgsl->ctxt_lock);
		dev_err(hgsl->dev,
			"context id %d already created\n",
			param.context_id);
		ret = -EBUSY;
		goto err_free;
	}

	hgsl->contexts[param.context_id] = ctxt;

	priv->pid = task_pid_nr(current);

	hgsl->contexts[param.context_id]->pid = priv->pid;
	hgsl->contexts[param.context_id]->dbq_assigned = true;

	kref_init(&ctxt->kref);
	write_unlock(&hgsl->ctxt_lock);


	ret = hgsl_hsync_timeline_create(ctxt);
	if (ret < 0) {
		dev_err(hgsl->dev,
			"hsync timeline failed for context %d\n",
			param.context_id);
		goto err_free;
	}

	return 0;

err_free:
	kfree(ctxt);
err_dma_unmap:
	dma_buf_vunmap(dmabuf, vbase);
err_dma_put:
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	dma_buf_put(dmabuf);

	return ret;
}

static int hgsl_context_destroy(struct file *filep, unsigned long arg,
				bool force_cleanup)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	uint32_t context_id;
	struct hgsl_context *ctxt;

	if (hgsl->db_off) {
		dev_err(hgsl->dev, "Doorbell not open\n");
		return -EPERM;
	}

	if (!is_global_db(hgsl->tcsr_idx)) {
		dev_err(hgsl->dev, "Global doorbell not supported for this process\n");
		return -ENODEV;
	}

	if (!force_cleanup)
		copy_from_user(&context_id, USRPTR(arg),
						sizeof(context_id));
	else
		context_id = *(uint32_t *)arg;

	if (context_id >= HGSL_CONTEXT_NUM) {
		dev_err(hgsl->dev, "Invalid context id %d\n", context_id);
		return -EINVAL;
	}

	write_lock(&hgsl->ctxt_lock);
	if (hgsl->contexts[context_id] == NULL) {
		write_unlock(&hgsl->ctxt_lock);
		dev_err(hgsl->dev,
			"context id %d is not created\n",
			context_id);
		return -EINVAL;
	}

	ctxt = hgsl->contexts[context_id];
	hgsl->contexts[context_id] = NULL;

	/* unblock all waiting threads on this context */
	ctxt->in_destroy = true;
	wake_up_all(&ctxt->wait_q);

	ctxt->dbq_assigned = false;

	write_unlock(&hgsl->ctxt_lock);

	hgsl_hsync_timeline_put(ctxt->timeline);

	kref_put(&ctxt->kref, _destroy_context);
	return 0;
}

static int hgsl_wait_timestamp(struct file *filep, unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct hgsl_wait_ts_info param;
	struct hgsl_active_wait *wait;
	struct hgsl_context *ctxt;
	unsigned int timestamp;
	int ret;

	if (hgsl->db_off) {
		dev_err(hgsl->dev, "Doorbell not open\n");
		return -EPERM;
	}

	if (!is_global_db(hgsl->tcsr_idx)) {
		dev_err(hgsl->dev, "Global doorbell not supported for this process\n");
		return -ENODEV;
	}

	copy_from_user(&param, USRPTR(arg), sizeof(param));

	if (param.context_id >= HGSL_CONTEXT_NUM) {
		dev_err(hgsl->dev, "Invalid context id %d\n",
						param.context_id);
		return -EINVAL;
	}

	timestamp = param.timestamp;

	read_lock(&hgsl->ctxt_lock);
	ctxt = hgsl->contexts[param.context_id];
	read_unlock(&hgsl->ctxt_lock);
	if (ctxt == NULL) {
		dev_err(hgsl->dev,
			"context id %d is not created\n",
			param.context_id);
		return -EINVAL;
	}

	if (_timestamp_retired(ctxt, timestamp))
		return 0;

	kref_get(&ctxt->kref);

	wait = kzalloc(sizeof(*wait), GFP_KERNEL);
	if (!wait)
		return -ENOMEM;

	wait->ctxt = ctxt;
	wait->timestamp = timestamp;

	spin_lock(&hgsl->active_wait_lock);
	list_add_tail(&wait->head, &hgsl->active_wait_list);
	spin_unlock(&hgsl->active_wait_lock);

	ret = wait_event_interruptible_timeout(ctxt->wait_q,
				_timestamp_retired(ctxt, timestamp) ||
						ctxt->in_destroy,
				msecs_to_jiffies(param.timeout));
	if (ret == 0)
		ret = -ETIMEDOUT;
	else if (ret == -ERESTARTSYS)
		/* Let user handle this */
		ret = -EINTR;
	else
		ret = 0;

	spin_lock(&hgsl->active_wait_lock);
	list_del(&wait->head);
	spin_unlock(&hgsl->active_wait_lock);

	kfree(wait);

	kref_put(&ctxt->kref, _destroy_context);

	return ret;
}

static int hgsl_dbq_release(struct file *filep, unsigned long arg,
				bool force_cleanup)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct doorbell_queue *dbq;
	struct hgsl_dbq_release_info rel_info;
	int ret = 0;

	if (hgsl->db_off) {
		dev_err(hgsl->dev, "Doorbell not open\n");
		return -EPERM;
	}

	if (!force_cleanup)
		ret = copy_from_user(&rel_info, USRPTR(arg),
						sizeof(rel_info));
	else
		rel_info = *(struct hgsl_dbq_release_info *)arg;

	dbq = &hgsl->dbq[priv->dbq_idx];
	hgsl_dbq_set_state_info((uint32_t *)dbq->vbase,
				HGSL_DBQ_METADATA_CONTEXT_INFO,
				rel_info.ctxt_id,
				HGSL_DBQ_CONTEXT_CONTEXT_ID_OFFSET_IN_DWORD,
				rel_info.ctxt_id);

	hgsl_dbq_set_state_info((uint32_t *)dbq->vbase,
				HGSL_DBQ_METADATA_CONTEXT_INFO,
				rel_info.ctxt_id,
				HGSL_DBQ_CONTEXT_DESTROY_OFFSET_IN_DWORD,
				1);

	rel_info.ref_count = (dbq->state == DB_STATE_Q_INIT_DONE) ? 1 : 0;

	if (!force_cleanup)
		ret = copy_to_user(USRPTR(arg), &rel_info, sizeof(rel_info));

	return priv->dbq_idx;
}

static int hgsl_open(struct inode *inodep, struct file *filep)
{
	struct hgsl_priv *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	struct qcom_hgsl  *hgsl = container_of(inodep->i_cdev,
					       struct qcom_hgsl, cdev);

	if (!priv)
		return -ENOMEM;

	idr_init(&priv->isync_timeline_idr);
	spin_lock_init(&priv->isync_timeline_lock);

	priv->dev = hgsl;
	filep->private_data = priv;
	return 0;
}

static int hgsl_release(struct inode *inodep, struct file *filep)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct hgsl_dbq_release_info rel_info;
	int i;

	for (i = 0; i < HGSL_CONTEXT_NUM; i++) {
		if ((hgsl->contexts != NULL) &&
			(hgsl->contexts[i] != NULL) &&
			(priv->pid == hgsl->contexts[i]->pid)) {
			rel_info.ctxt_id = hgsl->contexts[i]->context_id;
			if (hgsl->contexts[i]->dbq_assigned)
				hgsl_dbq_release(filep,
						(unsigned long)&rel_info,
						true);

			hgsl_context_destroy(filep,
					(unsigned long)&rel_info.ctxt_id,
					true);
		}
	}

	hgsl_isync_fini(priv);

	kfree(priv);
	return 0;
}

static ssize_t hgsl_read(struct file *filep, char __user *buf, size_t count,
		loff_t *pos)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	uint32_t version = 0;
	uint32_t release = 0;
	char buff[100];

	if (!hgsl->db_off) {
		hgsl_reg_read(&hgsl->reg_ver, 0, &version);
		hgsl_reg_read(&hgsl->reg_ver, 4, &release);
		snprintf(buff, 100, "gpu HW Version:%x HW Release:%x\n",
							version, release);
	} else
		snprintf(buff, 100, "Doorbell closed\n");

	return simple_read_from_buffer(buf, count, pos,
			buff, strlen(buff) + 1);
}

static int hgsl_ioctl_hsync_fence_create(struct file *filep,
					   unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct qcom_hgsl *hgsl = priv->dev;
	struct hgsl_hsync_fence_create param;
	struct hgsl_context *ctxt;
	int ret = 0;

	if (hgsl->db_off) {
		dev_err(hgsl->dev, "Doorbell not open\n");
		return -EPERM;
	}

	copy_from_user(&param, USRPTR(arg), sizeof(param));

	if (param.context_id >= HGSL_CONTEXT_NUM)
		return -EINVAL;

	read_lock(&hgsl->ctxt_lock);
	ctxt = hgsl->contexts[param.context_id];
	read_unlock(&hgsl->ctxt_lock);

	if (ctxt == NULL) {
		dev_err(hgsl->dev,
			"context id %d is not created\n",
			param.context_id);
		return -EINVAL;
	}

	kref_get(&ctxt->kref);

	param.fence_fd = hgsl_hsync_fence_create_fd(ctxt, param.timestamp);
	if (param.fence_fd < 0) {
		ret = param.fence_fd;
		goto out;
	}
	copy_to_user(USRPTR(arg), &param, sizeof(param));
out:
	kref_put(&ctxt->kref, _destroy_context);

	return ret;
}

static int hgsl_ioctl_isync_timeline_create(struct file *filep,
					unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	uint32_t param = 0;
	int ret = 0;

	ret = hgsl_isync_timeline_create(priv, &param);
	if (ret == 0)
		copy_to_user(USRPTR(arg), &param, sizeof(param));

	return ret;
}

static int hgsl_ioctl_isync_timeline_destroy(struct file *filep,
					unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	uint32_t param = 0;
	int ret = 0;

	copy_from_user(&param, USRPTR(arg), sizeof(param));
	ret = hgsl_isync_timeline_destroy(priv, param);

	return ret;
}

static int hgsl_ioctl_isync_fence_create(struct file *filep,
					unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct hgsl_isync_create_fence param;
	int ret = 0;
	int fence = 0;

	copy_from_user(&param, USRPTR(arg), sizeof(param));

	ret = hgsl_isync_fence_create(priv, param.timeline_id,
						param.ts, &fence);

	if (ret == 0) {
		param.fence_id = fence;
		copy_to_user(USRPTR(arg), &param, sizeof(param));
	}

	return ret;
}

static int hgsl_ioctl_isync_fence_signal(struct file *filep,
					unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct hgsl_isync_signal_fence param;
	int ret = 0;

	copy_from_user(&param, USRPTR(arg), sizeof(param));

	ret = hgsl_isync_fence_signal(priv, param.timeline_id,
						  param.fence_id);

	return ret;
}

static int hgsl_ioctl_isync_forward(struct file *filep,
					unsigned long arg)
{
	struct hgsl_priv *priv = filep->private_data;
	struct hgsl_isync_forward param;
	int ret = 0;

	copy_from_user(&param, USRPTR(arg), sizeof(param));

	ret = hgsl_isync_forward(priv, param.timeline_id,
						  param.ts);

	return ret;
}

static long hgsl_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	case HGSL_IOCTL_ISSUE_CMDS:
		ret = hgsl_cmdstream_db_issueib(filep, arg);
		break;
	case HGSL_IOCTL_DBQ_GETSTATE:
		ret = hgsl_dbq_get_state(filep, arg);
		break;
	case HGSL_IOCTL_DBQ_ASSIGN:
		ret = hgsl_dbq_assign(filep, arg);
		break;
	case HGSL_IOCTL_DBQ_INIT:
		ret = hgsl_dbq_init(filep, arg);
		break;
	case HGSL_IOCTL_DBQ_RELEASE:
		ret = hgsl_dbq_release(filep, arg, false);
		break;
	case HGSL_IOCTL_CTXT_CREATE:
		ret = hgsl_context_create(filep, arg);
		break;
	case HGSL_IOCTL_CTXT_DESTROY:
		ret = hgsl_context_destroy(filep, arg, false);
		break;
	case HGSL_IOCTL_WAIT_TIMESTAMP:
		ret = hgsl_wait_timestamp(filep, arg);
		break;
	case HGSL_IOCTL_HSYNC_FENCE_CREATE:
		ret = hgsl_ioctl_hsync_fence_create(filep, arg);
		break;
	case HGSL_IOCTL_ISYNC_TIMELINE_CREATE:
		ret = hgsl_ioctl_isync_timeline_create(filep, arg);
		break;
	case HGSL_IOCTL_ISYNC_TIMELINE_DESTROY:
		ret = hgsl_ioctl_isync_timeline_destroy(filep, arg);
		break;
	case HGSL_IOCTL_ISYNC_FENCE_CREATE:
		ret = hgsl_ioctl_isync_fence_create(filep, arg);
		break;
	case HGSL_IOCTL_ISYNC_FENCE_SIGNAL:
		ret = hgsl_ioctl_isync_fence_signal(filep, arg);
		break;
	case HGSL_IOCTL_ISYNC_FORWARD:
		ret = hgsl_ioctl_isync_forward(filep, arg);
		break;

	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static long hgsl_compat_ioctl(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	return hgsl_ioctl(filep, cmd, arg);
}

static const struct file_operations hgsl_fops = {
	.owner = THIS_MODULE,
	.open = hgsl_open,
	.release = hgsl_release,
	.read = hgsl_read,
	.unlocked_ioctl = hgsl_ioctl,
	.compat_ioctl = hgsl_compat_ioctl
};

static int qcom_hgsl_register(struct platform_device *pdev,
				struct qcom_hgsl *hgsl_dev)
{
	int ret;

	ret = alloc_chrdev_region(&hgsl_dev->device_no, 0,
						HGSL_DEV_NUM,
						HGSL_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "alloc_chrdev_region failed %d\n", ret);
		return ret;
	}

	hgsl_dev->driver_class = class_create(THIS_MODULE, HGSL_DEVICE_NAME);
	if (IS_ERR(hgsl_dev->driver_class)) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "class_create failed %d\n", ret);
		goto exit_unreg_chrdev_region;
	}

	hgsl_dev->class_dev = device_create(hgsl_dev->driver_class,
					NULL,
					hgsl_dev->device_no,
					hgsl_dev, HGSL_DEVICE_NAME);

	if (IS_ERR(hgsl_dev->class_dev)) {
		dev_err(&pdev->dev, "class_device_create failed %d\n", ret);
		ret = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&hgsl_dev->cdev, &hgsl_fops);

	hgsl_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&hgsl_dev->cdev,
					MKDEV(MAJOR(hgsl_dev->device_no), 0),
					1);
	if (ret < 0) {
		dev_err(&pdev->dev, "cdev_add failed %d\n", ret);
		goto exit_destroy_device;
	}

	return 0;

exit_destroy_device:
	device_destroy(hgsl_dev->driver_class, hgsl_dev->device_no);
exit_destroy_class:
	class_destroy(hgsl_dev->driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(hgsl_dev->device_no, 1);
	return ret;
}

static void qcom_hgsl_deregister(struct platform_device *pdev)
{
	struct qcom_hgsl *hgsl_dev = platform_get_drvdata(pdev);

	cdev_del(&hgsl_dev->cdev);
	device_destroy(hgsl_dev->driver_class, hgsl_dev->device_no);
	class_destroy(hgsl_dev->driver_class);
	unregister_chrdev_region(hgsl_dev->device_no, HGSL_DEV_NUM);
}

static bool hgsl_is_db_off(struct platform_device *pdev)
{
	uint32_t db_off = 0;

	if (pdev == NULL)
		return true;

	of_property_read_u32(pdev->dev.of_node, "db-off", &db_off);

	return db_off == 1 ? true : false;
}

static int hgsl_reg_map(struct platform_device *pdev,
			char *res_name, struct reg *reg)
{
	struct resource *res;
	int ret = 0;

	if ((pdev == NULL) || (res_name == NULL) || (reg == NULL)) {
		ret = -EINVAL;
		goto exit;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						res_name);
	if (res == NULL) {
		dev_err(&pdev->dev, "get resource :%s failed\n",
								res_name);
		ret = -EINVAL;
		goto exit;
	}

	if (res->start == 0 || resource_size(res) == 0) {
		dev_err(&pdev->dev, "Register region %s is invalid\n",
								res_name);
		ret = -EINVAL;
		goto exit;
	}

	reg->paddr = res->start;
	reg->size = resource_size(res);
	if (devm_request_mem_region(&pdev->dev,
					reg->paddr, reg->size,
					res_name) == NULL) {
		dev_err(&pdev->dev, "request_mem_region  for %s failed\n",
								res_name);
		ret = -ENODEV;
		goto exit;
	}

	reg->vaddr = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (reg->vaddr == NULL) {
		dev_err(&pdev->dev, "Unable to remap %s registers\n",
								res_name);
		ret = -ENODEV;
		goto exit;
	}

exit:
	return ret;
}

static int qcom_hgsl_probe(struct platform_device *pdev)
{
	struct qcom_hgsl *hgsl_dev;
	int ret;
	int i;

	hgsl_dev = devm_kzalloc(&pdev->dev, sizeof(*hgsl_dev), GFP_KERNEL);
	if (!hgsl_dev)
		return -ENOMEM;

	hgsl_dev->dev = &pdev->dev;
	mutex_init(&hgsl_dev->lock);

	ret = qcom_hgsl_register(pdev, hgsl_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "qcom_hgsl_register failed, ret %d\n",
									ret);
		return ret;
	}

	hgsl_dev->db_off = hgsl_is_db_off(pdev);

	if (!hgsl_dev->db_off) {
		ret = hgsl_reg_map(pdev, IORESOURCE_HWINF,
						&hgsl_dev->reg_ver);
		if (ret < 0) {
			dev_err(&pdev->dev, "Unable to map resource:%s\n",
							IORESOURCE_HWINF);
			goto exit_dereg;
		}
	}


	for (i = 0; i < MAX_DB_QUEUE; i++) {
		mutex_init(&hgsl_dev->dbq[i].lock);
		hgsl_dev->dbq[i].state = DB_STATE_Q_UNINIT;
	}
	atomic_set(&hgsl_dev->seq_num, 0);

	platform_set_drvdata(pdev, hgsl_dev);

	return 0;

exit_dereg:
	qcom_hgsl_deregister(pdev);
	return ret;
}

static int qcom_hgsl_remove(struct platform_device *pdev)
{
	struct qcom_hgsl *hgsl = platform_get_drvdata(pdev);
	struct hgsl_tcsr *tcsr_sender, *tcsr_receiver;
	int i;

	for (i = 0; i < HGSL_TCSR_NUM; i++) {
		tcsr_sender = hgsl->tcsr[i][HGSL_TCSR_ROLE_SENDER];
		tcsr_receiver = hgsl->tcsr[i][HGSL_TCSR_ROLE_RECEIVER];

		if (tcsr_sender) {
			hgsl_tcsr_disable(tcsr_sender);
			hgsl_tcsr_free(tcsr_sender);
		}

		if (tcsr_receiver) {
			hgsl_tcsr_disable(tcsr_receiver);
			hgsl_tcsr_free(tcsr_receiver);
			flush_workqueue(hgsl->wq);
			destroy_workqueue(hgsl->wq);
			kfree(hgsl->contexts);
			hgsl->contexts = NULL;
		}
	}

	memset(hgsl->tcsr, 0, sizeof(hgsl->tcsr));

	for (i = 0; i < MAX_DB_QUEUE; i++)
		if (hgsl->dbq[i].state == DB_STATE_Q_INIT_DONE)
			hgsl_reset_dbq(&hgsl->dbq[i]);

	qcom_hgsl_deregister(pdev);
	return 0;
}

static const struct of_device_id qcom_hgsl_of_match[] = {
	{ .compatible = "qcom,hgsl" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_hgsl_of_match);

static struct platform_driver qcom_hgsl_driver = {
	.probe = qcom_hgsl_probe,
	.remove = qcom_hgsl_remove,
	.driver  = {
		.name  = "qcom-hgsl",
		.of_match_table = qcom_hgsl_of_match,
	},
};
module_platform_driver(qcom_hgsl_driver);

MODULE_DESCRIPTION("QTI Hypervisor Graphics system driver");
MODULE_LICENSE("GPL v2");
