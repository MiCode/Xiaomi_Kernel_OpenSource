/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _LINUX_HWCONF_MANAGER_H
#define _LINUX_HWCONF_MANAGER_H

#include "mi_disp_config.h"

#ifdef CONFIG_HWCONF_MANAGER
extern int register_hw_component_info(char *component_name);
extern int add_hw_component_info(char *component_name, char *key, char *value);
extern int unregister_hw_component_info(char *component_name);
extern int register_hw_monitor_info(char *component_name);
extern int add_hw_monitor_info(char *component_name, char *mon_key, char *mon_value);
extern int update_hw_monitor_info(char *component_name, char *mon_key, char *mon_value);
extern int hw_monitor_notifier_register(struct notifier_block *nb);
extern int hw_monitor_notifier_unregister(struct notifier_block *nb);
extern int unregister_hw_monitor_info(char *component_name);
int hwconf_init(void);
void hwconf_exit(void);

#else
static inline int register_hw_component_info(char *component_name)
{
}
static inline int add_hw_component_info(char *component_name, char *key, char *value)
{
}
static inline int unregister_hw_component_info(char *component_name)
{
}
static inline int register_hw_monitor_info(char *component_name)
{
}
static inline int add_hw_monitor_info(char *component_name, char *mon_key, char *mon_value)
{
}
static inline int update_hw_monitor_info(char *component_name, char *mon_key, char *mon_value)
{
}
static inline int hw_monitor_notifier_register(struct notifier_block *nb)
{
}
static inline int hw_monitor_notifier_unregister(struct notifier_block *nb)
{
}
static inline int unregister_hw_monitor_info(char *component_name)
{
}
int hwconf_init(void){return 0;};
void hwconf_exit(void){};

#endif

#endif
