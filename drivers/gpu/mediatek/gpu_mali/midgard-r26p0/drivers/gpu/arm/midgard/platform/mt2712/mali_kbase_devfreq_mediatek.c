/*
 *
 * (C) COPYRIGHT 2014-2018 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <mali_kbase.h>
#include <mali_kbase_tlstream.h>
#include "mali_kbase_config_platform.h"
#include <backend/gpu/mali_kbase_pm_internal.h>
#ifdef CONFIG_DEVFREQ_THERMAL
#include <mali_kbase_runtime_pm.h>
#endif
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/devfreq.h>
#ifdef CONFIG_DEVFREQ_THERMAL
#include <linux/devfreq_cooling.h>
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#include <linux/pm_opp.h>
#else /* Linux >= 3.13 */
/* In 3.13 the OPP include header file, types, and functions were all
 * renamed. Use the old filename for the include, and define the new names to
 * the old, when an old kernel is detected.
 */
#include <linux/opp.h>
#define dev_pm_opp opp
#define dev_pm_opp_get_voltage opp_get_voltage
#define dev_pm_opp_get_opp_count opp_get_opp_count
#define dev_pm_opp_find_freq_ceil opp_find_freq_ceil
#define dev_pm_opp_find_freq_floor opp_find_freq_floor
#endif /* Linux >= 3.13 */
#include "mali_kbase_devfreq_mediatek.h"
#include "platform/mtk_platform_common.h"

static struct kbase_device *g_kbdev;


/**
 * opp_translate - Translate nominal OPP frequency from devicetree into real
 *                 frequency and core mask
 * @kbdev:     Device pointer
 * @freq:      Nominal frequency
 * @core_mask: Pointer to u64 to store core mask to
 *
 * Return: Real target frequency
 *
 * This function will only perform translation if an operating-points-v2-mali
 * table is present in devicetree. If one is not present then it will return an
 * untranslated frequency and all cores enabled.
 */
static unsigned long opp_translate(struct kbase_device *kbdev,
		unsigned long freq, u64 *core_mask)
{
	int i;

	for (i = 0; i < kbdev->num_opps; i++) {
		if (kbdev->opp_table[i].opp_freq == freq) {
			*core_mask = kbdev->opp_table[i].core_mask;
			return kbdev->opp_table[i].real_freq;
		}
	}

	/* Failed to find OPP - return all cores enabled & nominal frequency */
	*core_mask = kbdev->gpu_props.props.raw_props.shader_present;

	return freq;
}

static s32
kbase_devfreq_target(struct device *dev, u_long *target_freq, u32 flags)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	unsigned long nominal_freq;
	u_long freq = 0;
	u64 voltage;
	s32 err;
	u64 core_mask;
	const struct mfg_base *mfg = (struct mfg_base *)kbdev->platform_context;

	freq = *target_freq;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_lock();
#endif
	opp = devfreq_recommended_opp(dev, &freq, flags);
	voltage = dev_pm_opp_get_voltage(opp);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_unlock();
#endif
	if (IS_ERR_OR_NULL(opp)) {
		dev_notice(dev, "Failed to get opp (%ld)\n", PTR_ERR(opp));
		return PTR_ERR(opp);
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	dev_pm_opp_put(opp);
#endif

	nominal_freq = freq;

	/*
	 * Only update if there is a change of frequency
	 */
	if (kbdev->current_nominal_freq == nominal_freq) {
		*target_freq = nominal_freq;
		return 0;
	}

	freq = opp_translate(kbdev, nominal_freq, &core_mask);
#ifdef CONFIG_REGULATOR
	if ((kbdev->regulator != NULL) && (kbdev->current_voltage != voltage)
			&& (kbdev->current_freq < freq)) {
		err = regulator_set_voltage(kbdev->regulator, (s32)voltage,
						(s32)voltage);
		if (err != 0) {
			dev_notice(dev, "Failed to increase voltage (%d)\n",
								err);
			return err;
		}
	}
#endif

	err = clk_set_rate(mfg->mfg_pll, freq);
	if (err != 0) {
		dev_notice(dev, "Failed to set clock %lu (target %lu)\n",
				freq, *target_freq);
		return err;
	}

#ifdef CONFIG_REGULATOR
	if ((kbdev->regulator != NULL) && (kbdev->current_voltage != voltage)
			&& (kbdev->current_freq > freq)) {
		err = regulator_set_voltage(kbdev->regulator, (s32)voltage,
						(s32)voltage);
		if (err != 0) {
			dev_notice(dev, "Failed to decrease voltage (%d)\n",
								err);
			return err;
		}
	}
#endif

	kbase_devfreq_set_core_mask(kbdev, core_mask);

	*target_freq = nominal_freq;
	kbdev->current_voltage = voltage;
	kbdev->current_nominal_freq = nominal_freq;
	kbdev->current_freq = freq;
	kbdev->current_core_mask = core_mask;

	KBASE_TLSTREAM_AUX_DEVFREQ_TARGET((u64)nominal_freq);

	return err;
}

