/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_LOG_H__
#define __GED_LOG_H__

#include "ged_type.h"

#if defined(__GNUC__)
#define GED_LOG_BUF_FORMAT_PRINTF(x, y) __printf(x, y)
#else
#define GED_LOG_BUF_FORMAT_PRINTF(x, y)
#endif

#define GED_LOG_BUF_NAME_LENGTH 64
#define GED_LOG_BUF_NODE_NAME_LENGTH 64


#define GED_LOG_BUF_TYPE_RINGBUFFER			        0
#define GED_LOG_BUF_TYPE_QUEUEBUFFER			    1
#define GED_LOG_BUF_TYPE_QUEUEBUFFER_AUTO_INCREASE	2
#define GED_LOG_BUF_TYPE                            int

GED_LOG_BUF_HANDLE ged_log_buf_alloc(int i32MaxLineCount,
	int i32MaxBufferSizeByte, GED_LOG_BUF_TYPE eType, const char *pszName,
	const char *pszNodeName);

GED_ERROR ged_log_buf_resize(GED_LOG_BUF_HANDLE hLogBuf, int i32NewMaxLineCount,
	int i32NewMaxBufferSizeByte);

GED_ERROR ged_log_buf_ignore_lines(GED_LOG_BUF_HANDLE hLogBuf,
	int i32LineCount);

GED_ERROR ged_log_buf_reset(GED_LOG_BUF_HANDLE hLogBuf);

void ged_log_buf_free(GED_LOG_BUF_HANDLE hLogBuf);

/* query by Name, return NULL if not found */
GED_LOG_BUF_HANDLE ged_log_buf_get(const char *pszName);

/* register a pointer,
 * it will be set after the corresponding buffer is allcated.
 */
int ged_log_buf_get_early(const char *pszName,
	GED_LOG_BUF_HANDLE *callback_set_handle);

GED_ERROR ged_log_buf_print(GED_LOG_BUF_HANDLE hLogBuf,
	const char *fmt, ...) GED_LOG_BUF_FORMAT_PRINTF(2, 3);

GED_ERROR
ged_log_buf_print2(GED_LOG_BUF_HANDLE hLogBuf, int i32LogAttrs,
	const char *fmt, ...) GED_LOG_BUF_FORMAT_PRINTF(3, 4);

GED_ERROR ged_log_system_init(void);

void ged_log_system_exit(void);

int ged_log_buf_write(GED_LOG_BUF_HANDLE hLogBuf,
	const char __user *pszBuffer, int i32Count);

void ged_log_trace_begin(char *name);

void ged_log_trace_end(void);

void ged_log_trace_counter(char *name, int count);

void ged_log_perf_trace_counter(char *name, long long count, int pid,
	unsigned long frameID, u64 BQID);

void ged_log_perf_trace_batch_counter(char *name, long long count, int pid,
	unsigned long frameID, u64 BQID, char *batch_str);

void ged_log_dump(GED_LOG_BUF_HANDLE hLogBuf);
// Frame-based
noinline void Policy__Frame_based__Frequency(int v1, int v2);
noinline void Policy__Frame_based__Workload(int v1, int v2);
noinline void Policy__Frame_based__Workload__Source(int v1, int v2, int v3);
noinline void Policy__Frame_based__GPU_Time(int v1, int v2, int v3);
noinline void Policy__Frame_based__GPU_Time__Detail(int v1, int v2, int v3);
noinline void Policy__Frame_based__Margin(int v1, int v2, int v3);
noinline void Policy__Frame_based__Margin__Detail(unsigned int v1, int v2, int v3, int v4, int v5);
// Loading-based
noinline void Policy__Loading_based__Opp(int v1);
noinline void Policy__Loading_based__Loading(unsigned int v1, unsigned int v2);
noinline void Policy__Loading_based__Loading__Detail(unsigned int v1, unsigned int v2,
	unsigned int v3, unsigned int v4, int v5);
noinline void Policy__Loading_based__Bound(int v1, int v2, int v3, int v4);
noinline void Policy__Loading_based__Step(unsigned int v1, unsigned int v2, int v3, int v4);
noinline void Policy__Loading_based__GPU_Time(int v1, int v2, int v3, int v4, int v5);
noinline void Policy__Loading_based__Margin(int v1, int v2, int v3);
noinline void Policy__Loading_based__Margin__Detail(unsigned int v1, int v2, int v3,
	unsigned int v4, int v5);
// DCS
noinline void Policy__DCS(int v1, int v2);
noinline void Policy__DCS__Detail(unsigned int v1);
// Common
noinline void Policy__Common__Commit_Reason(int v1, int v2);
// Frequency
noinline void Frequency__(long long v1, unsigned long v2);

#if defined(CONFIG_GPU_MT8167) || defined(CONFIG_GPU_MT8173) ||\
defined(CONFIG_GPU_MT6739) || defined(CONFIG_GPU_MT6761)\
|| defined(CONFIG_GPU_MT6765)
extern void ged_dump_fw(void);
#endif

unsigned int is_gpu_ged_log_enable(void);

#endif
