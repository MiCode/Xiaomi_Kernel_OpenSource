// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "kgsl_device.h"
#include "kgsl_eventlog.h"
#include "kgsl_util.h"

#define EVENTLOG_SIZE SZ_8K
#define MAGIC 0xabbaabba
#define LOG_FENCE_NAME_LEN 74

/*
 * This an internal event used to skip empty space at the bottom of the
 * ringbuffer
 */

#define LOG_SKIP 1
#define LOG_FIRE_EVENT 2
#define LOG_CMDBATCH_SUBMITTED_EVENT 3
#define LOG_CMDBATCH_RETIRED_EVENT 4
#define LOG_SYNCPOINT_FENCE_EVENT 5
#define LOG_SYNCPOINT_FENCE_EXPIRE_EVENT 6
#define LOG_TIMELINE_FENCE_ALLOC_EVENT 7
#define LOG_TIMELINE_FENCE_RELEASE_EVENT 8

static spinlock_t lock;
static void *kgsl_eventlog;
static int eventlog_wptr;

struct kgsl_log_header {
	u32 magic;
	int pid;
	u64 time;
	u32 eventid;
	u32 size;
};

/* Add a marker to skip the rest of the eventlog and start over fresh */
static void add_skip_header(u32 offset)
{
	struct kgsl_log_header *header = kgsl_eventlog + offset;

	header->magic = MAGIC;
	header->time = local_clock();
	header->pid = 0;
	header->eventid = LOG_SKIP;
	header->size = EVENTLOG_SIZE - sizeof(*header) - offset;
}

static void *kgsl_eventlog_alloc(u32 eventid, u32 size)
{
	struct kgsl_log_header *header;
	u32 datasize = size + sizeof(*header);
	unsigned long flags;
	void *data;

	if (!kgsl_eventlog)
		return NULL;

	spin_lock_irqsave(&lock, flags);
	if (eventlog_wptr + datasize > (EVENTLOG_SIZE - sizeof(*header))) {
		add_skip_header(eventlog_wptr);
		eventlog_wptr = datasize;
		data = kgsl_eventlog;
	} else {
		data = kgsl_eventlog + eventlog_wptr;
		eventlog_wptr += datasize;
	}
	spin_unlock_irqrestore(&lock, flags);

	header = data;

	header->magic = MAGIC;
	header->time = local_clock();
	header->pid = current->pid;
	header->eventid = eventid;
	header->size = size;

	return data + sizeof(*header);
}

void kgsl_eventlog_init(void)
{
	kgsl_eventlog = kzalloc(EVENTLOG_SIZE, GFP_KERNEL);
	eventlog_wptr = 0;

	spin_lock_init(&lock);

	kgsl_add_to_minidump("KGSL_EVENTLOG", (u64) kgsl_eventlog,
				__pa(kgsl_eventlog), EVENTLOG_SIZE);
}

void kgsl_eventlog_exit(void)
{
	kgsl_remove_from_minidump("KGSL_EVENTLOG", (u64) kgsl_eventlog,
				__pa(kgsl_eventlog), EVENTLOG_SIZE);

	kfree(kgsl_eventlog);
	kgsl_eventlog = NULL;
	eventlog_wptr = 0;
}

void log_kgsl_fire_event(u32 id, u32 ts, u32 type, u32 age)
{
	struct {
		u32 id;
		u32 ts;
		u32 type;
		u32 age;
	} *entry;

	entry = kgsl_eventlog_alloc(LOG_FIRE_EVENT, sizeof(*entry));
	if (!entry)
		return;

	entry->id = id;
	entry->ts = ts;
	entry->type = type;
	entry->age = age;
}

void log_kgsl_cmdbatch_submitted_event(u32 id, u32 ts, u32 prio, u64 flags)
{
	struct {
		u32 id;
		u32 ts;
		u32 prio;
		u64 flags;
	} *entry;

	entry = kgsl_eventlog_alloc(LOG_CMDBATCH_SUBMITTED_EVENT, sizeof(*entry));
	if (!entry)
		return;

	entry->id = id;
	entry->ts = ts;
	entry->prio = prio;
	entry->flags = flags;
}

void log_kgsl_cmdbatch_retired_event(u32 id, u32 ts, u32 prio, u64 flags,
		u64 start, u64 retire)
{
	struct {
		u32 id;
		u32 ts;
		u32 prio;
		u64 flags;
		u64 start;
		u64 retire;
	} *entry;

	entry = kgsl_eventlog_alloc(LOG_CMDBATCH_RETIRED_EVENT, sizeof(*entry));
	if (!entry)
		return;

	entry->id = id;
	entry->ts = ts;
	entry->prio = prio;
	entry->flags = flags;
	entry->start = start;
	entry->retire = retire;
}

void log_kgsl_syncpoint_fence_event(u32 id, char *fence_name)
{
	struct {
		u32 id;
		char name[LOG_FENCE_NAME_LEN];
	} *entry;

	entry = kgsl_eventlog_alloc(LOG_SYNCPOINT_FENCE_EVENT, sizeof(*entry));
	if (!entry)
		return;

	entry->id = id;
	memset(entry->name, 0, sizeof(entry->name));
	strlcpy(entry->name, fence_name, sizeof(entry->name));
}

void log_kgsl_syncpoint_fence_expire_event(u32 id, char *fence_name)
{
	struct {
		u32 id;
		char name[LOG_FENCE_NAME_LEN];
	} *entry;

	entry = kgsl_eventlog_alloc(LOG_SYNCPOINT_FENCE_EXPIRE_EVENT, sizeof(*entry));
	if (!entry)
		return;

	entry->id = id;
	memset(entry->name, 0, sizeof(entry->name));
	strlcpy(entry->name, fence_name, sizeof(entry->name));
}

void log_kgsl_timeline_fence_alloc_event(u32 id, u64 seqno)
{
	struct {
		u32 id;
		u64 seqno;
	} *entry;

	entry = kgsl_eventlog_alloc(LOG_TIMELINE_FENCE_ALLOC_EVENT, sizeof(*entry));
	if (!entry)
		return;

	entry->id = id;
	entry->seqno = seqno;
}

void log_kgsl_timeline_fence_release_event(u32 id, u64 seqno)
{
	struct {
		u32 id;
		u64 seqno;
	} *entry;

	entry = kgsl_eventlog_alloc(LOG_TIMELINE_FENCE_RELEASE_EVENT, sizeof(*entry));
	if (!entry)
		return;

	entry->id = id;
	entry->seqno = seqno;
}
