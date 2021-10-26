/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/perf_event.h>
#include <trace/events/mtk_events.h>
#ifdef CONFIG_MTK_QOS_FRAMEWORK
#include <mtk_qos_ipi.h>
#endif
#ifdef CONFIG_MTK_STATIC_POWER
#include <mtk_common_static_power.h>
#endif
#ifdef CONFIG_THERMAL
#include <mtk_thermal.h>
#endif
#ifdef CONFIG_MTK_CPU_FREQ
#include <mtk_cpufreq_common_api.h>
#endif
#ifndef __CHECKER__
#define CREATE_TRACE_POINTS
#include <mtk_swpm_tracker_trace.h>
#endif
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_reservedmem.h>
#endif
#include <mtk_swpm_common.h>
#include <mtk_swpm_platform.h>
#include <mtk_swpm_sp_platform.h>
#include <mtk_swpm_interface.h>

#undef swpm_pmu_enable

/****************************************************************************
 *  Macro Definitions
 ****************************************************************************/
/* #define LOG_LOOP_TIME_PROFILE */

/****************************************************************************
 *  Type Definitions
 ****************************************************************************/

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static unsigned int swpm_init_state;

/* index snapshot */
static struct mutex swpm_snap_lock;
static struct mem_swpm_index mem_idx_snap;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
/* share sram for average power index */
static struct share_index *share_idx_ref;
static struct share_ctrl *share_idx_ctrl;
/* type of unsigned int pointer for share_index iterator */
static unsigned int *idx_ref_uint_ptr;
/* numbers of unsigned int output to LTR with iterator */
static unsigned int idx_output_size;

/* share dram for subsys related table communication */
static phys_addr_t rec_phys_addr, rec_virt_addr;
static unsigned long long rec_size;
#endif

#define swpm_pmu_get_sta(u)  ((swpm_pmu_sta & (1 << u)) >> u)
#define swpm_pmu_set_sta(u)  (swpm_pmu_sta |= (1 << u))
#define swpm_pmu_clr_sta(u)  (swpm_pmu_sta &= ~(1 << u))
static struct mutex swpm_pmu_mutex;
static unsigned int swpm_pmu_sta;

__weak int mt_spower_get_leakage_uW(int dev, int voltage, int deg)
{
	return 0;
}
struct core_swpm_rec_data *core_ptr;
struct mem_swpm_rec_data *mem_ptr;
struct me_swpm_rec_data *me_ptr;

static DEFINE_PER_CPU(struct perf_event *, l3dc_events);
static DEFINE_PER_CPU(struct perf_event *, inst_spec_events);
static DEFINE_PER_CPU(struct perf_event *, cycle_events);
static struct perf_event_attr l3dc_event_attr = {
	.type           = PERF_TYPE_RAW,
	.config         = 0x2B,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};
static struct perf_event_attr inst_spec_event_attr = {
	.type           = PERF_TYPE_RAW,
	.config         = 0x1B,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};
static struct perf_event_attr cycle_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_CPU_CYCLES,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};

