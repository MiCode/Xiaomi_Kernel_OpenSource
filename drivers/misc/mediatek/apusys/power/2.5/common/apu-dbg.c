// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/devfreq.h>
#include <linux/of_address.h>

#include "apu_io.h"
#include "apu_dbg.h"
#include "apu_log.h"
#include "apu_common.h"
#include "apu_dbg.h"
#include "apusys_power.h"
#include "apu_rpc.h"
#include "apusys_core.h"
#include "apu_regulator.h"
#include "apu_clk.h"

DECLARE_DELAYED_WORK(pw_info_work, apupw_dbg_power_info);
static char buffer[__LOG_BUF_LEN];
static struct apu_dbg apupw_dbg = {
	.log_lvl = NO_LVL,
};

static inline void _apupw_separte(struct seq_file *s, char *separate)
{
	seq_puts(s, "\n");
	seq_printf(s, separate);
	seq_puts(s, "\n");
}

/**
 * _dbg_id_to_dvfsuser() - transfer wiki's id to dvfs_user
 * @dbg_id: the device id defined on wiki.
 *
 * Based on wiki's document, transfer real dvfs_user.
 */
static const enum DVFS_USER _apupw_dbg_id2user(int dbg_id)
{
	static const int ids[] = {
		[0] = VPU0,
		[1] = VPU1,
		[2] = VPU2,
		[3] = MDLA0,
		[4] = MDLA1,
	};

	if (dbg_id < 0 || dbg_id >= ARRAY_SIZE(ids)) {
		pr_info("Not support device id: %d\n", dbg_id);
		return -EINVAL;
	}
	return ids[dbg_id];
}

static struct apu_dev *_apupw_valid_df(enum DVFS_USER usr)
{
	struct apu_dev *ad = apu_find_device(usr);

	if (IS_ERR_OR_NULL(ad) || IS_ERR_OR_NULL(ad->df))
		return NULL;

	return ad;
}

static struct apu_dev *_apupw_valid_leaf_df(enum DVFS_USER usr)
{
	struct apu_dev *ad = apu_find_device(usr);
	struct apu_gov_data *gov_data;

	if (IS_ERR_OR_NULL(ad) || IS_ERR_OR_NULL(ad->df))
		return NULL;

	gov_data = (struct apu_gov_data *)ad->df->data;
	if (gov_data->depth)
		return NULL;

	return ad;
}

static struct apu_dev *_apupw_valid_parent_df(enum DVFS_USER usr)
{
	struct apu_dev *ad = apu_find_device(usr);
	struct apu_gov_data *gov_data;

	if (IS_ERR_OR_NULL(ad) || IS_ERR_OR_NULL(ad->df))
		return NULL;

	gov_data = (struct apu_gov_data *)ad->df->data;
	if (!gov_data->depth)
		return NULL;

	return ad;
}

static bool _apupw_valid_input(u8 param, int argc)
{
	int expect = 0;

	switch (param) {
	case POWER_PARAM_FIX_OPP:
	case POWER_PARAM_DVFS_DEBUG:
	case POWER_PARAM_LOG_LEVEL:
	case POWER_PARAM_CURR_STATUS:
	case POWER_PARAM_OPP_TABLE:
		expect = 1;
		if (argc != expect)
			goto INVALID;
		break;
	case POWER_HAL_CTL:
	case POWER_PARAM_SET_POWER_HAL_OPP:
	case POWER_PARAM_POWER_STRESS:
		expect = 3;
		if (argc != expect)
			goto INVALID;
		break;
	}
	return true;
INVALID:
	pr_info("para:%d expected:%d, received:%d\n", param, expect, argc);
	return false;
}

static int _apupw_set_freq_range(struct apu_dev *ad, ulong min, ulong max)
{
	int ret = 0;

	/*
	 * Protect against theoretical sysfs writes between
	 * device_add and dev_pm_qos_add_request
	 */
	if (!dev_pm_qos_request_active(&ad->df->user_max_freq_req))
		return -EINVAL;
	if (!dev_pm_qos_request_active(&ad->df->user_min_freq_req))
		return -EAGAIN;

	/* Change qos min freq to fix freq */
	ret = dev_pm_qos_update_request(&ad->df->user_min_freq_req, min);
	if (ret < 0)
		return ret;

	/* Change qos max freq to fix freq and input min/mas are Khz already */
	ret = dev_pm_qos_update_request(&ad->df->user_max_freq_req, max);
	pr_info("[%s] [%s] max/min %luMhz/%luMhz, ret = %d\n",
		apu_dev_name(ad->dev), __func__, TOKHZ(max), TOKHZ(min), ret);

	return ret;
}

