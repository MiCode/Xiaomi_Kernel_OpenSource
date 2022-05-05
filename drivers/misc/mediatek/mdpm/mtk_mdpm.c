// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "mtk_pbm.h"
#include "mtk_mdpm.h"
#include "mtk_mdpm_platform.h"
#include "mtk_mdpm_platform_data.h"
#include "mtk_mdpm_platform_table.h"
#include "mtk_ccci_common.h"

static u32 cnted_share_mem[SHARE_MEM_SIZE];
static u32 rfhw_sel;
static int mt_mdpm_debug;
static int md_gen;

static struct md_power_status mdpm_power_sta;

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
static u32 *dbm_share_mem;
static bool md1_ccci_ready;
#endif

static int section_shift[DBM_SECTION_NUM] = {
	DBM_SECTION_1,  DBM_SECTION_2,  DBM_SECTION_3,
	DBM_SECTION_4,  DBM_SECTION_5,  DBM_SECTION_6,
	DBM_SECTION_7,  DBM_SECTION_8,  DBM_SECTION_9,
	DBM_SECTION_10, DBM_SECTION_11, DBM_SECTION_12};

static struct tx_power *mdpm_tx_pwr;
static struct mdpm_scenario *mdpm_scen;
static int *scen_priority;

enum mdpm_platform {
	MT6873_MDPM_DATA,
	MT6893_MDPM_DATA,
	MT6983_MDPM_DATA,
	MT6879_MDPM_DATA,
	MT6895_MDPM_DATA,
	MT6855_MDPM_DATA,
	MT6789_MDPM_DATA
};

void init_md_section_level(enum pbm_kicker kicker, u32 *share_mem)
{
	if (!mdpm_tx_pwr || !mdpm_scen || !scen_priority) {
		pr_notice("MD power table is empty\n");
		return;
	}
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	if (share_mem == NULL) {
		pr_info_ratelimited("can't get dbm share memory\n");
		return;
	}
#ifdef DBM_RESERVE_OFFSET
	share_mem += DBM_RESERVE_OFFSET;
#endif
	if ((md_gen == 6295) || (md_gen == 6293))
		share_mem -= DBM_GEN95_OFFSET;

	dbm_share_mem = share_mem;
	if (kicker == KR_MD1) {
		init_md1_section_level(dbm_share_mem);
		init_version_check(dbm_share_mem);
		md1_ccci_ready = 1;
	} else
		pr_notice("unknown MD kicker: %d\n", kicker);
#else
	return;
#endif
}
EXPORT_SYMBOL(init_md_section_level);

int get_md1_power(enum mdpm_power_type power_type, bool need_update)
{
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	u32 share_reg;
	enum md_scenario scenario;
	int scenario_power, tx_power;

	if (!mdpm_tx_pwr || !mdpm_scen || !scen_priority) {
		pr_notice("[md1_power] MD power table is empty\n");
		return 0;
	}

	if (power_type >= POWER_TYPE_NUM ||
		power_type < 0) {
		pr_notice("[md1_power] invalid power_type=%d\n",
			power_type);
		return 0;
	}

	if (need_update == false)
		return mdpm_power_sta.total_power;

	memset((void *)&mdpm_power_sta, 0, sizeof(struct md_power_status));

	if (!md1_ccci_ready) {
		pr_info_ratelimited("%s: md1_ccci_ready=%d\n", __func__, md1_ccci_ready);
		return MAX_MD1_POWER;
	}

	share_reg = get_md1_status_reg();
	if (dbm_share_mem == NULL)
		return MAX_MD1_POWER;

#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
	scenario = get_md1_scenario_by_shm(dbm_share_mem);
#else
	scenario = get_md1_scenario(share_reg, power_type);
#endif
	scenario_power = get_md1_scenario_power(scenario, power_type,
		&mdpm_power_sta);

	tx_power = get_md1_tx_power(scenario, dbm_share_mem, power_type,
		&mdpm_power_sta);

	if (mt_mdpm_debug)
		pr_info("[md1_power] scenario_power=%d tx_power=%d total=%d\n",
			scenario_power, tx_power, scenario_power + tx_power);

	return scenario_power + tx_power;
#else
	return 0;
#endif /* CONFIG_MTK_ECCCI_DRIVER */
}
EXPORT_SYMBOL(get_md1_power);

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
	if (copy_from_user(desc, buffer, len))
		return 0;

	len = (len < 0) ? 0 : len;
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
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,			\
	.proc_read		= seq_read,					\
	.proc_lseek		= seq_lseek,					\
	.proc_release		= single_release,				\
	.proc_write		= mt_ ## name ## _proc_write,			\
}

