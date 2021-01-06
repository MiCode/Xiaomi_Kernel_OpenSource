/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_VIRTIO_BACKEND_H
#define __HH_VIRTIO_BACKEND_H

#if IS_ENABLED(CONFIG_HH_VIRTIO_BACKEND)
int hh_virtio_mmio_init(const char *vm_name, hh_label_t label,
		hh_capid_t cap_id, int linux_irq, u64 base, u64 size);

#else
static inline int hh_virtio_mmio_init(const char *vm_name, hh_label_t label,
		hh_capid_t cap_id, int linux_irq, u64 base, u64 size)
{
	return -ENODEV;
}
#endif

#endif
