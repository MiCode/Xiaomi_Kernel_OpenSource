/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

#include <mach/msm_iomap.h>

#include "wallclk.h"

#define WALLCLK_MODULE_NAME	"wallclk"
#define WALLCLK_MODULE_NAME_LEN	10

#define FLAG_WALLCLK_INITED		0x1
#define FLAG_WALLCLK_SFN_REF_SET	0x2
#define FLAG_WALLCLK_ENABLED		0x4

#define WALLCLK_TIMER_INTERVAL_NS	100000

#define WALLCLK_SHARED_MEM_SIZE		1024
#define ALIGN_64(addr)			(((addr) + 7) & ~0x7)

#define GPS_EPOCH_DIFF			315964800

struct wallclk_cnt {
	u32	pulse;
	u32	clk;
};

struct wallclk_reg {
	u32	ctrl;
	u32	pulse_cnt;
	u32	snapshot_clock_cnt;
	u32	clock_cnt;
	u32	__unused__[5];
	u32	base_time0;
	u32	base_time1;
};

struct wallclk_sm {
	struct wallclk_reg	reg;
	u32			sfn_ref;
};

struct wallclk_cfg {
	u32	ppns;
	u32	clk_rate;
	u32	clk_rate_v;	/* clk_rate = clk_rate_v x clk_rate_p */
	u32	clk_rate_p;	/* power of 10 */
	u32	ns_per_clk_rate_v;
};

struct wallclk {
	struct wallclk_sm	*shm;

	struct wallclk_cfg	cfg;

	struct timespec		tv;

	struct hrtimer		timer;
	ktime_t			interval;

	spinlock_t		lock;
	u32			flags;

	char			name[WALLCLK_MODULE_NAME_LEN];
};

static struct wallclk wall_clk;

static inline int is_valid_register(u32 offset)
{
	int rc = 0;

	switch (offset) {
	case CTRL_REG_OFFSET:
	case PULSE_CNT_REG_OFFSET:
	case CLK_CNT_SNAPSHOT_REG_OFFSET:
	case CLK_CNT_REG_OFFSET:
	case CLK_BASE_TIME0_OFFSET:
	case CLK_BASE_TIME1_OFFSET:
		rc = 1;
		break;
	default:
		break;
	}
	return rc;
}

static inline void wallclk_ctrl_reg_set(struct wallclk *wclk, u32 v)
{
	struct wallclk_reg *reg = &wclk->shm->reg;

	if (v & CTRL_ENABLE_MASK) {
		if (!(wclk->flags & FLAG_WALLCLK_ENABLED)) {
			getnstimeofday(&wclk->tv);
			__raw_writel(0, &reg->snapshot_clock_cnt);
			__raw_writel(0, &reg->clock_cnt);
			__raw_writel(0, &reg->pulse_cnt);
			hrtimer_start(&wclk->timer,
				      wclk->interval,
				      HRTIMER_MODE_REL);
			wclk->flags |= FLAG_WALLCLK_ENABLED;
		}
	} else {
		if (wclk->flags & FLAG_WALLCLK_ENABLED) {
			hrtimer_cancel(&wclk->timer);
			wclk->flags &= ~FLAG_WALLCLK_ENABLED;
		}
	}

	__raw_writel(v, &reg->ctrl);
}

static inline void wallclk_cfg_init(struct wallclk_cfg *cfg,
				    u32 ppns,
				    u32 clk_rate)
{
	cfg->ppns = ppns;
	cfg->clk_rate = clk_rate;
	cfg->clk_rate_v = clk_rate;
	cfg->clk_rate_p = 1;
	cfg->ns_per_clk_rate_v = 1000000000;

	while (!(cfg->clk_rate_v % 10)) {
		cfg->clk_rate_v /= 10;
		cfg->clk_rate_p *= 10;
		cfg->ns_per_clk_rate_v /= 10;
	}
}

static inline struct timespec timestamp_convert(const struct timespec *tv)
{
	struct timespec rc;

	rc.tv_sec = tv->tv_sec - GPS_EPOCH_DIFF;
	rc.tv_nsec = tv->tv_nsec;

	return rc;
}

static inline void timespec_delta_to_wclk_cnt(const struct timespec *tv,
					      const struct wallclk_cfg *cfg,
					      struct wallclk_cnt *wclk_cnt)
{
	long ns;

	wclk_cnt->pulse = tv->tv_sec / cfg->ppns;
	wclk_cnt->clk = (tv->tv_sec % cfg->ppns) * cfg->clk_rate;

	ns = tv->tv_nsec;
	while (ns >= cfg->ns_per_clk_rate_v) {
		ns -= cfg->ns_per_clk_rate_v;
		wclk_cnt->clk += cfg->clk_rate_v;
	}

	wclk_cnt->clk += (ns * cfg->clk_rate_v)/cfg->ns_per_clk_rate_v;
}

