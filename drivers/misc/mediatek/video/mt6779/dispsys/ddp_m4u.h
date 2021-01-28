/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __DSI_M4U_H__
#define __DSI_M4U_H__

#include <linux/types.h>
#if defined(CONFIG_MTK_M4U)
#include "m4u.h"
#include "m4u_port.h"
#endif
#include "ddp_hal.h"
#if defined(CONFIG_MTK_ION)
#include "mtk_ion.h"
#include "ion_drv.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

/**
 * display m4u port wrapper
 * -- by chip
 */
#if defined(CONFIG_MTK_M4U)
/* larb 0 */
/* TODO: PVRIC, postmask, fake engine */
#define DISP_M4U_PORT_DISP_OVL0 M4U_PORT_DISP_OVL0
#define DISP_M4U_PORT_DISP_RDMA0 M4U_PORT_DISP_RDMA0
#define DISP_M4U_PORT_DISP_WDMA0 M4U_PORT_DISP_WDMA0
#define DISP_M4U_PORT_DISP_POSTMASK M4U_PORT_DISP_POSTMASK0
#define DISP_M4U_PORT_DISP_OVL0_HDR M4U_PORT_DISP_OVL0_HDR

/* larb 1 */
#define DISP_M4U_PORT_DISP_OVL0_2L M4U_PORT_DISP_OVL0_2L
#define DISP_M4U_PORT_DISP_OVL1_2L M4U_PORT_DISP_OVL1_2L
#define DISP_M4U_PORT_DISP_RDMA1 M4U_PORT_DISP_RDMA1
#define DISP_M4U_PORT_DISP_OVL0_2L_HDR M4U_PORT_DISP_OVL0_2L_HDR
#else
/* larb 0 */
/* TODO: PVRIC, postmask, fake engine */
#define DISP_M4U_PORT_DISP_OVL0 0
#define DISP_M4U_PORT_DISP_RDMA0 0
#define DISP_M4U_PORT_DISP_WDMA0 0
#define DISP_M4U_PORT_DISP_POSTMASK 0
#define DISP_M4U_PORT_DISP_OVL0_HDR 0

/* larb 1 */
#define DISP_M4U_PORT_DISP_OVL0_2L 0
#define DISP_M4U_PORT_DISP_OVL1_2L 0
#define DISP_M4U_PORT_DISP_RDMA1 0
#define DISP_M4U_PORT_DISP_OVL0_2L_HDR 0
#endif

struct module_to_m4u_port_t {
	enum DISP_MODULE_ENUM module;
	int larb;
	int port;
};

int module_to_m4u_port(enum DISP_MODULE_ENUM module);
enum DISP_MODULE_ENUM m4u_port_to_module(int port);
int disp_m4u_callback(int port, unsigned long mva,
					  void *data);
void disp_m4u_init(void);
int config_display_m4u_port(void);

int disp_mva_map_kernel(enum DISP_MODULE_ENUM module, unsigned int mva,
			unsigned int size, unsigned long *map_va,
			unsigned int *map_size);
int disp_mva_unmap_kernel(unsigned int mva, unsigned int size,
			  unsigned long map_va);

struct ion_client *disp_ion_create(const char *name);
struct ion_handle *disp_ion_alloc(struct ion_client *client,
				  unsigned int heap_id_mask, size_t align,
				  unsigned int size);
int *disp_aosp_ion_alloc(unsigned int heap_id_mask,
				  unsigned int size);
int disp_ion_get_mva(struct ion_client *client, struct ion_handle *handle,
		     unsigned int *mva, int port);
int disp_aosp_ion_get_iova(struct device *dev, int fd,
		     dma_addr_t *iova);
struct ion_handle *disp_ion_import_handle(struct ion_client *client, int fd);
void disp_ion_free_handle(struct ion_client *client, struct ion_handle *handle);
#if defined(MTK_FB_ION_SUPPORT)
void disp_ion_cache_flush(struct ion_client *client, struct ion_handle *handle,
			  enum ION_CACHE_SYNC_TYPE sync_type);
#endif
void disp_ion_destroy(struct ion_client *client);

int disp_hal_allocate_framebuffer(phys_addr_t pa_start, phys_addr_t pa_end,
				  unsigned long *va, unsigned long *mva);

#ifdef MTKFB_M4U_SUPPORT
int disp_allocate_mva(struct m4u_client_t *client, enum DISP_MODULE_ENUM module,
		      unsigned long va, struct sg_table *sg_table,
		      unsigned int size, unsigned int prot, unsigned int flags,
		      unsigned int *pMva);
#endif

#ifdef CONFIG_MTK_IOMMU_V2
int disp_aosp_set_dev(struct device *dev);
int disp_aosp_release_reserved_area(phys_addr_t pa_start,
		     phys_addr_t pa_end);
int disp_aosp_alloc_iova(struct device *dev, phys_addr_t pa_start,
		     phys_addr_t pa_end,
		     unsigned long *va,
		     dma_addr_t *iova);


int disp_aosp_mmap(struct vm_area_struct *vma, unsigned long va,
	unsigned long mva, unsigned int size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DSI_M4U_H__ */
