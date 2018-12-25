/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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
#include <linux/version.h>
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <linux/sched/clock.h>	/* local_clock */
#endif

#include "public/mc_user.h"

#include "mci/mcifc.h"
#include "mci/mciiwp.h"
#include "mci/mcimcp.h"
#include "mci/mcinq.h"
#include "mci/mcitime.h"	/* struct mcp_time */
#include "main.h"
#include "fastcall.h"
#include "nq.h"

#define NQ_NUM_ELEMS		64

static struct {
	struct mutex buffer_mutex;	/* Lock on SWd communication buffer */
	struct mcp_buffer *mcp_buffer;
	struct interworld_session *iwp_buffer;
	struct task_struct *irq_bh_thread;
	struct completion irq_bh_complete;
	bool irq_bh_active;
	int irq;
	int (*scheduler_cb)(enum nq_scheduler_commands);
	void (*crash_handler_cb)(void);
	void (*mcp_notif_handler)(u32 id, u32 payload);
	void (*iwp_notif_handler)(u32 id, u32 payload);
	/* MobiCore MCI information */
	unsigned int order;
	union {
		void		*base;
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
	struct kasnprintf_buf	dump;
	/* Time */
	struct mcp_time		*time;
} l_ctx;

static inline bool is_iwp_id(u32 id)
{
	return (id & SID_IWP_NOTIFICATION) != 0;
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

static inline bool nq_notifications_flush_nolock(void)
{
	bool flushed = false;

	while (!list_empty(&l_ctx.notifications) && !notif_queue_full()) {
		struct nq_session *session;

		session = list_first_entry(&l_ctx.notifications,
					   struct nq_session, list);
		mc_dev_devel("pop %x", session->id);
		notif_queue_push(session->id, session->payload);
		nq_session_state_update(session, NQ_NOTIF_SENT);
		list_del_init(&session->list);
		flushed = true;
	}

	return flushed;
}

bool nq_notifications_flush(void)
{
	bool flushed = false;

	mutex_lock(&l_ctx.notifications_mutex);
	flushed = nq_notifications_flush_nolock();
	mutex_unlock(&l_ctx.notifications_mutex);
	return flushed;
}

int nq_session_notify(struct nq_session *session, u32 id, u32 payload)
{
	int ret = 0;

	if (!l_ctx.scheduler_cb)
		return -EAGAIN;

	mutex_lock(&l_ctx.notifications_mutex);
	session->id = id;
	session->payload = payload;
	if (!list_empty(&l_ctx.notifications) || notif_queue_full()) {
		if (!list_empty(&session->list)) {
			if (payload != session->payload) {
				mc_dev_notice("skip %x payload change %x -> %x",
					   session->id, session->payload,
					   payload);
			} else {
				mc_dev_devel("skip %x payload %x",
					     session->id, payload);
			}
			ret = -EAGAIN;
		} else {
			mc_dev_devel("push %x payload %x", session->id,
				     payload);
			/* session->payload = payload; */
			list_add_tail(&session->list, &l_ctx.notifications);
			nq_session_state_update(session, NQ_NOTIF_QUEUED);
		}

		nq_notifications_flush_nolock();

		if (l_ctx.scheduler_cb(MC_NQ_YIELD))
			ret = -EPROTO;
	} else {
		mc_dev_devel("send %x payload %x", session->id, payload);
		notif_queue_push(session->id, payload);
		nq_session_state_update(session, NQ_NOTIF_SENT);
		if (l_ctx.scheduler_cb(MC_NQ_NSIQ))
			ret = -EPROTO;
	}

	mutex_unlock(&l_ctx.notifications_mutex);
	return ret;
}

void nq_update_time(void)
{
	struct timespec tm;

	getnstimeofday(&tm);
	l_ctx.time->wall_clock_seconds = tm.tv_sec;
	l_ctx.time->wall_clock_nsec = tm.tv_nsec;
	if (g_ctx.f_monotonic_time) {
		getrawmonotonic(&tm);
		l_ctx.time->monotonic_seconds = tm.tv_sec;
		l_ctx.time->monotonic_nsec = tm.tv_nsec;
	}
}

union mcp_message *nq_get_mcp_message(void)
{
	return &l_ctx.mcp_buffer->message;
}

struct interworld_session *nq_get_iwp_buffer(void)
{
	return l_ctx.iwp_buffer;
}

void nq_register_notif_handler(void (*handler)(u32 id, u32 payload), bool iwp)
{
	if (iwp)
		l_ctx.iwp_notif_handler = handler;
	else
		l_ctx.mcp_notif_handler = handler;
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

	while (l_ctx.irq_bh_active) {
		wait_for_completion_killable(&l_ctx.irq_bh_complete);

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
		if (l_ctx.scheduler_cb)
			l_ctx.scheduler_cb(MC_NQ_NSIQ);
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

bool nq_session_is_gp(const struct nq_session *session)
{
	return session->is_gp;
}

u64 nq_session_notif_cpu_clk(const struct nq_session *session)
{
	return session->cpu_clk;
}

void nq_session_exit(struct nq_session *session)
{
	mutex_lock(&l_ctx.notifications_mutex);
	list_del(&session->list);
	mutex_unlock(&l_ctx.notifications_mutex);
}

void nq_session_state_update(struct nq_session *session,
			     enum nq_notif_state state)
{
	mutex_lock(&session->mutex);
	session->state = state;
	session->cpu_clk = local_clock();
	mutex_unlock(&session->mutex);
}

const char *nq_session_state_string(struct nq_session *session)
{
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

static const struct file_operations mc_debug_crashdump_ops = {
	.read = debug_crashdump_read,
	.llseek = default_llseek,
};

static ssize_t debug_smclog_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	return debug_generic_read(file, user_buf, count, ppos,
				  mc_fastcall_debug_smclog);
}

static const struct file_operations mc_debug_smclog_ops = {
	.read = debug_smclog_read,
	.llseek = default_llseek,
	.open = debug_generic_open,
	.release = debug_generic_release,
};

void nq_dump_status(void)
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
	};

	char uuid_str[33];
	int ret = 0;
	size_t i;

	if (l_ctx.dump.off)
		ret = -EBUSY;

	mc_dev_notice("TEE halted. Status dump:");
	for (i = 0; i < (size_t)ARRAY_SIZE(status_map); i++) {
		u32 info;

		if (!mc_fc_info(status_map[i].index, NULL, &info)) {
			mc_dev_notice("  %-20s= 0x%08x",
				status_map[i].msg, info);
			if (ret >= 0)
				ret = kasnprintf(&l_ctx.dump, "%-20s= 0x%08x\n",
						 status_map[i].msg, info);
		}
	}

	/* construct UUID string */
	for (i = 0; i < 4; i++) {
		u32 info;
		size_t j;

		if (mc_fc_info(MC_EXT_INFO_ID_MC_EXC_UUID + i, NULL, &info))
			return;

		for (j = 0; j < sizeof(info); j++) {
			snprintf(&uuid_str[(i * sizeof(info) + j) * 2], 3,
				 "%02x", (info >> (j * 8)) & 0xff);
		}
	}

	mc_dev_notice("  %-20s= 0x%s", "mcExcep.uuid", uuid_str);
	if (ret >= 0)
		ret = kasnprintf(&l_ctx.dump, "%-20s= 0x%s\n", "mcExcep.uuid",
				 uuid_str);

	if (ret < 0) {
		kfree(l_ctx.dump.buf);
		l_ctx.dump.off = 0;
		return;
	}

	debugfs_create_file("crashdump", 0400, g_ctx.debug_dir, NULL,
			    &mc_debug_crashdump_ops);
	debugfs_create_file("last_smc_commands", 0400, g_ctx.debug_dir, NULL,
			    &mc_debug_smclog_ops);
	if (l_ctx.crash_handler_cb)
		l_ctx.crash_handler_cb();
}

void nq_register_scheduler(int (*scheduler_cb)(enum nq_scheduler_commands))
{
	l_ctx.scheduler_cb = scheduler_cb;
}

void nq_register_crash_handler(void (*crash_handler_cb)(void))
{
	l_ctx.crash_handler_cb = crash_handler_cb;
}

static inline int set_sleep_mode_rq(u16 sleep_req)
{
	mutex_lock(&l_ctx.buffer_mutex);
	l_ctx.mcp_buffer->flags.sleep_mode.sleep_req = sleep_req;
	mutex_unlock(&l_ctx.buffer_mutex);
	return 0;
}

int nq_suspend(void)
{
	return set_sleep_mode_rq(MC_FLAG_REQ_TO_SLEEP);
}

int nq_resume(void)
{
	return set_sleep_mode_rq(MC_FLAG_NO_SLEEP_REQ);
}

bool nq_suspended(void)
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

bool nq_get_idle_timeout(s32 *timeout)
{
	u32 schedule;
	bool ret;

	mutex_lock(&l_ctx.buffer_mutex);
	schedule = l_ctx.mcp_buffer->flags.schedule;
	if (schedule == MC_FLAG_SCHEDULE_IDLE) {
		if (g_ctx.f_timeout)
			*timeout = l_ctx.mcp_buffer->flags.timeout_ms;
		else
			*timeout = -1;

		ret = true;
	} else {
		ret = false;
	}

	mutex_unlock(&l_ctx.buffer_mutex);
	return ret;
}

void nq_reset_idle_timeout(void)
{
	mutex_lock(&l_ctx.buffer_mutex);
	l_ctx.mcp_buffer->flags.timeout_ms = -1;
	mutex_unlock(&l_ctx.buffer_mutex);
}

int nq_start(void)
{
	size_t q_len = ALIGN(2 * (sizeof(struct notification_queue_header) +
		NQ_NUM_ELEMS * sizeof(struct notification)), 4);
	int ret;
	struct irq_data *irq_d;

	/* Make sure we have an interrupt number before going on */
#if defined(CONFIG_OF)
	l_ctx.irq = irq_of_parse_and_map(g_ctx.mcd->of_node, 0);
	mc_dev_info("SSIQ from dts is 0x%08x", l_ctx.irq);
#endif
#if defined(MC_INTR_SSIQ)
	mc_dev_info("MC_INTR_SSIQ is 0x%08x", MC_INTR_SSIQ);
	if (l_ctx.irq <= 0)
		l_ctx.irq = MC_INTR_SSIQ;
#endif

	if (l_ctx.irq <= 0) {
		mc_dev_notice("No IRQ number, aborting");
		return -EINVAL;
	}

	mc_dev_info("FINAL SSIQ is 0x%08x\n", l_ctx.irq);

	/*
	 * Initialize the time structure for SWd
	 * At this stage, we don't know if the SWd needs to get the REE time and
	 * we set it anyway.
	 */
	nq_update_time();

	/* Call the INIT fastcall to setup shared buffers */
	ret = mc_fc_init(
		virt_to_phys(l_ctx.base),
		(uintptr_t)l_ctx.mcp_buffer - (uintptr_t)l_ctx.base,
		q_len,
		sizeof(*l_ctx.mcp_buffer));

	if (ret)
		return ret;

	/* Set initialization values */
#if defined(MC_INTR_SSIQ_SWD)
	l_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_IRQ;
	l_ctx.mcp_buffer->message.init_values.irq = MC_INTR_SSIQ_SWD;
#endif
	l_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_TIME;
	irq_d = irq_get_irq_data(l_ctx.irq);
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
		(u32)((uintptr_t)l_ctx.time - (uintptr_t)l_ctx.base);
	l_ctx.mcp_buffer->message.init_values.time_len =
			sizeof(*l_ctx.time);

	l_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_IWP;
	l_ctx.mcp_buffer->message.init_values.iws_buf_ofs =
		(u64)((uintptr_t)l_ctx.iwp_buffer - (uintptr_t)l_ctx.base);
	l_ctx.mcp_buffer->message.init_values.iws_buf_size =
		MAX_IW_SESSION * sizeof(struct interworld_session);

	/* First empty N-SIQ to setup of the MCI structure */
	ret = mc_fc_nsiq();
	if (ret)
		return ret;

	/*
	 * Wait until the TEE state switches to MC_STATUS_INITIALIZED
	 * It is assumed that it always switches state at some point
	 */
	do {
		u32 status = 0;
		u32 timeslot;

		ret = mc_fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL);
		if (ret)
			return ret;

		switch (status) {
		case MC_STATUS_NOT_INITIALIZED:
			/* Switch to the TEE to give it more CPU time. */
			ret = EAGAIN;
			for (timeslot = 0; timeslot < 10; timeslot++) {
				int tmp_ret = mc_fc_yield();

				if (tmp_ret)
					return tmp_ret;
			}

			/* No need to loop like mad */
			if (ret == EAGAIN)
				usleep_range(100, 500);

			break;
		case MC_STATUS_HALT:
			nq_dump_status();
			mc_dev_notice("halt during init, state 0x%x", status);
			return -ENODEV;
		case MC_STATUS_INITIALIZED:
			mc_dev_devel("ready");
			break;
		default:
			/* MC_STATUS_BAD_INIT or anything else */
			mc_dev_notice("MCI init failed, state 0x%x", status);
			return -EIO;
		}
	} while (ret == EAGAIN);

	/* Set up S-SIQ interrupt handler and its bottom-half */
	l_ctx.irq_bh_active = true;
	l_ctx.irq_bh_thread = kthread_run(irq_bh_worker, NULL, "tee_irq_bh");
	if (IS_ERR(l_ctx.irq_bh_thread)) {
		mc_dev_notice("irq_bh_worker thread creation failed");
		return PTR_ERR(l_ctx.irq_bh_thread);
	}
	set_user_nice(l_ctx.irq_bh_thread, -20);
	return request_irq(l_ctx.irq, irq_handler, IRQF_TRIGGER_RISING,
			   "trustonic", NULL);
}

void nq_stop(void)
{
	free_irq(l_ctx.irq, NULL);
	l_ctx.irq_bh_active = false;
	kthread_stop(l_ctx.irq_bh_thread);
}

int nq_init(void)
{
	size_t q_len, buf_len;
	unsigned long mci;

	mutex_init(&l_ctx.buffer_mutex);
	init_completion(&l_ctx.irq_bh_complete);
	/* Setup notification queue mutex */
	mutex_init(&l_ctx.notifications_mutex);
	INIT_LIST_HEAD(&l_ctx.notifications);

	/* NQ_NUM_ELEMS must be power of 2 */
	q_len = ALIGN(2 * (sizeof(struct notification_queue_header) +
			   NQ_NUM_ELEMS * sizeof(struct notification)), 4);

	buf_len = q_len +
		sizeof(*l_ctx.time) +
		sizeof(*l_ctx.mcp_buffer) +
		MAX_IW_SESSION * sizeof(struct interworld_session);

	l_ctx.order = get_order(buf_len);

	mci = __get_free_pages(GFP_USER | __GFP_ZERO, l_ctx.order);
	if (!mci)
		return -ENOMEM;

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
	if ((unsigned long)l_ctx.iwp_buffer != mci)
		mc_dev_notice("ERROR: iws buffer is not aligned");

	mci += MAX_IW_SESSION * sizeof(struct interworld_session);

	l_ctx.time = (void *)ALIGN(mci, 8);
	return 0;
}

void nq_exit(void)
{
	if (l_ctx.dump.off)
		kfree(l_ctx.dump.buf);
	free_pages((unsigned long)l_ctx.base, l_ctx.order);
}
