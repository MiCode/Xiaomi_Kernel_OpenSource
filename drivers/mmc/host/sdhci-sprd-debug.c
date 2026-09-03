// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2022 Spreadtrum, Inc.
// Author: Wei Zheng <wei.zheng@unisoc.com>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/cdev.h>
#include <linux/ktime.h>
#include <linux/reboot.h>
#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/iopoll.h>

#include <trace/hooks/mmc.h>

#include "../core/core.h"
#include "../core/mmc_ops.h"
#include "../core/card.h"
#include "../core/queue.h"
#include "sdhci.h"
#include "mmc_swcq.h"
#include <linux/mmc/sdio_func.h>

#define mmc_card_clr_removed(c) ((c)->state &= ~MMC_CARD_REMOVED)

#define ANOMALY_WARNING_LEVEL 3
#define ANOMALY_REMOVED_LEVEL 4
#define HOT_PLUG_MAX_TIMES 5

#define SD_SPEED_LIMIT 1
#define SD_VALID_CALC_CNT 100

#define MMC_ARRAY_SIZE 14	/* 2^(14-2) = 4096ms/blocks */
#define MMC_SPEED_0M 0
#define MMC_SPEED_1M 100

u32 sd_hotplug_cnt;
EXPORT_SYMBOL(sd_hotplug_cnt);

bool anomaly_sd_remove_flag;
EXPORT_SYMBOL(anomaly_sd_remove_flag);

/* Store anomaly information of mmc every 10 seconds */
struct sd_anomaly_info {
	int rd_err_times; /* durations(us) of data read timeout error occurrance */
	int wr_err_times; /* durations(us) of data write timeout error occurrance */
};

/*
 * convert ms to index: (ilog2(ms) + 1)
 * array[6]++ means the time is: 32ms <= time < 64ms
 * [0] [1] [2] [3] [4] [5]  [6]  [7]  [8]   [9]   [10]  [11]   [12]   [13]
 * 0ms 1ms 2ms 4ms 8ms 16ms 32ms 64ms 128ms 256ms 512ms 1024ms 2048ms 4096ms
 *
 * convert block_length to index: (ilog2(block_length) + 1)
 * array[6]++ means the block_length  is: 32blocks <= block_length < 64blocks
 * [0]     [1]      [2]      [3]      [4]      [5]       [6]       [7]
 * 0block  1blocks  2blocks  4blocks  8blocks  16blocks  32blocks  64blocks
 * [8]        [9]        [10]       [11]        [12]        [13]
 * 128blocks  256blocks  512blocks  1024blocks  2048blocks  4096blocks
 */
struct mmc_debug_info {
	u32 cmd;
	u32 arg;
	u32 blocks;
	u32 intmask;
	u64 read_total_blocks;
	u64 write_total_blocks;
	struct mmc_request *mrq;
	struct sd_anomaly_info ano_info;
	ktime_t cnt_time;
	ktime_t start_time;
	ktime_t end_time;
	unsigned long read_total_time;
	unsigned long write_total_time;
	unsigned long cmd_2_end[MMC_ARRAY_SIZE];
	unsigned long data_2_end[MMC_ARRAY_SIZE];
	unsigned long block_len[MMC_ARRAY_SIZE];
};

struct mmc_debug_timer {
	ktime_t cnt_time;
	struct sdhci_host *host;
	struct timer_list debug_timer;	/* Timer for debug data */
};

struct mmc_debug_worker {
	struct sdhci_host *host;
	struct mmc_debug_info info;
	spinlock_t lock;
	struct work_struct print_work;
};

#define rq_log(array, fmt, ...) \
	pr_err(fmt ":%5ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld\n", \
		##__VA_ARGS__, array[0], array[1], array[2], array[3], \
		array[4], array[5], array[6], array[7], array[8], \
		array[9], array[10], array[11], array[12], array[13])

static struct mmc_debug_info mmc_debug[3];
static struct mmc_debug_timer mmc_timer[3];
static struct mmc_debug_worker mmc_worker[3];