static s32
kbase_devfreq_cur_freq(struct device *dev, u_long *freq)
{
	const struct kbase_device *kbdev = dev_get_drvdata(dev);

	*freq = kbdev->current_nominal_freq;

	return 0;
}

static s32
kbase_devfreq_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	struct kbasep_pm_metrics diff;

	kbase_pm_get_dvfs_metrics(kbdev, &kbdev->last_devfreq_metrics, &diff);

	stat->busy_time = diff.time_busy;
	stat->total_time = diff.time_busy + diff.time_idle;
	stat->current_frequency = kbdev->current_nominal_freq;
	stat->private_data = NULL;

	return 0;
}

static s32 kbase_devfreq_init_freq_table(const struct kbase_device *kbdev,
		struct devfreq_dev_profile *dp)
{
	s32 count;
	s32 i = 0;
	u_long freq = 0;
	struct dev_pm_opp *opp;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_lock();
#endif
	count = dev_pm_opp_get_opp_count(kbdev->dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_unlock();
#endif
	if (count < 0)
		return count;

	dp->freq_table = kmalloc_array((u_long)count, sizeof(dp->freq_table[0]),
				GFP_KERNEL);
	if (dp->freq_table == NULL)
		return -ENOMEM;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_lock();
#endif
	for (i = 0, freq = ULONG_MAX; i < count; i++, freq--) {
		opp = dev_pm_opp_find_freq_floor(kbdev->dev, &freq);
		if (IS_ERR(opp))
			break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
		dev_pm_opp_put(opp);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0) */

		dp->freq_table[i] = freq;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_unlock();
#endif

	if (count != i)
		dev_notice(kbdev->dev, "Unable to enumerate all OPPs (%d!=%d\n",
				count, i);

	dp->max_state = (u32)i;

	return 0;
}

static void kbase_devfreq_term_freq_table(const struct kbase_device *kbdev)
{
	const struct devfreq_dev_profile *dp = kbdev->devfreq->profile;

	kfree(dp->freq_table);
}

static void kbase_devfreq_exit(struct device *dev)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	kbase_devfreq_term_freq_table(kbdev);
}

