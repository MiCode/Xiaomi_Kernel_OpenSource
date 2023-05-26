/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <mach/mtk_pbm.h>
#include <mtk_mdpm_platform.h>
#include <mtk_mdpm_common.h>
#if MD_POWER_METER_ENABLE
#include <mtk_mdpm_platform_data.h>
#endif
#include <helio-dvfsrc.h>

#if MD_POWER_METER_ENABLE
char log_buffer[128];
int usedBytes;
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[MDPM] " fmt

#if MD_POWER_METER_ENABLE

static u32 cnted_share_mem[SHARE_MEM_SIZE];
static u32 rfhw_sel;

static int section_shift[DBM_SECTION_NUM] = {
	DBM_SECTION_1,  DBM_SECTION_2,  DBM_SECTION_3,
	DBM_SECTION_4,  DBM_SECTION_5,  DBM_SECTION_6,
	DBM_SECTION_7,  DBM_SECTION_8,  DBM_SECTION_9,
	DBM_SECTION_10, DBM_SECTION_11, DBM_SECTION_12};

static struct tx_power mdpm_tx_pwr[TX_DBM_NUM] = {
	[TX_2G_DBM] = {
		.dbm_name = "2G",
		.shm_dbm_idx = {M_2G_DBM_TABLE, M_2G_DBM_1_TABLE},
		.shm_sec_idx = {M_2G_SECTION_LEVEL, M_2G_SECTION_1_LEVEL},
		.rfhw = &rfhw0[TX_2G_DBM],
	},

	[TX_3G_DBM] = {
		.dbm_name = "3G",
		.shm_dbm_idx = {M_3G_DBM_TABLE, M_3G_DBM_1_TABLE},
		.shm_sec_idx = {M_3G_SECTION_LEVEL, M_3G_SECTION_1_LEVEL},
		.rfhw = &rfhw0[TX_3G_DBM],
	},

	[TX_3GTDD_DBM] = {
		.dbm_name = "3GTDD",
		.shm_dbm_idx = {M_TDD_DBM_TABLE, M_TDD_DBM_1_TABLE},
		.shm_sec_idx = {M_TDD_SECTION_LEVEL, M_TDD_SECTION_1_LEVEL},
		.rfhw = &rfhw0[TX_3GTDD_DBM],
	},

	[TX_4G_CC0_DBM] = {
		.dbm_name = "4G_CC0",
		.shm_dbm_idx = {M_4G_DBM_TABLE, M_4G_DBM_2_TABLE},
		.shm_sec_idx = {M_4G_SECTION_LEVEL, M_4G_SECTION_9_LEVEL},
		.rfhw = &rfhw0[TX_4G_CC0_DBM],
	},

	[TX_4G_CC1_DBM] = {
		.dbm_name = "4G_CC1",
		.shm_dbm_idx = {M_4G_DBM_1_TABLE, M_4G_DBM_3_TABLE},
		.shm_sec_idx = {M_4G_SECTION_LEVEL, M_4G_SECTION_9_LEVEL},
		.rfhw = &rfhw0[TX_4G_CC1_DBM],
	},

	[TX_C2K_DBM] = {
		.dbm_name = "C2K",
		.shm_dbm_idx = {M_C2K_DBM_1_TABLE, M_C2K_DBM_2_TABLE},
		.shm_sec_idx = {M_C2K_SECTION_1_LEVEL, M_C2K_SECTION_2_LEVEL},
		.rfhw = &rfhw0[TX_C2K_DBM],
	}
};

static int get_md1_tx_power_by_rat(u32 *dbm_mem, u32 *old_dbm_mem,
	enum tx_rat_type rat, enum mdpm_power_type power_type,
	struct md_power_status *md_power_s);

