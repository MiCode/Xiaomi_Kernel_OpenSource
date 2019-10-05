/*
 *  linux/drivers/mmc/core/core.h
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *  Copyright 2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _MMC_CORE_CORE_H
#define _MMC_CORE_CORE_H

#include <linux/delay.h>
#include <linux/sched.h>

struct mmc_host;
struct mmc_card;
struct mmc_request;

#define MMC_CMD_RETRIES        3

struct mmc_bus_ops {
	void (*remove)(struct mmc_host *);
	void (*detect)(struct mmc_host *);
	int (*pre_suspend)(struct mmc_host *);
	int (*suspend)(struct mmc_host *);
	int (*resume)(struct mmc_host *);
	int (*runtime_suspend)(struct mmc_host *);
	int (*runtime_resume)(struct mmc_host *);
	int (*power_save)(struct mmc_host *);
	int (*power_restore)(struct mmc_host *);
	int (*alive)(struct mmc_host *);
	int (*shutdown)(struct mmc_host *);
	int (*reset)(struct mmc_host *);
};

void mmc_attach_bus(struct mmc_host *host, const struct mmc_bus_ops *ops);
void mmc_detach_bus(struct mmc_host *host);

struct device_node *mmc_of_find_child_device(struct mmc_host *host,
		unsigned func_num);

void mmc_init_erase(struct mmc_card *card);

void mmc_set_chip_select(struct mmc_host *host, int mode);
void mmc_set_clock(struct mmc_host *host, unsigned int hz);
void mmc_set_bus_mode(struct mmc_host *host, unsigned int mode);
void mmc_set_bus_width(struct mmc_host *host, unsigned int width);
u32 mmc_select_voltage(struct mmc_host *host, u32 ocr);
int mmc_set_uhs_voltage(struct mmc_host *host, u32 ocr);
int mmc_set_signal_voltage(struct mmc_host *host, int signal_voltage);
void mmc_set_timing(struct mmc_host *host, unsigned int timing);
void mmc_set_driver_type(struct mmc_host *host, unsigned int drv_type);
int mmc_select_drive_strength(struct mmc_card *card, unsigned int max_dtr,
			      int card_drv_type, int *drv_type);
void mmc_power_up(struct mmc_host *host, u32 ocr);
void mmc_power_off(struct mmc_host *host);
void mmc_power_cycle(struct mmc_host *host, u32 ocr);
void mmc_set_initial_state(struct mmc_host *host);

static inline void mmc_delay(unsigned int ms)
{
	if (ms < 1000 / HZ) {
		cond_resched();
		mdelay(ms);
	} else {
		msleep(ms);
	}
}

void mmc_rescan(struct work_struct *work);
void mmc_start_host(struct mmc_host *host);
void mmc_stop_host(struct mmc_host *host);

int _mmc_detect_card_removed(struct mmc_host *host);
int mmc_detect_card_removed(struct mmc_host *host);

int mmc_attach_mmc(struct mmc_host *host);
int mmc_attach_sd(struct mmc_host *host);
int mmc_attach_sdio(struct mmc_host *host);

/* Module parameters */
extern bool use_spi_crc;

/* Debugfs information for hosts and cards */
void mmc_add_host_debugfs(struct mmc_host *host);
void mmc_remove_host_debugfs(struct mmc_host *host);

void mmc_add_card_debugfs(struct mmc_card *card);
void mmc_remove_card_debugfs(struct mmc_card *card);

void mmc_init_context_info(struct mmc_host *host);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
void mmc_wait_cmdq_empty(struct mmc_host *host);
void mmc_do_check(struct mmc_host *host);
void mmc_do_stop(struct mmc_host *host);
void mmc_do_status(struct mmc_host *host);
void mmc_wait_cmdq_done(struct mmc_request *mrq);
int mmc_run_queue_thread(void *data);
#endif

int mmc_execute_tuning(struct mmc_card *card);
int mmc_hs200_to_hs400(struct mmc_card *card);
int mmc_hs400_to_hs200(struct mmc_card *card);

#ifdef CONFIG_PM_SLEEP
void mmc_register_pm_notifier(struct mmc_host *host);
void mmc_unregister_pm_notifier(struct mmc_host *host);
#else
static inline void mmc_register_pm_notifier(struct mmc_host *host) { }
static inline void mmc_unregister_pm_notifier(struct mmc_host *host) { }
#endif

void mmc_wait_for_req_done(struct mmc_host *host, struct mmc_request *mrq);
bool mmc_is_req_done(struct mmc_host *host, struct mmc_request *mrq);

struct mmc_async_req;
#ifdef CONFIG_MTK_EMMC_HW_CQ
struct mmc_cmdq_req;

int mmc_cmdq_discard_queue(struct mmc_host *host, u32 tasks);
int mmc_cmdq_halt(struct mmc_host *host, bool enable);
int mmc_cmdq_halt_on_empty_queue(struct mmc_host *host);
void mmc_cmdq_post_req(struct mmc_host *host, int tag, int err);
int mmc_cmdq_start_req(struct mmc_host *host,
			      struct mmc_cmdq_req *cmdq_req);
int mmc_cmdq_prepare_flush(struct mmc_command *cmd);
int mmc_cmdq_wait_for_dcmd(struct mmc_host *host,
			struct mmc_cmdq_req *cmdq_req);
int mmc_cmdq_erase(struct mmc_cmdq_req *cmdq_req,
	      struct mmc_card *card, unsigned int from, unsigned int nr,
	      unsigned int arg);
void mmc_cmdq_up_rwsem(struct mmc_host *host);
int mmc_cmdq_down_rwsem(struct mmc_host *host, struct request *rq);
#endif

struct mmc_async_req *mmc_start_areq(struct mmc_host *host,
				     struct mmc_async_req *areq,
				     enum mmc_blk_status *ret_stat);

int mmc_erase(struct mmc_card *card, unsigned int from, unsigned int nr,
		unsigned int arg);
int mmc_can_erase(struct mmc_card *card);
int mmc_can_trim(struct mmc_card *card);
int mmc_can_discard(struct mmc_card *card);
int mmc_can_sanitize(struct mmc_card *card);
int mmc_can_secure_erase_trim(struct mmc_card *card);
int mmc_erase_group_aligned(struct mmc_card *card, unsigned int from,
			unsigned int nr);
unsigned int mmc_calc_max_discard(struct mmc_card *card);

int mmc_set_blocklen(struct mmc_card *card, unsigned int blocklen);
int mmc_set_blockcount(struct mmc_card *card, unsigned int blockcount,
			bool is_rel_write);

int __mmc_claim_host(struct mmc_host *host, atomic_t *abort);
void mmc_release_host(struct mmc_host *host);
void mmc_get_card(struct mmc_card *card);
void mmc_put_card(struct mmc_card *card);
int mmc_try_claim_host(struct mmc_host *host, unsigned int delay);

#if defined(CONFIG_MMC_FFU)
extern int mmc_reinit_oldcard(struct mmc_host *host);
#endif

#ifdef CONFIG_MTK_EMMC_HW_CQ
void mmc_blk_cmdq_req_done(struct mmc_request *mrq);
#endif
extern int mmc_blk_cmdq_switch(struct mmc_card *card, int enable);
/**
 *	mmc_claim_host - exclusively claim a host
 *	@host: mmc host to claim
 *
 *	Claim a host for a set of operations.
 */
static inline void mmc_claim_host(struct mmc_host *host)
{
	__mmc_claim_host(host, NULL);
}

#endif
