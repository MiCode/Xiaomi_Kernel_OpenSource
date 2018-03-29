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

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <asm/page.h>
#include <asm/io.h>

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
/* #include <asm/mach-types.h> */
#include <asm/cacheflush.h>
#include <asm/io.h>

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/debugfs.h>

#include <linux/ftrace_event.h>
#include <linux/bug.h>

#define MMPROFILE_INTERNAL
#include <mmprofile_internal.h>

#ifdef CONFIG_MTK_EXTMEM
#include <linux/exm_driver.h>
#endif

/* #pragma GCC optimize ("O0") */
#define MMP_DEVNAME "mmp"

#define MMProfileDefaultBufferSize 0x18000
#define MMProfileDefaultMetaBufferSize 0x800000

#define MMProfileDumpBlockSize (1024*4)

#define TAG_MMPROFILE "mmprofile"

#ifdef CONFIG_TRACING

#define ENABLE_MMP_TRACING
#ifdef ENABLE_MMP_TRACING
#define MMP_TRACING
#endif

#endif /* CONFIG_TRACING */

static bool mmp_log_on;
static bool mmp_trace_log_on;

#define MMP_LOG(prio, fmt, arg...) \
	do { \
		if (mmp_log_on) \
			pr_debug("MMP:%s(): "fmt"\n", __func__, ##arg); \
	} while (0)

#define MMP_MSG(fmt, arg...) pr_warn("MMP: %s(): "fmt"\n", __func__, ##arg)

typedef struct {
	MMProfile_EventInfo_t event_info;
	struct list_head list;
} MMProfile_RegTable_t;

typedef struct {
	struct list_head list;
	unsigned int block_size;
	unsigned int cookie;
	MMP_MetaDataType data_type;
	unsigned int data_size;
	unsigned char meta_data[1];
} MMProfile_MetaDataBlock_t;

static int bMMProfileInitBuffer;
static unsigned int MMProfile_MetaDataCookie = 1;
static DEFINE_MUTEX(MMProfile_BufferInitMutex);
static DEFINE_MUTEX(MMProfile_RegTableMutex);
static DEFINE_MUTEX(MMProfile_MetaBufferMutex);
static MMProfile_Event_t *pMMProfileRingBuffer;
static unsigned char *pMMProfileMetaBuffer;
static MMProfile_Global_t MMProfileGlobals
__aligned(PAGE_SIZE) = {
	.buffer_size_record = MMProfileDefaultBufferSize,
	.new_buffer_size_record = MMProfileDefaultBufferSize,
	.buffer_size_bytes = ((sizeof(MMProfile_Event_t) * MMProfileDefaultBufferSize +
		      (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1))),
	.record_size = sizeof(MMProfile_Event_t),
	.meta_buffer_size = MMProfileDefaultMetaBufferSize,
	.new_meta_buffer_size = MMProfileDefaultMetaBufferSize,
	.selected_buffer = MMProfilePrimaryBuffer,
	.reg_event_index = sizeof(MMProfileStaticEvents) / sizeof(MMP_StaticEvent_t),
	.max_event_count = MMProfileMaxEventCount,
};

static MMProfile_RegTable_t MMProfile_RegTable = {
	.list = LIST_HEAD_INIT(MMProfile_RegTable.list),
};

static struct list_head MMProfile_MetaBufferList = LIST_HEAD_INIT(MMProfile_MetaBufferList);
static unsigned char MMProfileDumpBlock[MMProfileDumpBlockSize];

/* Internal functions begin */
static int MMProfileRegisterStaticEvents(int sync);

static void MMProfileForceStart(int start);

unsigned int MMProfileGetDumpSize(void)
{
	unsigned int size;

	MMP_LOG(ANDROID_LOG_DEBUG, "+enable %u, start %u", MMProfileGlobals.enable,
		MMProfileGlobals.start);
	MMProfileForceStart(0);
	if (MMProfileRegisterStaticEvents(0) == 0)
		return 0;
	size = sizeof(MMProfile_Global_t);
	size += sizeof(MMProfile_EventInfo_t) * (MMProfileGlobals.reg_event_index + 1);
	size += MMProfileGlobals.buffer_size_bytes;
	MMP_LOG(ANDROID_LOG_DEBUG, "-size %u", size);
	return size;
}

static unsigned int MMProfileFillDumpBlock(void *pSrc, void *pDst,
					   unsigned int *pSrcPos, unsigned int *pDstPos,
					   unsigned int SrcSize, unsigned int DstSize)
{
	unsigned int SrcLeft = SrcSize - *pSrcPos;
	unsigned int DstLeft = DstSize - *pDstPos;

	if ((SrcLeft == 0) || (DstLeft == 0))
		return 0;
	if (SrcLeft < DstLeft) {
		memcpy(((unsigned char *)pDst) + *pDstPos, ((unsigned char *)pSrc) + *pSrcPos,
		       SrcLeft);
		*pSrcPos += SrcLeft;
		*pDstPos += SrcLeft;
		return SrcLeft;
	}

	memcpy(((unsigned char *)pDst) + *pDstPos, ((unsigned char *)pSrc) + *pSrcPos,
	       DstLeft);
	*pSrcPos += DstLeft;
	*pDstPos += DstLeft;
	return DstLeft;
}

void MMProfileGetDumpBuffer(unsigned int Start, unsigned long *pAddr, unsigned int *pSize)
{
	unsigned int total_pos = Start;
	unsigned int region_pos;
	unsigned int block_pos = 0;
	unsigned int region_base = 0;
	unsigned int copy_size;
	*pAddr = (unsigned long)MMProfileDumpBlock;
	*pSize = MMProfileDumpBlockSize;
	if (!bMMProfileInitBuffer) {
		MMP_LOG(ANDROID_LOG_DEBUG, "Ringbuffer is not initialized");
		*pSize = 0;
		return;
	}
	if (total_pos < (region_base + sizeof(MMProfile_Global_t))) {
		/* Global structure */
		region_pos = total_pos;
		copy_size =
		    MMProfileFillDumpBlock(&MMProfileGlobals, MMProfileDumpBlock, &region_pos,
					   &block_pos, sizeof(MMProfile_Global_t),
					   MMProfileDumpBlockSize);
		if (block_pos == MMProfileDumpBlockSize)
			return;
		total_pos = region_base + sizeof(MMProfile_Global_t);
	}
	region_base += sizeof(MMProfile_Global_t);
	if (MMProfileRegisterStaticEvents(0) == 0) {
		MMP_LOG(ANDROID_LOG_DEBUG, "static event not register");
		*pSize = 0;
		return;
	}
	if (total_pos <
	    (region_base +
	     sizeof(MMProfile_EventInfo_t) * (MMProfileGlobals.reg_event_index + 1))) {
		/* Register table */
		MMP_Event index;
		MMProfile_RegTable_t *pRegTable;
		MMProfile_EventInfo_t EventInfoDummy = { 0, "" };
		unsigned int SrcPos;
		unsigned int Pos = 0;

		region_pos = total_pos - region_base;
		if (mutex_trylock(&MMProfile_RegTableMutex) == 0) {
			MMP_LOG(ANDROID_LOG_DEBUG, "fail to get reg lock");
			*pSize = 0;
			return;
		}
		if (Pos + sizeof(MMProfile_EventInfo_t) > region_pos) {
			if (region_pos > Pos)
				SrcPos = region_pos - Pos;
			else
				SrcPos = 0;
			copy_size =
			    MMProfileFillDumpBlock(&EventInfoDummy, MMProfileDumpBlock, &SrcPos,
						   &block_pos, sizeof(MMProfile_EventInfo_t),
						   MMProfileDumpBlockSize);
			if (block_pos == MMProfileDumpBlockSize) {
				mutex_unlock(&MMProfile_RegTableMutex);
				return;
			}
		}
		Pos += sizeof(MMProfile_EventInfo_t);
		index = MMP_RootEvent;
		list_for_each_entry(pRegTable, &(MMProfile_RegTable.list), list) {
			if (Pos + sizeof(MMProfile_EventInfo_t) > region_pos) {
				if (region_pos > Pos)
					SrcPos = region_pos - Pos;
				else
					SrcPos = 0;
				copy_size =
				    MMProfileFillDumpBlock(&(pRegTable->event_info),
							   MMProfileDumpBlock, &SrcPos, &block_pos,
							   sizeof(MMProfile_EventInfo_t),
							   MMProfileDumpBlockSize);
				if (block_pos == MMProfileDumpBlockSize) {
					mutex_unlock(&MMProfile_RegTableMutex);
					return;
				}
			}
			Pos += sizeof(MMProfile_EventInfo_t);
			index++;
		}
		mutex_unlock(&MMProfile_RegTableMutex);
		total_pos =
		    region_base +
		    sizeof(MMProfile_EventInfo_t) * (MMProfileGlobals.reg_event_index + 1);
	}
	region_base += sizeof(MMProfile_EventInfo_t) * (MMProfileGlobals.reg_event_index + 1);
	if (total_pos < (region_base + MMProfileGlobals.buffer_size_bytes)) {
		/* Primary buffer */
		region_pos = total_pos - region_base;
		copy_size =
		    MMProfileFillDumpBlock(pMMProfileRingBuffer, MMProfileDumpBlock, &region_pos,
					   &block_pos, MMProfileGlobals.buffer_size_bytes,
					   MMProfileDumpBlockSize);
		if (block_pos == MMProfileDumpBlockSize)
			return;
	} else {
		*pSize = 0;
	}
	MMP_LOG(ANDROID_LOG_DEBUG, "end t=%u,r =%u,block_pos=%u", total_pos, region_base,
		block_pos);
	*pSize = block_pos;
}

static void MMProfileInitBuffer(void)
{
	if (!MMProfileGlobals.enable)
		return;
	if (in_interrupt())
		return;
	mutex_lock(&MMProfile_BufferInitMutex);
	if ((!bMMProfileInitBuffer) ||
	    (MMProfileGlobals.buffer_size_record != MMProfileGlobals.new_buffer_size_record) ||
	    (MMProfileGlobals.meta_buffer_size != MMProfileGlobals.new_meta_buffer_size)) {
		/* Initialize */
		/* Allocate memory. */
		unsigned int bResetRingBuffer = 0;
		unsigned int bResetMetaBuffer = 0;

		if (!pMMProfileRingBuffer) {
			MMProfileGlobals.buffer_size_record =
			    MMProfileGlobals.new_buffer_size_record;
			MMProfileGlobals.buffer_size_bytes =
			    ((sizeof(MMProfile_Event_t) * MMProfileGlobals.buffer_size_record +
			      (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1)));
			bResetRingBuffer = 1;
		} else if (MMProfileGlobals.buffer_size_record !=
			   MMProfileGlobals.new_buffer_size_record) {
			vfree(pMMProfileRingBuffer);
			MMProfileGlobals.buffer_size_record =
			    MMProfileGlobals.new_buffer_size_record;
			MMProfileGlobals.buffer_size_bytes =
			    ((sizeof(MMProfile_Event_t) * MMProfileGlobals.buffer_size_record +
			      (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1)));
			bResetRingBuffer = 1;
		}
		if (bResetRingBuffer) {
			pMMProfileRingBuffer =
#ifdef CONFIG_MTK_EXTMEM
			    (MMProfile_Event_t *)
			    extmem_malloc_page_align(MMProfileGlobals.buffer_size_bytes);
#else
			    vmalloc(MMProfileGlobals.buffer_size_bytes);
#endif
		}
		MMP_LOG(ANDROID_LOG_DEBUG, "pMMProfileRingBuffer=0x%08lx",
			(unsigned long)pMMProfileRingBuffer);

		if (!pMMProfileMetaBuffer) {
			MMProfileGlobals.meta_buffer_size = MMProfileGlobals.new_meta_buffer_size;
			bResetMetaBuffer = 1;
		} else if (MMProfileGlobals.meta_buffer_size !=
			   MMProfileGlobals.new_meta_buffer_size) {
			vfree(pMMProfileMetaBuffer);
			MMProfileGlobals.meta_buffer_size = MMProfileGlobals.new_meta_buffer_size;
			bResetMetaBuffer = 1;
		}
		if (bResetMetaBuffer) {
			pMMProfileMetaBuffer =
#ifdef CONFIG_MTK_EXTMEM
			    (unsigned char *)
			    extmem_malloc_page_align(MMProfileGlobals.meta_buffer_size);
#else
			    vmalloc(MMProfileGlobals.meta_buffer_size);
#endif
		}
		MMP_LOG(ANDROID_LOG_DEBUG, "pMMProfileMetaBuffer=0x%08lx",
			(unsigned long)pMMProfileMetaBuffer);

		if ((!pMMProfileRingBuffer) || (!pMMProfileMetaBuffer)) {
			if (pMMProfileRingBuffer) {
				vfree(pMMProfileRingBuffer);
				pMMProfileRingBuffer = NULL;
			}
			if (pMMProfileMetaBuffer) {
				vfree(pMMProfileMetaBuffer);
				pMMProfileMetaBuffer = NULL;
			}
			bMMProfileInitBuffer = 0;
			mutex_unlock(&MMProfile_BufferInitMutex);
			MMP_LOG(ANDROID_LOG_DEBUG, "Cannot allocate buffer");
			return;
		}

		if (bResetRingBuffer)
			memset((void *)(pMMProfileRingBuffer), 0,
			       MMProfileGlobals.buffer_size_bytes);
		if (bResetMetaBuffer) {
			memset((void *)(pMMProfileMetaBuffer), 0,
			       MMProfileGlobals.meta_buffer_size);
			/* Initialize the first block in meta buffer. */
			{
				MMProfile_MetaDataBlock_t *pBlock =
				    (MMProfile_MetaDataBlock_t *) pMMProfileMetaBuffer;
				pBlock->block_size = MMProfileGlobals.meta_buffer_size;
				INIT_LIST_HEAD(&MMProfile_MetaBufferList);
				list_add_tail(&(pBlock->list), &MMProfile_MetaBufferList);
			}
		}
		bMMProfileInitBuffer = 1;
	}
	mutex_unlock(&MMProfile_BufferInitMutex);
}

static void MMProfileResetBuffer(void)
{
	if (!MMProfileGlobals.enable)
		return;
	if (bMMProfileInitBuffer) {
		memset((void *)(pMMProfileRingBuffer), 0, MMProfileGlobals.buffer_size_bytes);
		MMProfileGlobals.write_pointer = 0;
		mutex_lock(&MMProfile_MetaBufferMutex);
		MMProfile_MetaDataCookie = 1;
		memset((void *)(pMMProfileMetaBuffer), 0, MMProfileGlobals.meta_buffer_size);
		/* Initialize the first block in meta buffer. */
		{
			MMProfile_MetaDataBlock_t *pBlock =
			    (MMProfile_MetaDataBlock_t *) pMMProfileMetaBuffer;
			pBlock->block_size = MMProfileGlobals.meta_buffer_size;
			INIT_LIST_HEAD(&MMProfile_MetaBufferList);
			list_add_tail(&(pBlock->list), &MMProfile_MetaBufferList);
		}
		mutex_unlock(&MMProfile_MetaBufferMutex);
	}
}

static void MMProfileForceStart(int start)
{
	MMP_MSG("start: %d", start);
	if (!MMProfileGlobals.enable)
		return;
	MMP_LOG(ANDROID_LOG_DEBUG, "+start %d", start);
	if (start && (!MMProfileGlobals.start)) {
		MMProfileInitBuffer();
		MMProfileResetBuffer();
	}
	MMProfileGlobals.start = start;
	MMP_LOG(ANDROID_LOG_DEBUG, "-start=%d", MMProfileGlobals.start);
}

/* this function only used by other kernel modules. */
void MMProfileStart(int start)
{
#ifndef FORBID_MMP_START
	MMProfileForceStart(start);
#endif
}

void MMProfileEnable(int enable)
{
	MMP_MSG("enable: %d", enable);
	if (enable)
		MMProfileRegisterStaticEvents(1);
	MMProfileGlobals.enable = enable;
	if (enable == 0)
		MMProfileForceStart(0);
}

/* if using remote tool (PC side) or adb shell command, can always start mmp */
static void MMProfileRemoteStart(int start)
{
	MMP_MSG("remote start: %d", start);
	if (!MMProfileGlobals.enable)
		return;
	MMP_LOG(ANDROID_LOG_DEBUG, "remote +start %d", start);
	if (start && (!MMProfileGlobals.start)) {
		MMProfileInitBuffer();
		MMProfileResetBuffer();
	}
	MMProfileGlobals.start = start;
	MMP_LOG(ANDROID_LOG_DEBUG, "remote -start=%d", MMProfileGlobals.start);
}

static MMP_Event MMProfileFindEventInt(MMP_Event parent, const char *name)
{
	MMP_Event index;
	MMProfile_RegTable_t *pRegTable;

	index = MMP_RootEvent;
	list_for_each_entry(pRegTable, &(MMProfile_RegTable.list), list) {
		if ((parent == 0) || (pRegTable->event_info.parentId == parent)) {
			if (strncmp(pRegTable->event_info.name, name, MMProfileEventNameMaxLen) ==
			    0) {
				return index;
			}
		}
		index++;
	}
	return 0;
}

static int MMProfileGetEventName(MMP_Event event, char *name, size_t *size)
{
	MMP_Event curr_event = event;	/* current event for seraching */
	MMProfile_EventInfo_t *eventInfo[32];	/* event info for all level of the event */
	int infoCnt = 0;
	int found = 0;
	int ret = -1;

	if ((NULL == name) || (NULL == size)) {
		/* parameters invalid */
		return ret;
	}

	while (1) {
		MMProfile_RegTable_t *pRegTable;
		int curr_found = 0;
		MMP_Event index = MMP_RootEvent;

		/* check the event */
		if ((MMP_InvalidEvent == curr_event)
		    || (curr_event > MMProfileGlobals.reg_event_index)) {
			/* the event invalid */
			break;
		}

		if (infoCnt >= ARRAY_SIZE(eventInfo)) {
			/* the level of event is out of limite */
			found = 1;
			break;
		}

		/* search the info for the event */
		list_for_each_entry(pRegTable, &(MMProfile_RegTable.list), list) {
			if (index == curr_event) {
				/* find this event */
				curr_found = 1;
				eventInfo[infoCnt] = &pRegTable->event_info;
				break;
			}
			index++;
		}

		if (!curr_found) {
			/* can not find the event */
			break;
		}


		if ((MMP_RootEvent == eventInfo[infoCnt]->parentId) ||
		    (MMP_InvalidEvent == eventInfo[infoCnt]->parentId)) {
			/* find all path for the event */
			found = 1;
			infoCnt++;
			break;
		}

		/* search the parent of the event */
		curr_event = eventInfo[infoCnt]->parentId;
		infoCnt++;
	}

	if (found) {
		size_t needLen = 0;
		size_t actualLen = 0;
		int infoCntUsed = 0;
		int i;

		BUG_ON(!(infoCnt > 0));

		for (i = 0; i < infoCnt; i++) {
			needLen += strlen(eventInfo[i]->name) + 1;	/* after each name has a ':' or '\0' */
			if (needLen <= *size) {
				/* buffer size is ok */
				infoCntUsed = i + 1;
			}
		}

		for (i = infoCntUsed - 1; i >= 0; i--) {
			strncpy(&name[actualLen], eventInfo[i]->name, strlen(eventInfo[i]->name) + 1);
			actualLen += strlen(eventInfo[i]->name);
			if (i > 0) {
				/* not the last name */
				name[actualLen] = ':';
			}
			actualLen++;
		}

		ret = (int)actualLen;
		*size = needLen;
	}

	return ret;
}

static int MMProfileConfigEvent(MMP_Event event, char *name, MMP_Event parent, int sync)
{
	MMP_Event index;
	MMProfile_RegTable_t *pRegTable;

	if (in_interrupt())
		return 0;
	if ((event >= MMP_MaxStaticEvent) ||
	    (event >= MMProfileMaxEventCount) || (event == MMP_InvalidEvent)) {
		return 0;
	}
	if (sync) {
		mutex_lock(&MMProfile_RegTableMutex);
	} else {
		if (mutex_trylock(&MMProfile_RegTableMutex) == 0)
			return 0;
	}
	index = MMProfileFindEventInt(parent, name);
	if (index) {
		mutex_unlock(&MMProfile_RegTableMutex);
		return 1;
	}
	pRegTable = kmalloc(sizeof(MMProfile_RegTable_t), GFP_KERNEL);
	if (!pRegTable) {
		mutex_unlock(&MMProfile_RegTableMutex);
		return 0;
	}
	strncpy(pRegTable->event_info.name, name, MMProfileEventNameMaxLen);
	pRegTable->event_info.name[MMProfileEventNameMaxLen] = 0;
	pRegTable->event_info.parentId = parent;
	list_add_tail(&(pRegTable->list), &(MMProfile_RegTable.list));

	mutex_unlock(&MMProfile_RegTableMutex);
	return 1;
}

static int MMProfileRegisterStaticEvents(int sync)
{
	static unsigned int bStaticEventRegistered;
	unsigned int static_event_count = 0;
	unsigned int i;
	int ret = 1;

	if (in_interrupt())
		return 0;
	if (bStaticEventRegistered)
		return 1;
	static_event_count = sizeof(MMProfileStaticEvents) / sizeof(MMP_StaticEvent_t);
	for (i = 0; i < static_event_count; i++) {
		ret = ret
		    && MMProfileConfigEvent(MMProfileStaticEvents[i].event,
					    MMProfileStaticEvents[i].name,
					    MMProfileStaticEvents[i].parent, sync);
	}
	bStaticEventRegistered = 1;
	return ret;
}

/* the MMP_TRACING is defined only when CONFIG_TRACING is defined and we enable mmp to trace its API. */
#ifdef MMP_TRACING
static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(0 == tracing_mark_write_addr))
		tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");
}

