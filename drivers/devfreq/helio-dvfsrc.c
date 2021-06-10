/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/devfreq.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#include <linux/notifier.h>
#include <linux/pm_qos.h>

#include <linux/slab.h>

#include "governor.h"

#include <helio-dvfsrc.h>
#include <helio-dvfsrc-opp.h>
#include <mtk_dvfsrc_reg.h>
#include <mtk_spm_vcore_dvfs.h>
#include <mtk_spm_vcore_dvfs_ipi.h>
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <v1/sspm_ipi.h>
#include <sspm_ipi_pin.h>
#endif

#include <mtk_vcorefs_governor.h>

#include <mt-plat/aee.h>

#include <linux/regulator/consumer.h>
static struct regulator *vcore_reg_id;

__weak void helio_dvfsrc_platform_init(struct helio_dvfsrc *dvfsrc) { }
__weak void spm_check_status_before_dvfs(void) { }
__weak int spm_dvfs_flag_init(void) { return 0; }
__weak int vcore_opp_init(void) { return 0; }
__weak int spm_vcorefs_get_dvfs_opp(void) { return 0; }

static struct opp_profile opp_table[VCORE_DVFS_OPP_NUM];
struct helio_dvfsrc *dvfsrc;


int get_cur_vcore_dvfs_opp(void)
{
	int dvfsrc_level_bit = dvfsrc_read(dvfsrc, DVFSRC_LEVEL) >> 16;
	int dvfsrc_level = 0;

	for (dvfsrc_level = 0;
		dvfsrc_level < VCORE_DVFS_OPP_NUM - 1; dvfsrc_level++)
		if ((dvfsrc_level_bit & (1 << dvfsrc_level)) > 0)
			break;

	return VCORE_DVFS_OPP_NUM - dvfsrc_level - 1;
}

void dvfsrc_update_opp_table(void)
{
	struct opp_profile *opp_ctrl_table = opp_table;
	int opp;

	mutex_lock(&dvfsrc->devfreq->lock);
	for (opp = 0; opp < VCORE_DVFS_OPP_NUM; opp++)
		opp_ctrl_table[opp].vcore_uv = vcorefs_get_vcore_by_steps(opp);

	mutex_unlock(&dvfsrc->devfreq->lock);
}

char *dvfsrc_get_opp_table_info(char *p)
{
	struct opp_profile *opp_ctrl_table = opp_table;
	int i;
	char *buff_end = p + PAGE_SIZE;

	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++) {
		p += snprintf(p, buff_end - p,
				"[OPP%d] vcore_uv: %d (0x%x)\n",
				i, opp_ctrl_table[i].vcore_uv,
				vcore_uv_to_pmic(opp_ctrl_table[i].vcore_uv));
		p += snprintf(p, buff_end - p, "[OPP%d] ddr_khz : %d\n",
				i, opp_ctrl_table[i].ddr_khz);
		p += snprintf(p, buff_end - p, "\n");
	}

	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++)
		p += snprintf(p, buff_end - p,
				"OPP%d  : %u\n", i, opp_ctrl_table[i].vcore_uv);

	return p;
}
#if !defined(CONFIG_MACH_MT6771) && !defined(CONFIG_MACH_MT6765)

