/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _READY_H_
#define _READY_H_

#include <linux/notifier.h>

enum sensor_ready_priority {
	READY_STDPRI, /* others use std */
	READY_HIGHPRI, /* transceiver use high */
	READY_HIGHESTPRI, /* sensor comm use highest */
};

void sensor_ready_notifier_chain_register(struct notifier_block *nb);
void sensor_ready_notifier_chain_unregister(struct notifier_block *nb);
int host_ready_init(void);
void host_ready_exit(void);

#endif
