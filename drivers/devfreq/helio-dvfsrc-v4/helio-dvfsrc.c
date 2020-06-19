// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
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
#include <linux/sched/clock.h>
#include <linux/of.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <mt-plat/aee.h>
#include <mt-plat/mtk_secure_api.h>
#include <mtk_dramc.h>

#include "helio-dvfsrc.h"
#include "mtk_dvfsrc_reg.h"

static struct regulator *vcore_reg_id;
static struct helio_dvfsrc *dvfsrc;
static DEFINE_MUTEX(sw_req1_mutex);
static DEFINE_SPINLOCK(force_req_lock);
static DEFINE_SPINLOCK(spm_lock);
static int local_spm_load_firmware_status = 1;

#define DVFSRC_REG(offset) (dvfsrc->regs + offset)

enum vcorefs_smc_cmd {
	VCOREFS_SMC_CMD_0,
	VCOREFS_SMC_CMD_1,
	VCOREFS_SMC_CMD_2,
	VCOREFS_SMC_CMD_3,
	NUM_VCOREFS_SMC_CMD,
};

u32 spm_read(u32 offset)
{
	unsigned int val;

	regmap_read(dvfsrc->spm_map, offset, &val);

	return val;
}

static void spm_dvfsfw_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&spm_lock, flags);
	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_0, 0, 0, 0);

	spin_unlock_irqrestore(&spm_lock, flags);
}

void spm_go_to_vcorefs(int spm_flags)
{
	unsigned long flags;

	spin_lock_irqsave(&spm_lock, flags);
	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_1, spm_flags, 0, 0);

	spin_unlock_irqrestore(&spm_lock, flags);
}

void spm_dvfs_pwrap_cmd(int pwrap_cmd, int pwrap_vcore)
{
	unsigned long flags;

	spin_lock_irqsave(&spm_lock, flags);
	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_3, pwrap_cmd, pwrap_vcore, 0);

	spin_unlock_irqrestore(&spm_lock, flags);
}

u32 spm_get_dvfs_level(void)
{
	return spm_read(SPM_SW_RSV_9) & 0xFFFF;
}

u32 spm_get_dvfs_final_level(void)
{
	int rsv9 = spm_read(SPM_SW_RSV_9) & 0xFFFF;
	int event_sta = spm_read(SPM_DVFS_EVENT_STA) & 0xFFFF;

	if (event_sta != 0)
		return min(rsv9, event_sta);
	else
		return rsv9;
}

int is_spm_enabled(void)
{
	return spm_read(PCM_REG15_DATA) != 0 ? 1 : 0;
}

void mtk_spmfw_init(int dvfsrc_en, int skip_check)
{
	if (is_spm_enabled() != 0x0 && !skip_check)
		return;

	spm_dvfsfw_init();

	spm_go_to_vcorefs(dvfsrc->dvfsrc_flag);
}

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

static void dvfsrc_set_sw_req(int data, int mask, int shift)
{
	dvfsrc_rmw(DVFSRC_SW_REQ, data, mask, shift);
}

void dvfsrc_set_sw_req2(int data, int mask, int shift)
{
	dvfsrc_rmw(DVFSRC_SW_REQ2, data, mask, shift);
}

