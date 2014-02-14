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

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/msm_ion.h>
#include <linux/iommu.h>
#include <linux/qcom_iommu.h>
#include <linux/msm_iommu_domains.h>
#include <linux/msm-bus-board.h>
#include <asm-generic/sizes.h>

#include "vpu_resources.h"
#include "vpu_v4l2.h"

/*
 * Device Tree Resources
 */

enum vpu_iommu_domains {
	VPU_HLOS_IOMMU_DOMAIN,
	VPU_CP_IOMMU_DOMAIN,
	VPU_FW_IOMMU_DOMAIN,
	VPU_MAX_IOMMU_DOMAIN,
};

static struct vpu_iommu_map iommus_array[VPU_MAX_IOMMU_DOMAIN] = {
	[VPU_HLOS_IOMMU_DOMAIN] = {
		.client_name = "vpu_nonsecure",
		.ctx_name = "vpu_hlos",
		.domain_num = -1,
		.is_secure = 0,
		.partitions = {
			{
				.start = SZ_128K,
				.size = SZ_1G - SZ_128K,
			},
		},
		.npartitions = 1,
	},

	[VPU_CP_IOMMU_DOMAIN] = {
		.client_name = "vpu_secure",
		.ctx_name = "vpu_cp",
		.domain_num = -1,
		.is_secure = 1,
		.partitions = {
			{
				.start = SZ_1G,
				.size = SZ_1G,
			},
		},
		.npartitions = 1,
	},

	[VPU_FW_IOMMU_DOMAIN] = {
		.client_name = "vpu_firmware",
		.ctx_name = "vpu_fw",
		.domain_num = -1,
		.is_secure = 1,
		.partitions = {
			{
				.start = 0,
				.size = SZ_16M,
			},
		},
		.npartitions = 1,
	},
};

struct vpu_clock_descr {
	char *name;
	u32 flag;	/* enum vpu_clock_flag */
	u32 pwr_frequencies[VPU_POWER_MAX];
};

const struct vpu_clock_descr vpu_clock_set[VPU_MAX_CLKS] = {
		[VPU_BUS_CLK] = {
			.name = "vdp_bus_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT | CLOCK_SCALABLE,
			.pwr_frequencies = { 40000000,  80000000,  80000000},
		},
		[VPU_MAPLE_CLK] = {
			.name = "core_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT | CLOCK_SCALABLE,
			.pwr_frequencies = {200000000, 400000000, 400000000},
		},
		[VPU_VDP_CLK] = {
			.name = "vdp_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT | CLOCK_SCALABLE,
			.pwr_frequencies = {200000000, 200000000, 400000000},
		},
		[VPU_VDP_XIN] = {
			.name = "vdp_xin_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT | CLOCK_SCALABLE,
			.pwr_frequencies = {200000000, 467000000, 467000000},
		},
		[VPU_AHB_CLK] = {
			.name = "iface_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT,
		},
		[VPU_AXI_CLK] = {
			.name = "bus_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT,
		},
		[VPU_SLEEP_CLK] = {
			.name = "sleep_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT,
		},
		[VPU_CXO_CLK] = {
			.name = "cxo_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT,
		},
		[VPU_MAPLE_AXI_CLK] = {
			.name = "maple_bus_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT,
		},
		[VPU_PRNG_CLK] = {
			.name = "prng_clk",
			.flag = CLOCK_CORE | CLOCK_BOOT,
		},
		[VPU_FRC_GPROC] {
			.name = "gproc_clk",
			.flag = CLOCK_FRC | CLOCK_BOOT,
		},
		[VPU_FRC_KPROC] {
			.name = "kproc_clk",
			.flag = CLOCK_FRC | CLOCK_BOOT,
		},
		[VPU_FRC_SDMC_FRCS] {
			.name = "sdmc_frcs_clk",
			.flag = CLOCK_FRC,
		},
		[VPU_FRC_SDME_FRCF] {
			.name = "sdme_frcf_clk",
			.flag = CLOCK_FRC,
		},
		[VPU_FRC_SDME_FRCS] {
			.name = "sdme_frcs_clk",
			.flag = CLOCK_FRC,
		},
		[VPU_FRC_SDME_VPRO] {
			.name = "sdme_vproc_clk",
			.flag = CLOCK_FRC,
		},
		[VPU_FRC_HDMC_FRCF] {
			.name = "hdmc_frcf_clk",
			.flag = CLOCK_FRC,
		},
		[VPU_FRC_PREPROC] {
			.name = "preproc_clk",
			.flag = CLOCK_FRC,
		},
		[VPU_FRC_FRC_XIN] {
			.name = "frc_xin_clk",
			.flag = CLOCK_FRC,
		},
		[VPU_FRC_MAPLE_AXI] {
			.name = "maple_axi_clk",
			.flag = CLOCK_FRC,
		},
		[VPU_QDSS_AT] {
			.name = "qdss_at_clk",
			.flag = CLOCK_QDSS,
		},
		[VPU_QDSS_TSCTR_DIV8] {
			.name = "qdss_tsctr_div8_clk",
			.flag = CLOCK_QDSS,
		},
};

