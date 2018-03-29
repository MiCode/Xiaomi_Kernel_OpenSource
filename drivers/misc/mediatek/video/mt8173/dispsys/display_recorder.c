/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <stdarg.h>
#include <mt-plat/met_drv.h>

#include "ddp_mmp.h"
#include "debug.h"

#include "disp_drv_log.h"

#include "disp_lcm.h"
#include "disp_utils.h"
#include "mtkfb_info.h"
#include "mtkfb.h"

#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_log.h"

#include "m4u.h"
#include "m4u_port.h"
#include "primary_display.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

#include "ddp_manager.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "disp_session.h"
#include "ddp_mmp.h"
#include <linux/ftrace_event.h>

#if defined(CONFIG_MT_ENG_BUILD)

unsigned int gCapturePriLayerEnable = 0;
unsigned int gCaptureWdmaLayerEnable = 0;
unsigned int gCapturePriLayerDownX = 20;
unsigned int gCapturePriLayerDownY = 20;
unsigned int gCapturePriLayerNum = 4;

static DEFINE_SPINLOCK(gdprec_logger_spinlock);

static dprec_debug_control _control = { 0 };

static reg_base_map reg_map[] = {
	{"MMSYS", (0xf4000000)},
	{"OVL0", (0xF400c000)},
	{"OVL1", (0xF400d000)},
	{"RDMA0", (0xF400e000)},
	{"RDMA1", (0xF400f000)},
	{"RDMA2", (0xF4010000)},
	{"WDMA0", (0xF4011000)},
	{"WDMA1", (0xF4012000)},
	{"COLOR0", (0xF4013000)},
	{"COLOR1", (0xF4014000)},
	{"AAL", (0xF4015000)},
	{"GAMMA", (0xF4016000)},
	{"MERGE", (0xF4017000)},
	{"SPLIT0", (0xF4018000)},
	{"SPLIT1", (0xF4019000)},
	{"UFOE", (0xF401a000)},
	{"DSI0", (0xF401b000)},
	{"DSI1", (0xF401c000)},
	{"DPI", (0xF401d000)},
	{"PWM0", (0xF401e000)},
	{"PWM1", (0xF401f000)},
	{"MUTEX", (0xF4020000)},
	{"SMI_LARB0", (0xF4021000)},
	{"SMI_COMMON", (0xF4022000)},
	{"OD", (0xF4023000)},
	{"MIPITX0", (0xF0215000)},
	{"MIPITX1", (0xF0216000)},
};

static event_string_map event_map[] = {
	{"Set Config Dirty", DPREC_EVENT_CMDQ_SET_DIRTY},
	{"Wait Stream EOF", DPREC_EVENT_CMDQ_WAIT_STREAM_EOF},
	{"Signal CMDQ Event-Stream EOF", DPREC_EVENT_CMDQ_SET_EVENT_ALLOW},
	{"Flush CMDQ", DPREC_EVENT_CMDQ_FLUSH},
	{"Reset CMDQ", DPREC_EVENT_CMDQ_RESET},
	{"Frame Done", DPREC_EVENT_FRAME_DONE},
	{"Frame Start", DPREC_EVENT_FRAME_START},
};



static MMP_Event dprec_mmp_event_spy(DPREC_LOGGER_ENUM l)
{
	switch (l & 0xffffff) {
	case DPREC_LOGGER_PRIMARY_MUTEX:
		return ddp_mmp_get_events()->primary_sw_mutex;
	case DPREC_LOGGER_PRIMARY_TRIGGER:
		return ddp_mmp_get_events()->primary_trigger;
	case DPREC_LOGGER_PRIMARY_CONFIG:
		return ddp_mmp_get_events()->primary_config;
	case DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY:
		return ddp_mmp_get_events()->primary_set_dirty;
	case DPREC_LOGGER_PRIMARY_CMDQ_FLUSH:
		return ddp_mmp_get_events()->primary_cmdq_flush;
	case DPREC_LOGGER_DISPMGR_PREPARE:
		return ddp_mmp_get_events()->session_prepare;
	case DPREC_LOGGER_DISPMGR_SET_INPUT:
		return ddp_mmp_get_events()->session_set_input;
	case DPREC_LOGGER_DISPMGR_TRIGGER:
		return ddp_mmp_get_events()->session_trigger;
	case DPREC_LOGGER_DISPMGR_RELEASE:
		return ddp_mmp_get_events()->session_release;
	case DPREC_LOGGER_DISPMGR_WAIT_VSYNC:
		return ddp_mmp_get_events()->session_wait_vsync;
	case DPREC_LOGGER_DISPMGR_CACHE_SYNC:
		return ddp_mmp_get_events()->primary_cache_sync;
	}
	return 0xffff;
}

void dprec_to_mmp(unsigned int type_logsrc, MMP_LogType mmp_log, unsigned int data1, unsigned data2)
{
	int MMP_Event = dprec_mmp_event_spy(type_logsrc);

	if (MMP_Event < 0xffff)
		MMProfileLogEx(MMP_Event, mmp_log, data1, data2);

}

const char *_find_module_by_reg_addr(unsigned int reg)
{
	int i = 0;
	unsigned int module_offset = 0x1000;
	unsigned int base = (reg & (~(module_offset - 1)));

	for (i = 0; i < sizeof(reg_map) / sizeof(reg_base_map); i++) {
		if (base == reg_map[i].module_reg_base)
			return reg_map[i].module_name;
	}

	return "unknown";
}

