// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>
 */

#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/timekeeping.h>

#define ISPDVFS_DBG
#ifdef ISPDVFS_DBG
#define ISP_LOGD(fmt, args...) \
	do { \
		if (mtk_ispdvfs_dbg_level) \
			pr_notice("[ISPDVFS] %s(): " fmt "\n",\
				__func__, ##args); \
	} while (0)
#else
#define ISPDVFS_DBG(fmt, args...)
#endif
#define ISP_LOGI(fmt, args...) \
	pr_notice("[ISPDVFS] %s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("[ISPDVFS] error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("[ISPDVFS] fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

#define SPM_VMM_HW_SEM_REG_OFST 0x6A8
#define SPM_VMM_ISO_REG_OFST 0xF30
#define SPM_VMM_EXT_BUCK_ISO_BIT 16
#define SPM_AOC_VMM_SRAM_ISO_DIN_BIT 17
#define SPM_AOC_VMM_SRAM_LATCH_ENB 18
#define SPM_HW_SEM_TIMEOUT_NS 1000000000 /* 1 sec for acquiring sem timeout */

struct vmm_spm_drv_data {
	void __iomem *spm_reg;
	struct notifier_block nb;
};

struct vmm_spm_drv_data g_drv_data;

static int acquire_hw_semaphore(void __iomem *hw_sem_addr)
{
	u32 hw_sem;
	u64 w_timestamp, r_timestamp, time_remain;

	/* Try to acquire hw semaphore */
	hw_sem = readl_relaxed(hw_sem_addr);
	hw_sem |= 0x1;
	writel_relaxed(hw_sem, hw_sem_addr);
	w_timestamp = ktime_get_boottime_ns();

	do {
		hw_sem = readl_relaxed(hw_sem_addr) & 0x1;

		r_timestamp = ktime_get_boottime_ns();
		time_remain = r_timestamp - w_timestamp;
		if (time_remain > SPM_HW_SEM_TIMEOUT_NS) {
			ISP_LOGE("Acquire hw sem timeout (%lu)\n", time_remain);
			WARN_ON(1);
			return -EINVAL;
		}
	} while (hw_sem != 0x1);

	return 0;
}

static void release_hw_semaphore(void __iomem *hw_sem_addr)
{
	u32 hw_sem;

	hw_sem = readl_relaxed(hw_sem_addr);
	hw_sem |= 0x1;
	writel_relaxed(hw_sem, hw_sem_addr);
}

static void vmm_buck_isolation_off(void __iomem *base)
{
	void __iomem *reg_buck_iso_addr = base + SPM_VMM_ISO_REG_OFST;
	void __iomem *hw_sem_addr = base + SPM_VMM_HW_SEM_REG_OFST;
	u32 reg_buck_iso_val;

	if (acquire_hw_semaphore(hw_sem_addr)) {
		ISP_LOGE("vmm_buck_isolation_off fail\n");
		return;
	}

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_VMM_EXT_BUCK_ISO_BIT);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_AOC_VMM_SRAM_ISO_DIN_BIT);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_AOC_VMM_SRAM_LATCH_ENB);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	release_hw_semaphore(hw_sem_addr);
}

static void vmm_buck_isolation_on(void __iomem *base)
{
	void __iomem *reg_buck_iso_addr = base + SPM_VMM_ISO_REG_OFST;
	void __iomem *hw_sem_addr = base + SPM_VMM_HW_SEM_REG_OFST;
	u32 reg_buck_iso_val;

	if (acquire_hw_semaphore(hw_sem_addr)) {
		ISP_LOGE("vmm_buck_isolation_on fail\n");
		return;
	}

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_AOC_VMM_SRAM_LATCH_ENB);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_AOC_VMM_SRAM_ISO_DIN_BIT);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_VMM_EXT_BUCK_ISO_BIT);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	release_hw_semaphore(hw_sem_addr);
}

static int regulator_event_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct vmm_spm_drv_data *drv_data = &g_drv_data;

	if (!drv_data->spm_reg) {
		ISP_LOGE("SPM_BASE is NULL");
		return -EINVAL;
	}

	if (event == REGULATOR_EVENT_ENABLE) {
		vmm_buck_isolation_off(drv_data->spm_reg);
		ISP_LOGI("VMM regulator enable done");
	} else if (event == REGULATOR_EVENT_PRE_DISABLE) {
		vmm_buck_isolation_on(drv_data->spm_reg);
		ISP_LOGI("VMM regulator before disable");
	}

	return 0;
}

static int vmm_spm_probe(struct platform_device *pdev)
{
	struct vmm_spm_drv_data *drv_data = &g_drv_data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct regulator *reg;
	s32 ret;

	/* SPM registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ISP_LOGE("fail to get resource SPM_BASE");
		return -EINVAL;
	}

	drv_data->spm_reg = devm_ioremap(dev, res->start, resource_size(res));
	if (!(drv_data->spm_reg)) {
		ISP_LOGE("fail to ioremap SPM_BASE: 0x%llx", res->start);
		return -EINVAL;
	}

	reg = devm_regulator_get(dev, "vmm-pmic");
	if (IS_ERR(reg)) {
		ISP_LOGE("devm_regulator_get vmm-pmic fail");
		return PTR_ERR(reg);
	}

	drv_data->nb.notifier_call = regulator_event_notify;
	ret = devm_regulator_register_notifier(reg, &drv_data->nb);
	if (ret)
		ISP_LOGE("Failed to register notifier: %d\n", ret);

	return ret;
}

static const struct of_device_id of_vmm_spm_match_tbl[] = {
	{
		.compatible = "mediatek,vmm_spm",
	},
	{}
};

static struct platform_driver drv_vmm_spm = {
	.probe = vmm_spm_probe,
	.driver = {
		.name = "mtk-vmm-spm",
		.of_match_table = of_vmm_spm_match_tbl,
	},
};

static int __init mtk_vmm_spm_init(void)
{
	s32 status;

	status = platform_driver_register(&drv_vmm_spm);
	if (status) {
		pr_notice("Failed to register VMM SPM driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_vmm_spm_exit(void)
{
	platform_driver_unregister(&drv_vmm_spm);
}

module_init(mtk_vmm_spm_init);
module_exit(mtk_vmm_spm_exit);
MODULE_DESCRIPTION("MTK VMM SPM driver");
MODULE_AUTHOR("Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>");
MODULE_LICENSE("GPL v2");
