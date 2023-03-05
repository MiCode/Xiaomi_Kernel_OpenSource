// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_mmdvfs.h>
#include "../../misc/mediatek/smi/mtk-smi-dbg.h"

#define MAX_OPP_NUM (6)
#define MAX_MUX_NUM (10)
#define MAX_HOPPING_CLK_NUM (2)

#if IS_ENABLED(CONFIG_MMPROFILE)
#include "../../misc/mediatek/mmp/mmprofile.h"

struct mmdvfs_mmp_events_t {
	mmp_event mmdvfs;
	mmp_event freq_change;
};
static struct mmdvfs_mmp_events_t mmdvfs_mmp_events;
#endif

enum {
	ACTION_DEFAULT,
	ACTION_IHDM, /* Voltage Increase: Hopping First, Decrease: MUX First*/
};

enum {
	AOV_MUX, AOV_LP_PARENT, AOV_PARENT, AOV_CLK_NUM
};

struct mmdvfs_mux_data {
	const char *mux_name;
	struct clk *mux;
	struct clk *clk_src[MAX_OPP_NUM];
	struct clk *clk_src_lp;
	struct clk *clk_src_normal;
};

struct mmdvfs_hopping_data {
	const char *hopping_name;
	struct clk *hopping_clk;
	u32 hopping_rate[MAX_OPP_NUM];
};

struct mmdvfs_drv_data {
	bool need_change_voltage;
	u32 request_voltage;
	u32 num_muxes;
	struct mmdvfs_mux_data muxes[MAX_MUX_NUM];
	u32 num_hoppings;
	struct mmdvfs_hopping_data hoppings[MAX_HOPPING_CLK_NUM];
	u32 action;
	struct notifier_block nb;
	u32 voltages[MAX_OPP_NUM];
	u32 num_voltages;
	struct mutex lp_mutex;
	bool lp_mode;
	u32 last_opp_level;
};

static struct regulator *vcore_reg_id;

#define MMDVFS_RECORD_NUM (10)
struct mmdvfs_opp_record {
	struct notifier_block nb;
	u8 idx;
	ktime_t time[MMDVFS_RECORD_NUM];
	u8 opp_level[MMDVFS_RECORD_NUM];
};

static struct mmdvfs_opp_record *mmdvfs_dbg;

struct mmdvfs_dbg_data {
	struct mmdvfs_drv_data *drv_data;
	struct regulator *reg;
	s32 force_step;
	s32 vote_step;
	bool is_notifier_registered;
	bool is_aov_enable;
	bool is_aov_mux_enable;
	struct clk *aov_clk[AOV_CLK_NUM];
};

struct mmdvfs_dbg_data *dbg_data;

static u32 log_level;
enum mmdvfs_log_level {
	log_freq = 0,
};

static u32 trace_ena;
enum mmdvfs_trace_ena {
	trace_systrace_ena = 0,
};


static BLOCKING_NOTIFIER_HEAD(mmdvfs_notifier_list);

static record_opp record_opp_fp;

void mmdvfs_debug_record_opp_set_fp(record_opp fp)
{
	record_opp_fp = fp;
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_record_opp_set_fp);

/**
 * register_mmdvfs_notifier - register multimedia clk changing notifier
 * @nb: notifier block
 *
 * Register notifier block to receive clk changing  notification.
 */
int register_mmdvfs_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mmdvfs_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(register_mmdvfs_notifier);

/**
 * unregister_mmdvfs_notifier - unregister multimedia clk changing notifier
 * @nb: notifier block
 *
 * Unregister clk changing notifier block.
 */
int unregister_mmdvfs_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mmdvfs_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_mmdvfs_notifier);