/* rt => /100, uW => 1000, res => *100000 */
#define CORE_DEFAULT_DEG (30)
#define CORE_DEFAULT_LKG (32)
#define CORE_LKG_RT_RES (100)
static unsigned int core_lkg;
static unsigned int core_lkg_replaced;
static unsigned short core_lkg_rt[NR_CORE_LKG_TYPE] = {
	0, 15957, 5732, 7522, 4211, 6688, 10739,
	26127, 19969, 3056
};
static unsigned short core_volt_tbl[NR_CORE_VOLT] = {
	550, 600, 650, 725, 750,
};
static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {
	400, 600, 800, 933, 1333, 1866, 2133, 2750,
};
static struct aphy_core_bw_data aphy_ref_core_bw_tbl[] = {
	[DDR_400] = {
	.bw = {	32, 71, 150, 283, 650, 964,
	1283, 1588, 1923, 2235, 2648, 2884},
	},
	[DDR_600] = {
	.bw = {	46, 107, 398, 1013, 1929, 2246,
	2521, 3086, 3350, 3631, 3946, 4398},
	},
	[DDR_800] = {
	.bw = {	60, 142, 538, 1349, 2572, 2995,
	3369, 4133, 4450, 4825, 5221, 5897},
	},
	[DDR_933] = {
	.bw = {	70, 173, 626, 1575, 3002, 3497,
	3929, 4803, 5187, 5611, 6115, 6880},
	},
	[DDR_1333] = {
	.bw = {	87, 213, 811, 2020, 3858, 4500,
	5030, 6145, 6624, 7205, 7826, 8831},
	},
	[DDR_1866] = {
	.bw = {	136, 335, 1267, 3169, 6010, 6946,
	7754, 9455, 10138, 11066, 11948, 13588},
	},
	[DDR_2133] = {
	.bw = {	160, 393, 1479, 3708, 6997, 8053,
	8978, 10943, 11692, 12931, 13839, 15679},
	},
	[DDR_2750] = {
	.bw = {	160, 393, 1479, 3708, 6997, 8053,
	8978, 10943, 11692, 12931, 13839, 15679},
	},
};
static struct aphy_core_pwr_data aphy_def_core_pwr_tbl[] = {
	[APHY_VCORE] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {7, 17, 34, 64, 136, 198,
	257, 320, 358, 387, 387, 395},
			.write_coef = {13, 30, 61, 116, 256, 373,
	487, 613, 726, 773, 773, 773},
		},
		[DDR_600] = {
			.read_coef = {9, 23, 83, 198, 358, 427,
	450, 467, 471, 473, 475, 475},
			.write_coef = {23, 53, 201, 492, 946, 1036,
	1155, 1156, 1156, 1156, 1156, 1159},
		},
		[DDR_800] = {
			.read_coef = {80, 202, 571, 921, 1307, 1416,
	1497, 1498, 1501, 1502, 1502, 1502},
			.write_coef = {93, 211, 686, 1209, 1911, 2084,
	2158, 2158, 2158, 2158, 2161, 2164},
		},
		[DDR_933] = {
			.read_coef = {104, 267, 747, 1206, 1680, 1824,
	1918, 1925, 1925, 1925, 1925, 1925},
			.write_coef = {110, 282, 868, 1525, 2406, 2624,
	2810, 2813, 2818, 2818, 2818, 2823},
		},
		[DDR_1333] = {
			.read_coef = {187, 451, 1175, 1873, 2506, 2685,
	2814, 2847, 2847, 2847, 2847, 2847},
			.write_coef = {194, 417, 1337, 2349, 3621, 3972,
	4095, 4095, 4095, 4095, 4105, 4105},
		},
		[DDR_1866] = {
			.read_coef = {364, 852, 2208, 3393, 4403, 4733,
	4880, 4919, 4921, 4940, 4940, 4940},
			.write_coef = {375, 835, 2433, 4205, 6067, 6602,
	7060, 7086, 7086, 7093, 7117, 7141},
		},
		[DDR_2133] = {
			.read_coef = {471, 1029, 2664, 4111, 5165, 5510,
	5675, 5778, 5794, 5809, 5809, 5809},
			.write_coef = {489, 1013, 2914, 5115, 7116, 7722,
	8270, 8316, 8316, 8359, 8359, 8359},
		},
		[DDR_2750] = {
			.read_coef = {456, 995, 2575, 3974, 4993, 5326,
	5486, 5585, 5601, 5615, 5615, 5615},
			.write_coef = {472, 979, 2817, 4944, 6879, 7465,
	7994, 8039, 8039, 8081, 8081, 8081},
		},
	},
	.coef_idle = {948, 1115, 1282, 1546, 1838, 2754, 3076, 2973},
	},
};
static struct aphy_others_bw_data aphy_ref_others_bw_tbl[] = {
	[DDR_400] = {
	.bw = {	32, 64, 96, 128,
	160, 320, 480, 640, 800, 960,
	1274, 1594, 1907, 2240, 2534, 2861},
	},
	[DDR_600] = {
	.bw = {	48, 96, 144, 192,
	240, 480, 720, 960, 1195, 1440,
	1910, 2390, 2832, 3326, 3806, 4310},
	},
	[DDR_800] = {
	.bw = {	64, 128, 192, 256,
	320, 640, 960, 1274, 1600, 1901,
	2534, 3168, 3802, 4474, 5075, 5741},
	},
	[DDR_933] = {
	.bw = {	75, 149, 224, 299,
	373, 746, 1120, 1493, 1851, 2217,
	2986, 3657, 4434, 5225, 5934, 6621},
	},
	[DDR_1333] = {
	.bw = {	96, 192, 288, 384,
	480, 960, 1430, 1920, 2381, 2842,
	3792, 4771, 5693, 6720, 7488, 8640},
	},
	[DDR_1866] = {
	.bw = {	149, 299, 448, 597,
	746, 1493, 2239, 2986, 3657, 4434,
	5852, 7315, 8613, 10450, 11868, 13181},
	},
	[DDR_2133] = {
	.bw = {	171, 341, 512, 683,
	853, 1706, 2560, 3413, 4181, 5068,
	6689, 8361, 9846, 11945, 13566, 15068},
	},
	[DDR_2750] = {
	.bw = {	171, 341, 512, 683,
	853, 1706, 2560, 3413, 4181, 5068,
	6689, 8361, 9846, 11945, 13566, 15068},
	},
};
static struct aphy_others_pwr_data aphy_def_others_pwr_tbl[] = {
	[APHY_VDDQ_0P6V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {9, 9, 9, 9,
	9, 9, 9, 9, 9, 9,
	9, 12, 12, 12, 12, 12},
			.write_coef = {16, 37, 58, 79,
	100, 205, 295, 360, 460, 525,
	670, 815, 940, 1070, 1175, 1275},
		},
		[DDR_600] = {
			.read_coef = {23, 25, 27, 30,
	32, 38, 53, 56, 56, 56,
	56, 56, 56, 56, 56, 56},
			.write_coef = {29, 69, 109, 149,
	189, 339, 484, 624, 753, 874,
	1068, 1268, 1439, 1603, 1758, 1928},
		},
		[DDR_800] = {
			.read_coef = {23, 29, 35, 41,
	53, 71, 83, 89, 89, 89,
	89, 89, 89, 89, 89, 89},
			.write_coef = {58, 108, 158, 208,
	258, 468, 653, 823, 988, 1093,
	1413, 1663, 1903, 2128, 2303, 2528},
		},
		[DDR_933] = {
			.read_coef = {26, 39, 53, 63,
	72, 97, 111, 113, 119, 119,
	119, 119, 119, 119, 119, 119},
			.write_coef = {78, 148, 218, 278,
	328, 568, 773, 1008, 1203, 1353,
	1678, 1958, 2248, 2513, 2798, 2983},
		},
		[DDR_1333] = {
			.read_coef = {37, 57, 77, 91,
	115, 150, 168, 182, 182, 182,
	182, 182, 182, 182, 182, 182},
			.write_coef = {133, 206, 278, 372,
	433, 778, 1044, 1378, 1606, 1823,
	2250, 2661, 3017, 3378, 3639, 4001},
		},
		[DDR_1866] = {
			.read_coef = {53, 55, 60, 65,
	69, 73, 84, 88, 103, 185,
	456, 809, 1118, 1560, 1918, 2237},
			.write_coef = {180, 325, 435, 610,
	705, 1115, 1470, 1810, 2125, 2445,
	3030, 3590, 4080, 4730, 5170, 5430},
		},
		[DDR_2133] = {
			.read_coef = {83, 83, 87, 93,
	99, 104, 111, 118, 134, 220,
	479, 826, 1102, 1609, 1963, 2281},
			.write_coef = {255, 450, 575, 830,
	940, 1345, 1710, 2040, 2330, 2670,
	3200, 3750, 4170, 4940, 5450, 5470},
		},
		[DDR_2750] = {
			.read_coef = {83, 83, 87, 93,
	99, 104, 111, 118, 134, 220,
	479, 826, 1102, 1609, 1963, 2281},
			.write_coef = {255, 450, 575, 830,
	940, 1345, 1710, 2040, 2330, 2670,
	3200, 3750, 4170, 4940, 5450, 5470},
		},
	},
	.coef_idle = {85, 87, 87, 87, 97, 1190, 1200, 1200},
	},
	[APHY_VM_0P75V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {25, 38, 52, 65,
	79, 146, 206, 259, 326, 375,
	480, 566, 652, 750, 825, 893},
			.write_coef = {21, 32, 43, 54,
	65, 120, 165, 200, 250, 280,
	350, 415, 475, 535, 580, 615},
		},
		[DDR_600] = {
			.read_coef = {38, 56, 75, 94,
	113, 203, 296, 383, 461, 536,
	645, 769, 878, 990, 1106, 1213},
			.write_coef = {50, 65, 80, 95,
	110, 190, 265, 340, 405, 460,
	555, 650, 730, 805, 870, 940},
		},
		[DDR_800] = {
			.read_coef = {45, 71, 98, 124,
	150, 266, 371, 476, 566, 653,
	814, 881, 1106, 1241, 1370, 1495},
			.write_coef = {85, 95, 105, 115,
	160, 265, 365, 455, 535, 585,
	750, 865, 975, 1080, 1155, 1250},
		},
		[DDR_933] = {
			.read_coef = {54, 88, 122, 155,
	181, 320, 445, 549, 680, 755,
	954, 1104, 1292, 1423, 1546, 1646},
			.write_coef = {57, 97, 137, 167,
	197, 327, 432, 562, 662, 737,
	897, 1032, 1177, 1297, 1422, 1507},
		},
		[DDR_1333] = {
			.read_coef = {75, 117, 158, 204,
	246, 438, 600, 767, 900, 1046,
	1271, 1513, 1746, 1959, 2152, 2356},
			.write_coef = {100, 139, 178, 233,
	267, 461, 606, 789, 917, 1033,
	1250, 1456, 1634, 1811, 1928, 2078},
		},
		[DDR_1866] = {
			.read_coef = {193, 309, 388, 538,
	553, 853, 1056, 1307, 1434, 1596,
	1873, 2166, 2417, 2747, 3013, 3221},
			.write_coef = {242, 383, 478, 677,
	742, 1013, 1182, 1333, 1492, 1618,
	1828, 2048, 2237, 2508, 2658, 2752},
		},
		[DDR_2133] = {
			.read_coef = {289, 461, 559, 671,
	784, 1155, 1343, 1665, 1785, 1950,
	2209, 2520, 2771, 3180, 3473, 3701},
			.write_coef = {365, 585, 715, 1050,
	1125, 1420, 1580, 1705, 1875, 2020,
	2165, 2395, 2575, 2940, 3130, 3165},
		},
		[DDR_2750] = {
			.read_coef = {289, 461, 559, 671,
	784, 1155, 1343, 1665, 1785, 1950,
	2209, 2520, 2771, 3180, 3473, 3701},
			.write_coef = {365, 585, 715, 1050,
	1125, 1420, 1580, 1705, 1875, 2020,
	2165, 2395, 2575, 2940, 3130, 3165},
		},
	},
	.coef_idle = {50, 50, 50, 53, 59, 63, 67, 67},
	},
	[APHY_VIO_1P2V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {37, 62, 88, 113,
	138, 264, 396, 516, 642, 738,
	966, 1122, 1260, 1452, 1602, 1668},
			.write_coef = {1, 2, 3, 4,
	5, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10},
		},
		[DDR_600] = {
			.read_coef = {52, 76, 100, 124,
	148, 292, 448, 604, 748, 874,
	1000, 1186, 1342, 1498, 1658, 1807},
			.write_coef = {2, 4, 6, 8,
	11, 13, 13, 13, 13, 13,
	13, 13, 13, 13, 13, 13},
		},
		[DDR_800] = {
			.read_coef = {36, 60, 84, 108,
	174, 342, 492, 648, 756, 870,
	1074, 1380, 1398, 1542, 1679, 1812},
			.write_coef = {8, 10, 13, 15,
	20, 20, 20, 20, 20, 20,
	20, 20, 20, 20, 20, 20},
		},
		[DDR_933] = {
			.read_coef = {36, 72, 108, 144,
	174, 342, 499, 618, 792, 858,
	1062, 1194, 1392, 1548, 1694, 1816},
			.write_coef = {5, 5, 5, 10,
	20, 20, 20, 20, 20, 20,
	20, 20, 20, 20, 20, 20},
		},
		[DDR_1333] = {
			.read_coef = {40, 80, 120, 160,
	193, 380, 553, 720, 860, 1000,
	1147, 1340, 1554, 1707, 1847, 1994},
			.write_coef = {6, 6, 6, 11,
	28, 28, 28, 28, 28, 28,
	28, 28, 28, 28, 28, 28},
		},
		[DDR_1866] = {
			.read_coef = {54, 102, 144, 192,
	228, 426, 618, 810, 894, 1014,
	1170, 1368, 1560, 1710, 1831, 1917},
			.write_coef = {5, 10, 15, 25,
	25, 25, 25, 25, 25, 25,
	25, 25, 25, 25, 25, 25},
		},
		[DDR_2133] = {
			.read_coef = {60, 108, 144, 204,
	234, 426, 612, 834, 894, 1032,
	1176, 1356, 1548, 1704, 1809, 1880},
			.write_coef = {5, 10, 15, 25,
	25, 25, 25, 25, 25, 25,
	25, 25, 25, 25, 25, 25},
		},
		[DDR_2750] = {
			.read_coef = {60, 108, 144, 204,
	234, 426, 612, 834, 894, 1032,
	1176, 1356, 1548, 1704, 1809, 1880},
			.write_coef = {5, 10, 15, 25,
	25, 25, 25, 25, 25, 25,
	25, 25, 25, 25, 25, 25},
		},
	},
	.coef_idle = {185, 187, 201, 215, 450, 530, 620, 620},
	},
};

