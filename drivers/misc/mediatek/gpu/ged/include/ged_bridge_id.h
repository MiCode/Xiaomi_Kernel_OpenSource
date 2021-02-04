/*
 * Copyright (C) 2011-2017 MediaTek Inc.
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

#ifndef __GED_BRIDGE_ID_H__
#define __GED_BRIDGE_ID_H__

#include "ged_type.h"

typedef struct _GED_BRIDGE_PACKAGE {
	uint32_t ui32FunctionID;
	int32_t i32Size;
	void *pvParamIn;
	int32_t i32InBufferSize;
	void *pvParamOut;
	int32_t i32OutBufferSize;
} GED_BRIDGE_PACKAGE;

#define GPU_TUNER_BUF_NAME_LEN 128

/*****************************************************************************
 *  IOCTL values.
 *****************************************************************************/

#define GED_MAGIC 'g'

#define GED_IO(INDEX)    _IO(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_IOW(INDEX)   _IOW(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_IOR(INDEX)   _IOR(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_IOWR(INDEX)  _IOWR(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_GET_BRIDGE_ID(X)  _IOC_NR(X)

/******************************************************************************
 *  IOCTL Commands
 ******************************************************************************/
typedef enum {
	GED_BRIDGE_COMMAND_LOG_BUF_GET = 0,
	GED_BRIDGE_COMMAND_LOG_BUF_WRITE = 1,
	GED_BRIDGE_COMMAND_LOG_BUF_RESET = 2,
	GED_BRIDGE_COMMAND_BOOST_GPU_FREQ = 3,
	GED_BRIDGE_COMMAND_MONITOR_3D_FENCE = 4,
	GED_BRIDGE_COMMAND_QUERY_INFO = 5,
	GED_BRIDGE_COMMAND_NOTIFY_VSYNC = 6,
	GED_BRIDGE_COMMAND_DVFS_PROBE = 7,
	GED_BRIDGE_COMMAND_DVFS_UM_RETURN = 8,
	GED_BRIDGE_COMMAND_EVENT_NOTIFY = 9,
	GED_BRIDGE_COMMAND_WAIT_HW_VSYNC = 10,
	GED_BRIDGE_COMMAND_QUERY_TARGET_FPS = 11,
	GED_BRIDGE_COMMAND_VSYNC_WAIT = 12,
	GED_BRIDGE_COMMAND_GPU_HINT_TO_CPU = 13,
	GED_BRIDGE_COMMAND_HINT_FORCE_MDP = 14,

	GED_BRIDGE_COMMAND_GE_ALLOC = 100,
	GED_BRIDGE_COMMAND_GE_GET = 101,
	GED_BRIDGE_COMMAND_GE_SET = 102,
	GED_BRIDGE_COMMAND_GPU_TIMESTAMP = 103,
	GED_BRIDGE_COMMAND_TARGET_FPS = 104,
	GED_BRIDGE_COMMAND_GE_INFO = 105,
	GED_BRIDGE_COMMAND_GPU_TUNER_STATUS = 106,
} GED_BRIDGE_COMMAND_ID;

#define GED_BRIDGE_IO_LOG_BUF_GET           GED_IOWR(GED_BRIDGE_COMMAND_LOG_BUF_GET)
#define GED_BRIDGE_IO_LOG_BUF_WRITE         GED_IOWR(GED_BRIDGE_COMMAND_LOG_BUF_WRITE)
#define GED_BRIDGE_IO_LOG_BUF_RESET         GED_IOWR(GED_BRIDGE_COMMAND_LOG_BUF_RESET)
#define GED_BRIDGE_IO_BOOST_GPU_FREQ        GED_IOWR(GED_BRIDGE_COMMAND_BOOST_GPU_FREQ)
#define GED_BRIDGE_IO_MONITOR_3D_FENCE      GED_IOWR(GED_BRIDGE_COMMAND_MONITOR_3D_FENCE)
#define GED_BRIDGE_IO_QUERY_INFO            GED_IOWR(GED_BRIDGE_COMMAND_QUERY_INFO)
#define GED_BRIDGE_IO_NOTIFY_VSYNC          GED_IOWR(GED_BRIDGE_COMMAND_NOTIFY_VSYNC)
#define GED_BRIDGE_IO_DVFS_PROBE            GED_IOWR(GED_BRIDGE_COMMAND_DVFS_PROBE)
#define GED_BRIDGE_IO_DVFS_UM_RETURN        GED_IOWR(GED_BRIDGE_COMMAND_DVFS_UM_RETURN)
#define GED_BRIDGE_IO_EVENT_NOTIFY          GED_IOWR(GED_BRIDGE_COMMAND_EVENT_NOTIFY)
#define GED_BRIDGE_IO_WAIT_HW_VSYNC         GED_IOWR(GED_BRIDGE_COMMAND_WAIT_HW_VSYNC)
#define GED_BRIDGE_IO_VSYNC_WAIT        GED_IOWR(GED_BRIDGE_COMMAND_VSYNC_WAIT)
#define GED_BRIDGE_IO_GPU_HINT_TO_CPU \
	GED_IOWR(GED_BRIDGE_COMMAND_GPU_HINT_TO_CPU)
