/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/subsystem_restart.h>
#include <mach/qdsp6v2/apr.h>
#include <linux/of_device.h>
#include <linux/msm_audio_ion.h>

struct msm_audio_ion_private {
	bool smmu_enabled;
	/*u32 group_id;*/
	/*u32 domain_id;*/
};

static struct msm_audio_ion_private msm_audio_ion_data = {0,};


static int msm_audio_ion_get_phys(struct ion_client *client,
				  struct ion_handle *handle,
				  ion_phys_addr_t *addr, size_t *len);



int msm_audio_ion_alloc(const char *name, struct ion_client **client,
			struct ion_handle **handle, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = 0;

	*client = msm_audio_ion_client_create(UINT_MAX, name);
	if (IS_ERR_OR_NULL((void *)(*client))) {
		pr_err("%s: ION create client for AUDIO failed\n", __func__);
		goto err;
	}

	*handle = ion_alloc(*client, bufsz, SZ_4K, (0x1<<ION_AUDIO_HEAP_ID), 0);
	if (IS_ERR_OR_NULL((void *) (*handle))) {
		pr_err("%s: ION memory allocation for AUDIO failed\n",
			__func__);
		goto err_ion_client;
	}

	rc = msm_audio_ion_get_phys(*client, *handle, paddr, pa_len);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		goto err_ion_handle;
	}

	/*Need to add condition SMMU enable or not */
	*vaddr = ion_map_kernel(*client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		goto err_ion_handle;
	}

	if (bufsz != 0)
		memset((void *)*vaddr, 0, bufsz);

	return 0;

err_ion_handle:
	ion_free(*client, *handle);
err_ion_client:
	msm_audio_ion_client_destroy(*client);
err:
	return -EINVAL;

}

int msm_audio_ion_import(const char *name, struct ion_client **client,
			struct ion_handle **handle, int fd,
			unsigned long *ionflag, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = 0;

	*client = msm_audio_ion_client_create(UINT_MAX, name);
	if (IS_ERR_OR_NULL((void *)(*client))) {
		pr_err("%s: ION create client for AUDIO failed\n", __func__);
		goto err;
	}

	/* name should be audio_acdb_client or Audio_Dec_Client,
	bufsz should be 0 and fd shouldn't be 0 as of now
	*/
	*handle = ion_import_dma_buf(*client, fd);
	pr_err("%s: DMA Buf name=%s, fd=%d handle=%p\n", __func__,
							name, fd, *handle);
	if (IS_ERR_OR_NULL((void *) (*handle))) {
		pr_err("%s: ion import dma buffer failed\n",
				__func__);
		goto err_ion_handle;
		}

	if (ionflag != NULL) {
		rc = ion_handle_get_flags(*client, *handle, ionflag);
		if (rc) {
			pr_err("%s: could not get flags for the handle\n",
				__func__);
			goto err_ion_handle;
		}
	}

	rc = msm_audio_ion_get_phys(*client, *handle, paddr, pa_len);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
				__func__, rc);
		goto err_ion_handle;
	}

	/*Need to add condition SMMU enable or not */
	*vaddr = ion_map_kernel(*client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		goto err_ion_handle;
	}

	if (bufsz != 0)
		memset((void *)*vaddr, 0, bufsz);

	return 0;

err_ion_handle:
	ion_free(*client, *handle);
	msm_audio_ion_client_destroy(*client);
err:
	return -EINVAL;

}

int msm_audio_ion_free(struct ion_client *client, struct ion_handle *handle)
{
	/* add condition for SMMU enabled */
	ion_unmap_kernel(client, handle);

	ion_free(client, handle);
	msm_audio_ion_client_destroy(client);
	return 0;
}


bool msm_audio_ion_is_smmu_available(void)
{
	return msm_audio_ion_data.smmu_enabled;
}

/* move to static section again */
struct ion_client *msm_audio_ion_client_create(unsigned int heap_mask,
					const char *name)
{
	pr_debug("%s: smmu_enabled = %d\n", __func__,
					msm_audio_ion_data.smmu_enabled);


	return msm_ion_client_create(heap_mask, name);
}