struct bus_pdata_config {
	int masters[2];
	int slaves[2];
	char *name;
};

static struct bus_pdata_config bus_pdata_config = {
	.masters = {MSM_BUS_MASTER_VPU, MSM_BUS_MASTER_VPU},
	.slaves = {MSM_BUS_SLAVE_EBI_CH0, MSM_BUS_SLAVE_EBI_CH0},
	.name = "qcom,bus-load-vector-tbl",
};

static int __get_u32_array_num_elements(struct platform_device *pdev,
		const char *name, u32 element_width)
{
	struct device_node *np = pdev->dev.of_node;
	int len_bytes = 0, num_elements = 0;

	if (!of_get_property(np, name, &len_bytes))
		return 0;

	num_elements = len_bytes / (sizeof(u32) * element_width);
	return num_elements;
}

static int __vpu_load_bus_vector_data(struct vpu_platform_resources *res,
		int num_elements, int num_ports)
{
	struct bus_vector {
		u32 load;
		u32 ab;
		u32 ib;
	};
	struct platform_device *pdev = res->pdev;
	struct msm_bus_scale_pdata *bus_pdata = &res->bus_pdata;
	struct bus_vector *vectors; /* temporary store of bus load data */
	int i, j;

	vectors = devm_kzalloc(&pdev->dev, sizeof(*vectors) * num_elements,
				GFP_KERNEL);
	if (!vectors) {
		pr_err("Failed to alloc bus_vectors\n");
		return -ENOMEM;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
	    bus_pdata_config.name, (u32 *)vectors,
	    num_elements * (sizeof(*vectors)/sizeof(u32)))) {
		pr_err("Failed to read bus values\n");
		return -EINVAL;
	}

	bus_pdata->name = bus_pdata_config.name;
	bus_pdata->num_usecases = num_elements;

	bus_pdata->usecase = devm_kzalloc(&pdev->dev,
		sizeof(*bus_pdata->usecase) * num_elements, GFP_KERNEL);
	if (!bus_pdata->usecase) {
		pr_err("Failed to alloc bus_pdata usecase\n");
		return -ENOMEM;
	}

	for (i = 0; i < bus_pdata->num_usecases; i++) {
		bus_pdata->usecase[i].vectors = devm_kzalloc(&pdev->dev,
			sizeof(*bus_pdata->usecase[i].vectors) * num_ports,
			GFP_KERNEL);
		if (!bus_pdata->usecase[i].vectors) {
			pr_err("Failed to alloc usecase vectors\n");
			return -ENOMEM;
		}

		res->bus_table.loads[i] = vectors[i].load;

		for (j = 0; j < num_ports; j++) {
			bus_pdata->usecase[i].vectors[j].ab =
					(u64)vectors[i].ab * 1000;
			bus_pdata->usecase[i].vectors[j].ib =
					(u64)vectors[i].ib * 1000;
			bus_pdata->usecase[i].vectors[j].src =
					bus_pdata_config.masters[j];
			bus_pdata->usecase[i].vectors[j].dst =
					bus_pdata_config.slaves[j];
			pr_debug("load=%d, ab=%llu, ib=%llu, src=%d, dst=%d\n",
				res->bus_table.loads[i],
				bus_pdata->usecase[i].vectors[j].ab,
				bus_pdata->usecase[i].vectors[j].ib,
				bus_pdata->usecase[i].vectors[j].src,
				bus_pdata->usecase[i].vectors[j].dst);
		}
		bus_pdata->usecase[i].num_paths = num_ports;
	}

	devm_kfree(&pdev->dev, vectors);
	return 0;
}

