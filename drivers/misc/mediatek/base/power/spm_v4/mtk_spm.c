/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/atomic.h>

#include <mtk_spm_early_porting.h>

#include <mtk_sleep.h>
#include <mtk_spm_idle.h>
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING) && \
	!defined(CONFIG_MACH_MT6739) && \
	!defined(CONFIG_MACH_MT6771)
#include <mtk_pmic_api_buck.h>
#elif defined(CONFIG_MACH_MT6739)
#include "pmic_api_buck.h"
#endif
#include <upmu_sw.h>
#if !defined(CONFIG_FPGA_EARLY_PORTING)
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
#include <mtk_spm_vcore_dvfs.h>
#endif
#endif /* CONFIG_FPGA_EARLY_PORTING */

#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
//#include <linux/irqchip/mtk-eic.h>
#include <linux/suspend.h>
#include <mt-plat/mtk_secure_api.h>
#ifdef CONFIG_MTK_WD_KICKER
#include <mach/wd_api.h>
#endif
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
#include <linux/pm_wakeup.h>
#endif

#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <mtk_spm_misc.h>
#include <mtk_spm_resource_req_internal.h>
#include <mtk_spm_internal.h>
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING) && defined(CONFIG_MTK_SYS_CIRQ)
#include <mt-plat/mtk_cirq.h>
#endif

//#include <trace/events/mtk_events.h>

#include <mtk_lp_sysfs.h>
#include <mtk_lp_kernfs.h>
#include "mtk_idle_sysfs.h"

int __spmfw_idx = -1;
int spm_for_gps_flag;

void __iomem *spm_base;
void __iomem *sleep_reg_md_base;
u32 spm_irq_0;

#if defined(CONFIG_MACH_MT6763)
#define NF_EDGE_TRIG_IRQS	7
#elif defined(CONFIG_MACH_MT6739)
#define NF_EDGE_TRIG_IRQS	2
#elif defined(CONFIG_MACH_MT6771)
#define NF_EDGE_TRIG_IRQS	3 /* remove auxadc (lowbattery_irq_b) */
#endif
static u32 edge_trig_irqs[NF_EDGE_TRIG_IRQS];

/**************************************
 * Config and Parameter
 **************************************/

/**************************************
 * Define and Declare
 **************************************/
struct spm_irq_desc {
	unsigned int irq;
	irq_handler_t handler;
};

static twam_handler_t spm_twam_handler;

void __attribute__((weak)) spm_sodi3_init(void)
{
	spm_crit2("NO %s !!!\n", __func__);
}

void __attribute__((weak)) spm_sodi_init(void)
{
	spm_crit2("NO %s !!!\n", __func__);
}

void __attribute__((weak)) spm_deepidle_init(void)
{
	spm_crit2("NO %s !!!\n", __func__);
}

