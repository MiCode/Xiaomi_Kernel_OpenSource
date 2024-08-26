 /*
 * mi_hwconf_manager.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#ifndef _LINUX_HWCONF_MANAGER_H
#define _LINUX_HWCONF_MANAGER_H

#ifdef CONFIG_MI_HWCONF_MANAGER
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
    return 0;
}
static inline int add_hw_component_info(char *component_name, char *key, char *value)
{
    return 0;
}
static inline int unregister_hw_component_info(char *component_name)
{
    return 0;
}
static inline int register_hw_monitor_info(char *component_name)
{
    return 0;
}
static inline int add_hw_monitor_info(char *component_name, char *mon_key, char *mon_value)
{
    return 0;
}
static inline int update_hw_monitor_info(char *component_name, char *mon_key, char *mon_value)
{
    return 0;
}
static inline int hw_monitor_notifier_register(struct notifier_block *nb)
{
    return 0;
}
static inline int hw_monitor_notifier_unregister(struct notifier_block *nb)
{
    return 0;
}
static inline int unregister_hw_monitor_info(char *component_name)
{
    return 0;
}
int hwconf_init(void) { return 0; }
void hwconf_exit(void) {}
#endif
#endif
