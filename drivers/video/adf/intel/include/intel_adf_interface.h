/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_ADF_INTERFACE_H_
#define INTEL_ADF_INTERFACE_H_

#include <video/adf.h>
#include <video/adf_format.h>

#include "core/intel_dc_config.h"
#include "intel_adf_device.h"
#include "intel_adf_sync.h"

struct intel_adf_interface {
	struct adf_interface base;
	struct intel_pipe *pipe;
	struct task_struct *event_handling_thread;
	struct kthread_worker event_handling_worker;
	struct kthread_work hotplug_connected_work;
	struct intel_adf_sync_timeline *vsync_timeline;
	atomic_t post_seqno;
	struct list_head active_list;
};

static inline struct intel_adf_interface *to_intel_intf(
	struct adf_interface *intf)
{
	return container_of(intf, struct intel_adf_interface, base);
}

extern int intel_adf_interface_handle_event(struct intel_adf_interface *intf);
extern struct sync_fence *intel_adf_interface_create_vsync_fence(
	struct intel_adf_interface *intf, u32 interval);
extern int intel_adf_interface_init(struct intel_adf_interface *intf,
	struct intel_adf_device *dev, struct intel_pipe *pipe, u32 intf_idx);
extern void intel_adf_interface_destroy(struct intel_adf_interface *intf);

#endif /* INTEL_ADF_INTERFACE_H_ */