static int is_rw_cmd(struct sdhci_host *host, u32 cmd, u32 arg)
{
	/* judge sdio read/write cmd type */
	if ((host->mmc->index == 2) && (cmd == SD_IO_RW_DIRECT || cmd == SD_IO_RW_EXTENDED))
		cmd = arg >> 31 ? MMC_EXECUTE_WRITE_TASK : MMC_EXECUTE_READ_TASK;

	/* READ return 0, WRITE return 1, others return -EINVAL */
	if (cmd == MMC_READ_MULTIPLE_BLOCK || cmd == MMC_EXECUTE_READ_TASK ||
		cmd == MMC_READ_SINGLE_BLOCK)
		return 0;
	else if (cmd == MMC_WRITE_BLOCK || cmd == MMC_EXECUTE_WRITE_TASK ||
		cmd == MMC_WRITE_MULTIPLE_BLOCK)
		return 1;
	else
		return -EINVAL;
}

static void mmc_debug_is_emmc(struct sdhci_host *host, struct mmc_debug_info *info)
{
	bool flag = true;

	if (HOST_IS_EMMC_TYPE(host->mmc) && info->mrq &&
		host->mmc->cqe_ops->cqe_timeout)
		host->mmc->cqe_ops->cqe_timeout(host->mmc, info->mrq, &flag);
}

static void mmc_debug_info_print(struct sdhci_host *host, struct mmc_debug_info *info)
{
	const char *name = mmc_hostname(host->mmc);

	pr_err("|__time%8s: r= %ldus, w= %ldus\n",
		name, info->read_total_time, info->write_total_time);
	pr_err("|__timeout%5s: r= %dus, w= %dus\n",
		name, info->ano_info.rd_err_times, info->ano_info.wr_err_times);
}

static int mmc_schedule_delayed_work(struct delayed_work *work, unsigned long delay)
{
	/*
	 * We use the system_freezable_wq, because of two reasons.
	 * First, it allows several works (not the same work item) to be
	 * executed simultaneously. Second, the queue becomes frozen when
	 * userspace becomes frozen during system PM.
	 */
	return queue_delayed_work(system_freezable_wq, work, delay);
}

static void sprd_mmc_power_off(struct mmc_host *host)
{
	if (host->ios.power_mode == MMC_POWER_OFF)
		return;

	host->ios.clock = 0;
	host->ios.vdd = 0;
	host->ios.power_mode = MMC_POWER_OFF;
	host->ios.chip_select = MMC_CS_DONTCARE;
	host->ios.bus_mode = MMC_BUSMODE_PUSHPULL;
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;
	host->ios.drv_type = 0;
	host->ios.enhanced_strobe = false;

	host->ops->set_ios(host, &(host->ios));
}

static void __sprd_mmc_stop_host(struct mmc_host *host)
{
	host->rescan_disable = 1;
	cancel_delayed_work_sync(&host->detect);
}

static void sprd_mmc_stop_host(struct mmc_host *host)
{
	__sprd_mmc_stop_host(host);

	/* clear pm flags now and let card drivers set them as needed */
	host->pm_flags = 0;

	if (host->bus_ops) {
		pr_err("%s: %s\n", mmc_hostname(host), __func__);
		host->bus_ops->remove(host);
		mmc_claim_host(host);
		host->bus_ops = NULL;
		sprd_mmc_power_off(host);
		mmc_release_host(host);
		return;
	}

	mmc_claim_host(host);
	sprd_mmc_power_off(host);
	mmc_release_host(host);
}

static void mmc_remove_anomaly_sd(struct mmc_host *host)
{
	sprd_mmc_stop_host(host);
}

static inline int mmc_tot_in_flight_sd(struct mmc_queue *mq)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&mq->lock, flags);
	ret = mq->in_flight[MMC_ISSUE_SYNC] + mq->in_flight[MMC_ISSUE_DCMD] +
		mq->in_flight[MMC_ISSUE_ASYNC];
	if (!ret)
		mq->busy = true;
	spin_unlock_irqrestore(&mq->lock, flags);

	return ret;
}

static void sd_anomaly_check_handle(struct mmc_debug_info *info,
	struct sdhci_host *host, int sd_anomaly_level)
{
	struct mmc_blk_data *md = dev_get_drvdata(&host->mmc->card->dev);
	struct mmc_queue *mq = &md->queue;
	int in_flight;
	const char *name = mmc_hostname(host->mmc);