const char *_get_event_string(DPREC_EVENT event)
{
	int i = 0;

	for (i = 0; i < sizeof(event_map) / sizeof(event_string_map); i++) {
		if (event == event_map[i].event)
			return event_map[i].event_string;
	}

	return "unknown";
}

static unsigned long long get_current_time_us(void)
{
	unsigned long long time = sched_clock();
	/* return do_div(time,1000); */
	return time;

#if 0
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
#endif
}


#define dprec_string_max_length 512
static unsigned char dprec_string_buffer[dprec_string_max_length] = { 0 };
dprec_logger logger[DPREC_LOGGER_NUM] = { {0} };

#define dprec_dump_max_length (1024*16*4)
static unsigned char dprec_string_buffer_analysize[dprec_dump_max_length] = { 0 };

static unsigned int analysize_length;


unsigned int dprec_error_log_len = 0;
unsigned int dprec_error_log_buflen = DPREC_ERROR_LOG_BUFFER_LENGTH;
unsigned int dprec_error_log_id = 0;

char dprec_error_log_buffer[DPREC_ERROR_LOG_BUFFER_LENGTH];
static dprec_logger_event dprec_vsync_irq_event;

int dprec_init(void)
{
	memset((void *)&_control, 0, sizeof(_control));
	memset((void *)&logger, 0, sizeof(logger));
	memset((void *)dprec_error_log_buffer, 0, DPREC_ERROR_LOG_BUFFER_LENGTH);
	ddp_mmp_init();

	dprec_logger_event_init(&dprec_vsync_irq_event, "VSYNC_IRQ", DPREC_LOGGER_LEVEL_SYSTRACE,
				NULL);
	return 0;
}

void dprec_event_op(DPREC_EVENT event)
{
	int len = 0;

	if (_control.overall_switch == 0)
		return;

	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[DPREC]");
	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[EVENT]");
	len +=
	    scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[%s]",
		      _get_event_string(event));

	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "\n");
}


static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	unsigned long ret = 0;

	if ((long long)nsec < 0)
		nsec = -nsec;

	ret = do_div(nsec, 1000000);
	return ret / 10000;
}

static long long msec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000000);
		return -nsec;
	}
	do_div(nsec, 1000000000);

	return nsec;
}

static unsigned long msec_low(unsigned long long nsec)
{
	unsigned long ret = 0;

	if ((long long)nsec < 0)
		nsec = -nsec;

	ret = do_div(nsec, 1000000000);
	return ret / 10000000;
}


/* #define SPLIT_NS(x) (unsigned long long)nsec_high(x), (unsigned long)nsec_low(x) */
/* #define SPLIT_MS(x) (unsigned long long)msec_high(x), (unsigned long)msec_low(x) */


const char *dprec_logger_spy(DPREC_LOGGER_ENUM l)
{
	switch (l) {
	case DPREC_LOGGER_PRIMARY_TRIGGER:
		return "Primary Path Trigger";
	case DPREC_LOGGER_PRIMARY_MUTEX:
		return "Primary Path Mutex";
	case DPREC_LOGGER_DISPMGR_WAIT_VSYNC:
		return "Wait VYSNC Signal";
	case DPREC_LOGGER_RDMA0_TRANSFER:
		return "RDMA0 Transfer";
	case DPREC_LOGGER_DSI_EXT_TE:
		return "DSI_EXT_TE";
	case DPREC_LOGGER_ESD_RECOVERY:
		return "ESD Recovery";
	case DPREC_LOGGER_ESD_CHECK:
		return "ESD Check";
	case DPREC_LOGGER_ESD_CMDQ:
		return "ESD CMDQ Keep";
	case DPREC_LOGGER_PRIMARY_CONFIG:
		return "Primary Path Config";
	case DPREC_LOGGER_DISPMGR_PREPARE:
		return "DISPMGR Prepare";
	case DPREC_LOGGER_DISPMGR_CACHE_SYNC:
		return "DISPMGR Cache Sync";
	case DPREC_LOGGER_DISPMGR_SET_INPUT:
		return "DISPMGR Set Input";
	case DPREC_LOGGER_DISPMGR_TRIGGER:
		return "DISPMGR Trigger";
	case DPREC_LOGGER_DISPMGR_RELEASE:
		return "DISPMGR Release";
	case DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY:
		return "Primary CMDQ Dirty";
	case DPREC_LOGGER_PRIMARY_CMDQ_FLUSH:
		return "Primary CMDQ Flush";
	case DPREC_LOGGER_PRIMARY_BUFFER_KEEP:
		return "Fence Buffer Keep";
	case DPREC_LOGGER_WDMA_DUMP:
		return "Screen Capture(wdma)";

	default:
		return "unknown";
	}
}

void dprec_logger_trigger(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	DPREC_LOGGER_ENUM source;
	dprec_logger *l;
	unsigned long long time;

	dprec_to_mmp(type_logsrc, MMProfileFlagPulse, val1, val2);

	source = type_logsrc & 0xffffff;
	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];
	time = get_current_time_us();
	if (l->count == 0) {
		l->ts_start = time;
		l->ts_trigger = time;
		l->period_min_frame = 0xffffffffffffffff;
	} else {
		l->period_frame = time - l->ts_trigger;

		if (l->period_frame > l->period_max_frame)
			l->period_max_frame = l->period_frame;

		if (l->period_frame < l->period_min_frame)
			l->period_min_frame = l->period_frame;

		l->ts_trigger = time;

		if (l->count == 0)
			l->ts_start = l->ts_trigger;
	}
	l->count++;