int dvfsrc_get_bw(int type)
{
	int ret = 0;
	int i;

	switch (type) {
	case QOS_TOTAL:
		ret = dvfsrc_sram_read(dvfsrc, QOS_TOTAL_BW);
		break;
	case QOS_CPU:
		ret = dvfsrc_sram_read(dvfsrc, QOS_CPU_BW);
		break;
	case QOS_MM:
		ret = dvfsrc_sram_read(dvfsrc, QOS_MM_BW);
		break;
	case QOS_GPU:
		ret = dvfsrc_sram_read(dvfsrc, QOS_GPU_BW);
		break;
	case QOS_MD_PERI:
		ret = dvfsrc_sram_read(dvfsrc, QOS_MD_PERI_BW);
		break;
	case QOS_TOTAL_AVE:
		for (i = 0; i < QOS_TOTAL_BW_BUF_SIZE; i++)
			ret += dvfsrc_sram_read(dvfsrc, QOS_TOTAL_BW_BUF(i));
		ret /= QOS_TOTAL_BW_BUF_SIZE;
		break;
	default:
		break;
	}

	return ret;
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
void dvfsrc_update_sspm_vcore_opp_table(int opp, unsigned int vcore_uv)
{
	struct qos_data qos_d;

	qos_d.cmd = QOS_IPI_VCORE_OPP;
	qos_d.u.vcore_opp.opp = opp;
	qos_d.u.vcore_opp.vcore_uv = vcore_uv;

	qos_ipi_to_sspm_command(&qos_d, 3);
}

void dvfsrc_update_sspm_ddr_opp_table(int opp, unsigned int ddr_khz)
{
	struct qos_data qos_d;

	qos_d.cmd = QOS_IPI_DDR_OPP;
	qos_d.u.ddr_opp.opp = opp;
	qos_d.u.ddr_opp.ddr_khz = ddr_khz;

	qos_ipi_to_sspm_command(&qos_d, 3);
}

#endif
#endif

void dvfsrc_init_opp_table(void)
{
	struct opp_profile *opp_ctrl_table = opp_table;
	int opp;

	mutex_lock(&dvfsrc->devfreq->lock);
	dvfsrc->curr_vcore_uv = vcorefs_get_curr_vcore();
	dvfsrc->curr_ddr_khz = vcorefs_get_curr_ddr();

	pr_info("curr_vcore_uv: %u, curr_ddr_khz: %u\n",
			dvfsrc->curr_vcore_uv,
			dvfsrc->curr_ddr_khz);

	for (opp = 0; opp < VCORE_DVFS_OPP_NUM; opp++) {
		opp_ctrl_table[opp].vcore_uv = vcorefs_get_vcore_by_steps(opp);
		opp_ctrl_table[opp].ddr_khz = vcorefs_get_ddr_by_steps(opp);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		dvfsrc_update_sspm_vcore_opp_table(opp,
						opp_ctrl_table[opp].vcore_uv);
		dvfsrc_update_sspm_ddr_opp_table(opp,
						opp_ctrl_table[opp].ddr_khz);
#endif

		pr_info("opp %u: vcore_uv: %u, ddr_khz: %u\n", opp,
				opp_ctrl_table[opp].vcore_uv,
				opp_ctrl_table[opp].ddr_khz);
	}

	spm_vcorefs_pwarp_cmd();
	mutex_unlock(&dvfsrc->devfreq->lock);
}

int is_qos_can_work(void)
{
	if (dvfsrc)
		return dvfsrc->enable;

	return 0;
}

static struct devfreq_dev_profile helio_devfreq_profile = {
	.polling_ms	= 0,
};

static int helio_dvfsrc_reg_config(struct helio_dvfsrc *dvfsrc,
					struct reg_config *config)
{
#if 0
	int i;

	mutex_lock(&dvfsrc->devfreq->lock);
	for (i = 0; config[i].offset != -1; i++)
		dvfsrc_write(dvfsrc, config[i].offset, config[i].val);

	mutex_unlock(&dvfsrc->devfreq->lock);
#endif
	return 0;
}

static int helio_governor_event_handler(struct devfreq *devfreq,
					unsigned int event, void *data)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = dev_get_drvdata(devfreq->dev.parent);

	switch (event) {
	case DEVFREQ_GOV_SUSPEND:
		helio_dvfsrc_reg_config(dvfsrc, dvfsrc->suspend_config);
		break;

	case DEVFREQ_GOV_RESUME:
		helio_dvfsrc_reg_config(dvfsrc, dvfsrc->resume_config);
		break;

	default:
		break;
	}
	return 0;
}

static struct devfreq_governor helio_dvfsrc_governor = {
	.name = "helio_dvfsrc",
	.event_handler = helio_governor_event_handler,
};

static void helio_dvfsrc_enable(struct helio_dvfsrc *dvfsrc)
{
	mutex_lock(&dvfsrc->devfreq->lock);

#if !defined(CONFIG_MACH_MT6771) && !defined(CONFIG_MACH_MT6765)
	dvfsrc_write(dvfsrc, DVFSRC_VCORE_REQUEST,
		(dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST) & ~(0x3 << 20)));
	dvfsrc_write(dvfsrc, DVFSRC_EMI_REQUEST,
		(dvfsrc_read(dvfsrc, DVFSRC_EMI_REQUEST) & ~(0x3 << 20)));
#endif

	dvfsrc->enable = 1;

	mutex_unlock(&dvfsrc->devfreq->lock);
}