static int _apupw_default_freq_range(struct apu_dev *ad)
{
	int ret = 0;

	/*
	 * Protect against theoretical sysfs writes between
	 * device_add and dev_pm_qos_add_request
	 */
	if (!dev_pm_qos_request_active(&ad->df->user_max_freq_req))
		return -EINVAL;
	if (!dev_pm_qos_request_active(&ad->df->user_min_freq_req))
		return -EAGAIN;

	/* Change qos min freq to fix freq */
	ret = dev_pm_qos_update_request(&ad->df->user_min_freq_req,
					TOKHZ(ad->df->scaling_min_freq));
	if (ret < 0)
		return ret;

	/* Change qos max freq to fix freq */
	ret = dev_pm_qos_update_request(&ad->df->user_max_freq_req,
					TOKHZ(ad->df->scaling_max_freq));
	pr_info("[%s] [%s] restore default max/min %luMhz/%luMhz, ret = %d\n",
		apu_dev_name(ad->dev), __func__,
		TOMHZ(ad->df->scaling_max_freq),
		TOMHZ(ad->df->scaling_min_freq), ret);

	return ret;
}

enum LOG_LEVEL apupw_dbg_get_loglvl(void)
{
	return apupw_dbg.log_lvl;
}

void apupw_dbg_set_loglvl(enum LOG_LEVEL lvl)
{
	apupw_dbg.log_lvl = lvl;
}

int apupw_dbg_get_fixopp(void)
{
	return apupw_dbg.fix_opp;
}

void apupw_dbg_set_fixopp(int fix)
{
	apupw_dbg.fix_opp = fix;
}

void apupw_dbg_power_info(struct work_struct *work)
{
	struct apu_dbg_clk *dbg_clk;
	struct apu_dbg_regulator *dbg_reg;
	struct apu_dbg_cg *dbg_cg;
	int n_pos = 0;

	memset(buffer, 0, sizeof(buffer));
	n_pos = snprintf(buffer, LOG_LEN, "APUPWR v[");
	if (n_pos <= 0)
		goto out;

	list_for_each_entry_reverse(dbg_reg, &apupw_dbg.reg_list, node)
		n_pos += snprintf((buffer + n_pos), (LOG_LEN - n_pos),
				"%u,", TOMV(regulator_get_voltage(dbg_reg->reg)));

	n_pos += snprintf((buffer + n_pos), (sizeof(buffer) - n_pos - 1), "]f[");

	/* search the list of dbg_clk_list for this clk */
	list_for_each_entry_reverse(dbg_clk, &apupw_dbg.clk_list, node)
		n_pos += snprintf((buffer + n_pos), (LOG_LEN - n_pos),
				"%lu,", TOMHZ(clk_get_rate(dbg_clk->clk)));

	n_pos += snprintf((buffer + n_pos), (LOG_LEN - n_pos),
			"]r[%lx,%lx,", apu_spm_wakeup_value(), apu_rpc_rdy_value());

	list_for_each_entry_reverse(dbg_cg, &apupw_dbg.cg_list, node)
		n_pos += snprintf((buffer + n_pos), (LOG_LEN - n_pos),
				"0x%x,", apu_readl(dbg_cg->reg));

	n_pos += snprintf((buffer + n_pos), (LOG_LEN - n_pos), "]");

	pr_info("%s", buffer);
out:
	if (apupw_dbg.poll_interval)
		queue_delayed_work(pm_wq, &pw_info_work,
			msecs_to_jiffies(apupw_dbg.poll_interval));
}

