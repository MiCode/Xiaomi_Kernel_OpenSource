// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2020 TRUSTONIC LIMITED
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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/delay.h> /* msleep */
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/sched/clock.h>	/* local_clock */
#include <tee_sanity.h>

#include "platform.h"			/* CPU-related information */

#include "public/mc_user.h"
#include "public/mc_linux_api.h"

#include "mci/mcifc.h"
#include "mci/mciiwp.h"
#include "mci/mcimcp.h"
#include "mci/mcinq.h"
#include "mci/mcitime.h"		/* struct mcp_time */

#include "main.h"
#include "clock.h"
#include "fastcall.h"
#include "logging.h"
#include "nq.h"

#define NQ_NUM_ELEMS		64
#define DEFAULT_TIMEOUT_MS	20000	/* We do nothing on timeout anyway */

#if !defined(NQ_TEE_WORKER_THREADS)
#define NQ_TEE_WORKER_THREADS	1
#endif

static struct {
	struct mutex buffer_mutex;	/* Lock on SWd communication buffer */
	struct mcp_buffer *mcp_buffer;
	struct interworld_session *iwp_buffer;
	struct task_struct *irq_bh_thread;
	struct completion irq_bh_complete;
	bool irq_bh_thread_run;
	int irq;
	struct blocking_notifier_head tee_stop_notifiers;
	void (*mcp_notif_handler)(u32 id, u32 payload);
	void (*iwp_notif_handler)(u32 id, u32 payload);
	/* MobiCore MCI information */
	unsigned int order;
	union {
		void		*mci;
		struct {
			struct notification_queue *tx;
			struct notification_queue *rx;
		} nq;
	};
	/*
	 * This notifications list is to be used to queue notifications when the
	 * notification queue overflows, so no session gets its notification
	 * lost, especially MCP.
	 */
	struct mutex		notifications_mutex;
	struct list_head	notifications;
	/* Dump buffer */
	char			*tee_version;
	struct kasnprintf_buf	dump;
	/* Time */
	struct mcp_time		*time;
	/* Protects above shared MCP time */
	struct mutex		mcp_time_mutex;

	/* Scheduler */
	struct task_struct	*tee_worker[NQ_TEE_WORKER_THREADS];
	bool			tee_scheduler_run;
	bool			tee_hung;

	/* Logging */
	phys_addr_t		log_buffer;
	u32			log_buffer_size;
	bool			log_buffer_busy;

	/* TEE affinity */
	unsigned long		default_affinity_mask;
#if defined(BIG_CORE_SWITCH_AFFINITY_MASK)
	atomic_t		big_core_demand_cnt;
#endif
	atomic_t		tee_affinity;

	/* TEE workers */
	atomic_t		workers_started;
	atomic_t		workers_run;
	wait_queue_head_t	workers_wq;

	/* TEE worker counters */
	char			struct_workers_counters_buf[1024];
	int			struct_workers_counters_buf_len;
	struct {
		atomic64_t	nyield;
		atomic64_t	syield;
		atomic64_t	sbusy;
		atomic64_t	sresume;
	} tee_worker_counters[NQ_TEE_WORKER_THREADS];
} l_ctx;

enum counter_id {
	TEE_WORKER_COUNTER_NYIELD,
	TEE_WORKER_COUNTER_SYIELD,
	TEE_WORKER_COUNTER_BUSY,
	TEE_WORKER_COUNTER_RESUME
};

static long get_tee_affinity(void)
{
	return atomic_read(&l_ctx.tee_affinity);
}

static u32 get_workers(void)
{
	return atomic_read(&l_ctx.workers_run);
}

static u32 get_required_workers(void)
{
	return READ_ONCE(l_ctx.mcp_buffer->flags.required_workers);
}

static u8 get_tee_flags(void)
{
	return READ_ONCE(l_ctx.mcp_buffer->flags.tee_flags);
}

static inline bool is_iwp_id(u32 id)
{
	return (id & SID_IWP_NOTIFICATION) != 0;
}

static inline void session_state_update_internal(struct nq_session *session,
						 enum nq_notif_state state)
{
	mutex_lock(&session->mutex);
	session->state = state;
	session->cpu_clk = local_clock();
	mutex_unlock(&session->mutex);
}

/*
 * Notification Queue overflow management:
 * - once the SWd NQ is full, sessions get added to the overflow queue:
 *   'l_ctx.notifications'
 * - as long as this queue is not empty, new notifications get added there
 *   first, if not already present, then the queue is flushed
 * - the queue is also flushed by the scheduler once the SWd has run
 */
static inline bool notif_queue_full(void)
{
	struct notification_queue *tx = l_ctx.nq.tx;

	return (tx->hdr.write_cnt - tx->hdr.read_cnt) == tx->hdr.queue_size;
}

static inline void notif_queue_push(u32 session_id, u32 payload)
{
	struct notification_queue_header *hdr = &l_ctx.nq.tx->hdr;
	u32 i = hdr->write_cnt % hdr->queue_size;

	l_ctx.nq.tx->notification[i].session_id = session_id;
	l_ctx.nq.tx->notification[i].payload = payload;
	/*
	 * Ensure notification[] is written before we update the counter
	 * We want a ARM dmb() / ARM64 dmb(sy) here
	 */
	smp_mb();

	hdr->write_cnt++;
	/*
	 * Ensure write_cnt is written before new notification
	 * We want a ARM dsb() / ARM64 dsb(sy) here
	 */
	rmb();
}

