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

#include "apu_dbg.h"
#include "apu_log.h"
#include "apu_common.h"
#include "apu_dbg.h"
#include "apusys_power.h"
#include "apu_rpc.h"
#include "apusys_core.h"

static LIST_HEAD(dbg_clk_list);
static LIST_HEAD(dbg_regulator_list);
static struct dentry *apusys_power_dir;
static void dbg_power_info(struct work_struct *work);
DECLARE_DELAYED_WORK(pw_info_work, dbg_power_info);
static char buffer[128];
int log_level = 1;
int poll_interval;
/**
 * struct apu_dbg_clk - associate a clk with a notifier
 * @clk: struct clk * to associate the notifier with
 * @notifier_head: a blocking_notifier_head for this clk
 * @node: linked list pointers
 *
 * A list of struct clk_notifier is maintained by the notifier code.
 * An entry is created whenever code registers the first notifier on a
 * particular @clk.  Future notifiers on that @clk are added to the
 * @notifier_head.
 */
struct apu_dbg_clk {
	enum DVFS_USER user;
	const char *name;
	struct clk	*clk;
	struct list_head node;
};

/**
 * struct apu_dbg_clk - associate a clk with a notifier
 * @clk: struct clk * to associate the notifier with
 * @notifier_head: a blocking_notifier_head for this clk
 * @node: linked list pointers
 *
 * A list of struct clk_notifier is maintained by the notifier code.
 * An entry is created whenever code registers the first notifier on a
 * particular @clk.  Future notifiers on that @clk are added to the
 * @notifier_head.
 */
struct apu_dbg_regulator {
	enum DVFS_USER user;
	const char *name;
	struct regulator *reg;
	struct list_head node;
};

/**
 * _dbg_id_to_dvfsuser() - transfer wiki's id to dvfs_user
 * @dbg_id: the device id defined on wiki.
 *
 * Based on wiki's document, transfer real dvfs_user.
 */
static const enum DVFS_USER _dbg_id_to_dvfsuser(int dbg_id)
{
	static const int ids[] = {
		[0] = VPU0,
		[1] = VPU1,
		[2] = VPU2,
		[3] = MDLA0,
		[4] = MDLA1,
	};

	if (dbg_id < 0 || dbg_id >= ARRAY_SIZE(ids))
		return -EINVAL;
	return ids[dbg_id];
}

static void dbg_power_info(struct work_struct *work)
{
	struct apu_dbg_clk *dbg_clk;
	struct apu_dbg_regulator *dbg_reg;
	int n_pos;

	memset(buffer, 0, sizeof(buffer));
	n_pos = snprintf(buffer, (sizeof(buffer) - 1), "APUPWR v[");
	list_for_each_entry_reverse(dbg_reg, &dbg_regulator_list, node)
		n_pos += snprintf((buffer + n_pos), (sizeof(buffer) - n_pos - 1),
				"%u,", TOMV(regulator_get_voltage(dbg_reg->reg)));

	n_pos += snprintf((buffer + n_pos), (sizeof(buffer) - n_pos - 1), "]f[");

	/* search the list of dbg_clk_list for this clk */
	list_for_each_entry_reverse(dbg_clk, &dbg_clk_list, node)
		n_pos += snprintf((buffer + n_pos), (sizeof(buffer) - n_pos - 1),
				"%u,", TOMHZ(clk_get_rate(dbg_clk->clk)));

	n_pos += snprintf((buffer + n_pos), (sizeof(buffer) - n_pos - 1),
			"]r[%x,%x]\n", apu_spm_wakeup_value(), apu_rpc_rdy_value());

	pr_info("%s", buffer);
	if (poll_interval)
		queue_delayed_work(pm_wq, &pw_info_work,
			msecs_to_jiffies(poll_interval));
}

int apu_dbg_register_clk(const char *name, struct clk *clk)
{
	struct apu_dbg_clk *dbg_clk = NULL;
	int ret = -ENOMEM;

	if (IS_ERR_OR_NULL(clk) || !name)
		return -EINVAL;

	if (list_empty(&dbg_clk_list))
		goto allocate;

	/* search the list of dbg_clk_list for this clk */
	list_for_each_entry(dbg_clk, &dbg_clk_list, node)
		if (!dbg_clk || dbg_clk->clk == clk)
			break;
allocate:

	/* if clk wasn't in the dbg_clk_list, allocate new clk_notifier */
	if (!dbg_clk || dbg_clk->clk != clk) {
		dbg_clk = kzalloc(sizeof(*dbg_clk), GFP_KERNEL);
		if (!dbg_clk)
			goto out;
		dbg_clk->clk = clk;
		dbg_clk->name = name;
		list_add(&dbg_clk->node, &dbg_clk_list);
		ret = 0;
	}
out:
	return ret;
}


