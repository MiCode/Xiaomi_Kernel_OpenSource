/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_TSPP_H_
#define _MSM_TSPP_H_

#include <linux/tspp.h> /* tspp_source */

struct msm_tspp_platform_data {
	int num_gpios;
	const struct msm_gpio *gpios;
	const char *tsif_pclk;
	const char *tsif_ref_clk;
};

struct tspp_data_descriptor {
	void *virt_base;   /* logical address of the actual data */
	u32 phys_base;     /* physical address of the actual data */
	u32 size;          /* size of buffer in bytes */
	int id;            /* unique identifier */
	void *user;        /* user-defined data */
};

typedef void (tspp_notifier)(int channel_id, void *user);
typedef void* (tspp_allocator)(int channel_id, u32 size,
	u32 *phys_base, void *user);
typedef void (tspp_memfree)(int channel_id, u32 size,
	void *virt_base, u32 phys_base, void *user);

/* Kernel API functions */
int tspp_open_stream(u32 dev, u32 channel_id,
			struct tspp_select_source *source);
int tspp_close_stream(u32 dev, u32 channel_id);
int tspp_open_channel(u32 dev, u32 channel_id);
int tspp_close_channel(u32 dev, u32 channel_id);
int tspp_get_ref_clk_counter(u32 dev,
	enum tspp_source source, u32 *tcr_counter);
int tspp_add_filter(u32 dev, u32 channel_id, struct tspp_filter *filter);
int tspp_remove_filter(u32 dev, u32 channel_id,	struct tspp_filter *filter);
int tspp_set_key(u32 dev, u32 channel_id, struct tspp_key *key);
int tspp_register_notification(u32 dev, u32 channel_id, tspp_notifier *notify,
	void *data, u32 timer_ms);
int tspp_unregister_notification(u32 dev, u32 channel_id);
const struct tspp_data_descriptor *tspp_get_buffer(u32 dev, u32 channel_id);
int tspp_release_buffer(u32 dev, u32 channel_id, u32 descriptor_id);
int tspp_allocate_buffers(u32 dev, u32 channel_id, u32 count,
	u32 size, u32 int_freq, tspp_allocator *alloc,
	tspp_memfree *memfree, void *user);

#endif /* _MSM_TSPP_H_ */