#define PROC_FOPS_RO(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,		\
	.proc_read		= seq_read,				\
	.proc_lseek		= seq_lseek,				\
	.proc_release	= single_release,			\
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
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(mdpm_debug),
		PROC_ENTRY(mdpm_power),
	};

	dir = proc_mkdir("mdpm", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/mdpm @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0660, dir, entries[i].fops))
			pr_notice("@%s: create /proc/mdpm/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}

static int get_md1_scenario_internal(u32 share_reg)
{
	int hit = -1;
	int i = 0;

#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
	for (i = 0; i < SCENARIO_NUM; i++) {
		if ((share_reg & mdpm_scen[scen_priority[i]].scenario_reg) ==
			mdpm_scen[scen_priority[i]].scenario_reg) {
			hit = scen_priority[i];
			break;
		}
	}

	if  (hit == -1)
		hit = S_STANDBY;
#else
	for (i = 0; i < SCENARIO_NUM; i++) {

		if (mdpm_scen[i].scenario_reg == share_reg) {
			hit = i;
			break;
		}
	}

	if  (hit == -1)
		pr_notice("[%s] ERROR, unknown scenario [%d]\n",
			__func__, share_reg);
#endif

	return hit;
}

static int get_shm_idx(enum tx_power_table tx_dbm, int sec_shift, bool get_dbm)
{
	int idx = 0, mem_idx = 0;

	idx = sec_shift / 32;

	if (idx >= DBM_TABLE_SIZE || idx < 0 || tx_dbm >= TX_DBM_NUM) {
		pr_notice("[%s] ERROR, exceed index %d %d %d\n",
			__func__, idx, sec_shift, tx_dbm);
			WARN_ON_ONCE(1);
		return mem_idx;
	}

	if (get_dbm) {
		/* get dbm index */
		mem_idx = mdpm_tx_pwr[tx_dbm].shm_dbm_idx[idx];
		if (mem_idx > DBM_TABLE_END
			|| mem_idx < DBM_TABLE_START) {
			pr_notice("[%s] ERROR, not in dbm table %d %d %d\n",
				__func__, idx, tx_dbm, mem_idx);
				WARN_ON_ONCE(1);
		}
	} else {
		/* get section index */
		mem_idx = mdpm_tx_pwr[tx_dbm].shm_sec_idx[idx];
		if (!(mem_idx <= SECTION_LEVEL_END
					&& mem_idx >= SECTION_LEVEL_START)
				&& !(mem_idx <= SECTION_LEVEL_2_END
					&& mem_idx >= SECTION_LEVEL_2_START)) {
			pr_notice("[%s] ERROR, not in section level %d %d %d\n",
				__func__, idx, tx_dbm, mem_idx);
				WARN_ON_ONCE(1);
		}
	}

	return mem_idx;
}

static int mdpm_shm_write(u32 *share_mem, enum share_mem_mapping mem_idx,
	u32 value, u32 mask, u32 shift)
{
	if (mem_idx >= SHARE_MEM_SIZE)
		return -EINVAL;

	share_mem[mem_idx] = (share_mem[mem_idx] & (~(mask << shift)))
		| ((value & mask) << shift);

	return 0;
}

static int mdpm_shm_read(u32 *share_mem, enum share_mem_mapping mem_idx,
	u32 *value, u32 mask, u32 shift)
{
	if (mem_idx >= SHARE_MEM_SIZE)
		return -EINVAL;

	*value = (share_mem[mem_idx] >> shift) & mask;

	return 0;
}

static u32 check_shm_version(u32 *share_mem)
{
	static u32 mdpm_version_check;

	if (mdpm_version_check == VERSION_INIT) {
		mdpm_shm_read(share_mem, M_VERSION_CHECK,
			&mdpm_version_check, VERSION_CHECK_VALID_MASK,
			VERSION_CHECK_VALID_SHIFT);
	}

	switch (mdpm_version_check) {
	case VERSION_INIT:
		if (mt_mdpm_debug)
			pr_info_ratelimited("mdpm share memory: MD not init\n");

		break;
	case VERSION_INVALID:
		pr_info("dpm share memory: MD check version ERROR\n");
		WARN_ON_ONCE(1);
		break;
	case VERSION_VALID:
	default:
		break;
	}

	return mdpm_version_check;
}

static u32 get_rfhw(u32 *share_mem)
{
	u32 rfhw_version;
	static u32 rfhw_check;
	static bool rfhw_updated;

	if (rfhw_updated == 1)
		return rfhw_sel;
	else if (rfhw_check == RF_HW_INIT) {
		mdpm_shm_read(share_mem, M_RF_HW, &rfhw_check,
			RF_HW_VALID_MASK, RF_HW_VALID_SHIFT);
	}

	if (rfhw_check == RF_HW_VALID) {
		mdpm_shm_read(share_mem, M_RF_HW, &rfhw_version,
				RF_HW_VERSION_MASK, RF_HW_VERSION_SHIFT);
		if (rfhw_version < RF_HW_NUM) {
			rfhw_updated = 1;
			return rfhw_version;
		} else if (1)
			pr_notice("wrong rfhw_version %d\n", rfhw_version);
	}

	return rfhw_sel;
}

void init_version_check(u32 *share_mem)
{
	mdpm_shm_write(share_mem, M_VERSION_CHECK, VERSION_INIT,
		VERSION_CHECK_VALID_MASK, VERSION_CHECK_VALID_SHIFT);
	mdpm_shm_write(share_mem, M_VERSION_CHECK, AP_MD_MDPM_VERSION,
		VERSION_CHECK_VERSION_MASK, VERSION_CHECK_VERSION_SHIFT);
}

unsigned int get_md1_status_reg(void)
{
#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
	return 0;
#else
	return vcorefs_get_md_scenario();
#endif
}

void init_md1_section_level(u32 *share_mem)
{
	u32 mem[SHARE_MEM_SIZE];
	int i, j, index, offset;

	memset(&mem[0], 0, sizeof(u32) * SHARE_MEM_SIZE);

	for (i = 0; i < DBM_SECTION_NUM; i++) {
		for (j = 0; j < TX_DBM_NUM; j++) {
			if (!mdpm_tx_pwr[j].rfhw)
				continue;
			if (mdpm_tx_pwr[j].rfhw->section[i] >
				DBM_SECTION_MASK) {
				pr_notice("[%s] md1_section_level too large i:%d s:%d !\n",
					__func__, j, i);
				WARN_ON_ONCE(1);
			}

			index = get_shm_idx(j, section_shift[i], false);
			offset = section_shift[i] % 32;
			mem[index] |= mdpm_tx_pwr[j].rfhw->section[i] << offset;
		}
	}

	memcpy(&share_mem[SECTION_LEVEL_START], &mem[SECTION_LEVEL_START],
		sizeof(u32) * (SECTION_LEVEL_END - SECTION_LEVEL_START + 1));

	memcpy(&share_mem[SECTION_LEVEL_2_START], &mem[SECTION_LEVEL_2_START],
		sizeof(u32) * (SECTION_LEVEL_2_END - SECTION_LEVEL_2_START + 1));

	pr_info("AP2MD1 section, 2G: 0x%08x%08x(0x%08x%08x), 3G: 0x%08x%08x(0x%08x %08x)\n",
		mem[M_2G_SECTION_LEVEL], mem[M_2G_SECTION_1_LEVEL],
		share_mem[M_2G_SECTION_LEVEL], share_mem[M_2G_SECTION_1_LEVEL],
		mem[M_3G_SECTION_LEVEL], mem[M_3G_SECTION_1_LEVEL],
		share_mem[M_3G_SECTION_LEVEL],
		share_mem[M_3G_SECTION_1_LEVEL]);
	pr_info("4G_CC0:0x%08x%08x(0x%08x%08x),4G_CC1:0x%08x%08x(0x%08x%08x)\n",
		mem[M_4G_SECTION_LEVEL], mem[M_4G_SECTION_9_LEVEL],
		share_mem[M_4G_SECTION_LEVEL], share_mem[M_4G_SECTION_9_LEVEL],
		mem[M_4G_SECTION_LEVEL], mem[M_4G_SECTION_9_LEVEL],
		share_mem[M_4G_SECTION_LEVEL],
		share_mem[M_4G_SECTION_9_LEVEL]);
	pr_info("3GTDD: 0x%08x%08x(0x%08x%08x)\n",
		mem[M_TDD_SECTION_LEVEL], mem[M_TDD_SECTION_1_LEVEL],
		share_mem[M_TDD_SECTION_LEVEL],
		share_mem[M_TDD_SECTION_1_LEVEL]);
	pr_info("C2K: 0x%08x%08x(0x%08x%08x), addr: 0x%p\n",
		mem[M_C2K_SECTION_1_LEVEL], mem[M_C2K_SECTION_2_LEVEL],
		share_mem[M_C2K_SECTION_1_LEVEL],
		share_mem[M_C2K_SECTION_2_LEVEL], share_mem);
	pr_info("NR_CC0:0x%08x%08x(0x%08x%08x),NR_CC1:0x%08x%08x(0x%08x%08x)\n",
		mem[M_NR_SECTION_LEVEL], mem[M_NR_SECTION_1_LEVEL],
		share_mem[M_NR_SECTION_LEVEL], share_mem[M_NR_SECTION_1_LEVEL],
		mem[M_NR_SECTION_2_LEVEL], mem[M_NR_SECTION_3_LEVEL],
		share_mem[M_NR_SECTION_2_LEVEL],
		share_mem[M_NR_SECTION_3_LEVEL]);
	pr_info("MMW_TX1:0x%08x%08x(0x%08x%08x),MMW_TX2:0x%08x%08x(0x%08x%08x)\n",
		mem[M_MMW_SECTION_LEVEL], mem[M_MMW_SECTION_1_LEVEL],
		share_mem[M_MMW_SECTION_LEVEL], share_mem[M_MMW_SECTION_1_LEVEL],
		mem[M_MMW_SECTION_2_LEVEL], mem[M_MMW_SECTION_3_LEVEL],
		share_mem[M_MMW_SECTION_2_LEVEL],
		share_mem[M_MMW_SECTION_3_LEVEL]);
}

#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
enum md_scenario get_md1_scenario_by_shm(u32 *share_mem)
{
	int scenario = S_STANDBY;
	u32 scen_status = -1;

	mdpm_shm_read(share_mem, M_MD_SCENARIO, &scen_status,
		MDPM_SHARE_MEMORY_MASK, MDPM_SHARE_MEMORY_SHIFT);

	scenario = get_md1_scenario_internal(scen_status);

	scenario = (scenario < 0) ? S_STANDBY : scenario;

	if (mt_mdpm_debug)
		pr_info("MD1 scenario: %d(%s), scen_status: 0x%x\n",
			scenario, mdpm_scen[scenario].scenario_name,
			scen_status);

	return scenario;
}
#else
enum md_scenario get_md1_scenario(u32 share_reg,
	enum mdpm_power_type power_type)
{
	int scenario = S_STANDBY;

	share_reg = share_reg & SHARE_REG_MASK;

	scenario = get_md1_scenario_internal(share_reg);

	scenario = (scenario < 0) ? S_STANDBY : scenario;

	if (mt_mdpm_debug)
		pr_info("MD1 scenario: %d(%s), reg: 0x%x\n",
			scenario, mdpm_scen[scenario].scenario_name,
			share_reg);

	return scenario;
}
#endif

int get_md1_scenario_power(enum md_scenario scenario,
	enum mdpm_power_type power_type, struct md_power_status *mdpm_pwr_sta)
{
	int s_power = 0, ret;

	switch (power_type) {
	case MAX_POWER:
		s_power = mdpm_scen[scenario].scenario_power->max;
		break;
	case AVG_POWER:
		s_power = mdpm_scen[scenario].scenario_power->avg;
		break;
	default:
		pr_notice("%s error power_type=%d\n", __func__, power_type);
		break;
	}
	mdpm_pwr_sta->scenario_id = scenario;
	ret = snprintf(mdpm_pwr_sta->scenario_name, sizeof(mdpm_pwr_sta->scenario_name),
			"%s", mdpm_scen[scenario].scenario_name);
	if (ret < 0)
		pr_info_ratelimited("%s:%d: copy scenario_name fail %d\n", __func__, __LINE__, ret);

	mdpm_pwr_sta->scanario_power = s_power;
	mdpm_pwr_sta->power_type = power_type;

	return s_power;
}

static int get_md1_tx_power_by_table(u32 *dbm_mem, u32 *old_dbm_mem,
	struct tx_power *tx_pwr, enum tx_power_table dbm_type,
	enum tx_rat_type rat, enum mdpm_power_type power_type,
	int *sec_shift, struct md_power_status *md_power_s)
{

	int pa_power = 0, rf_power = 0;
	int section, index, offset, i;
	bool cmp = true;

	if (dbm_type >= TX_DBM_NUM || power_type >= POWER_TYPE_NUM) {
		pr_notice("error argument dbm_type=%d power_type=%d\n",
			dbm_type, power_type);
		return 0;
	}

	if (tx_pwr == NULL) {
		pr_notice("no data for tx_power\n");
		return 0;
	}

	for (i  = 0; i < DBM_TABLE_SIZE; i++) {
		if (dbm_mem[tx_pwr->shm_dbm_idx[i]] !=
			old_dbm_mem[tx_pwr->shm_dbm_idx[i]]) {
			cmp = false;
			break;
		}
	}

	if (cmp) {
		if (mt_mdpm_debug == 2)
			pr_info("%s dBm no TX power, reg: 0x%08x%08x(0x%08x%08x) return 0\n",
			tx_pwr->dbm_name,
			dbm_mem[tx_pwr->shm_dbm_idx[0]],
			dbm_mem[tx_pwr->shm_dbm_idx[1]],
			old_dbm_mem[tx_pwr->shm_dbm_idx[0]],
			old_dbm_mem[tx_pwr->shm_dbm_idx[1]]);

		return 0;
	}

	for (section = 0; section < DBM_SECTION_NUM; section++) {
		index = get_shm_idx(dbm_type, sec_shift[section], true);
		offset = sec_shift[section] % 32;

		if (((dbm_mem[index] >> offset) & DBM_SECTION_MASK) !=
			((old_dbm_mem[index] >> offset) & DBM_SECTION_MASK)) {

			switch (power_type) {
			case MAX_POWER:
				pa_power = tx_pwr->rfhw->pa_power.max[section];
				rf_power = tx_pwr->rfhw->rf_power.max[section];
				break;
			case AVG_POWER:
				pa_power = tx_pwr->rfhw->pa_power.avg[section];
				rf_power = tx_pwr->rfhw->rf_power.avg[section];
				break;
			default:
				pr_notice("%s error power_type=%d\n",
					__func__, power_type);
				break;
			}

			if (mt_mdpm_debug)
				pr_info("%s dBm: reg:0x%08x%08x(0x%08x%08x),pa:%d,rf:%d,s:%d\n",
				tx_pwr->dbm_name, dbm_mem[tx_pwr->shm_dbm_idx[0]],
				dbm_mem[tx_pwr->shm_dbm_idx[1]],
				old_dbm_mem[tx_pwr->shm_dbm_idx[0]],
				old_dbm_mem[tx_pwr->shm_dbm_idx[1]],
				pa_power, rf_power, section+1);


			for (i	= 0; i < DBM_TABLE_SIZE; i++) {
				memcpy(
				&old_dbm_mem[tx_pwr->shm_dbm_idx[i]],
				&dbm_mem[tx_pwr->shm_dbm_idx[i]], sizeof(u32));
			}

			md_power_s->rat = rat;
			md_power_s->power_type = power_type;
			md_power_s->dbm_section = section + 1;
			md_power_s->pa_power += pa_power;
			md_power_s->rf_power += rf_power;
			md_power_s->tx_power += pa_power + rf_power;

			break;
		}
	}

	return pa_power + rf_power;

}

static int get_md1_tx_power_by_rat(u32 *dbm_mem, u32 *old_dbm_mem,
	enum tx_rat_type rat, enum mdpm_power_type power_type,
	struct md_power_status *md_power_s)
{
	int power;
	struct tx_power *tx_pwr = NULL;

	if (rat > RAT_NUM || rat <= 0 || power_type >= POWER_TYPE_NUM) {
		pr_notice("error argument rat_type=%d power_type=%d\n", rat,
			power_type);
		return 0;
	}

	switch (rat) {
	case RAT_2G:
		tx_pwr = &mdpm_tx_pwr[TX_2G_DBM];
		power = get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_2G_DBM, rat, power_type,
			section_shift, md_power_s);
		break;
	case RAT_3G:
		tx_pwr = &mdpm_tx_pwr[TX_3G_DBM];
		power = get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_3G_DBM, rat, power_type,
			section_shift, md_power_s);
		break;
	case RAT_4G:
		tx_pwr = &mdpm_tx_pwr[TX_4G_CC0_DBM];
		power = get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_4G_CC0_DBM, rat, power_type,
			section_shift, md_power_s);
		tx_pwr = &mdpm_tx_pwr[TX_4G_CC1_DBM];
		power = power + get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_4G_CC1_DBM, rat, power_type,
			section_shift, md_power_s);
		break;
	case RAT_5G:
		tx_pwr = &mdpm_tx_pwr[TX_NR_CC0_DBM];
		power = get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_NR_CC0_DBM, rat, power_type,
			section_shift, md_power_s);
		tx_pwr = &mdpm_tx_pwr[TX_NR_CC1_DBM];
		power = power + get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_NR_CC1_DBM, rat, power_type,
			section_shift, md_power_s);
		break;
	case RAT_MMW:
		tx_pwr = &mdpm_tx_pwr[TX_MMW_TX2_DBM];
		power = get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_MMW_TX2_DBM, rat, power_type,
			section_shift, md_power_s);
		if (!power) {
			tx_pwr = &mdpm_tx_pwr[TX_MMW_TX1_DBM];
			power = get_md1_tx_power_by_table(dbm_mem,
				old_dbm_mem, tx_pwr, TX_MMW_TX1_DBM, rat, power_type,
				section_shift, md_power_s);
		}
		break;
	case RAT_C2K:
		tx_pwr = &mdpm_tx_pwr[TX_C2K_DBM];
		power = get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_C2K_DBM, rat, power_type,
			section_shift, md_power_s);
		break;
	case RAT_3GTDD:
		tx_pwr = &mdpm_tx_pwr[TX_3GTDD_DBM];
		power = get_md1_tx_power_by_table(dbm_mem,
			old_dbm_mem, tx_pwr, TX_3GTDD_DBM, rat, power_type,
			section_shift, md_power_s);
		break;
	default:
		pr_notice("error argument rat_type=%d power_type=%d\n",
			rat, power_type);
		break;
	}

	return power;
}

