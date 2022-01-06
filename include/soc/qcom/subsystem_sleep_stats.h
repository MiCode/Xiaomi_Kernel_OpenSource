/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SUBSYSTEM_SLEEP_STATS_H__
#define __SUBSYSTEM_SLEEP_STATS_H__

bool has_system_slept(void);
bool has_subsystem_slept(void);
void subsystem_sleep_debug_enable(bool enable);
#endif /*__SUBSYSTEM_SLEEP_STATS_H__ */
