/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