static struct dram_pwr_conf dram_def_pwr_conf[] = {
	[DRAM_VDD1_1P8V] = {
		/* SW co-relation */
		.i_dd0 = 13000,
		.i_dd2p = 1000,
		.i_dd2n = 3000,
		.i_dd4r = 9000,
		.i_dd4w = 10000,
		.i_dd5 = 5000,
		.i_dd6 = 475,
	},
	[DRAM_VDD2_1P1V] = {
		/* SW co-relation */
		.i_dd0 = 50000,
		.i_dd2p = 8000,
		.i_dd2n = 13500,
		.i_dd4r = 147000,
		.i_dd4w = 151000,
		.i_dd5 = 25000,
		.i_dd6 = 625,
	},
	[DRAM_VDDQ_0P6V] = {
		.i_dd0 = 9000,
		.i_dd2p = 11250,
		.i_dd2n = 7500,
		.i_dd4r = 38000,
		.i_dd4w = 10000,
		.i_dd5 = 8000,
		.i_dd6 = 65,
	},
};

/* subsys share mem reference table */
static struct swpm_mem_ref_tbl mem_ref_tbl[NR_POWER_METER] = {
	[CPU_POWER_METER] = {0, NULL},
	[GPU_POWER_METER] = {0, NULL},
	[CORE_POWER_METER] = {0, NULL},
	[MEM_POWER_METER] = {0, NULL},
	[ISP_POWER_METER] = {0, NULL},
	[ME_POWER_METER] = {0, NULL},
};

