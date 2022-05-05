// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/remoteproc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/memremap.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/time64.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/sched/clock.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>

#include <mt-plat/aee.h>

#include "apu.h"
#include "apu_debug.h"
#include "apu_excep.h"
#include "apu_config.h"
#include "apusys_core.h"


struct mtk_apu *g_apu_struct;
uint32_t g_apu_log;

static void *apu_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	void *ptr = NULL;
	struct mtk_apu *apu = (struct mtk_apu *)rproc->priv;

	if (da < DRAM_OFFSET + CODE_BUF_SIZE) {
		ptr = apu->code_buf + (da - DRAM_OFFSET);
		dev_info(apu->dev, "%s: (DRAM): da = 0x%llx, len = 0x%x\n",
			__func__, da, len);
	} else if (da >= TCM_OFFSET && da < TCM_OFFSET + TCM_SIZE) {
		ptr = apu->md32_tcm + (da - TCM_OFFSET);
		dev_info(apu->dev, "%s: (TCM): da = 0x%llx, len = 0x%x\n",
			__func__, da, len);
	}
	return ptr;
}

static int __apu_run(struct rproc *rproc)
{
	struct mtk_apu *apu = (struct mtk_apu *)rproc->priv;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct device *dev = apu->dev;
	struct apu_run *run = &apu->run;
	struct timespec64 begin, end, delta;
	int ret;

	if (!hw_ops->start) {
		WARN_ON(1);
		return -EINVAL;
	}

	hw_ops->power_on(apu);
	hw_ops->start(apu);

	/* check if boot success */
	ktime_get_ts64(&begin);

	ret = wait_event_interruptible_timeout(
					run->wq,
					run->signaled,
					msecs_to_jiffies(10000));

	ktime_get_ts64(&end);

	if (ret == 0) {
		dev_info(dev, "APU initialization timeout!!\n");
		ret = -ETIME;
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_BOOT_TIMEOUT");
		goto stop;
	}
	if (ret == -ERESTARTSYS)
		dev_info(dev, "wait APU interrupted by a signal!!\n");

	delta = timespec64_sub(end, begin);
	dev_info(dev,
		 "APU uP boot done. boot time: %llu s, %llu ns. fw_ver: %s\n",
		 (uint64_t)delta.tv_sec, (uint64_t)delta.tv_nsec, run->fw_ver);

	return 0;

stop:
	if (!hw_ops->stop) {
		WARN_ON(1);
		return -EINVAL;
	}
	hw_ops->stop(apu);

	return ret;
}

static int apu_start(struct rproc *rproc)
{
	struct mtk_apu *apu = (struct mtk_apu *)rproc->priv;

	dev_info(apu->dev, "%s: try to boot uP\n", __func__);
	return __apu_run(rproc);
}

static int apu_attach(struct rproc *rproc)
{
	struct mtk_apu *apu = (struct mtk_apu *)rproc->priv;

	dev_info(apu->dev, "%s: try to boot uP\n", __func__);
	return __apu_run(rproc);
}

static int apu_stop(struct rproc *rproc)
{
	struct mtk_apu *apu = (struct mtk_apu *)rproc->priv;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;

	if (!hw_ops->stop) {
		WARN_ON(1);
		return -EINVAL;
	}
	hw_ops->stop(apu);

	return 0;
}

static const struct rproc_ops apu_ops = {
	.start		= apu_start,
	.stop		= apu_stop,
	.attach		= apu_attach,
	.da_to_va	= apu_da_to_va,
};

static void apu_dram_boot_remove(struct mtk_apu *apu)
{
	void *domain = iommu_get_domain_for_dev(apu->dev);
	u32 boundary = (u32) upper_32_bits(apu->code_da);
	u64 iova = CODE_BUF_DA | ((u64) boundary << 32);

	if (apu->platdata->flags & F_SECURE_BOOT)
		return;

	if (apu->platdata->flags & F_PRELOAD_FIRMWARE) {
		if (domain != NULL)
			iommu_unmap(domain, APU_SEC_FW_IOVA, apu->apusys_sec_mem_size);
	} else if ((apu->platdata->flags & F_BYPASS_IOMMU) == 0)
		if (domain != NULL)
			iommu_unmap(domain, iova, CODE_BUF_SIZE);

	if ((apu->platdata->flags & F_PRELOAD_FIRMWARE) == 0)
		dma_free_coherent(apu->dev, CODE_BUF_SIZE, apu->code_buf, apu->code_da);
}