static inline void mmp_kernel_trace_begin(char *name)
{
	if (mmp_trace_log_on) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr, "B|%d|%s\n", current->tgid, name);
	}
}

static inline void mmp_kernel_trace_counter(char *name, int count)
{
	if (mmp_trace_log_on) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr,
			"C|%d|%s|%d\n", in_interrupt() ? -1 : current->tgid, name, count);
	}
}

static inline void mmp_kernel_trace_end(void)
{
	if (mmp_trace_log_on) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr, "E\n");
	}
}
#else
static inline void mmp_kernel_trace_begin(char *name)
{
}

static inline void mmp_kernel_trace_end(void)
{
}

static inline void mmp_kernel_trace_counter(char *name, int count)
{
}
#endif

/* continue to use 32-bit value to store time value (separate into 2) */
static void system_time(unsigned int *low, unsigned int *high)
{
	unsigned long long temp;

	temp = sched_clock();
	*low = (unsigned int)(temp & 0xffffffff);
	*high = (unsigned int)((temp >> 32) & 0xffffffff);
	/* MMP_LOG(ANDROID_LOG_VERBOSE,"system_time,0x%08x,0x%08x", *high, *low); */
}

static void MMProfileLog_Int(MMP_Event event, MMP_LogType type, unsigned long data1,
			     unsigned long data2, unsigned int meta_data_cookie)
{
	char name[256];
	size_t prefix_len;
	size_t size;

	if (!MMProfileGlobals.enable)
		return;
	if ((event >= MMProfileMaxEventCount) || (event == MMP_InvalidEvent))
		return;
	if (bMMProfileInitBuffer && MMProfileGlobals.start
	    && (MMProfileGlobals.event_state[event] & MMP_EVENT_STATE_ENABLED)) {
		MMProfile_Event_t *pEvent = NULL;
		unsigned int index;
		unsigned int lock;
		/* Event ID 0 and 1 are protected. They are not allowed for logging. */
		if (unlikely(event < 2))
			return;
		index = (atomic_inc_return((atomic_t *) &(MMProfileGlobals.write_pointer)) - 1)
		    % (MMProfileGlobals.buffer_size_record);
		lock = atomic_inc_return((atomic_t *) &(pMMProfileRingBuffer[index].lock));
		if (unlikely(lock > 1)) {
			/* Do not reduce lock count since it need to be marked as invalid. */
			/* atomic_dec(&(pMMProfile_Globol->ring_buffer[index].lock)); */
			while (1) {
				index =
				    (atomic_inc_return
				     ((atomic_t *) &(MMProfileGlobals.write_pointer)) - 1)
				    % (MMProfileGlobals.buffer_size_record);
				lock =
				    atomic_inc_return((atomic_t *) &
						      (pMMProfileRingBuffer[index].lock));
				/* Do not reduce lock count since it need to be marked as invalid. */
				if (likely(lock == 1))
					break;
			}
		}
		pEvent = (MMProfile_Event_t *) &(pMMProfileRingBuffer[index]);
		system_time(&(pEvent->timeLow), &(pEvent->timeHigh));
		pEvent->id = event;
		pEvent->flag = type;
		pEvent->data1 = (unsigned int)data1;
		pEvent->data2 = (unsigned int)data2;
		pEvent->meta_data_cookie = meta_data_cookie;
		lock = atomic_dec_return((atomic_t *) &(pEvent->lock));
		if (unlikely(lock > 0)) {
			/* Someone has marked this record as invalid. Kill this record. */
			pEvent->id = 0;
			pEvent->lock = 0;
		}

		if ((MMProfileGlobals.event_state[event] & MMP_EVENT_STATE_FTRACE)
		    || (type & MMProfileFlagSystrace)) {

			/* ignore interrupt */
			if (in_interrupt())
				return;

			memset((void *)name, 0, 256);
			name[0] = 'M';
			name[1] = 'M';
			name[2] = 'P';
			name[3] = ':';
			prefix_len = strlen(name);
			size = sizeof(name) - prefix_len;

			if (MMProfileGetEventName(event, &name[prefix_len], &size) > 0) {
				if (type & MMProfileFlagStart) {
					mmp_kernel_trace_begin(name);
				} else if (type & MMProfileFlagEnd) {
					mmp_kernel_trace_end();
				} else if (type & MMProfileFlagPulse) {
					mmp_kernel_trace_counter(name, 1);
					mmp_kernel_trace_counter(name, 0);
				}
			}
		}
	}
}