	if (sd_anomaly_level >= ANOMALY_REMOVED_LEVEL && host->mmc->card) {
		if (sd_hotplug_cnt < HOT_PLUG_MAX_TIMES) {
			//SD HOT PLUG
			sd_hotplug_cnt++;
			pr_info("%s: sd_hotplug_cnt %d\n", name, sd_hotplug_cnt);
			mmc_card_set_removed(host->mmc->card);
			//detect sd card
			mmc_schedule_delayed_work(&(host->mmc->detect), 0);
		} else {
			mmc_card_set_removed(host->mmc->card);

			if (!read_poll_timeout(mmc_tot_in_flight_sd, in_flight, (in_flight == 0),
					10 * USEC_PER_MSEC, 2 * USEC_PER_SEC, false, mq)) {
				if (host->mmc->card) {
					sd_hotplug_cnt = 0;
					//remove sd card
					mmc_remove_anomaly_sd(host->mmc);
					anomaly_sd_remove_flag = true;
					pr_err("%s: remove anomaly sd card\n", name);
				}
			} else {
				if (host->mmc->card)
					mmc_card_clr_removed(host->mmc->card);
				pr_err("%s: sd in_flight not equal to 0, no remove\n", name);
			}
		}
	}
}

/* anomaly check for T-Cards */
static void sd_anomaly_check(struct mmc_debug_info *info, struct sdhci_host *host,
	u64 rspeed, u64 wspeed)
{
	char ecode[20] = "ERRCODE=";
	int anomaly_durations;
	u64 total_time;
	int level = 0; /* anomaly level */
	u32 polling_time = sdhci_sprd_mmc_update_polling_times() * 1000000; /*us*/

	/* The anomaly level increase for each condition matched */
	/* ANOMALY 1: DATA TIME OUT ERR */
	anomaly_durations = info->ano_info.rd_err_times + info->ano_info.wr_err_times;
	if (anomaly_durations && anomaly_durations < polling_time / 2) {
		level += 1;
		strcat(ecode, "1");
	} else if (anomaly_durations >= polling_time / 2) {
		level += 2;
		strcat(ecode, "2");
	} else
		strcat(ecode, "0");

	/* ANOMALY 2: DATA RSP OVER 0.5S */

	/* ANOMALY 3: READ SPEED BELOW SPEED_LIMIT */
	if ((info->read_total_blocks > SD_VALID_CALC_CNT || info->ano_info.rd_err_times)
			&& rspeed < SD_SPEED_LIMIT) {
		level += 1;
		strcat(ecode, "1");
	} else
		strcat(ecode, "0");

	/* ANOMALY 4: WRIET SPEED BELOW SPEED_LIMIT */
	if ((info->write_total_blocks > SD_VALID_CALC_CNT || info->ano_info.wr_err_times)
			&& wspeed < SD_SPEED_LIMIT) {
		level += 1;
		strcat(ecode, "1");
	} else
		strcat(ecode, "0");

	/* ANOMALY 5: HEAVY LOAD */
	total_time = info->read_total_time + info->write_total_time;
	if (total_time >= polling_time * 9 / 10) {
		level += 1;
		strcat(ecode, "1");
	} else
		strcat(ecode, "0");

	if (sdhci_sprd_mmc_update_sd_anomaly())
		level = sdhci_sprd_mmc_update_sd_anomaly();

	if (level >= ANOMALY_WARNING_LEVEL) {
		//show sd anomaly check result
		mmc_debug_info_print(host, info);
		pr_info("%s: identify anomaly level %d, %s\n",
			mmc_hostname(host->mmc), level, ecode);
		pr_err("%s: manufacturing date = %d-%d\n", mmc_hostname(host->mmc),
			host->mmc->card->cid.year, host->mmc->card->cid.month);
		sd_anomaly_check_handle(info, host, level);
	}
}