static int apu_dram_boot_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;
	int map_sg_sz = 0;
	void *domain;
	struct sg_table sgt;
	phys_addr_t pa;
	u32 boundary;
	u64 iova;

	if (apu->platdata->flags & F_SECURE_BOOT)
		return 0;

	if ((apu->platdata->flags & F_BYPASS_IOMMU) == 0) {
		domain = iommu_get_domain_for_dev(apu->dev);
		if (domain == NULL) {
			dev_info(dev, "%s: iommu_get_domain_for_dev fail\n", __func__);
			return -ENOMEM;
		}
	}

	if (apu->platdata->flags & F_PRELOAD_FIRMWARE &&
		(apu->platdata->flags & F_BYPASS_IOMMU) == 0) {
		apu->code_buf = (void *) apu->apu_sec_mem_base +
			apu->apusys_sec_info->up_code_buf_ofs;
		apu->code_da = APU_SEC_FW_IOVA;
		/* Map reserved code buffer to APU_SEC_FW_IOVA */
		ret = iommu_map(domain, APU_SEC_FW_IOVA,
				apu->apusys_sec_mem_start,
				apu->apusys_sec_mem_size, IOMMU_READ|IOMMU_WRITE);
		if (ret) {
			dev_info(dev, "%s: iommu_map fail(%d)\n", __func__, ret);
			return ret;
		}

		dev_info(dev, "%s: iommu_map done\n", __func__);
		return ret;
	}

	/* Allocate code buffer */
	apu->code_buf = dma_alloc_coherent(apu->dev, CODE_BUF_SIZE,
					&apu->code_da, GFP_KERNEL);
	if (apu->code_buf == NULL || apu->code_da == 0) {
		dev_info(dev, "%s: dma_alloc_coherent fail\n", __func__);
		return -ENOMEM;
	}
	memset(apu->code_buf, 0, CODE_BUF_SIZE);

	boundary = (u32) upper_32_bits(apu->code_da);
	iova = CODE_BUF_DA | ((u64) boundary << 32);
	dev_info(dev, "%s: boundary = %u, iova = 0x%llx\n",
		__func__, boundary, iova);

	if ((apu->platdata->flags & F_BYPASS_IOMMU) == 0) {
		sgt.sgl = NULL;
		/* Convert IOVA to sgtable */
		ret = dma_get_sgtable(apu->dev, &sgt, apu->code_buf,
			apu->code_da, CODE_BUF_SIZE);
		if (ret < 0 || sgt.sgl == NULL) {
			dev_info(dev, "get sgtable fail\n");
			return -EINVAL;
		}

		dev_info(dev, "%s: sgt.nents = %d, sgt.orig_nents = %d\n",
			__func__, sgt.nents, sgt.orig_nents);
		/* Map sg_list to MD32_BOOT_ADDR */
		map_sg_sz = iommu_map_sg(domain, iova, sgt.sgl,
			sgt.nents, IOMMU_READ|IOMMU_WRITE);
		dev_info(dev, "%s: sgt.nents = %d, sgt.orig_nents = %d\n",
			__func__, sgt.nents, sgt.orig_nents);
		dev_info(dev, "%s: map_sg_sz = %d\n", __func__, map_sg_sz);
		if (map_sg_sz != CODE_BUF_SIZE)
			dev_info(dev, "%s: iommu_map_sg fail(%d)\n", __func__, ret);

		pa = iommu_iova_to_phys(domain,
			iova + CODE_BUF_SIZE - SZ_4K);
		dev_info(dev, "%s: pa = 0x%llx\n",
			__func__, (uint64_t) pa);
		if (!pa) // pa should not be null
			dev_info(dev, "%s: check pa fail(0x%llx)\n",
				__func__, (uint64_t) pa);
	}

	dev_info(dev, "%s: apu->code_buf = 0x%llx, apu->code_da = 0x%llx\n",
		__func__, (uint64_t) apu->code_buf, (uint64_t) apu->code_da);

	return ret;
}