int is_dvfsrc_opp_fixed(void)
{
	if (!is_qos_can_work())
		return 1;

	if ((spm_dvfs_flag_init()&
		(SPM_FLAG_DIS_VCORE_DVS|SPM_FLAG_DIS_VCORE_DFS)) != 0)
		return 1;

	if (dvfsrc->skip)
		return 1;

#if defined(CONFIG_MACH_MT6771)
	if (is_force_opp_enable())
		return 1;
#endif

	return 0;
}

static int commit_data(struct helio_dvfsrc *dvfsrc, int type, int data)
{
	int ret = 0;
	int level = 0;
	int opp = 0;
	int last_cnt = 0;
	int opp_uv;
	int vcore_uv = 0;

	mutex_lock(&dvfsrc->devfreq->lock);

	if (!dvfsrc->enable)
		goto out;

	if (is_force_opp_enable())
		goto out;

	if (dvfsrc->skip)
		goto out;

	spm_check_status_before_dvfs();

	if (type == PM_QOS_EMI_OPP ||
	    type == PM_QOS_VCORE_OPP ||
	    type == PM_QOS_VCORE_DVFS_FIXED_OPP) {
		last_cnt = dvfsrc_read(dvfsrc, DVFSRC_LAST);
		ret = wait_for_completion
			(is_dvfsrc_in_progress(dvfsrc) == 0, DVFSRC_TIMEOUT);
		if (ret) {
			pr_info("[%s] wait no idle, class: %d, data: 0x%x",
				__func__, type, data);
			pr_info("rc_level: 0x%x (last: %d -> %d)\n",
				dvfsrc_read(dvfsrc, DVFSRC_LEVEL),
				last_cnt, dvfsrc_read(dvfsrc, DVFSRC_LAST));

			/* aee_kernel_warning(NULL); */
			/* goto out; */
		}
	}

	switch (type) {
	case PM_QOS_MEMORY_BANDWIDTH:
		if (data == PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE)
			break;
		if (dvfsrc->log_mask & (0x1 << type))
			pr_info("[%s] class: %d, data: 0x%x\n",
				__func__, type, data);
		dvfsrc_write(dvfsrc, DVFSRC_SW_BW_0, data / 100);
		break;
	case PM_QOS_CPU_MEMORY_BANDWIDTH:
		dvfsrc_write(dvfsrc, DVFSRC_SW_BW_1, data / 100);
		break;
	case PM_QOS_GPU_MEMORY_BANDWIDTH:
		dvfsrc_write(dvfsrc, DVFSRC_SW_BW_2, data / 100);
		break;
	case PM_QOS_MM_MEMORY_BANDWIDTH:
#if defined(CONFIG_MACH_MT6771)
		dvfsrc_write(dvfsrc, DVFSRC_SW_BW_3, data / 75); /* x 4/3 */
#else
		dvfsrc_write(dvfsrc, DVFSRC_SW_BW_3, data / 100);
#endif
		break;
	case PM_QOS_MD_PERI_MEMORY_BANDWIDTH:
		dvfsrc_write(dvfsrc, DVFSRC_SW_BW_4, data / 100);
		break;
	case PM_QOS_EMI_OPP:
		if (dvfsrc->log_mask & (0x1 << type))
			pr_info("[%s] class: %d, data: 0x%x\n",
				__func__, type, data);

		if (data >= DDR_OPP_NUM)
			data = DDR_OPP_NUM - 1;
		else if (data < 0)
			data = DDR_OPP_NUM - 1;

		level = DDR_OPP_NUM - data - 1;
		dvfsrc_write(dvfsrc, DVFSRC_SW_REQ,
				(dvfsrc_read(dvfsrc, DVFSRC_SW_REQ)
				& ~(0x3)) | level);
		udelay(1);
		ret = wait_for_completion
		(is_dvfsrc_in_progress(dvfsrc) == 0, DVFSRC_TIMEOUT);
		udelay(1);
#if defined(CONFIG_MACH_MT6771)
		opp = get_min_opp_for_ddr(data);
		ret = wait_for_completion(spm_vcorefs_get_dvfs_opp() <= opp,
				SPM_DVFS_TIMEOUT);
#else
		ret = wait_for_completion
		(get_dvfsrc_level(dvfsrc) >= emi_to_vcore_dvfs_level[level],
		SPM_DVFS_TIMEOUT);
#endif
		if (ret < 0) {
			pr_info
			("[%s] wair not complete, class: %d, data: 0x%x\n",
			__func__, type, data);
			spm_vcorefs_dump_dvfs_regs(NULL);
			aee_kernel_warning("VCOREFS",
			"emi_opp cannot be done.");
		}

		break;
	case PM_QOS_VCORE_OPP:
		if (dvfsrc->log_mask & (0x1 << type))
			pr_info("[%s] class: %d, data: 0x%x\n",
				__func__, type, data);

		if (data >= VCORE_OPP_NUM)
			data = VCORE_OPP_NUM - 1;
		else if (data < 0)
			data = VCORE_OPP_NUM - 1;

		level = ((VCORE_OPP_NUM - data - 1) << 24);
		dvfsrc_write(dvfsrc, DVFSRC_VCORE_REQUEST2,
				(dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST2)
				& ~(0x03000000)) | level);
		udelay(1);
		ret = wait_for_completion
		(is_dvfsrc_in_progress(dvfsrc) == 0, DVFSRC_TIMEOUT);
		udelay(1);
#if defined(CONFIG_MACH_MT6771)
		opp = get_min_opp_for_vcore(data);
		ret = wait_for_completion(spm_vcorefs_get_dvfs_opp() <= opp,
				SPM_DVFS_TIMEOUT);
#else
		ret = wait_for_completion
		(get_dvfsrc_level(dvfsrc) >= vcore_to_vcore_dvfs_level[level],
				SPM_DVFS_TIMEOUT);
#endif
		if (ret < 0) {
			pr_info
			("[%s] not complete, class: %d, data: 0x%x\n",
			__func__, type, data);
			spm_vcorefs_dump_dvfs_regs(NULL);
			aee_kernel_warning("VCOREFS",
			"vcore_opp cannot be done.");
		}

		if (vcore_reg_id) {
			vcore_uv = regulator_get_voltage(vcore_reg_id);
			opp_uv = get_vcore_opp_volt(get_min_opp_for_vcore(opp));
				if (vcore_uv < opp_uv) {
					pr_info("DVFS FAIL= %d %d 0x%08x %08x\n",
					vcore_uv, opp_uv,
					dvfsrc_read(dvfsrc, DVFSRC_LEVEL),
					spm_vcorefs_get_dvfs_opp());

					aee_kernel_warning("DVFSRC",
						"VCORE failed.",
						__func__);
				}
		}
		break;
	case PM_QOS_VCORE_DVFS_FIXED_OPP:
		if (dvfsrc->log_mask & (0x1 << type))
			pr_info("[%s] class: %d, data: 0x%x\n",
					__func__, type, data);

		if (data >= VCORE_DVFS_OPP_NUM)
			data = VCORE_DVFS_OPP_NUM;

		if (data == VCORE_DVFS_OPP_NUM) { /* no fix opp*/
			dvfsrc_write(dvfsrc, DVFSRC_BASIC_CONTROL,
					(dvfsrc_read(dvfsrc,
					DVFSRC_BASIC_CONTROL) & ~(1 << 15)));
			dvfsrc_write(dvfsrc, DVFSRC_FORCE,
				       dvfsrc_read(dvfsrc, DVFSRC_FORCE)
				       & 0xFFFF0000);
		} else { /* fix opp */
			level = 1 << (VCORE_DVFS_OPP_NUM - data - 1);
			dvfsrc_write(dvfsrc, DVFSRC_FORCE, level);
			dvfsrc_write(dvfsrc, DVFSRC_BASIC_CONTROL,
				(dvfsrc_read(dvfsrc, DVFSRC_BASIC_CONTROL)
				| (1 << 15)));
#if defined(CONFIG_MACH_MT6771)
			ret = wait_for_completion
			(spm_vcorefs_get_dvfs_opp() == data, SPM_DVFS_TIMEOUT);
#else
			ret = wait_for_completion
			(get_dvfsrc_level(dvfsrc) ==
				vcore_dvfs_to_vcore_dvfs_level[level],
			SPM_DVFS_TIMEOUT);
#endif
			if (ret < 0) {
				pr_info
				("[%s] not complete, class: %d, data: 0x%x\n",
				__func__, type, data);
				spm_vcorefs_dump_dvfs_regs(NULL);
				aee_kernel_exception("VCOREFS",
				"dvfsrc cannot be done.");
			}

		}
		break;
	default:
		break;
	}

