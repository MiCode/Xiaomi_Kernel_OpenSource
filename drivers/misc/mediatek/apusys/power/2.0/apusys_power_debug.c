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
#include <linux/mutex.h>

#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/dcache.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/proc_fs.h>

#include "apusys_power_ctl.h"
#include "hal_config_power.h"
#include "apu_log.h"
#include "apusys_power_debug.h"
#include "apusys_power.h"
#include "apu_power_api.h"
#include "apu_platform_debug.h"
#include "apusys_power_rule_check.h"


static struct mutex power_fix_dvfs_mtx;
static int g_debug_option;
bool is_power_debug_lock;
int fixed_opp;

int apu_power_power_stress(int type, int device, int opp)
{
	int id = 0;

	LOG_WRN("%s begin with type %d +++\n", __func__, type);

	if (type < 0 || type >= 10) {
		LOG_ERR("%s err with type = %d\n", __func__, type);
		return -1;
	}

	if (device != 9 && (device < 0 || device >= APUSYS_POWER_USER_NUM)) {
		LOG_ERR("%s err with device = %d\n", __func__, device);
		return -1;
	}

	if (type != 5 && (opp < 0 || opp >= APUSYS_MAX_NUM_OPPS)) {
		LOG_ERR("%s err with opp = %d\n", __func__, opp);
		return -1;
	}

	switch (type) {
	case 0: // config opp
		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_DVFS_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;
				apu_device_set_opp(id, opp);
			}
		} else {
			apu_device_set_opp(device, opp);
		}

		udelay(100);
		break;

	case 1: // config power on
		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_POWER_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;

				apu_device_power_on(id);
			}
		} else {
			apu_device_power_on(device);
		}
		break;

	case 2: // config power off
		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_POWER_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;

				apu_device_power_off(id);
			}
		} else {
			apu_device_power_off(device);
		}
		break;

	case 4: // power driver debug func
#if defined(CONFIG_MACH_MT6893) || defined(CONFIG_MACH_MT6877)
		// device: binning test
		// opp: raise test
		opp = ((device & 0xF) << 8) | opp;
#endif
		hal_config_power(PWR_CMD_DEBUG_FUNC, VPU0, &opp);
		break;

	case 5: // dvfs all combination test , opp = run count
		constraints_check_stress(opp);
		break;

	case 6:
		if (opp <= 3) {
		#if SUPPORT_VCORE_TO_IPUIF
			apu_qos_set_vcore(g_ipuif_opp_table[opp].ipuif_vcore);
		#endif
		}
		break;

	case 7: // power on/off suspend stress
		if (power_on_off_stress == 0)
			power_on_off_stress = 1;
		else
			power_on_off_stress = 0;
		break;

	case 8: // dump power info and options
		LOG_WRN("%s, BYPASS_POWER_OFF : %d\n",
				__func__, BYPASS_POWER_OFF);
		LOG_WRN("%s, BYPASS_POWER_CTL : %d\n",
				__func__, BYPASS_POWER_CTL);
		LOG_WRN("%s, BYPASS_DVFS_CTL : %d\n",
				__func__, BYPASS_DVFS_CTL);
		LOG_WRN("%s, DEFAULT_POWER_ON : %d\n",
				__func__, DEFAULT_POWER_ON);
		LOG_WRN("%s, AUTO_BUCK_OFF_SUSPEND : %d\n",
				__func__, AUTO_BUCK_OFF_SUSPEND);
		LOG_WRN("%s, AUTO_BUCK_OFF_DEEPIDLE : %d\n",
				__func__, AUTO_BUCK_OFF_DEEPIDLE);
		LOG_WRN("%s, VCORE_DVFS_SUPPORT : %d\n",
				__func__, VCORE_DVFS_SUPPORT);
		LOG_WRN("%s, ASSERTION_PERCENTAGE : %d\n",
				__func__, ASSERTION_PERCENTAGE);
#ifdef AGING_MARGIN
		LOG_WRN("%s, AGING_MARGIN : %d\n",
				__func__, AGING_MARGIN);
#endif
		LOG_WRN("%s, BINNING_VOLTAGE_SUPPORT : %d\n",
				__func__, BINNING_VOLTAGE_SUPPORT);
#ifdef CCF_SET_RATE
		LOG_WRN("%s, CCF_SET_RATE : %d\n",
			__func__, CCF_SET_RATE);
#endif
		LOG_WRN("%s, g_pwr_log_level : %d\n",
				__func__, g_pwr_log_level);
		LOG_WRN("%s, power_on_off_stress : %d\n",
				__func__, power_on_off_stress);
		LOG_WRN("%s, is_power_debug_lock : %d, fixed_opp: %d\n",
				__func__, is_power_debug_lock, fixed_opp);
		apu_get_power_info(0);
		break;

	default:
		LOG_WRN("%s invalid type %d !\n", __func__, type);
	}

	LOG_WRN("%s end with type %d ---\n", __func__, type);
	return 0;
}