static void set_all_muxes(struct mmdvfs_drv_data *drv_data, u32 opp_level)
{
	u32 num_muxes = drv_data->num_muxes;
	u32 i;
	struct clk *mux, *clk_src;
	s32 err;

	MMDVFS_SYSTRACE_BEGIN("%s opp:%d\n", __func__, opp_level);
	for (i = 0; i < num_muxes; i++) {
		mux = drv_data->muxes[i].mux;
		clk_src = drv_data->muxes[i].clk_src[opp_level];
		err = clk_prepare_enable(mux);
		if (err) {
			pr_notice("prepare mux(%s) fail:%d opp_level:%d\n",
				  drv_data->muxes[i].mux_name, err, opp_level);
			continue;
		}
		err = clk_set_parent(mux, clk_src);
		if (err)
			pr_notice("set parent(%s) fail:%d opp_level:%d\n",
				  drv_data->muxes[i].mux_name, err, opp_level);
		clk_disable_unprepare(mux);
	}
	MMDVFS_SYSTRACE_END();
}

static void set_all_hoppings(struct mmdvfs_drv_data *drv_data, u32 opp_level)
{
	u32 num_hoppings = drv_data->num_hoppings;
	u32 i, hopping_rate;
	struct clk *hopping;
	s32 err;

	MMDVFS_SYSTRACE_BEGIN("%s opp:%d\n", __func__, opp_level);
	for (i = 0; i < num_hoppings; i++) {
		hopping = drv_data->hoppings[i].hopping_clk;
		hopping_rate = drv_data->hoppings[i].hopping_rate[opp_level];
		err = clk_prepare_enable(hopping);
		if (err) {
			pr_notice("prepare hopping(%s) fail:%d opp_level:%d\n",
				  drv_data->hoppings[i].hopping_name,
				  err, opp_level);
			continue;
		}
		err = clk_set_rate(hopping, hopping_rate);
		if (err)
			pr_notice("set %s rate(%u) fail:%d opp_level:%d\n",
				  drv_data->hoppings[i].hopping_name,
				  hopping_rate, err, opp_level);
		clk_disable_unprepare(hopping);
	}
	MMDVFS_SYSTRACE_END();
}

static int mmdvfs_dbg_log_cb(struct notifier_block *nb,
		unsigned long value, void *v)
{
	int i;

	pr_notice("[smi] mmdvfs dump opp level start\n");
	for (i = mmdvfs_dbg->idx; i < MMDVFS_RECORD_NUM; i++) {
		pr_notice("[smi] (time, opp_level) = (%18llu, %d))\n",
				mmdvfs_dbg->time[i], mmdvfs_dbg->opp_level[i]);
	}
	for (i = 0; i < mmdvfs_dbg->idx; i++) {
		pr_notice("[smi] (time, opp_level) = (%18llu, %d))\n",
				mmdvfs_dbg->time[i], mmdvfs_dbg->opp_level[i]);
	}
	pr_notice("[smi] mmdvfs opp level end\n");

	return 0;
}