static int apupw_dbg_register_cg(const char *name, void __iomem *reg)
{
	struct apu_dbg_cg *dbg_cg = NULL;
	int ret = -ENOMEM;

	if (IS_ERR_OR_NULL(reg) || !name)
		return -EINVAL;

	if (list_empty(&apupw_dbg.cg_list))
		goto allocate;

	/* search the list of dbg_clk_list for this clk */
	list_for_each_entry(dbg_cg, &apupw_dbg.cg_list, node)
		if (!dbg_cg || dbg_cg->reg == reg)
			break;
allocate:

	/* if clk wasn't in the dbg_clk_list, allocate new clk_notifier */
	if (!dbg_cg || dbg_cg->reg != reg) {
		dbg_cg = kzalloc(sizeof(*dbg_cg), GFP_KERNEL);
		if (!dbg_cg)
			goto out;
		dbg_cg->reg = reg;
		dbg_cg->name = name;
		list_add(&dbg_cg->node, &apupw_dbg.cg_list);
		ret = 0;
	}
out:
	return ret;
}

static void apupw_dbg_unregister_cg(void)
{
	struct apu_dbg_cg *dbg_cg = NULL, *tmp = NULL;

	if (list_empty(&apupw_dbg.cg_list))
		return;

	list_for_each_entry_safe(dbg_cg, tmp, &apupw_dbg.cg_list, node) {
		iounmap(dbg_cg->reg);
		list_del(&dbg_cg->node);
		kfree(dbg_cg);
	}
}

static int apupw_dbg_register_clk(const char *name, struct clk *clk)
{
	struct apu_dbg_clk *dbg_clk = NULL;
	int ret = -ENOMEM;

	if (IS_ERR_OR_NULL(clk) || !name)
		return -EINVAL;

	if (list_empty(&apupw_dbg.clk_list))
		goto allocate;

	/* search the list of dbg_clk_list for this clk */
	list_for_each_entry(dbg_clk, &apupw_dbg.clk_list, node)
		if (dbg_clk->clk == clk) {
			ret = 0;
			goto out;
		}

allocate:
	/* if clk wasn't in the dbg_clk_list, allocate new dbg_clk */
	dbg_clk = kzalloc(sizeof(*dbg_clk), GFP_KERNEL);
	if (!dbg_clk)
		goto out;
	dbg_clk->clk = clk;
	dbg_clk->name = name;
	list_add(&dbg_clk->node, &apupw_dbg.clk_list);
	ret = 0;
out:
	return ret;
}

static void apupw_dbg_unregister_clk(void)
{
	struct apu_dbg_clk *dbg_clk = NULL, *tmp = NULL;

	if (list_empty(&apupw_dbg.clk_list))
		return;

	list_for_each_entry_safe(dbg_clk, tmp, &apupw_dbg.clk_list, node) {
		clk_put(dbg_clk->clk);
		list_del(&dbg_clk->node);
		kfree(dbg_clk);
	}
}

static int apupw_dbg_register_regulator(const char *name, struct regulator *reg)
{
	struct apu_dbg_regulator *dbg_reg = NULL;
	int ret = -ENOMEM;

	if (IS_ERR_OR_NULL(reg) || !name)
		return -EINVAL;

	if (list_empty(&apupw_dbg.reg_list))
		goto allocate;
	/* search the list of dbg_clk_list for this clk */
	list_for_each_entry(dbg_reg, &apupw_dbg.reg_list, node)
		if (!dbg_reg || dbg_reg->reg == reg)
			break;
allocate:

	/* if clk wasn't in the dbg_clk_list, allocate new clk_notifier */
	if (!dbg_reg || dbg_reg->reg != reg) {
		dbg_reg = kzalloc(sizeof(*dbg_reg), GFP_KERNEL);
		if (!dbg_reg)
			goto out;
		dbg_reg->reg = reg;
		dbg_reg->name = name;
		list_add(&dbg_reg->node, &apupw_dbg.reg_list);
		ret = 0;
	}
out:
	return ret;
}

static void apupw_dbg_unregister_regulator(void)
{
	struct apu_dbg_regulator *dbg_reg = NULL, *tmp = NULL;

	if (list_empty(&apupw_dbg.reg_list))
		return;

	list_for_each_entry_safe(dbg_reg, tmp, &apupw_dbg.reg_list, node) {
		regulator_put(dbg_reg->reg);
		list_del(&dbg_reg->node);
		kfree(dbg_reg);
	}
}