/* Must be called with l_ctx.notifications_mutex taken */
static inline bool nq_notifications_flush(void)
{
	bool flushed = false;

	while (!list_empty(&l_ctx.notifications) && !notif_queue_full()) {
		struct nq_session *session;

		session = list_first_entry(&l_ctx.notifications,
					   struct nq_session, list);
		mc_dev_devel("pop %x", session->id);
		notif_queue_push(session->id, session->payload);
		session_state_update_internal(session, NQ_NOTIF_SENT);
		list_del_init(&session->list);
		flushed = true;
	}

	return flushed;
}

static inline void nq_update_time(void)
{
	struct timespec tm1, tm2;

	getnstimeofday(&tm1);
	getrawmonotonic(&tm2);
	mutex_lock(&l_ctx.mcp_time_mutex);
	l_ctx.time->wall_clock_seconds = tm1.tv_sec;
	l_ctx.time->wall_clock_nsec    = tm1.tv_nsec;
	l_ctx.time->monotonic_seconds  = tm2.tv_sec;
	l_ctx.time->monotonic_nsec     = tm2.tv_nsec;
	mutex_unlock(&l_ctx.mcp_time_mutex);
}

static inline void nq_notif_handler(u32 id, u32 payload)
{
	mc_dev_devel("NQ notif for id %x payload %x", id, payload);
	if (is_iwp_id(id))
		l_ctx.iwp_notif_handler(id, payload);
	else
		l_ctx.mcp_notif_handler(id, payload);
}

static int irq_bh_worker(void *arg)
{
	struct notification_queue *rx = l_ctx.nq.rx;

	while (1) {
		wait_for_completion_killable(&l_ctx.irq_bh_complete);

		/* This thread can only be stopped with nq_stop */
		if (!l_ctx.irq_bh_thread_run)
			break;

		/* Deal with all pending notifications in one go */
		while ((rx->hdr.write_cnt - rx->hdr.read_cnt) > 0) {
			struct notification nf;

			nf = rx->notification[
				rx->hdr.read_cnt % rx->hdr.queue_size];

			/*
			 * Ensure read_cnt writing happens after buffer read
			 * We want a ARM dmb() / ARM64 dmb(sy) here
			 */
			smp_mb();
			rx->hdr.read_cnt++;
			/*
			 * Ensure read_cnt writing finishes before reader
			 * We want a ARM dsb() / ARM64 dsb(sy) here
			 */
			rmb();
			nq_notif_handler(nf.session_id, nf.payload);
		}

		/* This is needed to properly handle secure interrupts when */
		/* there is no active worker.                               */
		if (!get_workers())
			wake_up_process(
				l_ctx.tee_worker[NQ_TEE_WORKER_THREADS - 1]);
	}
	return 0;
}

static irqreturn_t irq_handler(int intr, void *arg)
{
	/* wake up thread to continue handling this interrupt */
	complete(&l_ctx.irq_bh_complete);
	return IRQ_HANDLED;
}

cpumask_t tee_set_affinity(void)
{
	cpumask_t old_affinity;
	unsigned long affinity = get_tee_affinity();

	old_affinity = current->cpus_allowed;
	mc_dev_devel("aff = %lx mask = %lx curr_aff = %*pbl (pid = %u)",
		     affinity,
		     l_ctx.default_affinity_mask,
		     cpumask_pr_args(&old_affinity),
		     current->pid);
	set_cpus_allowed_ptr(current, to_cpumask(&affinity));

	return old_affinity;
}

void tee_restore_affinity(cpumask_t old_affinity)
{
	cpumask_t current_affinity = current->cpus_allowed;

	(void)current_affinity;
	mc_dev_devel("aff = %*pbl mask = %lx curr_aff = %*pbl (pid = %u)",
		     cpumask_pr_args(&old_affinity),
		     l_ctx.default_affinity_mask,
		     cpumask_pr_args(&current_affinity),
		     current->pid);
	set_cpus_allowed_ptr(current, &old_affinity);
}

void nq_session_init(struct nq_session *session, bool is_gp)
{
	session->id = SID_INVALID;
	session->payload = 0;
	INIT_LIST_HEAD(&session->list);
	mutex_init(&session->mutex);
	session->state = NQ_NOTIF_IDLE;
	session->cpu_clk = 0;
	session->is_gp = is_gp;
}

void nq_session_exit(struct nq_session *session)
{
	mutex_lock(&l_ctx.notifications_mutex);
	if (!list_empty(&session->list))
		list_del(&session->list);
	mutex_unlock(&l_ctx.notifications_mutex);
}

void nq_session_state_update(struct nq_session *session,
			     enum nq_notif_state state)
{
	if (state < NQ_NOTIF_RECEIVED)
		return;

	session_state_update_internal(session, state);
}

