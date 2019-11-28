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

#include "apusys_power_ctl.h"
#include "hal_config_power.h"
#include "apu_log.h"
#include "apusys_power_debug.h"
#include "apusys_power.h"
#include "apu_power_api.h"

static struct mutex power_fix_dvfs_mtx;


static int g_debug_option;
bool is_power_debug_lock;
int fixed_opp;

static void change_log_level(int new_level)
{
	g_pwr_log_level = new_level;
	PWR_LOG_INF("%s, new log level = %d\n", __func__, g_pwr_log_level);
}

static void apu_power_dump_opp_table(struct seq_file *s)
{
	int opp_num;
	int buck_domain;

	seq_printf(s,
		"|opp| vpu0| vpu1| vpu2|mdla0|mdla1| conn|iommu|ipuif|\n");
	seq_printf(s,
		"|---------------------------------------------------|\n");
	for (opp_num = 0 ; opp_num < APUSYS_MAX_NUM_OPPS ; opp_num++) {
		seq_printf(s, "| %d |", opp_num);
		for (buck_domain = 0 ; buck_domain < APUSYS_BUCK_DOMAIN_NUM;
			buck_domain++) {
			seq_printf(s, " %d |",
			apusys_opps.opps[opp_num][buck_domain].freq / 1000);
		}
		seq_printf(s,
			"\n|---------------------------------------------------|\n");
	}
}

static int apu_power_dump_curr_status(struct seq_file *s, int oneline_str)
{
	struct apu_power_info info = {0};

	info.id = 0;
	info.type = 1;

	hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);

	// for thermal request, we print vpu and mdla freq
	if (oneline_str) {
		seq_printf(s, "%03u,%03u,%03u,%03u,%03u\n",
			((info.rpc_intf_rdy >> 2) & 0x1) ? info.dsp1_freq : 0,
			((info.rpc_intf_rdy >> 3) & 0x1) ? info.dsp2_freq : 0,
			((info.rpc_intf_rdy >> 4) & 0x1) ? info.dsp3_freq : 0,
			((info.rpc_intf_rdy >> 6) & 0x1) ? info.dsp6_freq : 0,
			((info.rpc_intf_rdy >> 7) & 0x1) ? info.dsp6_freq : 0);

		return 0;
	}

	seq_printf(s,
		"|curr| vpu0| vpu1| vpu2|mdla0|mdla1| conn|iommu|vcore|\n| opp|");

	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_VPU0,
					info.dsp1_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_VPU1,
					info.dsp2_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_VPU2,
					info.dsp3_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_MDLA0,
					info.dsp6_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_MDLA1,
					info.dsp6_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_APU_CONN,
					info.dsp_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_TOP_IOMMU,
					info.dsp7_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_VCORE,
					info.ipuif_freq * info.dump_div));
	seq_puts(s, "\n");

	seq_printf(s,
		"|freq| %03u | %03u | %03u | %03u | %03u | %03u | %03u | %03u |\n",
		info.dsp1_freq, info.dsp2_freq, info.dsp3_freq,
		info.dsp6_freq, info.dsp6_freq, info.dsp_freq,
		info.dsp7_freq, info.ipuif_freq);

	seq_printf(s,
		"| clk| dsp1| dsp2| dsp3| dsp6| dsp6|  dsp| dsp7|ipuif|\n(unit: MHz)\n\n");

	seq_printf(s, "vvpu:%u(mV), vmdla:%u(mV), vcore:%u(mV), vsram:%u(mV)\n",
			info.vvpu, info.vmdla, info.vcore, info.vsram);

	seq_puts(s, "\n");
	seq_printf(s,
	"rpc_intf_rdy:0x%x, spm_wakeup:0x%x\nvcore_cg_con:0x%x, conn_cg_con:0x%x\nvpu0_cg_con:0x%x, vpu1_cg_con:0x%x, vpu2_cg_con:0x%x\nmdla0_cg_con:0x%x, mdla1_cg_con:0x%x\n",
		info.rpc_intf_rdy, info.spm_wakeup,
		info.vcore_cg_stat, info.conn_cg_stat,
		info.vpu0_cg_stat, info.vpu1_cg_stat, info.vpu2_cg_stat,
		info.mdla0_cg_stat, info.mdla1_cg_stat);

	seq_puts(s, "\n");
	return 0;
}

