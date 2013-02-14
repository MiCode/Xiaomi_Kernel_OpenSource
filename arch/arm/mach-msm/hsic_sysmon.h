/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __HSIC_SYSMON_H__
#define __HSIC_SYSMON_H__

/**
 * enum hsic_sysmon_device_id - Supported HSIC subsystem devices
 */
enum hsic_sysmon_device_id {
	HSIC_SYSMON_DEV_EXT_MODEM,
	NUM_HSIC_SYSMON_DEVS
};

#if defined(CONFIG_MSM_HSIC_SYSMON) || defined(CONFIG_MSM_HSIC_SYSMON_MODULE)

extern int hsic_sysmon_open(enum hsic_sysmon_device_id id);
extern void hsic_sysmon_close(enum hsic_sysmon_device_id id);
extern int hsic_sysmon_read(enum hsic_sysmon_device_id id, char *data,
			    size_t len, size_t *actual_len, int timeout);
extern int hsic_sysmon_write(enum hsic_sysmon_device_id id, const char *data,
			     size_t len, int timeout);

#else /* CONFIG_MSM_HSIC_SYSMON || CONFIG_MSM_HSIC_SYSMON_MODULE */

static inline int hsic_sysmon_open(enum hsic_sysmon_device_id id)
{
	return -ENODEV;
}

static inline void hsic_sysmon_close(enum hsic_sysmon_device_id id) { }

static inline int hsic_sysmon_read(enum hsic_sysmon_device_id id, char *data,
				   size_t len, size_t *actual_len, int timeout)
{
	return -ENODEV;
}

static inline int hsic_sysmon_write(enum hsic_sysmon_device_id id,
				    const char *data, size_t len, int timeout)
{
	return -ENODEV;
}

#endif /* CONFIG_MSM_HSIC_SYSMON || CONFIG_MSM_HSIC_SYSMON_MODULE */

#endif /* __HSIC_SYSMON_H__ */
