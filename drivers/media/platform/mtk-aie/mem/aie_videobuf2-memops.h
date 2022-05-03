/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _AIE_MEDIA_VIDEOBUF2_MEMOPS_H
#define _AIE_MEDIA_VIDEOBUF2_MEMOPS_H

#include <media/videobuf2-v4l2.h>
#include <linux/mm.h>
#include <linux/refcount.h>

/**
 * struct vb2_vmarea_handler - common vma refcount tracking handler.
 *
 * @refcount:	pointer to &refcount_t entry in the buffer.
 * @put:	callback to function that decreases buffer refcount.
 * @arg:	argument for @put callback.
 */
struct vb2_vmarea_handler {
	refcount_t		*refcount;
	void			(*put)(void *arg);
	void			*arg;
};

extern const struct vm_operations_struct aie_vb2_common_vm_ops;

struct frame_vector *aie_vb2_create_framevec(unsigned long start,
					 unsigned long length);
void aie_vb2_destroy_framevec(struct frame_vector *vec);

#endif