static int __vpu_load_bus_vectors(struct vpu_platform_resources *res)
{
	int ret = 0;
	int num_elements = 0;
	struct platform_device *pdev = res->pdev;

	num_elements = __get_u32_array_num_elements(pdev,
			bus_pdata_config.name, 3);
	if (num_elements == 0) {
		pr_warn("no elements in %s\n", bus_pdata_config.name);
		return ret;
	}

	res->bus_table.count = num_elements;
	res->bus_table.loads = devm_kzalloc(&pdev->dev,
		sizeof(*res->bus_table.loads) * num_elements, GFP_KERNEL);
	if (!res->bus_table.loads) {
		pr_err("Failed to alloc memory\n");
		return -ENOMEM;
	}

	ret = __vpu_load_bus_vector_data(res, num_elements, 1);
	if (ret) {
		pr_err("Failed to load bus vector data\n");
		return ret;
	}

	return 0;
}

static int __vpu_load_clk_names(struct vpu_platform_resources *res)
{
	int ret = 0, i, j, num_elements;
	struct platform_device *pdev = res->pdev;
	const char *name;

	num_elements = of_property_count_strings(pdev->dev.of_node,
				"qcom,clock-names");
	if (num_elements <= 0) {
		pr_err("No valid clock list in device tree.\n");
		return -EINVAL;
	} else if (num_elements > VPU_MAX_CLKS) {
		pr_err("List of clocks to enable is too large\n");
		return -EINVAL;
	}

	for (i = 0; i < num_elements; i++) {
		bool found = false;
		ret = of_property_read_string_index(pdev->dev.of_node,
				"qcom,clock-names", i, &name);
		if (ret)
			return ret;

		for (j = 0; j < VPU_MAX_CLKS; j++) {
			if (strcmp(name, vpu_clock_set[j].name) == 0) {
				res->clock[j].name = vpu_clock_set[j].name;
				res->clock[j].flag = vpu_clock_set[j].flag |
								CLOCK_PRESENT;
				res->clock[j].pwr_frequencies =
					vpu_clock_set[j].pwr_frequencies;
				found = true;
				break;
			}
		}
		if (!found) {
			pr_err("clock %s not found\n", name);
			return -EINVAL;
		}
	}

	return 0;
}

static int __vpu_load_reg_values_table(struct vpu_platform_resources *res,
		struct reg_value_set *reg_set, const char *propname)
{
	int ret = 0;
	int num_elements = 0;
	struct platform_device *pdev = res->pdev;

	num_elements = __get_u32_array_num_elements(pdev, propname, 2);
	if (num_elements == 0) {
		pr_debug("no elements in %s\n", propname);
		return 0;
	}

	reg_set->count = num_elements;
	reg_set->table = devm_kzalloc(&pdev->dev,
		sizeof(*reg_set->table) * num_elements, GFP_KERNEL);
	if (!reg_set->table) {
		pr_err("Failed to allocate memory for %s\n", propname);
		return -ENOMEM;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node, propname,
			(u32 *)reg_set->table, num_elements * 2);
	if (ret) {
		pr_err("Failed to read %s table entries\n", propname);
		return ret;
	}

	return 0;
}

