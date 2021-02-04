/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
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
#include <linux/version.h>
#include <linux/sched.h>

#include "platform.h"			/* CPU-related information */

#include "public/mc_user.h"

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
#define SCHEDULING_FREQ		5	/**< N-SIQ every n-th time */
#define DEFAULT_TIMEOUT_MS	20000	/* We do nothing on timeout anyway */

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

	/* Scheduler */
	struct task_struct	*tee_scheduler_thread;
	bool			tee_scheduler_run;
	bool			tee_hung;
	int			boot_ret;
	struct completion	boot_complete;	/* Signal end of boot */
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

	/* Logging */
	phys_addr_t		log_buffer;
	u32			log_buffer_size;
	bool			log_buffer_busy;
} l_ctx;

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

static void retrieve_last_session_payload(u32 *session_id, u32 *payload)
{
	struct notification_queue_header *hdr = &l_ctx.nq.tx->hdr;
	u32 i = (hdr->write_cnt - 1) % hdr->queue_size;

	*session_id = l_ctx.nq.tx->notification[i].session_id;
	*payload = l_ctx.nq.tx->notification[i].payload;
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

static int nq_scheduler_command(enum sched_command command)
{
	if (IS_ERR_OR_NULL(l_ctx.tee_scheduler_thread))
		return -EFAULT;

	mutex_lock(&l_ctx.request_mutex);
	if (l_ctx.request < command) {
		l_ctx.request = command;
		complete(&l_ctx.idle_complete);
	}

	mutex_unlock(&l_ctx.request_mutex);
	return 0;
}

static inline void nq_update_time(void)
{
	struct timespec tm;

	getnstimeofday(&tm);
	l_ctx.time->wall_clock_seconds = tm.tv_sec;
	l_ctx.time->wall_clock_nsec = tm.tv_nsec;
	getrawmonotonic(&tm);
	l_ctx.time->monotonic_seconds = tm.tv_sec;
	l_ctx.time->monotonic_nsec = tm.tv_nsec;
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

		/*
		 * Finished processing notifications. It does not matter whether
		 * there actually were any notification or not.  S-SIQs can also
		 * be triggered by an SWd driver which was waiting for a FIQ.
		 * In this case the S-SIQ tells NWd that SWd is no longer idle
		 * an will need scheduling again.
		 */
		nq_scheduler_command(NSIQ);
	}
	return 0;
}

