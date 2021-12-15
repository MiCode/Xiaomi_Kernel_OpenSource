/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <mach/mtk_pbm.h>
#include "mtk_mdpm.h"
#include "mtk_mdpm_common.h"

#if MD_POWER_METER_ENABLE
char log_buffer[128];
int usedBytes;
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
static int section_level[SECTION_NUM+1] = { GUARDING_PATTERN,
					    BIT_SECTION_1,
					    BIT_SECTION_2,
					    BIT_SECTION_3,
					    BIT_SECTION_4,
					    BIT_SECTION_5,
					    BIT_SECTION_6 };

static int md1_section_level_2g[SECTION_NUM+1] = { GUARDING_PATTERN,
						   VAL_MD1_2G_SECTION_1,
						   VAL_MD1_2G_SECTION_2,
						   VAL_MD1_2G_SECTION_3,
						   VAL_MD1_2G_SECTION_4,
						   VAL_MD1_2G_SECTION_5,
						   VAL_MD1_2G_SECTION_6 };

static int md1_section_level_3g[SECTION_NUM+1] = { GUARDING_PATTERN,
						   VAL_MD1_3G_SECTION_1,
						   VAL_MD1_3G_SECTION_2,
						   VAL_MD1_3G_SECTION_3,
						   VAL_MD1_3G_SECTION_4,
						   VAL_MD1_3G_SECTION_5,
						   VAL_MD1_3G_SECTION_6 };

static int md1_section_level_4g_upL1[SECTION_NUM+1] = { GUARDING_PATTERN,
						   VAL_MD1_4G_upL1_SECTION_1,
						   VAL_MD1_4G_upL1_SECTION_2,
						   VAL_MD1_4G_upL1_SECTION_3,
						   VAL_MD1_4G_upL1_SECTION_4,
						   VAL_MD1_4G_upL1_SECTION_5,
						   VAL_MD1_4G_upL1_SECTION_6 };

static int md1_section_level_4g_upL2[SECTION_NUM+1] = { GUARDING_PATTERN,
						   VAL_MD1_4G_upL2_SECTION_1,
						   VAL_MD1_4G_upL2_SECTION_2,
						   VAL_MD1_4G_upL2_SECTION_3,
						   VAL_MD1_4G_upL2_SECTION_4,
						   VAL_MD1_4G_upL2_SECTION_5,
						   VAL_MD1_4G_upL2_SECTION_6 };

static int md1_section_level_tdd[SECTION_NUM+1] = { GUARDING_PATTERN,
						   VAL_MD1_TDD_SECTION_1,
						   VAL_MD1_TDD_SECTION_2,
						   VAL_MD1_TDD_SECTION_3,
						   VAL_MD1_TDD_SECTION_4,
						   VAL_MD1_TDD_SECTION_5,
						   VAL_MD1_TDD_SECTION_6 };

static int md1_section_level_c2k[SECTION_NUM+1] = { GUARDING_PATTERN,
						VAL_MD1_C2K_SECTION_1,
						VAL_MD1_C2K_SECTION_2,
						VAL_MD1_C2K_SECTION_3,
						VAL_MD1_C2K_SECTION_4,
						VAL_MD1_C2K_SECTION_5,
						VAL_MD1_C2K_SECTION_6 };

static int md1_scenario_pwr[POWER_CATEGORY_NUM][SCENARIO_NUM] = {
				{MAX_PW_STANDBY,
				 MAX_PW_2G_CONNECT,
				 MAX_PW_3G_C2K_TALKING,
				 MAX_PW_3G_4G_C2K_PAGING,
				 MAX_PW_3G_C2K_DATALINK,
				 MAX_PW_4G_DL_1CC,
				 MAX_PW_4G_DL_2CC},
				{AVG_PW_STANDBY,
				 AVG_PW_2G_CONNECT,
				 AVG_PW_3G_C2K_TALKING,
				 AVG_PW_3G_4G_C2K_PAGING,
				 AVG_PW_3G_C2K_DATALINK,
				 AVG_PW_4G_DL_1CC,
				 AVG_PW_4G_DL_2CC}
				};

