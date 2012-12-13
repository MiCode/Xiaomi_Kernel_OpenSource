/*
 * MobiCore Driver Logging Subsystem.
 *
 * The logging subsytem provides the interface between the Mobicore trace
 * buffer and the Linux log
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/miscdevice.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/device.h>

#include "mc_drv_module.h"
#include "mc_drv_module_fastcalls.h"

/* Default len of the log ring buffer 256KB*/
#define LOG_BUF_SIZE			(64 * PAGE_SIZE)

/* Max Len of a log line for printing */
#define LOG_LINE_SIZE			256

static uint32_t log_size = LOG_BUF_SIZE;

module_param(log_size, uint, 0);
MODULE_PARM_DESC(log_size, " Size of the log ringbuffer (or 256KB default).");

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

struct logmsg_struct {
	uint16_t ctrl;			/* Type and format of data */
	uint16_t source;		/* Unique value for each event source */
	uint32_t log_data;		/* Value, if any */
};

static uint32_t log_pos;		/* MobiCore log previous position */
static struct mc_trace_buf *log_buf;	/* MobiCore log buffer structure */
struct task_struct *log_thread;		/* Log Thread task structure */
static char *log_line;			/* Log Line buffer */
static uint32_t log_line_len;		/* Log Line buffer current len */

static void log_eol(void)
{
	if (!strnlen(log_line, LOG_LINE_SIZE))
		return;
	dev_info(mcd, "%s\n", log_line);
	log_line_len = 0;
	log_line[0] = 0;
}

/*
 * Collect chars in log_line buffer and output the buffer when it is full.
 * No locking needed because only "mobicore_log" thread updates this buffer.
 */
static void log_char(char ch)
{
	if (ch == '\n' || ch == '\r') {
		log_eol();
		return;
	}

	if (log_line_len >= LOG_LINE_SIZE - 1) {
		dev_info(mcd, "%s\n", log_line);
		log_line_len = 0;
		log_line[0] = 0;
	}

	log_line[log_line_len] = ch;
	log_line[log_line_len + 1] = 0;
	log_line_len++;
}

/*
 * Put a string to the log line.
 */
static void log_str(const char *s)
{
	int i;

	for (i = 0; i < strnlen(s, LOG_LINE_SIZE); i++)
		log_char(s[i]);
}

static uint32_t process_v1log(void)
{
	char *last_char = log_buf->buff + log_buf->write_pos;
	char *buff = log_buf->buff + log_pos;
	while (buff != last_char) {
		log_char(*(buff++));
		/* Wrap around */
		if (buff - (char *)log_buf >= log_size)
			buff = log_buf->buff;
	}
	return buff - log_buf->buff;
}

