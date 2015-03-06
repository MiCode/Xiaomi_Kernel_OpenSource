/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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
#include <linux/delay.h>

#include "public/mc_linux.h"
#include "public/mc_admin.h"

#include "mci/mcifc.h"
#include "mci/mcinq.h"		/* struct notification */

#include "main.h"
#include "fastcall.h"
#include "debug.h"
#include "logging.h"
#include "mcp.h"
#include "scheduler.h"

#define SCHEDULING_FREQ		5   /**< N-SIQ every n-th time */

static struct sched_ctx {
	struct task_struct	*thread;
	struct completion	complete;
	struct rw_semaphore	sched_lock;
	bool			thread_dead;
	struct mutex		request_mutex;	/* Protect the request below */
	enum {
		NONE,
		NSIQ,
		YIELD,
	}			request;
	/*
	 * This local queue is to be used to queue notifications when the
	 * notification queue is full, so no session gets its notification lost,
	 * especially MCP. Its must therefore be big enough to hold each session
	 * ID, i.e. MAX_NQ_ELEM, plus one to distinguish between queue empty and
	 * full.
	 */
	uint32_t		local_queue[MAX_NQ_ELEM];
	struct notification_queue_header local_queue_header;
	struct mutex		notification_mutex;	/* Local queue work */
} sched_ctx;

/*----------------------------------------------------------------------------*/
static const struct {
	uint idx;
	const char *msg;
} status_map[] = {
	/**< MobiCore control flags */
	{ MC_EXT_INFO_ID_FLAGS, "flags"},
	/**< MobiCore halt condition code */
	{ MC_EXT_INFO_ID_HALT_CODE, "haltCode"},
	/**< MobiCore halt condition instruction pointer */
	{ MC_EXT_INFO_ID_HALT_IP, "haltIp"},
	/**< MobiCore fault counter */
	{ MC_EXT_INFO_ID_FAULT_CNT, "faultRec.cnt"},
	/**< MobiCore last fault cause */
	{ MC_EXT_INFO_ID_FAULT_CAUSE, "faultRec.cause"},
	/**< MobiCore last fault meta */
	{ MC_EXT_INFO_ID_FAULT_META, "faultRec.meta"},
	/**< MobiCore last fault threadid */
	{ MC_EXT_INFO_ID_FAULT_THREAD, "faultRec.thread"},
	/**< MobiCore last fault instruction pointer */
	{ MC_EXT_INFO_ID_FAULT_IP, "faultRec.ip"},
	/**< MobiCore last fault stack pointer */
	{ MC_EXT_INFO_ID_FAULT_SP, "faultRec.sp"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_DFSR, "faultRec.arch.dfsr"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_ADFSR, "faultRec.arch.adfsr"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_DFAR, "faultRec.arch.dfar"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_IFSR, "faultRec.arch.ifsr"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_AIFSR, "faultRec.arch.aifsr"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_IFAR, "faultRec.arch.ifar"},
	/**< MobiCore configured by Daemon via fc_init flag */
	{ MC_EXT_INFO_ID_MC_CONFIGURED, "mcData.flags"},
	/**< MobiCore exception handler last partner */
	{ MC_EXT_INFO_ID_MC_EXC_PARTNER, "mcExcep.partner"},
	/**< MobiCore exception handler last peer */
	{ MC_EXT_INFO_ID_MC_EXC_IPCPEER, "mcExcep.peer"},
	/**< MobiCore exception handler last IPC message */
	{ MC_EXT_INFO_ID_MC_EXC_IPCMSG, "mcExcep.cause"},
	/**< MobiCore exception handler last IPC data */
	{MC_EXT_INFO_ID_MC_EXC_IPCDATA, "mcExcep.meta"},
};

static inline bool local_queue_empty(void)
{
	return sched_ctx.local_queue_header.write_cnt ==
		sched_ctx.local_queue_header.read_cnt;
}

static inline int local_queue_find(uint32_t session_id)
{
	if (!sched_ctx.local_queue_header.queue_size) {
		/* First call: initialise queue */
		int i;

		sched_ctx.local_queue_header.queue_size = MAX_NQ_ELEM;
		for (i = 0; i < MAX_NQ_ELEM; i++)
			sched_ctx.local_queue[i] = SID_INVALID;
	} else {
		int i;

		for (i = 0; i < MAX_NQ_ELEM; i++)
			if (sched_ctx.local_queue[i] == session_id)
				return i;
	}

	return -EAGAIN;
}

static inline void local_queue_push(uint32_t session_id)
{
	struct notification_queue_header *hdr = &sched_ctx.local_queue_header;
	uint32_t idx = hdr->write_cnt++ % hdr->queue_size;

	sched_ctx.local_queue[idx] = session_id;
}