static int md1_pa_pwr_2g[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_PA_2G_SECTION_1,
				 MAX_PW_MD1_PA_2G_SECTION_2,
				 MAX_PW_MD1_PA_2G_SECTION_3,
				 MAX_PW_MD1_PA_2G_SECTION_4,
				 MAX_PW_MD1_PA_2G_SECTION_5,
				 MAX_PW_MD1_PA_2G_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_PA_2G_SECTION_1,
				 AVG_PW_MD1_PA_2G_SECTION_2,
				 AVG_PW_MD1_PA_2G_SECTION_3,
				 AVG_PW_MD1_PA_2G_SECTION_4,
				 AVG_PW_MD1_PA_2G_SECTION_5,
				 AVG_PW_MD1_PA_2G_SECTION_6}
				};

static int md1_pa_pwr_3g[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_PA_3G_SECTION_1,
				 MAX_PW_MD1_PA_3G_SECTION_2,
				 MAX_PW_MD1_PA_3G_SECTION_3,
				 MAX_PW_MD1_PA_3G_SECTION_4,
				 MAX_PW_MD1_PA_3G_SECTION_5,
				 MAX_PW_MD1_PA_3G_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_PA_3G_SECTION_1,
				 AVG_PW_MD1_PA_3G_SECTION_2,
				 AVG_PW_MD1_PA_3G_SECTION_3,
				 AVG_PW_MD1_PA_3G_SECTION_4,
				 AVG_PW_MD1_PA_3G_SECTION_5,
				 AVG_PW_MD1_PA_3G_SECTION_6}
				};

static int md1_pa_pwr_4g_upL1[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_PA_4G_upL1_SECTION_1,
				 MAX_PW_MD1_PA_4G_upL1_SECTION_2,
				 MAX_PW_MD1_PA_4G_upL1_SECTION_3,
				 MAX_PW_MD1_PA_4G_upL1_SECTION_4,
				 MAX_PW_MD1_PA_4G_upL1_SECTION_5,
				 MAX_PW_MD1_PA_4G_upL1_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_PA_4G_upL1_SECTION_1,
				 AVG_PW_MD1_PA_4G_upL1_SECTION_2,
				 AVG_PW_MD1_PA_4G_upL1_SECTION_3,
				 AVG_PW_MD1_PA_4G_upL1_SECTION_4,
				 AVG_PW_MD1_PA_4G_upL1_SECTION_5,
				 AVG_PW_MD1_PA_4G_upL1_SECTION_6}
				};

static int md1_pa_pwr_4g_upL2[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				  MAX_PW_MD1_PA_4G_upL2_SECTION_1,
				  MAX_PW_MD1_PA_4G_upL2_SECTION_2,
				  MAX_PW_MD1_PA_4G_upL2_SECTION_3,
				  MAX_PW_MD1_PA_4G_upL2_SECTION_4,
				  MAX_PW_MD1_PA_4G_upL2_SECTION_5,
				  MAX_PW_MD1_PA_4G_upL2_SECTION_6},
				 {GUARDING_PATTERN,
				  AVG_PW_MD1_PA_4G_upL2_SECTION_1,
				  AVG_PW_MD1_PA_4G_upL2_SECTION_2,
				  AVG_PW_MD1_PA_4G_upL2_SECTION_3,
				  AVG_PW_MD1_PA_4G_upL2_SECTION_4,
				  AVG_PW_MD1_PA_4G_upL2_SECTION_5,
				  AVG_PW_MD1_PA_4G_upL2_SECTION_6}
				};

static int md1_pa_pwr_c2k[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_PA_C2K_SECTION_1,
				 MAX_PW_MD1_PA_C2K_SECTION_2,
				 MAX_PW_MD1_PA_C2K_SECTION_3,
				 MAX_PW_MD1_PA_C2K_SECTION_4,
				 MAX_PW_MD1_PA_C2K_SECTION_5,
				 MAX_PW_MD1_PA_C2K_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_PA_C2K_SECTION_1,
				 AVG_PW_MD1_PA_C2K_SECTION_2,
				 AVG_PW_MD1_PA_C2K_SECTION_3,
				 AVG_PW_MD1_PA_C2K_SECTION_4,
				 AVG_PW_MD1_PA_C2K_SECTION_5,
				 AVG_PW_MD1_PA_C2K_SECTION_6}
				};

