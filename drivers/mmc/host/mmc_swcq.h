/* SPDX-License-Identifier: GPL-2.0 */
//This file has been modified by Unisoc (Shanghai) Technologies Co., Ltd in 2023.
#ifndef LINUX_MMC_SWCQ_H
#define LINUX_MMC_SWCQ_H
#include <linux/blkdev.h>
#include <linux/mmc/mmc.h>
#include "../core/core.h"
#include "../core/queue.h"
#include "../core/mmc_ops.h"
#include "../core/card.h"
#include <linux/delay.h>
#include "../host/sdhci.h"


#define EMMC_MAX_QUEUE_DEPTH		(32)
#ifndef MMC_CQE_RETRIES
#define MMC_CQE_RETRIES 2
#endif

#define dbg_max_cnt (400)
#define TIMEOUT_PRINT_CNT 40
#define CMDQ_TIMEOUT_PRINT_CNT 80

#define RANDOM_BLKS 8

enum mmc_dbg_type {
	MMC_SEND_CMD,
	MMC_CMD_RSP,
	MMC_SWCQ_RQ,
	HSQ_POST_RQ,
	CMDQ_POST_RQ,
	HSQ_PUMP_RQ,
	CMDQ_PUMP_RQ,
	PUMP_RQ_BUSY,
	PUMP_RQ_EXCEPTION,
	CMDQ_WORK_FINISH,
	DBG_TYPE_NUM,
};

struct dbg_run_host_log {
	unsigned long long time_sec;
	unsigned long long time_usec;
	int type;
	int cmd;
	int arg;
	int blocks;
	int qcnt;
	int cmdq_cnt;
	/*
	 * flags =
	 * work_on<<24 | hsq_running<<16 | enabled <<8 |pump_busy<<4
	 * |busy<<2|timer_running<<1 | cmdq_mode
	 */
	unsigned int task_id_index;
	int flags;
	int skip;
	int pid;
	int tag;
	struct mmc_request *mrq;
};

struct cmdq_slot {
	struct mmc_request *mrq;
	struct mmc_request *ext_mrq;
	atomic_t used;
};
struct swcq_check {
	u32 blk_addr;
	u32 blocks;
	int idx;
};

struct swcq_node {
	struct mmc_request *mrq;
	struct list_head link;
	atomic_t used;
};

struct swcq_slot {
	struct mmc_request *mrq;
	unsigned long long time;
};

struct mmc_swcq {
	/*****GLOBAL*******/
	struct mmc_host *mmc;/*attached mmc host*/
	struct mmc_request *mrq;/*handling mrq(hsq mode)*/
	struct mmc_queue *mq;/*attached mmc queue*/
	wait_queue_head_t wait_queue;/*wait queue when busy*/
	struct swcq_slot *slot;/*slots for store block layer mmc request*/
	spinlock_t lock;/*lock for operating this struct*/
	struct work_struct retry_work;/*retry work for when busy*/

	int next_tag;/*next to fetch tag*/
	int num_slots;/*total slots*/
	atomic_t qcnt;/*active count in slot*/

	bool enabled;/*enable of this cqe driver*/
	bool waiting_for_idle;/*judge for hsq+cmdq idle state*/
	bool waiting_for_cmdq_idle;/*judge cmdq idle state*/
	bool waiting_for_hsq_idle;/*judge hsq mode idle state*/
	bool recovery_halt;/*recovery when halt*/
	/*******FOR CMDQ******/
	atomic_t		cmdq_cnt;/*cmdq request numbers in cmdq_slot*/
	atomic_t		random_cnt;/*record 4k random rq*/
	atomic_t		sequential_cnt; /*record non-4k sequential rq*/
	spinlock_t		cmd_que_lock;/*lock for operating on cmd que*/
	spinlock_t		rqlist_lock;/*lock for rqlist operating*/
	spinlock_t		data_que_lock;/*lock for data queue operating*/
	spinlock_t		log_lock;/*lock for adding log*/
	spinlock_t		cmd_node_lock;/*lock for node adding/removing*/
	spinlock_t		data_node_lock;/*lock for node adding/removing*/
	struct list_head	cmd_que;/*queue for cmd type request*/
	struct list_head	rq_list;/*list for tmp store cmd type request*/
	struct list_head	data_que;/*queue for data type request*/
	unsigned long		state;/*not use by now*/
	wait_queue_head_t	cmdq_que;/*wait for cmd finished*/
	struct mmc_request	*done_mrq;/*finished mmc request*/
	struct mmc_command	chk_cmd;/*cmd for checking task ready*/
	struct mmc_request	chk_mrq;
	struct mmc_command	que_cmd;
	struct mmc_request	que_mrq;
	struct mmc_command	deq_cmd;
	struct mmc_request	deq_mrq;

