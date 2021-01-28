/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/of.h>

#include <spm/mtk_spm.h>


#if defined(CONFIG_MTK_QOS_V2)
#include <mtk_qos_ipi.h>
#endif

#include <helio-dvfsrc-qos.h>

struct helio_dvfsrc *dvfsrc;

#define DVFSRC_REG(offset) (dvfsrc->regs + offset)

#if defined(DVFSRC_SMC_CONTROL)
#define SPM_REG(offset) (dvfsrc->spm_regs + offset)

u32 spm_reg_read(u32 offset)
{
	if (is_dvfsrc_enabled())
		return readl(SPM_REG(offset));
	else
		return 0;
}
#endif


u32 dvfsrc_read(u32 offset)
{
	if (is_dvfsrc_enabled())
		return readl(DVFSRC_REG(offset));
	else
		return 0;
}

void dvfsrc_write(u32 offset, u32 val)
{
	if (is_dvfsrc_enabled())
		writel(val, DVFSRC_REG(offset));
}

static void dvfsrc_restore(void)
{
	int i, class_type;

	for (i = DVFSRC_QOS_MEMORY_BANDWIDTH; i < DVFSRC_QOS_NUM_CLASSES; i++) {
		switch (i) {
		case DVFSRC_QOS_MEMORY_BANDWIDTH:
			class_type = PM_QOS_MEMORY_BANDWIDTH;
			break;
		case DVFSRC_QOS_CPU_MEMORY_BANDWIDTH:
			class_type = PM_QOS_CPU_MEMORY_BANDWIDTH;
			break;
		case DVFSRC_QOS_GPU_MEMORY_BANDWIDTH:
			class_type = PM_QOS_GPU_MEMORY_BANDWIDTH;
			break;
		case DVFSRC_QOS_MM_MEMORY_BANDWIDTH:
			class_type = PM_QOS_MM_MEMORY_BANDWIDTH;
			break;
		case DVFSRC_QOS_OTHER_MEMORY_BANDWIDTH:
			class_type = PM_QOS_OTHER_MEMORY_BANDWIDTH;
			break;
		case DVFSRC_QOS_DDR_OPP:
			class_type = PM_QOS_DDR_OPP;
			break;
		case DVFSRC_QOS_VCORE_OPP:
			class_type = PM_QOS_VCORE_OPP;
			break;
		case DVFSRC_QOS_SCP_VCORE_REQUEST:
			class_type = PM_QOS_SCP_VCORE_REQUEST;
		break;
		case DVFSRC_QOS_POWER_MODEL_DDR_REQUEST:
			class_type = PM_QOS_POWER_MODEL_DDR_REQUEST;
			break;
		case DVFSRC_QOS_POWER_MODEL_VCORE_REQUEST:
			class_type = PM_QOS_POWER_MODEL_VCORE_REQUEST;
			break;
		case DVFSRC_QOS_VCORE_DVFS_FORCE_OPP:
			class_type = PM_QOS_VCORE_DVFS_FORCE_OPP;
			break;
		case DVFSRC_QOS_ISP_HRT_BANDWIDTH:
			class_type = PM_QOS_ISP_HRT_BANDWIDTH;
			break;
		case DVFSRC_QOS_APU_MEMORY_BANDWIDTH:
			class_type = PM_QOS_APU_MEMORY_BANDWIDTH;
			break;
		default:
			continue;
		}
		commit_data(i, pm_qos_request(class_type), 0);
	}
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if defined(CONFIG_MTK_QOS_V2)
static void helio_dvfsrc_sspm_init(int dvfsrc_en)
{
	struct qos_ipi_data qos_d;

	qos_d.cmd = QOS_IPI_DVFSRC_ENABLE;
	qos_d.u.dvfsrc_enable.dvfsrc_en = dvfsrc_en;
	qos_ipi_to_sspm_command(&qos_d, 2);
}
#endif
#endif

void helio_dvfsrc_enable(int dvfsrc_en)
{
	int ret = 0;
	if (dvfsrc_en > 1 || dvfsrc_en < 0)
		return;

	if (dvfsrc->dvfsrc_enabled == dvfsrc_en && dvfsrc_en == 1) {
		pr_info("DVFSRC already enabled\n");
		return;
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if defined(CONFIG_MTK_QOS_V2)
	helio_dvfsrc_sspm_init(dvfsrc_en);
#endif
#endif

	dvfsrc->dvfsrc_enabled = dvfsrc_en;
	dvfsrc->opp_forced = 0;
	ret = sprintf(dvfsrc->force_start, "0");
	if (ret < 0)
		pr_info("sprintf fail\n");
	ret = sprintf(dvfsrc->force_end, "0");
	if (ret < 0)
		pr_info("sprintf fail\n");

	dvfsrc_restore();
	if (dvfsrc_en)
		dvfsrc_en |= (dvfsrc->dvfsrc_flag << 1);

	mtk_spmfw_init(dvfsrc_en, 1);
}

void helio_dvfsrc_flag_set(int flag)
{
	dvfsrc->dvfsrc_flag = flag;
}

int helio_dvfsrc_flag_get(void)
{
	return	dvfsrc->dvfsrc_flag;
}

static int helio_dvfsrc_common_init(void)
{
	dvfsrc_opp_level_mapping();
	dvfsrc_opp_table_init();

	return 0;
}

int is_dvfsrc_enabled(void)
{
	if (dvfsrc)
		return dvfsrc->dvfsrc_enabled == 1;

	return 0;
}

static void get_pm_qos_info(char *p)
{
	p += sprintf(p, "%-24s: %d\n",
			"PM_QOS_MEMORY_BW",
			pm_qos_request(PM_QOS_MEMORY_BANDWIDTH));
	p += sprintf(p, "%-24s: %d\n",
			"PM_QOS_CPU_MEMORY_BW",
			pm_qos_request(PM_QOS_CPU_MEMORY_BANDWIDTH));
	p += sprintf(p, "%-24s: %d\n",
			"PM_QOS_GPU_MEMORY_BW",
			pm_qos_request(PM_QOS_GPU_MEMORY_BANDWIDTH));
	p += sprintf(p, "%-24s: %d\n",
			"PM_QOS_MM_MEMORY_BW",
			pm_qos_request(PM_QOS_MM_MEMORY_BANDWIDTH));
	p += sprintf(p, "%-24s: %d\n",
			"PM_QOS_OTHER_MEMORY_BW",
			pm_qos_request(PM_QOS_OTHER_MEMORY_BANDWIDTH));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_DDR_OPP",
			pm_qos_request(PM_QOS_DDR_OPP));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_VCORE_OPP",
			pm_qos_request(PM_QOS_VCORE_OPP));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_SCP_VCORE_REQ",
			pm_qos_request(PM_QOS_SCP_VCORE_REQUEST));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_PM_DDR_REQ",
			pm_qos_request(PM_QOS_POWER_MODEL_DDR_REQUEST));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_PM_VCORE_REQ",
			pm_qos_request(PM_QOS_POWER_MODEL_VCORE_REQUEST));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_FORCE_OPP",
			pm_qos_request(PM_QOS_VCORE_DVFS_FORCE_OPP));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_ISP_HRT",
			pm_qos_request(PM_QOS_ISP_HRT_BANDWIDTH));
}