static void dvfsrc_get_timestamp(char *p)
{
	u64 sec = local_clock();
	u64 usec = do_div(sec, 1000000000);

	do_div(usec, 1000000);
	sprintf(p, "%llu.%llu", sec, usec);
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

static int get_target_level(void)
{
	return (dvfsrc_read(DVFSRC_LEVEL) & 0xFFFF);
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

static int commit_data(int type, int data, int check_spmfw)
{
	int ret = 0;
	int level = 16, opp = 16;
	unsigned long flags;
	int opp_uv = 0;
	int vcore_uv = 0;

	if (!is_dvfsrc_enabled())
		return ret;

	if ((spm_get_spmfw_idx() >= SPMFW_PC4_2CH_2667) &&
	   ((type == PM_QOS_EMI_OPP) ||
	   (type == PM_QOS_POWER_MODEL_DDR_REQUEST)))
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
	case PM_QOS_EMI_OPP:
		mutex_lock(&sw_req1_mutex);
		if (data >= DDR_OPP_NUM || data < 0)
			data = DDR_OPP_NUM - 1;

		opp = data;
		level = DDR_OPP_NUM - data - 1;

		dvfsrc_set_sw_req(level, EMI_SW_AP_MASK, EMI_SW_AP_SHIFT);

		if (!is_opp_forced() && check_spmfw) {
			ret = dvfsrc_wait_for_completion(
					get_cur_ddr_opp() <= opp,
					DVFSRC_TIMEOUT);
		}

		mutex_unlock(&sw_req1_mutex);
		break;
	case PM_QOS_VCORE_OPP:
		mutex_lock(&sw_req1_mutex);
		if (spm_get_spmfw_idx() >= SPMFW_PC4_2CH_2667) {
			if (data >= VCORE_OPP_1)
				data = VCORE_OPP_1;
		}

		if (data >= VCORE_OPP_NUM || data < 0)
			data = VCORE_OPP_NUM - 1;

		opp = data;

		if (spm_get_spmfw_idx() >= SPMFW_PC4_2CH_2667)
			level = VCORE_OPP_2 - data - 1;
		else
			level = VCORE_OPP_NUM - data - 1;

		dvfsrc_set_sw_req(level, VCORE_SW_AP_MASK, VCORE_SW_AP_SHIFT);

		if (!is_opp_forced() && check_spmfw) {
			ret = dvfsrc_wait_for_completion(
					(get_target_level() == 0),
					DVFSRC_TIMEOUT);
			ret = dvfsrc_wait_for_completion(
					get_cur_vcore_opp() <= opp,
					DVFSRC_TIMEOUT);
			if (vcore_reg_id) {
				vcore_uv = regulator_get_voltage(vcore_reg_id);
				opp_uv = get_vcore_uv_table(opp);
				if (vcore_uv < opp_uv) {
					pr_info("DVFS FAIL = %d %d 0x%x\n",
				vcore_uv, opp_uv, dvfsrc_read(DVFSRC_LEVEL));
					dvfsrc_dump_reg(NULL);
					aee_kernel_warning("DVFSRC",
						"%s: failed.", __func__);
				}
			}
		}

		mutex_unlock(&sw_req1_mutex);
		break;
	case PM_QOS_POWER_MODEL_DDR_REQUEST:
		if (data >= DDR_OPP_NUM || data < 0)
			data = 0;

		opp = DDR_OPP_NUM - data - 1;
		level = data;

		dvfsrc_set_sw_req2(level,
				EMI_SW_AP2_MASK, EMI_SW_AP2_SHIFT);
		break;
	case PM_QOS_VCORE_DVFS_FIXED_OPP:
		spin_lock_irqsave(&force_req_lock, flags);
		if (data >= VCORE_DVFS_OPP_NUM || data < 0)
			data = VCORE_DVFS_OPP_NUM;
		else
			data++;

		opp = data;
		level = VCORE_DVFS_OPP_NUM - data - 1;
		pr_info("DVFS PM_QOS_VCORE_DVFS_FIXED_OPP = %d %d\n",
			data, level);
		if (opp >= VCORE_DVFS_OPP_NUM) {
			dvfsrc_release_force();
			spin_unlock_irqrestore(&force_req_lock, flags);
			break;
		}
		dvfsrc_set_force_start(1 << level);
		if (check_spmfw) {
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
		dev_err(dvfsrc->dev,
			"type: 0x%x, data: 0x%x, opp: %d, level: %d\n", type,
			data, opp, level);
		dvfsrc_dump_reg(NULL);
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

int spm_load_firmware_status(void)
{
	if (local_spm_load_firmware_status == 1)
		local_spm_load_firmware_status =
		mt_secure_call(MTK_SIP_KERNEL_SPM_FIRMWARE_STATUS, 0, 0, 0, 0);

	if (!local_spm_load_firmware_status)
		dev_err(dvfsrc->dev, "SPM FIRMWARE IS NOT READY\n");
	/* -1 not init, 0: not loaded, 1: loaded, 2: loaded and kicked */
	return local_spm_load_firmware_status;
}

void helio_dvfsrc_enable(int dvfsrc_en)
{
	if (dvfsrc_en > 1 || dvfsrc_en < 0)
		return;

	if (!spm_load_firmware_status())
		return;

	dvfsrc->qos_enabled = 1;
	dvfsrc->dvfsrc_enabled = dvfsrc_en;
	dvfsrc->opp_forced = 0;
	sprintf(dvfsrc->force_start, "0");
	sprintf(dvfsrc->force_end, "0");

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
			"PM_QOS_EMI_OPP",
			pm_qos_request(PM_QOS_EMI_OPP));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_VCORE_OPP",
			pm_qos_request(PM_QOS_VCORE_OPP));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_FORCE_OPP",
			pm_qos_request(PM_QOS_VCORE_DVFS_FIXED_OPP));
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_PM_DDR_REQ",
			pm_qos_request(PM_QOS_POWER_MODEL_DDR_REQUEST));
	p += sprintf(p, "%-24s: %s\n",
			"Current Timestamp", timestamp);
	p += sprintf(p, "%-24s: %s\n",
			"Force Start Timestamp", dvfsrc->force_start);
	p += sprintf(p, "%-24s: %s\n",
			"Force End Timestamp", dvfsrc->force_end);
}

