/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/mfd/wcd9xxx/core.h>

#ifndef __MFD_WCD9XXX_IRQ_H
#define __MFD_WCD9XXX_IRQ_H
bool wcd9xxx_lock_sleep(struct wcd9xxx_core_resource *);
void wcd9xxx_unlock_sleep(struct wcd9xxx_core_resource *);
void wcd9xxx_nested_irq_lock(struct wcd9xxx_core_resource *);
void wcd9xxx_nested_irq_unlock(struct wcd9xxx_core_resource *);
int wcd9xxx_request_irq(struct wcd9xxx_core_resource *, int,
			irq_handler_t, const char *, void *);

void wcd9xxx_free_irq(struct wcd9xxx_core_resource *, int, void*);
void wcd9xxx_enable_irq(struct wcd9xxx_core_resource *, int);
void wcd9xxx_disable_irq(struct wcd9xxx_core_resource *, int);
void wcd9xxx_disable_irq_sync(struct wcd9xxx_core_resource *, int);

int wcd9xxx_irq_init(struct wcd9xxx_core_resource *);
void wcd9xxx_irq_exit(struct wcd9xxx_core_resource *);
#endif