static int md1_rf_pwr_2g[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_RF_2G_SECTION_1,
				 MAX_PW_MD1_RF_2G_SECTION_2,
				 MAX_PW_MD1_RF_2G_SECTION_3,
				 MAX_PW_MD1_RF_2G_SECTION_4,
				 MAX_PW_MD1_RF_2G_SECTION_5,
				 MAX_PW_MD1_RF_2G_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_RF_2G_SECTION_1,
				 AVG_PW_MD1_RF_2G_SECTION_2,
				 AVG_PW_MD1_RF_2G_SECTION_3,
				 AVG_PW_MD1_RF_2G_SECTION_4,
				 AVG_PW_MD1_RF_2G_SECTION_5,
				 AVG_PW_MD1_RF_2G_SECTION_6}
				};

static int md1_rf_pwr_3g[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_RF_3G_SECTION_1,
				 MAX_PW_MD1_RF_3G_SECTION_2,
				 MAX_PW_MD1_RF_3G_SECTION_3,
				 MAX_PW_MD1_RF_3G_SECTION_4,
				 MAX_PW_MD1_RF_3G_SECTION_5,
				 MAX_PW_MD1_RF_3G_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_RF_3G_SECTION_1,
				 AVG_PW_MD1_RF_3G_SECTION_2,
				 AVG_PW_MD1_RF_3G_SECTION_3,
				 AVG_PW_MD1_RF_3G_SECTION_4,
				 AVG_PW_MD1_RF_3G_SECTION_5,
				 AVG_PW_MD1_RF_3G_SECTION_6}
				};

static int md1_rf_pwr_4g_upL1[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_RF_4G_upL1_SECTION_1,
				 MAX_PW_MD1_RF_4G_upL1_SECTION_2,
				 MAX_PW_MD1_RF_4G_upL1_SECTION_3,
				 MAX_PW_MD1_RF_4G_upL1_SECTION_4,
				 MAX_PW_MD1_RF_4G_upL1_SECTION_5,
				 MAX_PW_MD1_RF_4G_upL1_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_RF_4G_upL1_SECTION_1,
				 AVG_PW_MD1_RF_4G_upL1_SECTION_2,
				 AVG_PW_MD1_RF_4G_upL1_SECTION_3,
				 AVG_PW_MD1_RF_4G_upL1_SECTION_4,
				 AVG_PW_MD1_RF_4G_upL1_SECTION_5,
				 AVG_PW_MD1_RF_4G_upL1_SECTION_6}
				};

static int md1_rf_pwr_4g_upL2[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_RF_4G_upL2_SECTION_1,
				 MAX_PW_MD1_RF_4G_upL2_SECTION_2,
				 MAX_PW_MD1_RF_4G_upL2_SECTION_3,
				 MAX_PW_MD1_RF_4G_upL2_SECTION_4,
				 MAX_PW_MD1_RF_4G_upL2_SECTION_5,
				 MAX_PW_MD1_RF_4G_upL2_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_RF_4G_upL2_SECTION_1,
				 AVG_PW_MD1_RF_4G_upL2_SECTION_2,
				 AVG_PW_MD1_RF_4G_upL2_SECTION_3,
				 AVG_PW_MD1_RF_4G_upL2_SECTION_4,
				 AVG_PW_MD1_RF_4G_upL2_SECTION_5,
				 AVG_PW_MD1_RF_4G_upL2_SECTION_6}
				};