static struct mdpm_scenario mdpm_scen[SCENARIO_NUM] = {
	[S_STANDBY] = {
		.scenario_reg = 0,
		.scenario_name = "S_STANDBY",
		.scenario_power = &md_scen_power[S_STANDBY],
		.tx_power_rat = {0, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_2G_NON_CONN] = {
		.scenario_reg = 1,
		.scenario_name = "S_2G_NON_CONN",
		.scenario_power = &md_scen_power[S_2G_NON_CONN],
		.tx_power_rat = {RAT_2G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_2G_CONN] = {
		.scenario_reg = 2,
		.scenario_name = "S_2G_CONN",
		.scenario_power = &md_scen_power[S_2G_CONN],
		.tx_power_rat = {RAT_2G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_3G_TDD_PAGING] = {
		.scenario_reg = 4,
		.scenario_name = "S_3G_TDD_PAGING",
		.scenario_power = &md_scen_power[S_3G_TDD_PAGING],
		.tx_power_rat = {RAT_3GTDD, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_3G_TDD_TALKING] = {
		.scenario_reg = 5,
		.scenario_name = "S_3G_TDD_TALKING",
		.scenario_power = &md_scen_power[S_3G_TDD_TALKING],
		.tx_power_rat = {RAT_3GTDD, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_3G_TDD_DATALINK] = {
		.scenario_reg = 6,
		.scenario_name = "S_3G_TDD_DATALINK",
		.scenario_power = &md_scen_power[S_3G_TDD_DATALINK],
		.tx_power_rat = {RAT_3GTDD, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_C2K_PAGING] = {
		.scenario_reg = 8,
		.scenario_name = "S_C2K_PAGING",
		.scenario_power = &md_scen_power[S_C2K_PAGING],
		.tx_power_rat = {RAT_C2K, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_C2K_EVDO] = {
		.scenario_reg = 9,
		.scenario_name = "S_C2K_EVDO",
		.scenario_power = &md_scen_power[S_C2K_EVDO],
		.tx_power_rat = {RAT_C2K, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_C2K_SHDR] = {
		.scenario_reg = 10,
		.scenario_name = "S_C2K_SHDR",
		.scenario_power = &md_scen_power[S_C2K_SHDR],
		.tx_power_rat = {RAT_C2K, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_C2K_1X] = {
		.scenario_reg = 11,
		.scenario_name = "S_C2K_1X",
		.scenario_power = &md_scen_power[S_C2K_1X],
		.tx_power_rat = {RAT_C2K, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_3G_UL1_TALKING] = {
		.scenario_reg = 12,
		.scenario_name = "S_3G_UL1_TALKING",
		.scenario_power = &md_scen_power[S_3G_UL1_TALKING],
		.tx_power_rat = {RAT_3G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_3G_UL1_PAGING] = {
		.scenario_reg = 13,
		.scenario_name = "S_3G_UL1_PAGING",
		.scenario_power = &md_scen_power[S_3G_UL1_PAGING],
		.tx_power_rat = {RAT_3G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_3G_UL1_DATA_1C] = {
		.scenario_reg = 14,
		.scenario_name = "S_3G_UL1_DATA_1C",
		.scenario_power = &md_scen_power[S_3G_UL1_DATA_1C],
		.tx_power_rat = {RAT_3G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_3G_UL1_DATA_2C] = {
		.scenario_reg = 15,
		.scenario_name = "S_3G_UL1_DATA_2C",
		.scenario_power = &md_scen_power[S_3G_UL1_DATA_2C],
		.tx_power_rat = {RAT_3G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_0D0U] = {
		.scenario_reg = 16,
		.scenario_name = "S_4G_0D0U",
		.scenario_power = &md_scen_power[S_4G_0D0U],
		.tx_power_rat = {RAT_4G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_1CC] = {
		.scenario_reg = 17,
		.scenario_name = "S_4G_1CC",
		.scenario_power = &md_scen_power[S_4G_1CC],
		.tx_power_rat = {RAT_4G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_2CC] = {
		.scenario_reg = 18,
		.scenario_name = "S_4G_2CC",
		.scenario_power = &md_scen_power[S_4G_2CC],
		.tx_power_rat = {RAT_4G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_3D1U] = {
		.scenario_reg = 19,
		.scenario_name = "S_4G_3D1U",
		.scenario_power = &md_scen_power[S_4G_3D1U],
		.tx_power_rat = {RAT_4G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_3D2U] = {
		.scenario_reg = 20,
		.scenario_name = "S_4G_3D2U",
		.scenario_power = &md_scen_power[S_4G_3D2U],
		.tx_power_rat = {RAT_4G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_4D1U] = {
		.scenario_reg = 21,
		.scenario_name = "S_4G_4D1U",
		.scenario_power = &md_scen_power[S_4G_4D1U],
		.tx_power_rat = {RAT_4G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_4D2U] = {
		.scenario_reg = 22,
		.scenario_name = "S_4G_4D2U",
		.scenario_power = &md_scen_power[S_4G_4D2U],
		.tx_power_rat = {RAT_4G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_4D3U] = {
		.scenario_reg = 23,
		.scenario_name = "S_4G_4D3U",
		.scenario_power = &md_scen_power[S_4G_4D3U],
		.tx_power_rat = {RAT_4G, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},

	[S_4G_POS] = {
		.scenario_reg = 31,
		.scenario_name = "S_4G_POS",
		.scenario_power = &md_scen_power[S_4G_POS],
		.tx_power_rat = {0, 0, 0, 0, 0},
		.tx_power_func = get_md1_tx_power_by_rat,
	},
};

#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
static int scen_priority[SCENARIO_NUM] = {
	S_4G_4D3U,
	S_4G_4D2U,
	S_4G_4D1U,
	S_4G_3D2U,
	S_4G_3D1U,
	S_4G_2CC,
	S_4G_1CC,
	S_3G_UL1_DATA_2C,
	S_3G_UL1_DATA_1C,
	S_3G_TDD_DATALINK,
	S_C2K_SHDR,
	S_C2K_EVDO,
	S_3G_UL1_TALKING,
	S_3G_TDD_TALKING,
	S_C2K_PAGING,
	S_C2K_1X,
	S_2G_CONN,
	S_4G_0D0U,
	S_3G_UL1_PAGING,
	S_3G_TDD_PAGING,
	S_2G_NON_CONN,
	S_STANDBY,
	S_4G_POS,
};
#endif

static int get_md1_scenario_internal(u32 share_reg)
{
	int hit = -1;
	int i = 0;

#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
	for (i = 0; i < SCENARIO_NUM; i++) {
		if (share_reg &
		(0x1 << mdpm_scen[scen_priority[i]].scenario_reg)) {
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

	if (idx >= DBM_TABLE_SIZE || idx < 0 || tx_dbm >= TX_DBM_NUM
		|| tx_dbm < 0) {
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
		if (mem_idx > SECTION_LEVEL_END
			|| mem_idx < SECTION_LEVEL_START) {
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
	if (mem_idx < 0 || mem_idx >= SHARE_MEM_SIZE)
		return -EINVAL;

	share_mem[mem_idx] = (share_mem[mem_idx] & (~(mask << shift)))
		| ((value & mask) << shift);

	return 0;
}

static int mdpm_shm_read(u32 *share_mem, enum share_mem_mapping mem_idx,
	u32 *value, u32 mask, u32 shift)
{
	if (mem_idx < 0 || mem_idx >= SHARE_MEM_SIZE)
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
		if (rfhw_version >= 0 && rfhw_version < RF_HW_NUM) {
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
	return vcorefs_get_md_scenario();
}

void init_md1_section_level(u32 *share_mem)
{
	u32 mem[SHARE_MEM_SIZE];
	int i, j, index, offset;

	memset(&mem[0], 0, sizeof(u32) * SHARE_MEM_SIZE);

	for (i = 0; i < DBM_SECTION_NUM; i++) {
		for (j = 0; j < TX_DBM_NUM; j++) {
			if (mdpm_tx_pwr[j].rfhw->section[i] >
				DBM_SECTION_MASK) {
				pr_notice("[%s] md1_section_level too large i:%d s:%d !\n",
					__func__, j, i);
				WARN_ON_ONCE(1);
			}

			index = get_shm_idx(j, section_shift[i],
				false);
			offset = section_shift[i] % 32;

			mem[index] |=
			mdpm_tx_pwr[j].rfhw->section[i] << offset;
		}
	}

	memcpy(&share_mem[SECTION_LEVEL_START], &mem[SECTION_LEVEL_START],
		sizeof(u32) * (SECTION_LEVEL_END - SECTION_LEVEL_START + 1));

	pr_info("AP2MD1 section, 2G: 0x%08x%08x(0x%08x%08x), 3G: 0x%08x%08x(0x%08x %08x)\n",
		mem[M_2G_SECTION_LEVEL], mem[M_2G_SECTION_1_LEVEL],
		share_mem[M_2G_SECTION_LEVEL], share_mem[M_2G_SECTION_1_LEVEL],
		mem[M_3G_SECTION_LEVEL], mem[M_3G_SECTION_1_LEVEL],
		share_mem[M_3G_SECTION_LEVEL],
		share_mem[M_3G_SECTION_1_LEVEL]);
	pr_info("4G_upL1:0x%08x%08x(0x%08x%08x),4G_upL2:0x%08x%08x(0x%08x%08x)\n",
		mem[M_4G_SECTION_LEVEL], mem[M_4G_SECTION_1_LEVEL],
		share_mem[M_4G_SECTION_LEVEL], share_mem[M_4G_SECTION_1_LEVEL],
		mem[M_4G_SECTION_LEVEL], mem[M_4G_SECTION_1_LEVEL],
		share_mem[M_4G_SECTION_LEVEL],
		share_mem[M_4G_SECTION_1_LEVEL]);
	pr_info("3GTDD: 0x%08x%08x(0x%08x%08x)\n",
		mem[M_TDD_SECTION_LEVEL], mem[M_TDD_SECTION_1_LEVEL],
		share_mem[M_TDD_SECTION_LEVEL],
		share_mem[M_TDD_SECTION_1_LEVEL]);
	pr_info("C2K: 0x%08x%08x(0x%08x%08x), addr: 0x%p\n",
		mem[M_C2K_SECTION_1_LEVEL], mem[M_C2K_SECTION_2_LEVEL],
		share_mem[M_C2K_SECTION_1_LEVEL],
		share_mem[M_C2K_SECTION_2_LEVEL], share_mem);
}

enum md_scenario get_md1_scenario(u32 share_reg,
	enum mdpm_power_type power_type)
{
	int scenario = S_STANDBY;

	share_reg = share_reg & SHARE_REG_MASK;

	scenario = get_md1_scenario_internal(share_reg);

	scenario = (scenario < 0) ? S_STANDBY : scenario;

	if (mt_mdpm_debug && scenario >= 0)
		pr_info("MD1 scenario: %d(%s), reg: 0x%x\n",
			scenario, mdpm_scen[scenario].scenario_name,
			share_reg);

	return scenario;
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

	if (mt_mdpm_debug && scenario >= 0)
		pr_info("MD1 scenario: %d(%s), scen_status: 0x%x\n",
			scenario, mdpm_scen[scenario].scenario_name,
			scen_status);

	return scenario;
}
#endif

int get_md1_scenario_power(enum md_scenario scenario,
	enum mdpm_power_type power_type, struct md_power_status *mdpm_pwr_sta)
{
	int s_power = 0;
	int temp;

	if (unlikely(scenario < 0))
		goto just_out;

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
	temp = snprintf(mdpm_pwr_sta->scenario_name, MAX_MDPM_NAME_LEN,
		"%s", mdpm_scen[scenario].scenario_name);
	if (temp < 0)
		pr_notice("%s error scenario_name\n", __func__);

	mdpm_pwr_sta->scanario_power = s_power;
	mdpm_pwr_sta->power_type = power_type;

just_out:
	return s_power;
}

int get_md1_tx_power(enum md_scenario scenario, u32 *share_mem,
	enum mdpm_power_type power_type,
	struct md_power_status *mdpm_pwr_sta)
{
	int i, rf_ret;
	enum tx_rat_type rat;
	int tx_power = 0, tx_power_max = 0;
	struct md_power_status mdpm_power_s_tmp;

#if 0
	if (scenario == S_STANDBY) {
		if (mt_mdpm_debug)
			pr_info("MD1 is standby, dBm pw: 0\n");

		return 0;
	}
#endif

	if (share_mem == NULL) {
		if (mt_mdpm_debug)
			pr_info("MD1 share_mem is NULL\n");

		return 0;
	}

	if (unlikely(scenario < 0))
		return 0;

	if (check_shm_version(share_mem) == VERSION_VALID)
		rf_ret = get_rfhw(share_mem);
	else
		return 0;

	if (rfhw_sel != rf_ret) {
		switch (rf_ret) {
		case 0:
			for (i = 0; i < TX_DBM_NUM; i++)
				mdpm_tx_pwr[i].rfhw = &rfhw0[i];

			rfhw_sel = rf_ret;
			break;

		case 1:
			for (i = 0; i < TX_DBM_NUM; i++)
				mdpm_tx_pwr[i].rfhw = &rfhw1[i];

			rfhw_sel = rf_ret;
			break;

		default:
			pr_notice("wrong rf_ret %d\n", rf_ret);
			break;
		}
	}

	if (mt_mdpm_debug == 2)
		for (i = 0; i < SHARE_MEM_SIZE; i++) {
			usedBytes += sprintf(log_buffer + usedBytes, "0x%x ",
				share_mem[i]);

			if ((i + 1) % 10 == 0) {
				usedBytes = 0;

			pr_info("%s\n", log_buffer);
		}
	}

	memset((void *)&mdpm_power_s_tmp, 0, sizeof(struct md_power_status));

	for (i = 0; i < MAX_DBM_FUNC_NUM; i++) {
		rat = mdpm_scen[scenario].tx_power_rat[i];
		if (rat == 0)
			break;

		tx_power = mdpm_scen[scenario].tx_power_func(share_mem,
			cnted_share_mem, rat, power_type, &mdpm_power_s_tmp);

		if (tx_power > tx_power_max) {
			tx_power_max = tx_power;
			mdpm_pwr_sta->rat = mdpm_power_s_tmp.rat;
			mdpm_pwr_sta->power_type = mdpm_power_s_tmp.power_type;
			mdpm_pwr_sta->dbm_section =
				mdpm_power_s_tmp.dbm_section;
			mdpm_pwr_sta->pa_power = mdpm_power_s_tmp.pa_power;
			mdpm_pwr_sta->rf_power = mdpm_power_s_tmp.rf_power;
			mdpm_pwr_sta->tx_power = mdpm_power_s_tmp.tx_power;
			mdpm_pwr_sta->total_power =
				mdpm_pwr_sta->scanario_power +
				mdpm_pwr_sta->tx_power;
		}
	}

	return tx_power_max;
}

static int get_md1_tx_power_by_table(u32 *dbm_mem, u32 *old_dbm_mem,
	struct tx_power *tx_pwr, enum tx_power_table dbm_type,
	enum tx_rat_type rat, enum mdpm_power_type power_type,
	int *sec_shift, struct md_power_status *md_power_s)
{

	int pa_power = 0, rf_power = 0;
	int section, index, offset, i;
	bool cmp = true;

	if (dbm_type >= TX_DBM_NUM || dbm_type < 0 ||
		power_type >= POWER_TYPE_NUM || power_type < 0) {
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
				tx_pwr->dbm_name,
				dbm_mem[tx_pwr->shm_dbm_idx[0]],
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

	if (rat > RAT_NUM || rat <= 0 ||
		power_type >= POWER_TYPE_NUM || power_type < 0) {
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

#ifdef MD_POWER_UT
void md_power_meter_ut(void)
{
	int i = 0, j = 0, k = 0, l = 0, index = 0, offset = 0, md_power = 0;
	u32 ret = 0;


	/* MD Version UT */
	memset(fake_share_mem, 0,
		sizeof(u32) * (DBM_TABLE_END - DBM_TABLE_START + 1));

	ret = check_shm_version(fake_share_mem);
	if (ret != VERSION_INIT) {
		pr_info("[UT] check_shm_version error %d , should be %d\n",
			ret, VERSION_INIT);
	} else
		pr_info("[UT] check_shm_version init OK\n");

	mdpm_shm_write(fake_share_mem, M_VERSION_CHECK, VERSION_VALID,
		VERSION_CHECK_VALID_MASK, VERSION_CHECK_VALID_SHIFT);

	ret = check_shm_version(fake_share_mem);
	if (ret != VERSION_VALID) {
		pr_info("[UT] check_shm_version error %d , should be %d\n",
			ret, VERSION_VALID);
	} else
		pr_info("[UT] check_shm_version valid OK\n");

	/* MD rfhw UT */
	mdpm_shm_write(fake_share_mem, M_RF_HW, 1,
		RF_HW_VERSION_MASK, RF_HW_VERSION_SHIFT);

	mdpm_shm_write(fake_share_mem, M_RF_HW, RF_HW_VALID,
		RF_HW_VALID_MASK, RF_HW_VALID_SHIFT);

	if (get_rfhw(fake_share_mem) == 0x1)
		pr_info("[UT] get_rfhw OK\n");
	else
		pr_info("[UT] get_rfhw ERROR\n");

#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
	/* check getting MD scenario with priority */
	for (i = 0; i < SCENARIO_NUM; i++) {
		fake_share_reg = 0;
		for (j = i; j < SCENARIO_NUM; j++) {
			fake_share_reg |= 0x1 <<
				mdpm_scen[scen_priority[j]].scenario_reg;
		}
		l = get_md1_scenario_internal(fake_share_reg);
		pr_info("test getting MD scenario:%d(%s) 0x%x\n",
			l, mdpm_scen[l].scenario_name, fake_share_reg);
		if (l != scen_priority[i])
			pr_info("[UT] getting scenario:%d(%s) 0x%x ERROR\n",
			l, mdpm_scen[l].scenario_name, fake_share_reg);
	}
#endif

	/* Tx Power UT */
	rfhw_sel = 0;
	for (i = 0; i < POWER_TYPE_NUM; i++) {
		if (mt_mdpm_debug)
			pr_info("[UT] ====== POWERTYPE:%d ======\n", i);

		for (j = 0; j <= 31; j++) {
			memset(fake_share_mem, 0, sizeof(u32) *
				(DBM_TABLE_END - DBM_TABLE_START + 1));

#ifdef GET_MD_SCEANRIO_BY_SHARE_MEMORY
				fake_share_mem[M_MD_SCENARIO] = 0x1 << j;
				fake_share_reg =
					fake_share_mem[M_MD_SCENARIO];
#else
				fake_share_reg = j;
#endif
			get_md1_power(i, true);

			for (k = 0; k < DBM_SECTION_NUM; k++) {
				if (mt_mdpm_debug) {
					l = get_md1_scenario_internal
						(fake_share_reg);
					l = (l < 0) ? S_STANDBY : l;
					pr_info("[UT] MD SCENARIO:%d(%s) 0x%x DBM SECTION:%d ======\n",
					j, mdpm_scen[l].scenario_name,
					fake_share_reg, k+1);
				}

				/* test if share_mem not change */
				md_power = get_md1_power(i, true);
				if (mt_mdpm_debug)
					pr_info("[UT] md_power:%d\n", md_power);

				/* test section min value */
				for (l = 0; l < TX_DBM_NUM; l++) {
					index = get_shm_idx(l,
						section_shift[k], true);
					offset = section_shift[k] % 32;
					fake_share_mem[index]
						|= 1 << offset;
				}

				md_power = get_md1_power(i, true);
				if (mt_mdpm_debug)
					pr_info("[UT] md_power:%d\n", md_power);

				/* test section median value */
				for (l = 0; l < TX_DBM_NUM; l++) {
					index = get_shm_idx(l,
						section_shift[k], true);
					offset = section_shift[k] % 32;
					fake_share_mem[index]
						|= 0x10 << offset;
				}

				md_power = get_md1_power(i, true);
				if (mt_mdpm_debug)
					pr_info("[UT] md_power:%d\n", md_power);

				/* test section max value */
				for (l = 0; l < TX_DBM_NUM; l++) {
					index = get_shm_idx(l,
						section_shift[k], true);
					offset = section_shift[k] % 32;
					fake_share_mem[index]
						|= DBM_SECTION_MASK << offset;
				}

				md_power = get_md1_power(i, true);
				if (mt_mdpm_debug)
					pr_info("[UT] md_power:%d\n", md_power);
			}
		}
	}
}
#endif /* MD_POWER_UT */

#endif /*MD_POWER_METER_ENABLE*/