void __attribute__((weak)) spm_vcorefs_init(void)
{
	spm_crit2("NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_power_gs_t_dump_suspend(int count, ...)
{
	spm_crit2("NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_power_gs_t_dump_dpidle(int count, ...)
{
	spm_crit2("NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_power_gs_t_dump_sodi3(int count, ...)
{
	spm_crit2("NO %s !!!\n", __func__);
}

void __attribute__((weak)) set_wakeup_sources(u32 *list, u32 num_events)
{
	spm_crit2("NO %s !!!\n", __func__);
}

int __attribute__((weak)) spm_fs_init(void)
{
	spm_crit2("NO %s !!!\n", __func__);
	return 0;
}

char *__attribute__((weak)) spm_vcorefs_dump_dvfs_regs(char *p)
{
	return NULL;
}

ssize_t __attribute__((weak))
get_spm_last_wakeup_src(char *ToUserBuf, size_t sz, void *priv) { return 0; }

ssize_t __attribute__((weak))
get_spm_sleep_count(char *ToUserBuf, size_t sz, void *priv) { return 0; }



/**************************************
 * Init and IRQ Function
 **************************************/
static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	u32 isr;
	unsigned long flags;
	struct twam_sig twamsig;

	spin_lock_irqsave(&__spm_lock, flags);
	/* get ISR status */
	isr = spm_read(SPM_IRQ_STA);
	if (isr & ISRS_TWAM) {
		twamsig.sig0 = spm_read(SPM_TWAM_LAST_STA0);
		twamsig.sig1 = spm_read(SPM_TWAM_LAST_STA1);
		twamsig.sig2 = spm_read(SPM_TWAM_LAST_STA2);
		twamsig.sig3 = spm_read(SPM_TWAM_LAST_STA3);
		udelay(40); /* delay 1T @ 32K */
	}

	/* clean ISR status */
	SMC_CALL(MTK_SIP_KERNEL_SPM_IRQ0_HANDLER, isr, 0, 0);
	spin_unlock_irqrestore(&__spm_lock, flags);

	if (isr & (ISRS_SW_INT1)) {
		spm_err("IRQ0 (ISRS_SW_INT1) HANDLER SHOULD NOT BE EXECUTED (0x%x)\n",
			isr);
#if !defined(CONFIG_FPGA_EARLY_PORTING)
		spm_vcorefs_dump_dvfs_regs(NULL);
#endif
		return IRQ_HANDLED;
	}

	if ((isr & ISRS_TWAM) && spm_twam_handler)
		spm_twam_handler(&twamsig);

	if (isr & (ISRS_SW_INT0 | ISRS_PCM_RETURN))
		spm_err("IRQ0 HANDLER SHOULD NOT BE EXECUTED (0x%x)\n", isr);

	return IRQ_HANDLED;
}

static int spm_irq_register(void)
{
	int i, err, r = 0;
	struct spm_irq_desc irqdesc[] = {
		{.irq = 0, .handler = spm_irq0_handler,}
	};
	irqdesc[0].irq = SPM_IRQ0_ID;
	for (i = 0; i < ARRAY_SIZE(irqdesc); i++) {
		if (cpu_present(i)) {
			err = request_irq(irqdesc[i].irq, irqdesc[i].handler,
					IRQF_TRIGGER_LOW |
					IRQF_NO_SUSPEND |
					IRQF_PERCPU,
					"SPM", NULL);
			if (err) {
				spm_err("FAILED TO REQUEST IRQ%d (%d)\n",
					i, err);
				r = -EPERM;
			}
		}
	}
	return r;
}

static void spm_register_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node)
		spm_err("find sleep node failed\n");
	spm_base = of_iomap(node, 0);
	if (!spm_base)
		spm_err("base spm_base failed\n");

	spm_irq_0 = irq_of_parse_and_map(node, 0);
	if (!spm_irq_0)
		spm_err("get spm_irq_0 failed\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep_reg_md");
	if (!node)
		spm_err("find sleep_reg_md node failed\n");
	sleep_reg_md_base = of_iomap(node, 0);
	if (!sleep_reg_md_base)
		spm_err("base sleep_reg_md_base failed\n");

	spm_err("spm_base = %p, sleep_reg_md_base = %p, spm_irq_0 = %d\n",
		spm_base, sleep_reg_md_base, spm_irq_0);

#if defined(CONFIG_MACH_MT6763)
	/* mipi_apb_tx_irq */
	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	if (!node) {
		spm_err("find mediatek,infracfg_ao syscon node failed\n");
	} else {
		edge_trig_irqs[1] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[1])
			spm_err("get mediatek,infracfg_ao syscon failed\n");
	}

	/* kp_irq_b */
	node = of_find_compatible_node(NULL, NULL, "mediatek,kp");
	if (!node) {
		spm_err("find mediatek,kp node failed\n");
	} else {
		edge_trig_irqs[2] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[2])
			spm_err("get mediatek,kp failed\n");
	}

	/* conn_wdt_irq_b */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6763-consys");
	if (!node) {
		spm_err("find mediatek,mt6763-consys node failed\n");
	} else {
		edge_trig_irqs[3] = irq_of_parse_and_map(node, 1);
		if (!edge_trig_irqs[3])
			spm_err("get mediatek,mt6763-consys failed\n");
	}

	/* md_wdt_int_ao */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdcldma");
	if (!node) {
		spm_err("find mdcldma node failed\n");
	} else {
		edge_trig_irqs[5] = irq_of_parse_and_map(node, 3);
		if (!edge_trig_irqs[5])
			spm_err("get mdcldma failed\n");
	}

	/* deprecated: no user used this irq at mt6763 */
#if 0
	/* lowbattery_irq_b */
	node = of_find_compatible_node(NULL, NULL, "mediatek,auxadc");
	if (!node) {
		spm_err("find auxadc node failed\n");
	} else {
		edge_trig_irqs[6] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[6])
			spm_err("get auxadc failed\n");
	}
#endif
#elif defined(CONFIG_MACH_MT6739)
	/* kp_irq_b */
	node = of_find_compatible_node(NULL, NULL, "mediatek,kp");
	if (!node) {
		spm_err("find kp node failed\n");
	} else {
		edge_trig_irqs[0] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[0])
			spm_err("get kp failed\n");
	}

	/* md_wdt_irq_b */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdcldma");
	if (!node) {
		spm_err("find mdcldma node failed\n");
	} else {
		edge_trig_irqs[1] = irq_of_parse_and_map(node, 3);
		if (!edge_trig_irqs[1])
			spm_err("get mdcldma failed\n");
	}
#elif defined(CONFIG_MACH_MT6771)
	/* mediatek,infracfg_ao */
	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	if (!node) {
		spm_err("find mediatek,infracfg_ao syscon node failed\n");
	} else {
		edge_trig_irqs[0] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[0])
			spm_err("get mediatek,infracfg_ao syscon failed\n");
	}

	/* mediatek,kp */
	node = of_find_compatible_node(NULL, NULL, "mediatek,kp");
	if (!node) {
		spm_err("find mediatek,kp node failed\n");
	} else {
		edge_trig_irqs[1] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[1])
			spm_err("get mediatek,kp failed\n");
	}

	/* mediatek,mdcldma */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdcldma");
	if (!node) {
		spm_err("find mdcldma node failed\n");
	} else {
		edge_trig_irqs[2] = irq_of_parse_and_map(node, 3);
		if (!edge_trig_irqs[2])
			spm_err("get mdcldma failed\n");
	}

	/* mediatek,auxadc */
	/* remove auxadc (lowbattery_irq_b)
	node = of_find_compatible_node(NULL, NULL, "mediatek,auxadc");
	if (!node) {
		spm_err("find mediatek,auxadc node failed\n");
	} else {
		edge_trig_irqs[3] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[3])
			spm_err("get mediatek,auxadc failed\n");
	}
	*/
#endif

#if defined(CONFIG_MACH_MT6763)
	spm_err("edge trigger irqs: %d, %d, %d, %d, %d, %d, %d\n",
		 edge_trig_irqs[0],
		 edge_trig_irqs[1],
		 edge_trig_irqs[2],
		 edge_trig_irqs[3],
		 edge_trig_irqs[4],
		 edge_trig_irqs[5],
		 edge_trig_irqs[6]);
#elif defined(CONFIG_MACH_MT6739)
	spm_err("edge trigger irqs: %d, %d\n",
		 edge_trig_irqs[0],
		 edge_trig_irqs[1]);
#elif defined(CONFIG_MACH_MT6771)
	spm_err("edge trigger irqs: %d, %d, %d\n",
		 edge_trig_irqs[0],
		 edge_trig_irqs[1],
		 edge_trig_irqs[2]);
	//	 edge_trig_irqs[3]); /* remove auxadc (lowbattery_irq_b) */
#endif

#if defined(CONFIG_MACH_MT6739)
	spm_set_dummy_read_addr(true);
#endif /* CONFIG_MACH_MT6739 */
}

static int local_spm_load_firmware_status = -1;
int spm_load_firmware_status(void)
{
	if (local_spm_load_firmware_status == -1)
		local_spm_load_firmware_status =
			SMC_CALL(MTK_SIP_KERNEL_SPM_FIRMWARE_STATUS, 0, 0, 0);
	return local_spm_load_firmware_status;
}

