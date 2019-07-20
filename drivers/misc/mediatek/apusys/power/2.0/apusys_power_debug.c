/*
 * Copyright (C) 2019 MediaTek Inc.
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


#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>


#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/dcache.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/string.h>


#include "apusys_power_ctl.h"
#include "hal_config_power.h"
#include "apu_log.h"
#include "apusys_power_debug.h"


static int apusys_debug_power_show(struct seq_file *s, void *unused)
{
	return 0;
}

static int apusys_debug_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_debug_power_show, inode->i_private);
}



int apusys_set_power_parameter(uint8_t param, int argc, int *args)
{
	int ret = 0;
	int i = 0, j = 0;

	switch (param) {
	case POWER_PARAM_FIX_OPP:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}
		switch (args[0]) {
		case 0:
			is_power_debug_lock = false;
			break;
		case 1:
			is_power_debug_lock = true;
			break;
		default:

			if (ret) {
				PWR_LOG_INF("invalid argument, received:%d\n",
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
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}

		ret = args[0] >= APUSYS_MAX_NUM_OPPS;
		if (ret) {
			PWR_LOG_INF(
				"opp step(%d) is out-of-bound,	 max opp:%d\n",
					(int)(args[0]), APUSYS_MAX_NUM_OPPS);
			goto out;
		}
		PWR_LOG_INF("@@test%d\n", argc);

		for (i = 0; i < APUSYS_BUCK_DOMAIN_NUM; i++)
			apusys_opps.cur_opp_index[i] = args[0];

		is_power_debug_lock = true;

		break;
#if 0
	case POWER_PARAM_JTAG:
		mdla_put_power(0);
		#if 0
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}

		is_jtag_enabled = args[0];
		ret = vpu_hw_enable_jtag(is_jtag_enabled);
		#endif
		break;

	case POWER_PARAM_LOCK:

		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}

		is_power_debug_lock = args[0];

		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}

		ret = args[0] >= opps.count;
		if (ret) {
			PWR_LOG_INF("opp step(%d) is out-of-bound, count:%d\n",
					(int)(args[0]), opps.count);
			goto out;
		}
		PWR_LOG_INF("@@lock%d\n", argc);

		opps.vcore.index = opps.vcore.opp_map[args[0]];
		opps.vvpu.index = opps.vvpu.opp_map[args[0]];
		opps.vmdla.index = opps.vmdla.opp_map[args[0]];
		opps.dsp.index = opps.dsp.opp_map[args[0]];
		opps.ipu_if.index = opps.ipu_if.opp_map[args[0]];
		opps.mdlacore.index = opps.mdlacore.opp_map[args[0]];


		mdla_opp_check(0, opps.dsp.index, opps.dsp.index);
		//user->power_opp = power->opp_step;

		ret = mdla_get_power(0);





		break;
#endif
	case POWER_HAL_CTL:
	{
		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
			"invalid argument, expected:1, received:%d\n",
								argc);
			goto out;
		}

		if (args[0] < 0 || args[0] > 1) {
			PWR_LOG_INF("user is invalid\n",
					(int)(args[0]));
			goto out;
		}
		if (args[1] > 100 || args[1] < 0) {
			PWR_LOG_INF("min boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		if (args[2] > 100 || args[2] < 0) {
			PWR_LOG_INF("max boost(%d) is out-of-bound\n",
					(int)(args[2]));
			goto out;
		}

		if (args[0] == 0 && APUSYS_VPU_NUM != 0) {
			for (i = VPU0; i < VPU0 + APUSYS_VPU_NUM; i++) {
				apusys_opps.power_lock_max_opp[i] =
					apusys_boost_value_to_opp(i, args[1]);
				apusys_opps.power_lock_min_opp[i] =
					apusys_boost_value_to_opp(i, args[2]);
			}
		}

		if (args[0] == 1 && APUSYS_MDLA_NUM != 0) {
			for (i = MDLA0; i < MDLA0 + APUSYS_MDLA_NUM; i++) {
				apusys_opps.power_lock_max_opp[i] =
					apusys_boost_value_to_opp(i, args[1]);
				apusys_opps.power_lock_min_opp[i] =
					apusys_boost_value_to_opp(i, args[2]);
			}
		}

		break;
	}
#if 0
	case POWER_EARA_CTL:
	{
		struct mdla_lock_power mdla_lock_power;

		ret = (argc == 2) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:3, received:%d\n",
									argc);
			goto out;
		}
		if (args[0] > 100 || args[0] < 0) {
			PWR_LOG_INF("min boost(%d) is out-of-bound\n",
					(int)(args[0]));
			goto out;
		}
		if (args[1] > 100 || args[1] < 0) {
			PWR_LOG_INF("max boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		mdla_lock_power.core = 1;
		mdla_lock_power.lock = true;
		mdla_lock_power.priority = MDLA_OPP_EARA_QOS;
		mdla_lock_power.max_boost_value = args[1];
		mdla_lock_power.min_boost_value = args[0];
		mdla_dvfs_debug("[mdla]EARA_LOCK+core:%d, maxb:%d, minb:%d\n",
			mdla_lock_power.core, mdla_lock_power.max_boost_value,
				mdla_lock_power.min_boost_value);
		ret = mdla_lock_set_power(&mdla_lock_power);
		if (ret) {
			PWR_LOG_INF("[POWER_HAL_LOCK]failed, ret=%d\n", ret);
			goto out;
		}
		break;
		}
#endif
	case POWER_PARAM_SET_USER_OPP:
		ret = (argc == 2) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}
		apusys_set_opp(args[0], args[1]);
		apusys_dvfs_policy(0);
		break;
	case POWER_PARAM_SET_THERMAL_OPP:
			ret = (argc == 2) ? 0 : -EINVAL;
			if (ret) {
				PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
				goto out;
			}
			apusys_opps.thermal_opp[args[0]] = args[1];
			apusys_dvfs_policy(0);
			break;
	case POWER_PARAM_SET_POWER_HAL_OPP:
			ret = (argc == 3) ? 0 : -EINVAL;
			if (ret) {
				PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
				goto out;
			}
			apusys_opps.power_lock_min_opp[args[0]] = args[1];
			apusys_opps.power_lock_max_opp[args[0]] = args[2];
			break;
	case POWER_PARAM_SET_POWER_OFF:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}
		apusys_opps.user_opp_index[args[0]] = APUSYS_MAX_NUM_OPPS - 1;
		for (j = 0 ; j < APUSYS_PATH_USER_NUM ; j++) {
			apusys_opps.user_path_volt[args[0]][j] =
							DVFS_VOLT_00_650000_V;
		}
		break;
	default:
		PWR_LOG_INF("unsupport the power parameter:%d\n", param);
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
		PWR_LOG_INF("copy_from_user failed, ret=%d\n", ret);
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
	else if (strcmp(token, "jtag") == 0)
		param = POWER_PARAM_JTAG;
	else if (strcmp(token, "lock") == 0)
		param = POWER_PARAM_LOCK;
	else if (strcmp(token, "volt_step") == 0)
		param = POWER_PARAM_VOLT_STEP;
	else if (strcmp(token, "power_hal") == 0)
		param = POWER_HAL_CTL;
	else if (strcmp(token, "eara") == 0)
		param = POWER_EARA_CTL;
	else if (strcmp(token, "user_opp") == 0)
		param = POWER_PARAM_SET_USER_OPP;
	else if (strcmp(token, "thermal_opp") == 0)
		param = POWER_PARAM_SET_THERMAL_OPP;
	else if (strcmp(token, "power_hal_opp") == 0)
		param = POWER_PARAM_SET_POWER_HAL_OPP;
	else if (strcmp(token, "power_off") == 0)
		param = POWER_PARAM_SET_POWER_OFF;
	else {
		ret = -EINVAL;
		PWR_LOG_INF("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			PWR_LOG_INF("fail to parse args[%d]\n", i);
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
	.release = seq_release,
	.write = apusys_debug_power_write,
};

struct dentry *apusys_power_dir;

void apusys_power_debugfs_init(void)
{
//	struct dentry *apusys_power_dir;
	int ret;

	PWR_LOG_INF("%s\n", __func__);

	apusys_power_dir = debugfs_create_dir("apusys", NULL);

	ret = IS_ERR_OR_NULL(apusys_power_dir);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		return;
	}

	debugfs_create_file("power", (0444),
		apusys_power_dir, NULL, &apusys_debug_power_fops);
}

void apusys_power_debugfs_exit(void)
{
	debugfs_remove(apusys_power_dir);
}

