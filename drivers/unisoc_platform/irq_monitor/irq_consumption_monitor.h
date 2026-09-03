/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Unisoc Inc.
 */

#ifndef __IRQ_CONSUMPTION_MONITOR_H__
#define __IRQ_CONSUMPTION_MONITOR_H__

#if IS_ENABLED(CONFIG_IRQ_CONSUMPTION_MONITOR)
void consumption_monitor_init(void);
#else
void consumption_monitor_init(void) {}
#endif

#if IS_ENABLED(CONFIG_IRQ_CONSUMPTION_MONITOR)
void consumption_monitor_exit(void);
#else
void consumption_monitor_exit(void) {}
#endif

#endif