static const struct mtk_lp_sysfs_op spm_sleep_count_fops = {
	.fs_read = get_spm_sleep_count,
};

static const struct mtk_lp_sysfs_op spm_last_wakeup_src_fops = {
	.fs_read = get_spm_last_wakeup_src,
};

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_PM
static int spm_pm_event(struct notifier_block *notifier,
			unsigned long pm_event,
			void *unused)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct spm_data spm_d;
	int ret;
	unsigned long flags;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	case PM_SUSPEND_PREPARE:
		spin_lock_irqsave(&__spm_lock, flags);
		ret = spm_to_sspm_command(SPM_SUSPEND_PREPARE, &spm_d);
		spin_unlock_irqrestore(&__spm_lock, flags);
		if (ret < 0) {
			printk_deferred("[name:spm&]#@# %s(%d) PM_SUSPEND_PREPARE return %d!!!\n",
			       __func__, __LINE__, ret);
			return NOTIFY_BAD;
		}
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		spin_lock_irqsave(&__spm_lock, flags);
		ret = spm_to_sspm_command(SPM_POST_SUSPEND, &spm_d);
		spin_unlock_irqrestore(&__spm_lock, flags);
		if (ret < 0) {
			printk_deferred("[name:spm&]#@# %s(%d) PM_POST_SUSPEND return %d!!!\n",
			       __func__, __LINE__, ret);
			return NOTIFY_BAD;
		}
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

static ssize_t show_debug_log(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	char *p = buf;

	p += sprintf(p, "for test\n");

	return p - buf;
}

static ssize_t store_debug_log(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t size)
{
	return size;
}

/* 644 */
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

static struct platform_device *pspmdev;

/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
struct wakeup_source *spm_wakelock;

void spm_pm_stay_awake(int sec)
{
	__pm_wakeup_event(spm_wakelock, jiffies_to_msecs(HZ * sec));
};
#endif

#ifdef CONFIG_MTK_DRAMC
#if defined(CONFIG_MACH_MT6763)
static void __spm_check_dram_type(void)
{
	int ddr_type = get_ddr_type();
	int emi_ch_num = get_emi_ch_num();

	if (ddr_type == TYPE_LPDDR4X && emi_ch_num == 2)
		__spmfw_idx = SPMFW_LP4X_2CH;
	else if (ddr_type == TYPE_LPDDR4X && emi_ch_num == 1)
		__spmfw_idx = SPMFW_LP4X_1CH;
	else if (ddr_type == TYPE_LPDDR3 && emi_ch_num == 1)
		__spmfw_idx = SPMFW_LP3_1CH;
	printk_deferred("[name:spm&]#@# %s(%d) __spmfw_idx 0x%x\n",
		__func__, __LINE__, __spmfw_idx);
};
#elif defined(CONFIG_MACH_MT6771)
static void __spm_check_dram_type(void)
{
	int ddr_type = get_ddr_type();
	int ddr_hz = dram_steps_freq(0);

	if (ddr_type == TYPE_LPDDR4X && ddr_hz == 3600)
		__spmfw_idx = SPMFW_LP4X_2CH_3733;
	else if (ddr_type == TYPE_LPDDR4X && ddr_hz == 3200)
		__spmfw_idx = SPMFW_LP4X_2CH_3200;
	else if (ddr_type == TYPE_LPDDR4 && ddr_hz == 3600)
		__spmfw_idx = SPMFW_LP4X_2CH_3733;
	else if (ddr_type == TYPE_LPDDR4 && ddr_hz == 3200)
		__spmfw_idx = SPMFW_LP4X_2CH_3200;
	else if (ddr_type == TYPE_LPDDR3 && ddr_hz == 1866)
		__spmfw_idx = SPMFW_LP3_1CH_1866;
	else if (ddr_type == TYPE_LPDDR4 && ddr_hz == 2400)
		__spmfw_idx = SPMFW_LP4_2CH_2400;
	printk_deferred("[name:spm&]#@# %s(%d) __spmfw_idx 0x%x (type:%d freq:%d)\n",
		__func__, __LINE__, __spmfw_idx, ddr_type, ddr_hz);
};
#elif defined(CONFIG_MACH_MT6739)
static void __spm_check_dram_type(void)
{
	__spmfw_idx = 0;
}
#endif /* CONFIG_MTK_DRAMC */
#else
static void __spm_check_dram_type(void)
{
	__spmfw_idx = 0;
}
#endif /* CONFIG_MTK_DRAMC */

int __spm_get_dram_type(void)
{
	if (__spmfw_idx == -1) {
		__spmfw_idx++;
		__spm_check_dram_type();
	}

	return __spmfw_idx;
}