static void set_all_clk(struct mmdvfs_drv_data *drv_data,
			u32 voltage, bool vol_inc)
{
	s32 i;
	u32 opp_level;
	MMDVFS_SYSTRACE_BEGIN("%s voltage:%u\n", __func__, voltage);
	for (i = drv_data->num_voltages - 1; i >= 0; i--) {
		if (voltage >= drv_data->voltages[i]) {
			opp_level = i;
			break;
		}
	}
	if (i < 0)
		opp_level = 0;

	mutex_lock(&drv_data->lp_mutex);
	do {
		if (opp_level == drv_data->last_opp_level)
			break;

		if (dbg_data->is_aov_enable && dbg_data->aov_clk[AOV_MUX]) {
			if (opp_level == 0 && drv_data->last_opp_level != MAX_OPP_NUM) {
				if (dbg_data->is_aov_mux_enable) {
					pr_notice("mmdvfs disable AOV MUX\n");
					clk_disable_unprepare(dbg_data->aov_clk[AOV_MUX]);
					dbg_data->is_aov_mux_enable = false;
				}
			} else if (drv_data->last_opp_level == 0 ||
				(drv_data->last_opp_level == MAX_OPP_NUM && opp_level != 0)) {
				pr_notice("mmdvfs enable AOV MUX\n");
				clk_prepare_enable(dbg_data->aov_clk[AOV_MUX]);
				dbg_data->is_aov_mux_enable = true;
			}
		}

		switch (drv_data->action) {
		/* Voltage Increase: Hopping First, Decrease: MUX First*/
		case ACTION_IHDM:
			if (vol_inc) {
				set_all_hoppings(drv_data, opp_level);
				set_all_muxes(drv_data, opp_level);
			} else {
				set_all_muxes(drv_data, opp_level);
				set_all_hoppings(drv_data, opp_level);
			}
			break;
		default:
			set_all_muxes(drv_data, opp_level);
			break;
		}
		drv_data->last_opp_level = opp_level;
	} while (0);
	mutex_unlock(&drv_data->lp_mutex);
	blocking_notifier_call_chain(&mmdvfs_notifier_list, opp_level, NULL);
	if (log_level & 1 << log_freq)
		pr_notice("set clk to opp level:%d\n", opp_level);

	/* Record mmdvfs opp log*/
	mmdvfs_dbg->time[mmdvfs_dbg->idx] = ktime_get();
	mmdvfs_dbg->opp_level[mmdvfs_dbg->idx] = opp_level;
	mmdvfs_dbg->idx = (mmdvfs_dbg->idx + 1) % MMDVFS_RECORD_NUM;
	record_opp_fp(opp_level);
	MMDVFS_SYSTRACE_END();
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(
		mmdvfs_mmp_events.freq_change,
		MMPROFILE_FLAG_PULSE, vol_inc, opp_level);
#endif
}

static int regulator_event_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	unsigned long uV;
	struct mmdvfs_drv_data *drv_data;
	struct pre_voltage_change_data *pvc_data;

	drv_data = container_of(nb, struct mmdvfs_drv_data, nb);

	MMDVFS_SYSTRACE_BEGIN("%s event:0x%lx\n", __func__, event);
	if (event == REGULATOR_EVENT_PRE_VOLTAGE_CHANGE) {
		pvc_data = data;
		uV = pvc_data->min_uV;
		MMDVFS_SYSTRACE_BEGIN("%s event=PRE_VOLTAGE_CHANGE old=%lu new=%lu\n",
			__func__, pvc_data->old_uV, pvc_data->min_uV);
		if (uV < pvc_data->old_uV) {
			set_all_clk(drv_data, uV, false);
			drv_data->request_voltage = uV;
		} else if (uV > pvc_data->old_uV || unlikely(drv_data->request_voltage == 0)) {
			drv_data->need_change_voltage = true;
		}
		if (log_level & 1 << log_freq)
			pr_notice("regulator event=PRE_VOLTAGE_CHANGE old=%lu new=%lu\n",
				pvc_data->old_uV, pvc_data->min_uV);
		MMDVFS_SYSTRACE_END();
	} else if (event == REGULATOR_EVENT_VOLTAGE_CHANGE) {
		uV = (unsigned long)data;
		MMDVFS_SYSTRACE_BEGIN("%s event=VOLTAGE_CHANGE voltage=%lu\n",
			__func__, uV);
		if (drv_data->need_change_voltage) {
			set_all_clk(drv_data, uV, true);
			drv_data->need_change_voltage = false;
			drv_data->request_voltage = uV;
		}
		if (log_level & 1 << log_freq)
			pr_notice("regulator event=VOLTAGE_CHANGE voltage=%lu\n", uV);
		MMDVFS_SYSTRACE_END();
	} else if (event == REGULATOR_EVENT_ABORT_VOLTAGE_CHANGE) {
		uV = (unsigned long)data;
		MMDVFS_SYSTRACE_BEGIN("%s event=ABORT_VOLTAGE_CHANGE voltage=%lu\n",
			__func__, uV);
		/* If clk was changed, restore to previous setting */
		if (uV != drv_data->request_voltage) {
			set_all_clk(drv_data, uV,
				    uV > drv_data->request_voltage);
			drv_data->need_change_voltage = false;
			drv_data->request_voltage = uV;
		}
		pr_info("regulator event=ABORT_VOLTAGE_CHANGE voltage=%lu\n",
			uV);
		MMDVFS_SYSTRACE_END();
	}
	MMDVFS_SYSTRACE_END();
	return 0;
}

