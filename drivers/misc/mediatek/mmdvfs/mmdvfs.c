/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#if IS_ENABLED(BUILD_MMDVFS)
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/math64.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#ifdef PLL_HOPPING_READY
#include <mtk_freqhopping_drv.h>
#endif

#include "mmdvfs_pmqos.h"


#include "mmdvfs_plat.h"
#include <mt-plat/aee.h>

#ifdef APPLY_CLK_LOG
#ifdef SMI_LAF
#include "mt6779_clkmgr.h"
#endif
#endif
#include "smi_pmqos.h"
#include "smi_public.h"

#include <helio-dvfsrc-opp.h>

#ifdef MMDVFS_MMP
#include "mmprofile.h"
#endif

#include "swpm_me.h"

#include <linux/regulator/consumer.h>
static struct regulator *vcore_reg_id;


#undef pr_fmt
#define pr_fmt(fmt) "[mmdvfs]" fmt

#define CLK_TYPE_NONE 0
#define CLK_TYPE_MUX 1
#define CLK_TYPE_PLL 2

#ifdef MMDVFS_MMP
struct mmdvfs_mmp_events_t {
	mmp_event mmdvfs;
	mmp_event freq_change;
	mmp_event ext_freq_change;
	mmp_event limit_change;
};
static struct mmdvfs_mmp_events_t mmdvfs_mmp_events;
#endif

static u32 log_level;
enum mmdvfs_log_level {
	log_freq = 0,
	log_limit,
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
	u64 freq_step;
};

struct mm_freq_limit_config {
	u32 limit_size;
	u32 limit_level;
	u32 limit_value;
	struct mm_freq_step_config **limit_steps;
};

struct mm_freq_config {
	struct notifier_block nb;
	const char *prop_name;
	u32 pm_qos_class;
	s32 current_step;
	struct mm_freq_step_config step_config[MAX_FREQ_STEP];
	struct mm_freq_limit_config limit_config;
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

#define FMETER_MUX_NODE_NAME "fmeter_mux_ids"
#define MAX_MUX_SIZE 9
static u32 mux_size;
static u32 fmeter_mux_ids[MAX_MUX_SIZE];
#ifdef APPLY_CLK_LOG
static u32 mux_real_freqs[MAX_MUX_SIZE];
#endif

#define VCORE_NODE_NAME "vopp_steps"
#define MAX_USER_SIZE (12) /* Must be multiple of 4 */
static u32 step_size;
static s32 vopp_steps[MAX_FREQ_STEP];
static s32 current_max_step = STEP_UNREQUEST;
static s32 force_step = STEP_UNREQUEST;
static bool mmdvfs_enable;
static bool mmdvfs_autok_enable;
static struct mtk_pm_qos_request vcore_request;
static DEFINE_MUTEX(step_mutex);

#ifdef MMDVFS_SKIP_SMI_CONFIG
static bool skip_smi_config = true;
#else
static bool skip_smi_config;
#endif

/* order should be same as pm_qos_class order for mmdvfs_qos_get_freq() */
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

static struct mm_freq_config dpe_freq = {
	.nb.notifier_call = mm_freq_notify,
	.prop_name = "dpe_freq",
	.pm_qos_class = PM_QOS_DPE_FREQ,
	.current_step = STEP_UNREQUEST,
};

static struct mm_freq_config ipe_freq = {
	.prop_name = "ipe_freq",
	.pm_qos_class = PM_QOS_RESERVED,
	.current_step = STEP_UNREQUEST,
};

static struct mm_freq_config ccu_freq = {
	.prop_name = "ccu_freq",
	.pm_qos_class = PM_QOS_RESERVED,
	.current_step = STEP_UNREQUEST,
};

static struct mm_freq_config img2_freq = {
	.prop_name = "img2_freq",
	.pm_qos_class = PM_QOS_RESERVED,
	.current_step = STEP_UNREQUEST,
};

struct mm_freq_config *all_freqs[] = {
	&disp_freq, &mdp_freq,
	&vdec_freq, &venc_freq,
	&img_freq, &cam_freq, &dpe_freq, &ipe_freq, &ccu_freq, &img2_freq};

int __attribute__ ((weak)) is_dvfsrc_opp_fixed(void) { return 1; }


static void mm_apply_vcore(s32 vopp)
{
	mtk_pm_qos_update_request(&vcore_request, vopp);

	if (vcore_reg_id) {
#ifdef CHECK_VOLTAGE
		u32 v_real, v_target;

		if (vopp >= 0 && vopp < VCORE_OPP_NUM) {
			v_real = regulator_get_voltage(vcore_reg_id);
			v_target = get_vcore_uv_table(vopp);
			if (v_real < v_target) {
				pr_info("err vcore %d < %d\n",
					v_real, v_target);
				if (!is_dvfsrc_opp_fixed())
					aee_kernel_warning("mmdvfs",
						"vcore(%d)<target(%d)\n",
						v_real, v_target);
			}
		}
#endif
	}
}

static s32 mm_set_mux_clk(s32 src_mux_id, const char *name,
		struct mm_freq_step_config *step_config, u32 step)
{
	s32 ret = 0;

