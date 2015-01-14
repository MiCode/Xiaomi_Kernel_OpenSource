/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2014 Intel Corporation.
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel Corporation
 * or licensors. Title to the Material remains with Intel
 * Corporation or its licensors. The Material contains trade
 * secrets and proprietary and confidential information of Intel or its
 * licensors. The Material is protected by worldwide copyright
 * and trade secret laws and treaty provisions. No part of the Material may
 * be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intel's prior
 * express written permission.
 *
 * No License under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or
 * delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */
#ifndef _TRACE_ISP_H_
#define _TRACE_ISP_H_

#include "css_trace.h"

#if !TRACE_ENABLE_ISP
#define DBG_init(buff_add, buff_size)
#define DBG_trace(verb_level, major, minor)
#define DBG_trace_value(verb_level, major, minor, value, format)
#define DBG_trace_value_24bit(verb_level, major, value, format)
#define DBG_trace_clock(verb_level, major, format)
#define DBG_trace_clock_delta(verb_level, major, format)
#else

#include <hrt/master_port.h>
#include "timer.isp.h"

/* tracer constants */

extern volatile int cur_loc;
extern volatile unsigned short counter;
extern volatile int started, verbosity;
extern volatile unsigned int major_masks;
extern volatile hrt_address trace_buff_add;
extern volatile unsigned int max_tp;
extern volatile unsigned int last_time;

/* initialize the tracer to all 0s and init the header.
   use 32bit writes to avoid the double-write problem. */
static inline void DBG_init(unsigned int buff_add, unsigned int buff_size)
{
	int i;
	hrt_address trace_header = 0;
	hrt_address trace_data = 0;

	trace_buff_add = (hrt_address)buff_add;
	max_tp = (buff_size - TRACE_ISP_HEADER_SIZE) / TRACE_ISP_ITEM_SIZE;

	trace_header = trace_buff_add;
	trace_data = trace_header + TRACE_ISP_HEADER_SIZE;

	/* set header 1st DWORD: version & sizes */
	_hrt_master_port_store_32(trace_header, ((TRACER_VER) | (max_tp << 16)));

	/* zero all trace points */
	for (i = 0; i < max_tp; i++)
	{
		_hrt_master_port_store_32(trace_data + (TRACE_ISP_ITEM_SIZE * i), 0);
	}

	/* init control vars */
	cur_loc = started = counter = 0;

}


/* /////////////////////////////////////////////////////////////////////////////// */
/*  The FW enters the major/minor of the last locations the FW was lately.
    The "next" location is always zeroed, and can tell where the buffer started. */
/* ////////////////////////////////////////////////////////////////////////////// */

/* logging routine: set the current position, advance the pointer and zero the next location.
   increment the index only if different than prev maj/min. If not, only increment counter.
   Note: do not put major/minor = 0 ! */
static inline void DBG_trace(int verb_level, unsigned char major, unsigned char minor)
{
	unsigned int dummy = 0;
	hrt_address trace_data = trace_buff_add + TRACE_ISP_HEADER_SIZE;

	if (((unsigned int)(1 << major) & major_masks) == 0)
		return;

	if (verb_level < verbosity)
		return;

	counter++;
	if (started) {
		hrt_address prev_address = trace_data + (TRACE_ISP_ITEM_SIZE * DBG_PREV_ITEM(cur_loc, max_tp));
		dummy = _hrt_master_port_load_32(prev_address);
		if ((FIELD_MAJOR_UNPACK(dummy) == major) && (FIELD_MINOR_UNPACK(dummy) ==  minor)) {
			_hrt_master_port_store_32(prev_address,	PACK_TRACEPOINT(TRACE_DUMP_FORMAT_POINT, major, minor, counter));
			return;
		}
	}
	started = 1;
	_hrt_master_port_store_32(trace_data + (TRACE_ISP_ITEM_SIZE * cur_loc), PACK_TRACEPOINT(TRACE_DUMP_FORMAT_POINT, major, minor, counter));
	cur_loc = DBG_NEXT_ITEM(cur_loc, max_tp);
	_hrt_master_port_store_32(trace_data + (TRACE_ISP_ITEM_SIZE * cur_loc), 0);
}


/* log a tracepoint with a 16-bit value */
static inline void DBG_trace_value(
	int verb_level, 
	unsigned char major, 
	unsigned char minor,
	unsigned short value, 
	TRACE_DUMP_FORMAT format)
{
	hrt_address trace_data = trace_buff_add + TRACE_ISP_HEADER_SIZE;

	if (verb_level < verbosity)
		return;

	_hrt_master_port_store_32(
			trace_data + (TRACE_ISP_ITEM_SIZE * cur_loc),
			PACK_TRACEPOINT(format, major, minor, value));
	cur_loc = DBG_NEXT_ITEM(cur_loc, max_tp);
	_hrt_master_port_store_32(trace_data + (TRACE_ISP_ITEM_SIZE * cur_loc), 0);
}

/* log a tracepoint with a 24-bit value */
static inline void DBG_trace_value_24bit(int verb_level, unsigned char major, unsigned int value, TRACE_DUMP_FORMAT format)
{
	hrt_address trace_data = trace_buff_add + TRACE_ISP_HEADER_SIZE;

	if (verb_level < verbosity)
		return;

	_hrt_master_port_store_32(
			trace_data + (TRACE_ISP_ITEM_SIZE * cur_loc),
			PACK_TRACE_VALUE24(format, major, value));
	cur_loc = DBG_NEXT_ITEM(cur_loc, max_tp);
	_hrt_master_port_store_32(trace_data + (TRACE_ISP_ITEM_SIZE * cur_loc), 0);
}

/* trace a clock value */
static inline void DBG_trace_clock(int verb_level, unsigned char major, TRACE_DUMP_FORMAT format)
{
	volatile unsigned int clock_value = tmr_clock_read();

	if (verb_level < verbosity)
		return;

	DBG_trace_value_24bit(verb_level, major, clock_value, format);
	last_time = clock_value;
}

/* trace a clock delta value */
static inline void DBG_trace_clock_delta(int verb_level, unsigned char major, TRACE_DUMP_FORMAT format)
{
	unsigned int clock_value = tmr_clock_read();
	if (verb_level < verbosity)
		return;

	DBG_trace_value_24bit(verb_level, major, clock_value - last_time, format);
	last_time = clock_value;

}

#endif /*  !TRACE_ENABLE_ISP */

#endif /* _TRACE_ISP_H_ */