void fix_dvfs_debug(void)
{
	int i = 0;
	int opp = 0;

	mutex_lock(&power_fix_dvfs_mtx);

	for (i = VPU0; i < VPU0 + APUSYS_VPU_NUM; i++)
		apusys_opps.next_opp_index[i] = fixed_opp;

	for (i = MDLA0; i < MDLA0 + APUSYS_MDLA_NUM; i++)
		apusys_opps.next_opp_index[i] = fixed_opp;

	// determine vpu / mdla / vcore voltage
	apusys_opps.next_buck_volt[VPU_BUCK] =
		apusys_opps.opps[fixed_opp][V_VPU0].voltage;
	apusys_opps.next_buck_volt[MDLA_BUCK] =
		apusys_opps.opps[fixed_opp][V_MDLA0].voltage;

	#if VCORE_DVFS_SUPPORT
	apusys_opps.next_buck_volt[VCORE_BUCK] =
		apusys_opps.opps[fixed_opp][V_VCORE].voltage;
	#else
	if (apusys_opps.next_buck_volt[VPU_BUCK] ==
		DVFS_VOLT_00_800000_V)
		apusys_opps.next_buck_volt[VCORE_BUCK] =
			DVFS_VOLT_00_600000_V;
	else
		apusys_opps.next_buck_volt[VCORE_BUCK] =
			DVFS_VOLT_00_575000_V;
	#endif

	// determine buck domain opp
	for (i = 0; i < APUSYS_BUCK_DOMAIN_NUM; i++) {
		if (dvfs_power_domain_support(i) == false)
			continue;
		for (opp = 0; opp < APUSYS_MAX_NUM_OPPS; opp++) {
			if ((i == V_APU_CONN ||	i == V_TOP_IOMMU) &&
				(apusys_opps.opps[opp][i].voltage ==
				apusys_opps.next_buck_volt[VPU_BUCK])) {
				apusys_opps.next_opp_index[i] = opp;
				break;
			} else if (i == V_VCORE &&
			apusys_opps.opps[opp][i].voltage ==
			apusys_opps.next_buck_volt[VCORE_BUCK]) {
				apusys_opps.next_opp_index[i] = opp;
				break;
			}
		}
	}

	is_power_debug_lock = true;
	apusys_dvfs_policy(0);

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
		PWR_LOG_INF("lock opp=%d\n", (int)(args[0]));

		fixed_opp = args[0];

		fix_dvfs_debug();

		break;

	case POWER_HAL_CTL:
	{
		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
			"invalid argument, expected:1, received:%d\n",
								argc);
			goto out;
		}

		if (args[0] < 0 || args[0] >= APUSYS_DVFS_USER_NUM) {
			PWR_LOG_INF("user(%d) is invalid\n",
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

		if ((args[0] == VPU0 || args[0] == VPU1 || args[0] == VPU2)
			&& APUSYS_VPU_NUM != 0) {
			for (i = VPU0; i < VPU0 + APUSYS_VPU_NUM; i++) {
				apusys_opps.power_lock_max_opp[i] =
					apusys_boost_value_to_opp(i, args[1]);
				apusys_opps.power_lock_min_opp[i] =
					apusys_boost_value_to_opp(i, args[2]);
			}
		}

		if ((args[0] == MDLA0 || args[0] == MDLA1)
			&& APUSYS_MDLA_NUM != 0) {
			for (i = MDLA0; i < MDLA0 + APUSYS_MDLA_NUM; i++) {
				apusys_opps.power_lock_max_opp[i] =
					apusys_boost_value_to_opp(i, args[1]);
				apusys_opps.power_lock_min_opp[i] =
					apusys_boost_value_to_opp(i, args[2]);
			}
		}
		apusys_dvfs_policy(0);
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
			PWR_LOG_INF(
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
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
				argc);
			goto out;
		}
		g_debug_option = POWER_PARAM_OPP_TABLE;
		break;
	case POWER_PARAM_CURR_STATUS:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
				"invalid argument, expected:1, received:%d\n",
				argc);
			goto out;
		}
		g_debug_option = POWER_PARAM_CURR_STATUS;
		break;
	case POWER_PARAM_LOG_LEVEL:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			PWR_LOG_INF(
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

	mutex_init(&power_fix_dvfs_mtx);

	apusys_power_dir = debugfs_create_dir("apusys", NULL);

	ret = IS_ERR_OR_NULL(apusys_power_dir);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		return;
	}

	debugfs_create_file("power", (0744),
		apusys_power_dir, NULL, &apusys_debug_power_fops);
}

void apusys_power_debugfs_exit(void)
{
	debugfs_remove(apusys_power_dir);
}