	if (step_config->clk_mux == NULL ||
		step_config->clk_source == NULL) {
		pr_notice("CCF handle can't be NULL during MMDVFS\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(step_config->clk_mux);

	if (ret) {
		pr_notice("prepare clk(%d): %s-%u\n",
			ret, name, step);
		return -EFAULT;
	}

	ret = clk_set_parent(
		step_config->clk_mux, step_config->clk_source);

	if (ret)
		pr_notice(
			"set parent(%d): %s-%u\n",
			ret, name, step);
#ifdef APPLY_CLK_LOG
	if (step_config->clk_mux_id == src_mux_id)
		mux_real_freqs[src_mux_id] =
			mt_get_ckgen_freq(fmeter_mux_ids[src_mux_id])/1000;
#endif

	clk_disable_unprepare(step_config->clk_mux);
	if (ret)
		pr_notice(
			"unprepare clk(%d): %s-%u\n",
			ret, name, step);
	return ret;
}

static s32 mm_set_freq_hopping_clk(const char *name,
		struct mm_freq_step_config *step_config, u32 step)
{
	s32 ret = 0;

#ifdef PLL_HOPPING_READY
	ret = mt_dfs_general_pll(
			step_config->pll_id, step_config->pll_value);
#endif

	if (ret)
		pr_notice("hopping rate(%d):(%u)-0x%08x, %s-%u\n",
			ret, step_config->pll_id, step_config->pll_value,
			name, step);
	return ret;
}

static s32 apply_clk_by_type(u32 clk_type, s32 src_mux_id,
	const char *name, struct mm_freq_step_config *config, s32 step)
{
	s32 ret = 0;

	if (clk_type == CLK_TYPE_MUX)
		ret = mm_set_mux_clk(src_mux_id, name, config, step);
	else if (clk_type == CLK_TYPE_PLL)
		ret = mm_set_freq_hopping_clk(name, config, step);
	return ret;
}

static void mm_check_limit(struct mm_freq_config *config,
	struct mm_freq_step_config **step_config, u32 step)
{
	struct mm_freq_step_config *normal_step = &config->step_config[step];
	struct mm_freq_step_config *limit_step;
	u32 level = config->limit_config.limit_level;

	if (unlikely(level)) {
		limit_step = &config->limit_config.limit_steps[level-1][step];
		*step_config = limit_step;
		if (log_level & 1 << log_limit)
			pr_notice(
				"limit %s: freq %llu -> %llu in step %u\n",
				config->prop_name, normal_step->freq_step,
				limit_step->freq_step, step);
#ifdef MMDVFS_MMP
		mmprofile_log_ex(
			mmdvfs_mmp_events.freq_change,
			MMPROFILE_FLAG_PULSE, limit_step->freq_step,
			config->pm_qos_class);
#endif
	} else {
		*step_config = normal_step;
	}
}

static s32 mm_apply_clk(s32 src_mux_id,
	struct mm_freq_config *config, u32 step, s32 old_step)
{
	struct mm_freq_step_config *step_config;
	s32 ret = 0;
	s32 operations[2];
	u32 i;

	if (step >= MAX_FREQ_STEP) {
		pr_notice(
			"Invalid clk apply step %d in %s\n",
			step, config->prop_name);
		return -EINVAL;
	}

	mm_check_limit(config, &step_config, step);

	if (step_config->clk_type == CLK_TYPE_NONE) {
		pr_notice("No need to change clk of %s\n", config->prop_name);
		return 0;
	}

	operations[0] = (step < old_step) ? CLK_TYPE_PLL : CLK_TYPE_MUX;
	operations[1] = (step < old_step) ? CLK_TYPE_MUX : CLK_TYPE_PLL;

	for (i = 0; i < ARRAY_SIZE(operations); i++) {
		if (step_config->clk_type & operations[i])
			ret = apply_clk_by_type(operations[i], src_mux_id,
				config->prop_name, step_config, step);
	}

	return ret;

}

/*
 * Each freq occupies 8 bits => 0~3:current_step 4~7:id
 * (id is mapping to index of all_freqs)
 */
static inline u32 set_freq_for_log(u32 freq, s32 cur_step, u32 id)
{
	cur_step &= 0xF;
	id <<= 4;
	return (freq | cur_step | id);
}

static void mm_apply_clk_for_all(u32 pm_qos_class, s32 src_mux_id,
	u32 step, s32 old_step)
{
	u32 i;
	u32 clk_mux_id;
	u32 real_freq = 0;
	u8 freq[MAX_USER_SIZE] = {0};
	bool set[ARRAY_SIZE(all_freqs)] = {false};
	u32 first_log;

	for (i = 0; i < ARRAY_SIZE(all_freqs); i++) {
		clk_mux_id = all_freqs[i]->step_config[step].clk_mux_id;
		if (!set[clk_mux_id]) {
			if (!mm_apply_clk(src_mux_id,
				all_freqs[i], step, old_step))
				set[clk_mux_id] = true;
		}
#ifdef APPLY_CLK_LOG
		if (all_freqs[i]->pm_qos_class == pm_qos_class)
			real_freq = mux_real_freqs[clk_mux_id];
#endif
		freq[i] = set_freq_for_log(
			freq[i], all_freqs[i]->current_step, i);
	}
	set_swpm_me_freq(all_freqs[3]->step_config[step].freq_step,
			all_freqs[2]->step_config[step].freq_step,
			all_freqs[1]->step_config[step].freq_step);
	first_log = (pm_qos_class << 16) | step;

#ifdef MMDVFS_MMP
	mmprofile_log_ex(
		mmdvfs_mmp_events.freq_change,
		MMPROFILE_FLAG_PULSE, first_log, real_freq);

	mmprofile_log_ex(
		mmdvfs_mmp_events.ext_freq_change,
		MMPROFILE_FLAG_PULSE, *((u32 *)&freq[0]), *((u32 *)&freq[4]));
#endif
	if (log_level & 1 << log_freq)
		pr_notice(
			"freq change:%u class:%u step:%u f0:%x f1:%x\n",
			real_freq, pm_qos_class, step,
			*((u32 *)&freq[0]), *((u32 *)&freq[4]));
}

static u32 mmdvfs_get_limit_status(u32 pm_qos_class)
{
	u32 i = pm_qos_class - PM_QOS_DISP_FREQ;

	if (i >= ARRAY_SIZE(all_freqs)) {
		pr_notice("[GET]Invalid class: %u\n", pm_qos_class);
		return false;
	}

	return all_freqs[i]->limit_config.limit_level;
}

static void update_step(u32 pm_qos_class, s32 src_mux_id)
{
	u32 i;
	s32 old_max_step;

	if (!mmdvfs_enable || !mmdvfs_autok_enable) {
		pr_notice("mmdvfs qos is disabled(%d)\n", pm_qos_class);
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
			&& (current_max_step < old_max_step
				|| old_max_step == STEP_UNREQUEST)) {
		/* configuration for higher freq */
		mm_apply_vcore(vopp_steps[current_max_step]);
		mm_apply_clk_for_all(pm_qos_class, src_mux_id,
			current_max_step, old_max_step);
	} else {
		/* configuration for lower freq */
		s32 vopp_step = STEP_UNREQUEST;
		u32 freq_step = step_size - 1;

		if (current_max_step != STEP_UNREQUEST) {
			vopp_step = vopp_steps[current_max_step];
			freq_step = current_max_step;
		}
		mm_apply_clk_for_all(
			pm_qos_class, src_mux_id, freq_step, old_max_step);
		mm_apply_vcore(vopp_step);
	}
	mutex_unlock(&step_mutex);

	if (!skip_smi_config) {
		/* update bwl due to freq change */
		mm_qos_update_larb_bwl(0xFFFF, false);
	}
}

module_param(skip_smi_config, bool, 0644);
MODULE_PARM_DESC(skip_smi_config, "mmqos smi config");

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
			if (freq_value <= mm_freq->step_config[step].freq_step)
				break;
		}
		mm_freq->current_step = step;
	}
	update_step(mm_freq->pm_qos_class,
		mm_freq->step_config[step].clk_mux_id);

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
	for (i = 0; i < step_size; i++)
		out_freq_steps[i] = mm_freq->step_config[i].freq_step;

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
		*clk_module = NULL;
	} else {
		/* message print */
		pr_notice("Get module clock: %s\n", clk_name);
	}
}

