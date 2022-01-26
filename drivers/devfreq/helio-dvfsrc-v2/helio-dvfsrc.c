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

#include <linux/devfreq.h>
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
#include <linux/sched/clock.h>
#include "governor.h"

#include <mt-plat/aee.h>
#include <spm/mtk_spm.h>
#include <mtk_dramc.h>

#include "helio-dvfsrc_v2.h"
#include "mtk_dvfsrc_reg.h"
#include <linux/regulator/consumer.h>

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>
#endif
static struct regulator *vcore_reg_id;
static struct helio_dvfsrc *dvfsrc;
static DEFINE_MUTEX(sw_req1_mutex);
static DEFINE_SPINLOCK(force_req_lock);

#define DVFSRC_REG(offset) (dvfsrc->regs + offset)
#define DVFSRC_SRAM_REG(offset) (dvfsrc->sram_regs + offset)

int __weak dram_steps_freq(unsigned int step)
{
	pr_info("get dram steps_freq fail\n");
	return -1;
}

u32 dvfsrc_read(u32 offset)
{
	return readl(DVFSRC_REG(offset));
}

void dvfsrc_write(u32 offset, u32 val)
{
	writel(val, DVFSRC_REG(offset));
}

#define dvfsrc_rmw(offset, val, mask, shift) \
	dvfsrc_write(offset, (dvfsrc_read(offset) & ~(mask << shift)) \
			| (val << shift))

u32 dvfsrc_sram_read(u32 offset)
{
	if (!is_qos_enabled() || offset >= QOS_SRAM_MAX_SIZE)
		return -1;

	return readl(DVFSRC_SRAM_REG(offset));
}

void dvfsrc_sram_write(u32 offset, u32 val)
{
	if (!is_qos_enabled() || offset >= QOS_SRAM_MAX_SIZE)
		return;

	writel(val, DVFSRC_SRAM_REG(offset));
}

u32 qos_sram_read(u32 offset)
{
	return dvfsrc_sram_read(offset);
}

void qos_sram_write(u32 offset, u32 val)
{
	dvfsrc_sram_write(offset, val);
}

static void dvfsrc_set_sw_req(int data, int mask, int shift)
{
	dvfsrc_rmw(DVFSRC_SW_REQ, data, mask, shift);
}

static void dvfsrc_set_sw_req2(int data, int mask, int shift)
{
	dvfsrc_rmw(DVFSRC_SW_REQ2, data, mask, shift);
}

static void dvfsrc_set_vcore_request(int data, int mask, int shift)
{
	dvfsrc_rmw(DVFSRC_VCORE_REQUEST, data, mask, shift);
}

static void dvfsrc_get_timestamp(char *p)
{
	int ret = 0;
	u64 sec = local_clock();
	u64 usec = do_div(sec, 1000000000);

	do_div(usec, 1000000);
	ret = sprintf(p, "%llu.%llu", sec, usec);
	if (ret < 0)
		pr_info("sprintf fail\n");
}

static void dvfsrc_set_force_start(int data)
{
	dvfsrc->opp_forced = 1;
	dvfsrc_get_timestamp(dvfsrc->force_start);
	dvfsrc_write(DVFSRC_FORCE, data);
	dvfsrc_rmw(DVFSRC_BASIC_CONTROL, 1,
			FORCE_EN_TAR_MASK, FORCE_EN_TAR_SHIFT);
}

static void dvfsrc_set_force_end(void)
{
	dvfsrc_write(DVFSRC_FORCE, 0);
}

static void dvfsrc_release_force(void)
{
	dvfsrc_rmw(DVFSRC_BASIC_CONTROL, 0,
			FORCE_EN_TAR_MASK, FORCE_EN_TAR_SHIFT);
	dvfsrc_write(DVFSRC_FORCE, 0);
	dvfsrc_get_timestamp(dvfsrc->force_end);
	dvfsrc->opp_forced = 0;
}