static int apupw_dbg_loglvl(u8 param, int argc, int *args)
{
	int ret = 0;

	switch (args[0]) {
	case NO_LVL:
		apupw_dbg.poll_interval = 0;
	case INFO_LVL:
	case VERBOSE_LVL:
		ret = cancel_delayed_work_sync(&pw_info_work);
		break;
	case PERIOD_LVL:
		apupw_dbg.poll_interval = 200;
		ret = queue_delayed_work(pm_wq, &pw_info_work,
				msecs_to_jiffies(apupw_dbg.poll_interval));
		break;
	case SHOW_LVL:
		apupw_dbg.option = POWER_PARAM_LOG_LEVEL;
		break;
	default:
		ret = -EINVAL;
		goto out;

	}
	if (args[0] < SHOW_LVL)
		apupw_dbg_set_loglvl(args[0]);

out:
	return ret;
}

static int apupw_dbg_power_stress(int type, int device, int opp)
{
	int id = 0;
	struct apu_dev *ad = NULL;
	struct apu_gov_data *gov_data = NULL;

	pr_info("%s begin with type %d +++\n", __func__, type);
	pr_info("%s type %d, device %d, opp %d\n", __func__, type, device, opp);

	if (type < 0 || type >= 10) {
		pr_info("%s err with type = %d\n", __func__, type);
		return -1;
	}

	switch (type) {
	case 0: /* config opp */
		if (device == 9) { /* all devices */
			for (id = 0 ; id < APUSYS_POWER_USER_NUM ; id++) {
				if (IS_ERR_OR_NULL(apu_find_device(id)) || id == APUCB)
					continue;

				/* if vcore adopt user devine governor, it may not activate update_devfreq */
				if (id != APUCORE)
					apu_device_set_opp(id, opp);
				else
					apu_qos_set_vcore(opp);
			}
		} else {
			id = _apupw_dbg_id2user(device);
			if (id < 0) {
				pr_info("%s err with device = %d\n", __func__, device);
				return -1;
			}
			apu_device_set_opp(id, opp);
		}
		break;

	case 1: /* config power on */
		if (device == 9) { /* all devices */
			for (id = 0 ; id < APUSYS_POWER_USER_NUM ; id++) {
				if (IS_ERR_OR_NULL(_apupw_valid_leaf_df(id)))
					continue;
				apu_device_power_on(id);
			}
		} else {
			id = _apupw_dbg_id2user(device);
			if (id < 0) {
				pr_info("%s err with device = %d\n", __func__, device);
				return -1;
			}
			apu_device_power_on(id);
		}
		break;

	case 2: /* config power off */
		if (device == 9) { /* all devices */
			/* set all devices to slowest opp */
			for (id = 0 ; id < APUSYS_POWER_USER_NUM ; id++) {
				ad = apu_find_device(id);
				if (IS_ERR_OR_NULL(ad) || id == APUCB)
					continue;

				/* if vcore adopt user devine governor, it may not activate update_devfreq */
				gov_data = ad->df->data;
				if (id != APUCORE)
					apu_device_set_opp(id, gov_data->max_opp);
				else
					apu_qos_set_vcore(gov_data->max_opp);
			}
			/* turn all devices off */
			for (id = 0 ; id < APUSYS_POWER_USER_NUM ; id++) {
				if (IS_ERR_OR_NULL(_apupw_valid_leaf_df(id)))
					continue;
				apu_device_power_off(id);
			}

		} else {
			id = _apupw_dbg_id2user(device);
			if (id < 0) {
				pr_info("%s err with device = %d\n", __func__, device);
				return -1;
			}
			ad = apu_find_device(id);
			if (IS_ERR_OR_NULL(ad))
				break;

			/*
			 * 1. set specific device's opp as slowest.
			 * 2. turn the specific device off
			 */
			gov_data = ad->df->data;
			if (id != APUCORE)
				apu_device_set_opp(id, gov_data->max_opp);
			else
				apu_qos_set_vcore(gov_data->max_opp);
			apu_device_power_off(id);
		}
		break;

	default:
		pr_info("%s invalid type %d !\n", __func__, type);
	}

	pr_info("%s end with type %d ---\n", __func__, type);
	return 0;
}