int get_md1_tx_power(enum md_scenario scenario, u32 *share_mem,
	enum mdpm_power_type power_type,
	struct md_power_status *mdpm_pwr_sta)
{
	int i, rf_ret, tx_power, tx_power_max, usedBytes = 0;
	enum tx_rat_type rat;
	struct md_power_status mdpm_power_s_tmp;
	char log_buffer[128];

	if (share_mem == NULL) {
		if (mt_mdpm_debug)
			pr_info("MD1 share_mem is NULL\n");

		return 0;
	}

	if (check_shm_version(share_mem) == VERSION_VALID)
		rf_ret = get_rfhw(share_mem);
	else
		return 0;

	if (mt_mdpm_debug == 2)
		for (i = 0; i < SHARE_MEM_SIZE; i++) {
			usedBytes += sprintf(log_buffer + usedBytes, "0x%x ",
				share_mem[i]);

			if ((i + 1) % 10 == 0 || (i + 1) == SHARE_MEM_SIZE) {
				usedBytes = 0;
				pr_info("%s\n", log_buffer);
			}
		}

	memset((void *)&mdpm_power_s_tmp, 0, sizeof(struct md_power_status));
	tx_power_max = 0;

	for (i = 0; i < MAX_DBM_FUNC_NUM; i++) {
		rat = mdpm_scen[scenario].tx_power_rat[i];
		if (rat == 0)
			break;

		tx_power = get_md1_tx_power_by_rat(share_mem,
			cnted_share_mem, rat, power_type, &mdpm_power_s_tmp);

		if (mdpm_scen[scenario].tx_power_rat_sum == true) {
			/* select sum RAT Tx power */
			tx_power_max += tx_power;
			mdpm_pwr_sta->rat |= mdpm_power_s_tmp.rat;
			mdpm_pwr_sta->power_type = mdpm_power_s_tmp.power_type;

			mdpm_pwr_sta->dbm_section =
				mdpm_power_s_tmp.dbm_section;
			mdpm_pwr_sta->pa_power += mdpm_power_s_tmp.pa_power;
			mdpm_pwr_sta->rf_power += mdpm_power_s_tmp.rf_power;
			mdpm_pwr_sta->tx_power += mdpm_power_s_tmp.tx_power;
			mdpm_pwr_sta->total_power +=
				mdpm_pwr_sta->scanario_power +
				mdpm_pwr_sta->tx_power;
		} else {
			/* select largest RAT Tx power */
			if (tx_power > tx_power_max) {
				tx_power_max = tx_power;
				mdpm_pwr_sta->rat = mdpm_power_s_tmp.rat;
				mdpm_pwr_sta->power_type =
					mdpm_power_s_tmp.power_type;
				mdpm_pwr_sta->dbm_section =
					mdpm_power_s_tmp.dbm_section;
				mdpm_pwr_sta->pa_power =
					mdpm_power_s_tmp.pa_power;
				mdpm_pwr_sta->rf_power =
					mdpm_power_s_tmp.rf_power;
				mdpm_pwr_sta->tx_power =
					mdpm_power_s_tmp.tx_power;
				mdpm_pwr_sta->total_power =
					mdpm_pwr_sta->scanario_power +
					mdpm_pwr_sta->tx_power;
			}
		}
	}