u32 dvfsrc_dump_reg(char *ptr, u32 count)
{
	char buf[1024];
	u32 index = 0;

	memset(buf, '\0', sizeof(buf));
	get_opp_info(buf);
	if (ptr) {
		index += scnprintf(&ptr[index], (count - index - 1),
		"%s\n", buf);
	} else
		pr_info("%s\n", buf);

	memset(buf, '\0', sizeof(buf));
	get_dvfsrc_reg(buf);
	if (ptr) {
		index += scnprintf(&ptr[index], (count - index - 1),
		"%s\n", buf);
	} else
		pr_info("%s\n", buf);

	memset(buf, '\0', sizeof(buf));
	get_dvfsrc_record(buf);
	if (ptr) {
		index += scnprintf(&ptr[index], (count - index - 1),
		"%s\n", buf);
	} else
		pr_info("%s\n", buf);

	memset(buf, '\0', sizeof(buf));
	get_spm_reg(buf);
	if (ptr) {
		index += scnprintf(&ptr[index], (count - index - 1),
		"%s\n", buf);
	} else
		pr_info("%s\n", buf);

	memset(buf, '\0', sizeof(buf));
	get_pm_qos_info(buf);
	if (ptr) {
		index += scnprintf(&ptr[index], (count - index - 1),
		"%s\n", buf);
	} else
		pr_info("%s\n", buf);

	return index;
}

