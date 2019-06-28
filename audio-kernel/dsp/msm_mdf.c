/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/ion_kernel.h>
#include <linux/msm_ion.h>
#include <dsp/msm_audio_ion.h>
#include <ipc/apr.h>
#include <dsp/msm_mdf.h>
#include <asm/dma-iommu.h>
#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/scm.h>
#include <dsp/q6audio-v2.h>
#include <dsp/q6core.h>
#include <asm/cacheflush.h>

#define VMID_SSC_Q6     5
#define VMID_LPASS      6
#define VMID_MSS_MSA    15
#define VMID_CDSP       30

#define MSM_MDF_PROBED         (1 << 0)
#define MSM_MDF_INITIALIZED    (1 << 1)
#define MSM_MDF_MEM_ALLOCATED  (1 << 2)
#define MSM_MDF_MEM_MAPPED     (1 << 3)
#define MSM_MDF_MEM_PERMISSION (1 << 4) /* 0 - HLOS, 1 - Subsys */

/* TODO: Update IOVA range for subsys SMMUs */
#define MSM_MDF_IOVA_START 0x80000000
#define MSM_MDF_IOVA_LEN 0x800000

#define MSM_MDF_SMMU_SID_OFFSET 32

#define ADSP_STATE_READY_TIMEOUT_MS 3000

/* mem protection defines */
#define TZ_MPU_LOCK_NS_REGION 0x00000025
#define MEM_PROTECT_AC_PERM_READ 0x4
#define MEM_PROTECT_AC_PERM_WRITE 0x2

#define MSM_AUDIO_SMMU_SID_OFFSET 32

enum {
	SUBSYS_ADSP, /* Audio DSP must have index 0 */
	SUBSYS_SCC,  /* Sensor DSP */
	SUBSYS_MSS,  /* Modem DSP */
	SUBSYS_CDSP, /* Compute DSP */
	SUBSYS_MAX,
};

struct msm_mdf_dest_vm_and_perm_info {
	uint32_t dst_vm;
	/* Destination VM defined by ACVirtualMachineId. */
	uint32_t dst_vm_perm;
	/* Permissions of the IPA to be mapped to VM, bitwise OR of AC_PERM. */
	uint64_t ctx;
	/* Destination of the VM-specific context information. */
	uint32_t ctx_size;
	/* Size of context buffer in bytes. */
};

struct msm_mdf_protect_mem {
	uint64_t dma_start_address;
	uint64_t dma_end_address;
	struct msm_mdf_dest_vm_and_perm_info dest_info[SUBSYS_MAX];
	uint32_t dest_info_size;
};

struct msm_mdf_mem {
	struct device *dev;
	uint8_t device_status;
	uint32_t map_handle;
	struct dma_buf *dma_buf;
	dma_addr_t dma_addr;
	size_t size;
	void *va;
};

static struct msm_mdf_mem mdf_mem_data = {NULL,};

struct msm_mdf_smmu {
	bool enabled;
	char *subsys;
	int vmid;
	uint32_t proc_id;
	struct device *cb_dev;
	uint8_t device_status;
	uint64_t sid;
	struct dma_iommu_mapping *mapping;
	dma_addr_t pa;
	size_t pa_len;
};

static struct msm_mdf_smmu mdf_smmu_data[SUBSYS_MAX] = {
	{
		.subsys = "adsp",
		.vmid = VMID_LPASS,
	},
	{
		.subsys = "dsps",
		.vmid = VMID_SSC_Q6,
		.proc_id = AVS_MDF_SSC_PROC_ID,
	},
	{
		.subsys = "modem",
		.vmid = VMID_MSS_MSA,
		.proc_id = AVS_MDF_MDSP_PROC_ID,
	},
	{
		.subsys = "cdsp",
		.vmid = VMID_CDSP,
		.proc_id = AVS_MDF_CDSP_PROC_ID,
	},
};

static void *ssr_handle;

static inline uint64_t buf_page_start(uint64_t buf)
{
	uint64_t start = (uint64_t) buf & PAGE_MASK;
	return start;
}

static inline uint64_t buf_page_offset(uint64_t buf)
{
	uint64_t offset = (uint64_t) buf & (PAGE_SIZE - 1);
	return offset;
}

