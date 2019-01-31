/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/pm_qos.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mutex.h>
#ifdef PLL_HOPPING_READY
#include <mach/mtk_freqhopping.h>
#endif

#ifdef VCORE_READY
#include <mtk_vcorefs_manager.h>
#endif

#include "mmdvfs_config_util.h"
#include "mmdvfs_pmqos.h"

#ifdef MMDVFS_MMP
#include "mmprofile.h"
#endif

#undef pr_fmt
#define pr_fmt(fmt) "[mmdvfs]" fmt

#define CLK_TYPE_NONE 0
#define CLK_TYPE_MUX 1
#define CLK_TYPE_PLL 2

#ifdef MMDVFS_MMP
struct mmdvfs_mmp_events_t {
	mmp_event mmdvfs;
	mmp_event freq_change;
};
static struct mmdvfs_mmp_events_t mmdvfs_mmp_events;
#endif

static u32 log_level;
enum mmdvfs_log_level {
	mmdvfs_log_freq_change = 0,
};

#define STEP_UNREQUEST -1
struct mm_freq_step_config {
	u32 clk_type; /* 0: don't set, 1: clk_mux, 2: pll hopping */
	struct clk *clk_mux;
	struct clk *clk_source;
	u32 clk_mux_id;
	u32 clk_source_id;
	u32 pll_id;
	u32 pll_value;
};

struct mm_freq_config {
	struct notifier_block nb;
	const char *prop_name;
	u32 pm_qos_class;
	s32 current_step;
	u64 freq_steps[MAX_FREQ_STEP];
	struct mm_freq_step_config step_config[MAX_FREQ_STEP];
};

enum mm_dprop { /*dprop: dts property */
	mm_dp_freq = 0,
	mm_dp_clk_type,		/* 1 */
	mm_dp_clk_param1,	/* 2 */
	mm_dp_clk_mux = mm_dp_clk_param1,
	mm_dp_pll_id = mm_dp_clk_param1,
	mm_dp_clk_param2,	/* 3 */
	mm_dp_clk_source = mm_dp_clk_param2,
	mm_dp_pll_value = mm_dp_clk_param2,
	mm_dp_max /* put max in the end */
};

#define vcore_node_name "vopp_steps"
static u32 step_size;
static s32 vopp_steps[MAX_FREQ_STEP];
static s32 current_max_step = STEP_UNREQUEST;
static s32 force_step = STEP_UNREQUEST;
static bool mmdvfs_enable;
static struct pm_qos_request vcore_request;
static DEFINE_MUTEX(step_mutex);

static int mm_freq_notify(struct notifier_block *nb,
		unsigned long freq_value, void *v);

static struct mm_freq_config disp_freq = {
	.nb.notifier_call = mm_freq_notify,
	.prop_name = "disp_freq",
	.pm_qos_class = PM_QOS_DISP_FREQ,
	.current_step = STEP_UNREQUEST,
};

static struct mm_freq_config mdp_freq = {
	.nb.notifier_call = mm_freq_notify,
	.prop_name = "mdp_freq",
	.pm_qos_class = PM_QOS_MDP_FREQ,
	.current_step = STEP_UNREQUEST,
};

static struct mm_freq_config vdec_freq = {
	.nb.notifier_call = mm_freq_notify,
	.prop_name = "vdec_freq",
	.pm_qos_class = PM_QOS_VDEC_FREQ,
	.current_step = STEP_UNREQUEST,
};

static struct mm_freq_config venc_freq = {
	.nb.notifier_call = mm_freq_notify,
	.prop_name = "venc_freq",
	.pm_qos_class = PM_QOS_VENC_FREQ,
	.current_step = STEP_UNREQUEST,
};

static struct mm_freq_config cam_freq = {
	.nb.notifier_call = mm_freq_notify,
	.prop_name = "cam_freq",
	.pm_qos_class = PM_QOS_CAM_FREQ,
	.current_step = STEP_UNREQUEST,
};

static struct mm_freq_config img_freq = {
	.nb.notifier_call = mm_freq_notify,
	.prop_name = "img_freq",
	.pm_qos_class = PM_QOS_IMG_FREQ,
	.current_step = STEP_UNREQUEST,
};

