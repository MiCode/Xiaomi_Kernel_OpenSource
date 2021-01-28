// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "mdla_debug.h"
#include "mdla.h"
#include "mdla_hw_reg.h"
#include "mdla_trace.h"
#include "mdla_dvfs.h"
#include "mdla_pmu.h"

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/io.h>
/* FIXME: No such file */
//#include <m4u.h>

#define ALGO_OF_MAX_POWER  (3)

/* global variables */
int g_mdla_log_level = 1;
unsigned int g_mdla_func_mask;

static int mdla_log_level_set(void *data, u64 val)
{
	g_mdla_log_level = val & 0xf;
	LOG_INF("g_mdla_log_level: %d\n", g_mdla_log_level);

	return 0;
}

static int mdla_log_level_get(void *data, u64 *val)
{
	*val = g_mdla_log_level;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mdla_debug_log_level_fops, mdla_log_level_get,
				mdla_log_level_set, "%llu\n");

static int mdla_func_mask_set(void *data, u64 val)
{
	g_mdla_func_mask = val & 0xffffffff;
	LOG_INF("g_func_mask: 0x%x\n", g_mdla_func_mask);

	return 0;
}

static int mdla_func_mask_get(void *data, u64 *val)
{
	*val = g_mdla_func_mask;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mdla_debug_func_mask_fops, mdla_func_mask_get,
				mdla_func_mask_set, "%llu\n");


#define IMPLEMENT_MDLA_DEBUGFS(name)					\
static int mdla_debug_## name ##_show(struct seq_file *s, void *unused)\
{					\
	mdla_dump_## name(s);		\
	return 0;			\
}					\
static int mdla_debug_## name ##_open(struct inode *inode, struct file *file) \
{					\
	return single_open(file, mdla_debug_ ## name ## _show, \
				inode->i_private); \
}                                                                             \
static const struct file_operations mdla_debug_ ## name ## _fops = {   \
	.open = mdla_debug_ ## name ## _open,                               \
	.read = seq_read,                                                    \
	.llseek = seq_lseek,                                                \
	.release = seq_release,                                             \
}


IMPLEMENT_MDLA_DEBUGFS(register);
IMPLEMENT_MDLA_DEBUGFS(opp_table);



#undef IMPLEMENT_MDLA_DEBUGFS

static int mdla_debug_power_show(struct seq_file *s, void *unused)
{
	mdla_dump_power(s);
	return 0;
}

static int mdla_debug_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdla_debug_power_show, inode->i_private);
}

static ssize_t mdla_debug_power_write(struct file *flip,
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
		LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		goto out;
	}

	tmp[count] = '\0';

	cursor = tmp;

	/* parse a command */
	token = strsep(&cursor, " ");
	if (strcmp(token, "fix_opp") == 0)
		param = MDLA_POWER_PARAM_FIX_OPP;
	else if (strcmp(token, "dvfs_debug") == 0)
		param = MDLA_POWER_PARAM_DVFS_DEBUG;
	else if (strcmp(token, "jtag") == 0)
		param = MDLA_POWER_PARAM_JTAG;
	else if (strcmp(token, "lock") == 0)
		param = MDLA_POWER_PARAM_LOCK;
	else if (strcmp(token, "volt_step") == 0)
		param = MDLA_POWER_PARAM_VOLT_STEP;
	else if (strcmp(token, "power_hal") == 0)
		param = MDLA_POWER_HAL_CTL;
	else if (strcmp(token, "eara") == 0)
		param = MDLA_EARA_CTL;
	else {
		ret = -EINVAL;
		LOG_ERR("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			LOG_ERR("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	mdla_set_power_parameter(param, i, args);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations mdla_debug_power_fops = {
	.open = mdla_debug_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = mdla_debug_power_write,
};

static int mdla_debug_prof_show(struct seq_file *s, void *unused)
{
	mdla_dump_prof(s);
	return 0;
}

static int mdla_debug_prof_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdla_debug_prof_show, inode->i_private);
}

static ssize_t mdla_debug_prof_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	pmu_reset_saved_counter();
	return count;
}

static const struct file_operations mdla_debug_prof_fops = {
	.open = mdla_debug_prof_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = mdla_debug_prof_write,
};