#if 0
	if (source == DPREC_LOGGER_DSI_EXT_TE) {
		/* DDPMSG("count=%d, time=%llu, period=%llu, max=%llu,min=%llu,start=%llu,trigger=%llu\n",
			l->count,
			time,
			l->period_frame,
			l->period_max_frame,
			l->period_min_frame,
			l->ts_start,
			l->ts_trigger); */

		/* DDPMSG("count=%d,period=%lld.%02lums,max=%lld.%02lums\n",
			l->count,
			(unsigned long long)nsec_high(l->period_frame),
			(unsigned long)nsec_low(l->period_frame),
			(unsigned long long)nsec_high(l->period_max_frame),
			(unsigned long)nsec_low(l->period_max_frame)); */
	}
#endif
	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
}

unsigned long long dprec_logger_get_current_hold_period(unsigned int type_logsrc)
{
	unsigned long long period = 0;
	unsigned long flags = 0;

	unsigned long long time = get_current_time_us();

	DPREC_LOGGER_ENUM source = type_logsrc & 0xffffff;
	dprec_logger *l;

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];
	if (l->ts_trigger)
		period = (time - l->ts_trigger);
	else
		period = 0;

	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);

	return period;
}

void dprec_logger_start(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	DPREC_LOGGER_ENUM source;
	dprec_logger *l;
	unsigned long long time;

	dprec_to_mmp(type_logsrc, MMProfileFlagStart, val1, val2);
	source = type_logsrc & 0xffffff;
	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];
	time = get_current_time_us();
	if (l->count == 0) {
		l->ts_start = time;
		l->period_min_frame = 0xffffffffffffffff;
	}

	l->ts_trigger = time;
	if (source == DPREC_LOGGER_RDMA0_TRANSFER)
		;/* DDPMSG("start, count=%llu,trigger=%llu.%02lums\n",
		l->count, SPLIT_NS(l->ts_trigger));
		*/
	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
}

void dprec_logger_done(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	DPREC_LOGGER_ENUM source;
	dprec_logger *l;
	unsigned long long time;

	dprec_to_mmp(type_logsrc, MMProfileFlagEnd, val1, val2);

	source = type_logsrc & 0xffffff;
	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];
	time = get_current_time_us();

	if (l->ts_start == 0)
		goto done;


	l->period_frame = time - l->ts_trigger;

	if (l->period_frame > l->period_max_frame)
		l->period_max_frame = l->period_frame;

	if (l->period_frame < l->period_min_frame)
		l->period_min_frame = l->period_frame;

	l->period_total += l->period_frame;
	l->count++;

	if (source == DPREC_LOGGER_RDMA0_TRANSFER)
		;/* DDPMSG("done, count=%llu,period=%llu.%02lums,max=%llu.%02lums\n",
		l->count, SPLIT_NS(l->period_frame), SPLIT_NS(l->period_max_frame));
		*/
done:
	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
}

void dprec_logger_event_init(dprec_logger_event *p, char *name, uint32_t level,
			     MMP_Event *mmp_root)
{
	if (p) {
		scnprintf(p->name, sizeof(p->name) / sizeof(p->name[0]), name);
		if (mmp_root)
			p->mmp =
			    MMProfileRegisterEvent((unsigned int)(unsigned long)mmp_root, name);
		else
			p->mmp = MMProfileRegisterEvent(ddp_mmp_get_events()->DDP, name);

		MMProfileEnableEventRecursive(p->mmp, 1);

		p->level = level;

		memset((void *)&p->logger, 0, sizeof(p->logger));
		DDPMSG("dprec logger event init, name=%s, level=0x%08x\n", name, level);
	}
}

#ifdef CONFIG_TRACING

static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(0 == tracing_mark_write_addr))
		tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");
}

static inline void mmp_kernel_trace_begin(char *name)
{
	__mt_update_tracing_mark_write_addr();
	event_trace_printk(tracing_mark_write_addr, "B|%d|%s\n", current->tgid, name);
}

static inline void mmp_kernel_trace_counter(char *name, int count)
{
	__mt_update_tracing_mark_write_addr();
	event_trace_printk(tracing_mark_write_addr, "C|%d|%s|%d\n",
			   in_interrupt() ? -1 : current->tgid, name, count);
}

static inline void mmp_kernel_trace_end(void)
{
	__mt_update_tracing_mark_write_addr();
	event_trace_printk(tracing_mark_write_addr, "E\n");
}
#endif

void dprec_start(dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	dprec_logger *l;
	unsigned long long time = 0;

	if (event) {
		if (event->level & DPREC_LOGGER_LEVEL_MMP)
			MMProfileLogEx(event->mmp, MMProfileFlagStart, val1, val2);

		if (event->level & DPREC_LOGGER_LEVEL_LOGGER) {
			spin_lock_irqsave(&gdprec_logger_spinlock, flags);
			l = &(event->logger);
			time = get_current_time_us();
			if (l->count == 0) {
				l->ts_start = time;
				l->period_min_frame = 0xffffffffffffffff;
			}

			l->ts_trigger = time;

			spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
		}
		if (event->level & DPREC_LOGGER_LEVEL_MOBILE_LOG)
			/* xlog_printk(ANDROID_LOG_DEBUG, "DPREC/"LOG_TAG, "
			DISP/%s start,0x%08x,0x%08x\n",\event->name, val1, val2); */

		if (event->level & DPREC_LOGGER_LEVEL_UART_LOG)
			DDPMSG("DISP/%s start,0x%08x,0x%08x\n", event->name, val1, val2);

#ifdef CONFIG_TRACING
		if (event->level & DPREC_LOGGER_LEVEL_SYSTRACE && _control.systrace) {
			char name[256];

			scnprintf(name, sizeof(name) / sizeof(name[0]), "K_%s_0x%x_0x%x",
				  event->name, val1, val2);

			mmp_kernel_trace_begin(name);
			/* trace_printk("B|%d|%s\n", current->pid, event->name); */
		}
#endif
	}
}

