// SPDX-License-Identifier: GPL-2.0
/*
 * cpu_load_monitor.h - CPU load monitor interface
 *
 * Copyright (c) 2026 Xiaomi, Inc., All rights reserved.
 */

#ifndef CPU_LOAD_MONITOR_H
#define CPU_LOAD_MONITOR_H

int cpu_load_monitor_init(void);
void cpu_load_monitor_exit(void);

#endif /* CPU_LOAD_MONITOR_H */