out:
	mutex_unlock(&dvfsrc->devfreq->lock);

	return ret;
}

#if !defined(CONFIG_MACH_MT6771) && !defined(CONFIG_MACH_MT6765)
void dvfsrc_set_vcore_request(unsigned int mask, unsigned int vcore_level)
{
	int r = 0;
	unsigned int val = 0;

	mutex_lock(&dvfsrc->devfreq->lock);

	/* check DVFS idle */
	r = wait_for_completion
		(is_dvfsrc_in_progress(dvfsrc) == 0, SPM_DVFS_TIMEOUT);
	if (r < 0) {
		spm_vcorefs_dump_dvfs_regs(NULL);
		aee_kernel_exception("VCOREFS", "dvfsrc cannot be idle.");
		goto out;
	}

	val = (spm_read(DVFSRC_VCORE_REQUEST) & ~mask) | vcore_level;
	dvfsrc_write(dvfsrc, DVFSRC_VCORE_REQUEST, val);

	r = wait_for_completion(
	get_dvfsrc_level(dvfsrc) >= vcore_to_vcore_dvfs_level[vcore_level],
	SPM_DVFS_TIMEOUT);
	if (r < 0) {
		spm_vcorefs_dump_dvfs_regs(NULL);
		aee_kernel_exception("VCOREFS", "dvfsrc cannot be done.");
	}

out:
	mutex_unlock(&dvfsrc->devfreq->lock);
}
#endif

