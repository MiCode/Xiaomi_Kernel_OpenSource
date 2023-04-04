// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/debugfs.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/suspend.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>

#include <mtk_sleep.h>	    /* slp_module_init */
#include <mt-plat/aee.h>	/* aee_xxx */

#include <mtk_spm_irq.h>
#include <mtk_spm_internal.h>
#include <mtk_sspm.h>

#include <mtk_idle_fs/mtk_idle_sysfs.h>

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
#include <linux/regmap.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/mfd/mt6397/core.h>

/* PMICWRAP Reg */
#define PMIC_RG_TOP2_RSV0_ADDR             (0x15A)
#endif /* CONFIG_MTK_PLAT_POWER_MT6761 */

DEFINE_SPINLOCK(__spm_lock);

void __iomem *spm_base;
void __iomem *sleep_reg_md_base;

static struct platform_device *pspmdev;
static struct wakeup_source *spm_wakelock;
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
static struct mt6357_priv *spm_priv;
#endif

/* FIXME: should not used externally !!! */
void *mt_spm_base_get(void)
{
	return spm_base;
}
EXPORT_SYMBOL(mt_spm_base_get);

void spm_pm_stay_awake(int sec)
{
	__pm_wakeup_event(spm_wakelock, jiffies_to_msecs(HZ * sec));
}

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
static int is_pmic_mrv(void)
{
	int ret, tmp_val = 0;
	struct regmap *regmap = spm_priv->regmap;

	ret = regmap_read(regmap, PMIC_RG_TOP2_RSV0_ADDR, &tmp_val);

	if (ret)
		return -1;

	if (tmp_val & (1 << 15))
		return 1;
	else
		return 0;
}
#endif

static void spm_register_init(unsigned int *spm_irq_0_ptr)
{
	struct device_node *node;
	unsigned int spmirq0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node)
		pr_info("[SPM] find sleep node failed\n");

	spm_base = of_iomap(node, 0);
	if (!spm_base)
		pr_info("[SPM] base spm_base failed\n");

	spmirq0 = irq_of_parse_and_map(node, 0);
	if (!spmirq0)
		pr_info("[SPM] get spm_irq_0 failed\n");
	*spm_irq_0_ptr = spmirq0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep_reg_md");
	if (!node)
		pr_info("[SPM] find sleep_reg_md node failed\n");

	sleep_reg_md_base = of_iomap(node, 0);
	if (!sleep_reg_md_base)
		pr_info("[SPM] base sleep_reg_md_base failed\n");

	pr_info("[SPM] spm_base = %p, sleep_reg_md_base = %p, spm_irq_0 = %d\n",
		spm_base, sleep_reg_md_base, spmirq0);
}

static ssize_t debug_log_show(
	struct device *dev, struct device_attribute *attr,
	char *buf)
{
	char *p = buf;

	p += sprintf(p, "for test\n");

	return p - buf;
}

static ssize_t debug_log_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR_RW(debug_log);
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
static int spm_init_done;

static const struct of_device_id spm_sleep_of_ids[] = {
	{.compatible = "mediatek,sleep",},
	{}
};
#endif

static int spm_probe(struct platform_device *pdev)
{
	int ret;
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
	struct mt6397_chip *chip = NULL;
	struct device_node *pmic_node = NULL;
	struct device_node *node = NULL;
	struct platform_device *pmic_pdev = NULL;

	if (spm_init_done)
		return 0;
	node = of_find_matching_node(NULL, spm_sleep_of_ids);
	if (!node) {
		dev_notice(&pdev->dev, "fail to find spm sleep node\n");
		return -ENODEV;
	}
	pmic_node = of_parse_phandle(node, "pmic", 0);
	if (!pmic_node) {
		dev_notice(&pdev->dev, "fail to find pmic node\n");
		return -ENODEV;
	}
	pmic_pdev = of_find_device_by_node(pmic_node);
	if (!pmic_pdev) {
		dev_notice(&pdev->dev, "fail to find pmic device or some project no pmic config\n");
		return -ENODEV;
	}
	chip = dev_get_drvdata(&(pmic_pdev->dev));
	if (!chip) {
		dev_notice(&pdev->dev, "fail to find pmic drv data\n");
		return -ENODEV;
	}
	if (IS_ERR_VALUE(chip->regmap)) {
		spm_priv->regmap = NULL;
		dev_notice(&pdev->dev, "get pmic regmap fail\n");
		return -ENODEV;
	}
#endif
	ret = device_create_file(&(pdev->dev), &dev_attr_debug_log);
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
	spm_priv = devm_kzalloc(&pdev->dev,
				sizeof(struct mt6357_priv),
				GFP_KERNEL);
	if (!spm_priv)
		return -ENOMEM;

	spm_priv->regmap = chip->regmap;

	/* Do initialization only one time */
	spm_init_done = 1;

	if (is_pmic_mrv())
		SMC_CALL(ARGS, SPM_ARGS_PMIC_MRV, 0, 0);
#endif
	return 0;
}

