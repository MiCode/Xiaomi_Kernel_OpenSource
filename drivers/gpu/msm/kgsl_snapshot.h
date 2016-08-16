/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _KGSL_SNAPSHOT_H_
#define _KGSL_SNAPSHOT_H_

#include <linux/types.h>

/* Snapshot header */

/* High word is static, low word is snapshot version ID */
#define SNAPSHOT_MAGIC 0x504D0002

/* GPU ID scheme:
 * [16:31] - core identifer (0x0002 for 2D or 0x0003 for 3D)
 * [00:16] - GPU specific identifier
 */

struct kgsl_snapshot_header {
	__u32 magic; /* Magic identifier */
	__u32 gpuid; /* GPU ID - see above */
	/* Added in snapshot version 2 */
	__u32 chipid; /* Chip ID from the GPU */
} __packed;

/* Section header */
#define SNAPSHOT_SECTION_MAGIC 0xABCD

struct kgsl_snapshot_section_header {
	__u16 magic; /* Magic identifier */
	__u16 id;    /* Type of section */
	__u32 size;  /* Size of the section including this header */
} __packed;

/* Section identifiers */
#define KGSL_SNAPSHOT_SECTION_OS           0x0101
#define KGSL_SNAPSHOT_SECTION_REGS         0x0201
#define KGSL_SNAPSHOT_SECTION_RB           0x0301
#define KGSL_SNAPSHOT_SECTION_RB_V2        0x0302
#define KGSL_SNAPSHOT_SECTION_IB           0x0401
#define KGSL_SNAPSHOT_SECTION_IB_V2        0x0402
#define KGSL_SNAPSHOT_SECTION_INDEXED_REGS 0x0501
#define KGSL_SNAPSHOT_SECTION_ISTORE       0x0801
#define KGSL_SNAPSHOT_SECTION_DEBUG        0x0901
#define KGSL_SNAPSHOT_SECTION_DEBUGBUS     0x0A01
#define KGSL_SNAPSHOT_SECTION_GPU_OBJECT   0x0B01
#define KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2 0x0B02
#define KGSL_SNAPSHOT_SECTION_MEMLIST      0x0E01
#define KGSL_SNAPSHOT_SECTION_MEMLIST_V2   0x0E02
#define KGSL_SNAPSHOT_SECTION_SHADER       0x1201

#define KGSL_SNAPSHOT_SECTION_END          0xFFFF

/* OS sub-section header */
#define KGSL_SNAPSHOT_OS_LINUX             0x0001
#define KGSL_SNAPSHOT_OS_LINUX_V3          0x00000202

/* Linux OS specific information */
struct kgsl_snapshot_linux {
	int osid;                   /* subsection OS identifier */
	int state;		    /* 1 if the thread is running, 0 for hung */
	__u32 seconds;		    /* Unix timestamp for the snapshot */
	__u32 power_flags;            /* Current power flags */
	__u32 power_level;            /* Current power level */
	__u32 power_interval_timeout; /* Power interval timeout */
	__u32 grpclk;                 /* Current GP clock value */
	__u32 busclk;		    /* Current busclk value */
	__u32 ptbase;		    /* Current ptbase */
	__u32 pid;		    /* PID of the process that owns the PT */
	__u32 current_context;	    /* ID of the current context */
	__u32 ctxtcount;	    /* Number of contexts appended to section */
	unsigned char release[32];  /* kernel release */
	unsigned char version[32];  /* kernel version */
	unsigned char comm[16];	    /* Name of the process that owns the PT */
} __packed;

struct kgsl_snapshot_linux_v2 {
	int osid;                   /* subsection OS identifier */
	__u32 seconds;		    /* Unix timestamp for the snapshot */
	__u32 power_flags;            /* Current power flags */
	__u32 power_level;            /* Current power level */
	__u32 power_interval_timeout; /* Power interval timeout */
	__u32 grpclk;                 /* Current GP clock value */
	__u32 busclk;		    /* Current busclk value */
	__u64 ptbase;		    /* Current ptbase */
	__u32 pid;		    /* PID of the process that owns the PT */
	__u32 current_context;	    /* ID of the current context */
	__u32 ctxtcount;	    /* Number of contexts appended to section */
	unsigned char release[32];  /* kernel release */
	unsigned char version[32];  /* kernel version */
	unsigned char comm[16];	    /* Name of the process that owns the PT */
} __packed;

/*
 * This structure contains a record of an active context.
 * These are appended one after another in the OS section below
 * the header above
 */

struct kgsl_snapshot_linux_context {
	__u32 id;			/* The context ID */
	__u32 timestamp_queued;		/* The last queued timestamp */
	__u32 timestamp_retired;	/* The last timestamp retired by HW */
};

struct kgsl_snapshot_linux_context_v2 {
	__u32 id;			/* The context ID */
	__u32 timestamp_queued;		/* The last queued timestamp */
	__u32 timestamp_consumed;	/* The last timestamp consumed by HW */
	__u32 timestamp_retired;	/* The last timestamp retired by HW */
};
/* Ringbuffer sub-section header */
struct kgsl_snapshot_rb {
	int start;  /* dword at the start of the dump */
	int end;    /* dword at the end of the dump */
	int rbsize; /* Size (in dwords) of the ringbuffer */
	int wptr;   /* Current index of the CPU write pointer */
	int rptr;   /* Current index of the GPU read pointer */
	int count;  /* Number of dwords in the dump */
	__u32 timestamp_queued; /* The last queued timestamp */
	__u32 timestamp_retired; /* The last timestamp retired by HW */
} __packed;

