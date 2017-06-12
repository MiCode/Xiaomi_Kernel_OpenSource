/* Copyright (c) 2016 The Linux Foundation. All rights reserved.
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

#ifndef MSM_SNAPSHOT_API_H_
#define MSM_SNAPSHOT_API_H_

#include <linux/types.h>

/* High word is the magic, low word is the snapshot header version */
#define SNAPSHOT_MAGIC 0x504D0002

struct msm_snapshot_header {
	__u32 magic;
	__u32 gpuid;
	__u32 chipid;
} __packed;

#define SNAPSHOT_SECTION_MAGIC 0xABCD

struct msm_snapshot_section_header {
	__u16 magic;
	__u16 id;
	__u32 size;
} __packed;

/* Section identifiers */
#define SNAPSHOT_SECTION_OS		0x0101
#define SNAPSHOT_SECTION_REGS_V2	0x0202
#define SNAPSHOT_SECTION_RB_V2		0x0302
#define SNAPSHOT_SECTION_IB_V2		0x0402
#define SNAPSHOT_SECTION_INDEXED_REGS	0x0501
#define SNAPSHOT_SECTION_DEBUG		0x0901
#define SNAPSHOT_SECTION_DEBUGBUS	0x0A01
#define SNAPSHOT_SECTION_GPU_OBJECT_V2	0x0B02
#define SNAPSHOT_SECTION_MEMLIST_V2	0x0E02
#define SNAPSHOT_SECTION_SHADER		0x1201
#define SNAPSHOT_SECTION_END		0xFFFF

#define SNAPSHOT_OS_LINUX_V3          0x00000202

struct msm_snapshot_linux {
	struct msm_snapshot_section_header header;
	int osid;
	__u32 seconds;
	__u32 power_flags;
	__u32 power_level;
	__u32 power_interval_timeout;
	__u32 grpclk;
	__u32 busclk;
	__u64 ptbase;
	__u32 pid;
	__u32 current_context;
	__u32 ctxtcount;
	unsigned char release[32];
	unsigned char version[32];
	unsigned char comm[16];
} __packed;

struct msm_snapshot_ringbuffer {
	struct msm_snapshot_section_header header;
	int start;
	int end;
	int rbsize;
	int wptr;
	int rptr;
	int count;
	__u32 timestamp_queued;
	__u32 timestamp_retired;
	__u64 gpuaddr;
	__u32 id;
} __packed;

struct msm_snapshot_regs {
	struct msm_snapshot_section_header header;
	__u32 count;
} __packed;

struct msm_snapshot_indexed_regs {
	struct msm_snapshot_section_header header;
	__u32 index_reg;
	__u32 data_reg;
	__u32 start;
	__u32 count;
} __packed;

#define SNAPSHOT_DEBUG_CP_MEQ		7
#define SNAPSHOT_DEBUG_CP_PM4_RAM	8
#define SNAPSHOT_DEBUG_CP_PFP_RAM	9
#define SNAPSHOT_DEBUG_CP_ROQ		10
#define SNAPSHOT_DEBUG_SHADER_MEMORY	11
#define SNAPSHOT_DEBUG_CP_MERCIU	12

struct msm_snapshot_debug {
	struct msm_snapshot_section_header header;
	__u32 type;
	__u32 size;
} __packed;

struct msm_snapshot_debugbus {
	struct msm_snapshot_section_header header;
	__u32 id;
	__u32 count;
} __packed;

struct msm_snapshot_shader {
	struct msm_snapshot_section_header header;
	__u32 type;
	__u32 index;
	__u32 size;
} __packed;

#define SNAPSHOT_GPU_OBJECT_SHADER  1
#define SNAPSHOT_GPU_OBJECT_IB      2
#define SNAPSHOT_GPU_OBJECT_GENERIC 3
#define SNAPSHOT_GPU_OBJECT_DRAW    4
#define SNAPSHOT_GPU_OBJECT_GLOBAL  5

struct msm_snapshot_gpu_object {
	struct msm_snapshot_section_header header;
	__u32 type;
	__u64 gpuaddr;
	__u64 pt_base;
	__u64 size;
} __packed;
#endif
