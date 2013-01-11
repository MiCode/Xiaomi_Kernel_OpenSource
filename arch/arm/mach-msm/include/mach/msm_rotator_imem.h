/* Copyright (c) 2009, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_ROTATOR_IMEM_H__

enum {
	ROTATOR_REQUEST,
	JPEG_REQUEST
};

/* Allocates imem for the requested owner.
   Aquires a mutex, so DO NOT call from isr context */
int msm_rotator_imem_allocate(int requestor);
/* Frees imem if currently owned by requestor.
   Unlocks a mutex, so DO NOT call from isr context */
void msm_rotator_imem_free(int requestor);

#endif