static inline u32 wallclk_cnt_to_sfn(const struct wallclk_cnt *cnt,
				     const struct wallclk_cfg *cfg)
{
	u32 sfn;
	u32 delta, p;

	sfn = SFN_PER_SECOND * cnt->pulse * cfg->ppns;
	if (cfg->clk_rate_p > 100) {
		p = cfg->clk_rate_p/100;
		delta = cnt->clk/(cfg->clk_rate_v * p);
	} else {
		p = 100/cfg->clk_rate_p;
		delta = (cnt->clk * p)/cfg->clk_rate_v;
	}
	sfn += delta;

	return sfn;
}

static void update_wallclk(struct wallclk *wclk)
{
	struct timespec tv;
	struct timespec delta_tv;
	struct wallclk_cnt cnt;
	struct wallclk_reg *reg = &wclk->shm->reg;

	spin_lock(&wclk->lock);
	getnstimeofday(&tv);
	delta_tv = timespec_sub(tv, wclk->tv);
	timespec_delta_to_wclk_cnt(&delta_tv, &wclk->cfg, &cnt);
	__raw_writel(cnt.pulse, &reg->pulse_cnt);
	__raw_writel(cnt.clk, &reg->clock_cnt);
	__raw_writel(cnt.clk, &reg->snapshot_clock_cnt);

	spin_unlock(&wclk->lock);
}

static int set_sfn(struct wallclk *wclk, u16 sfn)
{
	int rc = 0;
	struct wallclk_reg *reg = &wclk->shm->reg;
	u32 v;
	struct timespec ts;

	if (sfn > MAX_SFN) {
		rc = -EINVAL;
		goto out;
	}

	if (!(wclk->flags & FLAG_WALLCLK_INITED)) {
		rc = -EIO;
		goto out;
	}

	spin_lock_bh(&wclk->lock);

	v = __raw_readl(&reg->ctrl);
	wallclk_ctrl_reg_set(wclk, v & ~CTRL_ENABLE_MASK);

	getnstimeofday(&wclk->tv);
	ts = timestamp_convert(&wclk->tv);
	__raw_writel(ts.tv_sec, &reg->base_time0);
	__raw_writel(ts.tv_nsec, &reg->base_time1);

	wclk->shm->sfn_ref = sfn;
	wclk->flags |= FLAG_WALLCLK_SFN_REF_SET;

	__raw_writel(0, &reg->pulse_cnt);
	__raw_writel(0, &reg->clock_cnt);
	__raw_writel(0, &reg->snapshot_clock_cnt);
	hrtimer_start(&wclk->timer, wclk->interval, HRTIMER_MODE_REL);
	wclk->flags |= FLAG_WALLCLK_ENABLED;
	__raw_writel(v | CTRL_ENABLE_MASK, &reg->ctrl);

	spin_unlock_bh(&wclk->lock);

out:
	return rc;
}

static int get_sfn(struct wallclk *wclk)
{
	struct wallclk_cnt cnt;
	int rc = 0;
	u32 sfn;

	if (!(wclk->flags & FLAG_WALLCLK_INITED)) {
		rc = -EIO;
		goto out;
	}

	spin_lock_bh(&wclk->lock);

	if (!(wclk->flags & FLAG_WALLCLK_ENABLED) ||
	    !(wclk->flags & FLAG_WALLCLK_SFN_REF_SET)) {
		rc = -EIO;
		goto unlock;
	}

	cnt.pulse = __raw_readl(&(wclk->shm->reg.pulse_cnt));
	cnt.clk = __raw_readl(&(wclk->shm->reg.clock_cnt));
	sfn = wallclk_cnt_to_sfn(&cnt, &wclk->cfg);

	sfn += wclk->shm->sfn_ref;
	rc = sfn & MAX_SFN;

unlock:
	spin_unlock_bh(&wclk->lock);
out:
	return rc;
}

enum hrtimer_restart wallclk_timer_cb(struct hrtimer *timer)
{
	update_wallclk(&wall_clk);
	hrtimer_forward_now(timer, wall_clk.interval);
	return HRTIMER_RESTART;
}

int wallclk_set_sfn(u16 sfn)
{
	return set_sfn(&wall_clk, sfn);
}
EXPORT_SYMBOL_GPL(wallclk_set_sfn);

int wallclk_get_sfn(void)
{
	return get_sfn(&wall_clk);
}
EXPORT_SYMBOL_GPL(wallclk_get_sfn);

int wallclk_set_sfn_ref(u16 sfn)
{
	int rc = 0;

	if (sfn > MAX_SFN) {
		rc = -EINVAL;
		goto out;
	}

	if (!(wall_clk.flags & FLAG_WALLCLK_INITED)) {
		rc = -EIO;
		goto out;
	}

	spin_lock_bh(&wall_clk.lock);

	wall_clk.shm->sfn_ref = sfn;
	wall_clk.flags |= FLAG_WALLCLK_SFN_REF_SET;

	spin_unlock_bh(&wall_clk.lock);

out:
	return rc;
}
EXPORT_SYMBOL_GPL(wallclk_set_sfn_ref);