static const struct of_device_id of_mmdvfs_match_tbl[] = {
	{
		.compatible = "mediatek,mmdvfs",
	},
	{}
};

/**
 * unregister_mmdvfs_notifier - unregister multimedia clk changing notifier
 * @nb: notifier block
 *
 * Unregister clk changing notifier block.
 */
void mmdvfs_set_lp_mode(bool lp_mode)
{
	struct mmdvfs_drv_data *drv_data = dbg_data->drv_data;
	struct mmdvfs_mux_data *mux;
	int i;

	mutex_lock(&drv_data->lp_mutex);
	do {
		if (lp_mode == drv_data->lp_mode)
			break;

		drv_data->lp_mode = lp_mode;
		for (i = 0; i < drv_data->num_muxes; i++) {
			mux = &drv_data->muxes[i];
			if (!mux->clk_src_lp)
				continue;

			mux->clk_src[0] = lp_mode ? mux->clk_src_lp
				: mux->clk_src_normal;
		}
		if (drv_data->last_opp_level == 0)
			set_all_muxes(drv_data, 0);
	} while (0);
	mutex_unlock(&drv_data->lp_mutex);
}
EXPORT_SYMBOL_GPL(mmdvfs_set_lp_mode);

static int set_lp_mode(const char *val, const struct kernel_param *kp)
{
	int result;
	int new_lp_mode;

	result = kstrtoint(val, 0, &new_lp_mode);
	if (result) {
		pr_notice("mmdvfs lp mode failed: %d\n", result);
		return result;
	}
	mmdvfs_set_lp_mode(new_lp_mode);
	return 0;
}

static struct kernel_param_ops lp_mode_ops = {
	.set = set_lp_mode,
};
module_param_cb(lp_mode, &lp_mode_ops, NULL, 0644);
MODULE_PARM_DESC(lp_mode, "set low power mode");

int mmdvfs_dbg_clk_set(int step, bool is_force)
{
	struct mmdvfs_drv_data *drv_data;
	s32 ret = 0;
	u32 v_real = 0;
	int volt = 0;
	s32 last_force_step;

	if (!dbg_data) {
		pr_notice("%s: dbg_data is not ready!\n", __func__);
		return -EINVAL;
	}
	drv_data = dbg_data->drv_data;

	if (!drv_data) {
		pr_notice("%s: drv_data is not ready!\n", __func__);
		return -EINVAL;
	}

	if (step >= (int)drv_data->num_voltages) {
		pr_notice("%s: invalid force_step(%d)\n", __func__, step);
		return -EINVAL;
	}

	if (is_force) {
		last_force_step = dbg_data->force_step;
		dbg_data->force_step = step;
	} else {
		dbg_data->vote_step = step;
	}

	if (step < 0) {
		if (is_force && !dbg_data->is_notifier_registered) {
			ret = devm_regulator_register_notifier(
					dbg_data->reg, &drv_data->nb);
			dbg_data->is_notifier_registered = true;
			if (ret)
				pr_notice("%s: failed to register notifier(%d)\n",
					__func__, ret);
		}

		regulator_set_voltage(dbg_data->reg, 0, INT_MAX);
	} else {
		volt = drv_data->voltages[drv_data->num_voltages-1-step];
		if (is_force) {
			if (dbg_data->is_notifier_registered) {
				devm_regulator_unregister_notifier(
						dbg_data->reg, &drv_data->nb);
				dbg_data->is_notifier_registered = false;
			}
			if ((last_force_step < 0 && volt > drv_data->request_voltage)
				|| (last_force_step >= 0 && step < last_force_step)) {
				regulator_set_voltage(
					dbg_data->reg, volt, INT_MAX);
				if (!IS_ERR(vcore_reg_id)) {
					v_real = regulator_get_voltage(vcore_reg_id);
					pr_notice("%s: step=%d volt=%d r_volt=%d is_force=%d\n",
							__func__, step, volt, v_real, is_force);
				}
				set_all_clk(drv_data, volt, true);
			} else {
				set_all_clk(drv_data, volt, false);
				regulator_set_voltage(
					dbg_data->reg, volt, INT_MAX);
				if (!IS_ERR(vcore_reg_id)) {
					v_real = regulator_get_voltage(vcore_reg_id);
					pr_notice("%s: step=%d volt=%d r_volt=%d is_force=%d\n",
							__func__, step, volt, v_real, is_force);
				}
			}
		} else {
			regulator_set_voltage(dbg_data->reg, volt, INT_MAX);
		}
	}
	pr_notice("%s: step=%d volt=%d is_force=%d\n", __func__, step, volt, is_force);
	return ret;
}

