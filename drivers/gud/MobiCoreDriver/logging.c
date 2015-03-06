/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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

#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "public/mc_linux.h"

#include "mci/mcifc.h"
#include "mci/mcinq.h"
#include "mci/mcimcp.h"

#include "main.h"
#include "fastcall.h"
#include "logging.h"

#ifdef MC_MEM_TRACES

/* Supported log buffer version */
#define MC_LOG_VERSION			2

/* Default length of the log ring buffer 256KiB */
#define LOG_BUF_SIZE			(64 * PAGE_SIZE)

/* Max Len of a log line for printing */
#define LOG_LINE_SIZE			256

static uint32_t g_log_size = LOG_BUF_SIZE;

/* Definitions for log version 2 */
#define LOG_TYPE_MASK			(0x0007)
#define LOG_TYPE_CHAR			0
#define LOG_TYPE_INTEGER		1

/* Field length */
#define LOG_LENGTH_MASK			(0x00F8)
#define LOG_LENGTH_SHIFT		3

/* Extra attributes */
#define LOG_EOL				(0x0100)
#define LOG_INTEGER_DECIMAL		(0x0200)
#define LOG_INTEGER_SIGNED		(0x0400)

struct mc_logmsg {
	uint16_t ctrl;		/* Type and format of data */
	uint16_t source;	/* Unique value for each event source */
	uint32_t log_data;	/* Value, if any */
};

/* MobiCore internal trace buffer structure. */
struct mc_trace_buf {
	uint32_t version;	/* version of trace buffer */
	uint32_t length;	/* length of buff */
	uint32_t head;		/* last write position */
	uint8_t buff[];		/* start of the log buffer */
};

struct mc_log_ctx {
	struct task_struct *thread;	/* Log Thread task structure */
	struct mc_trace_buf *trace_buf;	/* Log circular buffer (with header) */
	uint32_t tail;			/* MobiCore log read position */
	uint32_t line_len;		/* Log Line buffer current length */
	int thread_err;
	uint16_t prev_source;		/* Previous Log source */
	char line[LOG_LINE_SIZE];	/* Log Line buffer */
};

static inline void log_eol(uint16_t source)
{
	struct mc_log_ctx *log = g_ctx.log;

	if (!strnlen(log->line, LOG_LINE_SIZE)) {
		/* In case a TA tries to print a 0x0 */
		log->line_len = 0;
		return;
	}

	if (log->prev_source)
		/* MobiCore Userspace */
		dev_info(g_ctx.mcd, "%03x|%s\n", log->prev_source, log->line);
	else
		/* MobiCore kernel */
		dev_info(g_ctx.mcd, "%s\n", log->line);

	log->line_len = 0;
	log->line[0] = 0;
}

/*
 * Collect chars in log->line buffer and output the buffer when it is full.
 * No locking needed because only "mobicore_log" thread updates this buffer.
 */
static inline void log_char(char ch, uint16_t source)
{
	struct mc_log_ctx *log = g_ctx.log;

	if (ch == '\n' || ch == '\r') {
		log_eol(source);
		return;
	}

	if (log->line_len >= (LOG_LINE_SIZE - 1) || source != log->prev_source)
		log_eol(source);

	log->line[log->line_len++] = ch;
	log->line[log->line_len] = 0;
	log->prev_source = source;
}

static inline void log_string(uint32_t ch, uint16_t source)
{
	while (ch) {
		log_char(ch & 0xFF, source);
		ch >>= 8;
	}
}

static inline void log_number(uint32_t format, uint32_t value, uint16_t source)
{
	int width = (format & LOG_LENGTH_MASK) >> LOG_LENGTH_SHIFT;
	char fmt[16];
	char buffer[32];
	const char *reader = buffer;

	if (format & LOG_INTEGER_DECIMAL)
		if (format & LOG_INTEGER_SIGNED)
			snprintf(fmt, sizeof(fmt), "%%%ud", width);
		else
			snprintf(fmt, sizeof(fmt), "%%%uu", width);
	else
		snprintf(fmt, sizeof(fmt), "%%0%ux", width);

	snprintf(buffer, sizeof(buffer), fmt, value);
	while (*reader)
		log_char(*reader++, source);
}

static inline int log_msg(void *data)
{
	struct mc_logmsg *msg = (struct mc_logmsg *)data;
	int log_type = msg->ctrl & LOG_TYPE_MASK;

	switch (log_type) {
	case LOG_TYPE_CHAR:
		log_string(msg->log_data, msg->source);
		break;
	case LOG_TYPE_INTEGER:
		log_number(msg->ctrl, msg->log_data, msg->source);
		break;
	}
	if (msg->ctrl & LOG_EOL)
		log_eol(msg->source);

	return sizeof(*msg);
}

