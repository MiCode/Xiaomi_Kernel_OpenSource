/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MACH_STM_H
#define __MACH_STM_H

enum {
	OST_ENTITY_NONE			= 0x0,
	OST_ENTITY_FTRACE_EVENTS	= 0x1,
	OST_ENTITY_TRACE_PRINTK		= 0x2,
	OST_ENTITY_TRACE_MARKER		= 0x4,
	OST_ENTITY_DEV_NODE		= 0x8,
	OST_ENTITY_ALL			= 0xF,
};

enum {
	STM_OPTION_NONE			= 0x0,
	STM_OPTION_TIMESTAMPED		= 0x08,
	STM_OPTION_GUARANTEED		= 0x80,
};

#define stm_log_inv(entity_id, proto_id, data, size)			\
	stm_trace(STM_OPTION_NONE, entity_id, proto_id, data, size)

#define stm_log_inv_ts(entity_id, proto_id, data, size)			\
	stm_trace(STM_OPTION_TIMESTAMPED, entity_id, proto_id,		\
		  data, size)

#define stm_log_gtd(entity_id, proto_id, data, size)			\
	stm_trace(STM_OPTION_GUARANTEED, entity_id, proto_id,		\
		  data, size)

#define stm_log_gtd_ts(entity_id, proto_id, data, size)			\
	stm_trace(STM_OPTION_GUARANTEED | STM_OPTION_TIMESTAMPED,	\
		  entity_id, proto_id, data, size)

#define stm_log(entity_id, data, size)					\
	stm_log_inv_ts(entity_id, 0, data, size)

#ifdef CONFIG_MSM_QDSS
extern int stm_trace(uint32_t options, uint8_t entity_id, uint8_t proto_id,
		     const void *data, uint32_t size);
#else
static inline int stm_trace(uint32_t options, uint8_t entity_id,
			    uint8_t proto_id, const void *data, uint32_t size)
{
	return 0;
}
#endif

#endif