static void dvfsrc_set_sw_bw(int type, int data)
{
	switch (type) {
	case PM_QOS_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_0, data / 100);
		break;
	case PM_QOS_CPU_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_1, data / 100);
		break;
	case PM_QOS_GPU_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_2, data / 100);
		break;
	case PM_QOS_MM_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_3, data / 100);
		break;
	case PM_QOS_OTHER_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_4, data / 100);
		break;
	default:
		break;
	}
}


int is_dvfsrc_opp_fixed(void)
{
	int ret;
	unsigned long flags;

	if (!is_dvfsrc_enabled())
		return 1;

	if (!(dvfsrc_read(DVFSRC_BASIC_CONTROL) & 0x100))
		return 1;

	if (helio_dvfsrc_flag_get() != 0)
		return 1;

	spin_lock_irqsave(&force_req_lock, flags);
	ret = is_opp_forced();
	spin_unlock_irqrestore(&force_req_lock, flags);

	return ret;
}

static int get_target_level(void)
{
	return (dvfsrc_read(DVFSRC_LEVEL) & 0xFFFF);
}

u32 get_dvfs_final_level(void)
{
#if 0
	u32 level_register = dvfsrc_read(DVFSRC_LEVEL);
	u32 target_level, current_level;

	target_level = level_register & 0xFFFF;
	current_level = level_register >> CURRENT_LEVEL_SHIFT;

	if (target_level != 0)
		return min(current_level, target_level);
	else
		return current_level;
#endif
	return dvfsrc_read(DVFSRC_LEVEL) >> CURRENT_LEVEL_SHIFT;
}

int get_sw_req_vcore_opp(void)
{
	int opp = -1;
	int sw_req = -1;

	/* return opp 0, if dvfsrc not enable */
	if (!is_dvfsrc_enabled())
		return 0;
	/* 1st get sw req opp  no lock protect is ok*/
	if (!is_opp_forced()) {
		sw_req = (dvfsrc_read(DVFSRC_SW_REQ) >> VCORE_SW_AP_SHIFT);
		sw_req = sw_req & VCORE_SW_AP_MASK;
		sw_req = VCORE_OPP_NUM - sw_req - 1;
		return sw_req;  /* return sw_request, as vcore floor level*/
	}
	opp = get_cur_vcore_opp();
	return opp; /* return opp , as vcore fixed level*/
}