	return tx_power_max;
}

static int mdpm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mdpm_data *mdpm_data;
	struct device_node *node = dev->of_node;

	mdpm_data = (struct mdpm_data *) of_device_get_match_data(dev);

	if (!mdpm_data) {
		dev_notice(dev, "Error: Failed to get mdpm platform data\n");
		return -ENODATA;
	}

	mdpm_data->dev = &pdev->dev;

	platform_set_drvdata(pdev, mdpm_data);

	mdpm_tx_pwr = mdpm_data->tx_power_t;
	mdpm_scen = mdpm_data->scenario_power_t;
	scen_priority = mdpm_data->prority_t;

	if (of_property_read_u32(node,
		"mediatek,md_generation", &md_gen) != 0)
		md_gen = 0;

	mt_mdpm_create_procfs();

	return 0;
}

static struct mdpm_data mt6873_mdpm_data = {
	.platform = MT6873_MDPM_DATA,
	.scenario_power_t = mt6873_mdpm_scen,
	.tx_power_t = mt6873_mdpm_tx_pwr,
	.prority_t = (void *)&mt6873_scen_priority
};

static struct mdpm_data mt6893_mdpm_data = {
	.platform = MT6893_MDPM_DATA,
	.scenario_power_t = mt6893_mdpm_scen,
	.tx_power_t = mt6893_mdpm_tx_pwr,
	.prority_t = (void *)&mt6893_scen_priority
};