#define DEFINE_MDLA_DEBUGFS(name)  \
	struct dentry *mdla_d##name

	DEFINE_MDLA_DEBUGFS(root);
	DEFINE_MDLA_DEBUGFS(timeout);
	DEFINE_MDLA_DEBUGFS(e1_detect_timeout);
	DEFINE_MDLA_DEBUGFS(e1_detect_count);
	DEFINE_MDLA_DEBUGFS(poweroff_time);
	DEFINE_MDLA_DEBUGFS(klog);
	DEFINE_MDLA_DEBUGFS(register);
	DEFINE_MDLA_DEBUGFS(opp_table);
	DEFINE_MDLA_DEBUGFS(power);

	DEFINE_MDLA_DEBUGFS(eng0);
	DEFINE_MDLA_DEBUGFS(eng1);
	DEFINE_MDLA_DEBUGFS(eng2);
	DEFINE_MDLA_DEBUGFS(eng11);


	DEFINE_MDLA_DEBUGFS(power);

	DEFINE_MDLA_DEBUGFS(prof_start);
	DEFINE_MDLA_DEBUGFS(prof);
	DEFINE_MDLA_DEBUGFS(period);
	DEFINE_MDLA_DEBUGFS(cmd_trace);
	DEFINE_MDLA_DEBUGFS(op_trace);
	DEFINE_MDLA_DEBUGFS(c1);
	DEFINE_MDLA_DEBUGFS(c2);
	DEFINE_MDLA_DEBUGFS(c3);
	DEFINE_MDLA_DEBUGFS(c4);
	DEFINE_MDLA_DEBUGFS(c5);
	DEFINE_MDLA_DEBUGFS(c6);
	DEFINE_MDLA_DEBUGFS(c7);
	DEFINE_MDLA_DEBUGFS(c8);
	DEFINE_MDLA_DEBUGFS(c9);
	DEFINE_MDLA_DEBUGFS(c10);
	DEFINE_MDLA_DEBUGFS(c11);
	DEFINE_MDLA_DEBUGFS(c12);
	DEFINE_MDLA_DEBUGFS(c13);
	DEFINE_MDLA_DEBUGFS(c14);
	DEFINE_MDLA_DEBUGFS(c15);

u32 mdla_klog;

void mdla_debugfs_init(void)
{
	int ret;

	mdla_klog = 0x40; /* print timeout info by default */

	mdla_droot = debugfs_create_dir("mdla", NULL);

	ret = IS_ERR_OR_NULL(mdla_droot);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		return;
	}

	mdla_dtimeout = debugfs_create_u32("timeout", 0660, mdla_droot,
		&mdla_timeout);
	mdla_de1_detect_timeout = debugfs_create_u32("e1_detect_timeout",
			0660, mdla_droot, &mdla_e1_detect_timeout);
	mdla_de1_detect_timeout = debugfs_create_u32("e1_detect_count",
			0440, mdla_droot, &mdla_e1_detect_count);
	mdla_dpoweroff_time = debugfs_create_u32("poweroff_time",
		0660, mdla_droot, &mdla_poweroff_time);
	mdla_dklog = debugfs_create_u32("klog", 0660, mdla_droot,
		&mdla_klog);


	mdla_deng0 = debugfs_create_u32("eng0",
				0660, mdla_droot, &cfg_eng0);
	mdla_deng1 = debugfs_create_u32("eng1",
				0660, mdla_droot, &cfg_eng1);
	mdla_deng2 = debugfs_create_u32("eng2",
				0660, mdla_droot, &cfg_eng2);
	mdla_deng11 = debugfs_create_u32("eng11",
				0660, mdla_droot, &cfg_eng11);

	/* Prof */
	mdla_dprof_start = debugfs_create_u32("prof_start",
			0660, mdla_droot, &cfg_timer_en);

	mdla_dc1 = debugfs_create_u32("c1",	0660,
			mdla_droot, &cfg_pmu_event[0]);
	mdla_dc2 = debugfs_create_u32("c2",	0660,
			mdla_droot, &cfg_pmu_event[1]);
	mdla_dc3 = debugfs_create_u32("c3",	0660,
			mdla_droot, &cfg_pmu_event[2]);
	mdla_dc4 = debugfs_create_u32("c4",	0660,
			mdla_droot, &cfg_pmu_event[3]);
	mdla_dc5 = debugfs_create_u32("c5",	0660,
			mdla_droot, &cfg_pmu_event[4]);
	mdla_dc6 = debugfs_create_u32("c6",	0660,
			mdla_droot, &cfg_pmu_event[5]);
	mdla_dc7 = debugfs_create_u32("c7",	0660,
			mdla_droot, &cfg_pmu_event[6]);
	mdla_dc8 = debugfs_create_u32("c8",	0660,
			mdla_droot, &cfg_pmu_event[7]);
	mdla_dc9 = debugfs_create_u32("c9",	0660,
			mdla_droot, &cfg_pmu_event[8]);
	mdla_dc10 = debugfs_create_u32("c10",	0660,
			mdla_droot, &cfg_pmu_event[9]);
	mdla_dc11 = debugfs_create_u32("c11",
			0660, mdla_droot, &cfg_pmu_event[10]);
	mdla_dc12 = debugfs_create_u32("c12",
			0660, mdla_droot, &cfg_pmu_event[11]);
	mdla_dc13 = debugfs_create_u32("c13",
			0660, mdla_droot, &cfg_pmu_event[12]);
	mdla_dc14 = debugfs_create_u32("c14",
			0660, mdla_droot, &cfg_pmu_event[13]);
	mdla_dc15 = debugfs_create_u32("c15",
			0660, mdla_droot, &cfg_pmu_event[14]);


	mdla_dperiod = debugfs_create_u64("period",
			0660, mdla_droot, &cfg_period);

	mdla_dcmd_trace = debugfs_create_u32("cmd_trace",
			0660, mdla_droot, &cfg_cmd_trace);

	mdla_dop_trace = debugfs_create_u32("op_trace",
			0660, mdla_droot, &cfg_op_trace);