int mmdvfs_set_force_step(int force_step)
{
	return mmdvfs_dbg_clk_set(force_step, true);
}
EXPORT_SYMBOL_GPL(mmdvfs_set_force_step);

static int set_force_step(const char *val, const struct kernel_param *kp)
{
	int result;
	int new_force_step;

	result = kstrtoint(val, 0, &new_force_step);
	if (result) {
		pr_notice("mmdvfs set force step failed: %d\n", result);
		return result;
	}
	return mmdvfs_set_force_step(new_force_step);
}

static struct kernel_param_ops set_force_step_ops = {
	.set = set_force_step,
};
module_param_cb(force_step, &set_force_step_ops, NULL, 0644);
MODULE_PARM_DESC(force_step, "force mmdvfs to specified step");

int mmdvfs_set_vote_step(int vote_step)
{
	return mmdvfs_dbg_clk_set(vote_step, false);
}
EXPORT_SYMBOL_GPL(mmdvfs_set_vote_step);

static int set_vote_step(const char *val, const struct kernel_param *kp)
{
	int result;
	int vote_step;

	result = kstrtoint(val, 0, &vote_step);
	if (result) {
		pr_notice("mmdvfs set vote step failed: %d\n", result);
		return result;
	}

	return mmdvfs_set_vote_step(vote_step);
}

