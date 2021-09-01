/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _GH_VIRTIO_BACKEND_H
#define _GH_VIRTIO_BACKEND_H

#if IS_ENABLED(CONFIG_GH_VIRTIO_BACKEND)
int gh_virtio_mmio_exit(gh_vmid_t vmid, const char *vm_name);
#else
static inline int gh_virtio_mmio_exit(gh_vmid_t vmid, const char *vm_name)
{
	return -EINVAL;
}
#endif
#endif /* _GH_VIRTIO_BACKEND_H */