	atomic_t		cq_rw;
	atomic_t		cq_w;
	unsigned int	wp_error;
	atomic_t		cq_wait_rdy;
	atomic_t		cq_rdy_cnt;
	unsigned long	task_id_index;
	int				cur_rw_task;
  #define CQ_TASK_IDLE 99
	atomic_t	is_data_dma;
	atomic_t	cq_tuning_now;
	unsigned int	data_mrq_queued[EMMC_MAX_QUEUE_DEPTH];
	unsigned int	cmdq_support_changed;
	int			align_size;
	struct mmc_queue_req	mqrq[EMMC_MAX_QUEUE_DEPTH];/*mqrq for cmd44/45*/
	bool need_polling;/*polling mode when cmdq mode*/
	bool need_intr;/*interrupt mode after polling timeout */
	wait_queue_head_t wait_cmdq_idle;/*wait for cmdq being idle*/
	wait_queue_head_t wait_hsq_idle;/*wait for hsq being idle*/
	struct cmdq_slot cmdq_slot[EMMC_MAX_QUEUE_DEPTH];/*slot only cmdq request*/

	struct work_struct cmdq_work;/*work handling cmdq issues*/
	struct delayed_work delayed_pump_work;/*delayed work to pump request when required*/
	struct timer_list check_timer;	/* Timer for checking if need cmdq */

	int worker_pid;/*cmdq work pid*/
	atomic_t work_on;/*cmdq work is running*/
	atomic_t busy;/*cmd6 is in busy*/

	int cmdq_depth;/*cmdq depth support by emmc device*/
	int poll_timeout;/*polling timeout value*/
	int timeout;/*interval of cmdq checking timer*/
	bool pump_busy;/*during pump processing*/
	bool initialized;/*ready to use after initialized*/
	bool cmdq_mode;/*swcq running mode. 1: cmdq mode 0: hsq mode*/
	bool timer_running;/*cmdq checking timer running state*/
	bool mode_need_change;/*mode need change state*/
	bool hsq_running;/*1: hsq is running 0: hsq in not running*/
	bool switch_en;/*1:enable switch 0: disable switch*/
	struct swcq_check *check_slot;/*slot for checking, duplicate of slot*/
	struct swcq_node *cmd_node_array;/*node pool(workaround to replace link of mmc_host)*/
	struct swcq_node *data_node_array;/*node pool(workaround to replace link of mmc_host)*/
	int dbg_host_cnt;/*debug entries of cmd history*/
	int debug1;/*record the times of mmc mode changed*/
	struct dbg_run_host_log cmd_history[dbg_max_cnt];/*cmd hisotry buffer*/
	int recovery_cnt; /* record the times of entering cmdq recovery mode */
	bool cmdq_support;	/* Command Queue supported */
	u64 r1_address_error;
	u64 r1_block_len_error;
	u64 r1_wp_violation;
	u64 r1_card_ecc_failed;
	u64 r1_cc_error;
	u64 r1_error;
	u64 r1_out_of_range;
};
/*
 * There is one mmc_blk_data per slot.
 */
struct mmc_blk_data {
	struct device	*parent;
	struct gendisk	*disk;
	struct mmc_queue queue;
	struct list_head part;
	struct list_head rpmbs;

	unsigned int	flags;
#define MMC_BLK_CMD23	(1 << 0)	/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR	(1 << 1)	/* MMC Reliable write support */

	struct kref	kref;
	unsigned int	read_only;
	unsigned int	part_type;
	unsigned int	reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD	BIT(3)
#define MMC_BLK_CQE_RECOVERY	BIT(4)

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with dev_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int	part_curr;
	int	area_type;

	/* debugfs files (only in main mmc_blk_data) */
	struct dentry *status_dentry;
	struct dentry *ext_csd_dentry;
};

#define IS_RT_CLASS_REQ(x)	\
	(IOPRIO_PRIO_CLASS(req_get_ioprio(x)) == IOPRIO_CLASS_RT)

#define mmc_card_cmdq(c)        ((c)->ext_csd.cmdq_en)
#define HOST_IS_EMMC_TYPE(c) (((c)->caps2 & (MMC_CAP2_NO_SDIO | MMC_CAP2_NO_SD)) \
						== (MMC_CAP2_NO_SDIO | MMC_CAP2_NO_SD))
#define HOST_IS_SD_TYPE(c) (((c)->caps2 & (MMC_CAP2_NO_MMC | MMC_CAP2_NO_SDIO)) \
						== (MMC_CAP2_NO_MMC | MMC_CAP2_NO_SDIO))

int mmc_swcq_init(struct mmc_swcq *swcq, struct mmc_host *mmc);
void mmc_swcq_suspend(struct mmc_host *mmc);
int mmc_swcq_resume(struct mmc_host *mmc);
bool mmc_swcq_finalize_request(struct mmc_host *mmc, struct mmc_request *mrq);
void mmc_wait_cmdq_done(struct mmc_request *mrq);
void dump_cmd_history(struct mmc_swcq *swcq, int print_num);
extern enum mmc_issue_type mmc_issue_type(struct mmc_queue *mq, struct request *req);
extern void mmc_cqe_check_busy(struct mmc_queue *mq);
extern void dbg_add_host_log(struct mmc_host *mmc, int type,
		int cmd, int arg, struct mmc_request *mrq);
#endif
