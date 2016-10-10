/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _NET_CNSS_COMMON_H_
#define _NET_CNSS_COMMON_H_

/* max 20mhz channel count */
#define CNSS_MAX_CH_NUM		45

int cnss_pci_request_bus_bandwidth(int bandwidth);
int cnss_sdio_request_bus_bandwidth(int bandwidth);

void cnss_sdio_device_crashed(void);
void cnss_pci_device_crashed(void);

void cnss_pci_device_self_recovery(void);
void cnss_sdio_device_self_recovery(void);

void *cnss_pci_get_virt_ramdump_mem(unsigned long *size);
void *cnss_sdio_get_virt_ramdump_mem(unsigned long *size);

void cnss_sdio_schedule_recovery_work(void);
void cnss_pci_schedule_recovery_work(void);

int cnss_pcie_set_wlan_mac_address(const u8 *in, uint32_t len);
int cnss_sdio_set_wlan_mac_address(const u8 *in, uint32_t len);

u8 *cnss_pci_get_wlan_mac_address(uint32_t *num);
u8 *cnss_sdio_get_wlan_mac_address(uint32_t *num);
int cnss_sdio_power_up(struct device *dev);
int cnss_sdio_power_down(struct device *dev);
int cnss_pcie_power_up(struct device *dev);
int cnss_pcie_power_down(struct device *dev);
const char *cnss_wlan_get_evicted_data_file(void);
#endif /* _NET_CNSS_COMMON_H_ */
