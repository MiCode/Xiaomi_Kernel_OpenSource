/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _H_VPU_RESOURCES_H_
#define _H_VPU_RESOURCES_H_

#include <linux/platform_device.h>
#include <linux/msm_iommu_domains.h>
#include <mach/msm_bus.h>

/*
 * Device Tree Resources
 */
#define VPU_MAX_PLANES 2

enum vpu_clocks {
	VPU_BUS_CLK,
	VPU_MAPLE_CLK,
	VPU_VDP_CLK,
	VPU_VDP_XIN,
	VPU_AHB_CLK,
	VPU_AXI_CLK,
	VPU_SLEEP_CLK,
	VPU_CXO_CLK,
	VPU_MAPLE_AXI_CLK,
	VPU_PRNG_CLK,

	VPU_FRC_GPROC,
	VPU_FRC_KPROC,
	VPU_FRC_SDMC_FRCS,
	VPU_FRC_SDME_FRCF,
	VPU_FRC_SDME_FRCS,
	VPU_FRC_SDME_VPRO,
	VPU_FRC_HDMC_FRCF,
	VPU_FRC_PREPROC,
	VPU_FRC_FRC_XIN,
	VPU_FRC_MAPLE_AXI,

	VPU_QDSS_AT,
	VPU_QDSS_TSCTR_DIV8,

	VPU_MAX_CLKS
};

enum vpu_clock_flag {

	/* Group section */
	CLOCK_CORE =		1 << 1,
	CLOCK_FRC =		1 << 2,
	CLOCK_QDSS =		1 << 3,
	CLOCK_ALL_GROUPS =	(1 << 12) - 1,

	/* Property section */
	CLOCK_PRESENT =		1 << 12,
	CLOCK_SCALABLE =	1 << 13,
	CLOCK_BOOT =		1 << 14,
};

enum vpu_power_mode {
	VPU_POWER_SVS,
	VPU_POWER_NOMINAL,
	VPU_POWER_TURBO,
	VPU_POWER_DYNAMIC,
	VPU_POWER_MAX = VPU_POWER_DYNAMIC
};

struct vpu_clock {
	struct clk *clk;
	u32 status;
	u32 dynamic_freq;
	const char *name;
	const u32 *pwr_frequencies;
	u32 flag;
};

struct bus_load_tbl {
	u32 *loads;
	int count;
};

struct reg_value_pair {
	u32 reg_offset;
	u32 value;
};

struct reg_value_set {
	struct reg_value_pair *table;
	int count;
};

struct vpu_iommu_map {
	const char *client_name;
	const char *ctx_name;
	struct device *ctx;
	struct iommu_domain *domain;
	int domain_num;
	struct msm_iova_partition partitions[1];
	int npartitions;
	bool is_secure;
	bool enabled;
	bool attached;
};

struct iommu_set {
	struct vpu_iommu_map *iommu_maps;
	int count;
};

struct vpu_platform_resources {
	struct platform_device *pdev;

	/* device register and mem window */
	phys_addr_t register_base_phy;
	phys_addr_t mem_base_phy;
	phys_addr_t vbif_base_phy;
	u32 register_size;
	u32 mem_size;
	u32 vbif_size;

	/* interrupt number */
	u32 irq; /* Firmware to APPS IPC irq */
	u32 irq_wd; /* Firmware's watchdog irq */

	/* device tree configs */
	struct bus_load_tbl bus_table;
	struct msm_bus_scale_pdata bus_pdata;
	struct reg_value_set vbif_reg_set;
	struct iommu_set iommu_set;
	struct vpu_clock clock[VPU_MAX_CLKS];

	/* VPU memory client */
	void *mem_client;
};

int read_vpu_platform_resources(struct vpu_platform_resources *res,
			struct platform_device *pdev);
void free_vpu_platform_resources(struct vpu_platform_resources *res);


/* Registration of IOMMU hardware domains */
int register_vpu_iommu_domains(struct vpu_platform_resources *res);
void unregister_vpu_iommu_domains(struct vpu_platform_resources *res);

/* Activation of IOMMU hardware domains */
int attach_vpu_iommus(struct vpu_platform_resources *res);
void detach_vpu_iommus(struct vpu_platform_resources *res);


/*
 * memory API
 */
enum mem_device_id {
	MEM_VPU_ID = 0,
	MEM_VCAP_ID,
	MEM_MDP_ID,
	MEM_MAX_ID
};

/* Create/Destroy memory buffer allocation handle */
void *vpu_mem_create_handle(void *mem_client);
void vpu_mem_destroy_handle(void *mem_handle);

/* Map pre-allocated memory identified by fd (map to VPU domain) */
int vpu_mem_map_fd(void *mem_handle, int fd, u32 length,
		u32 offset, bool secure);

/* Allocate memory then map to VPU domain */
int vpu_mem_alloc(void *mem_handle, u32 size, bool secure);

/* map to a specific device domain. Call after mem_map_fd or mem_alloc */
int vpu_mem_map_to_device(void *mem_handle, u32 device_id, int domain_num);

/* unmap from a specific device domain */
void vpu_mem_unmap_from_device(void *mem_handle, u32 device_id);

/* Get iommu mapped address for the given device id */
phys_addr_t vpu_mem_addr(void *mem_handle, u32 device_id);

/* Get buffer size for corresponding handle */
u32 vpu_mem_size(void *mem_handle);

#endif /* _H_VPU_RESOURCES_H_ */