/* order should be same as pm_qos_class order */
struct mm_freq_config *all_freqs[] = {
	&disp_freq, &mdp_freq, &vdec_freq, &venc_freq, &img_freq, &cam_freq};

static void mm_apply_vcore(s32 vopp)
{
	pm_qos_update_request(&vcore_request, vopp);
}

static s32 mm_apply_clk(struct mm_freq_config *config, u32 step)
{
	struct mm_freq_step_config step_config;
	s32 ret = 0;

	if (step >= MAX_FREQ_STEP) {
		pr_notice(
			"Invalid clk apply step %d in %s\n",
			step, config->prop_name);
		return -EINVAL;
	}

	step_config = config->step_config[step];
	if (step_config.clk_type == CLK_TYPE_NONE) {
		pr_notice("No need to change clk of %s\n", config->prop_name);
		return 0;
	}

	if (config->pm_qos_class == PM_QOS_DISP_FREQ) {
		u64 freq = config->freq_steps[step];

#ifdef MMDVFS_MMP
		mmprofile_log_ex(
			mmdvfs_mmp_events.freq_change,
			MMPROFILE_FLAG_PULSE, step, freq);
#endif
		if (log_level & 1 << mmdvfs_log_freq_change)
			pr_notice(
				"mmdvfs freq change: step (%u), freq: %llu MHz",
				step, freq);
	}

	if (step_config.clk_type == CLK_TYPE_MUX) {

		if (step_config.clk_mux == NULL ||
			step_config.clk_source == NULL) {
			pr_notice("CCF handle can't be NULL during MMDVFS\n");
			return -EINVAL;
		}

		ret = clk_prepare_enable(step_config.clk_mux);

		if (ret) {
			pr_notice("prepare clk(%d): %s-%u\n",
				ret, config->prop_name, step);
			return -EFAULT;
		}

		ret = clk_set_parent(
			step_config.clk_mux, step_config.clk_source);

		if (ret)
			pr_notice(
				"set parent(%d): %s-%u\n",
				ret, config->prop_name, step);

		clk_disable_unprepare(step_config.clk_mux);
		if (ret)
			pr_notice(
				"unprepare clk(%d): %s-%u\n",
				ret, config->prop_name, step);
	} else if (step_config.clk_type == CLK_TYPE_PLL) {
	#ifdef PLL_HOPPING_READY
		ret = mt_dfs_general_pll(
				step_config.pll_id, step_config.pll_value);
	#endif
		pr_notice("pll hopping: (%u)-0x%08x, %s-%u\n",
			step_config.pll_id, step_config.pll_value,
			config->prop_name, step);

		if (ret)
			pr_notice("hopping rate(%d):(%u)-0x%08x, %s-%u\n",
				ret, step_config.pll_id, step_config.pll_value,
				config->prop_name, step);
	}
	return ret;

}

static void mm_apply_clk_for_all(u32 step)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(all_freqs); i++)
		mm_apply_clk(all_freqs[i], step);
}

static void update_step(void)
{
	u32 i;
	s32 old_max_step;

	if (!mmdvfs_enable) {
		pr_notice("mmdvfs qos is disabled\n");
		return;
	}

	if (!step_size) {
		pr_notice("no step available skip\n");
		return;
	}

	mutex_lock(&step_mutex);
	old_max_step = current_max_step;
	current_max_step = step_size;
	if (force_step != STEP_UNREQUEST) {
		current_max_step = force_step;
	} else {
		for (i = 0; i < ARRAY_SIZE(all_freqs); i++) {
			if (all_freqs[i]->current_step != -1 &&
				all_freqs[i]->current_step < current_max_step)
				current_max_step = all_freqs[i]->current_step;
		}
		if (current_max_step == step_size)
			current_max_step = STEP_UNREQUEST;
	}

	if (current_max_step == old_max_step) {
		mutex_unlock(&step_mutex);
		return;
	}

	if (current_max_step != STEP_UNREQUEST
			&& current_max_step < old_max_step) {
		/* configuration for higher freq */
		mm_apply_vcore(vopp_steps[current_max_step]);
		mm_apply_clk_for_all(current_max_step);
	} else {
		/* configuration for lower freq */
		s32 vopp_step = STEP_UNREQUEST;
		u32 freq_step = step_size - 1;

		if (current_max_step != STEP_UNREQUEST) {
			vopp_step = vopp_steps[current_max_step];
			freq_step = current_max_step;
		}
		mm_apply_clk_for_all(freq_step);
		mm_apply_vcore(vopp_step);
	}
	mutex_unlock(&step_mutex);
}