static int __vpu_load_iommu_maps(struct vpu_platform_resources *res)
{
	int ret = 0, i, j, num_elements;
	struct platform_device *pdev = res->pdev;
	const char *name;
	u32 count = 0;

	num_elements = of_property_count_strings(pdev->dev.of_node,
			"qcom,enabled-iommu-maps");
	if (num_elements <= 0) {
		pr_warn("No list of IOMMUs to be enabled\n");
		return ret;
	} else if (num_elements > VPU_MAX_IOMMU_DOMAIN) {
		pr_err("List of IOMMUs to enable is too large\n");
		return -EINVAL;
	}

	for (i = 0; i < num_elements; i++) {
		ret = of_property_read_string_index(pdev->dev.of_node,
				"qcom,enabled-iommu-maps", i, &name);
		if (ret)
			return ret;

		for (j = 0; j < VPU_MAX_IOMMU_DOMAIN; j++) {
			if (strcmp(name, iommus_array[j].client_name) == 0) {
				iommus_array[j].enabled = 1;
				count++;
			}
		}
	}

	if (!count) {
		pr_err("No valid IOMMU names specified\n");
		return -EINVAL;
	}

	/* when these values are set, indicates that some IOMMUs are enabled */
	res->iommu_set.count = VPU_MAX_IOMMU_DOMAIN;
	res->iommu_set.iommu_maps = iommus_array;

	return 0;
}

int read_vpu_platform_resources(struct vpu_platform_resources *res,
		struct platform_device *pdev)
{
	int ret = 0;
	struct resource *kres = NULL;
	if (!res || !pdev)
		return -EINVAL;

	res->pdev = pdev;
	if (!pdev->dev.of_node) {
		pr_err("Device Tree node not found\n");
		return -ENOENT;
	}

	kres = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vpu_csr");
	res->register_base_phy = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : 0;

	kres = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vpu_smem");
	res->mem_base_phy = kres ? kres->start : -1;
	res->mem_size = kres ? (kres->end + 1 - kres->start) : 0;

	kres = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vpu_vbif");
	res->vbif_base_phy = kres ? kres->start : -1;
	res->vbif_size = kres ? (kres->end + 1 - kres->start) : 0;

	if (res->register_size == 0 || res->mem_size == 0) {
		pr_err("Failed to read IO memory resources\n");
		return -ENODEV;
	}
	pr_debug("CSR base = 0x%08x, size = 0x%x\n",
			(u32) res->register_base_phy, res->register_size);
	pr_debug("Shared mem base = 0x%08x, size = 0x%x\n",
			(u32) res->mem_base_phy, res->mem_size);
	pr_debug("VBIF base = 0x%08x, size = 0x%x\n",
			(u32) res->vbif_base_phy, res->vbif_size);

	kres = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vpu_wdog");
	res->irq_wd = kres ? kres->start : 0;

	kres = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vpu_hfi");
	res->irq = kres ? kres->start : 0;

	if (res->irq_wd == 0 || res->irq == 0) {
		pr_err("Failed to read IRQ resources\n");
		return -ENODEV;
	}
	pr_debug("Wdog IRQ = %d\n", res->irq_wd);
	pr_debug("IPC IRQ = %d\n", res->irq);

	/*
	 * start devres group.
	 * 'read_vpu_platform_resources' is group identifier
	 */
	if (!devres_open_group(&pdev->dev, read_vpu_platform_resources,
			GFP_KERNEL))
		return -ENOMEM;

	ret = __vpu_load_clk_names(res);
	if (ret) {
		pr_err("Failed to load clock names: %d\n", ret);
		goto err_read_dt_resources;
	}

	ret = __vpu_load_bus_vectors(res);
	if (ret) {
		pr_err("Failed to load bus vectors: %d\n", ret);
		goto err_read_dt_resources;
	}
	ret = __vpu_load_reg_values_table(res, &res->vbif_reg_set,
			"qcom,vbif-reg-presets");
	if (ret) {
		pr_err("Failed to load register values table: %d\n", ret);
		goto err_read_dt_resources;
	}
	ret = __vpu_load_iommu_maps(res);
	if (ret) {
		pr_err("Failed to load iommu maps: %d\n", ret);
		goto err_read_dt_resources;
	}

	/* no errors, close devres group */
	devres_close_group(&pdev->dev, read_vpu_platform_resources);

	return 0;

err_read_dt_resources:
	free_vpu_platform_resources(res);
	return ret;
}

