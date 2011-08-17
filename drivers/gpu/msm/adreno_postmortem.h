/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#ifndef __ADRENO_POSTMORTEM_H
#define __ADRENO_POSTMORTEM_H

struct kgsl_device;

int adreno_postmortem_dump(struct kgsl_device *device, int manual);

#endif /* __ADRENO_POSTMORTEM_H */