int __init spm_module_init(void)
{
	int r = 0;
	int ret = -1;
	int is_ext_buck = 0;

	int i;
	unsigned int irq_type;

	struct mtk_lp_sysfs_handle *pParent = NULL;
	struct mtk_lp_sysfs_handle entry_spm;

#if defined(CONFIG_MACH_MT6771)
	struct device_node *sleep_node;
	const char *pMethod = NULL;
#endif

#if defined(CONFIG_MACH_MT6739)
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
	spm_crit2("pmic_ver %d\n", PMIC_LP_CHIP_VER());
#endif
#endif

/* TODO: fix */
/* #if !defined(SPM_K414_EARLY_PORTING)
	wakeup_source_init(&spm_wakelock, "spm");
#endif */
	spm_wakelock = wakeup_source_register(NULL, "spm");
	if (spm_wakelock == NULL) {
		pr_debug("fail to request spm_wakelock\n");
		return ret;
	}
#if defined(CONFIG_MACH_MT6771)
	else {
		sleep_node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");

		pr_info("success to request spm_wakelock\n");

		if (sleep_node) {
			of_property_read_string(sleep_node, "suspend-method", &pMethod);

			if (pMethod) {
				if (!strcmp(pMethod, "disable"))
					__pm_stay_awake(spm_wakelock);
			}
		} else
			__pm_stay_awake(spm_wakelock);
	}
#endif
	spm_register_init();
	if (spm_irq_register() != 0)
		r = -EPERM;
#if defined(CONFIG_PM)
	if (spm_fs_init() != 0)
		r = -EPERM;
#endif

	/* Note: Initialize irq type to avoid pending irqs */
	for (i = 0; i < NF_EDGE_TRIG_IRQS; i++) {
		if (edge_trig_irqs[i]) {
			irq_type = irq_get_trigger_type(edge_trig_irqs[i]);
			irq_set_irq_type(edge_trig_irqs[i], irq_type);
		}
	}

#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
	set_wakeup_sources(edge_trig_irqs, NF_EDGE_TRIG_IRQS);
#endif

	spm_sodi3_init();
	spm_sodi_init();
	spm_deepidle_init();

#ifdef CONFIG_MTK_DRAMC
	/* get __spmfw_idx */
	__spm_check_dram_type();
#endif /* CONFIG_MTK_DRAMC */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MTK_DRAMC
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
	if (spm_golden_setting_cmp(1) != 0)
		aee_kernel_warning("SPM Warning",
			"SPM Warning, dram golden setting mismach");
#else
	if (spm_golden_setting_cmp(1) != 0)
		spm_crit2("SPM Warning, dram golden setting mismach");
#endif
#endif /* CONFIG_MTK_DRAMC */
#endif /* CONFIG_FPGA_EARLY_PORTING */

	spm_phypll_mode_check();

	ret = platform_driver_register(&spm_dev_drv);
	if (ret) {
		printk_deferred("[name:spm&]fail to register platform driver\n");
		return ret;
	}

	pspmdev = platform_device_register_simple("spm", -1, NULL, 0);
	if (IS_ERR(pspmdev)) {
		printk_deferred("[name:spm&]Failed to register platform device.\n");
		return -EINVAL;
	}

	mtk_idle_sysfs_root_entry_create();

	if (mtk_idle_sysfs_entry_root_get(&pParent) == 0) {
		mtk_lp_sysfs_entry_func_create("spm", 0444,
			pParent, &entry_spm);
		mtk_lp_sysfs_entry_func_node_add("spm_sleep_count", 0444,
			&spm_sleep_count_fops, &entry_spm, NULL);
		mtk_lp_sysfs_entry_func_node_add("spm_last_wakeup_src", 0444,
			&spm_last_wakeup_src_fops, &entry_spm, NULL);
	}

	/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
	spm_resource_req_debugfs_init(&entry_spm);
#endif
	spm_suspend_debugfs_init(&entry_spm);


#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_PM
	ret = register_pm_notifier(&spm_pm_notifier_func);
	if (ret) {
		printk_deferred("[name:spm&]Failed to register PM notifier.\n");
		return ret;
	}
#endif /* CONFIG_PM */
#endif /* CONFIG_FPGA_EARLY_PORTING */

#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#if defined(CONFIG_MACH_MT6763)
	is_ext_buck = is_ext_buck_exist();
#endif
#endif
	printk_deferred("[name:spm&]#@# %s(%d) is_ext_buck_exist() 0x%x\n",
		__func__, __LINE__, is_ext_buck);
	SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS, SPM_ARGS_SPMFW_IDX,
		 __spm_get_dram_type(), is_ext_buck);

	spm_vcorefs_init();

	return 0;
}

/**************************************
 * TWAM Control API
 **************************************/
static unsigned int idle_sel;
void spm_twam_set_idle_select(unsigned int sel)
{
	idle_sel = sel & 0x3;
}
EXPORT_SYMBOL(spm_twam_set_idle_select);

static unsigned int window_len;
void spm_twam_set_window_length(unsigned int len)
{
	window_len = len;
}
EXPORT_SYMBOL(spm_twam_set_window_length);

static struct twam_sig mon_type;
void spm_twam_set_mon_type(struct twam_sig *mon)
{
	if (mon) {
		mon_type.sig0 = mon->sig0 & 0x3;
		mon_type.sig1 = mon->sig1 & 0x3;
		mon_type.sig2 = mon->sig2 & 0x3;
		mon_type.sig3 = mon->sig3 & 0x3;
	}
}
EXPORT_SYMBOL(spm_twam_set_mon_type);

void spm_twam_register_handler(twam_handler_t handler)
{
	spm_twam_handler = handler;
}
EXPORT_SYMBOL(spm_twam_register_handler);

void spm_twam_enable_monitor(const struct twam_sig *twamsig,
			     bool speed_mode)
{
	u32 sig0 = 0, sig1 = 0, sig2 = 0, sig3 = 0;
	u32 mon0 = 0, mon1 = 0, mon2 = 0, mon3 = 0;
	unsigned int sel;
	unsigned int length;
	unsigned long flags;

	if (twamsig) {
		sig0 = twamsig->sig0 & 0x1f;
		sig1 = twamsig->sig1 & 0x1f;
		sig2 = twamsig->sig2 & 0x1f;
		sig3 = twamsig->sig3 & 0x1f;
	}

	/* Idle selection */
	sel = idle_sel;
	/* Window length */
	length = window_len;
	/* Monitor type */
	mon0 = mon_type.sig0 & 0x3;
	mon1 = mon_type.sig1 & 0x3;
	mon2 = mon_type.sig2 & 0x3;
	mon3 = mon_type.sig3 & 0x3;

	spin_lock_irqsave(&__spm_lock, flags);
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) & ~ISRM_TWAM);
	/* Signal Select */
	spm_write(SPM_TWAM_IDLE_SEL, sel);
	/* Monitor Control */
	spm_write(SPM_TWAM_CON,
		  (sig3 << 27) |
		  (sig2 << 22) |
		  (sig1 << 17) |
		  (sig0 << 12) |
		  (mon3 << 10) |
		  (mon2 << 8) |
		  (mon1 << 6) |
		  (mon0 << 4) |
		  (speed_mode ? TWAM_SPEED_MODE_ENABLE_LSB : 0) |
		  TWAM_ENABLE_LSB);
	/* Window Length */
	/* 0x13DDF0 for 50ms, 0x65B8 for 1ms, */
	/* 0x1458 for 200us, 0xA2C for 100us */
	/* in speed mode (26 MHz) */
	spm_write(SPM_TWAM_WINDOW_LEN, length);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_debug("enable TWAM for signal %u, %u, %u, %u (%u)\n",
		  sig0, sig1, sig2, sig3, speed_mode);
}
EXPORT_SYMBOL(spm_twam_enable_monitor);