static irqreturn_t irq_handler(int intr, void *arg)
{
	/* wake up thread to continue handling this interrupt */
	complete(&l_ctx.irq_bh_complete);
	return IRQ_HANDLED;
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

		if (nq_scheduler_command(YIELD))
			ret = -EPROTO;
	} else {
		mc_dev_devel("send %x payload %x", session->id, payload);
		notif_queue_push(session->id, payload);
		session_state_update_internal(session, NQ_NOTIF_SENT);
		if (nq_scheduler_command(NSIQ))
			ret = -EPROTO;
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
	for (i = 0; i < (size_t)ARRAY_SIZE(status_map); i++) {
		u32 info;

		if (fc_info(status_map[i].index, NULL, &info))
			return;

		mc_dev_info("  %-22s= 0x%08x", status_map[i].msg, info);
		if (ret >= 0)
			ret = kasnprintf(&l_ctx.dump, "%-22s= 0x%08x\n",
					 status_map[i].msg, info);
	}

	/* construct UUID string */
	for (i = 0; i < 4; i++) {
		u32 info;
		size_t j;

		if (fc_info(MC_EXT_INFO_ID_MC_EXC_UUID + i, NULL, &info))
			return;

		for (j = 0; j < sizeof(info); j++) {
			snprintf(&uuid_str[(i * sizeof(info) + j) * 2], 3,
				 "%02x", (info >> (j * 8)) & 0xff);
		}
	}

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

static inline void set_sleep_mode_rq(u16 sleep_req)
{
	mutex_lock(&l_ctx.buffer_mutex);
	l_ctx.mcp_buffer->flags.sleep_mode.sleep_req = sleep_req;
	mutex_unlock(&l_ctx.buffer_mutex);
}

static inline bool nq_suspended(void)
{
	struct mcp_flags *flags = &l_ctx.mcp_buffer->flags;
	bool ret;

	mutex_lock(&l_ctx.buffer_mutex);
	ret = flags->sleep_mode.ready_to_sleep & MC_STATE_READY_TO_SLEEP;
	if (!ret) {
		mc_dev_devel("IDLE=%d", flags->schedule);
		mc_dev_devel("Request Sleep=%d", flags->sleep_mode.sleep_req);
		mc_dev_devel("Sleep Ready=%d",
			     flags->sleep_mode.ready_to_sleep);
	}

	mutex_unlock(&l_ctx.buffer_mutex);
	return ret;
}

/*
 * Get the requested SWd sleep timeout value (ms)
 * - if the timeout is -1, wait indefinitely
 * - if the timeout is 0, re-schedule immediately (timeouts in µs in the SWd)
 * - otherwise sleep for the required time
 * returns true if sleep is required, false otherwise
 */
static inline bool nq_get_idle_timeout(s32 *timeout)
{
	u32 schedule;
	bool ret;

	mutex_lock(&l_ctx.buffer_mutex);
	schedule = l_ctx.mcp_buffer->flags.schedule;
	if (schedule == MC_FLAG_SCHEDULE_IDLE) {
		*timeout = l_ctx.mcp_buffer->flags.timeout_ms;
		ret = true;
	} else {
		ret = false;
	}

	mutex_unlock(&l_ctx.buffer_mutex);
	return ret;
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
	complete(&l_ctx.idle_complete);
	nq_scheduler_command(NONE);
}

static int nq_scheduler_pm_command(enum sched_command command)
{
	int ret = -EPERM;

	if (IS_ERR_OR_NULL(l_ctx.tee_scheduler_thread))
		return -EFAULT;

	mutex_lock(&l_ctx.sleep_mutex);

	/* Send request */
	nq_scheduler_command(command);

	/* Wait for scheduler to reply */
	wait_for_completion(&l_ctx.sleep_complete);
	mutex_lock(&l_ctx.request_mutex);
	if (command == SUSPEND) {
		if (l_ctx.suspended)
			ret = 0;
	} else {
		if (!l_ctx.suspended)
			ret = 0;
	}

	mutex_unlock(&l_ctx.request_mutex);
	mutex_unlock(&l_ctx.sleep_mutex);
	return ret;
}

static int nq_boot_tee(void)
{
	size_t q_len = ALIGN(2 * (sizeof(struct notification_queue_header) +
		NQ_NUM_ELEMS * sizeof(struct notification)), 4);
	struct irq_data *irq_d = irq_get_irq_data(l_ctx.irq);
	int ret;

	/* Call the INIT fastcall to setup shared buffers */
	ret = fc_init(virt_to_phys(l_ctx.mci),
		      (uintptr_t)l_ctx.mcp_buffer - (uintptr_t)l_ctx.mci, q_len,
		      sizeof(*l_ctx.mcp_buffer));
	logging_run();
	if (ret)
		return ret;

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
		return ret;

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
			return ret;

		switch (status) {
		case MC_STATUS_NOT_INITIALIZED:
			/* Switch to the TEE to give it more CPU time. */
			ret = EAGAIN;
			for (timeslice = 0; timeslice < 10; timeslice++) {
				int tmp_ret = fc_yield(timeslice);

				logging_run();
				if (tmp_ret)
					return tmp_ret;
			}

			/* No need to loop like mad */
			if (ret == EAGAIN)
				usleep_range(100, 500);

			break;
		case MC_STATUS_HALT:
			ret = -ENODEV;
			nq_handle_tee_crash();
			mc_dev_err(ret, "halt during init, state 0x%x", status);
			return ret;
		case MC_STATUS_INITIALIZED:
			mc_dev_devel("ready");
			break;
		default:
			/* MC_STATUS_BAD_INIT or anything else */
			ret = -EIO;
			mc_dev_err(ret, "MCI init failed, state 0x%x", status);
			return ret;
		}
	} while (ret == EAGAIN);

	return ret;
}