void dvfsrc_set_power_model_ddr_request(unsigned int level)
{
	commit_data(DVFSRC_QOS_POWER_MODEL_DDR_REQUEST, level, 1);
}

static int pm_qos_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_cpu_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_CPU_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_gpu_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_GPU_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_mm_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_MM_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_other_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_OTHER_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_ddr_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_DDR_OPP, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_vcore_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_VCORE_OPP, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_scp_vcore_request_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_SCP_VCORE_REQUEST, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_power_model_ddr_request_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_POWER_MODEL_DDR_REQUEST, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_power_model_vcore_request_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_POWER_MODEL_VCORE_REQUEST, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_vcore_dvfs_force_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_VCORE_DVFS_FORCE_OPP, l, 1);

	return NOTIFY_OK;
}

static int pmqos_isp_hrt_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_ISP_HRT_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_apu_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(DVFSRC_QOS_APU_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static void pm_qos_notifier_register(void)
{
	dvfsrc->pm_qos_memory_bw_nb.notifier_call =
		pm_qos_memory_bw_notify;
	dvfsrc->pm_qos_cpu_memory_bw_nb.notifier_call =
		pm_qos_cpu_memory_bw_notify;
	dvfsrc->pm_qos_gpu_memory_bw_nb.notifier_call =
		pm_qos_gpu_memory_bw_notify;
	dvfsrc->pm_qos_mm_memory_bw_nb.notifier_call =
		pm_qos_mm_memory_bw_notify;
	dvfsrc->pm_qos_other_memory_bw_nb.notifier_call =
		pm_qos_other_memory_bw_notify;
	dvfsrc->pm_qos_ddr_opp_nb.notifier_call =
		pm_qos_ddr_opp_notify;
	dvfsrc->pm_qos_vcore_opp_nb.notifier_call =
		pm_qos_vcore_opp_notify;
	dvfsrc->pm_qos_scp_vcore_request_nb.notifier_call =
		pm_qos_scp_vcore_request_notify;
	dvfsrc->pm_qos_power_model_ddr_request_nb.notifier_call =
		pm_qos_power_model_ddr_request_notify;
	dvfsrc->pm_qos_power_model_vcore_request_nb.notifier_call =
		pm_qos_power_model_vcore_request_notify;
	dvfsrc->pm_qos_vcore_dvfs_force_opp_nb.notifier_call =
		pm_qos_vcore_dvfs_force_opp_notify;
	dvfsrc->pm_qos_isp_hrt_bw_nb.notifier_call =
		pmqos_isp_hrt_memory_bw_notify;
	dvfsrc->pm_qos_apu_memory_bw_nb.notifier_call =
		pm_qos_apu_memory_bw_notify;

	pm_qos_add_notifier(PM_QOS_MEMORY_BANDWIDTH,
			&dvfsrc->pm_qos_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_CPU_MEMORY_BANDWIDTH,
			&dvfsrc->pm_qos_cpu_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_GPU_MEMORY_BANDWIDTH,
			&dvfsrc->pm_qos_gpu_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_MM_MEMORY_BANDWIDTH,
			&dvfsrc->pm_qos_mm_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_OTHER_MEMORY_BANDWIDTH,
			&dvfsrc->pm_qos_other_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_DDR_OPP,
			&dvfsrc->pm_qos_ddr_opp_nb);
	pm_qos_add_notifier(PM_QOS_VCORE_OPP,
			&dvfsrc->pm_qos_vcore_opp_nb);
	pm_qos_add_notifier(PM_QOS_SCP_VCORE_REQUEST,
			&dvfsrc->pm_qos_scp_vcore_request_nb);
	pm_qos_add_notifier(PM_QOS_POWER_MODEL_DDR_REQUEST,
			&dvfsrc->pm_qos_power_model_ddr_request_nb);
	pm_qos_add_notifier(PM_QOS_POWER_MODEL_VCORE_REQUEST,
			&dvfsrc->pm_qos_power_model_vcore_request_nb);
	pm_qos_add_notifier(PM_QOS_VCORE_DVFS_FORCE_OPP,
			&dvfsrc->pm_qos_vcore_dvfs_force_opp_nb);
	pm_qos_add_notifier(PM_QOS_ISP_HRT_BANDWIDTH,
			&dvfsrc->pm_qos_isp_hrt_bw_nb);
	pm_qos_add_notifier(PM_QOS_APU_MEMORY_BANDWIDTH,
			&dvfsrc->pm_qos_apu_memory_bw_nb);
}

__weak void pm_qos_trace_dbg_show_request(int pm_qos_class)
{
}

static DEFINE_RATELIMIT_STATE(tracelimit, 5 * HZ, 1);

void vcorefs_trace_qos(void)
{
	if (__ratelimit(&tracelimit)) {
		pm_qos_trace_dbg_show_request(PM_QOS_DDR_OPP);
		pm_qos_trace_dbg_show_request(PM_QOS_VCORE_OPP);
	}
}

static int helio_dvfsrc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;

	if (!spm_load_firmware_status()) {
		pr_err("SPM FIRMWARE IS NOT READY\n");
		return -ENODEV;
	}

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	dvfsrc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	platform_set_drvdata(pdev, dvfsrc);
	dvfsrc->dev = &pdev->dev;

	ret = helio_dvfsrc_add_interface(&pdev->dev);
	if (ret)
		return ret;

	pm_qos_notifier_register();

	helio_dvfsrc_common_init();

	if (of_property_read_u32(np, "dvfsrc_flag",
		(u32 *) &dvfsrc->dvfsrc_flag))
		dvfsrc->dvfsrc_flag = 0;

	helio_dvfsrc_platform_pre_init(dvfsrc);

	helio_dvfsrc_config(dvfsrc);

	pr_info("%s: init done\n", __func__);

	return 0;
}

