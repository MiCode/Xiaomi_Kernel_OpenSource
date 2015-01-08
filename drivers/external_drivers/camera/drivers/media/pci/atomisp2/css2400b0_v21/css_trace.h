/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __CSS_TRACE_H_
#define __CSS_TRACE_H_

#include <type_support.h>

/*
	structs and constants for tracing
*/

/* one tracer item: major, minor and counter. The counter value can be used for GP data */
struct trace_item_t {
	uint8_t   major;
	uint8_t   minor;
	uint16_t  counter;
};

/* trace header: holds the version and the topology of the tracer. */
struct trace_header_t {
	/* 1st dword */
	uint8_t   version;
	uint8_t   max_threads;
	uint16_t  max_tracer_points;
	/* 2nd dword */
	uint32_t  command;
	/* 3rd & 4th dword */
	uint32_t  data[2];
	/* 5th & 6th dword: debug pointer mechanism */
	uint32_t  debug_ptr_signature;
	uint32_t  debug_ptr_value;
};

#define TRACER_VER			2
#define TRACE_BUFF_ADDR       0xA000
#define TRACE_BUFF_SIZE       0x1000	/* 4K allocated */

#ifdef IS_ISP_2500_SYSTEM
#define TRACE_ENABLE_SP0 1
#define TRACE_ENABLE_SP1 1
#define TRACE_ENABLE_ISP 0
#else
#define TRACE_ENABLE_SP0 0
#define TRACE_ENABLE_SP1 0
#define TRACE_ENABLE_ISP 0
#endif

typedef enum {
	TRACE_SP0_ID,
	TRACE_SP1_ID,
	TRACE_ISP_ID
} TRACE_CORE_ID;

/* TODO: add timing format? */
typedef enum {
	TRACE_DUMP_FORMAT_POINT,
	TRACE_DUMP_FORMAT_VALUE24_HEX,
	TRACE_DUMP_FORMAT_VALUE24_DEC,
	TRACE_DUMP_FORMAT_VALUE24_TIMING,
	TRACE_DUMP_FORMAT_VALUE24_TIMING_DELTA
} TRACE_DUMP_FORMAT;


/* currently divided as follows:*/
#if (TRACE_ENABLE_SP0 + TRACE_ENABLE_SP1 + TRACE_ENABLE_ISP == 3)
/* can be divided as needed */
#define TRACE_SP0_SIZE (TRACE_BUFF_SIZE/4)
#define TRACE_SP1_SIZE (TRACE_BUFF_SIZE/4)
#define TRACE_ISP_SIZE (TRACE_BUFF_SIZE/2)
#elif (TRACE_ENABLE_SP0 + TRACE_ENABLE_SP1 + TRACE_ENABLE_ISP == 2)
#if TRACE_ENABLE_SP0
#define TRACE_SP0_SIZE (TRACE_BUFF_SIZE/2)
#else
#define TRACE_SP0_SIZE (0)
#endif
#if TRACE_ENABLE_SP1
#define TRACE_SP1_SIZE (TRACE_BUFF_SIZE/2)
#else
#define TRACE_SP1_SIZE (0)
#endif
#if TRACE_ENABLE_ISP
#define TRACE_ISP_SIZE (TRACE_BUFF_SIZE/2)
#else
#define TRACE_ISP_SIZE (0)
#endif
#elif (TRACE_ENABLE_SP0 + TRACE_ENABLE_SP1 + TRACE_ENABLE_ISP == 1)
#if TRACE_ENABLE_SP0
#define TRACE_SP0_SIZE (TRACE_BUFF_SIZE)
#else
#define TRACE_SP0_SIZE (0)
#endif
#if TRACE_ENABLE_SP1
#define TRACE_SP1_SIZE (TRACE_BUFF_SIZE)
#else
#define TRACE_SP1_SIZE (0)
#endif
#if TRACE_ENABLE_ISP
#define TRACE_ISP_SIZE (TRACE_BUFF_SIZE)
#else
#define TRACE_ISP_SIZE (0)
#endif
#else
#define TRACE_SP0_SIZE (0)
#define TRACE_SP1_SIZE (0)
#define TRACE_ISP_SIZE (0)
#endif

