/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
#ifndef __Z180_H
#define __Z180_H

#include "kgsl_device.h"

#define DEVICE_2D_NAME "kgsl-2d"
#define DEVICE_2D0_NAME "kgsl-2d0"
#define DEVICE_2D1_NAME "kgsl-2d1"

#define Z180_PACKET_SIZE 15
#define Z180_PACKET_COUNT 8
#define Z180_RB_SIZE (Z180_PACKET_SIZE*Z180_PACKET_COUNT \
			  *sizeof(uint32_t))
#define Z180_DEVICE(device) \
		KGSL_CONTAINER_OF(device, struct z180_device, dev)

#define Z180_DEFAULT_PWRSCALE_POLICY  NULL

/* Wait a maximum of 10 seconds when trying to idle the core */
#define Z180_IDLE_TIMEOUT (10 * 1000)

struct z180_ringbuffer {
	unsigned int prevctx;
	struct kgsl_memdesc      cmdbufdesc;
};

struct z180_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	int current_timestamp;
	int timestamp;
	struct z180_ringbuffer ringbuffer;
	spinlock_t cmdwin_lock;
};

int z180_dump(struct kgsl_device *, int);

#endif /* __Z180_H */