void msm_audio_ion_client_destroy(struct ion_client *client)
{
	pr_debug("%s: smmu_enabled = %d\n", __func__,
		msm_audio_ion_data.smmu_enabled);

	ion_client_destroy(client);
}

int msm_audio_ion_import_legacy(const char *name, struct ion_client *client,
			struct ion_handle **handle, int fd,
			unsigned long *ionflag, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = 0;
	/* client is already created for legacy and given*/
	/* name should be audio_acdb_client or Audio_Dec_Client,
	bufsz should be 0 and fd shouldn't be 0 as of now
	*/
	*handle = ion_import_dma_buf(client, fd);
	pr_err("%s: DMA Buf name=%s, fd=%d handle=%p\n", __func__,
							name, fd, *handle);
	if (IS_ERR_OR_NULL((void *) (*handle))) {
		pr_err("%s: ion import dma buffer failed\n",
			__func__);
		goto err_ion_handle;
		}

	if (ionflag != NULL) {
		rc = ion_handle_get_flags(client, *handle, ionflag);
		if (rc) {
			pr_err("%s: could not get flags for the handle\n",
							__func__);
			goto err_ion_handle;
		}
	}

	rc = msm_audio_ion_get_phys(client, *handle, paddr, pa_len);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		goto err_ion_handle;
	}

	/*Need to add condition SMMU enable or not */
	*vaddr = ion_map_kernel(client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		goto err_ion_handle;
	}

	if (bufsz != 0)
		memset((void *)*vaddr, 0, bufsz);

	return 0;

err_ion_handle:
	ion_free(client, *handle);
	return -EINVAL;

}

int msm_audio_ion_free_legacy(struct ion_client *client,
			      struct ion_handle *handle)
{
	/* To add condition for SMMU enabled */
	ion_unmap_kernel(client, handle);

	ion_free(client, handle);
	/* no client_destrody in legacy*/
	return 0;
}


static int msm_audio_ion_get_phys(struct ion_client *client,
		struct ion_handle *handle,
		ion_phys_addr_t *addr, size_t *len)
{
	int rc = 0;
	pr_debug("%s: smmu_enabled = %d\n", __func__,
		msm_audio_ion_data.smmu_enabled);

	if (msm_audio_ion_data.smmu_enabled) {
		/* SMMU enabled case ion_map_iommu()*/
	} else {
		/* SMMU is disabled*/
		rc = ion_phys(client, handle, addr, len);
	}
	pr_debug("%s: addr= 0x%p, len= %d\n", __func__, addr, *len);
	return rc;
}




static int msm_audio_ion_probe(struct platform_device *pdev)
{
	int rc = 0;
	const char *msm_audio_ion_dt = "qcom,smmu-enabled";
	bool smmu_enabled;

	if (pdev->dev.of_node == NULL) {
		pr_err("%s: device tree is not found\n", __func__);
		msm_audio_ion_data.smmu_enabled = 0;
		return 0;
	}

	smmu_enabled = of_property_read_bool(pdev->dev.of_node,
					     msm_audio_ion_dt);
	msm_audio_ion_data.smmu_enabled = smmu_enabled;

	pr_debug("%s: SMMU-Enabled = %d\n", __func__, smmu_enabled);
	return rc;
}

static int msm_audio_ion_remove(struct platform_device *pdev)
{
	pr_debug("%s: msm audio ion is unloaded\n", __func__);

	return 0;
}

static const struct of_device_id msm_audio_ion_dt_match[] = {
	{ .compatible = "qcom,msm-audio-ion" },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_audio_ion_dt_match);

static struct platform_driver msm_audio_ion_driver = {
	.driver = {
		.name = "msm-audio-ion",
		.owner = THIS_MODULE,
		.of_match_table = msm_audio_ion_dt_match,
	},
	.probe = msm_audio_ion_probe,
	.remove = __devexit_p(msm_audio_ion_remove),
};

static int __init msm_audio_ion_init(void)
{
	return platform_driver_register(&msm_audio_ion_driver);
}
module_init(msm_audio_ion_init);

static void __exit msm_audio_ion_exit(void)
{
	platform_driver_unregister(&msm_audio_ion_driver);
}
module_exit(msm_audio_ion_exit);

MODULE_DESCRIPTION("MSM Audio ION module");
MODULE_LICENSE("GPL v2");
