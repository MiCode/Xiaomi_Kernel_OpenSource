/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#ifndef _CNSS_BUS_H
#define _CNSS_BUS_H

#include "main.h"

#define QCA6174_VENDOR_ID		0x168C
#define QCA6174_DEVICE_ID		0x003E
#define QCA6174_REV_ID_OFFSET		0x08
#define QCA6174_REV3_VERSION		0x5020000
#define QCA6174_REV3_2_VERSION		0x5030000
#define QCA6290_VENDOR_ID		0x17CB
#define QCA6290_DEVICE_ID		0x1100
#define QCA6390_VENDOR_ID		0x17CB
#define QCA6390_DEVICE_ID		0x1101
#define QCN7605_VENDOR_ID               0x17CB
#define QCN7605_DEVICE_ID               0x1102

#define QCN7605_USB_VENDOR_ID             0x05C6
#define QCN7605_STANDALONE_PRODUCT_ID    0x9900
#define QCN7605_COMPOSITE_PRODUCT_ID     0x9901

#define QCN7605_COMPOSITE_DEVICE_ID     QCN7605_COMPOSITE_PRODUCT_ID
#define QCN7605_STANDALONE_DEVICE_ID    QCN7605_STANDALONE_PRODUCT_ID

enum cnss_dev_bus_type cnss_get_dev_bus_type(struct device *dev);
enum cnss_dev_bus_type cnss_get_bus_type(struct cnss_plat_data *plat_priv);
void *cnss_bus_dev_to_bus_priv(struct device *dev);
struct cnss_plat_data *cnss_bus_dev_to_plat_priv(struct device *dev);
int cnss_bus_init(struct cnss_plat_data *plat_priv);
void cnss_bus_deinit(struct cnss_plat_data *plat_priv);
int cnss_bus_load_m3(struct cnss_plat_data *plat_priv);
int cnss_bus_alloc_fw_mem(struct cnss_plat_data *plat_priv);
int cnss_bus_alloc_qdss_mem(struct cnss_plat_data *plat_priv);
void cnss_bus_free_qdss_mem(struct cnss_plat_data *plat_priv);
u32 cnss_bus_get_wake_irq(struct cnss_plat_data *plat_priv);
int cnss_bus_force_fw_assert_hdlr(struct cnss_plat_data *plat_priv);
void cnss_bus_fw_boot_timeout_hdlr(unsigned long data);
void cnss_bus_collect_dump_info(struct cnss_plat_data *plat_priv,
				bool in_panic);
int cnss_bus_call_driver_probe(struct cnss_plat_data *plat_priv);
int cnss_bus_call_driver_remove(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_powerup(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_shutdown(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_crash_shutdown(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_ramdump(struct cnss_plat_data *plat_priv);
int cnss_bus_register_driver_hdlr(struct cnss_plat_data *plat_priv, void *data);
int cnss_bus_unregister_driver_hdlr(struct cnss_plat_data *plat_priv);
int cnss_bus_call_driver_modem_status(struct cnss_plat_data *plat_priv,
				      int modem_current_status);
int cnss_bus_update_status(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_status status);
bool cnss_bus_req_mem_ind_valid(struct cnss_plat_data *plat_priv);
int cnss_get_msi_assignment(struct cnss_plat_data *plat_priv,
			    char *msi_name,
			    int *num_vectors,
			    u32 *user_base_data,
			    u32 *base_vector);
int cnss_bus_get_iova(struct cnss_plat_data *plat_priv, u64 *addr, u64 *size);
int cnss_bus_get_iova_ipa(struct cnss_plat_data *plat_priv, u64 *addr,
			  u64 *size);
#endif /* _CNSS_BUS_H */