static int apupw_dbg_dvfs(u8 param, int argc, int *args)
{
	enum DVFS_USER user;
	struct apu_dev *ad = NULL;
	int ret = 0;

	pr_info("[%s] @@test%d lock opp=%d\n", __func__, argc, (int)(args[0]));
	for (user = MDLA; user < APUSYS_POWER_USER_NUM; user++) {
		ad = _apupw_valid_df(user);
		if (!ad)
			continue;
		if (args[0] >= 0)
			ret = _apupw_set_freq_range(ad, TOKHZ(apu_opp2freq(ad, args[0])),
						    TOKHZ(apu_opp2freq(ad, args[0])));
		else
			ret = _apupw_default_freq_range(ad);
	}

	/* only ret < 0 is fail, since dev_pm_qos_update_request will return 1 when pass */
	if (ret < 0)
		pr_info("[%s] @@test%d lock opp=%d fail, ret %d\n",
			__func__, argc, (int)(args[0]), ret);
	return ret;
}

static int apupw_dbg_dump_table(struct seq_file *s)
{
	int len = 0;
	char info[128];
	char *separate = NULL;
	struct dev_pm_opp *opp = NULL;
	struct apu_dev *ad = NULL;
	enum DVFS_USER usr;
	int ret = 0;
	ulong freq = 0, volt = 0;

	for (usr = MDLA; usr < APUSYS_POWER_USER_NUM; usr++) {
		ad = _apupw_valid_parent_df(usr);
		if (IS_ERR_OR_NULL(ad))
			continue;

		memset(info, 0, sizeof(info));
		freq = len = 0;
		len += snprintf((info + len), (sizeof(info) - len),
				"|%*s|", 10, apu_dev_string(ad->user));
		do {
			opp = dev_pm_opp_find_freq_ceil(ad->dev, &freq);
			if (IS_ERR(opp)) {
				if (PTR_ERR(opp) == -ERANGE)
					break;
				ret = PTR_ERR(opp);
				goto out;
			}
			freq = dev_pm_opp_get_freq(opp);
			volt = dev_pm_opp_get_voltage(opp);
			dev_pm_opp_put(opp);
			len += snprintf((info + len), (sizeof(info) - len),
					"%3luMhz(%3lumv) [%d/%3d]|", TOMHZ(freq), TOMV(volt),
					apu_freq2opp(ad, freq), apu_freq2boost(ad, freq));
			freq += 1;
		} while (1);
		seq_printf(s, info);
		/* add separator line */
		separate = kzalloc(len, GFP_KERNEL);
		memset(separate, '-', len - 1);
		_apupw_separte(s, separate);
		kfree(separate);
	}

out:
	return ret;
}

static int apupw_dbg_dump_stat(struct seq_file *s)
{
	struct apu_dbg_clk *dbg_clk = NULL;
	struct apu_dbg_regulator *dbg_reg = NULL;
	struct apu_dbg_cg *dbg_cg = NULL;
	u64 time;
	ulong  rem_nsec;

	time = sched_clock();
	rem_nsec = do_div(time, 1000000000);

	seq_printf(s, "[%5lu.%06lu]\n|curr|", (ulong)time, rem_nsec / 1000);
	list_for_each_entry_reverse(dbg_clk, &apupw_dbg.clk_list, node)
		seq_printf(s, "%s|", dbg_clk->name);
	seq_puts(s, "\n");

	seq_puts(s, "|freq|");
	list_for_each_entry_reverse(dbg_clk, &apupw_dbg.clk_list, node)
		seq_printf(s, "%lu|", TOMHZ(clk_get_rate(dbg_clk->clk)));
	seq_puts(s, "\n");

	seq_puts(s, "|clk |");
	list_for_each_entry_reverse(dbg_clk, &apupw_dbg.clk_list, node)
		seq_printf(s, "%s|", __clk_get_name(dbg_clk->clk));
	seq_puts(s, "\n(unit: MHz)\n\n");

	list_for_each_entry_reverse(dbg_reg, &apupw_dbg.reg_list, node)
		seq_printf(s, "%s:%u(mV), ", dbg_reg->name,
			   TOMV(regulator_get_voltage(dbg_reg->reg)));
	seq_printf(s, "\n\nrpc_intf_rdy:0x%lx, spm_wakeup:0x%lx\n",
		   apu_rpc_rdy_value(), apu_spm_wakeup_value());
	list_for_each_entry_reverse(dbg_cg, &apupw_dbg.cg_list, node)
		seq_printf(s, "%s:0x%x, ", dbg_cg->name, apu_readl(dbg_cg->reg));
	seq_puts(s, "\n");

	return 0;
}

