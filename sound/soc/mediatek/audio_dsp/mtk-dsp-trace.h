/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-dsp-trace.h --  Mediatek ADSP platform
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chipeng <Chipeng.chang@mediatek.com>
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_snd_dsp

#if !defined(_MTK_DSP_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_DSP_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(mtk_dsp_pcm_copy_dl,
	TP_PROTO(int id, int copy_size, int avail),
	TP_ARGS(id, copy_size, avail),
	TP_STRUCT__entry(
		__field(int, id)
		__field(int, copy_size)
		__field(int, avail)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->copy_size = copy_size;
		__entry->avail = avail;
	),
	TP_printk("id:%d copy_size:%d avail:%d",  __entry->id,
		  __entry->copy_size, __entry->avail)
);

TRACE_EVENT(mtk_dsp_dl_consume_handler,
	TP_PROTO(unsigned long pRead, unsigned long pWrite, int bufLen, int datacount, int id),
	TP_ARGS(pRead, pWrite, bufLen, datacount, id),
	TP_STRUCT__entry(
		__field(unsigned long, pRead)
		__field(unsigned long, pWrite)
		__field(int, bufLen)
		__field(int, datacount)
		__field(int, id)
	),
	TP_fast_assign(
		__entry->pRead = pRead;
		__entry->pWrite = pWrite;
		__entry->bufLen = bufLen;
		__entry->datacount = datacount;
		__entry->id = id;
	),
	TP_printk("read:%lu write:%lu len:%d datacount:%d id:%d ",
		   (unsigned long)__entry->pRead, (unsigned long)__entry->pWrite,
		   __entry->bufLen, __entry->datacount, __entry->id)
);

TRACE_EVENT(mtk_dsp_check_exception,
	TP_PROTO(unsigned int param1, unsigned int param2, int underflow, int id),
	TP_ARGS(param1, param2, underflow, id),
	TP_STRUCT__entry(
		__field(unsigned int, param1)
		__field(unsigned int, param2)
		__field(int, underflow)
		__field(int, id)
	),
	TP_fast_assign(
		__entry->param1 = param1;
		__entry->param2 = param2;
		__entry->underflow = underflow;
		__entry->id = id;
	),
	TP_printk("msg param1:%u param2:%u underflow:%d id:%d",
		  __entry->param1, __entry->param2, __entry->underflow, __entry->id)
);

TRACE_EVENT(mtk_dsp_start,
	TP_PROTO(int underflow, int id),
	TP_ARGS(underflow, id),
	TP_STRUCT__entry(
		__field(int, underflow)
		__field(int, id)
	),
	TP_fast_assign(
		__entry->underflow = underflow;
		__entry->id = id;
	),
	TP_printk("underflow:%d id:%d", __entry->underflow, __entry->id)
);

TRACE_EVENT(mtk_dsp_stop,
	TP_PROTO(int id),
	TP_ARGS(id),
	TP_STRUCT__entry(
		__field(int, id)
	),
	TP_fast_assign(
		__entry->id = id;
	),
	TP_printk("id:%d",  __entry->id)
);

#endif /* _MTK_DSP_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mtk-dsp-trace
#include <trace/define_trace.h>
