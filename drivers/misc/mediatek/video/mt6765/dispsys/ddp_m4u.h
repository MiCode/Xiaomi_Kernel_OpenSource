/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DSI_M4U_H__
#define __DSI_M4U_H__

#ifdef CONFIG_MTK_IOMMU_V2
#include "mach/mt_iommu.h"
#include <soc/mediatek/smi.h>
#elif defined(CONFIG_MTK_M4U)
#include "m4u.h"
#include "m4u_port.h"
#endif
#include "ddp_hal.h"
#include "mtk_ion.h"
#include "ion_drv.h"
#ifdef __cplusplus
extern "C" {
#endif

/* display m4u port wrapper
 * -- by chip
 */
#define DISP_M4U_PORT_DISP_OVL0 M4U_PORT_DISP_OVL0
#define DISP_M4U_PORT_DISP_OVL0_2L M4U_PORT_DISP_2L_OVL0_LARB0
#define DISP_M4U_PORT_DISP_RDMA0 M4U_PORT_DISP_RDMA0
#define DISP_M4U_PORT_DISP_WDMA0 M4U_PORT_DISP_WDMA0

struct module_to_m4u_port_t {
	enum DISP_MODULE_ENUM module;
	int larb;
	int port;
};

int module_to_m4u_port(enum DISP_MODULE_ENUM module);
enum DISP_MODULE_ENUM m4u_port_to_module(int port);
int disp_m4u_callback(int port, unsigned long mva, void *data);
void disp_m4u_init(void);
int config_display_m4u_port(void);

int disp_mva_map_kernel(enum DISP_MODULE_ENUM module, unsigned int mva,
	unsigned int size, unsigned long *map_va, unsigned int *map_size);
int disp_mva_unmap_kernel(unsigned int mva, unsigned int size,
	unsigned long map_va);

struct ion_client *disp_ion_create(const char *name);
struct ion_handle *disp_ion_alloc(struct ion_client *client,
	unsigned int heap_id_mask, size_t align, unsigned int size);
int disp_ion_get_mva(struct ion_client *client, struct ion_handle *handle,
	unsigned long *mva, int port);
struct ion_handle *disp_ion_import_handle(struct ion_client *client, int fd);
void disp_ion_free_handle(struct ion_client *client,
	struct ion_handle *handle);
void disp_ion_cache_flush(struct ion_client *client,
	struct ion_handle *handle, enum ION_CACHE_SYNC_TYPE sync_type);
void disp_ion_destroy(struct ion_client *client);

#ifdef CONFIG_MTK_M4U
int disp_allocate_mva(struct m4u_client_t *client, enum DISP_MODULE_ENUM module,
	unsigned long va, struct sg_table *sg_table, unsigned int size,
	unsigned int prot, unsigned int flags, unsigned int *pMva);
int disp_hal_allocate_framebuffer(phys_addr_t pa_start, phys_addr_t pa_end,
	unsigned long *va, unsigned long *mva);
#endif


#ifdef __cplusplus
}
#endif
#endif