void dprec_done(dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	dprec_logger *l;
	unsigned long long time = 0;

	if (event) {
		if (event->level & DPREC_LOGGER_LEVEL_MMP)
			MMProfileLogEx(event->mmp, MMProfileFlagEnd, val1, val2);

		if (event->level & DPREC_LOGGER_LEVEL_LOGGER) {
			spin_lock_irqsave(&gdprec_logger_spinlock, flags);
			l = &(event->logger);
			time = get_current_time_us();

			if (l->ts_start != 0) {
				l->period_frame = time - l->ts_trigger;

				if (l->period_frame > l->period_max_frame)
					l->period_max_frame = l->period_frame;

				if (l->period_frame < l->period_min_frame)
					l->period_min_frame = l->period_frame;

				l->ts_trigger = 0;
				l->period_total += l->period_frame;
				l->count++;
			}

			spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
		}
		if (event->level & DPREC_LOGGER_LEVEL_MOBILE_LOG)
			/* xlog_printk(ANDROID_LOG_DEBUG,"DPREC"LOG_TAG,
			"DISP/"LOG_TAG,"done,0x%08x,0x%08x\n",event->name, val1, val2); */

		if (event->level & DPREC_LOGGER_LEVEL_UART_LOG)
			DDPMSG("DISP/%s done,0x%08x,0x%08x\n", event->name, val1, val2);

#ifdef CONFIG_TRACING
		if (event->level & DPREC_LOGGER_LEVEL_SYSTRACE && _control.systrace) {
			mmp_kernel_trace_end();
			/* trace_printk("E|%s\n", event->name); */
		}
#endif
	}
}

void dprec_trigger(dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	dprec_logger *l;
	unsigned long long time = 0;

	if (event) {
		if (event->level & DPREC_LOGGER_LEVEL_MMP)
			MMProfileLogEx(event->mmp, MMProfileFlagPulse, val1, val2);

		if (event->level & DPREC_LOGGER_LEVEL_LOGGER) {
			spin_lock_irqsave(&gdprec_logger_spinlock, flags);
			l = &(event->logger);
			time = get_current_time_us();
			if (l->count == 0) {
				l->ts_start = time;
				l->ts_trigger = time;
				l->period_min_frame = 0xffffffffffffffff;
			} else {
				l->period_frame = time - l->ts_trigger;

				if (l->period_frame > l->period_max_frame)
					l->period_max_frame = l->period_frame;

				if (l->period_frame < l->period_min_frame)
					l->period_min_frame = l->period_frame;

				l->ts_trigger = time;

				if (l->count == 0)
					l->ts_start = l->ts_trigger;

			}

			l->count++;
			spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
		}
		if (event->level & DPREC_LOGGER_LEVEL_MOBILE_LOG) {
			/* xlog_printk(ANDROID_LOG_DEBUG, "DPREC/" \
			LOG_TAG, "%s trigger,0x%08x,0x%08x\n",event->name, val1, val2); */
		}
		if (event->level & DPREC_LOGGER_LEVEL_UART_LOG)
			DDPMSG("DISP/%s trigger,0x%08x,0x%08x\n", event->name, val1, val2);

#ifdef CONFIG_TRACING
		if (event->level & DPREC_LOGGER_LEVEL_SYSTRACE && _control.systrace) {
			char name[256];

			scnprintf(name, sizeof(name) / sizeof(name[0]), "K_%s_0x%x_0x%x",
				  event->name, val1, val2);
			mmp_kernel_trace_begin(name);
			mmp_kernel_trace_end();
			/* trace_printk("B|%d|%s\n", current->pid, event->name); */
		}
#endif
	}
}


void dprec_submit(dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
	if (event) {
		if (event->level & DPREC_LOGGER_LEVEL_MMP)
			MMProfileLogEx(event->mmp, MMProfileFlagPulse, val1, val2);

		if (event->level & DPREC_LOGGER_LEVEL_LOGGER)
			return;

		if (event->level & DPREC_LOGGER_LEVEL_MOBILE_LOG)
			/* xlog_printk(ANDROID_LOG_DEBUG, "DPREC/"LOG_TAG,
			\"%s trigger,0x%08x,0x%08x\n",event->name, val1, val2); */

		if (event->level & DPREC_LOGGER_LEVEL_UART_LOG)
			DDPMSG("DISP/%s trigger,0x%08x,0x%08x\n", event->name, val1, val2);
	}
}

void dprec_logger_submit(unsigned int type_logsrc, unsigned long long period,
			 unsigned int fence_idx)
{
	unsigned long flags = 0;
	dprec_logger *l;

	DPREC_LOGGER_ENUM source = type_logsrc & 0xffffff;

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];

	l->period_frame = period;

	if (l->period_frame > l->period_max_frame)
		l->period_max_frame = l->period_frame;

	if (l->period_frame < l->period_min_frame)
		l->period_min_frame = l->period_frame;

	l->period_total += l->period_frame;
	l->count++;

	if (source == DPREC_LOGGER_RDMA0_TRANSFER)
		/* DDPMSG("done, count=%llu,period=%llu.%02lums,max=%llu.%02lums\n",\
		l->count, SPLIT_NS(l->period_frame), SPLIT_NS(l->period_max_frame)); */