static long MMProfileLogMetaInt(MMP_Event event, MMP_LogType type, MMP_MetaData_t *pMetaData,
				long bFromUser)
{
	unsigned long retn;
	void __user *pData;

	if (!MMProfileGlobals.enable)
		return 0;
	if ((event >= MMProfileMaxEventCount) || (event == MMP_InvalidEvent))
		return -3;
	if (bMMProfileInitBuffer && MMProfileGlobals.start
	    && (MMProfileGlobals.event_state[event] & MMP_EVENT_STATE_ENABLED)) {
		MMProfile_MetaDataBlock_t *pNode = NULL;
		unsigned long block_size;

		if (unlikely(!pMetaData))
			return -1;
		block_size =
		    ((offsetof(MMProfile_MetaDataBlock_t, meta_data) + pMetaData->size) + 3) & (~3);
		if (block_size > MMProfileGlobals.meta_buffer_size)
			return -2;
		mutex_lock(&MMProfile_MetaBufferMutex);
		pNode = list_entry(MMProfile_MetaBufferList.prev, MMProfile_MetaDataBlock_t, list);
		/* If the tail block has been used, move the first block to tail and use it for new meta data. */
		if (pNode->data_size > 0) {
			list_move_tail(MMProfile_MetaBufferList.next, &MMProfile_MetaBufferList);
			pNode =
			    list_entry(MMProfile_MetaBufferList.prev, MMProfile_MetaDataBlock_t,
				       list);
		}
		/* Migrate a block with enough size. The room is collected by sacrificing least recent used blocks. */
		while (pNode->block_size < block_size) {
			MMProfile_MetaDataBlock_t *pNextNode =
			    list_entry(pNode->list.next, MMProfile_MetaDataBlock_t, list);
			if (&(pNextNode->list) == &MMProfile_MetaBufferList)
				pNextNode =
				    list_entry(pNextNode->list.next, MMProfile_MetaDataBlock_t,
					       list);

			list_del(&(pNextNode->list));
			pNode->block_size += pNextNode->block_size;
		}
		/* Split the block if left memory is enough for a new block. */
		if (((unsigned long)pNode + block_size) <
		    ((unsigned long)pMMProfileMetaBuffer + MMProfileGlobals.meta_buffer_size)
		    && ((unsigned long)pNode + block_size) >
		    ((unsigned long)pMMProfileMetaBuffer + MMProfileGlobals.meta_buffer_size -
		     offsetof(MMProfile_MetaDataBlock_t, meta_data))) {
			block_size =
			    (unsigned long)pMMProfileMetaBuffer +
			    MMProfileGlobals.meta_buffer_size - (unsigned long)pNode;
		}
		if ((pNode->block_size - block_size) >=
		    offsetof(MMProfile_MetaDataBlock_t, meta_data)) {
			MMProfile_MetaDataBlock_t *pNewNode =
			    (MMProfile_MetaDataBlock_t *) ((unsigned long)pNode + block_size);
			if ((unsigned long)pNewNode >=
			    ((unsigned long)pMMProfileMetaBuffer +
			     MMProfileGlobals.meta_buffer_size))
				pNewNode =
				    (MMProfile_MetaDataBlock_t *) ((unsigned long)pNewNode -
								   MMProfileGlobals.
								   meta_buffer_size);
			pNewNode->block_size = pNode->block_size - block_size;
			pNewNode->data_size = 0;
			list_add(&(pNewNode->list), &(pNode->list));
			pNode->block_size = block_size;
		}
		/* Fill data */
		pNode->data_size = pMetaData->size;
		pNode->data_type = pMetaData->data_type;
		pNode->cookie = MMProfile_MetaDataCookie;
		MMProfileLog_Int(event, type, pMetaData->data1, pMetaData->data2,
				 MMProfile_MetaDataCookie);
		MMProfile_MetaDataCookie++;
		if (MMProfile_MetaDataCookie == 0)
			MMProfile_MetaDataCookie++;
		pData = (void __user *)(pMetaData->pData);
		if (((unsigned long)(pNode->meta_data) + pMetaData->size) >
		    ((unsigned long)pMMProfileMetaBuffer + MMProfileGlobals.meta_buffer_size)) {
			unsigned long left_size =
			    (unsigned long)pMMProfileMetaBuffer +
			    MMProfileGlobals.meta_buffer_size - (unsigned long)(pNode->meta_data);
			if (bFromUser) {
				retn =
				    copy_from_user(pNode->meta_data, pData, left_size);
				retn =
				    copy_from_user(pMMProfileMetaBuffer,
						   (void *)((unsigned long)pData +
							    left_size),
						   pMetaData->size - left_size);
			} else {
				memcpy(pNode->meta_data, pData, left_size);
				memcpy(pMMProfileMetaBuffer,
				       (void *)((unsigned long)pData + left_size),
				       pMetaData->size - left_size);
			}
		} else {
			if (bFromUser)
				retn =
				    copy_from_user(pNode->meta_data, pData,
						   pMetaData->size);
			else
				memcpy(pNode->meta_data, pData, pMetaData->size);
		}
		mutex_unlock(&MMProfile_MetaBufferMutex);
	}
	return 0;
}

