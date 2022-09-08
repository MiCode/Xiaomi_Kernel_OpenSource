// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/sched/clock.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_helper.h"
#include "sspm_ipi.h"
#include "sspm_reservedmem.h"
#include "sspm_reservedmem_define.h"
#include "sspm_sysfs.h"
#include "sspm_timesync.h"

/* #define TINYSYS_TIME_TESTING */

struct timesync_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int ts_h;
	unsigned int ts_l;
	unsigned int clk_h;
	unsigned int clk_l;
};

static struct timesync_ctrl_s *ts_ctl;
static unsigned int sspm_ts_inited;
static struct sspm_work_struct sspm_timesync_work;
static struct timer_list sspm_timesync_timer;
static DEFINE_MUTEX(sspm_timesync_mutex);
#if defined(TINYSYS_TIME_TESTING) && defined(DEBUG)
static unsigned int sspm_sync_cnt;
#endif

static void sspm_timesync_timestamp(unsigned long long src, unsigned int *ts_h,
	unsigned int *ts_l)
{
	*ts_l = (unsigned int)(src & 0x00000000FFFFFFFF);
	*ts_h = (unsigned int)((src & 0xFFFFFFFF00000000) >> 32);
}

void sspm_timesync_ts_get(unsigned int *ts_h, unsigned int *ts_l)
{
	unsigned long long ap_ts;

	ap_ts = sched_clock();

	sspm_timesync_timestamp(ap_ts, ts_h, ts_l);
}

void sspm_timesync_clk_get(unsigned int *clk_h, unsigned int *clk_l)
{
	unsigned long long ap_clk;

	ap_clk = mtk_timer_src_count();

	sspm_timesync_timestamp(ap_clk, clk_h, clk_l);
}

static void __tinysys_time_sync(int mode)
{
	struct plt_ipi_data_s ipi_data;
	int ret, ackdata;

	if (sspm_ts_inited) {
		memset((void *)&ipi_data, 0, sizeof(ipi_data));

		mutex_lock(&sspm_timesync_mutex);

		ipi_data.cmd = PLT_TIMESYNC_SYNC;

#if defined(TINYSYS_TIME_TESTING) && defined(DEBUG)
		if (mode == 1)
			ipi_data.u.ts.mode = mode;
#endif
		/* inject timestamp and clk */
		sspm_timesync_ts_get(&ts_ctl->ts_h, &ts_ctl->ts_l);
		sspm_timesync_clk_get(&ts_ctl->clk_h, &ts_ctl->clk_l);

		ret = sspm_ipi_send_sync(IPI_ID_PLATFORM, IPI_OPT_POLLING,
		    &ipi_data, sizeof(ipi_data) / SSPM_MBOX_SLOT_SIZE,
		    &ackdata, 1);
		if (ret != 0)
			pr_err("SSPM: logger IPI fail ret=%d\n", ret);

		mutex_unlock(&sspm_timesync_mutex);
	}
}

static void tinysys_time_sync(void)
{
	__tinysys_time_sync(0);
}

static void timesync_ws(struct work_struct *ws)
{
#if defined(TINYSYS_TIME_TESTING) && defined(DEBUG)
	pr_debug("resync time about %d sec (%d)\n", TIMESYNC_TIMEOUT,
		sspm_sync_cnt++);
#endif
	tinysys_time_sync();
}

static void sspm_ts_timeout(struct timer_list *unused)
{
	sspm_schedule_work(&sspm_timesync_work);

	sspm_timesync_timer.expires = jiffies + TIMESYNC_TIMEOUT;
	add_timer(&sspm_timesync_timer);
}

unsigned int __init sspm_timesync_init(phys_addr_t start, phys_addr_t limit)
{
	unsigned int last_ofs;

	last_ofs = 0;

	ts_ctl = (struct timesync_ctrl_s *) (uintptr_t)start;
	ts_ctl->base = PLT_TIMESYNC_SYNC; /* magic */
	ts_ctl->size = sizeof(*ts_ctl);

	sspm_timesync_ts_get(&ts_ctl->ts_h, &ts_ctl->ts_l);
	sspm_timesync_clk_get(&ts_ctl->clk_h, &ts_ctl->clk_l);

	last_ofs += ts_ctl->size;

	if (last_ofs >= limit) {
		pr_err("SSPM:%s() initial fail, last_ofs=%u, limit=%u\n",
			__func__, last_ofs, (unsigned int) limit);
		goto error;
	}

	sspm_ts_inited = 1;
	return last_ofs;

error:
	sspm_ts_inited = 0;
	ts_ctl = NULL;
	return 0;
}

int __init sspm_timesync_init_done(void)
{
#if defined(TINYSYS_TIME_TESTING) && defined(DEBUG)
	int ret;
#endif

	tinysys_time_sync();

	INIT_WORK(&sspm_timesync_work.work, timesync_ws);
	timer_setup(&sspm_timesync_timer, &sspm_ts_timeout, 0);
	sspm_timesync_timer.expires = jiffies + TIMESYNC_TIMEOUT;
	add_timer(&sspm_timesync_timer);

#if defined(TINYSYS_TIME_TESTING) && defined(DEBUG)
	ret = sspm_sysfs_create_file(&dev_attr_sspm_time_sync);

	if (unlikely(ret != 0))
		pr_err("[SSPM]: %s create file fail\n", __func__);
#endif

	return 0;
}