static int spm_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id spm_of_ids[] = {
	{.compatible = "mediatek,sleep",},
	{}
};

static struct platform_driver spm_dev_drv = {
	.probe = spm_probe,
	.remove = spm_remove,
	.driver = {
	.name = "spm",
	.owner = THIS_MODULE,
	.of_match_table = spm_of_ids,
	},
};

static int spm_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	struct timespec64 ts;
	struct rtc_time tm;

	ktime_get_real_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec, &tm);

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		printk_deferred(
		"[name:spm&][SPM] PM: suspend entry %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		printk_deferred(
		"[name:spm&][SPM] PM: suspend exit %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block spm_pm_notifier_func = {
	.notifier_call = spm_pm_event,
	.priority = 0,
};


static const struct mtk_idle_sysfs_op spm_last_wakeup_src_fops = {
	.fs_read = get_spm_last_wakeup_src,
};

static const struct mtk_idle_sysfs_op spm_sleep_count_fops = {
	.fs_read = get_spm_sleep_count,
};

static const struct mtk_idle_sysfs_op spm_last_debug_flag_fops = {
	.fs_read = get_spm_last_debug_flag,
};

static const struct mtk_idle_sysfs_op spm_spmfw_version_fops = {
	.fs_read = get_spmfw_version,
};

static int spm_module_init(void)
{
	unsigned int spm_irq_0 = 0;
	int r = 0;
	int ret = -1;
	struct mtk_idle_sysfs_handle pParent2ND;
	struct mtk_idle_sysfs_handle *pParent = NULL;

	spm_wakelock = wakeup_source_register(NULL, "spm");
	if (spm_wakelock == NULL) {
		pr_debug("fail to request spm_wakelock\n");
		return ret;
	}
	spm_register_init(&spm_irq_0);

	/* implemented in mtk_spm_irq.c */
	if (mtk_spm_irq_register(spm_irq_0) != 0)
		r = -EPERM;
#if IS_ENABLED(CONFIG_PM)
	if (spm_fs_init() != 0)
		r = -EPERM;
#endif

	/* implemented in mtk_spm_dram.c */
	spm_do_dram_config_check();

	ret = platform_driver_register(&spm_dev_drv);
	if (ret) {
		pr_debug("fail to register platform driver\n");
		return ret;
	}

	pspmdev = platform_device_register_simple("spm", -1, NULL, 0);
	if (IS_ERR(pspmdev)) {
		pr_debug("Failed to register platform device.\n");
		return -EINVAL;
	}

	mtk_idle_sysfs_entry_create();
	if (mtk_idle_sysfs_entry_root_get(&pParent) == 0) {
		mtk_idle_sysfs_entry_func_create("spm", 0444
			, pParent, &pParent2ND);
		mtk_idle_sysfs_entry_func_node_add("spm_sleep_count", 0444
			, &spm_sleep_count_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("spm_last_wakeup_src", 0444
			, &spm_last_wakeup_src_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("spm_last_debug_flag", 0444
			, &spm_last_debug_flag_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("spmfw_version", 0444
			, &spm_spmfw_version_fops, &pParent2ND, NULL);
	}

	ret = register_pm_notifier(&spm_pm_notifier_func);
	if (ret) {
		pr_debug("Failed to register PM notifier.\n");
		return ret;
	}

	SMC_CALL(ARGS, SPM_ARGS_SPMFW_IDX, spm_get_spmfw_idx(), 0);

	return 0;
}

int mtk_spm_init(void)
{
	int ret;

	ret = spm_module_init();

	return ret;
}

bool mtk_spm_base_ready(void)
{
	return spm_base != 0;
}

unsigned int mtk_spm_read_register(int register_index)
{
	if (register_index == SPM_PWRSTA)
		return spm_read(PWR_STATUS);
	else if (register_index == SPM_MD1_PWR_CON)
		return spm_read(MD1_PWR_CON);
	else if (register_index == SPM_REG13)
		return spm_read(PCM_REG13_DATA);
	else if (register_index == SPM_SPARE_ACK_MASK)
		return spm_read(SPARE_ACK_MASK);
	else
		return 0;
}

MODULE_DESCRIPTION("SPM Driver v0.1");