void apu_dbg_unregister_clk(void)
{
	struct apu_dbg_clk *dbg_clk = NULL;

	if (list_empty(&dbg_clk_list))
		return;

	list_for_each_entry(dbg_clk, &dbg_clk_list, node) {
		list_del(&dbg_clk->node);
		kfree(dbg_clk);
	}
}

int apu_dbg_register_regulator(const char *name, struct regulator *reg)
{
	struct apu_dbg_regulator *dbg_reg = NULL;
	int ret = -ENOMEM;

	if (IS_ERR_OR_NULL(reg) || !name)
		return -EINVAL;

	if (list_empty(&dbg_regulator_list))
		goto allocate;
	/* search the list of dbg_clk_list for this clk */
	list_for_each_entry(dbg_reg, &dbg_regulator_list, node)
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
		list_add(&dbg_reg->node, &dbg_regulator_list);
		ret = 0;
	}
out:
	return ret;
}


void apu_dbg_unregister_regulator(void)
{
	struct apu_dbg_regulator *dbg_reg = NULL;

	if (list_empty(&dbg_regulator_list))
		return;

	list_for_each_entry(dbg_reg, &dbg_regulator_list, node) {
		list_del(&dbg_reg->node);
		kfree(dbg_reg);
	}
}

int apu_power_power_stress(int type, int device, int opp)
{
	int id = 0;

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
				apu_device_set_opp(id, opp);
			}
		} else {
			id = _dbg_id_to_dvfsuser(device);
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
				if (IS_ERR_OR_NULL(apu_find_device(id)) && id != APUCB)
					continue;
				apu_device_power_on(id);
			}
		} else {
			id = _dbg_id_to_dvfsuser(device);
			if (id < 0) {
				pr_info("%s err with device = %d\n", __func__, device);
				return -1;
			}
			apu_device_power_on(id);
		}
		break;

	case 2: /* config power off */
		if (device == 9) { /* all devices */
			for (id = 0 ; id < APUSYS_POWER_USER_NUM ; id++) {
				if (IS_ERR_OR_NULL(apu_find_device(id)) && id != APUCB)
					continue;
				apu_device_power_off(id);
			}
		} else {
			id = _dbg_id_to_dvfsuser(device);
			if (id < 0) {
				pr_info("%s err with device = %d\n", __func__, device);
				return -1;
			}
			apu_device_power_off(id);
		}
		break;

	default:
		pr_info("%s invalid type %d !\n", __func__, type);
	}

	pr_info("%s end with type %d ---\n", __func__, type);
	return 0;
}

static inline void change_log_level(int new_level)
{
	log_level = new_level;
	pr_info("%s, new log level = %d\n", __func__, log_level);

}

static int apusys_debug_power_show(struct seq_file *s, void *unused)
{
/* TODO
 *	switch (g_debug_option) {
 *	case POWER_PARAM_OPP_TABLE:
 *		apu_power_dump_opp_table(s);
 *		break;
 *	case POWER_PARAM_CURR_STATUS:
 *		apu_power_dump_curr_status(s, 0);
 *		break;
 *	case POWER_PARAM_LOG_LEVEL:
 *		seq_printf(s, "g_pwr_log_level = %d\n", g_pwr_log_level);
 *		break;
 *	default:
 *		apu_power_dump_curr_status(s, 1); // one line string
 *	}
 */
	return 0;
}

static int apusys_debug_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_debug_power_show, inode->i_private);
}