int nq_session_notify(struct nq_session *session, u32 id, u32 payload)
{
	int ret = 0;

	mutex_lock(&l_ctx.notifications_mutex);
	session->id = id;
	session->payload = payload;
	if (!list_empty(&l_ctx.notifications) || notif_queue_full()) {
		if (!list_empty(&session->list)) {
			ret = -EAGAIN;
			if (payload != session->payload) {
				mc_dev_err(ret,
					   "skip %x payload change %x -> %x",
					   session->id, session->payload,
					   payload);
			} else {
				mc_dev_devel("skip %x payload %x",
					     session->id, payload);
			}
		} else {
			mc_dev_devel("push %x payload %x", session->id,
				     payload);
			/* session->payload = payload; */
			list_add_tail(&session->list, &l_ctx.notifications);
			session_state_update_internal(session, NQ_NOTIF_QUEUED);
		}

		nq_notifications_flush();
	} else {
		cpumask_t old_affinity;

		mc_dev_devel("send %x payload %x", session->id, payload);
		notif_queue_push(session->id, payload);
		session_state_update_internal(session, NQ_NOTIF_SENT);

		nq_update_time();
		old_affinity = tee_set_affinity();
		if (fc_nsiq(session->id, payload))
			ret = -EPROTO;
		tee_restore_affinity(old_affinity);
		wake_up_process(l_ctx.tee_worker[NQ_TEE_WORKER_THREADS - 1]);
		logging_run();
	}

	mutex_unlock(&l_ctx.notifications_mutex);
	return ret;
}

const char *nq_session_state(const struct nq_session *session, u64 *cpu_clk)
{
	if (cpu_clk)
		*cpu_clk = session->cpu_clk;

	switch (session->state) {
	case NQ_NOTIF_IDLE:
		return "idle";
	case NQ_NOTIF_QUEUED:
		return "queued";
	case NQ_NOTIF_SENT:
		return "sent";
	case NQ_NOTIF_RECEIVED:
		return "received";
	case NQ_NOTIF_CONSUMED:
		return "consumed";
	case NQ_NOTIF_DEAD:
		return "dead";
	}
	return "error";
}

static ssize_t debug_crashdump_read(struct file *file, char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	if (l_ctx.dump.off)
		return simple_read_from_buffer(user_buf, count, ppos,
					       l_ctx.dump.buf, l_ctx.dump.off);

	return 0;
}

static const struct file_operations debug_crashdump_ops = {
	.read = debug_crashdump_read,
	.llseek = default_llseek,
};

static ssize_t debug_smclog_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	return debug_generic_read(file, user_buf, count, ppos,
				  mc_fastcall_debug_smclog);
}

static const struct file_operations debug_smclog_ops = {
	.read = debug_smclog_read,
	.llseek = default_llseek,
	.open = debug_generic_open,
	.release = debug_generic_release,
};

static void nq_dump_status(void)
{
	static const struct {
		unsigned int index;
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
		/**< MobiCore last crashing task offset */
		{MC_EXT_INFO_ID_TASK_OFFSET,
		"faultRec.offset.task"},
		/**< MobiCore last crashing task's mcLib offset */
		{MC_EXT_INFO_ID_MCLIB_OFFSET,
		"faultRec.offset.mclib"},
	};

	char uuid_str[33];
	int ret = 0;
	size_t i;
	cpumask_t old_affinity;

	if (l_ctx.dump.off)
		ret = -EBUSY;

	mc_dev_info("TEE HALTED");
	if (l_ctx.tee_version) {
		mc_dev_info("TEE version: %s", l_ctx.tee_version);
		if (ret >= 0)
			ret = kasnprintf(&l_ctx.dump, "TEE version: %s\n",
					 l_ctx.tee_version);
	}

	mc_dev_info("Status dump:");
	old_affinity = tee_set_affinity();
	for (i = 0; i < (size_t)ARRAY_SIZE(status_map); i++) {
		u32 info;

		if (fc_info(status_map[i].index, NULL, &info)) {
			tee_restore_affinity(old_affinity);
			return;
		}

		mc_dev_info("  %-22s= 0x%08x", status_map[i].msg, info);
		if (ret >= 0)
			ret = kasnprintf(&l_ctx.dump, "%-22s= 0x%08x\n",
					 status_map[i].msg, info);
	}

	/* construct UUID string */
	for (i = 0; i < 4; i++) {
		u32 info;
		size_t j;

		if (fc_info(MC_EXT_INFO_ID_MC_EXC_UUID + i, NULL, &info)) {
			tee_restore_affinity(old_affinity);
			return;
		}

		for (j = 0; j < sizeof(info); j++) {
			snprintf(&uuid_str[(i * sizeof(info) + j) * 2], 3,
				 "%02x", (info >> (j * 8)) & 0xff);
		}
	}
	tee_restore_affinity(old_affinity);

	mc_dev_info("  %-22s= 0x%s", "mcExcep.uuid", uuid_str);
	if (ret >= 0)
		ret = kasnprintf(&l_ctx.dump, "%-22s= 0x%s\n", "mcExcep.uuid",
				 uuid_str);

	if (ret < 0) {
		kfree(l_ctx.dump.buf);
		l_ctx.dump.off = 0;
		return;
	}

	debugfs_create_file("crashdump", 0400, g_ctx.debug_dir, NULL,
			    &debug_crashdump_ops);
	debugfs_create_file("last_smc_commands", 0400, g_ctx.debug_dir, NULL,
			    &debug_smclog_ops);
}

static void nq_handle_tee_crash(void)
{
	/*
	 * Do not change the call order: the debugfs nq status file needs
	 * to be created before requesting the Daemon to read it.
	 */
	nq_dump_status();
	blocking_notifier_call_chain(&l_ctx.tee_stop_notifiers, 0, NULL);
}

union mcp_message *nq_get_mcp_buffer(void)
{
	return &l_ctx.mcp_buffer->message;
}