static void mmdvfs_get_step_node(struct device *dev,
	const char *name, struct mm_freq_step_config *step_config)
{
	s32 result;
	u32 step[mm_dp_max] = {0};

	result = of_property_read_u32_array(dev->of_node,
		name, step, mm_dp_max);
	if (likely(!result)) {
		step_config->freq_step = step[mm_dp_freq];
		step_config->clk_type |= step[mm_dp_clk_type];
		if (step[mm_dp_clk_type] == CLK_TYPE_MUX) {
			step_config->clk_mux_id =
				step[mm_dp_clk_mux];
			step_config->clk_source_id =
				step[mm_dp_clk_source];
			get_module_clock_by_index(dev,
				step[mm_dp_clk_mux],
				&step_config->clk_mux);
			get_module_clock_by_index(dev,
				step[mm_dp_clk_source],
				&step_config->clk_source);
		} else if (step[mm_dp_clk_type] == CLK_TYPE_PLL) {
			step_config->pll_id =
				step[mm_dp_pll_id];
			step_config->pll_value =
				step[mm_dp_pll_value];
		}
		pr_notice("%s: %lluMHz, clk:%u/%u/%u\n",
			name, step_config->freq_step,
			step_config->clk_type,
			step[mm_dp_clk_param1], step[mm_dp_clk_param2]);
	} else {
		pr_notice("read freq steps %s failed (%d)\n", name, result);
	}
}