int wallclk_get_sfn_ref(void)
{
	int rc = 0;

	if (!(wall_clk.flags & FLAG_WALLCLK_INITED)) {
		rc = -EIO;
		goto out;
	}

	spin_lock_bh(&wall_clk.lock);

	if (!(wall_clk.flags & FLAG_WALLCLK_SFN_REF_SET)) {
		rc = -EAGAIN;
		goto unlock;
	}
	rc = wall_clk.shm->sfn_ref;

unlock:
	spin_unlock_bh(&wall_clk.lock);
out:
	return rc;
}
EXPORT_SYMBOL_GPL(wallclk_get_sfn_ref);

int wallclk_reg_read(u32 offset, u32 *p)
{
	int rc = 0;

	if (!(wall_clk.flags & FLAG_WALLCLK_INITED)) {
		rc = -EIO;
		goto out;
	}

	if (!is_valid_register(offset)) {
		rc = -EINVAL;
		goto out;
	}

	spin_lock_bh(&wall_clk.lock);
	*p = __raw_readl((char *)&wall_clk.shm->reg + offset);
	spin_unlock_bh(&wall_clk.lock);
out:
	return rc;
}
EXPORT_SYMBOL_GPL(wallclk_reg_read);

int wallclk_reg_write(u32 offset, u32 val)
{
	int rc = 0;
	char *p;

	if (!(wall_clk.flags & FLAG_WALLCLK_INITED)) {
		rc = -EIO;
		goto out;
	}

	p = (char *)&wall_clk.shm->reg;

	spin_lock_bh(&wall_clk.lock);
	switch (offset) {
	case CTRL_REG_OFFSET:
		wallclk_ctrl_reg_set(&wall_clk, val);
		break;
	case PULSE_CNT_REG_OFFSET:
	case CLK_BASE_TIME0_OFFSET:
	case CLK_BASE_TIME1_OFFSET:
		__raw_writel(val, p + offset);
		break;
	case CLK_CNT_REG_OFFSET:
		__raw_writel(val, p + CLK_CNT_REG_OFFSET);
		__raw_writel(val, p + CLK_CNT_SNAPSHOT_REG_OFFSET);
		break;
	case CLK_CNT_SNAPSHOT_REG_OFFSET:
		rc = -EIO;
		break;
	default:
		rc = -EINVAL;
		break;
	}

	spin_unlock_bh(&wall_clk.lock);
out:
	return rc;
}
EXPORT_SYMBOL_GPL(wallclk_reg_write);

static int __init wallclk_init(void)
{
	int rc = 0;
	u32 addr;

	memset(&wall_clk, 0, sizeof(wall_clk));

	addr = (u32)MSM_SHARED_RAM_BASE + MSM_SHARED_RAM_SIZE -
		WALLCLK_SHARED_MEM_SIZE;
	wall_clk.shm = (struct wallclk_sm *)ALIGN_64(addr);

	__raw_writel(0, &(wall_clk.shm->reg.ctrl));
	__raw_writel(0, &(wall_clk.shm->reg.pulse_cnt));
	__raw_writel(0, &(wall_clk.shm->reg.snapshot_clock_cnt));
	__raw_writel(0, &(wall_clk.shm->reg.clock_cnt));
	__raw_writel(0, &(wall_clk.shm->reg.clock_cnt));
	__raw_writel(0, &(wall_clk.shm->reg.base_time0));
	__raw_writel(0, &(wall_clk.shm->reg.base_time1));

	wall_clk.shm->sfn_ref = 0;

	wallclk_cfg_init(&wall_clk.cfg, PPNS_PULSE, CLK_RATE);

	strlcpy(wall_clk.name, WALLCLK_MODULE_NAME, WALLCLK_MODULE_NAME_LEN);

	hrtimer_init(&wall_clk.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	wall_clk.timer.function = wallclk_timer_cb;
	wall_clk.interval = ns_to_ktime(WALLCLK_TIMER_INTERVAL_NS);
	spin_lock_init(&wall_clk.lock);

	wall_clk.flags |= FLAG_WALLCLK_INITED;

	printk(KERN_INFO "%s: clk_rate=%u ppns=%u clk_reg_addr=0x%x\n",
	       wall_clk.name, wall_clk.cfg.clk_rate, wall_clk.cfg.ppns,
	       (int)(&wall_clk.shm->reg));
	return rc;
}

static void __exit wallclk_exit(void)
{
	if (wall_clk.flags & FLAG_WALLCLK_INITED) {
		spin_lock_bh(&wall_clk.lock);
		wallclk_ctrl_reg_set(&wall_clk, 0);
		wall_clk.flags = 0;
		spin_unlock_bh(&wall_clk.lock);
	}
}

module_init(wallclk_init);
module_exit(wallclk_exit);

MODULE_DESCRIPTION("Wall clock");
MODULE_LICENSE("GPL v2");
