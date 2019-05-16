// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/version.h>

#include "main.h"
#include "logging.h"

/* Supported log buffer version */
#define MC_LOG_VERSION			2

/* Default length of the log ring buffer 256KiB */
#define LOG_BUF_ORDER			6

/* Max Len of a log line for printing */
#define LOG_LINE_SIZE			256

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

/* active cpu id */
#define LOG_CPUID_MASK            (0xF000)
#define LOG_CPUID_SHIFT           12

struct mc_logmsg {
	u16	ctrl;		/* Type and format of data */
	u16	source;		/* Unique value for each event source */
	u32	log_data;	/* Value, if any */
};

/* MobiCore internal trace buffer structure. */
struct mc_trace_buf {
	u32	version;	/* version of trace buffer */
	u32	length;		/* length of buff */
	u32	head;		/* last write position */
	u8	buff[];		/* start of the log buffer */
};

static struct logging_ctx {
	struct kthread_work work;
	struct kthread_worker worker;
	struct task_struct *thread;
	union {
		struct mc_trace_buf *trace_buf;	/* Circular log buffer */
		unsigned long trace_page;
	};
	u32	tail;			/* MobiCore log read position */
	int	thread_err;
	u16	prev_source;		/* Previous Log source */
	char	line[LOG_LINE_SIZE + 1];/* Log Line buffer */
	u32	line_len;		/* Log Line buffer current length */
	bool	enabled;		/* Log can be disabled via debugfs */
	bool	dead;
} log_ctx;

static inline void log_eol(u16 source, u32 cpuid)
{
	if (!log_ctx.line_len)
		return;

	if (log_ctx.prev_source)
		/* TEE user-space */
		dev_info(g_ctx.mcd, "%03x(%u)|%s\n", log_ctx.prev_source,
			 cpuid, log_ctx.line);
	else
		/* TEE kernel */
		dev_info(g_ctx.mcd, "mtk(%u)|%s\n", cpuid, log_ctx.line);
	log_ctx.line[0] = '\0';
	log_ctx.line_len = 0;
}

/*
 * Collect chars in log_ctx.line buffer and output the buffer when it is full.
 * No locking needed because only "mobicore_log" thread updates this buffer.
 */
static inline void log_char(char ch, u16 source, u32 cpuid)
{
	if (ch == '\0')
		return;

	if (ch == '\n' || ch == '\r') {
		log_eol(source, cpuid);
		return;
	}

	if (log_ctx.line_len >= LOG_LINE_SIZE || source != log_ctx.prev_source)
		log_eol(source, cpuid);

	log_ctx.line[log_ctx.line_len++] = ch;
	log_ctx.line[log_ctx.line_len] = 0;
	log_ctx.prev_source = source;
}

static inline void log_string(u32 ch, u16 source, u32 cpuid)
{
	while (ch) {
		log_char(ch & 0xFF, source, cpuid);
		ch >>= 8;
	}
}

static inline void log_number(u32 format, u32 value, u16 source, u32 cpuid)
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
		log_char(*reader++, source, cpuid);
}

static inline int log_msg(void *data)
{
	struct mc_logmsg *msg = (struct mc_logmsg *)data;
	int log_type = msg->ctrl & LOG_TYPE_MASK;
	int cpuid = ((msg->ctrl & LOG_CPUID_MASK) >> LOG_CPUID_SHIFT);

	switch (log_type) {
	case LOG_TYPE_CHAR:
		log_string(msg->log_data, msg->source, cpuid);
		break;
	case LOG_TYPE_INTEGER:
		log_number(msg->ctrl, msg->log_data, msg->source, cpuid);
		break;
	}
	if (msg->ctrl & LOG_EOL)
		log_eol(msg->source, cpuid);

	return sizeof(*msg);
}

static void logging_worker(struct kthread_work *work)
{
	static DEFINE_MUTEX(local_mutex);

	mutex_lock(&local_mutex);
	while (log_ctx.trace_buf->head != log_ctx.tail) {
		if (log_ctx.trace_buf->version != MC_LOG_VERSION) {
			mc_dev_err(-EINVAL, "Bad log data v%d (exp. v%d), stop",
				   log_ctx.trace_buf->version, MC_LOG_VERSION);
			log_ctx.dead = true;
			break;
		}

		log_ctx.tail += log_msg(&log_ctx.trace_buf->buff[log_ctx.tail]);
		/* Wrap over if no space left for a complete message */
		if ((log_ctx.tail + sizeof(struct mc_logmsg)) >
						log_ctx.trace_buf->length)
			log_ctx.tail = 0;
	}
	mutex_unlock(&local_mutex);
}

/*
 * Wake up the log reader thread
 * This should be called from the places where calls into MobiCore have
 * generated some logs(eg, yield, SIQ...)
 */
void logging_run(void)
{
	if (log_ctx.enabled && !log_ctx.dead &&
	    log_ctx.trace_buf->head != log_ctx.tail)
		kthread_queue_work(&log_ctx.worker, &log_ctx.work);
}

/*
 * Setup MobiCore kernel log. It assumes it's running on CORE 0!
 * The fastcall will complain if that is not the case!
 */
int logging_init(phys_addr_t *buffer, u32 *size)
{
	/*
	 * We are going to map this buffer into virtual address space in SWd.
	 * To reduce complexity there, we use a contiguous buffer.
	 */
	log_ctx.trace_page = __get_free_pages(GFP_KERNEL | __GFP_ZERO,
					      LOG_BUF_ORDER);
	if (!log_ctx.trace_page)
		return -ENOMEM;

	*buffer = virt_to_phys((void *)(log_ctx.trace_page));
	*size = BIT(LOG_BUF_ORDER) * PAGE_SIZE;

	/* Logging thread */
	kthread_init_work(&log_ctx.work, logging_worker);
	kthread_init_worker(&log_ctx.worker);
	log_ctx.thread = kthread_create(kthread_worker_fn, &log_ctx.worker,
					"tee_log");
	if (IS_ERR(log_ctx.thread))
		return PTR_ERR(log_ctx.thread);

	wake_up_process(log_ctx.thread);

	/* Debugfs switch */
	log_ctx.enabled = true;
	debugfs_create_bool("swd_debug", 0600, g_ctx.debug_dir,
			    &log_ctx.enabled);
	return 0;
}

void logging_exit(bool buffer_busy)
{
	/*
	 * This is not racey as the only caller for logging_run is the
	 * scheduler which gets stopped before us, and long before we exit.
	 */
	kthread_stop(log_ctx.thread);
	if (!buffer_busy)
		free_pages(log_ctx.trace_page, LOG_BUF_ORDER);
}