static void mmdvfs_get_step_array_node(struct device *dev,
	const char *freq_name, struct mm_freq_step_config step_configs[])
{
	struct property *prop;
	u32 count = 0;
	const char *name;
	char ext_name[32] = {0};

	pr_notice("start get step node of %s\n", freq_name);
	of_property_for_each_string(dev->of_node, freq_name, prop, name) {
		if (count >= MAX_FREQ_STEP) {
			pr_notice("freq setting %s is over the MAX_STEP (%d)\n",
				freq_name, MAX_FREQ_STEP);
			break;
		}
		pr_notice(" node name %s\n", name);
		mmdvfs_get_step_node(dev, name, &step_configs[count]);
		strncpy(ext_name, name, sizeof(ext_name)-1);
		strncat(ext_name, "_ext",
			sizeof(ext_name)-strlen(name)-1);
		mmdvfs_get_step_node(dev,
			ext_name, &step_configs[count]);
		count++;
	}
	if (count != step_size)
		pr_notice("freq setting %s is not same as vcore_steps (%d)\n",
			freq_name, step_size);
	pr_notice("%s: step size:%u\n", freq_name, step_size);
}

static void mmdvfs_get_limit_step_node(struct device *dev,
	const char *freq_name,
	struct mm_freq_limit_config *limit_config)
{
#ifdef MMDVFS_LIMIT
	s32 result, i;
	char ext_name[32] = {0};
	u32 limit_size = 0;

	strncpy(ext_name, freq_name, sizeof(ext_name)-1);
	strncat(ext_name, "_limit_size",
		sizeof(ext_name)-strlen(freq_name)-1);
	result = of_property_read_u32(dev->of_node, ext_name, &limit_size);
	if (result < 0 || !limit_size)
		return;

	pr_notice("[limit]%s size: %u\n", freq_name, limit_size);
	limit_config->limit_size = limit_size;
	limit_config->limit_steps = kcalloc(limit_size,
		sizeof(*limit_config->limit_steps), GFP_KERNEL);
	for (i = 0; i < limit_size; i++) {
		limit_config->limit_steps[i] = kcalloc(MAX_FREQ_STEP,
			sizeof(*limit_config->limit_steps[i]), GFP_KERNEL);
		result = snprintf(ext_name, sizeof(ext_name) - 1,
			"%s_limit_%d", freq_name, i);
		if (result < 0) {
			pr_notice("snprint fail(%d) freq=%s id=%d\n",
				result, freq_name, i);
			continue;
		}
		pr_notice("[limit]%s-%d: %s\n", freq_name, i, ext_name);
		mmdvfs_get_step_array_node(dev, ext_name,
			limit_config->limit_steps[i]);
	}
#else
	pr_notice("MMDVFS limit is off\n");
#endif
}

