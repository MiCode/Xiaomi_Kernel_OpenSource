/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VPUBUF_MEMOPS_H
#define _VPUBUF_MEMOPS_H

#include "vpubuf-core.h"
#include <linux/mm.h>

/**
 * struct vpu_vmarea_handler - common vma refcount tracking handler
 *
 * @refcount:	pointer to refcount entry in the buffer
 * @put:	callback to function that decreases buffer refcount
 * @arg:	argument for @put callback
 */
struct vpu_vmarea_handler {
	atomic_t *refcount;
	void (*put)(void *arg);
	void *arg;
};

extern const struct vm_operations_struct vpu_common_vm_ops;

struct frame_vector *vpu_create_framevec(unsigned long start,
					 unsigned long length, bool write);
void vpu_destroy_framevec(struct frame_vector *vec);

#endif
