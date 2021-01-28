// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <mach/mtk_pbm.h>
#include <mtk_mdpm_common.h>
#include <mtk_mdpm_platform.h>

#if MD_POWER_METER_ENABLE
#include "mtk_vcore_dvfs.h"
#include "mtk_ccci_common.h"
#endif

int mt_mdpm_debug;
struct md_power_status mdpm_power_sta;

#ifdef MD_POWER_UT
u32 fake_share_reg;
u32 fake_share_mem[SHARE_MEM_SIZE];
#endif

#if MD_POWER_METER_ENABLE
static bool md1_ccci_ready;
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[MDPM] " fmt

#define _BIT_(_bit_)		(unsigned int)(1 << (_bit_))
#define _BITMASK_(_bits_)	\
(((unsigned int)-1>>(31-((1)?_bits_)))&~((1U<<((0)?_bits_))-1))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#if MD_POWER_METER_ENABLE
void init_md_section_level(enum pbm_kicker kicker)
{
	u32 *share_mem = NULL;
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	share_mem =
		(u32 *)get_smem_start_addr(MD_SYS1, SMEM_USER_RAW_DBM, NULL);
	if (share_mem == NULL) {
		pr_info_ratelimited("can't get dbm share memory\n");
		return;
	}
#else
	return;
#endif

	if (kicker == KR_MD1) {
		init_md1_section_level(share_mem);
		init_version_check(share_mem);
		md1_ccci_ready = 1;
	} else
		pr_notice("unknown MD kicker: %d\n", kicker);
}

int get_md1_power(enum mdpm_power_type power_type, bool need_update)
{
#if defined(MD_POWER_UT) || !defined(MD_SCEANRIO_USE_SHARE_MEMORY)
	u32 share_reg;
#endif
	u32 *share_mem;
	enum md_scenario scenario;
	int scenario_power, tx_power;

#if !defined(CONFIG_MTK_ECCCI_DRIVER)
	return 0;
#endif

	if (power_type >= POWER_TYPE_NUM ||
		power_type < 0) {
		pr_notice("[md1_power] invalid power_type=%d\n",
			power_type);
		return 0;
	}

	if (need_update == false)
		return mdpm_power_sta.total_power;

	memset((void *)&mdpm_power_sta, 0, sizeof(struct md_power_status));

#ifdef MD_POWER_UT
	share_mem = fake_share_mem;
	share_reg = fake_share_reg;
#else
	if (!md1_ccci_ready)
		return MAX_MD1_POWER;

	share_reg = get_md1_status_reg();
	share_mem = (u32 *)get_smem_start_addr(MD_SYS1, SMEM_USER_RAW_DBM,
		NULL);
	if (share_mem == NULL) {
		pr_info_ratelimited("can't get dbm share memory\n");
		return MAX_MD1_POWER;
	}
#endif

#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
	scenario = get_md1_scenario_by_shm(share_mem);
#else
	scenario = get_md1_scenario(share_reg, power_type);
#endif
	scenario_power = get_md1_scenario_power(scenario, power_type,
		&mdpm_power_sta);

	tx_power = get_md1_tx_power(scenario, share_mem, power_type,
		&mdpm_power_sta);

	if (mt_mdpm_debug)
		pr_info("[md1_power] scenario_power=%d tx_power=%d total=%d\n",
			scenario_power, tx_power, scenario_power + tx_power);

	return scenario_power + tx_power;
}

static int mt_mdpm_debug_proc_show(struct seq_file *m, void *v)
{
	if (mt_mdpm_debug)
		seq_printf(m, "mdpm debug enabled mt_mdpm_debug=%d\n",
			mt_mdpm_debug);
	else
		seq_puts(m, "mdpm debug disabled\n");

	return 0;
}

/*
 * enable debug message
 */
static ssize_t mt_mdpm_debug_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	len = (len < 0) ? 0 : len;
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	/* if (sscanf(desc, "%d", &debug) == 1) { */
	if (kstrtoint(desc, 10, &debug) == 0) {
		if (debug >= 0 && debug <= 2)
			mt_mdpm_debug = debug;
		else
			pr_notice("should be [0:disable, 1,2:enable level]\n");
	} else
		pr_notice("should be [0:disable, 1,2:enable level]\n");

	return count;
}

static int mt_mdpm_power_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "MAX power: scenario=%dmW dbm=%dmW total=%dmW\n scenario name=%s(%d) section=%d, rat=%d, power_type=%d\n",
		mdpm_power_sta.scanario_power, mdpm_power_sta.tx_power,
		mdpm_power_sta.total_power,
		mdpm_power_sta.scenario_name, mdpm_power_sta.scenario_id,
		mdpm_power_sta.dbm_section, mdpm_power_sta.rat,
		mdpm_power_sta.power_type);

	return 0;
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

PROC_FOPS_RW(mdpm_debug);
PROC_FOPS_RO(mdpm_power);

static int mt_mdpm_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(mdpm_debug),
		PROC_ENTRY(mdpm_power),
	};

	dir = proc_mkdir("mdpm", NULL);

	if (!dir) {
		pr_info("fail to create /proc/mdpm @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0664, dir, entries[i].fops))
			pr_info("@%s: create /proc/mdpm/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}

#else /* MD_POWER_METER_ENABLE */
void init_md_section_level(enum pbm_kicker kicker)
{
	pr_notice("MD_POWER_METER_ENABLE:0\n");
}

int get_md1_power(enum mdpm_power_type power_type, bool need_update)
{
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	return MAX_MD1_POWER;
#else
	return 0;
#endif
}
#endif /* MD_POWER_METER_ENABLE */

static int __init mdpm_module_init(void)
{
	int ret = 0;

#if MD_POWER_METER_ENABLE
	mt_mdpm_create_procfs();
#ifdef MD_POWER_UT
	mt_mdpm_debug = 1;
	init_md1_section_level(fake_share_mem);
	init_version_check(fake_share_mem);
	md_power_meter_ut();
#endif
#endif /* MD_POWER_METER_ENABLE */
	return ret;
}

static void __exit mdpm_module_exit(void)
{

}

module_init(mdpm_module_init);
module_exit(mdpm_module_exit);

MODULE_DESCRIPTION("MDPM Driver v0.1");