static int apusys_set_power_parameter(uint8_t param, int argc, int *args)
{
	int ret = 0;
	enum DVFS_USER user;
	struct apu_dev *ad;
	struct apu_gov_data *gov_data;
	int freq;

	switch (param) {
	case POWER_PARAM_LOG_LEVEL:
		switch (args[0]) {
		case 0:
			cancel_delayed_work_sync(&pw_info_work);
			change_log_level(args[0]);
			break;
		case 1:
			queue_delayed_work(pm_wq, &pw_info_work,
					msecs_to_jiffies(poll_interval));
			break;
		case 2:
			change_log_level(args[0]);
			break;
		case 3:
			poll_interval = (args[1]);
			pr_info("%s, new poll_interval = %d\n", __func__, poll_interval);
			if (poll_interval)
				queue_delayed_work(pm_wq, &pw_info_work,
					msecs_to_jiffies(poll_interval));
			break;

		default:
			if (ret) {
				pr_info("invalid argument, received:%d\n",
						(int)(args[0]));
				goto out;
			}
			ret = -EINVAL;
			goto out;

		}
		break;

	case POWER_PARAM_DVFS_DEBUG:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			pr_info(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}
		pr_info("@@test%d\n", argc);
		pr_info("lock opp=%d\n", (int)(args[0]));
		for (user = MDLA; user < APUSYS_POWER_USER_NUM; user++) {
			ad = apu_find_device(user);
			if (IS_ERR_OR_NULL(ad) || IS_ERR_OR_NULL(ad->devfreq))
				continue;
			gov_data = (struct apu_gov_data *)ad->devfreq->data;
			if (gov_data->depth)
				continue;
			freq = apu_opp2freq(ad, (int)(args[0]));
			mutex_lock_nested(&ad->devfreq->lock, gov_data->depth);
			ad->devfreq->max_freq = freq;
			ad->devfreq->min_freq = freq;
			mutex_unlock(&ad->devfreq->lock);
		}
		break;
	case POWER_HAL_CTL:
	{
		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			pr_info(
			"invalid argument, expected:1, received:%d\n",
								argc);
			goto out;
		}

		user = _dbg_id_to_dvfsuser(args[0]);
		if (user < 0) {
			pr_info("APU dvfsuser %d not exist\n", args[0]);
			goto out;
		}
		ad = apu_find_device(user);
		if (IS_ERR_OR_NULL(ad))
			goto out;

		mutex_lock(&ad->devfreq->lock);
		ad->devfreq->max_freq = apu_opp2freq(ad, args[1]);
		ad->devfreq->min_freq = apu_opp2freq(ad, args[2]);
		mutex_unlock(&ad->devfreq->lock);

		break;
	}
	case POWER_PARAM_POWER_STRESS:
		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			pr_info(
				"invalid argument, expected:3, received:%d\n",
				argc);
			goto out;
		}
		/*
		 * arg0 : type
		 * arg1 : device , 9 = all devices
		 * arg2 : opp
		 */
		apu_power_power_stress(args[0], args[1], args[2]);
		break;

/* TODO
 *	case POWER_PARAM_SET_USER_OPP:
 *		ret = (argc == 2) ? 0 : -EINVAL;
 *		if (ret) {
 *			PWR_LOG_INF(
 *				"invalid argument, expected:1, received:%d\n",
 *									argc);
 *			goto out;
 *		}
 *		apusys_set_opp(args[0], args[1]);
 *		apusys_dvfs_policy(0);
 *		break;
 *	case POWER_PARAM_SET_THERMAL_OPP:
 *			ret = (argc == 2) ? 0 : -EINVAL;
 *			if (ret) {
 *				PWR_LOG_ERR(
 *				"invalid argument, expected:1, received:%d\n",
 *									argc);
 *				goto out;
 *			}
 *			if (args[0] < 0 || args[0] >= APUSYS_DVFS_USER_NUM) {
 *				PWR_LOG_ERR("user(%d) is invalid\n",
 *						(int)(args[0]));
 *				goto out;
 *			}
 *
 *			ret = (args[1] >= APUSYS_MAX_NUM_OPPS);
 *			if (ret) {
 *				PWR_LOG_ERR("opp-%d is too big, max opp:%d\n",
 *					    (int)(args[0]),
 *					    APUSYS_MAX_NUM_OPPS - 1);
 *				goto out;
 *			}
 *
 *			apusys_opps.thermal_opp[args[0]] = args[1];
 *			apusys_dvfs_policy(0);
 *			break;
 *	case POWER_PARAM_SET_POWER_HAL_OPP:
 *			ret = (argc == 3) ? 0 : -EINVAL;
 *			if (ret) {
 *				PWR_LOG_INF(
 *				"invalid argument, expected:1, received:%d\n",
 *									argc);
 *				goto out;
 *			}
 *
 *			if (args[0] < 0 || args[0] >= APUSYS_DVFS_USER_NUM) {
 *				PWR_LOG_ERR("user(%d) is invalid\n",
 *						(int)(args[0]));
 *				goto out;
 *			}
 *
 *			ret = (args[1] >= APUSYS_MAX_NUM_OPPS);
 *			if (ret) {
 *				PWR_LOG_ERR("opp-%d is too big, max opp:%d\n",
 *					    (int)(args[1]),
 *					    APUSYS_MAX_NUM_OPPS - 1);
 *				goto out;
 *			}
 *
 *			ret = (args[1] >= APUSYS_MAX_NUM_OPPS);
 *			if (ret) {
 *				PWR_LOG_ERR("opp-%d is too big, max opp:%d\n",
 *					    (int)(args[1]),
 *					    APUSYS_MAX_NUM_OPPS - 1);
 *				goto out;
 *			}
 *
 *			apusys_opps.power_lock_min_opp[args[0]] = args[1];
 *			apusys_opps.power_lock_max_opp[args[0]] = args[2];
 *			break;
 *	case POWER_PARAM_GET_POWER_REG:
 *		ret = (argc == 1) ? 0 : -EINVAL;
 *		if (ret) {
 *			PWR_LOG_INF(
 *				"invalid argument, expected:1, received:%d\n",
 *									argc);
 *			goto out;
 *		}
 *		apu_power_reg_dump();
 *		break;
 *	case POWER_PARAM_OPP_TABLE:
 *		ret = (argc == 1) ? 0 : -EINVAL;
 *		if (ret) {
 *			PWR_LOG_ERR(
 *				"invalid argument, expected:1, received:%d\n",
 *				argc);
 *			goto out;
 *		}
 *		g_debug_option = POWER_PARAM_OPP_TABLE;
 *		break;
 *	case POWER_PARAM_CURR_STATUS:
 *		ret = (argc == 1) ? 0 : -EINVAL;
 *		if (ret) {
 *			PWR_LOG_ERR(
 *				"invalid argument, expected:1, received:%d\n",
 *				argc);
 *			goto out;
 *		}
 *		g_debug_option = POWER_PARAM_CURR_STATUS;
 *		break;
 *	case POWER_PARAM_LOG_LEVEL:
 *		ret = (argc == 1) ? 0 : -EINVAL;
 *		if (ret) {
 *			PWR_LOG_ERR(
 *				"invalid argument, expected:1, received:%d\n",
 *				argc);
 *			goto out;
 *		}
 *		if (args[0] == 9)
 *			g_debug_option = POWER_PARAM_LOG_LEVEL;
 *		else
 *			change_log_level(args[0]);
 *		break;
 *
 */
	default:
		pr_info("unsupport the power parameter:%d\n", param);
		break;
	}