static struct mdpm_data mt6983_mdpm_data = {
	.platform = MT6983_MDPM_DATA,
	.scenario_power_t = mt6983_mdpm_scen,
	.tx_power_t = mt6983_mdpm_tx_pwr,
	.prority_t = (void *)&mt6983_scen_priority
};

static struct mdpm_data mt6879_mdpm_data = {
	.platform = MT6879_MDPM_DATA,
	.scenario_power_t = mt6879_mdpm_scen,
	.tx_power_t = mt6879_mdpm_tx_pwr,
	.prority_t = (void *)&mt6879_scen_priority
};

static struct mdpm_data mt6895_mdpm_data = {
	.platform = MT6895_MDPM_DATA,
	.scenario_power_t = mt6895_mdpm_scen,
	.tx_power_t = mt6895_mdpm_tx_pwr,
	.prority_t = (void *)&mt6895_scen_priority
};

static struct mdpm_data mt6855_mdpm_data = {
	.platform = MT6855_MDPM_DATA,
	.scenario_power_t = mt6855_mdpm_scen,
	.tx_power_t = mt6855_mdpm_tx_pwr,
	.prority_t = (void *)&mt6855_scen_priority
};

static struct mdpm_data mt6789_mdpm_data = {
	.platform = MT6789_MDPM_DATA,
	.scenario_power_t = mt6789_mdpm_scen,
	.tx_power_t = mt6789_mdpm_tx_pwr,
	.prority_t = (void *)&mt6789_scen_priority
};