void free_vpu_platform_resources(struct vpu_platform_resources *res)
{
	/* free all allocations in devres group*/
	devres_release_group(&res->pdev->dev, read_vpu_platform_resources);
	memset(res, 0 , sizeof(*res));
}


/*
 * IOMMU memory management
 */
enum iommu_mem_prop {
	MEM_CACHED = ION_FLAG_CACHED,
	MEM_SECURE = ION_FLAG_SECURE,
};

struct vpu_mem_client {
	struct ion_client *clnt;
	struct vpu_platform_resources *res;
};

struct vpu_mem_handle {
	u32 mapped; /* bitmask of mapped devices */
	phys_addr_t device_addr[MEM_MAX_ID]; /* iommu mapped addresses */

	/* iommu domain information (per device) */
	int domain_num[MEM_MAX_ID];

	/* memory buffer information */
	unsigned long size;
	unsigned long flags;
	u32 offset;

	/* memory management objects */
	struct vpu_mem_client *mem_client;
	struct ion_handle *hndl;
};

/* checks that IOMMUs are enabled. Allows debugging with contiguous memory */
static inline int __is_iommu_present(struct vpu_platform_resources *res)
{
	if (res)
		return (res->iommu_set.count > 0 &&
			res->iommu_set.iommu_maps != NULL);
	else
		return 0;
}

/* gets the appropriate VPU domain number */
static int __get_iommu_domain_number(struct vpu_platform_resources *res,
		unsigned long flags, int *domain_num)
{
	struct vpu_iommu_map *iommu_map;
	int i;
	bool is_secure = (flags & MEM_SECURE) ? 1 : 0;

	*domain_num = -1;

	for (i = 0; i < res->iommu_set.count; i++) {
		iommu_map = &res->iommu_set.iommu_maps[i];
		if (iommu_map->enabled && iommu_map->is_secure == is_secure) {
			*domain_num = iommu_map->domain_num;
			return 0;
		}
	}
	return -ENOENT;
}

/* Create ion client */
static void *__vpu_mem_create_client(struct vpu_platform_resources *res)
{
	struct vpu_mem_client *mem_client;
	struct ion_client *ion_client;

	mem_client = kzalloc(sizeof(*mem_client), GFP_KERNEL);
	if (!mem_client) {
		pr_err("memory allocation failed\n");
		return NULL;
	}

	ion_client = msm_ion_client_create(-1, "VPU");
	if (IS_ERR(ion_client)) {
		pr_err("ION client creation failed\n");
		kfree(mem_client);
		return NULL;
	}

	mem_client->res = res;
	mem_client->clnt = ion_client;

	return (void *) mem_client;
}

/* Destroy ion client */
static void __vpu_mem_destroy_client(void *mem_client)
{
	struct vpu_mem_client *client =
			(struct vpu_mem_client *) mem_client;
	if (!client)
		return;

	ion_client_destroy(client->clnt);
	kfree(client);
}

/* Create memory handle. A handle identifies a memory region */
void *vpu_mem_create_handle(void *mem_client)
{
	struct vpu_mem_handle *handle;

	if (!mem_client)
		return NULL;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return NULL;

	handle->mem_client = (struct vpu_mem_client *) mem_client;

	return handle;
}

/* Release memory handle. Any resources assigned to the handle are released */
static void __vpu_mem_release_handle(void *mem_handle, u32 device_id)
{
	struct ion_client *ion_client;
	struct ion_handle *ion_handle;
	struct vpu_mem_handle *handle = (struct vpu_mem_handle *) mem_handle;
	int i = 0, end = 0;
	if (!handle)
		return;

	ion_client = handle->mem_client->clnt;
	ion_handle = handle->hndl;

	if (device_id < MEM_MAX_ID) {
		i = device_id;
		end = i + 1;
	} else {
		i = 0;
		end = MEM_MAX_ID;
	}

	/* loop for all possible devices */
	for (; i < end; i++) {

		if (handle->mapped & (1 << i)) {

			if (__is_iommu_present(handle->mem_client->res))
				ion_unmap_iommu(ion_client, ion_handle,
					handle->domain_num[i], 0);

			if (handle->flags & MEM_SECURE) {
				if (msm_ion_unsecure_buffer(ion_client,
						ion_handle))
					pr_warn("Failed to unsecure memory\n");
			}

			handle->mapped &= ~(1 << i);
			handle->domain_num[i] = -1;
			handle->device_addr[i] = 0;
		}
	}

	if (!handle->mapped) { /* cleared all resources */
		handle->size = 0;
		handle->flags = 0;
		handle->offset = 0;

		if (ion_handle) {
			ion_free(ion_client, ion_handle);
			handle->hndl = 0;
		}
	}
}