static int kbase_devfreq_init_core_mask_table(struct kbase_device *kbdev)
{
	struct device_node *opp_node = of_parse_phandle(kbdev->dev->of_node,
			"operating-points-v2", 0);
	struct device_node *node;
	int i = 0;
	int count;
	u64 shader_present = kbdev->gpu_props.props.raw_props.shader_present;

	if (!opp_node)
		return 0;
	if (!of_device_is_compatible(opp_node, "operating-points-v2-mali"))
		return 0;

	count = dev_pm_opp_get_opp_count(kbdev->dev);
	kbdev->opp_table = kmalloc_array(count,
			sizeof(struct kbase_devfreq_opp), GFP_KERNEL);
	if (!kbdev->opp_table)
		return -ENOMEM;

	for_each_available_child_of_node(opp_node, node) {
		u64 core_mask;
		u64 opp_freq, real_freq;
		const void *core_count_p;

		if (of_property_read_u64(node, "opp-hz", &opp_freq)) {
			dev_warn(kbdev->dev, "OPP is missing required opp-hz property\n");
			continue;
		}
		if (of_property_read_u64(node, "opp-hz-real", &real_freq))
			real_freq = opp_freq;
		if (of_property_read_u64(node, "opp-core-mask", &core_mask))
			core_mask = shader_present;
		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_11056) &&
				core_mask != shader_present) {
			dev_warn(kbdev->dev, "Ignoring OPP %llu - Dynamic Core Scaling not supported on this GPU\n",
					opp_freq);
			continue;
		}

		core_count_p = of_get_property(node, "opp-core-count", NULL);
		if (core_count_p) {
			u64 remaining_core_mask =
				kbdev->gpu_props.props.raw_props.shader_present;
			int core_count = be32_to_cpup(core_count_p);

			core_mask = 0;

			for (; core_count > 0; core_count--) {
				int core = ffs(remaining_core_mask);

				if (!core) {
					dev_err(kbdev->dev, "OPP has more cores than GPU\n");
					return -ENODEV;
				}

				core_mask |= (1ull << (core-1));
				remaining_core_mask &= ~(1ull << (core-1));
			}
		}

		if (!core_mask) {
			dev_err(kbdev->dev, "OPP has invalid core mask of 0\n");
			return -ENODEV;
		}

		kbdev->opp_table[i].opp_freq = opp_freq;
		kbdev->opp_table[i].real_freq = real_freq;
		kbdev->opp_table[i].core_mask = core_mask;

		dev_info(kbdev->dev, "OPP %d : opp_freq=%llu real_freq=%llu core_mask=%llx\n",
				i, opp_freq, real_freq, core_mask);

		i++;
	}

	kbdev->num_opps = i;

	return 0;
}

s32 kbase_devfreq_init(struct kbase_device *kbdev)
{
	struct devfreq_dev_profile *dp;
	s32 err, ret;
	const struct mfg_base *mfg = (struct mfg_base *)kbdev->platform_context;

	ret = 0;
	g_kbdev = kbdev;

	if (mfg->mfg_pll == NULL)
		return -ENODEV;

	kbdev->current_freq = clk_get_rate(mfg->mfg_pll);
	kbdev->current_nominal_freq = kbdev->current_freq;

	dp = &kbdev->devfreq_profile;

	dp->initial_freq = kbdev->current_freq;
	dp->polling_ms = 100;
	dp->target = kbase_devfreq_target;
	dp->get_dev_status = kbase_devfreq_status;
	dp->get_cur_freq = kbase_devfreq_cur_freq;
	dp->exit = kbase_devfreq_exit;

	if (kbase_devfreq_init_freq_table(kbdev, dp) != 0)
		return -EFAULT;

	if (dp->max_state > 0) {
		/* Record the maximum frequency possible */
		kbdev->gpu_props.props.core_props.gpu_freq_khz_max =
			dp->freq_table[0] / 1000;
	};

	err = kbase_devfreq_init_core_mask_table(kbdev);
	if (err)
		return err;

	kbdev->devfreq = devfreq_add_device(kbdev->dev, dp,
				"performance", NULL);
	if (IS_ERR(kbdev->devfreq)) {
		kfree(dp->freq_table);
		return (s32)PTR_ERR(kbdev->devfreq);
	}

	/* devfreq_add_device only copies a few of kbdev->dev's fields, so
	 * set drvdata explicitly so IPA models can access kbdev. */
	dev_set_drvdata(&kbdev->devfreq->dev, kbdev);

	err = devfreq_register_opp_notifier(kbdev->dev, kbdev->devfreq);
	if (err != 0) {
		dev_notice(kbdev->dev,
			"Failed to register OPP notifier (%d)\n", err);
		goto opp_notifier_failed;
	}

	kbdev->devfreq->min_freq = GPU_FREQ_KHZ_MIN * 1000;
	kbdev->devfreq->max_freq = GPU_FREQ_KHZ_MAX * 1000;

#ifdef CONFIG_DEVFREQ_THERMAL
	err = kbase_power_model_simple_init(kbdev);
	if ((err != 0) && (err != -ENODEV) && (err != -EPROBE_DEFER)) {
		dev_notice(kbdev->dev,
			"Failed to initialize simple power model (%d)\n",
			err);
		goto cooling_failed;
	}
	if (err == -EPROBE_DEFER)
		goto cooling_failed;
	if (err != -ENODEV) {
		kbdev->devfreq_cooling = of_devfreq_cooling_register_power(
				kbdev->dev->of_node,
				kbdev->devfreq,
				&power_model_simple_ops);
		if (IS_ERR_OR_NULL(kbdev->devfreq_cooling)) {
			err = PTR_ERR(kbdev->devfreq_cooling);
			dev_notice(kbdev->dev,
				"Failed to register cooling device (%d)\n",
				err);
			goto cooling_failed;
		}
	} else {
		err = 0;
	}
#endif

	kbdev->pm.debug_core_mask[0] = mfg->gpu_core_mask;
	kbdev->pm.debug_core_mask[1] = mfg->gpu_core_mask;
	kbdev->pm.debug_core_mask[2] = mfg->gpu_core_mask;
	kbdev->pm.debug_core_mask_all = mfg->gpu_core_mask;

	return 0;

#ifdef CONFIG_DEVFREQ_THERMAL
cooling_failed:
	ret = devfreq_unregister_opp_notifier(kbdev->dev, kbdev->devfreq);
#endif /* CONFIG_DEVFREQ_THERMAL */
opp_notifier_failed:
	if (devfreq_remove_device(kbdev->devfreq) != 0)
		dev_notice(kbdev->dev, "Failed to terminate devfreq (%d)\n",
								err);
	else
		kbdev->devfreq = NULL;

	return err;
}