/****************************************************************************
 *  Static Function
 ****************************************************************************/
static void swpm_send_enable_ipi(unsigned int type, unsigned int enable)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) &&		\
	defined(CONFIG_MTK_QOS_FRAMEWORK)
	struct qos_ipi_data qos_d;

	qos_d.cmd = QOS_IPI_SWPM_ENABLE;
	qos_d.u.swpm_enable.type = type;
	qos_d.u.swpm_enable.enable = enable;
	qos_ipi_to_sspm_command(&qos_d, 3);
#endif
}

static void swpm_pmu_start(int cpu)
{
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);

	if (l3_event)
		perf_event_enable(l3_event);
	if (i_event)
		perf_event_enable(i_event);
	if (c_event)
		perf_event_enable(c_event);
}

static void swpm_pmu_stop(int cpu)
{
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);

	if (l3_event)
		perf_event_disable(l3_event);
	if (i_event)
		perf_event_disable(i_event);
	if (c_event)
		perf_event_disable(c_event);
}

static void swpm_pmu_set_enable(int cpu, int enable)
{
	struct perf_event *event;
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);

	if (enable) {
		if (!l3_event) {
			event = perf_event_create_kernel_counter(
				&l3dc_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				swpm_err("create l3dc counter error!\n");
				return;
			}
			per_cpu(l3dc_events, cpu) = event;
		}
		if (!i_event && cpu >= NR_CPU_L_CORE) {
			event = perf_event_create_kernel_counter(
				&inst_spec_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				swpm_err("create inst_spec counter error!\n");
				return;
			}
			per_cpu(inst_spec_events, cpu) = event;
		}
		if (!c_event && cpu >= NR_CPU_L_CORE) {
			event = perf_event_create_kernel_counter(
				&cycle_event_attr, cpu,	NULL, NULL, NULL);
			if (IS_ERR(event)) {
				swpm_err("create cycle counter error!\n");
				return;
			}
			per_cpu(cycle_events, cpu) = event;
		}

		swpm_pmu_start(cpu);
	} else {
		swpm_pmu_stop(cpu);

		if (l3_event) {
			per_cpu(l3dc_events, cpu) = NULL;
			perf_event_release_kernel(l3_event);
		}
		if (i_event) {
			per_cpu(inst_spec_events, cpu) = NULL;
			perf_event_release_kernel(i_event);
		}
		if (c_event) {
			per_cpu(cycle_events, cpu) = NULL;
			perf_event_release_kernel(c_event);
		}
	}
}

static void swpm_pmu_set_enable_all(unsigned int enable)
{
	int i;
	unsigned int swpm_pmu_user, swpm_pmu_en;

	swpm_pmu_user = enable >> SWPM_CODE_USER_BIT;
	swpm_pmu_en = enable & 0x1;

	if (swpm_pmu_user > NR_SWPM_PMU_USER) {
		swpm_err("pmu_user invalid = %d\n",
			 swpm_pmu_user);
		return;
	}

	swpm_lock(&swpm_pmu_mutex);
	get_online_cpus();
	if (swpm_pmu_en) {
		if (!swpm_pmu_sta) {
#ifdef CONFIG_MTK_CACHE_CONTROL
			ca_force_stop_set_in_kernel(1);
#endif
			for (i = 0; i < num_possible_cpus(); i++)
				swpm_pmu_set_enable(i, swpm_pmu_en);
		}
		if (!swpm_pmu_get_sta(swpm_pmu_user))
			swpm_pmu_set_sta(swpm_pmu_user);

	} else {
		if (swpm_pmu_get_sta(swpm_pmu_user))
			swpm_pmu_clr_sta(swpm_pmu_user);

		if (!swpm_pmu_sta) {
			for (i = 0; i < num_possible_cpus(); i++)
				swpm_pmu_set_enable(i, swpm_pmu_en);
#ifdef CONFIG_MTK_CACHE_CONTROL
			ca_force_stop_set_in_kernel(0);
#endif
		}
	}
	put_online_cpus();
	swpm_unlock(&swpm_pmu_mutex);

	swpm_err("pmu_enable: %d, user_sta: %d\n",
		 swpm_pmu_en, swpm_pmu_sta);
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static void swpm_send_init_ipi(unsigned int addr, unsigned int size,
			      unsigned int ch_num)
{
#if defined(CONFIG_MTK_QOS_FRAMEWORK)
	struct qos_ipi_data qos_d;
	struct share_wrap *wrap_d;
	unsigned int offset;

	qos_d.cmd = QOS_IPI_SWPM_INIT;
	qos_d.u.swpm_init.dram_addr = addr;
	qos_d.u.swpm_init.dram_size = size;
	qos_d.u.swpm_init.dram_ch_num = ch_num;
	offset = qos_ipi_to_sspm_command(&qos_d, 4);

	if (offset == -1) {
		swpm_err("qos ipi not ready init fail\n");
		goto error;
	} else if (offset == 0) {
		swpm_err("swpm share sram init fail\n");
		goto error;
	}

	/* get wrapped sram address */
	wrap_d = (struct share_wrap *)
		sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!wrap_d) {
		swpm_err("swpm share sram offset fail\n");
		goto error;
	}

	/* get sram power index and control address from wrap data */
	share_idx_ref = (struct share_index *)
		sspm_sbuf_get(wrap_d->share_index_addr);
	share_idx_ctrl = (struct share_ctrl *)
		sspm_sbuf_get(wrap_d->share_ctrl_addr);

	/* init extension index address */
	swpm_sp_init(sspm_sbuf_get(wrap_d->share_index_ext_addr),
		     sspm_sbuf_get(wrap_d->share_ctrl_ext_addr));

	/* init unsigned int ptr for share index iterator */
	idx_ref_uint_ptr = (unsigned int *)share_idx_ref;
	/* init output size to LTR except window_cnt with decrease 1 */
	idx_output_size =
		(sizeof(struct share_index)/sizeof(unsigned int)) - 1;

	swpm_err("share_index size check = %zu\n",
		 sizeof(struct share_index));
#if SWPM_TEST
	swpm_err("wrap_d = 0x%p, wrap index addr = 0x%p, wrap ctrl addr = 0x%p\n",
		 wrap_d, wrap_d->share_index_addr, wrap_d->share_ctrl_addr);
	swpm_err("share_idx_ref = 0x%p, share_idx_ctrl = 0x%p\n",
		 share_idx_ref, share_idx_ctrl);
#endif
#endif
	swpm_init_state = 1;
	return;

error:
	swpm_init_state = 0;
	share_idx_ref = NULL;
	share_idx_ctrl = NULL;
	idx_ref_uint_ptr = NULL;
	idx_output_size = 0;
}