static struct kernel_param_ops set_vote_step_ops = {
	.set = set_vote_step,
};
module_param_cb(vote_step, &set_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmdvfs log level");

#define MAX_DUMP (PAGE_SIZE - 1)
struct mmdvfs_drv_data *drv_data;
int dump_setting(char *buf, const struct kernel_param *kp)
{
	u32 i, j;
	int length = 0;

	length += snprintf(buf + length, MAX_DUMP  - length,
		"mux number:%d\n", drv_data->num_muxes);
	length += snprintf(buf + length, MAX_DUMP  - length,
		"mux:");
	for (i = 0; i < drv_data->num_muxes; i++) {
		length += snprintf(buf + length, MAX_DUMP  - length,
		"%s ", drv_data->muxes[i].mux_name);
	}
	length += snprintf(buf + length, MAX_DUMP  - length,
		"\n");
	length += snprintf(buf + length, MAX_DUMP  - length,
		"hopping number:%d\n", drv_data->num_hoppings);
	for (i = 0; i < drv_data->num_hoppings; i++) {
		length += snprintf(buf + length, MAX_DUMP  - length,
		"%s: ", drv_data->hoppings[i].hopping_name);
		for (j = 0; j < MAX_OPP_NUM; j++) {
			if (!drv_data->hoppings[i].hopping_rate[j])
				break;
			length += snprintf(buf + length, MAX_DUMP  - length,
				"%d ", drv_data->hoppings[i].hopping_rate[j]);
		}
		length += snprintf(buf + length, MAX_DUMP  - length,
		"\n");
	}
	length += snprintf(buf + length, MAX_DUMP  - length,
		"action: %d\n", drv_data->action);
	length += snprintf(buf + length, MAX_DUMP  - length,
		"voltage level:");
	for (i = 0; i < MAX_OPP_NUM; i++) {
		if (!drv_data->voltages[i])
			break;
		length += snprintf(buf + length, MAX_DUMP  - length,
		"%d ", drv_data->voltages[i]);
	}
	length += snprintf(buf + length, MAX_DUMP  - length,
		"\n");
	length += snprintf(buf + length, MAX_DUMP  - length,
		"request voltage:%d\n", drv_data->request_voltage);
#ifdef MMDVFS_DBG
	length += snprintf(buf + length, MAX_DUMP  - length,
		"force_step:%d\n", dbg_data->force_step);
	length += snprintf(buf + length, MAX_DUMP  - length,
		"vote_step:%d\n", dbg_data->vote_step);
#endif
	if (length >= MAX_DUMP)
		length = MAX_DUMP - 1;

	return length;
}

static struct kernel_param_ops dump_param_ops = {.get = dump_setting};
module_param_cb(dump_setting, &dump_param_ops, NULL, 0444);
MODULE_PARM_DESC(dump_setting, "dump mmdvfs current setting");

static int mmdvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regulator *reg;
	u32 num_mux = 0, num_hopping = 0;
	u32 num_clksrc, hopping_rate, num_hopping_rate;
	u32 num_lp_clksrc = 0;
	struct property *mux_prop, *clksrc_prop;
	struct property *hopping_prop, *hopping_rate_prop;
	struct property *aov_prop;
	struct property *lp_mux_prop;
	const char *mux_name, *clksrc_name, *hopping_name;
	char prop_name[32];
	const char *aov_prop_name;
	const char *lp_clksrc_name;
	const __be32 *p;
	s32 ret, i = 0;
	unsigned long freq;
	struct dev_pm_opp *opp;
	struct clk *clk;

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_enable(1);
	if (mmdvfs_mmp_events.mmdvfs == 0) {
		mmdvfs_mmp_events.mmdvfs =
			mmprofile_register_event(MMP_ROOT_EVENT, "MMDVFS");
		mmdvfs_mmp_events.freq_change =	mmprofile_register_event(
			mmdvfs_mmp_events.mmdvfs, "freq_change");
		mmprofile_enable_event_recursive(mmdvfs_mmp_events.mmdvfs, 1);
	}
	mmprofile_start(1);
#endif

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;
	of_property_for_each_string(dev->of_node, "mediatek,support-mux",
				    mux_prop, mux_name) {
		if (num_mux >= MAX_MUX_NUM) {
			pr_notice("Too many items in support_mux\n");
			return -EINVAL;
		}
		drv_data->muxes[num_mux].mux = devm_clk_get(dev, mux_name);
		drv_data->muxes[num_mux].mux_name = mux_name;
		snprintf(prop_name, sizeof(prop_name) - 1,
			 "mediatek,mux-%s", mux_name);
		num_clksrc = 0;
		of_property_for_each_string(dev->of_node, prop_name,
					    clksrc_prop, clksrc_name) {
			if (num_clksrc >= MAX_OPP_NUM) {
				pr_notice("Too many items in %s\n", prop_name);
				return -EINVAL;
			}
			drv_data->muxes[num_mux].clk_src[num_clksrc] =
				devm_clk_get(dev, clksrc_name);
			num_clksrc++;
		}
		num_mux++;
	}

	drv_data->num_muxes = num_mux;
	of_property_for_each_string(dev->of_node, "mediatek,support-hopping",
				    hopping_prop, hopping_name) {
		if (num_hopping >= MAX_HOPPING_CLK_NUM) {
			pr_notice("Too many items in support_hopping\n");
			return -EINVAL;
		}
		drv_data->hoppings[num_hopping].hopping_clk =
					devm_clk_get(dev, hopping_name);
		drv_data->hoppings[num_hopping].hopping_name = hopping_name;
		snprintf(prop_name, sizeof(prop_name) - 1,
			 "mediatek,hopping-%s", hopping_name);
		num_hopping_rate = 0;
		of_property_for_each_u32(dev->of_node, prop_name,
					 hopping_rate_prop, p, hopping_rate) {
			if (num_hopping_rate >= MAX_OPP_NUM) {
				pr_notice("Too many items in %s\n", prop_name);
				return -EINVAL;
			}
			drv_data->hoppings[num_hopping].hopping_rate
					[num_hopping_rate] = hopping_rate;
			num_hopping_rate++;
		}
		num_hopping++;
	}
	drv_data->num_hoppings = num_hopping;
	of_property_read_u32(dev->of_node,
			     "mediatek,action", &drv_data->action);
	/* Get voltage info from opp table */
	dev_pm_opp_of_add_table(dev);
	freq = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		drv_data->voltages[drv_data->num_voltages] =
			dev_pm_opp_get_voltage(opp);
		freq++;
		drv_data->num_voltages++;
		dev_pm_opp_put(opp);
	}
	reg = devm_regulator_get(dev, "dvfsrc-vcore");
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	dbg_data = devm_kzalloc(dev, sizeof(*dbg_data), GFP_KERNEL);
	if (!dbg_data)
		return -ENOMEM;
	dbg_data->drv_data = drv_data;
	dbg_data->reg = reg;
	dbg_data->force_step = -1;
	dbg_data->vote_step = -1;
	dbg_data->is_notifier_registered = true;

	of_property_for_each_string(dev->of_node, "clock-names", aov_prop, aov_prop_name) {
		if (!strncmp("aov", aov_prop_name, 3)) {
			if (i == AOV_CLK_NUM) {
				dev_notice(dev, "%s: aov clk num is wrong\n", __func__);
				ret = -EINVAL;
				break;
			}
			clk = devm_clk_get(dev, aov_prop_name);
			if (IS_ERR(clk)) {
				dev_notice(dev, "%s: clks of %s init failed\n",
					__func__, aov_prop_name);
				ret = PTR_ERR(clk);
				break;
			}
			dbg_data->aov_clk[i++] = clk;
		}
	}

	drv_data->nb.notifier_call = regulator_event_notify;
	ret = devm_regulator_register_notifier(reg, &drv_data->nb);
	if (ret)
		pr_notice("Failed to register notifier: %d\n", ret);

	vcore_reg_id = regulator_get(dev, "_vcore");
	if (IS_ERR(vcore_reg_id)) {
		pr_info("regulator_get vcore_reg_id failed: %ld\n",
				PTR_ERR(vcore_reg_id));
	}

	of_property_for_each_string(dev->of_node, "mediatek,mux-lp",
				    lp_mux_prop, lp_clksrc_name) {
		if (num_lp_clksrc >= drv_data->num_muxes) {
			pr_notice("Too many items in mux-lp\n");
			return -EINVAL;
		}
		if (strlen(lp_clksrc_name)) {
			drv_data->muxes[num_lp_clksrc].clk_src_lp
				= devm_clk_get(dev, lp_clksrc_name);
			drv_data->muxes[num_lp_clksrc].clk_src_normal
				= drv_data->muxes[num_lp_clksrc].clk_src[0];
		}
		num_lp_clksrc++;
	}
	mutex_init(&drv_data->lp_mutex);
	drv_data->last_opp_level = MAX_OPP_NUM;
	mmdvfs_dbg = kzalloc(sizeof(*mmdvfs_dbg), GFP_KERNEL);
	if (!mmdvfs_dbg)
		return -ENOMEM;

	mmdvfs_dbg->nb.notifier_call = mmdvfs_dbg_log_cb;
	mtk_smi_dbg_register_notifier(&mmdvfs_dbg->nb);

	return ret;
}