struct interworld_session *nq_get_iwp_buffer(void)
{
	return l_ctx.iwp_buffer;
}

void nq_set_version_ptr(char *version)
{
	l_ctx.tee_version = version;
}

void nq_register_notif_handler(void (*handler)(u32 id, u32 payload), bool iwp)
{
	if (iwp)
		l_ctx.iwp_notif_handler = handler;
	else
		l_ctx.mcp_notif_handler = handler;
}

int nq_register_tee_stop_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&l_ctx.tee_stop_notifiers, nb);
}

int nq_unregister_tee_stop_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&l_ctx.tee_stop_notifiers,
						  nb);
}

ssize_t nq_get_stop_message(char __user *buffer, size_t size)
{
	size_t max_len = l_ctx.dump.size - l_ctx.dump.off;
	char *buf = l_ctx.dump.buf;
	int ret;

	if (!l_ctx.dump.off || !max_len)
		return 0;

	if (size > max_len)
		size = max_len;

	ret = copy_to_user(buffer, buf, size);
	if (ret)
		return -EFAULT;

	return size;
}

void nq_signal_tee_hung(void)
{
	mc_dev_devel("force stop the notification queue");
	/* Stop the tee_scheduler thread */
	l_ctx.tee_hung = true;
	l_ctx.tee_scheduler_run = false;
	wake_up_all(&l_ctx.workers_wq);
}

static int nq_boot_tee(void)
{
	size_t q_len = ALIGN(2 * (sizeof(struct notification_queue_header) +
		NQ_NUM_ELEMS * sizeof(struct notification)), 4);
	struct irq_data *irq_d = irq_get_irq_data(l_ctx.irq);
	int ret;

	/* Call the INIT fastcall to setup shared buffers */
	cpumask_t old_affinity = tee_set_affinity();

	ret = fc_init(virt_to_phys(l_ctx.mci),
		      (uintptr_t)l_ctx.mcp_buffer - (uintptr_t)l_ctx.mci, q_len,
		      sizeof(*l_ctx.mcp_buffer));
	logging_run();
	if (ret)
		goto out;

	/* Set initialization values */
#if defined(MC_INTR_SSIQ_SWD)
	l_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_IRQ;
	l_ctx.mcp_buffer->message.init_values.irq = MC_INTR_SSIQ_SWD;
#endif
	l_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_TIME;
	if (irq_d) {
#ifdef CONFIG_MTK_SYSIRQ
		if (irq_d->parent_data) {
			l_ctx.mcp_buffer->message.init_values.flags |=
				MC_IV_FLAG_IRQ;
			l_ctx.mcp_buffer->message.init_values.irq =
				irq_d->parent_data->hwirq;
			mc_dev_info("irq_d->parent_data->hwirq is 0x%lx\n",
				irq_d->parent_data->hwirq);
		}
#else
		l_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_IRQ;
		l_ctx.mcp_buffer->message.init_values.irq = irq_d->hwirq;
		mc_dev_info("irq_d->hwirq is 0x%lx\n", irq_d->hwirq);
#endif
	}
	l_ctx.mcp_buffer->message.init_values.time_ofs =
		(u32)((uintptr_t)l_ctx.time - (uintptr_t)l_ctx.mci);
	l_ctx.mcp_buffer->message.init_values.time_len =
			sizeof(*l_ctx.time);

	l_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_IWP;
	l_ctx.mcp_buffer->message.init_values.iws_buf_ofs =
		(u64)((uintptr_t)l_ctx.iwp_buffer - (uintptr_t)l_ctx.mci);
	l_ctx.mcp_buffer->message.init_values.iws_buf_size =
		MAX_IW_SESSION * sizeof(struct interworld_session);

	/* First empty N-SIQ to setup of the MCI structure */
	ret = fc_nsiq(0, 0);
	logging_run();
	if (ret)
		goto out;

	/*
	 * Wait until the TEE state switches to MC_STATUS_INITIALIZED
	 * It is assumed that it always switches state at some point
	 */
	do {
		u32 status = 0;
		u32 timeslice;

		ret = fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL);
		logging_run();
		if (ret)
			goto out;

		switch (status) {
		case MC_STATUS_NOT_INITIALIZED:
			/* Switch to the TEE to give it more CPU time. */
			ret = -EAGAIN;
			for (timeslice = 0; timeslice < 10; timeslice++) {
				int tmp_ret = fc_yield(0, 0, NULL);

				logging_run();
				if (tmp_ret) {
					ret = tmp_ret;
					goto out;
				}
			}

			/* No need to loop like mad */
			if (ret == -EAGAIN)
				usleep_range(100, 500);

			break;
		case MC_STATUS_HALT:
			ret = -ENODEV;
			nq_handle_tee_crash();
			mc_dev_err(ret, "halt during init, state 0x%x", status);
			goto out;
		case MC_STATUS_INITIALIZED:
			mc_dev_devel("ready");
			break;
		default:
			/* MC_STATUS_BAD_INIT or anything else */
			ret = -EIO;
			mc_dev_err(ret, "MCI init failed, state 0x%x", status);
			goto out;
		}
	} while (ret == -EAGAIN);

out:
	tee_restore_affinity(old_affinity);
	return ret;
}

static int tee_wait_infinite(void)
{
	return wait_event_interruptible_exclusive(l_ctx.workers_wq,
					!l_ctx.tee_scheduler_run ||
					get_workers() < get_required_workers());
}