/* Map handle memory to given device ID. Considers buffer secure status */
static int __vpu_mem_map_handle(struct vpu_mem_handle *handle, u32 device_id,
		bool secure)
{
	struct ion_client *ion_client;
	struct ion_handle *ion_handle;
	int ret = 0, domain_number;
	unsigned long align = SZ_4K;
	bool secure_flag;

	if (!handle || !handle->hndl || device_id >= MEM_MAX_ID)
		return -EINVAL;

	ion_client = handle->mem_client->clnt;
	ion_handle = handle->hndl;

	ret = ion_handle_get_flags(ion_client, ion_handle, &handle->flags);
	if (ret) {
		pr_err("Failed to get ion flags: %d\n", ret);
		return ret;
	}

	/* check buffer flags */
	secure = secure ? true : false;
	secure_flag = (handle->flags & MEM_SECURE) ? true : false;
	if (secure != secure_flag) {
		pr_err("Buffer CP status does not match request\n");
		return -EINVAL;
	}

	if (__is_iommu_present(handle->mem_client->res)) {

		if (device_id == MEM_VPU_ID) {
			ret = __get_iommu_domain_number(handle->mem_client->res,
				handle->flags, &handle->domain_num[device_id]);
			if (ret) {
				pr_err("Failed to get iommu domain\n");
				return ret;
			}
		}

		domain_number = handle->domain_num[device_id];

		if (handle->flags & MEM_SECURE) { /* handle secure buffers */
			pr_debug("Securing ION buffer\n");
			align = SZ_1M;
			ret = msm_ion_secure_buffer(ion_client, ion_handle,
					VIDEO_PIXEL, 0);
			if (ret) {
				pr_err("Failed to secure memory\n");
				return ret;
			}
		}

		pr_debug("Using IOMMU mapping\n");
		ret = ion_map_iommu(ion_client, ion_handle, domain_number, 0,
				align, 0, &handle->device_addr[device_id],
				&handle->size, 0 , 0);
	} else {
		pr_debug("Using physical mem address\n");
		ret = ion_phys(ion_client, ion_handle,
				&handle->device_addr[device_id],
				(size_t *)&handle->size);
	}

	if (ret) {
		pr_err("Failed to map ion buffer\n");
		if (handle->flags & MEM_SECURE)
			msm_ion_unsecure_buffer(ion_client, ion_handle);
		return ret;
	}

	handle->device_addr[device_id] += handle->offset;
	handle->mapped |= (1 << device_id);

	pr_debug("memory map success. Addr = 0x%08x, size = 0x%08x\n",
		(u32) handle->device_addr[MEM_VPU_ID], (u32) handle->size);

	return 0;
}

int vpu_mem_map_fd(void *mem_handle, int fd, u32 length,
		u32 offset, bool secure)
{
	struct vpu_mem_handle *handle = (struct vpu_mem_handle *) mem_handle;
	struct ion_client *ion_client;
	struct ion_handle *ion_handle;
	int ret = 0;

	if (!handle)
		return -EINVAL;

	if (handle->mapped)
		__vpu_mem_release_handle(mem_handle, -1);

	ion_client = handle->mem_client->clnt;
	ion_handle = ion_import_dma_buf(ion_client, fd);
	if (IS_ERR_OR_NULL(ion_handle)) {
		pr_err("ION import failed with %ld\n", PTR_ERR(ion_handle));
		return -ENOMEM;
	}

	handle->hndl = ion_handle;
	handle->offset = offset;

	ret = __vpu_mem_map_handle(handle, MEM_VPU_ID, secure);
	if (ret) {
		pr_err("iommu memory mapping failed\n");
		goto err_map_handle;
	}

	if (handle->size < length || handle->size < offset ||
			handle->size < (offset + length)) {
		pr_err("mapped buffer is too small\n");
		__vpu_mem_release_handle(mem_handle, -1);
		return -ENOMEM;
	}

	/* length can be smaller than fd size if there is an offset */
	handle->size = length;

	return 0;

err_map_handle:
	ion_free(ion_client, ion_handle);
	handle->hndl = 0;
	handle->offset = 0;
	return ret;
}