static inline int buf_num_pages(uint64_t buf, ssize_t len)
{
	uint64_t start = buf_page_start(buf) >> PAGE_SHIFT;
	uint64_t end = (((uint64_t) buf + len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	int nPages = end - start + 1;
	return nPages;
}

static inline uint64_t buf_page_size(uint32_t size)
{
	uint64_t sz = (size + (PAGE_SIZE - 1)) & PAGE_MASK;

	return sz > PAGE_SIZE ? sz : PAGE_SIZE;
}

static inline void *uint64_to_ptr(uint64_t addr)
{
	void *ptr = (void *)((uintptr_t)addr);
	return ptr;
}

static inline uint64_t ptr_to_uint64(void *ptr)
{
	uint64_t addr = (uint64_t)((uintptr_t)ptr);
	return addr;
}

static int msm_mdf_dma_buf_map(struct msm_mdf_mem *mem,
			       struct msm_mdf_smmu *smmu)
{
	int rc = 0;

	if (!smmu)
		return -EINVAL;
	if (smmu->device_status & MSM_MDF_MEM_MAPPED)
		return 0;
	if (smmu->enabled) {
		if (smmu->cb_dev == NULL) {
			pr_err("%s: cb device is not initialized\n",
				__func__);
			/* Retry if LPASS cb device is not ready
			 * from audio ION during probing.
			 */
			if (!strcmp("adsp", smmu->subsys)) {
				rc = msm_audio_ion_get_smmu_info(&smmu->cb_dev,
						&smmu->sid);
				if (rc) {
					pr_err("%s: msm_audio_ion_get_smmu_info failed, rc = %d\n",
						__func__, rc);
					goto err;
				}
			} else
				return -ENODEV;
		}

		smmu->pa = dma_map_single_attrs(smmu->cb_dev, mem->va,
			mem->size, DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
		if (dma_mapping_error(smmu->cb_dev, smmu->pa)) {
			rc = -ENOMEM;
			pr_err("%s: failed to map single, rc = %d\n",
				__func__, rc);
			goto err;
		}
		smmu->pa_len = mem->size;

		/* Append the SMMU SID information to the IOVA address */
		if (smmu->sid)
			smmu->pa |= smmu->sid;
	} else {
		smmu->pa = mem->dma_addr;
		smmu->pa_len = mem->size;
	}
	pr_err("%s: pa=%pa, pa_len=%zd\n", __func__,
		&smmu->pa, smmu->pa_len);

	smmu->device_status |= MSM_MDF_MEM_MAPPED;

	return 0;
err:
	return rc;
}

static int msm_mdf_alloc_dma_buf(struct msm_mdf_mem *mem)
{
	int rc = 0;

	if (!mem)
		return -EINVAL;

	if (mem->device_status & MSM_MDF_MEM_ALLOCATED)
		return 0;

	if (mem->dev == NULL) {
		pr_err("%s: device is not initialized\n",
		__func__);
		return -ENODEV;
	}

	mem->va = dma_alloc_attrs(mem->dev, mem->size,
			&mem->dma_addr, GFP_KERNEL, DMA_ATTR_NO_KERNEL_MAPPING);
	if (IS_ERR_OR_NULL(mem->va)) {
		pr_err("%s: failed to allocate dma memory, rc = %d\n",
			__func__, rc);
		return -ENOMEM;
	}
	mem->va = phys_to_virt(mem->dma_addr);
	mem->device_status |= MSM_MDF_MEM_ALLOCATED;
	return rc;
}

static int msm_mdf_free_dma_buf(struct msm_mdf_mem *mem)
{
	if (!mem)
		return -EINVAL;

	if (mem->dev == NULL) {
		pr_err("%s: device is not initialized\n",
		__func__);
		return -ENODEV;
	}

	//dma_free_coherent(mem->dev, mem->size, mem->va,
	//				  mem->dma_addr);

	mem->device_status &= ~MSM_MDF_MEM_ALLOCATED;
	return 0;
}

static int msm_mdf_dma_buf_unmap(struct msm_mdf_mem *mem,
				 struct msm_mdf_smmu *smmu)
{
	if (!smmu)
		return -EINVAL;

	if (smmu->enabled) {
		if (smmu->cb_dev == NULL) {
			pr_err("%s: cb device is not initialized\n",
				__func__);
			return -ENODEV;
		}
		//if (smmu->pa && mem->size)
			//dma_unmap_single(smmu->cb_dev, smmu->pa,
			//		 mem->size, DMA_BIDIRECTIONAL);
	}

	smmu->device_status &= ~MSM_MDF_MEM_MAPPED;

	return 0;
}

static int msm_mdf_map_memory_to_subsys(struct msm_mdf_mem *mem,
				struct msm_mdf_smmu *smmu)
{
	int rc = 0;

	if (!mem || !smmu)
		return -EINVAL;

	/* Map mdf shared memory to ADSP */
	if (!strcmp("adsp", smmu->subsys)) {
		rc = q6core_map_memory_regions((phys_addr_t *)&smmu->pa,
				ADSP_MEMORY_MAP_MDF_SHMEM_4K_POOL,
				(uint32_t *)&smmu->pa_len, 1, &mem->map_handle);
		if (rc)  {
			pr_err("%s: q6core_map_memory_regions failed, rc = %d\n",
				__func__, rc);
		}
	} else {
		if (mem->map_handle) {
			/* Map mdf shared memory to remote DSPs */
			rc = q6core_map_mdf_shared_memory(mem->map_handle,
					(phys_addr_t *)&smmu->pa, smmu->proc_id,
					(uint32_t *)&smmu->pa_len, 1);
			if (rc)  {
				pr_err("%s: q6core_map_mdf_shared_memory failed, rc = %d\n",
					__func__, rc);
			}
		}
	}
	return rc;
}

static void msm_mdf_unmap_memory_to_subsys(struct msm_mdf_mem *mem,
				struct msm_mdf_smmu *smmu)
{
	if (!mem || !smmu)
		return;

	if (!strcmp("adsp", smmu->subsys)) {
		if (mem->map_handle)
			q6core_memory_unmap_regions(mem->map_handle);
	}
}

static int msm_mdf_assign_memory_to_subsys(struct msm_mdf_mem *mem)
{
	int ret = 0, i;
	struct scm_desc desc = {0};
	struct msm_mdf_protect_mem *scm_buffer;
	uint32_t fnid;

	scm_buffer = kzalloc(sizeof(struct msm_mdf_protect_mem), GFP_KERNEL);
	if (!scm_buffer)
		return -ENOMEM;

	scm_buffer->dma_start_address = mem->dma_addr;
	scm_buffer->dma_end_address = mem->dma_addr + buf_page_size(mem->size);
	for (i = 0; i < SUBSYS_MAX; i++) {
		scm_buffer->dest_info[i].dst_vm = mdf_smmu_data[i].vmid;
		scm_buffer->dest_info[i].dst_vm_perm =
			MEM_PROTECT_AC_PERM_READ | MEM_PROTECT_AC_PERM_WRITE;
		scm_buffer->dest_info[i].ctx = 0;
		scm_buffer->dest_info[i].ctx_size = 0;
	}
	scm_buffer->dest_info_size =
		sizeof(struct msm_mdf_dest_vm_and_perm_info) * SUBSYS_MAX;

	/* flush cache required by scm_call2 */
	dmac_flush_range(scm_buffer, ((void *)scm_buffer) +
					sizeof(struct msm_mdf_protect_mem));

	desc.args[0] = scm_buffer->dma_start_address;
	desc.args[1] = scm_buffer->dma_end_address;
	desc.args[2] = virt_to_phys(&(scm_buffer->dest_info[0]));
	desc.args[3] = scm_buffer->dest_info_size;

	desc.arginfo = SCM_ARGS(4, SCM_VAL, SCM_VAL, SCM_RO, SCM_VAL);

	fnid = SCM_SIP_FNID(SCM_SVC_MP, TZ_MPU_LOCK_NS_REGION);
	ret = scm_call2(fnid, &desc);
	if (ret < 0) {
		pr_err("%s: SCM call2 failed, ret %d scm_resp %llu\n",
			__func__, ret, desc.ret[0]);
	}
	/* No More need for scm_buffer, freeing the same */
	kfree(scm_buffer);
	return ret;
}

/**
 * msm_mdf_mem_init - Initializes MDF memory pool and
 * map memory to subsystem
 *
 * Returns 0 on success or ret on failure.
 */

int msm_mdf_mem_init(void)
{
	int rc = 0, i, j;
	struct msm_mdf_mem *mem = &mdf_mem_data;
	struct msm_mdf_smmu *smmu;
	unsigned long timeout = jiffies +
		msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
	int adsp_ready = 0;

	if (!(mdf_mem_data.device_status & MSM_MDF_PROBED))
		return -ENODEV;

	if (mdf_mem_data.device_status & MSM_MDF_INITIALIZED)
		return 0;

	/* TODO: pulling may not be needed as Q6 Core state should be
	 * checked during machine driver probing.
	 */
	do {
		if (!q6core_is_adsp_ready()) {
			pr_err("%s: ADSP Audio NOT Ready\n",
				__func__);
			/* ADSP will be coming up after subsystem restart and
			 * it might not be fully up when the control reaches
			 * here. So, wait for 50msec before checking ADSP state
			 */
			msleep(50);
		} else {
			pr_debug("%s: ADSP Audio Ready\n",
					__func__);
			adsp_ready = 1;
			break;
		}
	} while (time_after(timeout, jiffies));

	if (!adsp_ready) {
		pr_err("%s: timed out waiting for ADSP Audio\n",
			__func__);
		return -ETIMEDOUT;
	}

	if (mem->device_status & MSM_MDF_MEM_ALLOCATED) {
		for (i = 0; i < SUBSYS_MAX; i++) {
			smmu = &mdf_smmu_data[i];
			rc = msm_mdf_dma_buf_map(mem, smmu);
			if (rc) {
				pr_err("%s: msm_mdf_dma_buf_map failed, rc = %d\n",
					__func__, rc);
				goto err;
			}
		}

		rc = msm_mdf_assign_memory_to_subsys(mem);
		if (rc) {
			pr_err("%s: msm_mdf_assign_memory_to_subsys failed\n",
			__func__);
			goto err;
		}

		for (j = 0; j < SUBSYS_MAX; j++) {
			smmu = &mdf_smmu_data[j];
			rc = msm_mdf_map_memory_to_subsys(mem, smmu);
			if (rc) {
				pr_err("%s: msm_mdf_map_memory_to_subsys failed\n",
					__func__);
				goto err;
			}
		}

		mdf_mem_data.device_status |= MSM_MDF_INITIALIZED;
	}
	return 0;
err:
	return rc;
}
EXPORT_SYMBOL(msm_mdf_mem_init);

int msm_mdf_mem_deinit(void)
{
	int rc = 0, i;
	struct msm_mdf_mem *mem = &mdf_mem_data;
	struct msm_mdf_smmu *smmu;

	if (!(mdf_mem_data.device_status & MSM_MDF_INITIALIZED))
		return -ENODEV;

	for (i = SUBSYS_MAX - 1; i >= 0; i--) {
		smmu = &mdf_smmu_data[i];
		msm_mdf_unmap_memory_to_subsys(mem, smmu);
	}

	if (!rc) {
		for (i = SUBSYS_MAX - 1; i >= 0; i--) {
			smmu = &mdf_smmu_data[i];
			msm_mdf_dma_buf_unmap(mem, smmu);
		}

		msm_mdf_free_dma_buf(mem);
		mem->device_status &= ~MSM_MDF_MEM_ALLOCATED;
	}

	mdf_mem_data.device_status &= ~MSM_MDF_INITIALIZED;

	return 0;
}
EXPORT_SYMBOL(msm_mdf_mem_deinit);

static int msm_mdf_restart_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *_cmd)
{
	static int boot_count = 3;

	/* During LPASS boot, HLOS receives events:
	 *  SUBSYS_BEFORE_POWERUP
	 *  SUBSYS_PROXY_VOTE
	 *  SUBSYS_AFTER_POWERUP - need skip
	 *  SUBSYS_PROXY_UNVOTE
	 */
	if (boot_count) {
		boot_count--;
		return NOTIFY_OK;
	}

	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("Subsys Notify: Shutdown Started\n");
		/* Unmap and free memory upon restart event. */
		msm_mdf_mem_deinit();
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		pr_debug("Subsys Notify: Shutdown Completed\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		pr_debug("Subsys Notify: Bootup Started\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		pr_debug("Subsys Notify: Bootup Completed\n");
		/* Allocate and map memory after restart complete. */
		if (msm_mdf_mem_init())
			pr_err("msm_mdf_mem_init failed\n");
		break;
	default:
		pr_err("Subsys Notify: Generel: %lu\n", code);
		break;
	}
	return NOTIFY_DONE;
}

static const struct of_device_id msm_mdf_match_table[] = {
	{ .compatible = "qcom,msm-mdf", },
	{ .compatible = "qcom,msm-mdf-mem-region", },
	{ .compatible = "qcom,msm-mdf-cb", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_mdf_match_table);

static int msm_mdf_cb_probe(struct device *dev)
{
	struct msm_mdf_smmu *smmu;
	u64 smmu_sid = 0;
	u64 smmu_sid_mask = 0;
	struct of_phandle_args iommuspec;
	const char *subsys;
	int rc = 0, i;

	subsys = of_get_property(dev->of_node, "label", NULL);
	if (!subsys) {
		dev_err(dev, "%s: could not get label\n",
			__func__);
		return -EINVAL;
	}

	for (i = 0; i < SUBSYS_MAX; i++) {
		if (!mdf_smmu_data[i].subsys)
			continue;
		if (!strcmp(subsys, mdf_smmu_data[i].subsys))
			break;
	}
	if (i >= SUBSYS_MAX) {
		dev_err(dev, "%s: subsys %s not supported\n",
			__func__, subsys);
		return -EINVAL;
	}

	smmu = &mdf_smmu_data[i];

	smmu->enabled = of_property_read_bool(dev->of_node,
						"qcom,smmu-enabled");

	dev_info(dev, "%s: SMMU is %s for %s\n", __func__,
		(smmu->enabled) ? "enabled" : "disabled",
		smmu->subsys);

	if (smmu->enabled) {
		/* Get SMMU SID information from Devicetree */
		rc = of_property_read_u64(dev->of_node,
					"qcom,smmu-sid-mask",
					&smmu_sid_mask);
		if (rc) {
			dev_err(dev,
				"%s: qcom,smmu-sid-mask missing in DT node, using default\n",
				__func__);
			smmu_sid_mask = 0xFFFFFFFFFFFFFFFF;
		}

		rc = of_parse_phandle_with_args(dev->of_node, "iommus",
					"#iommu-cells", 0, &iommuspec);
		if (rc)
			dev_err(dev, "%s: could not get smmu SID, ret = %d\n",
				__func__, rc);
		else
			smmu_sid = (iommuspec.args[0] & smmu_sid_mask);

		smmu->sid =
			smmu_sid << MSM_AUDIO_SMMU_SID_OFFSET;

		smmu->cb_dev = dev;
	}
	return 0;
}

static int msm_mdf_remove(struct platform_device *pdev)
{
	int rc = 0, i;

	for (i = 0; i < SUBSYS_MAX; i++) {
		if (!IS_ERR_OR_NULL(mdf_smmu_data[i].cb_dev))
			arm_iommu_detach_device(mdf_smmu_data[i].cb_dev);
		if (!IS_ERR_OR_NULL(mdf_smmu_data[i].mapping))
			arm_iommu_release_mapping(mdf_smmu_data[i].mapping);
		mdf_smmu_data[i].enabled = 0;
	}
	mdf_mem_data.device_status = 0;

	return rc;
}

static int msm_mdf_probe(struct platform_device *pdev)
{
	int rc = 0;
	enum apr_subsys_state q6_state;
	struct device *dev = &pdev->dev;
	uint32_t mdf_mem_data_size = 0;

	/* TODO: MDF probing should have no dependency
	 * on ADSP Q6 state.
	 */
	q6_state = apr_get_q6_state();
	if (q6_state == APR_SUBSYS_DOWN) {
		dev_dbg(dev, "defering %s, adsp_state %d\n",
			__func__, q6_state);
		rc = -EPROBE_DEFER;
		goto err;
	} else
		dev_dbg(dev, "%s: adsp is ready\n", __func__);

	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-mdf-cb"))
		return msm_mdf_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-mdf-mem-region")) {
		mdf_mem_data.dev = dev;

		rc = of_property_read_u32(dev->of_node,
				    "qcom,msm-mdf-mem-data-size",
				    &mdf_mem_data_size);
		if (rc) {
			dev_dbg(&pdev->dev, "MDF mem data size entry not found\n");
			goto err;
		}

		mdf_mem_data.size = mdf_mem_data_size;
		dev_info(dev, "%s: mem region size %zd\n",
			__func__, mdf_mem_data.size);
		msm_mdf_alloc_dma_buf(&mdf_mem_data);
		return 0;
	}

	rc = of_platform_populate(pdev->dev.of_node,
					msm_mdf_match_table,
					NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to populate child nodes",
			__func__);
		goto err;
	}
	mdf_mem_data.device_status |= MSM_MDF_PROBED;

err:
	return rc;
}

static struct platform_driver msm_mdf_driver = {
	.probe = msm_mdf_probe,
	.remove = msm_mdf_remove,
	.driver = {
		.name = "msm-mdf",
		.owner = THIS_MODULE,
		.of_match_table = msm_mdf_match_table,
	},
};

static struct notifier_block nb = {
	.priority = 0,
	.notifier_call = msm_mdf_restart_notifier_cb,
};

int __init msm_mdf_init(void)
{
	/* Only need to monitor SSR from ADSP, which
	 * is the master DSP managing MDF memory.
	 */
	ssr_handle = subsys_notif_register_notifier("adsp", &nb);
	return platform_driver_register(&msm_mdf_driver);
}

void __exit msm_mdf_exit(void)
{
	platform_driver_unregister(&msm_mdf_driver);

	if (ssr_handle)
		subsys_notif_unregister_notifier(ssr_handle, &nb);
}

MODULE_DESCRIPTION("MSM MDF Module");
MODULE_LICENSE("GPL v2");