static inline bool tee_sleep(s32 timeout_ms)
{
	bool infinite_timeout = timeout_ms < 0;

	/* TEE is going to sleep */
	mc_clock_disable();
	do {
		s32 local_timeout_ms;
		unsigned long jiffies;

		if (infinite_timeout) {
			local_timeout_ms = DEFAULT_TIMEOUT_MS;
		} else {
			local_timeout_ms = timeout_ms;
			if (local_timeout_ms > DEFAULT_TIMEOUT_MS)
				local_timeout_ms = DEFAULT_TIMEOUT_MS;
		}

		jiffies = msecs_to_jiffies(local_timeout_ms);
		if (wait_for_completion_timeout(&l_ctx.idle_complete, jiffies))
			break;

		if (!infinite_timeout)
			timeout_ms -= local_timeout_ms;
	} while (timeout_ms);

	/* TEE is getting back to work */
	mc_clock_enable();
	return timeout_ms == 0;
}

/*
 * This thread, and only this thread, schedules the SWd. Hence, reading the idle
 * status and its associated timeout is safe from race conditions.
 */
static int tee_scheduler(void *arg)
{
	bool swd_notify = false;
	int ret = 0;

	/* Enable TEE clock */
	mc_clock_enable();

	/* Logging */
	if (l_ctx.log_buffer_size) {
		ret = fc_trace_init(l_ctx.log_buffer, l_ctx.log_buffer_size);
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
	l_ctx.boot_ret = nq_boot_tee();
	complete(&l_ctx.boot_complete);
	if (l_ctx.boot_ret) {
		mc_clock_disable();
		return l_ctx.boot_ret;
	}

	/* Run */
	while (1) {
		s32 timeout_ms = -1;
		bool pm_request = false;
		u8 tee_flags;

		if (l_ctx.suspended || nq_get_idle_timeout(&timeout_ms)) {
			/* If timeout is 0 we keep scheduling the SWd */
			if (!timeout_ms)
				nq_scheduler_command(NSIQ);
			else if (tee_sleep(timeout_ms))
				/* Timed out, force SWd schedule */
				nq_scheduler_command(NSIQ);
		}

		/*
		 * Potential exit causes:
		 * 1) nq_stop is called: just stop the thread (no crash dump)
		 * 2) nq_signal_tee_hung: breaks the loop and handle the hang as
		 *    a crash
		 * 3) The thread detects a TEE crash and breaks the loop
		 */
		if (!l_ctx.tee_scheduler_run)
			break;

		/* Get requested command if any */
		mutex_lock(&l_ctx.request_mutex);
		switch (l_ctx.request) {
		case NONE:
			break;
		case YIELD:
			swd_notify = false;
			break;
		case NSIQ:
			swd_notify = true;
			break;
		case SUSPEND:
			/* Force N_SIQ */
			swd_notify = true;
			set_sleep_mode_rq(MC_FLAG_REQ_TO_SLEEP);
			pm_request = true;
			break;
		case RESUME:
			/* Force N_SIQ */
			swd_notify = true;
			set_sleep_mode_rq(MC_FLAG_NO_SLEEP_REQ);
			pm_request = true;
			break;
		}

		l_ctx.request = NONE;
		nq_update_time();
		mutex_unlock(&l_ctx.request_mutex);

		/* Reset timeout so we don't loop if SWd halted */
		mutex_lock(&l_ctx.buffer_mutex);
		l_ctx.mcp_buffer->flags.timeout_ms = -1;
		mutex_unlock(&l_ctx.buffer_mutex);

		if (swd_notify) {
			u32 session_id = 0;
			u32 payload = 0;

			retrieve_last_session_payload(&session_id, &payload);
			swd_notify = false;

			/* Call SWd scheduler */
			fc_nsiq(session_id, payload);
		} else {
			/* Resume SWd from where it was */
			fc_yield(0);
		}

		/* Always flush log buffer after the SWd has run */
		logging_run();

		/* Check crash */
		mutex_lock(&l_ctx.buffer_mutex);
		tee_flags = l_ctx.mcp_buffer->flags.tee_flags;
		mutex_unlock(&l_ctx.buffer_mutex);
		if (tee_flags & MC_STATE_FLAG_TEE_HALT_MASK) {
			ret = -EHOSTUNREACH;
			mc_dev_err(ret, "TEE halted, exiting");
			break;
		}

		/* Should have suspended by now if requested */
		mutex_lock(&l_ctx.request_mutex);
		if (pm_request) {
			l_ctx.suspended = nq_suspended();
			complete(&l_ctx.sleep_complete);
		}

		mutex_unlock(&l_ctx.request_mutex);

		/* Flush pending notifications if possible */
		mutex_lock(&l_ctx.notifications_mutex);
		if (nq_notifications_flush())
			complete(&l_ctx.idle_complete);

		mutex_unlock(&l_ctx.notifications_mutex);
	}

	mc_dev_devel("loop exit, ret is %d", ret);
	if (ret || l_ctx.tee_hung) {
		/* There is an error, the tee must have crashed */
		nq_handle_tee_crash();
	}

	/* Logging */
	ret = fc_trace_deinit();
	if (!ret)
		l_ctx.log_buffer_busy = false;
	else
		mc_dev_err(ret, "failed to unregister log buffer");

	mc_clock_disable();
	return ret;
}

int nq_suspend(void)
{
	return nq_scheduler_pm_command(SUSPEND);
}

int nq_resume(void)
{
	return nq_scheduler_pm_command(RESUME);
}

int nq_start(void)
{
	int ret;
#if defined(CPU_IDS)
	struct cpumask new_mask;
	unsigned int cpu_id[] = CPU_IDS;
	int i;
#endif
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

	/*
	 * Initialize the time structure for SWd
	 * At this stage, we don't know if the SWd needs to get the REE time and
	 * we set it anyway.
	 */
	nq_update_time();

	/* Setup S-SIQ interrupt handler and its bottom-half */
	l_ctx.irq_bh_thread_run = true;
	l_ctx.irq_bh_thread = kthread_run(irq_bh_worker, NULL, "tee_irq_bh");
	if (IS_ERR(l_ctx.irq_bh_thread)) {
		ret = PTR_ERR(l_ctx.irq_bh_thread);
		mc_dev_err(ret, "irq_bh_worker thread creation failed");
		return ret;
	}

	/* Scheduler */
	l_ctx.tee_scheduler_run = true;
	l_ctx.tee_scheduler_thread = kthread_create(tee_scheduler, NULL,
						    "tee_scheduler");
	if (IS_ERR(l_ctx.tee_scheduler_thread)) {
		ret = PTR_ERR(l_ctx.tee_scheduler_thread);
		mc_dev_err(ret, "tee_scheduler thread creation failed");
		return ret;
	}
#if defined(CPU_IDS)
	cpumask_clear(&new_mask);
	for (i = 0; i < NB_CPU; i++)
		cpumask_set_cpu(cpu_id[i], &new_mask);
	set_cpus_allowed_ptr(l_ctx.tee_scheduler_thread, &new_mask);
	mc_dev_info("tee_scheduler running only on %d CPU", NB_CPU);
#endif

	wake_up_process(l_ctx.tee_scheduler_thread);

	wait_for_completion(&l_ctx.boot_complete);
	if (l_ctx.boot_ret)
		return l_ctx.boot_ret;

	complete(&l_ctx.idle_complete);
	return 0;
}

void nq_stop(void)
{
	/* Scheduler */
	l_ctx.tee_scheduler_run = false;
	complete(&l_ctx.idle_complete);
	kthread_stop(l_ctx.tee_scheduler_thread);

	/* NQ */
	l_ctx.irq_bh_thread_run = false;
	complete(&l_ctx.irq_bh_complete);
	kthread_stop(l_ctx.irq_bh_thread);
	free_irq(l_ctx.irq, NULL);
}

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

	/* Scheduler */
	init_completion(&l_ctx.boot_complete);
	init_completion(&l_ctx.idle_complete);
	init_completion(&l_ctx.sleep_complete);
	mutex_init(&l_ctx.sleep_mutex);
	mutex_init(&l_ctx.request_mutex);
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