static int commit_data(int type, int data, int check_spmfw)
{
	int ret = 0;
	int level = 16, opp = 16;
	unsigned long flags;
#if defined(CONFIG_MTK_ENG_BUILD)
	int opp_uv = 0;
	int vcore_uv = 0;
#endif
	if (!is_dvfsrc_enabled())
		return ret;

	if (check_spmfw)
		mtk_spmfw_init(1, 0);

	switch (type) {
	case PM_QOS_MEMORY_BANDWIDTH:
	case PM_QOS_CPU_MEMORY_BANDWIDTH:
	case PM_QOS_GPU_MEMORY_BANDWIDTH:
	case PM_QOS_MM_MEMORY_BANDWIDTH:
	case PM_QOS_OTHER_MEMORY_BANDWIDTH:
		if (data < 0)
			data = 0;
		dvfsrc_set_sw_bw(type, data);
		break;
	case PM_QOS_DDR_OPP:
		mutex_lock(&sw_req1_mutex);
		if (data >= DDR_OPP_NUM || data < 0)
			data = DDR_OPP_NUM - 1;

		opp = data;
		level = DDR_OPP_NUM - data - 1;

		dvfsrc_set_sw_req(level, EMI_SW_AP_MASK, EMI_SW_AP_SHIFT);

		if (!is_opp_forced() && check_spmfw) {
			udelay(1);
			ret = dvfsrc_wait_for_completion(
					get_cur_ddr_opp() <= opp,
					DVFSRC_TIMEOUT);
		}

		mutex_unlock(&sw_req1_mutex);
		break;
	case PM_QOS_VCORE_OPP:
		mutex_lock(&sw_req1_mutex);
		if (data >= VCORE_OPP_NUM || data < 0)
			data = VCORE_OPP_NUM - 1;

		opp = data;
		level = VCORE_OPP_NUM - data - 1;
		mb(); /* make sure setting first */
		dvfsrc_set_sw_req(level, VCORE_SW_AP_MASK, VCORE_SW_AP_SHIFT);
		mb(); /* make sure checking then */
		if (!is_opp_forced() && check_spmfw) {
			udelay(1);
			ret = dvfsrc_wait_for_completion(
					(get_target_level() == 0),
					DVFSRC_TIMEOUT);
			udelay(1);
			ret = dvfsrc_wait_for_completion(
					get_cur_vcore_opp() <= opp,
					DVFSRC_TIMEOUT);
		}
#if defined(CONFIG_MTK_ENG_BUILD)
		if (!is_opp_forced() && check_spmfw) {
			if (vcore_reg_id) {
				vcore_uv = regulator_get_voltage(vcore_reg_id);
				opp_uv = get_vcore_uv_table(opp);
				if (vcore_uv < opp_uv) {
					pr_info("DVFS FAIL = %d %d 0x%x\n",
				vcore_uv, opp_uv, dvfsrc_read(DVFSRC_LEVEL));
					dvfsrc_dump_reg(NULL, 0);
					aee_kernel_warning("DVFSRC",
						"%s: failed.", __func__);
				}
			}
		}
#endif
		mutex_unlock(&sw_req1_mutex);
		break;
	case PM_QOS_SCP_VCORE_REQUEST:
		if (data >= VCORE_OPP_NUM || data < 0)
			data = 0;

		opp = VCORE_OPP_NUM - data - 1;
		level = data;

		dvfsrc_set_vcore_request(level,
				VCORE_SCP_GEAR_MASK, VCORE_SCP_GEAR_SHIFT);

		if (!is_opp_forced() && check_spmfw) {
			udelay(1);
			ret = dvfsrc_wait_for_completion(
					get_cur_vcore_opp() <= opp,
					DVFSRC_TIMEOUT);
		}

		break;
	case PM_QOS_POWER_MODEL_DDR_REQUEST:
		if (data >= DDR_OPP_NUM || data < 0)
			data = 0;

		opp = DDR_OPP_NUM - data - 1;
		level = data;

		dvfsrc_set_sw_req2(level,
				EMI_SW_AP2_MASK, EMI_SW_AP2_SHIFT);
		break;
	case PM_QOS_POWER_MODEL_VCORE_REQUEST:
		if (data >= VCORE_OPP_NUM || data < 0)
			data = 0;

		opp = VCORE_OPP_NUM - data - 1;
		level = data;

		dvfsrc_set_sw_req2(level,
				VCORE_SW_AP2_MASK, VCORE_SW_AP2_SHIFT);
		break;
	case PM_QOS_VCORE_DVFS_FORCE_OPP:
		spin_lock_irqsave(&force_req_lock, flags);
		if (data >= VCORE_DVFS_OPP_NUM || data < 0)
			data = VCORE_DVFS_OPP_NUM;

		opp = data;
		level = VCORE_DVFS_OPP_NUM - data - 1;

		if (opp == VCORE_DVFS_OPP_NUM) {
			dvfsrc_release_force();
			spin_unlock_irqrestore(&force_req_lock, flags);
			break;
		}
		dvfsrc_set_force_start(1 << level);
		if (check_spmfw) {
			udelay(1);
			ret = dvfsrc_wait_for_completion(
					get_cur_vcore_dvfs_opp() == opp,
					DVFSRC_TIMEOUT);
		}

		dvfsrc_set_force_end();
		spin_unlock_irqrestore(&force_req_lock, flags);
		break;
	default:
		break;
	}

	if (ret < 0) {
		pr_info("%s: type: 0x%x, data: 0x%x, opp: %d, level: %d\n",
				__func__, type, data, opp, level);
		dvfsrc_dump_reg(NULL, 0);
		aee_kernel_warning("DVFSRC", "%s: failed.", __func__);
	}

	return ret;
}