static int mm_freq_notify(struct notifier_block *nb,
		unsigned long freq_value, void *v)
{
	struct mm_freq_config *mm_freq;
	s32 step;

	mm_freq = container_of(nb, struct mm_freq_config, nb);
	if (!step_size) {
		pr_notice(
			"no step available in %s, skip\n", mm_freq->prop_name);
		return NOTIFY_OK;
	}

	step = step_size - 1;
	if (freq_value == PM_QOS_MM_FREQ_DEFAULT_VALUE) {
		mm_freq->current_step = STEP_UNREQUEST;
	} else {
		for (; step >= 1; step--) {
			if (freq_value <= mm_freq->freq_steps[step])
				break;
		}
		mm_freq->current_step = step;
	}
	update_step();

	return NOTIFY_OK;
}

int mmdvfs_qos_get_freq_steps(u32 pm_qos_class,
	u64 *out_freq_steps, u32 *out_step_size)
{
	struct mm_freq_config *mm_freq = NULL;
	u32 i;

	if (!out_freq_steps || !out_step_size)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(all_freqs); i++) {
		if (pm_qos_class == all_freqs[i]->pm_qos_class) {
			mm_freq = all_freqs[i];
			break;
		}
	}

	if (!mm_freq)
		return -ENXIO;

	*out_step_size = step_size;
	memcpy(
		out_freq_steps,
		mm_freq->freq_steps,
		sizeof(mm_freq->freq_steps));
	return 0;
}
EXPORT_SYMBOL_GPL(mmdvfs_qos_get_freq_steps);

static void get_module_clock_by_index(struct device *dev,
	u32 index, struct clk **clk_module)
{
	const char *clk_name;
	s32 result;

	result = of_property_read_string_index(dev->of_node, "clock-names",
		index, &clk_name);
	if (unlikely(result)) {
		pr_notice("Cannot get module name of index (%u), result (%d)\n",
			index, result);
		return;
	}

	*clk_module = devm_clk_get(dev, clk_name);
	if (IS_ERR(*clk_module)) {
		/* error status print */
		pr_notice("Cannot get module clock: %s\n", clk_name);
	} else {
		/* message print */
		pr_notice("Get module clock: %s\n", clk_name);
	}
}

static void mmdvfs_get_step_node(struct device *dev,
	const char *name, struct mm_freq_config *mm_freq, u32 index)
{
	s32 result;
	u32 step[mm_dp_max] = {0};

	result =
		of_property_read_u32_array(dev->of_node, name, step, mm_dp_max);
	if (likely(!result)) {
		mm_freq->freq_steps[index] = step[mm_dp_freq];
		mm_freq->step_config[index].clk_type = step[mm_dp_clk_type];
		if (step[mm_dp_clk_type] == CLK_TYPE_MUX) {
			mm_freq->step_config[index].clk_mux_id =
				step[mm_dp_clk_mux];
			mm_freq->step_config[index].clk_source_id =
				step[mm_dp_clk_source];
			get_module_clock_by_index(dev,
				step[mm_dp_clk_mux],
				&mm_freq->step_config[index].clk_mux);
			get_module_clock_by_index(dev,
				step[mm_dp_clk_source],
				&mm_freq->step_config[index].clk_source);
		} else if (step[mm_dp_clk_type] == CLK_TYPE_PLL) {
			mm_freq->step_config[index].pll_id =
				step[mm_dp_pll_id];
			mm_freq->step_config[index].pll_value =
				step[mm_dp_pll_value];
		}
	} else {
		pr_notice("read freq steps %s failed (%d)\n", name, result);
	}
	pr_notice("%s: %lluMHz, clk:%u/%u/%u\n",
		name, mm_freq->freq_steps[index],
		mm_freq->step_config[index].clk_type,
		step[mm_dp_clk_param1], step[mm_dp_clk_param2]);
}