void spm_twam_disable_monitor(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	spm_write(SPM_TWAM_CON, spm_read(SPM_TWAM_CON) & ~TWAM_ENABLE_LSB);
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) | ISRM_TWAM);
	spm_write(SPM_IRQ_STA, ISRC_TWAM);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_debug("disable TWAM\n");
}
EXPORT_SYMBOL(spm_twam_disable_monitor);

/**************************************
 * SPM Golden Seting API(MEMPLL Control, DRAMC)
 **************************************/
#ifdef CONFIG_MTK_DRAMC
struct ddrphy_golden_cfg {
	u32 base;
	u32 offset;
	u32 mask;
	u32 value;
};

#if defined(CONFIG_MACH_MT6763)
static struct ddrphy_golden_cfg ddrphy_setting_lp4_2ch[] = {
	{DRAMC_AO_CHA, 0x038, 0xc0000027, 0xc0000007},
	{DRAMC_AO_CHB, 0x038, 0xc0000027, 0xc0000007},
	/* LP4-2ch +++ */
	{PHY_AO_CHA, 0x284, 0x001bff00, 0x00000100},
	{PHY_AO_CHB, 0x284, 0x001bff00, 0x00000100},
	{PHY_AO_CHA, 0xc20, 0xfff00000, 0x00200000},
	{PHY_AO_CHB, 0xc20, 0xfff00000, 0x00200000},
	{PHY_AO_CHA, 0xca0, 0xfff00000, 0x00200000},
	{PHY_AO_CHB, 0xca0, 0xfff00000, 0x00200000},
	{PHY_AO_CHA, 0xd20, 0xfff00000, 0x00000000},
	{PHY_AO_CHB, 0xd20, 0xfff00000, 0x00000000},
	{PHY_AO_CHA, 0x298, 0x00770000, 0x00770000},
	{PHY_AO_CHB, 0x298, 0x00770000, 0x00770000},
	/* LP4-2ch --- */
	{PHY_AO_CHA, 0x2a8, 0x0c000000, 0x00000000},
	{PHY_AO_CHB, 0x2a8, 0x0c000000, 0x00000000},
	/* {PHY_AO_CHA, 0xc00, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHA, 0xc80, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHB, 0xc00, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHB, 0xc80, 0x0001000f, 0x0001000f}, */
};

static struct ddrphy_golden_cfg ddrphy_setting_lp4_1ch[] = {
	{DRAMC_AO_CHA, 0x038, 0xc0000027, 0xc0000007},
	{DRAMC_AO_CHB, 0x038, 0xc0000027, 0xc0000007},
	/* LP4-1ch +++ */
	{PHY_AO_CHA, 0x284, 0x001bff00, 0x00100000},
	{PHY_AO_CHB, 0x284, 0x001bff00, 0x00000100},
	{PHY_AO_CHA, 0xc20, 0xfff00000, 0x00000000},
	{PHY_AO_CHB, 0xc20, 0xfff00000, 0x00200000},
	{PHY_AO_CHA, 0xca0, 0xfff00000, 0x00000000},
	{PHY_AO_CHB, 0xca0, 0xfff00000, 0x00200000},
	{PHY_AO_CHA, 0xd20, 0xfff00000, 0x00000000},
	{PHY_AO_CHB, 0xd20, 0xfff00000, 0x00200000},
	{PHY_AO_CHA, 0x298, 0x00770000, 0x00000000},
	{PHY_AO_CHB, 0x298, 0x00770000, 0x00770000},
	/* LP4-1ch --- */
	{PHY_AO_CHA, 0x2a8, 0x0c000000, 0x00000000},
	{PHY_AO_CHB, 0x2a8, 0x0c000000, 0x00000000},
	/* {PHY_AO_CHA, 0xc00, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHA, 0xc80, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHB, 0xc00, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHB, 0xc80, 0x0001000f, 0x0001000f}, */
};