/* done: */
	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);

	dprec_to_mmp(type_logsrc, MMProfileFlagPulse, (unsigned int)l->period_max_frame, fence_idx);
}

unsigned long long ts_dprec_reset = 0;
void dprec_logger_reset_all(void)
{
	int i = 0;

	for (i = 0; i < sizeof(logger) / sizeof(logger[0]); i++)
		dprec_logger_reset(i);

	ts_dprec_reset = get_current_time_us();
}

void dprec_logger_reset(DPREC_LOGGER_ENUM source)
{
	dprec_logger *l = &logger[source];

	memset((void *)l, 0, sizeof(dprec_logger));
	l->period_min_frame = 10000000;
	/* DDPMSG("count=%d, period=%llu, max=%llu,min=%llu,start=%llu,trigger=%llu\n", \
	l->count, l->period_frame, l->period_max_frame, l->period_min_frame, l->ts_start, l->ts_trigger); */
}

int dprec_logger_get_result_string_all(char *stringbuf, int strlen)
{
	int n = 0;
	int i = 0;

	n += scnprintf(stringbuf + n, strlen - n,
		       "|**** Display Driver Statistic Information Dump ****\n");
	n += scnprintf(stringbuf + n, strlen - n, "|Timestamp Begin=%llu.%03lds, End=%llu.%03lds\n",
		       (unsigned long long)msec_high(ts_dprec_reset),
		       (unsigned long)msec_low(ts_dprec_reset),
		       (unsigned long long)msec_high(get_current_time_us()),
		       (unsigned long)msec_low(get_current_time_us()));
	n += scnprintf(stringbuf + n, strlen - n,
		       "|------------------------+---------+------------+------------+------------+------------+------------|\n");
	n += scnprintf(stringbuf + n, strlen - n,
		       "|Event                   | count   | fps        |average(ms) | max(ms)    | min(ms)    |current(ms) |\n");
	n += scnprintf(stringbuf + n, strlen - n,
		       "|------------------------+---------+------------+------------+------------+------------+------------|\n");
	for (i = 0; i < sizeof(logger) / sizeof(logger[0]); i++)
		n += dprec_logger_get_result_string(i, stringbuf + n, strlen - n);

	n += scnprintf(stringbuf + n, strlen - n,
		       "|------------------------+---------+------------+------------+------------+------------+------------|\n");
	n += scnprintf(stringbuf + n, strlen - n, "dprec error log buffer\n");

	return n;
}

int dprec_logger_get_result_string(DPREC_LOGGER_ENUM source, char *stringbuf, int strlen)
{
	unsigned long flags = 0;
	int len = 0;
	dprec_logger *l = &logger[source];

	unsigned long long total = 0;
	unsigned long long avg = 0;
	unsigned long long count = 0;
	unsigned long long fps_high = 0;
	unsigned long fps_low = 0;

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	/* calculate average period need use available total period */
	if (l->period_total)
		total = l->period_total;
	else
		total = l->ts_trigger - l->ts_start;

	avg = total ? total : 1;
	count = l->count ? l->count : 1;
	do_div(avg, count);

	/* calculate fps need use whole time period */
	/* total = l->ts_trigger - l->ts_start; */
	fps_high = l->count;
	do_div(total, 1000 * 1000 * 1000);
	if (total == 0)
		total = 1;
	fps_high *= 100;
	do_div(fps_high, total);
	fps_low = do_div(fps_high, 100);

	len += scnprintf(stringbuf + len, strlen - len,
			 "|%-24s|%8llu |%8lld.%02ld |%8llu.%02ld |%8llu.%02ld |%8llu.%02ld |\n",
			 dprec_logger_spy(source), l->count, fps_high, fps_low,
			 nsec_high(avg), nsec_low(avg),
			 nsec_high(l->period_max_frame), nsec_low(l->period_max_frame),
			 nsec_high(l->period_min_frame), nsec_low(l->period_min_frame));

	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
	return len;
}


int dprec_logger_get_result_value(DPREC_LOGGER_ENUM source, unsigned long long *pcount,
				  unsigned long long *pavg, unsigned long long *pfps,
				  unsigned long long *pmax, unsigned long long *pmin)
{
	unsigned long flags = 0;
	int len = 0;
	dprec_logger *l = &logger[source];

	unsigned long long total = 0;
	unsigned long long avg = 0;
	unsigned long long count = 0;
	unsigned long long fps_high = 0;
	unsigned long fps_low = 0;

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	/* calculate average period need use available total period */
	if (l->period_total)
		total = l->period_total;
	else
		total = l->ts_trigger - l->ts_start;

	avg = total ? total : 1;
	count = l->count ? l->count : 1;
	do_div(avg, count);

	/* calculate fps need use whole time period */
	total = l->ts_trigger - l->ts_start;
	fps_high = l->count;
	do_div(total, 1000 * 1000 * 1000);
	if (total == 0)
		total = 1;
	fps_low = do_div(fps_high, total);
	if (pcount && pavg && pfps && pmax && pmin) {
		*pcount = count;
		*pavg = avg;
		*pfps = fps_high;
		*pmax = l->period_max_frame;
		*pmin = l->period_min_frame;
	}

	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
	return len;
}



typedef enum {
	DPREC_REG_OP = 1,
	DPREC_CMDQ_EVENT,
	DPREC_ERROR,
	DPREC_FENCE
} dprec_record_type;

