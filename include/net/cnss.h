/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#ifndef _NET_CNSS_H_
#define _NET_CNSS_H_

#include <linux/device.h>
#include <linux/pci.h>

/* max 20mhz channel count */
#define CNSS_MAX_CH_NUM       45

struct dev_info {
	struct device	*dev;
	char	*dump_buffer;
	unsigned long dump_size;
	int (*dev_shutdown)(void);
	int (*dev_powerup)(void);
	void (*dev_crashshutdown)(void);
};

struct cnss_wlan_driver {
	char *name;
	int  (*probe)(struct pci_dev *, const struct pci_device_id *);
	void (*remove)(struct pci_dev *);
	int  (*suspend)(struct pci_dev *, pm_message_t state);
	int  (*resume)(struct pci_dev *);
	const struct pci_device_id *id_table;
};

extern int cnss_config(struct dev_info *device_info);
extern void cnss_deinit(void);
extern void cnss_device_crashed(void);
extern int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count);
extern int cnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
						u16 *ch_count, u16 buf_len);
extern int cnss_wlan_register_driver(struct cnss_wlan_driver *driver);
extern void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver);

#endif /* _NET_CNSS_H_ */
