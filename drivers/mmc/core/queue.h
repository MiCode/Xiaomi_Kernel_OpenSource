/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MMC_QUEUE_H
#define MMC_QUEUE_H

#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>

#ifdef CONFIG_MTK_EMMC_HW_CQ
static inline bool mmc_req_is_special(struct request *req)
{
	return req &&
		(req_op(req) == REQ_OP_FLUSH ||
		 req_op(req) == REQ_OP_DISCARD ||
		 req_op(req) == REQ_OP_SECURE_ERASE);
}
#endif

static inline struct mmc_queue_req *req_to_mmc_queue_req(struct request *rq)
{
	return blk_mq_rq_to_pdu(rq);
}

struct mmc_queue_req;

static inline struct request *mmc_queue_req_to_req(struct mmc_queue_req *mqr)
{
	return blk_mq_rq_from_pdu(mqr);
}

struct task_struct;
struct mmc_blk_data;
struct mmc_blk_ioc_data;

struct mmc_blk_request {
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	struct mmc_request	mrq_que;
#endif
	struct mmc_request	mrq;
	struct mmc_command	sbc;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	struct mmc_command	que;
#endif
	struct mmc_command	cmd;
	struct mmc_command	stop;
	struct mmc_data		data;
	int			retune_retry_done;
};

/**
 * enum mmc_drv_op - enumerates the operations in the mmc_queue_req
 * @MMC_DRV_OP_IOCTL: ioctl operation
 * @MMC_DRV_OP_IOCTL_RPMB: RPMB-oriented ioctl operation
 * @MMC_DRV_OP_BOOT_WP: write protect boot partitions
 * @MMC_DRV_OP_GET_CARD_STATUS: get card status
 * @MMC_DRV_OP_GET_EXT_CSD: get the EXT CSD from an eMMC card
 */
enum mmc_drv_op {
	MMC_DRV_OP_IOCTL,
	MMC_DRV_OP_IOCTL_RPMB,
	MMC_DRV_OP_BOOT_WP,
	MMC_DRV_OP_GET_CARD_STATUS,
	MMC_DRV_OP_GET_EXT_CSD,
};

struct mmc_queue_req {
#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
	struct request		*req;
#endif
	struct mmc_blk_request	brq;
	struct scatterlist	*sg;
	struct mmc_async_req	areq;
	enum mmc_drv_op		drv_op;
	int			drv_op_result;
	void			*drv_op_data;
	unsigned int		ioc_count;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	atomic_t		index;
#endif
#ifdef CONFIG_MTK_EMMC_HW_CQ
	struct mmc_cmdq_req	cmdq_req;
#endif
};

struct mmc_queue {
	struct mmc_card		*card;
	struct task_struct	*thread;
	struct semaphore	thread_sem;
#ifdef CONFIG_MTK_EMMC_HW_CQ
	unsigned long	flags;
#define MMC_QUEUE_SUSPENDED	(1 << 0)
#else
	bool			suspended;
#endif
	bool			asleep;
	struct mmc_blk_data	*blkdata;
	struct request_queue	*queue;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	struct mmc_queue_req	mqrq[EMMC_MAX_QUEUE_DEPTH];
#endif
	/*
	 * FIXME: this counter is not a very reliable way of keeping
	 * track of how many requests that are ongoing. Switch to just
	 * letting the block core keep track of requests and per-request
	 * associated mmc_queue_req data.
	 */
	atomic_t		qcnt;
#ifdef CONFIG_MTK_EMMC_HW_CQ
	struct mmc_queue_req	*mqrq_cmdq;
	struct work_struct	cmdq_err_work;
	struct completion	cmdq_pending_req_done;
	struct completion	cmdq_shutdown_complete;
	struct request		*cmdq_req_peeked;
	int (*err_check_fn)(struct mmc_card *card, struct mmc_async_req *areq);
	void (*cmdq_shutdown)(struct mmc_queue *mq);
	int (*issue_fn)(struct mmc_queue *mq, struct request *req);
	int (*cmdq_issue_fn)(struct mmc_queue *mq,
			     struct request *req);
	void (*cmdq_complete_fn)(struct request *req);
	void (*cmdq_error_fn)(struct mmc_queue *mq);
	enum blk_eh_timer_return (*cmdq_req_timed_out)(struct request *req);
#endif
};

#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
#define IS_RT_CLASS_REQ(x)	\
	(IOPRIO_PRIO_CLASS(req_get_ioprio(x)) == IOPRIO_CLASS_RT)
#endif

extern int mmc_init_queue(struct mmc_queue *, struct mmc_card *, spinlock_t *,
			  const char *, int);
extern void mmc_cleanup_queue(struct mmc_queue *);
#ifdef CONFIG_MTK_EMMC_HW_CQ
extern int mmc_queue_suspend(struct mmc_queue *mq, int wait);
#else
extern void mmc_queue_suspend(struct mmc_queue *);
#endif
extern void mmc_queue_resume(struct mmc_queue *);
#ifdef CONFIG_MTK_EMMC_HW_CQ
extern unsigned int mmc_cmdq_queue_map_sg(struct mmc_queue *mq,
	struct mmc_queue_req *mqrq);
#endif
extern unsigned int mmc_queue_map_sg(struct mmc_queue *,
				     struct mmc_queue_req *);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
extern void mmc_wait_cmdq_empty(struct mmc_host *host);
extern bool mmc_blk_part_cmdq_en(struct mmc_queue *mq);
#endif

#ifdef CONFIG_MTK_EMMC_HW_CQ
extern int mmc_cmdq_init(struct mmc_queue *mq, struct mmc_card *card);
extern void mmc_cmdq_clean(struct mmc_queue *mq, struct mmc_card *card);
#endif
#endif