#define GED_BRIDGE_IO_HINT_FORCE_MDP \
	GED_IOWR(GED_BRIDGE_COMMAND_HINT_FORCE_MDP)

#define GED_BRIDGE_IO_GE_ALLOC              GED_IOWR(GED_BRIDGE_COMMAND_GE_ALLOC)
#define GED_BRIDGE_IO_GE_GET                GED_IOWR(GED_BRIDGE_COMMAND_GE_GET)
#define GED_BRIDGE_IO_GE_SET                GED_IOWR(GED_BRIDGE_COMMAND_GE_SET)
#define GED_BRIDGE_IO_QUERY_TARGET_FPS      GED_IOWR(GED_BRIDGE_COMMAND_QUERY_TARGET_FPS)
#define GED_BRIDGE_IO_GPU_TIMESTAMP         GED_IOWR(GED_BRIDGE_COMMAND_GPU_TIMESTAMP)
#define GED_BRIDGE_IO_GE_INFO               GED_IOWR(GED_BRIDGE_COMMAND_GE_INFO)
#define GED_BRIDGE_IO_GPU_TUNER_STATUS \
	GED_IOWR(GED_BRIDGE_COMMAND_GPU_TUNER_STATUS)

/******************************************************************************
 *  LOG_BUF_GET
 ******************************************************************************/

#define GED_LOG_BUF_NAME_LENGTH 64

/* Bridge in structure for LOG_BUF_GET */
typedef struct GED_BRIDGE_IN_LOGBUFGET_TAG {
	char acName[GED_LOG_BUF_NAME_LENGTH];
} GED_BRIDGE_IN_LOGBUFGET;


/* Bridge out structure for LOG_BUF_GETC */
typedef struct GED_BRIDGE_OUT_LOGBUFGET_TAG {
	GED_ERROR eError;
	GED_LOG_BUF_HANDLE hLogBuf;
} GED_BRIDGE_OUT_LOGBUFGET;

/******************************************************************************
 *  LOG_BUF_WRITE
 ******************************************************************************/

/* Bridge in structure for LOG_BUF_WRITE */
typedef struct GED_BRIDGE_IN_LOGBUFWRITE_TAG {
	GED_LOG_BUF_HANDLE hLogBuf;
	int attrs;
	char acLogBuf[GED_BRIDGE_IN_LOGBUF_SIZE];
} GED_BRIDGE_IN_LOGBUFWRITE;

/* Bridge out structure for LOG_BUF_WRITE */
typedef struct GED_BRIDGE_OUT_LOGBUFWRITE_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_LOGBUFWRITE;

/******************************************************************************
 *  LOG_BUF_RESET
 ******************************************************************************/

/* Bridge in structure for LOG_BUF_RESET */
typedef struct GED_BRIDGE_IN_LOGBUFRESET_TAG {
	GED_LOG_BUF_HANDLE hLogBuf;
} GED_BRIDGE_IN_LOGBUFRESET;

/* Bridge out structure for LOG_BUF_RESET */
typedef struct GED_BRIDGE_OUT_LOGBUFRESET_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_LOGBUFRESET;

/******************************************************************************
 *  BOOST GPU FREQ
 ******************************************************************************/

/* Bridge in structure for LOG_BUF_WRITE */
typedef struct GED_BRIDGE_IN_BOOSTGPUFREQ_TAG {
	GED_BOOST_GPU_FREQ_LEVEL eGPUFreqLevel;
} GED_BRIDGE_IN_BOOSTGPUFREQ;

