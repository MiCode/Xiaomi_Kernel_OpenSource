/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef MSM_MSM_EVTLOG_H_
#define MSM_MSM_EVTLOG_H_

#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/dcache.h>

/**
 * struct msm_evtlog_evt - Event log entry
 * @ktime:     Timestamp of event
 * @func:      Calling function name
 * @msg:       User provided string
 * @val1:      User provided value
 * @val2:      User provided value
 * @line:      Line number of caller
 * @pid:       Process id of logger
 */
struct msm_evtlog_evt {
	ktime_t ktime;
	const char *func;
	const char *msg;
	uint64_t val1;
	uint64_t val2;
	uint32_t line;
	uint32_t pid;
};

/**
 * struct msm_evtlog - current driver state information
 * @events:    Pointer to dynamically allocated event log buffer
 * @cnt:       Atomic number of events since clear. Can be used to calculate
 *             the current index. Note: The count does not wrap.
 *             Reset the event log by setting to zero.
 *             Used for lock-less producer synchronization.
 * @size:      Size of events array. Must be power of 2 to facilitate fast
 *             increments by using a bitmask to get rollover.
 * @dentry:    Filesystem entry of debugfs registration
 */
struct msm_evtlog {
	struct msm_evtlog_evt *events;
	atomic_t cnt;
	unsigned long size;
	struct dentry *dentry;
};

/**
 * msm_evtlog_init() - Create an event log, registered with debugfs.
 * @log:     Event log handle
 * @size:    Max # of events in buffer. Will be rounded up to power of 2.
 * @parent:  Parent directory entry for debugfs registration
 *
 * Return: error code.
 */
int msm_evtlog_init(struct msm_evtlog *log, int size, struct dentry *parent);

/**
 * msm_evtlog_destroy() - Destroy event log
 * @log:            Event log handle
 *
 * Unregisters debugfs node and frees memory.
 * Caller needs to make sure that log sampling has stopped.
 */
void msm_evtlog_destroy(struct msm_evtlog *log);

/**
 * msm_evtlog_sample() - Add entry to the event log
 * @evtlog:            Event log handle
 * @func:              Calling function name
 * @msg:               User provided string
 * @val1:              User provided value
 * @val2:              User provided value
 * @line:              Line number of caller
 */
void msm_evtlog_sample(
		struct msm_evtlog *log,
		const char *func,
		const char *msg,
		uint64_t val1,
		uint64_t val2,
		uint32_t line);

#endif /* MSM_MSM_EVTLOG_H_ */