static int apu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *apusys_sec_mem_node,
		*apusys_aee_coredump_mem_node;
	struct rproc *rproc;
	struct mtk_apu *apu;
	struct mtk_apu_platdata *data;
	struct mtk_apu_hw_ops *hw_ops;
	char *fw_name = "mrv.elf";
	int ret = 0;
	uint32_t up_code_buf_sz;

	dev_info(dev, "%s: enter\n", __func__);
	data = (struct mtk_apu_platdata *)of_device_get_match_data(dev);
	if (!data) {
		dev_info(dev, "%s: of_device_get_match_data fail\n", __func__);
		return -EINVAL;
	}
	dev_info(dev, "%s: platdata flags=0x%08x\n", __func__, data->flags);
	hw_ops = &data->ops;

	rproc = rproc_alloc(dev,
						np->name,
						&apu_ops,
						fw_name,
						sizeof(struct mtk_apu));

	if (!rproc) {
		dev_info(dev, "unable to allocate remoteproc\n");
		return -ENOMEM;
	}

	/*
	 * auto boot off
	 * make sure the firmware is in /vendor/firmware/mrv.elf
	 * before boot up.
	 *
	 * use below command to run uP:
	 * echo start > /sys/class/remoteproc/remoteproc0/state
	 */
	if (data->flags & F_AUTO_BOOT)
		rproc->auto_boot = true;
	else
		rproc->auto_boot = false;

	if (data->flags & F_DEBUG_LOG_ON)
		g_apu_log = 1;
	else
		g_apu_log = 0;

	apu = (struct mtk_apu *)rproc->priv;
	g_apu_struct = apu;
	dev_info(dev, "%s: apu=%p\n", __func__, apu);
	apu->rproc = rproc;
	apu->pdev = pdev;
	apu->dev = dev;
	apu->platdata = data;
	platform_set_drvdata(pdev, apu);
	spin_lock_init(&apu->reg_lock);

	if (apu->platdata->flags & F_PRELOAD_FIRMWARE) {
		/* prevent MPU violation when F_SECURE_BOOT is enabled */
		if ((apu->platdata->flags & F_SECURE_BOOT) == 0) {
			apusys_sec_mem_node = of_find_compatible_node(NULL, NULL,
				"mediatek,apu_apusys-rv_secure");
			if (!apusys_sec_mem_node) {
				apu_drv_debug("DT,mediatek,apu_apusys-rv_secure not found\n");
				ret = -EINVAL;
				goto out_free_rproc;
			}
			of_property_read_u64_index(apusys_sec_mem_node, "reg", 0,
				&(apu->apusys_sec_mem_start));
			of_property_read_u64_index(apusys_sec_mem_node, "reg", 1,
				&(apu->apusys_sec_mem_size));
			apu_drv_debug("%s: start = 0x%llx, size = 0x%llx\n",
				apusys_sec_mem_node->full_name, apu->apusys_sec_mem_start,
				apu->apusys_sec_mem_size);
			apu->apu_sec_mem_base = memremap(apu->apusys_sec_mem_start,
				apu->apusys_sec_mem_size, MEMREMAP_WC);

			ret = of_property_read_u32(np, "up_code_buf_sz",
						   &up_code_buf_sz);
			if (ret) {
				dev_info(dev, "parsing up_code_buf_sz error: %d\n", ret);
				ret = -EINVAL;
				goto out_free_rproc;
			}

			apu->apusys_sec_info = (struct apusys_secure_info_t *)
				(apu->apu_sec_mem_base + up_code_buf_sz);

			apu_drv_debug("up_fw_ofs = 0x%x, up_fw_sz = 0x%x\n",
				apu->apusys_sec_info->up_fw_ofs,
				apu->apusys_sec_info->up_fw_sz);

			apu_drv_debug("up_xfile_ofs = 0x%x, up_xfile_sz = 0x%x\n",
				apu->apusys_sec_info->up_xfile_ofs,
				apu->apusys_sec_info->up_xfile_sz);
		}

		apusys_aee_coredump_mem_node = of_find_compatible_node(NULL, NULL,
			"mediatek,apu_apusys-rv_aee-coredump");
		if (!apusys_aee_coredump_mem_node) {
			dev_info(dev, "DT,mediatek,apu_apusys-rv_aee-coredump not found\n");
			ret = -EINVAL;
			goto out_free_rproc;
		}
		of_property_read_u64_index(apusys_aee_coredump_mem_node, "reg", 0,
			&(apu->apusys_aee_coredump_mem_start));
		of_property_read_u64_index(apusys_aee_coredump_mem_node, "reg", 1,
			&(apu->apusys_aee_coredump_mem_size));
		apu_drv_debug("%s: start = 0x%llx, size = 0x%llx\n",
			apusys_aee_coredump_mem_node->full_name,
			apu->apusys_aee_coredump_mem_start,
			apu->apusys_aee_coredump_mem_size);
		apu->apu_aee_coredump_mem_base =
			ioremap_cache(apu->apusys_aee_coredump_mem_start,
				apu->apusys_aee_coredump_mem_size);

		apu->apusys_aee_coredump_info = (struct apusys_aee_coredump_info_t *)
			apu->apu_aee_coredump_mem_base;
	}

	apu_drv_debug("before pm_runtime_enable\n");
	pm_runtime_enable(&pdev->dev);

	/*
	 * CAUTION !
	 * this line will cause rpm refcnt of apu_top +2
	 * apusys_rv -> iommu0 -> apu_top
	 * apusys_rv -> iommu1 -> apu_top
	 */
	pm_runtime_get_sync(&pdev->dev);

	if (data->flags & F_AUTO_BOOT) {
		ret = hw_ops->power_init(apu);
		if (ret) {
			pm_runtime_put_sync(&pdev->dev);
			goto out_free_rproc;
		}
	}

	if (!hw_ops->apu_memmap_init) {
		pm_runtime_put_sync(&pdev->dev);
		if (data->flags & F_AUTO_BOOT)
			pm_runtime_put_sync(apu->power_dev);
		WARN_ON(1);
		goto out_free_rproc;
	}

	ret = hw_ops->apu_memmap_init(apu);
	if (ret)
		goto remove_apu_memmap;

	ret = apu_mem_init(apu);
	if (ret)
		goto remove_apu_mem;

	ret = apu_config_setup(apu);
	if (ret)
		goto remove_apu_config_setup;

	ret = apu_coredump_init(apu);
	if (ret)
		goto remove_apu_coredump;

	ret = apu_dram_boot_init(apu);
	if (ret)
		goto remove_apu_dram_boot;

	ret = apu_ipi_init(pdev, apu);
	if (ret)
		goto remove_apu_ipi;

	if (data->flags & F_AUTO_BOOT) {
		ret = apu_deepidle_init(apu);
		if (ret < 0)
			goto remove_apu_deepidle;
	}

	ret = apu_debug_init(apu);
	if (ret)
		goto remove_apu_debug;

	ret = apu_timesync_init(apu);
	if (ret)
		goto remove_apu_timesync;

	ret = apu_procfs_init(pdev);
	if (ret)
		goto remove_apu_procfs;

	ret = apu_excep_init(pdev, apu);
	if (ret < 0)
		goto remove_apu_excep;

	if (data->flags & F_PRELOAD_FIRMWARE)
		rproc->state = RPROC_DETACHED;

	ret = rproc_add(rproc);
	if (ret < 0) {
		dev_info(dev, "boot fail ret:%d\n", ret);
		goto remove_apu_excep;
	}

	/* to avoid running state being changed through rproc sysfs */
	if ((data->flags & F_PRELOAD_FIRMWARE) && (data->flags & F_AUTO_BOOT))
		rproc->state = RPROC_DETACHED;

	if (hw_ops->init) {
		ret = hw_ops->init(apu);
		if (ret)
			goto del_rproc;
	}

	pm_runtime_put_sync(&pdev->dev);
	if (data->flags & F_AUTO_BOOT)
		pm_runtime_put_sync(apu->power_dev);

	return 0;

