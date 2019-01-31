/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/stringify.h>
#include <linux/version.h>

#include "public/mc_user.h"

#include "main.h"
#include "fastcall.h"
#include "logging.h"
#include "mcp.h"
#include "nq.h"
#include "scheduler.h"

#define SCHEDULING_FREQ		5   /**< N-SIQ every n-th time */
#define DEFAULT_TIMEOUT_MS	60000

static struct sched_ctx {
	struct task_struct	*thread;
	bool			thread_run;
	struct completion	idle_complete;	/* Unblock scheduler thread */
	struct completion	sleep_complete;	/* Wait for sleep status */
	struct mutex		sleep_mutex;	/* Protect sleep request */
	struct mutex		request_mutex;	/* Protect all below */
	/* The order of this enum matters */
	enum sched_command {
		NONE,		/* No specific request */
		YIELD,		/* Run the SWd */
		NSIQ,		/* Schedule the SWd */
		SUSPEND,	/* Suspend the SWd */
		RESUME,		/* Resume the SWd */
	}			request;
	bool			suspended;
} sched_ctx;

static int mc_scheduler_command(enum sched_command command)
{
	if (IS_ERR_OR_NULL(sched_ctx.thread))
		return -EFAULT;

	mutex_lock(&sched_ctx.request_mutex);
	if (sched_ctx.request < command) {
		sched_ctx.request = command;
		complete(&sched_ctx.idle_complete);
	}

	mutex_unlock(&sched_ctx.request_mutex);
	return 0;
}

static int mc_scheduler_pm_command(enum sched_command command)
{
	int ret = -EPERM;

	if (IS_ERR_OR_NULL(sched_ctx.thread))
		return -EFAULT;

	mutex_lock(&sched_ctx.sleep_mutex);

	/* Send request */
	mc_scheduler_command(command);

	/* Wait for scheduler to reply */
	wait_for_completion(&sched_ctx.sleep_complete);
	mutex_lock(&sched_ctx.request_mutex);
	if (command == SUSPEND) {
		if (sched_ctx.suspended)
			ret = 0;
	} else {
		if (!sched_ctx.suspended)
			ret = 0;
	}

	mutex_unlock(&sched_ctx.request_mutex);

	mutex_unlock(&sched_ctx.sleep_mutex);
	return ret;
}

static int mc_dev_command(enum nq_scheduler_commands command)
{
	switch (command) {
	case MC_NQ_YIELD:
		return mc_scheduler_command(YIELD);
	case MC_NQ_NSIQ:
		return mc_scheduler_command(NSIQ);
	}

	return -EINVAL;
}

int mc_scheduler_suspend(void)
{
	return mc_scheduler_pm_command(SUSPEND);
}

int mc_scheduler_resume(void)
{
	return mc_scheduler_pm_command(RESUME);
}

/*
 * This thread, and only this thread, schedules the SWd. Hence, reading the idle
 * status and its associated timeout is safe from race conditions.
 */
static int tee_scheduler(void *arg)
{
	int timeslice = 0;	/* Actually scheduling period */
	int ret = 0;

	while (1) {
		s32 timeout_ms = -1;
		bool pm_request = false;

		if (sched_ctx.suspended || nq_get_idle_timeout(&timeout_ms)) {
			/* If timeout is 0 we keep scheduling the SWd */
			if (!timeout_ms) {
				mc_scheduler_command(NSIQ);
			} else {
				bool infinite_timeout = timeout_ms < 0;

				if ((timeout_ms < 0) ||
				    (timeout_ms > DEFAULT_TIMEOUT_MS))
					timeout_ms = DEFAULT_TIMEOUT_MS;

				if (!wait_for_completion_timeout(
					&sched_ctx.idle_complete,
					msecs_to_jiffies(timeout_ms))) {
					if (infinite_timeout)
						continue;

					/* Timed out, force SWd schedule */
					mc_scheduler_command(NSIQ);
				}
			}
		}

		if (kthread_should_stop() || !sched_ctx.thread_run)
			break;

		/* Get requested command if any */
		mutex_lock(&sched_ctx.request_mutex);
		if (sched_ctx.request == YIELD)
			/* Yield forced: increment timeslice */
			timeslice++;
		else if (sched_ctx.request >= NSIQ) {
			/* Force N_SIQ, also to suspend/resume SWd */
			timeslice = 0;
			if (sched_ctx.request == SUSPEND) {
				nq_suspend();
				pm_request = true;
			} else if (sched_ctx.request == RESUME) {
				nq_resume();
				pm_request = true;
			}
		}

		if (g_ctx.f_time)
			nq_update_time();

		sched_ctx.request = NONE;
		mutex_unlock(&sched_ctx.request_mutex);

		/* Reset timeout so we don't loop if SWd halted */
		nq_reset_idle_timeout();
		if (timeslice--) {
			/* Resume SWd from where it was */
			ret = mc_fc_yield();
		} else {
			timeslice = SCHEDULING_FREQ;
			/* Call SWd scheduler */
			ret = mc_fc_nsiq();
		}

		/* Always flush log buffer after the SWd has run */
		mc_logging_run();
		if (ret)
			break;

		/* Should have suspended by now if requested */
		mutex_lock(&sched_ctx.request_mutex);
		if (pm_request) {
			sched_ctx.suspended = nq_suspended();
			complete(&sched_ctx.sleep_complete);
		}

		mutex_unlock(&sched_ctx.request_mutex);

		/* Flush pending notifications if possible */
		if (nq_notifications_flush())
			complete(&sched_ctx.idle_complete);
	}

	mc_dev_devel("exit, ret is %d", ret);
	return ret;
}

int mc_scheduler_start(void)
{
	sched_ctx.thread_run = true;
	sched_ctx.thread = kthread_run(tee_scheduler, NULL, "tee_scheduler");
	if (IS_ERR(sched_ctx.thread)) {
		mc_dev_notice("tee_scheduler thread creation failed");
		return PTR_ERR(sched_ctx.thread);
	}
	set_user_nice(sched_ctx.thread, -20);
	nq_register_scheduler(mc_dev_command);
	complete(&sched_ctx.idle_complete);
	return 0;
}

void mc_scheduler_stop(void)
{
	nq_register_scheduler(NULL);
	sched_ctx.thread_run = false;
	complete(&sched_ctx.idle_complete);
	kthread_stop(sched_ctx.thread);
}

int mc_scheduler_init(void)
{
	init_completion(&sched_ctx.idle_complete);
	init_completion(&sched_ctx.sleep_complete);
	mutex_init(&sched_ctx.sleep_mutex);
	mutex_init(&sched_ctx.request_mutex);
	return 0;
}