static int md1_rf_pwr_c2k[POWER_CATEGORY_NUM][SECTION_NUM+1] = {
				{GUARDING_PATTERN,
				 MAX_PW_MD1_RF_C2K_SECTION_1,
				 MAX_PW_MD1_RF_C2K_SECTION_2,
				 MAX_PW_MD1_RF_C2K_SECTION_3,
				 MAX_PW_MD1_RF_C2K_SECTION_4,
				 MAX_PW_MD1_RF_C2K_SECTION_5,
				 MAX_PW_MD1_RF_C2K_SECTION_6},
				{GUARDING_PATTERN,
				 AVG_PW_MD1_RF_C2K_SECTION_1,
				 AVG_PW_MD1_RF_C2K_SECTION_2,
				 AVG_PW_MD1_RF_C2K_SECTION_3,
				 AVG_PW_MD1_RF_C2K_SECTION_4,
				 AVG_PW_MD1_RF_C2K_SECTION_5,
				 AVG_PW_MD1_RF_C2K_SECTION_6}
				};

static int get_md1_2g_dbm_power(u32 *share_mem, unsigned int power_category);
static int get_md1_3g_dbm_power(u32 *share_mem, unsigned int power_category);
static int get_md1_4g_dbm_power(u32 *share_mem, unsigned int power_category);
static int get_md1_c2k_dbm_power(u32 *share_mem, unsigned int power_category);

struct mdpm mdpm_info[SCENARIO_NUM] = {
	[S_STANDBY] = {
		.scenario_power = {
			MAX_PW_STANDBY,
			AVG_PW_STANDBY},
		.dbm_power_func = {
			get_md1_4g_dbm_power, /* 4G 0D0U  or sleep */
			NULL, NULL, NULL, NULL
		},
	},

	[S_2G_CONNECT] = {
		.scenario_power = {
			MAX_PW_2G_CONNECT,
			AVG_PW_2G_CONNECT},
		.dbm_power_func = {
			get_md1_2g_dbm_power,
			get_md1_4g_dbm_power, /* 2G=1, 4G 0D0U or sleep */
			NULL, NULL, NULL
		},

	},

	[S_3G_C2K_TALKING] = {
		.scenario_power = {
			MAX_PW_3G_C2K_TALKING,
			AVG_PW_3G_C2K_TALKING},
		.dbm_power_func = {
			get_md1_3g_dbm_power,
			get_md1_c2k_dbm_power,
			NULL, NULL, NULL
		},
	},

	[S_3G_4G_C2K_PAGING] = {
		.scenario_power = {
			MAX_PW_3G_4G_C2K_PAGING,
			AVG_PW_3G_4G_C2K_PAGING},
		.dbm_power_func = {
			get_md1_3g_dbm_power,
			get_md1_c2k_dbm_power,
			get_md1_4g_dbm_power,
			NULL, NULL
		},
	},

	[S_3G_C2K_DATALINK] = {
		.scenario_power = {
			MAX_PW_3G_C2K_DATALINK,
			AVG_PW_3G_C2K_DATALINK},
		.dbm_power_func = {
			get_md1_3g_dbm_power,
			get_md1_c2k_dbm_power,
			NULL, NULL, NULL
		},
	},

	[S_4G_DL_1CC] = {
		.scenario_power = {
			MAX_PW_4G_DL_1CC,
			AVG_PW_4G_DL_1CC},
		.dbm_power_func = {
			get_md1_4g_dbm_power,
			NULL, NULL, NULL
		},
	},

	[S_4G_DL_2CC] = {
		.scenario_power = {
			MAX_PW_4G_DL_2CC,
			AVG_PW_4G_DL_2CC},
		.dbm_power_func = {
			get_md1_4g_dbm_power,
			NULL, NULL, NULL, NULL
		},
	},
};

