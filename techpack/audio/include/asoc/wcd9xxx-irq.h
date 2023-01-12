/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2017, 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/types.h>
#include "core.h"

#ifndef __MFD_WCD9XXX_IRQ_H
#define __MFD_WCD9XXX_IRQ_H
#ifdef CONFIG_WCD9XXX_CODEC_CORE
bool wcd9xxx_lock_sleep(struct wcd9xxx_core_resource *wcd9xxx_res);
void wcd9xxx_unlock_sleep(struct wcd9xxx_core_resource *wcd9xxx_res);
void wcd9xxx_nested_irq_lock(struct wcd9xxx_core_resource *wcd9xxx_res);
void wcd9xxx_nested_irq_unlock(struct wcd9xxx_core_resource *wcd9xxx_res);
int wcd9xxx_request_irq(struct wcd9xxx_core_resource *wcd9xxx_res, int irq,
			irq_handler_t handler, const char *name, void *data);

void wcd9xxx_free_irq(struct wcd9xxx_core_resource *wcd9xxx_res,
			int irq, void *data);
void wcd9xxx_enable_irq(struct wcd9xxx_core_resource *wcd9xxx_res, int irq);
void wcd9xxx_disable_irq(struct wcd9xxx_core_resource *wcd9xxx_res,
			int irq);
void wcd9xxx_disable_irq_sync(struct wcd9xxx_core_resource *wcd9xxx_res,
			int irq);

int wcd9xxx_irq_init(struct wcd9xxx_core_resource *wcd9xxx_res);
void wcd9xxx_irq_exit(struct wcd9xxx_core_resource *wcd9xxx_res);
int wcd9xxx_irq_drv_init(void);
void wcd9xxx_irq_drv_exit(void);
#else
bool wcd9xxx_lock_sleep(struct wcd9xxx_core_resource *wcd9xxx_res)
{
	return false;
}
void wcd9xxx_unlock_sleep(struct wcd9xxx_core_resource *wcd9xxx_res)
{
}
void wcd9xxx_nested_irq_lock(struct wcd9xxx_core_resource *wcd9xxx_res)
{
}
void wcd9xxx_nested_irq_unlock(struct wcd9xxx_core_resource *wcd9xxx_res)
{
}
int wcd9xxx_request_irq(struct wcd9xxx_core_resource *wcd9xxx_res, int irq,
			irq_handler_t handler, const char *name, void *data)
{
	return 0;
}

void wcd9xxx_free_irq(struct wcd9xxx_core_resource *wcd9xxx_res,
			int irq, void *data)
{
}
void wcd9xxx_enable_irq(struct wcd9xxx_core_resource *wcd9xxx_res, int irq)
{
}
void wcd9xxx_disable_irq(struct wcd9xxx_core_resource *wcd9xxx_res,
			int irq)
{
}
void wcd9xxx_disable_irq_sync(struct wcd9xxx_core_resource *wcd9xxx_res,
			int irq)
{
}

int wcd9xxx_irq_init(struct wcd9xxx_core_resource *wcd9xxx_res)
{
	return 0;
}
void wcd9xxx_irq_exit(struct wcd9xxx_core_resource *wcd9xxx_res)
{
}
int wcd9xxx_irq_drv_init(void)
{
	return 0;
}

void wcd9xxx_irq_drv_exit(void)
{
}
#endif
#endif