del_rproc:
	rproc_del(rproc);


remove_apu_excep:
	apu_excep_remove(pdev, apu);

remove_apu_procfs:
	apu_procfs_remove(pdev);

remove_apu_timesync:
	apu_timesync_remove(apu);

remove_apu_debug:
	apu_debug_remove(apu);

remove_apu_deepidle:
	apu_deepidle_exit(apu);

remove_apu_ipi:
	apu_ipi_remove(apu);

remove_apu_dram_boot:
	apu_dram_boot_remove(apu);

remove_apu_coredump:
	apu_coredump_remove(apu);

remove_apu_config_setup:
	apu_config_remove(apu);

remove_apu_mem:
	apu_mem_remove(apu);

remove_apu_memmap:
	pm_runtime_put_sync(&pdev->dev);
	if (data->flags & F_AUTO_BOOT)
		pm_runtime_put_sync(apu->power_dev);
	if (!hw_ops->apu_memmap_remove) {
		WARN_ON(1);
		return -EINVAL;
	}
	hw_ops->apu_memmap_remove(apu);

out_free_rproc:
	rproc_free(rproc);

	return ret;
}

static int apu_remove(struct platform_device *pdev)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;

	if (hw_ops->exit)
		hw_ops->exit(apu);

	rproc_del(apu->rproc);

	apu_deepidle_exit(apu);

	apu_excep_remove(pdev, apu);
	apu_procfs_remove(pdev);
	apu_timesync_remove(apu);
	apu_debug_remove(apu);
	apu_ipi_remove(apu);
	apu_dram_boot_remove(apu);
	apu_coredump_remove(apu);
	apu_config_remove(apu);
	apu_mem_remove(apu);
	if (!hw_ops->apu_memmap_remove) {
		WARN_ON(1);
		return -EINVAL;
	}
	hw_ops->apu_memmap_remove(apu);

	rproc_free(apu->rproc);

	return 0;
}