#ifdef MD_POWER_UT
void md_power_meter_ut(void)
{
	int i = 0, j = 0, k = 0, l = 0;
	int md_power = 0;

	for (i = 0; i < POWER_CATEGORY_NUM; i++) {
		if (mt_mdpm_debug)
			pr_info("[UT] ====== POWERCATEGORY:%d ======\n", i);

		for (j = 0; j <= 16; j++) {
			if (mt_mdpm_debug)
				pr_info("[UT] ====== MD SCENARIO:%d ======\n",
					j);

			if (j == 0)
				fake_share_reg = 0;
			else
				fake_share_reg = _BIT_(j-1);

			memset(fake_share_mem, 0, sizeof(u32) *
				SECTION_LEVLE_2G);
			get_md1_power(i, true);

			for (k = 0; k < SECTION_NUM; k++) {
				if (mt_mdpm_debug)
					pr_info("[UT] ====== DBM SECTION:%d ======\n",
					k + 1);

				/* test if share_mem not change */
				md_power = get_md1_power(i, true);
				if (mt_mdpm_debug)
					pr_info("[UT] md_power:%d ====(dbm=0)\n",
					md_power);

				/* test section min value */
				for (l = 0; l < SECTION_LEVLE_2G; l++)
					fake_share_mem[l] |= 1 <<
						section_level[k+1];

				md_power = get_md1_power(i, true);
				if (mt_mdpm_debug)
					pr_info("[UT] md_power:%d ====\n",
					md_power);

				/* test section median value */
				for (l = 0; l < SECTION_LEVLE_2G; l++)
					fake_share_mem[l] |= 0x10 <<
						section_level[k+1];

				md_power = get_md1_power(i, true);
				if (mt_mdpm_debug)
					pr_info("[UT] md_power:%d ====\n",
					md_power);

				/* test section max value */
				for (l = 0; l < SECTION_LEVLE_2G; l++)
					fake_share_mem[l] |= SECTION_VALUE <<
						section_level[k+1];

				md_power = get_md1_power(i, true);
				if (mt_mdpm_debug)
					pr_info("[UT] md_power:%d ====\n",
					md_power);
			}
		}
	}
}
#endif /* MD_POWER_UT */

static int is_scenario_hit(u32 share_reg, unsigned int scenario)
{
	int hit = 0;

	switch (scenario) {
	case S_STANDBY:
		/* if bit 15 and bit 1 to 7 are not asserted */
		if ((share_reg & _BITMASK_(7:1)) == 0)
			hit = 1;
		break;
	case S_2G_CONNECT:
		/* if bit 1 is asserted */
		if ((share_reg & _BIT_(1)) != 0)
			hit = 1;
		break;
	case S_3G_C2K_TALKING:
		/* if bit 2 is asserted */
		if ((share_reg & _BIT_(2)) != 0)
			hit = 1;
		break;
	case S_3G_4G_C2K_PAGING:
		/* if bit 3 is asserted */
		if ((share_reg & _BIT_(3)) != 0)
			hit = 1;
		break;
	case S_3G_C2K_DATALINK:
		/* if bit 4 are not asserted */
		if ((share_reg & _BIT_(4)) != 0)
			hit = 1;
		break;
	case S_4G_DL_1CC:
		/* if bit 5 is asserted */
		if ((share_reg & _BIT_(5)) != 0)
			hit = 1;
		break;
	case S_4G_DL_2CC:
		/* if bit 6 or bit 7 is asserted */
		if ((share_reg & _BITMASK_(7:6)) != 0)
			hit = 1;
		break;
	default:
		pr_err("[%s] ERROR, unknown scenario [%d]\n",
			__func__, scenario);
		WARN_ON_ONCE(1);
		break;
	}

	return hit;
}