/* Internal functions end */

/* Exposed APIs begin */
MMP_Event MMProfileRegisterEvent(MMP_Event parent, const char *name)
{
	MMP_Event index;
	MMProfile_RegTable_t *pRegTable;

	if (!MMProfileGlobals.enable)
		return 0;
	if (in_interrupt())
		return 0;
	mutex_lock(&MMProfile_RegTableMutex);
	/* index = atomic_inc_return((atomic_t*)&(MMProfileGlobals.reg_event_index)); */
	if (MMProfileGlobals.reg_event_index >= (MMProfileMaxEventCount - 1)) {
		mutex_unlock(&MMProfile_RegTableMutex);
		return 0;
	}
	/* Check if this event has already been registered. */
	index = MMProfileFindEventInt(parent, name);
	if (index) {
		mutex_unlock(&MMProfile_RegTableMutex);
		return index;
	}
	/* Check if the parent exists. */
	if ((parent == 0) || (parent > MMProfileGlobals.reg_event_index)) {
		mutex_unlock(&MMProfile_RegTableMutex);
		return 0;
	}
	/* Now register the new event. */
	pRegTable = kmalloc(sizeof(MMProfile_RegTable_t), GFP_KERNEL);
	if (!pRegTable) {
		mutex_unlock(&MMProfile_RegTableMutex);
		return 0;
	}
	index = ++(MMProfileGlobals.reg_event_index);
	if (strlen(name) > MMProfileEventNameMaxLen) {
		memcpy(pRegTable->event_info.name, name, MMProfileEventNameMaxLen);
		pRegTable->event_info.name[MMProfileEventNameMaxLen] = 0;
	} else
		strncpy(pRegTable->event_info.name, name, strlen(name) + 1);
	pRegTable->event_info.parentId = parent;
	list_add_tail(&(pRegTable->list), &(MMProfile_RegTable.list));
	MMProfileGlobals.event_state[index] = 0;
	mutex_unlock(&MMProfile_RegTableMutex);
	return index;
}
EXPORT_SYMBOL(MMProfileRegisterEvent);

MMP_Event MMProfileFindEvent(MMP_Event parent, const char *name)
{
	MMP_Event event;

	if (!MMProfileGlobals.enable)
		return 0;
	if (in_interrupt())
		return 0;
	mutex_lock(&MMProfile_RegTableMutex);
	event = MMProfileFindEventInt(parent, name);
	mutex_unlock(&MMProfile_RegTableMutex);
	return event;
}
EXPORT_SYMBOL(MMProfileFindEvent);

void MMProfileEnableFTraceEvent(MMP_Event event, long enable, long ftrace)
{
	unsigned int state;

	if (!MMProfileGlobals.enable)
		return;
	if ((event < 2) || (event >= MMProfileMaxEventCount))
		return;
	state = enable ? MMP_EVENT_STATE_ENABLED : 0;
	if (enable && ftrace)
		state |= MMP_EVENT_STATE_FTRACE;
	MMProfileGlobals.event_state[event] = state;
}
EXPORT_SYMBOL(MMProfileEnableFTraceEvent);

void MMProfileEnableEvent(MMP_Event event, long enable)
{
	MMProfileEnableFTraceEvent(event, enable, 0);
}
EXPORT_SYMBOL(MMProfileEnableEvent);

void MMProfileEnableFTraceEventRecursive(MMP_Event event, long enable, long ftrace)
{
	MMP_Event index;
	MMProfile_RegTable_t *pRegTable;

	index = MMP_RootEvent;
	MMProfileEnableFTraceEvent(event, enable, ftrace);
	list_for_each_entry(pRegTable, &(MMProfile_RegTable.list), list) {
		if (pRegTable->event_info.parentId == event)
			MMProfileEnableFTraceEventRecursive(index, enable, ftrace);

		index++;
	}
}
EXPORT_SYMBOL(MMProfileEnableFTraceEventRecursive);

void MMProfileEnableEventRecursive(MMP_Event event, long enable)
{
	MMP_Event index;
	MMProfile_RegTable_t *pRegTable;

	index = MMP_RootEvent;
	MMProfileEnableEvent(event, enable);
	list_for_each_entry(pRegTable, &(MMProfile_RegTable.list), list) {
		if (pRegTable->event_info.parentId == event)
			MMProfileEnableEventRecursive(index, enable);

		index++;
	}
}
EXPORT_SYMBOL(MMProfileEnableEventRecursive);

long MMProfileQueryEnable(MMP_Event event)
{
	if (!MMProfileGlobals.enable)
		return 0;
	if (event >= MMProfileMaxEventCount)
		return 0;
	if (event == MMP_InvalidEvent)
		return MMProfileGlobals.enable;
	return !!(MMProfileGlobals.event_state[event] & MMP_EVENT_STATE_ENABLED);
}
EXPORT_SYMBOL(MMProfileQueryEnable);

void MMProfileLogEx(MMP_Event event, MMP_LogType type, unsigned long data1, unsigned long data2)
{
	MMProfileLog_Int(event, type, data1, data2, 0);
}
EXPORT_SYMBOL(MMProfileLogEx);

void MMProfileLog(MMP_Event event, MMP_LogType type)
{
	MMProfileLogEx(event, type, 0, 0);
}
EXPORT_SYMBOL(MMProfileLog);

long MMProfileLogMeta(MMP_Event event, MMP_LogType type, MMP_MetaData_t *pMetaData)
{
	if (!MMProfileGlobals.enable)
		return 0;
	if (in_interrupt())
		return 0;
	return MMProfileLogMetaInt(event, type, pMetaData, 0);
}
EXPORT_SYMBOL(MMProfileLogMeta);

long MMProfileLogMetaStructure(MMP_Event event, MMP_LogType type,
			       MMP_MetaDataStructure_t *pMetaData)
{
	int ret = 0;

	if (!MMProfileGlobals.enable)
		return 0;
	if (event >= MMProfileMaxEventCount)
		return -3;
	if (in_interrupt())
		return 0;
	if (event == MMP_InvalidEvent)
		return 0;
	if (bMMProfileInitBuffer && MMProfileGlobals.start
	    && (MMProfileGlobals.event_state[event] & MMP_EVENT_STATE_ENABLED)) {
		MMP_MetaData_t MetaData;

		MetaData.data1 = pMetaData->data1;
		MetaData.data2 = pMetaData->data2;
		MetaData.data_type = MMProfileMetaStructure;
		MetaData.size = 32 + pMetaData->struct_size;
		MetaData.pData = vmalloc(MetaData.size);
		if (!MetaData.pData)
			return -1;
		memcpy(MetaData.pData, pMetaData->struct_name, 32);
		memcpy((void *)((unsigned long)(MetaData.pData) + 32), pMetaData->pData,
		       pMetaData->struct_size);
		ret = MMProfileLogMeta(event, type, &MetaData);
		vfree(MetaData.pData);
	}
	return ret;
}
EXPORT_SYMBOL(MMProfileLogMetaStructure);