#define TRACE_SP0_ADDR (TRACE_BUFF_ADDR)
#define TRACE_SP1_ADDR (TRACE_SP0_ADDR + TRACE_SP0_SIZE)
#define TRACE_ISP_ADDR (TRACE_SP1_ADDR + TRACE_SP1_SIZE)

/* check if it's a legal division */
#if (TRACE_BUFF_SIZE < TRACE_SP0_SIZE + TRACE_SP1_SIZE + TRACE_ISP_SIZE)
#error trace sizes are not divided correctly and are above limit
#endif

#define TRACE_SP0_HEADER_ADDR (TRACE_SP0_ADDR)
#define TRACE_SP0_HEADER_SIZE (sizeof(struct trace_header_t))
#define TRACE_SP0_ITEM_SIZE (sizeof(struct trace_item_t))
#define TRACE_SP0_DATA_ADDR (TRACE_SP0_HEADER_ADDR + TRACE_SP0_HEADER_SIZE)
#define TRACE_SP0_DATA_SIZE (TRACE_SP0_SIZE - TRACE_SP0_HEADER_SIZE)
#define TRACE_SP0_MAX_POINTS (TRACE_SP0_DATA_SIZE / TRACE_SP0_ITEM_SIZE)

#define TRACE_SP1_HEADER_ADDR (TRACE_SP1_ADDR)
#define TRACE_SP1_HEADER_SIZE (sizeof(struct trace_header_t))
#define TRACE_SP1_ITEM_SIZE (sizeof(struct trace_item_t))
#define TRACE_SP1_DATA_ADDR (TRACE_SP1_HEADER_ADDR + TRACE_SP1_HEADER_SIZE)
#define TRACE_SP1_DATA_SIZE (TRACE_SP1_SIZE - TRACE_SP1_HEADER_SIZE)
#define TRACE_SP1_MAX_POINTS (TRACE_SP1_DATA_SIZE / TRACE_SP1_ITEM_SIZE)

#define TRACE_ISP_HEADER_ADDR (TRACE_ISP_ADDR)
#define TRACE_ISP_HEADER_SIZE (sizeof(struct trace_header_t))
#define TRACE_ISP_ITEM_SIZE (sizeof(struct trace_item_t))
#define TRACE_ISP_DATA_ADDR (TRACE_ISP_HEADER_ADDR + TRACE_ISP_HEADER_SIZE)
#define TRACE_ISP_DATA_SIZE (TRACE_ISP_SIZE - TRACE_ISP_HEADER_SIZE)
#define TRACE_ISP_MAX_POINTS (TRACE_ISP_DATA_SIZE / TRACE_ISP_ITEM_SIZE)


/* offsets for master_port read/write */
#define HDR_HDR_OFFSET              0	/* offset of the header */
#define HDR_COMMAND_OFFSET          4	/* offset of the command */
#define HDR_DATA_OFFSET             8	/* offset of the command data */
#define HDR_DEBUG_SIGNATURE_OFFSET  16	/* offset of the param debug signature in trace_header_t */
#define HDR_DEBUG_POINTER_OFFSET    20	/* offset of the param debug pointer in trace_header_t */

/* common majors */
#define MAJOR_MAIN              1
#define MAJOR_ISP_STAGE_ENTRY   2
#define MAJOR_DMA_PRXY          3
#define MAJOR_START_ISP         4

#define DEBUG_PTR_SIGNATURE     0xABCD	/* signature for the debug parameter pointer */

/* command codes (1st byte) */
typedef enum {
	CMD_SET_ONE_MAJOR = 1,		/* mask in one major. 2nd byte in the command is the major code */
	CMD_UNSET_ONE_MAJOR = 2,	/* mask out one major. 2nd byte in the command is the major code */
	CMD_SET_ALL_MAJORS = 3,		/* set the major print mask. the full mask is in the data DWORD */
	CMD_SET_VERBOSITY = 4		/* set verbosity level */
} DBG_commands;

/* command signature */
#define CMD_SIGNATURE	0xAABBCC00