static void dvfsrc_restore(void)
{
	int i;

	for (i = PM_QOS_CPU_MEMORY_BANDWIDTH; i < PM_QOS_NUM_CLASSES; i++)
		commit_data(i, pm_qos_request(i), 0);
}

void helio_dvfsrc_enable(int dvfsrc_en)
{
	int ret = 0;
	if (dvfsrc_en > 1 || dvfsrc_en < 0)
		return;

	if (!spm_load_firmware_status())
		return;

	if (dvfsrc->dvfsrc_enabled == dvfsrc_en && dvfsrc_en == 1) {
		pr_info("DVFSRC already enabled\n");
		return;
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	helio_dvfsrc_sspm_ipi_init(dvfsrc_en);
#endif

	dvfsrc->qos_enabled = 1;
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

void dvfsrc_opp_table_init(void)
{
	int i;
	int vcore_opp, ddr_opp;

	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++) {
		vcore_opp = get_vcore_opp(i);
		ddr_opp = get_ddr_opp(i);

		if (vcore_opp == VCORE_OPP_UNREQ || ddr_opp == DDR_OPP_UNREQ) {
			set_opp_table(i, 0, 0);
			continue;
		}
		set_opp_table(i, get_vcore_uv_table(vcore_opp),
				dram_steps_freq(ddr_opp) * 1000);
	}
}

static int helio_dvfsrc_common_init(void)
{
	dvfsrc_opp_level_mapping();
	dvfsrc_opp_table_init();

	if (!spm_load_firmware_status()) {
		pr_info("SPM FIRMWARE IS NOT READY\n");
		return -ENODEV;
	}

	return 0;
}

int dvfsrc_get_emi_bw(int type)
{
	int ret = 0;
	int i;

	if (type == QOS_EMI_BW_TOTAL_AVE) {
		for (i = 0; i < QOS_TOTAL_BW_BUF_SIZE; i++)
			ret += (dvfsrc_sram_read(QOS_TOTAL_BW_BUF(i)) &
						QOS_TOTAL_BW_BUF_BW_MASK);
		ret /= QOS_TOTAL_BW_BUF_SIZE;
	} else
		ret = dvfsrc_sram_read(QOS_TOTAL_BW + 4 * type);

	return ret;
}

int get_vcore_dvfs_level(void)
{
	return dvfsrc_read(DVFSRC_LEVEL) >> CURRENT_LEVEL_SHIFT;
}

int is_qos_enabled(void)
{
	if (dvfsrc)
		return dvfsrc->qos_enabled == 1;
	return 0;
}

int is_dvfsrc_enabled(void)
{
	if (dvfsrc)
		return dvfsrc->dvfsrc_enabled == 1;

	return 0;
}

int is_opp_forced(void)
{
	if (dvfsrc)
		return dvfsrc->opp_forced == 1;

	return 0;
}

static void get_pm_qos_info(char *p)
{
	char timestamp[20];

	dvfsrc_get_timestamp(timestamp);
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
	p += sprintf(p, "%-24s: %d\n",
			"EMI_BW_TOTAL_AVE",
			dvfsrc_get_emi_bw(QOS_EMI_BW_TOTAL_AVE));
	p += sprintf(p, "%-24s: %d\n",
			"EMI_BW_TOTAL", dvfsrc_get_emi_bw(QOS_EMI_BW_TOTAL));
	p += sprintf(p, "%-24s: %d\n",
			"EMI_BW_TOTAL_W",
			dvfsrc_get_emi_bw(QOS_EMI_BW_TOTAL_W));
	p += sprintf(p, "%-24s: %d\n",
			"EMI_BW_CPU", dvfsrc_get_emi_bw(QOS_EMI_BW_CPU));
	p += sprintf(p, "%-24s: %d\n",
			"EMI_BW_GPU", dvfsrc_get_emi_bw(QOS_EMI_BW_GPU));
	p += sprintf(p, "%-24s: %d\n",
			"EMI_BW_MM", dvfsrc_get_emi_bw(QOS_EMI_BW_MM));
	p += sprintf(p, "%-24s: %d\n",
			"EMI_BW_OTHER", dvfsrc_get_emi_bw(QOS_EMI_BW_OTHER));
	p += sprintf(p, "%-24s: %s\n",
			"Current Timestamp", timestamp);
	p += sprintf(p, "%-24s: %s\n",
			"Force Start Timestamp", dvfsrc->force_start);
	p += sprintf(p, "%-24s: %s\n",
			"Force End Timestamp", dvfsrc->force_end);
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

static struct devfreq_dev_profile helio_devfreq_profile = {
	.polling_ms	= 0,
};

void helio_dvfsrc_reg_config(struct reg_config *config)
{
	int idx = 0;

	while (config[idx].offset != -1) {
		dvfsrc_write(config[idx].offset, config[idx].val);
		idx++;
	}
}

void helio_dvfsrc_sram_reg_init(void)
{
	int i;

	for (i = 0; i < 0x80; i += 4)
		dvfsrc_sram_write(i, 0);
}

void dvfsrc_set_power_model_ddr_request(unsigned int level)
{
	commit_data(PM_QOS_POWER_MODEL_DDR_REQUEST, level, 1);
}

static int helio_governor_event_handler(struct devfreq *devfreq,
					unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_SUSPEND:
		break;

	case DEVFREQ_GOV_RESUME:
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

static int pm_qos_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_cpu_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_CPU_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_gpu_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_GPU_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_mm_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_MM_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_other_memory_bw_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_OTHER_MEMORY_BANDWIDTH, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_ddr_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_DDR_OPP, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_vcore_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_VCORE_OPP, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_scp_vcore_request_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_SCP_VCORE_REQUEST, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_power_model_ddr_request_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_POWER_MODEL_DDR_REQUEST, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_power_model_vcore_request_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_POWER_MODEL_VCORE_REQUEST, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_vcore_dvfs_force_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_VCORE_DVFS_FORCE_OPP, l, 1);

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
}

static int helio_dvfsrc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;

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

	platform_set_drvdata(pdev, dvfsrc);

	dvfsrc->devfreq = devm_devfreq_add_device(&pdev->dev,
						 &helio_devfreq_profile,
						 "helio_dvfsrc",
						 NULL);

	ret = helio_dvfsrc_add_interface(&pdev->dev);
	if (ret)
		return ret;

	pm_qos_notifier_register();

	helio_dvfsrc_common_init();
	vcore_reg_id = regulator_get(&pdev->dev, "vcore");
	if (!vcore_reg_id)
		pr_info("regulator_get vcore_reg_id failed\n");

	if (of_property_read_u32(np, "dvfsrc_flag",
		(u32 *) &dvfsrc->dvfsrc_flag))
		dvfsrc->dvfsrc_flag = 0;
	pr_info("dvfsrc_flag = %d\n", dvfsrc->dvfsrc_flag);

	helio_dvfsrc_platform_init(dvfsrc);

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
	{ .compatible = "mediatek,dvfsrc-v2" },
	{ },
};

MODULE_DEVICE_TABLE(of, helio_dvfsrc_of_match);

static __maybe_unused int helio_dvfsrc_suspend(struct device *dev)
{
	int ret = 0;

	ret = devfreq_suspend_device(dvfsrc->devfreq);
	if (ret < 0) {
		pr_info("failed to suspend the devfreq devices\n");
		return ret;
	}

	return 0;
}

static __maybe_unused int helio_dvfsrc_resume(struct device *dev)
{
	int ret = 0;

	ret = devfreq_resume_device(dvfsrc->devfreq);
	if (ret < 0) {
		pr_info("failed to resume the devfreq devices\n");
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
		pr_info("%s: failed to add governor: %d\n", __func__, ret);
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
		pr_info("%s: failed to remove governor: %d\n", __func__, ret);
}
module_exit(helio_dvfsrc_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Helio dvfsrc driver");
