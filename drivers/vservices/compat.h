/*
 * drivers/vservices/compat.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Wrapper functions/definitions for compatibility between differnet kernel
 * versions.
 */

#ifndef _VSERVICES_COMPAT_H
#define _VSERVICES_COMPAT_H

#include <linux/workqueue.h>
#include <linux/version.h>

/* The INIT_WORK_ONSTACK macro has a slightly different name in older kernels */
#ifndef INIT_WORK_ONSTACK
#define INIT_WORK_ONSTACK(_work, _func) INIT_WORK_ON_STACK(_work, _func)
#endif

/*
 * We require a workqueue with  no concurrency. This is provided by
 * create_singlethread_workqueue() in kernel prior to 2.6.36.
 * In later versions, create_singlethread_workqueue() enables WQ_MEM_RECLAIM and
 * thus WQ_RESCUER, which allows work items to be grabbed by a rescuer thread
 * and run concurrently if the queue is running too slowly. We must use
 * alloc_ordered_workqueue() instead, to disable the rescuer.
 */
static inline struct workqueue_struct *
vs_create_workqueue(const char *name)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	return create_singlethread_workqueue(name);
#else
	return alloc_ordered_workqueue(name, 0);
#endif
}

/*
 * The max3 macro has only been present from 2.6.37
 * (commit: f27c85c56b32c42bcc54a43189c1e00fdceb23ec)
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
#define max3(x, y, z) ({			\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	typeof(z) _max3 = (z);			\
	(void) (&_max1 == &_max2);		\
	(void) (&_max1 == &_max3);		\
	_max1 > _max2 ? (_max1 > _max3 ? _max1 : _max3) : \
		(_max2 > _max3 ? _max2 : _max3); })
#endif

#endif /* _VSERVICES_COMPAT_H */
