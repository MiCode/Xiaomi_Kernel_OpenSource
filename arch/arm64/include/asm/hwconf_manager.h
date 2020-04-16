/*
 * hwconf_manager.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_HWCONF_MANAGER_H
#define _LINUX_HWCONF_MANAGER_H

int register_hw_component_info(char *component_name);
int add_hw_component_info(char *component_name, char *key, char *value);
int unregister_hw_component_info(char *component_name);

int register_hw_monitor_info(char *component_name);
int add_hw_monitor_info(char *component_name, char *mon_key, char *mon_value);
int update_hw_monitor_info(char *component_name, char *mon_key, char *mon_value);
int unregister_hw_monitor_info(char *component_name);

#endif
