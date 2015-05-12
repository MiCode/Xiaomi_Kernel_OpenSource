/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/msm-bus.h>
#include <asm/dma-iommu.h>
#include <linux/iommu.h>

#include "wil_platform.h"
#include "wil_platform_msm.h"

#define VIRTUAL_ADDR_RANGE_BASE	0x10000000 /* avoid NULL address */
#define VIRTUAL_ADDR_RANGE_SIZE	0x40000000

/**
 * struct wil_platform_data - wil6210 msm platform data
 *
 * @bus_scale: bus scaling information
 * @smmu_support: indication whether PCIe has SMMU support
 */
struct wil_platform_data {
	struct msm_bus_scale_pdata *bus_scale;
	bool smmu_support;
};

/**
 * struct wil_platform_msm - wil6210 msm platform module info
 *
 * @dev: device object
 * @msm_bus_handle: handle for using msm_bus API
 * @pdata: bus scale info retrieved from DT
 * @mapping: the SMMU virtual addresses pool
 */
struct wil_platform_msm {
	struct device *dev;
	uint32_t msm_bus_handle;
	struct wil_platform_data pdata;
	struct dma_iommu_mapping *mapping;
};

#define KBTOB(a) (a * 1000ULL)

/**
 * wil_platform_get_pdata() - Generate platform data from device tree
 * provided by clients.
 *
 * dev: device object
 * of_node: Device tree node to extract information from
 * pdata: Generated device tree information
 *
 * The function parses client's device tree node and provide
 * it back to user.
 * Note that bus_scale is dynamically allocated and it is
 * the caller's responsibility to free it when not needed any longer.
 */
static int wil_platform_get_pdata(
		struct device *dev,
		struct device_node *of_node,
		struct wil_platform_data *pdata)
{
	struct msm_bus_paths *usecase;
	int i, j, ret, len;
	unsigned int num_usecases, num_paths, mem_size;
	const uint32_t *vec_arr;
	struct msm_bus_vectors *vectors;

