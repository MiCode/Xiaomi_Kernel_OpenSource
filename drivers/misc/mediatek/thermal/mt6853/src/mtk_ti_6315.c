// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <ap_thermal_limit.h>
#include "mt-plat/mtk_thermal_monitor.h"

/* #define INTR_UT */
/* #define OT_THROTTLE_CPU */
/* #define OT_THROTTLE_GPU */

#define DEFAULT_6315OT_CPU_LIMIT	(800)
#define DEFAULT_6315OT_GPU_LIMIT	(1000)

static struct apthermolmt_user ap_6315ot;
static char *ap_6315ot_log = "ap_6315ot";
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static unsigned int cpu_limit = DEFAULT_6315OT_CPU_LIMIT;
static unsigned int gpu_limit = DEFAULT_6315OT_GPU_LIMIT;

static int virq_tempL_6;
static int virq_tempH_6;
static int virq_tempL_7;
static int virq_tempH_7;
#ifdef INTR_UT
static int virq_rcs0_6;
static int virq_rcs0_7;
#endif

static int tpmic6315_intr_probe(struct platform_device *dev);

static const struct of_device_id tpmic_intr_of_match[] = {
	{ .compatible = "mediatek,mt6315_therm_intr", },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, tpmic_intr_of_match);

static struct platform_driver tpmic_intr_pdrv = {
	.probe = tpmic6315_intr_probe,
	.remove = NULL,
	.driver = {
		.name = "tpmic6315_intr",
		.owner = THIS_MODULE,
		.of_match_table = tpmic_intr_of_match,
	},
};

static irqreturn_t mt6315_6_temp_l_int_handler(int irq, void *data)
{
	pr_info("%s 6315_6 CPU under 110D, irq=%d\n", __func__, irq);
	disable_irq_nosync(virq_tempL_6);
	enable_irq(virq_tempH_6);
#ifdef OT_THROTTLE_CPU
	pr_info("%s release cpu limit\n", __func__);
	apthermolmt_set_cpu_power_limit(&ap_6315ot, 0x7FFFFFFF);
#endif
	return IRQ_HANDLED;
}

static irqreturn_t mt6315_6_temp_h_int_handler(int irq, void *data)
{
	pr_info("%s 6315_6 CPU over 125D, irq=%d\n", __func__, irq);
	disable_irq_nosync(virq_tempH_6);
	enable_irq(virq_tempL_6);
#ifdef OT_THROTTLE_CPU
	pr_info("%s set cpu limit=%d\n", __func__, cpu_limit);
	apthermolmt_set_cpu_power_limit(&ap_6315ot, cpu_limit);
#endif
	return IRQ_HANDLED;
}

#ifdef INTR_UT
static irqreturn_t mt6315_6_rcs0_handler(int irq, void *data)
{
	pr_info("%s irq=%d\n", __func__, irq);
#ifdef OT_THROTTLE_CPU
	pr_info("%s set cpu limit=%d\n", __func__, cpu_limit);
	apthermolmt_set_cpu_power_limit(&ap_6315ot, cpu_limit);
#endif
	return IRQ_HANDLED;
}
#endif

static irqreturn_t mt6315_7_temp_l_int_handler(int irq, void *data)
{
	pr_info("%s 6315_7 GPU under 110D, irq=%d\n", __func__, irq);
	disable_irq_nosync(virq_tempL_7);
	enable_irq(virq_tempH_7);
#ifdef OT_THROTTLE_GPU
	pr_info("%s release gpu limit\n", __func__);
	apthermolmt_set_gpu_power_limit(&ap_6315ot, 0x7FFFFFFF);
#endif
	return IRQ_HANDLED;
}

static irqreturn_t mt6315_7_temp_h_int_handler(int irq, void *data)
{
	pr_info("%s 6315_7 GPU over 125D, irq=%d\n", __func__, irq);
	disable_irq_nosync(virq_tempH_7);
	enable_irq(virq_tempL_7);
#ifdef OT_THROTTLE_GPU
	pr_info("%s set gpu limit=%d\n", __func__, gpu_limit);
	apthermolmt_set_gpu_power_limit(&ap_6315ot, gpu_limit);
#endif
	return IRQ_HANDLED;
}

#ifdef INTR_UT
static irqreturn_t mt6315_7_rcs0_handler(int irq, void *data)
{
	pr_info("%s irq=%d\n", __func__, irq);
#ifdef OT_THROTTLE_GPU
	pr_info("%s set gpu limit=%d\n", __func__, gpu_limit);
	apthermolmt_set_gpu_power_limit(&ap_6315ot, gpu_limit);
#endif
	return IRQ_HANDLED;
}
#endif

static int tpmic6315_intr_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int ret = 0;

	node = of_find_matching_node(NULL, tpmic_intr_of_match);
	if (!node)
		pr_info("@%s: find tpmic_intr node failed\n", __func__);

	virq_tempL_6 = platform_get_irq(pdev, 0);
	virq_tempH_6 = platform_get_irq(pdev, 1);
	virq_tempL_7 = platform_get_irq(pdev, 3);
	virq_tempH_7 = platform_get_irq(pdev, 4);
#ifdef INTR_UT
	virq_rcs0_6 = platform_get_irq(pdev, 2);
	virq_rcs0_7 = platform_get_irq(pdev, 5);
#endif

