/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 */
#ifndef __USFCDEV_H__
#define __USFCDEV_H__

#include <linux/input.h>

/* TSC event type index in the containers of the handlers & handles */
#define TSC_EVENT_TYPE_IND 0
/* Number of supported event types to be filtered */
#define MAX_EVENT_TYPE_NUM 1

bool usfcdev_register(
	uint16_t event_type_ind,
	bool (*match_cb)(uint16_t, struct input_dev *dev));
void usfcdev_unregister(uint16_t event_type_ind);
bool usfcdev_set_filter(uint16_t event_type_ind, bool filter);
#endif /* __USFCDEV_H__ */
