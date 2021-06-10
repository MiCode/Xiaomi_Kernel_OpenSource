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

#ifndef __MMPROFILE_INTERNAL_H__
#define __MMPROFILE_INTERNAL_H__

#include "mmprofile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MMProfileMaxEventCount 1000

#define MMPROFILE_MAX_EVENT_COUNT 1000

#define MMP_EVENT_STATE_ENABLED (1 << 0)
#define MMP_EVENT_STATE_FTRACE  (1 << 1)

struct mmprofile_eventinfo_t {
	unsigned int parent_id;
	char name[MMPROFILE_EVENT_NAME_MAX_LEN + 1];
};

struct mmprofile_eventsetting_t {
	mmp_event event;
	unsigned int enable;
	unsigned int recursive;
	unsigned int ftrace;
};

struct mmprofile_eventlog_t {
	mmp_event event;
	enum mmp_log_type type;
	unsigned int data1;
	unsigned int data2;
};

struct mmprofile_event_t {
	unsigned int lock;
	unsigned int id;
	unsigned int time_low;
	unsigned int time_high;
	unsigned int flag;
	unsigned int data1;
	unsigned int data2;
	unsigned int meta_data_cookie;
};

struct mmprofile_global_t {
	unsigned int enable;
	unsigned int start;
	unsigned int write_pointer;
	unsigned int reg_event_index;
	unsigned int buffer_size_record;
	unsigned int buffer_size_bytes;
	unsigned int record_size;
	unsigned int meta_buffer_size;
	unsigned int new_buffer_size_record;
	unsigned int new_meta_buffer_size;
	unsigned int selected_buffer;
	unsigned int max_event_count;
	unsigned int event_state[MMPROFILE_MAX_EVENT_COUNT];
};

struct mmprofile_metadata_t {
	unsigned int cookie;
	enum mmp_metadata_type data_type;
	unsigned int data_size;
	unsigned int data_offset;
};

struct mmprofile_metalog_t {
	unsigned int id;
	enum mmp_log_type type;
	struct mmp_metadata_t meta_data;
};

#ifdef CONFIG_COMPAT
struct compat_mmprofile_metalog_t {
	unsigned int id;
	enum mmp_log_type type;
	struct compat_mmp_metadata_t meta_data;
};
#endif

#define MMPROFILE_GLOBALS_SIZE \
	((sizeof(struct mmprofile_global_t)+(PAGE_SIZE-1))&(~(PAGE_SIZE-1)))

#define CONFIG_MMPROFILE_PATH   "/data/MMProfileConfig.dat"

#define MMPROFILE_PRIMARY_BUFFER  1
#define MMPROFILE_GLOBALS_BUFFER  2
#define MMPROFILE_DATA_BUFFER 3

#define MMP_IOC_MAGIC 'M'

/* Note: MMP_IOC_DUMPEVENTINFO, arg points to a buffer:
 * sizeof(mmprofile_eventinfo_t)*MMPROFILE_MAX_EVENT_COUNT
 * Note: MMP_IOC_DUMPMETADATA, arg points to a buffer:
 * mmprofile_globals.meta_buffer_size
 */
#define MMP_IOC_ENABLE _IOW(MMP_IOC_MAGIC, 1, unsigned int)
#define MMP_IOC_START _IOW(MMP_IOC_MAGIC, 2, unsigned int)
#define MMP_IOC_TIME _IOR(MMP_IOC_MAGIC, 3, unsigned long long)
#define MMP_IOC_REGEVENT \
	_IOWR(MMP_IOC_MAGIC, 4, struct mmprofile_eventinfo_t)
#define MMP_IOC_FINDEVENT \
	_IOWR(MMP_IOC_MAGIC, 5, struct mmprofile_eventinfo_t)
#define MMP_IOC_ENABLEEVENT \
	_IOW(MMP_IOC_MAGIC, 6, struct mmprofile_eventsetting_t)
#define MMP_IOC_LOG \
	_IOW(MMP_IOC_MAGIC, 7, struct mmprofile_eventlog_t)
#define MMP_IOC_DUMPEVENTINFO \
	_IOR(MMP_IOC_MAGIC, 8, struct mmprofile_eventinfo_t)
#define MMP_IOC_METADATALOG _IOW(MMP_IOC_MAGIC, 9, struct mmprofile_metalog_t)
#define MMP_IOC_DUMPMETADATA \
	_IOR(MMP_IOC_MAGIC, 10, struct mmprofile_metalog_t)
#define MMP_IOC_SELECTBUFFER _IOW(MMP_IOC_MAGIC, 11, unsigned int)
#define MMP_IOC_TRYLOG _IOWR(MMP_IOC_MAGIC, 12, unsigned int)
#define MMP_IOC_ISENABLE _IOR(MMP_IOC_MAGIC, 13, unsigned int)
#define MMP_IOC_REMOTESTART _IOW(MMP_IOC_MAGIC, 14, unsigned int)
#define MMP_IOC_SETRECORDCNT _IOW(MMP_IOC_MAGIC, 15, unsigned int)
#define MMP_IOC_SETMETABUFSIZE _IOW(MMP_IOC_MAGIC, 16, unsigned int)
#define MMP_IOC_TEST _IOWR(MMP_IOC_MAGIC, 100, unsigned int)

/* fix build warning: unused */
/*static void mmprofile_init_buffer(void);*/
/*static void mmprofile_reset_buffer(void);*/

#ifdef __cplusplus
}
#endif
#endif
