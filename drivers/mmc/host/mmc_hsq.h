/* SPDX-License-Identifier: GPL-2.0 */
//This file has been modified by Unisoc (Shanghai) Technologies Co., Ltd in 2023.
#ifndef LINUX_MMC_HSQ_H
#define LINUX_MMC_HSQ_H

#define HSQ_NUM_SLOTS	64
#define HSQ_INVALID_TAG	HSQ_NUM_SLOTS

struct hsq_slot {
	struct mmc_request *mrq;
#ifdef CONFIG_SPRD_DEBUG
	unsigned long long time;
	int pre_next_tag;
	int pre_tail_tag;
	int post_next_tag;
	int post_tail_tag;
#endif
};

struct mmc_hsq {
	struct mmc_host *mmc;
	struct mmc_request *mrq;
	wait_queue_head_t wait_queue;
	struct hsq_slot *slot;
	spinlock_t lock;
	struct work_struct retry_work;

	int next_tag;
	int num_slots;
	int qcnt;
	int tail_tag;
	int tag_slot[HSQ_NUM_SLOTS];

	bool enabled;
	bool waiting_for_idle;
	bool recovery_halt;
#ifdef CONFIG_SPRD_DEBUG
	unsigned long long stamp1;
	unsigned long long stamp1_temp;
	unsigned long long stamp2;
	int record[HSQ_NUM_SLOTS];
	int record_end;
	int old_next_tag;
#endif
};

int mmc_hsq_init(struct mmc_hsq *hsq, struct mmc_host *mmc);
void mmc_hsq_suspend(struct mmc_host *mmc);
int mmc_hsq_resume(struct mmc_host *mmc);
bool mmc_hsq_finalize_request(struct mmc_host *mmc, struct mmc_request *mrq);

#endif