struct kgsl_snapshot_rb_v2 {
	int start;  /* dword at the start of the dump */
	int end;    /* dword at the end of the dump */
	int rbsize; /* Size (in dwords) of the ringbuffer */
	int wptr;   /* Current index of the CPU write pointer */
	int rptr;   /* Current index of the GPU read pointer */
	int count;  /* Number of dwords in the dump */
	__u32 timestamp_queued; /* The last queued timestamp */
	__u32 timestamp_retired; /* The last timestamp retired by HW */
	__u64 gpuaddr; /* The GPU address of the ringbuffer */
	__u32 id; /* Ringbuffer identifier */
} __packed;


/* Replay or Memory list section, both sections have same header */
struct kgsl_snapshot_replay_mem_list {
	/*
	 * Number of IBs to replay for replay section or
	 * number of memory list entries for mem list section
	 */
	int num_entries;
	/* Pagetable base to which the replay IBs or memory entries belong */
	__u32 ptbase;
} __packed;

/* Replay or Memory list section, both sections have same header */
struct kgsl_snapshot_mem_list_v2 {
	/*
	 * Number of IBs to replay for replay section or
	 * number of memory list entries for mem list section
	 */
	int num_entries;
	/* Pagetable base to which the replay IBs or memory entries belong */
	__u64 ptbase;
} __packed;


/* Indirect buffer sub-section header */
struct kgsl_snapshot_ib {
	__u32 gpuaddr; /* GPU address of the the IB */
	__u32 ptbase;  /* Base for the pagetable the GPU address is valid in */
	int size;    /* Size of the IB */
} __packed;

/* Indirect buffer sub-section header (v2) */
struct kgsl_snapshot_ib_v2 {
	__u64 gpuaddr; /* GPU address of the the IB */
	__u64 ptbase;  /* Base for the pagetable the GPU address is valid in */
	__u64 size;    /* Size of the IB */
} __packed;


/* Register sub-section header */
struct kgsl_snapshot_regs {
	__u32 count; /* Number of register pairs in the section */
} __packed;

/* Indexed register sub-section header */
struct kgsl_snapshot_indexed_regs {
	__u32 index_reg; /* Offset of the index register for this section */
	__u32 data_reg;  /* Offset of the data register for this section */
	int start;     /* Starting index */
	int count;     /* Number of dwords in the data */
} __packed;

/* Istore sub-section header */
struct kgsl_snapshot_istore {
	int count;   /* Number of instructions in the istore */
} __packed;

/* Debug data sub-section header */

/* A2XX debug sections */
#define SNAPSHOT_DEBUG_SX         1
#define SNAPSHOT_DEBUG_CP         2
#define SNAPSHOT_DEBUG_SQ         3
#define SNAPSHOT_DEBUG_SQTHREAD   4
#define SNAPSHOT_DEBUG_MIU        5

/* A3XX debug sections */
#define SNAPSHOT_DEBUG_VPC_MEMORY 6
#define SNAPSHOT_DEBUG_CP_MEQ     7
#define SNAPSHOT_DEBUG_CP_PM4_RAM 8
#define SNAPSHOT_DEBUG_CP_PFP_RAM 9
#define SNAPSHOT_DEBUG_CP_ROQ     10
#define SNAPSHOT_DEBUG_SHADER_MEMORY 11
#define SNAPSHOT_DEBUG_CP_MERCIU 12

struct kgsl_snapshot_debug {
	int type;    /* Type identifier for the attached tata */
	int size;   /* Size of the section in dwords */
} __packed;

struct kgsl_snapshot_debugbus {
	int id;	   /* Debug bus ID */
	int count; /* Number of dwords in the dump */
} __packed;

struct kgsl_snapshot_shader {
	int type;  /* SP/TP statetype */
	int index; /* SP/TP index */
	int size;  /* Number of dwords in the dump */
} __packed;

#define SNAPSHOT_GPU_OBJECT_SHADER  1
#define SNAPSHOT_GPU_OBJECT_IB      2
#define SNAPSHOT_GPU_OBJECT_GENERIC 3
#define SNAPSHOT_GPU_OBJECT_DRAW    4
#define SNAPSHOT_GPU_OBJECT_GLOBAL  5

struct kgsl_snapshot_gpu_object {
	int type;      /* Type of GPU object */
	__u32 gpuaddr; /* GPU address of the the object */
	__u32 ptbase;  /* Base for the pagetable the GPU address is valid in */
	int size;    /* Size of the object (in dwords) */
};

struct kgsl_snapshot_gpu_object_v2 {
	int type;      /* Type of GPU object */
	__u64 gpuaddr; /* GPU address of the the object */
	__u64 ptbase;  /* Base for the pagetable the GPU address is valid in */
	__u64 size;    /* Size of the object (in dwords) */
} __packed;

#endif