static int mmdvfs_probe(struct platform_device *pdev)
{
	u32 i, value = 0;
	struct device_node *node = pdev->dev.of_node;
	struct property *prop;
	struct mm_freq_config *mm_freq;
	const __be32 *p;
	u64 freq_steps[MAX_FREQ_STEP] = {0};

#ifdef MMDVFS_MMP
	mmprofile_enable(1);
	if (mmdvfs_mmp_events.mmdvfs == 0) {
		mmdvfs_mmp_events.mmdvfs =
			mmprofile_register_event(MMP_ROOT_EVENT, "MMDVFS");
		mmdvfs_mmp_events.freq_change =	mmprofile_register_event(
			mmdvfs_mmp_events.mmdvfs, "freq_change");
		mmdvfs_mmp_events.ext_freq_change = mmprofile_register_event(
			mmdvfs_mmp_events.mmdvfs, "ext_freq_change");
		mmdvfs_mmp_events.limit_change = mmprofile_register_event(
			mmdvfs_mmp_events.mmdvfs, "limit_change");
		mmprofile_enable_event_recursive(mmdvfs_mmp_events.mmdvfs, 1);
	}
	mmprofile_start(1);
#endif

	mmdvfs_enable = true;
	mmdvfs_autok_enable = true;
	mtk_pm_qos_add_request(&vcore_request, MTK_PM_QOS_VCORE_OPP,
		MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	step_size = 0;
	of_property_for_each_u32(node, VCORE_NODE_NAME, prop, p, value) {
		if (step_size >= MAX_FREQ_STEP) {
			pr_notice(
				"vcore_steps is over the MAX_STEP (%d)\n",
				MAX_FREQ_STEP);
			break;
		}
		vopp_steps[step_size] = value;
		step_size++;
	}

	mux_size = 0;
	of_property_for_each_u32(node, FMETER_MUX_NODE_NAME, prop, p, value) {
		if (mux_size >= MAX_MUX_SIZE) {
			pr_notice(
				"fmeter_mux_ids is over the MAX_MUX_SIZE (%d)\n",
				MAX_MUX_SIZE);
			break;
		}
		fmeter_mux_ids[mux_size] = value;
		mux_size++;
	}

	pr_notice("vcore_steps: [%u, %u, %u, %u, %u, %u], count:%u\n",
		vopp_steps[0], vopp_steps[1], vopp_steps[2],
		vopp_steps[3], vopp_steps[4], vopp_steps[5], step_size);

	for (i = 0; i < ARRAY_SIZE(all_freqs); i++) {
		mm_freq = all_freqs[i];
		mmdvfs_get_step_array_node(&pdev->dev, mm_freq->prop_name,
			mm_freq->step_config);

		if (likely(mm_freq->pm_qos_class >= PM_QOS_DISP_FREQ)) {
			mtk_pm_qos_add_notifier(mm_freq->pm_qos_class,
				&mm_freq->nb);
			pr_notice("%s: add notifier\n", mm_freq->prop_name);
		}

		mmdvfs_get_limit_step_node(&pdev->dev, mm_freq->prop_name,
			&mm_freq->limit_config);
	}

	mmdvfs_qos_get_freq_steps(PM_QOS_DISP_FREQ, freq_steps, &value);
	pr_notice("disp step size:%u\n", value);
	for (i = 0; i < value && i < MAX_FREQ_STEP; i++)
		pr_notice(" - step[%d]: %llu\n", i, freq_steps[i]);

	vcore_reg_id = regulator_get(&pdev->dev, "vcore");
	if (!vcore_reg_id)
		pr_info("regulator_get vcore_reg_id failed\n");
	return 0;

}

static int mmdvfs_remove(struct platform_device *pdev)
{
	u32 i;

	mtk_pm_qos_remove_request(&vcore_request);

	for (i = 0; i < ARRAY_SIZE(all_freqs); i++)
		mtk_pm_qos_remove_notifier(
			all_freqs[i]->pm_qos_class, &all_freqs[i]->nb);

	return 0;
}

static const struct of_device_id mmdvfs_of_ids[] = {
	{.compatible = "mediatek,mmdvfs",},
	{}
};

static struct platform_driver mmdvfs_driver = {
	.probe = mmdvfs_probe,
	.remove = mmdvfs_remove,
	.driver = {
		   .name = "mtk_mmdvfs",
		   .owner = THIS_MODULE,
		   .of_match_table = mmdvfs_of_ids,
	}
};

static int __init mmdvfs_init(void)
{
#ifdef CONFIG_FPGA_EARLY_PORTING
	return 0;
#else
	s32 status;

	status = platform_driver_register(&mmdvfs_driver);
	if (status != 0) {
		pr_notice(
			"Failed to register MMDVFS driver(%d)\n", status);
		return -ENODEV;
	}

	pr_notice("%s\n", __func__);
	return 0;
#endif /* CONFIG_FPGA_EARLY_PORTING */
}

static void __exit mmdvfs_exit(void)
{
	platform_driver_unregister(&mmdvfs_driver);
}


static int __init mmdvfs_late_init(void)
{
#ifdef MMDVFS_FORCE_STEP0
	mmdvfs_qos_force_step(0);
	mmdvfs_enable = false;
	pr_notice("force set step0 when late_init\n");
#else
	mmdvfs_qos_force_step(0);
	mmdvfs_qos_force_step(-1);
	pr_notice("force flip step0 when late_init\n");
#endif
	init_me_swpm();
	return 0;
}

u64 mmdvfs_qos_get_freq(u32 pm_qos_class)
{
	u32 i = pm_qos_class - PM_QOS_DISP_FREQ;
	u32 l, s;

	if (!step_size)
		return 0;
	if (i >= ARRAY_SIZE(all_freqs))
		i = 0;
	if (current_max_step < 0 || current_max_step >= step_size)
		s = step_size - 1;
	else
		s = current_max_step;
	l = all_freqs[i]->limit_config.limit_level;
	if (l)
		return all_freqs[i]->limit_config.limit_steps[l-1][s].freq_step;
	return all_freqs[i]->step_config[s].freq_step;
}
EXPORT_SYMBOL_GPL(mmdvfs_qos_get_freq);

void mmdvfs_qos_limit_config(u32 pm_qos_class, u32 limit_value,
	enum mmdvfs_limit_source source)
{
	u32 i = pm_qos_class - PM_QOS_DISP_FREQ;
	s32 old_level = 0;

	if (unlikely(i >= ARRAY_SIZE(all_freqs))) {
		pr_notice("[%d]Invalid class=%u %d\n",
			source, pm_qos_class, old_level);
		return;
	}

	if (!all_freqs[i]->limit_config.limit_size) {
		pr_notice("[%d]Not support limit: %u\n", source, pm_qos_class);
		return;
	}

	if (log_level & log_limit)
		pr_notice("[%d][%d]limit score update=(%d, %u, %u)\n",
			source, pm_qos_class, limit_value,
			all_freqs[i]->limit_config.limit_value,
			all_freqs[i]->limit_config.limit_level);

#ifdef MMDVFS_LIMIT
	mutex_lock(&step_mutex);
	old_level = all_freqs[i]->limit_config.limit_level;
	mmdvfs_update_limit_config(source, limit_value,
		&all_freqs[i]->limit_config.limit_value,
		&all_freqs[i]->limit_config.limit_level);

	if (old_level != all_freqs[i]->limit_config.limit_level) {
		pr_notice("MMDVFS limit level changed for %s %d->%d\n",
			all_freqs[i]->prop_name, old_level,
			all_freqs[i]->limit_config.limit_level);
		mm_apply_clk(-1, all_freqs[i], current_max_step,
			current_max_step);
	}
	mutex_unlock(&step_mutex);
#endif
#ifdef MMDVFS_MMP
	mmprofile_log_ex(
		mmdvfs_mmp_events.limit_change, MMPROFILE_FLAG_PULSE,
		all_freqs[i]->limit_config.limit_value, pm_qos_class);
#endif
}
EXPORT_SYMBOL_GPL(mmdvfs_qos_limit_config);

static int print_freq(char *buf, int length,
	struct mm_freq_step_config step_configs[], s32 current_step)
{
	u32 i;

	for (i = 0; i < step_size; i++) {
		length += snprintf(buf + length, PAGE_SIZE - length,
			(i == current_step) ? " v" : "  ");
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%u]vopp=%d freq=%llu clk=%u/%u/%u/%u/0x%08x\n",
			i, vopp_steps[i],
			step_configs[i].freq_step,
			step_configs[i].clk_type,
			step_configs[i].clk_mux_id,
			step_configs[i].clk_source_id,
			step_configs[i].pll_id,
			step_configs[i].pll_value);
		if (length >= PAGE_SIZE)
			break;
	}
	return length;
}