void init_md1_section_level(u32 *share_mem)
{
	u32 mem_2g = 0, mem_3g = 0, mem_4g_upL1 = 0, mem_4g_upL2 = 0;
	u32 mem_tdd = 0, mem_c2k = 0;
	int section;

	for (section = 1; section <= SECTION_NUM; section++) {
		if (md1_section_level_2g[section] > SECTION_VALUE
			|| md1_section_level_3g[section] > SECTION_VALUE
			|| md1_section_level_3g[section] > SECTION_VALUE
			|| md1_section_level_4g_upL1[section] > SECTION_VALUE
			|| md1_section_level_4g_upL2[section] > SECTION_VALUE
			|| md1_section_level_tdd[section] > SECTION_VALUE
			|| md1_section_level_c2k[section] > SECTION_VALUE) {
			pr_notice("[%s] md1_section_level too large !\n",
			__func__);
			WARN_ON_ONCE(1);
		}

		mem_2g |= md1_section_level_2g[section] <<
			section_level[section];
		mem_3g |= md1_section_level_3g[section] <<
			section_level[section];
		mem_4g_upL1 |= md1_section_level_4g_upL1[section] <<
			section_level[section];
		mem_4g_upL2 |= md1_section_level_4g_upL2[section] <<
			section_level[section];
		mem_tdd |= md1_section_level_tdd[section] <<
			section_level[section];
		mem_c2k |= md1_section_level_c2k[section] <<
			section_level[section];
	}

	/* Get 4 byte = 32 bit */
	mem_2g &= SECTION_LEN;
	mem_3g &= SECTION_LEN;
	mem_4g_upL1 &= SECTION_LEN;
	mem_4g_upL2 &= SECTION_LEN;
	mem_tdd &= SECTION_LEN;
	mem_c2k &= SECTION_LEN;

	share_mem[SECTION_LEVLE_2G] = mem_2g;
	share_mem[SECTION_LEVLE_3G] = mem_3g;
	share_mem[SECTION_LEVLE_4G] = mem_4g_upL1;
	share_mem[SECTION_1_LEVLE_4G] = mem_4g_upL2;
	share_mem[SECTION_LEVLE_TDD] = mem_tdd;
	share_mem[SECTION_1_LEVLE_C2K] = mem_c2k;

	pr_info("AP2MD1 section level, 2G: 0x%x(0x%x), 3G: 0x%x(0x%x), ",
			mem_2g, share_mem[SECTION_LEVLE_2G],
			mem_3g, share_mem[SECTION_LEVLE_3G]);
	pr_info(
	"4G_upL1:0x%x(0x%x),4G_upL2:0x%x(0x%x),TDD:0x%x(0x%x),addr:0x%p\n",
			mem_4g_upL1, share_mem[SECTION_LEVLE_4G],
			mem_4g_upL2, share_mem[SECTION_1_LEVLE_4G],
			mem_tdd, share_mem[SECTION_LEVLE_TDD],
			share_mem);
	pr_info("4G_upL1: 0x%x(0x%x), TDD: 0x%x(0x%x), addr: 0x%p\n",
		mem_4g_upL1, share_mem[SECTION_LEVLE_4G],
		mem_tdd, share_mem[SECTION_LEVLE_TDD],
		share_mem);
	pr_info("C2K section level, C2K: 0x%x(0x%x), addr: 0x%p\n",
			mem_c2k, share_mem[SECTION_1_LEVLE_C2K],
			share_mem);
}

unsigned int get_md1_scenario(u32 share_reg, unsigned int power_category)
{
	int pw_scenario = 0, scenario = -1;
	int i;

	/* get scenario index when working & max power (bit4 and bit5 no use) */
	for (i = 0; i < SCENARIO_NUM; i++) {
		if (is_scenario_hit(share_reg, i)) {
			if (md1_scenario_pwr[power_category][i] >=
				pw_scenario) {
				pw_scenario =
					md1_scenario_pwr[power_category][i];
				scenario = i;
			}
		}
	}

	scenario = (scenario < 0) ? S_STANDBY : scenario;

	if (mt_mdpm_debug)
		pr_info("MD1 scenario: 0x%x, reg: 0x%x, pw: %d\n",
			scenario, share_reg,
			md1_scenario_pwr[power_category][scenario]);

	return scenario;
}

int get_md1_scenario_power(unsigned int scenario, unsigned int power_category)
{
	return mdpm_info[scenario].scenario_power[power_category];
}