typedef struct {
	unsigned int use_cmdq;
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
} dprec_reg_op_record;

typedef struct {
	unsigned int fence_fd;
	unsigned int buffer_idx;
	unsigned int ion_fd;
} dprec_fence_record;

typedef struct {
	dprec_record_type type;
	unsigned int ts;
	union {
		dprec_fence_record fence;
		dprec_reg_op_record reg_op;
	} rec;
} dprec_record;

/* static int rdma0_done_cnt; */
void dprec_stub_irq(unsigned int irq_bit)
{
	/* DISP_REG_SET(NULL,DISP_REG_CONFIG_MUTEX_INTEN,0xffffffff); */
	if (irq_bit == DDP_IRQ_DSI0_EXT_TE)
		dprec_logger_trigger(DPREC_LOGGER_DSI_EXT_TE, irq_bit, 0);
	else if (irq_bit == DDP_IRQ_RDMA0_START)
		dprec_logger_start(DPREC_LOGGER_RDMA0_TRANSFER, irq_bit, 0);
	else if (irq_bit == DDP_IRQ_RDMA0_DONE)
		dprec_logger_done(DPREC_LOGGER_RDMA0_TRANSFER, irq_bit, 0);

	/* DDPMSG("irq:0x%08x\n", irq_bit); */
}

static unsigned int vsync_cnt;

void dprec_stub_event(DISP_PATH_EVENT event)
{
	/* DISP_REG_SET(NULL,DISP_REG_CONFIG_MUTEX_INTEN,0xffffffff); */
	if (event == DISP_PATH_EVENT_IF_VSYNC) {
		vsync_cnt++;
		/* MMProfileLogEx(ddp_mmp_get_events()->vsync_count, MMProfileFlagPulse, vsync_cnt, 0); */
		dprec_start(&dprec_vsync_irq_event, 0, 0);
		dprec_done(&dprec_vsync_irq_event, 0, 0);
	}
}

unsigned int dprec_get_vsync_count(void)
{
	return vsync_cnt;
}

void dprec_reg_op(void *cmdq, unsigned int reg, unsigned int val, unsigned int mask)
{
#if 0
	int len = 0;

	if (!cmdq)
		MMProfileLogEx(ddp_mmp_get_events()->dprec_cpu_write_reg, MMProfileFlagPulse, reg,
			       val);

	if (cmdq) {
		if (mask) {
			DISPPR_HWOP("%s/0x%p/0x%08x=0x%08x&0x%08x\n",
				    _find_module_by_reg_addr(reg), cmdq, reg, val, mask);
		} else {
			DISPPR_HWOP("%s/0x%p/0x%08x=0x%08x\n", _find_module_by_reg_addr(reg),
				    cmdq, reg, val);
		}
	} else {
		if (mask) {
			DISPPR_HWOP("%s/%08x=%08x&%08x\n", _find_module_by_reg_addr(reg), reg, val,
				    mask);
		} else {
			DISPPR_HWOP("%s/%08x=%08x\n", _find_module_by_reg_addr(reg), reg, val);
		}
	}

	if (_control.overall_switch == 0)
		return;


	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[DPREC]");
	len +=
	    scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[%s]",
		      _find_module_by_reg_addr(reg));
	len +=
	    scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[%s]",
		      cmdq ? "CMDQ" : "CPU");

	if (cmdq)
		len +=
		    scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[0x%p]",
			      cmdq);

	len +=
	    scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "0x%08x=0x%08x",
		      reg, val);

	if (mask)
		len +=
		    scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "&0x%08x",
			      mask);

	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "\n");

	printk(dprec_string_buffer);

	if (_control.cmm_dump) {
		DISPPR_HWOP("[CMM]D.S SD:0x%08x 0x%08x; write %s\n",
			    (_control.cmm_dump_use_va) ? reg : (reg & 0x1fffffff),
			    mask ? (val | mask) : val, (char *)_find_module_by_reg_addr(reg));
	}
#endif
}

void dprec_logger_dump(char *string)
{
	analysize_length +=
	    scnprintf(dprec_string_buffer_analysize + analysize_length,
		      dprec_dump_max_length - analysize_length, string);
}

void dprec_logger_dump_reset(void)
{
	analysize_length = 0;
	memset(dprec_string_buffer_analysize, 0, sizeof(dprec_string_buffer_analysize));
}

char *dprec_logger_get_dump_addr(void)
{
	return dprec_string_buffer_analysize;
}

unsigned int dprec_logger_get_dump_len(void)
{
/*	printk("dump_len %d\n", analysize_length);*/

	return analysize_length;
}

typedef enum {
	DPREC_DEBUG_BIT_OVERALL_SWITCH = 0,
	DPREC_DEBUG_BIT_CMM_DUMP_SWITCH,
	DPREC_DEBUG_BIT_CMM_DUMP_VA,
	DPREC_DEBUG_BIT_SYSTRACE,
} DPREC_DEBUG_BIT_ENUM;

int dprec_handle_option(unsigned int option)
{
	_control.overall_switch = (option & (1 << DPREC_DEBUG_BIT_OVERALL_SWITCH));
	_control.cmm_dump = (option & (1 << DPREC_DEBUG_BIT_CMM_DUMP_SWITCH));
	_control.cmm_dump_use_va = (option & (1 << DPREC_DEBUG_BIT_CMM_DUMP_VA));
	_control.systrace = (option & (1 << DPREC_DEBUG_BIT_SYSTRACE));
	DDPMSG("dprec control=0x%08x\n", (unsigned int)(unsigned long)&_control);
	return 0;
}


