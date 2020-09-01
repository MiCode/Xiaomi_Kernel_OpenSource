/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _KGSL_BUS_H
#define _KGSL_BUS_H

enum kgsl_bus_vote {
	KGSL_BUS_VOTE_OFF = 0,
	KGSL_BUS_VOTE_ON,
	KGSL_BUS_VOTE_MINIMUM,
};

struct kgsl_device;
struct platform_device;

int kgsl_bus_init(struct kgsl_device *device, struct platform_device *pdev);
void kgsl_bus_close(struct kgsl_device *device);
int kgsl_bus_update(struct kgsl_device *device, enum kgsl_bus_vote vote_state);

u32 *kgsl_bus_get_table(struct platform_device *pdev,
		const char *name, int *count);

#endif