static void change_log_level(int new_level)
{
	g_pwr_log_level = new_level;
	PWR_LOG_INF("%s, new log level = %d\n", __func__, g_pwr_log_level);
}

void fix_dvfs_debug(void)
{
	enum DVFS_VOLTAGE_DOMAIN i;
	enum DVFS_BUCK buck;

	mutex_lock(&power_fix_dvfs_mtx);
	for (i = 0; i < APUSYS_BUCK_DOMAIN_NUM; i++) {
		if (dvfs_power_domain_support(i) == false)
			continue;
		apusys_opps.next_opp_index[i] = fixed_opp;
		buck = apusys_buck_domain_to_buck[i];
		apusys_opps.next_buck_volt[buck] =
				max(apusys_opps.opps[fixed_opp][i].voltage,
				    apusys_opps.next_buck_volt[buck]);
	}
	is_power_debug_lock = true;
	apusys_dvfs_policy(0);

#if SUPPORT_VCORE_TO_IPUIF
	apusys_opps.qos_apu_vcore =
		apusys_opps.opps[fixed_opp][V_VCORE].voltage;
	apusys_ipuif_opp_change();
#endif

	mutex_unlock(&power_fix_dvfs_mtx);
}

static int apusys_debug_power_show(struct seq_file *s, void *unused)
{
	switch (g_debug_option) {
	case POWER_PARAM_OPP_TABLE:
		apu_power_dump_opp_table(s);
		break;
	case POWER_PARAM_CURR_STATUS:
		apu_power_dump_curr_status(s, 0);
		break;
	case POWER_PARAM_LOG_LEVEL:
		seq_printf(s, "g_pwr_log_level = %d\n", g_pwr_log_level);
		break;
	default:
		apu_power_dump_curr_status(s, 1); // one line string
	}

	return 0;
}

static int apusys_debug_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_debug_power_show, inode->i_private);
}

static int apusys_set_power_parameter(uint8_t param, int argc, int *args)
{
	int ret = 0;

	switch (param) {
	case POWER_PARAM_FIX_OPP:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_ERR(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}
		switch (args[0]) {
		case 0:
			is_power_debug_lock = false;

#if SUPPORT_VCORE_TO_IPUIF
			apusys_opps.qos_apu_vcore =
				VCORE_SHUTDOWN_VOLT;
#endif
			break;
		case 1:
			is_power_debug_lock = true;
			break;
		default:

			if (ret) {
				PWR_LOG_ERR("invalid argument, received:%d\n",
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
			PWR_LOG_ERR(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}

		ret = args[0] >= APUSYS_MAX_NUM_OPPS;
		if (ret) {
			PWR_LOG_ERR("opp (%d) is out-of-bound, max opp:%d\n",
				    (int)(args[0]), APUSYS_MAX_NUM_OPPS - 1);
			goto out;
		}
		PWR_LOG_INF("@@test%d\n", argc);
		PWR_LOG_INF("lock opp=%d\n", (int)(args[0]));

		fixed_opp = args[0];
		fix_dvfs_debug();

		break;

	case POWER_HAL_CTL:
	{
		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_ERR(
			"invalid argument, expected:1, received:%d\n",
								argc);
			goto out;
		}

		if (args[0] < 0 || args[0] >= APUSYS_DVFS_USER_NUM) {
			PWR_LOG_ERR("user(%d) is invalid\n",
					(int)(args[0]));
			goto out;
		}
		if (args[1] > 100 || args[1] < 0) {
			PWR_LOG_ERR("min boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		if (args[2] > 100 || args[2] < 0) {
			PWR_LOG_ERR("max boost(%d) is out-of-bound\n",
					(int)(args[2]));
			goto out;
		}

		/* setting max/min opp of user, args[0] */
		apusys_opps.power_lock_max_opp[args[0]] =
			apusys_boost_value_to_opp(args[0], args[1]);
		apusys_opps.power_lock_min_opp[args[0]] =
			apusys_boost_value_to_opp(args[0], args[2]);

		apusys_dvfs_policy(0);
		break;
	}

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
				PWR_LOG_ERR(
				"invalid argument, expected:1, received:%d\n",
									argc);
				goto out;
			}
			/* make sure args[0], DVFS_USER, within the range */
			if (args[0] < 0 || args[0] >= APUSYS_DVFS_USER_NUM) {
				PWR_LOG_ERR("user(%d) is invalid\n",
						(int)(args[0]));
				goto out;
			}

			/* make sure args[1], OPP, within the range */
			ret = (args[1] >= APUSYS_MAX_NUM_OPPS);
			if (ret) {
				PWR_LOG_ERR("opp-%d is too big, max opp:%d\n",
					    (int)(args[0]),
					    APUSYS_MAX_NUM_OPPS - 1);
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

			/* make sure args[0], DVFS_USER, within the range */
			if (args[0] < 0 || args[0] >= APUSYS_DVFS_USER_NUM) {
				PWR_LOG_ERR("user(%d) is invalid\n",
						(int)(args[0]));
				goto out;
			}

			/*
			 * Make sure args[1], min of power lock
			 * within the range
			 */
			ret = (args[1] >= APUSYS_MAX_NUM_OPPS);
			if (ret) {
				PWR_LOG_ERR("opp-%d is too big, max opp:%d\n",
					    (int)(args[1]),
					    APUSYS_MAX_NUM_OPPS - 1);
				goto out;
			}

			/*
			 * Make sure args[2], max of power lock
			 * within the range
			 */
			ret = (args[1] >= APUSYS_MAX_NUM_OPPS);
			if (ret) {
				PWR_LOG_ERR("opp-%d is too big, max opp:%d\n",
					    (int)(args[1]),
					    APUSYS_MAX_NUM_OPPS - 1);
				goto out;
			}

			apusys_opps.power_lock_min_opp[args[0]] = args[1];
			apusys_opps.power_lock_max_opp[args[0]] = args[2];
			break;
	case POWER_PARAM_GET_POWER_REG:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
									argc);
			goto out;
		}
		apu_power_reg_dump();
		break;
	case POWER_PARAM_POWER_STRESS:
		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_ERR(
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
	case POWER_PARAM_OPP_TABLE:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_ERR(
				"invalid argument, expected:1, received:%d\n",
				argc);
			goto out;
		}
		g_debug_option = POWER_PARAM_OPP_TABLE;
		break;
	case POWER_PARAM_CURR_STATUS:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_ERR(
				"invalid argument, expected:1, received:%d\n",
				argc);
			goto out;
		}
		g_debug_option = POWER_PARAM_CURR_STATUS;
		break;
	case POWER_PARAM_LOG_LEVEL:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_ERR(
				"invalid argument, expected:1, received:%d\n",
				argc);
			goto out;
		}
		if (args[0] == 9)
			g_debug_option = POWER_PARAM_LOG_LEVEL;
		else
			change_log_level(args[0]);
		break;
	default:
		PWR_LOG_ERR("unsupport the power parameter:%d\n", param);
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

static int apusys_power_fail_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_power_fail_show, inode->i_private);
}

