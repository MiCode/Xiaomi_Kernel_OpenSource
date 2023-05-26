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
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>

#include <mtk_cpuidle.h>	/* mtk_cpuidle_init */
#include <mtk_sleep.h>	    /* slp_module_init */
#include <mt-plat/aee.h>	/* aee_xxx */

#include <mtk_spm_irq.h>
#include <mtk_spm_internal.h>
#include <mtk_sspm.h>

#include <mtk_idle_fs/mtk_idle_sysfs.h>
#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#else
static inline int dram_steps_freq(unsigned int step)
{
	return 4266;
}
#endif

DEFINE_SPINLOCK(__spm_lock);

void __attribute__ ((weak)) mtk_idle_cond_check_init(void)
{
	aee_sram_printk("NO %s !!!\n", __func__);
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
}

/* Note: implemented in mtk_spm_vcorefs.c */
void  __attribute__ ((weak)) spm_vcorefs_init(void)
{
	aee_sram_printk("NO %s !!!\n", __func__);
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
}

/* Note: implemented in mtk_spm_dram.c */
int __attribute__ ((weak)) spm_get_spmfw_idx(void)
{
	aee_sram_printk("NO %s !!!\n", __func__);
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
	return 0;
}

/* Note: implemented in mtk_spm_irq.c */
int __attribute__ ((weak)) mtk_spm_irq_register(unsigned int spmirq0)
{
	aee_sram_printk("NO %s !!!\n", __func__);
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
	return 0;
}

/* Note: implemented in mtk_cpuidle.c */
int __attribute__ ((weak)) mtk_cpuidle_init(void) { return -EOPNOTSUPP; }

/* Note: implemented in mtk_spm_dram.c */
void __attribute__((weak)) spm_do_dram_config_check(void)
{
	aee_sram_printk("NO %s !!!\n", __func__);
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
}

/* Note: implemented in mtk_spm_fs.c */
int __attribute__((weak)) spm_fs_init(void)
{
	aee_sram_printk("NO %s !!!\n", __func__);
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
	return 0;
}


/* Note: implemented in mtk_spm_utils.c */
ssize_t __attribute__((weak)) get_spm_system_stats(
	char *ToUserBuf, size_t sz, void *priv) { return 0; }

ssize_t __attribute__((weak)) get_spm_subsystem_stats(
	char *ToUserBuf, size_t sz, void *priv) { return 0; }

/* Note: implemented in mtk_spm_sleep.c */
ssize_t __attribute__((weak)) get_spm_last_wakeup_src(
	char *ToUserBuf, size_t sz, void *priv) { return 0; }

/* Note: implemented in mtk_spm_sleep.c */
ssize_t __attribute__((weak)) get_spm_sleep_count(
	char *ToUserBuf, size_t sz, void *priv) { return 0; }

/* Note: implemented in mtk_spm_utils.c */
ssize_t __attribute__((weak)) get_spmfw_version(
	char *ToUserBuf, size_t sz, void *priv) { return 0; }

ssize_t __attribute__((weak)) get_spm_lp_ratio_timer_enable(
	char *ToUserBuf, size_t sz, void *priv) { return 0; }

ssize_t __attribute__((weak)) set_spm_lp_ratio_timer_enable(
	char *FromUserBuf, size_t sz, void *priv) { return 0; }

ssize_t __attribute__((weak)) get_spm_resource_req_timer_enable(
	char *ToUserBuf, size_t sz, void *priv) { return 0; }

ssize_t __attribute__((weak)) set_spm_resource_req_timer_enable(
	char *FromUserBuf, size_t sz, void *priv) { return 0; }

ssize_t __attribute__((weak)) set_network_traffic(
	char *FromUserBuf, size_t sz, void *priv) { return 0; }

void __iomem *spm_base;
void __iomem *sleep_reg_md_base;

static struct platform_device *pspmdev;
static struct wakeup_source *spm_wakelock;


/* FIXME: should not used externally !!! */
/* If remove: Need to check build break at */
/* vendor/mediatek/kernel_modules/met_drv_secur */
void *mt_spm_base_get(void)
{
	return spm_base;
}
EXPORT_SYMBOL(mt_spm_base_get);


void spm_pm_stay_awake(int sec)
{
	__pm_wakeup_event(spm_wakelock, jiffies_to_msecs(HZ * sec));
}

static void spm_register_init(unsigned int *spm_irq_0_ptr)
{
	struct device_node *node;
	unsigned int spmirq0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node)
		printk_deferred("[name:spm&][SPM] find sleep node failed\n");

	spm_base = of_iomap(node, 0);
	if (!spm_base)
		printk_deferred("[name:spm&][SPM] base spm_base failed\n");
	spmirq0 = irq_of_parse_and_map(node, 0);
	if (!spmirq0)
		printk_deferred("[name:spm&][SPM] get spm_irq_0 failed\n");
	*spm_irq_0_ptr = spmirq0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep_reg_md");
	if (!node)
		printk_deferred("[name:spm&][SPM] find sleep_reg_md node failed\n");

	sleep_reg_md_base = of_iomap(node, 0);
	if (!sleep_reg_md_base)
		printk_deferred("[name:spm&][SPM] base sleep_reg_md_base failed\n");

	printk_deferred("[name:spm&][SPM] spm_base = %p, sleep_reg_md_base = %p, spm_irq_0 = %d\n",
		spm_base, sleep_reg_md_base, spmirq0);
}

