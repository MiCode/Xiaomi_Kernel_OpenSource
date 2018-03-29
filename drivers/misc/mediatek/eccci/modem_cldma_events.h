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

/*
 * If TRACE_SYSTEM is defined, that will be the directory created
 * in the ftrace directory under /sys/kernel/debug/tracing/events/<system>
 *
 * The define_trace.h below will also look for a file name of
 * TRACE_SYSTEM.h where TRACE_SYSTEM is what is defined here.
 * In this case, it would look for sample.h
 *
 * If the header name will be different than the system name
 * (as in this case), then you can override the header name that
 * define_trace.h will look up by defining TRACE_INCLUDE_FILE
 *
 * This file is called trace-events-sample.h but we want the system
 * to be called "sample". Therefore we must define the name of this
 * file:
 *
 * #define TRACE_INCLUDE_FILE trace-events-sample
 *
 * As we do an the bottom of this file.
 *
 * Notice that TRACE_SYSTEM should be defined outside of #if
 * protection, just like TRACE_INCLUDE_FILE.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ccci

/*
 * Notice that this file is not protected like a normal header.
 * We also must allow for rereading of this file. The
 *
 *  || defined(TRACE_HEADER_MULTI_READ)
 *
 * serves this purpose.
 */
#if !defined(_TRACE_EVENT_SAMPLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_SAMPLE_H

/*
 * All trace headers should include tracepoint.h, until we finally
 * make it into a standard header.
 */
#include <linux/tracepoint.h>

/*
 * The TRACE_EVENT macro is broken up into 5 parts.
 *
 * name: name of the trace point. This is also how to enable the tracepoint.
 *   A function called trace_foo_bar() will be created.
 *
 * proto: the prototype of the function trace_foo_bar()
 *   Here it is trace_foo_bar(char *foo, int bar).
 *
 * args:  must match the arguments in the prototype.
 *    Here it is simply "foo, bar".
 *
 * struct:  This defines the way the data will be stored in the ring buffer.
 *    There are currently two types of elements. __field and __array.
 *    a __field is broken up into (type, name). Where type can be any
 *    type but an array.
 *    For an array. there are three fields. (type, name, size). The
 *    type of elements in the array, the name of the field and the size
 *    of the array.
 *
 *    __array( char, foo, 10) is the same as saying   char foo[10].
 *
 * fast_assign: This is a C like function that is used to store the items
 *    into the ring buffer.
 *
 * printk: This is a way to print out the data in pretty print. This is
 *    useful if the system crashes and you are logging via a serial line,
 *    the data can be printed to the console using this "printk" method.
 *
 * Note, that for both the assign and the printk, __entry is the handler
 * to the data structure in the ring buffer, and is defined by the
 * TP_STRUCT__entry.
 */

TRACE_EVENT(cldma_irq,
	TP_PROTO(unsigned int irq_type, unsigned int queue_mask),
	TP_ARGS(irq_type, queue_mask), TP_STRUCT__entry(__field(unsigned int, irq_type)
		__field(unsigned int, queue_mask)
	),
	TP_fast_assign(__entry->irq_type = irq_type; __entry->queue_mask = queue_mask;
	),
	TP_printk("%s	%u", ((__entry->irq_type == 0) ? "TX" : "RX"), __entry->queue_mask)
	);

TRACE_EVENT(cldma_rx,
	TP_PROTO(int queue_no, int ccci_ch, unsigned long long req_alloc_time, unsigned long long port_recv_time,
		unsigned long long skb_alloc_time, unsigned long long rx_cost_time, unsigned long long rx_bytes),
	TP_ARGS(queue_no, ccci_ch, req_alloc_time, port_recv_time, skb_alloc_time, rx_cost_time, rx_bytes),
	TP_STRUCT__entry(__field(int, queue_no)
		__field(int, ccci_ch)
		__field(unsigned long long, req_alloc_time)
		__field(unsigned long long, port_recv_time)
		__field(unsigned long long, skb_alloc_time)
		__field(unsigned long long, rx_cost_time)
		__field(unsigned long long, rx_bytes)
	),
	TP_fast_assign(__entry->queue_no = queue_no;
		__entry->ccci_ch = ccci_ch;
		__entry->req_alloc_time = req_alloc_time;
		__entry->port_recv_time = port_recv_time;
		__entry->skb_alloc_time = skb_alloc_time;
		__entry->rx_cost_time = rx_cost_time; __entry->rx_bytes = rx_bytes;
	),
	TP_printk("q%d	ch%d	%llu	%llu	%llu	%llu	%llu", __entry->queue_no, __entry->ccci_ch,
		__entry->req_alloc_time, __entry->port_recv_time, __entry->skb_alloc_time,
		__entry->rx_cost_time, __entry->rx_bytes)
	);

TRACE_EVENT(cldma_rx_done,
	TP_PROTO(int queue_no, unsigned long long rx_interal, unsigned long long total_time,
		unsigned int total_count, unsigned long long rx_bytes, unsigned long long sample_time,
		unsigned long long sample_rxbytes),
	TP_ARGS(queue_no, rx_interal, total_time, total_count,
		rx_bytes, sample_time, sample_rxbytes),
	TP_STRUCT__entry(__field(int, queue_no)
		__field(unsigned long long, rx_interal)
		__field(unsigned long long, total_time)
		__field(unsigned int, total_count)
		__field(unsigned long long, rx_bytes)
		__field(unsigned long long, sample_time)
		__field(unsigned long long, sample_rxbytes)
	),
	TP_fast_assign(__entry->queue_no = queue_no;
		__entry->rx_interal = rx_interal;
		__entry->total_time = total_time;
		__entry->total_count = total_count;
		__entry->rx_bytes = rx_bytes;
		__entry->sample_time = sample_time; __entry->sample_rxbytes = sample_rxbytes;
	),
	TP_printk("q%d	%llu	%llu	%u	%llu	%llu	%llu", __entry->queue_no, __entry->rx_interal,
		__entry->total_time, __entry->total_count, __entry->rx_bytes,
		__entry->sample_time, __entry->sample_rxbytes)
	);