static int helio_dvfsrc_remove(struct platform_device *pdev)
{
	helio_dvfsrc_remove_interface(&pdev->dev);
	return 0;
}

static const struct of_device_id helio_dvfsrc_of_match[] = {
	{ .compatible = "mediatek,dvfsrc" },
	{ .compatible = "mediatek,mt6785-dvfsrc" },
	{ },
};

MODULE_DEVICE_TABLE(of, helio_dvfsrc_of_match);

static __maybe_unused int helio_dvfsrc_suspend(struct device *dev)
{
	int ret = 0;

	if (dvfsrc->suspend) {
		ret = dvfsrc->suspend(dvfsrc);
		if (ret)
			return ret;
	}

	return 0;
}

static __maybe_unused int helio_dvfsrc_resume(struct device *dev)
{
	int ret = 0;

	if (dvfsrc->resume) {
		ret = dvfsrc->resume(dvfsrc);
		if (ret)
			return ret;
	}

	return ret;
}

static SIMPLE_DEV_PM_OPS(helio_dvfsrc_pm, helio_dvfsrc_suspend,
			 helio_dvfsrc_resume);

static struct platform_driver helio_dvfsrc_driver = {
	.probe	= helio_dvfsrc_probe,
	.remove	= helio_dvfsrc_remove,
	.driver = {
		.name = "helio-dvfsrc",
		.pm	= &helio_dvfsrc_pm,
		.of_match_table = helio_dvfsrc_of_match,
	},
};

static int __init helio_dvfsrc_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&helio_dvfsrc_driver);
	return ret;
}
late_initcall_sync(helio_dvfsrc_init)

static void __exit helio_dvfsrc_exit(void)
{
	platform_driver_unregister(&helio_dvfsrc_driver);
}
module_exit(helio_dvfsrc_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Helio dvfsrc driver");