static inline void swpm_pass_to_sspm(void)
{
#ifdef CONFIG_MTK_DRAMC
	swpm_send_init_ipi((unsigned int)(rec_phys_addr & 0xFFFFFFFF),
		(unsigned int)(rec_size & 0xFFFFFFFF), get_emi_ch_num());
#else
	swpm_send_init_ipi((unsigned int)(rec_phys_addr & 0xFFFFFFFF),
		(unsigned int)(rec_size & 0xFFFFFFFF), 2);
#endif
}

static void swpm_core_thermal_cb(void)
{
#ifdef CONFIG_THERMAL
#if CFG_THERM_LVTS
	int temp, infra_temp, cam_temp;

	if (!core_ptr)
		return;

	infra_temp = get_immediate_tslvts3_3_wrap();
	cam_temp = get_immediate_tslvts3_2_wrap();

	temp = (infra_temp + cam_temp) / 2;

	/* truncate negative deg */
	temp = (temp < 0) ? 0 : temp;
	core_ptr->thermal = (unsigned int)temp;

#if SWPM_TEST
	swpm_err("swpm_core infra = %d\n", infra_temp);
	swpm_err("swpm_core cam = %d\n", cam_temp);
	swpm_err("swpm_core cpuL = %d\n", get_immediate_cpuL_wrap());
	swpm_err("swpm_core cpuB = %d\n", get_immediate_cpuB_wrap());
#endif
#endif /* CFG_THERM_LVTS */
#endif
}

#ifdef CONFIG_THERMAL
static unsigned int swpm_get_cpu_temp(enum cpu_lkg_type type)
{
	unsigned int temp = 0;

	switch (type) {
	case CPU_L_LKG:
		temp = get_immediate_cpuL_wrap() / 1000;
		break;
	case CPU_B_LKG:
		temp = get_immediate_cpuB_wrap() / 1000;
		break;
	case DSU_LKG:
	default:
		temp = get_immediate_cpuL_wrap() / 1000;
		break;
	}

	return temp;
}
#endif

#ifdef CONFIG_MTK_STATIC_POWER
static int swpm_get_spower_devid(enum cpu_lkg_type type)
{
	int devid;

	switch (type) {
	case CPU_L_LKG:
		devid = MTK_SPOWER_CPULL;
		break;
	case CPU_B_LKG:
		devid = MTK_SPOWER_CPUL;
		break;
	case DSU_LKG:
	default:
		devid = MTK_SPOWER_CCI;
		break;
	}

	return devid;
}
#endif

static void swpm_update_lkg_table(void)
{
	int temp, dev_id, volt, i, j;
	unsigned int lkg = 0;

	for (i = 0; i < NR_CPU_LKG_TYPE; i++) {
#ifdef CONFIG_THERMAL
		temp = swpm_get_cpu_temp((enum cpu_lkg_type)i);
#else
		temp = 85;
#endif
		if (swpm_info_ref)
			swpm_info_ref->cpu_temp[i] = temp;

		for (j = 0; j < NR_CPU_OPP; j++) {
#ifdef CONFIG_MTK_CPU_FREQ
			volt = mt_cpufreq_get_volt_by_idx(i, j) / 100;
#else
			volt = 1000;
#endif
#ifdef CONFIG_MTK_STATIC_POWER
			dev_id = swpm_get_spower_devid((enum cpu_lkg_type)i);
#ifdef GET_UW_LKG
			lkg = mt_spower_get_leakage_uW(dev_id, volt, temp);
#else
			lkg = mt_spower_get_leakage(dev_id, volt, temp) * 1000;
#endif
#else
			dev_id = 0;
			lkg = 5000;
#endif
			if (swpm_info_ref)
				swpm_info_ref->cpu_lkg_pwr[i][j] = lkg;
		}
	}
#ifdef CONFIG_THERMAL
	//get gpu lkg and thermal
	if (swpm_info_ref) {
		swpm_info_ref->gpu_reserved[gthermal + 1] =
			get_immediate_gpu_wrap() / 1000;
		swpm_info_ref->gpu_reserved[glkg + 1] =
			mt_gpufreq_get_leakage_no_lock();
	}
#endif

	swpm_core_thermal_cb();
}

static void swpm_idx_snap(void)
{
	if (share_idx_ref) {
		swpm_lock(&swpm_snap_lock);
		/* directly copy due to 8 bytes alignment problem */
		mem_idx_snap.read_bw[0] = share_idx_ref->mem_idx.read_bw[0];
		mem_idx_snap.write_bw[0] = share_idx_ref->mem_idx.write_bw[0];
		swpm_unlock(&swpm_snap_lock);
	}
}

static char idx_buf[POWER_INDEX_CHAR_SIZE] = { 0 };

