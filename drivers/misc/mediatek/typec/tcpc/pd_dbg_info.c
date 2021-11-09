// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include "inc/pd_dbg_info.h"

#if IS_ENABLED(CONFIG_PD_DBG_INFO)

#define PD_INFO_BUF_SIZE	(2048*256)
#define MSG_POLLING_MS		20

#define OUT_BUF_MAX (128)
static struct {
	int used;
	int cnt;
	char buf[PD_INFO_BUF_SIZE + 1 + OUT_BUF_MAX];
} pd_dbg_buffer[2];

static bool dbg_log_en;
module_param(dbg_log_en, bool, 0644);

static struct mutex buff_lock;
static unsigned int using_buf;
static bool event_loop_thread_stop;
static wait_queue_head_t event_loop_wait_que;
static atomic_t busy = ATOMIC_INIT(0);
static atomic_t pending_event = ATOMIC_INIT(0);

void pd_dbg_info_lock(void)
{
	atomic_inc(&busy);
}
EXPORT_SYMBOL(pd_dbg_info_lock);

void pd_dbg_info_unlock(void)
{
	atomic_dec_if_positive(&busy);
}
EXPORT_SYMBOL(pd_dbg_info_unlock);

static inline bool pd_dbg_print_out(void)
{
	int used, cnt;
	unsigned int index, i;
	char *str;

	mutex_lock(&buff_lock);
	index = using_buf;
	using_buf ^= 0x01; /* exchange buffer */
	mutex_unlock(&buff_lock);

	used = pd_dbg_buffer[index].used;

	if (used == 0)
		return false;
	cnt = pd_dbg_buffer[index].cnt;

	pr_info("///PD dbg info %d %d\n", used, cnt);

	str = pd_dbg_buffer[index].buf;
	for (i = 0; i < cnt; i++) {
		while (atomic_read(&busy))
			usleep_range(1000, 2000);

		pr_info("%s", str);
		str += strlen(str) + 1;
	}

	/* pr_info("PD dbg info///\n"); */
	pd_dbg_buffer[index].used = 0;
	pd_dbg_buffer[index].cnt = 0;
	msleep(MSG_POLLING_MS);
	return true;
}

static int print_out_thread_fn(void *arg)
{
	int ret = 0;
	while (true) {
		ret = wait_event_interruptible(event_loop_wait_que,
					       atomic_read(&pending_event) |
					       event_loop_thread_stop);
		if (kthread_should_stop() || event_loop_thread_stop || ret) {
			pr_notice("%s exits(%d)\n", __func__, ret);
			break;
		}
		do {
			atomic_dec_if_positive(&pending_event);
		} while (pd_dbg_print_out());
	}

	return 0;
}

int pd_dbg_info(const char *fmt, ...)
{
	unsigned int index;
	va_list args;
	int r1, r2 = 0, used, left_size;
	u64 ts;
	unsigned long rem_usec;

	if (!dbg_log_en)
		return 0;

	ts = local_clock();
	rem_usec = do_div(ts, 1000000000) / 1000 / 1000;
	va_start(args, fmt);
	mutex_lock(&buff_lock);
	index = using_buf;
	used = pd_dbg_buffer[index].used;
	left_size = PD_INFO_BUF_SIZE - used;
	r1 = snprintf(pd_dbg_buffer[index].buf + used, left_size, "<%5lu.%03lu>",
		(unsigned long)ts, rem_usec);
	if (r1 <= 0 || r1 == left_size)
		goto out;
	left_size = PD_INFO_BUF_SIZE - (used + r1);
	r2 = vsnprintf(pd_dbg_buffer[index].buf + used + r1, left_size, fmt, args);
	if (r2 <= 0 || r2 == left_size)
		goto out;
	used += r1 + r2 + 1;

	if (pd_dbg_buffer[index].used == 0) {
		atomic_inc(&pending_event);
		wake_up_interruptible(&event_loop_wait_que);
	}

	pd_dbg_buffer[index].used = used;
	pd_dbg_buffer[index].cnt++;
out:
	mutex_unlock(&buff_lock);
	va_end(args);
	return r2;
}
EXPORT_SYMBOL(pd_dbg_info);

static struct task_struct *print_out_tsk;

int pd_dbg_info_init(void)
{
	pr_info("%s\n", __func__);
	mutex_init(&buff_lock);
	print_out_tsk = kthread_create(
			print_out_thread_fn, NULL, "pd_dbg_info");
	init_waitqueue_head(&event_loop_wait_que);
	atomic_set(&pending_event, 0);
	wake_up_process(print_out_tsk);
	return 0;
}

void pd_dbg_info_exit(void)
{
	event_loop_thread_stop = true;
	wake_up_interruptible(&event_loop_wait_que);
	kthread_stop(print_out_tsk);
	mutex_destroy(&buff_lock);
}

subsys_initcall(pd_dbg_info_init);
module_exit(pd_dbg_info_exit);

MODULE_DESCRIPTION("PD Debug Info Module");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com>");
MODULE_LICENSE("GPL");

#endif	/* CONFIG_PD_DBG_INFO */