TRACE_EVENT(cldma_tx,
	TP_PROTO(int queue_no, int ccci_ch, unsigned int free_slot, unsigned long long tx_interal,
		unsigned long long tx_cost_time, unsigned long long tx_bytes, unsigned long long sample_time,
		unsigned long long sample_txbytes),
	TP_ARGS(queue_no, ccci_ch, free_slot, tx_interal, tx_cost_time,
		tx_bytes, sample_time, sample_txbytes),
	TP_STRUCT__entry(__field(int, queue_no)
		__field(int, ccci_ch)
		__field(unsigned int, free_slot)
		__field(unsigned long long, tx_interal)
		__field(unsigned long long, tx_cost_time)
		__field(unsigned long long, tx_bytes)
		__field(unsigned long long, sample_time)
		__field(unsigned long long, sample_txbytes)
	),
	TP_fast_assign(__entry->queue_no = queue_no;
		__entry->ccci_ch = ccci_ch;
		__entry->free_slot = free_slot;
		__entry->tx_interal = tx_interal;
		__entry->tx_cost_time = tx_cost_time;
		__entry->tx_bytes = tx_bytes;
		__entry->sample_time = sample_time; __entry->sample_txbytes = sample_txbytes;
	),
	TP_printk("q%u	ch%d	%u	%llu	%llu	%llu	%llu	%llu", __entry->queue_no,
		__entry->ccci_ch, __entry->free_slot, __entry->tx_interal,
		__entry->tx_cost_time, __entry->tx_bytes, __entry->sample_time, __entry->sample_txbytes)
	);

TRACE_EVENT(cldma_tx_done,
	TP_PROTO(int queue_no, unsigned long long tx_interal, unsigned long long total_time,
		unsigned int total_count), TP_ARGS(queue_no, tx_interal, total_time, total_count),
	TP_STRUCT__entry(__field(int, queue_no)
		__field(unsigned long long, tx_interal)
		__field(unsigned long long, total_time)
		__field(unsigned int, total_count)
	),
	TP_fast_assign(__entry->queue_no = queue_no;
		__entry->total_time = tx_interal;
		__entry->total_time = total_time; __entry->total_count = total_count;
	),
	TP_printk("q%d	%llu	%llu	%u", __entry->queue_no, __entry->tx_interal, __entry->total_time,
		__entry->total_count)
	);

TRACE_EVENT(cldma_error,
	TP_PROTO(int queue_no, unsigned int ccci_ch, unsigned int error_no, unsigned int line_no),
	TP_ARGS(queue_no, ccci_ch, error_no, line_no),
	TP_STRUCT__entry(__field(int, queue_no)
		__field(int, ccci_ch)
		__field(unsigned int, error_no)
		__field(unsigned int, line_no)
	),
	TP_fast_assign(__entry->queue_no = queue_no;
		__entry->ccci_ch = ccci_ch; __entry->error_no = error_no; __entry->line_no = line_no;
	),
	TP_printk("q%d	ch=%d	err=%u	line_no=%u", __entry->queue_no, __entry->ccci_ch, __entry->error_no,
		__entry->line_no)
	);

TRACE_EVENT(ccci_skb_rx,
	TP_PROTO(unsigned long long *dl_delay),
	TP_ARGS(dl_delay),
	TP_STRUCT__entry(
		__array(unsigned long long,	dl_delay, 8)
	),

	TP_fast_assign(
		memcpy(__entry->dl_delay, dl_delay, 8*sizeof(unsigned long long));
	),

	TP_printk("	%llu	%llu	%llu	%llu	%llu	%llu	%llu	%llu",
		__entry->dl_delay[0], __entry->dl_delay[1], __entry->dl_delay[2], __entry->dl_delay[3],
		__entry->dl_delay[4], __entry->dl_delay[5], __entry->dl_delay[6], __entry->dl_delay[7])
);
#endif

/***** NOTICE! The #if protection ends here. *****/

/*
 * There are several ways I could have done this. If I left out the
 * TRACE_INCLUDE_PATH, then it would default to the kernel source
 * include/trace/events directory.
 *
 * I could specify a path from the define_trace.h file back to this
 * file.
 *
 * #define TRACE_INCLUDE_PATH ../../samples/trace_events
 *
 * But the safest and easiest way to simply make it use the directory
 * that the file is in is to add in the Makefile:
 *
 * CFLAGS_trace-events-sample.o := -I$(src)
 *
 * This will make sure the current path is part of the include
 * structure for our file so that define_trace.h can find it.
 *
 * I could have made only the top level directory the include:
 *
 * CFLAGS_trace-events-sample.o := -I$(PWD)
 *
 * And then let the path to this directory be the TRACE_INCLUDE_PATH:
 *
 * #define TRACE_INCLUDE_PATH samples/trace_events
 *
 * But then if something defines "samples" or "trace_events" as a macro
 * then we could risk that being converted too, and give us an unexpected
 * result.
 */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
/*
 * TRACE_INCLUDE_FILE is not needed if the filename and TRACE_SYSTEM are equal
 */
#define TRACE_INCLUDE_FILE modem_cldma_events
#include <trace/define_trace.h>