out:
	return ret;
}


static ssize_t apusys_debug_power_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	const int max_arg = 5;
	unsigned int args[max_arg];

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
	if (strcmp(token, "fix_opp") == 0)
		param = POWER_PARAM_FIX_OPP;
	else if (strcmp(token, "dvfs_debug") == 0)
		param = POWER_PARAM_DVFS_DEBUG;
	else if (strcmp(token, "power_hal") == 0)
		param = POWER_HAL_CTL;
	else if (strcmp(token, "user_opp") == 0)
		param = POWER_PARAM_SET_USER_OPP;
	else if (strcmp(token, "thermal_opp") == 0)
		param = POWER_PARAM_SET_THERMAL_OPP;
	else if (strcmp(token, "power_hal_opp") == 0)
		param = POWER_PARAM_SET_POWER_HAL_OPP;
	else if (strcmp(token, "reg_dump") == 0)
		param = POWER_PARAM_GET_POWER_REG;
	else if (strcmp(token, "power_stress") == 0)
		param = POWER_PARAM_POWER_STRESS;
	else if (strcmp(token, "opp_table") == 0)
		param = POWER_PARAM_OPP_TABLE;
	else if (strcmp(token, "curr_status") == 0)
		param = POWER_PARAM_CURR_STATUS;
	else if (strcmp(token, "log_level") == 0)
		param = POWER_PARAM_LOG_LEVEL;
	else {
		ret = -EINVAL;
		pr_info("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			pr_info("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	apusys_set_power_parameter(param, i, args);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations apusys_debug_power_fops = {
	.open = apusys_debug_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = apusys_debug_power_write,
};

int apu_power_drv_init(struct apusys_core_info *info)
{
	int ret;

	apusys_power_dir = debugfs_create_dir("apupwr", info->dbg_root);
	ret = IS_ERR_OR_NULL(apusys_power_dir);
	if (ret) {
		pr_info("failed to create debug dir.\n");
		goto out;
	}
	debugfs_create_file("power", (0644),
		apusys_power_dir, NULL, &apusys_debug_power_fops);
	debugfs_create_symlink("power", info->dbg_root,	"./apupwr/power");

out:
	return ret;
}
EXPORT_SYMBOL(apu_power_drv_init);

void apu_power_drv_exit(void)
{
	debugfs_remove(apusys_power_dir);
}
EXPORT_SYMBOL(apu_power_drv_exit);