int get_md1_dBm_power(unsigned int scenario, u32 *share_mem,
	unsigned int power_category)
{
	int i;
	int dbm_power = 0, dbm_power_max = 0;

#if 0
	if (scenario == S_STANDBY) {
		if (mt_mdpm_debug)
			pr_info("MD1 is standby, dBm pw: 0\n");

		return 0;
	}
#endif

	if (share_mem == NULL) {
		if (mt_mdpm_debug)
			pr_info("MD1 share_mem is NULL, use max MD dbm power: %d\n"
				, MAX_DBM_POWER);

		return MAX_DBM_POWER;
	}

	usedBytes = 0;
	for (i = 0; i < SHARE_MEM_BLOCK_NUM; i++) {
		usedBytes += sprintf(log_buffer + usedBytes, "0x%x ",
			share_mem[i]);

		if ((i + 1) % 10 == 0) {
			usedBytes = 0;
			if (mt_mdpm_debug)
				pr_info("%s\n", log_buffer);
		}
	}

	for (i = 0; i < MAX_DBM_FUNC_NUM; i++) {
		if (mdpm_info[scenario].dbm_power_func[i] == NULL)
			break;

		dbm_power = mdpm_info[scenario].dbm_power_func[i](share_mem,
			power_category);
		dbm_power_max = (dbm_power > dbm_power_max) ?
			dbm_power : dbm_power_max;
	}

	return dbm_power_max;
}

static int get_md1_2g_dbm_power(u32 *share_mem, unsigned int power_category)
{
	static u32 bef_share_mem;
	static int pa_power, rf_power;
	int section;

	if (share_mem[DBM_2G_TABLE] == bef_share_mem) {
		if (mt_mdpm_debug)
			pr_info("2G dBm no TX power, reg: 0x%x(0x%x) return 0\n",
				share_mem[DBM_2G_TABLE], bef_share_mem);

		return 0;
	}

	for (section = 1; section <= SECTION_NUM; section++) {
		if (((share_mem[DBM_2G_TABLE] >> section_level[section]) &
			SECTION_VALUE) !=
			((bef_share_mem >> section_level[section]) &
			SECTION_VALUE)) {
			/* get PA power */
			pa_power = md1_pa_pwr_2g[power_category][section];

			/* get RF power */
			rf_power = md1_rf_pwr_2g[power_category][section];

			if (mt_mdpm_debug)
				pr_info("2G dBm: reg:0x%x(0x%x),pa:%d,rf:%d,s:%d\n",
					share_mem[DBM_2G_TABLE], bef_share_mem,
					pa_power, rf_power, section);

			bef_share_mem = share_mem[DBM_2G_TABLE];

			break;
		}
	}
	return pa_power + rf_power;
}

static int get_md1_3g_dbm_power(u32 *share_mem, unsigned int power_category)
{
	static u32 bef_share_mem;
	static int pa_power, rf_power;
	int section;

	if (share_mem[DBM_3G_TABLE] == bef_share_mem) {
		if (mt_mdpm_debug)
			pr_info("3G dBm no TX power, reg: 0x%x(0x%x) return 0\n",
				share_mem[DBM_2G_TABLE], bef_share_mem);

		return 0;
	}

	for (section = 1; section <= SECTION_NUM; section++) {
		if (((share_mem[DBM_3G_TABLE] >> section_level[section]) &
			SECTION_VALUE) !=
			((bef_share_mem >> section_level[section]) &
			SECTION_VALUE)) {
			/* get PA power */
			pa_power = md1_pa_pwr_3g[power_category][section];

			/* get RF power */
			rf_power = md1_rf_pwr_3g[power_category][section];

			if (mt_mdpm_debug)
				pr_info("3G dBm: reg:0x%x(0x%x),pa:%d,rf:%d,s:%d\n",
					share_mem[DBM_3G_TABLE], bef_share_mem,
					pa_power, rf_power, section);

			bef_share_mem = share_mem[DBM_3G_TABLE];

			break;
		}
	}
	return pa_power + rf_power;
}