static struct ddrphy_golden_cfg ddrphy_setting_lp3_1ch[] = {
	{DRAMC_AO_CHA, 0x038, 0xc0000027, 0xc0000007},
	{DRAMC_AO_CHB, 0x038, 0xc0000027, 0xc0000007},
	/* LP3-1ch +++ */
	{PHY_AO_CHA, 0x284, 0x001bff00, 0x00000100},
	{PHY_AO_CHB, 0x284, 0x001bff00, 0x00100000},
	{PHY_AO_CHA, 0xc20, 0xfff00000, 0x00000000},
	{PHY_AO_CHB, 0xc20, 0xfff00000, 0x00200000},
	{PHY_AO_CHA, 0xca0, 0xfff00000, 0x00200000},
	{PHY_AO_CHB, 0xca0, 0xfff00000, 0x00200000},
	{PHY_AO_CHA, 0xd20, 0xfff00000, 0x00000000},
	{PHY_AO_CHB, 0xd20, 0xfff00000, 0x00200000},
	{PHY_AO_CHA, 0x298, 0x00770000, 0x00570000},
	{PHY_AO_CHB, 0x298, 0x00770000, 0x00070000},
	/* LP3-1ch --- */
	{PHY_AO_CHA, 0x2a8, 0x0c000000, 0x00000000},
	{PHY_AO_CHB, 0x2a8, 0x0c000000, 0x00000000},
	/* {PHY_AO_CHA, 0xc00, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHA, 0xc80, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHB, 0xc00, 0x0001000f, 0x0001000f}, */
	/* {PHY_AO_CHB, 0xc80, 0x0001000f, 0x0001000f}, */
};
#elif defined(CONFIG_MACH_MT6771)
static struct ddrphy_golden_cfg ddrphy_setting_lp4_2ch[] = {
	{DRAMC_AO_CHA, 0x038, 0xc0000027, 0xc0000007},
	{DRAMC_AO_CHB, 0x038, 0xc0000027, 0xc0000007},
	{PHY_AO_CHA, 0x0284, 0x001bff00, 0x00000100},
	{PHY_AO_CHB, 0x0284, 0x001bff00, 0x00000100},
	{PHY_AO_CHA, 0x0c20, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x0c20, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x1120, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x1120, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x1620, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x1620, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x0ca0, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x0ca0, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x11a0, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x11a0, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x16a0, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x16a0, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x0d20, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x0d20, 0xfff80000, 0x00000000},
	{PHY_AO_CHA, 0x1220, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x1220, 0xfff80000, 0x00000000},
	{PHY_AO_CHA, 0x1720, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x1720, 0xfff80000, 0x00000000},
	{PHY_AO_CHA, 0x0298, 0x00770000, 0x00770000},
	{PHY_AO_CHB, 0x0298, 0x00770000, 0x00770000},
	{PHY_AO_CHA, 0x02a8, 0x0c000000, 0x00000000},
	{PHY_AO_CHB, 0x02a8, 0x0c000000, 0x00000000},
	{PHY_AO_CHA, 0x028c, 0xffffffff, 0x806003be},
	{PHY_AO_CHB, 0x028c, 0xffffffff, 0x806003be},
	{PHY_AO_CHA, 0x0084, 0x00100000, 0x00000000},
	{PHY_AO_CHB, 0x0084, 0x00100000, 0x00000000},
	{PHY_AO_CHA, 0x0104, 0x00100000, 0x00000000},
	{PHY_AO_CHB, 0x0104, 0x00100000, 0x00000000},
	{PHY_AO_CHA, 0x0184, 0x00100000, 0x00000000},
	{PHY_AO_CHB, 0x0184, 0x00100000, 0x00000000},
	{PHY_AO_CHA, 0x0c34, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x0c34, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x0cb4, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x0cb4, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x0d34, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x0d34, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x1134, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x1134, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x11b4, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x11b4, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x1234, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x1234, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x1634, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x1634, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x16b4, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x16b4, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x1734, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x1734, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x0c1c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x0c1c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x0c9c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x0c9c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x0d1c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x0d1c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x111c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x111c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x119c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x119c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x121c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x121c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x161c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x161c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x169c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x169c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x171c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x171c, 0x000e0000, 0x000e0000},
};

static struct ddrphy_golden_cfg ddrphy_setting_lp3_1ch[] = {
	{DRAMC_AO_CHA, 0x038, 0xc0000027, 0xc0000007},
	{DRAMC_AO_CHB, 0x038, 0xc0000027, 0xc0000007},
	{PHY_AO_CHA, 0x0284, 0x001bff00, 0x00000100},
	{PHY_AO_CHB, 0x0284, 0x001bff00, 0x00100000},
	{PHY_AO_CHA, 0x0c20, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x0c20, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x1120, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x1120, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x1160, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x1160, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x0ca0, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x0ca0, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x11a0, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x11a0, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x16a0, 0xfff80000, 0x00200000},
	{PHY_AO_CHB, 0x16a0, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x0d20, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x0d20, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x1220, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x1220, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x1720, 0xfff80000, 0x00000000},
	{PHY_AO_CHB, 0x1720, 0xfff80000, 0x00200000},
	{PHY_AO_CHA, 0x0298, 0x00770000, 0x00570000},
	{PHY_AO_CHB, 0x0298, 0x00770000, 0x00070000},
	{PHY_AO_CHA, 0x02a8, 0x0c000000, 0x00000000},
	{PHY_AO_CHB, 0x02a8, 0x0c000000, 0x00000000},
	{PHY_AO_CHA, 0x028c, 0xffffffff, 0x806003be},
	{PHY_AO_CHB, 0x028c, 0xffffffff, 0x806003be},
	{PHY_AO_CHA, 0x0084, 0x00100000, 0x00000000},
	{PHY_AO_CHB, 0x0084, 0x00100000, 0x00000000},
	{PHY_AO_CHA, 0x0104, 0x00100000, 0x00000000},
	{PHY_AO_CHB, 0x0104, 0x00100000, 0x00000000},
	{PHY_AO_CHA, 0x0184, 0x00100000, 0x00000000},
	{PHY_AO_CHB, 0x0184, 0x00100000, 0x00000000},
	{PHY_AO_CHA, 0x0c34, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x0c34, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x0cb4, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x0cb4, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x0d34, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x0d34, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x1134, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x1134, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x11b4, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x11b4, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x1234, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x1234, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x1634, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x1634, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x16b4, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x16b4, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x1734, 0x00000001, 0x00000001},
	{PHY_AO_CHB, 0x1734, 0x00000001, 0x00000001},
	{PHY_AO_CHA, 0x0c1c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x0c1c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x0c9c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x0c9c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x0d1c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x0d1c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x111c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x111c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x119c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x119c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x121c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x121c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x161c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x161c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x169c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x169c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHA, 0x171c, 0x000e0000, 0x000e0000},
	{PHY_AO_CHB, 0x171c, 0x000e0000, 0x000e0000},
};
#elif defined(CONFIG_MACH_MT6739)
static struct ddrphy_golden_cfg _ddrphy_setting[] = {
	{PHY_AO_CHA, 0x5c0, 0xffffffff, 0x063C0000},
	{PHY_AO_CHA, 0x5c4, 0xffffffff, 0x00000000},
	{PHY_AO_CHA, 0x5c8, 0xffffffff, 0x0000FC10},
	{PHY_AO_CHA, 0x5cc, 0xffffffff, 0x40101000},
};
#endif

