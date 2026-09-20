/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MM_MEMFD_ASHMEM_SHIM_H
#define __MM_MEMFD_ASHMEM_SHIM_H

/*
 * mm/memfd-ashmem-shim.h
 *
 * Ashmem compatability for memfd
 *
 * Copyright (c) 2025, Google LLC.
 * Author: Isaac J. Manjarres <isaacmanjarres@google.com>
 *
 */

#include <linux/fs.h>

long memfd_ashmem_shim_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
long memfd_ashmem_shim_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#endif
#endif /* __MM_MEMFD_ASHMEM_SHIM_H */
