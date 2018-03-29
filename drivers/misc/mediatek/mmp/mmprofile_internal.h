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

#define MMP_EVENT_STATE_ENABLED (1 << 0)
#define MMP_EVENT_STATE_FTRACE  (1 << 1)

typedef struct {
	unsigned int parentId;
	char name[MMProfileEventNameMaxLen + 1];
} MMProfile_EventInfo_t;

struct MMProfile_EventSetting_t {
	MMP_Event event;
	unsigned int enable;
	unsigned int recursive;
	unsigned int ftrace;
};

struct MMProfile_EventLog_t {
	MMP_Event event;
	MMP_LogType type;
	unsigned int data1;
	unsigned int data2;
};

typedef struct {
	unsigned int lock;
	unsigned int id;
	unsigned int timeLow;
	unsigned int timeHigh;
	unsigned int flag;
	unsigned int data1;
	unsigned int data2;
	unsigned int meta_data_cookie;
} MMProfile_Event_t;

typedef struct {
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
	unsigned int event_state[MMProfileMaxEventCount];
} MMProfile_Global_t;

typedef struct {
	unsigned int cookie;
	MMP_MetaDataType data_type;
	unsigned int data_size;
	unsigned int data_offset;
} MMProfile_MetaData_t;

typedef struct {
	unsigned int id;
	MMP_LogType type;
	MMP_MetaData_t meta_data;
} MMProfile_MetaLog_t;

#ifdef CONFIG_COMPAT
struct Compat_MMProfile_MetaLog_t {
	unsigned int id;
	MMP_LogType type;
	struct Compat_MMP_MetaData_t meta_data;
};
#endif

#define MMProfileGlobalsSize     ((sizeof(MMProfile_Global_t)+(PAGE_SIZE-1))&(~(PAGE_SIZE-1)))

#define CONFIG_MMPROFILE_PATH   "/data/MMProfileConfig.dat"

#define MMProfilePrimaryBuffer  1
#define MMProfileGlobalsBuffer  2
#define MMProfileMetaDataBuffer 3

#define MMP_IOC_MAGIC 'M'

/* Note: MMP_IOC_DUMPEVENTINFO, arg points to a buffer: sizeof(MMProfile_EventInfo_t)*MMProfileMaxEventCount */
/* Note: MMP_IOC_DUMPMETADATA, arg points to a buffer: MMProfileGlobals.meta_buffer_size */
#define MMP_IOC_ENABLE          _IOW(MMP_IOC_MAGIC, 1, unsigned int)
#define MMP_IOC_START           _IOW(MMP_IOC_MAGIC, 2, unsigned int)
#define MMP_IOC_TIME            _IOR(MMP_IOC_MAGIC, 3, unsigned long long)
#define MMP_IOC_REGEVENT        _IOWR(MMP_IOC_MAGIC, 4, MMProfile_EventInfo_t)
#define MMP_IOC_FINDEVENT       _IOWR(MMP_IOC_MAGIC, 5, MMProfile_EventInfo_t)
#define MMP_IOC_ENABLEEVENT     _IOW(MMP_IOC_MAGIC, 6, struct MMProfile_EventSetting_t)
#define MMP_IOC_LOG             _IOW(MMP_IOC_MAGIC, 7, struct MMProfile_EventLog_t)
#define MMP_IOC_DUMPEVENTINFO   _IOR(MMP_IOC_MAGIC, 8, MMProfile_EventInfo_t)
#define MMP_IOC_METADATALOG     _IOW(MMP_IOC_MAGIC, 9, MMProfile_MetaLog_t)
#define MMP_IOC_DUMPMETADATA    _IOR(MMP_IOC_MAGIC, 10, MMProfile_MetaLog_t)
#define MMP_IOC_SELECTBUFFER    _IOW(MMP_IOC_MAGIC, 11, unsigned int)
#define MMP_IOC_TRYLOG          _IOWR(MMP_IOC_MAGIC, 12, unsigned int)
#define MMP_IOC_ISENABLE        _IOR(MMP_IOC_MAGIC, 13, unsigned int)
#define MMP_IOC_REMOTESTART     _IOW(MMP_IOC_MAGIC, 14, unsigned int)
#define MMP_IOC_TEST            _IOWR(MMP_IOC_MAGIC, 100, unsigned int)

/* fix build warning: unused */
/*static void MMProfileInitBuffer(void);*/
/*static void MMProfileResetBuffer(void);*/

#ifdef __cplusplus
}
#endif
#endif