static inline uint32_t local_queue_pop(void)
{
	struct notification_queue_header *hdr = &sched_ctx.local_queue_header;
	uint32_t idx = hdr->read_cnt++ % hdr->queue_size;
	uint32_t session_id;

	session_id = sched_ctx.local_queue[idx];
	sched_ctx.local_queue[idx] = SID_INVALID;
	return session_id;
}

static inline bool notif_queue_full(void)
{
	struct notification_queue *tx = g_ctx.nq.tx;

	return (tx->hdr.write_cnt - tx->hdr.read_cnt) == tx->hdr.queue_size;
}

static inline void notif_queue_push(uint32_t session_id)
{
	struct notification_queue_header *hdr = &g_ctx.nq.tx->hdr;
	uint32_t idx = hdr->write_cnt % hdr->queue_size;

	g_ctx.nq.tx->notification[idx].session_id = session_id;
	g_ctx.nq.tx->notification[idx].payload = 0;
	hdr->write_cnt++;
}

static inline bool local_queue_flush(void)
{
	bool flushed = false;

	while (!local_queue_empty() && !notif_queue_full()) {
		notif_queue_push(local_queue_pop());
		flushed = true;
	}

	return flushed;
}

void mc_dev_dump_mobicore_status(void)
{
	char uuid_str[33];
	int i;

	/* read additional info about exception-point and print */
	dev_err(g_ctx.mcd, "<t-base halted. Status dump:");

	for (i = 0; i < ARRAY_SIZE(status_map); i++) {
		uint32_t info;

		if (!mc_fc_info(status_map[i].idx, NULL, &info))
			dev_err(g_ctx.mcd, "  %-20s= 0x%08x", status_map[i].msg,
				info);
	}

	/* construct UUID string */
	for (i = 0; i < 4; i++) {
		uint32_t info;
		int j;

		if (mc_fc_info(MC_EXT_INFO_ID_MC_EXC_UUID + i, NULL, &info))
			return;

		for (j = 0; j < sizeof(info); j++) {
			snprintf(&uuid_str[(i * sizeof(info) + j) * 2], 3,
				 "%02x", (info >> (j * 8)) & 0xff);
		}
	}

	dev_err(g_ctx.mcd, "  %-20s= 0x%s", "mcExcep.uuid", uuid_str);
}

void mc_dev_schedule(void)
{
	if (sched_ctx.thread_dead)
		return;

	complete(&sched_ctx.complete);
}

/* Yield to MobiCore */
int mc_dev_yield(void)
{
	if (sched_ctx.thread_dead)
		return -EFAULT;

	mutex_lock(&sched_ctx.request_mutex);
	/* Give higher priority to NSIQ */
	if (sched_ctx.request != NSIQ)
		sched_ctx.request = YIELD;
	mutex_unlock(&sched_ctx.request_mutex);
	complete(&sched_ctx.complete);
	return 0;
}

/* call common notify */
int mc_dev_nsiq(void)
{
	if (sched_ctx.thread_dead)
		return -EFAULT;

	mutex_lock(&sched_ctx.request_mutex);
	sched_ctx.request = NSIQ;
	mutex_unlock(&sched_ctx.request_mutex);
	complete(&sched_ctx.complete);
	return 0;
}

/* Return MobiCore driver version */
uint32_t mc_dev_get_version(void)
{
	return MC_VERSION(MCDRVMODULEAPI_VERSION_MAJOR,
			  MCDRVMODULEAPI_VERSION_MINOR);
}

/*
 * This thread, and only this thread, schedules the SWd. Hence, reading the idle
 * status and its associated timeout is safe from race conditions.
 */
static int schedule_thread(void *arg)
{
	int timeslice = SCHEDULING_FREQ;	/* Actually scheduling period */
	int ret = 0;

	MCDRV_DBG("enter");
	while (1) {
		unsigned long time_left = 0;
		int32_t timeout_ms;

		if (mcp_get_idle_timeout(&timeout_ms)) {
			bool was_completed = false;

			if (timeout_ms < 0) {
				wait_for_completion(&sched_ctx.complete);
				was_completed = true;
			} else {
				if (timeout_ms > 0)
					time_left = wait_for_completion_timeout(
						&sched_ctx.complete,
						msecs_to_jiffies(timeout_ms));
				else
					/* Don't busy loop */
					usleep_range(100, 500);

				if (time_left)
					was_completed = true;
				else
					/* Timed out, force SWd schedule */
					timeslice = 0;
			}

			/* Get requested command if any */
			if (was_completed) {
				mutex_lock(&sched_ctx.request_mutex);
				if (sched_ctx.request == NSIQ)
					timeslice = 0;
				else if (sched_ctx.request == YIELD)
					timeslice++;

				sched_ctx.request = NONE;
				mutex_unlock(&sched_ctx.request_mutex);
			}
		}

		if (kthread_should_stop())
			break;

		/*
		 * No timeout expiration means we got completed by an external
		 * mean, so don't give time to the SWd [again].
		 */
		if (!time_left) {
			/* Reset timeout so we don't loop if SWd halted */
			mcp_reset_idle_timeout();
			if (timeslice--) {
				/* Resume SWd from where it was */
				ret = mc_fc_yield();
				if (ret)
					MCDRV_ERROR("yield failed");
			} else {
				timeslice = SCHEDULING_FREQ;
				/* Call SWd scheduler */
				ret = mc_fc_nsiq();
				if (ret)
					MCDRV_ERROR("N-SIQ failed");
			}

			if (ret)
				break;
		}

		/* Flush local queue if possible */
		mutex_lock(&sched_ctx.notification_mutex);
		if (local_queue_flush())
			complete(&sched_ctx.complete);

		mutex_unlock(&sched_ctx.notification_mutex);
	}

	sched_ctx.thread_dead = true;
	MCDRV_DBG("exit, ret is %d", ret);
	return ret;
}