long MMProfileLogMetaStringEx(MMP_Event event, MMP_LogType type, unsigned long data1,
			      unsigned long data2, const char *str)
{
	long ret = 0;

	if (!MMProfileGlobals.enable)
		return 0;
	if (event >= MMProfileMaxEventCount)
		return -3;
	if (in_interrupt())
		return 0;
	if (event == MMP_InvalidEvent)
		return 0;
	if (bMMProfileInitBuffer && MMProfileGlobals.start
	    && (MMProfileGlobals.event_state[event] & MMP_EVENT_STATE_ENABLED)) {
		MMP_MetaData_t MetaData;

		MetaData.data1 = data1;
		MetaData.data2 = data2;
		MetaData.data_type = MMProfileMetaStringMBS;
		MetaData.size = strlen(str) + 1;
		MetaData.pData = vmalloc(MetaData.size);
		if (!MetaData.pData)
			return -1;
		strncpy((char *)MetaData.pData, str, strlen(str) + 1);
		ret = MMProfileLogMeta(event, type, &MetaData);
		vfree(MetaData.pData);
	}
	return ret;
}
EXPORT_SYMBOL(MMProfileLogMetaStringEx);

long MMProfileLogMetaString(MMP_Event event, MMP_LogType type, const char *str)
{
	return MMProfileLogMetaStringEx(event, type, 0, 0, str);
}
EXPORT_SYMBOL(MMProfileLogMetaString);

long MMProfileLogMetaBitmap(MMP_Event event, MMP_LogType type, MMP_MetaDataBitmap_t *pMetaData)
{
	int ret = 0;

	if (!MMProfileGlobals.enable)
		return 0;
	if (event >= MMProfileMaxEventCount)
		return -3;
	if (in_interrupt())
		return 0;
	if (event == MMP_InvalidEvent)
		return 0;
	if (bMMProfileInitBuffer && MMProfileGlobals.start
	    && (MMProfileGlobals.event_state[event] & MMP_EVENT_STATE_ENABLED)) {
		MMP_MetaData_t MetaData;
		char *pSrc, *pDst;
		long pitch;

		MetaData.data1 = pMetaData->data1;
		MetaData.data2 = pMetaData->data2;
		MetaData.data_type = MMProfileMetaBitmap;
		MetaData.size = sizeof(MMP_MetaDataBitmap_t) + pMetaData->data_size;
		MetaData.pData = vmalloc(MetaData.size);
		if (!MetaData.pData)
			return -1;
		pSrc = (char *)pMetaData->pData + pMetaData->start_pos;
		pDst = (char *)((unsigned long)(MetaData.pData) + sizeof(MMP_MetaDataBitmap_t));
		pitch = pMetaData->pitch;
		memcpy(MetaData.pData, pMetaData, sizeof(MMP_MetaDataBitmap_t));
		if (pitch < 0)
			((MMP_MetaDataBitmap_t *) (MetaData.pData))->pitch = -pitch;
		if ((pitch > 0) && (pMetaData->down_sample_x == 1)
		    && (pMetaData->down_sample_y == 1))
			memcpy(pDst, pSrc, pMetaData->data_size);
		else {
			unsigned int x, y, x0, y0;
			unsigned int new_width, new_height;
			unsigned int Bpp = pMetaData->bpp / 8;

			new_width = (pMetaData->width - 1) / pMetaData->down_sample_x + 1;
			new_height = (pMetaData->height - 1) / pMetaData->down_sample_y + 1;
			MMP_LOG(ANDROID_LOG_DEBUG, "n(%u,%u),o(%u, %u,%d,%u) ", new_width,
				new_height, pMetaData->width, pMetaData->height, pMetaData->pitch,
				pMetaData->bpp);
			for (y = 0, y0 = 0; y < pMetaData->height;
			     y0++, y += pMetaData->down_sample_y) {
				if (pMetaData->down_sample_x == 1)
					memcpy(pDst + new_width * Bpp * y0,
					       pSrc + pMetaData->pitch * y, pMetaData->width * Bpp);
				else {
					for (x = 0, x0 = 0; x < pMetaData->width;
					     x0++, x += pMetaData->down_sample_x) {
						memcpy(pDst + (new_width * y0 + x0) * Bpp,
						       pSrc + pMetaData->pitch * y + x * Bpp, Bpp);
					}
				}
			}
			MetaData.size = sizeof(MMP_MetaDataBitmap_t) + new_width * Bpp * new_height;
		}
		ret = MMProfileLogMeta(event, type, &MetaData);
		vfree(MetaData.pData);
	}
	return ret;
}
EXPORT_SYMBOL(MMProfileLogMetaBitmap);

/* Exposed APIs end */

/* Debug FS begin */
static struct dentry *g_pDebugFSDir;
static struct dentry *g_pDebugFSStart;
static struct dentry *g_pDebugFSBuffer;
static struct dentry *g_pDebugFSGlobal;
static struct dentry *g_pDebugFSReset;
static struct dentry *g_pDebugFSEnable;
static struct dentry *g_pDebugFSMMP;

static ssize_t mmprofile_dbgfs_reset_write(struct file *file, const char __user *buf, size_t size,
					   loff_t *ppos)
{
	MMProfileResetBuffer();
	return 1;
}

static ssize_t mmprofile_dbgfs_start_read(struct file *file, char __user *buf, size_t size,
					  loff_t *ppos)
{
	char str[32];
	int r;

	MMP_LOG(ANDROID_LOG_DEBUG, "start=%d", MMProfileGlobals.start);
	r = sprintf(str, "start = %d\n", MMProfileGlobals.start);
	return simple_read_from_buffer(buf, size, ppos, str, r);
}

static ssize_t mmprofile_dbgfs_start_write(struct file *file, const char __user *buf, size_t size,
					   loff_t *ppos)
{
	unsigned int str;
	int start;
	ssize_t ret;

	ret = simple_write_to_buffer(&str, 4, ppos, buf, size);
	if ((str & 0xFF) == 0x30)
		start = 0;
	else
		start = 1;
	MMP_LOG(ANDROID_LOG_DEBUG, "start=%d", start);
	MMProfileForceStart(start);
	return ret;
}

static ssize_t mmprofile_dbgfs_enable_read(struct file *file, char __user *buf, size_t size,
					   loff_t *ppos)
{
	char str[32];
	int r;

	MMP_LOG(ANDROID_LOG_DEBUG, "enable=%d", MMProfileGlobals.enable);
	r = sprintf(str, "enable = %d\n", MMProfileGlobals.enable);
	return simple_read_from_buffer(buf, size, ppos, str, r);
}

static ssize_t mmprofile_dbgfs_enable_write(struct file *file, const char __user *buf, size_t size,
					    loff_t *ppos)
{
	unsigned int str;
	int enable;
	ssize_t ret;

	ret = simple_write_to_buffer(&str, 4, ppos, buf, size);
	if ((str & 0xFF) == 0x30)
		enable = 0;
	else
		enable = 1;
	MMP_LOG(ANDROID_LOG_DEBUG, "enable=%d", enable);
	MMProfileEnable(enable);
	return ret;
}

static ssize_t mmprofile_dbgfs_buffer_read(struct file *file, char __user *buf, size_t size,
					   loff_t *ppos)
{
	static unsigned int backup_state;
	unsigned int copy_size = 0;
	unsigned int total_copy = 0;
	unsigned long Addr;

	if (!bMMProfileInitBuffer)
		return -EFAULT;
	MMP_LOG(ANDROID_LOG_VERBOSE, "size=%ld ppos=%d", (unsigned long)size, (int)(*ppos));
	if (*ppos == 0) {
		backup_state = MMProfileGlobals.start;
		MMProfileForceStart(0);
	}
	while (size > 0) {
		MMProfileGetDumpBuffer(*ppos, &Addr, &copy_size);
		if (copy_size == 0) {
			if (backup_state)
				MMProfileForceStart(1);
			break;
		}
		if (size >= copy_size) {
			size -= copy_size;
		} else {
			copy_size = size;
			size = 0;
		}
		if (copy_to_user(buf + total_copy, (void *)Addr, copy_size)) {
			MMP_LOG(ANDROID_LOG_DEBUG, "fail to copytouser total_copy=%d", total_copy);
			break;
		}
		*ppos += copy_size;
		total_copy += copy_size;
	}
	return total_copy;
	/* return simple_read_from_buffer(buf, size, ppos, pMMProfileRingBuffer, MMProfileGlobals.buffer_size_bytes); */
}

static ssize_t mmprofile_dbgfs_global_read(struct file *file, char __user *buf, size_t size,
					   loff_t *ppos)
{
	return simple_read_from_buffer(buf, size, ppos, &MMProfileGlobals, MMProfileGlobalsSize);
}

static ssize_t mmprofile_dbgfs_global_write(struct file *file, const char __user *buf, size_t size,
					    loff_t *ppos)
{
	return simple_write_to_buffer(&MMProfileGlobals, MMProfileGlobalsSize, ppos, buf, size);
}

static const struct file_operations mmprofile_dbgfs_enable_fops = {
	.read = mmprofile_dbgfs_enable_read,
	.write = mmprofile_dbgfs_enable_write,
	.llseek = generic_file_llseek,
};

static const struct file_operations mmprofile_dbgfs_start_fops = {
	.read = mmprofile_dbgfs_start_read,
	.write = mmprofile_dbgfs_start_write,
	.llseek = generic_file_llseek,
};

static const struct file_operations mmprofile_dbgfs_reset_fops = {
	.write = mmprofile_dbgfs_reset_write,
	.llseek = generic_file_llseek,
};