static int get_md1_4g_upL1_dbm_power(u32 *share_mem,
	unsigned int power_category)
{
	static u32 bef_share_mem;
	static int pa_power, rf_power;
	int section;

	if (share_mem[DBM_4G_TABLE] == bef_share_mem) {
		if (mt_mdpm_debug)
			pr_info("4G dBm no TX power, reg: 0x%x(0x%x) return 0\n",
				share_mem[DBM_2G_TABLE], bef_share_mem);

		return 0;
	}

	for (section = 1; section <= SECTION_NUM; section++) {
		if (((share_mem[DBM_4G_TABLE] >> section_level[section]) &
			SECTION_VALUE) !=
			((bef_share_mem >> section_level[section]) &
			SECTION_VALUE)) {
			/* get PA power */
			pa_power = md1_pa_pwr_4g_upL1[power_category][section];

			/* get RF power */
			rf_power = md1_rf_pwr_4g_upL1[power_category][section];

			if (mt_mdpm_debug)
				pr_info("4G dBm: reg:0x%x(0x%x),pa:%d,rf:%d,s:%d\n",
					share_mem[DBM_4G_TABLE], bef_share_mem,
					pa_power, rf_power, section);

			bef_share_mem = share_mem[DBM_4G_TABLE];

			break;
		}
	}
	return pa_power + rf_power;
}

static int get_md1_4g_upL2_dbm_power(u32 *share_mem,
	unsigned int power_category)
{
	static u32 bef_share_mem;
	static int pa_power, rf_power;
	int section;

	if (share_mem[DBM_4G_1_TABLE] == bef_share_mem) {
		if (mt_mdpm_debug)
			pr_info("4G_1 dBm no TX power, reg: 0x%x(0x%x) return 0\n",
				share_mem[DBM_4G_1_TABLE], bef_share_mem);

		return 0;
	}

	for (section = 1; section <= SECTION_NUM; section++) {
		if (((share_mem[DBM_4G_1_TABLE] >> section_level[section]) &
			SECTION_VALUE) !=
			((bef_share_mem >> section_level[section]) &
			SECTION_VALUE)) {
			/* get PA power */
			pa_power = md1_pa_pwr_4g_upL2[power_category][section];

			/* get RF power */
			rf_power = md1_rf_pwr_4g_upL2[power_category][section];

			if (mt_mdpm_debug)
				pr_info("4G1 dBm:reg:0x%x(0x%x),pa:%d,rf:%d,s:%d\n",
					share_mem[DBM_4G_1_TABLE],
					bef_share_mem,
					pa_power, rf_power, section);

			bef_share_mem = share_mem[DBM_4G_1_TABLE];

			break;
		}
	}
	return pa_power + rf_power;
}

static int get_md1_4g_dbm_power(u32 *share_mem, unsigned int power_category)
{
	int dbm_power = 0;

	dbm_power = get_md1_4g_upL1_dbm_power(share_mem, power_category);
	dbm_power += get_md1_4g_upL2_dbm_power(share_mem, power_category);
	return dbm_power;
}

static int get_md1_c2k_dbm_power(u32 *share_mem, unsigned int power_category)
{
	static u32 bef_share_mem;
	static int pa_power, rf_power;
	int section;

	if (share_mem[DBM_C2K_1_TABLE] == bef_share_mem) {
		if (mt_mdpm_debug)
			pr_info("C2K dBm, no TX power, reg: 0x%x(0x%x) return 0\n",
			share_mem[DBM_C2K_1_TABLE], bef_share_mem);

		return 0;
	}

	for (section = 1; section <= SECTION_NUM; section++) {
		if (((share_mem[DBM_C2K_1_TABLE] >> section_level[section]) &
			SECTION_VALUE) !=
			((bef_share_mem >> section_level[section]) &
			SECTION_VALUE)) {
			/* get PA power */
			pa_power = md1_pa_pwr_c2k[power_category][section];

			/* get RF power */
			rf_power = md1_rf_pwr_c2k[power_category][section];

			if (mt_mdpm_debug)
				pr_info("C2K dBm update, reg:0x%x(0x%x),pa:%d,rf:%d,s:%d\n",
					share_mem[DBM_C2K_1_TABLE],
					bef_share_mem,
					pa_power, rf_power, section);

			bef_share_mem = share_mem[DBM_C2K_1_TABLE];

			break;
		}
	}
	return pa_power + rf_power;
}
#endif /*MD_POWER_METER_ENABLE*/