static ssize_t show_debug_log(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	char *p = buf;

	p += sprintf(p, "for test\n");

	return p - buf;
}

static ssize_t store_debug_log(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(debug_log, 0664, show_debug_log, store_debug_log);

static int spm_probe(struct platform_device *pdev)
{
	int ret;

	ret = device_create_file(&(pdev->dev), &dev_attr_debug_log);

	return 0;
}

static int spm_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id spm_of_ids[] = {
	{.compatible = "mediatek,SLEEP",},
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

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_PM
static int spm_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

#if 0 /* Avoid race condition between Suspend sync and Idle async IPI cmd */
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct spm_data spm_d;
	int ret;
	unsigned long flags;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#endif

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	case PM_SUSPEND_PREPARE:
#if 0 /* Avoid race condition between Suspend sync and Idle async IPI cmd */

		spin_lock_irqsave(&__spm_lock, flags);
		ret = spm_to_sspm_command(SPM_SUSPEND_PREPARE, &spm_d);
		spin_unlock_irqrestore(&__spm_lock, flags);
		if (ret < 0) {
			printk_deferred("[name:spm&]#@# %s(%d) PM_SUSPEND_PREPARE return %d!!!\n",
				__func__, __LINE__, ret);
			return NOTIFY_BAD;
		}
#endif
		printk_deferred(
		"[name:spm&][SPM] PM: suspend entry %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
#if 0 /* Avoid race condition between Suspend sync and Idle async IPI cmd */

		spin_lock_irqsave(&__spm_lock, flags);
		ret = spm_to_sspm_command(SPM_POST_SUSPEND, &spm_d);
		spin_unlock_irqrestore(&__spm_lock, flags);
		if (ret < 0) {
			printk_deferred("[name:spm&]#@# %s(%d) PM_POST_SUSPEND return %d!!!\n",
				__func__, __LINE__, ret);
			return NOTIFY_BAD;
		}

#endif
		printk_deferred(
		"[name:spm&][SPM] PM: suspend exit %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		return NOTIFY_DONE;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	}
	return NOTIFY_OK;
}

static struct notifier_block spm_pm_notifier_func = {
	.notifier_call = spm_pm_event,
	.priority = 0,
};
#endif /* CONFIG_PM */
#endif /* CONFIG_FPGA_EARLY_PORTING */

static const struct mtk_idle_sysfs_op spm_system_stats_fops = {
	.fs_read = get_spm_system_stats,
};

static const struct mtk_idle_sysfs_op spm_subsystem_stats_fops = {
	.fs_read = get_spm_subsystem_stats,
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

static const struct mtk_idle_sysfs_op spm_lp_ratio_timer_enable_fops = {
	.fs_read = get_spm_lp_ratio_timer_enable,
	.fs_write = set_spm_lp_ratio_timer_enable,
};

static const struct mtk_idle_sysfs_op spm_resource_req_timer_enable_fops = {
	.fs_read = get_spm_resource_req_timer_enable,
	.fs_write = set_spm_resource_req_timer_enable,
};

static const struct mtk_idle_sysfs_op network_traffic_fops = {
	.fs_write = set_network_traffic,
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
#if defined(CONFIG_PM)
	if (spm_fs_init() != 0)
		r = -EPERM;
#endif

	/* implemented in mtk_spm_dram.c */
	//spm_do_dram_config_check();

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
		mtk_idle_sysfs_entry_func_node_add("system_stats", 0444
			, &spm_system_stats_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("subsystem_stats", 0444
			, &spm_subsystem_stats_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("spm_sleep_count", 0444
			, &spm_sleep_count_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("spm_last_wakeup_src", 0444
			, &spm_last_wakeup_src_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("spm_last_debug_flag", 0444
			, &spm_last_debug_flag_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("spmfw_version", 0444
			, &spm_spmfw_version_fops, &pParent2ND, NULL);
		mtk_idle_sysfs_entry_func_node_add("spm_dump_lp_ratio_enable",
			0444, &spm_lp_ratio_timer_enable_fops, &pParent2ND,
			NULL);
		mtk_idle_sysfs_entry_func_node_add("spm_dump_res_req_enable",
			0444, &spm_resource_req_timer_enable_fops, &pParent2ND,
			NULL);
		mtk_idle_sysfs_entry_func_node_add("network_traffic", 0444
			, &network_traffic_fops, &pParent2ND, NULL);
	}


#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_PM
	ret = register_pm_notifier(&spm_pm_notifier_func);
	if (ret) {
		pr_debug("Failed to register PM notifier.\n");
		return ret;
	}
#endif /* CONFIG_PM */
#endif /* CONFIG_FPGA_EARLY_PORTING */
#if MTK_FEATURE_ENABLE_KICK_SPMFW
	SMC_CALL(ARGS, SPM_ARGS_SPMFW_IDX_KICK, spm_get_spmfw_idx(),
		dram_steps_freq(0));
#endif

	spm_vcorefs_init();

	return 0;
}

int mtk_spm_init(void)
{
	int ret;

	mtk_cpuidle_init();
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
/* SPARE_ACK_MASK removed, srcclkeni can not ctrl xo_wcn at MT6779 */
#if 0
	else if (register_index == SPM_SPARE_ACK_MASK)
		return spm_read(SPARE_ACK_MASK);
#endif
	else
		return 0;
}

MODULE_DESCRIPTION("SPM Driver v0.1");