/* Bridge out structure for LOG_BUF_WRITE */
typedef struct GED_BRIDGE_OUT_BOOSTGPUFREQ_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_BOOSTGPUFREQ;

/*****************************************************************************
 *  MONITOR 3D FENCE
 *****************************************************************************/

/* Bridge in structure for MONITOR3DFENCE */
typedef struct GED_BRIDGE_IN_MONITOR3DFENCE_TAG {
	int fd;
} GED_BRIDGE_IN_MONITOR3DFENCE;

/* Bridge out structure for MONITOR3DFENCE */
typedef struct GED_BRIDGE_OUT_MONITOR3DFENCE_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_MONITOR3DFENCE;

/*****************************************************************************
 *  QUERY INFO
 *****************************************************************************/

/* Bridge in structure for QUERY INFO*/
typedef struct GED_BRIDGE_IN_QUERY_INFO_TAG {
	GED_INFO eType;
} GED_BRIDGE_IN_QUERY_INFO;


/* Bridge out structure for QUERY INFO*/
typedef struct GED_BRIDGE_OUT_QUERY_INFO_TAG {
	uint64_t   retrieve;
} GED_BRIDGE_OUT_QUERY_INFO;

/*****************************************************************************
 *  NOTIFY VSYNC
 *****************************************************************************/

/* Bridge in structure for VSYNCEVENT */
typedef struct GED_BRIDGE_IN_NOTIFY_VSYNC_TAG {
	GED_VSYNC_TYPE eType;
} GED_BRIDGE_IN_NOTIFY_VSYNC;

/* Bridge out structure for VSYNCEVENT */
typedef struct GED_BRIDGE_OUT_NOTIFY_VSYNC_TAG {
	GED_DVFS_UM_QUERY_PACK sQueryData;
	GED_ERROR eError;
} GED_BRIDGE_OUT_NOTIFY_VSYNC;

/*****************************************************************************
 *  DVFS PROBE
 *****************************************************************************/

/* Bridge in structure for DVFS_PROBE */
typedef struct GED_BRIDGE_IN_DVFS_PROBE_TAG {
	int          pid;
} GED_BRIDGE_IN_DVFS_PROBE;

/* Bridge out structure for DVFS_PROBE */
typedef struct GED_BRIDGE_OUT_DVFS_PROBE_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_DVFS_PROBE;

/*****************************************************************************
 *  DVFS UM RETURN
 *****************************************************************************/

/* Bridge in structure for DVFS_UM_RETURN */
typedef struct GED_BRIDGE_IN_DVFS_UM_RETURN_TAG {
	uint64_t gpu_tar_freq;
	bool bFallback;
} GED_BRIDGE_IN_DVFS_UM_RETURN;

/* Bridge out structure for DVFS_UM_RETURN */
typedef struct GED_BRIDGE_OUT_DVFS_UM_RETURN_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_DVFS_UM_RETURN;

/*****************************************************************************
 *  EVENT NOTIFY
 *****************************************************************************/

/* Bridge in structure for EVENT_NOTIFY */
typedef struct GED_BRIDGE_IN_EVENT_NOTIFY_TAG {
	GED_DVFS_VSYNC_OFFSET_SWITCH_CMD eEvent;
	bool bSwitch;
} GED_BRIDGE_IN_EVENT_NOTIFY;

/* Bridge out structure for EVENT_NOTIFY */
typedef struct GED_BRIDGE_OUT_EVENT_NOTIFY_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_EVENT_NOTIFY;

/*****************************************************************************
 *  WAIT HW VSync
 *****************************************************************************/

/* Bridge in structure for creation */
typedef struct GED_BRIDGE_IN_WAIT_HW_VSYNC_TAG {
	int tid;
} GED_BRIDGE_IN_WAIT_HW_VSYNC;

/* Bridge out structure for creation */
typedef struct GED_BRIDGE_OUT_WAIT_HW_VSYNC_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_WAIT_HW_VSYNC;

/*****************************************************************************
 *  GPU_TIMESTAMP
 *****************************************************************************/

/* Bridge in structure for creation */
typedef struct GED_BRIDGE_IN_GPU_TIMESTAMP_TAG {
	int pid;
	uint64_t ullWnd;
	int32_t i32FrameID;
	int fence_fd;
	int QedBuffer_length;
	int isSF;
} GED_BRIDGE_IN_GPU_TIMESTAMP;