static const struct of_device_id mdpm_of_match[] = {
	{
		.compatible = "mediatek,mt6873-mdpm",
		.data = (void *)&mt6873_mdpm_data,
	},
	{
		.compatible = "mediatek,mt6893-mdpm",
		.data = (void *)&mt6893_mdpm_data,
	},
	{
		.compatible = "mediatek,mt6983-mdpm",
		.data = (void *)&mt6983_mdpm_data,
	},
	{
		.compatible = "mediatek,mt6879-mdpm",
		.data = (void *)&mt6879_mdpm_data,
	},
	{
		.compatible = "mediatek,mt6895-mdpm",
		.data = (void *)&mt6895_mdpm_data,
	},
	{
		.compatible = "mediatek,mt6855-mdpm",
		.data = (void *)&mt6855_mdpm_data,
	},
	{
		.compatible = "mediatek,mt6789-mdpm",
		.data = (void *)&mt6789_mdpm_data,
	},
	{
	},
};

MODULE_DEVICE_TABLE(of, mdpm_of_match);

static struct platform_driver mdpm_driver = {
	.driver = {
		.name = "mtk-modem_power_meter",
		.of_match_table = mdpm_of_match,
	},
	.probe = mdpm_probe,
};

module_platform_driver(mdpm_driver);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK modem power meter driver");
MODULE_LICENSE("GPL");