static void mmc_debug_print(struct mmc_debug_info *info, struct sdhci_host *host)
{
	u64 read_speed = 0, write_speed = 0;
	u64 wspeed_mod = 0, rspeed_mod = 0;
	const char *name = mmc_hostname(host->mmc);

	/* calculate read/write speed */
	if (info->read_total_time) {
		read_speed = info->read_total_blocks * 48828;
		do_div(read_speed, info->read_total_time);
	}
	if (info->write_total_time) {
		write_speed = info->write_total_blocks * 48828;
		do_div(write_speed, info->write_total_time);
	}

	/* print debug messages of mmc io */
	rq_log(info->cmd_2_end, "|__c2e%9s", name);
	rq_log(info->data_2_end, "|__d2e%9s", name);
	rq_log(info->block_len, "|__blocks%6s", name);
	rspeed_mod = do_div(read_speed, 100);
	wspeed_mod = do_div(write_speed, 100);
	pr_err("|__speed%7s: r= %lld.%lldM/s, w= %lld.%lldM/s, r_blk= %lld, w_blk= %lld\n",
		name, read_speed, rspeed_mod, write_speed, wspeed_mod,
		info->read_total_blocks, info->write_total_blocks);

	if (HOST_IS_EMMC_TYPE(host->mmc))
		sdhci_sprd_mmc_update_throughput(host, read_speed, write_speed,
			info->read_total_blocks, info->write_total_blocks);
	else if (HOST_IS_SD_TYPE(host->mmc) && host->mmc->card)
		sd_anomaly_check(info, host, read_speed, write_speed);
}

static void mmc_debug_print_handler(struct work_struct *work)
{
	struct mmc_debug_worker *wk = container_of(work, struct mmc_debug_worker, print_work);
	struct mmc_debug_info info_temp;
	unsigned long flags;

	spin_lock_irqsave(&wk->lock, flags);
	memcpy(&info_temp, &wk->info, sizeof(struct mmc_debug_info));
	spin_unlock_irqrestore(&wk->lock, flags);

	mmc_debug_print(&info_temp, wk->host);
}

static void mmc_debug_calc(struct sdhci_host *host, struct mmc_debug_info *info)
{
	int rw = is_rw_cmd(host, info->cmd, info->arg);

	/* record read/write info */
	if (rw == 0) {
		info->read_total_blocks += info->blocks;
		info->read_total_time += ktime_to_us(info->end_time - info->start_time);
	} else if (rw == 1) {
		info->write_total_blocks += info->blocks;
		info->write_total_time += ktime_to_us(info->end_time - info->start_time);
	}
}

static void mmc_debug_handle_rsp(struct sdhci_host *host, struct mmc_debug_info *info)
{
	u8 index;
	u32 msecs;
	u32 polling_times;
	struct mmc_debug_worker *work = NULL;
	unsigned long flags;
	const char *name = mmc_hostname(host->mmc);

	int rw = is_rw_cmd(host, info->cmd, info->arg);
	if (info->intmask & SDHCI_INT_RESPONSE) {
		/* cmd interrupt respond */
		info->end_time = ktime_get();
		msecs = ktime_to_ms(info->end_time - info->start_time);
		index = msecs > 0 ? min((MMC_ARRAY_SIZE - 1), ilog2(msecs) + 1) : 0;
		info->cmd_2_end[index]++;
		if (index >= 11) {
			pr_err("%s: cmd rsp is %d ms! cmd= %d blk= %d arg= %x mrq= %p rsp= %x\n",
				name, msecs, info->cmd, info->blocks, info->arg,
				info->mrq, sdhci_readl(host, SDHCI_RESPONSE));
			mmc_debug_is_emmc(host, info);
		}
	}

	if (info->intmask & SDHCI_INT_DATA_END || info->intmask & SDHCI_INT_DATA_TIMEOUT) {
		/* data interrupt respond */
		info->end_time = ktime_get();
		msecs = ktime_to_ms(info->end_time - info->start_time);
		index = info->blocks > 0 ? min((MMC_ARRAY_SIZE - 1), ilog2(info->blocks) + 1) : 0;
		info->block_len[index]++;
		index = msecs > 0 ? min((MMC_ARRAY_SIZE - 1), ilog2(msecs) + 1) : 0;
		info->data_2_end[index]++;
		if (info->intmask & SDHCI_INT_DATA_TIMEOUT) {
			info->blocks = 0;
			if (rw == 0)
				info->ano_info.rd_err_times += msecs * 1000;
			else if (rw == 1)
				info->ano_info.wr_err_times += msecs * 1000;
		} else if (index >= 11) {
			pr_err("%s: data rsp is %d ms! cmd= %d blk= %d arg= %x mrq= 0x%p\n",
				name, msecs, info->cmd, info->blocks, info->arg, info->mrq);
			mmc_debug_is_emmc(host, info);
		}
		mmc_debug_calc(host, info);
		info->start_time = 0;

		polling_times = sdhci_sprd_mmc_update_polling_times();
		if ((ktime_to_ms(ktime_get()) - info->cnt_time) > (polling_times * 1000ULL)) {
			work = &mmc_worker[host->mmc->index];

			spin_lock_irqsave(&work->lock, flags);
			memcpy(&work->info, info, sizeof(struct mmc_debug_info));
			spin_unlock_irqrestore(&work->lock, flags);

			/* clear mmc_debug structure */
			memset(info, 0, sizeof(struct mmc_debug_info));
			info->cnt_time = ktime_to_ms(ktime_get());

			schedule_work(&work->print_work);
		}
	}
}

