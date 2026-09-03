/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright 2019-2023 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

 /* mpms: modem powermanger source */
#ifndef _SPRD_PCIE_RESOURCE_H
#define _SPRD_PCIE_RESOURCE_H

#ifdef CONFIG_UNISOC_PCIE_EP_DEVICE
#include <linux/platform_device.h>
#endif
#include <linux/soc/sprd/hwfeature.h>

#if defined(CONFIG_UNISOC_PCIE_EP_DEVICE) || defined(CONFIG_PCIE_EPF_SPRD)
/*
 * sprd_pcie_wait_resource
 * Returns:
 *  0 resource ready,
 *  < 0 resoure not ready,
 *  -%ERESTARTSYS if it was interrupted by a signal.
 */
int sprd_pcie_wait_resource(u32 dst, int timeout);

int sprd_pcie_request_resource(u32 dst);
int sprd_pcie_release_resource(u32 dst);
int sprd_pcie_resource_trash(u32 dst);
bool sprd_pcie_is_defective_chip(void);
#else
/* dummy functions */
static inline int sprd_pcie_wait_resource(u32 dst, int timeout) {return 0; }

static inline int sprd_pcie_request_resource(u32 dst) {return 0; }
static inline int sprd_pcie_release_resource(u32 dst) {return 0; }
static inline int sprd_pcie_resource_trash(u32 dst) {return 0; }
static inline bool sprd_pcie_is_defective_chip(void) {return false; }
#endif

#ifdef CONFIG_PCIE_EPF_SPRD
int sprd_pcie_resource_client_init(u32 dst, u32 ep_fun);
int sprd_register_pcie_resource_first_ready(u32 dst,
					    void (*notify)(void *p),
					    void *data);
#endif

#ifdef CONFIG_UNISOC_PCIE_EP_DEVICE
int sprd_pcie_resource_host_init(u32 dst, u32 ep_dev,
				 struct platform_device *pcie_dev);

/*
 * sprd_pcie_resource_reboot_ep
 * reboot ep contains rescan ep device.
 */
void sprd_pcie_resource_reboot_ep(u32 dst, bool b_warm_reboot);

/*
 * sprd_pcie_wait_load_resource
 * In case of the open the feature CONFIG_PCIE_SPRD_SPLIT_BAR,
 * It has 2 times pcie scan action in host side boot process.
 * After the first scan, the ep only have 2 bar can be used for
 * memory map, the pcie resource is not completely ready,
 * but the host can load images for ep, so we add the special api
 * sprd_pcie_wait_load_resource, this api will return after
 * the first scan action.
 * Returns:
 *  0 resource ready,
 *  < 0 resoure not ready,
 *  -%ERESTARTSYS if it was interrupted by a signal.
 */
int sprd_pcie_wait_load_resource(u32 dst);

/*
 * sprd_pcie_wait_remove_resource
 * max wait 5s for pcie remove.
 */
int sprd_pcie_wait_remove_resource(u32 dst);

/*
 * sprd_pcie_img_load_done
 * If the chip is AA chip, It has 2 times pcie scan action in host side
 * boot process. After the first scan, the ep only have 2 bar can be
 * used for memory map, the pcie resource is not completely ready,
 * but the host can load images for ep,  and after or images are loaded,
 * need call this api to start the 2nd scan action.
 */
void sprd_pcie_img_load_done(u32 dst);


/*
 * sprd_pcie_ep_power_on
 */
bool sprd_pcie_ep_power_on(void);
#else
static inline int sprd_pcie_wait_remove_resource(u32 dst) { return 0; }
#endif /* CONFIG_UNISOC_PCIE_EP_DEVICE */

#endif /* _SPRD_PCIE_RESOURCE_H */
