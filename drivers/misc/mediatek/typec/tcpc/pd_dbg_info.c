// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#ifdef CONFIG_PD_DBG_INFO

#define PD_INFO_BUF_SIZE	(2048*256)
#define MSG_POLLING_MS		20

#define OUT_BUF_MAX (128)
static struct {
	int used;
	char buf[PD_INFO_BUF_SIZE + 1 + OUT_BUF_MAX];
} pd_dbg_buffer[2];

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

void pd_dbg_info_unlock(void)
{
	atomic_dec_if_positive(&busy);
}

static inline bool pd_dbg_print_out(void)
{
	char temp;
	int used;
	unsigned int index, i;

	mutex_lock(&buff_lock);
	index = using_buf;
	using_buf ^= 0x01; /* exchange buffer */
	mutex_unlock(&buff_lock);

	used = pd_dbg_buffer[index].used;

	if (used == 0)
		return false;

	pd_dbg_buffer[index].buf[used] = '\0';

	pr_info("///PD dbg info %ud\n", used);

	for (i = 0; i < used; i += OUT_BUF_MAX) {
		temp = pd_dbg_buffer[index].buf[OUT_BUF_MAX + i];
		pd_dbg_buffer[index].buf[OUT_BUF_MAX + i] = '\0';

		while (atomic_read(&busy))
			usleep_range(1000, 2000);

		pr_info("%s", pd_dbg_buffer[index].buf + i);
		pd_dbg_buffer[index].buf[OUT_BUF_MAX + i] = temp;
	}

	/* pr_info("PD dbg info///\n"); */
	pd_dbg_buffer[index].used = 0;
	msleep(MSG_POLLING_MS);
	return true;
}

static int print_out_thread_fn(void *arg)
{
	while (true) {
		wait_event_interruptible(event_loop_wait_que,
				atomic_read(&pending_event) |
				event_loop_thread_stop);
		if (kthread_should_stop() || event_loop_thread_stop)
			break;
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
	int r;
	int used;
	u64 ts;
	unsigned long rem_usec;

	ts = local_clock();
	rem_usec = do_div(ts, 1000000000) / 1000 / 1000;
	va_start(args, fmt);
	mutex_lock(&buff_lock);
	index = using_buf;
	used = pd_dbg_buffer[index].used;
	r = snprintf(pd_dbg_buffer[index].buf + used,
		PD_INFO_BUF_SIZE - used, "<%5lu.%03lu>",
		(unsigned long)ts, rem_usec);
	if (r > 0)
		used += r;
	r = vsnprintf(pd_dbg_buffer[index].buf + used,
			PD_INFO_BUF_SIZE - used, fmt, args);
	if (r > 0)
		used += r;

	if (pd_dbg_buffer[index].used == 0) {
		atomic_inc(&pending_event);
		wake_up_interruptible(&event_loop_wait_que);
	}

	pd_dbg_buffer[index].used = used;
	mutex_unlock(&buff_lock);
	va_end(args);
	return r;
}

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