/* Bridge out structure for creation */
typedef struct GED_BRIDGE_OUT_GPU_TIMESTAMP_TAG {
	GED_ERROR eError;
	int is_ged_kpi_enabled;
} GED_BRIDGE_OUT_GPU_TIMESTAMP;

/*****************************************************************************
 *  QUERY_TARGET_FPS (for FRR20)
 *****************************************************************************/

typedef struct GED_BRIDGE_IN_QUERY_TARGET_FPS_TAG {
	int pid;
	uint64_t cid;
	int fenceFd;
} GED_BRIDGE_IN_QUERY_TARGET_FPS;

typedef struct GED_BRIDGE_OUT_QUERY_TARGET_FPS_TAG {
	int fps;
} GED_BRIDGE_OUT_QUERY_TARGET_FPS;

/******************************************************************************
 *  BOOST GPU FREQ
 ******************************************************************************/
struct GED_BRIDGE_IN_GPU_HINT_TO_CPU {
	int32_t i32BridgeFD;
	int32_t tid;
	int32_t hint;
};

struct GED_BRIDGE_OUT_GPU_HINT_TO_CPU {
	GED_ERROR eError;
	int32_t boost_flag; // 1:boost 0:not_boost
	int32_t boost_value;
};

/******************************************************************************
 *  HINT VIDEO CODEC FORCE MDP
 ******************************************************************************/
struct GED_BRIDGE_IN_HINT_FORCE_MDP {
	int32_t i32BridgeFD;
	int32_t hint; /* 1: Do MDP, 0: No MDP, -1: No overwrite */
};

struct GED_BRIDGE_OUT_HINT_FORCE_MDP {
	GED_ERROR eError;
	int32_t mdp_flag; /* 1: Do MDP, 0: No MDP */
};

/*****************************************************************************
 *  GE - gralloc_extra functions
 *****************************************************************************/

/* Bridge in structure for GE_ALLOC */
typedef struct GED_BRIDGE_IN_GE_ALLOC_TAG {
	int region_num;
	uint32_t region_sizes[0];
} GED_BRIDGE_IN_GE_ALLOC;

/* Bridge out structure for GE_ALLOC */
typedef struct GED_BRIDGE_OUT_GE_ALLOC_TAG {
	int ge_fd;
	GED_ERROR eError;
} GED_BRIDGE_OUT_GE_ALLOC;

/* Bridge in structure for GE_GET */
typedef struct GED_BRIDGE_IN_GE_GET_TAG {
	int ge_fd;
	int region_id;
	/* Here uint32_* means that the unit is 32bit.
	 * For example: uint32_offset 1 => offset 4 bytes
	 */
	int uint32_offset;
	int uint32_size;
} GED_BRIDGE_IN_GE_GET;

/* Bridge out structure for GE_GET */
typedef struct GED_BRIDGE_OUT_GE_GET_TAG {
	GED_ERROR eError;
	uint32_t data[0];
} GED_BRIDGE_OUT_GE_GET;

/* Bridge in structure for GE_SET */
typedef struct GED_BRIDGE_IN_GE_SET_TAG {
	int ge_fd;
	int region_id;
	/* Here uint32_* means that the unit is 32bit.
	 * For example: uint32_offset 1 => offset 4 bytes
	 */
	int uint32_offset;
	int uint32_size;
	uint32_t data[0];
} GED_BRIDGE_IN_GE_SET;

/* Bridge out structure for GE_SET */
typedef struct GED_BRIDGE_OUT_GE_SET_TAG {
	GED_ERROR eError;
} GED_BRIDGE_OUT_GE_SET;

/* Bridge in structure for GE_INFO */
typedef struct GED_BRIDGE_IN_GE_INFO_TAG {
	int ge_fd;
} GED_BRIDGE_IN_GE_INFO;

/* Bridge out structure for GE_INFO */
typedef struct GED_BRIDGE_OUT_GE_INFO_TAG {
	uint64_t unique_id;
	GED_ERROR eError;
} GED_BRIDGE_OUT_GE_INFO;

/* Bridge in structure for GPU_TUNER_STATUS */
struct GED_BRIDGE_IN_GPU_TUNER_STATUS {
	char name[GPU_TUNER_BUF_NAME_LEN];
};

/* Bridge out structure for GPU_TUNER_STATUS */
struct GED_BRIDGE_OUT_GPU_TUNER_STATUS {
	int feature;
};

#endif