	/* first read num_usecases and num_paths so we can calculate
	 * amount of memory to allocate
	 */
	ret = of_property_read_u32(of_node, "qcom,msm-bus,num-cases",
				   &num_usecases);
	if (ret) {
		dev_err(dev, "Error: num-usecases not found\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(of_node, "qcom,msm-bus,num-paths",
				   &num_paths);
	if (ret) {
		dev_err(dev, "Error: num_paths not found\n");
		return -EINVAL;
	}

	/* pdata->bus_scale memory layout:
	 *   msm_bus_scale_pdata
	 *   msm_bus_paths[num_usecases]
	 *   msm_bus_vectors[num_usecases][num_paths]
	 */
	mem_size = sizeof(struct msm_bus_scale_pdata) +
		   sizeof(struct msm_bus_paths) * num_usecases +
		   sizeof(struct msm_bus_vectors) * num_usecases * num_paths;

	pdata->bus_scale = kzalloc(mem_size, GFP_KERNEL);
	if (!pdata->bus_scale)
		return -ENOMEM;

	ret = of_property_read_string(of_node, "qcom,msm-bus,name",
				      &pdata->bus_scale->name);
	if (ret) {
		dev_err(dev, "Error: Client name not found\n");
		goto err;
	}

	if (of_property_read_bool(of_node, "qcom,msm-bus,active-only")) {
		pdata->bus_scale->active_only = 1;
	} else {
		dev_info(dev, "active_only flag absent.\n");
		dev_info(dev, "Using dual context by default\n");
	}

	pdata->bus_scale->num_usecases = num_usecases;
	pdata->bus_scale->usecase =
		(struct msm_bus_paths *)(pdata->bus_scale + 1);

	vec_arr = of_get_property(of_node, "qcom,msm-bus,vectors-KBps", &len);
	if (vec_arr == NULL) {
		dev_err(dev, "Error: Vector array not found\n");
		ret = -ENOMEM;
		goto err;
	}

	if (len != num_usecases * num_paths * sizeof(uint32_t) * 4) {
		dev_err(dev, "Error: Length-error on getting vectors\n");
		ret = -EINVAL;
		goto err;
	}

	vectors = (struct msm_bus_vectors *)
		(pdata->bus_scale->usecase + num_usecases);
	for (i = 0; i < num_usecases; i++) {
		usecase = &pdata->bus_scale->usecase[i];
		usecase->num_paths = num_paths;
		usecase->vectors = &vectors[i];

		for (j = 0; j < num_paths; j++) {
			int index = ((i * num_paths) + j) * 4;

			usecase->vectors[j].src = be32_to_cpu(vec_arr[index]);
			usecase->vectors[j].dst =
				be32_to_cpu(vec_arr[index + 1]);
			usecase->vectors[j].ab = (uint64_t)
				KBTOB(be32_to_cpu(vec_arr[index + 2]));
			usecase->vectors[j].ib = (uint64_t)
				KBTOB(be32_to_cpu(vec_arr[index + 3]));
		}
	}

	if (of_property_read_bool(of_node, "qcom,smmu-support")) {
		pdata->smmu_support = 1;
	} else {
		dev_info(dev, "smmu_support flag absent.\n");
		pdata->smmu_support = 0;
	}

	return 0;

err:
	kfree(pdata->bus_scale);
	return ret;
}

/* wil_platform API (callbacks) */

static int wil_platform_bus_request(void *handle,
				    uint32_t kbps /* KBytes/Sec */)
{
	int rc, i;
	struct wil_platform_msm *msm = (struct wil_platform_msm *)handle;
	int vote = 0; /* vote 0 in case requested kbps cannot be satisfied */
	struct msm_bus_paths *usecase;
	uint32_t usecase_kbps;
	uint32_t min_kbps = ~0;

	/* find the lowest usecase that is bigger than requested kbps */
	for (i = 0; i < msm->pdata.bus_scale->num_usecases; i++) {
		usecase = &msm->pdata.bus_scale->usecase[i];
		/* assume we have single path (vectors[0]). If we ever
		 * have multiple paths, need to define the behavior */
		usecase_kbps = div64_u64(usecase->vectors[0].ib, 1000);
		if (usecase_kbps >= kbps && usecase_kbps < min_kbps) {
			min_kbps = usecase_kbps;
			vote = i;
		}
	}

	rc = msm_bus_scale_client_update_request(msm->msm_bus_handle, vote);
	if (rc)
		dev_err(msm->dev, "Failed msm_bus voting. kbps=%d vote=%d, rc=%d\n",
			kbps, vote, rc);
	else
		/* TOOD: remove */
		dev_info(msm->dev, "msm_bus_scale_client_update_request succeeded. kbps=%d vote=%d\n",
			 kbps, vote);

	return rc;
}

static void wil_platform_uninit(void *handle)
{
	struct wil_platform_msm *msm = (struct wil_platform_msm *)handle;

	dev_info(msm->dev, "wil_platform_uninit\n");

	if (msm->msm_bus_handle)
		msm_bus_scale_unregister_client(msm->msm_bus_handle);

	if (msm->pdata.smmu_support) {
		arm_iommu_detach_device(msm->dev);
		arm_iommu_release_mapping(msm->mapping);
	}

	kfree(msm->pdata.bus_scale);
	kfree(msm);
}

static int wil_platform_msm_bus_register(struct wil_platform_msm *msm)
{
	msm->msm_bus_handle =
		msm_bus_scale_register_client(msm->pdata.bus_scale);
	if (!msm->msm_bus_handle) {
		dev_err(msm->dev, "Failed msm_bus registration\n");
		return -EINVAL;
	}

	dev_info(msm->dev, "msm_bus registration succeeded! handle 0x%x\n",
		 msm->msm_bus_handle);

	return 0;
}

/**
 * wil_platform_msm_init() - wil6210 msm platform module init
 *
 * The function must be called before all other functions in this module.
 * It returns a handle which is used with the rest of the API
 *
 */
void *wil_platform_msm_init(struct device *dev, struct wil_platform_ops *ops)
{
	struct device_node *of_node;
	struct wil_platform_msm *msm;
	int disable_htw = 1;
	dma_addr_t base = VIRTUAL_ADDR_RANGE_BASE;
	size_t size = VIRTUAL_ADDR_RANGE_SIZE;
	int order = 0;
	int rc;

	memset(ops, 0, sizeof(*ops));

	of_node = of_find_compatible_node(NULL, NULL, "qcom,wil6210");
	if (!of_node) {
		/* this could mean non-msm platform */
		dev_err(dev, "DT node not found\n");
		return NULL;
	}

	msm = kzalloc(sizeof(*msm), GFP_KERNEL);
	if (!msm)
		return NULL;

	msm->dev = dev;

	rc = wil_platform_get_pdata(msm->dev, of_node, &msm->pdata);
	if (rc) {
		dev_err(msm->dev, "Failed getting DT info (%d)\n", rc);
		goto free_mem;
	}

	/* register with msm_bus module for scaling requests */
	rc = wil_platform_msm_bus_register(msm);
	if (rc)
		goto free_mem;

	if (!msm->pdata.smmu_support)
		return (void *)msm;

	msm->mapping = arm_iommu_create_mapping(&platform_bus_type,
		base, size, order);

	if (IS_ERR_OR_NULL(msm->mapping)) {
		rc = PTR_ERR(msm->mapping) ?: -ENODEV;
		dev_err(dev, "Failed to create IOMMU mapping (%d)\n", rc);
		goto free_mem;
	}

	rc = iommu_domain_set_attr(msm->mapping->domain,
			DOMAIN_ATTR_COHERENT_HTW_DISABLE,
			&disable_htw);
	if (rc) {
		/* This error can be ignored and not considered fatal,
		 * but let the users know this happened
		 */
		dev_err(dev, "Warning: disable coherent HTW failed (%d)\n", rc);
	}

	rc = arm_iommu_attach_device(dev, msm->mapping);
	if (rc) {
		dev_err(dev, "arm_iommu_attach_device failed (%d)\n", rc);
		goto release_mapping;
	}

	ops->bus_request = wil_platform_bus_request;
	ops->uninit = wil_platform_uninit;

	return (void *)msm;

release_mapping:
	arm_iommu_release_mapping(msm->mapping);
free_mem:
	kfree(msm);
	return NULL;
}