#define MAX_DUMP (PAGE_SIZE - 1)
int dump_setting(char *buf, const struct kernel_param *kp)
{
	u32 i, l;
	int length = 0;
	struct mm_freq_config *mm_freq;

	length += snprintf(buf + length, MAX_DUMP  - length,
		"force_step: %d\n", force_step);
	for (i = 0; i < ARRAY_SIZE(all_freqs); i++) {
		mm_freq = all_freqs[i];
		length += snprintf(buf + length, MAX_DUMP  - length,
			"[%s] step_size: %u current_step:%d (%lluMhz)\n",
			mm_freq->prop_name, step_size, mm_freq->current_step,
			mmdvfs_qos_get_freq(PM_QOS_DISP_FREQ + i));
		length = print_freq(buf, length,
			mm_freq->step_config, mm_freq->current_step);
		l = mm_freq->limit_config.limit_level;
		if (l) {
			length += snprintf(buf + length, MAX_DUMP  - length,
				"-[limit] level=%u value=0x%x\n",
				mm_freq->limit_config.limit_level,
				mm_freq->limit_config.limit_value);
			length = print_freq(buf, length,
				mm_freq->limit_config.limit_steps[l-1],
				mm_freq->current_step);
		}
		if (length >= MAX_DUMP)
			break;
	}
	if (length >= MAX_DUMP)
		length = MAX_DUMP - 1;

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
	update_step(PM_QOS_NUM_CLASSES, -1);
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

void mmdvfs_autok_qos_enable(bool enable)
{
	pr_notice("%s: step_size=%d current_max_step=%d\n",
		__func__, step_size, current_max_step);
	if (!enable && step_size > 0 && current_max_step == STEP_UNREQUEST)
		mmdvfs_qos_force_step(step_size - 1);

	mmdvfs_autok_enable = enable;
	if (enable && step_size > 0)
		mmdvfs_qos_force_step(-1);
	pr_notice("mmdvfs_autok enabled? %d\n", enable);
}
EXPORT_SYMBOL_GPL(mmdvfs_autok_qos_enable);

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

void mmdvfs_prepare_action(enum mmdvfs_prepare_event event)
{
	if (event == MMDVFS_PREPARE_CALIBRATION_START) {
		mmdvfs_autok_qos_enable(false);
		pr_notice("mmdvfs service is disabled for calibration\n");
	} else if (event == MMDVFS_PREPARE_CALIBRATION_END) {
		mmdvfs_autok_qos_enable(true);
		pr_notice("mmdvfs service is enabled after calibration\n");
	} else {
		pr_notice("%s: unknown event code:%d\n", __func__, event);
	}
}

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmdvfs log level");

static s32 vote_freq;
static bool vote_req_init;
struct mtk_pm_qos_request vote_req;
int set_vote_freq(const char *val, const struct kernel_param *kp)
{
	int result;
	int new_vote_freq;

	result = kstrtoint(val, 0, &new_vote_freq);
	if (result) {
		pr_notice("force set step failed: %d\n", result);
		return result;
	}

	if (!vote_req_init) {
		mtk_pm_qos_add_request(
			&vote_req, PM_QOS_DISP_FREQ,
			PM_QOS_MM_FREQ_DEFAULT_VALUE);
		vote_req_init = true;
	}
	vote_freq = new_vote_freq;
	mtk_pm_qos_update_request(&vote_req, vote_freq);
	return 0;
}
static struct kernel_param_ops vote_freq_ops = {
	.set = set_vote_freq,
	.get = param_get_int,
};

module_param_cb(vote_freq, &vote_freq_ops, &vote_freq, 0644);
MODULE_PARM_DESC(vote_freq, "vote mmdvfs to specified freq, 0 for unset");

static s32 mmdvfs_ut_case;
int mmdvfs_ut_set(const char *val, const struct kernel_param *kp)
{
	int result;
	int value1, value2;
	u32 old_log_level = log_level;
	struct mtk_pm_qos_request disp_req = {};

	result = sscanf(val, "%d %d", &mmdvfs_ut_case, &value1);
	if (result != 2) {
		pr_notice("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	pr_notice("%s (case_id, value): (%d,%d)\n",
		__func__, mmdvfs_ut_case, value1);

	log_level = 1 << log_freq |
		1 << log_limit;
	mtk_pm_qos_add_request(&disp_req, PM_QOS_DISP_FREQ,
		PM_QOS_MM_FREQ_DEFAULT_VALUE);

	switch (mmdvfs_ut_case) {
	case 0:
		result = sscanf(val, "%d %d %d", &mmdvfs_ut_case,
			&value1, &value2);
		if (result != 3) {
			pr_notice("invalid arguments: %s\n", val);
			break;
		}
		pr_notice("limit test score: %d\n", value2);
		pr_notice("limit initial: %d\n",
			mmdvfs_get_limit_status(value1));
		/* limit enable then opp1 -> opp0 */
		mmdvfs_qos_limit_config(value1, 1, MMDVFS_LIMIT_THERMAL);
		mmdvfs_qos_limit_config(value1, value2, MMDVFS_LIMIT_CAM);
		mtk_pm_qos_update_request(&disp_req, 1000);
		pr_notice("limit enable then opp up: %d freq=%llu MHz\n",
			mmdvfs_get_limit_status(value1),
			mmdvfs_qos_get_freq(value1));
		/* limit disable when opp0 */
		mmdvfs_qos_limit_config(value1, 0, MMDVFS_LIMIT_THERMAL);
		pr_notice("limit disable when opp up: %d freq=%llu MHz\n",
			mmdvfs_get_limit_status(value1),
			mmdvfs_qos_get_freq(value1));
		/* limit enable when opp0 */
		mmdvfs_qos_limit_config(value1, 1, MMDVFS_LIMIT_THERMAL);
		pr_notice("limit enable when opp up: %d freq=%llu MHz\n",
			mmdvfs_get_limit_status(value1),
			mmdvfs_qos_get_freq(value1));
		/* limit disable then opp0 -> opp1 */
		mmdvfs_qos_limit_config(value1, 0, MMDVFS_LIMIT_THERMAL);
		mtk_pm_qos_update_request(&disp_req, 0);
		pr_notice("limit disable then opp down: %d freq=%llu MHz\n",
			mmdvfs_get_limit_status(value1),
			mmdvfs_qos_get_freq(value1));
		/* limit enable when opp1 */
		mmdvfs_qos_limit_config(value1, 1, MMDVFS_LIMIT_THERMAL);
		mtk_pm_qos_update_request(&disp_req, 0);
		pr_notice("limit enable when opp down: %d freq=%llu MHz\n",
			mmdvfs_get_limit_status(value1),
			mmdvfs_qos_get_freq(value1));
		/* limit disable when opp1 */
		mmdvfs_qos_limit_config(value1, 0, MMDVFS_LIMIT_THERMAL);
		mtk_pm_qos_update_request(&disp_req, 0);
		pr_notice("limit disable when opp down: %d freq=%llu MHz\n",
			mmdvfs_get_limit_status(value1),
			mmdvfs_qos_get_freq(value1));

		break;
	case 1:
		result = sscanf(val, "%d %d %d", &mmdvfs_ut_case,
			&value1, &value2);
		if (result != 3) {
			pr_notice("invalid arguments: %s\n", val);
			mmdvfs_qos_limit_config(value1, 0,
				MMDVFS_LIMIT_THERMAL);
			break;
		}
		pr_notice("limit test score: %d\n", value2);
		pr_notice("limit initial: %d freq=%llu MHz\n",
			mmdvfs_get_limit_status(value1),
			mmdvfs_qos_get_freq(value1));
		mmdvfs_qos_limit_config(value1, 1, MMDVFS_LIMIT_THERMAL);
		mmdvfs_qos_limit_config(value1, value2, MMDVFS_LIMIT_CAM);
		pr_notice("limit now: %d freq=%llu MHz\n",
			mmdvfs_get_limit_status(value1),
			mmdvfs_qos_get_freq(value1));

		break;
	default:
		pr_notice("invalid case_id: %d\n", mmdvfs_ut_case);
		break;
	}

	mtk_pm_qos_remove_request(&disp_req);

	pr_notice("%s END\n", __func__);
	log_level = old_log_level;
	return 0;
}

static struct kernel_param_ops mmdvfs_ut_ops = {
	.set = mmdvfs_ut_set,
	.get = param_get_int,
};
module_param_cb(mmdvfs_ut_case, &mmdvfs_ut_ops, &mmdvfs_ut_case, 0644);
MODULE_PARM_DESC(mmdvfs_ut_case, "force mmdvfs UT test case");

late_initcall(mmdvfs_late_init);
module_init(mmdvfs_init);
module_exit(mmdvfs_exit);

MODULE_DESCRIPTION("MTK MMDVFS driver");
MODULE_AUTHOR("Damon Chu<damon.chu@mediatek.com>");
MODULE_LICENSE("GPL");
#else
#include <linux/string.h>
#include <linux/math64.h>
#include "mmdvfs_pmqos.h"

int mmdvfs_qos_get_freq_steps(u32 pm_qos_class,
	u64 *out_freq_steps, u32 *out_step_size)
{
	return 0;
}

int mmdvfs_qos_force_step(int step)
{
	return 0;
}

void mmdvfs_qos_enable(bool enable)
{
}

void mmdvfs_autok_qos_enable(bool enable)
{
}

u64 mmdvfs_qos_get_freq(u32 pm_qos_class)
{
	return 0;
}

void mmdvfs_qos_limit_config(u32 pm_qos_class, u32 limit_value,
	enum mmdvfs_limit_source source)
{
}

void mmdvfs_prepare_action(enum mmdvfs_prepare_event event)
{
}
#endif