int dprec_state_machine_op(DPREC_STM_EVENT op)
{
	return 0;
}

int dprec_mmp_dump_ovl_layer(OVL_CONFIG_STRUCT *ovl_layer, unsigned int l,
			     unsigned int session /*1:primary, 2:external, 3:memory */)
{
	if (gCapturePriLayerEnable) {
		if (gCapturePriLayerNum >= HW_OVERLAY_COUNT) {
			ddp_mmp_ovl_layer(ovl_layer, gCapturePriLayerDownX, gCapturePriLayerDownY,
					  session);
		} else if (gCapturePriLayerNum == l) {
			ddp_mmp_ovl_layer(ovl_layer, gCapturePriLayerDownX, gCapturePriLayerDownY,
					  session);
		}
		return 0;
	}
	return -1;
}

int dprec_mmp_dump_wdma_layer(void *wdma_layer, unsigned int wdma_num)
{
	if (gCaptureWdmaLayerEnable)
		ddp_mmp_wdma_layer((WDMA_CONFIG_STRUCT *) wdma_layer, wdma_num,
				   gCapturePriLayerDownX, gCapturePriLayerDownY);

	return -1;
}

#define DPREC_LOGGER_BUFFER_COUNT 2

static DEFINE_SPINLOCK(dprec_logger_error_spinlock);
unsigned int dprec_logger_error_id = 0;
unsigned int dprec_logger_error_buflen[DPREC_LOGGER_BUFFER_COUNT] = { 16 * 1024, 16 * 1024 };

char dprec_logger_error_buffer[DPREC_LOGGER_BUFFER_COUNT][16 * 1024];

static DEFINE_SPINLOCK(dprec_logger_fence_spinlock);
unsigned int dprec_logger_fence_id = 0;
unsigned int dprec_logger_fence_buflen[DPREC_LOGGER_BUFFER_COUNT] = { 16 * 1024, 16 * 1024 };

char dprec_logger_fence_buffer[DPREC_LOGGER_BUFFER_COUNT][16 * 1024];

static DEFINE_SPINLOCK(dprec_logger_hwop_spinlock);
unsigned int dprec_logger_hwop_id = 0;
unsigned int dprec_logger_hwop_buflen[DPREC_LOGGER_BUFFER_COUNT] = { 16 * 1024, 16 * 1024 };

char dprec_logger_hwop_buffer[DPREC_LOGGER_BUFFER_COUNT][16 * 1024];


char *dprec_logger_find_buf(DPREC_LOGGER_PR_TYPE type, unsigned int *len)
{
	char *p = NULL;

	if (type == DPREC_LOGGER_ERROR) {
		spin_lock(&dprec_logger_error_spinlock);
		if (dprec_logger_error_buflen[dprec_logger_error_id] < 64) {
			dprec_logger_error_id++;
			dprec_logger_error_id = dprec_logger_error_id % DPREC_LOGGER_BUFFER_COUNT;
		}
		p = dprec_logger_error_buffer[dprec_logger_error_id];
		dprec_logger_error_buflen[dprec_logger_error_id] = 16 * 1024;

		spin_unlock(&dprec_logger_error_spinlock);

		return p;
	}
	if (type == DPREC_LOGGER_FENCE) {
		spin_lock(&dprec_logger_fence_spinlock);
		if (dprec_logger_fence_buflen[dprec_logger_fence_id] < 64) {
			dprec_logger_fence_id++;
			dprec_logger_fence_id = dprec_logger_fence_id % DPREC_LOGGER_BUFFER_COUNT;
			p = dprec_logger_fence_buffer[dprec_logger_fence_id];
			dprec_logger_fence_buflen[dprec_logger_fence_id] = 16 * 1024;
		}
		spin_unlock(&dprec_logger_fence_spinlock);

		return p;
	}
	if (type == DPREC_LOGGER_HWOP) {
		spin_lock(&dprec_logger_hwop_spinlock);
		if (dprec_logger_hwop_buflen[dprec_logger_hwop_id] < 64) {
			dprec_logger_hwop_id++;
			dprec_logger_hwop_id = dprec_logger_hwop_id % DPREC_LOGGER_BUFFER_COUNT;
		}
		p = dprec_logger_hwop_buffer[dprec_logger_hwop_id];
		dprec_logger_hwop_buflen[dprec_logger_hwop_id] = 16 * 1024;

		spin_unlock(&dprec_logger_hwop_spinlock);

		return p;
	}
	*len = 0;
	return NULL;

}

static DEFINE_SPINLOCK(dprec_logger_spinlock);
unsigned int dprec_logger_id[DPREC_LOGGER_PR_NUM] = { 0, 0, 0 };

unsigned int dprec_logger_len[DPREC_LOGGER_PR_NUM][DPREC_LOGGER_BUFFER_COUNT];
char dprec_logger_buffer[DPREC_LOGGER_PR_NUM][DPREC_LOGGER_BUFFER_COUNT][16 * 1024];

