/*
 * Copyright (C) 2012 NVIDIA Corporation.
 *
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

#ifndef _NVSHM_QUEUE_H
#define _NVSHM_QUEUE_H

extern int nvshm_init_queue(struct nvshm_handle *handle);
extern struct nvshm_iobuf *nvshm_queue_get(struct nvshm_handle *handle);
extern int nvshm_queue_put(struct nvshm_handle *handle,
			   struct nvshm_iobuf *iob);
extern void nvshm_process_queue(struct nvshm_handle *handle);
extern void nvshm_abort_queue(struct nvshm_handle *handle);
#endif /* _NVSHM_QUEUE_H */
