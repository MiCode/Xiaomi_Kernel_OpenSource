// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <mali_kbase.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <platform/mtk_platform_common.h>
#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MALI_MTK_DVFS_POLICY)
#include "mtk_gpu_dvfs.h"
#endif
#include <mtk_gpufreq.h>
#include <ged_dvfs.h>
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif

static bool mfg_powered;
static DEFINE_MUTEX(mfg_pm_lock);
static struct kbase_device *mali_kbdev;
#if IS_ENABLED(CONFIG_PROC_FS)
static struct proc_dir_entry *mtk_mali_root;
#endif

struct kbase_device *mtk_common_get_kbdev(void)
{
	return mali_kbdev;
}

bool mtk_common_pm_is_mfg_active(void)
{
	return mfg_powered;
}

void mtk_common_pm_mfg_active(void)
{
	mutex_lock(&mfg_pm_lock);
	mfg_powered = true;
	mutex_unlock(&mfg_pm_lock);
}

void mtk_common_pm_mfg_idle(void)
{
	mutex_lock(&mfg_pm_lock);
	mfg_powered = false;
	mutex_unlock(&mfg_pm_lock);
}

int mtk_common_gpufreq_commit(int opp_idx)
{
	int ret = -1;

	mutex_lock(&mfg_pm_lock);
	if (opp_idx >= 0 && mtk_common_pm_is_mfg_active()) {
		ret = mt_gpufreq_target(opp_idx, KIR_POLICY);
	}
	mutex_unlock(&mfg_pm_lock);

	return ret;
}

int mtk_common_ged_dvfs_get_last_commit_idx(void)
{
#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MALI_MTK_DVFS_POLICY)
	return (int)ged_dvfs_get_last_commit_idx();
#else
	return -1;
#endif
}

#if IS_ENABLED(CONFIG_PROC_FS)
static int mtk_common_gpu_utilization_show(struct seq_file *m, void *v)
{
#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MALI_MTK_DVFS_POLICY)
	unsigned int util_active, util_3d, util_ta, util_compute, cur_opp_idx;

	mtk_common_update_gpu_utilization();

	cur_opp_idx = mt_gpufreq_get_cur_freq_index();

	util_active = mtk_common_get_util_active();
	util_3d = mtk_common_get_util_3d();
	util_ta = mtk_common_get_util_ta();
	util_compute = mtk_common_get_util_compute();

	seq_printf(m, "ACTIVE=%u 3D/TA/COMPUTE=%u/%u/%u OPP_IDX=%u MFG_PWR=%d\n",
	           util_active, util_3d, util_ta, util_compute, cur_opp_idx, mfg_powered);
#else
	seq_puts(m, "GPU DVFS doesn't be enabled\n");
#endif

	return 0;
}

DEFINE_PROC_SHOW_ATTRIBUTE(mtk_common_gpu_utilization);

void mtk_common_procfs_init(void)
{
  	mtk_mali_root = proc_mkdir("mtk_mali", NULL);
  	if (!mtk_mali_root) {
  		pr_info("cannot create /proc/%s\n", "mtk_mali");
  		return;
  	}
	proc_create("utilization", 0444, mtk_mali_root, &mtk_common_gpu_utilization_proc_ops);
}

void mtk_common_procfs_exit(void)
{
	mtk_mali_root = NULL;
	remove_proc_entry("utilization", mtk_mali_root);
	remove_proc_entry("mtk_mali", NULL);
}
#endif

int mtk_common_device_init(struct kbase_device *kbdev)
{
	if (!kbdev) {
		pr_info("@%s: kbdev is NULL\n", __func__);
		return -1;
	}

	mali_kbdev = kbdev;

#if IS_ENABLED(CONFIG_MALI_MTK_IOMMU_V2)
	if (g_ion_device) {
		kbdev->client = ion_client_create(g_ion_device, "mali_kbase");
	}

	if (kbdev->client == NULL) {
		pr_info("@%s: create ion client failed!\n", __func__);
	}
#endif

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MALI_MTK_DVFS_POLICY)
#if IS_ENABLED(CONFIG_MALI_MTK_DVFS_LOADING_MODE)
	ged_dvfs_cal_gpu_utilization_ex_fp = mtk_common_cal_gpu_utilization_ex;
#else
	ged_dvfs_cal_gpu_utilization_fp = mtk_common_cal_gpu_utilization;
#endif
	ged_dvfs_gpu_freq_commit_fp = mtk_common_ged_dvfs_commit;
#endif

	return 0;
}

void mtk_common_device_term(struct kbase_device *kbdev)
{
	if (!kbdev) {
		pr_info("@%s: kbdev is NULL\n", __func__);
		return;
	}

#if IS_ENABLED(CONFIG_MALI_MTK_IOMMU_V2)
	if (kbdev->client != NULL) {
		ion_client_destroy(kbdev->client);
	}
#endif

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MALI_MTK_DVFS_POLICY)
#if IS_ENABLED(CONFIG_MALI_MTK_DVFS_LOADING_MODE)
	ged_dvfs_cal_gpu_utilization_ex_fp = NULL;
#else
	ged_dvfs_cal_gpu_utilization_fp = NULL;
#endif
	ged_dvfs_gpu_freq_commit_fp = NULL;
#endif
}