static void swpm_log_loop(unsigned long data)
{
	char buf[256] = {0};
	char *ptr = buf;
	char *idx_ptr = idx_buf;
	int i;
#ifdef LOG_LOOP_TIME_PROFILE
	ktime_t t1, t2;
	unsigned long long diff, diff2;

	t1 = ktime_get();
#endif

	/* initialization retry */
	if (!swpm_init_state)
		swpm_pass_to_sspm();

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & swpm_log_mask) {
			ptr += snprintf(ptr, 256, "%s/",
				swpm_power_rail_to_string((enum power_rail)i));
		}
	}
	ptr--;
	ptr += sprintf(ptr, " = ");

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & swpm_log_mask) {
			ptr += snprintf(ptr, 256, "%d/",
				swpm_get_avg_power((enum power_rail)i, 50));
		}
	}
	ptr--;
	ptr += sprintf(ptr, " uA");

	/* for LTR */
	if (share_idx_ref && share_idx_ctrl) {
		memset(idx_buf, 0, sizeof(char) * POWER_INDEX_CHAR_SIZE);
		/* exclude window_cnt */
		for (i = 0; i < idx_output_size; i++) {
			idx_ptr += snprintf(idx_ptr, POWER_INDEX_CHAR_SIZE,
					    "%d,", *(idx_ref_uint_ptr+i));
		}
#if SWPM_TEST
		idx_ptr--;
		idx_ptr += snprintf(idx_ptr, POWER_INDEX_CHAR_SIZE,
				    " window_cnt = %d",
				    share_idx_ref->window_cnt);
#endif

		/* snapshot the last completed average index data */
		swpm_idx_snap();

		/* set share sram clear flag and release lock */
		share_idx_ctrl->clear_flag = 1;

		/* put power index data to ftrace */
		trace_swpm_power_idx(idx_buf);
	}
	/* put power data to ftrace */
	trace_swpm_power(buf);

#ifdef LOG_LOOP_TIME_PROFILE
	t2 = ktime_get();
#endif

	swpm_update_lkg_table();

#ifdef LOG_LOOP_TIME_PROFILE
	diff = ktime_to_us(ktime_sub(t2, t1));
	diff2 = ktime_to_us(ktime_sub(ktime_get(), t2));
	swpm_err("exe time = %llu/%lluus\n", diff, diff2);
#endif

	swpm_update_periodic_timer();
}

#endif /* #ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT : 573 */

static void swpm_core_static_data_init(void)
{
#ifdef CONFIG_MTK_STATIC_POWER
	unsigned int lkg, lkg_scaled;
#endif
	unsigned int i, j;

	if (!core_ptr)
		return;

#ifdef CONFIG_MTK_STATIC_POWER
	/* init core static power once, efuse default mW */
	lkg = mt_spower_get_efuse_lkg(MTK_SPOWER_VCORE);
#else
	/* default CORE_DEFAULT_LKG mW */
	lkg = CORE_DEFAULT_LKG;
#endif

	/* recording default lkg data, and check replacement data */
	core_lkg = lkg;
	lkg = (!core_lkg_replaced) ? lkg : core_lkg_replaced;

	/* efuse static power unit mW with voltage scaling */
	for (i = 0; i < NR_CORE_VOLT; i++) {
		lkg_scaled = lkg * core_volt_tbl[i] / V_OF_FUSE_VCORE
			* core_volt_tbl[i] / V_OF_FUSE_VCORE;
		for (j = 0; j < NR_CORE_LKG_TYPE; j++) {
			/* unit (uW) */
			core_ptr->core_lkg_pwr[i][j] =
			(core_lkg_rt[j]) ?
			(lkg_scaled * core_lkg_rt[j] / CORE_LKG_RT_RES)
			: lkg_scaled;
		}
	}
}

static void swpm_core_pwr_data_init(void)
{
	if (!core_ptr)
		return;

	/* copy core volt data */
	memcpy(core_ptr->core_volt_tbl, core_volt_tbl,
	       sizeof(core_volt_tbl));
	/* copy aphy core pwr data */
	memcpy(core_ptr->aphy_core_bw_tbl, aphy_ref_core_bw_tbl,
	       sizeof(aphy_ref_core_bw_tbl));
	memcpy(core_ptr->aphy_core_pwr_tbl, aphy_def_core_pwr_tbl,
	       sizeof(aphy_def_core_pwr_tbl));

	swpm_info("aphy core_bw[%ld]/core[%ld]/core_volt[%ld]\n",
		  (unsigned long)sizeof(aphy_ref_core_bw_tbl),
		  (unsigned long)sizeof(aphy_def_core_pwr_tbl),
		  (unsigned long)sizeof(core_volt_tbl));
}

static void swpm_mem_pwr_data_init(void)
{
	if (!mem_ptr)
		return;

	/* copy aphy others pwr data */
	memcpy(mem_ptr->aphy_others_bw_tbl, aphy_ref_others_bw_tbl,
	       sizeof(aphy_ref_others_bw_tbl));
	memcpy(mem_ptr->aphy_others_pwr_tbl, aphy_def_others_pwr_tbl,
	       sizeof(aphy_def_others_pwr_tbl));
	/* copy dram pwr data */
	memcpy(mem_ptr->dram_conf, dram_def_pwr_conf,
	       sizeof(dram_def_pwr_conf));
	memcpy(mem_ptr->ddr_opp_freq, ddr_opp_freq,
	       sizeof(ddr_opp_freq));
}

static void swpm_me_pwr_data_init(void)
{
	if (!me_ptr)
		return;

	me_ptr->disp_resolution = 0;
	me_ptr->disp_fps = 0;
	me_ptr->disp_active = 1;
	me_ptr->venc_freq = 0;
	me_ptr->venc_active = 0;
	me_ptr->vdec_freq = 0;
	me_ptr->vdec_active = 0;


	swpm_info("ME disp_resolution=%d disp_fps=%d\n",
		me_ptr->disp_resolution, me_ptr->disp_fps);
}