#define CREATE_MDLA_DEBUGFS(name)                         \
	{                                                           \
		mdla_d##name = debugfs_create_file(#name, 0644, \
				mdla_droot,         \
				NULL, &mdla_debug_ ## name ## _fops);       \
		if (IS_ERR_OR_NULL(mdla_d##name))                          \
			LOG_ERR("failed to create debug file[" #name "].\n"); \
	}

	CREATE_MDLA_DEBUGFS(register);
	CREATE_MDLA_DEBUGFS(opp_table);
	CREATE_MDLA_DEBUGFS(power);
	CREATE_MDLA_DEBUGFS(prof);

#undef CREATE_MDLA_DEBUGFS
	cfg_eng0 = 0x0;
	cfg_eng1 = 0x0;
	cfg_eng2 = 0x0;
	cfg_eng11 = 0x600;
}

void mdla_debugfs_exit(void)
{
#define REMOVE_MDLA_DEBUGFS(name) \
	debugfs_remove(mdla_d##name)

	REMOVE_MDLA_DEBUGFS(timeout);
	REMOVE_MDLA_DEBUGFS(poweroff_time);
	REMOVE_MDLA_DEBUGFS(klog);
	REMOVE_MDLA_DEBUGFS(register);
	REMOVE_MDLA_DEBUGFS(opp_table);
	REMOVE_MDLA_DEBUGFS(power);
	REMOVE_MDLA_DEBUGFS(prof_start);
	REMOVE_MDLA_DEBUGFS(prof);
	REMOVE_MDLA_DEBUGFS(period);
	REMOVE_MDLA_DEBUGFS(cmd_trace);
	REMOVE_MDLA_DEBUGFS(op_trace);

	REMOVE_MDLA_DEBUGFS(eng0);
	REMOVE_MDLA_DEBUGFS(eng1);
	REMOVE_MDLA_DEBUGFS(eng2);
	REMOVE_MDLA_DEBUGFS(eng11);

	REMOVE_MDLA_DEBUGFS(c1);
	REMOVE_MDLA_DEBUGFS(c2);
	REMOVE_MDLA_DEBUGFS(c3);
	REMOVE_MDLA_DEBUGFS(c4);
	REMOVE_MDLA_DEBUGFS(c5);
	REMOVE_MDLA_DEBUGFS(c6);
	REMOVE_MDLA_DEBUGFS(c7);
	REMOVE_MDLA_DEBUGFS(c8);
	REMOVE_MDLA_DEBUGFS(c9);
	REMOVE_MDLA_DEBUGFS(c10);
	REMOVE_MDLA_DEBUGFS(c11);
	REMOVE_MDLA_DEBUGFS(c12);
	REMOVE_MDLA_DEBUGFS(c13);
	REMOVE_MDLA_DEBUGFS(c14);
	REMOVE_MDLA_DEBUGFS(c15);
	REMOVE_MDLA_DEBUGFS(root);
}

#define dump_reg_top(name) \
	mdla_timeout_debug("%s: %.8x\n", #name, mdla_reg_read(name))

#define dump_reg_cfg(name) \
	mdla_timeout_debug("%s: %.8x\n", #name, mdla_cfg_read(name))

void dump_timeout_debug_info(void)
{
	u32 mreg_top_g_idle, c2r_exe_st, ste_debug_if_1;
	int i;

	mreg_top_g_idle = mdla_reg_read(MREG_TOP_G_IDLE);
	c2r_exe_st = mdla_reg_read(0x0000);
	ste_debug_if_1 = mdla_reg_read(0x0EA8);
	if (((ste_debug_if_1&0x1C0) != 0x0 && (ste_debug_if_1&0x3) == 0x3)) {
		mdla_timeout_debug(
				"Matched, %s, mdla_timeout:%d, mreg_top_g_idle: %08x, c2r_exe_st: %08x, ste_debug_if_1: %08x\n",
				__func__, mdla_timeout,
				mreg_top_g_idle, c2r_exe_st, ste_debug_if_1);
	} else {
		mdla_timeout_debug(
				"Not match, %s, mdla_timeout:%d, mreg_top_g_idle: %08x, c2r_exe_st: %08x, ste_debug_if_1: %08x\n",
				__func__, mdla_timeout,
				mreg_top_g_idle, c2r_exe_st, ste_debug_if_1);
	}

	mdla_timeout_debug("0x19000148: %08X\n",
			ioread32(apu_conn_top + 0x148));
	mdla_timeout_debug("0x19000150: %08X\n",
			ioread32(apu_conn_top + 0x150));
	mdla_timeout_debug("0x1902006C: %08X\n",
			ioread32(apu_conn_top + 0x2006C));
	mdla_timeout_debug("0x19020070: %08X\n",
			ioread32(apu_conn_top + 0x20070));

	for (i = 0x0000; i < 0x1000; i += 4)
		mdla_timeout_debug("0x1938%04X: %08X\n", i, mdla_cfg_read(i));
	for (i = 0x0000; i < 0x1000; i += 4)
		mdla_timeout_debug("0x1939%04X: %08X\n", i, mdla_reg_read(i));
}
void mdla_dump_reg(void)
{
	mdla_timeout_debug("mdla_timeout\n");
	// TODO: too many registers, dump only debug required ones.
	dump_reg_cfg(MDLA_CG_CON);
	dump_reg_cfg(MDLA_SW_RST);
	dump_reg_cfg(MDLA_MBIST_MODE0);
	dump_reg_cfg(MDLA_MBIST_MODE1);
	dump_reg_cfg(MDLA_MBIST_CTL);
	dump_reg_cfg(MDLA_RP_OK0);
	dump_reg_cfg(MDLA_RP_OK1);
	dump_reg_cfg(MDLA_RP_OK2);
	dump_reg_cfg(MDLA_RP_OK3);
	dump_reg_cfg(MDLA_RP_FAIL0);
	dump_reg_cfg(MDLA_RP_FAIL1);
	dump_reg_cfg(MDLA_RP_FAIL2);
	dump_reg_cfg(MDLA_RP_FAIL3);
	dump_reg_cfg(MDLA_MBIST_FAIL0);
	dump_reg_cfg(MDLA_MBIST_FAIL1);
	dump_reg_cfg(MDLA_MBIST_FAIL2);
	dump_reg_cfg(MDLA_MBIST_FAIL3);
	dump_reg_cfg(MDLA_MBIST_FAIL4);
	dump_reg_cfg(MDLA_MBIST_FAIL5);
	dump_reg_cfg(MDLA_MBIST_DONE0);
	dump_reg_cfg(MDLA_MBIST_DONE1);
	dump_reg_cfg(MDLA_MBIST_DEFAULT_DELSEL);
	dump_reg_cfg(MDLA_SRAM_DELSEL0);
	dump_reg_cfg(MDLA_SRAM_DELSEL1);
	dump_reg_cfg(MDLA_RP_RST);
	dump_reg_cfg(MDLA_RP_CON);
	dump_reg_cfg(MDLA_RP_PRE_FUSE);
	dump_reg_cfg(MDLA_AXI_CTRL);
	dump_reg_cfg(MDLA_AXI1_CTRL);

	dump_reg_top(MREG_TOP_G_REV);
	dump_reg_top(MREG_TOP_G_INTP0);
	dump_reg_top(MREG_TOP_G_INTP1);
	dump_reg_top(MREG_TOP_G_INTP2);
	dump_reg_top(MREG_TOP_G_CDMA0);
	dump_reg_top(MREG_TOP_G_CDMA1);
	dump_reg_top(MREG_TOP_G_CDMA2);
	dump_reg_top(MREG_TOP_G_CDMA3);
	dump_reg_top(MREG_TOP_G_CDMA4);
	dump_reg_top(MREG_TOP_G_CDMA5);
	dump_reg_top(MREG_TOP_G_CDMA6);
	dump_reg_top(MREG_TOP_G_CUR0);
	dump_reg_top(MREG_TOP_G_CUR1);
	dump_reg_top(MREG_TOP_G_FIN0);
	dump_reg_top(MREG_TOP_G_FIN1);
	dump_reg_top(MREG_TOP_G_IDLE);

	/* for DCM and CG */
	dump_reg_top(MREG_TOP_ENG0);
	dump_reg_top(MREG_TOP_ENG1);
	dump_reg_top(MREG_TOP_ENG2);
	dump_reg_top(MREG_TOP_ENG11);
	dump_timeout_debug_info();
}

void mdla_dump_buf(int mask, void *kva, int group, u32 size)
{
	if (mdla_klog & mask) {
		print_hex_dump_debug("mdla-buf: ",
			DUMP_PREFIX_OFFSET, 32, group, kva, size, 0);
	}
}

void mdla_dump_ce(struct command_entry *ce)
{
	int i;
	char *ptr;

	if (ce == NULL)
		return;
	if (!(mdla_klog & MDLA_DBG_TIMEOUT))
		return;
	if (ce->kva == NULL)
		return;

	ptr = (char *)ce->kva;


	for (i = 0; i < ce->count; i++) {
		mdla_timeout_debug("mdla-buf: %s: cmd_id: %u, cmd[%d/%d], kva=%p\n",
			__func__, ce->id, i, ce->count, ptr);
		mdla_dump_buf(MDLA_DBG_TIMEOUT, ptr, 4, MREG_CMD_SIZE);
		ptr += MREG_CMD_SIZE;
	}
}

int mdla_dump_register(struct seq_file *s)
{
#define seq_reg_top(name) \
	seq_printf(s, "%s: %.8x\n", #name, mdla_reg_read(name))

#define seq_reg_cfg(name) \
	seq_printf(s, "%s: %.8x\n", #name, mdla_cfg_read(name))

	seq_reg_cfg(MDLA_CG_CON);
	seq_reg_cfg(MDLA_SW_RST);
	seq_reg_cfg(MDLA_MBIST_MODE0);
	seq_reg_cfg(MDLA_MBIST_MODE1);
	seq_reg_cfg(MDLA_MBIST_CTL);
	seq_reg_cfg(MDLA_RP_OK0);
	seq_reg_cfg(MDLA_RP_OK1);
	seq_reg_cfg(MDLA_RP_OK2);
	seq_reg_cfg(MDLA_RP_OK3);
	seq_reg_cfg(MDLA_RP_FAIL0);
	seq_reg_cfg(MDLA_RP_FAIL1);
	seq_reg_cfg(MDLA_RP_FAIL2);
	seq_reg_cfg(MDLA_RP_FAIL3);
	seq_reg_cfg(MDLA_MBIST_FAIL0);
	seq_reg_cfg(MDLA_MBIST_FAIL1);
	seq_reg_cfg(MDLA_MBIST_FAIL2);
	seq_reg_cfg(MDLA_MBIST_FAIL3);
	seq_reg_cfg(MDLA_MBIST_FAIL4);
	seq_reg_cfg(MDLA_MBIST_FAIL5);
	seq_reg_cfg(MDLA_MBIST_DONE0);
	seq_reg_cfg(MDLA_MBIST_DONE1);
	seq_reg_cfg(MDLA_MBIST_DEFAULT_DELSEL);
	seq_reg_cfg(MDLA_SRAM_DELSEL0);
	seq_reg_cfg(MDLA_SRAM_DELSEL1);
	seq_reg_cfg(MDLA_RP_RST);
	seq_reg_cfg(MDLA_RP_CON);
	seq_reg_cfg(MDLA_RP_PRE_FUSE);
	seq_reg_cfg(MDLA_AXI_CTRL);
	seq_reg_cfg(MDLA_AXI1_CTRL);

	seq_reg_top(MREG_TOP_G_REV);
	seq_reg_top(MREG_TOP_G_INTP0);
	seq_reg_top(MREG_TOP_G_INTP1);
	seq_reg_top(MREG_TOP_G_INTP2);
	seq_reg_top(MREG_TOP_G_CDMA0);
	seq_reg_top(MREG_TOP_G_CDMA1);
	seq_reg_top(MREG_TOP_G_CDMA2);
	seq_reg_top(MREG_TOP_G_CDMA3);
	seq_reg_top(MREG_TOP_G_CDMA4);
	seq_reg_top(MREG_TOP_G_CDMA5);
	seq_reg_top(MREG_TOP_G_CDMA6);
	seq_reg_top(MREG_TOP_G_CUR0);
	seq_reg_top(MREG_TOP_G_CUR1);
	seq_reg_top(MREG_TOP_G_FIN0);
	seq_reg_top(MREG_TOP_G_FIN1);

	return 0;
}