/* log_worker() - Worker thread processing the log->buf buffer. */
static int log_worker(void *arg)
{
	struct mc_log_ctx *log = (struct mc_log_ctx *)arg;
	enum {
		s_process_log,
		s_idle,
		s_wait_stop,
	} state = s_process_log;

	log->thread_err = 0;
	while (!kthread_should_stop()) {
		switch (state) {
		case s_process_log:
			if (log->trace_buf->head == log->tail) {
				state = s_idle;
				break;
			}

			if (log->trace_buf->version != MC_LOG_VERSION) {
				dev_err(g_ctx.mcd,
					"Bad log data v%d (exp. v%d), stop.\n",
					log->trace_buf->version,
					MC_LOG_VERSION);
				state = s_wait_stop;
				log->thread_err = -EFAULT;
				break;
			}

			log->tail += log_msg(&log->trace_buf->buff[log->tail]);
			/* Wrap over if no space left for a complete message */
			if ((log->tail + sizeof(struct mc_logmsg)) >
							log->trace_buf->length)
				log->tail = 0;

			break;
		case s_idle:
			state = s_process_log;
			/* no break */
		case s_wait_stop:
			set_current_state(TASK_UNINTERRUPTIBLE);
			if (!kthread_should_stop())
				schedule();

			__set_current_state(TASK_RUNNING);
		}
	}

	dev_info(g_ctx.mcd, "logging thread stopped (ret=%d)\n",
		 log->thread_err);
	return log->thread_err;
}

/*
 * Wake up the log reader thread
 * This should be called from the places where calls into MobiCore have
 * generated some logs(eg, yield, SIQ...)
 */
void mobicore_log_read(void)
{
	if (g_ctx.log)
		wake_up_process(g_ctx.log->thread);
}

/*
 * Setup MobiCore kernel log. It assumes it's running on CORE 0!
 * The fastcall will complain is that is not the case!
 */
int mobicore_log_setup(void)
{
	struct mc_log_ctx *log;
	struct sched_param param = {.sched_priority = 1 };
	int ret;

	if (g_ctx.log) {
		dev_err(g_ctx.mcd, "bad context (double initialisation?)\n");
		return -EINVAL;
	}

	/* Sanity check for the log size */
	if (g_log_size < PAGE_SIZE) {
		dev_err(g_ctx.mcd, "log_size too small (< PAGE_SIZE)\n");
		return -EINVAL;
	}

	g_log_size = PAGE_ALIGN(g_log_size);

	log = kzalloc(sizeof(*log), GFP_KERNEL);
	if (!log)
		return -ENOMEM;

	/*
	 * We are going to map this buffer into virtual address space in SWd.
	 * To reduce complexity there, we use a contiguous buffer.
	 */
	log->trace_buf = (struct mc_trace_buf *)__get_free_pages(
				GFP_KERNEL | __GFP_ZERO, get_order(g_log_size));
	if (!log->trace_buf) {
		dev_err(g_ctx.mcd, "Failed to get page for logger\n");
		ret = -ENOMEM;
		goto fail_alloc_buf;
	}

	log->thread = kthread_create(log_worker, log, "mc_log");
	if (IS_ERR(log->thread)) {
		dev_err(g_ctx.mcd, "could not create thread\n");
		ret = -EFAULT;
		goto fail_create_thread;
	}

	sched_setscheduler(log->thread, SCHED_IDLE, &param);

	ret = mc_fc_mem_trace(virt_to_phys((void *)(log->trace_buf)),
			      g_log_size);

	/* If the setup failed we must free the memory allocated */
	if (ret) {
		dev_err(g_ctx.mcd, "shared traces setup failed\n");
		ret = -EIO;
		goto fail_setup_buf;
	}

	g_ctx.log = log;

	mobicore_log_read();

	dev_dbg(g_ctx.mcd, "fc_log Logger version %u\n",
		log->trace_buf->version);
	return 0;

fail_setup_buf:
	kthread_stop(log->thread);
fail_create_thread:
	free_pages((ulong) log->trace_buf, get_order(g_log_size));
fail_alloc_buf:
	kfree(log);

	return ret;
}

/*
 * Free kernel log components.
 * ATTN: We can't free the log buffer because it's also in use by MobiCore and
 * even if the module is unloaded MobiCore is still running.
 */
void mobicore_log_free(void)
{
	if (unlikely(!g_ctx.log))
		return;

	if (!IS_ERR_OR_NULL(g_ctx.log->thread))
		/* We don't really care what the thread returns for exit */
		kthread_stop(g_ctx.log->thread);

	if (!mc_fc_mem_trace(0, 0))
		free_pages((ulong) g_ctx.log->trace_buf, get_order(g_log_size));
	else
		dev_err(g_ctx.mcd, "Something wrong (memory leak?)\n");

	kfree(g_ctx.log);
	g_ctx.log = NULL;
}

#endif /* MC_MEM_TRACES */