void mtk_mmdvfs_aov_enable(const bool enable)
{
	if (!dbg_data)
		return;
	dbg_data->is_aov_enable = enable;
	if (!enable && dbg_data->aov_clk[AOV_MUX] && dbg_data->is_aov_mux_enable) {
		pr_notice("%s disable AOV MUX\n", __func__);
		clk_disable_unprepare(dbg_data->aov_clk[AOV_MUX]);
		dbg_data->is_aov_mux_enable = false;
	}
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_aov_enable);

static int mmdvfs_set_aov_clk(bool is_sys_resume)
{
	int err = 0, i;
	struct clk *parent;

	if (!dbg_data)
		return -EINVAL;

	if (dbg_data->is_aov_enable) {
		for (i = 0; i < AOV_CLK_NUM; i++) {
			if (!dbg_data->aov_clk[i]) {
				pr_notice("%s aov_clk[%d] is NULL\n", __func__, i);
				return -EINVAL;
			}
		}
		err = clk_prepare_enable(dbg_data->aov_clk[AOV_MUX]);
		if (err) {
			pr_notice("%s prepare aov_mux fail:%d\n", __func__, err);
			return -EINVAL;
		}
		parent = (is_sys_resume) ? dbg_data->aov_clk[AOV_PARENT] :
				dbg_data->aov_clk[AOV_LP_PARENT];
		err = clk_set_parent(dbg_data->aov_clk[AOV_MUX], parent);
		if (err)
			pr_notice("%s set parent fail:%d is_sys_resume:%d\n",
				__func__, err, is_sys_resume);
		clk_disable_unprepare(dbg_data->aov_clk[AOV_MUX]);
		pr_notice("%s: is_sys_resume=%d\n", __func__, is_sys_resume);
	}

	return err;
}