#ifndef MT6879_APUSYS_RV_PLAT_DATA
const struct mtk_apu_platdata mt6879_platdata;
#endif
#ifndef MT6893_APUSYS_RV_PLAT_DATA
const struct mtk_apu_platdata mt6893_platdata;
#endif
#ifndef MT6895_APUSYS_RV_PLAT_DATA
const struct mtk_apu_platdata mt6895_platdata;
#endif
#ifndef MT6983_APUSYS_RV_PLAT_DATA
const struct mtk_apu_platdata mt6983_platdata;
#endif

static const struct of_device_id mtk_apu_of_match[] = {
	{ .compatible = "mediatek,mt6879-apusys_rv", .data = &mt6879_platdata},
	{ .compatible = "mediatek,mt6893-apusys_rv", .data = &mt6893_platdata},
	{ .compatible = "mediatek,mt6895-apusys_rv", .data = &mt6895_platdata},
	{ .compatible = "mediatek,mt6983-apusys_rv", .data = &mt6983_platdata},
	{},
};

static struct platform_driver mtk_apu_driver = {
	.probe = apu_probe,
	.remove = apu_remove,
	.driver = {
		.name = "mtk-apu",
		.of_match_table = of_match_ptr(mtk_apu_of_match),
	},
};

int apu_rproc_init(struct apusys_core_info *info)
{
	int ret;

	ret = platform_driver_register(&mtk_apu_driver);
	if (ret)
		pr_info("failed to register mtk_apu_driver\n");

	return ret;
}

void apu_rproc_exit(void)
{
	platform_driver_unregister(&mtk_apu_driver);
}