static int apupw_dbg_show(struct seq_file *s, void *unused)
{
	switch (apupw_dbg.option) {
	case POWER_PARAM_OPP_TABLE:
		apupw_dbg_dump_table(s);
		break;
	case POWER_PARAM_CURR_STATUS:
		apupw_dbg_dump_stat(s);
		break;
	case POWER_PARAM_LOG_LEVEL:
		seq_printf(s, "log_level = %d\n", apupw_dbg_get_loglvl());
		seq_printf(s, "fix_opp = %d\n", apupw_dbg_get_fixopp());
		break;
	default:
		break;
	}
	return 0;
}

static int apupw_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, apupw_dbg_show, inode->i_private);
}

static int apupw_dbg_set_parameter(u8 param, int argc, int *args)
{
	int ret = 0;
	struct apu_dev *ad = NULL;

	ret = _apupw_valid_input(param, argc);
	if (!ret)
		goto out;

	switch (param) {
	case POWER_PARAM_FIX_OPP:
		apupw_dbg_set_fixopp(args[0]);
		break;
	case POWER_PARAM_LOG_LEVEL:
		ret = apupw_dbg_loglvl(param, argc, args);
		break;
	case POWER_PARAM_DVFS_DEBUG:
		ret = apupw_dbg_dvfs(param, argc, args);
		break;
	case POWER_HAL_CTL:
	case POWER_PARAM_SET_POWER_HAL_OPP:
		ad = _apupw_valid_df(_apupw_dbg_id2user(args[0]));
		if (IS_ERR_OR_NULL(ad)) {
			ret = PTR_ERR(ad);
			goto out;
		}
		if (param == POWER_PARAM_SET_POWER_HAL_OPP)
			ret = _apupw_set_freq_range(ad, apu_opp2freq(ad, args[2]),
						    apu_opp2freq(ad, args[1]));
		else if (param == POWER_HAL_CTL)
			ret = _apupw_set_freq_range(ad, apu_boost2freq(ad, args[2]),
						    apu_boost2freq(ad, args[1]));
		break;
	case POWER_PARAM_POWER_STRESS:
		/*
		 * arg0 : type
		 * arg1 : device , 9 = all devices
		 * arg2 : opp
		 */
		ret = apupw_dbg_power_stress(args[0], args[1], args[2]);
		break;
	case POWER_PARAM_CURR_STATUS:
		apupw_dbg.option = POWER_PARAM_CURR_STATUS;
		break;
	case POWER_PARAM_OPP_TABLE:
		apupw_dbg.option = POWER_PARAM_OPP_TABLE;
		break;
	default:
		pr_info("unsupport the power parameter:%d\n", param);
		ret = -EINVAL;
		break;
	}

out:
	return ret;
}

