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

/* rt => /100000, uA => *1000, res => 100 */
#define CORE_DEFAULT_DEG (30)
#define CORE_DEFAULT_LKG (103)
#define CORE_LKG_RT_RES (100)
static unsigned int core_lkg;
static unsigned int core_lkg_replaced;
static unsigned short core_lkg_rt[NR_CORE_LKG_TYPE] = {
	7547, 4198, 5670, 11214, 16155, 19466,
	5042, 9301, 12999, 2085, 6321
};
static unsigned short core_volt_tbl[NR_CORE_VOLT] = {
	575, 600, 650, 725, 750,
};
static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {
	400, 600, 800, 933, 1200, 1600, 1866, 2133,
};
static struct aphy_core_bw_data aphy_ref_core_bw_tbl[] = {
	[DDR_400] = {
	.bw = {63, 142, 300, 566, 1300, 1929,
	2566, 3176, 3847, 4471, 5296, 5769},
	},
	[DDR_600] = {
	.bw = {93, 213, 797, 2026, 3859, 4493,
	5042, 6171, 6701, 7262, 7892, 8796},
	},
	[DDR_800] = {
	.bw = {120, 283, 1077, 2697, 5145, 5990,
	6738, 8266, 8899, 9650, 10442, 11793},
	},
	[DDR_933] = {
	.bw = {141, 345, 1253, 3150, 6005, 6995,
	7859, 9607, 10375, 11222, 12230, 13759},
	},
	[DDR_1200] = {
	.bw = {175, 427, 1621, 4039, 7716, 8999,
	10061, 12290, 13249, 14410, 15652, 17662},
	},
	[DDR_1600] = {
	.bw = {237, 583, 2218, 5530, 10540, 12232,
	13670, 16677, 17946, 19335, 21059, 24038},
	},
	[DDR_1866] = {
	.bw = {273, 670, 2535, 6338, 12021, 13892,
	15507, 18910, 20276, 22132, 23896, 27176},
	},
	[DDR_2133] = {
	.bw = {321, 786, 2957, 7416, 13995, 16105,
	17957, 21886, 23383, 25861, 27678, 31358},
	},
};
static struct aphy_core_pwr_data aphy_def_core_pwr_tbl[] = {
	[APHY_VCORE] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {15, 33, 68, 129, 271, 396,
	515, 640, 715, 773, 774, 789},
			.write_coef = {26, 59, 123, 231, 513, 746,
	973, 1226, 1451, 1547, 1547, 1547},
		},
		[DDR_600] = {
			.read_coef = {17, 46, 166, 397, 715, 854,
	901, 934, 941, 946, 950, 950},
			.write_coef = {46, 107, 401, 985, 1893, 2071,
	2310, 2313, 2313, 2313, 2313, 2319},
		},
		[DDR_800] = {
			.read_coef = {161, 402, 1142, 1842, 2613, 2832,
	2995, 2997, 3001, 3005, 3005, 3005},
			.write_coef = {186, 421, 1372, 2417, 3822, 4167,
	4316, 4317, 4317, 4317, 4323, 4328},
		},
		[DDR_933] = {
			.read_coef = {210, 532, 1494, 2412, 3361, 3649,
	3836, 3850, 3850, 3850, 3850, 3850},
			.write_coef = {220, 564, 1735, 3051, 4811, 5248,
	5620, 5627, 5636, 5636, 5636, 5645},
		},
		[DDR_1200] = {
			.read_coef = {340, 816, 2114, 3371, 4510, 4831,
	5064, 5123, 5123, 5123, 5123, 5123},
			.write_coef = {349, 750, 2406, 4228, 6517, 7148,
	7370, 7370, 7370, 7370, 7388, 7388},
		},
		[DDR_1600] = {
			.read_coef = {507, 1265, 3285, 5025, 6733, 7293,
	7527, 7532, 7537, 7539, 7565, 7642},
			.write_coef = {515, 1236, 3644, 6201, 9280, 10126,
	10813, 10830, 10844, 10844, 10844, 10996},
		},
		[DDR_1866] = {
			.read_coef = {731, 1704, 4417, 6786, 8805, 9465,
	9759, 9838, 9842, 9880, 9880, 9880},
			.write_coef = {749, 1671, 4866, 8410, 12134, 13204,
	14120, 14171, 14171, 14186, 14234, 14283},
		},
		[DDR_2133] = {
			.read_coef = {946, 2058, 5326, 8221, 10330, 11020,
	11350, 11555, 11587, 11617, 11617, 11617},
			.write_coef = {978, 2026, 5828, 10230, 14233, 15444,
	16540, 16632, 16632, 16719, 16719, 16719},
		},
	},
	.coef_idle = {1895, 2229, 2563, 3092, 3307, 4334, 5508, 6152},
	},
};
static struct aphy_others_bw_data aphy_ref_others_bw_tbl[] = {
	[DDR_400] = {
	.bw = {64, 128, 192, 256,
	320, 640, 960, 1280, 1600, 1920,
	2547, 3187, 3814, 4480, 5069, 5722},
	},
	[DDR_600] = {
	.bw = {96, 192, 288, 384,
	480, 960, 1440, 1920, 2390, 2880,
	3821, 4781, 5664, 6653, 7613, 8621},
	},
	[DDR_800] = {
	.bw = {128, 256, 384, 512,
	640, 1280, 1920, 2547, 3200, 3802,
	5069, 6336, 7603, 8947, 10150, 11482},
	},
	[DDR_933] = {
	.bw = {149, 299, 448, 597,
	746, 1493, 2239, 2986, 3702, 4434,
	5971, 7315, 8867, 10450, 11868, 13241},
	},
	[DDR_1200] = {
	.bw = {192, 384, 576, 768,
	960, 1920, 2861, 3840, 4762, 5683,
	7584, 9542, 11386, 13440, 14976, 17280},
	},
	[DDR_1600] = {
	.bw = {256, 512, 768, 1024,
	1280, 2560, 3840, 5069, 6323, 7603,
	10138, 12672, 15206, 17894, 19866, 22810},
	},
	[DDR_1866] = {
	.bw = {299, 597, 896, 1194,
	1493, 2986, 4478, 5971, 7315, 8867,
	11704, 14629, 17227, 20899, 23736, 26363},
	},
	[DDR_2133] = {
	.bw = {341, 683, 1024, 1365,
	1706, 3413, 5119, 6826, 8361, 10136,
	13378, 16723, 19692, 23890, 27132, 30135},
	},
};
static struct aphy_others_pwr_data aphy_def_others_pwr_tbl[] = {
	[APHY_VDDQ_0P6V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {30, 30, 30, 30,
	30, 30, 30, 30, 30, 30,
	30, 40, 40, 40, 40, 40},
			.write_coef = {32, 74, 116, 158,
	200, 410, 590, 720, 920, 1050,
	1340, 1630, 1880, 2140, 2350, 2550},
		},
		[DDR_600] = {
			.read_coef = {75, 83, 91, 99,
	107, 127, 177, 187, 187, 187,
	187, 187, 187, 187, 187, 187},
			.write_coef = {57, 137, 217, 297,
	377, 677, 967, 1247, 1507, 1747,
	2137, 2537, 2877, 3207, 3517, 3857},
		},
		[DDR_800] = {
			.read_coef = {76, 96, 116, 136,
	176, 236, 276, 296, 296, 296,
	296, 296, 296, 296, 296, 296},
			.write_coef = {116, 216, 316, 416,
	516, 936, 1306, 1646, 1976, 2186,
	2826, 3326, 3806, 4256, 4606, 5056},
		},
		[DDR_933] = {
			.read_coef = {85, 131, 177, 208,
	241, 324, 369, 378, 398, 398,
	398, 398, 398, 398, 398, 398},
			.write_coef = {156, 296, 436, 556,
	656, 1136, 1546, 2016, 2406, 2706,
	3356, 3916, 4496, 5026, 5596, 5966},
		},
		[DDR_1200] = {
			.read_coef = {111, 171, 232, 274,
	345, 451, 503, 545, 545, 545,
	545, 545, 545, 545, 545, 545},
			.write_coef = {240, 370, 500, 670,
	780, 1400, 1880, 2480, 2890, 3280,
	4050, 4790, 5430, 6080, 6550, 7200},
		},
		[DDR_1600] = {
			.read_coef = {70, 90, 110, 120,
	130, 140, 160, 190, 240, 500,
	1440, 2640, 3780, 5040, 6216, 7378},
			.write_coef = {210, 400, 590, 780,
	940, 1770, 2460, 3160, 3840, 4440,
	5720, 6860, 7980, 9040, 9780, 10780},
		},
		[DDR_1866] = {
			.read_coef = {178, 184, 200, 216,
	230, 244, 279, 292, 344, 616,
	1519, 2697, 3726, 5201, 6393, 7457},
			.write_coef = {360, 650, 870, 1220,
	1410, 2230, 2940, 3620, 4250, 4890,
	6060, 7180, 8160, 9460, 10340, 10860},
		},
		[DDR_2133] = {
			.read_coef = {277, 278, 291, 311,
	330, 347, 370, 393, 447, 732,
	1598, 2754, 3672, 5363, 6542, 7603},
			.write_coef = {510, 900, 1150, 1660,
	1880, 2690, 3420, 4080, 4660, 5340,
	6400, 7500, 8340, 9880, 10900, 10940},
		},
	},
	.coef_idle = {170, 173, 174, 174, 174, 2360, 2380, 2400},
	},
	[APHY_VM_0P75V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {66, 102, 138, 174,
	210, 390, 550, 690, 870, 1000,
	1280, 1510, 1740, 2000, 2200, 2380},
			.write_coef = {42, 64, 86, 108,
	130, 240, 330, 400, 500, 560,
	700, 830, 950, 1070, 1160, 1230},
		},
		[DDR_600] = {
			.read_coef = {100, 150, 200, 250,
	300, 540, 790, 1020, 1230, 1430,
	1720, 2050, 2340, 2640, 2949, 3236},
			.write_coef = {100, 130, 160, 190,
	220, 380, 530, 680, 810, 920,
	1110, 1300, 1460, 1610, 1740, 1880},
		},
		[DDR_800] = {
			.read_coef = {120, 190, 260, 330,
	400, 710, 990, 1270, 1510, 1740,
	2170, 2350, 2950, 3310, 3653, 3986},
			.write_coef = {170, 190, 210, 230,
	320, 530, 730, 910, 1070, 1170,
	1500, 1730, 1950, 2160, 2310, 2500},
		},
		[DDR_933] = {
			.read_coef = {144, 234, 324, 414,
	484, 854, 1186, 1464, 1814, 2014,
	2544, 2944, 3444, 3794, 4122, 4390},
			.write_coef = {114, 194, 274, 334,
	394, 654, 864, 1124, 1324, 1474,
	1794, 2064, 2354, 2594, 2844, 3014},
		},
		[DDR_1200] = {
			.read_coef = {180, 280, 380, 490,
	590, 1050, 1440, 1840, 2160, 2510,
	3050, 3630, 4190, 4700, 5165, 5653},
			.write_coef = {180, 250, 320, 420,
	480, 830, 1090, 1420, 1650, 1860,
	2250, 2620, 2940, 3260, 3470, 3740},
		},
		[DDR_1600] = {
			.read_coef = {260, 420, 580, 720,
	860, 1470, 2050, 2530, 2890, 3310,
	4100, 4830, 5500, 6170, 6795, 7378},
			.write_coef = {240, 360, 480, 610,
	720, 1210, 1570, 1920, 2220, 2430,
	2980, 3400, 3800, 4150, 4370, 4680},
		},
		[DDR_1866] = {
			.read_coef = {515, 825, 1035, 1435,
	1475, 2275, 2815, 3485, 3825, 4255,
	4995, 5775, 6445, 7325, 8036, 8591},
			.write_coef = {485, 765, 955, 1355,
	1485, 2025, 2365, 2665, 2985, 3235,
	3655, 4095, 4475, 5015, 5315, 5505},
		},
		[DDR_2133] = {
			.read_coef = {770, 1230, 1490, 1790,
	2090, 3080, 3580, 4440, 4760, 5200,
	5890, 6720, 7390, 8480, 9261, 9869},
			.write_coef = {730, 1170, 1430, 2100,
	2250, 2840, 3160, 3410, 3750, 4040,
	4330, 4790, 5150, 5880, 6260, 6330},
		},
	},
	.coef_idle = {100, 100, 100, 106, 106, 116, 125, 135},
	},
	[APHY_VIO_1P2V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {62, 104, 146, 188,
	230, 440, 660, 860, 1070, 1230,
	1610, 1870, 2100, 2420, 2670, 2780},
			.write_coef = {2, 4, 6, 8,
	10, 20, 20, 20, 20, 20,
	20, 20, 20, 20, 20, 20},
		},
		[DDR_600] = {
			.read_coef = {86, 126, 166, 206,
	246, 486, 746, 1006, 1246, 1456,
	1666, 1976, 2236, 2496, 2764, 3012},
			.write_coef = {5, 8, 12, 15,
	21, 26, 26, 26, 26, 26,
	26, 26, 26, 26, 26, 26},
		},
		[DDR_800] = {
			.read_coef = {60, 100, 140, 180,
	290, 570, 820, 1080, 1260, 1450,
	1790, 2300, 2330, 2570, 2799, 3019},
			.write_coef = {15, 20, 25, 30,
	40, 40, 40, 40, 40, 40,
	40, 40, 40, 40, 40, 40},
		},
		[DDR_933] = {
			.read_coef = {60, 120, 180, 240,
	290, 570, 831, 1030, 1320, 1430,
	1770, 1990, 2320, 2580, 2824, 3026},
			.write_coef = {10, 10, 10, 20,
	40, 40, 40, 40, 40, 40,
	40, 40, 40, 40, 40, 40},
		},
		[DDR_1200] = {
			.read_coef = {60, 120, 180, 240,
	290, 570, 830, 1080, 1290, 1500,
	1720, 2010, 2330, 2560, 2770, 2990},
			.write_coef = {10, 10, 10, 20,
	50, 50, 50, 50, 50, 50,
	50, 50, 50, 50, 50, 50},
		},
		[DDR_1600] = {
			.read_coef = {80, 160, 240, 300,
	370, 710, 1040, 1310, 1490, 1660,
	1940, 2300, 2620, 2860, 3084, 3286},
			.write_coef = {50, 50, 50, 50,
	50, 50, 50, 50, 50, 50,
	50, 50, 50, 50, 50, 50},
		},
		[DDR_1866] = {
			.read_coef = {90, 170, 240, 320,
	380, 710, 1030, 1350, 1490, 1690,
	1950, 2280, 2600, 2850, 3052, 3195},
			.write_coef = {10, 20, 30, 50,
	50, 50, 50, 50, 50, 50,
	50, 50, 50, 50, 50, 50},
		},
		[DDR_2133] = {
			.read_coef = {100, 180, 240, 340,
	390, 710, 1020, 1390, 1490, 1720,
	1960, 2260, 2580, 2840, 3015, 3133},
			.write_coef = {10, 20, 30, 50,
	50, 50, 50, 50, 50, 50,
	50, 50, 50, 50, 50, 50},
		},
	},
	.coef_idle = {370, 374, 402, 430, 810, 880, 1060, 1241},
	},
};