int mc_dev_notify(uint32_t session_id)
{
	int ret = 0;

	mutex_lock(&sched_ctx.notification_mutex);
	if (session_id == SID_MCP)
		MCDRV_DBG("to <t-base");
	else
		MCDRV_DBG("to session %x", session_id);

	/* Notify <t-base about new data */
	if (!local_queue_empty() || notif_queue_full()) {
		if (local_queue_find(session_id) >= 0)
			ret = -EAGAIN;
		else
			local_queue_push(session_id);

		local_queue_flush();

		if (mc_dev_yield()) {
			MCDRV_ERROR("MC_SMC_N_YIELD failed");
			ret = -EPROTO;
		}
	} else {
		notif_queue_push(session_id);
		if (mc_dev_nsiq()) {
			MCDRV_ERROR("MC_SMC_N_SIQ failed");
			ret = -EPROTO;
		}
	}

	mutex_unlock(&sched_ctx.notification_mutex);
	return ret;
}

int mc_dev_sched_init(void)
{
	int ret = 0;
	uint32_t timeslot;

	init_rwsem(&sched_ctx.sched_lock);

	init_completion(&sched_ctx.complete);
	mutex_init(&sched_ctx.request_mutex);

	/* Setup notification queue mutex */
	mutex_init(&sched_ctx.notification_mutex);

	/* First empty N-SIQ which results in set up of the MCI structure */
	ret = mc_dev_nsiq();
	if (ret) {
		MCDRV_ERROR("sending N-SIQ failed");
		goto cleanup;
	}

	sched_ctx.thread = kthread_run(schedule_thread, NULL,
				       "mc_dev_schedule_thread");
	if (IS_ERR(sched_ctx.thread)) {
		MCDRV_ERROR("mc_dev_schedule thread creation failed");
		ret = PTR_ERR(sched_ctx.thread);
		goto cleanup;
	}

	/*
	 * Wait until <t-base state switches to MC_STATUS_INITIALIZED
	 * It is assumed that <t-base always switches state at a certain
	 * point in time.
	 */
	do {
		uint32_t status = 0;

		ret = mc_fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL);
		if (ret) {
			MCDRV_ERROR("receiving status was failed");
			break;
		}

		switch (status) {
		case MC_STATUS_NOT_INITIALIZED:
			/* Switch to <t-base to give it more CPU time. */
			ret = EAGAIN;
			for (timeslot = 0; timeslot < 10; timeslot++) {
				int tmp_ret = mc_dev_yield();

				if (tmp_ret) {
					MCDRV_ERROR("yield to SWd failed");
					ret = tmp_ret;
					break;
				}
			}

			/* No need to loop like mad */
			if (ret == EAGAIN)
				usleep_range(100, 500);

			break;
		case MC_STATUS_HALT:
			mc_dev_dump_mobicore_status();
			MCDRV_ERROR("<t-base halt during init. State 0x%x",
				    status);
			ret = -ENODEV;
			break;
		case MC_STATUS_INITIALIZED:
			mobicore_log_read();
			MCDRV_DBG("<t-base is ready");
			ret = 0;
			break;
		default:
			/* MC_STATUS_BAD_INIT or anything else */
			MCDRV_ERROR("MCI init failed, state 0x%x", status);
			ret = -EIO;
			break;
		}
	} while (ret == EAGAIN);

	if (ret == 0) {
		complete(&sched_ctx.complete);
		return 0;
	}

cleanup:
	if (!IS_ERR_OR_NULL(sched_ctx.thread)) {
		/* We don't really care what the thread returns for exit */
		kthread_stop(sched_ctx.thread);
		complete(&sched_ctx.complete);
	}

	return ret;
}

void mc_dev_sched_cleanup(void)
{
	if (!IS_ERR_OR_NULL(sched_ctx.thread)) {
		/* We don't really care what the thread returns for exit */
		kthread_stop(sched_ctx.thread);
		complete(&sched_ctx.complete);
	}
}