/* shared macros in traces infrastructure */
/* increment the pointer cyclicly */
#define DBG_NEXT_ITEM(x, max_items) (((x+1) >= max_items) ? 0 : x+1)
#define DBG_PREV_ITEM(x, max_items) ((x) ? x-1 : max_items-1)

#define FIELD_MASK(width) (((1 << (width)) - 1))
#define FIELD_PACK(value,mask,offset) (((value) & (mask)) << (offset))
#define FIELD_UNPACK(value,mask,offset) (((value) >> (offset)) & (mask))


#define FIELD_VALUE_OFFSET		(0)
#define FIELD_VALUE_WIDTH		(16)
#define FIELD_VALUE_MASK		FIELD_MASK(FIELD_VALUE_WIDTH)
#define FIELD_VALUE_PACK(f)		FIELD_PACK(f,FIELD_VALUE_MASK,FIELD_VALUE_OFFSET)
#define FIELD_VALUE_UNPACK(f)	FIELD_UNPACK(f,FIELD_VALUE_MASK,FIELD_VALUE_OFFSET)

#define FIELD_MINOR_OFFSET		(FIELD_VALUE_OFFSET + FIELD_VALUE_WIDTH)
#define FIELD_MINOR_WIDTH		(8)
#define FIELD_MINOR_MASK		FIELD_MASK(FIELD_MINOR_WIDTH)
#define FIELD_MINOR_PACK(f)		FIELD_PACK(f,FIELD_MINOR_MASK,FIELD_MINOR_OFFSET)
#define FIELD_MINOR_UNPACK(f)	FIELD_UNPACK(f,FIELD_MINOR_MASK,FIELD_MINOR_OFFSET)

#define FIELD_MAJOR_OFFSET		(FIELD_MINOR_OFFSET + FIELD_MINOR_WIDTH)
#define FIELD_MAJOR_WIDTH		(5)
#define FIELD_MAJOR_MASK		FIELD_MASK(FIELD_MAJOR_WIDTH)
#define FIELD_MAJOR_PACK(f)		FIELD_PACK(f,FIELD_MAJOR_MASK,FIELD_MAJOR_OFFSET)
#define FIELD_MAJOR_UNPACK(f)	FIELD_UNPACK(f,FIELD_MAJOR_MASK,FIELD_MAJOR_OFFSET)

#define FIELD_FORMAT_OFFSET		(FIELD_MAJOR_OFFSET + FIELD_MAJOR_WIDTH)
#define FIELD_FORMAT_WIDTH 		(3)
#define FIELD_FORMAT_MASK 		FIELD_MASK(FIELD_FORMAT_WIDTH)
#define FIELD_FORMAT_PACK(f)	FIELD_PACK(f,FIELD_FORMAT_MASK,FIELD_FORMAT_OFFSET)
#define FIELD_FORMAT_UNPACK(f)	FIELD_UNPACK(f,FIELD_FORMAT_MASK,FIELD_FORMAT_OFFSET)

#define FIELD_VALUE_24_OFFSET		(0)
#define FIELD_VALUE_24_WIDTH		(24)
#define FIELD_VALUE_24_MASK			FIELD_MASK(FIELD_VALUE_24_WIDTH)
#define FIELD_VALUE_24_PACK(f)		FIELD_PACK(f,FIELD_VALUE_24_MASK,FIELD_VALUE_24_OFFSET)
#define FIELD_VALUE_24_UNPACK(f)	FIELD_UNPACK(f,FIELD_VALUE_24_MASK,FIELD_VALUE_24_OFFSET)

#define PACK_TRACEPOINT(format,major, minor, value)	\
	(FIELD_FORMAT_PACK(format) | FIELD_MAJOR_PACK(major) | FIELD_MINOR_PACK(minor) | FIELD_VALUE_PACK(value))

#define PACK_TRACE_VALUE24(format, major, value)	\
	(FIELD_FORMAT_PACK(format) | FIELD_MAJOR_PACK(major) | FIELD_VALUE_24_PACK(value))

#endif /* __CSS_TRACE_H_ */