static const uint8_t HEX2ASCII[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

static void dbg_raw_nro(uint32_t format, uint32_t value)
{
	int digits = 1;
	uint32_t base = (format & LOG_INTEGER_DECIMAL) ? 10 : 16;
	int width = (format & LOG_LENGTH_MASK) >> LOG_LENGTH_SHIFT;
	int negative = 0;
	uint32_t digit_base = 1;

	if ((format & LOG_INTEGER_SIGNED) != 0 && ((signed int)value) < 0) {
		negative = 1;
		value = (uint32_t)(-(signed int)value);
		width--;
	}

	/* Find length and divider to get largest digit */
	while (value / digit_base >= base) {
		digit_base *= base;
		digits++;
	}

	if (width > digits) {
		char ch = (base == 10) ? ' ' : '0';
		while (width > digits) {
			log_char(ch);
			width--;
		}
	}

	if (negative)
		log_char('-');

	while (digits-- > 0) {
		uint32_t d = value / digit_base;
		log_char(HEX2ASCII[d]);
		value = value - d * digit_base;
		digit_base /= base;
	}
}

static void log_msg(struct logmsg_struct *msg)
{
	unsigned char msgtxt[5];
	int mpos = 0;

	switch (msg->ctrl & LOG_TYPE_MASK) {
	case LOG_TYPE_CHAR: {
		uint32_t ch;
		ch = msg->log_data;
		while (ch != 0) {
			msgtxt[mpos++] = ch&0xFF;
			ch >>= 8;
		}
		msgtxt[mpos] = 0;
		log_str(msgtxt);
		break;
	}
	case LOG_TYPE_INTEGER: {
		dbg_raw_nro(msg->ctrl, msg->log_data);
		break;
	}
	default:
		break;
	}
	if (msg->ctrl & LOG_EOL)
		log_eol();
}

static uint32_t process_v2log(void)
{
	char *last_msg = log_buf->buff + log_buf->write_pos;
	char *buff = log_buf->buff + log_pos;
	while (buff != last_msg) {
		log_msg((struct logmsg_struct *)buff);
		buff += sizeof(struct logmsg_struct);
		/* Wrap around */
		if ((buff + sizeof(struct logmsg_struct)) >
		    ((char *)log_buf + log_size))
			buff = log_buf->buff;
	}
	return buff - log_buf->buff;
}

static int log_worker(void *p)
{
	if (log_buf == NULL)
		return -EFAULT;

	while (!kthread_should_stop()) {
		if (log_buf->write_pos == log_pos)
			schedule_timeout_interruptible(MAX_SCHEDULE_TIMEOUT);

		switch (log_buf->version) {
		case 1:
			log_pos = process_v1log();
			break;
		case 2:
			log_pos = process_v2log();
			break;
		default:
			MCDRV_DBG_ERROR(mcd, "Unknown Mobicore log data");
			MCDRV_DBG_ERROR(mcd, "version %d logging disabled.",
					log_buf->version);
			log_pos = log_buf->write_pos;
			/*
			 * Stop the thread as we have no idea what
			 * happens next
			 */
			return -EFAULT;
		}
	}
	MCDRV_DBG(mcd, "Logging thread stopped!");
	return 0;
}

/*
 * Wakeup the log reader thread
 * This should be called from the places where calls into MobiCore have
 * generated some logs(eg, yield, SIQ...)
 */
void mobicore_log_read(void)
{
	if (log_thread == NULL || IS_ERR(log_thread))
		return;

	wake_up_process(log_thread);
}

/*
 * Setup MobiCore kernel log. It assumes it's running on CORE 0!
 * The fastcall will complain is that is not the case!
 */
long mobicore_log_setup(void *data)
{
	unsigned long phys_log_buf;
	union fc_generic fc_log;

	long ret;
	log_pos = 0;
	log_buf = NULL;
	log_thread = NULL;
	log_line = NULL;
	log_line_len = 0;

	/* Sanity check for the log size */
	if (log_size < PAGE_SIZE)
		return -EFAULT;
	else
		log_size = PAGE_ALIGN(log_size);

	log_line = kzalloc(LOG_LINE_SIZE, GFP_KERNEL);
	if (IS_ERR(log_line)) {
		MCDRV_DBG_ERROR(mcd, "failed to allocate log line!");
		return -ENOMEM;
	}

	log_thread = kthread_create(log_worker, NULL, "mobicore_log");
	if (IS_ERR(log_thread)) {
		MCDRV_DBG_ERROR(mcd, "MobiCore log thread creation failed!");
		ret = -EFAULT;
		goto mobicore_log_setup_log_line;
	}

	/*
	 * We are going to map this buffer into virtual address space in SWd.
	 * To reduce complexity there, we use a contiguous buffer.
	 */
	log_buf = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
					   get_order(log_size));
	if (!log_buf) {
		MCDRV_DBG_ERROR(mcd, "Failed to get page for logger!");
		ret = -ENOMEM;
		goto mobicore_log_setup_kthread;
	}
	phys_log_buf = virt_to_phys(log_buf);

	memset(&fc_log, 0, sizeof(fc_log));
	fc_log.as_in.cmd = MC_FC_NWD_TRACE;
	fc_log.as_in.param[0] = phys_log_buf;
	fc_log.as_in.param[1] = log_size;

	MCDRV_DBG(mcd, "fc_log virt=%p phys=%p ",
		  log_buf, (void *)phys_log_buf);
	mc_fastcall(&fc_log);
	MCDRV_DBG(mcd, "fc_log out ret=0x%08x", fc_log.as_out.ret);

	/* If the setup failed we must free the memory allocated */
	if (fc_log.as_out.ret) {
		MCDRV_DBG_ERROR(mcd, "MobiCore shared traces setup failed!");
		free_pages((unsigned long)log_buf, get_order(log_size));
		log_buf = NULL;
		ret = -EIO;
		goto mobicore_log_setup_kthread;
	}

	MCDRV_DBG(mcd, "fc_log Logger version %u\n", log_buf->version);
	return 0;

mobicore_log_setup_kthread:
	kthread_stop(log_thread);
	log_thread = NULL;
mobicore_log_setup_log_line:
	kfree(log_line);
	log_line = NULL;
	return ret;
}

/*
 * Free kernel log componenets.
 * ATTN: We can't free the log buffer because it's also in use by MobiCore and
 * even if the module is unloaded MobiCore is still running.
 */
void mobicore_log_free(void)
{
	if (log_thread && !IS_ERR(log_thread)) {
		/* We don't really care what the thread returns for exit */
		kthread_stop(log_thread);
	}

	kfree(log_line);
}