static ssize_t apusys_power_fail_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	return 0;
}

static const struct file_operations apusys_power_fail_fops = {
	.open = apusys_power_fail_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = apusys_power_fail_write,
};

static const struct file_operations apusys_debug_power_fops = {
	.open = apusys_debug_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = apusys_debug_power_write,
};

struct dentry *apusys_power_dir;

void apusys_power_debugfs_init(void)
{
//	struct dentry *apusys_power_dir;
	int ret;

	PWR_LOG_INF("%s\n", __func__);

	mutex_init(&power_fix_dvfs_mtx);

	apusys_power_dir = debugfs_create_dir("apusys", NULL);

	ret = IS_ERR_OR_NULL(apusys_power_dir);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		return;
	}

	debugfs_create_file("power", (0644),
		apusys_power_dir, NULL, &apusys_debug_power_fops);
	debugfs_create_file("power_dump_fail_log", (0644),
		apusys_power_dir, NULL, &apusys_power_fail_fops);
}

void apusys_power_debugfs_exit(void)
{
	debugfs_remove(apusys_power_dir);
}

#if defined(CONFIG_MACH_MT6877)
static int mt_apupwr_minOPP_proc_show(struct seq_file *m, void *v)
{
	if (is_power_debug_lock)
		seq_puts(m, "apupwr minOPP enabled\n");
	else
		seq_puts(m, "apupwr minOPP disabled\n");

	seq_printf(m, "fixed_opp: %d\n",
		fixed_opp);

	return 0;
}

static ssize_t mt_apupwr_minOPP_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	int minOPP = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &minOPP) == 0) {
		if (minOPP == 0)
			is_power_debug_lock = false;
		else if (minOPP == 1) {
			fixed_opp = APUSYS_MAX_NUM_OPPS - 1;
			fix_dvfs_debug();
		} else
			pr_notice("should be [0:disable,1:enable]\n");
	} else
		pr_notice("should be [0:disable,1:enable]\n");

	return count;
}

#define PROC_FOPS_RW(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct file_operations mt_ ## name ## _proc_fops = {	\
	.owner		= THIS_MODULE,					\
	.open		= mt_ ## name ## _proc_open,			\
	.read		= seq_read,					\
	.llseek		= seq_lseek,					\
	.release	= single_release,				\
	.write		= mt_ ## name ## _proc_write,			\
}

#define PROC_FOPS_RO(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct file_operations mt_ ## name ## _proc_fops = {	\
	.owner		= THIS_MODULE,				\
	.open		= mt_ ## name ## _proc_open,		\
	.read		= seq_read,				\
	.llseek		= seq_lseek,				\
	.release	= single_release,			\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}

PROC_FOPS_RW(apupwr_minOPP);

int apusys_power_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(apupwr_minOPP),
	};

	dir = proc_mkdir("apupwr", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/apupwr @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0664, dir, entries[i].fops))
			pr_notice("@%s: create /proc/apupwr/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}
#else
int apusys_power_create_procfs(void)
{
	return 0;
}
#endif