static int mmdvfs_pm_suspend(struct device *dev)
{
	mmdvfs_set_aov_clk(false);
	return 0;
}

static int mmdvfs_pm_resume(struct device *dev)
{
	mmdvfs_set_aov_clk(true);
	return 0;
}

bool mmdvfs_systrace_enabled(void)
{
	return trace_ena & (1 << trace_systrace_ena);
}

noinline int tracing_mark_write(char *fmt, ...)
{
#if IS_ENABLED(CONFIG_MTK_FTRACER)
	char buf[TRACE_MSG_LEN];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (len >= TRACE_MSG_LEN) {
		pr_notice("%s trace size %u exceed limit\n", __func__, len);
		return -1;
	}
	trace_puts(buf);
#endif
	return 0;
}

module_param(trace_ena, uint, 0644);
MODULE_PARM_DESC(trace_ena, "mmdvfs trace enable");

static const struct dev_pm_ops mmdvfs_pm_ops = {
	.suspend = mmdvfs_pm_suspend,
	.resume  = mmdvfs_pm_resume,
};

static struct platform_driver mmdvfs_drv = {
	.probe = mmdvfs_probe,
	.driver = {
		.name = "mtk-mmdvfs",
		.of_match_table = of_mmdvfs_match_tbl,
		.pm = &mmdvfs_pm_ops,
	},
};

static int __init mtk_mmdvfs_init(void)
{
	s32 status;

	status = platform_driver_register(&mmdvfs_drv);
	if (status) {
		pr_notice("Failed to register MMDVFS driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mmdvfs_exit(void)
{
	platform_driver_unregister(&mmdvfs_drv);
}

module_init(mtk_mmdvfs_init);
module_exit(mtk_mmdvfs_exit);
MODULE_DESCRIPTION("MTK MMDVFS driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
