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
TRACE_EVENT(port_net_rx,
	    TP_PROTO(unsigned int md_id, unsigned int queue_no,
			unsigned int ccci_ch, unsigned int rx_cb_time,
			unsigned int rx_total_time), TP_ARGS(md_id, queue_no,
			ccci_ch, rx_cb_time, rx_total_time),
	    TP_STRUCT__entry(__field(unsigned int, md_id)
			__field(unsigned int, queue_no)
			__field(unsigned int, ccci_ch)
			__field(unsigned int, rx_cb_time)
			__field(unsigned int, rx_total_time)
	    ),
	    TP_fast_assign(__entry->md_id = md_id;
			   __entry->queue_no = queue_no;
			   __entry->ccci_ch = ccci_ch;
			   __entry->rx_cb_time = rx_cb_time;
			   __entry->rx_total_time = rx_total_time;
	    ),
	    TP_printk("md%u	q%d	ch%u	%u	%u",
			  __entry->md_id + 1,
			  __entry->queue_no,
			  __entry->ccci_ch,
		      __entry->rx_cb_time,
			  __entry->rx_total_time)
	);

TRACE_EVENT(port_net_tx,
	    TP_PROTO(int md_id,
			unsigned int queue_no,
			unsigned int ccci_ch,
			unsigned int req_alloc_time,
			unsigned int send_req_time,
			unsigned int tx_total_time),
			TP_ARGS(md_id, queue_no, ccci_ch,
			req_alloc_time, send_req_time,
			tx_total_time),
	    TP_STRUCT__entry(__field(unsigned int, md_id)
			     __field(unsigned int, queue_no)
			     __field(unsigned int, ccci_ch)
			     __field(unsigned int, req_alloc_time)
			     __field(unsigned int, send_req_time)
			     __field(unsigned int, tx_total_time)
	    ),
	    TP_fast_assign(__entry->md_id = md_id;
			   __entry->queue_no = queue_no;
			   __entry->ccci_ch = ccci_ch;
			   __entry->req_alloc_time = req_alloc_time;
			   __entry->send_req_time = send_req_time;
			   __entry->tx_total_time = tx_total_time;
	    ),
	    TP_printk("md%d	q%u	ch%u	%u	%u	%u",
				__entry->md_id + 1, __entry->queue_no,
				__entry->ccci_ch, __entry->req_alloc_time,
				__entry->send_req_time, __entry->tx_total_time)
	);

TRACE_EVENT(port_net_error,
	    TP_PROTO(int md_id, int queue_no, unsigned int ccci_ch,
		unsigned int error_no, unsigned int line_no),
	    TP_ARGS(md_id, queue_no, ccci_ch, error_no, line_no),
		TP_STRUCT__entry(__field(int, md_id)
			__field(int, queue_no)
			__field(int, ccci_ch)
			__field(unsigned int, error_no)
			__field(unsigned int, line_no)
	    ),
	    TP_fast_assign(__entry->md_id = md_id;
			__entry->queue_no = queue_no;
			__entry->ccci_ch = ccci_ch;
			__entry->error_no = error_no;
			__entry->line_no = line_no;
	    ),
	    TP_printk("md%d	q%d	ch=%d	err=%u	line_no=%u",
		(__entry->md_id + 1), __entry->queue_no,
			__entry->ccci_ch, __entry->error_no, __entry->line_no)
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
#define TRACE_INCLUDE_FILE port_net_events
#include <trace/define_trace.h>