char *dvfsrc_dump_reg(char *ptr)
{
	char buf[1300];

	memset(buf, '\0', sizeof(buf));
	get_opp_info(buf);
	if (ptr)
		ptr += sprintf(ptr, "%s\n", buf);
	else
		pr_info("%s\n", buf);

	memset(buf, '\0', sizeof(buf));
	get_dvfsrc_reg(buf);
	if (ptr)
		ptr += sprintf(ptr, "%s\n", buf);
	else
		pr_info("%s\n", buf);

	memset(buf, '\0', sizeof(buf));
	get_dvfsrc_record(buf);
	if (ptr)
		ptr += sprintf(ptr, "%s\n", buf);
	else
		pr_info("%s\n", buf);

	memset(buf, '\0', sizeof(buf));
	get_dvfsrc_record_latch(buf);
	if (ptr)
		ptr += sprintf(ptr, "%s\n", buf);
	else
		pr_info("%s\n", buf);

	memset(buf, '\0', sizeof(buf));
	get_pm_qos_info(buf);
	if (ptr)
		ptr += sprintf(ptr, "%s\n", buf);
	else
		pr_info("%s\n", buf);

	return ptr;
}

void helio_dvfsrc_reg_config(struct reg_config *config)
{
	int idx = 0;

	while (config[idx].offset != -1) {
		dvfsrc_write(config[idx].offset, config[idx].val);
		idx++;
	}
}

void dvfsrc_set_power_model_ddr_request(unsigned int level)
{
	commit_data(PM_QOS_POWER_MODEL_DDR_REQUEST, level, 1);
}

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
	commit_data(PM_QOS_EMI_OPP, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_vcore_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_VCORE_OPP, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_vcore_dvfs_force_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_VCORE_DVFS_FIXED_OPP, l, 1);

	return NOTIFY_OK;
}

static int pm_qos_power_model_ddr_request_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_POWER_MODEL_DDR_REQUEST, l, 1);

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
	dvfsrc->pm_qos_vcore_dvfs_force_opp_nb.notifier_call =
		pm_qos_vcore_dvfs_force_opp_notify;
	dvfsrc->pm_qos_power_model_ddr_request_nb.notifier_call =
		pm_qos_power_model_ddr_request_notify;

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
	pm_qos_add_notifier(PM_QOS_EMI_OPP,
			&dvfsrc->pm_qos_ddr_opp_nb);
	pm_qos_add_notifier(PM_QOS_VCORE_OPP,
			&dvfsrc->pm_qos_vcore_opp_nb);
	pm_qos_add_notifier(PM_QOS_VCORE_DVFS_FIXED_OPP,
			&dvfsrc->pm_qos_vcore_dvfs_force_opp_nb);
	pm_qos_add_notifier(PM_QOS_POWER_MODEL_DDR_REQUEST,
			&dvfsrc->pm_qos_power_model_ddr_request_nb);
}

static int helio_dvfsrc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dvfsrc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	dvfsrc->spm_map =
		syscon_regmap_lookup_by_compatible("mediatek,mt8168-scpsys");
	if (IS_ERR(dvfsrc->spm_map)) {
		dev_err(dvfsrc->dev, "no syscon\n");
		return -ENODEV;
	}

	dvfsrc->clk_dvfsrc = devm_clk_get(dvfsrc->dev, "dvfsrc");
	if (IS_ERR(dvfsrc->clk_dvfsrc)) {
		dev_err(dvfsrc->dev, "failed to get clock: %ld\n",
			PTR_ERR(dvfsrc->clk_dvfsrc));
		return PTR_ERR(dvfsrc->clk_dvfsrc);
	}

	ret = clk_prepare_enable(dvfsrc->clk_dvfsrc);
	if (ret) {
		dev_err(dvfsrc->dev, "failed to enable clock: %ld\n",
			PTR_ERR(dvfsrc->clk_dvfsrc));
		return ret;
	}

	if (of_property_read_u32(np, "dvfsrc_flag",
	   (u32 *) &dvfsrc->dvfsrc_flag))
		dvfsrc->dvfsrc_flag = 0;


	platform_set_drvdata(pdev, dvfsrc);

	ret = helio_dvfsrc_add_interface(&pdev->dev);
	if (ret)
		return ret;

	pm_qos_notifier_register();

	vcore_reg_id = regulator_get(&pdev->dev, "vcore");
	if (!vcore_reg_id)
		pr_info("regulator_get vcore_reg_id failed\n");

	dvfsrc_opp_level_mapping();
	dvfsrc_opp_table_init();
	spm_load_firmware_status();

	helio_dvfsrc_platform_init(dvfsrc);

	dev_info(dvfsrc->dev, "init done\n");

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

static struct platform_driver helio_dvfsrc_driver = {
	.probe	= helio_dvfsrc_probe,
	.remove	= helio_dvfsrc_remove,
	.driver = {
		.name = "helio-dvfsrc",
		.of_match_table = helio_dvfsrc_of_match,
	},
};

static int __init helio_dvfsrc_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&helio_dvfsrc_driver);
	if (ret)
		pr_err("%s: failed to add driver: %d\n", __func__, ret);

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