static ssize_t apupw_dbg_write(struct file *flip, const char __user *buffer,
			       size_t count, loff_t *f_pos)
{
#define MAX_ARG 5
	char *tmp, *token, *cursor;
	int ret, i, param;
	unsigned int args[MAX_ARG];

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		pr_info("[%s] copy_from_user failed, ret=%d\n", __func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	cursor = tmp;
	/* parse a command */
	token = strsep(&cursor, " ");
	if (!strcmp(token, "fix_opp"))
		param = POWER_PARAM_FIX_OPP;
	else if (!strcmp(token, "dvfs_debug"))
		param = POWER_PARAM_DVFS_DEBUG;
	else if (!strcmp(token, "power_hal"))
		param = POWER_HAL_CTL;
	else if (!strcmp(token, "power_hal_opp"))
		param = POWER_PARAM_SET_POWER_HAL_OPP;
	else if (!strcmp(token, "power_stress"))
		param = POWER_PARAM_POWER_STRESS;
	else if (!strcmp(token, "opp_table"))
		param = POWER_PARAM_OPP_TABLE;
	else if (!strcmp(token, "curr_status"))
		param = POWER_PARAM_CURR_STATUS;
	else if (!strcmp(token, "log_level"))
		param = POWER_PARAM_LOG_LEVEL;
	else {
		ret = -EINVAL;
		pr_info("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < MAX_ARG && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtoint(token, 10, &args[i]);
		if (ret) {
			pr_info("fail to parse args[%d](%s)", i, token);
			goto out;
		}
	}
	apupw_dbg_set_parameter(param, i, args);
	ret = count;
out:
	kfree(tmp);
	return ret;
}

static const struct file_operations apupw_dbg_fops = {
	.open = apupw_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = apupw_dbg_write,
};

int apupw_dbg_register_nodes(struct device *dev)
{
	struct property *prop;
	const char *name;
	struct regulator *reg;
	int idx = 0, ret = 0, i = 0;
	u64 phyaddr = 0, offset = 0;
	struct clk *clk;
	unsigned int psize;
	const __be32 *paddr = NULL;
	void __iomem *cgaddr = NULL;

	INIT_LIST_HEAD(&apupw_dbg.clk_list);
	INIT_LIST_HEAD(&apupw_dbg.cg_list);
	INIT_LIST_HEAD(&apupw_dbg.reg_list);

	of_property_for_each_string(dev->of_node, "voltage-names",
				    prop, name) {
		reg = regulator_get_optional(dev, name);
		if (IS_ERR_OR_NULL(reg)) {
			ret = PTR_ERR(reg);
			apower_err(dev, "Get [%s] voltage node fail, ret = %d\n", name, ret);
			goto out;
		}

		ret = apupw_dbg_register_regulator(name, reg);
		if (ret)
			goto out;
	}

	of_property_for_each_string(dev->of_node, "clock-names",
				    prop, name) {
		clk = clk_get(dev, name);
		if (IS_ERR_OR_NULL(clk)) {
			ret = PTR_ERR(clk);
			apower_err(dev, "Get [%s] clock node fail, ret = %d\n", name, ret);
			goto out;
		}

		ret = apupw_dbg_register_clk(name, clk);
		if (ret)
			goto out;
	}

	idx = 0;
	of_property_for_each_string(dev->of_node, "cg-names",
				    prop, name) {
		paddr = of_get_property(dev->of_node, "cgs", &psize);
		if (!paddr) {
			ret = -EINVAL;
			apower_err(dev, "Get [%s] cg node fail, ret = %d\n", name, ret);
			goto out;
		}
		psize /= 4;
		for (i = 0; psize >= 4; psize -= 4, paddr += 4, i++)
			if (i == idx) {
				offset = of_read_number(paddr + 2, 2);
				break;
			}
		phyaddr = of_translate_address(dev->of_node, paddr);
		cgaddr = ioremap(phyaddr, PAGE_SIZE);
		if (!cgaddr) {
			ret = -ENOMEM;
			goto out;
		}

		ret = apupw_dbg_register_cg(name, (cgaddr + offset));
		if (ret)
			goto out;
		idx++;
	}
out:
	return ret;
}

void apupw_dbg_release_nodes(void)
{
	apupw_dbg_unregister_clk();
	apupw_dbg_unregister_regulator();
	apupw_dbg_unregister_cg();
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int apupw_dbg_init(struct apusys_core_info *info)
{
	/* creating apupw directory */
	apupw_dbg.dir = debugfs_create_dir("apupwr", info->dbg_root);
	if (IS_ERR_OR_NULL(apupw_dbg.dir)) {
		pr_info("failed to create \"apupwr\" debug dir.\n");
		goto out;
	}

	/* creating power file */
	apupw_dbg.file = debugfs_create_file("power", (0644),
		apupw_dbg.dir, NULL, &apupw_dbg_fops);
	if (IS_ERR_OR_NULL(apupw_dbg.file)) {
		pr_info("failed to create \"power\" debug file.\n");
		goto out;
	}

	/* symbolic link to /d/apupwr/power */
	apupw_dbg.sym_link = debugfs_create_symlink("power", info->dbg_root, "./apupwr/power");
	if (IS_ERR_OR_NULL(apupw_dbg.sym_link)) {
		pr_info("failed to create \"power\" symbolic file.\n");
		goto out;
	}
out:
	return 0;
}

void apupw_dbg_exit(void)
{
	debugfs_remove(apupw_dbg.sym_link);
	debugfs_remove(apupw_dbg.file);
	debugfs_remove(apupw_dbg.dir);
}
#else
int apupw_dbg_init(struct apusys_core_info *info) { return 0; }
void apupw_dbg_exit(void) {}
#endif