static void swpm_init_pwr_data(void)
{
	int ret;
	phys_addr_t *ptr = NULL;

	ret = swpm_mem_addr_request(CORE_SWPM_TYPE, &ptr);
	if (!ret)
		core_ptr = (struct core_swpm_rec_data *)ptr;

	ret = swpm_mem_addr_request(MEM_SWPM_TYPE, &ptr);
	if (!ret)
		mem_ptr = (struct mem_swpm_rec_data *)ptr;

	ret = swpm_mem_addr_request(ME_SWPM_TYPE, &ptr);
	if (!ret)
		me_ptr = (struct me_swpm_rec_data *)ptr;

	swpm_core_static_data_init();
	swpm_core_pwr_data_init();
	swpm_mem_pwr_data_init();
	swpm_me_pwr_data_init();
}

#if SWPM_TEST
/* self value checking for mem reference table */
static inline void swpm_interface_unit_test(void)
{
	int i, ret;
	phys_addr_t *ptr = NULL;
	struct isp_swpm_rec_data *isp_ptr;

	for (i = 0; i < NR_SWPM_TYPE; i++) {
		ret = swpm_mem_addr_request((enum swpm_type)i, &ptr);

		if (!ret) {
			swpm_err("swpm_mem_tbl[%d] = 0x%x\n", i, ptr);

			/* data access test */
			switch (i) {
			case ISP_POWER_METER:
				isp_ptr = (struct isp_swpm_rec_data *)ptr;
				isp_ptr->isp_data[2] = 0x123;
				break;
			}
		} else {
			swpm_err("swpm_mem_tbl[%d] = NULL\n", i);
		}
	}
}
#endif

/* init share mem reference table for subsys */
static inline void swpm_subsys_data_ref_init(void)
{
	swpm_lock(&swpm_mutex);

	mem_ref_tbl[MEM_POWER_METER].valid = true;
	mem_ref_tbl[MEM_POWER_METER].virt =
		(phys_addr_t *)&swpm_info_ref->mem_reserved;
	mem_ref_tbl[CORE_POWER_METER].valid = true;
	mem_ref_tbl[CORE_POWER_METER].virt =
		(phys_addr_t *)&swpm_info_ref->core_reserved;
	mem_ref_tbl[GPU_POWER_METER].valid = true;
	mem_ref_tbl[GPU_POWER_METER].virt =
		(phys_addr_t *)&swpm_info_ref->gpu_reserved;
	mem_ref_tbl[ISP_POWER_METER].valid = true;
	mem_ref_tbl[ISP_POWER_METER].virt =
		(phys_addr_t *)&swpm_info_ref->isp_reserved;
	mem_ref_tbl[ME_POWER_METER].valid = true;
	mem_ref_tbl[ME_POWER_METER].virt =
		(phys_addr_t *)&swpm_info_ref->me_reserved;

	swpm_unlock(&swpm_mutex);
}

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	static char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		return NULL;

	buf[len] = '\0';

	return buf;
}

static int dram_bw_proc_show(struct seq_file *m, void *v)
{
	swpm_lock(&swpm_snap_lock);
	seq_printf(m, "DRAM BW R/W=%d/%d\n",
		   mem_idx_snap.read_bw[0],
		   mem_idx_snap.write_bw[0]);
	swpm_unlock(&swpm_snap_lock);

	return 0;
}

static int idd_tbl_proc_show(struct seq_file *m, void *v)
{
	int i;

	if (!mem_ptr)
		return 0;

	for (i = 0; i < NR_DRAM_PWR_TYPE; i++) {
		seq_puts(m, "==========================\n");
		seq_printf(m, "idx %d i_dd0 = %d\n", i,
			mem_ptr->dram_conf[i].i_dd0);
		seq_printf(m, "idx %d i_dd2p = %d\n", i,
			mem_ptr->dram_conf[i].i_dd2p);
		seq_printf(m, "idx %d i_dd2n = %d\n", i,
			mem_ptr->dram_conf[i].i_dd2n);
		seq_printf(m, "idx %d i_dd4r = %d\n", i,
			mem_ptr->dram_conf[i].i_dd4r);
		seq_printf(m, "idx %d i_dd4w = %d\n", i,
			mem_ptr->dram_conf[i].i_dd4w);
		seq_printf(m, "idx %d i_dd5 = %d\n", i,
			mem_ptr->dram_conf[i].i_dd5);
		seq_printf(m, "idx %d i_dd6 = %d\n", i,
			mem_ptr->dram_conf[i].i_dd6);
	}
	seq_puts(m, "==========================\n");

	return 0;
}
static ssize_t idd_tbl_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int type, idd_idx, val;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!mem_ptr)
		goto end;

	if (swpm_status) {
		swpm_err("disable swpm for data change, need to restart manually\n");
		swpm_set_enable(ALL_METER_TYPE, 0);
	}

	if (sscanf(buf, "%d %d %d", &type, &idd_idx, &val) == 3) {
		if (type < (NR_DRAM_PWR_TYPE) &&
		    idd_idx < (sizeof(struct dram_pwr_conf) /
			       sizeof(unsigned int)))
			*(&mem_ptr->dram_conf[type].i_dd0 + idd_idx) = val;
	} else {
		swpm_err("echo <type> <idx> <val> > /proc/swpm/idd_tbl\n");
	}

end:
	return count;
}

static unsigned int pmu_ms_mode;
static int pmu_ms_mode_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", pmu_ms_mode);
	return 0;
}
static ssize_t pmu_ms_mode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable = 0;
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &enable)) {
		pmu_ms_mode = enable;

		/* TODO: remove this path after qos commander ready */
		swpm_set_update_cnt(0, (0x1 << SWPM_CODE_USER_BIT) |
				    pmu_ms_mode);
	} else
		swpm_err("echo <0/1> > /proc/swpm/pmu_ms_mode\n");

	return count;
}

static unsigned int swpm_pmsr_en = 1;
static int swpm_pmsr_en_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "swpm_pmsr only support disabling cmd\n");
	seq_printf(m, "%d\n", swpm_pmsr_en);
	return 0;
}
static ssize_t swpm_pmsr_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable = 0;
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &enable)) {
		if (!enable) {
			swpm_pmsr_en = enable;
			/* TODO: remove this path after qos commander ready */
			swpm_set_update_cnt(0, 9696 << SWPM_CODE_USER_BIT);
		}
	} else
		swpm_err("echo <0/1> > /proc/swpm/swpm_pmsr_en\n");

	return count;
}