void mmc_debug_update(struct sdhci_host *host, struct mmc_command *cmd, u32 intmask)
{
	struct mmc_debug_info *info = &mmc_debug[host->mmc->index];

	if (!intmask && cmd) {
		/* send cmd */
		info->cmd = cmd->opcode;
		info->arg = cmd->arg;
		info->mrq = cmd->mrq;
		info->blocks = cmd->mrq->data ? cmd->mrq->data->blocks : 0;
		info->start_time = ktime_get();

		return;
	}

	/* handle mmc interrupt */
	info->intmask = intmask;
	if ((!(intmask & SDHCI_INT_ERROR_MASK) || (intmask & SDHCI_INT_DATA_TIMEOUT))
	       && info->start_time)
		mmc_debug_handle_rsp(host, info);
}
EXPORT_SYMBOL(mmc_debug_update);

/* add mmc debug timer to check whether the hardware times out */
static void sdhci_timeout_debug_timer(struct timer_list *t)
{
	struct mmc_debug_timer *info = from_timer(info, t, debug_timer);
	struct sdhci_host *host = info->host;
	unsigned long flags;
	u32 intmask;

	spin_lock_irqsave(&host->lock, flags);

	intmask = sdhci_readl(host, SDHCI_INT_STATUS);
	pr_err("%s: waiting for interrupt over %lldms! (256ms timer) int_state = 0x%x\n",
		mmc_hostname(host->mmc), ktime_to_ms(ktime_get()) - info->cnt_time, intmask);
	sdhci_dumpregs(host);

	spin_unlock_irqrestore(&host->lock, flags);
}

void sdhci_sprd_mod_debug_timer(struct sdhci_host *host, unsigned long time)
{
	struct mmc_debug_timer *info = &mmc_timer[host->mmc->index];

	mod_timer(&info->debug_timer, time);
	info->cnt_time = ktime_to_ms(ktime_get());
}
EXPORT_SYMBOL(sdhci_sprd_mod_debug_timer);

void sdhci_sprd_del_debug_timer(struct sdhci_host *host)
{
	struct mmc_debug_timer *info = &mmc_timer[host->mmc->index];

	del_timer(&info->debug_timer);
}
EXPORT_SYMBOL(sdhci_sprd_del_debug_timer);

void sdhci_sprd_del_debug_timer_sync(struct sdhci_host *host)
{
	struct mmc_debug_timer *info = &mmc_timer[host->mmc->index];

	del_timer_sync(&info->debug_timer);
}
EXPORT_SYMBOL(sdhci_sprd_del_debug_timer_sync);

void sdhci_sprd_debug_timer_setup(struct sdhci_host *host)
{
	struct mmc_debug_timer *info = &mmc_timer[host->mmc->index];

	info->host = host;
	timer_setup(&info->debug_timer, sdhci_timeout_debug_timer, 0);
}
EXPORT_SYMBOL(sdhci_sprd_debug_timer_setup);

void sdhci_sprd_debug_init(struct sdhci_host *host)
{
	struct mmc_debug_worker *work = &mmc_worker[host->mmc->index];

	spin_lock_init(&work->lock);

	work->host = host;
	INIT_WORK(&work->print_work, mmc_debug_print_handler);
	sdhci_sprd_debug_timer_setup(host);
}
EXPORT_SYMBOL(sdhci_sprd_debug_init);