static struct dram_pwr_conf dram_def_pwr_conf[] = {
	[DRAM_VDD1_1P8V] = {
		/* SW co-relation */
		.i_dd0 = 11369,
		.i_dd2p = 1125,
		.i_dd2n = 1100,
		.i_dd4r = 3733,
		.i_dd4w = 4266,
		.i_dd5 = 2500,
		.i_dd6 = 218,
	},
	[DRAM_VDD2_1P1V] = {
		/* SW co-relation */
		.i_dd0 = 44700,
		.i_dd2p = 1150,
		.i_dd2n = 7950,
		.i_dd4r = 125000,
		.i_dd4w = 139300,
		.i_dd5 = 12040,
		.i_dd6 = 266,
	},
	[DRAM_VDDQ_0P6V] = {
		.i_dd0 = 140,
		.i_dd2p = 75,
		.i_dd2n = 75,
		.i_dd4r = 152994,
		.i_dd4w = 511,
		.i_dd5 = 75,
		.i_dd6 = 36,
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

	infra_temp = get_immediate_tslvts6_0_wrap();
	cam_temp = get_immediate_tslvts6_1_wrap();

	temp = (infra_temp + cam_temp) / 2;

	/* truncate negative deg */
	temp = (temp < 0) ? 0 : temp;
	core_ptr->thermal = (unsigned int)temp;

#if SWPM_TEST
	swpm_err("swpm_core infra lvts6_0 = %d\n", infra_temp);
	swpm_err("swpm_core cam lvts6_1 = %d\n", cam_temp);
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
	case CPU_BB_LKG:
		devid = MTK_SPOWER_CPUB;
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
		mem_idx_snap.read_bw[1] = share_idx_ref->mem_idx.read_bw[1];
		mem_idx_snap.write_bw[0] = share_idx_ref->mem_idx.write_bw[0];
		mem_idx_snap.write_bw[1] = share_idx_ref->mem_idx.write_bw[1];
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
	if (!swpm_init_state) {
		swpm_pass_to_sspm();
	}

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
	/* init core static power once */
	lkg = mt_spower_get_efuse_lkg(MTK_SPOWER_VCORE);
#else
	lkg = 0;
#endif

	/* default CORE_DEFAULT_LKG mA, efuse default mW to mA */
	lkg = (!lkg) ? CORE_DEFAULT_LKG
			: (lkg * 1000 / V_OF_FUSE_VCORE);
	/* recording default lkg data, and check replacement data */
	core_lkg = lkg;
	lkg = (!core_lkg_replaced) ? lkg : core_lkg_replaced;

	/* efuse static power unit mW with voltage scaling */
	for (i = 0; i < NR_CORE_VOLT; i++) {
		lkg_scaled = lkg * core_volt_tbl[i] / V_OF_FUSE_VCORE;
		for (j = 0; j < NR_CORE_LKG_TYPE; j++) {
			/* unit (uA) */
			core_ptr->core_lkg_pwr[i][j] =
				lkg_scaled * core_lkg_rt[j] / CORE_LKG_RT_RES;
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
	seq_printf(m, "DRAM BW [N]R/W=%d/%d,[S]R/W=%d/%d\n",
		   mem_idx_snap.read_bw[0],
		   mem_idx_snap.write_bw[0],
		   mem_idx_snap.read_bw[1],
		   mem_idx_snap.write_bw[1]);
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
	struct swpm_entry core_lkg_rp = PROC_ENTRY(core_static_replace);

	swpm_append_procfs(&idd_tbl);
	swpm_append_procfs(&dram_bw);
	swpm_append_procfs(&pmu_mode);
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