static int mmdvfs_probe(struct platform_device *pdev)
{
	u32 i, count, vopp;
	const char *name;
	struct device_node *node = pdev->dev.of_node;
	struct property *prop;
	struct mm_freq_config *mm_freq;
	const __be32 *p;

#ifdef MMDVFS_MMP
	mmprofile_enable(1);
	if (mmdvfs_mmp_events.mmdvfs == 0) {
		mmdvfs_mmp_events.mmdvfs =
			mmprofile_register_event(MMP_ROOT_EVENT, "MMDVFS");
		mmdvfs_mmp_events.freq_change =
		mmprofile_register_event(
			mmdvfs_mmp_events.mmdvfs, "freq_change");
		mmprofile_enable_event_recursive(mmdvfs_mmp_events.mmdvfs, 1);
	}
	mmprofile_start(1);
#endif

	mmdvfs_enable = true;
	pm_qos_add_request(&vcore_request, PM_QOS_VCORE_OPP,
		PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	step_size = 0;
	of_property_for_each_u32(node, vcore_node_name, prop, p, vopp) {
		if (step_size >= MAX_FREQ_STEP) {
			pr_notice(
				"vcore_steps is over the MAX_STEP (%d)\n",
				MAX_FREQ_STEP);
			break;
		}
		vopp_steps[step_size] = vopp;
		step_size++;
	}
	pr_notice("vcore_steps: [%u, %u, %u, %u, %u, %u], count:%u\n",
		vopp_steps[0], vopp_steps[1], vopp_steps[2],
		vopp_steps[3], vopp_steps[4], vopp_steps[5], step_size);

	for (i = 0; i < ARRAY_SIZE(all_freqs); i++) {
		count = 0;
		mm_freq = all_freqs[i];
		of_property_for_each_string(
			node, mm_freq->prop_name, prop, name) {
			if (count >= MAX_FREQ_STEP) {
				pr_notice("freq setting %s is over the MAX_STEP (%d)\n",
					mm_freq->prop_name, MAX_FREQ_STEP);
				break;
			}
			mmdvfs_get_step_node(&pdev->dev, name, mm_freq, count);
			count++;
		}
		if (count != step_size)
			pr_notice("freq setting %s is not same as vcore_steps (%d)\n",
				mm_freq->prop_name, step_size);
		pr_notice("%s: step size:%u\n", mm_freq->prop_name, step_size);
		pm_qos_add_notifier(mm_freq->pm_qos_class, &mm_freq->nb);
	}

	return 0;

}

static int mmdvfs_remove(struct platform_device *pdev)
{
	u32 i;

	pm_qos_remove_request(&vcore_request);
	for (i = 0; i < ARRAY_SIZE(all_freqs); i++)
		pm_qos_remove_notifier(
			all_freqs[i]->pm_qos_class, &all_freqs[i]->nb);

	return 0;
}

static const struct of_device_id mmdvfs_of_ids[] = {
	{.compatible = "mediatek,mmdvfs_pmqos",},
	{}
};

static struct platform_driver mmdvfs_pmqos_driver = {
	.probe = mmdvfs_probe,
	.remove = mmdvfs_remove,
	.driver = {
		   .name = "mtk_mmdvfs_pmqos",
		   .owner = THIS_MODULE,
		   .of_match_table = mmdvfs_of_ids,
	}
};

static int __init mmdvfs_pmqos_init(void)
{
	s32 status;

	status = platform_driver_register(&mmdvfs_pmqos_driver);
	if (status != 0) {
		pr_notice(
			"Failed to register MMDVFS-PMQOS driver(%d)\n", status);
		return -ENODEV;
	}

	pr_notice("mmdvfs_pmqos_init\n");
	return 0;
}

static void __exit mmdvfs_pmqos_exit(void)
{
	platform_driver_unregister(&mmdvfs_pmqos_driver);
}

u64 mmdvfs_qos_get_freq(u32 pm_qos_class)
{
	u64 current_freq = 0;
	u32 index = pm_qos_class - PM_QOS_DISP_FREQ;

	if (index >= 0 && index < ARRAY_SIZE(all_freqs) &&
	current_max_step >= 0 && current_max_step < step_size)
		current_freq = all_freqs[index]->freq_steps[current_max_step];

	return current_freq;
}
EXPORT_SYMBOL_GPL(mmdvfs_qos_get_freq);

int dump_setting(char *buf, const struct kernel_param *kp)
{
	u32 i, j, clk_param1, clk_param2, clk_type;
	int length = 0;
	struct mm_freq_config *mm_freq;

	for (i = 0; i < ARRAY_SIZE(all_freqs); i++) {
		mm_freq = all_freqs[i];
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%s] step_size: %u, current_step:%d (%lluMhz)\n",
			mm_freq->prop_name, step_size, mm_freq->current_step,
			mmdvfs_qos_get_freq(mm_freq->pm_qos_class));
		for (j = 0; j < step_size; j++) {
			clk_type = mm_freq->step_config[j].clk_type;
			if (clk_type == CLK_TYPE_MUX) {
				clk_param1 =
					mm_freq->step_config[j].clk_mux_id;
				clk_param2 =
					mm_freq->step_config[j].clk_source_id;
			} else if (clk_type == CLK_TYPE_PLL) {
				clk_param1 =
					mm_freq->step_config[j].pll_id;
				clk_param2 =
					mm_freq->step_config[j].pll_value;
			} else {
				clk_param1 = 0;
				clk_param2 = 0;
			}
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  [step:%u] vopp: %d, freq: %llu, clk(%u/%u/%u)\n",
				j, vopp_steps[j], mm_freq->freq_steps[j],
				mm_freq->step_config[j].clk_type,
				clk_param1, clk_param2);
			if (length >= PAGE_SIZE)
				break;
		}
		if (length >= PAGE_SIZE)
			break;
	}
	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops dump_param_ops = {.get = dump_setting};