int vpu_mem_alloc(void *mem_handle, u32 size, bool secure)
{
	struct vpu_mem_handle *handle = (struct vpu_mem_handle *) mem_handle;
	struct ion_client *ion_client;
	struct ion_handle *ion_handle;
	unsigned long align, flags;
	unsigned long heap_mask;
	int ret = 0;

	if (!handle || !size)
		return -EINVAL;

	if (handle->mapped)
		__vpu_mem_release_handle(mem_handle, -1);

	ion_client = handle->mem_client->clnt;
	flags = secure ? MEM_SECURE : 0;

	if (!__is_iommu_present(handle->mem_client->res)) {
		align = SZ_4K;
		heap_mask = ION_HEAP(ION_SYSTEM_CONTIG_HEAP_ID);
	} else {
		if (flags & MEM_SECURE) {
			heap_mask = ION_HEAP(ION_CP_MM_HEAP_ID);
			align = SZ_1M;
		} else {
			heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID);
			align = SZ_4K;
		}
	}

	ion_handle = ion_alloc(ion_client, ALIGN(size, align), align,
			heap_mask, flags);
	if (IS_ERR_OR_NULL(ion_handle)) {
		pr_err("ION alloc failed with %ld\n", PTR_ERR(ion_handle));
		ret = -ENOMEM;
		goto err_alloc_handle;
	}
	handle->hndl = ion_handle;

	ret = __vpu_mem_map_handle(handle, MEM_VPU_ID, secure);
	if (ret) {
		pr_err("iommu memory mapping failed\n");
		goto err_map_handle;
	}

	pr_debug("ION alloc success\n");
	return 0;

err_map_handle:
	ion_free(ion_client, ion_handle);
	handle->hndl = 0;
err_alloc_handle:
	return ret;
}

int vpu_mem_map_to_device(void *mem_handle, u32 device_id, int domain_num)
{
	struct vpu_mem_handle *handle = (struct vpu_mem_handle *) mem_handle;
	bool secure;

	if (!mem_handle || device_id >= MEM_MAX_ID || domain_num < 0)
		return -EINVAL;

	handle->domain_num[device_id] = domain_num;
	secure = (handle->flags & MEM_SECURE) ? true : false;

	return __vpu_mem_map_handle(handle, device_id, secure);
}

void vpu_mem_unmap_from_device(void *mem_handle, u32 device_id)
{
	if (!mem_handle)
		return;

	__vpu_mem_release_handle(mem_handle, device_id);
}

void vpu_mem_destroy_handle(void *mem_handle)
{
	if (!mem_handle)
		return;

	__vpu_mem_release_handle(mem_handle, -1);
	kfree(mem_handle);
}

phys_addr_t vpu_mem_addr(void *mem_handle, u32 device_id)
{
	struct vpu_mem_handle *handle = (struct vpu_mem_handle *) mem_handle;

	if (!handle || !handle->hndl || device_id >= MEM_MAX_ID)
		return 0;
	else if (!(handle->mapped & (1 << device_id)))
		return 0;
	else
		return handle->device_addr[device_id];
}

u32 vpu_mem_size(void *mem_handle)
{
	struct vpu_mem_handle *handle = (struct vpu_mem_handle *) mem_handle;

	if (!handle || !handle->hndl)
		return 0;
	else
		return handle->size;
}