#ifdef INTR_UT
	if (virq_tempL_6 <= 0 || virq_tempH_6 <= 0 || virq_rcs0_6 <= 0 ||
		virq_tempL_7 <= 0 || virq_tempH_7 <= 0 || virq_rcs0_7 <= 0) {
#else
	if (virq_tempL_6 <= 0 || virq_tempH_6 <= 0 ||
		virq_tempL_7 <= 0 || virq_tempH_7 <= 0) {
#endif
		pr_info("%s: get irq error\n", __func__);
		return 0;
	}

	pr_info("%s: 6_temp_back_110D = %d(%d)\n"
			, __func__
			, platform_get_irq(pdev, 0)
			, platform_get_irq_byname(pdev, "6315_6_temp_l"));
	pr_info("%s: 6_temp_over_125D = %d(%d)\n"
			, __func__
			, platform_get_irq(pdev, 1)
			, platform_get_irq_byname(pdev, "6315_6_temp_h"));
	pr_info("%s: 7_temp_back_110D = %d(%d)\n"
			, __func__
			, platform_get_irq(pdev, 3)
			, platform_get_irq_byname(pdev, "6315_7_temp_l"));
	pr_info("%s: 7_temp_over_125D = %d(%d)\n"
			, __func__
			, platform_get_irq(pdev, 4)
			, platform_get_irq_byname(pdev, "6315_7_temp_h"));

#ifdef INTR_UT
	pr_info("%s: 6_rcs0 = %d(%d)\n"
			, __func__
			, platform_get_irq(pdev, 2)
			, platform_get_irq_byname(pdev, "6315_6_rcs0"));
	pr_info("%s: 7_rcs0 = %d(%d)\n"
			, __func__
			, platform_get_irq(pdev, 5)
			, platform_get_irq_byname(pdev, "6315_7_rcs0"));
#endif

	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "6315_6_temp_l"),
		NULL, mt6315_6_temp_l_int_handler, IRQF_TRIGGER_NONE,
		"6315_S6_TEMP_L", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request test irq fail\n");

	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "6315_6_temp_h"),
		NULL, mt6315_6_temp_h_int_handler, IRQF_TRIGGER_NONE,
		"6315_S6_TEMP_H", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request test irq fail\n");

	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "6315_7_temp_l"),
		NULL, mt6315_7_temp_l_int_handler, IRQF_TRIGGER_NONE,
		"6315_S7_TEMP_L", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request test irq fail\n");

	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "6315_7_temp_h"),
		NULL, mt6315_7_temp_h_int_handler, IRQF_TRIGGER_NONE,
		"6315_S7_TEMP_H", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request test irq fail\n");

#ifdef INTR_UT
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "6315_6_rcs0"),
		NULL, mt6315_6_rcs0_handler, IRQF_TRIGGER_NONE,
		"6315_S6_RCS0", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request test irq fail\n");

	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "6315_7_rcs0"),
		NULL, mt6315_7_rcs0_handler, IRQF_TRIGGER_NONE,
		"6315_S7_RCS0", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request test irq fail\n");
#endif

	disable_irq_nosync(virq_tempL_6);
	disable_irq_nosync(virq_tempL_7);

	return 0;
}

static int cl_6315ot_read(struct seq_file *m, void *v)
{

	seq_printf(m, "[%s] C/G limit = %d/%d\n",
		__func__, cpu_limit, gpu_limit);

	return 0;
}
static ssize_t cl_6315ot_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;
	int c_limit, g_limit;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	if (sscanf(desc, "%d %d", &c_limit, &g_limit) == 2) {
		pr_info("[%s] set C/G limit = %d/%d\n",
			__func__, c_limit, g_limit);
		cpu_limit = (c_limit != 0) ? c_limit : 0x7FFFFFFF;
		gpu_limit = (g_limit != 0) ? g_limit : 0x7FFFFFFF;

		return count;
	}

	pr_info("%s bad argument\n", __func__);
	return -EINVAL;
}

static int cl_6315ot_open(struct inode *inode, struct file *file)
{
	return single_open(file, cl_6315ot_read, NULL);
}

static const struct file_operations cl_6315ot_fops = {
	.owner = THIS_MODULE,
	.open = cl_6315ot_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = cl_6315ot_write,
	.release = single_release,
};

static int __init mtk_ti_6315_init(void)
{
	int ret = 0;

	/* register platform driver */
	ret = platform_driver_register(&tpmic_intr_pdrv);
	if (ret) {
		pr_info("fail to register %s driver ~~~\n", __func__);
		goto end;
	}

	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;
		struct proc_dir_entry *dir_entry = NULL;

		dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
		if (!dir_entry) {
			pr_info(
			"[%s]: mkdir /proc/driver/thermal failed\n", __func__);
		} else {
			entry = proc_create("cl6315ot_limit", 0664,
						dir_entry,
						&cl_6315ot_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
		}
	}

	/* register user to thermal */
	apthermolmt_register_user(&ap_6315ot, ap_6315ot_log);

end:
	return 0;
}

static void __exit mtk_ti_6315_exit(void)
{
	apthermolmt_unregister_user(&ap_6315ot);
	platform_driver_unregister(&tpmic_intr_pdrv);
	pr_info("%s\n", __func__);
}

late_initcall(mtk_ti_6315_init);
module_exit(mtk_ti_6315_exit);
