/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __HAB_GHS_H
#define __HAB_GHS_H

#include <ghs_vmm/kgipc.h>
#define GIPC_RECV_BUFF_SIZE_BYTES   (32*1024)

struct ghs_vdev {
	int be;
	void *read_data; /* buffer to receive from gipc */
	size_t read_size;
	int read_offset;
	GIPC_Endpoint endpoint;
	spinlock_t io_lock;
	char name[32];
	struct tasklet_struct task;
};
#endif /* __HAB_GHS_H */
