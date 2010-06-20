/*
 * include/linux/ashmem.h
 *
 * Copyright 2008 Google Inc.
 * Author: Robert Love
 *
 * This file is dual licensed.  It may be redistributed and/or modified
 * under the terms of the Apache 2.0 License OR version 2 of the GNU
 * General Public License.
 */

#ifndef _LINUX_ASHMEM_H
#define _LINUX_ASHMEM_H

#include <uapi/linux/ashmem.h>

int get_ashmem_file(int fd, struct file **filp, struct file **vm_file,
			unsigned long *len);
void put_ashmem_file(struct file *file);

#endif	/* _LINUX_ASHMEM_H */