int register_vpu_iommu_domains(struct vpu_platform_resources *res)
{
	int i = 0, ret = -EINVAL;
	struct vpu_iommu_map *iommu_map;
	struct msm_iova_layout layout;

	if (!res)
		return ret;

	for (i = 0; i < res->iommu_set.count; i++) {
		iommu_map = &res->iommu_set.iommu_maps[i];
		if (!iommu_map->enabled)
			continue;

		memset(&layout, 0, sizeof(layout));
		layout.partitions = iommu_map->partitions;
		layout.npartitions = iommu_map->npartitions;
		layout.client_name = iommu_map->client_name;
		layout.is_secure = iommu_map->is_secure;

		iommu_map->domain_num = msm_register_domain(&layout);
		if (iommu_map->domain_num < 0) {
			pr_err("IOMMU domain %d register fail\n", i);
			goto fail_group;
		}

		iommu_map->domain = msm_get_iommu_domain(iommu_map->domain_num);
		if (!iommu_map->domain) {
			pr_err("Failed to get IOMMU domain %d\n", i);
			goto fail_group;
		}

		iommu_map->ctx = msm_iommu_get_ctx(iommu_map->ctx_name);
		if (IS_ERR_OR_NULL(iommu_map->ctx)) {
			if (PTR_ERR(iommu_map->ctx) == -EPROBE_DEFER) {
				pr_warn("EPROBE_DEFER from iommu_get_ctx: %s\n",
					iommu_map->ctx_name);
				ret = -EPROBE_DEFER;
			} else {
				pr_err("failed iommu_get_ctx: %s\n",
					iommu_map->ctx_name);
			}
			goto fail_group;
		}

		pr_debug("iommu %d: %s domain_number = %d\n",
			i, iommu_map->client_name, iommu_map->domain_num);
	}

	/* Create VPU device iommu (ion) client */
	res->mem_client = __vpu_mem_create_client(res);
	if (!res->mem_client) {
		pr_err("could not create iommu client\n");
		ret = -ENOMEM;
		goto fail_group;
	}

	return 0;

fail_group:
	unregister_vpu_iommu_domains(res);
	return ret;
}

void unregister_vpu_iommu_domains(struct vpu_platform_resources *res)
{
	struct vpu_iommu_map *iommu_map;
	int i = 0;

	if (!res)
		return;

	detach_vpu_iommus(res); /* if not already detached */

	if (res->mem_client) {
		__vpu_mem_destroy_client(res->mem_client);
		res->mem_client = NULL;
	}

	for (i = 0; i < res->iommu_set.count; i++) {
		iommu_map = &res->iommu_set.iommu_maps[i];
		if (iommu_map->domain)
			msm_unregister_domain(iommu_map->domain);
		iommu_map->domain = NULL;
		iommu_map->domain_num = -1;
		iommu_map->ctx = NULL;
	}
}

int attach_vpu_iommus(struct vpu_platform_resources *res)
{
	int i, ret;
	struct vpu_iommu_map *iommu_map;

	if (!res)
		return -EINVAL;
	pr_debug("Enter function\n");

	for (i = 0; i < res->iommu_set.count; i++) {
		iommu_map = &res->iommu_set.iommu_maps[i];
		if (!iommu_map->enabled || iommu_map->attached)
			continue;

		ret = iommu_attach_device(iommu_map->domain, iommu_map->ctx);
		if (ret) {
			pr_err("Failed to attach IOMMU device %s\n",
					iommu_map->ctx_name);
			goto fail_group;
		}
		iommu_map->attached = 1;
	}
	return 0;

fail_group:
	detach_vpu_iommus(res);
	return ret;
}

void detach_vpu_iommus(struct vpu_platform_resources *res)
{
	struct vpu_iommu_map *iommu_map;
	int i = 0;

	if (!res)
		return;
	pr_debug("Enter function\n");

	for (i = 0; i < res->iommu_set.count; i++) {
		iommu_map = &res->iommu_set.iommu_maps[i];
		if (iommu_map->attached)
			iommu_detach_device(iommu_map->domain, iommu_map->ctx);
		iommu_map->attached = 0;
	}
}