static int core_static_replace_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "default: %d, replaced %d (valid:0~999)\n",
		   core_lkg,
		   core_lkg_replaced);
	return 0;
}
static ssize_t core_static_replace_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int val = 0;
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &val)) {
		core_lkg_replaced = (val < 1000) ? val : core_lkg_replaced;

		/* reset core static power data */
		swpm_core_static_data_init();
	} else
		swpm_err("echo <val> > /proc/swpm/core_static_replace\n");

	return count;
}

PROC_FOPS_RW(idd_tbl);
PROC_FOPS_RO(dram_bw);
PROC_FOPS_RW(pmu_ms_mode);
PROC_FOPS_RW(swpm_pmsr_en);
PROC_FOPS_RW(core_static_replace);

static void swpm_cmd_dispatcher(unsigned int type,
				unsigned int val)
{
	switch (type) {
	case SET_PMU:
		swpm_pmu_set_enable_all(val);
		break;
	}
}

static struct swpm_core_internal_ops plat_ops = {
	.cmd = swpm_cmd_dispatcher,
};

/***************************************************************************
 *  API
 ***************************************************************************/
void swpm_set_enable(unsigned int type, unsigned int enable)
{
	if (!swpm_init_state
	    || (type != ALL_METER_TYPE && type >= NR_POWER_METER))
		return;

	if (type == ALL_METER_TYPE) {
		int i;

		for_each_pwr_mtr(i) {
			if (enable) {
				if (swpm_get_status(i))
					continue;

				if (i == CPU_POWER_METER)
					swpm_pmu_enable(SWPM_PMU_INTERNAL, 1);
				swpm_set_status(i);
			} else {
				if (!swpm_get_status(i))
					continue;

				if (i == CPU_POWER_METER)
					swpm_pmu_enable(SWPM_PMU_INTERNAL, 0);
				swpm_clr_status(i);
			}
		}
		swpm_send_enable_ipi(type, enable);
	} else if (type < NR_POWER_METER) {
		if (enable && !swpm_get_status(type)) {
			if (type == CPU_POWER_METER)
				swpm_pmu_enable(SWPM_PMU_INTERNAL, 1);
			swpm_set_status(type);
		} else if (!enable && swpm_get_status(type)) {
			if (type == CPU_POWER_METER)
				swpm_pmu_enable(SWPM_PMU_INTERNAL, 0);
			swpm_clr_status(type);
		}
		swpm_send_enable_ipi(type, enable);
	}
}

/* TODO: move to static internal use */
char *swpm_power_rail_to_string(enum power_rail p)
{
	char *s;

	switch (p) {
	case VPROC2:
		s = "VPROC2";
		break;
	case VPROC1:
		s = "VPROC1";
		break;
	case VGPU:
		s = "VGPU";
		break;
	case VCORE:
		s = "VCORE";
		break;
	case VDRAM:
		s = "VDRAM";
		break;
	case VIO18_DDR:
		s = "VIO18_DDR";
		break;
	case VIO18_DRAM:
		s = "VIO18_DRAM";
		break;
	default:
		s = "None";
		break;
	}

	return s;
}

void swpm_set_update_cnt(unsigned int type, unsigned int cnt)
{
	if (!swpm_init_state
	    || (type != ALL_METER_TYPE && type >= NR_POWER_METER))
		return;

	swpm_lock(&swpm_mutex);
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) &&		\
	defined(CONFIG_MTK_QOS_FRAMEWORK)
	{
		struct qos_ipi_data qos_d;

		qos_d.cmd = QOS_IPI_SWPM_SET_UPDATE_CNT;
		qos_d.u.swpm_set_update_cnt.type = type;
		qos_d.u.swpm_set_update_cnt.cnt = cnt;
		qos_ipi_to_sspm_command(&qos_d, 3);
	}
#endif
	swpm_unlock(&swpm_mutex);
}

static void swpm_platform_procfs(void)
{
	struct swpm_entry idd_tbl = PROC_ENTRY(idd_tbl);
	struct swpm_entry dram_bw = PROC_ENTRY(dram_bw);
	struct swpm_entry pmu_mode = PROC_ENTRY(pmu_ms_mode);
	struct swpm_entry swpm_pmsr = PROC_ENTRY(swpm_pmsr_en);
	struct swpm_entry core_lkg_rp = PROC_ENTRY(core_static_replace);

	swpm_append_procfs(&idd_tbl);
	swpm_append_procfs(&dram_bw);
	swpm_append_procfs(&pmu_mode);
	swpm_append_procfs(&swpm_pmsr);
	swpm_append_procfs(&core_lkg_rp);
}

static int __init swpm_platform_init(void)
{
	int ret = 0;

#ifdef BRINGUP_DISABLE
	swpm_err("swpm is disabled\n");
	goto end;
#endif

	swpm_create_procfs();

	swpm_platform_procfs();

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	swpm_get_rec_addr(&rec_phys_addr,
			  &rec_virt_addr,
			  &rec_size);

	swpm_info_ref = (struct swpm_rec_data *)(uintptr_t)rec_virt_addr;
#endif

	if (!swpm_info_ref) {
		swpm_err("get sspm dram addr failed\n");
		ret = -1;
		goto end;
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	ret |= swpm_reserve_mem_init(&rec_virt_addr, &rec_size);
#endif

	swpm_subsys_data_ref_init();

	ret |= swpm_interface_manager_init(mem_ref_tbl, NR_SWPM_TYPE);

	swpm_init_pwr_data();

	swpm_core_ops_register(&plat_ops);

#if SWPM_TEST
	swpm_interface_unit_test();
#endif

	swpm_pmu_enable(SWPM_PMU_CPU_DVFS, 1);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	swpm_pass_to_sspm();

	/* set preiodic timer task */
	swpm_set_periodic_timer(swpm_log_loop);
#endif

#if SWPM_TEST
	/* enable all pwr meter and set swpm timer to start */
	swpm_set_enable(ALL_METER_TYPE, EN_POWER_METER_ONLY);
	swpm_update_periodic_timer();
#endif
end:
	return ret;
}
late_initcall_sync(swpm_platform_init)

static void __exit swpm_platform_exit(void)
{
	swpm_set_enable(ALL_METER_TYPE, 0);
	swpm_sp_exit();
}
module_exit(swpm_platform_exit)