static const struct file_operations mmprofile_dbgfs_buffer_fops = {
	.read = mmprofile_dbgfs_buffer_read,
	.llseek = generic_file_llseek,
};

static const struct file_operations mmprofile_dbgfs_global_fops = {
	.read = mmprofile_dbgfs_global_read,
	.write = mmprofile_dbgfs_global_write,
	.llseek = generic_file_llseek,
};


/* Debug FS end */

static char cmd_buf[128];
static void process_dbg_cmd(char *cmd)
{
	if (0 == strncmp(cmd, "mmp_log_on:", 11)) {
		char *p = (char *)cmd + 11;
		unsigned long value;

		if (0 == kstrtoul(p, 10, &value) && 0 != value)
			mmp_log_on = 1;
		else
			mmp_log_on = 0;
		MMP_MSG("mmp_log_on=%d\n", mmp_log_on);
	} else if (0 == strncmp(cmd, "mmp_trace_log_on:", 17)) {
		char *p = (char *)cmd + 17;
		unsigned long value;

		if (0 == kstrtoul(p, 10, &value) && 0 != value)
			mmp_trace_log_on = 1;
		else
			mmp_trace_log_on = 0;
		MMP_MSG("mmp_trace_log_on=%d\n", mmp_trace_log_on);
	} else {
		MMP_MSG("invalid mmp debug command: %s\n", NULL != cmd ? cmd : "(empty)");
	}
}

/* Driver specific begin */
/*
static dev_t mmprofile_devno;
static struct cdev *mmprofile_cdev;
static struct class *mmprofile_class = NULL;
*/
static int mmprofile_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mmprofile_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mmprofile_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t mmprofile_write(struct file *file, const char __user *data, size_t len,
			       loff_t *ppos)
{
	ssize_t ret;
	size_t length = len;

	if (length > 127)
		length = 127;
	ret = length;

	if (copy_from_user(&cmd_buf, data, length))
		return -EFAULT;

	cmd_buf[length] = 0;
	process_dbg_cmd(cmd_buf);
	return ret;
}