static int tee_wait_timeout(unsigned int timeout_ms)
{
	return wait_event_interruptible_timeout(l_ctx.workers_wq,
						!l_ctx.tee_scheduler_run ||
						get_workers() <
						get_required_workers(),
						msecs_to_jiffies(timeout_ms));
}

static int tee_worker_wait(unsigned int timeout_ms)
{
	int ret, workers;

	if (timeout_ms)
		ret = tee_wait_timeout(timeout_ms);
	else
		ret = tee_wait_infinite();
	if (ret < 0)
		return ret; /* interrupted by signal */

	/* Assert nb workers between one and maximum statically allowed */
	workers = atomic_inc_return(&l_ctx.workers_run);
	if (workers < 1 || workers > NQ_TEE_WORKER_THREADS)
		return -EINVAL;

	return 0;
}

static int tee_worker_counter_inc(int worker_id, enum counter_id cnt_id)
{
	if (worker_id >= NQ_TEE_WORKER_THREADS) {
		mc_dev_err(-EINVAL, "worker id out of range (%d)!", worker_id);
		return -EINVAL;
	}

	switch (cnt_id) {
	case TEE_WORKER_COUNTER_NYIELD:
		atomic64_inc(&l_ctx.tee_worker_counters[worker_id].nyield);
		break;
	case TEE_WORKER_COUNTER_SYIELD:
		atomic64_inc(&l_ctx.tee_worker_counters[worker_id].syield);
		break;
	case TEE_WORKER_COUNTER_BUSY:
		atomic64_inc(&l_ctx.tee_worker_counters[worker_id].sbusy);
		break;
	case TEE_WORKER_COUNTER_RESUME:
		atomic64_inc(&l_ctx.tee_worker_counters[worker_id].sresume);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned long tee_worker_counter_get(int worker_id,
					    enum counter_id cnt_id)
{
	long ret = 0;

	if (worker_id >= NQ_TEE_WORKER_THREADS) {
		mc_dev_err(-EINVAL, "worker id out of range (%d)!", worker_id);
		return -EINVAL;
	}

	switch (cnt_id) {
	case TEE_WORKER_COUNTER_NYIELD:
		ret = (unsigned long)atomic64_read(
		&l_ctx.tee_worker_counters[worker_id].nyield);
		break;
	case TEE_WORKER_COUNTER_SYIELD:
		ret = (unsigned long)atomic64_read(
		&l_ctx.tee_worker_counters[worker_id].syield);
		break;
	case TEE_WORKER_COUNTER_BUSY:
		ret = (unsigned long)atomic64_read(
		&l_ctx.tee_worker_counters[worker_id].sbusy);
		break;
	case TEE_WORKER_COUNTER_RESUME:
		ret = (unsigned long)atomic64_read(
		&l_ctx.tee_worker_counters[worker_id].sresume);
		break;
	default:
		/* error case, returned value is 0 */
		break;
	}

	return ret;
}

static s32 tee_schedule(uintptr_t arg, unsigned int *timeout_ms)
{
	u32 run;
	struct fc_s_yield resp;
	bool tee_halted;
	u32 req_workers;
	int ret, id = (int)arg;
	*timeout_ms = 0;

	while (true) {
		/* If nq_stop or nq_signal_tee_hung called */
		if (!l_ctx.tee_scheduler_run) {
			ret = -EIO;
			goto exit;
		}

		/* Check crash */
		tee_halted = get_tee_flags() & MC_STATE_FLAG_TEE_HALT_MASK;
		if (tee_halted) {
			nq_signal_tee_hung(); /* Signal other workers */
			ret = -EHOSTUNREACH;
			goto exit;
		}

		/* Adjust worker CPU affinity based on global TEE affinity.
		 * No need to save it, we are running in our own kthread
		 */
		tee_set_affinity();

		/* Set basic utils to boost TEE performance. */
		mtk_set_task_basic_util(current);

		/* Refresh MCI REE time */
		nq_update_time();

		/* Count number of worker N-Yield SMCs */
		tee_worker_counter_inc(id, TEE_WORKER_COUNTER_NYIELD);

		/* Relinquish current CPU to TEE */
		ret = fc_yield(0, 0, &resp);
		/* Any worker wakes up the log thread upon returning from SWd.
		 * Logging should happen whether the yield succeded or not
		 */
		logging_run();

		if (ret)
			goto exit;

		switch (resp.resp) {
		case MC_SMC_S_YIELD:
			/* NOTE: resp.code returns -1 (infinite), -2, 0, or   */
			/* positive min. timeout value of internal time queue.*/
			tee_worker_counter_inc(id, TEE_WORKER_COUNTER_SYIELD);
			mc_dev_devel("[%d] MC_SMC_S_YIELD timeout=%d",
				     id, resp.code);
			mc_dev_devel("[%d] req workers=%d workers=%d",
				     id,
				     get_required_workers(),
				     get_workers());

			if ((s32)resp.code > 0) {
				*timeout_ms = resp.code;
			} else if (resp.code == 0) {
				/* Reschedule the TEE immediately */
				continue;
			}
			break;

		case MC_SMC_S_BUSY:
			tee_worker_counter_inc(id, TEE_WORKER_COUNTER_BUSY);
			mc_dev_devel("[%d] BUSY", id);
			break;

		case MC_SMC_S_RESUME:
			tee_worker_counter_inc(id, TEE_WORKER_COUNTER_RESUME);
			break;

		case MC_SMC_S_HALT:
			mc_dev_devel("[%d] received TEE halt response.", id);
			nq_signal_tee_hung(); /* Signal other workers */
			ret = -EHOSTUNREACH;
			goto exit;

		default:
			mc_dev_devel("[%d] unknown TEE response (0x%x)", id,
				     resp.resp);
			nq_signal_tee_hung(); /* Signal other workers */
			ret = -EIO;
			goto exit;
		}

		/* Probe TEE activity */
		run         = get_workers();
		req_workers = get_required_workers();

		/* If SWd has more threads to run, then add a worker */
		if (run < req_workers && run < NQ_TEE_WORKER_THREADS) {
			int i = 0;

			mc_dev_devel("[%d] R1 run=%d sc=%d", id, run,
				     req_workers);
			while ((i < NQ_TEE_WORKER_THREADS)) {
				if (wake_up_process(
			    l_ctx.tee_worker[NQ_TEE_WORKER_THREADS - 1 - i]))
					break;
				i++;
			}
		}

		/* If SWd has less threads to run, then current worker */
		/* likely goes sleeping.                               */
		if (run > req_workers) {
			mc_dev_devel("[%d] R2 run=%d sc=%d", id, run,
				     req_workers);
			atomic_dec(&l_ctx.workers_run);
			ret = 0;
			goto exit;
		}
	}

exit:
	return ret;
}

/*
 * tee_worker is instantiated into NQ_TEE_WORKER_THREADS threads
 */
static int tee_worker(void *arg)
{
	uintptr_t id = (uintptr_t)arg;
	int ret;
	unsigned int timeout_ms = 0;

	mc_dev_devel("[%ld] starts", id);
	atomic_inc(&l_ctx.workers_started);

	while (true) {
		ret = tee_worker_wait(timeout_ms);
		if (ret)
			break; /* Worker received a signal, exit */

		mc_dev_devel("[%ld] wake up run=%d required_workers=%d",
			     id, get_workers(), get_required_workers());
		ret = tee_schedule(id, &timeout_ms);
		if (ret)
			break;
	}

	mc_dev_devel("[%ld] exits, ret=%d", id, ret);
	if (!atomic_dec_return(&l_ctx.workers_started)) {
		mc_dev_info("TEE scheduler exits ...");
		if (l_ctx.tee_hung)
			/* There is an error, the tee must have crashed */
			nq_handle_tee_crash();

		/* Logging */
		ret = fc_trace_deinit();
		if (!ret)
			l_ctx.log_buffer_busy = false;
		else
			mc_dev_err(ret, "failed to unregister log buffer");

		/* Disable TEE clock */
		mc_clock_disable();
	}

	return ret;
}

static ssize_t debug_tee_worker_read(struct file *file, char __user *buffer,
				     size_t buffer_len, loff_t *ppos)
{
	char worker_buffer[512];
	int worker_id, ret;

	if (*ppos)
		goto exit;

	memset(l_ctx.struct_workers_counters_buf, 0,
	       sizeof(l_ctx.struct_workers_counters_buf));
	memset(worker_buffer, 0, sizeof(worker_buffer));
	for (worker_id = 0; worker_id < NQ_TEE_WORKER_THREADS; worker_id++) {
		ret = snprintf(worker_buffer,
			       sizeof(worker_buffer),
			       "tee_worker[%d]:\n"
			       "nyield: %lu\n"
			       "syield: %lu\n"
			       "busy:   %lu\n"
			       "resume: %lu\n",
			       worker_id,
		tee_worker_counter_get(worker_id, TEE_WORKER_COUNTER_NYIELD),
		tee_worker_counter_get(worker_id, TEE_WORKER_COUNTER_SYIELD),
		tee_worker_counter_get(worker_id, TEE_WORKER_COUNTER_BUSY),
		tee_worker_counter_get(worker_id, TEE_WORKER_COUNTER_RESUME));
		if (ret <= 0)
			break;
		strlcat(l_ctx.struct_workers_counters_buf, worker_buffer,
			sizeof(l_ctx.struct_workers_counters_buf));
	}

	l_ctx.struct_workers_counters_buf_len =
		strlen(l_ctx.struct_workers_counters_buf);

exit:
	return simple_read_from_buffer(buffer, buffer_len, ppos,
				       l_ctx.struct_workers_counters_buf,
				       l_ctx.struct_workers_counters_buf_len);
}

static const struct file_operations mc_debug_tee_worker_ops = {
	.read = debug_tee_worker_read
};

int nq_start(void)
{
	char worker_name[15];
	int ret, cnt;

	/* Make sure we have the interrupt before going on */
#if defined(CONFIG_OF)
	l_ctx.irq = irq_of_parse_and_map(g_ctx.mcd->of_node, 0);
	mc_dev_info("SSIQ from dts is 0x%08x", l_ctx.irq);
#endif
#if defined(MC_INTR_SSIQ)
	if (l_ctx.irq <= 0)
		l_ctx.irq = MC_INTR_SSIQ;
#endif

	if (l_ctx.irq <= 0) {
		ret = -EINVAL;
		mc_dev_err(ret, "No IRQ number, aborting");
		return ret;
	}

	ret = request_irq(l_ctx.irq, irq_handler, IRQF_TRIGGER_RISING,
			  "trustonic", NULL);
	if (ret)
		return ret;

#define MC_DISABLE_IRQ_WAKEUP
#ifdef MC_DISABLE_IRQ_WAKEUP
	mc_dev_info("irq_set_irq_wake on irq %u disabled", l_ctx.irq);
#else
	ret = irq_set_irq_wake(l_ctx.irq, 1);
	if (ret) {
		mc_dev_err(ret, "irq_set_irq_wake error on irq %u",
			   l_ctx.irq);
		return ret;
	}
#endif

	/* Enable TEE clock */
	mc_clock_enable();

	/*
	 * Initialize the time structure for SWd
	 * At this stage, we don't know if the SWd needs to get the REE time and
	 * we set it anyway.
	 */
	nq_update_time();

	/* Init TEE workers wait queue */
	init_waitqueue_head(&l_ctx.workers_wq);

	/* Setup S-SIQ interrupt handler and its bottom-half */
	l_ctx.irq_bh_thread_run = true;
	l_ctx.irq_bh_thread = kthread_run(irq_bh_worker, NULL, "tee_irq_bh");
	if (IS_ERR(l_ctx.irq_bh_thread)) {
		ret = PTR_ERR(l_ctx.irq_bh_thread);
		mc_dev_err(ret, "irq_bh_worker thread creation failed");
		return ret;
	}

	/* Logging */
	if (l_ctx.log_buffer_size) {
		cpumask_t old_affinity = tee_set_affinity();

		ret = fc_trace_init(l_ctx.log_buffer, l_ctx.log_buffer_size);
		tee_restore_affinity(old_affinity);
		if (!ret) {
			logging_run();
			l_ctx.log_buffer_busy = true;
			mc_dev_info("registered log buffer of size %d",
				    l_ctx.log_buffer_size);
		} else {
			mc_dev_err(ret, "failed to register log buffer");
			/* Ignore error */
			ret = 0;
		}
	} else {
		mc_dev_info("no log buffer to register");
	}

	/* Bootup */
	ret = nq_boot_tee();
	if (ret) {
		mc_clock_disable();
		return ret;
	}

	/* Init TEE workers */
	atomic_set(&l_ctx.workers_started, 0);
	atomic_set(&l_ctx.workers_run, 0);
	l_ctx.tee_scheduler_run = true;

	for (cnt = 0; cnt < NQ_TEE_WORKER_THREADS; cnt++) {
		snprintf(worker_name, 13, "tee_worker/%d", cnt);
		l_ctx.tee_worker[cnt] = kthread_create(tee_worker,
						       (void *)((uintptr_t)cnt),
						       worker_name);

		if (IS_ERR(l_ctx.tee_worker[cnt])) {
			ret = PTR_ERR(l_ctx.tee_worker[cnt]);
			mc_dev_err(ret, "tee_worker thread creation failed");
			return ret;
		}

		wake_up_process(l_ctx.tee_worker[cnt]);
	}

	/* Create worker debugfs entry */
	debugfs_create_file("workers_counters", 0600, g_ctx.debug_dir, NULL,
			    &mc_debug_tee_worker_ops);

	return 0;
}

void nq_stop(void)
{
	u32 worker;

	/* Make all workers exit */
	l_ctx.tee_scheduler_run = false;
	wake_up_all(&l_ctx.workers_wq);

	for (worker = 0; worker < NQ_TEE_WORKER_THREADS; worker++)
		kthread_stop(l_ctx.tee_worker[worker]);

	/* NQ */
	l_ctx.irq_bh_thread_run = false;
	complete(&l_ctx.irq_bh_complete);
	kthread_stop(l_ctx.irq_bh_thread);
	free_irq(l_ctx.irq, NULL);
}

static ssize_t debug_tee_affinity_write(struct file *file,
					const char __user *buffer,
					size_t buffer_len, loff_t *x)
{
	long tee_affinity = 0;

	/* Invalid data, nothing to do */
	if (buffer_len < 1)
		return -EINVAL;

	if (kstrtol_from_user(buffer, buffer_len, 16, &tee_affinity))
		return -EINVAL;

	if (!tee_affinity)
		return -EINVAL;

	mc_dev_devel("aff = 0x%lx, mask = 0x%lx",
		     tee_affinity,
		     l_ctx.default_affinity_mask);
	tee_affinity &= l_ctx.default_affinity_mask;
	if (!tee_affinity) {
		mc_dev_devel("Can't have an empty affinity.");
		mc_dev_devel("Restoring the default mask");
		tee_affinity = l_ctx.default_affinity_mask;
	}

	mc_dev_devel("tee_affinity 0x%lx", tee_affinity);
	atomic_set(&l_ctx.tee_affinity, tee_affinity);

	return buffer_len;
}

static ssize_t debug_tee_affinity_read(struct file *file, char __user *buffer,
				       size_t buffer_len, loff_t *ppos)
{
	char cpu_str[8];
	int ret = 0;

	ret = snprintf(cpu_str, sizeof(cpu_str), "%lx\n",
		       get_tee_affinity());
	if (ret < 0)
		return -EINVAL;

	return simple_read_from_buffer(buffer, buffer_len, ppos,
				       cpu_str, ret);
}

#if defined(BIG_CORE_SWITCH_AFFINITY_MASK)
void set_tee_worker_threads_on_big_core(bool big_core)
{
	mc_dev_devel("%s", big_core ? "big_affinity" : "default_affinity");

	if (big_core) {
		atomic_inc(&l_ctx.big_core_demand_cnt);
		atomic_set(&l_ctx.tee_affinity, BIG_CORE_SWITCH_AFFINITY_MASK);
	} else {
		/* decrease state in case of several overlapping requests */
		if (atomic_dec_if_positive(&l_ctx.big_core_demand_cnt) <= 0)
			atomic_set(&l_ctx.tee_affinity,
				   l_ctx.default_affinity_mask);
	}
}
#endif

static const struct file_operations mc_debug_tee_affinity_ops = {
	.write = debug_tee_affinity_write,
	.read = debug_tee_affinity_read,
};

int nq_init(void)
{
	size_t q_len, mci_len;
	unsigned long mci;
	int ret;

	ret = mc_clock_init();
	if (ret)
		goto err_clock;

	ret = logging_init(&l_ctx.log_buffer, &l_ctx.log_buffer_size);
	if (ret)
		goto err_logging;

	/* Setup crash handler function list */
	BLOCKING_INIT_NOTIFIER_HEAD(&l_ctx.tee_stop_notifiers);

	mutex_init(&l_ctx.buffer_mutex);
	init_completion(&l_ctx.irq_bh_complete);
	/* Setup notification queue mutex */
	mutex_init(&l_ctx.notifications_mutex);
	INIT_LIST_HEAD(&l_ctx.notifications);
	mutex_init(&l_ctx.mcp_time_mutex);

	/* NQ_NUM_ELEMS must be power of 2 */
	q_len = ALIGN(2 * (sizeof(struct notification_queue_header) +
			   NQ_NUM_ELEMS * sizeof(struct notification)), 4);

	mci_len = q_len +
		sizeof(*l_ctx.time) +
		sizeof(*l_ctx.mcp_buffer) +
		MAX_IW_SESSION * sizeof(struct interworld_session);

	l_ctx.order = get_order(mci_len);

	mci = __get_free_pages(GFP_USER | __GFP_ZERO, l_ctx.order);
	if (!mci)
		goto err_mci;

	l_ctx.nq.tx = (struct notification_queue *)mci;
	l_ctx.nq.tx->hdr.queue_size = NQ_NUM_ELEMS;
	mci += sizeof(struct notification_queue_header) +
	    l_ctx.nq.tx->hdr.queue_size * sizeof(struct notification);

	l_ctx.nq.rx = (struct notification_queue *)mci;
	l_ctx.nq.rx->hdr.queue_size = NQ_NUM_ELEMS;
	mci += sizeof(struct notification_queue_header) +
	    l_ctx.nq.rx->hdr.queue_size * sizeof(struct notification);

	l_ctx.mcp_buffer = (void *)ALIGN(mci, 8);
	mci += sizeof(struct mcp_buffer);

	/* interworld_buffer contains:
	 *   MAX_IW_SESSION session, and for each session S(i), we could have
	 *   D(i) extra data, NB: D(i) could be different from D(j)
	 *
	 * v0: D(i) = 0
	 */
	/* mci should be already 8 bytes aligned */
	l_ctx.iwp_buffer = (void *)ALIGN(mci, 8);
	mci += MAX_IW_SESSION * sizeof(struct interworld_session);

	l_ctx.time = (void *)ALIGN(mci, 8);

	#if defined(PLAT_DEFAULT_TEE_AFFINITY_MASK)
	l_ctx.default_affinity_mask = PLAT_DEFAULT_TEE_AFFINITY_MASK;
	#else
	l_ctx.default_affinity_mask = (1 << nr_cpu_ids) - 1;
	#endif

#if defined(BIG_CORE_SWITCH_AFFINITY_MASK)
	atomic_set(&l_ctx.big_core_demand_cnt, 0);
#endif

	mc_dev_devel("Default affinity : %lx", l_ctx.default_affinity_mask);
	atomic_set(&l_ctx.tee_affinity, l_ctx.default_affinity_mask);
	/* Create tee affinity debugfs entry */
	debugfs_create_file("tee_affinity_mask", 0600, g_ctx.debug_dir, NULL,
			    &mc_debug_tee_affinity_ops);
	return 0;

err_mci:
	logging_exit(l_ctx.log_buffer_busy);
err_logging:
	mc_clock_exit();
err_clock:
	return ret;
}

void nq_exit(void)
{
	if (l_ctx.dump.off)
		kfree(l_ctx.dump.buf);

	free_pages((unsigned long)l_ctx.mci, l_ctx.order);
	logging_exit(l_ctx.log_buffer_busy);
	mc_clock_exit();
}

#ifdef MC_TEE_HOTPLUG
int nq_cpu_off(unsigned int cpu)
{
	int err;
	unsigned long tee_affinity = get_tee_affinity();
	unsigned long new_affinity = tee_affinity & (~(1 << cpu));
	cpumask_t old_affinity;

	mc_dev_devel("%s cpu %d tee_affinity = %lx new_affinity = %lx",
		     __func__,
		     cpu,
		     tee_affinity,
		     new_affinity);

	old_affinity = current->cpus_allowed;
	set_cpus_allowed_ptr(current, to_cpumask(&new_affinity));

	err = fc_cpu_off();
	set_cpus_allowed_ptr(current, &old_affinity);

	return err;
}

int nq_cpu_on(unsigned int cpu)
{
	return 0;
}
#endif