int spm_golden_setting_cmp(bool en)
{
	int i, ddrphy_num, r = 0;
	struct ddrphy_golden_cfg *ddrphy_setting;

	if (!en)
		return r;

#if defined(CONFIG_MACH_MT6763)
	switch (__spm_get_dram_type()) {
	case SPMFW_LP4X_2CH:
		ddrphy_setting = ddrphy_setting_lp4_2ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp4_2ch);
		break;
	case SPMFW_LP4X_1CH:
		ddrphy_setting = ddrphy_setting_lp4_1ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp4_1ch);
		break;
	case SPMFW_LP3_1CH:
		ddrphy_setting = ddrphy_setting_lp3_1ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp3_1ch);
		break;
	default:
		return r;
	}
	/*Compare Dramc Goldeing Setting */
#elif defined(CONFIG_MACH_MT6771)
	switch (__spm_get_dram_type()) {
	case SPMFW_LP4X_2CH_3733:
	case SPMFW_LP4_2CH_2400:
		ddrphy_setting = ddrphy_setting_lp4_2ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp4_2ch);
		break;
	case SPMFW_LP4X_2CH_3200:
		ddrphy_setting = ddrphy_setting_lp4_2ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp4_2ch);
		break;
	case SPMFW_LP3_1CH_1866:
		ddrphy_setting = ddrphy_setting_lp3_1ch;
		ddrphy_num = ARRAY_SIZE(ddrphy_setting_lp3_1ch);
		break;
	default:
		return r;
	}
#elif defined(CONFIG_MACH_MT6739)
	ddrphy_setting = _ddrphy_setting;
	ddrphy_num = ARRAY_SIZE(_ddrphy_setting);
#endif

	for (i = 0; i < ddrphy_num; i++) {
		u32 value;

		value = lpDram_Register_Read(ddrphy_setting[i].base,
					     ddrphy_setting[i].offset);
		if ((value & ddrphy_setting[i].mask) !=
		    ddrphy_setting[i].value) {
			spm_crit2(
"dramc mismatch addr: 0x%.2x, offset: 0x%.3x, mask: 0x%.8x, val: 0x%x, read: 0x%x\n",
				ddrphy_setting[i].base,
				ddrphy_setting[i].offset,
				ddrphy_setting[i].mask,
				ddrphy_setting[i].value, value);
			r = -EPERM;
		}
	}

	return r;

}
#endif /* CONFIG_MTK_DRAMC */

void spm_phypll_mode_check(void)
{
#if defined(CONFIG_MACH_MT6771)
	unsigned int val = spm_read(SPM_POWER_ON_VAL0);

	if ((val & (R0_SC_PHYPLL_MODE_SW_PCM | R0_SC_PHYPLL2_MODE_SW_PCM))
			!= R0_SC_PHYPLL_MODE_SW_PCM) {

/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
		aee_kernel_warning("SPM Warning",
			"SPM Warning, Invalid SPM_POWER_ON_VAL0: 0x%08x\n",
			val);
#else
		spm_crit2(
			"SPM Warning, Invalid SPM_POWER_ON_VAL0: 0x%08x\n",
			val);
#endif
	}
#endif
}

#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
void spm_pmic_power_mode(int mode, int force, int lock)
{
	static int prev_mode = -1;

	if (mode < PMIC_PWR_NORMAL || mode >= PMIC_PWR_NUM) {
		printk_deferred("[name:spm&]wrong spm pmic power mode");
		return;
	}

	if (force == 0 && mode == prev_mode)
		return;

#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
	switch (mode) {
	case PMIC_PWR_NORMAL:
		/* nothing */
		break;
	case PMIC_PWR_DEEPIDLE:
#if defined(CONFIG_MACH_MT6763)
		/* nothing */
#elif defined(CONFIG_MACH_MT6739)
		pmic_ldo_vldo28_lp(SRCLKEN2, 1, HW_LP);
#elif defined(CONFIG_MACH_MT6771)
		/* nothing */
#endif
		break;
	case PMIC_PWR_SODI3:
#if defined(CONFIG_MACH_MT6763)
		pmic_ldo_vsram_proc_lp(SRCLKEN0, 1, HW_LP);
		pmic_ldo_vldo28_lp(SRCLKEN0, 0, HW_LP);
		pmic_ldo_vldo28_lp(SW, 1, SW_ON);
#elif defined(CONFIG_MACH_MT6739)
		/* nothing */
#elif defined(CONFIG_MACH_MT6771)
		/* nothing */
#endif
		break;
	case PMIC_PWR_SODI:
#if defined(CONFIG_MACH_MT6763)
		/* nothing */
#elif defined(CONFIG_MACH_MT6739)
		pmic_ldo_vldo28_lp(SRCLKEN2, 0, HW_LP);
		pmic_ldo_vldo28_lp(SW, 1, SW_ON);
#endif
		break;
	case PMIC_PWR_SUSPEND:
#if defined(CONFIG_MACH_MT6763)
		pmic_ldo_vsram_proc_lp(SRCLKEN0, 0, HW_LP);
		pmic_ldo_vsram_proc_lp(SW, 1, SW_OFF);
		pmic_ldo_vldo28_lp(SRCLKEN0, 1, HW_LP);
#elif defined(CONFIG_MACH_MT6739)
		/* nothing */
#elif defined(CONFIG_MACH_MT6771)
		/* nothing */
#endif
		break;
	default:
		printk_deferred(
		"[name:spm&]spm pmic power mode (%d) is not configured\n",
		mode);
	}
#endif

	prev_mode = mode;
}
#endif /* !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) */

void *mt_spm_base_get(void)
{
	return spm_base;
}
EXPORT_SYMBOL(mt_spm_base_get);

void mt_spm_for_gps_only(int enable)
{
	spm_for_gps_flag = !!enable;
#if 0
	printk_deferred("[name:spm&]#@# %s(%d) spm_for_gps_flag %d\n",
		 __func__, __LINE__, spm_for_gps_flag);
#endif
}
EXPORT_SYMBOL(mt_spm_for_gps_only);

/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
void mt_spm_dcs_s1_setting(int enable, int flags)
{
	flags &= 0xf;
	SMC_CALL(MTK_SIP_KERNEL_SPM_DCS_S1, enable, flags, 0);
}
EXPORT_SYMBOL(mt_spm_dcs_s1_setting);
#endif

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT

