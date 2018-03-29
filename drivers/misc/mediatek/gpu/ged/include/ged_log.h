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

#ifndef __GED_LOG_H__
#define __GED_LOG_H__

#include "ged_type.h"

#if defined(__GNUC__)
#define GED_LOG_BUF_FORMAT_PRINTF(x,y)		__attribute__((format(printf,x,y)))
#else
#define GED_LOG_BUF_FORMAT_PRINTF(x,y)
#endif

#define GED_LOG_BUF_NAME_LENGTH 64
#define GED_LOG_BUF_NODE_NAME_LENGTH 64

typedef enum GED_LOG_BUF_TYPE_TAG
{
    GED_LOG_BUF_TYPE_RINGBUFFER,
    GED_LOG_BUF_TYPE_QUEUEBUFFER,
    GED_LOG_BUF_TYPE_QUEUEBUFFER_AUTO_INCREASE,
} GED_LOG_BUF_TYPE;

GED_LOG_BUF_HANDLE ged_log_buf_alloc(int i32MaxLineCount, int i32MaxBufferSizeByte, GED_LOG_BUF_TYPE eType, const char* pszName, const char* pszNodeName);

GED_ERROR ged_log_buf_resize(GED_LOG_BUF_HANDLE hLogBuf, int i32NewMaxLineCount, int i32NewMaxBufferSizeByte);

GED_ERROR ged_log_buf_ignore_lines(GED_LOG_BUF_HANDLE hLogBuf, int i32LineCount);

GED_ERROR ged_log_buf_reset(GED_LOG_BUF_HANDLE hLogBuf);

void ged_log_buf_free(GED_LOG_BUF_HANDLE hLogBuf);

/* query by Name, return NULL if not found */
GED_LOG_BUF_HANDLE ged_log_buf_get(const char* pszName);

/* register a pointer, it will be set after the corresponding buffer is allcated. */
int ged_log_buf_get_early(const char* pszName, GED_LOG_BUF_HANDLE *callback_set_handle);

GED_ERROR ged_log_buf_print(GED_LOG_BUF_HANDLE hLogBuf, const char *fmt, ...) GED_LOG_BUF_FORMAT_PRINTF(2,3);

enum
{
    /* bit 0~7 reserved for internal used */
    GED_RESVERED                = 0xFF,

    /* log with a prefix kernel time */
    GED_LOG_ATTR_TIME           = 0x100,

    /* log with a prefix user time, pid, tid */
    GED_LOG_ATTR_TIME_TPT       = 0x200,
};

GED_ERROR ged_log_buf_print2(GED_LOG_BUF_HANDLE hLogBuf, int i32LogAttrs, const char *fmt, ...) GED_LOG_BUF_FORMAT_PRINTF(3,4);

GED_ERROR ged_log_system_init(void);

void ged_log_system_exit(void);

int ged_log_buf_write(GED_LOG_BUF_HANDLE hLogBuf, const char __user *pszBuffer, int i32Count);

void ged_log_trace_begin(char *name);

void ged_log_trace_end(void);

void ged_log_trace_counter(char *name, int count);

#endif