static int pm_qos_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = container_of(b, struct helio_dvfsrc, pm_qos_memory_bw_nb);

	commit_data(dvfsrc, PM_QOS_MEMORY_BANDWIDTH, l);

	return NOTIFY_OK;
}

static int pm_qos_cpu_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = container_of(b, struct helio_dvfsrc, pm_qos_cpu_memory_bw_nb);

	commit_data(dvfsrc, PM_QOS_CPU_MEMORY_BANDWIDTH, l);

	return NOTIFY_OK;
}

static int pm_qos_gpu_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = container_of(b, struct helio_dvfsrc, pm_qos_gpu_memory_bw_nb);

	commit_data(dvfsrc, PM_QOS_GPU_MEMORY_BANDWIDTH, l);

	return NOTIFY_OK;
}

static int pm_qos_mm_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = container_of(b, struct helio_dvfsrc, pm_qos_mm_memory_bw_nb);

	commit_data(dvfsrc, PM_QOS_MM_MEMORY_BANDWIDTH, l);

	return NOTIFY_OK;
}

static int pm_qos_md_peri_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = container_of(b,
			struct helio_dvfsrc, pm_qos_md_peri_memory_bw_nb);

	commit_data(dvfsrc, PM_QOS_MD_PERI_MEMORY_BANDWIDTH, l);

	return NOTIFY_OK;
}

static int pm_qos_emi_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = container_of(b, struct helio_dvfsrc, pm_qos_emi_opp_nb);

	commit_data(dvfsrc, PM_QOS_EMI_OPP, l);

	return NOTIFY_OK;
}

static int pm_qos_vcore_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = container_of(b, struct helio_dvfsrc, pm_qos_vcore_opp_nb);

	commit_data(dvfsrc, PM_QOS_VCORE_OPP, l);

	return NOTIFY_OK;
}

static int pm_qos_vcore_dvfs_fixed_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = container_of(b,
			struct helio_dvfsrc, pm_qos_vcore_dvfs_fixed_opp_nb);

	commit_data(dvfsrc, PM_QOS_VCORE_DVFS_FIXED_OPP, l);

	return NOTIFY_OK;
}

