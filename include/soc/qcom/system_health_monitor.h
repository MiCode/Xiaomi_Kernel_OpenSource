/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef SYSTEM_HEALTH_MONITOR_H
#define SYSTEM_HEALTH_MONITOR_H

#ifdef CONFIG_SYSTEM_HEALTH_MONITOR
/**
 * kern_check_system_health() - Check the system health
 *
 * This function is used by the kernel drivers to initiate the
 * system health check. This function in turn trigger SHM to send
 * QMI message to all the HMAs connected to it.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
int kern_check_system_health(void);
#else
static inline int kern_check_system_health(void)
{
	return -ENODEV;
}
#endif /* CONFIG_SYSTEM_HEALTH_MONITOR */

#endif /* SYSTEM_HEALTH_MONITOR_H */
