/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _KGSL_EVENTLOG_H
#define _KGSL_EVENTLOG_H

void kgsl_eventlog_init(void);
void kgsl_eventlog_exit(void);

void log_kgsl_fire_event(u32 id, u32 ts, u32 type, u32 age);
void log_kgsl_cmdbatch_submitted_event(u32 id, u32 ts, u32 prio, u64 flags);
void log_kgsl_cmdbatch_retired_event(u32 id, u32 ts, u32 prio, u64 flags,
		u64 start, u64 retire);
void log_kgsl_syncpoint_fence_event(u32 id, char *fence_name);
void log_kgsl_syncpoint_fence_expire_event(u32 id, char *fence_name);
void log_kgsl_timeline_fence_alloc_event(u32 id, u64 seqno);
void log_kgsl_timeline_fence_release_event(u32 id, u64 seqno);
size_t kgsl_snapshot_eventlog_buffer(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv);
#endif