void kbase_devfreq_term(struct kbase_device *kbdev)
{
	s32 err;

	dev_dbg(kbdev->dev, "Term Mali devfreq\n");

#ifdef CONFIG_DEVFREQ_THERMAL
	if (kbdev->devfreq_cooling != NULL)
		devfreq_cooling_unregister(kbdev->devfreq_cooling);

	kbase_ipa_term(kbdev);
#endif

	err = devfreq_unregister_opp_notifier(kbdev->dev, kbdev->devfreq);

	err = devfreq_remove_device(kbdev->devfreq);
	if (err != 0)
		dev_notice(kbdev->dev, "Failed to terminate devfreq (%d)\n",
								err);
	else
		kbdev->devfreq = NULL;

	kfree(kbdev->opp_table);
}

KBASE_EXPORT_TEST_API(mtk_kbase_report_gpu_memory_usage)
unsigned int mtk_kbase_report_gpu_memory_usage(void)
{
	struct list_head *entry;
	const struct list_head *kbdev_list;
	int gpu_pages = 0;

	kbdev_list = kbase_dev_list_get();
	list_for_each(entry, kbdev_list) {
		struct kbase_device *kbdev = NULL;

		kbdev = list_entry(entry, struct kbase_device, entry);
		gpu_pages = atomic_read(&(kbdev->memdev.used_pages));
	}
	kbase_dev_list_put(kbdev_list);

	return ((unsigned int)gpu_pages * 4096U);
}

KBASE_EXPORT_TEST_API(mtk_kbase_report_gpu_loading)
unsigned int mtk_kbase_report_gpu_loading(void)
{
	struct kbasep_pm_metrics diff;

	kbase_pm_get_dvfs_metrics(g_kbdev, &g_kbdev->last_devfreq_metrics, &diff);

	return ((unsigned int)(diff.time_busy  * 100U / (diff.time_busy + diff.time_idle)));
}

KBASE_EXPORT_TEST_API(mtk_kbase_report_cur_freq)
unsigned int mtk_kbase_report_cur_freq(void)
{
	unsigned long freq;

	int ret = kbase_devfreq_cur_freq(g_kbdev->dev, &freq);

	if (ret != 0)
		return 0;

	return (unsigned int)(freq / 1000UL);
}

void mtk_kbase_gpu_debug_init(void)
{
	mtk_get_gpu_memory_usage_fp = mtk_kbase_report_gpu_memory_usage;
	mtk_get_gpu_loading_fp = mtk_kbase_report_gpu_loading;
	mtk_get_gpu_freq_fp = mtk_kbase_report_cur_freq;
}