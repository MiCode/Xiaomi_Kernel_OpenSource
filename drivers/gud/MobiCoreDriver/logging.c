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

#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/device.h>

#include "fastcall.h"
#include "main.h"
#include "logging.h"

#ifndef CONFIG_TRUSTONIC_TEE_NO_TRACES

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

static struct logging_ctx {
	struct work_struct work;
	union {
		struct mc_trace_buf *trace_buf;	/* Circular log buffer */
		unsigned long trace_page;
	};
	bool buffer_is_shared;		/* Log buffer cannot be freed */
	uint32_t tail;			/* MobiCore log read position */
	uint32_t line_len;		/* Log Line buffer current length */
	int thread_err;
	uint16_t prev_source;		/* Previous Log source */
	char line[LOG_LINE_SIZE];	/* Log Line buffer */
	bool dead;
} log_ctx;

static inline void log_eol(uint16_t source)
{
	if (!strnlen(log_ctx.line, LOG_LINE_SIZE)) {
		/* In case a TA tries to print a 0x0 */
		log_ctx.line_len = 0;
		return;
	}

	if (log_ctx.prev_source)
		/* MobiCore Userspace */
		dev_info(g_ctx.mcd, "%03x|%s\n", log_ctx.prev_source,
			 log_ctx.line);
	else
		/* MobiCore kernel */
		dev_info(g_ctx.mcd, "%s\n", log_ctx.line);

	log_ctx.line_len = 0;
	log_ctx.line[0] = 0;
}

/*
 * Collect chars in log_ctx.line buffer and output the buffer when it is full.
 * No locking needed because only "mobicore_log" thread updates this buffer.
 */
static inline void log_char(char ch, uint16_t source)
{
	if (ch == '\n' || ch == '\r') {
		log_eol(source);
		return;
	}

	if ((log_ctx.line_len >= (LOG_LINE_SIZE - 1)) ||
	    (source != log_ctx.prev_source))
		log_eol(source);

	log_ctx.line[log_ctx.line_len++] = ch;
	log_ctx.line[log_ctx.line_len] = 0;
	log_ctx.prev_source = source;
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

static void log_worker(struct work_struct *work)
{
	while (log_ctx.trace_buf->head != log_ctx.tail) {
		if (log_ctx.trace_buf->version != MC_LOG_VERSION) {
			dev_err(g_ctx.mcd,
				"Bad log data v%d (exp. v%d), stop.\n",
				log_ctx.trace_buf->version,
				MC_LOG_VERSION);
			log_ctx.dead = true;
			break;
		}

		log_ctx.tail += log_msg(&log_ctx.trace_buf->buff[log_ctx.tail]);
		/* Wrap over if no space left for a complete message */
		if ((log_ctx.tail + sizeof(struct mc_logmsg)) >
						log_ctx.trace_buf->length)
			log_ctx.tail = 0;
	}
}

/*
 * Wake up the log reader thread
 * This should be called from the places where calls into MobiCore have
 * generated some logs(eg, yield, SIQ...)
 */
void mc_logging_run(void)
{
	if (!log_ctx.dead && (log_ctx.trace_buf->head != log_ctx.tail))
		schedule_work(&log_ctx.work);
}

int mc_logging_start(void)
{
	int ret = mc_fc_mem_trace(virt_to_phys((void *)(log_ctx.trace_page)),
				  BIT(LOG_BUF_ORDER) * PAGE_SIZE);

	if (ret) {
		dev_err(g_ctx.mcd, "shared traces setup failed\n");
		return ret;
	}

	log_ctx.buffer_is_shared = true;
	dev_dbg(g_ctx.mcd, "fc_log version %u\n", log_ctx.trace_buf->version);
	mc_logging_run();
	return 0;
}

void mc_logging_stop(void)
{
	if (!mc_fc_mem_trace(0, 0))
		log_ctx.buffer_is_shared = false;

	mc_logging_run();
	flush_work(&log_ctx.work);
}

/*
 * Setup MobiCore kernel log. It assumes it's running on CORE 0!
 * The fastcall will complain is that is not the case!
 */
int mc_logging_init(void)
{
	/*
	 * We are going to map this buffer into virtual address space in SWd.
	 * To reduce complexity there, we use a contiguous buffer.
	 */
	log_ctx.trace_page = __get_free_pages(GFP_KERNEL | __GFP_ZERO,
					      LOG_BUF_ORDER);
	if (!log_ctx.trace_page)
		return -ENOMEM;

	INIT_WORK(&log_ctx.work, log_worker);
	return 0;
}

void mc_logging_exit(void)
{
	/*
	 * This is not racey as the only caller for mc_logging_run is the
	 * scheduler which gets stopped before us, and long before we exit.
	 */
	if (!log_ctx.buffer_is_shared)
		free_pages(log_ctx.trace_page, LOG_BUF_ORDER);
	else
		dev_err(g_ctx.mcd, "log buffer unregister not supported\n");
}

#endif /* !CONFIG_TRUSTONIC_TEE_NO_TRACES */