module_param_cb(dump_setting, &dump_param_ops, NULL, 0444);
MODULE_PARM_DESC(dump_setting, "dump mmdvfs current setting");

int mmdvfs_qos_force_step(int step)
{
	if (step >= (s32)step_size || step < STEP_UNREQUEST) {
		pr_notice("force set step invalid: %d\n", step);
		return -EINVAL;
	}
	force_step = step;
	update_step();
	return 0;
}
EXPORT_SYMBOL_GPL(mmdvfs_qos_force_step);

int set_force_step(const char *val, const struct kernel_param *kp)
{
	int result;
	int new_force_step;

	result = kstrtoint(val, 0, &new_force_step);
	if (result) {
		pr_notice("force set step failed: %d\n", result);
		return result;
	}
	return mmdvfs_qos_force_step(new_force_step);
}

static struct kernel_param_ops force_step_ops = {
	.set = set_force_step,
	.get = param_get_int,
};
module_param_cb(force_step, &force_step_ops, &force_step, 0644);
MODULE_PARM_DESC(force_step, "force mmdvfs to specified step, -1 for unset");

void mmdvfs_qos_enable(bool enable)
{
	mmdvfs_enable = enable;
	pr_notice("mmdvfs enabled? %d\n", enable);
}
EXPORT_SYMBOL_GPL(mmdvfs_qos_enable);

int set_enable(const char *val, const struct kernel_param *kp)
{
	int result;
	bool enable;

	result = kstrtobool(val, &enable);
	if (result) {
		pr_notice("force set enable: %d\n", result);
		return result;
	}
	mmdvfs_qos_enable(enable);
	return 0;
}

static struct kernel_param_ops mmdvfs_enable_ops = {
	.set = set_enable,
	.get = param_get_bool,
};
module_param_cb(
	mmdvfs_enable, &mmdvfs_enable_ops, &mmdvfs_enable, 0644);
MODULE_PARM_DESC(mmdvfs_enable, "enable or disable mmdvfs");

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmdvfs log level");

arch_initcall(mmdvfs_pmqos_init);
module_exit(mmdvfs_pmqos_exit);

MODULE_DESCRIPTION("MTK MMDVFS driver");
MODULE_AUTHOR("Damon Chu<damon.chu@mediatek.com>");
MODULE_LICENSE("GPL");
