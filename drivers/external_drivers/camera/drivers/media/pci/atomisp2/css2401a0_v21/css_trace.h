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


/* buffer offsets and sizes, common to SP and host */
#define TRACE_BUFF_ADDR       0xA000
#define TRACE_BUFF_SIZE       0x1000	/* 4K allocated */
#define SP1_TRACER_OFFSET     (TRACE_BUFF_SIZE/2)
#define MAX_TRACER_POINTS     (TRACE_BUFF_SIZE/sizeof(struct trace_item_t))

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

#endif /* __CSS_TRACE_H_ */