int dprec_logger_pr(unsigned int type, char *fmt, ...)
{
	int n = 0;
	uint64_t time = get_current_time_us();
	unsigned long rem_nsec;

	char *buf = NULL;
	int len = 0;

	if (type >= DPREC_LOGGER_PR_NUM)
		return -1;

	spin_lock(&dprec_logger_spinlock);
	if (dprec_logger_len[type][dprec_logger_id[type]] < 64) {
		dprec_logger_id[type]++;
		dprec_logger_id[type] = dprec_logger_id[type] % DPREC_LOGGER_BUFFER_COUNT;
		dprec_logger_len[type][dprec_logger_id[type]] = 16 * 1024;
	}

	buf =
	    dprec_logger_buffer[type][dprec_logger_id[type]] + 16 * 1024 -
	    dprec_logger_len[type][dprec_logger_id[type]];
	len = dprec_logger_len[type][dprec_logger_id[type]];

	if (buf) {
		va_list args;

		rem_nsec = do_div(time, 1000000000);
		n += snprintf(buf + n, len - n, "[%5lu.%06lu]", (unsigned long)time,
			      rem_nsec / 1000);

		va_start(args, fmt);
		n += vscnprintf(buf + n, len - n, fmt, args);
		va_end(args);
	}

	dprec_logger_len[type][dprec_logger_id[type]] -= n;
	spin_unlock(&dprec_logger_spinlock);

	return n;
}

char *_logger_pr_type_spy(DPREC_LOGGER_PR_TYPE type)
{
	switch (type) {
	case DPREC_LOGGER_ERROR:
		return "error";
	case DPREC_LOGGER_FENCE:
		return "fence";
	case DPREC_LOGGER_HWOP:
		return "hwop";
	default:
		return "unknown";
	}
}

int dprec_logger_get_buf(DPREC_LOGGER_PR_TYPE type, char *stringbuf, int strlen)
{
	int n = 0;
	int i = 0;

	if (type >= DPREC_LOGGER_PR_NUM || strlen < 0)
		return 0;

	for (i = 0; i < DPREC_LOGGER_BUFFER_COUNT; i++) {
		n += scnprintf(stringbuf + n, strlen - n, "dprec log buffer[%s][%d]\n",
			       _logger_pr_type_spy(type), i);
		n += scnprintf(stringbuf + n, strlen - n, "%s\n", dprec_logger_buffer[type][i]);
	}

	return n;
}

#else

unsigned int gCapturePriLayerEnable = 0;
unsigned int gCaptureWdmaLayerEnable = 0;
unsigned int gCapturePriLayerDownX = 20;
unsigned int gCapturePriLayerDownY = 20;
unsigned int gCapturePriLayerNum = 8;

dprec_logger logger[DPREC_LOGGER_NUM] = { { 0 } };

unsigned int dprec_error_log_len = 0;
unsigned int dprec_error_log_buflen = DPREC_ERROR_LOG_BUFFER_LENGTH;
unsigned int dprec_error_log_id = 0;

int dprec_init(void)
{
	return 0;
}

void dprec_event_op(DPREC_EVENT event)
{
}

void dprec_logger_trigger(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
}

unsigned long long dprec_logger_get_current_hold_period(unsigned int type_logsrc)
{
	return 0;
}

void dprec_logger_start(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
}

void dprec_logger_done(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
}

void dprec_logger_event_init(dprec_logger_event *p, char *name, uint32_t level,
			     MMP_Event *mmp_root)
{
}

void dprec_logger_frame_seq_begin(unsigned int session_id, unsigned frm_sequence)
{
}

void dprec_logger_frame_seq_end(unsigned int session_id, unsigned frm_sequence)
{
}

void dprec_start(dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
}

void dprec_done(dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
}

void dprec_trigger(dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
}

void dprec_submit(dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
}

void dprec_logger_submit(unsigned int type_logsrc, unsigned long long period,
			 unsigned int fence_idx)
{
}

void dprec_logger_reset_all(void)
{
}

void dprec_logger_reset(DPREC_LOGGER_ENUM source)
{
}

int dprec_logger_get_result_string(DPREC_LOGGER_ENUM source, char *stringbuf, int strlen)
{
	return 0;
}

int dprec_logger_get_result_string_all(char *stringbuf, int strlen)
{
	return 0;
}

void dprec_stub_irq(unsigned int irq_bit)
{
}

void dprec_stub_event(DISP_PATH_EVENT event)
{
}

unsigned int dprec_get_vsync_count(void)
{
	return 0;
}

void dprec_reg_op(void *cmdq, unsigned int reg, unsigned int val, unsigned int mask)
{
}

void dprec_logger_dump(char *string)
{
}

void dprec_logger_dump_reset(void)
{
}

char *dprec_logger_get_dump_addr()
{
	return NULL;
}

unsigned int dprec_logger_get_dump_len(void)
{
	return 0;
}

int dprec_handle_option(unsigned int option)
{
	return 0;
}

int dprec_mmp_dump_ovl_layer(OVL_CONFIG_STRUCT *ovl_layer, unsigned int l,
			     unsigned int session /*1:primary, 2:external, 3:memory */)
{
	return 0;
}

int dprec_mmp_dump_wdma_layer(void *wdma_layer, unsigned int wdma_num)
{
	return 0;
}

int dprec_mmp_dump_rdma_layer(void *rdma_layer, unsigned int rdma_num)
{
	return 0;
}

int dprec_logger_pr(unsigned int type, char *fmt, ...)
{
	return 0;
}

int dprec_logger_get_buf(DPREC_LOGGER_PR_TYPE type, char *stringbuf, int len)
{
	return 0;
}

ssize_t dprec_read_from_buffer(void __user *to, size_t count, loff_t *ppos,
				const void *from, size_t available)
{
	loff_t pos = *ppos;
	size_t ret;

	if (pos < 0)
		return -EINVAL;

	ret = copy_to_user(to, from, available);
	if (ret == available)
		return -EFAULT;
	available -= ret;
	*ppos = pos + available;
	return available;
}
#endif