#define SPM_D_LEN		(8) /* # of cmd + arg0 + arg1 + ... */
/* #define SPM_VCOREFS_D_LEN	(4) *//* # of cmd + arg0 + arg1 + ... */

#include <v1/sspm_ipi.h>

int spm_to_sspm_command_async(u32 cmd, struct spm_data *spm_d)
{
	unsigned int ret = 0;

	switch (cmd) {
	case SPM_DPIDLE_ENTER:
	case SPM_DPIDLE_LEAVE:
	case SPM_ENTER_SODI:
	case SPM_LEAVE_SODI:
	case SPM_ENTER_SODI3:
	case SPM_LEAVE_SODI3:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_async(IPI_ID_SPM_SUSPEND,
					  IPI_OPT_DEFAUT, spm_d, SPM_D_LEN);
		if (ret != 0)
			printk_deferred("[name:spm&]#@# %s(%d) sspm_ipi_send_async(cmd:0x%x) ret %d\n",
			       __func__, __LINE__, cmd, ret);
		break;
	default:
		printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) wrong!!!\n",
		       __func__, __LINE__, cmd);
		break;
	}

	return ret;
}

int spm_to_sspm_command_async_wait(u32 cmd)
{
	int ack_data = 0;
	unsigned int ret = 0;

	switch (cmd) {
	case SPM_DPIDLE_ENTER:
	case SPM_DPIDLE_LEAVE:
	case SPM_ENTER_SODI:
	case SPM_LEAVE_SODI:
	case SPM_ENTER_SODI3:
	case SPM_LEAVE_SODI3:
		ret = sspm_ipi_send_async_wait(IPI_ID_SPM_SUSPEND,
					       IPI_OPT_DEFAUT, &ack_data);

		if (ret != 0) {
			printk_deferred("[name:spm&]#@# %s(%d) sspm_ipi_send_async_wait(cmd:0x%x) ret %d\n",
			       __func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) return %d\n",
			       __func__, __LINE__, cmd, ret);
		}
		break;
	default:
		printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) wrong!!!\n",
		       __func__, __LINE__, cmd);
		break;
	}

	return ret;
}

int spm_to_sspm_command(u32 cmd, struct spm_data *spm_d)
{
	int ack_data = 0;
	unsigned int ret = 0;
	/* struct spm_data _spm_d; */

	switch (cmd) {
	case SPM_SUSPEND:
	case SPM_RESUME:
	case SPM_DPIDLE_ENTER:
	case SPM_DPIDLE_LEAVE:
	case SPM_ENTER_SODI:
	case SPM_ENTER_SODI3:
	case SPM_LEAVE_SODI:
	case SPM_LEAVE_SODI3:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND,
					 IPI_OPT_POLLING,
					 spm_d, SPM_D_LEN,
					 &ack_data, 1);
		if (ret != 0) {
			printk_deferred("[name:spm&]#@# %s(%d) sspm_ipi_send_sync(cmd:0x%x) ret %d\n",
			       __func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) return %d\n",
			       __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_VCORE_PWARP_CMD:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND,
					 IPI_OPT_POLLING, spm_d,
					 SPM_D_LEN, &ack_data, 1);
		if (ret != 0) {
			printk_deferred("[name:spm&]#@# %s(%d) sspm_ipi_send_sync(cmd:0x%x) ret %d\n",
			       __func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) return %d\n",
			       __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_SUSPEND_PREPARE:
	case SPM_POST_SUSPEND:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND,
					 IPI_OPT_POLLING, spm_d,
					 SPM_D_LEN, &ack_data, 1);
		if (ret != 0) {
			printk_deferred("[name:spm&]#@# %s(%d) sspm_ipi_send_sync(cmd:0x%x) ret %d\n",
			       __func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) return %d\n",
			       __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_DPIDLE_PREPARE:
	case SPM_POST_DPIDLE:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND,
					 IPI_OPT_POLLING, spm_d,
					 SPM_D_LEN, &ack_data, 1);
		if (ret != 0) {
			printk_deferred("[name:spm&]#@# %s(%d) sspm_ipi_send_sync(cmd:0x%x) ret %d\n",
			       __func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) return %d\n",
			       __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_SODI_PREPARE:
	case SPM_POST_SODI:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND,
					 IPI_OPT_POLLING, spm_d,
					 SPM_D_LEN, &ack_data, 1);
		if (ret != 0) {
			printk_deferred("[name:spm&]#@# %s(%d) sspm_ipi_send_sync(cmd:0x%x) ret %d\n",
			       __func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) return %d\n",
			       __func__, __LINE__, cmd, ret);
		}
		break;
	default:
		printk_deferred("[name:spm&]#@# %s(%d) cmd(%d) wrong!!!\n",
		       __func__, __LINE__, cmd);
		break;
	}

	return ret;
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

void unmask_edge_trig_irqs_for_cirq(void)
{
	int i;

	for (i = 0; i < NF_EDGE_TRIG_IRQS; i++) {
		if (edge_trig_irqs[i]) {
			/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
			/* unmask edge trigger irqs */
			mt_irq_unmask_for_sleep_ex(edge_trig_irqs[i]);
#endif
		}
	}
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static atomic_t ipi_lock_cnt;

bool is_sspm_ipi_lock_spm(void)
{
	int lock_cnt = -1;
	bool ret = false;

	lock_cnt = atomic_read(&ipi_lock_cnt);

	ret = (lock_cnt == 0) ? false : true;

	return ret;
}

void sspm_ipi_lock_spm_scenario(int start,
				int id,
				int opt,
				const char *name)
{
	if (id == IPI_ID_SPM_SUSPEND)
		return;

	if (id < 0 || id >= IPI_ID_TOTAL)
		return;

	if (start)
		atomic_inc(&ipi_lock_cnt);
	else
		atomic_dec(&ipi_lock_cnt);

	/* FTRACE tag */
	//trace_sspm_ipi(start, id, opt);
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

MODULE_DESCRIPTION("SPM Driver v0.1");