static long mmprofile_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	unsigned long retn = 0;

	switch (cmd) {
	case MMP_IOC_ENABLE:
		if ((arg == 0) || (arg == 1))
			MMProfileEnable((int)arg);
		else
			ret = -EINVAL;
		break;
	case MMP_IOC_REMOTESTART:	/* if using remote tool (PC side) or adb shell command, can always start mmp */
		if ((arg == 0) || (arg == 1))
			MMProfileRemoteStart((int)arg);
		else
			ret = -EINVAL;
		break;
	case MMP_IOC_START:
		if ((arg == 0) || (arg == 1))
			MMProfileForceStart((int)arg);
		else
			ret = -EINVAL;
		break;
	case MMP_IOC_TIME:
		{
			unsigned int time_low;
			unsigned int time_high;
			unsigned long long time;
			unsigned long long *pTimeUser = (unsigned long long __user *)arg;

			system_time(&time_low, &time_high);
			time = time_low + ((unsigned long long)time_high << 32);
			put_user(time, pTimeUser);
		}
		break;
	case MMP_IOC_REGEVENT:
		{
			MMProfile_EventInfo_t event_info;
			MMProfile_EventInfo_t __user *pEventInfoUser = (MMProfile_EventInfo_t __user *)arg;

			retn =
			    copy_from_user(&event_info, pEventInfoUser, sizeof(MMProfile_EventInfo_t));
			event_info.name[MMProfileEventNameMaxLen] = 0;
			event_info.parentId =
			    MMProfileRegisterEvent(event_info.parentId, event_info.name);
			retn =
			    copy_to_user(pEventInfoUser, &event_info, sizeof(MMProfile_EventInfo_t));
		}
		break;
	case MMP_IOC_FINDEVENT:
		{
			MMProfile_EventInfo_t event_info;
			MMProfile_EventInfo_t __user *pEventInfoUser = (MMProfile_EventInfo_t __user *)arg;

			retn =
			    copy_from_user(&event_info, pEventInfoUser, sizeof(MMProfile_EventInfo_t));
			event_info.name[MMProfileEventNameMaxLen] = 0;
			mutex_lock(&MMProfile_RegTableMutex);
			event_info.parentId =
			    MMProfileFindEventInt(event_info.parentId, event_info.name);
			mutex_unlock(&MMProfile_RegTableMutex);
			retn =
			    copy_to_user(pEventInfoUser, &event_info, sizeof(MMProfile_EventInfo_t));
		}
		break;
	case MMP_IOC_ENABLEEVENT:
		{
			MMP_Event event;
			unsigned int enable;
			unsigned int recursive;
			unsigned int ftrace;
			struct MMProfile_EventSetting_t __user *pEventSettingUser =
				(struct MMProfile_EventSetting_t __user *)arg;

			get_user(event, &pEventSettingUser->event);
			get_user(enable, &pEventSettingUser->enable);
			get_user(recursive, &pEventSettingUser->recursive);
			get_user(ftrace, &pEventSettingUser->ftrace);
			if (recursive) {
				mutex_lock(&MMProfile_RegTableMutex);
				MMProfileEnableFTraceEventRecursive(event, enable, ftrace);
				mutex_unlock(&MMProfile_RegTableMutex);
			} else
				MMProfileEnableFTraceEvent(event, enable, ftrace);
		}
		break;
	case MMP_IOC_LOG:
		{
			MMP_Event event;
			MMP_LogType type;
			unsigned int data1;
			unsigned int data2;
			struct MMProfile_EventLog_t __user *pEventLogUser =
				(struct MMProfile_EventLog_t __user *)arg;

			get_user(event, &pEventLogUser->event);
			get_user(type, &pEventLogUser->type);
			get_user(data1, &pEventLogUser->data1);
			get_user(data2, &pEventLogUser->data2);
			MMProfileLogEx(event, type, data1, data2);
		}
		break;
	case MMP_IOC_DUMPEVENTINFO:
		{
			MMP_Event index;
			MMProfile_RegTable_t *pRegTable;
			MMProfile_EventInfo_t __user *pEventInfoUser = (MMProfile_EventInfo_t __user *)arg;
			MMProfile_EventInfo_t EventInfoDummy = { 0, "" };

			MMProfileRegisterStaticEvents(1);
			mutex_lock(&MMProfile_RegTableMutex);
			retn =
			    copy_to_user(pEventInfoUser, &EventInfoDummy,
					 sizeof(MMProfile_EventInfo_t));
			index = MMP_RootEvent;
			list_for_each_entry(pRegTable, &(MMProfile_RegTable.list), list) {
				retn =
				    copy_to_user(&pEventInfoUser[index], &(pRegTable->event_info),
						 sizeof(MMProfile_EventInfo_t));
				index++;
			}
			for (; index < MMProfileMaxEventCount; index++) {
				retn =
				    copy_to_user(&pEventInfoUser[index], &EventInfoDummy,
						 sizeof(MMProfile_EventInfo_t));
			}
			mutex_unlock(&MMProfile_RegTableMutex);
		}
		break;
	case MMP_IOC_METADATALOG:
		{
			MMProfile_MetaLog_t MetaLog;
			MMProfile_MetaLog_t __user *pMetaLogUser = (MMProfile_MetaLog_t __user *)arg;
			MMP_MetaData_t MetaData;
			MMP_MetaData_t __user *pMetaDataUser;

			retn = copy_from_user(&MetaLog, pMetaLogUser, sizeof(MMProfile_MetaLog_t));
			pMetaDataUser = (MMP_MetaData_t __user *)&(pMetaLogUser->meta_data);
			retn = copy_from_user(&MetaData, pMetaDataUser, sizeof(MMP_MetaData_t));
			MMProfileLogMetaInt(MetaLog.id, MetaLog.type, &MetaData, 1);
		}
		break;
	case MMP_IOC_DUMPMETADATA:
		{
			unsigned int meta_data_count = 0;
			unsigned int offset = 0;
			unsigned int index;
			unsigned int buffer_size = 0;
			MMProfile_MetaDataBlock_t *pMetaDataBlock;
			MMProfile_MetaData_t __user *pMetaData = (MMProfile_MetaData_t __user *)(arg + 8);

			mutex_lock(&MMProfile_MetaBufferMutex);
			list_for_each_entry(pMetaDataBlock, &MMProfile_MetaBufferList, list) {
				if (pMetaDataBlock->data_size > 0) {
					put_user(pMetaDataBlock->cookie,
						 &(pMetaData[meta_data_count].cookie));
					put_user(pMetaDataBlock->data_size,
						 &(pMetaData[meta_data_count].data_size));
					put_user(pMetaDataBlock->data_type,
						 &(pMetaData[meta_data_count].data_type));
					buffer_size += pMetaDataBlock->data_size;
					meta_data_count++;
				}
			}
			put_user(meta_data_count, (unsigned int __user *)arg);
			/* pr_debug("[mmprofile_ioctl] meta_data_count=%d meta_data_size=%x\n",
			   meta_data_count, buffer_size); */
			offset = 8 + sizeof(MMProfile_MetaData_t) * meta_data_count;
			index = 0;
			list_for_each_entry(pMetaDataBlock, &MMProfile_MetaBufferList, list) {
				if (pMetaDataBlock->data_size > 0) {
					put_user(offset - 8, &(pMetaData[index].data_offset));
					/* pr_debug("[mmprofile_ioctl] MetaRecord: offset=%x size=%x\n",
					   offset-8, pMetaDataBlock->data_size); */
					if (((unsigned long)(pMetaDataBlock->meta_data) +
					     pMetaDataBlock->data_size) >
					    ((unsigned long)pMMProfileMetaBuffer +
					     MMProfileGlobals.meta_buffer_size)) {
						unsigned long left_size =
						    (unsigned long)pMMProfileMetaBuffer +
						    MMProfileGlobals.meta_buffer_size -
						    (unsigned long)(pMetaDataBlock->meta_data);
						retn =
						    copy_to_user((void __user *)(arg + offset),
								 pMetaDataBlock->meta_data,
								 left_size);
						retn =
						    copy_to_user((void __user *)(arg + offset + left_size),
								 pMMProfileMetaBuffer,
								 pMetaDataBlock->data_size -
								 left_size);
					} else
						retn =
						    copy_to_user((void __user *)(arg + offset),
								 pMetaDataBlock->meta_data,
								 pMetaDataBlock->data_size);
					offset = (offset + pMetaDataBlock->data_size + 3) & (~3);
					index++;
				}
			}
			put_user(offset - 8, (unsigned int __user *)(arg + 4));
			/* pr_debug("[mmprofile_ioctl] Finished: offset=%x\n", offset-8); */
			mutex_unlock(&MMProfile_MetaBufferMutex);
		}
		break;
	case MMP_IOC_SELECTBUFFER:
		MMProfileGlobals.selected_buffer = arg;
		break;
	case MMP_IOC_TRYLOG:
		if ((!MMProfileGlobals.enable) ||
		    (!bMMProfileInitBuffer) ||
		    (!MMProfileGlobals.start) ||
		    (arg >= MMProfileMaxEventCount) ||
		    (arg == MMP_InvalidEvent))
			ret = -EINVAL;
		else if (!(MMProfileGlobals.event_state[arg] & MMP_EVENT_STATE_ENABLED))
			ret = -EINVAL;
		break;
	case MMP_IOC_ISENABLE:
		{
			unsigned int isEnable;
			unsigned int __user *pUser = (unsigned int __user *)arg;
			MMP_Event event;

			get_user(event, pUser);
			isEnable = (unsigned int)MMProfileQueryEnable(event);
			put_user(isEnable, pUser);
		}
		break;
	case MMP_IOC_TEST:
		{
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
#define COMPAT_MMP_IOC_METADATALOG     _IOW(MMP_IOC_MAGIC, 9, struct Compat_MMProfile_MetaLog_t)
#define COMPAT_MMP_IOC_DUMPMETADATA    _IOR(MMP_IOC_MAGIC, 10, struct Compat_MMProfile_MetaLog_t)
static long mmprofile_ioctl_compat(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	unsigned long retn;

	switch (cmd) {
	case MMP_IOC_ENABLE:
		ret = mmprofile_ioctl(file, MMP_IOC_ENABLE, arg);
		break;
	case MMP_IOC_REMOTESTART:	/* if using remote tool (PC side) or adb shell command, can always start mmp */
		ret = mmprofile_ioctl(file, MMP_IOC_REMOTESTART, arg);
		break;
	case MMP_IOC_START:
		ret = mmprofile_ioctl(file, MMP_IOC_START, arg);
		break;
	case MMP_IOC_TIME:
		{
			unsigned int time_low;
			unsigned int time_high;
			unsigned long long time;
			unsigned long long __user *pTimeUser;

			pTimeUser = compat_ptr(arg);

			system_time(&time_low, &time_high);
			time = time_low + ((unsigned long long)time_high << 32);
			put_user(time, pTimeUser);
		}
		break;
	case MMP_IOC_REGEVENT:
		{
			MMProfile_EventInfo_t event_info;
			MMProfile_EventInfo_t __user *pEventInfoUser;

			pEventInfoUser = compat_ptr(arg);

			retn =
			    copy_from_user(&event_info, pEventInfoUser, sizeof(MMProfile_EventInfo_t));
			event_info.name[MMProfileEventNameMaxLen] = 0;
			event_info.parentId =
			    MMProfileRegisterEvent(event_info.parentId, event_info.name);
			retn =
			    copy_to_user(pEventInfoUser, &event_info, sizeof(MMProfile_EventInfo_t));
		}
		break;
	case MMP_IOC_FINDEVENT:
		{
			MMProfile_EventInfo_t event_info;
			MMProfile_EventInfo_t __user *pEventInfoUser;

			pEventInfoUser = compat_ptr(arg);

			retn =
			    copy_from_user(&event_info, pEventInfoUser, sizeof(MMProfile_EventInfo_t));
			event_info.name[MMProfileEventNameMaxLen] = 0;
			mutex_lock(&MMProfile_RegTableMutex);
			event_info.parentId =
			    MMProfileFindEventInt(event_info.parentId, event_info.name);
			mutex_unlock(&MMProfile_RegTableMutex);
			retn =
			    copy_to_user(pEventInfoUser, &event_info, sizeof(MMProfile_EventInfo_t));
		}
		break;
	case MMP_IOC_ENABLEEVENT:
		{
			MMP_Event event;
			unsigned int enable;
			unsigned int recursive;
			unsigned int ftrace;
			struct MMProfile_EventSetting_t __user *pEventSettingUser;

			pEventSettingUser = compat_ptr(arg);

			get_user(event, &pEventSettingUser->event);
			get_user(enable, &pEventSettingUser->enable);
			get_user(recursive, &pEventSettingUser->recursive);
			get_user(ftrace, &pEventSettingUser->ftrace);
			if (recursive) {
				mutex_lock(&MMProfile_RegTableMutex);
				MMProfileEnableFTraceEventRecursive(event, enable, ftrace);
				mutex_unlock(&MMProfile_RegTableMutex);
			} else
				MMProfileEnableFTraceEvent(event, enable, ftrace);
		}
		break;
	case MMP_IOC_LOG:
		{
			MMP_Event event;
			MMP_LogType type;
			unsigned int data1;
			unsigned int data2;
			struct MMProfile_EventLog_t __user *pEventLogUser;

			pEventLogUser = compat_ptr(arg);

			get_user(event, &pEventLogUser->event);
			get_user(type, &pEventLogUser->type);
			get_user(data1, &pEventLogUser->data1);
			get_user(data2, &pEventLogUser->data2);
			MMProfileLogEx(event, type, data1, data2);
		}
		break;
	case MMP_IOC_DUMPEVENTINFO:
		{
			MMP_Event index;
			MMProfile_RegTable_t *pRegTable;
			MMProfile_EventInfo_t __user *pEventInfoUser;
			MMProfile_EventInfo_t EventInfoDummy = { 0, "" };

			pEventInfoUser = compat_ptr(arg);

			MMProfileRegisterStaticEvents(1);
			mutex_lock(&MMProfile_RegTableMutex);
			retn =
			    copy_to_user(pEventInfoUser, &EventInfoDummy,
					 sizeof(MMProfile_EventInfo_t));
			index = MMP_RootEvent;
			list_for_each_entry(pRegTable, &(MMProfile_RegTable.list), list) {
				retn =
				    copy_to_user(&pEventInfoUser[index], &(pRegTable->event_info),
						 sizeof(MMProfile_EventInfo_t));
				index++;
			}
			for (; index < MMProfileMaxEventCount; index++) {
				retn =
				    copy_to_user(&pEventInfoUser[index], &EventInfoDummy,
						 sizeof(MMProfile_EventInfo_t));
			}
			mutex_unlock(&MMProfile_RegTableMutex);
		}
		break;
	case COMPAT_MMP_IOC_METADATALOG:
		{
			MMProfile_MetaLog_t MetaLog;
			struct Compat_MMProfile_MetaLog_t CompatMetaLog;
			struct Compat_MMProfile_MetaLog_t __user *pCompatMetaLogUser;

			pCompatMetaLogUser = compat_ptr(arg);

			retn = copy_from_user(&CompatMetaLog, pCompatMetaLogUser,
				sizeof(struct Compat_MMProfile_MetaLog_t));
			{
				MetaLog.id = CompatMetaLog.id;
				MetaLog.type = CompatMetaLog.type;
				MetaLog.meta_data.data1 = CompatMetaLog.meta_data.data1;
				MetaLog.meta_data.data2 = CompatMetaLog.meta_data.data2;
				MetaLog.meta_data.data_type = CompatMetaLog.meta_data.data_type;
				MetaLog.meta_data.size = CompatMetaLog.meta_data.size;
				MetaLog.meta_data.pData = compat_ptr(CompatMetaLog.meta_data.pData);
			}
			MMProfileLogMetaInt(MetaLog.id, MetaLog.type, &(MetaLog.meta_data), 1);
		}
		break;
	case COMPAT_MMP_IOC_DUMPMETADATA:
		{
			unsigned int meta_data_count = 0;
			unsigned int offset = 0;
			unsigned int index;
			unsigned int buffer_size = 0;
			MMProfile_MetaDataBlock_t *pMetaDataBlock;
			MMProfile_MetaData_t __user *pMetaData;
			unsigned int __user *pUser;

			pMetaData = compat_ptr(arg + 8);

			mutex_lock(&MMProfile_MetaBufferMutex);
			list_for_each_entry(pMetaDataBlock, &MMProfile_MetaBufferList, list) {
				if (pMetaDataBlock->data_size > 0) {
					put_user(pMetaDataBlock->cookie,
						 &(pMetaData[meta_data_count].cookie));
					put_user(pMetaDataBlock->data_size,
						 &(pMetaData[meta_data_count].data_size));
					put_user(pMetaDataBlock->data_type,
						 &(pMetaData[meta_data_count].data_type));
					buffer_size += pMetaDataBlock->data_size;
					meta_data_count++;
				}
			}
			pUser = compat_ptr(arg);
			put_user(meta_data_count, pUser);
			/* pr_debug("[mmprofile_ioctl] meta_data_count=%d meta_data_size=%x\n",
			   meta_data_count, buffer_size); */
			offset = 8 + sizeof(MMProfile_MetaData_t) * meta_data_count;
			index = 0;
			list_for_each_entry(pMetaDataBlock, &MMProfile_MetaBufferList, list) {
				if (pMetaDataBlock->data_size > 0) {
					put_user(offset - 8, &(pMetaData[index].data_offset));
					/* pr_debug("[mmprofile_ioctl] MetaRecord: offset=%x size=%x\n",
					   offset-8, pMetaDataBlock->data_size); */
					if (((unsigned long)(pMetaDataBlock->meta_data) +
					     pMetaDataBlock->data_size) >
					    ((unsigned long)pMMProfileMetaBuffer +
					     MMProfileGlobals.meta_buffer_size)) {
						unsigned long left_size =
						    (unsigned long)pMMProfileMetaBuffer +
						    MMProfileGlobals.meta_buffer_size -
						    (unsigned long)(pMetaDataBlock->meta_data);
						pUser = compat_ptr(arg + offset);
						retn =
						    copy_to_user(pUser,
								 pMetaDataBlock->meta_data,
								 left_size);
						pUser = compat_ptr(arg + offset + left_size);
						retn =
						    copy_to_user(pUser,
								 pMMProfileMetaBuffer,
								 pMetaDataBlock->data_size -
								 left_size);
					} else {
						pUser = compat_ptr(arg + offset);
						retn =
						    copy_to_user(pUser,
								 pMetaDataBlock->meta_data,
								 pMetaDataBlock->data_size);
					}
					offset = (offset + pMetaDataBlock->data_size + 3) & (~3);
					index++;
				}
			}
			pUser = compat_ptr(arg + 4);
			put_user(offset - 8, pUser);
			/* pr_debug("[mmprofile_ioctl] Finished: offset=%x\n", offset-8); */
			mutex_unlock(&MMProfile_MetaBufferMutex);
		}
		break;
	case MMP_IOC_SELECTBUFFER:
		ret = mmprofile_ioctl(file, MMP_IOC_SELECTBUFFER, arg);
		break;
	case MMP_IOC_TRYLOG:
		if ((!MMProfileGlobals.enable) ||
		    (!bMMProfileInitBuffer) ||
		    (!MMProfileGlobals.start) ||
		    (arg >= MMProfileMaxEventCount) ||
		    (!(MMProfileGlobals.event_state[arg] & MMP_EVENT_STATE_ENABLED)))
			ret = -EINVAL;
		break;
	case MMP_IOC_ISENABLE:
		{
			unsigned int isEnable;
			unsigned int __user *pUser;
			MMP_Event event;

			pUser = compat_ptr(arg);

			get_user(event, pUser);
			isEnable = (unsigned int)MMProfileQueryEnable(event);
			put_user(isEnable, pUser);
		}
		break;
	case MMP_IOC_TEST:
		{
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#endif

static int mmprofile_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned int pos = 0;
	unsigned int i = 0;

	if (MMProfileGlobals.selected_buffer == MMProfileGlobalsBuffer) {
		/* vma->vm_flags |= VM_RESERVED; */
		/* vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot); */

		pos = vma->vm_start;
		for (i = 0; i < MMProfileGlobalsSize; i += PAGE_SIZE, pos += PAGE_SIZE) {
			unsigned long pfn;
			/* pr_debug("[mmprofile_mmap] mmap pos=0x%08x va=0x%08x pa=0x%08x pfn=0x%08x\n",
			   pos, (unsigned long)(&MMProfileGlobals) + i,
			   virt_to_phys((unsigned long)(&MMProfileGlobals) + i),
			   phys_to_pfn(__virt_to_phys((unsigned long)(&MMProfileGlobals) + i))); */
			/* flush_dcache_page(virt_to_page((void*)((unsigned long)(&MMProfileGlobals) + i))); */
			pfn = __phys_to_pfn(__virt_to_phys((unsigned long)(&MMProfileGlobals) + i));
			if (remap_pfn_range
			    (vma, pos, pfn, PAGE_SIZE, vma->vm_page_prot | PAGE_SHARED))
				return -EAGAIN;
			/* pr_debug("pfn: 0x%08x\n", pfn); */
		}
	} else if (MMProfileGlobals.selected_buffer == MMProfilePrimaryBuffer) {
		MMProfileInitBuffer();

		if (!bMMProfileInitBuffer)
			return -EAGAIN;
		/* vma->vm_flags |= VM_RESERVED; */
		/* vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot); */

		pos = vma->vm_start;

		for (i = 0; i < MMProfileGlobals.buffer_size_bytes;
		     i += PAGE_SIZE, pos += PAGE_SIZE) {
			/* pr_debug("[mmprofile_mmap] mmap pos=0x%08x va=0x%08x pfn=0x%08x\n",
			   pos, (void*)((unsigned long)pMMProfileRingBuffer + i),
			   vmalloc_to_pfn((void*)((unsigned long)pMMProfileRingBuffer + i))); */
			/* flush_dcache_page(vmalloc_to_page((void*)((unsigned long)(pMMProfileRingBuffer) + i))); */
			if (remap_pfn_range
			    (vma, pos,
			     vmalloc_to_pfn((void *)((unsigned long)pMMProfileRingBuffer + i)),
			     PAGE_SIZE, vma->vm_page_prot | PAGE_SHARED))
				return -EAGAIN;
		}
	} else
		return -EINVAL;
	return 0;
}

const struct file_operations mmprofile_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mmprofile_ioctl,
	.open = mmprofile_open,
	.release = mmprofile_release,
	.read = mmprofile_read,
	.write = mmprofile_write,
	.mmap = mmprofile_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mmprofile_ioctl_compat,
#endif
};


static int mmprofile_probe(void)
{
#if 0
	struct class_device *class_dev = 0;
	int ret = alloc_chrdev_region(&mmprofile_devno, 0, 1, MMP_DEVNAME);

	mmprofile_cdev = cdev_alloc();
	mmprofile_cdev->owner = THIS_MODULE;
	mmprofile_cdev->ops = &mmprofile_fops;
	ret = cdev_add(mmprofile_cdev, mmprofile_devno, 1);
	mmprofile_class = class_create(THIS_MODULE, MMP_DEVNAME);
	class_dev =
	    (struct class_device *)device_create(mmprofile_class, NULL, mmprofile_devno, NULL,
						 MMP_DEVNAME);
#endif
	mmp_log_on = false;
	mmp_trace_log_on = false;
	/* Create debugfs */
	g_pDebugFSDir = debugfs_create_dir("mmprofile", NULL);
	if (g_pDebugFSDir) {
		/* Create debugfs files. */
		g_pDebugFSMMP =
		    debugfs_create_file("mmp", S_IFREG | S_IRUGO, g_pDebugFSDir, NULL,
					&mmprofile_fops);
		g_pDebugFSEnable =
		    debugfs_create_file("enable", S_IRUSR | S_IWUSR, g_pDebugFSDir, NULL,
					&mmprofile_dbgfs_enable_fops);
		g_pDebugFSStart =
		    debugfs_create_file("start", S_IRUSR | S_IWUSR, g_pDebugFSDir, NULL,
					&mmprofile_dbgfs_start_fops);
		g_pDebugFSBuffer =
		    debugfs_create_file("buffer", S_IRUSR, g_pDebugFSDir, NULL,
					&mmprofile_dbgfs_buffer_fops);
		g_pDebugFSGlobal =
		    debugfs_create_file("global", S_IRUSR, g_pDebugFSDir, NULL,
					&mmprofile_dbgfs_global_fops);
		g_pDebugFSReset =
		    debugfs_create_file("reset", S_IWUSR, g_pDebugFSDir, NULL,
					&mmprofile_dbgfs_reset_fops);
	}
	return 0;
}

static int mmprofile_remove(void)
{
	debugfs_remove(g_pDebugFSDir);
	debugfs_remove(g_pDebugFSEnable);
	debugfs_remove(g_pDebugFSStart);
	debugfs_remove(g_pDebugFSGlobal);
	debugfs_remove(g_pDebugFSBuffer);
	debugfs_remove(g_pDebugFSReset);
	debugfs_remove(g_pDebugFSMMP);
	return 0;
}

#if 0
static struct platform_driver mmprofile_driver = {
	.probe = mmprofile_probe,
	.remove = mmprofile_remove,
	.driver = {.name = MMP_DEVNAME}
};

static struct platform_device mmprofile_device = {
	.name = MMP_DEVNAME,
	.id = 0,
};
#endif
static int __init mmprofile_init(void)
{
#if 0
	if (platform_device_register(&mmprofile_device))
		return -ENODEV;

	if (platform_driver_register(&mmprofile_driver)) {
		platform_device_unregister(&mmprofile_device);
		return -ENODEV;
	}
#endif
	mmprofile_probe();
	return 0;
}

static void __exit mmprofile_exit(void)
{
#if 0
	device_destroy(mmprofile_class, mmprofile_devno);
	class_destroy(mmprofile_class);
	cdev_del(mmprofile_cdev);
	unregister_chrdev_region(mmprofile_devno, 1);

	platform_driver_unregister(&mmprofile_driver);
	platform_device_unregister(&mmprofile_device);
#endif
	mmprofile_remove();
}

/* Driver specific end */

module_init(mmprofile_init);
module_exit(mmprofile_exit);
MODULE_AUTHOR("Tianshu Qiu <tianshu.qiu@mediatek.com>");
MODULE_DESCRIPTION("MMProfile Driver");
MODULE_LICENSE("GPL");