static void pm_qos_notifier_register(struct helio_dvfsrc *dvfsrc)
{
	dvfsrc->pm_qos_memory_bw_nb.notifier_call =
					pm_qos_memory_bw_notify;
	dvfsrc->pm_qos_cpu_memory_bw_nb.notifier_call =
					pm_qos_cpu_memory_bw_notify;
	dvfsrc->pm_qos_gpu_memory_bw_nb.notifier_call =
					pm_qos_gpu_memory_bw_notify;
	dvfsrc->pm_qos_mm_memory_bw_nb.notifier_call =
					pm_qos_mm_memory_bw_notify;
	dvfsrc->pm_qos_md_peri_memory_bw_nb.notifier_call =
					pm_qos_md_peri_memory_bw_notify;
	dvfsrc->pm_qos_emi_opp_nb.notifier_call = pm_qos_emi_opp_notify;
	dvfsrc->pm_qos_vcore_opp_nb.notifier_call = pm_qos_vcore_opp_notify;
	dvfsrc->pm_qos_vcore_dvfs_fixed_opp_nb.notifier_call =
					pm_qos_vcore_dvfs_fixed_opp_notify;

	pm_qos_add_notifier(PM_QOS_MEMORY_BANDWIDTH,
					&dvfsrc->pm_qos_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_CPU_MEMORY_BANDWIDTH,
					&dvfsrc->pm_qos_cpu_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_GPU_MEMORY_BANDWIDTH,
					&dvfsrc->pm_qos_gpu_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_MM_MEMORY_BANDWIDTH,
					&dvfsrc->pm_qos_mm_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_MD_PERI_MEMORY_BANDWIDTH,
					&dvfsrc->pm_qos_md_peri_memory_bw_nb);
	pm_qos_add_notifier(PM_QOS_EMI_OPP, &dvfsrc->pm_qos_emi_opp_nb);
	pm_qos_add_notifier(PM_QOS_VCORE_OPP, &dvfsrc->pm_qos_vcore_opp_nb);
	pm_qos_add_notifier(PM_QOS_VCORE_DVFS_FIXED_OPP,
				&dvfsrc->pm_qos_vcore_dvfs_fixed_opp_nb);
}

static int helio_dvfsrc_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	dvfsrc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	dvfsrc->sram_regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dvfsrc->sram_regs))
		return PTR_ERR(dvfsrc->sram_regs);

	ret = helio_dvfsrc_add_interface(&pdev->dev);
	if (ret)
		return ret;

	helio_dvfsrc_platform_init(dvfsrc);

	vcore_reg_id = regulator_get(&pdev->dev, "vcore");
	if (!vcore_reg_id)
		pr_info("regulator_get vcore_reg_id failed\n");

	dvfsrc->devfreq = devm_devfreq_add_device(&pdev->dev,
						 &helio_devfreq_profile,
						 "helio_dvfsrc",
						 NULL);
#if !defined(CONFIG_MACH_MT6771) && !defined(CONFIG_MACH_MT6765)
	vcore_opp_init();
	dvfsrc_init_opp_table();
	spm_check_status_before_dvfs();

	ret = helio_dvfsrc_reg_config(dvfsrc, dvfsrc->init_config);
	if (ret)
		return ret;
#endif

	pm_qos_notifier_register(dvfsrc);
	helio_dvfsrc_enable(dvfsrc);

	platform_set_drvdata(pdev, dvfsrc);

	return 0;
}

static int helio_dvfsrc_remove(struct platform_device *pdev)
{
	helio_dvfsrc_remove_interface(&pdev->dev);
	return 0;
}

static const struct of_device_id helio_dvfsrc_of_match[] = {
	{ .compatible = "mediatek,dvfsrc_top" },
	{ .compatible = "mediatek,helio-dvfsrc-v2" },
	{ },
};

MODULE_DEVICE_TABLE(of, helio_dvfsrc_of_match);

static __maybe_unused int helio_dvfsrc_suspend(struct device *dev)
{
	struct helio_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	int ret = 0;

	ret = devfreq_suspend_device(dvfsrc->devfreq);
	if (ret < 0) {
		dev_err(dev, "failed to suspend the devfreq devices\n");
		return ret;
	}

	return 0;
}

static __maybe_unused int helio_dvfsrc_resume(struct device *dev)
{
	struct helio_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	int ret = 0;

	ret = devfreq_resume_device(dvfsrc->devfreq);
	if (ret < 0) {
		dev_err(dev, "failed to resume the devfreq devices\n");
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

	ret = devfreq_add_governor(&helio_dvfsrc_governor);
	if (ret) {
		pr_err("%s: failed to add governor: %d\n", __func__, ret);
		return ret;
	}

	ret = platform_driver_register(&helio_dvfsrc_driver);
	if (ret)
		devfreq_remove_governor(&helio_dvfsrc_governor);

	return ret;
}
late_initcall_sync(helio_dvfsrc_init)

static void __exit helio_dvfsrc_exit(void)
{
	int ret = 0;

	platform_driver_unregister(&helio_dvfsrc_driver);

	ret = devfreq_remove_governor(&helio_dvfsrc_governor);
	if (ret)
		pr_err("%s: failed to remove governor: %d\n", __func__, ret);
}
module_exit(helio_dvfsrc_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Helio dvfsrc driver");
